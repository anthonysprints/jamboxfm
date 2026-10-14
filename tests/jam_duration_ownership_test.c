/* SPDX-License-Identifier: GPL-3.0-only */
/* Share only the existing real-adapter test fixtures; run a separate ownership suite. */
#define main jam_duration_core_suite_main
#include "jam_poly_duration_test.c"
#undef main

static void mixed_sequence_owners(void)
{
    track_t *t=fresh(2,V_POLY,16);
    step_add(t,0,60,100,LV_NORM,0); t->step[1].time=ST_TIE;
    put(t,2,60,4); t->p[P_SGATE]=128;
    playback(); at(t,2,1);
    CHECK(gated(t,60), "ending an old legacy group cannot cut off its newer explicit-duration onset");
    at(t,3,1); CHECK(gated(t,60), "legacy group gate expiry leaves the explicit pitch owner sounding");
    at(t,6,1); CHECK(!gated(t,60), "mixed same-pitch transition ends at the explicit endpoint");

    t=fresh(2,V_POLY,16); put(t,0,60,6);
    step_add(t,2,60,100,LV_NORM,0); /* Duration zero deliberately keeps this new onset's legacy gate. */
    playback(); at(t,2,1); CHECK(gated(t,60), "new legacy onset can retrigger an independently sustained pitch");
    at(t,3,1); CHECK(!gated(t,60), "legacy retrigger keeps its short gate instead of the older explicit endpoint");
}
static void clear_and_restart(void)
{
    track_t *t=fresh(2,V_POLY,16); put(t,0,60,8);
    playback(); at(t,1,1); steps_clear(t); blocks(2);
    CHECK(!gated(t,60), "clearing a playing pattern releases its old independent owners");
    t=fresh(2,V_POLY,16); put(t,0,60,8);
    playback(); at(t,1,1); jam_emu_key(7,1); steps_clear(t); blocks(2);
    CHECK(gated(t,60), "pattern clear preserves the same pitch physically held on the keyboard");
    jam_emu_key(7,0); blocks(2); CHECK(!gated(t,60), "cleared pattern cannot retain the pitch after its last live release");
    t=fresh(2,V_POLY,16); put(t,4,60,8);
    playback(); at(t,5,1); CHECK(gated(t,60), "late-pattern note is active before a transport rewind");
    transport_req=3; blocks(2);
    CHECK(song.playing && !gated(t,60), "START rewind to an empty first step releases prior independent notes");
}
static void record_follow_and_sustain(void)
{
    track_t *t=fresh(2,V_POLY,16); start_record(t);
    at(t,0,1); jam_emu_key(7,1); at(t,3,1); track_select(1);
    CHECK(song.rec==(1u<<1) && duration(t,0,60)==3, "moving the record arm finalizes the previous track at its quantized endpoint");
    CHECK(gated(t,60), "record-track change preserves a key physically held on its original part");
    jam_emu_key(7,0); blocks(2); CHECK(!gated(t,60), "old track's key releases after record follows another track");

    t=fresh(2,V_POLY,16); start_record(t);
    at(t,0,1); jam_emu_key(7,1);
    at(t,1,1); sus_set(t,1); jam_emu_key(7,0);
    at(t,2,1); jam_emu_key(11,1);
    at(t,4,1); jam_emu_key(11,0);
    CHECK(gated(t,60) && gated(t,64), "sustain pedal holds both independently started notes after key release");
    at(t,5,1); sus_set(t,0); blocks(2);
    CHECK(duration(t,0,60)==5 && duration(t,2,64)==3 && !gated_total(t),
          "pedal-up records each note's own duration and releases both live owners");
}
static void preset_midi_and_division(void)
{
    track_t *t=fresh(2,V_POLY,16); put(t,0,60,8);
    song.g[G_MIDI]=1; playback(); at(t,1,1);
    CHECK((mo_set[2][60>>5]>>(60&31))&1u, "independent sequencer note is registered for MIDI output");
    panic_req=1u<<2; blocks(2);
    CHECK(!((mo_set[2][60>>5]>>(60&31))&1u), "preset/program panic ends external MIDI notes before forgetting their independent owners");
    t=fresh(2,V_POLY,16); put(t,0,60,16);
    playback(); at(t,4,1); t->p[P_SDIV]=0; blocks(2);
    CHECK(!gated(t,60), "division changes release endpoints expressed on the prior grid");
    t=fresh(2,V_POLY,16); put(t,8,60,16);
    playback(); at(t,10,1); t->p[P_SLEN]=4;
    at(t,11,1); CHECK(gated(t,60), "shortened loop retains a late onset until its bounded new endpoint");
    at(t,12,1); CHECK(!gated(t,60), "shortened loop clips an active note to the new loop-length bound");
}
static void free_take_quantized_duplicates(void)
{
    track_t *t=fresh(2,V_POLY,16);
    /* Valid overlapping input events that quantize to the same pitch/onset slot. */
    ft_on=1; ft_trk=2; ft_n=2; ft_t=FS*2u/CTL;
    ft_ev[0].t=0; ft_ev[0].d=(uint16_t)(ft_t/2u); ft_ev[0].note=60; ft_ev[0].vel=100;
    ft_ev[1].t=1; ft_ev[1].d=1; ft_ev[1].note=60; ft_ev[1].vel=100;
    ft_close();
    CHECK(song.playing && t->p[P_SLEN]==16 && duration(t,0,60)==8,
          "free-take duplicate onsets retain the longest overlapping duration when quantized together");
    blocks((FS*350u/1000u+CTL-1u)/CTL); /* Free-take closing gesture owns PLAY/REC for 300 ms. */
}
static void mixed_track_overdub_and_arp_source(void)
{
    track_t *t=fresh(2,V_POLY,16); put(t,0,60,2);
    step_add(t,8,67,100,LV_NORM,0);
    for (uint32_t i=9;i<14;i++) t->step[i].time=ST_TIE;
    playback(); at(t,10,1); tap(B_REC); jam_emu_key(11,1);
    at(t,12,1); jam_emu_key(11,0);
    CHECK(duration(t,8,67)==6 && duration(t,10,64)==2,
          "recording converts a remaining legacy hold even on a track with other explicit notes");
    playback(); at(t,10,1);
    CHECK(gated(t,67) && gated(t,64), "mixed-track overdub preserves both the legacy hold and later onset on replay");
    at(t,12,1); CHECK(gated(t,67) && !gated(t,64), "mixed-track converted hold outlasts the new short note");
    t=fresh(2,V_POLY,16); put(t,2,60,4);
    t->p[P_AMODE]=1; t->p[P_AOCT]=2; t->p[P_ARATE]=0; t->p[P_AGATE]=64;
    playback(); at(t,0,1); jam_emu_key(7,1);
    at(t,2,1);
    CHECK(gated(t,60), "a held arp source key cannot suppress a sequenced pitch after its arp gate ends");
    jam_emu_key(7,0); blocks(2);
    CHECK(gated(t,60), "releasing the arp source preserves the active independent sequencer owner");
}
int main(void)
{
    jam_emu_init();
    mixed_sequence_owners(); clear_and_restart(); record_follow_and_sustain();
    preset_midi_and_division(); free_take_quantized_duplicates(); mixed_track_overdub_and_arp_source();
    jam_emu_panic(); blocks(2);
    printf("Independent duration ownership: %u checks passed.\n", checks);
    return 0;
}
