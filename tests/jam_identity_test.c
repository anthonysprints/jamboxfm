/* SPDX-License-Identifier: GPL-3.0-only */
/* Real ota.c before instrument initialization, with queued SysEx and inert
 * flash hooks. Compile TEST_MODE=0 (Sloop), 1 (app), 2 (rescue), 3 (loader). */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define JAMBOX_PROTO (TEST_MODE != 0)
#define FELUCCA_OTA_DRYRUN 0
#if TEST_MODE == 3
#define FELUCCA_LOADER 1
#define FELUCCA_ID "ota-FM-1_700"
#elif TEST_MODE != 0
#define FELUCCA_ID "FM-1_700"
static uint8_t recovery_active = TEST_MODE == 2;
#define OTA_IDENTITY (recovery_active ? "FM-1_000" : FELUCCA_ID)
#define OTA_JAMBOX_MODE (recovery_active ? 1u : 0u)
#else
#define FELUCCA_ID "FM-1_900"
#endif

typedef struct { uint32_t n; uint8_t b[1024]; } frame_t;
static frame_t rx[16], tx[32];
static uint32_t rx_r, rx_w, tx_n, writes, now_ms, interleave, read_requests;

static uint32_t pack(const uint8_t *p, uint32_t n, uint8_t *b)
{
    uint32_t acc = 0, bits = 0, out = 0;
    while (n--) {
        acc |= (uint32_t)*p++ << bits;
        bits += 8u;
        while (bits >= 7u) { b[out++] = acc & 127u; bits -= 7u; acc >>= 7; }
    }
    if (bits) b[out++] = acc;
    return out;
}

static uint32_t unpack(const uint8_t *p, uint32_t n, uint8_t *b)
{
    uint32_t acc = 0, bits = 0, out = 0;
    while (n--) {
        assert(*p < 128u);
        acc |= (uint32_t)*p++ << bits;
        bits += 7u;
        if (bits >= 8u) { b[out++] = acc; bits -= 8u; acc >>= 8; }
    }
    assert(!acc);
    return out;
}

static void enqueue_raw(const uint8_t *raw, uint32_t n)
{
    assert(rx_w < 16u && n <= 700u);
    rx[rx_w].n = pack(raw, n, rx[rx_w].b);
    rx_w++;
}

static void enqueue(uint8_t cmd, const uint8_t *body, uint32_t n)
{
    uint8_t raw[640] = {0, 0x59};
    uint32_t i, sum = 0;
    assert(n + 7u <= sizeof raw);
    raw[2] = cmd;
    raw[3] = n; raw[4] = n >> 8; raw[5] = n >> 16;
    for (i = 0; i < n; i++) { raw[6u + i] = body[i]; sum += body[i]; }
    raw[6u + n] = (uint8_t)~sum;
    enqueue_raw(raw, n + 7u);
}

static int ota_wire_send(const uint8_t *p, uint32_t n)
{
    uint8_t body[520];
    uint32_t i, len, sum = 0;
    frame_t *f;
    assert(tx_n < 32u && n >= 2u && n < 100u && p[0] == 0xF0 && p[n - 1u] == 0xF7);
    f = &tx[tx_n++];
    f->n = unpack(p + 1, n - 2u, f->b);
    assert(f->n >= 7u && f->b[0] == 0 && f->b[1] == 0x59);
    assert(f->n == 7u + f->b[3] + ((uint32_t)f->b[4] << 8) + ((uint32_t)f->b[5] << 16));
    for (i = 6u; i + 1u < f->n; i++) sum += f->b[i];
    assert(f->b[f->n - 1u] == (uint8_t)~sum);
    if (f->b[2] != 0x30) return 0;
    read_requests++;
    if (!interleave) return 0;
    assert(f->n == 15u);
    len = f->b[11] + ((uint32_t)f->b[12] << 8);
    enqueue(0x11, NULL, 0);
    enqueue(0x12, NULL, 0);
    enqueue(0x12, (const uint8_t *)"x", 1); /* a nonempty probe never starts work */
    memcpy(body, f->b + 6, 8);
    for (i = 0; i < len; i++) body[8u + i] = (uint8_t)(i * 23u + 137u);
    enqueue(0x30, body, 8u + len);
    return 0;
}

static int ota_frame_get(const uint8_t **p, uint32_t *n)
{
    if (rx_r == rx_w) return 0;
    *p = rx[rx_r].b; *n = rx[rx_r].n;
    return 1;
}
static void ota_frame_done(void) { assert(rx_r < rx_w); rx_r++; }
static uint32_t ota_now_ms(void) { return now_ms; }
static void ota_idle(void) { now_ms++; }
static int ota_erase(uint32_t off) { (void)off; writes++; return -1; }
static int ota_prog(uint32_t off, const void *p, uint32_t n) { (void)off; (void)p; (void)n; writes++; return -1; }
static int ota_fread(uint32_t off, void *p, uint32_t n) { (void)off; (void)p; (void)n; writes++; return -1; }
static void ota_show(uint32_t step, int32_t code) { (void)step; (void)code; writes++; }
static void ota_commit(const uint8_t *p) { (void)p; writes++; }
#include "../firmware/src/ota.c"

static void reset(void)
{
    rx_r = rx_w = tx_n = now_ms = interleave = read_requests = 0;
    ota_deadline = 0;
}

static void expect_family(const frame_t *f)
{
    uint8_t expected[48] = {'J', 'A', 'M', 'B', 'O', 'X', 'F', 'M', 1};
    const char *id = TEST_MODE == 3 ? "ota-FM-1_700" : TEST_MODE == 2 ? "FM-1_000" : "FM-1_700";
    expected[9] = TEST_MODE == 3 ? 2 : TEST_MODE == 2 ? 1 : 0;
    expected[10] = TEST_MODE == 3 ? 0x09 : TEST_MODE == 2 ? 0x05 : 0x17;
    memcpy(expected + 12, "0.1.0-dev", 9);
    memcpy(expected + 32, id, strlen(id));
    assert(f->n == 55u && f->b[2] == 0x12 && !memcmp(f->b + 6, expected, sizeof expected));
}

static void reject_queries(void)
{
    uint32_t i;
    uint8_t raw[8] = {0, 0x59, 0x12, 0, 0, 0, 255, 0};
    for (i = 0; i < 9u; i++) {
        reset();
        enqueue_raw(raw, 7);
        switch (i) {
        case 0: rx[0].b[0] = 1; break;                   /* envelope magic */
        case 1: rx[0].b[3] ^= 1; break;                  /* declared length */
        case 2: rx[0].b[7] ^= 1; break;                  /* checksum */
        case 3: rx[0].n--; break;                        /* truncation */
        case 4: rx[0].b[rx[0].n++] = 0; break;           /* extra pack7 group */
        case 5: rx[0].b[0] |= 128u; break;               /* MIDI status bit */
        case 6: rx_w = 0; enqueue(0x12, (const uint8_t *)"x", 1); break;
        case 7: rx_w = 0; enqueue(0x13, NULL, 0); break;  /* unsupported command */
        case 8: rx[0].n = 0; break;
        }
        ota_service();
        assert(tx_n == 0 && rx_r == rx_w);
    }
    reset();
    enqueue(0x30, (const uint8_t *)"x", 1);
    rx[0].b[rx[0].n - 1u] |= 64u;                      /* unused pack7 high bit */
    assert(ota_take() == 0);
}

int main(void)
{
    uint32_t i, k;
    uint8_t b[513], saved_msg[sizeof ota_msg], saved_wire[sizeof ota_wire];
    static const uint32_t lengths[] = {0, 1, 481, 512};
    reset();
    enqueue(0x11, NULL, 0);
    ota_service();
    assert(tx_n == 1 && tx[0].n == 34u && tx[0].b[2] == 0x11);
    assert(!strcmp((const char *)tx[0].b + 6, TEST_MODE == 2 ? "FM-1_000" : FELUCCA_ID));
    enqueue(0x12, NULL, 0);
    ota_service();
#if JAMBOX_PROTO
    assert(tx_n == 2);
    expect_family(&tx[1]);
    reject_queries();
#else
    assert(tx_n == 1);                                  /* Sloop gains no JamBox claim */
#endif
    memcpy(saved_msg, ota_msg, sizeof ota_msg);
    memcpy(saved_wire, ota_wire, sizeof ota_wire);
    assert(ota_send_msg(0x12, NULL, OTA_TX_BODY_MAX + 1u) == -1);
    assert(!memcmp(saved_msg, ota_msg, sizeof ota_msg) && !memcmp(saved_wire, ota_wire, sizeof ota_wire));
    for (k = 0; k < sizeof lengths / sizeof lengths[0]; k++) {
        reset();
        interleave = 1;
        memset(b, 0x5A, sizeof b);
        assert(!ota_read(0x12345678u, b, lengths[k]));
        assert(read_requests == 1 && rx_r == rx_w && tx_n == (TEST_MODE ? 3u : 2u));
        for (i = 0; i < lengths[k]; i++) assert(b[i] == (uint8_t)(i * 23u + 137u));
        assert(b[lengths[k]] == 0x5A);
#if JAMBOX_PROTO
        expect_family(&tx[2]);
#endif
    }
    reset();
    enqueue(0x12, NULL, 0);
    assert(ota_read(0, b, 1) == -1 && read_requests == 3u && now_ms == 15000u);
    assert(writes == 0);                                /* no app/flash/init hooks */
    printf("identity mode %d: exact records, TX bounds, read interleaving and no writes passed%s\n",
           TEST_MODE, TEST_MODE ? "; malformed probes rejected" : "; JamBox probe unsupported");
    return 0;
}
