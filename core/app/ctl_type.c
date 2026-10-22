/* SPDX-License-Identifier: GPL-3.0-only */
/* P1.5u: the typewriter, one keyboard for every name (PLAN.md "Typing names on the keybed"; mockups/fizz-names.html).
 * Built through ctl_latch.c (it includes this file after ctl_presets.c and ctl_lib.c, whose SAVE PRESET and name
 * editor open it with qd_type_start).
 *
 *   THE KEYS     the 11 black keys A..K, the 16 white keys L..Z and SPACE (reading order: the top row, then the
 *                bottom row); SHIFT held: 0..9 and -, . , ' & ! ? # + / ( ) * : _ ~ and SPACE. A key types at the
 *                cursor (inserting) and moves it on; upper case, no repeat, twelve characters (full: the row flashes
 *                orange, nothing typed). The suggested name starts selected: the first key replaces it.
 *   ALGORITHM    the cursor; SELECT the character under it (A..Z, 0..9, the signs, SPACE); PRESETS name ideas (a
 *                rolodex; the one stopped on is the name, selected again)
 *   F1           deletes the character before the cursor (a selected name: all of it); SHIFT + F1 clears
 *   SAVE         confirms (SAVE PRESET: saved NEW; a project's name: kept); F2 REPLACE (SAVE PRESET, where offered);
 *                HOME cancels (SAVE PRESET: closed; a project's name: back to the project page)
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

void qd_type_start(qd_app_t *a, const char *name, int spark)
{
    qd_typer_t *y = &a->ui.ty;
    memset(y, 0, sizeof *y);
    snprintf(y->name, sizeof y->name, "%s", name ? name : "");
    y->cur = ty_len(y);
    y->sel = y->name[0] != 0;
    y->spark = spark && y->sel;
    y->hit = -1;
    y->fresh = -1;
    y->del_at = -1;
    y->idea = -1;
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

/* PRESETS' ideas: SAVE PRESET's from the sound; a project's a few word pairs (its own name first) */
static void ty_ideas(qd_app_t *a)
{
    qd_typer_t *y = &a->ui.ty;
    if (a->ui.latch == QD_LATCH_SAVE) {
        y->nidea = qd_sound_ideas(a->model.t[a->sel].patch, y->ideas, QD_TY_NIDEA);
    } else {
        static const char *const W1[10] = {"NIGHT", "SLOW", "NEON", "LATE", "DEEP", "LOST", "GOLD", "BLUE", "LOW", "ODD"};
        static const char *const W2[10] = {"BUS", "TAPE", "GLOW", "SKETCH", "DRIVE", "ROOM", "LOOP", "CITY", "RAIN",
                                           "DREAM"};
        const int p = a->ui.nm.proj < 0 ? 0 : a->ui.nm.proj;
        int i;
        y->nidea = 0;
        snprintf(y->ideas[y->nidea++], QD_SOUND_NAME, "%s", a->per.pname[p]);
        for (i = 0; y->nidea < QD_TY_NIDEA && i < 40; i++) {
            char nm[32];
            int j, dup = 0;
            snprintf(nm, sizeof nm, "%s %s", W1[(i * 3 + p) % 10], W2[(i * 7 + p * 3 + i / 10) % 10]);
            if (strlen(nm) > QD_TY_LEN)
                continue;
            for (j = 0; j < y->nidea; j++)
                dup |= !strcmp(y->ideas[j], nm);
            if (!dup)
                memcpy(y->ideas[y->nidea++], nm, QD_SOUND_NAME);
        }
    }
}

int qd_type_knob(qd_app_t *a, int knob, int d)
{
    qd_typer_t *y = &a->ui.ty;
    int len = ty_len(y);
    if (knob == QD_K_ALGO) {                            /* the cursor (a selection ends, the cursor at its end) */
        ty_unselect(y, 0);
        y->cur = y->cur + d < 0 ? 0 : y->cur + d > len ? len : y->cur + d;
        y->idea_ms = 0;
    } else if (knob == QD_K_SELECT) {                   /* the character under the cursor (at the end: a new one) */
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
    } else if (knob == QD_K_PRESETS) {                  /* name ideas: the one stopped on is the name, selected */
        if (!y->nidea)
            ty_ideas(a);
        if (y->nidea) {
            y->idea = y->idea < 0 ? (d > 0 ? d - 1 : 0) : y->idea + d;
            y->idea = y->idea < 0 ? 0 : y->idea >= y->nidea ? y->nidea - 1 : y->idea;
            memcpy(y->name, y->ideas[y->idea], QD_SOUND_NAME);
            y->cur = ty_len(y);
            y->sel = 1;
            y->spark = 1;
            y->fresh = -1;
            y->idea_ms = ty_now(a);
            ty_changed(a);
        }
    }
    return 1;                                           /* (KNOB 1..4: nothing) */
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
    case QD_B_OCTUP:                                    /* F2: REPLACE where offered */
        if (!down && save)
            qd_sv_replace(a);
        u->oct_eaten &= (uint8_t)~2u;
        return 1;
    case QD_B_SAVE:                                     /* SAVE: confirmed */
        if (down) {
            if (save) {
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
