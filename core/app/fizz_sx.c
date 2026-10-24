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

static void sx_store_reply(qd_app_t *a, int status, int pool, int slot)
{
    const uint8_t b[5] = {QD_SX_R_STORE, (uint8_t)status, (uint8_t)pool, (uint8_t)((slot >> 7) & 0x7F), (uint8_t)(slot & 0x7F)};
    if (status)
        a->sx.errors++;
    sx_reply(a, b, 5);
}

/* STORE: body[0..n) after the command byte, its checksum included */
static void sx_store(qd_app_t *a, const uint8_t *b, uint32_t n, int six)
{
    uint8_t blob[QD_SX_BLOB6 + 8];
    char name[QD_SX_NAME + 1];
    int16_t patch[QD_QUAD_NP];
    int flags, pool, slot, i, track, lib_slot = 0;
    const uint32_t body = six ? QD_SX_STORE6_BODY : QD_SX_STORE_BODY;   /* (E1: STORE6: the 128-byte blob) */
    if (n != body + 1u && n != body + 2u) {             /* (P1.5x: + the tags byte) */
        sx_store_reply(a, QD_SX_BAD_LENGTH, n ? (b[0] & QD_SXF_POOL) != 0 : 0, 0);
        return;
    }
    flags = b[0];
    if (flags & QD_SXF_DUMP)
        return;                                         /* (P1.6m: a DUMP's reply, echoed: not stored, no reply) */
    pool = (flags & QD_SXF_POOL) != 0;
    slot = b[1] << 7 | b[2];
    for (i = 0; i < QD_SX_NAME; i++)                    /* the name: printable ASCII, the trailing spaces off */
        name[i] = b[3 + i] >= 0x20 && b[3 + i] <= 0x7E ? (char)b[3 + i] : b[3 + i] ? '?' : ' ';
    name[QD_SX_NAME] = 0;
    for (i = QD_SX_NAME - 1; i >= 0 && name[i] == ' '; i--)
        name[i] = 0;
    if (!name[0])
        strcpy(name, "IMPORTED");
    if (qd_sx_unpack7(b + 22, six ? QD_SX_BLOB6_7 : QD_SX_BLOB7, blob) != (six ? QD_SX_BLOB6 : QD_SX_BLOB)) {
        sx_store_reply(a, QD_SX_BAD_LENGTH, pool, slot);
        return;
    }
    if (blob[1] == 5 || blob[1] == 6)                   /* (version 5 / 6: DRIVE after its values, as FIZZ packs it) */
        blob[87] = b[15] & 0x7F;
    if ((!six && blob[1] == 6) || !qd_quad_unpack(blob, six ? QD_SX_BLOB6 : QD_SX_BLOB, patch)) {
        sx_store_reply(a, QD_SX_BAD_PATCH, pool, slot);
        return;
    }
    patch[QD_QP_DRIVE] = (int16_t)(b[15] & 0x7F);
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
    qd_undo_input(a, 0);                                /* (an input: the batch's gesture stays open while they come) */
    a->undo.join = (flags & QD_SXF_BATCH) != 0;         /* (a batch: the previous import's level goes on) */
    if (pool) {                                         /* P1.5w: a MY preset (imported), referenced at that number */
        const int m = qd_lib_free_slot(a);
        qd_sound_t s;
        qd_lib_store_ex(a, m, name, patch, QD_SNDF_IMPORTED);
        qd_undo_snd(a, QD_UNDO_RPOOL, slot);            /* (P2.1: as it was, for the device's undo) */
        memset(&s, 0, sizeof s);
        s.preset = qd_lib_preset(a, m);
        strcpy(s.name, "?");
        qd_sound_write(&a->proj, slot, &s);
        qd_pool_resolve(a);
        lib_slot = m;
    } else {
        qd_lib_store_ex(a, slot - 1, name, patch, QD_SNDF_IMPORTED);   /* (P1.5t: one of MY presets, imported) */
        lib_slot = slot - 1;
    }
    {   /* P1.5x: the preset keeps the mix the frame gives (mixmask), LOAD or not; the tags byte (nonzero) its tags */
        qd_sound_t ls;
        const int mask = b[21];
        qd_lib_read(a, lib_slot, &ls);
        ls.pan = mask & QD_SXM_PAN ? (int8_t)((int)b[17] - 64) : 0;
        ls.chorus = mask & QD_SXM_CHORUS ? b[18] : 0;
        ls.delay = mask & QD_SXM_DELAY ? b[19] : 0;
        ls.reverb = mask & QD_SXM_REVERB ? b[20] : 0;
        if (n == body + 2u && (b[body] & QD_PK_MASK))
            ls.tags = (uint8_t)(b[body] & QD_PK_MASK);
        qd_lib_write(a, lib_slot, &ls);
        qd_pool_resolve(a);
    }
    a->undo.join = 0;
    track = b[16];
    if ((flags & QD_SXF_LOAD) && track < QD_NTRACK) {   /* onto a track: its sound (a pool sound: its source), its mix */
        qd_track_t *t = &a->model.t[track];
        const int mask = b[21];
        qd_track_load_sound(t, pool ? slot : 0, name, patch);
        t->preset = pool ? qd_lib_preset(a, lib_slot) : qd_lib_preset(a, slot - 1);
        if (mask & QD_SXM_PAN)
            t->mix.pan = (int8_t)((int)b[17] - 64);
        if (mask & QD_SXM_CHORUS)
            t->mix.chorus = b[18];
        if (mask & QD_SXM_DELAY)
            t->mix.delay = b[19];
        if (mask & QD_SXM_REVERB)
            t->mix.reverb = b[20];
    }
    qd_ctl_imported(a, name, pool, slot);               /* the panel: its cassette, "imported" */
    sx_store_reply(a, QD_SX_OK, pool, slot);
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
 * the preset made again) */
static int sx_dump_one(qd_app_t *a, int src, int k)
{
    static uint8_t b[QD_SX_STORE6_BODY + 4];
    uint8_t blob[QD_SX_BLOB6];
    qd_sound_t s;
    uint32_t o = 0;
    int i, six;
    if (!sx_entry(a, src, k, &s))
        return -1;
    six = qd_quad_ext_used(s.patch);
    if (six)
        qd_quad_pack(s.patch, blob);
    else
        qd_quad_pack5(s.patch, blob);
    b[o++] = six ? QD_SX_STORE6 : QD_SX_STORE;
    b[o++] = (uint8_t)(QD_SXF_DUMP | (src == QD_SXS_POOL ? QD_SXF_POOL : 0));
    {
        const int slot = src == QD_SXS_FACTORY ? 0 : k + 1;   /* (a factory preset: back as a new MY preset) */
        b[o++] = (uint8_t)((slot >> 7) & 0x7F), b[o++] = (uint8_t)(slot & 0x7F);
    }
    for (i = 0; i < QD_SX_NAME; i++)
        b[o++] = (uint8_t)(s.name[i] && i < (int)strlen(s.name) ? s.name[i] & 0x7F : ' ');
    b[o++] = (uint8_t)(s.patch[QD_QP_DRIVE] & 0x7F);
    b[o++] = 0;                                         /* (no LOAD) */
    b[o++] = (uint8_t)(s.pan + 64 < 0 ? 0 : s.pan + 64 > 127 ? 127 : s.pan + 64);
    b[o++] = s.chorus & 0x7F, b[o++] = s.delay & 0x7F, b[o++] = s.reverb & 0x7F;
    b[o++] = QD_SXM_PAN | QD_SXM_CHORUS | QD_SXM_DELAY | QD_SXM_REVERB;
    o += qd_sx_pack7(blob, six ? QD_SX_BLOB6 : QD_SX_BLOB, b + o);
    b[o++] = (uint8_t)(s.tags & QD_PK_MASK);
    return sx_reply(a, b, o) ? 1 : 0;
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
    if (src > QD_SXS_POOL || (slot && sx_dump_one(a, src, slot - 1) < 0)) {
        const uint8_t e[3] = {QD_SX_R_ERROR, QD_SX_BAD_SLOT, QD_SX_DUMP};
        a->sx.errors++;
        sx_reply(a, e, 3);
        return;
    }
    if (!slot) {
        a->sx.job = QD_SX_DUMP;
        a->sx.job_src = src, a->sx.job_at = 0;
        sx_job(a);
    }
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
    default: {
        const uint8_t b[3] = {QD_SX_R_ERROR, QD_SX_BAD_COMMAND, f[4] & 0x7F};
        a->sx.errors++;
        sx_reply(a, b, 3);
        break;
    }
    }
}
