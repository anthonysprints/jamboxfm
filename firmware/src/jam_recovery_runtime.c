/* SPDX-License-Identifier: GPL-3.0-only */
/* Development-only manual recovery. No automatic save or legacy persistence. */
#if JAMBOX_PROTO && JAMBOX_RECOVERY
#define JR_FREESTANDING 1 /* the amalgamation already supplies its tiny libc */
#include "../experimental/recovery/jr_codec.c"
#include "../experimental/recovery/jr_store.c"
#undef JR_FREESTANDING

static project_t jrr_work __attribute__((section(".pool")));
static jam_recovery_sidecar_t jrr_current;
static uint8_t jrr_metadata[JR_METADATA_BYTES];
static uint32_t jrr_sums[5];
static jr_store_state_t jrr_store;
static uint32_t jrr_saved_input, jrr_saved_midi;
static uint8_t jrr_saved, jrr_dirty, jrr_error, jrr_saving, jrr_audio_frozen;
static const jr_nor_t jrr_nor = {0, jr_hw_read, jr_hw_erase, jr_hw_program};

static uint8_t jrr_le(uint32_t value, uint32_t byte)
{ return (uint8_t)(value >> (byte * 8u)); }
static uint32_t jrr_u32(const uint8_t *p)
{ return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
static void jrr_put16(uint8_t *p, uint32_t value)
{ p[0]=(uint8_t)value; p[1]=(uint8_t)(value>>8); }

/* Explicit wire fields; neither native padding nor native byte order escapes. */
static uint8_t jrr_project_byte(const project_t *p, uint32_t off, uint32_t sum)
{
    if (off < 4) return jrr_le(PROJ_MAGIC, off);
    if (off < 8) return jrr_le(JR_SNAPSHOT_BYTES, off-4);
    if (off < 72) return jrr_le((uint16_t)p->g[(off-8)/2], (off-8)%2);
    if (off == 72) return p->sel;
    if (off == 73) return p->drdly;
    if (off < 76) return p->rsv[off-74];
    if (off < 3812) {
        const proj_trk_t *t=&p->t[(off-76)/934];
        uint32_t at=(off-76)%934, i;
        if (at < 106) return jrr_le((uint16_t)t->p[at/2], at%2);
        if (at < 122) return jrr_le((uint16_t)t->e[(at-106)/2], (at-106)%2);
        if (at < 132) return t->x[at-122];
        if (at == 132) return t->engine;
        if (at == 133) return t->preset;
        if (at < 774) {
            i=(at-134)/10; at=(at-134)%10;
            if ((off-76)/934 == TRK_DRUM) {
                const dstep_t *s=&t->dstep[i];
                return at<2 ? s->on[at] : at<6 ? s->lvl[at-2] : s->rat[at-6];
            } else {
                const step_t *s=&t->step[i];
                switch(at) {
                case 4:return s->n; case 5:return s->time; case 6:return s->flags;
                case 7:return s->vel; case 8:return s->lvl; case 9:return s->rat;
                default:return s->note[at];
                }
            }
        }
        if (at < 838) return t->mf[at-774];
        i=(at-838)/4; at=(at-838)%4;
        return at==0 ? t->lock[i].step : at==1 ? t->lock[i].param :
            jrr_le((uint16_t)t->lock[i].val, at-2);
    }
    if (off < 4580) {
        uint32_t at=off-3812;
        return p->note_len[at/256][(at%256)/4][at%4];
    }
    return jrr_le(sum, off-4580);
}
static uint32_t jrr_project_sum(const project_t *p)
{
    uint32_t h=0x811c9dc5u;
    for (uint32_t i=0;i<4580;i++) h=(h^jrr_project_byte(p,i,0))*16777619u;
    return h;
}
static uint8_t jrr_syn_byte(uint32_t at)
{
    if(at<4) return jrr_le(DSU_MAGIC,at);
    if(at<6) return jrr_le(1,at-4);
    if(at<8) return jrr_le(4,at-6);
    const dsu_kit_t *k=&dsu.k[(at-8)/364]; at=(at-8)%364;
    if(at<8)return (uint8_t)k->name[at];
    if(at==8)return k->crush;
    if(at==9)return k->src;
    if(at<12)return 0;
    const dsnd_t *s=&k->s[(at-12)/22];
    switch((at-12)%22) {
    case 0:return s->wave; case 1:return s->src; case 2:return s->pitch;
    case 3:return s->fine; case 4:return s->bend; case 5:return s->btime;
    case 6:return s->hold; case 7:return s->decay; case 8:return s->tlev;
    case 9:return s->t2; case 10:return s->t2lev; case 11:return s->click;
    case 12:return s->nlev; case 13:return s->nhold; case 14:return s->ndec;
    case 15:return s->flt; case 16:return s->fcut; case 17:return (uint8_t)s->fenv;
    case 18:return s->hpf; case 19:return s->chip; case 20:return s->drive;
    default:return s->level;
    }
}
static int jrr_source_read(void *ctx, uint32_t off, void *dst, uint32_t n)
{
    uint8_t *out=dst; (void)ctx;
    if(off>JR_PAYLOAD_BYTES || n>JR_PAYLOAD_BYTES-off || (!dst && n))return -1;
    fm1_wdt_feed();
    while(n--) {
        uint8_t v=0;
        if(off<JR_FM6_OFFSET) {
            uint32_t s=off/JR_SNAPSHOT_BYTES;
            if(!s || (jrr_metadata[168]&(1u<<(s-1))))
                v=jrr_project_byte(s ? project_slot(s-1):&jrr_work,off%JR_SNAPSHOT_BYTES,jrr_sums[s]);
        } else if(off<JR_SYN_OFFSET) {
            uint32_t at=off-JR_FM6_OFFSET, s=at/468, t=(at%468)/156;
            const project_t *p=s ? project_slot(s-1):&jrr_work;
            if((!s || (jrr_metadata[168]&(1u<<(s-1)))) && p->t[t].engine==ENGI_FM6)
                v=(s ? jam_recovery_section_sidecar(s-1):&jrr_current)->fm6[t][at%156];
        } else if(off<JR_METADATA_OFFSET) v=jrr_syn_byte(off-JR_SYN_OFFSET);
        else if(off<JR_ARRANGEMENT_OFFSET) v=jrr_metadata[off-JR_METADATA_OFFSET];
        else {
            uint32_t at=off-JR_ARRANGEMENT_OFFSET;
            if(at==0)v=arrangement.count;
            else if(at==1)v=arrangement.loop;
            else if(at>=4)v=(at&1u) ? arrangement.entry[(at-4)/2].bars:arrangement.entry[(at-4)/2].scene;
        }
        *out++=v; ++off;
    }
    return 0;
}
static void jrr_identity(uint32_t s, const project_t *p, const jam_recovery_sidecar_t *c)
{
    for(uint32_t t=0;t<4;t++) {
        int curated=(c->sound_valid&(1u<<t)) && c->sound_id[t]<jam_sound_count(t);
        jr_codec_identity_set(jrr_metadata,s,t,curated ? (t+1)*256u+c->sound_id[t]+1u:0u,
            curated?1:3,p->t[t].engine,1); /* parameters are the authority, never reload a recipe */
    }
}
static int jrr_capture(void)
{
    uint32_t mask=0;
    if(!dsu_valid(&dsu))dsu_defaults();
    proj_capture(&jrr_work); jam_recovery_sidecar_capture(&jrr_current);
    for(uint32_t i=0;i<4;i++)if(proj_ok(project_slot(i)))mask|=1u<<i;
    jr_codec_metadata_init(jrr_metadata,(uint8_t)mask);
    jrr_sums[0]=jrr_project_sum(&jrr_work);jrr_identity(0,&jrr_work,&jrr_current);
    for(uint32_t i=0;i<4;i++)if(mask&(1u<<i)) {
        const jam_recovery_sidecar_t *c=jam_recovery_section_sidecar(i);
        if(!(c->sound_valid&JAM_RECOVERY_SIDECAR_READY))return -1;
        jrr_sums[i+1]=jrr_project_sum(project_slot(i));jrr_identity(i+1,project_slot(i),c);
    }
    uint8_t *s=jrr_metadata+JR_META_SESSION,*p=jrr_metadata+JR_META_PREFS;
    s[9]=song.sel;s[10]=(uint8_t)song.octave;s[11]=ui.jam_home_view;
    s[12]=arrangement_enabled!=0;s[13]=live_sec>=0 && live_sec<4 && (mask&(1u<<live_sec)) ? (uint8_t)live_sec:255;
    p[8]=(uint8_t)settings.palette;p[9]=(uint8_t)settings.lowcut;p[10]=(uint8_t)settings.zoom;
    p[11]=lights_lvl;p[12]=lights_keys;p[13]=lights_notes;p[14]=usb_full;p[15]=usb_serial;
    p[16]=rec_tempo;p[17]=rec_count;p[18]=lights_mout;p[19]=lights_sync;p[20]=lights_min;
    for(uint32_t i=0;i<16;i++)jrr_put16(p+32+i*2,ml_tab[i]);
    memcpy(p+64,midi_set,6);
    return 0;
}
static int jrr_transfer_pending(void)
{
    if(usb.ota_req || usb.uboot_req || sx_ready || sx_collect || sx_busy || so_w!=so_r)return 1;
    for(uint32_t i=0;i<SMP_USER_SLOTS;i++)if(ed_smp_open[i])return 1;
#if FELUCCA_UAC
    if(uac.feed)return 1;
#endif
    return 0;
}
static int jrr_validate(void *ctx, const jr_source_t *source)
{
    (void)ctx;
    if(jr_transaction_active && jrr_transfer_pending())return JR_VALID_IO;
    return jr_codec_validate(0,source);
}
static uint32_t jrr_audio_pause(void)
{
    uint32_t previous=0;
#ifndef JAM_RECOVERY_HOST
    previous=fm1_irq_pause(FM1_IRQ_ALNK0);
    audio_silence();
#endif
    jrr_audio_frozen=1;
    return previous;
}
static void jrr_audio_resume(uint32_t previous)
{
    jrr_audio_frozen=0;
#ifndef JAM_RECOVERY_HOST
    fm1_irq_resume(FM1_IRQ_ALNK0,previous);
#else
    (void)previous;
#endif
}
static void jam_recovery_mark_dirty(void) { jrr_dirty=1; }
static int jam_recovery_is_claimed(void) { return jrr_store.allocation_claimed; }
static const char *jam_recovery_block_reason(void)
{
    if(!flash_ok)return "FLASH UNAVAILABLE";
    if(jrr_saving)return "SAVING";
    if(!jrr_store.writable)return "RECOVERY READ ONLY";
    if(song.playing || transport_req)return "STOP PLAYBACK FIRST";
    if(song.rec || rec_wait || ft_on || ci_on)return "STOP RECORDING FIRST";
    if(fm1_in.notes)return "RELEASE KEYS FIRST";
    for(uint32_t i=0;i<NTRK;i++)if(trk[i].nheld || trk[i].arp_phys)return "RELEASE NOTES FIRST";
    for(uint32_t t=0;t<NPART;t++)for(uint32_t v=0;v<NVOICE;v++)
        if(trk[t].v[v].active && trk[t].v[v].gate)return "RELEASE NOTES FIRST";
    for(uint32_t t=0;t<NTRK;t++)if(pc_req[t])return "WAIT FOR SOUND";
    for(uint32_t t=0;t<NPART;t++)
        if(trk[t].eng_req==ENGI_FM6 &&
            (fm6_slot[t]!=trk[t].p[P_E7] || (fm6_pend&(1u<<t))))return "WAIT FOR SOUND";
    for(uint32_t i=0;i<16;i++)if(ch_sus[i])return "RELEASE SUSTAIN";
    if(jrr_transfer_pending())return "CLOSE USB TRANSFER";
    return 0;
}
static int jam_recovery_can_save(void) { return !jam_recovery_block_reason(); }
static const char *jam_recovery_status(void)
{
    if(jrr_saving)return "Saving";
    if(jrr_error)return "Error";
    if(!jrr_store.writable)return "Read only";
    if(!jrr_store.allocation_claimed)return "Setup needed";
    if(jrr_saved_input!=ui_input_ms || jrr_saved_midi!=mi_w)jrr_dirty=1;
    return jrr_saved && !jrr_dirty ? "Saved":"Pending";
}
static int jam_recovery_save(int initialize)
{
    jr_source_t source={0,jrr_source_read,JR_PAYLOAD_BYTES};
    if(!jam_recovery_can_save())return JR_STORE_READ_ONLY;
    uint32_t paused=jrr_audio_pause();
    /* The audio ISR may have processed a note/transport/program event between
     * the UI's first check and masking its source. Recheck the frozen state. */
    if(!jam_recovery_can_save()) { jrr_audio_resume(paused);return JR_STORE_READ_ONLY; }
    jam_inputs_reset();
    for(uint32_t t=0;t<NTRK;t++)trk_all_off(&trk[t]);
    jrr_saving=1;jrr_error=0;
    ui.force=1;jam_recovery_ui_draw();
#ifndef JAM_RECOVERY_HOST
    lcd_sync();
#endif
    int result=JR_STORE_INVALID_SOURCE;
    if(!jrr_capture()) {
        jr_store_state_t updated;
        jr_transaction_active=1;
        result=jr_store_save(&jrr_nor,&source,jrr_validate,0,initialize,&updated);
        jr_transaction_active=0;
        /* Rejected input does not invalidate a prior scan or touch NOR. Keep
         * Save available after the user selects supported built-in sounds. */
        if(result!=JR_STORE_INVALID_SOURCE && result!=JR_STORE_INVALID_ARGUMENT)
            jrr_store=updated;
    }
    jam_inputs_reset();
    for(uint32_t e=0;e<NE;e++)(void)panel_enc(e); /* discard turns accumulated during the pause */
    jrr_saving=0;
    if(!result) { jrr_saved=1;jrr_dirty=0;jrr_saved_input=ui_input_ms;jrr_saved_midi=mi_w; }
    else { jrr_error=1;jrr_dirty=1; }
    jrr_audio_resume(paused);
    return result;
}

typedef struct { const jr_source_t *source;uint32_t at;int failed; } jrr_reader_t;
static uint32_t jrr_read(jrr_reader_t *r,uint32_t n)
{
    uint8_t b[4]={0,0,0,0};
    if(r->failed || r->source->read(r->source->ctx,r->at,b,n))r->failed=1;
    r->at+=n;return (uint32_t)b[0]|(uint32_t)b[1]<<8|(uint32_t)b[2]<<16|(uint32_t)b[3]<<24;
}
static int jrr_decode_project(const jr_source_t *source,uint32_t s,project_t *p)
{
    jrr_reader_t r={source,s*JR_SNAPSHOT_BYTES,0};
    memset(p,0,sizeof *p);
    p->magic=jrr_read(&r,4);p->size=jrr_read(&r,4);
    for(uint32_t i=0;i<32;i++)p->g[i]=(int16_t)jrr_read(&r,2);
    p->sel=(uint8_t)jrr_read(&r,1);p->drdly=(uint8_t)jrr_read(&r,1);
    p->rsv[0]=(uint8_t)jrr_read(&r,1);p->rsv[1]=(uint8_t)jrr_read(&r,1);
    for(uint32_t t=0;t<4;t++) {
        proj_trk_t *q=&p->t[t];
        for(uint32_t i=0;i<53;i++)q->p[i]=(int16_t)jrr_read(&r,2);
        for(uint32_t i=0;i<8;i++)q->e[i]=(int16_t)jrr_read(&r,2);
        for(uint32_t i=0;i<10;i++)q->x[i]=(uint8_t)jrr_read(&r,1);
        q->engine=(uint8_t)jrr_read(&r,1);q->preset=(uint8_t)jrr_read(&r,1);
        for(uint32_t i=0;i<64;i++) {
            if(t==TRK_DRUM) {
                for(uint32_t k=0;k<2;k++)q->dstep[i].on[k]=(uint8_t)jrr_read(&r,1);
                for(uint32_t k=0;k<4;k++)q->dstep[i].lvl[k]=(uint8_t)jrr_read(&r,1);
                for(uint32_t k=0;k<4;k++)q->dstep[i].rat[k]=(uint8_t)jrr_read(&r,1);
            } else {
                step_t *v=&q->step[i];
                for(uint32_t k=0;k<4;k++)v->note[k]=(uint8_t)jrr_read(&r,1);
                v->n=(uint8_t)jrr_read(&r,1);v->time=(uint8_t)jrr_read(&r,1);v->flags=(uint8_t)jrr_read(&r,1);
                v->vel=(uint8_t)jrr_read(&r,1);v->lvl=(uint8_t)jrr_read(&r,1);v->rat=(uint8_t)jrr_read(&r,1);
            }
        }
        for(uint32_t i=0;i<64;i++)q->mf[i]=(uint8_t)jrr_read(&r,1);
        for(uint32_t i=0;i<24;i++) {
            q->lock[i].step=(uint8_t)jrr_read(&r,1);q->lock[i].param=(uint8_t)jrr_read(&r,1);
            q->lock[i].val=(int16_t)jrr_read(&r,2);
        }
    }
    for(uint32_t t=0;t<3;t++)for(uint32_t i=0;i<64;i++)for(uint32_t k=0;k<4;k++)p->note_len[t][i][k]=(uint8_t)jrr_read(&r,1);
    uint32_t sum=jrr_read(&r,4);
    if(r.failed || p->magic!=PROJ_MAGIC || p->size!=sizeof *p ||
            sum!=jrr_project_sum(p))return -1;
    p->sum=proj_sum(p);return 0;
}
static int jrr_decode_syn(const jr_source_t *source)
{
    jrr_reader_t r={source,JR_SYN_OFFSET,0};
    dsu.magic=jrr_read(&r,4);dsu.ver=(uint16_t)jrr_read(&r,2);dsu.n=(uint16_t)jrr_read(&r,2);
    for(uint32_t k=0;k<4;k++) {
        dsu_kit_t *u=&dsu.k[k];
        for(uint32_t i=0;i<8;i++)u->name[i]=(char)jrr_read(&r,1);
        u->crush=(uint8_t)jrr_read(&r,1);u->src=(uint8_t)jrr_read(&r,1);(void)jrr_read(&r,2);
        u->rsv[0]=u->rsv[1]=0;
        for(uint32_t n=0;n<16;n++) {
            dsnd_t *v=&u->s[n];
#define JRR_DS(field) v->field=(uint8_t)jrr_read(&r,1)
            JRR_DS(wave);JRR_DS(src);JRR_DS(pitch);JRR_DS(fine);JRR_DS(bend);JRR_DS(btime);
            JRR_DS(hold);JRR_DS(decay);JRR_DS(tlev);JRR_DS(t2);JRR_DS(t2lev);JRR_DS(click);
            JRR_DS(nlev);JRR_DS(nhold);JRR_DS(ndec);JRR_DS(flt);JRR_DS(fcut);
            v->fenv=(int8_t)jrr_read(&r,1);JRR_DS(hpf);JRR_DS(chip);JRR_DS(drive);JRR_DS(level);
#undef JRR_DS
        }
    }
    dsu_dirty=0;return r.failed || !dsu_valid(&dsu)?-1:0;
}
static void jrr_defaults(void)
{
    for(uint32_t i=0;i<4;i++)memset(project_slot(i),0,sizeof(project_t));
    jam_recovery_sidecars_reset();dsu_defaults();dsu_dirty=0;
    memset(&arrangement_clock,0,sizeof arrangement_clock);arr_defaults(&arrangement);arrangement_enabled=0;live_sec=-1;
    memset(&settings,0,sizeof settings);settings_init();
    lights_lvl=lights_keys=lights_sync=lights_mout=lights_min=0;lights_notes=1;
    usb_full=usb_serial=0;memset((void *)ml_tab,0,sizeof ml_tab);
    static const uint8_t defaults[6]=MIDI_SET_DEF;memcpy(midi_set,defaults,6);
    felucca_init();
}
static int jrr_apply(const jr_source_t *source,uint32_t expected_crc)
{
    uint8_t arr[36];
    if(jr_codec_validate(0,source)!=JR_VALID_OK)return -1;
    jrr_defaults();
    if(source->read(source->ctx,JR_METADATA_OFFSET,jrr_metadata,sizeof jrr_metadata))return -1;
    uint32_t mask=jrr_metadata[168];
    for(uint32_t s=0;s<5;s++)if(!s || (mask&(1u<<(s-1)))) {
        project_t *p=s ? project_slot(s-1):&jrr_work;
        jam_recovery_sidecar_t *c=s ? jam_recovery_section_sidecar(s-1):&jrr_current;
        if(jrr_decode_project(source,s,p))return -1;
        jrr_sums[s]=jrr_project_sum(p);
        memset(c,0,sizeof *c);
        for(uint32_t t=0;t<3;t++)if(source->read(source->ctx,JR_FM6_OFFSET+(s*3+t)*156,c->fm6[t],156))return -1;
        for(uint32_t t=0;t<4;t++) {
            const uint8_t *id=jrr_metadata+(s*4+t)*8;
            if(id[4]==1) { c->sound_valid|=(uint8_t)(1u<<t);c->sound_id[t]=(uint8_t)((jrr_u32(id)&255u)-1); }
        }
        c->sound_valid|=JAM_RECOVERY_SIDECAR_READY;
    }
    if(jrr_decode_syn(source) || source->read(source->ctx,JR_ARRANGEMENT_OFFSET,arr,36))return -1;
    arrangement.count=arr[0];arrangement.loop=arr[1];arrangement.reserved[0]=arrangement.reserved[1]=0;
    for(uint32_t i=0;i<16;i++) { arrangement.entry[i].scene=arr[4+i*2];arrangement.entry[i].bars=arr[5+i*2]; }
    /* Bind the staged values themselves to the committed CRC. Re-reading NOR
     * would miss a transient successful but incorrect read during decoding. */
    jr_source_t staged={0,jrr_source_read,JR_PAYLOAD_BYTES};
    uint8_t page[JR_STORE_PAGE_BYTES];uint32_t staged_crc;
    if(source_crc(&staged,&staged_crc,page) || staged_crc!=expected_crc ||
            jr_codec_validate(0,&staged)!=JR_VALID_OK)return -1;
    proj_apply_with_sidecar(&jrr_work,1,&jrr_current);
    for(uint32_t t=0;t<3;t++)trk[t].engine=trk[t].eng_req;
    const uint8_t *p=jrr_metadata+JR_META_PREFS,*s=jrr_metadata+JR_META_SESSION;
    settings.magic=SETTINGS_MAGIC;settings.palette=p[8];settings.lowcut=p[9];settings.zoom=p[10];settings_init();
    lights_lvl=p[11];lights_keys=p[12];lights_notes=p[13];usb_full=p[14];usb_serial=p[15];rec_tempo=p[16];rec_count=p[17];
    lights_mout=p[18];lights_sync=p[19];lights_min=p[20];
    for(uint32_t i=0;i<16;i++)ml_tab[i]=(uint16_t)((uint32_t)p[32+i*2]|(uint32_t)p[33+i*2]<<8);
    memcpy(midi_set,p+64,6);
    song.g[G_SYNC]=lights_sync;song.g[G_MIDI]=lights_mout;song.g[G_ROUTE]=lights_min;
    song.g[G_CH1]=midi_set[0];song.g[G_CH2]=midi_set[1];song.g[G_CH3]=midi_set[2];
    song.g[G_DRCH]=midi_set[3];song.g[G_CHOTH]=midi_set[4];song.g[G_BEND]=midi_set[5];
    jam_inputs_reset();song.playing=song.rec=rec_wait=ft_on=ci_on=0;transport_req=0;
    memset(&arrangement_clock,0,sizeof arrangement_clock);arrangement_enabled=s[12];live_sec=s[13]==255?-1:(int8_t)s[13];
    song.sel=s[9];song.octave=(int8_t)s[10];ui.jam_home_view=s[11];go_home();ui.force=1;
    return 0;
}
static void jam_recovery_boot(void)
{
    jrr_saved=jrr_error=jrr_saving=0;jrr_dirty=1;
    memset(&jrr_store,0,sizeof jrr_store);jrr_store.winner=-1;
    if(!flash_ok)return;
    (void)jr_store_scan(&jrr_nor,jrr_validate,0,&jrr_store);
    /* A previous application reset before becoming healthy. Preserve the
     * record, start fresh, and leave storage read only for this boot. */
    if(bootguard.magic==BOOTGUARD_MAGIC && bootguard.failed) {
        jrr_defaults();jrr_store.writable=0;return;
    }
    int first=jrr_store.winner;
    for(int attempt=0;attempt<2;attempt++) {
        int copy=attempt ? (first<0?-1:1-first):first;
        if(copy<0 || jrr_store.copies[copy].status!=JR_CANDIDATE_VALID)continue;
        jr_store_reader_t reader;jr_source_t source;
        jr_store_source(&reader,&jrr_nor,(unsigned)copy,jrr_store.copies[copy].length,&source);
        if(!jrr_apply(&source,jrr_store.copies[copy].payload_crc)) {
            jrr_saved=1;jrr_dirty=0;jrr_saved_input=ui_input_ms;jrr_saved_midi=mi_w;return;
        }
        jrr_defaults();jrr_store.writable=0; /* uncertain reads must never authorize a write */
    }
    jrr_defaults();
}
#endif
