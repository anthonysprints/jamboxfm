/* SPDX-License-Identifier: GPL-3.0-only */
/* Real firmware DSP and sequencer; host only. The optional WAV is an audition
 * fixture with demo notes, distinct from the drums-only fresh session. */
#define main upstream_hostsim_main
#include "hostsim.c"
#undef main
#include "../firmware/src/jam_palette.c"

static uint32_t jam_checks, jam_failures;
#define CHECK(c, label) do { jam_checks++; if (!(c)) { fprintf(stderr, "FAIL: %s\n", label); jam_failures++; } } while (0)

typedef struct {
    step_t steps[NSTEP];
    int8_t micro[NSTEP];
    uint8_t fill[NSTEP / 4];
    plock_t locks[NLOCK];
} pattern_snapshot_t;
static void pattern_copy(pattern_snapshot_t *p, const track_t *t)
{
    memcpy(p->steps, t->step, sizeof p->steps);
    memcpy(p->micro, t->micro, sizeof p->micro);
    memcpy(p->fill, t->fill, sizeof p->fill);
    memcpy(p->locks, t->lock, sizeof p->locks);
}
static void block_run(void) { int32_t b[CTL * 2]; mix_block(b, CTL); }

static void test_session(void)
{
    uint32_t k, i, melodic_empty = 1, drum_hits = 0;
    host_tracks_init();
    fm6_init();
    dsu_defaults();
    CHECK(jam_new_dusty(), "new Dusty accepted while stopped");
    CHECK(!song.playing && !song.rec && song.sel == 1 && song.g[G_BPM] == 88, "stopped 88 BPM, chord keyboard selected");
    CHECK(song.g[G_SWING] == 0, "swing initially straight");
    CHECK(jam_role_track(0) == 3 && jam_role_track(1) == 0 && jam_role_track(2) == 1 && jam_role_track(3) == 2,
          "display roles map to stable internal tracks");
    CHECK(str_eq(DRUM_KIT_NAMES[TDRUM->p[P_E0]], "BOOMBAP"), "drums use named synthesized BOOMBAP kit");
    for (k = 0; k < NTRK; k++) {
        CHECK(trk[k].p[P_SLEN] == 32 && trk[k].p[P_SDIV] == 2, "two bars of sixteenths on every role");
        CHECK(trk[k].p[P_AMODE] == 0 && trk[k].p[P_ITYPE] == 0, "no arp or insert surprise");
        for (i = 0; i < NSTEP; i++) {
            if (k < NPART) melodic_empty &= trk[k].step[i].time == ST_REST && trk[k].step[i].n == 0;
            else for (uint32_t l = 0; l < DRUM_LANES; l++) drum_hits += dstep_has(&trk[k].dstep[i], l);
            CHECK(trk[k].micro[i] == 0 && step_fill(&trk[k], i) == 0, "new pattern has no hidden timing or fill flags");
        }
    }
    CHECK(melodic_empty, "melodic patterns left empty for the player");
    CHECK(drum_hits == 29, "ready beat has exactly 29 explicit drum hits");
    CHECK(trk[0].p[P_VOICE] == V_LEGATO && trk[1].p[P_VOICE] == V_POLY && trk[2].p[P_VOICE] == V_LEGATO,
          "bass/lead mono, chords poly");
    CHECK(trk[1].p[P_CHORD] == 2 && trk[1].p[P_QUANT] == 1 && trk[2].p[P_QUANT] == 1,
          "one-key sevenths with every chord and lead key available through SNAP");
    block_run();
    for (k = 0; k < NPART; k++) CHECK(trk[k].engine == 0, "melodic role renders ANALOG");
    song.playing = 1;
    CHECK(!jam_new_dusty() && song.playing && drum_hits == 29, "new palette refuses a playing session");
    song.playing = 0;
}

static void test_preservation(void)
{
    uint32_t k;
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        pattern_snapshot_t before, after;
        t->p[P_LEVEL] = 73; t->p[P_PAN] = -11; t->p[P_MUTE] = 1;
        t->p[P_ROOT] = 7; t->p[P_SCALE] = 3; t->p[P_QUANT] = 1;
        t->p[P_CHORD] = 3; t->p[P_STRUM] = 12; t->p[P_VLEAD] = 1;
        t->p[P_SLEN] = 48; t->p[P_SDIV] = 1; t->p[P_SSWING] = 21; t->p[P_SGATE] = 91;
        if (k == TRK_DRUM) dstep_set(&t->dstep[41], 7, LV_GHOST, 2);
        else { t->step[41].time = ST_NOTE; t->step[41].n = 1; t->step[41].note[0] = 63; }
        t->micro[41] = -13;
        t->fill[10] = 0x55;
        t->lock[0].step = 41; t->lock[0].param = P_DIST; t->lock[0].val = 12;
        pattern_copy(&before, t);
        CHECK(jam_sound_load(k, 1), "curated variant loads");
        pattern_copy(&after, t);
        CHECK(!memcmp(&before, &after, sizeof before), "sound load preserves all notes, nudges, fills and lock records");
        CHECK(t->p[P_LEVEL] == 73 && t->p[P_PAN] == -11 && t->p[P_MUTE] == 1, "sound load preserves mix");
        CHECK(t->p[P_ROOT] == 7 && t->p[P_SCALE] == 3 && t->p[P_QUANT] == 1 && t->p[P_CHORD] == 3 &&
              t->p[P_STRUM] == 12 && t->p[P_VLEAD] == 1, "sound load preserves harmonic and playing choices");
        CHECK(t->p[P_SLEN] == 48 && t->p[P_SDIV] == 1 && t->p[P_SSWING] == 21 && t->p[P_SGATE] == 91,
              "sound load preserves sequencer settings");
        CHECK(jam_sound_index(k) == 1 && jam_sound_count(k) == (k == TRK_DRUM ? 8u : 12u),
              "expanded bank metadata follows selected sound");
    }
    CHECK(!jam_sound_load(4, 0) && !jam_sound_load(0, jam_sound_count(0)), "invalid sound requests rejected");
    jam_sound_invalidate(0);
    CHECK(str_eq(jam_sound_name(0), "ANALOG") && jam_sound_index(0) == 0,
          "generic analog load invalidates curated name and browser index");
    CHECK(jam_sound_load(0, 1) && str_eq(jam_sound_name(0), "MUTED BASS"), "curated load restores identity");
    trk[0].eng_req = 1;
    CHECK(str_eq(jam_sound_name(0), ENGINES[1]->name) && jam_sound_index(0) == 0,
          "foreign engine displays actual engine with safe browser index");
    TDRUM->p[P_E0] = 1; /* DEEP is outside the curated selection; ACOUSTIC is now ROCK KIT. */
    CHECK(str_eq(jam_sound_name(TRK_DRUM), DRUM_KIT_NAMES[1]), "deep KIT change displays actual kit");
    jam_sound_invalidate(99); /* invalid requests do not shift out of range */
}

static void test_reset(void)
{
    static const uint8_t device_ids[] = {G_MIDI, G_SYNC, G_ROUTE, G_DRCH, G_CH1, G_CH2, G_CH3, G_CHOTH, G_BEND};
    int16_t device_values[NELEM(device_ids)];
    CHECK(jam_new_dusty(), "reset setup");
    block_run();
    fm1_in.notes = 1u << 7; /* hold physical C4 across New */
    block_run();
    CHECK(kb_kind[7] == KS_NOTE && kb_n[7] == 4, "old session really owns a held four-note key");
    for (uint32_t i = 0; i < NELEM(device_ids); i++)
        device_values[i] = song.g[device_ids[i]] = GP[device_ids[i]].max;
    song.master_q12 = 3072;
    song.g[G_TUNE] = 49; song.g[G_ROLL] = 4; song.g[G_TXSRC] = 3;
    song.g[G_TXRATE] = GP[G_TXRATE].max;
    drum_mute = 0xFFFF; er_mute = 1; er_lanes = 7;
    sus_on[1] = 1; sus_wait[1][1] = 1u << 28;
    midi_sel_on[1][60] = 2; ch_bend[1] = 2; ch_sus[1] = 2;
    trk[1].bend = 111; vl_n[1] = 4;
    punch.req = PX_FREEZE; punch.hold = 1; punch.keybit = 1;
    roll[0].on = 1; roll[0].trk = 1; roll[0].note = 60;
    aud_lanes = 1; dsu_aud_vel = 90;
    pc_req[1] = 5;
    step_in_push(60u | 1u << 7);
    midi_in_q[mi_w++ % MQ] = 0x09u | 0x90u << 8 | 72u << 16 | 100u << 24;
    CHECK(jam_new_dusty(), "New accepts stopped session with old live input");
    CHECK(!drum_mute && !er_mute && !er_lanes, "new ready beat clears drum lane mutes and erase state");
    CHECK(song.g[G_TUNE] == 0 && song.g[G_ROLL] == GP[G_ROLL].def &&
          song.g[G_TXSRC] == 0 && song.g[G_TXRATE] == GP[G_TXRATE].def, "new resets musical globals");
    for (uint32_t i = 0; i < NELEM(device_ids); i++)
        CHECK(song.g[device_ids[i]] == device_values[i], "new preserves device MIDI setting");
    CHECK(song.master_q12 == 3072, "new preserves master volume");
    CHECK(kb_kind[7] == KS_NONE && kb_prev == fm1_in.notes && !trk[1].nheld,
          "new drops old key ownership and waits for held key release");
    CHECK(mi_r == mi_w && step_in_r == step_in_w && !pc_req[1] && !aud_lanes && !dsu_aud_vel,
          "new discards pending note, preset and audition requests");
    CHECK(!midi_sel_on[1][60] && !sus_on[1] && !sus_wait[1][1] && !ch_bend[1] && !ch_sus[1] && !trk[1].bend,
          "new clears MIDI note, pedal and pitch-wheel ownership");
    CHECK(!roll[0].on && punch.req == -1 && !punch.hold && !punch.keybit && !vl_n[1],
          "new ends roll, punch and prior voice-leading context");
    block_run();
    CHECK(kb_kind[7] == KS_NONE && !trk[1].nheld, "old held key is not re-triggered by next audio block");
    fm1_in.notes = 0;
    block_run();
    for (uint32_t i = 0; i < NELEM(device_ids); i++) song.g[device_ids[i]] = GP[device_ids[i]].def;
    song.master_q12 = 4096;
}

static void test_macros(void)
{
    uint32_t k, m;
    CHECK(jam_new_dusty(), "reset stopped session for macro tests");
    block_run();
    for (k = 0; k < NTRK; k++) for (m = 0; m < 4; m++) {
        int16_t other[NTRK][P_COUNT];
        for (uint32_t p = 0; p < NTRK; p++) memcpy(other[p], trk[p].p, sizeof other[p]);
        jam_macro_set(k, m, -100);
        CHECK(jam_macro_get(k, m) == 0, "macro clamps below zero");
        jam_macro_set(k, m, 200);
        CHECK(jam_macro_get(k, m) == 100, "macro clamps above maximum");
        for (uint32_t p = 0; p < NTRK; p++) if (p != k)
            CHECK(!memcmp(other[p], trk[p].p, sizeof other[p]), "macro does not edit another role's patch");
        CHECK(jam_macro_name(k, m)[0] != 0, "macro is labeled");
    }
    CHECK(song.g[G_DUST] == 0, "drum controls do not secretly change master dust");
    trk[1].lk_n = 1; trk[1].lk_param[0] = P_E4; trk[1].lk_base[0] = 1;
    jam_macro_set(1, 0, 50);
    CHECK(trk[1].lk_base[0] == trk[1].p[P_E4], "live edit updates active lock's restored base");
    trk[1].lk_n = 0;
}

static void demo_notes(void)
{
    static const uint8_t chord[2][4] = {{48, 51, 55, 58}, {44, 48, 51, 55}};
    for (uint32_t bar = 0; bar < 2; bar++) {
        uint32_t at = bar * 16;
        step_t *s = &trk[1].step[at];
        s->time = ST_NOTE; s->n = 4; s->vel = 82;
        memcpy(s->note, chord[bar], 4);
        for (uint32_t j = 1; j < 12; j++) trk[1].step[at + j].time = ST_TIE;
        s = &trk[0].step[at]; s->time = ST_NOTE; s->n = 1; s->note[0] = bar ? 32 : 36; s->vel = 92;
        for (uint32_t j = 1; j < 4; j++) trk[0].step[at + j].time = ST_TIE;
        s = &trk[0].step[at + 10]; s->time = ST_NOTE; s->n = 1; s->note[0] = bar ? 39 : 43; s->vel = 76;
    }
}
static void test_audio(const char *path)
{
    int32_t b[CTL * 2], peak = 0, tail_peak = 0;
    uint64_t energy = 0;
    uint32_t voices_max = 0, notes_free = 1, drum_free = 1, last_lead = 0;
    uint32_t musical_blocks = (FS * 240u / 88u * 4u) / CTL;
    uint32_t tail_blocks = FS * 16u / CTL;
    uint32_t wav_tail = FS * 3u / CTL;
    FILE *wav = path ? fopen(path, "wb") : 0;
    CHECK(!path || wav, "open audition WAV");
    CHECK(jam_new_dusty(), "fresh palette for real DSP render");
    block_run();
    demo_notes();
    if (wav) wav_hdr(wav, (musical_blocks + wav_tail) * CTL);
    seq_start();
    for (uint32_t n = 0; n < musical_blocks; n++) {
        if (n % 689u == 0) {
            static const uint8_t melody[8] = {72, 75, 79, 77, 75, 72, 70, 67};
            if (last_lead) input_off(&trk[2], last_lead);
            last_lead = melody[n / 689u % 8u];
            input_on(&trk[2], last_lead, 80);
        }
        mix_block(b, CTL);
        uint32_t v = voices_busy(); if (v > voices_max) voices_max = v;
        for (uint32_t i = 0; i < CTL; i++) {
            for (uint32_t c = 0; c < 2; c++) {
                int32_t a = b[2 * i + c] < 0 ? -b[2 * i + c] : b[2 * i + c];
                if (a > peak) peak = a;
                energy += (uint32_t)a;
            }
            if (wav) wav_put(wav, b[2 * i], b[2 * i + 1]);
        }
    }
    if (last_lead) input_off(&trk[2], last_lead);
    seq_stop();
    for (uint32_t k = 0; k < NTRK; k++) trk_all_off(&trk[k]);
    for (uint32_t n = 0; n < tail_blocks; n++) {
        mix_block(b, CTL);
        for (uint32_t i = 0; i < CTL; i++) {
            if (wav && n < wav_tail) wav_put(wav, b[2 * i], b[2 * i + 1]);
            if (n > tail_blocks - FS / CTL) for (uint32_t c = 0; c < 2; c++) {
                int32_t a = b[2 * i + c] < 0 ? -b[2 * i + c] : b[2 * i + c];
                if (a > tail_peak) tail_peak = a;
            }
        }
    }
    if (wav) fclose(wav);
    for (uint32_t k = 0; k < NPART; k++) for (uint32_t i = 0; i < NVOICE; i++) notes_free &= !trk[k].v[i].active;
    for (uint32_t i = 0; i < NDRUM; i++) drum_free &= !drums.v[i].active;
    CHECK(energy > 100000u && peak > 1000, "complete jam produces audible nonzero samples");
    CHECK(peak < 30000, "complete jam stays below near-full-scale clipping");
    CHECK(voices_max <= NVOICE, "shared synth voice budget remains bounded");
    CHECK(notes_free && drum_free, "all synth and drum voices release after stop");
    CHECK(tail_peak <= 8, "effects settle to near silence after release");
    printf("audio: peak=%d, max synth voices=%u, final tail peak=%d; native render only\n", peak, voices_max, tail_peak);
}

static void test_macro_audio(void)
{
    int32_t b[CTL * 2], peak = 0;
    uint32_t clipped = 0;
    for (uint32_t variant = 0; variant < 2; variant++) {
        CHECK(jam_new_dusty(), "new palette for macro-extreme render");
        for (uint32_t t = 0; t < NTRK; t++) {
            jam_sound_load(t, variant);
            for (uint32_t m = 0; m < 4; m++) jam_macro_set(t, m, 100);
        }
        block_run();
        demo_notes();
        seq_start();
        input_on(&trk[2], 79, 100);
        for (uint32_t n = 0; n < FS * 3u / CTL; n++) {
            mix_block(b, CTL);
            for (uint32_t i = 0; i < CTL * 2; i++) {
                int64_t a = b[i] < 0 ? -(int64_t)b[i] : b[i];
                if (a > peak) peak = (int32_t)a;
                clipped += a >= 32767;
            }
        }
        input_off(&trk[2], 79);
        seq_stop();
    }
    CHECK(peak > 1000 && peak < 32767 && !clipped, "both variants render bounded unclipped audio at macro maxima");
    printf("macro audio: peak=%d, full-scale samples=%u; native render only\n", peak, clipped);
}

int main(int argc, char **argv)
{
    test_session();
    test_preservation();
    test_reset();
    test_macros();
    test_audio(argc > 1 ? argv[1] : 0);
    test_macro_audio();
    printf("jam_palette: %u checks, %u failures\n", jam_checks, jam_failures);
    return jam_failures ? 1 : 0;
}
