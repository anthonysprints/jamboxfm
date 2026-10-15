/* SPDX-License-Identifier: GPL-3.0-only */
/* Applicable melodic sound controls through the production panel, renderer,
 * audio and parameter descriptors. The shared recovery writer is a double:
 * this fixture never accesses a physical device or writes flash. */
#define JAM_DRUM_NAVIGATION_HARNESS_ONLY 1
#include "jam_drum_navigation_test.c"

typedef struct {
    int16_t p[NTRK][P_COUNT], g[G_COUNT];
    step_t notes[NTRK][NSTEP];
    int8_t micro[NTRK][NSTEP];
    plock_t lock[NTRK][NLOCK];
    uint8_t fill[NTRK][NSTEP / 4], duration[NTRK][NSTEP][4];
    uint8_t mode[NTRK];
    dsu_bank_t kits;
} sound_state_t;

static void state_read(sound_state_t *s)
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
    s->kits = dsu;
}

static void state_expect(const sound_state_t *s, const char *why)
{
    sound_state_t now;
    state_read(&now);
    expect(!memcmp(s, &now, sizeof now), why);
}

static unsigned engine_index(const engine_t *e)
{
    unsigned i;
    for (i = 0; i < NENGINES; i++) if (ENGINES[i] == e) return i;
    fprintf(stderr, "Missing fixture engine: %s\n", e->name);
    exit(2);
}

static void melodic_setup(const engine_t *e)
{
    setup(JAM_HOME_SOUND);
    dsu_defaults();
    memset(&mlu, 0, sizeof mlu);
    ml_arm = ml_heard = 0;
    song.sel = 0;
    set_engine_of(TSEL, engine_index(e));
    go_home();
    frames(20);
}

static const param_desc_t *slot_expect(const char *title, unsigned slot, unsigned id,
                                      const char *label)
{
    int16_t *vp = 0;
    const page_t *pg = &PAGES[page_named(title)];
    const param_desc_t *d = page_desc(pg, slot, &vp);
    expect(page_shown(pg) && pg->id[slot] == id && d && vp == &TSEL->p[id],
           "visible sound column targets its actual stored track parameter");
    if (label) expect(d && !strcmp(d->label, label), "sound column uses its contextual control name");
    return d;
}

static void page_open(const char *title)
{
    const page_t *want = &PAGES[page_named(title)];
    unsigned n;
    go_home();
    ui.fam_last[want->fam] = 0;
    for (n = 0; n < NPAGES; n++) {
        open_family(want->fam);
        frame();
        if (cur_page() == want) return;
    }
    fprintf(stderr, "Could not navigate to %s for %s\n", title, ENGINES[TSEL->eng_req]->name);
    exit(2);
}

static void capture(const char *name)
{
    ui.msg_t = 0;                    /* show the page instead of a stale learn hint */
    ui.force = 1;
    frame();
    ppm(name);
}

static void hint_expect(const char *expected)
{
    expect(strlen(expected) < sizeof ui.msg && ui.msg_t && !strcmp(ui.msg, expected),
           "hidden modulation knob explains its condition in a complete on-screen hint");
}

static void test_fm_envelope(void)
{
    static const unsigned destinations[] = {P_LD_PIT, P_LD_FLT, P_LD_SHP, P_LD_AMP};
    sound_state_t before;
    unsigned k;
    melodic_setup(&ENG_FM6);
    TSEL->p[P_ATK] = 27; TSEL->p[P_DEC] = 65;
    TSEL->p[P_SUS] = 94; TSEL->p[P_REL] = 38;
    TSEL->p[P_ED_FLT] = 19; TSEL->p[P_ED_SHP] = -22;
    TSEL->p[P_ED_PIT] = 7; TSEL->p[P_ESYNC] = 1;
    TSEL->p[P_E4] = 0;
    state_read(&before);
    tap(B_ENV);
    expect(!ui.home && !strcmp(cur_page()->title, "FM ENV") &&
               cur_page()->scope == SC_TRACK && cur_page()->graph == GR_NONE,
           "FM6 ENV enters its working controls without a misleading ADSR drawing");
    expect(!page_shown(&PAGES[page_named("ENV")]) && !page_shown(&PAGES[page_named("ENV DEST")]),
           "FM6 does not offer the generic ENV or ENV DEST screens");
    state_expect(&before, "entering FM ENV preserves old ADSR, hidden routes and the complete jam");
    slot_expect("FM ENV", 0, P_E4, "TIME");
    slot_expect("FM ENV", 1, P_ED_PIT, "PITCH");
    mlu.on = 1;
    turn(EN_K1, 1);
    expect(TSEL->p[P_E4] > 0 && ui.lock_par == P_E4 && mlu.pick && mlu.id == P_E4 && ml_arm,
           "FM ENV TIME edits real FM6 time and selects the same parameter for locks/MIDI learn");
    before.p[0][P_E4] = TSEL->p[P_E4];
    state_expect(&before, "FM ENV TIME leaves generic envelope values and every neighboring control untouched");
    turn(EN_K2, 1);
    expect(TSEL->p[P_ED_PIT] > 7 && ui.lock_par == P_ED_PIT && mlu.id == P_ED_PIT,
           "FM ENV PITCH edits the engine's fixed pitch envelope and retains MIDI/lock targeting");
    before.p[0][P_ED_PIT] = TSEL->p[P_ED_PIT];
    mlu.pick = ml_arm = 0;
    ui.lock_par = P_REL;
    ui.msg_t = 0;
    for (k = 2; k < 4; k++) turn(EN_K1 + k, 7);
    expect(ui.lock_par == P_REL && !mlu.pick && !ml_arm,
           "empty FM ENV columns cannot choose a lock or MIDI learn target");
    state_expect(&before, "empty FM ENV knobs cannot change saved sound values");
    expect(!ui.msg_t, "empty FM ENV columns do not display an irrelevant modulation warning");
    memset(&mlu, 0, sizeof mlu); ml_arm = 0;
    tap(B_ENV); turn(EN_SELECT, 3);
    expect(!strcmp(cur_page()->title, "FM ENV"), "repeated ENV or page walking cannot reach FM6's inactive generic pages");
    before.g[G_BPM] = song.g[G_BPM];
    state_expect(&before, "FM ENV navigation does not normalize or erase hidden stored parameters");
    capture("fm6-envelope");

    page_open("LFO DEST");
    for (k = 0; k < 4; k++) {
        slot_expect("LFO DEST", k, destinations[k], 0);
        int16_t was = TSEL->p[destinations[k]];
        turn(EN_K1 + k, was < 0 ? -1 : 1);
        expect(TSEL->p[destinations[k]] != was, "FM6 retains all four working LFO destinations");
    }
    capture("fm6-lfo-destinations");
}

static void test_route_visibility(void)
{
    static const engine_t *const engines[] = {&ENG_ANALOG, &ENG_DIGITAL, &ENG_PHASE, &ENG_LOFI,
        &ENG_SAMPLE, &ENG_FORMANT, &ENG_TRIO, &ENG_DRAWBAR, &ENG_GRAIN, &ENG_PHYS, &ENG_NOISE};
    static const unsigned ids[] = {P_ED_FLT, P_ED_PIT, P_ED_SHP, P_ESYNC,
                                    P_LD_PIT, P_LD_FLT, P_LD_SHP, P_LD_AMP};
    unsigned e, k;
    for (e = 0; e < NELEM(engines); e++) {
        sound_state_t before;
        melodic_setup(engines[e]);
        if (engines[e] == &ENG_ANALOG || engines[e] == &ENG_TRIO || engines[e] == &ENG_DIGITAL)
            TSEL->p[P_E0] = 0;
        for (k = 0; k < NELEM(ids); k++) TSEL->p[ids[k]] = ids[k] == P_ESYNC ? 1 : 13 + k;
        state_read(&before);
        tap(B_ENV);
        expect(!strcmp(cur_page()->title, "ENV") && cur_page()->graph == GR_ADSR &&
                   !page_shown(&PAGES[page_named("FM ENV")]),
               "ordinary engines retain their working ADSR page and do not offer FM ENV");
        tap(B_ENV);
        expect(!strcmp(cur_page()->title, "ENV DEST"), "ordinary ENV navigation reaches ENV DEST");
        state_expect(&before, "entering ordinary ENV pages does not change any stored sound values");
        for (k = 0; k < NELEM(ids); k++) {
            const char *title = k < 4 ? "ENV DEST" : "LFO DEST";
            const unsigned slot = k % 4;
            const page_t *pg = &PAGES[page_named(title)];
            int16_t *vp = 0;
            const param_desc_t *d;
            int hidden = (engines[e] == &ENG_DRAWBAR &&
                             (ids[k] == P_ED_FLT || ids[k] == P_ED_SHP || ids[k] == P_LD_FLT || ids[k] == P_LD_SHP)) ||
                         ((engines[e] == &ENG_SAMPLE || engines[e] == &ENG_ANALOG ||
                           engines[e] == &ENG_LOFI || engines[e] == &ENG_PHYS || engines[e] == &ENG_TRIO) &&
                             (ids[k] == P_ED_SHP || ids[k] == P_LD_SHP));
            page_open(title);
            d = page_desc(pg, slot, &vp);
            expect(hidden ? !d && !vp : d && vp == &TSEL->p[ids[k]],
                   "reviewed routes are hidden only when the selected engine cannot use them");
            if (!hidden) continue;
            ui.lock_par = P_REL;
            mlu.on = 1; mlu.pick = ml_arm = 0;
            turn(EN_K1 + slot, 3);
            hint_expect(engines[e] == &ENG_ANALOG ? "SHAPE NEEDS PWM WAVE" :
                        engines[e] == &ENG_TRIO ? "PWM: SELECT PULSE WAVE" : "UNUSED FOR THIS SOUND");
            expect(ui.lock_par == P_REL && !mlu.pick && !ml_arm,
                   "hidden modulation knobs neither select MIDI learn nor a step-lock target");
            state_expect(&before, "hidden knob turns preserve the old amount and every saved pattern/sound field");
            memset(&mlu, 0, sizeof mlu); ml_arm = 0;
        }
        if (engines[e] == &ENG_DRAWBAR) {
            page_open("ENV");
            turn(EN_K1, 1);
            expect(TSEL->p[P_ATK] != before.p[0][P_ATK], "organ retains its working amplitude attack control");
            capture("organ-envelope");
            page_open("ENV DEST"); capture("organ-envelope-destinations");
            page_open("LFO DEST"); capture("organ-lfo-destinations");
        }
        if (engines[e] == &ENG_SAMPLE) capture("sample-lfo-destinations");
    }
}

static void test_engine_and_wave_changes(void)
{
    sound_state_t before;
    int16_t *vp;
    melodic_setup(&ENG_ANALOG);
    TSEL->p[P_ED_SHP] = 27; TSEL->p[P_LD_SHP] = -16;
    page_open("ENV DEST");
    for (unsigned wave = 0; wave <= 4; wave++) {
        TSEL->p[P_E0] = wave;
        state_read(&before);
        frame();
        const param_desc_t *d = page_desc(cur_page(), 2, &vp);
        expect(wave == 4 ? d && vp == &TSEL->p[P_ED_SHP] : !d && !vp,
               "Analog shape envelope is visible exactly when PWM is selected");
        state_expect(&before, "changing waveform visibility never resets the saved modulation amounts");
    }
    page_open("LFO DEST");
    TSEL->p[P_E0] = 4; frame();
    expect(page_desc(cur_page(), 2, &vp) && vp == &TSEL->p[P_LD_SHP], "Analog PWM retains working shape LFO");
    capture("analog-pwm-lfo-destinations");
    TSEL->p[P_E0] = 0; state_read(&before); frame();
    turn(EN_K3, 9);
    hint_expect("SHAPE NEEDS PWM WAVE");
    state_expect(&before, "non-PWM shape LFO remains stored while its knob is hidden");
    capture("analog-saw-lfo-destinations");

    melodic_setup(&ENG_FM6); tap(B_ENV);
    TSEL->eng_req = (uint8_t)engine_index(&ENG_ANALOG);
    state_read(&before); frame();
    expect(page_shown(cur_page()) && cur_page()->fam == FAM_ENV && strcmp(cur_page()->title, "FM ENV"),
           "changing away from FM6 immediately repairs the now-inapplicable envelope page");
    state_expect(&before, "page repair after an engine change never changes stored parameters");
    TSEL->eng_req = ENGI_FM6;
    state_read(&before); frame();
    expect(!strcmp(cur_page()->title, "FM ENV"), "changing into FM6 repairs generic ENV into FM ENV");
    state_expect(&before, "repairing generic ENV into FM ENV preserves old generic envelope values");

}

static void test_conditional_modes_and_labels(void)
{
    /* These case lists record the DSP audit, separately from its compact
     * firmware bit masks. Switching cases must leave their saved amounts alone. */
    static const uint8_t trio_shape[16] = {0,1,1,0,0,0,1,1,0,0,1,0,0,0,0,1};
    static const struct {
        const engine_t *engine;
        const char *cutoff, *shape;
    } labels[] = {{&ENG_DIGITAL, "IDX1", "IDX2"}, {&ENG_PHASE, "DEP1", "DEP2"},
                  {&ENG_FM6, "MOD", "FDBK"}};
    sound_state_t before;
    unsigned i, mode, source;
    int16_t *vp;
    for (i = 0; i < 2; i++) {
        melodic_setup(i ? &ENG_TRIO : &ENG_DIGITAL);
        TSEL->p[P_ED_FLT] = 17; TSEL->p[P_ED_SHP] = -23;
        TSEL->p[P_LD_FLT] = -14; TSEL->p[P_LD_SHP] = 26;
        for (mode = 0; mode < (i ? 16u : 8u); mode++) {
            TSEL->p[P_E0] = mode;
            state_read(&before);
            for (source = 0; source < 2; source++) {
                unsigned cut_slot = source ? 1 : 0;
                page_open(source ? "LFO DEST" : "ENV DEST");
                const param_desc_t *cut = page_desc(cur_page(), cut_slot, &vp);
                expect(i || mode != 7 ? cut && vp : !cut && !vp,
                       "Digital parallel-sine mode hides timbre buses while TRIO keeps its actual filter");
                const param_desc_t *shape = page_desc(cur_page(), 2, &vp);
                unsigned has_shape = i ? trio_shape[mode] : mode != 7;
                expect(has_shape ? shape && vp : !shape && !vp,
                       "TRIO waves and Digital algorithms expose only DSP-supported shape modulation");
                if (!has_shape) {
                    turn(EN_K3, 3);
                    hint_expect(i ? "PWM: SELECT PULSE WAVE" : "ALG 8: NO FM MOD");
                }
            }
            state_expect(&before, "conditional route visibility and ignored knobs preserve every saved amount");
        }
    }
    for (i = 0; i < NELEM(labels); i++) {
        melodic_setup(labels[i].engine);
        if (labels[i].engine == &ENG_DIGITAL) TSEL->p[P_E0] = 0;
        page_open("LFO DEST");
        slot_expect("LFO DEST", 1, P_LD_FLT, labels[i].cutoff);
        slot_expect("LFO DEST", 2, P_LD_SHP, labels[i].shape);
        if (labels[i].engine == &ENG_PHASE) capture("phase-lfo-destinations");
        if (labels[i].engine != &ENG_FM6) {
            page_open("ENV DEST");
            slot_expect("ENV DEST", 0, P_ED_FLT, labels[i].cutoff);
            slot_expect("ENV DEST", 2, P_ED_SHP, labels[i].shape);
            if (labels[i].engine == &ENG_DIGITAL) capture("digital-envelope-destinations");
        }
    }
    for (i = 0; i < 2; i++) {
        melodic_setup(i ? &ENG_PHYS : &ENG_NOISE);
        for (mode = 0; mode <= (unsigned)ENGINES[TSEL->eng_req]->edit[0].max; mode++) {
            TSEL->p[P_E0] = mode;
            state_read(&before);
            for (source = 0; source < 2; source++) {
                page_open(source ? "LFO DEST" : "ENV DEST");
                const param_desc_t *d = page_desc(cur_page(), 2, &vp);
                expect(i ? !d && !vp : d && vp,
                       "PHYS has no shape route in any model; Noise retains its working mode-dependent route");
            }
            state_expect(&before, "audited Noise/PHYS model changes do not normalize hidden modulation amounts");
        }
    }
}

static void test_performance_ownership(void)
{
    sound_state_t before;
    unsigned k;
    melodic_setup(&ENG_FM6);
    tap(B_SEQ);
    expect(song.seq_mode, "melodic SEQ owns step entry before returning to sound editing");
    state_read(&before);
    tap(B_ENV);
    expect(!song.seq_mode && !kb_grid && !strcmp(cur_page()->title, "FM ENV"),
           "ENV releases sequencer key ownership before the first playable key");
    fm1_in.notes = 1u << key_of_white(4); frame();
    expect(kb_kind[key_of_white(4)] == KS_NOTE, "first key on FM ENV plays the instrument");
    state_expect(&before, "first playable key after ENV cannot accidentally write the sequencer");
    fm1_in.notes = 0; frame();

    tap(B_SAVE);
    expect(ui.recovery_screen == 1 && jam_recovery_ui_modal && !recovery_calls,
           "SAVE still opens the explicit manual checkpoint modal from FM ENV");
    state_read(&before);
    for (k = 0; k < 4; k++) turn(EN_K1 + k, 9);
    tap(B_ENV); tap(B_LFO);
    expect(ui.recovery_screen == 1 && !recovery_calls, "manual Save modal owns navigation before hidden sound controls");
    state_expect(&before, "turning knobs under manual Save cannot edit the hidden melodic sound");
    tap(B_SAVE);
    expect(recovery_calls == 1, "only a deliberate Save now invokes the checkpoint writer");
    tap(B_HOME);
    tap(B_ENV);
    rec_wait = ci_on = 1;
    state_read(&before);
    for (k = 0; k < 4; k++) turn(EN_K1 + k, 2);
    state_expect(&before, "count-in owns knobs ahead of FM ENV and leaves sound values alone");
    rec_wait = ci_on = 0;
    press(B_SCL);
    key(0);
    release(B_SCL);
    expect(ui.layer == LY_PLAY && ly_lock == LY_PLAY && trk[0].p[P_ROOT] == 5 &&
               trk[1].p[P_ROOT] == 5 && trk[2].p[P_ROOT] == 5,
           "held SEL still sets the song key for all melodic tracks and returns to ordinary play");

    melodic_setup(&ENG_DRAWBAR);
    page_open("FILTER");
    /* FAM_EDIT also has an Analog-only FILTER. Use the FX family explicitly. */
    open_family(FAM_FX);
    while (cur_page()->id[0] != P_TFLT) open_family(FAM_FX);
    TSEL->p[P_TFLT] = 0;
    turn(EN_K1, -1);
    expect(TSEL->p[P_TFLT] < 0, "organ's working track FX filter remains editable despite hiding unsupported synth-filter routes");
}

static void test_drum_trial05_preserved(void)
{
    dsu_bank_t before;
    setup(JAM_HOME_GROOVE); dsu_defaults();
    TDRUM->p[P_E0] = DRUM_SAMPLED + 2;
    before = dsu;
    tap(B_ENV);
    expect(on_drum_page() && drum_page == 2 && !kb_grid, "Drums ENV retains Trial05's playable DRUM SOUND screen");
    tap(B_ENV);
    expect(on_drum_page() && drum_page == 3, "Drums ENV retains explicit factory-kit copy confirmation");
    turn(EN_K1, 2);
    tap(B_HOME);
    expect(!memcmp(&before, &dsu, sizeof dsu), "canceling drum copy still preserves all saved SYN kits");
    tap(B_ENV); tap(B_ENV); turn(EN_K1, 2); tap(B_ENV);
    expect(drum_page == 2 && drum_kit() == DRUM_SYN + 2 &&
               !memcmp(dsu.k[2].s, DS_KITS[2].s, sizeof dsu.k[2].s),
           "confirmed drum copy still selects and copies the chosen SYN bank exactly");
    key(key_of_white(2));
    expect(pen_lane == 2 && !kb_grid, "first DRUM SOUND pad still selects the played drum");
    tap(B_SEQ);
    expect(on_drum_page() && drum_page == 0 && kb_grid && drum_lane == 2, "Drums SEQ still returns to the selected drum's step grid");
    tap(B_SEQ);
    expect(drum_lane == 10 && drum_page == 0, "repeated Drums SEQ still swaps the two eight-row banks");
    tap(B_EDIT);
    expect(on_drum_page() && drum_page == 1 && !kb_grid, "Drums EDIT still opens whole-kit sound editing");
    expect(!recovery_calls, "melodic sound-page cleanup introduces no automatic drum saving");
}

int main(int argc, char **argv)
{
    outdir = argc > 1 ? argv[1] : "build/jam-sound-pages-tests/screens";
    test_fm_envelope();
    test_route_visibility();
    test_engine_and_wave_changes();
    test_conditional_modes_and_labels();
    test_performance_ownership();
    test_drum_trial05_preserved();
    printf("sound-pages: %u checks, %d failures\n", checks, fails);
    return fails ? 1 : 0;
}
