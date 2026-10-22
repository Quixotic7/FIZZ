/* SPDX-License-Identifier: GPL-3.0-only */
/* P1.5t: the library browser and SAVE PRESET (PLAN.md "The sound system, redesigned (approved: P1.5t)";
 * mockups/fizz-presets.html). Built through ctl_latch.c (it includes this file before ctl_lib.c, which routes the
 * keys, knobs and OCT keys of its pages here).
 *
 * THE BROWSER (QD_LATCH_BROWSE): PRESETS turned with nothing held. It opens on the track's current preset (the track
 * keeps its own sound until the first detent); every detent moves one entry and the track plays it at once: the
 * preview is the model's track sound and the sequencer's preview pointer (qd_seq_preview), never the pattern's copy,
 * so neither undo nor saving sees it; stopped, a short note each turn. SELECT jumps ten; the white keys play it in
 * the track's scale. F2 LOADS (the pattern's track sound takes it: one undo level); F1 or HOME CANCELS (the own sound
 * back, edits and all); any other way out is a cancel. B1..B8 the kinds (ALL, BASS .. FX), B9 the source (ALL ->
 * MINE -> POOL), B10 + POOL (a library entry copied into the pool, not loaded; there already: the tray pulses; full:
 * it flashes orange) / - POOL (in the POOL source: out; a sound steps use is armed first, B10 again takes it out),
 * B11 deletes one of MY presets (twice).
 * SAVE PRESET (QD_LATCH_SAVE): SAVE. A name from the sound (the sparkle); B10 another (the dice); SELECT the letter
 * under the cursor, ALGORITHM the cursor, B11 deletes a letter (the twelve cells shown); SAVE again a NEW preset in
 * MY presets; F2 REPLACES the MY preset the sound came from (when it did); F1 cancels. Saved: the track plays that
 * preset (not edited). */

static const char SV_AL[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -";

/* ---------------------------------------------------------------- the list */
int qd_br_list(const qd_app_t *a, int src, int kind, int *out)
{
    int n = 0, k;
    if (src == QD_BR_POOL) {
        for (k = 1; k <= QD_NSOUND; k++)
            if (qd_sound_used(&a->proj, k) && (!kind || a->proj.sound[k - 1].kind == kind - 1))
                out[n++] = QD_BR_POOL_CODE + k;
        return n;
    }
    if (src == QD_BR_ALL)
        for (k = 0; k < QD_NFACTORY; k++) {
            qd_sound_t f;
            qd_factory_preset(k, &f);
            if (!kind || f.kind == kind - 1)
                out[n++] = k;
        }
    for (k = 0; k < QD_NLIB; k++)
        if (a->per.lib[k].name[0] && (!kind || a->per.lib[k].kind == kind - 1))
            out[n++] = QD_BR_MINE_CODE + k;
    return n;
}

int qd_br_entry(const qd_app_t *a, int code, qd_sound_t *s)
{
    if (code >= QD_BR_POOL_CODE) {
        if (!qd_sound_used(&a->proj, code - QD_BR_POOL_CODE))
            return 0;
        *s = a->proj.sound[code - QD_BR_POOL_CODE - 1];
        return 1;
    }
    if (code >= QD_BR_MINE_CODE) {
        if (code - QD_BR_MINE_CODE >= QD_NLIB || !a->per.lib[code - QD_BR_MINE_CODE].name[0])
            return 0;
        *s = a->per.lib[code - QD_BR_MINE_CODE];
        return 1;
    }
    if (code < 0 || code >= QD_NFACTORY)
        return 0;
    qd_factory_preset(code, s);
    return 1;
}

int qd_br_chosen(const qd_app_t *a, qd_sound_t *s, int *code, int *n)
{
    static int list[QD_BR_LIST];
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
                                 : c >= QD_BR_MINE_CODE ? preset && a->per.lib[c - QD_BR_MINE_CODE].preset == preset
                                                        : preset == c + 1)
            return i;
    }
    if (preset)                                         /* (in the POOL: the pool sound of that preset) */
        for (i = 0; i < n; i++)
            if (list[i] >= QD_BR_POOL_CODE && a->proj.sound[list[i] - QD_BR_POOL_CODE - 1].preset == preset)
                return i;
    return -1;
}

/* ---------------------------------------------------------------- the preview */
static int br_note(qd_app_t *a) { return qd_snd_scale_note(a, 0); }

/* the track plays the chosen (moved), else its own sound */
static void br_apply(qd_app_t *a)
{
    qd_brui_t *b = &a->ui.br;
    qd_track_t *t = &a->model.t[b->t];
    qd_sound_t s;
    int code = 0;
    if (b->moved && qd_br_chosen(a, &s, &code, 0)) {
        qd_track_load_sound(t, code >= QD_BR_POOL_CODE ? code - QD_BR_POOL_CODE : 0, s.name, s.patch);
        t->preset = s.preset;
        qd_seq_preview(&a->seq, b->t, t->patch);
    } else {
        qd_track_from_tsound(t, &b->own);
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
    static int list[QD_BR_LIST];
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
    static int list[QD_BR_LIST];
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
    static int list[QD_BR_LIST];
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
    u->tk_snd.kind = (uint8_t)qd_preset_kind(u->br.own.patch);
    u->tk_snd.flags = u->br.own.edited;                 /* (the takeover: its name with a *) */
    if (had)
        u->tk_snd2 = s;
    else
        memset(&u->tk_snd2, 0, sizeof u->tk_snd2);
    br_back_to(a, reopen);                              /* (br_closed: the own sound back) */
    qd_ctl_takeover(a, QD_TK_BACK, 0, 0, 0, -1);
}
void qd_br_cancel(qd_app_t *a) { br_cancel(a, 1); }
void qd_br_home(qd_app_t *a) { br_cancel(a, 0); }

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
static void br_pool(qd_app_t *a)                       /* B10 */
{
    qd_brui_t *b = &a->ui.br;
    qd_sound_t s;
    int code = 0, k;
    if (!qd_br_chosen(a, &s, &code, 0))
        return;
    b->fx_ms = a->ms ? a->ms : 1u;
    if (b->src != QD_BR_POOL) {                         /* + POOL */
        k = qd_pool_find(a, &s);
        if (k) {
            b->fx = QD_BRFX_THERE;
            return;
        }
        k = qd_pool_add_sound(a, &s);
        b->fx = k ? QD_BRFX_IN : QD_BRFX_FULL;
        b->fx_k = k;
        return;
    }
    k = code - QD_BR_POOL_CODE;                         /* - POOL */
    if (qd_pool_users_all(a, k, 0) && !b->out_armed) {
        b->out_armed = 1;                               /* (steps use it: OUT?, B10 again) */
        b->fx = QD_BRFX_NONE;
        return;
    }
    b->out_armed = 0;
    if (b->own.sound == k)
        b->own.sound = 0;
    qd_pool_delete(a, k);
    b->fx = QD_BRFX_OUT;
    br_relist(a, -1);
    if (b->moved)
        br_apply(a);
}

static void br_delete(qd_app_t *a)                     /* B11: one of MY presets, twice */
{
    qd_brui_t *b = &a->ui.br;
    qd_sound_t s;
    int code = 0;
    if (!qd_br_chosen(a, &s, &code, 0) || code < QD_BR_MINE_CODE || code >= QD_BR_POOL_CODE)
        return;
    if (!b->del_armed) {
        b->del_armed = 1;
        return;
    }
    b->del_armed = 0;
    qd_lib_delete(a, code - QD_BR_MINE_CODE);
    br_relist(a, -1);
    if (b->moved)
        br_apply(a);
}

void qd_br_key(qd_app_t *a, int key, int w, int b, int down, int vel)
{
    qd_brui_t *br = &a->ui.br;
    if (w >= 0) {                                       /* the W keys play it, in the track's scale */
        if (down)
            qd_app_note_on(a, key, qd_snd_scale_note(a, w), vel);
        return;
    }
    if (!down)
        return;
    if (b != 9) {
        br->out_armed = 0;
        br->fx = QD_BRFX_NONE;                          /* (another list: + POOL's animation over) */
    }
    if (b != 10)
        br->del_armed = 0;
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
            static int list[QD_BR_LIST];
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
    for (i = 0; i < 8; i++)
        key[QD_BLACK_KEY[i]] = i == b->kind ? QD_LED_LIT : QD_LED_DIM;
    key[QD_BLACK_KEY[8]] = QD_LED_LIT;
    key[QD_BLACK_KEY[9]] = b->out_armed ? (fast ? QD_LED_LIT : QD_LED_OFF) : code >= 0 ? QD_LED_LIT : QD_LED_DIM;
    key[QD_BLACK_KEY[10]] = code >= QD_BR_MINE_CODE && code < QD_BR_POOL_CODE
                                ? (b->del_armed ? (fast ? QD_LED_LIT : QD_LED_OFF) : QD_LED_LIT) : QD_LED_OFF;
}

/* ---------------------------------------------------------------- SAVE PRESET */
static void sv_set_name(qd_saveui_t *v, const char *nm)
{
    int k;
    memset(v->name, ' ', QD_SOUND_NAME - 1);
    v->name[QD_SOUND_NAME - 1] = 0;
    for (k = 0; k < QD_SOUND_NAME - 1 && nm[k]; k++)
        v->name[k] = nm[k];
}
static void sv_trimmed(const qd_saveui_t *v, char *out)
{
    int e = QD_SOUND_NAME - 1, s = 0;
    memcpy(out, v->name, QD_SOUND_NAME);
    while (e > 0 && out[e - 1] == ' ')
        e--;
    out[e] = 0;
    while (out[s] == ' ')
        s++;
    if (s)
        memmove(out, out + s, (size_t)(e - s + 1));
}
/* a name no MY preset has: "DARK BASS" -> "DARK BASS 2" .. */
static void sv_unique(const qd_app_t *a, const char *nm, char *out)
{
    int n, k, clash;
    snprintf(out, QD_SOUND_NAME, "%s", nm);
    for (n = 2; n < 100; n++) {
        for (clash = 0, k = 0; k < QD_NLIB && !clash; k++)
            clash = a->per.lib[k].name[0] && !strcmp(a->per.lib[k].name, out);
        if (!clash)
            return;
        {
            char base[QD_SOUND_NAME];
            const int room = QD_SOUND_NAME - 1 - (n < 10 ? 2 : 3);
            snprintf(base, sizeof base, "%.*s", room, nm);
            while (strlen(base) && base[strlen(base) - 1] == ' ')
                base[strlen(base) - 1] = 0;
            snprintf(out, QD_SOUND_NAME, "%s %d", base, n);
        }
    }
}
static void sv_update_new(qd_app_t *a)
{
    qd_saveui_t *v = &a->ui.sv;
    char nm[QD_SOUND_NAME];
    sv_trimmed(v, nm);
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
    v->variant = 0;
    v->cur = 0;
    v->naming = 0;
    v->roll_ms = 0;
    v->replace = qd_lib_find(a, t->preset);             /* (from one of MY presets: F2 replaces it) */
    if (v->replace >= 0) {
        sv_set_name(v, a->per.lib[v->replace].name);
        v->spark = 0;
    } else {
        qd_sound_autoname(t->patch, 0, nm);
        sv_set_name(v, nm);
        v->spark = 1;
    }
    sv_update_new(a);
}

static void sv_done(qd_app_t *a, int slot)
{
    qd_ui_t *u = &a->ui;
    qd_track_t *t = &a->model.t[a->sel];
    const qd_sound_t *s = &a->per.lib[slot];
    memcpy(t->sound_name, s->name, QD_SOUND_NAME);
    t->preset = s->preset;
    t->sound = 0;
    t->edited = 0;
    u->tk_snd = *s;
    if (u->sv.ret_mode != QD_MODE_STEP && u->sv.ret_mode != QD_MODE_IDLE)
        u->mode = u->sv.ret_mode;
    qd_latch_close(a);
    u->oct_eaten |= 3u;
    qd_ctl_takeover(a, QD_TK_SAVED, 0, 0, 0, -1);
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
    sv_done(a, slot);
}

void qd_sv_replace(qd_app_t *a)
{
    qd_saveui_t *v = &a->ui.sv;
    char nm[QD_SOUND_NAME];
    if (v->replace < 0 || !a->per.lib[v->replace].name[0])
        return;                                         /* (factory presets are never replaced) */
    sv_trimmed(v, nm);
    qd_lib_replace(a, v->replace, nm, a->model.t[a->sel].patch);
    sv_done(a, v->replace);
}

void qd_sv_key(qd_app_t *a, int b, int down)
{
    qd_saveui_t *v = &a->ui.sv;
    int k;
    if (!down)
        return;
    if (b == 9) {                                       /* another name from the sound (the dice) */
        char nm[QD_SOUND_NAME];
        v->variant++;
        qd_sound_autoname(a->model.t[a->sel].patch, v->variant, nm);
        sv_set_name(v, nm);
        v->spark = 1;
        v->cur = 0;
        sv_update_new(a);
    } else if (b == 10) {                               /* a letter deleted */
        for (k = v->cur; k < QD_SOUND_NAME - 2; k++)
            v->name[k] = v->name[k + 1];
        v->name[QD_SOUND_NAME - 2] = ' ';
        v->naming = 1;
        v->spark = 0;
        sv_update_new(a);
    }
}

int qd_sv_knob(qd_app_t *a, int knob, int d)
{
    qd_saveui_t *v = &a->ui.sv;
    if (knob == QD_K_SELECT) {                          /* the letter */
        const char *p = strchr(SV_AL, v->name[v->cur]);
        const int len = (int)sizeof SV_AL - 1;
        int i = p ? (int)(p - SV_AL) : len - 2;
        i = ((i + d) % len + len) % len;
        v->name[v->cur] = SV_AL[i];
        v->roll_ms = a->ms ? a->ms : 1u;
        v->roll_dir = d > 0 ? 1 : -1;
        v->naming = 1;
        v->spark = 0;
        sv_update_new(a);
    } else if (knob == QD_K_ALGO) {                     /* the cursor */
        v->cur = v->cur + d < 0 ? 0 : v->cur + d > QD_SOUND_NAME - 2 ? QD_SOUND_NAME - 2 : v->cur + d;
        v->naming = 1;
    }
    return 1;
}

void qd_sv_leds(const qd_app_t *a, uint8_t *key)
{
    int i;
    (void)a;
    for (i = 0; i < 16; i++)
        key[QD_WHITE_KEY[i]] = QD_LED_OFF;
    for (i = 0; i < 11; i++)
        key[QD_BLACK_KEY[i]] = QD_LED_OFF;
    key[QD_BLACK_KEY[9]] = QD_LED_LIT;
    key[QD_BLACK_KEY[10]] = QD_LED_DIM;
}
