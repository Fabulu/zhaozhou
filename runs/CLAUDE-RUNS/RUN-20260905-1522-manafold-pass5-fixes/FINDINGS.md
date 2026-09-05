# Manafold Pass 5 Fixes - Findings

**Agent ID:** RUN-20260905-1522-manafold-pass5-fixes
**Created:** 2026-09-05 (Europe/Zurich)
**Parent Task:** bounded fix pass on QA items 2-10 after the pass-4 gates
**Status:** Complete

## Summary
All in-scope QA items (2-10) are fixed, judged at native on the shipping
rig, and shipped. Item 1 (shape legibility at native) was left untouched,
deliberately - it is the owner's decision. Zixxtrixx is proven
byte-identical (1136/1136 frames) from a pristine baseline built this run.

## Findings
* Item 2: the burying fog was three things - the drag fling on violent
  stationary gestures, stacked knead gains, and the smear plane fed by the
  full pale halos. Clamped/eased/refed; the loop reads on every exhibit.
* Item 3: short clips never kneaded because the hashed first fold cycle
  did not fit before the release tail; it now compresses to fit.
* Item 4: the missing-poster and the untracked page were the same class of
  silent failure; the page is committed (-text pinned), the include is
  unguarded, build-direct.sh hard-errors, .gitignore and comments fixed.
  Bonus finding: pass-4's inspect encode FAILED, leaving a 0-byte
  manafold-inspect.webm - tovideo.py writes the poster only after ffmpeg
  succeeds, which is exactly how the poster went missing. Re-encoded.
* Item 5: probe now measures per named part and gates the LENS (>= 1215
  pm); star arm untouched; no stand-off chase (raising the dome would bury
  the star and re-break containment).
* Item 6: `slot < 14` -> `slot < 15`; damage runs its authored 250.
* Item 7: ablation gate retired; U02_FOLD_FREEZE=1 (bones animate, field
  frozen) proven discriminating by CRC and by looking.
* Item 8: white ring tube 15->22 mm, offset 52->60 - a ring, not a
  crescent, on both eyes in front view.
* Item 9: orbit/wander cycles quantised per clip + release-faded feed;
  hover seam 4.07 -> 1.89 (ratio 5.62 -> 2.39, house class).
* Item 10: manafold-inspect.png ships with the re-encode.

## Recommendations
* Item 1 needs the owner's call between "bigger particles" and "nameable
  shapes" before anyone touches mote size, stencil scale, pocket or camera.
* The closure-rim look is falsifiable now that the mana thinned; a
  mana-off diagnostic lane is still worth the small cost.

## Files Examined
See TASK_LOG.md in this run folder; key sources: tools/reel/manafold_*.h,
manafold_probe.cpp, zhao_reel.cpp, build-direct.sh, website/tools/tovideo.py.
