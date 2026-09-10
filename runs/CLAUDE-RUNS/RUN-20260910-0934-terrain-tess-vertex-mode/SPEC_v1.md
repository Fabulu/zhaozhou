# SPEC v1: TERRAIN.TESS vertex mode + index-triple mode

**Run ID:** RUN-20260910-0934
**Created:** 2026-09-10 09:34 UTC+02:00
**Status:** Active
**Previous Version:** N/A

---

## Objective

`reports/PROJECTION-ADOPTION-20260910.md` §7 items 1 and 2: give
`zhao_terrain_tess` a per-job MODE so the same block that walks triangles can
(1) emit the 81 lattice vertices of a job in index order with the geomorph
applied — `zref::terrain::detail::vertex_at` in hardware, using the block's
OWN lattice read, parent reads and blend — and (2) emit index TRIPLES
(ia, ib, ic) for every triangle it would have emitted, covering stitched and
coarse jobs the level-0 walker cannot. Default mode is today's behaviour,
bit-identical and cycle-identical. NOT a new block (the second-copy pattern).

Success = a new directed differential over the identity probe's case space
(3 lattices x 3 origins x 4 own levels x 4^4 neighbour levels x 6 morphs x 2
surfaces = 110,592 jobs) holds: vertex mode == `vertex_at` on every stride
vertex, triple mode == `tessellate`'s corners inverted to lattice indices, and
triples applied to the vertex-mode output rebuild `tessellate`'s triangles bit
for bit; `terrain_tess_directed` / `_random` / `_normals` pass with their
sources unchanged.

---

## Scope

**In Scope:**

- `fpga/rtl/terrain/zhao_terrain_tess.sv`: `job_mode_i`, vertex stream
  (2-deep credit-gated skid), reference stream (1-deep register), three new
  counters, one parameter (`IDX_W`), elaboration guard.
- `tests/terrain/tess_harness.hpp`: Driver learns the mode and collects the
  two new streams (existing test sources untouched).
- `tests/terrain/terrain_tess_modes_directed.cpp`: the differential.
- `tests/CMakeLists.txt`: registration.
- `fpga/rtl/synth/zhao_pair_tess_normals.sv`: tie the new ports off (mode 0).
- `fpga/rtl/prod/zhao_prod_top.sv`: regenerated (port change).
- `design/contracts/TERRAIN.TESS.md`: the new ports and the two chosen laws.
- `reports/TERRAIN-TESS-VERTEX-MODE-20260910.md`.

**Out of Scope:**

- Quartus fit (named, not run). Commit (owner's call). Any change to the
  geomorph blend cone. Deleting `zhao_terrain_topo` (recommendation only).
  TERRAIN.SEQ composition (§7 item 3). Stitch topology (owner stop condition).

---

## Constraints

- Mode 0 bit- and cycle-identical: the directed suite's throughput bound and
  every triangle must be unchanged; new logic enters mode 0's cones only as
  AND terms on REGISTERED mode bits (control), never in the blend arithmetic.
- Off-grid vertices at level >= 1 (vertices the level does not carry) are
  emitted as the PLAIN lattice vertex with `vtx_stride_o = 0` — `vertex_at`
  would read parents OUTSIDE the lattice at the patch edge (e.g. vi = 1,
  s = 2 -> parent -1), so "vertex_at over all 81 at every level" is
  undefined there; the brief's criterion is restated honestly.
- A job rejected in mode 0 (stitched + void) is rejected in every mode; the
  reject counter counts presentations.
- Counters new here SATURATE (spec/counters.md §4); the block's three
  pre-existing counters wrap and are not touched.
- Build: standalone verilator_bin, -std=gnu++17, absolute include paths,
  space-free Mdir under build-tessvtx/, rm -rf between builds.

---

## Don't Retry

- `python tools/quartus/gen_prod_top.py --help` REGENERATES the top (the tool
  ignores unknown flags and writes by default). Use `--check`. It was a no-op
  on the clean tree (git diff empty) — verified, not assumed.

---

## Open Questions

- Whether the VTX-mode scan skip (unstitched jobs need no cell-state scan
  because vertices exist regardless of solidity) should also apply to
  stitched jobs (the scan there exists only to reproduce the reject).
