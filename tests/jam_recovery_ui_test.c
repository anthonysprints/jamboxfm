/* SPDX-License-Identifier: GPL-3.0-only */
/* Real panel input and LCD paths with a recording recovery-adapter double.
 * This fixture proves UI authorization and routing, not physical flash writes.
 * Run with tests/run_jam_recovery_ui_tests.sh. */
#define JAMBOX_PROTO 1
#define JAMBOX_RECOVERY 1
#define UI_PAGES_HARNESS_ONLY 1
#include "ui_pages_test.c"

static unsigned recovery_calls, recovery_initializes;
static int recovery_claimed, recovery_ready = 1, recovery_failure;
static const char *recovery_mock_status = "Setup needed";
static const char *recovery_mock_reason = "";
static int jam_recovery_save(int initialize) {
    recovery_calls++;
    recovery_initializes += initialize != 0;
    if (recovery_failure) {
        recovery_mock_status = "Error";
        return 4;
    }
    recovery_claimed = 1;
    recovery_mock_status = "Saved";
    return 0;
}
static const char *jam_recovery_status(void) { return recovery_mock_status; }
static int jam_recovery_can_save(void) { return recovery_ready; }
static int jam_recovery_is_claimed(void) { return recovery_claimed; }
static const char *jam_recovery_block_reason(void) { return recovery_mock_reason; }
static void turn_knob(uint32_t role, int32_t steps) {
    encs[panel.enc[role]] += steps * panel.dir[role];
    frame();
}
static void reset_ui(void) {
    memset(&ui, 0, sizeof ui);
    memset(encs, 0, sizeof encs);
    panel = PANEL_DEFAULT;
    layers_init();
    palette_set(1);
    host_tracks_init();
    jam_new_dusty();
    go_home();
    jam_recovery_ui_modal = 0;
    ly_lock = LY_PLAY;
    fm1_in.buttons = edges_btn = fm1_in.notes = 0;
    song.playing = song.rec = rec_wait = ft_on = ci_on = 0;
    transport_req = 0;
    recovery_calls = recovery_initializes = recovery_claimed = recovery_failure = 0;
    recovery_ready = 1;
    recovery_mock_status = "Setup needed";
    recovery_mock_reason = "";
    frame();
}
static uint32_t screen_hash(void) {
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < 240u * 240u; i++) h = (h ^ screen[i]) * 16777619u;
    return h;
}
int main(void) {
    uint32_t before;
    reset_ui();
    tap(B_SAVE);
    check(ui.recovery_screen == 1 && jam_recovery_ui_modal && cur_btn() == B_SAVE,
          "SAVE tap opens dedicated modal with SAVE LED");
    check(!recovery_calls && !recovery_initializes, "opening recovery never writes flash");
    press(B_SAVE); frames(140); release(B_SAVE);
    check(ui.recovery_screen == 1 && !recovery_calls, "long SAVE on status never initializes");
    tap(B_SAVE);
    check(ui.recovery_screen == 2 && !ui.recovery_ack, "separate tap opens unacknowledged setup warning");
    before = screen_hash();
    recovery_mock_status = "Saving"; frame();
    check(before != screen_hash(), "Saving visibly replaces the first-use setup warning during a write");
    recovery_mock_status = "Setup needed"; frame();
    check(before == screen_hash(), "setup warning returns when no write is active");
    press(B_SAVE); frames(140); release(B_SAVE);
    check(!recovery_calls, "setup hold without backup acknowledgement never writes");
    press(B_SAVE); turn_knob(EN_K1, 1); frames(140); release(B_SAVE);
    check(!ui.recovery_ack && !recovery_calls, "cannot acknowledge backup while SAVE is held");
    turn_knob(EN_K1, 1);
    check(ui.recovery_ack, "separate knob turn explicitly acknowledges backup readiness");
    press(B_SAVE); frames(50); release(B_SAVE);
    check(!recovery_calls && ui.recovery_screen == 2, "short setup hold cancels without writing");
    press(B_SAVE);
    fm1_ms = ui.recovery_save_t0 + 1999u;
    ui_input(); ui_draw();
    check(!recovery_calls, "setup remains uncommitted at 1,999 milliseconds of the fresh hold");
    fm1_ms = ui.recovery_save_t0 + 2000u;
    ui_input(); ui_draw();
    release(B_SAVE);
    check(recovery_calls == 1 && recovery_initializes == 1 && recovery_claimed && ui.recovery_screen == 1,
          "fresh deliberate 2-second hold initializes and saves exactly once");
    frames(20);
    check(recovery_calls == 1, "release after confirmed setup never starts another save");
    tap(B_SAVE);
    check(recovery_calls == 2 && recovery_initializes == 1, "claimed Save now never reinitializes allocation");
    before = screen_hash();
    recovery_mock_status = "Saving"; frame();
    check(before != screen_hash(), "Saving visibly replaces Saved on the claimed status screen");
    recovery_mock_status = "Saved"; frame();
    recovery_ready = 0; recovery_mock_reason = "RELEASE KEYS FIRST";
    tap(B_SAVE);
    check(recovery_calls == 2, "blocked Save now never calls writer");
    press(B_SAVE);
    check(layer_now() == LY_PLAY, "SAVE inside modal cannot become section layer");
    press(B_SEQ);
    check(layer_now() == LY_PLAY, "SEQ inside modal cannot become step-writing layer");
    release(B_SEQ); release(B_SAVE);
    check(!song.rec && !song.seq_mode, "modal ignores recorder and ordinary step writing");
    press(B_REC); frames(150); release(B_REC);
    check(!song.rec && !ui.hold_kind && !rec_wait && !ft_on,
          "holding REC inside modal cannot arm, clear or start free recording");
    press(B_SEQ);
    fm1_in.notes |= 1u << 10;
    frame();
    check(kb_kind[10] == KS_NOTE && !song.seq_mode,
          "keys audition musical notes while shortcut buttons are suppressed");
    fm1_in.notes &= ~(1u << 10); frame(); release(B_SEQ);
    press(B_HOME);
    check(!ui.recovery_screen && ui.home && jam_recovery_ui_modal, "HOME escapes while keeping shortcuts suppressed until release");
    release(B_HOME); frame();
    check(!jam_recovery_ui_modal && ui.jam_home_view == JAM_HOME_GROOVE,
          "HOME release restores normal controls without toggling Home view");
    press(B_SAVE);
    check(layer_now() == LY_SONG, "held SAVE performance layer remains available outside modal");
    key(key_of_white(0)); release(B_SAVE);
    check(!ui.recovery_screen, "SAVE performance gesture does not open recovery afterward");
    rec_wait = 1; tap(B_SAVE);
    check(!ui.recovery_screen && !jam_recovery_ui_modal, "record-arm refuses modal entry");
    rec_wait = 0; song.rec = 1; tap(B_SAVE);
    check(!ui.recovery_screen, "armed recording refuses modal entry");
    song.rec = 0;
    ft_on = 1; jam_recovery_ui_open();
    check(!ui.recovery_screen && !jam_recovery_ui_modal, "active free take refuses modal entry");
    ft_on = 0; ci_on = 1; jam_recovery_ui_open();
    check(!ui.recovery_screen && !jam_recovery_ui_modal, "count-in refuses modal entry");
    ci_on = 0;

    reset_ui();
    jam_recovery_ui_open();
    tap(B_SAVE); turn_knob(EN_K1, 1);
    tap(B_HOME); frame();
    tap(B_SAVE); tap(B_SAVE);
    check(ui.recovery_screen == 2 && !ui.recovery_ack && !recovery_calls,
          "HOME cancellation discards backup acknowledgement before reopening setup");
    turn_knob(EN_K1, 1);
    recovery_ready = 0; recovery_mock_reason = "RECOVERY READ ONLY";
    press(B_SAVE); frames(140); release(B_SAVE);
    check(!recovery_calls && ui.recovery_screen == 2,
          "acknowledgement and long hold cannot bypass read-only adapter");
    recovery_ready = 1; recovery_mock_reason = ""; frames(140);
    check(!recovery_calls, "unblocking after released setup hold cannot initialize automatically");
    press(B_SAVE); frames(135); release(B_SAVE);
    check(recovery_calls == 1 && recovery_initializes == 1,
          "unblocked setup still requires a new deliberate SAVE hold");
    recovery_failure = 1;
    tap(B_SAVE);
    check(recovery_calls == 2 && !strcmp(jam_recovery_status(), "Error"),
          "writer failure remains visible as Error without claiming a successful save");
    before = screen_hash();
    recovery_mock_status = "Pending"; frame();
    check(before != screen_hash(), "status changes redraw the modal without a forced redraw");
    press(B_HOME); press(B_SEQ); release(B_HOME);
    check(jam_recovery_ui_modal && !ui.recovery_screen,
          "cancel keeps shortcut suppression while a second button remains held");
    release(B_SEQ); frame();
    check(!jam_recovery_ui_modal, "normal shortcuts resume once all cancel-time buttons release");

    reset_ui();
    song.sel = TRK_DRUM;
    kb_grid = 1;
    jam_recovery_ui_open();
    check(!kb_grid && !song.seq_mode, "opening from drum grid clears step-writing keyboard mode");
    press(B_PLAY); release(B_PLAY);
    check(!song.playing && !transport_req, "PLAY in recovery cannot start stopped transport");
    song.playing = 1;
    press(B_PLAY);
    check(transport_req == 2, "explicit PLAY can request a stop before saving");
    release(B_PLAY);

    /* A sound change can leave the FM6 patch waiting for the old voice fade.
     * Opening recovery immediately must keep that background poll alive, or
     * the target patch never arrives and the new engine remains blocked. */
    reset_ui();
    song.sel = 1;
    trk_note_on(&trk[1], 60, 100);
    frame();
    check(jam_sound_load(1, 10), "FM6 preset can be selected while the previous sound is ringing");
    check((fm6_pend & (1u << 1)) && trk[1].pold,
          "regression fixture opens during a genuinely pending FM6 patch fade");
    jam_recovery_ui_open();
    frames(4);
    check(ui.recovery_screen == 1 && !(fm6_pend & (1u << 1)),
          "recovery modal continues polling until the pending FM6 patch loads");
    check(trk[1].engine == ENGI_FM6 && !trk[1].pold && !trk[1].xf_on &&
              fm6_slot[1] == trk[1].p[P_E7],
          "old voice fade and FM6 engine switch finish while recovery remains open");
    {
        uint8_t packed[FM6_PACKED], expected[FP_SIZE + 1u];
        fm6_slot_get((uint32_t)trk[1].p[P_E7], packed);
        fm6_unpack(packed, expected);
        check(!memcmp(expected, fm6_patch[1], sizeof expected),
              "settled modal patch matches the selected FM6 sound");
    }
    printf("recovery-ui: %s\n", fails ? "FAIL" : "all checks passed");
    return fails != 0;
}
