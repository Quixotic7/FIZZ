/* SPDX-License-Identifier: GPL-3.0-only */
/* P1.5y: names (names.h): the owner's namesizer, ported line for line where it decides (namesizer.lua: rnd,
 * phonic_nonsense, new_word), with FIZZ's limits (twelve characters, upper case). Its word lists are names_words.h.
 * phonic_nonsense is namesizer's adaptation of github.com/euangoddard/phonic-nonsense (Euan Goddard; that repository
 * states no licence: the credit is kept here). Built through sounds.c (it includes this file). */
#include <stdio.h>
#include <string.h>

#include "names.h"
#include "names_words.h"

uint32_t qd_name_next(uint32_t *seed)
{
    uint32_t x = *seed ? *seed : 1u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *seed = x;
}
static int nm_int(uint32_t *s, int lo, int hi) { return lo + (int)(qd_name_next(s) % (uint32_t)(hi - lo + 1)); }   /* math.random(lo, hi) */
static float nm_unit(uint32_t *s) { return (float)(qd_name_next(s) >> 8) / 16777216.f; }   /* math.random() */
#define NM_N(a) ((int)(sizeof(a) / sizeof((a)[0])))
#define NM_PICK(s, a) ((a)[nm_int((s), 0, NM_N(a) - 1)])

/* namesizer's tables */
static const char *const NM_CONSONANTS[] = {"b", "c", "d", "f", "g", "h", "j", "k", "l", "m", "n", "p", "r", "s", "t",
                                            "v", "w", "x", "y", "z"};
static const char *const NM_VOWELS[] = {"a", "e", "i", "o", "u"};
static const char *const NM_FUNKY[] = {"ca", "lll", "moo", "fu", "cha", "cream", "choc", "silk", "ice", "eee", "goo",
    "_a_", "two", "zzz", "boo", "cats", "boom", "ch", "ck", "digi", "ome", "sh", "ish", "ed", "milk", "cyber", "punk",
    "delta", "epsi", "beta", "fire", "uzz"};
/* START_SOUNDS: START_ONLY_SOUNDS, SIMPLE_SOUNDS, CONSONANTAL_DIGRAPHS in namesizer's order */
static const char *const NM_START[] = {"j", "qu", "", "m", "s", "d", "t", "n", "p", "g", "c", "k", "b", "f", "l", "h",
    "sh", "r", "v", "y", "w", "th", "z", "ch", "bl", "br", "cl", "cr", "dr", "fl", "fr", "gl", "gr", "pl", "pr", "sl",
    "sm", "sn", "sp", "st", "sw", "tr", "tw"};
/* MIDDLE_SOUNDS then the vowels; END_SOUNDS then SIMPLE_SOUNDS */
static const char *const NM_MIDDLE[] = {"ee", "igh", "oo", "ar", "or", "air", "ir", "ou", "oy", "ea", "oi", "aw", "ur",
    "ow", "ai", "oa", "ew", "a", "e", "i", "o", "u"};
static const char *const NM_END[] = {"ss", "ll", "ck", "ng", "nk", "x", "m", "s", "d", "t", "n", "p", "g", "c", "k",
    "b", "f", "l", "h", "sh", "r", "v", "y", "w", "th", "z", "ch"};
static const char *const NM_VCE[] = {"d", "k", "f", "l", "m", "n", "p", "t", "v"};
static const char *const NM_COMPOUND[] = {"are", "er", "ow", "ew", "ire", "ear", "ure"};

/* FIZZ's typewriter set (ctl_type.c TY_AL) */
static int nm_char_ok(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || (c && strchr("-.,'&!?#+/()*:_~ ", c));
}
int qd_name_ok(const char *s)
{
    size_t n = s ? strlen(s) : 0, i;
    if (!n || n > QD_NAME_MAX || s[0] == ' ' || s[n - 1] == ' ')
        return 0;
    for (i = 0; i < n; i++)
        if (!nm_char_ok(s[i]))
            return 0;
    return 1;
}

/* a list's word as namesizer's gsub('[%p%c%s]', '') leaves it, upper case (at most 31) */
static void nm_clean(const char *w, char *out)
{
    int n = 0;
    for (; *w && n < 31; w++) {
        const char c = *w;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
            out[n++] = (char)(c >= 'a' && c <= 'z' ? c - 32 : c);
    }
    out[n] = 0;
}
static void nm_upper(char *s)
{
    for (; *s; s++)
        if (*s >= 'a' && *s <= 'z')
            *s = (char)(*s - 32);
}
static void nm_fit(char *s)                             /* (at most twelve, no hyphen or space at the end) */
{
    size_t n = strlen(s);
    if (n > QD_NAME_MAX)
        s[n = QD_NAME_MAX] = 0;
    while (n && (s[n - 1] == '-' || s[n - 1] == ' '))
        s[--n] = 0;
}

void qd_name_rnd(uint32_t *seed, char *out)
{
    char d[32], t[32];
    int k;
    for (k = 0; k < 16; k++) {                          /* (only a pair that fits twelve) */
        nm_clean(NM_PICK(seed, NM_DESCRIPTORS), d);
        nm_clean(NM_PICK(seed, NM_THINGS), t);
        if (d[0] && t[0] && strlen(d) + 1 + strlen(t) <= QD_NAME_MAX) {
            memcpy(out, d, strlen(d));
            out[strlen(d)] = ' ';
            memcpy(out + strlen(d) + 1, t, strlen(t) + 1);
            return;
        }
    }
    /* (none fitted: one of the two alone, the one that fits; else the thing cut to twelve) */
    snprintf(out, QD_NAME_MAX + 1, "%s", strlen(d) <= QD_NAME_MAX && d[0] ? d : t);
    nm_fit(out);
}

/* namesizer's getWordTypeAtRandom and phonic_nonsense (its types 1 and 2 as namesizer has them) */
void qd_name_phonic(uint32_t *seed, char *out)
{
    const float r = nm_unit(seed);
    char w[32] = "";
    int k;
    for (k = 0; k < 8 && !w[0]; k++) {                  /* (the empty start and a short ending can give nothing) */
        if (r < 0.1f)
            snprintf(w, sizeof w, "%s%s%s", NM_PICK(seed, NM_START), NM_PICK(seed, NM_MIDDLE), NM_PICK(seed, NM_END));
        else if (r < 0.2f)
            snprintf(w, sizeof w, "%s%s%se", NM_PICK(seed, NM_START), NM_PICK(seed, NM_VOWELS), NM_PICK(seed, NM_VCE));
        else
            snprintf(w, sizeof w, "%s%s", NM_PICK(seed, NM_START), NM_PICK(seed, NM_COMPOUND));
    }
    nm_upper(w);
    nm_fit(w);
    memcpy(out, w, strlen(w) + 1);
}

/* namesizer's new_word: length 4..8, a hyphen in the middle half the time, a funky piece a fifth of the letters */
void qd_name_word(uint32_t *seed, char *out)
{
    char w[64];
    int k;
    for (k = 0; k < 8; k++) {                           /* (a word longer than twelve: another) */
        const int length = nm_int(seed, 4, 8);
        const int hyphen = nm_unit(seed) > 0.5f && length > 3, hypheni = (length + 1) / 2;   /* util.round(length / 2) */
        int i, b = 1, n = 0;
        for (i = 1; i <= length && n < 48; i++) {
            const char *piece;
            if (hyphen && i == hypheni)
                piece = "-";
            else if (nm_unit(seed) < 0.2f)
                piece = NM_PICK(seed, NM_FUNKY);
            else
                piece = b ? NM_PICK(seed, NM_CONSONANTS) : NM_PICK(seed, NM_VOWELS);
            memcpy(w + n, piece, strlen(piece));
            n += (int)strlen(piece);
            b = nm_unit(seed) > 0.5f;
        }
        w[n] = 0;
        if (n <= QD_NAME_MAX)
            break;
    }
    nm_upper(w);
    nm_fit(w);
    memcpy(out, w, strlen(w) + 1);
}

/* ---------------------------------------------------------------- the ideas */
static int nm_add(char (*out)[QD_NAME_MAX + 1], int *kind, int n, const char *nm, int k)
{
    int j;
    if (!qd_name_ok(nm))
        return n;
    for (j = 0; j < n; j++)
        if (!strcmp(out[j], nm))
            return n;
    memcpy(out[n], nm, strlen(nm) + 1);
    if (kind)
        kind[n] = k;
    return n + 1;
}

int qd_name_ideas(uint32_t seed, const int16_t *patch, char (*out)[QD_NAME_MAX + 1], int *kind, int max)
{
    /* the order: a pair (the suggestion), the sound's, then pairs between a phonic word, a cosmic word, the sound's */
    static const uint8_t PLAN[12] = {QD_NK_RND, QD_NK_SOUND, QD_NK_RND, QD_NK_PHONIC, QD_NK_RND, QD_NK_SOUND,
                                     QD_NK_RND, QD_NK_WORD, QD_NK_RND, QD_NK_PHONIC, QD_NK_SOUND, QD_NK_RND};
    char snd[8][QD_NAME_MAX + 1];
    const int nsnd = patch ? qd_sound_ideas(patch, snd, 8) : 0;
    uint32_t s = seed;
    int n = 0, i, si = 0, tries, guard = 0;
    char nm[QD_NAME_MAX + 1];
    if (max <= 0)
        return 0;
    for (i = 0; i < 12 && n < max; i++) {
        const int k = PLAN[i];
        for (tries = 0; tries < 8; tries++) {           /* (a duplicate: another of the same kind) */
            const int n0 = n;
            if (k == QD_NK_SOUND) {
                if (si >= nsnd)
                    break;
                n = nm_add(out, kind, n, snd[si++], k);
            } else {
                if (k == QD_NK_RND)
                    qd_name_rnd(&s, nm);
                else if (k == QD_NK_PHONIC)
                    qd_name_phonic(&s, nm);
                else
                    qd_name_word(&s, nm);
                n = nm_add(out, kind, n, nm, k);
            }
            if (n > n0)
                break;
        }
    }
    while (n < max && n < 12) {                         /* (fewer sound ideas: more pairs) */
        const int n0 = n;
        qd_name_rnd(&s, nm);
        n = nm_add(out, kind, n, nm, QD_NK_RND);
        if (n == n0 && ++guard > 64)
            break;
    }
    return n;
}

int qd_name_project_ideas(uint32_t seed, const char *own, char (*out)[QD_NAME_MAX + 1], int *kind, int max)
{
    static const uint8_t PLAN[11] = {QD_NK_RND, QD_NK_RND, QD_NK_PHONIC, QD_NK_RND, QD_NK_RND, QD_NK_WORD, QD_NK_RND,
                                     QD_NK_RND, QD_NK_PHONIC, QD_NK_RND, QD_NK_RND};
    uint32_t s = seed;
    int n = 0, i, guard = 0;
    char nm[QD_NAME_MAX + 1];
    if (max <= 0)
        return 0;
    if (own && own[0])
        n = nm_add(out, kind, n, own, QD_NK_OWN);
    for (i = 0; n < max && i < 11 + 64 && guard < 128; i++, guard++) {
        const int k = i < 11 ? PLAN[i] : QD_NK_RND;
        if (k == QD_NK_RND)
            qd_name_rnd(&s, nm);
        else if (k == QD_NK_PHONIC)
            qd_name_phonic(&s, nm);
        else
            qd_name_word(&s, nm);
        n = nm_add(out, kind, n, nm, k);
    }
    return n;
}

int qd_name_roll(uint32_t *seed, const int16_t *patch, const char *avoid, char *out)
{
    char snd[8][QD_NAME_MAX + 1];
    const int nsnd = patch ? qd_sound_ideas(patch, snd, 8) : 0;
    int tries, k = QD_NK_RND;
    for (tries = 0; tries < 6; tries++) {
        const int r = nm_int(seed, 0, 9);
        k = r < 6 ? QD_NK_RND : r == 6 ? QD_NK_PHONIC : r == 7 ? QD_NK_WORD : nsnd ? QD_NK_SOUND : QD_NK_RND;
        if (k == QD_NK_SOUND)
            memcpy(out, snd[nm_int(seed, 0, nsnd - 1)], QD_NAME_MAX + 1);
        else if (k == QD_NK_PHONIC)
            qd_name_phonic(seed, out);
        else if (k == QD_NK_WORD)
            qd_name_word(seed, out);
        else
            qd_name_rnd(seed, out);
        if (qd_name_ok(out) && (!avoid || strcmp(out, avoid)))
            break;
    }
    return k;
}
