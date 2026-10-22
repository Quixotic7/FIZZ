/* SPDX-License-Identifier: GPL-3.0-only */
/* Saving (persist.h): the records, written by themselves; the projects, the pool, the library. Built through app.c
 * (it includes this file and core/store/store.c: the core's source lists stay as they are).
 *
 * The records' bytes (version 1; the host's byte order and struct layouts: a Phase 2 format would spell them out):
 *   SET  'G' 1, project, pattern, track, knob layout, view, page follow, the effects' parameters (QD_NFXP int16),
 *        the latch mode (P1.5b), 1 (P1.5s: the view is the user's; a record without it powers on in EDIT)
 *   HDR  'H' 1, name (QD_PROJ_NAME), demo, global key scale and root, launch, record mode, quantize, overdub, each
 *        track's mix (level, pan, chorus, delay, reverb) and voice limit and reserve
 *   PAT  'P' 2, the step's and the track settings' sizes, master, swing, used, bpm, nlock, the eight track settings,
 *        lock_start, lock_n; per track a page mask, per page in it a step mask and the steps set (non-empty); the
 *        locks; the eight track sounds (each its packed patch (96: quad_voice.h, DRIVE with it), its pool source,
 *        edited, its name, P1.5t: its preset id (u16)). 'P' 1 (0.6): no preset id (read from its pool source, else a
 *        factory sound of that name and patch)
 *   SND / LIB  'S' 2, the name, the packed patch, P1.5t: the preset id (u16), the kind, the flags; 'S' 1 (0.6): the
 *        name and the packed patch (a library sound becomes a MY preset, id QD_PRESET_MINE0 + its place; a pool
 *        sound remembers the factory preset of its name and patch); or 'E' 1: an empty slot
 *   HDR  (P1.5t) one more byte: 1, the project's pool starts empty (a project of 0.6 has none: its pool's absent
 *        records are the old starting pool) */
#include "../voice/fx/qd_fx.h"

static uint8_t pr_buf[QDS_MAXREC], pr_buf2[QDS_MAXREC];

static uint32_t fnv(const uint8_t *p, uint32_t n)
{
    uint32_t h = 2166136261u, i;
    for (i = 0; i < n; i++)
        h = (h ^ p[i]) * 16777619u;
    return h ? h : 1u;
}

static int empty_step(const qd_step_t *s)
{
    static const qd_step_t z;
    return !memcmp(s, &z, sizeof z);
}

/* ---------------------------------------------------------------- the records' bytes */
static uint32_t ser_pat(const qd_pattern_t *p, uint8_t *b)
{
    uint32_t o = 0;
    int t, pg, s;
    b[o++] = 'P', b[o++] = 2;
    b[o++] = (uint8_t)sizeof(qd_step_t), b[o++] = (uint8_t)sizeof(qd_tset_t);
    memcpy(b + o, &p->master, 2), o += 2;
    b[o++] = p->swing, b[o++] = p->used;
    memcpy(b + o, &p->bpm, 2), o += 2;
    memcpy(b + o, &p->nlock, 2), o += 2;
    memcpy(b + o, p->trk, sizeof p->trk), o += sizeof p->trk;
    memcpy(b + o, p->lock_start, sizeof p->lock_start), o += sizeof p->lock_start;
    memcpy(b + o, p->lock_n, sizeof p->lock_n), o += sizeof p->lock_n;
    for (t = 0; t < QD_PAT_NTRACK; t++) {
        uint8_t mask = 0;
        for (pg = 0; pg < QD_NPAGE; pg++)
            for (s = 0; s < QD_PAGE; s++)
                if (!empty_step(&p->step[t][pg * QD_PAGE + s]))
                    mask |= (uint8_t)(1u << pg);
        b[o++] = mask;
        for (pg = 0; pg < QD_NPAGE; pg++) {
            uint16_t sm = 0;
            if (!((mask >> pg) & 1u))
                continue;
            for (s = 0; s < QD_PAGE; s++)
                if (!empty_step(&p->step[t][pg * QD_PAGE + s]))
                    sm |= (uint16_t)(1u << s);
            memcpy(b + o, &sm, 2), o += 2;
            for (s = 0; s < QD_PAGE; s++)
                if ((sm >> s) & 1u)
                    memcpy(b + o, &p->step[t][pg * QD_PAGE + s], sizeof(qd_step_t)), o += sizeof(qd_step_t);
        }
    }
    memcpy(b + o, p->lock, (size_t)p->nlock * sizeof(qd_lock_t)), o += (uint32_t)p->nlock * sizeof(qd_lock_t);
    for (t = 0; t < QD_PAT_NTRACK; t++) {
        qd_quad_pack(p->snd[t].patch, b + o), o += QD_QUAD_BLOB;
        b[o++] = p->snd[t].sound, b[o++] = p->snd[t].edited;
        memcpy(b + o, p->snd[t].name, QD_SOUND_NAME), o += QD_SOUND_NAME;
        memcpy(b + o, &p->snd[t].preset, 2), o += 2;
    }
    return o;
}

static int de_pat(qd_pattern_t *p, const uint8_t *b, uint32_t len)
{
    uint32_t o = 0;
    int t, pg, s, v;
    if (len < 4 || b[0] != 'P' || (b[1] != 1 && b[1] != 2) || b[2] != sizeof(qd_step_t) || b[3] != sizeof(qd_tset_t))
        return 0;
    v = b[1];
    o = 4;
    memcpy(&p->master, b + o, 2), o += 2;
    p->swing = b[o++], p->used = b[o++];
    memcpy(&p->bpm, b + o, 2), o += 2;
    memcpy(&p->nlock, b + o, 2), o += 2;
    if (p->nlock > QD_NLOCK)
        return 0;
    memcpy(p->trk, b + o, sizeof p->trk), o += sizeof p->trk;
    memcpy(p->lock_start, b + o, sizeof p->lock_start), o += sizeof p->lock_start;
    memcpy(p->lock_n, b + o, sizeof p->lock_n), o += sizeof p->lock_n;
    memset(p->step, 0, sizeof p->step);
    for (t = 0; t < QD_PAT_NTRACK; t++) {
        const uint8_t mask = b[o++];
        for (pg = 0; pg < QD_NPAGE; pg++) {
            uint16_t sm;
            if (!((mask >> pg) & 1u))
                continue;
            memcpy(&sm, b + o, 2), o += 2;
            for (s = 0; s < QD_PAGE; s++)
                if ((sm >> s) & 1u) {
                    if (o + sizeof(qd_step_t) > len)
                        return 0;
                    memcpy(&p->step[t][pg * QD_PAGE + s], b + o, sizeof(qd_step_t)), o += sizeof(qd_step_t);
                }
        }
    }
    memcpy(p->lock, b + o, (size_t)p->nlock * sizeof(qd_lock_t)), o += (uint32_t)p->nlock * sizeof(qd_lock_t);
    for (t = 0; t < QD_PAT_NTRACK; t++) {
        if (o + QD_QUAD_BLOB + 2 + QD_SOUND_NAME + (v >= 2 ? 2u : 0u) > len)
            return 0;
        memset(&p->snd[t], 0, sizeof p->snd[t]);
        qd_quad_unpack(b + o, p->snd[t].patch), o += QD_QUAD_BLOB;
        p->snd[t].sound = b[o++], p->snd[t].edited = b[o++];
        memcpy(p->snd[t].name, b + o, QD_SOUND_NAME), o += QD_SOUND_NAME;
        p->snd[t].name[QD_SOUND_NAME - 1] = 0;
        if (v >= 2)
            memcpy(&p->snd[t].preset, b + o, 2), o += 2;
    }
    return o == len;
}

static uint32_t ser_snd(const qd_sound_t *s, uint8_t *b)
{
    uint32_t o = 2 + QD_SOUND_NAME + QD_QUAD_BLOB;
    if (!s->name[0]) {
        b[0] = 'E', b[1] = 1;
        return 2;
    }
    b[0] = 'S', b[1] = 2;
    memcpy(b + 2, s->name, QD_SOUND_NAME);
    qd_quad_pack(s->patch, b + 2 + QD_SOUND_NAME);
    memcpy(b + o, &s->preset, 2), o += 2;               /* P1.5t */
    b[o++] = s->kind, b[o++] = s->flags;
    return o;
}

/* a record into s; v1: 0.6's (its preset id and kind are the caller's to fill: de_snd_v1 says so) */
static int de_snd_v1;
static int de_snd(qd_sound_t *s, const uint8_t *b, uint32_t len)
{
    const uint32_t n1 = 2 + QD_SOUND_NAME + QD_QUAD_BLOB;
    memset(s, 0, sizeof *s);
    de_snd_v1 = 0;
    if (len == 2 && b[0] == 'E' && b[1] == 1)
        return 1;
    if (b[0] != 'S' || !((b[1] == 1 && len == n1) || (b[1] == 2 && len == n1 + 4)))
        return 0;
    memcpy(s->name, b + 2, QD_SOUND_NAME);
    s->name[QD_SOUND_NAME - 1] = 0;
    if (!s->name[0])
        s->name[0] = '?';
    qd_quad_unpack(b + 2 + QD_SOUND_NAME, s->patch);
    if (b[1] == 2) {
        memcpy(&s->preset, b + n1, 2);
        s->kind = b[n1 + 2] < QD_NPK ? b[n1 + 2] : (uint8_t)qd_preset_kind(s->patch);
        s->flags = b[n1 + 3];
    } else {
        s->kind = (uint8_t)qd_preset_kind(s->patch);   /* (0.6's: the kind read now) */
        de_snd_v1 = 1;
    }
    return 1;
}

static uint32_t ser_hdr(const qd_app_t *a, uint8_t *b)
{
    const qd_persist_t *ps = &a->per;
    const qd_project_t *pr = &a->proj;
    uint32_t o = 0;
    int t;
    b[o++] = 'H', b[o++] = 1;
    memcpy(b + o, ps->pname[ps->proj], QD_PROJ_NAME), o += QD_PROJ_NAME;
    b[o++] = ps->pdemo;
    b[o++] = pr->key_scale, b[o++] = pr->key_root, b[o++] = pr->launch;
    b[o++] = pr->rec_mode, b[o++] = pr->rec_quant, b[o++] = pr->rec_dub;
    for (t = 0; t < QD_NTRACK; t++) {
        const qd_track_t *k = &a->model.t[t];
        b[o++] = k->mix.level, b[o++] = (uint8_t)k->mix.pan, b[o++] = k->mix.chorus, b[o++] = k->mix.delay;
        b[o++] = k->mix.reverb, b[o++] = k->limit, b[o++] = k->reserve;
    }
    b[o++] = pr->rec_ekeys;                             /* P1.5b (a record of 0.4 has none: SCALE) */
    b[o++] = pr->pool_empty;                            /* P1.5t (0.6's have none: the old starting pool) */
    return o;
}

static int de_hdr(qd_app_t *a, int p, const uint8_t *b, uint32_t len, int apply)
{
    qd_persist_t *ps = &a->per;
    qd_project_t *pr = &a->proj;
    uint32_t o = 2;
    int t;
    if (len < 2 + QD_PROJ_NAME + 7 + 7 * QD_NTRACK || len > 2 + QD_PROJ_NAME + 7 + 7 * QD_NTRACK + 2 || b[0] != 'H' ||
        b[1] != 1)
        return 0;
    memcpy(ps->pname[p], b + o, QD_PROJ_NAME), o += QD_PROJ_NAME;
    ps->pname[p][QD_PROJ_NAME - 1] = 0;
    if (!apply)
        return 1;
    ps->pdemo = b[o++];
    pr->key_scale = b[o++], pr->key_root = b[o++], pr->launch = b[o++];
    pr->rec_mode = b[o++], pr->rec_quant = b[o++], pr->rec_dub = b[o++];
    for (t = 0; t < QD_NTRACK; t++) {
        qd_track_t *k = &a->model.t[t];
        k->mix.level = b[o++], k->mix.pan = (int8_t)b[o++], k->mix.chorus = b[o++], k->mix.delay = b[o++];
        k->mix.reverb = b[o++], k->limit = b[o++], k->reserve = b[o++];
    }
    pr->rec_ekeys = o < len ? (uint8_t)(b[o] != 0) : 0;
    pr->pool_empty = o + 1 < len ? (uint8_t)(b[o + 1] != 0) : 0;
    return 1;
}

static uint32_t ser_set(const qd_app_t *a, uint8_t *b)
{
    uint32_t o = 0;
    int i;
    b[o++] = 'G', b[o++] = 1;
    b[o++] = (uint8_t)a->per.proj, b[o++] = (uint8_t)a->seq.cur, b[o++] = (uint8_t)a->sel;
    b[o++] = (uint8_t)a->ui.knobs, b[o++] = (uint8_t)a->ui.view, b[o++] = (uint8_t)a->ui.follow;
    b[o++] = QD_NFXP;
    for (i = 0; i < QD_NFXP; i++) {
        const int16_t v = (int16_t)qd_fx_get(i);
        memcpy(b + o, &v, 2), o += 2;
    }
    b[o++] = (uint8_t)a->ui.lmode;                      /* P1.5b: the latch mode */
    b[o++] = 1;                                         /* P1.5s: the view is the user's (EDIT the default since) */
    return o;
}

static int de_set(const uint8_t *b, uint32_t len, int *proj, int *pat, int *sel, qd_app_t *a)
{
    int i;
    const uint32_t nf = len >= 9 ? b[8] : 0;            /* (P1.5b: an older record has fewer effects' values, no latch
                                                         * mode: the rest stay at their defaults) */
    if (len < 9 || b[0] != 'G' || b[1] != 1 || nf > QD_NFXP ||
        (len != 9u + 2u * nf && len != 10u + 2u * nf && len != 11u + 2u * nf))
        return 0;
    *proj = b[2] % QD_NPROJ, *pat = b[3] % QD_NPAT, *sel = b[4] % QD_NTRACK;
    a->ui.knobs = b[5] % QD_NKNOBS;
    a->ui.view = len == 11u + 2u * nf ? b[6] % QD_NVIEW : QD_VIEW_EDIT;   /* (P1.5s: a record from before EDIT was
                                                         * the default view: EDIT) */
    a->ui.follow = b[7] != 0;
    for (i = 0; i < (int)nf; i++) {
        int16_t v;
        memcpy(&v, b + 9 + 2 * i, 2);
        qd_fx_set(i, v);
    }
    a->ui.lmode = len >= 10u + 2u * nf ? b[9 + 2 * nf] % 2 : QD_LMODE_VALUES;
    return 1;
}

/* an item's key and bytes now */
static uint32_t item_key(const qd_app_t *a, int it)
{
    const int p = a->per.proj;
    if (it == QD_PI_SET)
        return QD_RKEY(QD_RK_SET, 0, 0);
    if (it == QD_PI_HDR)
        return QD_RKEY(QD_RK_HDR, p, 0);
    if (it < QD_PI_SND)
        return QD_RKEY(QD_RK_PAT, p, it - QD_PI_PAT);
    if (it < QD_PI_LIB)
        return QD_RKEY(QD_RK_SND, p, it - QD_PI_SND + 1);
    return QD_RKEY(QD_RK_LIB, 0, it - QD_PI_LIB);
}
static uint32_t item_bytes(const qd_app_t *a, int it, uint8_t *b)
{
    if (it == QD_PI_SET)
        return ser_set(a, b);
    if (it == QD_PI_HDR)
        return ser_hdr(a, b);
    if (it < QD_PI_SND)
        return ser_pat(&a->proj.pat[it - QD_PI_PAT], b);
    if (it < QD_PI_LIB)
        return ser_snd(&a->proj.sound[it - QD_PI_SND], b);
    return ser_snd(&a->per.lib[it - QD_PI_LIB], b);
}
static qd_store_t *item_store(qd_app_t *a, int it) { return it >= QD_PI_LIB ? &a->per.lib_st : &a->per.prj_st; }

/* everything as it is now: as written (after a load) */
static void mark_all_saved(qd_app_t *a)
{
    int it;
    for (it = 0; it < QD_PI_N; it++) {
        const uint32_t h = fnv(pr_buf, item_bytes(a, it, pr_buf));
        a->per.saved[it] = a->per.seen[it] = h;
        a->per.changed[it] = a->ms;
    }
}

/* ---------------------------------------------------------------- a project in and out */
static void proj_initial(qd_app_t *a, int demo, int pool_empty)
{
    qd_project_t *pr = &a->proj;
    int t;
    qd_project_init(pr);
    if (pool_empty) {                                   /* P1.5t: a new project's pool starts empty */
        qd_sounds_clear(pr);
        pr->pool_empty = 1;
    }
    if (demo)
        qd_demo_load(pr);
    for (t = 0; t < QD_NTRACK; t++) {                   /* the model's mix and voices: the default kit's */
        qd_model_t m;
        qd_model_init(&m);
        a->model.t[t].mix = m.t[t].mix;
        a->model.t[t].limit = m.t[t].limit;
        a->model.t[t].reserve = m.t[t].reserve;
    }
}

/* project p from the flash into the app (absent records: its defaults) */
static void proj_read(qd_app_t *a, int p)
{
    qd_persist_t *ps = &a->per;
    int len, k;
    ps->proj = p;
    len = qd_store_get(&ps->prj_st, QD_RKEY(QD_RK_HDR, p, 0), pr_buf, sizeof pr_buf);
    ps->pdemo = 0;
    {
        int empty = 0;
        if (len > 0 && len != (int)QDS_DELETED) {
            ps->pdemo = pr_buf[2 + QD_PROJ_NAME];
            empty = len == 2 + QD_PROJ_NAME + 7 + 7 * QD_NTRACK + 2 && pr_buf[len - 1];
        }
        proj_initial(a, ps->pdemo, empty);
    }
    if (len > 0 && len != (int)QDS_DELETED)
        de_hdr(a, p, pr_buf, (uint32_t)len, 1);
    for (k = 0; k < QD_NPAT; k++) {
        len = qd_store_get(&ps->prj_st, QD_RKEY(QD_RK_PAT, p, k), pr_buf, sizeof pr_buf);
        if (len > 0 && len != (int)QDS_DELETED) {
            static qd_pattern_t tmp;
            tmp = a->proj.pat[k];
            if (!de_pat(&a->proj.pat[k], pr_buf, (uint32_t)len))
                a->proj.pat[k] = tmp;                   /* (a record of another layout: the default) */
        }
    }
    for (k = 1; k <= QD_NSOUND; k++) {
        len = qd_store_get(&ps->prj_st, QD_RKEY(QD_RK_SND, p, k), pr_buf, sizeof pr_buf);
        if (len > 0 && len != (int)QDS_DELETED) {
            qd_sound_t *s = &a->proj.sound[k - 1];
            if (de_snd(s, pr_buf, (uint32_t)len) && de_snd_v1)
                s->preset = (uint16_t)qd_factory_of(s->name, s->patch);   /* (0.6's: the factory preset it is) */
        }
    }
    qd_sound_recount(&a->proj);
    for (k = 0; k < QD_NPAT; k++) {                     /* (0.6's track sounds: their preset from the pool source) */
        int t;
        for (t = 0; t < QD_PAT_NTRACK; t++) {
            qd_tsound_t *ts = &a->proj.pat[k].snd[t];
            if (ts->preset)
                continue;
            if (qd_sound_used(&a->proj, ts->sound))
                ts->preset = a->proj.sound[ts->sound - 1].preset;
            else
                ts->preset = (uint16_t)qd_factory_of(ts->name, ts->patch);
        }
    }
    a->snd_pat = -1;                                    /* (the pattern's sounds into the tracks at the next ms) */
}

/* the records of project p deleted (a header first: then nothing of it is found) */
static void proj_erase_records(qd_app_t *a, int p)
{
    qd_store_t *st = &a->per.prj_st;
    int i;
    qd_store_del(st, QD_RKEY(QD_RK_HDR, p, 0));
    qd_store_flush(st);
    for (i = 0; i < st->nkey; i++)
        if (((st->ent[i].key >> 16) & 0xFFu) == (uint32_t)p && st->ent[i].len != QDS_DELETED &&
            (st->ent[i].key >> 24) != QD_RK_SET) {
            qd_store_del(st, st->ent[i].key);
            qd_store_flush(st);
        }
}

static void put_now(qd_store_t *st, uint32_t key, const uint8_t *b, uint32_t len)
{
    qd_store_flush(st);
    qd_store_put(st, key, b, len);
    qd_store_flush(st);
}

static void write_header(qd_app_t *a)
{
    const uint32_t n = ser_hdr(a, pr_buf);
    put_now(&a->per.prj_st, QD_RKEY(QD_RK_HDR, a->per.proj, 0), pr_buf, n);
    a->per.pexists[a->per.proj] = 1;
}

/* ---------------------------------------------------------------- attach, the looks, the writes */
static void lib_read(qd_app_t *a)
{
    qd_persist_t *ps = &a->per;
    int k;
    for (k = 0; k < QD_NLIB; k++) {
        const int len = qd_store_get(&ps->lib_st, QD_RKEY(QD_RK_LIB, 0, k), pr_buf, sizeof pr_buf);
        memset(&ps->lib[k], 0, sizeof ps->lib[k]);
        if (len > 0 && len != (int)QDS_DELETED && de_snd(&ps->lib[k], pr_buf, (uint32_t)len) && de_snd_v1 &&
            ps->lib[k].name[0])
            ps->lib[k].preset = (uint16_t)(QD_PRESET_MINE0 + k);   /* (0.6's library: MY presets, saved) */
    }
}

static void names_read(qd_app_t *a)
{
    qd_persist_t *ps = &a->per;
    int p;
    for (p = 0; p < QD_NPROJ; p++) {
        const int len = qd_store_get(&ps->prj_st, QD_RKEY(QD_RK_HDR, p, 0), pr_buf, sizeof pr_buf);
        ps->pexists[p] = len > 0 && len != (int)QDS_DELETED;
        memset(ps->pname[p], 0, QD_PROJ_NAME);
        if (ps->pexists[p])
            de_hdr(a, p, pr_buf, (uint32_t)len, 0);
    }
}

void qd_persist_attach(qd_app_t *a, const qd_flash_t *fl)
{
    qd_persist_t *ps = &a->per;
    int proj = 0, pat = QD_PAT_START, sel = 0, len;
    ps->fl = *fl;
    if (!qd_store_mount(&ps->lib_st, fl, QD_FLASH_LIB, QD_FLASH_LIB_SEC) ||
        !qd_store_mount(&ps->prj_st, fl, QD_FLASH_PRJ, QD_FLASH_PRJ_SEC))
        return;
    qd_store_set_reserve(&ps->prj_st, 10);              /* (a full pattern record: 8 sectors) */
    ps->on = 1;
    ps->job = -1;
    lib_read(a);
    names_read(a);
    len = qd_store_get(&ps->prj_st, QD_RKEY(QD_RK_SET, 0, 0), pr_buf, sizeof pr_buf);
    if (len > 0 && len != (int)QDS_DELETED)
        de_set(pr_buf, (uint32_t)len, &proj, &pat, &sel, a);
    if (!ps->pexists[proj]) {                           /* an erased flash (or its project gone): the first, with
                                                         * the demos */
        proj = 0;
        if (!ps->pexists[0]) {
            ps->proj = 0;
            ps->pdemo = 1;
            proj_initial(a, 1, 0);                      /* (the demo keeps its starting pool: its drums) */
            memset(ps->pname[0], 0, QD_PROJ_NAME);
            strcpy(ps->pname[0], "PROJECT 1");
            write_header(a);
        }
    }
    proj_read(a, proj);
    qd_seq_select(&a->seq, pat, 1);
    qd_app_tempo(a, qd_seq_bpm(&a->seq));
    qd_ctl_select_track(a, sel);
    mark_all_saved(a);
    ps->saved[QD_PI_SET] ^= 1u;                         /* (the settings written once: the project's in them) */
}

int qd_persist_dirty(const qd_app_t *a)
{
    int it;
    if (!a->per.on)
        return 0;
    for (it = 0; it < QD_PI_N; it++)
        if (a->per.seen[it] != a->per.saved[it])
            return 1;
    return a->per.job >= 0;
}

static void look(qd_app_t *a, int it)
{
    qd_persist_t *ps = &a->per;
    const uint32_t h = fnv(pr_buf, item_bytes(a, it, pr_buf));
    if (h != ps->seen[it]) {
        ps->seen[it] = h;
        ps->changed[it] = a->ms;
    }
}

/* the job done (committed, or dropped: then it is tried again) */
static void job_end(qd_app_t *a)
{
    qd_persist_t *ps = &a->per;
    const qd_store_t *st = item_store(a, ps->job);
    if (st->c.drops == ps->job_drops) {
        ps->saved[ps->job] = ps->job_hash;
        ps->written++;
        if (ps->job >= QD_PI_PAT && ps->job < QD_PI_SND)
            ps->written_ms[ps->job - QD_PI_PAT] = a->ms ? a->ms : 1u;
    }
    ps->job = -1;
}

static int job_start(qd_app_t *a, int it)
{
    qd_persist_t *ps = &a->per;
    qd_store_t *st = item_store(a, it);
    const uint32_t n = item_bytes(a, it, pr_buf2);
    if (qd_store_busy(st))
        return 0;
    ps->job_hash = fnv(pr_buf2, n);
    ps->job_drops = st->c.drops;
    if (!qd_store_put(st, item_key(a, it), pr_buf2, n))
        return 0;
    ps->job = it;
    return 1;
}

void qd_persist_ms(qd_app_t *a)
{
    qd_persist_t *ps = &a->per;
    const int playing = a->seq.playing, sounding = qd_pool_in_use(&a->pool) > 0;
    const int quiet = !playing && !sounding && a->ms - a->ui.touch_ms >= QD_QUIET_MS;
    int it, k;
    if (!ps->on)
        return;
    if (a->ms % QD_SAVE_LOOK_MS == 0) {                 /* the looks */
        look(a, QD_PI_SET);
        look(a, QD_PI_HDR);
        look(a, QD_PI_PAT + a->seq.cur);
        ps->scan = (ps->scan + 1) % QD_NPAT;
        look(a, QD_PI_PAT + ps->scan);
        for (k = 0; k < 4; k++) {
            look(a, QD_PI_SND + (ps->scan * 2 + k) % QD_NSOUND);
            look(a, QD_PI_LIB + (ps->scan * 4 + k) % QD_NLIB);   /* (P1.5t: 256: four new ones a look) */
        }
    }
    if (ps->job < 0 && a->ms % 15u == 0) {              /* the next record due */
        for (it = 0; it < QD_PI_N; it++)
            if (ps->seen[it] != ps->saved[it] && a->ms - ps->changed[it] >= QD_SAVE_DEBOUNCE_MS) {
                job_start(a, it);
                break;
            }
    }
    if (a->ms % 15u == 0) {                             /* a UI frame: §7's writes */
        qd_store_step(&ps->prj_st, playing, quiet, playing ? 1 : 8);
        qd_store_step(&ps->lib_st, playing, quiet, playing ? 1 : 8);
        if (ps->job >= 0 && !qd_store_busy(item_store(a, ps->job)))
            job_end(a);
    }
}

int qd_persist_flush(qd_app_t *a)
{
    qd_persist_t *ps = &a->per;
    int it;
    if (!ps->on)
        return 1;
    if (ps->job >= 0) {
        qd_store_flush(item_store(a, ps->job));
        job_end(a);
    }
    for (it = 0; it < QD_PI_N; it++) {
        uint32_t n, h;
        n = item_bytes(a, it, pr_buf);
        h = fnv(pr_buf, n);
        ps->seen[it] = h;
        if (h == ps->saved[it])
            continue;
        if (job_start(a, it)) {
            qd_store_flush(item_store(a, it));
            job_end(a);
        }
    }
    return !qd_persist_dirty(a);
}

void qd_persist_use(const qd_app_t *a, uint32_t *live_pages, uint32_t *total_pages)
{
    if (!a->per.on) {
        if (live_pages)
            *live_pages = 0;
        if (total_pages)
            *total_pages = 1;
        return;
    }
    qd_store_use(&a->per.prj_st, live_pages, total_pages);
}

uint32_t qd_persist_proj_pages(const qd_app_t *a, int p)
{
    const qd_store_t *st = &a->per.prj_st;
    uint32_t n = 0;
    int i;
    if (!a->per.on)
        return 0;
    for (i = 0; i < st->nkey; i++)
        if (((st->ent[i].key >> 16) & 0xFFu) == (uint32_t)p && (st->ent[i].key >> 24) != QD_RK_SET)
            n += st->ent[i].npages;
    return n;
}

void qd_persist_proj_used(const qd_app_t *a, int p, uint8_t *used)
{
    const qd_store_t *st = &a->per.prj_st;
    int i;
    memset(used, 0, QD_NPAT);
    if (p == a->per.proj) {
        for (i = 0; i < QD_NPAT; i++)
            used[i] = a->proj.pat[i].used;
        return;
    }
    if (!a->per.on)
        return;
    for (i = 0; i < st->nkey; i++)
        if ((st->ent[i].key >> 24) == QD_RK_PAT && ((st->ent[i].key >> 16) & 0xFFu) == (uint32_t)p &&
            st->ent[i].len != QDS_DELETED)
            used[st->ent[i].key & 0x3Fu] = 1;
}

/* ---------------------------------------------------------------- the projects */
static int proj_switch_prep(qd_app_t *a)
{
    if (a->seq.playing)
        qd_app_stop(a);
    qd_persist_flush(a);
    return 1;
}

int qd_proj_load(qd_app_t *a, int p)
{
    qd_persist_t *ps = &a->per;
    if (p < 0 || p >= QD_NPROJ || !ps->on)
        return 0;
    qd_undo_reset(a);                                   /* (P1.5c: undo does not survive a project load) */
    if (!ps->pexists[p])
        return qd_proj_new(a, p);
    proj_switch_prep(a);
    proj_read(a, p);
    qd_seq_select(&a->seq, QD_PAT_START, 1);
    qd_app_tempo(a, qd_seq_bpm(&a->seq));
    mark_all_saved(a);
    ps->saved[QD_PI_SET] ^= 1u;
    return 1;
}

int qd_proj_new(qd_app_t *a, int p)
{
    qd_persist_t *ps = &a->per;
    if (p < 0 || p >= QD_NPROJ || !ps->on)
        return 0;
    qd_undo_reset(a);
    a->ui.view = QD_VIEW_EDIT;                          /* P1.5s: a new project starts in EDIT */
    proj_switch_prep(a);
    if (ps->pexists[p])
        proj_erase_records(a, p);
    ps->proj = p;
    ps->pdemo = 0;
    proj_initial(a, 0, 1);
    memset(ps->pname[p], 0, QD_PROJ_NAME);
    snprintf(ps->pname[p], QD_PROJ_NAME, "PROJECT %d", p + 1);
    write_header(a);
    a->snd_pat = -1;
    qd_seq_select(&a->seq, QD_PAT_START, 1);
    qd_app_tempo(a, qd_seq_bpm(&a->seq));
    mark_all_saved(a);
    ps->saved[QD_PI_SET] ^= 1u;
    return 1;
}

int qd_proj_copy(qd_app_t *a, int from, int to)
{
    qd_persist_t *ps = &a->per;
    qd_store_t *st = &ps->prj_st;
    static uint32_t keys[QDS_MAXKEY];
    int i, n = 0;
    if (from < 0 || from >= QD_NPROJ || to < 0 || to >= QD_NPROJ || from == to || !ps->on || !ps->pexists[from])
        return 0;
    qd_persist_flush(a);                                /* (the one worked on: all in the flash) */
    if (to == ps->proj)
        return 0;                                       /* (not over the one worked on) */
    if (ps->pexists[to])
        proj_erase_records(a, to);
    for (i = 0; i < st->nkey; i++)                      /* (the keys first: the copies change the index) */
        if (((st->ent[i].key >> 16) & 0xFFu) == (uint32_t)from && st->ent[i].len != QDS_DELETED &&
            (st->ent[i].key >> 24) != QD_RK_SET)
            keys[n++] = st->ent[i].key;
    for (i = 0; i < n; i++) {                           /* the header last: the copy exists once it is all there */
        const uint32_t k = keys[i];
        int len;
        if ((k >> 24) == QD_RK_HDR)
            continue;
        len = qd_store_get(st, k, pr_buf, sizeof pr_buf);
        if (len >= 0 && len != (int)QDS_DELETED)
            put_now(st, (k & 0xFF00FFFFu) | (uint32_t)to << 16, pr_buf, (uint32_t)len);
    }
    {
        const int len = qd_store_get(st, QD_RKEY(QD_RK_HDR, from, 0), pr_buf, sizeof pr_buf);
        if (len > 0 && len != (int)QDS_DELETED)
            put_now(st, QD_RKEY(QD_RK_HDR, to, 0), pr_buf, (uint32_t)len);
    }
    names_read(a);
    return ps->pexists[to];
}

int qd_proj_delete(qd_app_t *a, int p)
{
    qd_persist_t *ps = &a->per;
    if (p < 0 || p >= QD_NPROJ || !ps->on || !ps->pexists[p])
        return 0;
    if (p == ps->proj)
        return qd_proj_new(a, p);                       /* (the one worked on: a blank project in its place) */
    qd_persist_flush(a);
    proj_erase_records(a, p);
    names_read(a);
    return 1;
}

void qd_proj_name(qd_app_t *a, int p, const char *name)
{
    qd_persist_t *ps = &a->per;
    if (p < 0 || p >= QD_NPROJ)
        return;
    memset(ps->pname[p], 0, QD_PROJ_NAME);
    strncpy(ps->pname[p], name ? name : "", QD_PROJ_NAME - 1);
    if (ps->on && p != ps->proj && ps->pexists[p]) {    /* (another project: its header now) */
        const int len = qd_store_get(&ps->prj_st, QD_RKEY(QD_RK_HDR, p, 0), pr_buf, sizeof pr_buf);
        if (len >= 2 + QD_PROJ_NAME + 7 + 7 * QD_NTRACK) {
            memcpy(pr_buf + 2, ps->pname[p], QD_PROJ_NAME);
            put_now(&ps->prj_st, QD_RKEY(QD_RK_HDR, p, 0), pr_buf, (uint32_t)len);
        }
    }
}

/* ---------------------------------------------------------------- the pool */
int qd_pool_free_slot(const qd_project_t *pr)
{
    int k;
    for (k = 1; k <= QD_NSOUND; k++)                    /* (P1.5t: slot 1 as any other) */
        if (!pr->sound[k - 1].name[0])
            return k;
    return 0;
}

int qd_pool_add_sound(qd_app_t *a, const qd_sound_t *s)
{
    const int k = qd_pool_free_slot(&a->proj);
    qd_sound_t n;
    qd_undo_hint(a, QD_UK_SOUNDS);                      /* (P1.5c: one undo level) */
    if (!k)
        return 0;
    n = *s;
    n.flags = 0;
    if (!n.name[0])
        strcpy(n.name, "SOUND");
    a->proj.sound[k - 1] = n;
    qd_sound_recount(&a->proj);
    return k;
}

int qd_pool_add(qd_app_t *a, const char *name, const int16_t *patch)
{
    qd_sound_t n;
    memset(&n, 0, sizeof n);
    strncpy(n.name, name && name[0] ? name : "SOUND", QD_SOUND_NAME - 1);
    memcpy(n.patch, patch, sizeof n.patch);
    n.kind = (uint8_t)qd_preset_kind(patch);
    return qd_pool_add_sound(a, &n);
}

int qd_pool_find(const qd_app_t *a, const qd_sound_t *s)
{
    int k;
    for (k = 1; k <= QD_NSOUND; k++) {
        const qd_sound_t *p = &a->proj.sound[k - 1];
        if (!p->name[0])
            continue;
        if (s->preset && p->preset == s->preset && !memcmp(p->patch, s->patch, sizeof p->patch))
            return k;
        if (!strcmp(p->name, s->name) && !memcmp(p->patch, s->patch, sizeof p->patch))
            return k;
    }
    return 0;
}

/* every sound lock and track source of pool number x made y (and y x: a swap) in all the project's patterns */
static void renumber(qd_app_t *a, int x, int y, int swap)
{
    int p, t, s;
    for (p = 0; p < QD_NPAT; p++) {
        qd_pattern_t *P = &a->proj.pat[p];
        for (t = 0; t < QD_PAT_NTRACK; t++) {
            for (s = 0; s < QD_NSTEP; s++) {
                qd_step_t *st = &P->step[t][s];
                if (st->sound == x)
                    st->sound = (uint8_t)y;
                else if (swap && st->sound == y)
                    st->sound = (uint8_t)x;
            }
            if (P->snd[t].sound == x)
                P->snd[t].sound = (uint8_t)y;
            else if (swap && P->snd[t].sound == y)
                P->snd[t].sound = (uint8_t)x;
        }
    }
    for (t = 0; t < QD_NTRACK; t++) {                   /* (the tracks' mirror of the edited pattern) */
        if (a->model.t[t].sound == x)
            a->model.t[t].sound = (uint8_t)y;
        else if (swap && a->model.t[t].sound == y)
            a->model.t[t].sound = (uint8_t)x;
    }
}

int qd_pool_swap(qd_app_t *a, int x, int y)
{
    qd_sound_t tmp;
    qd_undo_hint(a, QD_UK_SOUNDS);                      /* (P1.5c: one undo level) */
    if (x < 1 || x > QD_NSOUND || y < 1 || y > QD_NSOUND || x == y)
        return 0;
    tmp = a->proj.sound[x - 1];
    a->proj.sound[x - 1] = a->proj.sound[y - 1];
    a->proj.sound[y - 1] = tmp;
    renumber(a, x, y, 1);
    qd_sound_recount(&a->proj);
    return 1;
}

int qd_pool_copy(qd_app_t *a, int from, int to)
{
    if (from < 1 || from > QD_NSOUND || to < 1 || to > QD_NSOUND || from == to || !a->proj.sound[from - 1].name[0])
        return 0;
    a->proj.sound[to - 1] = a->proj.sound[from - 1];
    qd_sound_recount(&a->proj);
    return 1;
}

int qd_pool_delete(qd_app_t *a, int k)
{
    int p, t, s;
    qd_undo_hint(a, QD_UK_SOUNDS);                      /* (P1.5c: one undo level) */
    if (k < 1 || k > QD_NSOUND || !a->proj.sound[k - 1].name[0])
        return 0;
    for (p = 0; p < QD_NPAT; p++) {
        qd_pattern_t *P = &a->proj.pat[p];
        for (t = 0; t < QD_PAT_NTRACK; t++) {
            for (s = 0; s < QD_NSTEP; s++)
                if (P->step[t][s].sound == k) {         /* back to the track's sound */
                    P->step[t][s].sound = 0;
                    P->step[t][s].ovr &= (uint16_t)~QD_OV(QD_PM_SOUND);
                }
            if (P->snd[t].sound == k)                   /* (the track keeps its copy: no source) */
                P->snd[t].sound = 0;
        }
    }
    for (t = 0; t < QD_NTRACK; t++)
        if (a->model.t[t].sound == k)
            a->model.t[t].sound = 0;
    memset(&a->proj.sound[k - 1], 0, sizeof a->proj.sound[k - 1]);
    qd_sound_recount(&a->proj);
    return 1;
}

int qd_pool_users(const qd_app_t *a, int k, uint8_t *tracks, int *steps)
{
    const qd_pattern_t *P = &a->proj.pat[a->seq.cur];
    int t, s, n = 0;
    uint8_t m = 0;
    for (t = 0; t < QD_PAT_NTRACK; t++) {
        if (a->model.t[t].sound == k)
            m |= (uint8_t)(1u << t);
        for (s = 0; s < QD_NSTEP; s++)
            n += P->step[t][s].sound == k && (P->step[t][s].flags & QD_SF_ON);
    }
    if (tracks)
        *tracks = m;
    if (steps)
        *steps = n;
    return m || n;
}

int qd_pool_users_all(const qd_app_t *a, int k, uint8_t *tracks)
{
    int p, t, s, n = 0;
    if (tracks)
        qd_pool_users(a, k, tracks, 0);
    for (p = 0; p < QD_NPAT; p++)
        for (t = 0; t < QD_PAT_NTRACK; t++)
            for (s = 0; s < QD_NSTEP; s++)
                n += a->proj.pat[p].step[t][s].sound == k && (a->proj.pat[p].step[t][s].flags & QD_SF_ON);
    return n;
}

/* ---------------------------------------------------------------- MY presets (the library's) */
int qd_lib_free_slot(const qd_app_t *a)
{
    int k;
    for (k = 0; k < QD_NLIB; k++)
        if (!a->per.lib[k].name[0])
            return k;
    return -1;
}

/* a new preset id: above every one this app knows (MINE's, the pool's, the tracks') */
static uint16_t lib_new_id(const qd_app_t *a)
{
    unsigned m = QD_PRESET_MINE0 + QD_NLIB - 1;
    int k, p, t;
    for (k = 0; k < QD_NLIB; k++)
        if (a->per.lib[k].name[0] && a->per.lib[k].preset > m)
            m = a->per.lib[k].preset;
    for (k = 0; k < QD_NSOUND; k++)
        if (a->proj.sound[k].preset > m)
            m = a->proj.sound[k].preset;
    for (p = 0; p < QD_NPAT; p++)
        for (t = 0; t < QD_PAT_NTRACK; t++)
            if (a->proj.pat[p].snd[t].preset > m)
                m = a->proj.pat[p].snd[t].preset;
    for (t = 0; t < QD_NTRACK; t++)
        if (a->model.t[t].preset > m)
            m = a->model.t[t].preset;
    return (uint16_t)(m >= 0xFFFFu ? QD_PRESET_MINE0 + QD_NLIB : m + 1u);
}

int qd_lib_store_ex(qd_app_t *a, int slot, const char *name, const int16_t *patch, int flags)
{
    qd_sound_t n;                                       /* (name or patch may be the slot's own) */
    qd_undo_hint(a, QD_UK_SOUNDS);                      /* (P1.5c: one undo level) */
    if (slot < 0 || slot >= QD_NLIB)
        return 0;
    memset(&n, 0, sizeof n);
    strncpy(n.name, name && name[0] ? name : "SOUND", QD_SOUND_NAME - 1);
    memcpy(n.patch, patch, sizeof n.patch);
    n.preset = lib_new_id(a);
    n.kind = (uint8_t)qd_preset_kind(patch);
    n.flags = (uint8_t)flags;
    a->per.lib[slot] = n;
    return 1;
}

int qd_lib_store(qd_app_t *a, int slot, const char *name, const int16_t *patch)
{
    return qd_lib_store_ex(a, slot, name, patch, 0);
}

int qd_lib_replace(qd_app_t *a, int slot, const char *name, const int16_t *patch)
{
    qd_sound_t *s;
    char nm[QD_SOUND_NAME];
    qd_undo_hint(a, QD_UK_SOUNDS);
    if (slot < 0 || slot >= QD_NLIB || !a->per.lib[slot].name[0])
        return 0;
    s = &a->per.lib[slot];
    memset(nm, 0, sizeof nm);
    strncpy(nm, name && name[0] ? name : s->name, QD_SOUND_NAME - 1);
    memcpy(s->name, nm, sizeof nm);
    memcpy(s->patch, patch, sizeof s->patch);
    s->kind = (uint8_t)qd_preset_kind(patch);
    s->flags = 0;                                       /* (saved here now) */
    return 1;
}

int qd_lib_delete(qd_app_t *a, int slot)
{
    qd_undo_hint(a, QD_UK_SOUNDS);                      /* (P1.5c: one undo level) */
    if (slot < 0 || slot >= QD_NLIB || !a->per.lib[slot].name[0])
        return 0;
    memset(&a->per.lib[slot], 0, sizeof a->per.lib[slot]);
    return 1;
}

int qd_lib_find(const qd_app_t *a, int id)
{
    int k;
    if (id < QD_PRESET_MINE0)
        return -1;
    for (k = 0; k < QD_NLIB; k++)
        if (a->per.lib[k].name[0] && a->per.lib[k].preset == id)
            return k;
    return -1;
}

int qd_preset_get(const qd_app_t *a, int id, qd_sound_t *out)
{
    int k;
    if (id >= 1 && id <= QD_NFACTORY) {
        qd_factory_preset(id - 1, out);
        return 1;
    }
    k = qd_lib_find(a, id);
    if (k < 0)
        return 0;
    *out = a->per.lib[k];
    return 1;
}
