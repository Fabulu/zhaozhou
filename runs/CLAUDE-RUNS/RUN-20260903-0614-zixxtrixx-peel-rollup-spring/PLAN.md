# PLAN — THE PEEL: roll up off the ground front-to-tail, stand on the tail, compress hard (Direction 25)

**Run:** RUN-20260903-0614-zixxtrixx-peel-rollup-spring
**Role:** architect. Plan only — no pose values are authored here, no builds run.
**Inputs:** Owner Direction 25 (binding — THE PEEL), Direction 24 (tail-anchor
law, qualified as D25 states), Direction 23 (smoothness — still stands),
the previous run's PLAN/TASK_LOG/REVIEW/QA/QA-2 and its five recon documents
(RECON-4 machinery map, RECON-1 pace recipe, RECON-5 legibility criteria),
the art law in `CLAUDE.md`.
**Lane:** `C:\programmieren\zencrifice\zixxtrixx-wholebody-s-spring-20260901`,
zhaozhou on `main` at `dbc0deec` (== origin/main). Line numbers below are
verified against that SHA. **Never touch the sibling live repos** — a hardware
agent is live there.

---

## 0. Headline decisions

1. **The peel is a TRAVELLING SUPPORT, not a pose trick.** `kSpringPlantSegment`
   stops being the law of the arming: the support becomes an authored
   milli-station route `spring_support_station_mk(arm)` that walks from
   station 14 back to the **tail tip** (the far end of segment 18) across the
   peel. The root solve already holds "the support station at its grounded
   baseline position"; generalising WHICH point is held makes the contact
   patch recede along the resting footprint **by construction — the contact
   point can never slide, and the tail-tip stand at the end of the peel is
   structural**, the same trick that nailed Direction 24's tail. §2 has the
   full argument and the rejected alternatives.
2. **The pose tables author the roll-up; the support route authors the
   contact.** The four-knot spline machinery survives unchanged. The two
   interior knots are re-authored (and renamed) as **peel-mid** (arm 220) and
   **tail-stand** (arm 400 — the existing entry/squash split, which does not
   move); **collapsed** (arm 1000) is re-authored as the strong compression on
   the tail tip. Tail rows are stance-aliased until the support passes them
   and **frozen from tail-stand to collapsed** — Direction 24's anchor law
   "from the moment the tail is the sole contact", again by construction.
3. **Faster: ground time drops from 168 to ≈116 frames (~31% faster).**
   Settle 4 keys / peel 28 / gather 2 / compression 16 / hold 8 —
   `kSaltoCompressEndKey` 72 → **50**, `kSaltoCompressHoldEndKey` 84 → **58**.
   The compression is deliberately the quick, strong beat ("then it
   compresses. Very strongly"); the peel is the slow, readable one ("slowly —
   but not too slowly"). Every beat still clears RECON-5's 16-frame travel
   floor; §4 derives the pace floor and the headroom.
4. **Stronger: the head goes visibly further back.** The tail-stand geometry
   itself carries the mass back over the tail; the collapsed knot then takes
   the nose to a bracketed **−20 to −35 px** on the fixed side camera (published:
   −12 px), chosen by eye. A new **law gate** — nose must stay
   `kSpringNosePastTailMarginMm` in front of the tail tip at every arming
   sample — is "it should never do that" made checkable.
5. **Clipping relaxed, declared, bounded** (§7): one declared self-press
   window over the compression, ticks **derived from the key constants**
   (never absolute — the previous run's landmine), depth-capped, ratchetable
   back down by one constant when the owner re-tightens.
6. **Keep everything the published pass earned**: the milli-key schedule with
   every arming half-key authored from it (the dead 30 Hz staircase stays
   dead), the life layer + floor, the jump camera's life-clock exclusion, the
   derived retime scaffolding (`kAtkRetimeShift`), the 23-key prime stride,
   the seam/slice naming. The smoothness machinery is not reopened — it is
   re-valued.
7. **Both zero-headroom bite declarations get re-declared with real headroom**
   while the motion is being re-authored anyway (§6, landmines 5–6).

---

## 1. What Direction 25 actually changes

The published pass performs: settle → become-the-S (rear lobe grows) → dwell →
press-the-S-flat → hold → launch, all on a **fixed station-14 belly plant** with
the tail aliased to stance in every knot.

Direction 25 replaces the middle of that with a **contact narrative**:

1. Start: the S — most of it already in the air, a stretch resting on the
   ground. (The idle stance IS this: front hook aerial at 80–138°, stations
   ~10–16 flat on the ground, tail curl down. No long "become the S" beat is
   owed any more — this is where the time savings come from.)
2. The front of the grounded stretch lifts. The lift **travels backward**:
   station after station peels until only the **tail tip** touches.
3. Standing on the tail end — possibly at an angle.
4. Compress, very strongly; head far back, never past the tail.
5. Launch (accepted salto unchanged).

The action IS the contact point travelling backward. That is a statement about
**which point the root solve holds to the ground**, which is why the mechanism
below is in the support machinery and not in another shape instruction.

---

## 2. The mechanism — the travelling support, decided and justified

### 2.1 What exists (verified at dbc0deec)

* `spring_support_origin_raw` (`zixxtrixx.h:2276`) walks the real quantised
  quat chain over the fixed prefix `b = 0..kSpringPlantSegment` (14) and
  returns that station's origin.
* `spring_root_from_quats_raw` (`:2311`) walks the **grounded baseline** and
  the **posed chain** to the same station and returns
  `baseline − posed + target_y`: the root offset that plants the posed
  station 14 exactly where the grounded station 14 rests (plus the authored
  lift/bite routes).
* Head motion is a consequence of articulating around that plant — never
  authored directly (`:2332` comment; RECON-4 §2).

### 2.2 The consequence nobody can author around

**While the plant is a fixed station 14, station 14 cannot leave the ground.**
The root solve exists to pin it. But the peel requires stations 10 through 18
to leave the ground *in sequence*, with the contact always at the un-peeled
front. Two ways to fake it with the existing machinery, both rejected:

* **(a) Pose tables alone, plant fixed at 14, "grounded stretch shortens from
  the front."** Stations 0–13 can peel (they are forward of the plant), but
  the moment the peel must pass station 14 the mechanism inverts: 14 must
  rise, which only the lift route can do — and the tail hangs off the
  station-14 walk, so lifting the plant lifts the still-grounded tail with
  it. Keeping the tail down would mean counter-authoring tail headings
  against a moving reference so that pose × lift-route arithmetic happens to
  land the tip on the dirt. That is grounding as an *emergent accident* —
  exactly the indirect-authoring class that failed six passes, and the exact
  opposite of what Direction 24 taught: the thing that must hold is held **by
  construction**.
* **(b) Lift-route driving (raise station 14's target through
  `kSpringOpenSupportLift`)** — same failure, stated in millimetres instead
  of headings, plus it burns the route that exists to author *bite*, not
  flight.

### 2.3 The decision

**Generalise the support walk to a milli-station parameter and author the
support's position on the arming as a route.**

```
support_mk(arm): 14000 ................. tail tip (19000)
                 |--- holds 14 ---|--- travels tailward ---|--- holds tip ---|
arm:             0            kSpringSupportPeelBeginArm   kSpringArmAssembledAt..1000
```

* `spring_support_origin_raw(quats, support_mk, x, y, z)`: walk the prefix to
  `floor(support_mk/1000)`, then advance `frac` of one segment length along
  that bone's own axis (the same `m[3] -= fxm(seg)` direction, scaled). At
  `support_mk = 19000` this is the **far end of segment 18 — the tail tip**.
  The old behaviour is `support_mk = 14000` exactly.
* `spring_root_from_quats_raw` takes the same `support_mk` and walks **both**
  the baseline and the posed chain to it. The posed support point is therefore
  held at the **grounded baseline position of that same body point** — i.e.
  each successive contact point is planted precisely where it was already
  resting in the footprint. **Contact never slides, at any interpolant, ever.**
  When `support_mk` reaches the tip and stops, the tail tip is the planted
  anchor and Direction 24's law re-engages structurally.
* `support_mk` is derived inside `spring_anchor_offset` /
  `spring_root_from_quats_raw` callers from `(entry, squash)` via the existing
  round-trip `spring_arm_amount` (`:2003`) and the new named route — **no call
  signature changes at the consumer level**, so all seven spring consumers
  (slot 3, slices, dummy/fly/six/nine, jump-one/multi) derive identically and
  the bit-exact parity gates keep meaning something.

Why this is right, beyond necessity:

* It is the **literal statement of the owner's sentence**. "The contact patch
  travels progressively rearward" becomes one monotone authored route.
* The peel's ground contact is **exact by construction** on both sides: behind
  the support, un-peeled stations sit at stance headings → they rest in their
  own baseline footprint at their declared bite; at the support, the plant is
  honest; forward of it, peeled stations lift as articulation. The pose
  tables only have to make the peeled stretch *look* like a roll-up — they
  no longer carry any grounding obligation.
* It reuses the project's one proven trick (D24's alias-to-stance = "no route,
  no fault") at the next level up: no relative motion between contact and
  ground because there is no route between them.

Cost, named: the support's world X now travels during the peel (it must — the
pivot walks backward). The probe gate at `zixx_probe.cpp:1759` pins support
X-drift to ≤1 mm against the station-14 formula; it is re-authored to check
≤1 mm against the **new** formula plus a new law: the support's X advances
**monotonically tailward, never forward** (landmine 2). Continuity: the lerped
origin is C0 with second-order kinks where `floor(support_mk)` steps; at ~10
frames per station of travel they are sub-millimetre — verified with the
springpose velocity column before anything renders (§9 experiment 1).

### 2.4 The pose knots that ride on it

Four knots, same spline (`spring_route_heading`, knots {0, 220, 400, 1000},
C1, bit-exact endpoints — machinery untouched):

| knot | arm | table | contact state | tail rows 15–18 |
|---|---|---|---|---|
| grounded | 0 | `kStanceSlope` alias (idle law — never edited) | resting stretch, support 14 | stance |
| **peel-mid** | 220 (`kSpringArmAbsorbAt`, kept) | re-authored, rename `kSpringPeelHeading` | front of stretch peeled (10–14 lifting), support ≈ mid-tail | **stance** (not yet passed) |
| **tail-stand** | 400 (`kSpringArmAssembledAt`, **not moved** — double-duty split, landmine 4) | re-authored, rename `kSpringTailStandHeading` | sole contact = tail tip; body rolled up, at an angle | **stand headings** (authored by eye) |
| **collapsed** | 1000 | `kSpringCollapsedHeading` re-authored | tip planted, compressed hard | **== tail-stand rows, verbatim** |

Tail rows equal on the last two knots ⇒ the tail's route from sole-contact to
launch is a constant — Direction 25's "from then on it does not slide, invert
or change size", by construction. During the peel (grounded→peel-mid→tail-
stand) the tail rows DO change — that is the roll-up itself, and Direction 25
explicitly scopes the anchor law to after sole contact.

Entry/squash semantics survive intact: **entry (arm 0→400) = the peel; squash
(400→1000) = the compression.** The deform (flatten/spread) is keyed on squash
→ the body flattens during the strong compression, not during the peel —
correct for D25 with zero re-plumbing. `kSpringSqueezeDeformLead = 340` gives
a slight pre-flatten as the body settles onto the tail; keep, judge by eye.

---

## 3. Staging — ordered, independently checkable, smallest risk first

Every stage: `build-direct.sh` (ONE target per call, `--clean` after struct
changes), probe, commit, push. Renders and CSVs beside the stage in this run
folder. The stale-binary tell applies throughout.

**Stage 0 — baseline.** Render `zixxtrixx-spring-side` + `zixxtrixx-jump-one`
at HEAD; run the committed instruments (previous run `evidence/qa/qa_*.py`,
`evidence/qa2/qa2_*.py`, `tools/reel/zixx_legibility.py`) and commit the
CSVs/sheets as the "before" of every before/after pair.
*Verified by:* probe green at HEAD; spring-side CRC `0x1B1AEAB6` reproduces
(QA-2's number — proves the build is the published one).

**Stage 1 — the support generalisation as a provable no-op.**
`spring_support_origin_raw` / `spring_root_from_quats_raw` gain the
`support_mk` parameter; the new route constant-returns
`kSpringPlantSegment * 1000`. Nothing else changes.
*Verified by:* full 22-subject bank BYTE-IDENTICAL (per-subject sequence CRCs
vs stage 0 — the previous run's stage-1 method); probe green. This converts
the central mechanism from a risk into a constant edit before any motion
exists.

**Stage 2 — instruments first, poses on paper.** Extend `zixx_springpose`
(comparison side only, asserts nothing): `schedule` gains columns for
`support_mk`, support world x/y, tail-tip world x/y, nose-to-tip margin, and
root velocity. Then bracket candidate **tail-stand** and **collapsed** tables
numerically and render them as single fixed-side stills (`pose` mode +
one-frame renders) — the by-eye choice of the stand's angle and the
compression's depth happens HERE, on stills, before any clip is wired.
*Verified by:* chosen stills read as (a) a roll-up arrested mid-peel, (b) an
animal standing on its tail end, (c) a strong compression with the nose
clearly behind its rest position and clearly in front of the tip. Owner's eye
loop, not a number.

**Stage 3 — the retime skeleton (old motion, new clock).**
`kSpringSettleInKeys` 4 (unchanged), `kSpringPeelEndKey = 32`
(replaces `kSpringBecomeSEndKey`), `kSpringPeelSettleKeys = 2`,
`kSaltoCompressEndKey = 50`, `kSaltoCompressHoldEndKey = 58`.
`kAtkRetimeShift` derives (+40), `kAttackKeys` asserts, all seam/slice/curve
literals slide (already derived — previous run's stage 1). Re-derive the
probe's phase-envelope relationships (`zixx_probe.cpp:1314`) around the new
schedule, citing Direction 25. Old knots still in place — this stage plays the
*published motion faster* and nothing else.
*Verified by:* creature compiles (seam gates ARE the +40 consistency check);
probe green; parity green; `zixx-springpose schedule` `move_mm` scales as
predicted; smoothness instruments on a fresh render — sil-XOR med must stay
under the balance clip's 3.37 %/f (published was 0.92; ~1.4 expected at 1.45×
speed). **This render is also §9 experiment 2**: does ~116 frames of ground
time read deliberate or hurried, beside the balance clip.

**Stage 4 — the peel.** Support route live
(`kSpringSupportPeelBeginArm ≈ 150` → tip at `kSpringArmAssembledAt`), knot
tables peel-mid and tail-stand installed (from stage 2's chosen stills),
entry-side lift route re-authored near-zero (the support rides the footprint
at its resting bite — the old whip-era lifts die), probe support gate
re-authored (travelling formula, ≤1 mm, monotone-tailward), life-wave tail
share zeroed from tail-stand onward (landmine 12), the new **contact-front
instrument** committed (§8).
*Verified by:* probe green (incl. the pose-probe ground walk — peeled stations
clear the terrain, un-peeled stations keep their declared bite); contact-front
pixel track on the fixed camera recedes monotonically to the tail tip and
never re-grounds; tail-tip world drift ≤1 mm across the whole arming; render +
every-frame contact sheet, judged by eye: **does the peel READ**.

**Stage 5 — the compression.** Collapsed knot installed (stage 2's choice,
adjusted by eye in the loop), tail rows verbatim-equal to tail-stand,
squash-side lift/bite re-authored **with headroom** (new
`kSpringCollapsedSupportLiftMm`; `kSpringDeclaredLoadedBiteMm` re-declared for
the tail-tip press, ≥10 mm clear of the observed deepest sample), the
nose-past-tail law gate added to the probe, the declared self-press window
added if (and only if) the chosen pose presses (§7).
*Verified by:* probe green; nose −20..−35 px back on the fixed camera
(bracket, final value by eye), monotone, margin to tip ≥ gate; compression
shape-rate max under 20 %/f (A2 cap — §9 experiment 3 if breached); launch
eye-check (§9 experiment 4).

**Stage 6 — life layer and polish, by eye.** Wobble periods 23/51 keys against
a ~50-key arming deliver ~2.2 and ~1 visible cycles — likely fine; amplitudes
re-judged at native res on the slow peel; hold quiver re-judged on the 8-key
hold and `kSpringHoldLivingDriftMm` re-recorded for it (landmine 9); chain-lag
envelope re-judged (a lag reads differently on a peel — the peel itself is
already a travelling delay; zero it if it doubles the read).
*Verified by:* contact-drift gate still ≤1 mm (the wave must never move the
support); A-criteria re-run; the eye.

**Stage 7 — full battery, encode, publish.** Full 22-subject render (15
byte-identical subjects must still be byte-identical; exactly the seven
spring consumers may change — QA-2's split); full instrument battery +
contact sheets + before/after pairs vs stage 0; archive the outgoing bank
(bump `MAX_ARCHIVE_GENERATIONS` AND both CSS selector families) BEFORE
re-encoding; `tovideo.py` then `togif.py`; publish
`deploy.ps1 -Project upheaval -Branch main` — finished creature pass, standing
authorisation, `-Branch` mandatory. Then reviewer, then QA, per the
established flow.

Stages 1 and 3 are provable no-ops or machinery-only. Stage 4 is the pass's
identity; stage 5 is its strength. If a stage-4 render already reads as the
owner's sentence, do not gold-plate stage 5's press — strong is a pose fact,
not a depth chase.

---

## 4. Timing — how the beats divide, how much faster, and the floor

| beat | keys | frames | arm | reads as |
|---|---|---|---|---|
| settle-in | 0–4 | 8 | 0 | alive, undecided (life floor already ramps here) |
| **THE PEEL** | 4–32 | 56 | 0 → 400 | contact recedes ~5 stations + tip ≈ 10 f/station — a slow roll, "not too slowly" |
| gather | 32–34 | 4 | 400 | arrival on the stand registers (owner knob; may be 0) |
| **COMPRESSION** | 34–50 | 32 | 400 → 1000 | strong and quick — the deliberate contrast to the peel |
| loaded hold | 50–58 | 16 | 1000 | living brace |
| release | 58+ | — | release arms | unchanged 4-key structure |

Ground time **116 frames ≈ 1.93 s** vs the published 168 ≈ 2.80 s: **~31%
faster**, which answers "too careful and too slow" without abandoning the
smoothness that finally passed.

**The pace floor** (RECON-5, derived from the accepted clips): every beat that
must read needs ≥16 frames of travel; settles ≥6 frames; half-life never below
16 (A1); shape rate med ≤7, p90 ≤12, **no frame over 20 %/f** (A2). Direction
25 has TWO ground beats, so the four-beat 112-frame floor no longer binds; the
two-beat floor is ≈ 8 + 32 + 4 + 32 + 16 ≈ **92 frames**. The design sits at
116 with both beats at 2–3.5× the travel floor.

**Does "faster" threaten the floor?** The published pass ran sil-XOR med 0.92
%/f, p90 5.5, half-life med 56 — between 2× and 3.5× inside every A-gate. A
1.45× speed-up spends roughly half that headroom; the compression, now doing
MORE travel (stronger) in half the published beat time, is the one place the
A2 20 %/f frame cap could bind — measured at stage 5, and the first remedy is
`kSaltoCompressEndKey` 50 → 54 (compression 16 → 20 keys), a one-constant
edit. The peel at 56 frames is comfortably inside everything.

All five key-line numbers are owner knobs; the beat proportions are the
authored opinion, the gates only catch the floor.

---

## 5. Named constants

New (owner knobs unless marked derived):

| constant | controls |
|---|---|
| `kSpringSupportPeelBeginArm` (≈150) | arm at which the support leaves station 14 |
| `kSpringSupportStartStationMk` (derived: `kSpringPlantSegment * 1000`) | where the walk starts |
| `kSpringSupportTailTipStationMk` (19000) | the far end of segment 18 — the stand |
| `spring_support_station_mk(arm)` route | the peel itself; breakpoints named, sorted, monotone |
| `kSpringPeelEndKey` (32) | end of the peel beat (replaces `kSpringBecomeSEndKey`) |
| `kSpringPeelSettleKeys` (2) | the gather on arrival at the stand (0 allowed) |
| `kSpringNosePastTailMarginMm` (≈50) | law gate: nose stays this far in front of the tip |
| `kSpringPeelPressFullMm` / `MicroMm` + derived window keys | the declared self-press (§7), only if the chosen pose presses |
| `kSpringTailStandHeading` | the tail-stand knot (renamed from assembled) |
| `kSpringPeelHeading` | the peel-mid knot (renamed from absorb) |

Existing, re-valued: `kSaltoCompressEndKey` (72→50), `kSaltoCompressHoldEndKey`
(84→58), `kSaltoSpringEntryEndKey` (derives from `kSpringPeelEndKey`),
`kSpringCollapsedHeading` (the strong press), `kSpringCollapsedSupportLiftMm`
(re-authored for the tip press, with headroom), `kSpringDeclaredLoadedBiteMm`
(re-declared, ≥10 mm headroom), `kJumpLandingLoadedBiteMm` (re-measured, then
re-declared with ≥10 mm headroom), the `kSpringOpenSupportLift` values
(near-zero: the support rides the footprint), `kSpringHoldLivingDriftMm`
(re-recorded for the 8-key hold), the wobble amplitudes (stage-6 eye call).

Existing, untouched by intent: `kSpringArmAbsorbAt` (220),
`kSpringArmAssembledAt` (400 — double duty, does not move),
`kSpringSupportCompensation` (1000, owner knob), `kStanceSlope` (idle law),
`kSaltoUnrollStride` (23, structural), the release arms 660/380/160 (§9
experiment 4 may re-space them), the deform strengths, the schedule/half-key
authoring machinery.

Every value lives in `zixxtrixx.h` as a named, editable constant (art law §6).

---

## 6. Landmine checklist (file:line verified at dbc0deec)

1. **`zixx_probe.cpp:1314`** — phase-envelope gate, re-authored last run as
   derived relationships *for the become-S structure* ("beat 1 ≥ half the
   arming" etc.). The peel/compress split violates it. Re-derive for
   Direction 25 in stage 3, same derived-relationship style.
2. **`zixx_probe.cpp:1759`** — "spring station-14 support left its authored
   per-sample path": hardwired to the fixed station and ≤1 mm X-drift. With
   the travelling support this gate would fail the CORRECT motion. Re-author:
   ≤1 mm against the new formula + monotone-tailward law + tail-tip drift
   ≤1 mm from tail-stand on.
3. **`zixx_probe.cpp:2502–2567`** — route stall/jump gate: skips a station
   only when **all four** knots are equal. Tail rows will now differ between
   peel-mid and tail-stand but be equal tail-stand→collapsed: a 600-arm-unit
   authored freeze that the gate calls a stall. Extend the guard to per-leg
   equality (a leg whose two bounding knots are equal is an anchor, not a
   stall). Also re-check the ≤256 a16-per-arm-unit step: tail rows rotate
   ~150° over 180 arm units ≈ 150/unit — inside, but station 17/18 must be
   checked, not assumed.
4. **`zixxtrixx.h:823`** — `kSpringArmAssembledAt` is BOTH the tail-stand
   spline knot AND the entry/squash split AND the release-curve pivot
   (`:2456–2463`). It stays at 400. Put the tail-stand THERE rather than
   moving IT.
5. **`zixxtrixx.h:881` / `:875`** — the spring bites **−60 mm against a 60 mm
   declaration — zero headroom** (QA-2 watch item). The collapsed support
   lift and loaded bite are re-authored this pass anyway: declare the new
   values with ≥10 mm headroom, as an owner-visible declaration.
6. **`zixxtrixx.h:7222` region** — `kJumpLandingLoadedBiteMm = 54` against an
   observed −53 (**1 mm headroom**, QA-2 watch item). The landing WILL move:
   the release arms sample the re-authored spline, so the landing tail pose
   changes. Re-measure at stage 5, re-declare with headroom.
7. **The declared-press trap** (previous run, §5.3): press windows must be
   **derived from the key constants**, never absolute ticks — an absolute
   window silently lands on the wrong frames under any retime. §7's window is
   `[2*kSaltoCompressEndKey_prevBeat .. 2*kSaltoCompressHoldEndKey]`-style
   derivations only.
8. **`zixxtrixx.h:2437`** — `heading_override` on the release midpoint control
   is still the loaded gun that re-imposes off-route poses. Do not touch.
9. **`zixxtrixx.h:892`** — `kSpringHoldLivingDriftMm = 90` was re-recorded
   against a TWELVE-key hold; this pass's hold is 8 keys. Re-record; the
   plant-slide half (0 mm) is law and stays.
10. **`zixxtrixx.h:2148–2160`** — the life floor ramp derives from
    `kSpringSettleInKeys` and the life envelope from `kSaltoCompressEndKey` —
    both derive correctly, but the wave's **tail share**
    (`kSpringLifeSupport0 = 15`, share 260, `:2100` region) would wobble the
    STAND. From tail-stand onward the supporting tail must carry zero wave
    (the contact-drift gate is the tripwire).
11. **`zixxtrixx.h:893`** — lift-route breakpoints must stay sorted (a
    build has broken on this before). The re-authored near-zero route keeps
    the breakpoint keys, changes only values, unless simplified — then
    re-sorted by construction.
12. **The 180° unwrap degeneracy** (previous run, landmine 12): no station may
    differ by exactly 32768 between adjacent knots — the shortest-arc unwrap
    flips direction on a ±1 edit. The tail-stand rotation (~150°) authors
    close to it; check every tail row pair when the tables land.
13. **Probe provenance representative keys** (moved to 20/50 last run,
    `zixx_probe.cpp` authored-vs-generic tripwire) are pinned to the 72-key
    schedule — re-derive for the 50-key one or the tripwire proves nothing.
14. **`spring_root_from_quats_raw` call sites with baked quats**
    (`zixxtrixx.h:2664, :3281, :7047, :7612, :7626, :7687`) must all derive
    `support_mk` from the SAME (entry,squash)→arm→route path or the bit-exact
    parity gates fail across consumers. One derivation helper, used
    everywhere.
15. **Seam/slice/curve literals** — all derived since the previous run's
    stage 1 (`static_assert`s at `:996–1000` pin them). Trust but verify: the
    compile-enforced seam gates are the tripwire for the +40 shift.
16. **Build traps**: one target per `build-direct.sh` call (a two-target call
    silently builds only the last); `--clean` after struct-layout changes; a
    measurement that did not move after a change that must have moved it is a
    stale exe; kill background builds before declaring a stage closed.
17. **Encode/publish traps**: encoding overwrites `public/renders/` in place —
    archive the outgoing bank (bump `MAX_ARCHIVE_GENERATIONS` AND both CSS
    selector families) first; `deploy.ps1` without `-Branch` silently ships a
    PREVIEW.
18. **The idle law**: `kStanceSlope` (`:714`) is the grounded knot AND the
    idle. It is not edited, and the 15 non-spring subjects must stay
    byte-identical (QA-2's split is the expectation: exactly 7 CRCs may
    change).

---

## 7. The clipping allowance (Direction 25: relaxed, bounded, declared)

**What is permitted:** self-overlap only, during the compression — the head
and neck coming far back over the rolled-up body may press into the coil.
Cap: `kSpringPeelPressFullMm ≈ 60` / `kSpringPeelPressMicroMm ≈ 110` at the
deformed skin (the coarser micro rung reads the same press deeper — previous
run's 52/96 precedent), station pairs ≥7 apart, window derived as
`[2*(kSpringPeelEndKey + kSpringPeelSettleKeys) .. 2*kSaltoCompressHoldEndKey]`
presentation ticks — **derived, never absolute** (landmine 7).

**What is NOT relaxed:** the ground-contact law (D25 says so explicitly) —
peeled stations clear the terrain, grounded stations keep their declared bite,
the pose-probe's vertex walk enforces both; the hold, release and airborne
phases stay zero-intersection law; anything outside the declared window is a
fault.

**How it is declared:** the named constants above plus a comment block citing
Direction 25's "for now even some overlap and clipping is okay, but keep it in
check", so the next reader knows it is a temporary owner ruling on THIS
motion.

**How it tightens later:** ratchet `kSpringPeelPressFullMm` down (and delete
the window when it reaches the deform-absorbable ~40 mm) once the owner
accepts the motion — one constant, one probe re-run. If the stage-5 pose
chooses not to press at all, the allowance is **not created** (do not inherit
allowances the motion does not need).

**The eye rule:** the overlap must never be what the eye lands on — checked on
the every-frame contact sheet at native res, not only by the depth number.

---

## 8. Verification plan — in pixels, on the fixed camera

The project's signature failure is a world-space number describing a sub-pixel
event. Primary verification is therefore **pixels on `zixxtrixx-spring-side`**
(fixed camera, the honest view; ~41 mm/px), using the calibrated instruments
already committed in the previous run's `evidence/qa/` and `evidence/qa2/`
(`qa_region.py`, `qa2_*.py`, `zixx_legibility.py`) — QA-2 proved they
reproduce to the digit across builds. Orientation on that camera: tail at
screen x ≈ 105–110, head at x ≈ 230.

**New committed instrument — the contact-front tracker** (one script, built
from `qa2_region`'s segmentation): per frame, the span of silhouette pixels
within N px of the ground line; its **head-most edge** is the contact front.

The reviewer and QA check, in this order:

1. **LOOK FIRST** (art law): every-frame contact sheet of the ground phase +
   the launch, at native res, beside the balance clip. Does the peel read as a
   roll-up? Does the animal stand on its tail end? Is the compression strong?
   Gates catch regressions; only looking catches wrongness.
2. **The peel** (D25 acceptance 1–2): contact-front x recedes monotonically
   from ≈ station-10's column to the tail tip's (≈105–110), never re-advances;
   final ground-band contact is a single small patch at the tip. 3D
   corroboration from the committed pose probe: grounded-vertex set per tick
   shrinks from the front, never regrows.
3. **The stand + anchor** (D24 within D25): tail-tip pixel route flat from the
   sole-contact frame to launch (the QA-2 fin-tip/bbox instruments, re-pointed
   at the tip); probe: tip world drift ≤1 mm.
4. **The compression** (acceptance 3–4): nose track — further back than
   −12 px (bracket −20..−35, final by eye), monotone, and
   `nose_x − tip_x` ≥ the margin gate at every frame; visibly-stronger check
   is a before/after pair of loaded poses, published vs new, same camera.
5. **The pace** (acceptance 5): ground time ≈116 frames; A1 half-life ≥16
   everywhere; A2 med ≤7 / p90 ≤12 / max <20 %/f; sil-XOR med below the
   balance clip's 3.37 %/f; 30 Hz odd/even parity ≈1.0 (the staircase stays
   dead); beats segment (settle / peel / gather / compress / hold / launch).
6. **The clipping** (acceptance 6): probe window/depth green; press not the
   eye's landing point on the sheets.
7. **Regression floor**: 15 of 22 bank CRCs byte-identical, exactly the seven
   spring consumers changed; accepted-salto flight compared like QA-2 did;
   probe PASS end to end; landing bite re-declared with headroom; production
   actually serves the new bank (the `-Branch` trap).

QA runs on the **encoded webms** (the shipped artifact), not the working
renders, per the established flow.

---

## 9. Uncertainties, and the cheapest experiment for each

1. **Does the lerped travelling-support origin move the root smoothly?**
   Springpose `schedule` root-velocity column across the peel, before any
   render — minutes. If station-boundary kinks show, smooth the origin lerp
   with the existing eased lerp; one function.
2. **Does ~116 frames read deliberate or hurried?** Stage 3's render IS the
   experiment (old motion, new clock), looked at beside the balance clip
   before the peel work begins. Knobs: the two beat-end keys.
3. **Does the 16-key compression breach the A2 20 %/f cap with the bigger
   travel?** Measure on stage 5's first render; remedy is 16 → 20 keys, one
   constant.
4. **Does the release (the peel run backward in 4 fast keys) read as a
   launch?** It should — unrolling forward off the tail IS the natural jump —
   but the release arms 660/380/160 were spaced for the old route's geometry.
   Stage-5 eye check of keys 58–66; remedy is re-spacing three constants.
5. **The stand's angle and the head's depth** — not decidable by number.
   Stage 2's still-image brackets, chosen by eye, are the experiment; the
   nose-margin law gate bounds the risk while the eye chooses.
6. **Does the chain-lag read as a second peel?** Stage-6 A/B render with the
   lag envelope zeroed; keep whichever reads as one body.

---

## 10. Deliberately not doing

* **Any "become the S" beat.** Direction 25 starts from the S the idle already
  is; the owner withdrew the shape instruction and the time it consumed is
  where "faster" comes from.
* **Moving `kSpringArmAssembledAt`,** the spline machinery, the half-key
  authoring, the interpolation stack, or the engine midpoint machinery — the
  smoothness these bought is an acceptance requirement (D23 stands).
* **Touching the salto wheel, flight curves, idle, balance, or `kStanceSlope`.**
  The accepted salto is unchanged (D25); idle is law.
* **A physics/IK solve for the contact.** The route is authored, the tables
  are authored — every value stays an owner knob (art law §6).
* **Re-tuning `kSpringSupportCompensation`, the deform strengths, or the
  flatten/spread pair** unless the stage-5 eye loop demands contact relief —
  and then as single named edits, not a sweep.
* **New pixel-domain reversal gates** — measured noise (two recons agree);
  reversals are taken from the pose tables only.
* **Chasing the D22 depth constants** — "compress very strongly" is a pose
  authored by eye at stage 2/5, verified in pixels, not a flatten chase.
* **Publishing anything mid-iteration.** The one publish is stage 7's finished
  pass, under the standing bestiary authorisation.
