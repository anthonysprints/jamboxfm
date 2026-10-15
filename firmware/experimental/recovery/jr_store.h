/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef JR_STORE_H
#define JR_STORE_H

/* Host-only experimental recovery store. No physical flash hooks are linked. */
#include <stdint.h>
#include "jr_io.h"

#define JR_STORE_SECTOR_BYTES 4096u
#define JR_STORE_PAGE_BYTES 256u
#define JR_STORE_SECTORS 7u
#define JR_STORE_HEADER_BYTES 256u
#define JR_STORE_PAYLOAD_MAX 28416u
#define JR_STORE_SCHEMA 1u
#define JR_STORE_KIND_RECOVERY 1u
#define JR_STORE_ALLOCATION_VERSION 1u
#define JR_STORE_HEADER_CRC_OFFSET 248u
#define JR_STORE_COMMIT_OFFSET 252u
#define JR_STORE_COMMIT_MARKER 0x31434d4au

/* JBR1 commit header: all integer fields are little endian. Bytes 28..247
 * are zero. Header CRC32 covers bytes 0..247; payload CRC covers exactly
 * length bytes. The final four bytes establish the commit. Allocation CRC32
 * covers the ordered fourteen physical sector addresses, each encoded LE32.
 * No packed C object is used as a wire representation. */
#define JR_STORE_SCHEMA_OFFSET 4u
#define JR_STORE_KIND_OFFSET 6u
#define JR_STORE_HEADER_LENGTH_OFFSET 8u
#define JR_STORE_ALLOCATION_OFFSET 10u
#define JR_STORE_LENGTH_OFFSET 12u
#define JR_STORE_GENERATION_OFFSET 16u
#define JR_STORE_PAYLOAD_CRC_OFFSET 20u
#define JR_STORE_ALLOCATION_CRC_OFFSET 24u

/* The callbacks must return zero only for a completely successful operation.
 * program() receives at most one aligned-page fragment; erase_sector() always
 * receives an exact sector from the declared allocation. */
typedef struct jr_nor {
    void *ctx;
    jr_read_fn read;
    int (*erase_sector)(void *ctx, uint32_t address);
    int (*program)(void *ctx, uint32_t address,
                   const void *bytes, uint32_t length);
} jr_nor_t;

enum jr_candidate_status {
    JR_CANDIDATE_EMPTY = 0,
    JR_CANDIDATE_LEGACY = 1,
    JR_CANDIDATE_INVALID = 2,
    JR_CANDIDATE_VALID = 3,
    JR_CANDIDATE_PROTECTED = 4,
    JR_CANDIDATE_IO_ERROR = 5
};

typedef struct jr_candidate {
    enum jr_candidate_status status;
    uint32_t generation;
    uint32_t length;
    uint32_t payload_crc;
} jr_candidate_t;

typedef struct jr_store_state {
    jr_candidate_t copies[2];
    int winner;                 /* -1, 0 (A), or 1 (B). */
    int writable;              /* False on uncertainty, unsupported data/ties. */
    int allocation_claimed;    /* A fully validated record claims both copies. */
} jr_store_state_t;

enum jr_store_result {
    JR_STORE_OK = 0,
    JR_STORE_SETUP_NEEDED = 1,
    JR_STORE_READ_ONLY = 2,
    JR_STORE_INVALID_SOURCE = 3,
    JR_STORE_IO_ERROR = 4,
    JR_STORE_VERIFY_ERROR = 5,
    JR_STORE_INVALID_ARGUMENT = 6
};

/* Exposed for independent NOR-map/fault tests; UINT32_MAX signals a bad index. */
uint32_t jr_store_sector_address(unsigned copy, unsigned sector);
uint32_t jr_store_allocation_crc(void);

/* No erase or program. Both complete records are checked before selection.
 * A returned old winner may still be read-only because the other copy is
 * unknown, unsupported, unreadable, or ambiguous. */
int jr_store_scan(const jr_nor_t *nor, jr_validate_fn validate,
                  void *validation_ctx, jr_store_state_t *out);

/* Explicit initialize_claim authorizes retiring the declared legacy allocation
 * only when no verified claim exists. It never bypasses protected records.
 * Source must remain coherent throughout this synchronous operation. Semantic
 * validation happens before any erase, after streaming, and on readback.
 * There is no device integration, runtime revision guard, or initialization UI
 * in this host prototype. Callers must freeze source mutations themselves.
 * On any error the saved revision must not advance, even if a commit header
 * reached NOR before its readback failed. */
int jr_store_save(const jr_nor_t *nor, const jr_source_t *source,
                  jr_validate_fn validate, void *validation_ctx,
                  int initialize_claim, jr_store_state_t *out);

/* A bounded read view over a valid candidate. Revalidate after constructing
 * this view if bytes can change; scan results are not a snapshot of NOR. */
typedef struct jr_store_reader {
    const jr_nor_t *nor;
    unsigned copy;
    uint32_t length;
    int read_failed;
} jr_store_reader_t;
int jr_store_read(void *ctx, uint32_t offset, void *dst, uint32_t length);
void jr_store_source(jr_store_reader_t *reader, const jr_nor_t *nor,
                     unsigned copy, uint32_t length, jr_source_t *out);

#endif
