# Q008 answer — camera-framing-map

- model: qwen38-quasar-dflash2-k8v4-112k  dispatch: f5a8bde96d4340dfb01d46c997af904a
- when: 2026-09-19T17:49:22  seconds: 130  finish: stop  status: ok
- usage: {"completion_tokens": 10860, "completion_tokens_details": {"reasoning_tokens": 8339}, "prompt_tokens": 5352, "prompt_tokens_details": {"cached_tokens": 0}, "total_tokens": 16212}
- inputs: [{"input": "tools/reel/zhao_reel.cpp:300-365", "sha256": "abf38dfd89c2cfc9", "chars": 3766}, {"input": "tools/reel/zhao_reel.cpp:1036-1066", "sha256": "2062f67b24e27d9d", "chars": 2183}, {"input": "tools/reel/zhao_reel.cpp:4244-4265", "sha256": "6356b6571cf761f1", "chars": 1296}, {"input": "tools/reel/zhao_reel.cpp:4600-4625", "sha256": "6c14b55d2428d5b1", "chars": 1571}, {"input": "tools/reel/zhao_reel.cpp:5800-5852", "sha256": "2d7bf7b4d2f2d8d0", "chars": 3251}]
- kind: task

## Answer

## 1. Geometric meaning of each `cam_pitch` parameter

From the header comment (zhao_reel.cpp:300-305) and the implementation (314-341):

| Param | Role | Cite |
|---|---|---|
| `k` (cam_k) | **Zoom / focal scale.** Multiplies x′ and y′: `x' = k·xv`, `y' = −k·yv`. Larger k → larger image (closer). | :302, :319, :4604-4607 |
| `eye_m` | **Camera height E** (world mm, fixed-point <<16). Sets the y-offset in the view: `yv = c·(y−E)+s·(z+D)`. | :301, :318 |
| `dist_m` | **Camera distance D** from origin (world mm, <<16). Enters the z-offset: `(z+D)`. | :301, :318 |
| `ps` (cam_ps) | **sin(pitch)**, 1.16 fixed-point. Positive = pitched **down** (view direction has −y component). | :301, :319-320, :1040 "26 deg down" |
| `pc` (cam_pc) | **cos(pitch)**, 1.16 fixed-point. | :301, :319-320 |
| `bias` (cam_bias) | **Vertical aim offset in NDC** (1.16). Added to y′ by scaling the w-row: `y' += bias·w`. Shifts where the frame is centred vertically without rotating or zooming. | :323-324, :330-332 |
| `bias_x` | **Lateral (x) aim offset in NDC.** Same construction on row 0. | :306-313, :326-328 |

**Is ps/pc a sin/cos pair?** Yes, both pairs sit on the 1.16 unit circle within rounding:

- Defaults (28732, 58903): 28732² + 58903² = 4 295 089 233 vs 65536² = 4 294 967 296 (Δ ≈ 0.003 %).
- Override (9000, 64900): 9000² + 64900² = 4 293 010 000 (Δ ≈ 0.046 %).

**Pitch angles:**
- Defaults: atan(28732/58903) ≈ **26.0° down** (confirmed by comment :1040).
- (9000, 64900): atan(9000/64900) ≈ **7.9° down** (shallower, used for Fall/Blown to keep sky headroom, :5833-5834).

## 2. Which knob shifts the creature vertically without changing pitch or zoom?

**`cam_bias`** (the `bias` argument to `cam_pitch`). It adds `bias·w` to the y′ row (:330-332), shifting the aim point in NDC y without altering k, ps, pc, or the rotation.

**Sign convention:** From the deaths override (slot 17/18, :5848-5851): `cam_bias = −5200` is described as "the bias drops the aim to where the clip actually ends" so a corpse at 715 mm no longer "settles into the bottom of the frame" (:5840-5841). Dropping the aim (negative bias) moves the creature **up** in frame. Therefore:

> **Negative `cam_bias` moves the creature UP in frame.**

This is consistent with the NDC convention where +y is up: a negative bias shifts the aim down, and the creature (below the old aim) rises toward centre. A single-render test (set cam_bias = 0 vs −8000 on the Trick default) would confirm the direction visually.

## 3. Per-slot overrides (from :5800-5851)

| Subject (slot) | Override | Stated reason |
|---|---|---|
| **Hasty** (8) | `cam_bias_x = 28 000`, `cam_bias_x_end = −28 000` | Follow-cam: aim starts ahead of the creature and ends behind it, tracking the 8400 mm traverse linearly. Picked from a 4-rung ladder (0 / −9000 / −18800 / −28000) at frame 235 (:5800-5803, :5815-5816). Drift deliberately gets **none** of this because its wrap-roots make a linear tracker harmful (:5804-5812). |
| **Flight** (kFlightSlot) | `cam_k = kU02CamKFlight` (value not shown) | Was "a thumbnail at 148 000" – too small; k raised so the creature fills the frame (:5823-5825). |
| **Fall** (9) | `cam_k = 150 000`, `cam_ps = 9 000`, `cam_pc = 64 900` | Started above frame; 190 of 340 frames were empty sky. Pull back (k ↑) and tip up (pitch 26°→≈8°) so creature is on-screen from frame 0; "keep sky headroom for the drop" (:5826-5835). |
| **Blown** (kBlownSlot) | `cam_k = 168 000`, `cam_ps = 9 000`, `cam_pc = 64 900` | Goes 4.2 m up (higher than the fall), so same tipped-up framing with "a bit more room" (k larger than Fall's) (:5836-5847). |
| **Deaths** (17, 18) | `cam_k = 190 000`, `cam_bias = −5 200` | Creature ends on ground at 715 mm; house framing centred at 1250 mm would put corpse at bottom. Bias drops the aim to the ending position (:5839-5851). |

**Trick** has no override in the shown code; it uses the defaults (:1038-1041): cam_k=127000, cam_eye=14, cam_dist=33, cam_bias=14000, cam_ps=28732, cam_pc=58903.

## 4. Framing recommendations

**Trick – lift the planted crown off the bottom edge (default camera):**

Knob: **`cam_bias`**. The default 14 000 (≈ +0.21 NDC) already pushes the aim up; the crown is at the bottom, so reduce the bias (or go negative) to shift the aim down and lift the creature.

Three rungs to ladder (by eye on render):

| Rung | cam_bias | NDC ≈ |
|---|---|---|
| 1 | 0 | 0 |
| 2 | −8 000 | −0.12 |
| 3 | −16 000 | −0.24 |

Pick the first rung where the crown clears the bottom edge with a small margin.

**Flight – vertical headroom for larger travel:**

Knob: **`cam_k`** (zoom-out). Raising k shrinks the whole frame content, giving symmetric top-and-bottom headroom for greater vertical travel without changing the pitch angle.

Suggested range to try (relative to whatever `kU02CamKFlight` currently is – value not shown):

| Rung | cam_k |
|---|---|
| 1 | current + 10 000 |
| 2 | current + 20 000 |
| 3 | current + 30 000 |

If the travel is mostly **upward**, a complementary (or alternative) knob is `cam_ps`/`cam_pc` toward a shallower pitch (e.g. ps 9 000 / pc 64 900 ≈ 8°, the same pair Fall and Blown already use), which trades ground for sky headroom asymmetrically.

---

## CONTINUATION
- **Done:**
  - Mapped all seven `cam_pitch` parameters to geometric roles with line cites (zhao_reel.cpp:300-341). Confirmed ps/pc are unit-circle sin/cos pairs; default pitch ≈ 26°, override pitch ≈ 8° (:1040, :5833-5834).
  - Identified `cam_bias` as the vertical-aim knob; negative sign lifts creature (inferred from deaths override :5848-5851).
  - Summarised all five per-slot overrides (Hasty, Flight, Fall, Blown, Deaths) with reasons (:5800-5851). Trick uses defaults (:1038-1041).
  - Recommended cam_bias ladder (0 / −8k / −16k) for Trick and cam_k range (+10k/+20k/+30k) for Flight.

- **Open:**
  - The value of `u02::kU02CamKFlight` is not shown; the Flight cam_k range is relative.
  - Screen y-axis convention (NDC +y = up) was inferred from the deaths example, not stated in code. A one-render test on Trick (cam_bias 0 vs −8000) would confirm direction.
  - Whether Trick needs `cam_eye` adjustment in addition to bias (lowering eye also lifts the creature but changes perspective) is not assessed.
  - The `cam_pull` / `cam_k_end` / `cam_bias_x` lerp machinery (:4248-4262, :4604-4617) is available if Trick or Flight later need animated framing, but no override uses it for these two subjects.

- **Next chunk:** Render Trick at the three cam_bias rungs and Flight at the three cam_k rungs; record which rung clears the bottom edge / provides headroom. Inputs needed: the render test harness (referenced in :300 "authoring every render test"), the value of `u02::kU02CamKFlight`, and the Trick subject's mesh bounding box (to confirm the crown's world-y).

## Coordinator verdict

**partial** — spot 5: cam_k is zoom-in (x'=k*xv, zhao_reel.cpp:319-327) OK; ps/pc pitch angles 26/7.9 deg OK; cam_bias = NDC aim shift, raster y is down-positive (y'=-k*yv), so negative bias raises creature OK (matches deaths -5200); Trick bias ladder 14000->0/-8000/-16000 sensible; Flight headroom advice REFUTED - it self-contradicts: headroom needs SMALLER cam_k (Flight was raised 148000->250000 to cure a thumbnail). Note: source comment for Fall says 'pulls back' while raising k 127000->150000 - misleading comment, flag to worker.
