/* SPDX-License-Identifier: GPL-3.0-only */
/* The sound editor (P1.4a; PLAN.md §8b "P1.4a, the sound editor on EDIT"; ctl.h). Built through ctl_latch.c (it
 * includes this file: the editor is a latched menu, QD_LATCH_SND, and the core's source lists stay as they are).
 *
 * EDIT (printed) opens it for the selected track; EDIT again, OCT- or HOME close it (the latch rule). FM TONE's 73
 * values and DRIVE are on QD_SND_NPAGE pages in five groups, one a black key: B1 OPERATORS, B2 OP ENVELOPES, B3 FILTERS,
 * B4 ENVELOPES (the amp's, LEVEL, DRIVE), B5 MODULATION (the three LFOs); a group's key again shows its next page
 * (round). SELECT steps through every page. KNOB 1..4 are the page's values (their colours), ALGORITHM a fifth where
 * the page has one (OPERATORS' first page: the algorithm). SHIFT + a knob: the finest step (a hundredth of a hi-res
 * value). PRESETS: the track's preset (a step held: its sound lock).
 *
 * Nothing held, a turn edits the track's sound live (qd_player_live_edit: the sounding voices follow, their own locks
 * stay). A finger on a step (held, or the step latch's selection), every turn is a p-lock on the held steps
 * (qd_ctl_lock_screen: a table a page, the P-LOCK hook of P1.3c), and B11 clears the held steps' locks on this page.
 * The W keys stay the steps (tap, hold, the latch). */

/* the pages: name, group, drawing (ui.h UI_SD_*), the ids on KNOB 1..4 and ALGORITHM (-1 none), the drawing's arg */
#define L(k, f) (QD_QP_LFO((k), (f)))
const qd_sndpage_t QD_SND_PAGES[QD_SND_NPAGE] = {
    {"RATIO", QD_SG_OPS, UI_SD_ALGO, {QD_QP_RC, QD_QP_RA, QD_QP_RB1, QD_QP_RB2, QD_QP_ALGO}, 0},
    {"TONE", QD_SG_OPS, UI_SD_TONE, {QD_QP_HARM, QD_QP_DTUN, QD_QP_FDBK, QD_QP_MIX, -1}, 0},
    {"OFFSET", QD_SG_OPS, UI_SD_OFS, {QD_QP_OFSC, QD_QP_OFSA, QD_QP_OFSB1, QD_QP_OFSB2, -1}, 0},
    {"ENV A", QD_SG_OPENV, UI_SD_OPENV, {QD_QP_AATK, QD_QP_ADEC, QD_QP_AEND, QD_QP_ALEV, QD_QP_ADLY}, 0},
    {"ENV B", QD_SG_OPENV, UI_SD_OPENV, {QD_QP_BATK, QD_QP_BDEC, QD_QP_BEND, QD_QP_BLEV, QD_QP_BDLY}, 1},
    {"TRIG", QD_SG_OPENV, UI_SD_TRIG, {QD_QP_ATRIG, QD_QP_ARST, QD_QP_BTRIG, QD_QP_BRST, QD_QP_PHRT}, 0},
    {"KEYS", QD_SG_OPENV, UI_SD_KEYS, {QD_QP_AKEY, QD_QP_B1KEY, QD_QP_B2KEY, QD_QP_VEL, -1}, 0},
    {"FILTER", QD_SG_FILTER, UI_SD_FILTER, {QD_QP_FREQ, QD_QP_RESO, QD_QP_FTYPE, QD_QP_FDEPTH, -1}, 0},
    {"F ENV", QD_SG_FILTER, UI_SD_ADSR, {QD_QP_FATK, QD_QP_FDEC, QD_QP_FSUS, QD_QP_FREL, QD_QP_FDLY}, 0},
    {"BASE", QD_SG_FILTER, UI_SD_BAND, {QD_QP_BASE, QD_QP_WIDTH, QD_QP_FKTRK, -1, -1}, 0},
    {"AMP", QD_SG_AMP, UI_SD_ADSR, {QD_QP_EATK, QD_QP_EDEC, QD_QP_ESUS, QD_QP_EREL, -1}, 1},
    {"LEVEL", QD_SG_AMP, UI_SD_DRIVE, {QD_QP_LEVEL, QD_QP_DRIVE, -1, -1, -1}, 0},
    {"LFO 1", QD_SG_MOD, UI_SD_LFO, {L(0, QD_QL_SPEED), L(0, QD_QL_MULT), L(0, QD_QL_DEPTH), L(0, QD_QL_FADE),
                                      L(0, QD_QL_DEST)}, 0},
    {"LFO 1 WAVE", QD_SG_MOD, UI_SD_LFOSHAPE, {L(0, QD_QL_WAVE), L(0, QD_QL_PHASE), L(0, QD_QL_TRIG), -1, -1}, 0},
    {"LFO 2", QD_SG_MOD, UI_SD_LFO, {L(1, QD_QL_SPEED), L(1, QD_QL_MULT), L(1, QD_QL_DEPTH), L(1, QD_QL_FADE),
                                      L(1, QD_QL_DEST)}, 1},
    {"LFO 2 WAVE", QD_SG_MOD, UI_SD_LFOSHAPE, {L(1, QD_QL_WAVE), L(1, QD_QL_PHASE), L(1, QD_QL_TRIG), -1, -1}, 1},
    {"LFO 3", QD_SG_MOD, UI_SD_LFO, {L(2, QD_QL_SPEED), L(2, QD_QL_MULT), L(2, QD_QL_DEPTH), L(2, QD_QL_FADE),
                                      L(2, QD_QL_DEST)}, 2},
    {"LFO 3 WAVE", QD_SG_MOD, UI_SD_LFOSHAPE, {L(2, QD_QL_WAVE), L(2, QD_QL_PHASE), L(2, QD_QL_TRIG), -1, -1}, 2},
};
#undef L
const char *const QD_SND_GROUP_NAME[QD_NSG] = {"OPERATORS", "OP ENVELOPES", "FILTERS", "ENVELOPES", "MODULATION"};

/* the editor's knob slots: KNOB 1..4, ALGORITHM */
static const uint8_t SND_KNOB[5] = {QD_K_1, QD_K_2, QD_K_3, QD_K_4, QD_K_ALGO};

/* P-LOCK's hook: a lock table a page (built once from the pages; the knobs' own takeover none: the page shows it) */
static qd_lockknob_t SND_LOCKS[QD_SND_NPAGE][6];
static qd_lockscreen_t SND_SCREEN[QD_SND_NPAGE];
const qd_lockscreen_t *qd_snd_lock_screen(int page)
{
    static int built;
    if (!built) {
        int p, k;
        for (p = 0; p < QD_SND_NPAGE; p++) {
            int n = 0;
            for (k = 0; k < 5; k++)
                if (QD_SND_PAGES[p].id[k] >= 0) {
                    SND_LOCKS[p][n].knob = SND_KNOB[k];
                    SND_LOCKS[p][n].id = QD_SND_PAGES[p].id[k];
                    SND_LOCKS[p][n++].tk = QD_TK_NONE;
                }
            SND_LOCKS[p][n].knob = QD_K_PRESETS;      /* PRESETS: the sound lock, as everywhere */
            SND_LOCKS[p][n].id = QD_LOCK_SOUND;
            SND_LOCKS[p][n++].tk = QD_TK_PRESET;
            SND_SCREEN[p].name = QD_SND_PAGES[p].name;
            SND_SCREEN[p].n = n;
            SND_SCREEN[p].k = SND_LOCKS[p];
        }
        built = 1;
    }
    return &SND_SCREEN[page < 0 ? 0 : page >= QD_SND_NPAGE ? QD_SND_NPAGE - 1 : page];
}

int qd_snd_open(const qd_app_t *a) { return a->ui.mode == QD_MODE_STEP && a->ui.latch == QD_LATCH_SND; }
int qd_snd_keys(const qd_app_t *a) { return qd_snd_open(a) && a->ui.ekeys; }
int qd_keys_on(const qd_app_t *a) { return a->ui.mode == QD_MODE_KEYS || qd_snd_keys(a); }

/* P1.5b: the editor's SCALE keys: W1 the root at the keyboard's octave (octave 3 + the shift: C3 = 48), then each
 * degree of the track's scale upwards (chromatic: a semitone each) */
int qd_snd_scale_note(const qd_app_t *a, int w)
{
    int sc, rt;
    qd_track_key(&a->proj, qd_ctl_cpat(a), a->sel, &sc, &rt);
    return qd_scale_note(sc, rt, 3 + a->oct, w);
}

/* P1.5b: the keys by the scale: the roots 80 %, the other notes of the scale 50 %, the rest dark, a key sounding (or
 * held) full. The keyboard (and the editor's FULL): each key's own note (before the snap); the editor's SCALE: the
 * white keys (every one in the scale), the black keys the editor's pages */
void qd_keys_leds(const qd_app_t *a, uint8_t *key)
{
    uint8_t notes[128];
    int sc, rt, i;
    const int scale = qd_snd_keys(a) && !a->proj.rec_ekeys;
    qd_ctl_sounding(a, notes);
    qd_track_key(&a->proj, qd_ctl_cpat(a), a->sel, &sc, &rt);
    for (i = 0; i < QD_NKEY; i++) {
        const int w = qd_ctl_white(i);
        int n, nn;
        if (scale) {
            if (w < 0)
                continue;                               /* (the pages: the editor's own) */
            n = qd_snd_scale_note(a, w);
            nn = n;
        } else {
            n = QD_NOTE0 + i + 12 * a->oct;
            nn = qd_rec_key_note(a, i);
        }
        if (((a->keys_held >> i) & 1u) || (nn >= 0 && nn < 128 && notes[nn]))
            key[i] = QD_LED_LIT;
        else if (n >= 0 && n < 128 && ((n % 12) + 12) % 12 == rt)
            key[i] = QD_LED_80;
        else if (n >= 0 && n < 128 && qd_scale_has(sc, rt, n))
            key[i] = QD_LED_HALF;
        else
            key[i] = QD_LED_OFF;
    }
}

/* EDIT (printed): open (in the step view, the screen's latches and the hold given up), or close. P1.5b: opened from
 * the keyboard, the keyboard stays on in it; closed with the keyboard on, back to the keyboard */
void qd_snd_button(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    if (qd_snd_open(a)) {
        const int keys = u->ekeys;
        qd_latch_close(a);
        if (keys)
            qd_ctl_set_mode(a, QD_MODE_KEYS);
        return;
    }
    qd_latch_open(a, QD_LATCH_SND);
    u->snd_knob = -1;
}

/* a black key: B1..B5 the group (again: its next page), B11 a step held: its locks on this page cleared */
void qd_snd_black(qd_app_t *a, int b)
{
    qd_ui_t *u = &a->ui;
    int p, first = -1, next = -1;
    if (b == 10) {
        const qd_sndpage_t *pg = &QD_SND_PAGES[u->snd_page];
        int i, k, n = 0;
        if (!u->nheld)
            return;
        for (i = 0; i < u->nheld; i++)
            for (k = 0; k < 5; k++)
                if (pg->id[k] >= 0) {
                    int16_t v;
                    n += qd_lock_get(qd_ctl_cpat(a), a->sel, u->held[i], pg->id[k], &v);
                }
        if (!n)
            return;
        qd_clip_keep(&u->clip, qd_ctl_cpat(a), a->seq.cur, a->sel);
        for (i = 0; i < u->nheld; i++)
            for (k = 0; k < 5; k++)
                if (pg->id[k] >= 0)
                    qd_lock_clear(qd_ctl_pat(a), a->sel, u->held[i], pg->id[k]);
        qd_ctl_confirm(a, "LOCKS CLEARED", 0);
        return;
    }
    if (b >= QD_NSG)
        return;
    for (p = 0; p < QD_SND_NPAGE; p++) {
        if (QD_SND_PAGES[p].group != b)
            continue;
        if (first < 0)
            first = p;
        if (next < 0 && p > u->snd_page && QD_SND_PAGES[u->snd_page].group == b)
            next = p;
    }
    u->snd_page = QD_SND_PAGES[u->snd_page].group == b && next >= 0 ? next : first;
    u->snd_knob = -1;
}

/* a knob; 1: taken */
int qd_snd_knob(qd_app_t *a, int knob, int d)
{
    qd_ui_t *u = &a->ui;
    const qd_sndpage_t *pg = &QD_SND_PAGES[u->snd_page];
    const int fine = qd_ctl_shift(a);
    int k, id = -1;
    if (knob == QD_K_SELECT) {
        if (fine && u->ekeys)                           /* (P1.5b: the keyboard on, SHIFT + SELECT: nothing) */
            return 1;
        u->snd_page = (u->snd_page + d % QD_SND_NPAGE + QD_SND_NPAGE) % QD_SND_NPAGE;
        u->snd_knob = -1;
        return 1;
    }
    if (knob == QD_K_PRESETS) {
        if (u->nheld)
            return qd_step_lock_knob(a, knob, d);       /* the sound lock */
        return 0;                                       /* (the track's preset: the step view's) */
    }
    for (k = 0; k < 5; k++)
        if (SND_KNOB[k] == knob)
            id = pg->id[k], u->snd_knob = k;
    u->snd_ms = a->ms;
    if (id < 0)
        return 1;
    if (u->nheld) {                                     /* a finger on a step: a p-lock */
        if (!fine)
            return qd_step_lock_knob(a, knob, d);
        qd_step_lock_fine(a, id, d);
        return 1;
    }
    {
        const qd_track_t *t = &a->model.t[a->sel];
        qd_player_live_edit(&a->player, a->sel, id,
                            t->patch[id] + (fine ? d : qd_ctl_acc(a, d, qd_step_id_steps(id)) * qd_step_id_detent(id)));
    }
    return 1;
}

/* the LEDs: B1..B5 the groups (this one lit, the others dim), B11 lit while the held steps have locks on this page */
void qd_snd_leds(const qd_app_t *a, uint8_t *black)
{
    const qd_ui_t *u = &a->ui;
    const qd_sndpage_t *pg = &QD_SND_PAGES[u->snd_page];
    int i, k, n = 0;
    for (i = 0; i < 11; i++)
        black[i] = i < QD_NSG ? (i == pg->group ? QD_LED_LIT : QD_LED_DIM) : QD_LED_OFF;
    for (i = 0; i < u->nheld; i++)
        for (k = 0; k < 5; k++)
            if (pg->id[k] >= 0) {
                int16_t v;
                n += qd_lock_get(qd_ctl_cpat(a), a->sel, u->held[i], pg->id[k], &v);
            }
    black[10] = n ? QD_LED_LIT : QD_LED_OFF;
}
