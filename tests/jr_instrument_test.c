/* SPDX-License-Identifier: GPL-3.0-only */
/* Experimental recovery against REAL instrument capture/apply and DSP. No
 * runtime integration or hardware access. --emit/--verify are separate-process
 * file fixtures used by the recovery runner to prove reset independence. */
#define _POSIX_C_SOURCE 200809L
#define JAM_STORAGE_LIBRARY 1
#include "jam_storage_harness.c"
#include "../firmware/experimental/recovery/jr_codec.h"
#include "jr_instrument_fixture.h"

typedef struct {
    uint8_t snapshot[JR_SNAPSHOTS][JR_SNAPSHOT_BYTES];
    uint8_t patch[JR_SNAPSHOTS][JR_PARTS][JR_FM6_BYTES];
    uint8_t syn[JR_SYN_BYTES], meta[JR_METADATA_BYTES], arr[JR_ARRANGEMENT_BYTES];
} jif_bundle_t;
static jif_bundle_t expected, restored;
static uint8_t payload[JR_PAYLOAD_BYTES];

static jr_payload_view_t jif_view(const jif_bundle_t *b)
{
    jr_payload_view_t v = {0};
    for (uint32_t s = 0; s < JR_SNAPSHOTS; ++s) {
        v.snapshot[s] = b->snapshot[s];
        for (uint32_t t = 0; t < JR_PARTS; ++t) v.fm6[s][t] = b->patch[s][t];
    }
    v.syn_bank = b->syn; v.metadata = b->meta; v.arrangement = b->arr;
    return v;
}
static int jif_validate(const jif_bundle_t *b)
{
    jr_payload_view_t v = jif_view(b);
    jr_source_t src = {&v, jr_codec_read_at, JR_PAYLOAD_BYTES};
    return jr_codec_validate(NULL, &src);
}

static void jif_seed_snapshot(uint32_t s, uint32_t seed)
{
    jam_new_dusty();
    song.g[G_BPM] = (int16_t)(79 + (seed % 30) + s * 9);
    song.g[G_SWING] = (int16_t)((seed + 17 * s) % 101);
    song.g[G_TUNE] = (int16_t)(-12 + (int32_t)s * 3);
    song.g[G_DFDBK] = (int16_t)(30 + s);
    song.g[G_TXSRC] = (int16_t)(s % 7);
    song.g[G_TXLVL] = (int16_t)(s * 11);
    song.g[G_TXTONE] = (int16_t)(-37 + (int32_t)s * 11);
    song.g[G_TXMOVE] = (int16_t)(s * 7);
    song.g[G_TXRATE] = (int16_t)(s % 5);
    song.g[G_TXWHEN] = (int16_t)(s & 1);
    song.g[G_TXDUCK] = (int16_t)((s + 1) & 1);
    song.sel = (uint8_t)((s + seed) % NTRK);
    for (uint32_t t = 0; t < NTRK; ++t) {
        track_t *q = &trk[t];
        steps_clear(q);
        if (t < NPART) {
            uint32_t e = (s * 3 + t + seed - 1) % NENGINES;
            host_preset(q, e, (seed + s + t) % ENGINES[e]->npresets);
            if (ENGINES[e] == &ENG_SAMPLE || e == ENGI_GRAIN) q->p[P_E0] %= SMP_NSETS;
            /* Different actual FM6 voice data, not merely a slot number. */
            uint8_t patch[156];
            fm6_unpack(FM6_INIT, patch);
            patch[FP_ALG] = (uint8_t)((seed + s * 5 + t) % 32);
            patch[FP_FB] = (uint8_t)((seed + s + t) % 8);
            patch[FP_OL] = (uint8_t)(65 + s + t);
            patch[FP_FC] = (uint8_t)(1 + s + t);
            memcpy(patch + FP_NAME, "EDITEDJAM ", 10);
            patch[FP_NAME + 8] = (uint8_t)('A' + s);
            fm6_set_patch(t, patch);
            fm6_slot[t] = (uint8_t)q->p[P_E7];
            fm6_pend &= (uint8_t)~(1u << t);
        } else {
            q->engine = q->eng_req = 0; q->preset = 0;
            for (uint32_t i = 0; i < 8; ++i) q->p[P_E0 + i] = ENGINES[0]->edit[i].def;
            q->p[P_E0] = (int16_t)(DRUM_SYN + s % DSU_N);
        }
        q->p[P_SLEN] = (int16_t)(16 + s * 8);
        q->p[P_SSWING] = (int16_t)((seed + s + t) % 101);
        q->p[P_ROOT] = (int16_t)((3 + s + t) % 12);
        q->p[P_SCALE] = (int16_t)(1 + s % 2);
        q->p[P_QUANT] = 1;
        q->p[P_PAN] = (int16_t)(-24 + (int32_t)t * 13);
        q->p[P_LEVEL] = (int16_t)(74 + s + t);
        q->p[P_MUTE] = (int16_t)((t == 2 && s == 3) ? 1 : 0);
        q->p[P_CHORD] = (int16_t)(t == 1 ? 2 + s % 2 : 0);
        q->p[P_VOICE] = t == 0 ? V_MONO : V_POLY;
        for (uint32_t i = 0; i < NSTEP; ++i) {
            if (t == TRK_DRUM) {
                dstep_set(&q->dstep[i], (i + s) % DRUM_LANES, 1 + i % 3, i % 4);
                dstep_set(&q->dstep[i], (i + s + 5) % DRUM_LANES, 1 + (i + 1) % 3, (i + 1) % 4);
            } else {
                step_t *st = &q->step[i];
                st->n = (uint8_t)(1 + (i + t) % 4);
                st->time = (uint8_t)(i % 11 == 7 ? ST_TIE : i % 13 == 9 ? ST_REST : ST_NOTE);
                st->flags = (uint8_t)(i % 4);
                st->vel = (uint8_t)(45 + (i + seed) % 80);
                st->lvl = (uint8_t)(i * 13 + s);
                st->rat = (uint8_t)(i * 7 + t);
                for (uint32_t n = 0; n < 4; ++n) {
                    st->note[n] = (uint8_t)(42 + t * 9 + s + n * 3 + i % 7);
                    q->note_len[i][n] = (uint8_t)(st->time == ST_NOTE && n < st->n ? 1 + (i + n * 7 + s) % 64 : 0);
                }
            }
            q->micro[i] = (int8_t)((int32_t)(i + s) % 64 - 32);
            step_fill_set(q, i, i % 3);
        }
        for (uint32_t i = 0, id = P_LEVEL; i < NLOCK; ++id) {
            if (id >= P_E0) id = P_LEVEL;
            if (!p_lockable(id)) continue;
            const param_desc_t *d = &TP[id];
            int32_t value = d->min + (int32_t)((seed + s + t + i) % (uint32_t)(d->max - d->min + 1));
            q->lock[i] = (plock_t){(uint8_t)((i * 3 + s) % NSTEP), (uint8_t)id, (int16_t)value};
            ++i;
        }
        /* Source-changing locks must be covered, even beyond the visible loop. */
        if (t == TRK_DRUM)
            q->lock[NLOCK-1] = (plock_t){63, P_E0, (int16_t)(DRUM_SYN + (s + 1) % DSU_N)};
        else if (ENGINES[q->eng_req] == &ENG_SAMPLE || q->eng_req == ENGI_GRAIN)
            q->lock[NLOCK-1] = (plock_t){63, P_E0, (int16_t)((s + 1) % SMP_NSETS)};
        else if (q->eng_req == ENGI_FM6)
            q->lock[NLOCK-1] = (plock_t){63, P_E7, (int16_t)(s % FM6_NFACTORY)};
        if (t < NPART) jam_lengths_changed(q);
    }
}

static void jif_make(jif_bundle_t *b, uint32_t seed)
{
    project_t p;
    memset(b, 0, sizeof *b); memset(nor, 255, sizeof nor); ordinary_boot();
    jr_codec_metadata_init(b->meta, 15);
    dsu_defaults();
    for (uint32_t k = 0; k < DSU_N; ++k) {
        snprintf(dsu.k[k].name, 8, "SYN%u-%u", k, seed % 100);
        for (uint32_t n = 0; n < DS_LANES; ++n) {
            dsu.k[k].s[n].pitch = (uint8_t)(35 + n + k + seed % 12);
            dsu.k[k].s[n].fine = (uint8_t)((n + seed) % 16);
            dsu.k[k].s[n].decay = (uint8_t)(30 + n + k);
            dsu.k[k].s[n].drive = (uint8_t)((n * 3 + seed + k) % 128);
            dsu.k[k].s[n].fenv = (int8_t)(-30 + (int32_t)n + (int32_t)k);
        }
        dsu_fix_kit(&dsu.k[k]);
    }
    jif_syn_encode(b->syn, &dsu);
    for (uint32_t s = 0; s < JR_SNAPSHOTS; ++s) {
        jif_seed_snapshot(s, seed); proj_capture(&p); jif_project_encode(b->snapshot[s], &p);
        if (s) *project_slot(s - 1) = p;
        for (uint32_t t = 0; t < NTRK; ++t) {
            uint8_t engine = t < NPART ? p.t[t].engine : 0;
            jr_codec_identity_set(b->meta, s, t, 0, 3, engine, 1);
            if (t < NPART && p.t[t].engine == ENGI_FM6)
                memcpy(b->patch[s][t], fm6_patch[t], JR_FM6_BYTES);
        }
    }
    uint8_t *session = b->meta + JR_META_SESSION;
    session[9] = (uint8_t)(seed % NTRK);
    session[10] = (uint8_t)(int8_t)-1; session[11] = 1;
    session[12] = 1; session[13] = 2;
    uint8_t *prefs = b->meta + JR_META_PREFS;
    prefs[8] = (uint8_t)(seed % 5); prefs[9] = 1; prefs[10] = 1;
    prefs[11] = 2; prefs[12] = 3; prefs[13] = 1; prefs[14] = 1;
    prefs[16] = 1; prefs[17] = 1; prefs[18] = 1; prefs[19] = 2; prefs[20] = 17;
    for (uint32_t i = 0; i < ML_N; ++i) {
        uint16_t entry = (uint16_t)(20 + i + (i >= 12)) | (uint16_t)((i % NTRK) << 7) | (uint16_t)((P_LEVEL + 1) << 9);
        uint8_t *at = prefs + 32 + 2 * i; jif_w16(&at, entry);
    }
    prefs[64] = 4; prefs[65] = 5; prefs[66] = 6; prefs[67] = 11; prefs[68] = 1; prefs[69] = 7;
    b->arr[0] = 16; b->arr[1] = 1;
    for (uint32_t i = 0; i < ARR_STEPS; ++i) {
        b->arr[4 + i*2] = (uint8_t)((i + seed) % 4);
        b->arr[5 + i*2] = (uint8_t)(1 + (i * 3 + seed) % 64);
    }
}

static int jif_unpack(jif_bundle_t *b, const jr_source_t *src)
{
    if (jr_codec_validate(NULL, src) != JR_VALID_OK) return 0;
    for (uint32_t s = 0; s < JR_SNAPSHOTS; ++s) {
        if (src->read(src->ctx, s * JR_SNAPSHOT_BYTES, b->snapshot[s], JR_SNAPSHOT_BYTES)) return 0;
        for (uint32_t t = 0; t < JR_PARTS; ++t)
            if (src->read(src->ctx, JR_FM6_OFFSET + (s * JR_PARTS + t) * JR_FM6_BYTES,
                          b->patch[s][t], JR_FM6_BYTES)) return 0;
    }
    return !src->read(src->ctx, JR_SYN_OFFSET, b->syn, JR_SYN_BYTES) &&
           !src->read(src->ctx, JR_METADATA_OFFSET, b->meta, JR_METADATA_BYTES) &&
           !src->read(src->ctx, JR_ARRANGEMENT_OFFSET, b->arr, JR_ARRANGEMENT_BYTES);
}

static void jif_apply(const jif_bundle_t *b, uint32_t snapshot)
{
    project_t p;
    /* Test-only cold transient reset. Real boot integration remains future
     * work; no running clock, REC, solo, keys, sustain, or DSP state is durable. */
    memset(trk, 0, sizeof trk); memset(&song, 0, sizeof song);
    memset(eng_arena, 0, sizeof eng_arena); memset(eng_arena_own, 0, sizeof eng_arena_own);
    memset(drw_t, 0, sizeof drw_t); memset(drw_v, 0, sizeof drw_v);
    formant_nz = 0x2545F491;
    memset(fm6_eff, 0, sizeof fm6_eff); memset(fm6_lfo, 0, sizeof fm6_lfo);
    memset(fm6_lfo_v, 0, sizeof fm6_lfo_v); memset(fm6_lfo_d, 0, sizeof fm6_lfo_d);
    memset(&drums, 0, sizeof drums); drums.set = -2;
    memset(sus_on, 0, sizeof sus_on); memset(sus_wait, 0, sizeof sus_wait);
    fm1_in.notes = fm1_in.buttons = transport_req = panic_req = 0;
    ft_on = ci_on = rec_wait = 0; song_kept = 0; rng_state = 0x1234567u; vage = 0;
    for (uint32_t i = 0; i < G_COUNT; ++i) song.g[i] = GP[i].def;
    fm6_init(); jif_project_decode(&p, b->snapshot[snapshot]); proj_apply(&p, 1);
    for (uint32_t t = 0; t < NPART; ++t) trk[t].engine = trk[t].eng_req;
    jif_apply_patch_sidecar(b->patch[snapshot]);
    jif_syn_decode(&dsu, b->syn);
    for (uint32_t s = 0; s < 4; ++s) jif_project_decode(project_slot(s), b->snapshot[s + 1]);
    arrangement.count = b->arr[0]; arrangement.loop = b->arr[1];
    arrangement.reserved[0] = arrangement.reserved[1] = 0;
    for (uint32_t i = 0; i < ARR_STEPS; ++i) {
        arrangement.entry[i].scene = b->arr[4 + i*2];
        arrangement.entry[i].bars = b->arr[5 + i*2];
    }
    memset(&arrangement_clock, 0, sizeof arrangement_clock);
    const uint8_t *session = b->meta + JR_META_SESSION, *prefs = b->meta + JR_META_PREFS;
    song.sel = snapshot ? p.sel : session[9]; song.octave = (int8_t)session[10];
    arrangement_enabled = session[12]; live_sec = (int8_t)session[13];
    settings.magic = SETTINGS_MAGIC; settings.palette = prefs[8]; settings.lowcut = prefs[9]; settings.zoom = prefs[10];
    settings_init();
    lights_lvl = prefs[11]; lights_keys = prefs[12]; lights_notes = prefs[13];
    usb_full = prefs[14]; usb_serial = prefs[15]; rec_tempo = prefs[16]; rec_count = prefs[17];
    lights_mout = prefs[18]; lights_sync = prefs[19]; lights_min = prefs[20];
    for (uint32_t i = 0; i < ML_N; ++i) {
        const uint8_t *at = prefs + 32 + 2*i; ml_tab[i] = (uint16_t)jif_r16(&at);
    }
    for (uint32_t i = 0; i < 6; ++i) midi_set[i] = prefs[64 + i];
    song.g[G_SYNC] = lights_sync; song.g[G_MIDI] = lights_mout; song.g[G_ROUTE] = lights_min;
    song.g[G_CH1] = midi_set[0]; song.g[G_CH2] = midi_set[1]; song.g[G_CH3] = midi_set[2];
    song.g[G_DRCH] = midi_set[3]; song.g[G_CHOTH] = midi_set[4]; song.g[G_BEND] = midi_set[5];
    jam_sound_valid = 0; /* Fixture uses explicit CUSTOM identities, not fake factory labels. */
    layers_init(); ui.jam_home_view = session[11]; go_home();
    song.master_q12 = 2048; /* Physical master gain is re-read, never serialized. */
}

static void jif_capture_preferences(uint8_t out[128])
{
    uint8_t *p = out; memset(out, 0, 128);
    memcpy(p, "JDP1", 4); p += 4; jif_w16(&p, 1); jif_w16(&p, 128);
    jif_w8(&p, settings.palette); jif_w8(&p, settings.lowcut); jif_w8(&p, settings.zoom);
    jif_w8(&p, lights_lvl); jif_w8(&p, lights_keys); jif_w8(&p, lights_notes);
    jif_w8(&p, usb_full); jif_w8(&p, usb_serial); jif_w8(&p, rec_tempo); jif_w8(&p, rec_count);
    jif_w8(&p, lights_mout); jif_w8(&p, lights_sync); jif_w8(&p, lights_min);
    p = out + 32;
    for (uint32_t i = 0; i < ML_N; ++i) jif_w16(&p, ml_tab[i]);
    for (uint32_t i = 0; i < 6; ++i) jif_w8(&p, midi_set[i]);
}

static uint32_t jif_render(const jif_bundle_t *b, uint32_t snapshot, int32_t *out)
{
    jif_apply(b, snapshot);
    uint32_t pos = 0;
    for (uint32_t t = 0; t < NPART; ++t) {
        trk_note_on(&trk[t], (uint32_t)(49 + 7*t), 97);
        trk_note_on(&trk[t], (uint32_t)(53 + 7*t), 83);
        for (uint32_t block = 0; block < 96; ++block) {
            const engine_t *e = ENGINES[trk[t].engine];
            if (e->block) e->block(&trk[t]);
            track_render(&trk[t], out + pos, CTL); pos += CTL;
        }
        trk_all_off(&trk[t]);
    }
    for (uint32_t kit = 0; kit < DSU_N; ++kit) {
        TDRUM->p[P_E0] = (int16_t)(DRUM_SYN + kit);
        drum_on(36, 113); drum_on(38, 94); drum_on(42, 87);
        for (uint32_t block = 0; block < 96; ++block) {
            memset(out + pos, 0, CTL * sizeof *out);
            drums_render_mono(out + pos, CTL); pos += CTL;
        }
    }
    return pos;
}
#define JIF_RENDER_SAMPLES (7u * 96u * CTL)
static int32_t pcm_before[JIF_RENDER_SAMPLES], pcm_after[JIF_RENDER_SAMPLES];

static void jif_check_restored(const jif_bundle_t *b)
{
    uint8_t wire[JR_SNAPSHOT_BYTES]; project_t q;
    for (uint32_t s = 0; s < JR_SNAPSHOTS; ++s) {
        jif_apply(b, s); proj_capture(&q); jif_project_encode(wire, &q);
        project_t wanted; jif_project_decode(&wanted, b->snapshot[s]);
        check(!memcmp(q.t, wanted.t, sizeof q.t), "real apply/capture preserves all four tracks, hidden steps, dynamics, locks and microtiming");
        check(!memcmp(q.note_len, wanted.note_len, sizeof q.note_len), "real apply/capture preserves independent note lengths in each of five snapshots");
        check(q.g[G_BPM] == wanted.g[G_BPM] && q.g[G_SWING] == wanted.g[G_SWING] && q.rsv[0] == wanted.rsv[0] &&
              q.rsv[1] == wanted.rsv[1] && q.g[G_DRCH] == wanted.g[G_DRCH] && q.g[G_INFO] == wanted.g[G_INFO],
              "real apply/capture restores tempo, swing and packed texture without treating routing as song data");
        int globals_equal = 1;
        for (uint32_t i = 0; i < PROJ_NG; ++i)
            if (i != G_SLOT && i != G_LOAD && i != G_SAVE && i != G_SYNC && i != G_MIDI && i != G_ROUTE && i != G_DRCH && i != G_INFO)
                globals_equal &= q.g[i] == wanted.g[i];
        check(globals_equal && q.drdly == wanted.drdly, "every musical global, shared effect setting and drum send survives real restore");
        for (uint32_t t = 0; t < NPART; ++t)
            if (trk[t].eng_req == ENGI_FM6)
                check(!memcmp(fm6_patch[t], b->patch[s][t], JR_FM6_BYTES) && !(fm6_pend & (1u << t)) &&
                      fm6_slot[t] == trk[t].p[P_E7], "actual edited FM6 sidecar survives restore and subsequent fm6_poll");
        uint8_t syn[JR_SYN_BYTES]; jif_syn_encode(syn, &dsu);
        check(!memcmp(syn, b->syn, sizeof syn), "all four shared SYN kits and every edited lane restore exactly");
        check(arrangement_ready() == 15 && arr_valid(&arrangement, 15) && arrangement.count == 16 &&
              arrangement.entry[15].bars == b->arr[35], "real arranger sees all A-D snapshots and sixteen restored arrangement entries");
        check(!song.playing && !song.rec && !song.solo && !fm1_in.notes && !ft_on && !ci_on && !rec_wait &&
              !arrangement_clock.running && !song_kept, "restore starts stopped and disarmed without transient notes, solo or arrangement clock");
        check(ui.home && ui.jam_home_view == b->meta[JR_META_SESSION + 11] && song.octave == -1 &&
              settings.palette == b->meta[JR_META_PREFS + 8] && settings.lowcut && settings.zoom &&
              lights_notes && lights_min == 17 && midi_set[5] == 7 && ml_tab[15],
              "own validated preferences and playing position restore through real UI/settings state");
        uint8_t prefs[128]; jif_capture_preferences(prefs);
        check(!memcmp(prefs, b->meta + JR_META_PREFS, sizeof prefs) && fx_lowcut == settings.lowcut,
              "every typed preference and all sixteen MIDI Learn bindings recapture exactly after real settings apply");
        jif_render(&expected, s, pcm_before);
        jif_render(b, s, pcm_after);
        if (memcmp(pcm_before, pcm_after, sizeof pcm_before)) {
            for (uint32_t i = 0; i < JIF_RENDER_SAMPLES; ++i)
                if (pcm_before[i] != pcm_after[i]) {
                    fprintf(stderr, "snapshot %u DSP difference sample %u part %u: %d / %d\n", s, i, i/(96u*CTL), pcm_before[i], pcm_after[i]);
                    break;
                }
        }
        check(!memcmp(pcm_before, pcm_after, sizeof pcm_before), "fresh DSP render is sample-identical for recovered engines, edited FM6 and every SYN kit");
        check(!all_zero(pcm_after, sizeof pcm_after), "render comparison contains audible signal rather than two silent buffers");
    }
}

static int jif_memory_read(void *ctx, uint32_t off, void *dst, uint32_t bytes)
{
    if (off > JR_PAYLOAD_BYTES || bytes > JR_PAYLOAD_BYTES - off) return -1;
    memcpy(dst, (const uint8_t *)ctx + off, bytes); return 0;
}
static int jif_write_file(const char *path, const jif_bundle_t *b)
{
    jr_payload_view_t v = jif_view(b);
    if (jr_codec_read_at(&v, 0, payload, sizeof payload)) return 0;
    FILE *f = fopen(path, "wb"); if (!f) return 0;
    int ok = fwrite(payload, 1, sizeof payload, f) == sizeof payload;
    return !fclose(f) && ok;
}
static int jif_read_file(const char *path)
{
    FILE *f = fopen(path, "rb"); if (!f) return 0;
    int ok = fread(payload, 1, sizeof payload, f) == sizeof payload && fgetc(f) == EOF;
    return !fclose(f) && ok;
}

static void jif_configure_engine(jif_bundle_t *b, uint32_t snapshot, uint32_t track, uint32_t engine)
{
    project_t p; jif_project_decode(&p, b->snapshot[snapshot]);
    proj_trk_t *t = &p.t[track]; t->engine = (uint8_t)engine; t->preset = 0;
    for (uint32_t i = 0; i < 8; ++i) pt_set(t, P_E0 + i, ENGINES[engine]->edit[i].def);
    if (track == TRK_DRUM) pt_set(t, P_E0, DRUM_DEFAULT_KIT);
    for (uint32_t i = 0; i < NLOCK; ++i) t->lock[i] = (plock_t){LOCK_FREE, 0, 0};
    if (track < NPART) {
        memset(b->patch[snapshot][track], 0, JR_FM6_BYTES);
        if (engine == ENGI_FM6) fm6_unpack(FM6_INIT, b->patch[snapshot][track]);
    }
    jr_codec_identity_set(b->meta, snapshot, track, 0, 3, (uint8_t)engine, 1);
    jif_project_encode(b->snapshot[snapshot], &p);
}
static void jif_expect_status(int actual, int wanted, const char *case_name,
                              uint32_t snapshot, uint32_t engine, uint32_t id, int32_t value)
{
    ++checks;
    if (actual != wanted) {
        ++failures;
        fprintf(stderr, "%s: snapshot=%u engine=%u parameter=%u value=%d: status=%d wanted=%d\n",
                case_name, snapshot, engine, id, value, actual, wanted);
    }
}
static int jif_source_status(uint32_t track, uint32_t engine, uint32_t id, int32_t value)
{
    if (track == TRK_DRUM && id == P_E0 && value >= (int32_t)DRUM_USR && value <= (int32_t)DRUM_PAIR)
        return JR_VALID_UNSUPPORTED;
    if (track < NPART && (ENGINES[engine] == &ENG_SAMPLE || engine == ENGI_GRAIN) &&
        id == P_E0 && value >= (int32_t)SMP_NSETS) return JR_VALID_UNSUPPORTED;
    if (track < NPART && engine == ENGI_FM6 && id == P_E7 && value >= (int32_t)FM6_NFACTORY)
        return JR_VALID_UNSUPPORTED;
    return JR_VALID_OK;
}
static void jif_parameter_case(jif_bundle_t *b, uint32_t snapshot, uint32_t track,
                               uint32_t engine, uint32_t id, int32_t value, int wanted, int lock)
{
    project_t q; jif_project_decode(&q, b->snapshot[snapshot]);
    if (lock) q.t[track].lock[0] = (plock_t){63, (uint8_t)id, (int16_t)value};
    else pt_set(&q.t[track], id, value);
    jif_project_encode(b->snapshot[snapshot], &q); /* valid inner FNV for malformed fields too */
    jif_expect_status(jif_validate(b), wanted, lock ? "CRC-valid parameter lock" : "CRC-valid parameter",
                      snapshot, engine, id, value);
}

static void jif_test_ranges_and_dependencies(void)
{
    static jif_bundle_t b;
    unsigned before_fail = failures;
    for (uint32_t track = 0; track < NTRK; ++track) {
        uint32_t limit = track == TRK_DRUM ? 1 : NENGINES;
        for (uint32_t engine = 0; engine < limit; ++engine) {
            for (uint32_t id = 0; id < P_COUNT; ++id) {
                const param_desc_t *d = id >= P_E0 ? &ENGINES[engine]->edit[id - P_E0] : &TP[id];
                if (track == TRK_DRUM && id == P_E0) d = &DRUM_KIT_DESC;
                int32_t values[4] = {d->min, d->max, d->min - 1, d->max + 1};
                for (uint32_t boundary = 0; boundary < 4; ++boundary) {
                    int32_t value = values[boundary];
                    /* JRM1's ten byte-sized parameters cannot express negative
                     * values; their upper-bound rejection is still exercised. */
                    int wanted = boundary < 2 ? jif_source_status(track, engine, id, value) : JR_VALID_INVALID;
                    if (!(id >= P_ITYPE && id < P_E0 && value < 0)) {
                        b = expected; jif_configure_engine(&b, 0, track, engine);
                        jif_parameter_case(&b, 0, track, engine, id, value, wanted, 0);
                    }
                    if (p_lockable(id)) {
                        b = expected; jif_configure_engine(&b, 0, track, engine);
                        jif_parameter_case(&b, 0, track, engine, id, value, wanted, 1);
                    }
                }
            }
        }
    }
    check(before_fail == failures, "wire validator agrees with real descriptors at every engine/common/drum boundary and lock boundary");
    before_fail = failures;
    for (uint32_t snapshot = 0; snapshot < JR_SNAPSHOTS; ++snapshot) {
        for (uint32_t engine = 0; engine < NENGINES; ++engine) {
            b = expected; jif_configure_engine(&b, snapshot, 0, engine);
            jif_expect_status(jif_validate(&b), JR_VALID_OK, "all twelve built-in engines", snapshot, engine, 0, 0);
            if (ENGINES[engine] == &ENG_SAMPLE || engine == ENGI_GRAIN) {
                for (uint32_t source = 0; source < SMP_NALL; ++source)
                    for (int lock = 0; lock <= 1; ++lock) {
                        b = expected; jif_configure_engine(&b, snapshot, 0, engine);
                        jif_parameter_case(&b, snapshot, 0, engine, P_E0, (int32_t)source,
                                           source < SMP_NSETS ? JR_VALID_OK : JR_VALID_UNSUPPORTED, lock);
                    }
            }
            if (engine == ENGI_FM6) {
                for (uint32_t slot = 0; slot < FM6_NSLOT; ++slot)
                    for (int lock = 0; lock <= 1; ++lock) {
                        b = expected; jif_configure_engine(&b, snapshot, 0, engine);
                        jif_parameter_case(&b, snapshot, 0, engine, P_E7, (int32_t)slot,
                                           slot < FM6_NFACTORY ? JR_VALID_OK : JR_VALID_UNSUPPORTED, lock);
                    }
            }
        }
        for (uint32_t kit = 0; kit < DRUM_KITS; ++kit)
            for (int lock = 0; lock <= 1; ++lock) {
                b = expected; jif_configure_engine(&b, snapshot, TRK_DRUM, 0);
                jif_parameter_case(&b, snapshot, TRK_DRUM, 0, P_E0, (int32_t)kit,
                                   kit >= DRUM_USR && kit <= DRUM_PAIR ? JR_VALID_UNSUPPORTED : JR_VALID_OK, lock);
            }
    }
    check(before_fail == failures, "all five snapshots resolve every sample/kit/FM6 dependency including hidden-step source locks");

    /* A valid opaque JRM1 hash never substitutes for semantic validation. */
    before_fail = failures;
    for (uint32_t s = 0; s < JR_SNAPSHOTS; ++s) {
        project_t q;
        b = expected; jif_project_decode(&q, b.snapshot[s]);
        q.t[0].step[63].n = 5; jif_project_encode(b.snapshot[s], &q);
        jif_expect_status(jif_validate(&b), JR_VALID_INVALID, "CRC-valid invalid note count", s, 0, 0, 5);
        b = expected; jif_project_decode(&q, b.snapshot[s]);
        q.note_len[2][63][0] = 65; jif_project_encode(b.snapshot[s], &q);
        jif_expect_status(jif_validate(&b), JR_VALID_INVALID, "CRC-valid invalid duration", s, 0, 0, 65);
        b = expected; jif_project_decode(&q, b.snapshot[s]);
        q.t[0].step[63].time = ST_REST; q.note_len[0][63][0] = 5;
        jif_project_encode(b.snapshot[s], &q);
        jif_expect_status(jif_validate(&b), JR_VALID_INVALID, "CRC-valid duration attached to rest", s, 0, 0, 5);
        b = expected; jif_project_decode(&q, b.snapshot[s]);
        q.t[0].step[63].n = 1; q.note_len[0][63][3] = 5;
        jif_project_encode(b.snapshot[s], &q);
        jif_expect_status(jif_validate(&b), JR_VALID_INVALID, "CRC-valid duration outside chord slots", s, 0, 0, 5);
        b = expected; jif_project_decode(&q, b.snapshot[s]);
        q.t[0].mf[63] = 0xC0; jif_project_encode(b.snapshot[s], &q);
        jif_expect_status(jif_validate(&b), JR_VALID_INVALID, "CRC-valid unknown fill condition", s, 0, 0, 3);
        b = expected; jif_project_decode(&q, b.snapshot[s]);
        q.t[1].lock[0] = (plock_t){63, P_MUTE, 2}; jif_project_encode(b.snapshot[s], &q);
        jif_expect_status(jif_validate(&b), JR_VALID_INVALID, "CRC-valid non-lockable parameter", s, 0, P_MUTE, 2);
        b = expected; jif_project_decode(&q, b.snapshot[s]);
        q.g[G_BPM] = 39; jif_project_encode(b.snapshot[s], &q);
        jif_expect_status(jif_validate(&b), JR_VALID_INVALID, "CRC-valid invalid tempo", s, 0, G_BPM, 39);
    }
    check(before_fail == failures, "CRC-valid malformed musical state cannot reach real apply-time clamping");
}

static void jif_test_curated_sounds(void)
{
    static jif_bundle_t b;
    unsigned before_fail = failures;
    for (uint32_t track = 0; track < NTRK; ++track)
        for (uint32_t index = 0; index < jam_sound_count(track); ++index) {
            b = expected; jif_apply(&b, 0);
            check(jam_sound_load(track, index), "real curated preset loads for recovery identity and dependency audit");
            for (uint32_t t = 0; t < NPART; ++t) {
                trk[t].engine = trk[t].eng_req;
                fm6_slot[t] = 255;
            }
            fm6_poll();
            /* Preserve all current metadata except the intentionally chosen
             * sound's explicit stable origin/ID. Capture never reloads a preset. */
            project_t q; proj_capture(&q); jif_project_encode(b.snapshot[0], &q);
            memset(b.patch[0], 0, sizeof b.patch[0]);
            for (uint32_t t = 0; t < NPART; ++t)
                if (trk[t].eng_req == ENGI_FM6) memcpy(b.patch[0][t], fm6_patch[t], JR_FM6_BYTES);
            jr_codec_identity_set(b.meta, 0, track, (track + 1) * 256 + index + 1, 1,
                                  track < NPART ? q.t[track].engine : 0, 1);
            jif_expect_status(jif_validate(&b), JR_VALID_OK, "curated identity/source", 0,
                              track < NPART ? q.t[track].engine : 0, track, (int32_t)index);
        }
    check(before_fail == failures, "all forty-four curated sounds fit the recovery dependency contract with stable explicit identities");
}

int main(int argc, char **argv)
{
    uint32_t seed = argc == 4 ? (uint32_t)strtoul(argv[3], NULL, 10) : 1;
    if (!seed || seed > 9999) { fprintf(stderr, "fixture seed must be 1..9999\n"); return 2; }
    jif_make(&expected, seed);
    check(jif_validate(&expected) == JR_VALID_OK, "complete real-instrument fixture passes independent field/dependency validation");
    if (failures) return 1;
    if (argc == 4 && !strcmp(argv[1], "--emit")) {
        check(jif_write_file(argv[2], &expected), "emit complete explicit little-endian instrument payload to file");
    } else if (argc == 4 && !strcmp(argv[1], "--verify")) {
        check(jif_read_file(argv[2]), "cold process reads exact payload without retained process state");
        jr_source_t src = {payload, jif_memory_read, sizeof payload};
        check(jif_unpack(&restored, &src), "validate entire persisted bundle before applying any instrument state");
        if (!failures) {
            check(!memcmp(&expected, &restored, sizeof expected), "cold-start bundle matches expected generation including every component");
            jif_check_restored(&restored);
        }
    } else if (argc == 1) {
        jr_payload_view_t v = jif_view(&expected);
        jr_source_t src = {&v, jr_codec_read_at, JR_PAYLOAD_BYTES};
        check(jif_unpack(&restored, &src), "streamed complete fixture decodes without trusting a raw native struct");
        if (!failures) jif_check_restored(&restored);
        if (!failures) jif_test_ranges_and_dependencies();
        if (!failures) jif_test_curated_sounds();
    } else {
        fprintf(stderr, "usage: %s [--emit|--verify PAYLOAD SEED]\n", argv[0]); return 2;
    }
    printf("Experimental real-instrument recovery: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
