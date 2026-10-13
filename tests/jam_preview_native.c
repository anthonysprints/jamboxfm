/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#define __attribute__(x)
#include "../firmware/host/jam_preview.c"

static double audio(uint32_t frames)
{
    double sum = 0;
    for (uint32_t f = 0; f < frames; f += CTL) {
        float *samples = preview_render();
        for (uint32_t i = 0; i < CTL * 2u; i++) {
            assert(isfinite(samples[i]) && fabsf(samples[i]) <= 1.0f);
            sum += fabsf(samples[i]);
        }
    }
    return sum;
}

int main(int argc, char **argv)
{
    jam_preview_init();
    preview_render(); /* the browser adapter primes its output buffer */
    if (argc == 2 && !strcmp(argv[1], "--audio")) {
        jam_preview_key(7, 1);
        for (uint32_t f = 0; f < 8192u; f += CTL) {
            if (f == 4096u) jam_preview_key(7, 0);
            if (fwrite(preview_render(), sizeof output, 1, stdout) != 1) return 1;
        }
        return 0;
    }
    assert(!song.playing && song.g[G_BPM] == 88 && song.sel == 1);
    assert(jam_role_track(0) == 3 && jam_role_track(1) == 0);
    jam_preview_key(7, 1); /* C white key; actual Sloop keyboard expands C minor 7 */
    assert(kb_kind[7] == KS_NOTE && kb_n[7] == 4);
    assert(kb_nt[7][0] == 60 && kb_nt[7][1] == 63 && kb_nt[7][2] == 67 && kb_nt[7][3] == 70);
    assert(audio(8192) > 1);
    jam_preview_key(7, 0);
    assert(kb_kind[7] == KS_NONE && !fm1_in.notes && !kb_prev);
    jam_preview_record(1);
    assert(rec_wait && !song.playing);
    jam_preview_key(7, 1);
    assert(song.playing && song.rec == 2 && !rec_wait);
    audio(FS / 3); jam_preview_key(7, 0); audio(FS / 3);
    jam_preview_key(9, 1); audio(FS / 3); jam_preview_key(9, 0);
    jam_preview_record(0);
    assert(song.playing && !song.rec && !rec_wait);
    assert(jam_preview_value(21, 1, 0) >= 8);
    step_t chord_copy[NSTEP]; memcpy(chord_copy, trk[1].step, sizeof chord_copy);
    jam_preview_select(0); jam_preview_record(1);
    jam_preview_key(7, 1); audio(FS / 3); jam_preview_key(7, 0); jam_preview_record(0);
    assert(jam_preview_value(21, 0, 0) >= 1);
    assert(!memcmp(chord_copy, trk[1].step, sizeof chord_copy));
    jam_preview_record(1); jam_preview_key(7, 1); jam_preview_select(2);
    assert(!song.rec && !rec_wait && !fm1_in.notes && kb_kind[7] == KS_NONE);
    jam_preview_panic(); audio(FS * 4);
    jam_preview_mute(3, 1); jam_preview_mute(0, 1); jam_preview_mute(2, 1);
    assert(audio(FS) < 0.1);
    jam_preview_play(1);
    assert(audio(FS * 3) > 1); /* recorded chords replay without any held browser key */
    jam_preview_panic(); audio(CTL);
    assert(!song.playing && !fm1_in.notes && !song.rec && !rec_wait);
    puts("JamBox native: local chord path, release ledger, two-chord/bass recording, replay and panic passed");
    return 0;
}
