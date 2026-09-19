# Q001 answer — trick-360-design

- model: qwen38-quasar-dflash2-k8v4-112k  dispatch: d00636c17eaa4ba586534d8620d2166e
- when: 2026-09-19T17:21:18  seconds: 160  finish: length  status: TRUNCATED
- usage: {"completion_tokens": 14000, "completion_tokens_details": {"reasoning_tokens": 9598}, "prompt_tokens": 5214, "prompt_tokens_details": {"cached_tokens": 0}, "total_tokens": 19214}
- inputs: [{"input": "tools/reel/manafold_art.h:2815-2880", "sha256": "a9dafb1aa7f44a09", "chars": 4997}, {"input": "tools/reel/manafold_clips.h:3204-3305", "sha256": "5cc438ca86c54dfa", "chars": 6321}]
- kind: task

## Answer

# Q001: Trick 360° Spin – Design (no patch)

## 1. Current Timeline (key ranges and constants)

| Phase | Keys | Duration | Defining constants |
|---|---|---|---|
| Approach | 0 – 41 | 42 | `kTrickFlipStartKey = 42` (art.h:2841); `kRootY` dip at key 22, rise at 34 (clips.h:3219-3220); `kGazeDown` look-down by key 8 (clips.h:3223-3224) |
| Flip (pitch-over) | 42 – 77 | 35 | `kTrickFlipStartKey` (art.h:2841); `kFlip` ramps 0→−1000 over keys 42–78 (clips.h:3211-3213); `kTrickFlipXA16 = −32768` = 180° about local X (art.h:2855) |
| Plant / hold | 78 – 147 | 70 | `kTrickPlantKey = 78` (art.h:2842); `kTrickLiftKey = 148` (art.h:2843); `kTrickPlantRootMm = 1534` (art.h:2839); `kTrickPlantDepthMm = 25` (art.h:2840); balance wobble active via `kBalFade` (clips.h:3247) |
| Righting | 148 – 185 | 37 | `kTrickLiftKey = 148` → `kTrickHomeKey = 186` (art.h:2843-2844); `kFlip` ramps −1000→+80→−40→0 over keys 148–186 (clips.h:3212-3213); `kTrickOvershootA16 = 2600` (art.h:2846) |
| Home / settle | 186 – 199 | 14 | `kTrickHomeKey = 186` (art.h:2844); `kRootY` returns to 1250 by key 199 (clips.h:3222) |

Total clip: `kTrickKeys = 200` (art.h:2819).

The planted-support pivot (clips.h:3290-3299) is active for `f ∈ [kTrickPlantKey, kTrickLiftKey)` = [78, 148). It locks `root_y` so the antenna-swell centre stays at `kTrickPlantRootMm + planted_support_reference_y_mm`.

## 2. Per-key root-orientation composition in `build_trick`

The root quaternion `g.q[kBRoot]` is built in this order (all inside the per-key loop, clips.h:3229-3230):

1. **`g.reset()`** then **`antenna_knead(…)`** (clips.h:3230-3231) – sets the base fold/knead pose on all joints including `kBRoot`.
2. **Flip + face-yaw post-multiply** (clips.h:3242-3244):
   ```cpp
   g.q[kBRoot] = quat_mul(
       g.q[kBRoot],                          // existing (knead)
       quat_mul(quat_mul(quat_x(flip_x),
                         quat_z(flip_z)),
                quat_y(face_yaw)));
   ```
   Inner-to-outer composition: `quat_x(flip_x)` → `quat_z(flip_z)` → `quat_y(face_yaw)`, all **post-multiplied** = applied in the **local** frame of the root. `flip_x` carries the 180° headstand (art.h:2855), `flip_z` is the legacy red-control (currently 0, art.h:2856), `face_yaw` is the +90° planted-face correction (art.h:2865).
3. **Planted balance wobble** (clips.h:3253-3270, only when `planted`): three more **post-multiplications** in local frame – `quat_z(wobble_z)`, `quat_x(wobble_x)`, `quat_y(showoff_yaw)` (showoff currently 0, art.h:2872).
4. **Loop / antenna-flex** (clips.h:3274-3279 or 3281): sets the loop-body hinge quaternions (`kBJunctionF`, etc.), not `kBRoot`.
5. **Root Y translation** (clips.h:3289-3301): `root_y_mm` is corrected by the planted-support pivot (clips.h:3297-3298), then written to `c.root[…,1]`.

**Where the world-Y spin goes:** A rotation about the **world** vertical (support) axis must be **pre-multiplied** on the fully-composed `kBRoot`, i.e. *after* steps 1-3, so it acts in the world frame before all local rotations:

```cpp
g.q[kBRoot] = quat_mul(quat_y(spin_a16), g.q[kBRoot]);  // world-Y pre-multiply
```

Insertion point: inside the `if (planted)` block (clips.h:3251), **after** the showoff-yaw line (clips.h:3270) and **before** the antenna-flex / `loop_pose` (clips.h:3271-3275). Pre-multiplying by `quat_y(spin_a16)` rotates the entire inverted body (antenna included) about the world Y axis. Because the antenna tip sits at the root's XZ position in the straight-headstand pose, the tip remains planted; only the body azimuth changes.

> **Assumption flagged:** the design assumes the antenna tip XZ coincides with the root XZ in the planted state (straight headstand, no lateral offset). The balance wobble (±5° about local Z and X, art.h:2845) introduces a small XZ offset; after the world-Y spin this traces a small arc (~13 mm at a 150 mm body radius). The committed probe (which checks the B-swell surface at every key/midpoint, art.h:2834-2838) must be re-verified.

## 3. Proposed new timeline

The owner's sequence: *180° (existing) → pause (existing) → 360° spin → overshoot → correction → righting (existing).*

### New / modified constants

| Constant | Value | Purpose |
|---|---|---|
| `kTrickKeys` | **250** (was 200) | Total clip length |
| `kTrickFlipStartKey` | 42 (unchanged) | |
| `kTrickPlantKey` | 78 (unchanged) | Contact window opens |
| **`kTrickSpinSettleKeys`** | **22** | Settle into headstand before spin starts |
| **`kTrickSpinStartKey`** | **100** (= 78 + 22) | Spin progress begins |
| **`kTrickSpinSegAKeys`** | **45** | Duration of segment A (0 → 1000+overshoot) |
| **`kTrickSpinSegBKeys`** | **25** | Duration of segment B (correction back to 1000) |
| **`kTrickSpinEndKey`** | **170** (= 100 + 45 + 25) | Spin progress ends |
| **`kTrickSpinOvershootPm`** | **40** | Overshoot past 360° in per-mille (≈ 14.4° in a16) |
| `kTrickLiftKey` | **180** (was 148) | Held plant ends; righting begins |
| `kTrickHomeKey` | **218** (was 186) | Righted |

All other constants (`kTrickPlantRootMm`, `kTrickPlantDepthMm`, `kTrickBalanceWobbleA16`, `kTrickOvershootA16`, `kTrickFlipXA16`, `kTrickFaceYawA16`, `kTrickShowoffYawA16`) remain unchanged.

### Key-range summary (new)

| Phase | Keys | Notes |
|---|---|---|
| Approach | 0 – 41 | unchanged |
| Flip | 42 – 77 | unchanged |
| Plant settle / balance | 78 – 99 | 22 keys; existing wobble runs, no spin |
| **Spin (seg A)** | **100 – 144** | 45 keys; 0 → 1040 pm |
| **Spin (seg B)** | **145 – 169** | 25 keys; 1040 → 1000 pm |
| Post-spin settle | 170 – 179 | 10 keys; wobble continues, spin at identity |
| **Righting** | **180 – 217** | 38 keys (was 37); `kFlip` −1000→0 with overshoot |
| **Home** | **218 – 249** | 32 keys (was 14) |

**`kTrickKeys` must grow** to 250. `kTrickLiftKey` moves from 148 → 180. `kTrickHomeKey` moves from 186 → 218. The `kFlip`, `kRootY`, `kGazeDown`, `kSquash`, and `kBalFade` key arrays (clips.h:3211-3228, 3247) must have their post-148 entries shifted by +32 and their key count updated.

**Contact window:** `[kTrickPlantKey, kTrickLiftKey)` = **[78, 180)**. The spin occupies [100, 170) ⊂ [78, 180) ✓. The planted-support pivot (clips.h:3292-3299) and the probe's depth/ownership checks (art.h:2834-2838) remain valid throughout the spin because the antenna stays planted (world-Y rotation about the support axis doesn't change Y, and the XZ offset from wobble is small – see risk 6.2).

## 4. C2 easing for the spin progress

Progress `p` is in **per-mille of one revolution**: 1000 pm = 360° = 65536 a16. Overshoot target: `P_peak = 1000 + kTrickSpinOvershootPm = 1040`.

Two segments, each a **quintic smoothstep** `S(x) = 10x³ − 15x⁴ + 6x⁵` (C∞, but in particular C², on [0,1]):

$$S'(x) = 30x^2(1-x)^2, \qquad S''(x) = 60x(1-x)(1-2x)$$

At x = 0: S = S' = S'' = 0. At x = 1: S = 1, S' = S'' = 0. ✓

**Segment A** (keys `kTrickSpinStartKey … kTrickSpinStartKey + kTrickSpinSegAKeys − 1`):

$$p_A(\tau) = P_{\text{peak}} \;\cdot\; S\!\left(\tfrac{f - K_{\text{spin}}}{K_A}\right), \quad \tau \in [0,\,K_A)$$

**Segment B** (keys `…+K_A … kTrickSpinEndKey − 1`):

$$p_B(\tau) = P_{\text{peak}} - kTrickSpinOvershootPm \;\cdot\; S\!\left(\tfrac{f - K_{\text{spin}} - K_A}{K_B}\right), \quad \tau \in [0,\,K_B)$$

**C² continuity proof at the join (f = K_spin + K_A):**

| Quantity | Seg A at x=1 | Seg B at x=0 | Match? |
|---|---|---|---|
| p | P_peak · 1 = 1040 | 1040 − 40·0 = 1040 | ✓ |
| p′ (per key) | P_peak · S'(1)/K_A = 0 | −40 · S'(0)/K_B = 0 | ✓ |
| p″ (per key²) | P_peak · S''(1)/K_A² = 0 | −40 · S''(0)/K_B² = 0 | ✓ |

**At clip start (f = K_spin):** p = 0, p′ = 0, p″ = 0 ✓
**At spin end (f = K_spin + K_A + K_B):** p = 1040 − 40·1 = 1000, p′ = 0, p″ = 0 ✓

**Angle conversion** (only when building the quaternion):

$$\theta_{a16} = \frac{p \;\times\; 65536}{1000}$$

At p = 1000: θ = 65536 ≡ 0 (identity). At p = 1040: θ = 68172 a16 ≈ 374.4°.

## 5. Pseudo-code for the spin insertion

```cpp
// --- NEW named constants (in manafold_art.h) ---
constexpr int kTrickKeys = 250;
constexpr int kTrickSpinSettleKeys = 22;
constexpr int kTrickSpinStartKey   = 100;   // = kTrickPlantKey + kTrickSpinSettleKeys
constexpr int kTrickSpinSegAKeys   = 45;
constexpr int kTrickSpinSegBKeys   = 25;
constexpr int kTrickSpinEndKey     = 170;   // = kTrickSpinStartKey + A + B
constexpr int32_t kTrickSpinOvershootPm = 40;
constexpr int kTrickLiftKey = 180;
constexpr int kTrickHomeKey = 218;

// --- Inside build_trick(), in the per-key loop (clips.h) ---
// … after the showoff-yaw quat_mul (clips.h:3270), still inside if(planted):

if (f >= kTrickSpinStartKey && f < kTrickSpinEndKey) {
  const int32_t Pk = 1000 + kTrickSpinOvershootPm;  // 1040
  int32_t pm;
  if (f < kTrickSpinStartKey + kTrickSpinSegAKeys) {
    // Segment A: 0 → Pk  (quintic)
    const int32_t x = (f - kTrickSpinStartKey) * 1000
                    / kTrickSpinSegAKeys;          // 0..999
    const int32_t x2 = x * x, x3 = x2 * x;
    const int32_t x4 = x3 * x, x5 = x4 * x;
    // S(x) = (10x³ - 15x⁴ + 6x⁵) / 1000³  →  keep in per-mille:
    const int32_t s5 = (10*x3 - 15*x4 + 6*x5) / 1000000;
    pm = Pk * s5 / 1000;
  } else {
    // Segment B: Pk → 1000  (correction)
    const int32_t t  = f - kTrickSpinStartKey - kTrickSpinSegAKeys;
    const int32_t x  = t * 1000 / kTrickSpinSegBKeys;
    const int32_t x2 = x * x, x3 = x2 * x;
    const int32_t x4 = x3 * x, x5 = x4 * x;
    const int32_t s5 = (10*x3 - 15*x4 + 6*x5) / 1000000;
    pm = Pk - kTrickSpinOvershootPm * s5 / 1000;
  }
  const int32_t spin_a16 = (pm * 65536) / 1000;
  // World-Y pre-multiply: antenna stays planted, body spins in azimuth
  g.q[kBRoot] = quat_mul(quat_y(spin_a16), g.q[kBRoot]);
}
```

> Note: `quat_mul(world, local)` = world-frame rotation applied **before** all existing local rotations (clips.h:3242-3244, 3253-3270). This is the key distinction from the existing post-multiplications.

## 6. Risks

| # | Risk | Detail | Severity |
|---|---|---|---|
| 6.1 | **Face read / judging camera** | During the 360° spin the face (with `kTrickFaceYawA16 = 16384`, art.h:2865) sweeps through all azimuths. The eyes will be edge-on to the fixed camera for roughly half the spin. This is inherent to a full revolution and is the "show-off" the owner wants; however, if the judging probe scores face-visibility per key, a 70-key edge-on window may fail. **Mitigation:** confirm the probe does not require a minimum face-azimuth during the plant, or gate the face-yaw to 0 during the spin (separate constant). | P2 |
| 6.2 | **Antenna support XZ drift** | The balance wobble (±5° local Z and X, art.h:2845) displaces the antenna tip from the root XZ by ~13 mm. Under the world-Y spin, this offset rotates, tracing a small

## Coordinator verdict

**partial** — spot 6: composition order + easing OK; P1 flaw - spin about root drags planted support (support has X offset kLoopTubeXMm; pivot fixes Y only); missed hard-coded 158; int32 overflow in quintic; 68172 should be 68157
