/* SPDX-License-Identifier: GPL-3.0-only */
/* Actual new panel-edit helpers -> shipping save adapter -> cold restore. */
#define _POSIX_C_SOURCE 200809L
#define JAMBOX_RECOVERY 1
#define JAM_STORAGE_LIBRARY 1
#include "jam_storage_harness.c"
#define JAM_RECOVERY_HOST 1
#include "../firmware/src/jam_recovery_runtime.c"
int main(void)
{
    unsigned bank, lane, knob;
    memset(nor, 255, sizeof nor);
    ordinary_boot(); jam_recovery_boot();
    song.sel = TRK_DRUM;
    snapshot();
    for (bank = 0; bank < DSU_N; bank++) {
        TDRUM->p[P_E0] = DRUM_SAMPLED + bank;
        drum_page = 2; studio_open(SC_DRUM);
        drum_sound_open(); drum_copy_slot = bank; drum_sound_open();
        check(drum_kit() == DRUM_SYN + bank, "panel copy selects the explicit SYN bank");
        for (lane = 0; lane < DRUM_LANES; lane++) {
            pen_lane = lane;
            for (knob = 0; knob < 4; knob++) drum_sound_turn(knob, knob == 3 ? -4 : 7);
        }
    }
    unchanged("copying and editing every sound never writes NOR without Save now");
    dsu_bank_t expected = dsu;
    int16_t selected_kit = TDRUM->p[P_E0];
    check(jam_recovery_save(1) == 0, "manual checkpoint accepts the real edited SYN banks");
    memset(&dsu, 0, sizeof dsu);
    memset(&song, 0, sizeof song);
    memset(trk, 0, sizeof trk);
    memset(&ui, 0, sizeof ui);
    snapshot(); ordinary_boot(); jam_recovery_boot();
    check(!memcmp(&expected, &dsu, sizeof dsu), "cold boot restores every byte of all four panel-edited SYN kits");
    check(TDRUM->p[P_E0] == selected_kit, "cold boot restores the selected edited kit");
    unchanged("cold restoring edited drum sounds does not write flash");
    printf("Drum sound save: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
