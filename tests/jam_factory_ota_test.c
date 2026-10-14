/* SPDX-License-Identifier: GPL-3.0-only */
/* Real JamBox ota.c, exact factory V15 package, simulated MIDI and NOR only.
 * This tests JamBox's first stage. The official loader, SPL, USB hardware,
 * and reset/power-loss behavior do not execute here. Run via the hash-gated
 * jam_factory_ota_run.sh, not against arbitrary firmware files. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JAMBOX_PROTO 1
#define FELUCCA_FLASH 1
#define FELUCCA_OTA 1
#define FELUCCA_PERSIST 0
#define FELUCCA_OTA_DRYRUN 0
#include "../firmware/src/app_policy.h"
#include "../firmware/src/jam_identity.h"
#define FELUCCA_ID JAMBOX_APP_ID
static int rescue;
#define OTA_IDENTITY (rescue ? JAMBOX_RESCUE_ID : JAMBOX_APP_ID)
#define OTA_JAMBOX_MODE (rescue ? JAMBOX_MODE_RESCUE : JAMBOX_MODE_APP)

static uint8_t nor[0x100000], before[sizeof nor], rx[640];
static uint8_t *logical;
static size_t logical_len;
static uint32_t flash_off, loader_off, loader_len;
static uint32_t rx_len, now_ms, requests, head_requests, body_requests, tail_requests;
static uint32_t forbidden_writes, writes, erase_calls, confirm_requests, head_seen, premature_writes;
static uint32_t mute_addr, corrupt_addr, fail_flash_read;
static uint8_t committed_parm[112], identity[27], family[JAMBOX_IDENTITY_SIZE];
static int rx_full, committed, last_code;

static void require(int ok, const char *why)
{
    if (!ok) { fprintf(stderr, "Fixture/host failure: %s\n", why); exit(2); }
}
static uint32_t read32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static uint32_t pack7(const uint8_t *p, uint32_t n, uint8_t *out)
{
    uint32_t acc = 0, bits = 0, used = 0;
    while (n--) {
        acc |= (uint32_t)*p++ << bits; bits += 8;
        while (bits >= 7) { out[used++] = acc & 0x7f; acc >>= 7; bits -= 7; }
    }
    if (bits) out[used++] = acc & 0x7f;
    return used;
}
static uint32_t unpack7(const uint8_t *p, uint32_t n, uint8_t *out, uint32_t cap)
{
    uint32_t acc = 0, bits = 0, used = 0;
    while (n--) {
        require(!(*p & 0x80), "non-seven-bit device frame");
        acc |= (uint32_t)*p++ << bits; bits += 7;
        if (bits >= 8) {
            require(used < cap, "oversized device frame");
            out[used++] = (uint8_t)acc; acc >>= 8; bits -= 8;
        }
    }
    require(!acc, "noncanonical device frame padding");
    return used;
}
static void host_reply(const uint8_t *p, uint32_t n)
{
    require(!rx_full && ((n * 8 + 6) / 7) <= sizeof rx, "receive queue overflow");
    rx_len = pack7(p, n, rx); rx_full = 1;
}
static int ota_wire_send(const uint8_t *p, uint32_t n)
{
    uint8_t u[64], m[527];
    uint32_t d, addr, len, sum = 0;
    require(n >= 2 && p[0] == 0xf0 && p[n - 1] == 0xf7, "device framing");
    d = unpack7(p + 1, n - 2, u, sizeof u);
    require(d >= 7 && !u[0] && u[1] == 0x59, "device envelope");
    require((uint32_t)u[3] + ((uint32_t)u[4] << 8) + ((uint32_t)u[5] << 16) == d - 7,
            "device envelope length");
    for (uint32_t i = 6; i + 1 < d; ++i) sum += u[i];
    require((uint8_t)~sum == u[d - 1], "device checksum");
    if (u[2] == 0x11) {
        require(d == 34, "identity length"); memcpy(identity, u + 6, sizeof identity); return 0;
    }
    if (u[2] == JAMBOX_IDENTITY_COMMAND) {
        require(d == 55, "family length"); memcpy(family, u + 6, sizeof family); return 0;
    }
    require(d == 15 && u[2] == 0x30 && !u[6] && !u[13], "device read request");
    addr = read32(u + 7); len = (uint32_t)u[11] | (uint32_t)u[12] << 8;
    require(len && len <= 512, "read request size");
    requests++;
    if (addr >= flash_off && addr < flash_off + 0x4000) {
        require(len == 512 && !((addr - flash_off) % 512), "complete aligned head blocks");
        head_requests++; head_seen |= 1u << ((addr - flash_off) / 512);
    }
    if (addr >= loader_off + 32 && addr < loader_off + 32 + loader_len) {
        body_requests++;
        if (addr == loader_off + 32 + loader_len - 481 && len == 481) tail_requests++;
    }
    if (addr == 0xe0000000u) confirm_requests++;
    if (addr == mute_addr) return 0;
    memset(m, 0, sizeof m);
    m[1] = 0x59; m[2] = 0x30;
    m[3] = (uint8_t)(len + 8); m[4] = (uint8_t)((len + 8) >> 8);
    memcpy(m + 7, u + 7, 7);
    if (addr == 0xe0000000u) {
        require(len == 8, "confirmation size"); memcpy(m + 14, "success", 8);
    } else {
        require(addr <= logical_len && len <= logical_len - addr, "read outside package");
        memcpy(m + 14, logical + addr, len);
        if (corrupt_addr >= addr && corrupt_addr - addr < len) m[14 + corrupt_addr - addr] ^= 1;
    }
    sum = 0;
    for (uint32_t i = 6; i < 14 + len; ++i) sum += m[i];
    m[14 + len] = (uint8_t)~sum; host_reply(m, 15 + len);
    return 0;
}
static int ota_frame_get(const uint8_t **p, uint32_t *n)
{
    if (!rx_full) return 0;
    *p = rx; *n = rx_len; return 1;
}
static void ota_frame_done(void) { rx_full = 0; }
static uint32_t ota_now_ms(void) { return now_ms; }
static void ota_idle(void) { now_ms++; }
static int ota_erase(uint32_t off)
{
    if (off < 0xe0000 || off >= 0xe5000 || off % 4096) { forbidden_writes++; return -1; }
    erase_calls++; memset(nor + off, 0xff, 4096); return 0;
}
static int ota_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *p = src;
    if (off < 0xe0000 || off > 0xe5000 || n > 0xe5000 - off) { forbidden_writes++; return -1; }
    if (head_seen != UINT32_MAX) premature_writes++;
    writes++;
    for (uint32_t i = 0; i < n; ++i) nor[off + i] &= p[i];
    return 0;
}
static int ota_fread(uint32_t off, void *dst, uint32_t n)
{
    require(off <= sizeof nor && n <= sizeof nor - off, "NOR read bounds");
    if (off == fail_flash_read) return -1;
    memcpy(dst, nor + off, n); return 0;
}
static void ota_show(uint32_t step, int32_t code) { if (step == 9) last_code = code; }
static void ota_commit(const uint8_t *parm)
{
    committed++; memcpy(committed_parm, parm, sizeof committed_parm);
}
#include "../firmware/src/ota.c"

static int check(const char *what, int ok)
{
    printf("%s: %-66s %s\n", rescue ? "rescue" : "normal", what, ok ? "ok" : "FAIL");
    return !ok;
}
static void reset_dev(void)
{
    /* JamBox app-only install preserves the actual factory 16 KiB head.
     * Distinct pattern elsewhere detects writes to app, calibration or user data. */
    for (uint32_t i = 0; i < sizeof nor; ++i) nor[i] = (uint8_t)(i * 13u + (i >> 9) + 27u);
    memcpy(nor, logical + flash_off, 0x4000);
    memset(nor + OTA_AREA, 0xff, OTA_AREA_LEN);
    memcpy(before, nor, sizeof nor);
    rx_full = committed = last_code = 0;
    now_ms = requests = head_requests = body_requests = tail_requests = 0;
    forbidden_writes = writes = erase_calls = confirm_requests = head_seen = premature_writes = 0;
    mute_addr = corrupt_addr = fail_flash_read = UINT32_MAX;
    memset(identity, 0, sizeof identity); memset(family, 0, sizeof family);
}
static int protected_equal(void)
{
    return !forbidden_writes && !premature_writes && !memcmp(nor, before, OTA_AREA) &&
        !memcmp(nor + OTA_AREA + OTA_AREA_LEN, before + OTA_AREA + OTA_AREA_LEN,
                sizeof nor - OTA_AREA - OTA_AREA_LEN);
}
static int erased(uint32_t off, uint32_t n)
{
    while (n--) if (nor[off++] != 0xff) return 0;
    return 1;
}
static int refused(int expected, int rc)
{
    return rc == expected && last_code == expected && !committed &&
        protected_equal() && erased(OTA_AREA, OTA_AREA_LEN);
}
static int run_mode(void)
{
    int bad = 0, rc;
    reset_dev();
    for (uint32_t cmd = 0x11; cmd <= 0x12; ++cmd) {
        uint8_t q[7] = {0, 0x59, (uint8_t)cmd, 0, 0, 0, 0xff};
        host_reply(q, sizeof q); ota_service();
    }
    bad += check("actual JamBox normal/rescue identity and capability response", !strcmp((char *)identity, OTA_IDENTITY) &&
            !memcmp(family, JAMBOX_FAMILY, 8) && family[9] == OTA_JAMBOX_MODE &&
            family[10] == (rescue ? JAMBOX_CAPS_RESCUE : JAMBOX_CAPS_APP) &&
            !strcmp((char *)family + 32, OTA_IDENTITY));
    rc = ota_session();
    bad += check("exact factory V15 loader stages and commits once", rc == 0 && committed == 1);
    bad += check("all 32 factory-head blocks checked before staging",
            head_requests == 32 && head_seen == UINT32_MAX && !premature_writes);
    bad += check("all 19,937 loader bytes staged, including final 481-byte block",
            body_requests == 39 && tail_requests == 1 &&
            !memcmp(nor + OTA_AREA + 32, logical + loader_off + 32, loader_len));
    bad += check("staged loader CRC matches pinned official 0x5881", ota_crc16(nor + OTA_AREA + 32, loader_len, 0) == 0x5881);
    bad += check("LOADER.BIN header CRC, address and full length", ota_rd16(nor + OTA_AREA) == ota_crc16(nor + OTA_AREA + 2, 30, 0) &&
            ota_rd32(nor + OTA_AREA + 4) == OTA_AREA && ota_rd32(nor + OTA_AREA + 8) == loader_len + 32 &&
            !memcmp(nor + OTA_AREA + 16, "LOADER.BIN", 10));
    bad += check("resume record equals RAM record and retains SPL compatibility", !memcmp(nor + OTA_RES, committed_parm, 112) &&
            ota_rd16(committed_parm) == ota_crc16(committed_parm + 2, 78, 0) &&
            ota_rd16(committed_parm + 2) == 0x5a0d && ota_rd16(committed_parm + 6) == 0x5441 &&
            !memcmp(committed_parm + 8, "ota-FM-1_015", 12) && ota_rd32(committed_parm + 72) == OTA_AREA);
    bad += check("staging padding intact; only loader, header and resume written",
            erase_calls == 5 && writes == 41 && confirm_requests == 1 &&
            erased(OTA_AREA + 32 + loader_len, OTA_RES - OTA_AREA - 32 - loader_len) &&
            erased(OTA_RES + 112, OTA_AREA + OTA_AREA_LEN - OTA_RES - 112));
    bad += check("entire head, app, calibration and user regions unchanged", protected_equal());
    bad += check("boot cleanup confines erases to staging and is idempotent", ota_boot_cleanup() == 1 &&
            erased(OTA_AREA, OTA_AREA_LEN) && protected_equal() && ota_boot_cleanup() == 0);

    for (uint32_t mismatch = 0; mismatch <= 0x3fffu; mismatch += 0x3fffu) {
        reset_dev(); nor[mismatch] ^= 1; memcpy(before, nor, sizeof nor);
        rc = ota_session();
        bad += check(mismatch ? "last factory-head byte differs: refuse before programming" :
                "first factory-head byte differs: refuse before programming",
                refused(-5, rc) && !writes && !body_requests && !confirm_requests &&
                head_requests == (mismatch ? 32u : 1u));
    }
    reset_dev(); fail_flash_read = 0x3e00; rc = ota_session();
    bad += check("failed installed-head read: refuse before programming", refused(-4, rc) && !writes);
    reset_dev(); mute_addr = loader_off + 32 + 1024; rc = ota_session();
    bad += check("host disappears after partial staging: abort and erase", refused(-9, rc) && writes == 2);
    reset_dev(); corrupt_addr = loader_off + 32 + loader_len - 1; rc = ota_session();
    bad += check("corrupt final loader byte: CRC failure cleans staging", refused(-11, rc) && tail_requests == 1);
    reset_dev(); fail_flash_read = OTA_AREA + 32; rc = ota_session();
    bad += check("staged readback failure: no resume record or commit", refused(-11, rc) && writes == 39);
    reset_dev(); mute_addr = 0xe0000000u; rc = ota_session();
    bad += check("missing host confirmation: no commit, all staging erased", refused(-13, rc) && writes == 40 && confirm_requests > 1);
    return bad;
}
int main(int argc, char **argv)
{
    uint8_t *raw, h[0x400];
    FILE *f;
    int bad = 0;
    require(argc == 2, "use jam_factory_ota_run.sh with the pinned factory V15 file");
    f = fopen(argv[1], "rb"); require(f != NULL, "open factory fixture");
    raw = malloc(699956); logical_len = 699956 - 20; logical = malloc(logical_len);
    require(raw && logical, "allocate package");
    require(fread(raw, 1, 699956, f) == 699956 && fgetc(f) == EOF, "exact package length"); fclose(f);
    for (size_t i = 0; i < 20; ++i) memcpy(logical + i * 47, raw + i * 48, 47);
    memcpy(logical + 20 * 47, raw + 20 * 48, 699956 - 20 * 48); free(raw);
    memcpy(h, logical, sizeof h); ota_jl_enc(h, 0x40);
    uint32_t entries = ota_rd16(h + 8);
    require(entries && entries <= 11, "entry count");
    require(ota_rd16(h) == ota_crc16(h + 2, 0x3e, 0) &&
            ota_rd16(h + 2) == ota_crc16(h + 0x40, entries * 0x50, 0), "UFW header CRCs");
    for (uint32_t i = 0; i < entries; ++i) {
        uint8_t *e = h + 0x40 + i * 0x50; ota_jl_enc(e, 0x50);
        uint32_t off = ota_rd32(e + 8), n = ota_rd32(e + 12);
        require(off <= logical_len && n <= logical_len - off, "entry bounds");
        /* Stage one consumes only flash.bin and ota.bin. Other stock entries
         * contain encoded data whose descriptors do not CRC the raw bytes. */
        if (ota_rd16(e) == 0 || ota_rd16(e) == 100)
            require(ota_rd16(e + 4) == ota_crc16(logical + off, n, 0), "flash/loader payload CRC");
        if (ota_rd16(e) == 0) { flash_off = off; require(n >= 0x4000, "factory head exists"); }
        if (ota_rd16(e) == 100) { loader_off = off; require(n >= 32, "loader header exists"); loader_len = n - 32; }
    }
    require(flash_off && loader_off, "factory flash and loader present");
    require(OTA_LDR_HCRC == 0xebaa && OTA_LDR_DCRC == 0x5881 && OTA_LDR_LEN == 0x4de1,
            "production official-loader constants match pinned fixture expectations");
    require(ota_rd16(logical + loader_off) == 0xebaa && ota_rd16(logical + loader_off + 2) == 0x5881 &&
            ota_rd32(logical + loader_off + 8) == 0x4de1 && loader_len == 0x4de1 &&
            ota_crc16(logical + loader_off + 2, 30, 0) == 0xebaa &&
            ota_crc16(logical + loader_off + 32, loader_len, 0) == 0x5881,
            "actual official loader length and complete header/data CRCs");
    for (rescue = 0; rescue < 2; ++rescue) bad += run_mode();
    free(logical);
    puts(bad ? "Factory V15 JamBox stage-one tests FAILED" :
            "Factory V15 JamBox stage-one: 34 checks passed; stock loader execution is not tested");
    return bad != 0;
}
