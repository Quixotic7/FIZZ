/* SPDX-License-Identifier: GPL-3.0-only */
/* P2.1: what a flash image holds, as the core reads it: every project (its header's state, its 64 patterns, its pool
 * of 128 sounds, its MIDI FX groups), MY presets (256), the factory tags, the settings; one line each, a hash of its
 * fields (field by field: no struct padding). The owner's projects must survive a store format's change: dump the
 * same image with the core before and after it (this file builds in either: the older one has the arrays, P2.1 the
 * read API) and compare the two texts.
 *
 *     flash_dump IMAGE [--migrate OUT]
 *
 * The image is read into RAM and never written back. --migrate (P2.1's core): every pattern record rewritten in the
 * newest format (qd_persist_migrate), the RAM image then written to OUT (a new file) and dumped again from it, after a
 * restart. Loading a project writes to the RAM copy (the one before is flushed, its pool's old records made
 * references), as the Standalone would. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "persist.h"
#include "qd_fx.h"
#include "sounds.h"

#ifdef QD_SK_LIB
#define FD_NEW 1                                        /* (P2.1's core: the read API, the patterns paged) */
#endif

#define FD_SIZE 0x100000u
static uint8_t *fl;
static int fl_read(void *c, uint32_t off, void *dst, uint32_t n) { (void)c; memcpy(dst, fl + off, n); return 0; }
static int fl_erase(void *c, uint32_t off) { (void)c; memset(fl + (off & ~4095u), 0xFF, 4096); return 0; }
static int fl_prog(void *c, uint32_t off, const void *src, uint32_t n)
{
    uint32_t i;
    (void)c;
    for (i = 0; i < n; i++)
        fl[off + i] &= ((const uint8_t *)src)[i];
    return 0;
}

static uint32_t h_;
static void H0(void) { h_ = 2166136261u; }
static void H(const void *p, size_t n)
{
    const uint8_t *b = (const uint8_t *)p;
    while (n--)
        h_ = (h_ ^ *b++) * 16777619u;
}
#define HV(v) H(&(v), sizeof(v))

static void snd_hash(const qd_sound_t *s)
{
    H(s->name, QD_SOUND_NAME);
    H(s->patch, sizeof s->patch);
    HV(s->preset), HV(s->tags), HV(s->flags), HV(s->pan), HV(s->chorus), HV(s->delay), HV(s->reverb);
}
static void tsnd_hash(const qd_tsound_t *s)
{
    H(s->patch, sizeof s->patch);
    HV(s->sound), HV(s->edited);
    H(s->name, QD_SOUND_NAME);
    HV(s->preset);
}
static uint32_t pat_hash(const qd_pattern_t *p)
{
    int t;
    H0();
    HV(p->master), HV(p->swing), HV(p->used), HV(p->bpm), HV(p->nlock);
    H(p->trk, sizeof p->trk);                           /* (the records keep these as they are) */
    H(p->lock_start, sizeof p->lock_start), H(p->lock_n, sizeof p->lock_n);
    {   /* (P2.1c: the steps as the records keep them: their 20-byte layout, as the older cores' dumps hash them) */
        int s;
        for (t = 0; t < QD_PAT_NTRACK; t++)
            for (s = 0; s < QD_NSTEP; s++) {
                uint8_t w[QD_STEP_WIRE];
                qd_step_wire(&p->step[t][s], w);
                H(w, QD_STEP_WIRE);
            }
    }
    H(p->lock, (size_t)p->nlock * sizeof p->lock[0]);
    for (t = 0; t < QD_PAT_NTRACK; t++)
        tsnd_hash(&p->snd[t]);
    return h_;
}

static const qd_pattern_t *pat_of(qd_app_t *a, int k)
{
#ifdef FD_NEW
    return qd_proj_cpat(&a->proj, k);
#else
    return &a->proj.pat[k];
#endif
}
static void pool_of(qd_app_t *a, int k, qd_sound_t *s)
{
#ifdef FD_NEW
    qd_sound_read(&a->proj, k, s);
#else
    *s = a->proj.sound[k - 1];
#endif
}
static void lib_of(qd_app_t *a, int i, qd_sound_t *s)
{
#ifdef FD_NEW
    qd_lib_read(a, i, s);
#else
    *s = a->per.lib[i];
#endif
}

static void dump_project(qd_app_t *a, FILE *o)
{
    const int p = a->per.proj;
    int k, t;
    H0();
    H(a->per.pname[p], QD_PROJ_NAME);
    HV(a->per.pdemo), HV(a->proj.key_scale), HV(a->proj.key_root), HV(a->proj.launch), HV(a->proj.rec_mode);
    HV(a->proj.rec_quant), HV(a->proj.rec_dub), HV(a->proj.rec_ekeys), HV(a->proj.pool_empty), HV(a->proj.rec_auto);
    for (t = 0; t < QD_NTRACK; t++) {
        const qd_track_t *k2 = &a->model.t[t];
        HV(k2->mix.level), HV(k2->mix.pan), HV(k2->mix.chorus), HV(k2->mix.delay), HV(k2->mix.reverb);
        HV(k2->limit), HV(k2->reserve), HV(k2->kind), HV(k2->named);
        H(k2->name, QD_TRACK_NAME);
    }
    fprintf(o, "project %d \"%s\" header %08x nsound %d\n", p + 1, a->per.pname[p], h_, a->proj.nsound);
    for (k = 0; k < QD_NPAT; k++) {
        const qd_pattern_t *P = pat_of(a, k);
        fprintf(o, "project %d pattern %d used %d %08x\n", p + 1, k + 1, P->used, pat_hash(P));
    }
    for (k = 1; k <= QD_NSOUND; k++) {
        qd_sound_t s;
        pool_of(a, k, &s);
        if (!s.name[0])
            continue;
        H0();
        snd_hash(&s);
        fprintf(o, "project %d pool %d \"%s\" preset %d %08x\n", p + 1, k, s.name, s.preset, h_);
    }
    H0();
    H(a->proj.mfx, sizeof a->proj.mfx);
    fprintf(o, "project %d midi-fx %08x\n", p + 1, h_);
}

static void dump(qd_app_t *a, FILE *o)
{
    int p, i, first = a->per.proj;
    const int cur = a->seq.cur, sel = a->sel;
    H0();
    for (i = 0; i < QD_NFXP; i++) {
        const int16_t v = (int16_t)qd_fx_get(i);
        HV(v);
    }
    fprintf(o, "settings project %d pattern %d track %d knobs %d view %d follow %d lmode %d effects %08x\n",
            a->per.proj + 1, a->seq.cur + 1, a->sel + 1, a->ui.knobs, a->ui.view, a->ui.follow, a->ui.lmode, h_);
    H0();
    H(a->per.ftags, sizeof a->per.ftags);
    HV(a->per.id_next);
    fprintf(o, "factory-tags %08x\n", h_);
    for (i = 0; i < QD_NLIB; i++) {
        qd_sound_t s;
        lib_of(a, i, &s);
        if (!s.name[0])
            continue;
        H0();
        snd_hash(&s);
        fprintf(o, "mine %d \"%s\" preset %d %08x\n", i + 1, s.name, s.preset, h_);
    }
    dump_project(a, o);                                 /* (the one worked on first: as it powers on) */
    for (p = 0; p < QD_NPROJ; p++)
        if (p != first && a->per.pexists[p] && qd_proj_load(a, p))
            dump_project(a, o);
    if (a->per.proj != first) {                         /* (back as it was: the project, its pattern, the track) */
        qd_proj_load(a, first);
        qd_seq_select(&a->seq, cur, 1);
        qd_ctl_select_track(a, sel);
    }
}

static void boot(qd_app_t *a)
{
    static const qd_flash_t F = {fl_read, fl_erase, fl_prog, 0};
    qd_app_init(a);
    qd_app_tick(a, 0);
    qd_persist_attach(a, &F);
}

int main(int argc, char **argv)
{
    static qd_app_t A;
    FILE *f;
    if (argc < 2) {
        fprintf(stderr, "usage: flash_dump IMAGE [--migrate OUT]\n");
        return 2;
    }
    fl = (uint8_t *)malloc(FD_SIZE);
    f = fopen(argv[1], "rb");
    if (!fl || !f || fread(fl, 1, FD_SIZE, f) != FD_SIZE) {
        fprintf(stderr, "flash_dump: cannot read %s (1 MiB)\n", argv[1]);
        return 1;
    }
    fclose(f);
    boot(&A);
    if (!A.per.on) {
        fprintf(stderr, "flash_dump: %s does not mount\n", argv[1]);
        return 1;
    }
    dump(&A, stdout);
#ifdef FD_NEW
    if (argc >= 4 && !strcmp(argv[2], "--migrate")) {
        int n = qd_persist_migrate(&A);
        qd_persist_flush(&A);
        f = fopen(argv[3], "wb");
        if (!f || fwrite(fl, 1, FD_SIZE, f) != FD_SIZE) {
            fprintf(stderr, "flash_dump: cannot write %s\n", argv[3]);
            return 1;
        }
        fclose(f);
        printf("---- migrated: %d pattern records rewritten; after a restart\n", n);
        boot(&A);
        dump(&A, stdout);
    }
#endif
    return 0;
}
