/* SPDX-License-Identifier: GPL-3.0-only */
/* QUADRILLE (e): FM TONE's extensions beside ChoralRoot's code (research/FMTONE-DN2-PLAN.md), FIZZ only. Built only
 * through quad_voice.c, after eng_quad.c (it reads quad_voice_t, the time tables); eng_quad.c calls these at its
 * marked hook points ("QUADRILLE (e)"), each behind its feature's test: with every new value at its default the voice
 * runs exactly ChoralRoot's code (test_quad_bitexact). Integer, as the voice is.
 *
 *   E1  AHD: the amp envelope's attack, a HOLD (a time on the amp attack's table, or NOTE: while the key is down), a
 *       decay to 0 (then the voice ends); SUS and REL unread, the gate unread in a timed HOLD (one-shot)
 *       DRIVE before the filter (QD_QP_DRPRE): ChoralRoot's DIST (qd_quad_dist) on the voice before its multimode
 *       filter, its state the voice's (qd_quad_voice_t.dpre); the mix's after the voice then skips it
 *       portamento: the glide's RATE table (quad_voice.c glides pitch_cur toward pitch16 once a block) */

/* E1: RATE's step a block (1/16 semitone) by PTIM: ChoralRoot's (voice.c, its slide: ENV_LIN[v] >> 14, at least 1;
 * tools/gen_tables.py ENV_LIN = 2^24 CTL / (ms(v) FS / 1000), ms(v) = 10000^(v / 127)) */
static const uint16_t QE_GLRATE[128] = {
    743, 691, 642, 597, 555, 517, 480, 447, 415, 386, 359, 334, 311, 289, 269, 250, 232, 216, 201, 187, 174, 162, 150,
    140, 130, 121, 112, 104, 97, 90, 84, 78, 72, 67, 63, 58, 54, 50, 47, 43, 40, 37, 35, 32, 30, 28, 26, 24, 22, 21, 19,
    18, 17, 15, 14, 13, 12, 11, 11, 10, 9, 8, 8, 7, 7, 6, 6, 5, 5, 4, 4, 4, 4, 3, 3, 3, 3, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1};

/* E1, hook 1: the amp envelope in AHD (stage 2 the attack, 6 the hold, 7 the decay; ADSR's stages met at a switch
 * mid-note: its decay / sustain is the hold, its release the decay). The hold's progress is dly[3] (the amp has no
 * DELAY: quad_note_on starts it with none). Q15, as quad_env_tick */
static int32_t quad_ext_ahd(quad_voice_t *s, int32_t atk, int32_t hold, int32_t dec, int gate)
{
    int32_t x = s->env[3];
    if (s->stage[3] == 3u || s->stage[3] == 1u) {
        s->stage[3] = s->stage[3] == 1u ? 2u : 6u;
        s->dly[3] = 0;
    } else if (s->stage[3] == 4u || s->stage[3] == 5u) {
        s->stage[3] = 7u;
    }
    switch (s->stage[3]) {
    case 2:                                     /* ATK: linear to 1 on the amp attack's table, as ADSR's */
        x += (int32_t)QUAD_T_AATK[atk & 127];
        if (x >= (1 << 24)) {
            x = 1 << 24;
            s->stage[3] = 6;
            s->dly[3] = 0;
        }
        break;
    case 6:                                     /* HOLD: NOTE until the key is up; else its time, the gate unread */
        if (hold >= 127) {
            if (!gate)
                s->stage[3] = 7;
        } else {
            s->dly[3] += QUAD_T_AATK[hold & 127];
            if (s->dly[3] >= (1u << 24))
                s->stage[3] = 7;
        }
        break;
    case 7:                                     /* DEC: exponential to 0 on the amp decay's table (127 INF: held) */
        if (dec >= 127)
            break;
        x -= (int32_t)(((int64_t)x * QUAD_T_ADEC[dec & 127] + 0xFFFFFF) >> 24);
        if (x < (1 << 12)) {
            x = 0;
            s->stage[3] = 0;
        }
        break;
    default:
        x = 0;
        break;
    }
    s->env[3] = x;
    return x >> 9;
}

/* E1: back to ADSR from an AHD stage (a switch mid-note): the hold is ADSR's decay / sustain, the decay its release */
static void quad_ext_adsr_back(quad_voice_t *s)
{
    if (s->stage[3] == 6u)
        s->stage[3] = 3;
    else if (s->stage[3] == 7u)
        s->stage[3] = 4;
}

/* E1, hook 2: DRIVE before the filter: ChoralRoot's DIST (the mix's, with the LFOs' DRIVE offset) on the voice's
 * signal before the multimode filter, its state the voice's */
static void quad_ext_drive_pre(track_t *t, voice_t *v, int32_t *b, uint32_t n)
{
    int32_t dr = v->patch[QD_QP_DRIVE];
    if (t->quad_fx_off[0])
        dr = clamp(dr + t->quad_fx_off[0], 0, 127);
    qd_quad_dist((qd_dist_t *)(void *)&v->dpre, dr, b, (int)n);
}
