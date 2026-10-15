/* SPDX-License-Identifier: GPL-3.0-only */
/* Experimental recovery against a byte-addressed NOR model. No hardware hooks. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../firmware/experimental/recovery/jr_store.h"
#include "../firmware/experimental/recovery/jr_codec.h"

#define FLASH_BYTES 0x100000u
#define BODY_BYTES 27272u
#define MAX_OPS 160u
static uint8_t image[FLASH_BYTES], baseline[FLASH_BYTES], protected_image[FLASH_BYTES];
static uint8_t body_a[BODY_BYTES], body_b[BODY_BYTES], recovered[BODY_BYTES];
static unsigned checks, failures;
static uint64_t interruption_points;
static uint32_t crc_a, crc_b;

typedef struct { unsigned erase; uint32_t address, length; uint8_t data[256]; } operation_t;
static operation_t operations[MAX_OPS];
static unsigned operation_count, recording;
static int read_fail_after = -1, erase_fail_after = -1, program_fail_after = -1;
static int ignore_erase, ignore_program, mutate_source, fail_commit_readback;
static unsigned erases, programs, reads;

static void require(int ok, const char *message)
{
    ++checks;
    if (!ok) { ++failures; fprintf(stderr, "FAIL: %s\n", message); }
}
static uint32_t crc32_bytes(const void *data, size_t length)
{
    const uint8_t *p = data;
    uint32_t crc = UINT32_MAX;
    while (length--) {
        crc ^= *p++;
        for (unsigned k=0;k<8;k++) crc=(crc>>1)^((0u-(crc&1u))&0xedb88320u);
    }
    return ~crc;
}
static void le16(uint8_t *p, uint16_t n) { p[0]=(uint8_t)n; p[1]=(uint8_t)(n>>8); }
static void le32(uint8_t *p, uint32_t n)
{ for(unsigned k=0;k<4;k++)p[k]=(uint8_t)(n>>(8*k)); }
static int allowed_sector(uint32_t address)
{
    for(unsigned c=0;c<2;c++) for(unsigned s=0;s<7;s++)
        if(address==jr_store_sector_address(c,s)) return 1;
    return 0;
}
static int allocated_byte(uint32_t address) { return allowed_sector(address & ~4095u); }
static int trip(int *counter)
{
    if (*counter<0) return 0;
    if (!*counter) return 1;
    --*counter; return 0;
}
static void log_operation(unsigned erase, uint32_t address, const void *data, uint32_t length)
{
    if(!recording) return;
    assert(operation_count<MAX_OPS);
    operation_t *op=&operations[operation_count++];
    op->erase=erase;op->address=address;op->length=length;
    if(!erase) { assert(length<=256); memcpy(op->data,data,length); }
}
static int nor_read(void *ctx, uint32_t address, void *data, uint32_t length)
{
    (void)ctx; ++reads;
    assert(address<=FLASH_BYTES && length<=FLASH_BYTES-address);
    if(trip(&read_fail_after)) return -1;
    memcpy(data,image+address,length);return 0;
}
static int nor_erase(void *ctx, uint32_t address)
{
    (void)ctx;++erases;
    assert(!(address&4095u) && allowed_sector(address));
    if(trip(&erase_fail_after)) return -1;
    log_operation(1,address,NULL,4096);
    if(!ignore_erase) memset(image+address,255,4096);
    if(mutate_source) { body_b[0]^=1; mutate_source=0; }
    return 0;
}
static int nor_program(void *ctx, uint32_t address, const void *data, uint32_t length)
{
    const uint8_t *p=data;(void)ctx;++programs;
    assert(length && length<=256 && (address&255u)+length<=256);
    assert(address<FLASH_BYTES && length<=FLASH_BYTES-address);
    assert(allocated_byte(address) && allocated_byte(address+length-1));
    if(trip(&program_fail_after)) return -1;
    log_operation(0,address,data,length);
    if(!ignore_program)for(uint32_t i=0;i<length;i++)image[address+i]&=p[i];
    if(fail_commit_readback &&
       ((address&~255u)==0x97000u || (address&~255u)==0x9e000u) &&
       (address&4095u)+length==256u)read_fail_after=0;
    return 0;
}
static jr_nor_t nor={NULL,nor_read,nor_erase,nor_program};
static int buffer_read(void *ctx,uint32_t offset,void *dst,uint32_t length)
{
    if(offset>BODY_BYTES || length>BODY_BYTES-offset)return -1;
    memcpy(dst,(uint8_t*)ctx+offset,length);return 0;
}
static jr_source_t source_a={body_a,buffer_read,BODY_BYTES};
static jr_source_t source_b={body_b,buffer_read,BODY_BYTES};
static int validate(void *ctx,const jr_source_t *source)
{
    (void)ctx;
    return jr_codec_validate(NULL,source);
}
static void reset_faults(void)
{
    read_fail_after=erase_fail_after=program_fail_after=-1;
    ignore_erase=ignore_program=mutate_source=recording=fail_commit_readback=0;
    reads=erases=programs=operation_count=0;
}
static int source_matches(unsigned copy,uint32_t length,const uint8_t *expected)
{
    jr_store_reader_t reader;jr_source_t source;
    if(length!=BODY_BYTES)return 0;
    jr_store_source(&reader,&nor,copy,length,&source);
    return !source.read(source.ctx,0,recovered,BODY_BYTES)&&!memcmp(recovered,expected,BODY_BYTES);
}
static void check_protected(void)
{
    for(uint32_t a=0;a<FLASH_BYTES;a++)
        if(!allocated_byte(a) && image[a]!=protected_image[a]) {
            require(0,"byte outside approved recovery allocation changed");return;
        }
    require(1,"all protected bytes preserved");
}
static void fresh_legacy(void)
{
    reset_faults();
    for(uint32_t i=0;i<FLASH_BYTES;i++)image[i]=(uint8_t)((i*37u+(i>>12)*13u+11u)&255u);
    memcpy(protected_image,image,sizeof image);
}
static void fix_header(unsigned copy)
{
    uint8_t *h=image+jr_store_sector_address(copy,0);
    le32(h+248,crc32_bytes(h,248));
}
static void set_generation(unsigned copy,uint32_t generation)
{
    le32(image+jr_store_sector_address(copy,0)+16,generation);fix_header(copy);
}
static uint32_t body_address(unsigned copy,uint32_t offset)
{
    uint32_t logical=256u+offset;
    return jr_store_sector_address(copy,logical/4096u)+logical%4096u;
}
static void fix_body_crc(unsigned copy)
{
    for(uint32_t i=0;i<BODY_BYTES;i++)recovered[i]=image[body_address(copy,i)];
    le32(image+jr_store_sector_address(copy,0)+20,crc32_bytes(recovered,BODY_BYTES));
    fix_header(copy);
}
static void check_old_or_new(int first,uint32_t old_generation,uint32_t new_generation)
{
    jr_store_state_t state;
    int rc=jr_store_scan(&nor,validate,NULL,&state);
    ++interruption_points;
    if(state.winner<0) {
        if(!first || (rc!=JR_STORE_SETUP_NEEDED && rc!=JR_STORE_READ_ONLY)) {
            fprintf(stderr,"lost valid generation at interruption %llu rc %d\n",
                    (unsigned long long)interruption_points,rc);exit(1);
        }
        return;
    }
    jr_candidate_t *c=&state.copies[state.winner];
    uint32_t expected_crc;
    if(c->generation==old_generation && !first) expected_crc=crc_a;
    else if(c->generation==new_generation)expected_crc=first?crc_a:crc_b;
    else { fprintf(stderr,"mixed/unexpected generation %u\n",c->generation);exit(1); }
    if(c->length!=BODY_BYTES || c->payload_crc!=expected_crc) {
        fprintf(stderr,"mixed recovery body at interruption %llu\n",(unsigned long long)interruption_points);exit(1);
    }
}
/* Replay the exact successful NOR mutation log and scan after every byte-prefix
 * of every erase/program. Reads do not mutate flash, so each prefix is also
 * the image left by a power cut at that point. The writer is separately run
 * with callback failures; this exhaustive loop tests actual loader selection. */
static void replay_interruptions(int first,uint32_t old_generation,uint32_t new_generation)
{
    memcpy(image,baseline,sizeof image);
    reset_faults();
    /* reset_faults clears the count, so the caller saves it separately. */
    unsigned n=0;
    while(n<MAX_OPS && operations[n].length)n++;
    check_old_or_new(first,old_generation,new_generation);
    for(unsigned k=0;k<n;k++) {
        const operation_t *op=&operations[k];
        for(uint32_t i=0;i<op->length;i++) {
            if(op->erase)image[op->address+i]=255;
            else image[op->address+i]&=op->data[i];
            check_old_or_new(first,old_generation,new_generation);
        }
    }
    require(source_matches(0,BODY_BYTES,first?body_a:body_b),"complete replay yields complete intended session");
    check_protected();
}
static void basic_and_exhaustive(void)
{
    jr_store_state_t state;
    fresh_legacy();
    require(jr_codec_validate(NULL,&source_a)==0 && jr_codec_validate(NULL,&source_b)==0,"real instrument source fixtures validate");
    require(jr_store_scan(&nor,validate,NULL,&state)==JR_STORE_SETUP_NEEDED && state.winner==-1,"foreign storage starts read-only without ownership");
    memcpy(baseline,image,sizeof image);
    require(jr_store_save(&nor,&source_a,validate,NULL,0,&state)==JR_STORE_SETUP_NEEDED && !erases && !programs,"no initialization permission means no writes");
    require(!memcmp(baseline,image,sizeof image),"declined initialization preserves all foreign data");
    memset(operations,0,sizeof operations);recording=1;
    require(jr_store_save(&nor,&source_a,validate,NULL,1,&state)==0,"explicit initialization commits first full session");
    recording=0;
    require(state.winner==0 && state.copies[0].generation==1 && state.allocation_claimed,"first verified record claims exact allocation");
    require(source_matches(0,BODY_BYTES,body_a),"first recovered payload matches all components");
    uint8_t committed_header[256];
    memcpy(committed_header,image+0x97000,sizeof committed_header);
    for(unsigned prefix=0;prefix<256;prefix++) {
        memset(image+0x97000,255,256);
        memcpy(image+0x97000,committed_header,prefix);
        require(jr_store_scan(&nor,validate,NULL,&state)==JR_STORE_SETUP_NEEDED &&
                state.winner==-1 && !state.allocation_claimed,
                "torn first header remains unclaimed and retryable");
    }
    memset(image+0x97000,255,256);memcpy(image+0x97000,committed_header,128);
    require(jr_store_save(&nor,&source_a,validate,NULL,1,&state)==0 &&
            state.copies[state.winner].generation==1,"explicit retry after interrupted initialization succeeds");
    check_protected();
    replay_interruptions(1,0,1);
    /* A read-only scan after first A mimics a fresh process: no RAM ownership. */
    require(jr_store_scan(&nor,validate,NULL,&state)==0 && state.allocation_claimed,"reboot obtains ownership from committed A only");
    require(jr_store_save(&nor,&source_a,validate,NULL,0,&state)==0 && state.winner==1,"claim permits first B checkpoint over approved legacy bytes");
    memcpy(baseline,image,sizeof image);reset_faults();
    memset(operations,0,sizeof operations);recording=1;
    require(jr_store_save(&nor,&source_b,validate,NULL,0,&state)==0 && state.winner==0,"replace inactive A with second distinct session");
    recording=0;
    require(source_matches(0,BODY_BYTES,body_b),"second complete session reads across all disjoint extents");
    replay_interruptions(0,2,3);
    /* Keep two valid old copies as the baseline for control-flow failure tests. */
    memcpy(image,baseline,sizeof image);reset_faults();
}
static void faults_and_versions(void)
{
    jr_store_state_t state;
    uint8_t saved_first=body_b[0];
    for(int k=0;k<7;k++) {
        memcpy(image,baseline,sizeof image);reset_faults();erase_fail_after=k;
        require(jr_store_save(&nor,&source_b,validate,NULL,0,&state)!=0,"erase failure cannot report success");
        reset_faults();require(jr_store_scan(&nor,validate,NULL,&state)==0 && state.copies[state.winner].generation==2,"erase failure retains old whole session");
    }
    unsigned page_count=(BODY_BYTES+255u)/256u+2u;
    for(unsigned k=0;k<page_count;k++) {
        memcpy(image,baseline,sizeof image);reset_faults();program_fail_after=(int)k;
        int rc=jr_store_save(&nor,&source_b,validate,NULL,0,&state);
        if(rc==0) { /* The final loop sentinel may be after every program. */
            require(k+1==page_count,"all actual program callback failures report an error");break;
        }
        reset_faults();require(jr_store_scan(&nor,validate,NULL,&state)==0 && state.copies[state.winner].generation==2,"program failure retains old whole session");
    }
    memcpy(image,baseline,sizeof image);reset_faults();read_fail_after=0;
    require(jr_store_save(&nor,&source_b,validate,NULL,0,&state)!=0 && !erases && !programs,"unreadable header cannot authorize an erase");
    memcpy(image,baseline,sizeof image);reset_faults();ignore_program=1;
    require(jr_store_save(&nor,&source_b,validate,NULL,0,&state)!=0,"write protection falsely reporting success is caught");
    reset_faults();require(jr_store_scan(&nor,validate,NULL,&state)==0 && state.copies[state.winner].generation==2,"write-protected failure retains previous generation");
    memcpy(image,baseline,sizeof image);reset_faults();ignore_erase=1;
    require(jr_store_save(&nor,&source_b,validate,NULL,0,&state)!=0,"ignored erase is caught before successful commit");
    memcpy(image,baseline,sizeof image);reset_faults();mutate_source=1;
    require(jr_store_save(&nor,&source_b,validate,NULL,0,&state)!=0,"source changed after prevalidation cannot report saved");
    body_b[0]=saved_first;reset_faults();require(jr_store_scan(&nor,validate,NULL,&state)==0 && state.copies[state.winner].generation==2,"source mutation leaves previous generation usable");
    memcpy(image,baseline,sizeof image);reset_faults();fail_commit_readback=1;
    require(jr_store_save(&nor,&source_b,validate,NULL,0,&state)!=0,"post-commit readback failure cannot report saved");
    reset_faults();require(jr_store_scan(&nor,validate,NULL,&state)==0 && state.copies[state.winner].generation==3 &&
        source_matches((unsigned)state.winner,BODY_BYTES,body_b),"fresh scan may recover committed generation after readback error");
    memcpy(image,baseline,sizeof image);reset_faults();
    jr_source_t invalid=source_b;invalid.length=BODY_BYTES-1;
    require(jr_store_save(&nor,&invalid,validate,NULL,0,&state)==JR_STORE_INVALID_SOURCE && !erases && !programs,"truncated source refused before erasing");
    invalid=source_b;invalid.length=JR_STORE_PAYLOAD_MAX+1;
    require(jr_store_save(&nor,&invalid,validate,NULL,0,&state)!=0 && !erases && !programs,"oversized source refused before erasing");

    memcpy(image,baseline,sizeof image);reset_faults();
    le16(image+jr_store_sector_address(0,0)+4,2);fix_header(0);
    require(jr_store_scan(&nor,validate,NULL,&state)==JR_STORE_READ_ONLY && state.winner==1 && !state.writable,"supported older copy recovers beside future schema read-only");
    require(jr_store_save(&nor,&source_b,validate,NULL,1,&state)==JR_STORE_READ_ONLY && !erases && !programs,"explicit initialization cannot overwrite future schema");
    memcpy(image,baseline,sizeof image);reset_faults();image[jr_store_sector_address(0,0)+3]='2';
    require(jr_store_save(&nor,&source_b,validate,NULL,1,&state)==JR_STORE_READ_ONLY && !erases,"future recovery family protected");
    memcpy(image,baseline,sizeof image);reset_faults();le16(image+jr_store_sector_address(0,0)+10,2);fix_header(0);
    require(jr_store_save(&nor,&source_b,validate,NULL,1,&state)==JR_STORE_READ_ONLY && !erases,"unknown allocation claim protected");
    memcpy(image,baseline,sizeof image);reset_faults();
    image[body_address(1,JR_METADATA_OFFSET+JR_META_PREFS+3)]='2';fix_body_crc(1);
    require(jr_store_scan(&nor,validate,NULL,&state)==JR_STORE_READ_ONLY && state.winner==0 && !state.writable,"future inner preferences recover older session read-only");
    require(jr_store_save(&nor,&source_b,validate,NULL,1,&state)==JR_STORE_READ_ONLY && !erases && !programs,"future inner component cannot be erased by initialization");
    memcpy(image,baseline,sizeof image);reset_faults();
    image[body_address(1,JR_METADATA_OFFSET+JR_META_ASSETS+20)]^=1;fix_body_crc(1);
    require(jr_store_scan(&nor,validate,NULL,&state)==JR_STORE_READ_ONLY && state.winner==0,"incompatible asset revision protects newer recovery");

    memcpy(image,baseline,sizeof image);reset_faults();
    image[body_address(1,4)]^=1;fix_body_crc(1);
    require(jr_store_scan(&nor,validate,NULL,&state)==0 && state.winner==0,"CRC-valid malformed newest body falls back on semantic validation");
    memcpy(image,baseline,sizeof image);reset_faults();image[body_address(1,100)]^=1;
    require(jr_store_scan(&nor,validate,NULL,&state)==0 && state.winner==0,"corrupt newest payload falls back");
    image[body_address(0,100)]^=1;
    require(jr_store_scan(&nor,validate,NULL,&state)!=0 && state.winner==-1,"two corrupt copies provide no invented session");
    memcpy(image,baseline,sizeof image);reset_faults();set_generation(0,UINT32_MAX-1);set_generation(1,UINT32_MAX);
    require(jr_store_save(&nor,&source_b,validate,NULL,0,&state)==0 && state.winner==0 && state.copies[0].generation==0,"generation rollover selects new zero generation");
    require(source_matches(0,BODY_BYTES,body_b),"wrapped generation restores newly saved content");
    memcpy(image,baseline,sizeof image);reset_faults();set_generation(0,10);set_generation(1,10);
    require(jr_store_scan(&nor,validate,NULL,&state)==0 && state.winner==0 && state.writable,"identical payload tie deterministically chooses A");
    require(jr_store_save(&nor,&source_b,validate,NULL,0,&state)==0,"save after identical tie succeeds");
    set_generation(0,11);set_generation(1,11);
    require(jr_store_scan(&nor,validate,NULL,&state)==JR_STORE_READ_ONLY && state.winner==0 && !state.writable,"conflicting payload tie remains read-only");
    memcpy(image,baseline,sizeof image);reset_faults();set_generation(0,0);set_generation(1,0x80000000u);
    require(jr_store_scan(&nor,validate,NULL,&state)==JR_STORE_READ_ONLY && !state.writable,"half-range ambiguous generation ordering remains read-only");
    jr_store_reader_t reader;jr_source_t source;
    jr_store_source(&reader,&nor,0,BODY_BYTES,&source);
    require(source.read(source.ctx,BODY_BYTES-1,recovered,2)!=0 && source.read(source.ctx,UINT32_MAX,recovered,2)!=0,"read view rejects truncation/overflow boundary crossing");
    reset_faults();check_protected();
}
static int read_file(const char *path,void *dst,size_t length)
{
    FILE *f=fopen(path,"rb");if(!f)return -1;
    size_t n=fread(dst,1,length,f);int extra=fgetc(f);int rc=fclose(f);
    return n==length && extra==EOF && !rc?0:-1;
}
static int write_file(const char *path,const void *data,size_t length)
{
    FILE *f=fopen(path,"wb");if(!f)return -1;
    int bad=fwrite(data,1,length,f)!=length;return fclose(f)||bad?-1:0;
}
/* --put/--get deliberately relaunch this program. Only the NOR file crosses
 * process boundaries; no static/retained RAM or previous scan state survives. */
static int file_mode(int argc,char **argv)
{
    jr_store_state_t state;reset_faults();
    if(!strcmp(argv[1],"--put") && argc==6) {
        if(read_file(argv[3],body_a,BODY_BYTES))return 2;
        FILE *f=fopen(argv[2],"rb");
        if(f) { fclose(f);if(read_file(argv[2],image,FLASH_BYTES))return 2; }
        else memset(image,255,sizeof image);
        int init=!strcmp(argv[4],"initialize");
        if(strcmp(argv[4],"initialize")&&strcmp(argv[4],"checkpoint"))return 2;
        int rc=jr_store_save(&nor,&source_a,validate,NULL,init,&state);
        if(rc) { fprintf(stderr,"file save failed %d\n",rc);return 1; }
        if(write_file(argv[2],image,FLASH_BYTES))return 2;
        FILE *r=fopen(argv[5],"w");if(!r)return 2;
        fprintf(r,"copy=%d generation=%u bytes=%u\n",state.winner,state.copies[state.winner].generation,state.copies[state.winner].length);
        return fclose(r)!=0;
    }
    if(!strcmp(argv[1],"--get") && argc==4) {
        if(read_file(argv[2],image,FLASH_BYTES))return 2;
        int rc=jr_store_scan(&nor,validate,NULL,&state);
        if((rc!=JR_STORE_OK && rc!=JR_STORE_READ_ONLY) || state.winner<0)return 1;
        jr_store_reader_t reader;jr_source_t source;
        jr_store_source(&reader,&nor,(unsigned)state.winner,state.copies[state.winner].length,&source);
        if(source.length!=BODY_BYTES || source.read(source.ctx,0,recovered,BODY_BYTES))return 1;
        return write_file(argv[3],recovered,BODY_BYTES)?2:0;
    }
    return 2;
}
int main(int argc,char **argv)
{
    if(argc>1 && argv[1][0]=='-')return file_mode(argc,argv);
    if(argc!=3 || read_file(argv[1],body_a,BODY_BYTES) || read_file(argv[2],body_b,BODY_BYTES)) {
        fprintf(stderr,"usage: %s valid-payload-a valid-payload-b\n",argv[0]);return 2;
    }
    require(memcmp(body_a,body_b,BODY_BYTES)!=0,"independent instrument fixtures differ");
    crc_a=crc32_bytes(body_a,BODY_BYTES);crc_b=crc32_bytes(body_b,BODY_BYTES);
    static const uint32_t addresses[2][7]={{0x97000,0x98000,0x99000,0x9a000,0x9b000,0x9c000,0x9d000},
                                          {0x9e000,0x9f000,0xdc000,0xdd000,0xde000,0xdf000,0xfe000}};
    for(unsigned c=0;c<2;c++)for(unsigned s=0;s<7;s++)require(jr_store_sector_address(c,s)==addresses[c][s],"independent declared allocation map");
    require(jr_store_sector_address(2,0)==UINT32_MAX && jr_store_sector_address(0,7)==UINT32_MAX,"invalid mapping indices refused");
    basic_and_exhaustive();faults_and_versions();
    printf("Experimental recovery NOR: %u checks, %llu byte-prefix interruptions, %u failures\n",checks,(unsigned long long)interruption_points,failures);
    return failures?1:0;
}
