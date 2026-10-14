/* SPDX-License-Identifier: GPL-3.0-only */
/* Included by seq.c. Endpoints follow the track grid through swing/tempo changes. */
static void jam_sequence_start(track_t *t, const step_t *s, uint32_t slot, uint32_t duration, uint32_t skip)
{
    uint32_t tr = trk_index(t), i, free = NVOICE, note = s->note[slot], victim = 0;
    jam_sequence_note_t *v;
    if (tr >= NPART || skip) return;
    /* A live recorded note keeps its physical release ownership across the
     * loop seam. It must not be retriggered by its freshly captured onset. */
    if ((song.rec & (1u<<tr)) && jam_live_owns(t,note)) return;
    for (i = 0; i < NVOICE; i++) {
        v = &jam_sequence_note[tr][i];
        if (v->active && v->note == note) { free = i; break; }
        if (!v->active && free == NVOICE) free = i;
        if ((int32_t)(v->end-jam_sequence_note[tr][victim].end) < 0) victim = i;
    }
    if (free == NVOICE) {
        free = victim;
        v = &jam_sequence_note[tr][free];
        if (!jam_live_owns(t,v->note)) trk_note_off(t,v->note);
        seq_out_off(t,v->note);
    }
    v = &jam_sequence_note[tr][free];
    v->active = 1; v->note = (uint8_t)note; v->generation = jam_pattern_generation[tr];
    v->start = t->seq_abs;
    v->end = t->seq_abs + (uint32_t)clamp(duration,1,trk_len(t));
    if (!jam_live_owns(t,note) && !roll_has(t,note)) {
        trk_note_chord(t,s->note,s->n,slot,step_vel(s,slot));
        seq_out_on(t,note,step_vel(s,slot));
    }
}
