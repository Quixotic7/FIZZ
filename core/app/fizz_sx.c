/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* P1.5r: FIZZ's SysEx (fizz_sx.h, docs/SYSEX.md). Built through app.c (it includes this file: the core's source lists
 * stay as they are). */

uint32_t qd_sx_pack7(const uint8_t *in, uint32_t n, uint8_t *out)
{
    uint32_t i = 0, o = 0;
    while (i < n) {
        const uint32_t k = n - i < 7u ? n - i : 7u;
        uint32_t j;
        uint8_t hi = 0;
        for (j = 0; j < k; j++)
            hi |= (uint8_t)(((in[i + j] >> 7) & 1u) << j);
        out[o++] = hi;
        for (j = 0; j < k; j++)
            out[o++] = in[i + j] & 0x7Fu;
        i += k;
    }
    return o;
}

uint32_t qd_sx_unpack7(const uint8_t *in, uint32_t n, uint8_t *out)
{
    uint32_t i = 0, o = 0;
    while (i < n) {
        const uint8_t hi = in[i++];
        uint32_t j;
        for (j = 0; j < 7u && i < n; j++)
            out[o++] = (uint8_t)(in[i++] | ((hi >> j) & 1u) << 7);
    }
    return o;
}

/* a reply into the queue: F0 7D 46 5A 01, the bytes, the checksum, F7 */
static void sx_job(qd_app_t *a);                        /* (P1.6m) */

/* P1.6l: the replies wait in a ring of bytes, each its length (2 bytes) then F0 .. F7 */
static uint32_t sx_room(const qd_sx_t *s) { return QD_SX_RING - (s->w - s->r); }
static void sx_put(qd_sx_t *s, uint8_t v) { s->ring[s->w++ % QD_SX_RING] = v; }
static uint8_t sx_at(const qd_sx_t *s, uint32_t i) { return s->ring[(s->r + i) % QD_SX_RING]; }
static int sx_reply(qd_app_t *a, const uint8_t *b, uint32_t n)
{
    qd_sx_t *s = &a->sx;
    uint32_t i, sum = 0;
    const uint32_t len = n + 7u;
    if (len > QD_SX_FRAME_MAX || len + 2u > sx_room(s))
        return 0;                                       /* (no room: the older stay; a host asks again) */
    sx_put(s, (uint8_t)(len & 0xFFu)), sx_put(s, (uint8_t)(len >> 8));
    sx_put(s, 0xF0), sx_put(s, QD_SX_ID0), sx_put(s, QD_SX_ID1), sx_put(s, QD_SX_ID2), sx_put(s, QD_SX_PROTO);
    for (i = 0; i < n; i++) {
        sx_put(s, b[i] & 0x7Fu);
        sum += b[i] & 0x7Fu;
    }
    sx_put(s, (uint8_t)((0x80u - (sum & 0x7Fu)) & 0x7Fu));
    sx_put(s, 0xF7);
    return 1;
}

int qd_app_sysex_peek(const qd_app_t *a)
{
    const qd_sx_t *s = &a->sx;
    return s->r == s->w ? 0 : (int)(sx_at(s, 0) | (uint32_t)sx_at(s, 1) << 8);
}

int qd_app_sysex_busy(const qd_app_t *a) { return a->sx.job != 0; }

int qd_app_sysex_take(qd_app_t *a, uint8_t *out, uint32_t max)
{
    qd_sx_t *s = &a->sx;
    const uint32_t n = (uint32_t)qd_app_sysex_peek(a);
    uint32_t i;
    if (!n || n > max)
        return 0;
    for (i = 0; i < n; i++)
        out[i] = sx_at(s, 2u + i);
    s->r += 2u + n;
    return (int)n;
}

/* P1.6l: LABELS: the change count (two 7-bit bytes), how many (49), each label its length (0..10) then its
 * characters (qd_ctl_labels) */
static uint32_t sx_labels_hash(const qd_labels_t *l)
{
    uint32_t h = 2166136261u, i;
    const uint8_t *p = (const uint8_t *)l->s;
    for (i = 0; i < sizeof l->s; i++)
        h = (h ^ p[i]) * 16777619u;
    return h;
}
static void sx_labels_reply(qd_app_t *a, const qd_labels_t *l)
{
    uint8_t b[4 + QD_LBL_N * QD_LBL_LEN];
    uint32_t o = 0;
    int i;
    b[o++] = QD_SX_R_LABELS;
    b[o++] = (uint8_t)((a->sx.lbl_count >> 7) & 0x7Fu);
    b[o++] = (uint8_t)(a->sx.lbl_count & 0x7Fu);
    b[o++] = QD_LBL_N;
    for (i = 0; i < QD_LBL_N; i++) {
        const uint32_t n = (uint32_t)strlen(l->s[i]);
        b[o++] = (uint8_t)n;
        memcpy(b + o, l->s[i], n);
        o += n;
    }
    sx_reply(a, b, o);
}
static void sx_labels(qd_app_t *a, const uint8_t *b, uint32_t n)
{
    static qd_labels_t l;
    qd_ctl_labels(a, &l);
    a->sx.lbl_sub = n >= 2u && (b[0] & QD_SXL_SUB);
    if (sx_labels_hash(&l) != a->sx.lbl_hash) {
        a->sx.lbl_hash = sx_labels_hash(&l);
        a->sx.lbl_count = (a->sx.lbl_count + 1u) & 0x3FFFu;
    }
    a->sx.lbl_ms = a->ms;
    sx_labels_reply(a, &l);
}
void qd_sx_ms(qd_app_t *a)
{
    static qd_labels_t l;
    uint32_t h;
    if (a->sx.job)
        sx_job(a);                                      /* (P1.6m: a LIST / DUMP going out as there is room) */
    if (!a->sx.lbl_sub || a->ms - a->sx.lbl_ms < QD_SX_LABELS_MS)
        return;
    a->sx.lbl_ms = a->ms;
    qd_ctl_labels(a, &l);
    h = sx_labels_hash(&l);
    if (h == a->sx.lbl_hash)
        return;                                         /* (the same set: nothing sent) */
    if (sx_room(&a->sx) < 2u * QD_SX_FRAME_MAX)
        return;                                         /* (the host is behind: the next look sends it) */
    a->sx.lbl_hash = h;
    a->sx.lbl_count = (a->sx.lbl_count + 1u) & 0x3FFFu;
    sx_labels_reply(a, &l);
}

void qd_app_sysex_version(qd_app_t *a, const char *version) { a->sx.version = version; }

/* a name field (12): printable ASCII (others '?'), the trailing spaces off; 0: empty */
static int sx_name(const uint8_t *b, char *name)
{
    int i;
    for (i = 0; i < QD_SX_NAME; i++)
        name[i] = b[i] >= 0x20 && b[i] <= 0x7E ? (char)b[i] : b[i] ? '?' : ' ';
    name[QD_SX_NAME] = 0;
    for (i = QD_SX_NAME - 1; i >= 0 && name[i] == ' '; i--)
        name[i] = 0;
    return name[0] != 0;
}

static void sx_store_reply(qd_app_t *a, int status, int pool, int slot)
{
    const uint8_t b[5] = {QD_SX_R_STORE, (uint8_t)status, (uint8_t)pool, (uint8_t)((slot >> 7) & 0x7F), (uint8_t)(slot & 0x7F)};
    if (status)
        a->sx.errors++;
    sx_reply(a, b, 5);
}

typedef char qd_sx_pend_np_ok[sizeof(((qd_sx_pend_t *)0)->patch) == QD_QUAD_NP * sizeof(int16_t) ? 1 : -1];
static int sx_job_begin(qd_app_t *a, int cmd, int src, int x, int y, int flags, int stopped);

/* the sound read (q): stored, loaded, auditioned (the handler, or the STORE job's end) */
static void sx_store_apply(qd_app_t *a, const qd_sx_pend_t *q)
{
    const int flags = q->flags, pool = (flags & QD_SXF_POOL) != 0, track = q->track;
    const int mask = q->mix[4];
    int slot = q->slot, lib_slot = 0;
    if ((flags & QD_SXF_AUDITION) && (flags & QD_SXF_LOAD) && track < QD_NTRACK) {   /* (P2-B: the track only) */
        qd_track_t *t = &a->model.t[track];
        qd_track_load_sound(t, q->has_x ? q->x_sound : 0, q->name, q->patch);
        t->preset = q->has_x ? q->x_id : 0;
        t->edited = (uint8_t)(q->has_x && (q->x_flags & QD_SXX_EDITED));
        if (mask & QD_SXM_PAN)
            t->mix.pan = (int8_t)((int)q->mix[0] - 64);
        if (mask & QD_SXM_CHORUS)
            t->mix.chorus = q->mix[1];
        if (mask & QD_SXM_DELAY)
            t->mix.delay = q->mix[2];
        if (mask & QD_SXM_REVERB)
            t->mix.reverb = q->mix[3];
        qd_track_to_tsound(t, &qd_ctl_pat(a)->snd[track]);   /* (the pattern's copy now: a pattern change not yet
                                                         * brought into the tracks would put the old one back) */
        sx_store_reply(a, QD_SX_OK, 0, 0);
        return;
    }
    qd_undo_input(a, 0);                                /* (an input: the batch's gesture stays open while they come) */
    a->undo.join = (flags & QD_SXF_BATCH) != 0;         /* (a batch: the previous import's level goes on) */
    if (pool) {                                         /* P1.5w: a MY preset (imported), referenced at that number */
        const int m = qd_lib_free_slot(a);
        qd_sound_t s;
        qd_lib_store_ex(a, m, q->name, q->patch, QD_SNDF_IMPORTED);
        qd_undo_snd(a, QD_UNDO_RPOOL, slot);            /* (P2.1: as it was, for the device's undo) */
        memset(&s, 0, sizeof s);
        s.preset = qd_lib_preset(a, m);
        strcpy(s.name, "?");
        qd_sound_write(&a->proj, slot, &s);
        qd_pool_resolve(a);
        lib_slot = m;
    } else {                                            /* (P1.5t: one of MY presets, imported; P2-B OWN: the owner's) */
        qd_lib_store_ex(a, slot - 1, q->name, q->patch, flags & QD_SXF_OWN ? 0 : QD_SNDF_IMPORTED);
        lib_slot = slot - 1;
    }
    {   /* P1.5x: the preset keeps the mix the frame gives (mixmask), LOAD or not; the tags byte (nonzero) its tags */
        qd_sound_t ls;
        qd_lib_read(a, lib_slot, &ls);
        ls.pan = mask & QD_SXM_PAN ? (int8_t)((int)q->mix[0] - 64) : 0;
        ls.chorus = mask & QD_SXM_CHORUS ? q->mix[1] : 0;
        ls.delay = mask & QD_SXM_DELAY ? q->mix[2] : 0;
        ls.reverb = mask & QD_SXM_REVERB ? q->mix[3] : 0;
        if (q->tags & QD_PK_MASK)
            ls.tags = (uint8_t)(q->tags & QD_PK_MASK);
        qd_lib_write(a, lib_slot, &ls);
        qd_pool_resolve(a);
    }
    a->undo.join = 0;
    if ((flags & QD_SXF_LOAD) && track < QD_NTRACK) {   /* onto a track: its sound (a pool sound: its source), its mix */
        qd_track_t *t = &a->model.t[track];
        qd_track_load_sound(t, pool ? slot : 0, q->name, q->patch);
        t->preset = qd_lib_preset(a, lib_slot);
        if (mask & QD_SXM_PAN)
            t->mix.pan = (int8_t)((int)q->mix[0] - 64);
        if (mask & QD_SXM_CHORUS)
            t->mix.chorus = q->mix[1];
        if (mask & QD_SXM_DELAY)
            t->mix.delay = q->mix[2];
        if (mask & QD_SXM_REVERB)
            t->mix.reverb = q->mix[3];
        qd_track_to_tsound(t, &qd_ctl_pat(a)->snd[track]);
    }
    qd_ctl_imported(a, q->name, pool, slot);            /* the panel: its cassette, "imported" */
    sx_store_reply(a, QD_SX_OK, pool, slot);
}

/* STORE: body[0..n) after the command byte, its checksum included. P2-B: over a used MY PRESETS place a job (the
 * other projects' records that load the preset keep their own copies first, a project a step: never inside the
 * handler), stopped only (BUSY while playing, as the other jobs); its reply when done */
static void sx_store(qd_app_t *a, const uint8_t *b, uint32_t n, int six)
{
    uint8_t blob[QD_SX_BLOB6 + 8];
    qd_sx_pend_t q;
    int flags, pool, slot;
    const uint32_t body = six ? QD_SX_STORE6_BODY : QD_SX_STORE_BODY;   /* (E1: STORE6: the 128-byte blob) */
    const int xtra = n == body + 2u + QD_SX_XTRA && (b[0] & QD_SXF_AUDITION) && (b[0] & QD_SXF_LOAD);   /* (P2-B: the track's extras) */
    if (n != body + 1u && n != body + 2u && !xtra) {    /* (P1.5x: + the tags byte) */
        sx_store_reply(a, QD_SX_BAD_LENGTH, n ? (b[0] & QD_SXF_POOL) != 0 : 0, 0);
        return;
    }
    flags = b[0];
    if (flags & QD_SXF_DUMP)
        return;                                         /* (P1.6m: a DUMP's reply, echoed: not stored, no reply) */
    pool = (flags & QD_SXF_POOL) != 0;
    slot = b[1] << 7 | b[2];
    if ((flags & QD_SXF_LOAD) && b[16] >= QD_NTRACK) {  /* (Phase 1: a track FIZZ does not have: nothing stored) */
        sx_store_reply(a, QD_SX_BAD_TRACK, pool, slot);
        return;
    }
    memset(&q, 0, sizeof q);
    if (!sx_name(b + 3, q.name))                        /* the name: printable ASCII, the trailing spaces off */
        strcpy(q.name, "IMPORTED");
    if (qd_sx_unpack7(b + 22, six ? QD_SX_BLOB6_7 : QD_SX_BLOB7, blob) != (six ? QD_SX_BLOB6 : QD_SX_BLOB)) {
        sx_store_reply(a, QD_SX_BAD_LENGTH, pool, slot);
        return;
    }
    if (blob[1] == 5 || blob[1] == 6)                   /* (version 5 / 6: DRIVE after its values, as FIZZ packs it) */
        blob[87] = b[15] & 0x7F;
    if ((!six && blob[1] == 6) || !qd_quad_unpack(blob, six ? QD_SX_BLOB6 : QD_SX_BLOB, q.patch)) {
        sx_store_reply(a, QD_SX_BAD_PATCH, pool, slot);
        return;
    }
    q.patch[QD_QP_DRIVE] = (int16_t)(b[15] & 0x7F);
    q.flags = (uint8_t)flags, q.track = b[16];
    memcpy(q.mix, b + 17, 5);
    q.tags = n >= body + 2u ? b[body] : 0;
    if (xtra) {
        const uint8_t *x = b + body + 1u;
        q.has_x = 1;
        q.x_id = (uint16_t)((x[0] & 0x7F) << 14 | (x[1] & 0x7F) << 7 | (x[2] & 0x7F));
        q.x_sound = (uint8_t)((x[3] & 0x7F) << 7 | (x[4] & 0x7F));
        q.x_flags = x[5];
    }
    if ((flags & QD_SXF_AUDITION) && (flags & QD_SXF_LOAD)) {
        sx_store_apply(a, &q);                          /* (P2-B: the track only, nothing stored) */
        return;
    }
    if (pool) {                                         /* POOL: 1..128 (P1.5t: slot 1 as any), 0 the first free */
        if (slot == 0)
            slot = qd_pool_free_slot(&a->proj);
        else if (slot < 1 || slot > QD_NSOUND) {
            sx_store_reply(a, QD_SX_BAD_SLOT, pool, slot);
            return;
        }
        if (!slot) {
            sx_store_reply(a, QD_SX_FULL, pool, 0);
            return;
        }
        if (qd_lib_free_slot(a) < 0) {                  /* (P1.5w: a pool slot references a new MY preset) */
            sx_store_reply(a, QD_SX_FULL, pool, 0);
            return;
        }
    } else {                                            /* LIBRARY: MY presets (P1.5t), 1..256, 0 the first free */
        if (slot == 0)
            slot = qd_lib_free_slot(a) + 1;
        else if (slot > QD_NLIB) {
            sx_store_reply(a, QD_SX_BAD_SLOT, pool, slot);
            return;
        }
        if (!slot) {
            sx_store_reply(a, QD_SX_FULL, pool, 0);
            return;
        }
    }
    q.slot = (uint16_t)slot;
    if (!pool && qd_lib_used(a, slot - 1) && qd_lib_preset(a, slot - 1) >= QD_PRESET_MINE0) {
        if (!sx_job_begin(a, six ? QD_SX_STORE6 : QD_SX_STORE, QD_SXS_MINE, slot - 1, 0, 0, 1)) {
            sx_store_reply(a, QD_SX_BUSY, pool, slot);
            return;
        }
        a->sx.pend = q;                                 /* (P2-B: stored by the job, after the other projects' records) */
        return;
    }
    sx_store_apply(a, &q);
}

/* ---------------------------------------------------------------- P1.6m: LIST, DUMP */
/* source src's entry k (0-based): its sound (a pool slot: its preset, resolved; the pool's own name), 0 none */
static int sx_entry(const qd_app_t *a, int src, int k, qd_sound_t *s)
{
    if (src == QD_SXS_MINE) {
        if (k < 0 || k >= QD_NLIB || !qd_lib_used(a, k))
            return 0;
        qd_lib_read(a, k, s);
        return 1;
    }
    if (src == QD_SXS_FACTORY) {
        if (k < 0 || k >= QD_NFACTORY)
            return 0;
        qd_factory_get(a, k, s);
        return 1;
    }
    if (k < 0 || k >= QD_NSOUND || !qd_sound_used(&a->proj, k + 1))
        return 0;
    {
        int m;
        qd_sound_read(&a->proj, k + 1, s);
        m = qd_lib_find(a, s->preset);                  /* (a MY preset's: its mix, its tags, as the preset) */
        if (m >= 0) {
            qd_sound_t l;
            qd_lib_read(a, m, &l);
            s->pan = l.pan, s->chorus = l.chorus;
            s->delay = l.delay, s->reverb = l.reverb;
            s->tags = l.tags;
            s->flags = l.flags;
        }
    }
    return 1;
}
/* P2.1: an entry's name, id, tags and flags only (LIST's: no sound read; the rest zeros), 0 none */
static int sx_head(const qd_app_t *a, int src, int k, qd_sound_t *s)
{
    memset(s, 0, sizeof *s);
    if (src == QD_SXS_MINE) {
        if (k < 0 || k >= QD_NLIB || !qd_lib_used(a, k))
            return 0;
        memcpy(s->name, qd_lib_name(a, k), QD_SOUND_NAME);
        s->preset = qd_lib_preset(a, k), s->tags = (uint8_t)qd_lib_tags(a, k), s->flags = (uint8_t)qd_lib_flags(a, k);
        return 1;
    }
    if (src == QD_SXS_FACTORY)
        return sx_entry(a, src, k, s);
    if (k < 0 || k >= QD_NSOUND || !qd_sound_used(&a->proj, k + 1))
        return 0;
    {
        const int m = qd_lib_find(a, qd_sound_preset(&a->proj, k + 1));
        memcpy(s->name, qd_sound_name(&a->proj, k + 1), QD_SOUND_NAME);
        s->preset = qd_sound_preset(&a->proj, k + 1);
        s->tags = (uint8_t)(m >= 0 ? qd_lib_tags(a, m) : qd_sound_tags(&a->proj, k + 1));
        s->flags = (uint8_t)(m >= 0 ? qd_lib_flags(a, m) : qd_sound_flags(&a->proj, k + 1));
    }
    return 1;
}
static int sx_count(const qd_app_t *a, int src)
{
    qd_sound_t s;
    int k, n = 0, max = src == QD_SXS_MINE ? QD_NLIB : src == QD_SXS_FACTORY ? QD_NFACTORY : QD_NSOUND;
    for (k = 0; k < max; k++)
        n += sx_head(a, src, k, &s);
    return n;
}

/* DUMP's reply: a STORE (STORE6 when the sound uses a value 74..) frame of it, the DUMP flag set: its slot, name,
 * DRIVE, pan, the sends (mixmask 0F), its patch, its tags; FIZZ ignores it as it is (sent back with the flag cleared:
 * the preset made again). P2-B: a track's (tr 0..5; -1 none) with LOAD and its track, then the track's extras
 * (QD_SX_XTRA: its preset id, pool sound, flags, MIDI channel, the selected track). (P2-B: built in qd_sx_t, the pool,
 * not in RAM) */
static int sx_dump_sound(qd_app_t *a, const qd_sound_t *s, int flags, int slot, int tr)
{
    uint8_t *const b = a->sx.dump;
    uint8_t blob[QD_SX_BLOB6];
    uint32_t o = 0;
    int i;
    const int six = qd_quad_ext_used(s->patch);
    if (six)
        qd_quad_pack(s->patch, blob);
    else
        qd_quad_pack5(s->patch, blob);
    b[o++] = six ? QD_SX_STORE6 : QD_SX_STORE;
    b[o++] = (uint8_t)(QD_SXF_DUMP | flags | (tr >= 0 ? QD_SXF_LOAD : 0));
    b[o++] = (uint8_t)((slot >> 7) & 0x7F), b[o++] = (uint8_t)(slot & 0x7F);
    for (i = 0; i < QD_SX_NAME; i++)
        b[o++] = (uint8_t)(s->name[i] && i < (int)strlen(s->name) ? s->name[i] & 0x7F : ' ');
    b[o++] = (uint8_t)(s->patch[QD_QP_DRIVE] & 0x7F);
    b[o++] = (uint8_t)(tr >= 0 ? tr : 0);               /* (the track: a track's) */
    b[o++] = (uint8_t)(s->pan + 64 < 0 ? 0 : s->pan + 64 > 127 ? 127 : s->pan + 64);
    b[o++] = s->chorus & 0x7F, b[o++] = s->delay & 0x7F, b[o++] = s->reverb & 0x7F;
    b[o++] = QD_SXM_PAN | QD_SXM_CHORUS | QD_SXM_DELAY | QD_SXM_REVERB;
    o += qd_sx_pack7(blob, six ? QD_SX_BLOB6 : QD_SX_BLOB, b + o);
    b[o++] = (uint8_t)(s->tags & QD_PK_MASK);
    if (tr >= 0) {                                      /* (P2-B: the track's extras) */
        const qd_track_t *t = &a->model.t[tr];
        b[o++] = (uint8_t)((t->preset >> 14) & 0x7F), b[o++] = (uint8_t)((t->preset >> 7) & 0x7F);
        b[o++] = (uint8_t)(t->preset & 0x7F);
        b[o++] = (uint8_t)((t->sound >> 7) & 0x7F), b[o++] = (uint8_t)(t->sound & 0x7F);
        b[o++] = (uint8_t)((t->edited ? QD_SXX_EDITED : 0) | (tr == a->sel ? QD_SXX_SELECTED : 0) |
                           (a->ui.rec ? QD_SXX_REC : 0) | (a->seq.playing ? QD_SXX_PLAYING : 0));
        b[o++] = (uint8_t)(qd_seq_track_pat(&a->seq, tr)->trk[tr].chan & 0x0F);
        b[o++] = (uint8_t)a->sel;
    }
    return sx_reply(a, b, o) ? 1 : 0;
}
static int sx_dump_one(qd_app_t *a, int src, int k)
{
    qd_sound_t s;
    if (!sx_entry(a, src, k, &s))
        return -1;
    return sx_dump_sound(a, &s, src == QD_SXS_POOL ? QD_SXF_POOL : 0, src == QD_SXS_FACTORY ? 0 : k + 1, -1);
}
/* P2-B: track t's sound (slot 1..6; 0 the selected one) as the editor sees it: its name, patch, mix, its preset's tags */
static int sx_dump_track(qd_app_t *a, int slot)
{
    qd_sound_t s;
    const int t = slot ? slot - 1 : a->sel, m = t < QD_NTRACK ? qd_lib_find(a, a->model.t[t].preset) : -1;
    const qd_track_t *tk;
    if (t >= QD_NTRACK)
        return -1;
    tk = &a->model.t[t];
    memset(&s, 0, sizeof s);
    memcpy(s.name, tk->sound_name, sizeof s.name < sizeof tk->sound_name ? sizeof s.name : sizeof tk->sound_name);
    s.name[sizeof s.name - 1] = 0;
    memcpy(s.patch, tk->patch, sizeof s.patch);
    s.pan = tk->mix.pan, s.chorus = tk->mix.chorus, s.delay = tk->mix.delay, s.reverb = tk->mix.reverb;
    s.tags = (uint8_t)(m >= 0 ? qd_lib_tags(a, m) : tk->preset >= 1 && tk->preset <= QD_NFACTORY
                                                     ? qd_factory_tags(a, tk->preset - 1) : qd_preset_tags(tk->patch));
    return sx_dump_sound(a, &s, 0, 0, t);
}

/* LIST's reply: part, last (1: the listing's end), the total (2 x 7 bits), how many here, the entries: source, slot
 * (2), preset id (3 x 7 bits), name (12, spaces), tags, flags (bit 0 imported, bit 1 in this project's pool) */
static int sx_list_part(qd_app_t *a)
{
    static uint8_t b[8 + QD_SX_LIST_PER * QD_SX_ENTRY];
    qd_sx_t *x = &a->sx;
    const int total = sx_count(a, QD_SXS_MINE) + (x->job_flags & QD_SXL_FACTORY ? QD_NFACTORY : 0) +
                      (x->job_flags & QD_SXL_POOL ? sx_count(a, QD_SXS_POOL) : 0);
    uint32_t o = 6;
    int n = 0;
    while (n < QD_SX_LIST_PER && x->job_src <= QD_SXS_POOL) {
        const int src = x->job_src, max = src == QD_SXS_MINE ? QD_NLIB : src == QD_SXS_FACTORY ? QD_NFACTORY : QD_NSOUND;
        qd_sound_t s;
        if ((src == QD_SXS_FACTORY && !(x->job_flags & QD_SXL_FACTORY)) ||
            (src == QD_SXS_POOL && !(x->job_flags & QD_SXL_POOL)) || x->job_at >= max) {
            x->job_src++, x->job_at = 0;
            continue;
        }
        if (sx_head(a, src, x->job_at, &s)) {
            int i, inpool = 0;
            const int slot = x->job_at + 1;
            for (i = 1; i <= QD_NSOUND && !inpool; i++)
                inpool = qd_sound_used(&a->proj, i) && qd_sound_preset(&a->proj, i) == s.preset && s.preset;
            b[o++] = (uint8_t)src;
            b[o++] = (uint8_t)((slot >> 7) & 0x7F), b[o++] = (uint8_t)(slot & 0x7F);
            b[o++] = (uint8_t)((s.preset >> 14) & 0x7F), b[o++] = (uint8_t)((s.preset >> 7) & 0x7F);
            b[o++] = (uint8_t)(s.preset & 0x7F);
            for (i = 0; i < QD_SX_NAME; i++)
                b[o++] = (uint8_t)(i < (int)strlen(s.name) ? s.name[i] & 0x7F : ' ');
            b[o++] = (uint8_t)(s.tags & QD_PK_MASK);
            b[o++] = (uint8_t)((s.flags & QD_SNDF_IMPORTED ? 1 : 0) | (inpool ? 2 : 0));
            n++;
        }
        x->job_at++;
    }
    b[0] = QD_SX_R_LIST;
    b[1] = (uint8_t)(x->job_part & 0x7F);
    b[2] = (uint8_t)(x->job_src > QD_SXS_POOL);
    b[3] = (uint8_t)((total >> 7) & 0x7F), b[4] = (uint8_t)(total & 0x7F);
    b[5] = (uint8_t)n;
    if (!sx_reply(a, b, o))
        return 0;
    x->job_part++;
    return 1;
}

/* the job going out: as many replies as there is room for now (the rest: qd_sx_ms, as the host reads) */
static void sx_job(qd_app_t *a)
{
    qd_sx_t *x = &a->sx;
    while (x->job && sx_room(x) >= QD_SX_FRAME_MAX + 2u) {
        if (x->job == QD_SX_LIST) {
            if (!sx_list_part(a))
                return;
            if (x->job_src > QD_SXS_POOL)
                x->job = 0;
        } else {                                        /* DUMP, a whole source */
            const int max = x->job_src == QD_SXS_MINE ? QD_NLIB : x->job_src == QD_SXS_FACTORY ? QD_NFACTORY : QD_NSOUND;
            if (x->job_at >= max) {
                x->job = 0;
                return;
            }
            if (sx_dump_one(a, x->job_src, x->job_at) == 0)
                return;                                 /* (no room after all: again later; -1: an empty place) */
            x->job_at++;
        }
    }
}

static void sx_list(qd_app_t *a, const uint8_t *b, uint32_t n)
{
    qd_sx_t *x = &a->sx;
    x->job = QD_SX_LIST;
    x->job_flags = n >= 2u ? b[0] : 0;
    x->job_src = QD_SXS_MINE, x->job_at = 0, x->job_part = 0;
    sx_job(a);
}

/* DUMP: source, slot (2 x 7 bits; 1-based; 0: every one of that source, paced) */
static void sx_dump(qd_app_t *a, const uint8_t *b, uint32_t n)
{
    int src, slot;
    if (n < 4u) {
        const uint8_t e[3] = {QD_SX_R_ERROR, QD_SX_BAD_LENGTH, QD_SX_DUMP};
        sx_reply(a, e, 3);
        return;
    }
    src = b[0], slot = b[1] << 7 | b[2];
    if (src == QD_SXS_TRACK ? sx_dump_track(a, slot) < 0 : src > QD_SXS_POOL || (slot && sx_dump_one(a, src, slot - 1) < 0)) {
        const uint8_t e[3] = {QD_SX_R_ERROR, QD_SX_BAD_SLOT, QD_SX_DUMP};
        a->sx.errors++;
        sx_reply(a, e, 3);
        return;
    }
    if (!slot && src != QD_SXS_TRACK) {
        a->sx.job = QD_SX_DUMP;
        a->sx.job_src = src, a->sx.job_at = 0;
        sx_job(a);
    }
}

/* ---------------------------------------------------------------- the web manager, Phase 1 */
/* (docs/WEB-MANAGER-PLAN.md §5, docs/SYSEX.md) the quick ones in the handler (the lock: a directory entry, a record
 * queued); those that touch patterns or other projects' records a job (qd_sx_work_t), stepped by the store's step,
 * their reply when done. A slot on the wire: 1-based, two 7-bit bytes. ACK: 52 <cmd> <status> <a hi lo> <b hi lo> */
static void sx_ack(qd_app_t *a, int cmd, int status, int v1, int v2)
{
    const uint8_t b[7] = {QD_SX_R_ACK, (uint8_t)(cmd & 0x7F), (uint8_t)status, (uint8_t)((v1 >> 7) & 0x7F),
                          (uint8_t)(v1 & 0x7F), (uint8_t)((v2 >> 7) & 0x7F), (uint8_t)(v2 & 0x7F)};
    if (status)
        a->sx.errors++;
    sx_reply(a, b, 7);
}
static int sx_u14(const uint8_t *b) { return (b[0] & 0x7F) << 7 | (b[1] & 0x7F); }
static uint32_t sx_put14(uint8_t *b, uint32_t o, uint32_t v)
{
    b[o] = (uint8_t)((v >> 7) & 0x7F), b[o + 1] = (uint8_t)(v & 0x7F);
    return o + 2u;
}

/* STATUS (42): proto, tracks, flags (QD_SXT_*), the project worked on (0..7), its pattern, projects there; MY PRESETS
 * used, the pool's sounds, records committed (3 bytes), the library's store pages used / all, the projects' (2 bytes
 * each); the job's command (0 none) */
static void sx_status(qd_app_t *a)
{
    const qd_persist_t *ps = &a->per;
    uint8_t b[25];
    uint32_t u[6] = {0, 0, 0, 0, 0, 0}, o = 0;
    int k, np = 0;
    for (k = 0; k < QD_NLIB; k++)
        u[0] += (uint32_t)qd_lib_used(a, k);
    for (k = 1; k <= QD_NSOUND; k++)
        u[1] += (uint32_t)qd_sound_used(&a->proj, k);
    for (k = 0; k < QD_NPROJ; k++)
        np += ps->pexists[k] || k == ps->proj;
    if (ps->on) {
        qd_store_use(&ps->lib_st, &u[2], &u[3]);
        qd_store_use(&ps->prj_st, &u[4], &u[5]);
    }
    b[o++] = QD_SX_R_STATUS;
    b[o++] = QD_SX_PROTO;
    b[o++] = QD_NTRACK;
    b[o++] = (uint8_t)((a->seq.playing ? QD_SXT_PLAYING : 0) | (ps->on ? 0 : QD_SXT_NOSTORE) |
                       (qd_persist_dirty(a) ? QD_SXT_DIRTY : 0) | (a->sx.wk.cmd ? QD_SXT_JOB : 0) |
                       (ps->pdemo ? QD_SXT_DEMO : 0) | (a->ui.rec ? QD_SXT_REC : 0));
    b[o++] = (uint8_t)ps->proj;
    b[o++] = (uint8_t)(a->seq.cur & 0x7F);
    b[o++] = (uint8_t)np;
    o = sx_put14(b, o, u[0]);
    o = sx_put14(b, o, u[1]);
    b[o++] = (uint8_t)((ps->written >> 14) & 0x7F);
    o = sx_put14(b, o, ps->written);
    for (k = 2; k < 6; k++)
        o = sx_put14(b, o, u[k]);
    b[o++] = a->sx.wk.cmd;
    b[o++] = (uint8_t)a->sel;                           /* (P2-B: the selected track) */
    sx_reply(a, b, o);
}

/* PROJECTS (63): the one worked on, 8, then each: flags (1 there, 2 worked on, 4 the demos), name (12), store pages (2) */
static void sx_projects(qd_app_t *a)
{
    const qd_persist_t *ps = &a->per;
    uint8_t b[3 + QD_NPROJ * (3 + QD_SX_NAME)];
    uint32_t pages[QD_NPROJ], o = 0;
    int p, i;
    memset(pages, 0, sizeof pages);
    if (ps->on)                                         /* (one pass over the index, not eight) */
        for (i = 0; i < ps->prj_st.nkey; i++) {
            const qds_ent_t *e = &ps->prj_st.ent[i];
            const uint32_t q = (e->key >> 16) & 0xFFu;
            if (q < QD_NPROJ && (e->key >> 24) != QD_RK_SET)
                pages[q] += (uint32_t)qd_store_ent_pages(e);
        }
    b[o++] = QD_SX_R_PROJECTS;
    b[o++] = (uint8_t)ps->proj;
    b[o++] = QD_NPROJ;
    for (p = 0; p < QD_NPROJ; p++) {
        const uint32_t n = (uint32_t)strlen(ps->pname[p]);
        b[o++] = (uint8_t)((ps->pexists[p] || p == ps->proj ? 1 : 0) | (p == ps->proj ? 2 | (ps->pdemo ? 4 : 0) : 0));
        for (i = 0; i < QD_SX_NAME; i++)
            b[o++] = (uint8_t)((uint32_t)i < n ? ps->pname[p][i] & 0x7F : ' ');
        o = sx_put14(b, o, pages[p]);
    }
    sx_reply(a, b, o);
}

/* a source's slot (1-based): its browser code (qd_tags_set's), -1 none there */
static int sx_code(const qd_app_t *a, int src, int slot)
{
    if (src == QD_SXS_MINE)
        return slot >= 1 && slot <= QD_NLIB && qd_lib_used(a, slot - 1) ? QD_BR_MINE_CODE + slot - 1 : -1;
    if (src == QD_SXS_FACTORY)
        return slot >= 1 && slot <= QD_NFACTORY ? slot - 1 : -1;
    if (src == QD_SXS_POOL)
        return slot >= 1 && slot <= QD_NSOUND && qd_sound_used(&a->proj, slot) ? QD_BR_POOL_CODE + slot : -1;
    return -1;
}

/* a job begun: 1 (0: one is running, or it needs the transport stopped and FIZZ plays: BUSY) */
static int sx_job_begin(qd_app_t *a, int cmd, int src, int x, int y, int flags, int stopped)
{
    qd_sx_work_t *w = &a->sx.wk;
    if (w->cmd || (stopped && a->seq.playing))
        return 0;
    memset(w, 0, sizeof *w);
    w->cmd = (uint8_t)cmd, w->src = (uint8_t)src, w->x = (uint8_t)x, w->y = (uint8_t)y, w->flags = (uint8_t)flags;
    w->id = src == QD_SXS_MINE && cmd != QD_SX_PROJECT ? qd_lib_preset(a, x) : 0;
    return 1;
}

/* the body's least length (its checksum included) */
static uint32_t sx_need(int cmd)
{
    return cmd == QD_SX_RENAME ? 4u + QD_SX_NAME : cmd == QD_SX_SWAP || cmd == QD_SX_POOLREF ? 6u
         : cmd == QD_SX_TAGS || cmd == QD_SX_DELETE ? 5u : cmd == QD_SX_FLUSH ? 1u : 4u;
}

/* TAGS, RENAME, DELETE, SWAP, POOL REF, USERS, PROJECT, FLUSH (body b[0..n), its checksum last): the status to answer
 * (an ACK of v[0], v[1]); -1 a job begun (its reply when it is done).
 *   TAGS      source, slot, tags (FIZZ's seven; a factory preset's are the owner's, as on the panel)
 *   RENAME    source (MY PRESETS, a project), slot, name[12]: a MY preset keeps its id (its references follow:
 *             the pool slots referencing it show the new name); another project's header: a job (a record written)
 *   DELETE    source (MY PRESETS, the pool), slot, flags (CONFIRM): a job, stopped only (below)
 *   SWAP      source, a, b: MY PRESETS' places (the ids go with them: no reference changes) now; the pool's numbers
 *             in every pattern too: a job, stopped only, the undo levels forgotten. Into an empty place: a move
 *   POOL REF  pool slot (0 the first free), preset id (3 x 7 bits; 0: cleared, DELETE's job with CONFIRM): the slot
 *             references the preset (no copy); the steps locking that number play it
 *   USERS     source, slot: a job (other projects' records read), while playing too
 *   PROJECT   op (LOAD, NEW, COPY p onto q, DELETE), p, q (1..8): a job, stopped only */
static int sx_edit(qd_app_t *a, int cmd, const uint8_t *b, uint32_t n, int *v)
{
    qd_persist_t *ps = &a->per;
    const int src = b[0];
    int slot = n >= 3u ? sx_u14(b + 1) : 0, code;
    char name[QD_SX_NAME + 1];
    qd_sound_t s;
    v[0] = slot, v[1] = 0;
    if (n < sx_need(cmd))
        return QD_SX_BAD_LENGTH;
    code = sx_code(a, src, slot);
    switch (cmd) {
    case QD_SX_FLUSH:
        return sx_job_begin(a, cmd, 0, 0, 0, 0, 1) ? -1 : QD_SX_BUSY;
    case QD_SX_TAGS:
        if (code < 0)
            return src > QD_SXS_POOL ? QD_SX_BAD_SLOT : QD_SX_NOT_FOUND;
        qd_undo_input(a, 0);
        v[1] = b[3] & QD_PK_MASK;
        return qd_tags_set(a, code, v[1]) ? QD_SX_OK : QD_SX_NOT_FOUND;
    case QD_SX_RENAME:
        if (!sx_name(b + 3, name))
            strcpy(name, src == QD_SXS_PROJECT ? "PROJECT" : "SOUND");
        if (src == QD_SXS_PROJECT) {
            const int p = slot - 1;
            if (p < 0 || p >= QD_NPROJ)
                return QD_SX_BAD_SLOT;
            if (p != ps->proj && !ps->pexists[p])
                return QD_SX_NOT_FOUND;
            if (p != ps->proj && ps->on) {              /* (another project's header: the job writes it) */
                if (!sx_job_begin(a, cmd, src, p, 0, 0, 0))
                    return QD_SX_BUSY;
                memset(ps->pname[p], 0, QD_PROJ_NAME);
                strncpy(ps->pname[p], name, QD_PROJ_NAME - 1);
                return -1;
            }
            qd_proj_name(a, p, name);                   /* (the one worked on: in RAM, its header written by itself) */
            return QD_SX_OK;
        }
        if (src != QD_SXS_MINE || slot < 1 || slot > QD_NLIB)
            return QD_SX_BAD_SLOT;
        if (code < 0)
            return QD_SX_NOT_FOUND;
        qd_undo_input(a, 0);                            /* (its id kept: its references follow) */
        qd_undo_hint(a, QD_UK_SOUNDS);
        qd_undo_snd(a, QD_UNDO_RLIB, slot - 1);
        qd_lib_read(a, slot - 1, &s);
        memset(s.name, 0, sizeof s.name);
        strncpy(s.name, name, QD_SOUND_NAME - 1);
        qd_lib_write(a, slot - 1, &s);
        qd_pool_resolve(a);
        return QD_SX_OK;
    case QD_SX_DELETE:
    case QD_SX_USERS:
        if ((src != QD_SXS_MINE && src != QD_SXS_POOL) || slot < 1 || slot > (src == QD_SXS_MINE ? QD_NLIB : QD_NSOUND))
            return QD_SX_BAD_SLOT;
        if (code < 0)
            return QD_SX_NOT_FOUND;
        return sx_job_begin(a, cmd, src, slot - 1, 0, cmd == QD_SX_DELETE ? b[3] : 0, cmd == QD_SX_DELETE) ? -1
                                                                                                          : QD_SX_BUSY;
    case QD_SX_SWAP: {
        const int y = sx_u14(b + 3), max = src == QD_SXS_MINE ? QD_NLIB : QD_NSOUND;
        v[1] = y;
        if ((src != QD_SXS_MINE && src != QD_SXS_POOL) || slot < 1 || y < 1 || slot > max || y > max || slot == y)
            return QD_SX_BAD_SLOT;
        if (code < 0 && sx_code(a, src, y) < 0)
            return QD_SX_NOT_FOUND;
        if (src == QD_SXS_POOL)
            return sx_job_begin(a, cmd, src, slot, y, 0, 1) ? -1 : QD_SX_BUSY;
        {
            qd_sound_t t;
            qd_undo_input(a, 0);
            qd_undo_hint(a, QD_UK_SOUNDS);
            qd_undo_snd(a, QD_UNDO_RLIB, slot - 1);
            qd_undo_snd(a, QD_UNDO_RLIB, y - 1);
            qd_lib_read(a, slot - 1, &s);
            qd_lib_read(a, y - 1, &t);
            qd_lib_write(a, slot - 1, &t);
            qd_lib_write(a, y - 1, &s);
        }
        return QD_SX_OK;
    }
    case QD_SX_POOLREF: {
        const int id = (b[2] & 0x7F) << 14 | (b[3] & 0x7F) << 7 | (b[4] & 0x7F);
        slot = sx_u14(b);
        v[0] = slot, v[1] = id;
        if (slot > QD_NSOUND)
            return QD_SX_BAD_SLOT;
        if (!id) {                                      /* (cleared: DELETE's job, with CONFIRM) */
            if (!slot || !qd_sound_used(&a->proj, slot))
                return QD_SX_NOT_FOUND;
            return sx_job_begin(a, cmd, QD_SXS_POOL, slot - 1, 0, QD_SXD_CONFIRM, 1) ? -1 : QD_SX_BUSY;
        }
        if (!((id >= 1 && id <= QD_NFACTORY) || qd_lib_find(a, id) >= 0))
            return QD_SX_NOT_FOUND;
        if (!slot)
            slot = v[0] = qd_pool_free_slot(&a->proj);
        if (!slot)
            return QD_SX_FULL;
        qd_undo_input(a, 0);
        qd_undo_hint(a, QD_UK_SOUNDS);
        qd_undo_snd(a, QD_UNDO_RPOOL, slot);
        memset(&s, 0, sizeof s);
        s.preset = (uint16_t)id;
        strcpy(s.name, "?");
        qd_sound_write(&a->proj, slot, &s);
        qd_pool_resolve(a);
        qd_sound_recount(&a->proj);
        return QD_SX_OK;
    }
    case QD_SX_PROJECT: {
        const int p = b[1] - 1, q = b[2] - 1;
        v[0] = b[1], v[1] = b[2];
        if (src < QD_SXP_LOAD || src > QD_SXP_DELETE || p < 0 || p >= QD_NPROJ ||
            (src == QD_SXP_COPY && (q < 0 || q >= QD_NPROJ || q == p || q == ps->proj)))
            return QD_SX_BAD_SLOT;
        if (!ps->on || ((src == QD_SXP_COPY || src == QD_SXP_DELETE) && !ps->pexists[p] && p != ps->proj))
            return QD_SX_NOT_FOUND;
        return sx_job_begin(a, cmd, src, p, q < 0 ? 0 : q, 0, 1) ? -1 : QD_SX_BUSY;
    }
    }
    return QD_SX_BAD_COMMAND;
}

/* USERS (57): source, slot, how many projects, each (project, steps 2), this project's tracks (bit t) */
static void sx_users_reply(qd_app_t *a)
{
    const qd_sx_work_t *w = &a->sx.wk;
    uint8_t b[8 + 3 * QD_NPROJ];
    uint32_t o = 0;
    int p;
    b[o++] = QD_SX_R_USERS;
    b[o++] = w->src;
    o = sx_put14(b, o, (uint32_t)w->x + 1u);
    b[o++] = w->np;
    for (p = 0; p < QD_NPROJ; p++)
        if (w->pst[p]) {
            b[o++] = (uint8_t)p;
            o = sx_put14(b, o, (uint32_t)w->pst[p] - 1u);
        }
    b[o++] = (uint8_t)(w->tracks & 0x7F);
    sx_reply(a, b, o);
}

/* the job's next step (the store's: between frames, outside the locks; paused while playing, USERS apart). DELETE and
 * USERS (POOL REF 0 too): the users counted first, a project a step (MY presets); DELETE without CONFIRM of a used one
 * answers IN USE (projects, steps) and stops; else MY preset: each other project's records that load it keep their
 * copy (a project a step), then qd_preset_delete (its pool references go: their steps back to the track's sound, as on
 * the panel; one undo level); a pool sound: qd_pool_delete */
void qd_sx_work(qd_app_t *a)
{
    qd_sx_work_t *w = &a->sx.wk;
    const int cmd = w->cmd;
    int st = QD_SX_OK, v1 = w->x + 1, v2 = w->y + 1;
    if (!cmd || (a->seq.playing && cmd != QD_SX_USERS))
        return;
    switch (cmd) {
    case QD_SX_STORE:                                   /* (P2-B: a STORE over a used MY PRESETS place) */
    case QD_SX_STORE6: {
        const int id = qd_lib_used(a, w->x) ? qd_lib_preset(a, w->x) : 0;
        if (id && w->step < QD_NPROJ) {                 /* (the other projects' records that load it: their copies) */
            if (w->step != a->per.proj)
                qd_preset_will_change_proj(a, id, w->step);
            w->step++;
            return;
        }
        if (id)                                         /* (this one's last: as it is now) */
            qd_preset_will_change_proj(a, id, a->per.proj);
        w->cmd = 0;
        a->per.wc_done = (uint16_t)id;                  /* (lib_put: no will_change again) */
        sx_store_apply(a, &a->sx.pend);
        a->per.wc_done = 0;
        return;
    }
    case QD_SX_FLUSH:
        qd_persist_flush(a);
        w->cmd = 0;
        sx_status(a);
        return;
    case QD_SX_RENAME: {                                /* (another project's header) */
        char nm[QD_PROJ_NAME];
        memcpy(nm, a->per.pname[w->x], sizeof nm);
        qd_proj_name(a, w->x, nm);
        v2 = 0;
        break;
    }
    case QD_SX_PROJECT: {
        const int p = w->x, q = w->y;
        st = (w->src == QD_SXP_LOAD ? (p == a->per.proj || qd_proj_load(a, p)) : w->src == QD_SXP_NEW ? qd_proj_new(a, p)
              : w->src == QD_SXP_COPY ? qd_proj_copy(a, p, q) : qd_proj_delete(a, p)) ? QD_SX_OK : QD_SX_NOT_FOUND;
        break;
    }
    case QD_SX_SWAP: {                                  /* (the pool's) */
        int i;
        v1 = w->x, v2 = w->y;
        if (!w->step) {
            if (qd_pool_unref_pending(a))
                return;                                 /* (a removed sound's locks still in records: swept first) */
            qd_pool_swap_slots(a, w->x, w->y);
            w->step = 1;
            return;
        }
        for (i = 0; i < 4 && w->step <= QD_NPAT; i++, w->step++)   /* (a few patterns a step: most need no write) */
            if (!qd_pool_renumber_pat(a, w->step - 1, w->x, w->y))
                return;                                 /* (no room now: again next step) */
        if (w->step <= QD_NPAT)
            return;
        qd_sound_recount(&a->proj);
        break;
    }
    default:
        v2 = 0;
        if (w->src == QD_SXS_MINE ? qd_lib_preset(a, w->x) != w->id : !qd_sound_used(&a->proj, w->x + 1)) {
            st = QD_SX_NOT_FOUND;                       /* (gone, or another one there now) */
            break;
        }
        if (cmd == QD_SX_USERS || !(w->flags & QD_SXD_CONFIRM)) {
            int s = 0;
            if (w->step < QD_NPROJ) {                   /* (a project a step; the pool: this one, once) */
                const int p = w->src == QD_SXS_MINE ? w->step : a->per.proj;
                if (w->src == QD_SXS_MINE ? qd_preset_usage_proj(a, w->id, p, &s, &w->tracks)
                                          : (s = qd_pool_users_all(a, w->x + 1, &w->tracks), 1)) {
                    w->np++;
                    w->steps = (uint16_t)(w->steps + s > 0x3FFF ? 0x3FFF : w->steps + s);
                    w->pst[p] = (uint8_t)(s >= 254 ? 255 : s + 1);
                }
                w->step = (uint8_t)(w->src == QD_SXS_MINE ? w->step + 1 : QD_NPROJ);
                return;
            }
            if (cmd == QD_SX_USERS) {
                sx_users_reply(a);
                w->cmd = 0;
                return;
            }
            if (w->steps || w->tracks || (w->src == QD_SXS_MINE && w->np)) {
                st = QD_SX_IN_USE, v1 = w->np, v2 = w->steps;   /* (the page asks, then sends it with CONFIRM) */
                break;
            }
            w->flags |= QD_SXD_CONFIRM, w->step = 0;    /* (unused: deleted now) */
        }
        if (w->src == QD_SXS_MINE && w->step < QD_NPROJ) {   /* (the other projects' records that load it) */
            if (w->step != a->per.proj)
                qd_preset_will_change_proj(a, w->id, w->step);
            w->step++;
            return;
        }
        qd_undo_input(a, 0);
        if (w->src == QD_SXS_MINE)
            qd_preset_delete(a, w->x);                  /* (this project's records, its pool references) */
        else
            qd_pool_delete(a, w->x + 1);
        break;
    }
    w->cmd = 0;
    sx_ack(a, cmd, st, v1, v2);
}

/* P2-B: PARAM: track (0..5, 127 the selected), id, value + 2^20 (3 x 7 bits), flags (01 ACK): the track's sound as a knob
 * turned on the unit (qd_player_live_edit: clamped, the voices sounding follow; no undo level) */
static void sx_param(qd_app_t *a, const uint8_t *b, uint32_t n)
{
    int t, id, st = QD_SX_OK;
    if (n < 7u) {
        sx_ack(a, QD_SX_PARAM, QD_SX_BAD_LENGTH, 0, 0);
        return;
    }
    t = b[0] == QD_SXP_SELECTED ? a->sel : b[0];
    id = b[1];
    if (t >= QD_NTRACK)
        st = QD_SX_BAD_TRACK;
    else if (id >= QD_QUAD_NP)
        st = QD_SX_NOT_FOUND;
    else
        qd_player_live_edit(&a->player, t, id, (int)((uint32_t)b[2] << 14 | (uint32_t)b[3] << 7 | b[4]) - QD_SXP_OFS);
    if (st || (b[5] & QD_SXP_ACK))
        sx_ack(a, QD_SX_PARAM, st, t + 1, id);
}

void qd_app_sysex(qd_app_t *a, const uint8_t *f, uint32_t n)
{
    uint32_t i, sum = 0;
    a->edit_gen++;                                      /* (P2.6c: the patterns may change: persist.c looks) */
    if (n < 5u || f[0] != QD_SX_ID0 || f[1] != QD_SX_ID1 || f[2] != QD_SX_ID2)
        return;                                         /* (not FIZZ's) */
    a->sx.frames++;
    if (f[3] != QD_SX_PROTO) {
        const uint8_t b[3] = {QD_SX_R_ERROR, QD_SX_BAD_VERSION, f[3] & 0x7F};
        a->sx.errors++;
        sx_reply(a, b, 3);
        return;
    }
    for (i = 4; i < n; i++)
        sum += f[i];
    if (n < 6u || (sum & 0x7Fu)) {                      /* (a command and its checksum at least; the sum 0) */
        if (n >= 5u && (f[4] == QD_SX_STORE || f[4] == QD_SX_STORE6))
            sx_store_reply(a, QD_SX_BAD_CHECKSUM, n > 5u ? (f[5] & QD_SXF_POOL) != 0 : 0, 0);
        else {
            const uint8_t b[3] = {QD_SX_R_ERROR, QD_SX_BAD_CHECKSUM, f[4] & 0x7F};
            a->sx.errors++;
            sx_reply(a, b, 3);
        }
        return;
    }
    switch (f[4]) {
    case QD_SX_IDENTITY: {
        uint8_t b[QD_SX_OUT_MAX];
        const char *v = a->sx.version ? a->sx.version : "FIZZ";
        uint32_t o = 0;
        b[o++] = QD_SX_R_IDENTITY;
        b[o++] = QD_SX_PROTO;
        for (; *v && o < QD_SX_OUT_MAX - 8u; v++)
            b[o++] = (uint8_t)(*v & 0x7F);
        sx_reply(a, b, o);
        break;
    }
    case QD_SX_STORE:
        sx_store(a, f + 5, n - 5u, 0);
        break;
    case QD_SX_STORE6:                                  /* E1 */
        sx_store(a, f + 5, n - 5u, 1);
        break;
    case QD_SX_LABELS:                                  /* P1.6l */
        sx_labels(a, f + 5, n - 5u);
        break;
    case QD_SX_LIST:                                    /* P1.6m */
        sx_list(a, f + 5, n - 5u);
        break;
    case QD_SX_DUMP:
        sx_dump(a, f + 5, n - 5u);
        break;
    case QD_SX_PARAM:                                   /* (Manager P2-B) */
        sx_param(a, f + 5, n - 5u);
        break;
    case QD_SX_STATUS:                                  /* (the web manager's Phase 1) */
        sx_status(a);
        break;
    case QD_SX_PROJECTS:
        sx_projects(a);
        break;
    case QD_SX_FLUSH:
    case QD_SX_TAGS:
    case QD_SX_RENAME:
    case QD_SX_DELETE:
    case QD_SX_SWAP:
    case QD_SX_POOLREF:
    case QD_SX_USERS:
    case QD_SX_PROJECT: {
        int v[2];
        const int st = sx_edit(a, f[4], f + 5, n - 5u, v);
        if (st >= 0)
            sx_ack(a, f[4], st, v[0], v[1]);
        break;
    }
    default: {
        const uint8_t b[3] = {QD_SX_R_ERROR, QD_SX_BAD_COMMAND, f[4] & 0x7F};
        a->sx.errors++;
        sx_reply(a, b, 3);
        break;
    }
    }
}
