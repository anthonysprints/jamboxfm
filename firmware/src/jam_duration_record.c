/* SPDX-License-Identifier: GPL-3.0-only */
/* Included by seq.c. Lengths use the same quantized grid as note starts. */
static void jam_record_prepare(track_t *t)
{
    uint32_t i, k, n, len = trk_len(t), tr = trk_index(t);
    if (tr >= NPART) return;
    /* Preserve legacy held groups before inserting a new onset into a TIE.
     * Short legacy steps retain their existing gate/ratchet articulation. */
    for (i = 0; i < len; i++) if (t->step[i].time == ST_NOTE && t->step[i].n) {
        n = 1;
        while (n < len && t->step[(i+n)%len].time == ST_TIE) n++;
        if (n > 1) for (k = 0; k < t->step[i].n && k < 4; k++)
            if (!t->note_len[i][k]) {
                t->note_len[i][k] = (uint8_t)n;
                t->duration_mode = 1;
            }
    }
    /* An already sounding legacy group becomes independently owned without
     * retriggering it. Recording can then insert a note underneath that hold. */
    if (t->duration_mode && t->seq_n) {
        uint32_t back = 0, origin = t->seq_idx % len;
        while (back < len && t->step[origin].time == ST_TIE) { origin = (origin+len-1)%len; back++; }
        if (back < len && t->note_len[origin][0] > back) {
            for (i = 0; i < t->seq_n && i < NVOICE; i++) {
                jam_sequence_note[tr][i].active = 1;
                jam_sequence_note[tr][i].generation = jam_pattern_generation[tr];
                jam_sequence_note[tr][i].note = t->seq_notes[i];
                jam_sequence_note[tr][i].start = t->seq_abs - back;
                jam_sequence_note[tr][i].end = t->seq_abs - back + t->note_len[origin][0];
            }
            t->seq_n = t->seq_hold = 0;
        }
    }
}
static int jam_record_valid(track_t *t, const jam_record_hold_t *h)
{
    return h->active && h->generation == jam_pattern_generation[trk_index(t)] &&
        t->step[h->step].time == ST_NOTE && h->slot < t->step[h->step].n &&
        t->step[h->step].note[h->slot] == h->note && t->note_len[h->step][h->slot];
}
static void jam_record_begin(track_t *t, uint32_t idx, uint32_t abs, uint32_t note)
{
    uint32_t tr = trk_index(t), i, slot, free = 16;
    jam_record_hold_t *h;
    if (tr >= NPART) return;
    for (slot = 0; slot < t->step[idx].n && t->step[idx].note[slot] != note; slot++);
    if (slot >= 4 || slot >= t->step[idx].n) return;
    for (i = 0; i < 16; i++) {
        h = &jam_record_hold[tr][i];
        if (!jam_record_valid(t,h)) h->active = 0;
        if (h->active && h->note == note) {
            t->note_len[h->step][h->slot] = (uint8_t)clamp((int32_t)(abs-h->start),1,trk_len(t));
            h->active = 0;
        }
        if (!h->active && free == 16) free = i;
    }
    t->note_len[idx][slot] = 1;
    t->duration_mode = 1;
    if (free == 16) {
        /* Beyond the audible voice budget, finish the oldest held capture and
         * keep recording the newest press rather than silently shortening it. */
        free = 0;
        for (i=1; i<16; i++) if ((int32_t)(jam_record_hold[tr][i].start-jam_record_hold[tr][free].start) < 0) free=i;
        h = &jam_record_hold[tr][free];
        t->note_len[h->step][h->slot] = (uint8_t)clamp((int32_t)(abs-h->start),1,trk_len(t));
    }
    h = &jam_record_hold[tr][free];
    h->start = abs; h->generation = jam_pattern_generation[tr]; h->active = 1;
    h->note = (uint8_t)note; h->step = (uint8_t)idx; h->slot = (uint8_t)slot;
}
static void jam_record_extend(track_t *t, uint32_t abs)
{
    uint32_t i, tr = trk_index(t), len = trk_len(t);
    if (tr >= NPART) return;
    for (i = 0; i < 16; i++) {
        jam_record_hold_t *h = &jam_record_hold[tr][i];
        if (!jam_record_valid(t,h)) { h->active = 0; continue; }
        if (!(song.rec & (1u<<tr))) { h->active = 0; continue; }
        if ((int32_t)(abs-h->start) < 0) continue;
        t->note_len[h->step][h->slot] = (uint8_t)clamp((int32_t)(abs-h->start+1),1,len);
        if (abs-h->start+1 >= len) h->active = 0;
    }
}
static void jam_record_end(track_t *t, uint32_t note)
{
    uint32_t i, later, tr = trk_index(t), abs;
    if (tr >= NPART) return;
    abs = rec_target(t,&later);
    for (i = 0; i < 16; i++) {
        jam_record_hold_t *h = &jam_record_hold[tr][i];
        if (!jam_record_valid(t,h)) { h->active = 0; continue; }
        if (h->note == note) {
            t->note_len[h->step][h->slot] = (uint8_t)clamp((int32_t)(abs-h->start),1,trk_len(t));
            h->active = 0;
        }
    }
}
static void jam_record_finish(track_t *t)
{
    uint32_t i, later, tr = trk_index(t), abs;
    if (tr >= NPART) return;
    abs = rec_target(t,&later);
    for (i = 0; i < 16; i++) {
        jam_record_hold_t *h = &jam_record_hold[tr][i];
        if (jam_record_valid(t,h))
            t->note_len[h->step][h->slot] = (uint8_t)clamp((int32_t)(abs-h->start),1,trk_len(t));
        h->active = 0;
    }
}
