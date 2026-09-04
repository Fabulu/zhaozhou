# RECON 2/2 — the NEW mana for Unnamed02 (effects lane)

**Date:** 2026-09-05 · **Against:** `Upheaval/creature/Unnamed02/OWNER-DIRECTION-2-2026-09-04.md`
**Lane:** `zixxtrixx-wholebody-s-spring-20260901`, zhaozhou at `780bf4f0`, Upheaval at `aa364ee`.
**Scope:** effects only. Form, rig and eyes belong to the other recon.
Nothing committed. Diagnostics under the run scratchpad.

Provenance: **[V]** read in the named source · **[R]** rendered and looked at by me
this pass · **[I]** inference. Every cost figure is **arithmetic**, never a frame
measurement — `spec/counters.md` still has no fragment, particle or polygon
counter (`09-ENGINE-GOTCHAS.md` §5). Say so wherever these numbers are published.

Unit throughout: **one full-screen pass = 92,160 pixel-visits** at 384x240
(`reports/PER_PIXEL_BUDGET.md:191`, the one measured number in the machine).

---

## 0. Bottom line, in the order the architect should decide

1. **The pulsar is a static ball, not a pulse.** [R] Rendered: its duty strobe
   moves **150 pixels of 92,160 (0.2%) with a peak delta of 9/255**. What it
   actually is: a 28 px near-white disc under an 80 px cyan halo, 19.2% of the
   frame lit. It is a *good big soft blob* and a *non-existent pulse*. Both facts
   are knobs, not laws.
2. **Do NOT plumb the celestial compositor into the creature.** [V] Every
   celestial pixel tests `kStarDepth (== 1) > depth`, i.e. **sky only**. The
   pulsar cannot draw inside the ring of balls through that path at any radius.
   The creature already owns a better one: `u02::glow_splat` is the same baked
   corona sprite with a **parameterised depth**, and it works today.
3. **The smear already exists and I have looked at it.** [R] §15 motion trails,
   shipping on `white-dwarf`: a bright core with eight decaying stamped ghosts
   blurred into haze. It is *exactly* "a broken frame buffer". The look is
   settled; only the cost and the depth law are open.
4. **The §15 implementation is full-viewport and costs ~224 full-screen passes
   per light.** [V] Its *fill* is 3% of the frame; its *work* is not. Three
   conduits cannot have it as written. Two cheaper shapes are ranked in §2.
5. **The cheapest full-scene smear in the machine is a persistence term on the
   quarter-res glow plane** — `POST.GATHER`'s 96x60 emissive buffer. 5,760
   cells, ~0.25 of a full-screen pass **for the entire frame regardless of
   conduit count**, +11.5 KB. It smears only what is tagged emissive, which is
   exactly the mana. This is my strongest recommendation and it is new.
6. **Lightning is buildable and one half of it is already written.**
   `unnamed02_fx.h:100` implements the `FX.LIGHTNING` recurrence from
   `reports/ADDLIGHTNING.md` verbatim. The missing half is hardware, and
   `ADDLIGHTNING.md` **overstates what exists**: see §4.
7. **Particles cannot be the big blobs.** [V] Particle size is `U 0.4.4`, so a
   particle is a **flat rect or flat tri of at most 15.9 px**, with no falloff.
   Big soft plasma is the splat path, not the particle path.

---

## 1. The Noctis pulsar — what it is, what it costs, what moving it needs

### 1.1 What it is [V][R]

* Subject `pulsar`, `zhao_reel.cpp:4249`; celestial case 3 at `:1225`.
* Screen-space, hard-coded: `L.x_px = 192, L.y_px = 120`. `disc_r_px = 28`,
  `halo_r_px = 80`, tint `(0,63,63)` cyan, `flare_mode = 2`, `trail = nullptr`
  (the subject explicitly opts out of the smear).
* The strobe law is frozen: `pulsar_active(spin_phase) = spin_phase < 0x4000`
  (`zref_star.hpp:98`), gain 160 vs 32. It gates **the flare splats only**
  (`star_compose.cpp:504-506`) — not the disc, not the halo.

### 1.2 What it costs, measured off the render [R]

| quantity | value |
|---|---|
| lit (non-black) pixels | **17,691 = 19.2% of the frame** |
| near-white core (g>200) | 11,191 px |
| pixels that CHANGE between flash and no-flash | **150 (0.16%)**, peak delta **9/255** |
| distinct frames in 64 | **two** (CRCs `0x91630E42` / `0x7D0DF976`) |
| duty | 3 frames on, 8-9 off — the quarter-turn law, correct |

Draw cost is one 56 px CLUT8 quad + one 160 px CLUT8 quad, both nearest-sampled
through a 64-entry palette: **~24,000 pixel-visits, 0.26 of a full-screen pass**.
Animation costs **zero** — it is a palette/gain change, per §4's bake-don't-compute law.

**The honest verdict:** as a bloom it is excellent and nearly free. As a *pulsar*
it does not pulse. The subject note claiming the strobe "is now clearly visible"
is wrong at 240p, and this is the crayon-grain failure mode again — mathematically
present, visually absent.

**To make it pulse**, modulate what is actually large: `halo_r_px` and the ramp
gain, on the same duty phase. A halo breathing 60 to 90 px at 4 Hz is a huge read
and still costs only the extra annulus (~14,000 px on the flash frames).

### 1.3 Putting it inside the ring of balls

**The ring's pocket is small.** [V] Hinges A/B/C sit at `(170,780)`,
`(-120,1450)`, `(-620,1150)` mm (`unnamed02_art.h:65-67`); that triangle's
inradius is **193 mm**, and the loop blade (`kLoopBladeRxMm = 105`) eats into it,
leaving a clear disc of roughly **130-190 mm**. Against `kBodyRadiusMm = 450`
rendering ~35 px, that is **about 10-15 px of screen radius** at the house camera. [I]

So "the pulsar inside the ring" means: a **~12 px core inside the loop**, with the
halo deliberately spilling out over and behind the arms. That is the pulsar at
roughly half scale, and it is the right composition — the arms will read as a
cage around a contained light.

**What moving it requires — and the trap.** The celestial path will not do it.
`draw_clut_quad`, `composite_trail` and the glint loop all gate on
`kStarDepth > depth[idx]` with `kStarDepth = 1` (`zref_star.hpp:203`,
`star_compose.cpp:71,320,362`). Sky prefills depth 0, so **a celestial pixel
paints on sky and nowhere else.** Behind the creature's own arms, over terrain,
inside the loop — all rejected. This is the same wall the last recon found on the
planet bloom (its §8 item 7), and it is a frozen law, not a bug.

**The route that works today, with no engine change:** `u02::glow_splat`
(`unnamed02_fx.h:350`) is the same `zref::star::corona_sprite` bake, the same
64-entry per-frame ramp, the same saturating additive composite — but it takes
`centre_d` as a parameter and depth-tests against the *conduit's own* 1/w, and it
has a `depth_test = false` mode already used for the core that shines through the
belly (`zhao_reel.cpp:2955-2958`). Aiming it at a ring-centre anchor instead of
`kBRoot` is **a changed anchor and two changed radii**. That is the whole job.

Give the anchor its own bone (the other recon owns the rig; flag it there) so the
light moves when the hinges play with it — Direction §4's "the rig and the effect
are one performance".

---

## 2. SMEAR — the signature look. Ranked menu.

The owner: *"smear like crazy, like it has a broken frame buffer."* That is not a
metaphor here; it is the literal source of §15. Noctis **retained the indexed
framebuffer while travelling, cleared palette bits, subtracted from intensity,
smoothed twice and drew the fresh star sharp** (`spec/stars_and_flares.md` §15).
Zhaozhou reconstructs that from a position ring instead of retaining the frame.

**I rendered it.** [R] `white-dwarf`, frames 8/20/36: a hard bright core with
eight discrete fading ghosts behind it, blurred until the scallops half-dissolve
into a directional haze. Lit footprint **2,336-2,453 px = 2.5-2.7% of the frame**.
It looks precisely like the ask. Contact sheet: `scratchpad/recon-fx2/wd_sheet.png`.

### The five techniques, ranked by (impact / cost) with what each needs

| # | technique | look | cost per conduit | what it needs |
|---|---|---|---|---|
| **1** | **Glow-plane persistence ("glow echo")** | whole-frame smear on everything emissive; trails follow real motion for free | **~0.25 of a full-screen pass for the WHOLE FRAME**, conduit count irrelevant | 1 new hardware term + 11.5 KB. See §2.1 |
| **2** | **Stamp trail of glow splats** (no blur) | eight scalloped ghosts — deliberately steppy, reads as a dropped frame buffer | **10.2% of a pass** (8 ghosts, r 30 to 8) | nothing. Reel authoring on `glow_splat` + an 8-entry position ring |
| **3** | **Bounded §15 reconstruction** (fade + directional blur, trimmed) | the shipped `white-dwarf` look, hazy not steppy | **7.6 passes** at 4 ages x 2 blur passes over a 96x96 box | port ~80 lines of `star_compose.cpp` into `unnamed02_fx.h` with a parameterised depth and a bbox |
| **4** | **§15 as implemented** | the shipped look exactly | **~224 passes** | nothing new — but see §2.2. Unaffordable at three conduits |
| **5** | **True framebuffer feedback / deliberate non-clear** | full RGB persistence, ghosting on the creature itself | 1 pass + a 276 KB frame copy | `POST.ECHO` revived **and** a read-back path that does not exist. See §2.3 |

### 2.1 The recommendation: persistence on the quarter-res glow plane [V][I]

`POST.GATHER` already produces **a 96x60 RGB glow plane from emissive-tagged
fragments** during resolve (`design/contracts/POST.GATHER.md:33-44`), and
`POST.COMPOSITE` already upscales it additively. Every additive effect in this
creature — the splat, the bolt, the particles — already writes a GLOW tag
(`RASTER.FRAGMENT.md:64-67`, `glow_tag()` throughout `star_compose.cpp`).

Add one term: `glow[t] = sat(gather[t] + k * glow[t-1])`, `k` a frozen Q0.16
constant on the `61/64` model `radial_decay` already uses.

* **Cost:** 5,760 cells x (read + mul + add + write) is about **23K ops = 0.25 of
  a full-screen pass, for the entire frame**, however many conduits are on it.
* **Storage:** one more 96x60 RGB565 plane = **11.5 KB M10K**, doubling the
  existing POSTBUF allocation (`sky_and_beams.md` §3: 11.5-12.3 KB today).
* **It smears only emissive things.** Terrain, the creature's pigment and the sky
  are untouched; the mana ghosts. That is the correct scoping and it falls out of
  the tag lane for free.
* **It is a smaller ask than POST.ECHO** in every dimension: 1/16 the pixels,
  1/24 the bytes, and it rides a buffer that already exists and is already
  read back by the compositor.
* Compare: §15 needs **two viewport-sized u8 scratch planes per active light**
  (its own stated bound) — 184 KB per light against 11.5 KB for the whole frame.

**On the record for the hardware lane:** a `glow_persist` mode of
`POST.COMPOSITE`'s glow path, sibling to `radial_decay`. One frozen decay
constant, one extra plane, saturating accumulate. `POST.COMPOSITE` is phase 11,
maturity SPECIFIED, no RTL — so this lands as spec text now and costs nothing
until that block is built.

### 2.2 Why §15 as written is 224 passes [V]

`trail_source_step` (`star_compose.cpp:274-308`) iterates **the whole viewport**
`p.x0..p.x1 by p.y0..p.y1`, not a bounding box, and is called **once per age**
(up to 8), each doing 1 fade pass + `passes = 3` eight-tap convolutions:

    8 ages x (1 + 3x9) ops x 92,160 px  =  20.6M pixel-ops  =  224 full-screen passes

Plus two full `std::vector<uint8_t>` allocations of `w * (vy0+vh)` **per light per
frame** (`:393-395`). The rendered *fill* is 2.5%; the *work* is not, and the
difference is the trap. Restricting the plane to the trail bbox is a pure
optimisation with no visual consequence if the box covers stamps + kernel reach
(4 texels x 3 passes x 8 ages, about 96 texels of spread) — I have **not** proved
that by rendering, so it is [I], and it is the first thing to prototype if route 3
is chosen.

### 2.3 Full framebuffer feedback — allowed by the specs, but read this first [V]

Under Direction §0 the question is what the specs permit, and the answer is
*"nothing yet, and the nearest block is the most fragile one in the machine."*

* `POST.ECHO` (`design/contracts/POST.ECHO.md`) is **one-directional**: it echoes
  the composited frame **out** to a capture buffer. Nothing reads it back in.
  Feedback needs a second, unwritten path into `POST.COMPOSITE`.
* It is **`deferred: true`, `cut_order: 1`** — *"the first thing to go if
  synthesis fails"* (`design/blocks.yml:3072-3100`). Its contract's own words:
  *"keep no expensive storage or datapath for it now"*, synthesis ceiling
  **zero**. Ten of its thirteen sections are deliberately unwritten.
* `spec/stars_and_flares.md:190` scopes it out explicitly: *"No persistent
  framebuffer and no full-scene `pfade`."* That is a scoping statement for the
  stars spec, not a global refusal — Direction §0 permits reopening it.

**Building the creature's signature look on cut-order 1 is the wrong bet.** The
glow plane (§2.1) buys most of the look on a buffer that is already scheduled,
already read back, and 1/16 the size. If the architect wants full-scene RGB
persistence anyway, that is a real ask and it belongs on the hardware record as
*"revive POST.ECHO and add a compositor read path"* — stated as such, not smuggled
in as a knob. In the software reel it is free and can be prototyped immediately:
the reel already runs full-frame post passes after compose
(`zhao_reel.cpp:2780+`, the contour/boil/cel-ink experiments).

---

## 3. Big plasma blobs — the answer is the splat, not the particle

### 3.1 Why not particles [V]

`Particle::size` is `uint8_t` in `U 0.4.4` px (`zref_render.hpp:194-202`), so
`side = raw/16` and the **maximum particle is 15.9 px**. `blit_pattern_block`
paints a **flat solid rect**; the tri branch paints a **flat 3-vertex fan**
(`sprites.cpp:82-104,178-195`). No radial falloff exists anywhere on that path,
and `PART.SOFT`'s contract emits a scissored **rect span** too
(`PART.SOFT.md:48-53`). Additive is real and wired (flag b2, `sprites.cpp:158`),
but additive flat squares at 240p are flecks. That is what the owner axed.

### 3.2 The blob that works, today [V][R]

`u02::glow_splat` + `zref::star::corona_sprite` is a **big soft bright additive
blob with a parameterised depth**, already on this creature, already proven.

I baked the two profiles the engine owns and splatted them at 384x240 against a
dark violet ground (`scratchpad/recon-fx2/blobs.cpp`, `blobs2.png`):

* **`corona_sprite(0)`** — §4's frozen **linear cone**. Good, slightly conical.
* **`corona_sprite_bloom(24)`** — the **unratified Lorentzian**
  (`star_bake.cpp:170-190`), `63a^2/(a^2+rr^2)`: a tight saturated core with a
  skirt that never reaches zero inside the sprite. **This is the plasma one.**
* **`corona_sprite_bloom(48)`** — a broad even ball. Good for a slow mana pool.

Its comment is the owner's own phrase almost verbatim, and its cost is
**identical** — the LUT is the texture; a different profile is bake time and
nothing else. It is called by nothing today. Wire it up.

**One art finding, and it matters more than the profile choice.** [R] My first
bake gave all three blobs a hard circular rim, because `glow_build_ramp` maps
index 1 to the ramp's `lo` colour and the current `kGlowLo = {40,8,64}` is a
visible violet. **The "no findable edge" property needs the ramp's bottom to fade
to black, not to a dark colour.** With `lo = {0,0,0}` the same sprites read as
formless bloom with no boundary anywhere. Compare `blobs.png` (rimmed) and
`blobs2.png` (formless): same code, same profiles, one constant.

### 3.3 Blob cost arithmetic [I]

The sprite is nonzero over 69% of its square (circle r=60 in a 128 grid), so a
splat at radius `r` writes about `2.76 r^2` px, each 1 LUT read + 3 saturating adds:

| r px | written px | % of a full-screen pass |
|---|---|---|
| 46 (current centre glow) | 5,840 | **6.3%** |
| 30 | 2,484 | 2.7% |
| 15 (a ring-pocket core) | 621 | 0.67% |
| 8 (a plasma bullet) | 177 | 0.19% |

**Three conduits, each a 46 px halo + a 15 px ring core: 21% of one pass.**
Colour is free — one 64-entry ramp per frame is shared by every splat in the
frame, so blue, violet, gold and cyan mana cost the same as one colour if they
share a ramp, and one extra 64x3-byte build if they do not.

---

## 4. LIGHTNING, under the new rule

### 4.1 What the specs define [V]

* `reports/ADDLIGHTNING.md` is the sanctioned answer and it names the chain:
  deterministic bolt path -> `FORGE.PRIM` ribbon -> `RASTER.FRAGMENT` additive ->
  `POST.GATHER` glow tag -> `POST.COMPOSITE` bloom, with `PART.*` sparks.
* `FORGE.PRIM` (`design/contracts/FORGE.PRIM.md`): six frozen families including
  **ribbon**; `MAX_SEGMENTS = 64`, `MAX_SIDES = 8`, worst case 1,024 triangles;
  **owns no memory** — *"a ribbon described by 8 parameters costs 8 parameters of
  bandwidth instead of 1,024 triangles of vertex data"*. A 24-segment bolt is
  **48 triangles**. Geometry is not the cost.
* `RASTER.FRAGMENT` already owns the blend: `beam_additive_fade` =
  `colour = tex x vertex; dst = sat(dst+src)`, Z-test on, Z-write off, ADD
  (`RASTER.FRAGMENT.md:65`). Four modes exist: REPLACE / ALPHA / ADD / ADD_MOD.
* The refusal was **live per-pixel line drawing** (`stars_and_flares.md:87`,
  charter §26) — an unbounded read-modify-write, i.e. a fragment program. A
  bounded ribbon is not that, and never was.

### 4.2 Correction: ADDLIGHTNING.md overstates what is built [V]

That report says *"the present RTL is real, but it currently owns topology only."*
**I cannot find it.** `fpga/rtl/forge/` contains exactly one module,
`zhao_forge_cliff.sv` (FORGE.CLIFF, the terrain rim plan). Nothing in `fpga/rtl/`
mentions ribbons or `MAX_SEGMENTS`. There is **no `zref::ForgePrim`** anywhere in
`reference/` or `tests/`. `FORGE.PRIM` is maturity **SPECIFIED**, `maturity_log:
[]`, and its named tests do not exist.

So the missing work is **larger** than that report implies: not "chiefly the
position evaluator" but the topology generator, the evaluator, the reference
oracle and the test suite — the whole block.

### 4.3 Half of it is already written, on this creature [V]

`unnamed02_fx.h:100-130`, `bolt_beads()`, implements the `FX.LIGHTNING`
recurrence exactly as `ADDLIGHTNING.md` specifies it: `P_i = lerp(start,end,i/N)
+ perp1 x jitter(seed,phase,i) + perp2 x jitter(seed^2,phase,i)`, `kBoltSegs = 12`
plus an 8-segment branch, re-hashed every `kBoltRehashFrames = 3` so it strikes
rather than boils. Its header states the migration intent outright: this
authoring moves unchanged onto the ribbon evaluator the day it lands.

**But it does not read as lightning.** [R] I rendered `unnamed02-crackle`
(600 frames). At native 384x240 the bolt is **a scatter of disconnected white
triangles inside the loop** — see `scratchpad/recon-fx2/crackle_06.png`. The
recurrence is right; the *representation* is wrong. Beads at 12 segments over a
short span leave visible gaps, and flat white tris have no core-and-glow.

### 4.4 The route, and what it puts on the hardware record

**Now, reel-side, no engine change** — three fixes to the existing evaluator,
all authoring:
1. **Draw the segments, not the vertices.** Stamp along each segment at ~2 px
   spacing so the path is continuous. Same recurrence, more beads.
2. **Two layers:** a hot narrow core (small, near-white) over a wider calm
   additive halo. `ADDLIGHTNING.md`'s own "Zhaozhou lightning" recipe.
3. **Let the smear do the afterglow** (§2). A bolt that ghosts for four frames
   reads as a strike; a bolt that vanishes reads as noise.

**On the record for the hardware lane** — this is the honest list:
* **`FORGE.PRIM` ribbon family**: topology generator + parameter block +
  position evaluator + `zref::ForgePrim` oracle + directed/random tests.
  Phase 11, SPECIFIED, **zero RTL, zero oracle** today.
* **`FX.LIGHTNING`** as an *effects-level* contract (not a seventh Forge family),
  with the field list in `ADDLIGHTNING.md`: start/end anchor, tick, seed,
  segments, jitter, width, colour, intensity, branches, lifetime, semantic
  weight, viewport mask. Bounded: at most 24 segments, at most 2 branches of 8.
* **The LOD ladder** it needs already exists in `PART.LADDER` (frozen owner
  ruling 2026-08-31 §2.5): ribbon -> soft sprite -> glint, chosen per camera.
* Cost when built: a 24-segment 3 px-wide bolt is **48 triangles and ~400 px** —
  **0.4% of a pass**. Lightning is not a fill problem. It is a block problem.

---

## 5. Small emitters shooting plasma from inside the ring

Feasible, cheap, and the answer is **mini glow splats, not particles**.

* 8-12 emitters launched from the ring pocket, each an `r = 6..10 px` splat on
  the same shared frame ramp: **12 x ~200 px = 2,400 px = 2.6% of a pass**.
* Integer ballistics already exist in the reel (`zhao_reel.cpp:3088-3099`) and
  the fx header already runs per-kind emitter tables — this is a smaller table,
  not new machinery.
* Smear them by pushing 3-4 stamp ghosts per bullet (§2 route 2): **4x cost =
  10% of a pass**, still fine.
* Where they must NOT come from: `draw_population`. A 15.9 px flat additive
  square is the thing that was just axed.

---

## 6. Other candidates — concrete, for the owner to choose among

All of these are reel authoring over machinery that renders today, unless marked.

1. **Breathing halo.** Drive `kCentreGlowRadiusPx` and the ramp gain off the
   body's stretch/inhale curve. The creature inhales, the mana swells. Zero new
   code — a knob wired to an existing pose value. **Free.**
2. **Ring-caged core.** The pulsar core at ~12 px inside the loop with a 46 px
   halo spilling through the arms, so the arms silhouette *against their own
   light*. Depth-test the halo, draw the core with `depth_test = false` so it
   glows through the blade. **~7% of a pass.** Already-proven two-layer pattern.
3. **Mana that answers the hinges.** The three hinge anchors already exist in
   `FxAnchors`. Put a small splat on each and let the hinge play modulate their
   gain — the effect becomes the performance Direction §4 asks for. **~1%.**
4. **The anamorphic streak.** `zref::star::FlareSprites::streak` is a baked 96x16
   additive sprite that already exists (`zref_star.hpp:242-247`). One splat at
   the strike frame is a horizontal flash bar across the creature. **~1,500 px,
   1.6%.** The cheapest "something just discharged" in the machine.
5. **Palette boil on the mana ramp.** §3's CLUT rotation: `rot = (tick/3) mod 63`
   over the glow ramp makes the blob's colour crawl and churn. **Touches zero
   pixels** — it is a palette rotation. Free motion.
6. **Two-tone plasma.** Two ramps, two splats, slightly offset and
   counter-rotating: blue core, violet outer, beating against each other. One
   extra 192-byte ramp build per frame. **+3%.**
7. **Drip.** `kDroplets` was the one deliberately opaque kind. Kept as 3-4 *large*
   splats that fall and fade, it is the only non-additive read in the set and
   gives the eye something solid to track. **~2%.**
8. **Glow-echo whole-creature ghosting** (needs §2.1). Because the echo is on the
   emissive plane, the creature's own bright rim ghosts too when it moves fast —
   the "hasty, slightly clumsy flight" of Direction §5 gets its motion blur for
   free, from the same term. **Included in §2.1's 0.25 passes.**
9. **Bolt-lit environment.** `kCreatureMaxPointLights = 4`
   (`zref_creature.hpp:1084`) and the additive light term is approved and wired
   (`reports/CREATURE-LIGHT-ADDITIVE-TERM.md`). One broad cyan source pulsing on
   the strike frame makes the bolt *illuminate* the pink body. **+3 MACs and one
   saturating add per source per fragment.** The trap from `08-LIGHTING.md`
   applies: a source that clamps all three channels becomes a hue-neutral
   floodlight that erases the rest of the rig.

---

## 7. Cost, assembled

Per full-screen pass (92,160 px). Three conduits on screen, which is the case
that decides this.

| element | per conduit | x3 |
|---|---|---|
| 46 px outer halo | 6.3% | 18.9% |
| 12-15 px ring core | 0.7% | 2.1% |
| 3 hinge splats @ 10 px | 0.9% | 2.7% |
| bolt, continuous two-layer, 24 seg | 0.4% | 1.2% |
| 12 plasma bullets @ 8 px | 2.6% | 7.8% |
| **subtotal, no smear** | **10.9%** | **32.7%** |
| + smear route 2 (stamp trail, 8 ghosts) | 10.2% | 30.6% |
| **total, route 2** | **21.1%** | **63.3%** |
| + smear route 1 (glow echo) instead | — | **+0.25% total** |
| + smear route 3 (bounded §15) instead | 760% | 2,280% |
| + smear route 4 (§15 as written) | 22,400% | 67,200% |

**Reading:** everything except the smear is comfortably inside one full-screen
pass for three conduits. **The smear is the entire cost decision**, and the
spread between the cheapest and the shipped implementation is four orders of
magnitude. Route 1 for the machine, route 2 for the reel today.

**Health warnings.** 0 of 91 modules have an RTL fit at HEAD — do not quote
timing slack. Every percentage here is arithmetic against a stated capacity. The
100 MHz clock is a placeholder. `RASTER.INTERP`'s own budget is 1 pixel walked
per clock against 1,666,667 clocks, so "one full-screen pass" is about 5.5% of a
frame's clocks — the 63% figure above is **3.5% of the frame's clock budget**, not
63% of it. The real fill risk is DDR and the per-pixel path, where
`PER_PIXEL_BUDGET.md:135-140` found *"no slack anywhere"*.

---

## 8. Provenance

**Rendered and looked at [R]:** `pulsar` (64 frames, two distinct, 150-px strobe,
19.2% lit), `white-dwarf` (the §15 smear, 2.5-2.7% lit), `noctis-flare`,
`unnamed02-crackle` (600 frames, the bolt reads as loose triangles), and two
bakes of `corona_sprite` / `corona_sprite_bloom` at 384x240 through a plasma ramp.
Artefacts: `scratchpad/recon-fx2/{pulsar_flash,pulsar_off,wd_sheet,nf_sheet,crackle_sheet,crackle_06,blobs,blobs2}.png`.

**Verified in source [V]:** the pulsar subject and celestial case 3; the duty law;
`kStarDepth == 1` and the sky-only celestial depth gate at all three call sites;
`trail_source_step`'s full-viewport loops and per-light plane allocation; the
trail kernel's heading tracking; `glow_splat`'s parameterised depth and its two
call sites; `corona_sprite` / `corona_sprite_bloom`; `glow_build_ramp`'s index-1
behaviour; the particle record's `U 0.4.4` size cap; `draw_population` flag b2 and
the additive branch; `PART.SOFT`'s rect span; `POST.GATHER`'s 96x60 emissive
plane; `POST.ECHO`'s deferral, cut_order 1 and one-way direction; `FORGE.PRIM`'s
frozen limits and SPECIFIED maturity; **the absence of FORGE.PRIM RTL and of
`zref::ForgePrim`**; `bolt_beads`' FX.LIGHTNING recurrence; the ring inradius
from the hinge constants; `RASTER.FRAGMENT`'s four blend modes and six recipes.

**Inferred [I]:** that bounding the §15 plane to the trail bbox is visually
neutral; that the ring pocket is 10-15 screen px (derived from world constants and
one measured body radius, so it is a projection estimate and should be checked by
rendering a test splat there); the glow-echo cost model; every percentage in §7.

**Not attempted:** no source edited, nothing committed, nothing published, no
background job left running. No hardware measurement exists for any figure here.
