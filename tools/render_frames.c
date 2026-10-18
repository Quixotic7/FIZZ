/* SPDX-License-Identifier: GPL-3.0-only */
/* render_frames: QUADRILLE headless, for review. Runs the app (core/app) on the core's clock (tick every ms, a frame
 * every 15 ms, the audio in 128-frame blocks) with a scripted input, and writes the screen at chosen times as PNG
 * (the panel's RGB565 expanded back to 8 bits), the LEDs as a text dump and one diagram, and a short log.
 *
 *     render_frames OUT_DIR [--ms N] [--at T1,T2,..]
 *
 * The script (P1.0's review, then P1.1's): PLAY pressed at 500 ms (released at 600; P1.3: it also wakes the step
 * view), KNOB 1 turned +8 at 3000, KEYS on at 3900 and off at 4600 around key 0 (F3) held from 4000 to 4500 (track 1,
 * DRUMS: FM TONE); TRACKS held 6200..6400 with white key 3 tapped at 6300 (track
 * 3, KEYS, selected), KEYS pressed at 6500 (the KEYS screen), C4 E4 G4 B4 held from 6600 to 7700 at velocities 110, 90,
 * 100, 80, PRESETS turned +1 at 7200 (EP -> BELL: the picker). Defaults: 8000 ms, frames at 0, 1000, 2200, 2500, 4200,
 * 6000, 7000 and 7400 ms (2200 is 1.7 s after PLAY at 122 BPM: the moment mockups/png/line-01-idle.png shows; 7000 the
 * KEYS screen with the four notes held, also written as keys.png; 7400 the picker, also picker.png). A frame "at T" is
 * drawn at T exactly (the core draws every 15 ms; drawing is a pure function of the app's state, so an extra frame
 * changes nothing). The LEDs are also dumped at 550 ms (PLAY held) and 6350 (TRACKS held).
 * The app is driven directly (app.h), as the glue drives it; the ABI's own path is tested by FM1VST's ctests.
 *
 * Then P1.2's script, from a fresh app (the frames named): PATTERN held + white key 3 at 100 ms (stopped: A03 at once),
 * TRACKS tapped at 300 (the tracks screen), PLAY at 500; tracks-1.png at 3200 and tracks-2.png at 5650 (A03 playing:
 * the eight clocks at their own rates; leds-tracks.png with the second); white key 13 at 6000 (track 5 muted:
 * tracks-3-muted.png at 6500); HOME at 7000; PATTERN held + white key 1 at 7200 (A01 queued to the end of A03's master
 * length): idle-queued.png at 7600 (the name in its lit half of the blink) and idle-changed.png at 13500, after the
 * change. Its LEDs go to leds.txt too.
 *
 * Then P1.3's (render_p13.c): every screen of P1.3 from a real state, p13-*.png, and the contact sheet p13-sheet.png
 * beside line.html's mocks (--mocks DIR, default ../mockups/png). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "plutovg.h"
#include "qd_gfx.h"
#include "fonts_gen.h"

#define MAX_AT 32

static const char *const BTN_ROLE[QD_NBTN] = {"TRACKS", "PATTERN", "PAGE", "MUTE", "SOUND", "FUNC", "HOME",
                                               "SAVE", "KEYS", "MIX", "PLAY", "REC", "OCT-", "OCT+"};
static const char *const BTN_PRINTED[QD_NBTN] = {"FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME",
                                                  "SAVE", "ARP", "SEQ", "PLAY", "REC", "OCT-", "OCT+"};

int render_p13(const char *out, const char *mocks);   /* render_p13.c */
static int write_lcd_png(const char *path, const uint16_t *lcd_be);
int p13_write_lcd_png(const char *path, const uint16_t *lcd_be) { return write_lcd_png(path, lcd_be); }

static int write_lcd_png(const char *path, const uint16_t *lcd_be)
{
    plutovg_surface_t *s = plutovg_surface_create(QD_W, QD_H);
    unsigned char *d = plutovg_surface_get_data(s);
    const int stride = plutovg_surface_get_stride(s);
    int x, y, ok;
    for (y = 0; y < QD_H; y++)
        for (x = 0; x < QD_W; x++) {
            const uint16_t be = lcd_be[y * QD_W + x], c = (uint16_t)((be >> 8) | (be << 8));
            const uint32_t r = ((c >> 11) & 31u) * 255u / 31u, g = ((c >> 5) & 63u) * 255u / 63u,
                           b = (c & 31u) * 255u / 31u;
            ((uint32_t *)(void *)(d + y * stride))[x] = 0xFF000000u | (r << 16) | (g << 8) | b;
        }
    ok = plutovg_surface_write_to_png(s, path);
    plutovg_surface_destroy(s);
    return ok;
}

/* the LEDs as a little panel: the 14 buttons (role over printed label), the 27 keys as a keybed (F3 .. G5), PLAY's
 * green LED; lit = filled */
static int write_led_png(const char *path, const qd_leds_t *l, uint32_t at_ms)
{
    const int W = 760, H = 260;
    plutovg_surface_t *s = plutovg_surface_create(W, H);
    plutovg_canvas_t *c = plutovg_canvas_create(s);
    plutovg_font_face_t *f = plutovg_font_face_load_from_data(qd_fonts_gen[3].data, qd_fonts_gen[3].len, 0, NULL, NULL);
    static const int black[12] = {0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0};   /* from F: F F# G G# A A# B C C# D D# E */
    char t[64];
    int i, wi = 0, ok;
    plutovg_canvas_set_rgb(c, 0.08f, 0.08f, 0.09f);
    plutovg_canvas_rect(c, 0, 0, (float)W, (float)H);
    plutovg_canvas_fill(c);
    plutovg_canvas_set_font(c, f, 11.f);
    snprintf(t, sizeof t, "LEDs at %u ms (filled = lit)", (unsigned)at_ms);
    plutovg_canvas_set_rgb(c, 0.8f, 0.8f, 0.8f);
    plutovg_canvas_fill_text(c, t, -1, PLUTOVG_TEXT_ENCODING_UTF8, 12.f, 20.f);
    for (i = 0; i < QD_NBTN; i++) {
        const float x = 12.f + (float)i * 52.f, y = 34.f, lit = l->button[i] / 255.f;
        plutovg_canvas_round_rect(c, x, y, 46.f, 26.f, 4.f, 4.f);
        plutovg_canvas_set_rgb(c, 1.f * lit, 0.27f * lit, 0.2f * lit);
        plutovg_canvas_fill_preserve(c);
        plutovg_canvas_set_rgb(c, 0.6f, 0.6f, 0.6f);
        plutovg_canvas_stroke(c);
        plutovg_canvas_set_rgb(c, 0.75f, 0.75f, 0.75f);
        plutovg_canvas_set_font_size(c, 8.f);
        plutovg_canvas_fill_text(c, BTN_ROLE[i], -1, PLUTOVG_TEXT_ENCODING_UTF8, x + 2.f, y + 38.f);
        plutovg_canvas_fill_text(c, BTN_PRINTED[i], -1, PLUTOVG_TEXT_ENCODING_UTF8, x + 2.f, y + 48.f);
    }
    {   /* PLAY's green LED, by PLAY */
        const float x = 12.f + 10.f * 52.f + 23.f, y = 98.f, lit = l->play_green / 255.f;
        plutovg_canvas_circle(c, x, y, 6.f);
        plutovg_canvas_set_rgb(c, 0.1f * lit, 0.9f * lit, 0.3f * lit);
        plutovg_canvas_fill_preserve(c);
        plutovg_canvas_set_rgb(c, 0.6f, 0.6f, 0.6f);
        plutovg_canvas_stroke(c);
    }
    for (i = 0; i < QD_NKEY; i++) {    /* white keys first, then the black ones over them */
        if (!black[i % 12]) {
            const float x = 12.f + (float)wi * 46.f, lit = l->key[i] / 255.f;
            plutovg_canvas_rect(c, x, 120.f, 44.f, 120.f);
            plutovg_canvas_set_rgb(c, 0.2f + 0.8f * lit, 0.2f + 0.3f * lit, 0.22f + 0.1f * lit);
            plutovg_canvas_fill_preserve(c);
            plutovg_canvas_set_rgb(c, 0.6f, 0.6f, 0.6f);
            plutovg_canvas_stroke(c);
            wi++;
        }
    }
    wi = 0;
    for (i = 0; i < QD_NKEY; i++) {
        if (black[i % 12]) {
            const float x = 12.f + (float)wi * 46.f - 14.f, lit = l->key[i] / 255.f;
            plutovg_canvas_rect(c, x, 120.f, 28.f, 72.f);
            plutovg_canvas_set_rgb(c, 0.05f + 0.95f * lit, 0.05f + 0.3f * lit, 0.06f + 0.1f * lit);
            plutovg_canvas_fill_preserve(c);
            plutovg_canvas_set_rgb(c, 0.6f, 0.6f, 0.6f);
            plutovg_canvas_stroke(c);
        } else {
            wi++;
        }
    }
    ok = plutovg_surface_write_to_png(s, path);
    plutovg_font_face_destroy(f);
    plutovg_canvas_destroy(c);
    plutovg_surface_destroy(s);
    return ok;
}

static void dump_leds(FILE *o, const qd_leds_t *l, uint32_t ms)
{
    int i;
    fprintf(o, "%6u ms  keys ", (unsigned)ms);
    for (i = 0; i < QD_NKEY; i++)
        fputc(l->key[i] ? '#' : '.', o);
    fprintf(o, "  buttons");
    for (i = 0; i < QD_NBTN; i++)
        if (l->button[i])
            fprintf(o, " %s", BTN_ROLE[i]);
    fprintf(o, "%s\n", l->play_green ? "  PLAY-green" : "");
}

int main(int argc, char **argv)
{
    static const uint8_t CHORD[4] = {7, 11, 14, 18}, CVEL[4] = {110, 90, 100, 80};   /* C4 E4 G4 B4 */
    uint32_t total = 8000, at[MAX_AT] = {0, 1000, 2200, 2500, 4200, 6000, 7000, 7400}, frame_ms = 0, ms, last_frame = 0;
    int nat = 8, i, peak = 0, green = 0, failed = 0;
    uint64_t frames_done = 0;
    const char *out, *mocks = "../mockups/png";
    char path[1024];
    static uint16_t lcd[QD_W * QD_H];
    static qd_app_t app;
    qd_leds_t leds;
    FILE *log;

    if (argc < 2) {
        fprintf(stderr, "usage: render_frames OUT_DIR [--ms N] [--at T1,T2,..] [--mocks DIR]\n");
        return 2;
    }
    out = argv[1];
    for (i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--ms") && i + 1 < argc) {
            total = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (!strcmp(argv[i], "--mocks") && i + 1 < argc) {
            mocks = argv[++i];
        } else if (!strcmp(argv[i], "--at") && i + 1 < argc) {
            char *p = argv[++i];
            nat = 0;
            while (*p && nat < MAX_AT) {
                at[nat++] = (uint32_t)strtoul(p, &p, 10);
                if (*p == ',')
                    p++;
            }
        } else {
            fprintf(stderr, "render_frames: unknown argument %s\n", argv[i]);
            return 2;
        }
    }
    snprintf(path, sizeof path, "%s/leds.txt", out);
    log = fopen(path, "w");
    if (!log) {
        fprintf(stderr, "render_frames: cannot write %s (does the folder exist?)\n", path);
        return 1;
    }
    fprintf(log, "QUADRILLE P1.1, render_frames: PLAY at 500 ms, KNOB 1 +8 at 3000, key 0 (F3) held 4000..4500, TRACKS +"
                 " white key 3 at 6300, KEYS at 6500, C4 E4 G4 B4 held 6600..7700, PRESETS +1 at 7200\n");
    if (!qd_init()) {
        fprintf(stderr, "render_frames: qd_init failed\n");
        return 1;
    }
    qd_app_init(&app);
    for (ms = 0; ms <= total; ms++) {
        /* the script: the input as the glue hands it over, before the tick */
        if (ms == 500)
            qd_app_button(&app, QD_B_PLAY, 1);
        if (ms == 600)
            qd_app_button(&app, QD_B_PLAY, 0);
        if (ms == 3000)
            qd_app_knob(&app, QD_K_1, 8);
        if (ms == 3900)                                 /* (P1.3: the keybed plays in KEYS mode) */
            qd_app_button(&app, QD_B_KEYS, 1);
        if (ms == 3920)
            qd_app_button(&app, QD_B_KEYS, 0);
        if (ms == 4600)
            qd_app_button(&app, QD_B_KEYS, 1);
        if (ms == 4620)
            qd_app_button(&app, QD_B_KEYS, 0);
        if (ms == 4000)
            qd_app_key(&app, 0, 1, 100);
        if (ms == 4500)
            qd_app_key(&app, 0, 0, 0);
        if (ms == 6200)
            qd_app_button(&app, QD_B_TRACKS, 1);
        if (ms == 6300)
            qd_app_key(&app, 4, 1, 100);                /* white key 3 (A3): track 3 */
        if (ms == 6320)
            qd_app_key(&app, 4, 0, 0);
        if (ms == 6400)
            qd_app_button(&app, QD_B_TRACKS, 0);
        if (ms == 6500)
            qd_app_button(&app, QD_B_KEYS, 1);
        if (ms == 6550)
            qd_app_button(&app, QD_B_KEYS, 0);
        for (i = 0; i < 4; i++) {
            if (ms == 6600u + 20u * (uint32_t)i)
                qd_app_key(&app, CHORD[i], 1, CVEL[i]);
            if (ms == 7700)
                qd_app_key(&app, CHORD[i], 0, 0);
        }
        if (ms == 7200)
            qd_app_knob(&app, QD_K_PRESETS, 1);
        qd_app_tick(&app, ms);
        if (ms == 0 || ms - last_frame >= 15u) {
            last_frame = ms;
            frame_ms = ms;
            qd_app_frame(&app, lcd);
        }
        while (frames_done + 128u <= (uint64_t)ms * QD_FS / 1000u) {
            int16_t blk[256];
            int k;
            qd_app_audio(&app, blk, 128);
            for (k = 0; k < 256; k++)
                if (ms >= 4000 && ms < 4700 && abs(blk[k]) > peak)   /* (key 0's note, FM TONE) */
                    peak = abs(blk[k]);
            frames_done += 128u;
        }
        qd_app_leds(&app, &leds);
        if (leds.play_green != green) {  /* the beat LED's edges, for the log */
            if (ms < 2600)
                fprintf(log, "%6u ms  PLAY-green %s\n", (unsigned)ms, leds.play_green ? "on" : "off");
            green = leds.play_green;
        }
        if (ms == 550 || ms == 6350)
            dump_leds(log, &leds, ms);
        for (i = 0; i < nat; i++) {
            if (at[i] != ms)
                continue;
            qd_app_frame(&app, lcd);
            frame_ms = ms;
            snprintf(path, sizeof path, "%s/frame-%05u.png", out, (unsigned)ms);
            if (!write_lcd_png(path, lcd)) {
                fprintf(stderr, "render_frames: cannot write %s\n", path);
                failed = 1;
            }
            printf("%s (%u ms: %s, %d BPM, turn %.3f)\n", path, (unsigned)frame_ms,
                   app.playing ? "playing" : "stopped", app.bpm, (double)app.turn);
            dump_leds(log, &leds, ms);
            if (ms == 4200 || ms == 7000) {
                snprintf(path, sizeof path, "%s/leds-%05u.png", out, (unsigned)ms);
                failed |= !write_led_png(path, &leds, ms);
                printf("%s\n", path);
            }
            if (ms == 7000 || ms == 7400) {             /* the P1.1 screens by name */
                snprintf(path, sizeof path, "%s/%s.png", out, ms == 7000 ? "keys" : "picker");
                failed |= !write_lcd_png(path, lcd);
                printf("%s\n", path);
            }
        }
    }
    fprintf(log, "the sound while key 0 was held (track 1, FM TONE): peak %d\n", peak);

    /* ---- P1.2: the tracks screen while A03 plays, a pattern queued */
    fprintf(log, "\nP1.2: PATTERN + white key 3 at 100 ms (A03), TRACKS tapped at 300, PLAY at 500, white key 13 at "
                 "6000 (mute track 5), HOME at 7000, PATTERN + white key 1 at 7200 (A01 queued)\n");
    qd_app_init(&app);
    frames_done = 0;
    last_frame = 0;
    for (ms = 0; ms <= 14000; ms++) {
        static const struct { uint32_t at; const char *name; } F[] = {
            {3200, "tracks-1"}, {5650, "tracks-2"}, {6500, "tracks-3-muted"}, {7600, "idle-queued"},
            {13500, "idle-changed"}};
        if (ms == 100)
            qd_app_button(&app, QD_B_PATTERN, 1);
        if (ms == 120)
            qd_app_key(&app, 4, 1, 100);                /* white key 3 */
        if (ms == 140)
            qd_app_key(&app, 4, 0, 0);
        if (ms == 160)
            qd_app_button(&app, QD_B_PATTERN, 0);
        if (ms == 300)
            qd_app_button(&app, QD_B_TRACKS, 1);
        if (ms == 350)
            qd_app_button(&app, QD_B_TRACKS, 0);
        if (ms == 500)
            qd_app_button(&app, QD_B_PLAY, 1);
        if (ms == 550)
            qd_app_button(&app, QD_B_PLAY, 0);
        if (ms == 6000)
            qd_app_key(&app, 21, 1, 100);               /* white key 13: track 5 muted */
        if (ms == 6050)
            qd_app_key(&app, 21, 0, 0);
        if (ms == 7000)
            qd_app_button(&app, QD_B_HOME, 1);
        if (ms == 7050)
            qd_app_button(&app, QD_B_HOME, 0);
        if (ms == 7200)
            qd_app_button(&app, QD_B_PATTERN, 1);
        if (ms == 7220)
            qd_app_key(&app, 0, 1, 100);                /* white key 1: A01, queued */
        if (ms == 7240)
            qd_app_key(&app, 0, 0, 0);
        if (ms == 7260)
            qd_app_button(&app, QD_B_PATTERN, 0);
        qd_app_tick(&app, ms);
        if (ms == 0 || ms - last_frame >= 15u) {
            last_frame = ms;
            qd_app_frame(&app, lcd);
        }
        while (frames_done + 128u <= (uint64_t)ms * QD_FS / 1000u) {
            int16_t blk[256];
            qd_app_audio(&app, blk, 128);
            frames_done += 128u;
        }
        qd_app_leds(&app, &leds);
        if (ms == 7230)
            dump_leds(log, &leds, ms);                  /* (PATTERN held: A03 lit, A01 blinking, A02 dim) */
        for (i = 0; i < (int)(sizeof F / sizeof F[0]); i++) {
            char pn[4];
            if (F[i].at != ms)
                continue;
            qd_app_frame(&app, lcd);
            snprintf(path, sizeof path, "%s/%s.png", out, F[i].name);
            failed |= !write_lcd_png(path, lcd);
            printf("%s (%u ms: %s %s, queued %d)\n", path, (unsigned)ms, qd_pat_name(app.seq.cur, pn),
                   app.playing ? "playing" : "stopped", qd_seq_queued(&app.seq));
            dump_leds(log, &leds, ms);
            if (ms == 5650) {
                snprintf(path, sizeof path, "%s/leds-tracks.png", out);
                failed |= !write_led_png(path, &leds, ms);
                printf("%s\n", path);
            }
        }
    }
    fclose(log);
    printf("%s/leds.txt (the LEDs at each frame time, the beat LED's edges to 2600 ms; key 0's peak %d)\n", out, peak);
    failed |= render_p13(out, mocks);                   /* P1.3: every screen from a real state, the contact sheet */
    qd_shutdown();
    return failed;
}
