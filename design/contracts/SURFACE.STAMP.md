# Contract — SURFACE.STAMP (Deterministic stamp engine)

> Ledger: `design/blocks.yml` · owner ZH-052 · phase 6 · maturity SPECIFIED

## Purpose and exclusions

The Scar Scribe stamp engine: it walks a patch's 64×64 surface sheet, decides
coverage from the `SurfaceStamp` command's circle or annulus, blends the source
strength into every covered texel and writes the result back to
`SURFACE.SHEET`. RTL: `fpga/rtl/surface/zhao_surface_stamp.sv`, with the blend
arithmetic in `fpga/rtl/surface/zhao_surface_blend.sv`.

**Out of scope, deliberately:** no height16 scar and no breach law
(`TERRAIN.BAKE` owns layers B and D — see **The seam that could not be closed**
below); no spline, textured-brush or noise-brush primitive (charter §12 lists
them, but the ABI opcode encodes a circle/annulus and inventing wire format for
the rest would be inventing ABI); no draw-time sheet sampling; no VRAM port.

## Law FOUND versus law CHOSEN

### (1) FOUND, ratified, already shipping

`zref::render::stamp_surface` (`reference/src/zrender/terrain.cpp`) is executed
today by the software console for ABI opcode `SurfaceStamp 0x0210`, and it is
pinned by committed goldens: `tests/render/render_golden.cpp` stamps a
crack ring into patch 44, and `tests/render/render_heightfield.cpp` checks an
annulus hole texel by texel. Every line of it is law here:

```
texel centre:  wx = ex0 + ((ex1 - ex0) * (2i + 1)) / 128        (j likewise)
radii:         r_outer2 = r*r
               r_inner  = rw > 0 ? max(r - rw, 0) : 0
coverage:      !(d2 > r_outer2 || d2 < r_inner2)
source byte:   src = strength >> 8
blend:         operation == 1 ? sat8((dst >> 1) + src) : max(dst, src)
tag:           written on every covered texel, OUTSIDE the if/else
scan order:    j outer, i inner (z-then-x)
```

Four consequences that look like defects and are **kept faithfully**, because a
capture must replay:

1. **The `/ 128` is C++ integer division — truncation toward zero**, which is
   not an arithmetic shift when `ex1 - ex0` is negative. An inverted envelope
   therefore differs from the shift form by one raw fx16 LSB, and this block
   truncates. `trunc128()` in the RTL does the `+1` correction explicitly.
2. **`r` is used SIGNED and squared**, so a negative radius covers exactly like
   its magnitude. Reproduced, not corrected.
3. **Both radii are INCLUSIVE** — the outer test is `>` and the inner is `<`, so
   a texel exactly on either rim is stamped.
4. **`strength >> 8` TRUNCATES**, where `spec/qformats.md` §2 would round a
   `unit8` conversion half-up. **Recorded as a deliberate divergence from the
   qformats family**: the reference does not round, and the goldens pin it.
   `0x01FF → 1`, not 2.

### (2) FOUND but never before implemented — the ops.yml blends

`design/ops.yml` carries five `stamp_mode` ops whose `implementation_blocks:`
list names this block. Their semantics are written (`dst = max(dst, src)`,
`sat16(dst + src)`, `sat16(dst - src)`, `dst = src`, and AGE's "decays toward
zero by a per-material age rate"), but the `zref::fieldir::stamp_*` functions
they name **do not exist anywhere in this tree**, and neither does the
`tests/differential/field_stamp_modes.cpp` they point at (checked 2026-08-19).
`zhao_surface_blend` and `zref::surface::blend_apply` are their first
implementation. Two notes:

- ops.yml says `sat16` while layer F stores **8 bits** per texel (charter §12,
  `terrain_rules` §2). The saturation is at 8 bits here because that is the
  width the destination has; a 16-bit saturate on an 8-bit store is not a
  choice, it is a line written before the layer table existed.
- **AGE's rate is CHOSEN**: a right shift of 0..7, so decay is exact, monotone
  and TERMINATES. *Rejected:* the qformats §2 `unit_mul` form
  `(dst*rate + 128) >> 8` — more expressive and the right shape once a
  per-material table exists, but it costs a multiplier and a rate of 255/256 has
  a **fixed point at 1**, so an aged scar lingers at strength 1 forever. The
  formal property `a_age_terminates` is the proof that the shift does not.

### (3) CHOSEN, with the rejected alternative recorded

**S1 — the blend select is two-level.** `cmd_blend_en_i = 0` is the RATIFIED
path: the blend is `cmd_operation_i == 1 ? DECAY_ACC : MAX`, the reference's own
branch **including its `else`**, so a capture carrying `operation = 7` replays as
a max stamp exactly as the software console replays it. `cmd_blend_en_i = 1`
selects an ops.yml mode from `cmd_blend_i`.
*Rejected:* widening the ABI `operation` byte's meaning to 0..4. That
reinterprets a frozen wire field; `capture_format.md` §1.2's rule is that
opcodes and field *sets* never change, and silently re-meaning a field's
*values* is the same betrayal by another route.

**S2 — the field brush delivers one result per VISITED texel** — all 4,096, in
the same j-outer/i-inner scan order — **not one per covered texel**. A result
whose texel is not covered is consumed and DISCARDED.
*Rejected:* results only for covered texels, which makes the consumption rate
depend on a coverage test that lives in *this* block, so `FIELD.SEQ.STAMP` would
have to reproduce this geometry bit-for-bit or the pair deadlocks.
(`design/contracts/TERRAIN.PATCH.md` chose the same discipline for its field
lanes, chosen law 2, for the same reason.)
`tag_op` unpacks as `tag = [7:0]`, `blend = [10:8]`, `age_shift = [14:12]`;
`spec/form/field-ir.md` §7.1 names the lane but no layout is written anywhere,
so the packing is chosen. The field's `strength` is a **u16 carrying the ABI's
format**, so ONE `>> 8` conversion serves both paths — *rejected:* a pre-reduced
u8, which puts a second, differently rounded conversion upstream and guarantees
the two paths drift. The `emissive` output lane of field-ir §7.1 is **DROPPED**:
layer F has two bytes, charter §12 spends both, and a third would change the
frozen 8,192 B layer size.

**S3 — `stamp_results` carries `{texel, tag, strength_after, strength_before}`.**
`TERRAIN.BAKE` turns stamps into layer-B height16 scars and needs the DELTA, not
just the new value; sending `before` costs eight wires and saves BAKE a second
read port onto the sheet.
*Rejected:* emitting only the new value and letting BAKE re-read — a second
reader on a store whose whole rate budget is one texel per clock.

**S4 — an ACQUIRE that overflows aborts the whole stamp before any write.** The
SURFACE.SHEET ledger note is "overflow rejects the stamp, never partial-writes";
this block enforces it **structurally** — the texel loop cannot start until the
ACQUIRE has answered, so there is no ordering in which a write escapes.
*Rejected:* starting the loop optimistically and relying on SURFACE.SHEET to
drop the writes. Same sheet contents, but a wrong `surface_texels_touched`, a
wrong `stamp_results` stream, and 4,096 cycles burned to accomplish nothing.
Measured: a rejected stamp terminates in under 200 cycles.

**S5 — `brush` is NOT an input.** `commands.zidl` carries
`handle32[brush] brush` and charter §12 lists textured and noise brushes, but
**nothing in this tree defines a brush page's format** and `stamp_surface` never
reads the field. A port wired to nothing is worse than an absent one — it looks
like a contract. When the brush page lands, S2's field path is where it arrives.
Recorded, not hidden.

## The seam that could not be closed

The obvious next link is `SURFACE.SHEET → TERRAIN.PATCH`, because
`TERRAIN.PATCH` takes `baked_scars`. **It was not wired, and building it would
have meant inventing law:**

- `TERRAIN.PATCH`'s `scar_i` is **layer B** — a height16 per-vertex bake delta on
  the 33×33 lattice (`design/contracts/TERRAIN.PATCH.md`,
  `spec/terrain_rules.md` §2). `SURFACE.SHEET` owns **layer F** — a 64×64
  `{tag u8, strength u8}` appearance sheet. Different layers, different extents,
  different elements, different owners: `terrain_rules` §7 says "B (scar)
  written only by TERRAIN.BAKE … F written only by SURFACE.STAMP".
- The block that converts one into the other is **`TERRAIN.BAKE`**
  (`design/blocks.yml`: `inputs: [stamp_results]`, `outputs: [baked_scars]`,
  upstream `SURFACE.STAMP`, downstream `TERRAIN.PATCH`) — and it is **phase 7**,
  maturity SPECIFIED, with no RTL in `fpga/rtl/terrain/` and no reference model.
  `zref::terrain::bake_dig` bakes from a `DigStamp` paraboloid, not from a
  surface sheet; there is **no ratified strength → height16 mapping anywhere in
  the tree**.

**What would be needed:** `TERRAIN.BAKE` in RTL, and before that a ratified
mapping from a layer-F texel `{tag, strength}` (or from a `stamp_results`
delta) to a layer-B height16 contribution at a lattice vertex — which is a
gameplay/format decision, not an implementation one. Until that exists,
`stamp_results` is wired out to the edge of `surface_stamp_chain.cpp` and
checked for exactly what BAKE will need. Fabricating the conversion would have
produced a green test asserting an invention.

## Clock and reset semantics

Single clock `clk`, `gpu` domain, no CDC. Reset `rst_n`, async assert / sync
release. On reset: `SIdle`, cursor 0, both pipeline stages empty, both counters
0, `idle_o` high, no pulse outputs asserted.

## Input and output packet layouts

### `dispatch` (in, ready/valid) — one `SurfaceStamp`

`cmd_handle_i` (32, the patch handle = the sheet identity), `cmd_operation_i`
(8, the raw ABI byte), `cmd_tag_i` (8), `cmd_strength_i` (16),
`cmd_tx_i`/`cmd_ty_i` (32 signed, the transform2fx **translation**),
`cmd_radius_i`/`cmd_ring_width_i` (32 signed, fx16 world metres),
`cmd_env_x0_i`/`z0`/`x1`/`z1` (32 signed, the patch envelope),
`cmd_blend_en_i` + `cmd_blend_i` (3) + `cmd_age_shift_i` (3) — S1/AGE,
`cmd_field_en_i` — S2, `cmd_src_id_i` (16).

The transform2fx **2×2 rotation is not a port**: a circle or annulus is its own
rotation, and `stamp_surface` says so in a comment and never reads `r00..r11`.

### `stamp_field_results` (in, ready/valid) — S2

`fld_tag_op_i` (32), `fld_strength_i` (16). One record per visited texel.

### to `SURFACE.SHEET`

The request port (`req_valid_o`/`req_ready_i`, `req_op_o`, `req_handle_o`,
`req_texel_o`, `req_src_id_o`) carries the ACQUIRE and then one READ per covered
texel; `sheet_pages` (`pg_valid_i`/`pg_ready_o`, `pg_status_i`,
`pg_strength_i`) returns them; the write port (`wr_*`) writes them back with
both byte enables asserted — `stamp_surface` writes the tag on every covered
texel, outside its if/else, even for a blend that leaves strength at 0.

### `stamp_results` (out, ready/valid) — S3

`res_texel_o` (12), `res_tag_o` (8), `res_strength_o` (8, after the blend),
`res_before_o` (8, before), `res_src_id_o` (16).

### status

`stamp_done_o` and `stamp_rejected_o` (1-cycle pulses), `idle_o`.

## Backpressure rules

Full ready/valid on every channel. `fld_ready_o` is deliberately **not** a
function of `fld_valid_i` (a ready that waits on its own valid deadlocks against
a producer that waits on ready); the dependence runs the other way, with the
sheet read's `req_valid_o` gated on `fld_valid_i`. `pg_ready_o` likewise does
not depend on `pg_valid_i`.

Stage 2 tracks the write and the result acceptances independently with two
sticky flags, so either consumer may stall without the other.
`surface_stamp_directed` stalls all three channels at once and asserts each
stall path was actually taken, that the run took longer, and that the result is
byte-identical to the unstalled run.

## Memory ownership

**None.** No VRAM port, no cache, no M10K. Every texel comes from and returns to
`SURFACE.SHEET` over the ports above. The block's whole state is two pipeline
stages, one 13-bit cursor, the per-stamp constant registers and two counters.

## Q formats and rounding

- `strength` (u16) → the source byte: `>> 8`, **truncating**. A deliberate
  divergence from `spec/qformats.md` §2's round-half-up `unit8` conversion,
  because the reference truncates and the goldens pin it. See FOUND (1)(4).
- `(ex1 - ex0) * (2i + 1) / 128` → **truncation toward zero**, not an arithmetic
  shift. See FOUND (1)(1).
- the distance compare is **exact integer arithmetic with no rounding at all**
  inside the stated domain.
- the blends saturate at 8 bits and round nowhere; AGE's shift discards low bits
  without rounding, which is what makes it exactly monotone.

**The stated input domain, and why it is not the whole word.** The reference
computes `dx*dx + dz*dz` in `int64`; for arbitrary int32 envelope and transform
words `|dx|` reaches 2³³ and `dx*dx` overflows `int64` — the reference has left
its own arithmetic, so a differential out there compares two overflows rather
than testing this block (the identical argument
`design/contracts/TERRAIN.NORMALS.md` makes for its own domain). **The domain is
±4,096 world metres** (fx16 raw magnitude ≤ 2²⁸) on every envelope corner, on
the transform translation and on radius/ring_width. Inside it `|dx| < 2³⁰`,
`d2 < 2⁶¹` and `r*r < 2⁵⁷`, so the 64-bit datapath is EXACT and matches the
reference bit for bit. 4,096 m is 64 canonical 64-m patches from the island
datum in each direction, far outside any island (`terrain_rules` §1.5), so the
bound costs nothing real.

Every comparison in the geometry is signed on both sides. A Verilog compare goes
unsigned if *either* operand is, and that trap already cost this tree 29
vanished tiles in `GEOM.BINNER` (`design/contracts/GEOM.CLIP.md`).

## Latency (fixed or variable)

**Variable**, as the ledger records: one ACQUIRE round trip, then 4,096 cursor
steps (one per texel, covered or not), then a two-deep drain. A rejected stamp
returns without entering the loop.

## Target throughput

Ledger: **1 stamp texel per clock**. **Met, measured:** a full-cover stamp
(radius 32 m over the ±8 m envelope, all 4,096 texels written) takes **4,102
cycles** — 4,096 texels plus the acquire and the drain.

## Overflow and malformed-input behaviour

- **Residency overflow** → S4: the stamp is abandoned before any write.
  `stamp_rejected_o` pulses, neither counter moves, and the sheet is
  byte-identical afterwards.
- **A stamp entirely outside the envelope** → completes normally, writes zero
  texels, emits zero results.
- **radius 0** → covers exactly the texel whose centre is the translation (both
  radii inclusive).
- **negative radius** → covers like its magnitude (FOUND (1)(2)).
- **inverted envelope** (`x1 < x0`) → the truncating divide branch; the only
  route to it.
- **`ring_width >= radius`** → `r_inner` clamps to 0, i.e. a filled disc.
- **blend code 7** (unassigned) → falls to MAX, matching the shape of the
  reference's own `else` on the operation byte.

## Counters and traces

`surface_stamps_o` counts completed stamps — a **rejected** stamp does not
count. `surface_texels_touched_o` counts retired texel writes. Both are 32-bit
and saturate rather than wrap; only reset clears them. `source_ids: true` is
honoured: `cmd_src_id_i` rides the sheet request port, the sheet write port and
every `stamp_results` record.

## Scalar reference function

`zref::surface::stamp_apply` / `stamp_apply_field` / `blend_apply` in
`reference/include/zref/zref_surface.hpp`.

**The ledger's `reference_model` was AMENDED, from `zref::SurfaceStamp` to the
symbol above.** That is a deviation from "honour the ledger entry" and it is
recorded as one, in the same shape `design/contracts/TERRAIN.PATCH.md` records
its own. The reason: `zref::SurfaceStamp` defines nothing anywhere in this tree
(`zhao_abi::ZhCmdSurfaceStamp` is the wire struct, not a model), while the law
this block implements has been executing for waves under the name
`zref::render::stamp_surface`. Citing a phantom would have been exactly the
`zref::CmdDma` failure rule V17 exists to catch.

For the ratified path this is a
**VIEW** onto `zref::render::stamp_surface`, not a second implementation:
`surface_stamp_directed`'s first test runs 400 randomized commands (including
inverted envelopes, negative radii and annuli) through `stamp_surface` and
through the decomposition on an identically pre-dirtied sheet and requires all
4,096 texels to agree bit for bit, and asserts that the sweep actually covered
texels so the agreement is not vacuous. Without that cross-check the file would
be a second implementation, which charter §29-6 forbids.

## Directed tests

`tests/surface/surface_stamp_directed.cpp` — the faithfulness cross-check
above, then: an empty sheet and a single stamp; the truncating `>> 8`; the
`render_heightfield` annulus fixture texel by texel; three overlapping strikes
(the shape of the `terrain-scars` gallery render); the four sheet corners and an
envelope-edge clip checked on both sides; a stamp entirely outside; radius 0;
negative radius; an inverted envelope; saturation to 255 through repeated
decay-accumulate and to 0 through SUB; persistence across five frames with a
non-covering stamp proving nothing else disturbs the scar; residency rejection;
backpressure on all three channels with the stall paths asserted taken; all six
blends plus the ABI's `operation = 7 → MAX` else-branch; the field brush; the
`stamp_results` stream compared record for record and asserted strictly
ascending; and the counters and throughput.

## Randomized differential tests

`tests/surface/surface_stamp_random.cpp`, two lanes against
`zref::render::stamp_surface` through the proven view:

- **Lane A, gameplay-shaped.** The canonical 64 m battlefield patch
  (`terrain_rules` §1.3: 32×32 cells at 2.0 m pitch), radii 0.25..12 m centred
  on or near the patch, the ABI's two operations, real tag bytes.
- **Lane B, at the domain limit.** Envelope corners and translations out to
  ±4,096 m, inverted envelopes, negative radii, radii that swallow the whole
  sheet and radii that miss it, all seven blend codes, and the field brush.

A single uniform lane would land almost entirely in lane B's regime and would
pass while the arithmetic was useless for real terrain.

**Coverage is asserted, not printed**, and three of the assertions exist because
they *failed* first:

- Lane A must reach partial coverage, full coverage, zero coverage, annuli, the
  255 rail, both ABI operations, a residency rejection, backpressure, and — added
  after the mutation sweep — a texel **exactly on the outer rim** and one exactly
  on the inner rim.
- Lane B must reach inverted envelopes, negative radii, the field brush, full
  and zero coverage, the 0 rail, and at least six of the seven blend codes.

The rim-exact case is **constructed**, not rolled: on the canonical envelope two
texel centres in a row are an exact whole number of metres apart, so a stamp
centred on texel *i₀* with radius *k* metres puts texel *i₀+k* exactly on the
outer rim. The full-coverage radius and the residency rejection are likewise
**scheduled by stamp index** rather than rolled, because a 1-in-20 roll over 60
stamps is a coin flip on whether the case is sampled at all.

## Composition

`tests/surface/surface_stamp_chain.cpp` runs this block against the **real**
`SURFACE.SHEET`, then reads the sheet back through SHEET's own read port and
hands it to `zref::render::sample_sheet` — the **draw-time consumer**, whose
world→texel mapping is *floor across the envelope*, independent of the stamp's
texel-centre rule. A u/v transposition or a dropped 64× survives both standalone
suites and dies there. Cases: one stamp end to end; the sampler agreement over
all 4,096 texels at two sample points each; persistence over four frames with a
re-acquire every frame; residency overflow writing nothing and terminating in
under 200 cycles; the real 4,096-cycle clear sweep stalling the real stamp so a
recycled slot is provably clean; two patches resident at once; and 24 randomized
stamps under `stamp_results` backpressure.

## Formal properties

`tests/formal/surface_blend.sby` + `tests/formal/surface_blend_fv.sv`, on
`zhao_surface_blend` — **the exact module this block instantiates, and the only
instance of it**. The property is **scope-total, not bounded**: the harness is
one `always_comb` and its 22 free input bits (mode 3, dst 8, src 8, age_shift 3)
are the module's whole input space, all 4,194,304 of them, so depth 2 is the
full state space rather than a horizon.

Nine properties, written against the arithmetic in wide lanes rather than
against a restatement of the case arms: REPLACE is the source; MAX (and the
ABI's operation-0 STAMP) is exactly `max(dst, src)`; DECAY_ACC is exactly
`min(255, (dst>>1)+src)`; ADD and SUB are exactly `min(255, dst+src)` and
`max(0, dst-src)`, two-sided so both the wrap and the early clamp die; AGE is
exactly `dst >> shift`; SUB and AGE can never brighten; ADD and MAX can never
darken — **and DECAY_ACC is deliberately excluded from that**, because
`(dst>>1)+0` halves, so asserting monotonicity there would assert something
false about the ratified blend; and AGE with a non-zero shift strictly decreases
a non-zero destination, which is the theorem that justifies the shift over the
rejected `unit_mul` form. The cover task is load-bearing and passes: both rails
actually fire, MAX takes each branch, DECAY_ACC actually halves, AGE actually
reaches zero.

**Not asserted, and why:** "the result never leaves the 8-bit field". `out_o` IS
eight bits wide, so that is a tautology about a port declaration rather than a
theorem about the arithmetic — unlike `zhao_raster_blend`, whose internal lane
is wider than its output.

**What is NOT proved formally:** the coverage geometry, the ABI
operation → blend mapping, the residency handshake, the pipeline, the counters
and the results stream. Those are the differential lanes' and the mutation
sweep's job, and they are not bounded arithmetic cores.

## Synthesis / resource ceiling

Budget group `geometry_mantle`. **Estimate only — this block has not been
synthesized:** no Quartus fit, no timing closure, no device numbers, and it is
deliberately not in `fpga/files.qip`.

Shape: **two 64×64 signed multipliers** for `dx²` and `dz²`, evaluated every
cycle in the cursor path; two more for `r*r` and `r_inner*r_inner`, evaluated
**once per stamp** at command accept; two small 41-bit multiplies for the texel
centres; three wide (64/72-bit) comparators; the `zhao_surface_blend` instance
(one 9-bit adder, one 9-bit subtractor, one comparator, one barrel shifter); and
roughly 400 flops of per-stamp constants, two pipeline stages and two counters.

**The two per-cycle squares are the cost, and they are honestly oversized.** The
stated ±4,096 m domain bounds `|dx|` at 2³⁰, so a 31×31 multiplier and a 62-bit
compare would suffice; the datapath is written at 64 bits to match the
reference's `int64` exactly rather than to be small. Narrowing it is a real
optimisation and a real risk (it changes what happens outside the domain), which
is why it is recorded here rather than done blind. `dx` also changes only with
`i` and `dz` only with `j`, so an incremental form exists — the truncating
divide is what makes it non-obvious, and it was not attempted.

## Integration capture cases

None yet. The captures that exercise the sheet today go through the software
console; a fabric-side capture arrives with the `TERRAIN.BAKE` seam above.

## Mutation evidence

Each mutation was applied alone, rebuilt, and the **test binary re-hashed** to
prove the relink actually happened before the result was believed. Every one
changed the hash.

| # | Mutation | Caught by |
|---|---|---|
| 1 | the write texel index is transposed (`{s2_texel[5:0], s2_texel[11:6]}`) | `surface_stamp_directed`, `surface_stamp_random` (both lanes), `surface_stamp_chain` |
| 2 | decay-accumulate REPLACES instead of accumulating | all four |
| 3 | ADD's saturation removed | directed + both random lanes; **not** the chain, which only drives the ABI ops |
| 4 | decay-accumulate's saturation removed (the ratified path) | all four |
| 6 | the outer rim made exclusive (`>` → `>=`), clipping the edge texel | directed only at first — **see the finding below** — then also both random lanes |
| 8 | `stamp_results` loses the pre-blend strength (the delta BAKE needs) | directed + both random lanes |
| 9 | SUB's saturation removed, in `zhao_surface_blend` | `formal_surface_blend` **and** directed + both random lanes |

(Mutations 5 and 7 are `SURFACE.SHEET`'s and are recorded in that contract.)

**Finding, recorded rather than quietly fixed.** Mutation 6 was caught only by
the directed suite. Both random lanes stayed green, because a uniformly random
metre-scale radius essentially never makes `d2 == r*r` exactly, so neither lane
could ever sample the boundary the mutation moved. A green lane that sampled
nothing is not evidence. Lane A now constructs the equality and asserts it was
reached, and the mutation now fails both random lanes as well. Two other lane-A
counters (full coverage, residency rejection) turned out to be coin flips on a
1-in-20 roll and read zero on the very next seed shift; both are now scheduled
by stamp index. The same lesson, three times, in one file.

## Notes

Capture-exact: identical inputs replay to identical sheets (§20 discipline).
The blend vocabulary is the five stamp modes of `design/ops.yml` plus the two
ABI operations, and which of those is *ratified* versus *first implemented here*
is spelled out in **Law FOUND versus law CHOSEN** above.
