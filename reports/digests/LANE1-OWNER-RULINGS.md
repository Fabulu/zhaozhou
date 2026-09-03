# LANE 1 DIGEST — THE OWNER RULINGS

Recon extraction of the nine authority documents, so a working agent can act
without re-reading ~200 KB. **Nothing here is new decision-making.** Every line
carries its source document and the commit date that establishes its rank.

Dates are **git commit dates**, not file mtimes — a rebase stamped much of the
tree 2026-09-03 and mtimes are worthless for ordering.

---

## THE TRUE ORDERING (`git log`, authoritative)

| # | document | commit | date | rank |
|---|---|---|---|---|
| 1 | `reports/WordOfCaution` | `c4613282` | **08-25 11:15** | oldest; still live on Earth60 |
| 2 | `reports/PIPELINEINGHINTS` | `5a946221` | **08-25 15:05** | advice, never ratified |
| 3 | `reports/OPEN-SPEC-DEPTH-QUANTISATION.md` | `db02ada1` | 08-31 01:08 | **SUPERSEDED** `f8c9ebbe` 09-02 22:51 |
| 4 | `reports/OWNER-RULINGS-20260831.md` | `f8c2b32a` | **08-31 04:30** | first ruling pass |
| 5 | `reports/DEPTH_PROFILE_NEXT_STEPS.md` | `74171521` | 08-31 06:06 | live checklist |
| 6 | `reports/OWNER-RULINGS-COMPLETE-20260831.md` | `f5d16530` | **08-31 22:17** | the 28-question authority |
| 7 | `reports/REARCHITECTUREADVICE.md` | `6633662c` | **09-02 11:54** | amended `3bc30824` 09-02 12:56 |
| 8 | `reports/Addendum` | `3a231a33` | **09-02 12:32** | corrects #7; **already applied** |
| 9 | `reports/OWNER-RULINGS-BUILDABILITY-20260902.md` | `94179885` | **09-02 21:44** | **NEWEST OF THIS SET — WINS** |

**Documents newer than all nine, owned by other lanes** (they outrank everything
above where they overlap):
`Islandrearchitect.md` 09-03 06:53 → `2` 07:19 → `3` 07:24 → `4` 07:56 →
**`islandrearchitecture5.md` 09-03 08:04 (LIVE)**; `SaveTheRendered.md` 10:23;
the two animation architecture files 14:13; **`WeNeedSomeMeasurements.md` 14:58**
(the source of the 37,719 / 100 / 497 gates).

---

# TOP 10 THINGS A WORKING AGENT MUST KNOW

1. **`OWNER-RULINGS-BUILDABILITY-20260902.md` (09-02 21:44) is the senior
   document of this set.** Its R1–R11, X1–X9 and T1–T12 supersede the 08-31
   rulings wherever they touch the same subject. It supersedes exactly three
   things and confirms the rest — see *Supersession ledger* below.

2. **Depth is CLOSED. Do not re-ask, re-derive, or re-choose `wmin`/`wmax`/
   `scale`.** Asking again *is itself the recorded error* — the question reached
   a published owner page twice. R1 + the superseded banner on
   `OPEN-SPEC-DEPTH-QUANTISATION.md`.
   **But the hardware still does not produce depth**: `zhao_geom_project.sv`
   contains no `invw24` and no `depth_profile` port. Steps 5–6 of
   `DEPTH_PROFILE_NEXT_STEPS.md` are open. See *Contradiction C1*.

3. **Never grow the on-chip triangle arena.** Ruling 08-31 #4 and R7 both say
   the production binner uses an external local-SDRAM `GEOM.PARAMBUF` in bank 3.
   "Choose a kMesh budget and grow the arena" is explicitly named as
   contradicting binding law. `GEOM.PARAMBUF` is now registered and
   REFERENCE_COMPLETE — the architecture is not open for reconsideration.

4. **`276,480` has exactly one meaning: `384 × 240 × 3.0` conservative
   PRE-Early-Z covered fragments.** Not samples, not survivors, not divides, not
   a measured frame. Cross-mode design target **320,000**; design deadline
   **1,333,333 clocks**; **1,666,667 is the FAULT BOUNDARY, not a target**
   (R2, and 08-31 §5.2). `tools/render/sample_budget.cpp` is a *conservative
   upper envelope only* and must never be cited as measured workload — already
   relabelled in-source per R2.

5. **The TMU workload is 1,094,600, not 541,640.** 541,640 is the ONE-sample
   figure (276,480 terrain + 92,160 sky + 128,000 stars + 45,000 clouds).
   `MATERIAL_ARCHITECTURE.md` ratified 0–3 samples per fragment; the three-sample
   terrain profile is 1,094,600 samples/frame before creatures and misses.
   **Sizing anything at 541,640 meets a target the material ruling already
   superseded** (`Addendum` 09-02 12:32, applied into `REARCHITECTUREADVICE.md`).

6. **The MHz hierarchy is a restatement of an owner requirement, not a
   relaxation.** 105 MHz acceptance floor (full composition) · 110 MHz
   architecture objective · 115 MHz stretch · **100 MHz product clock, no
   lucky-seed dependency**. Leaves design against 150 MHz; texture leaf island
   120–125 MHz across three seeds; texture-survivor composition 115–120 MHz.
   *"Do not reject the machine at 109 MHz"* and *"the 110–115 goal is abandoned"*
   are different statements; only the first is intended (`Addendum`, Phase 4 of
   the buildability brief).

7. **Class A / B / C authority is written law** (08-31 §11, restated 08-31 §6).
   Class C — **return to Fabian** — is: game-visible behaviour, permanent
   command/cartridge ABI, **capture-changing numeric law**, representation-ladder
   meaning, content-tier guarantees, feature cuts/defers, deterministic ordering.
   Everything else you decide (A) or decide-with-evidence (B).

8. **Overflow never truncates an army.** Every subsystem repeats the same law:
   The Measure degrades representations and seals quotas *before* the frame; an
   unexpected hard overflow **faults the frame, drains, repeats the previous
   complete frame, records source IDs** — and never publishes a frame missing an
   arbitrary tail. Binner, PARAMBUF, particles, terrain composed cache,
   malformed materials, animation residency misses: all identical.

9. **A leaf fit is reconnaissance, not authorisation.** Buildability brief,
   Phase 1: *"A leaf fit does not authorize production integration."* And
   *"a lucky 125-MHz leaf does not excuse a 100-MHz composed ready chain."*
   Pair this with CLAUDE.md's *"a gate passing is not the thing looking right."*

10. **Section 7 of the 08-31 authority is PROVISIONAL TUNING and must never
    reach RTL or the command ABI.** Mana trickle rates, the attenuation
    smoothstep, decay rates, the spell-level claim-reset table — game data only.
    *"A tuning value that reaches silicon stops being tuning."*

---

# SUPERSESSION LEDGER — what beats what, and by which date

| subject | loser | winner | note |
|---|---|---|---|
| depth `wmin`/`wmax`/`scale` | pass-1 single `0.5 / 16384` | **08-31 #1 three profiles**, confirmed R1 09-02 | banner added to the open-spec doc 09-02 22:51 |
| depth profile selection ABI | "two reserved bits **or** an additive command" (08-31 #1, `DEPTH_PROFILE_NEXT_STEPS` step 3) | **`SetView.flags[1:0]`, no separate command** (08-31 COMPLETE §1) | the only Class-C part; decided, in `spec/commands.zidl:257` |
| binner capacity | "just put the arena in SDRAM" (pass 1) | **08-31 #4 `GEOM.PARAMBUF`**, re-affirmed R7 | |
| TEXJOIN v2 state | `required_mask = PRIMARY \| AUX`, one primary result (`REARCHITECTUREADVICE` as written 11:54) | **`sample_count 0..3` + `sample_required/arrived_mask[2:0]`** (`Addendum` 12:32) | correction already merged at 12:56 |
| Field fit point A | `OUTSTANDING=16 DIST_BANKS=8 RING_UNITS=8` | **`CTX=32 LANES=4 OUTSTANDING=12 GATHERS=4 REGS=64 DIST_BANKS=4 RING_UNITS=4`** (`Addendum`) | the 16/8 point was already *discredited* by the Field optimisation record |
| POST.GATHER M10K ceiling | `≤ 10 M10K` | **`≤ 30 M10K`** (R5) | shape, not bytes: 7,680 Duo cells at 256×40 |
| terrain residency architecture | direct-mapped prototype | **256 sets × 4 ways, `zhao_terrain_residency_v2` built BESIDE it** (T9/T10) | *"Must not be integrated as the world directory"* |
| terrain residency interface | interface lacking `island_id` | **`{resource_epoch, island_id, patch_ix, patch_iz}`** (T1) | two islands may legally overlap in local patch coords |
| texture cache design | generic 4-way write-back w/ PLRU (older 110-115 spec) | **four independent direct-mapped read-only lanes, one blocking fill** | the 110-115 spec described a cache that does not exist |
| TMU v2 status | its own stale header ("incomplete, nothing retires") | **functionally complete and tested**; problem is streaming, not completion | header is a documented lie |
| AUX interface | TMU-style perspective-correct U/V (the old TEXJOIN test's model) | **world x/z + patch envelope x0/z0/x1/z1 + sheet handle, issued at ALLOCATION** | AUX must not wait for perspective |
| "zero contracts unwritten by accident" | that audit claim | **FALSE — `SW.STREAM` is a real stub** (T12) | now filled: 1 residual `TODO` mention, historical |

---

# NUMBERS THAT ARE THE OWNER'S AND MUST NOT BE INVENTED

The documents repeatedly mark values as the owner's to choose. Reproducing them
here so nobody re-derives one.

**Already chosen — use exactly these, never regenerate:**

| value | number | source |
|---|---|---|
| depth profiles | `WORLD_LONG 1.0/16,384 SCALE 2^40 d(wmax) 1024` · `WORLD_STANDARD 0.5/8,192 SCALE 2^39 1024` · `CLOSE 0.25/2,048 SCALE 2^38 2048` · profile 3 **reserved and refused** | 08-31 #1, R1 |
| serious-match tier | **256 active creatures**, 128/player Duo, 64/participant in 4-way, ≤4 wizards, 32,768 particles, one Level-9 spectacle | 08-31 COMPLETE §5.1 |
| kMesh guarantee | **32 machine-wide**, ≥16 per active Duo view; giant is a **separate ≥32,768 tile-reference reserve** | R7 |
| pre-Z fragments | 276,480 (Z60 stress) · 320,000 cross-mode target · 92,160 (Z60 1×) · 294,912 (Duo) · 307,200 (Storm) | 08-31 #3, R2 |
| clocks | design **1,333,333** · fault boundary **1,666,667** | 08-31 #3, R2 |
| particle capacity | **32,768 required**, 65,536 stretch *only after physical board bandwidth evidence* | 08-31 #2.1, R3 |
| particle bit layout | pos 54 / vel 33 / age 10 / species 7 / size 6 / spin 6 / flags 4 / variation 8 = 128 | 08-31 §2.1, exact field ranges in R3 |
| Forge limits | `MAX_SEGMENTS = 64`, `MAX_SIDES = 8`, **six families, no seventh** | 08-31 §6.5, R11 |
| meshlet | ≤64 unique vertices, ≤126 triangles, u8 local indices, 64-byte aligned | 08-31 #5, R11 |
| max children per spawn event | **16** (local bursts only; HPS may seed large populations directly) | 08-31 §2.4 |
| effect buffers | Z60 96×60 · Storm 80×60 · Duo 128×60 (quarter-linear) | 08-31 §9, §4 |
| compositor displacement bound | **±8 px X, ±4 scanlines Y** | 08-31 §9, R5, R6 |
| PARAMBUF minimum tier | 32,768 projected vertices / 8,192 triangles / 65,536 tile refs; preferred 65,536 / 16,384 / 131,072 in 4 MiB | R7 |
| terrain page | **21,376 B**, 1,024 local slots; 8×8 km at 2 m = 15,625 pages ≈ **318.5 MiB**, cannot all be resident | T2, preamble |
| terrain prefetch ceiling | **32 whole pages/frame** ≈ 41 MB/s. Provisional — board counters may LOWER it immediately; raising needs measured evidence | T7 |
| composed cache | **256 slots**, for live-composed patches only — *not* a cap on visible terrain | T6 |
| mip law | `mip17[i,j]=fine33[2i,2j]`, `mip9[i,j]=fine33[4i,4j]`, **no averaging**, 1,480 B in a 1,536 B record | T8 |
| set index hash | **CRC-8/ATM, poly 0x07, init 0**, LE bytes of `{island_id,patch_ix,patch_iz}`, xor `resource_epoch[7:0]` into the final byte | T9 |
| memory clients | `ENGINE0` framebuffer · `ENGINE1` render-geometry incl. PARAMBUF · **new `ZHAO_CLIENT_TERRAIN_BUILD = 6`, best-effort/background**. **Client 5 stays unspent** | T3 |
| command opcodes | `TerrainEpoch @ 0x0220` size 16 · `SubmitTerrainSet @ 0x0230` size 32 · patch record 32 B | T5 |
| combiner rounding | `unit_mul8=(a*b+128)>>8` · `modulate2x8=sat((a*b+64)>>7)` · `lerp8` w unit8 raw/256 | R9 |
| post transform | curves R[32] G[64] B[32] + signed 3×3 Q2.14 matrix + colour8 bias; **9 products/pixel against a 12-DSP post ceiling** | R6 |
| ladder thresholds | meshlet ≥18 px · shard 6–18 · ribbon ≥4 · sprite 2–8 · glint 0.5–2 · culled <0.5; **20% hysteresis, 4-frame hold** | 08-31 §2.5 — **Class B defaults, not ABI** |
| resource gates | **37,719 ALM / 100 DSP / ~497 M10K** (10% charter reserve on 41,910 / 112 / 553) | `WeNeedSomeMeasurements.md` 09-03 14:58 |

**Explicitly PROVISIONAL TUNING — game data, never RTL or ABI** (08-31 §7):
claim cell 2 m × 2 m = 1 Mantle cell, 32×32 = 1,024 per 64 m patch,
four-neighbour connectivity · trickle 0.25/s · per-well ≤2.00/s, **top two wells
only**, max 4.00 + 0.25 = 4.25/s · conduction needs `owner match && strength ≥
128 && not void` · attenuation `d≤64 m → 1.0`, `64<d<512: 1 − smoothstep((d−64)/448)`,
`d≥512 → 0`, times the weakest link on the path · claim propagation **4 Hz**,
64 strength/tick, ~2 m/s frontier · decay 6 strength/s unpressured, up to
18/s under hostile pressure · summon-emergence bonus **up to 15%** and
**nothing else** — no damage, armour, speed, healing, vision or range ·
the spell-level claim-reset table (L1 4–16 cells … L9 512–2,048 … L10 1,024–8,192
default, uncapped).

**Refused / reserved values a working agent may NOT fill in:**
depth profile 3 (*reserved and refused until re-proved*) · `TWOD.PLANE`
roles 2 and 3 (*descriptor refused*) · `MEASURE.HISTOGRAM`'s error metric,
bucket edges and cutoff rule (**the documented refusal IS the implementation**,
R10) · particle flags bit 3 (RESERVED, zero in, preserved zero) · the token's
unused transport bits.

---

# BINDING DECISIONS BY SUBSYSTEM
### with hardware consequence and current status

## DEPTH — `LIVE, PARTIALLY BUILT`
*08-31 #1 · 08-31 COMPLETE §1 · R1 · `DEPTH_PROFILE_NEXT_STEPS.md`*

* Three profiles + one reserved; `SetView.flags[1:0]` selects; zero = `WORLD_LONG`
  so existing zero-filled captures keep meaning. **No separate view-depth command
  in v1.** Games never upload arbitrary near/far.
* **`wmax` is a depth CLAMP, not a far-clip plane.** Consequence:
  **`GEOM.PROJECT` row 2 stays inert and the culler stays at FIVE planes.**
  Distant islands must not vanish at a wall — The Measure, fog and the
  representation ladder control distant visibility.
* `scale`/`shift` are **generated** from the frozen `rcp_u24` law. **Never
  hand-write the scale into RTL.** The power-of-two scale is a coincidence of
  these three near planes; a fourth profile would not have it and the generator
  must not assume it.
* `SCALE` is solved from the reciprocal's **actual output** at `wmin`, not the
  ideal `0xFFFFFF * Wmin` — the ideal form gives `0xFFFFFE`, one short of the pin.

**Status:** table generated (`zref_depth.hpp`, `compiler/src/generated/depth.ts`,
`QFMT_VERSION 3`), oracle `zref::depth_of` exists, spec §8 amended, ABI in
`spec/commands.zidl:257`, proof in `tests/proofs/depth_profile_law.cpp`.
**NOT DONE: steps 5 and 6** — see Contradiction C1.

## BINNER / GEOM.PARAMBUF — `LIVE, BLOCK REGISTERED`
*08-31 #4 · R7 · Phase 5*

Do not grow the frame-resident M10K arena (~49% of every M10K for one army).
External local-SDRAM parameter buffer instead. **`ENGINE0` stays the framebuffer
writer; `ENGINE1` owns the parameter buffer** under a frame-generation-checked
`MEM.GUARD` region. On-chip is limited to: tile directory (head/tail/count +
active tails), prefetch FIFOs, a small projected-vertex cache, and an
**opportunistic** expanded triangle-context cache — *not* the rejected full
TriangleContext arena. One tile lifecycle = one clear + one resolve. **No
framebuffer readback. No naive flush-and-continue.**

Records: `ProjectedVertex` 24 B · `TriangleDescriptor` 16 B (vertex **ids**, not
vertices) · tile-reference chunk 64 B (14 triangle IDs + `frame_generation`).
Bank 3 map at `0x0600_0000` (view 0) / `0x0640_0000` (view 1) / `0x0680_0000`
scratch — **present in `spec/memory_rules.md:322`.**

**Status:** `GEOM.PARAMBUF` in `design/blocks.yml:2161`, contract written,
REFERENCE_COMPLETE. **Sizing is blocked on a real 256-creature trace**, not on a
design — the docket's agreed sequence step 2. The analytic numbers already
computed (VDECODE ~494,000 vertices, PART.STATE 1 MiB/tick, POST.COMPOSITE
5 passes) **are not that trace.**

## PARTICLES — `LIVE, NUMERIC LAW LANDED`
*08-31 §2.1–2.5 · R3 · Phase 7*

128-bit record with R3's exact bit positions and Q formats. **Position is s18
relative to a population origin** on a 1/256 m grid, ±512 m — if a population
cannot stay inside the cube, **split it, never saturate silently**. Rebase only
between complete ticks, by the same exact offset on every live record, and
**capture the rebase**. Tick is exactly 60 Hz, semi-implicit, ordered.
Randomness is **stateless deterministic hashing**, never a mutable PRNG seed —
a seed's sequence changes when a particle is culled or batched differently.
Twelve recipe IDs are the closed vocabulary; **hardware never infers a force
from colour, size or speed.** Five collision responses selected explicitly by
species descriptor. FPGA collision sources: **live deformed terrain
height/normal + explicit analytic planes only**; creature/unit gameplay
collision stays HPS-authoritative. **Survivors always outrank children.**

**Status:** `zhao_part_record.sv`, `zhao_part_expand.sv`, `zref_particle.hpp`
all carry `PARTICLE_FORMAT_VERSION = 1` and the QFMT_VERSION 3 amendment.
`PART.LADDER` REFERENCE_COMPLETE.

## 2D PLANES AND HUD — `LIVE, BUILT`
*08-31 §3, §9 · R4*

**Two descriptors, ONE time-multiplexed restricted engine — never two copies,
never a second TMU.** R4 adds `role[1:0]`, `blend_mode[1:0]`, `opacity` unit8:
role 0 BACKDROP (beneath the world, **blend must be REPLACE**, no depth test/
write), role 1 ATMOSPHERE (post stage 4, ALPHA or ADD), **roles 2 and 3
refused**. **No arbitrary depth test and no depth write in v1** — water, lava or
any plane that must intersect geometry is triangles through the main renderer.
This is R4 *narrowing* 08-31 §3.1, whose "water/lava/bounded world-space depth
plane" list could have authorised hidden depth behaviour.
`TWOD.SPRITE` is HUD-only on the primary TMU: no private sampler, no text
rasterizer; the game authors layout and text in software; HUD draws **after**
distortion, bloom, grading, ink and flash.

**Status:** contract carries the roles (`design/contracts/TWOD.PLANE.md:36-52`);
both blocks REFERENCE_COMPLETE.

## POST — `LIVE, GATHER BUILT`
*08-31 §4, §9 · R5 · R6*

Frozen visible order (9 stages, ink **last** on the world image, HUD after).
**`POST.GATHER` collects during tile resolve and must never reread the completed
framebuffer to rediscover tags, and must never backpressure `RASTER.RESOLVE`** —
if a fit cannot meet that with the double bank, add a tile-summary FIFO rather
than stalling resolve. 16×16 tile → exactly 4×4 effect cells, two ping-pong
banks of 16 register cells, **no global M10K read-modify-write on the resolve
path**. Every tile writes all sixteen cells including zeros, so no giant reset
loop. Clamp separately at Duo view boundaries — **a displacement can never
sample the other player's view.**
`POST.COMPOSITE` is **one bounded line stream, not five full passes**: one
source pixel fetched into the line system **once**; nine complete source lines
in a ring; ~103,680 work items for Z60 against ~461,000 for full passes.
**Do not instantiate a generic 65,536-entry RGB565 remap in M10K** — 128 KiB
before shape, and it contradicts the resource ceiling.
`POST.ECHO` **DEFERRED, first on the cut list, keep no storage or datapath.**

**Status:** `POST.GATHER` REFERENCE_COMPLETE with the **corrected ≤30 M10K
ceiling** recorded in-contract. `POST.COMPOSITE` is in R-block D (blocked until
adopted) and unbuilt.

## MATERIALS / TEXJOIN — `LIVE, REPAIRS PARTIALLY LANDED`
*R8 · R9 · X2 · X3 · `Addendum`*

0–3 samples is a **capability, not a per-fragment guarantee**. Every material
asset carries a compiler-validated fallback chain; **HPS/The Measure picks one
recipe before sealing; hardware never decides what a layer means.** Default
terrain chain high → medium → base → Gouraud-only. Governor targets **≤80% of
product-clock sample capacity** using measured **post-Early-Z** requests.
Eight frozen recipes (`PASSTHRU` … `TERRAIN_DETAIL_MASK`), sample 0 owns alpha
and the palette index, status ORed over all required samples. **These are
Zhaozhou-native — do not label them Sacrifice-exact and stop waiting for a donor
law.** A malformed asset that reaches hardware raises a sticky frame fault and
repeats the previous frame — **never a plausible placeholder texel.**
Combiner is **its own registered II=1 pipeline**, not a big combinational case
on the retirement path.
Cel materials are an explicit exception to per-vertex fog: unfogged lighting →
toon bands → modulate texture → **fog** → post, so distant fog is not quantised
into hard bands (08-31 §5.3, FROZEN VISUAL LAW; docket D12 flags it as
contradicting the general law — that contradiction is *resolved by the ruling*).

**Status:** `MATERIAL_RECIPE_VERSION = 1` appears in `MATERIAL_ARCHITECTURE.md`
but **not yet as a code constant** — see Contradiction C3.
X3's TEXJOIN repairs **are landed**: `zhao_raster_texjoin_v2.sv` has `GENW = 8`
(was 2) and the `free_cnt_q` accept/retire race is fixed to a single net update
per clock (`if (acc && !ret) … else if (!acc && ret) …`, lines 522-523).
X2's two-lane PERSPUV **is landed** (`zhao_raster_perspuv_svc.sv`), refitted
62.67 → 99.14 MHz.

## THE TEXTURE-SURVIVOR ISLAND — `LIVE, AND IN TROUBLE`
*`REARCHITECTUREADVICE.md` 09-02 11:54 · X1–X9 · Phases 0–4*

Production topology, in order: `Early-Z survivor → TEXJOIN_ALLOC {slot,gen8} →
ONE RCP per fragment → 0–3 perspective-pair requests → 0–3 TMU requests → AUX
issued IMMEDIATELY AT ALLOCATION (concurrently) → token-indexed result writes →
MATERIAL.COMBINE II=1 → registered allocation-order retire → RASTER.FRAGMENT.`
Token = `{generation[7:0], slot[3:0], sample_index[1:0]}` in a 16-bit field.

**The non-negotiable rules** (`REARCHITECTUREADVICE.md`, "things the agent must
not do", plus the buildability brief's checklist):
* Latency may grow. **Exact arithmetic, output ordering and sustained throughput
  may not regress.**
* **External `src_id` is visible context, NEVER transaction identity.**
* **RAM read → fabric capture → calculation.** Never RAM → broad logic → commit
  in one stage. **No M10K output may launch a broad combinational cone.**
* **Ready is local.** A ready signal may not traverse more than one subsystem —
  no `TEXJOIN → TMU → cache → memory` ready chain.
* **Do not build a generic 4-way write-back texture cache.** The real thing is
  four independent direct-mapped **read-only** lanes with one blocking fill and
  same-line multicast. No PLRU, no dirty victims, **no MSHRs / hit-under-miss
  until traces prove the miss engine, not the hit path, is the limiter.**
* **Do not resurrect `OPTIMIZATION_TECHNIQUE=SPEED`** — measured harmful, lost
  3.01 MHz and added area. `HIGH PERFORMANCE EFFORT` + Balanced remain
  authoritative.
* **No false paths, no multicycle constraints on real synchronous behaviour, no
  seed fishing.**
* **Do not replace the old implementations** until the new ones pass the exact
  tests — keep them as executable specifications / oracles.
* **Do not redesign `RASTER.TOON`, `SURFACE.SHEET` or current
  `TERRAIN.NORMALS`** unless a *current composed fit* names them. TOON already
  delivers 405,515 fragments/frame against 320,000; Surface Sheet already has
  the right synchronous shape.
* **Do not reopen `EARLY-Z`/`TILESTORE`** because one seed dislikes one of them —
  only when the same logical cone appears in **≥2 of 3** full-composition seeds.
* **Do not push the flat core 99.5 → 110 MHz before integration.** The partial
  composition (96.87 avg / 99.50 peak) is **provisionally FROZEN**; there is no
  longer one dominant defect worth another blind local machining campaign.

**Nine X-items:** X1 rcp24 scheduler good, no rewrite justified · X2 PERSPUV two
lanes ✅ landed · X3 TEXJOIN eight defects, **do not integrate unchanged**
(gen8 ✅, free_cnt race ✅; the entry-scan → work-FIFO conversion, return skids,
held registered retirement and zero-sample guard still need verifying) ·
X4 TMU planner needs CLUT8/CLUT4 through resident palette + the 79-case suite on
the composed stream · X5 cache dispatcher — **do not add per-class pre-sorting**
unless HOL stalls exceed **2%** of island cycles · X6 palette `BEGIN/WRITE/END`
with CRC, **never reload a slot with the same generation** · X7 cache C0–C4
synchronous seam, **do not accept the fit if the RAMs become flops/MLABs** ·
X8 terrain residency prototype **must not be integrated** · X9 the failed block
fit was an **apparatus failure, not a timing result** — `design/fit_targets.yml`
✅ now exists (10 KB).

**Status: the island as built is 18,497 ALM / 28,143 reg / 10 M10K / 25 DSP —
2.5× its own redline and worse than the 15,749 / 25,123 / 11 prototype it
replaced on every axis except DSP.** That recovery work is
`reports/islandrearchitecture5.md` (09-03 08:04), owned by another lane.

## FIELD / EARTH — `LIVE, SECOND CAMPAIGN`
*`WordOfCaution` 08-25 · `PIPELINEINGHINTS` 08-25 · `REARCHITECTUREADVICE` §2 ·
`Addendum` · 08-31 §8*

* **`FIELD.SEQ.EARTH / WARP / FLOW / FORMATION / STAMP` are program profiles on
  ONE shared Field engine, not five datapaths** (08-31 §8). Author them as
  content requires.
* **Do not rewrite Field v3 arithmetic.** The unmeasured problem is production
  composition and physical realisation. Build one `zhao_field_earth_island`
  around the existing quad engine. **Do not instantiate the old dense
  evaluators.**
* Fit point **A = `CTX=32 LANES=4 OUTSTANDING=12 GATHERS=4 REGS=64 DIST_BANKS=4
  RING_UNITS=4`** (the actually-accepted config), **B = a deliberately reduced
  candidate**. Fitting the discredited `OUTSTANDING=16 / DIST_BANKS=8 /
  RING_UNITS=8` point as primary would measure a machine nobody intends to ship
  and make B look good against a straw man (`Addendum`). A fat worst case is a
  legitimate **third** point that must say so.
* A separate Field/Earth PLL domain is allowed **only** with real async FIFOs and
  ≥25% workload reserve. **Do not cut synchronous paths with timing exceptions
  and call it a second clock.**
* **`WordOfCaution` (08-25) is still live and still unmet.** Two mandatory
  deliverables: (1) a **timing path-family census** of ≥200 setup paths grouped
  by logical cone, with counts above 10/12/15/18 ns and runner-up families —
  because Wave 8 proved removing the #1 path can buy almost nothing when an
  equal cone waits behind it; (2) an **end-to-end Earth60 capacity budget**:
  affected patches × 1,089 vertices × overlapping fields × instructions ×
  measured clocks/instruction, **plus TERRAIN.PATCH's own 1+n compose clocks per
  vertex**, plus queue/storage bandwidth and 20% reserve, across 1/2/3/4 Field
  cores. **Pick the physical Field-core count only after this workload exists.**
* **LIVE-TO-BAKE LAW:** visible and gameplay-relevant live deformation updates at
  **60 Hz**. `TERRAIN.BAKE` may run behind it but **must never make visible
  deformation advance in chunks.** The live field stays authoritative until the
  exact persistent bake commits, then retires without a height, collision or
  navigation discontinuity. The present analytic bake can consume ~79% of a frame
  for 64 page-covering records — **not sign-off quality**; choose and measure
  either the pipelined-divider or fetched-stencil route and bound the maximum
  backlog before terrain sign-off.
* **`MAX_OP_CYCLES = 80` is a formal/liveness constant, not a product law.**
  Raise and re-prove it if more pipeline stages are needed. **Never sacrifice the
  100 MHz clock merely to preserve 80.**
* **`PIPELINEINGHINTS` is EXPLORATION, not law.** Its own closing words: *"I
  wouldn't prescribe the whole design as law."* It asks the agent to
  *investigate* a barrel/multithreaded Field (8–16 vertex contexts, one
  instruction in flight each), a `reg[1:0]`-banked register file (4 banks × 3
  replicas = 12 memories serving all seven operands in one cycle), dropping the
  speculative three-product issue once II=1 is the goal, characterising
  `MUL_LANES = 1/2/3` (3/6/9 DSP against the old Field's **79**), and a
  `TERRAIN.PATCH` **ordered pipeline** — one stage per field, different vertices
  in different stages — turning 1/(1+n) vertex/clock into **1 vertex/clock after
  fill** while preserving exact per-vertex saturation order.
  **Its single most important instruction: report LATENCY and INITIATION
  INTERVAL as two completely different numbers.** *"NORMALIZE takes 68 clocks"*
  says nothing about sustaining Earth60. Build the opcode → resource-token table
  and compute utilisation, not average latency.
  **Suggested first deliverable is explicitly NOT RTL** — it is the pipeline/
  resource model and steady-state throughput calculation.
* **Overload must never become temporal stutter.** Negotiate spatial sample
  density, distant-patch LOD, cosmetic sinks and bake latency **before** ever
  negotiating live terrain update cadence. Near/gameplay terrain stays 60 Hz.
  *"Temporal smoothness is the contract."*

## THE 8 KM TERRAIN WORLD — `LIVE, T1–T12`
*Buildability brief, 09-02 21:44 · Phase 6*

* **T1 key** `{resource_epoch, island_id, patch_ix, patch_iz}`. Patch pitch is an
  island-table property validated against the page header, **not part of the
  key**. **No global coordinate projection is identity; two islands may legally
  overlap in local patch coordinates.**
* **T2** exact bank-2 map at `0x0400_0000` (✅ in `spec/memory_rules.md:294`).
  **No separate permanent E/F/H pools** — those layers live inside the
  21,376-byte page. Mip pools are derived caches, not canonical assets. Every
  region **DENY-BY-DEFAULT** with **state-aware** permissions.
* **T3** add `ZHAO_CLIENT_TERRAIN_BUILD = 6`, best-effort/background (✅ in
  `spec/memory_rules.md:336`). **It does not join guaranteed round-robin merely
  because a page is late** — a late page means a declared proxy and recorded
  pressure, never stolen scanout/render service. **Client 5 stays unspent.**
  Re-run MEM.ARBITER liveness and guard proofs after adding client 6.
* **T4** HPS owns canonical **B and D** — **do not write them back on eviction**.
  **Layer F (8 KiB surface sheet) has no HPS mirror**, so `dirty_F` is tracked
  separately and its writeback is a **barrier**: wait for journal ACK before the
  slot may enter LOADING. A single generic dirty bit is insufficient; keep
  `modified_BD` (counter only), `dirty_F` (writeback barrier), `mips_stale`.
* **T5** two versioned commands, opcodes confirmed by the ZIDL generator before
  commit. **One `SubmitTerrainSet` covers the whole required+prefetch set** — do
  not emit one DrawProcedural per patch, do not overload an existing terrain
  field command. **The sealed list is capture data; replay does not rerun the HPS
  visibility walk.**
* **T6** 256 composed slots are for **live-composed** patches only. Static/baked
  visible pages render from resident page layers and consume no slot.
  **Hardware never silently displays stale base terrain in place of a required
  live field.**
* **T7** working set = both views' visible sets ∪ one-patch Moore ring ∪ **30
  frames (0.5 s)** of camera prediction ∪ explicit gameplay patches. **Union the
  views before deduplication.** **A half-loaded or CRC-failed page is never
  rendered**; an ordinary miss uses the declared proxy — **do not freeze the old
  camera frame for ordinary traversal.**
* **T8** the exact decimation law, **no averaging** — an averaging mip cannot
  promise that adjacent patches agree exactly on a shared edge, and the seam
  cracks. **HPS does not implement a second mip law.**
* **T9/T10** **reject direct-map as production**; 256 sets × 4 ways; CRC-8/ATM
  set index; full key in every way; replacement state updates on **canonical
  claim acceptance, never on asynchronous fill completion**; synchronous metadata
  RAM one bank per way; **1,024-entry init sweep with ready low on reset — do not
  async-reset an inferred RAM**; **no software "two islands must not collide"
  restriction exists.**
* **T11 full console reset is NOT level unload.** `END_FLUSH` drains, waits for
  pins to reach zero, writes back every `dirty_F`, waits for journal ACKs, emits
  teardown ACK. `ABORT` is legal only for reset/fault recovery and **must record
  that it discarded state.**
* **T12 `SW.STREAM` is the real remaining stub.** Deadline-driven software
  backpressure: it may defer PREFETCH records but **may not mutate a sealed
  REQUIRED list**; if staging cannot meet the deadline it selects
  proxy/fallback **before sealing**; it must **never expose a half-built page
  list to CMD.DMA.**

**Status:** `zhao_terrain_residency_v2.sv` built beside the v1 direct-map file
(both present) with `SETS=256 WAYS=4` and the CRC-8/ATM law in its header ✅.
`zhao_terrain_mipgen.sv` implements the exact nested decimation ✅.
`SW.STREAM.md` filled (10.6 KB) ✅. Guard map and client 6 in
`spec/memory_rules.md` ✅.

## GEOMETRY CONTRACTS — `DECIDED`
*08-31 #5, #6, §6.1–6.5 · R11*

* **`GEOM.MESHFETCH`** — write and build. Versioned 64-byte-aligned descriptor.
  **Cull only when outside EVERY active camera.** Bound centre meshlet-local;
  radius uses **maximum absolute instance scale**. Keep descriptor fetch separate
  from VDECODE.
* **`GEOM.VDECODE`** — **land RAW/CANONICAL format 0 FIRST. Do not block the
  geometry path on perfect compression.** Formats 1 (packed rigid) and 2
  (two-weight skinned) are additive, gated on an asset bake-off over Zixxtrixx +
  **ten structurally different creatures**, comparing silhouette error, cel-band
  flips, normal angular error, bytes/vertex and decoder ALM/DSP/Fmax.
* **`GEOM.LOOM`** — composes transforms from an ARM-supplied **parent-before-child
  topologically sorted stream**. **No recursion, no cycle detection, no matrix
  inversion, no gameplay event generation, no autonomous gait or formation
  logic.** Gait/formation are **input values** from Form/Field programs.
  Keep-world reparenting is computed on ARM between frames.
  Body-patch giant stays prototype-before-silicon.
* **`FORGE.PRIM`** — one bounded generator, **six families and no seventh**
  (✅ `zhao_forge_prim.sv:1`). Subdivision selected **before** acceptance;
  **never emit a partial primitive.** Shard burst is a **particle population**;
  chain/spline wall/low cone are uses of existing families.
* **`GEOM.WARP` DEFERRED**, cut-order 5, ARM lowering remains valid — **must not
  block conventional geometry or creature completion.**
* **`INPUT.SNAC` DEFERRED/optional** — MiSTer path satisfies the contract; if
  ever built it must emit the same canonical `PadFrame` and **may not create a
  second input semantics.**
* **`MEASURE.HISTOGRAM` — the refusal STANDS and IS the implementation.** Do not
  invent an error metric, bucket boundaries, cutoff rule and governor interface
  merely to make the ledger look complete. Measure v1 = ARM predicts thresholds
  from prior counters, FPGA does local traversal, token guard rejects
  low-priority refinement near the limit.

**Status:** all three contracts written; `GEOM.VDECODE` REFERENCE_COMPLETE;
`FORGE.PRIM` UNIT_VERIFIED. WARP/SNAC/HISTOGRAM closed by decision (`699daf3`).

## BUILD ORDER — `LIVE, UNCHANGED BY EVERY DOCUMENT`
*08-31 §12 · 08-31 COMPLETE closing line · buildability Phases 0–7 · docket*

Wave A close the conventional renderer → Wave B conventional geometry assets →
Wave C spectacle. The 08-31 authority states in as many words that answering 28
questions **does not redirect the current work**, and §8 adds: *"Do not alter
this ruling set because a predicted timing offender has a persuasive name;
TimeQuest paths decide the next timing intervention."*
The buildability brief refines it to Phase 0 (repair the fit apparatus) → 1
exploratory leaf fits → 2 required texture repairs → 3 composition → 4 physical
gates → 5 PARAMBUF → 6 terrain world → 7 particles and post, and adds
**"do not add further leaf blocks until the texture island is composed and
fit."**
Board truth and SDRAM bandwidth run alongside: **no absolute bandwidth or
resource claim before physical measurement.**

## PROCESS AND EVIDENCE — `LIVE`

* **Every fit archives** exact commit/source hashes, Quartus version and settings
  readback, seed, WNS/TNS + endpoint count, worst 100 paths, path owner and
  startpoint kind, resources by hierarchy, compiled-source and
  elaborated-instance manifests, queue high-water marks,
  accepted/issued/returned/retired counters, all stall reasons, and a functional
  signature or rendered CRC.
* **A structural edit is accepted only when** the targeted path disappears
  structurally, exact outputs are unchanged, throughput does not silently fall,
  the same-seed comparison is favourable, and **≥3 seeds support any
  architecture-level conclusion.**
* **One structural hypothesis per commit and per fit.** Private worktree and
  private build/fit directories — never share a Quartus workspace with another
  lane. **Commit every independently green result immediately.**
* **Mutation checklist per new block:** correctness and II tested *separately*;
  simultaneous push+pop; reset/stall while full and while empty; **mutate ready
  coupling so the test demonstrably catches serialization**; mutate generation
  compare, one enum encoding, one rounding constant, one overflow boundary;
  run twice for deterministic equality.
* **Per memory block:** prove expected M10K inference **from Quartus, not from
  comments**; archive startpoint kinds; no M10K output launching a broad
  combinational cone; test read-during-write explicitly or architect it away;
  derive widths from the parameter or assert the fixed value;
  **count events with ONE net counter update per clock** (this is the exact class
  of bug X3 found in `free_cnt_q`).

---

# CONTRADICTIONS FOUND

## C1 — depth is fully specified, proved and oracle'd, and **the hardware still does not produce it**
`grep` over `fpga/rtl/geometry/zhao_geom_project.sv` finds **no `invw24` and no
`depth_profile`** — and nothing under `fpga/rtl/geometry/` mentions `invw24` at
all. `DEPTH_PROFILE_NEXT_STEPS.md` steps **5** (*"GEOM.PROJECT carries the
attribute packet and emits `invw24`… the last piece of the renderer's step 6, and
**the only thing that was ever actually blocked**"*) and **6** (*"the profile is
captured"*) are open. The docket's DONE table lists D10 steps 1, 2, 3 and 4 and
is silent on 5 and 6, so the docket is **not wrong, but it reads as closed**.
R1's own words — *"remaining work is mechanical… wire GEOM.PROJECT"* — are the
correct framing. **This is the smallest high-value piece of unbuilt hardware in
the entire ruling set.**

## C2 — `QFMT_VERSION` disagrees with itself across four generated files
R3: *"bump `QFMT_VERSION` 2 → 3 on commit."*

| file | value |
|---|---|
| `reference/include/zref/generated/zref_tables.hpp:9` | **3** |
| `compiler/src/generated/tables.ts:4` | **3** |
| `fpga/rtl/generated/zhao_abi_pkg.sv:19` | **2** |
| `runtime/include/zhao_abi.h:18` | **2** |
| `compiler/src/generated/abi.ts:12` | **2** |

Two generators emit the same named constant from different sources and one of
them never got the R3 bump. `QFMT_VERSION` is **capture-visible numeric law**,
i.e. Class C — a mismatch between the RTL package's idea of the format version
and the reference tables' is exactly the kind of silent skew the version exists
to prevent. **Worth one small commit; not a design question.**

## C3 — `MATERIAL_RECIPE_VERSION = 1` exists only in prose
R9 says *"Add `MATERIAL_RECIPE_VERSION = 1`."* It appears in
`reports/MATERIAL_ARCHITECTURE.md:243` and in the ruling itself, and **in no
header, package or generated table.** The eight recipes are frozen law; the
version constant that makes them versionable is not yet a constant.

## C4 — terrain RTL landed after a standing REJECT on adding terrain RTL
`islandrearchitecture5.md` (09-03 08:04, another lane's file, quoted from the
docket): *"REJECT: adding terrain/Field RTL faster than the texture fit can be
closed."* `fpga/rtl/terrain/zhao_terrain_normalmap.sv` (324 lines) was **created
at 09-03 18:13** in `62467567`, ten hours later, and its own header cites a
separate same-day owner ruling: *"we make it and see how bad it is… Normal maps
would be a huge gain though."*
Two owner instructions from the same day point opposite ways. **I am not
resolving this** — but note the docket's own D20 entry states the gate before
RTL is *"the amended oracle goes in the zref renderer and **the owner looks at
the island under a moving sun first**"*, and the RTL exists. Under CLAUDE.md's
art law that gate is not optional. **Flag to the owner rather than deciding it.**

## C5 — the owner's own ALM subtotal is now known to be optimistic
`WeNeedSomeMeasurements.md` (09-03 14:58) computes `13,146 + ~16,995 ≈ 30,141`
ALM (72%) and *"roughly 7,578 ALMs before violating the reserve rule."*
The measured island is **18,497**, so the real subtotal is **31,643** and the
remaining margin under the 37,719 line is **~6,076 ALM**, not 7,578 — before
geometry, terrain, Field, particles, post and all integration glue. The same
document's DSP arithmetic is unaffected and still comfortable (41/112 = 36.6%),
and it **explicitly blesses the PERSPUV 3 → 6 DSP doubling**, which resolves any
apparent tension between X2's two-lane ruling and a DSP-frugality reading.

## C6 — minor: in-source ruling citations are drifting
`zhao_raster_perspuv_svc.sv:52` attributes the two-lane requirement to
*"ruling R7, 2026-09-03"*; it is **X2**, and R7 is `GEOM.PARAMBUF`.
`zhao_raster_texjoin_v2.sv:59` attributes the 8-bit generation to *"Ruled X5"*;
it is **X3** item 2 (X5 is the cache-response dispatcher). The engineering is
right in both cases and only the citation is wrong — but `reports/`
already contains a `PHANTOM-CITATIONS-AUDIT.md`, so this pattern is known and
recurring.

---

# STILL OPEN, AND WHO OWNS IT

| item | owner | note |
|---|---|---|
| A **fourth depth profile** | **Fabian (Class C)** | reserved and refused; needs new evidence, a new proof and an explicit ABI ruling. **Must not be smuggled in as custom numbers.** |
| Whether the particle stretch tier (65,536) is taken | **evidence, then Class B** | gated on *physical board bandwidth*, which needs the board |
| `MEASURE.HISTOGRAM` v2 | **refused; revisit only after real game traces prove v1 inadequate** | needs a named producer + Q format, generated bucket edges, exact budget/cutoff with hysteresis, versioned governor input, mutation-tested reference |
| Compressed vertex formats 1 and 2 | **Class B bake-off** | Zixxtrixx + ten structurally different creatures |
| Field core count | **blocked on the Earth60 workload** (`WordOfCaution`) | *"Pick the physical Field-core count only after this workload exists."* |
| The bake route (pipelined divider vs fetched stencil) | **must be chosen and measured before terrain sign-off** | current analytic bake ≈79% of a frame for 64 records |
| `GEOM.PARAMBUF` sizing | **blocked on a real 256-creature trace, not on a design** | the analytic numbers are not that trace |
| Cache pre-sorting (X5) | **gated on a 2% HOL-stall threshold** | keep the simpler block until traces say otherwise |
| PERSPUV single-lane fallback | **allowed only if a real material trace proves avg demand ≤0.5 pair/clock AND the 3-sample tier shares recovered coordinates by contract** | currently two lanes, correctly |
| Board-gated blocks | **the board** | `SYS.PLL`, `SYS.RESET`, `SYS.CDC`, `MEM.SDRAM`, `SW.TOOLS.BOARDPROBE`. *"Nothing waits on hardware" is too broad.* |
| The five contradictions above | C1–C3 are small commits; **C4 is Fabian's** | |

---

# ALREADY DONE OR OBSOLETE — do not re-do

* **Every one of `Addendum`'s three corrections is already merged** into
  `REARCHITECTUREADVICE.md` (`3bc30824`, 09-02 12:56): the `sample_count 0..3`
  TEXJOIN state, the 105/110/115/100 MHz hierarchy, and the Field configuration A.
  The `Addendum` is now a rationale document, not an outstanding instruction.
* **`OPEN-SPEC-DEPTH-QUANTISATION.md` is superseded** and carries its own banner
  (09-02 22:51). Its body is retained only as the reasoning the ruled numbers
  satisfy. **A reader who treats it as an open question re-asks something already
  answered — which is precisely the failure the banner exists to prevent.**
* **X2 (two-lane PERSPUV) — landed**, 62.67 → 99.14 MHz.
* **X3 items 2 and 3 (8-bit generation; the `free_cnt_q` same-cycle
  accept+retire race) — landed.**
* **X9 (`design/fit_targets.yml`) — landed** (10 KB).
* **T12 (`SW.STREAM` contract) — filled** (10.6 KB; the one remaining `TODO`
  string is a historical reference to the stub, not a live stub).
* **T2/T3 (guard map, client 6) — landed** in `spec/memory_rules.md`.
* **T8 (mip law), T9/T10 (`residency_v2` set-associative), R4 (plane roles),
  R5 (≤30 M10K correction), R3 (particle format), R11 (Forge six families) —
  all landed.**
* **`ShellFixes.md`'s three items are DONE**, and two of them measured *better*
  than the document asked (docket correction, `c295ff5d` 09-01 18:36). Only the
  record-framer streaming rewrite remains, and whether it is needed at all is
  unmeasurable until the renderer stops dominating.
* **All 28 owner questions answered** (`f5d1653`).
* The docket's **"zero of 92 buildable"** and **"seven stub contracts"** are both
  stale; `tools/ledger/remaining.py` now derives the real list.

---

## ONE CLOSING WARNING FROM THE DOCUMENTS THEMSELVES

The buildability brief's central conclusion, verbatim:

> Zhaozhou is not blocked by dozens of unanswered game-design questions. It is
> blocked by a much smaller set of concrete numeric, storage, transaction and
> physical-composition problems — and several of today's "finished leaves" still
> need one more architecture pass before they are allowed into the machine.

And the buildability brief's own framing of maturity, which the 18,497-ALM island
has now proved the hard way:

> **"A source file exists and a directed test passes" is not the same maturity as
> production-buildable hardware.**
