# Pass 17 staged-span and Fall source-gate code review

**Date:** 2026-09-18
**Scope:** read-only review of the 22-bone staged C-End repair and Q5 Fall one-shot gate. No source edits, builds, renders, commits, or pushes.

## Verdict

**Fall Q5: clean. Signed-span structure: one blocking checker blind spot and one gate-headroom concern.**

The six helpers are parent-ordered inside the 32-bone ceiling, zero-rest-offset children of the intended upstream articulation, and written from one staged-fraction helper at both authored keys and nonlinear rear midpoints. Signed rounding is symmetric, the intermediate product is 64-bit, and the current magnitudes are far from overflow. The C-End zone sequence preserves the old HingeC→HingeD ramp exactly at zero delta and divides the 750 mm translation run into 90/330/330 mm stages.

Q5 selects the legacy Fall control before the function-local static bank is first constructed, so its process-local mutation cannot be hidden by `u02::type()` caching. It compares the final resolved root and deform frame plus raw quaternion/local-translation/scale midpoint tracks, handles absent scale/local tracks explicitly, and judges the mutant only from Q5's own failure count. It asserts correct held behavior rather than asserting the production bug.

## Findings

### 1. HIGH — the fatal shipping gate checks scalar tracks, not posed shipping-ring ordering

**Files:** `tools/reel/manafold_spangate.cpp:423-458`, `461-535`, `547-619`

`minimum_free_ring_step_y()` is exercised only by `check_synthetic_signs()`. Those synthetic clips have identity rotations and straight +Y spans. The shipping walk in `check_shipping_tracks()` never decodes a shipping pose or skins a ring: it checks duplicate tracks, staged fractions, per-span pm bounds, and the analytic `free_end - free_start + delta` margin.

A legal shipping key can therefore combine an in-range signed delta with a large HingeC/HingeD orientation difference whose two-weight blend locally pinches, reverses, or overlaps ring centroids. All fatal G5 conditions remain green because the scalar delta and helper fractions are correct. This is the same class of defect the additional `EMid` helper addresses—6-bit LBS behavior rather than analytic length—but the normal bank gate still proves it only in a straight synthetic pose.

**Required follow-up:** add a production-bank key/midpoint walk through `decode_pose()` + `skin_vertex()` over the real compiled rings. For each signed-gradient zone, test consecutive posed ring-centroid progress against that zone's posed endpoint direction (not world Y), plus a positive minimum separation. Run it over every shipping key and midpoint and make a mutation that perturbs one staged helper/weight while leaving scalar tracks legal. The pending every-frame render remains necessary likeness evidence, but should not be the only instrument capable of finding a posed LBS reversal.

### 2. MEDIUM — the B-C acceptance band is fitted within only a few millimetres of the current answer

**Files:** `tools/reel/manafold_art.h:2164-2175`; evidence statement `PASS17-SIGNED-SPAN-IMPLEMENTATION.md:27-33`

The shared free-span floor is authored at 24 mm specifically because the current B-C minimum leaves 27 mm; the B-C compaction floor is `-430 pm` while the recorded current extreme is approximately `-429 pm`. The effective headroom is roughly 1–3 mm—well below one native pixel and much tighter than this repository's own rule against re-recording a gate to admit its measurement.

A harmless one-millimetre pose or rounding change can now fail a structural gate even if the stick remains visually unchanged, encouraging the next pass to nudge the bound to its new answer. Conversely, the prose claim of “deliberate headroom” is not supported for the binding B-C case.

**Required follow-up:** use the post-repair native/every-frame ladder to choose a visibly wrong rejection band with real room, or re-author B-C so the accepted pose sits comfortably inside the existing band. Keep the strict positive structural limit separate from the by-eye compaction read. Do not derive a replacement value from the current 27 mm measurement alone.

## Non-blocking observations

- The C-End staged fractions use signed round-to-nearest-away-from-zero at `manafold_clips.h:147-169`; keys and recomputed nonlinear midpoints share that exact writer.
- `finalize_rear_follow_midpoints()` skips nonlinear re-solving only on a held final segment after midpoint tracks already hold the final key. Q5 verifies this assumption across all shipped pose channels.
- Rear helper/socket coincidence still uses a 10 mm tolerance against an observed 8.965 mm worst. Integrated native/4× evidence should explicitly confirm that this quantized residual does not read as a seam; the numeric headroom alone is not likeness evidence.
- Existing IDs 0..15 remain stable; helper IDs 16..21 all have parents below their own IDs.

## Exact follow-up verification

1. Fresh `mspan` normal plus all 18 mutations after adding the posed shipping-ring walk.
2. A dedicated legal-track/posed-rotation mutant that fails only the new shipping-ring check.
3. Fresh `mqa` normal, `--fall-only`, and `--fail-fall-wrap`; normal/fall-only must return 0, and the mutant command may return 0 only after Q5 itself reports the restored wrap.
4. Post-repair every-frame/native/4× review of the five formerly collapsed C-End witnesses and the 8.965 mm worst helper/socket witness.

As I always say, a ruler can prove the passengers have tickets—but only a walk down the aisle proves nobody is standing on the same seat!
