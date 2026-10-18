/* SPDX-License-Identifier: GPL-3.0-only */
/* Acceptance A (P1.1): QUADRILLE's FM TONE (core/voice/quad, through quad_voice.h) against ChoralRoot's own engine
 * compiled from the v0.25.1 tag (ref/cr_quad_ref.c), sample for sample.
 *
 * Every factory sound (INIT and the 16) on one voice, in these scenarios: notes 36, 48, 60, 72, 84 at velocities
 * 100, 127, 64, 20, 110, held 0.4 s and released, run until the engine says the voice is done (or 12 s); at 97 BPM
 * (the synced LFOs and the envelope DELAYs follow the tempo); on pool voice 3 (the LFOs' random seed is the voice's
 * index: ChoralRoot's slot 3 of the part); struck again in place while it sounds (the retrigger path: PHRT, TRIG,
 * RESET); and killed mid-note (the fade steal: one block ramped to 0). The patches themselves are compared too.
 *
 *     test_quad_bitexact            prints a line a sound and the totals; exit 1 on any difference */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "quad_voice.h"
#include "ref/bitexact.h"

#define MAXS (12u * 44100u)

static uint32_t qd_render(int preset, const be_scen_t *sc, int32_t *out, uint32_t max)
{
    static qd_quad_owner_t o;
    static qd_quad_voice_t v;
    int16_t p[QD_QUAD_NP];
    uint32_t b, n = 0;
    qd_quad_preset_patch(preset, p);
    qd_quad_owner_init(&o);
    o.bpm = (int16_t)sc->bpm;
    o.quad_patch = p;
    qd_quad_voice_init(&v, sc->vidx);
    qd_quad_voice_start(&v, p, &o, sc->note, sc->vel);
    for (b = 0; n + QD_QUAD_CTL <= max; b++) {
        if (b == sc->hold)
            qd_quad_voice_release(&v);
        if (sc->again_at && b == sc->again_at)
            qd_quad_voice_start(&v, p, &o, sc->again_note, sc->again_vel);
        if (sc->again_at && b == sc->again_at + sc->hold)
            qd_quad_voice_release(&v);
        if (sc->kill_at && b == sc->kill_at)
            qd_quad_voice_kill(&v);
        memset(out + n, 0, QD_QUAD_CTL * sizeof *out);
        qd_quad_owner_block(&o);
        if (!qd_quad_voice_render(&v, out + n))
            break;
        n += QD_QUAD_CTL;
    }
    return n;
}

int main(void)
{
    static int32_t a[MAXS], b[MAXS];
    static const be_scen_t SC[] = {
        /* note vel hold  again  kill vidx bpm */
        {36, 100, 551, 0, 0, 0, 0, 0, 120},
        {48, 127, 551, 0, 0, 0, 0, 0, 120},
        {60, 64, 551, 0, 0, 0, 0, 0, 120},
        {72, 20, 551, 0, 0, 0, 0, 0, 120},
        {84, 110, 551, 0, 0, 0, 0, 0, 120},
        {57, 90, 551, 0, 0, 0, 0, 0, 97},              /* another tempo */
        {64, 100, 400, 0, 0, 0, 0, 3, 120},            /* pool voice 3 */
        {60, 100, 300, 200, 67, 80, 0, 0, 120},        /* struck again in place (another note), while held */
        {60, 100, 150, 500, 60, 127, 0, 0, 120},       /* .. the same note again, in its release */
        {62, 100, 551, 0, 0, 0, 160, 0, 120},          /* killed mid-note: the fade steal */
    };
    const int ns = (int)(sizeof SC / sizeof SC[0]);
    int k, s, bad = 0, nsnd = cr_ref_npresets();
    uint64_t samples = 0, renders = 0;
    if (nsnd != QD_QUAD_NPRESET) {
        printf("test_quad_bitexact: ChoralRoot has %d sounds, QUADRILLE %d\n", nsnd, QD_QUAD_NPRESET);
        return 1;
    }
    for (k = 0; k < nsnd; k++) {
        int16_t pq[QD_QUAD_NP], pr[QD_QUAD_NP];
        uint64_t ks = 0;
        int kbad = 0, worst = 0;
        qd_quad_preset_patch(k, pq);
        cr_ref_preset_patch(k, pr);
        if (memcmp(pq, pr, sizeof pq) || strcmp(qd_quad_preset_name(k), cr_ref_preset_name(k))) {
            printf("  FAIL %-12s the patch or the name differs\n", qd_quad_preset_name(k));
            kbad++;
        }
        for (s = 0; s < ns; s++) {
            uint32_t na = qd_render(k, &SC[s], a, MAXS), nb = cr_ref_render(k, &SC[s], b, MAXS), i, diff = 0;
            int32_t md = 0;
            if (na != nb) {
                printf("  FAIL %-12s scenario %d: %u samples here, %u in ChoralRoot\n", qd_quad_preset_name(k), s,
                       (unsigned)na, (unsigned)nb);
                kbad++;
            }
            for (i = 0; i < (na < nb ? na : nb); i++)
                if (a[i] != b[i]) {
                    int32_t d = abs(a[i] - b[i]);
                    if (!diff)
                        printf("  FAIL %-12s scenario %d: first difference at sample %u (%d against %d)\n",
                               qd_quad_preset_name(k), s, (unsigned)i, (int)a[i], (int)b[i]);
                    diff++;
                    md = d > md ? d : md;
                }
            if (diff) {
                printf("       %u samples differ, at most by %d\n", (unsigned)diff, (int)md);
                kbad++;
            }
            for (i = 0; i < na; i++)
                worst = abs(a[i]) > worst ? abs(a[i]) : worst;
            ks += na;
            renders++;
        }
        printf("  %s %-12s %d scenarios, %llu samples (%.1f s), %s; peak %d\n", kbad ? "FAIL" : "ok  ",
               qd_quad_preset_name(k), ns, (unsigned long long)ks, ks / 44100.0, kbad ? "DIFFERENT" : "identical", worst);
        samples += ks;
        bad += kbad;
    }
    printf("test_quad_bitexact: %d sounds x %d scenarios = %llu renders, %llu samples compared: %s\n", nsnd, ns,
           (unsigned long long)renders, (unsigned long long)samples, bad ? "DIFFERENCES" : "all identical");
    return bad ? 1 : 0;
}
