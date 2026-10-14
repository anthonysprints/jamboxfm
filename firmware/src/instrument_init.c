/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* power-on: three parts with their default sounds (TRK_DEF), the drum track, empty patterns */
static void felucca_init(void)
{
    uint32_t i;
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    fm6_init();                               /* every part's FM6 patch: the init voice */
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        track_defaults(t);
        if (i < NPART) {
            set_engine_of(t, TRK_DEF[i][0]);
            apply_preset_to(t, TRK_DEF[i][1]);   /* with its sends */
            t->engine = t->eng_req;
        }
        track_defaults_steps(t);              /* the sequencers start empty */
    }
    TDRUM->p[P_E0] = DRUM_DEFAULT_KIT;        /* the 808 kit */
    song.sel = 0;
    song.master_q12 = 2048;
#if JAMBOX_PROTO
    jam_new_dusty();                          /* fresh RAM session; no foreign autosave import */
#else
    autosave_resume();                        /* the project as it was left (project.c) */
#endif
    song.g[G_SYNC] = (int16_t)lights_sync;    /* a setting of the FM-1 (panel.c) */
    song.g[G_MIDI] = (int16_t)lights_mout;
    song.g[G_ROUTE] = (int16_t)lights_min;
    song.g[G_CH1] = midi_set[0], song.g[G_CH2] = midi_set[1], song.g[G_CH3] = midi_set[2];   /* (2.6: GLO > MIDI, */
    song.g[G_DRCH] = midi_set[3], song.g[G_CHOTH] = midi_set[4], song.g[G_BEND] = midi_set[5];   /* settings of the FM-1) */
    layers_init();                            /* the panel's layer buttons for the keys (ui_layers.c) */
#if JAMBOX_PROTO
    ui.jam_home_view = JAM_HOME_GROOVE;
#endif
    go_home();
    ui.force = 1;
}
