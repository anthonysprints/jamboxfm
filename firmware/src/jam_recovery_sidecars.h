/* SPDX-License-Identifier: GPL-3.0-only */
/* Recovery-only live sound state that the private JRM1 musical snapshot does
 * not contain. Included after proj_apply, before arranger_scene.c. These are
 * RAM bookkeeping, not a native structure to write to flash. */
#ifndef JAM_RECOVERY_SIDECARS_H
#define JAM_RECOVERY_SIDECARS_H

/* sound_valid's low NTRK bits identify curated sound labels; bit 7 means the
 * whole sidecar was captured or fully restored. Empty/uninitialized sidecars
 * must leave ordinary project/FM6 factory loading intact. A recovery importer
 * sets READY only after all patch bytes and identities have been validated. */
#define JAM_RECOVERY_SIDECAR_READY 0x80u
typedef struct {
    uint8_t fm6[NPART][FP_SIZE + 1u];
    uint8_t sound_id[NTRK];
    uint8_t sound_valid;
} jam_recovery_sidecar_t;
_Static_assert(sizeof(jam_recovery_sidecar_t) == 473u, "bounded live sound sidecar");
_Static_assert(NTRK < 7u, "curated bits leave the sidecar readiness marker free");

static jam_recovery_sidecar_t jam_recovery_sections[4];

/* Mutable for the validated boot importer; index 0..3 means section A..D.
 * The recovery wire snapshots use 1..4 for these same sections. */
static jam_recovery_sidecar_t *jam_recovery_section_sidecar(uint32_t section)
{
    return section < 4u ? &jam_recovery_sections[section] : 0;
}

/* Capture alongside proj_capture with audio mutations excluded. Do not reload
 * presets: an edited curated sound keeps both its actual patch and its name. */
static void jam_recovery_sidecar_capture(jam_recovery_sidecar_t *out)
{
    uint32_t t;
    memset(out, 0, sizeof *out);
    for (t = 0; t < NPART; t++)
        if (trk[t].eng_req == ENGI_FM6)
            memcpy(out->fm6[t], fm6_patch[t], sizeof out->fm6[t]);
    for (t = 0; t < NTRK; t++)
        if (jam_sound_is_curated(t)) {
            out->sound_id[t] = (uint8_t)jam_sound_index(t);
            out->sound_valid |= (uint8_t)(1u << t);
        }
    out->sound_valid |= JAM_RECOVERY_SIDECAR_READY;
}

/* Apply only after ordinary proj_apply and with audio mutations excluded.
 * Synchronize the loaded PTCH marker and cancel a deferred preset load, so
 * the next fm6_poll cannot immediately replace this actual edited patch.
 * Later PTCH knob changes / factory parameter locks still use normal polling. */
static void jam_recovery_sidecar_apply(const jam_recovery_sidecar_t *in)
{
    uint32_t t;
    if (!in || !(in->sound_valid & JAM_RECOVERY_SIDECAR_READY))
        return;
    for (t = 0; t < NPART; t++)
        if (trk[t].eng_req == ENGI_FM6) {
            fm6_set_patch(t, in->fm6[t]);
            fm6_slot[t] = (uint8_t)trk[t].p[P_E7];
            fm6_pend &= (uint8_t)~(1u << t);
        }
    jam_sound_valid = (uint8_t)(in->sound_valid & ((1u << NTRK) - 1u));
    for (t = 0; t < NTRK; t++) {
        jam_sound_id[t] = in->sound_id[t];
        if (in->sound_id[t] >= jam_sound_count(t) || !jam_sound_is_curated(t))
            jam_sound_invalidate(t);
    }
}

static void proj_apply_with_sidecar(const project_t *p, int all,
                                  const jam_recovery_sidecar_t *sidecar)
{
    proj_apply(p, all);
    jam_recovery_sidecar_apply(sidecar);
}

#endif
