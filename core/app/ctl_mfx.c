/* SPDX-License-Identifier: GPL-3.0-only */
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

int qd_mfx_open(const qd_app_t *a) { return a->ui.mode == QD_MODE_STEP && a->ui.latch == QD_LATCH_MFX; }
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
    if (u->mode != QD_MODE_STEP)
        qd_ctl_set_mode(a, QD_MODE_STEP);
    u->slatch = 0;
    qd_step_release_all(a);
    qd_latch_open(a, QD_LATCH_MFX);
    u->mfx_slot &= 7;
    u->mfx_page = 0;
    u->mfx_knob = -1;
    u->mfx_arm = -1;
    u->mfx_f1 = u->mfx_f1_used = 0;
    u->mfx_lift_from = -1;
    u->mfx_roll_ms = u->mfx_gsel_ms = 0;
    mf_first_slot(a);
}
static void mf_closed(qd_app_t *a) { a->ui.mfx_arm = -1, a->ui.mfx_f1 = 0; }

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

void qd_mfx_black(qd_app_t *a, int b, int down)
{
    qd_ui_t *u = &a->ui;
    const int g = qd_mfx_tgroup(a);
    qd_mfx_slot_t *s = mf_slot(a);
    if (!down || !g)
        return;
    if (b != 10)
        u->mfx_arm = -1;                                /* (any other key disarms B11) */
    if (b < QD_MFX_NSLOT) {
        qd_mfx_group_t *G = &a->proj.mfx[g - 1];
        if (mf_held(a, QD_B_OCTDN) && b != u->mfx_slot) {   /* F1 + B: copied there */
            G->slot[b] = G->slot[u->mfx_slot];
            u->mfx_slot = b, u->mfx_page = 0;
            u->mfx_f1_used = 1;
            qd_ctl_confirm(a, "COPIED", "");
            return;
        }
        if (mf_held(a, QD_B_OCTUP) && b != u->mfx_slot) {   /* F2 + B: moved there */
            mf_move(G, u->mfx_slot, b);
            u->mfx_lift_from = u->mfx_slot, u->mfx_lift_to = b, u->mfx_lift_ms = a->ms ? a->ms : 1u;
            u->mfx_slot = b;
            return;
        }
        if (b == u->mfx_slot && G->slot[b].type) {     /* again: its next page */
            const int n = qd_mfx_npages(G->slot[b].type);
            u->mfx_page = n > 0 ? (u->mfx_page + 1) % n : 0;
        } else {
            u->mfx_slot = b, u->mfx_page = 0;
        }
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
    if (down)                                           /* the track through the group (its release: the app's) */
        qd_app_note_on(a, key, qd_snd_scale_note(a, w), vel);
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

int qd_mfx_knob(qd_app_t *a, int knob, int d)
{
    qd_ui_t *u = &a->ui;
    const int g = qd_mfx_tgroup(a);
    qd_mfx_slot_t *s = mf_slot(a);
    if (knob != QD_K_ALGO)
        u->mfx_arm = -1;
    switch (knob) {
    case QD_K_ALGO: {                                   /* the track's group (the screen follows) */
        qd_tset_t *ts = &qd_ctl_pat(a)->trk[a->sel];
        const int was = qd_mfx_tgroup(a);
        if (!(a->ms - u->mfx_gsel_ms < QD_TK_MS && u->mfx_gsel_ms))
            u->mfx_gsel_from = was;
        ts->mfx = (uint8_t)mf_clampi(was + d, 0, QD_MFX_NGROUP);
        u->mfx_gsel_ms = a->ms ? a->ms : 1u;
        u->mfx_arm = -1;
        u->mfx_page = 0;
        mf_first_slot(a);
        return 1;
    }
    case QD_K_PRESETS: {                                /* the slot's effect (NONE, then the ten) */
        int ty;
        if (!s)
            return 1;
        if (!(a->ms - u->mfx_roll_ms < QD_TK_MS && u->mfx_roll_ms))
            u->mfx_roll_from = s->type;
        ty = mf_clampi(s->type + d, 0, QD_MFX_NTYPE - 1);
        if (ty != s->type)
            qd_mfx_slot_init(s, ty);
        u->mfx_roll_ms = a->ms ? a->ms : 1u;
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
    if (!qd_mfx_open(a) && !(d < 0 && !down && u->mfx_f1))
        return 0;
    if (d < 0) {                                        /* F1: a tap closes the screen at its release */
        if (down) {
            u->mfx_f1 = 1, u->mfx_f1_used = 0;
            return 1;
        }
        if (u->mfx_f1 && !u->mfx_f1_used && qd_mfx_open(a))
            qd_latch_close(a);
        u->mfx_f1 = 0;
        return 1;
    }
    return 1;                                           /* F2: held for a move (with a B) */
}

void qd_mfx_leds(const qd_app_t *a, uint8_t *key)
{
    const qd_ui_t *u = &a->ui;
    const int g = qd_mfx_tgroup(a), lane = qd_mfx_lane_page(a);
    const int bl = (int)((a->ms / QD_BLINK_MS) & 1u) == 0;
    int k;
    memset(key, 0, QD_NKEY);
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

/* ARP tapped: the track's group's arp on / off. No group: the first one no track of the pattern uses (an empty one
 * first), the track routed to it, an arp added; a group with no arp: one added in its first empty slot */
void qd_mfx_arp_tap(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    qd_pattern_t *P = qd_ctl_pat(a);
    int g = qd_mfx_tgroup(a), on = 0, k;
    qd_undo_hint(a, QD_UK_EDIT);
    if (!g) {                                           /* (unused and empty, else unused, else empty) */
        int pass, t;
        for (pass = 0; pass < 3 && !g; pass++)
            for (k = 1; k <= QD_MFX_NGROUP && !g; k++) {
                int used = 0, empty = 1, j;
                for (t = 0; t < QD_PAT_NTRACK; t++)
                    used |= P->trk[t].mfx == k;
                for (j = 0; j < QD_MFX_NSLOT; j++)
                    empty &= a->proj.mfx[k - 1].slot[j].type == QD_MFX_NONE;
                if ((pass == 0 && !used && empty) || (pass == 1 && !used) || (pass == 2 && empty))
                    g = k;
            }
        if (!g) {
            qd_ctl_confirm(a, "NO GROUP", "");
            return;
        }
        P->trk[a->sel].mfx = (uint8_t)g;
    }
    k = qd_mfx_arp_slot(&a->proj.mfx[g - 1]);
    u->tk_new = -1;
    if (k < 0) {
        k = qd_mfx_arp_toggle(&a->player.mfx, g, &on);
        if (k < 0) {
            qd_ctl_confirm(a, "GROUP FULL", "");
            return;
        }
        u->tk_new = k;
    } else {
        qd_mfx_arp_toggle(&a->player.mfx, g, &on);
    }
    u->tk_g = g, u->tk_on = on;
    qd_ctl_takeover(a, QD_TK_MFX_ARP, 0, 0, 0, -1);
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
