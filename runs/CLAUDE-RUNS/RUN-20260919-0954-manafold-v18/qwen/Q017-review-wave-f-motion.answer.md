# Q017 answer — review-wave-f-motion

- model: qwen38-quasar-dflash2-k8v4-112k  dispatch: 93db72793f9d46a781364e85560fb191
- when: 2026-09-19T19:19:58  seconds: 958  finish: stop  status: ok
- usage: {"completion_tokens": 72641, "completion_tokens_details": {"reasoning_tokens": 69502}, "prompt_tokens": 11994, "prompt_tokens_details": {"cached_tokens": 0}, "total_tokens": 84635}
- inputs: [{"input": "git show d7d51171 -- tools/reel/manafold_art.h", "sha256": "87c39a07566e93bb", "chars": 14580}, {"input": "git show d7d51171 -- tools/reel/manafold_clips.h", "sha256": "781bdad4e01602ac", "chars": 18517}]
- kind: task

## Answer

## FINDINGS
none found

## REVIEW

### 1. Flight seam, C2, and old-value equivalence

The Flight clock is:

- `t_f = f * cyc * 65536 / K` at `manafold_clips.h:4922`.
- `w_f = t_f + (c16 * sin(t_f)) / 65536 + (h16 * cos(t_f)) / 65536` at `manafold_clips.h:4923`-`4929`, with cosine implemented as `sin(t + 0x4000)` at `manafold_clips.h:4928`.
- Every channel is sampled through `flight_sin(w16, phase)`, which masks the summed angle to 16 bits at `manafold_clips.h:4934`.

For the seam, compare `f` and `f + K`. Because `cyc` is an integer and `K` divides `K * cyc * 65536`:

```text
t_{f+K} = t_f + cyc * 65536
```

exactly in the integer arithmetic at `manafold_clips.h:4922`. The perturbation terms depend only on `t_f & 0xFFFF` and `(t_f + 0x4000) & 0xFFFF` at `manafold_clips.h:4927`-`4928`, so adding `cyc * 65536` leaves them unchanged. Therefore:

```text
w_{f+K} = w_f + cyc * 65536
```

and `flight_sin` at `manafold_clips.h:4934` discards whole turns via the 16-bit mask. Thus height, pitch, bank, trail, and breath all have the same value at `f` and `f + K`; the loop seam is exact for every channel that reads the warped clock. The height write in the `build_flight` hunk beginning at `manafold_clips.h:4949` adds `g_u02_flight_lift_mm` and scales `g_u02_flight_amp_mm`, but the seam argument is unaffected because it uses the same `bob`.

C0/C1/C2: the warp is a single periodic harmonic, not a piecewise remap with joins:

```text
w(t) = t + c sin(t) + h cos(t)
w'(t) = 1 + c cos(t) - h sin(t)
w''(t) = -c sin(t) - h cos(t)
```

So there are no remap joins to break C0/C1/C2. For the shipping values, `kFlightRiseFrac16 = 24000` at `manafold_art.h:2819` gives `c16 = (32768 - 24000)/2 = 4384` at `manafold_clips.h:4923`, and `kFlightTopHang16 = 4000` at `manafold_art.h:2823` gives `h16 = 4000` at `manafold_clips.h:4924`. The maximum derivative perturbation is `sqrt(4384^2 + 4000^2) ≈ 5915` angle16, below the stated conservative monotone guard `kFlightWarpMaxA16 = 10000` at `manafold_art.h:2834`.

Old values: if `g_u02_flight_rise_frac16` is the v17 symmetric value `0x8000` and `g_u02_flight_top_hang16` is 0, then `c16 == 0 && h16 == 0`, so `flight_phase16` returns `t` unchanged at `manafold_clips.h:4925`. `flight_sin` is documented at `manafold_clips.h:4931`-`4932` as the exact expression `sinp()` evaluates with the warped base substituted. With old cycle count 4, old amplitude, and zero lift, the shown Flight path reduces to the old `sinp`-based path. The explicit cycle count is read at `manafold_clips.h:4959`; the v17 cycle count was 4 per `manafold_art.h:2800`. The actual `sinp` implementation is not shown, so bit-identical old bytes are established only to the extent that the provided code/comment claims that `t` and `flight_sin` duplicate `sinp`.

### 2. Trick spin C2, identity, overflow, pivot/compensation

The spin progress uses two segments of:

```text
S(x) = 10x^3 - 15x^4 + 6x^5
```

implemented in int64 at `manafold_clips.h:3224`. Analytically:

```text
S(0) = 0, S'(0) = 0, S''(0) = 0
S(1) = 1, S'(1) = 0, S''(1) = 0
```

so each segment is C2 at both ends. The first segment runs `0 -> peak` for `f < s1` at `manafold_clips.h:3231`; the second runs `peak -> final` for `f < s2` at `manafold_clips.h:3232`; after settle it holds `final` at `manafold_clips.h:3233`. The join at `s1` is exact because the second segment starts from `peak_u` at `manafold_clips.h:3216` and `manafold_clips.h:3232`.

For shipping constants:

- `kTrickSpinGainPm = 1000` at `manafold_art.h:2979`, so `final_u = 1000 * 1000 = 1,000,000` at `manafold_clips.h:3215`.
- `kTrickSpinOvershootPm = 40` at `manafold_art.h:2978`, so `peak_u = 1,040,000` at `manafold_clips.h:3216`.

`build_trick` skips the spin quaternion and support compensation when `spin_u % 1000000 != 0` is false at `manafold_clips.h:3357`. Therefore the final held value `1,000,000` is treated as identity, with no residual angle or root X/Z correction.

The yaw is applied as a world-axis pre-multiply:

```cpp
g.q[kBRoot] = quat_mul(quat_y(trick_spin_a16(spin_u)), g.q[kBRoot]);
```

at `manafold_clips.h:3360`. The support centre is sampled before the yaw with `trick_support_center_xyz(g, 1000, bx, by, bz)` at `manafold_clips.h:3359`, then after the yaw with `trick_support_center_xyz(g, 1000, ax, ay, az)` at `manafold_clips.h:3361`. The compensation is:

```cpp
root_x_um = bx - ax;
root_z_um = bz - az;
```

at `manafold_clips.h:3363`-`3364`, and written to root X/Z at `manafold_clips.h:3384` and `3386`. The comment at `manafold_clips.h:3379` states root X/Z are zero in `clip_shell` and remain zero outside the turn; with that precondition, the post-compensation world support X/Z equals the pre-spin support X/Z.

Overflow: the ease products are int64 at `manafold_clips.h:3228`. For the default segment lengths 28 and 12 and `peak_u = 1,040,000`, the largest shown int64 product is far below the int64 limit. `trick_spin_a16` multiplies at most about `1,040,000 * 65536` at `manafold_clips.h:3237`, also far below int64 overflow. The scaled support walk uses int32 at `manafold_clips.h:3182` and `3188`; the provided material does not show the magnitudes of `kLoopTubeXMm`, `kLoopNeckExitYMm`, or `kLoopArcMm`, so int32 overflow of the support walk is not disprovable from the shown material.

### 3. Spin window and unrelated timing

`trick_spin_progress_u` returns 0 for `f <= s0` or `f >= kTrickLiftKey` at `manafold_clips.h:3214`. The shipping spin keys are:

- `kTrickSpinStartKey = 100` at `manafold_art.h:2975`
- `kTrickSpinTurnKey = 128` at `manafold_art.h:2976`
- `kTrickSpinSettleKey = 140` at `manafold_art.h:2977`

The static assertions require `kTrickSpinStartKey > kTrickPlantKey` at `manafold_art.h:2983` and `kTrickSpinSettleKey < kTrickLiftKey` at `manafold_art.h:2988`. Therefore, for the default constants, nonzero spin is confined to the plant hold `[kTrickPlantKey, kTrickLiftKey)`.

The clip length does not change; the comment at `manafold_art.h:2970` explicitly says `kTrickKeys` does not grow and explains that all `sinp(f, K, n)` oscillators remain keyed to the same `K`. The balance-fade window was changed from a literal 158 to `kTrickBalFadeEndKey = kTrickLiftKey + 10` at `manafold_art.h:2958`, and `build_trick` now uses that constant at `manafold_clips.h:3305`-`3308`. The provided diff does not show the definition of `kTrickLiftKey`; the old removed `kBalFade` line in the hunk at `manafold_clips.h:3299` used `148` and `158`, so exact old-byte equivalence of the fade end depends on `kTrickLiftKey == 148`, which is not independently shown.

Drift: the only Drift code shown is the constant `kU02DriftCamBiasX` and inline `g_u02_drift_cam_bias_x` at `manafold_art.h:2736`-`2737`. The reel code that applies it is not shown.

## CONTINUATION
- Done: Flight warp is seam-exact because `t_{f+K}=t_f+cyc*65536` and `flight_sin` masks to 16 bits (`manafold_clips.h:4922`, `4934`); neutral v17 knobs return `t` unchanged (`manafold_clips.h:4925`). Shipping warp is monotone with `c16=4384`, `h16=4000` (`manafold_art.h:2819`, `2823`; `manafold_clips.h:4923`-`4924`). Trick spin segments are analytic C2 quintics (`manafold_clips.h:3224`, `3231`-`3233`); shipping final is `1,000,000` and is skipped as identity (`manafold_clips.h:3215`, `3357`). Spin yaw is world pre-multiplied and X/Z compensation uses pre/post support samples (`manafold_clips.h:3359`-`3364`). Default spin keys are inside the plant hold (`manafold_art.h:2975`-`2977`, `2983`, `2988`); `kTrickKeys` is stated unchanged (`manafold_art.h:2970`).
- Open: `sinp()` implementation not shown, so old Flight byte identity rests on the comment at `manafold_clips.h:4931`-`4932`. `kTrickLiftKey` value not shown, so fade end `lift+10` equality to old literal 158 is not independently proven (`manafold_art.h:2958`; `manafold_clips.h:3299`). Reel validation for `kFlightWarpMaxA16`, ladder spin key order, and non-whole-turn `g_u02_trick_spin_gain_pm` is not shown. int32 overflow of scaled support walk is not disprovable without loop arc/tube constants. Drift application of `g_u02_drift_cam_bias_x` is not shown.
- Next chunk: run or read the Wave-E neutral byte diff and mqa C2/spin metrics. Needs: `sinp()` source, `kTrickLiftKey`/`kTrickPlantKey` definitions, reel strict-parse/validation code, `kLoopArcMm`/`kLoopTubeXMm` values, and the old Wave-E bank for Flight/Trick neutral comparison.

## Coordinator verdict

**verified** — xhigh, 69.5k reasoning, 958s, 'none found'. spot 3/3 on d7d51171: exact rational quintic in int64 (q<=~2e14, no overflow) clips.h:3218-3233; whole-turn progress skipped so hold keys byte-identical (3359); pivot compensation from pre/post support samples bx-ax (3361-3366). Honest 'open' list (sinp, lift value) - lift 148+10=158 confirmed by coordinator.
