/* SPDX-License-Identifier: GPL-3.0-only */
/* Research only: current production panel/LCD/DSP in a native host fixture.
 * No firmware image, flash writer, MIDI connection or website deployment. */
#define JAM_DRUM_NAVIGATION_HARNESS_ONLY 1
#include "../../../tests/jam_drum_navigation_test.c"

static void load_sound(unsigned part, unsigned sound)
{
    setup(JAM_HOME_SOUND);
    song.sel = (uint8_t)part;
    if (!jam_sound_load(part, sound)) exit(2);
    frames(24); /* engine transition, pending FM6 patch and release quiet period */
}
static const char *visible_title(const page_t *p)
{
    return p->scope == SC_ENGINE ? ENGINES[TSEL->eng_req]->page_title[p->id[0] != P_E0] : p->title;
}
static void inventory(void)
{
    static const unsigned buttons[] = {B_EDIT, B_ENV, B_LFO, B_FX};
    unsigned part, sound, family, records = 0;
    unsigned screenshots = 0;
    printf("part\tsound\tengine\tbutton\tinternal_page\tvisible_title\tknob1\tknob2\tknob3\tknob4\n");
    for (part = 0; part < NPART; part++) for (sound = 0; sound < JAM_MELODIC_SOUNDS; sound++) {
        load_sound(part, sound);
        for (family = 0; family < NELEM(buttons); family++) {
            uint8_t seen[NPAGES] = {0};
            go_home();
            for (unsigned n = 0; n < NPAGES; n++) {
                tap(buttons[family]);
                if (seen[ui.page]) break;
                seen[ui.page] = 1;
                const page_t *p = cur_page();
                printf("%s\t%s\t%s\t%u\t%s\t%s", jam_role_name(part), jam_sound_name(part), ENGINES[TSEL->eng_req]->name, buttons[family], p->title, visible_title(p));
                for (unsigned k = 0; k < 4; k++) {
                    int16_t *v;
                    const param_desc_t *d = page_desc(p, k, &v);
                    if (!d || !v) printf("\tabsent");
                    else printf("\t%s:%d:%d:%d", d->label, d->min, d->max, *v);
                }
                putchar('\n'); records++;
                if (family == 0 && p->scope == SC_ENGINE && !(screenshots & (1u << TSEL->eng_req))) {
                    char name[40];
                    snprintf(name, sizeof name, "edit-%s", ENGINES[TSEL->eng_req]->name);
                    ui.force = 1; frame(); ppm(name);
                    screenshots |= 1u << TSEL->eng_req;
                }
                if (TSEL->eng_req == ENGI_FM6 && family == 1 && p->graph == GR_ADSR) {
                    ui.force = 1; frame(); ppm("fm6-visible-ENV");
                }
            }
        }
    }
    fprintf(stderr, "Inventory: %u curated sounds, %u reachable page records; host-only production UI.\n", NPART * JAM_MELODIC_SOUNDS, records);
}

typedef struct { const char *name; unsigned part, sound, id; int low, high, gate_id, gate_value; } case_t;
static const case_t cases[] = {
    {"analog_detune_second_osc_off",0,0,P_E1,0,100,P_E2,0},
    {"analog_detune_second_osc_on",0,0,P_E1,0,100,P_E2,64},
    {"analog_shape_sine",2,0,P_LD_SHP,0,64,P_E0,3},
    {"analog_shape_pwm",2,0,P_LD_SHP,0,64,P_E0,4},
    {"organ_env_filter",1,6,P_ED_FLT,0,64,-1,0},
    {"organ_lfo_filter",1,6,P_LD_FLT,0,64,-1,0},
    {"organ_lfo_shape",1,6,P_LD_SHP,0,64,-1,0},
    {"organ_lfo_pitch_control",1,6,P_LD_PIT,0,24,-1,0},
    {"sample_lfo_shape",1,5,P_LD_SHP,0,64,-1,0},
    {"sample_lfo_filter_control",1,5,P_LD_FLT,0,64,P_E4,60},
    {"fm6_attack",1,10,P_ATK,0,127,-1,0},
    {"fm6_release",1,10,P_REL,0,127,-1,0},
    {"fm6_decay",1,10,P_DEC,0,127,-1,0},
    {"fm6_sustain",1,10,P_SUS,0,127,-1,0},
    {"fm6_env_filter",1,10,P_ED_FLT,0,64,-1,0},
    {"fm6_env_shape",1,10,P_ED_SHP,0,64,-1,0},
    {"fm6_pitch_env_control",1,10,P_ED_PIT,0,24,-1,0},
    {"fm6_mod_env_control",1,10,P_E4,-40,40,-1,0},
    {"analog_attack_control",2,0,P_ATK,0,100,-1,0},
    {"organ_attack_control",1,6,P_ATK,0,100,-1,0},
    {"analog_lfo_rate_without_destination",2,0,P_LRATE,5,90,-1,0},
    {"analog_lfo_rate_with_destination",2,0,P_LRATE,5,90,P_LD_FLT,32},
    {"digital_env_filter_controls_fm",1,2,P_ED_FLT,0,50,-1,0},
};
typedef struct { uint64_t hash, energy; } render_t;
static render_t render(const case_t *c, int high)
{
    render_t result = {1469598103934665603ull, 0};
    load_sound(c->part, c->sound);
    for (unsigned t = 0; t < NTRK; t++) steps_clear(&trk[t]);
    song.g[G_CLOCK] = song.g[G_DUST] = song.g[G_DUCK] = song.g[G_FILT] = 0;
    TSEL->p[P_DIST] = TSEL->p[P_CHOR] = TSEL->p[P_DLY] = TSEL->p[P_REV] = 0;
    TSEL->p[P_TFLT] = TSEL->p[P_ITYPE] = TSEL->p[P_SLCR] = 0;
    TSEL->p[P_LRATE] = 60;
    TSEL->p[P_LD_PIT] = TSEL->p[P_LD_FLT] = TSEL->p[P_LD_SHP] = TSEL->p[P_LD_AMP] = 0;
    TSEL->p[P_ED_PIT] = TSEL->p[P_ED_FLT] = TSEL->p[P_ED_SHP] = 0;
    if (c->gate_id >= 0) TSEL->p[c->gate_id] = (int16_t)c->gate_value;
    TSEL->p[c->id] = (int16_t)(high ? c->high : c->low);
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
/* Each low/high render forks the same untouched parent to preserve every
 * production RNG, oscillator and effect history without reset lookalikes. */
static render_t capture(const case_t *c, int high)
{
    int fds[2], status;
    render_t r;
    if (pipe(fds)) exit(2);
    fflush(stdout);
    pid_t pid = fork();
    if (pid < 0) exit(2);
    if (!pid) {
        close(fds[0]); r = render(c, high);
        if (write(fds[1], &r, sizeof r) != sizeof r) _exit(2);
        close(fds[1]); _exit(0);
    }
    close(fds[1]);
    if (read(fds[0], &r, sizeof r) != sizeof r) exit(2);
    close(fds[0]);
    if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) || WEXITSTATUS(status)) exit(2);
    return r;
}
static void routes(void)
{
    load_sound(0, 0); tap(B_EDIT);
    int bpm = song.g[G_BPM];
    printf("EDIT first: %s / BPM %d\n", visible_title(cur_page()), bpm);
    turn(EN_SELECT, 1);
    printf("SELECT +1: %s / BPM %d\n", visible_title(cur_page()), song.g[G_BPM]);
    if (song.g[G_BPM] != bpm) exit(2);
    tap(B_HOME); tap(B_EDIT);
    printf("HOME then EDIT: %s (remembered)\n", visible_title(cur_page()));
    tap(B_EDIT); /* Analog FILTER. */
    printf("next EDIT: %s\n", visible_title(cur_page()));
    if (!jam_sound_load(0, 9)) exit(2); frames(24);
    printf("same part switches to DIGITAL: %s / shown=%d\n", visible_title(cur_page()), page_shown(cur_page()));
    if (!page_shown(cur_page())) exit(2);
    tap(B_HOME); turn(EN_SELECT, 1);
    printf("HOME SELECT +1: BPM %d\n", song.g[G_BPM]);
    if (song.g[G_BPM] != bpm + 1) exit(2);
    tap(B_EDIT); track_select(TRK_DRUM); frame();
    printf("EDIT page switches to Drums: home=%u scope=%u page=%s\n", ui.home, cur_page()->scope, cur_page()->title);
    if (!ui.home && !page_shown(cur_page())) exit(2);
}
int main(int argc, char **argv)
{
    outdir = "build/sound-page-research/screens";
    if (argc > 1 && !strcmp(argv[1], "--audio")) {
        puts("case\tlow_hash\thigh_hash\tlow_energy\thigh_energy\tchanged");
        for (unsigned i = 0; i < NELEM(cases); i++) {
            render_t a = capture(&cases[i], 0), b = capture(&cases[i], 1);
            if (!a.energy || !b.energy) { fprintf(stderr, "Silent render: %s\n", cases[i].name); return 2; }
            printf("%s\t%016llx\t%016llx\t%llu\t%llu\t%s\n", cases[i].name, (unsigned long long)a.hash, (unsigned long long)b.hash, (unsigned long long)a.energy, (unsigned long long)b.energy, a.hash != b.hash ? "yes" : "no");
        }
    } else if (argc > 1 && !strcmp(argv[1], "--routes")) routes();
    else inventory();
    return fails ? 1 : 0;
}
