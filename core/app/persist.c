/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* Saving (persist.h): the records, written by themselves; the projects, the pool, the library. Built through app.c
 * (it includes this file and core/store/store.c: the core's source lists stay as they are).
 *
 * The records' bytes (version 1; the host's byte order and struct layouts: a Phase 2 format would spell them out):
 *   SET  'G' 2 (1: eight tracks; 2026-10-24), project, pattern, track, reserved (0; was the knob layout), view, page follow, the effects' parameters (QD_NFXP int16),
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
 *        empty
 *   PAT  (P2.1) 'P' 5: TRACK SOUNDS BY REFERENCE. After the first 12 bytes (as 'P' 4's), the eight track sounds'
 *        references (u16 each: a preset id, 0 none); the rest as 'P' 4's, except that a track sound with a reference
 *        is only its pool source and edited bytes (2, not 145). A track sound is written as a reference when it is
 *        not edited and is its preset as the record would hold it (the same name, the same packed patch); read, it is
 *        that preset's name and packed patch again: a copy's round trip, exactly. A track keeps the sound it loaded:
 *        before one of MY presets changes or goes (REPLACE, a store over it, a delete, an undo or redo of one), every
 *        record referring to it, in every project, is written again with the copy (pat_unref). Factory presets are
 *        compiled in and never change. A demo-density pattern: ~2.7 KB as 'P' 4, ~1.6 KB as 'P' 5 */
#include <stddef.h>

#include "../voice/fx/qd_fx.h"
#include "../seq/names.h"

typedef char qd_tset_old_ok[offsetof(qd_tset_t, mfx) == QD_TSET_SIZE_OLD ? 1 : -1];   /* (P1.6c: 'P' 1..3's) */

/* P2.1: no record is ever held whole in RAM unless it is short (the device has no room for a 28 KB one): a record is
 * made by an emitter (em_t) that hashes its bytes and copies the ones of a window (a page the store is writing, or
 * the whole of a short record), and a long one is read through a reader (rd_t: a page at a time) */
#define PR_SMALL 2560u                                  /* the longest short record (MIDI FX's 2.3 KB) */
static uint8_t pr_buf[PR_SMALL];
typedef struct {
    uint8_t *win;                                       /* the window's bytes (0: hash only) .. */
    uint32_t woff, wn;                                  /* .. bytes [woff, woff + wn) of the record */
    uint32_t pos;                                       /* bytes made */
    uint32_t h;                                         /* their FNV-1a */
} em_t;
static void em_init(em_t *e, uint8_t *win, uint32_t woff, uint32_t wn)
{
    e->win = win, e->woff = woff, e->wn = wn, e->pos = 0, e->h = 2166136261u;
}
static void em(em_t *e, const void *src, uint32_t n)
{
    const uint8_t *b = (const uint8_t *)src;
    uint32_t i;
    for (i = 0; i < n; i++)
        e->h = (e->h ^ b[i]) * 16777619u;
    if (e->win && e->pos + n > e->woff && e->pos < e->woff + e->wn) {
        const uint32_t from = e->pos < e->woff ? e->woff - e->pos : 0u;
        const uint32_t to = e->pos + n > e->woff + e->wn ? e->woff + e->wn - e->pos : n;
        memcpy(e->win + (e->pos + from - e->woff), b + from, to - from);
    }
    e->pos += n;
}
static void em8(em_t *e, uint32_t v)
{
    const uint8_t b = (uint8_t)v;
    em(e, &b, 1);
}
static uint32_t em_hash(const em_t *e) { return e->h ? e->h : 1u; }

/* a record read in parts (P2.1): the store's bytes a page at a time, the record's length; rd_get 0: past its end */
typedef struct {
    qd_store_t *st;
    uint32_t key, len, pos;
    uint32_t boff, bn;                                  /* the bytes in buf: the record's [boff, boff + bn) */
    uint8_t buf[QDS_PAYLOAD];
} rd_t;
static int rd_open(rd_t *r, qd_store_t *st, uint32_t key)   /* the record's length (QDS_DELETED, -1: none) */
{
    const int len = qd_store_get(st, key, 0, 0);
    memset(r, 0, sizeof *r);
    r->st = st, r->key = key;
    r->len = len > 0 && len != (int)QDS_DELETED ? (uint32_t)len : 0u;
    return len;
}
static int rd_get(rd_t *r, void *dst, uint32_t n)
{
    uint8_t *d = (uint8_t *)dst;
    if (r->pos + n > r->len)
        return 0;
    while (n) {
        uint32_t take;
        if (r->pos < r->boff || r->pos >= r->boff + r->bn) {
            r->boff = r->pos / QDS_PAYLOAD * QDS_PAYLOAD;
            r->bn = (uint32_t)qd_store_read(r->st, r->key, r->boff, r->buf, QDS_PAYLOAD);
            if (!r->bn)
                return 0;
        }
        take = r->boff + r->bn - r->pos < n ? r->boff + r->bn - r->pos : n;
        memcpy(d, r->buf + (r->pos - r->boff), take);
        d += take, n -= take, r->pos += take;
    }
    return 1;
}
static int rd_u8(rd_t *r, uint8_t *v) { return rd_get(r, v, 1); }
static uint32_t rd_left(const rd_t *r) { return r->len - r->pos; }

static void pool_cache(const qd_app_t *a, qd_sound_t *s);   /* (P1.5w: the pool's references, below) */
static void pool_fixup(qd_app_t *a);
#if QD_PAT_RAM < QD_NPAT
static void pool_sweep(qd_app_t *a, int all);          /* (2026-10-24: the pool's removed sounds' locks, below) */
/* (2026-10-24) out of qd_app_t (the single-core build's RAM code region holds the app's .bss to the byte): the pool
 * slots taken out whose locks some records still hold (not given out again until they are gone), those patterns
 * (bit p: cleared as each is read in), for one project (a test's other app: none) .. */
static struct {
    const qd_project_t *pr;
    uint64_t need;
    uint8_t slot[(QD_NSOUND + 7) / 8];
} pu;
#define PU_NEED(p) (pu.pr == (p) ? pu.need : 0u)
#define PU_SLOT(p, k) (pu.pr == (p) && (pu.slot[((k) - 1) >> 3] >> (((k) - 1) & 7) & 1u))
/* .. and qd_pool_users_all's count from the patterns out of RAM: for slot k, while qd_users_gen and the patterns in
 * RAM are as they were (the browser's view asks every frame) */
static struct {
    const qd_app_t *a;
    int8_t in[QD_PAT_RAM];
    uint8_t k, tr;
    uint16_t gen, n;
} uc;
#endif

static int empty_step(const qd_step_t *s)
{
    static const qd_step_t z;
    return !memcmp(s, &z, sizeof z);
}

/* ---------------------------------------------------------------- the records' bytes */
/* P2.1: track sound t's reference (its preset's id), 0: written as a copy */
int qd_persist_write_v4;                                /* (the tests: 'P' 4 records, as before P2.1) */
static int ser_noref;                                   /* (pat_unref: the preset no reference may name now) */
static void lib_scan(const qd_app_t *a, int i, qd_sound_t *out);
static int preset_rec(const qd_app_t *a, int id, char *name, uint8_t *blob)   /* (a preset as a record holds it) */
{
    qd_sound_t p;
    if (id >= 1 && id <= QD_NFACTORY) {
        qd_factory_preset(id - 1, &p);
    } else {
        const int k = qd_lib_find(a, id);
        if (k < 0)
            return 0;
        lib_scan(a, k, &p);
    }
    memcpy(name, p.name, QD_SOUND_NAME);
    qd_quad_pack(p.patch, blob);
    return 1;
}
static uint16_t tsound_ref(const qd_app_t *a, const qd_tsound_t *ts)
{
    char nm[QD_SOUND_NAME];
    uint8_t x[QD_QUAD_BLOB], y[QD_QUAD_BLOB];
    if (!a || qd_persist_write_v4 || !ts->preset || ts->edited || ts->preset == ser_noref ||
        !preset_rec(a, ts->preset, nm, y) || memcmp(nm, ts->name, QD_SOUND_NAME))
        return 0;
    qd_quad_pack(ts->patch, x);
    return memcmp(x, y, sizeof x) ? 0 : ts->preset;
}

static void ser_pat(const qd_app_t *a, const qd_pattern_t *p, em_t *e)
{
    uint16_t ref[QD_PAT_NTRACK];
    int t, pg, s;
    for (t = 0; t < QD_PAT_NTRACK; t++)
        ref[t] = tsound_ref(a, &p->snd[t]);
    em8(e, 'P'), em8(e, qd_persist_write_v4 ? 4 : 5);
    em8(e, QD_STEP_WIRE), em8(e, sizeof(qd_tset_t));   /* (P2.1c: the steps' 20-byte layout, as before) */
    em(e, &p->master, 2);
    em8(e, p->swing), em8(e, p->used);
    em(e, &p->bpm, 2);
    em(e, &p->nlock, 2);
    if (!qd_persist_write_v4)
        em(e, ref, sizeof ref);                         /* (P2.1: 'P' 5) */
    em(e, p->trk, sizeof p->trk);
    em(e, p->lock_start, sizeof p->lock_start);
    em(e, p->lock_n, sizeof p->lock_n);
    for (t = 0; t < QD_PAT_NTRACK; t++) {
        uint8_t mask = 0;
        for (pg = 0; pg < QD_NPAGE; pg++)
            for (s = 0; s < QD_PAGE; s++)
                if (!empty_step(&p->step[t][pg * QD_PAGE + s]))
                    mask |= (uint8_t)(1u << pg);
        em8(e, mask);
        for (pg = 0; pg < QD_NPAGE; pg++) {
            uint16_t sm = 0;
            if (!((mask >> pg) & 1u))
                continue;
            for (s = 0; s < QD_PAGE; s++)
                if (!empty_step(&p->step[t][pg * QD_PAGE + s]))
                    sm |= (uint16_t)(1u << s);
            em(e, &sm, 2);
            for (s = 0; s < QD_PAGE; s++)
                if ((sm >> s) & 1u) {
                    uint8_t w[QD_STEP_WIRE];
                    qd_step_wire(&p->step[t][pg * QD_PAGE + s], w);
                    em(e, w, QD_STEP_WIRE);
                }
        }
    }
    em(e, p->lock, (uint32_t)p->nlock * sizeof(qd_lock_t));
    for (t = 0; t < QD_PAT_NTRACK; t++) {
        uint8_t blob[QD_QUAD_BLOB];
        if (ref[t]) {                                   /* (P2.1: by reference) */
            em8(e, p->snd[t].sound), em8(e, p->snd[t].edited);
            continue;
        }
        qd_quad_pack(p->snd[t].patch, blob);
        em(e, blob, QD_QUAD_BLOB);
        em8(e, p->snd[t].sound), em8(e, p->snd[t].edited);
        em(e, p->snd[t].name, QD_SOUND_NAME);
        em(e, &p->snd[t].preset, 2);
    }
}

/* P1.6c: a record's track settings' size by its version ('P' 4, 5: today's; 1..3: before the MIDI FX field); 0 bad */
static uint32_t pat_tset_size(const uint8_t *b, uint32_t len)
{
    if (len < 4 || b[0] != 'P' || b[1] < 1 || b[1] > 5 || b[2] != QD_STEP_WIRE)
        return 0;
    if (b[1] >= 4)
        return b[3] == sizeof(qd_tset_t) ? (uint32_t)sizeof(qd_tset_t) : 0u;
    return b[3] == QD_TSET_SIZE_OLD ? (uint32_t)QD_TSET_SIZE_OLD : 0u;
}

static int de_pat(const qd_app_t *a, qd_pattern_t *p, rd_t *r)
{
    uint8_t h[4];
    uint16_t ref[QD_PAT_NTRACK];
    int t, pg, s, v;
    uint32_t tsz;
    if (!rd_get(r, h, 4) || !(tsz = pat_tset_size(h, r->len)))
        return 0;
    v = h[1];
    if (!rd_get(r, &p->master, 2) || !rd_u8(r, &p->swing) || !rd_u8(r, &p->used) || !rd_get(r, &p->bpm, 2) ||
        !rd_get(r, &p->nlock, 2))
        return 0;
    memset(ref, 0, sizeof ref);
    if (v >= 5 && !rd_get(r, ref, sizeof ref))         /* (P2.1: the track sounds' references) */
        return 0;
    if (p->nlock > QD_NLOCK)
        return 0;
    if (tsz * QD_PAT_NTRACK > rd_left(r))
        return 0;
    memset(p->trk, 0, sizeof p->trk);                   /* (P1.6c: an older record's tracks: MIDI FX OFF) */
    for (t = 0; t < QD_PAT_NTRACK; t++)
        rd_get(r, &p->trk[t], tsz);
    if (!rd_get(r, p->lock_start, sizeof p->lock_start) || !rd_get(r, p->lock_n, sizeof p->lock_n))
        return 0;
    memset(p->step, 0, sizeof p->step);
    for (t = 0; t < QD_PAT_NTRACK; t++) {
        uint8_t mask = 0;
        rd_u8(r, &mask);
        for (pg = 0; pg < QD_NPAGE; pg++) {
            uint16_t sm = 0;
            if (!((mask >> pg) & 1u))
                continue;
            rd_get(r, &sm, 2);
            for (s = 0; s < QD_PAGE; s++)
                if ((sm >> s) & 1u) {
                    uint8_t w[QD_STEP_WIRE];
                    if (!rd_get(r, w, QD_STEP_WIRE))
                        return 0;
                    qd_step_unwire(&p->step[t][pg * QD_PAGE + s], w);
                }
        }
    }
    rd_get(r, p->lock, (uint32_t)p->nlock * sizeof(qd_lock_t));
    if (v < 3) {                                        /* (E1: the mix's lock ids past the new values) */
        int k;
        for (k = 0; k < p->nlock; k++)
            if (p->lock[k].id >= QD_QUAD_NP5)
                p->lock[k].id = (uint8_t)(p->lock[k].id + (QD_QUAD_NP - QD_QUAD_NP5));
    }
    for (t = 0; t < QD_PAT_NTRACK; t++) {
        const uint32_t bl = v >= 3 ? QD_QUAD_BLOB : QD_QUAD_BLOB5;
        uint8_t blob[QD_QUAD_BLOB];
        if (ref[t]) {                                   /* (P2.1: its preset's name and packed patch, as a copy) */
            char nm[QD_SOUND_NAME];
            memset(&p->snd[t], 0, sizeof p->snd[t]);
            if (!rd_u8(r, &p->snd[t].sound) || !rd_u8(r, &p->snd[t].edited))
                return 0;
            p->snd[t].preset = ref[t];
            if (a && preset_rec(a, ref[t], nm, blob)) {
                memcpy(p->snd[t].name, nm, QD_SOUND_NAME);
                qd_quad_unpack(blob, QD_QUAD_BLOB, p->snd[t].patch);
            } else {                                    /* (gone, which pat_unref never lets happen: INIT) */
                qd_quad_preset_patch(0, p->snd[t].patch);
                strcpy(p->snd[t].name, "?");
            }
            continue;
        }
        if (bl + 2 + QD_SOUND_NAME + (v >= 2 ? 2u : 0u) > rd_left(r))
            return 0;
        memset(&p->snd[t], 0, sizeof p->snd[t]);
        rd_get(r, blob, bl);
        qd_quad_unpack(blob, (int)bl, p->snd[t].patch);
        rd_u8(r, &p->snd[t].sound), rd_u8(r, &p->snd[t].edited);
        rd_get(r, p->snd[t].name, QD_SOUND_NAME);
        p->snd[t].name[QD_SOUND_NAME - 1] = 0;
        if (v >= 2)
            rd_get(r, &p->snd[t].preset, 2);
    }
    return rd_left(r) == 0;
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

/* a pool slot's record (P1.5w: a reference, 'R' 1 and the preset id) or a library place's */
static uint32_t snd_rec(const qd_sound_t *s, int pool, uint8_t *b)
{
    if (pool && s->name[0] && s->preset) {
        b[0] = 'R', b[1] = 1;
        memcpy(b + 2, &s->preset, 2);
        return 4;
    }
    return ser_snd(s, b);                               /* (an empty slot; a copy the library had no room for) */
}

/* ---------------------------------------------------------------- P2.1: MY presets read and written whole */
#if QD_SND_RAM
#define LIB_DIR(a, i) (&(a)->per.ldir[i])
#else
#define LIB_DIR(a, i) (&(a)->per.lib[i])
#endif
void qd_lib_read(const qd_app_t *a, int i, qd_sound_t *out)
{
    if (i < 0 || i >= QD_NLIB) {
        memset(out, 0, sizeof *out);
        return;
    }
#if QD_SND_RAM
    if (!LIB_DIR(a, i)->name[0]) {                      /* (an empty place: nothing to read) */
        memset(out, 0, sizeof *out);
        return;
    }
    memcpy(out, qd_sndc_get((qd_sndc_t *)&a->per.sc, QD_SK_LIB + i), sizeof *out);
#else
    memcpy(out, &a->per.lib[i], sizeof *out);
#endif
}
void qd_lib_write(qd_app_t *a, int i, const qd_sound_t *s)
{
    if (i < 0 || i >= QD_NLIB)
        return;
#if QD_SND_RAM
    qd_sound_dir(&a->per.ldir[i], s);
    qd_sndc_put(&a->per.sc, QD_SK_LIB + i, s);
#else
    memcpy(&a->per.lib[i], s, sizeof *s);
#endif
}
int qd_lib_used(const qd_app_t *a, int i) { return i >= 0 && i < QD_NLIB && LIB_DIR(a, i)->name[0]; }
const char *qd_lib_name(const qd_app_t *a, int i) { return i >= 0 && i < QD_NLIB ? LIB_DIR(a, i)->name : ""; }
uint16_t qd_lib_preset(const qd_app_t *a, int i) { return i >= 0 && i < QD_NLIB ? LIB_DIR(a, i)->preset : 0; }
int qd_lib_tags(const qd_app_t *a, int i) { return i >= 0 && i < QD_NLIB ? LIB_DIR(a, i)->tags : 0; }
int qd_lib_flags(const qd_app_t *a, int i) { return i >= 0 && i < QD_NLIB ? LIB_DIR(a, i)->flags : 0; }

/* an item's sound as it is now (P2.1, the device: the one in RAM, its place's age untouched) */
static void snd_now(const qd_app_t *a, int it, qd_sound_t *out)
{
#if QD_SND_RAM
    const int key = it < QD_PI_LIB ? it - QD_PI_SND + 1 : QD_SK_LIB + it - QD_PI_LIB;
    const qd_sound_t *p = qd_sndc_peek(&a->per.sc, key);
    if (p) {
        memcpy(out, p, sizeof *out);
        return;
    }
#endif
    if (it < QD_PI_LIB)
        qd_sound_read(&a->proj, it - QD_PI_SND + 1, out);
    else
        qd_lib_read(a, it - QD_PI_LIB, out);
}

/* a pool slot's (key 1..QD_NSOUND) or a library place's (QD_SK_LIB + i) sound from its record */
static void pool_cache(const qd_app_t *a, qd_sound_t *s);
static void snd_from_rec(const qd_app_t *a, int key, qd_sound_t *s, const uint8_t *b, uint32_t len)
{
    if (key < QD_SK_LIB) {
        if (len == 4 && b[0] == 'R' && b[1] == 1) {     /* P1.5w: a reference (resolved below) */
            uint16_t id;
            memcpy(&id, b + 2, 2);
            memset(s, 0, sizeof *s);
            s->preset = id;
            pool_cache(a, s);                           /* (gone: INIT, flagged: the fixup drops it) */
        } else if (de_snd(s, b, len) && de_snd_v1) {
            s->preset = (uint16_t)qd_factory_of(s->name, s->patch);   /* (0.6's: the factory preset it is) */
        }
    } else if (de_snd(s, b, len) && de_snd_v1 && s->name[0]) {
        s->preset = (uint16_t)(QD_PRESET_MINE0 + key - QD_SK_LIB);   /* (0.6's library: MY presets, saved) */
    }
}
#if QD_SND_RAM
static void snd_load(void *ctx, int key, qd_sound_t *dst);
#endif

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
    if (pr->rec_dub > QD_TAKE_REPLACE)
        pr->rec_dub = QD_TAKE_CAPTURE;
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
    b[o++] = 'G', b[o++] = QD_PRJ_FORMAT;               /* (2026-10-24: 'G' 2, six tracks; 'G' 1 eight) */
    b[o++] = (uint8_t)a->per.proj, b[o++] = (uint8_t)a->seq.cur, b[o++] = (uint8_t)a->sel;
    b[o++] = 0, b[o++] = (uint8_t)a->ui.view, b[o++] = (uint8_t)a->ui.follow;
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
    if (len < 9 || b[0] != 'G' || b[1] != QD_PRJ_FORMAT || nf > QD_NFXP ||
        (len != 9u + 2u * nf && len != 10u + 2u * nf && len != 11u + 2u * nf))
        return 0;
    *proj = b[2] % QD_NPROJ, *pat = b[3] % QD_NPAT, *sel = b[4] % QD_NTRACK;
    /* b[5]: reserved (was the knob layout A..D; written 0, ignored since 2026-10-25) */
    a->ui.view = len == 11u + 2u * nf ? b[6] % QD_NVIEW : QD_VIEW_EDIT;   /* (P1.5s: a record from before EDIT was
                                                         * the default view: EDIT) */
    a->ui.follow = b[7] != 0;
    for (i = 0; i < (int)nf; i++) {
        int16_t v;
        memcpy(&v, b + 9 + 2 * i, 2);
        qd_fx_set(i, v);
    }
    a->ui.lmode = len >= 10u + 2u * nf ? b[9 + 2 * nf] % 2 : QD_LMODE_MODES;
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
static uint32_t item_small(const qd_app_t *a, int it, uint8_t *b)   /* (every item but a pattern: short) */
{
    if (it == QD_PI_FTAG)
        return ser_ftag(a, b);
    if (it == QD_PI_SET)
        return ser_set(a, b);
    if (it == QD_PI_HDR)
        return ser_hdr(a, b);
    if (it == QD_PI_MFX)
        return ser_mfx(a, b);
    {
        qd_sound_t s;                                   /* (P1.5w: a pool slot: a reference) */
        snd_now(a, it, &s);
        return snd_rec(&s, it < QD_PI_LIB, b);
    }
}
/* an item's record through an emitter (P2.1: a pattern's never whole in RAM) */
static void item_em(const qd_app_t *a, int it, em_t *e)
{
    static uint8_t b[PR_SMALL];
    if (it >= QD_PI_PAT && it < QD_PI_SND) {
        ser_pat(a, qd_proj_cpat(&a->proj, it - QD_PI_PAT), e);
        return;
    }
    em(e, b, item_small(a, it, b));
}
static uint32_t item_hash(const qd_app_t *a, int it, uint32_t *len)
{
    em_t e;
    em_init(&e, 0, 0, 0);
    item_em(a, it, &e);
    if (len)
        *len = e.pos;
    return em_hash(&e);
}
static qd_store_t *item_store(qd_app_t *a, int it)
{
    return it >= QD_PI_LIB && it != QD_PI_MFX ? &a->per.lib_st : &a->per.prj_st;
}

/* P2.1: an item there to look at: a pattern, and at the device's sizes a pool or library sound, only while it is in
 * RAM (out of it, its record is what it is) */
static int item_here(const qd_app_t *a, int it)
{
    if (it >= QD_PI_PAT && it < QD_PI_SND)
        return qd_proj_resident(&a->proj, it - QD_PI_PAT);
#if QD_SND_RAM
    if (it >= QD_PI_SND && it < QD_PI_LIB)
        return qd_sndc_peek(&a->per.sc, it - QD_PI_SND + 1) != 0;
    if (it >= QD_PI_LIB && it < QD_PI_LIB + QD_NLIB)
        return qd_sndc_peek(&a->per.sc, QD_SK_LIB + it - QD_PI_LIB) != 0;
#endif
    return 1;
}

/* a pattern's record: its hash; its bytes, a window at a time (the store's source) */
#if QD_PAT_RAM < QD_NPAT
static uint32_t pat_hash(const qd_app_t *a, const qd_pattern_t *P)
{
    em_t e;
    em_init(&e, 0, 0, 0);
    ser_pat(a, P, &e);
    return em_hash(&e);
}
#endif
typedef struct {
    const qd_app_t *a;
    const qd_pattern_t *P;
} pat_src_t;
static int pat_src(void *ctx, uint32_t off, uint8_t *dst, uint32_t n)
{
    const pat_src_t *c = (const pat_src_t *)ctx;
    em_t e;
    em_init(&e, dst, off, n);
    ser_pat(c->a, c->P, &e);
    return 0;
}

/* everything as it is now: as written (after a load) */
static void mark_all_saved(qd_app_t *a)
{
    int it;
    for (it = 0; it < QD_PI_N; it++) {
        uint32_t h;
        if (!item_here(a, it))
            continue;
        h = item_hash(a, it, 0);
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

/* a pattern's track sounds of 0.6 (no preset id): their preset from their pool source, else their factory sound */
static void tsound_fixup(const qd_app_t *a, qd_pattern_t *P)
{
    int t;
    for (t = 0; t < QD_PAT_NTRACK; t++) {
        qd_tsound_t *ts = &P->snd[t];
        if (ts->preset)
            continue;
        if (qd_sound_used(&a->proj, ts->sound))
            ts->preset = qd_sound_preset(&a->proj, ts->sound);
        else
            ts->preset = (uint16_t)qd_factory_of(ts->name, ts->patch);
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
#if QD_PAT_RAM < QD_NPAT
    for (k = 0; k < QD_NPAT; k++) {                     /* P2.1: the patterns stay in the store (read in when asked
                                                         * for: pat_load); their used flags for the overviews */
        rd_t r;
        uint8_t h[8];
        len = rd_open(&r, &ps->prj_st, QD_RKEY(QD_RK_PAT, p, k));
        a->proj.pat_used[k] = (uint8_t)(a->proj.demo && k < QD_DEMO_N);
        if (len > 0 && len != (int)QDS_DELETED && rd_get(&r, h, 8) && pat_tset_size(h, (uint32_t)len))
            a->proj.pat_used[k] = h[7];
    }
#else
    for (k = 0; k < QD_NPAT; k++) {
        rd_t r;
        len = rd_open(&r, &ps->prj_st, QD_RKEY(QD_RK_PAT, p, k));
        if (len > 0 && len != (int)QDS_DELETED) {
            qd_pattern_t *tmp = qd_pat_borrow();
            *tmp = *qd_proj_pat(&a->proj, k);
            if (!de_pat(a, qd_proj_pat(&a->proj, k), &r))
                *qd_proj_pat(&a->proj, k) = *tmp;                  /* (a record of another layout: the default) */
            qd_pat_return(tmp);
        }
    }
#endif
#if QD_SND_RAM
    for (k = 1; k <= QD_NSOUND; k++) {                  /* P2.1: the pool's sounds stay in the store (read in when
                                                         * asked for: snd_load); their names, ids, tags, flags */
        qd_sound_t s;
        snd_load(a, k, &s);
        qd_sound_dir(&a->proj.sdir[k - 1], &s);
    }
#else
    for (k = 1; k <= QD_NSOUND; k++) {
        len = qd_store_get(&ps->prj_st, QD_RKEY(QD_RK_SND, p, k), pr_buf, sizeof pr_buf);
        if (len > 0 && len != (int)QDS_DELETED)
            snd_from_rec(a, k, &a->proj.sound[k - 1], pr_buf, (uint32_t)len);
    }
#endif
    len = qd_store_get(&ps->prj_st, QD_RKEY(QD_RK_MFX, p, 0), pr_buf, sizeof pr_buf);   /* P1.6c (none: empty) */
    if (len > 0 && len != (int)QDS_DELETED)
        de_mfx(a, pr_buf, (uint32_t)len);
    qd_sound_recount(&a->proj);
#if QD_PAT_RAM >= QD_NPAT
    for (k = 0; k < QD_NPAT; k++)                       /* (0.6's track sounds: their preset from the pool source) */
        tsound_fixup(a, qd_proj_pat(&a->proj, k));
#endif
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

/* P2.1: a record copied to another key, a page at a time (never whole in RAM) */
typedef struct {
    qd_store_t *st;
    uint32_t key;
} copy_ctx_t;
static int copy_src(void *ctx, uint32_t off, uint8_t *dst, uint32_t n)
{
    const copy_ctx_t *c = (const copy_ctx_t *)ctx;
    return qd_store_read(c->st, c->key, off, dst, n) == (int)n ? 0 : -1;
}
static void copy_now(qd_store_t *st, uint32_t from, uint32_t to)
{
    copy_ctx_t c;
    const int len = qd_store_get(st, from, 0, 0);
    if (len < 0 || len == (int)QDS_DELETED)
        return;
    c.st = st, c.key = from;
    qd_store_flush(st);
    if (!len)
        qd_store_put(st, to, 0, 0);
    else
        qd_store_put_src(st, to, (uint32_t)len, copy_src, 0, &c);
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
#if QD_SND_RAM
    qd_sndc_drop(&ps->sc, QD_SK_LIB, QD_SK_LIB + QD_NLIB - 1);
    for (k = 0; k < QD_NLIB; k++) {                     /* P2.1: MY presets stay in the store (read in when asked for:
                                                         * snd_load); their names, ids, tags, flags */
        qd_sound_t s;
        snd_load(a, QD_SK_LIB + k, &s);
        qd_sound_dir(&ps->ldir[k], &s);
    }
#else
    for (k = 0; k < QD_NLIB; k++) {
        const int len = qd_store_get(&ps->lib_st, QD_RKEY(QD_RK_LIB, 0, k), pr_buf, sizeof pr_buf);
        memset(&ps->lib[k], 0, sizeof ps->lib[k]);
        if (len > 0 && len != (int)QDS_DELETED)
            snd_from_rec(a, QD_SK_LIB + k, &ps->lib[k], pr_buf, (uint32_t)len);
    }
#endif
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

/* 2026-10-24 (six tracks, the owner: a fresh start for the PROJECTS only): the projects' format is the settings
 * record's version ('G' QD_PRJ_FORMAT, written at every attach). A projects' store with records whose settings record
 * is missing or of another version holds projects of another format (eight tracks, 'G' 1: 0.11 and before): its
 * sectors are erased at the mount (before any sound: the port attaches before the audio starts) and it is mounted
 * again, empty: a blank flash's start. The library's store (MY PRESETS, the factory tags, the preset ids) is a region
 * of its own and is never touched. 0: the region is bad */
static int prj_format_ok(qd_persist_t *ps, const qd_flash_t *fl)
{
    uint8_t g[2] = {0};
    const int len = qd_store_get(&ps->prj_st, QD_RKEY(QD_RK_SET, 0, 0), g, sizeof g);
    int k;
    if (!ps->prj_st.nkey || (len >= 2 && len != (int)QDS_DELETED && g[0] == 'G' && g[1] == QD_PRJ_FORMAT))
        return 1;
    for (k = 0; k < ps->prj_st.nsec; k++)               /* (projects of another format: the region erased) */
        if (ps->prj_st.sseq[k])
            fl->erase(fl->ctx, QD_FLASH_PRJ + (uint32_t)k * QDS_SECTOR);
    ps->prj_wiped++;
    return qd_store_mount(&ps->prj_st, fl, QD_FLASH_PRJ, QD_FLASH_PRJ_SEC, ps->prj_ent, QD_PRJ_KEYS);
}

void qd_persist_attach(qd_app_t *a, const qd_flash_t *fl)
{
    qd_persist_t *ps = &a->per;
    int proj = 0, pat = QD_PAT_START, sel = 0, len;
    ps->fl = *fl;
    if (!qd_store_mount(&ps->lib_st, fl, QD_FLASH_LIB, QD_FLASH_LIB_SEC, ps->lib_ent, QD_LIB_KEYS) ||
        !qd_store_mount(&ps->prj_st, fl, QD_FLASH_PRJ, QD_FLASH_PRJ_SEC, ps->prj_ent, QD_PRJ_KEYS))
        return;
    if (!prj_format_ok(ps, fl))                         /* 2026-10-24: projects of another format: a fresh start */
        return;
    qd_store_set_reserve(&ps->prj_st, 10);              /* (a full pattern record: 8 sectors) */
    qd_store_set_reserve(&ps->lib_st, QD_FLASH_LIB_RESERVE);   /* (P2.4: a record a page) */
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
    ps->saved[QD_PI_SET] ^= 1u;
    ps->maybe_dirty = 1;                         /* (the settings written once: the project's in them) */
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

/* P2.4: a pattern's bytes in RAM, hashed fast (a word at a time; with the records committed so far: a MY preset's
 * change can change how a pattern's track sounds are written): the same as at its last look, its record is too, and
 * the record's own hash (the serialisation, the presets read) is not worked out again */
static uint32_t pat_raw_of(const qd_app_t *a, const qd_pattern_t *P)
{
    const uint8_t *w = (const uint8_t *)(const void *)P;   /* (a pattern is 2-byte aligned: words through memcpy) */
    uint32_t h = 0x9E3779B9u ^ a->per.written, i;
    for (i = 0; i < sizeof *P / 4u; i++) {
        uint32_t x;
        memcpy(&x, w + 4u * i, 4);
        h ^= x;
        h *= 0x85EBCA6Bu;
        h ^= h >> 15;
    }
    return h ? h : 1u;
}
static uint32_t pat_raw_hash(const qd_app_t *a, int k) { return pat_raw_of(a, qd_proj_cpat(&a->proj, k)); }

static void look(qd_app_t *a, int it)
{
    qd_persist_t *ps = &a->per;
    uint32_t h;
    if (!item_here(a, it))
        return;
    if (it == QD_PI_SET || it == QD_PI_HDR || it == QD_PI_FTAG || it == QD_PI_MFX) {   /* (P2.6c: the settings, the
                                                         * header, the tags, the MIDI FX: hashed again only after an
                                                         * input; every 11th all the same) */
        const int j = it == QD_PI_SET ? 0 : it == QD_PI_HDR ? 1 : it == QD_PI_FTAG ? 2 : 3;
        if (ps->it_gen[j] == (uint16_t)a->edit_gen && ps->it_wr[j] == (uint16_t)ps->written && ps->it_skip[j] < 10u) {
            ps->it_skip[j]++;
            return;
        }
        ps->it_gen[j] = (uint16_t)a->edit_gen, ps->it_wr[j] = (uint16_t)ps->written, ps->it_skip[j] = 0;
    }
    if (it >= QD_PI_PAT && it < QD_PI_SND) {
        const int k = it - QD_PI_PAT;
        uint32_t r;
        if (ps->pat_gen[k] == (uint16_t)a->edit_gen && ps->pat_wr[k] == (uint16_t)ps->written && ps->pat_skip[k] < 10u) {
            ps->pat_skip[k]++;                          /* (P2.6c: no input since its last look: not hashed again; a
                                                         * real look at least every 11th, for a change by other ways) */
            return;
        }
        ps->pat_gen[k] = (uint16_t)a->edit_gen, ps->pat_wr[k] = (uint16_t)ps->written, ps->pat_skip[k] = 0;
        r = pat_raw_hash(a, k);
        if (r == ps->pat_raw[it - QD_PI_PAT])
            return;
        ps->pat_raw[it - QD_PI_PAT] = r;
    }
    h = item_hash(a, it, 0);
    if (h != ps->seen[it]) {
        ps->seen[it] = h;
        ps->changed[it] = a->ms;
        ps->maybe_dirty = 1;
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
    ps->maybe_dirty = 1;                                /* (a dropped record is still to write) */
}

/* P2.1: the record the store is writing, made again page by page from the item (its bytes never held whole); before
 * its commit page the item must still be what it was when it was queued */
static int job_src(void *ctx, uint32_t off, uint8_t *dst, uint32_t n)
{
    qd_app_t *a = (qd_app_t *)ctx;
    em_t e;
    em_init(&e, dst, off, n);
    item_em(a, a->per.job_it, &e);
    return 0;
}
static int job_ok(void *ctx)
{
    qd_app_t *a = (qd_app_t *)ctx;
    return item_hash(a, a->per.job_it, 0) == a->per.job_hash;
}

static int job_start(qd_app_t *a, int it)
{
    qd_persist_t *ps = &a->per;
    qd_store_t *st = item_store(a, it);
    uint32_t n;
    const uint32_t h = item_hash(a, it, &n);
    if (qd_store_busy(st))
        return 0;
    ps->job_hash = h;
    ps->job_drops = st->c.drops;
    ps->job_it = it;
    if (n <= QDS_JOBBUF) {                              /* (short, and the host's every record: copied now, as in
                                                         * Phase 1: what is written is what it was at the start) */
        static uint8_t flat[QDS_JOBBUF];
        em_t e;
        em_init(&e, flat, 0, n);
        item_em(a, it, &e);
        if (!qd_store_put(st, item_key(a, it), flat, n))
            return 0;
    } else if (!qd_store_put_src(st, item_key(a, it), n, job_src, job_ok, a)) {
        return 0;
    }
    ps->job = it;
    return 1;
}

/* §7's writes: a step of each store's (a page while playing, up to 8 stopped; erases only when quiet), the job done */
static void persist_writes(qd_app_t *a, int playing, int quiet)
{
    qd_persist_t *ps = &a->per;
    const uint32_t e0 = ps->prj_st.c.erases;
    qd_store_step(&ps->prj_st, playing, quiet, playing ? 1 : 8);
    qd_store_step(&ps->lib_st, playing, quiet && ps->prj_st.c.erases == e0, playing ? 1 : 8);   /* (P2.4: one erase a
                                                         * call at most, the two stores together) */
    if (ps->job >= 0 && !qd_store_busy(item_store(a, ps->job)))
        job_end(a);
}

void qd_persist_ms(qd_app_t *a)
{
    qd_persist_t *ps = &a->per;
    const int playing = a->seq.playing, sounding = qd_pool_in_use(&a->pool) > 0;
    const int quiet = !playing && !sounding && a->ms - a->ui.touch_ms >= QD_QUIET_MS;
    int it, k;
#if QD_PAT_RAM < QD_NPAT
    if (PU_NEED(&a->proj) && !playing && !ps->own_step && a->ms % 15u == 7u)
        pool_sweep(a, 0);                               /* (2026-10-24: a removed sound's locks, a pattern a frame) */
#endif
    if (a->sx.wk.cmd && !ps->own_step && a->ms % 15u == 11u)
        qd_sx_work(a);                                  /* (web manager P1: a SysEx command's job, a step a frame) */
    if (!ps->on)
        return;
    k = (int)(a->ms % QD_SAVE_LOOK_MS);                 /* the looks: every QD_SAVE_LOOK_MS, one item a ms (P2.6c: the
                                                         * 14 of them spread, not in one ms: the longest ms ~1 item) */
    if (k == 0)
        look(a, QD_PI_SET);
    else if (k == 1)
        look(a, QD_PI_HDR);
    else if (k == 2)
        look(a, QD_PI_FTAG);
    else if (k == 3)
        look(a, QD_PI_MFX);                             /* P1.6c */
    else if (k == 4)
        look(a, QD_PI_PAT + a->seq.cur);
    else if (k == 5) {
        ps->scan = (ps->scan + 1) % QD_NPAT;
        look(a, QD_PI_PAT + ps->scan);
    } else if (k >= 6 && k < 14) {
        const int j = (k - 6) / 2;
        if (!(k & 1))
            look(a, QD_PI_SND + (ps->scan * 2 + j) % QD_NSOUND);
        else
            look(a, QD_PI_LIB + (ps->scan * 4 + j) % QD_NLIB);   /* (P1.5t: 256: four new ones a look) */
    }
    if (ps->job < 0 && ps->urgent) {                    /* P2.4: the one a prefetch waits on: now, no debounce */
        it = ps->urgent - 1;
        if (!item_here(a, it))
            ps->urgent = 0;                             /* (gone from RAM already: written as it went) */
        else {
            look(a, it);
            if (ps->seen[it] == ps->saved[it] || job_start(a, it))
                ps->urgent = 0;
        }
    }
    if (ps->job < 0 && a->ms % 15u == 0 && ps->maybe_dirty) {   /* the next record due (P2.6c: none dirty: no scan) */
        int dirty = 0;
        for (it = 0; it < QD_PI_N; it++)
            if (ps->seen[it] != ps->saved[it]) {
                dirty = 1;
                if (a->ms - ps->changed[it] >= QD_SAVE_DEBOUNCE_MS && item_here(a, it)) {   /* (P2.6c: in RAM: a
                                                         * sound's job never reads one in) */
                    job_start(a, it);
                    break;
                }
            }
        if (!dirty)
            ps->maybe_dirty = 0;
    }
    if (a->ms % 15u == 0 && !ps->own_step)              /* a UI frame: §7's writes (P2.4: or the port's own call) */
        persist_writes(a, playing, quiet);
}

void qd_persist_soon(qd_app_t *a, int it)
{
    if (!a->per.urgent)
        a->per.urgent = it + 1;
}

void qd_persist_own_step(qd_app_t *a, int on) { a->per.own_step = on != 0; }

void qd_persist_step(qd_app_t *a)
{
    const int playing = a->seq.playing, sounding = qd_pool_in_use(&a->pool) > 0;
#if QD_PAT_RAM < QD_NPAT
    if (PU_NEED(&a->proj) && !playing)                  /* (2026-10-24: a removed sound's locks, a pattern a step) */
        pool_sweep(a, 0);
#endif
    if (a->sx.wk.cmd)                                   /* (web manager P1: a SysEx command's job, a step a step) */
        qd_sx_work(a);
    if (a->per.on)
        persist_writes(a, playing, !playing && !sounding && a->ms - a->ui.touch_ms >= QD_QUIET_MS);
}

int qd_persist_flush(qd_app_t *a)
{
    qd_persist_t *ps = &a->per;
    int it;
    qd_pool_sweep_all(a);                               /* (2026-10-24: no removed sound's lock left in a record) */
    if (!ps->on)
        return 1;
    if (ps->job < 0) {                                  /* (a sound queued as it left RAM: snd_save) */
        qd_store_flush(&ps->prj_st);
        qd_store_flush(&ps->lib_st);
    }
    if (ps->job >= 0) {
        qd_store_flush(item_store(a, ps->job));
        job_end(a);
    }
    for (it = 0; it < QD_PI_N; it++) {
        uint32_t h;
        if (!item_here(a, it))
            continue;
        h = item_hash(a, it, 0);
        ps->seen[it] = h;
        ps->maybe_dirty = 1;
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
            n += (uint32_t)qd_store_ent_pages(&st->ent[i]);
    return n;
}

void qd_persist_proj_used(const qd_app_t *a, int p, uint8_t *used)
{
    const qd_store_t *st = &a->per.prj_st;
    int i;
    memset(used, 0, QD_NPAT);
    if (p == a->per.proj) {
        for (i = 0; i < QD_NPAT; i++)
            used[i] = qd_proj_used(&a->proj, i);
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
    qd_seq_select(&a->seq, 0, 1);              /* (a project opens on A01) */
    qd_ctl_select_track(a, 0);
    qd_app_tempo(a, qd_seq_bpm(&a->seq));
    mark_all_saved(a);
    ps->saved[QD_PI_SET] ^= 1u;
    ps->maybe_dirty = 1;
    pool_fixup(a);                                      /* (P1.5w) */
    return 1;
}

/* P1.6g: a new project's tracks named from their sounds: the kind from the preset's tags, the name rolled from the
 * namesizer and the sound's own ideas (not typed: a kind chosen later renames it) */
static void names_from_sounds(qd_app_t *a)
{
    int t;
    for (t = 0; t < QD_NTRACK; t++) {
        const qd_tsound_t *s = &qd_proj_pat(&a->proj, QD_PAT_START)->snd[t];
        qd_track_t *k = &a->model.t[t];
        uint32_t seed = (a->ms + 1u) * 2654435761u + (uint32_t)t * 40503u + 7u;
        char nm[QD_NAME_MAX + 1];
        int kind = -1;
        if (s->preset >= 1 && s->preset <= QD_NFACTORY) {
            qd_sound_t f;
            qd_factory_preset(s->preset - 1, &f);
            kind = qd_tags_first(f.tags);
        }
        qd_name_roll(&seed, s->patch, 0, nm);
        memset(k->name, 0, sizeof k->name);
        strncpy(k->name, nm[0] ? nm : qd_track_kind_name(kind), QD_TRACK_NAME - 1);
        k->kind = kind >= 0 && kind < 7 ? (uint8_t)kind : QD_TKIND_NONE;
        k->named = 0;
    }
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
    names_from_sounds(a);                               /* (P1.6g) */
    memset(ps->pname[p], 0, QD_PROJ_NAME);
    snprintf(ps->pname[p], QD_PROJ_NAME, "PROJECT %d", p + 1);
    write_header(a);
    a->snd_pat = -1;
    qd_seq_select(&a->seq, 0, 1);              /* (a project opens on A01) */
    qd_ctl_select_track(a, 0);
    qd_app_tempo(a, qd_seq_bpm(&a->seq));
    mark_all_saved(a);
    ps->saved[QD_PI_SET] ^= 1u;
    ps->maybe_dirty = 1;
    return 1;
}

int qd_proj_copy(qd_app_t *a, int from, int to)
{
    qd_persist_t *ps = &a->per;
    qd_store_t *st = &ps->prj_st;
    uint32_t *keys;                                     /* (P2.1b: borrowed for the copy, no 4 KB static) */
    int i, n = 0;
    if (from < 0 || from >= QD_NPROJ || to < 0 || to >= QD_NPROJ || from == to || !ps->on || !ps->pexists[from])
        return 0;
    qd_persist_flush(a);                                /* (the one worked on: all in the flash) */
    if (to == ps->proj)
        return 0;                                       /* (not over the one worked on) */
    keys = (uint32_t *)qd_tmp_borrow(sizeof(uint32_t) * QD_PRJ_KEYS);
    if (!keys)
        return 0;
    if (ps->pexists[to])
        proj_erase_records(a, to);
    for (i = 0; i < st->nkey; i++)                      /* (the keys first: the copies change the index) */
        if (((st->ent[i].key >> 16) & 0xFFu) == (uint32_t)from && st->ent[i].len != QDS_DELETED &&
            (st->ent[i].key >> 24) != QD_RK_SET)
            keys[n++] = st->ent[i].key;
    for (i = 0; i < n; i++) {                           /* the header last: the copy exists once it is all there */
        const uint32_t k = keys[i];
        if ((k >> 24) == QD_RK_HDR)
            continue;
        copy_now(st, k, (k & 0xFF00FFFFu) | (uint32_t)to << 16);
    }
    qd_tmp_return(keys);
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
        if (!qd_sound_used(pr, k)
#if QD_PAT_RAM < QD_NPAT
            && !PU_SLOT(pr, k)                          /* (2026-10-24: its old locks not all gone yet) */
#endif
        )
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

/* P2.1: MY preset i read for a look through many (the device: from the store; the places in RAM left as they are) */
static void lib_scan(const qd_app_t *a, int i, qd_sound_t *out)
{
#if QD_SND_RAM
    const qd_sound_t *p = qd_sndc_peek(&a->per.sc, QD_SK_LIB + i);
    if (!qd_lib_used(a, i))
        memset(out, 0, sizeof *out);
    else if (p)
        memcpy(out, p, sizeof *out);
    else
        snd_load((void *)a, QD_SK_LIB + i, out);
#else
    qd_lib_read(a, i, out);
#endif
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
        if (qd_lib_used(a, k)) {
            lib_scan(a, k, &p);
            if (same_sound(p.patch, s->patch))
                return p.preset;
        }
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
    if (s->tags & QD_PK_MASK) {
        qd_sound_t n;
        qd_lib_read(a, slot, &n);
        n.tags = (uint8_t)(s->tags & QD_PK_MASK);
        qd_lib_write(a, slot, &n);
    }
    return qd_lib_preset(a, slot);
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
        qd_sound_t s;
        if (!qd_sound_used(&a->proj, k) || !qd_sound_preset(&a->proj, k))
            continue;
        qd_sound_read(&a->proj, k, &s);
        pool_cache(a, &s);
        qd_sound_write(&a->proj, k, &s);
        if (drop && (s.flags & QD_SNDF_MISSING) && s.preset >= QD_PRESET_MINE0)
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
        qd_sound_t s;
        if (!qd_sound_used(&a->proj, k) || (qd_sound_flags(&a->proj, k) & QD_SNDF_MISSING))
            continue;
        qd_sound_read(&a->proj, k, &s);
        s.preset = (uint16_t)preset_of_sound(a, &s, 0);
        qd_sound_write(&a->proj, k, &s);
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
    qd_undo_snd(a, QD_UNDO_RPOOL, k);                   /* (P2.1: as it was, for the device's undo) */
    {
        qd_sound_t n;
        memset(&n, 0, sizeof n);
        n.preset = (uint16_t)id;
        pool_cache(a, &n);
        qd_sound_write(&a->proj, k, &n);
    }
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
        const uint16_t pre = qd_sound_preset(&a->proj, k);
        if (!qd_sound_used(&a->proj, k))
            continue;
        if (s->preset && pre == s->preset)              /* (P1.5w: a reference to it) */
            return k;
        if (!pre) {                                     /* (a copy: its name and sound) */
            qd_sound_t c;
            qd_sound_read(&a->proj, k, &c);
            if (!strcmp(c.name, s->name) && !memcmp(c.patch, s->patch, sizeof c.patch))
                return k;
        }
    }
    return 0;
}

/* every sound lock and track source of pool number x made y (and y x: a swap): one pattern's (the references it
 * changed) */
static int pat_renum(qd_pattern_t *P, int x, int y, int swap)
{
    int t, s, n = 0;
    for (t = 0; t < QD_PAT_NTRACK; t++) {
        for (s = 0; s < QD_NSTEP; s++) {
            qd_step_t *st = &P->step[t][s];
            if (st->sound == x)
                st->sound = (uint8_t)y, n++;
            else if (swap && st->sound == y)
                st->sound = (uint8_t)x, n++;
        }
        if (P->snd[t].sound == x)
            P->snd[t].sound = (uint8_t)y, n++;
        else if (swap && P->snd[t].sound == y)
            P->snd[t].sound = (uint8_t)x, n++;
    }
    return n;
}
/* (every pattern: qd_pool_swap's loop; the pool swap's job: qd_pool_renumber_pat) the tracks' sources */
static void renumber_tracks(qd_app_t *a, int x, int y, int swap)
{
    int t;
    for (t = 0; t < QD_NTRACK; t++) {                   /* (the tracks' mirror of the edited pattern) */
        if (a->model.t[t].sound == x)
            a->model.t[t].sound = (uint8_t)y;
        else if (swap && a->model.t[t].sound == y)
            a->model.t[t].sound = (uint8_t)x;
    }
}

/* the two slots' sounds swapped, and the tracks' sources (the patterns: the callers') */
static void slots_swap(qd_app_t *a, int x, int y)
{
    qd_sound_t sx, sy;
    qd_sound_read(&a->proj, x, &sx);
    qd_sound_read(&a->proj, y, &sy);
    qd_sound_write(&a->proj, x, &sy);
    qd_sound_write(&a->proj, y, &sx);
    renumber_tracks(a, x, y, 1);
}

int qd_pool_swap(qd_app_t *a, int x, int y)
{
    int p;
    qd_undo_hint(a, QD_UK_SOUNDS);                      /* (P1.5c: one undo level) */
    if (x < 1 || x > QD_NSOUND || y < 1 || y > QD_NSOUND || x == y)
        return 0;
    qd_undo_snd(a, QD_UNDO_RPOOL, x);
    qd_undo_snd(a, QD_UNDO_RPOOL, y);
    slots_swap(a, x, y);
    for (p = 0; p < QD_NPAT; p++)
        pat_renum(qd_proj_pat(&a->proj, p), x, y, 1);
    qd_sound_recount(&a->proj);
    return 1;
}

/* (web manager P1) the pool swap in parts, for a job outside the locks (fizz_sx.c): the two slots and the tracks'
 * sources now (no undo level: the levels before are forgotten, their bytes would not match); then each pattern by
 * qd_pool_renumber_pat */
void qd_pool_swap_slots(qd_app_t *a, int x, int y)
{
    qd_undo_reset(a);
    slots_swap(a, x, y);
    qd_sound_recount(&a->proj);
    a->edit_gen++;
}

int qd_pool_copy(qd_app_t *a, int from, int to)
{
    qd_sound_t c;
    if (from < 1 || from > QD_NSOUND || to < 1 || to > QD_NSOUND || from == to || !qd_sound_used(&a->proj, from))
        return 0;
    qd_undo_snd(a, QD_UNDO_RPOOL, to);
    qd_sound_read(&a->proj, from, &c);
    qd_sound_write(&a->proj, to, &c);
    qd_sound_recount(&a->proj);
    return 1;
}

/* one pattern's locks of pool slot k back to the track's sound, its track sounds' sources gone (rec: each lock kept for
 * undo: P1.5w); the locks it had */
static int pat_unref(qd_app_t *a, qd_pattern_t *P, int p, int k, int rec)
{
    int t, s, n = 0;
    for (t = 0; t < QD_PAT_NTRACK; t++) {
        for (s = 0; s < QD_NSTEP; s++)
            if (P->step[t][s].sound == k) {             /* back to the track's sound */
                const int ov = (P->step[t][s].ovr & QD_OV(QD_PM_SOUND)) != 0;
                if (rec)
                    qd_undo_xlock(a, p, t, s, k, ov);
                P->step[t][s].sound = 0;
                P->step[t][s].ovr &= (uint16_t)~QD_OV(QD_PM_SOUND);
                n++;
            }
        if (P->snd[t].sound == k)                       /* (the track keeps its copy: no source) */
            P->snd[t].sound = 0, n++;
    }
    return n;
}

#if QD_PAT_RAM < QD_NPAT
static int rec_refs(qd_app_t *a, int p, int k, int *on, uint8_t *tracks, int rec);
/* (2026-10-24: the owner's freeze) a pattern in RAM now; one out of RAM whose record holds a lock of k: its pending
 * bit (pu.need), its locks gone as it is read in (pat_load) or as qd_persist_step reaches it; nothing read in
 * or written here. No record to read (its default: a demo): read in now */
static void pool_unref_pats(qd_app_t *a, int k, int rec)
{
    int p;
    for (p = 0; p < QD_NPAT; p++) {
        const int r = rec && p != a->undo.pat;
        int n;
        if (qd_proj_resident(&a->proj, p)) {
            pat_unref(a, qd_proj_pat(&a->proj, p), p, k, r);
            continue;
        }
        n = rec_refs(a, p, k, 0, 0, r);                 /* (the locks kept for undo from the record) */
        if (n < 0)
            pat_unref(a, qd_proj_pat(&a->proj, p), p, k, r);
        else if (n > 0) {
            if (pu.pr != &a->proj)
                memset(&pu, 0, sizeof pu), pu.pr = &a->proj;
            pu.slot[(k - 1) >> 3] |= (uint8_t)(1u << ((k - 1) & 7));
            pu.need |= (uint64_t)1 << p;
        }
    }
    qd_users_gen++;
}
/* a pattern just read in: the pending slots' locks gone (after its record's hash: it is written again) */
static void pat_unref_pending(qd_app_t *a, int p, qd_pattern_t *P)
{
    int k;
    if (!(PU_NEED(&a->proj) >> p & 1u))
        return;
    for (k = 1; k <= QD_NSOUND; k++)
        if (PU_SLOT(&a->proj, k))
            pat_unref(a, P, p, k, 0);
    pu.need &= ~((uint64_t)1 << p);
    if (!pu.need)
        memset(&pu, 0, sizeof pu);
    qd_users_gen++;
}
/* the main loop's: one pending pattern read in (never a write: qd_proj_fetch); all: every one now (an undo, a flush) */
static void pool_sweep(qd_app_t *a, int all)
{
    int p;
    for (p = 0; p < QD_NPAT && PU_NEED(&a->proj); p++) {
        if (!(PU_NEED(&a->proj) >> p & 1u))
            continue;
        if (all)
            qd_proj_pat(&a->proj, p);
        else if (!qd_proj_fetch(&a->proj, p))
            return;                                     /* (no clean slot yet: later) */
        if (qd_proj_resident(&a->proj, p))
            pat_unref_pending(a, p, qd_proj_pat(&a->proj, p));
        if (!all)
            return;
    }
}
void qd_pool_sweep_all(qd_app_t *a) { pool_sweep(a, 1); }
uint64_t qd_pool_unref_pending(const qd_app_t *a) { return PU_NEED(&a->proj); }
#else
static void pool_unref_pats(qd_app_t *a, int k, int rec)
{
    int p;
    for (p = 0; p < QD_NPAT; p++)
        pat_unref(a, qd_proj_pat(&a->proj, p), p, k, rec && p != a->undo.pat);
}
void qd_pool_sweep_all(qd_app_t *a) { (void)a; }
uint64_t qd_pool_unref_pending(const qd_app_t *a) { (void)a; return 0; }
#endif

/* pool slot k emptied: its locks in every pattern back to the track's sound (rec: P1.5w, kept for undo outside the
 * gesture's pattern), the tracks' sources gone */
static void pool_unref(qd_app_t *a, int k, int rec)
{
    int t;
    pool_unref_pats(a, k, rec);
    for (t = 0; t < QD_NTRACK; t++)
        if (a->model.t[t].sound == k)
            a->model.t[t].sound = 0;
    if (rec)
        qd_undo_snd(a, QD_UNDO_RPOOL, k);
    qd_sound_clear(&a->proj, k);
    qd_sound_recount(&a->proj);
}
static void pool_unref_all(qd_app_t *a, int k) { pool_unref(a, k, 0); }

int qd_pool_delete(qd_app_t *a, int k)
{
    qd_undo_hint(a, QD_UK_SOUNDS);                      /* (P1.5c: one undo level) */
    if (k < 1 || k > QD_NSOUND || !qd_sound_used(&a->proj, k))
        return 0;
    pool_unref(a, k, a->undo.open);                     /* (P1.5w: the other patterns' locks kept for undo) */
    return 1;
}

int qd_pool_users(const qd_app_t *a, int k, uint8_t *tracks, int *steps)
{
    const qd_pattern_t *P = qd_proj_cpat(&a->proj, a->seq.cur);
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

#if QD_PAT_RAM < QD_NPAT
/* pattern p's references to pool slot k as its record holds it, read a page at a time (no slot taken, nothing
 * written): the steps locking it (on: those sounding; tracks: theirs), its track sounds' sources; rec: each lock kept
 * for undo. -1: a record it cannot read (read it in) */
static qd_store_t *pat_store(qd_app_t *a);
static int rd_skip(rd_t *r, uint32_t n)
{
    if (r->pos + n > r->len)
        return 0;
    r->pos += n;
    return 1;
}
static int rec_refs(qd_app_t *a, int p, int k, int *on, uint8_t *tracks, int rec)
{
    qd_store_t *st = pat_store(a);
    const qd_pattern_t *Z = 0;
    rd_t r;
    uint8_t h[4];
    uint16_t ref[QD_PAT_NTRACK], nlock;
    uint32_t tsz;
    int t, pg, s, v, n = 0, len;
    if (!st)
        return -1;
    len = rd_open(&r, st, QD_RKEY(QD_RK_PAT, a->per.proj, p));
    if (len <= 0 || len == (int)QDS_DELETED) {          /* (its default: blank, or a demo: its drums' locks) */
        if (!a->proj.demo || p >= QD_DEMO_N)
            return 0;
        for (s = 0; s < 16; s++)
            if (QD_DEMO_DRUMS[p][s] == k) {
                n++;
                if (rec)
                    qd_undo_xlock(a, p, 0, s, k, 1);
                if (on)
                    ++*on;
                if (tracks)
                    *tracks |= 1u;
            }
        return n;
    }
    if (!rd_get(&r, h, 4) || !(tsz = pat_tset_size(h, r.len)) || !rd_skip(&r, 6) || !rd_get(&r, &nlock, 2))
        return -1;
    v = h[1];
    memset(ref, 0, sizeof ref);
    if ((v >= 5 && !rd_get(&r, ref, sizeof ref)) || !rd_skip(&r, tsz * QD_PAT_NTRACK) ||
        !rd_skip(&r, sizeof Z->lock_start + sizeof Z->lock_n))
        return -1;
    for (t = 0; t < QD_PAT_NTRACK; t++) {
        uint8_t mask = 0;
        rd_u8(&r, &mask);
        for (pg = 0; pg < QD_NPAGE; pg++) {
            uint16_t sm = 0;
            if (!((mask >> pg) & 1u))
                continue;
            rd_get(&r, &sm, 2);
            for (s = 0; s < QD_PAGE; s++)
                if ((sm >> s) & 1u) {
                    uint8_t w[QD_STEP_WIRE];
                    qd_step_t x;
                    if (!rd_get(&r, w, QD_STEP_WIRE))
                        return -1;
                    qd_step_unwire(&x, w);
                    if (x.sound != k)
                        continue;
                    n++;
                    if (rec)
                        qd_undo_xlock(a, p, t, pg * QD_PAGE + s, k, (x.ovr & QD_OV(QD_PM_SOUND)) != 0);
                    if (x.flags & QD_SF_ON) {
                        if (on)
                            ++*on;
                        if (tracks)
                            *tracks |= (uint8_t)(1u << t);
                    }
                }
        }
    }
    if (!rd_skip(&r, (uint32_t)nlock * sizeof(qd_lock_t)))
        return -1;
    for (t = 0; t < QD_PAT_NTRACK; t++) {               /* (the track sounds' sources) */
        uint8_t snd = 0;
        if (!ref[t] && !rd_skip(&r, v >= 3 ? QD_QUAD_BLOB : QD_QUAD_BLOB5))
            return -1;
        if (!rd_u8(&r, &snd) || !rd_skip(&r, 1) || (!ref[t] && !rd_skip(&r, QD_SOUND_NAME + (v >= 2 ? 2u : 0u))))
            return -1;
        n += snd == k;
    }
    return n;
}
#endif

static int pat_users(const qd_pattern_t *P, int k, uint8_t *tracks)
{
    int t, s, n = 0;
    for (t = 0; t < QD_PAT_NTRACK; t++)
        for (s = 0; s < QD_NSTEP; s++)
            if (P->step[t][s].sound == k && (P->step[t][s].flags & QD_SF_ON)) {
                n++;
                if (tracks)                             /* (P1.5v: the tracks whose steps lock it too) */
                    *tracks |= (uint8_t)(1u << t);
            }
    return n;
}

/* (2026-10-24) the patterns in RAM from RAM; the others from their records, never read in: on the device, the ones out
 * of RAM once per change (qd_users_gen, the patterns in RAM), not every frame */
int qd_pool_users_all(const qd_app_t *a, int k, uint8_t *tracks)
{
    int p, n = 0;
    uint8_t m = 0;
    qd_pool_users(a, k, &m, 0);
#if QD_PAT_RAM < QD_NPAT
    {
        qd_app_t *w = (qd_app_t *)a;                    /* (the store's reader: not the model) */
        int same = uc.a == a && uc.k == k && uc.gen == qd_users_gen;
        for (p = 0; same && p < QD_PAT_RAM; p++)
            same = uc.in[p] == a->proj.pat_in[p];
        if (!same) {
            uc.n = 0, uc.tr = 0;
            for (p = 0; p < QD_NPAT; p++) {
                int on = 0;
                if (qd_proj_resident(&a->proj, p))
                    continue;
                if (rec_refs(w, p, k, &on, &uc.tr, 0) < 0)   /* (a record it cannot read: read in) */
                    on = pat_users(qd_proj_cpat(&a->proj, p), k, &uc.tr);
                uc.n = (uint16_t)(uc.n + on);
            }
            uc.a = a, uc.k = (uint8_t)k, uc.gen = qd_users_gen;
            for (p = 0; p < QD_PAT_RAM; p++)
                uc.in[p] = a->proj.pat_in[p];
        }
        n = uc.n, m |= uc.tr;
        for (p = 0; p < QD_PAT_RAM; p++)
            if (a->proj.pat_in[p] >= 0)
                n += pat_users(&a->proj.pat[p], k, &m);
    }
#else
    for (p = 0; p < QD_NPAT; p++)
        n += pat_users(qd_proj_cpat(&a->proj, p), k, &m);
#endif
    if (tracks)
        *tracks = m;
    return n;
}

/* ---------------------------------------------------------------- P2.1: the track sounds' references */
#if QD_PAT_RAM < QD_NPAT
static qd_store_t *pat_store(qd_app_t *a);
#endif
/* the store the project's pattern records are in (none: the host with no flash, every pattern in RAM) */
static qd_store_t *pat_backing(qd_app_t *a)
{
    if (a->per.on)
        return &a->per.prj_st;
#if QD_PAT_RAM < QD_NPAT
    return pat_store(a);
#else
    return 0;
#endif
}

/* pattern k of project p written again now, in the newest format: from RAM (the project worked on, the pattern in
 * RAM) or read through its record (the current presets: the record's references as they still are) */
static int pat_rewrite(qd_app_t *a, qd_store_t *st, int p, int k)
{
    qd_persist_t *ps = &a->per;
    const uint32_t key = QD_RKEY(QD_RK_PAT, p, k);
    const qd_pattern_t *P;
    pat_src_t c;
    em_t e;
    qd_pattern_t *S = 0;
    if (p == ps->proj && qd_proj_resident(&a->proj, k)) {
        P = qd_proj_cpat(&a->proj, k);
    } else {
        rd_t r;
        S = qd_pat_borrow();                            /* (P2.1b: the device: the arena's room; none: not now) */
        if (!S)
            return 0;
        qd_pat_default(&a->proj, k, S);
        rd_open(&r, st, key);
        if (!de_pat(a, S, &r)) {
            qd_pat_return(S);
            return 0;
        }
        if (p == ps->proj)
            tsound_fixup(a, S);                         /* (0.6's track sounds: their presets, as a load does) */
        P = S;
    }
    em_init(&e, 0, 0, 0);
    ser_pat(a, P, &e);
    c.a = a, c.P = P;
    qd_store_flush(st);
    if (qd_store_put_src(st, key, e.pos, pat_src, 0, &c))
        qd_store_flush(st);
    if (p == ps->proj)
        ps->saved[QD_PI_PAT + k] = ps->seen[QD_PI_PAT + k] = em_hash(&e);
    if (S)
        qd_pat_return(S);
    return 1;
}

/* P2.1b: a 'P' 5 record written again with each track sound that refers to preset id a copy, streamed from the record
 * as it is (no pattern held: the rest of its bytes the same; a reference's two bytes become the copy's 145, the preset
 * as it still is) */
typedef struct {
    qd_store_t *st;
    uint32_t key;
    const qd_app_t *a;
    int id;
} unref_src_t;
static void unref_emit(const unref_src_t *u, em_t *e)
{
    uint8_t h[12], buf[64];
    uint16_t ref[QD_PAT_NTRACK], nref[QD_PAT_NTRACK];
    uint32_t len, sec = 0, off;
    int t;
    rd_t r;
    len = (uint32_t)rd_open(&r, u->st, u->key);
    rd_get(&r, h, 12);
    rd_get(&r, ref, sizeof ref);
    for (t = 0; t < QD_PAT_NTRACK; t++) {
        nref[t] = ref[t] == u->id ? 0 : ref[t];
        sec += ref[t] ? 2u : 2u + QD_QUAD_BLOB + QD_SOUND_NAME + 2u;
    }
    em(e, h, 12);
    em(e, nref, sizeof nref);
    for (off = 12u + (uint32_t)sizeof ref; off < len - sec;) {   /* (the body as it is) */
        const uint32_t n = len - sec - off < sizeof buf ? len - sec - off : (uint32_t)sizeof buf;
        rd_get(&r, buf, n);
        em(e, buf, n);
        off += n;
    }
    for (t = 0; t < QD_PAT_NTRACK; t++) {
        if (!ref[t]) {                                  /* (a copy: as it is) */
            uint8_t c[QD_QUAD_BLOB + 2 + QD_SOUND_NAME + 2];
            rd_get(&r, c, sizeof c);
            em(e, c, sizeof c);
        } else if (ref[t] != u->id) {                   /* (another reference: as it is) */
            uint8_t c[2];
            rd_get(&r, c, 2);
            em(e, c, 2);
        } else {                                        /* (this one: the copy of the preset as it still is) */
            uint8_t c[2], blob[QD_QUAD_BLOB];
            char nm[QD_SOUND_NAME];
            const uint16_t pre = ref[t];
            rd_get(&r, c, 2);
            if (!preset_rec(u->a, pre, nm, blob)) {
                int16_t init[QD_QUAD_NP];
                qd_quad_preset_patch(0, init);
                qd_quad_pack(init, blob);
                memset(nm, 0, sizeof nm);
                strcpy(nm, "?");
            }
            em(e, blob, QD_QUAD_BLOB);
            em(e, c, 2);                                /* (its pool source and edited bytes) */
            em(e, nm, QD_SOUND_NAME);
            em(e, &pre, 2);
        }
    }
}
static int unref_src(void *ctx, uint32_t off, uint8_t *dst, uint32_t n)
{
    em_t e;
    em_init(&e, dst, off, n);
    unref_emit((const unref_src_t *)ctx, &e);
    return 0;
}
static void pat_unref_stream(qd_app_t *a, qd_store_t *st, int p, int k, int id)
{
    qd_persist_t *ps = &a->per;
    unref_src_t u;
    em_t e;
    u.st = st, u.key = QD_RKEY(QD_RK_PAT, p, k), u.a = a, u.id = id;
    em_init(&e, 0, 0, 0);
    unref_emit(&u, &e);
    qd_store_flush(st);
    if (qd_store_put_src(st, u.key, e.pos, unref_src, 0, &u))
        qd_store_flush(st);
    if (p == ps->proj)
        ps->saved[QD_PI_PAT + k] = ps->seen[QD_PI_PAT + k] = em_hash(&e);
}

/* a pattern record's version and its track sounds' references ('P' 5's; others: none); 0: no pattern record */
static int pat_rec_refs(qd_store_t *st, int p, int k, uint16_t *ref)
{
    uint8_t h[12 + 2 * QD_PAT_NTRACK];
    const int len = qd_store_get(st, QD_RKEY(QD_RK_PAT, p, k), h, sizeof h);
    memset(ref, 0, sizeof(uint16_t) * QD_PAT_NTRACK);
    if (len < 12 || len == (int)QDS_DELETED || h[0] != 'P')
        return 0;
    if (h[1] >= 5 && len >= (int)sizeof h)
        memcpy(ref, h + 12, sizeof(uint16_t) * QD_PAT_NTRACK);
    return h[1];
}

/* before MY preset id changes or goes: every pattern record that refers to it (every project's) written again with
 * the track's own copy */
static void will_change(qd_app_t *a, int id, int p0, int p1)
{
    qd_persist_t *ps = &a->per;
    qd_store_t *st = pat_backing(a);
    int p, k, t;
    if (!st || id < QD_PRESET_MINE0 || (id == ps->wc_done && p1 - p0 > 1))
        return;                                         /* (P2-B: a STORE job's: each project done already) */
    if (ps->on && ps->job >= 0) {                       /* (the record being written: finished first) */
        qd_store_flush(item_store(a, ps->job));
        job_end(a);
    }
    ser_noref = id;
    for (p = p0; p < p1; p++) {
        if (p != ps->proj && (!ps->on || !ps->pexists[p]))
            continue;
        for (k = 0; k < QD_NPAT; k++) {
            uint16_t ref[QD_PAT_NTRACK];
            int hit = 0;
            if (pat_rec_refs(st, p, k, ref) < 5)
                continue;
            for (t = 0; t < QD_PAT_NTRACK; t++)
                hit |= ref[t] == id;
            if (!hit)
                continue;
            if (p == ps->proj && qd_proj_resident(&a->proj, k))
                pat_rewrite(a, st, p, k);               /* (in RAM: as it is there) */
            else
                pat_unref_stream(a, st, p, k, id);      /* (P2.1b: its record, streamed; no pattern held) */
        }
    }
    ser_noref = 0;
}
void qd_preset_will_change(qd_app_t *a, int id) { will_change(a, id, 0, QD_NPROJ); }
/* (web manager P1) the same for project p alone (a job's step: one project's records at a time) */
void qd_preset_will_change_proj(qd_app_t *a, int id, int p) { will_change(a, id, p, p + 1); }

/* (web manager P1) a pool swap's pattern k (the project worked on): in RAM if it is there; else its record read into
 * the arena's room (qd_pat_borrow: between frames only), renumbered, and written again if it changed (nothing to read:
 * its default, a demo's, written if it changed). 1 done; 0 not now (no room: again later) */
int qd_pool_renumber_pat(qd_app_t *a, int k, int x, int y)
{
#if QD_PAT_RAM < QD_NPAT
    qd_persist_t *ps = &a->per;
    qd_store_t *st;
    qd_pattern_t *S;
    rd_t r;
    int len;
#endif
    if (k < 0 || k >= QD_NPAT)
        return 1;
#if QD_PAT_RAM < QD_NPAT
    if (!qd_proj_resident(&a->proj, k)) {
        st = pat_backing(a);
        if (!st)
            return 1;
        S = qd_pat_borrow();
        if (!S)
            return 0;
        qd_pat_default(&a->proj, k, S);
        len = rd_open(&r, st, QD_RKEY(QD_RK_PAT, ps->proj, k));
        if (len > 0 && len != (int)QDS_DELETED) {
            if (!de_pat(a, S, &r)) {
                qd_pat_return(S);
                return 1;                               /* (a record it cannot read: as it is) */
            }
            tsound_fixup(a, S);
        }
        if (pat_renum(S, x, y, 1)) {
            em_t e;
            pat_src_t c;
            if (ps->on && ps->job >= 0) {               /* (the record being written: finished first) */
                qd_store_flush(item_store(a, ps->job));
                job_end(a);
            }
            em_init(&e, 0, 0, 0);
            ser_pat(a, S, &e);
            c.a = a, c.P = S;
            qd_store_flush(st);
            if (qd_store_put_src(st, QD_RKEY(QD_RK_PAT, ps->proj, k), e.pos, pat_src, 0, &c))
                qd_store_flush(st);
            ps->saved[QD_PI_PAT + k] = ps->seen[QD_PI_PAT + k] = em_hash(&e);
            qd_users_gen++;
        }
        qd_pat_return(S);
        return 1;
    }
#endif
    if (pat_renum(qd_proj_pat(&a->proj, k), x, y, 1)) {
        a->edit_gen++;
#if QD_PAT_RAM < QD_NPAT
        qd_users_gen++;
#endif
    }
    return 1;
}

/* every pattern record of an older format, of every project, written again in the newest ('P' 5: track sounds by
 * reference), each project loaded in turn (its records read as a load reads them); the project worked on again at
 * the end. The records written; 0 nothing older */
int qd_persist_migrate(qd_app_t *a)
{
    qd_persist_t *ps = &a->per;
    const int first = ps->proj;
    int p, k, n = 0;
    if (!ps->on)
        return 0;
    for (p = 0; p < QD_NPROJ; p++) {
        int old = 0;
        uint16_t ref[QD_PAT_NTRACK];
        if (!ps->pexists[p])
            continue;
        for (k = 0; k < QD_NPAT && !old; k++) {
            const int v = pat_rec_refs(&ps->prj_st, p, k, ref);
            old = v && v < 5;
        }
        if (!old)
            continue;
        if (p != ps->proj && !qd_proj_load(a, p))
            continue;
        if (ps->job >= 0) {
            qd_store_flush(item_store(a, ps->job));
            job_end(a);
        }
        for (k = 0; k < QD_NPAT; k++) {
            const int v = pat_rec_refs(&ps->prj_st, p, k, ref);
            if (v && v < 5)
                n += pat_rewrite(a, &ps->prj_st, p, k);
        }
    }
    if (ps->proj != first)
        qd_proj_load(a, first);
    return n;
}

/* ---------------------------------------------------------------- MY presets (the library's) */
int qd_lib_free_slot(const qd_app_t *a)
{
    int k;
    for (k = 0; k < QD_NLIB; k++)
        if (!qd_lib_used(a, k))
            return k;
    return -1;
}

/* a new preset id: above every one this app knows (MINE's, the pool's, the tracks') */
static uint16_t lib_new_id(const qd_app_t *a)
{
    unsigned m = QD_PRESET_MINE0 + QD_NLIB - 1;
    int k, t;
    for (k = 0; k < QD_NLIB; k++)
        if (qd_lib_used(a, k) && qd_lib_preset(a, k) > m)
            m = qd_lib_preset(a, k);
    for (k = 1; k <= QD_NSOUND; k++)
        if (qd_sound_preset(&a->proj, k) > m)
            m = qd_sound_preset(&a->proj, k);
    /* (2026-10-25: the patterns' track sounds not looked at: paging them in read 64 records under the lock, ~1 s
     * on the unit; id_next, below, covers every id given out) */
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
    if (qd_lib_used(a, slot))                           /* (P2.1: over one that is there: its references copies) */
        qd_preset_will_change(a, qd_lib_preset(a, slot));
    n.preset = qd_lib_used(a, slot) ? qd_lib_preset(a, slot) : lib_new_id(a);
    if (n.preset >= a->per.id_next)
        a->per.id_next = (uint16_t)(n.preset + 1u);
    n.tags = (uint8_t)qd_preset_tags(patch);
    n.flags = (uint8_t)flags;
    qd_undo_snd(a, QD_UNDO_RLIB, slot);
    qd_lib_write(a, slot, &n);
    return 1;
}

int qd_lib_store_ex(qd_app_t *a, int slot, const char *name, const int16_t *patch, int flags)
{
    const int was = qd_lib_used(a, slot);
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
    qd_sound_t s;
    char nm[QD_SOUND_NAME];
    qd_undo_hint(a, QD_UK_SOUNDS);
    if (slot < 0 || slot >= QD_NLIB || !qd_lib_used(a, slot))
        return 0;
    qd_lib_read(a, slot, &s);
    qd_preset_will_change(a, s.preset);                /* (P2.1: the tracks that loaded it keep their copies) */
    qd_undo_snd(a, QD_UNDO_RLIB, slot);
    memset(nm, 0, sizeof nm);
    strncpy(nm, name && name[0] ? name : s.name, QD_SOUND_NAME - 1);
    memcpy(s.name, nm, sizeof nm);
    memcpy(s.patch, patch, sizeof s.patch);
    s.flags = 0;                                        /* (saved here now; P1.5v: its tags kept) */
    qd_lib_write(a, slot, &s);
    pool_resolve(a, 0);                                 /* (P1.5w: the pool's references to it play it now) */
    return 1;
}

int qd_lib_delete(qd_app_t *a, int slot)
{
    qd_undo_hint(a, QD_UK_SOUNDS);                      /* (P1.5c: one undo level) */
    if (slot < 0 || slot >= QD_NLIB || !qd_lib_used(a, slot))
        return 0;
    qd_preset_will_change(a, qd_lib_preset(a, slot));  /* (P2.1) */
    qd_undo_snd(a, QD_UNDO_RLIB, slot);
    {
        qd_sound_t z;
        memset(&z, 0, sizeof z);
        qd_lib_write(a, slot, &z);
    }
    return 1;
}

int qd_lib_find(const qd_app_t *a, int id)
{
    int k;
    if (id < QD_PRESET_MINE0)
        return -1;
    for (k = 0; k < QD_NLIB; k++)
        if (qd_lib_used(a, k) && qd_lib_preset(a, k) == id)
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
    qd_lib_read(a, k, out);
    return 1;
}

/* ---------------------------------------------------------------- P1.5v: the tags */
int qd_factory_tags(const qd_app_t *a, int k)
{
    if (k >= 0 && k < QD_NFACTORY && (a->per.ftags[k] & 0x80u))
        return (int)(a->per.ftags[k] & QD_PK_MASK);
    return qd_factory_tag(k);
}

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
        const int k = code - QD_BR_POOL_CODE;
        const uint16_t pre = qd_sound_preset(&a->proj, k);
        int m;
        if (!qd_sound_used(&a->proj, k))
            return 0;
        if (pre >= 1 && pre <= QD_NFACTORY)
            return qd_tags_set(a, pre - 1, tags);
        m = qd_lib_find(a, pre);
        if (m >= 0)
            return qd_tags_set(a, QD_BR_MINE_CODE + m, tags);
        qd_undo_hint(a, QD_UK_SOUNDS);                  /* (a copy) */
        qd_undo_snd(a, QD_UNDO_RPOOL, k);
        {
            qd_sound_t c;
            qd_sound_read(&a->proj, k, &c);
            c.tags = (uint8_t)tags;
            qd_sound_write(&a->proj, k, &c);
        }
        return 1;
    }
    if (code >= QD_BR_MINE_CODE) {                      /* (one of MY presets) */
        const int m = code - QD_BR_MINE_CODE;
        qd_sound_t c;
        if (m >= QD_NLIB || !qd_lib_used(a, m))
            return 0;
        qd_undo_hint(a, QD_UK_SOUNDS);
        qd_undo_snd(a, QD_UNDO_RLIB, m);
        qd_lib_read(a, m, &c);
        c.tags = (uint8_t)tags;
        qd_lib_write(a, m, &c);
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
static int rec_pat_steps(rd_t *r, const uint8_t *mask)
{
    uint8_t h[12];
    int t, pg, s, n = 0;
    uint32_t tsz;
    if (!rd_get(r, h, 12) || !(tsz = pat_tset_size(h, r->len)))
        return 0;
    r->pos += (uint32_t)(tsz * QD_PAT_NTRACK + sizeof(((qd_pattern_t *)0)->lock_start) +
                         sizeof(((qd_pattern_t *)0)->lock_n)) + (h[1] >= 5 ? 2u * QD_PAT_NTRACK : 0u);
    for (t = 0; t < QD_PAT_NTRACK && rd_left(r); t++) {
        uint8_t pm;
        rd_u8(r, &pm);
        for (pg = 0; pg < QD_NPAGE; pg++) {
            uint16_t sm;
            if (!((pm >> pg) & 1u))
                continue;
            if (!rd_get(r, &sm, 2))
                return n;
            for (s = 0; s < QD_PAGE; s++)
                if ((sm >> s) & 1u) {
                    qd_step_t st;
                    uint8_t w[QD_STEP_WIRE];
                    if (!rd_get(r, w, QD_STEP_WIRE))
                        return n;
                    qd_step_unwire(&st, w);                 /* (P2.1c: the records' layout) */
                    n += st.sound && ((mask[st.sound >> 3] >> (st.sound & 7)) & 1u) && (st.flags & QD_SF_ON);
                }
        }
    }
    return n;
}

/* (web manager P1) one project's part of qd_preset_usage: 1 its pool references id (steps: the steps locking it;
 * tracks: this project's tracks playing or locking it) */
int qd_preset_usage_proj(const qd_app_t *a, int id, int p, int *steps, uint8_t *tracks)
{
    const qd_persist_t *ps = &a->per;
    int k, ns = 0;
    uint8_t mask[(QD_NSOUND + 8) / 8];
    int used = 0;
    if (p == ps->proj) {                                /* this project: its pool in RAM */
        for (k = 1; k <= QD_NSOUND; k++)
            if (qd_sound_used(&a->proj, k) && qd_sound_preset(&a->proj, k) == id) {
                uint8_t m = 0;
                used = 1;
                ns += qd_pool_users_all(a, k, &m);
                if (tracks)
                    *tracks |= m;
            }
        if (steps)
            *steps = ns;
        return used;
    }
    if (steps)
        *steps = 0;
    if (!ps->on || p < 0 || p >= QD_NPROJ || !ps->pexists[p])
        return 0;
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
        return 0;
    for (k = 0; k < QD_NPAT; k++) {
        rd_t r;
        const int len = rd_open(&r, (qd_store_t *)&ps->prj_st, QD_RKEY(QD_RK_PAT, p, k));
        if (len > 0 && len != (int)QDS_DELETED)
            ns += rec_pat_steps(&r, mask);
    }
    if (steps)
        *steps = ns;
    return 1;
}

void qd_preset_usage(const qd_app_t *a, int id, int *projects, int *steps)
{
    int p, np = 0, ns = 0;
    for (p = 0; p < QD_NPROJ; p++) {                    /* (this project from RAM, the others from their records) */
        int s = 0;
        if (qd_preset_usage_proj(a, id, p, &s, 0))
            np++, ns += s;
    }
    if (projects)
        *projects = np;
    if (steps)
        *steps = ns;
}

int qd_preset_delete(qd_app_t *a, int slot)
{
    int k, id;
    qd_sound_t z;
    if (slot < 0 || slot >= QD_NLIB || !qd_lib_used(a, slot))
        return 0;
    id = qd_lib_preset(a, slot);
    qd_undo_hint(a, QD_UK_SOUNDS);                      /* (one level: the preset, its references, their locks) */
    qd_preset_will_change(a, id);                       /* (P2.1: the tracks that loaded it keep their copies) */
    for (k = 1; k <= QD_NSOUND; k++)
        if (qd_sound_used(&a->proj, k) && qd_sound_preset(&a->proj, k) == id)
            pool_unref(a, k, a->undo.open);
    qd_undo_snd(a, QD_UNDO_RLIB, slot);
    memset(&z, 0, sizeof z);
    qd_lib_write(a, slot, &z);
    return 1;
}

void qd_lib_unique_name(const qd_app_t *a, const char *nm, char *out)
{
    int n, k, clash;
    snprintf(out, QD_SOUND_NAME, "%s", nm);
    for (n = 2; n < 100; n++) {
        for (clash = 0, k = 0; k < QD_NLIB && !clash; k++)
            clash = qd_lib_used(a, k) && !strcmp(qd_lib_name(a, k), out);
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

/* ---------------------------------------------------------------- P2.1: the patterns' backing (pattern.h's io) */
#if QD_PAT_RAM < QD_NPAT
/* the store the patterns go out to: the flash's (the projects' store), or with no flash attached (the host's tests and
 * tools at the device's sizes: never the device itself, whose platform always attaches its flash) a RAM one */
#ifndef QD_TARGET_FM1
static int sw_read(void *c, uint32_t off, void *dst, uint32_t n)
{
    memcpy(dst, ((qd_app_t *)c)->per.swap_nor + off, n);
    return 0;
}
static int sw_erase(void *c, uint32_t off)
{
    memset(((qd_app_t *)c)->per.swap_nor + (off & ~(QDS_SECTOR - 1u)), 0xFF, QDS_SECTOR);
    return 0;
}
static int sw_prog(void *c, uint32_t off, const void *src, uint32_t n)
{
    uint8_t *d = ((qd_app_t *)c)->per.swap_nor + off;
    uint32_t i;
    for (i = 0; i < n; i++)
        d[i] &= ((const uint8_t *)src)[i];
    return 0;
}
#endif
static qd_store_t *pat_store(qd_app_t *a)
{
    if (a->per.on)
        return &a->per.prj_st;
#ifndef QD_TARGET_FM1
    if (!a->per.swap_on) {
        const qd_flash_t fl = {sw_read, sw_erase, sw_prog, a};
        memset(a->per.swap_nor, 0xFF, sizeof a->per.swap_nor);
        qd_store_mount(&a->per.swap_st, &fl, 0, QD_SWAP_SEC, a->per.swap_ent, QD_SWAP_KEYS);
        a->per.swap_on = 1;
    }
    return &a->per.swap_st;
#else
    return 0;
#endif
}

/* pattern k from its record (none, or one of another layout: its default), its track sounds of 0.6 put right; what
 * the store holds of it: its hash */
static int pat_load(void *ctx, int k, qd_pattern_t *dst)
{
    qd_app_t *a = (qd_app_t *)ctx;
    qd_store_t *st = pat_store(a);
    int ok = 0;
    qd_pat_default(&a->proj, k, dst);
    if (st) {
        rd_t r;
        const int len = rd_open(&r, st, QD_RKEY(QD_RK_PAT, a->per.proj, k));
        if (len > 0 && len != (int)QDS_DELETED) {
            ok = de_pat(a, dst, &r);
            if (!ok)
                qd_pat_default(&a->proj, k, dst);
        }
    }
    tsound_fixup(a, dst);
    a->per.saved[QD_PI_PAT + k] = a->per.seen[QD_PI_PAT + k] = pat_hash(a, dst);
    a->per.changed[QD_PI_PAT + k] = a->ms;
#if QD_PAT_RAM < QD_NPAT
    pat_unref_pending(a, k, dst);                       /* (2026-10-24: a removed sound's locks: after the hash, so
                                                         * written again) */
#endif
    return 1;
}

/* pattern k leaving RAM: written to the store now if it is not what the store holds (its slot is taken at once) */
static void pat_save(void *ctx, int k, const qd_pattern_t *src)
{
    qd_app_t *a = (qd_app_t *)ctx;
    qd_persist_t *ps = &a->per;
    qd_store_t *st = pat_store(a);
    pat_src_t c;
    em_t e;
    if (!st)
        return;
    em_init(&e, 0, 0, 0);
    ser_pat(a, src, &e);
    if (em_hash(&e) == ps->saved[QD_PI_PAT + k])
        return;                                         /* (as the store holds it) */
    if (ps->on && ps->job >= 0) {                       /* (the record being written first) */
        qd_store_flush(item_store(a, ps->job));
        job_end(a);
    }
    qd_store_flush(st);
    c.a = a, c.P = src;
    if (qd_store_put_src(st, QD_RKEY(QD_RK_PAT, ps->proj, k), e.pos, pat_src, 0, &c))
        qd_store_flush(st);
    ps->saved[QD_PI_PAT + k] = ps->seen[QD_PI_PAT + k] = em_hash(&e);
    if (ps->on)
        ps->written_ms[k] = a->ms ? a->ms : 1u;
}

/* P2.4: what the store holds of pattern k is P as it is (it can leave RAM with no write): the fast hash first (the
 * same as at the last look: the record's hash then), else the record's hash worked out; not while it is being written */
static int pat_clean(void *ctx, int k, const qd_pattern_t *P)
{
    qd_app_t *a = (qd_app_t *)ctx;
    qd_persist_t *ps = &a->per;
    const int it = QD_PI_PAT + k;
    uint32_t r;
    if (!pat_store(a))
        return 1;
    if (ps->job == it)
        return 0;
    r = pat_raw_of(a, P);
    if (r != ps->pat_raw[k]) {
        const uint32_t h = pat_hash(a, P);
        ps->pat_raw[k] = r;
        if (h != ps->seen[it]) {
            ps->seen[it] = h;
            ps->changed[it] = a->ms;
            ps->maybe_dirty = 1;
        }
    }
    return ps->seen[it] == ps->saved[it];
}
static void pat_save_soon(void *ctx, int k) { qd_persist_soon((qd_app_t *)ctx, QD_PI_PAT + k); }

/* the patterns that must stay in RAM now: the sequencer's (playing, queued, the tracks', the chain's next, the trigs
 * and the MIDI FX notes waiting), the open undo gesture's, the record being written */
static int pat_pinned(void *ctx, int k)
{
    const qd_app_t *a = (const qd_app_t *)ctx;
    const qd_seq_t *s = &a->seq;
    int i, g, sl, h;
    if (k == s->cur || (a->undo.open && k == a->undo.pat))
        return 1;
    if (a->per.job == QD_PI_PAT + k && qd_store_busy(&a->per.prj_st))
        return 1;
    if (!s->playing)                                    /* P2.4: stopped, the cursors' last patterns pin nothing */
        return 0;
    if (k == s->sw_pat || (s->nchain > 1 && k == s->chain[(s->cpos + 1) % s->nchain]))
        return 1;
    for (i = 0; i < QD_PAT_NTRACK; i++)
        if (s->cur_t[i].pat == k)
            return 1;
    for (i = 0; i < QD_SEQ_NREC; i++)
        if (s->rec[i].used && s->rec[i].pat == k)
            return 1;
    for (g = 0; g < QD_MFX_NGROUP; g++)
        for (sl = 0; sl < QD_MFX_NSLOT; sl++) {
            const qd_mfx_srt_t *r = qd_mfx_srt(&a->player.mfx, g, sl);   /* (P2.1b: the pool's) */
            for (h = 0; r && h < QD_MFX_NHOLD; h++)
                if (r->h[h].used && r->h[h].n.pat == k)
                    return 1;
        }
    return 0;
}

#if QD_SND_RAM
/* P2.1: the pool's and MY presets' sounds out of RAM (pattern.h qd_sndc_t): their records in the store (the flash's: a
 * pool slot's in the projects' store, a MY preset's in the library's; no flash: the RAM one) */
static int snd_item(int key) { return key >= QD_SK_LIB ? QD_PI_LIB + key - QD_SK_LIB : QD_PI_SND + key - 1; }
static qd_store_t *snd_store(qd_app_t *a, int key)
{
    if (a->per.on)
        return key >= QD_SK_LIB ? &a->per.lib_st : &a->per.prj_st;
    return pat_store(a);
}
static uint32_t snd_hash(const qd_sound_t *s, int pool, uint8_t *b, uint32_t *n)
{
    em_t e;
    *n = snd_rec(s, pool, b);
    em_init(&e, 0, 0, 0);
    em(&e, b, *n);
    return em_hash(&e);
}
/* a sound from its record (none: its default: a pool slot of a project that started with the factory's, that sound;
 * else empty); what the store holds of it: its hash */
static void snd_load(void *ctx, int key, qd_sound_t *dst)
{
    qd_app_t *a = (qd_app_t *)ctx;
    qd_store_t *st = snd_store(a, key);
    const int it = snd_item(key);
    uint8_t b[2 + QD_SOUND_NAME + QD_QUAD_BLOB + 16];
    uint32_t n;
    int len = -1;
    memset(dst, 0, sizeof *dst);
    if (st && qd_store_busy(st) && st->job_key == item_key(a, it))
        qd_store_flush(st);                             /* (its record queued as it left RAM: written first) */
    if (st)
        len = qd_store_get(st, item_key(a, it), b, sizeof b);
    if (len > 0 && len != (int)QDS_DELETED)
        snd_from_rec(a, key, dst, b, (uint32_t)len);
    else if (key < QD_SK_LIB && !a->proj.pool_empty && key <= QD_NFACTORY) {
        dst->preset = (uint16_t)key;                    /* (the factory's: a reference to it, with the owner's tags) */
        pool_cache(a, dst);
    }
    a->per.saved[it] = a->per.seen[it] = snd_hash(dst, key < QD_SK_LIB, b, &n);
    a->per.changed[it] = a->ms;
}
/* a sound leaving RAM: written now if it is not what the store holds (its own record being written: finished first) */
static void snd_save(void *ctx, int key, const qd_sound_t *src)
{
    qd_app_t *a = (qd_app_t *)ctx;
    qd_persist_t *ps = &a->per;
    qd_store_t *st = snd_store(a, key);
    const int it = snd_item(key);
    uint8_t b[2 + QD_SOUND_NAME + QD_QUAD_BLOB + 16];
    uint32_t n, h;
    if (!st)
        return;
    h = snd_hash(src, key < QD_SK_LIB, b, &n);
    if (ps->on && ps->job == it) {
        qd_store_flush(item_store(a, it));
        job_end(a);
    }
    if (h == ps->saved[it])
        return;                                         /* (as the store holds it) */
    if (ps->on && !qd_store_busy(st) && n <= QDS_JOBBUF && qd_store_put(st, item_key(a, it), b, n)) {
        ps->saved[it] = ps->seen[it] = h;               /* (2026-10-24: queued, copied; written by qd_persist_step, a
                                                         * page a step, not here: snd_load waits for it if asked) */
        return;
    }
    if (ps->on && ps->job >= 0) {                       /* (the record being written first) */
        qd_store_flush(item_store(a, ps->job));
        job_end(a);
    }
    put_now(st, item_key(a, it), b, n);
    ps->saved[it] = ps->seen[it] = h;
}
#endif

#if QD_SND_RAM
static int snd_clean(void *ctx, int key, const qd_sound_t *s)
{
    qd_app_t *a = (qd_app_t *)ctx;
    const int it = snd_item(key);
    uint8_t b[2 + QD_SOUND_NAME + QD_QUAD_BLOB + 16];
    uint32_t n;
    if (!snd_store(a, key))
        return 1;
    return a->per.job != it && snd_hash(s, key < QD_SK_LIB, b, &n) == a->per.saved[it];
}
static void snd_save_soon(void *ctx, int key) { qd_persist_soon((qd_app_t *)ctx, snd_item(key)); }
#endif

void qd_persist_io(qd_app_t *a)
{
    a->proj.io.load = pat_load;
    a->proj.io.save = pat_save;
    a->proj.io.pinned = pat_pinned;
    a->proj.io.clean = pat_clean;
    a->proj.io.save_soon = pat_save_soon;
    a->proj.io.ctx = a;
#ifndef QD_TARGET_FM1
    a->per.swap_on = 0;
#endif
#if QD_SND_RAM
    qd_sndc_init(&a->per.sc);
    a->per.sc.io.load = snd_load;
    a->per.sc.io.save = snd_save;
    a->per.sc.io.clean = snd_clean;
    a->per.sc.io.save_soon = snd_save_soon;
    a->per.sc.io.ctx = a;
    a->proj.sc = &a->per.sc;
#endif
}
#else
void qd_persist_io(qd_app_t *a) { (void)a; }
#endif

/* the patterns the sequencer will ask for read in now (the UI's time, not the audio's). P2.4: without a write
 * (pattern.h qd_proj_fetch): a slot waiting for its pattern's record to be written is taken once it is (asked again
 * 8 ms on); then the pool sounds the playing tracks' next steps lock (sounds.h qd_sndc_fetch): the audio side reads
 * no sound in, a lock whose sound is not in RAM plays the track's own */
#if QD_SND_RAM
#define PF_SND (QD_SND_RAM - 2)                           /* (two places left to the UI's reading) */
#define PF_AHEAD 16                                       /* (the steps a track looked at: a bounded scan) */
static int pf_add(int *keys, int n, int k)
{
    int i;
    for (i = 0; i < n; i++)
        if (keys[i] == k)
            return n;
    keys[n] = k;
    return n + 1;
}
/* the sound locks soon wanted: the queued pattern's first two steps a track, then each track's next PF_AHEAD steps
 * (playing: from its cursor, in its pattern; stopped: the pattern's own from its start), the tracks in turn, then the
 * queued one's up to PF_AHEAD; at most PF_SND */
static int pf_sounds(qd_app_t *a, int *keys)
{
    const qd_seq_t *s = &a->seq;
    const qd_pattern_t *PT[3][QD_PAT_NTRACK];
    int LEN[3][QD_PAT_NTRACK], n = 0, j, t, q;
    for (q = 0; q < 3; q++)                             /* (P2.6c: each track's pattern and loop once a scan) */
        for (t = 0; t < QD_PAT_NTRACK; t++) {
            const int pat = q != 1 ? s->sw_pat : s->playing ? s->cur_t[t].pat : s->cur;
            PT[q][t] = pat >= 0 && qd_proj_resident(&a->proj, pat) ? qd_proj_cpat(&a->proj, pat) : 0;
            LEN[q][t] = PT[q][t] ? qd_track_len(&PT[q][t]->trk[t]) : 0;
        }
    for (q = 0; q < 3 && n < PF_SND; q++)
        for (j = q == 2 ? 2 : 0; j < (q == 0 ? 2 : PF_AHEAD) && n < PF_SND; j++)
            for (t = 0; t < QD_PAT_NTRACK && n < PF_SND; t++) {
                const qd_pattern_t *P = PT[q][t];
                const int len = LEN[q][t];
                int idx, st;
                if (!P || j >= len)
                    continue;
                idx = q == 1 && s->playing ? (s->cur_t[t].idx + j) % len : j;
                st = qd_track_loop_step(&P->trk[t], idx < 0 ? 0 : idx);
                if (st >= 0 && st < QD_NSTEP && (P->step[t][st].flags & QD_SF_ON) &&
                    (P->step[t][st].ovr & QD_OV(QD_PM_SOUND)) && P->step[t][st].sound &&
                    qd_sound_used(&a->proj, P->step[t][st].sound))
                    n = pf_add(keys, n, P->step[t][st].sound);
            }
    return n;
}
#endif
void qd_persist_prefetch(qd_app_t *a)
{
    const qd_seq_t *s = &a->seq;
    int need[3], n = 0, i, wait = 0;
    a->proj.pat_fb = (int8_t)s->cur;                    /* (the playing one first: it may take a pinned slot) */
    need[n++] = s->cur;
    if (s->sw_pat >= 0)
        need[n++] = s->sw_pat;
    if (s->nchain > 1)
        need[n++] = s->chain[(s->cpos + 1) % s->nchain];
    for (i = 0; i < n; i++)
        if (!qd_proj_resident(&a->proj, need[i]))
            break;
    if (i < n && (!a->per.fetch_ms || a->ms - a->per.fetch_ms >= 8u)) {
        for (i = 0; i < n; i++)
            if (!qd_proj_fetch(&a->proj, need[i]))
                wait = 1;
        a->per.fetch_ms = wait ? (a->ms ? a->ms : 1u) : 0u;
    }
#if QD_SND_RAM
    if (a->proj.sc && a->ms - a->per.snd_pf_ms >= 8u) {   /* (P2.6c: every 8 ms by the clock: a coalesced tick too) */
        uint32_t key = 2166136261u;                     /* (P2.6c: what the scan reads: the same, no scan) */
        int keys[PF_SND], t;
        a->per.snd_pf_ms = a->ms;
        key = (key ^ (uint32_t)s->cur) * 16777619u;
        key = (key ^ (uint32_t)s->sw_pat) * 16777619u;
        key = (key ^ (uint32_t)s->playing) * 16777619u;
        key = (key ^ a->edit_gen) * 16777619u;
        for (t = 0; s->playing && t < QD_PAT_NTRACK; t++)
            key = (key ^ ((uint32_t)s->cur_t[t].idx << 8 ^ (uint32_t)s->cur_t[t].pat)) * 16777619u;
        if (key != a->per.snd_pf_key || a->per.snd_pf_left) {
            int nk;
            a->per.snd_pf_key = key;
            nk = pf_sounds(a, keys);
            a->per.snd_pf_left = 0;
            for (i = 0; i < nk; i++)
                if (!qd_sndc_fetch(a->proj.sc, keys[i], keys, nk))
                    a->per.snd_pf_left = 1;     /* (one waits for a place: scanned again) */
        }
    }
#endif
}
