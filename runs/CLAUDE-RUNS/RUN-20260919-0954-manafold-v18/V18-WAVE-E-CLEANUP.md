# Manafold version 18 — Wave E: live presentation cleanup

**Date:** 2026-09-19
**Worker:** Claude (sole Opus worker; the coordinator ran local Qwen prep, `V18-WAVE-EF-PREP.md`, which I re-verified before acting on it)
**Scope:** Wave E only. Wave F (Flight, Trick) was not started.
**Source commit:** Zhaozhou `0381bdec` on `manafold-v18`.
**Verdict:** **CLOSED.**
- The live history mist is gone from all 22 live subjects, and a renderer assertion plus a committed gate with two fired controls now enforce it.
- Crackle ships normal mana (candidate 9, day sky), with an exact legacy control.
- Drift and Blown keep candidate 9, since no new fault was exposed.
- The bounded particle attempt shipped as a small, clean, exactly switchable fade.
- The matrix passes 97/97, and every picture decision was taken by looking.

## 1. History mist: retired from every live subject

**Cause (confirmed by picture, L4).** `subject_u02_clip` gave every non-slot-7 clip `s.u02_mist = true`, the 48×30 creature-following persistent plane. On a travelling clip it leaves chunky grey-violet history cells behind the animal. Baseline vs Wave E at the most-changed frames (Hasty f149/f187, Drift f98/f209, Blown f164/f194): every "before" frame has the blocky trail, and every "after" frame is clean, with creature, ink contour, fold, lightning and motes intact. That plane **was** the Hasty smear and the Drift/Blown relic trail. The contour shell (`u02_shell`) is a separate flag and is untouched.

**Inheritors of the old default (enumerated from `zhao_reel.cpp`):**

| Group | Subjects | Wave E behaviour |
|---|---|---|
| Live site bank (22) | hover, inspect, channel, trick, damage, hasty, flight, fall, hit, taunt, taunt2, death-drop, death-gutter, lasso, blown, taunt3, drift, curious, startle, rest, pirouette, crackle | **mist off** (the new builder default) |
| Archived v17 mana menu (6) and mana lab (10), trio | `manafold-mana-*`, `manalab-*`, `manafold-trio` | keep `u02_mist = true` **explicitly** at the call site |
| `manafold-crackle-legacy` (new control) | — | explicit mist on (legacy bytes) |
| Already explicit before Wave E | still (slot 7), antenna-fixed, antenna-quarter, nodule-solo, ordering views, fogprobe-mana/off (off); fogprobe-mist and mist variants (on) | unchanged |

Note on the prep's "diagnostics" list: `manafold-inspect` is a **live** site subject (it is in `creatures.json`), so it loses the mist like every live clip.

**Mechanism:**
- `ZHAO_U02_LIVE_MIST=off|legacy` is strict. `legacy` restores the v17 builder default as the positive control.
- `kU02LiveSiteSubjects[22]` sits beside a `static_assert(22)`.
- `CreatureReelCtx` gains **executed-block receipts** (`u02_mist_frames_run`, `u02_smear_frames_run`), incremented *inside* the plane blocks. After the frames are rendered, `render_scene` compares the declared flags **and** the receipts on every live subject. It prints a `live-history:` line and returns **RC 5** on any history. The two operands come from different paths, so a plane re-enabled by some route other than the flag is still caught.
- The committed `tools/reel/manafold_live_history_gate.py` checks the renderer table against `Upheaval/website/creatures.json` (LIST), then renders all 22 in the production invocation (HISTORY).

**Receipts (`V18-WAVE-E-RECEIPTS/`):**
- **Normal:** LIST OK 22; renderer RC 0; 22/22 `declared 0/0 executed 0/0 OK`.
- **Legacy control:** RC 5; 22/22 FAIL with executed mist frames > 0 (for example Hover 600, Death Drop 227); LIST stays green. **21/22 sequence CRCs equal the Wave-D bank exactly.** The 22nd is Crackle, which is the intended mana change.
- **List-drift control:** LIST fires (`site-not-in-renderer ['manafold-hover']`); HISTORY does not run.
- **Non-live identity:** 13/13 non-live subjects byte-identical, baseline vs Wave E (fade off). The 13 are still, antenna-fixed, antenna-quarter, nodule-solo, ordering-fixed, fogprobe-mana/off/mist, mist-parked, mana-aqua, mana-stack, manalab-control-channel and trio.
- **Corroboration:** `fogprobe-mana` (smear and mist off) is now byte-identical to live Rest (`0xD25EF330`).

## 2. Crackle: normal mana on the day presentation

Crackle was the only live subject still on an old mana: candidate 4 (lightning only) under the night backdrop. It now takes the builder's candidate 9 with no `u02_backdrop` call, so it sits under the day sky. That ends the deliberate Crackle/Channel backdrop pairing; Channel keeps its night backdrop, and the comment says so. `manafold-crackle-legacy` is the verbatim old block and is **not** live.

- **Exactness:** `manafold-crackle-legacy` = **`0xEDDC80D7`** = Wave-D `manafold-crackle` (600 frames).
- **Found during closure:** the new mote fade (§4) defaults on and had reached the legacy control's surge motes (`0x8D4A8F80`). The legacy block now switches the fade off around its own render, and after the rebuild it is exact again.
- **Looked at (L8, 2×, f0/177/300/478):** legacy is a thin dotted sparkle string inside the loop on a violet night sky with a pale disc. It barely reads, and it is the relic the owner named. New is the bank's normal fold and lightning figures (spiral, cross, knot) with a mote cloud on the ordinary day sky. **The pictures agree: ship 9/day.**
- **Every frame (L9):** 600 continuous frames, figures present in every one, and the wrap matches.

## 3. Drift and Blown after history removal

- **Drift (L6, 300 frames):** a continuous right-to-left traverse on candidate 9 with no trail. The relic read is gone and no mana fault remains. **Candidate 9 kept.** Open and pre-existing (Wave E changed no animation or camera): f260–298 sit at the extreme left edge with the antenna and mana partly cropped, and f299→f0 is the traverse restart.
- **Blown (L7, 292 frames):** a continuous phrase of launch, inverted tumble, drop, catch and settle, with f291 matching f0. The mana rides the antenna with no trail. **Candidate 9 kept.**

## 4. Particles through the antenna: bounded attempt, shipped small

**Pre-layer route (plan's A/B): declined by construction, as the prep predicted.** Pre splats draw while the depth buffer holds only terrain, and then the creature composites over them (`creature_hook`), so every mote inside the silhouette is hidden, including motes genuinely in front. That flattens depth, the plan's own stop condition. `glow_splat` is confirmed to depth-test per pixel at the centre depth and never to write depth.

**Narrow candidate taken instead, using existing depth (no centreline model, collision or repulsion).** The "through" read is the one-frame flip that happens when a flat mote disc's *centre* crosses a stick's front surface. Antenna pixels come from depth the renderer already has: creature-covered (`depth != pre_depth`) but not body-covered (`u02_body_cover`). For fold and surge mote bodies only (`ManaSplat::surface_fade`), visibility ramps to zero with a smoothstep as the centre's view depth approaches that surface, within `kMoteSurfaceFadeMm`.

- **First cut rejected by reasoning:** a centre-pixel read would drop a mote in *one* frame as it slides sideways onto a stick at matching depth. The shipped form averages over the disc footprint instead (`kMoteSurfaceFadeTaps` = 5×5).
- **Fade operands:** additive halo by gain; soft body by opacity; hard-opaque surge core drawn soft at opacity f (its gain would darken, not clear).
- **Knobs:** `kMoteSurfaceFadeDefault = true`, `kMoteSurfaceFadeMm = 120`, `ZHAO_U02_MOTE_SURFACE_FADE=off|on`, `ZHAO_U02_MOTE_SURFACE_FADE_MM=1..2000`, all strict.
- **Exact-off:** `off` equals the no-fade binary on 23/23 live+legacy subjects and 13/13 non-live subjects.
- **Scope:** mana-bearing non-live diagnostics (fogprobe-mana, mist-parked, trio, mana-aqua/stack, manalab) now render under the fade. Archived media are stored bytes and are not re-rendered.
- **Cost:** 25 depth reads per mote splat.

**By-eye selection:**
- **L2:** a first-cut ladder of off/60/120/240 on Channel, Hover and Taunt III, 4× worst-changed crops.
- **L3:** a footprint strip of 8 consecutive frames. Motes that sat on the Hover right stick dim gradually with no blink, and the open-O field is untouched.
- **L10:** 4× on Death Drop, Death Gutter, Lasso and Channel. Stick-sitting motes are thinned, motes truly in front still overlap, and there are no dark discs or holes.
- **L14:** native plate. The change is subtle: a light-touch cleanup with the cloud preserved.
- **Choice:** 240 began fading motes genuinely in front, so **120 mm** is the shipped value. This stays within the owner's "don't spend much effort" budget.

## 5. Gate matrix: 97/97 (`V18-WAVE-E-RECEIPTS/gate-matrix.txt`)

**Build.** Direct clean build (`build-direct.sh --output .tmp/v18-waveE-final --clean`, g++ 16.1.0). All binaries postdate the last source edit. The renderer was rebuilt at 18:09:31 after the legacy opt-out; the gate binaries do not compile `zhao_reel.cpp`. No CMake result is claimed.

- **Normals, 11/11 RC 0:** mspan, msmooth, mprobe, mjointpub, mqa, mmeshcheck (**CLEAN**, 1416 groups, 4200 edges), moutline, mshell, mshell `--selftest`, mnodule, meyesize.
- **mspan, 35/35 RC 1:** every attribution mask is **identical to Wave D**.
- **msmooth, 16/16 RC 1:** attributed in the same categories as Wave D, with mote-visibility firing only `0x80000`.
- **Protected legs, 27/27:** each mprobe control fails only its own lines (support ownership only; support depth only; mirror only 5d; outline and scale only 5c).
- **New Wave E rows, 8/8:** live-history normal, legacy control and list-drift control; crackle-legacy identity; fade exact-off; fade fires; non-live identity; 6/6 malformed selectors RC 2.

| Binary | MD5 |
|---|---|
| `zhao-reel-cel.exe` (production) | `ae7f03a5c32357d891e7915d758d316f` (SHA-256 `4b789bf1fb2a0fb92b688f11596b3f71b70fe60c944c084e5a37a17a22ecd8b8`) |
| gates | `V18-WAVE-E-RECEIPTS/binaries.txt` |

Final bank CRCs (production invocation, `ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross`): `crc-final-live22-plus-crackle-legacy.txt`. Examples: Hasty `0xC6FB59FB`, Drift `0x3A11AB0C`, Blown `0x840DA184`, Crackle `0xCC1BDA8B`, Channel `0xD49861D0`, Hover `0x24B3FE60`.

## 6. Pictures

Committed every-frame sheets from the final bank (`V18-WAVE-E-MANAFOLD_*-ALLFRAMES.png`):
- Hasty, Drift, Blown, Crackle and Crackle-legacy;
- the particle witnesses Channel, Hover, Taunt III, Death Drop, Death Gutter and Lasso.

I looked at 14 downscaled or cropped images in total (L1–L14 in `look-notes.md`, written after each look). Hover, Taunt III and Death Gutter were judged through the worst-changed 4× crops, consecutive-frame strips and the native plate rather than their full sheets. Their only changes from the accepted Wave-D bank are the history removal and the fade.

## 7. Open (none blocks Wave E)

1. **Drift framing (pre-existing):** f260–298 at the extreme left edge, with the antenna and mana partly cropped. This is a camera/owner question for Wave F or the final bank review.
2. **Fade coverage is a render-side operand no gate traces.** msmooth traces mote visibility inside the fx, not the renderer fade. It is continuous by construction (a smoothstep over a 25-tap footprint mean) and was judged by eye across consecutive frames. If it is ever retuned, re-look rather than trust the gates.
3. **Carried from Wave D:** the Trick crown framing and the End-swell stub, both Wave F.

## 8. Files

- **Source (`0381bdec`):** `tools/reel/zhao_reel.cpp`, `tools/reel/manafold_fx.h`, `tools/reel/manafold_live_history_gate.py`.
- **Evidence:** this report, `V18-WAVE-E-RECEIPTS/`, eleven `V18-WAVE-E-MANAFOLD_*-ALLFRAMES.png`, `TASK_LOG.md`, and the coordinator's `V18-WAVE-EF-PREP.md` and `qwen/` (unedited, committed at the coordinator's request).
- **Not committed:** `.tmp/`, raw `.rgb` frames and the viewing JPEGs.
