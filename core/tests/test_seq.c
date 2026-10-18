/* SPDX-License-Identifier: GPL-3.0-only */
/* The sequencer's tests (core/seq: pattern.h, seq.h, player.h; PLAN.md §4 .. §6):
 *
 *   units       the lock pool (sorted, counted, full, copy, clear), the editing API, the conditions' table, the patch
 *               of a trig (a sound lock and p-locks against a patch built by hand), the dip (the largest sample step
 *               of a routing change on a sounding voice, with and without it), determinism (the same run twice: the
 *               same samples)
 *   timelines   sixteen patterns, each aimed at a feature, run through the sequencer, the player and the voice pool
 *               (the voices rendered, no effects); every event becomes a line of text (the sample, the track, on / off /
 *               lock / legato / dip, the note, the velocity, the hash of the voice's patch, its own mask and mix locks)
 *               written to OUT/timeline-NAME.txt and compared with the committed golden GOLDEN/NAME.txt; and every
 *               timeline is checked against THE REFERENCE below, an independent implementation from first principles:
 *               it walks each track's steps in plain loops (no look-ahead, no queue, no cursors), computes each
 *               event's time in seconds from the tempo, the rate, swing, micro, ratchet and strum, evaluates the
 *               conditions and chance with its own code, builds each trig's patch by hand (the preset or the track's
 *               sound, the locks written over it) and hashes it, and replays the holds (lengths, INF, a newer strike of
 *               the same note, TIE). Every reference event must be in the timeline (within a sample: the reference's
 *               seconds against the engine's integer ticks) and every timeline event in the reference.
 *
 *     test_seq GOLDEN_DIR OUT_DIR [--bless]     exit 1 on a failed check; --bless writes the goldens */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mix.h"
#include "player.h"
#include "seq.h"

static int fails, checks;
#define CHECK(c, ...)                                     \
    do {                                                  \
        checks++;                                         \
        if (!(c)) {                                       \
            fails++;                                      \
            printf("  FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                          \
            printf("\n");                                 \
        }                                                 \
    } while (0)

static const char *GOLD, *OUT;
static int BLESS;

/* ================================================================ the rig: sequencer, player, pool, mix === */
static qd_project_t PROJ;
static qd_model_t MODEL;
static qd_pool_t POOL;
static qd_mix_t MIX;
static qd_seq_t SEQ;
static qd_player_t PL;

typedef struct {
    int64_t sample;
    int track, kind, note, vel, voice, step, hit, id, value;
    uint32_t hash;
    int hashed;
    char own[320], mix[64];
} tl_t;
#define NTL 60000
static tl_t ENG[NTL], REF[NTL];
static int NENG, NREF;
static char USED[NTL];

static void own_str(const qd_vattr_t *at, char *out)
{
    int i, n = 0, all = 1;
    for (i = 0; i < QD_QUAD_NP; i++) {
        const int b = (int)((at->own[i >> 5] >> (i & 31)) & 1u);
        all &= b;
        n += b;
    }
    if (!n) {
        strcpy(out, "-");
        return;
    }
    if (all) {
        strcpy(out, "all");
        return;
    }
    out[0] = 0;
    for (i = 0; i < QD_QUAD_NP; i++)
        if ((at->own[i >> 5] >> (i & 31)) & 1u)
            snprintf(out + strlen(out), 320 - strlen(out), "%s%d", out[0] ? "," : "", i);
}

static void mix_str(const qd_vattr_t *at, char *out)
{
    static const char K[QD_VMIX_N] = {'p', 'd', 'r', 'c', 'x', 'l'};
    int k;
    out[0] = 0;
    for (k = 0; k < QD_VMIX_N; k++)
        if (at->mixm & (1u << k))
            snprintf(out + strlen(out), 64 - strlen(out), "%s%c%d", out[0] ? "," : "", K[k], at->mixv[k]);
    if (!out[0])
        strcpy(out, "-");
}

static void log_cb(void *ctx, const qd_plog_t *l)
{
    tl_t *e;
    (void)ctx;
    if (NENG >= NTL)
        return;
    e = &ENG[NENG++];
    memset(e, 0, sizeof *e);
    e->sample = (int64_t)l->sample;
    e->track = l->track;
    e->kind = l->kind;
    e->note = l->note;
    e->vel = l->kind == QD_LOG_OFF || l->kind == QD_LOG_OFF_KEPT || l->kind == QD_LOG_LOCK || l->kind == QD_LOG_DIP
                 ? 0 : l->vel;
    e->voice = l->voice;
    e->step = l->step;
    e->hit = l->hit;
    e->id = l->kind == QD_LOG_DIP ? l->id : -1;
    e->value = l->kind == QD_LOG_DIP ? l->value : 0;
    e->hashed = l->voice >= 0 && l->kind != QD_LOG_OFF && l->kind != QD_LOG_OFF_KEPT;
    e->hash = e->hashed ? l->hash : 0;
    if (e->hashed) {
        own_str(&l->at, e->own);
        mix_str(&l->at, e->mix);
    } else {
        strcpy(e->own, "-");
        strcpy(e->mix, "-");
    }
}

static void rig_reset(void)
{
    int t;
    qd_model_init(&MODEL);
    qd_project_init(&PROJ);
    qd_pool_init(&POOL, 16);
    for (t = 0; t < QD_NTRACK; t++)
        qd_pool_set_track(&POOL, t, MODEL.t[t].limit, MODEL.t[t].reserve);
    qd_mix_init(&MIX);
    qd_seq_init(&SEQ, &PROJ, &MODEL);
    qd_player_init(&PL, &SEQ, &POOL, &MODEL);
    PL.log = log_cb;
    NENG = 0;
}

/* one block: the sequencer into the pool, the voices rendered and mixed (no effects); mono = (l + r) / 2 */
static void rig_block(int32_t *mono)
{
    qd_quad_owner_t *ow[QD_NTRACK];
    qd_mix_track_t mt[QD_NTRACK];
    int32_t l[32], r[32], sc[32], sd[32], sr[32];
    int i;
    for (i = 0; i < QD_NTRACK; i++) {
        ow[i] = &MODEL.t[i].owner;
        mt[i] = MODEL.t[i].mix;
    }
    qd_player_block(&PL);
    qd_mix_block(&MIX, &POOL, ow, mt, QD_NTRACK, l, r, sc, sd, sr);
    if (mono)
        for (i = 0; i < 32; i++)
            mono[i] = (l[i] + r[i]) / 2;
}

static void rig_run_to(uint64_t sample)
{
    while (SEQ.now < sample)
        rig_block(0);
}

static const char *const KIND[] = {"on", "off", "off~", "lock", "legato", "dip", "live"};

static void line_text(const tl_t *e, char *out, int with_voice)
{
    char h[16], idv[24] = "";
    if (e->hashed)
        snprintf(h, sizeof h, "%08x", e->hash);
    else
        strcpy(h, "-");
    if (e->id >= 0)
        snprintf(idv, sizeof idv, " %d=%d", e->id, e->value);
    if (with_voice)
        snprintf(out, 1024, "%9lld T%d %-6s n%-3d v%-3d %s own=%s mix=%s vx%d s%d h%d%s", (long long)e->sample, e->track + 1,
                KIND[e->kind], e->note, e->vel, h, e->own, e->mix, e->voice, e->step, e->hit, idv);
    else
        snprintf(out, 1024, "%9lld T%d %-6s n%-3d v%-3d %s own=%s mix=%s%s", (long long)e->sample, e->track + 1, KIND[e->kind],
                e->note, e->vel, h, e->own, e->mix, idv);
}

/* the timeline: written, compared with its golden (or the golden written with --bless) */
static void timeline(const char *name, const char *title)
{
    char path[1024], line[1024], gl[1024];
    FILE *o, *g;
    int i, diff = 0, ln = 0;
    snprintf(path, sizeof path, "%s/timeline-%s.txt", OUT, name);
    o = fopen(path, "w");
    if (!o) {
        CHECK(0, "cannot write %s", path);
        return;
    }
    fprintf(o, "# %s: %s\n# sample track kind note vel patch-hash own-mask mix-locks voice step hit [id=value]\n",
            name, title);
    for (i = 0; i < NENG; i++) {
        line_text(&ENG[i], line, 1);
        fprintf(o, "%s\n", line);
    }
    fclose(o);
    snprintf(path, sizeof path, "%s/%s.txt", GOLD, name);
    if (BLESS) {
        o = fopen(path, "w");
        if (o) {
            fprintf(o, "# %s: %s\n# sample track kind note vel patch-hash own-mask mix-locks voice step hit "
                       "[id=value]\n", name, title);
            for (i = 0; i < NENG; i++) {
                line_text(&ENG[i], line, 1);
                fprintf(o, "%s\n", line);
            }
            fclose(o);
        }
        return;
    }
    g = fopen(path, "r");
    if (!g) {
        CHECK(0, "%s: no golden %s (run with --bless to write it)", name, path);
        return;
    }
    i = 0;
    while (fgets(gl, sizeof gl, g)) {
        gl[strcspn(gl, "\n")] = 0;
        ln++;
        if (gl[0] == '#')
            continue;
        if (i >= NENG) {
            diff++;
            if (diff < 4)
                printf("    golden line %d not in the run: %s\n", ln, gl);
            continue;
        }
        line_text(&ENG[i++], line, 1);
        if (strcmp(line, gl)) {
            diff++;
            if (diff < 4)
                printf("    line %d: golden  %s\n            run     %s\n", ln, gl, line);
        }
    }
    fclose(g);
    if (i < NENG)
        diff += NENG - i;
    CHECK(!diff, "%s: the timeline differs from its golden (%d lines)", name, diff);
}

/* every reference line in the timeline (tol samples apart at most) and every timeline line in the reference */
static int compare_ref(const char *name, int tol)
{
    int i, j, miss = 0, extra = 0;
    char a[1024], b[1024];
    memset(USED, 0, sizeof USED);
    for (i = 0; i < NREF; i++) {
        const tl_t *r = &REF[i];
        int found = -1;
        for (j = 0; j < NENG; j++) {
            const tl_t *e = &ENG[j];
            if (USED[j] || e->track != r->track || e->kind != r->kind || e->note != r->note || e->vel != r->vel ||
                e->hashed != r->hashed || (r->hashed && e->hash != r->hash) || strcmp(e->own, r->own) ||
                strcmp(e->mix, r->mix) || e->id != r->id || e->value != r->value)
                continue;
            if (llabs(e->sample - r->sample) <= tol) {
                found = j;
                break;
            }
        }
        if (found < 0) {
            if (miss++ < 5) {
                line_text(r, a, 0);
                printf("    %s: the reference's %s not in the timeline\n", name, a);
            }
        } else {
            USED[found] = 1;
        }
    }
    for (j = 0; j < NENG; j++)
        if (!USED[j] && extra++ < 5) {
            line_text(&ENG[j], b, 1);
            printf("    %s: the timeline's %s not in the reference\n", name, b);
        }
    for (j = 0, extra = 0; j < NENG; j++)
        extra += !USED[j];
    CHECK(!miss && !extra, "%s: reference %d events, timeline %d: %d missing, %d extra", name, NREF, NENG, miss,
          extra);
    return !miss && !extra;
}

/* ================================================================ the test's patterns === */
typedef struct {
    int t, s, type, nn, notes[4], vel, len, micro, ratch, ramp, cond, chance, sound, strum, nlk;
    int lk_id[8], lk_v[8];
} TS;
typedef struct { int rate, length, swing, limit, transpose; } TT;
typedef struct {
    int bpm, swing, master;
    TT trk[8];
    TS st[600];
    int n;
} TP;
static TP TPAT[8];

static TP *tp_new(int k, int bpm)
{
    TP *p = &TPAT[k];
    int t;
    memset(p, 0, sizeof *p);
    p->bpm = bpm;
    p->swing = 50;
    for (t = 0; t < 8; t++) {
        p->trk[t].rate = QD_RATE_1;
        p->trk[t].length = 16;
        p->trk[t].limit = 6;
    }
    return p;
}

static TS *S(TP *p, int t, int s, int type, int n1, int n2, int n3, int n4)
{
    TS *st = &p->st[p->n++];
    const int nv[4] = {n1, n2, n3, n4};
    int k;
    memset(st, 0, sizeof *st);
    st->t = t;
    st->s = s;
    st->type = type;
    for (k = 0; k < 4; k++)
        if (nv[k])
            st->notes[st->nn++] = nv[k];
    st->vel = 100;
    st->len = QD_LEN_1;
    st->ratch = 1;
    st->chance = 100;
    return st;
}
static void LK(TS *st, int id, int v)
{
    st->lk_id[st->nlk] = id;
    st->lk_v[st->nlk++] = v;
}

/* the test's pattern into the project through the editing API */
static void build(const TP *tp, int k)
{
    qd_pattern_t *P = &PROJ.pat[k];
    int i, t;
    qd_pat_init(P);
    P->bpm = (uint16_t)tp->bpm;
    P->swing = (uint8_t)tp->swing;
    P->master = (uint16_t)tp->master;
    for (t = 0; t < 8; t++) {
        P->trk[t].rate = (uint8_t)tp->trk[t].rate;
        P->trk[t].length = (uint8_t)tp->trk[t].length;
        P->trk[t].swing = (uint8_t)tp->trk[t].swing;
        P->trk[t].limit = (uint8_t)tp->trk[t].limit;
        P->trk[t].transpose = (int8_t)tp->trk[t].transpose;
    }
    for (i = 0; i < tp->n; i++) {
        const TS *s = &tp->st[i];
        uint8_t nv[4];
        int k2, r = 0;
        for (k2 = 0; k2 < s->nn; k2++)
            nv[k2] = (uint8_t)s->notes[k2];
        r |= qd_step_set_trig(P, s->t, s->s, s->type);
        if (s->nn)
            r |= qd_step_set_notes(P, s->t, s->s, nv, s->nn);
        r |= qd_step_set_vel(P, s->t, s->s, s->vel);
        r |= qd_step_set_len(P, s->t, s->s, s->len);
        r |= qd_step_set_micro(P, s->t, s->s, s->micro);
        r |= qd_step_set_ratchet(P, s->t, s->s, s->ratch, s->ramp);
        r |= qd_step_set_cond(P, s->t, s->s, s->cond);
        r |= qd_step_set_chance(P, s->t, s->s, s->chance);
        r |= qd_step_set_sound(P, s->t, s->s, s->sound);
        r |= qd_step_set_strum(P, s->t, s->s, s->strum);
        for (k2 = 0; k2 < s->nlk; k2++)
            r |= qd_lock_set(P, s->t, s->s, s->lk_id[k2], s->lk_v[k2]);
        if (s->type != QD_TRIG_NOTE)                    /* (the setters made it a NOTE: its type again) */
            r |= qd_step_set_trig(P, s->t, s->s, s->type);
        CHECK(r == QD_OK, "build: track %d step %d: %d", s->t, s->s, r);
    }
    CHECK(qd_pat_check(P), "build: pattern %d consistent", k);
}

/* ================================================================ THE REFERENCE === */
/* An independent model of what PLAN §4 .. §6 ask, written plainly: no look-ahead, no queue, no cursors. */
/* a step's ticks at each rate, from its 16ths (8, 4, 2, 4/3, 1, 2/3, 1/2, 3/2, 3/4) x 50400 */
static const int R_STEP_T[QD_NRATE] = {403200, 201600, 100800, 67200, 50400, 33600, 25200, 75600, 37800};
static const double R_LEN[QD_NLEN] = {0.125, 0.25, 0.5, 0.75, 1, 1.5, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128,
                                      -1};

static uint64_t r_mix64(uint64_t z)                     /* splitmix64 (the roll's documented generator) */
{
    z += 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}
static int r_roll(uint64_t seed, int64_t origin_ticks, int t, int64_t n)
{
    const uint64_t s = r_mix64(seed ^ (uint64_t)origin_ticks);
    return (int)(r_mix64(s ^ ((uint64_t)t << 56) ^ (uint64_t)n) % 100u);
}
static uint32_t r_fnv(const int16_t *p)
{
    uint32_t h = 2166136261u;
    int i;
    for (i = 0; i < QD_QUAD_NP; i++) {
        h = (h ^ ((uint16_t)p[i] & 0xFFu)) * 16777619u;
        h = (h ^ ((uint16_t)p[i] >> 8)) * 16777619u;
    }
    return h;
}

/* a trig's sound by hand */
typedef struct {
    int16_t patch[QD_QUAD_NP];
    qd_vattr_t at;
} rsnd_t;
static void r_sound(const TS *s, int t, rsnd_t *o)
{
    int k;
    memset(o, 0, sizeof *o);
    if (s->sound) {
        qd_quad_preset_patch(s->sound - 1, o->patch);
        for (k = 0; k < QD_QUAD_NP; k++)
            o->at.own[k / 32] |= 1u << (k % 32);
    } else {
        memcpy(o->patch, MODEL.t[t].patch, sizeof o->patch);
    }
    for (k = 0; k < s->nlk; k++) {
        const int id = s->lk_id[k];
        int16_t lo, hi;
        int v = s->lk_v[k];
        qd_lock_range(id, &lo, &hi);
        v = v < lo ? lo : v > hi ? hi : v;
        if (id < QD_QUAD_NP) {
            o->patch[id] = (int16_t)v;
            o->at.own[id / 32] |= 1u << (id % 32);
        } else {
            o->at.mixm |= (uint8_t)(1u << (id - QD_QUAD_NP));
            o->at.mixv[id - QD_QUAD_NP] = (int16_t)v;
        }
    }
}

/* the raw events of the run */
typedef struct {
    int64_t tick;
    int kind;                           /* QD_EV_* */
    int t, note, vel, step, hit, seq, pat;
    int64_t gate, g, L;
    const TS *ts;
} rraw_t;
#define NRAW 40000
static rraw_t RAW[NRAW];
static int NRAW_;

/* a segment of the run: pattern tp (index k in the project) from tick o to tick e (-1: on) */
typedef struct { const TP *tp; int k; int64_t o, e; } rseg_t;

static double R_BPM;                                    /* the run's tempo (the reference's patterns keep one) */
static int64_t R_FILL_ON = -1, R_FILL_OFF = -1;         /* FILL held from / to these samples (between blocks) */
static uint64_t R_SEED;

static double r_sec_of_tick(int64_t tick) { return (double)tick / 50400.0 * 15.0 / R_BPM; }
static int64_t r_sample_of_tick(int64_t tick)
{
    const double x = r_sec_of_tick(tick) * 44100.0;
    return x <= 0 ? 0 : (int64_t)ceil(x - 1e-7);
}
/* the sample at which the step at grid g (step length L) is resolved: the block holding g - 2L */
static int64_t r_resolve_block(int64_t g, int64_t L)
{
    const double x = r_sec_of_tick(g - 2 * L) * 44100.0;
    return x <= 0 ? 0 : (int64_t)floor(x / 32.0);
}
static int r_fill_at(int64_t g, int64_t L)
{
    const int64_t b = r_resolve_block(g, L) * 32;       /* (FILL is read when the step is resolved) */
    return R_FILL_ON >= 0 && b >= R_FILL_ON && (R_FILL_OFF < 0 || b < R_FILL_OFF);
}

static const TS *r_step_at(const TP *tp, int t, int idx)
{
    int i;
    for (i = 0; i < tp->n; i++)
        if (tp->st[i].t == t && tp->st[i].s == idx)
            return &tp->st[i];
    return 0;
}

static int r_cond_ab(int cond, int *a, int *b)          /* the table: 1:2, 2:2, 1:3, .. 8:8 from QD_COND_AB */
{
    int bb, aa, k = QD_COND_AB;
    for (bb = 2; bb <= 8; bb++)
        for (aa = 1; aa <= bb; aa++, k++)
            if (k == cond) {
                *a = aa, *b = bb;
                return 1;
            }
    return 0;
}

/* every raw event of the segments, up to tick end */
static void r_generate(const rseg_t *seg, int nseg, int64_t end)
{
    int si, t;
    NRAW_ = 0;
    for (si = 0; si < nseg; si++) {
        const TP *tp = seg[si].tp;
        int64_t hg[8][2048];                            /* the conditional steps' grids and results, per track */
        int hr[8][2048], hn[8];
        memset(hn, 0, sizeof hn);
        for (t = 0; t < 8; t++) {
            const TT *tt = &tp->trk[t];
            const int64_t L = R_STEP_T[tt->rate];
            int64_t n;
            int pre = 0;
            for (n = 0;; n++) {
                const int64_t g = seg[si].o + n * L;
                const int idx = (int)(n % tt->length);
                const int64_t pass = n / tt->length;
                const TS *s;
                int ok = 1, a, b, conditional;
                if ((seg[si].e >= 0 && g >= seg[si].e) || g >= end)
                    break;
                s = r_step_at(tp, t, idx);
                if (!s)
                    continue;
                switch (s->cond) {
                case QD_COND_ALWAYS: ok = 1; break;
                case QD_COND_FILL: ok = r_fill_at(g, L); break;
                case QD_COND_NFILL: ok = !r_fill_at(g, L); break;
                case QD_COND_PRE: ok = pre; break;
                case QD_COND_NPRE: ok = !pre; break;
                case QD_COND_NEI:
                case QD_COND_NNEI: {
                    int r = 0, k;
                    if (t > 0)
                        for (k = hn[t - 1] - 1; k >= 0; k--)
                            if (hg[t - 1][k] <= g) {
                                r = hr[t - 1][k];
                                break;
                            }
                    ok = s->cond == QD_COND_NEI ? r : !r;
                    break;
                }
                case QD_COND_1ST: ok = pass == 0; break;
                case QD_COND_N1ST: ok = pass != 0; break;
                default:
                    if (r_cond_ab(s->cond, &a, &b))
                        ok = pass % b == a - 1;
                    break;
                }
                if (ok && s->chance < 100)
                    ok = r_roll(R_SEED, seg[si].o, t, n) < s->chance;
                conditional = s->chance < 100 || s->cond == QD_COND_FILL || s->cond == QD_COND_NFILL ||
                              s->cond == QD_COND_1ST || s->cond == QD_COND_N1ST || s->cond >= QD_COND_AB;
                if (conditional) {
                    pre = ok;
                    hg[t][hn[t]] = g;
                    hr[t][hn[t]++] = ok;
                }
                if (!ok)
                    continue;
                {
                    const int64_t swing = tt->swing && (idx & 1) ? (int64_t)(tp->swing - 50) * L / 50 : 0;
                    const int64_t start = g + swing + (int64_t)s->micro * L / 24;
                    const double lenf = R_LEN[s->len];
                    const int64_t gate_all = lenf < 0 ? -1 : (int64_t)llround(lenf * (double)L);
                    int notes[4], k, h;
                    for (k = 0; k < s->nn; k++) {
                        const int x = s->notes[k] + tt->transpose;
                        notes[k] = x < 1 ? 1 : x > 127 ? 127 : x;
                    }
                    {   /* (the notes as the step keeps them: ascending) */
                        int i2, j2;
                        for (i2 = 1; i2 < s->nn; i2++)
                            for (j2 = i2; j2 > 0 && notes[j2 - 1] > notes[j2]; j2--) {
                                const int x = notes[j2];
                                notes[j2] = notes[j2 - 1];
                                notes[j2 - 1] = x;
                            }
                    }
                    if (s->type == QD_TRIG_LOCK || (s->type == QD_TRIG_TIE && tt->limit == 1)) {
                        rraw_t *r = &RAW[NRAW_++];
                        memset(r, 0, sizeof *r);
                        r->tick = start;
                        r->kind = s->type == QD_TRIG_LOCK ? QD_EV_LOCK : QD_EV_TIE;
                        r->t = t;
                        r->note = s->type == QD_TRIG_TIE ? notes[0] : 0;
                        r->vel = s->type == QD_TRIG_TIE ? s->vel : 0;
                        r->gate = gate_all;
                        r->step = idx;
                        r->g = g;
                        r->L = L;
                        r->ts = s;
                        r->pat = seg[si].k;
                        r->seq = NRAW_;
                        continue;
                    }
                    for (h = 0; h < s->ratch; h++) {
                        const int64_t slice = L / s->ratch;
                        int64_t gate;
                        int v = s->vel;
                        if (s->ratch == 1)
                            gate = gate_all;
                        else if (gate_all < 0)
                            gate = h < s->ratch - 1 ? slice : -1;
                        else if (gate_all <= L)
                            gate = gate_all / s->ratch;
                        else
                            gate = h < s->ratch - 1 ? slice : gate_all - (s->ratch - 1) * slice;
                        if (s->ratch > 1) {                 /* the ramps */
                            if (s->ramp == QD_RAMP_UP)
                                v = s->vel * (h + 1) / s->ratch;
                            else if (s->ramp == QD_RAMP_DOWN)
                                v = s->vel * (s->ratch - h) / s->ratch;
                            else if (s->ramp == QD_RAMP_ACCENT)
                                v = h ? s->vel * 5 / 8 : s->vel;
                        }
                        v = v < 1 ? 1 : v;
                        for (k = 0; k < s->nn; k++) {
                            rraw_t *r = &RAW[NRAW_++];
                            memset(r, 0, sizeof *r);
                            r->tick = start + h * slice + (int64_t)k * s->strum * L / 24;
                            r->kind = QD_EV_ON;
                            r->t = t;
                            r->note = notes[k];
                            r->vel = v;
                            r->gate = gate;
                            r->step = idx;
                            r->hit = h;
                            r->g = g;
                            r->L = L;
                            r->ts = s;
                            r->pat = seg[si].k;
                            r->seq = NRAW_;
                        }
                    }
                }
            }
        }
    }
}

/* the replay: the raw events and their OFFs in time order, the holds, the lines */
typedef struct { int note, gated; rsnd_t snd; } rvoice_t;
static rvoice_t RV[8][16];
static int RVN[8];

static void r_line(int64_t sample, int t, int kind, int note, int vel, const rsnd_t *snd, int id, int value)
{
    tl_t *e = &REF[NREF++];
    memset(e, 0, sizeof *e);
    e->sample = sample;
    e->track = t;
    e->kind = kind;
    e->note = note;
    e->vel = vel;
    e->voice = -1;
    e->id = id;
    e->value = value;
    if (snd) {
        e->hashed = 1;
        e->hash = r_fnv(snd->patch);
        own_str(&snd->at, e->own);
        mix_str(&snd->at, e->mix);
    } else {
        strcpy(e->own, "-");
        strcpy(e->mix, "-");
    }
}

typedef struct { int64_t tick; int t, note, seq; uint32_t serial; } roff_t;

static int r_before(int64_t ta, int ka, int tra, int sa, int64_t tb, int kb, int trb, int sb)
{
    if (ta != tb)
        return ta < tb;
    if (ka != kb)
        return ka < kb;
    if (tra != trb)
        return tra < trb;
    return sa < sb;
}

static int r_cmp_raw(const void *a, const void *b)
{
    const rraw_t *x = (const rraw_t *)a, *y = (const rraw_t *)b;
    return r_before(x->tick, x->kind, x->t, x->seq, y->tick, y->kind, y->t, y->seq) ? -1 : 1;
}

/* limit per track (choke the oldest), the voice list */
static void r_voice_on(int t, int note, const rsnd_t *snd, int limit)
{
    int i;
    for (i = 0; i < RVN[t]; i++)
        if (RV[t][i].note == note && !memcmp(RV[t][i].snd.patch, snd->patch, sizeof snd->patch)) {
            RV[t][i].gated = 1;                         /* struck again in place */
            RV[t][i].snd.at = snd->at;
            return;
        }
    for (i = 0; i < RVN[t]; i++)                         /* (released voices: the reference lets them go) */
        if (RV[t][i].note == note || !RV[t][i].gated) {
            memmove(&RV[t][i], &RV[t][i + 1], (size_t)(RVN[t] - i - 1) * sizeof RV[t][0]);
            RVN[t]--;
            i--;
        }
    while (RVN[t] >= limit) {
        memmove(&RV[t][0], &RV[t][1], (size_t)(RVN[t] - 1) * sizeof RV[t][0]);
        RVN[t]--;
    }
    RV[t][RVN[t]].note = note;
    RV[t][RVN[t]].gated = 1;
    RV[t][RVN[t]++].snd = *snd;
}

static void r_replay(int64_t end_sample)
{
    static roff_t off[20000];
    int noff = 0, i = 0, t, k, skip_n[8] = {0}, inf_n[8] = {0};
    static uint32_t last_on[8][128];
    uint32_t serial = 0, skip_serial[8][8], inf_serial[8][32];
    int skip_note[8][8], inf_note[8][32];
    int64_t inf_g[8];
    int oseq = 0;
    memset(last_on, 0, sizeof last_on);
    memset(RVN, 0, sizeof RVN);
    NREF = 0;
    qsort(RAW, (size_t)NRAW_, sizeof RAW[0], r_cmp_raw);
    for (;;) {
        int take_off = -1, o;
        for (o = 0; o < noff; o++)
            if (take_off < 0 || r_before(off[o].tick, 0, off[o].t, off[o].seq, off[take_off].tick, 0,
                                         off[take_off].t, off[take_off].seq))
                take_off = o;
        if (take_off >= 0 && (i >= NRAW_ || r_before(off[take_off].tick, QD_EV_OFF, off[take_off].t,
                                                     off[take_off].seq, RAW[i].tick, RAW[i].kind, RAW[i].t, -1))) {
            const roff_t f = off[take_off];
            const int64_t smp = r_sample_of_tick(f.tick);
            off[take_off] = off[--noff];
            if (smp >= end_sample)
                continue;
            {   /* a MONO track's OFF while a TIE waits (resolved, not played): held for the TIE */
                int j, held = 0;
                for (j = i; j < NRAW_ && !held; j++)
                    if (RAW[j].kind == QD_EV_TIE && RAW[j].t == f.t && RAW[j].tick >= f.tick &&
                        r_resolve_block(RAW[j].g, RAW[j].L) * 32 <= (smp / 32) * 32)
                        held = 1;
                if (held) {
                    skip_note[f.t][skip_n[f.t]] = f.note;
                    skip_serial[f.t][skip_n[f.t]++] = f.serial;
                    continue;
                }
            }
            if (last_on[f.t][f.note] == f.serial) {
                last_on[f.t][f.note] = 0;
                for (k = 0; k < RVN[f.t]; k++)
                    if (RV[f.t][k].note == f.note)
                        RV[f.t][k].gated = 0;
                r_line(smp, f.t, QD_LOG_OFF, f.note, 0, 0, -1, 0);
            } else {
                r_line(smp, f.t, QD_LOG_OFF_KEPT, f.note, 0, 0, -1, 0);
            }
            continue;
        }
        if (i >= NRAW_)
            break;
        {
            const rraw_t *r = &RAW[i++];
            const int64_t smp = r_sample_of_tick(r->tick);
            const int limit = TPAT[0].trk[r->t].limit;  /* (patch tables: TPAT[r->pat - base] would do; all the
                                                         * reference's multi-pattern runs keep the limits equal) */
            rsnd_t snd;
            if (smp >= end_sample)
                continue;
            r_sound(r->ts, r->t, &snd);
            t = r->t;
            if (r->kind == QD_EV_LOCK) {
                int any = 0;
                for (k = 0; k < RVN[t]; k++) {
                    int j;
                    if (!RV[t][k].gated)
                        continue;
                    for (j = 0; j < r->ts->nlk; j++) {
                        const int id = r->ts->lk_id[j];
                        int16_t lo, hi;
                        int v = r->ts->lk_v[j];
                        qd_lock_range(id, &lo, &hi);
                        v = v < lo ? lo : v > hi ? hi : v;
                        if (id < QD_QUAD_NP) {
                            const int routing = id == QD_QP_ALGO || id == QD_QP_FTYPE || id == QD_QP_PHRT ||
                                                id == QD_QP_LFO(0, QD_QL_WAVE) || id == QD_QP_LFO(1, QD_QL_WAVE) ||
                                                id == QD_QP_LFO(2, QD_QL_WAVE) || id == QD_QP_LFO(0, QD_QL_TRIG) ||
                                                id == QD_QP_LFO(1, QD_QL_TRIG) || id == QD_QP_LFO(2, QD_QL_TRIG);
                            const int changed = RV[t][k].snd.patch[id] != v;
                            RV[t][k].snd.patch[id] = (int16_t)v;
                            RV[t][k].snd.at.own[id / 32] |= 1u << (id % 32);
                            if (routing && changed)
                                r_line(smp, t, QD_LOG_DIP, RV[t][k].note, 0, &RV[t][k].snd, id, v);
                        } else {
                            RV[t][k].snd.at.mixm |= (uint8_t)(1u << (id - QD_QUAD_NP));
                            RV[t][k].snd.at.mixv[id - QD_QUAD_NP] = (int16_t)v;
                        }
                    }
                    r_line(smp, t, QD_LOG_LOCK, RV[t][k].note, 0, &RV[t][k].snd, -1, 0);
                    any = 1;
                }
                if (!any)
                    r_line(smp, t, QD_LOG_LOCK, 0, 0, 0, -1, 0);
                continue;
            }
            /* ON: INF notes of another step out first (a TIE takes the INF voice over: no OFF) */
            if (r->kind == QD_EV_TIE)
                inf_n[t] = 0;
            if (inf_n[t] && inf_g[t] != r->g) {
                for (k = 0; k < inf_n[t]; k++) {
                    const int nt = inf_note[t][k];
                    if (last_on[t][nt] == inf_serial[t][k]) {
                        int j;
                        last_on[t][nt] = 0;
                        for (j = 0; j < RVN[t]; j++)
                            if (RV[t][j].note == nt)
                                RV[t][j].gated = 0;
                        r_line(smp, t, QD_LOG_OFF, nt, 0, 0, -1, 0);
                    } else {
                        r_line(smp, t, QD_LOG_OFF_KEPT, nt, 0, 0, -1, 0);
                    }
                }
                inf_n[t] = 0;
            }
            serial++;
            if (r->kind == QD_EV_TIE) {
                int held = -1;
                skip_n[t] = 0;
                for (k = 0; k < RVN[t]; k++)
                    if (RV[t][k].gated)
                        held = k;
                if (held >= 0) {
                    int j;
                    rvoice_t *v = &RV[t][held];
                    last_on[t][v->note] = 0;
                    v->note = r->note;
                    for (j = 0; j < r->ts->nlk; j++) {
                        const int id = r->ts->lk_id[j];
                        if (id < QD_QUAD_NP) {
                            v->snd.patch[id] = snd.patch[id];
                            v->snd.at.own[id / 32] |= 1u << (id % 32);
                        } else {
                            v->snd.at.mixm |= (uint8_t)(1u << (id - QD_QUAD_NP));
                            v->snd.at.mixv[id - QD_QUAD_NP] = snd.at.mixv[id - QD_QUAD_NP];
                        }
                    }
                    last_on[t][r->note] = serial;
                    r_line(smp, t, QD_LOG_LEGATO, r->note, r->vel, &v->snd, -1, 0);
                } else {
                    r_voice_on(t, r->note, &snd, limit);
                    last_on[t][r->note] = serial;
                    r_line(smp, t, QD_LOG_ON, r->note, r->vel, &snd, -1, 0);
                }
            } else {
                r_voice_on(t, r->note, &snd, limit);
                last_on[t][r->note] = serial;
                r_line(smp, t, QD_LOG_ON, r->note, r->vel, &snd, -1, 0);
            }
            if (r->gate >= 0) {
                off[noff].tick = r->tick + (r->gate > 0 ? r->gate : 1);
                off[noff].t = t;
                off[noff].note = r->note;
                off[noff].serial = serial;
                off[noff++].seq = oseq++;
            } else {
                inf_g[t] = r->g;
                inf_note[t][inf_n[t]] = r->note;
                inf_serial[t][inf_n[t]++] = serial;
            }
        }
    }
    (void)skip_serial;
}

/* ================================================================ running a test pattern === */
static int32_t *AUDIO;                                  /* the mono mix of the last run (determinism) */
static size_t NAUDIO;

/* play pattern 0 of the project (built from TPAT[0]) for sec seconds; fill toggled as R_FILL_* say */
static uint64_t end_of(double sec) { return ((uint64_t)(sec * 44100.0) + 31u) / 32u * 32u; }   /* (whole blocks) */

static void run_pattern(double sec, uint64_t seed)
{
    const uint64_t end = end_of(sec);
    rig_reset();
    build(&TPAT[0], 0);
    qd_model_tempo(&MODEL, TPAT[0].bpm);
    qd_seq_seed(&SEQ, seed);
    qd_seq_select(&SEQ, 0, 0);
    qd_seq_play(&SEQ);
    NAUDIO = 0;
    while (SEQ.now < end) {
        int32_t m[32];
        if (R_FILL_ON >= 0)
            qd_seq_fill(&SEQ, (int64_t)SEQ.now >= R_FILL_ON && (R_FILL_OFF < 0 || (int64_t)SEQ.now < R_FILL_OFF));
        rig_block(m);
        if (AUDIO) {
            memcpy(AUDIO + NAUDIO, m, sizeof m);
            NAUDIO += 32;
        }
    }
}

/* the reference for pattern 0 alone over sec seconds */
static void ref_single(double sec, uint64_t seed)
{
    rseg_t seg;
    const int64_t end_tick = (int64_t)(sec / (15.0 / TPAT[0].bpm) * 50400.0) + 1;
    R_BPM = TPAT[0].bpm;
    R_SEED = seed;
    seg.tp = &TPAT[0];
    seg.k = 0;
    seg.o = 0;
    seg.e = -1;
    r_generate(&seg, 1, end_tick + 4 * 403200);
    r_replay((int64_t)end_of(sec));
    (void)end_tick;
    if (getenv("QD_DUMP_REF")) {
        int i;
        char b[1024];
        for (i = 0; i < NREF; i++) {
            line_text(&REF[i], b, 0);
            printf("REF %s\n", b);
        }
    }
}

/* a timeline test: its golden, its reference */
static void pattern_test(const char *name, const char *title, double sec, uint64_t seed)
{
    int n0 = fails;
    run_pattern(sec, seed);
    /* (the events in the last 2 steps before the end may be still to come in one or the other: both stop at sec) */
    ref_single(sec, seed);
    timeline(name, title);
    compare_ref(name, 1);
    printf("  %-4s %-16s %5d events (%u resolved, %u played), reference %d%s\n", fails == n0 ? "ok" : "FAIL", name,
           NENG, SEQ.resolved, SEQ.played, NREF, "");
}

/* ================================================================ the timelines === */
static void p_sixteenths(void)
{
    TP *p = tp_new(0, 120);
    int s;
    for (s = 0; s < 16; s++)
        S(p, 0, s, QD_TRIG_NOTE, 48 + (s % 4) * 3, 0, 0, 0)->len = 2;   /* 1/2 a step */
    pattern_test("p01-sixteenths", "track 1, a NOTE every 16th at 120 BPM, half-step gates", 4.0, 1);
}

static void p_polymeter(void)
{
    static const int RATE[8] = {QD_RATE_1, QD_RATE_3_2, QD_RATE_1_2, QD_RATE_2_3, QD_RATE_4_3, QD_RATE_2,
                                QD_RATE_1_4, QD_RATE_1_8};
    static const int LEN[8] = {16, 12, 7, 5, 9, 3, 4, 2};
    TP *p = tp_new(0, 110);
    int t, s;
    for (t = 0; t < 8; t++) {
        p->trk[t].rate = RATE[t];
        p->trk[t].length = LEN[t];
        for (s = 0; s < LEN[t]; s++)
            if (s == 0 || (s * 5 + t) % 3 == 0)
                S(p, t, s, QD_TRIG_NOTE, 40 + t * 5 + s, 0, 0, 0)->len = 3;
    }
    pattern_test("p02-polymeter", "8 tracks: rates 1, 3/2, 1/2, 2/3, 4/3, 2, 1/4, 1/8 x, lengths 16 12 7 5 9 3 4 2",
                 8.0, 1);
}

static void p_swing(void)
{
    TP *p = tp_new(0, 100);
    int s;
    p->swing = 66;
    p->trk[0].swing = 1;
    p->trk[1].swing = 0;
    p->trk[2].swing = 1;
    p->trk[2].rate = QD_RATE_1_2;
    p->trk[2].length = 7;
    for (s = 0; s < 16; s++) {
        S(p, 0, s, QD_TRIG_NOTE, 60, 0, 0, 0)->len = 1;
        if (s % 2 == 0)
            S(p, 1, s, QD_TRIG_NOTE, 36, 0, 0, 0)->len = 1;
        if (s < 7)
            S(p, 2, s, QD_TRIG_NOTE, 72 + s, 0, 0, 0)->len = 2;
    }
    pattern_test("p03-swing", "swing 66 %: track 1 on (16ths), track 2 off, track 3 on at 1/2x over 7 steps", 4.0, 1);
}

static void p_micro(void)
{
    TP *p = tp_new(0, 120);
    TS *st;
    st = S(p, 0, 0, QD_TRIG_NOTE, 36, 0, 0, 0);
    st->micro = -23;                                    /* fires in the last step of the pass before */
    S(p, 0, 4, QD_TRIG_NOTE, 38, 0, 0, 0)->micro = 12;
    S(p, 0, 8, QD_TRIG_NOTE, 40, 0, 0, 0)->micro = -6;
    S(p, 0, 12, QD_TRIG_NOTE, 41, 0, 0, 0)->micro = 23;
    p->trk[1].rate = QD_RATE_1_2;
    p->trk[1].length = 8;
    S(p, 1, 0, QD_TRIG_NOTE, 60, 0, 0, 0)->micro = -12;
    S(p, 1, 3, QD_TRIG_NOTE, 64, 0, 0, 0)->micro = 5;
    p->trk[2].length = 5;
    S(p, 2, 0, QD_TRIG_NOTE, 72, 0, 0, 0)->micro = -1;
    S(p, 2, 4, QD_TRIG_NOTE, 74, 0, 0, 0)->micro = 1;
    pattern_test("p04-micro", "micro-timing: -23/24 on step 1 (the pass before), +12, -6, +23; at 1/2x; on 5 steps",
                 4.0, 1);
}

static void p_strum(void)
{
    TP *p = tp_new(0, 120);
    TS *st;
    p->trk[0].length = 32;
    S(p, 0, 0, QD_TRIG_NOTE, 48, 52, 55, 59)->len = 6;  /* together */
    st = S(p, 0, 4, QD_TRIG_NOTE, 48, 51, 55, 58);
    st->strum = 3;
    st->len = 6;
    st = S(p, 0, 8, QD_TRIG_NOTE, 53, 57, 60, 0);
    st->strum = 8;
    st->len = 4;
    st = S(p, 0, 12, QD_TRIG_NOTE, 55, 59, 62, 65);
    st->strum = 23;
    st->micro = -5;
    st->len = 8;
    st = S(p, 0, 20, QD_TRIG_NOTE, 60, 64, 67, 0);
    st->strum = 4;
    st->ratch = 2;                                      /* the strum at every hit */
    st->len = 6;
    st = S(p, 0, 26, QD_TRIG_NOTE, 50, 62, 74, 98);     /* offsets up to +48 */
    st->strum = 6;
    pattern_test("p05-strum", "chords strummed 0, 3, 8, 23 (micro -5), with a ratchet x2, offsets to +48", 4.0, 1);
}

static void p_ratchet(void)
{
    TP *p = tp_new(0, 100);
    TS *st;
    int r, k = 0;
    p->trk[0].length = 32;
    p->trk[1].length = 32;
    for (r = 2; r <= 8; r++) {                          /* every count, the ramps in turn, at a step and less */
        st = S(p, 0, k * 4, QD_TRIG_NOTE, 60 + r, 0, 0, 0);
        st->ratch = r;
        st->ramp = (r - 2) % 4;
        st->vel = 120;
        st->len = (r & 1) ? QD_LEN_1 : 2;
        k++;
    }
    for (r = 0; r < 4; r++) {                           /* longer than a step: the last hit holds to the length */
        st = S(p, 1, r * 8, QD_TRIG_NOTE, 40 + r, 0, 0, 0);
        st->ratch = 2 + r;
        st->ramp = r;
        st->len = 8 + r;                                /* 4, 6, 8, 12 steps */
        st->vel = 110;
    }
    st = S(p, 2, 0, QD_TRIG_NOTE, 70, 0, 0, 0);         /* INF: the last hit holds until the next NOTE */
    st->ratch = 3;
    st->len = QD_LEN_INF;
    S(p, 2, 8, QD_TRIG_NOTE, 71, 0, 0, 0);
    pattern_test("p06-ratchet", "ratchets x2..x8, ramps flat / up / down / accent, gates under a step, the last hit "
                 "holding 4..12 steps, INF", 6.0, 1);
}

static void p_lengths(void)
{
    TP *p = tp_new(0, 140);
    int l;
    p->trk[0].length = 64;
    for (l = 0; l < QD_NLEN - 1; l++) {                 /* every length on its own track-1 step */
        TS *st = S(p, 0, (l * 3) % 64, QD_TRIG_NOTE, 40 + l, 0, 0, 0);
        st->len = l;
    }
    p->trk[1].length = 16;                              /* INF held until the track's next NOTE */
    S(p, 1, 0, QD_TRIG_NOTE, 50, 57, 0, 0)->len = QD_LEN_INF;
    S(p, 1, 6, QD_TRIG_NOTE, 52, 0, 0, 0)->len = QD_LEN_INF;
    S(p, 1, 9, QD_TRIG_LOCK, 0, 0, 0, 0);               /* (a LOCK step is not a NOTE: INF holds on) */
    S(p, 1, 12, QD_TRIG_NOTE, 55, 0, 0, 0)->len = 8;   /* 4 steps */
    p->trk[2].length = 8;                               /* a long note across the loop's end, and the same note
                                                         * struck again under it */
    S(p, 2, 6, QD_TRIG_NOTE, 67, 0, 0, 0)->len = 12;    /* 16 steps */
    S(p, 2, 2, QD_TRIG_NOTE, 67, 0, 0, 0)->len = 2;
    pattern_test("p07-lengths", "every length 1/8 .. 128 steps, INF until the next NOTE (a LOCK step between), a "
                 "note held across the loop's end and struck again under it", 12.0, 1);
}

static void p_conditions(void)
{
    TP *p = tp_new(0, 150);
    TS *st;
    int b, a, s = 0;
    /* track 1: FILL / NOT FILL (FILL held from bar 3 to bar 5), 1ST / NOT 1ST, PRE / NOT PRE after an A:B */
    S(p, 0, 0, QD_TRIG_NOTE, 36, 0, 0, 0)->cond = QD_COND_FILL;
    S(p, 0, 2, QD_TRIG_NOTE, 37, 0, 0, 0)->cond = QD_COND_NFILL;
    S(p, 0, 4, QD_TRIG_NOTE, 38, 0, 0, 0)->cond = QD_COND_1ST;
    S(p, 0, 6, QD_TRIG_NOTE, 39, 0, 0, 0)->cond = QD_COND_N1ST;
    S(p, 0, 8, QD_TRIG_NOTE, 40, 0, 0, 0)->cond = qd_cond_ab(1, 3);
    S(p, 0, 10, QD_TRIG_NOTE, 41, 0, 0, 0)->cond = QD_COND_PRE;
    S(p, 0, 12, QD_TRIG_NOTE, 42, 0, 0, 0)->cond = QD_COND_NPRE;
    S(p, 0, 14, QD_TRIG_NOTE, 43, 0, 0, 0)->cond = qd_cond_ab(2, 2);
    /* track 2: NEI / NOT NEI (track 1's state), at its own rate */
    p->trk[1].rate = QD_RATE_1_2;
    p->trk[1].length = 8;
    S(p, 1, 1, QD_TRIG_NOTE, 60, 0, 0, 0)->cond = QD_COND_NEI;
    S(p, 1, 3, QD_TRIG_NOTE, 62, 0, 0, 0)->cond = QD_COND_NNEI;
    S(p, 1, 5, QD_TRIG_NOTE, 64, 0, 0, 0)->cond = QD_COND_NEI;
    S(p, 1, 7, QD_TRIG_NOTE, 65, 0, 0, 0)->cond = QD_COND_NNEI;
    /* tracks 3..7: every A:B, B = 2..8 (35), on short loops so the passes come fast */
    for (b = 2; b <= 8; b++)
        for (a = 1; a <= b; a++) {
            const int t = 2 + s / 8, idx = s % 8;
            p->trk[t].length = 8;
            st = S(p, t, idx, QD_TRIG_NOTE, 48 + b * 4 + a, 0, 0, 0);
            st->cond = qd_cond_ab(a, b);
            st->len = 2;
            s++;
        }
    /* track 8: NEI of track 7 */
    p->trk[7].length = 4;
    S(p, 7, 0, QD_TRIG_NOTE, 90, 0, 0, 0)->cond = QD_COND_NEI;
    S(p, 7, 2, QD_TRIG_NOTE, 91, 0, 0, 0)->cond = QD_COND_NNEI;
    /* FILL held from step 36.5 to step 68.5 (between the steps' resolutions) */
    R_FILL_ON = (int64_t)(36.5 * 15.0 / 150.0 * 44100.0) / 32 * 32;
    R_FILL_OFF = (int64_t)(68.5 * 15.0 / 150.0 * 44100.0) / 32 * 32;
    pattern_test("p08-conditions", "FILL / NOT FILL (FILL held bars 3..5), 1ST / NOT 1ST, A:B, PRE / NOT PRE, NEI / "
                 "NOT NEI (track 2 on track 1 at 1/2x, track 8 on track 7), every A:B for B = 2..8", 16.0, 1);
    R_FILL_ON = R_FILL_OFF = -1;
}

static void p_chance(void)
{
    TP *p = tp_new(0, 130);
    TS *st;
    int s;
    for (s = 0; s < 16; s++) {
        st = S(p, 0, s, QD_TRIG_NOTE, 60 + s, 0, 0, 0);
        st->chance = (s % 5) * 25;                      /* 0, 25, 50, 75, 100 */
        st->len = 2;
    }
    st = S(p, 1, 0, QD_TRIG_NOTE, 48, 52, 55, 0);       /* a chord with a ratchet: all of it or none */
    st->chance = 50;
    st->ratch = 3;
    S(p, 1, 4, QD_TRIG_NOTE, 40, 0, 0, 0)->cond = QD_COND_PRE;   /* PRE after a chance */
    S(p, 1, 6, QD_TRIG_NOTE, 41, 0, 0, 0)->cond = QD_COND_NPRE;
    st = S(p, 1, 8, QD_TRIG_NOTE, 43, 0, 0, 0);
    st->chance = 50;
    st->cond = qd_cond_ab(1, 2);
    pattern_test("p09-chance", "chance 0 / 25 / 50 / 75 / 100 % (seed 12345), a chord and ratchet together, PRE after "
                 "a chance, A:B then chance", 8.0, 12345);
}

static void p_soundlocks(void)
{
    TP *p = tp_new(0, 120);
    TS *st;
    int s;
    p->trk[0].limit = 4;
    for (s = 0; s < 16; s++) {
        static const int SND[16] = {14, 0, 17, 0, 3, 0, 17, 14, 14, 0, 17, 0, 3, 17, 17, 0};
        st = S(p, 0, s, QD_TRIG_NOTE, s % 4 == 0 ? 36 : 60, 0, 0, 0);
        st->sound = SND[s];                             /* MARIMBA's own (0), NOISE-ISH 17, METAL 10.. as in SND */
        st->len = 2;
    }
    st = S(p, 1, 0, QD_TRIG_NOTE, 48, 0, 0, 0);         /* the same note, the same sound: struck again in place */
    st->sound = 4;
    st->len = 8;
    st = S(p, 1, 2, QD_TRIG_NOTE, 48, 0, 0, 0);
    st->sound = 4;
    st = S(p, 1, 4, QD_TRIG_NOTE, 48, 0, 0, 0);         /* .. another sound: a fresh voice, the old one released */
    st->sound = 5;
    st->len = 6;
    pattern_test("p10-soundlocks", "sound locks per step on the drum track (presets by number, none = the track's), "
                 "the same note again with the same sound and with another", 4.0, 1);
}

static void p_plocks(void)
{
    TP *p = tp_new(0, 120);
    TS *st;
    st = S(p, 0, 0, QD_TRIG_NOTE, 48, 0, 0, 0);
    LK(st, QD_QP_FREQ, 4000);
    LK(st, QD_QP_EDEC, 20);
    st = S(p, 0, 4, QD_TRIG_NOTE, 48, 0, 0, 0);         /* a sound lock and p-locks: the p-locks win */
    st->sound = 9;
    LK(st, QD_QP_HARM, -1200);
    LK(st, QD_QP_LEVEL, 80);
    st = S(p, 0, 8, QD_TRIG_NOTE, 55, 0, 0, 0);         /* the mix: pan, the three sends, the level */
    LK(st, QD_LK_PAN, -40);
    LK(st, QD_LK_DELAY, 60);
    LK(st, QD_LK_REVERB, 90);
    LK(st, QD_LK_CHORUS, 30);
    LK(st, QD_LK_LEVEL, 70);
    st = S(p, 0, 12, QD_TRIG_NOTE, 60, 64, 0, 0);       /* a chord with an ALGO and FTYPE lock: from the trig, no dip */
    LK(st, QD_QP_ALGO, 5);
    LK(st, QD_QP_FTYPE, QD_QF_HP12);
    LK(st, QD_QP_FREQ, 99999);                          /* (clamped to 127.00) */
    st = S(p, 0, 14, QD_TRIG_NOTE, 62, 0, 0, 0);        /* LFO locks */
    LK(st, QD_QP_LFO(0, QD_QL_DEPTH), 3000);
    LK(st, QD_QP_LFO(0, QD_QL_DEST), 25);
    pattern_test("p11-plocks", "p-locks: FREQ and AMP DEC; a sound lock with HARM and LEVEL over it; the mix (pan, "
                 "delay, reverb, chorus, level); ALGO and TYPE on a chord; an LFO", 4.0, 1);
}

static void p_lockstep(void)
{
    TP *p = tp_new(0, 120);
    TS *st;
    p->trk[2].length = 32;
    st = S(p, 2, 0, QD_TRIG_NOTE, 60, 64, 67, 0);       /* KEYS (EP): a chord held 8 steps */
    st->len = 10;
    st = S(p, 2, 2, QD_TRIG_LOCK, 0, 0, 0, 0);          /* trigless: FREQ and AMP DEC into the held voices */
    LK(st, QD_QP_FTYPE, QD_QF_LP12);
    LK(st, QD_QP_FREQ, 6000);
    LK(st, QD_QP_EDEC, 60);
    st = S(p, 2, 4, QD_TRIG_LOCK, 0, 0, 0, 0);          /* ALGO and the filter TYPE: the dip */
    LK(st, QD_QP_ALGO, 7);
    LK(st, QD_QP_FTYPE, QD_QF_HP12);
    st = S(p, 2, 5, QD_TRIG_LOCK, 0, 0, 0, 0);          /* an LFO's WAVE and TRIG, the pan */
    LK(st, QD_QP_LFO(1, QD_QL_WAVE), 3);
    LK(st, QD_QP_LFO(1, QD_QL_TRIG), 1);
    LK(st, QD_LK_PAN, 30);
    st = S(p, 2, 6, QD_TRIG_LOCK, 0, 0, 0, 0);          /* the same ALGO again: no change, no dip */
    LK(st, QD_QP_ALGO, 7);
    st = S(p, 2, 14, QD_TRIG_LOCK, 0, 0, 0, 0);         /* nothing sounding: nothing to lock */
    LK(st, QD_QP_FREQ, 3000);
    st = S(p, 2, 16, QD_TRIG_NOTE, 62, 65, 0, 0);       /* a new chord: the track's sound again (no own values) */
    st->len = 9;                                        /* 6 steps */
    st = S(p, 2, 18, QD_TRIG_LOCK, 0, 0, 0, 0);
    LK(st, QD_QP_PHRT, 0);
    pattern_test("p12-lockstep", "LOCK (trigless) steps into a held chord: FREQ, AMP DEC; ALGO and TYPE (dips); an "
                 "LFO's WAVE and TRIG (dips), PAN; the same ALGO (no dip); none sounding; PHRT", 4.0, 1);
}

static void p_tie(void)
{
    TP *p = tp_new(0, 110);
    TS *st;
    p->trk[1].limit = 1;                                /* BASS: MONO */
    st = S(p, 1, 0, QD_TRIG_NOTE, 36, 0, 0, 0);
    st->len = 2;
    S(p, 1, 1, QD_TRIG_TIE, 43, 0, 0, 0)->len = 2;      /* legato */
    st = S(p, 1, 2, QD_TRIG_TIE, 41, 0, 0, 0);          /* legato, with a lock and a higher velocity */
    st->vel = 120;
    st->len = 4;
    LK(st, QD_QP_FREQ, 5000);
    S(p, 1, 4, QD_TRIG_NOTE, 38, 0, 0, 0)->len = 1;     /* a NOTE: a new attack (the choke) */
    S(p, 1, 8, QD_TRIG_TIE, 40, 0, 0, 0)->len = 2;      /* nothing held (released 3 steps before): a NOTE */
    st = S(p, 1, 12, QD_TRIG_NOTE, 36, 0, 0, 0);
    st->len = QD_LEN_INF;
    S(p, 1, 14, QD_TRIG_TIE, 48, 0, 0, 0)->len = 1;     /* a TIE out of an INF note */
    p->trk[3].limit = 4;                                /* a POLY track: TIE is a NOTE */
    S(p, 3, 0, QD_TRIG_NOTE, 72, 0, 0, 0)->len = 2;
    S(p, 3, 1, QD_TRIG_TIE, 74, 0, 0, 0)->len = 2;
    pattern_test("p13-tie", "TIE on a MONO bass: legato (the gate held into it), with a lock; a NOTE after; a TIE "
                 "with nothing held; out of INF; TIE on a POLY track", 4.0, 1);
}

/* p14: pattern change queued and immediate, and a chain (three patterns, looped) */
static void p_change(void)
{
    TP *a = tp_new(0, 120), *b, *c;
    int s;
    rseg_t seg[16];
    int nseg = 0, n0 = fails;
    int64_t x;
    for (s = 0; s < 16; s += 2)
        S(a, 0, s, QD_TRIG_NOTE, 48, 0, 0, 0)->len = 2;
    S(a, 1, 15, QD_TRIG_NOTE, 60, 0, 0, 0)->micro = 0;
    b = tp_new(1, 120);
    b->trk[0].length = 12;                              /* master: 12 steps (the longest track: 16 at 1x is the
                                                         * default; this pattern's tracks are all 12 long) */
    for (s = 0; s < 8; s++)
        b->trk[s].length = 12;
    for (s = 0; s < 12; s += 3)
        S(b, 0, s, QD_TRIG_NOTE, 55, 0, 0, 0)->len = 2;
    S(b, 1, 0, QD_TRIG_NOTE, 67, 0, 0, 0)->micro = -12;   /* a negative micro on step 1 of the incoming pattern */
    c = tp_new(2, 120);
    c->master = 8;                                      /* master 8 sixteenths */
    for (s = 0; s < 8; s++)
        S(c, 0, s, QD_TRIG_NOTE, 72 + s, 0, 0, 0)->len = 1;
    rig_reset();
    build(a, 0);
    build(b, 1);
    build(c, 2);
    qd_model_tempo(&MODEL, 120);
    qd_seq_seed(&SEQ, 1);
    qd_seq_select(&SEQ, 0, 0);
    qd_seq_play(&SEQ);
    /* queue B at 0.6 s (in A's first pass: it takes over at 16 steps = 2 s); then C immediately at 2.9 s (the
     * next 16th); then the chain A, B, C from 4.3 s (C's master is 8: the chain starts at its next boundary) */
    {
        const uint64_t q1 = 26432, q2 = 127904, q3 = 189632;   /* (block-aligned: 0.599, 2.900, 4.300 s) */
        int chain[3] = {0, 1, 2};
        while (SEQ.now < 12 * 44100) {
            if (SEQ.now == q1)
                qd_seq_select(&SEQ, 1, 0);
            if (SEQ.now == q2)
                qd_seq_select(&SEQ, 2, 1);
            if (SEQ.now == q3)
                qd_seq_chain(&SEQ, chain, 3);
            rig_block(0);
        }
        /* the reference's segments: A from 0; B from 32 sixteenths? no: A's master is 16 steps (16 x 50400) */
        {
            const int64_t TA = 16 * 50400, TB = 12 * 50400, TC = 8 * 50400;
            const int64_t t2 = (int64_t)((q2 * 8 * 120000 + 104999) / 105000);   /* the tick at q2 (ceil) */
            const int64_t t3 = (int64_t)((q3 * 8 * 120000 + 104999) / 105000);
            int64_t o, cx;
            int k;
            seg[nseg++] = (rseg_t){a, 0, 0, TA};        /* A until its master */
            x = (t2 + 50399) / 50400 * 50400;           /* C immediately: the next 16th after q2 */
            seg[nseg++] = (rseg_t){b, 1, TA, x};
            o = x;
            cx = o + ((t3 - o) / TC + 1) * TC;          /* the chain from C's next boundary */
            seg[nseg++] = (rseg_t){c, 2, o, cx};
            for (k = 0; nseg < 16; k++) {
                const TP *tp = k % 3 == 0 ? a : k % 3 == 1 ? b : c;
                const int64_t m = k % 3 == 0 ? TA : k % 3 == 1 ? TB : TC;
                seg[nseg++] = (rseg_t){tp, k % 3, cx, cx + m};
                cx += m;
            }
        }
    }
    R_BPM = 120;
    R_SEED = 1;
    r_generate(seg, nseg, (int64_t)14 * 8 * 50400);
    r_replay(12 * 44100);
    timeline("p14-change", "A (16 steps) plays; B queued in A's first pass takes over at its end; C immediately (the "
             "next 16th); then the chain A, B, C looped (masters 16, 12, 8)");
    compare_ref("p14-change", 1);
    printf("  %-4s %-16s %5d events, reference %d\n", fails == n0 ? "ok" : "FAIL", "p14-change", NENG, NREF);
}

/* p15: MIDI clock in, 120 BPM, then 140, then 100: the 16ths land on their pulses */
static void p_clock(void)
{
    TP *p = tp_new(0, 120);
    int s, n0 = fails, k, nk = 0;
    static int64_t pulse[400];
    double tsec = 0.2;
    for (s = 0; s < 16; s++)
        S(p, 0, s, QD_TRIG_NOTE, 48 + s, 0, 0, 0)->len = 2;
    for (k = 0; k < 384; k++) {                         /* 2 bars at 120, 1 at 140, 1 at 100 */
        const double bpm = k < 192 ? 120.0 : k < 288 ? 140.0 : 100.0;
        pulse[nk++] = (int64_t)floor(tsec * 44100.0 / 32.0 + 0.5) * 32;
        tsec += 60.0 / bpm / 24.0;
    }
    rig_reset();
    build(p, 0);
    qd_model_tempo(&MODEL, 120);
    qd_seq_select(&SEQ, 0, 0);
    {
        int pk = 0, b140 = 0, b100 = 0;
        qd_seq_midi_start(&SEQ);
        while (pk < nk) {
            while (pk < nk && (int64_t)SEQ.now == pulse[pk]) {
                qd_seq_midi_clock(&SEQ);
                pk++;
            }
            if (pk == 280)
                b140 = qd_seq_bpm(&SEQ);
            if (pk == 383)
                b100 = qd_seq_bpm(&SEQ);
            rig_block(0);
        }
        CHECK(b140 == 140 && b100 == 100, "clock: the tempo read %d at 140, %d at 100", b140, b100);
    }
    /* the reference: 16th n ON at pulse 6n, its OFF (a half step) at pulse 6n + 3 */
    NREF = 0;
    for (k = 0; k + 3 < nk; k += 6) {
        const int n = k / 6;
        rsnd_t snd;
        r_sound(&p->st[n % 16], 0, &snd);
        r_line(pulse[k], 0, QD_LOG_ON, 48 + n % 16, 100, &snd, -1, 0);
        r_line(pulse[k + 3], 0, QD_LOG_OFF, 48 + n % 16, 0, 0, -1, 0);
    }
    timeline("p15-midiclock", "MIDI clock in: START, 24 PPQN at 120 BPM (2 bars), 140 (1 bar), 100 (1 bar); pulses on "
             "block edges; a 16th every 6 pulses");
    compare_ref("p15-midiclock", 0);
    printf("  %-4s %-16s %5d events, reference %d (every 16th on its pulse, to the sample)\n",
           fails == n0 ? "ok" : "FAIL", "p15-midiclock", NENG, NREF);
}

/* p16: the four sounds of the drum track's sound locks layered with p-locks, at 2x over 3 steps, muted midway */
static void p_mix(void)
{
    TP *p = tp_new(0, 96);
    TS *st;
    p->trk[0].rate = QD_RATE_2;
    p->trk[0].length = 3;
    st = S(p, 0, 0, QD_TRIG_NOTE, 36, 0, 0, 0);
    st->sound = 4;
    LK(st, QD_QP_EDEC, 10);
    LK(st, QD_LK_REVERB, 0);
    st = S(p, 0, 1, QD_TRIG_NOTE, 84, 0, 0, 0);
    st->sound = 17;
    LK(st, QD_QP_FREQ, 9000);
    LK(st, QD_LK_PAN, 25);
    p->trk[4].transpose = 12;                           /* transpose */
    p->trk[4].rate = QD_RATE_3_4;
    p->trk[4].length = 5;
    S(p, 4, 0, QD_TRIG_NOTE, 60, 0, 0, 0);
    S(p, 4, 3, QD_TRIG_NOTE, 120, 0, 0, 0);             /* (clamped to 127 by the transpose) */
    pattern_test("p16-rates-xpose", "track 1 at 2x over 3 steps with sound locks and p-locks; track 5 at 3/4x over "
                 "5 steps transposed +12 (a note clamped at 127)", 3.0, 1);
}

/* ================================================================ units === */
static void u_lockpool(void)
{
    static qd_pattern_t P, Q;
    int t, s, k, r, n0 = fails;
    int16_t v;
    qd_pat_init(&P);
    CHECK(qd_pat_check(&P), "empty pattern consistent");
    /* locks in random order on several tracks: sorted by track, step, id */
    for (k = 0; k < 600; k++) {
        t = (k * 7) % 8, s = (k * 13) % 128;
        r = qd_lock_set(&P, t, s, (k * 31) % QD_LK_N, k);
        CHECK(r == QD_OK, "lock_set %d: %d", k, r);
    }
    CHECK(qd_pat_check(&P), "600 locks: sorted, counted, flags right");
    CHECK(qd_lock_get(&P, (599 * 7) % 8, (599 * 13) % 128, (599 * 31) % QD_LK_N, &v) && v == 599,
          "lock_get finds the last");
    /* the same lock again: replaced, not added */
    {
        const int had = qd_lock_get(&P, 0, 0, 34, 0);
        k = P.nlock;
        qd_lock_set(&P, 0, 0, 34, 5000);
        qd_lock_set(&P, 0, 0, 34, 6000);
        CHECK(P.nlock == k + !had, "a lock set twice is one");
        CHECK(qd_lock_get(&P, 0, 0, 34, &v) && v == 6000, "the newer value (%d)", v);
    }
    /* fill the pool: one more is refused, nothing changes */
    for (t = 0; P.nlock < QD_NLOCK; t++)
        qd_lock_set(&P, t % 8, (t / 8) % 128, 72 - (t / 1024) % 70, t);
    CHECK(P.nlock == QD_NLOCK, "the pool full: %d", P.nlock);
    {
        int id = 0, tt = 0, ss = 0, found = 0;
        for (tt = 0; tt < 8 && !found; tt++)
            for (ss = 0; ss < 128 && !found; ss++)
                for (id = 0; id < QD_LK_N && !found; id++)
                    if (!qd_lock_get(&P, tt, ss, id, 0))
                        found = 1;
        memcpy(&Q, &P, sizeof Q);
        r = qd_lock_set(&P, tt - 1, ss - 1, id - 1, 1);
        CHECK(found && r == QD_EFULL && !memcmp(&P, &Q, sizeof P), "full: QD_EFULL (%d), the pattern unchanged", r);
    }
    /* a step copy onto an empty step of a full pool: refused, unchanged */
    s = -1;
    for (k = 0; k < 128 && s < 0; k++)
        if (!qd_lock_count(&P, 5, k))
            s = k;
    if (s >= 0) {
        memcpy(&Q, &P, sizeof Q);
        r = qd_step_copy(&P, 5, s, &P, 0, 0);
        CHECK(r == QD_EFULL && !memcmp(&P, &Q, sizeof P), "full: a copy with locks refused (%d)", r);
    }
    /* clears give room back and stay consistent */
    qd_track_clear(&P, 2);
    CHECK(qd_pat_check(&P) && P.lock_n[2] == 0, "track clear: no locks left on track 3");
    qd_page_clear(&P, 0, 0);
    CHECK(qd_pat_check(&P), "page clear consistent");
    for (s = 0; s < 16; s++)
        CHECK(!qd_lock_count(&P, 0, s), "page clear: step %d has no locks", s);
    qd_step_clear(&P, 1, (1 * 13) % 128);
    CHECK(qd_pat_check(&P), "step clear consistent");
    /* copies: a step, a page, a track, between patterns, with their locks */
    qd_pat_init(&P);
    qd_pat_init(&Q);
    qd_step_set_trig(&P, 0, 3, QD_TRIG_NOTE);
    qd_lock_set(&P, 0, 3, QD_QP_FREQ, 4000);
    qd_lock_set(&P, 0, 3, QD_LK_PAN, -10);
    qd_step_set_trig(&P, 0, 9, QD_TRIG_LOCK);
    qd_lock_set(&P, 0, 9, QD_QP_ALGO, 3);
    r = qd_step_copy(&Q, 4, 100, &P, 0, 3);
    CHECK(r == QD_OK && qd_lock_count(&Q, 4, 100) == 2 && qd_pat_check(&Q), "step copy with its two locks");
    r = qd_page_copy(&Q, 6, 7, &P, 0, 0);
    CHECK(r == QD_OK && qd_lock_count(&Q, 6, 115) == 2 && qd_lock_count(&Q, 6, 121) == 1 && qd_pat_check(&Q),
          "page copy: steps 3 and 9 of page 1 onto page 8");
    CHECK(qd_step_type(qd_step(&Q, 6, 121)) == QD_TRIG_LOCK, "page copy keeps the trig type");
    r = qd_track_copy(&Q, 1, &P, 0);
    CHECK(r == QD_OK && Q.lock_n[1] == 3 && qd_pat_check(&Q), "track copy: 3 locks");
    r = qd_page_copy(&P, 0, 0, &P, 0, 0);                /* onto itself */
    CHECK(r == QD_OK && qd_pat_check(&P) && P.nlock == 3, "page copy onto itself");
    r = qd_page_copy(&P, 0, 1, &P, 0, 0);                /* overlapping pattern: page 1 to page 2 */
    CHECK(r == QD_OK && qd_pat_check(&P) && P.nlock == 6, "page copy within a track: 6 locks");
    qd_lock_clear(&P, 0, 3, QD_QP_FREQ);
    CHECK(qd_lock_count(&P, 0, 3) == 1 && qd_pat_check(&P), "lock clear: one left");
    qd_lock_clear(&P, 0, 3, QD_LK_PAN);
    CHECK(!(qd_step(&P, 0, 3)->flags & QD_SF_LOCKS), "lock clear: the step's flag goes");
    printf("  %-4s lock pool         sorted / counted / flags, replace, full (QD_EFULL, nothing changed), clears, "
           "copies\n", fails == n0 ? "ok" : "FAIL");
}

static void u_edit(void)
{
    static qd_pattern_t P;
    const qd_step_t *st;
    uint8_t n[4];
    int r, n0 = fails, a, b, c;
    char buf[8];
    qd_pat_init(&P);
    P.trk[2].def_note = 50;
    P.trk[2].def_vel = 90;
    P.trk[2].def_len = 6;
    CHECK(sizeof(qd_step_t) == 12 && sizeof(qd_lock_t) == 4, "step 12 bytes (%d), lock 4 (%d)",
          (int)sizeof(qd_step_t), (int)sizeof(qd_lock_t));
    r = qd_step_set_trig(&P, 2, 5, QD_TRIG_NOTE);
    st = qd_step(&P, 2, 5);
    CHECK(r == QD_OK && st->note[0] == 50 && st->vel == 90 && st->len == 6 && st->chance == 100,
          "a new trig takes the track's defaults");
    {
        const uint8_t ch[4] = {67, 60, 64, 60};
        r = qd_step_set_notes(&P, 2, 5, ch, 4);
    }
    CHECK(r == QD_OK && qd_step_notes(st, n) == 3 && n[0] == 60 && n[1] == 64 && n[2] == 67 && st->note[1] == 4,
          "notes: ascending, no repeats, 2..4 as offsets");
    {
        const uint8_t wide[2] = {30, 79};
        CHECK(qd_step_set_notes(&P, 2, 5, wide, 2) == QD_ENOTE, "an offset over +48: refused");
    }
    CHECK(qd_step_set_ratchet(&P, 2, 5, 8, QD_RAMP_ACCENT) == QD_OK && qd_step_ratchet(st) == 8 &&
          qd_step_ramp(st) == QD_RAMP_ACCENT && qd_step_type(st) == QD_TRIG_NOTE, "ratchet x8, accent");
    CHECK(qd_step_set_micro(&P, 2, 5, -24) == QD_EARG && qd_step_set_micro(&P, 2, 5, -23) == QD_OK &&
          st->micro == -23, "micro -23..23");
    CHECK(qd_step_set_strum(&P, 2, 5, 24) == QD_EARG && qd_step_set_strum(&P, 2, 5, 23) == QD_OK, "strum 0..23");
    CHECK(qd_step_set_chance(&P, 2, 5, 101) == QD_EARG && qd_step_set_chance(&P, 2, 5, 0) == QD_OK, "chance");
    CHECK(qd_step_set_sound(&P, 2, 5, 18) == QD_EARG && qd_step_set_sound(&P, 2, 5, 17) == QD_OK, "sound 0..17");
    CHECK(qd_step_set_len(&P, 2, 5, QD_LEN_INF) == QD_OK && qd_len_eighths(QD_LEN_INF) < 0, "INF");
    CHECK(qd_step_set_trig(&P, 2, 5, QD_TRIG_TIE) == QD_OK && qd_step_type(st) == QD_TRIG_TIE &&
          qd_step_ratchet(st) == 8, "the type changes, the rest stays");
    CHECK(qd_step_set_vel(&P, 9, 5, 100) == QD_EARG && qd_step_set_vel(&P, 2, 128, 100) == QD_EARG, "out of range");
    qd_lock_set(&P, 2, 5, QD_QP_FREQ, 1);
    CHECK(qd_step_set_trig(&P, 2, 5, QD_TRIG_NONE) == QD_OK && !qd_lock_count(&P, 2, 5) && !st->flags &&
          !st->note[0], "NONE: the step and its locks gone");
    CHECK(qd_lock_set(&P, 2, 6, QD_QP_FREQ, 3000) == QD_OK && qd_step_type(qd_step(&P, 2, 6)) == QD_TRIG_LOCK,
          "a lock on an empty step makes a LOCK step");
    CHECK(qd_lock_set(&P, 2, 6, QD_LK_N, 0) == QD_EARG, "lock id out of range");
    {
        int16_t v;
        qd_lock_set(&P, 2, 6, QD_LK_PAN, -100);
        CHECK(qd_lock_get(&P, 2, 6, QD_LK_PAN, &v) && v == -64, "a lock is clamped to its range (%d)", v);
    }
    /* the conditions' table */
    c = 0;
    for (b = 2; b <= 8; b++)
        for (a = 1; a <= b; a++) {
            int a2, b2;
            const int k = qd_cond_ab(a, b);
            c += k >= QD_COND_AB && k < QD_NCOND && qd_cond_ab_of(k, &a2, &b2) && a2 == a && b2 == b;
        }
    CHECK(c == 35 && qd_cond_ab(1, 2) == QD_COND_AB && qd_cond_ab(8, 8) == QD_NCOND - 1 && qd_cond_ab(3, 2) < 0,
          "A:B: 35 conditions, 1:2 first, 8:8 last (%d)", c);
    CHECK(!strcmp(qd_cond_name(qd_cond_ab(3, 4), buf), "3:4") && !strcmp(qd_cond_name(QD_COND_NNEI, buf), "!NEI"),
          "condition names");
    CHECK(qd_rate_ticks(QD_RATE_3_4) == 67200 && qd_rate_ticks(QD_RATE_4_3) == 37800 &&
          qd_rate_ticks(QD_RATE_2_3) == 75600 && qd_rate_ticks(QD_RATE_1_8) == 403200, "rates in ticks");
    {
        const int64_t m0 = qd_pat_master_ticks(&P);
        P.trk[4].length = 64;
        P.trk[4].rate = QD_RATE_1_2;
        CHECK(m0 == 16 * QD_TPS && qd_pat_master_ticks(&P) == 128 * QD_TPS, "master auto: the longest track");
        P.master = 32;
        CHECK(qd_pat_master_ticks(&P) == 32 * QD_TPS, "master 32");
        P.master = QD_MASTER_INF;
        CHECK(qd_pat_master_ticks(&P) == 0, "master INF");
    }
    printf("  %-4s editing API       defaults, notes as offsets, every field's range, NONE, LOCK by a lock, 44 "
           "conditions, rates, master\n", fails == n0 ? "ok" : "FAIL");
}

/* the patch of a trig: the sound lock's preset, the p-locks over it; the own mask; the mix locks */
static void u_patch(void)
{
    int16_t want[QD_QUAD_NP];
    int i, n0 = fails, v = -1;
    TP *p = tp_new(0, 120);
    TS *st = S(p, 0, 0, QD_TRIG_NOTE, 50, 0, 0, 0);
    st->sound = 7;                                      /* GLASS PAD (preset 6) */
    LK(st, QD_QP_FREQ, 4321);
    LK(st, QD_QP_ADEC, 12);
    LK(st, QD_LK_REVERB, 99);
    st = S(p, 0, 2, QD_TRIG_NOTE, 52, 0, 0, 0);         /* no sound lock: the track's sound and two locks */
    LK(st, QD_QP_HARM, 750);
    LK(st, QD_QP_MIX, 10);
    run_pattern(0.3, 1);
    qd_quad_preset_patch(6, want);
    want[QD_QP_FREQ] = 4321;
    want[QD_QP_ADEC] = 12;
    for (i = 0; i < NENG && ENG[i].kind != QD_LOG_ON; i++)
        ;
    if (i < NENG) {
        v = ENG[i].voice;
        CHECK(!memcmp(POOL.v[v].q.patch, want, sizeof want), "sound lock + p-locks = GLASS PAD with FREQ 43.21, A DEC 12");
        CHECK(!strcmp(ENG[i].own, "all") && !strcmp(ENG[i].mix, "r99"), "own all, reverb 99 (%s, %s)", ENG[i].own,
              ENG[i].mix);
    }
    CHECK(v >= 0, "the trig sounded");
    rig_run_to((uint64_t)(0.3 * 44100) + 11025 / 2);
    memcpy(want, MODEL.t[0].patch, sizeof want);
    want[QD_QP_HARM] = 750;
    want[QD_QP_MIX] = 10;
    for (i = 0, v = -1; i < NENG; i++)
        if (ENG[i].kind == QD_LOG_ON && ENG[i].note == 52)
            v = ENG[i].voice;
    CHECK(v >= 0 && !memcmp(POOL.v[v].q.patch, want, sizeof want), "the track's sound + 2 locks");
    CHECK(v >= 0 && POOL.v[v].at.own[0] == ((1u << QD_QP_HARM) | (1u << QD_QP_MIX)) && !POOL.v[v].at.own[1] &&
          !POOL.v[v].at.own[2], "own: HARM and MIX");
    /* a live edit reaches the voice's other values and not its own; a waiting trig too */
    if (v >= 0) {
        qd_player_live_edit(&PL, 0, QD_QP_FREQ, 2222);
        qd_player_live_edit(&PL, 0, QD_QP_HARM, -500);
        CHECK(POOL.v[v].q.patch[QD_QP_FREQ] == 2222 && POOL.v[v].q.patch[QD_QP_HARM] == 750,
              "live: FREQ reaches the voice, its own HARM stays");
        CHECK(MODEL.t[0].patch[QD_QP_HARM] == -500, "live: the track's sound changed");
    }
    printf("  %-4s patch resolution  sound lock + p-locks = the patch built by hand; the track's sound + locks; own "
           "masks; live edits pass own values by\n", fails == n0 ? "ok" : "FAIL");
}

/* the dip: a routing change on a sounding voice, with and without it. The measure: the largest step between two
 * samples in the four blocks from the change, against the held tone's own largest step before and after the change
 * and its peak (the dip's ramp to 0 and back over 32 samples each steps by peak / 32 at most, on top of the tone's) */
static double dip_measure(int on, int preset, int note, int id, int from, int to, double *base_out, double *peak_out,
                          int both)
{
    static qd_pool_t P;
    static qd_quad_owner_t O;
    static int16_t patch[QD_QUAD_NP];
    int32_t out[32], prev = 0;
    double worst = 0, base = 0, peak = 0;
    int b, i;
    qd_pool_init(&P, 4);
    P.dip_on = on;
    qd_quad_owner_init(&O);
    qd_quad_preset_patch(preset, patch);
    patch[QD_QP_EATK] = 0;
    patch[QD_QP_EDEC] = 127;                            /* (held: AMP DEC INF) */
    patch[QD_QP_ADEC] = patch[QD_QP_BDEC] = 127;
    patch[QD_QP_AEND] = patch[QD_QP_BEND] = 127;
    patch[id] = (int16_t)from;
    if (both) {                                         /* X and Y both heard, B1 at full: the carriers move */
        patch[QD_QP_MIX] = 0;
        patch[QD_QP_BLEV] = 43;
        patch[QD_QP_ALEV] = 0;
    }
    if (id == QD_QP_FTYPE)
        patch[QD_QP_FREQ] = 4000;
    O.quad_patch = patch;
    qd_pool_note_on(&P, 0, &O, note, 110, patch);
    for (b = 0; b < 400; b++) {
        if (b == 300)
            qd_pool_write(&P, 0, id, to, 0);
        memset(out, 0, sizeof out);
        qd_pool_block_begin(&P);
        qd_quad_owner_block(&O);
        qd_pool_render(&P, 0, out);
        for (i = 0; i < 32; i++) {
            const double d = fabs((double)out[i] - (double)prev);
            prev = out[i];
            if ((b >= 200 && b < 300) || (b >= 330 && b < 400)) {
                base = d > base ? d : base;             /* the held tone's own largest step, old and new */
                peak = fabs((double)out[i]) > peak ? fabs((double)out[i]) : peak;
            }
            if (b >= 300 && b < 304)
                worst = d > worst ? d : worst;          /* around the change */
        }
    }
    *base_out = base;
    *peak_out = peak;
    return worst;
}

static void u_dip(void)
{
    static const struct { int preset, note, id, from, to, both; const char *what; } C[] = {
        {0, 45, QD_QP_FTYPE, QD_QF_LP12, QD_QF_HP12, 0, "INIT A2, TYPE LP12 -> HP12"},
        {0, 45, QD_QP_FTYPE, QD_QF_HP12, QD_QF_LP24, 0, "INIT A2, TYPE HP12 -> LP24"},
        {0, 45, QD_QP_ALGO, 1, 3, 1, "INIT+B1 A2, ALGO 1 -> 3"},
        {0, 45, QD_QP_ALGO, 1, 5, 1, "INIT+B1 A2, ALGO 1 -> 5"},
        {0, 45, QD_QP_ALGO, 1, 8, 1, "INIT+B1 A2, ALGO 1 -> 8"},
        {2, 45, QD_QP_ALGO, 2, 4, 0, "BELL A2, ALGO 2 -> 4"},
        {1, 57, QD_QP_ALGO, 2, 6, 0, "EP A3, ALGO 2 -> 6"},
        {7, 45, QD_QP_PHRT, 1, 0, 0, "HOLLOW A2, PHRT ALL -> OFF"},
    };
    unsigned k;
    int n0 = fails;
    for (k = 0; k < sizeof C / sizeof C[0]; k++) {
        double b0, b1, p0, p1;
        const double w0 = dip_measure(0, C[k].preset, C[k].note, C[k].id, C[k].from, C[k].to, &b0, &p0, C[k].both);
        const double w1 = dip_measure(1, C[k].preset, C[k].note, C[k].id, C[k].from, C[k].to, &b1, &p1, C[k].both);
        const double lim = (b0 + p0 / 32.0) * 1.1 + 2.0;
        printf("       dip  %-28s the tones (before, after): largest step %6.0f, peak %6.0f; at the change: without "
               "the dip %6.0f, with it %6.0f\n", C[k].what, b0, p0, w0, w1);
        CHECK(w1 <= lim, "%s: with the dip the change steps no more than the tone and the ramp (%.0f vs %.0f)",
              C[k].what, w1, lim);
        if (w0 > lim)                                   /* (a change that steps: the dip takes it away) */
            CHECK(w1 < w0, "%s: the dip makes the step smaller (%.0f vs %.0f)", C[k].what, w1, w0);
    }
    printf("  %-4s the dip           a routing change on a sounding voice: no step beyond the tone's own and the "
           "ramp's\n", fails == n0 ? "ok" : "FAIL");
}

static void u_determinism(void)
{
    static int32_t a[44100 * 4 + 64], b[44100 * 4 + 64];
    int n0 = fails;
    size_t na;
    TP *p = tp_new(0, 128);
    TS *st;
    int s;
    for (s = 0; s < 16; s++) {
        st = S(p, s % 3, s, QD_TRIG_NOTE, 40 + s * 2, 0, 0, 0);
        st->chance = 60;
        st->ratch = 1 + s % 3;
        st->micro = s % 5 - 2;
        if (s % 4 == 0)
            LK(st, QD_QP_FREQ, 2000 + s * 300);
    }
    AUDIO = a;
    run_pattern(4.0, 777);
    na = NAUDIO;
    AUDIO = b;
    run_pattern(4.0, 777);
    CHECK(na == NAUDIO && !memcmp(a, b, na * sizeof a[0]), "the same run twice: the same samples");
    {
        double e = 0;
        size_t i;
        for (i = 0; i < na; i++)
            e += fabs((double)a[i]);
        CHECK(e > 1e6, "and it sounds (%g)", e);
    }
    AUDIO = b;
    run_pattern(4.0, 778);
    CHECK(memcmp(a, b, na * sizeof a[0]) != 0, "another seed: other rolls");
    AUDIO = 0;
    printf("  %-4s determinism       4 s of 3 tracks with chance, ratchets, micro, locks, twice: identical\n",
           fails == n0 ? "ok" : "FAIL");
}

/* the transport: stop releases, the counters reset; mute */
static void u_transport(void)
{
    int n0 = fails, i, held = 0;
    TP *p = tp_new(0, 120);
    S(p, 0, 0, QD_TRIG_NOTE, 40, 0, 0, 0)->len = QD_LEN_INF;
    S(p, 1, 0, QD_TRIG_NOTE, 52, 0, 0, 0)->len = 16;
    S(p, 2, 0, QD_TRIG_NOTE, 64, 0, 0, 0)->cond = QD_COND_1ST;
    run_pattern(0.5, 1);
    for (i = 0; i < POOL.nvoice; i++)
        held += POOL.v[i].q.active && POOL.v[i].q.gate;
    CHECK(held == 2, "two notes held (INF and 16 steps): %d", held);
    qd_player_stop(&PL);
    for (i = 0, held = 0; i < POOL.nvoice; i++)
        held += POOL.v[i].q.active && POOL.v[i].q.gate;
    CHECK(held == 0 && SEQ.nev == 0 && !SEQ.playing, "stop: every note released, the queue empty");
    NENG = 0;
    qd_seq_play(&SEQ);
    rig_run_to(SEQ.now + 4410);
    for (i = 0, held = 0; i < NENG; i++)
        held += ENG[i].kind == QD_LOG_ON && ENG[i].note == 64;
    CHECK(held == 1, "PLAY again: pass 1 again (1ST plays)");
    PROJ.pat[0].trk[1].mute = 1;
    NENG = 0;
    rig_run_to(SEQ.now + 44100 * 5 / 2);
    for (i = 0, held = 0; i < NENG; i++)
        held += ENG[i].kind == QD_LOG_ON && ENG[i].track == 1;
    CHECK(held == 0, "a muted track plays nothing (%d)", held);
    (void)0;
    /* master INF: a queued pattern waits for ever; immediate still changes at the next 16th */
    PROJ.pat[0].master = QD_MASTER_INF;
    qd_pat_init(&PROJ.pat[5]);
    qd_seq_select(&SEQ, 5, 0);
    rig_run_to(SEQ.now + 44100 * 5);
    CHECK(SEQ.cur == 0 && qd_seq_queued(&SEQ) == 5, "master INF: the queued pattern waits (%d)", SEQ.cur);
    qd_seq_select(&SEQ, 5, 1);
    rig_run_to(SEQ.now + 44100 * 15 / 120 + 64);
    CHECK(SEQ.cur == 5 && qd_seq_queued(&SEQ) < 0, "immediate: at the next 16th (%d)", SEQ.cur);
    printf("  %-4s transport         stop releases every note and empties the queue; PLAY restarts the passes; "
           "mute; master INF waits\n", fails == n0 ? "ok" : "FAIL");
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: test_seq GOLDEN_DIR OUT_DIR [--bless]\n");
        return 2;
    }
    GOLD = argv[1];
    OUT = argv[2];
    BLESS = argc > 3 && !strcmp(argv[3], "--bless");
    printf("test_seq: the sequencer (core/seq)%s\n", BLESS ? ", writing the goldens" : "");
    u_lockpool();
    u_edit();
    u_patch();
    u_dip();
    u_transport();
    u_determinism();
    p_sixteenths();
    p_polymeter();
    p_swing();
    p_micro();
    p_strum();
    p_ratchet();
    p_lengths();
    p_conditions();
    p_chance();
    p_soundlocks();
    p_plocks();
    p_lockstep();
    p_tie();
    p_change();
    p_clock();
    p_mix();
    printf("test_seq: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
