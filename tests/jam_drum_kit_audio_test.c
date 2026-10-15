/* SPDX-License-Identifier: GPL-3.0-only */
/* Real drum voices and the production mix path, using the panel fixture's
 * native harness. Each render forks the same untouched parent: RNG, effects,
 * envelopes and limiter therefore start identically without reset lookalikes.
 * No flash writer, MIDI connection or firmware image is used. */
#define JAM_DRUM_NAVIGATION_HARNESS_ONLY 1
#include "jam_drum_navigation_test.c"
#include <errno.h>

#define AUDIO_FRAMES (FS * 2u)
typedef struct {
    int16_t pcm[AUDIO_FRAMES * 2u];
    uint64_t left, right, tail, delay_send, room_send;
    uint32_t peak;
} audio_render_t;
typedef struct {
    int level, pan, filter, echo, room;
    int insert, amount, a, b, c;
    int slicer, pattern, rate, depth;
    int unused_synth_fx;
} audio_setting_t;

static const audio_setting_t dry_setting = {
    .level = 100, .amount = 127, .a = 100, .b = 96, .c = 96,
    .pattern = 1, .rate = 1, .depth = 127
};

static uint64_t abs64(int32_t v) { return v < 0 ? -(int64_t)v : v; }

static void audio_setup(unsigned kit, const audio_setting_t *s)
{
    unsigned t;
    setup(JAM_HOME_SOUND);
    for (t = 0; t < NTRK; t++) steps_clear(&trk[t]);
    memset(&drums, 0, sizeof drums);
    drums.set = -2;
    song.g[G_CLOCK] = song.g[G_TXSRC] = song.g[G_DUST] = song.g[G_DUCK] = 0;
    song.g[G_FILT] = 0;
    song.g[G_DRLVL] = s->level;
    song.g[G_DRREV] = s->room;
    song.g[G_DRDLY] = s->echo;
    song.g[G_DMIX] = 100;
    song.g[G_DFDBK] = 75;
    TDRUM->p[P_E0] = (int16_t)kit;
    TDRUM->p[P_PAN] = s->pan;
    TDRUM->p[P_TFLT] = s->filter;
    TDRUM->p[P_ITYPE] = s->insert;
    TDRUM->p[P_IMIX] = s->amount;
    TDRUM->p[P_IA] = s->a;
    TDRUM->p[P_IB] = s->b;
    TDRUM->p[P_IC] = s->c;
    TDRUM->p[P_SLCR] = s->slicer;
    TDRUM->p[P_SLPAT] = s->pattern;
    TDRUM->p[P_SLRATE] = s->rate;
    TDRUM->p[P_SLDEPTH] = s->depth;
    TDRUM->p[P_DIST] = TDRUM->p[P_CHOR] = TDRUM->p[P_DLY] =
        TDRUM->p[P_REV] = s->unused_synth_fx;
    slicer_start(0);
}

/* A short hand-played phrase, followed by silence, distinguishes a genuine
 * echo/room tail from merely changing the loudness of the original hits. */
static void render_audio(unsigned kit, const audio_setting_t *s, audio_render_t *r)
{
    static const uint8_t notes[] = {36, 38, 42, 46, 38, 36};
    unsigned frame_index, hit = 0;
    int32_t out[CTL * 2u];
    audio_setup(kit, s);
    memset(r, 0, sizeof *r);
    for (frame_index = 0; frame_index < AUDIO_FRAMES; frame_index += CTL) {
        unsigned i, n = AUDIO_FRAMES - frame_index;
        if (n > CTL) n = CTL;
        if (hit < NELEM(notes) && frame_index >= hit * FS / 8u) {
            drum_on(notes[hit], 100);
            hit++;
        }
        mix_block(out, n);
        for (i = 0; i < n; i++) {
            unsigned at = (frame_index + i) * 2u;
            uint64_t l = abs64(out[i * 2u]), q = abs64(out[i * 2u + 1u]);
            r->pcm[at] = (int16_t)out[i * 2u];
            r->pcm[at + 1u] = (int16_t)out[i * 2u + 1u];
            r->left += l;
            r->right += q;
            if (frame_index + i >= FS) r->tail += l + q;
            if (l > r->peak) r->peak = (uint32_t)l;
            if (q > r->peak) r->peak = (uint32_t)q;
            r->delay_send += abs64(send_d[i]);
            r->room_send += abs64(send_r[i]);
        }
    }
}

static int transfer_all(int fd, void *data, size_t n, int writing)
{
    uint8_t *p = data;
    while (n) {
        ssize_t done = writing ? write(fd, p, n) : read(fd, p, n);
        if (done < 0 && errno == EINTR) continue;
        if (done <= 0) return 0;
        p += done;
        n -= (size_t)done;
    }
    return 1;
}

static void capture(unsigned kit, const audio_setting_t *s, audio_render_t *r)
{
    int fds[2], status;
    pid_t pid;
    if (pipe(fds)) { perror("pipe"); exit(2); }
    fflush(stdout);
    pid = fork();
    if (pid < 0) { perror("fork"); exit(2); }
    if (!pid) {
        static audio_render_t child_audio;
        int ok;
        close(fds[0]);
        render_audio(kit, s, &child_audio);
        ok = transfer_all(fds[1], &child_audio, sizeof child_audio, 1);
        close(fds[1]);
        _exit(ok ? 0 : 2);
    }
    close(fds[1]);
    if (!transfer_all(fds[0], r, sizeof *r, 0)) {
        fprintf(stderr, "Incomplete audio from child process\n");
        exit(2);
    }
    close(fds[0]);
    if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) || WEXITSTATUS(status)) {
        fprintf(stderr, "Audio child process failed\n");
        exit(2);
    }
}

static uint64_t difference(const audio_render_t *a, const audio_render_t *b)
{
    unsigned i;
    uint64_t result = 0;
    for (i = 0; i < AUDIO_FRAMES * 2u; i++)
        result += abs64((int32_t)a->pcm[i] - b->pcm[i]);
    return result;
}

static void audio_expect(int ok, unsigned kit, const char *what)
{
    char label[180];
    snprintf(label, sizeof label, "%s: %s", DRUM_KIT_NAMES[kit], what);
    expect(ok, label);
}

static void test_kit_audio(unsigned kit)
{
    static audio_render_t dry, changed, left, gated;
    audio_setting_t s;
    unsigned ty;
    capture(kit, &dry_setting, &dry);
    audio_expect(dry.left > 100000 && dry.right == dry.left && dry.peak < 32767,
                 kit, "dry kit is audible, centered and bounded");
    capture(kit, &dry_setting, &changed);
    audio_expect(!difference(&dry, &changed), kit,
                 "independent renders from identical state are byte-identical");

    s = dry_setting; s.level = 30;
    capture(kit, &s, &changed);
    audio_expect(changed.left > 0 && changed.left * 2u < dry.left, kit,
                 "kit Level audibly reduces the actual drum signal");
    s.level = 0;
    capture(kit, &s, &changed);
    audio_expect(!changed.left && !changed.right, kit, "kit Level zero silences the phrase");

    s = dry_setting; s.pan = -64;
    capture(kit, &s, &left);
    s.pan = 63;
    capture(kit, &s, &changed);
    audio_expect(left.left > left.right * 20u && changed.right > changed.left * 20u,
                 kit, "Pan moves the actual dry drums to the chosen stereo side");

    s = dry_setting; s.filter = -58;
    capture(kit, &s, &changed);
    audio_expect(changed.left > 1000 && difference(&dry, &changed) > dry.left / 5u,
                 kit, "kit low-pass changes audible drum output");
    s.filter = 58;
    capture(kit, &s, &changed);
    audio_expect(changed.left > 1000 && difference(&dry, &changed) > dry.left / 5u,
                 kit, "kit high-pass changes audible drum output");

    s = dry_setting; s.echo = 95;
    capture(kit, &s, &changed);
    audio_expect(!dry.delay_send && changed.delay_send > 100000 &&
                     changed.tail > dry.tail + 100000 && difference(&dry, &changed) > 100000,
                 kit, "Echo feeds the drum delay bus and creates an audible tail");
    s = dry_setting; s.room = 95;
    capture(kit, &s, &changed);
    audio_expect(!dry.room_send && changed.room_send > 100000 &&
                     changed.tail > dry.tail + 100000 && difference(&dry, &changed) > 100000,
                 kit, "Room feeds the drum reverb bus and creates an audible tail");

    s = dry_setting; s.insert = IT_FUZZ; s.amount = 0;
    capture(kit, &s, &changed);
    audio_expect(!difference(&dry, &changed), kit, "insert Amount zero leaves the audio unchanged");
    for (ty = IT_SOFT; ty < IT_N; ty++) {
        char label[100];
        s = dry_setting; s.insert = (int)ty;
        capture(kit, &s, &changed);
        snprintf(label, sizeof label, "%s insert changes audible drums and stays bounded", N_ITYPE[ty]);
        audio_expect(changed.left > 1000 && changed.peak < 32767 &&
                         difference(&dry, &changed) > 100000,
                     kit, label);
    }

    s = dry_setting; s.slicer = SL_GATE;
    capture(kit, &s, &gated);
    audio_expect(gated.left > 1000 && difference(&dry, &gated) > dry.left / 10u,
                 kit, "Gate rhythmically changes the actual drum phrase");
    s.depth = 0;
    capture(kit, &s, &changed);
    audio_expect(!difference(&dry, &changed), kit, "Gate Depth zero leaves the audio unchanged");
    s.depth = 127; s.pattern = 13;
    capture(kit, &s, &changed);
    audio_expect(difference(&gated, &changed) > dry.left / 10u, kit,
                 "Gate Pattern changes which drum hits are heard");
    s.pattern = 1; s.rate = 2;
    capture(kit, &s, &changed);
    audio_expect(difference(&gated, &changed) > dry.left / 10u, kit,
                 "Gate Rate changes the rhythm of the audible drums");
    s = dry_setting;
    s.slicer = SL_STUT;
    capture(kit, &s, &changed);
    audio_expect(changed.left > 1000 && changed.peak < 32767 &&
                     difference(&dry, &changed) > dry.left / 10u,
                 kit, "Stutter repeats the actual drum phrase and stays bounded");

    s = dry_setting; s.unused_synth_fx = 127;
    capture(kit, &s, &changed);
    audio_expect(!difference(&dry, &changed), kit,
                 "generic synth FX have no drum effect and must not be offered as kit controls");
}

int main(void)
{
    static const unsigned kits[] = {0, DRUM_SAMPLED, DRUM_SYN};
    unsigned i;
    for (i = 0; i < NELEM(kits); i++) test_kit_audio(kits[i]);
    printf("drum kit audio: %u checks, %d failures; sampled/factory synth/SYN production DSP\n",
           checks, fails);
    return fails ? 1 : 0;
}
