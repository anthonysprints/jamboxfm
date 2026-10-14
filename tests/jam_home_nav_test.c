/* SPDX-License-Identifier: GPL-3.0-only */
/* Drive HOME ownership through the production browser panel adapter. No
 * synthetic HOME handlers: physical events enter ui_input + keyboard_block. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../firmware/host/jam_emu.c"

static unsigned checks, failures;
#define CHECK(c, message) do { checks++; if (!(c)) { failures++; fprintf(stderr, "FAIL: %s (line %d)\n", message, __LINE__); } } while (0)

static void advance_ms(uint32_t ms)
{
    uint32_t start = fm1_ms;
    do { jam_emu_render(); } while (fm1_ms - start < ms);
    jam_emu_frame();
}

static void tap(uint32_t button)
{
    jam_emu_button(button, 1);
    jam_emu_button(button, 0);
    jam_emu_frame();
}

/* The hardware samples several edges at once. This preserves that ordering
 * for HOME-release + PLAY-press, which separate adapter calls cannot model. */
static void button_edges(uint32_t down, uint32_t up)
{
    uint32_t role, before = fm1_in.buttons;
    for (role = 0; role < NB; role++) {
        uint32_t bit = 1u << panel.btn[role];
        if (down & (1u << role)) fm1_in.buttons |= bit;
        if (up & (1u << role)) fm1_in.buttons &= ~bit;
    }
    emu_button_edges |= fm1_in.buttons & ~before;
    emu_input();
}

static void settled_home(void)
{
    jam_emu_release();
    if (ui.menu) menu_close();
    ly_lock = LY_PLAY;
    ui.hold_kind = 0;
    jam_emu_panic(); advance_ms(30);
    rec_wait = ft_on = ci_on = 0;
    assert(jam_new_dusty());
    rec_count = rec_tempo = 0;
    go_home(); advance_ms(300);
}

static void select_view(uint32_t view)
{
    assert(jam_home_active());
    if (ui.jam_home_view != view) tap(B_HOME);
    assert(ui.jam_home_view == view);
}

typedef struct {
    step_t melodic[NPART][NSTEP];
    dstep_t drum[NSTEP];
    int16_t params[NTRK][P_COUNT];
    uint8_t selected;
    int8_t octave;
    int16_t tempo;
} musical_state_t;

static void musical_state(musical_state_t *s)
{
    uint32_t i;
    memset(s, 0, sizeof *s);
    for (i = 0; i < NPART; i++) memcpy(s->melodic[i], trk[i].step, sizeof s->melodic[i]);
    memcpy(s->drum, TDRUM->dstep, sizeof s->drum);
    for (i = 0; i < NTRK; i++) memcpy(s->params[i], trk[i].p, sizeof s->params[i]);
    s->selected = song.sel; s->octave = song.octave; s->tempo = song.g[G_BPM];
}

static uint32_t gated(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++) n += t->v[i].active && t->v[i].gate;
    return n;
}

static void test_basic_routes(void)
{
    uint32_t view, k;
    static const uint8_t pages[] = {B_ENV, B_LFO, B_FX, B_SCL, B_EDIT, B_SEQ, B_ARP, B_SAVE};
    CHECK(ui.home == 1 && ui.jam_home_view == JAM_HOME_GROOVE && jam_home_active(),
          "cold boot selects ordinary Groove Home");
    for (view = JAM_HOME_GROOVE; view <= JAM_HOME_SOUND; view++) {
        settled_home(); select_view(view);
        go_home(); go_home(); jam_emu_frame();
        CHECK(ui.home == 1 && ui.jam_home_view == view && !song.seq_mode,
              "go_home returns idempotently without toggling the remembered view");
        jam_emu_button(B_HOME, 1);
        CHECK(ui.jam_home_view == view, "HOME press alone does not switch views");
        jam_emu_button(B_HOME, 0); jam_emu_frame();
        CHECK(ui.home == 1 && ui.jam_home_view == (view ^ 1u) && !song.seq_mode && !kb_grid,
              "ordinary HOME release switches once while retaining playable Home scope");
        for (k = 0; k < 4u; k++) jam_emu_frame();
        CHECK(ui.jam_home_view == (view ^ 1u), "idle frames do not repeat a HOME release");
        tap(B_HOME);
        CHECK(ui.jam_home_view == view, "the next HOME tap switches back");
        for (k = 0; k < sizeof pages; k++) {
            tap(pages[k]);
            CHECK(!ui.home, "ordinary page button leaves Home");
            tap(B_HOME);
            CHECK(ui.home == 1 && ui.jam_home_view == view && !song.seq_mode,
                  "HOME returns from deep and sequencer pages without switching again");
        }
        studio_open(SC_SONG); jam_emu_frame();
        CHECK(on_song_page(), "song editor fixture is active");
        tap(B_HOME);
        CHECK(ui.home == 1 && ui.jam_home_view == view, "song editor HOME restores remembered view");
        track_select(TRK_DRUM); studio_open(SC_DRUM); jam_emu_frame();
        CHECK(on_drum_page(), "drum editor fixture is active");
        tap(B_HOME);
        CHECK(ui.home == 1 && ui.jam_home_view == view, "drum editor HOME restores remembered view");
        studio_open(SC_DRUM); track_select(1); jam_emu_frame();
        CHECK(ui.home == 1 && ui.jam_home_view == view,
              "automatic drum-editor exit restores the remembered view without toggling");
        assert(jam_new_dusty()); advance_ms(30);
        CHECK(ui.jam_home_view == view, "a fresh musical session preserves the powered-session Home preference");
        go_home();
        jam_emu_encoder(EN_PRESET, 1); advance_ms(30);
        CHECK(ui.jam_home_view == view, "changing sound preserves the Home preference");
    }
}

static void test_menu_ownership(void)
{
    uint32_t view;
    for (view = JAM_HOME_GROOVE; view <= JAM_HOME_SOUND; view++) {
        settled_home(); select_view(view);
        jam_emu_button(B_HOME, 1); advance_ms(750);
        CHECK(ui.menu && ui.jam_home_view == view, "HOME hold opens settings without switching Home");
        jam_emu_button(B_HOME, 0); jam_emu_frame();
        CHECK(ui.menu && ui.jam_home_view == view, "release after HOME hold stays consumed");
        tap(B_HOME);
        CHECK(!ui.menu && ui.home == 1 && ui.jam_home_view == view,
              "short HOME in settings closes to remembered Home without a second toggle");
        CHECK(!strcmp(ui.msg, "SETTINGS IN RAM ONLY"), "short HOME settings exit uses menu_close");
        tap(B_ENV);
        jam_emu_button(B_HOME, 1); advance_ms(750); jam_emu_button(B_HOME, 0);
        CHECK(ui.menu, "settings can cover a deep page");
        tap(B_HOME);
        CHECK(!ui.menu && ui.home == 1 && ui.jam_home_view == view,
              "settings opened from a deep page still return to remembered Home");
        jam_emu_button(B_HOME, 1); advance_ms(750); jam_emu_button(B_HOME, 0);
        jam_emu_button(B_HOME, 1); advance_ms(750);
        CHECK(!ui.menu && ui.jam_home_view == view, "the existing HOME hold also closes settings");
        jam_emu_button(B_HOME, 0); jam_emu_frame();
        CHECK(ui.jam_home_view == view, "release after closing settings by hold does not toggle");
    }
}

static void test_layer_ownership(void)
{
    uint32_t view;
    for (view = JAM_HOME_GROOVE; view <= JAM_HOME_SOUND; view++) {
        settled_home(); select_view(view);
        jam_emu_button(B_SEQ, 1); advance_ms(160);
        tap(B_HOME); jam_emu_button(B_SEQ, 0); jam_emu_frame();
        CHECK(ly_lock == LY_STEP && ui.layer == LY_STEP && ui.jam_home_view == view,
              "SEQ held plus HOME locks the editor and never toggles Home");
        tap(B_HOME);
        CHECK(ly_lock == LY_PLAY && ui.layer == LY_PLAY && ui.jam_home_view == view,
              "HOME used to unlock a layer is consumed without changing the underlying view");

        jam_emu_button(B_SEQ, 1); advance_ms(160); tap(B_HOME); jam_emu_button(B_SEQ, 0);
        jam_emu_button(B_HOME, 1); advance_ms(750);
        CHECK(ui.menu && ly_lock == LY_PLAY && ui.jam_home_view == view,
              "HOME held from a locked layer still opens settings and unlocks without toggling");
        jam_emu_button(B_HOME, 0);
        /* No idle frame between closing settings and the next HOME press:
         * the old layer-unlock gesture must already have released ownership. */
        jam_emu_button(B_HOME, 1); jam_emu_button(B_HOME, 0);
        CHECK(!ui.menu && ui.jam_home_view == view,
              "settings exit after a locked-layer hold consumes its HOME tap");
        jam_emu_button(B_HOME, 1); jam_emu_button(B_HOME, 0);
        CHECK(ui.jam_home_view == (view == JAM_HOME_GROOVE ? JAM_HOME_SOUND : JAM_HOME_GROOVE),
              "a fresh HOME tap immediately after settings exit is not swallowed by the old layer unlock");
        tap(B_HOME); /* restore this iteration's view */

        /* Opposite release order: the layer is gone when HOME comes up. */
        jam_emu_button(B_SEQ, 1); advance_ms(160); jam_emu_button(B_HOME, 1);
        jam_emu_button(B_SEQ, 0); jam_emu_button(B_HOME, 0); jam_emu_frame();
        CHECK(ly_lock == LY_PLAY && ui.jam_home_view == view,
              "releasing SEQ before HOME cannot turn its gesture into a view switch");
        go_home(); advance_ms(300);

        /* Layer ownership counts before its 140 ms screen delay expires. */
        jam_emu_button(B_FX, 1); jam_emu_button(B_HOME, 1);
        jam_emu_button(B_FX, 0); jam_emu_button(B_HOME, 0);
        CHECK(ui.jam_home_view == view, "an undisplayed held layer still owns its HOME gesture");
        go_home(); advance_ms(300);

        jam_emu_button(B_HOME, 1); jam_emu_button(B_SEQ, 1); advance_ms(160);
        jam_emu_button(B_SEQ, 0); jam_emu_button(B_HOME, 0);
        CHECK(ui.jam_home_view == view, "a layer entered after HOME press invalidates ordinary toggle ownership");
        go_home(); advance_ms(300);

        jam_emu_button(B_SEQ, 1); advance_ms(160); jam_emu_button(B_HOME, 1);
        button_edges(1u << B_PLAY, 1u << B_HOME); advance_ms(20);
        CHECK(song.playing && ly_lock == LY_STEP && ui.jam_home_view == view,
              "simultaneous PLAY with a layer HOME release plays without leaking a Home toggle");
        jam_emu_button(B_PLAY, 0); jam_emu_button(B_SEQ, 0);
        tap(B_HOME); tap(B_PLAY); advance_ms(20);
        CHECK(!song.playing && ui.jam_home_view == view, "locked-layer cleanup does not consume or duplicate view changes");

        jam_emu_button(B_HOME, 1); button_edges(1u << B_PLAY, 1u << B_HOME); advance_ms(20);
        CHECK(song.playing && ui.jam_home_view == (view ^ 1u),
              "ordinary simultaneous HOME-release and PLAY switches once and starts playback");
        jam_emu_button(B_PLAY, 0); tap(B_PLAY); advance_ms(20);
    }
}

static void test_record_overlays(void)
{
    uint32_t view, i;
    musical_state_t before, after;
    settled_home(); select_view(JAM_HOME_SOUND); view = ui.jam_home_view;
    tap(B_REC);
    CHECK(rec_wait && !song.playing, "physical REC arms the stopped recorder");
    tap(B_HOME);
    CHECK(rec_wait && ui.jam_home_view == view && !jam_home_active(),
          "HOME cannot change the view behind the armed recorder");
    jam_emu_button(B_HOME, 1); tap(B_REC); jam_emu_button(B_HOME, 0);
    CHECK(!rec_wait && ui.jam_home_view == view,
          "an arm ending before HOME release does not reclassify the gesture as a toggle");
    jam_emu_button(B_HOME, 1); tap(B_REC); tap(B_REC); jam_emu_button(B_HOME, 0);
    CHECK(!rec_wait && ui.jam_home_view == view,
          "a recorder entering and leaving during HOME invalidates ordinary toggle ownership");

    settled_home(); select_view(JAM_HOME_SOUND); rec_count = 1; rec_tempo = 1;
    tap(B_REC); tap(B_PLAY); advance_ms(20);
    CHECK(ci_on && rec_wait, "physical REC then PLAY starts count-in");
    tap(B_HOME);
    CHECK(ci_on && rec_wait && ui.jam_home_view == view,
          "HOME preserves count-in and the remembered view");
    tap(B_REC); advance_ms(20);

    settled_home(); select_view(JAM_HOME_SOUND);
    for (i = 0; i < NTRK; i++) steps_clear(&trk[i]);
    rec_tempo = rec_count = 0;
    tap(B_REC); jam_emu_key(10, 1); advance_ms(40);
    CHECK(ft_on && !rec_wait, "first played note starts the physical free-take workflow");
    tap(B_HOME);
    CHECK(ft_on && ui.jam_home_view == view, "HOME neither cancels a free take nor changes the hidden view");
    jam_emu_key(10, 0); tap(B_PLAY); advance_ms(20);

    settled_home(); select_view(JAM_HOME_SOUND);
    musical_state(&before);
    jam_emu_button(B_REC, 1); advance_ms(750);
    CHECK(ui.hold_kind == 1, "held REC displays its existing clear confirmation");
    tap(B_HOME); musical_state(&after);
    CHECK(ui.hold_kind == 1 && ui.jam_home_view == view && !memcmp(&before, &after, sizeof before),
          "HOME in a clear hold neither toggles views nor clears a pattern");
    jam_emu_button(B_REC, 0); jam_emu_frame();
    CHECK(!ui.hold_kind && ui.jam_home_view == view, "releasing the clear hold preserves view and session");
}

static void test_live_state(void)
{
    musical_state_t before, after;
    uint32_t view, count;
    uint8_t held[4];
    settled_home(); select_view(JAM_HOME_GROOVE);
    jam_emu_key(10, 1); advance_ms(30);
    count = kb_n[10]; memcpy(held, kb_nt[10], sizeof held);
    musical_state(&before);
    tap(B_HOME); musical_state(&after);
    CHECK(ui.jam_home_view == JAM_HOME_SOUND && count > 1 && kb_n[10] == count &&
          !memcmp(held, kb_nt[10], sizeof held) && gated(TSEL) == count && !memcmp(&before, &after, sizeof before),
          "toggling while holding a chord preserves pitches, voices, sound and every recorded step");
    jam_emu_key(10, 0); advance_ms(30);
    CHECK(!gated(TSEL), "the held chord still releases cleanly after a view switch");

    jam_emu_encoder(EN_ALGO, 1); /* Lead: single-note mode. */
    jam_emu_key(10, 1); advance_ms(30);
    view = ui.jam_home_view; musical_state(&before);
    tap(B_HOME); musical_state(&after);
    CHECK(song.sel == 2 && kb_n[10] == 1u && gated(TSEL) == 1u && ui.jam_home_view == (view ^ 1u) &&
          !memcmp(&before, &after, sizeof before),
          "toggling while holding a single lead note preserves its voice and musical state");
    jam_emu_key(10, 0); advance_ms(30);
    CHECK(!gated(TSEL), "the held single note releases after toggling");
    jam_emu_encoder(EN_ALGO, -1); /* Back to Chords. */

    tap(B_PLAY); advance_ms(30); tap(B_REC);
    CHECK(song.playing && song.rec && !rec_wait && jam_home_active(),
          "running overdub retains ordinary Home ownership");
    jam_emu_key(10, 1); advance_ms(30);
    count = kb_n[10]; memcpy(held, kb_nt[10], sizeof held);
    view = ui.jam_home_view;
    musical_state(&before); tap(B_HOME); musical_state(&after);
    CHECK(ui.jam_home_view == (view ^ 1u) && song.playing && song.rec && kb_n[10] == count &&
          !memcmp(held, kb_nt[10], sizeof held) && gated(TSEL) == count && !memcmp(&before, &after, sizeof before),
          "a HOME tap during overdub changes only the view, retaining recorded data and held voices");
    advance_ms(40);
    CHECK(song.playing && song.rec && gated(TSEL) == count,
          "audio and recording continue after the overdub view switch");
    jam_emu_key(10, 0); tap(B_REC); tap(B_PLAY); advance_ms(30);
    CHECK(!song.playing && !song.rec && !gated(TSEL), "normal release and transport controls still finish the take");
}

int main(void)
{
    jam_emu_init();
    test_basic_routes();
    test_menu_ownership();
    test_layer_ownership();
    test_record_overlays();
    test_live_state();
    CHECK(!jam_emu_value(18, 0), "all tested navigation draws remain inside the 240 by 240 LCD");
    printf("jam-home-nav: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
