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
