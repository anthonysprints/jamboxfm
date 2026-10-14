/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 JamBoxFM contributors */
/* JamBox Home through the real UI input/draw paths. The inherited harness
 * asserts every LCD transfer is inside 240x240 and runs audio between frames.
 * Usage: jam_ui_test OUTPUT_DIRECTORY (optional PPM screenshots). */
#define JAMBOX_PROTO 1
#define UI_PAGES_HARNESS_ONLY 1
#include "ui_pages_test.c"

static void turn(uint32_t role, int32_t steps)
{
    encs[panel.enc[role]] += steps * panel.dir[role];
    frame();
}

static void home_setup(void)
{
    memset(&ui, 0, sizeof ui);
    memset(encs, 0, sizeof encs);
    panel = PANEL_DEFAULT;
    layers_init();
    palette_set(1);
    host_tracks_init();
    check(jam_new_dusty(), "the stopped Dusty session initializes");
    go_home();
    frame();
}

static uint32_t strip_hash(uint32_t y, uint32_t h)
{
    uint32_t i, hash = 2166136261u;
    for (i = y * 240u; i < (y + h) * 240u; i++)
        hash = (hash ^ screen[i]) * 16777619u;
    return hash;
}

static void home_ppm(uint32_t view, const char *name)
{
    char path[80];
    if (!outdir) return;
    snprintf(path, sizeof path, "jam-%s-%s", view == JAM_HOME_SOUND ? "sound" : "groove", name);
    ppm(path);
}

static void test_home_controls(uint32_t view)
{
    uint32_t k, old_index, old_track, before, macro_before, foot_before;
    uint32_t sound_y = view == JAM_HOME_SOUND ? 76u : 144u;
    uint32_t sound_h = view == JAM_HOME_SOUND ? 44u : 32u;
    uint32_t macro_y = view == JAM_HOME_SOUND ? 124u : 176u;
    uint32_t macro_h = view == JAM_HOME_SOUND ? 80u : 40u;
    uint32_t foot_y = view == JAM_HOME_SOUND ? 208u : 216u;
    uint32_t foot_h = view == JAM_HOME_SOUND ? 32u : 24u;
    int16_t bpm;
    step_t patterns[NTRK][NSTEP];
    home_setup();
    check(ui.jam_home_view == JAM_HOME_GROOVE, "the Home view starts on GROOVE");
    if (view == JAM_HOME_SOUND) tap(B_HOME);
    check(ui.jam_home_view == view, "both Home views are reachable with the physical HOME button");
    check(ui.home && jam_home_active(), "startup uses the JamBox Home");
    check(song.sel == 1 && jam_home_role(song.sel) == 2, "startup selects the chord role");
    home_ppm(view, "chords");

    bpm = song.g[G_BPM];
    turn(EN_ALGO, 1);
    check(song.sel == 2 && song.g[G_BPM] == bpm, "ALGORITHM chooses Lead without changing tempo");
    turn(EN_ALGO, -20);
    check(song.sel == TRK_DRUM, "ALGORITHM clamps to Drums in display order");
    turn(EN_ALGO, 1);
    check(song.sel == 0, "the next role after Drums is Bass");
    for (k = 0; k < NTRK; k++) memcpy(patterns[k], trk[k].step, sizeof patterns[k]);
    old_index = jam_sound_index(song.sel);
    turn(EN_PRESET, 1);
    check(jam_sound_index(song.sel) != old_index, "PRESETS browses the selected part's curated sounds");
    before = 1;
    for (k = 0; k < NTRK; k++) before &= !memcmp(patterns[k], trk[k].step, sizeof patterns[k]);
    check(before, "curated sound browsing preserves all four patterns");
    old_index = jam_sound_index(song.sel); old_track = song.sel;
    before = strip_hash(0, 36);
    turn(EN_SELECT, 1);
    check(song.g[G_BPM] == bpm + 1 && song.sel == old_track && jam_sound_index(song.sel) == old_index,
          "SELECT raises tempo one BPM without changing part or sound");
    check(before != strip_hash(0, 36), "the Home BPM display follows the tempo knob");
    turn(EN_SELECT, -1);
    check(song.g[G_BPM] == bpm, "SELECT lowers tempo one BPM");
    turn(EN_SELECT, 1000);
    check(song.g[G_BPM] == GP[G_BPM].max, "tempo clamps at 240 BPM");
    turn(EN_SELECT, -1000);
    check(song.g[G_BPM] == GP[G_BPM].min, "tempo clamps at 40 BPM");
    song.g[G_BPM] = bpm;
    before = 1;
    for (k = 0; k < NTRK; k++) before &= !memcmp(patterns[k], trk[k].step, sizeof patterns[k]);
    check(before, "tempo changes preserve all four recorded patterns");

    if (view == JAM_HOME_SOUND) for (k = 0; k < 4u; k++) {
        jam_macro_set(song.sel, k, 0);
        turn(EN_K1 + k, 1);
        check(jam_macro_get(song.sel, k) > 0, "one detent moves even a narrow-range macro");
        turn(EN_K1 + k, 1000);
        check(jam_macro_get(song.sel, k) == 100, "macro turns clamp at 100");
        turn(EN_K1 + k, -1000);
        check(jam_macro_get(song.sel, k) == 0, "macro turns clamp at zero");
    }
    check(song.g[G_BPM] == bpm, "the four sound macros leave tempo alone");

    tap(B_ENV);
    check(!ui.home && cur_page()->fam == FAM_ENV, "ENV still reaches the deep envelope page");
    tap(B_HOME);
    check(ui.home && ui.jam_home_view == view && !vis_on, "HOME restores the remembered view from a deep page");
    tap(B_EDIT);
    check(!ui.home && cur_page()->fam == FAM_EDIT, "EDIT tap still reaches deep sound controls");
    tap(B_HOME);
    tap(B_SCL);
    check(!ui.home && cur_page()->fam == FAM_SCL, "SEL opens the real chord/scale pages");
    tap(B_HOME);
    tap(B_FX);
    check(!ui.home && cur_page()->fam == FAM_FX, "FX tap still reaches effect controls");
    tap(B_HOME);

    /* The narrow hook must not read controls owned by a recorder/layer/menu. */
    rec_wait = 1;
    check(!jam_home_input() && !jam_home_draw(), "the armed recorder owns input and display");
    rec_wait = 0; ft_on = 1;
    check(!jam_home_input(), "free capture retains its encoder handling");
    ft_on = 0; ci_on = 1;
    check(!jam_home_input(), "count-in retains its encoder handling");
    ci_on = 0; ui.layer = LY_FX;
    check(!jam_home_input() && !jam_home_draw(), "a held FX layer retains input and display");
    ui.layer = LY_PLAY; ui.menu = 1;
    check(!jam_home_input() && !jam_home_draw(), "the system menu retains input and display");
    ui.menu = 0; ui.hold_kind = 1;
    check(!jam_home_input(), "a clear/save confirmation retains its input");
    ui.hold_kind = 0;

    /* Real transport buttons still execute before the Home encoder hook. */
    tap(B_PLAY); frame();
    check(song.playing, "PLAY starts the shared transport from Home");
    turn(EN_SELECT, 1);
    check(song.playing && song.g[G_BPM] == bpm + 1, "tempo changes while the groove keeps playing");
    turn(EN_SELECT, -1);
    frames(12);
    home_ppm(view, "playing");
    tap(B_PLAY); frame();
    check(!song.playing, "PLAY stops the shared transport from Home");

    track_select(1); trk[1].p[P_MUTE] = 1; song.rec = 1u << 1;
    ui.force = 1; frame();
    home_ppm(view, "muted-armed");
    song.rec = 0; trk[1].p[P_MUTE] = 0;
    for (k = 0; k < 4u; k++) {
        track_select(jam_role_track(k));
        ui.force = 1;
        frame();
    }
    /* A real upstream preset load must invalidate a curated starting label,
     * including when the engine remains ANALOG. Home then browses back in. */
    track_select(1);
    jam_sound_load(1, 0); ui.force = 1; frame();
    before = strip_hash(sound_y, sound_h);
    apply_preset_to(&trk[1], 0);
    ui.force = 0; frame();
    check(strcmp(jam_sound_name(1), "DUST KEYS") != 0 &&
          strcmp(jam_sound_name(1), ENGINES[0]->name) == 0,
          "an upstream ANALOG preset no longer carries the curated name");
    check(before != strip_hash(sound_y, sound_h), "the sound title refreshes without a forced redraw");
    turn(EN_PRESET, 1);
    check(strcmp(jam_sound_name(1), "SOFT STABS") == 0,
          "curated browsing restores its own sound label after a deep preset load");

    /* All-zero macros expose a missing availability cache key: only their
     * editability changes on engine switch. Verify the rendered strips. */
    for (k = 0; k < 4u; k++) jam_macro_set(1, k, 0);
    ui.hot_t = ui.msg_t = 0; ui.force = 1; frame();
    macro_before = strip_hash(macro_y, macro_h); foot_before = strip_hash(foot_y, foot_h);
    set_engine_of(&trk[1], ENGI_NOISE);
    ui.force = 0; frame();
    check(!jam_home_macros_available(1), "other engines direct editing to their real sound pages");
    check((macro_before != strip_hash(macro_y, macro_h)) == (view == JAM_HOME_SOUND),
          "engine availability changes Sound macros but leaves Groove mixer controls available");
    check((foot_before != strip_hash(foot_y, foot_h)) == (view == JAM_HOME_SOUND),
          "only Sound needs an unavailable-engine edit hint");
    home_ppm(view, "other-engine");

    jam_sound_load(1, 0);
    ui_message("READY"); ui.force = 1; frame();
    foot_before = strip_hash(foot_y, foot_h); macro_before = strip_hash(macro_y, macro_h);
    set_engine_of(&trk[1], ENGI_NOISE);
    ui.force = 0; frame();
    if (view == JAM_HOME_SOUND)
        check(ui.msg_t && foot_before != strip_hash(foot_y, foot_h),
              "the engine hint refreshes while the first footer line shows a message");
    else
        check(ui.msg_t && foot_before == strip_hash(foot_y, foot_h) && macro_before == strip_hash(macro_y, macro_h),
              "Groove retains its controls and status message for every engine");
}

static uint32_t rect_hash(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    uint32_t a, b, hash = 2166136261u;
    for (b = y; b < y + h; b++)
        for (a = x; a < x + w; a++) hash = (hash ^ screen[b * 240u + a]) * 16777619u;
    return hash;
}
static void draw_home(void)
{
    check(jam_home_draw(), "the ordinary Home renderer owns this frame");
    ui.force = 0;
}
static void note_step(track_t *t, uint32_t i)
{
    if (is_drum(t)) dstep_set(&t->dstep[i], 0, LV_NORM, 0);
    else {
        t->step[i].time = ST_NOTE; t->step[i].n = 1; t->step[i].note[0] = 60;
    }
}
static void test_groove_rows(void)
{
    static const uint8_t lengths[] = {1,15,16,17,32,64};
    static const char *const ranges[] = {"01-01/01","01-15/15","01-16/16","17-17/17","17-32/32","49-64/64"};
    uint32_t a, i, r, j, before, m_before, s_before, r_before, saved_bank;
    const uint32_t chord_y = 32u + 2u * 28u;
    char range[9];
    home_setup();
    ui.msg_t = ui.hot_t = 0;
    for (a = 0; a < sizeof lengths / sizeof lengths[0]; a++) {
        uint32_t len = lengths[a], last = len - 1u;
        for (i = 0; i < NTRK; i++) {
            steps_clear(&trk[i]); trk[i].p[P_SLEN] = len;
            trk[i].p[P_SDIV] = 2; trk[i].seq_idx = last;
            note_step(&trk[i], last);
            if (len > 2 && i != TRK_DRUM) trk[i].step[1].time = ST_TIE;
        }
        song.playing = 1; draw_home();
        for (r = 0; r < NTRK; r++) {
            uint32_t tno = jam_role_track(r), y = 32u + r * 28u;
            check(jam_groove_cache.bank[tno] == last / 16u, "each row follows its literal 16-step playback window");
            jam_groove_range(range, jam_groove_cache.bank[tno], len);
            check(!strcmp(range, ranges[a]), "row range labels represent exact steps and total length");
            check(screen[(y + 25u) * 240u + 8u + last % 16u * 14u] == swap16(C_WHITE),
                  "the playhead marks the final actual step, including a partial window");
            check(jam_groove_cell(&trk[tno], last, len) == JAM_CELL_NOTE &&
                  jam_groove_cell(&trk[tno], len, len) == JAM_CELL_DISABLED,
                  "stored final steps remain visible and out-of-length cells are disabled");
            for (j = last % 16u + 1u; j < 16u; j++)
                check(screen[(y + 20u) * 240u + 8u + j * 14u] == swap16(C_BLACK),
                      "partial final windows do not invent extra stored steps");
        }
        song.playing = 0; draw_home();
        for (r = 0; r < NTRK; r++) {
            uint32_t tno = jam_role_track(r), y = 32u + r * 28u;
            check(jam_groove_cache.bank[tno] == last / 16u, "stopping retains each row's last displayed window");
            check(screen[(y + 25u) * 240u + 8u + last % 16u * 14u] == swap16(C_BLACK),
                  "stopped rows have no fake playhead");
        }
    }
    /* Browsing sounds and ordinary edits do not reset the independent overview. */
    saved_bank = jam_groove_cache.bank[1];
    check(saved_bank == 3, "the 64-step fixture ends in its fourth display window");
    check(jam_sound_load(1, 1), "a different curated sound can be loaded"); ui.force = 1; draw_home();
    check(jam_groove_cache.bank[1] == saved_bank, "sound browsing preserves the stopped pattern window");
    trk[1].step[48].time = ST_NOTE; trk[1].step[48].n = 1; trk[1].step[48].note[0] = 65;
    draw_home(); check(jam_groove_cache.bank[1] == saved_bank, "ordinary note edits preserve the stopped window");
    trk[1].p[P_SLEN] = 17; draw_home();
    check(jam_groove_cache.bank[1] == 1, "shortening a pattern clamps its stopped window");
    steps_clear(&trk[1]); draw_home();
    check(jam_groove_cache.bank[1] == 0 && jam_groove_cache.bank[2] == 3,
          "replacing one pattern resets only its display window");

    for (i = 0; i < NTRK; i++) {
        steps_clear(&trk[i]); trk[i].p[P_SLEN] = 16; note_step(&trk[i],0);
        trk[i].p[P_MUTE] = 0; trk[i].p[P_LEVEL] = 94; trk[i].p[P_SDIV] = 2;
    }
    track_select(1); song.solo = song.rec = 0; draw_home();
    before = strip_hash(chord_y,28);
    trk[1].step[0].time = ST_TIE; draw_home();
    check(before != strip_hash(chord_y,28) && jam_groove_cell(&trk[1],0,16) == JAM_CELL_TIE,
          "changing a note to a tie changes its distinct stored-step mark");
    check(screen[(chord_y + 17u) * 240u + 8u] == swap16(RGB(26,26,30)) &&
          screen[(chord_y + 20u) * 240u + 8u] != swap16(RGB(26,26,30)),
          "a tie draws a line instead of a filled note cell");
    before = strip_hash(chord_y,28); trk[1].step[0].time = ST_REST; draw_home();
    check(before != strip_hash(chord_y,28), "changing a tie to empty invalidates the row cache");
    note_step(&trk[1],0); draw_home();
    before = rect_hash(136,chord_y,32,16); trk[1].p[P_SDIV] = 7; draw_home();
    check(before != rect_hash(136,chord_y,32,16), "non-sixteenth divisions are explicitly labelled");
    m_before = rect_hash(176,chord_y,8,16);
    s_before = rect_hash(192,chord_y,8,16);
    r_before = rect_hash(208,chord_y,8,16);
    trk[1].p[P_MUTE] = 1; song.solo = song.rec = 1u << 1; draw_home();
    check(m_before != rect_hash(176,chord_y,8,16) && s_before != rect_hash(192,chord_y,8,16) &&
          r_before != rect_hash(208,chord_y,8,16), "mute, solo and recording each have a simultaneous indicator");
    check(screen[(chord_y + 18u) * 240u + 8u] == swap16(C_DIM), "stored cells remain visible while muted");
    check(screen[(32u + 28u + 18u) * 240u + 8u] == swap16(C_DIM) &&
          rect_hash(176,32u+28u,8,16) == m_before,
          "solo-excluded rows are dimmed without a false manual-mute mark");
    trk[1].p[P_MUTE] = 0; song.solo = song.rec = 0; trk[1].p[P_LEVEL] = 0; draw_home();
    check(screen[(chord_y + 18u) * 240u + 8u] == swap16(C_DIM) &&
          rect_hash(176,chord_y,8,16) == m_before, "zero-level rows are dimmed without a false manual-mute mark");
    trk[1].p[P_LEVEL] = 94; draw_home();
    /* Put a marker in unused row padding. An unchanged cached row must leave it alone. */
    screen[(chord_y + 27u) * 240u + 239u] = swap16(0x1234);
    draw_home();
    check(screen[(chord_y + 27u) * 240u + 239u] == swap16(0x1234), "unchanged rows do not redraw every UI frame");
    trk[1].step[0].time = ST_TIE; draw_home();
    check(screen[(chord_y + 27u) * 240u + 239u] == swap16(C_BLACK), "a visible row change redraws its bounded strip");

    /* One screenshot demonstrates independent rows, divisions, ties and status. */
    trk[TRK_DRUM].p[P_SLEN] = 16; trk[TRK_DRUM].seq_idx = 5;
    trk[0].p[P_SLEN] = 15; trk[0].seq_idx = 14; trk[0].p[P_MUTE] = 1;
    trk[1].p[P_SLEN] = 32; trk[1].seq_idx = 23; trk[1].p[P_SDIV] = 2;
    note_step(&trk[1],16); trk[1].step[17].time = ST_TIE;
    trk[2].p[P_SLEN] = 64; trk[2].seq_idx = 49; trk[2].p[P_SDIV] = 1; note_step(&trk[2],48);
    song.playing = 1; song.rec = 1u << 1; draw_home(); home_ppm(JAM_HOME_GROOVE,"mixed-lengths");
    check(jam_groove_cache.bank[TRK_DRUM] == 0 && jam_groove_cache.bank[0] == 0 &&
          jam_groove_cache.bank[1] == 1 && jam_groove_cache.bank[2] == 3,
          "unequal track lengths use their own actual playhead windows");
    song.playing = song.rec = 0; ui.msg_t = ui.hot_t = 0; draw_home();
    before = strip_hash(0,240);
    ui.jam_home_view = JAM_HOME_SOUND; draw_home(); home_ppm(JAM_HOME_SOUND,"overview-return");
    ui.jam_home_view = JAM_HOME_GROOVE; draw_home();
    check(before == strip_hash(0,240), "a view round trip restores every Groove pixel without stale Sound fragments");
    before = strip_hash(0,240); ui.force = 1; draw_home();
    check(before == strip_hash(0,240), "forced redraw matches the incrementally cached Groove frame");
    ui.jam_home_view = JAM_HOME_SOUND;
    check(jam_new_dusty(), "a fresh musical session can replace these patterns");
    check(ui.jam_home_view == JAM_HOME_SOUND, "a fresh musical session preserves the remembered Home view");
    ui.jam_home_view = JAM_HOME_GROOVE; draw_home();
    check(jam_groove_cache.bank[0] == 0 && jam_groove_cache.bank[1] == 0 &&
          jam_groove_cache.bank[2] == 0 && jam_groove_cache.bank[TRK_DRUM] == 0,
          "a replaced session resets all remembered pattern windows");
}

static void test_groove_controls(void)
{
    uint32_t r, k, j, before, old_macro[4];
    step_t patterns[NTRK][NSTEP];
    int16_t params[NTRK][P_COUNT];
    char text[24];
    home_setup();
    for (r = 0; r < NTRK; r++) {
        track_select(jam_role_track(r));
        for (k = 0; k < 4; k++) old_macro[k] = jam_macro_get(song.sel,k);
        for (j = 0; j < NTRK; j++) {
            memcpy(patterns[j], trk[j].step, sizeof patterns[j]);
            memcpy(params[j], trk[j].p, sizeof params[j]);
        }
        song.g[G_SWING] = 0;
        turn(EN_K1, 1);
        check(song.g[G_SWING] == 1, "Groove SWING changes the shared sequencer swing one unit per detent");
        for (j = 0; j < NTRK; j++)
            check(!memcmp(params[j], trk[j].p, sizeof params[j]), "global swing leaves all part parameters intact");
        TSEL->p[P_MUTE] = 1;
        *jam_groove_parameter(song.sel,1) = 63;
        turn(EN_K2, 1);
        check(jam_groove_value(song.sel,1) == 64 && TSEL->p[P_MUTE] == 1,
              "Groove LEVEL uses the correct synth/drum level and retains explicit mute");
        TSEL->p[P_SLEN] = 32;
        turn(EN_K3, -1);
        check(TSEL->p[P_SLEN] == 31, "STEPS moves one actual step with no encoder acceleration");
        turn(EN_K3, 1);
        check(TSEL->p[P_SLEN] == 32, "extending the loop restores its former length");
        for (j = 0; j < NTRK; j++)
            check(!memcmp(patterns[j], trk[j].step, sizeof patterns[j]), "length changes preserve every stored note including hidden steps");
        TSEL->p[P_PAN] = 0; turn(EN_K4, -1);
        check(TSEL->p[P_PAN] == -1, "Groove PAN changes only the selected part");
        for (j = 0; j < NTRK; j++) if (j != song.sel) {
            check(trk[j].p[P_PAN] == params[j][P_PAN] && trk[j].p[P_LEVEL] == params[j][P_LEVEL] &&
                  trk[j].p[P_SLEN] == params[j][P_SLEN], "other parts retain their pan, level and length");
        }
        for (k = 0; k < 4; k++) {
            if (song.sel == TRK_DRUM && k == 1u)
                check(jam_macro_get(song.sel,k) == jam_percent(song.g[G_DRLVL],0,110),
                      "drum LEVEL shares its existing mixer gain with the Sound view");
            else
                check(jam_macro_get(song.sel,k) == old_macro[k], "Groove controls preserve timbre and effect macros");
            turn(EN_K1+k, -1000);
            check(jam_groove_value(song.sel,k) == (k == 2 ? 1 : k == 3 ? -64 : 0), "Groove controls clamp at their lower limit");
            turn(EN_K1+k, 1000);
            check(jam_groove_value(song.sel,k) == (k == 0 ? 100 : k == 1 ? 127 : k == 2 ? 64 : 63), "Groove controls clamp at their upper limit");
        }
        jam_groove_text(text,song.sel,3); check(!strcmp(text,"R100"), "pan shows its right endpoint");
        TSEL->p[P_PAN] = -64; jam_groove_text(text,song.sel,3); check(!strcmp(text,"L100"), "pan shows its left endpoint");
        TSEL->p[P_PAN] = 0; jam_groove_text(text,song.sel,3); check(!strcmp(text,"CENTER"), "pan zero reads CENTER");
        ui.hot_t = 0; ui.force = 1; frame(); before = strip_hash(176,40);
        TSEL->p[P_PAN] = 32; frame();
        check(before != strip_hash(176,40), "parameter changes invalidate the cached Groove control strip");
    }
}

static void test_expanded_bank_controls(void)
{
    uint32_t track, index, macro, count, before;
    char position[20], expected[20];
    home_setup();
    tap(B_HOME);
    for (track = 0; track < NTRK; track++) {
        track_select(track);
        count = jam_sound_count(track);
        check(count == (track == TRK_DRUM ? 8u : 12u), "the expanded bank is reachable on every role");
        for (index = 0; index < count; index++) {
            check(jam_sound_load(track, index), "every curated preset loads through the shared sound API");
            frame();
            jam_home_preset_position(position, track);
            snprintf(expected, sizeof expected, "%u/%u", index + 1u, count);
            check(!strcmp(position, expected), "Home shows the current preset's actual position");
            check(jam_home_macros_available(track), "every curated engine exposes useful Sound controls");
            for (macro = 0; macro < 4u; macro++) {
                jam_macro_set(track, macro, 0);
                turn(EN_K1 + macro, 1);
                check(jam_macro_get(track, macro) > 0,
                      "one physical detent moves every macro, including coarse organ speed");
            }
        }
        turn(EN_PRESET, 1);
        check(jam_sound_index(track) == 0, "forward browsing wraps from the last sound to the first");
        turn(EN_PRESET, -1);
        check(jam_sound_index(track) == count - 1u, "reverse browsing wraps through the complete bank");
        check(song.sel == track && song.g[G_BPM] == 88, "browsing never changes the selected role or tempo");
    }
    track_select(1);
    jam_sound_load(1, 0); frame();
    before = strip_hash(40, 32);
    turn(EN_PRESET, 1);
    check(before != strip_hash(40, 32), "the Sound preset position refreshes when browsing");
    set_engine_of(&trk[1], ENGI_NOISE); frame();
    jam_home_preset_position(position, 1);
    check(!strcmp(position, "--/12"), "a deep sound outside the curated bank never claims a false preset position");
}

int main(int argc, char **argv)
{
    if (argc > 1) outdir = argv[1];
    test_home_controls(JAM_HOME_GROOVE);
    test_home_controls(JAM_HOME_SOUND);
    test_groove_rows();
    test_groove_controls();
    test_expanded_bank_controls();
    puts(fails ? "jam-ui: FAILED" : "jam-ui: all checks passed");
    return fails ? 1 : 0;
}
