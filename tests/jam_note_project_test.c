/* SPDX-License-Identifier: GPL-3.0-only */
/* Real project/arranger/editor round trips; no target build or device access. */
#define JAM_STORAGE_LIBRARY 1
#include "jam_storage_harness.c"

static void seed_notes(void)
{
    for (uint32_t t = 0; t < NTRK; t++) steps_clear(&trk[t]);
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_SLEN] = 16;
    trk[0].step[0] = (step_t){.note={60,64},.n=2,.time=ST_NOTE,.vel=91,.lvl=4,.rat=4};
    trk[0].step[1] = (step_t){.note={67},.n=1,.time=ST_NOTE,.vel=87};
    trk[0].step[0].flags = SF_ACCENT;
    trk[0].note_len[0][0] = 8;
    trk[0].note_len[0][1] = 2;
    trk[0].note_len[1][0] = 4;
    trk[0].step[48] = (step_t){.note={72},.n=1,.time=ST_NOTE};
    trk[0].note_len[48][0] = 64;
    trk[0].micro[1] = -7;
    trk[0].lock[0] = (plock_t){.step=1,.param=P_PAN,.val=-12};
    step_fill_set(&trk[0], 1, FC_FILL);
    trk[1].step[5] = (step_t){.note={41},.n=1,.time=ST_NOTE};
    trk[1].note_len[5][0] = 13;
    trk[2].step[63] = (step_t){.note={77},.n=1,.time=ST_NOTE};
    trk[2].note_len[63][0] = 17;
    for (uint32_t i=0;i<NPART;i++) jam_lengths_changed(&trk[i]);
}
int main(void)
{
    project_t q, parsed, untouched;
    project_v6_t old;
    memset(nor, 255, sizeof nor); ordinary_boot(); seed_notes();
    check(sizeof(project_t)==4584 && sizeof(project_v6_t)==3816 && sizeof(step_t)==10,
          "JRM1 grows only its project tail; legacy format and step layout stay fixed");
    check(project_slot(0)!=project_slot(1) && project_slot(1)!=project_slot(2) &&
          project_slot(2)!=project_slot(3) && project_slot(3)==project_slot(7),
          "all four split RAM slots are distinct with the same bounded slot addressing");
    proj_capture(&q);
    check(q.magic==0x4A524D31u && q.size==sizeof q && proj_ok(&q), "captured JRM1 has its own magic, size and checksum");
    check(q.note_len[0][0][0]==8 && q.note_len[0][0][1]==2 && q.note_len[0][1][0]==4,
          "capture keeps staggered onset lengths independently");
    check(q.note_len[0][48][0]==64, "capture keeps hidden-step durations for later loop expansion");
    check(q.note_len[1][5][0]==13 && q.note_len[2][63][0]==17,
          "capture stores separate duration tables for every melodic part");
    memset(&parsed,0xAA,sizeof parsed);
    check(proj_import(&parsed,&q,sizeof q) && !memcmp(&parsed,&q,sizeof q), "whole-project byte round trip preserves all duration tables");
    q.note_len[0][0][1] ^= 1;
    untouched=parsed;
    check(!proj_import(&parsed,&q,sizeof q) && !memcmp(&parsed,&untouched,sizeof parsed), "duration-tail corruption rejects without modifying the destination");
    q=untouched;
    check(!proj_import(&parsed,&q,sizeof q-1) && !proj_import(&parsed,&q,sizeof q+1), "JRM1 rejects truncated or oversized project bytes");
    memset(trk[0].note_len,0,sizeof trk[0].note_len); trk[0].duration_mode=0;
    proj_apply(&q,1);
    check(trk[0].duration_mode && trk[0].note_len[0][0]==8 && trk[0].note_len[0][1]==2 && trk[0].note_len[1][0]==4,
          "apply restores independent lengths and its playback-mode cache");
    check(trk[1].note_len[5][0]==13 && trk[2].note_len[63][0]==17 && trk[1].duration_mode && trk[2].duration_mode,
          "apply restores each melodic part without sharing or shifting duration tables");
    check(trk[0].step[0].vel==91 && trk[0].step[0].flags==SF_ACCENT && trk[0].step[0].lvl==4 && trk[0].step[0].rat==4 &&
          trk[0].micro[1]==-7 && trk[0].lock[0].param==P_PAN && step_fill(&trk[0],1)==FC_FILL,
          "duration round trip preserves dynamics, ratchets, nudges, locks and conditions");
    q.note_len[0][0][0]=255; q.note_len[0][0][3]=5; q.note_len[0][2][0]=7;
    memset(trk[TRK_DRUM].note_len,255,sizeof trk[TRK_DRUM].note_len); trk[TRK_DRUM].duration_mode=1;
    proj_apply(&q,1);
    check(!trk[0].note_len[0][0] && !trk[0].note_len[0][3] && !trk[0].note_len[2][0] && trk[0].note_len[0][1]==2,
          "apply discards malformed, unused-slot and non-onset durations");
    check(all_zero(trk[TRK_DRUM].note_len,sizeof trk[TRK_DRUM].note_len) && !trk[TRK_DRUM].duration_mode,
          "drum tracks never inherit melodic duration metadata");
    seed_notes();
    for (uint32_t slot=0;slot<4;slot++) {
        trk[0].note_len[0][0]=(uint8_t)(slot+3); song.g[G_BPM]=(int16_t)(100+slot); section_store(slot);
    }
    check(arrangement_ready()==15, "all four larger snapshots remain available to the arranger");
    for (uint32_t slot=0;slot<4;slot++) {
        proj_apply(project_slot(slot),1);
        check(trk[0].note_len[0][0]==slot+3 && song.g[G_BPM]==100+(int)slot,
              "each RAM scene restores its own durations and project values");
    }
    trk[0].note_len[0][0]=11; song_backup(); arrangement_apply(3);
    check(trk[0].note_len[0][0]==6, "arranger application loads the fourth split snapshot");
    song_restore(); check(trk[0].note_len[0][0]==11, "song stop restores the independent lengths of the working loop");
    proj_capture(&q); memset(&old,0,sizeof old);
    memcpy(&old,&q,__builtin_offsetof(project_v6_t,sum)); old.magic=PROJ_MAGIC_JRM0; old.size=sizeof old;
    old.sum=proj_hash(&old,sizeof old-4u);
    check(!proj_import(&parsed,&old,sizeof old), "ordinary import does not silently migrate legacy RAM state");
    check(proj_import_legacy(&parsed,&old,sizeof old) && proj_ok(&parsed) &&
          all_zero(parsed.note_len,sizeof parsed.note_len) && !memcmp(parsed.t,old.t,sizeof old.t),
          "explicit JRM0 conversion keeps legacy notes and initializes all independent lengths to zero");
    old.magic=PROJ_MAGIC_V6; old.sum=proj_hash(&old,sizeof old-4u);
    check(proj_import_legacy(&parsed,&old,sizeof old) && proj_ok(&parsed), "explicit FUN6 conversion is separate from ordinary family-restricted import");
    old.sum^=1; untouched=parsed;
    check(!proj_import_legacy(&parsed,&old,sizeof old) && !memcmp(&parsed,&untouched,sizeof parsed), "explicit legacy conversion validates checksum before mutation");
    proj_apply(&parsed,1);
    check(!trk[0].duration_mode && all_zero(trk[0].note_len,sizeof trk[0].note_len), "applying imported legacy content selects legacy group articulation");
    seed_notes(); ed_begin(ED_STEP_GET); ed_step_out(&trk[0],0);
    uint8_t body[15]; memcpy(body,ed_out+5,sizeof body);
    check(ED_PROTO==11 && ed_n==20 && body[11]==8 && body[12]==2 && body[13]==0 && body[14]==0,
          "JamBox editor protocol advertises and exports the four optional duration bytes");
    memset(trk[0].note_len[0],0,4); ed_step_in(&trk[0],0,body,15);
    check(trk[0].note_len[0][0]==8 && trk[0].note_len[0][1]==2 && trk[0].duration_mode,
          "extended editor step replacement round trips independent note lengths");
    uint32_t sig=ed_step_sig(&trk[0],0); trk[0].note_len[0][0]++;
    check(ed_step_sig(&trk[0],0)!=sig, "duration-only edits change the editor watch signature");
    body[11]=127; body[12]=64; body[13]=9; ed_step_in(&trk[0],0,body,15);
    check(!trk[0].note_len[0][0] && trk[0].note_len[0][1]==64 && !trk[0].note_len[0][2],
          "editor ignores malformed lengths and durations outside valid note slots");
    ed_step_in(&trk[0],0,body,11);
    check(all_zero(trk[0].note_len[0],4) && trk[0].note_len[1][0]==4 && trk[0].duration_mode,
          "legacy editor replacement clears only the replaced step's stale lengths");
    body[5]=ST_REST; ed_step_in(&trk[0],0,body,15);
    check(all_zero(trk[0].note_len[0],4), "editor cannot attach lengths to a rest");
    for (uint32_t i=0;i<NSTEP;i++) memset(trk[0].note_len[i],0,4);
    ed_step_in(&trk[0],0,body,11);
    check(!trk[0].duration_mode, "clearing the final explicit length returns the track to legacy mode");
    ed_begin(ED_STEP_GET); ed_step_out(TDRUM,0);
    check(ed_n==16, "drum editor payload remains the legacy eleven-byte format");
    printf("JamBox note project/editor: %u checks, %u failures\n",checks,failures);
    return failures ? 1 : 0;
}
