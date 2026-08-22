# Owner docket — Zhaozhou

## Timing closure: two decisions, both measured, both yours

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
