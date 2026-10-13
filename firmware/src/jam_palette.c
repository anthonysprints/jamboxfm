/* SPDX-License-Identifier: GPL-3.0-only */
/* JamBoxFM's first palette. Include after seq.c in both device and host builds.
 * UI/main-loop entry points use the same short IRQ exclusion as upstream sound
 * loading; call with IRQs enabled (host adapters supply no-op IRQ functions).
 * No flash writes, allocation, or automatic transport start. */

typedef struct {
    const char *name;
    int8_t e[8];
    uint8_t env[4];
    int8_t fenv, transpose;
    uint8_t lrate, movement, grit, space;
} jam_patch_t;

static const jam_patch_t jam_patches[NPART][2] = {
    {
        {"ROUND BASS", {2, 0, 0, 0, 78, 8, 8, 20}, {2, 66, 88, 20}, 12, -24, 28, 0, 10, 0},
        {"MUTED BASS", {0, 0, 0, 0, 52, 14, 16, 38}, {1, 54, 52, 12}, 20, -24, 28, 8, 20, 0},
    },
    {
        {"DUST KEYS", {2, 5, 32, 0, 68, 8, 4, 32}, {2, 74, 28, 42}, 18, 0, 32, 15, 8, 28},
        {"SOFT STABS", {4, 5, 24, 0, 54, 6, 4, 32}, {0, 58, 36, 26}, 20, 0, 32, 8, 12, 18},
    },
    {
        {"MELLOW LEAD", {2, 0, 0, 0, 88, 12, 6, 48}, {4, 66, 104, 28}, 8, 0, 36, 12, 8, 32},
        {"DUST REED", {4, 0, 0, 0, 72, 10, 6, 48}, {6, 74, 100, 24}, 6, 0, 36, 18, 10, 24},
    },
};
static const char *const jam_drum_names[2] = {"DUST KIT", "SOFT KIT"};
static const char *const jam_drum_sources[2] = {"BOOMBAP", "LO-FI"};
static uint8_t jam_sound_id[NTRK];   /* curated browser position; not yet a persisted format field */
static uint8_t jam_sound_valid;
static int32_t jam_kit_index(uint32_t index);

/* Generic preset/project loaders invalidate this session-only label. A deep
 * parameter edit keeps the label: it is still an edited version of that sound. */
void jam_sound_invalidate(uint32_t track)
{
    if (track < NTRK) jam_sound_valid &= (uint8_t)~(1u << track);
}

uint32_t jam_role_track(uint32_t role)
{
    static const uint8_t map[NTRK] = {TRK_DRUM, 0, 1, 2};
    return map[role % NTRK];
}
const char *jam_role_name(uint32_t track)
{
    static const char *const names[NTRK] = {"BASS", "CHORDS", "LEAD", "DRUMS"};
    return track < NTRK ? names[track] : "";
}
uint32_t jam_sound_count(uint32_t track) { return track < NTRK ? 2u : 0u; }
uint32_t jam_sound_index(uint32_t track)
{
    if (track == TRK_DRUM) return TDRUM->p[P_E0] == jam_kit_index(1) ? 1u : 0u;
    return track < NPART && trk[track].eng_req == 0 && ((jam_sound_valid >> track) & 1u) ? jam_sound_id[track] : 0u;
}
const char *jam_sound_name(uint32_t track)
{
    if (track >= NTRK) return "";
    if (track == TRK_DRUM) {
        for (uint32_t i = 0; i < 2; i++)
            if (TDRUM->p[P_E0] == jam_kit_index(i)) return jam_drum_names[i];
        return DRUM_KIT_NAMES[(uint32_t)TDRUM->p[P_E0] % DRUM_KITS];
    }
    if (trk[track].eng_req != 0 || !((jam_sound_valid >> track) & 1u))
        return ENGINES[trk[track].eng_req % NENGINES]->name;
    return jam_patches[track][jam_sound_id[track] & 1u].name;
}
const char *jam_macro_name(uint32_t track, uint32_t macro)
{
    static const char *const melodic[4] = {"BRIGHT", "MOVE", "GRIT", "SPACE"};
    static const char *const drum[4] = {"TONE", "LEVEL", "ECHO", "ROOM"};
    if (track >= NTRK || macro >= 4u) return "";
    return (track == TRK_DRUM ? drum : melodic)[macro];
}

static int32_t jam_scale(int32_t percent, int32_t lo, int32_t hi)
{
    return lo + (clamp(percent, 0, 100) * (hi - lo) + 50) / 100;
}
static uint32_t jam_percent(int32_t value, int32_t lo, int32_t hi)
{
    return (uint32_t)clamp(((value - lo) * 100 + (hi - lo) / 2) / (hi - lo), 0, 100);
}
/* An edit under an active step lock changes the restored base too. */
static void jam_param(track_t *t, uint32_t id, int32_t value)
{
    uint32_t i;
    t->p[id] = (int16_t)value;
    for (i = 0; i < t->lk_n; i++)
        if (t->lk_param[i] == id) t->lk_base[i] = t->p[id];
}
static const uint8_t jam_bright_lo[NPART] = {40, 36, 48};
static const uint8_t jam_bright_hi[NPART] = {104, 104, 116};
static const uint8_t jam_move_hi[NPART] = {16, 24, 20};
static const uint8_t jam_grit_hi[NPART] = {20, 36, 28};
static const uint8_t jam_echo_hi[NPART] = {12, 32, 44};
static const uint8_t jam_room_hi[NPART] = {8, 28, 30};

static void jam_macro_write(uint32_t track, uint32_t macro, int32_t value)
{
    track_t *t = &trk[track];
    if (track == TRK_DRUM) {
        switch (macro) {
        case 0: jam_param(t, P_TFLT, jam_scale(value, -32, 0)); break;
        case 1: song.g[G_DRLVL] = (int16_t)jam_scale(value, 0, 110); break;
        case 2: song.g[G_DRDLY] = (int16_t)jam_scale(value, 0, 24); break;
        case 3: song.g[G_DRREV] = (int16_t)jam_scale(value, 0, 24); break;
        }
    } else {
        switch (macro) {
        case 0: jam_param(t, P_E4, jam_scale(value, jam_bright_lo[track], jam_bright_hi[track])); break;
        case 1: jam_param(t, P_LD_FLT, jam_scale(value, 0, jam_move_hi[track])); break;
        case 2: jam_param(t, P_DIST, jam_scale(value, 0, jam_grit_hi[track])); break;
        case 3:
            jam_param(t, P_DLY, jam_scale(value, 0, jam_echo_hi[track]));
            jam_param(t, P_REV, jam_scale(value, 0, jam_room_hi[track]));
            break;
        }
    }
}
void jam_macro_set(uint32_t track, uint32_t macro, int32_t percent)
{
    if (track >= NTRK || macro >= 4u) return;
    /* These first melodic macros describe ANALOG, not arbitrary engine IDs. */
    if (track < NPART && trk[track].eng_req != 0) return;
    fm1_irq_off();
    jam_macro_write(track, macro, percent);
    fm1_irq_on();
}
uint32_t jam_macro_get(uint32_t track, uint32_t macro)
{
    const track_t *t;
    if (track >= NTRK || macro >= 4u) return 0;
    t = &trk[track];
    if (track == TRK_DRUM) {
        switch (macro) {
        case 0: return jam_percent(t->p[P_TFLT], -32, 0);
        case 1: return jam_percent(song.g[G_DRLVL], 0, 110);
        case 2: return jam_percent(song.g[G_DRDLY], 0, 24);
        default: return jam_percent(song.g[G_DRREV], 0, 24);
        }
    }
    if (t->eng_req != 0) return 0;
    switch (macro) {
    case 0: return jam_percent(t->p[P_E4], jam_bright_lo[track], jam_bright_hi[track]);
    case 1: return jam_percent(t->p[P_LD_FLT], 0, jam_move_hi[track]);
    case 2: return jam_percent(t->p[P_DIST], 0, jam_grit_hi[track]);
    default: return jam_percent(t->p[P_DLY], 0, jam_echo_hi[track]);
    }
}

/* Same preservation boundary as Sloop param_kept(): never touch the pattern,
 * mix, root/scale/Assist choices, track filter, strum or voice leading. */
static int jam_sound_keeps(uint32_t id)
{
    return id == P_LEVEL || id == P_PAN || id == P_MUTE ||
        (id >= P_SLEN && id <= P_SGATE) || (id >= P_ROOT && id <= P_QUANT) ||
        id == P_CHORD || id == P_TFLT || id == P_STRUM || id == P_VLEAD;
}
static int32_t jam_kit_index(uint32_t index)
{
    uint32_t k;
    for (k = DRUM_SAMPLED; k < DRUM_USR; k++)
        if (str_eq(DRUM_KIT_NAMES[k], jam_drum_sources[index])) return (int32_t)k;
    return -1;                         /* do not silently substitute a sampled kit */
}
static void jam_sound_write(uint32_t track, uint32_t index)
{
    track_t *t = &trk[track];
    uint32_t i;
    locks_restore(t);
    jam_sound_id[track] = (uint8_t)index;
    if (track == TRK_DRUM) {
        t->p[P_E0] = (int16_t)jam_kit_index(index);
        return;
    }
    {
        const jam_patch_t *p = &jam_patches[track][index];
        sound_change(t);
        panic_req |= (uint8_t)(1u << track);
        t->eng_req = 0;
        t->preset = 0;                 /* custom name comes from this module, not ANALOG_PRESETS[0] */
        t->user = 0;
        for (i = 0; i < P_E0; i++)
            if (!jam_sound_keeps(i)) t->p[i] = TP[i].def;
        for (i = 0; i < 8u; i++) t->p[P_E0 + i] = p->e[i];
        for (i = 0; i < 4u; i++) t->p[P_ATK + i] = p->env[i];
        t->p[P_ED_FLT] = p->fenv;
        t->p[P_TRANS] = p->transpose;
        t->p[P_VOICE] = track == 1u ? V_POLY : V_LEGATO;
        t->p[P_LRATE] = p->lrate;
        t->p[P_LTRIG] = 1;              /* free-running, gentle sine movement */
        t->p[P_CHOR] = track == 1u ? 12 : 0;
        jam_macro_write(track, 1, p->movement);
        jam_macro_write(track, 2, p->grit);
        jam_macro_write(track, 3, p->space);
    }
    jam_sound_valid |= (uint8_t)(1u << track);
}
int jam_sound_load(uint32_t track, uint32_t index)
{
    if (track >= NTRK || index >= 2u) return 0;
    if (track == TRK_DRUM && jam_kit_index(index) < 0) return 0;
    fm1_irq_off();
    jam_sound_write(track, index);
    fm1_irq_on();
    return 1;
}

/* End inputs owned by the previous session before replacing its notes. Keep
 * MIDI routing, controller calibration and the master volume as device settings.
 * Held physical keys must be lifted before they can enter the new session. */
static void jam_inputs_reset(void)
{
    uint32_t k;
    for (k = 0; k < NELEM(kb_kind); k++) {
        if (kb_kind[k] == KS_NOTE || kb_kind[k] == KS_DRUM || kb_kind[k] == KS_ROLL || kb_kind[k] == KS_FX)
            key_up(k);
        kb_kind[k] = KS_NONE;
        kb_n[k] = 0;
    }
    for (k = 0; k < NROLL; k++) roll_end(k);
    kb_prev = fm1_in.notes;
    ly_lock = LY_PLAY;
    kb_grid = 0;
    lk_r = lk_w;
    mi_r = mi_w;                         /* discard old queued notes, CCs and transport requests */
    step_in_r = step_in_w;
    step_in_lost = 0;
    memset((void *)pc_req, 0, sizeof pc_req);
    memset(mi_pen, 0, sizeof mi_pen);
    memset(midi_sel_on, 0, sizeof midi_sel_on);
    memset(ch_sus, 0, sizeof ch_sus);
    memset(ch_bend, 0, sizeof ch_bend);
    memset(sus_on, 0, sizeof sus_on);
    memset(sus_wait, 0, sizeof sus_wait);
    memset(vl_n, 0, sizeof vl_n);
    memset(er_notes, 0, sizeof er_notes);
    drum_mute = er_lanes = er_mute = er_flash = 0;
    aud_lanes = aud_lvl = dsu_aud_vel = 0;
    punch.req = -1; punch.hold = 0; punch.keybit = 0; /* existing short wet fade ends the effect */
    last_note = pen_note[0] = 60;
    pen_n = 1; pen_lane = 0;
    for (k = 0; k < NTRK; k++) {
        trk[k].nheld = trk[k].arp_phys = trk[k].arp_note = 0;
        trk[k].bend = 0;
    }
}

int jam_new_dusty(void)
{
    uint32_t k, i;
    static const uint8_t level[NPART] = {94, 86, 92};
    static const uint8_t kicks[] = {0, 7, 10, 16, 22, 26};
    static const uint8_t snares[] = {4, 12, 20, 28};
    if (jam_kit_index(0) < 0) return 0;
    fm1_irq_off();
    if (song.playing || transport_req == 1u || transport_req == 3u || ft_on || ci_on) {
        fm1_irq_on();
        return 0;
    }
    seq_stop();
    jam_inputs_reset();
    transport_req = 0;
    rec_wait = 0;
    song.rec = song.solo = song.seq_mode = 0;
    song.octave = 0;
    for (i = G_BPM; i <= G_CDEPTH; i++) song.g[i] = GP[i].def;
    for (i = G_DUST; i <= G_ROLL; i++) song.g[i] = GP[i].def;
    for (i = G_TXSRC; i <= G_TXDUCK; i++) song.g[i] = GP[i].def;
#if FELUCCA_ARRANGER
    arrangement_enabled = 0;
#endif
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        trk_all_off(t);
        for (i = 0; i < P_COUNT; i++) t->p[i] = TP[i].def;
        steps_clear(t);
        t->p[P_SLEN] = 32;
        t->p[P_SDIV] = 2;             /* sixteenths: two bars in 4/4 */
        t->p[P_ROOT] = 0;
        t->p[P_SCALE] = 2;            /* C minor; upstream helpers for this first slice */
        if (k == 1u) t->p[P_CHORD] = 2;
        if (k == 1u || k == 2u) t->p[P_QUANT] = 2;
        if (k < NPART) t->p[P_LEVEL] = level[k];
        jam_sound_write(k, 0);
    }
    song.g[G_BPM] = 88;
    song.g[G_SWING] = 0;              /* independent control; user turns it up */
    song.g[G_CLOCK] = 1;              /* click only while recording */
    song.g[G_DTIME] = 1;
    song.g[G_DFDBK] = 36;
    song.g[G_DCOLOR] = 52;
    song.g[G_DMIX] = 72;
    song.g[G_RSIZE] = 48;
    song.g[G_RDAMP] = 84;
    song.g[G_CRATE] = 30;
    song.g[G_CDEPTH] = 28;
    song.g[G_DRLVL] = 88;
    song.g[G_DRREV] = 8;
    song.g[G_DRDLY] = 0;
    song.g[G_DUST] = song.g[G_DUCK] = song.g[G_FILT] = 0;
    song.g[G_TXSRC] = 0;
    for (i = 0; i < NELEM(kicks); i++)
        dstep_set(&TDRUM->dstep[kicks[i]], 0, i == 1u || i == 4u ? LV_SOFT : LV_NORM, 0);
    for (i = 0; i < NELEM(snares); i++) dstep_set(&TDRUM->dstep[snares[i]], 2, LV_NORM, 0);
    for (i = 0; i < 32u; i += 2u) dstep_set(&TDRUM->dstep[i], 4, i & 2u ? LV_SOFT : LV_NORM, 0);
    dstep_set(&TDRUM->dstep[15], 5, LV_SOFT, 0);
    dstep_set(&TDRUM->dstep[31], 5, LV_SOFT, 0);
    dstep_set(&TDRUM->dstep[31], 2, LV_GHOST, 0);
    song.sel = 1;                    /* PLAY starts the beat; the keyboard is ready for chords */
    panic_req = (1u << NTRK) - 1u;
    fm1_irq_on();
    return 1;
}
