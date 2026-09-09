# Task Log: RUN-20260909-0316 - [Describe objective here]

**Created:** 2026-09-09 03:16 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260909-0316-p14-reel-light-mana-camera/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-09 03:16 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260909-0316
- Created working directory
- Initial context: [brief description]

---

## Subagent Spawns

*Log subagent spawns and their findings here*

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| | | | | |

---

## Files Created

*Updated as files are created*

---

## Decisions Made

*Updated as decisions are made*

---

## Next Steps

*Updated as progress is made*

## IMPL-REEL — pass 14, "light, mana, camera"

Lane: `C:\programmieren\zencrifice\manafold-p14-reel\{zhaozhou,Upheaval}`.
Items: R5 (framing + mist), R6 (lightning draws shapes), R7 lamp experiment,
R8 (dark side), R9 (loop/showcase hygiene), R10.3-4 (instrument hygiene).

### Baseline build
`tools/reel/build-direct.sh --output ../build-reel --clean cel` -> exit 0.
md5 `12ca4ca3fa95aeae45e8091c9341afc6` (build-reel/base-md5.txt).
Every A/B this pass comes from ONE binary per generation; md5 recorded each time.

### Reading before authoring — what the SOURCE says (all verified by grep/read,
### not inherited)

* **R6(a) is ALREADY DONE in the shipping tree.** `mana_fold` (fx.h:1832-1866)
  walks `fold_edge_link(ph.shape_to, i)` and calls `bolt_path(...)` for every
  linked station pair, stamping halo+core along it. The plan's "route the bolt
  primitive along fold_edge_link" is not a missing wire. What is missing is that
  the edge is stamped in the FOLD's own dim aqua (halo 220 pm x `lit`, core
  430 pm at r=2 px) while `mana_lightning` composites an INDEPENDENT white
  strand at 950-1080 pm over the same pocket with no knowledge of the stencil.
  => the mechanism to attack is the COMPETITION, not the routing.
* **R6(d) is CLOSED.** Grepped every `s.planet` site in zhao_reel.cpp: the only
  one that sets `planet = 1` for a Manafold clip is `u02_backdrop()`, called
  from exactly two places (slot 2 in `subject_u02_clip`, and `crackle`).
  Everything else sets `planet = 0` explicitly. The `ffae071e` half-landing was
  properly repaired in pass 13. No further work; recorded so nobody re-opens it.
* **`crackle` runs candidate 4 — LIGHTNING ONLY, no fold at all.** `subject_u02_clip`
  sets `s.u02_mana = 9` for every slot but 7; `crackle` then overrides to `4`.
  So `crackle` has never drawn a single stencil figure. That is why the review
  sees "a necklace sliding on a track" and why the two clips "do not look like
  one creature's two mana clips" — they are running different code.
* **R5's "copy `fall`'s mist budget" cannot be done as stated: there is no
  per-clip mist budget.** `u02::g_u02_mist` is ONE global; every showcase clip
  runs the same `kMist*` defaults. `fall`, `drift` and `hasty` are already
  byte-identical in mist config. The difference is `mist_speed_mul_pm`
  (fx.h:1008) applied twice, plus `kMistKeepPm = 420` memory integrating over a
  long horizontal traverse with the plane following at only 820 pm.
* **R8 has no free light slot.** `zref_creature.hpp:1206` caps
  `kCreatureMaxPointLights = 4` and `sample_u02_moving_sources` fills all four
  (warm/blue/orange/green). A fifth point light is a shared-core edit. So the
  rim has to come from the rig's own key/fill/ambient triple or from
  repurposing one of the four.


---

# RELAUNCH 2026-09-09 ~08:00 -- Owner Direction 10 arrived while the lane was stopped

**WHERE I AM, written BEFORE the rebuild comes back** (CLAUDE.md: the
half-finished thing in your head is exactly what a result destroys).

In progress: clean rebuild #2 with the TWO-PASS strand draw + the stamp/cap
knobs. Next step when it lands: re-render `channel` baseline AND two-pass from
the SAME binary, crop f363 at 4x, and check whether the halo survives its own
line's white. Then the stamp ladder, then the sunset backdrop.

## What the pictures have already settled (each one named, each one looked at)

1. **The `
` in the D10 rung fprintf had been eaten** -- a raw newline inside
   a string literal, which cannot compile. Gotcha 21 is filed under `.ps1` and
   Windows paths; it applies to **C++ source and to bash heredocs**, and my
   FIRST repair of it silently no-opped for the same reason. Fixed with
   `chr(92)` and verified by `repr`, not by eye.
2. **`ZIXX_EXP=celmain` is required.** Without it `channel` dies at frame 80
   with rc=1 and a lost stdout buffer. Same fact IMPL-FACE found from the other
   side ("the shipping terminator is the SMOOTH toon branch").
3. **The strand mechanism is REAL and the subtraction is real.** Baseline vs
   strand over all 420 frames: 417 changed, and **darker px 357,417 vs lighter
   187,054** -- the dark halo moves nearly twice as many pixels as the white
   core. `opaque + soft` is `dst*(1-a) + pal*a`, verified by reading
   `glow_splat`, so a ramp darker than the sky genuinely subtracts.
4. **But it does not look like lightning, in two specific ways**, and both were
   invisible to every count I had:
   * **The white core is EXACTLY ONE PIXEL** (measured through a dot at f363:
     dx=0 is (255,255,255), dx=+/-1 is background). Saturated -- so gain is not
     the lever, AGAIN.
   * **The halo's mean drop is 30 lum out of ~700, 4%.** The crayon grain
     verbatim: mathematically present, visually invisible.
5. **Bigger is the WRONG axis, and the plate refuted it.** core 7 / dark 24
   made the pocket *lighter and mushier*, not darker. Cause found:
   `/*bloom=*/true` -- every mana splat uses the soft bloom sprite, so a big
   radius is a fuzzy ball and never a line; and with dark-then-white stamped
   PER BEAD, each bead's additive white cancels the previous bead's dark.
6. **So the fix is ordering, not scale** -- one dark pass over the whole figure,
   then one white pass -- and **the axis I never gave myself a knob for was the
   one the owner named first**: CONNECTEDNESS. I had built overrides for core
   radius, halo radius and halo gain, and none for spacing. That is the gain
   ladder error committed a second time: sweeping the axis I had a knob on
   rather than the axis carrying the fault.

## Still open
Stamp/cap ladder; both backdrops (`ZHAO_U02_PLANET=1` night / `ZHAO_U02_NOPLANET=1`
sunset -- one binary, both skies, verified working); R5 hasty/drift; R8; R7; R9;
R10.3-4.


---

# CLOSING STATE 2026-09-09

**Pushed to origin main:** zhaozhou `8a8cb71e`, Upheaval `356fe1e`.
Deliverable: `Upheaval/creature/Manafold/PASS-14-FINDINGS-REEL.md` +
`pass14-plates-reel/` (7 plates, both backdrops, provenance beside each).

## Shipped
* **D10 strand**, default **OFF** (`kFoldStrandOn`), one env away, plate delivered.
* **R5 hasty follow-cam**, default on, value picked off the ladder by eye.
* **`rungsweep.py`** -- one binary, N env rungs, a plate, and it PURGES.
* **`cam_pitch` gains a lateral bias**, 0 == the old matrix element for element.

## Verified, not asserted
    channel 0x241D7382   crackle 0x16B89597   hover 0x30AADDA2   (== baseline)
    drift   0xBAA55FD8   (== baseline)        hasty 0x9B5E323B -> 0xBF69669B
    reel --check: all sequence CRCs match
    hasty f235 md5 c5b3522e == the -28000 rung picked off the plate

## Five times measurement beat reading, and twice it beat my eye
1. Two rungs I described as "slightly different" were BYTE-IDENTICAL.
2. A pixel census said the sunset favours the white core 2.4:1; the plate says
   the dark surround is the loudest thing in frame. The census measures AREA;
   the owner's sentence is about CONTRAST.
3. "Reverts to pass 13 byte for byte" was false, and only a baseline build
   could say so.
4. The R5 follow-cam sign was backwards; every rung made the fault worse.
5. The same follow REFUTED itself on drift, whose fault is the root WRAP.
And the eye was right twice where numbers were quiet: "bigger" looked wrong
before the darkpx count confirmed it, and PERSEG 4 looked like lightning.

## NOT done
R7, R8 (unstarted new rim law -- do not half-author it), R9, R10.3-4.
Drift's wrap-frame clip is open and deliberately not papered over.

## Do NOT delete this lane.
