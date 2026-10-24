/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* P1.6l (the owner: "update the labels dynamically to show what each knob and button does. Like hold Shift and it
 * should update with labels of available shortcuts"): every control's job now, a short label (ctl.h qd_labels_t: up
 * to ten characters, upper case; no job: empty), for a page that draws the panel (SysEx LABELS, fizz_sx.c). Built
 * through view.c (it includes this file: the screens' models are its, so the words follow what the controls do: the
 * sound editor's page ids (qd_snd_ids), a held step's value keys (qd_step_value_keys), the knobs' jobs
 * (qd_ctl_knob_job), the typewriter's characters (qd_type_char), the effects' and settings' cells (fill_fx, ..)).
 *
 * The order (QD_LBL_*): the 7 knobs SELECT ALGORITHM PRESETS KNOB 1..4 and MASTER, the 14 buttons in QD_B_* order
 * (the FM-1's panel labels FX TRACKS MIX LATCH EDIT SHIFT HOME SOUNDS FILL PATTERNS PLAY REC F1 F2), the 16 white
 * keys W1..W16, the 11 black keys B1..B11. The contexts, the first that holds: the typewriter; SEQ held; SEL held; the
 * browser; SHIFT held; the settings menus, the mix, the effects, the project; the sound editor; the keyboard; an F
 * layer; steps held; a mode key held; the step view. */

typedef char qd_lbl_order_ok[QD_LBL_W0 == QD_LBL_BTN0 + QD_NBTN && QD_LBL_N == 49 && QD_LBL_MASTER == QD_K_4 + 1 ? 1 : -1];
static int lb_held(const qd_app_t *a, int b) { return (int)((a->buttons_held >> b) & 1u); }
static int lb_finger(const qd_app_t *a) { return (a->keys_held & 0x7FFFFFFu) != 0; }
static void lb_set(qd_labels_t *o, int i, const char *s)
{
    int k = 0;
    if (i < 0 || i >= QD_LBL_N)
        return;
    for (; s && s[k] && k < QD_LBL_LEN - 1; k++)
        o->s[i][k] = (char)(s[k] >= 'a' && s[k] <= 'z' ? s[k] - 32 : (s[k] & 0x7F) < 0x20 ? ' ' : s[k] & 0x7F);
    o->s[i][k] = 0;
}
static void lb_setn(qd_labels_t *o, int i, const char *pre, int n)
{
    char b[24];
    snprintf(b, sizeof b, "%s%d", pre, n);
    lb_set(o, i, b);
}
#define LB_KNOB(k) (QD_LBL_KNOB0 + (k))                 /* QD_K_SELECT .. QD_K_4; QD_LBL_MASTER */
#define LB_BTN(b) (QD_LBL_BTN0 + (b))
#define LB_W(w) (QD_LBL_W0 + (w))
#define LB_B(b) (QD_LBL_B0 + (b))

/* the buttons' everyday jobs (the step view, the keyboard), each overridden by its context below */
static void lb_buttons(const qd_app_t *a, qd_labels_t *o)
{
    const qd_ui_t *u = &a->ui;
    const int keys = u->mode == QD_MODE_KEYS;
    lb_set(o, LB_BTN(QD_B_TRACKS), "EFFECTS");
    lb_set(o, LB_BTN(QD_B_PATTERN), "TRACK SET");
    lb_set(o, LB_BTN(QD_B_PAGE), "MIX");
    lb_set(o, LB_BTN(QD_B_MUTE), !u->slatch ? "LATCH" : u->lmode == QD_LMODE_MODES ? "MODES" : "VALUES");
    lb_set(o, LB_BTN(QD_B_SOUND), "EDIT");
    lb_set(o, LB_BTN(QD_B_FUNC), "SHIFT");
    lb_set(o, LB_BTN(QD_B_HOME), "HOME");
    lb_set(o, LB_BTN(QD_B_SAVE), "SAVE");
    lb_set(o, LB_BTN(QD_B_KEYS), "MIDI FX");       /* (P1.6d: a tap the MIDI FX screen, held the arp on / off;
                                                         * SHIFT + ARP: FILL) */
    lb_set(o, LB_BTN(QD_B_MIX), "PATTERNS");
    lb_set(o, LB_BTN(QD_B_PLAY), a->seq.playing ? "STOP" : "PLAY");
    lb_set(o, LB_BTN(QD_B_REC), u->rec ? "REC OFF" : keys ? "STEPS" : "KEYS");   /* (P1.6f: what a tap switches to;
                                                         * P1.6g: armed or recording, a tap stops it, in any view) */
    lb_set(o, LB_BTN(QD_B_OCTDN), keys ? "OCT-" : "F1");
    lb_set(o, LB_BTN(QD_B_OCTUP), keys ? "OCT+" : "F2");
    lb_set(o, LB_KNOB(QD_LBL_MASTER - QD_LBL_KNOB0), "VOLUME");
}

/* a knob's job (qd_ctl_knob_job) as a word */
static const char *lb_job(int job, int mode)
{
    static const char *const MAIN[UI_NFIELD] = {"NOTES", "VELOCITY", "LENGTH", "MICRO", "RATCHET", "CHANCE",
                                                "CONDITION", "TRIG", "FUNCTION", "SOUND", ""};
    static const char *const NOTEN[5] = {"NOTE 1", "NOTE 2", "NOTE 3", "NOTE 4", "NOTE 5"};
    switch (job) {
    case QD_KJ_MAIN:
    case QD_KJ_QUIET: return mode >= 0 && mode < UI_NFIELD ? MAIN[mode] : "";
    case QD_KJ_NOTE1: case QD_KJ_NOTE2: case QD_KJ_NOTE3: case QD_KJ_NOTE4: case QD_KJ_NOTE5:
        return NOTEN[job - QD_KJ_NOTE1];
    case QD_KJ_TIE: return "TIE";
    case QD_KJ_STRUM: return "STRUM";
    case QD_KJ_RAMP: return "RAMP";
    case QD_KJ_COND: return "CONDITION";
    case QD_KJ_CHANCE: return "CHANCE";
    case QD_KJ_FTYPE: return "FUNCTION";
    case QD_KJ_LEAP: return "LEAP";
    case QD_KJ_JTGT: return "TARGET";
    case QD_KJ_CHORD: return "CHORD";
    case QD_KJ_MFX: return "MIDI FX";
    default: return "";
    }
}

/* the keys as notes (the keyboard, the editor's keyboard): each key's note, by the scale */
static void lb_notes(const qd_app_t *a, qd_labels_t *o, int scale_whites)
{
    char nm[8];
    int i;
    for (i = 0; i < QD_NKEY; i++) {
        const int w = qd_ctl_white(i), b = qd_ctl_black(i);
        int n;
        if (scale_whites) {                             /* (the editor's SCALE: the white keys the scale, the blacks
                                                         * its pages) */
            if (w < 0)
                continue;
            n = qd_snd_scale_note(a, w);
        } else {
            n = qd_rec_key_note(a, i);
        }
        if (n < 0 || n > 127)
            continue;
        qd_note_name(n, nm);
        lb_set(o, w >= 0 ? LB_W(w) : LB_B(b), nm);
    }
}

/* the step view's F1 / F2 layers: their keys (P1.6h: the main pages' too); 1 when one is held */
static int lb_flayer(const qd_app_t *a, qd_labels_t *o, int layer)
{
    int i;
    if (layer == QD_LAYER_F1) {                         /* F1: the pages */
        for (i = 0; i < QD_NPAGE; i++)
            lb_setn(o, LB_B(i), "PAGE ", i + 1);
        lb_set(o, LB_B(8), "CLEAR PG");
        lb_set(o, LB_B(9), "COPY PG");
        lb_set(o, LB_B(10), "UNDO");
        for (i = 0; i < 16; i++)
            lb_set(o, LB_W(i), "COPY STEP");
        return 1;
    }
    if (layer == QD_LAYER_F2) {                         /* F2: Form's pick-up and drop */
        for (i = 0; i < 16; i++)
            lb_set(o, LB_W(i), "PICK/DROP");
        return 1;
    }
    (void)a;
    return 0;
}

void qd_ctl_labels(const qd_app_t *a, qd_labels_t *o)
{
    const qd_ui_t *u = &a->ui;
    const int shift = qd_ctl_shift(a), step = u->mode == QD_MODE_STEP;
    int i, k;
    memset(o, 0, sizeof *o);
    if (u->mode == QD_MODE_IDLE) {                      /* asleep: HOME wakes it */
        lb_set(o, LB_BTN(QD_B_HOME), "WAKE");
        lb_set(o, LB_BTN(QD_B_PLAY), a->seq.playing ? "STOP" : "PLAY");
        lb_set(o, QD_LBL_MASTER, "VOLUME");
        return;
    }
    lb_buttons(a, o);

    if (qd_type_open(a)) {                              /* the typewriter: the characters, its knobs, its buttons */
        const int save = u->latch == QD_LATCH_SAVE;
        for (i = 0; i < 16; i++) {
            const char c = qd_type_char(1, i, shift);
            const char s[2] = {c, 0};
            lb_set(o, LB_W(i), c == ' ' ? "SPACE" : s);
        }
        for (i = 0; i < 11; i++) {
            const char c = qd_type_char(0, i, shift);
            const char s[2] = {c, 0};
            lb_set(o, LB_B(i), c == ' ' ? "SPACE" : s);
        }
        memset(o->s[QD_LBL_BTN0], 0, (size_t)QD_NBTN * QD_LBL_LEN);
        lb_set(o, LB_KNOB(QD_K_SELECT), "LETTER");
        lb_set(o, LB_KNOB(QD_K_1), "LETTER");
        lb_set(o, LB_KNOB(QD_K_ALGO), "CURSOR");
        lb_set(o, LB_KNOB(QD_K_PRESETS), "NEW NAME");
        lb_set(o, LB_KNOB(QD_K_4), "NEW NAME");
        lb_set(o, LB_BTN(QD_B_OCTDN), shift ? "CLEAR" : "DELETE");
        if (save && u->sv.replace >= 0)
            lb_set(o, LB_BTN(QD_B_OCTUP), u->sv.choice ? "NEW" : "REPLACE");
        lb_set(o, LB_BTN(QD_B_SAVE), "SAVE");
        lb_set(o, LB_BTN(QD_B_HOME), "CANCEL");
        lb_set(o, LB_BTN(QD_B_FUNC), "SIGNS");
        lb_set(o, QD_LBL_MASTER, "VOLUME");
        return;
    }
    if (lb_held(a, QD_B_MIX)) {                            /* SEQ held: the patterns, the banks */
        const int bank = u->seq_bank;
        for (i = 0; i < 16; i++) {
            char b[8];
            qd_pat_name(bank * 16 + i, b);
            lb_set(o, LB_W(i), b);
        }
        for (i = 0; i < 4; i++) {
            char b[8] = "BANK A";
            b[5] = (char)('A' + i);
            lb_set(o, LB_B(i), b);
        }
        lb_set(o, LB_KNOB(QD_K_ALGO), "LAUNCH");
        return;
    }
    if (lb_held(a, QD_B_PATTERN)) {                        /* SEL held: mute, solo, the track */
        for (i = 0; i < QD_NTRACK; i++) {
            lb_setn(o, LB_W(i), "MUTE ", i + 1);
            lb_setn(o, LB_W(8 + i), "SOLO ", i + 1);
            lb_setn(o, LB_B(i), "TRACK ", i + 1);
        }
        lb_set(o, LB_BTN(QD_B_FUNC), "SETTINGS");
        return;
    }
    if (step && u->latch == QD_LATCH_BROWSE) {         /* the library browser */
        static const char *const KIND[7] = {"BASS", "KEYS", "PLUCK", "PAD", "LEAD", "DRUMS", "FX"};
        static const char *const SRC[QD_NBRSRC] = {"MY PRESETS", "POOL", "ALL"};   /* (B9: where it goes next) */
        qd_sound_t s;
        int code = -1;
        const int has = qd_br_chosen(a, &s, &code, 0);
        lb_set(o, LB_B(0), shift ? "" : "ALL");
        for (i = 0; i < 7; i++) {
            char b[16];
            snprintf(b, sizeof b, "%s%s", shift ? "TAG " : "", KIND[i]);
            lb_set(o, LB_B(1 + i), b);
        }
        if (!shift) {
            lb_set(o, LB_B(8), SRC[u->br.src % QD_NBRSRC]);
            lb_set(o, LB_B(9), has ? (qd_br_pool_k(a) ? "-POOL" : "+POOL") : "");
            lb_set(o, LB_B(10), has && code >= QD_BR_MINE_CODE && code < QD_BR_POOL_CODE ? "DELETE" : "");
        }
        lb_set(o, LB_KNOB(QD_K_PRESETS), "PRESET");
        lb_set(o, LB_KNOB(QD_K_SELECT), shift ? "TRACK" : "PRESET X10");
        lb_set(o, LB_BTN(QD_B_OCTUP), "LOAD");
        lb_set(o, LB_BTN(QD_B_OCTDN), "CANCEL");
        lb_set(o, LB_BTN(QD_B_HOME), "CANCEL");
        lb_set(o, LB_BTN(QD_B_FUNC), "TAGS");
        return;
    }
    if (shift && step && (!u->latch || u->latch == QD_LATCH_F3 || u->latch == QD_LATCH_SND ||
                          u->latch == QD_LATCH_MFX)) {   /* SHIFT's (P1.6c: the MIDI FX screen's too) */
        static const char *const SK[11] = {"SNAP SAVE", "", "RECALL", "COPY PTN", "PASTE", "TAP", "CLICK", "FOLLOW",
                                           "DOUBLE", "", "CLEAR TRK"};
        for (i = 0; i < 11; i++)
            lb_set(o, LB_B(i), QD_SHIFT_KEYS[i] ? SK[i] : "");
        for (i = 0; i < 16; i++)
            lb_set(o, LB_W(i), "");
        for (k = QD_K_SELECT; k <= QD_K_4; k++)
            lb_set(o, LB_KNOB(k), "");
        lb_set(o, LB_KNOB(QD_K_SELECT), "TRACK");
        if (u->latch == QD_LATCH_MFX) {                 /* (P1.6c: the MIDI FX screen: a lane's W a random value) */
            if (qd_mfx_lane_page(a))
                for (i = 0; i < 16; i++)
                    lb_set(o, LB_W(i), "RANDOM");
        } else if (u->latch == QD_LATCH_SND && !qd_snd_keys(a)) {   /* (the editor: SHIFT + a knob the finest step) */
            for (k = QD_K_ALGO; k <= QD_K_4; k++)
                if (k != QD_K_PRESETS)
                    lb_set(o, LB_KNOB(k), "FINE");
        } else if (u->nheld) {
            lb_set(o, LB_KNOB(QD_K_ALGO), u->hmode == UI_F_RATCH ? "RAMP" : "STRUM");
        } else {
            lb_set(o, LB_KNOB(QD_K_ALGO), "SCALE");
            lb_set(o, LB_KNOB(QD_K_PRESETS), "ROOT");
            for (k = QD_K_1; k <= QD_K_4; k++)
                lb_set(o, LB_KNOB(k), "FINE");
        }
        memset(o->s[QD_LBL_BTN0], 0, (size_t)QD_NBTN * QD_LBL_LEN);
        lb_set(o, LB_BTN(QD_B_FUNC), "SHIFT");
        lb_set(o, LB_BTN(QD_B_OCTDN), "UNDO");
        lb_set(o, LB_BTN(QD_B_OCTUP), "REDO");
        lb_set(o, LB_BTN(QD_B_HOME), "VIEW");
        lb_set(o, LB_BTN(QD_B_REC), "REC SET");
        lb_set(o, LB_BTN(QD_B_PATTERN), "SETTINGS");
        lb_set(o, LB_BTN(QD_B_MIX), "AT ONCE");
        lb_set(o, LB_BTN(QD_B_KEYS), "FILL");          /* (P1.6d) */
        return;
    }
    if (shift && u->mode == QD_MODE_KEYS) {             /* the keyboard, SHIFT: the scale, the root, the track */
        lb_set(o, LB_KNOB(QD_K_SELECT), "TRACK");
        lb_set(o, LB_KNOB(QD_K_ALGO), "SCALE");
        lb_set(o, LB_KNOB(QD_K_PRESETS), "ROOT");
        lb_set(o, LB_BTN(QD_B_REC), "REC SET");
        lb_set(o, LB_BTN(QD_B_OCTDN), "UNDO");
        lb_set(o, LB_BTN(QD_B_OCTUP), "REDO");
        lb_notes(a, o, 0);
        return;
    }
    if (step && QD_LATCH_MENU(u->latch)) {              /* the settings menus */
        static const char *const REC[4] = {"MODE", "QUANTIZE", "OVERDUB", "EDIT KEYS"};
        static const char *const SET[4] = {"KNOBS", "FOLLOW", "", ""};
        static const char *const TRK[3][4] = {{"SCALE", "ROOT", "FOLLOW KEY", "MIDI CH"},
                                              {"VOICES", "SWING", "FOLLOW", "MIDI FX"},
                                              {"KIND", "NAME", "", ""}};   /* (P1.6d: page 3) */
        static const char *const REC2[4] = {"AUTOMATION", "", "", ""};   /* P1.6d: the record settings' page 2 */
        const char *const *r = u->latch == QD_LATCH_REC ? (u->rec_page ? REC2 : REC) : u->latch == QD_LATCH_SET ? SET
                             : TRK[u->trk_page < 0 || u->trk_page > 2 ? 0 : u->trk_page];
        for (k = 0; k < 4; k++)
            lb_set(o, LB_KNOB(QD_K_1 + k), r[k]);
        if (u->latch == QD_LATCH_TRK) {
            for (i = 0; i < QD_NTRACK; i++)
                lb_setn(o, LB_B(i), "TRACK ", i + 1);
            lb_set(o, LB_KNOB(QD_K_SELECT), "PAGE");
            lb_set(o, LB_BTN(QD_B_PATTERN), "PAGE");
        }
        if (u->latch == QD_LATCH_REC)
            lb_set(o, LB_KNOB(QD_K_SELECT), "PAGE");
        lb_set(o, LB_BTN(QD_B_OCTDN), "BACK");
        lb_set(o, LB_BTN(QD_B_HOME), "BACK");
        return;
    }
    if (u->latch != QD_LATCH_MFX && lb_flayer(a, o, qd_ctl_main_layer(a))) {   /* P1.6h: the mix, the effects, the
                                                         * editor, F1 / F2 held: the step view's layers */
        return;
    }
    if (step && u->latch == QD_LATCH_MIX) {             /* the mix: the track, its level, pan, sends */
        static const char *const MIXK[2][4] = {{"LEVEL", "PAN", "DELAY", "REVERB"}, {"CHORUS", "DELAY", "REVERB", ""}};
        for (k = 0; k < 4; k++)
            lb_set(o, LB_KNOB(QD_K_1 + k), MIXK[u->mix_page & 1][k]);
        lb_set(o, LB_KNOB(QD_K_SELECT), "TRACK");
        lb_set(o, LB_KNOB(QD_K_ALGO), "TRACK");
        for (i = 0; i < QD_NTRACK; i++)
            lb_setn(o, LB_B(i), "TRACK ", i + 1);
        lb_set(o, LB_BTN(QD_B_PAGE), (u->mix_page & 1) ? "FADERS" : "SENDS");
        lb_set(o, LB_BTN(QD_B_HOME), "BACK");
        return;
    }
    if (step && u->latch == QD_LATCH_FX) {              /* the effects: the page's cells */
        static ui_fx_t fv;
        static const char *const FXB[QD_FX_NKEY] = {"OVERVIEW", "DELAY", "REVERB", "CHORUS", "MASTER", "ROUTING"};
        fill_fx(a, &fv);
        for (k = 0; k < 4; k++)
            lb_set(o, LB_KNOB(QD_K_1 + k), fv.has[k] && fv.lab[k] ? fv.lab[k] : "");
        lb_set(o, LB_KNOB(QD_K_SELECT), "PAGE");
        for (i = 0; i < QD_FX_NKEY; i++)
            lb_set(o, LB_B(i), FXB[i]);
        lb_set(o, LB_BTN(QD_B_TRACKS), "NEXT PAGE");
        lb_set(o, LB_BTN(QD_B_HOME), "BACK");
        return;
    }
    if (step && u->latch == QD_LATCH_PRJ) {             /* the project pages */
        for (i = 0; i < 8; i++)
            lb_setn(o, LB_W(i), "PROJECT ", i + 1);
        lb_set(o, LB_B(9), "NAME");
        lb_set(o, LB_B(10), "DELETE");
        lb_set(o, LB_KNOB(QD_K_SELECT), "PAGE");
        lb_set(o, LB_BTN(QD_B_OCTUP), "LOAD");
        lb_set(o, LB_BTN(QD_B_OCTDN), "BACK");
        lb_set(o, LB_BTN(QD_B_HOME), "BACK");
        return;
    }
    if (qd_mfx_open(a)) {                               /* P1.6c: the MIDI FX screen */
        const int g = qd_mfx_tgroup(a), lane = qd_mfx_lane_page(a), ml = qd_ctl_main_layer(a);
        char val[12];
        lb_set(o, LB_KNOB(QD_K_ALGO), qd_mfx_steps_held(a, 0) ? "STEP MFX" : "GROUP");   /* (P1.6e: STEPS' steps) */
        lb_set(o, LB_BTN(QD_B_REC), u->rec ? "REC OFF" : u->mode == QD_MODE_STEP ? "KEYS" : "STEPS");   /* (P1.6e:
                                                         * what REC switches to; P1.6f: the main view's) */
        if (u->mode == QD_MODE_STEP && !lane)
            for (i = 0; i < 16; i++)
                lb_setn(o, LB_W(i), "STEP ", u->page[a->sel] * 16 + i + 1);
        for (i = 0; i < 16 && ml != QD_LAYER_NONE; i++) /* P1.6h, STEPS: F1 / F2 on the steps */
            lb_set(o, LB_W(i), ml == QD_LAYER_F1 ? "COPY STEP" : "PICK/DROP");
        if (ml == QD_LAYER_F1)
            lb_set(o, LB_B(8), "CLEAR PG");
        lb_set(o, LB_BTN(QD_B_OCTDN), a->ui.mfx_f1_copied || a->ui.f1_copied ? "PASTE" : "COPY");   /* (P1.6h: no
                                                         * CLOSE) */
        lb_set(o, LB_BTN(QD_B_OCTUP), a->ui.mfx_f2_holding ? "DROP" : "PICK UP");   /* (P1.6d: the sequencer's F1 /
                                                         * F2) */
        lb_set(o, LB_BTN(QD_B_HOME), "CLOSE");
        lb_set(o, LB_BTN(QD_B_KEYS), "CLOSE");
        if (!g)
            return;
        lb_set(o, LB_KNOB(QD_K_PRESETS), "EFFECT");
        lb_set(o, LB_KNOB(QD_K_SELECT), "PAGE");
        for (i = 0; i < QD_MFX_NSLOT; i++) {
            const int ty = a->proj.mfx[g - 1].slot[i].type;
            lb_set(o, LB_B(i), ty ? qd_mfx_type_name(ty) : (i == u->mfx_slot ? "EMPTY" : ""));
            if (!ty && i != u->mfx_slot)
                lb_setn(o, LB_B(i), "SLOT ", i + 1);
        }
        if (a->proj.mfx[g - 1].slot[u->mfx_slot & 7].type) {
            lb_set(o, LB_B(9), a->proj.mfx[g - 1].slot[u->mfx_slot & 7].bypass ? "PLAY" : "BYPASS");
            lb_set(o, LB_B(10), u->mfx_arm == u->mfx_slot ? "REMOVE!" : "REMOVE");
        }
        if (ml == QD_LAYER_F1)
            lb_set(o, LB_B(10), "UNDO");
        for (k = 0; k < 4; k++)
            lb_set(o, LB_KNOB(QD_K_1 + k), qd_mfx_knob_name(a, k, val));
        for (i = 0; i < 16; i++) {
            if (lane)
                lb_setn(o, LB_W(i), "LANE ", i + 1);
        }
        if (!lane && u->mode != QD_MODE_STEP)
            lb_notes(a, o, 1);
        return;
    }
    if (qd_snd_open(a)) {                               /* the sound editor: its groups, the page's values */
        static const char *const GRP[QD_NSG] = {"PLAY/MOD", "OPERATORS", "OP ENV", "FILTERS", "AMP/FX", "LFO 1", "LFO 2",
                                                "LFO 3"};
        static const int KN[5] = {QD_K_1, QD_K_2, QD_K_3, QD_K_4, QD_K_ALGO};
        if (qd_snd_keys(a))
            lb_notes(a, o, !a->proj.rec_ekeys);
        if (!qd_snd_keys(a) || !a->proj.rec_ekeys)      /* (FULL: the black keys play too) */
            for (i = 0; i < QD_NSG; i++)
                lb_set(o, LB_B(i), GRP[i]);
        if (!qd_snd_keys(a))
            for (i = 0; i < 16; i++)
                lb_setn(o, LB_W(i), "STEP ", u->page[a->sel] * 16 + i + 1);
        lb_set(o, LB_B(8), "DICE");
        lb_set(o, LB_B(9), "NEW SOUND");
        lb_set(o, LB_B(10), u->nheld ? "CLR LOCKS" : "");
        for (k = 0; k < 5; k++)
            lb_set(o, LB_KNOB(KN[k]), qd_snd_label(a, u->snd_page, k));
        lb_set(o, LB_KNOB(QD_K_SELECT), "PAGE");
        lb_set(o, LB_KNOB(QD_K_PRESETS), u->nheld ? "SOUND LOCK" : "PRESETS");
        lb_set(o, LB_BTN(QD_B_SOUND), "CLOSE");
        lb_set(o, LB_BTN(QD_B_REC), u->rec ? "REC OFF" : qd_snd_keys(a) ? "STEPS" : "KEYS");
        return;
    }
    if (u->mode == QD_MODE_KEYS) {                      /* the keyboard */
        lb_notes(a, o, 0);
        lb_set(o, LB_KNOB(QD_K_SELECT), "TEMPO");
        lb_set(o, LB_KNOB(QD_K_PRESETS), "PRESETS");
        lb_set(o, LB_KNOB(QD_K_1), "FREQ");
        lb_set(o, LB_KNOB(QD_K_2), "DECAY");
        lb_set(o, LB_KNOB(QD_K_3), "HARM");
        lb_set(o, LB_KNOB(QD_K_4), "FEEDBACK");
        return;
    }
    /* ---- the step view */
    for (i = 0; i < 16; i++)
        lb_setn(o, LB_W(i), "STEP ", u->page[a->sel] * 16 + i + 1);
    {
        const int layer = qd_ctl_layer(a);
        if (layer == QD_LAYER_F1) {                     /* F1: the pages */
            lb_flayer(a, o, QD_LAYER_F1);
            return;
        }
        if (layer == QD_LAYER_F2) {                     /* F2: Form's pick-up and drop */
            lb_flayer(a, o, QD_LAYER_F2);
            return;
        }
        if (layer == QD_LAYER_F3 || u->latch == QD_LATCH_F3) {   /* F3: the track's length, speed, start, end */
            lb_set(o, LB_KNOB(QD_K_SELECT), "TEMPO");
            lb_set(o, LB_KNOB(QD_K_ALGO), "SWING");
            lb_set(o, LB_KNOB(QD_K_1), "LENGTH");
            lb_set(o, LB_KNOB(QD_K_2), "RATE");
            lb_set(o, LB_KNOB(QD_K_3), "START");
            lb_set(o, LB_KNOB(QD_K_4), "END");
            lb_set(o, LB_BTN(QD_B_OCTDN), "CLOSE");
            return;
        }
    }
    if (u->nheld) {                                     /* steps held: the mode's values, the knobs' jobs */
        char lab[10][6];
        uint16_t lit, def, none;
        int clr;
        qd_step_value_keys(a, lab, &lit, &def, &none, &clr);
        for (i = 0; i < 10; i++)
            lb_set(o, LB_B(i), lab[i]);
        lb_set(o, LB_B(10), u->hmode == UI_F_PLOCK ? "CLR LOCKS" : "RESET");
        if (u->slatch && u->lmode == QD_LMODE_MODES && !lb_finger(a))   /* (the latch in MODES: the hold modes) */
            for (i = 0; i < 11; i++)
                lb_set(o, LB_B(i), UI_FIELD_NAME[i < UI_NFIELD ? i : 0]);
        lb_set(o, LB_KNOB(QD_K_SELECT), "PAGE");
        lb_set(o, LB_KNOB(QD_K_PRESETS), "SOUND LOCK");
        lb_set(o, LB_KNOB(QD_K_ALGO), lb_job(QD_KJ_MAIN, u->hmode));
        for (k = QD_K_1; k <= QD_K_4; k++)
            lb_set(o, LB_KNOB(k), u->hmode == UI_F_PLOCK ? qd_quad_value_name(qd_step_quick_id(k - QD_K_1))
                                                          : lb_job(qd_ctl_knob_job(u->hmode, k), u->hmode));
        return;
    }
    for (i = 0; i < 11; i++)                            /* nothing held: the black keys the hold modes */
        lb_set(o, LB_B(i), UI_FIELD_NAME[i < UI_NFIELD ? i : 0]);
    lb_set(o, LB_KNOB(QD_K_SELECT), "PAGE");
    lb_set(o, LB_KNOB(QD_K_PRESETS), "PRESETS");
    if (u->hmode == UI_F_PLOCK) {
        lb_set(o, LB_KNOB(QD_K_ALGO), "TRACK");
        for (k = QD_K_1; k <= QD_K_4; k++)
            lb_set(o, LB_KNOB(k), qd_quad_value_name(qd_step_quick_id(k - QD_K_1)));
    } else {
        char b[24];
        snprintf(b, sizeof b, "%s DEF", UI_FIELD_NAME[u->hmode >= 0 && u->hmode < UI_NFIELD ? u->hmode : 0]);
        lb_set(o, LB_KNOB(QD_K_ALGO), b);
        for (k = QD_K_1; k <= QD_K_4; k++)
            lb_set(o, LB_KNOB(k), lb_job(qd_ctl_knob_job(u->hmode, k), u->hmode));
    }
}
