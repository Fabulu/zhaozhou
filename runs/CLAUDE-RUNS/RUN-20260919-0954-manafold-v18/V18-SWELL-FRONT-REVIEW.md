# Manafold version 18 — Wave D independent swell / Front review

**Date:** 2026-09-19
**Scope:** uncommitted Wave-D source, gates, controls and selected pictures
**Verdict:** **BLOCKED before commit — selected swell/Front art reads correctly, but the ReturnTip repair changes the visible return taper and Trick contact is not support-owned**

## Verified good

- The complete `1000/700/550/400/250 pm` Hover ladder and selected/legacy plates support the by-eye `400 pm` choice. All five selected stations remain visibly thicker than their adjacent runs and no longer dominate as threaded balls; `250 pm` approaches a uniform strap.
- Shipping amplitudes are independent named Front/A/B/C/End constants. `selected|legacy` chooses the two real families before `make_loop()`, and the independent global multiplier is applied afterward to only the five swell additions. Stick stations, half-widths, weights and topology are unchanged by that selector.
- `check_swell_size()` rebuilds actual production geometry in selected, legacy and zero-swell modes and compares the source-selected compiled mode against those independent products. The exact `--fail-swell-size` control reaches 36 changed production rings and is attributed only to `kCatSwellSize`.
- Ten public clips carry real clip-specific Front X/Y curves. Their authored tables begin/end at identity and use quintic C2 segments; `normal|mute` changes only JunctionF local quaternions. `mspan` rebuilds each named production clip, compares normal/mute visible selected-core vertices and requires all ten to clear the unchanged 20 mm floor. Independent rerun reports Rest weakest at `24.37 mm`, zero direct local-channel collateral, zero production mismatch and zero seam failures.
- The selected/Front-mute native plates visibly change the body-to-Front angle while preserving Neck/A/B/C/End motion, body-like root material, open O and continuous skin. Complete selected sheets show no gross new root shear, waist, outline, eye or effect fault.
- `msmooth` now keeps stable surge/fold/lab mote positions in their histories independently from rendered visibility. A visibility-only mutant can no longer manufacture a position teleport merely by removing samples; `life_pm == 0` remains the explicit absent-state boundary.
- Independent execution of the final binaries reproduced `mspan=0`, `--fail-front-flex=1`, `--fail-swell-size=1`, and `mprobe=0`. The report accurately records the repository-wide CMake `--clean-first` command as RC 1 on missing Verilator-generated copy inputs; it does not claim a clean CMake success.

## P1 blocker — the alleged terminal-only taper repair changes the visible return run

**Files:** `tools/reel/manafold_art.h`, `tools/reel/manafold_model.h`, `tools/reel/manafold_clips.h`

Wave D changes the final authored taper key from `42/26` to `0/0` while its station remains `3190 mm`. The production mesh ends at `2930 mm`, so that key is never sampled directly. It defines the interpolation slope from the preceding `58/29` key at `2030 mm` across **every** return ring after 2030.

At the visible End station (`2660 mm`) this changes the baseline taper by roughly `50/28 -> 27/14 mm` before the selected swell is added. At the terminal sampled ring (`2930 mm`) it is still roughly `13/7 mm`, not the “zero-radius cap” claimed by source comments and the implementation report. Thus:

1. the repair is not confined to the invisible terminal ring;
2. visible return-stick/root geometry was changed outside the swell-only Wave-D scope;
3. the 400 pm selection pictures include this unreported stick thinning, so they do not isolate smaller balls from a thinner adjacent return;
4. the comment “preceding visible taper remains 58/29” names a key, not the interpolated visible surface, and is materially misleading.

**Required repair:** restore the accepted `kLoopBlade*` final key `42/26`. If the terminal cap needs zero radius, apply an explicit terminal-ring-only radius override in `make_loop()` (or equivalent named cap operand) after ordinary taper/swell evaluation. Keep the Q16 ReturnTip target if it remains correct. Add a committed positive control that restores the unsafe terminal cap/burial condition and fires the burial detector. Then rerender/review the swell ladder and all selected/legacy comparisons, because the visible End run changes.

No stick-taper measurement should choose the swell value; the corrected complete motion must be looked at again.

## P1 blocker — Trick root-height correction is certified by the wrong contact operand

**Files:** `tools/reel/manafold_art.h`, `tools/reel/manafold_probe.cpp`

Wave D changes `kTrickPlantRootMm 1706 -> 1690`, saying the committed 3D probe proved selected smaller swells changed the load-bearing contact from `-25` to `-9 mm`. The probe currently pools **every mesh vertex** and records only the global minimum Y during the contact/apron window. It does not record mesh ownership, antenna support region, ring/station or whether the deepest vertex is the planted loop peak.

Therefore `-25 mm` proves that *some* creature vertex penetrates the ground, not that the intended antenna support owns contact. This is precisely the checker gap already named in `V18-ARCHITECTURE.md` §8 and cannot justify moving a production root-height knob before the support operand exists.

**Required repair:** strengthen the committed 3D probe now (or revert the root-height change until Wave F) so every planted key/midpoint identifies the deepest/contact vertices and requires the declared contact to come from the antenna support region around the loop peak. Prove the body/eyes/other antenna regions do not become the support, preserve the unchanged `-60..-5 mm` band and add a fired wrong-support positive control. Only then retain or revise `1690`.

## P2 — two Front-channel representations drift from the ratified ownership story

**Files:** `tools/reel/manafold_clips.h`, `V18-SWELL-FRONT-ART.md`

Wave B appended `HingePlay::tilt_front/yaw_front`, and the architecture/report says public art is carried through that object. Wave D instead introduces a separate `FrontFlexPose`/`front_flex_at()` path and directly post-multiplies JunctionF in `apply_front_flex()`. No shipping call populates the appended HingePlay Front fields.

The rendered composition is currently equivalent—existing fold/rest is already on JunctionF, then X/Y are post-multiplied—but there are now two Front X/Y APIs and the report’s ownership statement is false. That is a future ordering/double-authority hazard.

**Required cleanup:** route both mechanism stimuli and shipping tables through one shared Front-axis application/representation, or remove the unused HingePlay Front fields and correct the architecture/reports. Keep the fixed order `rest/fold Z -> X -> Y` and the existing normal/mute isolation.

## Control/build disposition

The focused matrix is otherwise credible:

- direct clean touched-target builds are identified by exact hashes;
- normal protected gates return RC 0;
- 34/34 span/root/Front/swell controls and 16/16 effect controls are reported attributed RC 1;
- independent focused rerun confirmed the two new controls and normal probe;
- selector invalid cases are strict;
- unrelated Zixxtrixx identity is preserved;
- no Wave-D lane process remains.

The failed repository-wide clean CMake regeneration is an infrastructure receipt, not a source verdict. The source packet must not be committed until both P1 blockers are repaired and the corrected visual/gate packet is regenerated. Wave E must not begin on this tree.
