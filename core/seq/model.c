/* SPDX-License-Identifier: GPL-3.0-only */
/* The model (model.h): the default tracks. */
#include <string.h>

#include "model.h"

/* the default kit: a usable band, one factory sound a track (ChoralRoot's 16, quad_voice.h's numbering), the voice
 * limits of the brief (drums 4, the bass mono, the rest 6) */
static const struct {
    const char *name;
    uint8_t preset, limit;
} QD_TRACK_DEF[QD_NTRACK] = {
    {"DRUMS", 13, 4},                   /* MARIMBA: short, struck, no sustain (low notes thump, high ones click) */
    {"BASS", 3, 1},                     /* BASS: ChoralRoot's mono bass */
    {"KEYS", 1, 6},                     /* EP */
    {"LEAD", 8, 6},                     /* SQUARE LEAD */
    {"PLUCK", 4, 6},                    /* PLUCK */
    {"PAD", 6, 6},                      /* GLASS PAD */
    {"BELL", 2, 6},                     /* BELL */
    {"NOISE", 16, 6},                   /* NOISE-ISH */
};

void qd_track_load_preset(qd_track_t *t, int k)
{
    k = k < 0 ? 0 : k >= QD_QUAD_NPRESET ? QD_QUAD_NPRESET - 1 : k;
    qd_quad_preset_patch(k, t->patch);
    t->sound = (uint8_t)(k + 1);
    t->edited = 0;
    memset(t->sound_name, 0, sizeof t->sound_name);
    strncpy(t->sound_name, qd_quad_preset_name(k), sizeof t->sound_name - 1);
}

void qd_track_load_sound(qd_track_t *t, int k, const char *name, const int16_t *patch)
{
    memcpy(t->patch, patch, sizeof t->patch);
    t->sound = (uint8_t)k;
    t->edited = 0;
    memset(t->sound_name, 0, sizeof t->sound_name);
    strncpy(t->sound_name, name ? name : "", sizeof t->sound_name - 1);
}

const char *qd_track_sound_name(const qd_track_t *t) { return t->sound_name; }

void qd_model_bind(qd_model_t *m)
{
    int i;
    for (i = 0; i < QD_NTRACK; i++) {
        m->t[i].owner.quad_patch = m->t[i].patch;   /* (the FREE LFOs run at the track's sound's rates) */
        m->t[i].owner.m_v = 0;
    }
}

void qd_model_tempo(qd_model_t *m, int bpm)
{
    int i;
    for (i = 0; i < QD_NTRACK; i++)
        m->t[i].owner.bpm = (int16_t)bpm;
}

void qd_model_init(qd_model_t *m)
{
    int i;
    memset(m, 0, sizeof *m);
    for (i = 0; i < QD_NTRACK; i++) {
        qd_track_t *t = &m->t[i];
        strncpy(t->name, QD_TRACK_DEF[i].name, QD_TRACK_NAME - 1);
        qd_track_load_preset(t, QD_TRACK_DEF[i].preset);
        t->mix.level = QD_MIX_LEVEL_DEFAULT;
        t->mix.pan = 0;
        t->mix.reverb = QD_MIX_REVERB_DEFAULT;
        t->mix.delay = t->mix.chorus = 0;
        t->limit = QD_TRACK_DEF[i].limit;
        t->reserve = 0;
        qd_quad_owner_init(&t->owner);
    }
    qd_model_bind(m);
}
