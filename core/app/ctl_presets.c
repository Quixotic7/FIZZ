/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* P1.5t: the library browser and SAVE PRESET (PLAN.md "The sound system, redesigned (approved: P1.5t)";
 * mockups/fizz-presets.html). Built through ctl_latch.c (it includes this file before ctl_lib.c, which routes the
 * keys, knobs and OCT keys of its pages here).
 *
 * THE BROWSER (QD_LATCH_BROWSE): PRESETS turned with nothing held. It opens on the track's current preset (the track
 * keeps its own sound until the first detent); every detent moves one entry and the track plays it at once: the
 * preview is the model's track sound and the sequencer's preview pointer (qd_seq_preview), never the pattern's copy,
 * so neither undo nor saving sees it; stopped, a short note each turn. SELECT jumps ten; the white keys play it in
 * the track's scale. F2 LOADS (the pattern's track sound takes it: one undo level); F1 or HOME CANCELS (the own sound
 * back, edits and all); any other way out is a cancel. B1..B8 the kinds (ALL, BASS .. FX: P1.5v: every preset with
 * that tag), B9 the source (ALL -> MINE -> POOL), B10 the pool, from any source (P1.5v): + POOL (a library entry
 * copied into the pool, not loaded; full: the tray flashes orange) when it is not there, - POOL when it is (a sound
 * steps use is armed first: a red X, OUT?, its users going to TRACK; B10 again takes it out), B11 deletes one of MY
 * presets (armed first: a red X, DELETE?, where it is used: the projects whose pools reference it, the steps locking
 * it, going to TRACK; B11 again: it breaks up and fizzes away, its references gone from the pools (P1.5w: the pool
 * holds references; this project's now, the others' as they load), their steps back to their track's sound; one
 * undo level). Any other key disarms. SHIFT + B2..B8 toggles that tag on the chosen (P1.5v: a factory
 * preset's tags are the owner's too, its sound read only).
 * SAVE PRESET (QD_LATCH_SAVE): SAVE. A name from the sound (the sparkle), selected, typed on the keybed (P1.5u:
 * ctl_type.c's typewriter: the keys type, PRESETS name ideas, ALGORITHM the cursor, SELECT the character, F1
 * deletes); SAVE again does the choice: NEW (a new preset in MY presets) or, when the sound came from one of MY
 * presets, REPLACE (that one): P1.5v: F2 switches the choice, both shown; HOME cancels. Saved: the track plays that preset (not edited). */

/* ---------------------------------------------------------------- the list */
int qd_br_buf[QD_BR_LIST];                              /* (P2.1b: one list for all: each is used and done before the
                                                         * next, none kept across a call that lists) */
int qd_br_list(const qd_app_t *a, int src, int kind, int *out)
{
    int n = 0, k;
    const unsigned bit = kind ? QD_PKB(kind - 1) : QD_PK_MASK;   /* (P1.5v: every one with the tag) */
    if (src == QD_BR_POOL) {
        for (k = 1; k <= QD_NSOUND; k++)
            if (qd_sound_used(&a->proj, k) && (!kind || (qd_sound_tags(&a->proj, k) & bit)))
                out[n++] = QD_BR_POOL_CODE + k;
        return n;
    }
    if (src == QD_BR_ALL)
        for (k = 0; k < QD_NFACTORY; k++)
            if (!kind || ((unsigned)qd_factory_tags(a, k) & bit))   /* (P2.1b: the tags alone) */
                out[n++] = k;
    for (k = 0; k < QD_NLIB; k++)
        if (qd_lib_used(a, k) && (!kind || (qd_lib_tags(a, k) & bit)))
            out[n++] = QD_BR_MINE_CODE + k;
    return n;
}

int qd_br_entry(const qd_app_t *a, int code, qd_sound_t *s)
{
    if (code >= QD_BR_POOL_CODE) {
        if (!qd_sound_used(&a->proj, code - QD_BR_POOL_CODE))
            return 0;
        qd_sound_read(&a->proj, code - QD_BR_POOL_CODE, s);
        return 1;
    }
    if (code >= QD_BR_MINE_CODE) {
        if (code - QD_BR_MINE_CODE >= QD_NLIB || !qd_lib_used(a, code - QD_BR_MINE_CODE))
            return 0;
        qd_lib_read(a, code - QD_BR_MINE_CODE, s);
        return 1;
    }
    if (code < 0 || code >= QD_NFACTORY)
        return 0;
    qd_factory_get(a, code, s);
    return 1;
}

int qd_br_chosen(const qd_app_t *a, qd_sound_t *s, int *code, int *n)
{
    int *const list = qd_br_buf;
    const qd_brui_t *b = &a->ui.br;
    const int m = qd_br_list(a, b->src, b->kind, list);
    if (n)
        *n = m;
    if (!m)
        return 0;
    if (code)
        *code = list[b->pos < 0 ? 0 : b->pos >= m ? m - 1 : b->pos];
    return qd_br_entry(a, list[b->pos < 0 ? 0 : b->pos >= m ? m - 1 : b->pos], s);
}

/* where the entry for (preset id, pool number) is in the list (-1 none) */
static int br_find(const qd_app_t *a, const int *list, int n, int preset, int pool)
{
    int i;
    for (i = 0; i < n; i++) {
        const int c = list[i];
        if (c >= QD_BR_POOL_CODE ? (pool && c - QD_BR_POOL_CODE == pool)
                                 : c >= QD_BR_MINE_CODE ? preset && qd_lib_preset(a, c - QD_BR_MINE_CODE) == preset
                                                        : preset == c + 1)
            return i;
    }
    if (preset)                                         /* (in the POOL: the pool sound of that preset) */
        for (i = 0; i < n; i++)
            if (list[i] >= QD_BR_POOL_CODE && qd_sound_preset(&a->proj, list[i] - QD_BR_POOL_CODE) == preset)
                return i;
    return -1;
}

/* ---------------------------------------------------------------- the preview */
static int br_note(qd_app_t *a) { return qd_snd_scale_note(a, 0); }

/* the track plays the chosen (moved), else its own sound */
/* P1.5x: a preset's mix onto the track (its pan and sends; its level stays) */
static void br_mix(qd_track_t *t, const qd_sound_t *s)
{
    t->mix.pan = s->pan;
    t->mix.chorus = s->chorus, t->mix.delay = s->delay, t->mix.reverb = s->reverb;
}

static void br_apply(qd_app_t *a)
{
    qd_brui_t *b = &a->ui.br;
    qd_track_t *t = &a->model.t[b->t];
    qd_sound_t s;
    int code = 0;
    if (b->moved && qd_br_chosen(a, &s, &code, 0)) {
        qd_track_load_sound(t, code >= QD_BR_POOL_CODE ? code - QD_BR_POOL_CODE : 0, s.name, s.patch);
        t->preset = s.preset;
        br_mix(t, &s);                                  /* (P1.5x: its mix heard too) */
        qd_seq_preview(&a->seq, b->t, t->patch);
    } else {
        qd_track_from_tsound(t, &b->own);
        t->mix = b->own_mix;
        qd_seq_preview(&a->seq, b->t, 0);
    }
}

static void br_sound(qd_app_t *a)                      /* stopped: a short note (playing: heard in the pattern) */
{
    if (!a->seq.playing && a->ui.br.moved)
        qd_ctl_preview(a, br_note(a), 100);
}

/* the list changed (a filter, the source, an entry out): the same entry if it is there, else the place clamped */
static void br_relist(qd_app_t *a, int keep_code)
{
    int *const list = qd_br_buf;
    qd_brui_t *b = &a->ui.br;
    const int n = qd_br_list(a, b->src, b->kind, list);
    int i;
    for (i = 0; i < n && list[i] != keep_code; i++)
        ;
    if (i < n)
        b->pos = i;
    else
        b->pos = b->pos >= n ? n - 1 : b->pos < 0 ? 0 : b->pos;
    if (b->pos < 0)
        b->pos = 0;
    b->del_armed = b->out_armed = 0;
}

/* ---------------------------------------------------------------- open, move, leave */
static void br_open(qd_app_t *a)
{
    int *const list = qd_br_buf;
    qd_ui_t *u = &a->ui;
    qd_brui_t *b = &u->br;
    const qd_track_t *t = &a->model.t[a->sel];
    int n, i;
    b->ret_mode = u->mode;
    b->ret_latch = u->latch == QD_LATCH_SND ? QD_LATCH_SND : QD_LATCH_NONE;
    if (u->mode != QD_MODE_STEP)
        qd_ctl_set_mode(a, QD_MODE_STEP);
    if (!u->slatch)
        qd_step_release_all(a);
    u->tk = QD_TK_NONE;
    qd_latch_open(a, QD_LATCH_BROWSE);
    b->t = a->sel;
    qd_track_to_tsound(t, &b->own);
    b->own_mix = t->mix;                                /* (P1.5x: the track's own mix, back on a cancel) */
    b->src = QD_BR_ALL;
    b->kind = 0;
    b->moved = 0;
    b->del_armed = b->out_armed = 0;
    b->fx = QD_BRFX_NONE;
    n = qd_br_list(a, b->src, b->kind, list);           /* (on the track's current preset) */
    i = br_find(a, list, n, t->preset, 0);
    b->pos = i < 0 ? 0 : i;
}

void qd_br_turn(qd_app_t *a, int d)
{
    qd_brui_t *b = &a->ui.br;
    int n;
    int *const list = qd_br_buf;
    if (a->ui.latch != QD_LATCH_BROWSE) {
        br_open(a);
        return;
    }
    n = qd_br_list(a, b->src, b->kind, list);
    if (!n || !d)
        return;
    b->pos = b->pos + d < 0 ? 0 : b->pos + d >= n ? n - 1 : b->pos + d;
    b->moved = 1;
    b->move_ms = a->ms ? a->ms : 1u;
    b->move_dir = d > 0 ? 1 : -1;
    b->del_armed = b->out_armed = 0;
    br_apply(a);
    br_sound(a);
}

/* (the latch closed some other way: EDIT, a mode change, HOME) */
static void br_closed(qd_app_t *a)
{
    qd_brui_t *b = &a->ui.br;
    qd_track_from_tsound(&a->model.t[b->t], &b->own);
    a->model.t[b->t].mix = b->own_mix;
    qd_seq_preview(&a->seq, b->t, 0);
    if (b->ret_mode != QD_MODE_STEP && b->ret_mode != QD_MODE_IDLE && a->ui.mode == QD_MODE_STEP)
        a->ui.mode = b->ret_mode;
}

static void br_back_to(qd_app_t *a, int reopen)        /* F1 / F2: back where it came from (HOME: to the view) */
{
    const int ret = a->ui.br.ret_latch;
    a->ui.oct_eaten |= 3u;
    qd_latch_close(a);
    if (reopen && ret == QD_LATCH_SND)
        qd_latch_open(a, QD_LATCH_SND);
}

static void br_cancel(qd_app_t *a, int reopen)
{
    qd_ui_t *u = &a->ui;
    qd_sound_t s;
    const int had = u->br.moved && qd_br_chosen(a, &s, 0, 0);
    memset(&u->tk_snd, 0, sizeof u->tk_snd);
    memcpy(u->tk_snd.name, u->br.own.name, QD_SOUND_NAME);
    memcpy(u->tk_snd.patch, u->br.own.patch, sizeof u->tk_snd.patch);
    u->tk_snd.tags = (uint8_t)qd_preset_tags(u->br.own.patch);
    u->tk_snd.flags = u->br.own.edited;                 /* (the takeover: its name with a *) */
    if (had)
        u->tk_snd2 = s;
    else
        memset(&u->tk_snd2, 0, sizeof u->tk_snd2);
    br_back_to(a, reopen);                              /* (br_closed: the own sound back) */
    qd_ctl_takeover(a, QD_TK_BACK, 0, 0, 0, -1);
}
void qd_br_cancel(qd_app_t *a) { br_cancel(a, 1); }

/* P1.5z: SHIFT + SELECT in the browser: the track browsed put back exactly (its own sound and mix, as a cancel), then
 * track t selected and browsed from its own sound, the list where it was */
void qd_br_track(qd_app_t *a, int t)
{
    qd_brui_t *b = &a->ui.br;
    qd_track_from_tsound(&a->model.t[b->t], &b->own);
    a->model.t[b->t].mix = b->own_mix;
    qd_seq_preview(&a->seq, b->t, 0);
    a->ui.latch = QD_LATCH_NONE;                        /* (the selection would close the browser: it stays) */
    qd_ctl_select_track(a, t);
    a->ui.latch = QD_LATCH_BROWSE;
    b->t = a->sel;
    qd_track_to_tsound(&a->model.t[b->t], &b->own);
    b->own_mix = a->model.t[b->t].mix;
    b->moved = 0;
    b->del_armed = b->out_armed = 0;
}
void qd_br_home(qd_app_t *a) { br_cancel(a, 0); }

/* Manager (docs/SYSEX.md BROWSE): the browser opened on track t (selected first) at the preset with id `preset`, as
 * if PRESETS had turned to it: the track previews it, OCT- cancels, OCT+ loads. Open already (another track: that
 * one's preview put back first, as SHIFT + SELECT does): moved there; the list ALL, no kind. 0: not in the list */
int qd_br_goto(qd_app_t *a, int t, int preset)
{
    int *const list = qd_br_buf;
    qd_brui_t *b = &a->ui.br;
    int n = qd_br_list(a, QD_BR_ALL, 0, list), i;
    if (!preset || br_find(a, list, n, preset, 0) < 0)
        return 0;
    if (a->ui.latch == QD_LATCH_BROWSE) {
        if (b->t != t)
            qd_br_track(a, t);
    } else {
        qd_ctl_select_track(a, t);
        br_open(a);
    }
    b->src = QD_BR_ALL, b->kind = 0;
    n = qd_br_list(a, b->src, b->kind, list);
    i = br_find(a, list, n, preset, 0);
    b->move_dir = i < b->pos ? -1 : 1;
    b->pos = i;
    b->moved = 1;
    b->move_ms = a->ms ? a->ms : 1u;
    b->del_armed = b->out_armed = 0;
    br_apply(a);
    br_sound(a);
    return 1;
}

void qd_br_load(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    qd_brui_t *b = &u->br;
    qd_sound_t s;
    int code = 0;
    qd_tsound_t now;
    if (!qd_br_chosen(a, &s, &code, 0)) {
        br_cancel(a, 1);
        return;
    }
    qd_undo_hint(a, QD_UK_SOUNDS);                      /* (one undo level: the pattern's track sound) */
    memset(&now, 0, sizeof now);
    memcpy(now.patch, s.patch, sizeof now.patch);
    now.sound = (uint8_t)(code >= QD_BR_POOL_CODE ? code - QD_BR_POOL_CODE : 0);
    memcpy(now.name, s.name, QD_SOUND_NAME);
    now.preset = s.preset;
    b->own = now;                                       /* (the latch closes on it: br_closed keeps it) */
    br_mix(&a->model.t[b->t], &s);                      /* (P1.5x: and its mix, the preset's: kept) */
    b->own_mix = a->model.t[b->t].mix;
    qd_ctl_pat(a)->snd[b->t] = now;
    u->tk_snd = s;
    br_back_to(a, 1);
    qd_ctl_takeover(a, QD_TK_LOADED, 0, 0, 0, -1);
}

void qd_br_sync(qd_app_t *a)
{
    qd_brui_t *b = &a->ui.br;
    if (a->ui.latch != QD_LATCH_BROWSE)
        return;
    qd_track_to_tsound(&a->model.t[b->t], &b->own);     /* (another pattern's track sound: the own one now) */
    br_apply(a);
}

/* ---------------------------------------------------------------- its keys and knobs */
/* the pool number the chosen is (its own in the POOL; else the pool's copy of it), 0 none */
static int br_pool_k(const qd_app_t *a, const qd_sound_t *s, int code)
{
    return code >= QD_BR_POOL_CODE ? code - QD_BR_POOL_CODE : qd_pool_find(a, s);
}

int qd_br_pool_k(const qd_app_t *a)
{
    qd_sound_t s;
    int code = 0;
    return qd_br_chosen(a, &s, &code, 0) ? br_pool_k(a, &s, code) : 0;
}

static void br_pool(qd_app_t *a)                       /* B10: + POOL, or - POOL when it is there (P1.5v: any source) */
{
    qd_brui_t *b = &a->ui.br;
    qd_sound_t s;
    int code = 0, k;
    if (!qd_br_chosen(a, &s, &code, 0))
        return;
    b->fx_ms = a->ms ? a->ms : 1u;
    k = br_pool_k(a, &s, code);
    if (!k) {                                           /* + POOL */
        k = qd_pool_add_sound(a, &s);
        b->fx = k ? QD_BRFX_IN : QD_BRFX_FULL;
        b->fx_k = k;
        b->out_armed = 0;
        return;
    }
    if (qd_pool_users_all(a, k, 0) && !b->out_armed) { /* - POOL: steps use it: OUT? first, B10 again */
        b->out_armed = 1;
        b->fx = QD_BRFX_NONE;
        return;
    }
    b->out_armed = 0;
    if (b->own.sound == k)
        b->own.sound = 0;
    qd_pool_delete(a, k);                               /* (its steps back to their track's sound) */
    b->fx = QD_BRFX_OUT;
    if (b->src == QD_BR_POOL)
        br_relist(a, -1);
    if (b->moved)
        br_apply(a);
}

static void br_delete(qd_app_t *a)                     /* B11: one of MY presets, twice (P1.5v: it breaks up) */
{
    qd_brui_t *b = &a->ui.br;
    qd_sound_t s;
    int code = 0;
    if (!qd_br_chosen(a, &s, &code, 0) || code < QD_BR_MINE_CODE || code >= QD_BR_POOL_CODE)
        return;
    if (!b->del_armed) {
        b->del_armed = 1;
        b->arm_ms = a->ms ? a->ms : 1u;
        qd_preset_usage(a, s.preset, &b->use_proj, &b->use_steps);   /* (P1.5w: where it is used) */
        return;
    }
    b->del_armed = 0;
    b->fx_snd = s;                                      /* (the one going: its pieces) */
    if (b->own.sound && b->own.sound <= QD_NSOUND && qd_sound_preset(&a->proj, b->own.sound) == s.preset)
        b->own.sound = 0;
    qd_preset_delete(a, code - QD_BR_MINE_CODE);        /* (P1.5w: its references with it, their locks to TRACK) */
    b->fx = QD_BRFX_DEL;
    b->fx_ms = a->ms ? a->ms : 1u;
    br_relist(a, -1);
    if (b->moved)
        br_apply(a);
}

/* SHIFT + B2..B8: that tag on the chosen, on or off */
static void br_tag(qd_app_t *a, int tag)
{
    qd_brui_t *b = &a->ui.br;
    qd_sound_t s;
    int code = 0;
    if (!qd_br_chosen(a, &s, &code, 0))
        return;
    a->ui.shift_used = 1;
    if (!qd_tags_set(a, code, s.tags ^ QD_PKB(tag)))
        return;
    b->tag_ms = a->ms ? a->ms : 1u;
    b->tag_k = tag;
    b->tag_on = !(s.tags & QD_PKB(tag));
    br_relist(a, code);                                 /* (the same one, if its filter still lists it) */
    if (b->moved)
        br_apply(a);
}

void qd_br_key(qd_app_t *a, int key, int w, int b, int down, int vel)
{
    qd_brui_t *br = &a->ui.br;
    if (down && b != 9)
        br->out_armed = 0;                              /* (any other key disarms) */
    if (down && b != 10)
        br->del_armed = 0;
    if (w >= 0) {                                       /* the W keys play it, in the track's scale */
        if (down)
            qd_app_note_on(a, key, qd_snd_scale_note(a, w), vel);
        return;
    }
    if (!down)
        return;
    if (b != 9 && br->fx != QD_BRFX_DEL)
        br->fx = QD_BRFX_NONE;                          /* (another list: + POOL's animation over) */
    if (qd_ctl_shift(a)) {                              /* P1.5v: SHIFT: the tags (no SHIFT shortcuts here) */
        a->ui.shift_used = 1;
        if (b >= 1 && b <= 7)
            br_tag(a, b - 1);
        return;
    }
    if (b <= 7) {                                       /* the kinds */
        qd_sound_t s;
        int code = -1;
        if (!qd_br_chosen(a, &s, &code, 0))
            code = -1;
        br->kind = b;
        br_relist(a, code);
        if (br->moved)
            br_apply(a);
        return;
    }
    if (b == 8) {                                       /* the source */
        qd_sound_t s;
        int code = -1;
        if (!qd_br_chosen(a, &s, &code, 0))
            code = -1;
        br->src = (br->src + 1) % QD_NBRSRC;
        {
            int *const list = qd_br_buf;
            const int n = qd_br_list(a, br->src, br->kind, list);
            const int i = code >= 0 ? br_find(a, list, n, s.preset, code >= QD_BR_POOL_CODE ? code - QD_BR_POOL_CODE : 0)
                                    : -1;
            br_relist(a, i >= 0 ? list[i] : -1);
        }
        if (br->moved)
            br_apply(a);
        return;
    }
    if (b == 9) {
        if (!br->out_armed)
            br->arm_ms = a->ms ? a->ms : 1u;
        br_pool(a);
        return;
    }
    br_delete(a);
}

int qd_br_knob(qd_app_t *a, int knob, int d)
{
    if (knob == QD_K_PRESETS) {
        qd_br_turn(a, qd_ctl_acc(a, d, QD_BR_LIST));
        return 1;
    }
    if (knob == QD_K_SELECT) {                          /* ten at a time */
        qd_br_turn(a, d * 10);
        return 1;
    }
    return 1;                                           /* (the others: nothing while browsing) */
}

void qd_br_leds(const qd_app_t *a, uint8_t *key)
{
    const qd_brui_t *b = &a->ui.br;
    const int fast = (a->ms / QD_FAST_MS) & 1u;
    qd_sound_t s;
    int code = -1, i;
    if (!qd_br_chosen(a, &s, &code, 0))
        code = -1;
    for (i = 0; i < 16; i++)
        key[QD_WHITE_KEY[i]] = QD_LED_DIM;              /* (they play it) */
    for (i = 0; i < 8; i++)                             /* P1.5x (the owner): the filter solid; B2..B8 the chosen's
                                                         * tags breathing (SHIFT too: a toggle shows at once); the
                                                         * rest off */
        key[QD_BLACK_KEY[i]] = i == b->kind ? QD_LED_LIT
                               : i >= 1 && code >= 0 && ((s.tags >> (i - 1)) & 1u) ? qd_led_breath_lo(a->ms) : QD_LED_OFF;
    key[QD_BLACK_KEY[8]] = QD_LED_LIT;
    key[QD_BLACK_KEY[9]] = b->out_armed ? (fast ? QD_LED_LIT : QD_LED_OFF)   /* (P1.5v: add lit, remove breathing) */
                           : code < 0 ? QD_LED_DIM : br_pool_k(a, &s, code) ? qd_led_breath(a->ms) : QD_LED_LIT;
    key[QD_BLACK_KEY[10]] = code >= QD_BR_MINE_CODE && code < QD_BR_POOL_CODE
                                ? (b->del_armed ? (fast ? QD_LED_LIT : QD_LED_OFF) : QD_LED_LIT) : QD_LED_OFF;
}

/* ---------------------------------------------------------------- SAVE PRESET */
/* a name no MY preset has: "DARK BASS" -> "DARK BASS 2" .. (P1.5w: persist.c's, migration's too) */
static void sv_unique(const qd_app_t *a, const char *nm, char *out) { qd_lib_unique_name(a, nm, out); }
static void sv_update_new(qd_app_t *a)
{
    qd_saveui_t *v = &a->ui.sv;
    char nm[QD_SOUND_NAME];
    qd_type_trimmed(a, nm);
    sv_unique(a, nm[0] ? nm : "SOUND", v->newname);
}

void qd_sv_open(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    qd_saveui_t *v = &u->sv;
    const qd_track_t *t = &a->model.t[a->sel];
    char nm[QD_SOUND_NAME];
    if (u->latch == QD_LATCH_SAVE) {                    /* SAVE again: NEW */
        qd_sv_new(a);
        return;
    }
    v->ret_mode = u->mode;
    if (u->mode != QD_MODE_STEP)
        qd_ctl_set_mode(a, QD_MODE_STEP);
    if (!u->slatch)
        qd_step_release_all(a);
    u->tk = QD_TK_NONE;
    qd_latch_open(a, QD_LATCH_SAVE);
    v->replace = qd_lib_find(a, t->preset);             /* (from one of MY presets: REPLACE offered) */
    v->choice = 0;                                      /* (P1.5v: NEW first; F2 switches) */
    if (v->replace >= 0) {
        qd_type_start(a, qd_lib_name(a, v->replace), 0);
    } else {                                            /* P1.5y: a namesizer pair, its ideas' first */
        const uint32_t seed = qd_type_seed(a);
        uint32_t s = seed;
        qd_name_rnd(&s, nm);
        qd_type_start(a, nm, 1);                        /* (P1.5u: selected: the first key typed replaces it) */
        a->ui.ty.seed = seed;
        a->ui.ty.rseed = s;                             /* (P1.5z: the rolls go on from there) */
    }
    sv_update_new(a);
}

static void sv_done(qd_app_t *a, int slot)
{
    qd_ui_t *u = &a->ui;
    qd_track_t *t = &a->model.t[a->sel];
    qd_sound_t s;
    qd_lib_read(a, slot, &s);
    memcpy(t->sound_name, s.name, QD_SOUND_NAME);
    t->preset = s.preset;
    t->sound = 0;
    t->edited = 0;
    u->tk_snd = s;
    if (u->sv.ret_mode != QD_MODE_STEP && u->sv.ret_mode != QD_MODE_IDLE)
        u->mode = u->sv.ret_mode;
    qd_latch_close(a);
    u->oct_eaten |= 3u;
    qd_ctl_takeover(a, QD_TK_SAVED, 0, 0, 0, -1);
}

/* P1.5x: the track's pan and sends saved with the sound */
static void sv_mix(qd_app_t *a, int slot)
{
    const qd_mix_track_t *m = &a->model.t[a->sel].mix;
    qd_sound_t s;
    qd_lib_read(a, slot, &s);
    s.pan = m->pan;
    s.chorus = m->chorus, s.delay = m->delay, s.reverb = m->reverb;
    qd_lib_write(a, slot, &s);
    qd_pool_resolve(a);                                 /* (the pool's references: its mix too) */
}

void qd_sv_new(qd_app_t *a)
{
    const int slot = qd_lib_free_slot(a);
    if (slot < 0) {
        qd_ctl_confirm(a, "FULL", 0);
        return;
    }
    sv_update_new(a);
    qd_lib_store(a, slot, a->ui.sv.newname, a->model.t[a->sel].patch);
    sv_mix(a, slot);
    sv_done(a, slot);
}

void qd_sv_replace(qd_app_t *a)
{
    qd_saveui_t *v = &a->ui.sv;
    char nm[QD_SOUND_NAME];
    if (v->replace < 0 || !qd_lib_used(a, v->replace))
        return;                                         /* (factory presets are never replaced) */
    qd_type_trimmed(a, nm);
    if (!nm[0])
        snprintf(nm, sizeof nm, "%s", qd_lib_name(a, v->replace));
    qd_lib_replace(a, v->replace, nm, a->model.t[a->sel].patch);
    sv_mix(a, v->replace);
    sv_done(a, v->replace);
}

void qd_sv_leds(const qd_app_t *a, uint8_t *key) { qd_type_leds(a, key); }   /* (P1.5u: the typewriter's) */
