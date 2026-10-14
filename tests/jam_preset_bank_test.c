/* SPDX-License-Identifier: GPL-3.0-only */
/* The curated bank through production keyboard, voice allocator and stereo DSP.
 * No hardware access. JSON and the optional WAV are audition evidence, not an
 * assertion that native timing proves the FM-1's real-time performance. */
#define JAMBOX_PROTO 1
#define main upstream_hostsim_main
#include "hostsim.c"
#undef main
#include "../firmware/src/jam_palette.c"

static unsigned checks, failures;
static uint32_t current_track, current_preset;
#define CHECK(c, label) do { checks++; if (!(c)) { \
    fprintf(stderr, "FAIL track=%u sound=%u: %s (line %d)\n", \
            current_track, current_preset, label, __LINE__); failures++; } } while (0)

typedef struct {
    step_t step[NSTEP];
    uint8_t duration[NSTEP][4], duration_mode;
    int8_t micro[NSTEP];
    uint8_t fill[NSTEP / 4];
    plock_t lock[NLOCK];
} pattern_copy_t;

static const uint8_t kept[] = {
    P_LEVEL, P_PAN, P_MUTE, P_ROOT, P_SCALE, P_QUANT, P_CHORD,
    P_TFLT, P_STRUM, P_VLEAD, P_SLEN, P_SDIV, P_SSWING, P_SGATE
};

static void pattern_copy(pattern_copy_t *to, const track_t *from)
{
    memset(to, 0, sizeof *to);
    memcpy(to->step, from->step, sizeof to->step);
    memcpy(to->duration, from->note_len, sizeof to->duration);
    to->duration_mode = from->duration_mode;
    memcpy(to->micro, from->micro, sizeof to->micro);
    memcpy(to->fill, from->fill, sizeof to->fill);
    memcpy(to->lock, from->lock, sizeof to->lock);
}

static void render_block(int32_t *out)
{
    fm6_poll();                 /* the production main loop services FM6 loads */
    mix_block(out, CTL);
}

static void blocks(uint32_t n)
{
    int32_t out[CTL * 2];
    while (n--) render_block(out);
}

static void fresh(void)
{
    fm1_in.notes = 0;
    keyboard_block();
    song.playing = 0;
    song.rec = 0;
    transport_req = 0;
    CHECK(jam_new_dusty(), "fresh stopped session is available");
    blocks(32);                /* complete the previous preset's engine fade */
}

static const param_desc_t *descriptor(const track_t *t, uint32_t parameter)
{
    const engine_t *e;
    if (parameter < P_E0) return &TP[parameter];
    e = ENGINES[t->eng_req];
    return e->desc ? e->desc(t, parameter - P_E0) : &e->edit[parameter - P_E0];
}

static void parameters_valid(const track_t *t)
{
    uint32_t p;
    CHECK(t->eng_req < NENGINES, "sound requests an existing engine");
    if (t->eng_req >= NENGINES) return;
    for (p = 0; p < P_COUNT; p++) {
        const param_desc_t *d = descriptor(t, p);
        if (t->p[p] < d->min || t->p[p] > d->max) {
            fprintf(stderr, "  parameter %u %s=%d, allowed %d..%d\n", p,
                    d->label, t->p[p], d->min, d->max);
            CHECK(0, "all loaded and macro-edited parameters obey their descriptors");
        }
    }
}

static void seed_patterns(void)
{
    uint32_t k, i;
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        t->p[P_LEVEL] = 73 + k; t->p[P_PAN] = -11 + k; t->p[P_MUTE] = 1;
        t->p[P_ROOT] = 3; t->p[P_SCALE] = 1; t->p[P_QUANT] = 1;
        t->p[P_CHORD] = 2; t->p[P_TFLT] = -7; t->p[P_STRUM] = 12; t->p[P_VLEAD] = 1;
        t->p[P_SLEN] = 48; t->p[P_SDIV] = 1; t->p[P_SSWING] = 21; t->p[P_SGATE] = 91;
        for (i = 0; i < NSTEP; i++) {
            if (k == TRK_DRUM) dstep_set(&t->dstep[i], i % DRUM_LANES, LV_SOFT, i % 4);
            else {
                t->step[i].time = ST_NOTE;
                t->step[i].n = 2;
                t->step[i].note[0] = 48 + i % 12;
                t->step[i].note[1] = 60 + i % 12;
                t->step[i].vel = 91;
                t->note_len[i][0] = i % 64 + 1;
                t->note_len[i][1] = (i + 11) % 64 + 1;
            }
            t->micro[i] = (int8_t)(i % 23) - 11;
        }
        t->duration_mode = k != TRK_DRUM;
        t->fill[10] = 0x55;
        t->lock[0].step = 41; t->lock[0].param = P_DIST; t->lock[0].val = 12;
    }
}

static void test_loads(void)
{
    uint32_t k, p, i, n = 0;
    fresh();
    seed_patterns();
    for (k = 0; k < NTRK; k++) {
        char names[32][32];
        CHECK(jam_sound_count(k) == (k == TRK_DRUM ? 8u : 12u), "expanded bank has the promised choices");
        for (p = 0; p < jam_sound_count(k); p++) {
            pattern_copy_t before[NTRK], after;
            int16_t globals[G_COUNT], preserved[NTRK][NELEM(kept)];
            int16_t initial[P_COUNT];
            uint8_t engine, preset;
            current_track = k; current_preset = p;
            for (i = 0; i < NTRK; i++) {
                pattern_copy(&before[i], &trk[i]);
                for (uint32_t j = 0; j < NELEM(kept); j++) preserved[i][j] = trk[i].p[kept[j]];
            }
            memcpy(globals, song.g, sizeof globals);
            CHECK(jam_sound_load(k, p), "every advertised sound loads");
            CHECK(jam_sound_index(k) == p, "browser position identifies the loaded sound");
            CHECK(jam_sound_name(k)[0] && strlen(jam_sound_name(k)) <= 12,
                  "every sound has a name that fits the browser");
            snprintf(names[p], sizeof names[p], "%s", jam_sound_name(k));
            for (i = 0; i < p; i++) CHECK(strcmp(names[i], names[p]), "names within a role are distinct");
            CHECK(!memcmp(globals, song.g, sizeof globals), "loading a sound preserves tempo and all global settings");
            for (i = 0; i < NTRK; i++) {
                pattern_copy(&after, &trk[i]);
                CHECK(!memcmp(&before[i], &after, sizeof after),
                      "sound loads preserve every track's notes, independent lengths, nudges, fills and locks");
                for (uint32_t j = 0; j < NELEM(kept); j++)
                    CHECK(trk[i].p[kept[j]] == preserved[i][j], "sound loads preserve mix, key, chord and timing choices");
            }
            if (k == TRK_DRUM) {
                CHECK(trk[k].p[P_E0] >= 0 && trk[k].p[P_E0] < DRUM_USR,
                      "all curated drum kits resolve to existing factory kits");
            } else {
                CHECK(trk[k].p[P_VOICE] == V_POLY, "enabling chords overrides a factory mono voice preference");
                CHECK(trk[k].p[P_AMODE] == 0 && trk[k].p[P_ITYPE] == 0,
                      "a starting sound does not introduce an arpeggiator or insert effect");
                parameters_valid(&trk[k]);
            }
            memcpy(initial, trk[k].p, sizeof initial);
            engine = trk[k].eng_req; preset = trk[k].preset;
            /* Drum TONE is the track filter: intentionally preserved on loads. */
            for (i = k == TRK_DRUM ? 1u : 0u; i < 4; i++) jam_macro_set(k, i, 100);
            if (k != TRK_DRUM) {
                for (i = P_E0; i < P_COUNT; i++) trk[k].p[i] = descriptor(&trk[k], i)->min;
                trk[k].p[P_GLIDE] = 63; trk[k].p[P_TRANS] = 24; trk[k].user = 3;
            }
            CHECK(jam_sound_load(k, p), "edited sounds can be reloaded");
            CHECK(!memcmp(initial, trk[k].p, sizeof initial) && trk[k].eng_req == engine &&
                  trk[k].preset == preset && !trk[k].user,
                  "reloading restores the same parameter recipe after sound edits");
            n++;
        }
    }
    CHECK(n == 44, "44 curated choices verified without mutating a pattern");
}

static void test_macros(void)
{
    uint32_t k, p, m, other;
    for (k = 0; k < NTRK; k++) for (p = 0; p < jam_sound_count(k); p++) {
        current_track = k; current_preset = p;
        fresh();
        CHECK(jam_sound_load(k, p), "macro test loads a curated sound");
        CHECK(jam_macro_supported(k), "every curated sound exposes working Home macros");
        if (k < NPART)
            CHECK(trk[k].p[P_VOICE] == (k == 1 ? V_POLY : V_LEGATO),
                  "ordinary Bass and Lead loads stay monophonic until chords are enabled");
        for (m = 0; m < 4; m++) {
            pattern_copy_t before, after;
            int16_t prior[NTRK][P_COUNT], low[P_COUNT], globals[G_COUNT];
            pattern_copy(&before, &trk[k]);
            for (other = 0; other < NTRK; other++) memcpy(prior[other], trk[other].p, sizeof prior[other]);
            memcpy(globals, song.g, sizeof globals);
            CHECK(jam_macro_name(k, m)[0], "a macro has a visible label");
            CHECK(jam_macro_step(k, m) > 0 && jam_macro_step(k, m) <= 100,
                  "macro detents always make a usable positive step");
            jam_macro_set(k, m, -200);
            CHECK(jam_macro_get(k, m) == 0, "macro clamps to its low endpoint");
            memcpy(low, trk[k].p, sizeof low);
            int16_t lowglobals[G_COUNT];
            memcpy(lowglobals, song.g, sizeof lowglobals);
            jam_macro_set(k, m, 200);
            CHECK(jam_macro_get(k, m) == 100, "macro clamps to its high endpoint");
            CHECK(memcmp(low, trk[k].p, sizeof low) || memcmp(lowglobals, song.g, sizeof lowglobals),
                  "turning every macro changes an actual sound or drum mixer parameter");
            if (k != TRK_DRUM) parameters_valid(&trk[k]);
            pattern_copy(&after, &trk[k]);
            CHECK(!memcmp(&before, &after, sizeof before), "macros preserve notes and their articulation");
            for (other = 0; other < NTRK; other++) if (other != k)
                CHECK(!memcmp(prior[other], trk[other].p, sizeof prior[other]), "macros leave other tracks' parameters alone");
            for (other = 0; other < NELEM(kept); other++)
                if (!(k == TRK_DRUM && m == 0 && kept[other] == P_TFLT))
                    CHECK(prior[k][kept[other]] == trk[k].p[kept[other]],
                          "sound macros preserve the selected track's mix, timing and playing choices");
            if (k != TRK_DRUM)
                CHECK(!memcmp(globals, song.g, sizeof globals), "melodic macros do not change master or drum settings");
            CHECK(song.g[G_BPM] == globals[G_BPM] && song.g[G_SWING] == globals[G_SWING],
                  "sound macros preserve the groove's tempo and swing");
            for (other = 0; other < P_COUNT; other++) if (low[other] != trk[k].p[other]) {
                trk[k].lk_n = 1;
                trk[k].lk_param[0] = other;
                trk[k].lk_base[0] = -999;
                jam_macro_set(k, m, 0);
                CHECK(trk[k].lk_base[0] == trk[k].p[other],
                      "macro edits under an active parameter lock update its restored base");
                trk[k].lk_n = 0;
                break;
            }
        }
    }
}

static uint32_t gates(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++) n += t->v[i].active && t->v[i].gate;
    return n;
}

static void test_chords(void)
{
    uint32_t k, p;
    for (k = 0; k < NPART; k++) for (p = 0; p < jam_sound_count(k); p++) {
        track_t *t = &trk[k];
        current_track = k; current_preset = p;
        fresh();
        song.sel = k;
        t->p[P_ROOT] = 3; t->p[P_SCALE] = 1; t->p[P_QUANT] = 1;
        t->p[P_CHORD] = 2; t->p[P_STRUM] = 0; t->p[P_VLEAD] = 0;
        CHECK(jam_sound_load(k, p), "a sound loads with four-note chord mode already enabled");
        blocks(32);
        fm1_in.notes = 1u << 10; /* printed D#, in D# major */
        keyboard_block();
        blocks(2);
        CHECK(kb_n[10] == 4 && gates(t) == 4,
              "a black root produces all four chord voices for every melodic preset");
        CHECK(voices_busy() == 4, "four-note chords use four shared voices, without hidden unison layering");
        fm1_in.notes = 0; keyboard_block();
        CHECK(!gates(t), "releasing a one-key chord releases every voice gate");
        blocks(16);
    }
}

static void test_invalid(void)
{
    track_t saved[NTRK];
    song_t oldsong;
    uint32_t ids[NTRK];
    int curated[NTRK];
    char names[NTRK][32];
    current_track = current_preset = 999;
    fresh();
    memcpy(saved, trk, sizeof saved); oldsong = song;
    for (uint32_t k = 0; k < NTRK; k++) {
        ids[k] = jam_sound_index(k); curated[k] = jam_sound_is_curated(k);
        snprintf(names[k], sizeof names[k], "%s", jam_sound_name(k));
    }
    CHECK(!jam_sound_load(NTRK, 0) && !jam_sound_load(UINT32_MAX, UINT32_MAX), "invalid tracks are rejected");
    for (uint32_t k = 0; k < NTRK; k++) {
        CHECK(!jam_sound_load(k, jam_sound_count(k)) && !jam_sound_load(k, UINT32_MAX), "out-of-bank indices are rejected");
        jam_macro_set(k, 4, 100); jam_macro_set(k, UINT32_MAX, 100);
    }
    jam_macro_set(NTRK, 0, 100); jam_macro_set(UINT32_MAX, UINT32_MAX, 100);
    CHECK(!memcmp(saved, trk, sizeof saved) && !memcmp(&oldsong, &song, sizeof song),
          "invalid load and macro requests do not mutate music, runtime state or browser identity");
    for (uint32_t k = 0; k < NTRK; k++)
        CHECK(ids[k] == jam_sound_index(k) && curated[k] == jam_sound_is_curated(k) &&
              !strcmp(names[k], jam_sound_name(k)), "invalid requests preserve all curated browser identities");
    CHECK(!jam_sound_count(NTRK) && !jam_sound_count(UINT32_MAX) && !jam_sound_name(NTRK)[0] &&
          !jam_sound_index(NTRK) && !jam_macro_supported(NTRK) && !jam_macro_get(NTRK, 0),
          "invalid track queries return safe empty results");
}

static void test_macro_audio(uint32_t track, uint32_t preset, int32_t percent)
{
    const uint32_t phrase = FS / 2u / CTL, tail = FS * 8u / CTL;
    int32_t out[CTL * 2], peak = 0, tailpeak = 0;
    uint64_t squares = 0, samples = 0;
    uint32_t maximum = 0;
    fresh(); song.sel = track;
    CHECK(jam_sound_load(track, preset), "macro audio test loads its sound");
    for (uint32_t m = 0; m < 4; m++) jam_macro_set(track, m, percent);
    blocks(32);
    if (track < NPART) {
        fm1_in.notes = 1u << (track == 2u ? 19 : 7);
        keyboard_block();
    } else {
        input_on(TDRUM, 36, 100); input_on(TDRUM, 38, 100); input_on(TDRUM, 42, 100);
    }
    for (uint32_t b = 0; b < phrase + tail; b++) {
        if (b == phrase && track < NPART) { fm1_in.notes = 0; keyboard_block(); }
        render_block(out);
        uint32_t voices = voices_busy();
        if (voices > maximum) maximum = voices;
        for (uint32_t s = 0; s < CTL * 2u; s++) {
            int32_t a = out[s] < 0 ? -out[s] : out[s];
            if (b < phrase) {
                if (a > peak) peak = a;
                squares += (uint64_t)((int64_t)out[s] * out[s]); samples++;
            }
            if (b >= phrase + tail - FS / CTL && a > tailpeak) tailpeak = a;
        }
    }
    uint32_t active = 0;
    for (uint32_t p = 0; p < NPART; p++)
        for (uint32_t v = 0; v < NVOICE; v++) active += trk[p].v[v].active;
    for (uint32_t v = 0; v < NDRUM; v++) active += drums.v[v].active;
    CHECK(maximum <= NVOICE, "macro extremes keep the shared voice count bounded");
    CHECK(peak < 32767, "all four macros together at either extreme produce bounded output");
    if (track < NPART || percent)
        CHECK(peak > 32 && sqrt((double)squares / samples) > 2,
              "macro extremes retain an audible instrument (drum LEVEL zero intentionally mutes)");
    CHECK(!active && tailpeak <= 8, "macro extremes still release their notes and effect tails");
}

static void test_audio(const char *report_path, const char *wav_path)
{
    const uint32_t phrase = FS * 2u / CTL, tail = FS * 8u / CTL, wavtail = FS / CTL;
    FILE *rep = fopen(report_path, "w"), *wav = wav_path ? fopen(wav_path, "wb") : 0;
    uint32_t total = 0;
    CHECK(rep && (!wav_path || wav), "audition evidence files open");
    if (!rep || (wav_path && !wav)) { if (rep) fclose(rep); return; }
    for (uint32_t k = 0; k < NTRK; k++) total += jam_sound_count(k);
    fresh(); blocks(tail);      /* the preceding chord-voice tests left FX tails */
    if (wav) wav_hdr(wav, total * (phrase + wavtail) * CTL);
    fprintf(rep, "{\"sample_rate\":%u,\"note_velocity\":100,\"render\":\"native production DSP\",\"sounds\":[\n", FS);
    uint32_t row = 0;
    for (uint32_t k = 0; k < NTRK; k++) for (uint32_t p = 0; p < jam_sound_count(k); p++) {
        int32_t out[CTL * 2], peak = 0, tailpeak = 0;
        uint64_t squares = 0, samples = 0, rails = 0;
        uint32_t maximum = 0, pregain_peak = 0, limited_blocks = 0;
        int32_t minimum_gain = 32768;
        current_track = k; current_preset = p;
        fresh(); song.sel = k;
        CHECK(jam_sound_load(k, p), "audition loads the requested sound");
        blocks(32);
        if (k < NPART) {
            fm1_in.notes = 1u << (k == 2u ? 19 : 7); /* C5 lead; C4 or its transposed bass/chord */
            keyboard_block();
        }
        for (uint32_t b = 0; b < phrase + tail; b++) {
            if (k == TRK_DRUM && b < phrase) {
                uint32_t step0 = b * CTL * 8u / FS, step1 = (b + 1u) * CTL * 8u / FS;
                if (!b || step0 != step1) {
                    uint32_t step = !b ? 0 : step1;
                    if (!(step % 4u)) input_on(TDRUM, 36, 100);
                    if (step % 8u == 4u) input_on(TDRUM, 38, 100);
                    if (!(step % 2u)) input_on(TDRUM, 42, 100);
                }
            }
            if (b == phrase && k < NPART) { fm1_in.notes = 0; keyboard_block(); }
            render_block(out);
            if (b < phrase) {
                if (lim_dac.g < minimum_gain) minimum_gain = lim_dac.g;
                limited_blocks += lim_dac.g < 32768;
                for (uint32_t s = 0; s < CTL; s++) {
                    uint32_t l = (uint32_t)(lim_dac.dl[s] < 0 ? -lim_dac.dl[s] : lim_dac.dl[s]);
                    uint32_t r = (uint32_t)(lim_dac.dr[s] < 0 ? -lim_dac.dr[s] : lim_dac.dr[s]);
                    if (l > pregain_peak) pregain_peak = l;
                    if (r > pregain_peak) pregain_peak = r;
                }
            }
            uint32_t voices = voices_busy();
            if (voices > maximum) maximum = voices;
            for (uint32_t s = 0; s < CTL * 2u; s++) {
                int32_t a = out[s] < 0 ? -out[s] : out[s];
                if (b < phrase) {
                    if (a > peak) peak = a;
                    squares += (uint64_t)((int64_t)out[s] * out[s]);
                    samples++;
                    rails += a >= 32767;
                }
                if (b >= phrase + tail - FS / CTL && a > tailpeak) tailpeak = a;
            }
            if (wav && b < phrase + wavtail)
                for (uint32_t s = 0; s < CTL; s++) wav_put(wav, out[2 * s], out[2 * s + 1]);
        }
        uint32_t active = 0;
        for (uint32_t part = 0; part < NPART; part++)
            for (uint32_t v = 0; v < NVOICE; v++) active += trk[part].v[v].active;
        for (uint32_t v = 0; v < NDRUM; v++) active += drums.v[v].active;
        double rms = sqrt((double)squares / (double)samples);
        CHECK(peak > 256 && rms > 16, "every patch produces audible audio in its intended register");
        CHECK(!rails && peak < 32767, "default patches do not hit the digital output rails");
        if (k < NPART)
            CHECK(-20 * log10(minimum_gain / 32768.0) <= 1.0,
                  "default melodic sounds need at most 1 dB of peak limiting in the role audition");
        CHECK(maximum <= NVOICE, "audition stays inside the shared synth voice budget");
        CHECK(maximum == (k == TRK_DRUM ? 0u : k == 1u ? 4u : 1u),
              "default roles use the intended chord or single-note voice count");
        CHECK(!active && tailpeak <= 8, "voices and effects settle after releasing the patch");
        fprintf(rep, "%s{\"track\":%u,\"index\":%u,\"name\":\"%s\",\"engine\":\"%s\",\"audition_start_seconds\":%.6f,\"audition_duration_seconds\":%.6f,\"peak\":%d,\"rms\":%.3f,\"rms_dbfs\":%.3f,\"rail_samples\":%llu,\"max_synth_voices\":%u,\"tail_peak\":%d,\"active_after_release\":%u,\"pre_limiter_peak\":%u,\"max_limiter_reduction_db\":%.3f,\"limited_blocks_percent\":%.3f}",
                row ? ",\n" : "", k, p, jam_sound_name(k),
                k == TRK_DRUM ? "DRUM" : ENGINES[trk[k].engine]->name,
                (double)row * (phrase + wavtail) * CTL / FS, (double)(phrase + wavtail) * CTL / FS,
                peak, rms, rms > 0 ? 20 * log10(rms / 32768.0) : -120.0,
                (unsigned long long)rails, maximum, tailpeak, active,
                pregain_peak, -20 * log10(minimum_gain / 32768.0), 100.0 * limited_blocks / phrase);
        printf("audition %s %2u %-12s peak=%5d RMS=%6.1f dBFS=%6.2f tail=%d active=%u\n",
               jam_role_name(k), p, jam_sound_name(k), peak, rms, 20 * log10(rms / 32768.0), tailpeak, active);
        row++;
    }
    for (uint32_t k = 0; k < NTRK; k++) for (uint32_t p = 0; p < jam_sound_count(k); p++) {
        current_track = k; current_preset = p;
        test_macro_audio(k, p, 0); test_macro_audio(k, p, 100);
    }
    fprintf(rep, "\n],\"checks\":%u,\"failures\":%u}\n", checks, failures);
    fclose(rep); if (wav) fclose(wav);
}

int main(int argc, char **argv)
{
    host_tracks_init(); fm6_init(); dsu_defaults();
    test_loads(); test_macros(); test_chords(); test_invalid();
    test_audio(argc > 1 ? argv[1] : "build/jam-preset-bank-tests/metrics.json", argc > 2 ? argv[2] : 0);
    printf("preset bank: %u checks, %u failures; native audio only\n", checks, failures);
    return failures != 0;
}
