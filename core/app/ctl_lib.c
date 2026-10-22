/* SPDX-License-Identifier: GPL-3.0-only */
/* The pages over the step view (P1.5a; P1.5t: the sounds page's grid is gone, PLAN.md "The sound system,
 * redesigned"): the library browser and SAVE PRESET (ctl_presets.c), the name editor (a project's name), the project
 * pages; their keys, knobs, OCT keys and clock (ctl.c's rows call these). Built through ctl_latch.c (it includes this
 * file). Single clicks: a press again confirms, nothing is held to confirm; SHIFT + a button stays free (P1.5b's
 * snapshots).
 *
 *   PRESETS (nothing held)   THE LIBRARY BROWSER (ctl_presets.c)
 *   SAVE (anywhere)          SAVE PRESET (ctl_presets.c)
 *   THE NAME EDITOR (B10 on the project page): P1.5u: the typewriter (ctl_type.c): the project's name selected, the
 *        keys type; SAVE keeps it, HOME leaves the old name (both back to the project page).
 *   SEQ tapped (SEQ held stays the patterns)  THE PROJECT: its number and name, its 64 patterns, the flash and the pool
 *        in use. SELECT or SEQ again: THE PROJECTS, eight on W1..W8: W chooses (it blinks with F2), F2 loads (an empty
 *        one: a new project), F1 + W copies one onto another (onto one that exists: armed, that W again), B11 + W
 *        deletes one, B10 names the one worked on. */

int qd_lib_open(const qd_app_t *a)
{
    return a->ui.mode == QD_MODE_STEP && (a->ui.latch == QD_LATCH_BROWSE || a->ui.latch == QD_LATCH_SAVE ||
                                          a->ui.latch == QD_LATCH_NAME || a->ui.latch == QD_LATCH_PRJ);
}

static void lib_open_page(qd_app_t *a, int kind)
{
    qd_ui_t *u = &a->ui;
    if (u->mode != QD_MODE_STEP)
        qd_ctl_set_mode(a, QD_MODE_STEP);
    if (!u->slatch)
        qd_step_release_all(a);
    u->tk = QD_TK_NONE;
    qd_latch_open(a, kind);
}

/* SAVE (not with SHIFT: P1.5b's) */
void qd_lib_save(qd_app_t *a) { qd_sv_open(a); }

void qd_lib_back(qd_app_t *a) { qd_latch_close(a); }

/* ---------------------------------------------------------------- the name editor (a project's) */
void qd_name_open(qd_app_t *a, int target, int proj)
{
    qd_nameui_t *n = &a->ui.nm;
    n->ret = a->ui.latch;
    n->target = target;
    n->proj = proj;
    qd_latch_open(a, QD_LATCH_NAME);
    qd_type_start(a, a->per.pname[proj < 0 ? 0 : proj], 0);
}

static void name_keep(qd_app_t *a)
{
    char out[QD_SOUND_NAME];
    qd_type_trimmed(a, out);
    if (!out[0])
        return;                                         /* (an empty name is not kept) */
    qd_proj_name(a, a->ui.nm.proj, out);
}

static void name_back(qd_app_t *a) { qd_latch_open(a, QD_LATCH_PRJ); }

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
void qd_lib_key(qd_app_t *a, int key, int w, int b, int down, int vel)
{
    if (a->ui.latch == QD_LATCH_BROWSE) {
        qd_br_key(a, key, w, b, down, vel);
        return;
    }
    if (a->ui.latch == QD_LATCH_SAVE || a->ui.latch == QD_LATCH_NAME)
        return;                                         /* (P1.5u: the typewriter's rows take the keys first) */
    if (w >= 0)
        prj_white(a, w, down);
    else
        prj_black(a, b, down);
}

int qd_lib_knob(qd_app_t *a, int knob, int d)
{
    if (a->ui.latch == QD_LATCH_BROWSE)
        return qd_br_knob(a, knob, d);
    if (a->ui.latch == QD_LATCH_SAVE || a->ui.latch == QD_LATCH_NAME)
        return qd_type_knob(a, knob, d);                /* P1.5u: the typewriter */
    if (knob == QD_K_SELECT)
        a->ui.pj.page = d > 0;
    return 1;
}

/* the OCT keys on these pages, at their release: the browser OCT- cancels, OCT+ loads; the projects OCT- back (held +
 * W copies), OCT+ loads (P1.5u: SAVE PRESET's and the name editor's are the typewriter's: qd_type_button). 1 taken */
int qd_lib_oct(qd_app_t *a, int d, int down)
{
    qd_ui_t *u = &a->ui;
    if (!qd_lib_open(a))
        return 0;
    if (u->latch == QD_LATCH_BROWSE) {
        if (down)
            return 1;
        if (d > 0)
            qd_br_load(a);
        else
            qd_br_cancel(a);
        u->oct_eaten &= (uint8_t)~(d < 0 ? 1u : 2u);
        return 1;
    }
    if (u->latch == QD_LATCH_SAVE || u->latch == QD_LATCH_NAME)
        return 1;
    {
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
}

void qd_lib_ms(qd_app_t *a) { (void)a; }
