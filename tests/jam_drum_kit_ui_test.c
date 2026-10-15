/* SPDX-License-Identifier: GPL-3.0-only */
/* Real panel, audio and native LCD paths. The recovery writer is the shared
 * navigation fixture's call-counting double: no hardware or flash is used. */
#define JAM_DRUM_NAVIGATION_HARNESS_ONLY 1
#include "jam_drum_navigation_test.c"

typedef struct {
    int16_t p[NTRK][P_COUNT], g[G_COUNT];
    step_t notes[NTRK][NSTEP];
    int8_t micro[NTRK][NSTEP];
    plock_t lock[NTRK][NLOCK];
    uint8_t fill[NTRK][NSTEP / 4], duration[NTRK][NSTEP][4];
    uint8_t mode[NTRK];
} kit_state_t;

static void kit_read(kit_state_t *s)
{
    unsigned t;
    memset(s, 0, sizeof *s);
    memcpy(s->g, song.g, sizeof s->g);
    for (t = 0; t < NTRK; t++) {
        memcpy(s->p[t], trk[t].p, sizeof s->p[t]);
        memcpy(s->notes[t], trk[t].step, sizeof s->notes[t]);
        memcpy(s->micro[t], trk[t].micro, sizeof s->micro[t]);
        memcpy(s->lock[t], trk[t].lock, sizeof s->lock[t]);
        memcpy(s->fill[t], trk[t].fill, sizeof s->fill[t]);
        memcpy(s->duration[t], trk[t].note_len, sizeof s->duration[t]);
        s->mode[t] = trk[t].duration_mode;
    }
}

static void kit_expect_state(const kit_state_t *s, const char *what)
{
    kit_state_t actual;
    kit_read(&actual);
    expect(!memcmp(s, &actual, sizeof actual), what);
}

static void kit_open(unsigned view)
{
    setup(view);
    memset(&mlu, 0, sizeof mlu);
    ml_arm = ml_heard = 0;
    tap(B_EDIT);
}

static void kit_expect_idle(const char *what)
{
    expect(!song.playing && !song.rec && !rec_wait && !ft_on && !ci_on && !recovery_calls, what);
}

static void kit_pad_without_edit(unsigned lane)
{
    kit_state_t before;
    unsigned key_id = key_of_white(lane);
    kit_read(&before);
    fm1_in.notes = 1u << key_id;
    frame();
    expect(!kb_grid && kb_kind[key_id] == KS_DRUM,
           "first key after SOUND/FX entry immediately plays a drum pad");
    kit_expect_state(&before, "playing a SOUND/FX pad does not rewrite notes or sound settings");
    fm1_in.notes = 0;
    frame();
}

static void test_direct_sound_entry(void)
{
    unsigned view, origin;
    for (view = JAM_HOME_GROOVE; view <= JAM_HOME_SOUND; view++) {
        for (origin = 0; origin < 4; origin++) {
            kit_state_t before;
            setup(view);
            if (origin == 1) tap(B_SEQ);
            if (origin == 2) tap(B_FX);
            if (origin == 3) open_family(FAM_GLO);
            kit_read(&before);
            tap(B_EDIT);
            expect(on_drum_page() && drum_page == 1 && !kb_grid && !song.seq_mode,
                   "EDIT from Home/GRID/FX/settings directly opens playable KIT SOUND");
            kit_expect_state(&before, "opening KIT SOUND preserves the complete jam and mix");
            tap(B_EDIT);
            expect(on_drum_page() && drum_page == 1 && !kb_grid,
                   "repeated EDIT stays in KIT SOUND rather than toggling to step editing");
            kit_pad_without_edit(3);
            tap(B_SEQ);
            expect(on_drum_page() && drum_page == 0 && kb_grid,
                   "SEQ from KIT SOUND returns directly to GRID");
            tap(B_SEQ);
            expect(on_drum_page() && drum_page == 0 && kb_grid,
                   "repeated SEQ stays in GRID after sound editing");
            tap(B_HOME);
            expect(ui.home && ui.jam_home_view == view,
                   "HOME returns from drum workspaces to its remembered Groove/Sound view");
            kit_expect_idle("ordinary drum navigation never starts transport, recording or saving");
        }
    }
}

static void test_sound_controls_and_sources(void)
{
    kit_state_t before;
    unsigned i, visited;
    kit_open(JAM_HOME_GROOVE);
    TDRUM->p[P_E0] = 0;
    song.g[G_DRLVL] = 75;
    song.g[G_DRREV] = 31;
    song.g[G_DRDLY] = 17;
    TDRUM->p[P_TFLT] = -19;
    TDRUM->p[P_PAN] = -11;
    kit_read(&before);
    turn(EN_K2, 1);
    expect(song.g[G_DRLVL] == 76, "KIT SOUND Level changes whole-kit level by one detent");
    before.g[G_DRLVL] = song.g[G_DRLVL];
    kit_expect_state(&before, "KIT SOUND Level preserves every other sound and pattern field");
    turn(EN_K3, 1);
    expect(TDRUM->p[P_TFLT] == -18 && song.g[G_DRREV] == 31,
           "KIT SOUND Tone changes the working kit filter rather than reverb");
    before.p[TRK_DRUM][P_TFLT] = TDRUM->p[P_TFLT];
    kit_expect_state(&before, "KIT SOUND Tone preserves every other sound and pattern field");
    turn(EN_K4, 1);
    expect(TDRUM->p[P_PAN] == -10, "KIT SOUND Pan moves the whole kit stereo position");
    before.p[TRK_DRUM][P_PAN] = TDRUM->p[P_PAN];
    kit_expect_state(&before, "KIT SOUND Pan preserves every other sound and pattern field");

    memset(usr_nz, 0, sizeof usr_nz);
    for (i = 0; i < DRUM_USR; i++)
        expect(drum_kit_available(i), "every factory sampled/synthesized kit is available");
    for (i = DRUM_USR; i <= DRUM_PAIR; i++)
        expect(!drum_kit_available(i), "empty user-sample kit sources are unavailable");
    for (i = DRUM_SYN; i < DRUM_KITS; i++)
        expect(drum_kit_available(i), "SYN user synth banks retain their real default sounds");
    /* Model a sample catalogue already validated by the existing scanner. No
     * flash header is fabricated/read and no sample voice is triggered. */
    memset(usr_zone, 0, sizeof usr_zone);
    usr_nz[2] = 1;
    usr_zone[2][0].n = 8;
    usr_zone[2][0].rate = 65536;
    usr_zone[2][0].lo = usr_zone[2][0].hi = LANE_NOTE[3];
    expect(drum_kit_available(DRUM_USR + 2) && drum_kit_available(DRUM_PAIR) &&
               !drum_kit_available(DRUM_USR) && !drum_kit_available(DRUM_USR + 3),
           "one validated panel-lane sample enables its own USR kit and the USR3+4 combination");
    usr_zone[2][0].lo = usr_zone[2][0].hi = 1;
    expect(!drum_kit_available(DRUM_USR + 2) && !drum_kit_available(DRUM_PAIR),
           "a user sample outside all panel drum lanes does not advertise a playable kit");
    usr_zone[2][0].lo = usr_zone[2][0].hi = LANE_NOTE[3];
    usr_zone[2][0].n = 0;
    expect(!drum_kit_available(DRUM_USR + 2), "a zero-length user sample does not enable an empty kit");
    usr_zone[2][0].n = 8;
    usr_zone[2][0].rate = 0;
    expect(!drum_kit_available(DRUM_USR + 2), "a zero-rate user sample does not enable an empty kit");
    memset(usr_nz, 0, sizeof usr_nz);
    TDRUM->p[P_E0] = 0;
    visited = 1;
    for (i = 1; i < DRUM_USR + DSU_N; i++) {
        turn(EN_K1, 1);
        expect(drum_kit_available(drum_kit()), "KIT SOUND kit browsing never selects an empty source");
        if (i < DRUM_USR) expect(drum_kit() == i, "full KIT SOUND browser includes factory kits in order");
        else expect(drum_kit() == DRUM_SYN + i - DRUM_USR,
                    "full KIT SOUND browser skips empty samples and reaches SYN1 through SYN4");
        visited++;
    }
    expect(visited == DRUM_USR + DSU_N && drum_kit() == DRUM_KITS - 1,
           "full KIT SOUND browser exposes all populated factory and SYN kit choices");
    turn(EN_K1, -DSU_N);
    expect(drum_kit() == DRUM_USR - 1,
           "reverse KIT SOUND browsing skips the complete empty-user-sample gap");
    before.p[TRK_DRUM][P_E0] = TDRUM->p[P_E0];
    kit_expect_state(&before, "full kit browsing preserves the beat, mix, sends and other parts");
    ui.force = 1;
    frame();
    ppm("kit-sound");
}

static void test_kit_fx_scope(void)
{
    static const unsigned ids[] = {P_TFLT, G_DRDLY, G_DRREV, P_PAN};
    unsigned slot;
    for (slot = 0; slot < 4; slot++) {
        kit_state_t before;
        int16_t *vp;
        const param_desc_t *desc;
        unsigned global = slot == 1 || slot == 2;
        kit_open(JAM_HOME_SOUND);
        tap(B_FX);
        expect(cur_page()->scope == SC_KITFX && !kb_grid && !song.seq_mode,
               "FX from outside its family directly opens playable KIT FX");
        TDRUM->p[P_TFLT] = -21;
        TDRUM->p[P_PAN] = -15;
        song.g[G_DRDLY] = 23;
        song.g[G_DRREV] = 29;
        ui.lock_par = P_REL;
        memset(&mlu, 0, sizeof mlu);
        mlu.on = 1;
        ml_arm = ml_heard = 0;
        desc = page_desc(cur_page(), slot, &vp);
        expect(desc && vp == (global ? &song.g[ids[slot]] : &TDRUM->p[ids[slot]]) &&
                   page_global(cur_page(), slot) == global,
               "KIT FX parameter descriptor and edit scope point to the same real control");
        kit_read(&before);
        turn(EN_K1 + slot, 1);
        expect(vp && *vp > (global ? before.g[ids[slot]] : before.p[TRK_DRUM][ids[slot]]),
               "each KIT FX knob changes its intended live sound field");
        if (global) before.g[ids[slot]] = *vp;
        else before.p[TRK_DRUM][ids[slot]] = *vp;
        kit_expect_state(&before, "a KIT FX turn changes only its intended field across the whole jam");
        if (global)
            expect(ui.lock_par == P_REL && !mlu.pick && !ml_arm,
                   "kit Echo/Room sends never select an unrelated melodic parameter for lock/MIDI learn");
        else
            expect(ui.lock_par == ids[slot] && mlu.pick && mlu.id == ids[slot] &&
                       mlu.trk == TRK_DRUM && ml_arm,
                   "KIT FX Filter/Pan retain the correct track parameter lock and MIDI-learn target");
        memset(&mlu, 0, sizeof mlu);
        ml_arm = 0;
        kit_pad_without_edit(4);
        kit_expect_idle("KIT FX controls do not start transport, recording or saving");
        if (slot == 3) { ui.force = 1; frame(); ppm("kit-fx"); }
    }
}

static void test_fx_depth_and_left_knobs(void)
{
    static const char *const expected[] = {"KIT FX", "SLICER", "INSERT", "INSERT 2", "ALL ECHO", "ALL ROOM"};
    unsigned i, k, start;
    kit_state_t before;
    kit_open(JAM_HOME_GROOVE);
    tap(B_FX);
    for (i = 0; i < NELEM(expected); i++) {
        unsigned active = 0;
        expect(!strcmp(cur_page()->title, expected[i]),
               "FX tap order contains useful kit controls and explicitly shared echo/room pages");
        for (k = 0; k < 4; k++) {
            int16_t *vp;
            const param_desc_t *d = page_desc(cur_page(), k, &vp);
            active += d && vp;
        }
        expect(active && (i != 5 || active == 2),
               "every drum FX page has working controls and ALL ROOM omits inactive chorus columns");
        song.g[G_BPM] = 100;
        start = ui.page;
        turn(EN_SELECT, 3);
        expect(song.g[G_BPM] == 103 && ui.page == start,
               "SELECT counts one BPM per detent without navigating drum FX pages");
        start = jam_sound_index(TRK_DRUM);
        kit_read(&before);
        turn(EN_PRESET, 1);
        expect(jam_sound_index(TRK_DRUM) == (start + 1) % jam_sound_count(TRK_DRUM),
               "PRESETS browses curated whole kits on every drum FX page");
        before.p[TRK_DRUM][P_E0] = TDRUM->p[P_E0];
        kit_expect_state(&before, "FX kit browsing preserves every other pattern and sound field");
        if (i == 1) { ui.force = 1; frame(); ppm("kit-slicer"); }
        if (i == 2) { ui.force = 1; frame(); ppm("kit-insert"); }
        if (i == 5) { ui.force = 1; frame(); ppm("kit-all-room"); }
        tap(B_FX);
    }
    expect(cur_page()->scope == SC_KITFX, "FX wraps from shared room back to KIT FX");
    tap(B_HOME);
    tap(B_FX);
    expect(cur_page()->scope == SC_KITFX, "fresh FX entry always starts at KIT FX despite remembered depth");
    turn(EN_ALGO, 1);
    expect(song.sel == jam_role_track(1) && ui.home && !kb_grid,
           "ALGORITHM from drum FX advances to displayed Bass and returns to remembered Home");
}

static void test_unavailable_taps_and_track_handoff(void)
{
    static const unsigned buttons[] = {B_ENV, B_LFO, B_SCL, B_ARP};
    static const char *const messages[] = {"EDIT: WHOLE KIT SOUND", "FX: EFFECTS + SLICER",
        "DRUMS: PLAY PADS", "HOLD ARP + PAD: ROLL"};
    static const unsigned families[] = {FAM_ENV, FAM_LFO, FAM_SCL, FAM_ARP, FAM_EDIT, FAM_FX};
    unsigned workspace, i;
    for (workspace = 0; workspace < 3; workspace++) {
        kit_open(JAM_HOME_SOUND);
        if (workspace == 0) tap(B_HOME);
        if (workspace == 2) tap(B_FX);
        for (i = 0; i < NELEM(buttons); i++) {
            unsigned page = ui.page, home = ui.home;
            kit_state_t before;
            kit_read(&before);
            tap(buttons[i]);
            expect(ui.page == page && ui.home == home &&
                       ui.msg_t && !strcmp(ui.msg, messages[i]),
                   "unavailable synth-page taps keep the current workspace and give useful drum guidance");
            kit_expect_state(&before, "contextual guidance never changes a sound, beat or shared musical setting");
        }
    }
    for (i = 0; i < NELEM(families); i++) {
        setup(JAM_HOME_GROOVE);
        song.sel = 0;
        open_family(families[i]);
        frame();
        expect(!ui.home && page_shown(cur_page()), "melodic tracks still expose their applicable synth pages");
        track_select(TRK_DRUM);
        frame();
        expect(ui.home || page_shown(cur_page()),
               "switching a synth page to Drums never strands the user on a hidden page");
        if (i < 4) expect(ui.home, "a family with no drum pages returns safely to remembered Home");
    }
}

static void test_held_controls_and_save(void)
{
    kit_state_t before;
    unsigned workspace, i;
    for (workspace = 0; workspace < 2; workspace++) {
        kit_open(JAM_HOME_SOUND);
        if (workspace) tap(B_FX);
        unsigned page = ui.page;
        int16_t tone = TDRUM->p[P_TFLT], echo = song.g[G_DRDLY], room = song.g[G_DRREV];
        song.g[G_FILT] = 0;
        press(B_FX);
        turn(EN_K1, 1);
        release(B_FX);
        frames(20);
        expect(ui.page == page && song.g[G_FILT] > 0 && TDRUM->p[P_TFLT] == tone &&
                   song.g[G_DRDLY] == echo && song.g[G_DRREV] == room,
               "held FX owns its knob without also editing the underlying kit page");
        song.g[G_ROLL] = 1;
        press(B_ARP);
        turn(EN_K1, 1);
        key(key_of_white(2));
        release(B_ARP);
        frames(20);
        expect(ui.page == page && song.g[G_ROLL] == 2 && !song.rec,
               "held ARP retains its roll rate and playable pad gesture without opening a dead page");
        for (i = 0; i < NPART; i++) trk[i].p[P_ROOT] = 0;
        press(B_SCL);
        key(key_of_white(1));
        release(B_SCL);
        frames(20);
        expect(ui.page == page && trk[0].p[P_ROOT] == trk[1].p[P_ROOT] &&
                   trk[1].p[P_ROOT] == trk[2].p[P_ROOT] && trk[0].p[P_ROOT] != 0,
               "held SEL retains the shared melodic song-key gesture on Drums");
        {
            dstep_t pattern[NSTEP];
            unsigned erased = 1;
            dstep_set(&TDRUM->dstep[1], 3, LV_SOFT, 2);
            dstep_set(&TDRUM->dstep[6], 3, LV_HARD, 1);
            dstep_set(&TDRUM->dstep[6], 4, LV_GHOST, 3);
            memcpy(pattern, TDRUM->dstep, sizeof pattern);
            press(B_EDIT);
            key(key_of_white(3));
            release(B_EDIT);
            frames(20);
            for (i = 0; i < NSTEP; i++) erased &= !dstep_has(&TDRUM->dstep[i], 3);
            expect(erased && ui.page == page && dstep_has(&TDRUM->dstep[6], 4),
                   "held EDIT erases the chosen drum across steps without also navigating its page");
            press(B_EDIT);
            tap(B_OCTDN);
            release(B_EDIT);
            frames(20);
            expect(ui.page == page && !memcmp(pattern, TDRUM->dstep, sizeof pattern),
                   "held EDIT plus OCT-minus still undoes the precise drum erasure");
        }
        pen_lane = 7;
        kit_read(&before);
        recovery_clears_lane = 1;
        tap(B_SAVE);
        expect(ui.recovery_screen && jam_recovery_ui_modal && !kb_grid && !recovery_calls,
               "SAVE from SOUND/FX opens the manual checkpoint screen without writing");
        key(key_of_white(1));
        kit_expect_state(&before, "keys in manual saving never change the hidden beat or kit controls");
        {
            unsigned selected_drum = pen_lane; /* A modal pad can audition/select a new drum. */
            tap(B_SAVE);
            expect(recovery_calls == 1 && pen_lane == selected_drum,
                   "explicit Save now calls the writer once and preserves the latest selected drum");
        }
        kit_expect_state(&before, "manual Save now leaves all kit effects, notes and other parts unchanged");
        tap(B_HOME);
        expect(ui.home && ui.jam_home_view == JAM_HOME_SOUND && !ui.recovery_screen,
               "HOME leaves manual saving in the remembered Home view");
    }
}

int main(int argc, char **argv)
{
    outdir = argc > 1 ? argv[1] : "build/jam-drum-kit-ui-tests/screens";
    test_direct_sound_entry();
    test_sound_controls_and_sources();
    test_kit_fx_scope();
    test_fx_depth_and_left_knobs();
    test_unavailable_taps_and_track_handoff();
    test_held_controls_and_save();
    printf("drum-kit-ui: %u checks, %s\n", checks, fails ? "FAIL" : "all checks passed");
    return fails != 0;
}
