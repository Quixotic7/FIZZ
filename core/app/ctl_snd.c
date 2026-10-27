/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* The sound editor (P1.4a; PLAN.md §8b "P1.4a, the sound editor on EDIT"; ctl.h). Built through ctl_latch.c (it
 * includes this file: the editor is a latched menu, QD_LATCH_SND, and the core's source lists stay as they are).
 *
 * EDIT (printed) opens it for the selected track; EDIT again, OCT- or HOME close it (the latch rule). FM TONE's 73
 * values and DRIVE are on QD_SND_NPAGE pages in five groups, one a black key: B1 OPERATORS, B2 OP ENVELOPES, B3 FILTERS,
 * B4 ENVELOPES (the amp's, LEVEL, DRIVE), B5 MODULATION (the three LFOs); a group's key again shows its next page
 * (round). SELECT steps through every page. KNOB 1..4 are the page's values (their colours), ALGORITHM a fifth where
 * the page has one (OPERATORS' first page: the algorithm). A hi-res value (hundredths: LFO SPEED,
 * DEPTH) moves 0.01 a slow detent, accelerated to ~2.67 a detent spun fast (two turns cross -64..64); SHIFT + a knob:
 * 1.00 a detent, never accelerated (qd_step_id_turn). PRESETS: the track's preset (a step held: its sound lock).
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
    {"TRIG", QD_SG_TRIG, UI_SD_TRIG, {QD_QP_ATRIG, QD_QP_ARST, QD_QP_BTRIG, QD_QP_BRST, QD_QP_PHRT}, 0, 0, {0}},
    {"KEYS", QD_SG_TRIG, UI_SD_KEYS, {QD_QP_AKEY, QD_QP_B1KEY, QD_QP_B2KEY, QD_QP_VEL, -1}, 0, 0, {0}},
    /* E1 (research/FMTONE-DN2-PLAN.md §7): portamento, in the sound; B1 is PLAY / MOD */
    {"PORT", QD_SG_TRIG, UI_SD_PORT, {QD_QP_PORT, QD_QP_PTIM, QD_QP_PSLOPE, -1, -1}, 0, 0, {0}},
    /* 2026-10-26 (mockups/fizz-unison-voice.html): how the sound takes voices, ALGORITHM its PRIORITY */
    {"VOICE", QD_SG_TRIG, UI_SD_VOICE, {QD_QP_VMODE, QD_QP_UNI, QD_QP_UDET, QD_QP_USPR, QD_QP_VPRIO}, 0, 0, {0}},
    /* E4: the eight modulation slots: ALGORITHM the slot, KNOB 1 SOURCE, 2 DEST, 3 DEPTH (snd_mod_ids) */
    {"MOD", QD_SG_TRIG, UI_SD_MOD, {QD_QP_MOD(0, QD_QM_SRC), QD_QP_MOD(0, QD_QM_DST), QD_QP_MOD(0, QD_QM_DEPTH), -1, -1}, 0, 0,
     {0}},
    /* P1.5i (the owner, as ChoralRoot and the Digitone): RATIO B is one value, the pair (QD_QP_RB1 here stands for it:
     * B1 x 19 + B2, B2 the fast hand; snd_pair), and MIX moved here from TONE */
    {"RATIO", QD_SG_OPS, UI_SD_ALGO, {QD_QP_RC, QD_QP_RA, QD_QP_RB1, QD_QP_MIX, QD_QP_ALGO}, 0, 0, {0}},
    {"TONE", QD_SG_OPS, UI_SD_TONE, {QD_QP_HARM, QD_QP_DTUN, QD_QP_FDBK, QD_QP_MIX, -1}, 0, 0, {0}},   /* (P1.5n: MIX here
                                                         * too, the one value on two pages) */
    {"OFFSET", QD_SG_OPS, UI_SD_OFS, {QD_QP_OFSC, QD_QP_OFSA, QD_QP_OFSB1, QD_QP_OFSB2, -1}, 0, 0, {0}},
    {"ENV A", QD_SG_OPENV, UI_SD_OPENV, {QD_QP_AATK, QD_QP_ADEC, QD_QP_AEND, QD_QP_ALEV, QD_QP_ADLY}, 0, 0, {0}},
    {"ENV B", QD_SG_OPENV, UI_SD_OPENV, {QD_QP_BATK, QD_QP_BDEC, QD_QP_BEND, QD_QP_BLEV, QD_QP_BDLY}, 1, 0, {0}},
    /* E2: FILTER's ALGORITHM is the MACHINE (E4.1: turned in the Digitone II's order: MULTI, LOWPASS 4, LEGACY
     * LP/HP/OFF, COMB-, COMB+, EQ), KNOB 2 / 3 by it (snd_filter_ids); BASE's KNOB 4 BWPOST */
    {"FILTER", QD_SG_FILTER, UI_SD_FILTER, {QD_QP_FREQ, QD_QP_RESO, QD_QP_FTYPE, QD_QP_FDEPTH, QD_QP_FTYPE}, 0, 0, {0}},
    {"F ENV", QD_SG_FILTER, UI_SD_ADSR, {QD_QP_FATK, QD_QP_FDEC, QD_QP_FSUS, QD_QP_FREL, QD_QP_FDLY}, 0, 0, {0}},
    {"BASE", QD_SG_FILTER, UI_SD_BAND, {QD_QP_BASE, QD_QP_WIDTH, QD_QP_FKTRK, QD_QP_BWPOST, -1}, 0, 0, {0}},
    /* E1: AMP's MODE on ALGORITHM (ADSR / AHD; in AHD: ATK HOLD DEC); LEVEL's ROUTE (DRIVE before or after the
     * filter); B5 is AMP / FX */
    {"AMP", QD_SG_AMP, UI_SD_ADSR, {QD_QP_EATK, QD_QP_EDEC, QD_QP_ESUS, QD_QP_EREL, QD_QP_AMOD}, 1, QD_QP_AMOD,
     {QD_QP_EATK, QD_QP_HOLD, QD_QP_EDEC, -1, QD_QP_AMOD}},
    {"LEVEL", QD_SG_AMP, UI_SD_DRIVE, {QD_QP_LEVEL, QD_QP_DRIVE, QD_QP_DRPRE, -1, -1}, 0, 0, {0}},
    /* E3: B5's CRUSH: BITS, SRR, ROUTE (before / after the filter) */
    {"CRUSH", QD_SG_AMP, UI_SD_CRUSH, {QD_QP_BITS, QD_QP_SRR, QD_QP_CRPOST, -1, -1}, 0, 0, {0}},
    /* P1.5k (the owner): an LFO's page SPEED MULT DEPTH DEST (DEST in the menu now, not ALGORITHM's), its WAVE page
     * WAVE PHASE TRIG FADE; ALGORITHM nothing on them (ChoralRoot's LFO has these eight values, no fifth) */
    {"LFO 1", QD_SG_LFO1, UI_SD_LFO, {L(0, QD_QL_SPEED), L(0, QD_QL_MULT), L(0, QD_QL_DEPTH), L(0, QD_QL_DEST), -1}, 0, 0, {0}},
    {"LFO 1 WAVE", QD_SG_LFO1, UI_SD_LFOSHAPE, {L(0, QD_QL_WAVE), L(0, QD_QL_PHASE), L(0, QD_QL_TRIG), L(0, QD_QL_FADE),
                                                -1}, 0, 0, {0}},
    {"LFO 2", QD_SG_LFO2, UI_SD_LFO, {L(1, QD_QL_SPEED), L(1, QD_QL_MULT), L(1, QD_QL_DEPTH), L(1, QD_QL_DEST), -1}, 1, 0, {0}},
    {"LFO 2 WAVE", QD_SG_LFO2, UI_SD_LFOSHAPE, {L(1, QD_QL_WAVE), L(1, QD_QL_PHASE), L(1, QD_QL_TRIG), L(1, QD_QL_FADE),
                                                -1}, 1, 0, {0}},
    {"LFO 3", QD_SG_LFO3, UI_SD_LFO, {L(2, QD_QL_SPEED), L(2, QD_QL_MULT), L(2, QD_QL_DEPTH), L(2, QD_QL_DEST), -1}, 2, 0, {0}},
    {"LFO 3 WAVE", QD_SG_LFO3, UI_SD_LFOSHAPE, {L(2, QD_QL_WAVE), L(2, QD_QL_PHASE), L(2, QD_QL_TRIG), L(2, QD_QL_FADE),
                                                -1}, 2, 0, {0}},
};
#undef L
const char *const QD_SND_GROUP_NAME[QD_NSG] = {"PLAY / MOD", "OPERATORS", "OP ENVELOPES", "FILTERS", "AMP / FX",
                                               "LFO 1", "LFO 2", "LFO 3"};
int qd_snd_page_of(const char *name)
{
    int p;
    for (p = 0; p < QD_SND_NPAGE; p++)
        if (!strcmp(QD_SND_PAGES[p].name, name))
            return p;
    return 0;
}

/* the value id as the editor sees it now: the held step's (its lock, else what it starts from), else the track's */
static int snd_now(const qd_app_t *a, int id)
{
    if (a->ui.nheld) {
        int16_t x;
        return qd_lock_get(qd_ctl_cpat(a), a->sel, a->ui.held[0], id, &x)
                   ? x : qd_step_base((qd_app_t *)a, a->sel, a->ui.held[0], id);
    }
    return a->model.t[a->sel].patch[id];
}

/* ---------------------------------------------------------------- FIZZ MIDI (e): the MIDI track's editor */
/* research/FIZZ-MIDI-PLAN.md §5 (mockups/fizz-midi-track.html screens 4..8). A track whose sound is a MIDI sound
 * (quad_voice.h qd_msnd_is: ENGINE MIDI) has, in place of the sound editor, 7 pages of 4 slots (u->msnd_page): SELECT or
 * B1..B7 the page; KNOB 1..4 the slots' values 0..127, each change sent at once on the track's MIDI OUT as its CC (an
 * untouched slot sends nothing until turned: its first turn starts it from 0); SHIFT + a knob the slot's CC number
 * (sent nothing). A step held (or the latch's selection): a p-lock (the slot's value id, quad_voice.h QD_MSND_ID: its
 * range 0..127), the step sends it as it plays (app.c mo_mlk); B11 clears the page's, F2 + a knob one, REC +
 * AUTOMATION records them (sent at once too). The values' ids, so everything the sound editor does with ids works */
int qd_msnd_open(const qd_app_t *a) { return qd_snd_open(a) && qd_msnd_is(a->model.t[a->sel].patch); }
static int8_t MSND_IDS[5];
static const int8_t *msnd_ids(const qd_app_t *a)
{
    const int p = a->ui.msnd_page % QD_MSND_NPAGE;
    int k;
    for (k = 0; k < 4; k++)
        MSND_IDS[k] = (int8_t)QD_MSND_ID[p * QD_MSND_PAGE + k];
    MSND_IDS[4] = -1;
    return MSND_IDS;
}
/* a CC number or a touched flag: the track's sound, the edited pattern's, the trigs waiting (no voice to follow) */
static void msnd_raw(qd_app_t *a, int id, int v)
{
    const int t = a->sel;
    qd_tsound_t *ps = &qd_ctl_pat(a)->snd[t];
    if (a->model.t[t].patch[id] != v)
        a->model.t[t].patch[id] = (int16_t)v, a->model.t[t].edited = 1;
    if (ps->patch[id] != v)
        ps->patch[id] = (int16_t)v, ps->edited = 1;
    qd_seq_live_patch(&a->seq, t, id, v);
}
static int msnd_knob(qd_app_t *a, int knob, int d)
{
    qd_ui_t *u = &a->ui;
    const int t = a->sel;
    const int16_t *p = a->model.t[t].patch;
    int k, slot, id, v;
    if (knob == QD_K_SELECT) {
        u->msnd_page = (uint8_t)((u->msnd_page + QD_MSND_NPAGE + d % QD_MSND_NPAGE) % QD_MSND_NPAGE);
        u->snd_knob = -1, u->msnd_cck = -1;
        return 1;
    }
    if (knob == QD_K_PRESETS)
        return u->nheld ? qd_step_lock_knob(a, knob, d) : 0;
    if (knob < QD_K_1)
        return 1;                                       /* (ALGORITHM: nothing) */
    k = knob - QD_K_1;
    slot = u->msnd_page % QD_MSND_NPAGE * QD_MSND_PAGE + k;
    id = QD_MSND_ID[slot];
    u->snd_knob = k, u->snd_ms = a->ms;
    if (qd_ctl_shift(a)) {                              /* SHIFT: the slot's CC number (nothing sent) */
        const int c = p[QD_MSND_CC(slot)], n = (c & 127) + d;
        msnd_raw(a, QD_MSND_CC(slot), (c & QD_MSND_TOUCHED) | (n < 0 ? 0 : n > 127 ? 127 : n));
        u->msnd_cck = (int8_t)k;
        return 1;
    }
    u->msnd_cck = -1;
    if (u->nheld)                                       /* a finger on a step: a p-lock */
        return qd_step_lock_knob(a, knob, d);
    if (qd_auto_on(a)) {                                /* REC + AUTOMATION: a lock on the step playing, heard now */
        v = qd_auto_cur(a, id) + qd_ctl_acc(a, d, 128);
        v = v < 0 ? 0 : v > 127 ? 127 : v;
        if (qd_auto_put(a, id, v))
            qd_app_msnd_send(a, t, slot, v, 1);
        return 1;
    }
    v = p[id] + qd_ctl_acc(a, d, 128);
    v = v < 0 ? 0 : v > 127 ? 127 : v;
    if (v == p[id] && (p[QD_MSND_CC(slot)] & QD_MSND_TOUCHED))
        return 1;
    qd_player_live_edit(&a->player, t, id, v);
    if (!(p[QD_MSND_CC(slot)] & QD_MSND_TOUCHED))
        msnd_raw(a, QD_MSND_CC(slot), p[QD_MSND_CC(slot)] | QD_MSND_TOUCHED);
    qd_app_msnd_send(a, t, slot, v, 0);
    return 1;
}
/* the black keys: B1..B7 the page, B11 the held steps' locks on it (the sound editor's) */
static void msnd_black(qd_app_t *a, int b)
{
    if (b >= 0 && b < QD_MSND_NPAGE) {
        a->ui.msnd_page = (uint8_t)b;
        a->ui.snd_knob = -1, a->ui.msnd_cck = -1;
    } else if (b == 10) {
        qd_snd_black(a, 10);
    }
}
static void msnd_leds(const qd_app_t *a, uint8_t *black)
{
    const qd_ui_t *u = &a->ui;
    const int8_t *ids = msnd_ids(a);
    int i, k, n = 0;
    for (i = 0; i < 11; i++)
        black[i] = i < QD_MSND_NPAGE ? (i == u->msnd_page % QD_MSND_NPAGE ? QD_LED_LIT : QD_LED_DIM) : QD_LED_OFF;
    for (i = 0; i < u->nheld; i++)
        for (k = 0; k < 4; k++) {
            int16_t v;
            n += qd_lock_get(qd_ctl_cpat(a), a->sel, u->held[i], ids[k], &v);
        }
    black[10] = n ? QD_LED_LIT : QD_LED_OFF;
}
/* ENGINE (track settings page 3, KNOB 3): the track's sound becomes a new MIDI sound (every slot untouched, CC 16 + 4
 * (page - 1) + k), or FM TONE's INIT (the MIDI values go: the patch held them); the edited pattern's track sound (the
 * engine is the sound's: a preset, a pattern carry it). MIDI with MIDI OUT OFF: MIDI OUT the track's number */
void qd_msnd_engine(qd_app_t *a, int t, int midi)
{
    qd_tsound_t s;
    qd_tset_t *ts = &qd_ctl_pat(a)->trk[t];
    if (t < 0 || t >= QD_NTRACK || !midi == !qd_msnd_is(a->model.t[t].patch))
        return;
    qd_undo_hint(a, QD_UK_SOUNDS);
    memset(&s, 0, sizeof s);
    if (midi)
        qd_msnd_init(s.patch);
    else
        qd_quad_preset_patch(0, s.patch), s.preset = 1;
    strcpy(s.name, midi ? "MIDI" : "INIT");
    qd_player_flush_track(&a->player, t);
    qd_ctl_pat(a)->snd[t] = s;
    qd_track_from_tsound(&a->model.t[t], &s);
    qd_seq_preview(&a->seq, t, 0);
    if (midi && !ts->mout)
        ts->mout = (uint8_t)(t + 1);
}

/* E2: the filter machines: FTYPE 0..2 LEGACY (its TYPE on KNOB 3), 3 LOWPASS 4, 4 MULTI, 5 COMB+, 6 COMB-, 7 EQ; the
 * FILTER page's ids by machine, KNOB 2 / 3's names */
int qd_snd_machine(int ftype) { return ftype <= QD_QF_HP12 ? 0 : ftype - 2; }   /* 0 LEGACY .. 5 EQ */
const char *const QD_SND_MACHINE[6] = {"LEGACY", "LOWPASS 4", "MULTI", "COMB+", "COMB-", "EQ"};
static const int8_t SND_FILTER_IDS[6][5] = {
    {QD_QP_FREQ, QD_QP_RESO, QD_QP_FTYPE, QD_QP_FDEPTH, QD_QP_FTYPE},    /* LEGACY: TYPE (OFF, LP12, HP12) */
    {QD_QP_FREQ, QD_QP_RESO, -1, QD_QP_FDEPTH, QD_QP_FTYPE},             /* LOWPASS 4 */
    {QD_QP_FREQ, QD_QP_RESO, QD_QP_FSHAPE, QD_QP_FDEPTH, QD_QP_FTYPE},   /* MULTI: TYPE, the morph */
    {QD_QP_FREQ, QD_QP_RESO, QD_QP_FSHAPE, QD_QP_FDEPTH, QD_QP_FTYPE},   /* COMB+: FDBK, LPF */
    {QD_QP_FREQ, QD_QP_RESO, QD_QP_FSHAPE, QD_QP_FDEPTH, QD_QP_FTYPE},   /* COMB- */
    {QD_QP_FREQ, QD_QP_FGAIN, QD_QP_FSHAPE, QD_QP_FDEPTH, QD_QP_FTYPE}}; /* EQ: GAIN, Q */
static const char *const SND_FILTER_LAB[6][2] = {{"RESO", "TYPE"}, {"RESO", ""}, {"RESO", "TYPE"}, {"FDBK", "LPF"},
                                                 {"FDBK", "LPF"}, {"GAIN", "Q"}};
const char *qd_snd_label(const qd_app_t *a, int page, int k)
{
    const int8_t *ids = qd_snd_ids(a, page);
    if (qd_msnd_is(a->model.t[a->sel].patch)) {        /* (FIZZ MIDI (e): "CC 74") */
        static char lab[4][8];
        const int c = k >= 0 && k < 4 ? a->model.t[a->sel].patch[QD_MSND_CC(a->ui.msnd_page % QD_MSND_NPAGE *
                                                                          QD_MSND_PAGE + k)] & 127 : -1;
        if (c < 0)
            return "";
        {
            char *o = lab[k];
            *o++ = 'C', *o++ = 'C', *o++ = ' ';
            if (c >= 100)
                *o++ = (char)('0' + c / 100);
            if (c >= 10)
                *o++ = (char)('0' + c / 10 % 10);
            *o++ = (char)('0' + c % 10), *o = 0;
        }
        return lab[k];
    }
    if (k == 4 && QD_SND_PAGES[page].draw == UI_SD_MOD)
        return "SLOT";                                  /* (E4: ALGORITHM the slot, no value of its own) */
    if (k < 0 || k > 4 || ids[k] < 0)
        return "";
    if (QD_SND_PAGES[page].draw == UI_SD_FILTER && (k == 1 || k == 2))
        return SND_FILTER_LAB[qd_snd_machine(snd_now(a, QD_QP_FTYPE))][k - 1];
    if (QD_SND_PAGES[page].draw == UI_SD_FILTER && k == 4)
        return "MACHINE";
    if (QD_SND_PAGES[page].draw == UI_SD_MOD && k == 4)
        return "SLOT";
    return ids[k] == QD_QP_RB1 ? "RATIO B" : qd_quad_value_name(ids[k]);
}
static int8_t SND_MOD_IDS[QD_NMOD][5];
static const int8_t *snd_mod_ids(int slot)
{
    int8_t *r = SND_MOD_IDS[slot < 0 ? 0 : slot >= QD_NMOD ? QD_NMOD - 1 : slot];
    slot = slot < 0 ? 0 : slot >= QD_NMOD ? QD_NMOD - 1 : slot;
    r[0] = (int8_t)QD_QP_MOD(slot, QD_QM_SRC), r[1] = (int8_t)QD_QP_MOD(slot, QD_QM_DST);
    r[2] = (int8_t)QD_QP_MOD(slot, QD_QM_DEPTH), r[3] = r[4] = -1;
    return r;
}
int qd_snd_page_all_ids(int page, int8_t *out)   /* every id the page can show (its machines, its alt), each once */
{
    const qd_sndpage_t *pg = &QD_SND_PAGES[page];
    int n = 0, k, j, m;
    if (pg->draw == UI_SD_MOD) {                        /* (E4: every slot's) */
        for (m = 0; m < QD_NMOD; m++)
            for (k = 0; k < 3; k++)
                out[n++] = (int8_t)QD_QP_MOD(m, k);
        return n;
    }
    for (m = 0; m < (pg->draw == UI_SD_FILTER ? 6 : 1); m++)
        for (k = 0; k < 10; k++) {
            const int id = pg->draw == UI_SD_FILTER ? (k < 5 ? SND_FILTER_IDS[m][k] : -1)
                         : k < 5 ? pg->id[k] : pg->alt_if ? pg->alt[k - 5] : -1;
            int dup = id < 0;
            for (j = 0; j < n && !dup; j++)
                dup = out[j] == id;
            if (!dup)
                out[n++] = (int8_t)id;
        }
    return n;
}

const int8_t *qd_snd_ids(const qd_app_t *a, int page)
{
    const qd_sndpage_t *pg = &QD_SND_PAGES[page < 0 ? 0 : page >= QD_SND_NPAGE ? QD_SND_NPAGE - 1 : page];
    if (qd_msnd_is(a->model.t[a->sel].patch))          /* (FIZZ MIDI (e): a MIDI track: its page's four slots) */
        return msnd_ids(a);
    if (pg->draw == UI_SD_FILTER)                       /* (E2: by its machine) */
        return SND_FILTER_IDS[qd_snd_machine(snd_now(a, QD_QP_FTYPE))];
    if (pg->draw == UI_SD_MOD)                          /* (E4: the chosen slot's) */
        return snd_mod_ids(a->ui.mod_slot);
    if (!pg->alt_if)
        return pg->id;
    return snd_now(a, pg->alt_if) ? pg->alt : pg->id;
}

/* the editor's knob slots: KNOB 1..4, ALGORITHM */
static const uint8_t SND_KNOB[5] = {QD_K_1, QD_K_2, QD_K_3, QD_K_4, QD_K_ALGO};

/* P-LOCK's hook: a lock table a page (built once from the pages; the knobs' own takeover none: the page shows it) */
static qd_lockknob_t SND_LOCKS[6];                   /* (E2: built for the page as it is now) */
static qd_lockscreen_t SND_SCREEN;
const qd_lockscreen_t *qd_snd_lock_screen(const qd_app_t *a, int page)
{
    const int8_t *ids;
    int k, n = 0;
    page = page < 0 ? 0 : page >= QD_SND_NPAGE ? QD_SND_NPAGE - 1 : page;
    ids = qd_snd_ids(a, page);
    for (k = 0; k < 5; k++)
        if (ids[k] >= 0 && !(k == 4 && QD_SND_PAGES[page].draw == UI_SD_FILTER && ids[2] == ids[4])) {
            SND_LOCKS[n].knob = SND_KNOB[k];
            SND_LOCKS[n].id = ids[k];
            SND_LOCKS[n++].tk = QD_TK_NONE;
        }
    SND_LOCKS[n].knob = QD_K_PRESETS;                   /* PRESETS: the sound lock, as everywhere */
    SND_LOCKS[n].id = QD_LOCK_SOUND;
    SND_LOCKS[n++].tk = QD_TK_PRESET;
    SND_SCREEN.name = qd_msnd_is(a->model.t[a->sel].patch) ? "MIDI" : QD_SND_PAGES[page].name;
    SND_SCREEN.n = n;
    SND_SCREEN.k = SND_LOCKS;
    return &SND_SCREEN;
}

int qd_snd_open(const qd_app_t *a) { return a->ui.mode == QD_MODE_STEP && a->ui.latch == QD_LATCH_SND; }
int qd_snd_keys(const qd_app_t *a) { return (qd_snd_open(a) || qd_fx_open(a)) && a->ui.ekeys; }
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
            key[i] = QD_LED_ROOT;
        else if (n >= 0 && n < 128 && qd_scale_has(sc, rt, n))
            key[i] = QD_LED_SCALE;
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
static void snd_rnd_key(qd_app_t *a, int b, int down);
void qd_snd_black_up(qd_app_t *a, int b) { snd_rnd_key(a, b, 0); }
void qd_snd_black(qd_app_t *a, int b)
{
    qd_ui_t *u = &a->ui;
    int p, first = -1, next = -1;
    if (b != 10 && qd_msnd_is(a->model.t[a->sel].patch)) {   /* (FIZZ MIDI (e): B1..B7 the pages) */
        msnd_black(a, b);
        return;
    }
    if (b == 10) {
        const int8_t *ids = qd_snd_ids(a, u->snd_page);
        int i, k, n = 0;
        if (!u->nheld)
            return;
        for (i = 0; i < u->nheld; i++)
            for (k = 0; k < 5; k++)
                if (ids[k] >= 0) {
                    int16_t v;
                    n += qd_lock_get(qd_ctl_cpat(a), a->sel, u->held[i], ids[k], &v);
                    if (ids[k] == QD_QP_RB1)
                        n += qd_lock_get(qd_ctl_cpat(a), a->sel, u->held[i], QD_QP_RB2, &v);
                }
        if (!n)
            return;
        qd_clip_keep(&u->clip, qd_ctl_cpat(a), a->seq.cur, a->sel);
        for (i = 0; i < u->nheld; i++)
            for (k = 0; k < 5; k++)
                if (ids[k] >= 0) {
                    qd_lock_clear(qd_ctl_pat(a), a->sel, u->held[i], ids[k]);
                    if (ids[k] == QD_QP_RB1)         /* (the pair's other half) */
                        qd_lock_clear(qd_ctl_pat(a), a->sel, u->held[i], QD_QP_RB2);
                }
        qd_ctl_confirm(a, "LOCKS CLEARED", 0);
        return;
    }
    if (b == 8 || b == 9) {                             /* P1.5c: B9 the dice (the page), B10 held the bottle (all) */
        snd_rnd_key(a, b, 1);
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
    {
        const int cg = QD_SND_PAGES[u->snd_page].group, rp = u->snd_grp_pg[b & 15] - 1;
        if (cg >= 0 && cg < 16)
            u->snd_grp_pg[cg] = (uint8_t)(u->snd_page + 1);   /* (2026-10-26: the page this group was left on) */
        u->snd_page = cg == b ? (next >= 0 ? next : first)
                    : rp >= 0 && rp < QD_SND_NPAGE && QD_SND_PAGES[rp].group == b ? rp : first;
        u->snd_grp_pg[b & 15] = (uint8_t)(u->snd_page + 1);
    }
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
    if (qd_auto_on(a)) {                                /* P1.6d: automation recording */
        const int c = snd_pair_step(qd_auto_cur(a, QD_QP_RB1), qd_auto_cur(a, QD_QP_RB2), d, fine);
        qd_auto_put(a, QD_QP_RB1, c / SND_NRCB);
        qd_auto_put(a, QD_QP_RB2, c % SND_NRCB);
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
    if (qd_auto_on(a))                                  /* P1.6d: automation recording: the take's value */
        return qd_auto_cur(a, id);
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
    if (qd_auto_put(a, id, v))                          /* P1.6d: automation recording */
        return;
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
        if (qd_snd_ids(a, u->snd_page)[k] == id)
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
        snd_put(a, pid, dep + qd_step_id_turn(a, pid, d));
    }
}

/* E4: B1 held + a knob on value id: the slot with the chosen source -> id's destination (else the first empty one)
 * takes it, each detent its depth; no destination, or no slot free: refused (the shake) */
int qd_snd_mod_slot_of(const qd_app_t *a, int src, int dest)
{
    int k;
    for (k = 0; k < QD_NMOD; k++)
        if (snd_now(a, QD_QP_MOD(k, QD_QM_SRC)) == src && snd_now(a, QD_QP_MOD(k, QD_QM_DST)) == dest)
            return k;
    return -1;
}
static void snd_modmap(qd_app_t *a, int id, int d)
{
    qd_ui_t *u = &a->ui;
    const int dest = qd_quad_dest_of(id, 1), src = u->mod_src ? u->mod_src : QD_MS_VEL;
    int k, slot;
    u->snd_lk_k = -1;
    for (k = 0; k < 5; k++)
        if (qd_snd_ids(a, u->snd_page)[k] == id)
            u->snd_lk_k = k;
    u->snd_lk_id = id;
    slot = dest < 0 ? -1 : qd_snd_mod_slot_of(a, src, dest);
    for (k = 0; dest >= 0 && slot < 0 && k < QD_NMOD; k++)
        if (!snd_now(a, QD_QP_MOD(k, QD_QM_SRC)))
            slot = k;
    if (slot < 0) {
        u->snd_lk_bad_ms = a->ms ? a->ms : 1u;
        return;
    }
    u->snd_lk_bad_ms = 0;
    u->mod_slot = slot;
    if (snd_now(a, QD_QP_MOD(slot, QD_QM_SRC)) != src || snd_now(a, QD_QP_MOD(slot, QD_QM_DST)) != dest) {
        snd_put(a, QD_QP_MOD(slot, QD_QM_SRC), src);
        snd_put(a, QD_QP_MOD(slot, QD_QM_DST), dest);
        snd_put(a, QD_QP_MOD(slot, QD_QM_DEPTH), 0);
    }
    snd_put(a, QD_QP_MOD(slot, QD_QM_DEPTH), snd_cur(a, QD_QP_MOD(slot, QD_QM_DEPTH)) + qd_ctl_acc(a, d, 255));
}

/* E4: B1: a tap its group's next page (as before), held the slots' hold-to-map; W1..W8 while held: the source */
void qd_snd_mod_key(qd_app_t *a, int down)
{
    qd_ui_t *u = &a->ui;
    if (down) {
        u->snd_lk = 4;
        u->snd_lk_used = 0;
        u->snd_lk_ms = a->ms;
        u->snd_lk_k = -1;
        if (!u->mod_src)
            u->mod_src = QD_MS_VEL;
        return;
    }
    if (u->snd_lk != 4)
        return;
    u->snd_lk = 0;
    if (!u->snd_lk_used)
        qd_snd_black(a, 0);                             /* (a tap: B1's group, its next page) */
}
void qd_snd_mod_src(qd_app_t *a, int w)
{
    if (w < 0 || w >= QD_MS_N - 1)
        return;
    a->ui.mod_src = w + 1;
    a->ui.snd_lk_used = 1;
}

/* E4.1: the machines in the Digitone II's menu order (MULTI, LOWPASS 4, LEGACY, COMB-, COMB+, EQ; the stored FTYPE as
 * before); P1.6d: the FILTER key held, the popup of the six */
static const int16_t SND_MACH_FT[6] = {QD_QF_LP12, QD_QF_LP24, QD_QF_MULTI, QD_QF_COMBP, QD_QF_COMBN, QD_QF_EQ};
const int8_t QD_SND_MENU[6] = {2, 1, 0, 4, 3, 5};
int qd_snd_machine_pos(const qd_app_t *a)               /* the machine's place in the menu (0..5) */
{
    const int m = qd_snd_machine(snd_now(a, QD_QP_FTYPE));
    int pos = 0;
    while (pos < 5 && QD_SND_MENU[pos] != m)
        pos++;
    return pos;
}
static void snd_machine_at(qd_app_t *a, int pos)        /* (LEGACY: LP12, its default TYPE; through the dip) */
{
    pos = pos < 0 ? 0 : pos > 5 ? 5 : pos;
    if (pos != qd_snd_machine_pos(a))
        snd_put(a, QD_QP_FTYPE, SND_MACH_FT[(int)QD_SND_MENU[pos]]);
}
/* P1.6d: the FILTER group's key (B4): a tap its pages (at the release), held QD_FPOP_MS the machines' popup (while
 * held: ALGORITHM or any knob turns the choice, B1..B6 pick one; let go: closed, the choice kept) */
void qd_snd_fkey(qd_app_t *a, int down)
{
    qd_ui_t *u = &a->ui;
    if (down) {
        u->snd_fkey = 1, u->snd_fkey_ms = a->ms, u->snd_fkey_used = 0, u->snd_fpop = 0;
        return;
    }
    if (!u->snd_fkey)
        return;
    u->snd_fkey = 0;
    if (u->snd_fpop) {
        u->snd_fpop = 0;
        return;
    }
    if (!u->snd_fkey_used)
        qd_snd_black(a, QD_SG_FILTER);                  /* (a tap: its pages) */
}
void qd_snd_fkey_ms(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    if (u->snd_fkey && !u->snd_fpop && !u->snd_fkey_used && a->ms - u->snd_fkey_ms >= QD_FPOP_MS && qd_snd_open(a)) {
        u->snd_fpop = 1;
        u->snd_page = qd_snd_page_of("FILTER");
        u->snd_knob = -1;
    }
    if (u->snd_fkey && !qd_snd_open(a))
        u->snd_fkey = u->snd_fpop = 0;
}
int qd_snd_fpop_key(qd_app_t *a, int b)                 /* B1..B6 with the popup up: that machine; 1 taken */
{
    if (!a->ui.snd_fpop)
        return 0;
    if (b >= 0 && b < 6)
        snd_machine_at(a, b);
    return 1;
}

/* a knob; 1: taken */
int qd_snd_knob(qd_app_t *a, int knob, int d)
{
    qd_ui_t *u = &a->ui;
    const int8_t *ids = qd_snd_ids(a, u->snd_page);
    const int fine = qd_ctl_shift(a);
    int k, id = -1;
    if (qd_msnd_is(a->model.t[a->sel].patch))          /* FIZZ MIDI (e): a MIDI track's editor */
        return msnd_knob(a, knob, d);
    if (u->snd_fkey && !u->snd_fpop) {                  /* (the FILTER key down, a knob: the popup at once) */
        u->snd_fpop = 1;
        u->snd_page = qd_snd_page_of("FILTER");
        u->snd_knob = -1;
    }
    if (u->snd_fpop) {                                  /* P1.6d: the machines' popup: any knob the choice */
        if (d)
            snd_machine_at(a, qd_snd_machine_pos(a) + (d > 0 ? 1 : -1));
        return 1;
    }
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
    if (QD_SND_PAGES[u->snd_page].draw == UI_SD_MOD && knob == QD_K_ALGO && !u->snd_lk) {   /* E4: the slot */
        u->mod_slot = u->mod_slot + (d > 0 ? 1 : -1) < 0 ? 0 : u->mod_slot + (d > 0 ? 1 : -1) > QD_NMOD - 1
                    ? QD_NMOD - 1 : u->mod_slot + (d > 0 ? 1 : -1);
        u->snd_knob = 4;
        u->snd_ms = a->ms;
        return 1;
    }
    for (k = 0; k < 5; k++)
        if (SND_KNOB[k] == knob)
            id = ids[k], u->snd_knob = k;
    u->snd_ms = a->ms;
    if (u->snd_lk) {                                    /* P1.5k: an LFO key held: the knob's value its destination */
        if (id >= 0 && u->snd_lk == 4)
            snd_modmap(a, id, d);                       /* (E4: B1 held: a modulation slot) */
        else if (id >= 0)
            snd_map(a, u->snd_lk - 1, id, d);
        u->snd_lk_used = 1;
        return 1;
    }
    if (id < 0)
        return 1;
    if (!qd_quad_lockable(id)) {                        /* 2026-10-26, the VOICE page: the track's sound only (a step
                                                         * held: nothing; no automation take) */
        int16_t lo, hi;
        int v;
        if (u->nheld || !qd_quad_value_range(id, &lo, &hi, 0))
            return 1;
        v = a->model.t[a->sel].patch[id] + qd_step_id_turn(a, id, d);
        qd_player_live_edit(&a->player, a->sel, id, v < lo ? lo : v > hi ? hi : v);
        return 1;
    }
    if ((id >= QD_QP_LFO0 && id < QD_QP_LFO0 + 3 * 8 && (id - QD_QP_LFO0) % 8 == QD_QL_DEST) ||
        (id >= QD_QP_MOD0 && id < QD_QP_VMODE && (id - QD_QP_MOD0) % 3 == QD_QM_DST)) {   /* (E4: a slot's DEST too) */
        snd_dest(a, id, d);                             /* P1.5k: DEST, through ChoralRoot's list in its order */
        return 1;
    }
    if (id == QD_QP_RB1) {                              /* P1.5i: RATIO B, the pair */
        snd_pair(a, fine ? d : qd_ctl_acc(a, d, SND_NRCB * SND_NRCB), fine);   /* (P1.6g: accelerated as the others:
                                                         * the pair is one value of 361 steps, B1 every 19th) */
        return 1;
    }
    if (id == QD_QP_FTYPE) {                            /* E2: ALGORITHM the machine, LEGACY's KNOB 3 its TYPE */
        const int cur = snd_cur(a, QD_QP_FTYPE);
        if (knob == QD_K_ALGO) {
            const int pos = qd_snd_machine_pos(a);
            snd_machine_at(a, pos + (d > 0 ? 1 : -1));
            u->snd_fpop_ms = a->ms ? a->ms : 1u;        /* (P1.6g: the popup, a second after the last detent) */
        } else {
            snd_put(a, QD_QP_FTYPE, cur + d < 0 ? 0 : cur + d > QD_QF_HP12 ? QD_QF_HP12 : cur + d);
        }
        return 1;
    }
    if (u->nheld) {                                     /* a finger on a step: a p-lock */
        if (!fine)
            return qd_step_lock_knob(a, knob, d);
        qd_step_lock_fine(a, id, d * qd_step_id_detent(id));   /* (SHIFT: a hi-res value 1.00) */
        return 1;
    }
    if (qd_auto_on(a)) {                                /* P1.6d: automation recording */
        qd_auto_put(a, id, qd_auto_cur(a, id) + qd_step_id_turn(a, id, d));
        return 1;
    }
    {
        const qd_track_t *t = &a->model.t[a->sel];
        qd_player_live_edit(&a->player, a->sel, id,
                            t->patch[id] + qd_step_id_turn(a, id, d));
    }
    return 1;
}

/* the LEDs: B1..B5 the groups (this one lit, the others dim), B11 lit while the held steps have locks on this page */
void qd_snd_leds(const qd_app_t *a, uint8_t *black)
{
    const qd_ui_t *u = &a->ui;
    const qd_sndpage_t *pg = &QD_SND_PAGES[u->snd_page];
    const int8_t *ids = qd_snd_ids(a, u->snd_page);
    int i, k, n = 0;
    if (qd_msnd_is(a->model.t[a->sel].patch)) {        /* (FIZZ MIDI (e): B1..B7 the pages) */
        msnd_leds(a, black);
        return;
    }
    for (i = 0; i < 11; i++)                            /* (P1.5l: B1..B8 the groups, B6..B8 the LFOs: also lit held) */
        black[i] = i < QD_NSG ? (i == pg->group || (i >= QD_SG_LFO1 && u->snd_lk == i - QD_SG_LFO1 + 1) ? QD_LED_LIT
                                                                                                        : QD_LED_DIM)
                              : QD_LED_OFF;
    black[8] = u->rnd_kind == 1 || u->rnd_kind == 4 ? QD_LED_LIT : QD_LED_DIM;   /* P1.5c: B9 the dice, B10 the bottle */
    black[9] = u->rnd_kind == 2 || u->rnd_kind == 3 ? QD_LED_LIT : QD_LED_DIM;
    for (i = 0; i < u->nheld; i++)
        for (k = 0; k < 5; k++)
            if (ids[k] >= 0) {
                int16_t v;
                n += qd_lock_get(qd_ctl_cpat(a), a->sel, u->held[i], ids[k], &v);
                if (ids[k] == QD_QP_RB1)
                    n += qd_lock_get(qd_ctl_cpat(a), a->sel, u->held[i], QD_QP_RB2, &v);
            }
    black[10] = n ? QD_LED_LIT : QD_LED_OFF;
    if (u->snd_fpop) {                                  /* P1.6d: the machines' popup: B1..B6 the six, the current lit */
        const int pos = qd_snd_machine_pos(a);
        for (i = 0; i < 11; i++)
            black[i] = i < 6 ? (i == pos ? QD_LED_LIT : QD_LED_DIM) : QD_LED_OFF;
        black[QD_SG_FILTER] = pos == QD_SG_FILTER ? QD_LED_LIT : QD_LED_MID;   /* (held) */
    }
}

/* ---------------------------------------------------------------- P1.5c: randomise (B9 the page, B10 held the sound) */
/* THE RULES: every value inside its range (snd_put clamps), and inside a musical part of it so the sound stays
 * playable: ratios from the plain ones (C 0.50 1 2 3 4; A 0.50 1 2 3 4; B's pair from 0.50 1 2 3), the operator
 * envelopes' levels never low (A LEV, B LEV 40 .. 100 %: no carrier silent), their attacks short, decays and ends
 * audible; the filter open enough (FREQ from 45 %), RESO at most 60 %, its type kept; the amp's attack short,
 * sustain from 40 %, release modest; LEVEL kept (the loudness is the mix's); DRIVE at most 25 %; the LFOs gentle (depth
 * at most a third, their destinations kept: no new modulation appears from nowhere); the switches (TRIG, RESET,
 * PHRT) kept. A fraction pair (lo, hi) of the range a value, or kept */
typedef struct { int8_t id; uint8_t lo, hi; } snd_rule_t;   /* lo, hi: % of the range; 255: kept */
static const snd_rule_t SND_RULES[] = {
    {QD_QP_HARM, 25, 75}, {QD_QP_DTUN, 40, 60}, {QD_QP_FDBK, 0, 60}, {QD_QP_MIX, 0, 100},
    {QD_QP_OFSC, 45, 55}, {QD_QP_OFSA, 45, 55}, {QD_QP_OFSB1, 45, 55}, {QD_QP_OFSB2, 45, 55},
    {QD_QP_AATK, 0, 30}, {QD_QP_ADEC, 15, 80}, {QD_QP_AEND, 20, 100}, {QD_QP_ALEV, 40, 100},
    {QD_QP_BATK, 0, 30}, {QD_QP_BDEC, 15, 80}, {QD_QP_BEND, 20, 100}, {QD_QP_BLEV, 40, 100},
    {QD_QP_ADLY, 0, 10}, {QD_QP_BDLY, 0, 10}, {QD_QP_VEL, 20, 80},
    {QD_QP_AKEY, 0, 30}, {QD_QP_B1KEY, 0, 30}, {QD_QP_B2KEY, 0, 30},
    {QD_QP_FATK, 0, 30}, {QD_QP_FDEC, 15, 70}, {QD_QP_FSUS, 20, 100}, {QD_QP_FREL, 10, 60},
    {QD_QP_FREQ, 45, 100}, {QD_QP_RESO, 0, 60}, {QD_QP_FDEPTH, 40, 75}, {QD_QP_FDLY, 0, 5},
    {QD_QP_FKTRK, 0, 60}, {QD_QP_BASE, 0, 20}, {QD_QP_WIDTH, 70, 100},
    {QD_QP_EATK, 0, 20}, {QD_QP_EDEC, 20, 80}, {QD_QP_ESUS, 40, 100}, {QD_QP_EREL, 10, 55},
    {QD_QP_DRIVE, 0, 25},
};
static uint32_t snd_rnd(qd_app_t *a)                    /* xorshift32 (seeded by the clock once) */
{
    uint32_t x = a->ui.rnd_seed ? a->ui.rnd_seed : (a->ms * 2654435761u) | 1u;
    x ^= x << 13, x ^= x >> 17, x ^= x << 5;
    a->ui.rnd_seed = x;
    return x;
}
static int snd_pick(qd_app_t *a, const uint8_t *from, int n) { return from[snd_rnd(a) % (uint32_t)n]; }
static void snd_rnd_value(qd_app_t *a, int id)
{
    static const uint8_t RC[5] = {1, 3, 4, 5, 6}, RA[5] = {1, 3, 7, 11, 15}, RB[4] = {1, 3, 4, 5};
    int16_t lo, hi;
    size_t k;
    if (id == QD_QP_ALGO) {
        snd_put(a, id, 1 + (int)(snd_rnd(a) % 8u));
        return;
    }
    if (id == QD_QP_RC || id == QD_QP_RA) {
        snd_put(a, id, id == QD_QP_RC ? snd_pick(a, RC, 5) : snd_pick(a, RA, 5));
        return;
    }
    if (id == QD_QP_RB1 || id == QD_QP_RB2) {          /* (the pair: both) */
        snd_put(a, QD_QP_RB1, snd_pick(a, RB, 4));
        snd_put(a, QD_QP_RB2, snd_pick(a, RB, 4));
        return;
    }
    if (id >= QD_QP_LFO0 && id < QD_QP_LFO0 + 3 * 8) {  /* the LFOs: gentle, their destinations kept */
        const int f = (id - QD_QP_LFO0) % 8;
        if (f == QD_QL_DEST || f == QD_QL_TRIG)
            return;
        if (!qd_quad_value_range(id, &lo, &hi, 0))
            return;
        if (f == QD_QL_DEPTH) {
            const int r = (hi - lo) / 6;
            snd_put(a, id, (int)(snd_rnd(a) % (uint32_t)(2 * r + 1)) - r);
        } else if (f == QD_QL_FADE) {
            snd_put(a, id, 0);
        } else {
            snd_put(a, id, lo + (int)(snd_rnd(a) % (uint32_t)(hi - lo + 1)));
        }
        return;
    }
    for (k = 0; k < sizeof SND_RULES / sizeof SND_RULES[0]; k++)
        if (SND_RULES[k].id == id) {
            if (!qd_quad_value_range(id, &lo, &hi, 0) || hi <= lo)
                return;
            {
                const int a0 = lo + (hi - lo) * SND_RULES[k].lo / 100, a1 = lo + (hi - lo) * SND_RULES[k].hi / 100;
                snd_put(a, id, a0 + (int)(snd_rnd(a) % (uint32_t)(a1 - a0 + 1)));
            }
            return;
        }
    /* (not in the rules: kept: LEVEL, FTYPE, the TRIG / RESET / PHRT switches) */
}

/* page: that page's values (the pair both halves); -1: every value of the sound. A step held: its p-locks */
void qd_snd_randomise(qd_app_t *a, int page)
{
    int k;
    qd_undo_hint(a, QD_UK_RANDOM);
    if (page >= 0) {
        const int8_t *ids = qd_snd_ids(a, page);
        for (k = 0; k < 5; k++)
            if (ids[k] >= 0)
                snd_rnd_value(a, ids[k]);
        return;
    }
    for (k = 0; k < QD_QUAD_NP; k++)
        if (k != QD_QP_RB2)
            snd_rnd_value(a, k);
}

/* B9 held (P1.6d: a second): the dice shake harder and harder, then roll: the page new as they roll (released early:
 * nothing); B10 down: the bottle fills while held, the sound new at the pop (released early: nothing) */
static void snd_rnd_key(qd_app_t *a, int b, int down)
{
    qd_ui_t *u = &a->ui;
    if (b == 8 && down && !u->rnd_kind) {
        u->rnd_kind = 1;
        u->rnd_ms = a->ms;
    } else if (b == 8 && !down && u->rnd_kind == 1) {
        u->rnd_kind = 0;                                /* (P1.6d: let go before the second: nothing) */
    } else if (b == 9 && down && !u->rnd_kind) {
        u->rnd_kind = 2;
        u->rnd_ms = a->ms;
    } else if (b == 9 && !down && u->rnd_kind == 2) {
        u->rnd_kind = 0;                                /* (released early: nothing) */
    }
}
void qd_snd_rnd_ms(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    if (!u->rnd_kind)
        return;
    if (!qd_snd_open(a)) {
        u->rnd_kind = 0;
        return;
    }
    if (u->rnd_kind == 1 && a->ms - u->rnd_ms >= QD_RND_PAGE_HOLD_MS) {   /* P1.6d: held a second: they roll */
        qd_snd_randomise(a, u->snd_page);
        u->rnd_kind = 4;
        u->rnd_ms = a->ms;
    } else if (u->rnd_kind == 4 && a->ms - u->rnd_ms >= QD_RND_DICE_MS) {
        u->rnd_kind = 0;
    } else if (u->rnd_kind == 2 && a->ms - u->rnd_ms >= QD_RND_HOLD_MS) {
        qd_snd_randomise(a, -1);
        u->rnd_kind = 3;
        u->rnd_ms = a->ms;
    } else if (u->rnd_kind == 3 && a->ms - u->rnd_ms >= QD_RND_BURST_MS) {
        u->rnd_kind = 0;
    }
}
