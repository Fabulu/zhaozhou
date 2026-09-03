# Contract — TERRAIN.RESIDENCY (Page residency directory)

> Ledger: `design/blocks.yml` · gpu clock · maturity UNIT_VERIFIED
> RTL: `fpga/rtl/terrain/zhao_terrain_residency_v2.sv`
> Superseded prototype: `fpga/rtl/terrain/zhao_terrain_residency.sv` — **not
> the world directory**, see X8/T9.

## Purpose and exclusions

Answers one question for the whole 8 km world: **is this patch's ground in
local SDRAM, and may this job still use the handle it holds?**

**Written 2026-09-03 from rulings T1/T9/T10.** The RTL existed with a passing
suite and had no contract and no ledger entry — the third block found in that
state this week, after `GEOM.PARAMBUF` and `TERRAIN.MIPGEN`.

**Exclusions.** It does not load pages, does not verify CRCs itself (the loader
reports the CRC it saw and the directory compares), does not generate mips
(`TERRAIN.MIPGEN`) and does not write back scars (the F-sheet journal is
`SW.STREAM`'s). It owns *who may use what*, and nothing else.

## Why the prototype is superseded

`zhao_terrain_residency.sv` is a 1,024-slot **direct map keyed on `{px, py}`**.
Two independent reasons it cannot be the world directory:

* **The key is wrong.** T1 makes the canonical key
  `{resource_epoch, island_id, patch_ix, patch_iz}` and states that **two
  islands may legally overlap in local patch coordinates**. A `{px, py}`
  directory answers island A's lookup with island B's ground — wrong in a way
  no single frame reveals.
* **The mapping is wrong.** A direct map on the low bits of each axis has a
  **2,048 m period**: two patches collide when they differ by 32 in *both*
  axes, which sounds rare and is exactly what an 8 km traversal does.

T9: *"determinism does not require direct mapping — a deterministic
set-associative cache is deterministic under a canonical request order."*

The prototype is kept, not deleted. It is what v2 is measured against.

## Input and output packet layouts

**Shape:** 256 sets × 4 ways = 1,024 slots. **The full key is stored in every
way** — a set index is a hash, a hash collision is normal, and the key is what
answers.

**Handle:** `{resource_epoch:u32, slot:u10, generation:u8}`.

### Ports

| port | direction | purpose |
|---|---|---|
| `lu_*` | query | is this patch resident? |
| `cl_*` | mutation | claim a slot; returns the victim |
| `fin_*` | mutation | loader completion, with success and the CRC it saw |
| `dm_*` | mutation | deformation marks `modified_BD` / `dirty_F` / `mips_stale` |
| `pin_*` / `unpin_*` | mutation | reference counting |
| `wb_*` | mutation | journal writeback acknowledgement |
| `chk_*` | query | is this in-flight handle still its own page? |

### The set index

**CRC-8/ATM, polynomial `0x07`, initial 0**, over the little-endian bytes of
`{island_id, patch_ix, patch_iz}`, with `resource_epoch[7:0]` xored **into the
final byte of the message**.

**That last clause has two readings** — the final byte of the message, or the
CRC's single output byte — and this is the committed one, behind
`SET_EPOCH_IN_MESSAGE` so the other is one line away. It is called out because
a silently chosen hash is the kind of decision that becomes unquestionable by
being invisible.

## Backpressure rules

**Every mutation port has its own `ready`.** Mutations are serialized through
one port with a fixed priority:

    0  writeback ACK    1  claim    2  loader finish
    3  dirty mark       4  unpin    5  pin

A loser is **backpressured, never dropped**: a dropped pin leaks a reference, a
dropped unpin pins a page forever.

**Queries are not backpressured — they are simply not answered that clock.**
`lu_valid_o` and `chk_valid_o` stay low when a mutation takes the address port.
Both callers already tolerate that; they are queries, not transactions.

`ready_o` is **low for 256 clocks after reset** while the metadata banks are
swept. Nothing may be presented before it rises.

## Memory ownership

**Synchronous metadata RAM banks, one per way** (T10), with registered lookup
and capture. `valid`/state bits that need a reset stay in flops.

The 256-clock init sweep replaces an asynchronous reset. The prototype clears
1,024 slots in one clock, which is correct in simulation and is a 1,024-entry
asynchronous reset fan-out no memory block can implement — **a directory that
infers registers instead of M10K is a directory that will not fit.**

The pages themselves live in `TERRAIN.PAGE_POOL`; see `spec/memory_rules.md`
§5b.

## Q formats and rounding

None. Keys, generations and counters are integers.

## Latency (fixed or variable)

`fixed:2` for a query — one clock to address the banks, one for the registered
compare — plus a **one-clock stall** when a mutation is writing the same set
the query wants to read. That stall is a deliberate choice over a bypass: a
bypass is four more comparators and four more wide muxes on the block's widest
path, and the collision is two consecutive events in the same set out of 256.

## Target throughput

One mutation or one query per clock. This directory answers a few hundred
claims a frame, not one a clock.

## Overflow and malformed-input behaviour

Replacement, in T9's order: **(1)** matching key; **(2)** an invalid way;
**(3)** a clean unpinned way by per-set round robin; **(4)** a `dirty_F`
unpinned way, same order, entering `EVICT_PENDING`; **(5)** all pinned —
**backpressure and count**, never displace.

* **No slot reuse before the pin count is zero.**
* **No `dirty_F` reuse before the writeback ACK.** This is T4's barrier and the
  most important rule in the file: a dirty page holds scars the player made,
  layer F has no canonical HPS mirror, and reusing the slot early means that
  terrain silently heals.
* **A CRC-failed or aborted load is `FAULTED`, never resident** (T7: a
  half-loaded page is never rendered).
* **A lookup hits only on `RESIDENT_CLEAN` or `RESIDENT_DIRTY_F`.** A page that
  is `RESERVED`, `LOADING` or in `MIPGEN` is not ground yet, and answering with
  it composes an unwritten height lattice.
* **Every non-claim event is checked against the stored `{epoch, generation}`
  before it may write.** A stale FIN/DIRTY/UNPIN does not lose a race — it is
  rejected on identity and counted.
* **Re-claiming a resident patch does not advance the generation.** A
  visible-set rebuild re-submits resident patches every frame; advancing here
  would tell every in-flight job it is stale, every frame, forever.

## Scalar reference function

`zref::terrain::residency_set_index` and `zref::terrain::crc8_atm_byte`
(`reference/include/zref/zref_terrain.hpp`).

**That is the oracle for the MAPPING, not for the state machine**, and the
distinction is deliberate rather than an omission. The hash is the part that
could be silently wrong in a way nothing else would catch, so it has a
definition outside the RTL that implements it; the directed suite checks that
280 claims land in the sets that symbol names. The state machine is verified by
the cases below against the rules above.

## Directed tests

`tests/terrain/terrain_residency_v2_directed.cpp` — 38 checks, including the
two the prototype cannot express at all:

* **two islands at identical local coordinates**, both resident, separately
  findable, neither displacing the other;
* **a dirty victim with a delayed writeback ACK** — the slot does *not* become
  resident until the journal acknowledges, however many loader completions
  arrive first.

And: the reset sweep really takes 256 clocks; a claimed page is not resident
until loaded *and* mipped; re-claiming does not advance the generation; four
adversarial keys colliding in one set, all pinned, refuse a fifth; a clean way
is taken before a dirty one; CRC failure and aborted load both fault; stale FIN
and stale DIRTY are rejected on identity; a handle survives 255 intervening
claims; a lookup in a newer epoch misses; **the same claim stream produces the
same slots twice.**

**A bug the suite caught:** `WAYW'(WAYS)` truncates 4 to two bits and is
**zero**, so the round-robin walk was a divide by zero and always returned way
0. It passed nine of eleven cases, because way 0 is very often the right
answer. The case that caught it was "is a clean way taken before a dirty one" —
which matters because rule 4 costs a journal round trip.

## Randomized differential tests

Planned: the 8 km traversal against an independent model of the rules above.
The property worth the file is the same one the prototype's random lane found —
**a dirty page displaced without being flagged means permanent scars silently
heal**, and no single-frame check can see it.

## Integration capture cases

None. **No RTL is integrated and nothing is on hardware.** T10 says to build
this beside the prototype and not to integrate the direct map; composition is a
later increment.

## Synthesis / resource ceiling

Unfitted. **The acceptance question is inference, not Fmax:** if the metadata
banks come back as flops rather than M10K, the block has not met T10 whatever
its clock says.

## Notes

leaf.
