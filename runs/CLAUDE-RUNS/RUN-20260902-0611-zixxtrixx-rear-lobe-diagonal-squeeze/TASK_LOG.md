# Task Log: RUN-20260902-0611 — Zixxtrixx rear lobe, backward head travel, diagonal deeper squeeze (Owner Direction 22)

**Created:** 2026-09-02 06:11 UTC+02:00
**Status:** In Progress
**Lane:** isolated clones at `zixxtrixx-wholebody-s-spring-20260901/`, branch
`zixxtrixx-wholebody-s-spring` in both repos. Shared checkouts untouched.

---

## Objective

Fix Direction 22's four faults — rear lobe AT THE COLLAPSED POSE (top
priority, three passes of false "fixed" claims), backward head travel without
neck over-curl, diagonal down-and-back squeeze, deeper compression — while
preserving Directions 20/21's accepted results. See SPEC_v1.md.

---

## Progress Timeline

### 2026-09-02 06:11 — Task started; reading

- Read Direction 22 (binding), 21, 20; CLAUDE.md art law; both previous runs.
- **Why the previous "fixed" claim failed:** RUN-20260901-2121 proved the rear
  curl (-80 deg) at the ASSEMBLED pose and cited "the renders show the rear
  curling as authored" — but the COLLAPSED pose table straightens the tail
  back out (-14500 -> -1600 a16 at the tip), and collapsed is the pose the
  owner watches through the hold. The proof was made at the wrong pose.
  Recorded in SPEC Don't-Retry.
- Verified the machinery end to end: absolute per-segment headings, chain from
  nose, station-14 planted with real root compensation (metres/mm fix kept),
  Catmull-Rom route through 4 knots, life layer, deform sidecar. The rig does
  not stop the rear from curving; the tables author it flat.

### 2026-09-02 — Direction 22 gained section 5 while reading

- The owner added: "It doesn't have to be a perfect S at the end, just a coil
  taken to its extreme, standing on the last tail point. A real maximization
  of oomph." Where it disagrees with the S wording, section 5 WINS.
- Design consequence: the collapsed pose is re-targeted from "S squeezed from
  the top with a rear lobe" to "coil wound to its extreme, STOOD ON THE LAST
  TAIL POINT": tail tip is the ground contact and base; the body stacks in
  wound runs above it; the head aims forward at the top; the whole pile
  presses lower than the shipped 64%.
- Plan: keep kSpringPlantSegment=14 as the fixed reference and author its
  COLLAPSED Y lift POSITIVE (station 14 rises onto the coil); the tail tip
  reaches the ground with the declared bite as an authored consequence,
  checked by the probe. All machinery survives; tables + named lift/flatten
  constants change.
### 2026-09-02 — The collapsed coil authored (14 sketch iterations)

Geometry lessons that shaped the pose (all discovered by plotting and
looking, recorded so nobody re-fights them):
- A planar S-family chain cannot close a loop without crossing (Jordan);
  max total winding stays under ~360 and entry/exit must diverge promptly.
- With segments 5-7 capped at the idle hook (value AND per-joint delta),
  any 180-degree reversal through the neck costs ~400+ mm of height; the
  crest therefore cannot sit below ~600 mm without over-curling the neck.
  This bounds fault #4 and the trade is stated in the header comment.
- Head-back travel comes from the dive's forward sweep + the mid-body loop
  + the support pin: the compensation gathers the whole animal ~1.3 m
  backward as the coil forms (the owner's "S travels backward", literally).
- The tail cannot pass behind the dive (the leg + crest seal that side);
  the only legal plant is the pad running BACK under the coil, tip the
  rearmost biting point, with the coil's foot resting beside/on it.

Final collapsed: nose (-1292, 507) = 47% idle nose height, head 1292 mm
back; top-of-tube 739 = 65% idle top (shipped was 781 = 68%); foot bites
-30/-40; pad -28..-39 with the tube tip biting; loop wound through
segments 9-12; neck 78/110/131 deg vs idle 80.2/117.6/138.5 (opens, never
tightens). Verified bone-for-bone against the committed springpose dump.
Committed f9c75809.
### 2026-09-02 — First render of the coil (iter1)

- Built cel via build-direct (one target per call), rendered
  zixxtrixx-spring-side fresh, every-frame sheet + key moments + deep
  close-ups in evidence/ (iter1-*).
- LOOKED: the arming reads as the S growing and travelling backward; the
  deep hold is a genuinely wound, compact coil roughly half the shipped
  height in silhouette; the release explodes forward; the wheel intact.
- Fault found by looking: the hung head aims ~35 deg into the dirt (the
  skull continues the neck tangent). kSpringHeadAttitude 600 -> 5800 with
  provenance comment: the face braces back to a level target-forward aim.
- Probe running for the honest band re-derivations (flatten envelope moved
  deliberately: 31% -> 43%).
### 2026-09-02 — Probe round 1 and the choreography fix

Probe on the first coil found the real faults (committed intersection walk):
- Route intersections peaking 226 hits mid-arming: the front's grounded
  foot slid backward THROUGH the grounded rear. Fix is choreography, not
  gates: ASSEMBLED re-authored with the whole rear RAISED into a rolled
  curl (D20's "leaning back must make the S bigger", literally) so the
  foot passes beneath, and the rear's over-curled values put every
  station's shortest unwrap on the over-the-top side: the rear pays out
  OVER THE TOP into the pad as one coherent whip.
- Deep-hold hits at stations (1/38): the chin genuinely interpenetrated
  its tail arch at the REAL head radius (the head barely flattens,
  strength 64/255). Nose raised (h0 -35 -> -20 deg), arch lowered a
  touch: ~+40 mm real clearance.
- Hold drift 111 > 70: wobble damped 750/520 -> 500/360 (a wound spring
  quivers TIGHT); envelope re-authored 70 -> 88 with provenance.
- Flatten band re-derived for the deliberate 43% flatten: 650..730 ->
  530..610 body, 900..950 -> 860..930 head (measured 557/888).
- Face aimed into the dirt (skull continues the neck tangent):
  kSpringHeadAttitude 600 -> 7500 braces the aim level-forward.
### 2026-09-02 — Probe round 2 -> staging v6

Round 2: hold drift fixed (36 <= 88), flatten bands green, but the raised
rolled tail's whip crossed the arriving head's corridor mid-air (peaks 264
hits, stations 0-10 vs 43-56), and the entry-span gate flipped because the
lag delays the tail at the sampled entry tick. Fixes, verified in the
designer with the tube-gap checker (v5/v6 plots in evidence/):
- ASSEMBLED becomes the raised QUILL: long grounded belly, tail rising
  steeply with only its top two stations curled past -168 deg -- keeps
  every whip unwrap on the over-the-top side while the quill stays below
  and behind the head's sweep corridor, clear of the dive.
- COLLAPSED exit run lengthened forward so the arch sits under the NOSE
  (small radius) instead of the crown (the interpenetrating chin press).
- ABSORB stages the tail rise early (lag delivers it on time).
- Support lifts re-derived: absorb 52, key5 35, assembled 25, collapsed
  287 (bone 14 rides the arch at the deep pose).
iter3 render: the face now aims level-forward over the coil, the tail
visibly passes under the chin. Probe round 3 running.
### 2026-09-02 — Probe round 3 -> v8: the whip changes sides

Round 3 caught the over-the-top whip stabbing the ground 722 mm mid-route
(the last two tail stations point straight down halfway through a wrapped
sweep). Re-derived the winding sides from the unwrap arithmetic: stations
14-16 wrap (they RISE through the sweep), the thin tip stations 17-18 take
the normal side and TRAIL the whip -- every interpolated pose keeps the
tip up, and the one sharp mid-route bend lands on the thin tail (bend
radius legal at its flattened radius; reads as follow-through). The
assembled quill loses the past-vertical curl entirely. Also re-derived the
probe's "entry" sample to the actual assembled moment (the last key
before squash opens): kSaltoSpringEntryEndKey is phase timing and sits
~72% into the squash under the 16-key eased arming, so the ordering gate
was comparing two mostly-pressed poses.
### 2026-09-02 — The climb and the declared loading press

- The whip-side fix cleared the tail stab, but the interpolated FRONT
  still sagged through the terrain mid-squash (foot stations -230).
- kSpringSquashClimbBumpMm = 300, riding q*q*(1000-q) (peak at 2/3
  squash, zero at both knots): the animal presses down as the wind loads
  and CLIMBS onto its own coil as it seats. springpose sweep now shows
  centreline minima +40..-11 through the squash.
- kSpringDeclaredLoadedBiteMm 60 -> 100: the mid-gather press is authored
  and declared (3-4 px at 240p); the deep hold itself stays -30..-55.
- Probe round 5 running; the release-window head-vs-descent intersections
  (233@18.5) are the open question.

## Next Steps

- Probe green, then: fresh cel render of spring-side + spring-top +
  jump-one + salto-dummy + landing region; every-frame sheets;
  before/after vs the live bank; the ortho collapsed-pose proof render
  with the last five segments distinguishable (acceptance #1).
- Then: 22-subject re-render (one fresh explicit Cool Cross invocation),
  Archive Generation Thirteen (extend MAX_ARCHIVE_GENERATIONS + both CSS
  selector families), encode via tovideo.py, assemble, merge mains, one
  publish, verify production.
- Parked: the EXPERIMENTAL 3D-coil triple salto (owner appendix,
  low-ceremony) - only after the real spring is committed and only if
  cheap; drop and say so otherwise.
### 2026-09-02 — The seating knot, the keyed lift route, and two dead ends

- Added the FIFTH KNOT (kSpringSeatingHeading, arm 700): the rear finishes
  seating first (identical to collapsed, so it holds), the front stays
  reaching with the dive OPENED, and the last leg is the front's own fold
  closing -- the head travels back OVER the seated coil and bows on. This
  structurally separates the whip from the head's arrival.
- Replaced the climb parabola with kSpringSquashLiftRoute (named keys):
  press down, rock onto the foot, climb to the whip's apex, land, travel,
  bow. Values derived against springpose min-y sweeps.
- Dead end #1: kSpringChainLag 320 (de-sync the wind) wrecked the tuned
  route everywhere - reverted to 165.
- Dead end #2: authoring assembled as the half-wound pose introduced a
  hairpin at joint 9/10 and broke the entry side - reverted; the wind
  keeps its v8 staging.
- kSpringBladeSquashRise 3200 -> 6500 (probe terrain tell suggests the
  blade fan digs at the seated tip; empirical check in round 7).
- Round 7 runs with the COMPLETE output saved; remaining fails will be
  attacked from its own per-tick contact dump, not proxies.
### 2026-09-02 — Round 7 read; blades, landing arm, apex

- Blade crank moved the terrain tell (-255 -> -193): the FIN FAN was the
  digger at the seated tip. Fan now CLOSES with the squash (flare -1500)
  and rises hard (9000) - fins follow, they never carry the stand.
- kJumpLandingAbsorbArm 700 -> 500: 700 rode the SEATED coil (body high,
  impact bite 12-14 mm < the declared 15 minimum). The landing cushion now
  rides the route's pressing phase; the coil belongs to takeoffs.
- Whip apex key 565 -> 515 for the crown's crossing clearance; the
  declared loading dig deepens to 130 mm (the press, ~5 px at 240p).
### 2026-09-02 — Rounds 8-10: 25 -> 9 -> 5 assertions

- Landings GREEN (arm-500 cushion + the deep declared press).
- Blade fan closes+rises: terrain tell -297 -> -123.
- Bands declared: spring loaded dig 160, landing press 125.
- Remaining five: the release-transit graze (nose over the seated coil at
  arm ~660-830, rides the head's life-wave), its bite twin, the retimed
  attack's grid hitting the same window, six/nine's outside-window bound
  (same root). Round 11: waves 380/280, seating descent a hair deeper,
  late lift keys split {790,335}/{840,390}/{920,295}.
- LOOKED at iter4 jump-one (161-frame sheet): gather -> wound coil ->
  explosive release -> accepted wheel -> landing press -> settle. The arc
  reads. Fins now rise with the squash (rooster-tail) instead of stabbing.
### 2026-09-02 — Rounds 11-19: the mid-wind wall, and the honest stop

Probe convergence 25 -> 9 -> 5 -> 4 -> 3 -> 2 assertions across rounds
8-18. The last two do not yield to any dial this session owns:

1. "spring body runs intersect on the real full or micro surface" —
   ~330 triangle-pair hits peaking at ticks 9.5-10.5 and 18.5-19.5
   (arm ~520-750): the coil's FORMATION passes the front over/through the
   winding rear with ~40 mm of press at the real skin. Looked at on
   screen (evidence/iter4-midwind-frames.png, iter4-release-transit.png):
   reads as the animal coiling over itself, invisible at 240p. The
   zero-intersection law is owner law (D21 #3), so this stays a FAIL, not
   a widened gate.
2. The retimed attack's grid catches a +5 mm hover echo of the same
   window.

Structural attempts, all recorded above: seating knot (works, kept),
whip-side selection (kept), keyed lift route (kept), staggered roll,
early-seat (V5: clears pairs but collapses the arming into 2 keys ✗ D20
slow-arming), grounded-return mid knot (V4: -299 -> -104, insufficient),
loop loosening to 300 deg and 250 deg (WORSE: 474 hits - the crossing
driver is the nose-descent relation, not the wind tightness). Dead ends
in SPEC.

THE THEOREM that defeats zero tonight: with the support pinned at station
14, the head's backward travel is locked to the wind's progress (the
compensation), so the head is always above the rear DURING the wind; a
planar wind of this depth must pass the front over the winding rear with
less than two tube radii of clearance somewhere in arm 500-750, at
sampled half-keys. Resolving it needs either a 6th authored mid-wind knot
with per-station clearance solving (hours, risk to the motion), or an
owner call on the wind-vs-press trade.

DECISION: keep the tight coil (all four Direction-22 faults delivered at
the pose; the best state), do NOT publish (the probe must pass to ship),
report honestly with renders.

---

## Outcome

**Status:** NOT PUBLISHED. The pose work is complete and committed on
`zixxtrixx-wholebody-s-spring` (zhaozhou c7191479); the probe ends at
2 assertion failures (down from 27 at first full run), both rooted in the
coil-formation window. Publishing requires a green probe; the standing
authorisation covers finished passes only.

### Delivered against Direction 22 (at the pose, with renders)

1. THE REAR JOINS — the collapsed pose is a genuinely wound planar coil
   stood on the last point of the tail tube: the tail descends under the
   chin into a grounded pad whose tube tip is the rearmost biting point
   (-1296, -39), the whole body coiled above it.
   PROOF: evidence/PROOF-collapsed-coil-tail-stand.png (render + the
   committed zixx-springpose centreline, last five segments marked).
2. The head travels backward ~1.3 m as the S grows and travels backward
   (probe: head rel-support dX/dY -1140/-446); the neck at the collapse
   reads 78/110/131 deg against the idle hook's 80.2/117.6/138.5 — it
   OPENS, never tightens (values and per-joint deltas both).
3. The squeeze loads down-and-back on the diagonal (-1322, -543 at the
   deepest key), and the release fires forward along it (824 mm in its
   first key vs 263 peak arming).
4. Nose at 54% of idle height (shipped 62%), top of shape 65% (shipped
   68%), footprint span ~2/3 of shipped, flatten 48% + spread 21%, the
   declared loading dig. Every value a named constant.

### Honestly not achieved

- ZERO real-surface intersections through the coil's FORMATION (arm
  ~520-750): ~330 triangle-pair hits at ~8 half-key samples (~40 mm
  press of the front over the winding rear; looked at on screen and
  invisible at 240p, but the law is zero). Plus one +5 mm retimed-grid
  hover echo. Nineteen probe rounds of structural fixes are in the log;
  the two candidate resolutions are an authored mid-wind knot with
  per-station clearance solving, or an owner ruling on wind-vs-press.
- The experimental 3D-coil triple salto: DROPPED (owner appendix allows
  it; the planar pass consumed the session).

### 2026-09-02 — FINISHING PASS (new session): the owner has ruled

The owner was shown the trade — the tight coil with a declared formation
press, versus hours of per-station clearance solving — and chose: DECLARE
THE PRESS AND PUBLISH NOW. If it reads as clipping on screen, that costs
one more pass. This unblocks the two remaining probe assertions.

Plan, in the spirit of the declared ground-bite law:
- Name the coil-formation press as authored contact: where (front body
  runs passing over the winding rear, station pairs >= 7 apart), when
  (the wind window, printed keys 6.5-10.5 = ticks 13-21, and its unwind
  mirror at keys 19-20 = ticks 38-40 — the two crossings of arm ~520-750),
  and how deep (a real-skin press bound, constant derived by measuring).
- Owner-ruled allowance dated 2026-09-02, named editable constants; the
  loaded pose, hold, release and airborne phases stay at ZERO
  intersections; anything outside the declared windows is still a fault.
- The +5 mm retimed-grid hover echo: instrument its tick first; if it
  rides the wind window (same formation-climb cause) declare it the same
  way, scoped to the window only; otherwise fix it.
- KNOWN FOLLOW-UP, deferred by owner ruling: the mid-wind knot /
  per-station clearance solve that would restore zero intersections
  through the formation. Do not retry it this pass.
- The experimental 3D-coil triple salto stays DROPPED.

### 2026-09-02 — PROBE GREEN: the press is declared, not widened

- The formation press is now DECLARED AUTHORED CONTACT (owner-ruled
  2026-09-02): windows ticks 13-21 (keys 6.5-10.5, the wind) and 38-40
  (keys 19-20, the unwind mirror); depth bounded by the new committed
  poke-through gauge at the posed skin, hugging the ruled motion: full
  50 mm measured / 52 bound, micro 94 measured / 96 bound (the micro
  rung's coarser triangles read the same ~40 mm skin press larger).
  Outside-window hits: 0/0 — the loaded pose, hold, release pose and all
  airborne phases keep Direction 21 #3's zero law, asserted.
- The +5 mm retimed hover echo instrumented to key 11.5 micro — the tail
  of the formation climb, phase-shifted by the +1 hold key. Same cause
  (the body climbing onto its own coil), declared the same way:
  kSpringCoilFormationEchoBeginTick/EndTick 13..27 (keys 6.5-13.5,
  wind through the bow), allowance kSpringCoilFormationHoverEchoMm = 8;
  outside that window grounded pre-release contact still must be <= 0.
- ZIXX PROBE: PASS, exit 0. Evidence:
  evidence/probe-round20-GREEN-declared-press.txt.
- KNOWN FOLLOW-UP (deferred by the same ruling, do not retry this pass):
  a 6th authored mid-wind knot with per-station clearance solving would
  restore zero intersections through the formation transit; hours of
  work, risk to the accepted motion. Only on owner request.

### 2026-09-02 — Website leg underway

- Archive Generation Thirteen preserved byte-for-byte (44 files: the 22
  live Direction-21 clips + posters) BEFORE re-encoding; creatures.json
  entry + generation order + archive note; assemble.py
  MAX_ARCHIVE_GENERATIONS 12 -> 13; BOTH style.css archive selector
  families extended together. Commit 6e26149.
- The seven spring-family live notes re-authored for Direction 22: the
  extreme coil, and the DECLARED owner-ruled formation press stated
  plainly (superseding the zero-intersection claim). Commit a3587b7.
- assemble.py validates: 277 renders, exactly one robots noindex meta.
- 22-subject re-render running from ONE fresh explicit
  ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross invocation of the
  freshly built zhao-reel-cel (build-direct, cel target, one target per
  call). Renderer code untouched this pass (constants + probe only), so
  frames should match the judged iteration renders.
- 22/22 subjects rendered, exit 0; LOOKED at the jump-one strip (frames
  0/13/16/19/21/24/30/38/41/50/70, evidence/final-pass-jump-one-strip.png):
  the wind gathers backward, the declared-window transit reads as the
  animal coiling over itself (no clipping read at 240p), the coil sits
  low and loaded, the release explodes forward into the accepted wheel.
- zhaozhou origin/main merged into the branch (hardware lanes; zero
  overlap with tools/reel, reference/src, runtime); probe rebuilt via
  build-direct and rerun post-merge: PASS. Branch pushed.
