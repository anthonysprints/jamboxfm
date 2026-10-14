/* SPDX-License-Identifier: GPL-3.0-only */
/* Compile the production browser adapter natively and exercise its public ABI. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "../firmware/host/jam_emu.c"

static unsigned checks;
#define CHECK(condition, message) do { assert(condition); checks++; puts(message); } while (0)
static void tap_emu(uint32_t role)
{
    jam_emu_button(role, 1);
    jam_emu_button(role, 0);
    jam_emu_render();
    jam_emu_frame();
}
static void blocks(unsigned n) { while (n--) jam_emu_render(); }

static void test_snap_lights(void)
{
    unsigned i;
    track_t *t = &trk[2];
    jam_emu_panic(); jam_emu_render();
    assert(jam_new_dusty());
    song.sel = 2;
    t->p[P_ROOT] = 3; t->p[P_SCALE] = 1; /* D# major. */
    t->p[P_QUANT] = 1; t->p[P_CHORD] = 0;
    go_home(); blocks(400);

    jam_emu_key(10, 1); /* D#4 is already in the scale. */
    CHECK(kb_nt[10][0] == 63 && jam_emu_value(14, 0) == 1,
          "SNAP D# plays one unchanged note");
    for (i = 0; i < 10; i++) {
        jam_emu_render(); jam_emu_frame();
        assert(jam_emu_leds()[24] == 255 && jam_emu_leds()[25] == 0);
    }
    CHECK(1, "SNAP D# lights only D#, through the note-hit flash and sustained voice");
    jam_emu_key(10, 0); blocks(400);
    CHECK(!jam_emu_value(14, 0) && jam_emu_leds()[24] == 0 && jam_emu_leds()[25] == 0,
          "SNAP note and lights release cleanly");

    jam_emu_key(11, 1); jam_emu_render(); jam_emu_frame(); /* E4 -> D#4. */
    CHECK(kb_nt[11][0] == 63 && jam_emu_value(14, 0) == 1 &&
          jam_emu_leds()[24] == 255 && jam_emu_leds()[25] == 255,
          "SNAP E lights the pressed E and sounding D# for one note");
    jam_emu_key(11, 0); blocks(400);

    /* A note entirely between UI frames must still flash its actual pitch. */
    trk_note_on(t, 63, 100); trk_note_off(t, 63);
    jam_emu_frame(); jam_emu_frame();
    CHECK(!fm1_in.notes && jam_emu_leds()[24] == 255 && jam_emu_leds()[25] == 0,
          "short sequencer notes flash D# without the unpressed E alias");
    for (i = 0; i < 8; i++) jam_emu_frame();
    CHECK(jam_emu_leds()[24] == 0 && jam_emu_leds()[25] == 0,
          "short-note lights expire after their display window");

    t->p[P_TRANS] = 2;
    jam_emu_key(10, 1); jam_emu_render(); jam_emu_frame();
    CHECK(kb_nt[10][0] == 65 && jam_emu_leds()[24] == 255 &&
          jam_emu_leds()[26] == 255 && jam_emu_leds()[25] == 0,
          "transposed SNAP lights the pressed D# and actual sounding F");
    jam_emu_key(10, 0); blocks(400);
    t->p[P_TRANS] = 0; song.octave = 1;
    jam_emu_key(10, 1); jam_emu_render(); jam_emu_frame();
    CHECK(kb_nt[10][0] == 75 && jam_emu_leds()[24] == 255 && jam_emu_leds()[25] == 0,
          "SNAP lights follow the selected octave without aliases");
    jam_emu_key(10, 0); blocks(400);

    song.octave = 0; t->p[P_ROOT] = 4; /* E major: lowest F3 snaps below the keyboard. */
    jam_emu_key(0, 1); jam_emu_render(); jam_emu_frame();
    CHECK(kb_nt[0][0] == 52 && jam_emu_leds()[14] == 255 && !keys_sounding(t),
          "SNAP notes outside the visible register do not wrap to another octave");
    jam_emu_key(0, 0); jam_emu_panic(); jam_emu_render();
}

static uint32_t emu_key_lights(void)
{
    uint32_t k, mask = 0;
    for (k = 0; k < 27u; k++)
        if (jam_emu_leds()[14u + k] == 255)
            mask |= 1u << k;
    return mask;
}

static void light_frames(uint32_t n)
{
    while (n--) { jam_emu_render(); jam_emu_frame(); }
}

static void test_chord_tone_lights(void)
{
    uint32_t i;
    const uint32_t ds_major = (1u << 10) | (1u << 14) | (1u << 17);
    const uint32_t f_minor = (1u << 12) | (1u << 15) | (1u << 19);
    track_t *t = &trk[1];
    jam_emu_panic(); jam_emu_render();
    assert(jam_new_dusty());
    song.sel = 1;
    t->p[P_ROOT] = 3; t->p[P_SCALE] = 1; /* D# major. */
    t->p[P_QUANT] = 1; t->p[P_CHORD] = 1;
    t->p[P_STRUM] = t->p[P_VLEAD] = t->p[P_AMODE] = 0;
    t->p[P_VOICE] = V_POLY;
    go_home(); blocks(400);

    jam_emu_key(10, 1);
    CHECK(kb_n[10] == 3 && kb_root[10] == 63,
          "D# in D# major starts a chord from its printed root");
    for (i = 0; i < 10u; i++) {
        light_frames(1);
        assert(emu_key_lights() == ds_major);
    }
    CHECK(1, "live SNAP chord lights D#, G and A# without the unpressed E alias");
    jam_emu_key(10, 0); light_frames(8);
    CHECK(!keys_sounding(t) && !emu_key_lights(),
          "released chord lights clear after the short hit-flash window");

    jam_emu_key(11, 1);
    for (i = 0; i < 10u; i++) {
        light_frames(1);
        assert(emu_key_lights() == (ds_major | (1u << 11)));
    }
    CHECK(kb_n[11] == 3 && kb_root[11] == 63,
          "SNAP chord from E lights pressed E plus the sounding D#, G and A#");
    trk_note_on(t, 65, 100); light_frames(1);
    CHECK(emu_key_lights() == (ds_major | (1u << 11) | (1u << 12)),
          "simultaneous playback adds its actual F pitch beside a live chord");
    jam_emu_key(11, 0); light_frames(8);
    CHECK(emu_key_lights() == (1u << 12),
          "releasing the live chord leaves the independent playback note lit");
    trk_note_off(t, 65); light_frames(8);
    CHECK(!emu_key_lights(), "playback lights clear after note-off and flash expiry");

    jam_emu_key(10, 1); jam_emu_key(10, 0);
    light_frames(2);
    CHECK(emu_key_lights() == ds_major,
          "a chord tapped between UI frames flashes all three actual pitches");
    light_frames(8);
    CHECK(!emu_key_lights(), "brief chord flashes expire without sticking");

    trk_note_on(t, 63, 100); trk_note_off(t, 63); light_frames(2);
    CHECK(emu_key_lights() == (1u << 10), "short playback note populates this part's flash cache");
    song.sel = 2; jam_emu_frame();
    CHECK(!emu_key_lights(), "switching parts never borrows the previous part's flash, even for one frame");
    song.sel = 1; light_frames(8);

    t->p[P_TRANS] = 2;
    jam_emu_key(10, 1); light_frames(2);
    CHECK(kb_root[10] == 65 && emu_key_lights() == (f_minor | (1u << 10)),
          "transposed chord lights pressed D# plus sounding F, G# and C");
    t->p[P_TRANS] = 0; light_frames(2);
    CHECK(emu_key_lights() == (f_minor | (1u << 10)),
          "changing transpose while held keeps the actual held pitches lit");
    jam_emu_key(10, 0); light_frames(8);
    song.octave = 1;
    jam_emu_key(10, 1); light_frames(2);
    CHECK(kb_root[10] == 75 && emu_key_lights() == ds_major,
          "all chord lights follow the selected keyboard octave without aliases");
    jam_emu_key(10, 0); light_frames(8); song.octave = 0;

    t->p[P_QUANT] = 0; t->p[P_CHORD] = 6;
    jam_emu_key(11, 1); light_frames(2);
    CHECK(kb_root[11] == 64 && emu_key_lights() == ((1u << 11) | (1u << 15) | (1u << 18)),
          "QNT OFF fixed major chord lights E, G# and B");
    jam_emu_key(11, 0); light_frames(8);
    t->p[P_QUANT] = 1; t->p[P_ROOT] = 4; t->p[P_CHORD] = 1;
    jam_emu_key(0, 1); light_frames(2);
    CHECK(kb_root[0] == 52 && emu_key_lights() == (1u | (1u << 3) | (1u << 6)),
          "a root below the keyboard still lights visible G# and B chord tones without wrapping");
    jam_emu_key(0, 0); light_frames(8);

    t->p[P_ROOT] = 3; t->p[P_VLEAD] = 2;
    jam_emu_key(10, 1); light_frames(2);
    CHECK(emu_key_lights() == ((1u << 10) | (1u << 14) | (1u << 17) | (1u << 22)),
          "first inversion lights G, A# and upper D# alongside the pressed lower D#");
    jam_emu_key(10, 0); light_frames(8);
    t->p[P_VLEAD] = 0; t->p[P_STRUM] = 60;
    jam_emu_key(10, 1); light_frames(2);
    CHECK(emu_key_lights() == (1u << 10),
          "a strummed chord initially lights only its sounding root");
    blocks((FS * 65u / 1000u + CTL - 1u) / CTL); jam_emu_frame();
    CHECK(emu_key_lights() == ((1u << 10) | (1u << 14)),
          "strum lights the third when that note starts, before the fifth");
    blocks((FS * 65u / 1000u + CTL - 1u) / CTL); jam_emu_frame();
    CHECK(emu_key_lights() == ds_major, "strum lights the fifth when it starts");
    jam_emu_key(10, 0); light_frames(8); t->p[P_STRUM] = 0;

    trk_note_on(t, 63, 100); trk_note_on(t, 67, 100); trk_note_on(t, 70, 100);
    light_frames(2);
    CHECK(emu_key_lights() == ds_major,
          "sequencer and MIDI chord voices show actual pitches with no SNAP aliases");
    trk_note_on(&trk[2], 64, 100); light_frames(2);
    CHECK(emu_key_lights() == ds_major,
          "another part's notes do not add unrelated lights to the selected part");
    trk_note_off(&trk[2], 64);
    trk_note_off(t, 63); trk_note_off(t, 67); trk_note_off(t, 70);
    light_frames(8);
    CHECK(!emu_key_lights(), "playback chord lights release after the hit flashes");

    t->p[P_QUANT] = 2;
    jam_emu_key(7, 1); light_frames(2); /* WHITE C4 -> root D#4. */
    CHECK(kb_root[7] == 63 && emu_key_lights() == (ds_major | (1u << 7)),
          "WHITE lights the pressed C and actual D#, G and A# chord pitches");
    jam_emu_key(7, 0); light_frames(8);
    jam_emu_key(10, 1); light_frames(2);
    CHECK(kb_kind[10] == KS_MOD && !jam_emu_value(14, 0) && emu_key_lights() == (1u << 10),
          "WHITE D# remains a modifier and shows its physical press");
    jam_emu_key(10, 0);
    trk_note_on(t, 63, 100); trk_note_off(t, 63); light_frames(2);
    CHECK(emu_key_lights() == (1u << 10),
          "WHITE playback flashes the actual D# pitch instead of an input-layout alias");
    light_frames(8);
    CHECK(!emu_key_lights(), "WHITE short-note flashes expire normally");

    t->p[P_QUANT] = 0; t->p[P_CHORD] = 0; t->p[P_TRANS] = 2;
    jam_emu_key(10, 1); light_frames(2);
    CHECK(emu_key_lights() == ((1u << 10) | (1u << 12)),
          "transposed single notes in OFF light both the pressed D# and sounding F");
    jam_emu_key(10, 0); light_frames(8);
    jam_emu_panic(); jam_emu_render();
}

static uint32_t emu_gated_notes(const track_t *t, uint8_t *notes)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].gate)
            notes[n++] = (uint8_t)t->v[i].note;
    sort_notes(notes, n);
    return n;
}

static void test_chord_panel_controls(void)
{
    uint32_t i;
    static const uint32_t parts[2] = {0, 2}; /* Bass and Lead. */
    uint8_t notes[NVOICE];
    for (i = 0; i < 2u; i++) {
        track_t *t = &trk[parts[i]];
        uint8_t root = i ? 63 : 39; /* Dusty Bass sounds two octaves below Lead. */
        jam_emu_panic(); jam_emu_render();
        assert(jam_new_dusty());
        go_home(); blocks(400);
        jam_emu_encoder(EN_ALGO, i ? 1 : -1);
        CHECK(song.sel == parts[i] && !t->p[P_CHORD] && t->p[P_VOICE] == V_LEGATO,
              "Bass/Lead starts with chord building OFF and its legato sound");
        tap_emu(B_SCL);
        jam_emu_encoder(EN_SELECT, -1); /* First SEL page, even if SEL 2 was remembered. */
        blocks(400); /* A tap leaves the held-layer knob quiet window. */
        CHECK(!ui.home && cur_page()->id[3] == P_CHORD,
              "physical SEL opens the chord control on knob 4");
        jam_emu_encoder(EN_K1, 3); /* C -> D#. */
        jam_emu_encoder(EN_K2, -1); /* Minor -> major. */
        if (!t->p[P_QUANT]) jam_emu_encoder(EN_K3, 1); /* Bass OFF -> SNAP. */
        jam_emu_encoder(EN_K4, 1); /* OFF -> TRIAD. */
        CHECK(t->p[P_ROOT] == 3 && t->p[P_SCALE] == 1 && t->p[P_CHORD] == 1 &&
              t->p[P_QUANT] == 1 && t->p[P_VOICE] == V_POLY,
              "SEL chord knob enables POLY on Bass/Lead without an EDIT detour");
        jam_emu_key(10, 1); blocks(16);
        CHECK(emu_gated_notes(t, notes) == 3u && notes[0] == root && notes[1] == root + 4u && notes[2] == root + 7u,
              "physical D# sounds three actual gated D# major voices on Bass/Lead");
        jam_emu_key(10, 0); blocks(16);
        CHECK(!emu_gated_notes(t, notes), "all three Bass/Lead chord voices release");

        jam_emu_encoder(EN_K4, -1);
        jam_emu_key(10, 1); blocks(16);
        CHECK(!t->p[P_CHORD] && t->p[P_VOICE] == V_POLY && emu_gated_notes(t, notes) == 1u && notes[0] == root,
              "CHORD OFF plays one note and does not silently restore a prior voice mode");
        jam_emu_key(10, 0); blocks(16);

        jam_emu_encoder(EN_K4, 6); /* OFF -> fixed MAJOR, appended ID 6. */
        jam_emu_encoder(EN_SELECT, 1);
        CHECK(cur_page()->id[2] == P_VLEAD && !strcmp(TP[P_VLEAD].label, "INVERT"),
              "SELECT reaches SEL 2 with INVERT on knob 3");
        jam_emu_encoder(EN_K3, 2); /* ROOT -> AUTO -> +1. */
        jam_emu_key(10, 1); blocks(16);
        CHECK(t->p[P_VLEAD] == 2 && emu_gated_notes(t, notes) == 3u &&
              notes[0] == root + 4u && notes[1] == root + 7u && notes[2] == root + 12u,
              "SEL 2 knob 3 first inversion sounds G, A# and D# above");
        jam_emu_key(10, 0); blocks(16);

        tap_emu(B_HOME); blocks(400);
        jam_emu_encoder(EN_PRESET, 1); blocks(400);
        CHECK(jam_sound_index(parts[i]) == 1u && t->p[P_CHORD] == 6 &&
              t->p[P_VLEAD] == 2 && t->p[P_VOICE] == V_POLY,
              "Home PRESETS preserves the Bass/Lead chord choice, inversion and POLY");
        jam_emu_key(10, 1); blocks(16);
        CHECK(emu_gated_notes(t, notes) == 3u && notes[0] == root + 4u && notes[1] == root + 7u && notes[2] == root + 12u,
              "the next curated Bass/Lead sound still plays all inverted chord tones");
        jam_emu_key(10, 0); blocks(16);
    }

    jam_emu_panic(); jam_emu_render();
    assert(jam_new_dusty());
    go_home(); blocks(400);
    jam_emu_encoder(EN_ALGO, 1); /* Lead: CHORD OFF / LEGATO. */
    jam_emu_button(B_SCL, 1);
    jam_emu_encoder(EN_K1, 6); /* Held SEL uses its own chord knob. */
    jam_emu_button(B_SCL, 0); blocks(400);
    CHECK(trk[2].p[P_CHORD] == 6 && trk[2].p[P_VOICE] == V_POLY,
          "held SEL plus knob 1 also enables full chord POLY on Lead");
    jam_emu_key(10, 1); blocks(16);
    CHECK(emu_gated_notes(&trk[2], notes) == 3u && notes[0] == 63 && notes[1] == 67 && notes[2] == 70,
          "held-SEL chord control produces three actual voices from the black D# key");
    jam_emu_key(10, 0); jam_emu_panic(); jam_emu_render();
}

static void test_bass_voice_explanation(void)
{
    track_t *t = &trk[0];
    uint8_t notes[NVOICE];
    uint32_t page;
    jam_emu_release(); jam_emu_panic(); blocks(40);
    CHECK(jam_new_dusty(), "Bass behavior starts from a fresh session");
    track_select(0); go_home(); blocks(400);
    CHECK(t->p[P_VOICE] == V_LEGATO && !t->p[P_CHORD], "Bass defaults to LEGATO with automatic chords off");
    jam_emu_key(7,1); jam_emu_key(14,1); blocks(4);
    CHECK(emu_gated_notes(t,notes) == 1u, "default Bass sounds one voice when two keys are held");
    jam_emu_release(); blocks(40);
    step_clear(&t->step[0]);
    step_add(t,0,60,100,LV_NORM,0); step_add(t,0,67,100,LV_NORM,0);
    CHECK(t->step[0].n == 1 && t->step[0].note[0] == 67, "LEGATO recording replaces an existing note on the same step");
    tap_emu(B_EDIT);
    for (page=0; page<NPAGES; page++) if (PAGES[page].fam == FAM_EDIT && PAGES[page].id[0] == P_VOICE) break;
    assert(page < NPAGES); ui.page = page; page_entered();
    jam_emu_encoder(EN_K1,-2);
    CHECK(t->p[P_VOICE] == V_POLY, "EDIT > VOICE > knob 1 selects POLY through the real page handler");
    tap_emu(B_HOME); jam_emu_key(7,1); jam_emu_key(14,1); blocks(4);
    CHECK(emu_gated_notes(t,notes) == 2u, "POLY Bass sounds two separately pressed keys together");
    jam_emu_release(); blocks(40); step_clear(&t->step[0]);
    step_add(t,0,60,100,LV_NORM,0); step_add(t,0,67,100,LV_NORM,0);
    CHECK(t->step[0].n == 2, "POLY recording keeps two different notes on the same step");
    jam_sound_load(0,1); blocks(40);
    CHECK(t->p[P_VOICE] == V_LEGATO, "browsing a Bass sound currently restores LEGATO when chords are off");
    CHECK(jam_new_dusty(), "voice investigation restores a fresh session");
}

static void test_home_views(void)
{
    uint32_t view, before, selected, rec;
    uint8_t notes[4], held;
    step_t pattern[NSTEP];
    jam_emu_panic(); jam_emu_render();
    assert(jam_new_dusty());
    go_home(); blocks(400);
    if (ui.jam_home_view != JAM_HOME_GROOVE) tap_emu(B_HOME);

    for (view = JAM_HOME_GROOVE; view <= JAM_HOME_SOUND; view++) {
        CHECK(jam_emu_value(6, 0) && jam_emu_value(20, 0) && (uint32_t)jam_emu_value(21, 0) == view,
              "both Home views retain the Home flag and normal control ownership");
        before = (uint32_t)song.g[G_BPM];
        jam_emu_encoder(EN_SELECT, 1);
        CHECK((uint32_t)song.g[G_BPM] == before + 1u, "SELECT changes one BPM in either Home view");
        jam_emu_encoder(EN_SELECT, -1);
        selected = song.sel;
        jam_emu_encoder(EN_ALGO, 1);
        CHECK(song.sel == 2u && ui.jam_home_view == view, "ALGORITHM selects Lead without switching Home views");
        jam_emu_encoder(EN_ALGO, -1);
        CHECK(song.sel == selected, "ALGORITHM returns to the same Chords part in either view");
        jam_emu_encoder(EN_PRESET, 1); blocks(400);
        CHECK(jam_sound_index(song.sel) == 1u && ui.jam_home_view == view,
              "PRESETS browses the sound while preserving the Home view");
        jam_emu_encoder(EN_PRESET, -1); blocks(400);
        before = view == JAM_HOME_GROOVE ? song.g[G_SWING] : jam_macro_get(song.sel, 0);
        jam_emu_encoder(EN_K1, before == 100u ? -1 : 1);
        CHECK((view == JAM_HOME_GROOVE ? (uint32_t)song.g[G_SWING] : jam_macro_get(song.sel, 0)) != before,
              "KNOB 1 edits Groove swing or Sound brightness according to the visible view");

        tap_emu(B_ENV);
        CHECK(!jam_emu_value(6, 0) && (uint32_t)jam_emu_value(21, 0) == view,
              "opening ENV retains the remembered Home view in the appended ABI");
        tap_emu(B_HOME);
        CHECK(jam_emu_value(6, 0) && (uint32_t)jam_emu_value(21, 0) == view,
              "HOME from ENV returns to the remembered view without toggling");
        if (view == JAM_HOME_GROOVE) tap_emu(B_HOME);
    }

    /* A menu owns the complete HOME hold/release; its short close is consumed. */
    jam_emu_button(B_HOME, 1); blocks((FS * 800u / 1000u + CTL - 1u) / CTL);
    CHECK(ui.menu && !jam_emu_value(20, 0) && ui.jam_home_view == JAM_HOME_SOUND,
          "holding HOME opens settings without secretly switching Home views");
    jam_emu_button(B_HOME, 0); jam_emu_frame();
    CHECK(ui.menu && ui.jam_home_view == JAM_HOME_SOUND, "releasing the HOME hold leaves settings open");
    tap_emu(B_HOME);
    CHECK(!ui.menu && jam_emu_value(20, 0) && ui.jam_home_view == JAM_HOME_SOUND,
          "a short HOME tap closes settings to remembered Sound without toggling");

    jam_emu_button(B_SEQ, 1); tap_emu(B_HOME); jam_emu_button(B_SEQ, 0); jam_emu_frame();
    CHECK(jam_emu_value(11, 0) == LY_STEP && !jam_emu_value(20, 0) && ui.jam_home_view == JAM_HOME_SOUND,
          "held SEQ plus HOME locks the editing layer without switching Home views");
    tap_emu(B_HOME); blocks(400);
    CHECK(jam_emu_value(11, 0) == LY_PLAY && jam_emu_value(20, 0) && ui.jam_home_view == JAM_HOME_SOUND,
          "HOME unlocking the editing layer is consumed without toggling");
    CHECK(jam_new_dusty() && ui.jam_home_view == JAM_HOME_SOUND,
          "a fresh musical session preserves the chosen Home view");
    go_home(); blocks(400);

    /* Sound, record ownership and the captured pitches survive ordinary toggles. */
    tap_emu(B_PLAY); tap_emu(B_REC);
    CHECK(song.playing && song.rec && jam_emu_value(20, 0), "overdubbing keeps ordinary Home controls available");
    jam_emu_key(10, 1); blocks(16);
    rec = song.rec; held = kb_n[10]; memcpy(notes, kb_nt[10], sizeof notes);
    memcpy(pattern, TSEL->step, sizeof pattern);
    before = song.g[G_BPM];
    tap_emu(B_HOME);
    CHECK(ui.jam_home_view == JAM_HOME_GROOVE && song.playing && song.rec == rec && song.g[G_BPM] == before &&
          kb_n[10] == held && !memcmp(kb_nt[10], notes, sizeof notes) && !memcmp(TSEL->step, pattern, sizeof pattern),
          "Sound to Groove preserves a held chord, tempo, overdub state and captured pattern");
    tap_emu(B_HOME);
    CHECK(ui.jam_home_view == JAM_HOME_SOUND && song.playing && song.rec == rec &&
          kb_n[10] == held && !memcmp(kb_nt[10], notes, sizeof notes) && !memcmp(TSEL->step, pattern, sizeof pattern),
          "Groove to Sound preserves the same held chord and recording");
    jam_emu_key(10, 0); blocks(16);
    CHECK(!jam_emu_value(14, 0) && jam_emu_value(19, song.sel) >= held,
          "the chord releases and its recorded notes remain after both view switches");
    jam_emu_panic(); jam_emu_render();
    tap_emu(B_REC);
    CHECK(rec_wait && !jam_emu_value(20, 0), "armed recorder owns its overlay above either Home view");
    tap_emu(B_HOME);
    CHECK(rec_wait && ui.jam_home_view == JAM_HOME_SOUND,
          "HOME on the armed overlay neither cancels recording nor switches the hidden view");
    jam_emu_panic(); jam_emu_render();
    CHECK(jam_emu_value(18, 0) == 0, "both Home views and overlays keep LCD transfers within the physical screen");
}

int main(int argc, char **argv)
{
    uint32_t i, nonblack = 0, before, k;
    double energy = 0;
    const uint8_t *rgba;
    jam_emu_init();
    CHECK(jam_emu_frames() == CTL && jam_emu_rate() == FS, "fixed audio ABI");
    CHECK(jam_emu_value(6, 0) == 1 && jam_emu_value(0, 0) == 1, "real JamBox Home starts on chords");
    CHECK(jam_emu_value(21, 0) == JAM_HOME_GROOVE, "appended Home-view ABI explicitly starts on Groove");
    CHECK(jam_emu_value(20, 0), "browser knob hints follow the actual Home control owner");
    CHECK(jam_emu_screen_version() > 0, "boot LCD snapshot exists before audio starts");
    rgba = jam_emu_screen();
    for (i = 0; i < 240u * 240u; i++) {
        assert(rgba[i * 4u + 3u] == 255);
        nonblack += (rgba[i * 4u] | rgba[i * 4u + 1u] | rgba[i * 4u + 2u]) != 0;
    }
    CHECK(nonblack > 1000u, "LCD exports opaque rendered RGBA pixels");
    if (argc > 1) {
        FILE *f = fopen(argv[1], "wb");
        assert(f); fprintf(f, "P6\n240 240\n255\n");
        for (i = 0; i < 240u * 240u; i++) fwrite(rgba + i * 4u, 1, 3, f);
        fclose(f);
    }
    before = jam_emu_value(8, 0);
    jam_emu_encoder(EN_ALGO, 1);
    CHECK(jam_emu_value(0, 0) == 2 && (uint32_t)jam_emu_value(8, 0) == before, "ALGORITHM chooses role through physical UI");
    jam_emu_encoder(EN_ALGO, -1);
    before = jam_emu_value(22, 0);
    jam_emu_encoder(EN_K1, 1);
    CHECK((uint32_t)jam_emu_value(22, 0) == before + 1u, "Groove K1 reaches global swing through the real panel");
    jam_emu_key(0, 1);
    CHECK(jam_emu_value(4, 0) == 1 && jam_emu_value(14, 0) > 1, "one physical key expands the startup chord");
    jam_emu_key(0, 0);
    CHECK(jam_emu_value(4, 0) == 0 && jam_emu_value(14, 0) == 0, "key-up releases the original chord");

    jam_emu_button(B_FX, 1);
    jam_emu_key(0, 1);
    CHECK(jam_emu_value(11, 0) == LY_FX && jam_emu_value(14, 0) == 0, "held FX owns its key before the keyboard can play it");
    CHECK(!jam_emu_value(20, 0), "held FX hides Home-only knob hints");
    jam_emu_frame();
    CHECK(jam_emu_leds()[B_FX] == 255, "logical button LEDs follow the firmware matrix");
    jam_emu_release();
    CHECK(jam_emu_value(4, 0) == 0 && jam_emu_value(5, 0) == 0 && jam_emu_value(11, 0) == LY_PLAY,
          "release clears keys, buttons and the held layer");
    jam_emu_button(B_SCL, 1); jam_emu_key(0, 1);
    CHECK(trk[0].p[P_ROOT] == 5 && lk_r == lk_w,
          "SEL plus F dispatches its scale action before another physical event");
    jam_emu_button(B_SCL, 0); jam_emu_key(0, 0);
    CHECK(trk[0].p[P_ROOT] == 5 && !jam_emu_value(14, 0) && lk_r == lk_w,
          "releasing SEL before its key preserves the scale action and releases ownership");
    jam_emu_button(B_SCL, 1); jam_emu_key(2, 1);
    jam_emu_key(2, 0); jam_emu_button(B_SCL, 0);
    CHECK(trk[0].p[P_ROOT] == 7 && !jam_emu_value(14, 0) && lk_r == lk_w,
          "releasing the key before SEL performs the same complete handoff");
    blocks(400); /* the firmware's post-layer encoder quiet window */
    tap_emu(B_ENV);
    CHECK(!jam_emu_value(6, 0) && cur_page()->fam == FAM_ENV, "ENV opens the real deep page");
    tap_emu(B_HOME);
    CHECK(jam_emu_value(6, 0), "HOME returns to the real overview");

    tap_emu(B_PLAY);
    CHECK(jam_emu_value(1, 0), "PLAY starts the shared sequencer");
    before = jam_emu_value(10, 0);
    for (k = 0; k < 512; k++) {
        const float *audio = jam_emu_render();
        for (i = 0; i < CTL * 2u; i++) {
            assert(isfinite(audio[i]) && audio[i] >= -1.0f && audio[i] <= 1.0f);
            energy += audio[i] * audio[i];
        }
    }
    CHECK(energy > 0.01, "shared C audio renders a bounded audible starter groove");
    CHECK((uint32_t)jam_emu_value(10, 0) > before + 15u, "audio sample time advances the physical UI");
    jam_emu_master(31);
    CHECK(jam_emu_value(16, 0) == 31, "master volume reaches the DSP level");
    jam_emu_panic(); jam_emu_render();
    CHECK(!jam_emu_value(1, 0) && !jam_emu_value(4, 0) && !jam_emu_value(5, 0), "panic stops and releases physical controls");
    tap_emu(B_REC);
    CHECK(jam_emu_value(3, 0), "physical REC arms the real stopped recorder");
    jam_emu_key(0, 1); blocks(256); jam_emu_key(0, 0);
    CHECK(jam_emu_value(19, 1) >= 4, "physical keys record actual chord notes into the sequence");
    jam_emu_panic(); jam_emu_render();
    panel_setup();
    CHECK(str_eq(ui.msg, "BROWSER PANEL FIXED"), "hardware calibration has a nonblocking browser handler");
    project_save(0);
    CHECK(str_eq(ui.msg, "STORAGE DISABLED") && !project_used(0), "storage never reports a pretend save");
    jam_emu_button(B_SAVE, 1); jam_emu_key(key_of_white(4), 1); jam_emu_frame();
    CHECK(str_eq(ui.msg, "STORAGE DISABLED") && !project_used(0),
          "the held SAVE gesture leaves its slot empty and reports STORAGE DISABLED");
    jam_emu_release();
    CHECK(jam_emu_value(18, 0) == 0, "all real LCD transfers stay within 240 by 240");
    test_snap_lights();
    test_chord_tone_lights();
    test_chord_panel_controls();
    test_bass_voice_explanation();
    test_home_views();
    printf("jam-emu: %u checks passed\n", checks);
    return 0;
}
