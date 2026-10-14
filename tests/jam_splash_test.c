/* SPDX-License-Identifier: GPL-3.0-only */
/* Actual JamBox splash and font renderer, checking canvas/display bounds and
 * readable labels independently of bitmap assets. Optional PPM for review. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define JAMBOX_PROTO 1
static uint16_t screen[240u * 240u];
static uint32_t blits, texts, rects;
static void lcd_sync(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{
    uint32_t row;
    assert(w && h && x + w <= 240u && y + h <= 240u && w * h <= 240u * 124u);
    for (row = 0; row < h; row++) memcpy(screen + (y + row) * 240u + x, p + row * w, w * sizeof *p);
    blits++;
}
#include "../firmware/src/gfx.c"
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t col, row;
    assert(x + w <= 240u && y + h <= 240u);
    for (row = 0; row < h; row++)
        for (col = 0; col < w; col++) screen[(y + row) * 240u + x + col] = swap16(c);
}
static int32_t checked_text(int32_t x, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    static const char *expected[] = {"JAMBOXFM", "0.1.0-dev", "TEMPORARY SESSION", "SLOOP + FELUCCA"};
    assert(texts < 4 && !strcmp(s, expected[texts++]));
    assert(x >= 0 && y >= 0 && x + text_w(f, s) <= (int32_t)cv_w && y + f->h <= (int32_t)cv_h);
    return cv_text(x, y, f, s, c);
}
static void checked_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    assert(x >= 0 && y >= 0 && w > 0 && h > 0 && x + w <= (int32_t)cv_w && y + h <= (int32_t)cv_h);
    rects++;
    cv_rect(x, y, w, h, c);
}
#define cv_text checked_text
#define cv_rect checked_rect
#include "../firmware/src/splash.c"
#undef cv_text
#undef cv_rect

int main(int argc, char **argv)
{
    uint32_t i, pixels = 0;
    sloop_splash();
    assert(blits == 2 && texts == 4 && rects == 8);
    for (i = 0; i < 240u * 240u; i++) pixels += screen[i] != C_BLACK;
    assert(pixels > 5000u && pixels < 12000u);
    if (argc > 1) {
        FILE *f = fopen(argv[1], "wb");
        assert(f && fprintf(f, "P6\n240 240\n255\n") > 0);
        for (i = 0; i < 240u * 240u; i++) {
            uint16_t p = swap16(screen[i]);
            uint8_t rgb[3] = {(p >> 11) * 255u / 31u, ((p >> 5) & 63u) * 255u / 63u, (p & 31u) * 255u / 31u};
            assert(fwrite(rgb, 1, 3, f) == 3);
        }
        assert(!fclose(f));
    }
    puts("JamBox splash: real renderer, four labels, original vector bars and canvas/display bounds passed");
    return 0;
}
