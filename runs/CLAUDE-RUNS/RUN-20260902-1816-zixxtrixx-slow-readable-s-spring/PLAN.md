# PLAN — the slow, readable, whole-body S spring (Direction 23)

**Run:** RUN-20260902-1816-zixxtrixx-slow-readable-s-spring
**Role:** architect. Plan only — no pose values are authored here, no builds run.
**Inputs:** Owner Direction 23 (binding), Directions 22/21/20 (where still
standing), the five recon documents in `recon/`, the art law in `CLAUDE.md`.
**Lane:** `C:\programmieren\zencrifice\zixxtrixx-wholebody-s-spring-20260901`,
zhaozhou checked out on `main` at `441160ed` (== origin/main; the local
`zixxtrixx-wholebody-s-spring` branch is an ancestor). Commit to `main` of this
lane and push, as the recon commit did. **Never touch the sibling live repos.**

---

## 0. Headline decisions

1. **Start from Generation Thirteen** — surgical three-file restore of
   `tools/reel/zixxtrixx.h`, `tools/reel/zixx_probe.cpp`,
   `tools/reel/zixx_springpose.cpp` from `a2f601ef`. Verified: every commit
   touching those files since `a2f601ef` is a Direction-22 coil commit
   (`f9c75809..00202270`), and nothing touched `tools/reel/` after `00202270`.
   The restore discards exactly the rejected work and nothing else.
2. **The revert is the floor, not the fix.** The pace work is the pass:
   the arming grows from 16 keys to **64 keys** (+ an 8-key living hold),
   ground time before launch goes 36 → **144 frames**, the whole jump clip
   from 161 to ≈ **270 frames**. The retime applies to the SHARED spring
   timing — all saltos and jumps together, per Direction 21 §4 and the
   bit-exact parity gates.
3. **The central fix is making the pose route evenly spaced in shape under a
   smooth clock**: (a) the seating knot and its 9.4×-rate leg die with the
   revert; (b) the two remaining interior knots get re-spaced by measured
   shape-arc-length (bias removal, then adjusted by eye); (c) the single
   trapezoid clock is replaced by a per-beat smoothstep schedule evaluated at
   milli-key resolution, so 60 Hz midpoints sit on the eased curve instead of
   on a linear chord — killing the 30 Hz velocity staircase by construction;
   (d) the support lift route is re-authored monotone (the six-reversal whip
   route is a D22 artifact and dies with the revert).
4. **The jump camera gets the salto camera's fix**: track the plan's smooth
   trajectory, never the raw root with the life wave in it.
5. **The life layer is untouched in structure.** Recon 3's ablation proves it
   innocent (63 reversals with waves on, 63 with waves off). Amplitude is
   re-judged by eye once the primary is slow.
6. **Beat 2 gets one authored art pass**: head slightly back and slowly down
   (checked in the fixed side view, in pixels), and the rear KEEPS its curl
   from assembled to collapsed — which simultaneously satisfies the standing
   whole-body law and makes the route monotone per station.
7. **Acceptance criteria: adopt Recon 5's A/B/C table with amendments**
   (§7 below), plus Recon 2 §3b's jerk thresholds as the lead smoothness gate.
8. **Delete, do not inherit, the coil-formation press allowance** — it was an
   owner ruling on a motion Direction 23 withdraws. (The revert deletes it.)

---

## 1. Starting point: revert to `a2f601ef`, then build the pace on top

**Decision: option (a) — restore the three reel files from `a2f601ef` and
re-time/re-ease on top.** Not a re-author from the reference's tables, and not
a partial cherry-pick of constants.

Why:

* Recon 2 established with high confidence that Generation Thirteen
  (`a2f601ef`) is "the reference implementation": the only bank the owner
  praised, the only one that performs Direction 23's described motion, and
  frames 45+ of its clip are byte-equivalent to live — the regression is
  confined to the arming.
* Every real fix (the metres/mm `spring_anchor_offset` unit bug, the honest
  station-14 plant, the rollover removal, the restored salto, flatten-and-
  spread contact) predates `a2f601ef`. **The revert forfeits nothing** (Recon 2
  §6, verified by grep across the commits).
* The git history makes the restore *exact*: `git log a2f601ef..HEAD --
  tools/reel/` lists only the seven D22 commits. A full-file checkout of the
  three files is simpler and more provable than a constant-by-constant
  cherry-pick — afterwards `git diff a2f601ef HEAD -- tools/reel/` must be
  empty, which is a one-command verification no hand-merge can offer.
* The restore also brings back the *matched probe bands* (cross-section
  ratios, hold drift, bite envelope) that pair with Gen Thirteen's
  flatten/spread values — cherry-picking tables without their bands is how a
  probe goes red for the wrong reason.

What the revert kills, by name: `kSpringSeatingHeading` + `kSpringArmSeatingAt`
(the 9.4× leg, Recon 3 J1), the eight-knot whip `kSpringSquashLiftRoute`
(the six vertical reversals, J2 — added in `05c606bd`), the whole
`kSpringCoilFormation*` declared-press block (§0.8), the raised
flatten/spread/loaded-bite trio, and the 280.5° mid-body wind.

What it does NOT fix (and the rest of this plan does): the pace (Gen Thirteen's
half-life is 7 frames against the accepted floor of 16 — Recon 5), the residual
jerk (5× the balance clip — Recon 2 §3b), the camera, the chord midpoints, the
head's backward travel, and the rear losing curl into the collapsed pose.

---

## 2. Timing design

### 2.1 The four beats on the key line (30 Hz keys; 1 key = 2 frames)

Direction 23's beats map onto the machinery as: beat 1 = entry
(arm 0→400, grounded → absorb → assembled — "become the S"), beat 2 = squash
(arm 400→1000, assembled → collapsed — "compress"), beat 3 = the loaded hold,
beat 4 = the release (unchanged and fast, per Direction 20 §4).

| beat | keys | frames | arm | what moves |
|---|---|---|---|---|
| settle-in | 0–4 | 8 | holds 0 | life layer only; the entry seam is velocity-continuous by construction (Recon 1 §5.2) |
| **1. become the S** | 4–36 | 64 | 0 → 400, one smoothstep | whole body into the assembled S, balance-rise pace (its rise is 98 frames; 64 is in-family and twice a stunt's urgency) |
| settle | 36–42 | 12 | holds 400 | life only; the beat READS because it ends |
| **2. compress** | 42–64 | 44 | 400 → 1000, one smoothstep | head slightly back + slowly down, everything descends together |
| **3. loaded** | 64–72 | 16 | holds 1000 | living hold (`kSpringHoldLivingDriftMm`), never frozen |
| **4. release** | 72–76 / 76–78 / 78–82 | — | release arms unchanged | existing 4-key release + 2-key rigid rise + 4-key wheel gather, untouched durations |

So: `kSaltoCompressEndKey = 64`, `kSaltoCompressHoldEndKey = 72`,
`kSaltoSpringReleasePoseKey = 76`, `kSaltoRigidReleaseEndKey = 78`,
`kSaltoReleaseEndKey = 82`, `kSaltoCoilPoseKey = 83` (derives),
`kSaltoUnrollStartKey = 106` **(= 83 + 23, preserving the prime stride)**,
`kSaltoUnrollEndKey = 114`. Downstream attack keys all shift by **+54**;
`kAttackKeys = 294`. Jump clip: 64 + 8 + 4 + 38 + 6 + 14 + 1 ≈ 135 keys ≈
**270 frames** (Recon 5 predicted 260–300).

Ground time before launch: 72 keys = **144 frames** — above the 112-frame
floor, at the bottom of the 150–180 target band. Every number above is an
owner knob and an eye call; the budget is designed so each beat clears Recon
5's 16-frame travel / 6-frame settle minima with margin, not so it hits a
measured optimum. If it drags on screen, shorten beat 1 first.

### 2.2 Even in shape under a uniform clock — the central fix

Three mechanisms, in order:

1. **Knot re-spacing.** After the revert the route is grounded(0) →
   absorb(220) → assembled(400) → collapsed(1000). Measure each leg's
   shape-space arc length (sum over stations of |Δposition|, integrated in
   sub-steps — Recon 3's instrument, which was validated against the committed
   probe to 2 mm). Re-position `kSpringArmAbsorbAt` (and check
   `kSpringArmAssembledAt`) so arm-units per shape-mm are roughly constant
   across legs. This is measurement removing a BIAS (the clock's
   non-uniformity); the poses themselves are not touched by it. The final
   values are then adjusted by eye and live in the named constants.
   **Caution:** `kSpringArmAssembledAt` is double-duty — spline knot AND
   entry/squash split (`zixxtrixx.h` ~:831). If its knot position must move,
   prefer moving `kSpringArmAbsorbAt` and re-shaping the beat-1 ease instead;
   only move the split with the full §5 checklist in hand.
2. **A per-beat smoothstep clock at milli-key resolution.** Replace the single
   trapezoid `spring_arm_ease` with a schedule function:
   `arm(key_mk) = 400·ss(key_mk over beat 1) `, dwell, `400 + 600·ss(key_mk
   over beat 2)`, dwell — the balance clip's own device (its entire rise is ONE
   smoothstep, Recon 1 §1.2). Smoothstep endpoints have zero velocity, so the
   dwells join C1 by construction and the four beats segment on the shape-rate
   plot (criterion A4) without any further engineering.
3. **Midpoints ON the eased curve.** The 60 Hz midpoints currently take the
   arm exactly halfway between neighbour keys — a linear chord, which is why
   all 63 reversals land on odd frames (Recon 3 J4). With the schedule taking
   milli-keys, the midpoint evaluates the schedule at `key*1000 + 500` — still
   exactly on the route (any arm value is on the route), but now on the eased
   clock. The velocity staircase disappears by construction. The release-side
   midpoints (`kSpringReleaseMidpointControl`) are untouched.

Plus two route repairs:

4. **Support lift route re-authored monotone.** The reverted (pre-whip) route
   is the starting point; the new schedule needs its breakpoints re-expressed
   on the new squash timeline. Law: the loaded support height is approached
   monotonically — at most ONE authored direction change across the whole
   arming, and none faster than the beat it lives in. Recon 3 J6: making this
   route monotone alone removed half of all reversals.
5. **Per-station monotonicity from the tables.** After the beat-2 art pass
   (§4, stage 6) the criterion is: each station's heading crosses the
   absorb → assembled → collapsed knots with at most 2 direction reversals
   across the whole arming (criterion A5), measured from the pose tables,
   never from pixels. The balance rise has 0.

### 2.3 What "slightly backward" means, checkably

Authored and verified **in the fixed side view, in pixels** — Recon 2 proved
the world-space metre count reads backwards on screen. Gen Thirteen's head
moves ~5–9 px back during arming; Direction 23 wants a modest, visible
increase, not a rewrite. Working target: **10–25 px back and visibly down** at
384×240, adjusted by eye, with closure (head tip to tail tip / arc length)
**never below 0.40** (criterion B3 — this IS "never approaches the tail" as a
gate). The neck hook must not curl tighter than Gen Thirteen's to fake it
(D22 §2 still informs the mechanism: the travel comes from the mid-body
carrying the front back, authored in the collapsed table's segments 8–9 and
the support height, not from folding segment 5–7 harder).

---

## 3. Staging — ordered, each independently checkable, smallest risk first

Every stage: build with `build-direct.sh` (ONE target per call, `--clean`
after any struct change), run the probe, commit, push. Renders go beside the
stage in this run folder. The stale-binary tell applies throughout: a number
that did not move after a change that must have moved it means a stale exe.

**Stage 0 — scaffold and baseline.**
Fill SPEC/TASK_LOG. Build cel + probe at HEAD; render `zixxtrixx-jump-one`,
`zixxtrixx-salto-dummy`, `zixxtrixx-balance`; run the new legibility probe
(§6.1 — commit the tool first, it is comparison-side only) on all three and
commit the baseline CSVs/plots. These are the "before" of every before/after
pair. *Verified by:* probe green at HEAD, baseline numbers match Recon 5's
(half-life 5, shape p90 56, solidity 0.85 on live arming).

**Stage 1 — name the literals (no-op refactor).**
In the slice/seam block at the end of `zixxtrixx.h` (search
`slice_clip(atk_local, kSlotAtkRelease`), replace the raw literals
17/29/52/60/74/75/224/239 — and their twins in the seam table — with named
constants derived from the `kSalto*` timeline (`kSaltoCoilPoseKey` and
`kSaltoUnrollStartKey/EndKey` already exist; add stick/recover key constants).
Replace `JumpPlan::release_keys = 4` with `kSpringReleaseMidpointCount`.
Express every attack-curve knot ≥ key 22 (`kAtkLift/kAtkFwd/kAtkAim/kAtkCurl/
kAtkAuth/kAtkSpin`) as `base + kAtkRetimeShift` where
`kAtkRetimeShift = kSaltoCompressHoldEndKey - 18` (currently 0).
*Verified by:* bank is BYTE-IDENTICAL — per-subject `meta.txt` sequence CRCs
unchanged against stage 0; probe green. This stage converts the retime from a
minefield into a constant edit, and it is provably free.
*(Do this against live, before the revert — the literals are identical in both
states, and doing it first means the revert diff and the retime diff each stay
single-purpose.)*

**Stage 2 — the surgical revert.**
`git checkout a2f601ef -- tools/reel/zixxtrixx.h tools/reel/zixx_probe.cpp
tools/reel/zixx_springpose.cpp`, then re-apply stage 1's refactor on top (it
is mechanical; the two diffs stay separate commits).
*Verified by:* `git diff a2f601ef HEAD -- tools/reel/` shows only the stage-1
naming; probe green with ZERO self-intersections and no declared windows;
rendered jump-one frames from launch onward CRC-match the live bank (Recon 2:
frames 45+ byte-equivalent); arming bbox/area table matches Recon 2 §3's REF
column; support Δy sign changes ≤ 1 across the arming (the whip is gone).

**Stage 3 — the jump camera.**
Give `zixx_jump_track` (zixxtrixx.h ~:7518) the salto camera's fix: track the
plan's smooth trajectory — the schedule's eased lift, life clock excluded
(`kSpringNoLife`) — never `spring_root_anchor_x/offset` raw. Camera only;
the bank must not change.
*Verified by:* bank CRCs unchanged; background churn (mean |Δ| over
non-creature pixels) during the arming falls to balance-clip levels; horizon
row moves < 1 px/frame during the arming.

**Stage 4 — the retime skeleton.**
Set the §2.1 key constants; `kAtkRetimeShift` becomes +54 and the whole
downstream attack slides; `kAttackKeys = 294`; re-point
`kSaltoSpringEntryEndKey` at the beat-1 end key; **re-author the probe's
phase-envelope gate** (`zixx_probe.cpp:1355`) around the new schedule as
derived relationships (entry < compress, hold length, release deltas
unchanged), with a comment citing Direction 23 — it is a §5.2 regression band,
not a law, and the band it records was fitted to the rejected schedule.
Same-commit re-derivations: none of the coil-press ticks exist any more
(deleted in stage 2); check `kSpringHoldLivingDriftMm`'s gate against the
longer hold. The route itself is unchanged — same poses, same trapezoid,
just stretched.
*Verified by:* creature compiles (the seam gates ARE the check that the +54
shift is consistent — key 83 == key 110 is enforced at compile); probe green;
parity gates green; flight/landing unchanged by eye; `zixx-springpose
schedule` shows per-key `move_mm` scaled down ≈ 4× everywhere; render + contact
sheet of every arming frame.

**Stage 5 — the schedule and the even route (the central fix).**
Implement §2.2: milli-key per-beat smoothstep schedule, midpoints on the eased
curve, knot re-spacing from measured arc length, monotone support route on the
new squash timeline.
*Verified by:* velocity-staircase plot (head speed per frame) is smooth —
no adjacent-pair step ratio above ~1.3 inside a beat; `move_mm` column even
within each beat; reversals from the pose tables counted; legibility probe on
a fresh render: A1 (half-life ≥ 16), A2 (shape p90 ≤ 12, no frame > 20),
A3 (jolts ≤ 5/s, none closer than 8 frames), A4 (four beats segment);
jerk (Recon 2 §3b) within 2 px centroid / 4 px head / 60 px² area. Then LOOK:
the four-panel plot beside the balance clip's must show broad humps, not a
picket fence.

**Stage 6 — the beat-2 art pass (author by eye).**
The one stage that touches pose values, and it is authored by eye per the art
law: render, look, compare, adjust. Two targets: (1) the collapsed rear
(segments 14–18) keeps or deepens the assembled curl — no station loses its
lobe on the way into the squeeze (this is D20 §6/D21 §3's standing whole-body
law AND what makes the route monotone); (2) the head lands slightly back and
down per §2.3. Fixed orthographic side view of the collapsed pose committed as
evidence, last five segments distinguishable, centreline curvature changing
sign behind the support.
*Verified by:* A5 reversals ≤ 2 per station from the tables; B1–B4 on the
render (solidity ≤ 0.70, hole ≤ 6%, closure ≥ 0.40, spine ≥ 0.65× median);
head travel measured in the fixed side view; probe (intersection, bite,
lateral-span, cross-section) green; and the eye, at native resolution, beside
the balance clip.

**Stage 7 — the life layer, by eye.**
Amplitudes re-judged once the primary is slow (the revert restores Gen
Thirteen's values; against a ≤ 6 deg/key primary they may finally be visible —
Recon 1's crayon-grain point). Periods stay ≥ 12 frames (A6);
`kSpringWobblePeriodKeys` 23/51 both clear the floor on the longer arming.
Do not add per-station lag to the rise — the balance rise has none (Recon 1
§1.4). *Verified by:* the hold visibly quivers at native res; A-criteria still
pass; probe drift gate green.

**Stage 8 — full verification, encode, publish.**
Full 22-subject render; legibility probe + four-panel plots on jump-one,
salto-dummy, and balance (committed); every-frame contact sheet + centroid-
locked 2× zoom sheet of the arming; before/after pairs vs stage 0; probe
green end to end. Archive the outgoing bank (bump `MAX_ARCHIVE_GENERATIONS`
AND both CSS selector families) BEFORE re-encoding. Encode
(`tovideo.py`, then `togif.py` for archive). Publish via
`deploy.ps1 -Project upheaval -Branch main` — this is a finished creature
pass, the standing authorisation applies, and `-Branch` is mandatory.
Then hand to the reviewer and QA per Direction 23's owner-specified process.

Stages 1–4 are cheap and near-riskless (two are provably no-ops on the bank or
the pixels). Stage 5 is the fix. Stage 6 is the art. If time is short, a pass
that stops after stage 5 with Gen Thirteen's poses is already publishable-by-
eye-check; stage 6 is what answers D23 acceptance #4 fully.

---

## 4. Named constants (what exists, what is new, which are owner knobs)

New (all owner knobs unless marked derived):

| constant | controls |
|---|---|
| `kSpringSettleInKeys` (4) | life-only keys before arm moves; the entry seam |
| `kSpringBecomeSEndKey` (36) | end of beat 1 travel |
| `kSpringBecomeSSettleKeys` (6) | the dwell between beats 1 and 2 |
| `kSaltoCompressEndKey` (16 → 64) | end of beat 2 / the arming (exists) |
| `kSaltoCompressHoldEndKey` (18 → 72) | end of the loaded beat (exists) |
| `kAtkRetimeShift` (derived: `kSaltoCompressHoldEndKey − 18`) | slides every downstream attack key |
| `kAtkStickStartKey/EndKey`, `kAtkRecoverStartKey/EndKey` | replace literals 74/75/224/239 (derived from the shifted timeline) |
| `kSpringArmMaxStationStepMm` (~100, recorded AFTER acceptance) | arming-only probe band beside `kJumpMaxStationStepMm` (Recon 1 §5.1) |
| beat-schedule knots inside the new `spring_arm_schedule(key_mk)` | the per-beat eases; every breakpoint named |

Existing, re-valued or re-pointed: `kSpringArmAbsorbAt` (re-spaced by
arc-length then eye), `kSaltoSpringReleasePoseKey/RigidReleaseEndKey/
ReleaseEndKey` (+54), `kSaltoUnrollStartKey/EndKey` (+54, stride 23 kept),
`kAttackKeys` (294), `kSaltoSpringEntryEndKey` (re-pointed at beat-1 end),
the support-lift route breakpoints (new squash timeline, monotone).

Existing, restored by the revert and NOT re-valued here:
the four pose tables (grounded/absorb/assembled/collapsed),
`kSpringBodyFlattenQ16`/`SpreadQ16` (20000/8000), the support lift values,
`kSpringSupportCompensation` (1000 — owner knob, do not touch),
`kSpringHoldLivingDriftMm`, the wobble amplitudes (stage 7 eye pass),
`kSpringAirWobblePeriodKeys` (23 — see landmine 5).

Deleted by the revert, do not resurrect: `kSpringSeatingHeading`,
`kSpringArmSeatingAt`, all nine `kSpringCoilFormation*` constants, the
eight-knot whip lift route.

Every one of these lives in `zixxtrixx.h` as a named, editable constant —
nothing is "generated from the reference so it is not a knob" (art law §6).

---

## 5. Landmine checklist (all verified against the lane at 441160ed)

1. **`zixx_probe.cpp:1355`** — phase-envelope band pins the arming to the
   rejected schedule (`kSaltoSpringEntryEndKey ∈ [10,14]`, compress−entry ∈
   [3,6], hold ∈ [2,4]). MUST be re-authored in stage 4 or the probe forbids
   the slower arming. `kSaltoSpringEntryEndKey` (`zixxtrixx.h:1036`) is
   otherwise inert — it exists to feed this gate.
2. **Slice/seam literals** — `slice_clip(..., kSlotAtkRelease, 17, 29)`,
   `duplicate_pose_clip(..., 29)`, `slice_clip(..., kSlotAtkUnroll, 52, 60)`,
   stick 74/75, recover 224/239, and the seam table's 17/12/0/1/8/9/11/15
   pairs (zixxtrixx.h ~:8008–:8040 at HEAD). `kAtkCompressSliceLastKey` on the
   same lines DERIVES while its neighbours don't — retiming moves one and not
   the other. Stage 1 removes this class of fault before anything else moves.
3. **The attack curve tables** (`kAtkLift` ~:1443, `kAtkFwd` ~:1457, `kAtkAim`
   ~:1478, `kAtkCurl` ~:1490, `kAtkAuth` ~:1497, `kAtkSpin` ~:1508) carry raw
   key literals from 22 to 239 sitting beside derived knots on the same lines.
   All shift by `kAtkRetimeShift`. `kAttackKeys = 240` (:560) grows with them.
4. **`JumpPlan::release_keys = 4`** (~:7191) is a bare literal where
   `zixx_plan_attack` uses `kSpringReleaseMidpointCount`. Stage 1 fixes it.
5. **The prime-number contract** (zixxtrixx.h ~:2211–2220): attack key 29 must
   equal key 52 (compile-enforced via the coil/unroll seams), so the stride
   stays 23 and `kSpringAirWobblePeriodKeys` may only be 23 or 1. Deriving
   `kSaltoUnrollStartKey = kSaltoCoilPoseKey + 23` preserves it under any
   shift by construction; the compile seam gate is the tripwire.
6. **`kSpringArmAssembledAt`** is BOTH a spline knot and the entry/squash
   split (~:831). Moving it re-parameterises the whole clock and shifts which
   keys are entry vs squeeze; `kSpringAbsorbProfile` derives from it. Prefer
   not to move it (§2.2.1).
7. **The coil-formation press allowance** (`kSpringCoilFormation*`, :942–966)
   is stated in ABSOLUTE presentation ticks and was an owner ruling on the
   withdrawn D22 motion. **Delete (via the revert), never inherit** — a
   retime would silently land the allowance on the wrong frames.
8. **Probe bands paired to pose values**: cross-section ratio [490,570]/
   [850,925], bite envelope, hold drift 88 → these pair with LIVE's
   flatten/spread. The three-file revert restores the matching Gen-Thirteen
   bands together — never cherry-pick tables without their bands.
9. **`uses_default_shared_spring_timing`** exists twice (~:3486 AttackPlan,
   ~:7324 JumpPlan) and the release-curve algorithm is hand-duplicated
   (~:3509 / ~:7330). Both derive from the constants, so the retime is
   automatic — but any *algorithm* change must hit both twins.
10. **`zc::AttackPlan` engine defaults** (`reference/include/zref/
    zref_creature.hpp:962`) still read the long-rejected 12/6/4. Harmless only
    because `zixx_plan_attack` overwrites all three; leave a comment, do not
    depend on it.
11. **Support-route breakpoints must stay sorted** by their key (entry or
    squash) — an out-of-order breakpoint has broken a build before.
12. **Never let two adjacent knots differ by exactly 32768 (180.0°) on any
    station** — the shortest-arc unwrap is degenerate there and a ±1 edit
    flips the direction the whole tail swings (Recon 3's station-14 trap; the
    instance dies with the seating table, the rule outlives it).
13. **Owned entry midpoints** `kSpringEarlyEntryOwnedMidpointKey = 1` /
    `kSpringEntryOwnedMidpointKey = 4` (~:2510) are pinned to key indices of
    the OLD schedule; under the milli-key schedule they are either re-pointed
    or retired (the schedule supersedes their purpose). Also
    `kSpringMiddlePoseKey` is live-but-inert — do not reactivate it.
14. **Build traps**: one target per `build-direct.sh` call; `--clean` after
    struct-layout changes; `cmake --build` races Verilator — compile the reel
    directly; a measurement that did not move is a stale exe.
15. **Encode/publish traps**: archive the outgoing bank (bump
    `MAX_ARCHIVE_GENERATIONS` AND both CSS selector families) before
    re-encoding — encoding overwrites in place; `deploy.ps1 -Branch` is
    mandatory or Wrangler silently ships a preview.
16. **Kill background builds** before declaring any stage closed.

---

## 6. Verification plan

### 6.1 Committed diagnostics (comparison side only — they assert nothing about what to author)

1. **`tools/reel/zixx_legibility.py`** (new, committed): per clip, a CSV of
   frame, silhouette area, convex solidity, enclosed-hole %, closure, spine
   arc length, translation-compensated shape rate, pose half-life, head-blob
   found, Weber contrast — **colour segmentation, never a median plate**
   (Recon 5's declared trap: a moving camera poisons background subtraction),
   with a frame-beside-mask overlay sheet spot-checked every pass.
2. **Four-panel time-series plot** per clip (shape rate, half-life,
   solidity+hole, closure) on the same axes as the balance clip's — the
   picket-fence-vs-broad-humps read at a glance.
3. **Every-frame contact sheet** of the arming (uniform sampling misses the
   broken frame — CLAUDE.md "Seeing the work properly"), plus a
   centroid-locked 2× zoom sheet, plus before/after pairs against stage 0.
4. **`zixx-springpose schedule`** (already committed) — the per-key `move_mm`
   column is the direct even-motion read-out; build it by hand per Recon 4 §5.4.
5. **Beat segmentation printout** — if it does not print four runs, the four
   beats do not exist on screen, whatever the tables say.
6. **Jerk table** (Recon 2 §3b's instrument): max frame-to-frame delta-of-
   delta on silhouette centroid, head centroid, area, over the arming window.

### 6.2 Criteria — Recon 5 §7 adopted, with these amendments

Adopted as written: **A1–A6, B1–B6, C1–C2**, applied to the arming window only
(a whole-clip gate passes the knot), thresholds as recorded (they are the
accepted clips' own measured ranges).

Amendments:

* **Add A0 (lead gate, from Recon 2 §3b): jerk ≤ ~2 px silhouette centroid,
  ≤ ~4 px head centroid, ≤ ~60 px² area, per frame, over the arming.**
  Direction 23's acceptance #1 is smoothness; this is its sharpest measured
  form (balance 1.5/4.0/53; Gen Thirteen 7.2/9.3/157; live 23/31/1168).
* **A5 (reversals) is taken from the pose tables, never from the render** —
  both Recon 3 and Recon 5 independently showed pixel-domain reversal counts
  are quantisation noise that would REJECT the balance clip. Never build a
  gate on pixel reversal counts.
* **C2 recorded as ≥ 112 frames ground time with the §2.1 design at 144**;
  if beat trimming in the eye pass pulls under ~130, that is an owner-visible
  pace decision, not a silent one.
* **B-criteria are regression floors, not targets** — Gen Thirteen already
  passes all of B; the work is A.
* Probe additions AFTER owner acceptance (probe bands are recorded
  regressions, not laws): `kSpringArmMaxStationStepMm ≈ 100` at 30 Hz keys
  over the arming only, and re-recorded phase-envelope/hold-drift bands.

### 6.3 Who checks what

* **Implementer, every stage:** the stage's own verification (§3), probe
  green, springpose schedule, commit the evidence.
* **Reviewer:** the art law's loop — look at the whole thing, in motion, at
  native resolution, beside the balance clip. Gates catch regressions; only
  looking catches wrongness. Specifically: do the four beats read in order,
  is any beat hurried, does the head read as slightly-back-and-down in the
  side view, does the hold live. Plus: diff review of stages 1/2 (the
  provable no-ops) and the landmine checklist.
* **QA:** run the legibility probe + criteria table on the final encoded
  webms (the shipped artifact, not the working renders); verify frames-after-
  launch equivalence claims; verify the archive/CSS bump; verify production
  actually serves the new bank (the -Branch trap); verify no background tasks
  left running.

### 6.4 Ruling on the Recon 1 / Recon 3 conflict (path/net 1.82 vs 1.23)

**Recon 3's 1.23 stands.** Its replica was validated against the committed
probe evidence to 2 mm on all 20 stations; Recon 1's reconstruction pinned
station 14 and excluded the shipped root solve, which inflates apparent head
path (its own §6 says the mm are indicative). Recon 1's *qualitative* finding
survives fully — the route doubles back — but the wandering lives in the TAIL
(path/net 3.85–4.30 at stations 14–15, Recon 3 §Part 3), which is where the
monotonicity work of stages 5–6 is aimed. No gate uses the head's path/net
number, so nothing downstream depends on the discrepancy.

---

## 7. Deliberately not doing, and why

* **The extreme coil, any of it** — withdrawn by Direction 23. The declared
  press allowance goes with it (owner ruling on a withdrawn motion).
* **D22 §4's "compress more" depth chase** — not restated in Direction 23's
  acceptance, and chasing depth is how the last pass wound into the knot.
  Gen Thirteen's ~64% bottom is kept this pass; if the owner re-asks, the
  knobs are `kSpringBodyFlattenQ16` and the collapsed table, one edit away.
* **D22's experimental 3D-coil triple salto** — optional appendix to a
  superseded direction; the rebuild outranks it every time.
* **Touching the life layer's structure** — proven innocent by ablation
  (63 → 63). Amplitude is an eye call in stage 7, nothing more.
* **Touching the release, flight, salto wheel, landing, idle, or balance** —
  the release stays fast (D20 §4), the salto is the accepted one, frames from
  launch onward are the byte-equivalent accepted material, idle is law.
* **A general presentation/interpolation rework** — the chord-midpoint fix is
  scoped to the spring arm schedule; `c.interpolate` and the engine's midpoint
  machinery are untouched.
* **Authoring pose values in this document** — the tables are authored by eye
  in stage 6, per the art law. This plan fixes budgets, gates and procedure,
  never a heading.
* **New gates on pixel-domain reversal counts** — measured to be noise; would
  reject the balance clip and pass the rejected bank.

## 8. Uncertainties, and the cheapest experiment for each

1. **Do the re-spaced knots + per-beat clock alone meet A2, or do the
   absorb/assembled tables need re-spacing in shape too?** Cheapest: run the
   Recon-3-style replica (or `zixx-springpose schedule`) on candidate
   constants BEFORE any render — minutes, no build of the full reel.
2. **Is 144 frames of ground time enough, or does it still read hurried?**
   Cheapest: stage 4's render (route unchanged, just stretched) is itself the
   experiment — look at it beside the balance clip before stage 5 begins.
3. **Does the +54 shift interact with any attack-phase consumer not on the
   landmine list?** Cheapest: the compile's seam gates plus the probe's parity
   gates — both loud, both already exist, both run at stage 4.
4. **How far back should the head actually travel?** Not decidable from a
   number — single-frame fixed-ortho renders of collapsed-pose candidates,
   looked at, in stage 6. The 10–25 px band is a starting bracket, not a spec.
5. **Will Gen Thirteen's restored wobble amplitudes read as life or as noise
   on the slow primary?** Cheapest: stage 7 is one render of the hold with
   two amplitude values; the A6 period floor already bounds the risk.
