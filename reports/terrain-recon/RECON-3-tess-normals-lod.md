# Terrain recon 3: tess / normals / lod, and where the 31 MHz actually is

2026-09-09, rescue phase. Read-only. Tree clean at inspection.

## Multiplier sites, counted not assumed

**`zhao_terrain_tess.sv`** -- ONE wide multiply, the geomorph blend at `:599`,
17b x signed 34b. Plus four NARROW function-call sites: `span_mask` (`:356`,
called twice at `:364-365`) and `proj_t` (`:448`, called twice at `:461-462`).

`span_mask`'s header at `:317-333` is worth reading: it records that this
replaced a **128-multiply-site double loop** (four comparisons x 8x8 cells) with
two sites. Somebody already did this domain's biggest arithmetic collapse.

The recon could **not** reconcile the ledger's 6 DSP to one wide site and
declined to guess a Cyclone V mapping rather than invent one. That is the right
call and it leaves a real open question.

**`zhao_terrain_normals.sv`** -- ONE nonconstant multiply, `m_p = m_a * m_b` at
`:203`, 33x33->66b, **shared across six operand-mux cases** by an `mseq`/`m_busy`
sequencer at `:239-330`. Its own comment says so.

**`zhao_terrain_lod.sv`** -- ONE shared multiplier, `mul_p = mul_a * mul_b` at
`:273`, shared between the square and eval steps.

**So two of the three blocks have ALREADY done the thing the rescue roadmap asks
for**: one physical multiplier, sequenced across clients, operands muxed. They
are the existence proof that the pattern works in this codebase, and they should
be the model the geometry domain copies -- not a new invention.

## The `m_p_q` repair is at HEAD

`zhao_terrain_normals.sv:182-184, 201-203, 250-252, 296-330`. Registered product,
accumulation one cycle later. `git hash-object` on the working file returns
`4975a0bf9bd9d71cf239bc76803552ab2c110f30`, matching the blob cited in the brief
exactly -- independent confirmation rather than a reading of the diff.

## The geomorph blend has `project_core`'s unregistered shape

`zhao_terrain_tess.sv:592-608`: `m_dab -> m_half (rescale1) -> m_hc (fx_add_sat)
-> m_d -> m_prod (MULTIPLY) -> m_step (rescale16) -> m_y (fx_add_sat)` is **all
combinational wire**, no register between the multiply and the following
add-and-saturate, and `last_y` is captured straight into `o_cy`/`o_by` in one
edge (`:824, 841, 846`). Multiply-then-add-then-saturate in a single cycle, the
same shape flagged in the projection core.

## THE FINDING: the 31 MHz is NOT the arithmetic

`reports/synthesis/blockpaths/zhao_pair_tess_normals.{fit,setup.summary}.rpt`,
dated 2026-09-07, carry a `sources.sha256` that **exactly matches the current
working-tree SHA256 of all three RTL files**. So this receipt already describes
the `m_p_q`-repaired tree -- the brief's "no fresh receipt" caveat was written
before this fit ran and is wrong.

And the worst path is not where anyone assumed:

    worst slack -20.21 ns
      launch  wrapper lat_mem (altsyncram) PORT_B_WRITE_ENABLE_REG
      capture zhao_terrain_tess:u_tess | vy[0/1][*]
    second-worst family: vy[*] -> o_cy[*]

**Not the normals multiply chain. Not the geomorph multiply chain.** It is a
memory write-enable register driving tess's internal `vy[]` array, and then that
array driving the outputs. The arithmetic anyone would have optimised is not the
critical path, and the launch point is in the **bench wrapper's** memory.

Fit row: 1,574 ALM / 1,577 reg / 9 DSP / 1 RAM block / 2,048 bits.

**And the two numbers disagree.** `zhao_terrain_tess.sv:309-311` cites **32.42
MHz** for this pair with "TESS->TESS family at 40.11 MHz from 1,803 paths"; the
ledger says 31.10. The summary has 1,812 rows. Somebody must reconcile these
before either is quoted -- they are the same fit.

## Arrays

- `lod.sv:253-256`: `lvl[16]` 2b, `mrp[16]` 17b, `hld[16]` 8b, `sid[16]` 16b =
  688 bits total. Per-patch decision store, indexed access -- candidate by shape,
  far too small to be worth an M10K.
- `tess.sv:289,474,546`: `vx/vy/vz/vh[3]` (384b), `tv_i/tv_j[3]`, `mc[3]` --
  per-triangle working registers touched every clock. **Not candidates.** But
  `vy[]` is on the worst path twice, so its SHAPE is a timing question even
  though it is not a memory question.

## Vertex reuse

The 1,089-vs-6,144 ratio is stated in `TERRAIN.PROJECT.md:208-209` and
`workloads.yml:239-256`, describing the DOWNSTREAM project block. `tess` itself
asserts it nowhere -- but its behaviour is consistent: it re-fetches per triangle
corner via `lat_req_o` and holds only the current triangle, so 16 jobs x 128
triangles x 3 corners = 6,144 lattice reads for 1,089 unique positions.
**Structural corroboration of the arena's premise from the producer side.**
