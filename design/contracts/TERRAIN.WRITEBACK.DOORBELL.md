# Contract — the F-sheet journal doorbell (SW.STREAM ⇄ TERRAIN.WRITEBACK)

> Part of TERRAIN.WRITEBACK's capability (`design/blocks.yml` TERRAIN.WRITEBACK); not a
> ledger block of its own.
> Ruling: owner ruling **R14** (`reports/OWNER-RULINGS-20260919-EVENING.md`), provisional.
> RTL: `fpga/rtl/terrain/zhao_terrain_jdoorbell.sv`
> Test: `tests/terrain/jdoorbell_directed.cpp` (the block), and
> `tests/terrain/world_composed_directed.cpp` (the block inside the composed terrain world,
> replacing the bench's minted ticket).
> Positive control: `tests/mutants/zhao_terrain_jdoorbell_mutant.sv`.

## What was missing

TERRAIN.WRITEBACK's job port needs two fields TERRAIN.SEQ does not produce: **where in the
HPS journal the sheet goes** (`j_journal_addr_i`) and **the ticket the journal echoes back**
(`j_seq_i`). Nothing in `fpga/rtl` owned either. `tests/terrain/tb_terrain_world.sv` minted
the ticket in glue and called that "a finding rather than a convenience: no contract says who
owns the journal ticket". Core entry I28 refused the composition on exactly that.

R14 names the owner: **SW.STREAM owns both.** The journal is SW.STREAM's structure
(`design/contracts/SW.STREAM.md`: "SW.STREAM owns the `Journal`; the slot state machine is the
hardware's"), so the space in it and the name of each entry are software's to hand out, and
hardware must never invent either.

## The exchange, in four messages

Every message is a ready/valid transfer across the HPS edge of `zhao_console_core`. In
Verilator the harness IS the HPS (plan D10), exactly as it is for the FRAME_RING view
(`hps_state_i`) and the terrain arena configuration (`terr_cfg_arena_base_i`); these are the
HPS's own words, not tie-offs.

| # | direction | message | fields |
|---|---|---|---|
| D0 | HPS → FPGA | the journal DESCRIPTOR, held for the epoch | `journal_base:u32`, `journal_bytes:u32` |
| D1 | HPS → FPGA | a GRANT: one journal entry, posted ahead of need | `slot:u16`, `ticket:u32` |
| D2 | FPGA → HPS | a RETURN: the hardware hands the ticket back | `ticket:u32`, `final:1`, `ok:1`, `verdict:u4` |
| D3 | HPS → FPGA | an ACK: the sheet is durable (or refused) | `ticket:u32`, `ok:1` |

This is R14's sentence field for field: the HPS writes {journal base (D0), slot, ticket (D1)}
into the mailbox; hardware acknowledges by returning the ticket (D2).

### The laws

1. **The address is software's arithmetic, not a policy.** A job's journal address is
   `journal_base + slot × 8,192` — the F sheet's size (`spec/terrain_rules.md` §2, 64×64 ×
   {tag, strength}). 8,192 is a power of two, so it is a shift. The writeback's existing arena
   check (`[journal_base, journal_base + journal_bytes)`, verdict `kSheetJournal`) still judges
   the result; a grant naming a slot outside the arena is refused by that check, counted, and
   returned — never clamped.
2. **One grant per writeback job, consumed in posting order.** A job offered by TERRAIN.SEQ
   while no grant is posted WAITS (the sequencer is backpressured). It is never given a ticket
   the hardware made up. The wait is counted in CYCLES (`starved_cycles_o`).
3. **Every consumed grant is returned FINAL exactly once.** A sheet that reaches the journal is
   returned TWICE: first `final=0` (LANDED — "the bytes are in your entry; make them durable and
   ACK"), then `final=1` when the job completes after the ACK. A job refused or faulted before
   its sheet landed is returned once, `final=1`, with its verdict — its journal entry is free
   again and no ACK is expected. So software recycles an entry on FINAL and ACKs on LANDED, and
   neither rule needs to know why a job failed.
4. **The ACK is matched by ticket, by TERRAIN.WRITEBACK.** D3 goes straight to the writeback's
   `ack_*` port, whose ticket table already refuses an ACK for a ticket it does not hold
   (`acks_unmatched_o`) and a duplicate ticket in flight (`kSheetSeqInFlight`). The doorbell adds
   no second matcher.
5. **The return queue cannot overflow, by credit.** A grant is consumed only while fewer than
   `TICKETS` consumed tickets have not yet had their FINAL record taken by the HPS. Each such
   ticket owes at most two records, and the queue holds `2 × TICKETS`, so a record always has a
   place. The landing pulse and the completion cannot be stalled, which is why the space is
   reserved when the grant is consumed rather than checked when the record arrives.
   `ret_overflow_o` is the guard; it is unreachable while the credit is right, so it is fired by
   a committed mutant that removes the credit.
6. **Nothing here times out.** A ticket whose ACK never comes keeps its credit forever; the
   writeback's watchdog (`acks_overdue_o`) reports it. A stopwatch that freed the credit would
   let the return queue overflow, and one that faulted the ticket would be a policy nobody ruled.

## SW.STREAM's half

`design/contracts/SW.STREAM.md` carries the matching sentence: SW.STREAM keeps the journal
descriptor (D0) current for the epoch, keeps grants (D1) posted ahead of need, ACKs (D3) every
LANDED return once the sheet is durable, and recycles an entry on its FINAL return. A shortage of
posted grants stalls dirty evictions and is visible as `starved_cycles_o`; it is software
backpressure in T12's sense, not a hardware fault.

## Not decided here

* How many grants SW.STREAM should keep posted. `GRANTS` (default 4) is the mailbox depth, not a
  policy. The pressure is `starved_cycles_o`; nothing has measured it on a board.
* Retry after a NAK. Ruling T4/T11 territory, as TERRAIN.WRITEBACK.md already records.
* The physical HPS side. On a board these words cross the HPS-to-FPGA boundary; that transport
  is MEM.HPS.BRIDGE's and the board's, and D10 is what lets the harness stand in for it today.
