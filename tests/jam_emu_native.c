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

static void test_chord_root_lights(void)
{
    uint32_t i;
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
        jam_emu_render(); jam_emu_frame();
        assert(emu_key_lights() == (1u << 10));
    }
    CHECK(1, "live SNAP chord lights only D#, without aliases or third/fifth flashes");
    jam_emu_key(10, 0); jam_emu_render(); jam_emu_frame();
    CHECK(!keys_sounding(t) && !emu_key_lights(),
          "chord root and tone flashes disappear on key-up");

    jam_emu_key(11, 1);
    for (i = 0; i < 10u; i++) {
        jam_emu_render(); jam_emu_frame();
        assert(emu_key_lights() == ((1u << 10) | (1u << 11)));
    }
    CHECK(kb_n[11] == 3 && kb_root[11] == 63,
          "SNAP chord from E lights pressed E and corrected D# only");
    /* Live root feedback temporarily owns the same part's pitch lights. */
    trk_note_on(t, 65, 100); jam_emu_frame();
    CHECK(emu_key_lights() == ((1u << 10) | (1u << 11)),
          "simultaneous playback tones do not obscure live chord root feedback");
    jam_emu_key(11, 0); jam_emu_frame();
    CHECK(emu_key_lights() == (1u << 12),
          "with local keys released, sustained playback lights its actual F pitch");
    trk_note_off(t, 65); jam_emu_frame(); jam_emu_frame();
    CHECK(!emu_key_lights(), "released playback leaves no residual chord-mode tone flash");

    /* There is no UI frame to attribute these note_hits to a live source. */
    jam_emu_key(10, 1); jam_emu_key(10, 0);
    for (i = 0; i < 10u; i++) {
        jam_emu_render(); jam_emu_frame();
        assert(!emu_key_lights());
    }
    CHECK(1, "a chord tapped between UI frames leaves no third/fifth or alias flashes");

    t->p[P_TRANS] = 2;
    jam_emu_key(10, 1); jam_emu_render(); jam_emu_frame();
    CHECK(kb_root[10] == 65 && emu_key_lights() == ((1u << 10) | (1u << 12)),
          "transposed chord lights pressed D# plus corrected root F, not its chord tones");
    t->p[P_TRANS] = 0;
    jam_emu_frame();
    CHECK(emu_key_lights() == ((1u << 10) | (1u << 12)),
          "changing transpose while held keeps the original chord root feedback");
    jam_emu_key(10, 0); jam_emu_frame();
    song.octave = 1;
    jam_emu_key(10, 1); jam_emu_render(); jam_emu_frame();
    CHECK(kb_root[10] == 75 && emu_key_lights() == (1u << 10),
          "chord root feedback follows the selected keyboard octave without aliases");
    jam_emu_key(10, 0); song.octave = 0;

    t->p[P_QUANT] = 0;
    jam_emu_key(11, 1); jam_emu_render(); jam_emu_frame();
    CHECK(kb_root[11] == 64 && emu_key_lights() == (1u << 11),
          "QNT OFF chord lights only its uncorrected printed root E");
    jam_emu_key(11, 0); jam_emu_frame();
    t->p[P_QUANT] = 1; t->p[P_ROOT] = 4;
    jam_emu_key(0, 1); jam_emu_render(); jam_emu_frame();
    CHECK(kb_root[0] == 52 && emu_key_lights() == 1u && !keys_sounding(t),
          "a chord root below the visible register does not wrap or light its upper tones");
    jam_emu_key(0, 0); jam_emu_frame();

    t->p[P_ROOT] = 3;
    trk_note_on(t, 63, 100); trk_note_on(t, 67, 100); trk_note_on(t, 70, 100);
    jam_emu_frame(); jam_emu_frame();
    CHECK(emu_key_lights() == ((1u << 10) | (1u << 14) | (1u << 17)),
          "sequencer-only chords show actual sounding pitches with no SNAP aliases");
    trk_note_off(t, 63); trk_note_off(t, 67); trk_note_off(t, 70);
    jam_emu_frame(); jam_emu_frame();
    CHECK(!emu_key_lights(), "sequencer-only chord lights release without shared hit flashes");

    t->p[P_QUANT] = 2;
    jam_emu_key(10, 1); jam_emu_frame();
    CHECK(kb_kind[10] == KS_MOD && !jam_emu_value(14, 0),
          "explicit WHITE layout preserves D# as a held chord modifier");
    jam_emu_key(10, 0);
    trk_note_on(t, 63, 100); trk_note_off(t, 63);
    jam_emu_frame(); jam_emu_frame();
    CHECK(emu_key_lights() == (1u << 7),
          "WHITE retains short-note flashes on its mapped white key");
    for (i = 0; i < 8u; i++) jam_emu_frame();
    CHECK(!emu_key_lights(), "WHITE short-note flashes expire normally");
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

int main(int argc, char **argv)
{
    uint32_t i, nonblack = 0, before, k;
    double energy = 0;
    const uint8_t *rgba;
    jam_emu_init();
    CHECK(jam_emu_frames() == CTL && jam_emu_rate() == FS, "fixed audio ABI");
    CHECK(jam_emu_value(6, 0) == 1 && jam_emu_value(0, 0) == 1, "real JamBox Home starts on chords");
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
    before = jam_emu_value(12, 0);
    jam_emu_encoder(EN_K1, 1);
    CHECK((uint32_t)jam_emu_value(12, 0) > before, "K1 reaches the real sound macro");
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
    test_chord_root_lights();
    test_chord_panel_controls();
    printf("jam-emu: %u checks passed\n", checks);
    return 0;
}
