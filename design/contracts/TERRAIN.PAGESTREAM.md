# Contract — TERRAIN.PAGESTREAM (a resident page slot, as a lattice)

> Ledger: `design/blocks.yml` · gpu clock · maturity UNIT_VERIFIED
> RTL: `fpga/rtl/terrain/zhao_terrain_pagestream.sv`
> Reference model: `zref::terrain::page_lattice` — `reference/include/zref/zref_terrain_page.hpp`
> Tests: `tests/terrain/pagestream_rtl_directed.cpp` (+ `tests/terrain/tb_pagestream.sv`)

## Purpose

Read layers **A**, **B** and **C** out of one resident `TERRAIN.PAGE_POOL` slot
and emit the patch's **33×33 height lattice**, one vertex at a time, row-major,
with all three planes present on the same beat.

## The hole it fills, and how that hole moved

`tests/terrain/world_composed_directed.cpp` phase C has reported since
2026-09-07 that the assembled world layer **never calls a loaded page
RESIDENT**: a claim sets `mips_stale`, the loader's `fin` parks the entry in
`ST_MIPGEN`, and only a *second* completion moves it to `RESIDENT_CLEAN` — the
only state a lookup hits on. Eight pages were fetched, CRC-verified and
byte-identical in their slots while `resident_o` stayed at **zero**.

That finding's cause has already moved once, which is why this is a block and
not a wire. The first description said `zhao_terrain_mipgen` had no slot,
generation, epoch or completion port. True when written; false within hours,
because those ports landed the same day. The hole is one step further back:
`TERRAIN.MIPGEN` consumes a lattice through `fine_valid_i`, `TERRAIN.PATCH`
consumes the same lattice through `base_i`/`scar_i`/`bottom_i`, and nothing in
`fpga/rtl` turned a resident slot into either stream.

## Why the three planes are read together

`spec/terrain_rules.md` §2, Island Patch v1, page-relative:

| layer | extent | element | bytes | offset |
|---|---|---|---:|---:|
| A top base height | 33×33 | height16 | 2,178 | 64 |
| B top scar delta | 33×33 | height16 | 2,178 | 2,242 |
| C bottom height | 33×33 | height16 | 2,178 | 4,420 |

`TERRAIN.PATCH`'s compose lane takes `base_i`, `scar_i` and `bottom_i` on one
beat, and the three planes are 2,178 bytes apart. So three cursors advance in
lockstep, each with its own 64-byte staging buffer, and a vertex is emitted when
all three have their sample.

The alternative — read A whole, then B, then C — needs 1,089 × 16 bits per plane
held across the other two passes. That is **17,424 bits**; this arrangement
needs **1,536**.

## A sample never straddles anything, and that is arithmetic

`TERRAIN.WRITEBACK` had to re-lane layer F because `10,694 mod 8 = 6`. The same
worry applies here and the answer is different, so it is written out rather than
assumed:

- all three offsets are **even** and the element is 2 bytes, so sample *k* of
  plane *P* sits at page byte `P + 2k`, also even;
- an even byte address holds a 16-bit value entirely inside one 64-bit word —
  the lane is 0, 2, 4 or 6, and 6 + 2 = 8 fits exactly;
- the same at 64-byte granularity — the lane is 0..62, and 62 + 2 = 64 fits the
  staging buffer exactly.

So the buffer's coverage test is a plain address compare, not a re-laner. B and
C still *start* mid-burst (`2242 mod 64 = 2`, `4420 mod 64 = 4`), which only
means their first burst carries 62 and 60 useful bytes — handled by the same
compare without knowing it.

Enforced at elaboration in the RTL and by `static_assert` in the reference
header, because a layout revision that made an offset odd would silently return
the two halves of two neighbouring vertices.

## Measured cost

105 bursts per lattice, derived rather than observed and then checked against
the fit of the derivation: plane *P* needs one burst per distinct value of
`(P + 2k) >> 6` over *k* in 0..1088, i.e.
`floor((P + 2176)/64) − floor(P/64) + 1` = 35 + 35 + 35.

3,544 clocks per lattice with an unstalled fabric (2-cycle read latency),
against `TERRAIN.PAGELOADER`'s 6,726 for the page load that precedes it.

## Exclusions

**It does not compose.** `spec/terrain_rules.md` §3.4 —
`compose_top = max(fx(base) + fx(scar), fx(bottom))` — is `TERRAIN.PATCH`'s law
and `zref::terrain::compose_vertex` its oracle. This block hands over the three
planes untouched. A second implementation of that saturating clamp is a second
thing to keep in step with §3.4, and its first divergence is ground subtly in
the wrong place with every counter agreeing.

It does not decide residency, does not publish, does not verify the page CRC
(`TERRAIN.PAGELOADER` did that before the page was called loaded), and does not
place the lattice in the world — `wx`/`wz` come from the island directory and
the patch envelope, not from these three planes.

## OPEN RULING — which surface does MIPGEN decimate at load time?

Owner ruling **T8** gives the mip law exactly —
`mip17[i,j] = fine33[2i, 2j]`, `mip9[i,j] = fine33[4i, 4j]` — and says a page
becomes RESIDENT only after *"resident mips complete"*. **It does not say which
surface `fine33` is at load time**, and the candidates are not equivalent:

- **layer A alone** — a scarred patch would show unscarred ground at coarse LOD;
- **`compose_top`** (A + B clamped at C) — correct-looking, but it is §3.4
  arithmetic, which this block deliberately does not own.

The choice is **not made here**. This block emits all three planes and the
selection belongs in whatever adapter feeds `TERRAIN.MIPGEN`, where it is one
visible wire rather than a decision buried in a datapath. Inventing it is
exactly the fault this tree keeps recording.

## Ports

| field | width | meaning |
|---|---|---|
| `j_valid_i` / `j_ready_o` | 1 | one job = one lattice |
| `j_slot_i` | `SLOTW` (11) | pool slot; **one bit wider than the pool needs**, so 1,024 arrives as a refusal rather than as slot 0 |
| `j_gen_i` `j_epoch_i` `j_src_id_i` | 8 / 32 / 32 | identity, returned unaltered |
| `guard_req_o` / `guard_rsp_i` | — | MEM.GUARD read client, `len` 64 |
| `beat_valid_i` `beat_data_i` `beat_last_i` | 1 / 64 / 1 | eight beats per accepted request |
| `v_valid_o` / `v_ready_i` | 1 | one vertex per beat |
| `v_base_o` `v_scar_o` `v_bottom_o` | signed 16 | layers A / B / C, height16, **untouched** |
| `v_vi_o` `v_vj_o` | 6 | 0..32; carried for the subpatch mask, **not** a second source of truth |
| `v_first_o` `v_last_o` | 1 | vertex 0 and vertex 1,088 |
| `done_*` | — | one job, one completion, always |

## Backpressure and protocol

Ready/valid throughout. The guard handshake is the **two-cycle** one
`zhao_mem_guard.sv` states outright: `rsp.ready` is a **level** (`!fwd_active`),
`rsp.ok` a **pulse** the cycle after the accept. They are never high together,
and a client that tests them in one arm reads every pass as a denial. This block
waits for the level, then the pulse, and only then arms its beat counter — so a
request the guard is about to refuse never starts a burst count.

**One job, one completion, always** — the rule `TERRAIN.PAGELOADER`'s contract
states, for the same reason: a refusal that produced silence would leave the
directory's entry parked forever with nothing counted, which is the exact
failure this block exists to remove.

## Verdicts

| code | meaning |
|---:|---|
| 0 | ok |
| 1 | slot ≥ `REGION_SLOTS` |
| 2 | job epoch ≠ `cfg_epoch_i` |
| 3 | MEM.GUARD refused a read |
| 4 | a burst ended early |

A short burst **faults** rather than being tolerated: the staging buffer would
then hold a mixture of this page and the last one, which reads as terrain.

## Scalar reference function

`zref::terrain::page_lattice` (`reference/include/zref/zref_terrain_page.hpp`),
with `zref::terrain::LatticeVertex` as the record and
`zref::terrain::page_h16` as the element decode.

**The oracle is the ORDER and the VALUE together, and neither alone is enough.**
`TERRAIN.MIPGEN`'s fine port carries no coordinate at all and derives everything
from scan position, so a consumer trusting a coordinate against a
differently-ordered scan would decimate the wrong vertices while every count
agreed.

## Evidence

Every way this block can be wrong produces exactly 1,089 vertices: the wrong
slot, the wrong lane, the wrong plane, the wrong scan order, a stale staging
buffer. All of them render as terrain and none of them move a count.

So `pagestream_rtl_directed.cpp` compares the **whole stream, vertex by vertex
and field by field**, with a fixture built so that each confusion produces a
different number — each plane filled from a distinct injective function of the
vertex index, with disjoint ranges; **scars negative**, because a zero-extending
read would pass every test drawn from positive values; layers D..H and the
header filled with values that are not plausible heights, so a cursor that ran
past its plane cannot be mistaken for terrain; and **neighbouring slots poisoned
with different constants**, so *"read the slot you were told to"* is checkable
rather than assumed. The two slots compared in phase C differ at **every** one
of the 1,089 vertices, so that check cannot pass by reading the wrong page.

The **real** `zhao_mem_guard` watches the DUT's request wires as an observer:
`shadow_ok == shadow_req` with `shadow_viol == 0` **and** `shadow_fwd ==
shadow_req`, because a guard that passed every request and forwarded none would
satisfy the first two and still be broken.

Four stall patterns, one of them mostly-ready — a sibling block's differential
passed a 15,625-case sweep and still missed a dropped answer because every phase
held ready high, and the pattern that found it was three-in-four. All four logs
must be identical.

Every verdict is **fired**, and a good lattice is streamed *after* the four
faults: a block that faulted correctly and then never streamed again would pass
every other check in the file.

51 checks, 0 failures.
