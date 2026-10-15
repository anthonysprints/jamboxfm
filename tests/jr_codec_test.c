/* SPDX-License-Identifier: GPL-3.0-only */
/* Unit fixtures deliberately write each wire field, independently of runtime
 * structs. jr_instrument_test.c supplies the complementary real instrument
 * encoder/restore checks. This program never links a hardware flash driver. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../firmware/experimental/recovery/jr_codec.h"
static unsigned checks;
#define CHECK(x) do { checks++; if(!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
typedef struct {
    uint8_t snapshot[5][4584], fm6[5][3][156], syn[1464], metadata[512], arrangement[36];
    jr_payload_view_t view;
    jr_source_t source;
} fixture_t;
static fixture_t fixture;
static void w16(uint8_t *b,uint32_t v) { b[0]=(uint8_t)v; b[1]=(uint8_t)(v>>8); }
static void w32(uint8_t *b,uint32_t v)
{ b[0]=(uint8_t)v; b[1]=(uint8_t)(v>>8); b[2]=(uint8_t)(v>>16); b[3]=(uint8_t)(v>>24); }
static void seal(unsigned s)
{ w32(fixture.snapshot[s]+4580,jr_codec_fnv1a(fixture.snapshot[s],4580)); }
static uint8_t *track(unsigned s,unsigned t) { return fixture.snapshot[s]+76+t*934; }
static void parameter(unsigned s,unsigned t,unsigned id,int v)
{
    uint8_t *p=track(s,t);
    if(id<53) w16(p+id*2,(uint32_t)v);
    else if(id<63) p[122+id-53]=(uint8_t)v;
    else w16(p+106+(id-63)*2,(uint32_t)v);
    seal(s);
}
static void engine(unsigned s,unsigned t,unsigned e)
{
    uint8_t *p=track(s,t); unsigned i;
    p[132]=(uint8_t)e; p[133]=0;
    memset(p+106,0,16);
    memset(fixture.fm6[s][t],0,156);
    if(e==9) for(i=145;i<155;i++) fixture.fm6[s][t][i]=' ';
    jr_codec_identity_set(fixture.metadata,s,t,0,3,(uint8_t)e,1);
    seal(s);
}
static void init(void)
{
    unsigned s,t,k,i;
    memset(&fixture,0,sizeof fixture);
    for(s=0;s<5;s++) {
        uint8_t *b=fixture.snapshot[s];
        w32(b,0x4a524d31u); w32(b+4,4584); w16(b+8,88); w16(b+8+16*2,1);
        b[74]=0x80; b[75]=2;
        fixture.view.snapshot[s]=b;
        for(t=0;t<4;t++) {
            uint8_t *p=track(s,t);
            w16(p+19*2,1); w16(p+20*2,64); w16(p+29*2,64); w16(p+32*2,64); w16(p+46*2,1);
            if(t<3) {
                for(k=0;k<64;k++) {
                    uint8_t *st=p+134+k*10;
                    for(i=0;i<4;i++) st[i]=(uint8_t)(36+(k+i)%64);
                    st[4]=4; st[5]=0; st[6]=3; st[7]=127; st[8]=0xe4; st[9]=0x1b;
                    for(i=0;i<4;i++) b[3812+(t*64+k)*4+i]=(uint8_t)(64-i);
                }
            } else {
                for(k=0;k<640;k++) p[134+k]=(uint8_t)(k+s);
            }
            for(k=0;k<64;k++) p[774+k]=(uint8_t)k;
            for(k=0;k<24;k++) p[838+k*4]=255;
            jr_codec_identity_set(fixture.metadata,s,t,0,3,0,1);
            if(t<3) fixture.view.fm6[s][t]=fixture.fm6[s][t];
        }
        seal(s);
    }
    jr_codec_metadata_init(fixture.metadata,15);
    for(s=0;s<5;s++) for(t=0;t<4;t++) jr_codec_identity_set(fixture.metadata,s,t,0,3,0,1);
    fixture.metadata[160+12]=1;
    fixture.metadata[160+13]=3;
    w32(fixture.syn,0x31555344u); w16(fixture.syn+4,1); w16(fixture.syn+6,4);
    for(k=0;k<4;k++) {
        uint8_t *kit=fixture.syn+8+k*364;
        memcpy(kit,"TEST KIT",8); kit[8]=0xff; kit[9]=(uint8_t)k;
        for(i=0;i<16;i++) kit[12+i*22+21]=128;
    }
    fixture.arrangement[0]=16; fixture.arrangement[1]=1;
    for(k=0;k<16;k++) { fixture.arrangement[4+2*k]=(uint8_t)(k%4); fixture.arrangement[5+2*k]=64; }
    fixture.view.syn_bank=fixture.syn;
    fixture.view.metadata=fixture.metadata;
    fixture.view.arrangement=fixture.arrangement;
    fixture.source.ctx=&fixture.view; fixture.source.read=jr_codec_read_at; fixture.source.length=JR_PAYLOAD_BYTES;
}
static int valid(void) { return jr_codec_validate(NULL,&fixture.source); }
static void basic(void)
{
    uint8_t *image=malloc(JR_PAYLOAD_BYTES), b[97]; uint32_t off,n;
    init(); CHECK(valid()==JR_VALID_OK); CHECK(JR_PAYLOAD_BYTES==27272);
    CHECK(jr_codec_fnv1a("hello",5)==0x4f9f2cabu);
    CHECK(image!=NULL); CHECK(!jr_codec_read_at(&fixture.view,0,image,JR_PAYLOAD_BYTES));
    /* Golden wire ordering/byte order/content fingerprint, including the
     * frozen 64-byte asset/catalog identity. Intentional schema or asset
     * migrations require an explicit golden-fixture review. */
    CHECK(jr_codec_fnv1a(image,JR_PAYLOAD_BYTES)==0x02f76d98u);
    for(off=0;off<JR_PAYLOAD_BYTES;off+=n) {
        n=JR_PAYLOAD_BYTES-off; if(n>sizeof b) n=sizeof b;
        CHECK(!jr_codec_read_at(&fixture.view,off,b,n)); CHECK(!memcmp(image+off,b,n));
    }
    CHECK(jr_codec_read_at(NULL,0,b,1));
    CHECK(jr_codec_read_at(&fixture.view,0,NULL,1));
    CHECK(jr_codec_read_at(&fixture.view,UINT32_MAX,b,1));
    CHECK(jr_codec_read_at(&fixture.view,JR_PAYLOAD_BYTES,b,1));
    CHECK(jr_codec_read_at(&fixture.view,1,b,UINT32_MAX));
    CHECK(!jr_codec_read_at(&fixture.view,JR_PAYLOAD_BYTES,NULL,0));
    fixture.source.length--; CHECK(valid()==JR_VALID_INVALID); fixture.source.length+=2; CHECK(valid()==JR_VALID_INVALID);
    free(image);
}
static void snapshot_checks(void)
{
    unsigned s,t,e,k;
    for(s=0;s<5;s++) for(t=0;t<3;t++) for(e=0;e<12;e++) {
        init(); engine(s,t,e); CHECK(valid()==JR_VALID_OK);
    }
    init(); fixture.snapshot[0][0]^=1; seal(0); CHECK(valid()==JR_VALID_UNSUPPORTED);
    init(); fixture.snapshot[0][8]=0; seal(0); CHECK(valid()==JR_VALID_INVALID);
    init(); fixture.snapshot[0][72]=4; seal(0); CHECK(valid()==JR_VALID_INVALID);
    init(); fixture.snapshot[0][73]=128; seal(0); CHECK(valid()==JR_VALID_INVALID);
    init(); fixture.snapshot[0][74]|=64; seal(0); CHECK(valid()==JR_VALID_INVALID);
    init(); fixture.snapshot[0][75]=5; seal(0); CHECK(valid()==JR_VALID_INVALID);
    init(); fixture.snapshot[0][4580]^=1; CHECK(valid()==JR_VALID_INVALID);
    init(); parameter(0,0,25,12); CHECK(valid()==JR_VALID_INVALID);
    init(); parameter(0,0,27,3); CHECK(valid()==JR_VALID_INVALID);
    init(); parameter(0,0,49,9); CHECK(valid()==JR_VALID_INVALID);
    init(); parameter(0,0,52,5); CHECK(valid()==JR_VALID_INVALID);
    init(); parameter(0,0,53,9); CHECK(valid()==JR_VALID_INVALID);
    init(); parameter(0,0,60,11); CHECK(valid()==JR_VALID_INVALID);
    init(); parameter(0,0,63,5); CHECK(valid()==JR_VALID_INVALID);
    init(); track(0,0)[132]=12; seal(0); CHECK(valid()==JR_VALID_UNSUPPORTED);
    init(); track(0,3)[132]=1; seal(0); CHECK(valid()==JR_VALID_INVALID);
    init(); track(0,0)[133]=22; seal(0); CHECK(valid()==JR_VALID_INVALID);
    for(k=0;k<4;k++) {
        init(); track(0,0)[134+k]=128; seal(0); CHECK(valid()==JR_VALID_INVALID);
        init(); fixture.snapshot[0][3812+k]=65; seal(0); CHECK(valid()==JR_VALID_INVALID);
    }
    init(); track(0,0)[138]=5; seal(0); CHECK(valid()==JR_VALID_INVALID);
    init(); track(0,0)[139]=3; seal(0); CHECK(valid()==JR_VALID_INVALID);
    init(); track(0,0)[140]=4; seal(0); CHECK(valid()==JR_VALID_INVALID);
    init(); track(0,0)[141]=128; seal(0); CHECK(valid()==JR_VALID_INVALID);
    init(); track(0,0)[139]=1; seal(0); CHECK(valid()==JR_VALID_INVALID);
    init(); track(0,0)[138]=3; seal(0); CHECK(valid()==JR_VALID_INVALID);
    init(); track(0,0)[774]=0xc0; seal(0); CHECK(valid()==JR_VALID_INVALID);
    init(); track(0,0)[838]=64; seal(0); CHECK(valid()==JR_VALID_INVALID);
    init(); track(0,0)[838]=63; track(0,0)[839]=25; seal(0); CHECK(valid()==JR_VALID_INVALID);
    init(); track(0,0)[838]=63; track(0,0)[839]=71; seal(0); CHECK(valid()==JR_VALID_INVALID);
    /* Hidden steps, full onsets, long independent notes, all lock slots and
     * packed fill/micro values survive verbatim. */
    init();
    for(k=0;k<24;k++) {
        uint8_t *p=track(4,2)+838+4*k;
        p[0]=(uint8_t)(63-k); p[1]=50; w16(p+2,(uint32_t)(-32+(int)k));
    }
    seal(4); CHECK(valid()==JR_VALID_OK);
}
static void dependencies(void)
{
    unsigned s,t,k;
    for(s=0;s<5;s++) for(t=0;t<3;t++) {
        init(); engine(s,t,4); parameter(s,t,63,8); CHECK(valid()==JR_VALID_UNSUPPORTED);
        init(); engine(s,t,8); parameter(s,t,63,11); CHECK(valid()==JR_VALID_UNSUPPORTED);
        init(); engine(s,t,9); parameter(s,t,70,8); CHECK(valid()==JR_VALID_UNSUPPORTED);
        for(k=0;k<24;k++) {
            uint8_t *p;
            init(); engine(s,t,4); p=track(s,t)+838+k*4; p[0]=63; p[1]=63; w16(p+2,8); seal(s);
            CHECK(valid()==JR_VALID_UNSUPPORTED);
            init(); engine(s,t,9); p=track(s,t)+838+k*4; p[0]=63; p[1]=70; w16(p+2,34); seal(s);
            CHECK(valid()==JR_VALID_UNSUPPORTED);
        }
    }
    for(k=37;k<=41;k++) { init(); parameter(4,3,63,(int)k); CHECK(valid()==JR_VALID_UNSUPPORTED); }
    for(k=42;k<=45;k++) { init(); parameter(4,3,63,(int)k); CHECK(valid()==JR_VALID_OK); }
    init(); fixture.snapshot[4][74]=0x87; seal(4); CHECK(valid()==JR_VALID_UNSUPPORTED);
    init(); fixture.metadata[320]^=1; CHECK(valid()==JR_VALID_UNSUPPORTED);
}
static void sidecars(void)
{
    unsigned i;
    init(); fixture.fm6[0][0][0]=1; CHECK(valid()==JR_VALID_INVALID);
    init(); fixture.syn[3]='2'; CHECK(valid()==JR_VALID_UNSUPPORTED);
    init(); w16(fixture.syn+4,2); CHECK(valid()==JR_VALID_UNSUPPORTED);
    init(); w16(fixture.syn+6,5); CHECK(valid()==JR_VALID_INVALID);
    init(); engine(0,0,9); fixture.fm6[0][0][0]=100; CHECK(valid()==JR_VALID_INVALID);
    init(); engine(0,0,9); fixture.fm6[0][0][145]=31; CHECK(valid()==JR_VALID_INVALID);
    init(); engine(0,0,9); fixture.fm6[0][0][155]=1; CHECK(valid()==JR_VALID_INVALID);
    for(i=0;i<4;i++) {
        uint8_t *kit;
        init(); kit=fixture.syn+8+i*364; kit[0]=127; CHECK(valid()==JR_VALID_INVALID);
        init(); kit=fixture.syn+8+i*364; kit[9]=32; CHECK(valid()==JR_VALID_INVALID);
        init(); kit=fixture.syn+8+i*364; kit[10]=1; CHECK(valid()==JR_VALID_INVALID);
        init(); kit=fixture.syn+8+i*364; kit[12]=6; CHECK(valid()==JR_VALID_INVALID);
        init(); kit=fixture.syn+8+i*364; kit[13]=5; CHECK(valid()==JR_VALID_INVALID);
        init(); kit=fixture.syn+8+i*364; kit[15]=16; CHECK(valid()==JR_VALID_INVALID);
        init(); kit=fixture.syn+8+i*364; kit[16]=97; CHECK(valid()==JR_VALID_INVALID);
        init(); kit=fixture.syn+8+i*364; kit[12+20]=128; CHECK(valid()==JR_VALID_INVALID);
    }
}
static void metadata_checks(void)
{
    unsigned s,i;
    init(); fixture.metadata[164]=2; CHECK(valid()==JR_VALID_UNSUPPORTED);
    init(); fixture.metadata[163]='2'; CHECK(valid()==JR_VALID_UNSUPPORTED);
    init(); fixture.metadata[195]='2'; CHECK(valid()==JR_VALID_UNSUPPORTED);
    init(); fixture.metadata[168]=16; CHECK(valid()==JR_VALID_INVALID);
    init(); fixture.metadata[169]=4; CHECK(valid()==JR_VALID_INVALID);
    init(); fixture.metadata[170]=252; CHECK(valid()==JR_VALID_INVALID);
    init(); fixture.metadata[171]=2; CHECK(valid()==JR_VALID_INVALID);
    init(); fixture.metadata[172]=2; CHECK(valid()==JR_VALID_INVALID);
    init(); fixture.metadata[173]=4; CHECK(valid()==JR_VALID_INVALID);
    init(); fixture.metadata[174]=1; CHECK(valid()==JR_VALID_INVALID);
    init(); fixture.metadata[384]=1; CHECK(valid()==JR_VALID_INVALID);
    init(); fixture.metadata[192+8]=5; CHECK(valid()==JR_VALID_INVALID);
    init(); fixture.metadata[192+19]=3; CHECK(valid()==JR_VALID_INVALID);
    init(); fixture.metadata[192+64]=17; CHECK(valid()==JR_VALID_INVALID);
    init(); fixture.metadata[192+69]=0; CHECK(valid()==JR_VALID_INVALID);
    init(); w16(fixture.metadata+192+32,2u|(120u<<9)); CHECK(valid()==JR_VALID_OK);
    init(); w16(fixture.metadata+192+32,64u|(120u<<9)); CHECK(valid()==JR_VALID_INVALID);
    init(); w16(fixture.metadata+192+32,2u|(64u<<9)); CHECK(valid()==JR_VALID_INVALID);
    init(); fixture.metadata[4]=255; CHECK(valid()==JR_VALID_UNSUPPORTED);
    init(); fixture.metadata[5]=1; CHECK(valid()==JR_VALID_INVALID);
    init(); fixture.metadata[6]=2; CHECK(valid()==JR_VALID_INVALID);
    init(); jr_codec_identity_set(fixture.metadata,0,0,0x101,1,0,0); CHECK(valid()==JR_VALID_OK);
    init(); jr_codec_identity_set(fixture.metadata,0,0,0x10a,1,0,0); CHECK(valid()==JR_VALID_INVALID);
    init(); jr_codec_identity_set(fixture.metadata,0,0,1,2,0,0); CHECK(valid()==JR_VALID_OK);
    init(); jr_codec_identity_set(fixture.metadata,0,0,23,2,0,0); CHECK(valid()==JR_VALID_INVALID);
    init(); fixture.arrangement[0]=17; CHECK(valid()==JR_VALID_INVALID);
    init(); fixture.arrangement[4]=4; CHECK(valid()==JR_VALID_INVALID);
    init(); fixture.arrangement[5]=65; CHECK(valid()==JR_VALID_INVALID);
    init();
    fixture.metadata[168]=0; fixture.metadata[172]=0; fixture.metadata[173]=255;
    for(s=1;s<5;s++) {
        memset(fixture.snapshot[s],0,4584); memset(fixture.fm6[s],0,3*156); memset(fixture.metadata+s*32,0,32);
    }
    CHECK(valid()==JR_VALID_OK);
    for(s=1;s<5;s++) {
        fixture.snapshot[s][4583]=1; CHECK(valid()==JR_VALID_INVALID); fixture.snapshot[s][4583]=0;
        fixture.fm6[s][2][155]=1; CHECK(valid()==JR_VALID_INVALID); fixture.fm6[s][2][155]=0;
        fixture.metadata[s*32+31]=1; CHECK(valid()==JR_VALID_INVALID); fixture.metadata[s*32+31]=0;
    }
    fixture.metadata[172]=1; CHECK(valid()==JR_VALID_INVALID); fixture.metadata[172]=0;
    /* Every reserved preference byte must remain canonical zero. */
    for(i=70;i<128;i++) {
        fixture.metadata[192+i]=1; CHECK(valid()==JR_VALID_INVALID); fixture.metadata[192+i]=0;
    }
}
typedef struct { jr_source_t source; uint32_t fail_at; } faulty_t;
static int faulty_read(void *ctx,uint32_t offset,void *dst,uint32_t bytes)
{
    faulty_t *f=ctx;
    if(offset<=f->fail_at && bytes>f->fail_at-offset) return -1;
    return f->source.read(f->source.ctx,offset,dst,bytes);
}
static void io_checks(void)
{
    faulty_t f; jr_source_t s; unsigned i;
    static const uint32_t positions[]={0,75,76,3811,3812,4583,4584,22919,22920,25259,25260,26723,26724,27235,27236,27271};
    init(); engine(4,2,9); f.source=fixture.source; s.ctx=&f; s.read=faulty_read; s.length=JR_PAYLOAD_BYTES;
    for(i=0;i<sizeof positions/sizeof positions[0];i++) {
        f.fail_at=positions[i]; CHECK(jr_codec_validate(NULL,&s)==JR_VALID_IO);
    }
    CHECK(jr_codec_validate(NULL,NULL)==JR_VALID_INVALID);
    s.read=NULL; CHECK(jr_codec_validate(NULL,&s)==JR_VALID_INVALID);
}
int main(void)
{
    basic(); snapshot_checks(); dependencies(); sidecars(); metadata_checks(); io_checks();
    printf("jr_codec: %u checks passed\n",checks); return 0;
}
