/* SPDX-License-Identifier: GPL-3.0-only */
/* Drum entry and gesture ownership through the shared panel, audio and LCD
 * paths. The recovery adapter below records calls without accessing flash.
 * Run with tests/run_jam_drum_navigation_tests.sh. */
#define JAMBOX_PROTO 1
#define JAMBOX_RECOVERY 1
#define UI_PAGES_HARNESS_ONLY 1
#include "ui_pages_test.c"

static unsigned checks, recovery_calls;
static unsigned recovery_clears_lane;      /* model the runtime input reset */
static int jam_recovery_save(int initialize)
{
    (void)initialize;
    recovery_calls++;
    if (recovery_clears_lane) pen_lane = 0;
    return 0;
}
static const char *jam_recovery_status(void) { return "Saved"; }
static int jam_recovery_can_save(void) { return 1; }
static int jam_recovery_is_claimed(void) { return 1; }
static const char *jam_recovery_block_reason(void) { return ""; }

static void expect(int ok, const char *what)
{
    checks++;
    check(ok, what);
}

static void turn(uint32_t role, int32_t steps)
{
    encs[panel.enc[role]] += steps * panel.dir[role];
    frame();
}

static unsigned page_named(const char *title)
{
    unsigned i;
    for (i = 0; i < NPAGES; i++)
        if (!strcmp(PAGES[i].title, title)) return i;
    fprintf(stderr, "Missing test page: %s\n", title);
    exit(2);
}

static void setup(unsigned view)
{
    /* Every scenario ends its gestures. Reset song state as a new jam does,
     * then give any existing release-time knob quiet period time to expire. */
    fm1_in.buttons = fm1_in.notes = edges_btn = notes_seen = 0;
    song.playing = song.rec = rec_wait = ft_on = ci_on = 0;
    rec_count = rec_tempo = 0;
    transport_req = 0;
    jam_recovery_ui_modal = 0;
    ly_lock = LY_PLAY;
    memset(&ui, 0, sizeof ui);
    memset(encs, 0, sizeof encs);
    panel = PANEL_DEFAULT;
    layers_init();
    palette_set(1);
    host_tracks_init();
    if (!jam_new_dusty()) exit(2);
    song.sel = TRK_DRUM;
    go_home();
    ui.jam_home_view = (uint8_t)view;
    drum_page = 1;                         /* entry must not inherit old KIT */
    drum_lane = drum_cursor = 0;
    recovery_calls = 0;
    recovery_clears_lane = 0;
    frames(20);
}

static void test_home_entry(void)
{
    static const unsigned views[] = {JAM_HOME_GROOVE, JAM_HOME_SOUND};
    static const char *const remembered[] = {"PATTERN", "SONG"};
    dstep_t before[NSTEP];
    unsigned v, r;
    for (v = 0; v < NELEM(views); v++) {
        for (r = 0; r < NELEM(remembered); r++) {
            setup(views[v]);
            ui.fam_last[FAM_SEQ] = (uint8_t)page_named(remembered[r]);
            memcpy(before, TDRUM->dstep, sizeof before);
            tap(B_SEQ);
            expect(on_drum_page() && !drum_page && cur_btn() == B_SEQ,
                   "Drums SEQ opens GRID despite remembered PATTERN/SONG");
            expect(!memcmp(before, TDRUM->dstep, sizeof before) && !song.playing &&
                       !song.rec && !rec_wait && !ft_on && !recovery_calls,
                   "opening GRID neither changes the beat nor starts recording/saving");

            /* No idle frame between the button release and the first key. */
            key(key_of_white(1));
            expect(dstep_has(&TDRUM->dstep[1], 0),
                   "first white key immediately after SEQ edits its step without REC");
            key(key_of_white(1));
            expect(!memcmp(before, TDRUM->dstep, sizeof before),
                   "pressing the same step again restores the original beat");
            tap(B_HOME);
            expect(ui.home && ui.jam_home_view == views[v],
                   "HOME from GRID returns to the remembered Home view");

            /* Home must also switch ownership before the next audio frame. */
            fm1_in.notes = 1u << key_of_white(1);
            frame();
            expect(kb_kind[key_of_white(1)] == KS_DRUM &&
                       !memcmp(before, TDRUM->dstep, sizeof before),
                   "first key immediately after HOME plays a drum without editing steps");
            fm1_in.notes = 0;
            frame();
        }
    }
}

static void test_other_pages(void)
{
    static const unsigned families[] = {FAM_ENV, FAM_LFO, FAM_EDIT, FAM_FX, FAM_GLO, FAM_SCL};
    unsigned i;
    for (i = 0; i < NELEM(families); i++) {
        setup(JAM_HOME_SOUND);
        open_family(families[i]);
        frame();
        ui.fam_last[FAM_SEQ] = (uint8_t)page_named("SONG");
        tap(B_SEQ);
        expect(on_drum_page() && !drum_page && !song.seq_mode,
               "SEQ from an ordinary drum parameter page opens GRID, not SONG");
    }
    setup(JAM_HOME_GROOVE);
    open_family(FAM_SEQ);
    ui.page = (uint8_t)page_named("SONG");
    ui.fam_last[FAM_SEQ] = ui.page;
    page_entered();
    frame();
    tap(B_SEQ);
    expect(on_drum_page() && !drum_page && !arrangement_enabled,
           "SEQ also escapes the inherited SONG page into drum GRID");
}

static void test_existing_editor(void)
{
    dstep_t before[NSTEP], expected[NSTEP];
    setup(JAM_HOME_GROOVE);
    tap(B_SEQ);
    frame();
    turn(EN_K1, 2);
    memcpy(before, TDRUM->dstep, sizeof before);
    key(key_of_white(1));
    expect(dstep_has(&TDRUM->dstep[1], 2) && !dstep_has(&TDRUM->dstep[1], 0),
           "GRID sound knob chooses the drum lane edited by the step keys");
    memcpy(expected, before, sizeof expected);
    dstep_set(&expected[1], 2, LV_NORM, 0);
    expect(!memcmp(expected, TDRUM->dstep, sizeof expected) && !song.playing,
           "a GRID step edit changes only the selected lane and cell");
    key(key_of_white(1));
    expect(!memcmp(before, TDRUM->dstep, sizeof before) && !song.rec && !rec_wait,
           "selected-lane step toggle changes no other drum or recorder state");
    tap(B_SEQ);
    expect(on_drum_page() && drum_page == 0 && kb_grid,
           "repeated SEQ stays in GRID with step-key ownership");
    tap(B_EDIT);
    expect(on_drum_page() && drum_page == 1 && !kb_grid,
           "EDIT opens KIT and immediately restores playable pads");
    tap(B_SEQ);
    expect(on_drum_page() && drum_page == 0 && kb_grid,
           "SEQ from KIT returns directly to GRID with step-key ownership");
    tap(B_EDIT);
    expect(on_drum_page() && drum_page == 1,
           "EDIT tap opens KIT SOUND without inheriting a synth edit page");
    tap(B_HOME);
    tap(B_SEQ);
    expect(on_drum_page() && drum_page == 0,
           "fresh SEQ entry returns to GRID after leaving KIT");
}

static void test_held_and_locked(void)
{
    setup(JAM_HOME_SOUND);
    pen_lane = 2;
    press(B_SEQ);
    key(key_of_white(1));
    release(B_SEQ);
    expect(ui.home && ui.jam_home_view == JAM_HOME_SOUND && ui.layer == LY_PLAY &&
               !on_drum_page() && dstep_has(&TDRUM->dstep[1], 2),
           "held SEQ edits steps and releases without opening GRID");

    setup(JAM_HOME_GROOVE);
    press(B_SEQ);
    frames(30);                            /* deliberately beyond tap threshold */
    release(B_SEQ);
    expect(ui.home && ui.layer == LY_PLAY && !on_drum_page(),
           "long untouched SEQ hold releases without becoming a page tap");

    setup(JAM_HOME_SOUND);
    press(B_SEQ);
    tap(B_HOME);
    release(B_SEQ);
    expect(ly_lock == LY_STEP && ui.layer == LY_STEP && ui.home,
           "SEQ plus HOME still locks the advanced steps layer");
    key(key_of_white(1));
    expect(dstep_has(&TDRUM->dstep[1], 0),
           "locked SEQ still edits a drum step without holding the button");
    tap(B_SEQ);
    expect(ly_lock == LY_PLAY && ui.layer == LY_PLAY && ui.home && !on_drum_page(),
           "SEQ tap that unlocks a layer does not also open GRID");
    tap(B_SEQ);
    expect(on_drum_page() && !drum_page,
           "a subsequent separate SEQ tap opens GRID normally");

    setup(JAM_HOME_GROOVE);
    press(B_FX);
    tap(B_HOME);
    release(B_FX);
    tap(B_SEQ);
    expect(ly_lock == LY_PLAY && ui.layer == LY_PLAY && ui.home && !on_drum_page(),
           "SEQ used to unlock another performance layer is consumed once");
}

static void test_transport_and_save(void)
{
    dstep_t before[NSTEP];
    setup(JAM_HOME_SOUND);
    tap(B_SEQ);
    tap(B_PLAY);
    expect(song.playing && !song.rec && !rec_wait && on_drum_page(),
           "PLAY from GRID starts the loop without recording or leaving the editor");
    tap(B_PLAY);
    expect(!song.playing && !song.rec && on_drum_page(),
           "PLAY again stops the loop and preserves GRID");
    memcpy(before, TDRUM->dstep, sizeof before);
    tap(B_SAVE);
    expect(ui.recovery_screen == 1 && jam_recovery_ui_modal && !kb_grid &&
               !song.seq_mode && !recovery_calls,
           "SAVE from GRID opens manual saving and releases step-key ownership");
    key(key_of_white(1));
    expect(!memcmp(before, TDRUM->dstep, sizeof before) && !recovery_calls,
           "playing a key on the save screen neither edits GRID nor saves implicitly");
    tap(B_SAVE);
    expect(recovery_calls == 1,
           "only the explicit Save now tap calls the recovery writer");
    tap(B_HOME);
    expect(ui.home && ui.jam_home_view == JAM_HOME_SOUND && !ui.recovery_screen,
           "HOME exits manual saving into the remembered Home view");
}

static void test_recording_ownership(void)
{
    dstep_t before[NSTEP];
    unsigned i;
    setup(JAM_HOME_SOUND);
    tap(B_SEQ);
    tap(B_REC);
    expect(ui.home && ui.jam_home_view == JAM_HOME_SOUND && rec_wait && !kb_grid &&
               !song.playing && !song.rec,
           "REC from stopped GRID arms recording on the remembered playable Home");
    fm1_in.notes = 1u << key_of_white(1);
    frame();
    expect(kb_kind[key_of_white(1)] == KS_DRUM && song.playing &&
               (song.rec & (1u << TRK_DRUM)) && !rec_wait && !kb_grid,
           "first pad after GRID record-arm plays a drum and starts live recording");
    fm1_in.notes = 0;
    frame();
    tap(B_REC);
    expect(song.playing && !song.rec && !rec_wait && ui.home,
           "REC cancels live overdub while the loop keeps playing");
    tap(B_PLAY);

    setup(JAM_HOME_GROOVE);
    tap(B_SEQ);
    rec_count = 1;
    tap(B_REC);
    tap(B_PLAY);
    expect(ui.home && ui.jam_home_view == JAM_HOME_GROOVE && ci_on && rec_wait &&
               !song.playing && !kb_grid,
           "GRID record-arm supports the existing count-in on playable Home");
    memcpy(before, TDRUM->dstep, sizeof before);
    fm1_in.notes = 1u << key_of_white(1);
    frame();
    expect(kb_kind[key_of_white(1)] == KS_DRUM && ci_on &&
               !memcmp(before, TDRUM->dstep, sizeof before),
           "pads during count-in audition drums without editing the hidden grid");
    fm1_in.notes = 0;
    frame();
    tap(B_REC);
    expect(!ci_on && !rec_wait && !song.rec && !song.playing,
           "REC still cancels count-in without starting the loop");

    setup(JAM_HOME_SOUND);
    for (i = 0; i < NTRK; i++) steps_clear(&trk[i]);
    tap(B_SEQ);
    tap(B_REC);
    fm1_in.notes = 1u << key_of_white(1);
    frame();
    expect(ui.home && ui.jam_home_view == JAM_HOME_SOUND && ft_on &&
               kb_kind[key_of_white(1)] == KS_DRUM && !kb_grid,
           "empty-jam REC from GRID accepts the first drum into a free take");
    fm1_in.notes = 0;
    frame();
    tap(B_PLAY);

    setup(JAM_HOME_GROOVE);
    tap(B_PLAY);
    tap(B_SEQ);
    tap(B_REC);
    expect(ui.home && ui.jam_home_view == JAM_HOME_GROOVE && song.playing &&
               (song.rec & (1u << TRK_DRUM)) && !kb_grid,
           "REC from playing GRID starts overdub on the remembered Home");
    tap(B_SEQ);
    expect(on_drum_page() && !drum_page && song.playing &&
               (song.rec & (1u << TRK_DRUM)) && kb_grid,
           "explicit SEQ during overdub opens GRID while recording continues");
    key(key_of_white(1));
    expect(dstep_has(&TDRUM->dstep[1], 0) && song.playing &&
               (song.rec & (1u << TRK_DRUM)),
           "intentional GRID view during overdub uses white keys for direct steps");
    tap(B_SAVE);
    expect(!ui.recovery_screen && !recovery_calls && on_drum_page(),
           "SAVE while recording refuses manual-save entry without losing GRID");
    tap(B_REC);
    expect(on_drum_page() && !song.rec && !rec_wait && song.playing && kb_grid,
           "REC ending overdub keeps GRID and leaves transport running");
    tap(B_PLAY);
}

static void open_recording_workspace(unsigned kit)
{
    setup(JAM_HOME_SOUND);
    if (kit) {
        tap(B_SEQ);
        tap(B_EDIT);
    }
}

static int recording_workspace_preserved(unsigned kit)
{
    return kit ? on_drum_page() && drum_page == 1 :
                 ui.home && ui.jam_home_view == JAM_HOME_SOUND;
}

static void test_recorder_first(void)
{
    dstep_t before[NSTEP];
    unsigned kit, i;
    /* Check both ordinary Home and dedicated KIT. In particular, the recorder
     * guard must run before an existing KIT can toggle to GRID. */
    for (kit = 0; kit <= 1; kit++) {
        open_recording_workspace(kit);
        tap(B_REC);
        memcpy(before, TDRUM->dstep, sizeof before);
        tap(B_SEQ);
        expect(recording_workspace_preserved(kit) && rec_wait && !kb_grid &&
                   !song.playing && !song.rec &&
                   !memcmp(before, TDRUM->dstep, sizeof before),
               "REC then SEQ preserves the armed workspace and its playable keys");
        expect(ui.msg_t && !strcmp(ui.msg, "FINISH TAKE FIRST"),
               "blocked recorder-first GRID entry explains how to continue");
        fm1_in.notes = 1u << key_of_white(1);
        frame();
        expect(kb_kind[key_of_white(1)] == KS_DRUM && song.playing &&
                   (song.rec & (1u << TRK_DRUM)) && !kb_grid,
               "first pad after blocked SEQ still starts the armed drum performance");
        fm1_in.notes = 0;
        frame();
        tap(B_REC);
        tap(B_PLAY);

        open_recording_workspace(kit);
        rec_count = 1;
        tap(B_REC);
        tap(B_PLAY);
        memcpy(before, TDRUM->dstep, sizeof before);
        tap(B_SEQ);
        expect(recording_workspace_preserved(kit) && ci_on && rec_wait &&
                   !kb_grid && !song.playing &&
                   !memcmp(before, TDRUM->dstep, sizeof before),
               "SEQ during count-in keeps its existing workspace and recorder ownership");
        fm1_in.notes = 1u << key_of_white(1);
        frame();
        expect(kb_kind[key_of_white(1)] == KS_DRUM && ci_on &&
                   !memcmp(before, TDRUM->dstep, sizeof before),
               "pad after blocked count-in SEQ auditions without toggling a step");
        fm1_in.notes = 0;
        frame();
        tap(B_REC);
        tap(B_SEQ);
        expect(on_drum_page() && !drum_page && kb_grid && !rec_wait && !ci_on,
               "a separate SEQ tap works again once count-in has been cancelled");

        open_recording_workspace(kit);
        for (i = 0; i < NTRK; i++) steps_clear(&trk[i]);
        tap(B_REC);
        key(key_of_white(1));
        memcpy(before, TDRUM->dstep, sizeof before);
        tap(B_SEQ);
        expect(recording_workspace_preserved(kit) && ft_on && !kb_grid &&
                   !memcmp(before, TDRUM->dstep, sizeof before),
               "SEQ during a free take preserves its workspace and unfinished recording");
        fm1_in.notes = 1u << key_of_white(2);
        frame();
        expect(kb_kind[key_of_white(2)] == KS_DRUM && ft_on && !kb_grid &&
                   !memcmp(before, TDRUM->dstep, sizeof before),
               "pad after blocked free-take SEQ records a drum without editing GRID");
        fm1_in.notes = 0;
        frame();
        tap(B_PLAY);
    }
}

static void test_melodic_routes(void)
{
    unsigned track;
    for (track = 0; track < NPART; track++) {
        setup(JAM_HOME_GROOVE);
        song.sel = (uint8_t)track;
        tap(B_SEQ);
        expect(!ui.home && cur_page()->scope == SC_STEP && song.seq_mode && !on_drum_page(),
               "melodic SEQ still opens its STEP editor");
        tap(B_SEQ);
        expect(!strcmp(cur_page()->title, "PATTERN") && song.seq_mode,
               "melodic repeated SEQ still cycles to PATTERN");
        tap(B_SEQ);
        expect(on_song_page() && !on_drum_page(),
               "melodic repeated SEQ still reaches SONG");
        tap(B_HOME);
        tap(B_SEQ);
        expect(on_song_page() && !on_drum_page(),
               "melodic SEQ still restores its remembered SONG destination");
    }
}

#ifndef JAM_DRUM_NAVIGATION_HARNESS_ONLY
int main(void)
{
    test_home_entry();
    test_other_pages();
    test_existing_editor();
    test_held_and_locked();
    test_transport_and_save();
    test_recording_ownership();
    test_recorder_first();
    test_melodic_routes();
    printf("drum-navigation: %u checks, %s\n", checks, fails ? "FAIL" : "all checks passed");
    return fails != 0;
}
#endif
