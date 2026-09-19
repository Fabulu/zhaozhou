# Q003 answer — review2-trick-360-design

- model: qwen38-quasar-dflash2-k8v4-112k  dispatch: 51fa47914ba94fc188b7cd9bc9d9ec0d
- when: 2026-09-19T17:27:50  seconds: 109  finish: stop  status: ok
- usage: {"completion_tokens": 9146, "completion_tokens_details": {"reasoning_tokens": 7812}, "prompt_tokens": 9991, "prompt_tokens_details": {"cached_tokens": 0}, "total_tokens": 19137}
- inputs: [{"input": "tools/reel/manafold_art.h:2815-2880", "sha256": "d50e9b8b32eb6dd3", "chars": 5003}, {"input": "tools/reel/manafold_clips.h:3204-3305", "sha256": "5cc438ca86c54dfa", "chars": 6321}]
- kind: review of Q001

## Answer

## Claim-by-Claim Review

**C1 – Pre-multiply keeps support planted because support is at root XZ**
**CONFIRMED** (with the assumption correctly flagged). The planted-support pivot at `clips.h:3297-3298` corrects only `root_y_mm`; the quaternion `g.q[kBRoot]` is orientation-only, so pre-multiplying `quat_y(spin)` rotates the body about the world Y axis without moving the root's XZ translation. In the ideal 180°-X headstand the antenna swell lies along the local Y (world-down) axis, i.e. at the root's XZ. The answer correctly flags the wobble-induced XZ offset as an assumption. The mechanism is sound; the caveat is noted.

**C2 – All key literals that must move are identified**
**REFUTED.** The hard-coded `158` in the balance-fade guard at `clips.h:3248` (`f >= kTrickPlantKey && f < 158`) is not part of the `kBalFade` array; it is a separate inline literal. When `kTrickLiftKey` moves from 148→180, this `158` must shift (e.g. to 190), but the answer only lists the *array* and its entries. Missing this leaves the wobble cutting off 32 keys too early in the new timeline.

**C3 – C² continuity at start, join, and end**
**CONFIRMED.** S(x)=10x³−15x⁴+6x⁵ has S′=30x²(1−x)² and S″=60x(1−x)(1−2x), both vanishing at x=0 and x=1. Each segment's p, p′, p″ match at the join (f = K_spin+K_A): value = P_peak, velocity = 0, acceleration = 0. Verified algebraically.

**C4 – θ = 68 172 a16 at p = 1040 pm**
**REFUTED.** 1040 × 65 536 / 1000 = 68 157.44, i.e. **68 157** (truncated) in integer arithmetic. The answer's 68 172 is off by 15 units. (`clips.h` conversion formula confirmed at the answer's own §4: θ = p·65536/1000.)

---

## Missed Items (≤ 3)

1. **Hard-coded `158` at `clips.h:3248`.** The guard `f < 158` is an inline literal, not an entry in `kBalFade[]`. Shifting the array does not fix it; it must move independently (to ~190 for the new lift key). Leaving it at 158 would kill the balance wobble 32 keys early.

2. **`int32_t` overflow in the quintic pseudo-code (§5).** With `x` up to 999: `x3 ≈ 10⁹` (fits), but `x4 = x3·x ≈ 10¹²` and `x5 ≈ 10¹⁵` both exceed `int32_t` max (~2.1×10⁹). The code must use `int64_t` for `x4`, `x5`, and the polynomial sum, or scale differently. As written, the spin angle would be garbage for every key past the first few.

3. **`K`-dependent phase in existing animation calls.** `antenna_knead(g, 13, …, K, f)` (`clips.h:3231`) and `front_flex_play(13, f, K)` (`clips.h:3250`) as well as every `sinp(f, K, n)` call inside the planted block (`clips.h:3257, 3262, 3270, 3273, 3279`) use `K = kTrickKeys` in their periodicity. Raising K from 200→250 changes the phase of all these oscillators across the *entire* clip, not just the plant window. The answer does not discuss whether the approach, flip, or righting phases will be visually altered.

## CONTINUATION
- **Done:** (a) Pre-multiply is the correct operation for a world-Y spin; the Y-only pivot at `clips.h:3297-3298` is the sole support correction and does not touch XZ. (b) C² easing math is correct at all three boundaries. (c) The a16 conversion formula is correct but the worked example has an arithmetic error (68 157, not 68 172). (d) The `158` literal at `clips.h:3248` is a missed timing dependency.
- **Open:** Whether the balance wobble's XZ drift (~13 mm arc under spin) exceeds the probe's tolerance at `art.h:2834-2838`; whether `front_flex_play` and `antenna_knead` phase shifts (K 200→250) are acceptable or need re-authoring; whether a XZ correction should be added to the pivot or the wobble should be gated to zero during the spin.
- **Next chunk:** Validate the probe (B-swell surface check at every key/midpoint) against the new 250-key timeline with the spin applied, specifically keys 100–170 where the antenna is in contact and the wobble + spin XZ offset is maximal. Inputs needed: the probe's ownership/depth assertion code and the `trick_support_center_y_mm` / B-swell vertex lookup.

## Coordinator verdict

**partial** — spot 5: 158 miss, arithmetic, easing, int32 overflow, K-rephase all correct; C1 WRONG - confirmed the flawed root-pivot claim (reviewer echoed author error)
