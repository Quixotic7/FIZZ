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
 *        records are the old starting pool)
 *   SND / LIB  (P1.5v) 'S' 3: as 'S' 2 with the TAGS (a byte, bit QD_PK_k) where the kind was; an 'S' 2's kind
 *        becomes its one tag
 *   SND / LIB  (P1.5x) 'S' 4: as 'S' 3, then its mix: pan (-64..63), chorus, delay, reverb; an older record: no
 *        sends, pan centre (a factory preset's are its table's)
 *   PAT / SND / LIB  (E1) 'P' 3 and 'S' 5: the packed patches are version-6 blobs (QD_QUAD_BLOB, 128 bytes: the values
 *        74..); 'P' 1..2 and 'S' 1..4 carry 96-byte blobs (their values 74.. at their defaults), and a 'P' 1..2's lock
 *        ids of the mix (74..78 then) move up past the new values (QD_LK_PAN ..)
 *   FTAG 'F' 1, the factory presets' tags as the owner set them (QD_NFACTORY bytes: 0x80 | tags, 0 the classifier's);
 *        in the library's store. P1.5w: 'F' 2, then the next MY preset id (u16)
 *   SND  (P1.5w) 'R' 1, a pool slot's preset id (u16): THE POOL HOLDS REFERENCES. An 'S' record in the pool (0.7.2 and
 *        older: a copy) becomes a reference when its project loads: to the preset with that very sound (factory or
 *        MINE), else to a new MY preset made from it (its name made unique, its tags kept). A reference to a MY
 *        preset that is gone (deleted while another project was open) is dropped when its project loads, its steps
 *        back to their track's sound
 *   PAT  (P1.6c) 'P' 4: the track settings with their MIDI FX group (qd_tset_t.mfx, appended: 'P' 1..3 carry 42-byte
 *        track settings, read with it 0, OFF); a step's MIDI FX is its former pad byte (0: TRACK)
 *   MFX  (P1.6c) 'X' 1, the slot's size, the five MIDI FX groups (qd_mfx_group_t) as they are; none: every group
 *        empty */
#include <stddef.h>

#include "../voice/fx/qd_fx.h"

typedef char qd_tset_old_ok[offsetof(qd_tset_t, mfx) == QD_TSET_SIZE_OLD ? 1 : -1];   /* (P1.6c: 'P' 1..3's) */

static uint8_t pr_buf[QDS_MAXREC], pr_buf2[QDS_MAXREC];
static void pool_cache(const qd_app_t *a, qd_sound_t *s);   /* (P1.5w: the pool's references, below) */
static void pool_fixup(qd_app_t *a);

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
    b[o++] = 'P', b[o++] = 4;
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

/* P1.6c: a record's track settings' size by its version ('P' 4: today's; 1..3: before the MIDI FX field); 0 bad */
static uint32_t pat_tset_size(const uint8_t *b, uint32_t len)
{
    if (len < 4 || b[0] != 'P' || b[1] < 1 || b[1] > 4 || b[2] != sizeof(qd_step_t))
        return 0;
    if (b[1] >= 4)
        return b[3] == sizeof(qd_tset_t) ? (uint32_t)sizeof(qd_tset_t) : 0u;
    return b[3] == QD_TSET_SIZE_OLD ? (uint32_t)QD_TSET_SIZE_OLD : 0u;
}

static int de_pat(qd_pattern_t *p, const uint8_t *b, uint32_t len)
{
    uint32_t o = 0;
    int t, pg, s, v;
    const uint32_t tsz = pat_tset_size(b, len);
    if (!tsz)
        return 0;
    v = b[1];
    o = 4;
    memcpy(&p->master, b + o, 2), o += 2;
    p->swing = b[o++], p->used = b[o++];
    memcpy(&p->bpm, b + o, 2), o += 2;
    memcpy(&p->nlock, b + o, 2), o += 2;
    if (p->nlock > QD_NLOCK)
        return 0;
    if (o + tsz * QD_PAT_NTRACK > len)
        return 0;
    memset(p->trk, 0, sizeof p->trk);                   /* (P1.6c: an older record's tracks: MIDI FX OFF) */
    for (t = 0; t < QD_PAT_NTRACK; t++)
        memcpy(&p->trk[t], b + o, tsz), o += tsz;
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
    if (v < 3) {                                        /* (E1: the mix's lock ids past the new values) */
        int k;
        for (k = 0; k < p->nlock; k++)
            if (p->lock[k].id >= QD_QUAD_NP5)
                p->lock[k].id = (uint8_t)(p->lock[k].id + (QD_QUAD_NP - QD_QUAD_NP5));
    }
    for (t = 0; t < QD_PAT_NTRACK; t++) {
        const uint32_t bl = v >= 3 ? QD_QUAD_BLOB : QD_QUAD_BLOB5;
        if (o + bl + 2 + QD_SOUND_NAME + (v >= 2 ? 2u : 0u) > len)
            return 0;
        memset(&p->snd[t], 0, sizeof p->snd[t]);
        qd_quad_unpack(b + o, (int)bl, p->snd[t].patch), o += bl;
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
    b[0] = 'S', b[1] = 5;
    memcpy(b + 2, s->name, QD_SOUND_NAME);
    qd_quad_pack(s->patch, b + 2 + QD_SOUND_NAME);
    memcpy(b + o, &s->preset, 2), o += 2;               /* P1.5t */
    b[o++] = s->tags, b[o++] = s->flags;                /* (P1.5v: the tags) */
    b[o++] = (uint8_t)s->pan, b[o++] = s->chorus, b[o++] = s->delay, b[o++] = s->reverb;   /* (P1.5x: its mix) */
    return o;
}

/* a record into s; v1: 0.6's (its preset id and kind are the caller's to fill: de_snd_v1 says so) */
static int de_snd_v1;
static int de_snd(qd_sound_t *s, const uint8_t *b, uint32_t len)
{
    const uint32_t bl = len >= 2 && b[1] >= 5 ? QD_QUAD_BLOB : QD_QUAD_BLOB5;   /* (E1: 'S' 5 a version-6 blob) */
    const uint32_t n1 = 2 + QD_SOUND_NAME + bl;
    memset(s, 0, sizeof *s);
    de_snd_v1 = 0;
    if (len == 2 && b[0] == 'E' && b[1] == 1)
        return 1;
    if (b[0] != 'S' || !((b[1] == 1 && len == n1) || ((b[1] == 2 || b[1] == 3) && len == n1 + 4) ||
                         ((b[1] == 4 || b[1] == 5) && len == n1 + 8)))
        return 0;
    memcpy(s->name, b + 2, QD_SOUND_NAME);
    s->name[QD_SOUND_NAME - 1] = 0;
    if (!s->name[0])
        s->name[0] = '?';
    qd_quad_unpack(b + 2 + QD_SOUND_NAME, (int)bl, s->patch);
    if (b[1] >= 2) {
        memcpy(&s->preset, b + n1, 2);
        if (b[1] >= 3)
            s->tags = (uint8_t)(b[n1 + 2] & QD_PK_MASK);
        else                                            /* (0.7.0 .. 0.7.1's one kind: one tag) */
            s->tags = b[n1 + 2] < QD_NPK ? QD_PKB(b[n1 + 2]) : (uint8_t)qd_preset_tags(s->patch);
        s->flags = b[n1 + 3];
        if (b[1] >= 4) {                                /* (P1.5x: its mix; older: none, pan centre) */
            s->pan = (int8_t)b[n1 + 4];
            s->chorus = b[n1 + 5] & 0x7F, s->delay = b[n1 + 6] & 0x7F, s->reverb = b[n1 + 7] & 0x7F;
            s->pan = s->pan < -64 ? -64 : s->pan > 63 ? 63 : s->pan;
        }
    } else {
        s->tags = (uint8_t)qd_preset_tags(s->patch);   /* (0.6's: the kind read now) */
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
    b[o++] = pr->rec_auto;                              /* P1.6d (0.9.0's have none: ON) */
    for (t = 0; t < QD_NTRACK; t++) {                   /* P1.6d: the tracks' kinds and names (0.9.0's: the default
                                                         * kit's) */
        const qd_track_t *k = &a->model.t[t];
        b[o++] = k->kind, b[o++] = k->named;
        memcpy(b + o, k->name, QD_TRACK_NAME - 1), o += QD_TRACK_NAME - 1;
    }
    return o;
}

static int de_hdr(qd_app_t *a, int p, const uint8_t *b, uint32_t len, int apply)
{
    qd_persist_t *ps = &a->per;
    qd_project_t *pr = &a->proj;
    uint32_t o = 2;
    int t;
    if (len < 2 + QD_PROJ_NAME + 7 + 7 * QD_NTRACK || len > 2 + QD_PROJ_NAME + 7 + 7 * QD_NTRACK + 3 + QD_NTRACK * (QD_TRACK_NAME + 1) || b[0] != 'H' ||
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
    pr->rec_auto = o + 2 < len ? (uint8_t)(b[o + 2] != 0) : 1;
    o += 3;
    if (o + QD_NTRACK * (QD_TRACK_NAME + 1) <= len)     /* P1.6d: the kinds and names (else the default kit's) */
        for (t = 0; t < QD_NTRACK; t++) {
            qd_track_t *k = &a->model.t[t];
            char nm[QD_TRACK_NAME];
            k->kind = b[o] < 7 ? b[o] : QD_TKIND_NONE;
            k->named = b[o + 1] != 0;
            memcpy(nm, b + o + 2, QD_TRACK_NAME - 1), nm[QD_TRACK_NAME - 1] = 0;
            if (nm[0])
                memcpy(k->name, nm, sizeof nm);
            o += QD_TRACK_NAME + 1;
        }
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

/* P1.5v: the factory tags' record */
static uint32_t ser_ftag(const qd_app_t *a, uint8_t *b)
{
    b[0] = 'F', b[1] = 2;
    memcpy(b + 2, a->per.ftags, QD_NFACTORY);
    memcpy(b + 2 + QD_NFACTORY, &a->per.id_next, 2);    /* (P1.5w) */
    return 2 + QD_NFACTORY + 2;
}

/* P1.6c: the project's MIDI FX groups: 'X' 1, the slot's size, then the five groups as they are */
static uint32_t ser_mfx(const qd_app_t *a, uint8_t *b)
{
    b[0] = 'X', b[1] = 1, b[2] = (uint8_t)sizeof(qd_mfx_slot_t);
    memcpy(b + 3, a->proj.mfx, sizeof a->proj.mfx);
    return 3 + (uint32_t)sizeof a->proj.mfx;
}
static int de_mfx(qd_app_t *a, const uint8_t *b, uint32_t len)
{
    int g, k;
    if (len != 3 + sizeof a->proj.mfx || b[0] != 'X' || b[1] != 1 || b[2] != sizeof(qd_mfx_slot_t))
        return 0;
    memcpy(a->proj.mfx, b + 3, sizeof a->proj.mfx);
    for (g = 0; g < QD_MFX_NGROUP; g++)                 /* (a type not known: the slot empty) */
        for (k = 0; k < QD_MFX_NSLOT; k++)
            if (a->proj.mfx[g].slot[k].type >= QD_MFX_NTYPE)
                qd_mfx_slot_init(&a->proj.mfx[g].slot[k], QD_MFX_NONE);
    return 1;
}

/* an item's key and bytes now */
static uint32_t item_key(const qd_app_t *a, int it)
{
    const int p = a->per.proj;
    if (it == QD_PI_FTAG)
        return QD_RKEY(QD_RK_FTAG, 0, 0);
    if (it == QD_PI_SET)
        return QD_RKEY(QD_RK_SET, 0, 0);
    if (it == QD_PI_HDR)
        return QD_RKEY(QD_RK_HDR, p, 0);
    if (it == QD_PI_MFX)
        return QD_RKEY(QD_RK_MFX, p, 0);
    if (it < QD_PI_SND)
        return QD_RKEY(QD_RK_PAT, p, it - QD_PI_PAT);
    if (it < QD_PI_LIB)
        return QD_RKEY(QD_RK_SND, p, it - QD_PI_SND + 1);
    return QD_RKEY(QD_RK_LIB, 0, it - QD_PI_LIB);
}
static uint32_t item_bytes(const qd_app_t *a, int it, uint8_t *b)
{
    if (it == QD_PI_FTAG)
        return ser_ftag(a, b);
    if (it == QD_PI_SET)
        return ser_set(a, b);
    if (it == QD_PI_HDR)
        return ser_hdr(a, b);
    if (it == QD_PI_MFX)
        return ser_mfx(a, b);
    if (it < QD_PI_SND)
        return ser_pat(&a->proj.pat[it - QD_PI_PAT], b);
    if (it < QD_PI_LIB) {
        const qd_sound_t *s = &a->proj.sound[it - QD_PI_SND];
        if (s->name[0] && s->preset) {                  /* P1.5w: a reference */
            b[0] = 'R', b[1] = 1;
            memcpy(b + 2, &s->preset, 2);
            return 4;
        }
        return ser_snd(s, b);                           /* (an empty slot; a copy the library had no room for) */
    }
    return ser_snd(&a->per.lib[it - QD_PI_LIB], b);
}
static qd_store_t *item_store(qd_app_t *a, int it)
{
    return it >= QD_PI_LIB && it != QD_PI_MFX ? &a->per.lib_st : &a->per.prj_st;
}

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
        qd_track_name_default(&a->model.t[t], t);       /* P1.6d (a project of 0.9.0: the names it had) */
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
            empty = len >= 2 + QD_PROJ_NAME + 7 + 7 * QD_NTRACK + 2 && pr_buf[2 + QD_PROJ_NAME + 7 + 7 * QD_NTRACK + 1];
                                                        /* (P1.6d: pool_empty's place, the record longer since) */
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
            if (len == 4 && pr_buf[0] == 'R' && pr_buf[1] == 1) {   /* P1.5w: a reference (resolved below) */
                memset(s, 0, sizeof *s);
                memcpy(&s->preset, pr_buf + 2, 2);
                pool_cache(a, s);                       /* (gone: INIT, flagged: the fixup drops it) */
            } else if (de_snd(s, pr_buf, (uint32_t)len) && de_snd_v1) {
                s->preset = (uint16_t)qd_factory_of(s->name, s->patch);   /* (0.6's: the factory preset it is) */
            }
        }
    }
    len = qd_store_get(&ps->prj_st, QD_RKEY(QD_RK_MFX, p, 0), pr_buf, sizeof pr_buf);   /* P1.6c (none: empty) */
    if (len > 0 && len != (int)QDS_DELETED)
        de_mfx(a, pr_buf, (uint32_t)len);
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
    memset(ps->ftags, 0, sizeof ps->ftags);             /* P1.5v: the factory tags (none: the classifier's) */
    ps->id_next = 0;
    {
        const int len = qd_store_get(&ps->lib_st, QD_RKEY(QD_RK_FTAG, 0, 0), pr_buf, sizeof pr_buf);
        if (len >= 2 + QD_NFACTORY && pr_buf[0] == 'F' && (pr_buf[1] == 1 || pr_buf[1] == 2))
            memcpy(ps->ftags, pr_buf + 2, QD_NFACTORY);
        if (len == 2 + QD_NFACTORY + 2 && pr_buf[1] == 2)
            memcpy(&ps->id_next, pr_buf + 2 + QD_NFACTORY, 2);
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
    pool_fixup(a);                                      /* (P1.5w: after: what it changes is written) */
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
        look(a, QD_PI_FTAG);
        look(a, QD_PI_MFX);                             /* P1.6c */
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
    pool_fixup(a);                                      /* (P1.5w) */
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

/* P1.5w: one sound's patch as the flash keeps it (two sounds the same: their packed patches the same) */
static int same_sound(const int16_t *x, const int16_t *y)
{
    uint8_t bx[QD_QUAD_BLOB], by[QD_QUAD_BLOB];
    qd_quad_pack(x, bx);
    qd_quad_pack(y, by);
    return !memcmp(bx, by, sizeof bx);
}

/* the preset (its id) that is this sound: its own id when trusted and there, else the one with that very sound
 * (factory first, then MINE); 0 none */
static int preset_with_sound(const qd_app_t *a, const qd_sound_t *s, int trust_id)
{
    qd_sound_t p;
    int k;
    if (s->preset && qd_preset_get(a, s->preset, &p) && (trust_id || same_sound(p.patch, s->patch)))
        return s->preset;
    for (k = 0; k < QD_NFACTORY; k++) {
        qd_factory_get(a, k, &p);
        if (same_sound(p.patch, s->patch))
            return k + 1;
    }
    for (k = 0; k < QD_NLIB; k++)
        if (a->per.lib[k].name[0] && same_sound(a->per.lib[k].patch, s->patch))
            return a->per.lib[k].preset;
    return 0;
}

static int lib_put(qd_app_t *a, int slot, const char *name, const int16_t *patch, int flags);

/* .. or a new MY preset made from it (its name made unique, its tags kept); 0: the library full */
static int preset_of_sound(qd_app_t *a, const qd_sound_t *s, int trust_id)
{
    char nm[QD_SOUND_NAME];
    int id = preset_with_sound(a, s, trust_id), slot;
    if (id)
        return id;
    slot = qd_lib_free_slot(a);
    if (slot < 0)
        return 0;
    qd_lib_unique_name(a, s->name[0] ? s->name : "SOUND", nm);
    lib_put(a, slot, nm, s->patch, s->flags & QD_SNDF_IMPORTED);
    if (s->tags & QD_PK_MASK)
        a->per.lib[slot].tags = (uint8_t)(s->tags & QD_PK_MASK);
    return a->per.lib[slot].preset;
}

/* a pool slot's cache from its preset (gone: INIT, greyed) */
static void pool_cache(const qd_app_t *a, qd_sound_t *s)
{
    qd_sound_t p;
    const uint16_t id = s->preset;
    if (qd_preset_get(a, id, &p)) {
        *s = p;
        s->flags = (uint8_t)(p.flags & QD_SNDF_IMPORTED);
    } else {
        qd_factory_preset(0, &p);
        *s = p;
        s->flags = QD_SNDF_MISSING;
    }
    s->preset = id;
}

/* the references' caches; drop: (a project loading) a reference to a MY preset that is gone is dropped, its locks
 * back to the track's sound */
static void pool_unref_all(qd_app_t *a, int k);
static void pool_resolve(qd_app_t *a, int drop)
{
    int k;
    for (k = 1; k <= QD_NSOUND; k++) {
        qd_sound_t *s = &a->proj.sound[k - 1];
        if (!s->name[0] || !s->preset)
            continue;
        pool_cache(a, s);
        if (drop && (s->flags & QD_SNDF_MISSING) && s->preset >= QD_PRESET_MINE0)
            pool_unref_all(a, k);
    }
    qd_sound_recount(&a->proj);
}
void qd_pool_resolve(qd_app_t *a) { pool_resolve(a, 0); }

/* a project loaded: its copies made references (0.7.2 and older), its dangling references dropped */
static void pool_fixup(qd_app_t *a)
{
    int k;
    for (k = 1; k <= QD_NSOUND; k++) {                  /* (a reference: its own preset again; a copy: the preset
                                                         * with its very sound, or a new MY one; the library full: it
                                                         * stays a copy) */
        qd_sound_t *s = &a->proj.sound[k - 1];
        if (s->name[0] && !(s->flags & QD_SNDF_MISSING))
            s->preset = (uint16_t)preset_of_sound(a, s, 0);
    }
    pool_resolve(a, 1);
}

int qd_pool_add_sound(qd_app_t *a, const qd_sound_t *s)
{
    const int k = qd_pool_free_slot(&a->proj);
    int id;
    qd_undo_hint(a, QD_UK_SOUNDS);                      /* (P1.5c: one undo level) */
    if (!k)
        return 0;
    id = preset_of_sound(a, s, 1);                      /* (P1.5w: a reference: the preset it is, or a new MY one) */
    if (!id)
        return 0;
    memset(&a->proj.sound[k - 1], 0, sizeof a->proj.sound[k - 1]);
    a->proj.sound[k - 1].preset = (uint16_t)id;
    pool_cache(a, &a->proj.sound[k - 1]);
    qd_sound_recount(&a->proj);
    return k;
}

int qd_pool_add(qd_app_t *a, const char *name, const int16_t *patch)
{
    qd_sound_t n;
    memset(&n, 0, sizeof n);
    strncpy(n.name, name && name[0] ? name : "SOUND", QD_SOUND_NAME - 1);
    memcpy(n.patch, patch, sizeof n.patch);
    n.tags = (uint8_t)qd_preset_tags(patch);
    return qd_pool_add_sound(a, &n);
}

int qd_pool_find(const qd_app_t *a, const qd_sound_t *s)
{
    int k;
    for (k = 1; k <= QD_NSOUND; k++) {
        const qd_sound_t *p = &a->proj.sound[k - 1];
        if (!p->name[0])
            continue;
        if (s->preset && p->preset == s->preset)        /* (P1.5w: a reference to it) */
            return k;
        if (!p->preset && !strcmp(p->name, s->name) && !memcmp(p->patch, s->patch, sizeof p->patch))
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

/* pool slot k emptied: its locks in every pattern back to the track's sound (rec: P1.5w, kept for undo outside the
 * gesture's pattern), the tracks' sources gone */
static void pool_unref(qd_app_t *a, int k, int rec)
{
    int p, t, s;
    for (p = 0; p < QD_NPAT; p++) {
        qd_pattern_t *P = &a->proj.pat[p];
        for (t = 0; t < QD_PAT_NTRACK; t++) {
            for (s = 0; s < QD_NSTEP; s++)
                if (P->step[t][s].sound == k) {         /* back to the track's sound */
                    const int ov = (P->step[t][s].ovr & QD_OV(QD_PM_SOUND)) != 0;
                    if (rec && p != a->undo.pat)
                        qd_undo_xlock(a, p, t, s, k, ov);
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
}
static void pool_unref_all(qd_app_t *a, int k) { pool_unref(a, k, 0); }

int qd_pool_delete(qd_app_t *a, int k)
{
    qd_undo_hint(a, QD_UK_SOUNDS);                      /* (P1.5c: one undo level) */
    if (k < 1 || k > QD_NSOUND || !a->proj.sound[k - 1].name[0])
        return 0;
    pool_unref(a, k, a->undo.open);                     /* (P1.5w: the other patterns' locks kept for undo) */
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
                if (a->proj.pat[p].step[t][s].sound == k && (a->proj.pat[p].step[t][s].flags & QD_SF_ON)) {
                    n++;
                    if (tracks)                         /* (P1.5v: the tracks whose steps lock it too) */
                        *tracks |= (uint8_t)(1u << t);
                }
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
    if (a->per.id_next > m + 1u)                        /* (P1.5w: never an id used before) */
        m = a->per.id_next - 1u;
    return (uint16_t)(m >= 0xFFFFu ? QD_PRESET_MINE0 + QD_NLIB : m + 1u);
}

/* a MY preset stored (no undo level: the callers' own); into one that is there: its id kept (P1.5w: its references
 * follow) */
static int lib_put(qd_app_t *a, int slot, const char *name, const int16_t *patch, int flags)
{
    qd_sound_t n;                                       /* (name or patch may be the slot's own) */
    if (slot < 0 || slot >= QD_NLIB)
        return 0;
    memset(&n, 0, sizeof n);
    strncpy(n.name, name && name[0] ? name : "SOUND", QD_SOUND_NAME - 1);
    memcpy(n.patch, patch, sizeof n.patch);
    n.preset = a->per.lib[slot].name[0] ? a->per.lib[slot].preset : lib_new_id(a);
    if (n.preset >= a->per.id_next)
        a->per.id_next = (uint16_t)(n.preset + 1u);
    n.tags = (uint8_t)qd_preset_tags(patch);
    n.flags = (uint8_t)flags;
    a->per.lib[slot] = n;
    return 1;
}

int qd_lib_store_ex(qd_app_t *a, int slot, const char *name, const int16_t *patch, int flags)
{
    const int was = slot >= 0 && slot < QD_NLIB && a->per.lib[slot].name[0];
    qd_undo_hint(a, QD_UK_SOUNDS);                      /* (P1.5c: one undo level) */
    if (!lib_put(a, slot, name, patch, flags))
        return 0;
    if (was)
        pool_resolve(a, 0);                             /* (P1.5w: over one that was there: its references follow) */
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
    s->flags = 0;                                       /* (saved here now; P1.5v: its tags kept) */
    pool_resolve(a, 0);                                 /* (P1.5w: the pool's references to it play it now) */
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
        qd_factory_get(a, id - 1, out);
        return 1;
    }
    k = qd_lib_find(a, id);
    if (k < 0)
        return 0;
    *out = a->per.lib[k];
    return 1;
}

/* ---------------------------------------------------------------- P1.5v: the tags */
void qd_factory_get(const qd_app_t *a, int k, qd_sound_t *out)
{
    qd_factory_preset(k, out);
    if (k >= 0 && k < QD_NFACTORY && (a->per.ftags[k] & 0x80u))
        out->tags = (uint8_t)(a->per.ftags[k] & QD_PK_MASK);
}

int qd_tags_set(qd_app_t *a, int code, int tags)
{
    tags &= (int)QD_PK_MASK;
    if (code >= QD_BR_POOL_CODE) {                      /* (a pool sound: P1.5w: its preset's tags) */
        const qd_sound_t *s;
        int m;
        if (!qd_sound_used(&a->proj, code - QD_BR_POOL_CODE))
            return 0;
        s = &a->proj.sound[code - QD_BR_POOL_CODE - 1];
        if (s->preset >= 1 && s->preset <= QD_NFACTORY)
            return qd_tags_set(a, s->preset - 1, tags);
        m = qd_lib_find(a, s->preset);
        if (m >= 0)
            return qd_tags_set(a, QD_BR_MINE_CODE + m, tags);
        qd_undo_hint(a, QD_UK_SOUNDS);                  /* (a copy) */
        a->proj.sound[code - QD_BR_POOL_CODE - 1].tags = (uint8_t)tags;
        return 1;
    }
    if (code >= QD_BR_MINE_CODE) {                      /* (one of MY presets) */
        if (code - QD_BR_MINE_CODE >= QD_NLIB || !a->per.lib[code - QD_BR_MINE_CODE].name[0])
            return 0;
        qd_undo_hint(a, QD_UK_SOUNDS);
        a->per.lib[code - QD_BR_MINE_CODE].tags = (uint8_t)tags;
        pool_resolve(a, 0);
        return 1;
    }
    if (code < 0 || code >= QD_NFACTORY)
        return 0;
    a->per.ftags[code] = (uint8_t)(0x80u | (unsigned)tags);   /* (the factory sound stays as it is; its tags the
                                                         * owner's, not undone) */
    pool_resolve(a, 0);
    return 1;
}

/* ---------------------------------------------------------------- P1.5w: a preset's users, its delete */
/* the steps (on) of a pattern record whose sound lock is in mask (a bit a pool number), from its bytes */
static int rec_pat_steps(const uint8_t *b, uint32_t len, const uint8_t *mask)
{
    uint32_t o;
    int t, pg, s, n = 0;
    uint16_t nlock;
    const uint32_t tsz = pat_tset_size(b, len);
    if (!tsz)
        return 0;
    o = 4 + 2 + 2 + 2;
    memcpy(&nlock, b + o, 2), o += 2;
    o += (uint32_t)(tsz * QD_PAT_NTRACK + sizeof(((qd_pattern_t *)0)->lock_start) +
                    sizeof(((qd_pattern_t *)0)->lock_n));
    for (t = 0; t < QD_PAT_NTRACK && o < len; t++) {
        const uint8_t pm = b[o++];
        for (pg = 0; pg < QD_NPAGE; pg++) {
            uint16_t sm;
            if (!((pm >> pg) & 1u))
                continue;
            if (o + 2 > len)
                return n;
            memcpy(&sm, b + o, 2), o += 2;
            for (s = 0; s < QD_PAGE; s++)
                if ((sm >> s) & 1u) {
                    qd_step_t st;
                    if (o + sizeof st > len)
                        return n;
                    memcpy(&st, b + o, sizeof st), o += sizeof st;
                    n += st.sound && ((mask[st.sound >> 3] >> (st.sound & 7)) & 1u) && (st.flags & QD_SF_ON);
                }
        }
    }
    (void)nlock;
    return n;
}

void qd_preset_usage(const qd_app_t *a, int id, int *projects, int *steps)
{
    const qd_persist_t *ps = &a->per;
    int p, k, np = 0, ns = 0;
    {   /* this project: its pool in RAM */
        int used = 0;
        for (k = 1; k <= QD_NSOUND; k++)
            if (qd_sound_used(&a->proj, k) && a->proj.sound[k - 1].preset == id) {
                used = 1;
                ns += qd_pool_users_all(a, k, 0);
            }
        np += used;
    }
    for (p = 0; ps->on && p < QD_NPROJ; p++) {          /* the others: their records */
        uint8_t mask[(QD_NSOUND + 8) / 8];
        int used = 0;
        if (p == ps->proj || !ps->pexists[p])
            continue;
        memset(mask, 0, sizeof mask);
        for (k = 1; k <= QD_NSOUND; k++) {
            uint8_t b[8];
            const int len = qd_store_get((qd_store_t *)&ps->prj_st, QD_RKEY(QD_RK_SND, p, k), b, sizeof b);
            uint16_t r;
            if (len != 4 || b[0] != 'R' || b[1] != 1)
                continue;
            memcpy(&r, b + 2, 2);
            if (r == id)
                mask[k >> 3] |= (uint8_t)(1u << (k & 7)), used = 1;
        }
        if (!used)
            continue;
        np++;
        for (k = 0; k < QD_NPAT; k++) {
            const int len = qd_store_get((qd_store_t *)&ps->prj_st, QD_RKEY(QD_RK_PAT, p, k), pr_buf, sizeof pr_buf);
            if (len > 0 && len != (int)QDS_DELETED)
                ns += rec_pat_steps(pr_buf, (uint32_t)len, mask);
        }
    }
    if (projects)
        *projects = np;
    if (steps)
        *steps = ns;
}

int qd_preset_delete(qd_app_t *a, int slot)
{
    int k, id;
    if (slot < 0 || slot >= QD_NLIB || !a->per.lib[slot].name[0])
        return 0;
    id = a->per.lib[slot].preset;
    qd_undo_hint(a, QD_UK_SOUNDS);                      /* (one level: the preset, its references, their locks) */
    for (k = 1; k <= QD_NSOUND; k++)
        if (qd_sound_used(&a->proj, k) && a->proj.sound[k - 1].preset == id)
            pool_unref(a, k, a->undo.open);
    memset(&a->per.lib[slot], 0, sizeof a->per.lib[slot]);
    return 1;
}

void qd_lib_unique_name(const qd_app_t *a, const char *nm, char *out)
{
    int n, k, clash;
    snprintf(out, QD_SOUND_NAME, "%s", nm);
    for (n = 2; n < 100; n++) {
        for (clash = 0, k = 0; k < QD_NLIB && !clash; k++)
            clash = a->per.lib[k].name[0] && !strcmp(a->per.lib[k].name, out);
        if (!clash)
            return;
        {
            char base[QD_SOUND_NAME];
            const int room = QD_SOUND_NAME - 1 - (n < 10 ? 2 : 3);
            snprintf(base, sizeof base, "%.*s", room, nm);
            while (strlen(base) && base[strlen(base) - 1] == ' ')
                base[strlen(base) - 1] = 0;
            snprintf(out, QD_SOUND_NAME, "%s %d", base, n);
        }
    }
}
