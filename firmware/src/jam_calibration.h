/* SPDX-License-Identifier: GPL-3.0-only */
/* JamBox's read-only bridge to an existing FM-1 physical panel calibration.
 * Included by project.c after panel.c and storage.c. No settings migration,
 * user preferences, sounds or sessions are imported, and no flash is written.
 */
#ifndef JAM_CALIBRATION_H
#define JAM_CALIBRATION_H
#if JAMBOX_PROTO && FELUCCA_FLASH && !FELUCCA_PERSIST

_Static_assert(sizeof(panel_t) == 32u && NB == 14 && NE == 7,
               "audited PAN5 panel layout must remain 32 bytes");

static int jam_panel_valid(const panel_t *q)
{
    uint32_t i, buttons = 0, encoders = 0;
    if (q->magic != PANEL_MAGIC)
        return 0;
    for (i = 0; i < NB; i++) {
        if (q->btn[i] >= 14u || ((buttons >> q->btn[i]) & 1u))
            return 0;
        buttons |= 1u << q->btn[i];
    }
    for (i = 0; i < NE; i++) {
        if (q->enc[i] >= 7u || ((encoders >> q->enc[i]) & 1u) ||
            (q->dir[i] != 1 && q->dir[i] != -1))
            return 0;
        encoders |= 1u << q->enc[i];
    }
    return 1;
}

/* Call after the JEDEC check initializes flash_ok. A fresh default is used
 * even after a warm reset: the shared PAN5 magic alone cannot identify which
 * firmware left .noinit RAM behind. Return 1 only when flash calibration was
 * imported; 0 means the default mapping. The caller resets retained musical
 * state separately, without clearing bootguard/crash records.
 *
 * PAN5 contains the 14 button matrix IDs, the 7 encoder matrix IDs and each
 * encoder's clockwise sign (+1/-1). These are permutations, not key/scale or
 * sound choices. There are no saved analog ADC offsets/gains here: the master
 * knob is an ADC input and encoder detent rest states are learned by the HAL.
 */
static int jam_calibration_boot(void)
{
    st_hdr_t h;
    panel_t candidate;
    uint32_t magic, offset = 16u;
    int known = 0;
    panel = PANEL_DEFAULT;
    if (!flash_ok || st_current(OBJ_SETTINGS, &h) < 0 || h.len < sizeof magic)
        return 0;
    memcpy(&magic, st_buf, sizeof magic);
    /* Match the complete CRC-validated payload's actual size. st_load's
     * truncated prefix length cannot distinguish these schemas. PER3 has
     * different meanings in Sloop and Felucca, but their panel offset agrees.
     * Deliberately reject unknown future sizes rather than migrate a prefix.
     */
    switch (magic) {
    case 0x50455231u:                       /* PER1: palette + panel */
        known = h.len == 40u;
        offset = 8u;
        break;
    case 0x50455232u:                       /* PER2: shared legacy; Sloop without arranger + lights */
        known = h.len == 48u || h.len == 52u;
        break;
    case 0x50455233u:                       /* PER3: Felucca + bold; Sloop + arrangement/lights */
        known = h.len == 52u || h.len == 84u || h.len == 88u ||
                h.len == 1552u ||           /* Sloop: 88 + 1464-byte user synth drum bank */
                h.len == 1584u || h.len == 1588u || h.len == 1592u; /* + MIDI preferences */
        break;
    case 0x50455234u:                       /* PER4: Felucca/Ghoulbox + favorites */
        known = h.len == 572u;
        break;
    case 0x50455235u:                       /* PER5: ChoralRoot + 192-byte settings block */
        known = h.len == 764u;
        break;
    }
    if (!known || offset + sizeof candidate > h.len)
        return 0;
    memcpy(&candidate, st_buf + offset, sizeof candidate);
    if (!jam_panel_valid(&candidate))
        return 0;
    panel = candidate;
    return 1;
}

#endif
#endif
