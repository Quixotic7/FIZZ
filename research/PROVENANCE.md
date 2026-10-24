# Where FIZZ's code comes from (L1, 2026-10-23)

**Why this audit:** the owner moved FIZZ from GPL-3.0-only to **MPL-2.0** (PLAN.md §13, "The base: fm1-base"). His
rule was that Felucca code is rewritten, not relicensed: Felucca is Leo Kuroshita's (@kurogedelic, Hügelton
Instruments, GPL-3.0-only). This file records, per file and (in mixed files) per function, where FIZZ's firmware
code and the data it generates came from.

**Method:**
- **Felucca:** each file was compared with Felucca 1.0 (`hugelton/Felucca` 727f272, 2026-10-05). That is the revision
  ChoralRoot forked from: its fork commit c1c3cec matches 727f272 in every shared file we checked, and does not match
  0.9-beta. Felucca 1.0.1 and 1.0.5.1 / 1.1.5 were checked too, for later ports. The comparison used a token screen
  per function (`funcsim.py`), then each candidate was read.
- **ChoralRoot:** its history (`../ChoralRootFM1`, `git log --follow -S -L`) shows who brought each function in.
- **SLOOP:** `isod89/sloop-fm1` was compared by reading. Its chorus and limiter are credited by ChoralRoot as designs.
- **Melodee:** Kerem Kilic's code was traced through ChoralRoot's fde3745.
- The three audit reports and their scratch work are summarised here. Authorship was taken from git; every
  ChoralRoot commit except nine is Michael P Jones's (the owner's).

## The result in one table

| Area | Origin | Licence now |
|---|---|---|
| `core/app`, `core/seq`, `core/ui`, `core/gfx` (except `fonts_gen.h`), `core/store`, `core/glue`, `core/qd_config.h`, `core/tests` (except `tests/ref/cr_*_ref.c`), `tools/` | FIZZ's own, written for the owner (FMToneSeq: 252 commits, all by the owner) | MPL-2.0 |
| `core/voice/pool.c`, `core/voice/quad/quad_ext.c`, `qd_fx.c/.h`, `quad_voice.h` | FIZZ's own | MPL-2.0 |
| `core/voice/quad/eng_quad.c`, `quad_tables.h`, `quad_times.h`; `core/voice/fx/cr_fx.c`, `cr_ram.h`, `fz_out.c` (the Clean chain); `core/tests/quad_goldens/` | **The owner's ChoralRoot code** (ChoralRoot v0.25.1). Felucca-derived parts are rewritten, see below | MPL-2.0 (the owner's relicence) |
| `core/voice/quad/fz_*.h`, `tools/gen_fz_tables.py`, `core/tests/test_fz_dsp.c` | **New in L1**, written clean-room from `research/L1-CLEANROOM-SPEC.md` | MPL-2.0 |
| ChoralRoot's `firmware/src/cr_cpu1.h` (the second core's start and jobs), for the port's dual core (P2.6, `firmware/DUALCORE.md`) | **The owner's ChoralRoot code**: he relicensed it MPL-2.0 for FIZZ on 2026-10-23. Code reused from it carries a provenance line | MPL-2.0 (the owner's relicence) |
| `core/tests/ref/cr_quad_ref.c`, `cr_fx_ref.c` | Test oracles that compile ChoralRoot's own tree, Felucca's code included (`tools/build/cr-v0.25.1`, not in FIZZ) | GPL-3.0-only (development only; never in the firmware) |
| `third_party/plutovg` | plutovg v1.3.3 (Samuel Ugochukwu); FreeType files; stb | MIT; FTL; MIT OR Unlicense (patched by FIZZ, see `third_party/README.md`) |
| `assets/fonts`, `core/gfx/fonts_gen.h` | Inter Tight (The Inter Project Authors) | OFL-1.1 (no Reserved Font Name) |
| `site/` | The web page, with RemoteFM1 copies | GPL-3.0-only (not firmware) |

## Felucca code that was in FIZZ, and what replaced it (L1 (1), commit 2548de0)

Every output is unchanged. `test_quad_bitexact` (170 renders, 8,622,880 samples) and `test_fx_bitexact` compare
against ChoralRoot's own engine and effects, and the goldens and renders are identical.

**How the rewrite was done.** An implementer worked from `research/L1-CLEANROOM-SPEC.md`, a description of what each
part computes, and was forbidden to read Felucca, ChoralRoot or the old files. Integrating the new code into the
callers was done separately.

| Was | Felucca origin | Replaced by |
|---|---|---|
| `core/voice/quad/dsp.c` (all; verbatim Felucca 1.0 `dsp.c`, ChoralRoot never changed it) | `dsp.c`: `clamp`, `mulq15`, `sine_i`/`osc_sine`, `osc_tri`, `softclip`, `soft_knee`, `tsvf_t`/`tsvf_coef_k`, `CTL_LOG2`, `VOICE_FS` (and unused `mulq16`, `blep`, saw / pulse, `noise32`, `tsvf_coef`, `tsvf_lp`, `amp_at`, `voice_amp`, `cents_inc`) | `fz_dsp.h` (the used ones only) |
| `core/voice/quad/cr_tables.h` and `tools/cr_tables.py`: Felucca's `tools/gen_tables.py` output, byte for byte, including `pitch_inc`'s text | `gen_tables.py`: SINE, PITCH_OCT + `pitch_inc`, LFO_INC, SVF_G, LEVEL_Q12, TANH_Q15 | `tools/gen_fz_tables.py` → `fz_tables.h`, from the tables' mathematical definitions, with the same values (hash-checked) |
| `core/voice/quad/cr_core.h` (most of it) | `core.h`: `NELEM`, the `F_*` formats, `param_desc_t`, `G_*`, `V_*`, `vmod_t`, `preset_t`, `FX()`, `PAT()`, and their comments | `fz_core.h` (only what FIZZ uses, its own comments) |
| `core/voice/fx/fx.c` | `fx.c`: the header, `cho_buf`, the `fx` state, `dc_block`, `lowcut1`, `spk_bass` (1.0.5.1), the limiter and soft clip `master_out` (as `master_loop`), `fx_usb_fixed` / `MASTER_FULL`, and the Classic branch of `out_master` | `fz_out.c`: the owner's Clean chain only, with the DC blocker as `fz_dc_step`. The Classic master, LOWCUT / BASS+, the USB staging and the fade between chains were dead code in FIZZ and are removed |
| `core/voice/fx/master.c` | `master_of_pot` (Felucca 1.1.5 `fx.c`; the square law is in 1.0 `main.c`) | `fz_master_of_pot` |
| `core/voice/fx/cr_fx.c` `xc_block` (the chorus's line write, its interpolated read, the position formula: Felucca's `fx_buses` chorus, rescaled by the owner) and `xf_isqrt` (token-near-identical to Felucca's `eng_noise.c` `isqrt32`) | `fx.c` `fx_buses`; `eng_noise.c` | `fz_chorus_block`, `fz_isqrt` (`xc_block` is now a call) |
| `quad_voice.c` `qd_quad_dist` (Felucca 1.0 `fx.c` `track_dist`, line for line) | `track_dist` | `fz_drive` |
| `eng_quad.c` `quad_env_tick` (the stage skeleton of Felucca's `voice.c` `env_tick`; the owner's extensions on it) and `quad_ext.c` `quad_ext_ahd` (FIZZ's, modelled on it) | `voice.c` `env_tick` | `fz_env_step`, `fz_ahd_step` |
| `eng_quad.c` `quad_svf_coef` (a finer `tsvf_coef_k`), the `QUAD_SVF` loop and LP24 pre-stage, and `quad_ext.c`'s machine loop (Felucca's trapezoidal SVF tick as written in `eng_noise.c`) | `dsp.c` `tsvf_coef_k`, `tsvf_lp`; `eng_noise.c` | `fz_svf_coef_fine`, `fz_svf_tick` |
| `quad_voice.c` glide (TIME: `voice.c` `glide_set`; the step: `track_render`) and `quad_ext.c` `QE_GLRATE` (Felucca's `ENV_LIN >> 14`) | `voice.c` | `fz_glide_time_step`, `fz_glide_move`; QE_GLRATE generated in `fz_tables.h` |
| `quad_voice.c` the block driver (from the owner's `cr_quad_test.c` `drv_tick`, which mirrored `voice.c` `track_render`'s protocol) | `voice.c` | rewritten as FIZZ's own `qd_quad_voice_render` |
| `mix.c` `mix_loop` and the level, pan and send lines (Felucca `fx.c` `mix_part`'s per-sample loop, `LEVEL_Q12`, the pan law, `·258`, the overflow guard) | `fx.c` `mix_part` | `fz_mix_voice`, `fz_pan_gains`, `fz_send_gain`, `fz_send_xmax` |
| `ctl_preview.c` `fnv()` (the FNV-1a step, the same body as Felucca `ui.c`) | `ui.c` | written anew (a public algorithm) |
| `tests/test_fx_bitexact.c` `noise()` (xorshift32 as Felucca's `reverb_test.c`) | test only | an LCG, both sides fed the same |

**Kept, judged not Felucca text:**
- **Published algorithms written differently:** `quad_xs` (Marsaglia xorshift32), the DX7 feedback average (`fb1`/`fb2`), FNV-1a.
- **One-line idioms:** the LFO square `ph < 2^31 ? 32767 : -32767` and the random value `(r >> 16) - 32768`. Also the phase wrap that draws a new random value.
- **Interfaces and facts:**
  - the shape of 7-field `param_desc_t` initialisers;
  - the 30 Hz – 16 kHz cutoff law, which `QE_COMB_LEN` and `QUAD_BW_K` use;
  - single constants: MASTER's default pot 724, the mix level default 104, the chorus defaults 40 / 60;
  - the FM-1's key matrix and factory panel map, through FM1VST's `panel.h`, which FIZZ's glue reads. These are hardware facts.

**Note:** fm1-base's LICENSING.md records that Leo Kuroshita gave permission to use the Felucca code he owns under
MPL-2.0 ([hugelton/felucca-bsp#4](https://github.com/hugelton/felucca-bsp/issues/4)). The owner chose the rewrite all
the same, so FIZZ does not rely on that permission.

## Other third parties

- **Melodee (Kerem Kilic, GPL-3.0, via ChoralRoot fde3745):**
  - the name `F_OFS` (a format id), and the voice mix's saturation limits ±884000 / ±524287. These two numbers are now
    inside `fz_mix_voice`, as the specification gave them.
  - `vmod_t.plog` and the names `F_FMNOTE` / `F_FMFRQ` are gone.
  - **For the owner:** the two limits are numbers, but they are Melodee's choice.
- **SLOOP 2.6 (isod89, GPL-3.0-only):**
  - Designs only: the limiter's one-block look-ahead with a linear gain, and the chorus's two taps per block.
  - A side-by-side reading of SLOOP's `fx_buses` and `lim_block` found no shared text, apart from Felucca's own chorus
    lines, which both had and which are now rewritten. Credited in LICENSING.md.
- **OMX-27 v1.15.4 (okyeron/OMX-27; no licence):**
  - `core/seq/mfx.c` and the F2 pick-up in `ctl.c` re-implement its mechanics from `research/OMX-MIDIFX.md`, a
    behaviour spec.
  - Copied facts: the RATE table {1 … 64}, the 17 arp pattern names and orders, the effect types and limits. No code
    was translated.
  - In OMX the arpeggiator, repeat, chord, randomizer, selector and notemaster are the owner's code. Harmonizer, scaler,
    transpose, chance, monophonic and the interface are mostly j__r0d's (Jarrod Kozeal). FIZZ's versions of those are a
    few lines each.
  - **For the owner:** whether to note this in the credits.
- **phonic-nonsense (Euan Goddard; MIT):**
  - `core/seq/names.c` `qd_name_phonic` uses its phonics grapheme lists and three word shapes, through the owner's
    namesizer (MIT, the owner's).
  - The owner confirmed (P2.4, 2026-10-23) that phonic-nonsense is MIT.
- **Inter Tight:** OFL-1.1 (`assets/fonts/OFL.txt`; no Reserved Font Name).
- **plutovg:** MIT. The FreeType files under it are FTL only as shipped; FTL asks that changes be stated, which
  `third_party/README.md` now does. stb is MIT OR Unlicense.
- **The ape (`core/ui/ape_poses.h`):** traced by Claude from `UI Inspiration/fizz-tap-breakdance-ape.webp`, which the
  mock-up calls the owner's reference ("tracing allowed").
  - The owner confirmed (P2.4, 2026-10-23) that the image is the owner's own artwork.
- **genkigenki (M. Geisler):**
  - Nine ChoralRoot commits: the SMK-37 port and two header commits.
  - The header commits change only line endings and an e-mail address, and the second undoes the first.
  - Nothing of theirs is in FIZZ.
- **Elektron (Digitone):** design and measured times only, no code.

## A latent fault found on the way (not changed: bit-exactness)

In `fz_send_xmax` (ChoralRoot's mix arithmetic), when send 1 or 2 ramps *down*, its old gain is not counted in the
overflow guard. A loud voice can then overflow int32 in `(xs·g) >> 15` while that send ramps. The implementer's
UBSan run caught it: `77067·29863`. The fix is to take each send's max(old, new). It would change samples only in
that overflow case.
