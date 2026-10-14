/* SPDX-License-Identifier: GPL-3.0-only */
/* Actual local keyboard -> chord -> MIDI / voices / ARP / recording paths. */
#include <assert.h>
#define JAMBOX_PROTO 1
#define main hostsim_main
#include "hostsim.c"
#undef main

static unsigned checks;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "chord check failed at line %d: %s\n", __LINE__, #x); abort(); } checks++; } while (0)
static void keys(uint32_t mask) { fm1_in.notes = mask; keyboard_block(); }
static void reset(void)
{
    unsigned i;
    keys(0);
    for (i = 0; i < NTRK; i++) trk_all_off(&trk[i]);
    memset(trk, 0, sizeof trk); memset(&song, 0, sizeof song);
    memset(kb_kind, 0, sizeof kb_kind); memset(kb_n, 0, sizeof kb_n);
    memset(sus_on, 0, sizeof sus_on); memset(sus_wait, 0, sizeof sus_wait);
    memset(vl_n, 0, sizeof vl_n); memset(held_key, 0, sizeof held_key);
    memset(stq, 0, sizeof stq); memset(&fm1_in, 0, sizeof fm1_in);
    kb_prev = 0; in_key = 0; in_chord = 0; ft_on = rec_wait = ci_on = panic_req = 0;
    transport_req = 0; clk_beat = clk_pos = 0; ly_lock = LY_PLAY;
    host_tracks_init(); usb.config = 1; mo_w = mo_r = 0;
    for (i = 0; i < NPART; i++) {
        host_preset(&trk[i], 0, 0);
        trk[i].p[P_VOICE] = V_POLY; trk[i].p[P_ALLOC] = 1;
        trk[i].p[P_ROOT] = 3; trk[i].p[P_SCALE] = 1;
        trk[i].p[P_QUANT] = 1; trk[i].p[P_CHORD] = 6;
        trk[i].p[P_TRANS] = trk[i].p[P_AMODE] = trk[i].p[P_AHOLD] = 0;
        trk[i].p[P_STRUM] = trk[i].p[P_VLEAD] = trk[i].p[P_MUTE] = 0;
    }
}
static unsigned gates(const track_t *t)
{
    unsigned i, n = 0;
    for (i = 0; i < NVOICE; i++) n += t->v[i].active && t->v[i].gate;
    return n;
}
static int gate(const track_t *t, unsigned note)
{
    unsigned i;
    for (i = 0; i < NVOICE; i++) if (t->v[i].active && t->v[i].gate && t->v[i].note == note) return 1;
    return 0;
}
static void pitches(unsigned k, unsigned n, unsigned a, unsigned b, unsigned c, unsigned d)
{
    const unsigned want[4] = {a,b,c,d}; unsigned i;
    CHECK(kb_kind[k] == KS_NOTE && kb_n[k] == n);
    for (i = 0; i < n; i++) CHECK(kb_nt[k][i] == want[i]);
}
static void mapping(void)
{
    static const unsigned black[] = {1,3,5,8,10,13,15,17,20,22,25};
    static const unsigned shape[][4] = {{63,0,0,0},{63,67,70,0},{63,67,70,74},
        {63,67,74,77},{63,68,70,0},{63,70,75,0},{63,67,70,0},{63,66,70,0},{63,66,69,0}};
    static const unsigned size[] = {1,3,4,4,3,3,3,3,3};
    unsigned p, q, k, type;
    reset(); CHECK(TP[P_CHORD].max == 8);
    for (p = 0; p < NPART; p++) {
        song.sel = p;
        for (type = 0; type <= 8; type++) {
            trk[p].p[P_CHORD] = type;
            keys(1u << 10);
            pitches(10, size[type], shape[type][0], shape[type][1], shape[type][2], shape[type][3]);
            CHECK(gates(&trk[p]) == size[type]); keys(0); CHECK(!gates(&trk[p]));
        }
        trk[p].p[P_CHORD] = 6;
        for (q = 0; q < 2; q++) {
            trk[p].p[P_QUANT] = q;
            for (k = 0; k < sizeof black / sizeof black[0]; k++) {
                unsigned n = 53 + black[k];
                /* Independent D# major membership (Eb F G Ab Bb C D). */
                if (q) while (!(0x5ADu & (1u << (n % 12u)))) n--;
                keys(1u << black[k]); pitches(black[k], 3, n, n + 4, n + 7, 0);
                keys(0); CHECK(!gates(&trk[p]));
            }
        }
    }
    for (p = 0; p < NPART; p++) {
        reset(); song.sel = p; trk[p].p[P_VOICE] = V_MONO;
        song.playing = 1; song.rec = 1u << p;
        keys(1u << 10);
        CHECK(trk[p].p[P_VOICE] == V_POLY && gates(&trk[p]) == 3 && mo_w == 3);
        CHECK(trk[p].step[0].n == 3 && trk[p].step[0].note[0] == 63 &&
              trk[p].step[0].note[1] == 67 && trk[p].step[0].note[2] == 70);
        keys(0); CHECK(!gates(&trk[p]) && mo_w == 6);
    }
    reset(); trk[0].p[P_ROOT] = 0; trk[0].p[P_CHORD] = 7;
    keys(1u << 14); pitches(14, 3, 67,70,74,0); /* G minor retains Bb in C-major SNAP. */
    keys(0);
    trk[0].p[P_QUANT] = 0;
    keys(1u << 10); pitches(10, 3, 63,66,70,0); /* OFF retains printed D#. */
    keys(0);
    { uint8_t c[4]; CHECK(chord_notes(&trk[0], 126, c) == 1 && c[0] == 126); }
    puts("chords: all shapes, black roots, synth parts, OFF/SNAP and unchanged fixed intervals passed");
}
static void ownership(void)
{
    unsigned reverse;
    for (reverse = 0; reverse < 2; reverse++) {
        reset(); keys(1u << 10); keys((1u << 10) | (1u << 11));
        pitches(10,3,63,67,70,0); pitches(11,3,63,67,70,0);
        CHECK(gates(&trk[0]) == 3 && mo_w == 3);
        keys(1u << (reverse ? 10 : 11)); CHECK(gates(&trk[0]) == 3 && mo_w == 3);
        keys(0); CHECK(!gates(&trk[0]) && mo_w == 6);

        reset(); trk[0].p[P_QUANT] = 0;
        keys(1u << 7); keys((1u << 7) | (1u << 14)); /* C major and G major share G. */
        CHECK(gates(&trk[0]) == 5 && mo_w == 5);
        keys(1u << (reverse ? 7 : 14));
        CHECK(gates(&trk[0]) == 3 && gate(&trk[0],67) && mo_w == 7);
        CHECK(gate(&trk[0],reverse ? 60 : 71) && gate(&trk[0],reverse ? 64 : 74));
        keys(0); CHECK(!gates(&trk[0]) && mo_w == 10);
    }
    reset(); keys(1u << 10);
    trk[0].p[P_ROOT] = 7; trk[0].p[P_SCALE] = 2; trk[0].p[P_QUANT] = 2;
    trk[0].p[P_CHORD] = 8; trk[0].p[P_TRANS] = 12; song.octave = 1; song.sel = 2;
    keys(0); CHECK(!gates(&trk[0]) && mo_w == 6);
    reset(); keys(1u << 10); trk[0].p[P_CHORD] = 0;
    keys(0); CHECK(!gates(&trk[0]) && mo_w == 6);

    reset(); keys(1u << 10); song.sel = 1; keys((1u << 10) | (1u << 11));
    CHECK(gates(&trk[0]) == 3 && gates(&trk[1]) == 3);
    keys(1u << 11); CHECK(!gates(&trk[0]) && gates(&trk[1]) == 3); keys(0);

    reset(); keys(1u << 10); panic_req = 1; events_block(CTL);
    CHECK(!gates(&trk[0]) && kb_kind[10] == KS_NONE);
    keys((1u << 10) | (1u << 11)); CHECK(gates(&trk[0]) == 3);
    keys(0); CHECK(!gates(&trk[0]));
    puts("chords: shared tones / duplicate roots in both release orders, original releases, part isolation and panic passed");
}
static void arp_and_strum(void)
{
    unsigned reverse, i;
    for (reverse = 0; reverse < 2; reverse++) {
        reset(); trk[0].p[P_AMODE] = 1;
        keys(1u << 10); keys((1u << 10) | (1u << 11));
        CHECK(trk[0].nheld == 3 && trk[0].arp_phys == 3);
        keys(1u << (reverse ? 10 : 11));
        CHECK(trk[0].nheld == 3 && trk[0].arp_phys == 3);
        for (i = 0; i < 3; i++) CHECK((held_key[0][i] & 31u) == (reverse ? 11u : 12u));
        trk[0].p[P_QUANT] = 0; trk[0].arp_idx = 0xFFFFFFFFu;
        CHECK(arp_next(&trk[0]) == (reverse ? 63u : 64u));
        CHECK(arp_next(&trk[0]) == (reverse ? 67u : 68u));
        CHECK(arp_next(&trk[0]) == (reverse ? 70u : 71u));
        keys(0); CHECK(!trk[0].nheld && !trk[0].arp_phys);

        reset(); trk[0].p[P_STRUM] = reverse ? -20 : 20;
        keys(1u << 10); keys((1u << 10) | (1u << 11));
        CHECK(gates(&trk[0]) == 1); keys(1u << (reverse ? 10 : 11));
        strum_block(FS / 10); CHECK(gates(&trk[0]) == 3);
        keys(0); CHECK(!gates(&trk[0]));
        keys(1u << 10); keys(0); strum_block(FS / 10); CHECK(!gates(&trk[0]));

        reset(); trk[0].p[P_QUANT] = 0; trk[0].p[P_STRUM] = 20;
        keys(1u << 7); keys((1u << 7) | (1u << 14));
        keys(1u << (reverse ? 7 : 14)); strum_block(FS / 10);
        CHECK(gates(&trk[0]) == 3 && gate(&trk[0],67));
        keys(0); CHECK(!gates(&trk[0]));
    }
    reset(); trk[0].p[P_AMODE] = 1; trk[0].p[P_AHOLD] = 1;
    keys(1u << 10); keys((1u << 10) | (1u << 11)); keys(1u << 11); keys(0);
    CHECK(trk[0].nheld == 3 && !trk[0].arp_phys);
    keys(1u << 14); CHECK(trk[0].nheld == 3 && trk[0].held[0] == 67); keys(0);
    puts("chords: ARP owner transfer, HOLD replacement and strum cancellation passed");
}
static void recording_and_white(void)
{
    unsigned i;
    reset(); song.playing = song.rec = 1;
    keys(1u << 10); keys((1u << 10) | (1u << 11));
    CHECK(trk[0].step[0].n == 3 && trk[0].step[0].note[0] == 63 &&
          trk[0].step[0].note[1] == 67 && trk[0].step[0].note[2] == 70);
    CHECK(trk[0].rh_n == 3); keys(1u << 11); CHECK(trk[0].rh_n == 3);
    keys(0); CHECK(!trk[0].rh_n);

    reset(); trk[0].p[P_QUANT] = 2; trk[0].p[P_CHORD] = 1;
    keys(1u << 10); CHECK(kb_kind[10] == KS_MOD && !gates(&trk[0]));
    keys((1u << 10) | (1u << 7)); pitches(7,3,67,70,75,0); /* WHITE C -> Eb, D# inversion. */
    keys(1u << 7); pitches(7,3,63,67,70,0); keys(0); CHECK(!gates(&trk[0]));
    trk[0].p[P_CHORD] = 7; /* fixed minor, major scale: modifier flips the actual minor third. */
    keys(1u << 1); keys((1u << 1) | (1u << 7)); pitches(7,3,63,67,70,0); keys(0);

    reset(); trk[0].p[P_QUANT] = trk[1].p[P_QUANT] = 2;
    keys(1u << 10); song.sel = 1; keys((1u << 10) | (1u << 7));
    pitches(7,3,63,67,70,0); keys(0); /* track 0's modifier must not invert track 1. */

    reset(); trk[0].p[P_QUANT] = 2; keys(1u << 10);
    trk[0].p[P_QUANT] = 1; keys((1u << 10) | (1u << 7));
    pitches(7,3,60,64,67,0); keys(1u << 7); pitches(7,3,60,64,67,0); keys(0);

    reset(); trk[0].p[P_ROOT] = 0; trk[0].p[P_QUANT] = 2; trk[0].p[P_CHORD] = 1;
    keys((1u << 7) | (1u << 11)); /* C and E share E/G. */
    CHECK(gates(&trk[0]) == 4);
    keys((1u << 7) | (1u << 11) | (1u << 1));
    pitches(7,3,60,63,67,0); pitches(11,3,64,68,71,0);
    CHECK(gates(&trk[0]) == 6);
    keys((1u << 7) | (1u << 11)); CHECK(gates(&trk[0]) == 4);
    keys(1u << 11); CHECK(gates(&trk[0]) == 3 && gate(&trk[0],64) && gate(&trk[0],67)); keys(0);
    for (i = 0; i < NPART; i++) CHECK(!gates(&trk[i]));
    puts("chords: recorded pitches / holds and explicit WHITE modifiers passed");
}
static void inversions(void)
{
    static const unsigned want[3][3] = {{67,70,75},{70,75,79},{75,79,82}};
    static const unsigned power[3][3] = {{67,72,84},{72,79,84},{79,84,96}};
    unsigned i;
    reset(); CHECK(TP[P_VLEAD].max == 4);
    for (i = 0; i < 3; i++) {
        trk[0].p[P_VLEAD] = i + 2;
        keys(1u << 10); pitches(10,3,want[i][0],want[i][1],want[i][2],0);
        CHECK(kb_root[10] == 63); keys(0); CHECK(!gates(&trk[0]));
    }
    reset(); trk[0].p[P_QUANT] = 0; trk[0].p[P_VLEAD] = 1;
    keys(1u << 7); pitches(7,3,60,64,67,0); keys(0);
    keys(1u << 12); pitches(12,3,60,65,69,0); keys(0); /* AUTO keeps old nearest voicing. */
    { uint8_t c[4], before[4], n = vl_n[0];
      memcpy(before, vl_prev[0], sizeof before);
      CHECK(chord_preview_notes(&trk[0], 67, 0, c) == 3);
      CHECK(c[0] == 59 && c[1] == 62 && c[2] == 67);
      CHECK(vl_n[0] == n && !memcmp(before, vl_prev[0], sizeof before));
      trk[0].p[P_VLEAD] = 3;
      CHECK(chord_preview_notes(&trk[0], 63, 0, c) == 3 && c[0] == 70 && c[1] == 75 && c[2] == 79);
      CHECK(vl_n[0] == n && !memcmp(before, vl_prev[0], sizeof before)); }

    reset(); trk[0].p[P_VLEAD] = 2; song.playing = song.rec = 1;
    keys(1u << 10);
    CHECK(trk[0].step[0].n == 3 && trk[0].step[0].note[0] == 67 &&
          trk[0].step[0].note[1] == 70 && trk[0].step[0].note[2] == 75);
    trk[0].p[P_VLEAD] = 4; keys(0); CHECK(!gates(&trk[0]));
    { uint8_t c[4]; CHECK(chord_play_notes(&trk[0], 120,0,c) == 3); CHECK(c[2] <= 127); }
    for (i = 0; i < 3; i++) {
        reset(); trk[0].p[P_QUANT] = 0; trk[0].p[P_CHORD] = 5; trk[0].p[P_VLEAD] = i + 2;
        keys(1u << 7); pitches(7,3,power[i][0],power[i][1],power[i][2],0);
        CHECK(gates(&trk[0]) == 3 && mo_w == 3);
        keys(0); CHECK(!gates(&trk[0]) && mo_w == 6);
        { uint8_t c[4];
          CHECK(chord_play_notes(&trk[0],115,0,c) == 3 && c[0] == 115 && c[1] == 122 && c[2] == 127);
          CHECK(chord_play_notes(&trk[0],108,0,c) == 3 && c[0] == 108 && c[1] == 115 && c[2] == 120); }
    }
    reset(); trk[0].p[P_ROOT] = 0; trk[0].p[P_QUANT] = 2; trk[0].p[P_CHORD] = 5;
    keys(1u << 10); keys((1u << 10) | (1u << 7));
    pitches(7,3,67,72,84,0); CHECK(gates(&trk[0]) == 3 && mo_w == 3);
    keys(1u << 7); pitches(7,3,60,67,72,0); CHECK(gates(&trk[0]) == 3);
    keys(0); CHECK(!gates(&trk[0]) && mo_w == 8);
    { uint8_t c[4]; CHECK(chord_play_notes(&trk[0],115,CM_INV,c) == 3 &&
                          c[0] == 115 && c[1] == 122 && c[2] == 127); }
    reset(); trk[0].p[P_CHORD] = 5; trk[0].p[P_VLEAD] = 1;
    { uint8_t c[4];
      static const unsigned roots[] = {65,67,60,57,62};
      CHECK(chord_play_notes(&trk[0],60,0,c) == 3 && c[0] == 60 && c[1] == 67 && c[2] == 72);
      CHECK(chord_play_notes(&trk[0],64,0,c) == 3 && c[0] == 64 && c[1] == 71 && c[2] == 76);
      for (i = 0; i < sizeof roots / sizeof roots[0]; i++)
          CHECK(chord_play_notes(&trk[0],roots[i],0,c) == 3 && c[0] < c[1] && c[1] < c[2]); }
    puts("chords: manual +1/+2/+3, unchanged AUTO, recorded voicing and bounded pitches passed");
}
int main(void)
{
    mapping(); ownership(); arp_and_strum(); recording_and_white(); inversions();
    printf("JamBox chord behavior: %u checks passed.\n", checks);
    return 0;
}
