/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef JR_IO_H
#define JR_IO_H
#include <stdint.h>
/* All readers return zero for an exact read; nonzero is an I/O failure. */
typedef int (*jr_read_fn)(void *ctx, uint32_t offset, void *dst, uint32_t bytes);
typedef struct jr_source {
    void *ctx;
    jr_read_fn read;
    uint32_t length;
} jr_source_t;
enum { JR_VALID_IO = -1, JR_VALID_OK = 0, JR_VALID_INVALID = 1, JR_VALID_UNSUPPORTED = 2 };
typedef int (*jr_validate_fn)(void *validation_ctx, const jr_source_t *source);
#endif
