# Task Log: RUN-20260904-0705 - Unnamed02 mana conduit: architecture plan

**Created:** 2026-09-04 07:05 UTC+02:00
**Status:** Complete
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260904-0705-conduit-architecture/

---

## Objective

Produce the implementation PLAN for creature 02 (the floating mana conduit,
`Upheaval/creature/Unnamed02/`) — a plan, not an implementation. No geometry,
no pose values, no builds, no renders. Rule where the two recons left choices,
confront the three known obstacles (opaque particles, the single pre-resolve
hook slot, the ≤2 suns/flares cap), and hand an implementer an ordered spike
list, build path, form/motion/effects design, knob list, verification plan and
cut order.

---

## Progress Timeline

### 2026-09-04 07:05 UTC+02:00 - Task Started

- Fetched `origin main` in both lane repos; zhaozhou at `0ef1c35c`
  (= origin/main), Upheaval at `1cb1ec5` (= origin/main), local branch
  `zixxtrixx-wholebody-s-spring` in both, exactly on origin/main.
- Created run via init-run.ps1, slug `conduit-architecture`.

### 2026-09-04 - Reading pass (in the ordered sequence)

1. `OWNER-DIRECTION-1-2026-09-04.md` — binding; includes the late FLOATS
   amendment. `Unnamed02/reports/` does not exist yet.
2. `Unnamed02/README.md` + all three concept sheets LOOKED AT (Side, Front,
   Description incl. the oblique bulging-eye sketch).
3. `RECON-FX-FINDINGS.md` (469 lines) and `RECON-NEWCREATURE-FINDINGS.md`
   (617 lines) — inherited, not re-derived.
4. `00-START-HERE.md`, `07-MOTION-STYLE.md`, `08-LIGHTING.md` (incl.
   Directions 29/30), `05-BUDGETS.md`, blueprint layout.
5. `zhaozhou/CLAUDE.md` (matches root working rules) and
   `reports/DoubleHelixTornado.md`.

### 2026-09-04 - PLAN.md written

- `PLAN.md` committed in this run folder. Headline rulings in its §0;
  five-spike list in §2 with the hook-chaining spike first, as required.

---

## Subagent Spawns

None — serial run by instruction.

---

## Files Created

- `PLAN.md` — the deliverable.
- This TASK_LOG.

---

## Decisions Made

(Full list with rationale: PLAN.md §0. Summary:)

1. Additive blend IS wired into the particle path (new flag bit; off =
   CRC-identical). Spike S3.
2. Pre-resolve hooks chained sky → celestial → creature; Spike S1 first;
   fallback = explicit composition inside creature_hook for u02 subjects.
3. Sun effects are scene-level; per-conduit centre glow is a shared-ramp baked
   additive sprite, NOT a sun/flare — dissolves the ≤2-per-view cap collision.
4. Lightning = deterministic jittered bead-chain on the Population tri path.
5. Ten kinds = ten named emitter tables feeding one capped scene Population.
6. Code beside Zixxtrixx split into migration-shaped headers, plus
   `unnamed02_fx.h`; durable record in the Upheaval package via scaffold-to-
   sibling-and-merge.
7. Eyes are real faceted geometry with lens+pupil bones and a deform squint.
8. Compression = deform sidecar, one `kCompressAmpPm` knob driving sidecar and
   sympathetic hinge bob.
9. Probe asserts clearance (floats); ground-contact law declared inapplicable
   in CREATURE.json.
10. Publish at end of finished pass without asking (standing authorisation +
    explicit direction), `-Branch main` mandatory.

---

## Next Steps

For the implementer, not this run:

1. Re-read `OWNER-DIRECTION-1-2026-09-04.md` and check `Unnamed02/reports/`
   before the first edit (the file grew twice in one day).
2. Run spikes S1–S5 in PLAN.md §2 order; log verdicts.
3. Follow the build path in PLAN.md §3.2.

---

# IMPLEMENTATION (same run folder, implementer session, 2026-09-04)

**Status: IN PROGRESS.** The architecture pass above delivered PLAN.md; this
section logs the implementation that executes it. Serial, no helper agents.

## 2026-09-04 - Implementation started

- Fetched origin main both repos: zhaozhou `a311faf6`, Upheaval `223403bb`
  (Upheaval moved past the plan's `1cb1ec5` — new HEAD is on origin/main,
  lane branch `zixxtrixx-wholebody-s-spring` tracks it exactly).
- Re-read OWNER-DIRECTION-1 (119 lines, FLOATS amendment present).
  `Unnamed02/reports/` still does not exist. No newer direction.
- LOOKED at all three concept sheets. Side: pink teardrop, loop with visible
  drawn hinge nodes at the loop corners. Front: blade-like antenna, two eyes
  in outward V. Description: oblique eye sketch shows the lens standing proud
  of the silhouette.
- Read both recons, 00/05/07/08 docs, CLAUDE.md, PLAN.md.

## Spike S1 — chain the pre-resolve hooks (STARTING)

Confirmed in this lane: `set_pre_resolve` stores one `(fn, ctx)`
(`zref_render.hpp:326`); three exclusive installers in `zhao_reel.cpp` at
2951 (cel_hook), 2972 (planet_sky_hook), 3047 (creature_hook) — creature
silently wins. Building the chained dispatcher next.

## Spike S1 — VERDICT: PASS

- Chained dispatcher (`HookChain` + `chained_pre_resolve`, order sky ->
  celestial -> creature) replaces the three exclusive `set_pre_resolve` calls.
- Composite proof: `u02-s1-{bloom,creature,chained}` diagnostic subjects.
  LOOKED at frames: bloom paints only depth==0 sky, starfield + S01 corona
  ride over the bloomed sky and are depth-tested behind the hill, creature
  composes over everything. `evidence/{bloom,creature,chained}-f24.png`.
  (The watchdog renders as a black silhouette under celmain/cool-cross in
  BOTH creature-only and chained — preexisting look of that subject under
  the zixx env, not a chain artefact.)
- `--check` all pinned CRCs match: `evidence-s1-check-chained.txt`.
- 22-subject bank baseline (pristine a311faf6 binary) vs chained binary:
  ALL-IDENTICAL — `evidence-s1-bank-identity.txt`.

## Spike S2 — VERDICT: PASS

- `Species` enum (`kAuto`,`kWatchdog`,`kZixxtrixx`,`kUnnamed02`) on
  SceneSubject; kAuto resolves to the legacy `creature >= 3` rule so all 59
  existing subject builders are untouched; u02 subjects will set it
  explicitly. kUnnamed02 dog-type arm wires in at the form milestone.
- `--check` green (`evidence-s2s3-check.txt`); bank ALL-IDENTICAL
  (`evidence-s2s3-bank-identity.txt`, S2+S3 build vs pristine baseline).

## Spike S3 — VERDICT: PASS (with a colour-calibration lesson)

- `draw_population` flag b2 = additive: point path saturating per-channel add
  in `blit_pattern_block`; tri path via existing `BlendMode::kAdditive` with
  full vertex alpha (a=65536, interp_alpha on) = exactly RASTER.FRAGMENT ADD.
  Reel knob `SceneSubject::pop_flags` (default 0x0003 = historic).
- Identity: flag clear proven byte-identical — --check + full bank (above).
- Look: `u02-s3-motes` vs `u02-s3-motes-add` (cyan 6 px motes on the zixx
  idle stage). At native 240p the additive motes read as GLOW — they brighten
  what they sit on (white tip over rose sky, luminous cyan over dark
  terrain); opaque motes read as flat confetti everywhere.
  `evidence/motes{,-add}-tailcrop.png` (6x nearest crops).
- LESSON for the mana tables: full-brightness cyan ADDED over bright pink
  saturates all three channels -> white. Additive kind colours must sit
  UNDER the channel ceiling (Direction 30's law, now proven on particles).
  The ten emitter tables will carry calm additive colours.
- Placement lore for later diagnostics: the zixx idle stage sits on the
  bump-crown at ~8.8 m; the fixed non-orbit camera maps the arch to roughly
  world x -700..+100, and the S bows in x-z, so "in front of the body" needs
  z <= -900 mm. Two mote fans burned an hour learning this; authored fans
  are the fast instrument.

## Spike S4 — VERDICT: PASS (with two real discoveries)

- `make_ball()` written in `unnamed02_model.h` (UV sphere at half-step cosine
  ring spacing, both caps); 8-bone rig in `unnamed02_rig.h`; maths helpers
  (the one sanctioned copy) + Rig + still clip in `unnamed02_clips.h`;
  composition in `unnamed02.h`; species/tilt/facing/slot wiring for
  kUnnamed02 through render_scene (floats: TiltMode::kNone, no travel,
  no bulk, no gibs). Diagnostic subjects u02-s4-{side,front,tq,wire,ids,
  stage,unlit}.
- DISCOVERY 1 — THE PROJECTION IS ANISOTROPIC: isotropic NDC through the
  384x240 viewport paints 1 m of world X across ~1.66x the pixels of 1 m of
  world Y (project_vertex maps +-1 NDC to 192 px x 120 px). Every world
  sphere renders as a 1.66:1 wide ellipse; Zixxtrixx (a long tube authored
  by eye under this projection) never showed it. Countered with the
  `kVStretchPm` knob (1660): vertical form dimensions are authored in
  on-screen proportions and stretched at build. The balls now READ as balls
  from side/front/tq (evidence/s4-{side,front}-balls.png).
- DISCOVERY 2 — A LATENT ENGINE BUG in build_ring_part: a bottom cap's apex
  vertex sits between two rings in the vertex list, and the next band derived
  its lower-ring base as `hi - (n + dup)` — off by one whenever a capped part
  kept a second band in the same meshlet. Zixxtrixx's 28-side rings always
  split first (masked); the watchdog's low-segment capped parts hit it from
  day one (a stray wedge at the rear hip — evidence/s4-watchdog-cap-
  before-after.png); creature 02's 16-seg ball opened 4 edges per bottom cap.
  FIXED by tracking the base explicitly; the two watchdog CRC pins re-pinned
  loudly (0xE47538C1 / 0xC06ACA1B); zixx bank identity proof in
  evidence-s4-bank-identity.txt (render in flight as this is written).
- u02_meshcheck.cpp COMMITTED (manifold edges position-keyed, seam law,
  degenerate faces): CLEAN — 10 meshlets, 772 tris.
- Segment taper to the poles produced a visible zipper sliver on the face at
  240p; segments are now uniform (352-tri body ball; the ~80-tri saving was
  not worth a facial blemish).
- Also observed: ZIXX_EXP=celmain renders any UNTEXTURED creature black
  (the untextured-gouraud toon path rescales pre-lit 255-scale lanes to a
  65536-scale band level then >>16 -> ~1 count). Latent engine defect, not
  fixed here (creature 02 ships textured; grey form iteration uses
  ZIXX_LIGHT only). Logged for the hardware/reference lane.
- A recurring "beam" artifact chased for over an hour turned out to be the
  documented STALE-ARTIFACT traps (stale frame dirs + stale binaries) plus
  the REAL cap bug above. New personal law: `rm -rf` the subject dir before
  every re-render, and never judge a fresh question on an old directory.

## Spike S5 — VERDICT: PASS

- `unnamed02_fx.h`: the centre glow as a TWO-LAYER shared-ramp splat over
  the engine's own §4 halo_atmo corona bake — outer aura (46 px, gain 380pm)
  drawn BEFORE the compose, depth-tested against the conduit centre's own
  1/w (paints over sky AND the terrain behind it — the star path's
  kStarDepth law could not); inner core (13 px, gain 420pm) drawn AFTER the
  compose with no depth test — the light shines through the belly.
- Three glows (one real + two phantom conduit centres), exactly ONE 64-entry
  ramp build per frame by construction; zero interaction with the <=2
  sun/flare caps (never a ComposeLight, never a flare slot).
- Read at 2x: a soft pink aura rimming the grey body with a violet core
  inside it — the light lives in the belly (evidence/s5-glow-trio.png).
  Radius/gain are knobs; final values chosen in scene against pink pigment
  at the fx milestone.

## S4 aftermath — the identity gate caught two of my own mistakes

1. The UNGATED cap fix moved all 22 zixx bank CRCs (the tapered nose rings
   hit the same latent bug): approved art does not change silently, so the
   fix became OPT-IN (`RingPart::cap_base_fix`, default false = legacy
   bit-frozen layout; only creature-02 parts set it). Watchdog pins reverted
   with a NOTE. The ungated fix (and the watchdog's stray hip wedge it cures)
   is reported to the owner as an available improvement.
2. A careless sed for the u02 camera changed EVERY `cam_k = 300000` in
   zhao_reel.cpp — ten subjects, five of them bank clips (damage, hit, look,
   taunt, slow-taunt). Caught by the bank comparison, restored, re-verified:
   all five back to baseline CRCs exactly. Lesson: never sed a value that is
   not unique to the thing being edited.
   Final proof: evidence-s4-bank-identity.txt (17 IDENTICAL from the gated
   bank render + the five re-verified against baseline after the cam_k
   restore; one full ALL-IDENTICAL bank run gates the publish).

## FORM MILESTONE — the grey creature stands (evidence/form-*.png)

- Teardrop body (taper + lean knobs), the flat antenna loop as ONE
  straight-bound chain FOLDED by pose (loop_rest, the zixx tail_rest pattern
  — the drawn loop shape is a stance, per the house law), corners at the
  three hinge balls, both tube ends capped and buried in the body.
- The face: two faceted purple almond lenses (real geometry, 8-seg facets)
  angled outward in the front sheet's V, cyan four-point star pupils (two
  crossed blades on the pupil bones) standing proud of the lens.
- Side view shows the lens bulging OFF the body silhouette — the
  description sheet's "abstehendes Auge" oblique sketch read.
- u02-meshcheck CLEAN: 20 meshlets, 1444 tris (budget ~1300; the loop caps
  and uniform segments spent a little up — the read earns it).
- Upheaval package scaffolded-and-merged: CREATURE.json (clearance
  contract, projection note, honest source pointers), SPEC.md (the plan
  distilled + spike verdicts).
