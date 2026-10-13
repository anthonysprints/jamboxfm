/* SPDX-License-Identifier: GPL-3.0-only */
/* Real USB-MIDI packet/parser and rescue control flow; only USB registers,
 * clocks, display and boot hooks are simulated. No device or flash access. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#include "../firmware/hal/fm1_usb.h"

static uint8_t regs[256], epregs[16][256], index_reg, read_value;
static uint8_t sent[4096];
static uint32_t sent_n, ticks, now_ms, fm1_ms, feeds, rom, reset_on_idle, reset_on_publish;
static void mock_read(uint32_t r)
{
    read_value = r < 16u ? regs[r] : epregs[index_reg][r];
    if (r >= 2u && r <= 6u) regs[r] = 0; /* interrupt status: read clears */
}
static void mock_write(uint32_t r, uint32_t v)
{
    if (r == 14u) index_reg = (uint8_t)v;
    if (r < 16u) regs[r] = (uint8_t)v;
    else epregs[index_reg][r] = (uint8_t)v;
}
static void mock_send(uint32_t ep, const void *p, uint32_t n)
{
    assert(ep == 1u && sent_n + n <= sizeof sent);
    memcpy(sent + sent_n, p, n);
    sent_n += n;
}
#define fm1_usb_sie_on() 1u
#define fm1_usb_sie_rd_start(r) mock_read(r)
#define fm1_usb_sie_wr_start(r, v) mock_write(r, v)
#define fm1_usb_sie_done() 1u
#define fm1_usb_sie_data() read_value
#define fm1_usb_sof_take() 0u
#define fm1_usb_reset() ((void)0)
#define fm1_usb_attach(p) ((void)(p))
#define fm1_usb_off() ((void)0)
#define fm1_usb_ep0_buf(p) ((void)(p))
#define fm1_usb_ep_txbuf(ep, p) ((void)(ep), (void)(p))
#define fm1_usb_ep_rxbuf(ep, p) ((void)(ep), (void)(p))
#define fm1_usb_ep0_send(p, n) ((void)(p), (void)(n))
#define fm1_usb_ep_send(ep, p, n) mock_send(ep, p, n)
#define fm1_usb_ep_enable(n) ((void)(n))
#define fm1_usb_rx_sync() ((void)0)
static void publish_hook(void);
#define RING_PUBLISH() publish_hook()
#define FELUCCA_OTA 1
#define FELUCCA_CDC 0
#define FELUCCA_UAC 0
static void fm1_delay_ms(uint32_t n) { now_ms += n; ticks += n * 24000u; }
#include "../firmware/src/usb.c"
static void publish_hook(void)
{
    __asm__ volatile("" ::: "memory");
    if (reset_on_publish && !--reset_on_publish) {
        regs[S_INTRUSB] = 4u;
        usb_poll(); /* model the real TIMER5 ISR between check and publication */
    }
}

#include "../firmware/src/bootguard.h"
static bootguard_t bootguard;
static uint8_t recovery_active, flash_ok;
static struct { uint32_t buttons; } fm1_in;
static uint32_t font, sessions;
#define FM1_TICKS_PER_US 24u
#define FL_FAR(x) x
#define FONT_S font
#define C_BLACK 0
#define C_WHITE 65535
static uint32_t fm1_ticks(void) { return ticks; }
static void fm1_wdt_feed(void) { feeds++; }
static void fm1_enter_uboot(void) { rom++; }
static void fm1_input_init(void) {}
static void fm1_input_scan(void) { ticks += 24000u; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void lcd_init(void) {}
static void lcd_fill(int x, int y, int w, int h, int c)
{ (void)x; (void)y; (void)w; (void)h; (void)c; }
static void draw_text_box(int x, int y, int w, const void *f, const char *s, int c, int a)
{ (void)x; (void)y; (void)w; (void)f; (void)s; (void)c; (void)a; }
static void lcd_sync(void) {}
static void ota_service(void) {}
static int ota_session(void) { sessions++; return -1; }
#include "../firmware/src/recovery.c"
static uint32_t ota_now_ms(void) { return now_ms; }
static void ota_idle(void)
{
    now_ms++;
    if (reset_on_idle) {
        reset_on_idle = 0;
        regs[S_INTRUSB] = 4u;
        usb_poll();
    }
}

static void reset_fixture(void)
{
    memset(&usb, 0, sizeof usb);
    memset(regs, 0, sizeof regs);
    memset(epregs, 0, sizeof epregs);
    usb.up = usb.config = 1;
    sx_ready = sx_collect = sx_busy = sx_open = sx_cancel = 0;
    sx_pos = sx_frame_len = so_r = so_w = mi_r = mi_w = mo_r = mo_w = 0;
    now_ms = ticks = fm1_ms = sent_n = feeds = sessions = rom = reset_on_idle = reset_on_publish = 0;
    recovery_last = recovery_usb_last = recovery_fraction = 0;
    bootguard_clear(&bootguard);
    flash_ok = 1;
}

/* A host parser that requires an ending CIN before a new SysEx begins. This
 * checks the actual wire framing as well as the next complete message. */
typedef struct {
    uint8_t body[1024];
    uint32_t length, completed, nested, open;
} host_t;
static host_t host;
static void host_packet(uint32_t pkt)
{
    uint32_t cin = pkt & 15u, k;
    uint32_t n = cin == 4u || cin == 7u ? 3u : cin == 6u ? 2u : cin == 5u ? 1u : 0u;
    assert(n);
    for (k = 0; k < n; k++) {
        uint8_t b = (uint8_t)(pkt >> (8u * (k + 1u)));
        if (b == 0xF0u) {
            host.nested += host.open;
            host.open = 1;
            host.length = 0;
        } else if (b == 0xF7u) {
            assert(cin != 4u && k + 1u == n && host.open);
            host.completed++;
            host.open = 0;
        } else {
            assert(host.open && b < 0x80u && host.length < sizeof host.body);
            host.body[host.length++] = b;
        }
    }
}
static void drain_to_host(void)
{
    while (so_r != so_w) host_packet(sx_out_q[so_r++ % SXQ]);
}

static void transport_tests(void)
{
    uint8_t large[230], retry[] = {0xF0, 0x22, 0x24, 0x35, 0x7F, 0xF7};
    uint32_t start, reset_count;
    const uint8_t *frame;
    uint32_t length;
    memset(large, 0x33, sizeof large);
    large[0] = 0xF0;
    large[sizeof large - 1u] = 0xF7;
    reset_fixture();
    memset(&host, 0, sizeof host);
    assert(ota_wire_send(large, sizeof large) == -1);
    assert(now_ms == 501u && !sx_busy && sx_open && so_w - so_r == SXQ);
    start = now_ms;
    assert(ota_wire_send(retry, sizeof retry) == -1);
    assert(now_ms - start == 501u && sx_open && !sx_busy);
    drain_to_host();
    assert(host.open && !host.completed);
    assert(ota_wire_send(retry, sizeof retry) == 0);
    assert(sx_out_q[so_r % SXQ] == (5u | 0xF7u << 8));
    /* Decode each USB event through the real firmware input parser too. */
    while (so_r != so_w) {
        uint32_t pkt = sx_out_q[so_r++ % SXQ];
        host_packet(pkt);
        midi_in_event(pkt);
    }
    assert(!sx_open && !sx_busy && !host.open && !host.nested && host.completed == 2u);
    assert(host.length == sizeof retry - 2u && !memcmp(host.body, retry + 1, host.length));
    assert(usb.ota_req && !sx_ready); /* complete retry is the real updater key */
    puts("partial SysEx timeout, bounded retry, F7 closure and updater-key parser: passed");

    /* A failure before the first byte must not introduce an orphan F7. */
    reset_fixture();
    so_w = SXQ;
    assert(ota_wire_send(retry, sizeof retry) == -1 && now_ms == 501u && !sx_open);
    so_r = so_w;
    assert(!ota_wire_send(retry, sizeof retry));
    assert((sx_out_q[so_r % SXQ] & 0xFFFFu) == 0xF004u);
    reset_fixture();
    so_w = SXQ;
    usb.config = 0;
    assert(ota_wire_send(retry, sizeof retry) == -1 && !now_ms && !sx_busy && !sx_open);
    puts("zero-byte timeout/disconnect leave no spurious terminator: passed");

    /* A real usb_poll bus reset discards the previous connection's prefix. */
    reset_fixture();
    assert(ota_wire_send(large, sizeof large) == -1 && sx_open);
    reset_count = usb.resets;
    regs[S_INTRUSB] = 4u;
    usb_poll();
    assert(usb.resets == reset_count + 1u && !sx_open && so_r == so_w && !usb.config);
    usb.config = 1;
    retry[4] = 0x12; /* ordinary payload rather than a soft key */
    assert(!ota_wire_send(retry, sizeof retry));
    assert((sx_out_q[so_r % SXQ] & 0xFFFFu) == 0xF004u);
    while (so_r != so_w) midi_in_event(sx_out_q[so_r++ % SXQ]);
    assert(ota_frame_get(&frame, &length) && length == sizeof retry - 2u);
    assert(!memcmp(frame, retry + 1, length));
    ota_frame_done();
    puts("bus reset clears abandoned frame; next full payload parses: passed");

    /* Reset during the wait also flushes the ring, so a fullness-only check
     * would otherwise queue the old frame's tail and falsely report success. */
    reset_fixture();
    reset_on_idle = 1;
    assert(ota_wire_send(large, sizeof large) == -1);
    assert(now_ms == 1u && usb.resets == 1u && !sx_open && !sx_busy && so_r == so_w);
    usb.config = 1;
    assert(!ota_wire_send(retry, sizeof retry));
    assert((sx_out_q[so_r % SXQ] & 0xFFFFu) == 0xF004u);
    while (so_r != so_w) midi_in_event(sx_out_q[so_r++ % SXQ]);
    assert(ota_frame_get(&frame, &length) && length == sizeof retry - 2u);
    assert(!memcmp(frame, retry + 1, length));
    ota_frame_done();
    puts("bus reset during partial-send wait cancels stale tail; next payload parses: passed");

    reset_fixture();
    assert(ota_wire_send(large, sizeof large) == -1 && sx_open);
    reset_on_idle = 1;
    assert(ota_wire_send(retry, sizeof retry) == -1);
    assert(usb.resets == 1u && !sx_open && !sx_busy && so_r == so_w);
    usb.config = 1;
    assert(!ota_wire_send(retry, sizeof retry));
    assert((sx_out_q[so_r % SXQ] & 0xFFFFu) == 0xF004u);
    while (so_r != so_w) midi_in_event(sx_out_q[so_r++ % SXQ]);
    assert(ota_frame_get(&frame, &length) && length == sizeof retry - 2u);
    assert(!memcmp(frame, retry + 1, length));
    ota_frame_done();
    puts("bus reset while waiting to close old frame leaves the next connection clean: passed");

    reset_fixture();
    reset_on_publish = 2u; /* six-byte frame: interrupt at its final USB event */
    assert(ota_wire_send(retry, sizeof retry) == -1);
    assert(usb.resets == 1u && !sx_open && !sx_busy && so_r == so_w);
    usb.config = 1;
    assert(!ota_wire_send(retry, sizeof retry));
    while (so_r != so_w) midi_in_event(sx_out_q[so_r++ % SXQ]);
    assert(ota_frame_get(&frame, &length) && length == sizeof retry - 2u);
    assert(!memcmp(frame, retry + 1, length));
    ota_frame_done();
    puts("ISR reset at final-packet publication cancels and removes stale tail: passed");

    reset_fixture();
    assert(ota_wire_send(large, sizeof large) == -1 && sx_open);
    so_r = so_w; /* old prefix was taken by the former host */
    reset_on_publish = 1u; /* interrupt publication of the standalone F7 */
    assert(ota_wire_send(retry, sizeof retry) == -1);
    assert(usb.resets == 1u && !sx_open && !sx_busy && so_r == so_w);
    usb.config = 1;
    assert(!ota_wire_send(retry, sizeof retry));
    assert((sx_out_q[so_r % SXQ] & 0xFFFFu) == 0xF004u);
    while (so_r != so_w) midi_in_event(sx_out_q[so_r++ % SXQ]);
    assert(ota_frame_get(&frame, &length) && length == sizeof retry - 2u);
    assert(!memcmp(frame, retry + 1, length));
    ota_frame_done();
    puts("ISR reset at old-frame-F7 publication leaves next full frame intact: passed");
}

static void receive_packet(const uint8_t *p, uint32_t n)
{
    assert(n <= 64u && !(n & 3u));
    memcpy(ep1rx, p, n);
    epregs[1][S_RXCOUNT1] = (uint8_t)n;
    epregs[1][S_RXCOUNT2] = 0;
    epregs[1][S_RXCSR1] = 1u;
    regs[S_INTRRX1] = 2u;
    ticks += 12000u;
    recovery_step();
    assert(!usb.rx_pend && mi_r == mi_w);
}
static void recovery_tests(void)
{
    uint8_t notes[64];
    uint8_t start[] = {4, 0xF0, 0x22, 0x24};
    uint8_t finish[] = {7, 0x35, 0x7F, 0xF7};
    uint32_t i, before;
    reset_fixture();
    for (i = 0; i < 16u; i++) {
        notes[i * 4u] = 9; notes[i * 4u + 1u] = 0x90;
        notes[i * 4u + 2u] = (uint8_t)(40 + i); notes[i * 4u + 3u] = 100;
    }
    /* Demonstrate the ordinary app's backpressure remains intact. */
    assert(ep1_take(notes, sizeof notes));
    assert(ep1_take(notes, sizeof notes));
    assert(ep1_take(notes, sizeof notes));
    before = mi_w;
    assert(!ep1_take(start, sizeof start) && mi_w == before && !usb.ota_req);
    /* Rescue makes room before polling; mixed notes cannot block a split key. */
    receive_packet(start, sizeof start);
    for (i = 0; i < 100u; i++) receive_packet(notes, sizeof notes);
    /* Channel messages correctly abort SysEx; send a fresh split command. */
    receive_packet(start, sizeof start);
    receive_packet(finish, sizeof finish);
    assert(sessions == 1u && !usb.ota_req && feeds >= 103u);
    /* The same RX path still accepts the independent ROM key. */
    finish[2] = 0x7D;
    receive_packet(start, sizeof start);
    receive_packet(finish, sizeof finish);
    assert(rom == 1u && !bootguard.pending);
    puts("rescue drains saturated MIDI before/after real USB poll; OTA and ROM keys work: passed");
}
int main(void)
{
    transport_tests();
    recovery_tests();
    puts("JamBox update transport tests passed");
    return 0;
}
