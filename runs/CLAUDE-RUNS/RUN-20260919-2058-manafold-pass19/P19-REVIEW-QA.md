# Manafold pass 19: independent review and QA

**Date:** 2026-09-19
**Reviewer:** Claude (sole agent, independent of the implementer; no Qwen, no sub-agents)
**Reviewed:** Zhaozhou `manafold-pass19` `d172a5aa`, `2de50e08`, `c5a7b82c`, `b1fddb5d` against Owner Directions 20 and 19
**Verdict:** **FIXED.** The implementation's cause analysis is right and its three fixes work: the rear connection is whole, the rear is calm, and Drift's lines are proportional. Review found and repaired:
- one art fault: the End no longer read as a ball (Direction 19 item 3);
- two instrument faults: a detector that could not see a class of fast joint, and a line check that never touched production splats;
- one gate that was holding an art value.

## 1. Verdict by item

| Item | Verdict | Deciding look |
|---|---|---|
| D20.1: rear connection whole and smooth | **PASS** | `P19-REVIEW-REAR-3WAY-3X.jpg` and `P19-REVIEW-CLIPS-REAR-VS-V18-2_5X.jpg`. v18 shows the torn fork or stub at the entry (Inspect f218/f300/f340, Rest f342, Lasso f170). Every reviewed clip is now one continuous strut into the body. |
| D20.2: rear calm but alive | **PASS** | `P19-REVIEW-INSPECT-PRESS-SNAP-VS-V18-2X.jpg`. In v18 a stub flicks beside the base through Inspect f208–224, and Pirouette f44–47 shows a jag. The review build has one strut with a gentle bend and nothing flicking. |
| D19.3: End ball still visibly thicker than the sticks | **FAIL at review, FIXED** | See §3. |
| 35° swallow-beat bend | **Accepted as authored** | `P19-REVIEW-SWALLOW-BEND-4X.jpg`. Hover f366–400 reads as a rounded elbow at the entry. The outline is continuous, with no crease or tear. With the End ball on the knee, it reads as the ball joint turning. |
| D20.3: Drift lines proportional | **PASS** | `P19-REVIEW-DRIFT-LINES-360-VS-V18-2X.jpg`. v18 is a fat glowing blob over the antenna; at 360 it is a fine outlined loop at the creature's scale. |
| 360 vs 285 | **360 kept** | `P19-REVIEW-HOVER-REST-LINES-360-VS-LEGACY-NATIVE.jpg`. Hover and Rest keep the same figure, the white core and the navy surround, with a touch less halo. That is the proportional read the owner asked for. 285 would hold mid-distance lines at close-up weight, which is the complaint in a milder form. |

## 2. Source and checker review

**Correct:**
- `rear_socket_compose` returns `authored` untouched under `legacy-root`.
- `hinge_play`'s `amb == 1000` path is exact, and the knead wag's `x*1000/1000` is exact.
- Midpoints nlerp the composed key quats. `quat16_nlerp` is hemisphere-safe, so the new ~150°-from-identity End quats cannot blend the long way round.
- `rear_relative` is re-solved per midpoint.
- The line scaling is applied at both draw sites, and `primary_radius_q8 = 0` outside `celmain` falls back to the legacy width.
- The legacy toggles reproduced the v18 bank on **9/9** subjects: the implementer's 7, plus Lasso `0x26666BC0` and Pirouette `0x18DB7B27` from the v18 bank log.

**Instrument findings (the detector law):**

1. **R1's rotation operand is structurally tautological.** HingeD's world frame is `Root·qd` and RearSocket's is `Root·qd·Authored`, so `rel` is exactly `|Authored|` whatever `qd` is. Any fault in the arm solve moves both sides together, so this operand is blind to it. It does catch its own regression: remove the compose and it fires, which is what its control shows. The **centreline-bend** half of R1, measured on skinned ring centroids, is the independent detector. It is kept as the part of R1 that matters. No change, recorded here so nobody quotes `rel` as evidence about the arm.
2. **R2 measured the wrong quantity (fixed).** "End joint step" was `| |rel_i| − |rel_i−1| |`, the change in the bend's *magnitude*. That is a lower bound on the joint's motion. It reads **zero** for a joint sweeping round at constant bend, for example the End's tilt and yaw oscillators in quadrature. It now measures the angle of `P^T Q` between successive arm→End relative rotations.
   - Shipping worst: **2.39 → 2.76°/sample** (Hover s225), so the old metric under-read by ~15%.
   - v18 frame control: 7.16. Ambient ×3: 7.99. Ambient 1000: 6.67, 650: 4.40, 600: 4.07.
3. **R2's ceiling held an art value (fixed, 4.0 → 6.0).** Under the true metric, 4.0 sat right at the 600 ambient rung (4.07), a rung that was judged by eye. The gate was therefore enforcing that judgement and would have refused the owner "a notch more wiggle". At 6.0 it guards the actual regression, the v18 snap (6.67 in the arm frame, 7.16 in legacy-root), and the eye keeps the range up to ~850. Both R2 controls still fire at 6.0.
4. **R3 never touched production splats (fixed).** R3 swept `mana_line_r_px` as a pure function, so dropping `line_push` from the fold or bolt producers would have left it green. R3 now also runs a **census** over complete Drift and Hover through the production `mana_fill(3)`/`mana_lightning` path, the same producers msmooth traces. It requires both flagged line splats (Drift 362,660; Hover 705,330) and unflagged ones (motes keep their size: 27,900 / 55,800). The new control `--fail-line-flag` strips the flags after production and fires **0x4 only**. The renderer's draw-site call is still covered by the render receipts rather than a gate: distance and legacy lines give different Drift CRCs, and legacy reproduces v18.
5. **Controls fire only their own category:** frame 0x3 (declared: v18 had both the hairpin and the fast joint), joint 0x2, line-scale 0x4, line-flag 0x4.

## 3. The End ball (Direction 19 item 3)

**Problem.** Before pass 19 the End read as a ball *because the stub stood out of the body*. With the stub gone, the End is the 20/25 mm long-low swell (half-width 280 mm) centred ~30 mm outside the waterline, so it is half buried. The looks (Inspect f300/f340 at 2×) showed the A/B/C corners still reading as mild knuckles, while the End read as nothing: a faint flare at the entry.

**Change.** The End ball is a second, shorter swell, MAX-combined with the long one:
- It is centred just proud of the surface: `kKnuckleEndBallAtMm = 2560`, `kKnuckleEndBallHalfMm = 150`.
- It sits entirely **inside** the long swell's support. This is a `static_assert`, and the renderer's env parse refuses a rung outside it with RC 2.
- So the swell-support stations, the rear span gradient and every skin weight are unchanged. The change is mesh profile only. The long swell's station and half-width feed `kRootSwellSupportStartMm[1]` and the rear signed-span run, so moving them would have moved skinning. That is why they were not the knob.
- It stands down under legacy swell mode (the exact v17 family) and under `REAR_SOCKET_FRAME=legacy-root`, where the stub it replaces is back. So the legacy control is still byte-exact v18.
- The knobs are `ZHAO_U02_END_SWELL_RX_MM/RZ_MM` and `ZHAO_U02_END_BALL_{AT,HALF,RX,RZ}_MM`, all strict, with 5 new selector legs.

**Ladder** (`P19-REVIEW-END-BALL-LADDER-2X.jpg`, Inspect f300/f40, then 4× at f300):

| Rung | Read |
|---|---|
| Shipping (long swell 20/25) | plain strut, flare only |
| B: long swell 36/45 | still a flare |
| **C: ball 2560/150, 30/36** | **a soft shoulder into a rounded swelling just proud of the body, in family with A/B/C: visibly thicker than the stick without protruding** |
| D: ball 2590/120, 26/34 | barely registers |
| E: ball 2560/150, 42/50 | a knob with a hard shoulder, edging back toward the rejected bead |

**Chosen: C** (`kKnuckleEndBallRxMm/RzMm = 30/36`). It was then checked in motion:
- Hover's swallow window: the ball rides the knee.
- Rest, Taunt III, Trick, Channel and Lasso rear crops: a smooth rounded entry everywhere.
- The Inspect press-snap strip.
- Every-frame sheets: continuous, with no pop.

## 4. Receipts

**Source:** Zhaozhou `1eb115e4` (the review fix, on top of `b1fddb5d`). `tools/reel` is clean.

**Renderer:** `.tmp/p19-rev/bin/zhao-reel-cel.exe`, a clean direct build (g++ 16.1.0) whose every binary postdates the last edit.
- MD5 `510fab169ec12c48022160114227512f`
- SHA-256 `ddd0fe99d84a8b47f953e9a2b5a5f26919865688f3ee0951bd81c069499bb231`
- All binaries are listed in `P19-RECEIPTS/review/binaries.txt`.

**Identity** (`P19-RECEIPTS/review/identity-renders.txt`):
- **Legacy toggles** (`REAR_SOCKET_FRAME=legacy-root` + `MANA_LINE_SCALE=legacy`) are **9/9 byte-identical to the v18 bank**. That is the implementer's 7 plus Lasso and Pirouette, and it holds with the End ball present in source.
- **Pre-review shipping:** reproduced exactly from `b1fddb5d` (Hover `0x40E1DBF1`, Inspect `0x7E4F8487`).
- **Default knobs:** the knob-bearing build with the ball at 0 was byte-identical to pre-review (Hover `0x40E1DBF1`). The shipped Inspect equals the looked-at C rung (`0x779615BB`).

**Final shipping sequence CRCs:**

| Subject | CRC |
|---|---|
| Hover | `0xA2D0E051` |
| Inspect | `0x779615BB` |
| Drift | `0x9A76FE69` |
| Rest | `0xA8B3AD33` |
| Taunt III | `0x75BC4777` |
| Channel | `0x8D8022A0` |
| Trick | `0x71BB47B9` |
| Lasso | `0xF6B893FE` |
| Pirouette | `0xE05DB5C3` |

The every-frame sheets in `P19-SHEETS/` were regenerated from these renders, and Lasso and Pirouette were added. The earlier sheets showed the pre-review End.

**Gate matrix: 128/128 PASS** (`P19-RECEIPTS/review/gate-matrix.txt`, extended `gatematrix_p19.sh`):
- normals 12/12;
- rear controls 4/4, each with its exact declared mask: frame 0x3, joint 0x2, line-scale 0x4, **line-flag 0x4 (new)**;
- mspan controls 35/35;
- msmooth 16/16;
- protected legs 27/27;
- Wave-F mqa 5/5;
- selectors 26/26 RC 2, which is 21 plus **5 new End swell/ball selectors**, including the out-of-support rung;
- live-history 3/3.

**Key normal numbers after the End ball:**

| Gate | Result |
|---|---|
| mrear R1 | worst rotation 16.48° and worst centreline turn 35.60° (was 35.20), both Hover swallow |
| mrear R2 | 2.76°/sample, ceiling 6.0 |
| mrear R3 | 0 law violations; census 362,660 / 705,330 line splats |
| mspan G6c | selected/legacy differ 36, actual mismatches 0 |
| mspan G6 | ring order: 0 reversed, 0 pinched, min separation 5.899 mm |
| mspan G7 | closure 6.065 mm |
| mspan terminal burial | worst rho 1087 pm |
| mmeshcheck | CLEAN, 1416 groups, 4200 edges |

**Images read:** 16, all ≤1600 px JPEG. Per-look notes are in `P19-RECEIPTS/review/look-notes.md`, written after each look.

## 5. Open for the owner (non-blocking)

1. The swallow beat still bends the tube ~35° at the End ball's body-side rim. It reads as a ball joint turning. If the owner wants it softer, the lever is `kSwallowEndJointA16PerMm` (20), and mjointpub's 20 mm End floor is the constraint.
2. The End ball is 30/36 by eye. If the owner wants it bolder or subtler, `ZHAO_U02_END_BALL_RX_MM/RZ_MM` ladder it without a rebuild. E (42/50) is where it starts reading as a bead again.
3. The ambient End gain stays at 400. With R2 at 6.0 the owner can ask for more wiggle up to ~850 without a gate change.
4. **Not done here (later packets):** the 22-subject bank, the encode, the merge and the deploy.
