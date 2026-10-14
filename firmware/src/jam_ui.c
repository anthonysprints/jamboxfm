/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 JamBoxFM contributors */
/* Prototype Home, included at the end of ui.c with JAMBOX_PROTO enabled.
 * Uses the existing small canvas and physical encoders. Recording, held-button
 * layers, menus and deep pages retain their upstream handlers. No Assist toggle
 * is shown until its input/recording contract has been implemented. */

static struct {
    uint32_t head, roles, sound, macro, foot;
    uint8_t valid;
} jam_home_cache;

static uint32_t jam_home_hash(uint32_t h, const char *s)
{
    while (*s)
        h = (h ^ (uint8_t)*s++) * 16777619u;
    return h;
}

static uint32_t jam_home_role(uint32_t track)
{
    uint32_t r;
    for (r = 0; r < 4u; r++)
        if (jam_role_track(r) == track)
            return r;
    return 0;
}

static int jam_home_active(void)
{
    return ui.home && !ui.menu && ui.layer == LY_PLAY && !ui.hold_kind &&
           !rec_wait && !ft_on && !ci_on;
}

static int jam_home_macros_available(uint32_t track)
{
    return track == TRK_DRUM || (track < NPART && trk[track].eng_req == 0);
}

static void jam_home_browse(int32_t steps)
{
    uint32_t count = jam_sound_count(song.sel), index;
    if (!steps || !count)
        return;
    index = (uint32_t)(((int32_t)(jam_sound_index(song.sel) % count) +
             steps % (int32_t)count + (int32_t)count) % (int32_t)count);
    if (!jam_sound_load(song.sel, index)) {
        ui_message("SOUND UNAVAILABLE");
        return;
    }
    sync_reload = 1;
    ui.hot_t = 0;
    ui.force = 1;
}

/* Called after ordinary button handling, before the generic encoder paths.
 * Returns 0 without reading an encoder when another screen owns the controls. */
static int jam_home_input(void)
{
    uint32_t k;
    int32_t s;
    if (!jam_home_active())
        return 0;
    if ((s = panel_enc(EN_SELECT)) != 0) {
        int32_t r = (int32_t)jam_home_role(song.sel);
        /* Clamp the increment before adding it: a noisy encoder cannot wrap. */
        r = clamp(r + clamp(s, -4, 4), 0, 3);
        track_select(jam_role_track((uint32_t)r));
    }
    jam_home_browse(panel_enc(EN_PRESET));
    /* Bottom-right left encoder: one BPM per detent on Home. */
    if ((s = panel_enc(EN_ALGO)) != 0) {
        song.g[G_BPM] = (int16_t)clamp(song.g[G_BPM] + clamp(s, -200, 200),
                                      GP[G_BPM].min, GP[G_BPM].max);
        ui.bpm_t = 40;
    }
    for (k = 0; k < 4u; k++) {
        if ((s = panel_enc(EN_K1 + k)) == 0)
            continue;
        if (!jam_home_macros_available(song.sel))
            continue;
        /* Five percent per detent also moves the narrow 12/16-value ranges;
         * adding 1 to their rounded getter could otherwise remain stuck. */
        jam_macro_set(song.sel, k, (uint32_t)clamp(
            (int32_t)jam_macro_get(song.sel, k) + clamp(s, -20, 20) * 5, 0, 100));
        ui.hot_col = (uint8_t)k;
        ui.hot_t = 40;
    }
    return 1;
}

/* Copy only text that fits its rectangle; cv_text itself also clips pixels. */
static void jam_home_text(int32_t x, int32_t y, int32_t width,
                          const felucca_font_t *font, const char *s, uint16_t color)
{
    char b[32];
    str_cpy(b, s, sizeof b);
    while (b[0] && text_w(font, b) > width)
        b[str_len(b) - 1u] = 0;
    cv_text(x, y, font, b, color);
}

/* Five short strips; no full-screen RAM buffer or unconditional 60 Hz redraw.
 * The caller runs ui_timers() and clears ui.force after this returns 1. */
static int jam_home_draw(void)
{
    uint32_t k, r, head, roles, sound, macro, foot;
    uint32_t selected = song.sel % NTRK;
    int force;
    char b[20];
    if (!jam_home_active())
        return 0;
    force = ui.force || !jam_home_cache.valid;
    head = (uint32_t)song.g[G_BPM] * 131u + song.playing * 17u + song.rec * 29u +
           (song.playing ? (clk_beat & 3u) + 1u : 0u);
    roles = selected;
    for (k = 0; k < NTRK; k++)
        roles = roles * 31u + (uint32_t)!!trk[k].p[P_MUTE] + ((song.rec >> k) & 1u) * 3u;
    sound = jam_home_hash(selected + 1u, jam_sound_name(selected));
    macro = (selected + 1u) * 2u + (uint32_t)jam_home_macros_available(selected);
    for (k = 0; k < 4u; k++) {
        macro = jam_home_hash(macro, jam_macro_name(selected, k));
        macro = macro * 131u + jam_macro_get(selected, k);
    }
    macro = macro * 7u + (ui.hot_t ? (uint32_t)ui.hot_col + 1u : 0u);
    /* The second line changes with engine support even while a message owns
     * the first line, so availability is part of both footer cache keys. */
    foot = (uint32_t)!jam_home_macros_available(selected);
    if (ui.msg_t)
        foot = jam_home_hash(foot + 1u, ui.msg);
    if (force) {
        lcd_fill(0, 0, 240, 240, C_BLACK);
        jam_home_cache.valid = 1;
    }
    if (force || head != jam_home_cache.head) {
        const char *state = song.playing ? (song.rec ? "REC" : "PLAY") : (song.rec ? "ARM" : "STOP");
        jam_home_cache.head = head;
        cv_begin(240, 36, C_BLACK);
        cv_text(8, 1, &FONT_S, "JAMBOXFM", C_WHITE);
        fmt_int(b, song.g[G_BPM]);
        str_cpy(b + str_len(b), " BPM", sizeof b - str_len(b));
        cv_text(232 - text_w(&FONT_S, b), 1, &FONT_S, b, C_HI);
        cv_text(8, 19, &FONT_S, state, song.rec ? RGB(255, 90, 70) : C_HI);
        for (k = 0; k < 4u; k++)
            cv_rect(190 + (int32_t)k * 11, 23, 7, 7,
                    song.playing && (clk_beat & 3u) == k ? C_HI : C_LINE);
        cv_blit(0, 0);
    }
    if (force || roles != jam_home_cache.roles) {
        jam_home_cache.roles = roles;
        cv_begin(240, 32, C_BLACK);
        for (r = 0; r < 4u; r++) {
            uint32_t t = jam_role_track(r);
            uint16_t bg = t == selected ? C_HI : C_LINE;
            uint16_t fg = t == selected ? C_BLACK : C_WHITE;
            int32_t x = (int32_t)r * 60;
            const char *label = jam_role_name(t);
            cv_rect(x + 2, 0, 56, 32, bg);
            jam_home_text(x + 4, 0, 52, &FONT_S, label, fg);
            if (trk[t].p[P_MUTE])
                cv_text(x + 4, 16, &FONT_S, "mute", fg);
            else if ((song.rec >> t) & 1u)
                cv_text(x + 4, 16, &FONT_S, "rec", fg);
            else if (t == selected)
                cv_rect(x + 5, 25, 46, 2, fg);
        }
        cv_blit(0, 40);
    }
    if (force || sound != jam_home_cache.sound) {
        jam_home_cache.sound = sound;
        cv_begin(240, 44, C_BLACK);
        jam_home_text(8, 6, 224, &FONT_L, jam_sound_name(selected), C_WHITE);
        cv_blit(0, 76);
    }
    if (force || macro != jam_home_cache.macro) {
        jam_home_cache.macro = macro;
        cv_begin(240, 80, C_BLACK);
        for (k = 0; k < 4u; k++) {
            uint32_t v = jam_macro_get(selected, k);
            int available = jam_home_macros_available(selected);
            int32_t x = (int32_t)k * 60;
            uint16_t fg = ui.hot_t && ui.hot_col == k ? C_WHITE : C_HI;
            jam_home_text(x + 4, 3, 52, &FONT_S, available ? jam_macro_name(selected, k) : "EDIT", C_GRAY);
            if (available)
                fmt_int(b, (int32_t)v);
            else
                str_cpy(b, "--", sizeof b);
            cv_text(x + 30 - text_w(&FONT_L, b) / 2, 24, &FONT_L, b, fg);
            b[0] = 'K'; b[1] = (char)('1' + k); b[2] = 0;
            cv_text(x + 4, 59, &FONT_S, b, C_GRAY);
            cv_rect(x + 4, 76, 52, 3, C_LINE);
            if (v)
                cv_rect(x + 4, 76, (int32_t)(v * 52u / 100u), 3, fg);
        }
        cv_blit(0, 124);
    }
    if (force || foot != jam_home_cache.foot) {
        jam_home_cache.foot = foot;
        cv_begin(240, 32, C_BLACK);
        jam_home_text(8, 0, 224, &FONT_S,
                      ui.msg_t ? ui.msg : "SELECT part  PRESET sound", ui.msg_t ? C_HI : C_GRAY);
        jam_home_text(8, 16, 224, &FONT_S,
                      jam_home_macros_available(selected) ? "ALGO tempo / SEL scale" : "ALGO tempo / EDIT sound", C_GRAY);
        cv_blit(0, 208);
    }
    return 1;
}
