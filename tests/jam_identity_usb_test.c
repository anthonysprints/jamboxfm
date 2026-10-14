/* SPDX-License-Identifier: GPL-3.0-only */
/* Actual USB descriptor selection; no register/USB access or instrument init. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define JAMBOX_PROTO (TEST_MODE != 0)
#define FELUCCA_CDC 0
#define FELUCCA_UAC 0
#define FELUCCA_OTA 0
#define RING_PUBLISH() ((void)0)
#if TEST_MODE == 3
#define FELUCCA_LOADER 1
#define FELUCCA_USB_PID 0x0002
#elif TEST_MODE != 0
static uint8_t recovery_active = TEST_MODE == 2;
#define JAMBOX_USB_RESCUE (recovery_active != 0u)
#endif
static void fm1_delay_ms(uint32_t n) { (void)n; }
#include "../firmware/src/usb.c"

static void expect_string(uint32_t index, const char *text)
{
    uint32_t i;
    uint16_t n;
    const uint8_t *p;
    assert(get_desc(0x0300u | index, &p, &n));
    assert(n == strlen(text) * 2u + 2u && p[0] == n && p[1] == 3);
    for (i = 0; text[i]; i++) assert(p[2u + i * 2u] == (uint8_t)text[i] && p[3u + i * 2u] == 0);
}

int main(void)
{
    uint16_t n;
    const uint8_t *p;
    assert(get_desc(0x0100, &p, &n) && n == 18);
    assert(p[8] == 9 && p[9] == 0x12 && p[10] == (TEST_MODE == 3 ? 2 : 1) && p[11] == 0);
#if JAMBOX_PROTO
    expect_string(1, "JamBoxFM");
    expect_string(2, TEST_MODE == 3 ? "JamBoxFM Update" : TEST_MODE == 2 ? "JamBoxFM Rescue" : "JamBoxFM");
#if TEST_MODE != 3
    recovery_active = !recovery_active;
    expect_string(2, recovery_active ? "JamBoxFM Rescue" : "JamBoxFM");
#endif
#else
    expect_string(1, "H\xFCgelton Instruments");
    expect_string(2, "Felucca");
#endif
    printf("USB identity mode %d: manufacturer, product, descriptor lengths and VID/PID passed\n", TEST_MODE);
    return 0;
}
