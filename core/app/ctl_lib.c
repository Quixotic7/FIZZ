/* SPDX-License-Identifier: GPL-3.0-only */
/* The pages over the step view (P1.5a; P1.5t: the sounds page's grid is gone, PLAN.md "The sound system,
 * redesigned"): the library browser and SAVE PRESET (ctl_presets.c), the name editor (a project's name), the project
 * pages; their keys, knobs, OCT keys and clock (ctl.c's rows call these). Built through ctl_latch.c (it includes this
 * file). Single clicks: a press again confirms, nothing is held to confirm; SHIFT + a button stays free (P1.5b's
 * snapshots).
 *
 *   PRESETS (nothing held)   THE LIBRARY BROWSER (ctl_presets.c)
 *   SAVE (anywhere)          SAVE PRESET (ctl_presets.c)
 *   THE NAME EDITOR (B10 on the project page): twelve cells; SELECT the letter, ALGORITHM the cursor, B11 deletes a
 *        letter, B10 again another name, F2 keeps, OCT- leaves the old name.
 *   SEQ tapped (SEQ held stays the patterns)  THE PROJECT: its number and name, its 64 patterns, the flash and the pool
 *        in use. SELECT or SEQ again: THE PROJECTS, eight on W1..W8: W chooses (it blinks with F2), F2 loads (an empty
 *        one: a new project), F1 + W copies one onto another (onto one that exists: armed, that W again), B11 + W
 *        deletes one, B10 names the one worked on. */

static const char LIB_AL[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -";

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
    const char *src;
    int k;
    n->ret = a->ui.latch;
    n->target = target;
    n->proj = proj;
    n->variant = 0;
    n->cur = 0;
    src = a->per.pname[proj < 0 ? 0 : proj];
    memset(n->buf, ' ', QD_SOUND_NAME - 1);
    n->buf[QD_SOUND_NAME - 1] = 0;
    for (k = 0; k < QD_SOUND_NAME - 1 && src[k]; k++)
        n->buf[k] = src[k];
    qd_latch_open(a, QD_LATCH_NAME);
}

static void name_keep(qd_app_t *a)
{
    qd_nameui_t *n = &a->ui.nm;
    char out[QD_SOUND_NAME];
    int e = QD_SOUND_NAME - 1;
    memcpy(out, n->buf, QD_SOUND_NAME);
    while (e > 0 && out[e - 1] == ' ')
        e--;
    out[e] = 0;
    if (!e)
        return;                                         /* (an empty name is not kept) */
    qd_proj_name(a, n->proj, out);
}

static void name_back(qd_app_t *a) { qd_latch_open(a, QD_LATCH_PRJ); }

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
    if (b == 9) {                                       /* another name (the dice) */
        char nm[QD_SOUND_NAME];
        n->variant++;
        snprintf(nm, sizeof nm, "SKETCH %d", n->variant + 1);
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
void qd_lib_key(qd_app_t *a, int key, int w, int b, int down, int vel)
{
    if (a->ui.latch == QD_LATCH_BROWSE) {
        qd_br_key(a, key, w, b, down, vel);
        return;
    }
    if (a->ui.latch == QD_LATCH_SAVE) {
        if (b >= 0)
            qd_sv_key(a, b, down);
        return;
    }
    if (a->ui.latch == QD_LATCH_NAME) {
        if (down && b >= 0)
            name_black(a, b);
        return;
    }
    if (w >= 0)
        prj_white(a, w, down);
    else
        prj_black(a, b, down);
}

int qd_lib_knob(qd_app_t *a, int knob, int d)
{
    if (a->ui.latch == QD_LATCH_BROWSE)
        return qd_br_knob(a, knob, d);
    if (a->ui.latch == QD_LATCH_SAVE)
        return qd_sv_knob(a, knob, d);
    if (a->ui.latch == QD_LATCH_NAME) {
        name_knob(a, knob, d);
        return 1;
    }
    if (knob == QD_K_SELECT)
        a->ui.pj.page = d > 0;
    return 1;
}

/* the OCT keys on these pages, at their release: the browser OCT- cancels, OCT+ loads; SAVE PRESET OCT- cancels,
 * OCT+ replaces; the name editor OCT- back, OCT+ keeps; the projects OCT- back (held + W copies), OCT+ loads. 1 taken */
int qd_lib_oct(qd_app_t *a, int d, int down)
{
    qd_ui_t *u = &a->ui;
    if (!qd_lib_open(a))
        return 0;
    if (u->latch == QD_LATCH_BROWSE || u->latch == QD_LATCH_SAVE) {
        if (down)
            return 1;
        if (u->latch == QD_LATCH_BROWSE) {
            if (d > 0)
                qd_br_load(a);
            else
                qd_br_cancel(a);
        } else if (d > 0) {
            qd_sv_replace(a);
        } else {
            if (u->sv.ret_mode != QD_MODE_STEP && u->sv.ret_mode != QD_MODE_IDLE)
                u->mode = u->sv.ret_mode;
            qd_latch_close(a);
        }
        u->oct_eaten &= (uint8_t)~(d < 0 ? 1u : 2u);
        return 1;
    }
    if (u->latch == QD_LATCH_NAME) {
        if (down)
            return 1;
        if (d > 0)
            name_keep(a);
        name_back(a);
        return 1;
    }
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
