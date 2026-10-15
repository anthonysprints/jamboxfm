/* SPDX-License-Identifier: GPL-3.0-only */
#include "jr_store.h"
#include <stddef.h>
#include <string.h>

_Static_assert(JR_STORE_PAYLOAD_MAX + JR_STORE_HEADER_BYTES ==
               JR_STORE_SECTORS * JR_STORE_SECTOR_BYTES,
               "recovery allocation capacity");
_Static_assert(JR_STORE_HEADER_BYTES == JR_STORE_PAGE_BYTES,
               "one-page recovery commit");

static const uint32_t sectors[2][JR_STORE_SECTORS] = {
    { 0x97000u, 0x98000u, 0x99000u, 0x9a000u, 0x9b000u, 0x9c000u, 0x9d000u },
    { 0x9e000u, 0x9f000u, 0xdc000u, 0xdd000u, 0xde000u, 0xdf000u, 0xfe000u }
};

static uint16_t get16(const uint8_t *p)
{ return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8)); }
static uint32_t get32(const uint8_t *p)
{ return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static void put16(uint8_t *p, uint16_t n)
{ p[0] = (uint8_t)n; p[1] = (uint8_t)(n >> 8); }
static void put32(uint8_t *p, uint32_t n)
{ p[0] = (uint8_t)n; p[1] = (uint8_t)(n >> 8);
  p[2] = (uint8_t)(n >> 16); p[3] = (uint8_t)(n >> 24); }

static uint32_t crc_add(uint32_t crc, const uint8_t *p, uint32_t n)
{
    static const uint32_t table[16] = {
        0x00000000u, 0x1db71064u, 0x3b6e20c8u, 0x26d930acu,
        0x76dc4190u, 0x6b6b51f4u, 0x4db26158u, 0x5005713cu,
        0xedb88320u, 0xf00f9344u, 0xd6d6a3e8u, 0xcb61b38cu,
        0x9b64c2b0u, 0x86d3d2d4u, 0xa00ae278u, 0xbdbdf21cu
    };
    while (n--) {
        crc ^= *p++;
        crc = (crc >> 4) ^ table[crc & 15u];
        crc = (crc >> 4) ^ table[crc & 15u];
    }
    return crc;
}

/* A torn first commit can leave any field byte erased. These compatible
 * partial bytes are invalid, but cannot masquerade as an unknown schema or
 * allocation and permanently prevent an intentional retry of initialization. */
static int partial_field(const uint8_t *p, uint32_t expected, unsigned n)
{
    unsigned i;
    for (i = 0; i < n; ++i)
        if (p[i] != 0xff && p[i] != (uint8_t)(expected >> (8 * i))) return 0;
    return 1;
}

uint32_t jr_store_sector_address(unsigned copy, unsigned sector)
{
    if (copy > 1 || sector >= JR_STORE_SECTORS) return UINT32_MAX;
    return sectors[copy][sector];
}

uint32_t jr_store_allocation_crc(void)
{
    uint8_t word[4];
    uint32_t crc = UINT32_MAX;
    unsigned c, s;
    for (c = 0; c < 2; ++c) for (s = 0; s < JR_STORE_SECTORS; ++s) {
        put32(word, sectors[c][s]);
        crc = crc_add(crc, word, sizeof word);
    }
    return crc ^ UINT32_MAX;
}

/* This private reader maps only the complete declared logical allocation. */
static int logical_read(const jr_nor_t *nor, unsigned copy, uint32_t off,
                        void *dst, uint32_t n)
{
    uint8_t *p = (uint8_t *)dst;
    uint32_t chunk, sector, within;
    if (!nor || !nor->read || copy > 1 ||
        off > JR_STORE_SECTORS * JR_STORE_SECTOR_BYTES ||
        n > JR_STORE_SECTORS * JR_STORE_SECTOR_BYTES - off || (!dst && n))
        return -1;
    while (n) {
        sector = off / JR_STORE_SECTOR_BYTES;
        within = off % JR_STORE_SECTOR_BYTES;
        chunk = JR_STORE_SECTOR_BYTES - within;
        if (chunk > n) chunk = n;
        if (nor->read(nor->ctx, sectors[copy][sector] + within, p, chunk))
            return -1;
        off += chunk; p += chunk; n -= chunk;
    }
    return 0;
}

int jr_store_read(void *ctx, uint32_t offset, void *dst, uint32_t length)
{
    jr_store_reader_t *r = (jr_store_reader_t *)ctx;
    if (!r) return -1;
    if (r->copy > 1 || r->length > JR_STORE_PAYLOAD_MAX ||
        offset > r->length || length > r->length - offset || (!dst && length) ||
        logical_read(r->nor, r->copy, JR_STORE_HEADER_BYTES + offset,
                     dst, length)) {
        r->read_failed = 1;
        return -1;
    }
    return 0;
}

void jr_store_source(jr_store_reader_t *r, const jr_nor_t *nor,
                     unsigned copy, uint32_t length, jr_source_t *out)
{
    if (!r || !out) return;
    r->nor = nor; r->copy = copy; r->length = length; r->read_failed = 0;
    out->ctx = r; out->read = jr_store_read; out->length = length;
}

typedef struct bounded_reader {
    const jr_source_t *source;
    int failed;
} bounded_reader_t;

static int bounded_read(void *ctx, uint32_t offset, void *dst, uint32_t n)
{
    bounded_reader_t *r = (bounded_reader_t *)ctx;
    if (offset > r->source->length || n > r->source->length - offset ||
        (!dst && n) || r->source->read(r->source->ctx, offset, dst, n)) {
        r->failed = 1;
        return -1;
    }
    return 0;
}

static int source_crc(const jr_source_t *source, uint32_t *out, uint8_t *page)
{
    uint32_t off = 0, n, crc = UINT32_MAX;
    while (off < source->length) {
        n = source->length - off;
        if (n > JR_STORE_PAGE_BYTES) n = JR_STORE_PAGE_BYTES;
        if (source->read(source->ctx, off, page, n)) return -1;
        crc = crc_add(crc, page, n);
        off += n;
    }
    *out = crc ^ UINT32_MAX;
    return 0;
}

/* Empty means every byte in the copy is erased, not just its first page. */
static int copy_erased(const jr_nor_t *nor, unsigned copy, uint8_t *page)
{
    uint32_t off, i;
    int erased = 1;
    for (off = 0; off < JR_STORE_SECTORS * JR_STORE_SECTOR_BYTES;
         off += JR_STORE_PAGE_BYTES) {
        if (logical_read(nor, copy, off, page, JR_STORE_PAGE_BYTES)) return -1;
        for (i = 0; i < JR_STORE_PAGE_BYTES; ++i)
            if (page[i] != 0xff) erased = 0;
    }
    return erased;
}

static void candidate_read(const jr_nor_t *nor, unsigned copy,
                           jr_validate_fn validate, void *validation_ctx,
                           jr_candidate_t *candidate, uint8_t *hdr)
{
    uint32_t crc, i;
    int result, erased, frame_valid;
    jr_store_reader_t r;
    jr_source_t source;
    memset(candidate, 0, sizeof *candidate);
    if (logical_read(nor, copy, 0, hdr, JR_STORE_HEADER_BYTES)) {
        candidate->status = JR_CANDIDATE_IO_ERROR; return;
    }
    frame_valid = get16(hdr + 8) == JR_STORE_HEADER_BYTES &&
        get32(hdr + JR_STORE_COMMIT_OFFSET) == JR_STORE_COMMIT_MARKER &&
        get32(hdr + JR_STORE_HEADER_CRC_OFFSET) ==
            (crc_add(UINT32_MAX, hdr, JR_STORE_HEADER_CRC_OFFSET) ^ UINT32_MAX);
    if (memcmp(hdr, "JBR1", 4)) {
        /* A future family using the shared envelope is protected. A future JBR
         * magic is also protected even if that family changed its framing.
         * Erased/partially written FF suffixes cannot claim a future family. */
        if (frame_valid || (!memcmp(hdr, "JBR", 3) && hdr[3] != 0xff)) {
            candidate->status = JR_CANDIDATE_PROTECTED; return;
        }
        erased = copy_erased(nor, copy, hdr);
        candidate->status = erased < 0 ? JR_CANDIDATE_IO_ERROR :
            erased ? JR_CANDIDATE_EMPTY : JR_CANDIDATE_LEGACY;
        return;
    }
    /* Never erase identifiable newer schema/allocation/kind data merely
     * because an older decoder cannot validate its framing. FF fields may be
     * an interrupted known-format header and do not establish ownership. */
    if ((get16(hdr + 4) != JR_STORE_SCHEMA &&
            (frame_valid || !partial_field(hdr + 4, JR_STORE_SCHEMA, 2))) ||
        (get16(hdr + 6) != JR_STORE_KIND_RECOVERY &&
            (frame_valid || !partial_field(hdr + 6, JR_STORE_KIND_RECOVERY, 2))) ||
        (get16(hdr + 10) != JR_STORE_ALLOCATION_VERSION &&
            (frame_valid || !partial_field(hdr + 10, JR_STORE_ALLOCATION_VERSION, 2))) ||
        (get32(hdr + 24) != jr_store_allocation_crc() &&
            (frame_valid || !partial_field(hdr + 24, jr_store_allocation_crc(), 4)))) {
        candidate->status = JR_CANDIDATE_PROTECTED; return;
    }
    candidate->status = JR_CANDIDATE_INVALID;
    if (!frame_valid || get16(hdr + 4) != JR_STORE_SCHEMA ||
        get16(hdr + 6) != JR_STORE_KIND_RECOVERY ||
        get16(hdr + 10) != JR_STORE_ALLOCATION_VERSION ||
        get32(hdr + 24) != jr_store_allocation_crc()) return;
    for (i = 28; i < JR_STORE_HEADER_CRC_OFFSET; ++i)
        if (hdr[i]) return;
    candidate->length = get32(hdr + 12);
    candidate->generation = get32(hdr + 16);
    candidate->payload_crc = get32(hdr + 20);
    if (!candidate->length || candidate->length > JR_STORE_PAYLOAD_MAX) return;
    jr_store_source(&r, nor, copy, candidate->length, &source);
    if (source_crc(&source, &crc, hdr)) {
        candidate->status = JR_CANDIDATE_IO_ERROR; return;
    }
    if (crc != candidate->payload_crc) return;
    result = validate(validation_ctx, &source);
    if (r.read_failed || result == JR_VALID_IO)
        candidate->status = JR_CANDIDATE_IO_ERROR;
    else if (result == JR_VALID_UNSUPPORTED)
        candidate->status = JR_CANDIDATE_PROTECTED;
    else if (result == JR_VALID_OK)
        candidate->status = JR_CANDIDATE_VALID;
}

/* At equal generations CRC equality alone does not prove identical payloads. */
static int copies_equal(const jr_nor_t *nor, uint32_t length, uint8_t *page)
{
    uint32_t off = 0, n;
    while (off < length) {
        n = length - off;
        if (n > JR_STORE_PAGE_BYTES / 2) n = JR_STORE_PAGE_BYTES / 2;
        if (logical_read(nor, 0, JR_STORE_HEADER_BYTES + off, page, n) ||
            logical_read(nor, 1, JR_STORE_HEADER_BYTES + off, page + n, n))
            return -1;
        if (memcmp(page, page + n, n)) return 0;
        off += n;
    }
    return 1;
}

static int scan_with_scratch(const jr_nor_t *nor, jr_validate_fn validate,
                              void *validation_ctx, jr_store_state_t *out,
                              uint8_t *scratch)
{
    unsigned c;
    uint32_t delta;
    int equal, io = 0, protected = 0;
    if (!out) return JR_STORE_INVALID_ARGUMENT;
    memset(out, 0, sizeof *out); out->winner = -1;
    if (!nor || !nor->read || !validate) return JR_STORE_INVALID_ARGUMENT;
    for (c = 0; c < 2; ++c) {
        candidate_read(nor, c, validate, validation_ctx, &out->copies[c], scratch);
        if (out->copies[c].status == JR_CANDIDATE_IO_ERROR) io = 1;
        if (out->copies[c].status == JR_CANDIDATE_PROTECTED) protected = 1;
        if (out->copies[c].status == JR_CANDIDATE_VALID) {
            out->allocation_claimed = 1;
            out->winner = (int)c;
        }
    }
    out->writable = !io && !protected;
    if (out->copies[0].status == JR_CANDIDATE_VALID &&
        out->copies[1].status == JR_CANDIDATE_VALID) {
        delta = out->copies[1].generation - out->copies[0].generation;
        if (!delta) {
            out->winner = 0;
            equal = 0;
            if (out->copies[0].length == out->copies[1].length &&
                out->copies[0].payload_crc == out->copies[1].payload_crc)
                equal = copies_equal(nor, out->copies[0].length, scratch);
            if (equal < 0) { io = 1; out->writable = 0; }
            else if (!equal) out->writable = 0;
        } else if (delta == 0x80000000u) {
            out->winner = 0; out->writable = 0;
        } else out->winner = delta < 0x80000000u ? 1 : 0;
    }
    if (io) return JR_STORE_IO_ERROR;
    if (!out->writable) return JR_STORE_READ_ONLY;
    return out->winner >= 0 ? JR_STORE_OK : JR_STORE_SETUP_NEEDED;
}

int jr_store_scan(const jr_nor_t *nor, jr_validate_fn validate,
                  void *validation_ctx, jr_store_state_t *out)
{
    uint8_t scratch[JR_STORE_PAGE_BYTES];
    return scan_with_scratch(nor, validate, validation_ctx, out, scratch);
}

static int erase_target(const jr_nor_t *nor, unsigned copy, uint8_t *page)
{
    unsigned s;
    uint32_t off, i;
    for (s = 0; s < JR_STORE_SECTORS; ++s) {
        if (nor->erase_sector(nor->ctx, sectors[copy][s])) return JR_STORE_IO_ERROR;
        for (off = 0; off < JR_STORE_SECTOR_BYTES; off += JR_STORE_PAGE_BYTES) {
            if (nor->read(nor->ctx, sectors[copy][s] + off, page, JR_STORE_PAGE_BYTES))
                return JR_STORE_IO_ERROR;
            for (i = 0; i < JR_STORE_PAGE_BYTES; ++i)
                if (page[i] != 0xff) return JR_STORE_VERIFY_ERROR;
        }
    }
    return JR_STORE_OK;
}

static int payload_program(const jr_nor_t *nor, unsigned copy,
                           const jr_source_t *source, uint32_t expected_crc,
                           uint8_t *page)
{
    uint32_t off = 0, logical, within, sector, n, crc = UINT32_MAX;
    while (off < source->length) {
        logical = JR_STORE_HEADER_BYTES + off;
        within = logical % JR_STORE_SECTOR_BYTES;
        sector = logical / JR_STORE_SECTOR_BYTES;
        n = JR_STORE_PAGE_BYTES - (within % JR_STORE_PAGE_BYTES);
        if (n > source->length - off) n = source->length - off;
        if (source->read(source->ctx, off, page, n)) return JR_STORE_IO_ERROR;
        crc = crc_add(crc, page, n);
        if (nor->program(nor->ctx, sectors[copy][sector] + within, page, n))
            return JR_STORE_IO_ERROR;
        off += n;
    }
    return (crc ^ UINT32_MAX) == expected_crc ? JR_STORE_OK : JR_STORE_VERIFY_ERROR;
}

static void header_make(uint8_t *hdr, uint32_t length, uint32_t generation,
                        uint32_t payload_crc)
{
    memset(hdr, 0, JR_STORE_HEADER_BYTES);
    memcpy(hdr, "JBR1", 4);
    put16(hdr + 4, JR_STORE_SCHEMA);
    put16(hdr + 6, JR_STORE_KIND_RECOVERY);
    put16(hdr + 8, JR_STORE_HEADER_BYTES);
    put16(hdr + 10, JR_STORE_ALLOCATION_VERSION);
    put32(hdr + 12, length); put32(hdr + 16, generation);
    put32(hdr + 20, payload_crc);
    put32(hdr + 24, jr_store_allocation_crc());
    put32(hdr + JR_STORE_HEADER_CRC_OFFSET,
          crc_add(UINT32_MAX, hdr, JR_STORE_HEADER_CRC_OFFSET) ^ UINT32_MAX);
    put32(hdr + JR_STORE_COMMIT_OFFSET, JR_STORE_COMMIT_MARKER);
}

int jr_store_save(const jr_nor_t *nor, const jr_source_t *source,
                  jr_validate_fn validate, void *validation_ctx,
                  int initialize_claim, jr_store_state_t *out)
{
    uint8_t hdr[JR_STORE_HEADER_BYTES];
    jr_store_state_t state;
    jr_candidate_t verified;
    bounded_reader_t bounded;
    jr_source_t checked, written;
    jr_store_reader_t written_reader;
    uint32_t crc, read_crc, generation;
    unsigned target;
    int result;
    if (!out) return JR_STORE_INVALID_ARGUMENT;
    memset(out, 0, sizeof *out); out->winner = -1;
    if (!nor || !nor->read || !nor->erase_sector || !nor->program ||
        !source || !source->read || !validate) return JR_STORE_INVALID_ARGUMENT;
    if (!source->length || source->length > JR_STORE_PAYLOAD_MAX)
        return JR_STORE_INVALID_SOURCE;
    bounded.source = source; bounded.failed = 0;
    checked.ctx = &bounded; checked.read = bounded_read; checked.length = source->length;
    result = validate(validation_ctx, &checked);
    if (bounded.failed || result == JR_VALID_IO) return JR_STORE_IO_ERROR;
    if (result != JR_VALID_OK) return JR_STORE_INVALID_SOURCE;
    if (source_crc(&checked, &crc, hdr)) return JR_STORE_IO_ERROR;
    result = scan_with_scratch(nor, validate, validation_ctx, &state, hdr);
    *out = state;
    if (result == JR_STORE_IO_ERROR || result == JR_STORE_INVALID_ARGUMENT)
        return result;
    if (!state.writable) return JR_STORE_READ_ONLY;
    if (!state.allocation_claimed && !initialize_claim) return JR_STORE_SETUP_NEEDED;
    target = state.winner < 0 ? 0u : (unsigned)(1 - state.winner);
    generation = state.winner < 0 ? 1u : state.copies[state.winner].generation + 1u;
    result = erase_target(nor, target, hdr);
    if (result != JR_STORE_OK) return result;
    result = payload_program(nor, target, &checked, crc, hdr);
    if (result != JR_STORE_OK) return result;
    jr_store_source(&written_reader, nor, target, checked.length, &written);
    if (source_crc(&written, &read_crc, hdr)) return JR_STORE_IO_ERROR;
    if (read_crc != crc) return JR_STORE_VERIFY_ERROR;
    result = validate(validation_ctx, &written);
    if (written_reader.read_failed || result == JR_VALID_IO) return JR_STORE_IO_ERROR;
    if (result != JR_VALID_OK) return JR_STORE_VERIFY_ERROR;
    result = validate(validation_ctx, &checked);
    if (bounded.failed || result == JR_VALID_IO) return JR_STORE_IO_ERROR;
    if (result != JR_VALID_OK) return JR_STORE_VERIFY_ERROR;
    if (source_crc(&checked, &read_crc, hdr)) return JR_STORE_IO_ERROR;
    if (read_crc != crc) return JR_STORE_VERIFY_ERROR;
    header_make(hdr, checked.length, generation, crc);
    if (nor->program(nor->ctx, sectors[target][0], hdr, sizeof hdr))
        return JR_STORE_IO_ERROR;
    candidate_read(nor, target, validate, validation_ctx, &verified, hdr);
    if (verified.status == JR_CANDIDATE_IO_ERROR) return JR_STORE_IO_ERROR;
    if (verified.status != JR_CANDIDATE_VALID || verified.length != checked.length ||
        verified.generation != generation || verified.payload_crc != crc)
        return JR_STORE_VERIFY_ERROR;
    out->copies[target] = verified;
    out->winner = (int)target; out->allocation_claimed = 1; out->writable = 1;
    return JR_STORE_OK;
}
