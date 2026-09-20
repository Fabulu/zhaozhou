# Task Log: RUN-20260920-0544 - Manafold pass 20 (Owner Direction 21)

**Created:** 2026-09-20 05:44 UTC+02:00
**Status:** Complete
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

### 2026-09-20 - INDEPENDENT REVIEW + QA: **BLOCKED** (P20-REVIEW-QA.md)
- **Claims 1 and 2 CONFIRMED.** The arc repair closes by construction (checked
  the algebra at s=0 and s=L, the degenerates, the overflow headroom and the
  determinism), its control fires on the real defect, and **the eye agrees**:
  at Inspect f380 -- pass 19's own worst rip -- the P19 tube ends in a pinched
  stub and the P20 one wraps into the body as a rounded tube. At Channel f080,
  the worst sample in the bank (113 deg turn, 361 mm hand-off), the join reads
  as one continuous trunk. R1's 60->140 and the hand-off demotion are supported
  by the render, which is the only support that counts for them.
- **Claim 3 IS FALSE AS SHIPPED, on both halves.**
  * `mrear --gate --dip` **overrode the shipping gain** (550) with **1000**. The
    "19 of 21 reach strictly lowest, shipping configuration" in packet 7 and the
    matrix leg `n-mrear-dip` both measured 1000. At 550 it is **4 of 21**
    (slots 0, 1, 2, 7 -- and 7 is the 2-frame Still diagnostic).
  * At 1000, **mspan G9 fails at 50.96 deg against an 8 deg ceiling** -- 6.4x
    over. Position step is unchanged (72.36 vs 72.30 mm), so it is the packet-6
    **ROLL FLIP returning**, not motion. The roll-stable aim postponed it past
    the shipping amplitude rather than removing it.
  * Gain and depth multiply into the same `s`, so this is the MECHANISM:
    550 -> 7.77 deg / 4 clips, 650 -> 9.21 / 9, 750 -> 11.28 / 14,
    1000 -> 50.96 / 19. **The ranking and the continuity ceiling trade
    monotonically and the dent cannot satisfy both.** Ladder in
    `P20-RECEIPTS/review-dip-ladder.txt`.
  * **Particles: 588 of 600 Hover frames are BYTE-IDENTICAL** to
    `ZHAO_U02_FOLD_DIP_PM=0`. Strongest frame moves **72 px** past 24/255, of
    92,160; the dip's own geometry moves 6,145. Invisible at native. The
    crayon-grain failure exactly.
- **Two more instrument faults.** R5's `dip_stuck` arm has **no control** and
  has never fired. **mspan G10 DENT PIN was specified in the architecture and
  never built** -- the dent's central contract is ungated.
- **Re-measured with the committed pin probe** (`-DZHAO_P20_PINPROBE`): the
  shipping residual is **7 mm L1**, not the 21/38 mm still quoted. The
  roll-stable aim cut it ~5x -- an undeclared improvement.
- **Also measured:** `KNEAD_DIP_PM=0` and shipping BOTH read rail 0.692 /
  stretch 2.052 / step 0.1630, so **the stretch ceiling rise 1.80->2.10 was the
  BOW, not the dip**, contrary to gate-change row 4.
- **Repairs made (instruments and comments only; no bound, threshold, art value
  or shipping byte touched):** `--dip` no longer overrides the gain and now
  names it; R5's OPEN text states the shipping reality and carries the ladder;
  G6 prints its margin (shipping **66.43 deg** vs the 140 ceiling, mutant
  160.20); three stale comments in `manafold_rear_audit.cpp` and three in
  `manafold_art.h` that described repaired defects as current.
- **Controls fired by hand:** `--fail-rear-strain` (0x8, rail 0.129),
  `--fail-walk-pairing` (0x8000, 4 of 5, +750 mm), `--fail-no-dip` (0x10),
  `--fail-posed-order` (G6's new arm, 160.20 deg), `--fail-e-start/mid/presocket`.
- **Gate matrix re-run from scratch in ONE invocation after the repairs:
  151/151 PASS, 0 FAIL**, including both byte-identity legs
  (carried 3/3, legacy 3/3). `P20-RECEIPTS/gate-matrix-review.txt`.
- Not done, correctly: no 22-subject bank, no encode, no merge, no deploy.
  There is a genuine improvement to publish (the rear) and an unfinished item
  beside it (the dip), and the publish trigger is a pass that is DONE.

---

## Close session (Opus, sole implementer) — clearing the review's block

**In progress when this line was written:** the full gate matrix is running in
one invocation from `.tmp/p20-fin/bin`; the next step after it lands is the
documentation, then commit and push. Source is complete.

### 1. The roll flip: diagnosed, and repaired by construction

The reviewer's ladder reproduced exactly on a fresh build (G9 worst 7.772 at
gain 550, 11.283 at 750, **50.963** at 1000, position step flat at 72.30–72.36).

**Cause.** Not the named flip branch — `shortest_arc_from_y`'s exact-pole guard
is never entered. It is the branch's *neighbourhood*, and the mechanism is
QUANTISATION, not discontinuity. The shortest arc's axis is `a × v`, whose
MAGNITUDE vanishes as the press drives the A→B segment toward antiparallel with
where it already points. In exact arithmetic the rotation is continuous through
that region; in int16 lanes the axis direction is recovered from `(v_z, 0, −v_x)`
— two millimetre integers that have both collapsed to a handful of counts — so
the axis is quantised to tens of degrees, and a ~180° turn about an axis tens of
degrees wrong is an orientation tens of degrees wrong. 50.963° of angular step on
a carrier whose POSITION step was flat is exactly that signature.

**Repair.** `arc_from_y_about` (manafold_clips.h): the turn is taken as an ANGLE
about an axis the beat itself defines and carries continuously — the normal of
the pre-dent (A,B,C) triangle, computed ONCE per sample from the carriers as the
ambient pose left them, with no term in `dent_pm` at all. `|N|` is ~10^5 mm² and
never collapses, so there is no ill-conditioned neighbourhood to stay out of: a
179.9° turn is as accurate as a 5° one. The half-angle form comes from the
production `quat_axis` (fx_sin/fx_cos), never from a near-zero norm.
`nodule_aim_rollstable` takes the world axis append-only; with no axis it is the
old function, byte for byte.

**The chord was tried and measured WORSE** (G9 9.773 at depth 2200, 164.5 at
6000, against the normal's 7.591 / 15.9), because the chord is not perpendicular
to the segment and the function's +Y-component drop mangles it. Measurement on
the comparison side chose between two authored candidates; it did not pick the
value.

**Result at the shipping gain, same instrument, same binary:**

| configuration | G9 worst, OLD aim | NEW aim, shipping build |
|---|---|---|
| gain 550 / depth 2200 (was shipping) | 7.772 | **7.591** — the no-dip bank's own worst |
| gain 550 / depth 3400 | 9.928 FAIL | **7.591** |
| gain 550 / depth 4200 (**ships**) | — | **7.642** |
| gain 1000 / depth 2200 | **50.963 FAIL** | **7.591** — the review's worst case, gone |

**Other z-then-x aim paths: four, all on HingeD, all pre-existing, none at risk.**
`nodule_aim`'s three ambient callers plus the HingeD closure/rear-socket writes.
The quantisation fault needs the target to approach ANTIPARALLEL with where the
segment already points; HingeD aims at a fixed anchor it already points at, and
the dent cannot push it there because HingeC is PINNED (0.066 deg, G10). The
check if that ever changes: mspan G6's worst consecutive-step turn (66.43 deg
against 140) and G7's closure endpoint (6.065 mm). Both green, neither moved.
`nodule_aim` itself is the interesting residual -- same blind spot, exact-off
today -- and `arc_from_y_about` is available to it append-only when a future
beat needs it.

### 2. What then capped the depth, and the second repair

With the flip gone the limiter became an honest RATE, and the per-slot angular
trace showed why: the worst clips ran their whole beat as one smooth symmetric
16-frame hump peaking at exactly the rate `kKneadDipMinRampKeys = 9` sets. Nine
keys is 0.3 s at 30 Hz — a jab, not a knead. The floor scan reads 17/19 hosting
clips reachable at 18, 18/19 at 20–28 (a different clip missing at each rung, as
the dip COUNT flips between two and one at different clip lengths) and **19/19
from 30 up**. 30 keys ships: one second of press, and on a clip under ~230 keys
one deliberate knead instead of two fast ones.

### 3. B-lowest AT THE SHIPPING VALUE

`kKneadDentDepthPm` 2200 → **4200**, ramp floor 9 → **30**, and
`kKneadDipClipPm` re-trimmed one clip at a time against the measured pair
(R5 margin, that clip's own worst angular step).

**19 of 19 hosting clips reach B strictly lowest, worst margin +37 mm (slot 9),
G9 worst 7.642 against the UNMOVED 8.0° ceiling.** Every other gate green.

`kKneadDipClipPm[15]` and `[16]` are now **0**, and that is a bookkeeping repair
rather than a retreat: neither clip calls `antenna_knead`/`swallow_nodules` at
all (the lab runs a forked `lab_antenna_knead`; nodule-solo exists to show each
nodule moving INDEPENDENTLY), so the dent could never run on them while a
nonzero entry told R5 they HOSTED a dip — a permanently red, structurally
unreachable leg on two clips.

### 4. The particle reaction, re-authored

The first version added `dip_pm` to `agit`, a scalar the fold already runs near
the top of: 588/600 Hover frames byte-identical, 72 px at the strongest. It is
now a MOTION of the whole mana body — the figure and its particle cloud flatten,
spread and are carried down about the same pivot while B presses, and come back
with it (`kFoldDipDropMm/SquashPm/SpreadPm`). Same motes, same palette, same
distance-scaled lines. Hover: 127/600 frames changed, 2,745 px at the strongest
(was 72). Inspect 3,417, Drift 283 -- re-taken from the SHIPPING build after
the constants were raised, because quoting the ladder rung the eye rejected
would be this pass's own defect. Chosen by eye off the {0, 300, 650, 1000} ladder at
native and 3× on Inspect, Hover and Drift, with the exact-off bank beside it.

### 5. Gate items

- **G10 DENT PIN built**, with `--fail-dent-pin`. Its first version measured
  POSITION only and read an unchanged 8 mm under its own control, because
  `loop_walk(g,4,...)` composes HingeC's rotation AFTER advancing the position —
  the detector was structurally blind to the fault it was built for. It now
  measures the FRAME: shipping **0.066°**, mutant **93.658°**, attributed
  `0x10300`.
- **R5's `dip_stuck` arm has a control** (`--fail-dip-stuck`) — and building it
  exposed the same law again: the arm differenced B's own vertical travel, which
  every other layer dominates, so a dip frozen at the bottom still measured
  280–520 mm and the arm could not fire (rc 0). It now measures the RETURN on the
  RANKING. Shipping returns +363..+468 mm above; the control fires `0x10`.
- **The hand-off bound is back**, re-calibrated for the new solve: 320 (a
  pre-bow number, still being printed while 361 sailed past it) → **420 mm**,
  16% over the shipping 361, which is identical with the dip off. Fired by
  `--fail-rear-frame` at **523 mm**; `--fail-rear-joint` reads 393 and stays
  under, so it keeps failing only its own R1 detector. R4 now prints WHICH of
  its four conditions fired.

### 6. Override audit

Swept every gate and driver for assignments to a `g_u02_*` shipping global. All
remaining ones are declared mutants (save/set/restore under a `--fail-*` flag or
`break_check`), env ladders, or the shellgate's explicitly inverted-polarity
regression control. No second `--dip` was found. The durable fix is a **judged-
configuration banner**: `u02::print_judged_config()` prints, in one line, the
value each override-reachable constant is ACTUALLY being judged at, beside its
shipping value, and marks any that differ. mspan and mrear both print it, so a
figure taken from an overridden run can no longer be mistaken for a shipping one.

### 7. Identity

`ZHAO_U02_KNEAD_DIP_PM=0 ZHAO_U02_REAR_BOW=legacy` reproduces
P19-FINAL-BANK-INTEGRITY's own CRCs **exactly**: Hover `0xA2D0E051`, Inspect
`0x779615BB`, Taunt III `0x75BC4777`. That is the pass-19 contract and it is
intact. The two `SOLVER=carried` legs were re-baselined and declared: the carried
solver is a selectable alternative mechanism, and the dip's shared schedule (the
ramp floor and the per-clip shares) feeds both solvers by design.

### 8. A third report naming the wrong operand, found at the very end

Slot 23 is the fixed-camera bake of the idle. Every SCHEDULED layer is called
with `kIdleOrbitSlot`, so it renders on slot 0's dip share (715) while its own
`slot_id` is 23 -- past the end of `kKneadDipClipPm`. Three lookups indexed on
the clip's `slot_id` and fell onto a 750 default the renderer never used: R5's
printed share, R5's "does this clip author a dip" test, and `swallow_nodules`'
own fallback. `u02::knead_schedule_slot()` names the mapping once and all three
use it now.

Production is unaffected and it was checked rather than argued: the shipping
Hover / Inspect / Taunt III / Blown sequence CRCs are byte-identical across the
change (`0x93B95AEE`, `0xD00478A1`, `0xC81598AA`, `0xC6AAF7AD`), captured before
the edit and again after the rebuild. The fallback was a silent default waiting
for the next clip whose slot runs past the table -- the pass-5 orphaned-index
fault, one index later.

That is three instruments in one session that named or measured something other
than the thing they were for (G10's position, R5's travel, this share), plus the
`--dip` override the review found. The banner and the two repaired operands are
the durable part; the pattern is worth carrying forward.

### 9. Receipts

* **Gate matrix: 159 legs, 159 PASS, 0 FAIL**, one invocation, the beat ON, from
  a clean rebuild of the final source. `P20-RECEIPTS/gate-matrix-ship.txt`.
* **Shipping:** G9 angular step **7.642** deg (ceiling 8.0, unmoved), G10 dent
  pin **8 mm / 0.066 deg**, R4 hand-off **361 mm** (ceiling 420), R5 **19 of 19**
  clips B-lowest, worst margin **+37 mm**.
* **Controls fired by hand:** `--fail-dent-pin` (93.658 deg, `0x10300`),
  `--fail-dip-stuck` (`0x10`), `--fail-rear-frame` (hand-off 523 mm, `0xB`,
  attributed `[rail-floor rail-ceiling hand-off ]`), plus the banner's own
  positive control (two overrides named in one line).
* **Attachment parity at 1.9x the previous depth:** every R1 and R4 number is
  identical to `ZHAO_U02_KNEAD_DIP_PM=0`, digit for digit.
* **Pass-19 identity:** `0xA2D0E051 / 0x779615BB / 0x75BC4777`, exact.
* Not done, correctly: no 22-subject bank, no encode, no merge, no deploy.

---

# RE-REVIEW (independent, second review agent) — 2026-09-20

**Where I was before reading the matrix result:** every claim re-measured from my
own binaries and the visual QA complete; two instrument repairs written
(`print_judged_config`'s mode table, R5's recorded sensitivity); report drafted;
the only thing outstanding was the full matrix in one invocation.

## Verdict: FIXED

The first review BLOCKED on three things. I re-measured all three with binaries I
compiled myself, no overrides, and they hold:

* **19 of 19 hosting clips B strictly lowest, worst margin +37 mm (slot 9),
  G9 7.642 against an UNMOVED 8.0.** My numbers, not the report's.
* **The roll flip is gone, not smaller.** At the first review's own worst case
  (gain 1000 / depth 2200, which read **50.963 deg FAIL**) I measure **7.591** --
  exactly the dip-off bank's worst, i.e. the dent is invisible to G9 there. The
  ladder is smooth and monotone to depth 6000 with no cliff, which is the
  signature of a well-conditioned aim and independent corroboration of the
  quantisation diagnosis.
* **The particle reaction is visible at NATIVE.** Judged by looking at 384x240,
  not by pixel counts; the counts (127 frames / 2,745 px on Hover) were taken
  only as the bias check and they reproduce.

## Item 4 -- the hand-off bound 320 -> 420: LEGITIMATE. No correction.

Measured rather than argued. Three findings:

1. **The bound did not exist at the branch point.** It first appears at
   `934a73a4`, INSIDE pass 20, before the bow repair landed at `5a18cdf7`. It was
   never an inherited contract being relaxed.
2. **320 provably described the pre-bow geometry.** With `REAR_BOW=legacy` the
   band reads **hand-off 270 mm** (turn 35.60, rail 0.129). 320 = 270 x 1.185.
   The new 420 = 361 x 1.163 -- the SAME construction, applied to the geometry
   the accepted bow fix produced.
3. **The pass's new art contributes exactly zero.** 361 with the beat on is
   digit-identical to 361 with `KNEAD_DIP_PM=0`. A gate fitted to the art would
   have had to move FOR the art; this one did not move for it at all. The rise
   270 -> 361 (x1.34) is sub-linear in a centreline turn that rose x3.17, which
   is how a bend distributed along the helper chain behaves.

Caveat stated in the report: it is a REGRESSION ceiling, calibrated by the thing
it measures, and can never assert correctness. The correctness evidence is the
eye, taken at Channel f080 -- the exact 361 mm / 113 deg frame -- where the join
reads as one continuous fused trunk.

## Two real faults found and fixed (instruments only; no bound, no art value)

* **The judged-configuration banner was blind to every MECHANISM SELECTOR** --
  this pass's FOURTH wrong-operand case, inside the instrument built to stop
  them. `ZHAO_U02_KNEAD_DIP_SOLVER=carried` swings R5 from "19 reach lowest" to
  "18 NEVER reach lowest, worst -192 mm" and makes G10 read "0 dent samples";
  `ZHAO_U02_REAR_BOW=legacy` reverts the whole R4 family to the PRE-REPAIR
  creature. Both gates honour both. The banner printed "every shipping constant
  at its shipped value" for each. Fixed: a mode table printed first, plus six
  missing numeric rows; fired four ways.
* **R5's `dip_stuck` arm fires at 2 of 19 under its own control.** It fires, so
  it is not dead -- but the claim "nothing but the dip can hide that" is wrong:
  the return is measured against `min(A_y, C_y)` and the outer balls run their
  own schedule, so a B frozen at the bottom still reads "above the lower outer
  ball" on 17 clips. Sensitivity recorded beside the constant with the numbers a
  correction would be made from. Bound NOT moved -- out of remit.

## Confirmed honest rather than convenient

* Slot 21 (Taunt III) is **368 of 368 frames byte-identical** with the dip off --
  I rendered both banks. The dent provably does not run there and its +64 mm
  comes from its own crown shuffle, as declared.
* Slots 7/15/16 genuinely never call the knead layer (`build_still` and
  `build_nodule_solo` contain no call; `build_manalab` calls the forked
  `lab_antenna_knead`). The old 750 entries were an unreachable gate state.
* `knead_schedule_slot` has exactly the three callers the report names.
* Independent override sweep of all eleven gate/driver `.cpp` files: every
  `g_u02_*` assignment is a saved/restored declared mutant, a strict env parse,
  or the shellgate's declared inverted-polarity control. No second `--dip`.

Controls fired by me: `--fail-rear-frame` (523 mm, 0xB, `[rail-floor
rail-ceiling hand-off ]`), `--fail-rear-joint` (393 mm, deliberately under,
0xA), `--fail-dip-stuck` (0x10, ranking arm), `--fail-no-dip` (0x10, the other
arm), `--fail-dent-pin` (93.658 deg frame, position blind at 8 mm, 0x10300), and
the repaired banner four ways.

Visual verdicts: (a) rear connection whole through the orbit -- YES, including
f380 and Channel f080. (b) the knead reads AND B reaches the bottom -- YES, at
native; this is the change since the last review, which found it not legible.
(c) particles react deliberately -- YES, at native. (d) new faults -- NONE.

Full report: `P20-REVIEW-QA-2.md`. Pictures: `P20-LOOKS2/` (17).
Not done, correctly: no 22-subject bank, no encode, no merge, no deploy.

### Re-review receipts

* **Gate matrix: 159 legs, 159 PASS, 0 FAIL**, one invocation, the beat ON, from
  a clean rebuild of every binary after both repairs.
  `P20-RECEIPTS/gate-matrix-review2.txt`.
* **All four identity legs exact**, including the two the close re-baselined.
  `e-identity-pass19` reproduces pass 19 byte for byte:
  `0xA2D0E051 0x779615BB 0x75BC4777`.
* **The shipping bank reproduces from my own build with both repairs in place** --
  hover `0x93B95AEE`, inspect `0xD00478A1`, taunt3 `0xC81598AA`, blown
  `0xC6AAF7AD` -- so the repairs are renderer-invisible and the tree's bank is
  what the close says it is.
* `P20-RECEIPTS/review2-{mrear,mspan}-shipping.txt`,
  `review2-banner-selectors.txt` (the repaired banner naming all three
  mechanism overrides, plus the clean line),
  `review2-binaries-md5.txt`.
* One last rung rendered that the depth ladder did not have: at depth 4600 the
  extra press buys crowding at the left shoulder rather than read, so the
  shipped 4200 stands as a choice rather than as the ceiling's shadow.

### One I caused, and the rule that caught it late

I ran `zhao-reel-cel.exe` with no arguments to list its clip names. CLAUDE.md
warns about exactly this -- **the reel has no help flag**, `g_out = argv[1]`, so a
bare invocation starts a full default-bank render into the current directory. I
killed it inside two minutes and verified no orphan process, and I got the clip
names out of the source instead.

**What the kill did not undo was 2.29 GB in 49 directories at the repo root**,
and the instructive part is that `git status` showed **none of them** at first:
`.gitignore` covers `*.rgb`, so a directory holding only frames is invisible, and
the ones that did surface only surfaced because a `meta.txt` had landed beside
the frames. That is CLAUDE.md's "a rule that HIDES waste is not a rule that
removes it", reproduced live -- the tree looked clean while 2.29 GB sat in it.

Removed, scoped by creation-time window to my own two-minute run and only after
checking every directory held nothing but `.rgb` and `meta.txt`. One neighbouring
directory (`manafold-lasso`, 10:32) predates my run and was left alone.


---

# CLOSED (2026-09-20): published and production-verified

**Status: Complete.** Full records: `P20-FINAL-BANK-INTEGRITY.md`,
`P20-MEDIA-CLOSURE.md`, `P20-PRODUCTION-VERIFY.md`.

* **Pass 19 archived first**, before any encode could overwrite a live name:
  44/44 verified against the published receipt, copied to
  `archive-p19-manafold-*`, every COPY re-hashed, 44,320,731 bytes.
  `P19-ARCHIVE-SHA256.txt`. Fifteenth archive generation; `checkarchive`
  selftest fires fifteen red legs, up from nine.
* **Exact bank:** one renderer (MD5 `e95faca916627d1bddb02892c5eb67e1`), one
  invocation, 22 subjects, 7,992 frames, manifest
  `a40b41549383246d7c9580c768c936f8919eb810dce7c0ecae24e3cdb1313b15`,
  validator PASS, all four re-review CRCs reproduced.
* **Scope, on 22 subjects rather than three:** with all THREE pass-20 switches
  off the bank is byte-identical to pass 19 on 22/22. Rear 22, beat 21,
  pose-driven reaction 3 more on its own.
* **The finding of the packet:** the pass's own "pass-19 identity" leg was short
  an operand and sampled on the only three clips that could not fail. It holds
  on 19 of 22; Blown, Fall and Trick differ. Repaired, with a discriminating
  witness leg and a positive control. Nothing shipped changed.
* **Review:** every frame of all 22 clips on complete every-frame sheets, 20
  images, frames chosen by badness. No fault found. The rear join, the knead at
  native, and the mana reaction all read.
* **Gate matrix:** 161 legs, 161 PASS, 0 FAIL, one invocation.
* **Encode** `ENCODE_RC=0`, 22/22 + posters, probe 44/44. **Local gate**
  `DEPLOY_ASSEMBLEONLY_RC=0` with no skip flag, decode 1,508/1,508.
* **Published** `DEPLOY_RC=0` to `https://0cb48546.upheaval.pages.dev`, alias
  `https://upheaval.pages.dev`. Both mains fast-forwarded, the fast-forward
  proved rather than assumed.
* **Production verified 54/54 on BOTH hosts**, 66,906,938 bytes each, 0
  mismatches, 0 retries, twelve index checks green, verifier selftested on
  deliberately broken copies first (and it caught a broken negative of its own).
