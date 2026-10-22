/* SPDX-License-Identifier: GPL-3.0-only */
/* P1.5c: undo / redo, the snapshot, the pattern clipboard (undo.h). Built through app.c (it includes this file: the
 * core's source lists stay as they are). */

typedef char qd_undo_lib_ok[QD_NLIB_UNDO == QD_NLIB ? 1 : -1];

void qd_undo_init(qd_app_t *a)
{
    qd_hist_t *u = &a->undo;
    u->open = 0;
    u->n = u->pos = 0;
    u->used = 0;
    u->snap_ok = u->pclip_ok = 0;
}

void qd_undo_reset(qd_app_t *a)
{
    a->undo.open = 0;
    a->undo.n = a->undo.pos = 0;
    a->undo.used = 0;
}

static void undo_take(qd_app_t *a)                    /* the gesture's "before" */
{
    qd_hist_t *u = &a->undo;
    u->pat = a->seq.cur;
    u->pat0 = a->proj.pat[u->pat];
    memcpy(u->snd0, a->proj.sound, sizeof u->snd0);
    u->nsound0 = a->proj.nsound;
    memcpy(u->lib0, a->per.lib, sizeof u->lib0);
    u->hint = -1;
    u->knob = 0;
    u->open = 1;
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

/* the XOR difference of a region (now ^ before) appended to the pool: runs of (offset u32, length u16, bytes); a run
 * ends at 8 equal bytes in a row. Returns the bytes written (0: no change), -1: no room */
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
            if (o + 6u + len > room)
                return -1;
            memcpy(out + o, &s, 4);
            out[o + 4] = (uint8_t)len, out[o + 5] = (uint8_t)(len >> 8);
            for (k = 0; k < len; k++)
                out[o + 6u + k] = (uint8_t)(now[s + k] ^ before[s + k]);
            o += 6u + len;
            s += len;
        }
    }
    return (long)o;
}

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

/* a level: three regions' differences, each [u32 bytes] then its runs */
void qd_undo_close(qd_app_t *a)
{
    qd_hist_t *u = &a->undo;
    for (;;) {
        static uint8_t tmp[QD_UNDO_POOL];
        uint32_t o = 0, z = 0;
        long d;
        int r;
        const uint8_t *now[3], *was[3];
        uint32_t sz[3];
        if (!u->open)
            return;
        now[0] = (const uint8_t *)&a->proj.pat[u->pat], was[0] = (const uint8_t *)&u->pat0, sz[0] = sizeof u->pat0;
        now[1] = (const uint8_t *)a->proj.sound, was[1] = (const uint8_t *)u->snd0, sz[1] = sizeof u->snd0;
        now[2] = (const uint8_t *)a->per.lib, was[2] = (const uint8_t *)u->lib0, sz[2] = sizeof u->lib0;
        for (r = 0; r < 3; r++) {
            if (o + 4u > sizeof tmp)
                break;
            d = undo_diff(tmp + o + 4u, (uint32_t)(sizeof tmp - o - 4u), now[r], was[r], sz[r]);
            if (d < 0) {                                /* (bigger than the pool: not undoable, forgotten) */
                u->open = 0;
                return;
            }
            memcpy(tmp + o, &d, 4);
            o += 4u + (uint32_t)d;
            z += (uint32_t)d;
        }
        if (a->proj.nsound != u->nsound0)
            z++;
        u->open = 0;
        if (!z)
            return;                                     /* nothing changed: no level */
        if (u->pos < u->n) {                            /* a new edit after undoing: the redo levels go */
            u->used = u->pos ? u->lvl[u->pos - 1].off + u->lvl[u->pos - 1].len : 0u;
            u->n = u->pos;
        }
        while (u->n >= QD_UNDO_LEVELS || (u->n && u->used + o > QD_UNDO_POOL))
            undo_drop_oldest(u);
        if (u->used + o > QD_UNDO_POOL)
            return;
        memcpy(u->pool + u->used, tmp, o);
        u->lvl[u->n].off = u->used;
        u->lvl[u->n].len = o;
        u->lvl[u->n].pat = (uint8_t)u->pat;
        u->lvl[u->n].kind = (uint8_t)(u->hint >= 0 ? u->hint : u->knob ? QD_UK_KNOB : QD_UK_EDIT);
        u->used += o;
        u->n++;
        u->pos = u->n;
        return;
    }
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
    uint8_t *mem[3];
    uint32_t sz[3], o = 0;
    int r, t;
    mem[0] = (uint8_t *)&a->proj.pat[l->pat], sz[0] = sizeof(qd_pattern_t);
    mem[1] = (uint8_t *)a->proj.sound, sz[1] = sizeof a->proj.sound;
    mem[2] = (uint8_t *)a->per.lib, sz[2] = sizeof a->per.lib;
    for (r = 0; r < 3; r++) {
        uint32_t len;
        memcpy(&len, d + o, 4);
        undo_apply_runs(mem[r], sz[r], d + o + 4u, len);
        o += 4u + len;
    }
    qd_sound_recount(&a->proj);
    if (l->pat == a->seq.cur)                           /* the tracks' sounds back from the pattern */
        for (t = 0; t < QD_NTRACK; t++)
            qd_track_from_tsound(&a->model.t[t], &a->proj.pat[l->pat].snd[t]);
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
