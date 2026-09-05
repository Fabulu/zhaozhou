# RECON B — the ENGINE, the plasma, the smear, the rig cost

**Run:** `RUN-20260905-1910-manafold-p6-recon-engine`
**Lane:** own clones at `manafold-p6-recon-engine\{zhaozhou,Upheaval}`.
`Upheaval` pulled to `147cc9e` (Direction 5 with sections 0-BIS / 0-TER / 0-QUATER).
Hardware lane, `manafold-pass5-*` and `manafold-p6-recon-art` untouched.
**No creature constant was changed. Nothing published. Nothing deleted.**

Every number below came from a build I made (`build-direct.sh cel`, BUILD_RC=0)
and a render I ran, or from a line of source I quote.

---

## 0. THE HEADLINE — the axe must stay off

**`channel`'s particles ARE the mote field. Same function, same ramp, same knobs.**
This is the second of the coordinator's two cases, and it is not close.

    zhao_reel.cpp:5070   s.u02_mana  = slot == 2 ? 9 : (slot == 7 ? 0 : 3);
    zhao_reel.cpp:5074   s.u02_smear = slot == 2 ? 1 : (slot == 7 ? 0 : 3);

`manafold-channel` is slot 2, so it selects mana candidate **9**:

    manafold_fx.h:996  case 9: {  // the CHANNEL stack: the fold + the lightning strand
                         const int32_t ag = mana_fold(frame, slot, keys, A, stfx,
                                                      kRampAqua, crowd_pm, out);
                         mana_lightning(frame, A, out);

Candidate **3**, which every other clip uses, is the *same* `mana_fold(...
kRampAqua ...)` call without the lightning. Identical mechanism, identical
`kRampAqua`, identical `kMoteCount = 24`, identical `kMoteHaloRPxMin/Max = 7/10`.

**Honouring the axe would delete the best work on the creature.** Per Direction 5
section 0-BIS that is a stop-and-raise, and this is it.

### The differences that make `channel` look better are all DRIVE, not mechanism

| | `channel` (slot 2) | every other clip |
|---|---|---|
| mana candidate | **9** = `mana_fold` + `mana_lightning` | 3 = `mana_fold` only |
| smear rung | **1** SHORT/CLEAN, **tear off** | 3 LONG/GLITCHIER, **tear on** |
| `kKneadClipPm[slot]` | **900** — 2nd highest in the bank | 250-700 (hover 1000) |
| backdrop | `planet = 1` violet bloom, dark sky | terrain / peach sky |

`kKneadClipPm[2] = 900` is the likely reason it reads best: the fold's GRIP /
KNEAD / DRAG scalars are all driven by anchor motion, and channel works its hinges
harder than any clip but hover. **The mechanism was never the problem; the drive
on the other clips was low.**

### A bigger finding: Manafold has NO particles at all, and has not for two passes

    zhao_reel.cpp:3919-3921   cr_ctx.u02_add_pop.parts.clear();
                              cr_ctx.u02_tri_pop.parts.clear();
                              cr_ctx.u02_opq_pop.parts.clear();

Those three `zref::render::Population`s are **cleared every frame and never filled
anywhere** (2295-2297 declare, 3193-3206 draw-if-non-empty, 3919-3921 clear;
nothing pushes). Manafold puts **zero primitives on the engine particle path**.
Everything it draws is `glow_splat`.

So "too tiny and too many" fits the **ten-emitter particle set** — `SPEC.md`'s
"~76 live point sprites (mean ~2.5 px)" — which Direction 2 section 3 already axed
and which is **already gone**. It does not fit `mana_fold`'s 24 splats of 7-10 px
halo with an opaque 4-6 px core. Direction 5 section 0's axe, read against the
code, targets a system that no longer exists.

**For the record, as the owner asked:** cost was never a reason and no budget
document requires the axe. The mote field is **cheap and good**.
`MANAFOLD-INDEX.md` section 6 prices it at **~5.2% of a full-screen pass per
conduit** — the *strand* is the expensive part at ~12%. If anything here is
expensive it is the lightning the owner wants more of, not the motes.

---

## 1. THE SMEAR — what makes `hasty`'s trail, exactly

Two ingredients. Both are named constants; neither needs inventing.

### (a) The preset rung. `hasty` is on rung 3; `channel` is on rung 1.

    constexpr SmearPreset kSmearPresets[5] = {
        {0, 1, 0, 1, 0, 0},          // 0: no smear
        {620, 2, 40, 90, 420, 0},    // 1: SHORT/CLEAN     <-- channel
        {820, 4, 90, 260, 520, 0},   // 2: MID/GLITCHY
        {900, 6, 160, 430, 520, 1},  // 3: LONG/GLITCHIER  <-- hasty + 11 other clips
        {940, 8, 260, 560, 540, 1},  // 4: BROKEN-BUFFER
    };   // keep_pm, step_frames, jitter_pm, hard_clear_frames, gain_pm, tear

* **`step_frames`** is the glitch: the decay multiply lands only every N frames, so
  the trail *stutters* down instead of fading. Rung 3 steps every 6 frames.
* **`jitter_pm`** +/-160 gives per-cell uneven retention — the blotchy read.
* **`tear` = 1 rides rungs 3 and 4 ONLY.** `kSmearTear = {5 rows, 46 frames, 2
  cells}`: on hashed frames a 5-cell band composites with a +/-1-2 cell x offset.
  **`channel` at rung 1 has `tear = 0` — it has never had the tear.** That is the
  single most concrete thing to carry across.
* The plane is `96 x 60` composited at chunky 4x nearest; the blocks are deliberate.

### (b) Net screen drift — the one at risk from the motion re-author

Measured on my own renders (mana core = brightest 5% of aqua pixels, tracked frame
to frame):

| clip | abs vx | abs vy | **net x drift** | speed |
|---|---:|---:|---:|---:|
| `manafold-hasty` | 6.65 | 6.92 | **+1.90 px/frame** | 10.56 px/f |
| `manafold-channel` | 6.57 | 6.57 | +0.01 | 10.07 px/f |
| `manafold-mana-cyan` | 10.09 | 7.92 | +0.03 | 13.36 px/f |

The *instantaneous* core speed is nearly identical in hasty and channel, so the
plane is fed at the same rate. What hasty alone has is **net drift**: `build_hasty`
writes `c.root[f*3+0] = fxu((f - K/2) * kHastySpeedMmPerKey)`, a linear traverse.
The smear plane is **screen-space**, so the creature walks out from under its own
ghost and leaves it behind as a detached patch.

**Looked at, at 4x** (`evidence/hasty-smear-zoom.png`, frames 100/106/112/118): a
chunky pale-cyan residue with hard 4-px square edges and uneven per-cell brightness
hangs in the sky *behind and above* the creature while it advances. That is the
broken-framebuffer read, unmistakably. `evidence/smear-and-mana-plate.png` puts
channel / hasty / mana-stack side by side at 2x — channel's mana is a tight bright
cyan cluster in the loop window with **no detached residue at all**.

> **THE WARNING FOR 0-QUATER.** A pure vertical bob **oscillates and returns**: net
> drift goes to ~0 and the ghost re-attaches to the creature, reading as a halo
> rather than a trail — exactly what channel looks like today. **Rung 3 alone will
> not reproduce hasty's look on a stationary clip.** If the detached-ghost read is
> wanted on non-travelling clips, the plane needs relative motion some other way
> (longer `hard_clear_frames`, a deliberate per-frame cell-index drift in
> `smear_composite`, or keeping a small authored traverse). **Capture the
> before-plate from `manafold-hasty` at rung 3 BEFORE the motion is touched** — I
> have captured one and it is committed.

### What smear we can do TODAY vs what needs hardware

| route | state | cost |
|---|---|---|
| **1. Persistence plane** (`kSmearPresets`, `smear_update/feed/composite`) | **SHIPPING TODAY**, 5 rungs, depth-correct per cell | conduit-count **independent** |
| **2. Stamp-trail ghosts** (`kBulletGhosts`; Noctis's own `TrailHistory` 8-entry ring + `trail_fade` subtract-4, `zref_star.hpp:378-420`) | works today, no hardware | ~6.5% of a pass per conduit |
| **3. `POST.COMPOSITE glow_persist`** | **hardware ask 3, already filed**; phase 11, SPECIFIED, **no RTL** | +11.5 KB M10K, ~0.25 of a pass for the whole frame |
| **4. POST.ECHO framebuffer feedback** | **explicitly NOT asked** (cut-order 1) | — |

Route 1 is already the reference implementation for route 3. **Nothing about the
smear needs new hardware to ship this pass.**

---

## 2. "MORE NORMAL AND MORE LIGHTNING PARTICLES, DON'T GO OVERBOARD"

Measured on aqua/cyan mana pixels only (`g > r+25 & b > r+15 & max >= 110`), which
excludes the violet bloom and the pink pigment. `evidence/ceiling.py` is committed;
its first cut was **contaminated by channel's `planet=1` bloom core** (bloom is
near-white and matched a hue-neutral term) and those numbers were thrown away —
noted because it is exactly the gotcha section 4 lesson, "a near-white bloom core
eats additive effects", biting a measuring tool instead of an effect.

| clip | aqua px/frame | G=255 | B=255 | **BOTH G+B = 255** | mean hue spread |
|---|---:|---:|---:|---:|---:|
| `manafold-channel` | 628 | 26.0% | 44.3% | **22.1%** | 119.7 |
| `manafold-hasty` | 317 | 22.1% | 13.4% | 12.0% | 113.8 |
| `manafold-mana-cyan` | 745 | 11.0% | 17.8% | 11.0% | 89.0 |
| `manafold-mana-stack` | 1,983 | 24.2% | 27.5% | **22.1%** | 94.8 |

**Read this the right way.** `mana-stack` draws **3.2x channel's element density**
(pulsar + strand + fold) and lands at *exactly the same* 22.1% double-clamped
fraction, with **21% less hue spread** (94.8 vs 119.7). So:

> **The ceiling is not reached by adding elements. It is reached by OVERLAPPING
> them.** More splats in *new* positions cost almost nothing in clamping. More
> splats *stacked on the existing hot cores*, or higher gains, whiten immediately.
> **Add breadth, not depth.**

* **More "normal" particles:** raise `kMoteCount` (24) and *widen* the spread —
  `kCloudSpreadMm` (380), `kMoteOrbitRMinMm/MaxMm` (40/95), `kWanderCount` (3 of
  24), `kWanderEscapeMm` (780). **Do not raise `kMoteHaloGainPm` (340).** 24 -> 40
  motes at the same gain with a wider spread is the cheap direction; 24 -> 40 in
  the same pocket is the whitening direction.
* **More lightning:** `kStrandCount` is **1** today and the header itself says
  "the channel may carry 2". **The strand is the whitening element**, not the
  motes: `bolt_stamp` pushes a `kRampWhite` core at `kBoltCoreGainPm = 1000` with
  the depth test **off** — maximum-gain near-white (`kManaWhiteHi = 245,240,255`)
  shining through everything. Arithmetic: ~820 mm span / `kBoltStampMm` 22 = ~37
  stamps x (2 x `kBoltCoreRPx` 3)^2 = 36 px = **~1,330 px of near-white core per
  strand**, into a pocket 10-15 px across (gotchas section 10). **Two strands is
  the honest recommendation; three is over the line** and should be rendered and
  rejected on the record. The cheaper way to read as "more lightning" is
  `kSurgeMotes` (5) and `kSurgeBurstRPx` (11) — coloured, not white, already under
  the ceiling.
* **The smear feed has its own separate ceiling:** `smear_feed` refuses to
  accumulate a cell past **208** per channel and scales the add to preserve the
  ramp's hue ratio. That is why "feed it harder" whitens (the pass-5 lesson
  Direction 5 section 0-TER cites) — the *ramp* whitens with intensity,
  `kManaAquaHi` is `(175,255,236)`. **`kSmearFeedPm` (520) is the wrong knob; the
  rung is the right one.**

---

## 3. THE PARTICLE CEILING — section 11 CONFIRMED, from the spec

`spec/qformats.md:671` — the particle record: `| size | u8 | U 0.4.4 px |`.

**Confirmed: 255/16 = 15.9375 px maximum, and the record carries no colour, alpha
or falloff field** (species u6, size u8, spin u6, seed/flags u7; sums to 128 bits).
**Big plasma cannot be a bigger particle.** Settled; needs no further work.

| option for a large soft blob | cost | verdict |
|---|---|---|
| **`glow_splat` corona bake** (`zref::star::corona_sprite`) — radius is a free parameter; ramp index 0 must be black or every blob gets a hard rim | (2r)^2 px-visits; r=10 -> 400 px | **the answer, already in use** |
| `corona_sprite_bloom(24)` Lorentzian — tight core, skirt that never reaches zero | same as flat | baked but **unratified**; worth a plate |
| geometry (billboard / ring part) | meshlets + fill; needs a page or it renders **black** (section 0) | no advantage |
| the celestial compositor | **impossible** — `kStarDepth = 1` gates every celestial pixel at all three call sites | **cannot be done, do not try** |

---

## 4. COST — the axe, and the rig

### What deleting the mote field would give back (it is not being deleted)

`MANAFOLD-INDEX.md` section 6, the pass-4 honest multi-conduit figures:

| element | cost |
|---|---:|
| motes, per conduit | **~5.2% of a full-screen pass** |
| strand + surge, per conduit | ~12% |
| three folding conduits | ~0.6 passes = **3-3.5% of frame clock** |
| the smear | conduit-count **independent** |

The axe returns ~5.2%/conduit (~15.6% of a pass for the trio) — **one third of what
the lightning the owner just authorised costs.** Deleting the motes to buy room for
more lightning is a losing trade.

> **A correction the architect needs.** The brief called the mote field
> "arithmetic-bound". `09-ENGINE-GOTCHAS.md` section 5 says "cost is arithmetic
> here, not measurement" — meaning every cost figure on this project is a
> **calculation**, because `spec/counters.md` has no fragment/particle/polygon
> counters. The same section says the opposite of arithmetic-bound: **"Fill is the
> constraint, not geometry."** The mote field is fill-bound. Its per-frame
> arithmetic (MVC weights precomputed once in a `static`; per mote per frame two
> 18-term barycentric sums, a lerp, a yaw rotate, a drag lookup) is a few thousand
> integer ops — nothing.

### The rig expansion — priced, and essentially free

Today **12 bones of the 32 cap** (`manafold_rig.h:42`; `kMaxBones = 32`,
`zref_creature.hpp:144`). Balls today, all already boned: `kBJunctionF`,
`kBHingeA/B/C`, `kBLoopBase2`, plus `kBNeck`, `kBHingeD`, `kBRoot`, 4 eye bones.

`clip_frame_bytes(n) = 12 + 8n` (`zref_creature.hpp:314`). The bank is ~2,204 keys
across 15 clips:

| bones | bytes/frame | whole bank, uncompressed | share of the 24 MB pool |
|---:|---:|---:|---:|
| **12 (today)** | 108 | 238 KB | ~1.0% |
| 18 | 156 | 344 KB | ~1.4% |
| **20** | 172 | 379 KB | ~1.6% |
| 24 | 204 | 450 KB | ~1.9% |
| 32 (cap) | 268 | 591 KB | ~2.5% |

`05-BUDGETS.md`: ~343 KB compressed per creature type, ten resident types = 3.4 MB
= **14%** of the pool. **Doubling Manafold's bone count moves the whole pool by
well under one percent.**

Three more checks, all clean:

* **Per-vertex skinning does not get more expensive.** `RingSpec` carries `b0`,
  `b1`, `w0` — **two weights maximum**, independent of bone count.
* **The decoded-pose cache is already sized for 32.** 192 KiB / ~128 tuples =
  1.5 KiB/tuple; a pose is `bone_count x mat3x4fx` (48 B) = 1,536 B at 32 bones.
* **No per-meshlet bone-palette limit exists** in `zref_creature.hpp`; the only cap
  is the global 32.

**A concrete proposal to price against:** +2 connector bones (the tube span between
the body surface and each junction ball, front and back) and +3 inter-ball hinges
(A-B, B-C, C-D) for the "hinges all supposed to move up and down separately" ask =
**17 bones**, 15 spare. Even every ball plus every connector plus a hinge between
every pair lands under 24.

> **The rig expansion has no budget objection at all. If it does not happen the
> reason will be authoring effort or joint-limit tuning, not cost** — and per
> Direction 5 section 2a the suspects to check first are joint limits clamping the
> range, one driver moving all hinges together, easing flattening the interesting
> part, and foreshortening at the shipping camera. A per-hinge trajectory plot over
> a clip is the instrument; a flat line is the finding.

> **The deform sidecar is already live on this creature** — Direction 5 section 2c
> is correct, verified: `DeformSample` / `DeformRole` / `deform_skin_vertex` in
> `zref_creature.hpp:202-435`, `RingPart::deform_role/deform_axis/deform_strength`
> at 503-505, and `manafold_clips.h` already allocates the sidecar per clip and
> drives `compress_at`. Widening the hinges needs no new skinning.

---

## 5. LIGHTNING — already built, already approved by eye

`09-ENGINE-GOTCHAS.md` section 6 ("lightning does not exist") is **out of date for
the authoring, still true for the hardware.** Shipping today in `manafold-channel`:

* `bolt_path()` (`manafold_fx.h:290`) implements the `FX.LIGHTNING` recurrence
  **verbatim** — `P_i = lerp(start,end,i/N) + perp1*jitter(seed,phase,i) +
  perp2*jitter(seed^2,phase,i)`, <=24 segments, <=2 bounded branches.
* `bolt_stamp()` draws it as a **continuous two-layer chain of glow splats** — hot
  near-white core over a calm wider halo every `kBoltStampMm` = 22 mm, under one
  core radius at ~12.3 mm/px, so it is gap-free at native.
* `mana_lightning()` runs `kStrandCount` continuous strands re-hashed every
  `kBoltRehashFrames` = 7 (it buzzes, it does not strobe), plus `kSurgeMotes` = 5
  flowing along the path, endpoint bursts, and an anamorphic glint.

**Nothing needs to be built for lightning to ship this pass.** The hardware ask is
about making it *cheap*, not about making it *possible*.

### What goes on the hardware record — EXTEND, do not duplicate

`zhaozhou/reports/U02-MANA-HARDWARE-ASKS.md` already carries **three** asks plus a
pass-4 amendment. I checked `reports/` for anything newer; there is nothing.

1. **`FORGE.PRIM` ribbon family — the whole block.** `fpga/rtl/forge/` holds only
   `zhao_forge_cliff.sv`; no ribbon RTL, no `zref::ForgePrim` oracle, maturity
   SPECIFIED with an empty log. Cost when built: a 24-segment 3 px bolt is 48
   triangles and ~400 px = **0.4% of a pass**, against ~12% today.
2. **`FX.LIGHTNING` as an effects-level contract** above the ribbon.
3. **`POST.COMPOSITE glow_persist`** + the pass-4 amendment (persisted per-cell
   depth plane).

**The one new amendment this pass earns** — file under ask 3, do not open a fourth:

> **`glow_persist` must carry the ROW TEAR and the per-cell retention jitter, not
> just the quantised decay step.** The owner has now named `hasty`'s look
> specifically ("the perfect glitchy frame buffer looking thing... makes our mana
> look unique"), and the reel's reference implementation shows the read comes from
> three things together: `step_frames` (quantised decay), `jitter_pm` (per-cell
> retention jitter, one LFSR against the cell index) and **`tear`**
> (`kSmearTear = {5 rows, 46 frames, +/-2 cells}` — an index offset at composite,
> near-free). Ask 3 currently calls the jitter and the staggered hard clear
> "reel-side seasoning; if the block wants them they are one LFSR". That is now too
> weak: **the owner has picked the seasoning. It is the feature.**

Also: the ask's cost table is built on the old shipping stack (caged pulsar +
bullets + bolt + stamp-trail smear). The shipping stack is now fold + one strand +
persistence-plane smear. **Re-derive the table against candidate 9** so the
FORGE.PRIM justification quotes the real number.

---

## 6. THE SHIPPING RIG — exact invocation, and a Direction conflict to resolve

**"The experimental one from Zixxtrixx with the many colours" is the four-source
moving-light rig** — the warm inspection lamp plus blue, red and green world-space
sources on authored paths, which Direction 30 made the *normal* additive light
model (`08-LIGHTING.md`). In code:

    SceneSubject::creature_moving_light = true            // zhao_reel.cpp:1088
      -> cr_ctx.moving_rig  = g_u02_moving_rig            // :3545  (= &kU02MovingRigA26)
      -> sample_u02_moving_sources(...)                   // :4016  (u02-specific paths)
      -> kZixxMovingSourceCount = 4                       // :3019
      -> zc::g_creature_additive_light = g_zixx_additive_normal

Env, both set **explicitly** (the compiled default is not Cool Cross):

    ZIXX_EXP=celmain  ZIXX_LIGHT=diagonal-cool-cross  zhao-reel-cel.exe <outdir> <subjects...>

### TODAY, EXACTLY ONE SUBJECT RAISES IT, AND IT IS NOT A CLIP

    zhao_reel.cpp:7076-7084
      if (wanted("manafold-inspect")) {
        // Direction 3 sec.1: EXACTLY ONE subject carries the four coloured moving lights
        SceneSubject s = subject_u02_clip(0, "manafold-inspect", u02::kIdleKeys, true, nullptr);
        s.creature_moving_light = true;

and `subject_u02_clip` comments at :5077 "only manafold-inspect raises the moving
rig", because **Direction 3 section 1 called the four-light-everywhere look a
REGRESSION**.

**Direction 5 section 8 reverses Direction 3 section 1.** Direction 5 supersedes,
so the reversal stands — but **this is a deliberate, previously-rejected change and
it will move every plate, every CRC and every published render.** The architect must
state it explicitly rather than let an implementer discover it. Concretely it is
`s.creature_moving_light = true` inside `subject_u02_clip`, which also disables the
per-clip suns (`cr_ctx.sun_light = sub.sun != nullptr && ... &&
!sub.creature_moving_light`, :3550) — so **the named `kU02Sun*` per-clip suns stop
firing on every clip that takes the moving rig.** That is a second, non-obvious
consequence. The `planet = 1` violet bloom that makes channel's mana read so well is
independent of it and should be preserved.

Until that lands, **`manafold-inspect` is the only subject that can honestly be used
to judge under the shipping rig**, and it plays the *hover* clip, not channel.
**There is currently no subject that renders `channel`'s mana under the many-colour
rig.** Making one is arguably the architect's first task.
`10-GATE-CHECKLIST.md` item 1 exists for exactly this fault.

---

## 7. THE NOCTIS PULSAR — found, demoted to a candidate

Three things are called "pulsar"; the owner's "take the pulsar from Noctis we have"
means the first.

1. **`star-s11-pulsar`** — `effects-library.yaml:197`, spec class **S11** from the
   Noctis star gamut (`RUN-20260815-2307-noctis-suns-and-flares`). Reel subject
   **`pulsar`** = `subject_pulsar()` at `zhao_reel.cpp:4627`; celestial mode 3 at
   `:1226`. `disc_r_px = 28` (was 4), `halo_r_px = 80` (was 14), tint cyan
   `(0,63,63)`, lawful spin 715 angle16/tick, duty strobe on `spin_phase < 0x4000`.
   64 frames, `crc 0x69F44CA3` — **which I reproduced exactly on my own build**.
2. **`noctis-flare`** — celestial 2, S00. This is the one with the **motion smear**
   (`TrailHistory`, `zref_star.hpp:378-420`): an 8-entry position ring replayed at
   graded intensity with a subtract-4 decay. Smear route 2's ancestor.
3. **`u02::mana_fill` case 1, "the caged pulsar"** — the creature-local re-siting
   that already exists: `kPulsarCorePx` 13, `kPulsarHaloMinPx/MaxPx` 60/90 breathing
   at 4 Hz, `kPulsarCoreGainPm` 620. Visible in `manafold-mana-stack`.

**Placing #1 inside the ring of balls is impossible through its own path**: the
celestial compositor gates every pixel on `kStarDepth > depth` with `kStarDepth = 1`
at all three call sites, so it paints sky and nothing else by construction (section
10). The route is a creature-local `glow_splat`, and **#3 already is that**. So
"take the pulsar from Noctis" was **already done in pass 3**.

One caution if it gets folded in: gotchas section 9 records that the pulsar
subject's own note claims its strobe is "clearly visible" and, measured, it moves
**150 pixels of 92,160 with a peak delta of 9/255**, with only two distinct frames
in the sequence. **Do not inherit the strobe as authored.** And in my plate,
`mana-stack` (which carries the caged pulsar) reads **softer and whiter** than
channel — hue spread 94.8 vs 119.7. Folding the pulsar into channel risks trading
crispness for haze. Render it and look before adopting it.

---

## 8. WHAT MUST BE RETIRED — and one thing that must NOT be

**With the axe suspended this list is mostly moot.** It is the reference in case any
part of the fold system is later retired, and it contains one trap.

If the mote field ever does go, these go with it: `mana_fold` and `FoldState`;
`fold_stencils` / `fold_mvc` / `fold_weights` and the six stencils;
`kStencilPts/CentreU/CentreV/Scale/FaceYaw`, `kMoteCount/kWanderCount/
kMoteHaloRPxMin/Max/kMoteCoreOfHaloPm/kMoteHaloGainPm/kMoteCrowdPm/
kMoteOrbitPeriodMinF/MaxF/kMoteOrbitRMinMm/MaxMm`, `kCloudSpreadMm`,
`kWanderEscapeMm`, `kGripGamma/kCohBasePm/kCohMinPm`, `kDragLagFrames/kDragGainPm/
kDragMaxMm`, `kKneadJitterMm/kKneadVelRefMm/kKneadFeedPm`, `kFoldAnchorRestMm`; the
gates `U02_FOLD_FREEZE` / `U02_FOLD_LOCK` / `U02_FOLD_DEBUG`
(`zhao_reel.cpp:6919-6928`) and the globals `g_u02_fold_freeze/lock/debug/
release_pm`; mana candidates 3, 6, 8, 9 and the `fold_mana` branch at
`zhao_reel.cpp:3241`; `evidence/kneadcount.cpp` from the pass-5 QA run. The already
retired `U02_ABLATE_KNEAD` is gone and referenced only in comments.

> ### THE TRAP: `kKneadClipPm` IS NOT A PARTICLE KNOB
>
> Direction 5's carried-forward list says the `kKneadClipPm` knobs "are attached to
> a system being deleted. Retire them with it." **They are not.**
>
> `manafold_clips.h:523` `antenna_knead()` is a **BONE POSE LAYER**, composed onto
> `kBJunctionF`, `kBNeck`, `kBHingeA/B/C` and `kBLoopBase2` *before* every clip's
> `loop_pose` call. `kKneadClipPm[slot]` is its **per-clip gain** and `fold_phase()`
> (`manafold_clips.h:407`) is its **envelope**.
>
> **Deleting them would stop the antenna gesturing on every clip in the bank** — the
> exact opposite of Direction 5 section 2a ("the antennae are still super static")
> and section 6 ("it uses the ball hinges and plays around with them"). And
> `kKneadClipPm[2] = 900` is, per section 0 above, the most likely single reason
> `channel` looks best.
>
> **If any retirement happens, `antenna_knead`, `fold_phase`, `kKneadClipPm` and the
> `kKneadGrip*/kKneadWag*/kKneadTremor*` angles must be KEPT and given their own
> envelope.** They are the hinge expressiveness the owner wants more of. Also carry
> the pass-5 QA fix: replace the hand-written `slot < 15` at `manafold_clips.h:524`
> with `slot < std::size(kKneadClipPm)`.

---

## 9. Carried-forward defects the architect should not re-discover

From `RUN-20260905-1804-manafold-pass5-qa/FINDINGS.md`, still open:

1. **`kGazeMaxA16` is 34% over its own derivation.** `kWhiteRingTubeMm` moved 15 ->
   22 in pass 5 and the containment arithmetic was not re-run: the rule gives 1787,
   shipped is 2400. The comment at `manafold_art.h:285-291` still says "tube (15)"
   and is **factually false**. No gate exists. Re-derive it as a **committed gate**
   in `manafold_probe.cpp`.
2. **Seven of fifteen clips were retimed** by the pass-5 knead fix, three were
   reported: `pirouette` +111%, `hasty` +52%, `taunt2` +23%, `drift` +10% were never
   looked at. `hasty` is also the clip pass 5 used as its *control*.
3. `mediacheck.py` cannot fail on a 0-byte media file or an `index,follow` robots
   tag; `seam.py` is a meter with no threshold that returns 0 on nan/inf and whose
   stride-10 denominator aliases against clip periodicity.

---

## 10. Summary — possible now / needs hardware / what it costs

**Possible now, no hardware, no new machinery:** the whole plasma look
(`glow_splat` corona / Lorentzian bakes at any radius); the broken-framebuffer smear
including the row tear (five rungs, shipping); continuous lightning strands with
surge motes (in the bank today, approved by eye); the caged pulsar re-sited into the
ring pocket (done in pass 3); the full rig expansion to 17-24 bones; per-join skin
deformation through the existing sidecar.

**Needs hardware (all already filed; extend ask 3, open nothing new):**
`FORGE.PRIM` ribbons (turns the strand from ~12% of a pass into 0.4%);
`FX.LIGHTNING` as an effects contract; `POST.COMPOSITE glow_persist` (turns the
smear into +0.25% of a frame at any conduit count) — **amended** to carry the row
tear and per-cell jitter, not just the decay step.

**Costs, arithmetic (no fragment counters exist — `spec/counters.md`):** motes ~5.2%
of a pass per conduit; strand + surge ~12%; smear conduit-count independent; three
conduits ~0.6 passes = 3-3.5% of frame clock; rig expansion to 24 bones = +0.9% of
the 24 MB animation pool and **zero** per-vertex cost.

**Impossible, do not attempt:** a particle larger than 15.9 px or with any falloff;
a celestial star / pulsar / flare drawn in front of or inside a creature.

---

## Instruments and evidence committed with this run

* `evidence/ceiling.py` — the channel-ceiling headroom meter. **Read its docstring
  before trusting it**: its first version's hue-neutral term matched channel's
  `planet=1` bloom core and reported a contaminated number. The aqua-only mask in
  section 2 is the honest one. It says loudly when it finds zero pixels.
* `evidence/smear-and-mana-plate.png` — channel / hasty / mana-stack, 4 frames each,
  2x nearest so the 4-px smear cells stay honest blocks.
* `evidence/hasty-smear-zoom.png` — hasty frames 100/106/112/118 at 4x. **This is
  the before-plate Direction 5 section 0-TER asks for**, captured before the motion
  is touched.

**Renders made in this run** (`ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross`,
clean-clone build, BUILD_RC=0):

    pulsar               64 frames,   194 colours, crc 0x69F44CA3  (matches the pin)
    manafold-inspect    600 frames, 24850 colours, crc 0x86943538
    manafold-mana-cyan  600 frames,  7922 colours, crc 0x176A2E84
    manafold-mana-stack 600 frames, 10936 colours, crc 0x71570B33
    manafold-channel    420 frames,  9424 colours, crc 0xB489DCCD
    manafold-hasty      240 frames

## What I did NOT verify

* I did not judge the six mana-menu variants against each other by eye — the owner
  has picked and Direction 5 section 0-BIS says that picker is spent.
* I did not measure the *visual* effect of raising `kMoteCount` or `kStrandCount`;
  section 2's headroom is arithmetic plus a measured clamp fraction, and the actual
  rungs must be rendered and looked at.
* The trail/travel finding in section 1(b) is measured *and* looked at, but I did
  not render a hypothetical vertical-bob hasty to prove the ghost re-attaches. That
  is one render for the implementer and it should be done before the motion fix is
  accepted.
* Everything art-side — the pink, the fog, the eyes, the sheets — is Recon A's.

## Background jobs

Three background jobs were started (a recursive grep, the reel build, the
channel/hasty render). All three were polled to completion and the process table
verified empty of this run's children before the run was closed.
