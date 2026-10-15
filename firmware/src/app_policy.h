/* SPDX-License-Identifier: GPL-3.0-only */
/* Application data persistence is independent of the firmware updater. */
#pragma once
#ifndef JAMBOX_RECOVERY
#define JAMBOX_RECOVERY 0
#endif
#ifndef FELUCCA_PERSIST
#if JAMBOX_PROTO
#define FELUCCA_PERSIST 0
#elif defined(FELUCCA_FLASH)
#define FELUCCA_PERSIST FELUCCA_FLASH
#else
#define FELUCCA_PERSIST 0
#endif
#endif
#if FELUCCA_PERSIST && !FELUCCA_FLASH
#error "FELUCCA_PERSIST requires FELUCCA_FLASH"
#endif
#if FELUCCA_OTA && !FELUCCA_FLASH
#error "FELUCCA_OTA requires FELUCCA_FLASH"
#endif
#if JAMBOX_PROTO && FELUCCA_PERSIST
#error "JamBox temporary sessions require FELUCCA_PERSIST=0"
#endif
#if JAMBOX_PROTO && FELUCCA_OTA && (FELUCCA_OTA_DRYRUN || FELUCCA_OTA_RAMONLY)
#error "JamBox updater requires real writes and a persistent resume record"
#endif
#if JAMBOX_RECOVERY && (!JAMBOX_PROTO || !FELUCCA_FLASH || FELUCCA_PERSIST || !FELUCCA_OTA)
#error "JAMBOX_RECOVERY requires JAMBOX_PROTO=1, FELUCCA_FLASH=1, FELUCCA_PERSIST=0 and FELUCCA_OTA=1"
#endif
/* This override reaches the RAM driver's final erase/program checks, even if
 * a future app writer accidentally bypasses the higher-level storage gates.
 * The separate loader compilation supplies its own app-programming window. */
#if JAMBOX_PROTO && !FELUCCA_PERSIST
#if JAMBOX_RECOVERY
/* Only the synchronous recovery adapter may authorize recovery writes. Keep
 * this RAM byte visible to the final RAM-driver guards as well as callbacks.
 * The updater window is independent and retains its existing permissions. */
static unsigned char jr_transaction_active;
#define FL_RANGE_OK(off, n) (FL_IN(off, n, FL_OTA_LO, FL_OTA_HI) || \
                            (jr_transaction_active && FL_RECOVERY_OK(off, n)))
#else
#define FL_RANGE_OK(off, n) FL_IN(off, n, FL_OTA_LO, FL_OTA_HI)
#endif
#endif
