// zhao_terrain_jdoorbell_mutant.sv -- A DELIBERATELY BROKEN COPY. NOT SHIPPED.
//
// WHAT IT PROVES. `ret_overflow_o` in zhao_terrain_jdoorbell.sv watches for a
// return record with nowhere to go. That state is UNREACHABLE while the credit
// is right (contract law 5): a grant is consumed only while fewer than TICKETS
// tickets owe their FINAL, each owes at most two records, and the queue holds
// 2*TICKETS. No legal stimulus moves the counter, so "it can fire" would stay an
// argument forever.
//
// THE ONE SUBSTANTIVE CHANGE:
//
//     wire credit_ok = (owe < 32'(TICKETS));
//  -> wire credit_ok = 1'b1;                  // MUTANT: no credit
//
// (plus a lint-waived read of `owe`, which the mutant no longer consumes.)
// With the credit gone, 2*TICKETS+1 refused jobs whose FINAL records the HPS
// does not take overflow the queue.
//
// INVERTED POLARITY: driven by tests/terrain/jdoorbell_mutant_control.cpp,
// which PASSES when `ret_overflow_o` FIRES. Evidence about the INSTRUMENT, not
// the design. The module is RENAMED so a source list can never elaborate it in
// place of the real one.
//
// REGENERATE IT if zhao_terrain_jdoorbell.sv changes shape: a copy of an old
// version is a positive control for a block that no longer exists.
// tools/budget/mutant_copy_drift.py watches for exactly that.
// ---------------------------------------------------------------------------
// zhao_terrain_jdoorbell.sv - the F-sheet journal DOORBELL: SW.STREAM's
// journal entries and tickets, attached to TERRAIN.SEQ's writeback jobs.
//
// Contract: design/contracts/TERRAIN.WRITEBACK.DOORBELL.md. Owner ruling R14.
//
// ---------------------------------------------------------------------------
// WHAT IT IS FOR
// ---------------------------------------------------------------------------
// TERRAIN.WRITEBACK's job port needs `j_journal_addr_i` and `j_seq_i` -- where
// in the HPS journal a dirty page's F sheet goes, and the ticket the journal
// echoes back. TERRAIN.SEQ emits neither, and nothing in fpga/rtl owned them:
// the composed terrain bench minted the ticket in glue and called that "a
// finding rather than a convenience". Ruling R14 gives both to SW.STREAM. This
// block is the mailbox that ruling describes, and it does NOT invent either
// value: every address and every ticket it hands the writeback came from a
// grant the HPS posted.
//
//   D0  HPS -> FPGA  journal descriptor {base, bytes}, held for the epoch
//   D1  HPS -> FPGA  a GRANT {slot, ticket}, posted ahead of need
//   D2  FPGA -> HPS  a RETURN {ticket, final, ok, verdict}
//   D3  HPS -> FPGA  the ACK {ticket, ok} -- wired to TERRAIN.WRITEBACK's own
//                    ack port by the composer; its ticket table is the matcher
//
// ---------------------------------------------------------------------------
// THE LAWS THIS FILE ENFORCES (numbers are the contract's)
// ---------------------------------------------------------------------------
// 1. address = base + slot * F_BYTES, as a SHIFT in 64 bits. Not clamped and
//    not range-checked here: the writeback's arena check judges it, refuses it
//    as kSheetJournal and the refusal comes back as a FINAL return.
// 2. one grant per job, in posting order; no grant = the job WAITS.
// 3. every consumed grant is returned FINAL exactly once; a sheet that landed
//    is ALSO returned LANDED (final=0) first, which is what software ACKs.
// 5. the return queue cannot overflow, BY CREDIT: a grant is consumed only
//    while fewer than TICKETS consumed tickets still owe their FINAL record to
//    the HPS. Each owes at most two records and the queue holds 2*TICKETS. The
//    landing pulse and the completion cannot be stalled, so the space is
//    reserved when the grant is consumed, not checked when a record arrives.
//    `ret_overflow_o` is the guard; unreachable while the credit is right, it
//    is fired by tests/mutants/zhao_terrain_jdoorbell_mutant.sv.
// 6. nothing times out. A ticket that is never ACKed keeps its credit; the
//    writeback's watchdog reports it.
//
// The job fields pass through COMBINATIONALLY, which is deliberate rather than
// lazy: TERRAIN.SEQ holds its offer until ready (its wb port is a held offer)
// and TERRAIN.WRITEBACK captures at acceptance (check_ingress_capture.py
// discipline), so a register here would add a cycle and a second copy of the
// job with no stall to decouple.
//
// Conservative SystemVerilog subset. Lint gate: lint_terrain_jdoorbell.
`default_nettype none

module zhao_terrain_jdoorbell_mutant #(
    parameter int unsigned SLOTW   = 11,   // TERRAIN.RESIDENCY's slot index (+1 guard bit)
    parameter int unsigned GENW    = 8,
    parameter int unsigned GRANTS  = 4,    // mailbox depth: posted, not yet consumed
    parameter int unsigned TICKETS = 4,    // consumed tickets that may owe a FINAL
    // The F sheet: 64 x 64 x {tag u8, strength u8}. A power of two, so the
    // address law is a shift; refused at elaboration otherwise.
    parameter int unsigned F_BYTES = 8192
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- D0: the journal descriptor (HPS-owned, frame-scoped) ---------------
    input  var logic [31:0] cfg_journal_base_i,

    // ---- D1: grants ----------------------------------------------------------
    input  var logic        post_valid_i,
    output var logic        post_ready_o,
    input  var logic [15:0] post_slot_i,
    input  var logic [31:0] post_ticket_i,

    // ---- the writeback job from TERRAIN.SEQ ---------------------------------
    input  var logic                 sj_valid_i,
    output var logic                 sj_ready_o,
    input  var logic [SLOTW-1:0]     sj_slot_i,
    input  var logic [GENW-1:0]      sj_gen_i,
    input  var logic [31:0]          sj_epoch_i,
    input  var logic [31:0]          sj_island_i,
    input  var logic signed [15:0]   sj_ix_i,
    input  var logic signed [15:0]   sj_iz_i,
    input  var logic [31:0]          sj_src_id_i,

    // ---- ...and to TERRAIN.WRITEBACK, with SW.STREAM's two fields attached --
    output var logic                 wj_valid_o,
    input  var logic                 wj_ready_i,
    output var logic [SLOTW-1:0]     wj_slot_o,
    output var logic [GENW-1:0]      wj_gen_o,
    output var logic [31:0]          wj_epoch_o,
    output var logic [31:0]          wj_island_o,
    output var logic signed [15:0]   wj_ix_o,
    output var logic signed [15:0]   wj_iz_o,
    output var logic [63:0]          wj_journal_addr_o,
    output var logic [31:0]          wj_seq_o,
    output var logic [31:0]          wj_src_id_o,

    // ---- the writeback's statements -----------------------------------------
    // `landed_*` is a PULSE on the cycle a sheet's last journal beat retires,
    // carrying that job's own ticket (zhao_terrain_writeback, the same edge its
    // ticket table allocates). It cannot be stalled.
    input  var logic                 landed_valid_i,
    input  var logic [31:0]          landed_seq_i,
    // Completion: every job produces exactly one. Always accepted here -- the
    // space for its FINAL record was reserved when its grant was consumed.
    input  var logic                 done_valid_i,
    output var logic                 done_ready_o,
    input  var logic [SLOTW-1:0]     done_slot_i,
    input  var logic                 done_ok_i,
    input  var logic [3:0]           done_verdict_i,
    input  var logic [31:0]          done_seq_i,

    // ---- the completion, forwarded to TERRAIN.SEQ's barrier port -------------
    // TERRAIN.SEQ's `wb_done_*` is {valid, slot} with no ready: an event.
    output var logic                 seq_done_valid_o,
    output var logic [SLOTW-1:0]     seq_done_slot_o,

    // ---- D2: returns ---------------------------------------------------------
    output var logic                 ret_valid_o,
    input  var logic                 ret_ready_i,
    output var logic [31:0]          ret_ticket_o,
    output var logic                 ret_final_o,
    output var logic                 ret_ok_o,
    output var logic [3:0]           ret_verdict_o,

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0] grants_posted_o,       // EVENTS
    output var logic [31:0] grants_taken_o,        // EVENTS
    output var logic [31:0] returns_landed_o,      // EVENTS
    output var logic [31:0] returns_final_o,       // EVENTS
    output var logic [31:0] starved_cycles_o,      // CYCLES: a job waited for a grant
    output var logic [31:0] credit_stall_cycles_o, // CYCLES: a job waited for return space
    output var logic [31:0] tickets_owed_o,        // LEVEL: consumed, FINAL not yet taken
    output var logic [31:0] ret_overflow_o         // EVENTS: must stay 0 -- see law 5
);

  localparam int unsigned FSH = $clog2(F_BYTES);
  localparam int unsigned RD  = 2 * TICKETS;           // return queue depth
  localparam int unsigned GW  = (GRANTS > 1) ? $clog2(GRANTS) : 1;
  localparam int unsigned RW  = (RD > 1) ? $clog2(RD) : 1;

`ifndef SYNTHESIS
  initial begin
    if ((32'd1 << FSH) != 32'(F_BYTES))
      $fatal(1, "jdoorbell: F_BYTES=%0d is not a power of two; the address law is a shift", F_BYTES);
    if (GRANTS < 1 || TICKETS < 1)
      $fatal(1, "jdoorbell: GRANTS and TICKETS must be >= 1");
  end
`endif

  // ------------------------------------------------------------ the grants --
  logic [15:0] g_slot   [GRANTS];
  logic [31:0] g_ticket [GRANTS];
  logic [GW-1:0] g_rd, g_wr;
  logic [GW:0]   g_cnt;

  wire g_avail = (g_cnt != '0);
  wire g_full  = (g_cnt == (GW+1)'(GRANTS));

  assign post_ready_o = !g_full;
  wire post_fire = post_valid_i && post_ready_o;

  // -------------------------------------------------------------- the credit --
  logic [31:0] owe;
  // MUTANT: the credit is removed. Production reads:
  //   wire credit_ok = (owe < 32'(TICKETS));
  wire credit_ok = 1'b1;
  /* verilator lint_off UNUSEDSIGNAL */
  wire [31:0] mutant_owe_unread = owe;
  /* verilator lint_on UNUSEDSIGNAL */

  // ---------------------------------------------------------- the job path --
  wire gate    = g_avail && credit_ok;
  assign sj_ready_o = wj_ready_i && gate;
  assign wj_valid_o = sj_valid_i && gate;
  wire job_fire = sj_valid_i && sj_ready_o;

  assign wj_slot_o         = sj_slot_i;
  assign wj_gen_o          = sj_gen_i;
  assign wj_epoch_o        = sj_epoch_i;
  assign wj_island_o       = sj_island_i;
  assign wj_ix_o           = sj_ix_i;
  assign wj_iz_o           = sj_iz_i;
  assign wj_src_id_o       = sj_src_id_i;
  assign wj_seq_o          = g_ticket[g_rd];
  // Law 1, in 64 bits so a base near the top of the 32-bit HPS space cannot
  // wrap into a legal-looking address: the writeback refuses a nonzero upper
  // half as kSheetUnreachable.
  assign wj_journal_addr_o = {32'd0, cfg_journal_base_i}
                           + ({48'd0, g_slot[g_rd]} << FSH);

  // ---------------------------------------------------- completion forward --
  assign done_ready_o     = 1'b1;
  assign seq_done_valid_o = done_valid_i;
  assign seq_done_slot_o  = done_slot_i;
  wire done_fire = done_valid_i && done_ready_o;

  // -------------------------------------------------------- the returns ------
  logic [31:0] r_ticket  [RD];
  logic        r_final   [RD];
  logic        r_ok      [RD];
  logic [3:0]  r_verdict [RD];
  logic [RW-1:0] r_rd, r_wr;
  logic [RW:0]   r_cnt;

  assign ret_valid_o   = (r_cnt != '0);
  assign ret_ticket_o  = r_ticket[r_rd];
  assign ret_final_o   = r_final[r_rd];
  assign ret_ok_o      = r_ok[r_rd];
  assign ret_verdict_o = r_verdict[r_rd];
  wire ret_pop = ret_valid_o && ret_ready_i;

  // Up to two records arrive in one cycle: a sheet of job B lands while job
  // A's completion retires. LANDED is written first; they name different
  // tickets, so their mutual order carries no meaning.
  wire [1:0]  n_push = {1'b0, landed_valid_i} + {1'b0, done_fire};
  wire [RW:0] r_free = (RW+1)'(RD) - r_cnt + (RW+1)'(ret_pop);
  wire        overflow = ((RW+1)'(n_push) > r_free);

  // Pointer advance by 0..2 with an explicit wrap: no modulus, so a depth that
  // is not a power of two costs a compare rather than a divider.
  function automatic logic [RW-1:0] radv(input logic [RW-1:0] p, input logic [1:0] k);
    logic [RW+1:0] s;
    begin
      s = (RW+2)'(p) + (RW+2)'(k);
      if (s >= (RW+2)'(RD)) s = s - (RW+2)'(RD);
      radv = s[RW-1:0];
    end
  endfunction

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      g_rd  <= '0;
      g_wr  <= '0;
      g_cnt <= '0;
      for (int unsigned i = 0; i < GRANTS; i++) begin
        g_slot[i]   <= '0;
        g_ticket[i] <= '0;
      end
      owe   <= '0;
      r_rd  <= '0;
      r_wr  <= '0;
      r_cnt <= '0;
      for (int unsigned i = 0; i < RD; i++) begin
        r_ticket[i]  <= '0;
        r_final[i]   <= 1'b0;
        r_ok[i]      <= 1'b0;
        r_verdict[i] <= '0;
      end
      grants_posted_o       <= '0;
      grants_taken_o        <= '0;
      returns_landed_o      <= '0;
      returns_final_o       <= '0;
      starved_cycles_o      <= '0;
      credit_stall_cycles_o <= '0;
      ret_overflow_o        <= '0;
    end else begin
      // ---- grants: post and consume --------------------------------------
      if (post_fire) begin
        g_slot[g_wr]    <= post_slot_i;
        g_ticket[g_wr]  <= post_ticket_i;
        g_wr            <= (32'(g_wr) == GRANTS - 1) ? '0 : g_wr + GW'(1);
        grants_posted_o <= grants_posted_o + 32'd1;
      end
      if (job_fire) begin
        g_rd           <= (32'(g_rd) == GRANTS - 1) ? '0 : g_rd + GW'(1);
        grants_taken_o <= grants_taken_o + 32'd1;
      end
      g_cnt <= g_cnt + (GW+1)'(post_fire) - (GW+1)'(job_fire);

      if (sj_valid_i && !g_avail)             starved_cycles_o      <= starved_cycles_o + 32'd1;
      if (sj_valid_i && g_avail && !credit_ok) credit_stall_cycles_o <= credit_stall_cycles_o + 32'd1;

      // ---- credit: taken at the grant, given back at the FINAL pop ---------
      owe <= owe + 32'(job_fire) - 32'(ret_pop && r_final[r_rd]);

      // ---- returns ---------------------------------------------------------
      if (overflow) begin
        // Law 5's guard. Nothing is written: a partial write would leave the
        // queue holding a LANDED without its FINAL, which is worse than
        // counting the loss.
        ret_overflow_o <= ret_overflow_o + 32'd1;
      end else begin
        if (landed_valid_i) begin
          r_ticket[r_wr]  <= landed_seq_i;
          r_final[r_wr]   <= 1'b0;
          r_ok[r_wr]      <= 1'b1;
          r_verdict[r_wr] <= 4'd0;
          returns_landed_o <= returns_landed_o + 32'd1;
        end
        if (done_fire) begin
          r_ticket[radv(r_wr, {1'b0, landed_valid_i})]  <= done_seq_i;
          r_final[radv(r_wr, {1'b0, landed_valid_i})]   <= 1'b1;
          r_ok[radv(r_wr, {1'b0, landed_valid_i})]      <= done_ok_i;
          r_verdict[radv(r_wr, {1'b0, landed_valid_i})] <= done_verdict_i;
          returns_final_o <= returns_final_o + 32'd1;
        end
        r_wr <= radv(r_wr, n_push);
      end
      if (ret_pop) r_rd <= radv(r_rd, 2'd1);
      r_cnt <= r_cnt + (overflow ? '0 : (RW+1)'(n_push)) - (RW+1)'(ret_pop);
    end
  end

  assign tickets_owed_o = owe;

endmodule

`default_nettype wire
