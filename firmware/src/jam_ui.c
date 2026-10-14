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

static struct {
    uint32_t head, row[NTRK], sound, macro, foot;
    uint32_t generation[NTRK];
    uint8_t bank[NTRK], valid;
} jam_groove_cache;
static uint8_t jam_home_drawn_view = 0xFF;

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
    return jam_macro_supported(track);
}

/* GROOVE uses the existing sequencer/mixer parameters; SOUND edits the patch. */
static const char *const JAM_GROOVE_LABELS[4] = {"SWING", "LEVEL", "STEPS", "PAN"};
static int16_t *jam_groove_parameter(uint32_t track, uint32_t slot)
{
    track_t *t = &trk[track % NTRK];
    switch (slot) {
    case 0: return &song.g[G_SWING];
    case 1: return is_drum(t) ? &song.g[G_DRLVL] : &t->p[P_LEVEL];
    case 2: return &t->p[P_SLEN];
    default: return &t->p[P_PAN];
    }
}
static int32_t jam_groove_value(uint32_t track, uint32_t slot)
{
    return *jam_groove_parameter(track, slot);
}
static uint32_t jam_groove_percent(uint32_t track, uint32_t slot)
{
    int32_t v = jam_groove_value(track, slot);
    return (uint32_t)(slot == 0 ? v : slot == 1 ? v * 100 / 127 :
                      slot == 2 ? (v - 1) * 100 / 63 : (v + 64) * 100 / 127);
}
static void jam_groove_text(char *b, uint32_t track, uint32_t slot)
{
    int32_t v = jam_groove_value(track, slot);
    if (slot != 3u) { fmt_int(b, slot == 1u ? v * 100 / 127 : v); return; }
    if (!v) { str_cpy(b, "CENTER", 24); return; }
    b[0] = v < 0 ? 'L' : 'R';
    fmt_int(b + 1, v < 0 ? -v * 100 / 64 : v * 100 / 63);
}
static void jam_groove_edit(uint32_t slot, int32_t steps)
{
    int16_t *v = jam_groove_parameter(song.sel, slot);
    int32_t lo = slot == 2u ? 1 : slot == 3u ? -64 : 0;
    int32_t hi = slot == 0u ? 100 : slot == 1u ? 127 : slot == 2u ? NSTEP : 63;
    /* One unit per detent, no acceleration. Length only changes the playback
     * window, and level never clears an explicit mute. Hidden notes stay put. */
    *v = (int16_t)clamp(*v + clamp(steps, -127, 127), lo, hi);
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

static void jam_home_preset_position(char *b, uint32_t track)
{
    if (jam_sound_is_curated(track))
        fmt_int(b, (int32_t)jam_sound_index(track) + 1);
    else
        str_cpy(b, "--", 3);
    str_cpy(b + str_len(b), "/", 2);
    fmt_int(b + str_len(b), (int32_t)jam_sound_count(track));
}

/* Called after ordinary button handling, before the generic encoder paths.
 * Returns 0 without reading an encoder when another screen owns the controls. */
static int jam_home_input(void)
{
    uint32_t k;
    int32_t s;
    if (!jam_home_active())
        return 0;
    if ((s = panel_enc(EN_ALGO)) != 0) {
        int32_t r = (int32_t)jam_home_role(song.sel);
        /* Clamp the increment before adding it: a noisy encoder cannot wrap. */
        r = clamp(r + clamp(s, -4, 4), 0, 3);
        track_select(jam_role_track((uint32_t)r));
    }
    jam_home_browse(panel_enc(EN_PRESET));
    /* Top-right left encoder (SELECT): one BPM per detent on Home. */
    if ((s = panel_enc(EN_SELECT)) != 0) {
        song.g[G_BPM] = (int16_t)clamp(song.g[G_BPM] + clamp(s, -200, 200),
                                      GP[G_BPM].min, GP[G_BPM].max);
        ui.bpm_t = 40;
    }
    for (k = 0; k < 4u; k++) {
        if ((s = panel_enc(EN_K1 + k)) == 0)
            continue;
        if (ui.jam_home_view == JAM_HOME_GROOVE) {
            jam_groove_edit(k, s);
            ui.hot_col = (uint8_t)k;
            ui.hot_t = 40;
            continue;
        }
        if (!jam_home_macros_available(song.sel))
            continue;
        /* Continuous macros use five percent; coarse engine choices such as
         * organ rotary speed need a whole option per detent. */
        jam_macro_set(song.sel, k, (uint32_t)clamp(
            (int32_t)jam_macro_get(song.sel, k) + clamp(s, -20, 20) *
                (int32_t)jam_macro_step(song.sel, k), 0, 100));
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
static int jam_sound_home_draw(void)
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
    roles = roles * 31u + (jam_sound_is_curated(selected) ? jam_sound_index(selected) + 1u : 0u);
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
        cv_text(8, 1, &FONT_S, "SOUND", C_WHITE);
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
            else if (t == selected) {
                jam_home_preset_position(b, selected);
                jam_home_text(x + 4, 16, 52, &FONT_S, b, fg);
            }
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
                      ui.msg_t ? ui.msg : "HOME: GROOVE  PRESET sound", ui.msg_t ? C_HI : C_GRAY);
        jam_home_text(8, 16, 224, &FONT_S,
                      jam_home_macros_available(selected) ? "SELECT tempo / SEL scale" : "SELECT tempo / EDIT sound", C_GRAY);
        cv_blit(0, 208);
    }
    return 1;
}

enum { JAM_CELL_DISABLED, JAM_CELL_EMPTY, JAM_CELL_NOTE, JAM_CELL_TIE };
/* Every Groove cell is one stored step. Ties are not new note attacks, and the
 * drum row combines all lanes without pretending to show a selected drum lane. */
static uint32_t jam_groove_cell(const track_t *t, uint32_t step, uint32_t len)
{
    if (step >= len || step >= NSTEP)
        return JAM_CELL_DISABLED;
    if (is_drum(t))
        return dstep_mask(&t->dstep[step]) ? JAM_CELL_NOTE : JAM_CELL_EMPTY;
    if (t->step[step].time == ST_TIE)
        return JAM_CELL_TIE;
    return step_on(&t->step[step]) ? JAM_CELL_NOTE : JAM_CELL_EMPTY;
}
static void jam_groove_range(char *b, uint32_t bank, uint32_t len)
{
    uint32_t a = bank * 16u + 1u, z = (bank + 1u) * 16u;
    if (z > len) z = len;
    b[0] = (char)('0' + a / 10u); b[1] = (char)('0' + a % 10u);
    b[2] = '-';
    b[3] = (char)('0' + z / 10u); b[4] = (char)('0' + z % 10u);
    b[5] = '/';
    b[6] = (char)('0' + len / 10u); b[7] = (char)('0' + len % 10u); b[8] = 0;
}

static int jam_groove_home_draw(void)
{
    static const uint16_t colors[NTRK] = {
        RGB(40, 124, 255), RGB(30, 204, 112), RGB(255, 198, 24), RGB(255, 98, 26)
    };
    uint32_t selected = song.sel % NTRK, playing = !!song.playing;
    uint32_t head, sound, macro, foot, r, k;
    int force = ui.force || !jam_groove_cache.valid;
    char b[24];
    if (force) {
        lcd_fill(0, 0, 240, 240, C_BLACK);
        jam_groove_cache.valid = 1;
    }
    head = (uint32_t)song.g[G_BPM] * 131u + playing * 17u + song.rec * 29u +
           (playing ? (clk_beat & 3u) + 1u : 0u);
    if (force || head != jam_groove_cache.head) {
        const char *state = playing ? (song.rec ? "REC" : "PLAY") : (song.rec ? "ARM" : "STOP");
        jam_groove_cache.head = head;
        cv_begin(240, 32, C_BLACK);
        cv_text(8, 0, &FONT_S, "GROOVE", C_WHITE);
        fmt_int(b, song.g[G_BPM]);
        str_cpy(b + str_len(b), " BPM", sizeof b - str_len(b));
        cv_text(232 - text_w(&FONT_S, b), 0, &FONT_S, b, C_HI);
        cv_text(8, 16, &FONT_S, state, song.rec ? RGB(255, 90, 70) : C_HI);
        for (k = 0; k < 4u; k++)
            cv_rect(190 + (int32_t)k * 11, 22, 7, 7,
                    playing && (clk_beat & 3u) == k ? C_HI : C_LINE);
        cv_blit(0, 0);
    }
    for (r = 0; r < NTRK; r++) {
        uint32_t tno = jam_role_track(r), j, sig, bank;
        const track_t *t = &trk[tno];
        uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP);
        uint32_t pos = t->seq_idx % len, div = (uint32_t)t->p[P_SDIV] % NDIV_STEP;
        uint32_t mute = !!t->p[P_MUTE], solo = (song.solo >> tno) & 1u;
        uint32_t rec = (song.rec >> tno) & 1u, chosen = selected == tno;
        uint32_t level = is_drum(t) ? song.g[G_DRLVL] : t->p[P_LEVEL];
        uint32_t silent = trk_silent(t) || !level;
        uint32_t generation = jam_pattern_generation[tno];
        uint8_t cells[16];
        uint16_t col = colors[tno], ink = silent ? C_DIM : chosen ? col : C_GRAY;
        /* A sound change does not replace a pattern. Only the shared pattern
         * generation resets this independent display window; editing cursors
         * are never read or moved. A shorter pattern clamps its last window. */
        if (generation != jam_groove_cache.generation[tno]) {
            jam_groove_cache.generation[tno] = generation;
            jam_groove_cache.bank[tno] = 0;
        }
        bank = playing ? pos / 16u : jam_groove_cache.bank[tno];
        if (bank > (len - 1u) / 16u) bank = (len - 1u) / 16u;
        jam_groove_cache.bank[tno] = (uint8_t)bank;
        sig = len + bank * 67u + div * 277u;
        sig = sig * 31u + chosen + mute * 2u + solo * 4u + rec * 8u + silent * 16u;
        sig = sig * 71u + (playing ? pos + 1u : 0u);
        for (j = 0; j < 16u; j++) {
            cells[j] = (uint8_t)jam_groove_cell(t, bank * 16u + j, len);
            sig = sig * 31u + cells[j];
        }
        if (!force && sig == jam_groove_cache.row[tno])
            continue;
        jam_groove_cache.row[tno] = sig;
        cv_begin(240, 28, C_BLACK);
        if (chosen) cv_rect(2, 1, 3, 26, col);
        jam_home_text(8, 0, 48, &FONT_S, jam_role_name(tno), chosen ? C_WHITE : C_GRAY);
        jam_groove_range(b, bank, len);
        cv_text(64, 0, &FONT_S, b, chosen ? C_WHITE : C_GRAY);
        if (div != 2u)
            jam_home_text(136, 0, 32, &FONT_S, N_SDIV[div], C_GRAY);
        /* Independent marks: recording a muted or soloed part shows all states. */
        if (mute) cv_text(176, 0, &FONT_S, "M", C_WHITE);
        if (solo) cv_text(192, 0, &FONT_S, "S", C_HI);
        if (rec) cv_text(208, 0, &FONT_S, "R", RGB(255, 90, 70));
        for (j = 0; j < 16u; j++) {
            int32_t x = 8 + (int32_t)j * 14;
            if (!cells[j])
                continue;
            cv_rect(x, 17, 12, 7, RGB(26, 26, 30));
            if (!(j & 3u)) cv_rect(x, 16, 2, 1, C_GRAY);
            if (cells[j] == JAM_CELL_NOTE)
                cv_rect(x, 17, 12, 7, ink);
            else if (cells[j] == JAM_CELL_TIE)
                cv_rect(x, 20, 12, 2, ink);
            if (playing && bank * 16u + j == pos)
                cv_rect(x, 25, 12, 2, C_WHITE);
        }
        cv_blit(0, 32u + r * 28u);
    }
    sound = jam_home_hash(selected + 1u, jam_sound_name(selected));
    if (force || sound != jam_groove_cache.sound) {
        jam_groove_cache.sound = sound;
        cv_begin(240, 32, C_BLACK);
        jam_home_text(8, 0, 160, &FONT_S, jam_role_name(selected), colors[selected]);
        jam_home_preset_position(b, selected);
        cv_text(232 - text_w(&FONT_S, b), 0, &FONT_S, b, C_GRAY);
        jam_home_text(8, 16, 224, &FONT_S, jam_sound_name(selected), C_WHITE);
        cv_blit(0, 144);
    }
    macro = selected + 1u;
    for (k = 0; k < 4u; k++)
        macro = macro * 131u + (uint32_t)(jam_groove_value(selected, k) + 64);
    macro = macro * 7u + (ui.hot_t ? (uint32_t)ui.hot_col + 1u : 0u);
    if (force || macro != jam_groove_cache.macro) {
        jam_groove_cache.macro = macro;
        cv_begin(240, 40, C_BLACK);
        for (k = 0; k < 4u; k++) {
            uint32_t v = jam_groove_percent(selected, k);
            int32_t x = (int32_t)k * 60;
            uint16_t fg = ui.hot_t && ui.hot_col == k ? C_WHITE : C_HI;
            jam_home_text(x + 4, 0, 52, &FONT_S, JAM_GROOVE_LABELS[k], C_GRAY);
            jam_groove_text(b, selected, k);
            cv_text(x + 30 - text_w(&FONT_S, b) / 2, 16, &FONT_S, b, fg);
            cv_rect(x + 4, 36, 52, 3, C_LINE);
            if (v) cv_rect(x + 4, 36, (int32_t)(v * 52u / 100u), 3, fg);
        }
        cv_blit(0, 176);
    }
    foot = 0;
    if (ui.msg_t) foot = jam_home_hash(foot + 1u, ui.msg);
    if (force || foot != jam_groove_cache.foot) {
        jam_groove_cache.foot = foot;
        cv_begin(240, 24, C_BLACK);
        jam_home_text(8, 4, 224, &FONT_S, ui.msg_t ? ui.msg : "HOME: SOUND  HOLD SEQ: EDIT",
                      ui.msg_t ? C_HI : C_GRAY);
        cv_blit(0, 216);
    }
    return 1;
}

static int jam_home_draw(void)
{
    uint32_t view;
    if (!jam_home_active())
        return 0;
    view = ui.jam_home_view == JAM_HOME_SOUND ? JAM_HOME_SOUND : JAM_HOME_GROOVE;
    if (view != jam_home_drawn_view) {
        jam_home_cache.valid = jam_groove_cache.valid = 0;
        jam_home_drawn_view = (uint8_t)view;
    }
    return view == JAM_HOME_SOUND ? jam_sound_home_draw() : jam_groove_home_draw();
}
