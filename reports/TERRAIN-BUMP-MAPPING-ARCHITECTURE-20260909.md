# Terrain detail bump mapping — architecture, and the production block

2026-09-09, rescue phase, branch `zixxtrixx-v8-closeout`. Answers the owner's
`bumomapping.md` (2026-09-05, verbatim: *"we need detail bump mapping for
terrain. please architect it and set it up for production. I hope it is not
too expensive but terrain is the star of the show and we neglected giving it
first class treatment."*)

Every number is labelled **MEASURED** (an instrument ran this session),
**STRUCTURAL PREDICTION** (follows from the RTL's shape; the named fit
verifies it), or **UNKNOWN** (no instrument has answered; the instrument is
named). Nothing was committed; no Quartus fit was run (standing order).

---

## The answer to "I hope it is not too expensive", up front

**It is affordable, and the affordable version was built and tested today.**
The detail-bump organ costs:

| axis | cost | status |
|---|---|---|
| DSP | **0** — no multiply operator exists in the module | STRUCTURAL (grep-verifiable), fit confirms |
| ALM | **~450**, ceiling 500 | STRUCTURAL PREDICTION |
| M10K | **~14** of ~406 free (pyramid tile ~12 + two 256x40 K-tables), ceiling 15 | STRUCTURAL PREDICTION |
| external bandwidth / TMU samples | **0** — the texel never touches the TMU or the cache | BY CONSTRUCTION |
| frame clocks | **+0** on the fragment stream (bump-in-wire, II=1, measured); +~267 cycles per config epoch (~0.016% of 1,666,666) | MEASURED in sim |

On the rescue's axes — ALM at 139% committed, DSP at 171%, M10K at ~27% —
this block spends **only the abundant resource**. The delta wire's zero state
is bit-exact identity, so the cut lever stays free forever.

**The expensive thing next to it is not bump mapping.** TERRAIN.SHADE — the
per-triangle base light — is the block that costs real arithmetic, and its own
contract says why it must not be billed here: *production terrain has no
lighting path at all*; SHADE is needed with or without normal maps. The bump
organ composes with SHADE when SHADE lands, and degrades gracefully (not
silently — see Q5) until then.

---

## What already existed — the biggest finding

This feature was **80% architected on 2026-09-03**, two days before the owner
asked for it:

* `design/contracts/TERRAIN.NORMALMAP.md` — a full contract for exactly this
  organ: heightfield tangent trick, resident M10K tile, s9 delta, cut plan.
* `reports/NORMALMAP-ARCHITECTURE.md` — the decision record: five options
  priced, the always-resident-tile option chosen, the draft RTL's six defects
  documented.
* `reports/BRO-20260903-NORMALMAP-AND-ANIMATION-PATH.md` — the owner brief
  that split SHADE (terrain lighting, mandatory) from NORMALMAP (detail,
  cuttable).
* `reports/zhaozhou-terrain-mipmapping-architecture-2026-09-05.txt` — same
  day as `bumomapping.md`: the detail tile needs a seven-level mip tail or it
  aliases; integer-LOD contract; the albedo LOD-nibble defect.
* `reference/include/zref/zref_terrain_normalmap.hpp` /
  `zref_terrain_shade.hpp` — oracles, already amended once (the light is
  Q16.16, not s1.15 — "a factor of two in the relief").
* `fpga/rtl/terrain/zhao_terrain_normalmap.sv` — a DRAFT self-labelled known
  wrong, quarantined `unused` in the manifest.
* `reference/src/zrender/terrain.cpp` — `shade_flat_tri_dir_unclamped` exists,
  the detail-before-clamp composition is already ruled and coded, **gated off**
  (`kTerrainDetailStrength = 0`), and its current detail is a per-triangle
  positional hack, NOT the per-pixel tile — it must not be counted as the
  feature or as the look-gate.

Nothing else in `fpga/rtl/` does any part of per-fragment detail lighting
(swept: the draft was the only bump/detail RTL; renderer detail is the gated
hack above). So the work was not to invent an architecture — it was to
**reconcile the three documents that had drifted apart, correct the one that
is wrong, and build the block**. `bumomapping.md` landed at the repo root and
sat unread for four days; `reports/OWNER-DOCUMENT-INDEX.md` now exists partly
because of that.

## Where the brief and the standing documents were wrong — four findings

**F1. The contract's rescale constant is wrong by exactly 2 — confirmed,
fixed, and instrumented.** `TERRAIN.NORMALMAP.md` §Q-formats says
`strength/256 * d/128 * L/32768` "expressed with 8 fraction bits is
`strength*d*L / 2^23`". Its own format sentence computes 2^22
(8+7+15 in, 8 out). The oracle header documents the identical slip on its own
history ("the first version of this file said s1.15 … a factor of two in the
relief") and was fixed on 2026-09-03; the contract never was, and the wrong
23 had already propagated into `NORMALMAP-ARCHITECTURE.md` and the mipmapping
addendum — the frozen-copies corollary in CLAUDE.md, live. Consequences of
22: full-scale one-sun delta is ~253 (essentially full colour scale), not
~126, and one sun CAN rail s9 at the legal register corners.
`zref::terrain::normalmap_delta_s9` is now the executable law; the directed
suite pins the tie-rounding with literal vectors; and a `-GDELTA_SHIFT=23`
build of the real RTL **fails 1,830 of 4,738 checks** (MEASURED) — the law is
instrumented, not asserted. Contract amendment A1 records it.

**F2. The 2-DSP MAC the contract budgeted is refusable — zero DSP is
possible, and the rescue mandates the shape.**
`ZHAOZHOU_MEMORY_FIRST_RESOURCE_RESCUE_2026-09-09.txt` §14.3: *"For a fixed
per-epoch light coefficient, byte tables offer exact products."* The sun and
strength change per epoch; dx, dz are bytes. So:

    tblx[dx] = dx * Kx,   Kx = strength * SUM_suns(sun_x)     (exact, s33)
    tblz[dz] = dz * Kz,   Kz = strength * SUM_suns(sun_z)

Two 256x33 tables (one M10K each at 256x40), and the per-fragment datapath is
two RAM reads, one 34-bit add, ONE rounding, one clamp. The tables are filled
by an in-block sequencer with **no multiplier anywhere**: K by an 8-step
shift-add over strength's bits, the fill by pure accumulation from −128·K
(256 adds). Multi-sun folds into K by distributivity BEFORE the single
rounding, so the result is bit-identical to per-sun accumulation at any SUNS
— and SUNS=2 becomes literally free per fragment. Every product is exact;
`spec/qformats.md` §3's one-rounding law holds. Cost of the trade: a ~267-cycle
COLD window per config epoch, during which deltas are 0 and counted
(`cold_o`), which is also what makes "reset = bit-exact off" hold without the
un-inferable RAM reset.

**F3. The contract's application seam is two island generations stale.** It
names "TEXJOIN v2 retirement"; the island now runs `zhao_texture_fragrob`
(port-identical successor, allocation-order retirement kept), and the next
rearchitecture (`v3own`/`frag_expand`) makes ISSUED its own moment and warns —
mipmapping addendum §7 — *"do not revive a timing-only alignment FIFO across a
path that can reorder."* The seam law is therefore restated: the delta joins
the fragment at retirement **by tagged association, checked, not by timing**.
Concretely: the alignment FIFO carries `d_src_id_o`; at the join it is
compared against the retiring fragment's own id (two ids arriving through two
independent paths — a checker whose operands do NOT move together, per the
2026-09-08 law), with a mismatch counter and a seam-time mutant to fire it.
If the future fragment path ever reorders retirement, the FIFO becomes a
small id-indexed bank; the block itself does not change.

**F4. `NORMALMAP-ARCHITECTURE.md`'s SHADE shape (rsqrt table + Newton step,
10 DSP) is superseded** — rescue §14.1 by name: *"Do not adopt the older
normal-map report's approximate rsqrt/NR proposal as though it superseded the
later exact lighting contract."* TERRAIN.SHADE stays contracted on the exact
law (`shade_flat_tri_dir_unclamped`, exact magnitude, exact signed quotient),
with §6.5 coefficient tables for the epoch-constant sun dot and the II=3
Pareto row its contract already demands. That is a different lane's build;
this report only refuses to inherit the stale 10-DSP shape or bill any of it
to bump mapping.

---

## The six questions

### Q1 — Where does the detail normal come from

**A resident, tiling, world-anchored {s8 dz, s8 dx} pyramid in M10K, addressed
by the terrain UV the fragment already carries.** (Option C of
`NORMALMAP-ARCHITECTURE.md`, re-affirmed under rescue arithmetic.)

| option | M10K | bandwidth | verdict |
|---|---|---|---|
| A: tangent-space normal map in VRAM via TMU | 0 (VRAM ~10.9 KiB tiling / ~1.33 MiB unique) | **+276,480 TMU samples/frame (+51% on the known subtotal)**, cache pressure on an island that closed at 98.66 MHz | REJECTED; remains the escalation if unique-per-texel normals are ever wanted (the sample protocol already speaks 3 samples/fragment) |
| B: smuggle normals in albedo bits / derive from albedo | 0 | +taps | REJECTED: CLUT8 has no spare bits, and albedo luminance is not height — deriving relief from it is the art law's "measuring a projection" |
| C: resident M10K pyramid (CHOSEN) | ~14 | **0** | spends only the abundant resource; tiling noise is the right content — uniqueness stays the albedo's job |
| D: bake shading into albedo | 0 | 0 | dead under a moving sun — the owner's ask is precisely the moving-sun response; kept as post-cut fallback |
| E: denser tessellation | 0 | huge projector cost | REJECTED: detail frequency is far below cell size |

### Q2 — Tangent frame

**Derived, free, and already settled by the contract's central trick**: a
heightfield's tangent frame is world-axis-aligned, so `d = (dx, 0, dz)`
perturbs the surface normal in world XZ directly — no basis built, stored, or
interpolated, no per-vertex attributes. `zhao_terrain_normals` contributes the
**face normal** (the `n` in `dot(n,L)/|n|`) to TERRAIN.SHADE; it neither has
nor needs any tangent output. Cost of the frame: zero. This is also why D-8
("this block is not precedent for general tangent-space normal maps") stands
unchanged.

### Q3 — Where the perturbation happens

**Per fragment, as a bump-in-wire beside the texture path — and the clock
argument is that it costs nothing.**

* Per-vertex on a 33x33 patch lattice puts one detail sample every lattice
  cell and interpolates between — at the detail frequencies wanted (well
  below cell size is exactly what it CANNOT represent), that is not bump
  mapping, it is slightly lumpier Gouraud. Rejected on purpose, not price.
* Per-fragment: the block rides the existing fragment stream at II=1, fixed
  latency 6, in order. It adds **zero cycles per frame** to the
  1,666,666-cycle budget: the stream's rate is set by the texture path it
  parallels, and 276,480 terrain fragments pass through at one per clock
  either way (II=1 MEASURED: 64 fragments in 70 cycles including the 6-cycle
  latency, unstalled). The only frame-time cost is the epoch refill,
  ~267 cycles per sun/strength change (MEASURED 267), ≤ 0.016%/frame.
* The middle option (per-vertex base + per-fragment detail) is exactly what
  the SHADE/NORMALMAP split already is — the expensive divide is per
  triangle, the cheap dot is per fragment. That split is kept, not reopened.

ALM/DSP: the per-fragment arithmetic after F2 is two RAM reads + one 34-bit
add + round + clamp — no DSP, and the ALM bill is dominated by pipeline
registers (~450 STRUCTURAL, table below).

### Q4 — Mip and distance

Adopted from the addendum, into the block (contract amendment A3):

* the tile carries the **seven-level pyramid** (64..1, 5,461 words, bases
  0/4096/5120/5376/5440/5456/5460), built OFFLINE by averaging **signed**
  dx/dz per 2x2 and never re-normalising — the detail-lighting expression is
  linear in d before the clamp, so the mean texel gives the mean detail term;
* per fragment, an **integer level** input `f_lod_i` (the addendum's
  preferred contract — NOT the TMU's Q4.4-high-nibble form, whose adapter is
  the addendum's §2 known defect) plus two authored knobs:
  `lod_bias` (s5) and `max_level` (u3), level =
  `clamp(f_lod_i + lod_bias, 0, min(max_level, LEVELS-1))`;
* when albedo and detail share UV but differ in scale, the addendum's law
  `L_detail = L_albedo + 10 − uv_shift` is exactly the `lod_bias` register —
  the knob IS the formula, per frame, on the HPS;
* **reset state max_level = 0 reproduces the un-mipped contract bit-exactly**
  (MEASURED: the mip-off directed case), so the mip story cannot regress the
  base behaviour;
* the fallback if the LOD feed is late: fade `strength` with patch LOD tier —
  cheaper, but it dims relief rather than coarsening it, and shimmer persists
  until strength ~0. It is a stopgap, not the design.

What the mip tail costs: +1,365 words (~+4 M10K predicted over the flat
tile), +~40 ALM of level select/addressing, zero DSP, zero clocks. The LOD
FEED (footprint-derived per-fragment level) is the genuinely unbuilt part —
it is the same feed the albedo mip path needs, the addendum's §5, and it is
NOT charged to this organ; until it exists, `f_lod_i` can be driven 0 or by a
patch-tier word, both legal.

Aliasing at nearest sampling (no bilinear, deliberately — coarser than the
CLUT8 albedo it perturbs would be wrong) remains a **look risk**: the
mitigations are the pyramid (built), the bias knob (built), and the eye
(pending — see Not Verified).

### Q5 — Composition with TERRAIN.SHADE

**The block does not require SHADE to exist** — it was built and fully tested
standalone today, and its delta is defined additively so the seam is
SHADE-agnostic. But the LOOK the owner asked for requires a base to perturb:

* **Today (no SHADE, no terrain colour path at all):** terrain fragments
  modulate albedo by an effectively white vertex colour. Applying the delta
  there can only darken (255 saturates upward), so relief renders at half
  its dynamic range. Usable for bring-up, wrong for judging the look.
* **Minimum SHADE for a first honest look:** even a per-frame constant base
  colour < 255 (headroom for positive deltas) shows the relief moving under
  the sun. That is a demo rig, not production.
* **Production:** TERRAIN.SHADE per its contract (exact law, F4), whose
  output folds into the flat vertex colour; then the seam applies the delta.
  SHADE's own gate — the owner looks at the zref island under a moving sun
  before its RTL is built — still stands and is unaffected by this block
  existing.

**The seam law, refined against the ruled clamp order.** The reference ruling
(and rescue §14.2) is per-light `ndl = clamp01(raw + detail)` — detail inside
the light's clamp. The contract's cheap seam (`v'_c = clamp_u8(v_c + delta)`)
deviates twice: monochrome detail (declared, kept) and clamp position — it
would let detail darken below ambient and could not lift a back-face above
its clamped-zero base. The fold-image of the ruled clamp for one sun is a
**lit-range clamp**: `v'_c = clamp(v_c + delta, ambient_c, sat_u8(ambient_c +
sun_c))` — two config constants per lane and two comparators (~30 ALM over
the plain form), no multipliers, monochrome deviation still declared. This is
NOT the withdrawn ambient-floor (D5): it does not re-legislate §4a, it
implements the ratified per-light clamp's image through the ratified fold.
Recommended for the seam build; `zref::terrain::normalmap_apply` carries the
plain form, and the seam's choice goes through its own contract when the seam
is built.

### Q6 — The honest bill (this organ only; SHADE billed to terrain lighting)

**MEASURED this session** (instruments named):

| item | value | instrument |
|---|---|---|
| lint | 0 diagnostics, `-Wall` | verilator_bin 5.051 |
| Quartus-17 form gate | clean, 217 files | `tools/quartus/check_quartus17_syntax.py` (self-test 3 fire / 6 no-fire) |
| functional law | 4,738 checks, 0 failures | `tests/texture/terrain_normalmap_directed.cpp` vs `normalmap_delta_s9` |
| checker alive | `--break-oracle` run FAILS (1 injected corruption caught) | same suite, positive control |
| shift law instrumented | `-GDELTA_SHIFT=23` build fails 1,830 checks | same suite vs parameter-mutated DUT |
| counters | all four fire, EXACT match vs model (fragments 2,324 / zeroed 220 / railed 36+ / cold 4 in the run) | same suite |
| II / latency / epoch fill | 1 / 6 / 267 cycles | same suite, counted not asserted |

**STRUCTURAL PREDICTION** (the fit gate verifies):

| piece | ALM | DSP | M10K |
|---|---|---|---|
| pyramid tile 5,461 x 16 (simple dual port) | ~40 | | ~12 (2048x4-mode slicing; a naive inference may round worse — this is the fit's question) |
| K tables 2 x 256x33 | ~10 | | 2 (256x40 mode) |
| epoch sequencer (shift-add K + accumulate fill) | ~80 | 0 | |
| pipeline P0–P5 regs + skid (~700 flops) | ~250 | | |
| funnel shift, level clamp, address add, 34-bit round/clamp | ~60 | | |
| cfg + counters | ~60 | | |
| **block total** | **~450 (ceiling 500)** | **0 (ceiling 0)** | **~14 (ceiling 15)** |
| seam, when built (align FIFO + id check + 3 lit-range adds) | ~110 | 0 | 0 |

**UNKNOWN**, with the instrument for each: fitted ALM and actual RAM-block
count (fit gate G-BUMP1); Fmax — risk points are the P1 32→6 funnel-by-
`uv_shift` and the P5 34-bit add+round+clamp cone, each a single stage
(G-BUMP1; the repair if either misses is one more pipe stage, latency 7);
`quartus_map` acceptance of the new file beyond the form gate (a block never
through quartus_map is not shown synthesizable — first batched fit); the
LOOK (zref render, moving sun, 240p, the owner's eye).

The cut plan is unchanged from the contract and got cheaper: soft cut
`strength=0` (bit-exact, MEASURED); LEVELS=1 drops ~4 M10K; s4 texels halve
the tile; full cut reclaims ~450 ALM + 14 M10K and zero DSP — meaning the
strongest historical argument for cutting it (DSP scarcity) no longer applies
to it at all.

---

## What was built today (working tree, uncommitted)

| file | status | what |
|---|---|---|
| `fpga/rtl/terrain/zhao_terrain_normalmap.sv` | **REWRITTEN** | the production block: zero-DSP epoch tables, mip pyramid, skid/II=1/fixed-6, four counters + `table_ready_o`/`idle_o` |
| `tests/texture/terrain_normalmap_directed.cpp` | **NEW** | 4,738-check directed+random suite, coverage-asserted, `--break-oracle` positive control (placed outside `tests/terrain/`, which is a live other lane) |
| `reference/include/zref/zref_terrain_normalmap.hpp` | AMENDED (additive) | `normalmap_delta_s9` (the block law, shift 22), `normalmap_apply`, pyramid addressing/level-select law |
| `design/contracts/TERRAIN.NORMALMAP.md` | AMENDED | status header + dated amendment A1–A5 (rescale 22, zero-DSP semantics/cold window, mip tail, latency 6, ceilings 500/0/15) |
| `design/blocks.yml` | touched | latency `fixed:6`, test/reference citations to real files, stale draft note replaced; maturity left at REFERENCE_COMPLETE — promotion is the reviewer's call |
| `design/prod_manifest.yml` | touched | `zhao_terrain_normalmap` stays **`unused` — an OPEN deferral on purpose**, visible to `tools/budget/uncashed_cheques.py`, until the seam wiring lands |
| `reports/TERRAIN-BUMP-MAPPING-ARCHITECTURE-20260909.md` | NEW | this report |

Build/run recipe (mirrors the terrain lane's standalone pattern):

```
verilator_bin --lint-only -Wall fpga/rtl/terrain/zhao_terrain_normalmap.sv     # 0
python tools/quartus/check_quartus17_syntax.py                                 # clean
verilator_bin -cc --exe --build -j 4 -Wall --Mdir build/standalone/normalmap_obj \
  --prefix Vzhao_terrain_normalmap fpga/rtl/terrain/zhao_terrain_normalmap.sv \
  tests/texture/terrain_normalmap_directed.cpp \
  -CFLAGS "-std=gnu++17 -I ../../../reference/include"
./build/standalone/normalmap_obj/Vzhao_terrain_normalmap.exe                   # 4,738 / 0
./build/standalone/normalmap_obj/Vzhao_terrain_normalmap.exe --break-oracle    # FAILS: instrument proven
# and the shift-law control: same verilate with -GDELTA_SHIFT=23 -> 1,830 failures
```

No committed mutant was needed: every counter the block owns (`fragments_o`,
`zeroed_o`, `railed_o`, `cold_o`) is reachable with legal stimulus and is
driven to fire with exact-count assertions in the suite. The
unreachable-state detector this feature will eventually own — the seam's
id-mismatch counter — belongs to the seam build, where its mutant
(a deliberately skewed alignment write) is named in the order below.

## Implementation order, with the one fit gate

1. **DONE (this session):** block RTL + contract reconciliation + suite.
2. **The look-gate (art law, before any wiring or tuning):** put the
   per-pixel tile path into the zref renderer behind the existing
   `kTerrainDetailStrength` gate (replacing the per-triangle positional
   hack), author a first detail tile BY EYE, render the island under a moving
   sun at 240p, owner looks. Golden CRCs stay pinned by strength=0.
3. **Asset lane:** committed tile packer (pyramid by signed averaging, the
   oracle's `normalmap_pyramid_addr` as its layout law) + the `SetTerrainDetail`
   ABI amendment (cfg words 0–4, tile upload, the per-frame Q16.16→s1.15 sun
   conversion).
4. **TERRAIN.SHADE** per its own contract and rescue §14 (exact law, II=3
   Pareto row) — separate lane, separate budget, its own gate.
5. **The seam:** `f_detail` producer (binding compare), LOD feed (shared with
   albedo mip work), alignment FIFO + **tagged id check at retirement**
   (independent-operand comparison; mutant = skewed alignment write, committed
   under `tests/mutants/`), lit-range clamp application.
6. **FIT GATE G-BUMP1 — the ONE fit this feature spends**, batched with the
   terrain-lighting subsystem per the fit-batching rule. The question it
   answers, stated in advance: *"Composed with SHADE and the seam, does the
   detail organ hold ≤500 ALM / 0 DSP / ≤15 M10K (hierarchical attribution),
   does the 5,461-word tile actually infer ≤12 blocks, and does the island
   still close ≥100 MHz with the P1 funnel and P5 clamp cones in place?"*
   Everything else this feature needed to know, Verilator answered today in
   seconds.
7. Composed capture: one terrain triangle through the full path against the
   zref renderer with the same tile — the first moment a delta is SEEN
   applied — then the owner looks again, in motion.

## Not verified, each with its instrument

* **Fitted ALM / M10K inference / Fmax / quartus_map acceptance** — G-BUMP1.
  Until then every resource number above is structure, not silicon.
* **The look** (does authored tiling relief at 240p read as first-class
  terrain; does nearest-mip shimmer or level-banding show in motion) — the
  zref render loop and the owner's eye, steps 2 and 7. No gate substitutes.
* **The seam** (alignment under the v3 lifetime owner, the id-check counter
  firing, the lit-range clamp choice) — the seam's own directed test and
  mutant, step 5.
* **The LOD feed** (footprint-derived levels; the albedo LOD-nibble defect is
  still open per the addendum) — the shared mip lane, not this organ.
* **"No RAM read when `f_detail_i`=0"** — structural (gated read enable);
  sim cannot see power. If it ever matters, the instrument is the fit's RAM
  enable report.
* **HPS-side conversions** (sun angle → s1.15 XZ, `lod_bias` from uv_shift) —
  exist as laws in this report and the amendment, not as code; instrument is
  the driver lane's unit test when the ABI word is minted.
