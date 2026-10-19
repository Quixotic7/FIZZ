/* SPDX-License-Identifier: GPL-3.0-only */
/* The sequencer's tests (core/seq: pattern.h, seq.h, player.h, clip.h, scale.h, sounds.h; PLAN.md §4 .. §6, §8b):
 *
 *   units       the lock pool (sorted, counted, full, copy, clear), the editing API, the conditions' table, the patch
 *               of a trig (a sound lock and p-locks against a patch built by hand), the dip (the largest sample step
 *               of a routing change on a sounding voice, with and without it), determinism (the same run twice: the
 *               same samples); the clipboard (a step copied and pasted between tracks with its locks, each paste
 *               again; a page with its start and end, joining the loop; cut; clear a page, a track, steps; undo, undo
 *               again = redo, an undo for another pattern does nothing; a paste onto a full lock pool: QD_EFULL and
 *               nothing changed); the scales (names, sizes, qd_scale_note for every scale, root, octave and degree,
 *               negative ones too, against the test's own interval tables; step, snap, the track's key); the sound
 *               pool (23 sounds, their names, the presets, the drums as sounds.c makes them, add, full); P1.3c: the
 *               21 rates (base x type) whole and divisible, the clipboard's undo groups
 *   timelines   22 patterns, each aimed at a feature, run through the sequencer, the player and the voice pool
 *               (the voices rendered, no effects); every event becomes a line of text (the sample, the track, on / off /
 *               lock / legato / dip, the note, the velocity, the hash of the voice's patch, its own mask and mix locks)
 *               written to OUT/timeline-NAME.txt and compared with the committed golden GOLDEN/NAME.txt; and every
 *               timeline is checked against THE REFERENCE below, an independent implementation from first principles:
 *               it walks each track's steps in plain loops (no look-ahead, no queue, no cursors), computes each
 *               event's time in seconds from the tempo, the rate, swing, micro, ratchet and strum, evaluates the
 *               conditions and chance with its own code, builds each trig's patch by hand (the pool sound, by hand for
 *               the drums too, or the track's sound, the locks written over it) and hashes it, and replays the holds
 *               (lengths, INF, a newer strike of the same note, TIE). Every reference event must be in the timeline
 *               (within a sample: the reference's seconds against the engine's integer ticks) and every timeline event
 *               in the reference.
 *               PAGES AND DEFAULTS (PLAN §8b) the reference models itself, not through the engine's functions: a
 *               track's loop is the list of its looped pages' steps, ascending, each page from its start to its end
 *               (r_walk); loop position n mod that list's length plays the step at that absolute index, the passes are
 *               the list's wraps (A:B, 1ST) and swing takes the position's parity. A step's parameter it does not
 *               override is the track's default as it is at the block that resolves the step (two steps ahead:
 *               r_resolve_block), so a default changed while playing reaches the steps resolved after it; notes given
 *               as scale degrees are mapped with the test's own tables; solo and mute are read at the block an event
 *               leaves in. p01..p16 override every parameter of every step: their goldens are as before P1.3b.
 *               p01..p16  sixteenths, polymeter, swing, micro, strum, ratchet, lengths, conditions, chance, sound locks,
 *                         p-locks, LOCK steps, TIE, pattern change and chains, MIDI clock, rates and transpose
 *               p17-pages     pages with their own start and end (steps outside them silent; swing by loop position)
 *               p18-loops     one page alone, pages A..B, pages 1 and 3; A:B, 1ST counting the loop's passes
 *               p19-defaults  tapped steps playing the track's defaults (a chord, velocity, length, micro, ratchet,
 *                             chance, condition, strum, TIE and LOCK as the trig), a mix with overrides, the chord,
 *                             velocity and ratchet defaults changed while playing
 *               p20-pool      sound locks to the pool: the drums 18..23 with p-locks over them, one sound on many
 *                             steps, factory sounds, INIT, the track's own
 *               p21-scales    notes from degrees of seven scales (negative degrees, the edges, a track following the
 *                             global key)
 *               p22-solo      one track soloed, two, a soloed one that is muted, the solo cleared
 *               P1.3c: QD_TPS doubled to 100800 and QD_SUB halved (a 16th the same subticks): p01..p22 unchanged,
 *               byte for byte; a step 18 bytes (six notes since P1.3e, five and a pad before)
 *               p23-five      five notes a step: overridden, strummed, ratcheted, offsets to +48; a five-note default
 *                             changed while playing; five scale degrees; a TIE of five
 *               p24-launch    the launch modes BAR, BEAT, END, NOW (the reference reckons each change point)
 *               p28-six       (P1.3e) six notes a step: together, strummed, ratcheted, offsets to +48; a six-note
 *                             default changed while playing; a TIE of six; the six-note chord names
 *               p25..p27      every rate: the seven bases 1/1 .. 1/64, normal, dotted, triplet, with micro, ratchet x8,
 *                             strum, 1/8-step lengths and swing at each
 *
 *     test_seq GOLDEN_DIR OUT_DIR [--bless]     exit 1 on a failed check; --bless writes the goldens */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chord.h"
#include "clip.h"
#include "mix.h"
#include "player.h"
#include "scale.h"
#include "seq.h"
#include "sounds.h"

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
/* a step (TS): its values and the parameters it overrides (ovr, bit QD_PM_k; a parameter not overridden takes the
 * track's default); its notes either absolute or, sc set, scale degrees of the track's key at octave sc_oct. A track
 * (TT): its rate and so on; its pages (pages set: the loop mask and each page's start and end, 0..15; else `length`,
 * Form's flat rebuild: pages 1..k looped, full but the last); its defaults (def; def2 from R_DEF_AT on when has2); its
 * key (scale, root, or the pattern's global key when keyg). The tests p01..p16 override every parameter of every step
 * (S): their patterns are what they were before the defaults existed */
typedef struct { int nn, notes[QD_NNOTE], vel, len, micro, ratch, ramp, chance, cond, type, strum; } TD;
typedef struct {
    int t, s, type, nn, notes[QD_NNOTE], vel, len, micro, ratch, ramp, cond, chance, sound, strum, nlk;
    int lk_id[8], lk_v[8];
    int ovr;
    int sc, sc_oct, sc_n, sc_deg[QD_NNOTE];
} TS;
typedef struct {
    int rate, length, swing, limit, transpose, mute;
    int pages, loop, pstart[8], pend[8];
    TD def, def2;
    int has2;
    int scale, root, keyg;
} TT;
typedef struct {
    int bpm, swing, master;
    int key_scale, key_root;
    TT trk[8];
    TS st[600];
    int n;
} TP;
static TP TPAT[8];
#define OV_ALL ((1 << QD_NPM) - 1)

static void td_init(TD *d)                              /* (pattern.h's: C4, 100, a step, x1, 100 %, ALWAYS, NOTE) */
{
    memset(d, 0, sizeof *d);
    d->nn = 1;
    d->notes[0] = 60;
    d->vel = 100;
    d->len = QD_LEN_1;
    d->ratch = 1;
    d->chance = 100;
    d->cond = QD_COND_ALWAYS;
    d->type = QD_TRIG_NOTE;
}

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
        td_init(&p->trk[t].def);
        td_init(&p->trk[t].def2);
    }
    return p;
}

/* explicit pages: the loop mask, every page 1..16 until RANGE says otherwise */
static void PAGES(TT *tt, int loop)
{
    int p;
    tt->pages = 1;
    tt->loop = loop;
    for (p = 0; p < 8; p++) {
        tt->pstart[p] = 0;
        tt->pend[p] = 15;
    }
}
static void RANGE(TT *tt, int page, int start, int end)
{
    tt->pstart[page] = start;
    tt->pend[page] = end;
}

/* a step with every parameter overridden (p01..p16's) */
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
    st->ovr = OV_ALL;
    return st;
}
/* a tapped step: no overrides (the track's defaults); OV sets one parameter and its override */
static TS *D(TP *p, int t, int s)
{
    TS *st = S(p, t, s, QD_TRIG_NOTE, 0, 0, 0, 0);
    st->ovr = 0;
    return st;
}
static TS *OV(TS *st, int pm, int v)
{
    switch (pm) {
    case QD_PM_VEL: st->vel = v; break;
    case QD_PM_LEN: st->len = v; break;
    case QD_PM_MICRO: st->micro = v; break;
    case QD_PM_CHANCE: st->chance = v; break;
    case QD_PM_COND: st->cond = v; break;
    case QD_PM_TRIG: st->type = v; break;
    case QD_PM_SOUND: st->sound = v; break;
    case QD_PM_STRUM: st->strum = v; break;
    default: break;                                     /* (NOTE: NOTES / DEG; RATCH: RATCH) */
    }
    st->ovr |= QD_OV(pm);
    return st;
}
static void NOTES(TS *st, int n1, int n2, int n3, int n4)
{
    const int nv[4] = {n1, n2, n3, n4};
    int k;
    st->nn = 0;
    for (k = 0; k < 4; k++)
        if (nv[k])
            st->notes[st->nn++] = nv[k];
    st->ovr |= QD_OV(QD_PM_NOTE);
}
/* P1.3c: a fifth note added to a step's notes (S / NOTES give four), or a fifth degree to DEG's four */
static TS *NOTE5(TS *st, int n5)
{
    st->notes[st->nn++] = n5;
    st->ovr |= QD_OV(QD_PM_NOTE);
    return st;
}
static void DEG5(TS *st, int d4)
{
    st->sc_deg[4] = d4;
    st->sc_n = 5;
}
static void RATCH(TS *st, int count, int ramp)
{
    st->ratch = count;
    st->ramp = ramp;
    st->ovr |= QD_OV(QD_PM_RATCH);
}
/* the notes as n degrees of the track's key at octave oct (the engine's side: qd_scale_note at build time) */
static void DEG(TS *st, int oct, int n, int d0, int d1, int d2, int d3)
{
    st->sc = 1;
    st->sc_oct = oct;
    st->sc_n = n;
    st->sc_deg[0] = d0, st->sc_deg[1] = d1, st->sc_deg[2] = d2, st->sc_deg[3] = d3;
    st->ovr |= QD_OV(QD_PM_NOTE);
}
static void LK(TS *st, int id, int v)
{
    st->lk_id[st->nlk] = id;
    st->lk_v[st->nlk++] = v;
}

/* a track's defaults through the API */
static int apply_def(qd_pattern_t *P, int t, const TD *d)
{
    uint8_t nv[QD_NNOTE];
    int k, r = 0;
    for (k = 0; k < d->nn; k++)
        nv[k] = (uint8_t)d->notes[k];
    r |= qd_tdef_set_notes(P, t, nv, d->nn);
    r |= qd_tdef_set(P, t, QD_PM_VEL, d->vel);
    r |= qd_tdef_set(P, t, QD_PM_LEN, d->len);
    r |= qd_tdef_set(P, t, QD_PM_MICRO, d->micro);
    r |= qd_tdef_set(P, t, QD_PM_RATCH, qd_ratch_make(d->ratch, d->ramp));
    r |= qd_tdef_set(P, t, QD_PM_CHANCE, d->chance);
    r |= qd_tdef_set(P, t, QD_PM_COND, d->cond);
    r |= qd_tdef_set(P, t, QD_PM_TRIG, d->type);
    r |= qd_tdef_set(P, t, QD_PM_STRUM, d->strum);
    return r;
}

/* the test's pattern into the project through the editing API: a step sets only what it overrides */
static void build(const TP *tp, int k)
{
    qd_pattern_t *P = &PROJ.pat[k];
    int i, t;
    qd_pat_init(P);
    P->bpm = (uint16_t)tp->bpm;
    P->swing = (uint8_t)tp->swing;
    P->master = (uint16_t)tp->master;
    PROJ.key_scale = (uint8_t)tp->key_scale;
    PROJ.key_root = (uint8_t)tp->key_root;
    for (t = 0; t < 8; t++) {
        const TT *tt = &tp->trk[t];
        int r = 0, pg;
        qd_tset_set_rate(&P->trk[t], tt->rate);
        if (tt->pages) {
            r |= qd_page_set_loop(P, t, tt->loop);
            for (pg = 0; pg < 8; pg++)
                r |= qd_page_set_range(P, t, pg, tt->pstart[pg], tt->pend[pg]);
        } else {
            r |= qd_track_set_length(P, t, tt->length);
        }
        P->trk[t].swing = (uint8_t)tt->swing;
        P->trk[t].limit = (uint8_t)tt->limit;
        P->trk[t].transpose = (int8_t)tt->transpose;
        P->trk[t].mute = (uint8_t)tt->mute;
        P->trk[t].scale = (uint8_t)tt->scale;
        P->trk[t].root = (uint8_t)tt->root;
        P->trk[t].key_global = (uint8_t)tt->keyg;
        r |= apply_def(P, t, &tt->def);
        CHECK(r == QD_OK, "build: track %d's pages and defaults: %d", t, r);
    }
    for (i = 0; i < tp->n; i++) {
        const TS *s = &tp->st[i];
        uint8_t nv[QD_NNOTE];
        int k2, r = 0, nn = s->nn;
#define HAS(pm) ((s->ovr >> (pm)) & 1)
        if (s->sc) {                                    /* scale degrees: the engine's own mapping */
            int sc, root;
            qd_track_key(&PROJ, P, s->t, &sc, &root);
            for (k2 = 0, nn = s->sc_n; k2 < nn; k2++)
                nv[k2] = (uint8_t)qd_scale_note(sc, root, s->sc_oct, s->sc_deg[k2]);
        } else {
            for (k2 = 0; k2 < nn; k2++)
                nv[k2] = (uint8_t)s->notes[k2];
        }
        if (HAS(QD_PM_TRIG))
            r |= qd_step_set_trig(P, s->t, s->s, s->type);
        else
            r |= qd_step_on(P, s->t, s->s);
        if (HAS(QD_PM_NOTE) && nn)
            r |= qd_step_set_notes(P, s->t, s->s, nv, nn);
        if (HAS(QD_PM_VEL))
            r |= qd_step_set_vel(P, s->t, s->s, s->vel);
        if (HAS(QD_PM_LEN))
            r |= qd_step_set_len(P, s->t, s->s, s->len);
        if (HAS(QD_PM_MICRO))
            r |= qd_step_set_micro(P, s->t, s->s, s->micro);
        if (HAS(QD_PM_RATCH))
            r |= qd_step_set_ratchet(P, s->t, s->s, s->ratch, s->ramp);
        if (HAS(QD_PM_COND))
            r |= qd_step_set_cond(P, s->t, s->s, s->cond);
        if (HAS(QD_PM_CHANCE))
            r |= qd_step_set_chance(P, s->t, s->s, s->chance);
        if (HAS(QD_PM_SOUND))
            r |= qd_step_set_sound(P, s->t, s->s, s->sound);
        if (HAS(QD_PM_STRUM))
            r |= qd_step_set_strum(P, s->t, s->s, s->strum);
        for (k2 = 0; k2 < s->nlk; k2++)
            r |= qd_lock_set(P, s->t, s->s, s->lk_id[k2], s->lk_v[k2]);
        if (HAS(QD_PM_TRIG) && s->type != QD_TRIG_NOTE)   /* (its type again, after the setters) */
            r |= qd_step_set_trig(P, s->t, s->s, s->type);
        CHECK(r == QD_OK, "build: track %d step %d: %d", s->t, s->s, r);
        {   /* (sound 0 is the track's sound: no override; a step with no notes keeps the track's) */
            const int want = s->ovr & ~(s->sound ? 0 : QD_OV(QD_PM_SOUND)) & ~(nn ? 0 : QD_OV(QD_PM_NOTE));
            CHECK(qd_step(P, s->t, s->s)->ovr == want, "build: track %d step %d overrides %03x (the test's %03x)",
                  s->t, s->s, qd_step(P, s->t, s->s)->ovr, want);
        }
#undef HAS
    }
    CHECK(qd_pat_check(P), "build: pattern %d consistent", k);
}

/* ================================================================ THE REFERENCE === */
/* An independent model of what PLAN §4 .. §6 ask, written plainly: no look-ahead, no queue, no cursors. */
/* a step's ticks at each rate code (P1.3c: base x 3 + type), from first principles: a whole note is 16 sixteenths of
 * QD_TPS ticks; the base 1/2^b of it; dotted half as long again, triplet two thirds */
static int64_t R_STEP_T(int code)
{
    const int b = code / 3, ty = code % 3;
    int64_t l = (int64_t)QD_TPS * 16;
    int i;
    for (i = 0; i < b; i++)
        l /= 2;
    return ty == 1 ? l + l / 2 : ty == 2 ? l * 2 / 3 : l;
}
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

/* the sound pool by hand (sounds.h's list: 1 INIT, 2..17 the factory sounds, 18..23 the drums; the drums' values as
 * sounds.c's comment and tables give them, clamped to their ranges): sound k's patch */
typedef struct { int id, v; } rkv_t;
static int R_POOL_CLAMPED;                              /* (a drum value outside its range: none should be) */
#define R_N(a) ((int)(sizeof(a) / sizeof((a)[0])))
static void r_pool_patch(int k, int16_t *p)
{
    static const rkv_t KICK[] = {
        {QD_QP_EDEC, 32}, {QD_QP_ESUS, 0}, {QD_QP_EREL, 20}, {QD_QP_LFO(0, QD_QL_SPEED), 4000},
        {QD_QP_LFO(0, QD_QL_MULT), 18}, {QD_QP_LFO(0, QD_QL_DEST), 1}, {QD_QP_LFO(0, QD_QL_WAVE), 5},
        {QD_QP_LFO(0, QD_QL_TRIG), 4}, {QD_QP_LFO(0, QD_QL_DEPTH), 6399}, {QD_QP_ALEV, 60}, {QD_QP_ADEC, 4},
        {QD_QP_AEND, 0}, {QD_QP_RA, 8}, {QD_QP_LEVEL, 127}};
    static const rkv_t SNARE[] = {{QD_QP_FTYPE, QD_QF_LP12}, {QD_QP_FREQ, 9500}, {QD_QP_RESO, 1000}, {QD_QP_FDBK, 110},
                                  {QD_QP_EDEC, 24}, {QD_QP_ESUS, 0}, {QD_QP_EREL, 18}, {QD_QP_LEVEL, 120}};
    static const rkv_t CLAP[] = {{QD_QP_FTYPE, QD_QF_LP12}, {QD_QP_FREQ, 10500}, {QD_QP_RESO, 3000}, {QD_QP_FDBK, 120},
                                 {QD_QP_EDEC, 14}, {QD_QP_ESUS, 0}, {QD_QP_EREL, 12}};
    static const rkv_t GHOST[] = {{QD_QP_FTYPE, QD_QF_LP12}, {QD_QP_FREQ, 8000}, {QD_QP_RESO, 0}, {QD_QP_FDBK, 110},
                                  {QD_QP_EDEC, 16}, {QD_QP_ESUS, 0}, {QD_QP_EREL, 12}, {QD_QP_LEVEL, 70}};
    static const rkv_t HAT[] = {{QD_QP_FREQ, 9500}, {QD_QP_RESO, 1000}, {QD_QP_EDEC, 10}, {QD_QP_ESUS, 0},
                                {QD_QP_EREL, 8}};
    static const rkv_t OHAT[] = {{QD_QP_FREQ, 9000}, {QD_QP_RESO, 1000}, {QD_QP_EDEC, 32}, {QD_QP_ESUS, 0},
                                 {QD_QP_EREL, 26}};
    const rkv_t *kv = 0;
    int n = 0, i, preset = k - 1;
    switch (k) {
    case 18: preset = 0, kv = KICK, n = R_N(KICK); break;
    case 19: preset = 16, kv = SNARE, n = R_N(SNARE); break;
    case 20: preset = 16, kv = CLAP, n = R_N(CLAP); break;
    case 21: preset = 16, kv = GHOST, n = R_N(GHOST); break;
    case 22: preset = 16, kv = HAT, n = R_N(HAT); break;
    case 23: preset = 16, kv = OHAT, n = R_N(OHAT); break;
    default: break;
    }
    qd_quad_preset_patch(preset, p);
    for (i = 0; i < n; i++) {
        int16_t lo, hi;
        qd_lock_range(kv[i].id, &lo, &hi);
        R_POOL_CLAMPED += kv[i].v < lo || kv[i].v > hi;
        p[kv[i].id] = (int16_t)(kv[i].v < lo ? lo : kv[i].v > hi ? hi : kv[i].v);
    }
}

/* the scales by hand: each one's size and its intervals above the root */
static const struct { const char *name, *shrt; int n, iv[12]; } R_SC[QD_NSCALE] = {
    {"CHROMATIC", "CHR", 12, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}},
    {"MAJOR", "MAJ", 7, {0, 2, 4, 5, 7, 9, 11}},
    {"MINOR", "MIN", 7, {0, 2, 3, 5, 7, 8, 10}},
    {"DORIAN", "DOR", 7, {0, 2, 3, 5, 7, 9, 10}},
    {"PHRYGIAN", "PHR", 7, {0, 1, 3, 5, 7, 8, 10}},
    {"LYDIAN", "LYD", 7, {0, 2, 4, 6, 7, 9, 11}},
    {"MIXOLYDIAN", "MIX", 7, {0, 2, 4, 5, 7, 9, 10}},
    {"LOCRIAN", "LOC", 7, {0, 1, 3, 5, 6, 8, 10}},
    {"HARM MINOR", "HMI", 7, {0, 2, 3, 5, 7, 8, 11}},
    {"MEL MINOR", "MMI", 7, {0, 2, 3, 5, 7, 9, 11}},
    {"MAJ PENTA", "MPN", 5, {0, 2, 4, 7, 9}},
    {"MIN PENTA", "mPN", 5, {0, 3, 5, 7, 10}},
    {"BLUES", "BLU", 6, {0, 3, 5, 6, 7, 10}},
    {"WHOLE TONE", "WHL", 6, {0, 2, 4, 6, 8, 10}},
};
/* degree d of the scale from the root's note at octave oct (12 (oct + 1) + root): whole octaves of the scale's size
 * up or down, then the interval; clamped 0..127 */
static int r_scale_note(int sc, int root, int oct, int deg)
{
    const int n = R_SC[sc].n;
    int o = 0, x;
    while (deg < 0)
        deg += n, o--;
    o += deg / n;
    x = 12 * (oct + 1) + root + 12 * o + R_SC[sc].iv[deg % n];
    return x < 0 ? 0 : x > 127 ? 127 : x;
}
static int r_scale_has(int sc, int root, int note)
{
    int k;
    for (k = 0; k < R_SC[sc].n; k++)
        if (((note - root - R_SC[sc].iv[k]) % 12 + 12) % 12 == 0)
            return 1;
    return 0;
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
        r_pool_patch(s->sound, o->patch);
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

static double r_sec_of_tick(int64_t tick) { return (double)tick / (double)QD_TPS * 15.0 / R_BPM; }
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

/* the step at absolute index s (0..127) of track t */
static const TS *r_step_at(const TP *tp, int t, int s)
{
    int i;
    for (i = 0; i < tp->n; i++)
        if (tp->st[i].t == t && tp->st[i].s == s)
            return &tp->st[i];
    return 0;
}

/* a track's loop by hand: the absolute steps in the order they play, the looped pages ascending, each from its start
 * to its end (the `length` shorthand: pages 1..k, full but the last). w: room for 128; the count */
static int r_walk(const TT *tt, int *w)
{
    int loop, ps[8], pe[8], p, s, n = 0;
    if (tt->pages) {
        loop = tt->loop & 255 ? tt->loop & 255 : 1;
        for (p = 0; p < 8; p++)
            ps[p] = tt->pstart[p], pe[p] = tt->pend[p];
    } else {
        const int k = (tt->length + 15) / 16;
        loop = (1 << k) - 1;
        for (p = 0; p < 8; p++) {
            ps[p] = 0;
            pe[p] = p == k - 1 ? tt->length - 16 * (k - 1) - 1 : 15;
        }
    }
    for (p = 0; p < 8; p++)
        if ((loop >> p) & 1)
            for (s = ps[p]; s <= pe[p]; s++)
                w[n++] = 16 * p + s;
    return n;
}

/* a default changed at R_DEF_AT (a block's first sample; -1 none): the tracks with has2 take def2 from there */
static int64_t R_DEF_AT = -1;
/* the solo mask: R_SOLO0 from PLAY, R_SOLO_M[k] from sample R_SOLO_AT[k] on (block-aligned) */
static int R_SOLO0, R_NSOLO, R_SOLO_M[8];
static int64_t R_SOLO_AT[8];

/* the steps as they play, resolved by the reference (the raw events point at them) */
static TS RSTEP[20000];
static int NRSTEP;

/* step s of track tt as it plays when resolved for grid g (step length L): its own values where it overrides them,
 * else the track's defaults as they are at the block that resolves it; notes from degrees by the test's own scales */
static const TS *r_resolve(const TP *tp, const TT *tt, const TS *s, int64_t g, int64_t L)
{
    const TD *d = tt->has2 && R_DEF_AT >= 0 && r_resolve_block(g, L) * 32 >= R_DEF_AT ? &tt->def2 : &tt->def;
    TS *v = &RSTEP[NRSTEP < 20000 ? NRSTEP++ : 19999];
    if (NRSTEP >= 20000)
        CHECK(0, "the reference: too many steps resolved");
    int k;
#define HAS(pm) ((s->ovr >> (pm)) & 1)
    *v = *s;
    if (HAS(QD_PM_NOTE) && s->sc) {
        const int sc = tt->keyg ? tp->key_scale : tt->scale, root = tt->keyg ? tp->key_root : tt->root;
        v->nn = s->sc_n;
        for (k = 0; k < s->sc_n; k++)
            v->notes[k] = r_scale_note(sc, root, s->sc_oct, s->sc_deg[k]);
    } else if (!HAS(QD_PM_NOTE) || !s->nn) {
        v->nn = d->nn;
        for (k = 0; k < QD_NNOTE; k++)
            v->notes[k] = d->notes[k];
    }
    if (!HAS(QD_PM_VEL))
        v->vel = d->vel;
    if (!HAS(QD_PM_LEN))
        v->len = d->len;
    if (!HAS(QD_PM_MICRO))
        v->micro = d->micro;
    if (!HAS(QD_PM_RATCH))
        v->ratch = d->ratch, v->ramp = d->ramp;
    if (!HAS(QD_PM_CHANCE))
        v->chance = d->chance;
    if (!HAS(QD_PM_COND))
        v->cond = d->cond;
    if (!HAS(QD_PM_TRIG))
        v->type = d->type;
    if (!HAS(QD_PM_SOUND))
        v->sound = 0;                                   /* the track's own sound */
    if (!HAS(QD_PM_STRUM))
        v->strum = d->strum;
#undef HAS
    return v;
}

/* the track sounds at this sample (not muted; soloed when any is) */
static int r_audible(const TP *tp, int t, int64_t smp)
{
    const int64_t b = smp / 32 * 32;                    /* (read when the event leaves: its block) */
    int k, solo = R_SOLO0;
    for (k = 0; k < R_NSOLO; k++)
        if (R_SOLO_AT[k] <= b)
            solo = R_SOLO_M[k];
    return !tp->trk[t].mute && (!solo || ((solo >> t) & 1));
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
    NRSTEP = 0;
    for (si = 0; si < nseg; si++) {
        const TP *tp = seg[si].tp;
        int64_t hg[8][2048];                            /* the conditional steps' grids and results, per track */
        int hr[8][2048], hn[8];
        memset(hn, 0, sizeof hn);
        for (t = 0; t < 8; t++) {
            const TT *tt = &tp->trk[t];
            const int64_t L = R_STEP_T(tt->rate);
            int walk[128];
            const int len = r_walk(tt, walk);
            int64_t n;
            int pre = 0;
            for (n = 0;; n++) {
                const int64_t g = seg[si].o + n * L;
                const int idx = (int)(n % len);         /* the loop position (swing's parity) */
                const int64_t pass = n / len;           /* the loop's passes (A:B, 1ST) */
                const TS *s;
                int ok = 1, a, b, conditional;
                if ((seg[si].e >= 0 && g >= seg[si].e) || g >= end)
                    break;
                s = r_step_at(tp, t, walk[idx]);
                if (!s)
                    continue;
                s = r_resolve(tp, tt, s, g, L);
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
                    int notes[QD_NNOTE], k, h;
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
                        r->step = walk[idx];
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
                            r->step = walk[idx];
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
            if (!r_audible(&TPAT[0], t, smp))           /* muted, or another track soloed: nothing (no OFF to come) */
                continue;
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
    SEQ.solo = (uint8_t)R_SOLO0;
    NAUDIO = 0;
    while (SEQ.now < end) {
        int32_t m[32], k;
        for (k = 0; k < R_NSOLO; k++)                   /* the solo mask changed between blocks */
            if ((int64_t)SEQ.now == R_SOLO_AT[k])
                SEQ.solo = (uint8_t)R_SOLO_M[k];
        if (R_DEF_AT >= 0 && (int64_t)SEQ.now == R_DEF_AT)   /* a default changed while playing */
            for (k = 0; k < 8; k++)
                if (TPAT[0].trk[k].has2)
                    CHECK(apply_def(&PROJ.pat[0], k, &TPAT[0].trk[k].def2) == QD_OK, "track %d's new defaults", k);
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
    const int64_t end_tick = (int64_t)(sec / (15.0 / TPAT[0].bpm) * (double)QD_TPS) + 1;
    R_BPM = TPAT[0].bpm;
    R_SEED = seed;
    seg.tp = &TPAT[0];
    seg.k = 0;
    seg.o = 0;
    seg.e = -1;
    r_generate(&seg, 1, end_tick + 4 * 24 * (int64_t)QD_TPS);   /* (four of the longest steps, dotted 1/1) */
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
static void p_five(void);
static void p_launch(void);
static void p_rates(void);
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
        /* the reference's segments: A's master is 16 steps (16 x QD_TPS) */
        {
            const int64_t TA = 16 * QD_TPS, TB = 12 * QD_TPS, TC = 8 * QD_TPS;
            const int64_t t2 = (int64_t)((q2 * 8 * 120000 + QD_SUB - 1) / QD_SUB);   /* the tick at q2 (ceil) */
            const int64_t t3 = (int64_t)((q3 * 8 * 120000 + QD_SUB - 1) / QD_SUB);
            int64_t o, cx;
            int k;
            seg[nseg++] = (rseg_t){a, 0, 0, TA};        /* A until its master */
            x = (t2 + QD_TPS - 1) / QD_TPS * QD_TPS;    /* C immediately: the next 16th after q2 */
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
    r_generate(seg, nseg, (int64_t)14 * 8 * QD_TPS);
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

/* p17: pages with their own start and end; the steps outside the ranges (and on pages not looped) never play */
static void p_pages(void)
{
    TP *p = tp_new(0, 120);
    TS *st;
    int s;
    p->swing = 60;
    PAGES(&p->trk[0], 1);                               /* track 1: page 1, steps 3..10, alone */
    RANGE(&p->trk[0], 0, 2, 9);
    for (s = 0; s < 20; s++)                            /* (every step of page 1, and some of page 2: not looped) */
        S(p, 0, s, QD_TRIG_NOTE, 48 + s, 0, 0, 0)->len = 2;
    PAGES(&p->trk[1], 3);                               /* track 2: page 1 1..15, page 2 5..12; swing on: the loop's */
    RANGE(&p->trk[1], 0, 0, 14);                        /* position 15 (page 2's first, step 21) is odd: swung */
    RANGE(&p->trk[1], 1, 4, 11);
    p->trk[1].swing = 1;
    for (s = 0; s < 32; s++)
        if (s % 2 == 0 || s >= 28 || (s >= 16 && s < 20) || s == 15)
            S(p, 1, s, QD_TRIG_NOTE, 60 + s % 24, 0, 0, 0)->len = 1;
    PAGES(&p->trk[2], 1);                               /* track 3: a loop of one step (page 1, 6..6) at 2x */
    RANGE(&p->trk[2], 0, 5, 5);
    p->trk[2].rate = QD_RATE_2;
    S(p, 2, 4, QD_TRIG_NOTE, 70, 0, 0, 0);
    S(p, 2, 5, QD_TRIG_NOTE, 72, 0, 0, 0)->len = 1;
    S(p, 2, 6, QD_TRIG_NOTE, 74, 0, 0, 0);
    PAGES(&p->trk[3], (1 << 1) | (1 << 2));             /* track 4 at 1/2x: page 2 1..7 and page 3 9..16 */
    RANGE(&p->trk[3], 1, 0, 6);
    RANGE(&p->trk[3], 2, 8, 15);
    p->trk[3].rate = QD_RATE_1_2;
    for (s = 0; s < 48; s += 3) {
        st = S(p, 3, s, QD_TRIG_NOTE, 36 + s % 30, 0, 0, 0);
        st->len = 3;
    }
    pattern_test("p17-pages", "pages with their own start and end: page 1 3..10 alone; pages 1 (1..15) and 2 (5..12) "
                 "swung by loop position; a one-step loop at 2x; pages 2 (1..7) and 3 (9..16) at 1/2x", 6.0, 1);
}

/* p18: loops of pages: one page alone (not page 1), a contiguous A..B, a non-contiguous pair; A:B counts the loop's
 * passes */
static void p_loops(void)
{
    TP *p = tp_new(0, 140);
    TS *st;
    int s;
    PAGES(&p->trk[0], 1 << 2);                          /* track 1: page 3 alone (F1 double-tap), 5..12 */
    RANGE(&p->trk[0], 2, 4, 11);
    S(p, 0, 32 + 4, QD_TRIG_NOTE, 60, 0, 0, 0)->cond = qd_cond_ab(1, 2);
    S(p, 0, 32 + 6, QD_TRIG_NOTE, 62, 0, 0, 0)->cond = qd_cond_ab(2, 2);
    S(p, 0, 32 + 8, QD_TRIG_NOTE, 64, 0, 0, 0)->cond = qd_cond_ab(1, 3);
    S(p, 0, 32 + 10, QD_TRIG_NOTE, 65, 0, 0, 0)->cond = qd_cond_ab(3, 4);
    S(p, 0, 32 + 11, QD_TRIG_NOTE, 67, 0, 0, 0)->len = 2;
    S(p, 0, 32 + 0, QD_TRIG_NOTE, 40, 0, 0, 0);         /* (outside the range) */
    S(p, 0, 3, QD_TRIG_NOTE, 41, 0, 0, 0);              /* (page 1: not in the loop) */
    PAGES(&p->trk[1], 0xE);                             /* track 2 at 2x: pages 2..4 (hold 2, press 4): 16 + 4 + 4 */
    RANGE(&p->trk[1], 2, 8, 11);
    RANGE(&p->trk[1], 3, 0, 3);
    p->trk[1].rate = QD_RATE_2;
    for (s = 16; s < 64; s += 2) {
        st = S(p, 1, s, QD_TRIG_NOTE, 50 + s % 32, 0, 0, 0);
        st->len = 1;
        if (s == 16)
            st->cond = qd_cond_ab(1, 3);                /* the loop's first step, one pass in three */
        if (s == 42)
            st->cond = qd_cond_ab(2, 2);
    }
    S(p, 1, 2, QD_TRIG_NOTE, 90, 0, 0, 0);              /* (page 1: not in the loop) */
    PAGES(&p->trk[2], (1 << 0) | (1 << 2));             /* track 3 at 3/2x: pages 1 and 3, page 3's end at 8 (F3) */
    RANGE(&p->trk[2], 2, 0, 7);
    p->trk[2].rate = QD_RATE_3_2;
    for (s = 0; s < 48; s += 4) {
        st = S(p, 2, s, QD_TRIG_NOTE, 70 + s / 4, 0, 0, 0);
        st->len = 2;
        if (s == 32)
            st->cond = qd_cond_ab(2, 2);
        if (s == 36)
            st->cond = qd_cond_ab(1, 2);
        if (s == 0)
            st->cond = QD_COND_N1ST;
    }
    PAGES(&p->trk[3], 0xE0);                            /* track 4 at 3/4x: pages 6 (1..2), 7 (16), 8 (4..6) */
    RANGE(&p->trk[3], 5, 0, 1);
    RANGE(&p->trk[3], 6, 15, 15);
    RANGE(&p->trk[3], 7, 3, 5);
    p->trk[3].rate = QD_RATE_3_4;
    S(p, 3, 80, QD_TRIG_NOTE, 36, 0, 0, 0)->cond = QD_COND_1ST;
    S(p, 3, 81, QD_TRIG_NOTE, 37, 0, 0, 0)->cond = QD_COND_N1ST;
    S(p, 3, 111, QD_TRIG_NOTE, 38, 0, 0, 0)->cond = qd_cond_ab(4, 4);
    S(p, 3, 115, QD_TRIG_NOTE, 39, 0, 0, 0)->len = 2;
    S(p, 3, 116, QD_TRIG_NOTE, 40, 0, 0, 0)->cond = qd_cond_ab(2, 3);
    S(p, 3, 117, QD_TRIG_NOTE, 41, 0, 0, 0)->len = 1;
    S(p, 3, 96, QD_TRIG_NOTE, 42, 0, 0, 0);             /* (page 7's step 1: outside its range) */
    pattern_test("p18-loops", "loops: page 3 alone (5..12) with 1:2 2:2 1:3 3:4; pages 2..4 at 2x; pages 1 and 3 "
                 "(1..8) at 3/2x; pages 6..8 at 3/4x with 1ST, !1ST, 4:4, 2:3", 10.0, 1);
}

/* p19: steps with no overrides play the track's defaults; a mix; a default changed while playing moves only the steps
 * that do not override it, from the first step resolved after the change */
static void p_defaults(void)
{
    TP *p = tp_new(0, 120);
    TD *d;
    TS *st;
    int s;
    d = &p->trk[0].def;                                 /* track 1: a default chord, strummed, half-step gates */
    d->nn = 3, d->notes[0] = 48, d->notes[1] = 55, d->notes[2] = 64;
    d->vel = 90, d->len = 2, d->micro = 3, d->strum = 2;
    p->trk[0].def2 = *d;                                /* .. at 4.03 s: another chord (four notes) and velocity */
    d = &p->trk[0].def2;
    d->nn = 4, d->notes[0] = 50, d->notes[1] = 57, d->notes[2] = 62, d->notes[3] = 69;
    d->vel = 60;
    p->trk[0].has2 = 1;
    for (s = 0; s < 16; s += 4)
        D(p, 0, s);                                     /* tapped: all the defaults */
    OV(D(p, 0, 2), QD_PM_VEL, 120);                /* the velocity its own */
    NOTES(D(p, 0, 6), 72, 0, 0, 0);                     /* the notes its own */
    st = D(p, 0, 10);                                   /* the length and the strum its own */
    OV(st, QD_PM_LEN, 6);
    OV(st, QD_PM_STRUM, 0);
    S(p, 0, 14, QD_TRIG_NOTE, 59, 0, 0, 0)->len = 1;    /* everything its own */
    d = &p->trk[1].def;                                 /* track 2 at 1/2x: a ratchet up, micro -4, chance 70 % */
    d->notes[0] = 40, d->vel = 110, d->len = 1, d->micro = -4, d->ratch = 3, d->ramp = QD_RAMP_UP, d->chance = 70;
    p->trk[1].def2 = *d;                                /* .. at 4.03 s: x2 down */
    p->trk[1].def2.ratch = 2, p->trk[1].def2.ramp = QD_RAMP_DOWN;
    p->trk[1].has2 = 1;
    p->trk[1].rate = QD_RATE_1_2;
    p->trk[1].length = 8;
    D(p, 1, 0);
    D(p, 1, 2);
    D(p, 1, 5);
    OV(D(p, 1, 3), QD_PM_COND, QD_COND_1ST);      /* its own condition, the default chance */
    st = D(p, 1, 6);                                    /* its own chance and ratchet */
    OV(st, QD_PM_CHANCE, 100);
    RATCH(st, 1, QD_RAMP_FLAT);
    NOTES(st, 43, 0, 0, 0);
    d = &p->trk[2].def;                                 /* track 3, MONO at 3/2x: the default trig is TIE */
    d->notes[0] = 45, d->len = 2, d->type = QD_TRIG_TIE;
    p->trk[2].limit = 1;
    p->trk[2].rate = QD_RATE_3_2;
    p->trk[2].length = 8;
    st = D(p, 2, 0);                                    /* a NOTE (its own type), then legato into the defaults */
    OV(st, QD_PM_TRIG, QD_TRIG_NOTE);
    NOTES(st, 36, 0, 0, 0);
    D(p, 2, 1);
    st = D(p, 2, 2);
    NOTES(st, 41, 0, 0, 0);
    OV(st, QD_PM_VEL, 120);
    OV(st, QD_PM_LEN, 4);
    st = D(p, 2, 4);
    OV(st, QD_PM_TRIG, QD_TRIG_NOTE);
    OV(st, QD_PM_LEN, 1);
    NOTES(st, 38, 0, 0, 0);
    d = &p->trk[3].def;                                 /* track 4: the default trig is LOCK */
    d->type = QD_TRIG_LOCK;
    st = D(p, 3, 0);                                    /* a NOTE chord held 8 steps, LOCK steps into it */
    OV(st, QD_PM_TRIG, QD_TRIG_NOTE);
    NOTES(st, 60, 64, 67, 0);
    OV(st, QD_PM_LEN, 10);
    st = D(p, 3, 2);
    LK(st, QD_QP_FREQ, 6000);
    LK(st, QD_QP_EDEC, 60);
    st = D(p, 3, 4);
    LK(st, QD_LK_PAN, -30);
    D(p, 3, 6);                                         /* (a LOCK with no locks) */
    d = &p->trk[4].def;                                 /* track 5: INF, micro -6, NOT 1ST by default */
    d->notes[0] = 70, d->len = QD_LEN_INF, d->micro = -6, d->cond = QD_COND_N1ST;
    D(p, 4, 0);
    D(p, 4, 8);
    st = D(p, 4, 12);
    OV(st, QD_PM_COND, QD_COND_ALWAYS);
    NOTES(st, 74, 0, 0, 0);
    R_DEF_AT = 32 * 5560;                               /* 4.0345 s: between the steps' resolutions on every track */
    pattern_test("p19-defaults", "track defaults: a strummed chord, velocity, length, micro (tapped steps; steps with "
                 "their own velocity, notes, length); ratchet and chance; TIE and LOCK as the default trig; INF and NOT "
                 "1ST; the chord, velocity and ratchet changed at 4.03 s", 8.0, 4242);
    R_DEF_AT = -1;
}

/* p20: sound locks to the pool: the drums 18..23 with p-locks over them (the p-lock wins), one pool sound on many
 * steps, factory sounds, the track's own sound */
static void p_pool(void)
{
    TP *p = tp_new(0, 110);
    TS *st;
    int s;
    p->trk[0].def.notes[0] = 33;                        /* track 1: KICK (A1) */
    for (s = 0; s < 16; s += 4) {
        st = D(p, 0, s);
        OV(st, QD_PM_SOUND, QD_SND_KICK);
        if (s == 8)
            LK(st, QD_QP_LEVEL, 90);                    /* (KICK's LEVEL is 127: the lock's 90 plays) */
    }
    p->trk[1].def.notes[0] = 55;                        /* track 2: SNARE, GHOST, CLAP */
    p->trk[1].def.len = 2;
    OV(D(p, 1, 4), QD_PM_SOUND, QD_SND_SNARE);
    st = D(p, 1, 12);
    OV(st, QD_PM_SOUND, QD_SND_SNARE);
    LK(st, QD_QP_FREQ, 7000);
    LK(st, QD_LK_PAN, -20);
    OV(D(p, 1, 10), QD_PM_SOUND, QD_SND_GHOST);
    st = D(p, 1, 14);
    OV(st, QD_PM_SOUND, QD_SND_CLAP);
    NOTES(st, 60, 61, 62, 0);                           /* the clap: three bursts, strummed */
    OV(st, QD_PM_STRUM, 2);
    p->trk[2].def.notes[0] = 96;                        /* track 3: HAT on every other step, OPEN HAT twice, the */
    p->trk[2].def.len = 1;                              /* track's own sound between */
    p->trk[2].limit = 4;
    for (s = 0; s < 16; s += 2) {
        st = D(p, 2, s);
        OV(st, QD_PM_SOUND, s == 6 || s == 14 ? QD_SND_OHAT : QD_SND_HAT);
        if (s == 14)
            LK(st, QD_LK_REVERB, 80);
        if (s == 10)
            LK(st, QD_QP_EDEC, 4);
    }
    D(p, 2, 3);
    NOTES(D(p, 2, 11), 84, 0, 0, 0);
    p->trk[3].def.notes[0] = 48;                        /* track 4: factory sounds from the pool, INIT, its own */
    p->trk[3].def.len = 3;
    OV(D(p, 3, 0), QD_PM_SOUND, 2);
    OV(D(p, 3, 2), QD_PM_SOUND, QD_SND_INIT);
    OV(D(p, 3, 4), QD_PM_SOUND, 9);
    D(p, 3, 8);
    st = D(p, 3, 12);
    OV(st, QD_PM_SOUND, QD_SND_NOISE);
    LK(st, QD_QP_RESO, 4000);
    pattern_test("p20-pool", "sound locks to the pool: KICK (one with LEVEL locked), SNARE (one with FREQ and PAN), "
                 "GHOST, CLAP strummed, HAT on 6 steps, OPEN HAT (reverb), factory sounds 2, 9, 17 (RESO), INIT, the "
                 "track's own", 4.0, 1);
}

/* p21: notes from scale degrees (qd_scale_note at build time; the reference maps them with its own tables) */
static void p_scales(void)
{
    TP *p = tp_new(0, 100);
    TS *st;
    int s;
    p->key_scale = QD_SCALE_DORIAN;                     /* the global key: D dorian */
    p->key_root = 2;
    p->trk[0].scale = QD_SCALE_MAJOR;                   /* track 1: C major, octave 4, degrees -4..11 */
    for (s = 0; s < 16; s++)
        DEG(D(p, 0, s), 4, 1, s - 4, 0, 0, 0);
    p->trk[0].def.len = 2;
    p->trk[1].scale = QD_SCALE_MIN_PENTA;               /* track 2: A minor pentatonic, octave 3, and a chord */
    p->trk[1].root = 9;
    {
        static const int DG[7] = {-6, -1, 0, 3, 7, 12, -11};
        for (s = 0; s < 7; s++)
            DEG(D(p, 1, s * 2), 3, 1, DG[s], 0, 0, 0);
        DEG(D(p, 1, 14), 3, 3, 0, 2, 4, 0);
    }
    p->trk[2].scale = QD_SCALE_WHOLE_TONE;              /* track 3 follows the global key (its own is ignored) */
    p->trk[2].root = 5;
    p->trk[2].keyg = 1;
    for (s = 0; s < 8; s++)
        DEG(D(p, 2, s), 4, 1, s - 3, 0, 0, 0);
    st = D(p, 2, 10);
    DEG(st, 4, 4, 0, 2, 4, 6);                          /* a dorian seventh chord */
    OV(st, QD_PM_LEN, 6);
    p->trk[3].scale = QD_SCALE_HARM_MINOR;              /* track 4 at 2x: E harmonic minor, octave 2, 7 down to -8 */
    p->trk[3].root = 4;
    p->trk[3].rate = QD_RATE_2;
    for (s = 0; s < 16; s++)
        DEG(D(p, 3, s), 2, 1, 7 - s, 0, 0, 0);
    p->trk[3].def.len = 2;
    p->trk[4].scale = QD_SCALE_BLUES;                   /* track 5: G blues, octave 5, -8..7 */
    p->trk[4].root = 7;
    for (s = 0; s < 16; s++)
        DEG(D(p, 4, s), 5, 1, s - 8, 0, 0, 0);
    p->trk[4].def.len = 1;
    p->trk[5].scale = QD_SCALE_CHROMATIC;               /* track 6 at 1/2x: chromatic, -12..3 */
    p->trk[5].rate = QD_RATE_1_2;
    for (s = 0; s < 16; s++)
        DEG(D(p, 5, s), 4, 1, s - 12, 0, 0, 0);
    p->trk[6].scale = QD_SCALE_LOCRIAN;                 /* track 7: B locrian at the edges: octave -1 below its root */
    p->trk[6].root = 11;                                /* (note 2), octave 8 (125, and 134 clamped to 127), a chord */
    DEG(D(p, 6, 0), -1, 1, -5, 0, 0, 0);
    DEG(D(p, 6, 4), 8, 1, 4, 0, 0, 0);
    DEG(D(p, 6, 8), 1, 2, -3, 3, 0, 0);
    DEG(D(p, 6, 12), 8, 1, 9, 0, 0, 0);
    pattern_test("p21-scales", "notes from scale degrees: C major -4..11; A minor pentatonic and a chord; the global "
                 "key (D dorian) followed, a seventh chord; E harmonic minor at 2x; G blues; chromatic at 1/2x; B "
                 "locrian low and high", 4.0, 1);
}

/* p22: solo: one track soloed, two, a soloed track that is also muted, the solo cleared */
static void p_solo(void)
{
    TP *p = tp_new(0, 120);
    int s;
    for (s = 0; s < 16; s++) {
        S(p, 0, s, QD_TRIG_NOTE, 48, 0, 0, 0)->len = 2;
        if (s % 2 == 0)
            S(p, 1, s, QD_TRIG_NOTE, 60, 0, 0, 0)->len = 2;
        S(p, 2, s, QD_TRIG_NOTE, 72, 0, 0, 0)->len = 1;
        if (s % 4 == 0)
            S(p, 3, s, QD_TRIG_NOTE, 36, 43, 0, 0)->len = 4;
        if (s % 3 == 0)
            S(p, 4, s, QD_TRIG_NOTE, 84, 0, 0, 0)->len = 1;
    }
    p->trk[2].mute = 1;                                 /* track 3 muted (the pattern's flag) */
    R_SOLO0 = 1 << 1;                                   /* track 2 alone */
    R_NSOLO = 3;
    R_SOLO_AT[0] = 32 * 1300, R_SOLO_M[0] = (1 << 0) | (1 << 3);   /* 0.94 s: tracks 1 and 4 */
    R_SOLO_AT[1] = 32 * 2700, R_SOLO_M[1] = (1 << 1) | (1 << 2);   /* 1.96 s: track 2, and 3 (muted: silent) */
    R_SOLO_AT[2] = 32 * 4100, R_SOLO_M[2] = 0;          /* 2.97 s: no solo: every track but the muted one */
    pattern_test("p22-solo", "solo: track 2 alone; tracks 1 and 4 (0.94 s); track 2 and the muted track 3 (1.96 s); "
                 "cleared (2.97 s)", 4.0, 1);
    R_SOLO0 = R_NSOLO = 0;
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
    CHECK(qd_step_type(&Q, 6, 121) == QD_TRIG_LOCK, "page copy keeps the trig type");
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
    qd_rstep_t r;
    uint8_t n[QD_NNOTE];
    int rr, n0 = fails, a, b, c;
    char buf[8];
    qd_pat_init(&P);
    qd_tdef_set(&P, 2, QD_PM_NOTE, 50);
    qd_tdef_set(&P, 2, QD_PM_VEL, 90);
    qd_tdef_set(&P, 2, QD_PM_LEN, 6);
    CHECK(sizeof(qd_step_t) == 18 && sizeof(qd_lock_t) == 4, "step 18 bytes (%d), lock 4 (%d)",
          (int)sizeof(qd_step_t), (int)sizeof(qd_lock_t));
    rr = qd_step_on(&P, 2, 5);
    st = qd_step(&P, 2, 5);
    qd_step_resolve(&P, 2, 5, &r);
    CHECK(rr == QD_OK && st->ovr == 0 && r.on && r.type == QD_TRIG_NOTE && r.notes[0] == 50 && r.vel == 90 &&
              r.len == 6 && r.chance == 100,
          "a tapped step has no overrides: it plays the track's defaults");
    qd_tdef_set(&P, 2, QD_PM_VEL, 70);
    qd_step_resolve(&P, 2, 5, &r);
    CHECK(r.vel == 70, "a default changed: the step follows (%d)", r.vel);
    {
        const uint8_t ch[4] = {67, 60, 64, 60};
        rr = qd_step_set_notes(&P, 2, 5, ch, 4);
    }
    CHECK(rr == QD_OK && qd_step_notes(&P, 2, 5, n) == 3 && n[0] == 60 && n[1] == 64 && n[2] == 67 &&
              st->note[1] == 4 && qd_step_overrides(st, QD_PM_NOTE) && !qd_step_overrides(st, QD_PM_VEL),
          "notes: ascending, no repeats, 2..4 as offsets; NOTE overridden, VEL not");
    {
        const uint8_t wide[2] = {30, 79};
        CHECK(qd_step_set_notes(&P, 2, 5, wide, 2) == QD_ENOTE, "an offset over +48: refused");
    }
    qd_step_resolve(&P, 2, 5, &r);
    CHECK(qd_step_set_ratchet(&P, 2, 5, 8, QD_RAMP_ACCENT) == QD_OK && (qd_step_resolve(&P, 2, 5, &r), r.ratch == 8) &&
              r.ramp == QD_RAMP_ACCENT && r.type == QD_TRIG_NOTE, "ratchet x8, accent");
    CHECK(qd_step_set_micro(&P, 2, 5, -24) == QD_EARG && qd_step_set_micro(&P, 2, 5, -23) == QD_OK &&
              st->micro == -23, "micro -23..23");
    CHECK(qd_step_set_strum(&P, 2, 5, 24) == QD_EARG && qd_step_set_strum(&P, 2, 5, 23) == QD_OK, "strum 0..23");
    CHECK(qd_step_set_chance(&P, 2, 5, 101) == QD_EARG && qd_step_set_chance(&P, 2, 5, 0) == QD_OK, "chance");
    CHECK(qd_step_set_sound(&P, 2, 5, QD_NSOUND + 1) == QD_EARG && qd_step_set_sound(&P, 2, 5, QD_NSOUND) == QD_OK,
          "sound 0..128 (a pool number)");
    CHECK(qd_step_set_len(&P, 2, 5, QD_LEN_INF) == QD_OK && qd_len_eighths(QD_LEN_INF) < 0, "INF");
    CHECK(qd_step_set_trig(&P, 2, 5, QD_TRIG_TIE) == QD_OK && qd_step_type(&P, 2, 5) == QD_TRIG_TIE &&
              (qd_step_resolve(&P, 2, 5, &r), r.ratch == 8), "the type changes, the rest stays");
    CHECK(qd_step_reset(&P, 2, 5, QD_PM_TRIG) == QD_OK && qd_step_type(&P, 2, 5) == QD_TRIG_NOTE,
          "the type's override cleared: the track's NOTE again");
    CHECK(qd_step_set_vel(&P, 9, 5, 100) == QD_EARG && qd_step_set_vel(&P, 2, 128, 100) == QD_EARG, "out of range");
    qd_lock_set(&P, 2, 5, QD_QP_FREQ, 1);
    CHECK(qd_step_set_trig(&P, 2, 5, QD_TRIG_NONE) == QD_OK && !qd_lock_count(&P, 2, 5) && !st->flags && !st->ovr &&
              !st->note[0], "NONE: the step and its locks gone");
    CHECK(qd_lock_set(&P, 2, 6, QD_QP_FREQ, 3000) == QD_OK && qd_step_type(&P, 2, 6) == QD_TRIG_LOCK,
          "a lock on an empty step makes a LOCK step");
    CHECK(qd_lock_set(&P, 2, 6, QD_LK_N, 0) == QD_EARG, "lock id out of range");
    {
        int16_t v;
        qd_lock_set(&P, 2, 6, QD_LK_PAN, -100);
        CHECK(qd_lock_get(&P, 2, 6, QD_LK_PAN, &v) && v == -64, "a lock is clamped to its range (%d)", v);
    }
    CHECK(qd_step_set_vel(&P, 2, 9, 33) == QD_OK && qd_step_is_on(qd_step(&P, 2, 9)) &&
              qd_step_type(&P, 2, 9) == QD_TRIG_NOTE, "a value set on an empty step turns it on (the track's type)");
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
    CHECK(qd_rate_ticks(QD_RATE_3_4) == 4 * QD_TPS / 3 && qd_rate_ticks(QD_RATE_4_3) == 3 * QD_TPS / 4 &&
              qd_rate_ticks(QD_RATE_2_3) == 3 * QD_TPS / 2 && qd_rate_ticks(QD_RATE_1_8) == 8 * QD_TPS,
          "P1.3b's rates in ticks");
    {   /* P1.3c: the 21 codes, base x type, each whole and divisible as the engine needs */
        static const double SIXTEENTHS[7] = {16, 8, 4, 2, 1, 0.5, 0.25};
        int code, ok = 1, d;
        for (code = 0; code < QD_NRATE; code++) {
            const int b = qd_rate_val(code), ty = qd_rate_type(code);
            const double want = SIXTEENTHS[b] * (ty == 1 ? 1.5 : ty == 2 ? 2.0 / 3.0 : 1.0) * QD_TPS;
            const int64_t l = qd_rate_ticks(code);
            ok &= qd_rate_make(b, ty) == code && (double)l == want;
            for (d = 1; d <= 8; d++)
                ok &= l % d == 0;
            ok &= l % 24 == 0 && l % 50 == 0;
        }
        CHECK(ok && qd_rate_ticks(QD_RATE_CODE(6, 1)) == 37800 && qd_rate_ticks(QD_RATE_CODE(6, 2)) == 16800 &&
                  qd_rate_ticks(QD_RATE_CODE(0, 1)) == 24 * QD_TPS,
              "21 rates (7 bases x normal / dotted / triplet): whole ticks, divisible by 1..8, 24 and 50");
        CHECK(QD_RATE_1 == QD_RATE_CODE(4, 0) && QD_RATE_2_3 == QD_RATE_CODE(4, 1) && QD_RATE_3_2 == QD_RATE_CODE(4, 2) &&
                  QD_RATE_3_4 == QD_RATE_CODE(3, 2) && QD_RATE_4_3 == QD_RATE_CODE(5, 1),
              "P1.3b's nine rates are base x type codes");
    }
    {
        const int64_t m0 = qd_pat_master_ticks(&P);
        qd_track_set_length(&P, 4, 64);
        qd_tset_set_rate(&P.trk[4], QD_RATE_1_2);
        CHECK(m0 == 16 * QD_TPS && qd_pat_master_ticks(&P) == 128 * QD_TPS, "master auto: the longest track");
        P.master = 32;
        CHECK(qd_pat_master_ticks(&P) == 32 * QD_TPS, "master 32");
        P.master = QD_MASTER_INF;
        CHECK(qd_pat_master_ticks(&P) == 0, "master INF");
    }
    /* pages */
    qd_track_set_length(&P, 1, 24);
    CHECK(P.trk[1].loop == 3 && P.trk[1].pend[0] == 15 && P.trk[1].pend[1] == 7 && qd_track_len(&P.trk[1]) == 24 &&
              qd_track_loop_step(&P.trk[1], 20) == 20 && qd_track_loop_step(&P.trk[1], 24) == 0,
          "length 24: page 1 1..16, page 2 1..8");
    qd_page_set_loop(&P, 1, (1 << 0) | (1 << 2));
    qd_page_set_range(&P, 1, 2, 4, 9);
    CHECK(qd_track_len(&P.trk[1]) == 22 && qd_track_loop_step(&P.trk[1], 16) == 36 &&
              qd_track_loop_step(&P.trk[1], 21) == 41 && qd_track_step_pos(&P.trk[1], 40) == 20 &&
              qd_track_step_pos(&P.trk[1], 35) == -1 && qd_track_step_pos(&P.trk[1], 20) == -1,
          "pages 1 and 3 (5..10): 22 steps, page 3's step 5 after page 1");
    qd_page_set_loop(&P, 1, 0);
    CHECK(qd_track_loop(&P.trk[1]) == 1 && qd_page_set_range(&P, 1, 0, 9, 3) == QD_EARG, "the loop never empty; "
          "start <= end");
    printf("  %-4s editing API       defaults and overrides, notes as offsets, every field's range, NONE, LOCK by a "
           "lock, 44 conditions, rates, master, pages\n", fails == n0 ? "ok" : "FAIL");
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

/* track t of P as it is (its steps, its locks, its settings): for the undo checks */
typedef struct {
    qd_step_t step[QD_NSTEP];
    qd_lock_t lk[QD_NLOCK];
    int nlk;
    qd_tset_t ts;
} snap_t;
static void snap(const qd_pattern_t *P, int t, snap_t *s)
{
    memset(s, 0, sizeof *s);
    qd_steps_get(P, t, 0, QD_NSTEP, s->step, s->lk, &s->nlk);
    s->ts = P->trk[t];
}
static int snap_eq(const snap_t *a, const snap_t *b)
{
    return a->nlk == b->nlk && !memcmp(a->step, b->step, sizeof a->step) &&
           !memcmp(a->lk, b->lk, (size_t)a->nlk * sizeof a->lk[0]) && !memcmp(&a->ts, &b->ts, sizeof a->ts);
}

/* clip.h: the step and page clipboards, cut, clears, undo (and redo), a full pool */
static void u_clip(void)
{
    static qd_pattern_t P, Q, Q0;
    static qd_clip_t C;
    static snap_t A, B;
    const uint8_t ch[3] = {60, 64, 67};
    int r, s, i, n0 = fails;
    int16_t v = 0;
    qd_pat_init(&P);
    qd_clip_init(&C);
    CHECK(qd_clip_paste_step(&C, &P, 0, 0, 0) == QD_EARG && qd_clip_paste_page(&C, &P, 0, 0, 0) == QD_EARG &&
              !qd_clip_can_undo(&C, 0, 0), "an empty clipboard pastes nothing; nothing to undo");
    /* a step with a chord, its own velocity and two locks: copied to another track, pasted twice */
    qd_step_set_notes(&P, 0, 3, ch, 3);
    qd_step_set_vel(&P, 0, 3, 77);
    qd_lock_set(&P, 0, 3, QD_QP_FREQ, 4000);
    qd_lock_set(&P, 0, 3, QD_LK_PAN, -20);
    r = qd_clip_copy_step(&C, &P, 0, 3);
    CHECK(r == QD_OK && C.st.valid && C.st.nlk == 2, "copy a step: its two locks (%d)", C.st.nlk);
    for (s = 20; s <= 21; s++) {
        r = qd_clip_paste_step(&C, &P, 0, 5, s);
        CHECK(r == QD_OK && !memcmp(qd_step(&P, 5, s), qd_step(&P, 0, 3), sizeof(qd_step_t)) &&
                  qd_lock_count(&P, 5, s) == 2 && qd_lock_get(&P, 5, s, QD_QP_FREQ, &v) && v == 4000 &&
                  qd_lock_get(&P, 5, s, QD_LK_PAN, &v) && v == -20 && qd_pat_check(&P),
              "paste onto track 6 step %d: the step, its overrides and its locks", s + 1);
    }
    CHECK(qd_step_overrides(qd_step(&P, 5, 21), QD_PM_VEL) && !qd_step_overrides(qd_step(&P, 5, 21), QD_PM_LEN),
          "a pasted step keeps which parameters it overrides");
    /* a page: its steps, locks, start and end; pasted onto another track's page 4, which joins the loop */
    qd_page_set_range(&P, 0, 0, 2, 9);
    qd_step_on(&P, 0, 9);
    qd_lock_set(&P, 0, 12, QD_QP_EDEC, 50);             /* (a LOCK step) */
    r = qd_clip_copy_page(&C, &P, 0, 0);
    CHECK(r == QD_OK && C.pg.valid && C.pg.start == 2 && C.pg.end == 9 && C.pg.nlk == 3, "copy a page: 3 locks, "
          "start 3, end 10");
    snap(&P, 6, &A);
    r = qd_clip_paste_page(&C, &P, 0, 6, 3);
    CHECK(r == QD_OK && P.trk[6].pstart[3] == 2 && P.trk[6].pend[3] == 9 && qd_track_loop(&P.trk[6]) == (1 | 8) &&
              !memcmp(&P.step[6][48], &P.step[0][0], 16 * sizeof(qd_step_t)) && qd_lock_count(&P, 6, 51) == 2 &&
              qd_lock_count(&P, 6, 60) == 1 && qd_step_type(&P, 6, 60) == QD_TRIG_LOCK && qd_pat_check(&P),
          "paste a page onto track 7 page 4: steps, locks, start 3 end 10, the page in the loop (pages 1 and 4)");
    CHECK(qd_track_len(&P.trk[6]) == 16 + 8, "track 7's loop: 16 + 8 steps (%d)", qd_track_len(&P.trk[6]));
    snap(&P, 6, &B);
    r = qd_clip_undo(&C, &P, 0, 6);
    {
        static snap_t X;
        snap(&P, 6, &X);
        CHECK(r == 1 && snap_eq(&X, &A) && qd_track_loop(&P.trk[6]) == 1 && qd_pat_check(&P),
              "undo the page paste: the page empty, the loop page 1 again");
        r = qd_clip_undo(&C, &P, 0, 6);
        snap(&P, 6, &X);
        CHECK(r == 1 && snap_eq(&X, &B) && qd_pat_check(&P), "undo again: the paste again (redo)");
    }
    /* cut: copied, then cleared; undo restores it; undo again cuts it again */
    snap(&P, 0, &A);
    r = qd_clip_cut_step(&C, &P, 0, 0, 3);
    CHECK(r == QD_OK && !qd_step_is_on(qd_step(&P, 0, 3)) && !qd_lock_count(&P, 0, 3) && C.st.nlk == 2 &&
              C.st.step.vel == 77 && qd_pat_check(&P), "cut: the step cleared, in the clipboard with its locks");
    snap(&P, 0, &B);
    r = qd_clip_undo(&C, &P, 0, 0);
    {
        static snap_t X;
        snap(&P, 0, &X);
        CHECK(r == 1 && snap_eq(&X, &A) && qd_lock_count(&P, 0, 3) == 2, "undo the cut: the step and its locks back");
        r = qd_clip_undo(&C, &P, 0, 0);
        snap(&P, 0, &X);
        CHECK(r == 1 && snap_eq(&X, &B), "undo again: cut again (redo)");
        qd_clip_undo(&C, &P, 0, 0);
        snap(&P, 0, &X);
        CHECK(snap_eq(&X, &A), "and back");
    }
    r = qd_clip_paste_step(&C, &P, 0, 0, 4);            /* the cut step pasted elsewhere */
    CHECK(r == QD_OK && qd_lock_count(&P, 0, 4) == 2 && P.step[0][4].vel == 77, "paste the cut step");
    /* clear a page: only its steps; undo */
    qd_step_set_vel(&P, 0, 40, 33);
    snap(&P, 0, &A);
    r = qd_clip_clear_page(&C, &P, 0, 0, 0);
    for (s = 0, i = 0; s < 16; s++)
        i += qd_step_is_on(qd_step(&P, 0, s)) || qd_lock_count(&P, 0, s);
    CHECK(r == QD_OK && !i && qd_step_is_on(qd_step(&P, 0, 40)) && P.trk[0].pstart[0] == 2 && qd_pat_check(&P),
          "clear page 1: its steps and locks gone, page 3's step stays, the range stays");
    r = qd_clip_undo(&C, &P, 0, 0);
    snap(&P, 0, &B);
    CHECK(r == 1 && snap_eq(&A, &B), "undo the page clear");
    /* clear a track; an undo for another pattern index does nothing; the right one restores */
    r = qd_clip_clear_track(&C, &P, 0, 0);
    for (s = 0, i = 0; s < QD_NSTEP; s++)
        i += qd_step_is_on(qd_step(&P, 0, s));
    CHECK(r == QD_OK && !i && !P.lock_n[0] && qd_pat_check(&P), "clear the track: no steps, no locks");
    memcpy(&Q, &P, sizeof Q);
    r = qd_clip_undo(&C, &P, 1, 0);
    CHECK(r == 0 && !qd_clip_can_undo(&C, 1, 0) && qd_clip_can_undo(&C, 0, 0) && !memcmp(&P, &Q, sizeof P),
          "undo for another pattern (B01's level is A01's): nothing");
    CHECK(qd_clip_undo(&C, &P, 0, 7) == 0, "undo on a track never edited: nothing");
    r = qd_clip_undo(&C, &P, 0, 0);
    snap(&P, 0, &B);
    CHECK(r == 1 && snap_eq(&A, &B) && qd_pat_check(&P), "undo the track clear");
    {   /* clear steps */
        const int ss[2] = {4, 40};
        r = qd_clip_clear_steps(&C, &P, 0, 0, ss, 2);
        CHECK(r == QD_OK && !qd_step_is_on(qd_step(&P, 0, 4)) && !qd_step_is_on(qd_step(&P, 0, 40)) &&
                  qd_step_is_on(qd_step(&P, 0, 9)) && qd_pat_check(&P), "clear two steps");
        qd_clip_undo(&C, &P, 0, 0);
        snap(&P, 0, &B);
        CHECK(snap_eq(&A, &B), "undo the steps' clear");
    }
    /* a full lock pool: a paste with locks returns QD_EFULL and changes nothing */
    qd_pat_init(&Q);
    for (i = 0; Q.nlock < QD_NLOCK; i++)
        qd_lock_set(&Q, 1 + i % 7, (i / 7) % 128, (i / (7 * 128)) % QD_LK_N, i);
    memcpy(&Q0, &Q, sizeof Q0);
    r = qd_clip_paste_step(&C, &Q, 0, 0, 0);
    CHECK(r == QD_EFULL && !memcmp(&Q, &Q0, sizeof Q), "a step paste onto a full pool: QD_EFULL (%d), nothing changed",
          r);
    r = qd_clip_paste_page(&C, &Q, 0, 0, 2);
    CHECK(r == QD_EFULL && !memcmp(&Q, &Q0, sizeof Q), "a page paste onto a full pool: QD_EFULL (%d), the steps, the "
          "range and the loop unchanged", r);
    qd_clip_copy_step(&C, &P, 0, 9);                    /* (a step without locks still pastes) */
    r = qd_clip_paste_step(&C, &Q, 0, 0, 0);
    CHECK(r == QD_OK && qd_step_is_on(qd_step(&Q, 0, 0)) && qd_pat_check(&Q), "a step with no locks: pasted (%d)", r);
    {   /* P1.3c: a group (an F1 / F2 hold): a cut, a paste, another paste and a page clear are one undo level */
        static qd_pattern_t G, G0;
        qd_pat_init(&G);
        qd_step_set_vel(&G, 3, 1, 33);
        qd_step_on(&G, 3, 5);
        qd_lock_set(&G, 3, 5, QD_QP_HARM, 200);
        memcpy(&G0, &G, sizeof G0);
        qd_clip_group(&C, 1);
        qd_clip_cut_step(&C, &G, 7, 3, 1);
        qd_clip_paste_step(&C, &G, 7, 3, 9);
        qd_clip_paste_step(&C, &G, 7, 3, 10);
        qd_clip_clear_page(&C, &G, 7, 3, 1);
        qd_clip_group(&C, 0);
        CHECK(!qd_step_is_on(qd_step(&G, 3, 1)) && qd_step(&G, 3, 10)->vel == 33, "grouped: cut, two pastes, done");
        CHECK(qd_clip_undo(&C, &G, 7, 3) == 1 && !memcmp(G.step[3], G0.step[3], sizeof G.step[3]) &&
                  G.nlock == G0.nlock && qd_pat_check(&G),
              "one undo takes back the whole group (as before the first edit)");
        qd_clip_group(&C, 1);
        qd_clip_copy_step(&C, &G, 3, 5);
        qd_clip_paste_step(&C, &G, 7, 3, 6);
        CHECK(qd_clip_undo(&C, &G, 7, 3) == 1 && !qd_step_is_on(qd_step(&G, 3, 6)), "an undo inside a group ..");
        qd_clip_paste_step(&C, &G, 7, 3, 7);
        CHECK(qd_clip_undo(&C, &G, 7, 3) == 1 && !qd_step_is_on(qd_step(&G, 3, 7)) && !qd_step_is_on(qd_step(&G, 3, 6)),
              ".. ends its level: the next edit keeps a new one");
        qd_clip_group(&C, 0);
    }
    printf("  %-4s clipboard         step copy / paste (each again) between tracks with locks, page copy / paste "
           "(range, loop), cut, clears, undo / redo per track and pattern, a full pool, groups\n",
           fails == n0 ? "ok" : "FAIL");
}

/* scale.h against the test's own tables */
static void u_scale(void)
{
    int sc, root, oct, deg, note, bad = 0, n0 = fails, sz = 0, k;
    for (sc = 0; sc < QD_NSCALE; sc++) {
        sz += qd_scale_size(sc) == R_SC[sc].n;
        CHECK(!strcmp(qd_scale_name(sc), R_SC[sc].name) && !strcmp(qd_scale_short(sc), R_SC[sc].shrt),
              "scale %d's name %s (%s)", sc, qd_scale_name(sc), qd_scale_short(sc));
    }
    CHECK(sz == QD_NSCALE && qd_scale_size(QD_SCALE_CHROMATIC) == 12 && qd_scale_size(QD_SCALE_MAJOR) == 7 &&
              qd_scale_size(QD_SCALE_MAJ_PENTA) == 5 && qd_scale_size(QD_SCALE_BLUES) == 6, "the sizes: 12, 7, 5, 6");
    CHECK(!strcmp(qd_scale_name(-1), "CHROMATIC") && !strcmp(qd_scale_name(QD_NSCALE), "CHROMATIC"),
          "a scale out of range: chromatic");
    for (sc = 0; sc < QD_NSCALE; sc++)                 /* every scale, root, octave -1..9, degree -40..40 */
        for (root = 0; root < 12; root++)
            for (oct = -1; oct <= 9; oct++)
                for (deg = -40; deg <= 40; deg++)
                    if (qd_scale_note(sc, root, oct, deg) != r_scale_note(sc, root, oct, deg) && bad++ < 3)
                        printf("    scale %s root %d oct %d degree %d: %d, the test's %d\n", R_SC[sc].name, root, oct,
                               deg, qd_scale_note(sc, root, oct, deg), r_scale_note(sc, root, oct, deg));
    CHECK(!bad, "qd_scale_note: every scale, root, octave, degree (negative too) as the test's tables (%d differ)", bad);
    CHECK(qd_scale_note(QD_SCALE_MAJOR, 0, 4, 0) == 60 && qd_scale_note(QD_SCALE_MAJOR, 0, 4, 7) == 72 &&
              qd_scale_note(QD_SCALE_MAJOR, 0, 4, -1) == 59 && qd_scale_note(QD_SCALE_MAJOR, 0, 4, -7) == 48 &&
              qd_scale_note(QD_SCALE_MAJOR, 0, 4, -8) == 47 && qd_scale_note(QD_SCALE_MINOR, 9, 3, 2) == 60 &&
              qd_scale_note(QD_SCALE_MIN_PENTA, 9, 3, -1) == 55 && qd_scale_note(QD_SCALE_CHROMATIC, 0, 4, -13) == 47,
          "C major 0 / 7 / -1 / -7 / -8 = C4 C5 B3 C3 B2; A minor 2 = C4; A minor penta -1 = G3; chromatic -13 = B2");
    CHECK(qd_scale_note(QD_SCALE_MAJOR, 0, 9, 20) == 127 && qd_scale_note(QD_SCALE_MAJOR, 0, -1, -3) == 0,
          "clamped 0..127");
    for (sc = 0, bad = 0; sc < QD_NSCALE; sc++)         /* has / step / snap against the test's own walk */
        for (root = 0; root < 12; root++)
            for (note = 0; note < 128; note++) {
                int up = note, dn = note, sn = note;
                bad += qd_scale_has(sc, root, note) != r_scale_has(sc, root, note);
                for (k = 1; k <= 12; k++)
                    if (note + k <= 127 && r_scale_has(sc, root, note + k)) {
                        up = note + k;
                        break;
                    }
                for (k = 1; k <= 12; k++)
                    if (note - k >= 0 && r_scale_has(sc, root, note - k)) {
                        dn = note - k;
                        break;
                    }
                while (sn > 0 && !r_scale_has(sc, root, sn))
                    sn--;
                bad += qd_scale_step(sc, root, note, 1) != up;
                bad += qd_scale_step(sc, root, note, -1) != dn;
                bad += qd_scale_snap(sc, root, note) != sn;
            }
    CHECK(!bad, "qd_scale_has / qd_scale_step (up, down) / qd_scale_snap: every scale, root, note (%d differ)", bad);
    CHECK(qd_scale_step(QD_SCALE_MAJOR, 0, 60, 1) == 62 && qd_scale_step(QD_SCALE_MAJOR, 0, 64, 1) == 65 &&
              qd_scale_step(QD_SCALE_MAJOR, 0, 61, -1) == 60 && qd_scale_step(QD_SCALE_MAJOR, 0, 127, 1) == 127 &&
              qd_scale_snap(QD_SCALE_MAJOR, 0, 66) == 65 && qd_scale_snap(QD_SCALE_MIN_PENTA, 9, 59) == 57,
          "C major: up from C4 D4, from E4 F4, down from C#4 C4, none above 127; snap F#4 -> F4; A penta B3 -> A3");
    {
        static qd_project_t *pr = &PROJ;
        qd_pattern_t *P = &pr->pat[3];
        int s2, r2;
        qd_pat_init(P);
        pr->key_scale = QD_SCALE_LYDIAN;
        pr->key_root = 5;
        P->trk[2].scale = QD_SCALE_MAJOR;
        P->trk[2].root = 14;
        qd_track_key(pr, P, 2, &s2, &r2);
        CHECK(s2 == QD_SCALE_MAJOR && r2 == 2, "a track's own key (root 14 -> D): %d %d", s2, r2);
        P->trk[2].key_global = 1;
        qd_track_key(pr, P, 2, &s2, &r2);
        CHECK(s2 == QD_SCALE_LYDIAN && r2 == 5, "a track following the global key: %d %d", s2, r2);
        qd_track_key(0, P, 2, &s2, &r2);
        CHECK(s2 == QD_SCALE_MAJOR && r2 == 2, "(no project: its own)");
        P->trk[3].scale = 99;
        qd_track_key(pr, P, 3, &s2, &r2);
        CHECK(s2 == QD_SCALE_CHROMATIC && r2 == 0, "a scale out of range: chromatic");
        qd_project_init(pr);
    }
    printf("  %-4s scales            %d scales' names and sizes, qd_scale_note (every degree, octave, root; negative "
           "degrees), step, snap, the track's key\n", fails == n0 ? "ok" : "FAIL", QD_NSCALE);
}

/* sounds.h: the pool's first 23 sounds */
static void u_sounds(void)
{
    static const char *const DRUM[6] = {"KICK", "SNARE", "CLAP", "GHOST", "HAT", "OPEN HAT"};
    int16_t want[QD_QUAD_NP], init[QD_QUAD_NP];
    int k, i, bad = 0, n0 = fails;
    rig_reset();
    CHECK(PROJ.nsound == 23 && QD_SND_FIRST_FREE == 24 && QD_SND_KICK == 18 && QD_SND_OHAT == 23 &&
              QD_SND_NOISE == 17, "23 sounds; KICK 18 .. OPEN HAT 23 (%d)", PROJ.nsound);
    CHECK(!strcmp(qd_sound_name(&PROJ, 1), "INIT") && !strcmp(qd_sound_name(&PROJ, 2), "EP") &&
              !strcmp(qd_sound_name(&PROJ, 17), "NOISE-ISH"), "1 INIT, 2 EP, 17 NOISE-ISH");
    for (k = 1; k <= 17; k++)
        bad += strcmp(qd_sound_name(&PROJ, k), qd_quad_preset_name(k - 1)) != 0;
    for (k = 0; k < 6; k++)
        bad += strcmp(qd_sound_name(&PROJ, 18 + k), DRUM[k]) != 0;
    CHECK(!bad, "the names: the presets', then KICK, SNARE, CLAP, GHOST, HAT, OPEN HAT (%d differ)", bad);
    for (k = 1, bad = 0; k <= 23; k++) {
        r_pool_patch(k, want);
        if (memcmp(qd_sound_patch(&PROJ, k), want, sizeof want) && bad++ < 3)
            printf("    sound %d (%s): its patch is not the test's\n", k, qd_sound_name(&PROJ, k));
    }
    CHECK(!bad, "every patch: 1..17 the presets, 18..23 a preset with sounds.c's values (%d differ)", bad);
    CHECK(!R_POOL_CLAMPED, "the drums' values are within their ranges (%d clamped)", R_POOL_CLAMPED);
    qd_quad_preset_patch(0, init);
    memcpy(want, qd_sound_patch(&PROJ, QD_SND_KICK), sizeof want);
    for (i = 0, bad = 0; i < QD_QUAD_NP; i++) {
        const int listed = i == QD_QP_EDEC || i == QD_QP_ESUS || i == QD_QP_EREL || i == QD_QP_ALEV ||
                           i == QD_QP_ADEC || i == QD_QP_AEND || i == QD_QP_RA || i == QD_QP_LEVEL ||
                           (i >= QD_QP_LFO(0, 0) && i < QD_QP_LFO(1, 0));
        bad += !listed && want[i] != init[i];
    }
    CHECK(!bad && want[QD_QP_EDEC] == 32 && want[QD_QP_ESUS] == 0 && want[QD_QP_LEVEL] == 127 &&
              want[QD_QP_LFO(0, QD_QL_DEST)] == 1 && want[QD_QP_LFO(0, QD_QL_WAVE)] == 5 && want[QD_QP_RA] == 8,
          "KICK: INIT with AMP DEC 32 SUS 0, the pitch LFO (EXP, HALF), A LEV 60, RATIO A 2.00, LEVEL 127 (%d others)",
          bad);
    {
        const int16_t *sn = qd_sound_patch(&PROJ, QD_SND_SNARE), *hat = qd_sound_patch(&PROJ, QD_SND_HAT);
        qd_quad_preset_patch(QD_SND_NOISE - 1, init);
        CHECK(sn[QD_QP_FTYPE] == QD_QF_LP12 && sn[QD_QP_LEVEL] == 120 && hat[QD_QP_FTYPE] == init[QD_QP_FTYPE] &&
                  hat[QD_QP_FREQ] == 9500, "SNARE: NOISE-ISH through LP12, LEVEL 120; HAT: NOISE-ISH's own filter type");
    }
    CHECK(!strcmp(qd_sound_name(&PROJ, 0), "") && !strcmp(qd_sound_name(&PROJ, 24), "") &&
              qd_sound_patch(&PROJ, 0) == qd_sound_patch(&PROJ, 1) && qd_sound_patch(&PROJ, 99) == PROJ.sound[0].patch,
          "out of the pool: no name, INIT's patch");
    qd_quad_preset_patch(3, want);
    k = qd_sound_add(&PROJ, "A NAME TOO LONG", want);
    CHECK(k == 24 && PROJ.nsound == 24 && !strcmp(qd_sound_name(&PROJ, 24), "A NAME TOO ") &&
              !memcmp(qd_sound_patch(&PROJ, 24), want, sizeof want), "add: number 24, the name cut to 11 (%s)",
          qd_sound_name(&PROJ, 24));
    while (qd_sound_add(&PROJ, "X", want))
        ;
    CHECK(PROJ.nsound == QD_NSOUND && !qd_sound_add(&PROJ, "Y", want), "the pool full at %d: 0", QD_NSOUND);
    rig_reset();
    printf("  %-4s sound pool        23 sounds, their names, the presets, the drums as sounds.c makes them, KICK = INIT "
           "with its values, add, full\n", fails == n0 ? "ok" : "FAIL");
}

/* p23 (P1.3c): five notes a step: chords of five (overridden, strummed, with a ratchet, offsets to +48), a track whose
 * default chord is five notes (tapped steps; changed while playing to another five), five scale degrees in D dorian,
 * a TIE of five on a POLY track */
static void p_five(void)
{
    TP *p = tp_new(0, 118);
    TS *st;
    TD *d;
    int s;
    p->trk[0].limit = 12;
    p->trk[0].length = 32;
    NOTE5(S(p, 0, 0, QD_TRIG_NOTE, 48, 52, 55, 59), 62)->len = 6;   /* Cmaj9, together */
    st = NOTE5(S(p, 0, 6, QD_TRIG_NOTE, 50, 53, 57, 60), 64);       /* Dm9, strummed */
    st->strum = 5;
    st->len = 6;
    st = NOTE5(S(p, 0, 12, QD_TRIG_NOTE, 43, 47, 50, 53), 57);      /* G9, a ratchet x2 */
    st->ratch = 2;
    st->ramp = QD_RAMP_DOWN;
    st->len = 4;
    st = NOTE5(S(p, 0, 18, QD_TRIG_NOTE, 36, 48, 60, 72), 84);      /* offsets to +48 */
    st->strum = 3;
    st->micro = 6;
    NOTE5(S(p, 0, 24, QD_TRIG_NOTE, 60, 62, 64, 65), 67)->len = 2;   /* five steps of a scale */
    d = &p->trk[1].def;                                 /* track 2: a five-note default, changed at 3.0 s */
    d->nn = 5;
    d->notes[0] = 45, d->notes[1] = 48, d->notes[2] = 52, d->notes[3] = 55, d->notes[4] = 59;   /* Am9 */
    d->len = 3;
    d->strum = 2;
    p->trk[1].def2 = *d;
    p->trk[1].def2.notes[0] = 41, p->trk[1].def2.notes[1] = 45, p->trk[1].def2.notes[2] = 48;
    p->trk[1].def2.notes[3] = 52, p->trk[1].def2.notes[4] = 55;   /* Fmaj9 */
    p->trk[1].has2 = 1;
    p->trk[1].limit = 10;
    for (s = 0; s < 16; s += 4)
        D(p, 1, s);
    NOTES(D(p, 1, 10), 57, 0, 0, 0);                    /* one step its own single note */
    p->trk[2].scale = QD_SCALE_DORIAN;                  /* track 3: five degrees of D dorian */
    p->trk[2].root = 2;
    p->trk[2].limit = 10;
    st = D(p, 2, 2);
    DEG(st, 3, 4, 0, 2, 4, 6);
    DEG5(st, 8);
    OV(st, QD_PM_LEN, 6);
    st = D(p, 2, 10);
    DEG(st, 3, 4, -2, 1, 3, 5);
    DEG5(st, 7);
    p->trk[3].limit = 10;                               /* track 4: NOTE then a TIE of five on a POLY track */
    st = NOTE5(S(p, 3, 0, QD_TRIG_NOTE, 64, 67, 71, 74), 78);
    st->len = 3;
    NOTE5(S(p, 3, 2, QD_TRIG_TIE, 64, 67, 71, 74), 78)->len = 4;
    R_DEF_AT = 32 * 4134;                               /* 3.0 s (block-aligned) */
    pattern_test("p23-five", "five notes a step: Cmaj9 together, Dm9 strummed, G9 ratcheted, offsets to +48; a five-note "
                 "default (Am9 -> Fmaj9 at 3.0 s); five degrees of D dorian; a TIE of five", 6.0, 2323);
    R_DEF_AT = -1;
}

/* p24 (P1.3c): the launch modes. A (32 steps: master 32) plays; B queued at the next BAR; A again at the next BEAT;
 * B at the END of the master length; A NOW (the next 16th). The reference's change points are reckoned from the
 * playing pattern's start by hand */
static void p_launch(void)
{
    TP *a = tp_new(0, 120), *b;
    rseg_t seg[8];
    int s, nseg = 0, n0 = fails;
    for (s = 0; s < 32; s += 2)
        S(a, 0, s, QD_TRIG_NOTE, 48 + (s % 8), 0, 0, 0)->len = 1;
    a->trk[0].length = 32;
    S(a, 1, 31, QD_TRIG_NOTE, 72, 0, 0, 0);
    b = tp_new(1, 120);
    for (s = 0; s < 16; s++)
        S(b, 0, s, QD_TRIG_NOTE, 60 + s % 5, 0, 0, 0)->len = 1;
    S(b, 1, 0, QD_TRIG_NOTE, 67, 0, 0, 0)->micro = -12;
    rig_reset();
    build(a, 0);
    build(b, 1);
    qd_model_tempo(&MODEL, 120);
    qd_seq_seed(&SEQ, 1);
    qd_seq_select(&SEQ, 0, 0);
    qd_seq_play(&SEQ);
    {   /* (block-aligned samples: 0.700 s, 2.611 s, 4.405 s, 7.599 s) */
        const uint64_t q[4] = {30880, 115168, 194272, 335136};
        const int pat[4] = {1, 0, 1, 0}, mode[4] = {QD_LAUNCH_BAR, QD_LAUNCH_BEAT, QD_LAUNCH_END, QD_LAUNCH_NOW};
        int64_t o = 0, x;
        int k;
        const TP *tps[2] = {a, b};
        const int64_t master[2] = {32 * QD_TPS, 16 * QD_TPS};
        int cur = 0;
        while (SEQ.now < 9 * 44100) {
            for (k = 0; k < 4; k++)
                if (SEQ.now == q[k])
                    qd_seq_select_at(&SEQ, pat[k], mode[k]);
            rig_block(0);
        }
        for (k = 0; k < 4; k++) {                       /* the reference: each change point by hand */
            const int64_t t = (int64_t)((q[k] * 8 * 120000 + QD_SUB - 1) / QD_SUB);   /* the tick (ceil) */
            const int64_t unit = mode[k] == QD_LAUNCH_BAR ? 16 * QD_TPS : mode[k] == QD_LAUNCH_BEAT ? 4 * QD_TPS
                               : mode[k] == QD_LAUNCH_NOW ? QD_TPS : master[cur];
            if (mode[k] == QD_LAUNCH_NOW)
                x = (t + QD_TPS - 1) / QD_TPS * QD_TPS;
            else if (mode[k] == QD_LAUNCH_END)
                x = o + ((t - o) / unit + 1) * unit;
            else
                x = o + (t - o + unit - 1) / unit * unit;
            seg[nseg++] = (rseg_t){tps[cur], cur, o, x};
            o = x;
            cur = pat[k];
        }
        seg[nseg++] = (rseg_t){tps[cur], cur, o, -1};
    }
    R_BPM = 120;
    R_SEED = 1;
    r_generate(seg, nseg, (int64_t)20 * 8 * QD_TPS);
    r_replay(9 * 44100);
    timeline("p24-launch", "the launch modes: A (32 steps); B at the next BAR (2 s); A at the next BEAT (3 s); B at the "
             "END of A's master length (7 s); A NOW (the next 16th after 7.6 s)");
    compare_ref("p24-launch", 1);
    printf("  %-4s %-16s %5d events, reference %d\n", fails == n0 ? "ok" : "FAIL", "p24-launch", NENG, NREF);
}

/* p25 (P1.3c): every rate, base x type: three timelines (normal, dotted, triplet), each the seven bases 1/1 .. 1/64 on
 * tracks 1..7, with micro, a ratchet x8, a strum and 1/8-step lengths at each (the step's whole-tick divisions) */
static void p_rates_type(int type, const char *name, const char *title)
{
    TP *p = tp_new(0, 132);
    TS *st;
    int t, s;
    for (t = 0; t < 7; t++) {
        const int len = t < 2 ? 4 : t < 4 ? 8 : 16;
        p->trk[t].rate = QD_RATE_CODE(t, type);
        p->trk[t].length = len;
        p->trk[t].limit = 8;
        p->trk[t].swing = t >= 4;
        for (s = 0; s < len; s += t >= 5 ? 4 : 1) {
            st = S(p, t, s, QD_TRIG_NOTE, 36 + t * 6 + s % 5, s % 3 ? 0 : 43 + t * 6, 0, 0);
            st->len = s % 4 == 1 ? 0 : QD_LEN_1;        /* (index 0: 1/8 of a step) */
            st->micro = s % 4 == 2 ? (t % 2 ? 7 : -5) : 0;
            st->strum = s % 3 ? 0 : 1;
            if (s % 8 == 3)
                st->ratch = 8;
        }
    }
    p->swing = 58;
    pattern_test(name, title, 5.0, 2500 + (uint64_t)type);
}
/* p28 (P1.3e, §8b 2.2 "six notes"): six notes a step: C13 together, Cmaj13 strummed, C11 ratcheted, a six-note
 * default (Cm13 -> Fmaj13 at 2.0 s) on tapped steps, and a TIE of six on a POLY track */
static void p_six(void)
{
    TP *p = tp_new(0, 116);
    TS *st;
    TD *d;
    int s;
    p->trk[0].limit = 14;
    p->trk[0].length = 32;
    NOTE5(NOTE5(S(p, 0, 0, QD_TRIG_NOTE, 48, 52, 55, 58), 62), 69)->len = 6;    /* C13 (C E G Bb D A) */
    st = NOTE5(NOTE5(S(p, 0, 8, QD_TRIG_NOTE, 48, 52, 55, 59), 62), 69);         /* Cmaj13, strummed */
    st->strum = 4;
    st->len = 6;
    st = NOTE5(NOTE5(S(p, 0, 16, QD_TRIG_NOTE, 48, 52, 55, 58), 62), 65);        /* C11, a ratchet x3 */
    st->ratch = 3;
    st->ramp = QD_RAMP_UP;
    st->len = 6;
    st = NOTE5(NOTE5(S(p, 0, 24, QD_TRIG_NOTE, 36, 48, 55, 60), 67), 84);        /* offsets to +48 */
    st->micro = -4;
    d = &p->trk[1].def;                                 /* track 2: a six-note default, changed at 2.0 s */
    d->nn = 6;
    d->notes[0] = 48, d->notes[1] = 51, d->notes[2] = 55, d->notes[3] = 58, d->notes[4] = 62, d->notes[5] = 69;
    d->len = 3;
    p->trk[1].def2 = *d;
    p->trk[1].def2.notes[0] = 41, p->trk[1].def2.notes[1] = 45, p->trk[1].def2.notes[2] = 48;
    p->trk[1].def2.notes[3] = 52, p->trk[1].def2.notes[4] = 55, p->trk[1].def2.notes[5] = 62;
    p->trk[1].has2 = 1;
    p->trk[1].limit = 12;
    for (s = 0; s < 16; s += 4)
        D(p, 1, s);
    p->trk[3].limit = 12;                               /* track 4: NOTE then a TIE of six on a POLY track */
    st = NOTE5(NOTE5(S(p, 3, 0, QD_TRIG_NOTE, 60, 64, 67, 71), 74), 78);
    st->len = 3;
    NOTE5(NOTE5(S(p, 3, 2, QD_TRIG_TIE, 60, 64, 67, 71), 74), 78)->len = 4;
    R_DEF_AT = 32 * 2756;                               /* 2.0 s (block-aligned) */
    pattern_test("p28-six", "six notes a step: C13 together, Cmaj13 strummed, C11 ratcheted, offsets to +48; a six-note "
                 "default (Cm13 -> Fmaj13 at 2.0 s); a TIE of six", 5.0, 2828);
    R_DEF_AT = -1;
    {   /* the six-note names */
        static const uint8_t C13[6] = {48, 52, 55, 58, 62, 69}, CM13[6] = {48, 52, 55, 59, 62, 69};
        static const uint8_t C11[6] = {48, 52, 55, 58, 62, 65}, CMIN13[6] = {48, 51, 55, 58, 62, 69};
        char rt[4], q[8], rt2[4], q2[8], rt3[4], q3[8], rt4[4], q4[8];
        const int ok = qd_chord_name(C13, 6, rt, q) && qd_chord_name(CM13, 6, rt2, q2) &&
                       qd_chord_name(C11, 6, rt3, q3) && qd_chord_name(CMIN13, 6, rt4, q4);
        checks++;
        if (!ok || strcmp(q, "13") || strcmp(q2, "maj13") || strcmp(q3, "11") || strcmp(q4, "m13")) {
            fails++;
            printf("  FAIL six-note names: %s%s %s%s %s%s %s%s\n", rt, q, rt2, q2, rt3, q3, rt4, q4);
        }
    }
}

static void p_rates(void)
{
    p_rates_type(QD_RTYPE_NORMAL, "p25-rates-normal", "the seven bases 1/1 .. 1/64, normal: micro, ratchet x8, strum, "
                 "1/8-step lengths, swing");
    p_rates_type(QD_RTYPE_DOTTED, "p26-rates-dotted", "the seven bases dotted (1/1. .. 1/64.): the same steps");
    p_rates_type(QD_RTYPE_TRIPLET, "p27-rates-triplet", "the seven bases triplet (1/1T .. 1/64T): the same steps");
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
    u_clip();
    u_scale();
    u_sounds();
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
    p_pages();
    p_loops();
    p_defaults();
    p_pool();
    p_scales();
    p_solo();
    p_five();
    p_launch();
    p_rates();
    p_six();
    printf("test_seq: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
