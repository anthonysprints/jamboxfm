/* SPDX-License-Identifier: GPL-3.0-only */
#include "jr_codec.h"
#include "jr_asset_identity.h"
#include <string.h>

_Static_assert(JR_PAYLOAD_BYTES == 27272u, "full session payload bound");
/* Versioned wire constants, audited against project.c/core.h at 99cbf624.
 * Explicit offsets prevent compiler packing, host endian, or pointer size from
 * becoming a persistence ABI. The real-instrument fixture asserts these too. */
enum { JC_TRACK_BYTES = 934, JC_TRACK_HEAD = 134, JC_TRACK_BASE = 76,
       JC_STEPS = 64, JC_LOCKS = 24, JC_PARAM_COUNT = 71, JC_E0 = 63,
       JC_NOTE_LENGTHS = 3812, JC_SUM = 4580 };
/* SHA-256 generated assets + SHA-256 source catalog. The runner verifies the
 * generated identity against actual inputs; refreshing it requires a deliberate
 * compatibility/migration decision. See check_jam_recovery_identity.py. */
static const uint8_t jc_assets[64] = JR_ASSET_ID_BYTES;
static const uint8_t jc_npreset[12] = {22,22,8,8,11,7,14,8,8,8,26,11};
static const uint8_t jc_curated_engine[3][12] = {
    {0,0,0,0,0,0,0,0,0,1,1,2},
    {0,0,1,1,1,4,7,7,0,0,9,9},
    {0,0,0,0,0,0,2,2,1,4,4,9}
};
static uint16_t jc_u16(const uint8_t *b)
{ return (uint16_t)((uint16_t)b[0] | (uint16_t)b[1] << 8); }
static int32_t jc_i16(const uint8_t *b)
{ uint32_t u = jc_u16(b); return u >= 32768u ? (int32_t)u - 65536 : (int32_t)u; }
static uint32_t jc_u32(const uint8_t *b)
{ return (uint32_t)b[0] | (uint32_t)b[1]<<8 | (uint32_t)b[2]<<16 | (uint32_t)b[3]<<24; }
static void jc_put16(uint8_t *b, uint32_t v)
{ b[0]=(uint8_t)v; b[1]=(uint8_t)(v>>8); }
static void jc_put32(uint8_t *b, uint32_t v)
{ b[0]=(uint8_t)v; b[1]=(uint8_t)(v>>8); b[2]=(uint8_t)(v>>16); b[3]=(uint8_t)(v>>24); }
static int jc_zero_bytes(const uint8_t *b, uint32_t n)
{ uint32_t i; for(i=0;i<n;i++) if(b[i]) return 0; return 1; }
uint32_t jr_codec_fnv1a(const void *data, uint32_t bytes)
{
    const uint8_t *b = data;
    uint32_t h = 0x811c9dc5u, i;
    for(i=0;i<bytes;i++) h=(h^b[i])*16777619u;
    return h;
}
int jr_codec_read_at(void *context, uint32_t offset, void *dst, uint32_t bytes)
{
    const jr_payload_view_t *v=context;
    uint8_t *out=dst;
    if(!v || (!dst && bytes) || offset>JR_PAYLOAD_BYTES || bytes>JR_PAYLOAD_BYTES-offset) return -1;
    while(bytes) {
        uint32_t base, length, local, n;
        const uint8_t *p;
        if(offset<JR_FM6_OFFSET) {
            uint32_t i=offset/JR_SNAPSHOT_BYTES;
            base=i*JR_SNAPSHOT_BYTES; length=JR_SNAPSHOT_BYTES; p=v->snapshot[i];
        } else if(offset<JR_SYN_OFFSET) {
            uint32_t i=(offset-JR_FM6_OFFSET)/JR_FM6_BYTES;
            base=JR_FM6_OFFSET+i*JR_FM6_BYTES; length=JR_FM6_BYTES; p=v->fm6[i/3u][i%3u];
        } else if(offset<JR_METADATA_OFFSET) {
            base=JR_SYN_OFFSET; length=JR_SYN_BYTES; p=v->syn_bank;
        } else if(offset<JR_ARRANGEMENT_OFFSET) {
            base=JR_METADATA_OFFSET; length=JR_METADATA_BYTES; p=v->metadata;
        } else {
            base=JR_ARRANGEMENT_OFFSET; length=JR_ARRANGEMENT_BYTES; p=v->arrangement;
        }
        local=offset-base; n=length-local; if(n>bytes) n=bytes;
        if(p) memcpy(out,p+local,n); else memset(out,0,n);
        offset+=n; out+=n; bytes-=n;
    }
    return 0;
}
void jr_codec_metadata_init(uint8_t b[JR_METADATA_BYTES], uint8_t mask)
{
    uint8_t *s=b+JR_META_SESSION, *p=b+JR_META_PREFS;
    memset(b,0,JR_METADATA_BYTES);
    memcpy(s,"JMD1",4); jc_put16(s+4,1); jc_put16(s+6,JR_METADATA_BYTES);
    s[8]=mask; s[13]=255;
    memcpy(p,"JDP1",4); jc_put16(p+4,1); jc_put16(p+6,128);
    p[64]=1; p[65]=2; p[66]=3; p[67]=10; p[69]=2;
    memcpy(b+JR_META_ASSETS,jc_assets,sizeof jc_assets);
}
void jr_codec_identity_set(uint8_t b[JR_METADATA_BYTES], uint32_t snapshot,
                           uint32_t track, uint32_t id, uint8_t origin,
                           uint8_t engine, uint8_t edited)
{
    uint8_t *p;
    if(snapshot>=JR_SNAPSHOTS || track>=JR_TRACKS) return;
    p=b+(snapshot*4u+track)*8u;
    jc_put32(p,id); p[4]=origin; p[5]=engine; p[6]=edited; p[7]=0;
}
static int jc_read(const jr_source_t *s, uint32_t offset, void *dst, uint32_t n)
{
    if(offset>s->length || n>s->length-offset) return JR_VALID_INVALID;
    return s->read(s->ctx,offset,dst,n) ? JR_VALID_IO : JR_VALID_OK;
}
static int jc_zero(const jr_source_t *s, uint32_t offset, uint32_t n)
{
    uint8_t b[128]; int r;
    while(n) {
        uint32_t take=n>sizeof b ? sizeof b:n;
        r=jc_read(s,offset,b,take); if(r) return r;
        if(!jc_zero_bytes(b,take)) return JR_VALID_INVALID;
        offset+=take; n-=take;
    }
    return JR_VALID_OK;
}
static int jc_metadata(const uint8_t *b)
{
    const uint8_t *s=b+JR_META_SESSION, *p=b+JR_META_PREFS;
    uint32_t i;
    if(memcmp(s,"JMD1",4) || memcmp(p,"JDP1",4)) return JR_VALID_UNSUPPORTED;
    if(jc_u16(s+4)!=1 || jc_u16(p+4)!=1) return JR_VALID_UNSUPPORTED;
    if(jc_u16(s+6)!=512 || jc_u16(p+6)!=128 || s[8]>15 || s[9]>3 ||
       !((s[10]<=3) || s[10]>=253) || s[11]>1 || s[12]>1 ||
       (s[13]!=255 && (s[13]>3 || !(s[8]&(1u<<s[13])))) ||
       !jc_zero_bytes(s+14,18) || !jc_zero_bytes(b+384,128)) return JR_VALID_INVALID;
    if(memcmp(b+JR_META_ASSETS,jc_assets,sizeof jc_assets)) return JR_VALID_UNSUPPORTED;
    if(p[8]>4 || p[9]>1 || p[10]>1 || p[11]>3 || p[12]>3 || p[13]>1 ||
       p[14]>1 || p[15]>1 || p[16]>1 || p[17]>1 || p[18]>1 || p[19]>2 || p[20]>17 ||
       !jc_zero_bytes(p+21,11) || !jc_zero_bytes(p+70,58)) return JR_VALID_INVALID;
    for(i=64;i<68;i++) if(p[i]>16) return JR_VALID_INVALID;
    if(p[68]>1 || p[69]<1 || p[69]>12) return JR_VALID_INVALID;
    for(i=0;i<16;i++) {
        uint32_t e=jc_u16(p+32+2*i), cc=e&127u, code=e>>9;
        if(e && ((!code || (code>63 && code<120)) || cc==0 || cc>=120 || cc==1 ||
           cc==6 || cc==11 || cc==32 || cc==38 || cc==64 || (cc>=96 && cc<=101)))
            return JR_VALID_INVALID;
    }
    return JR_VALID_OK;
}
/* Common track parameter range maxima and minima, in the exact P_* order. */
static const int16_t jc_pmin[63]={
    0,0,0,0,0,-64,-64,-64,-64,0,0,0,0,-64,-64,-64,0,
    0,0,1,1,0,0,0,0,0,0,0,-24,1,0,0,1,0,0,0,0,0,0,-64,0,
    0,0,0,0,0,1,0,0,0,-64,-60,0,0,0,0,0,0,0,0,0,0,0};
static const int16_t jc_pmax[63]={
    127,127,127,127,127,63,63,63,63,127,4,127,127,63,63,63,127,
    5,5,4,127,100,127,1,1,11,15,2,24,64,8,100,127,127,127,127,127,3,127,63,1,
    1,2,1,127,2,16,5,127,8,63,60,4,8,127,127,127,127,2,1,10,1,1};
static const int16_t jc_emin[12][8]={
    {0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0},{0,-24,0,0,0,0,0,0},{0,0,0,-12,0,0,0,0},
    {0,-24,-24,0,0,0,0,0},{0,-8,-8,-8,0,0,0,0},{0,0,0,0,-24,0,0,0},
    {0,-7,-64,-16,-64,-7,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}};
static const int16_t jc_emax[12][8]={
    {4,127,127,127,127,127,127,127},{7,14,14,14,127,127,127,0},
    {7,8,127,127,127,1,127,0},{4,4,127,127,127,127,3,127},
    {11,24,127,1,127,0,127,0},{127,127,127,12,127,127,127,127},
    {15,24,24,50,3,127,127,127},{15,8,8,8,6,127,127,2},
    {11,127,127,127,24,127,127,127},{32,7,63,16,63,7,127,34},
    {3,127,127,127,127,127,127,127},{3,127,127,127,127,127,127,127}};
static int jc_param(uint32_t engine, uint32_t track, uint32_t id, int32_t value)
{
    int32_t lo,hi;
    if(id>=JC_PARAM_COUNT) return JR_VALID_INVALID;
    if(id<JC_E0) { lo=jc_pmin[id]; hi=jc_pmax[id]; }
    else if(track==3 && id==JC_E0) { lo=0; hi=45; }
    else { lo=jc_emin[engine][id-JC_E0]; hi=jc_emax[engine][id-JC_E0]; }
    if(value<lo || value>hi) return JR_VALID_INVALID;
    /* Sample slots, paired samples and mutable FM6 bank slots are outside the
     * captured dependency closure. SYN1..4 are covered by the shared bank. */
    if(track==3 && id==JC_E0 && value>=37 && value<=41) return JR_VALID_UNSUPPORTED;
    if(track!=3 && (engine==4 || engine==8) && id==JC_E0 && value>=8) return JR_VALID_UNSUPPORTED;
    if(track!=3 && engine==9 && id==70 && value>=8) return JR_VALID_UNSUPPORTED;
    return JR_VALID_OK;
}
static int jc_lockable(uint32_t id)
{
    return id<=16 || id==32 || (id>=33 && id<=36) || id==38 || id==39 || id==44 ||
        (id>=45 && id<=48) || id>=63 || id==50 || (id>=54 && id<=57);
}
static int jc_identity(const uint8_t *b, uint32_t track, uint32_t engine)
{
    uint32_t id=jc_u32(b), origin=b[4];
    if(b[5]!=engine || b[6]>1 || b[7]) return JR_VALID_INVALID;
    if(origin==1) {
        uint32_t bank=id&255u;
        if((id>>8)!=track+1u || !bank || bank>(track==3 ? 8u:12u)) return JR_VALID_INVALID;
        if(track<3 && jc_curated_engine[track][bank-1u]!=engine) return JR_VALID_INVALID;
    } else if(origin==2) {
        if(!id || id>(track==3 ? 46u:jc_npreset[engine])) return JR_VALID_INVALID;
    } else if(origin==3) {
        if(id || !b[6]) return JR_VALID_INVALID;
    } else return JR_VALID_UNSUPPORTED;
    return JR_VALID_OK;
}
static int jc_fm6(const jr_source_t *s, uint32_t snapshot, uint32_t part, uint32_t engine)
{
    static const uint8_t opmax[21]={99,99,99,99,99,99,99,99,99,99,99,3,3,7,3,7,99,1,31,99,14};
    static const uint8_t vmax[19]={99,99,99,99,99,99,99,99,31,7,1,99,99,99,99,1,5,7,48};
    uint8_t b[156]; uint32_t i; int r;
    r=jc_read(s,JR_FM6_OFFSET+(snapshot*3u+part)*156u,b,sizeof b); if(r) return r;
    if(engine!=9) return jc_zero_bytes(b,sizeof b) ? JR_VALID_OK:JR_VALID_INVALID;
    for(i=0;i<155;i++) {
        if(i>=145) { if(b[i]<32 || b[i]>126) return JR_VALID_INVALID; }
        else if(b[i]>(i<126 ? opmax[i%21u]:vmax[i-126u])) return JR_VALID_INVALID;
    }
    return b[155] ? JR_VALID_INVALID:JR_VALID_OK;
}
static int jc_snapshot(const jr_source_t *s, uint32_t snapshot, const uint8_t *identities)
{
    static const int16_t gmin[32]={40,0,0,-50,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,-64,0,0};
    static const int16_t gmax[32]={240,100,2,50,8,120,127,127,127,127,127,127,1,2,17,127,4,0,1,1,11,1,1,1,16383,127,127,127,127,63,4,1};
    uint8_t h[134], step[10], lengths[4], lock[4], hashbuf[128], mf[64];
    uint32_t base=snapshot*JR_SNAPSHOT_BYTES, i,k,t,hval=0x811c9dc5u; int r;
    r=jc_read(s,base,h,76); if(r) return r;
    if(jc_u32(h)!=0x4a524d31u) return JR_VALID_UNSUPPORTED;
    if(jc_u32(h+4)!=4584u || h[72]>3 || h[73]>127 ||
       (h[74]&0xc0u)!=0x80u || h[75]>4) return JR_VALID_INVALID;
    if((h[74]&15u)>10) return JR_VALID_INVALID;
    if((h[74]&15u)>=7) return JR_VALID_UNSUPPORTED;
    for(i=0;i<32;i++) {
        int32_t v=jc_i16(h+8+2*i);
        if(v<gmin[i] || v>gmax[i]) return JR_VALID_INVALID;
    }
    for(t=0;t<4;t++) {
        uint32_t tb=base+JC_TRACK_BASE+t*JC_TRACK_BYTES, engine;
        r=jc_read(s,tb,h,JC_TRACK_HEAD); if(r) return r;
        engine=h[132];
        if(engine>=12) return JR_VALID_UNSUPPORTED;
        if(t==3 && engine) return JR_VALID_INVALID;
        if(t<3 && h[133]!=255 && h[133]>=jc_npreset[engine]) return JR_VALID_INVALID;
        r=jc_identity(identities+t*8,t,engine); if(r) return r;
        for(i=0;i<71;i++) {
            int32_t v=i<53 ? jc_i16(h+2*i):i<63 ? h[122+i-53]:jc_i16(h+106+2*(i-63));
            r=jc_param(engine,t,i,v); if(r) return r;
        }
        for(k=0;k<64;k++) {
            r=jc_read(s,tb+134+k*10,step,sizeof step); if(r) return r;
            if(t<3) {
                if(step[4]>4 || step[5]>2 || (step[6]&~3u) || step[7]>127) return JR_VALID_INVALID;
                for(i=0;i<4;i++) if(step[i]>127) return JR_VALID_INVALID;
                r=jc_read(s,base+JC_NOTE_LENGTHS+(t*64+k)*4,lengths,sizeof lengths); if(r) return r;
                for(i=0;i<4;i++) if(lengths[i]>64 || (lengths[i] && (step[5]!=0 || i>=step[4])))
                    return JR_VALID_INVALID;
            }
        }
        /* Six-bit signed microtiming spans -32..31 exactly. Fill=3 is a
         * reserved legacy alias that proj_apply would silently turn into 0. */
        r=jc_read(s,tb+774,mf,sizeof mf); if(r) return r;
        for(k=0;k<64;k++) if((mf[k]>>6)==3) return JR_VALID_INVALID;
        for(k=0;k<24;k++) {
            r=jc_read(s,tb+838+k*4,lock,sizeof lock); if(r) return r;
            if(lock[0]==255) continue; /* ignored free-slot bytes can be stale */
            if(lock[0]>=64 || !jc_lockable(lock[1])) return JR_VALID_INVALID;
            r=jc_param(engine,t,lock[1],jc_i16(lock+2)); if(r) return r;
        }
        if(t<3) { r=jc_fm6(s,snapshot,t,engine); if(r) return r; }
    }
    for(i=0;i<JC_SUM;) {
        uint32_t take=JC_SUM-i>sizeof hashbuf ? sizeof hashbuf:JC_SUM-i;
        r=jc_read(s,base+i,hashbuf,take); if(r) return r;
        for(k=0;k<take;k++) hval=(hval^hashbuf[k])*16777619u;
        i+=take;
    }
    r=jc_read(s,base+JC_SUM,hashbuf,4); if(r) return r;
    return jc_u32(hashbuf)==hval ? JR_VALID_OK:JR_VALID_INVALID;
}
static int jc_syn(const jr_source_t *s)
{
    uint8_t h[12], d[22]; uint32_t k,i,j; int r;
    r=jc_read(s,JR_SYN_OFFSET,h,8); if(r) return r;
    if(jc_u32(h)!=0x31555344u || jc_u16(h+4)!=1) return JR_VALID_UNSUPPORTED;
    if(jc_u16(h+6)!=4) return JR_VALID_INVALID;
    for(k=0;k<4;k++) {
        uint32_t base=JR_SYN_OFFSET+8+k*364;
        r=jc_read(s,base,h,12); if(r) return r;
        for(i=0;i<8;i++) if(h[i] && (h[i]<32 || h[i]>126)) return JR_VALID_INVALID;
        if(h[9]>=32 || h[10] || h[11]) return JR_VALID_INVALID;
        for(j=0;j<16;j++) {
            r=jc_read(s,base+12+j*22,d,22); if(r) return r;
            if(d[0]>5 || (d[1]&15u)>4 || (d[1]&~0x1fu) || d[2]>127 || d[3]>15 || d[4]>96 ||
               d[5]>127 || d[7]>127 || d[8]>127 || d[10]>127 || d[11]>127 || d[12]>127 ||
               d[14]>127 || d[16]>127 || d[18]>127 || d[19]>127 || d[20]>127) return JR_VALID_INVALID;
        }
    }
    return JR_VALID_OK;
}
int jr_codec_validate(void *unused, const jr_source_t *s)
{
    uint8_t metadata[512], arr[36]; uint32_t mask,i; int r;
    (void)unused;
    if(!s || !s->read || s->length!=JR_PAYLOAD_BYTES) return JR_VALID_INVALID;
    r=jc_read(s,JR_METADATA_OFFSET,metadata,sizeof metadata); if(r) return r;
    r=jc_metadata(metadata); if(r) return r;
    mask=metadata[JR_META_SESSION+8];
    for(i=0;i<5;i++) {
        if(!i || (mask&(1u<<(i-1u)))) {
            r=jc_snapshot(s,i,metadata+i*32); if(r) return r;
        } else {
            if(!jc_zero_bytes(metadata+i*32,32)) return JR_VALID_INVALID;
            r=jc_zero(s,i*JR_SNAPSHOT_BYTES,JR_SNAPSHOT_BYTES); if(r) return r;
            r=jc_zero(s,JR_FM6_OFFSET+i*3*JR_FM6_BYTES,3*JR_FM6_BYTES); if(r) return r;
        }
    }
    r=jc_syn(s); if(r) return r;
    r=jc_read(s,JR_ARRANGEMENT_OFFSET,arr,sizeof arr); if(r) return r;
    if(!arr[0] || arr[0]>16 || arr[1]>1 || arr[2] || arr[3]) return JR_VALID_INVALID;
    for(i=0;i<16;i++) {
        uint32_t section=arr[4+i*2], bars=arr[5+i*2];
        if(section>3 || !bars || bars>64) return JR_VALID_INVALID;
        if(metadata[JR_META_SESSION+12] && i<arr[0] && !(mask&(1u<<section))) return JR_VALID_INVALID;
    }
    return JR_VALID_OK;
}
