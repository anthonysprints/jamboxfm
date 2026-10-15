/* SPDX-License-Identifier: GPL-3.0-only */
/* Actual DSP checks for the conditional UI routes. Every render forks the
 * same untouched parent; no production RNG or envelope reset lookalikes. */
#define JAM_DRUM_NAVIGATION_HARNESS_ONLY 1
#include "jam_drum_navigation_test.c"
typedef struct { uint64_t hash, energy; } render_t;

typedef struct { unsigned engine, mode, id; int low, high; } extra_case_t;
static render_t extra_render(const extra_case_t *c, int high)
{
    render_t result = {1469598103934665603ull, 0};
    setup(JAM_HOME_SOUND);
    song.sel = 2;
    set_engine(c->engine);
    frames(24);
    for (unsigned t = 0; t < NTRK; t++) steps_clear(&trk[t]);
    song.g[G_CLOCK] = song.g[G_DUST] = song.g[G_DUCK] = song.g[G_FILT] = 0;
    TSEL->p[P_DIST] = TSEL->p[P_CHOR] = TSEL->p[P_DLY] = TSEL->p[P_REV] = 0;
    TSEL->p[P_TFLT] = TSEL->p[P_ITYPE] = TSEL->p[P_SLCR] = 0;
    TSEL->p[P_LRATE] = 60;
    TSEL->p[P_LD_PIT] = TSEL->p[P_LD_FLT] = TSEL->p[P_LD_SHP] = TSEL->p[P_LD_AMP] = 0;
    TSEL->p[P_ED_PIT] = TSEL->p[P_ED_FLT] = TSEL->p[P_ED_SHP] = 0;
    TSEL->p[P_E0] = c->mode;
    /* Keep a changing modulation destination away from index saturation. */
    if (c->engine == 1) {
        TSEL->p[P_E4] = 40;
        TSEL->p[P_E5] = 45;
        TSEL->p[P_E6] = 8;
    }
    TSEL->p[c->id] = high ? c->high : c->low;
    fm6_poll();
    trk_note_on(TSEL, 60, 100);
    for (unsigned at = 0; at < FS * 2; at += CTL) {
        int32_t out[CTL * 2];
        if (at == (FS / 2 / CTL) * CTL) trk_note_off(TSEL, 60);
        mix_block(out, CTL);
        for (unsigned i = 0; i < CTL * 2; i++) {
            result.hash = (result.hash ^ (uint32_t)out[i]) * 1099511628211ull;
            result.energy += out[i] < 0 ? -(int64_t)out[i] : out[i];
        }
    }
    return result;
}
static render_t extra_capture(const extra_case_t *c, int high)
{
    int fds[2], status;
    render_t r;
    if (pipe(fds)) exit(2);
    fflush(stdout);
    pid_t pid = fork();
    if (pid < 0) exit(2);
    if (!pid) {
        close(fds[0]); r = extra_render(c, high);
        if (write(fds[1], &r, sizeof r) != sizeof r) _exit(2);
        close(fds[1]); _exit(0);
    }
    close(fds[1]);
    if (read(fds[0], &r, sizeof r) != sizeof r) exit(2);
    close(fds[0]);
    if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) || WEXITSTATUS(status)) exit(2);
    return r;
}
static unsigned extra_checks, extra_fail;
static void extra_compare(const char *name, const extra_case_t *c, int expected)
{
    render_t a = extra_capture(c, 0), b = extra_capture(c, 1);
    int changed = a.hash != b.hash;
    int ok = a.energy && b.energy && changed == expected;
    extra_checks++;
    extra_fail += !ok;
    printf("%s\t%u\t%u\t%u\t%016llx\t%016llx\t%llu\t%llu\t%d\t%d\t%s\n", name, c->engine, c->mode, c->id,
        (unsigned long long)a.hash, (unsigned long long)b.hash, (unsigned long long)a.energy, (unsigned long long)b.energy,
        expected, changed, ok ? "PASS" : "FAIL");
}
int main(void)
{
    static const unsigned buses[] = {P_ED_FLT, P_ED_SHP, P_LD_FLT, P_LD_SHP};
    static const char *names[] = {"digital_env_cutoff_bus", "digital_env_shape_bus", "digital_lfo_cutoff_bus", "digital_lfo_shape_bus"};
    static const unsigned trio_controls[] = {P_ED_SHP, P_LD_SHP, P_E7};
    static const char *trio_names[] = {"trio_env_pwm_bus", "trio_lfo_pwm_bus", "trio_static_pw"};
    puts("case\tengine\tmode\tparam_id\tlow_hash\thigh_hash\tlow_energy\thigh_energy\texpect_changed\tactual_changed\tresult");
    for (unsigned i = 0; i < 4; i++) for (unsigned alg = 0; alg <= 7; alg += 7) {
        extra_case_t c = {1, alg, buses[i], 0, 48};
        extra_compare(names[i], &c, alg == 0);
    }
    for (unsigned i = 0; i < 3; i++) for (unsigned wave = 0; wave < 16; wave++) {
        /* Expected set is specified independently, not read from UI helper. */
        int expected = wave == 1 || wave == 2 || wave == 6 || wave == 7 || wave == 10 || wave == 15;
        extra_case_t c = {6, wave, trio_controls[i], i == 2 ? 32 : 0, i == 2 ? 96 : 48};
        extra_compare(trio_names[i], &c, expected);
    }
    fprintf(stderr, "Extended DSP checks: %u pairs, %u failed; 112 non-silent production renders.\n", extra_checks, extra_fail);
    return extra_fail ? 1 : 0;
}
