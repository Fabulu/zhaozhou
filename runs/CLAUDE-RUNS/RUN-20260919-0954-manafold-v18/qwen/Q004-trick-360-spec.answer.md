# Q004 answer — trick-360-spec

- model: qwen38-quasar-dflash2-k8v4-112k  dispatch: 2ac67c2bb25442578002532af0d3085a
- when: 2026-09-19T17:30:20  seconds: 151  finish: stop  status: ok
- usage: {"completion_tokens": 12637, "completion_tokens_details": {"reasoning_tokens": 7090}, "prompt_tokens": 8468, "prompt_tokens_details": {"cached_tokens": 0}, "total_tokens": 21105}
- inputs: [{"input": "tools/reel/manafold_clips.h:30-120", "sha256": "8e4a0bbc51f338d8", "chars": 4513}, {"input": "tools/reel/manafold_clips.h:3169-3305", "sha256": "4417c2dc6b1504d6", "chars": 8294}, {"input": "tools/reel/manafold_art.h:2815-2880", "sha256": "d50e9b8b32eb6dd3", "chars": 5003}]
- kind: task  continues Q003

## Answer

# Trick 360° Planted Spin — Corrected Implementation Spec

## 1. Support-XYZ helper

The existing `trick_support_center_y_mm` (`clips.h:3169-3187`) walks the chain and rotates by `g.q[kBRoot]` but discards X and Z (`clips.h:3186` returns only `ry`). The spin needs all three. Refactor into a three-out parameter:

```cpp
// clips.h, inserted above trick_support_center_y_mm (line 3169)
inline void trick_support_center_xyz_mm(const Rig& g,
                                        int32_t& ox, int32_t& oy, int32_t& oz) {
  int32_t px = kLoopTubeXMm, py = kLoopNeckExitYMm, pz = 0;   // clips.h:3170
  zc::quat16 q = g.q[kBJunctionF];                             // clips.h:3171
  const zc::quat16 local[2] = {g.q[kBNeck], g.q[kBHingeA]};   // clips.h:3172
  const uint8_t span_child[3] = {kBNeck, kBHingeA, kBHingeB}; // clips.h:3173
  for (int i = 0; i < 3; ++i) {                                // clips.h:3174-3183
    const int32_t len = kLoopArcMm[i] + static_cast<int32_t>(
        (static_cast<int64_t>(g.local_t[span_child[i]][1]) * 1000) >> 16);
    int32_t dx = 0, dy = 0, dz = 0;
    quat_rot_vec(q, 0, len, 0, dx, dy, dz);   // clips.h:60-69
    px += dx;  py += dy;  pz += dz;
    if (i < 2) q = quat_mul(q, local[i]);     // clips.h:38-51
  }
  quat_rot_vec(g.q[kBRoot], px, py, pz, ox, oy, oz);  // was clips.h:3185
}
// Existing caller becomes a one-line delegate:
inline int32_t trick_support_center_y_mm(const Rig& g) {
  int32_t a, b, c;
  trick_support_center_xyz_mm(g, a, b, c);
  return b;
}
```

All values are int32_t mm. The walk is bit-identical to `clips.h:3170-3185`; the only change is returning three outputs instead of one.

## 2. Per-key root-XZ compensation (integer math)

**Principle.** The support's world position is `root_pos + quat_rot(g.q[kBRoot], local_xyz)`. A world-Y spin changes `g.q[kBRoot]`, which changes the rotated XZ of the support. To pin the support's world XZ to a reference, set:

```
root_x = ref_x − rx_rot
root_z = ref_z − rz_rot
```

where `(rx_rot, ry_rot, rz_rot)` is the support's X,Y,Z after the spinned root quaternion (`trick_support_center_xyz_mm` output), and `(ref_x, ref_z)` is captured once at the spin's first key (when spin angle = 0, so the rotation is identity and the reference equals the un-spinned pose).

**Why this keeps support XZ fixed.** At any spin key:

```
support_world_x = root_x + rx_rot = (ref_x − rx_rot) + rx_rot = ref_x   ∎
support_world_z = ref_z                                              ∎
```

The rotation about world Y preserves `ry_rot` (Y is the axis), so the existing Y-pivot (`clips.h:3297-3298`) is unaffected and still correct.

**Integer form** (all int32_t mm; no floating point):

```cpp
// ref_x, ref_z are int32_t captured at kTrickSpinStartKey (see §4)
// After the spin pre-multiply into g.q[kBRoot]:
int32_t sx, sy, sz;
trick_support_center_xyz_mm(g, sx, sy, sz);   // sx,sz = support XZ in root frame
root_x_mm = ref_x − sx;   // int32_t subtraction; |sx| < ~2000 mm, safe
root_z_mm = ref_z − sz;
```

No multiplication beyond the `quat_rot_vec` internals (`clips.h:63-68`, which already use int64_t). The subtraction is exact int32_t; the support local radius is on the order of a few hundred mm (`kLoopTubeXMm` + three arc segments), well within int32_t range.

**Placement relative to the Y-pivot.** The Y-pivot writes `root_y_mm` at `clips.h:3297-3298`. The XZ compensation writes `root_x_mm` and `root_z_mm`. They are independent axes and can be computed in either order. In the code, place the XZ compensation **after** the Y-pivot block (after `clips.h:3299`) so both corrections are visible before the root write at `clips.h:3301`.

## 3. Timeline, constants, and oscillator preservation

### New constants (in `art.h`, after `kTrickLiftKey` at `art.h:2854`)

```cpp
constexpr int kTrickSpinStartKey  = 90;   // spin begins (within plant 78–148)
constexpr int kTrickSpinEndKey    = 130;  // spin ends (40 keys for 360°)
constexpr int kTrickBalFadeEndKey = 158;  // was literal 158 at clips.h:3248
```

The spin window `[90,130)` sits strictly inside the contact window `[kTrickPlantKey, kTrickLiftKey)` = `[78,148)` (`art.h:2853-2854`), so the contact declaration covers the entire spin.

### Replacing the `158` literal

`clips.h:3247-3248` currently reads:

```cpp
static const Key kBalFade[] = {{0,1000},{148,1000},{158,0},{199,0}};
const int32_t bal = f >= kTrickPlantKey && f < 158 ? ...
```

Change both to `kTrickBalFadeEndKey`. The 158 is 10 keys past `kTrickLiftKey` (`art.h:2854`); the named constant makes the dependency explicit.

### Avoiding oscillator re-phasing

The existing oscillators are:

| Call | Site | `sinp(f, K, …)` |
|---|---|---|
| `antenna_knead` | `clips.h:3231` | internal `sinp(f,K,·)` |
| wobble Z | `clips.h:3256-3257` | `sinp(f,K,6)` |
| wobble X | `clips.h:3261-3262` | `sinp(f,K,4,0x4000)` |
| show-off yaw | `clips.h:3269-3270` | `sinp(f,K,2,0x6000)` |
| antenna flex | `clips.h:3273` | `sinp(f,K,8)` |
| `front_flex_play` | `clips.h:3250` | internal `sinp(f,K,·)` |

All use `K = kTrickKeys = 200` (`art.h:2830`) and the **absolute key** `f`. If `K` changed, every `sinp(f, K, n)` (`clips.h:108-112`) would re-phase because the period is `K/n` keys.

**Choice: do NOT change K, do NOT remap time.** The spin is an additive pre-multiply on `g.q[kBRoot]` during `[90,130)`. It is not a time remap; it does not shift `f` or alter `K`. Every oscillator above continues to receive the same `(f, K, n)` as today. No re-phasing.

### Spin easing (C² at both joins)

A linear 0→360° ramp has zero angular velocity at the boundaries but non-zero angular acceleration; the root-XZ velocity jumps. For C² continuity at both joins, use a **quintic ease-in-out** on the spin progress:

```cpp
// t in [0,1000] → eased in [0,1000], C² at t=0 and t=1000
inline int32_t quintic_ease(int32_t t) {
  const int64_t t64 = t;
  const int64_t t2 = (t64 * t64) / 1000;
  const int64_t t3 = (t2  * t64) / 1000;
  const int64_t t4 = (t2  * t2)  / 1000;
  const int64_t t5 = (t3  * t2)  / 1000;
  // 10t³ − 15t⁴ + 6t⁵  (all /1000 to stay in [0,1000])
  return static_cast<int32_t>((10 * t3 - 15 * t4 + 6 * t5) / 1000);
}
```

All intermediates are ≤ 10¹⁵, well within int64_t. This satisfies the "int64_t for quintic terms" requirement.

Per-key spin angle:

```cpp
const int32_t raw  = (f - kTrickSpinStartKey) * 1000
                   / (kTrickSpinEndKey - kTrickSpinStartKey);  // [0,1000]
const int32_t eased = quintic_ease(raw);
const int32_t spin_a16 = eased * 65536 / 1000;  // [0,65536) → 360°
```

At `f = kTrickSpinStartKey`: raw = 0, eased = 0, spin = 0 (identity).
At `f = kTrickSpinEndKey − 1`: raw = 975, eased ≈ 999.99 → spin ≈ 359.9°.
The first key **outside** the window (`f = 130`) has spin = 0, so the body snaps back. To avoid this, the window should be half-open `[start, end)` and the easing should reach exactly 1000 at `f = end − 1`, giving 65536 a16 = 360° = 0° (identity). In practice, `quintic_ease(1000) = 10 * 10⁹ − 15 * 10⁹ + 6 * 10⁹ = 10⁶ → 1000`, and `1000 * 65536 / 1000 = 65536`, which wraps to 0 in angle16. So the last spin key IS identity, and the first post-spin key (spin = 0) is also identity. No snap. C² is maintained.

## 4. Code sketch (changed lines + anchors in `build_trick`)

```
 3204| inline zc::Clip build_trick() {
 3205|   const int K = kTrickKeys;
 3206|   zc::Clip c = clip_shell(13, K, kHoverHeightMm);
 3207|   Rig g;
 3208|   int32_t planted_support_reference_y_mm = 0;
+      int32_t spin_ref_x_mm = 0, spin_ref_z_mm = 0;   // NEW
+      int32_t root_x_mm = 0, root_z_mm = 0;            // NEW
 3209|   ...
 3229|   for (int f = 0; f < K; ++f) {
 3230|     g.reset();
 3231|     antenna_knead(g, 13, EyeCam::kFixed, K, f);
 ...        // flip, wobble, antenna flex — UNCHANGED (clips.h:3232-3282)
 3247|     static const Key kBalFade[] = {{0,1000},{148,1000},{kTrickBalFadeEndKey,0},{199,0}};
 3248|     const int32_t bal = f >= kTrickPlantKey && f < kTrickBalFadeEndKey
                               ? curve(kBalFade, 4, f) : 0;
 ...
 3283|     face_rest(g);
 ...
 3289|     int32_t root_y_mm = curve(kRootY, 12, f);
 3290|     if (f == kTrickPlantKey)
 3291|       planted_support_reference_y_mm = trick_support_center_y_mm(g);
 3292|     if (f >= kTrickPlantKey && f < kTrickLiftKey) {
 3297|       root_y_mm = kTrickPlantRootMm + planted_support_reference_y_mm
 3298|                   - trick_support_center_y_mm(g);
 3299|     }
+      // --- SPIN LAYER (new, between Y-pivot and root write) ---
+      if (f == kTrickSpinStartKey) {
+        int32_t sx, sy, sz;
+        trick_support_center_xyz_mm(g, sx, sy, sz);
+        spin_ref_x_mm = sx;  spin_ref_z_mm = sz;
+      }
+      if (f >= kTrickSpinStartKey && f < kTrickSpinEndKey) {
+        const int32_t raw   = (f - kTrickSpinStartKey) * 1000
+                            / (kTrickSpinEndKey - kTrickSpinStartKey);
+        const int32_t eased = quintic_ease(raw);
+        const int32_t spin_a16 = eased * 65536 / 1000;
+        // world-Y pre-multiply: spin the body about the world vertical
+        g.q[kBRoot] = quat_mul(quat_y(spin_a16), g.q[kBRoot]);
+        // recompute support XZ under the spinned quaternion
+        int32_t sx, sy, sz;
+        trick_support_center_xyz_mm(g, sx, sy, sz);
+        root_x_mm = spin_ref_x_mm - sx;
+        root_z_mm = spin_ref_z_mm - sz;
+      }
+      // --- END SPIN LAYER ---
 3300|     g.write(c, f);
+      c.root[static_cast<size_t>(f) * 3 + 0] = fxu(root_x_mm);  // NEW
 3301|     c.root[static_cast<size_t>(f) * 3 + 1] = fxu(root_y_mm);
+      c.root[static_cast<size_t>(f) * 3 + 2] = fxu(root_z_mm);  // NEW
 3302|     c.deform[static_cast<size_t>(f)] = squash_impact(f, K, kSquash, 9);
 3303|   }
 3304|   return c;
```

Key invariants preserved:
- Pure-X flip (`kTrickFlipXA16 = -32768`, `kTrickFlipZA16 = 0` at `art.h:2866-2867`) — unchanged.
- +90° face yaw (`kTrickFaceYawA16 = 16384` at `art.h:2876`) — unchanged.
- Existing pause, righting, overshoot — unchanged.
- Camera is fixed (`EyeCam::kFixed`, `clips.h:3231`) — unchanged.
- All timing/amplitude are named constants.

## 5. Additional probe / gate checks

The committed probe already checks B-swell surface contact at every key and midpoint (`art.h:2847-2849`, `clips.h:3293-3296`). The spin adds three new invariants:

**(a) Spin-progress unwrap.** At every key `f ∈ [kTrickSpinStartKey, kTrickSpinEndKey)`, project the root quaternion onto the world-Y axis (extract the Y-rotation component) and verify it unwraps monotonically from 0° toward 360°. At `f = kTrickSpinEndKey − 1`, the projected angle must be within 1 a16 (0.003°) of 360° ≡ 0°. This catches a missing pre-multiply (which would leave the body un-spun) or a wrong-axis multiply (which would rotate about X or Z instead of Y).

**(b) Support-XZ drift bound.** At every key in the spin window, compute the support's world XZ and assert:

```
|support_world_x − spin_ref_x| ≤ 1 mm
|support_world_z − spin_ref_z| ≤ 1 mm
```

This catches integer-truncation error in `quat_rot_vec` (`clips.h:66-68` uses `>> 28`, losing < 0.5 units per component) and confirms the compensation formula is correct. The 1 mm bound matches the probe's existing ground-contact tolerance (`art.h:2833-2838`).

**(c) C² continuity at spin joins.** At `kTrickSpinStartKey` and `kTrickSpinEndKey`, verify:

- **Position (C⁰):** root XZ compensation is 0 (the support XZ equals the reference). Confirmed by the easing: `quintic_ease(0) = 0`, `quintic_ease(1000) = 1000 → 65536 a16 → identity`.
- **Velocity (C¹):** the per-key delta of root XZ (`root_x[f] − root_x[f−1]`) must be 0 at both joins. With the quintic, the first and second derivatives of the eased progress are 0 at the endpoints, so the angular velocity and acceleration are 0, hence the XZ velocity (which is `ω × r`, a linear function of ω) is 0.
- **Acceleration (C²):** the second delta (`root_x[f] − 2·root_x[f−1] + root_x[f−2]`) must be 0 at both joins. The quintic's second derivative is 0 at t = 0 and t = 1, so this holds.

A practical gate: sample keys `{start−2, start−1, start, start+1, start+2}` and `{end−2, end−1, end, end+1, end+2}` and assert the root-XZ acceleration vector is within 0.1 mm.

---

## Findings

| # | Location | Rating | Note |
|---|----------|--------|------|
| 1 | `clips.h:3248` | P1 | Literal `158` is a missed timing dependency; must become `kTrickBalFadeEndKey`. If `kTrickLiftKey` or the righting schedule is ever adjusted, the fade window silently desyncs. |
| 2 | `clips.h:3186` | P2 | `trick_support_center_y_mm` discards XZ. Any caller that later needs the full support position (the spin, or a future XZ probe) must re-walk the chain. Refactoring to the three-out helper is the structural fix. |
| 3 | `clips.h:3301` | P2 | Only `c.root[f*3+1]` is written. X and Z default to whatever `clip_shell` initialised. Adding the spin makes the XZ write mandatory; until then the absence is latent. |
| 4 | `clips.h:3256-3257,3261-3262` | P3 | The wobble X and Z amplitudes (`kTrickBalanceWobbleA16` and `/2`) multiply by `bal/1000` and `sinp(f,K,·)`. The spin does not interact with these (different axis), but the combined XZ drift of wobble + spin is what the probe must bound. No code change needed here. |

No P1 found in the easing math, the a16 conversion (68 157 confirmed), or the Y-pivot. The coordinator's "C1 WRONG — flawed root-pivot claim" is consistent: the Y-pivot at `clips.h:3297-3298` is correct for Y, and the spin's XZ compensation is a separate, additive correction.

## CONTINUATION
- **Done:** (a) Three-out support helper is a drop-in refactor of `clips.h:3169-3187` returning X,Y,Z instead of Y only. (b) Root-XZ compensation formula is `root_x = ref_x − sx`, `root_z = ref_z − sz` where `(sx,sy,sz)` is the post-spin support and `(ref_x,ref_z)` is captured at `kTrickSpinStartKey`; proof of fixed support XZ given. (c) Spin is a quintic-eased pre-multiply `quat_y(spin_a16)` on `g.q[kBRoot]` during `[90,130)`, C² at both joins; K=200 unchanged so no oscillator re-phases. (d) `158` → `kTrickBalFadeEndKey`. (e) Probe additions: unwrap check, 1 mm XZ drift bound, C² gate at joins.
- **Open:** (1) The ~13 mm XZ arc from balance wobble under the existing spin-free plant (carried from Q003) is NOT yet bounded against `art.h:2834-2838` tolerance; the spin adds its own XZ offset on top. (2) Whether `front_flex_play` (`clips.h:3250`) and `antenna_knead` (`clips.h:3231`) internal `sinp` calls remain within tolerance when the body is rotated 180°+ by the spin (their oscillators are in local frame, but the probe checks world-space surface). (3) The exact `fxu` scale factor for mm→fixed-point is not shown in the provided input; the spec assumes it is the existing convention at `clips.h:3301`.
- **Next chunk:** Run the B-swell surface probe at every key and midpoint in `[78,148]` with the spin applied, specifically keys 100–130 where the antenna is in contact and the combined wobble+spin XZ offset is maximal. Inputs needed: the probe's ownership/depth assertion code (referenced at `art.h:2847-2849`, `clips.h:3293-3296`) and the `trick_support_center_y_mm` / B-swell vertex lookup to verify the 1 mm XZ drift bound and the −25 mm declared penetration (`art.h:2851`) simultaneously.

## Coordinator verdict

**partial** — spot 6: support-pivot XZ proof OK, int64 quintic OK, 158->named OK, Y preserved under world yaw OK, root XZ default 0 so writes are byte-neutral OK, K kept 200 OK; MISSED owner's overshoot+correct-back entirely; claim 'last spin key is identity' false (raw=975 at end-1) - use (end-start-1) divisor. Usable as Wave-F input with those two fixes.
