# Manafold Pass 17 — Death Drop effect continuity repair

**Date:** 2026-09-19
**Direction:** Owner Direction 18
**Trigger:** exact final-bank batch 02
**Focused verdict:** **PASS — the visible death effect now fades continuously**
**Pass verdict:** a new exact 28-subject bank is mandatory before review resumes

## Final-bank blocker

Renderer `3390F2B8214473FEBFA32092A98E9056` kept a large opaque navy/black folded-mana mass through Death Drop f0233 and removed it entirely at f0234, the settle key. Body, eyes, contact, antenna and corpse pose remained continuous, isolating the defect to rendered effect visibility. The exact failure is recorded in `PASS17-FINAL-BATCH-02.md` and `PASS17-FINAL-BATCH02-DEATH-DROP-F0228-0236-4X.png`.

## Root cause

`fold_life_pm()` reduced coherence, gain and mote visibility toward zero, and every existing trace therefore reported a small continuous final step. But the navy backing and particle hearts are **soft opaque** splats. `glow_splat()` derived their blend alpha only from the sprite index; lowering palette gain drove their target colour toward black without reducing opacity. The picture therefore remained a broad opaque black mass until `life_pm == 0` stopped emitting the splats.

The checker watched the value being reduced, not the independent renderer operand that stayed full. Stable identity, gain, stamp count, accumulated energy and traced visibility could all pass while actual soft-opaque alpha held at 1000 and then fell to zero. This is the missing-visible-operand failure class, not evidence that the earlier fifteen controls were weak on what they actually measured.

## Production repair

1. `ManaSplat` now carries an explicit `opacity_pm`, identity-defaulted to 1000. `glow_splat()` applies it only to soft opaque alpha; the default path is byte-exact.
2. Death Drop and Death Gutter life are evaluated on the full Q4 presentation clock through `motion_c2_ease`, rather than an integer-key linear truncation.
3. The fold's soft opaque navy backing, non-strand core and mote hearts receive the death-life opacity independently of palette gain. Additive halo/shimmer/core gains retain their existing law.
4. `ZHAO_U02_DEATH_EFFECT_CONTROL=none|hard-cut` is strictly parsed. `hard-cut` restores the exact old integer life law and full opacity until zero.
5. `FxContinuityTrace::fold_backing_opacity_pm` is the exact operand passed to production splats. `msmooth` evaluates both death clips frame by frame, repeats each final tail three times, and deliberately excludes the authored site-loop restart from the settle detector.

The old control is not an approximation: all 450 control frames are byte-identical to the rejected exact bank, with the same sequence CRC32C `0xFC405D06` and the same f0233→f0234 cutoff.

## Clean build and gates

Clean direct output: `.tmp/p17-death-fx-repair-v4`.

| Binary | MD5 | SHA-256 |
|---|---|---|
| `zhao-reel-cel.exe` | `19816A52B969C2BE65A797E3E17C1795` | `0FE963669241C3A71DD5B8BD7D233264C1E15B0D5FF90F8ADC1B8EFAD9CAEF26` |
| `manafold-motiongate.exe` | `AB3520FEAC7BD90C2227E95EE4510427` | `1F27F71F15511EA2C2C7C85A23D0F7F66524DE59B8EA1E3FCADE1E5F11B87C19` |

`msmooth` normal is RC 0. Death backing opacity maxima are:

- Death Drop: step/acceleration/jerk `11 / 2 / 3 pm`;
- Death Gutter: `10 / 3 / 6 pm`.

The committed `--fail-death-effect-cutoff` control returns attributed RC 1 with only category `0x400000`; it produces `1000 / 1000 / 2000 pm` and fires at Death Drop f0234 and Death Gutter f0224. All sixteen effect/palette controls return RC 1 with exact attributed masks; no pre-existing control regressed. Invalid, numeric, leading-space and wrong-case renderer selector values return RC 2.

Focused regression matrix:

- `mqa` normal, eyes-only, legacy death-eye and Fall source/control legs: RC 0 by their established convention;
- `mprobe` normal RC 0; scale-inverse/outline/mirror controls RC 1;
- `mspan` normal RC 0;
- `moutline` normal/no-owner/forced-repaint: `0/1/1`.

Unaffected production proof: repaired Hover is 600/600 byte-identical and Channel 420/420 byte-identical to the rejected bank. The first changed Death Drop frame is f0038, immediately after the authored float-fail beat; f0000–f0037 remain exact.

## Rendered result

One repaired binary rendered both complete deaths:

| Subject | Frames | Sequence CRC32C |
|---|---:|---|
| `manafold-death-drop` | 450 | `0x4A8CE910` |
| `manafold-death-gutter` | 590 | `0x1D892126` |
| rejected hard-cut control | 450 | `0xFC405D06` |

Every frame of both repaired deaths was reviewed. Death Drop now carries bright cyan/white/navy life through the float failure, then progressively loses light, opaque backing and particle hearts across the diminishing bounces. By the settle approach the effect is already visually negligible; there is no replacement, blackout frame or corpse resurrection. Death Gutter retains its distinct mana-first read and later body failure.

Durable accepted evidence:

- `PASS17-DEATH-DROP-EFFECT-REPAIR-ACCEPTED-ALLFRAMES.png`
- `PASS17-DEATH-GUTTER-EFFECT-REPAIR-ACCEPTED-ALLFRAMES.png`
- `PASS17-DEATH-DROP-EFFECT-FADE-ACCEPTED-2X.png`
- `PASS17-DEATH-DROP-EFFECT-HARD-CUT-CONTROL-2X.png`
- `PASS17-DEATH-DROP-EFFECT-ACCEPTED-VS-CONTROL-NATIVE.png`

Earlier `PASS17-DEATH-*-FINAL-*`, unbounded one-row grids and pre-v4 repair plates are exploratory/superseded and must remain unstaged.

## Remaining boundary

The focused source/gate/picture repair is accepted. It changes both death outputs and the renderer binary, so the `3390F2...` bank and manifest `560c1237...` are permanently historical. Build one new exact renderer, rerender all 28 subjects in one invocation, verify the new manifest, and resume isolated review only from that bank. Non-death frames are expected to remain exact, but that expectation is not a substitute for the new one-binary receipt.
