/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 JamBoxFM contributors */
/* Browser panel adapter: the firmware DSP, sequencer, input and LCD renderer.
 * One AudioWorklet owns this instance. No device registers, flash, file I/O,
 * emulated USB connection or Web MIDI are needed. All persistence is disabled.
 * Initialize once per instance; recreate the WASM instance for a cold boot. */
#include <stdint.h>
#include <string.h>
#define JAMBOX_PROTO 1
#define FELUCCA_ARRANGER 1
#define FELUCCA_FLASH 0
#include "../src/app_policy.h"
#define FELUCCA_OTA 0
#define FELUCCA_ICONS 0
#define FELUCCA_SLICE 0
#define FELUCCA_VERSION "JAMBOX PANEL PREVIEW"
#define memset felucca_memset
#define memcpy felucca_memcpy
#define memcmp felucca_memcmp
#include "../src/libc.c"
#undef memset
#undef memcpy
#undef memcmp
#include "felucca_tables.h"
static uint8_t user_flash[4][0x14000];
#define SMP_USER_XIP(k) (user_flash[(k) & 3u])
static struct { volatile uint32_t notes, buttons; } fm1_in;
static void fm1_irq_off(void) {}
static void fm1_irq_on(void) {}
#include "../src/core.h"
#include "../src/engines.c"
#include "../src/drums.c"
#include "../src/params.c"
#include "../src/voice.c"
#include "../src/slicer.c"
#include "../src/fx.c"
static void fm1_delay_ms(uint32_t ms) { (void)ms; }
#include "../src/usb.c"
#include "../src/midi_uart.c"
#include "../src/arranger.c"
#include "../src/seq.c"
#include "../src/jam_palette.c"

/* LCD transfers carry big-endian RGB565, exactly as on the physical panel.
 * Convert only changed strips into the browser's persistent RGBA framebuffer. */
static uint8_t emu_rgba[240u * 240u * 4u];
static uint32_t emu_screen_revision, emu_lcd_errors;
static void lcd_sync(void) {}
static int emu_lcd_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    if (x > 240u || y > 240u || w > 240u - x || h > 240u - y) {
        emu_lcd_errors++;
        return 0;
    }
    return 1;
}
static void emu_pixel(uint32_t x, uint32_t y, uint16_t rgb)
{
    uint8_t *p = emu_rgba + (y * 240u + x) * 4u;
    p[0] = (uint8_t)((rgb >> 11) * 255u / 31u);
    p[1] = (uint8_t)(((rgb >> 5) & 63u) * 255u / 63u);
    p[2] = (uint8_t)((rgb & 31u) * 255u / 31u);
    p[3] = 255;
}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *src)
{
    uint32_t i, j;
    if (!emu_lcd_rect(x, y, w, h)) return;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++) {
            uint16_t p = src[j * w + i];
            emu_pixel(x + i, y + j, (uint16_t)((p >> 8) | (p << 8)));
        }
    emu_screen_revision++;
}
#include "../src/gfx.c"
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t rgb)
{
    uint32_t i, j;
    if (!emu_lcd_rect(x, y, w, h)) return;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++) emu_pixel(x + i, y + j, rgb);
    emu_screen_revision++;
}

static int32_t emu_enc[7];
static uint32_t emu_button_edges, emu_notes_seen;
static uint32_t fm1_ticks(void) { return fm1_ms * 24000u; }
#define FM1_TICKS_PER_US 24u
static int32_t fm1_enc_take(uint32_t e)
{
    int32_t steps = emu_enc[e];
    emu_enc[e] = 0;
    return steps;
}
/* The same matrix identifiers as firmware/hal/fm1_input.h, with no GPIO code. */
#define FM1_NCOL 11u
static const int8_t FM1_KEYMAP[5][FM1_NCOL] = {
    {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {5,11,4,10,3,9,2,8,-1,-1,-1},
    {34,35,36,37,38,40,39,13,7,6,12},
    {23,24,25,26,27,28,29,30,31,32,33},
    {0,1,15,14,17,16,19,18,20,21,22}
};
static uint8_t fm1_led[FM1_NCOL], fm1_led_dim[FM1_NCOL], fm1_led_bg[FM1_NCOL];
static volatile uint16_t fm1_led_bg_ns;
static void fm1_led_key(uint32_t id, int on) { (void)id; (void)on; }
static uint32_t fm1_input_edges(int unused)
{
    uint32_t e = emu_button_edges;
    (void)unused;
    emu_button_edges = 0;
    return e;
}
static uint32_t fm1_input_note_edges(void)
{
    uint32_t e = fm1_in.notes & ~emu_notes_seen;
    emu_notes_seen = fm1_in.notes;
    return e;
}
static void fm1_wdt_feed(void) { fm1_ms++; }
static int32_t fm1_adc_read(int channel) { (void)channel; return -1; }
static struct { uint32_t magic, stage, page, home, ui_frames; } felucca_dbg;
#define SCOPE_N 512u
static int16_t scope_buf[SCOPE_N], scope_bufr[SCOPE_N];
static uint32_t scope_w;
#include "../src/panel.c"
#include "../src/ui.c"

/* Deliberately empty storage. Never report a successful save or load. */
static uint8_t emu_storage_notice;
static void emu_no_storage(void) { emu_storage_notice = 1; ui_message("STORAGE DISABLED"); }
static int project_used(uint32_t slot) { (void)slot; return 0; }
static void project_save(uint32_t slot) { (void)slot; emu_no_storage(); }
static void project_load(uint32_t slot) { (void)slot; emu_no_storage(); }
static void project_erase(uint32_t slot) { (void)slot; emu_no_storage(); }
static void arrangement_save(void) { emu_no_storage(); }
static uint32_t arrangement_ready(void) { return 0; }
static void arrangement_apply(uint32_t scene) { (void)scene; }
static uint32_t section_bars(uint32_t scene) { (void)scene; return 1; }
static void song_backup(void) {}
static void song_restore(void) {}
static void section_store(uint32_t scene) { (void)scene; emu_no_storage(); }
static void section_load(uint32_t scene) { (void)scene; emu_no_storage(); }
static int up_used(uint32_t slot) { (void)slot; return 0; }
static int up_load(uint32_t slot) { (void)slot; return 0; }
static int up_load_to(track_t *t, uint32_t slot) { (void)t; (void)slot; return 0; }
static uint32_t up_count(void) { return 0; }
static uint32_t up_nth(uint32_t index) { (void)index; return UP_SLOTS; }
static uint32_t up_rank(uint32_t slot) { (void)slot; return 0; }
static void up_name(uint32_t slot, char *name) { (void)slot; str_cpy(name, "EMPTY", 13); }
static void up_slot_label(char *name, uint32_t slot) { fmt_int(name, (int32_t)slot + 1); }
static void up_ui(uint32_t op, uint32_t slot) { (void)op; (void)slot; emu_no_storage(); }
static void settings_save(void) {} /* Settings changes affect this instance only. */
#include "../src/ui_song.c"
#include "../src/ui_studio.c"
#include "../src/icons.c"
#include "../src/ui_draw.c"
#include "../src/ui_vis.c"
#include "../src/ui_layers.c"
#include "../src/ui_menu.c"
/* Keep the hardware calibration loop unreachable in the browser. */
#define panel_setup emu_unused_hardware_calibration
#include "../src/ui_input.c"
#undef panel_setup
static void panel_setup(void) { ui_message("BROWSER PANEL FIXED"); ui.force = 1; }

static uint8_t emu_leds[14u + 27u], emu_initialized;
static uint32_t emu_ui_samples, emu_ms_fraction;
static int32_t emu_audio[CTL * 2u];
static float emu_output[CTL * 2u];

static void emu_input(void)
{
    /* Layer masks and grid/step entry are established before a key can sound. */
    ui_input();
    keyboard_block();
    /* SEQ/SEL/GLO/SAVE and drum-grid keys hand actions back through lk_q.
     * Finish that handoff with the same physical modifier state: waiting for
     * the next browser event could release the layer and discard its press. */
    if (lk_r != lk_w)
        ui_input();
    /* Some upstream callers display SAVED after their void storage callback.
     * Replace that message after both input passes, when the callback is ours. */
    if (emu_storage_notice) {
        emu_storage_notice = 0;
        ui_message("STORAGE DISABLED");
    }
}

void jam_emu_frame(void)
{
    uint32_t i;
    if (!emu_initialized) return;
    emu_input();
    ui_leds();
    ui_draw();
    for (i = 0; i < 41u; i++) {
        uint32_t id = i < NB ? panel.btn[i] : i;
        uint8_t pos = led_pos[id];
        uint8_t bit = (uint8_t)(1u << (pos & 7u));
        emu_leds[i] = pos == 0xFF ? 0 : (fm1_led[pos >> 3] & bit) ? 255 :
            (fm1_led_dim[pos >> 3] & bit) ? 90 : (fm1_led_bg[pos >> 3] & bit) ? 32 : 0;
    }
}

void jam_emu_init(void)
{
    uint32_t i, k;
    if (emu_initialized) return;
    memset(user_flash, 0xFF, sizeof user_flash);
    for (i = 0; i < G_COUNT; i++) song.g[i] = GP[i].def;
    for (k = 0; k < NTRK; k++) {
        for (i = 0; i < P_COUNT; i++) trk[k].p[i] = TP[i].def;
        steps_clear(&trk[k]);
    }
    song.master_q12 = 4096;
    song.g[G_SYNC] = 0;
    fm1_ms = 1;
    dsu_defaults();
    fm6_init();
    arr_defaults(&arrangement);
    panel = PANEL_DEFAULT;
    settings.magic = SETTINGS_MAGIC;
    settings.palette = 1;
    settings.lowcut = settings.zoom = 0;
    settings_init();
    lights_lvl = LIGHTS_MID;
    lights_keys = KEYS_C;
    lights_notes = 1;
    layers_init();
    jam_new_dusty();
    /* Apply initial engine changes and their panic requests before the first
     * user key. This boot block is silent and is not returned to the browser. */
    fm6_poll();
    mix_block(emu_audio, CTL);
    go_home();
    emu_initialized = 1;
    jam_emu_frame();
}

uint32_t jam_emu_frames(void) { return CTL; }
uint32_t jam_emu_rate(void) { return FS; }
uint8_t *jam_emu_screen(void) { return emu_rgba; }
uint32_t jam_emu_screen_version(void) { return emu_screen_revision; }
uint8_t *jam_emu_leds(void) { return emu_leds; }
const char *jam_emu_sound_name(void) { return jam_sound_name(song.sel); }

/* Input IDs are printed roles, not matrix positions. Clockwise is positive. */
void jam_emu_button(uint32_t role, uint32_t on)
{
    uint32_t bit;
    if (!emu_initialized || role >= NB) return;
    bit = 1u << panel.btn[role];
    if (on && !(fm1_in.buttons & bit)) emu_button_edges |= bit;
    if (on) fm1_in.buttons |= bit; else fm1_in.buttons &= ~bit;
    emu_input();
}
void jam_emu_encoder(uint32_t role, int32_t delta)
{
    if (!emu_initialized || role >= NE) return;
    emu_enc[panel.enc[role]] = clamp(emu_enc[panel.enc[role]] + clamp(delta, -1024, 1024) * panel.dir[role], -4096, 4096);
    emu_input();
}
void jam_emu_key(uint32_t key, uint32_t on)
{
    if (!emu_initialized || key >= 27u) return;
    if (on) fm1_in.notes |= 1u << key; else fm1_in.notes &= ~(1u << key);
    emu_input();
}
void jam_emu_master(int32_t percent) { song.master_q12 = (uint32_t)clamp(percent, 0, 100) * 4096u / 100u; }
void jam_emu_release(void)
{
    if (!emu_initialized) return;
    fm1_in.notes = fm1_in.buttons = emu_button_edges = 0;
    memset(emu_enc, 0, sizeof emu_enc);
    emu_input();
}
void jam_emu_panic(void)
{
    uint32_t k;
    jam_emu_release();
    transport_req = 2;
    panic_req = 15;
    for (k = 0; k < NPART; k++) {
        trk[k].nheld = trk[k].arp_phys = trk[k].latched = 0;
        trk[k].arp_note = 0;
    }
}

float *jam_emu_render(void)
{
    uint32_t i;
    if (!emu_initialized) jam_emu_init();
    fm6_poll();
    mix_block(emu_audio, CTL);
    for (i = 0; i < CTL * 2u; i++) emu_output[i] = clamp(emu_audio[i], -32768, 32767) / 32768.0f;
    for (i = 1; i < CTL; i += 2u) {
        scope_bufr[scope_w & (SCOPE_N - 1u)] = vis_tap[2u * i + 1u];
        scope_buf[scope_w++ & (SCOPE_N - 1u)] = vis_tap[2u * i];
    }
    emu_ms_fraction += CTL * 1000u;
    fm1_ms += emu_ms_fraction / FS;
    emu_ms_fraction %= FS;
    emu_ui_samples += CTL;
    if (emu_ui_samples >= FS / 60u) {
        emu_ui_samples -= FS / 60u;
        jam_emu_frame();
    }
    return emu_output;
}

int32_t jam_emu_value(uint32_t field, uint32_t index)
{
    uint32_t i, mask;
    switch (field) {
    case 0: return song.sel;
    case 1: return song.playing;
    case 2: return song.rec;
    case 3: return rec_wait;
    case 4: return fm1_in.notes;
    case 5:
        for (i = 0, mask = 0; i < NB; i++) mask |= ((fm1_in.buttons >> panel.btn[i]) & 1u) << i;
        return (int32_t)mask;
    case 6: return ui.home;
    case 7: return ui.page;
    case 8: return song.g[G_BPM];
    case 9: return song.octave;
    case 10: return (int32_t)ui.frame;
    case 11: return ui.layer;
    case 12: return index < 4u ? (int32_t)jam_macro_get(song.sel, index) : -1;
    case 13: return jam_home_macros_available(song.sel);
    case 14:
        for (i = 0, mask = 0; i < 27u; i++)
            if (kb_kind[i] == KS_NOTE && kb_trk[i] == song.sel) mask += kb_n[i];
        return (int32_t)mask;
    case 15: return song.sel == TRK_DRUM ? -1 : TSEL->eng_req;
    case 16: return (int32_t)((song.master_q12 * 100u + 2048u) / 4096u);
    case 17: return (int32_t)emu_screen_revision;
    case 18: return (int32_t)emu_lcd_errors;
    case 19:
        if (index >= NTRK) return -1;
        for (i = 0, mask = 0; i < NSTEP; i++) {
            if (index == TRK_DRUM) mask += dstep_mask(&trk[index].dstep[i]) != 0;
            else if (trk[index].step[i].time == ST_NOTE) mask += trk[index].step[i].n;
        }
        return (int32_t)mask;
    default: return -1;
    }
}
