# The FIELD seam, mapped (research, 2026-09-20)

Two read-only surveys done while the account was rate-limited. Both are evidence for the FIELD packet's brief.

## The doorbell pattern (R14/R43/R51/R58), as BUILT

* There is NO reusable "CSR mailbox" module. Tree-wide, the only doorbell is `zhao_terrain_jdoorbell`,
  whose own header says it IS the mailbox ruling R14 describes.
* **R51's host register window did not exist in RTL at the time of the survey** (the HOSTDBG packet has
  since built `zhao_host_regwin.sv`; check it before assuming either way).
* The shipped pattern is THREE parts: a port group at the console edge (`terr_jdb_*` +
  `terr_cfg_journal_*`, forwarded through `zhao_console_board.sv`), the doorbell block, and a
  consumer that returns the ticket (`zhao_terrain_writeback`'s `landed_*` pulse).
* Four channels: D0 descriptor, D1 HPS→FPGA grant {slot, ticket}, D2 FPGA→HPS return
  {ticket, final, ok, verdict}, D3 HPS→FPGA ack — and **D3 bypasses the doorbell** to the consumer's
  ticket table, deliberately, so there is exactly ONE matcher.
* Law to copy: the return-queue credit is RESERVED when a grant is consumed, because the landed pulse
  and the completion cannot be stalled. `ret_overflow_o` is unreachable by construction, so it carries
  a committed inverted-polarity mutant.
* I28's entry is CLOSED IN PLACE: kept in the header table, in parentheses, with the date and the rulings.
  That is the convention for a closure.

## The FIELD host's second client — one seam seen from three sides

* `zhao_field_host` client vectors are packed `{fld_req_valid_i, sfa_req_valid}`, so
  **client 0 is the composed stamp adapter and client 1 is the console's raw edge** — the core says so:
  *"Client 1 is at this module's edge and is the seam I5 and I34 will take."*
* I34 is TERRAIN.PATCH's height lane (`fld_valid_i`/`fld_ready_o`/`fld_height_i`, with
  `fld_covers_o` answering the §9.1 footprint test — the test lives THERE, so a lane is offered for
  every accepted list entry and the block decides coverage).
* I5 is PART.UPDATE's `fld_*`: three s11 accelerations and a valid, **combinational, no ready**.
* The widths do not meet directly (host answers 4x32 of E record; PATCH wants one s32; UPDATE wants
  three s11), so each profile needs an adapter. `zhao_field_stamp_adapter` IS that pattern for S, and
  its "only 48 of 128 bits are the record" comment is the precedent.
* Loads are accepted ONLY while the fabric is idle; a load offered mid-run DEFERS the next grant.
  `ld_kind_i`: 0 uop, 1 table entry, 2 header, 3 uniform — and the HEADER is written LAST, because it
  commits the tables and marks the slot runnable.
* `SLOTW`/`PCW`/`REGW`/`TSELW`/`TIDXW`/`LDADDRW` are literals on purpose:
  `gen_prod_top.py` cannot evaluate a parameter expression when sizing a port and SILENTLY SKIPS a
  module it cannot size. Override what they derive from, never them.
* The console's FAB_* choices are argued in place (scalar front, one point in flight). `FAB_GROUP_PTS=1`
  is the handover's §3 saving taken in its correct form; the handover's own suggestion (forward LANES)
  would have been wrong.
* **An uncashed cheque:** `zhao_probe_walk_earth` and `zhao_probe_patch_acc` are built, differentially
  tested against zref, registered in ctest — and instantiated by NOTHING. With `zhao_field_v3_exec` they
  are the declared Earth triple; two thirds of it is finished and homeless.