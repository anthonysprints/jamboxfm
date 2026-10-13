/* SPDX-License-Identifier: GPL-3.0-only */
/* JamBox browser entry point. Keep Sloop's preview adapter unchanged. The
 * audio worklet is the sole owner of this instance and calls these functions
 * between render blocks; local pads use the firmware keyboard path. */
#include "preview.c"
#include "../src/jam_palette.c"

void jam_preview_init(void)
{
    preview_init();
    jam_new_dusty();
}

void jam_preview_key(uint32_t key, uint32_t on)
{
    if (key >= 27u) return;
    if (on) fm1_in.notes |= 1u << key;
    else fm1_in.notes &= ~(1u << key);
    /* Consume each edge even when press/release arrive in the same audio
     * quantum. keyboard_block maintains Sloop's release/track/chord ledger. */
    keyboard_block();
}

void jam_preview_release(void)
{
    fm1_in.notes = 0;
    keyboard_block();
}

void jam_preview_panic(void)
{
    jam_preview_release();
    preview_panic();
}

void jam_preview_select(uint32_t track)
{
    if (track >= NTRK) return;
    jam_preview_release();
    song.rec = rec_wait = 0; /* finish this part's take before moving to another */
    preview_select(track, 0);
}

void jam_preview_play(uint32_t on)
{
    if (!on) jam_preview_release();
    preview_play(on != 0);
}
void jam_preview_toggle_play(void)
{
    /* A second click can arrive before events_block consumes the first one. */
    uint32_t on = transport_req == 1u || transport_req == 3u || (!transport_req && song.playing);
    jam_preview_play(!on);
}

/* The existing Sloop recorder, deliberately restricted to TEMPO / NOTE:
 * playing -> grid-snapped overdub now; stopped -> first local note starts it.
 * Finishing a take releases held keys but keeps the loop playing. */
void jam_preview_record(uint32_t on)
{
    jam_preview_release();
    if (!on) { song.rec = rec_wait = 0; return; }
    rec_tempo = 1;
    rec_count = 0;
    if (song.playing) rec_begin();
    else rec_wait = 1;
}

void jam_preview_tempo(int32_t bpm) { preview_global(G_BPM, bpm); }
void jam_preview_swing(int32_t swing) { preview_global(G_SWING, swing); }
void jam_preview_octave(int32_t octave)
{
    jam_preview_release();
    song.octave = (int8_t)clamp(octave, -3, 3);
}
void jam_preview_mute(uint32_t track, uint32_t on)
{
    if (track < NTRK) preview_param(track, P_MUTE, on != 0);
}
void jam_preview_sound(uint32_t track, uint32_t sound)
{
    if (track >= NTRK) return;
    jam_preview_release();
    jam_sound_load(track, sound);
}
/* Explicit, per-part Sloop settings: root, scale, pitch mapping, chord mode.
 * These are not the proposed JamBox Assist master switch/timing modes. */
void jam_preview_setting(uint32_t track, uint32_t setting, int32_t value)
{
    static const uint8_t ids[] = {P_ROOT, P_SCALE, P_QUANT, P_CHORD};
    if (track >= NPART || setting >= 4u) return;
    jam_preview_release();
    preview_param(track, ids[setting], value);
}

/* Toggle only the chosen drum bit: other lanes, level, ratchet and timing stay. */
void jam_preview_drum_step(uint32_t step, uint32_t lane)
{
    if (step >= NSTEP || lane >= DRUM_LANES) return;
    TDRUM->dstep[step].on[lane >> 3] ^= (uint8_t)(1u << (lane & 7u));
}

int32_t jam_preview_value(uint32_t field, uint32_t track, uint32_t index)
{
    switch (field) {
    case 0: return song.sel;
    case 1: return song.playing;
    case 2: return song.g[G_BPM];
    case 3: return song.g[G_SWING];
    case 4: return song.octave;
    case 12: return fm1_in.notes;
    case 13: return index < NSTEP ? (int32_t)dstep_mask(&TDRUM->dstep[index]) : 0;
    case 16: return index < 27u ? (int32_t)kb_map(&trk[song.sel % NTRK], index) : -1;
    case 17: return index < 27u ? kb_n[index] : 0;
    case 18: return track < 27u && index < kb_n[track] ? kb_nt[track][index] : -1;
    case 19: return song.rec;
    case 20: return rec_wait;
    default: break;
    }
    if (track >= NTRK) return -1;
    switch (field) {
    case 5: return trk[track].p[P_MUTE];
    case 6: return trk[track].p[P_CHORD];
    case 7: return trk[track].p[P_SCALE];
    case 8: return trk[track].p[P_ROOT];
    case 9: return trk[track].p[P_QUANT];
    case 10: return preview_position(track);
    case 11: return trk[track].nheld;
    case 14: return trk[track].p[P_SLEN];
    case 21: {
        uint32_t count = 0, i;
        for (i = 0; i < NSTEP; i++) {
            if (track == TRK_DRUM) count += dstep_mask(&trk[track].dstep[i]) != 0;
            else if (trk[track].step[i].time == ST_NOTE) count += trk[track].step[i].n;
        }
        return (int32_t)count;
    }
    case 22: return track < NPART && index < NSTEP && trk[track].step[index].time == ST_NOTE ? trk[track].step[index].n : 0;
    case 23: return track < NPART && index / 4u < NSTEP ? trk[track].step[index / 4u].note[index % 4u] : -1;
    default: return -1;
    }
}

const char *jam_preview_pad_name(uint32_t key)
{
    static char label[8];
    const char *name;
    uint32_t note, i = 0;
    if (key >= 27u) return "";
    if (song.sel == TRK_DRUM) return LANE_NAME[lane_of_key(key)];
    note = kb_map(&trk[song.sel % NTRK], key);
    if (note == KB_SILENT) return "";
    name = N_NOTE[note % 12u];
    while (*name && i < 3u) label[i++] = *name++;
    if (note < 12u) label[i++] = '-';
    label[i++] = (char)('0' + (note < 12u ? 1u : note / 12u - 1u));
    label[i] = 0;
    return label;
}

const char *jam_preview_lane_name(uint32_t lane)
{
    return lane < DRUM_LANES ? LANE_NAME[lane] : "";
}
