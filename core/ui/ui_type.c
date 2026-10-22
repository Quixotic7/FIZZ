/* SPDX-License-Identifier: GPL-3.0-only */
/* P1.5u: the typewriter's screen (ui.h ui_type_t): SAVE PRESET's name and a project's, typed on the keybed, drawn as
 * mockups/fizz-names.html draws them: its positions, sizes, colours and motions (saveHead, nameRow, keybed, the
 * letter's flight, the ideas' rolodex, the SHIFT chip, the full row's flash, the deleted letter dropping away). Built
 * through ui_layer.c after ui_presets.c (its pr_button). PLAN §9a's clarity rules: the name, the keys' characters
 * (13 px), the ideas, the few words the mock-up has (SHIFT, NEW, REPLACE); the rest is drawing, colour and motion. */

#define TY_X0 9.f                                       /* the name's twelve cells */
#define TY_CW 18.5f
#define TY_Y 72.f
#define TY_KY 152.f                                     /* the keybed's top */
#define TY_KW 14.4f
#define TY_WH 76.f
#define TY_BW 12.5f
#define TY_BH 42.f
#define TY_GLOW 0x2a2414u                               /* a key pressed: its fill */

static const char TYD_BLACK[] = "ABCDEFGHIJK", TYD_WHITE[] = "LMNOPQRSTUVWXYZ ";
static const char TYD_SBLACK[] = "0123456789-", TYD_SWHITE[] = ".,'&!?#+/()*:_~ ";
static const int TY_BX[11] = {1, 2, 3, 5, 6, 8, 9, 10, 12, 13, 15};   /* a black key sits after this many white keys */

static float ty_black_x(int j) { return 5.f + (float)TY_BX[j] * TY_KW - TY_BW / 2.f - 0.75f; }
static float ty_white_x(int i) { return 5.f + (float)i * TY_KW; }

/* a key's character: SPACE as a little open bracket */
static void ty_char(char c, float cx, float y, float size, int weight, qd_rgb col)
{
    if (c == ' ') {
        const float p[8] = {cx - 4.f, y - 4.f, cx - 4.f, y, cx + 4.f, y, cx + 4.f, y - 4.f};
        ui_P(p, 4, col, weight >= 700 ? 1.6f : 1.2f, 0, QD_NONE);
        return;
    }
    {
        const char s[2] = {c, 0};
        ui_T(s, cx, y, size, weight, 0.f, col, QD_CENTER);
    }
}

/* the keybed with a character on every key; hit: the key glowing (0..10 black, 11..26 white; -1 none) */
static void ty_keybed(int hit, int shift)
{
    const char *B = shift ? TYD_SBLACK : TYD_BLACK, *W = shift ? TYD_SWHITE : TYD_WHITE;
    int i, j;
    for (i = 0; i < 16; i++) {
        const float x = ty_white_x(i);
        const int on = hit == 11 + i;
        ui_RR(x, TY_KY, TY_KW - 1.5f, TY_WH, 2.5f, on ? UI_LIB_COL : UI_G2, on ? 1.8f : 1.1f, on ? TY_GLOW : UI_BG);
        ty_char(W[i], x + (TY_KW - 1.5f) / 2.f, TY_KY + TY_WH - 8.f, 13.f, on ? 700 : 500, on ? UI_LIB_COL : UI_K3);
    }
    for (j = 0; j < 11; j++) {
        const float x = ty_black_x(j);
        const int on = hit == j;
        ui_RR(x, TY_KY - 2.f, TY_BW, TY_BH, 2.5f, on ? UI_LIB_COL : UI_G1, on ? 1.8f : 1.1f, on ? TY_GLOW : 0x111111u);
        ty_char(B[j], x + TY_BW / 2.f, TY_KY + TY_BH - 11.f, 13.f, on ? 700 : 500, on ? UI_LIB_COL : UI_K3);
    }
}

/* a key's character's place on the keybed (the flight's start) */
static void ty_key_at(int hit, float *x, float *y)
{
    if (hit >= 11) {
        *x = ty_white_x(hit - 11) + (TY_KW - 1.5f) / 2.f;
        *y = TY_KY + TY_WH - 8.f;
    } else {
        *x = ty_black_x(hit < 0 ? 0 : hit) + TY_BW / 2.f;
        *y = TY_KY + TY_BH - 11.f;
    }
}

/* the sparkle: a four-pointed star */
static void ty_spark(float x, float y, float r, qd_rgb col)
{
    float p[16];
    int k;
    for (k = 0; k < 8; k++) {
        const float an = -UI_PI * 0.5f + (float)k * UI_PI / 4.f, rr = k & 1 ? r * 0.28f : r;
        p[2 * k] = x + cosf(an) * rr;
        p[2 * k + 1] = y + sinf(an) * rr;
    }
    ui_P(p, 8, col, 1.f, 1, col);
}

/* the name: twelve cells, the cursor, a selection (the suggested name, typed over by the first key) */
static void ty_row(const ui_type_t *v)
{
    const int len = (int)strlen(v->name);
    int k;
    if (v->sel && len)
        ui_RR(TY_X0 - 2.f, TY_Y - 26.f, (float)len * TY_CW + 4.f, 34.f, 4.f, QD_NONE, 1.f, 0x3a3220u);
    for (k = 0; k < 12; k++) {
        const float x = TY_X0 + (float)k * TY_CW;
        const int c = k == v->cur && !v->sel, f = k == v->fresh;
        line2(x + 2.f, TY_Y + 6.f, x + TY_CW - 3.f, TY_Y + 6.f, c ? UI_KSEL : UI_G3, c ? 2.4f : 1.f);
        if (k < len && v->name[k] != ' ') {
            const char ch[2] = {v->name[k], 0};
            ui_T(ch, x + TY_CW / 2.f, TY_Y, 24.f, f ? 500 : 300, 0.f, f ? UI_LIB_COL : UI_K3, QD_CENTER);
        }
    }
    if (v->full < 1.f && (int)(v->full * 4.f) % 2 == 0)   /* full: the row flashes orange */
        ui_RR(TY_X0 - 3.f, TY_Y - 27.f, 12.f * TY_CW + 6.f, 36.f, 4.f, UI_K4, 1.5f, QD_NONE);
    if (v->del < 1.f && v->del_at >= 0) {               /* F1: the character drops away, orange */
        const char ch[2] = {v->del_ch, 0};
        qd_save();
        qd_alpha(qd_get_alpha() * (1.f - v->del));
        if (ch[0] != ' ')
            ui_T(ch, TY_X0 + (float)v->del_at * TY_CW + TY_CW / 2.f, TY_Y - v->del * 24.f, 24.f, 300, 0.f, UI_K4,
                 QD_CENTER);
        qd_restore();
    }
    if (v->fly < 1.f && v->fresh >= 0 && v->hit >= 0) { /* the typed letter flies up from its key */
        const float e = lb_ease(v->fly);
        float kx, ky;
        ty_key_at(v->hit, &kx, &ky);
        ty_char(v->name[v->fresh], kx + (TY_X0 + (float)v->fresh * TY_CW + TY_CW / 2.f - kx) * e, ky + (TY_Y - ky) * e,
                13.f + 11.f * e, 600, UI_LIB_COL);
    }
}

/* PRESETS turned: the ideas, a rolodex */
static void ty_ideas(const ui_type_t *v)
{
    int d;
    for (d = -2; d <= 2; d++) {
        const int i = v->idea + d;
        const float y = d < 0 ? 58.f + (float)(d + 1) * 22.f : 92.f + (float)d * 24.f;
        if (i < 0 || i >= v->nidea)
            continue;
        qd_save();
        qd_alpha(qd_get_alpha() * (d == 0 ? 1.f : d == 1 || d == -1 ? 0.6f : 0.3f));
        ui_T(v->ideas[i], 120.f, y, d ? 15.f : 24.f, d ? 400 : 300, 0.f, d ? UI_G1 : UI_K3, QD_CENTER);
        qd_restore();
    }
    ty_spark(219.f, 81.f, 6.f, UI_LIB_COL);
}

void ui_type_draw(const ui_type_t *v)
{
    ui_T(v->title, 8.f, 17.f, 12.f, 600, 1.f, UI_LIB_COL, QD_LEFT);
    if (v->track)
        ui_box(214.f, 4.f, v->track, UI_TRACK_PAL[(v->track - 1) & 7]);
    line2(8.f, 25.f, 232.f, 25.f, UI_G3, 1.f);
    if (v->ideas_on) {
        ty_ideas(v);
    } else {
        ty_row(v);
        if (v->spark)
            ty_spark(227.f, 47.f, 6.f, UI_LIB_COL);
    }
    if (v->shift) {                                     /* SHIFT held: the signs' layer */
        ui_RR(8.f, 112.f, 50.f, 18.f, 4.f, UI_K3, 1.3f, QD_NONE);
        ui_T("SHIFT", 33.f, 125.f, 12.f, 600, 0.f, UI_K3, QD_CENTER);
    } else if (v->replace && !v->ideas_on) {            /* NEW (as the new name) or REPLACE (F2) */
        if (v->newname && v->newname[0])
            ui_T(v->newname, 120.f, 104.f, 13.f, 400, 0.f, UI_G1, QD_CENTER);
        pr_button(12.f, 112.f, 104.f, "NEW", UI_LIB_COL);
        pr_button(124.f, 112.f, 104.f, "REPLACE", PR_ORANGE);
    }
    if (v->sub && v->sub[0] && !v->ideas_on && !v->shift)
        ui_T(v->sub, 120.f, 116.f, 14.f, 300, 0.f, UI_LIB_COL, QD_CENTER);
    ty_keybed(v->glow, v->shift);
}
