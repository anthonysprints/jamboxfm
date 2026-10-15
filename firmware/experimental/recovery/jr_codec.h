/* SPDX-License-Identifier: GPL-3.0-only */
/* Experimental host-only payload codec. Not included in firmware builds.
 * No native structs are stored: all multibyte values below are little endian.
 * JRM1 components retain the runtime format's explicitly specified LE fields,
 * with FNV-1a over bytes 0..4579. No JRM0/FUN* import is implicit.
 */
#ifndef JR_CODEC_H
#define JR_CODEC_H
#include "jr_io.h"
#define JR_SNAPSHOTS 5u
#define JR_PARTS 3u
#define JR_TRACKS 4u
#define JR_SNAPSHOT_BYTES 4584u
#define JR_FM6_BYTES 156u
#define JR_SYN_BYTES 1464u
#define JR_METADATA_BYTES 512u
#define JR_ARRANGEMENT_BYTES 36u
#define JR_SNAPSHOT_OFFSET 0u
#define JR_FM6_OFFSET (JR_SNAPSHOTS * JR_SNAPSHOT_BYTES)
#define JR_SYN_OFFSET (JR_FM6_OFFSET + JR_SNAPSHOTS * JR_PARTS * JR_FM6_BYTES)
#define JR_METADATA_OFFSET (JR_SYN_OFFSET + JR_SYN_BYTES)
#define JR_ARRANGEMENT_OFFSET (JR_METADATA_OFFSET + JR_METADATA_BYTES)
#define JR_PAYLOAD_BYTES (JR_ARRANGEMENT_OFFSET + JR_ARRANGEMENT_BYTES)
/* Snapshot zero is the working loop; 1..4 are sections A..D. An unused
 * section's snapshot, FM6 sidecars and identities must be all zero. An active
 * FM6 part has one sanitized actual patch; all non-FM6 sidecars are zero. */
typedef struct jr_payload_view {
    const uint8_t *snapshot[JR_SNAPSHOTS];
    const uint8_t *fm6[JR_SNAPSHOTS][JR_PARTS];
    const uint8_t *syn_bank;
    const uint8_t *metadata;
    const uint8_t *arrangement;
} jr_payload_view_t;
/* NULL components emit zero bytes. Every non-NULL component must have the
 * exact documented length and remain immutable throughout a transaction. */
int jr_codec_read_at(void *view, uint32_t offset, void *dst, uint32_t bytes);
int jr_codec_validate(void *unused, const jr_source_t *source);
uint32_t jr_codec_fnv1a(const void *data, uint32_t bytes);
/* Metadata: 0..159 five sets of four identities (8 bytes each): LE u32 ID,
 * origin (1 curated,2 engine factory,3 custom), engine, edited bool, zero.
 * Curated IDs are (track+1)*256 + one-based bank entry; factory IDs are
 * one-based engine preset index; custom ID is zero and edited must be one.
 * 160..191 session: "JMD1", LE16 version=1, LE16 bytes=512, section mask,
 * selected track, signed octave -3..3, Home view 0..1, arrangement enabled,
 * selected section (0..3 or FF), followed by 18 zero bytes.
 * 192..319 own typed preferences: "JDP1", LE16 version=1, LE16 bytes=128;
 * 8 palette(0..4),9 lowcut,10 zoom,11 light level(0..3),12 key lights(0..3),
 * 13 sounding lights,14 USB full,15 USB serial,16 rec tempo,17 rec count,
 * 18 MIDI out(0..1),19 sync(0..2),20 routing(0..17),21..31 zero;
 * 32..63 sixteen LE16 learned CC codes (seq.c encoding),64..66 channels1..3,
 * 67 drum channel,68 other-channel policy,69 bend range1..12,70..127 zero.
 * 320..383 fixed asset/catalog identity (SHA-256 generated assets + SHA-256
 * source catalog; verified manifest in check_jam_recovery_identity.py);
 * 384..511 zero reserved.
 * Boolean fields accept only 0/1. No panel calibration lives in this format.
 */
#define JR_META_IDENTITIES 0u
#define JR_META_SESSION 160u
#define JR_META_PREFS 192u
#define JR_META_ASSETS 320u
void jr_codec_metadata_init(uint8_t metadata[JR_METADATA_BYTES], uint8_t section_mask);
void jr_codec_identity_set(uint8_t metadata[JR_METADATA_BYTES], uint32_t snapshot,
                           uint32_t track, uint32_t id, uint8_t origin,
                           uint8_t engine, uint8_t edited);
#endif
