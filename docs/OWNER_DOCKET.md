# Owner docket — Zhaozhou

## 2026-08-22 — CDC seam DONE; two follow-on calls before the A/B remeasure

The ruled move is implemented: **DEBUG.CRC now runs in `vid_clk`** and nothing
per-pixel crosses the clock boundary any more. The block takes one displayed
RGB565 pixel per video clock and folds its two bytes in one `zhao_crc32c_fold`
tree; only the finalized 32-bit CRC crosses to `gpu_clk`, once per frame, on a
toggle with the value held stable beside it — the same pattern
`zhao_video_framectl` already uses and that
`tests/formal/video_framectl_one_fence.sby` already proves. The document
contradiction is closed the way you ruled: `design/blocks.yml` now says
`clock_domain: video`, matching `DEBUG.CRC.md`, which had been right all along.

Proven in **simulation only** — no fit was run for this change, because the
composed fit is yours to run. What was measured: 57 directed checks and 2,100
random checks on a cross-granularity differential (the device driven with
PIXELS, the shipped `zref::Crc32c` driven with the same stream as BYTES), the
shell lane green (`shell_golden_replay`, `shell_duo_markers_fast`), and a
22-mutant sweep: 22 attempted, 22 accounted, **20 caught**, 2 survivors, both
of them mutants proven EQUIVALENT in `tools/sweep_debug_crc.sh` (no input can
reach either).

**Two things I did not decide, because deciding either would change what your
A/B measures.**

### 1. Should `gpu_clk` and `vid_clk` now be cut in the SDC?

`fpga/quartus/shell_fit/zhao_shell_fit.sdc` says, deliberately:

> GPU and video are deliberately NOT cut from one another: the known
> phase-dependent displayed-byte crossing must remain visible in TimeQuest.

That crossing no longer exists. Every remaining `vid_clk <-> gpu_clk` path
except one (see 2) is a toggle handoff with its data held stable for a whole
frame, which is exactly the shape a clock-group cut is FOR. Cutting them would
almost certainly take the remaining vid/gpu hold analysis to zero.

**That is the reason not to do it silently.** A cut makes numbers better by
telling the tool to stop looking, and your A/B is specifically about hold. I
left the SDC untouched so the run you make measures the RTL change alone. If
you want the cut afterwards it is four lines, and the honest form is an
asynchronous clock group plus `set_max_skew` on the toggle handoffs, not a
blanket `set_false_path`.

### 2. `starvation_o` is the last unstructured vid -> gpu crossing

`zhao_shell_top` samples the video domain's 64-bit `scanout_starvation_cycles`
counter straight into a GPU register for counter id 30. It is guarded by a
PROTOCOL argument, not by structure: the counter only moves during active
lines, the frame tick lands in vblank, and `shell_err_cdc_o` trips if the value
ever moves across the sample window. The argument is sound, and it is still 64
bits of asynchronous data with 64 timed paths behind it — the only remaining
`vid_clk -> gpu_clk` family that is not a toggle handoff.

If the two hold violations you saw were on THIS family rather than on the CRC,
the remeasure will still show them. **Your call:**

* **(a)** run the A/B now, on the ruled change alone, and see what is left; or
* **(b)** let me convert the starvation sample to the same toggle handoff
  first — latch the counter in vid at the frame tick, cross a toggle, drop the
  tripwire to a redundant check — and run the A/B once against both fixes.

I recommend **(a)**: it is one change per measurement, and if the holds are
gone the second fix becomes optional rather than urgent. But (b) is the smaller
number of 40-minute fits if the holds turn out to be there.

---

## 2026-08-22 — CLOCK TARGETS: 120 MHz fabric + 150 MHz DDR (owner ask)

**Fabian, first:** *"Stretch target for 120 MHz. Historical reasons — GeForce 256
had that and it went with Sacrifice."*

**Fabian, clarifying:** *"That'd be 120 MHz GPU fabric, 150 MHz DDR interface.
That's the GeForce 256."*

So the target is the **GeForce 256 DDR** part, spelled out properly:

| lane | target | period | today |
| --- | ---: | ---: | --- |
| GPU fabric (`gpu_clk`) | **120 MHz** | 8.333 ns | 10.475 ns measured = **95.5 MHz** |
| memory interface | **150 MHz DDR** (300 MT/s) | 6.667 ns | **not characterized at all** |

**On the fabric number.** 100 MHz is not closed yet: worst setup at `6d23c84` is
10.475 ns, so the machine is good for about 95.5 MHz. 120 MHz is not 4.75% more,
it is **20.4% more**, and the previous campaign already took the worst path from
65 ns to 10.475 ns and ended placement-bound rather than logic-bound. The seven
fixes that bought those 55 ns were all accidental combinational depth — real
defects, now gone. Another 20% on a placement-bound design is pipelining,
floorplanning or a faster device: architecture, not cleanup.

**On the memory number, which is the more interesting half.** 150 MHz DDR is not
an RTL target at all — it is the SDRAM controller, the I/O timing and the board.
None of it is characterized today: the composed fit has **zero package pins**,
all harness I/O is virtual, and `MEM.SDRAM` is SPECIFIED / blocked_on: hardware
with a depth-900 refresh proof that has never finished. So this number cannot be
costed, or even honestly estimated, until the board is frozen. Recorded as the
target it is.

**Sequence, unchanged by this.** 100 MHz first, and the CDC seam before that
(the ruled next move) — moving that logic changes placement, so any 120 MHz
measurement taken before it would be measuring the wrong design.

**One thing worth flagging about the medium.** The GeForce 256 ran 120 MHz in
1999 on 220 nm dedicated silicon. This is an FPGA, where the fabric is roughly
an order of magnitude slower per gate — and the stated destination is
**fabricated silicon**, where 120 MHz stops being ambitious at all. The honest
reading of the ask is "match a GeForce 256 on the real device": a hard target on
the FPGA lane, and a low bar on the silicon lane. Worth deciding which lane the
number is meant to bind, because it changes whether it is a stretch goal or a
floor.

---

## RULED 2026-08-22 — "visibility sectors" is deleted. MESHFETCH culls a
## bounding sphere against each camera frustum.

I asked what a "camera visibility sector" was, because the phrase appeared
exactly twice in the repository and both were the block's own purpose line.
Fabian's ruling, recorded as given:

> "Somebody had a vague idea of spatial cells/portal sectors and wrote it into
> the ledger before any such system existed. Since the phrase has no
> corresponding data structure, algorithm, or format anywhere else, delete the
> word 'sectors' rather than invent a subsystem to justify it."

**THE LAW, as ruled:**

* `GEOM.MESHFETCH` performs **conservative per-camera frustum rejection of an
  instance/meshlet bound, before vertex decode**.
* The bound is a **bounding sphere**: `bound_center` + `bound_radius` in the
  descriptor. Not an AABB — a sphere is a few subtracts, multiplies and
  compares with no corner-walking, and a loose bound only costs performance.
* Per active camera: transform the bound into camera space, test the sphere
  against the frustum planes.
* **Reject only when the sphere is outside EVERY active camera.** In Duo, cull
  only if outside both.
* Optionally carry a **two-bit visibility result** (camera 0, camera 1)
  downstream, so work that genuinely is camera-specific is not duplicated.
* Static/rigid meshes take an **asset-generated** bound. Animated creatures take
  a **conservative animation-safe instance bound** — per-pose exact bounds are
  explicitly NOT required.
* `GEOM.CLIP` remains the exact per-triangle screen rejection stage. The two are
  complementary, not alternatives.
* **"Visibility sectors" is deleted. No sector system exists.**

**Explicitly forbidden for now:** meshlet occlusion sectors, BSP cells, portals,
island visibility grids, Hi-Z occlusion. Each needs new scene-format laws,
dynamic-update behaviour, memory structures and probably toolchain
participation — and for a world of floating, deforming, rotating terrain a rigid
baked visibility system could become actively annoying. Another rejection bit
can be added in front of MESHFETCH later without changing this law.

**Why here and not in GEOM.CLIP** (the option I had offered and Fabian rejected,
correctly): MESHFETCH feeds `GEOM.VDECODE` and `GEOM.POSE`, so rejecting an
invisible object here avoids compressed vertex fetch and decode, pose work,
skinning, projection, setup, binning and rasterisation. `GEOM.CLIP` receives
already-projected individual triangles — its cheap scissor test comes far too
late to save any of that.

**A correction to my own framing.** I had written that the existing projection
code "defines it completely". It does not: projection defines the camera and the
frustum, but the coarse BOUND REPRESENTATION was still an open choice, and the
bounding-sphere ruling above is what closes it.

---

## RULED 2026-08-22 — ONE ENGINE, FIVE PROFILES.

**Fabian's ruling: option 2 below.** One sequencer block; the five profiles
become configuration, and their ops are attributed to the blocks that consume
the output. `FIELD.SEQ.CORE` is already RTL_VERIFIED and is a complete engine,
so this is a ledger and contract change rather than new RTL.

What follows is the question as it stood, kept because it records WHY the
profiles were never distinguishable in hardware.

## (ruled) Are the five FIELD.SEQ profiles five blocks, or one used five ways?

Nothing in the RTL distinguishes them. `zhao_field_seq` has no profile input
and no profile-specific port. The thing that would distinguish them — which
registers the input and output lanes bind to — is carried by the DECODED
PROGRAM (`zfield::Decoded::in_lanes` / `out_lanes`, filled by the decoder from
the image), not by the block.

So a "profile" appears to be a program set plus shell wiring, not a hardware
variant. The ledger models five blocks (`EARTH`, `WARP`, `FLOW`, `FORMATION`,
`STAMP`), each wanting its own directed and random test.

I cannot write those tests without first deciding what each profile's I/O
contract is: `ops.yml` defines the profiles by name and description only, and
every one of the five contracts still has a generated TODO under "## Input and
output packet layouts". For FLOW that decision is particle behaviour, which is
reserved to you anyway.

**Two ways forward, and it is your call which:**

1. Keep five blocks and specify each profile's lane binding — then the tests
   are ordinary work.
2. Collapse them: one sequencer block, with the profiles becoming shell
   configuration and their ops attributed to the blocks that consume the
   output (`TERRAIN.PATCH`, `SURFACE.SHEET`, and so on).

Everything else about the sequencer is done: `FIELD.SEQ.CORE` is RTL_VERIFIED,
all 31 opcodes dispatch with a coverage gate, every Field IR piece carries a
mutation score, and the anti-hang law is formally proven.

## Three earth-field write ops need their law pinned

`FIELD.WRITE.MATERIAL`, `FIELD.WRITE.NAV` and `FIELD.WRITE.HAZARD` have no
tests, no reference functions, and no RTL — and unlike every other gap I closed
this session, these cannot be filled by working harder, because the law is not
written down anywhere.

Charter §11.2 names the layers and stops:

* layer 4 — "Base material map — two candidate material IDs plus a weight"
* layer 6 — "Gameplay state — heat, wetness, corruption, hazard and movement
  cost at a lower resolution"

`ops.yml`'s entries for NAV and HAZARD are one-liners pointing back at §11.2.
What a differential needs and nobody has stated: the encoding and width of each
layer, the resolution ("lower" — how much lower?), the blend or resolve rule
when two writes land on one cell, and the saturation behaviour.

MATERIAL is closest to reachable: layer 4 plus its ops.yml line ("2 candidate
material IDs + blend weight per cell; resolved deterministically") may be
enough to pin it. NAV and HAZARD are not.

These three will block `FIELD.SEQ.EARTH` — and therefore `TERRAIN.PATCH`
behind it — the moment either advances.

## RULED 2026-08-22 — BALANCED stays authoritative. Fix the CDC seam FIRST,
## then remeasure both fitter efforts.

**Fabian's ruling on fitter effort: defer, in a specific order.**

> "Right now the comparison is contaminated by a known structural clock-domain
> problem. A hold violation is qualitatively different from a setup miss. At
> -0.475 ns the balanced design is saying 'I can presently do about 95.5 MHz
> instead of 100 MHz'. A hold failure says 'this transfer is not physically safe
> even if you run the GPU at 20 MHz'."

The sequence, as ruled:

1. **BALANCED remains the authoritative fitter configuration.** HIGH PERFORMANCE
   is an experiment, not the shipping basis.
2. **Fix the video/GPU seam structurally** — specifically, move the displayed
   CRC into `vid_clk` rather than crossing per-pixel state.
3. Then run the SAME RTL twice, BALANCED and HIGH PERFORMANCE, and compare worst
   setup slack, TNS, failing endpoint count, worst hold slack and hold count,
   ALMs, and compile time.
4. Adopt HIGH PERFORMANCE only if it then has zero hold violations AND
   materially better setup. Otherwise stay BALANCED.

**And a direct instruction I am following:** do NOT spend time chasing the
remaining -0.475 ns of setup paths before the CDC decision. Moving that logic
changes placement enough that today's 56 endpoints may not be tomorrow's 56.

Measured at commit `6d23c84` (BALANCED): worst setup **-0.475 ns**, **56**
failing endpoints, **0** hold violations, **7,415** ALMs. The HIGH PERFORMANCE
experiment measured 17 failing setup endpoints and 2 hold violations, both on
the `vid_clk -> gpu_clk` seam.

---

## RULED 2026-08-22 — the creature-LOD boundary overflow: fix the law, never
## bake the wrap into silicon.

Found while building `zhao_geom_lod` against the shipped oracle: the random lane
caught the RTL and the reference disagreeing at `R = 59353, thresh = 40818,
e = 1, proj = 339695`. The cause was in the REFERENCE. `boundary_q8` computed
`thresh * bound_radius` in `__int128` and then narrowed the quotient to
`int32_t`; at 2,422,670,754 that wraps to **-1,872,296,542**, and a negative
boundary makes the eager-coarsen test false for every projected radius. **The
ladder refuses to coarsen and the creature stays pinned at a fine rung forever.**
Reachable with a small `micro_error` and a large threshold — both ordinary.

**Fabian's ruling, with an amendment I had missed:**

> "Fix the reference, but do not merely change `boundary_q8()` to `int64_t` and
> leave the following arithmetic unchanged. The quotient can approach 2^62, and
> multiplying that by 9 or 11 can overflow signed 64-bit."

That is correct, and it is why the fix is not a widening. **The boundary is now
never formed at all.** Both tests are cross-multiplied in `__int128` using the
same exact identity the RTL uses, so reference and hardware now evaluate ONE
mathematical predicate rather than the reference dividing and the RTL proving an
equivalent comparison by another route.

**Clamping to `INT32_MAX` was considered and rejected**, on Fabian's reasoning:
a clamp moves the 90% hysteresis threshold downward, so there are representable
radii for which `proj <= 0.9 * true_boundary` holds but `proj <= 0.9 * INT32_MAX`
does not. It fixes the catastrophic wrap while subtly changing the transition
law — and there is no reason to keep an int32 here at all.

**Regression cases added**, as directed: the exact reproducer; thresholds
astride the old 2^31 boundary; the smallest error term with the largest legal
radius and threshold; and the invariant that needs no oracle at all —

> for a fixed creature and threshold, decreasing the projected radius must NEVER
> produce a finer LOD decision.

That invariant is what the overflow actually broke, and it would have caught it
with no reference to compare against.

---

## (ruled) Timing closure: two decisions, both measured, both yours

### 1. Fitter effort -- 125 failing endpoints, or 17 with two hold violations?

The composed shell is 6.4% too fast-paced for its clock. Measured both ways:

|                    | BALANCED (current) | HIGH PERFORMANCE EFFORT |
| ------------------ | -----------------: | ----------------------: |
| worst path         |     10.64 ns       |            11.39 ns     |
| endpoints too slow |          125       |                 **17**  |
| hold violations    |        **0**       |                   **2** |
| logic cells        |        7,648       |                   8,147 |

Neither closes. High effort leaves seven times fewer things to fix, but two of
them are HOLD failures -- data arriving too EARLY, which no clock speed fixes
and which needs a real repair.

I reverted to BALANCED because every number this project has ever recorded was
taken on it, and switching makes them all incomparable. If you would rather
start closure from the 17-endpoint position, it is a one-line change.

### 2. The GPU/video crossing

Three hold violations appeared and vanished across this session's fits on the
seam where video-domain signals are sampled directly into GPU registers. They
come and go with placement, which means they are not fixed -- they are lucky.

A review you relayed proposed the real answer: the displayed-frame checksum
should live in the VIDEO clock domain, where the pixels already are, instead of
crossing every pixel into the GPU domain to be counted. `DEBUG.CRC.md` already
says the displayed lane is video-domain; `design/blocks.yml` says GPU; the
implementation followed blocks.yml and built the crossing. **The documents
disagree and the code picked one.**

That is an architecture decision, not an SDC constraint. A false path would
only stop the tool reporting a real crossing, and `set_max_delay` addresses
setup rather than the hold failures actually seen.

**RESOLVED 2026-08-22** — see the entry at the top of this file. The CRC moved
into `vid_clk`, `design/blocks.yml` was corrected to `clock_domain: video`, and
no per-pixel state crosses the boundary any more. What is still open is only
whether to cut the two clocks in the SDC, and the 64-bit `starvation_o`
sample.

Feature asks from Fabian, newest first. This file exists because the asks were
scattered across run `QUEUE-*.md` files and an agent's memory, which meant the
only complete list lived outside the repo. Everything here is an **ask**, not a
decision: an entry records what was asked, what already exists that it can lean
on, and the questions that must be answered before it becomes a spec change.

Nothing in this file is a schedule. The standing priority order is set in
`STATUS.md`; the standing content order is **terrain effects and 3D character
LOD/deformation first**, and entries below do not outrank that.

---

## 2026-08-21 — the Reality Tear (LUXURY, gated on proven hardware + measured slack)

> "If we have extra dsp and alm at the end, can we add some fancy silly thing?
> Or is the architecture too rigid"

**Not too rigid — and the effect is already specified in pieces.** Verified
against `design/blocks.yml` 2026-08-21, this is not a new feature needing a new
socket. Every part of it is already named:

| Block | What the ledger ALREADY says |
| --- | --- |
| `FORGE.PRIM` | "Ribbons, tubes, radial shells, rings, chains, shard bursts, billboard sheets, spline walls, cones" |
| `POST.GATHER` | "Accumulate low-resolution glow, **distortion-XY** and outline buffers" |
| `POST.COMPOSITE` | "Bloom, haze, **shockwave**, **refraction**, grading, palette and flash" |
| `TEXTURE.AUX` | "Restricted aux texel source (surface sheets, light/shadow compare, **distortion**)" |
| `POST.ECHO` | "Optional echo of the composited frame back to a capture buffer; **first on the §26 cut list**" — `deferred: true`, `cut_order: 1` |

Distortion is already a sanctioned aux use. Shockwave and refraction are
already composite modes. The distortion vector field is already a gather
output. The frame echo already exists as the designated first luxury to cut.
**The parts spell the effect; nobody had noticed.**

### The effect

Procedural spell geometry from the Forge writes glow/distortion strength into
the ordinary effect-tag path; the post gate turns that into a low-resolution
vector field that bends the finished image; `SURFACE.STAMP` leaves a persistent
scar underneath. Wormholes that twist the background, spells that fold the
screen inward, heat-haze creatures whose silhouettes distort the world behind
them, portals shedding fragments of previous frames, an unreality storm where
terrain scars, geometry and screen feedback all agree on one event.

> "Use the spare silicon to let enormous procedural spells physically generate
> geometry, scar the terrain and bend the completed image around themselves."

**Spend surplus geometry throughput on impossible spell geometry, not on every
unit gaining another 200 triangles.** One outrageous battlefield-scale twisting
object does more for the machine's identity than a uniform detail bump.

### What "spare" has to mean

Owner's own constraint, and it is the important half of this entry. A final fit
saying "20 DSPs free" does **not** mean 20 spendable DSPs. A luxury feature also
needs SDRAM bandwidth, M10K, routing near the right pipeline, timing margin,
frame-cycle slack and command capacity. A full-frame feedback effect might cost
almost no DSP and be impossible because it adds a framebuffer read; a
procedural geometry engine might cost eight DSPs and no bandwidth yet swamp
triangle setup.

So **spare means**: placed and routed, timing closed **on the actual board**,
worst-case workload at 60 Hz, and measured resource AND bandwidth reserve left
over.

Budget shape, if the machine lands near the estimated 92-95 DSP: keep **10-12
DSPs untouched** as engineering reserve, spend **5-8** on the luxury, and only
go to 10-16 if it lands at 80-85 with comfortable timing. Charter §25's 10%
reserve is a floor, not a target to consume.

### Easy, hard, and forbidden

**Very easy — after the renderer.** Post effects change no triangle packet, no
coverage, no depth or texture semantics, no tile layout, and not the bit-exact
geometry reference. Miss the budget and you drop resolution or taps or switch
it off; the base frame stays correct.

**Easy — before geometry setup.** A generator that emits ordinary triangles
feeds the same setup and raster path as everything else. `FORGE.PRIM` is
exactly this category.

**Medium — a new fixed material recipe**, if it uses texel, vertex colour,
depth and effect-tag data already present. Dangerous the moment it wants
another texture lookup or touches the fragment critical path.

**Hard, and mostly forbidden — the renderer's centre.** No second unrestricted
TMU, no general fragment shaders, no wider tile word, no extra arbitrary
interpolants, no second geometry pass, no shadow maps, no unrestricted
render-to-texture, no recursive portals. The ledger already defends this:
`TEXTURE.AUX` is "deliberately NOT a general second TMU (§26)".

**A fake portal from Forge geometry plus post distortion is easy. A genuinely
recursively rendered portal is nearly a new renderer.**

### What to reserve NOW, while the contracts are stubs

This is the actionable half and it costs no fabric. `FORGE.PRIM`,
`POST.GATHER` and `POST.COMPOSITE` are all still `SPECIFIED`, so the interfaces
can be shaped before anything is built:

- one optional post-effect dispatch;
- one bounded post recipe id (shockwave / heat haze / portal lens / radial
  streaks / feedback echo / chromatic smear — **one recipe per pixel, so one
  bounded DSP bank walks the selected one rather than six multiplier farms**);
- a low-resolution signed XY distortion buffer;
- a configurable quality level / tap count;
- a scheduler token budget;
- counters for processed texels, taps, dropped effects, deadline degradation;
- a deterministic fallback ladder: 4-tap filtered feedback -> 2-tap filtered
  warp -> nearest warp -> glow only -> off.

That ladder is what makes the luxury obey the machine's central rule:
**the effect negotiates; 60 Hz does not.**

### Status

**NOT SCHEDULED.** Gated on proven hardware and measured slack, per the owner.
Recorded now because the reservation is free today and expensive later.

---


## 2026-08-21 — latency is now a formal goal (RULING, not an ask)

> "Less latency is an improvement. It should be a goal, really, as long as it
> doesn't fuck up anything else. In fact I consider this a massive boon, a
> gargantuan success, and an architectural oversight that this hadn't been made
> a formal goal."

**Acted on, not queued.** `design/budgets/latency.md` is the budget document and
charter §25 now carries the rule. Latency joins ALM, DSP, M10K and bandwidth as
a budgeted resource: a change that moves it must say so and by how much, a
reduction is a win to be KEPT rather than a failing test to revert, and an
increase needs a reason better than "it was easier".

**Honest scope.** The blit path is measured — ~58k gpu cycles saved, one whole
frame earlier, roughly 16.7 ms at the 60 Hz field rate. **End-to-end
controller-sample to displayed-photon is NOT measured**, and nobody should quote
a number for it yet. Three gaps are named in the budget document rather than
guessed at, including whether `CMD.SCHEDULER`'s tick alignment costs a frame
that nobody has ever costed.

---


## 2026-08-21 — terrain rotated at arbitrary angles, and rotated in REAL TIME

> "our terrains can be rotated at different [angles]. Would be great if terrains
> could also be rotated in real time. So a skyscraper suddenly falls over."

Two asks, and they are not the same size.

### A. Arbitrary static rotation

Already resolved in principle, not yet in the spec. The resolution is **rotate
the ISLAND, not the patch**: a rotated *patch* breaks the one-solid-interval
column law that `spec/terrain_rules.md` is built on, the same wall that true
tunnels hit. An island is the right granularity because everything inside it
stays axis-aligned in island space.

`spec/terrain_rules.md` has no `orientation` field today — islands carry
translation only. **That single format change is the whole of this ask**, plus
an inverse transform on every world-space query that reaches terrain.

### B. Real-time rotation — the skyscraper that falls over

Much larger, and worth separating because the interesting failure modes are not
in the renderer.

**What it can lean on.** `GEOM.LOOM` already parents terrain patches under
transform nodes; that is the specced terrain-class-giant capability. A falling
skyscraper is a terrain-class giant whose transform is animated per tick. The
rendering path is therefore not the new part. Scars and deformation baked into
the sheet ride along for free, because they live in island space.

**The questions that decide whether this is cheap or expensive:**

1. **World-space column walking.** The one-interval law holds in island space.
   Once an island tilts, its columns are no longer vertical in world space, so
   anything that walks a world column — projectiles, particles settling,
   height-at-(x,z) queries — must inverse-transform first. Cheap per query,
   but it has to be *every* query, and a missed one is a desync rather than a
   visual glitch.
2. **The keel.** The deep textured keel is a downward curtain. When the tower
   is upright the keel points at the ground; when it has fallen the keel points
   sideways and is seen edge-on, which is exactly the "flimsy sheet" reading
   the keel was added to prevent. Does a tilting island grow a keel on the face
   that is now downward, or is the keel a full skirt from the start?
3. **The transition moment.** A structure that is part of the resident terrain
   set and then becomes dynamic has to move between two representations while
   the player is looking at it. Whether that hand-off can be made invisible is
   the real risk in this feature, and it is a determinism question before it is
   a visual one — both representations must agree exactly on the frame they
   swap.
4. **Rotation about what.** A skyscraper falling over is rotation about an edge
   at its base, not its centre. The pivot has to be authored, or derived from
   the contact edge; either way it is data the format does not carry yet.

**Not proposed here:** any answer to those four. They need Fabian's call on how
much of it is physics and how much is an authored performance.

**Placement.** This is a terrain effect, so it sits inside the standing top
priority — but behind the current hardware lane (`DEBUG.FRAMEBLIT` integration
and the composed Quartus fit), because it is a *format and sim* change and the
lane that would carry it is the same one those are holding.

---

## Standing asks, consolidated

Carried forward from `runs/CLAUDE-RUNS/RUN-20260816-0046-.../QUEUE-*.md` and
prior sessions, so the list is readable from the repo.

**Visual identity — non-negotiable**

- Noctis IV suns and lens flares; a whole gamut of suns, including from space.
- Moving suns must carry the Noctis **smear** (fade-not-clear ghost trails).
  Hard travelling discs are "not fully Noctis style".
- 360-degree skyboxes; god beams piercing cloud.
- Floating terrain islands, **more** deformable than Sacrifice: breaches,
  undercuts, rim bites.
- The rubbery liquid terrain feel: travelling waves, rebound dips, ground
  behaving like a membrane.

**Queued**

- Effect library: every sun variant and every terrain effect catalogued with
  screens and reels, renderable by id.
- Clouds in front of sky and sun.
- Atmospheric rain that darkens the sky as cloud rolls in; spells may cause
  weather. Weather is sim state and must replay exactly.
- Cheap, impressive global and local light changes. Explicitly **no ray
  tracing** — "we cheat like the cheap fucks we are."
- Rotated terrain sheets for vertical structures: skyscrapers and mountains
  built from several smaller rotated sheets, deformable. Four walls and a top.
- Deep textured keel on island rims so terrain reads solid rather than as a
  flimsy sheet. **Sequenced before rotated sheets.**
- Thick-atmosphere sun: the free win is the star ramp's P3 control point, which
  currently whitens the top and is backwards for thick air. Two real gaps
  remain — the falloff in §4 is linear and wants a soft shoulder, and the sky
  has an elevation ramp with no azimuthal term centred on the sun.
- Game modes: campaign, skirmish, 2P versus, 2P co-op. Split screen is decided
  and shipped.
- Digging and tunnels: a column is one solid interval, so true tunnels need
  Wounds. Trenches and keel burrows are free today; round overhangs already
  work via high-bottom slab columns.

**Process**

- Reel subjects must be legible at gallery scale, not merely correct.
- Site copy gate is hard: no em dashes, no AI-isms, including reel provenance
  strings.
