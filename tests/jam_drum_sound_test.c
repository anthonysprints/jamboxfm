/* SPDX-License-Identifier: GPL-3.0-only */
#define JAM_DRUM_NAVIGATION_HARNESS_ONLY 1
#include "jam_drum_navigation_test.c"

static void test_copy(void)
{
    dsu_bank_t before;
    unsigned slot, k;
    for (slot = 0; slot < DSU_N; slot++) {
        setup(JAM_HOME_GROOVE);
        dsu_defaults(); before = dsu;
        TDRUM->p[P_E0] = DRUM_SAMPLED + 2;
        tap(B_ENV);
        expect(on_drum_page() && drum_page == 2 && !kb_grid, "ENV opens playable DRUM SOUND");
        for (k = 0; k < 4; k++) turn(EN_K1 + k, 5);
        expect(!memcmp(&before, &dsu, sizeof dsu), "factory knobs cannot mutate any SYN bank");
        tap(B_ENV);
        expect(drum_page == 3, "factory editing requires explicit copy confirmation");
        turn(EN_K1, slot);
        turn(EN_PRESET, 1);
        expect(drum_kit() == DRUM_SAMPLED + 2, "browsing cannot change source during confirmation");
        ui.force = 1; frame(); ppm("copy-confirmation");
        tap(B_HOME);
        expect(ui.home && !memcmp(&before, &dsu, sizeof dsu), "HOME cancels copy without replacing a bank");
        tap(B_ENV); tap(B_ENV); turn(EN_K1, slot); tap(B_ENV);
        expect(drum_page == 2 && drum_kit() == DRUM_SYN + slot, "confirmed copy selects the chosen SYN slot");
        expect(!memcmp(dsu.k[slot].s, DS_KITS[2].s, sizeof dsu.k[slot].s), "complete factory recipes copied exactly");
        for (k = 0; k < DSU_N; k++) if (k != slot)
            expect(!memcmp(&before.k[k], &dsu.k[k], sizeof before.k[k]), "copy preserves every other SYN bank");
        expect(!recovery_calls && dsu_dirty, "copy never writes flash or invokes automatic saving");
    }
}
static void test_edits(void)
{
    static const unsigned mapping[16] = {0,14,1,2,3,4,3,11,15,5,6,7,8,9,10,12};
    unsigned kit, lane, k;
    for (kit = 0; kit < DS_NKITS; kit++) for (lane = 0; lane < DRUM_LANES; lane++) {
        dsu_bank_t before;
        setup(JAM_HOME_SOUND); dsu_defaults(); dsu_from_factory(0, kit);
        TDRUM->p[P_E0] = DRUM_SYN; pen_lane = lane;
        tap(B_ENV); before = dsu;
        expect(drum_sound_lane() == mapping[lane], "panel GM notes map to the correct recipe independent of panel order");
        for (k = 0; k < 4; k++) turn(EN_K1 + k, 3);
        expect(dsu.k[0].s[drum_sound_lane()].level != before.k[0].s[drum_sound_lane()].level,
               "every panel drum maps to an editable synth recipe");
        before.k[0].s[drum_sound_lane()] = dsu.k[0].s[drum_sound_lane()];
        expect(!memcmp(&before, &dsu, sizeof dsu), "editing touches only the selected recipe, never another sound or bank");
        expect(!kb_grid && !recovery_calls, "sound editing neither toggles hits nor saves automatically");
    }
    setup(JAM_HOME_SOUND); dsu_defaults(); TDRUM->p[P_E0] = DRUM_SYN;
    tap(B_ENV); pen_lane = 4;
    const dsnd_t *hat = drum_sound_recipe();
    pen_lane = 6;
    expect(hat == drum_sound_recipe(), "closed and pedal hats share precisely one recipe");
    key(key_of_white(2));
    expect(pen_lane == 2 && !kb_grid, "playing a drum selects it for sound editing");
    ui.force = 1; frame(); ppm("drum-sound");
    tap(B_SAVE);
    expect(ui.recovery_screen && !recovery_calls, "SAVE opens the normal manual checkpoint screen");
    dsu_bank_t before = dsu;
    turn(EN_K1, 4);
    expect(!memcmp(&before, &dsu, sizeof dsu), "save modal owns knobs before the hidden drum editor");
}
/* Actual production synth rendering, deterministic initial noise. No simulated
 * parameter-to-audio mapping: ds_on/ds_render are the shipping DSP. */
static uint64_t render(uint32_t note)
{
    dsv_t voice;
    int32_t out[CTL];
    uint64_t h = 1469598103934665603ull;
    unsigned block, i;
    rng_state = 0x1234567u;
    ds_on(&voice, dsu.k[0].s, dsu.k[0].crush, note, 100);
    for (block = 0; block < FS / CTL; block++) {
        memset(out, 0, sizeof out);
        ds_render(&voice, out, CTL);
        for (i = 0; i < CTL; i++) h = (h ^ (uint32_t)out[i]) * 1099511628211ull;
    }
    return h;
}
static void test_safety(void)
{
    setup(JAM_HOME_SOUND); dsu_defaults();
    TDRUM->p[P_E0] = 0;
    dsu_bank_t before = dsu;
    tap(B_ENV); tap(B_ENV);
    for (unsigned k = 0; k < 4; k++) turn(EN_K1 + k, 100);
    expect(drum_page == 2 && !memcmp(&before, &dsu, sizeof dsu), "sampled kits never offer or execute a synth copy");
    TDRUM->p[P_E0] = DRUM_SYN; pen_lane = 0;
    drum_on(36, 100); drum_on(38, 100);
    unsigned selected = 0, other = 0;
    turn(EN_K1, 1);
    for (unsigned i = 0; i < NDRUM; i++) if (drums.v[i].active) {
        selected += drums.v[i].note == 36;
        other += drums.v[i].note == 38;
    }
    expect(!selected && other, "recipe edit retires its live voice without interrupting a neighboring drum");
    tap(B_HOME); rec_wait = 1;
    tap(B_ENV);
    expect(ui.home && rec_wait, "ENV cannot hide the armed recorder to begin an edit");
    rec_wait = 0;
}
static void test_audio(void)
{
    static const unsigned lanes[] = {0, 2, 4, 5};
    unsigned kit, l, k;
    for (kit = 0; kit < DS_NKITS; kit++) for (l = 0; l < NELEM(lanes); l++) for (k = 0; k < 4; k++) {
        setup(JAM_HOME_SOUND); dsu_defaults(); dsu_from_factory(0, kit);
        TDRUM->p[P_E0] = DRUM_SYN; pen_lane = lanes[l]; tap(B_ENV);
        const dsnd_t *d = drum_sound_recipe();
        if (!k && !drum_sound_tunable(d)) continue;
        uint64_t before = render(LANE_NOTE[pen_lane]);
        turn(EN_K1 + k, k == 3 ? -12 : k == 2 ? -24 : 12);
        expect(before != render(LANE_NOTE[pen_lane]), "kick/snare/hats controls change actual synthesized audio");
    }
}
int main(int argc, char **argv)
{
    outdir = argc > 1 ? argv[1] : "build/jam-drum-sound-tests/screens";
    test_copy(); test_edits(); test_audio(); test_safety();
    printf("drum-sound: %u checks, all checks passed\n", checks);
    return 0;
}
