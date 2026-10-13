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

int main(int argc, char **argv)
{
    uint32_t i, nonblack = 0, before, k;
    double energy = 0;
    const uint8_t *rgba;
    jam_emu_init();
    CHECK(jam_emu_frames() == CTL && jam_emu_rate() == FS, "fixed audio ABI");
    CHECK(jam_emu_value(6, 0) == 1 && jam_emu_value(0, 0) == 1, "real JamBox Home starts on chords");
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
    jam_emu_encoder(EN_SELECT, 1);
    CHECK(jam_emu_value(0, 0) == 2 && (uint32_t)jam_emu_value(8, 0) == before, "SELECT chooses role through physical UI");
    jam_emu_encoder(EN_SELECT, -1);
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
    CHECK(str_eq(ui.msg, "BROWSER: NO STORAGE") && !project_used(0), "storage never reports a pretend save");
    jam_emu_button(B_SAVE, 1); jam_emu_key(key_of_white(4), 1); jam_emu_frame();
    CHECK(str_eq(ui.msg, "BROWSER: NO STORAGE"), "the held SAVE gesture cannot replace the storage warning with SAVED");
    jam_emu_release();
    CHECK(jam_emu_value(18, 0) == 0, "all real LCD transfers stay within 240 by 240");
    printf("jam-emu: %u checks passed\n", checks);
    return 0;
}
