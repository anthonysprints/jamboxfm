/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* SLOOP (based on FELUCCA): one compilation unit (the HAL is header-only). Order matters. */
#ifndef JAMBOX_PROTO
#define JAMBOX_PROTO 0
#endif
#ifndef FELUCCA_FLASH
#define FELUCCA_FLASH (!JAMBOX_PROTO)
#endif
#ifndef FELUCCA_OTA
#define FELUCCA_OTA (!JAMBOX_PROTO)
#endif
#ifndef FELUCCA_OTA_DRYRUN
#define FELUCCA_OTA_DRYRUN 0
#endif
#ifndef FELUCCA_OTA_RAMONLY
#define FELUCCA_OTA_RAMONLY 0
#endif
#include "app_policy.h"
#include "jam_recovery_api.h"
#if JAMBOX_PROTO
#include "jam_identity.h"
#endif
#if JAMBOX_PROTO && !defined(FELUCCA_VERSION)
#define FELUCCA_VERSION JAMBOX_NAME " " JAMBOX_RELEASE
#endif
#include <stdint.h>
#if FELUCCA_OTA
static uint8_t recovery_active;
#if JAMBOX_PROTO
#define JAMBOX_USB_RESCUE (recovery_active != 0u)
#endif
#endif
#include "fm1_time.h"
#include "fm1_sys.h"
#include "fm1_irq.h"
#include "fm1_guard.h"
#include "fm1_input.h"
#include "fm1_timer.h"
#include "fm1_audio.h"
#include "fm1_adc.h"
#include "fm1_lcd_hw.h"
#include "felucca_tables.h"

#include "libc.c"
#include "lcd.c"
#include "gfx.c"
#include "core.h"
#include "engines.c"
#include "drums.c"
#include "params.c"
#include "voice.c"
#include "slicer.c"          /* per-track SLICER insert, used by fx.c */
#include "fx.c"
#ifndef FELUCCA_ID
#if JAMBOX_PROTO
#define FELUCCA_ID JAMBOX_APP_ID
#else
#define FELUCCA_ID "FM-1_900"    /* package identity (build.py: the .fwsc marker string) */
#endif
#endif
#ifndef FELUCCA_CDC
#define FELUCCA_CDC 1            /* USB CDC-ACM serial console */
#endif
#ifndef FELUCCA_UAC                      /* (before usb.c: it defaults to 0) */
#define FELUCCA_UAC 1            /* USB audio input: the master output, 16-bit stereo 44.1 kHz (usb.c, after Felucca 1.0) */
#endif
#ifndef FELUCCA_UAC_TONE
#define FELUCCA_UAC_TONE 0       /* bench: the USB input sends test triangles instead of the music */
#endif
#include "usb.c"
#ifndef FELUCCA_UART
#define FELUCCA_UART 1           /* TRS MIDI IN on UART1 (3.5 mm jack) */
#endif
#if FELUCCA_UART
#include "midi_uart.c"
#endif
#define FELUCCA_ARRANGER 1
#include "arranger.c"
#include "seq.c"
#if JAMBOX_PROTO
#include "jam_palette.c"
#endif
#include "audio.c"
#include "panel.c"
#include "ui.c"
#include "ui_song.c"
#include "ui_studio.c"
#include "icons.c"           /* parameter icons (FELUCCA_ICONS), used by ui_draw.c */
#include "ui_draw.c"
#include "ui_vis.c"           /* the full-screen visualiser (HOME on HOME, 2.4) */
#include "ui_layers.c"       /* hold a function button: what the keys and knobs do (TE style) */
#include "ui_menu.c"
#include "ui_input.c"
#if FELUCCA_FLASH
#include "fm1_flash.h"
#include "flash_access.c"
#include "storage.c"
#endif
#include "upreset.c"          /* user presets (RAM mirror; flash with FELUCCA_PERSIST) */
#include "project.c"
#if FELUCCA_OTA
#if JAMBOX_PROTO
#define OTA_IDENTITY (recovery_active ? JAMBOX_RESCUE_ID : FELUCCA_ID)
#define OTA_JAMBOX_MODE (recovery_active ? JAMBOX_MODE_RESCUE : JAMBOX_MODE_APP)
#else
#define OTA_IDENTITY (recovery_active ? "FM-1_000" : FELUCCA_ID)
#endif
#include "ota.c"
static void recovery_poll(void);
static uint32_t ota_now_ms(void) { return fm1_ms; }
static void ota_idle(void)
{
    fm1_wdt_feed();
    if (recovery_active) recovery_poll();
}
#include "ota_flash_access.c"
static void ota_show(uint32_t step, int32_t code)
{
    static const char *const STEP[] = {"", "PACKAGE", "CHECK HEAD", "LOADER", "CONFIRM", "RESTART"};
    char b[24];
    if (recovery_active) return;             /* keep polled USB alive during OTA */
    lcd_fill(0, 0, 240, 240, C_BLACK);
    draw_text_box(0, 92, 240, &FONT_S, "UPDATE", C_WHITE, 1);
    if (step < 9u) {
        draw_text_box(0, 124, 240, &FONT_S, STEP[step < 6u ? step : 0], C_HI, 1);
        return;
    }
    if (code == 1)
        str_cpy(b, "DRY RUN OK", sizeof b);
    else {
        uint32_t k = 0, v = (uint32_t)-code;
        str_cpy(b, "FAILED  -", sizeof b);
        while (b[k])
            k++;
        if (v >= 10u)
            b[k++] = (char)('0' + v / 10u);
        b[k++] = (char)('0' + v % 10u);
        b[k] = 0;
    }
    draw_text_box(0, 124, 240, &FONT_S, b, C_HI, 1);
    fm1_delay_ms(1500);
}
static void ota_commit(const uint8_t *parm)
{
    bootguard_clear(&bootguard);                        /* intentional update reset */
    usb_detach();
    fm1_delay_ms(30);
    fm1_enter_update(parm);                             /* record into RAM, core reset (fm1_sys.h) */
}
#endif
#if FELUCCA_OTA
#include "editor.c"          /* web editor SysEx (needs the OTA SysEx plumbing) */
#endif
#if FELUCCA_CDC
#include "console.c"
#endif
#if FELUCCA_OTA
#include "recovery.c"        /* early, polled USB updater; no synth or settings */
#endif
#include "splash.c"          /* SLOOP boot logo (build/gen/sloop_logo.h) */
#include "instrument_init.c"
#include "jam_recovery_runtime.c"
#include "main.c"
