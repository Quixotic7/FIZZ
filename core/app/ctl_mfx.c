/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* P1.6c: the MIDI FX screen (ctl.h; PLAN.md "MIDI FX groups", mockups/fizz-midifx.html). Built through ctl_latch.c
 * (the core's source lists stay as they are).
 *
 *   SHIFT + ARP        the screen, latched: always the selected track's group (SHIFT + SELECT: the next track's)
 *   B1..B8             slot 1..8; the selected one again: its next page
 *   PRESETS            the slot's effect: NONE, then the ten (heard at once; a rolodex while turning)
 *   KNOB 1..4, SELECT  the page's values (KNOB 4 CHANCE on page 1); SELECT the page too
 *   ALGORITHM          the track's group: OFF, 1..5 (the screen follows; the five groups as small rails)
 *   B10                the slot bypassed / playing; B11 its effect removed (twice: the first press arms it)
 *   F1 + B n, F2 + B n the slot copied to n / moved to n (the others close up); F1 tapped (its release): closed
 *   the white keys     the track through the group (the editor's scale keyboard); on the arp's lanes (pages 3, 4)
 *                      the lane's 16 steps (a tap selects; SHIFT + W a random value), KNOB 1 the step's value, KNOB 4
 *                      the length; B1..B8 stay the slots
 *   ARP tapped         (anywhere) the track's group's arp on / off: none, one in the first empty slot; no group, the
 *                      first unused one (no track on it, empty first) with an arp in it; ARP held stays FILL */

static int mf_held(const qd_app_t *a, int b) { return (int)((a->buttons_held >> b) & 1u); }
static int mf_clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

/* (P1.6f: open over the step view or the keyboard: the mode is the white keys' STEPS / KEYS, one state for both) */
int qd_mfx_open(const qd_app_t *a)
{
    return (a->ui.mode == QD_MODE_STEP || a->ui.mode == QD_MODE_KEYS) && a->ui.latch == QD_LATCH_MFX;
}
int qd_mfx_tgroup(const qd_app_t *a)
{
    const int m = qd_ctl_cpat(a)->trk[a->sel].mfx;
    return m <= QD_MFX_NGROUP ? m : 0;
}
static qd_mfx_slot_t *mf_slot(qd_app_t *a)
{
    const int g = qd_mfx_tgroup(a);
    return g ? &a->proj.mfx[g - 1].slot[a->ui.mfx_slot & 7] : 0;
}
int qd_mfx_lane_page(const qd_app_t *a)
{
    const int g = qd_mfx_tgroup(a);
    if (!qd_mfx_open(a) || !g || a->proj.mfx[g - 1].slot[a->ui.mfx_slot & 7].type != QD_MFX_ARP)
        return 0;
    return a->ui.mfx_page == 2 ? 1 : a->ui.mfx_page == 3 ? 2 : 0;
}

static void mf_first_slot(qd_app_t *a)                  /* an empty slot selected: the first with an effect (the OMX's) */
{
    const int g = qd_mfx_tgroup(a);
    int k;
    if (!g || a->proj.mfx[g - 1].slot[a->ui.mfx_slot & 7].type)
        return;
    for (k = 0; k < QD_MFX_NSLOT; k++)
        if (a->proj.mfx[g - 1].slot[k].type) {
            a->ui.mfx_slot = k;
            a->ui.mfx_page = 0;
            return;
        }
}

void qd_mfx_screen(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    if (u->mode != QD_MODE_STEP && u->mode != QD_MODE_KEYS)
        qd_ctl_set_mode(a, QD_MODE_STEP);               /* (P1.6f: from the keyboard: KEYS; else STEPS) */
    if (!u->slatch)                                     /* (P1.6e: the latch's selection kept: STEPS locks it) */
        qd_step_release_all(a);
    qd_latch_open(a, QD_LATCH_MFX);
    u->mfx_wheld = 0;
    u->mfx_drop = -1, u->mfx_f1_src = -1;
    u->mfx_slot &= 7;
    u->mfx_page = 0;
    u->mfx_knob = -1;
    u->mfx_arm = -1;
    u->mfx_f1 = 0;
    u->mfx_lift_from = -1;
    u->mfx_roll_ms = u->mfx_gsel_ms = 0;
    u->mfx_bheld = 0, u->mfx_f1_copied = u->mfx_f2_loaded = u->mfx_f2_holding = 0, u->mfx_f2_from = -1;
    mf_first_slot(a);
}
static void mf_closed(qd_app_t *a) { a->ui.mfx_arm = -1, a->ui.mfx_f1 = 0; }
static int mf_key_down(const qd_app_t *a, int b) { return (int)((a->keys_held >> QD_BLACK_KEY[b]) & 1u); }

/* the slot moved from i to j, the ones between closing up (F2 + B) */
static void mf_move(qd_mfx_group_t *G, int i, int j)
{
    const qd_mfx_slot_t s = G->slot[i];
    int k;
    if (i < j)
        for (k = i; k < j; k++)
            G->slot[k] = G->slot[k + 1];
    else
        for (k = i; k > j; k--)
            G->slot[k] = G->slot[k - 1];
    G->slot[j] = s;
}

/* the slot key held let go (by its release, or found up: P1.6e, the stuck move: the hold is the key's, never the
 * slot's place): the move previewed dropped (one undo level; the chain unchanged until now); a tap of the selected slot
 * (no knob with it): its next page */
static void mf_drop(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    const int g = qd_mfx_tgroup(a), b = u->mfx_bheld - 1;
    u->mfx_bheld = 0;
    if (!g || b < 0 || b >= QD_MFX_NSLOT)
        return;
    if (u->mfx_bused) {
        if (u->mfx_dto != u->mfx_dfrom) {
            qd_undo_hint(a, QD_UK_EDIT);
            mf_move(&a->proj.mfx[g - 1], u->mfx_dfrom, u->mfx_dto);
        }
        u->mfx_slot = u->mfx_dto;
        u->mfx_drop = u->mfx_dto, u->mfx_drop_ms = a->ms ? a->ms : 1u;
    } else if (u->mfx_bwas && a->ms - u->mfx_bms < QD_TAP_MS && a->proj.mfx[g - 1].slot[b].type) {
        const int n = qd_mfx_npages(a->proj.mfx[g - 1].slot[b].type);
        u->mfx_page = n > 0 ? (u->mfx_page + 1) % n : 0;
        u->mfx_knob = -1;
    }
}
void qd_mfx_ms(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    if (u->mfx_bheld && (!qd_mfx_open(a) || !mf_key_down(a, u->mfx_bheld - 1)))
        mf_drop(a);
    if (u->mfx_wheld && !qd_mfx_open(a))
        u->mfx_wheld = 0;
}

void qd_mfx_black(qd_app_t *a, int b, int down)
{
    qd_ui_t *u = &a->ui;
    const int g = qd_mfx_tgroup(a);
    qd_mfx_slot_t *s = mf_slot(a);
    if (!down) {
        if (b < QD_MFX_NSLOT && u->mfx_bheld == b + 1)
            mf_drop(a);
        return;
    }
    if (u->mfx_bheld && b < QD_MFX_NSLOT)               /* (another slot key: the first one's move dropped) */
        mf_drop(a);
    if (!g)
        return;
    if (b != 10)
        u->mfx_arm = -1;                                /* (any other key disarms B11) */
    if (b < QD_MFX_NSLOT) {
        qd_mfx_group_t *G = &a->proj.mfx[g - 1];
        if (mf_held(a, QD_B_OCTDN) || mf_held(a, QD_B_OCTUP))
            u->mfx_fstep = 0;                           /* (P1.6h: the picture: the slot's) */
        if (mf_held(a, QD_B_OCTDN)) {                   /* P1.6d: F1 + B, the sequencer's F1: the first slot pressed
                                                         * copied, every other one pasted (the copy loads F2's hand) */
            if (!u->mfx_f1_copied) {
                u->mfx_clip = G->slot[b];
                u->mfx_f1_copied = 1;
                u->mfx_f1_src = b;
                u->mfx_f2_loaded = u->mfx_f2_holding = 1;
            } else {                                    /* (P1.6e: no words: F1's picture shows it) */
                qd_undo_hint(a, QD_UK_PASTE);
                G->slot[b] = u->mfx_clip;
            }
            u->mfx_slot = b, u->mfx_page = 0;
            return;
        }
        if (mf_held(a, QD_B_OCTUP)) {                   /* P1.6d: F2 + B, Form's pick-up / drop (k_f2_white's): not
                                                         * loaded CUT (an empty slot too); loaded, an empty slot PASTE;
                                                         * a full one PASTE holding, CUT empty-handed */
            int cut;
            if (!u->mfx_f2_loaded)
                cut = 1;
            else if (!G->slot[b].type)
                cut = 0;
            else
                cut = !u->mfx_f2_holding;
            if (cut) {
                u->mfx_clip = G->slot[b];
                qd_mfx_slot_init(&G->slot[b], QD_MFX_NONE);
                u->mfx_f2_loaded = u->mfx_f2_holding = 1;
                u->mfx_f2_from = b;
            } else {
                G->slot[b] = u->mfx_clip;
                u->mfx_f2_holding = 0;
                if (u->mfx_f2_from >= 0 && u->mfx_f2_from != b)   /* (its picture: carried from where it was cut) */
                    u->mfx_lift_from = u->mfx_f2_from, u->mfx_lift_to = b, u->mfx_lift_ms = a->ms ? a->ms : 1u;
            }
            u->mfx_slot = b, u->mfx_page = 0;
            return;
        }
        u->mfx_bheld = b + 1, u->mfx_bused = 0, u->mfx_bwas = b == u->mfx_slot;
        u->mfx_bms = a->ms;
        u->mfx_dfrom = u->mfx_dto = u->mfx_dprev = b;
        if (b != u->mfx_slot)
            u->mfx_slot = b, u->mfx_page = 0;
        u->mfx_knob = -1;
        return;
    }
    if (b == 9 && s && s->type) {                       /* B10: bypassed / playing */
        s->bypass ^= 1u;
        return;
    }
    if (b == 10 && s && s->type) {                      /* B11: armed, then removed */
        if (u->mfx_arm == u->mfx_slot) {
            qd_undo_hint(a, QD_UK_CLEAR);
            qd_mfx_slot_init(s, QD_MFX_NONE);
            u->mfx_arm = -1;
            u->mfx_page = 0;
            qd_ctl_confirm(a, "REMOVED", "");
        } else {
            u->mfx_arm = u->mfx_slot;
        }
    }
}

void qd_mfx_white(qd_app_t *a, int key, int w, int down, int vel)
{
    qd_ui_t *u = &a->ui;
    const int lane = qd_mfx_lane_page(a);
    u->mfx_arm = -1;
    if (lane) {                                         /* the arp's lane: its steps */
        qd_mfx_slot_t *s = mf_slot(a);
        if (!down || !s || w < 0 || w >= QD_MFX_LANE)
            return;
        u->mfx_lane[lane - 1] = w;
        if (qd_ctl_shift(a)) {                          /* SHIFT + W: a random value there */
            uint32_t x = a->ui.rnd_seed * 1664525u + 1013904223u + a->ms;
            a->ui.rnd_seed = x;
            s->lane[lane - 1][w] = (int8_t)(lane == 1 ? (int)((x >> 16) % QD_NAL) : (int)((x >> 16) % 25) - 12);
            u->shift_used = 1;
        }
        return;
    }
    if (u->mode == QD_MODE_STEP) {                      /* P1.6e, STEPS: held, the page's steps (ALGORITHM their MIDI
                                                         * FX); no sound */
        if (w < 0 || w >= 16)
            return;
        if (down)
            u->mfx_wheld |= (uint16_t)(1u << w);
        else
            u->mfx_wheld &= (uint16_t)~(1u << w);
        if (!u->mfx_wheld && u->tk == QD_TK_MFX_STEP)
            u->tk = QD_TK_NONE;                         /* (its picture: while held) */
        return;
    }
    if (down) {                                         /* the track through the group (its release: the app's) */
        qd_app_note_on(a, key, qd_snd_scale_note(a, w), vel);
        qd_rec_key(a, a->key_note[key], 1, vel);        /* (P1.6g: recording here too) */
    } else if ((a->keys_struck >> key) & 1u) {
        qd_rec_key(a, a->key_note[key], 0, 0);
    }
}

/* P1.6e: REC tapped there: the white keys STEPS (the track's steps of the page edited) or KEYS (as before); P1.6f: the
 * mode itself, the main view's (the screen stays open over either; leaving it, the view is the mode's) */
void qd_mfx_rec(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    if (u->mode == QD_MODE_KEYS) {
        u->mode = QD_MODE_STEP;
    } else {
        u->slatch = 0;                                  /* (into the keyboard: the latch ends, as anywhere) */
        qd_step_release_all(a);
        u->mode = QD_MODE_KEYS;
    }
    u->mfx_wheld = 0;
    if (u->tk == QD_TK_MFX_STEP)
        u->tk = QD_TK_NONE, u->tk_hold = 0;
}
int qd_mfx_steps_held(const qd_app_t *a, int *first)   /* STEPS: the W keys held, else the latch's selection */
{
    const qd_ui_t *u = &a->ui;
    int w, n = 0;
    if (!qd_mfx_open(a) || u->mode != QD_MODE_STEP || qd_mfx_lane_page(a))
        return 0;
    for (w = 0; w < 16; w++)
        if ((u->mfx_wheld >> w) & 1u) {
            if (!n && first)
                *first = u->page[a->sel] * QD_PAGE + w;
            n++;
        }
    if (!n && u->slatch && u->nheld) {
        if (first)
            *first = u->held[0];
        n = u->nheld;
    }
    return n;
}

static void mf_value(qd_app_t *a, qd_mfx_slot_t *s, int id, int d)
{
    const qd_mfx_par_t *p = qd_mfx_par(s->type, id);
    int steps;
    if (!p)
        return;
    steps = (p->hi - p->lo) / (p->step ? p->step : 1) + 1;
    s->v[id] = (int16_t)mf_clampi(s->v[id] + qd_ctl_acc(a, d, steps) * (p->step ? p->step : 1), p->lo, p->hi);
    if (s->type == QD_MFX_ARP && id == QD_MA_MODE && s->v[id] != QD_AM_OFF)
        s->v[QD_MA_LAST] = s->v[id];
}

/* P1.6d: a glide (hundredths) from `from` to `to` begun at t0: eased over QD_MFX_GLIDE_MS, where it is now */
int qd_mfx_glide(const qd_app_t *a, int from, int to, uint32_t t0)
{
    float e = (float)(a->ms - t0) / (float)QD_MFX_GLIDE_MS;
    e = e < 0.f ? 0.f : e > 1.f ? 1.f : e;
    e = e * e * (3.f - 2.f * e);
    return from + (int)((float)(to - from) * e);
}

int qd_mfx_knob(qd_app_t *a, int knob, int d)
{
    qd_ui_t *u = &a->ui;
    const int g = qd_mfx_tgroup(a);
    qd_mfx_slot_t *s = mf_slot(a);
    if (knob != QD_K_ALGO)
        u->mfx_arm = -1;
    if (u->mfx_bheld && !mf_key_down(a, u->mfx_bheld - 1))
        mf_drop(a);                                     /* (P1.6e: its release missed: dropped first) */
    if (u->mfx_bheld && g && d) {                       /* P1.6d: a slot key held + any knob: that slot along the rail,
                                                         * a place a detent, the others making room; P1.6e: a preview,
                                                         * dropped at the key's release */
        u->mfx_bused = 1;
        u->mfx_dprev = u->mfx_dto;
        u->mfx_dto = mf_clampi(u->mfx_dto + d, 0, QD_MFX_NSLOT - 1);
        u->mfx_dms = a->ms ? a->ms : 1u;
        return 1;
    }
    if (knob == QD_K_ALGO && d) {                       /* P1.6e, STEPS: steps held: their MIDI FX (TRACK, OFF, 1..5),
                                                         * TRIG's KNOB 2's lock, its picture */
        int s0 = 0;
        if (qd_mfx_steps_held(a, &s0)) {
            qd_pattern_t *p = qd_ctl_pat(a);
            const int t = a->sel, m = mf_clampi(p->step[t][s0].mfx + d, 0, QD_SMFX_G1 + QD_MFX_NGROUP - 1);
            int w, i;
            if (u->mfx_wheld) {
                for (w = 0; w < 16; w++)
                    if ((u->mfx_wheld >> w) & 1u)
                        qd_step_set_mfx(p, t, u->page[t] * QD_PAGE + w, m);
            } else {
                for (i = 0; i < u->nheld; i++)
                    qd_step_set_mfx(p, t, u->held[i], m);
            }
            u->tk_step = s0;
            qd_ctl_takeover(a, QD_TK_MFX_STEP, 1, d, 0, s0);
            u->tk_hold = u->mfx_wheld != 0;             /* (while the W keys are held; the latch's: a while) */
            return 1;
        }
    }
    switch (knob) {
    case QD_K_ALGO: {                                   /* the track's group (the screen follows; P1.6d: the slot
                                                         * kept, the glide from where it is) */
        qd_tset_t *ts = &qd_ctl_pat(a)->trk[a->sel];
        const int was = qd_mfx_tgroup(a);
        const int up = u->mfx_gsel_ms && a->ms - u->mfx_gsel_ms < QD_TK_MS;
        u->mfx_gsel_from = up ? qd_mfx_glide(a, u->mfx_gsel_from, u->mfx_gsel_to, u->mfx_gsel_t0) : was * 100;
        ts->mfx = (uint8_t)mf_clampi(was + d, 0, QD_MFX_NGROUP);
        u->mfx_gsel_to = ts->mfx * 100;
        u->mfx_gsel_ms = u->mfx_gsel_t0 = a->ms ? a->ms : 1u;
        u->mfx_arm = -1;
        u->mfx_page = 0;
        return 1;
    }
    case QD_K_PRESETS: {                                /* the slot's effect (NONE, then the ten) */
        int ty;
        const int up = u->mfx_roll_ms && a->ms - u->mfx_roll_ms < QD_TK_MS;
        if (!s)
            return 1;
        u->mfx_roll_from = up ? qd_mfx_glide(a, u->mfx_roll_from, u->mfx_roll_to, u->mfx_roll_t0) : s->type * 100;
        ty = mf_clampi(s->type + d, 0, QD_MFX_NTYPE - 1);
        if (ty != s->type)
            qd_mfx_slot_init(s, ty);
        u->mfx_roll_to = ty * 100;
        u->mfx_roll_ms = u->mfx_roll_t0 = a->ms ? a->ms : 1u;
        u->mfx_page = 0;
        return 1;
    }
    case QD_K_SELECT: {                                 /* the page */
        const int n = s ? qd_mfx_npages(s->type) : 0;
        if (n > 1)
            u->mfx_page = ((u->mfx_page + (d > 0 ? 1 : -1)) % n + n) % n;
        u->mfx_knob = -1;
        return 1;
    }
    default:
        break;
    }
    if (!g || !s || !s->type || knob < QD_K_1)
        return 1;
    {
        const int k = knob - QD_K_1, id = qd_mfx_page_id(s->type, u->mfx_page, k);
        if (id == QD_MFX_ID_LANE) {                     /* the lane's step selected */
            const int ln = u->mfx_page == 2 ? 0 : 1, w = u->mfx_lane[ln] & 15;
            const int lo = ln ? -48 : 0, hi = ln ? 48 : QD_NAL - 1;
            s->lane[ln][w] = (int8_t)mf_clampi(s->lane[ln][w] + (ln ? qd_ctl_acc(a, d, 97) : d), lo, hi);
        } else if (id >= 0) {
            mf_value(a, s, id, d);
        }
        if (id != -1) {
            u->mfx_knob = k;
            u->mfx_ms = a->ms ? a->ms : 1u;
        }
    }
    return 1;
}

int qd_mfx_oct(qd_app_t *a, int d, int down)
{
    qd_ui_t *u = &a->ui;
    const int open = qd_mfx_open(a);
    if (!open && down)
        return 0;
    if (d < 0) {                                        /* F1 (P1.6h: the owner: no exit on F1; HOME or ARP close) */
        if (down) {
            u->mfx_f1 = 1;
            u->mfx_f1_ms = a->ms;
            u->mfx_fstep = 0;
        } else {
            u->mfx_f1 = 0;
            u->mfx_f1_copied = 0;
            u->mfx_f1_src = -1;
        }
    } else if (down) {
        u->mfx_fstep = 0;
    } else {                                            /* F2 let go: the hand empties (Form's) */
        u->mfx_f2_loaded = u->mfx_f2_holding = 0, u->mfx_f2_from = -1;
    }
    return down && open && u->mode != QD_MODE_STEP;     /* (STEPS: on to the step view's F1 / F2: W the steps, B9 / B11
                                                         * the page's CLEAR, UNDO; every release too) */
}

void qd_mfx_leds(const qd_app_t *a, uint8_t *key)
{
    const qd_ui_t *u = &a->ui;
    const int g = qd_mfx_tgroup(a), lane = qd_mfx_lane_page(a);
    const int bl = (int)((a->ms / QD_BLINK_MS) & 1u) == 0;
    int k;
    memset(key, 0, QD_NKEY);
    if (!lane && u->mode == QD_MODE_STEP) {             /* P1.6e, STEPS: the page's steps as in the step view (a trig
                                                         * lit, a LOCK dim, none faint), held ones blinking */
        const qd_pattern_t *p = qd_ctl_cpat(a);
        for (k = 0; k < 16; k++) {
            const int st = u->page[a->sel] * QD_PAGE + k, ty = qd_step_type(p, a->sel, st);
            int i, sel = (u->mfx_wheld >> k) & 1u;
            for (i = 0; i < u->nheld && !u->mfx_wheld; i++)
                sel |= u->held[i] == st;
            key[QD_WHITE_KEY[k]] = sel ? (bl ? QD_LED_LIT : QD_LED_OFF)
                                 : !qd_page_in_range(&p->trk[a->sel], st) ? QD_LED_OFF
                                 : ty == QD_TRIG_LOCK ? QD_LED_DIM : ty ? QD_LED_LIT : QD_LED_FAINT;
        }
    }
    if (!g)
        return;
    for (k = 0; k < QD_MFX_NSLOT; k++) {                /* B1..B8: the selected lit, holding an effect dim, empty off;
                                                         * bypassed: blinking */
        const qd_mfx_slot_t *s = &a->proj.mfx[g - 1].slot[k];
        uint8_t lv = k == u->mfx_slot ? QD_LED_LIT : s->type ? QD_LED_DIM : QD_LED_OFF;
        if (s->type && s->bypass && !bl)
            lv = k == u->mfx_slot ? QD_LED_DIM : QD_LED_FAINT;
        key[QD_BLACK_KEY[k]] = lv;
    }
    {
        const qd_mfx_slot_t *s = &a->proj.mfx[g - 1].slot[u->mfx_slot & 7];
        key[QD_BLACK_KEY[9]] = s->type && !s->bypass ? QD_LED_LIT : QD_LED_OFF;   /* B10: the slot plays */
        key[QD_BLACK_KEY[10]] = !s->type ? QD_LED_OFF : u->mfx_arm == u->mfx_slot ? (bl ? QD_LED_LIT : QD_LED_OFF)
                                                                                    : QD_LED_DIM;   /* B11 */
        if (!lane && u->mode == QD_MODE_STEP)           /* (P1.6e, STEPS: the white keys above) */
            return;
        if (lane) {                                     /* the lane's steps: within its length dim, the selected
                                                         * blinking */
            const int len = s->v[lane == 1 ? QD_MA_MLEN : QD_MA_TLEN];
            for (k = 0; k < 16; k++)
                key[QD_WHITE_KEY[k]] = k == u->mfx_lane[lane - 1] ? (bl ? QD_LED_LIT : QD_LED_DIM)
                                                                   : k < len ? QD_LED_DIM : QD_LED_OFF;
            return;
        }
    }
    {   /* the white keys: the track's scale (roots 80 %, the rest 50 %), the notes sounding full */
        uint8_t notes[128];
        int sc, rt;
        qd_ctl_sounding(a, notes);
        qd_track_key(&a->proj, qd_ctl_cpat(a), a->sel, &sc, &rt);
        for (k = 0; k < 16; k++) {
            const int n = qd_snd_scale_note(a, k);
            key[QD_WHITE_KEY[k]] = n >= 0 && n < 128 && notes[n] ? QD_LED_LIT : ((n % 12) + 12) % 12 == rt ? QD_LED_80
                                                                                                    : QD_LED_HALF;
        }
    }
}

const char *qd_mfx_knob_name(const qd_app_t *a, int k, char *val)
{
    const int g = qd_mfx_tgroup(a);
    const qd_mfx_slot_t *s;
    int id;
    val[0] = 0;
    if (!g)
        return "";
    s = &a->proj.mfx[g - 1].slot[a->ui.mfx_slot & 7];
    id = qd_mfx_page_id(s->type, a->ui.mfx_page, k);
    if (id == QD_MFX_ID_LANE) {
        const int ln = a->ui.mfx_page == 2 ? 0 : 1;
        qd_mfx_lane_text(ln, s->lane[ln][a->ui.mfx_lane[ln] & 15], val);
        return ln ? "OFFSET" : "MOD";
    }
    if (id < 0)
        return "";
    qd_mfx_value_text(s, id, val);
    return qd_mfx_par(s->type, id)->name;
}
