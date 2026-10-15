/* SPDX-License-Identifier: GPL-3.0-only */
/* Panel-only editing of the existing saved SYN recipes; no new storage format.
 * drum_page 2: sound, 3: explicit factory-copy confirmation. */
static uint8_t drum_copy_slot;
static uint32_t drum_sound_lane(void)
{
    int32_t semi;
    return ds_lane(LANE_NOTE[drum_lane], &semi);
}
static const dsnd_t *drum_sound_recipe(void)
{
    uint32_t kit = drum_kit();
    if (kit >= DRUM_SYN) return &dsu_kit(kit - DRUM_SYN)->s[drum_sound_lane()];
    if (kit >= DRUM_SAMPLED && kit < DRUM_USR)
        return &DS_KITS[kit - DRUM_SAMPLED].s[drum_sound_lane()];
    return 0;
}
/* Stop only voices whose mutable recipe is about to change. The ordinary
 * declick tail absorbs their last sample. Mutation and voice retirement are
 * atomic against the audio interrupt; new hits use the complete new recipe. */
static void drum_sound_retire(uint32_t kit, const dsnd_t *d)
{
    uint32_t i;
    for (i = 0; i < NDRUM; i++)
        if (drums.v[i].active && drums.synth[i] && drums.kit[i] == kit && (!d || drums.ds[i].d == d)) {
            drums.tail += drums.v[i].s[7];
            drums.v[i].active = 0;
        }
}
static int drum_sound_tunable(const dsnd_t *d)
{
    return d->wave != DW_NONE || ((d->src & 15u) >= DN_METAL);
}
static void drum_sound_turn(uint32_t k, int32_t s)
{
    uint32_t kit = drum_kit();
    if (drum_page == 3) {
        if (!k) drum_copy_slot = (uint8_t)clamp(drum_copy_slot + clamp(s, -4, 4), 0, DSU_N - 1u);
    } else if (kit >= DRUM_SYN) {
        dsnd_t *d = &dsu.k[kit - DRUM_SYN].s[drum_sound_lane()];
        dsnd_t next = *d;
        s = clamp(s, -127, 127);
        if (!k && drum_sound_tunable(d)) {
            uint8_t *pitch = (d->src & 15u) == DN_CHIP && d->wave == DW_NONE ? &next.chip : &next.pitch;
            *pitch = (uint8_t)clamp(*pitch + s, 0, 127);
        }
        if (k == 1) {
            next.decay = (uint8_t)clamp(d->decay + s, 0, 127);
            next.ndec = (uint8_t)clamp(d->ndec + s, 0, 127);
        }
        if (k == 2) {
            next.fcut = (uint8_t)clamp(d->fcut + s, 0, 127);
            /* Whole-sound filter, including a pure sine kick. Retain the
             * existing filter type/resonance, enable LP if previously off. */
            next.flt |= DF_ALL;
            if (!(next.flt & 3u)) next.flt |= DF_LP;
        }
        if (k == 3) next.level = (uint8_t)clamp(d->level + s * 4, 0, 255);
        if (memcmp(d, &next, sizeof next)) {
            fm1_irq_off();
            drum_sound_retire(kit, d);
            *d = next;
            dsu_dirty = 1;
            fm1_irq_on();
        }
    } else ui_message(drum_sound_recipe() ? "ENV: COPY TO EDIT" : "SAMPLES: USE EDIT / FX");
    ui.force = 1;
}
static void drum_sound_open(void)
{
    uint32_t kit = drum_kit();
    if (rec_wait || ft_on || ci_on) { ui_message("FINISH TAKE FIRST"); return; }
    if (on_drum_page() && drum_page == 3) {
        if (kit >= DRUM_SAMPLED && kit < DRUM_USR) {
            (void)dsu_kit(0);
            fm1_irq_off();
            drum_sound_retire(DRUM_SYN + drum_copy_slot, 0);
            dsu_from_factory(drum_copy_slot, kit - DRUM_SAMPLED);
            dsu_dirty = 1;
            TDRUM->p[P_E0] = (int16_t)(DRUM_SYN + drum_copy_slot);
            fm1_irq_on();
            ui_message("COPIED / SAVE TO KEEP");
        }
        drum_page = 2;
    } else if (on_drum_page() && drum_page == 2 && kit >= DRUM_SAMPLED && kit < DRUM_USR) {
        drum_copy_slot = 0;
        drum_page = 3;
    } else drum_page = 2;
    studio_open(SC_DRUM);
    ui.force = 1;
}
static void drum_sound_draw(void)
{
    static uint32_t head, body, footer;
    static const char *const copy_labels[4] = {"slot", "", "", ""};
    static const char *const labels[4] = {"tune", "decay", "filter", "level"};
    const dsnd_t *d = drum_sound_recipe();
    uint32_t kit = drum_kit(), sig = kit * 197u + drum_lane * 31u + drum_page * 7u + drum_copy_slot;
    char b[24], v[4][12];
    const char *values[4] = {v[0], v[1], v[2], v[3]};
    int32_t ratio[4] = {0, 0, 0, 0};
    uint32_t i;
    te_header(drum_page == 3 ? "copy kit?" : "drum sound", TE_DRUM, &head);
    if (ui.force || sig != body) {
        body = sig;
        cv_begin(240, 120, C_BLACK);
        cv_text(4, 4, &FONT_L, drum_page == 3 ? DRUM_KIT_NAMES[DRUM_SYN + drum_copy_slot] : LANE_SHORT[drum_lane], C_WHITE);
        cv_text(4, 36, &FONT_S, DRUM_KIT_NAMES[kit], TE_DRUM);
        cv_text(4, 58, &FONT_S, drum_page == 3 ? "REPLACE WHOLE SYN KIT" : !d ? "SAMPLED / USE EDIT + FX" : kit < DRUM_SYN ? "ENV: COPY KIT TO EDIT" : "KEYS SELECT + PLAY", TE_G4);
        cv_text(4, 78, &FONT_S, drum_page == 3 ? "SHARED BY SECTIONS A-D" : kit >= DRUM_SYN ? "SAVE NOW TO KEEP EDITS" : "FACTORY KIT UNCHANGED", TE_G3);
        cv_text(4, 100, &FONT_S, drum_page == 3 ? "K1: SLOT  ENV: YES" : drum_lane == 4 || drum_lane == 6 ? "CLOSED + PEDAL LINKED" : "EDIT: KIT  SEQ: HITS", TE_G3);
        cv_blit(0, 40);
        cv_begin(240, 24, C_BLACK);
        cv_text(4, 4, &FONT_S, drum_page == 3 ? "HOME: CANCEL" : kit >= DRUM_SYN ? "SYN BANK SHARED BY A-D" : "", TE_G3);
        cv_blit(0, 160);
    }
    for (i = 0; i < 4; i++) str_cpy(v[i], "--", sizeof v[i]);
    if (d && drum_page != 3) {
        if (drum_sound_tunable(d)) {
            i = (d->src & 15u) == DN_CHIP && d->wave == DW_NONE ? d->chip : d->pitch;
            note_name(v[0], i); ratio[0] = (int32_t)i * 1000 / 127;
        }
        fmt_int(v[1], d->wave ? d->decay : d->ndec);
        fmt_int(v[2], d->fcut);
        fmt_int(b, ((int32_t)d->level - 128) / 4);
        str_cpy(v[3], b, sizeof v[3]);
        str_cpy(v[3] + str_len(v[3]), "dB", 3);
        ratio[1] = (d->wave ? d->decay : d->ndec) * 1000 / 127;
        ratio[2] = d->fcut * 1000 / 127;
        ratio[3] = d->level * 1000 / 255;
    }
    if (drum_page == 3) {
        str_cpy(v[0], DRUM_KIT_NAMES[DRUM_SYN + drum_copy_slot], sizeof v[0]);
        ratio[0] = drum_copy_slot * 1000 / 3;
        v[1][0] = v[2][0] = v[3][0] = 0;
    }
    te_dials(184, drum_page == 3 ? copy_labels : labels, values, ratio, 3u, &footer);
}
