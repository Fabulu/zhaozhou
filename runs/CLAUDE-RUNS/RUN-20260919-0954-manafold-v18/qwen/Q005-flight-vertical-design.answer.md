# Q005 answer — flight-vertical-design

- model: qwen38-quasar-dflash2-k8v4-112k  dispatch: b060eb6b92114df89e70eb18a93ffd86
- when: 2026-09-19T17:33:32  seconds: 120  finish: stop  status: ok
- usage: {"completion_tokens": 9551, "completion_tokens_details": {"reasoning_tokens": 5146}, "prompt_tokens": 4531, "prompt_tokens_details": {"cached_tokens": 0}, "total_tokens": 14082}
- inputs: [{"input": "tools/reel/manafold_art.h:2755-2800", "sha256": "170c2e03b89fd982", "chars": 3603}, {"input": "tools/reel/manafold_clips.h:4826-4910", "sha256": "127e1f38e124e087", "chars": 5305}, {"input": "tools/reel/manafold_clips.h:94-120", "sha256": "0100eed80bb8d709", "chars": 1360}]
- kind: task

## Answer

# Flight Vertical Design – Ladder Preparation

## 1. Current motion, key by key

**Loop structure.** `build_flight()` (manafold_clips.h:4826) iterates `f` over `[0, 175]` with `K = kFlightKeys = 176` (manafold_art.h:2763). The single clock is

```
cyc = K / kFlightBobPeriodKeys = 176 / 44 = 4   (manafold_clips.h:4841)
```

and every periodic channel calls `sinp(f, K, cyc, phase16)` (manafold_clips.h:108–112), which computes

```
angle = (f · cyc · 65536) / K + phase16   (mod 65536)
value = fx_sin(angle)                       // Q16.16
```

**Per-frame channels (all from manafold_clips.h unless noted):**

| Channel | Expression | Constant source |
|---|---|---|
| Root Y (height) | `fxu(kHoverHeightMm) + (fxu(kFlightBobAmpMm)·bob) >> 16` (4900–4902) | `kFlightBobAmpMm = 300` (art:2776) |
| Root X | hardcoded `0` (4897) | `kFlightSpeedMmPerKey = 25` (art:2771) is **dead** – not referenced in the loop |
| Root Z | **not shown** being written explicitly after `g.write`; retains whatever `clip_shell`/`g.write` set | not shown |
| Pitch (Z-quat) | `(kFlightPitchA16 · rise) >> 16 − kFlightPitchLeanA16` (4851–4855) | `kFlightPitchA16 = 1800`, `kFlightPitchLeanA16 = 500` (art:2780–2781) |
| Bank (X-quat) | `(kFlightBankA16 · sinp(f,K,cyc,0x2000)) >> 16` (4858–4861) | `kFlightBankA16 = 700` (art:2784) |
| Breath (deform) | `compress_at(f, K, cyc, breath, kFlightBreathPhase16)` (4906–4907) | `kFlightBreathGainPm = 1300`, `kFlightBreathPhase16 = 0x8000` (art:2787–2788); `breath = kCompressAmpPm·1300/1000` (4842) |
| Nodule trail Y | `−(kFlightTrailMm[i] · sinp(f,K,cyc, −kFlightTrailLag16[i])) >> 16` (4869–4879) | `{46,74,92}` mm, lags `{0x1000,0x1c00,0x2800}` (art:2794–2795) |
| Nodule trail lateral | `az −= ty[0]/3; cz += ty[2]/3` (4882–4883) | same constants |
| Sway | inside `loop_alive(…, kFlightSwayPm, …)` (4886) | `kFlightSwayPm = 60` (art:2797) |
| Gaze lift | `kGazeLiftMaxA16/2 · rise` (4889–4891) | not shown as a flight-specific constant |
| Antenna schedule | `antenna_knead(g, …, K, f)` (4845) | schedule internals not shown |

`rise = sinp(f, K, cyc, 0x4000)` (4848) is the analytic derivative of `bob` (phase +π/2), so pitch and gaze-lift both peak at mid-crossing (zero height) and are zero at the extremes.

**Loop seam (key 175 → key 0).** For every channel that uses `sinp(f, K, cyc, φ)`:

```
sinp(0)     → angle = 0·cyc·65536/176 + φ = φ
sinp(176)   → angle = 176·cyc·65536/176 + φ = cyc·65536 + φ
```

Since `cyc` is an integer, `cyc·65536` is an exact multiple of the 16-bit angle wrap, so `sinp(176) ≡ sinp(0)` for **every** phase. Root Y is a linear function of `bob`; pitch, bank, trail, and breath are all linear or affine functions of `sinp` outputs with integer `cyc`. The seam is therefore exact by construction for every periodic channel.

Two caveats:
- `antenna_knead` (4845) fills from a schedule whose internals are **not shown**; its seam depends on that schedule being K-periodic.
- `blink_at(f, 53)` (4892) is a one-shot discrete event, not a periodic channel; it does not affect the seam.
- `c.wrap_root_delta = true` (4837) was added because the clip previously traversed in X; with X now constant at 0 (4897), the wrap flag is inert but harmless.

---

## 2. Why four 300 mm bobs read as "bounce"

The reading comes from the **shape** of the waveform and the **coupling** of channels, all visible in the code:

1. **Pure symmetric sine.** `bob = sinp(f, K, cyc)` (4847) is an undistorted sine: rise and fall each occupy exactly K/2 keys (22 keys ≈ 0.74 s at the clip's rate). A flying or hovering creature would spend more time near the apex (glide/hang) and pass through the nadir more quickly. The code has no mechanism to skew the waveform; the rise and fall are mirror images. The author's own comment confirms the intent: "a bouncing thing squashes where it turns around" (manafold_art.h:2757–2758).

2. **Pitch locked to the derivative.** `rise = sinp(…, 0x4000)` (4848) feeds pitch directly (4851–4855). The nose pitches **up** while the body is rising through the centre and **down** while it is falling through the centre, and is level (lean-only) at both extremes. This is the exact kinematics of a ball tipping forward at the top of its arc. A gliding or flapping creature would hold a more constant nose-up attitude through the climb and transition to a shallower descent, not a sinusoidal wobble centred on zero.

3. **Breath phase at the bottom.** `kFlightBreathPhase16 = 0x8000` (π) means maximum compression occurs at the **nadir** (art:2788, 4903–4905). This is the classic squash-and-stretch of a bounce: the body is shortest where the velocity reverses at the bottom. For flight, the breath would more naturally phase with the wing-beat or the climb, not the lowest point.

4. **Four cycles in ~5.9 s.** `cyc = 4` (4841) gives a period of 44 keys ≈ 1.48 s. At 300 mm amplitude the peak-to-peak displacement is 600 mm with peak vertical velocity ≈ 300·2π/1.48 ≈ 1.27 m/s. Four such bounces in the clip is a repetitive, metronomic rhythm that the eye parses as "bouncing" rather than "flying."

5. **No horizontal traverse.** X is pinned to 0 (4897) and `kFlightSpeedMmPerKey` is unused (art:2771). Without any forward motion to offset the vertical oscillation, the pure up-down sine is all the viewer sees, which reinforces the bounce reading.

---

## 3. Proposed named knobs for the ladders

The file uses **pure `constexpr` constants with no env-override hook** (all lines 2763–2797 are `constexpr int` / `constexpr int32_t`; no `getenv`, no config-read pattern visible). So the knobs should follow the same `constexpr` pattern.

**Amplitude ladder (replace or parametrize `kFlightBobAmpMm`, art:2776):**

```cpp
constexpr int32_t kFlightBobAmpMm = 300;   // existing name, value varies per rung
```

A single constant is sufficient; the ladder is the *set of values tried*, not a new mechanism. No env-override needed (pattern not present in file).

**Integer-cycle ladder (replace `kFlightBobPeriodKeys`, art:2772):**

The current code derives `cyc = K / kFlightBobPeriodKeys` (clips:4841). Since `sinp` already takes an integer cycle count (clips:108), the cleaner knob is:

```cpp
constexpr int kFlightCyclesPerLoop = 4;   // replaces kFlightBobPeriodKeys
// and in build_flight():
//   const int cyc = kFlightCyclesPerLoop;
```

Any integer ≥ 1 works because `sinp(0) ≡ sinp(K)` for all integer `cyc` (shown above). `kFlightBobPeriodKeys` can be removed or kept as a derived comment.

**Shape knobs (new constants, same `constexpr` pattern):**

| Knob | Purpose | Suggested name |
|---|---|---|
| Waveform skew: fraction of the period the rise occupies (1/2 = symmetric sine; < 1/2 = faster rise, longer fall) | Make the climb and descent asymmetric so it reads as a powered climb + glide | `kFlightWaveRiseFrac16` (Q16, e.g. 0x6000 ≈ 0.75 means 75 % rise / 25 % fall) |
| Pitch lead: phase offset of pitch relative to the derivative (0 = current; positive = nose leads the climb) | Decouple pitch from the derivative so the nose stays up through the top instead of snapping level | `kFlightPitchLead16` |
| Top-hang: an additional cosine or clamp that slows the body near the apex | Gives a "glide at the top, snap through the bottom" feel | `kFlightTopHang16` (0 = off) |

Each knob, when set to its "neutral" value (riseFrac = 0x8000, pitchLead = 0, topHang = 0), must reproduce the current sine exactly so the ladder is a superset.

**No env-override hook** is added because the file does not use that pattern.

---

## 4. Ladder rungs and framing risk

Framing constant `kU02CamKFlight = 250000` is named in the brief but **not shown in the provided input**. The frustum cannot be computed from the given material. All framing claims below are "must verify on render."

### Amplitude ladder (cyc = 4 held fixed)

| Rung | `kFlightBobAmpMm` | Framing risk to check on render |
|---|---|---|
| A1 | 100 | Low. Creature bobs ±100 mm around `kHoverHeightMm`. Verify the bob is still *visible* (not sub-pixel) at `kU02CamKFlight` distance. |
| A2 | 200 | Moderate. Verify top of the bob does not clip the top of the frame and the bottom does not clip the floor/ground plane. |
| A3 | 300 (current) | Baseline. Verify current framing headroom. |
| A4 | 500 | High. 500 mm above `kHoverHeightMm` may exit the top of the frame; 500 mm below may hit the ground or exit the bottom. Must check both extremes on render. |

### Integer-cycle ladder (amplitude held at 300)

| Rung | `kFlightCyclesPerLoop` | Framing / visual risk to check on render |
|---|---|---|
| C1 | 1 | Very slow (~5.9 s per undulation). Check that the motion is still *perceptible* as flight rather than a slow drift; check for smear at the fast mid-crossing (peak velocity = 300·2π/5.9 ≈ 319 mm/s – low smear risk). |
| C2 | 2 | ~3 s per undulation. Moderate. Check smear at mid-crossing. |
| C3 | 4 (current) | ~1.5 s per undulation. Baseline. |
| C4 | 6 | ~1 s per undulation. Higher perceived speed. Check for **speed-smear** at the fast mid-crossings (peak velocity ≈ 300·2π/1.0 ≈ 1.9 m/s); check that the loop still reads as one coherent gesture and not a mechanical pump. |

**Cross-product note:** the brief says "render independent AMPLITUDE and INTEGER-CYCLE ladders, then choose by eye." This means each rung is rendered with the *other* knob held at its current value, not as a full Cartesian grid.

---

## 5. Gates that must stay green

| Gate | Why it matters | Coverage in the shown input |
|---|---|---|
| **Loop seam (key K−1 → key 0)** for every periodic channel | A visible jump at the loop point breaks the clip | Structural: `sinp` with integer `cyc` makes `sinp(0) ≡ sinp(K)` (clips:108–112). `wrap_root_delta = true` (clips:4837) handles root sub-frame blending. **Not shown:** an executable seam-check (the `wrapseam.py` referenced at clips:4836 is not in the input). |
| **C² continuity of root Y, pitch, bank, breath, nodule trail** | A kink or velocity jump causes a visible pop | Structural: every channel is an affine function of `sinp` (C∞) or of `compress_at`/`loop_alive` which take `cyc` and presumably call `sinp` internally. **Not shown:** the bodies of `compress_at`, `loop_alive`, `antenna_knead`, or `front_flex_play`, so their C² is not verifiable from the given lines. |
| **No smear** | Fast mid-crossing at high cyc/amp can paint ghost pixels | The R2 comment (clips:4829–4836) describes the *removed* horizontal-traverse smear and the `drift` metric (near-grey pixel count 295→729→59). The current vertical-only bob with X=0 (4897) should not re-introduce that, but **no executable smear check is shown** in the input. |
| **Framing (creature stays in camera)** | Amplitude or cycle changes can push the creature out of frame | `kU02CamKFlight = 250000` is named in the brief but **not shown in the provided code**. No frustum check is visible. Must be verified on render. |

**Not shown / not verifiable from the input:** the bodies of `compress_at`, `loop_alive`, `antenna_knead`, `front_flex_play`, `clip_shell`, `blink_at`, `apply_gaze`, `apply_squint`, `face_rest`; the value of `kHoverHeightMm`; the value of `kCompressAmpPm`; `kU02CamKFlight`; any executable seam, C², smear, or framing check.

---

## CONTINUATION
- **Done:** (1) Traced every per-frame channel in `build_flight()` (clips:4843–4908) to its named constant (art:2763–2797); confirmed `kFlightSpeedMmPerKey` (art:2771) is dead and Root X is hard-zero (clips:4897). (2) Proved the loop seam is exact for all `sinp`-driven channels because `cyc` is integer (clips:108–112, 4841). (3) Identified the four structural reasons the current sine reads as bounce: symmetric waveform, pitch = derivative, breath at nadir, 4 cycles / 5.9 s (clips:4847–4848, 4851–4855; art:2788). (4) Proposed `kFlightCyclesPerLoop` to replace `kFlightBobPeriodKeys`, kept `kFlightBobAmpMm` as the amplitude knob, and named three shape knobs (`kFlightWaveRiseFrac16`, `kFlightPitchLead16`, `kFlightTopHang16`); confirmed no env-override pattern exists in the file. (5) Laid out 4×4 ladder rungs with render-check framing risks; noted `kU02CamKFlight` and all helper-function bodies are not in the input.
- **Open:** (a) Root Z is never explicitly written in the shown loop – its value is whatever `clip_shell`/`g.write` sets; need to confirm it is constant across keys. (b) `antenna_knead`, `compress_at`, `loop_alive`, `front_flex_play` bodies are not shown; their C² and seam behaviour are unverified. (c) No executable seam, C², smear, or framing check code is in the input; `wrapseam.py` is referenced (clips:4836) but not provided. (d) `kHoverHeightMm` and `kCompressAmpPm` values are not shown, so absolute Y range cannot be computed. (e) How the waveform-skew knob (`kFlightWaveRiseFrac16`) would be implemented (e.g. a piecewise phase remap before `sinp`) is not designed yet.
- **Next chunk:** Read the bodies of `compress_at`, `loop_alive`, `antenna_knead`, and `front_flex_play` (search in manafold_clips.h / manafold_art.h) to confirm they all use the same integer-`cyc` `sinp` clock, verify C² at the seam, and locate any existing executable seam/smear/framing checks. Also read `clip_shell` and `g.write` to confirm Root Z is constant. Inputs needed: the full source of those four functions, `clip_shell`, `kHoverHeightMm`, `kCompressAmpPm`, and `kU02CamKFlight` definition.

## Coordinator verdict

**verified** — spot 5/5: cyc derivation, rise=derivative phase 0x4000, X hard-zero, kFlightSpeedMmPerKey dead, breath squash at bottom 0x8000 all confirmed; bounce diagnosis (symmetric sine + pitch-on-derivative + nadir squash + no traverse) is sound and actionable. Honest 'not shown' flags.
