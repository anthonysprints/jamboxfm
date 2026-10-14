/* SPDX-License-Identifier: GPL-3.0-only */
#if JAMBOX_PROTO
#include "jam_identity.h"
/* Original JamBox vector/text splash. No upstream logo bitmap is included. */
static void sloop_splash(void)
{
    uint32_t i;
    static const uint8_t bars[4] = {34, 54, 42, 68};
    static const uint16_t colors[4] = {RGB(225, 163, 92), RGB(144, 180, 151),
                                        RGB(204, 133, 116), RGB(174, 164, 200)};
    lcd_fill(0, 0, 240, 240, C_BLACK);
    cv_begin(240, 120, C_BLACK);
    for (i = 0; i < 4u; i++) {
        cv_rect(49 + (int32_t)i * 38, 86 - bars[i], 26, bars[i], colors[i]);
        cv_rect(49 + (int32_t)i * 38, 94, 26, 4, colors[i]);
    }
    cv_blit(0, 12);
    cv_begin(240, 108, C_BLACK);
    cv_text(120 - text_w(&FONT_L, "JAMBOXFM") / 2, 0, &FONT_L, "JAMBOXFM", C_WHITE);
    cv_text(120 - text_w(&FONT_S, JAMBOX_RELEASE) / 2, 38, &FONT_S, JAMBOX_RELEASE, RGB(196, 196, 204));
    cv_text(120 - text_w(&FONT_S, "TEMPORARY SESSION") / 2, 60, &FONT_S, "TEMPORARY SESSION", RGB(225, 163, 92));
    cv_text(120 - text_w(&FONT_S, "SLOOP + FELUCCA") / 2, 88, &FONT_S, "SLOOP + FELUCCA", RGB(96, 96, 104));
    cv_blit(0, 132);
}
#else
/* SLOOP boot splash: the logo (tools/gen_logo.py -> sloop_logo.h: 16 colours, RLE runs of
 * (run - 1) << 4 | colour), drawn into the canvas in bands of at most 120 rows. */
#include "sloop_logo.h"
static void sloop_logo_draw(uint32_t y0)
{
    uint32_t r0;
    for (r0 = 0; r0 < SLOOP_SPLASH_H; r0 += 120u) {
        uint32_t rows = SLOOP_SPLASH_H - r0 > 120u ? 120u : SLOOP_SPLASH_H - r0;
        uint32_t lo = r0 * SLOOP_SPLASH_W, hi = (r0 + rows) * SLOOP_SPLASH_W, pos = 0, i;
        cv_begin(SLOOP_SPLASH_W, rows, C_BLACK);
        for (i = 0; i < sizeof SLOOP_SPLASH_RLE && pos < hi; i++) {
            uint32_t run = (SLOOP_SPLASH_RLE[i] >> 4) + 1u, c = SLOOP_SPLASH_RLE[i] & 15u, p;
            if (c && pos + run > lo)
                for (p = pos < lo ? lo : pos; p < pos + run && p < hi; p++)
                    cv_px[p - lo] = swap16(SLOOP_SPLASH_PAL[c]);
            pos += run;
        }
        cv_blit((240u - SLOOP_SPLASH_W) / 2u, y0 + r0);
    }
}

/* the boot screen: the logo, the version, where it comes from */
static void sloop_splash(void)
{
    lcd_fill(0, 0, 240, 240, C_BLACK);
    sloop_logo_draw(10);
    const char *v = FELUCCA_VERSION;
    if (v[0] == 'S' && v[1] == 'L' && v[5] == ' ')
        v += 6;                                         /* "SLOOP 1.0" -> "1.0" under the wordmark */
    cv_begin(240, 36, C_BLACK);
    cv_text(120 - text_w(&FONT_S, v) / 2, 0, &FONT_S, v, RGB(196, 196, 204));
    cv_text(120 - text_w(&FONT_S, "based on felucca") / 2, 18, &FONT_S, "based on felucca", RGB(96, 96, 104));
    cv_blit(0, 202);
}
#endif
