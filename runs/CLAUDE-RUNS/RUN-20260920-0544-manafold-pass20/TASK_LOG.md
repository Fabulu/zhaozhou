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
