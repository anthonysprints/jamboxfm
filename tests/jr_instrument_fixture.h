/* SPDX-License-Identifier: GPL-3.0-only */
/* Host-only bridge between the real instrument and the experimental codec.
 * This is deliberately NOT included by felucca.c or the device build. In
 * particular, the FM6 section sidecars below are a fixture, not a runtime fix. */
#ifndef JR_INSTRUMENT_FIXTURE_H
#define JR_INSTRUMENT_FIXTURE_H

static void jif_w8(uint8_t **p, uint32_t v) { *(*p)++ = (uint8_t)v; }
static void jif_w16(uint8_t **p, uint32_t v)
{ jif_w8(p, v); jif_w8(p, v >> 8); }
static void jif_w32(uint8_t **p, uint32_t v)
{ jif_w16(p, v); jif_w16(p, v >> 16); }
static uint32_t jif_r8(const uint8_t **p) { return *(*p)++; }
static uint32_t jif_r16(const uint8_t **p)
{ uint32_t a = jif_r8(p); return a | jif_r8(p) << 8; }
static uint32_t jif_r32(const uint8_t **p)
{ uint32_t a = jif_r16(p); return a | jif_r16(p) << 16; }

/* Persist fields, not the host compiler's struct layout. The JRM1 checksum is
 * calculated over the explicit little-endian bytes. The experiment's codec
 * separately checks parameter semantics and all source dependencies. */
static void jif_project_encode(uint8_t out[4584], const project_t *q)
{
    uint8_t *p = out;
    jif_w32(&p, PROJ_MAGIC); jif_w32(&p, 4584);
    for (uint32_t i = 0; i < PROJ_NG; ++i) jif_w16(&p, (uint16_t)q->g[i]);
    jif_w8(&p, q->sel); jif_w8(&p, q->drdly);
    for (uint32_t i = 0; i < 2; ++i) jif_w8(&p, q->rsv[i]);
    for (uint32_t t = 0; t < NTRK; ++t) {
        const proj_trk_t *s = &q->t[t];
        for (uint32_t i = 0; i < PROJ_NC6; ++i) jif_w16(&p, (uint16_t)s->p[i]);
        for (uint32_t i = 0; i < 8; ++i) jif_w16(&p, (uint16_t)s->e[i]);
        for (uint32_t i = 0; i < PROJ_NX6; ++i) jif_w8(&p, s->x[i]);
        jif_w8(&p, s->engine); jif_w8(&p, s->preset);
        for (uint32_t i = 0; i < NSTEP; ++i) {
            if (t == TRK_DRUM) {
                const dstep_t *s0 = &s->dstep[i];
                for (uint32_t n = 0; n < 2; ++n) jif_w8(&p, s0->on[n]);
                for (uint32_t n = 0; n < 4; ++n) jif_w8(&p, s0->lvl[n]);
                for (uint32_t n = 0; n < 4; ++n) jif_w8(&p, s0->rat[n]);
            } else {
                const step_t *s0 = &s->step[i];
                for (uint32_t n = 0; n < 4; ++n) jif_w8(&p, s0->note[n]);
                jif_w8(&p, s0->n); jif_w8(&p, s0->time);
                jif_w8(&p, s0->flags); jif_w8(&p, s0->vel);
                jif_w8(&p, s0->lvl); jif_w8(&p, s0->rat);
            }
        }
        for (uint32_t i = 0; i < NSTEP; ++i) jif_w8(&p, s->mf[i]);
        for (uint32_t i = 0; i < NLOCK; ++i) {
            jif_w8(&p, s->lock[i].step); jif_w8(&p, s->lock[i].param);
            jif_w16(&p, (uint16_t)s->lock[i].val);
        }
    }
    for (uint32_t t = 0; t < NPART; ++t)
        for (uint32_t i = 0; i < NSTEP; ++i)
            for (uint32_t n = 0; n < 4; ++n) jif_w8(&p, q->note_len[t][i][n]);
    assert((size_t)(p - out) == 4580);
    jif_w32(&p, proj_hash(out, 4580));
    assert((size_t)(p - out) == 4584);
}

static void jif_project_decode(project_t *q, const uint8_t in[4584])
{
    const uint8_t *p = in;
    memset(q, 0, sizeof *q);
    q->magic = jif_r32(&p); q->size = jif_r32(&p);
    for (uint32_t i = 0; i < PROJ_NG; ++i) q->g[i] = (int16_t)jif_r16(&p);
    q->sel = (uint8_t)jif_r8(&p); q->drdly = (uint8_t)jif_r8(&p);
    for (uint32_t i = 0; i < 2; ++i) q->rsv[i] = (uint8_t)jif_r8(&p);
    for (uint32_t t = 0; t < NTRK; ++t) {
        proj_trk_t *s = &q->t[t];
        for (uint32_t i = 0; i < PROJ_NC6; ++i) s->p[i] = (int16_t)jif_r16(&p);
        for (uint32_t i = 0; i < 8; ++i) s->e[i] = (int16_t)jif_r16(&p);
        for (uint32_t i = 0; i < PROJ_NX6; ++i) s->x[i] = (uint8_t)jif_r8(&p);
        s->engine = (uint8_t)jif_r8(&p); s->preset = (uint8_t)jif_r8(&p);
        for (uint32_t i = 0; i < NSTEP; ++i) {
            if (t == TRK_DRUM) {
                dstep_t *s0 = &s->dstep[i];
                for (uint32_t n = 0; n < 2; ++n) s0->on[n] = (uint8_t)jif_r8(&p);
                for (uint32_t n = 0; n < 4; ++n) s0->lvl[n] = (uint8_t)jif_r8(&p);
                for (uint32_t n = 0; n < 4; ++n) s0->rat[n] = (uint8_t)jif_r8(&p);
            } else {
                step_t *s0 = &s->step[i];
                for (uint32_t n = 0; n < 4; ++n) s0->note[n] = (uint8_t)jif_r8(&p);
                s0->n = (uint8_t)jif_r8(&p); s0->time = (uint8_t)jif_r8(&p);
                s0->flags = (uint8_t)jif_r8(&p); s0->vel = (uint8_t)jif_r8(&p);
                s0->lvl = (uint8_t)jif_r8(&p); s0->rat = (uint8_t)jif_r8(&p);
            }
        }
        for (uint32_t i = 0; i < NSTEP; ++i) s->mf[i] = (uint8_t)jif_r8(&p);
        for (uint32_t i = 0; i < NLOCK; ++i) {
            s->lock[i].step = (uint8_t)jif_r8(&p); s->lock[i].param = (uint8_t)jif_r8(&p);
            s->lock[i].val = (int16_t)jif_r16(&p);
        }
    }
    for (uint32_t t = 0; t < NPART; ++t)
        for (uint32_t i = 0; i < NSTEP; ++i)
            for (uint32_t n = 0; n < 4; ++n) q->note_len[t][i][n] = (uint8_t)jif_r8(&p);
    (void)jif_r32(&p);
    assert((size_t)(p - in) == 4584);
    /* Runtime checksum is private and host-layout-specific, rebuilt only after
     * the wire content has passed full semantic validation. */
    q->sum = proj_sum(q);
}

static void jif_syn_encode(uint8_t out[1464], const dsu_bank_t *b)
{
    uint8_t *p = out;
    jif_w32(&p, DSU_MAGIC); jif_w16(&p, 1); jif_w16(&p, DSU_N);
    for (uint32_t k = 0; k < DSU_N; ++k) {
        const dsu_kit_t *u = &b->k[k];
        for (uint32_t i = 0; i < 8; ++i) jif_w8(&p, (uint8_t)u->name[i]);
        jif_w8(&p, u->crush); jif_w8(&p, u->src); jif_w16(&p, 0);
        for (uint32_t n = 0; n < DS_LANES; ++n) {
            const dsnd_t *s = &u->s[n];
            jif_w8(&p, s->wave); jif_w8(&p, s->src); jif_w8(&p, s->pitch);
            jif_w8(&p, s->fine); jif_w8(&p, s->bend); jif_w8(&p, s->btime);
            jif_w8(&p, s->hold); jif_w8(&p, s->decay); jif_w8(&p, s->tlev);
            jif_w8(&p, s->t2); jif_w8(&p, s->t2lev); jif_w8(&p, s->click);
            jif_w8(&p, s->nlev); jif_w8(&p, s->nhold); jif_w8(&p, s->ndec);
            jif_w8(&p, s->flt); jif_w8(&p, s->fcut); jif_w8(&p, (uint8_t)s->fenv);
            jif_w8(&p, s->hpf); jif_w8(&p, s->chip); jif_w8(&p, s->drive); jif_w8(&p, s->level);
        }
    }
    assert((size_t)(p - out) == 1464);
}

static void jif_syn_decode(dsu_bank_t *b, const uint8_t in[1464])
{
    const uint8_t *p = in;
    memset(b, 0, sizeof *b);
    b->magic = jif_r32(&p); b->ver = (uint16_t)jif_r16(&p); b->n = (uint16_t)jif_r16(&p);
    for (uint32_t k = 0; k < DSU_N; ++k) {
        dsu_kit_t *u = &b->k[k];
        for (uint32_t i = 0; i < 8; ++i) u->name[i] = (char)jif_r8(&p);
        u->crush = (uint8_t)jif_r8(&p); u->src = (uint8_t)jif_r8(&p); (void)jif_r16(&p);
        for (uint32_t n = 0; n < DS_LANES; ++n) {
            dsnd_t *s = &u->s[n];
            s->wave = (uint8_t)jif_r8(&p); s->src = (uint8_t)jif_r8(&p); s->pitch = (uint8_t)jif_r8(&p);
            s->fine = (uint8_t)jif_r8(&p); s->bend = (uint8_t)jif_r8(&p); s->btime = (uint8_t)jif_r8(&p);
            s->hold = (uint8_t)jif_r8(&p); s->decay = (uint8_t)jif_r8(&p); s->tlev = (uint8_t)jif_r8(&p);
            s->t2 = (uint8_t)jif_r8(&p); s->t2lev = (uint8_t)jif_r8(&p); s->click = (uint8_t)jif_r8(&p);
            s->nlev = (uint8_t)jif_r8(&p); s->nhold = (uint8_t)jif_r8(&p); s->ndec = (uint8_t)jif_r8(&p);
            s->flt = (uint8_t)jif_r8(&p); s->fcut = (uint8_t)jif_r8(&p); s->fenv = (int8_t)jif_r8(&p);
            s->hpf = (uint8_t)jif_r8(&p); s->chip = (uint8_t)jif_r8(&p); s->drive = (uint8_t)jif_r8(&p); s->level = (uint8_t)jif_r8(&p);
        }
    }
    assert((size_t)(p - in) == 1464);
}

/* Future runtime integration must make these sidecars part of section_store,
 * arrangement_apply and song_backup/restore. Test-local overrides demonstrate
 * the required order and prove that fm6_poll cannot overwrite a restored edit. */
static void jif_apply_patch_sidecar(const uint8_t patch[NPART][156])
{
    for (uint32_t t = 0; t < NPART; ++t) {
        if (trk[t].eng_req != ENGI_FM6) continue;
        fm6_set_patch(t, patch[t]);
        fm6_slot[t] = (uint8_t)trk[t].p[P_E7];
        fm6_pend &= (uint8_t)~(1u << t);
    }
    fm6_poll();
}

#endif
