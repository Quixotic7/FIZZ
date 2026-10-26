/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* P1.5u: the typewriter, one keyboard for every name (PLAN.md "Typing names on the keybed"; mockups/fizz-names.html).
 * Built through ctl_latch.c (it includes this file after ctl_presets.c and ctl_lib.c, whose SAVE PRESET and name
 * editor open it with qd_type_start).
 *
 *   THE KEYS     the 11 black keys A..K, the 16 white keys L..Z and SPACE (reading order: the top row, then the
 *                bottom row); SHIFT held: 0..9 and -, . , ' & ! ? # + / ( ) * : _ ~ and SPACE. A key types at the
 *                cursor (inserting) and moves it on; upper case, no repeat, twelve characters (full: the row flashes
 *                orange, nothing typed). The suggested name starts selected: the first key replaces it.
 *   ALGORITHM    the cursor; SELECT (P1.5z: or KNOB 1) the character under it (A..Z, 0..9, the signs, SPACE)
 *   PRESETS      P1.5z (the owner): right, or KNOB 4 right: a brand-new name every detent, for ever (qd_name_roll:
 *                mostly namesizer pairs, a made-up word now and then, the sound's own now and then; a die tumbles
 *                beside it); left: back through the last ten seen (stopping at the oldest; right then forward through
 *                them first). The one shown is the name, selected again
 *   F1           deletes the character before the cursor (a selected name: all of it); SHIFT + F1 clears
 *   SAVE         confirms (SAVE PRESET: its choice, NEW or REPLACE; a project's name: kept); F2 (SAVE PRESET, REPLACE
 *                offered) switches NEW / REPLACE (P1.5v); HOME cancels (SAVE PRESET: closed; a project's name: back to
 *                the project page)
 * While a name is typed SHIFT is the signs' layer: SHIFT + a button does nothing else (P1.5b / P1.5c's shortcuts:
 * snapshots, undo, the views, the settings), SHIFT + F1 clears. */

static const char TY_BLACK[] = "ABCDEFGHIJK", TY_WHITE[] = "LMNOPQRSTUVWXYZ ";
static const char TY_SBLACK[] = "0123456789-", TY_SWHITE[] = ".,'&!?#+/()*:_~ ";
static const char TY_AL[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-.,'&!?#+/()*:_~ ";   /* SELECT's drum */

int qd_type_open(const qd_app_t *a)
{
    return a->ui.mode == QD_MODE_STEP && (a->ui.latch == QD_LATCH_SAVE || a->ui.latch == QD_LATCH_NAME);
}

char qd_type_char(int white, int idx, int shift)
{
    if (white)
        return idx >= 0 && idx < 16 ? (shift ? TY_SWHITE : TY_WHITE)[idx] : 0;
    return idx >= 0 && idx < 11 ? (shift ? TY_SBLACK : TY_BLACK)[idx] : 0;
}

static uint32_t ty_now(const qd_app_t *a) { return a->ms ? a->ms : 1u; }
static int ty_len(const qd_typer_t *y) { return (int)strlen(y->name); }

uint32_t qd_type_seed(qd_app_t *a)
{
    a->ui.name_seed = a->ui.name_seed * 1664525u + 1013904223u + a->ms * 2654435761u;
    return a->ui.name_seed ? a->ui.name_seed : 1u;
}

void qd_type_start(qd_app_t *a, const char *name, int spark)
{
    qd_typer_t *y = &a->ui.ty;
    memset(y, 0, sizeof *y);
    y->seed = y->rseed = qd_type_seed(a);               /* (P1.5y: its namesizer seed; P1.5z: its rolls') */
    snprintf(y->name, sizeof y->name, "%s", name ? name : "");
    y->cur = ty_len(y);
    y->sel = y->name[0] != 0;
    y->spark = spark && y->sel;
    y->hit = -1;
    y->fresh = -1;
    y->del_at = -1;
    if (y->name[0])                                     /* (P1.5z: the first name the history's oldest) */
        qd_type_hist_push(a, y->name);
}

void qd_type_trimmed(const qd_app_t *a, char *out)
{
    const char *s = a->ui.ty.name;
    int e;
    while (*s == ' ')
        s++;
    snprintf(out, QD_SOUND_NAME, "%s", s);
    e = (int)strlen(out);
    while (e > 0 && out[e - 1] == ' ')
        out[--e] = 0;
}

static void ty_changed(qd_app_t *a)                     /* (SAVE PRESET: NEW's name follows) */
{
    if (a->ui.latch == QD_LATCH_SAVE)
        sv_update_new(a);
}

static void ty_unselect(qd_typer_t *y, int clear)       /* the selection ends: replaced (clear) or kept */
{
    if (y->sel && clear) {
        y->name[0] = 0;
        y->cur = 0;
    }
    y->sel = 0;
}

void qd_type_key(qd_app_t *a, int white, int idx, int down)
{
    qd_typer_t *y = &a->ui.ty;
    const int shift = qd_ctl_shift(a);
    const char ch = qd_type_char(white, idx, shift);
    int len;
    if (!down || !ch)
        return;
    if (shift)
        a->ui.shift_used = 1;
    y->hit = white ? 11 + idx : idx;
    y->hit_shift = shift;
    y->hit_ms = ty_now(a);
    y->idea_ms = 0;
    y->fresh = -1;
    ty_unselect(y, 1);                                  /* the suggestion: the first key replaces it */
    len = ty_len(y);
    if (len >= QD_TY_LEN) {                             /* full: the row flashes, nothing typed */
        y->full_ms = ty_now(a);
        return;
    }
    if (y->cur > len)
        y->cur = len;
    memmove(y->name + y->cur + 1, y->name + y->cur, (size_t)(len - y->cur + 1));
    y->name[y->cur] = ch;
    y->fresh = y->cur++;
    y->spark = 0;
    ty_changed(a);
}

static void ty_delete(qd_app_t *a)                      /* F1: the character before the cursor (selected: all) */
{
    qd_typer_t *y = &a->ui.ty;
    const int len = ty_len(y);
    y->fresh = -1;
    y->idea_ms = 0;
    if (y->sel) {
        ty_unselect(y, 1);
    } else if (y->cur > 0 && y->cur <= len) {
        y->del_ch = y->name[y->cur - 1];
        y->del_at = y->cur - 1;
        y->del_ms = ty_now(a);
        memmove(y->name + y->cur - 1, y->name + y->cur, (size_t)(len - y->cur + 1));
        y->cur--;
    }
    y->spark = 0;
    ty_changed(a);
}

static void ty_clear(qd_app_t *a)                       /* SHIFT + F1 */
{
    qd_typer_t *y = &a->ui.ty;
    y->name[0] = 0;
    y->cur = 0;
    y->sel = 0;
    y->spark = 0;
    y->fresh = -1;
    y->idea_ms = 0;
    ty_changed(a);
}

/* P1.5z: the names seen, a ring of QD_TY_NHIST: k back from the newest; a name in as the newest (the walk back ends) */
const char *qd_type_hist(const qd_app_t *a, int k)
{
    const qd_typer_t *y = &a->ui.ty;
    return k >= 0 && k < y->nhist ? y->hist[(y->hnew - k + QD_TY_NHIST) % QD_TY_NHIST] : "";
}
void qd_type_hist_push(qd_app_t *a, const char *nm)
{
    qd_typer_t *y = &a->ui.ty;
    if (!nm || !nm[0])
        return;
    y->hnew = (y->hnew + 1) % QD_TY_NHIST;
    snprintf(y->hist[y->hnew], QD_SOUND_NAME, "%s", nm);
    if (y->nhist < QD_TY_NHIST)
        y->nhist++;
    y->hpos = 0;
}

/* P1.5z: PRESETS / KNOB 4: right a new name rolled (or, back in the history, forward through it first), left back
 * through it (stopping at the oldest); the one shown is the name, selected */
static void ty_roll(qd_app_t *a, int d)
{
    qd_typer_t *y = &a->ui.ty;
    const int save = a->ui.latch == QD_LATCH_SAVE;
    int n = d < 0 ? -d : d, moved = 0;
    if (n > QD_TY_NHIST)
        n = QD_TY_NHIST;
    if (!y->nhist || strcmp(y->name, qd_type_hist(a, y->hpos)))
        qd_type_hist_push(a, y->name);                  /* (typed or edited since: kept, as the newest) */
    while (n-- > 0) {
        if (d > 0 && y->hpos > 0) {
            y->hpos--;
        } else if (d > 0) {
            char nm[QD_SOUND_NAME];
            if (!save && a->ui.nm.target == 3)          /* P1.6d: a track's: from its kind */
                qd_name_track_roll(&y->rseed, a->model.t[a->ui.nm.proj & 7].kind < 7 ? a->model.t[a->ui.nm.proj & 7].kind
                                                                                     : -1, qd_type_hist(a, 0), nm);
            else
                qd_name_roll(&y->rseed, save ? a->model.t[a->sel].patch : 0, qd_type_hist(a, 0), nm);
            qd_type_hist_push(a, nm);
            y->dice_ms = ty_now(a);
            y->dice_face = 1 + (int)(qd_name_next(&y->rseed) % 6u);
        } else if (y->hpos < y->nhist - 1) {
            y->hpos++;
        } else {
            break;                                      /* (the oldest: it stops) */
        }
        moved = 1;
    }
    if (!moved && !y->nhist)
        return;
    snprintf(y->name, sizeof y->name, "%s", qd_type_hist(a, y->hpos));
    y->cur = ty_len(y);
    y->sel = 1;
    y->spark = 1;
    y->fresh = -1;
    y->idea_ms = ty_now(a);
    ty_changed(a);
}

int qd_type_knob(qd_app_t *a, int knob, int d)
{
    qd_typer_t *y = &a->ui.ty;
    int len = ty_len(y);
    if (knob == QD_K_ALGO) {                            /* the cursor (a selection ends, the cursor at its end) */
        ty_unselect(y, 0);
        y->cur = y->cur + d < 0 ? 0 : y->cur + d > len ? len : y->cur + d;
        y->idea_ms = 0;
    } else if (knob == QD_K_SELECT || knob == QD_K_1) {   /* the character under the cursor (at the end: a new one;
                                                         * P1.5z: KNOB 1 too) */
        const int n = (int)sizeof TY_AL - 1;
        int i;
        ty_unselect(y, 0);
        if (y->cur >= QD_TY_LEN)
            y->cur = QD_TY_LEN - 1;
        if (y->cur >= len) {
            y->cur = len;
            i = d > 0 ? d - 1 : n + d;
            y->name[len + 1] = 0;
        } else {
            const char *p = strchr(TY_AL, y->name[y->cur]);
            i = (p ? (int)(p - TY_AL) : 0) + d;
        }
        y->name[y->cur] = TY_AL[((i % n) + n) % n];
        y->fresh = y->cur;
        y->roll_ms = ty_now(a);
        y->spark = 0;
        y->idea_ms = 0;
        ty_changed(a);
    } else if (knob == QD_K_PRESETS || knob == QD_K_4 || knob == QD_K_2) {   /* P1.5z: names rolled, endless; left: the
                                                         * last ten (KNOB 2: the settings page's knob that opened it) */
        ty_roll(a, d);
    }
    return 1;                                           /* (KNOB 3: nothing) */
}

int qd_type_button(qd_app_t *a, int button, int down)
{
    qd_ui_t *u = &a->ui;
    const int shift = qd_ctl_shift(a), save = u->latch == QD_LATCH_SAVE;
    if (shift && button != QD_B_FUNC && button != QD_B_OCTDN && button != QD_B_SAVE) {
        if (down) {                                     /* SHIFT + a button: nothing (SHIFT is the signs here); its
                                                         * hold (REC armed, PLAY's F3, LFO's screen, the maps) neither */
            u->shift_used = 1;
            if (button == QD_B_REC)
                u->rec_down_used = 1;
            else if (button == QD_B_PLAY)
                u->play_used = 1;
            else if (button == QD_B_MUTE)
                u->lfo_used = 1;
            else if (button == QD_B_TRACKS)
                u->tracks_tap = 0;
        }
        return 1;
    }
    switch (button) {
    case QD_B_FUNC:                                     /* SHIFT: the signs' layer (b_shift tracks it) */
        return 0;
    case QD_B_OCTDN:                                    /* F1: delete; SHIFT + F1: clear */
        if (down) {
            if (shift)
                u->shift_used = 1, ty_clear(a);
            else
                ty_delete(a);
        }
        u->oct_eaten &= (uint8_t)~1u;
        return 1;
    case QD_B_OCTUP:                                    /* F2: NEW / REPLACE (where offered) */
        if (down && save && u->sv.replace >= 0)
            u->sv.choice ^= 1;
        u->oct_eaten &= (uint8_t)~2u;
        return 1;
    case QD_B_SAVE:                                     /* SAVE: confirmed (SAVE PRESET: the choice) */
        if (down) {
            if (save && u->sv.choice && u->sv.replace >= 0) {
                qd_sv_replace(a);
            } else if (save) {
                qd_sv_new(a);
            } else {
                name_keep(a);
                name_back(a);
            }
        }
        return 1;
    case QD_B_HOME:                                     /* HOME: cancelled (SAVE PRESET: b_home closes it) */
        if (save)
            return 0;
        if (down)
            name_back(a);
        return 1;
    default:                                            /* (the rest as everywhere: PLAY, REC, SEQ ..) */
        return 0;
    }
}

/* the LEDs: all 27 keys lit (dim), the ones held bright */
void qd_type_leds(const qd_app_t *a, uint8_t *key)
{
    int i;
    for (i = 0; i < 16; i++)
        key[QD_WHITE_KEY[i]] = (a->keys_held >> QD_WHITE_KEY[i]) & 1u ? QD_LED_LIT : QD_LED_DIM;
    for (i = 0; i < 11; i++)
        key[QD_BLACK_KEY[i]] = (a->keys_held >> QD_BLACK_KEY[i]) & 1u ? QD_LED_LIT : QD_LED_DIM;
}
