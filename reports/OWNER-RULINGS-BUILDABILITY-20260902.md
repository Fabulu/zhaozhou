# ZHAOZHOU BUILDABILITY RULINGS AND ARCHITECTURE BRIEF

> **Received from the owner 2026-09-02, relayed from the reviewer.** Saved here
> and NOT in a run folder: CLAUDE.md, *"a run folder is the wrong home for
> anything durable — every pass creates a new one."* This governs the whole
> machine, so it sits beside the other senior reports.
>
> **Status: PROPOSED OWNER RULINGS.** Binding when the owner commits or
> otherwise adopts them. Until then they are the recommended architecture.

Date: 2026-09-02
Primary source snapshot audited: `42c74a5`
Latest main observed while closing: `28480b3`

---

## PURPOSE

Answers the outstanding questions in `reports/OWNER-SPEC-QUESTIONS.html`,
corrects stale premises in that audit, folds in the ten terrain-world
questions, and rules on defects in the RTL that landed 2026-09-02.

Five states are distinguished:

1. **DELIBERATELY ABSENT** — deferred or refused. Not a defect.
2. **BOARD-GATED** — authorable and simulable, not ratifiable without a board.
3. **BUILDABLE NOW** — law sufficient; implementation/testing/fitting remain.
4. **BLOCKED BY A MISSING LAW OR ABI** — this brief supplies the ruling.
5. **PROTOTYPE RTL, NOT PRODUCTION-BUILDABLE** — a known defect must be
   repaired before integration.

## EXECUTIVE VERDICT

The old claim that seven hardware contracts contain fifteen accidental TODO
sections is mostly stale. **But the current audit is not safe to use
unchanged.** Corrections:

* **Depth quantisation is already ruled, generated and proved.** Do not choose
  `wmin`/`wmax`/`scale` again.
* **276,480 already has one binding meaning:** conservative PRE-Early-Z covered
  fragments in Z60 at 3.0x overdraw. Not a sample count.
* **Particle behaviour is substantially frozen.** The remaining blocker is the
  numeric interpretation of the 128-bit record.
* **TWOD.SPRITE is already HUD-only** on the primary TMU; TWOD.PLANE is already
  one restricted time-multiplexed engine; compositor order is frozen.
* **The "choose a kMesh budget and grow the on-chip arena" path contradicts the
  binding 2026-08-31 ruling.** The production solution is an external
  local-SDRAM `GEOM.PARAMBUF`, which has no block, contract or ledger entry.
* **"Zero contracts are unwritten by accident" is FALSE.** `SW.STREAM` still has
  TODO sections and is on the critical path of the 8 km world.
* **GEOM.VDECODE format 0 (RAW/CANONICAL) is buildable now.**
* **The new texture leaves have directed evidence but no physical closure**, and
  several contain concrete defects.
* **The PERSPUV ~50% headroom claim is only true for ONE UV pair per fragment.**

---

## CORRECTED BUILDABILITY LEDGER

### A. DELIBERATELY ABSENT — do not revive by default
`INPUT.SNAC` (optional; MiSTer path suffices) · `GEOM.WARP` (ARM lowering
remains valid) · `POST.ECHO` · `MEASURE.HISTOGRAM` (documented refusal) ·
packed `GEOM.VDECODE` formats 1 and 2 (bake-off gated).

### B. BOARD-GATED — authorable, not ratifiable
`SYS.PLL` · `SYS.RESET` · `SYS.CDC` · `MEM.SDRAM` · `SW.TOOLS.BOARDPROBE`.

*The audit's "nothing waits on hardware" is too broad.* PLL frequencies, reset
release timing, CDC topology, SDRAM timing, sustained bandwidth and calibration
are board truth. `SW.TOOLS.REPORT` looks stale as a blocker — amend that row,
but do not infer BOARDPROBE or SDRAM are solved.

### C. BUILDABLE NOW
`GEOM.PROJECT` depth carry · `GEOM.MESHFETCH` · `GEOM.VDECODE` format 0 ·
`GEOM.LOOM` (transform composition only) · `FORGE.PRIM` (six v1 families only)
· `PART.STATE` (after qformats amendment) · `PART.LADDER` · `TWOD.SPRITE` ·
texture leaf prototypes as fit candidates.

`FORGE.PRIM` v1 families: ribbon, radial fan/ring, tube, radial shell,
billboard sheet, terrain cliff/skirt. **Remove shard burst, chain, cone and
spline-wall** unless separately re-ratified — shard burst is a particle
population.

### D. BLOCKED UNTIL THESE RULINGS ARE ADOPTED
`PART.UPDATE`/`COLLIDE`/`SPAWN` · `TWOD.PLANE` · `POST.GATHER` ·
`POST.COMPOSITE` · material combiner/TEXJOIN · **`GEOM.PARAMBUF` (no entry
exists)** · terrain world layer · **`SW.STREAM` (real stub)**.

---

## R1 — DEPTH QUANTISATION IS CLOSED

    profile 0 WORLD_LONG      wmin 1.0 m     wmax 16,384 m  SCALE 2^40  d(wmax) 1024
    profile 1 WORLD_STANDARD  wmin 0.5 m     wmax  8,192 m  SCALE 2^39  d(wmax) 1024
    profile 2 CLOSE           wmin 0.25 m    wmax  2,048 m  SCALE 2^38  d(wmax) 2048
    profile 3                 reserved and refused until re-proved

For all three: `d(wmin) = 0xFFFFFF` exactly; depth monotonic non-increasing;
far value non-zero; **`wmax` is a depth clamp, NOT a far clipping plane**;
scale generated from the reciprocal law, never hand-entered.
`SetView.flags[1:0]` selects. Zero remains WORLD_LONG.

Remaining work is mechanical: emit constants from fixgen; use `zref::depth_of`
as oracle; carry profile through capture/replay; wire GEOM.PROJECT; run the
endpoint and monotonic proofs. **Mark `OPEN-SPEC-DEPTH-QUANTISATION.md`
superseded.**

## R2 — 276,480 HAS ONE MEANING

`276,480 = 384 x 240 x 3.0` conservative PRE-Early-Z covered fragments.
Not a sample count, post-Z survivor count, divide count, AUX count or measured
frame.

Separate units: `pre_z_covered_fragments`, `post_z_survivors`,
`material_samples_issued`, `perspective_fragments`, `perspective_pairs`,
`aux_requests`, `filtered_channel_jobs`, `cache_accesses`, `cache_line_fills`.

Canonical pre-Z cross-mode design target: **320,000** covered fragments/frame.
Design deadline **1,333,333 clocks**; **1,666,667 is the fault boundary**.

**`tools/render/sample_budget.cpp` is a conservative upper envelope only.**
Rename its headings from "known samples" to "pre-Z no-rejection sample
envelope". It must not be used as a measured workload nor as proof that exactly
238,733 clocks of real reserve remain.

## R3 — PARTICLE128 V1 NUMERIC LAW

`PARTICLE_FORMAT_VERSION = 1`; bump `QFMT_VERSION` 2 -> 3 on commit.

    bits   0..17   position X        18..35  position Y     36..53  position Z
          54..64   velocity X        65..75  velocity Y     76..86  velocity Z
          87..96   age               97..103 species       104..109 size
         110..115  spin             116..119 flags         120..127 variation

* **position s18** — S 9.8 m relative to population origin; LSB 1/256 m;
  -512.000 .. +511.99609375 m
* **velocity s11** — S 2.8 m/tick; LSB 1/256; -4.000 .. +3.99609375 m/tick
  (~-240 .. +239.77 m/s at 60 Hz)
* **age u10** — whole 60 Hz ticks 0..1023; lifetime in species descriptor,
  1..1023; `lifetime == 0` refuses the descriptor
* **species u7** — table index 0..127
* **size u6** — U 2.4 relative radius multiplier;
  `radius = base_radius_fx16 * size/16`, one final round-half-up; world scale,
  never camera-space pixels
* **spin u6** — U 0.6 turns; angle16 = `spin << 10`; species carry signed spin
  rate in angle16/tick; stored phase wraps mod 64
* **flags** — bit0 STUCK, bit1 COLLIDED_THIS_TICK, bit2 BORN_THIS_TICK,
  bit3 RESERVED (zero in, preserved zero)
* **variation u8** — stateless deterministic code from frozen hash inputs, NOT
  a mutable PRNG state

**Population descriptor:** population_id; origin x/y/z fx16 on a 1/256-m grid;
active_count/capacity; species-table handle; tick index; capture source id.
World position = origin + local. HPS may rebase only between complete ticks by
subtracting the same exact 1/256-m offset from every live record and adding it
to the origin; the rebase is captured. If a population cannot stay inside the
±512 m cube, **split it** — do not silently saturate.

**Tick is exactly 60 Hz. Semi-implicit and ordered:** validate → derive
variation → evaluate frozen recipe → accumulate in wide lanes → round once into
S2.8 and saturate/count → `position_next = sat_s18(position + velocity_next)` →
increment age → emit bounded events → collision response → survivors compact,
then children append.

Twelve recipe IDs remain the vocabulary. **Hardware does not infer forces from
colour, size or speed.**

**FPGA v1 collision sources:** live deformed terrain height/normal; explicit
analytic planes. Creature/unit gameplay collision remains HPS-authoritative.
Responses: IGNORE, DIE, STICK, SLIDE, BOUNCE, selected by species.

**Ceiling behaviour (already closed):** survivors are never evicted for
children; earlier parents outrank later; later child groups refused
deterministically; no same-tick recursive spawn.

## R4 — THE TWO 2D PLANE SLOTS

`TWOD.SPRITE` stays HUD-only; no new ruling.

`TWOD.PLANE` is one time-multiplexed restricted sampler serving at most two
descriptors. Add `role[1:0]`, `blend_mode[1:0]`, `opacity` unit8.

* **0 BACKDROP** — beneath the resolved 3D world; sky/background only; blend
  must be REPLACE; no depth test, no depth write
* **1 ATMOSPHERE** — post stage 4, over the displaced world, before
  bloom/grade; ALPHA or ADD; frozen unit8 opacity law
* **2, 3** — reserved; descriptor refused

**No arbitrary depth test and no depth write in v1.** Water, lava, landscape or
any "depth plane" that must intersect ordinary geometry is triangles through
the main renderer. Amend the purpose line so typical uses do not authorise
hidden depth behaviour.

Same role in both slots: slot 0 composites first. Different roles: BACKDROP in
world setup, ATMOSPHERE in the post stream. All existing restrictions remain
(CLUT8/RGB565, nearest only, affine/line scroll, repeat/clamp, view mask, one
engine, no private general TMU).

## R5 — POST.GATHER STORAGE AND PRECISION

The contract's storage arithmetic is internally contradictory. Two levels:

**Tile-local accumulation** — 16x16 pixel tile maps to exactly 4x4 effect
cells. Two ping-pong banks of 16 register cells. Per cell: `glow_r/g/b` u16
saturating; `displacement_x/y` signed 8.8 in s16 wide saturating; `ink` 1-bit
OR. A resolved fragment updates at most one cell per plane. **No global M10K
read-modify-write on the resolve path.**

**Global effect cell (33 bits):** glow RGB565 16b; displacement X signed i8;
displacement Y signed i8; exterior ink 1b.

At tile flush: glow rounds/clamps once into RGB565; displacement rounds once to
integer pixels; X clamps [-8,+8]; Y clamps [-4,+4]; ink copied.

Duo has 128x60 = 7,680 cells. At the natural 256x40 M10K shape that is **thirty
M10Ks** — change the ceiling from <=10 to **<=30 M10K**. Compact total is
31,680 bytes; the physical count is thirty because width/depth shape matters.

Every tile writes all sixteen cells including zeros, so the frame overwrites the
active plane without a giant reset loop. **POST.GATHER must never backpressure
RASTER.RESOLVE**; if a fit cannot meet that with the double bank, add a small
tile-summary FIFO rather than stalling resolve. Clamp separately at Duo view
boundaries — a displacement can never sample the other player's view.

## R6 — POST.COMPOSITE IS A STREAM, NOT FIVE FULL PASSES

Keep the frozen visible order; change the implementation.

**A. Optional quarter-res glow prep** — separable blur over the compact glow
plane; one horizontal and one vertical quarter-res pass; no full-resolution
reread.

**B. One full-resolution streaming pass** — (1) BACKDROP + unresolved world;
(2) read compact displacement cell; (3) sample world colour through the
displaced coordinate; (4) sample blurred glow and ink through that same
coordinate at quarter res; (5) ATMOSPHERE/haze; (6) bloom/glow; (7) global
colour transform; (8) flash/tint; (9) exterior ink; (10) HUD.

**One source framebuffer pixel is fetched into the line system once.** Do not
resample the completed framebuffer separately for refraction, shockwave, haze,
bloom and ink.

**Line architecture:** bound ±8 X, ±4 Y; keep nine complete source lines in a
ring; horizontal ±8 from those lines; delay in-place writes until the old line
can no longer be sampled; clamp inside each Duo view before addressing; small
output-line delay queue if needed.

Z60: 92,160 pixel clocks + 11,520 glow cells ≈ **103,680 work items** rather
than ~461,000 full-pass clocks. Duo: 98,304 + 15,360 = 113,664.

**Global colour transform v1:** indexed palette cycling stays a TMU resident-
palette operation, not reconstructed after RGB resolve. Post transform =
generated per-channel curves R[32], G[64], B[32], then one generated signed
3x3 Q2.14 matrix plus signed colour8 bias; one wide sum and one round-half-up
per channel; saturate to colour8/RGB565. Nine products/pixel fits the 12-DSP
post ceiling at one pixel/clock.

**Do not instantiate a generic 65,536-entry RGB565 remap in M10K** — 128 KiB
before shape, and it contradicts the resource ceiling. Curve+matrix fusion only
if fixgen produces an exact equivalent and the fit is cheaper; unfused is
default.

POST.COMPOSITE owns an exclusive framebuffer read/write lease after resolve and
before publication. HUD follows post and never forces another read.

## R7 — GEOM.PARAMBUF AND THE kMESH GUARANTEE

**The production binner does not grow a frame-sized on-chip triangle arena.**
The 2026-08-31 ruling stands: projected unique vertices, compact triangle
descriptors and tile-reference chunks live in local SDRAM; ENGINE1 owns the
render-geometry region; on-chip is limited to tile directory, active chunk
tails, prefetch FIFOs, a small projected-vertex cache and an opportunistic
expanded-context cache; one tile clears and resolves exactly once; no
framebuffer readback; no arbitrary tail truncation.

*The LOD measurement is useful evidence that ordinary armies are cheaper than
the all-kMesh assumption, but it does not revoke this architecture.*

**Owner content guarantee:** 32 ordinary creatures at kMesh machine-wide; in
Duo at least 16 per active view; a single-view tier may use all 32. More may be
admitted by measured tile-reference cost but are not guaranteed.

**A giant is a separate quota** — reserve at least 32,768 tile references for
one giant before ordinary kMesh allocation. If the giant consumes the view's
budget, ordinary creatures demote by declared LOD priority; **the giant is
never silently truncated.**

Register the block: `GEOM.PARAMBUF`, gpu clock, ENGINE1, maturity SPECIFIED,
reference `zref::GeomParamBuffer`, tests directed/randomized/overflow/
frame-generation/stale-handle, physical target the common renderer clock with
registered SDRAM seams.

**External records:**

    ProjectedVertex, 24 B: screen_x s32 (legal s21), screen_y s32 (legal s21),
                           invw24 + status byte, u_over_w s32, v_over_w s32,
                           rgba8 u32
    TriangleDescriptor, 16 B: vertex_id[3] u16, material_id u16,
                              raster_state u32, source_id u32
    Tile-reference chunk, 64 B: next_chunk u32, count u16,
                                frame_generation u16, fourteen triangle IDs u32

**Per-view minimum acceptance tier:** 32,768 projected vertices; 8,192
triangles; 65,536 tile references.
**Preferred tier inside a 4 MiB arena:** 65,536; 16,384; 131,072.

**Local SDRAM bank 3 initial guard map:**

    0x0600_0000 .. 0x063F_FFFF   PARAMBUF view 0, 4 MiB
    0x0640_0000 .. 0x067F_FFFF   PARAMBUF view 1, 4 MiB
    0x0680_0000 .. 0x069F_FFFF   shared prefetch/chunk scratch, 2 MiB
    0x06A0_0000 .. 0x07FF_FFFF   reserved/unmapped pending evidence

The Measure seals quotas before the frame. On hard arena overflow: drain the
frame, repeat the prior complete frame, report source IDs. **Never publish a
frame with an arbitrary missing tail.**

## R8 — THREE-SAMPLE MATERIALS ARE A CAPABILITY, NOT A GUARANTEE

v1 supports 0..3 samples. Three-sample terrain is a real shipping capability
and must be tested, fitted and retained. **It is not guaranteed on every
terrain fragment of every stress frame.**

Every material asset carries a compiler-validated fallback chain; HPS/The
Measure selects one recipe before sealing. **Hardware does not decide what a
layer means.**

Default terrain chain: high = base + detail + light/material; medium = base +
the higher-priority secondary; base = base only with Gouraud; minimum =
Gouraud only, no TMU request.

Optional layers drop in asset-declared order. Base is mandatory for a textured
tier. A semantically required mask cannot be dropped — such a material must
declare a different valid fallback.

The governor uses measured **POST-Early-Z** material requests, cache-line fill
estimates and previous-frame queue pressure, targeting **no more than 80% of
product-clock sample capacity**. The 1,094,600 count is a required stress gate,
not a claim about every real frame.

## R9 — MATERIAL COMBINER V1

Stop waiting for an unspecified donor law. **These are Zhaozhou-native v1
recipes. Do not label them Sacrifice-exact.**

    unit_mul8(a,b)   = (a*b + 128) >> 8
    modulate2x8(a,b) = sat_u8((a*b + 64) >> 7)
    lerp8(a,b,w)     = sat_u8(a + rescale_s((b-a)*w, 8))    w unit8, raw/256

    0 PASSTHRU            count 0 or 1. count 0: has_texture=0, no sample read
    1 MODULATE            2: RGB = unit_mul8(s0,s1)              A = s0.a
    2 MODULATE2X          2: RGB = modulate2x8(s0,s1)            A = s0.a
    3 LERP                2: RGB = lerp8(s0,s1,recipe_weight)    A = s0.a
    4 ADD_SAT             2: RGB = sat_u8(s0+s1)                 A = s0.a
    5 MASK                2: RGB = s0.rgb          A = unit_mul8(s0.a, s1.a)
    6 TERRAIN_DETAIL_LIGHT 3: RGB = unit_mul8(modulate2x8(s0,s1), s2)  A = s0.a
    7 TERRAIN_DETAIL_MASK  3: RGB = modulate2x8(s0,s1)  A = unit_mul8(s0.a,s2.a)

For every multi-sample recipe: sample 0 is base and owns alpha unless the
recipe names a mask; output palette index is `sample0.index`; error/status bits
are ORed over all required samples; `recipe_weight` is stored in the TEXJOIN
record; a sample-count mismatch or unknown recipe is a material-asset error.

Reject malformed assets before sealing. If one reaches hardware, raise a sticky
frame fault and repeat the previous complete frame — **do not emit a plausible
placeholder texel.**

**Implement the combiner as its own registered II=1 pipeline**, not a large
combinational case on the TEXJOIN retirement path. Add
`MATERIAL_RECIPE_VERSION = 1`.

## R10 — MEASURE.HISTOGRAM REMAINS REFUSED

No producer defines one authoritative scalar error stream; no bucket or cutoff
law exists; Measure v1 does not consume it. A later v2 may add it only with a
named producer and Q format, generated bucket edges, an exact budget/cutoff
algorithm with hysteresis, a versioned governor input and a mutation-tested
reference. **The documented refusal is the correct implementation.**

## R11 — CONTRACT CORRECTIONS

* **GEOM.VDECODE** — format 0 RAW/CANONICAL authorised now; 1 and 2 bake-off
  gated.
* **GEOM.LOOM** — amend the purpose line. It evaluates a parent-before-child
  transform stream. Gait, formation and Form outputs are **input values**. No
  gait simulation, recursion, cycle discovery, matrix inversion or gameplay
  event generation.
* **FORGE.PRIM** — amend to the six v1 families. Do not let a broad purpose
  sentence silently ratify extra generators.
* **GEOM.MESHFETCH** — build against the 64-byte-aligned versioned descriptor
  and raw format first. Cull only outside every active camera. Keep descriptor
  fetch separate from VDECODE.

---

# RECENT TEXTURE RTL — WHAT IS GOOD AND WHAT IS NOT READY

The 2026-09-02 work is directionally strong: paired arithmetic tests,
local-ready discipline, same-line fill multicast, resident palette generations,
one bilinear lane, explicit HOL counters.

**But "a source file exists and a directed test passes" is not the same
maturity as production-buildable hardware.**

## X1 — RCP24 scheduler
Good: shipped reciprocal as hardware oracle; four jobs/reciprocal asserted;
~4.01 clocks/reciprocal; eight contexts remove idle bubbles.
Required: one-seed exploratory fit then three seeds; composed test with token
allocation and output stalls; prove no token reuse before completion; close at
the island target, not merely in Verilator. **No architectural rewrite
justified.**

## X2 — PERSPUV: THE THROUGHPUT CLAIM IS INCOMPLETE
One product/clock and one U/V pair in ~2 clocks is fine for a **one-sample**
material. It is **not** sufficient as the production three-sample lane:

    1,094,600 samples x 2 products = 2,189,200 products
    design budget                  = 1,333,333 clocks

**Production ruling:** compute RCP once per fragment; store the reciprocal in
the fragment/token record; enqueue one perspective-pair job per required
sample; **instantiate two parallel product paths, U and V, so one complete pair
starts each clock**; pipeline variable rescale/saturation after both products;
keep the existing one-product lane as oracle/area comparison.

A single lane may be selected only if a real material trace proves average
recovered-UV demand <= 0.5 pair/clock **and** the three-sample tier shares
recovered coordinates by contract.

## X3 — TEXJOIN v2: DO NOT INTEGRATE UNCHANGED
Good: sixteen slots; 0..3 samples; token-indexed returns; completion distinct
from retirement; local-storage ready; AUX identity; stale-generation test.

**Problems:**
1. Accepts a fragment **after** perspective recovery. The island allocates the
   token immediately after Early-Z so reciprocal/perspective and AUX run
   concurrently. Current topology forfeits AUX overlap and leaves
   variable-latency perspective outside the transaction table.
2. **Generation is two bits, not eight.** Four slot reuses is not a demonstrated
   ABA bound.
3. **`free_cnt_q` is decremented in accept and incremented in retire with
   separate nonblocking assignments — same-cycle accept and retire loses one
   update.** The PERSPUV lane already found this exact class of bug; the TEXJOIN
   test does not exercise it.
4. Issue selection scans 16 entries x 3 samples combinationally, and AUX scans
   all 16 again. **Likely a timing wall at 120-125 MHz.**
5. `tmu_rready`/`aux_rready` tied high with no return skid/FIFO. Invalid sample
   index 3, duplicate return, two errors in one clock, return to a
   retiring/reallocated slot and return-count collisions are not fully guarded.
6. `sample_count == 0` completes immediately but the combiner still reads
   uninitialised sample0 storage.
7. Output is a combinational view of table storage, not a held registered
   retirement packet.
8. Non-passthrough arithmetic is a placeholder, so it cannot feed the production
   fragment core.

**Production topology:** Early-Z survivor -> TEXJOIN_ALLOC {slot,gen8} -> one
RCP per fragment -> 0..3 perspective-pair requests after RCP -> 0..3 TMU
requests -> optional AUX **immediately at allocation** -> token-indexed result
writes -> MATERIAL.COMBINE II=1 -> registered allocation-order retire ->
RASTER.FRAGMENT.

`token = {generation[7:0], slot[3:0], sample_index[1:0]}` in a 16-bit transport
field, remaining bits reserved.

Replace entry scans with a token work FIFO for sample issue, an AUX FIFO, a
perspective-job FIFO, return skids and direct-index table writes. Compute the
occupancy delta **once per clock**.

Tests to add: accept+retire same clock; zero-sample path; duplicate return;
invalid sample index; generation wrap; output held under stall; simultaneous
TMU/AUX return; return coincident with slot reuse; malformed recipe; 16 slots
x 3 samples under randomized stalls.

## X4 — TMU planner
The non-CLUT transcription being differential-green after correcting two guessed
enum encodings is good evidence. **It is not a complete TMU proof:** CLUT was
excluded because resident palette translation changes the access stream;
decode/filter/palette/ROB composition is untested end to end; `src_id` must
become the internal token; physical ready chains unknown.

Required: CLUT8/CLUT4 exact tests through resident palette and cold fallback;
the inherited 79-case suite against the composed stream; randomized cache and
output stalls; fit.

## X5 — Cache-response dispatcher
Input-ready decoupling is correct; preserve the HOL counter. **Do not add
per-class pre-sorting** until traces show the shared raw FIFO is a limiter. Fit
it. If HOL stalls exceed 2% of texture-island cycles on the stress trace,
replace the shared ingress FIFO with three class-selected ingress queues;
otherwise keep the simpler block.

## X6 — Resident palette
The generation idea is correct; four-bit generation and the load protocol are
not final.

**Production load protocol:**
`BEGIN(slot, new_generation8, expected_crc)` — immediately marks the slot
nonresident and blocks/refuses same-slot lookup.
`WRITE(index, value)` — legal only while that slot is loading.
`END(slot, generation, crc_ok)` — resident only if all 256 entries arrived and
CRC matches.

**Never reload a slot with the same generation.** A lookup accepted on the same
clock as BEGIN must report nonresident/stale, never rely on read-during-write.
Test same-address lookup/write, aborted load, duplicate/missing entry, CRC
failure, generation wrap.

## X7 — Texture cache pipe
Good: same-line fill multicast; one line fetched once for all matching lanes;
acceptance no longer reads consumer ready; one blocking miss is evidence-based.

**Not physically acceptable yet:** the source calls itself staged but reads
tag/data arrays **combinationally** and classifies from those reads; there is
**no explicit M10K output capture stage** before broad compare/select; `REQN` is
nominal while pointer/count widths are hard-coded for four entries. **The
expected M10K inference and timing seam are unproved.**

**Production stages:** C0 local request FIFO; C1 register lane tag/index/beat
and issue synchronous RAM addresses; C2 capture tag/data M10K outputs into
fabric flops; C3 compare/classify, select one miss identity and
`fill_lane_mask`; C4 response FIFO / miss sequencer.

Keep multicast and one blocking miss. Either derive all widths from `REQN` or
assert `REQN == 4`. **Do not accept a cache fit as architectural closure if the
RAMs become flops/MLABs or an M10K output launches a broad combinational path.**

## X8 — Terrain residency prototype
**Must not be integrated as the world directory.** See T9/T10.

## X9 — Fit apparatus
The failed block fit was an **apparatus failure, not an RTL timing result**.
Add `design/fit_targets.yml`. Every target declares: top module; complete
ordered source closure; parameter overrides; clock port and period; virtual-pin
policy; expected RAM/DSP structures; the test command that must be green before
fit.

`run_block_fit` must validate the top is defined before launching Quartus and
must create a workspace or fail with an explicit "source closure missing".
**Never require a human to remember `-ExtraSources` for every new leaf.**

---

# THE 8 KM TERRAIN WORLD — BINDING RULINGS

A dense 8x8 km world at 2 m pitch is 15,625 pages x 21,376 B ≈ **318.5 MiB** and
cannot all be resident. The machine is a sparse/streamed world with 1,024 local
page slots.

## T1 — Canonical terrain key
`{ resource_epoch:u32, island_id:u32, patch_ix:i16, patch_iz:i16 }`

Patch pitch is a property of the island table, validated against the page
header; **not part of the lookup key**. No global coordinate projection is
identity. **Two islands may legally overlap in local patch coordinates.** The
current residency interface lacking `island_id` is **superseded**.

## T2 — Exact initial local-SDRAM guard map (bank 2)

    0x0400_0000 .. 0x054D_FFFF   TERRAIN.PAGE_POOL          1,024 x 21,376 B
    0x054E_0000 .. 0x0565_FFFF   TERRAIN.RESIDENT_MIP_POOL  1,024 x 1,536 B
    0x0566_0000 .. 0x056E_FFFF   TERRAIN.COMPOSED_HEIGHT      256 x 2,304 B
    0x056F_0000 .. 0x0577_FFFF   TERRAIN.COMPOSED_VELOCITY    256 x 2,304 B
    0x0578_0000 .. 0x057F_FFFF   TERRAIN.WRITEBACK_STAGING/JOURNAL  64 x 8 KiB
    0x0580_0000 .. 0x0585_FFFF   TERRAIN.COMPOSED_MIP_POOL    256 x 1,536 B
    0x0586_0000 .. 0x05FF_FFFF   reserved/unmapped until traces justify

**No separate permanent E/F/H pools** — those layers live inside the 21,376-byte
page. The mip pools are derived caches, not canonical assets. Bank 3 holds
GEOM.PARAMBUF (R7).

Every region starts **DENY-BY-DEFAULT** in MEM.GUARD, with **state-aware**
permissions: a loader may write only a LOADING slot; active terrain may read
only a ready slot with matching epoch/generation; bake/stamp writes only owned
layer ranges.

## T3 — Memory clients
Keep `ENGINE0` (framebuffer/render write) and `ENGINE1` (render-geometry domain,
including GEOM.PARAMBUF and active terrain page/composed traffic behind a local
arbiter) unchanged.

Add **`ZHAO_CLIENT_TERRAIN_BUILD = 6`** — a **best-effort/background** client
for HPS-to-local page loads, F-sheet writeback, prefetch and staging/journal
traffic. **It does not join guaranteed round-robin merely because a page is
late.** When a page is not ready the renderer uses a declared proxy and records
pressure rather than stealing scanout/render service.

Client ID 5 remains available for a measured split if board evidence proves
ENGINE1 arbitration is the limiter. **Do not spend it pre-emptively.** Re-run
MEM.ARBITER liveness and guard proofs after adding client 6.

## T4 — Canonical mirrors and dirty writeback
HPS owns canonical layers **B and D** and keeps them current from the same
deterministic commands. **Do not write B or D back on eviction.**

**Layer F (8 KiB surface sheet) has no canonical HPS mirror.** Therefore: track
`dirty_F` separately; on dirty eviction or explicit save copy exactly F to the
HPS terrain journal; **wait for journal acknowledgement before the slot may
enter LOADING**; reload F from the journal when the page returns.

A single generic dirty bit is insufficient. Keep at minimum: `modified_BD`
(counter only, no writeback), `dirty_F` (writeback barrier), `mips_stale`
(regeneration barrier).

## T5 — Permanent command ABI
Two versioned commands; opcodes confirmed by the ZIDL generator before commit.

    TerrainEpoch @ 0x0220, size 16
      epoch:u32; op:u8 (BEGIN=0, END_FLUSH=1, ABORT=2); flags:u8;
      reserved:u16; island_table_handle:u32; source_id:u32

    SubmitTerrainSet @ 0x0230, size 32
      resource_epoch:u32; list_offset:u32; list_bytes:u32; list_crc32c:u32;
      patch_count:u16; view_mask:u8; flags:u8; sequence:u32; reserved0/1:u32

    Patch-list record, 32 B
      island_id:u32; patch_ix:i16; patch_iz:i16; hps_page_addr:u64;
      expected_page_crc32c:u32;
      flags:u16 (REQUIRED, PREFETCH, DYNAMIC, DUAL, HAS_SAVED_F);
      view_mask:u8; priority:u8; source_id:u32; reserved:u32

**One SubmitTerrainSet covers the whole required+prefetch set.** Do not emit one
DrawProcedural per patch and do not overload an existing terrain field command.

Canonical order: required before prefetch; smaller priority first; view-union
key; island_id asc; patch_iz asc; patch_ix asc; source_id asc. **The sealed list
is capture data — replay does not rerun the HPS visibility walk.**

## T6 — The 256-patch composed cache
The 256 slots are for patches needing **live composed** height/velocity this
frame, **not a cap on all visible terrain**. Static/baked visible pages render
from resident page layers and consume no dynamic slot.

Deterministic pressure order: (1) bake and retire completed persistent fields
into B; (2) remove optional visual-only field programs in declared source
priority; (3) choose material/geometry fallback for distant optional
deformation; (4) retain all player-contact/gameplay/collision-required live
patches; (5) retain remaining visible live patches by projected importance then
canonical key.

**Hardware never silently displays stale base terrain in place of a required
live field.** If more than 256 REQUIRED dynamic patches remain after legal
degradation: fault the frame, drain, repeat the previous complete frame, record
rejected source IDs and keys.

## T7 — Prefetch policy and load budget
Working set: current visible set for both views; one-patch Moore ring around it;
predicted visible set **30 frames (0.5 s)** ahead from camera velocity; explicit
gameplay-required patches. **Union the views before deduplication.**

**Ceiling: 32 whole pages per frame** ≈ 41 MB/s at 21,376 B and 60 Hz.
Provisional, not a board claim. Order: required current, then predicted visible,
then neighbour ring, canonical ties.

Board counters may reduce this immediately; raising it requires sustained
measured bridge+SDRAM evidence.

**A half-loaded or CRC-failed page is never rendered.** A normal streaming miss
uses the island's declared proxy/coarse silhouette/open sky and records the
miss. **Do not freeze the old camera frame for ordinary traversal** — only hard
internal overflow/corruption repeats the prior complete frame.

## T8 — Mip derivation
Generate height mips **on the FPGA** in one `TERRAIN.MIPGEN` after page CRC
verification and after any bake changing B/D.

**Exact v1 law, no averaging:**

    mip17[i,j] = fine33[2*i, 2*j]   i,j in 0..16
    mip9 [i,j] = fine33[4*i, 4*j]   i,j in 0..8

Top and bottom. `(17*17 + 9*9) * 2 B * 2 surfaces = 1,480 B` in a 1,536-byte
record. **Nested decimation keeps shared vertices exact and introduces no
rounding.** A coarser 5x5 is selected from mip9 on demand; do not store a third
page mip without evidence.

A page becomes RESIDENT only after: payload CRC passes; bytes complete; resident
mips complete; any restored F sheet complete. Generate analogous mips for a live
composed lattice into COMPOSED_MIP_POOL. **HPS does not implement a second mip
law.**

## T9 — Residency mapping and replacement
**Reject the direct-map architecture as production.** Determinism does not
require direct mapping — a deterministic set-associative cache is deterministic
under a canonical request order and avoids the 2,048-m periodic thrash.

**256 sets x 4 ways = 1,024 slots.**

Set index: **CRC-8/ATM, polynomial 0x07, initial 0**, over the little-endian
bytes of `{island_id, patch_ix, patch_iz}`; xor `resource_epoch[7:0]` into the
final byte.

Store the full key in every way. Replacement: (1) matching key; (2) invalid way;
(3) clean unpinned way by per-set round-robin; (4) `dirty_F` unpinned way by the
same order, entering EVICT_PENDING; (5) all pinned: backpressure and count.

Update replacement state on **canonical claim acceptance**, never on
asynchronous fill completion. **No software "two islands must not collide"
restriction exists.**

## T10 — Terrain residency v2
Build `zhao_terrain_residency_v2` **beside** the prototype. Do not integrate the
direct-map file.

States: INVALID, RESERVED, EVICT_PENDING, LOADING, MIPGEN, RESIDENT_CLEAN,
RESIDENT_DIRTY_F, FAULTED.

Per slot: full island/ix/iz key; `resource_epoch` u32; **generation u8
minimum**; state; pin/refcount; `modified_BD`; `dirty_F`; `mips_stale`;
expected CRC / loader sequence.

Handle: `{resource_epoch:u32, slot:u10, generation:u8}`.

Rules: no slot reuse before pin count zero; no `dirty_F` reuse before writeback
ACK; loader completion carries success/failure and CRC identity; generation
increments only on successful new reservation; lookup hits only
RESIDENT_CLEAN/DIRTY_F; all mutation events serialized or with explicit atomic
priority; **same-cycle old FIN/DIRTY/UNPIN can never modify a newly reserved
occupant**; no giant reset loop over asynchronous metadata.

Use **synchronous metadata RAM banks, one per way**, with registered
lookup/capture. On power reset perform a 1,024-entry init sweep while ready is
low — **do not async-reset an inferred RAM.** Level epoch invalidation is
logical.

Required tests: two overlapping islands with same local coordinates; adversarial
keys colliding in one set; all four ways pinned; dirty victim with delayed
writeback ACK; stale FIN same clock as a new claim; stale DIRTY same clock as a
new claim; loader CRC failure; aborted load; duplicate finish; generation
reuse/wrap; handle held under 255 intervening claims; Duo working-set union;
teardown while jobs in flight; deterministic repeat of the same claim stream.

## T11 — Level teardown
**Full console reset is not level unload.**

`TerrainEpoch END_FLUSH`: stop accepting new terrain jobs for that epoch; drain
current patch/compose/bake jobs; wait for all pins to reach zero; write back
every `dirty_F` slot; wait for all journal ACKs; emit teardown ACK; invalidate
the epoch logically.

`TerrainEpoch BEGIN`: installs a strictly newer nonzero `resource_epoch` and the
island-table handle; starts with no resident hit from an older epoch; permits
background physical clearing later.

`ABORT` is legal only for reset/fault recovery, may discard dirty presentation
state, and **must record that it did so**.

## T12 — SW.STREAM: THE REAL REMAINING STUB
Fill `design/contracts/SW.STREAM.md` before declaring the world layer buildable.

Responsibilities: parse ISLAND_TABLE and sparse page maps; maintain HPS
canonical B/D; maintain the F-sheet journal; build the deterministic
current/predicted/prefetch set; stage complete 21,376-byte pages in HPS DDR;
validate cartridge/resource bounds before staging; emit TerrainEpoch and
SubmitTerrainSet; consume residency/load/writeback/pressure counters; preserve
sealed list bytes for capture/replay; **never expose a half-built page list to
CMD.DMA.**

Backpressure is **deadline-driven software backpressure**: SW.STREAM may defer
PREFETCH records but **may not mutate a sealed REQUIRED list**. If staging
cannot meet the deadline it selects proxy/fallback before sealing.

---

# FIT AND INTEGRATION ORDER

**Phase 0 — repair the fit apparatus.** `design/fit_targets.yml`; explicit
source closure; preflight top-module existence; archive fitter settings,
resources, path owners, RAM/DSP inference and three seeds where required.

**Phase 1 — exploratory leaf fits.** rcp24_svc; current perspuv_svc as
comparison; aux_div6/AUX pipe; bilerp_lane; rsp_dispatch; tmu_plan;
palette_res after protocol repair. *A leaf fit is reconnaissance. It does not
authorize production integration.*

**Phase 2 — required texture repairs.** Two-product II=1 PERSPUV pair lane;
TEXJOIN per X3; freeze and pipeline MATERIAL.COMBINE; rebuild cache with the
C0-C4 synchronous RAM seam; finish palette BEGIN/WRITE/END; add CLUT/full
inherited differential coverage.

**Phase 3 — texture-survivor composition.** One top: EARLYZ -> token allocation
-> RCP once per fragment -> perspective pair jobs per sample -> TMU planner ->
real cache -> response dispatcher -> resident palette/cold fallback -> nearest
or one bilinear lane -> token-indexed return -> AUX concurrently -> material
combine -> allocation-order retirement -> RASTER.FRAGMENT.

Stress: 0/1/2/3-sample recipes; the 1,094,600 envelope; measured post-Z traces;
same-line and four-line cache misses; random backpressure; repeated external
src_id; stale palette reload; generation reuse; malformed material; Duo;
cache-cold traversal.

**Phase 4 — physical gates.**

    new leaf datapaths                    design against 150 MHz
    perspective/TMU/cache/AUX leaf island 120-125 MHz across three seeds
    texture-survivor composition          115-120 MHz
    full intended composition             105 MHz acceptance floor
    architecture objective                110 MHz
    stretch / headroom                    115 MHz
    product clock                         100 MHz, no lucky-seed dependency

A lower leaf number may be accepted if the composed island still meets its gate
and the path is not structural. **A lucky 125-MHz leaf does not excuse a
100-MHz composed ready chain.**

**Phase 5 — GEOM.PARAMBUF.** Register contract/ledger/reference; allocate bank
3; adapt binner to compact external IDs and chunked refs; add frame-generation
guard; compose geometry -> binner -> ParamBuf -> edgewalk; measure real army and
giant traces.

**Phase 6 — terrain world.** Commit T1-T12 into senior specs/contracts; build
residency v2; add guard map and TERRAIN_BUILD client; implement HPS
SW.STREAM/visible list; loader+CRC+MIPGEN; composed cache allocator; F
writeback; command path; compose the existing organs; run 8-km traversal,
teleport, two-island collision and Duo captures.

**Phase 7 — particles and post.** Bump QFMT/particle format; build the particle
reference chain before RTL; plane role semantics; tile-local POST.GATHER;
one-pass line POST.COMPOSITE; only then fit spectacle with the renderer.

---

# MINIMUM MUTATION / TEST CHECKLIST

**Every new block:** test correctness and initiation interval separately;
include simultaneous push+pop; include reset/stall while full and while empty;
**mutate ready coupling so the test demonstrably catches serialization**;
mutate generation compare; mutate one enum encoding; mutate one rounding
constant; mutate one overflow boundary; run twice for deterministic equality.

**Every memory block:** prove expected M10K inference **from Quartus, not
comments**; archive startpoint kinds; no M10K output launches a broad
combinational cone; test read-during-write explicitly or architect it away;
derive parameter widths or assert fixed parameters; **count actual events with
one net counter update per clock.**

---

# FINAL ANSWERS TO THE ORIGINAL QUESTIONNAIRE

1. **Particle / plane / compositor** — broad behaviour already frozen; adopt
   R3's numeric law. FPGA collisions are live terrain + analytic planes only.
   Survivors outrank children. Sprites HUD-only. Planes have explicit
   BACKDROP/ATMOSPHERE roles and no arbitrary depth. Compositor keeps the frozen
   order but is one bounded line stream plus a quarter-res glow prepass.
2. **wmin/wmax/scale** — closed by R1. Do not ask again.
3. **kMesh budget** — 32 machine-wide, at least 16 per active Duo view; giant
   separate. **Do not grow the on-chip arena; build GEOM.PARAMBUF.**
4. **Three-sample terrain** — yes as a supported shipping tier behind an
   explicit deterministic fallback chain; **not** guaranteed on every fragment.
5. **276,480** — conservative pre-Early-Z covered fragments, Z60, 3.0x overdraw.
6. **MEASURE.HISTOGRAM** — leave refused.
7. **Terrain key** — `{resource_epoch, island_id, patch_ix, patch_iz}`.
8. **Terrain map/client/writeback/ABI/cache/prefetch/mips/replacement/unload** —
   T2-T11.

---

# IMMEDIATE REPOSITORY CHANGES

**Docs/spec:** mark OPEN-SPEC-DEPTH-QUANTISATION superseded; correct
OWNER-SPEC-QUESTIONS; fill SW.STREAM; add GEOM.PARAMBUF to blocks.yml and a
contract; amend qformats particle section and bump QFMT_VERSION; add plane role
fields; replace POST.GATHER memory/precision; replace POST.COMPOSITE five-pass
section; freeze material recipes; add terrain memory map, key and command ABI;
correct GEOM.VDECODE/LOOM/FORGE status language.

**RTL/tests before integration:** fix block-fit source manifests; rebuild
PERSPUV for one pair/clock; repair TEXJOIN v2; repair palette load protocol;
rebuild cache around synchronous capture; build residency v2 instead of
integrating the direct map; **do not add further leaf blocks until the texture
island is composed and fit.**

---

## THE CENTRAL CONCLUSION

> Zhaozhou is not blocked by dozens of unanswered game-design questions. It is
> blocked by a much smaller set of concrete numeric, storage, transaction and
> physical-composition problems — and several of today's "finished leaves" still
> need one more architecture pass before they are allowed into the machine.
