# Contract — TERRAIN.MIPFEED (the adapter that makes a loaded page into ground)

> Ledger: `design/blocks.yml` · gpu clock · maturity UNIT_VERIFIED
> RTL: `fpga/rtl/terrain/zhao_terrain_mipfeed.sv`
> Reference model: `zref::terrain::mipgen` — `reference/include/zref/zref_terrain.hpp`
> Tests: `tests/terrain/world_composed_directed.cpp` phases C and C2

## Purpose

Sequence `TERRAIN.PAGESTREAM` and `TERRAIN.MIPGEN` so that a page whose bytes
have landed acquires its mips and produces the **second completion**
`TERRAIN.RESIDENCY` needs before it will call the page RESIDENT.

`zhao_terrain_residency_v2` will not publish a page on one completion: a claim
sets `mips_stale`, the loader's `fin` moves the entry to `ST_MIPGEN`, and only a
second `fin` moves it to `RESIDENT_CLEAN` — the only state a lookup hits on.
MIPGEN produces that completion and consumes a lattice; PAGESTREAM produces a
lattice and consumes a page slot. This is the sequencing between them.

It is a block rather than a few wires in a top because it makes a **decision** —
which plane is which surface — and a decision that lives in a wiring diagram is
a decision nobody can find.

## Two passes, not a buffer

MIPGEN wants `FINE*FINE` samples per surface, surfaces in order, on one 16-bit
port. PAGESTREAM emits `FINE*FINE` vertices carrying **three planes each**. One
lattice pass therefore produces both surfaces' data at once and the port can
take only one of them.

The choice is between buffering 1,089 × 16 bits of the surface not being sent —
17,424 bits, which is exactly the buffering PAGESTREAM was arranged to avoid —
or streaming the page **twice** and selecting a different plane each time. The
second pass costs about 3,544 clocks; the page load that preceded it cost 6,726,
and neither is on the frame's critical path because the sequencer never waits
for a load to complete.

MIPGEN is started **once**, before the first pass, because its surface counter
runs across both. A `start` between the passes would send surface 1's samples
into surface 0's mip and count an abort — which is the shape of fault that
renders as terrain. `mg_aborts == 0` is the check.

## OPEN RULING — which plane is surface 0

Owner ruling **T8** gives the mip law exactly —
`mip17[i,j] = fine33[2i, 2j]`, `mip9[i,j] = fine33[4i, 4j]`, *"top and
bottom"* — and says a page becomes RESIDENT only after *"resident mips
complete"*. **It does not say what "top" is at load time**, and the readings are
not equivalent:

- **layer A**, the authored base height. What this block sends today. A page
  carrying scars would show *unscarred* ground at coarse LOD, and the seam
  between a scarred fine patch and its coarse neighbour would open.
- **`compose_top`**, `max(fx(base) + fx(scar), fx(bottom))` per
  `spec/terrain_rules.md` §3.4. Correct-looking, and it is `TERRAIN.PATCH`'s
  arithmetic — so the honest implementation is not to compute it here but to
  take surface 0's samples **from TERRAIN.PATCH's compose lane** instead of from
  the page. That is a rewiring of this block's `mg_fine_h_o` source, not a new
  law in it.

Layer A is sent because it needs no arithmetic this block has any business
owning, and because sending *something* is what makes the page resident at all.
The planes are named constants (`SURF0_PLANE`, `SURF1_PLANE`) so the ruling has
somewhere to land: when it arrives the change is one mux, here, named.

**Surface 1 is layer C, and that is not in doubt** — C *is* the bottom height.

## Two things the bench taught this block

**MIPGEN's `done_o` pulses before anyone is looking.** The last fine sample is
taken on the same cycle PAGESTREAM raises `v_last`, so MIPGEN completes while
this block is still in `S_PASS`/`S_PSDONE` collecting the *streamer's*
completion — two states before anything reads it. The first version waited for
`mg_done_i` in `S_WAITMG` and hung on the first page: the composed suite
reported eight pages loaded, two lattice passes done, 578 mip17 writes and
**zero** pages mipped, which is exactly the shape of a pulse nobody caught. The
pulse is now latched, cleared at `S_START` — the only cycle at which no scan can
be running.

**The directory validates the CRC on every completion, not only the loader's.**
A mip completion carrying zero is a CRC *failure*. Measured, not anticipated:
16 lattices streamed, 17,424 samples delivered, 4,624 mip17 writes, **eight CRC
failures**, zero pages resident. Every block had done its job and the page still
was not ground.

So the page's CRC travels with the mip request and back out on the completion,
as a **token rather than a claim**: `TERRAIN.PAGELOADER` checked the body,
nothing in the mip chain re-reads it, and the directory wants the identity it
already recorded. `j_crc_i` → `fin_crc_o`, unaltered.

## Exclusions

It does not read memory, does not decimate, does not decide residency, does not
compose §3.4, and does not count vertices — the end of a pass is PAGESTREAM's
`v_last_i`, which is that block's own idea of how long its lattice is. A second
copy of 33 here would be a second thing that has to agree, and its first
disagreement would be a pass that ended early with MIPGEN's surface counter half
way through.

It has **no skid buffer** on the sample path. MIPGEN is storage-free and takes a
sample every cycle it is busy, so a buffer here would exist for a stall the
block it feeds cannot produce. If that ever stops being true, this is where it
changes.

## One job, one completion, always

A refused pass still produces a completion, with `ok = 0`. The rule
`TERRAIN.PAGELOADER`'s contract states, for the same reason: a refusal that
produced silence would leave the directory's entry parked forever with nothing
counted, which is the exact failure this whole chain removes.

## Ports

| field | width | meaning |
|---|---|---|
| `j_valid_i` / `j_ready_o` | 1 | one job = one page's mips |
| `j_slot_i` `j_gen_i` `j_epoch_i` `j_src_id_i` | 11/8/32/32 | identity, returned unaltered |
| `j_crc_i` | 32 | the page's CRC, **carried not computed** — see above |
| `ps_*` / `v_*` / `ps_done_*` | — | TERRAIN.PAGESTREAM, job and lattice |
| `mg_start_o` `mg_job_*` `mg_fine_*` `mg_done_i` | — | TERRAIN.MIPGEN |
| `fin_*` | — | the second completion, to TERRAIN.RESIDENCY |

## Counters

| counter | port | meaning |
|---|---|---|
| `mipfeed_pages_mipped` | `pages_mipped_o` | pages whose mips completed |
| `mipfeed_pages_faulted` | `pages_faulted_o` | a pass the streamer refused |
| `mipfeed_samples` | `samples_sent_o` | fine samples that reached MIPGEN |

## Scalar reference function

`zref::terrain::mipgen` (`reference/include/zref/zref_terrain.hpp`), with
`zref::terrain::page_lattice` supplying the fine samples this block selects
from.

**The oracle is the SHAPE and the COMPLETION, not a colour.** The values are
MIPGEN's law and MIPGEN's differential; what this block is answerable for is
that a page produces 2 × 1,089 samples, 2 × 289 mip17 selections, 2 × 81 mip9
selections, **one** `start`, **zero** aborts and exactly one completion — all of
which are counted and all of which are checked.

## Evidence

`world_composed_directed.cpp` phase C runs the composed world layer with this
chain in place: eight pages loaded, **eight resident**, with
`h_mipgen_fins == 0` — the harness playing nothing, so "resident" is measuring
the machine and not the knob. The shape is checked against the derivation:
16 lattices (two passes per page), 17,424 samples, 4,624 mip17 writes, 1,296
mip9 writes, 0 aborts.

Phase C2 replays the identical cold frame twice more on a fresh directory: once
with the chain removed, where **not one** of the eight pages becomes ground —
the original defect, reproduced on demand rather than remembered — and once with
the harness's old stand-in, which is what the suite ran against for the day and
a half the chain did not exist.

**158 checks, 0 failures, 3 → 1 composition defects.**
