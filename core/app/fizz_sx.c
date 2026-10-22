/* SPDX-License-Identifier: GPL-3.0-only */
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
static void sx_reply(qd_app_t *a, const uint8_t *b, uint32_t n)
{
    qd_sx_t *s = &a->sx;
    uint8_t *q;
    uint32_t i, sum = 0, o = 0;
    if (s->w - s->r >= QD_SX_OUT || n + 7u > QD_SX_OUT_MAX)
        return;                                         /* (no room: the oldest stay; a host asks again) */
    q = s->q[s->w % QD_SX_OUT];
    q[o++] = 0xF0, q[o++] = QD_SX_ID0, q[o++] = QD_SX_ID1, q[o++] = QD_SX_ID2, q[o++] = QD_SX_PROTO;
    for (i = 0; i < n; i++) {
        q[o++] = b[i] & 0x7Fu;
        sum += b[i] & 0x7Fu;
    }
    q[o++] = (uint8_t)((0x80u - (sum & 0x7Fu)) & 0x7Fu);
    q[o++] = 0xF7;
    s->n[s->w % QD_SX_OUT] = (uint8_t)o;
    s->w++;
}

int qd_app_sysex_take(qd_app_t *a, uint8_t *out, uint32_t max)
{
    qd_sx_t *s = &a->sx;
    uint32_t n;
    if (s->r == s->w)
        return 0;
    n = s->n[s->r % QD_SX_OUT];
    if (n > max)
        return 0;
    memcpy(out, s->q[s->r % QD_SX_OUT], n);
    s->r++;
    return (int)n;
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
        qd_sound_t *s = &a->proj.sound[slot - 1];
        qd_lib_store_ex(a, m, name, patch, QD_SNDF_IMPORTED);
        memset(s, 0, sizeof *s);
        s->preset = a->per.lib[m].preset;
        strcpy(s->name, "?");
        qd_pool_resolve(a);
        lib_slot = m;
    } else {
        qd_lib_store_ex(a, slot - 1, name, patch, QD_SNDF_IMPORTED);   /* (P1.5t: one of MY presets, imported) */
        lib_slot = slot - 1;
    }
    {   /* P1.5x: the preset keeps the mix the frame gives (mixmask), LOAD or not; the tags byte (nonzero) its tags */
        qd_sound_t *ls = &a->per.lib[lib_slot];
        const int mask = b[21];
        ls->pan = mask & QD_SXM_PAN ? (int8_t)((int)b[17] - 64) : 0;
        ls->chorus = mask & QD_SXM_CHORUS ? b[18] : 0;
        ls->delay = mask & QD_SXM_DELAY ? b[19] : 0;
        ls->reverb = mask & QD_SXM_REVERB ? b[20] : 0;
        if (n == body + 2u && (b[body] & QD_PK_MASK))
            ls->tags = (uint8_t)(b[body] & QD_PK_MASK);
        qd_pool_resolve(a);
    }
    a->undo.join = 0;
    track = b[16];
    if ((flags & QD_SXF_LOAD) && track < QD_NTRACK) {   /* onto a track: its sound (a pool sound: its source), its mix */
        qd_track_t *t = &a->model.t[track];
        const int mask = b[21];
        qd_track_load_sound(t, pool ? slot : 0, name, patch);
        t->preset = pool ? a->per.lib[lib_slot].preset : a->per.lib[slot - 1].preset;
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

void qd_app_sysex(qd_app_t *a, const uint8_t *f, uint32_t n)
{
    uint32_t i, sum = 0;
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
    default: {
        const uint8_t b[3] = {QD_SX_R_ERROR, QD_SX_BAD_COMMAND, f[4] & 0x7F};
        a->sx.errors++;
        sx_reply(a, b, 3);
        break;
    }
    }
}
