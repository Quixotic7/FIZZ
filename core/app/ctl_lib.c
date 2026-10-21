/* SPDX-License-Identifier: GPL-3.0-only */
/* The sounds page, the name editor and the project pages (P1.5a; PLAN.md "P1.5a, saving, the library and the pool",
 * mockups/fizz-save-pool.html; ctl.h). Built through ctl_latch.c (it includes this file). Single clicks: a press again
 * confirms, nothing is held to confirm; SHIFT + a button stays free (P1.5b's snapshots).
 *
 *   SAVE tapped (anywhere)   THE SOUNDS PAGE on the LIBRARY, STORING the selected track's sound: the first free slot
 *        chosen, a name from the sound. SAVE again stores it (a slot that holds a sound: the first SAVE arms it, orange,
 *        SAVE again replaces it; anything else cancels). Stored: F2 blinks; F2 adds it to the project's POOL (its first
 *        free slot) and the page turns to the POOL.
 *   On the sounds page: B1..B8 the bank, W1..W16 its sounds (tapped: chosen, played once; held: played while held,
 *        the cassette and its wave on the screen), SELECT / PRESETS through all 128, B9 LIBRARY / POOL. F2 tapped: in
 *        the library the chosen sound into the pool, in the pool onto the selected track (the step latch's selection:
 *        locked on its steps). B10: the name editor. F1 + W copies, another W pastes (B9 between: across library and
 *        pool; onto a sound: armed first, that W again pastes). F2 + W picks up, another W drops (onto a sound: the two
 *        swap; in the pool every lock and track source follows). B11 + W deletes (its users flash, it fizzes away;
 *        INIT stays). OCT- tapped or HOME leaves.
 *   THE NAME EDITOR (B10): twelve cells; SELECT the letter, ALGORITHM the cursor, B11 deletes a letter, B10 again
 *        another name from the sound, F2 keeps, OCT- leaves the old name.
 *   SEQ tapped (SEQ held stays the patterns)  THE PROJECT: its number and name, its 64 patterns, the flash and the pool
 *        in use. SELECT or SEQ again: THE PROJECTS, eight on W1..W8: W chooses (it blinks with F2), F2 loads (an empty
 *        one: a new project), F1 + W copies one onto another (onto one that exists: armed, that W again), B11 + W
 *        deletes one, B10 names the one worked on. */

#define LIB_PLAY_MS 600u               /* a sound tapped plays this long */
#define LIB_FX_MS 900u                 /* an animation's length (stored, added, deleted, onto the track) */

static const char LIB_AL[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -";

int qd_lib_open(const qd_app_t *a)
{
    return a->ui.mode == QD_MODE_STEP &&
           (a->ui.latch == QD_LATCH_LIB || a->ui.latch == QD_LATCH_NAME || a->ui.latch == QD_LATCH_PRJ);
}

/* a slot's sound (tab 0 the library 0..127, 1 the pool: number - 1) */
const qd_sound_t *qd_lib_sound(const qd_app_t *a, int tab, int slot)
{
    slot = slot < 0 ? 0 : slot > 127 ? 127 : slot;
    return tab ? &a->proj.sound[slot] : &a->per.lib[slot];
}
static int slot_used(const qd_app_t *a, int tab, int slot) { return qd_lib_sound(a, tab, slot)->name[0] != 0; }

static void lib_fx(qd_app_t *a, int kind, int slot)
{
    qd_libui_t *l = &a->ui.lb;
    l->fx_kind = kind;
    l->fx_slot = slot;
    l->fx_ms = a->ms ? a->ms : 1u;
}

/* a sound heard: once (LIB_PLAY_MS), or while held (released by lib_stop) */
static void lib_stop(qd_app_t *a)
{
    qd_libui_t *l = &a->ui.lb;
    if (l->pv_on) {
        qd_pool_note_off(&a->pool, l->pv_track, 60);
        l->pv_on = 0;
    }
}
static void lib_play(qd_app_t *a, const int16_t *patch)
{
    qd_libui_t *l = &a->ui.lb;
    lib_stop(a);
    l->pv_track = a->sel;
    qd_pool_note_on(&a->pool, a->sel, &a->model.t[a->sel].owner, 60, 100, patch);
    l->pv_on = 1;
    l->pv_ms = a->ms;
}

static void cancel_arms(qd_libui_t *l) { l->armed = 0, l->f1_armed = -1; }

/* ---------------------------------------------------------------- open / leave */
static void lib_open_page(qd_app_t *a, int kind)
{
    qd_ui_t *u = &a->ui;
    if (u->mode != QD_MODE_STEP)
        qd_ctl_set_mode(a, QD_MODE_STEP);
    if (!u->slatch)                                     /* (the fingers' held steps; the step latch's selection stays:
                                                         * F2 in the pool locks on it) */
        qd_step_release_all(a);
    u->tk = QD_TK_NONE;
    qd_latch_open(a, kind);
}

static void lib_start_store(qd_app_t *a)
{
    qd_libui_t *l = &a->ui.lb;
    const int f = qd_lib_free_slot(a);
    l->tab = 0;
    l->chosen = f >= 0 ? f : l->chosen;
    l->storing = 1;
    l->armed = 0;
    l->offer = 0;
    l->namevar = 0;
    qd_sound_autoname(a->model.t[a->sel].patch, 0, l->sname);
    l->spark = 1;
}

/* SAVE (not with SHIFT: P1.5b's) */
void qd_lib_save(qd_app_t *a)
{
    qd_libui_t *l = &a->ui.lb;
    if (a->ui.latch != QD_LATCH_LIB) {
        lib_open_page(a, QD_LATCH_LIB);
        lib_start_store(a);
        return;
    }
    if (!l->storing) {
        lib_start_store(a);
        return;
    }
    if (slot_used(a, 0, l->chosen) && !l->armed) {      /* a sound there: armed first */
        l->armed = 1;
        return;
    }
    qd_lib_store(a, l->chosen, l->armed ? qd_lib_sound(a, 0, l->chosen)->name : l->sname, a->model.t[a->sel].patch);
    l->storing = 0;
    l->armed = 0;
    l->offer = 1;                                       /* F2: into the pool */
    lib_fx(a, 1, l->chosen);
}

void qd_lib_back(qd_app_t *a)
{
    lib_stop(a);
    qd_latch_close(a);
}

/* ---------------------------------------------------------------- the sounds page's keys */
static void to_pool(qd_app_t *a, const qd_sound_t *s)
{
    qd_libui_t *l = &a->ui.lb;
    const int k = qd_pool_add(a, s->name, s->patch);
    if (!k) {
        qd_ctl_confirm(a, "POOL FULL", 0);
        return;
    }
    l->tab = 1;
    l->chosen = k - 1;
    lib_fx(a, 2, l->chosen);
}

/* F2 tapped */
static void lib_f2(qd_app_t *a)
{
    qd_libui_t *l = &a->ui.lb;
    qd_ui_t *u = &a->ui;
    if (l->offer) {                                     /* the sound just stored: into the pool */
        l->offer = 0;
        to_pool(a, qd_lib_sound(a, 0, l->chosen));
        return;
    }
    if (!slot_used(a, l->tab, l->chosen))
        return;
    if (!l->tab) {
        to_pool(a, qd_lib_sound(a, 0, l->chosen));
        return;
    }
    if (u->slatch && u->nheld) {                        /* the step latch's selection: the sound locked on it */
        int i;
        for (i = 0; i < u->nheld; i++)
            qd_step_set_sound(qd_ctl_pat(a), a->sel, u->held[i], l->chosen + 1);
        lib_fx(a, 4, l->chosen);
        return;
    }
    {
        const qd_sound_t *s = qd_lib_sound(a, 1, l->chosen);
        qd_track_load_sound(&a->model.t[a->sel], l->chosen + 1, s->name, s->patch);
        lib_fx(a, 4, l->chosen);
    }
}

static void lib_white(qd_app_t *a, int w, int down)
{
    qd_libui_t *l = &a->ui.lb;
    const int slot = (l->chosen / 16) * 16 + w;
    if (!down) {
        if (l->w_down == w + 1) {
            l->w_down = 0;
            if (l->aud) {                               /* (held: it stops with the key) */
                l->aud = 0;
                lib_stop(a);
            }
        }
        return;
    }
    if (l->f1_down) {                                   /* F1 + W: copy, then paste */
        l->oct_used |= 1;
        if (l->f1_src < 0) {
            if (slot_used(a, l->tab, slot)) {
                l->f1_src = slot, l->f1_tab = l->tab;
                l->chosen = slot;
            }
            return;
        }
        if (slot_used(a, l->tab, slot) && l->f1_armed != slot) {
            l->f1_armed = slot;                         /* onto a sound: armed, that W again pastes */
            l->chosen = slot;
            return;
        }
        {
            const qd_sound_t s = *qd_lib_sound(a, l->f1_tab, l->f1_src);
            char nm[QD_SOUND_NAME];
            int n = (int)strlen(s.name);
            memcpy(nm, s.name, sizeof nm);
            if (!slot_used(a, l->tab, slot) && l->f1_tab == l->tab && n <= QD_SOUND_NAME - 3) {
                nm[n] = ' ', nm[n + 1] = '2', nm[n + 2] = 0;   /* a free slot: "GHOST 2" */
            }
            if (l->tab) {
                if (!slot)
                    return;                             /* (INIT stays) */
                a->proj.sound[slot] = s;
                memcpy(a->proj.sound[slot].name, nm, sizeof nm);
                qd_sound_recount(&a->proj);
            } else {
                qd_lib_store(a, slot, nm, s.patch);
            }
            l->chosen = slot;
            l->f1_armed = -1;
            lib_fx(a, 2, slot);
        }
        return;
    }
    if (l->f2_down) {                                   /* F2 + W: pick up, then drop (a filled slot: swap) */
        l->oct_used |= 2;
        if (l->f2_pick < 0) {
            if (slot_used(a, l->tab, slot))
                l->f2_pick = slot, l->chosen = slot;
            return;
        }
        if (slot != l->f2_pick) {
            if (l->tab)
                qd_pool_swap(a, l->f2_pick + 1, slot + 1);
            else
                qd_lib_swap(a, l->f2_pick, slot);
        }
        l->f2_pick = -1;
        l->chosen = slot;
        lib_fx(a, 5, slot);
        return;
    }
    if (l->b11_down) {                                  /* B11 + W: delete (its users flash, it fizzes away) */
        l->b11_used = 1;
        if (!slot_used(a, l->tab, slot) || (l->tab && slot == 0))
            return;
        l->chosen = slot;
        qd_pool_users(a, slot + 1, &l->del_tracks, &l->del_steps);
        if (!l->tab)
            l->del_tracks = 0, l->del_steps = 0;
        memcpy(l->del_name, qd_lib_sound(a, l->tab, slot)->name, QD_SOUND_NAME);
        l->del_kind = qd_sound_kind(qd_lib_sound(a, l->tab, slot)->patch);
        if (l->tab)
            qd_pool_delete(a, slot + 1);
        else
            qd_lib_delete(a, slot);
        lib_fx(a, 3, slot);
        return;
    }
    /* a W: chosen (storing: the slot it goes into), played once; held: played while held */
    cancel_arms(l);
    l->chosen = slot;
    l->w_down = w + 1;
    l->w_ms = a->ms;
    if (slot_used(a, l->tab, slot) && !l->storing)
        lib_play(a, qd_lib_sound(a, l->tab, slot)->patch);
}

static void lib_black(qd_app_t *a, int b, int down)
{
    qd_libui_t *l = &a->ui.lb;
    if (b == 10) {
        l->b11_down = down;
        if (down)
            l->b11_used = 0;
        return;
    }
    if (!down)
        return;
    cancel_arms(l);
    if (b < 8) {                                        /* the bank */
        l->chosen = b * 16 + l->chosen % 16;
        return;
    }
    if (b == 8) {                                       /* LIBRARY / POOL */
        l->tab ^= 1;
        l->storing = 0;
        l->offer = 0;
        if (l->tab && !l->chosen)
            l->chosen = 0;
        return;
    }
    if (b == 9)                                         /* the name editor */
        qd_name_open(a, l->storing ? 1 : 0, -1);
}

/* ---------------------------------------------------------------- the name editor */
void qd_name_open(qd_app_t *a, int target, int proj)
{
    qd_nameui_t *n = &a->ui.nm;
    const qd_libui_t *l = &a->ui.lb;
    const char *src = "";
    int k;
    n->ret = a->ui.latch;
    n->target = target;
    n->proj = proj;
    n->variant = 0;
    n->cur = 0;
    if (target == 2)
        src = a->per.pname[proj < 0 ? 0 : proj];
    else if (target == 1)
        src = l->sname;
    else if (slot_used(a, l->tab, l->chosen))
        src = qd_lib_sound(a, l->tab, l->chosen)->name;
    else
        return;
    memset(n->buf, ' ', QD_SOUND_NAME - 1);
    n->buf[QD_SOUND_NAME - 1] = 0;
    for (k = 0; k < QD_SOUND_NAME - 1 && src[k]; k++)
        n->buf[k] = src[k];
    qd_latch_open(a, QD_LATCH_NAME);
}

static void name_keep(qd_app_t *a)
{
    qd_nameui_t *n = &a->ui.nm;
    qd_libui_t *l = &a->ui.lb;
    char out[QD_SOUND_NAME];
    int e = QD_SOUND_NAME - 1;
    memcpy(out, n->buf, QD_SOUND_NAME);
    while (e > 0 && out[e - 1] == ' ')
        e--;
    out[e] = 0;
    if (!e)
        return;                                         /* (an empty name is not kept) */
    if (n->target == 2)
        qd_proj_name(a, n->proj, out);
    else if (n->target == 1) {
        memcpy(l->sname, out, QD_SOUND_NAME);
        l->spark = 0;
    } else if (l->tab) {
        memcpy(a->proj.sound[l->chosen].name, out, QD_SOUND_NAME);
    } else {
        memcpy(a->per.lib[l->chosen].name, out, QD_SOUND_NAME);
    }
}

static void name_back(qd_app_t *a) { qd_latch_open(a, a->ui.nm.ret == QD_LATCH_PRJ ? QD_LATCH_PRJ : QD_LATCH_LIB); }

static void name_black(qd_app_t *a, int b)
{
    qd_nameui_t *n = &a->ui.nm;
    int k;
    if (b == 10) {                                      /* a letter deleted */
        for (k = n->cur; k < QD_SOUND_NAME - 2; k++)
            n->buf[k] = n->buf[k + 1];
        n->buf[QD_SOUND_NAME - 2] = ' ';
        return;
    }
    if (b == 9) {                                       /* another name from the sound (the dice) */
        const qd_libui_t *l = &a->ui.lb;
        char nm[QD_SOUND_NAME];
        n->variant++;
        if (n->target == 2)
            snprintf(nm, sizeof nm, "SKETCH %d", n->variant + 1);
        else
            qd_sound_autoname(n->target == 1 ? a->model.t[a->sel].patch : qd_lib_sound(a, l->tab, l->chosen)->patch,
                              n->variant, nm);
        memset(n->buf, ' ', QD_SOUND_NAME - 1);
        for (k = 0; k < QD_SOUND_NAME - 1 && nm[k]; k++)
            n->buf[k] = nm[k];
        n->cur = 0;
    }
}

static void name_knob(qd_app_t *a, int knob, int d)
{
    qd_nameui_t *n = &a->ui.nm;
    if (knob == QD_K_SELECT) {                          /* the letter */
        const char *p = strchr(LIB_AL, n->buf[n->cur]);
        const int len = (int)sizeof LIB_AL - 1;
        int i = p ? (int)(p - LIB_AL) : len - 2;
        i = ((i + d) % len + len) % len;
        n->buf[n->cur] = LIB_AL[i];
        n->roll_ms = a->ms ? a->ms : 1u;
        n->roll_dir = d > 0 ? 1 : -1;
    } else if (knob == QD_K_ALGO) {                     /* the cursor */
        n->cur = n->cur + d < 0 ? 0 : n->cur + d > QD_SOUND_NAME - 2 ? QD_SOUND_NAME - 2 : n->cur + d;
    }
}

/* ---------------------------------------------------------------- the project pages */
void qd_prj_button(qd_app_t *a)
{
    qd_prjui_t *p = &a->ui.pj;
    if (a->ui.latch == QD_LATCH_PRJ) {
        p->page ^= 1;
        p->armed = -1;
        return;
    }
    lib_open_page(a, QD_LATCH_PRJ);
    p->page = 0;
    p->chosen = a->per.proj;
    p->src = -1;
    p->armed = -1;
}

static void prj_white(qd_app_t *a, int w, int down)
{
    qd_prjui_t *p = &a->ui.pj;
    if (!down || p->page != 1 || w >= QD_NPROJ)
        return;
    if (p->f1_down) {                                   /* F1 + W: copy one onto another */
        p->oct_used |= 1;
        if (p->src < 0) {
            if (a->per.pexists[w])
                p->src = w;
            return;
        }
        if (w == p->src)
            return;
        if (a->per.pexists[w] && p->armed != w) {
            p->armed = w;
            return;
        }
        qd_proj_copy(a, p->src, w);
        p->src = -1;
        p->armed = -1;
        p->chosen = w;
        return;
    }
    if (p->b11_down) {                                  /* B11 + W: delete */
        p->b11_used = 1;
        if (a->per.pexists[w])
            qd_proj_delete(a, w);
        return;
    }
    p->chosen = w;
    p->armed = -1;
}

static void prj_f2(qd_app_t *a)
{
    qd_prjui_t *p = &a->ui.pj;
    if (p->page != 1)
        return;
    if (p->chosen == a->per.proj)
        return;
    if (a->per.pexists[p->chosen])
        qd_proj_load(a, p->chosen);
    else
        qd_proj_new(a, p->chosen);
    p->page = 0;
}

static void prj_black(qd_app_t *a, int b, int down)
{
    qd_prjui_t *p = &a->ui.pj;
    if (b == 10) {
        p->b11_down = down;
        return;
    }
    if (down && b == 9)                                 /* B10: the one worked on named */
        qd_name_open(a, 2, a->per.proj);
}

/* ---------------------------------------------------------------- the entry points (ctl.c's rows) */
void qd_lib_key(qd_app_t *a, int w, int b, int down)
{
    if (a->ui.latch == QD_LATCH_NAME) {
        if (down && b >= 0)
            name_black(a, b);
        return;
    }
    if (a->ui.latch == QD_LATCH_PRJ) {
        if (w >= 0)
            prj_white(a, w, down);
        else
            prj_black(a, b, down);
        return;
    }
    if (w >= 0)
        lib_white(a, w, down);
    else
        lib_black(a, b, down);
}

int qd_lib_knob(qd_app_t *a, int knob, int d)
{
    qd_libui_t *l = &a->ui.lb;
    if (a->ui.latch == QD_LATCH_NAME) {
        name_knob(a, knob, d);
        return 1;
    }
    if (a->ui.latch == QD_LATCH_PRJ) {
        if (knob == QD_K_SELECT)
            a->ui.pj.page = d > 0;
        return 1;
    }
    if (knob == QD_K_SELECT || knob == QD_K_PRESETS) {  /* through all 128 */
        cancel_arms(l);
        d = qd_ctl_acc(a, d, 128);                      /* (P1.5b: spun, a sixteenth a detent) */
        l->chosen = l->chosen + d < 0 ? 0 : l->chosen + d > 127 ? 127 : l->chosen + d;
    }
    return 1;
}

/* the OCT keys on these pages: OCT- tapped back, held + W copy; OCT+ tapped F2's job, held + W pick up / drop. 1 taken */
int qd_lib_oct(qd_app_t *a, int d, int down)
{
    qd_ui_t *u = &a->ui;
    if (!qd_lib_open(a))
        return 0;
    if (u->latch == QD_LATCH_NAME) {
        if (down)
            return 1;
        if (d > 0) {
            name_keep(a);
            name_back(a);
        } else {
            name_back(a);
        }
        return 1;
    }
    if (u->latch == QD_LATCH_PRJ) {
        qd_prjui_t *p = &u->pj;
        if (down) {
            if (d < 0)
                p->f1_down = 1, p->src = -1;
            p->oct_used &= ~(d < 0 ? 1 : 2);
            return 1;
        }
        if (d < 0) {
            p->f1_down = 0;
            if (!(p->oct_used & 1))
                qd_lib_back(a);
        } else if (!(p->oct_used & 2)) {
            prj_f2(a);
        }
        return 1;
    }
    {
        qd_libui_t *l = &u->lb;
        if (down) {
            if (d < 0)
                l->f1_down = 1, l->f1_src = -1, l->f1_armed = -1;
            else
                l->f2_down = 1, l->f2_pick = -1;
            l->oct_used &= ~(d < 0 ? 1 : 2);
            l->armed = 0;
            return 1;
        }
        if (d < 0) {
            l->f1_down = 0;
            if (!(l->oct_used & 1))
                qd_lib_back(a);
        } else {
            l->f2_down = 0;
            if (!(l->oct_used & 2))
                lib_f2(a);
        }
        return 1;
    }
}

/* each ms: a sound tapped stops after LIB_PLAY_MS; a W held past QD_TAP_MS: heard while held, on the screen */
void qd_lib_ms(qd_app_t *a)
{
    qd_libui_t *l = &a->ui.lb;
    if (l->w_down && !l->aud && a->ms - l->w_ms >= QD_TAP_MS && slot_used(a, l->tab, l->chosen) && !l->storing) {
        l->aud = 1;
        if (!l->pv_on)
            lib_play(a, qd_lib_sound(a, l->tab, l->chosen)->patch);
    }
    if (l->pv_on && !l->aud && a->ms - l->pv_ms >= LIB_PLAY_MS)
        lib_stop(a);
    if (!qd_lib_open(a) && l->pv_on)
        lib_stop(a);
}
