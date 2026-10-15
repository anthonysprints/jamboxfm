/* SPDX-License-Identifier: GPL-3.0-only
 * Research-only route inventory using the shared UI harness. Not a device build. */
#define JAMBOX_PROTO 1
#define UI_PAGES_HARNESS_ONLY 1
#include "../../../tests/ui_pages_test.c"

static void setup(void)
{
    memset(&ui, 0, sizeof ui);
    memset(encs, 0, sizeof encs);
    panel = PANEL_DEFAULT;
    layers_init();
    palette_set(1);
    host_tracks_init();
    jam_new_dusty();
    song.sel = TRK_DRUM;
    go_home();
    frame();
}

static void report(const char *action)
{
    const page_t *p = cur_page();
    int n = 0;
    int16_t *value;
    for (unsigned k = 0; k < 4; k++)
        if (page_desc(p, k, &value)) n++;
    printf("%s: home=%u page=%s scope=%u meaningful_columns=%d drum_grid=%u layer=%u\n",
           action, ui.home, p->title, p->scope, n, on_drum_page(), ui.layer);
}

int main(void)
{
    static const unsigned buttons[] = {
        B_SEQ, B_EDIT, B_ENV, B_LFO, B_SCL, B_ARP, B_FX, B_GLO
    };
    static const char *const names[] = {
        "SEQ", "EDIT", "ENV", "LFO", "SEL", "ARP", "FX", "GLO"
    };
    outdir = "build/drum-research-screens";
    for (unsigned i = 0; i < 8; i++) {
        setup();
        for (unsigned j = 0; j < 3; j++) {
            tap(buttons[i]);
            report(names[i]);
            if (!j) ppm(names[i]);
        }
    }
    setup();
    press(B_SEQ);
    frames(35);
    tap(B_HOME);
    release(B_SEQ);
    report("hold SEQ + HOME then release SEQ");
    ppm("locked-SEQ");

    setup();
    studio_open(SC_DRUM);
    drum_page = 0;
    frame();
    report("internal existing dedicated GRID");
    ppm("dedicated-GRID");
    drum_page = 1;
    ui.force = 1;
    frame();
    ppm("dedicated-KIT");
    return 0;
}
