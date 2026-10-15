/* SPDX-License-Identifier: GPL-3.0-only */
/* The real panel/audio/LCD harness and navigation fixture are shared so these
 * controls are exercised with the same gesture ownership and recovery guards.
 * No flash writer or device connection is used. */
#define JAM_DRUM_NAVIGATION_HARNESS_ONLY 1
#include "jam_drum_navigation_test.c"

typedef union {
    step_t step[NSTEP];
    dstep_t drum[NSTEP];
} saved_steps_t;
typedef struct {
    saved_steps_t notes[NTRK];
    int8_t micro[NTRK][NSTEP];
    plock_t lock[NTRK][NLOCK];
    uint8_t fill[NTRK][NSTEP / 4];
    uint8_t durations[NTRK][NSTEP][4];
    uint8_t duration_mode[NTRK];
} beat_t;

static void beat_read(beat_t *b)
{
    unsigned t;
    for (t = 0; t < NTRK; t++) {
        memcpy(b->notes[t].step, trk[t].step, sizeof b->notes[t].step);
        memcpy(b->micro[t], trk[t].micro, sizeof b->micro[t]);
        memcpy(b->lock[t], trk[t].lock, sizeof b->lock[t]);
        memcpy(b->fill[t], trk[t].fill, sizeof b->fill[t]);
        memcpy(b->durations[t], trk[t].note_len, sizeof b->durations[t]);
        b->duration_mode[t] = trk[t].duration_mode;
    }
}

static int beat_equal(const beat_t *a, const beat_t *b)
{
    return !memcmp(a->notes, b->notes, sizeof a->notes) &&
           !memcmp(a->micro, b->micro, sizeof a->micro) &&
           !memcmp(a->lock, b->lock, sizeof a->lock) &&
           !memcmp(a->fill, b->fill, sizeof a->fill) &&
           !memcmp(a->durations, b->durations, sizeof a->durations) &&
           !memcmp(a->duration_mode, b->duration_mode, sizeof a->duration_mode);
}

static void expect_beat(const beat_t *expected, const char *what)
{
    beat_t actual;
    beat_read(&actual);
    expect(beat_equal(expected, &actual), what);
}

static void grid_setup(unsigned len)
{
    setup(JAM_HOME_GROOVE);
    TDRUM->p[P_SLEN] = (int16_t)len;
    memset(&undo, 0, sizeof undo);
    tap(B_SEQ);
    frame();
}

static void undo_gesture(unsigned redo)
{
    press(B_EDIT);
    tap(redo ? B_OCTUP : B_OCTDN);
    release(B_EDIT);
    frames(20);                              /* inherited release-time knob quiet period */
}

static void test_cell_controls(void)
{
    beat_t original, expected, undo_before;
    unsigned rank;
    static const uint8_t levels[] = {LV_GHOST, LV_SOFT, LV_NORM, LV_HARD};
    grid_setup(32);
    turn(EN_K1, 11);
    turn(EN_K2, 19);
    dstep_clr(&TDRUM->dstep[19], 11);
    beat_read(&original);
    turn(EN_K3, 3);
    turn(EN_K4, 3);
    expect_beat(&original, "Strength/Repeat on an empty cell create no hits or metadata");
    expect(!undo.valid && !song.playing && !song.rec && !rec_wait && !recovery_calls,
           "empty-cell knobs neither replace undo nor start transport/recording/saving");

    /* A simultaneous second drum and adjacent cells catch packed-bit damage. */
    dstep_set(&TDRUM->dstep[19], 11, LV_NORM, 0);
    dstep_set(&TDRUM->dstep[19], 10, LV_HARD, 2);
    dstep_set(&TDRUM->dstep[18], 11, LV_SOFT, 3);
    TDRUM->micro[19] = -7;
    step_fill_set(TDRUM, 19, FC_FILL);
    lock_set(TDRUM, 19, P_PAN, -12);
    beat_read(&original);
    expected = original;
    turn(EN_K3, 1);
    dstep_set(&expected.notes[TRK_DRUM].drum[19], 11, LV_HARD, 0);
    expect_beat(&expected, "Strength changes only the focused drum hit and preserves its neighbors");
    turn(EN_K3, 20);
    expect_beat(&expected, "Strength clamps at hard without rewriting timing or other lanes");
    turn(EN_K3, -3);
    dstep_set(&expected.notes[TRK_DRUM].drum[19], 11, LV_GHOST, 0);
    expect_beat(&expected, "three Strength detents move hard to ghost in musical order");
    for (rank = 1; rank < NELEM(levels); rank++) {
        turn(EN_K3, 1);
        dstep_set(&expected.notes[TRK_DRUM].drum[19], 11, levels[rank], 0);
        expect_beat(&expected, "each Strength detent advances ghost/soft/normal/hard");
    }
    turn(EN_K4, 3);
    dstep_set(&expected.notes[TRK_DRUM].drum[19], 11, LV_HARD, 3);
    expect_beat(&expected, "Repeat adds up to four hits within only the focused drum cell");
    turn(EN_K4, 20);
    expect_beat(&expected, "Repeat clamps at four and preserves Strength and other drums");
    undo_gesture(0);
    dstep_set(&expected.notes[TRK_DRUM].drum[19], 11, LV_HARD, 0);
    expect_beat(&expected, "Repeat at its end stop preserves the preceding meaningful undo");
    undo_gesture(1);
    dstep_set(&expected.notes[TRK_DRUM].drum[19], 11, LV_HARD, 3);
    expect_beat(&expected, "redo restores Repeat after its end-stop no-op");
    beat_read(&undo_before);
    turn(EN_K4, -2);
    dstep_set(&expected.notes[TRK_DRUM].drum[19], 11, LV_HARD, 1);
    expect_beat(&expected, "Repeat counts every encoder detent without changing Strength");
    turn(EN_K3, 1);                         /* already hard: preserve Repeat undo */
    undo_gesture(0);
    expect_beat(&undo_before, "EDIT plus OCT-minus undoes the last meaningful grid knob turn");
    undo_gesture(1);
    expect_beat(&expected, "EDIT plus OCT-plus redoes the exact scoped grid edits");

    /* Empty controls must not consume a useful undo from another step. */
    turn(EN_K2, 1);
    dstep_clr(&TDRUM->dstep[20], 11);
    turn(EN_K3, -1);
    turn(EN_K4, 1);
    undo_gesture(0);
    dstep_clr(&undo_before.notes[TRK_DRUM].drum[20], 11);
    expect_beat(&undo_before, "empty-cell knob turns preserve the last meaningful grid undo");
}

static void test_kit_and_tempo(void)
{
    unsigned kit_page, i, start, count;
    beat_t before;
    int16_t params[NTRK][P_COUNT], globals[G_COUNT];
    for (kit_page = 0; kit_page < 2; kit_page++) {
        grid_setup(32);
        if (kit_page) tap(B_EDIT);
        TDRUM->p[P_PAN] = -17;
        TDRUM->p[P_MUTE] = 1;
        song.g[G_DRLVL] = 79;
        song.g[G_DRREV] = 35;
        song.g[G_DRDLY] = 21;
        song.g[G_SWING] = 27;
        TDRUM->micro[4] = 9;
        step_fill_set(TDRUM, 4, FC_FILL);
        lock_set(TDRUM, 4, P_PAN, 16);
        beat_read(&before);
        for (i = 0; i < NTRK; i++) memcpy(params[i], trk[i].p, sizeof params[i]);
        memcpy(globals, song.g, sizeof globals);
        start = jam_sound_index(TRK_DRUM);
        count = jam_sound_count(TRK_DRUM);
        expect(count == 8, "ordinary drum workspace offers the eight curated Home kits");
        for (i = 1; i <= count; i++) {
            unsigned t, p;
            int same = 1;
            turn(EN_PRESET, 1);
            expect(jam_sound_index(TRK_DRUM) == (start + i) % count &&
                       jam_sound_is_curated(TRK_DRUM),
                   "PRESETS visits curated kits in the same wrapping order as Home");
            expect_beat(&before, "changing kits preserves every pattern, lock, nudge and fill");
            for (t = 0; t < NTRK; t++) for (p = 0; p < P_COUNT; p++)
                if (!(t == TRK_DRUM && p == P_E0) && trk[t].p[p] != params[t][p]) same = 0;
            expect(same && !memcmp(globals, song.g, sizeof globals),
                   "changing kits preserves all track mix/timing settings and shared globals");
        }
        turn(EN_PRESET, -1);
        expect(jam_sound_index(TRK_DRUM) == (start + count - 1) % count,
               "negative PRESETS detent wraps backward through curated kits");
        song.g[G_BPM] = 100;
        turn(EN_SELECT, 1);
        expect(song.g[G_BPM] == 101 && drum_page == kit_page,
               "SELECT adds exactly one BPM without switching GRID/KIT");
        turn(EN_SELECT, 3);
        expect(song.g[G_BPM] == 104 && drum_page == kit_page,
               "fast SELECT detents retain one BPM per detent in either drum workspace");
        turn(EN_SELECT, -2);
        expect(song.g[G_BPM] == 102 && drum_page == kit_page,
               "SELECT tempo also counts negative detents without acceleration");
        turn(EN_SELECT, 10000);
        expect(song.g[G_BPM] == GP[G_BPM].max, "SELECT clamps tempo at its supported maximum");
        turn(EN_SELECT, -10000);
        expect(song.g[G_BPM] == GP[G_BPM].min, "SELECT clamps tempo at its supported minimum");
        expect_beat(&before, "tempo turns leave all recorded notes and timing metadata intact");
        expect(!song.playing && !song.rec && !rec_wait && !recovery_calls,
               "kit/tempo browsing does not start recording, transport or flash saving");
    }
}

static void test_part_order_and_handoff(void)
{
    unsigned kit_page;
    for (kit_page = 0; kit_page < 2; kit_page++) {
        beat_t before;
        grid_setup(16);
        if (kit_page) tap(B_EDIT);
        beat_read(&before);
        turn(EN_ALGO, -1);
        expect(song.sel == TRK_DRUM && on_drum_page() && drum_page == kit_page,
               "ALGORITHM at first displayed part remains on Drums");
        turn(EN_ALGO, 1);
        expect(song.sel == jam_role_track(1) && ui.home && !kb_grid &&
                   ui.jam_home_view == JAM_HOME_GROOVE,
               "ALGORITHM advances Drums to Bass and immediately releases GRID ownership");
        turn(EN_ALGO, 1);
        expect(song.sel == jam_role_track(2), "next ALGORITHM detent selects Chords in display order");
        turn(EN_ALGO, 1);
        expect(song.sel == jam_role_track(3), "next ALGORITHM detent selects Lead in display order");
        turn(EN_ALGO, 1);
        expect(song.sel == jam_role_track(3), "ALGORITHM clamps at the last displayed part");
        turn(EN_ALGO, -3);
        expect(song.sel == TRK_DRUM, "batched negative ALGORITHM detents return Lead to Drums");
        expect_beat(&before, "part selection preserves every recorded pattern and drum cell");
    }
}

static void test_audition_and_selected_lane(void)
{
    beat_t before;
    grid_setup(32);
    beat_read(&before);
    aud_lanes = 0;
    turn(EN_K1, 12);
    expect(aud_lanes == (1u << 12) && drum_lane == 12 && pen_lane == 12,
           "Drum knob auditions exactly its new lane and shares held-SEQ selection");
    tap(B_PLAY);
    aud_lanes = 0;
    turn(EN_K2, 7);
    expect(song.playing && drum_cursor == 7 && !aud_lanes,
           "moving Step during playback focuses a cell without auditioning its whole beat");
    turn(EN_K1, 1);
    expect(aud_lanes == (1u << 13) && drum_lane == 13 && pen_lane == 13,
           "Drum selection remains a single-lane audition while playback runs");
    aud_lanes = 0;
    turn(EN_K2, 13);
    expect(drum_cursor == 20 && !aud_lanes,
           "Step crosses banks during playback without firing unrelated drum hits");
    expect_beat(&before, "auditioning/selecting drums and steps does not rewrite the beat");
    tap(B_PLAY);
    tap(B_HOME);
    key(key_of_lane(3));
    tap(B_SEQ);
    expect(drum_lane == 3 && pen_lane == 3 && kb_grid,
           "playing a pad on Home selects the same named lane when GRID is reopened");
    {
        unsigned idx = drum_cursor / 16u * 16u + 1u;
        dstep_clr(&TDRUM->dstep[idx], 3);
        dstep_clr(&TDRUM->dstep[idx], 13);
        press(B_SEQ);
        key(key_of_white(1));
        release(B_SEQ);
        expect(dstep_has(&TDRUM->dstep[idx], 3) && !dstep_has(&TDRUM->dstep[idx], 13),
               "held SEQ uses the shared drum lane and the current GRID step bank");
    }
}

static void test_guards_and_shrunk_length(void)
{
    beat_t before, expected;
    grid_setup(32);
    turn(EN_K1, 9);
    turn(EN_K2, 31);
    dstep_set(&TDRUM->dstep[31], 9, LV_HARD, 3);
    dstep_set(&TDRUM->dstep[5], 9, LV_SOFT, 1);
    TDRUM->p[P_SLEN] = 6;
    beat_read(&before);
    expected = before;
    turn(EN_K3, 1);
    dstep_set(&expected.notes[TRK_DRUM].drum[5], 9, LV_NORM, 1);
    expect(drum_cursor == 5, "pattern shortening clamps focus before the next knob is applied");
    expect_beat(&expected, "a knob after shortening edits the visible final cell and preserves hidden hits");
    turn(EN_K4, 1);
    dstep_set(&expected.notes[TRK_DRUM].drum[5], 9, LV_NORM, 2);
    expect_beat(&expected, "Repeat also targets the newly clamped visible cell after shortening");

    grid_setup(32);
    turn(EN_K1, 9);
    dstep_set(&TDRUM->dstep[0], 9, LV_NORM, 1);
    beat_read(&before);
    song.playing = 1;
    arrangement_enabled = 1;
    turn(EN_K3, 1);
    turn(EN_K4, 1);
    key(key_of_white(0));
    expect_beat(&before, "song-arrangement playback blocks grid knobs and step writes consistently");
    expect(ui.msg_t && !strcmp(ui.msg, "STOP THE SONG FIRST"),
           "blocked arrangement editing explains why the beat cannot be changed");
    arrangement_enabled = 0;
    tap(B_PLAY);
}

static void test_save_selection_and_held_release(void)
{
    beat_t before;
    grid_setup(32);
    turn(EN_K1, 14);
    recovery_clears_lane = 1;
    beat_read(&before);
    tap(B_SAVE);
    tap(B_SAVE);
    expect(recovery_calls == 1 && pen_lane == 14,
           "manual Save now preserves the selected drum across the runtime input reset");
    tap(B_HOME);
    tap(B_SEQ);
    expect(drum_lane == 14 && pen_lane == 14 && kb_grid,
           "returning to GRID after saving restores the same drum and step-key ownership");
    expect_beat(&before, "saving and returning to drums does not alter any recorded notes");

    grid_setup(32);
    turn(EN_K1, 2);
    turn(EN_K2, 17);
    dstep_set(&TDRUM->dstep[17], 2, LV_NORM, 0);
    dstep_set(&TDRUM->dstep[17], 3, LV_HARD, 2);
    beat_read(&before);
    press(B_SEQ);
    fm1_in.notes |= 1u << key_of_white(1);
    frame();
    turn(EN_K1, 1);
    fm1_in.notes &= ~(1u << key_of_white(1));
    frame();
    release(B_SEQ);
    expect(drum_lane == 3 && pen_lane == 3 && kb_grid,
           "held-step drum change becomes the selected GRID lane on release");
    expect_beat(&before, "releasing a step after changing drums never deletes the newly selected drum");
    frames(20);
    turn(EN_K1, -1);
    dstep_set(&TDRUM->dstep[1], 2, LV_SOFT, 3);
    beat_read(&before);
    press(B_SEQ);
    fm1_in.notes |= 1u << key_of_white(1);
    frame();
    key(1);                                      /* first black bank key: steps 1–16 */
    fm1_in.notes &= ~(1u << key_of_white(1));
    frame();
    release(B_SEQ);
    expect(drum_cursor / 16u == 0 && kb_grid,
           "held-SEQ page selection returns to that same bank in the ordinary grid");
    expect_beat(&before, "releasing a held step after changing banks never erases a different step");
}

static void test_held_controls_keep_priority(void)
{
    beat_t expected;
    int kit;
    unsigned lock_param;
    grid_setup(32);
    turn(EN_K1, 2);
    turn(EN_K2, 17);
    dstep_set(&TDRUM->dstep[17], 2, LV_NORM, 0);
    kit = TDRUM->p[P_E0];
    beat_read(&expected);
    press(B_SEQ);
    fm1_in.notes |= 1u << key_of_white(1);
    frame();
    turn(EN_K3, 1);
    dstep_set(&expected.notes[TRK_DRUM].drum[17], 2, LV_NORM, 1);
    expect_beat(&expected, "held SEQ plus Knob 3 keeps its existing Repeat role without also changing Strength");
    ui.lock_par = P_PAN;
    turn(EN_PRESET, 1);
    expect(TDRUM->p[P_E0] == kit && lock_find(TDRUM, 17, P_PAN, 0) >= 0,
           "PRESETS with a step held edits its parameter lock instead of changing kits");
    lock_param = ui.lock_par;
    turn(EN_ALGO, 1);
    expect(song.sel == TRK_DRUM && ui.lock_par != lock_param,
           "ALGORITHM with a step held chooses the lock parameter instead of changing parts");
    fm1_in.notes &= ~(1u << key_of_white(1));
    frame();
    release(B_SEQ);
    expect(on_drum_page() && kb_grid && drum_cursor / 16 == 1 &&
               dstep_has(&TDRUM->dstep[17], 2),
           "edited held step survives release into the same GRID bank");
}

static void test_seq_drum_bank_toggle(void)
{
    static const unsigned lengths[] = {1, 17, 32, 64};
    unsigned n, bank, lane;
    for (n = 0; n < NELEM(lengths); n++) {
        const unsigned len = lengths[n];
        grid_setup(len);
        dstep_set(&TDRUM->dstep[0], 0, LV_NORM, 0);
        turn(EN_K3, 1);                       /* keep a real edit in undo */
        TDRUM->micro[0] = -5;
        step_fill_set(TDRUM, 0, FC_FILL);
        lock_set(TDRUM, 0, P_PAN, -13);
        for (bank = 0; bank < (len + 15) / 16; bank++) {
            unsigned cursor = bank * 16;
            if (cursor + 5 < len) cursor += 5;
            turn(EN_K2, (int32_t)cursor - drum_cursor);
            for (lane = 0; lane < DRUM_LANES; lane++) {
                beat_t before;
                unsigned pass, t;
                unsigned char undo_before[sizeof undo];
                int16_t params[NTRK][P_COUNT], globals[G_COUNT];
                turn(EN_K1, (int32_t)lane - drum_lane);
                beat_read(&before);
                memcpy(undo_before, &undo, sizeof undo);
                for (t = 0; t < NTRK; t++) memcpy(params[t], trk[t].p, sizeof params[t]);
                memcpy(globals, song.g, sizeof globals);
                aud_lanes = 0;
                for (pass = 0; pass < 2; pass++) {
                    int same = 1;
                    tap(B_SEQ);
                    expect(on_drum_page() && !drum_page && kb_grid &&
                               ui.layer == LY_PLAY && drum_lane == (pass ? lane : (lane ^ 8u)) &&
                               pen_lane == drum_lane && drum_cursor == cursor,
                           "SEQ swaps drum banks in both directions while keeping row and step focus");
                    expect_beat(&before,
                                "SEQ drum-bank navigation preserves all notes, locks, nudge and fill");
                    for (t = 0; t < NTRK; t++)
                        if (memcmp(params[t], trk[t].p, sizeof params[t])) same = 0;
                    expect(same && !memcmp(globals, song.g, sizeof globals) &&
                               !memcmp(undo_before, &undo, sizeof undo) && !aud_lanes &&
                               !song.playing && !song.rec && !rec_wait && !recovery_calls,
                           "SEQ drum-bank navigation preserves mix and undo without audition, transport or saving");
                }
            }
        }
    }

    /* A held gesture owns SEQ until release; its release cannot become an
     * ordinary tap that unexpectedly swaps the drum currently being edited. */
    grid_setup(32);
    turn(EN_K1, 14);
    turn(EN_K2, 17);
    press(B_SEQ);
    frames(30);
    release(B_SEQ);
    expect(on_drum_page() && !drum_page && kb_grid && drum_lane == 14 && drum_cursor == 17,
           "long SEQ hold on GRID releases without swapping the selected drum bank");
    dstep_clr(&TDRUM->dstep[17], 14);
    press(B_SEQ);
    key(key_of_white(1));
    release(B_SEQ);
    expect(drum_lane == 14 && drum_cursor == 17 && dstep_has(&TDRUM->dstep[17], 14),
           "held SEQ step editing retains its selected drum instead of becoming a bank toggle");
    tap(B_SEQ);
    key(key_of_white(2));
    expect(drum_lane == 6 && dstep_has(&TDRUM->dstep[18], 6),
           "first step key after a bank toggle edits the corresponding drum in the newly visible bank");

    grid_setup(32);
    turn(EN_K1, 6);
    turn(EN_K2, 19);
    tap(B_PLAY);
    tap(B_REC);                              /* deliberately record on playable Home */
    tap(B_SEQ);                              /* explicit return to GRID during overdub */
    {
        beat_t before;
        unsigned rec = song.rec;
        beat_read(&before);
        tap(B_SEQ);
        expect(rec && song.rec == rec && song.playing && !rec_wait &&
                   on_drum_page() && !drum_page && kb_grid &&
                   drum_lane == 14 && drum_cursor == 19,
               "SEQ drum-bank switching during overdub keeps transport, recording and step focus");
        expect_beat(&before, "switching drum banks during overdub adds no recorded pad or step");
    }
    tap(B_REC);
    tap(B_PLAY);
}

static uint32_t screen_body_hash(void)
{
    unsigned i;
    uint32_t h = 2166136261u;
    /* Exclude the shared transport header and knob strip. An off-page marker
     * must update the drum workspace itself, not only bar.beat in the header. */
    for (i = 40u * 240u; i < 184u * 240u; i++) h = (h ^ screen[i]) * 16777619u;
    return h;
}

static void test_lengths_and_native_frames(void)
{
    static const unsigned lengths[] = {1, 16, 17, 32, 64};
    static const unsigned bank_key[] = {1, 3, 5, 8};
    unsigned n, lane, bank;
    for (n = 0; n < NELEM(lengths); n++) {
        const unsigned len = lengths[n];
        grid_setup(len);
        steps_clear(TDRUM);
        TDRUM->p[P_SLEN] = (int16_t)len;
        for (lane = 0; lane < DRUM_LANES; lane++) {
            turn(EN_K1, (int32_t)lane - drum_lane);
            for (bank = 0; bank < (len + 15) / 16; bank++) {
                unsigned first = bank * 16, end = first + 16 < len ? first + 16 : len;
                key(bank_key[bank]);
                expect(drum_lane == lane && drum_cursor / 16 == bank && drum_cursor < len,
                       "each valid step bank keeps the selected drum and an in-range cursor");
                key(key_of_white(0));
                expect(dstep_has(&TDRUM->dstep[first], lane),
                       "first white step key writes the selected lane in every valid bank");
                key(key_of_white(0));
                if (end - first < 16) {
                    beat_t before;
                    beat_read(&before);
                    key(key_of_white(end - first));
                    expect_beat(&before, "white keys beyond a partial final bank cannot add hidden hits");
                }
                turn(EN_K2, (int32_t)(end - 1) - drum_cursor);
                ui.force = 1;
                frame();                         /* lcd_blit/fill assert native bounds */
            }
        }
        turn(EN_K2, -1000);
        expect(drum_cursor == 0, "Step focus clamps at the first step");
        turn(EN_K2, 1000);
        expect(drum_cursor == len - 1, "Step focus clamps at the actual pattern length");
        if (len < NSTEP) {
            beat_t before;
            beat_read(&before);
            key(bank_key[(len + 15) / 16]);
            expect(drum_cursor < len, "unavailable black-key bank cannot select beyond pattern length");
            expect_beat(&before, "unavailable black-key bank neither plays nor rewrites hidden steps");
        }
    }

    grid_setup(32);
    dstep_set(&TDRUM->dstep[1], 2, LV_GHOST, 1);
    dstep_set(&TDRUM->dstep[3], 2, LV_SOFT, 2);
    dstep_set(&TDRUM->dstep[5], 2, LV_HARD, 3);
    ui.msg_t = 0;
    ui.force = 1;
    ui_draw();
    ppm("grid-first-lanes-strengths-and-repeats");
    grid_setup(64);
    turn(EN_K1, 8);
    turn(EN_K2, 17);
    ui.msg_t = 0;
    ui.force = 1;
    ui_draw();
    ppm("grid-lower-lanes-bank-2");
    {
        uint32_t first;
        song.playing = 1;
        TDRUM->seq_idx = 0;
        ui.force = 1;
        ui_draw();
        first = screen_body_hash();
        TDRUM->seq_idx = 16;
        ui_draw();
        expect(drum_cursor == 17 && screen_body_hash() != first,
               "on/off-page playhead indicator updates while the editing bank stays fixed");
        ppm("grid-playhead-visible");
        TDRUM->seq_idx = 48;
        ui_draw();
        expect(drum_cursor == 17, "off-page playback never chases the editor to a different bank");
        ppm("grid-playhead-away");
        song.playing = 0;
    }
    grid_setup(17);
    turn(EN_K1, 15);
    turn(EN_K2, 16);
    ui.msg_t = 0;
    ui.force = 1;
    ui_draw();
    ppm("grid-partial-bank-last-lane");
}

int main(int argc, char **argv)
{
    outdir = argc > 1 ? argv[1] : "build/jam-drum-grid-tests/screens";
    test_cell_controls();
    test_kit_and_tempo();
    test_part_order_and_handoff();
    test_audition_and_selected_lane();
    test_guards_and_shrunk_length();
    test_save_selection_and_held_release();
    test_held_controls_keep_priority();
    test_seq_drum_bank_toggle();
    test_lengths_and_native_frames();
    printf("drum-grid: %u checks, %s\n", checks, fails ? "FAIL" : "all checks passed");
    return fails != 0;
}
