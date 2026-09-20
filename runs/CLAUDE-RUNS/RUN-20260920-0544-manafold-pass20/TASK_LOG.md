# Task Log: RUN-20260920-0544 - [Describe objective here]

**Created:** 2026-09-20 05:44 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260920-0544-manafold-pass20/

---

## Objective


Manafold pass 20 from Owner Direction 21: (1) stop the rear connecting part leaving the body (a contact/attachment breach visible in Inspect; pass 19 damping was not enough and its gate missed it); (2) new authored kneading move where the middle-top ball (carrier B) sometimes dips low enough to become the lowest ball, on every animation; (3) the mana particles visibly react to that dip. Publish when finished.

---

## Progress Timeline

### 2026-09-20 05:44 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260920-0544
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

### 2026-09-20 05:44 - Started
- Branches `manafold-pass20` created in both repos from the production-verified pass-19 mains (Zhaozhou `4b3d4576`, Upheaval `d7086d2e`).
- Owner direction recorded durably: `Upheaval/creature/Manafold/OWNER-DIRECTION-21-2026-09-20.md`.
- One Opus worker implements; the coordinator organizes. No Qwen (another agent may still hold it; the owner restricted it in pass 19 and has not released it). GPT/Codex quota is still exhausted until 2026-09-25.
- Item 1 is treated as a defect in BOTH the motion and the gate: pass 19 shipped with 128/128 green while the socket still emerged, so the burial/attachment gate is part of the fault.

### 2026-09-20 ~06:10 - OWNER CORRECTION to item 1 (received mid-orientation)
Verbatim: *"the rear doesn't leave the body, but it rips a big piece out and it
stretches too much, which leads me to conclude there's too much motion in the
back nodule."*

Re-scope: item 1 is **not** an emergence/burial breach. No "socket left the
body" gate, no burial-threshold chase. The fault is **excess motion in the back
nodule (End carrier / rear ball)** dragging the body surface: a big piece reads
as ripped out and the skin stretches too far.
- Look for the stretch signature: skin weights spanning the socket, vertices
  shared between rear chain and body being pulled, a long swell run stretching
  rather than bending, pass 19's arm-following frame adding range, plus any
  remaining authored/oscillating rear authority.
- Fix = reduce the back nodule's motion range and/or how far its influence
  reaches into the body. Keep it alive (Direction 20 "a bit wiggly").
- Gate = per-frame surface stretch / attachment strain in the socket region
  (bounded edge-length / triangle-area change over body+socket vertices) plus a
  bound on the rear nodule's own motion range, with a fired positive control
  reproducing today's rip. Must state which pass-19 checks were blind to stretch.
- Inspect stays the primary witness; before/after at native and 4x.

Items 2 and 3 unchanged. Continuing from orientation, not restarting.

### Orientation findings so far (for P20-DIAGNOSIS.md)
- `antenna_knead` (manafold_clips.h:2368) is already the **always-on** layer on
  every performing clip. It is where the shared knead lives.
- Item 2's reference mechanism is Direction 16's **CROWN SHUFFLE**:
  `taunt3_order_pose`/`taunt3_order_target` (manafold_clips.h:1868-1950) with
  `kTaunt3OrderRank[4][3]` + High/Mid/Low mm tables (manafold_art.h:3526-3567).
  Its comment states the intent exactly: "Four held A/B/C rankings give every
  free carrier top and bottom ownership." That is already "B becomes lowest".
- The single production consumption point for carrier heights is
  `swallow_nodules(g, swal[5], lean_pm)` (manafold_clips.h:1824), described as
  keeping everything "under the same F/A/B/C/E public mute and attachment law".

### 2026-09-20 - Pass 20 closed
**Nothing ships on by default; the bank is byte-identical to pre-pass.**

- **Item 1 (rip):** root cause found and named -- ARC vs CHORD. The rear span's
  rest length `kRearSocketFromCMm` is an arc (1010 mm) while
  `finalize_rear_follow` measures a chord, so a band that should BOW when the
  loop closes is told to SHORTEN, by up to 662 mm (66% of itself). The skin
  folds to 0.129 of rest length at ring 49. Three repairs built, measured and
  rejected (span travel limit, carrier calm, deep bias) -- all recorded with
  their ladders as committed negative controls.
- **Why pass 19 missed it:** every rear metric is built from ring CENTROIDS,
  which cancel a surface fold. No leg of the 128-leg matrix measured a posed
  surface at all. New R4 STRAIN gate (mask 0x8) fixes that, with two floors so
  it cannot be read as blessing the defect, and a fired control.
- **Items 2+3 (dip, particles):** built, gated (R5 DIP, mask 0x10, fired
  control), C2, exact loop seam, generalised from Taunt III's crown shuffle as
  instructed. SHIP OFF: enabling the dip reds three mspan legs at ANY strength,
  including the signed-span contract. Enable with `ZHAO_U02_KNEAD_DIP_PM=1000`.
- **All 11 gate normals green.** Renderer MD5 `6fe99845defe8ab0f9b49b73d86b4cc8`.
- **Not done:** the 22-subject bank, encode, merge, deploy. Correctly so --
  there is no visible change to publish.
- **Found en route:** the pass-19 recorded CRCs do not reproduce in this tree
  (pre-edit binary gives hover 0xA2D0E051 vs P19's 0x40E1DBF1). Predates this
  pass. Chase before the next bank render.

### Gate matrix: 140/140 PASS, 0 FAIL
Including `n-mrear-dip` (R5 judged with the dip enabled), the two new controls
(`--fail-rear-strain` mask 0x9 declared, `--fail-no-dip` mask 0x10) and nine new
strict selectors. The pass-19 rear controls' declared masks were updated
honestly: `--fail-rear-frame` 0x3 -> 0xB, `--fail-rear-joint` 0x2 -> 0xA,
because those mutations genuinely strain the skin too -- a new true category,
not a moved goalpost.

### 2026-09-20 - REPAIR PACKET: the rip is fixed
- **Item 1 REPAIRED at the root.** `kRearSocketFromCMm` is an ARC length and
  `finalize_rear_follow` measured a CHORD; the band now BOWS on a circular arc
  instead of shortening. Closure preserved by construction (zero displacement at
  s=0, exactly the old endpoint at s=L); degenerates to the straight band as the
  chord approaches the arc; only the slack side bows. Integer throughout.
  **Worst rear rail strain 0.129 -> 0.692**, clearing R4's 0.50 target floor, so
  the declared OPEN BREACH is gone. Looked at: 4x Inspect f380 (splayed wedge ->
  clean even tube) and 6x f158/f160 (pinched step -> rounded shoulder).
- **Onset blend and turn cap: both tried, both worse, both shipped inert** with
  their ladders in the source.
- **Gates re-expressed, not weakened:** R1 centreline 60->140 (above the
  repaired 113, below v18's 171, control still fires); R4 hand-off demoted to
  reported (it measures curvature once the band bends); mspan G5 now checks the
  helpers against the PRODUCTION writer instead of one hard-coded formula; mspan
  G6 now bounds the consecutive-step turn and the pinch instead of projecting
  onto a straight axis. R4's regression floor 0.12 -> 0.40 and its control is now
  the DEFECT itself (`REAR_BOW=legacy`), firing mask 0x8 alone.
  *The old R4 control silently stopped firing once the bow overwrote the
  helpers; the matrix caught it as rc=0 exp=1.*
- **Dip verdict (b).** The bow removed the closure and jerk objections; mspan's
  signed bound / free-span margin remains (240 breaches at depth 300, 11 at 140).
  That envelope keeps the antenna attached and is not ours to widen for our own
  feature. Ships OFF. Fix is to redistribute across A/B/C as Taunt III's crown
  shuffle does, which is authoring, not gate-widening.
- **Method correction:** the previous packet's "breaks at any strength" ladder
  was run with an env knob mspan never parsed. An env control is only a control
  in a binary that reads it.
- **Legacy toggle byte-exact 3/3** against the authoritative
  P19-FINAL-BANK-INTEGRITY values. Open item 3 (CRC discrepancy) is CLOSED:
  P19-IMPLEMENTATION's numbers were pre-review.

### Gate matrix after the repair: 143/143 PASS, 0 FAIL
All 11 normals green, `n-mrear-dip` green, `--fail-rear-strain` now firing 0x8
on the real defect, and four new bow selectors returning RC 2.

### 2026-09-20 - PACKET 3: the dip ships; gate-change ledger written
- **Dip SHIPS ON** at `kKneadDipGainPm = 550`, ranking `{0,-1,0}` over the
  crown's own High/Mid/Low tables, reference depth 325 (B's authored low).
  All 11 gate normals green. Looked at on Inspect f200/f230/f260: the loop's
  top-middle presses down and the loop squeezes -- a knead, not a spasm.
- **B does NOT reach strictly lowest, and distribution is not the fix.**
  Re-authored across all three carriers as instructed and measured seven
  configurations: every one lands on the same C-E signed span (-687..+529).
  What stretches that span is B's own descent -- not which other carriers move,
  not the fold share (fold 0 is WORSE), not C (removing C entirely changes
  nothing). `kSpanStretchMaxPm[C-E]` is +440 and the bank already sits at +305
  before the dip exists: ~135 pm of headroom against ~225 needed. A's rise just
  moves the breach to F-A. Full table in P20-IMPLEMENTATION.md 2.5.
- **Trick excluded explicitly** (`kKneadDipClipPm[13] = 0`), with slot 7 and
  Taunt III.
- **P20-GATE-CHANGES.md** written for the reviewer: all six threshold/status
  changes with before/after/why-not-a-loosening/fired control, plus the three
  broken-instrument cases (R4's control that stopped firing, the inert
  REAR_BOW=legacy in the reel, the inert dip ladders in mspan/mprobe).
- **Live-history legs re-run cleanly** and the whole matrix re-run from scratch
  rather than patched, so it is a receipt and not a reconstruction.

### Final gate matrix: 144/144 PASS, 0 FAIL
Re-run from scratch with the dip shipping -- not patched, not reconstructed.
All three live-history legs pass on clean renders (the earlier FAILs were my own
orphan-cleanup kill landing mid-render, not a regression). Nothing else in the
matrix was affected by that cleanup: every other leg is a fresh run in this
same invocation.

### 2026-09-20 - PACKET 4: dedicated vertical built; STOP reported
- Option (a) built: `kKneadDipCarryCancelPm`. The nodule solve is a sequential
  CARRIED solve, so lowering B lowers C, and C starts the return arm -- that
  carry (not which carriers move) is why packet 3's seven rankings all landed on
  the same C-E figure. B goes down by d, C is handed +d back, and the reaction is
  absorbed in the A..C stretch through the existing signed-span helpers.
- It works: B strictly lowest 0/21 -> 9/21 at the same gain, 13/21 at 650,
  17/21 at 1000, and at 650 THE ATTACHMENT SPAN IS INSIDE ITS BOUND (C-E 438 vs
  440) with the only breaches on A-B (+2 pm) and B-C (-12 pm), both interior.
- It is still not enough: clearing those two interior bounds exposes two other
  mspan legs (carrier jerk; SpanDeltaE/RearSocket meet at End) that fail at
  every amplitude down to gain 350. All 21 needs ~gain 1000, i.e. C-E at 524 pm
  against 440 -- a 19% overshoot of the attachment bound. NOT TAKEN.
- SHIPPING: gain 550, cancel 0, all gates green, B not strictly lowest anywhere.
  Reported as NOT DONE. Ledger in P20-DIP-STOP.md.
- kSpanStretchMaxPm / kSpanCompactionMinPm / kSpanMinRunMm all exactly as pass 19
  left them. P20-GATE-CHANGES.md 7 records the considered-and-rejected widening.
- Matrix 144/144 in ONE invocation.

### 2026-09-20 - ARCHITECT PACKET: the solver change (P20-SOLVER-ARCHITECTURE.md)
- Read-only design for the ledger's option 2. Finding from the committed
  constants: the shipped dip lowers B's RANK mainly by HOISTING C (fold share
  -2000 pm flips B's fold, sending the B->C span upward ~500 mm est.), B's own
  descent is clamped at kNoduleOffsetMaxMm[1]=320, and the carry cancel was
  applied in offset space against a chain that had ROTATED. The attachment cost
  was the mechanism, not the gesture.
- Chosen mechanism: THE DENT -- a pinned two-bone re-fold of the A-B-C triangle.
  A and C keep world position AND frame; B is pressed along its perpendicular
  to the A-C chord through to its mirror (s=2000), which is stretch-free. Writes
  only HingeA/HingeB aims (existing nodule_aim), span deltas 1 and 2, and a
  world-pin of HingeC. F-A and C-E have no term in it.
- Falsifying experiment: Inspect only, depth 2000, mspan --csv + mrear --dip;
  any F-A change, >1 mm C-E change, crossing compaction > ~170 pm ducked, or
  R5 not strictly lowest -> falsified. Ten minutes, no render.
- Gates: new mspan G10 DENT PIN (+ --fail-dent-pin = the carried solver at
  gain 1000), R5 promoted to hard, R4 front-window floor (+ --fail-dent-overfold),
  CRC identity legs. No bound changed. kSpanStretchMaxPm comment at
  manafold_art.h:2337-2353 contradicts its array and must be repaired.

### 2026-09-20 - PACKET 5: THE DENT built; §9 experiment FALSIFIED on 2 of 4
- Steps 1-2 of the architect's order built exactly as specified. Off path is
  bytes (4/4), all 11 gate normals green, and BOTH identity controls pass:
  under `dent`, Taunt III and Still are byte-identical while Inspect changes.
- One implementation bug found and fixed BEFORE judging: the ambient duck
  divided by 1000000 instead of 1000, so at full envelope it scaled the ambient
  by 999/1000 -- a knob that looked wired and did nothing. Judging the design
  through it would have falsified it for the wrong reason.
- **Criterion 1 (F-A unchanged): PASSES.** -198..+199 in every configuration.
- **Criterion 2 (C-E within 1 mm): FALSIFIED.** With the duck off the pin leaks
  +19 mm / -11 mm. With the duck on, C-E sits at or below its no-dip values --
  but that is the duck compensating, not the pin holding.
- **Criterion 3 (interior compaction ~170 pm): FALSIFIED, ~3x.** A-B -365 pm
  (bound -330), B-C -482 pm (bound -430), ducked. The rest-pose estimate of
  ~102 mm at the crossing is far under the posed reality.
- **Criterion 4 (R5 strictly lowest on Inspect at s=2000): PASSES.** And
  bank-wide the dent reaches B-lowest on 13/21 at s=2000, 19/21 at s=3000 --
  the first mechanism this pass that makes B genuinely the lowest ball.
- STOPPED as instructed; no alternative mechanism improvised. Report with all
  numbers in P20-DENT-EXPERIMENT.md. Dent ships OFF; shipping bank unchanged.
- Repaired the stale kSpanStretchMaxPm comment (manafold_art.h:2337-2353) that
  described 490/-450 values the arrays never contained.

### 2026-09-20 - ARCHITECT PACKET, REVISION 2 (after P20-DENT-EXPERIMENT.md)
- The pin did NOT leak: the dent's chain walk (manafold_clips.h:835-842) reads
  each span's delta off the bone that STARTS the span; the closure walk and
  finalize_rear_follow read it off the bone that ENDS it (span_child[i] with
  kLoopArcMm[i]; set_span_delta(0) writes kBHingeA). Off by one on all three
  spans. Duck-1000/full-envelope zeroes every delta, both walks agree, and that
  row shows the pin holding. Duck-0 and mid-ramp (where the crossing lives)
  measured a misplaced triangle. Fix: ONE shared loop_walk, CRC-gated.
- The crossing deficit is real for a PLANAR press and grows as the loop closes.
  Revision 2 rotates B rigidly about the A-C chord instead (kKneadDentSwingPm):
  zero span change on every sample, same mirror endpoint. Extra depth for the
  clips that need it is a named overpress stretching only A-B/B-C under the
  full duck, inside 480/400. No bound changes; fallback named in R2.5.
- Next experiment: three render-free runs, central claim A-B/B-C <= 1 mm from
  no-dip with swing 1000 / duck 0.

### 2026-09-20 - PACKET 6: the walk, the swing, the look (P20-PACKET6-RESULT.md)
- ONE loop_walk. Factored out of the closure walk verbatim; the dent calls it
  for A (2 segments), B (3) and C + the entering frame (4). Off path is bytes:
  hover 0x79D3F0C5, inspect 0x0710E704, still 0x138FE8B0, taunt3 0xC81598AA.
- STALE BINARY TRAP, caught: build-direct.sh takes ONE target, so
  "build-direct.sh ... mspan mrear" built only mrear and the first old-vs-new
  comparison ran a spangate from before the fix. The tell was exactly the one
  CLAUDE.md names - old and new produced byte-identical CSVs after a change
  that had to move them. One target per invocation from here.
- Corrected vs old (dent=press, depth 2000, duck 0): C-E envelope -698..+324 ->
  -687..+304 (no-dip -687..+305), worst per-sample C-E delta -157 mm -> +21 mm,
  breaches 286 -> 255. The +19 mm leak WAS the walk. The 3x interior compaction
  was NOT: corrected it is worse at the extremes (A-B -217, B-C -256).
- The 21 mm residual is nodule_aim's own angular resolution (asin16 near its
  pole), measured with a committed probe (-DZHAO_P20_PINPROBE): C moves 38 mm
  L1 worst while B is reproduced to 3 mm. Renormalising the aim-touched quats
  changed nothing, which is how we know it is the angle, not the norm.
- THE SWING (kKneadDentSwingPm, 1000 = rigid circle, 0 = packet 5's press bit
  for bit). Central claim HOLDS: 0 bound breaches at every depth, F-A identical
  per sample, A-B 3 mm, B-C 5 mm, C-E envelope never exceeds no-dip.
- BUT: 55-80 deg angular step on carrier B at every swing setting (G9 ceiling
  8 deg; the press stays at the 7.59 deg baseline). Position step untouched, so
  it is a ROLL FLIP - nodule_aim's z-then-x aim is degenerate out of plane,
  which is where the swing sends B. R2.2 named this risk.
- Overpress buys nothing (R5 count unchanged) and costs 379 breaches at 1000.
  Stays 0.
- R5 vs depth (swing 1000, gain 550): 6/3/3/2/2 clips never lowest at depth
  1940/2200/2425/2700/3000 - so 18 of 21 at s ~ 1.0, not the s ~ 1.35 the
  design estimated.
- THE LOOK: at s >~ 1.4 the loop apex becomes a hard rectangular slab on both
  Inspect and Hover - and the PRESS does it too, so it is the fold, not the
  swing. At s ~ 1.0 it reads as a tube pressed down in the middle. Plates in
  P20-LOOKS/p20_depth_ladder.jpg, p20_hover.jpg, p20_w_ladder.jpg.
- GATE: mspan G11 WALK PAIRING + --fail-walk-pairing (fires: 4 of 5 segments
  mispaired, +750 mm, attributed 0x8000). Four new strict selectors. Dent depth
  range 4000 -> 6000 (a range, not a bound; no gate reads it). Nothing relaxed.
- Ship state unchanged: kKneadDipSolver = kCarried, dent OFF, bank bytes equal.
- Gate matrix 149/149 PASS, 0 FAIL (P20-RECEIPTS/gate-matrix.txt).

### 2026-09-20 - PACKET 7: the roll-stable aim, and the dip SHIPS
- shortest_arc_from_y + nodule_aim_rollstable: the minimal rotation from +Y to
  the target direction, so the aim leaves NO twist about the segment. Separate
  function; the production nodule solve keeps nodule_aim verbatim, so it is
  exact-off by construction.
- G9 worst angular step on a visible carrier: 55-80 deg (packet 6) -> 7.772 deg
  at depth 2200, against an 8 deg ceiling that was NOT moved. 2250 measures
  8.080 and is over, so 2200 is the deepest the ceiling admits. Accel 5.040 and
  jerk 5.000 are both below the no-dip bank's own 5.280 / 5.247.
- THE SLAB WAS THE ROLL FLIP, not the fold. With the roll-stable aim every rung
  of the ladder {2200, 2425, 2700} reads as a continuous kneading tube on
  Inspect and Hover. Packet 6's verdict is retracted with its cause named.
- SHIPPING: kKneadDipSolver = kDent, kKneadDentDepthPm = 2200, swing 1000,
  overpress 0. R5: 21 clips author a dip, 19 reach strictly lowest, 0 fail to
  return. The two misses are slot 15 (lab diagnostic, -191 mm) and slot 16
  (nodule-solo diagnostic, -74 mm); both are diagnostics. Slot 20 (blown) took
  the per-clip lever, kKneadDipClipPm[20] 750 -> 900, from -26 mm to lowest.
- Identity: SOLVER=carried reproduces the packet-5 bytes 4/4; adding
  REAR_BOW=legacy reproduces b7c096c2 3/3. Still and Taunt III are unchanged by
  the dent shipping because neither authors a dip. Both identity checks are now
  MATRIX LEGS (e-identity-carried, e-identity-legacy) rather than a markdown
  receipt that goes stale.
- Particle reaction rides along: manafold_fx.h reads B's sag from the posed rig,
  and ZHAO_U02_FOLD_DIP_PM=0 still reverts the mana to its old bytes.
- Gate matrix 151/151 PASS, 0 FAIL, including both byte-identity legs.
