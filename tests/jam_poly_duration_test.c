/* SPDX-License-Identifier: GPL-3.0-only */
/* Real production panel/audio adapter: independent POLY note recording and playback. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "../firmware/host/jam_emu.c"

static unsigned checks;
#define CHECK(c, message) do { if (!(c)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, message); abort(); } checks++; } while (0)

static void blocks(uint32_t n)
{
    while (n--) {
        const float *a = jam_emu_render();
        uint32_t i;
        for (i = 0; i < CTL * 2u; i++)
            assert(isfinite(a[i]) && a[i] >= -1.0f && a[i] <= 1.0f);
    }
}
static void tap(uint32_t button)
{
    jam_emu_button(button, 1); blocks(1);
    jam_emu_button(button, 0); blocks(1);
}
static uint32_t gated(const track_t *t, uint32_t note)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += t->v[i].active && t->v[i].gate && t->v[i].stage != 4u && t->v[i].note == note;
    return n;
}
static uint32_t gated_total(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++) n += t->v[i].active && t->v[i].gate && t->v[i].stage != 4u;
    return n;
}
static uint32_t note_age(const track_t *t, uint32_t note)
{
    uint32_t i, age = 0;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].gate && t->v[i].note == note && t->v[i].age > age)
            age = t->v[i].age;
    return age;
}
static int slot(const track_t *t, uint32_t step, uint32_t note)
{
    uint32_t k;
    const step_t *s = &t->step[step % NSTEP];
    if (s->time != ST_NOTE) return -1;
    for (k = 0; k < s->n && k < 4u; k++) if (s->note[k] == note) return (int)k;
    return -1;
}
static uint32_t duration(const track_t *t, uint32_t step, uint32_t note)
{
    int k = slot(t, step, note);
    return k < 0 ? 0u : t->note_len[step % NSTEP][k];
}
/* Eighths of a real swung step, away from boundaries. No private duration-runtime helpers. */
static void at(track_t *t, uint32_t absolute, uint32_t eighth)
{
    uint32_t i;
    for (i = 0; i < FS * 30u / CTL; i++) {
        uint32_t into, len, grid = trk_grid(t, &into, &len);
        if (grid == absolute && into >= len * eighth / 8u) return;
        if (grid > absolute) {
            fprintf(stderr, "Clock passed target %u + %u/8 (now %u).\n", absolute, eighth, grid);
            abort();
        }
        blocks(1);
    }
    CHECK(0, "grid target reached within bounded audio time");
}
static track_t *fresh(uint32_t part, uint32_t mode, uint32_t len)
{
    uint32_t i;
    track_t *t;
    jam_emu_panic(); blocks(2);
    CHECK(jam_new_dusty(), "fresh session starts stopped");
    blocks(400); /* Let the actual preset fades and engine handoff finish. */
    for (i = 0; i < NTRK; i++) {
        steps_clear(&trk[i]);
        trk[i].p[P_SLEN] = (int16_t)len;
        trk[i].p[P_SDIV] = 2;
        trk[i].p[P_MUTE] = i != part;
    }
    track_select(part); go_home();
    song.solo = song.rec = 0;
    song.octave = 0;
    song.g[G_BPM] = 120;
    song.g[G_SWING] = song.g[G_CLOCK] = song.g[G_SYNC] = 0;
    t = &trk[part];
    t->p[P_CHORD] = t->p[P_QUANT] = t->p[P_TRANS] = 0;
    t->p[P_AMODE] = t->p[P_AHOLD] = t->p[P_STRUM] = t->p[P_SSWING] = 0;
    t->p[P_VOICE] = (int16_t)mode;
    t->p[P_ATK] = 0; t->p[P_DEC] = 0; t->p[P_SUS] = 127; t->p[P_REL] = 0;
    t->p[P_ESYNC] = t->p[P_GLIDE] = 0;
    t->p[P_SGATE] = 64;
    t->p[P_LEVEL] = 90;
    blocks(2);
    return t;
}
static void start_record(track_t *t)
{
    tap(B_PLAY); tap(B_REC);
    CHECK(song.playing && song.rec == (1u << (uint32_t)(t - trk)), "panel starts quantized overdub on selected part");
}
static void playback(void)
{
    jam_emu_panic(); blocks(2);
    tap(B_PLAY);
    CHECK(song.playing && !song.rec, "playback restarts without record arm");
}
static void put(track_t *t, uint32_t step, uint32_t note, uint32_t len)
{
    int k;
    step_add(t, step, note, 100, LV_NORM, 0);
    k = slot(t, step, note);
    assert(k >= 0);
    t->note_len[step][k] = (uint8_t)len;
    t->duration_mode = 1;
    t->seq_active = 1;
}

static void test_staggered_recording(void)
{
    track_t *t = fresh(2, V_POLY, 16);
    start_record(t);
    at(t, 0, 1); jam_emu_key(7, 1);  /* C4 */
    at(t, 2, 1); jam_emu_key(11, 1); /* E4 */
    CHECK(gated(t,60) && gated(t,64), "live C remains held when staggered E starts");
    at(t, 4, 1); jam_emu_key(11, 0);
    CHECK(gated(t,60) && !gated(t,64), "releasing E leaves independently held C sounding");
    at(t, 6, 1); jam_emu_key(7, 0);
    CHECK(duration(t,0,60) == 6 && duration(t,2,64) == 2, "staggered recording stores C six steps and E two steps");
    CHECK(t->duration_mode && t->step[1].time == ST_REST, "POLY sustain uses individual lengths rather than rewriting rests as ties");
    playback();
    at(t,0,1); CHECK(gated(t,60) && !gated(t,64), "playback begins C alone");
    at(t,2,1); CHECK(gated(t,60) && gated(t,64), "later E onset leaves sequenced C sounding");
    at(t,4,1); CHECK(gated(t,60) && !gated(t,64), "sequenced E ends while C continues");
    at(t,6,1); CHECK(!gated_total(t), "both independently recorded notes end on their own grid endpoints");
}
static void test_chord_releases_and_quantization(void)
{
    track_t *t = fresh(1, V_POLY, 16);
    start_record(t);
    at(t,0,1); jam_emu_key(7,1); jam_emu_key(11,1); jam_emu_key(14,1);
    at(t,1,1); jam_emu_key(11,0);
    at(t,3,1); jam_emu_key(14,0);
    at(t,5,1); jam_emu_key(7,0);
    CHECK(t->step[0].n == 3 && duration(t,0,60)==5 && duration(t,0,64)==1 && duration(t,0,67)==3,
          "notes beginning as one chord keep three different release lengths");
    at(t,6,7); jam_emu_key(9,1); /* D4: nearest onset is step 7. */
    at(t,7,1); jam_emu_key(9,0);
    CHECK(slot(t,7,62)>=0 && duration(t,7,62)==1, "late onset rounds forward and a short note retains the one-step minimum");
    playback();
    at(t,0,1); CHECK(gated_total(t)==3, "same-step chord playback starts all three voices");
    at(t,1,1); CHECK(gated_total(t)==2 && gated(t,60) && gated(t,67), "shortest chord tone releases alone");
    at(t,3,1); CHECK(gated_total(t)==1 && gated(t,60), "middle chord tone releases while longest remains");
    at(t,5,1); CHECK(!gated_total(t), "longest chord tone releases without stuck companions");
}
static void test_overdub_and_retrigger(void)
{
    track_t *t = fresh(2,V_POLY,16);
    put(t,0,60,3); put(t,1,64,5); put(t,2,60,5);
    playback();
    at(t,0,1); uint32_t first = note_age(t,60);
    at(t,2,1);
    CHECK(gated(t,60)==1 && gated(t,64) && note_age(t,60)>first, "same-pitch onset retriggers one voice while another pitch overlaps");
    at(t,3,1); CHECK(gated(t,60) && gated(t,64), "earlier C endpoint cannot cut off its newer retrigger");
    at(t,6,1); CHECK(gated(t,60) && !gated(t,64), "unrelated E endpoint leaves retriggered C sounding");
    at(t,7,1); CHECK(!gated_total(t), "retriggers terminate at the replacement event's endpoint");

    t = fresh(2,V_POLY,16);
    put(t,0,60,6);
    playback(); tap(B_REC);
    at(t,2,1); jam_emu_key(11,1);
    at(t,4,1); jam_emu_key(11,0);
    CHECK(duration(t,0,60)==6 && duration(t,2,64)==2, "overdub keeps the earlier note's length while adding a staggered note");
    playback(); at(t,2,1);
    CHECK(gated(t,60) && gated(t,64), "overdubbed staggered pair replays together");
}
static void test_legacy_tie_overdub(void)
{
    track_t *t=fresh(2,V_POLY,16);
    uint32_t i;
    step_add(t,0,60,100,LV_NORM,0);
    for(i=1;i<6;i++) t->step[i].time=ST_TIE;
    t->seq_active=1;
    playback(); at(t,2,1); tap(B_REC); jam_emu_key(11,1);
    CHECK(gated(t,60) && gated(t,64), "overdub into an existing legacy tie leaves the long note sounding");
    at(t,4,1); jam_emu_key(11,0);
    CHECK(duration(t,0,60)==6 && duration(t,2,64)==2,
          "legacy tie chain is preserved as a duration before inserting a staggered onset");
    playback(); at(t,2,1);
    CHECK(gated(t,60) && gated(t,64), "converted legacy hold and new overdub replay together");
    at(t,4,1); CHECK(gated(t,60) && !gated(t,64), "converted legacy hold outlasts the new short note");
    at(t,6,1); CHECK(!gated_total(t), "converted legacy hold ends at its original endpoint");
}
static void test_loop_wrap(void)
{
    track_t *t = fresh(2,V_POLY,16);
    start_record(t);
    at(t,14,1); jam_emu_key(7,1);
    at(t,15,1); jam_emu_key(11,1);
    at(t,17,1); jam_emu_key(11,0);
    at(t,18,1); jam_emu_key(7,0);
    CHECK(duration(t,14,60)==4 && duration(t,15,64)==2, "recorded durations cross the pattern boundary independently");
    playback(); at(t,14,1);
    CHECK(gated(t,60) && !gated(t,64), "end-of-loop note starts in its original step");
    at(t,16,1); CHECK(gated(t,60) && gated(t,64), "loop restart preserves both crossing notes");
    at(t,17,1); CHECK(gated(t,60) && !gated(t,64), "short crossing note ends on next loop's step one");
    at(t,18,1); CHECK(!gated_total(t), "long crossing note ends without hanging across further loops");
    at(t,34,1); CHECK(!gated_total(t), "crossing notes release again after the next complete loop");
    at(t,50,1); CHECK(!gated_total(t), "repeated loop crossings do not accumulate active notes");
}
static void test_voice_capacity_and_clock_changes(void)
{
    track_t *t = fresh(2,V_POLY,16);
    const uint8_t chord[] = {60,64,67,71};
    uint32_t i;
    for (i=0;i<4;i++) put(t,0,chord[i],8);
    put(t,2,74,4); put(t,3,77,2);
    playback(); at(t,3,1);
    CHECK(gated_total(t)==6, "four-note onset plus staggered melody can use six of the eight synth voices");
    song.g[G_SWING]=76; song.g[G_BPM]=83;
    at(t,5,1); CHECK(gated_total(t)==5 && !gated(t,77), "swing and slower tempo preserve the short melody endpoint");
    song.g[G_BPM]=173;
    at(t,6,1); CHECK(gated_total(t)==4 && !gated(t,74), "tempo increase preserves the other melody endpoint");
    at(t,8,1); CHECK(!gated_total(t), "all chord notes end at their grid endpoint after swing and tempo changes");
}
static void test_live_sequence_ownership_and_stop(void)
{
    track_t *t = fresh(2,V_POLY,16);
    put(t,0,60,4);
    playback(); at(t,1,1); jam_emu_key(7,1);
    at(t,4,1); CHECK(gated(t,60), "sequencer note-off does not cut off the same pitch held on the keyboard");
    jam_emu_key(7,0); blocks(2);
    CHECK(!gated(t,60), "last live owner releases once sequenced owner has ended");

    playback(); at(t,1,1); jam_emu_key(7,1); jam_emu_key(7,0); blocks(1);
    CHECK(gated(t,60), "live key release does not cut off a still-sequenced note of the same pitch");
    at(t,4,1); CHECK(!gated(t,60), "sequenced owner ends normally after a live same-pitch tap");

    t=fresh(2,V_POLY,16); start_record(t);
    at(t,0,1); jam_emu_key(7,1);
    at(t,3,1); tap(B_REC);
    if (song.rec || duration(t,0,60)!=3)
        fprintf(stderr,"Disarm state: rec=%u length=%u seq_abs=%u gate=%u\n",song.rec,duration(t,0,60),t->seq_abs,gated(t,60));
    CHECK(!song.rec && duration(t,0,60)==3, "disarming record finalizes a held note at the current endpoint");
    CHECK(gated(t,60), "disarming record preserves physically held live input");
    jam_emu_key(7,0); blocks(1);
    CHECK(!gated_total(t), "live note releases cleanly after record is disarmed");
    playback(); at(t,0,1); tap(B_PLAY);
    CHECK(!song.playing && !gated_total(t), "transport stop releases independent playback notes");
    playback(); at(t,0,1); jam_emu_panic(); blocks(2);
    CHECK(!song.playing && !song.rec && !gated_total(t) && !fm1_in.notes, "panic clears transport, recorded note ownership and physical notes");

    t=fresh(2,V_POLY,16); start_record(t);
    at(t,0,1); jam_emu_key(7,1);
    at(t,3,1); tap(B_PLAY);
    CHECK(!song.playing && duration(t,0,60)==3, "STOP finalizes a held recorded note at the current endpoint");
    CHECK(gated(t,60), "STOP preserves the independently held keyboard voice");
    jam_emu_key(7,0); blocks(1);
    CHECK(!gated_total(t), "held keyboard note releases normally after transport STOP");
}
static void test_free_take(void)
{
    track_t *t = fresh(2,V_POLY,16);
    const uint32_t take_blocks=(FS*2u+CTL/2u)/CTL;
    rec_tempo=rec_count=0;
    tap(B_REC); jam_emu_key(7,1);
    CHECK(ft_on && !song.playing, "empty-session first note starts real free-take recorder");
    while (ft_t < take_blocks*4u/16u) blocks(1);
    jam_emu_key(11,1);
    while (ft_t < take_blocks*6u/16u) blocks(1);
    jam_emu_key(11,0);
    while (ft_t < take_blocks*8u/16u) blocks(1);
    jam_emu_key(7,0);
    while (ft_t < take_blocks) blocks(1);
    tap(B_REC);
    CHECK(song.playing && !ft_on && t->p[P_SLEN]==16, "two-second free take closes into one sixteen-step loop");
    CHECK(duration(t,0,60)==8 && duration(t,4,64)==2, "free take preserves overlapping note durations when quantizing its events");
    blocks((FS*350u/1000u+CTL-1u)/CTL); /* Existing free-take close gesture owns REC/PLAY for 300 ms. */
    playback(); at(t,4,1);
    CHECK(gated(t,60) && gated(t,64), "free-take playback reproduces staggered overlap");
    at(t,6,1); CHECK(gated(t,60) && !gated(t,64), "free-take short note ends independently");
    at(t,8,1); CHECK(!gated_total(t), "free-take long note also ends");
}
static void test_legacy_modes(void)
{
    track_t *t = fresh(0,V_LEGATO,16);
    CHECK(!t->p[P_CHORD] && t->p[P_VOICE]==V_LEGATO, "Bass keeps its existing one-note default");
    start_record(t); at(t,0,1);
    jam_emu_key(7,1); jam_emu_key(11,1);
    CHECK(gated_total(t)==1 && t->step[0].n==1 && t->step[0].note[0]==64, "LEGATO Bass retains one voice and one recorded onset per step");
    at(t,2,1); jam_emu_key(11,0); jam_emu_key(7,0);
    CHECK(!t->duration_mode && !duration(t,0,64), "legacy Bass continues to use the existing group articulation model");
    t=fresh(TRK_DRUM,V_POLY,16); start_record(t); at(t,0,1);
    jam_emu_key(7,1); jam_emu_key(7,0);
    CHECK(dstep_mask(&t->dstep[0]) && !t->duration_mode, "drum recording stays lane based without pitched note durations");
}
int main(void)
{
    jam_emu_init();
    test_staggered_recording();
    test_chord_releases_and_quantization();
    test_overdub_and_retrigger();
    test_legacy_tie_overdub();
    test_loop_wrap();
    test_voice_capacity_and_clock_changes();
    test_live_sequence_ownership_and_stop();
    test_free_take();
    test_legacy_modes();
    jam_emu_panic(); blocks(2);
    CHECK(!emu_lcd_errors, "production UI remains within the physical LCD during recording and playback");
    printf("Independent POLY recording: %u checks passed.\n",checks);
    return 0;
}
