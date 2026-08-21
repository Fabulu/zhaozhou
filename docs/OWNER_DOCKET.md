# Owner docket — Zhaozhou

Feature asks from Fabian, newest first. This file exists because the asks were
scattered across run `QUEUE-*.md` files and an agent's memory, which meant the
only complete list lived outside the repo. Everything here is an **ask**, not a
decision: an entry records what was asked, what already exists that it can lean
on, and the questions that must be answered before it becomes a spec change.

Nothing in this file is a schedule. The standing priority order is set in
`STATUS.md`; the standing content order is **terrain effects and 3D character
LOD/deformation first**, and entries below do not outrank that.

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
