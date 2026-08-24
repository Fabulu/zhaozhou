# SPEC v1: FIELD register file — flops to block memory

**Run ID:** RUN-20260824-0932
**Created:** 2026-08-24 09:32 UTC+02:00
**Status:** Active

---

## Objective

Convert `zhao_field_seq`'s 64×32 register file from flip-flops behind
asynchronous 64:1 muxes into inferred block memory, and remove the 64-entry
reset clear that is the second thing preventing inference.

Success is `blockMemoryBits > 0` on a map at a committed commit, values
bit-identical to `zfield::interpret`, and the mutation sweep re-scored.

## Why this and not the rest of the Field ruling

Measured first (`docs/OWNER_DOCKET.md`, 2026-08-24), because two of the ruling's
premises did not survive:

- **the three constant ROMs are already cheap** — 87 + 79 + 162 = **328 ALMs**
  for 16,448 ROM bits. Quartus constant-folds a case tree over constants well; a
  constant ROM is not the same problem as a read/write array, and §10's penalty
  curve was measured on arrays. Converting them is a *timing* item worth ~328
  ALMs, not the resource win the ruling implies;
- **the ALM target of 3,500–5,000 does not follow.** 7,958 − 1,371 = 6,587. The
  arithmetic is the block: `exec_shared` alone is 4,793 ALMs.

**The register file is the one storage item that is real**, and the calibration
measured its exact shape: 64×32 sync+no-reset infers at **40 ALMs**; with reset
it is **1,411**. So ~1,371 ALMs — and, more importantly, the block's own header
already names those 64:1 async muxes as **the dominant cost**, so this is a
timing change as much as a resource one. Timing is the block's real problem
(33.86 MHz against 100).

## What the RTL actually does today

**Four asynchronous read ports** (`zhao_field_seq.sv:269-272`):

    assign rd_a       = rf[ra];            // engine operand A
    assign rd_b       = rf[rb];            // engine operand B
    assign rd_c       = rf[rc];            // engine operand C
    assign rf_rdata_o = rf[rf_raddr_i];    // host port

**One write per edge — already serialised.** I first read the three lane writes
as simultaneous; they are not. `rf[i_dst]`, `rf[i_dst+1]`, `rf[i_dst+2]` sit in
an `if / else if` chain across **different states** (`Q_MWAIT`, `Q_WB1`,
`Q_WB2`), so at most one write happens on any edge. **The write side is already
single-port**, and wave 3 therefore does *not* depend on the ruling's wave 4.

**A 64-entry clear** on reset and on the clear command (`:370`, `:382`).

## The design

**Four replicated simple-dual-port RAMs**, one per read port, every write
broadcast to all four. 4 × 2,048 bits = 8,192 bits, one M10K each at most — **4
of 502 free**.

**Synchronous reads shift every capture down one state.** The address mux at
`:247-265` is unchanged, because it already keys off `state`, and `Q_LATCH`
already addresses from `ins_*_i` rather than `i_a`/`i_b` — the comment there says
why: those are only latched at the END of that state. So:

| state | drives addresses for | captures |
| --- | --- | --- |
| `Q_LATCH` | group 0 (`ins_*_i`) | — |
| `Q_RD1` | group 1 (`i_a+1`) | **group 0** → `a0`, `b0`, `cv` |
| `Q_RD2` | group 2 (`i_a+2`) | **group 1** → `a1`, `b1` |
| **`Q_RD3`** *(new)* | — | **group 2** → `a2`, `b2` |
| `Q_GATH` | | |

One extra state. **A simple instruction becomes 7 clocks instead of 6**, exactly
as the ruling predicted.

**A 64-bit valid bitmap replaces the clear.** Reset and the clear command zero
`rf_valid` in one cycle; a write sets its bit; a read returns 0 when the bit is
clear. That is exactly equivalent to clearing every entry **including after
reset, when M10K contents are undefined** — which is the reason it is required
rather than merely cheaper.

## Constraints

- values **bit-identical** to `zfield::interpret`;
- `blockMemoryBits > 0` on the map, or the implementation has **failed however
  green the tests are** — that rule found a block at 229% of the device;
- one Quartus job at a time; map before fit; never map uncommitted RTL;
- sweep in a worktree, preflight must report a **non-zero** mutant count.

## Don't Retry

- **Do not read an `if / else if` chain as simultaneous writes.** I did, and
  nearly recorded a false dependency on wave 4.
- **Do not expect removing the reset alone to make it infer.** The calibration
  measured 64×32 async+no-reset at **0 memory bits, 1,427 ALMs** — the async read
  kills it independently. Both changes are required.
- **Do not assume the latency table is unchanged.** This is not a combinational
  edit; a state is added, and `field_seq_directed`'s measured per-opcode latency
  table **will** move by one. That is expected, and it is the one place where a
  moving number is correct rather than alarming.

## Open Questions

- **CORRECTED**: the SPEC said "safe to clock in read_reg: it is only called
  once the walk is idle". **That was false.** Section 14 of the directed test
  watches reg[20] EVERY cycle during a run, so a clocking read_reg advanced the
  clock twice per iteration and swallowed retire pulses. The observer now holds
  the address and reads the port directly, with no extra edge.

- Whether the four copies should instead be two dual-port RAMs; deferred, since
  M10Ks are not scarce here (4 of 502) and replication keeps the read paths
  independent.
