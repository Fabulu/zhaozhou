# REVIEW — Direction 23 slow, readable, whole-body S spring

**Reviewer pass, 2026-09-02.** Lane
`zixxtrixx-wholebody-s-spring-20260901/zhaozhou`, branch `main`, HEAD
`ecf0e3ab`, working tree clean. Built with `tools/reel/build-direct.sh`
(cel, then probe), rendered with `ZIXX_EXP=celmain
ZIXX_LIGHT=diagonal-cool-cross`. `sacengine` never run. Nothing authored
was changed by this pass.

## Headline

**The pass is good.** It does the thing Direction 23 asked for first and
loudest: the jitter is gone, and it is gone by a wide margin — measured with my
own instrument, on my own renders, before I read the implementer's numbers, the
arming moves *more* smoothly than the tail-balance clip the owner named as the
standard. The four beats separate cleanly, the pace is deliberate without
dragging, the head's travel is small and never goes near the tail, the shape
stays planar, nothing clips, and — proved, not asserted — fifteen of the
twenty-two bank subjects still render byte-identical CRCs, so idle, walk, look,
taunt, the fall, the hits and the deaths are untouched, and the accepted salto
survives the +62-key retime intact.

There is one real fault, and it is a fault against the *direction*, not against
smoothness: **the descent is not a whole-body descent, and beat 1 is not a
whole-body beat.** Both halves of the animal do participate across the pass, but
they take turns — beat 1 is 83 % a tail action, beat 2's descent is confined to
head, neck and front, and the middle of the body moves 5 mm in beat 1 and 7 mm
in beat 2. Acceptance points 3 and 5 say otherwise in plain words.

## How I judged it

I rendered `zixxtrixx-spring-side` (fixed true-side camera, 183 frames),
`zixxtrixx-jump-one` (285 frames), `zixxtrixx-spring-top` and
`zixxtrixx-balance` myself, built every-frame contact sheets, and looked at
them before touching a number. I then wrote an independent silhouette
instrument (colour classifier, no median plate, mask verified by eye in
`evidence/review/segmentation-mask-check.png`) and a raw-pixel change profile
that needs no segmentation at all. Only after that did I check the
implementer's claims. I also extracted and built Generation Thirteen from
`a2f601ef` into a scratch tree so the "flight unchanged / landing
indistinguishable" claims could be settled by byte comparison rather than
inherited.

Playback is **60 fps** (confirmed from the published
`Upheaval/website/public/renders/zixxtrixx-jump-one.webm`, `60/1`, and from the
probe's own "max 60 Hz station step" gates). The reel emits two samples per
key, so the 72-key arming plus 8-key hold is **2.67 s of ground time**, and
`jump-one` is 4.75 s against the published 161-frame / 2.68 s clip. Any pace
judgement that assumed 20 fps would be wrong by a factor of three; I mention it
because it changes the answer.

## Verdict per acceptance point

### 1. The movement is smooth — **PASS**

Verified, and by a comfortable margin. On the fixed side camera, ground phase:

| per frame | spring arming | balance clip, same instrument |
|---|---|---|
| centroid-x speed, median / max | 0.131 / 0.657 px | 0.358 / 3.146 px |
| centroid-x jerk, max | 1.060 px | 1.495 px |
| centroid-y speed, median | 0.057 px | 0.109 px |
| centroid-y jerk, max | 0.381 px | 0.427 px |
| silhouette change, median | 1.49 % | — |

Everything the owner would watch moves sub-pixel per frame and is *quieter*
than the clip he holds up as the organic reference. The 30 Hz chord-midpoint
staircase is dead: alternate-sample mean-step ratios are 1.05–1.37 (a staircase
would be a large ratio), and the head's velocity autocorrelation is **+0.544**
— persistent, i.e. it keeps going the way it was going. The balance clip's own
is +0.662, and its area autocorrelation (−0.338) is *worse* than the spring's
(−0.280).

The tracking camera is fixed too: across the whole 160-frame arming of
`jump-one` the horizon row moves six rows, never more than one row in any
frame. Stage 3's camera work holds.

The one blemish is a single-frame pop at **f15** — see Fault 3.

### 2. A viewer can see what the animal is doing at every beat — **PASS**, with a reservation

The raw-pixel change profile (no segmentation involved) separates the beats
without being asked to:

```
settle f0-12    2.3 px/frame     (f0-f10 byte-identical)
beat 1 f12-72  44.1 px/frame
dwell  f72-82  22.9 px/frame
beat 2 f82-144 40.8 px/frame
hold  f144-160 23.3 px/frame
launch f161+   1435-1716 px/frame
```

Two running beats at roughly twice the rate of two quiet beats, with a quiet
hold and an explosive launch. That is a legible rhythm, and the every-frame
sheets confirm it: the animal is readable in every frame, nothing blobs,
nothing snaps. The reservation is that what beat 1 *shows* is the tail
gathering, which is Fault 1.

### 3. The whole body becomes the S, slowly, with the balance clip's organic quality — **FAIL (partial)**

Slow: yes, 1.00 s for the beat. Organic: yes, by the numbers above. **Whole
body: no.** Two independent instruments say the same thing.

Screen-region share of beat 1's cumulative silhouette change:

| | tail + fins | mid body | head + front |
|---|---|---|---|
| beat 1 | **82.5 %** | 6.7 % | 10.8 % |
| beat 2 | 45.3 % | 23.9 % | 30.8 % |

And the committed pose probe's own world-millimetre station table, which is not
a projection of anything:

```
SPRING full-S entry mean station travel / compression descent:
  head=51/-310  neck=47/-281  front=27/-124  middle=5/-7
  grounded run=8/-37  taper=105/-33  tail=257/1 mm
```

In beat 1 the tail travels 257 mm and **the middle of the animal travels 5 mm**.
`evidence/review/beat-outline-overlay.png` shows it at a glance: between the
red (rest) and yellow (assembled) outlines the tail fins swing up into their V
and the rear gathers, while the head and neck outlines sit almost on top of one
another.

### 4. The head moves slightly backward and slowly down; it never approaches or passes the tail — **PASS on the tail, NEEDS AN OWNER EYE on "slightly"**

Judged on the fixed-camera `zixxtrixx-spring-side`, as instructed.

*Never approaches the tail:* decisively. Across every ground frame the nose tip
sits at x 235–244 and the rearmost body pixel at x 109–131 — a gap that never
falls below **104 px**, and it *grows* as the head comes back only 7 px. The
Direction-22 complaint is fully answered.

*Slightly backward, slowly down:* nose tip **−7.0 px back, +10.3 px down** over
2.67 s. My figure and the implementer's (9.8 / 11.7, eye-anchored) agree to
within the difference between the two anchors, so that claim is sound. But the
head *as a region* moves only **2.18 px back** for 6.75 px down, its x route
wanders **0.85 px forward** around f30–60 before turning, and its total x path
is 4.4× its net displacement. The backward component is right at the threshold
of reading as a direction rather than as drift. This is inside the letter of
"slightly" and it is the right side to err on after Direction 22 — but it is an
eye call and it belongs to the owner, not to me.

### 5. Everything descends together during the compression — **FAIL**

The descent is confined to the front of the animal. Region centroid y,
f72 → f160, fixed side camera:

| region | descent |
|---|---|
| head / front | **+7.12 px down** |
| mid body | +2.62 px |
| tail / fins | **−0.25 px** (rises 1.5 px to f100, falls 1.8 px after) |

The probe's world-millimetre compression-descent column says the same: head
−310, neck −281, front −124, **middle −7, grounded run −37, taper −33, tail
+1 mm**. The tail V that rose during beat 1 simply stays up while the head
sinks. Some of that is physics — the belly is already on the ground and cannot
descend — but the raised tail can, and the direction says "everything else goes
down with it".

### 6. The four beats read in order and none is hurried — **PASS**

At 60 fps: settle 0.20 s, become-S 1.00 s, dwell 0.17 s, compress 1.03 s, hold
0.27 s. Ground time 2.67 s against the published bank's ~0.6 s — a 4.4×
slowdown. My eye call, which the implementer said it could not make alone: **it
reads deliberate, not draggy.** A second of tail gathering and a second of head
sinking is about as long as either action can hold attention with the content
it currently has, so the pace is at its ceiling *given* Fault 1 — if beat 1
gets more whole-body content it could carry more time, and if it does not, do
not lengthen it further.

### 7. The launch follows the compression — **PASS**

From f161 the change rate goes from ~110 to 1435–1716 px/frame and the body
unwinds into the airborne wheel over the following twelve frames
(`evidence/review/springside-launch-f160-182-3x.png`). It reads as a release of
the thing that was just loaded, and the flight it releases into is byte-exact
Generation Thirteen.

## Standing laws

| law | verdict | evidence |
|---|---|---|
| whole body to the last point of the tail **tube**, not the fins | **PASS** | probe: tail-station group entry travel 257 mm, "SPRING real tail followers: 166 vertices, entry travel mean/max 219/487 mm" |
| planar, no helix | **PASS** | probe: body/whole lateral span **0 / 14 mm**; and by eye, the high three-quarter strip stays a flat ribbon for all 160 frames (`planarity-top-strip.png`) |
| nothing clips | **PASS** | probe PASS; "SPRING real surface intersections full/micro: none / none"; terrain −42 mm at key 72 against a declared 34 resting / 60 loaded bite |
| the accepted salto unchanged | **PASS, proved** | from Gen-13 frame 44 onward `salto-dummy` differs by **at most 22 changed pixels in any frame** (0.02 % of the frame); `jump-one` flight f46–108 byte-identical at +124 |
| idle untouched | **PASS, proved** | `zixxtrixx-idle` CRC `0x25EE061F`, identical to the pre-run bank |

## The two excused columns, and the two jolts

I was asked to probe these hard, and I did. Two of the three claims survive; one
is right in its conclusion and wrong in its cause.

**A0 head-x jerk 14.4 px — VERIFIED as instrument noise.** The real head cannot
be doing this. On the fixed camera the nose tip's largest single-frame step in
the entire ground phase is **1 px**, with three sign changes in 17 non-zero
steps, and the head-region centroid moves 0.06 px per frame on average. A 14 px
frame-to-frame head jump is not present in the pixels. The eye-blob anchor
sliding along a horizontal neck explains it exactly. Accepted.

**A0 area jerk 196 px² — conclusion right, cause WRONG, and it names a real
one-frame render pop.** It is not thin-fin segmentation flicker, and it is not
segmentation at all. It is a single event at **frame 15**, and it is in the raw
pixel bytes: 54 changed pixels at f14, **388 at f15**, 66 at f16, in the *fixed*
camera where nothing else can move. The spatial map
(`f15-shading-pop-map.png`) puts 316 of those 388 pixels *inside* the
silhouette, spread over the whole creature's interior shading — neck, belly,
flank and fin together — not on the outline and not on one fin. The frame pair
at 8× (`f15-shading-pop-f13-16.png`) shows the green body's highlight banding
redistributing in one frame while the pose barely changes. The implementer's
*conclusion* still holds — it is not a motion fault, the silhouette does not
jump, and 388 changed pixels is well under the balance clip's own median of 480
— but it should be recorded as a rendering pop with a known frame, not filed
under segmentation noise. See Fault 3.

**The f46/f48 prominence-4 jolts — VERIFIED at the instrument's floor.** On
`jump-one`, silhouette change at f44–f52 runs 2.05, 1.79, 2.12, 2.05, 1.97,
1.29, 1.82, 1.82, 1.33 %/frame against a 1.68 % median. There is no event
there. Accepted.

## The revert's collateral

**Checked, and the claim holds.** I built Generation Thirteen from `a2f601ef`
in a scratch tree and compared frame by frame at the +124-frame offset
(`evidence/review/gen13-flight-identity.txt`).

* `jump-one` flight frames **46–108 are byte-identical**, plus 44, 110, 144 and
  146–160. The flight is exactly the accepted one.
* The landing (gen13 f111–145) differs, peaking at 5051 changed pixels. That
  number sounds alarming and is not: the creature silhouette is ~4800 px, so a
  few pixels of bodily offset XORs to roughly twice its area. Measured, the
  landing poses differ by up to (+7.4, +2.0) px of placement and by the
  tail-fin life-wave phase; side by side
  (`evidence/review/landing-head-vs-gen13.png`) they are the same landing.
  "Visually indistinguishable" is a fair description; "the same landing with
  the fins in a different phase and a few pixels of drift" is the precise one.

## Regression sweep of the accepted vocabulary

I rendered the whole 22-subject bank at HEAD and compared sequence CRCs against
the pre-run bank recorded in `evidence/stage1/crc-proof-22-subjects.txt`
(`evidence/review/bank-crc-vs-prerun.txt`).

**Fifteen subjects are byte-identical**: idle, look, walk, run, taunt,
slow-taunt, damage, hit, hitfloor, knockdown, death, death2, fall,
moving-light, balance. **Seven changed**, and they are exactly the seven that
consume the shared spring: attack, jump-one, jump-multi, salto-dummy,
salto-fly, salto-six, salto-nine. The +62-key shift moved nothing it should not
have moved. `zixx-probe` is PASS end to end at HEAD, including every parity,
seam and declared-contact gate.

## Faults, ranked by how much they cost the owner's read

**1. Beat 1 is a tail action and beat 2's descent is a front action; the middle
of the animal does neither.** (Acceptance 3 FAIL, acceptance 5 FAIL.) 82.5 % of
beat 1's on-screen change is in the tail third; the middle station group travels
5 mm during it. During the compression the head drops 310 mm, the neck 281, the
front 124 — and the middle 7 mm, the grounded run 37, the taper 33, the tail
+1 mm. The owner asked for the *whole body* to become the S and for *everything*
to go down with the head. What is on screen is two consecutive half-body
actions. This is the one thing standing between this pass and the direction.
*Where to look:* the assembled-heading table for the middle and grounded-run
stations (beat 1 gives them nothing), and the tail's collapse authorship — the
assembled tail curl is currently carried verbatim into the collapsed pose, by
design (stage 6), which is exactly why the tail cannot descend in beat 2.

**2. The head's backward travel may now be too slight to read.** (Acceptance 4,
needs an owner eye.) 2.18 px of head-region backward travel with a 0.85 px
forward excursion en route, over 2.67 s. Erring small was correct; whether it
has been erred past the point of reading is the owner's call, not mine.

**3. A one-frame whole-body shading pop at frame 15.** 388 changed pixels
against 54 and 66 either side — a 6–7× local spike, landing in the quietest
part of the clip 0.25 s in, just after the settle. It is below the balance
clip's ordinary frame-to-frame change in absolute terms and it does not move
the silhouette, so it is a blemish rather than a jitter fault. It is also a
cheap thing to chase, since it is one named frame in a fixed camera.

**4. Eleven byte-identical frames at the head of the clip.** `jump-one` f0–f10
and `spring-side` f0–f10 are exactly equal — not merely still, *identical*, so
the life layer is off as well as the primary. At 60 fps that is 0.18 s, which
is why I rank it last rather than first: it is short enough to read as a held
breath. But a totally frozen creature is the opposite of the "held-alive"
quality the rest of the pass earns, and in a looping clip it sits directly
after the landing settle. Starting the life clock at key 0 would cost nothing.

**5. The hold reads as alive, not as strain.** (Eye call, as flagged.) Over the
16 hold frames the outlines are within 1–2 px of each other, at the fin tips
and the nose only (`evidence/review/hold-quiver-outlines.png`), at 55–126
changed px/frame against a 150 clip median. My eye says *held-alive* — which is
what the implementer claimed — but not *loaded*. At 0.27 s the hold is also the
shortest of the four beats by a factor of four. If the owner wants the
compression to feel like stored energy, this is where to spend it, and
lengthening the hold is a knob, not a rebuild.

## What I verified vs what I inferred

**Verified with my own renders and my own instrument:** every smoothness number
above; the beat structure and its rates; head travel and the head-to-tail gap;
the per-region descent; planarity by eye on the top view; the f15 event and its
spatial extent; the f46/f48 non-event; the 30 Hz staircase being dead; the
tracking camera's steadiness; the 15/22 CRC identity; the Gen-13 flight and
salto byte comparison; the landing pairs.

**Taken from the committed probe without re-deriving:** the world-millimetre
station-travel and compression-descent columns (though my pixel-region
measurements agree with them independently), the lateral-span and
self-intersection gates, the terrain bite figures, the shape/support drift over
the hold.

**Accepted from the implementer without independent check:** the knot
re-spacing measurement and the decision to decline it; the move of the probe's
representative sample keys from 1/4 to 20/50; the stage-1 byte-identity proof of
the literal-naming refactor (its 22-subject CRC table is consistent with my own
sweep); the claim that the arming half-keys are now all authored from the
schedule rather than chord-baked.

## Recommendation

Do not publish this pass as it stands. Fault 1 is a direct miss on two written
acceptance points and it is the kind of miss the owner will see in the first
viewing — he asked for the whole body twice in one direction. Everything else
in the pass is sound, the machinery is right, and the fix is an authoring pass
on two pose tables, not another rebuild: give the middle and grounded-run
stations real travel in beat 1, and let the tail come down with the head in
beat 2. Faults 3, 4 and 5 are cheap and can ride along. Fault 2 should go to
the owner as a question with the head-track plot attached.

The smoothness problem that generated Direction 23 is solved. That part is
finished work and should be said plainly.

## Evidence in `evidence/review/`

| file | what it shows |
|---|---|
| `springside-contact-f000-091.png`, `springside-contact-f092-182.png` | every frame of the fixed-side diagnostic |
| `springside-launch-f160-182-3x.png` | the release, 3× |
| `beat-outline-overlay.png` | rest / assembled / loaded outlines on one plate — the Fault-1 picture |
| `speed-and-track-profile.png` | change rate, head x route, head vs tail vs whole-body y route, beats shaded |
| `planarity-top-strip.png` | high three-quarter view every 8 frames |
| `hold-quiver-outlines.png` | four hold outlines overlaid |
| `landing-head-vs-gen13.png` | HEAD over Generation Thirteen, five landing frames |
| `f15-shading-pop-map.png`, `f15-shading-pop-f13-16.png` | the frame-15 pop, mapped and at 8× |
| `segmentation-mask-check.png` | my mask, looked at, at rest and loaded |
| `bank-crc-vs-prerun.txt` | the 22-subject CRC sweep |
| `gen13-flight-identity.txt` | the byte comparison against `a2f601ef` |
| `reviewer-measurements.txt` | every number in this document, with its method |
