/* SPDX-License-Identifier: GPL-3.0-only */
/* Real device recovery adapter, instrument, UI and flash-policy integration.
 * Only the NOR chip and interrupt-freeze operations are host substitutions.
 * No experimental codec fixture creates or restores the musical session. */
#define _POSIX_C_SOURCE 200809L
#define JAMBOX_RECOVERY 1
#define JAM_STORAGE_LIBRARY 1
#include "jam_storage_harness.c"

static int jrt_program_fail_after = -1, jrt_erase_fail_after = -1;
static int jrt_update_during_program_after = -1;
static int jrt_read_fail_after = -1;
static uint32_t jrt_corrupt_read_address = UINT32_MAX;
static uint32_t jrt_corrupt_read_width = 1;
static unsigned jrt_program_calls, jrt_erase_calls, jrt_read_calls, jrt_faults;
static int jrt_read(void *ctx, uint32_t off, void *dst, uint32_t n)
{
    unsigned call = jrt_read_calls++;
    if (jrt_read_fail_after >= 0 && call == (unsigned)jrt_read_fail_after) {
        ++jrt_faults;
        return -97;
    }
    int rc = jr_hw_read(ctx, off, dst, n);
    if (!rc && n == jrt_corrupt_read_width && off == jrt_corrupt_read_address) {
        ((uint8_t *)dst)[0] ^= 1;
        jrt_corrupt_read_address = UINT32_MAX;
        ++jrt_faults;
    }
    return rc;
}
static int jrt_erase(void *ctx, uint32_t off)
{
    unsigned call = jrt_erase_calls++;
    if (jrt_erase_fail_after >= 0 && call == (unsigned)jrt_erase_fail_after) {
        ++jrt_faults;
        return -98;
    }
    return jr_hw_erase(ctx, off);
}
static int jrt_program(void *ctx, uint32_t off, const void *src, uint32_t n)
{
    unsigned call = jrt_program_calls++;
    if (jrt_program_fail_after >= 0 && call == (unsigned)jrt_program_fail_after) {
        ++jrt_faults;
        return -99;
    }
    int rc = jr_hw_program(ctx, off, src, n);
    if (!rc && jrt_update_during_program_after >= 0 &&
            call == (unsigned)jrt_update_during_program_after) {
        usb.ota_req = 1;
        ++jrt_faults;
    }
    return rc;
}
#define JAM_RECOVERY_HOST 1
#define jr_hw_read jrt_read
#define jr_hw_erase jrt_erase
#define jr_hw_program jrt_program
#include "../firmware/src/jam_recovery_runtime.c"
#undef jr_hw_read
#undef jr_hw_erase
#undef jr_hw_program

typedef struct {
    project_t current, section[4];
    jam_recovery_sidecar_t section_sound[4];
    uint8_t patch[NPART][FP_SIZE + 1u], sound_id[NTRK], sound_valid;
    dsu_bank_t syn;
    arr_config_t order;
    uint16_t learn[ML_N];
    uint8_t channels[6];
    uint32_t palette, lowcut, zoom;
    uint8_t lights, keys, notes, full, serial, mode, count, sync, out, in;
    uint8_t home, enabled;
    int8_t selected_section, octave;
} jrt_session_t;
static jrt_session_t jrt_old, jrt_new;
static uint8_t jrt_saved_flash[sizeof nor];
static uint8_t jrt_dual_flash[sizeof nor];

static void jrt_fault_reset(void)
{
    jrt_read_fail_after = jrt_erase_fail_after = jrt_program_fail_after = -1;
    jrt_update_during_program_after = -1;
    jrt_corrupt_read_address = UINT32_MAX;
    jrt_corrupt_read_width = 1;
    jrt_read_calls = jrt_erase_calls = jrt_program_calls = jrt_faults = 0;
}
static void jrt_boot(void)
{
    /* Power-on clears normal BSS. Retained slots/settings are deliberately
     * poisoned separately in tests to exercise the production boot policy. */
    memset(&song, 0, sizeof song);
    memset(trk, 0, sizeof trk);
    memset(&ui, 0, sizeof ui);
    memset(&drums, 0, sizeof drums);
    memset(&arrangement_clock, 0, sizeof arrangement_clock);
    memset(&dsu, 0, sizeof dsu);
    memset((void *)ml_tab, 0, sizeof ml_tab);
    memcpy(midi_set, (const uint8_t[])MIDI_SET_DEF, sizeof midi_set);
    lights_lvl = lights_keys = lights_sync = lights_mout = lights_min = 0;
    lights_notes = 1;
    rec_tempo = rec_count = usb_full = usb_serial = 0;
    rec_wait = ft_on = ci_on = transport_req = panic_req = 0;
    memset(&usb, 0, sizeof usb);
    sx_ready = sx_collect = sx_busy = sx_open = sx_cancel = 0;
    mi_r = mi_w = mo_r = mo_w = so_r = so_w = 0;
    fm1_in.notes = fm1_in.buttons = edges_btn = notes_seen = 0;
    jam_recovery_ui_modal = 0;
    memset(encs, 0, sizeof encs);
    jr_transaction_active = 0;
    jedec = 0x856014u;
    ordinary_boot();
    jam_recovery_boot();
}
static void jrt_capture(jrt_session_t *out)
{
    memset(out, 0, sizeof *out);
    proj_capture(&out->current);
    for (uint32_t s = 0; s < 4; ++s) {
        out->section[s] = *project_slot(s);
        out->section_sound[s] = *jam_recovery_section_sidecar(s);
    }
    for (uint32_t t = 0; t < NPART; ++t)
        if (trk[t].eng_req == ENGI_FM6)
            memcpy(out->patch[t], fm6_patch[t], sizeof out->patch[t]);
    /* Drum curation is derived from its kit, whereas melodic curation uses
     * jam_sound_valid. Compare the actual user-visible identity for both. */
    for (uint32_t t = 0; t < NTRK; ++t)
        if (jam_sound_is_curated(t)) {
            out->sound_id[t] = (uint8_t)jam_sound_index(t);
            out->sound_valid |= (uint8_t)(1u << t);
        }
    out->syn = dsu;
    out->order = arrangement;
    memcpy(out->learn, (const void *)ml_tab, sizeof out->learn);
    memcpy(out->channels, midi_set, sizeof out->channels);
    out->palette = settings.palette; out->lowcut = settings.lowcut; out->zoom = settings.zoom;
    out->lights = lights_lvl; out->keys = lights_keys; out->notes = lights_notes;
    out->full = usb_full; out->serial = usb_serial; out->mode = rec_tempo; out->count = rec_count;
    out->sync = lights_sync; out->out = lights_mout; out->in = lights_min;
    out->home = ui.jam_home_view; out->enabled = arrangement_enabled;
    out->selected_section = live_sec; out->octave = song.octave;
}
static int jrt_matches(const jrt_session_t *expected)
{
    jrt_session_t actual;
    jrt_capture(&actual);
    return !memcmp(&actual, expected, sizeof actual);
}
static void jrt_check_session(const jrt_session_t *expected)
{
    jrt_session_t actual;
    jrt_capture(&actual);
    check(!memcmp(&actual.current, &expected->current, sizeof actual.current),
          "real boot restores current notes, independent lengths, mix, sounds and globals");
    check(!memcmp(actual.section, expected->section, sizeof actual.section),
          "real boot restores all four complete A-D musical snapshots");
    check(!memcmp(actual.section_sound, expected->section_sound, sizeof actual.section_sound),
          "real boot restores A-D FM6 edits and curated sound identities");
    check(!memcmp(actual.patch, expected->patch, sizeof actual.patch),
          "real boot restores actual current FM6 patches rather than factory slots");
    check(!memcmp(actual.sound_id, expected->sound_id, sizeof actual.sound_id) &&
          actual.sound_valid == expected->sound_valid,
          "real boot restores current curated preset names and positions");
    check(!memcmp(&actual.syn, &expected->syn, sizeof actual.syn),
          "real boot restores all four edited SYN kits");
    check(!memcmp(&actual.order, &expected->order, sizeof actual.order) &&
          actual.enabled == expected->enabled && actual.selected_section == expected->selected_section,
          "real boot restores arrangement order, loop and selected A-D section");
    check(!memcmp(actual.learn, expected->learn, sizeof actual.learn) &&
          !memcmp(actual.channels, expected->channels, sizeof actual.channels),
          "real boot restores MIDI learn and device routing preferences");
    check(actual.palette == expected->palette && actual.lowcut == expected->lowcut &&
          actual.zoom == expected->zoom && actual.lights == expected->lights &&
          actual.keys == expected->keys && actual.notes == expected->notes &&
          actual.full == expected->full && actual.serial == expected->serial &&
          actual.mode == expected->mode && actual.count == expected->count &&
          actual.sync == expected->sync && actual.out == expected->out && actual.in == expected->in,
          "real boot restores display, lighting, USB and recording preferences");
    check(actual.home == expected->home && actual.octave == expected->octave &&
          song.sel == expected->current.sel,
          "real boot restores selected track, octave and last Home view");
    check(!song.playing && !song.rec && !rec_wait && !ft_on && !ci_on && !transport_req &&
          !arrangement_clock.running && !fm1_in.notes,
          "restoration leaves transport stopped, REC disarmed and no held keys");
}
static void jrt_seed_loop(uint32_t variant)
{
    (void)jam_new_dusty();
    check(jam_sound_load(0, variant % JAM_MELODIC_SOUNDS) && jam_sound_load(1, 10) &&
          jam_sound_load(2, 11) && jam_sound_load(TRK_DRUM, variant % JAM_DRUM_SOUNDS),
          "real curated sound loader prepares complete four-track fixture");
    for (uint32_t t = 0; t < NTRK; ++t) {
        track_t *q = &trk[t];
        q->engine = q->eng_req;
        steps_clear(q);
        q->p[P_SLEN] = (int16_t)(16 + (variant % 4) * 8);
        q->p[P_LEVEL] = (int16_t)(72 + t * 7 + variant % 9);
        q->p[P_PAN] = (int16_t)(-21 + (int32_t)t * 13);
        q->p[P_ROOT] = (int16_t)((3 + variant) % 12);
        q->p[P_SCALE] = 1;
        q->p[P_QUANT] = 1;
        q->p[P_SSWING] = (int16_t)(variant * 3 % 101);
        for (uint32_t i = 0; i < NSTEP; ++i) {
            if (t == TRK_DRUM) {
                dstep_set(&q->dstep[i], (i + variant) % DRUM_LANES,
                          1 + i % 3, i % 4);
            } else {
                step_t *s = &q->step[i];
                s->n = (uint8_t)(t ? 1 + i % 4 : 1);
                s->time = ST_NOTE; s->vel = (uint8_t)(63 + i % 64);
                for (uint32_t n = 0; n < s->n; ++n) {
                    s->note[n] = (uint8_t)(39 + t * 12 + (i + variant) % 12 + n * 3);
                    q->note_len[i][n] = (uint8_t)(1 + (i + n + variant) % 64);
                }
                jam_lengths_changed(q);
            }
            q->micro[i] = (int8_t)((int32_t)((i + variant) % 64) - 32);
            step_fill_set(q, i, i % 3);
        }
        q->lock[0] = (plock_t){3, P_LEVEL, (int16_t)(65 + variant % 30)};
        if (t < NPART && q->eng_req == ENGI_FM6) {
            uint8_t patch[FP_SIZE + 1u];
            memcpy(patch, fm6_patch[t], sizeof patch);
            patch[FP_ALG] = (uint8_t)((variant * 3 + t) % 32);
            patch[FP_FB] = (uint8_t)((variant + t) % 8);
            patch[FP_OL] = (uint8_t)(72 + variant % 20);
            memcpy(patch + FP_NAME, "JAM EDIT  ", 10);
            patch[FP_NAME + 9] = (uint8_t)('A' + variant % 26);
            fm6_set_patch(t, patch);
            fm6_slot[t] = (uint8_t)q->p[P_E7];
            fm6_pend &= (uint8_t)~(1u << t);
        }
    }
    song.g[G_BPM] = (int16_t)(91 + variant);
    song.g[G_SWING] = (int16_t)(variant * 7 % 101);
    song.g[G_DFDBK] = (int16_t)(29 + variant % 30);
    song.g[G_TXSRC] = 0;
    song.sel = (uint8_t)(variant % NTRK);
    transport_req = panic_req = 0;
}
static void jrt_seed_session(uint32_t seed)
{
    for (uint32_t s = 0; s < 4; ++s) {
        jrt_seed_loop(seed + s);
        section_store(s);
    }
    jrt_seed_loop(seed + 4);
    dsu_defaults();
    for (uint32_t k = 0; k < DSU_N; ++k) {
        snprintf(dsu.k[k].name, sizeof dsu.k[k].name, "JAM%u-%u", k, seed);
        for (uint32_t n = 0; n < DS_LANES; ++n) {
            dsu.k[k].s[n].pitch = (uint8_t)(35 + n + k + seed % 12);
            dsu.k[k].s[n].decay = (uint8_t)(28 + n + seed % 10);
            dsu.k[k].s[n].fenv = (int8_t)(-21 + (int32_t)n + (int32_t)k);
        }
        dsu_fix_kit(&dsu.k[k]);
    }
    arrangement.count = 8; arrangement.loop = 1;
    for (uint32_t i = 0; i < ARR_STEPS; ++i) {
        arrangement.entry[i].scene = (uint8_t)((i + seed) % 4);
        arrangement.entry[i].bars = (uint8_t)(1 + (i + seed) % 8);
    }
    arrangement_enabled = 1; live_sec = 2;
    settings.palette = seed % 5; settings.lowcut = settings.zoom = 1;
    lights_lvl = 2; lights_keys = 3; lights_notes = 1;
    rec_tempo = rec_count = usb_full = 1; usb_serial = 0;
    lights_sync = 2; lights_mout = 1; lights_min = 17;
    memcpy(midi_set, (const uint8_t[]){4, 5, 6, 11, 1, 7}, sizeof midi_set);
    song.g[G_CH1] = 4; song.g[G_CH2] = 5; song.g[G_CH3] = 6;
    song.g[G_DRCH] = 11; song.g[G_CHOTH] = 1; song.g[G_BEND] = 7;
    song.g[G_SYNC] = 2; song.g[G_MIDI] = 1; song.g[G_ROUTE] = 17;
    for (uint32_t i = 0; i < ML_N; ++i)
        ml_tab[i] = (uint16_t)((20 + i + (i >= 12)) | ((i % NTRK) << 7) | ((P_LEVEL + 1) << 9));
    song.octave = -1; ui.jam_home_view = JAM_HOME_SOUND;
    song.playing = song.rec = rec_wait = ft_on = ci_on = transport_req = panic_req = 0;
    fm1_in.notes = fm1_in.buttons = 0;
}
static void jrt_test_sidecars(void)
{
    memset(nor, 255, sizeof nor); jrt_boot();
    jrt_seed_loop(1); section_store(0);
    uint8_t first[FP_SIZE + 1u]; memcpy(first, fm6_patch[1], sizeof first);
    jrt_seed_loop(2); section_store(1);
    uint8_t second[FP_SIZE + 1u]; memcpy(second, fm6_patch[1], sizeof second);
    check(memcmp(first, second, sizeof first) != 0, "section test uses two genuinely different FM6 edits");
    arrangement_apply(0); fm6_poll();
    check(!memcmp(first, fm6_patch[1], sizeof first), "A to B to A restores A's edited FM6 patch");
    arrangement_apply(1); fm6_poll();
    check(!memcmp(second, fm6_patch[1], sizeof second), "section B restores B's edited FM6 patch");
    arrangement_apply(0); fm6_poll();
    check(!memcmp(first, fm6_patch[1], sizeof first), "return to section A survives subsequent factory polling");
    song_backup(); arrangement_apply(1); song_restore(); fm6_poll();
    check(!memcmp(first, fm6_patch[1], sizeof first), "arranger working-loop backup restores actual edited FM6 patch");
    check(jam_sound_is_curated(1) && jam_sound_index(1) == 10,
          "section and working-loop restoration preserve curated sound name");
}
static void jrt_test_write_gates(void)
{
    uint8_t byte = 0;
    static const uint32_t forbidden[] = {0, 0x96000, 0xa0000, 0xdb000, 0xe0000,
        0xe5000, 0xe7000, 0xfb000, 0xfc000, 0xfd000, 0xff000, 0xfffff};
    snapshot(); jr_transaction_active = 0;
    check(jr_hw_erase(NULL, 0x97000) != 0 && jr_hw_program(NULL, 0x97000, &byte, 1) != 0,
          "production recovery callbacks deny writes outside an authorized transaction");
    jr_transaction_active = 1;
    for (uint32_t i = 0; i < sizeof forbidden / sizeof forbidden[0]; ++i) {
        check(jr_hw_erase(NULL, forbidden[i]) != 0 &&
              jr_hw_program(NULL, forbidden[i], &byte, 1) != 0,
              "recovery callback policy rejects application, sample, calibration and OTA ranges");
    }
    check(jr_hw_program(NULL, 0x970ff, &byte, 2) != 0 &&
          jr_hw_program(NULL, 0x97000, &byte, 257) != 0 &&
          jr_hw_erase(NULL, 0x97001) != 0,
          "recovery callbacks reject crossed pages, oversized programs and misaligned erases");
    jr_transaction_active = 0;
    unchanged("rejected recovery callback requests never reach the raw NOR chip");
}
static void jrt_test_save_boot(void)
{
    panel_t calibration = alternative_panel();
    memset(nor, 255, sizeof nor);
    seed_settings(0x50455233u, 1592u, &calibration);
    jrt_fault_reset(); jrt_boot();
    check(!memcmp(&panel, &calibration, sizeof panel),
          "pre-existing validated hardware calibration is active before recovery setup");
    check(!jam_recovery_is_claimed(), "empty flash has no recovery allocation claim");
    snapshot();
    check(jam_recovery_save(0) != 0, "first save requires explicit recovery initialization");
    unchanged("ordinary Save now cannot silently retire the legacy allocation");
    jrt_seed_session(1); jrt_capture(&jrt_old);
    check(jam_recovery_save(1) == 0 && raw_erases == JR_STORE_SECTORS && raw_programs > 0,
          "explicit first save commits through real bounded recovery flash callbacks");
    check(!jr_transaction_active && jam_recovery_is_claimed(),
          "successful transaction closes write authorization and records the allocation claim");
    check(!strcmp(jam_recovery_status(), "Saved") && !jrr_audio_frozen,
          "successful checked save reports Saved and resumes the audio interrupt");
    check(!memcmp(before, nor, 0x97000) &&
          !memcmp(before + 0xa0000, nor + 0xa0000, 0x3c000) &&
          !memcmp(before + 0xe0000, nor + 0xe0000, 0x1e000) &&
          !memcmp(before + 0xff000, nor + 0xff000, 0x1000),
          "full session save preserves firmware, samples, calibration, FM6 and update allocations");
    memcpy(jrt_saved_flash, nor, sizeof nor);
    memset(project_slot(0), 0xa7, sizeof(project_t));
    memset(&settings, 0xa7, sizeof settings);
    snapshot(); jrt_boot();
    jrt_check_session(&jrt_old);
    check(!memcmp(&panel, &calibration, sizeof panel),
          "saved-session restoration preserves the independent physical panel calibration");
    check(!strcmp(jam_recovery_status(), "Saved") && !jrr_audio_frozen,
          "fully verified boot reports Saved without leaving audio frozen");
    unchanged("recovery boot restores without programming or repairing any NOR byte");
    fm6_poll();
    check(!memcmp(fm6_patch[1], jrt_old.patch[1], sizeof fm6_patch[1]),
          "first post-boot FM6 poll preserves recovered operator edits");
    jrt_seed_session(9); jrt_capture(&jrt_new);
    jrt_fault_reset(); jrt_erase_fail_after = 2;
    check(jam_recovery_save(0) != 0 && jrt_faults == 1 && !jr_transaction_active &&
          !jrr_audio_frozen && !strcmp(jam_recovery_status(), "Error"),
          "sector erase failure reports Error, revokes write permission and unfreezes audio");
    jrt_fault_reset(); snapshot(); jrt_boot();
    jrt_check_session(&jrt_old);
    unchanged("failed sector erase preserves the complete previous recovery generation");
    jrt_seed_session(9); jrt_capture(&jrt_new);
    jrt_fault_reset(); jrt_program_fail_after = 3;
    check(jam_recovery_save(0) != 0 && jrt_faults == 1 && !jr_transaction_active &&
          !jrr_audio_frozen && !strcmp(jam_recovery_status(), "Error"),
          "real program failure reports failure and always revokes transaction authorization");
    jrt_fault_reset(); snapshot(); jrt_boot();
    jrt_check_session(&jrt_old);
    unchanged("failed new save leaves the old whole generation recoverable without writes");
    jrt_seed_session(9); jrt_capture(&jrt_new);
    check(jam_recovery_save(0) == 0, "later Save now succeeds after an interrupted transaction");
    memcpy(jrt_dual_flash, nor, sizeof nor);
    snapshot(); jrt_boot(); jrt_check_session(&jrt_new);
    unchanged("new generation round-trip is also read-only at boot");
    ui_input_ms++;
    check(!strcmp(jam_recovery_status(), "Pending"),
          "new panel interaction conservatively marks the saved session Pending");
    jrt_boot(); mi_w++;
    check(!strcmp(jam_recovery_status(), "Pending"),
          "new MIDI activity cannot leave a misleading Saved indication");
}
static void jrt_test_late_read_failures(void)
{
    memcpy(nor, jrt_dual_flash, sizeof nor);
    jrt_fault_reset(); jrt_boot();
    unsigned count = jrt_read_calls;
    check(count > 32 && jrt_matches(&jrt_new),
          "baseline real boot validates and restores the newest of two complete generations");
    if (count < 32) return;
    /* Exercise reads after the initial candidate scans as well as the last
     * application/verification reads. A later component failure must never
     * publish a mixture of old/new sections, kits, preferences or FM6 patches. */
    for (unsigned i = count - 32; i < count; ++i) {
        memcpy(nor, jrt_dual_flash, sizeof nor);
        jrt_fault_reset(); jrt_read_fail_after = (int)i;
        snapshot(); jrt_boot();
        check(jrt_faults == 1 && (jrt_matches(&jrt_old) || jrt_matches(&jrt_new)),
              "late read error publishes one complete generation, never partially applied state");
        unchanged("read-error rollback and older-copy recovery never write or repair flash");
    }
    jrt_fault_reset();
}
static void jrt_test_transient_component_corruption(void)
{
    static const struct { uint32_t offset, width; const char *name; } faults[] = {
        {0, 4, "current project magic"}, {4, 4, "current project size"},
        {JR_SNAPSHOT_BYTES, 4, "section A project magic"},
        {JR_SNAPSHOT_BYTES + 4, 4, "section A project size"},
        {JR_SNAPSHOT_BYTES * 4, 4, "section D project magic"},
        {JR_SNAPSHOT_BYTES * 4 + 4, 4, "section D project size"},
        {JR_SYN_OFFSET, 4, "SYN bank magic"},
        {JR_SYN_OFFSET + 4, 2, "SYN bank version"},
        {JR_SYN_OFFSET + 6, 2, "SYN bank slot count"},
        {JR_SYN_OFFSET + 8 + 12 + 2, 1, "semantically valid SYN pitch"}
    };
    for (uint32_t i = 0; i < sizeof faults / sizeof faults[0]; ++i) {
        memcpy(nor, jrt_dual_flash, sizeof nor);
        jrt_fault_reset();
        /* Target typed decoding only: semantic validators/outer CRC readers
         * use larger reads. The committed NOR image remains unchanged. */
        uint32_t logical = JR_STORE_HEADER_BYTES + faults[i].offset;
        jrt_corrupt_read_address = jr_store_sector_address(1, logical / JR_STORE_SECTOR_BYTES) +
            logical % JR_STORE_SECTOR_BYTES;
        jrt_corrupt_read_width = faults[i].width;
        snapshot(); jrt_boot();
        char label[120];
        snprintf(label, sizeof label, "transient %s corruption cannot publish altered session data", faults[i].name);
        check(jrt_faults == 1 && (jrt_matches(&jrt_old) || jrt_matches(&jrt_new)), label);
        unchanged("component-integrity rollback never repairs or writes flash");
    }
    jrt_fault_reset();
}
static void jrt_test_transfer_arrives_during_save(void)
{
    memcpy(nor, jrt_saved_flash, sizeof nor); jrt_boot();
    jrt_seed_session(9); jrt_fault_reset(); jrt_update_during_program_after = 3;
    check(jam_recovery_save(0) != 0 && jrt_faults == 1 && usb.ota_req &&
          !jr_transaction_active && !jrr_audio_frozen,
          "USB update arriving during save aborts before commit and retains its pending request");
    jrt_fault_reset(); snapshot(); jrt_boot(); jrt_check_session(&jrt_old);
    unchanged("transfer-interrupted save leaves the preceding whole generation recoverable");
}
static void jrt_test_busy_states(void)
{
    static const char *const labels[] = {"playing", "recording", "record armed", "free take",
        "count-in", "transport request", "physical key", "pending USB SysEx", "update request",
        "held synth voice", "MIDI sustain", "held arpeggiator notes", "incoming SysEx",
        "outgoing SysEx", "UBOOT request"};
    for (uint32_t i = 0; i < sizeof labels / sizeof labels[0]; ++i) {
        jrt_boot();
        switch (i) {
        case 0: song.playing = 1; break;
        case 1: song.rec = 1; break;
        case 2: rec_wait = 1; break;
        case 3: ft_on = 1; break;
        case 4: ci_on = 1; break;
        case 5: transport_req = 1; break;
        case 6: fm1_in.notes = 1; break;
        case 7: sx_ready = 1; break;
        case 8: usb.ota_req = 1; break;
        case 9: trk[0].v[0].active = trk[0].v[0].gate = 1; break;
        case 10: ch_sus[15] = 1; break;
        case 11: trk[0].nheld = 1; break;
        case 12: sx_collect = 1; break;
        case 13: so_w++; break;
        case 14: usb.uboot_req = 1; break;
        }
        snapshot();
        char label[100]; snprintf(label, sizeof label, "Save now rejects %s before writing or claiming success", labels[i]);
        check(!jam_recovery_can_save() && jam_recovery_save(0) != 0 &&
              jam_recovery_block_reason() && *jam_recovery_block_reason() && !jr_transaction_active, label);
        unchanged("busy-state refusal preserves the entire NOR image");
    }
}
static void jrt_test_editor_dirty(void)
{
    memcpy(nor, jrt_dual_flash, sizeof nor); jrt_boot();
    check(!strcmp(jam_recovery_status(), "Saved"), "editor dirty test starts from verified saved state");
    uint8_t args[4] = {0, P_LEVEL, (8192 + 83) & 127, (8192 + 83) >> 7};
    snapshot(); command(ED_SET, args, sizeof args);
    check(TSEL->p[P_LEVEL] == 83 && !strcmp(jam_recovery_status(), "Pending"),
          "real editor parameter write marks recovery Pending without a physical input timestamp");
    unchanged("live editor edits only change RAM until explicit Save now");
}
static void jrt_test_invalid_source_retry(void)
{
    jrt_session_t expected;
    memcpy(nor, jrt_dual_flash, sizeof nor); jrt_boot();
    TDRUM->p[P_E0] = DRUM_USR;
    snapshot();
    check(jam_recovery_save(0) == JR_STORE_INVALID_SOURCE &&
          !strcmp(jam_recovery_status(), "Error") && jam_recovery_can_save() &&
          jam_recovery_is_claimed() && !jr_transaction_active && !jrr_audio_frozen,
          "unsupported user-sample dependency reports Error while retaining the prior writable allocation");
    unchanged("unsupported source rejection does not erase or alter the last saved generation");
    check(jam_sound_load(TRK_DRUM, 0), "user can replace an unsupported sample reference with a built-in kit");
    jrt_capture(&expected);
    check(jam_recovery_save(0) == 0 && !strcmp(jam_recovery_status(), "Saved"),
          "choosing a built-in sound allows immediate successful retry without rebooting");
    snapshot(); jrt_boot(); jrt_check_session(&expected);
    unchanged("retried built-in session restores completely without boot-time writes");
}
static void jrt_test_pending_sound_changes(void)
{
    jrt_session_t expected;
    memcpy(nor, jrt_dual_flash, sizeof nor); jrt_boot();
    for (uint32_t t = 0; t < NPART; ++t) engine_block(&trk[t]);
    uint32_t slot = ((uint32_t)trk[1].p[P_E7] + 1) % FM6_NFACTORY;
    trk[1].p[P_E7] = (int16_t)slot;
    snapshot();
    check(!jam_recovery_can_save() && jam_recovery_save(0) != 0 &&
          !strcmp(jam_recovery_block_reason(), "WAIT FOR SOUND"),
          "pending FM6 slot change prevents saving a new slot with the preceding operator patch");
    unchanged("pending FM6 slot refusal never writes or claims to save");
    ui.recovery_screen = 1; jam_recovery_ui_modal = 1;
    ui_input();
    uint8_t patch[FP_SIZE + 1u]; fm6_unpack(FM6_FACTORY[slot], patch);
    check(fm6_slot[1] == slot && !memcmp(fm6_patch[1], patch, sizeof patch) && jam_recovery_can_save(),
          "open recovery screen continues FM6 polling until the requested factory patch is ready");
    fm6_pend |= 1u << 1;
    snapshot();
    check(!jam_recovery_can_save() && jam_recovery_save(0) != 0,
          "deferred FM6 preset reload also blocks a premature save when its slot number matches");
    unchanged("deferred-patch refusal preserves the prior recovery generation");
    ui_input();
    check(!(fm6_pend & (1u << 1)) && jam_recovery_can_save(),
          "recovery modal services deferred FM6 reloads so Save becomes available again");
    pc_req[TRK_DRUM] = DRUM_DEFAULT_KIT + 1;
    snapshot();
    check(!jam_recovery_can_save() && jam_recovery_save(0) != 0,
          "queued drum Program Change prevents committing the preceding kit");
    unchanged("pending drum Program Change refusal never writes flash");
    ui_input();
    check(!pc_req[TRK_DRUM] && TDRUM->p[P_E0] == DRUM_DEFAULT_KIT && jam_recovery_can_save(),
          "recovery modal services drum Program Changes before allowing Save now");
    jrt_capture(&expected);
    check(jam_recovery_save(0) == 0, "settled sound changes can be saved without closing recovery or rebooting");
    snapshot(); jrt_boot(); jrt_check_session(&expected);
    check(!memcmp(fm6_patch[1], patch, sizeof patch),
          "restart restores the actual newly selected FM6 patch after deferred loading settled");
    unchanged("settled sound-change restoration performs no boot-time writes");
}
static void jrt_test_failed_boot_guard(void)
{
    memcpy(nor, jrt_saved_flash, sizeof nor);
    bootguard.magic = BOOTGUARD_MAGIC; bootguard.failed = 1; bootguard.pending = 0;
    snapshot(); jrt_boot();
    check(song.g[G_BPM] == 88 && project_slots_zero() && !arrangement_enabled &&
          !jam_recovery_can_save() && jam_recovery_save(1) != 0,
          "failed previous boot skips recovery and protects its record read-only");
    unchanged("boot-loop escape preserves the saved session without repair or initialization");
    bootguard_clear(&bootguard);
    snapshot(); jrt_boot(); jrt_check_session(&jrt_old);
    unchanged("healthy boot after guard reset restores the untouched complete saved session");
}
static void jrt_test_fallback(void)
{
    /* One valid A generation exists in the retained reference image. */
    memcpy(nor, jrt_saved_flash, sizeof nor);
    uint32_t a = jr_store_sector_address(0, 0);
    nor[a + JR_STORE_HEADER_BYTES + 47] ^= 1;
    snapshot(); jrt_boot();
    check(song.g[G_BPM] == 88 && project_slots_zero() && !arrangement_enabled &&
          !song.playing && !song.rec && !rec_wait,
          "corrupt only recovery record falls back to a complete fresh Dusty jam");
    unchanged("corrupt recovery does not trigger boot repairs or writes");
    memcpy(nor, jrt_saved_flash, sizeof nor);
    nor[a + JR_STORE_SCHEMA_OFFSET] = 2;
    uint32_t crc = st_crc32(nor + a, JR_STORE_HEADER_CRC_OFFSET);
    memcpy(nor + a + JR_STORE_HEADER_CRC_OFFSET, &crc, sizeof crc);
    snapshot(); jrt_boot();
    check(song.g[G_BPM] == 88 && !jam_recovery_can_save() &&
          jam_recovery_save(1) != 0 && !jr_transaction_active,
          "future committed recovery schema stays protected even after explicit initialization");
    unchanged("future schema protection neither migrates nor erases unknown saved work");
    memset(nor, 255, sizeof nor); snapshot(); jrt_boot();
    check(song.g[G_BPM] == 88 && project_slots_zero() && !arrangement_enabled &&
          ui.jam_home_view == JAM_HOME_GROOVE && !song.playing && !song.rec,
          "missing recovery starts the ordinary stopped and playable Dusty jam");
    unchanged("missing recovery does not initialize flash automatically");
}
static int jrt_cold_child(const char *path)
{
    memset(nor, 255, sizeof nor); jrt_boot(); jrt_seed_session(1); jrt_capture(&jrt_old);
    FILE *f = fopen(path, "rb");
    if (!f || fread(nor, 1, sizeof nor, f) != sizeof nor) return 2;
    fclose(f); snapshot(); jrt_boot(); jrt_check_session(&jrt_old);
    unchanged("new operating-system process recovers using only the NOR image");
    return failures ? 1 : 0;
}
static void jrt_test_fresh_process(const char *executable)
{
    char path[] = "/tmp/jambox-runtime-cold-XXXXXX";
    int fd = mkstemp(path);
    check(fd >= 0, "fresh-process recovery image has a private temporary path");
    if (fd < 0) return;
    FILE *f = fdopen(fd, "wb");
    int written = f && fwrite(jrt_saved_flash, 1, sizeof nor, f) == sizeof nor;
    if (f) fclose(f); else close(fd);
    check(written, "fresh-process fixture contains only the committed NOR image");
    if (written) {
        fflush(NULL);
        pid_t child = fork();
        if (child == 0) { execl(executable, executable, "--cold", path, (char *)NULL); _exit(127); }
        int status = 0;
        check(child > 0 && waitpid(child, &status, 0) == child &&
              WIFEXITED(status) && WEXITSTATUS(status) == 0,
              "actual adapter restores the full session after independent-process cold boot");
    }
    unlink(path);
}
int main(int argc, char **argv)
{
    if (argc == 3 && !strcmp(argv[1], "--cold")) return jrt_cold_child(argv[2]);
    jrt_test_sidecars(); jrt_test_write_gates(); jrt_test_save_boot(); jrt_test_late_read_failures();
    jrt_test_transient_component_corruption();
    jrt_test_transfer_arrives_during_save();
    jrt_test_busy_states(); jrt_test_editor_dirty(); jrt_test_failed_boot_guard();
    jrt_test_invalid_source_retry(); jrt_test_pending_sound_changes();
    jrt_test_fallback(); jrt_test_fresh_process(argv[0]);
    printf("JamBox real recovery runtime: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
