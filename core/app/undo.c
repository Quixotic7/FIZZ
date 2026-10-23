/* SPDX-License-Identifier: GPL-3.0-only */
/* P1.5c: undo / redo, the snapshot, the pattern clipboard (undo.h). Built through app.c (it includes this file: the
 * core's source lists stay as they are). P2.1: at the device's sizes (QD_UNDO_SLOTS) a 16 KB log with no whole copies
 * (undo.h); the host's as before. */

typedef char qd_undo_lib_ok[QD_NLIB_UNDO == QD_NLIB ? 1 : -1];

#ifdef QD_UNDO_SLOTS
/* (P2.1: the patterns compressed: seq/zr.h) */
#include "../seq/zr.h"
#endif

void qd_undo_init(qd_app_t *a)
{
    qd_hist_t *u = &a->undo;
    u->open = 0;
    u->n = u->pos = 0;
    u->used = 0;
    u->snap_ok = u->pclip_ok = 0;
#ifdef QD_UNDO_SLOTS
    u->top = 0;
    u->snap_len = u->pclip_len = 0;
#endif
}

void qd_undo_reset(qd_app_t *a)
{
    a->undo.open = 0;
    a->undo.n = a->undo.pos = 0;
    a->undo.used = 0;
#ifdef QD_UNDO_SLOTS
    a->undo.top = 0;
#endif
}

static void undo_drop_oldest(qd_hist_t *u)
{
    int i;
    const uint32_t cut = u->lvl[0].len;
    if (!u->n)
        return;
    memmove(u->pool, u->pool + cut, u->used - cut);
    u->used -= cut;
    for (i = 1; i < u->n; i++) {
        u->lvl[i - 1] = u->lvl[i];
        u->lvl[i - 1].off -= cut;
    }
    u->n--;
    if (u->pos > 0)
        u->pos--;
}

static void undo_take(qd_app_t *a)                    /* the gesture's "before" */
{
    qd_hist_t *u = &a->undo;
    u->pat = a->seq.cur;
#ifdef QD_UNDO_SLOTS
    {   /* P2.1: the pattern compressed at the log's top (the oldest levels make room) */
        const uint8_t *P = (const uint8_t *)qd_proj_cpat(&a->proj, u->pat);
        const uint32_t sz = zr_pack(P, sizeof(qd_pattern_t), 0, 0);
        while (sz <= QD_UNDO_POOL && u->n && u->used + sz > QD_UNDO_POOL)   /* (one that never fits: the levels
                                                         * stay) */
            undo_drop_oldest(u);
        u->b_ok = u->used + sz <= QD_UNDO_POOL;
        u->top = u->b_ok ? QD_UNDO_POOL - sz : 0u;
        if (u->b_ok)
            zr_pack(P, sizeof(qd_pattern_t), u->pool + u->top, sz);
        u->nslot = 0;
    }
#else
    u->pat0 = *qd_proj_pat(&a->proj, u->pat);
    memcpy(u->snd0, a->proj.sound, sizeof u->snd0);
    memcpy(u->lib0, a->per.lib, sizeof u->lib0);
#endif
    u->nsound0 = a->proj.nsound;
    memcpy(u->mfx0, a->proj.mfx, sizeof u->mfx0);     /* P1.6c */
    u->xl_n = 0;
    u->xl_over = 0;
    u->hint = -1;
    u->knob = 0;
    u->open = 1;
}

void qd_undo_xlock(qd_app_t *a, int pat, int t, int s, int k, int ovr)
{
    qd_hist_t *u = &a->undo;
    uint8_t *e;
    if (!u->open)
        return;
    if (u->xl_n >= QD_UNDO_XLOCKS) {
        u->xl_over = 1;
        return;
    }
    e = u->xl + u->xl_n * 5u;
    e[0] = (uint8_t)pat, e[1] = (uint8_t)t, e[2] = (uint8_t)s, e[3] = (uint8_t)k, e[4] = (uint8_t)(ovr != 0);
    u->xl_n++;
}

#ifdef QD_UNDO_SLOTS
/* P2.1: a pool slot (1..128) or a library slot (0..255) as it is now (0: none such); written back */
static int undo_slot(qd_app_t *a, int region, int slot, qd_sound_t *out)
{
    if (region == QD_UNDO_RPOOL && slot >= 1 && slot <= QD_NSOUND) {
        qd_sound_read(&a->proj, slot, out);
        return 1;
    }
    if (region == QD_UNDO_RLIB && slot >= 0 && slot < QD_NLIB) {
        qd_lib_read(a, slot, out);
        return 1;
    }
    return 0;
}
static void undo_slot_put(qd_app_t *a, int region, int slot, const qd_sound_t *s)
{
    if (region == QD_UNDO_RPOOL) {
        qd_sound_write(&a->proj, slot, s);
    } else if (region == QD_UNDO_RLIB) {
        if (qd_lib_used(a, slot))                       /* (the 'P' 5 records' references to it: copies first) */
            qd_preset_will_change(a, qd_lib_preset(a, slot));
        qd_lib_write(a, slot, s);
    }
}
#endif

void qd_undo_snd(qd_app_t *a, int region, int slot)
{
#ifdef QD_UNDO_SLOTS
    qd_hist_t *u = &a->undo;
    qd_sound_t s;
    int i;
    if (!u->open || !undo_slot(a, region, slot, &s))
        return;
    for (i = 0; i < u->nslot; i++)
        if (u->slot_r[i] == region && u->slot_k[i] == slot)
            return;                                     /* (kept already: its first "before" counts) */
    if (u->nslot >= QD_UNDO_NSLOT) {
        u->b_ok = 0;                                    /* (more than the log keeps: the gesture is no level) */
        return;
    }
    u->slot_r[u->nslot] = (uint8_t)region, u->slot_k[u->nslot] = (uint8_t)slot;
    memcpy(&u->slot0[u->nslot], &s, sizeof s);
    u->nslot++;
#else
    (void)a, (void)region, (void)slot;
#endif
}

static void undo_follow(qd_app_t *a)                  /* the active pattern changed under an open gesture: it closes */
{
    if (a->undo.open && a->undo.pat != a->seq.cur)
        qd_undo_close(a);
}

void qd_undo_input(qd_app_t *a, int knob)
{
    qd_hist_t *u = &a->undo;
    undo_follow(a);
    if (!u->open)
        undo_take(a);
    u->last_ms = a->ms;
    u->knob |= knob;
}

void qd_undo_hint(qd_app_t *a, int kind)
{
    undo_follow(a);
    if (kind != QD_UK_REC && a->undo.open && !(a->undo.join && a->undo.hint == kind)) {           /* an operation (a paste, a clear, a randomise ..): a level of its
                                                         * own, whatever came before it in the gesture (a take's notes:
                                                         * one level together) */
        const uint32_t last = a->undo.last_ms;
        qd_undo_close(a);
        undo_take(a);
        a->undo.last_ms = last;
    }
    if (!a->undo.open)
        undo_take(a);
    if (a->undo.hint < 0)
        a->undo.hint = kind;
}

/* the XOR difference of a region (now ^ before) into out (P2.1: out 0: counted only; no copy of a level is made before
 * it goes into the pool): runs of (offset u32, length u16, bytes); a run ends at 8 equal bytes in a row. Returns the
 * bytes (0: no change), -1: no room */
static long undo_diff(uint8_t *out, uint32_t room, const uint8_t *now, const uint8_t *before, uint32_t n)
{
    uint32_t i = 0, o = 0;
    while (i < n) {
        uint32_t s, e, eq = 0;
        if (now[i] == before[i]) {
            i++;
            continue;
        }
        s = i;
        e = i;
        while (i < n && eq < 8u) {
            if (now[i] == before[i])
                eq++;
            else
                eq = 0, e = i;
            i++;
        }
        while (s <= e) {                                 /* (a run of at most 65535 bytes) */
            const uint32_t len = e - s + 1u > 65535u ? 65535u : e - s + 1u;
            uint32_t k;
            if (out) {
                if (o + 6u + len > room)
                    return -1;
                memcpy(out + o, &s, 4);
                out[o + 4] = (uint8_t)len, out[o + 5] = (uint8_t)(len >> 8);
                for (k = 0; k < len; k++)
                    out[o + 6u + k] = (uint8_t)(now[s + k] ^ before[s + k]);
            }
            o += 6u + len;
            s += len;
        }
    }
    return (long)o;
}

#ifndef QD_UNDO_SLOTS
/* the library's runs: each MY preset they touch, before (the 'P' 5 records' references to it made copies) */
static void undo_lib_unref(qd_app_t *a, const uint8_t *d, uint32_t len)
{
    uint32_t o = 0;
    while (o + 6u <= len) {
        uint32_t s, l, k;
        memcpy(&s, d + o, 4);
        l = (uint32_t)d[o + 4] | (uint32_t)d[o + 5] << 8;
        for (k = s / sizeof(qd_sound_t); l && k <= (s + l - 1u) / sizeof(qd_sound_t) && k < QD_NLIB; k++)
            if (qd_lib_used(a, (int)k))
                qd_preset_will_change(a, qd_lib_preset(a, (int)k));
        o += 6u + l;
    }
}
#endif
static void undo_apply_runs(uint8_t *mem, uint32_t n, const uint8_t *d, uint32_t len)
{
    uint32_t o = 0;
    while (o + 6u <= len) {
        uint32_t s, l, k;
        memcpy(&s, d + o, 4);
        l = (uint32_t)d[o + 4] | (uint32_t)d[o + 5] << 8;
        for (k = 0; k < l && s + k < n; k++)
            mem[s + k] ^= d[o + 6u + k];
        o += 6u + l;
    }
}

/* a level: four regions' differences (the pattern, the pool, the library, P1.6c the MIDI FX groups), each [u32 bytes]
 * then its runs, then the other patterns' locks. P2.1, the device's: the pool's region holds the slots' entries
 * (region, slot, the slot's XOR with what it was), the library's is empty */
static void undo_regions(qd_app_t *a, const uint8_t **now, const uint8_t **was, uint32_t *sz)
{
    qd_hist_t *u = &a->undo;
#ifdef QD_UNDO_SLOTS
    qd_pattern_t *S = qd_pat_scratch();
    zr_unpack(u->pool + u->top, QD_UNDO_POOL - u->top, (uint8_t *)S, sizeof *S);
    u->top = 0;                                         /* (unpacked: its room is the levels' again) */
    now[0] = (const uint8_t *)qd_proj_cpat(&a->proj, u->pat), was[0] = (const uint8_t *)S, sz[0] = sizeof *S;
    now[1] = was[1] = 0, sz[1] = 0;
    now[2] = was[2] = 0, sz[2] = 0;
#else
    now[0] = (const uint8_t *)qd_proj_cpat(&a->proj, u->pat), was[0] = (const uint8_t *)&u->pat0, sz[0] = sizeof u->pat0;
    now[1] = (const uint8_t *)a->proj.sound, was[1] = (const uint8_t *)u->snd0, sz[1] = sizeof u->snd0;
    now[2] = (const uint8_t *)a->per.lib, was[2] = (const uint8_t *)u->lib0, sz[2] = sizeof u->lib0;
#endif
    now[3] = (const uint8_t *)a->proj.mfx, was[3] = (const uint8_t *)u->mfx0, sz[3] = sizeof u->mfx0;
}

#ifdef QD_UNDO_SLOTS
/* P2.1: the slots' entries (those that changed) into out (0: counted); the bytes */
static uint32_t undo_slots(qd_app_t *a, uint8_t *out)
{
    qd_hist_t *u = &a->undo;
    uint32_t o = 0;
    int i;
    for (i = 0; i < u->nslot; i++) {
        qd_sound_t cur;
        const uint8_t *now = (const uint8_t *)&cur;
        const uint8_t *was = (const uint8_t *)&u->slot0[i];
        uint32_t k, d = 0;
        if (!undo_slot(a, u->slot_r[i], u->slot_k[i], &cur))
            continue;
        for (k = 0; k < sizeof(qd_sound_t); k++)
            d |= (uint32_t)(now[k] ^ was[k]);
        if (!d)
            continue;
        if (out) {
            out[o] = u->slot_r[i], out[o + 1] = u->slot_k[i];
            for (k = 0; k < sizeof(qd_sound_t); k++)
                out[o + 2 + k] = (uint8_t)(now[k] ^ was[k]);
        }
        o += 2u + (uint32_t)sizeof(qd_sound_t);
    }
    return o;
}
#endif

void qd_undo_close(qd_app_t *a)
{
    qd_hist_t *u = &a->undo;
    const uint8_t *now[4], *was[4];
    uint32_t sz[4], len[4], o = 0, z = 0, xb;
    int r;
    if (!u->open)
        return;
#ifdef QD_UNDO_SLOTS
    if (!u->b_ok || u->xl_over) {                       /* (the gesture's before not kept: no level) */
        u->open = 0;
        u->top = 0;
        return;
    }
#endif
    undo_regions(a, now, was, sz);
    for (r = 0; r < 4; r++) {                           /* the level's size: counted first (no copy of it anywhere) */
#ifdef QD_UNDO_SLOTS
        if (r == 1)
            len[r] = undo_slots(a, 0);                  /* (P2.1: the slots' entries) */
        else
#endif
            len[r] = sz[r] ? (uint32_t)undo_diff(0, 0, now[r], was[r], sz[r]) : 0u;
        o += 4u + len[r];
        z += len[r];
    }
    if (u->xl_over) {                                   /* (P1.5w: the other patterns' locks: too many) */
        u->open = 0;
        return;
    }
    xb = u->xl_n * 5u;
    o += 4u + xb;
    z += xb;
    if (a->proj.nsound != u->nsound0)
        z++;
    u->open = 0;
    if (!z)
        return;                                         /* nothing changed: no level */
    if (u->pos < u->n) {                                /* a new edit after undoing: the redo levels go */
        u->used = u->pos ? u->lvl[u->pos - 1].off + u->lvl[u->pos - 1].len : 0u;
        u->n = u->pos;
    }
    while (u->n >= QD_UNDO_LEVELS || (u->n && u->used + o > QD_UNDO_POOL))
        undo_drop_oldest(u);
    if (u->used + o > QD_UNDO_POOL)
        return;                                         /* (bigger than the pool: not undoable, forgotten) */
    {
        uint8_t *b = u->pool + u->used;
        uint32_t w = 0;
        for (r = 0; r < 4; r++) {
            memcpy(b + w, &len[r], 4);
            w += 4u;
#ifdef QD_UNDO_SLOTS
            if (r == 1)
                undo_slots(a, b + w);
            else
#endif
            if (sz[r])
                undo_diff(b + w, len[r], now[r], was[r], sz[r]);
            w += len[r];
        }
        memcpy(b + w, &xb, 4);
        memcpy(b + w + 4u, u->xl, xb);
        w += 4u + xb;
        u->lvl[u->n].off = u->used;
        u->lvl[u->n].len = w;
    }
    u->lvl[u->n].pat = (uint8_t)u->pat;
    u->lvl[u->n].kind = (uint8_t)(u->hint >= 0 ? u->hint : u->knob ? QD_UK_KNOB : QD_UK_EDIT);
    u->used += u->lvl[u->n].len;
    u->n++;
    u->pos = u->n;
}

void qd_undo_ms(qd_app_t *a)
{
    qd_hist_t *u = &a->undo;
    undo_follow(a);
    if (u->open && a->ms - u->last_ms >= QD_UNDO_QUIET_MS && !a->keys_held && !a->buttons_held)
        qd_undo_close(a);
}

static void undo_apply(qd_app_t *a, const qd_undo_lvl_t *l)
{
    const uint8_t *d = a->undo.pool + l->off;
    uint8_t *mem[4];
    uint32_t sz[4], o = 0;
    int r, t;
    mem[0] = (uint8_t *)qd_proj_pat(&a->proj, l->pat), sz[0] = sizeof(qd_pattern_t);
#ifdef QD_UNDO_SLOTS
    mem[1] = mem[2] = 0, sz[1] = sz[2] = 0;             /* (P2.1: the slots' entries, below; the library's: none) */
#else
    mem[1] = (uint8_t *)a->proj.sound, sz[1] = sizeof a->proj.sound;
    mem[2] = (uint8_t *)a->per.lib, sz[2] = sizeof a->per.lib;
#endif
    mem[3] = (uint8_t *)a->proj.mfx, sz[3] = sizeof a->proj.mfx;   /* (P1.6c) */
    for (r = 0; r < 4; r++) {
        uint32_t len;
        memcpy(&len, d + o, 4);
#ifdef QD_UNDO_SLOTS
        if (r == 1) {                                   /* (P2.1: the slots' entries: XORed back) */
            uint32_t i;
            for (i = 0; i + 2u + sizeof(qd_sound_t) <= len; i += 2u + (uint32_t)sizeof(qd_sound_t)) {
                const uint8_t *e = d + o + 4u + i;
                qd_sound_t s;
                uint32_t k;
                if (!undo_slot(a, e[0], e[1], &s))
                    continue;
                for (k = 0; k < sizeof s; k++)
                    ((uint8_t *)&s)[k] ^= e[2 + k];
                undo_slot_put(a, e[0], e[1], &s);
            }
            o += 4u + len;
            continue;
        }
#endif
#ifndef QD_UNDO_SLOTS
        if (r == 2)                                     /* (P2.1: MY presets it changes: the references to them
                                                         * copies first) */
            undo_lib_unref(a, d + o + 4u, len);
#endif
        undo_apply_runs(mem[r], sz[r], d + o + 4u, len);
        o += 4u + len;
    }
    {   /* P1.5w: the other patterns' locks, XORed back (or cleared again) */
        uint32_t len, i;
        memcpy(&len, d + o, 4);
        for (i = 0; i + 5u <= len; i += 5u) {
            const uint8_t *e = d + o + 4u + i;
            if (e[0] < QD_NPAT && e[1] < QD_PAT_NTRACK && e[2] < QD_NSTEP) {
                qd_step_t *st = &qd_proj_pat(&a->proj, e[0])->step[e[1]][e[2]];
                st->sound ^= e[3];
                if (e[4])
                    st->ovr ^= (uint16_t)QD_OV(QD_PM_SOUND);
            }
        }
    }
    qd_pool_resolve(a);                                 /* (P1.5w: the pool's references and the library together) */
    qd_sound_recount(&a->proj);
    if (l->pat == a->seq.cur)                           /* the tracks' sounds back from the pattern */
        for (t = 0; t < QD_NTRACK; t++)
            qd_track_from_tsound(&a->model.t[t], &qd_proj_pat(&a->proj, l->pat)->snd[t]);
}

int qd_undo_step(qd_app_t *a, int dir)
{
    qd_hist_t *u = &a->undo;
    int k = -1;
    qd_undo_close(a);
    if (dir < 0 && u->pos > 0) {
        u->pos--;
        undo_apply(a, &u->lvl[u->pos]);
        k = u->lvl[u->pos].kind;
    } else if (dir > 0 && u->pos < u->n) {
        undo_apply(a, &u->lvl[u->pos]);
        k = u->lvl[u->pos].kind;
        u->pos++;
    }
    u->open = 0;                                        /* (the undo itself is no edit) */
    return k;
}

int qd_undo_count(const qd_app_t *a, int dir) { return dir < 0 ? a->undo.pos : a->undo.n - a->undo.pos; }

/* ---------------------------------------------------------------- the snapshot, the pattern clipboard */
int qd_hist_store(qd_app_t *a, int which)
{
    qd_hist_t *h = &a->undo;
    const qd_pattern_t *P = qd_proj_cpat(&a->proj, a->seq.cur);
#ifdef QD_UNDO_SLOTS
    {   /* P2.1: compressed; the snapshot's bytes first, the clipboard's after them */
        const uint32_t sz = zr_pack((const uint8_t *)P, sizeof *P, 0, 0);
        if (sz + (which ? h->snap_len : h->pclip_len) > QD_SNAP_BYTES)
            return 0;
        if (!which) {
            memmove(h->snapb + sz, h->snapb + h->snap_len, h->pclip_len);
            zr_pack((const uint8_t *)P, sizeof *P, h->snapb, sz);
            h->snap_len = sz;
        } else {
            zr_pack((const uint8_t *)P, sizeof *P, h->snapb + h->snap_len, sz);
            h->pclip_len = sz;
        }
    }
#else
    if (!which)
        h->snap = *P;
    else
        h->pclip = *P;
#endif
    if (!which)
        h->snap_ok = 1;
    else
        h->pclip_ok = 1;
    return 1;
}

int qd_hist_recall(qd_app_t *a, int which)
{
    qd_hist_t *h = &a->undo;
    qd_pattern_t *P;
    if (!(which ? h->pclip_ok : h->snap_ok))
        return 0;
    P = qd_proj_pat(&a->proj, a->seq.cur);
#ifdef QD_UNDO_SLOTS
    if (!which)
        zr_unpack(h->snapb, h->snap_len, (uint8_t *)P, sizeof *P);
    else
        zr_unpack(h->snapb + h->snap_len, h->pclip_len, (uint8_t *)P, sizeof *P);
#else
    *P = which ? h->pclip : h->snap;
#endif
    P->used = 1;
    return 1;
}
