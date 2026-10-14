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

int main(int argc, char **argv)
{
    uint32_t k, old_index, old_track, before, macro_before, foot_before;
    int16_t bpm;
    step_t patterns[NTRK][NSTEP];
    home_setup();
    if (argc > 1) outdir = argv[1];
    check(ui.home && jam_home_active(), "startup uses the JamBox Home");
    check(song.sel == 1 && jam_home_role(song.sel) == 2, "startup selects the chord role");
    if (outdir) ppm("jam-home-chords");

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

    for (k = 0; k < 4u; k++) {
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
    check(ui.home && !vis_on, "HOME returns to overview instead of opening the visualizer");
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
    if (outdir) ppm("jam-home-playing");
    tap(B_PLAY); frame();
    check(!song.playing, "PLAY stops the shared transport from Home");

    track_select(1); trk[1].p[P_MUTE] = 1; song.rec = 1u << 1;
    ui.force = 1; frame();
    if (outdir) ppm("jam-home-muted-armed");
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
    before = strip_hash(76, 44);
    apply_preset_to(&trk[1], 0);
    ui.force = 0; frame();
    check(strcmp(jam_sound_name(1), "DUST KEYS") != 0 &&
          strcmp(jam_sound_name(1), ENGINES[0]->name) == 0,
          "an upstream ANALOG preset no longer carries the curated name");
    check(before != strip_hash(76, 44), "the sound title refreshes without a forced redraw");
    turn(EN_PRESET, 1);
    check(strcmp(jam_sound_name(1), "SOFT STABS") == 0,
          "curated browsing restores its own sound label after a deep preset load");

    /* All-zero macros expose a missing availability cache key: only their
     * editability changes on engine switch. Verify the rendered strips. */
    for (k = 0; k < 4u; k++) jam_macro_set(1, k, 0);
    ui.hot_t = ui.msg_t = 0; ui.force = 1; frame();
    macro_before = strip_hash(124, 80); foot_before = strip_hash(208, 32);
    set_engine_of(&trk[1], 1);
    ui.force = 0; frame();
    check(!jam_home_macros_available(1), "other engines direct editing to their real sound pages");
    check(macro_before != strip_hash(124, 80), "zero-valued macros redraw as unavailable without forcing");
    check(foot_before != strip_hash(208, 32), "the deep-edit footer hint refreshes without forcing");
    if (outdir) ppm("jam-home-other-engine");

    jam_sound_load(1, 0);
    ui_message("READY"); ui.force = 1; frame();
    foot_before = strip_hash(208, 32);
    set_engine_of(&trk[1], 1);
    ui.force = 0; frame();
    check(ui.msg_t && foot_before != strip_hash(208, 32),
          "the engine hint refreshes while the first footer line shows a message");
    puts(fails ? "jam-ui: FAILED" : "jam-ui: all checks passed");
    return fails ? 1 : 0;
}
