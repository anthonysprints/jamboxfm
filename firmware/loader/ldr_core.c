/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca update loader, the portable part (step 2 of the M-UPGRADE update
 * protocol).
 *
 * The host serves the package ("logical image") with cmd 0x30 reads (ota.c).
 * The loader:
 *   1. reads the UFW header and finds flash.bin;
 *   2. checks that the package's app area decrypts with THIS chip's key
 *      (taken from isd_config.ini in the device's own flash head), so a
 *      package for another key is refused before anything is erased;
 *   3. writes ONLY the app area [0x4000, 0x93000), sector by sector, skipping
 *      sectors that are already equal, and verifies each one; the flash head
 *      (SPL, isd_config) is never written;
 *   4. checks the CRC16 the package stores for flash.bin against everything it
 *      was served (the head too, which it never writes; a mismatch: up to two
 *      more passes, rewriting what differs), asks 0xF0000000 ("success"), then
 *      invalidates the update record (RAM and flash) so the SPL boots the new
 *      app, and resets. A damaged package or no "success" leaves the flash
 *      record; retry can continue while the current loader remains reachable.
 * A flash record does NOT prove recovery after a power cycle. An upstream
 * hardware failure retained both a valid record and staged loader but did not
 * resume on cold boot (ChoralRoot docs/BOOT-SAFETY.md, pinned in our research).
 * Recovery before application code runs requires a separately verified path.
 *
 * Hooks from the platform (loader.c, tests/ldr_test.c):
 *   ldr_fread(off, p, n)  ldr_erase(off)  ldr_prog(off, p, n)   flash, 0 = ok
 *   ldr_record_clear()    forget the RAM update record
 *   ldr_flash_known()     the flash chip is the supported 1 MiB part (else no flash writes,
 *                         including record cleanup; preserve the records and refuse the update)
 *   ldr_progress(done, total)
 * plus the ota.c hooks (frames, time, idle). */
#define LDR_APP_LO 0x4000u
#define LDR_APP_HI 0x93000u
#if JAMBOX_PROTO
/* Two explicit staging contracts, never a scan of project/sample storage:
 * Sloop/JamBox uses E4F00; stock V15 uses E8F00. The latter requires the
 * additional record + staged-loader validation below before its sector may
 * be erased. See docs/build-evidence/factory-v15/stock-record-cleanup.md. */
#define LDR_REC_LO 0xE4000u
#define LDR_REC_HI 0xE5000u
#define LDR_STOCK_REC 0xE8F00u
#else
#define LDR_REC_LO 0x93000u                     /* update records live above the app ... */
#define LDR_REC_HI 0xFC000u                     /* ... and below Felucca's globals */
#endif

static int ldr_fread(uint32_t off, void *p, uint32_t n);
static int ldr_erase(uint32_t off);
static int ldr_prog(uint32_t off, const void *p, uint32_t n);
static void ldr_record_clear(void);
static int ldr_flash_known(void);
static void ldr_progress(uint32_t done, uint32_t total);

/* SFC cipher: per 32-byte block, key ^ (block offset >> 2) */
static void ldr_sfc(uint8_t *p, uint32_t n, uint32_t base_off, uint32_t key)
{
    uint32_t i, j;
    for (i = 0; i < n; i += 32u) {
        uint32_t k = key ^ ((base_off + i) >> 2);
        for (j = 0; j < 32u && i + j < n; j++) {
            p[i + j] ^= (uint8_t)k;
            k = ((k << 1) ^ (k & 0x8000u ? 0x1021u : 0u)) & 0xFFFFu;
        }
    }
}

/* chip key from the device's flash head: JLFS top entries (ENC 0xFFFF) -> isd_config.ini blob */
static int ldr_chip_key(uint32_t *key)
{
    uint8_t e[32], blob[34];
    uint32_t off, i, sum, k;
    for (off = 32; off < 0x400u; off += 32u) {
        if (ldr_fread(off, e, 32))
            return -1;
        ota_jl_enc(e, 32);
        if (ota_crc16(e + 2, 30, 0) != ota_rd16(e))
            return -2;
        if (!ota_memeq(e + 16, (const uint8_t *)"isd_config.ini", 15)) {
            if (ota_rd16(e + 14))
                return -3;                               /* last entry, not found */
            continue;
        }
        if (ldr_fread(ota_rd32(e + 4), blob, 34) || ota_crc16(blob, 32, 0) != ota_rd16(blob + 32))
            return -4;
        for (i = 0, sum = 0; i < 16u; i++)
            sum += blob[i];
        sum &= 0xFFu;
        sum = sum >= 0xE0u ? 0xAAu : sum <= 0x10u ? 0x55u : sum;
        for (i = 0, k = 0; i < 16u; i++)
            if ((uint32_t)(blob[16 + i] ^ blob[15 - i]) < sum)
                k |= 1u << i;
        *key = k;
        return 0;
    }
    return -3;
}

#define LDR_EBADPKG (-12)                       /* flash.bin as served does not match the package's CRC16 */
#define LDR_ENOFIN (-13)                        /* no "success" from the host */
#define LDR_EFLASH (-14)                        /* unsupported/missing flash: no writes, records preserved */
#define LDR_ERECORD (-15)                       /* resume-record cleanup could not be read/erased/verified */

#if JAMBOX_PROTO
/* Return 1 only for the known V15 handoff of our loader, 0 for no SPL-valid
 * record, or an error for a live record whose ownership cannot be established.
 * Stock writes 112 bytes but CRCs only the first 80. Its migration fields and
 * final 32 allocation bytes are not required to be zero. */
static int ldr_stock_record(void)
{
    uint8_t r[112], h[136];
    uint32_t addr, len, i, own = 0;
    if (ldr_fread(LDR_STOCK_REC, r, sizeof r))
        return LDR_ERECORD;
    if (ota_rd16(r + 6) != 0x5441u || !ota_rd16(r) || ota_rd16(r) != ota_crc16(r + 2, 78, 0))
        return 0;
    if (ota_rd16(r + 2) != 0x5A0Du || ota_rd16(r + 4) != 0x5A01u ||
        !ota_memeq(r + 8, (const uint8_t *)"ota-FM-1_015", 13))
        return LDR_ERECORD;
    addr = ota_rd32(r + 72);
    /* The stock allocator may move staging within VM space. Bound every read
     * above the app and below the record; do not assume one loader address. */
    if ((addr & 0xFFu) || addr < LDR_APP_HI || addr > LDR_STOCK_REC - sizeof h)
        return LDR_ERECORD;
    if (ldr_fread(addr, h, sizeof h))
        return LDR_ERECORD;
    len = ota_rd32(h + 8);
    if (len < sizeof h || len > 0x4F00u || len > LDR_STOCK_REC - addr ||
        ota_rd16(h) != ota_crc16(h + 2, 30, 0) || h[12] != 0x41u ||
        !ota_memeq(h + 16, (const uint8_t *)"LOADER.BIN", 11) || ota_rd32(h + 4) != addr)
        return LDR_ERECORD;
    /* Both V15 and our updater put addr at +4 and total length at +8.
     * Stock copies adjacent literal bytes after LOADER.BIN's NUL, so those
     * filename padding bytes are deliberately not compared. */
    if (ota_rd16(h + 32) != ota_crc16(h + 34, 30, 0) ||
        !ota_memeq(h + 48, (const uint8_t *)"usb_hid_ota.bin", 15) ||
        !ota_rd32(h + 36) || ota_rd32(h + 36) > 0x14000u || ota_rd32(h + 40) != 0x01C0A800u)
        return LDR_ERECORD;
    /* Same ownership marker in the first LZ4 literals as ota_stage(). This is
     * structural validation, not a second decompression/body-integrity test. */
    for (i = 0x48u; i + 16u <= sizeof h && !own; i++)
        own = ota_memeq(h + i, (const uint8_t *)"FELUCCA-LOADER-1", 16);
    return own ? 1 : LDR_ERECORD;
}
#endif

/* Clear flash records before clearing RAM state or permitting a success reset.
 * A failed cleanup stays in the current loader session for host retry. Startup's
 * separate RAM-record clearing policy is unchanged; this does not promise that
 * a RAM record was present when the session began. */
static int ldr_records_drop(void)
{
    uint32_t s, i;
#if JAMBOX_PROTO
    int stock = ldr_stock_record();
    if (stock < 0)
        return stock;
    if (stock) {
        uint8_t r[112];
        /* Do this first: a stock-staged loader header may itself be in E4000.
         * Never erase E4 and only then try to establish E8's ownership. */
        if (ldr_erase(0xE8000u) || ldr_fread(LDR_STOCK_REC, r, sizeof r))
            return LDR_ERECORD;
        for (i = 0; i < sizeof r; i++)
            if (r[i] != 0xFFu)
                return LDR_ERECORD;
    }
#endif
    for (s = LDR_REC_HI; s > LDR_REC_LO; s -= 0x1000u) {
        uint8_t r[80];
        if (ldr_fread(s - 0x100u, r, 80))
            return LDR_ERECORD;
        if (ota_rd16(r + 6) == 0x5441u && ota_rd16(r) && ota_rd16(r) == ota_crc16(r + 2, 78, 0)) {
            if (ldr_erase(s - 0x1000u) || ldr_fread(s - 0x100u, r, 80))
                return LDR_ERECORD;
            for (i = 0; i < sizeof r; i++)
                if (r[i] != 0xFFu)
                    return LDR_ERECORD;
        }
    }
    ldr_record_clear();
    return 0;
}

static int ldr_pass(void)
{
    static uint8_t hdr[0x400], sec[0x1000], cur[0x1000];
    uint32_t i, k, fl_off, fl_size, ota_off, ota_len, key, s, n, want, crc = 0, done = 0;
    int rc;
    /* 1. UFW header + entry list (ota.c) */
    if ((rc = ota_ufw(hdr, &fl_off, &fl_size, &ota_off, &ota_len, &want)) != 0)
        return rc;
    if (!fl_off || fl_size < LDR_APP_HI)
        return -3;
    /* 2. the package's app area must decrypt with this chip's key */
    if ((rc = ldr_chip_key(&key)) != 0)
        return -40 + rc;
    if (ota_read(fl_off + LDR_APP_LO, sec, 32))
        return -5;
    ldr_sfc(sec, 32, 0, key);
    if (ota_crc16(sec + 2, 30, 0) != ota_rd16(sec))
        return -6;                                       /* package for another chip key */
    if (!ldr_flash_known())                              /* never erase even a valid record on an unknown chip */
        return LDR_EFLASH;
#if JAMBOX_PROTO
    /* Discover an unreadable or unowned live stock record before changing the
     * app. Cleanup revalidates it after the complete transfer and host finish. */
    if ((rc = ldr_stock_record()) < 0)
        return rc;
#endif
    /* the head of flash.bin is never written, but it is part of the package's CRC */
    for (s = 0; s < LDR_APP_LO; s += 512u) {
        if (ota_read(fl_off + s, sec, 512))
            return -7;
        crc = ota_crc16(sec, 512, crc);
    }
    /* 3. app area, sector by sector */
    for (s = LDR_APP_LO; s < LDR_APP_HI; s += 0x1000u) {
        for (k = 0; k < 0x1000u; k += 512u)
            if (ota_read(fl_off + s + k, sec + k, 512))
                return -7;
        crc = ota_crc16(sec, 0x1000u, crc);
        if (ldr_fread(s, cur, 0x1000u))
            return -8;
        if (!ota_memeq(sec, cur, 0x1000u)) {
            uint32_t tries;
            for (tries = 0; tries < 2u; tries++) {
                if (ldr_erase(s))
                    return -9;
                for (k = 0; k < 0x1000u; k += 256u)
                    if (ldr_prog(s + k, sec + k, 256))
                        return -10;
                if (ldr_fread(s, cur, 0x1000u))
                    return -8;
                if (ota_memeq(sec, cur, 0x1000u))
                    break;
            }
            if (tries == 2u)
                return -11;                              /* verify failed twice */
        }
        done += 0x1000u;
        ldr_progress(done, LDR_APP_HI - LDR_APP_LO);
    }
    for (s = LDR_APP_HI; s < fl_size; s += n) {          /* (a flash.bin longer than the app area) */
        n = fl_size - s < 512u ? fl_size - s : 512u;
        if (ota_read(fl_off + s, sec, n))
            return -7;
        crc = ota_crc16(sec, n, crc);
    }
    /* 4. finish: the whole package arrived as it was built, the host confirms, the record goes,
     *    the SPL boots the app */
    if (crc != want)
        return LDR_EBADPKG;
    for (i = 0; i < 4u; i++)
        if (!ota_read(0xF0000000u, sec, 8) && ota_memeq(sec, (const uint8_t *)"success", 8))
            break;
    if (i == 4u)
        return LDR_ENOFIN;
    return ldr_records_drop();
}

/* a pass that does not add up (a byte damaged on the way) is run again: the host answers any read, so it
 * just serves the image once more and the sectors that differ are rewritten; a package that is damaged
 * itself fails every pass and the record stays */
static int ldr_session(void)
{
    int rc = LDR_EBADPKG;
    uint32_t pass;
    for (pass = 0; pass < 3u && rc == LDR_EBADPKG; pass++)
        rc = ldr_pass();
    return rc;
}
