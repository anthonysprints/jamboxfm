/* SPDX-License-Identifier: GPL-3.0-only */
/* JamBox identity is separate from the inherited FM-1 transport model marker.
 * 700 was not observed in the pinned research set; it is not a registered allocation.
 * Empty OTA command 0x12 queries this record without starting the instrument or an update.
 * Reply body (48 bytes): family[8], schema, mode, capabilities u16 LE,
 * release[20] and current transport identity[16], both ASCII and NUL-padded.
 * Use the envelope's exact length and checksum and all fields when classifying a device.
 */
#ifndef JAM_IDENTITY_H
#define JAM_IDENTITY_H
#define JAMBOX_NAME "JamBoxFM"
#define JAMBOX_FAMILY "JAMBOXFM"
#define JAMBOX_RELEASE "0.1.0-dev"
#define JAMBOX_APP_ID "FM-1_700"
#define JAMBOX_RESCUE_ID "FM-1_000"
#define JAMBOX_LOADER_ID "ota-FM-1_700"
#define JAMBOX_IDENTITY_COMMAND 0x12u
#define JAMBOX_IDENTITY_SCHEMA 1u
#define JAMBOX_IDENTITY_SIZE 48u
#define JAMBOX_MODE_APP 0u
#define JAMBOX_MODE_RESCUE 1u
#define JAMBOX_MODE_LOADER 2u
#define JAMBOX_CAP_UPDATER 1u
#define JAMBOX_CAP_TEMPORARY 2u
#define JAMBOX_CAP_APP_RESCUE 4u
#define JAMBOX_CAP_BOUNDED_LOADER 8u
#define JAMBOX_CAP_READONLY_CAL 16u
#define JAMBOX_CAPS_APP 0x17u
#define JAMBOX_CAPS_RESCUE 0x05u
#define JAMBOX_CAPS_LOADER 0x09u
/* Leave space for the required NUL in both variable-width wire fields. */
_Static_assert(sizeof JAMBOX_FAMILY == 9u, "JamBox family must contain exactly eight ASCII bytes");
_Static_assert(sizeof JAMBOX_RELEASE <= 20u, "JamBox release must fit its NUL-padded wire field");
_Static_assert(sizeof JAMBOX_APP_ID <= 16u && sizeof JAMBOX_RESCUE_ID <= 16u &&
               sizeof JAMBOX_LOADER_ID <= 16u, "JamBox transport identity must fit its wire field");
#endif
