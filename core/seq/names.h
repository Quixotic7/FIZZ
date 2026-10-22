/* SPDX-License-Identifier: GPL-3.0-only */
/* P1.5y: names for sounds and projects, a port of the owner's namesizer (namesizer.lua, written for elasticat), its
 * three generators deterministic from a seed (an xorshift32: the same seed, the same names, on every build):
 *
 *   qd_name_rnd      "DESCRIPTOR THING" from namesizer's two word lists (names_words.h), only pairs that fit twelve
 *                    characters; else the descriptor or the thing alone ("SPICY WALRUS", "COSMIC")
 *   qd_name_phonic   a word that could be real, from phonics' sounds (namesizer's phonic_nonsense, adapted from
 *                    github.com/euangoddard/phonic-nonsense) ("SHOOCK", "TRIRE")
 *   qd_name_word     a word from the cosmos: consonants and vowels, now and then a funky piece, sometimes a hyphen
 *                    (namesizer's new_word) ("ZU-CREAMB")
 *
 * Every name is upper case, at most twelve characters, in FIZZ's typewriter set (A..Z 0..9 - . , ' & ! ? # + / ( ) * :
 * _ ~ SPACE), no space at either end. Built through sounds.c (it includes names.c: the core's source lists stay). */
#ifndef QD_NAMES_H
#define QD_NAMES_H
#include <stdint.h>

#define QD_NAME_MAX 12                 /* (QD_SOUND_NAME - 1) */
uint32_t qd_name_next(uint32_t *seed);   /* the generators' xorshift32 (a seed of 0 is taken as 1) */
void qd_name_rnd(uint32_t *seed, char *out);      /* out: QD_NAME_MAX + 1 bytes; each advances *seed */
void qd_name_phonic(uint32_t *seed, char *out);
void qd_name_word(uint32_t *seed, char *out);
int qd_name_ok(const char *s);          /* fits twelve characters and FIZZ's set (no space at either end) */

/* PRESETS' name ideas for SAVE PRESET: max of them (12 on the panel): half namesizer pairs, two or three phonic
 * nonsense / cosmic words, and the sound's own ideas (qd_sound_ideas), one of them second; the first is the pair
 * qd_name_rnd gives for seed (SAVE PRESET's suggestion). kind (optional): each one's QD_NK_* */
enum { QD_NK_RND, QD_NK_PHONIC, QD_NK_WORD, QD_NK_SOUND, QD_NK_OWN };
int qd_name_ideas(uint32_t seed, const int16_t *patch, char (*out)[QD_NAME_MAX + 1], int *kind, int max);
/* P1.5z: one rolled name, endless (SAVE PRESET's PRESETS / KNOB 4 right): a namesizer pair six times in ten, a phonic
 * or cosmic word two (one each), the sound's own idea two (qd_sound_ideas; no patch: a pair); not avoid (the name just
 * shown) if it can help it. Its kind (QD_NK_*) */
int qd_name_roll(uint32_t *seed, const int16_t *patch, const char *avoid, char *out);
/* a project's ideas: its own name first (own, if any), then namesizer's (pairs, a phonic word, a cosmic word) */
int qd_name_project_ideas(uint32_t seed, const char *own, char (*out)[QD_NAME_MAX + 1], int *kind, int max);
#endif
