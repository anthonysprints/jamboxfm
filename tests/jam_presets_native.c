/* SPDX-License-Identifier: GPL-3.0-only */
/* Native sample reference for the deliverable panel adapter. Each invocation
 * cold-boots, loads one curated sound through the physical encoder roles, and
 * emits interleaved float32 audio. No private preset or DSP entry point is used. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../firmware/host/jam_emu.c"

int main(int argc, char **argv)
{
    static const int32_t role[4] = {1, 2, 3, 0};
    uint32_t track, preset, key, block, i;
    int32_t engine;
    if (argc != 5) {
        fprintf(stderr, "Usage: %s track preset expected-name expected-engine\n", argv[0]);
        return 2;
    }
    track = (uint32_t)strtoul(argv[1], 0, 10);
    preset = (uint32_t)strtoul(argv[2], 0, 10);
    engine = (int32_t)strtol(argv[4], 0, 10);
    assert(track < 4u && preset < (track == 3u ? 8u : 12u));
    jam_emu_init();
    assert(jam_emu_frames() == 32u && jam_emu_rate() == 44100u);
    assert(jam_emu_value(0, 0) == 1 && jam_emu_value(21, 0) == 0);
    jam_emu_encoder(1, role[track] - 2); /* ALGORITHM: part, from startup Chords. */
    assert(jam_emu_value(0, 0) == (int32_t)track);
    jam_emu_encoder(2, (int32_t)preset); /* PRESETS: curated sound. */
    jam_emu_frame();
    assert(!strcmp(jam_emu_sound_name(), argv[3]));
    assert(jam_emu_value(15, 0) == engine && jam_emu_value(13, 0) == 1);
    /* Match the JS fixture exactly, allowing engine fades and FM6 loads to
     * settle before a physical note starts. Transport remains stopped. */
    for (block = 0; block < 32u; block++) jam_emu_render();
    key = track == 3u ? 0u : 7u; /* Drum kick, or C (a chord on Chords). */
    jam_emu_key(key, 1);
    for (block = 0; block < 256u; block++) {
        float *audio;
        if (block == 128u) jam_emu_key(key, 0);
        audio = jam_emu_render();
        for (i = 0; i < 64u; i++) assert(isfinite(audio[i]) && fabsf(audio[i]) <= 1.0f);
        if (fwrite(audio, sizeof *audio, 64u, stdout) != 64u) return 1;
    }
    assert(jam_emu_value(4, 0) == 0 && jam_emu_value(1, 0) == 0);
    assert(jam_emu_value(18, 0) == 0);
    return 0;
}
