/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#if JAMBOX_PROTO && JAMBOX_RECOVERY
static void jam_recovery_boot(void);
static int jam_recovery_save(int initialize);
static int jam_recovery_can_save(void);
static int jam_recovery_is_claimed(void);
static const char *jam_recovery_status(void);
static const char *jam_recovery_block_reason(void);
static void jam_recovery_mark_dirty(void);
#endif
