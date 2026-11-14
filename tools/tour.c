/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* tour: FIZZ headless on the host, playing a script of panel gestures (ported from EvilGenius's tools/eg/tour.c). The
 * app (core/app) runs on the core's clock (a tick a ms, a frame every 15 ms, the audio in 128-frame blocks as they fall
 * due, as render_audio drives it) with a RAM flash of the host's layout attached (1 MiB, persist.h: the store the web
 * Manager's fizzstore.js reads as layout "host"), so projects and MY PRESETS can be saved, written out and loaded.
 *
 *     tour SCRIPT OUT_DIR                 (make -C tools tour: tools/build/tour)
 *
 * The script, a command a line ('#' starts a comment, but on a shot's line, where it may be in the caption):
 *     wait MS                      run the app MS milliseconds
 *     play | stop                  the transport (qd_app_play / qd_app_stop)
 *     tap BUTTON | down BUTTON | up BUTTON        by the printed names: FX SEL ENV LFO EDIT GLO HOME SAVE ARP SEQ
 *                                  PLAY REC OCT- OCT+ (a tap: down, 20 ms, up, 20 ms; down / up: 20 ms after)
 *     key KEY down [VEL] | key KEY up | tapkey KEY     KEY: W1..W16, B1..B11 (or 0..26, 0 = F3); tapkey: 60 ms held
 *     knob KNOB N                  KNOB: SELECT ALGO PRESETS K1 K2 K3 K4; N detents (+ clockwise), 30 ms apart
 *     shot NAME [caption..]        OUT_DIR/NAME.png (and a line in OUT_DIR/index.tsv)
 *     reset                        a power-on: the app again, on the flash as it is now (erase first for a blank one)
 *     erase                        the RAM flash blanked (all FF; takes effect at the next reset / load)
 *     wav PATH                     the audio from here on into PATH (16-bit stereo 44.1 kHz; closed at the script's end)
 *     film PATH                    the screen from here on into PATH, 30 frames a second, raw RGB24 (240 x 240), in
 *                                  step with a wav begun on the same line of time
 *     hold MS                      the film keeps its last picture for MS (the app and the sound run on)
 *     mark NAME                    prints the film's time here (ms)
 *     chain P P ..                 the patterns PLAY plays, one after another, and round again (1 = A01; up to 16)
 *     solo TRACK                   the first eight patterns keep only TRACK's steps (0..5)
 *     seed N                       PLAY's random seed fixed (qd_app_seed; 0: the clock's)
 *     pattern P                    the pattern selected (1 = A01 .. 64 = D16), as SEQ + W would (stopped: at once)
 *     project N [new]              project slot N (1..8) loaded (a blank one made when the slot is empty, or with new)
 *     name TEXT                    the project worked on renamed (up to 12 characters)
 *     song [ARGS..]                the linked song's hook (below) called, then the tracks take the pattern's sounds
 *     flush                        everything changed written into the RAM flash now (qd_persist_flush)
 *     store PATH                   flush, then the RAM flash (1 MiB, the host's layout) written to PATH: the image
 *                                  tools/pack_fizz.mjs turns into a .fizzprj / .fizzlib
 *     load PATH [N]                the RAM flash read from PATH (a 1 MiB host image: store's, or pack_fizz.mjs image's)
 *                                  and a power-on on it; with N, project slot N loaded after
 *
 * THE SONG HOOK. A C file linked in (make -C tools tour SONG=demo_song.c; tools/demo_song.c is linked by default when
 * it exists) defines
 *
 *     int fizz_tour_song(qd_app_t *a, int argc, char **argv);    argv[0] "song", then the script line's words
 *
 * and fills the project the app is working on through the core's editing API, as core/seq/demo.c does
 * (qd_proj_pat(&a->proj, k) and qd_step_on / qd_step_set_notes / qd_lock_set .., the patterns' track sounds
 * P->snd[t], qd_pool_add_sound(a, &s) for the pool (a MY preset made for each sound: it is a reference),
 * qd_lib_store_ex(a, slot, name, patch, 0) for MY PRESETS). It returns 0 on success (anything else: the script's
 * line failed). After it the tour reselects the pattern selected (the tracks take its sounds, the tempo its BPM) and
 * marks the project edited (written at the next flush / store). Without a song linked the weak default says so and
 * fails. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "ctl.h"
#include "plutovg.h"
#include "qd_gfx.h"

static qd_app_t A;

/* ---------------------------------------------------------------- the RAM flash (persist.h: the host's 1 MiB) --- */
#define FSIZE 0x100000u
static uint8_t FL[FSIZE];
static int f_read(void *c, uint32_t off, void *d, uint32_t n)
{
    (void)c;
    if (off + n > FSIZE)
        return -8;
    memcpy(d, FL + off, n);
    return 0;
}
static int f_erase(void *c, uint32_t off)
{
    (void)c;
    memset(FL + off / 4096u * 4096u, 0xFF, 4096u);
    return 0;
}
static int f_prog(void *c, uint32_t off, const void *src, uint32_t n)
{
    uint32_t i;
    (void)c;
    if (off / 256u != (off + n - 1u) / 256u || off + n > FSIZE)
        return -8;
    for (i = 0; i < n; i++)
        FL[off + i] &= ((const uint8_t *)src)[i];
    return 0;
}
static const qd_flash_t FLASH = {f_read, f_erase, f_prog, 0};

/* ---------------------------------------------------------------- the song hook (weak: a song file overrides it) --- */
__attribute__((weak)) int fizz_tour_song(qd_app_t *a, int argc, char **argv)
{
    (void)a, (void)argc, (void)argv;
    fprintf(stderr, "tour: no song linked (make -C tools tour SONG=your_song.c)\n");
    return 1;
}

/* ---------------------------------------------------------------- the clock, the film, the sound --- */
static uint32_t now;
static uint64_t frames_done;
static uint16_t lcd[QD_W * QD_H];
static FILE *wavf, *filmf;
static uint32_t wavn, filmms, filmn, filmhold;      /* (the film's own clock: a reset does not turn it back) */

static void film_frame(void)
{
    static unsigned char rgb[QD_W * QD_H * 3];
    int i;
    for (i = 0; i < QD_W * QD_H && !filmhold; i++) {    /* (held: the last picture again) */
        const uint16_t be = lcd[i], c = (uint16_t)((be >> 8) | (be << 8));
        rgb[i * 3] = (unsigned char)(((c >> 11) & 31u) * 255u / 31u);
        rgb[i * 3 + 1] = (unsigned char)(((c >> 5) & 63u) * 255u / 63u);
        rgb[i * 3 + 2] = (unsigned char)((c & 31u) * 255u / 31u);
    }
    fwrite(rgb, 1, sizeof rgb, filmf);
}

static void run(uint32_t n)
{
    while (n--) {
        now++;
        qd_app_tick(&A, now);
        if (now % 15u == 0u)
            qd_app_frame(&A, lcd);
        if (filmf) {
            if ((uint64_t)filmn * 1000u <= (uint64_t)filmms * 30u) {
                film_frame();
                filmn++;
            }
            filmms++;
            if (filmhold)
                filmhold--;
        }
        while (frames_done + 128u <= (uint64_t)now * QD_FS / 1000u) {
            int16_t blk[256];
            qd_app_audio(&A, blk, 128);
            if (wavf)
                wavn += (uint32_t)fwrite(blk, 2, 256, wavf) * 2u;
            frames_done += 128u;
        }
    }
}

static int write_png(const char *path)
{
    plutovg_surface_t *s = plutovg_surface_create(QD_W, QD_H);
    unsigned char *d = plutovg_surface_get_data(s);
    const int stride = plutovg_surface_get_stride(s);
    int x, y, ok;
    for (y = 0; y < QD_H; y++)
        for (x = 0; x < QD_W; x++) {
            const uint16_t be = lcd[y * QD_W + x], c = (uint16_t)((be >> 8) | (be << 8));
            const uint32_t r = ((c >> 11) & 31u) * 255u / 31u, g = ((c >> 5) & 63u) * 255u / 63u,
                           b = (c & 31u) * 255u / 31u;
            ((uint32_t *)(void *)(d + y * stride))[x] = 0xFF000000u | (r << 16) | (g << 8) | b;
        }
    ok = plutovg_surface_write_to_png(s, path);
    plutovg_surface_destroy(s);
    return ok;
}

static int button(const char *s)
{
    static const char *const N[QD_NBTN] = {"FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME",
                                           "SAVE", "ARP", "SEQ", "PLAY", "REC", "OCT-", "OCT+"};
    int i;
    for (i = 0; i < QD_NBTN; i++)
        if (!strcmp(s, N[i]))
            return i;
    return -1;
}
static int knob(const char *s)
{
    static const char *const N[QD_NKNOB] = {"SELECT", "ALGO", "PRESETS", "K1", "K2", "K3", "K4"};
    int i;
    for (i = 0; i < QD_NKNOB; i++)
        if (!strcmp(s, N[i]))
            return i;
    return -1;
}
static int key(const char *s)
{
    int n = atoi(s + 1);
    if (s[0] == 'W' && n >= 1 && n <= 16)
        return QD_WHITE_KEY[n - 1];
    if (s[0] == 'B' && n >= 1 && n <= 11)
        return QD_BLACK_KEY[n - 1];
    if (s[0] < '0' || s[0] > '9')
        return -1;
    n = atoi(s);
    return n >= 0 && n < QD_NKEY ? n : -1;
}

/* a power-on on the flash as it is */
static void reset(void)
{
    qd_app_init(&A);
    now = 0;
    frames_done = 0;
    qd_app_tick(&A, 0);
    qd_persist_attach(&A, &FLASH);
    run(100);
}

/* everything written (a few tries: a record queued as another finished) */
static int flush(void)
{
    int i;
    for (i = 0; i < 50; i++) {
        if (qd_persist_flush(&A))
            return 1;
        run(1);
    }
    return 0;
}

/* the pattern edited reselected: its track sounds into the tracks, its tempo */
static void resync(void)
{
    A.snd_pat = -1;
    A.edit_gen++;
    A.per.maybe_dirty = 1;
    if (!A.playing)                                     /* (playing: a select would queue it, the chain lost) */
        qd_seq_select(&A.seq, A.seq.cur, 1);
    qd_app_tempo(&A, qd_seq_bpm(&A.seq));
}

int main(int argc, char **argv)
{
    char line[512], path[1024];
    FILE *f, *idx;
    int ln = 0, fails = 0;
    if (argc < 3) {
        fprintf(stderr, "usage: tour SCRIPT OUT_DIR\n");
        return 2;
    }
    if (!(f = fopen(argv[1], "r"))) {
        perror(argv[1]);
        return 2;
    }
    snprintf(path, sizeof path, "%s/index.tsv", argv[2]);
    if (!(idx = fopen(path, "w"))) {
        perror(path);
        return 2;
    }
    memset(FL, 0xFF, sizeof FL);
    reset();
    while (fgets(line, sizeof line, f)) {
        char cmd[32] = "", a[256] = "", b[64] = "", c[64] = "";
        char *hash = strchr(line, '#');
        int n;
        ln++;
        if (hash && strncmp(line, "shot", 4))
            *hash = 0;
        n = sscanf(line, "%31s %255s %63s %63s", cmd, a, b, c);
        if (n < 1)
            continue;
        if (!strcmp(cmd, "wait"))
            run((uint32_t)atoi(a));
        else if (!strcmp(cmd, "reset"))
            reset();
        else if (!strcmp(cmd, "erase"))
            memset(FL, 0xFF, sizeof FL);
        else if (!strcmp(cmd, "wav")) {
            static const unsigned char H[44] = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ',
                                                16, 0, 0, 0, 1, 0, 2, 0, 0x44, 0xAC, 0, 0, 0x10, 0xB1, 2, 0, 4, 0, 16, 0,
                                                'd', 'a', 't', 'a', 0, 0, 0, 0};
            if (wavf)
                goto bad;                               /* (one wav a script) */
            if (!(wavf = fopen(a, "wb")))
                goto bad;
            fwrite(H, 1, 44, wavf);
        } else if (!strcmp(cmd, "film")) {
            if (filmf)
                fclose(filmf);
            if (!(filmf = fopen(a, "wb")))
                goto bad;
            filmms = 0, filmn = 0;
        } else if (!strcmp(cmd, "mark"))
            printf("%-12s %6u ms\n", a, (unsigned)filmms);
        else if (!strcmp(cmd, "hold"))
            filmhold = (uint32_t)atoi(a);
        else if (!strcmp(cmd, "chain")) {               /* (the sequencer's chain: the panel has no page for it) */
            int pats[QD_SEQ_NCHAIN], np = 0;
            char *q = strstr(line, "chain") + 5;
            while (np < QD_SEQ_NCHAIN && *q) {
                char *e;
                const long v = strtol(q, &e, 10);
                if (e == q)
                    break;
                pats[np++] = (int)v - 1;
                q = e;
            }
            qd_seq_chain(&A.seq, pats, np);
        } else if (!strcmp(cmd, "solo")) {              /* (a check of one track's level) */
            int p, t;
            for (p = 0; p < 8; p++)
                for (t = 0; t < QD_PAT_NTRACK; t++)
                    if (t != atoi(a))
                        memset(qd_proj_pat(&A.proj, p)->step[t], 0, sizeof qd_proj_pat(&A.proj, p)->step[t]);
        } else if (!strcmp(cmd, "seed"))
            qd_app_seed(&A, (uint32_t)strtoul(a, 0, 10));
        else if (!strcmp(cmd, "pattern")) {
            const int p = atoi(a) - 1;
            if (p < 0 || p >= QD_NPAT)
                goto bad;
            qd_seq_select(&A.seq, p, 1);
            resync();
            run(5);
        } else if (!strcmp(cmd, "project")) {
            const int p = atoi(a) - 1;
            if (p < 0 || p >= QD_NPROJ || !(n > 2 && !strcmp(b, "new") ? qd_proj_new(&A, p) : qd_proj_load(&A, p)))
                goto bad;
            run(20);
        } else if (!strcmp(cmd, "name")) {
            char *q = strstr(line, "name") + 4;
            while (*q == ' ' || *q == '\t')
                q++;
            q[strcspn(q, "\r\n")] = 0;
            qd_proj_name(&A, A.per.proj, q);
            A.edit_gen++;
            A.per.maybe_dirty = 1;
        } else if (!strcmp(cmd, "song")) {
            char *av[16], *tok, words[512];
            int ac = 0;
            snprintf(words, sizeof words, "%s", line);
            for (tok = strtok(words, " \t\r\n"); tok && ac < 16; tok = strtok(0, " \t\r\n"))
                av[ac++] = tok;
            if (fizz_tour_song(&A, ac, av))
                goto bad;
            resync();
            run(20);
        } else if (!strcmp(cmd, "flush")) {
            if (!flush())
                goto bad;
        } else if (!strcmp(cmd, "store")) {
            FILE *o;
            if (!flush() || !(o = fopen(a, "wb")))
                goto bad;
            fwrite(FL, 1, sizeof FL, o);
            fclose(o);
            printf("store  %s (project %d \"%s\")\n", a, A.per.proj + 1, A.per.pname[A.per.proj]);
        } else if (!strcmp(cmd, "load")) {
            FILE *i = fopen(a, "rb");
            size_t got;
            if (!i)
                goto bad;
            got = fread(FL, 1, sizeof FL, i);
            fclose(i);
            if (got != sizeof FL) {
                fprintf(stderr, "%s: %s is %lu bytes, not a host store image (%u)\n", argv[1], a, (unsigned long)got,
                        FSIZE);
                memset(FL, 0xFF, sizeof FL);
                goto bad;
            }
            reset();
            if (n > 2 && !qd_proj_load(&A, atoi(b) - 1))
                goto bad;
            run(20);
        } else if (!strcmp(cmd, "play"))
            qd_app_play(&A);
        else if (!strcmp(cmd, "stop"))
            qd_app_stop(&A);
        else if (!strcmp(cmd, "tap") || !strcmp(cmd, "down") || !strcmp(cmd, "up")) {
            const int k = button(a);
            if (k < 0)
                goto bad;
            if (cmd[0] != 'u') {
                qd_app_button(&A, k, 1);
                run(20);
            }
            if (cmd[0] != 'd') {
                qd_app_button(&A, k, 0);
                run(20);
            }
        } else if (!strcmp(cmd, "key") || !strcmp(cmd, "tapkey")) {
            const int k = key(a);
            if (k < 0)
                goto bad;
            if (!strcmp(cmd, "tapkey")) {
                qd_app_key(&A, k, 1, 100);
                run(60);
                qd_app_key(&A, k, 0, 0);
                run(30);
            } else if (!strcmp(b, "down")) {
                qd_app_key(&A, k, 1, n > 3 ? atoi(c) : 100);
                run(5);
            } else if (!strcmp(b, "up")) {
                qd_app_key(&A, k, 0, 0);
                run(5);
            } else
                goto bad;
        } else if (!strcmp(cmd, "knob")) {
            int k = knob(a), d = atoi(b), s = d < 0 ? -1 : 1;
            if (k < 0)
                goto bad;
            for (d = d < 0 ? -d : d; d > 0; d--) {
                qd_app_knob(&A, k, s);
                run(30);
            }
        } else if (!strcmp(cmd, "shot")) {
            const char *cap = strstr(line, a) + strlen(a);
            char capt[400];
            while (*cap == ' ')
                cap++;
            snprintf(capt, sizeof capt, "%s", cap);
            capt[strcspn(capt, "\r\n")] = 0;
            qd_app_frame(&A, lcd);
            snprintf(path, sizeof path, "%s/%s.png", argv[2], a);
            if (!write_png(path))
                fails++;
            fprintf(idx, "%s\t%s\n", a, capt);
            printf("%s  %s\n", a, capt);
        } else {
        bad:
            fprintf(stderr, "%s:%d: what is \"%.*s\"?\n", argv[1], ln, (int)strcspn(line, "\r\n"), line);
            fails++;
        }
    }
    fclose(f);
    fclose(idx);
    if (filmf) {
        fclose(filmf);
        printf("film   %u frames (%.3f s at 30 fps)\n", (unsigned)filmn, filmn / 30.0);
    }
    if (wavf) {                                         /* the WAV's sizes */
        const uint32_t riff = 36u + wavn;
        fseek(wavf, 4, SEEK_SET), fwrite(&riff, 4, 1, wavf);
        fseek(wavf, 40, SEEK_SET), fwrite(&wavn, 4, 1, wavf);
        fclose(wavf);
        printf("wav    %u frames (%.3f s)\n", (unsigned)(wavn / 4u), wavn / 4.0 / QD_FS);
    }
    return fails ? 1 : 0;
}
