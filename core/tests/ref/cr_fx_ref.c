/* SPDX-License-Identifier: GPL-3.0-only */
/* The reference side of the effects' bit-exact test (core/tests/test_fx_bitexact.c): ChoralRoot's effects and output
 * chain AS THEY ARE at v0.25.1, compiled from the tag's own tree through its host build (tests/hostsim.c with CR_FX 1,
 * as its tests/cr_fx_test.c and cr_output_test.c build them: the whole fx.c, cr_fx.c, the engines around them).
 * Driven as fx.c mix_block drives them after the parts: xc_block, xf_block, xq_block, out_master. The power-on state
 * is cr_fx_test.c's fx_reset plus the chorus's and the output chain's (the chorus's rate and depth from params.c's
 * GP, Output Clean from cr_ui.c's default, MASTER at the caller's level). Nothing of QUADRILLE is included. */
#define CR_FX 1
#define main hostsim_main
#include "hostsim.c"
#undef main

#include "bitexact.h"

static void ref_reset(int bpm, uint32_t master_q12)
{
    memset(&xf, 0, sizeof xf);
    memset(&xq, 0, sizeof xq);
    memset(&xc, 0, sizeof xc);
    memset(&fx, 0, sizeof fx);
    memset(xd_buf, 0, sizeof xd_buf);
    memset(xr_buf, 0, sizeof xr_buf);
    memset(xr_pbuf, 0, sizeof xr_pbuf);
    memset(cho_buf, 0, sizeof cho_buf);
    xc_live = 0;
    lim_env = LIM_T;
    lim_g = 32768;
    dc_l = dc_r = dce_l = dce_r = 0;
    fx_off = 0;
    fx_smooth = 1;
    fx_lowcut = 0;
    fx_usb_fixed = 0;
    xf_defaults();
    xf_bpm = (uint16_t)bpm;
    song.g[G_BPM] = (int16_t)bpm;
    song.g[G_CRATE] = GP[G_CRATE].def;
    song.g[G_CDEPTH] = GP[G_CDEPTH].def;
    fx_master_now(master_q12);
    fx_output_set(1u, 1);
}

void cr_fx_ref_run(int which, const int32_t *in, int32_t *out, uint32_t n, int bpm, uint32_t master_q12)
{
    static int32_t z[CTL], s[CTL], l[CTL], r[CTL];
    uint32_t i, k;
    ref_reset(bpm, master_q12);
    for (i = 0; i + CTL <= n; i += CTL) {
        for (k = 0; k < CTL; k++) {
            z[k] = 0;
            if (which == BE_FX_CLEAN) {
                l[k] = in[2u * (i + k)];
                r[k] = in[2u * (i + k) + 1u];
            } else {
                s[k] = in[i + k];
                l[k] = r[k] = 0;
            }
        }
        if (which == BE_FX_CHORUS)
            xc_block(s, l, r, CTL);
        else if (which == BE_FX_DELAY)
            xf_block(s, z, l, r, CTL);
        else if (which == BE_FX_REVERB)
            xf_block(z, s, l, r, CTL);
        if (which == BE_FX_CLEAN) {
            out_master(out + 2u * i, l, r, (const int32_t *)0, CTL);
        } else {
            for (k = 0; k < CTL; k++) {
                out[2u * (i + k)] = l[k];
                out[2u * (i + k) + 1u] = r[k];
            }
        }
    }
}

/* the whole chain on five inputs a frame (in[5i..5i+4]: the chorus, delay and reverb sends, the dry left and right) */
void cr_fx_ref_chain(const int32_t *in, int32_t *out, uint32_t n, int bpm, uint32_t master_q12)
{
    static int32_t sc[CTL], sd[CTL], sr[CTL], l[CTL], r[CTL];
    uint32_t i, k;
    ref_reset(bpm, master_q12);
    for (i = 0; i + CTL <= n; i += CTL) {
        for (k = 0; k < CTL; k++) {
            const int32_t *f = in + 5u * (i + k);
            sc[k] = f[0], sd[k] = f[1], sr[k] = f[2], l[k] = f[3], r[k] = f[4];
        }
        xc_block(sc, l, r, CTL);
        xf_block(sd, sr, l, r, CTL);
        xq_block(l, r, CTL);
        out_master(out + 2u * i, l, r, (const int32_t *)0, CTL);
    }
}
