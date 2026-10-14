/* SPDX-License-Identifier: GPL-3.0-only */
/* Independent note lengths through the real pattern tools, undo, entry and
 * piano-roll renderer. No device access or shared asset generation. */
#define JAMBOX_PROTO 1
#define UI_PAGES_HARNESS_ONLY 1
#include "ui_pages_test.c"

static track_t *duration_setup(void)
{
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    memset(&ui, 0, sizeof ui);
    memset(&undo, 0, sizeof undo);
    panel = PANEL_DEFAULT;
    layers_init();
    host_tracks_init();
    song.sel = 0;
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_SLEN] = 16;
    return &trk[0];
}

static void duration_note(track_t *t, uint32_t at, uint32_t pitch, uint32_t duration)
{
    track_step_clear(t, at);
    t->step[at].time = ST_NOTE;
    t->step[at].note[0] = (uint8_t)pitch;
    t->step[at].n = 1;
    t->step[at].vel = 100;
    t->note_len[at][0] = (uint8_t)duration;
    jam_lengths_changed(t);
}

static void test_lengths(void)
{
    track_t *t = duration_setup();
    step_t later;
    duration_note(t, 0, 60, 6);
    t->step[0].n = 2;
    t->step[0].note[1] = 64;
    t->note_len[0][1] = 3;
    duration_note(t, 2, 67, 4);
    later = t->step[2];
    check(note_len(t, 0) == 6, "independent LEN shows the longest note at that onset");
    check(note_set_len(t, 0, 8) == 8 && t->note_len[0][0] == 8 && t->note_len[0][1] == 5,
          "LEN moves all onset lengths together, preserving their difference");
    check(!memcmp(&later, &t->step[2], sizeof later) && t->note_len[2][0] == 4 && t->step[1].time == ST_REST,
          "extending a note preserves staggered onsets and never writes shared ties");
    check(note_set_len(t, 0, 3) == 3 && t->note_len[0][0] == 3 && t->note_len[0][1] == 1,
          "shortening independent lengths clamps each note at one step");
    duration_note(t, 14, 72, 3);
    check(note_set_len(t, 14, 7) == 7 && t->note_len[14][0] == 7,
          "independent LEN can extend across the loop boundary");
    duration_note(t, 5, 69, 0);
    check(note_set_len(t, 5, 4) == 4 && t->note_len[5][0] == 4 && t->step[6].time == ST_REST,
          "new LEN edits in an independent POLY pattern preserve later cells");

    t = duration_setup();
    duration_note(t, 4, 60, 0);
    t->step[5].time = ST_TIE;
    t->step[6].time = ST_TIE;
    check(note_len(t, 4) == 3 && note_set_len(t, 4, 2) == 2 && t->step[6].time == ST_REST && !t->duration_mode,
          "legacy patterns retain their original shared-tie length editing");
}

static void test_undo_and_tools(void)
{
    track_t *t = duration_setup();
    uint8_t lengths[NSTEP][4];
    step_t steps[NSTEP];
    uint32_t i;
    duration_note(t, 1, 60, 7);
    undo_mark(t, (undo_sess += 4u) | 3u);
    track_step_clear(t, 1);
    check(!t->duration_mode && !t->note_len[1][0], "clearing the last independent onset clears the mode cache");
    check(undo_swap(0) && t->step[1].note[0] == 60 && t->note_len[1][0] == 7 && t->duration_mode,
          "undo restores independent lengths with their original notes");
    check(undo_swap(1) && !t->duration_mode && !t->note_len[1][0] && t->step[1].time == ST_REST,
          "redo restores the cleared lengths and mode");

    t = duration_setup();
    t->p[P_SLEN] = 4;
    for (i = 0; i < 4; i++) duration_note(t, i, 60 + i, i + 2);
    duration_note(t, 20, 80, 55);
    memcpy(lengths, t->note_len, sizeof lengths);
    memcpy(steps, t->step, sizeof steps);
    pattern_rotate(t, 1);
    check(t->step[0].note[0] == 63 && t->note_len[0][0] == 5 && t->note_len[1][0] == 2 && t->note_len[20][0] == 55,
          "rotation moves note lengths with their onsets and preserves hidden cells");
    pattern_rotate(t, -1);
    check(!memcmp(steps, t->step, sizeof steps) && !memcmp(lengths, t->note_len, sizeof lengths),
          "rotating back restores notes and lengths exactly");
    pattern_length(t, 1);
    check(t->p[P_SLEN] == 8 && !memcmp(t->note_len, t->note_len + 4, 16) && t->note_len[20][0] == 55,
          "doubling the pattern duplicates each onset's lengths");
    pattern_length(t, -1);
    check(t->p[P_SLEN] == 4 && !memcmp(t->note_len, t->note_len + 4, 16),
          "halving keeps the hidden note lengths for a later extension");
    memcpy(lengths, t->note_len, sizeof lengths);
    pattern_transpose(t, 1);
    check(t->step[0].note[0] == 61 && !memcmp(lengths, t->note_len, sizeof lengths),
          "whole-pattern transpose preserves every independent duration");
    ui.cursor = 1;
    step_edit(1, 1);
    check(t->step[1].note[0] == 63 && t->note_len[1][0] == 3,
          "transposing one onset retains that note's independent length");

    duration_note(t, 2, 126, 2);
    t->step[2].n = 3;
    t->step[2].note[1] = t->step[2].note[2] = 127;
    t->note_len[2][1] = 7;
    t->note_len[2][2] = 5;
    t->step[2].lvl = LV_GHOST | LV_HARD << 2 | LV_SOFT << 4;
    t->step[2].rat = 1 | 2 << 2 | 3 << 4;
    ui.cursor = 2;
    step_edit(1, 1);
    check(t->step[2].n == 1 && t->step[2].note[0] == 127 && t->note_len[2][0] == 7 &&
          !t->note_len[2][1] && !t->note_len[2][2] && (t->step[2].lvl & 3u) == LV_HARD && (t->step[2].rat & 3u) == 3,
          "pitch clamp merges duplicates with longest length, strongest level and fastest ratchet");
}

static void test_entry_and_clear(void)
{
    track_t *t = duration_setup();
    duration_note(t, 0, 60, 8);
    duration_note(t, 5, 69, 3);
    ui.step_page = 0;
    step_down(0);
    step_up(0);
    check(t->step[0].time == ST_REST && !t->note_len[0][0] && t->note_len[5][0] == 3 && t->duration_mode,
          "SEQ step deletion clears only that step's duration metadata");
    t->note_len[0][0] = 40; /* stale metadata must not attach to a replacement */
    step_down(0);
    check(t->step[0].time == ST_NOTE && !t->note_len[0][0] && t->note_len[5][0] == 3,
          "SEQ step insertion starts with clean duration metadata");

    duration_note(t, 0, 60, 8);
    open_family(FAM_SEQ);
    ui.cursor = 0;
    ui.entry_open = 0;
    mi_ok = 1;
    step_in_r = step_in_w = 0;
    step_in_lost = 0;
    step_in_q[step_in_w++ % STEP_IN_Q] = 65 | 0x80;
    midi_entry();
    check(t->step[0].note[0] == 65 && !t->note_len[0][0] && t->note_len[5][0] == 3,
          "MIDI step replacement clears only the replaced onset's lengths");

    duration_note(t, 0, 60, 8);
    ui.entry_open = 0;
    ui.cursor = 0;
    t->p[P_CHORD] = 0;
    t->p[P_QUANT] = 0;
    fm1_in.notes = 1u << 12;
    seq_entry(fm1_in.notes);
    fm1_in.notes = 0;
    check(!t->note_len[0][0] && t->step[0].n && t->note_len[5][0] == 3,
          "physical step replacement clears only the replaced onset's lengths");
    track_step_clear(t, 0);
    t->note_len[0][0] = 7;
    ui.cursor = 0;
    step_edit(2, -1);
    check(!t->note_len[0][0] && t->duration_mode && t->note_len[5][0] == 3,
          "legacy TIME editing clears replaced cell metadata and keeps other note lengths");
}

static void test_roll(void)
{
    track_t *t = duration_setup();
    uint32_t before;
    t->p[P_SLEN] = 32;
    duration_note(t, 14, 60, 5);
    duration_note(t, 16, 67, 2);
    ui.bank = 1;
    memset(screen, 0, sizeof screen);
    cv_begin(240, 100, C_BLACK);
    graph_roll(t, C_HI);
    cv_blit(0, 0);
    check(screen[80 * 240] == swap16(C_GRAY) && screen[80 * 240 + 15] == swap16(C_GRAY),
          "piano roll shows a held pitch entering a later bank behind a staggered onset");
    before = steps_hash(t);
    t->note_len[14][0]++;
    check(steps_hash(t) != before, "changing only note length invalidates the pattern graphic cache");
    t->step[16].n = 4;
    t->step[16].note[2] = 70;
    t->step[16].note[3] = 72;
    before = steps_hash(t);
    t->step[16].note[3]++;
    check(steps_hash(t) != before, "editing the fourth chord note invalidates the pattern graphic cache");
    duration_note(t, 31, 60, 4);
    ui.bank = 0;
    cv_begin(240, 100, C_BLACK);
    graph_roll(t, C_HI);
    cv_blit(0, 0);
    check(screen[80 * 240] == swap16(C_GRAY), "piano roll shows independent sustains crossing the loop boundary");
    if (outdir) ppm("duration-roll-wrap");
}

static void test_recording_handoffs(void)
{
    track_t *t = duration_setup();
    song.playing = 1;
    song.rec = 1;
    clk_beat = clk_pos = 0;
    t->p[P_SDIV] = 2;
    rec_note(t, 60, 100, 0, 1);
    t->v[0].active = t->v[0].gate = 1;
    t->v[0].note = 60;
    clk_pos = BEAT_U * 3u / 4u;
    track_select(1);
    check(song.sel == 1 && song.rec == 2 && t->note_len[0][0] == 3 && t->v[0].gate,
          "recording follows track selection after finalizing the old held note, without cutting its live voice");

    t = duration_setup();
    song.playing = 1;
    clk_beat = clk_pos = 0;
    rec_wait = ft_on = ci_on = 0;
    arrangement_enabled = 0;
    t->p[P_SDIV] = 2;
    fm1_in.buttons = BT(B_REC);
    holds_input(BT(B_REC), 1000);
    rec_note(t, 60, 100, 0, 1);
    clk_pos = BEAT_U / 2u;
    holds_input(0, 1750);
    check(!song.rec && ui.hold_kind == 1 && t->note_len[0][0] == 2,
          "REC hold cancelling a new take finalizes held note lengths before restoring the earlier arm state");
    fm1_in.buttons = 0;
    holds_input(0, 1760);
}

int main(int argc, char **argv)
{
    outdir = argc > 1 ? argv[1] : 0;
    test_lengths();
    test_undo_and_tools();
    test_entry_and_clear();
    test_roll();
    test_recording_handoffs();
    printf("Independent duration UI: %s\n", fails ? "FAIL" : "passed");
    return !!fails;
}
