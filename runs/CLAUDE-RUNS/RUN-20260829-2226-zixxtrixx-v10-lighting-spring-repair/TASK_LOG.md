# Task Log: RUN-20260829-2226 - Zixxtrixx v10 lighting and rigid-spring repair

**Created:** 2026-08-29 22:26 UTC+02:00
**Status:** In Progress
**Working Directory:** `runs/CLAUDE-RUNS/RUN-20260829-2226-zixxtrixx-v10-lighting-spring-repair/`

---

## Objective

Preserve the approved v9 whole-body proportions and rendering style while repairing systemic light assignment/flicker and recurring malformed eye polygons at their roots, reconnecting tail fins, making constrained face changes, replacing the rear-curling spring with a shared rigid-S top-down compression, validating every production frame, and publishing one fully verified noindex v10 generation with v9 archived intact.

---

## Tracker Status

| Task | Scope | Status |
|---|---|---|
| #17 | setup and durable direction | Complete — `0d1f45e` / `f75306f`, pushed |
| #18 | lighting and eye artifacts | Complete — structural roots fixed in `1dd01a4`, pushed |
| #19 | constrained face and fins | Complete — `32ca883`, pushed |
| #20 | shared rigid-S spring | Complete — `065b732`, pushed |
| #21 | bounded risk-based validation | In progress — claimed |
| #22 | publish and handoff | Pending |

Only one task is active at a time. A task becomes complete only after its evidence, gates, logical commit and push are complete.

---

## Progress Timeline

### 2026-08-29 22:26 UTC+02:00 - Task started

- Generated Run ID `RUN-20260829-2226`.
- Owner assigned one sole implementation/modelling lane; no subagents may be spawned.
- Claimed tracker task #17; all later tasks remain pending.
- Recorded isolated clone contract:
  - zhaozhou clone `work/zhaozhou-v10`, branch `zixxtrixx-v10-lighting-spring`, base `54e74372367fa389d74d7bf74125352ae7bc6bf7`.
  - Upheaval clone `work/Upheaval-v10`, branch `zixxtrixx-v10-lighting-spring`, base `d97f7a424c9015c9ffc128406760fa5ccf370964`.
- Shared checkouts, hardware/migration lane and shared build/render outputs are forbidden. No worktree or Sacengine use; no `cmake --build`; direct lane-local builds only.

### 2026-08-29 22:41 UTC+02:00 - Prerequisite reading complete

- Read both clone-local `CLAUDE.md` files.
- Read every durable `OWNER-DIRECTION-*.md` through #9 in `Upheaval/creature/Zixxtrixx/`.
- Read `CREATURE-ASSET-OWNERSHIP-ARCHITECTURE.md`; migration remains docs-only and frozen from production creature edits until parent handoff after v10.
- Compared zhaozhou reports after the v9 closeout commit and read the only newer report, `reports/HARDWARE-LANE-STATUS.md`; it confirms the migration branch is separate and this lane must not release it.
- Read the complete v9 `TASK_LOG.md`, `SPEC_v1.md`, form/eye, motion/spring, impact, balance, taunt/fall and final production evidence summaries.
- Key inherited baseline: immutable v9 source `65350e04b4cabd357a28296f69713cd0c9b2a880`; integrated v9 Upheaval main `d97f7a424c9015c9ffc128406760fa5ccf370964`; published deployment `https://19a9bf54.upheaval.pages.dev`; 21 clips / 5,744 decoded cel-main frames.
- Wrote durable `Upheaval/creature/Zixxtrixx/OWNER-DIRECTION-10-2026-08-29.md` faithfully recording the approved-proportion freeze, systemic lighting/eye diagnosis, constrained face/fins and rigid-S spring requirements.
- Committed and pushed the durable direction early on the Upheaval v10 branch as `f75306f`.
- Filled v10 SPEC and TASK_LOG with ordered tracker mapping, evidence requirements and explicit failed approaches not to retry.

### 2026-08-29 22:48 UTC+02:00 - Setup pushed; lighting/eye diagnosis claimed

- Committed and pushed the zhaozhou run setup as `0d1f45e`.
- Task #17 is complete only after both setup commits reached their remote branches.
- Claimed task #18. No art constants or creature/renderer source have been changed.
- Next action is a clean direct-build v9 baseline followed by fixed-camera every-frame reproduction and root-cause isolation.

### 2026-08-29 22:55 UTC+02:00 - Owner bounded the validation campaign

- Stopped before launching the baseline build or any render; no unnecessary validation work had started.
- Updated durable owner direction #10, SPEC and this log: whole-catalog exhaustive review is explicitly rejected unless an actual regression signal justifies escalation.
- Defined named budget `V10-BUDGET-1` before running checks. It permits:
  1. at most two known lighting reproductions × two fixed cameras × four diagnostic/presentation modes;
  2. at most two known eye reproductions × two fixed cameras × normal/cel-main, with every-frame inspection only in bounded failure windows;
  3. one representative normal/deforming temporal trace plus the reused v9 mesh/reel checks after the root fix;
  4. every-frame review for only the four specifically broken spring subjects plus bounded attack contact/landing windows;
  5. one representative deterministic cel-main pair per changed milestone;
  6. final media manifest/count/poster and bounded start/middle/end decode sampling, with exhaustive catalogue work gated on a recorded regression trigger.
- Every executed check must be entered in a validation ledger with its acceptance question, reason, result and stop/escalation decision. Implementation and native-resolution art iteration stay dominant.
- Committed and pushed the corrected durable owner direction in Upheaval as `c8425f7`.

### 2026-08-30 - Task #18 structural roots fixed and pushed

- Used only the bounded knockdown eye window, death2 lighting reproduction, one badness-ranked temporal trace and existing structural gate; no catalogue rerender was triggered.
- Lighting root 1: v9 blended two already-clamped per-bone Lambert scalars. On a mixed ring this is not the deformed surface normal. Slot 28's selected surface reached a 19,000-Q16 disagreement, including ticks where legacy injected light while the normalized blended normal was back-facing. Production now transforms and blends the bind normal, normalizes once, then clamps one dot product through shared `skin_normal_lambert`.
- Lighting root 2: ring zipper triangles are inward-wound, while compiled smooth normals deliberately negate that cross to point outward. V9's 20% posed-face lane shaded the original inward order, opposing the 80% smooth lane. It suppressed genuine highlights and injected back-face response; toon thresholds made those contradictions read as arbitrary flicker. Face lighting now swaps B/C for outward evaluation without changing raster topology, rig constants or cel thresholds.
- Eye root: triangle-ID rendering mapped the frame-30 yellow shard to micro-rung head meshlet 0 triangles 81/89. The committed triangle probe proved their V lanes were 25..33 in the painted-eye domain. Generic micro decimation had removed alternating longitudinal rings and halved 22 radial sides to 11, reducing the eye boundary to isolated zipper fans at grazing poses. Named per-part micro controls now preserve both axes only for the painted head; the body still decimates normally.
- Causal isolation: fullbright, normal visualization and replacing the pupil tube with an open strip did not remove the shard; head mip limits 2 and 0 changed filtering but did not remove it. Both experiments were reverted. With original v9 pupil topology and full atlas mips restored, preserving head micro topology removed the shard and restored one coherent oval across bounded frames 22..41.
- Visual acceptance: death2's badness-ranked frames are substantially brighter and surface-coherent without global brightening; the knockdown frame-30 closeup and 22..41 sheet show no detached eye fan.
- Clean direct all rebuild passed after each struct-layout isolation; final meshcheck: 1,776 vertices, 1,106 shared-position groups, zero disagreeing binds, zero seam split, `meshcheck: OK`.
- Determinism D01: 185 knockdown files byte-identical, aggregate SHA-256 `3a542ba6eff022ed24d9953b3ee3718a5d6d6b360befb60fabcad2be6904700e`.
- Committed and pushed the structural repair and reusable diagnostics as `1dd01a4`. Task #18 complete; task #19 claimed.

### 2026-08-30 - Task #19 constrained face and fin repair complete

- Preserved every approved body taper, radius and centreline control. Both eye discs moved four atlas rows noseward; only their local support window moved from stations 3..6 to 2..5, and the moving-pupil pivot moved from station 5 to 4. Fixed side/front comparisons show a clear noseward move without a global skull or tube change.
- The two moving orange stripes retain their original shared gaze curves and boundary-to-boundary construction. Representative rest, vertical-extreme, diagonal and return frames show each stripe continuously reaching the painted ink boundary; no floating endpoint or pupil-motion loss was introduced.
- Moderately enlarged the mouth from three short rows to four unequal hand-wobbled rows. The fixed frontal comparison shows a wider readable slit while remaining far below the retired quarter-head grin.
- Fin attachment root: each separately capped leaf began at the already-capped body endpoint, with centres offset 56 mm on a tail tip only about that whole width. This left almost no overlap and exposed independently shaded closures/gaps as black cuts. V10 removes the blade root caps, buries open roots 72 mm inside the body shell, narrows their root offsets, converges both leaves into a shared fork-bound first ring, and blends the shoulders from fork to blade before normal fin flex. The approved tail stem and blade length/tip location remain unchanged.
- Visual acceptance used fixed close side/front, gameplay distance, 12 representative orbit views, eight idle phases, and targeted high-risk rear views—not catalogue review. The fork now reads as one planted Y across views, distances and idle deformation; no blade appears detached.
- Structural gate: 1,774 vertices, 1,098 shared-position groups, zero disagreeing binds, zero seam split in every registered clip, `meshcheck: OK`. A first 36 mm root offset accidentally made mirrored shoulder vertices exactly coincident with different blade binds and the gate correctly rejected it; the authored 34 mm overlap removes that false seam while retaining the visual root.
- Determinism D01 #19: side plus pupil-proof, 320/320 true cel-main files byte-identical; aggregate SHA-256 `d40fe6acc03842fb46ea78163c16f73c4e633d012e555e2abdd0ae161df27966`.
- Committed and pushed the constrained source/media-generation milestone as `32ca883`. Task #19 complete; task #20 claimed. Per coordinator direction, remote `origin/main` is currently `ccd31d9` with one unrelated hardware commit; this lane does not integrate it until task #22.

### 2026-08-30 - Task #20 shared rigid-S spring complete

- Replaced the v9 lateral concertina/rear curl with a separately authored side-plane compression profile. The profile retains the signature doubled-back S through explicit near-half-turn middle directions; the yaw lane was deleted, so the back and tail cannot coil sideways during preload.
- The entire silhouette descends from above under one named quadratic root drop. A smoothstepped profile arrives slightly ahead of root contact, avoiding a transition sweep through terrain. The deepest authored pose is almost flat, puts the head onto the ground, keeps the rear above its permitted floor, and retains the tube's volume.
- Separated grounded spring release from airborne wheel gather. Jump-one and jump-multi release the complete S while grounded, then form the approved wheel during the first five airborne keys. Base attack holds the intact S through key 22 before a six-key wheel gather; the accepted mature flight, turn counts, spear, impact and recovery paths remain unchanged.
- Programmable salto variants now use `zixx_plan_spring_amount` as the single body/root timing source. Their exact S is held for one key before the plan enters flight, eliminating the previous mismatched-root ground curl and half-tick terrain dip.
- Every frame was inspected for the four authorised subjects only: jump-one (143), jump-multi (143), salto-six (265), and salto-nine (367), plus the bounded close side/top spring views and native launch window. The whole S lowers coherently, the head meets the terrain, the rear never rolls or curls, release returns through the intact S, and the mature airborne wheel remains stable.
- Committed posed-geometry evidence passes every key and midpoint: deepest centre/lateral span `99/10 mm`, head/rear surface `-3/-10 mm`, declared bite `-23 mm`; jump contacts `-33 mm`; six/nine outside-phase minima `-23 mm`. Meshcheck remains 1,774 vertices, 1,098 shared-position groups, zero disagreeing binds and zero seam split.
- Reused only the affected choreography/planner/target/limit gates. Choreography retains 2 mm worst spin-migration error; planner preserves the 240-key golden preset and exact intercepts; all target interaction windows and the nine-salto native camera/LOD/range check pass.
- D01 true cel-main repeat: jump-one plus salto-nine, 510/510 files byte-identical, aggregate SHA-256 `afa5ab2b1b7ad598d6c9e7581030fe67876e8a1f507a9d52a1953dfcafaf60ad`.
- Committed and pushed the spring source and committed 3D probe as `065b732`. Task #20 complete; task #21 claimed. No catalogue escalation signal was found.

---

## Validation Ledger — `V10-BUDGET-1`

Budget set before any build/render. Each row must answer one question and end with stop or a concrete escalation trigger.

| Check ID | Acceptance question / why it exists | Bounded input | Result | Decision |
|---|---|---|---|---|
| B01 | Does current source direct-build cleanly, proving later output is not stale? | One clean direct `all` build | PASS — all 29 shared objects rebuilt; reel/cel/frozen-pupil and 8 probes linked | STOP — baseline build question answered |
| L01 | Which shared stage causes apparently random illumination assignment/flicker? | Death2 cel-main badness-ranked frames plus slot-28 mixed-normal/face trace | PASS — separately clamped influence responses disagreed by 19,000 Q16; inward posed-face response opposed outward smooth normals | STOP — two shared structural roots explain and remove the patches; no brightness/threshold changes |
| L02 | Does lighting remain temporally attached to surfaces after the fix? | One deforming surface at 60 Hz, 192 ticks, inward/outward face comparator | PASS — normalized response is continuous (`max_jump=2228`) and outward face response changes on the matching side; final render is coherent | STOP — no unexplained discontinuity or escalation signal |
| E01 | What structurally creates malformed eye triangles? | Knockdown frame 30/window 22..41; unlit/normals/wire/triangle IDs; exact triangle dump | PASS — micro head triangles 81/89, UV V=25..33, proved coarse eye-bearing shell fans; preserving head rings/sides removes shard | STOP — exact owner and causal repair proven |
| E02 | Does the eye fix generalize without catalogue exhaustion? | Window 22..41, badness-ranked death2, reused meshcheck | PASS — coherent eye boundary throughout bounded window; zero bind/seam/stretch faults; original pupil and full mips retained | STOP — no surviving artifact; no full-catalog trigger |
| F01 | Do the constrained face and fin changes satisfy direction without altering approved whole-body form? | Fixed side/front, gameplay still, pupil extrema, 12 orbit views, 8 idle phases, high-risk rear samples, meshcheck | PASS — eyes/support/pupils moved together noseward; mouth moderately larger; fork is a planted Y with no cap seam/detachment; body controls untouched; zero bind/seam faults | STOP — each task #19 acceptance question answered; no catalogue escalation signal |
| S01 | Does corrected spring lower the rigid S from the top with head contact and no rear curl? | Every frame of jump-one, jump-multi, salto-six, salto-nine; bounded close side/top and launch/contact windows; committed posed-vertex probe | PASS — no yaw/concertina remains; whole S descends to a 99 mm side span with head/rear surfaces -3/-10 mm; it releases intact before airborne wheel gather; all four every-frame sheets accepted | STOP — the named spring question is answered; no unrelated catalogue escalation |
| D01 | Are changed true cel-main outputs deterministic? | One representative pair per changed milestone | PASS #18 — 185/185 knockdown files byte-identical, SHA-256 `3a542ba6...4700e`; PASS #19 — 320/320 side+pupil files byte-identical, SHA-256 `d40fe6ac...27966`; PASS #20 — 510/510 jump-one+salto-nine files byte-identical, SHA-256 `afa5ab2b...f60ad` | STOP for #20; one final bounded media/site validation remains under #22 |
| M01 | Is final media/site structurally valid? | Manifest/count/poster plus bounded start/middle/end decode and desktop/narrow interaction | Deferred to #22 | Exhaustive decode only on mismatch/decode/browser signal |

---

## Subagent Spawns

None. Owner explicitly requires this to remain a sole implementation/modelling lane.

---

## Files Created

- `Upheaval/creature/Zixxtrixx/OWNER-DIRECTION-10-2026-08-29.md`
- `runs/CLAUDE-RUNS/RUN-20260829-2226-zixxtrixx-v10-lighting-spring-repair/SPEC_v1.md`
- `runs/CLAUDE-RUNS/RUN-20260829-2226-zixxtrixx-v10-lighting-spring-repair/TASK_LOG.md`
- `runs/CLAUDE-RUNS/RUN-20260829-2226-zixxtrixx-v10-lighting-spring-repair/evidence/` for committed v10 diagnostics and acceptance output.

---

## Decisions Made

- Approved v9 whole-body proportions are frozen; no radius or centreline redesign is authorised.
- Lighting and malformed-eye artifacts are diagnosed and repaired before any face, fin or spring art changes.
- Normal Gouraud and true cel-main share root-cause investigation; thresholds/global brightness cannot mask the defect.
- Eye artifact acceptance is bounded to the known knockdown reproduction plus representative badness-ranked samples and reusable structural gates; catalogue escalation requires a concrete surviving artifact.
- The spring is a mostly rigid-S top-down descent with head ground contact and no rear roll/curl, shared by all jumps/saltos.
- Targeted every-frame native visual review is authoritative for broken spring clips and bounded known defect reproductions; diagnostics compare and rank but do not author art.
- For unaffected catalogue coverage, representative/badness-ranked samples plus the smallest relevant reused v9 gates replace exhaustive review.
- Validation stops when its recorded acceptance question is answered; whole-catalog rerender/redecode needs a concrete logged regression trigger.
- V9 archive media and exact noindex contract are immutable inputs to v10 promotion.
- Tracker #23 is a separate post-v10 creature-authoring-blueprint lane blocked by #22. This lane will not build that blueprint. It will leave only a concise implementation-derived handoff in this TASK_LOG/evidence covering actual roots, reusable fixes, failed approaches, named controls and machinery paths; no extra validation or documentation project.

---

## Next Steps

1. Task #21: close the named bounded validation ledger using the already-rendered authorised windows and the smallest relevant inherited v9 gates; do not rerender the catalogue without a recorded regression signal.
2. Commit and push the validation closeout, then claim task #22.
3. Preserve/archive v9, promote v10 media, integrate both current mains without disturbing the hardware/migration lane, publish the finished noindex bestiary exactly once, verify production, and stop every job.
