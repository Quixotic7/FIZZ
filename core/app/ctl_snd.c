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
    /* P1.5l (the owner): the groups on B1..B8, in SELECT's order: B1 TRIG / KEYS (out of OP ENVELOPES), B2 OPERATORS,
     * B3 OP ENVELOPES, B4 FILTERS, B5 ENVELOPES, B6 / B7 / B8 the LFOs (the MODULATION key gone) */
    {"TRIG", QD_SG_TRIG, UI_SD_TRIG, {QD_QP_ATRIG, QD_QP_ARST, QD_QP_BTRIG, QD_QP_BRST, QD_QP_PHRT}, 0},
    {"KEYS", QD_SG_TRIG, UI_SD_KEYS, {QD_QP_AKEY, QD_QP_B1KEY, QD_QP_B2KEY, QD_QP_VEL, -1}, 0},
    /* P1.5i (the owner, as ChoralRoot and the Digitone): RATIO B is one value, the pair (QD_QP_RB1 here stands for it:
     * B1 x 19 + B2, B2 the fast hand; snd_pair), and MIX moved here from TONE */
    {"RATIO", QD_SG_OPS, UI_SD_ALGO, {QD_QP_RC, QD_QP_RA, QD_QP_RB1, QD_QP_MIX, QD_QP_ALGO}, 0},
    {"TONE", QD_SG_OPS, UI_SD_TONE, {QD_QP_HARM, QD_QP_DTUN, QD_QP_FDBK, QD_QP_MIX, -1}, 0},   /* (P1.5n: MIX here
                                                         * too, the one value on two pages) */
    {"OFFSET", QD_SG_OPS, UI_SD_OFS, {QD_QP_OFSC, QD_QP_OFSA, QD_QP_OFSB1, QD_QP_OFSB2, -1}, 0},
    {"ENV A", QD_SG_OPENV, UI_SD_OPENV, {QD_QP_AATK, QD_QP_ADEC, QD_QP_AEND, QD_QP_ALEV, QD_QP_ADLY}, 0},
    {"ENV B", QD_SG_OPENV, UI_SD_OPENV, {QD_QP_BATK, QD_QP_BDEC, QD_QP_BEND, QD_QP_BLEV, QD_QP_BDLY}, 1},
    {"FILTER", QD_SG_FILTER, UI_SD_FILTER, {QD_QP_FREQ, QD_QP_RESO, QD_QP_FTYPE, QD_QP_FDEPTH, -1}, 0},
    {"F ENV", QD_SG_FILTER, UI_SD_ADSR, {QD_QP_FATK, QD_QP_FDEC, QD_QP_FSUS, QD_QP_FREL, QD_QP_FDLY}, 0},
    {"BASE", QD_SG_FILTER, UI_SD_BAND, {QD_QP_BASE, QD_QP_WIDTH, QD_QP_FKTRK, -1, -1}, 0},
    {"AMP", QD_SG_AMP, UI_SD_ADSR, {QD_QP_EATK, QD_QP_EDEC, QD_QP_ESUS, QD_QP_EREL, -1}, 1},
    {"LEVEL", QD_SG_AMP, UI_SD_DRIVE, {QD_QP_LEVEL, QD_QP_DRIVE, -1, -1, -1}, 0},
    /* P1.5k (the owner): an LFO's page SPEED MULT DEPTH DEST (DEST in the menu now, not ALGORITHM's), its WAVE page
     * WAVE PHASE TRIG FADE; ALGORITHM nothing on them (ChoralRoot's LFO has these eight values, no fifth) */
    {"LFO 1", QD_SG_LFO1, UI_SD_LFO, {L(0, QD_QL_SPEED), L(0, QD_QL_MULT), L(0, QD_QL_DEPTH), L(0, QD_QL_DEST), -1}, 0},
    {"LFO 1 WAVE", QD_SG_LFO1, UI_SD_LFOSHAPE, {L(0, QD_QL_WAVE), L(0, QD_QL_PHASE), L(0, QD_QL_TRIG), L(0, QD_QL_FADE),
                                                -1}, 0},
    {"LFO 2", QD_SG_LFO2, UI_SD_LFO, {L(1, QD_QL_SPEED), L(1, QD_QL_MULT), L(1, QD_QL_DEPTH), L(1, QD_QL_DEST), -1}, 1},
    {"LFO 2 WAVE", QD_SG_LFO2, UI_SD_LFOSHAPE, {L(1, QD_QL_WAVE), L(1, QD_QL_PHASE), L(1, QD_QL_TRIG), L(1, QD_QL_FADE),
                                                -1}, 1},
    {"LFO 3", QD_SG_LFO3, UI_SD_LFO, {L(2, QD_QL_SPEED), L(2, QD_QL_MULT), L(2, QD_QL_DEPTH), L(2, QD_QL_DEST), -1}, 2},
    {"LFO 3 WAVE", QD_SG_LFO3, UI_SD_LFOSHAPE, {L(2, QD_QL_WAVE), L(2, QD_QL_PHASE), L(2, QD_QL_TRIG), L(2, QD_QL_FADE),
                                                -1}, 2},
};
#undef L
const char *const QD_SND_GROUP_NAME[QD_NSG] = {"TRIG / KEYS", "OPERATORS", "OP ENVELOPES", "FILTERS", "ENVELOPES",
                                               "LFO 1", "LFO 2", "LFO 3"};
int qd_snd_page_of(const char *name)
{
    int p;
    for (p = 0; p < QD_SND_NPAGE; p++)
        if (!strcmp(QD_SND_PAGES[p].name, name))
            return p;
    return 0;
}

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

/* P1.5k: LFO n's page (n 0..2): its LFO page, again its WAVE page, again back */
static int snd_lfo_page(int n)
{
    int p;
    for (p = 0; p < QD_SND_NPAGE; p++)
        if (QD_SND_PAGES[p].draw == UI_SD_LFO && QD_SND_PAGES[p].arg == n)
            return p;
    return 0;
}
void qd_snd_lfo_key(qd_app_t *a, int n, int down)
{
    qd_ui_t *u = &a->ui;
    if (down) {
        u->snd_lk = n + 1;
        u->snd_lk_used = 0;
        u->snd_lk_ms = a->ms;
        u->snd_lk_k = -1;
        return;
    }
    if (u->snd_lk != n + 1)
        return;
    u->snd_lk = 0;
    if (!u->snd_lk_used) {                              /* a tap: the LFO's page, again its WAVE page, again back */
        const int p0 = snd_lfo_page(n);
        u->snd_page = u->snd_page == p0 ? p0 + 1 : p0;
        u->snd_knob = -1;
    }
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
                    if (pg->id[k] == QD_QP_RB1)
                        n += qd_lock_get(qd_ctl_cpat(a), a->sel, u->held[i], QD_QP_RB2, &v);
                }
        if (!n)
            return;
        qd_clip_keep(&u->clip, qd_ctl_cpat(a), a->seq.cur, a->sel);
        for (i = 0; i < u->nheld; i++)
            for (k = 0; k < 5; k++)
                if (pg->id[k] >= 0) {
                    qd_lock_clear(qd_ctl_pat(a), a->sel, u->held[i], pg->id[k]);
                    if (pg->id[k] == QD_QP_RB1)         /* (the pair's other half) */
                        qd_lock_clear(qd_ctl_pat(a), a->sel, u->held[i], QD_QP_RB2);
                }
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

/* P1.5i: RATIO B, ChoralRoot's pair (eng_quad.c quad_get / quad_set, cr_edit.c's knob): the index B1 x 19 + B2
 * (0..360, 1.00 / 1.00 = 60), a detent one step of it (B2 the fast hand, carrying into B1, as the Digitone's), SHIFT
 * B1 a step (the slow hand); both written (live, or locked on the held steps: QD_QP_RB1 and QD_QP_RB2) */
#define SND_NRCB 19
static int snd_pair_step(int b1, int b2, int d, int fine)
{
    int c = b1 * SND_NRCB + b2;
    if (fine) {
        b1 += d;
        b1 = b1 < 0 ? 0 : b1 > SND_NRCB - 1 ? SND_NRCB - 1 : b1;
        return b1 * SND_NRCB + b2;
    }
    c += d;
    return c < 0 ? 0 : c > SND_NRCB * SND_NRCB - 1 ? SND_NRCB * SND_NRCB - 1 : c;
}
static void snd_pair(qd_app_t *a, int d, int fine)
{
    qd_ui_t *u = &a->ui;
    if (u->nheld) {
        int i;
        for (i = 0; i < u->nheld; i++) {
            const int s = u->held[i];
            int16_t v1, v2;
            const int b1 = qd_lock_get(qd_ctl_cpat(a), a->sel, s, QD_QP_RB1, &v1) ? v1 : qd_step_base(a, a->sel, s, QD_QP_RB1);
            const int b2 = qd_lock_get(qd_ctl_cpat(a), a->sel, s, QD_QP_RB2, &v2) ? v2 : qd_step_base(a, a->sel, s, QD_QP_RB2);
            const int c = snd_pair_step(b1, b2, d, fine);
            if (qd_lock_set(qd_ctl_pat(a), a->sel, s, QD_QP_RB1, c / SND_NRCB) == QD_EFULL ||
                qd_lock_set(qd_ctl_pat(a), a->sel, s, QD_QP_RB2, c % SND_NRCB) == QD_EFULL) {
                qd_ctl_confirm(a, "NO ROOM", "for locks: the pattern has 1024");
                return;
            }
        }
        return;
    }
    {
        const int16_t *p = a->model.t[a->sel].patch;
        const int c = snd_pair_step(p[QD_QP_RB1], p[QD_QP_RB2], d, fine);
        qd_player_live_edit(&a->player, a->sel, QD_QP_RB1, c / SND_NRCB);
        qd_player_live_edit(&a->player, a->sel, QD_QP_RB2, c % SND_NRCB);
    }
}

/* P1.5k: value id set on the held steps (locks) or the track's sound (live), absolutely */
static int snd_cur(qd_app_t *a, int id)                 /* (the held step's value, else the track's) */
{
    qd_ui_t *u = &a->ui;
    if (u->nheld) {
        int16_t v;
        return qd_lock_get(qd_ctl_cpat(a), a->sel, u->held[0], id, &v) ? v : qd_step_base(a, a->sel, u->held[0], id);
    }
    return a->model.t[a->sel].patch[id];
}
static void snd_put(qd_app_t *a, int id, int v)
{
    qd_ui_t *u = &a->ui;
    int16_t lo, hi;
    if (qd_quad_value_range(id, &lo, &hi, 0))
        v = v < lo ? lo : v > hi ? hi : v;
    if (u->nheld) {
        int i;
        for (i = 0; i < u->nheld; i++)
            if (qd_lock_set(qd_ctl_pat(a), a->sel, u->held[i], id, v) == QD_EFULL) {
                qd_ctl_confirm(a, "NO ROOM", "for locks: the pattern has 1024");
                return;
            }
        return;
    }
    qd_player_live_edit(&a->player, a->sel, id, v);
}
static void snd_dest(qd_app_t *a, int id, int d)
{
    snd_put(a, id, qd_quad_dest_step(snd_cur(a, id), d));
}
/* P1.5k: LFO n held + a knob on value id: its first detent makes id LFO n's destination (if ChoralRoot's list has it;
 * the depth from 0), each detent the depth (signed, accelerated); not a destination: nothing, the takeover says so */
static void snd_map(qd_app_t *a, int n, int id, int d)
{
    qd_ui_t *u = &a->ui;
    const int dest = qd_quad_dest_of(id, n), did = QD_QP_LFO(n, QD_QL_DEST), pid = QD_QP_LFO(n, QD_QL_DEPTH);
    int k;
    u->snd_lk_k = -1;
    for (k = 0; k < 5; k++)
        if (QD_SND_PAGES[u->snd_page].id[k] == id)
            u->snd_lk_k = k;
    u->snd_lk_id = id;
    if (dest < 0) {
        u->snd_lk_bad_ms = a->ms ? a->ms : 1u;
        return;
    }
    u->snd_lk_bad_ms = 0;
    {
        const int same = snd_cur(a, did) == dest;
        const int dep = same ? snd_cur(a, pid) : 0;
        if (!same)
            snd_put(a, did, dest);
        snd_put(a, pid, dep + qd_ctl_acc(a, d, qd_step_id_steps(pid)) * qd_step_id_detent(pid));
    }
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
    if (u->snd_lk) {                                    /* P1.5k: an LFO key held: the knob's value its destination */
        if (id >= 0)
            snd_map(a, u->snd_lk - 1, id, d);
        u->snd_lk_used = 1;
        return 1;
    }
    if (id < 0)
        return 1;
    if (id >= QD_QP_LFO0 && id < QD_QP_LFO0 + 3 * 8 && (id - QD_QP_LFO0) % 8 == QD_QL_DEST) {
        snd_dest(a, id, d);                             /* P1.5k: DEST, through ChoralRoot's list in its order */
        return 1;
    }
    if (id == QD_QP_RB1) {                              /* P1.5i: RATIO B, the pair */
        snd_pair(a, d, fine);
        return 1;
    }
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
    for (i = 0; i < 11; i++)                            /* (P1.5l: B1..B8 the groups, B6..B8 the LFOs: also lit held) */
        black[i] = i < QD_NSG ? (i == pg->group || (i >= QD_SG_LFO1 && u->snd_lk == i - QD_SG_LFO1 + 1) ? QD_LED_LIT
                                                                                                        : QD_LED_DIM)
                              : QD_LED_OFF;
    for (i = 0; i < u->nheld; i++)
        for (k = 0; k < 5; k++)
            if (pg->id[k] >= 0) {
                int16_t v;
                n += qd_lock_get(qd_ctl_cpat(a), a->sel, u->held[i], pg->id[k], &v);
                if (pg->id[k] == QD_QP_RB1)
                    n += qd_lock_get(qd_ctl_cpat(a), a->sel, u->held[i], QD_QP_RB2, &v);
            }
    black[10] = n ? QD_LED_LIT : QD_LED_OFF;
}
