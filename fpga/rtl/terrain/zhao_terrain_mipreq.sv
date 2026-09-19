// zhao_terrain_mipreq.sv -- THE MIP REQUEST'S OWNER.
//
// ---------------------------------------------------------------------------
// THE GAP THIS FILLS, AND WHY IT WAS A GAP
// ---------------------------------------------------------------------------
// `tests/terrain/tb_terrain_world.sv` names it exactly:
//
//     "Something has to notice that a page has landed and ask for its mips.
//      Nothing in `fpga/rtl` does: TERRAIN.RESIDENCY moves the entry to
//      ST_MIPGEN and has no port to ASK for anything, and the loader only
//      reports. So the trigger is minted here ... and, like the journal ticket
//      above, that glue is a finding rather than a convenience: no contract
//      says who owns the mip request."
//
// That finding is what this file answers.  It is a block rather than four lines
// in `zhao_console_core.sv` because the queue below is STATE, and this
// repository's standing rule is that state belongs in a file with a contract
// and a test rather than in a composer.
//
// ---------------------------------------------------------------------------
// THE TRIGGER IS READ OFF THE DIRECTORY, NOT CHOSEN
// ---------------------------------------------------------------------------
// This matters, because "the composer invented a policy" is the failure the
// whole terrain ledger is written to prevent, and a request queue looks exactly
// like one.  It is not, and the reason is in `zhao_terrain_residency_v2.sv`:
//
//   * a CLAIM writes `s_pack('0, 1'b0, victim_dirty_c, 1'b1, s0_crc, s0_seq)`
//     -- the fourth argument is `mips`, and it is 1'b1 on EVERY claim.  Every
//     newly claimed page is mips-stale by construction.
//   * the loader's completion arm reads
//     `s_mips(s) ? ST_MIPGEN : ST_RESIDENT_CLEAN`.
//   * `resident_o` and the lookup hit BOTH require ST_RESIDENT_CLEAN, and the
//     only arm that reaches it from ST_MIPGEN is a SECOND `EV_FIN`.
//
// So the set of pages that need a mip pass is not a policy question with
// several defensible answers: it is EXACTLY the set of loader completions the
// directory accepted with `ok` set.  Any other trigger would be wrong, not
// merely different.  This block emits one request per such event and nothing
// else, and it is `zhao_terrain_mipreq` rather than a wire because the rate
// does not match.
//
// ---------------------------------------------------------------------------
// IT IS A QUEUE AND NOT A REGISTER, AND THE ARITHMETIC IS WHY
// ---------------------------------------------------------------------------
// From the bench's own measurement: a page load is about 6,726 clocks; the two
// lattice passes the mip chain needs are about 7,088.  TERRAIN.MIPFEED IS
// SLOWER THAN THE LOADER THAT FEEDS IT.  A one-deep pending register therefore
// drops a request every time a page lands while the previous page's mips are
// still running -- and a dropped request shows up as A PAGE THAT NEVER BECOMES
// GROUND, which is the exact defect this whole chain exists to remove,
// reappearing one level up.
//
// DEPTH defaults to 8, which is the composed frame, and an overflow is COUNTED
// rather than silent.  `drops_o` is reachable with legal stimulus (hold
// `j_ready_i` low and present DEPTH+1 events) and
// `tests/terrain/terrain_mipreq_directed.cpp` case 4 fires it, so its zero in
// any other run is evidence rather than an untested claim.
//
// WHAT IT DOES NOT DO, so nobody reads more into it:
//   * it does not look at the directory's state.  It cannot: the directory has
//     no port that reports one.  It acts on the completion event, which is the
//     thing that CAUSES the state.
//   * it does not compute or check the CRC.  The page's CRC rides the request
//     and comes back on the mip completion as a TOKEN, because
//     TERRAIN.RESIDENCY validates the CRC on every completion it accepts and
//     not only the loader's.  The bench measured what a zero costs there: "16
//     lattices streamed, 17,424 samples delivered, 4,624 mip17 writes -- and
//     EIGHT CRC FAILURES with zero pages resident."
//   * it does not re-order.  First in, first out, so a page's mips are built in
//     the order the pages landed.
//
// Conservative SystemVerilog subset only (charter S2); no package deps.

module zhao_terrain_mipreq #(
    parameter int unsigned SLOTW = 10,   // the DIRECTORY's {set, way} handle
    parameter int unsigned GENW  = 8,
    parameter int unsigned DEPTH = 8     // the composed frame
) (
    input var logic clk,
    input var logic rst_n,

    // -----------------------------------------------------------------------
    // the event: a loader completion the directory ACCEPTED, with `ok` set.
    // A ONE-CYCLE PULSE, which is what `fin_valid && fin_ready && fin_ok` is.
    // -----------------------------------------------------------------------
    input var logic             ev_valid_i,
    input var logic [SLOTW-1:0] ev_slot_i,
    input var logic [GENW-1:0]  ev_gen_i,
    input var logic [31:0]      ev_epoch_i,
    input var logic [31:0]      ev_src_id_i,
    input var logic [31:0]      ev_crc_i,

    // -----------------------------------------------------------------------
    // the mip job out -- TERRAIN.MIPFEED's `j_*` port, field for field
    // -----------------------------------------------------------------------
    output var logic             j_valid_o,
    input  var logic             j_ready_i,
    output var logic [SLOTW-1:0] j_slot_o,
    output var logic [GENW-1:0]  j_gen_o,
    output var logic [31:0]      j_epoch_o,
    output var logic [31:0]      j_src_id_o,
    output var logic [31:0]      j_crc_o,

    // -----------------------------------------------------------------------
    // evidence (spec/counters.md S4: saturate, never wrap)
    // -----------------------------------------------------------------------
    output var logic [31:0] requests_o,   // events seen
    output var logic [31:0] issued_o,     // jobs handed to TERRAIN.MIPFEED
    output var logic [31:0] drops_o,      // events lost to a full queue
    // entries held, 0..DEPTH.  The width is written as `$clog2(DEPTH)` rather
    // than as the body's `PTRW` because a port may not reference a localparam
    // declared inside the module.
    output var logic [$clog2(DEPTH):0] level_o,
    output var logic                   idle_o
);

  localparam int unsigned PTRW = $clog2(DEPTH);

  // Quartus 17.0 needs an elaboration check inside `initial begin ... end`;
  // a bare module-scope `if` is a syntax error there even though Verilator
  // accepts it (CLAUDE.md, build note).
  initial begin
    if (DEPTH < 2)
      $fatal(1, "zhao_terrain_mipreq: DEPTH must be at least 2 (got %0d)", DEPTH);
    if ((DEPTH & (DEPTH - 1)) != 0)
      $fatal(1, "zhao_terrain_mipreq: DEPTH must be a power of two (got %0d)", DEPTH);
  end

  logic [SLOTW-1:0] q_slot  [0:DEPTH-1];
  logic [GENW-1:0]  q_gen   [0:DEPTH-1];
  logic [31:0]      q_epoch [0:DEPTH-1];
  logic [31:0]      q_src   [0:DEPTH-1];
  logic [31:0]      q_crc   [0:DEPTH-1];

  logic [PTRW-1:0] wp_q, rp_q;
  logic [PTRW:0]   lvl_q;

  logic full_c;
  logic push_c;
  logic pop_c;

  assign full_c = (lvl_q == (PTRW+1)'(DEPTH));

  assign j_valid_o  = (lvl_q != '0);
  assign j_slot_o   = q_slot [rp_q];
  assign j_gen_o    = q_gen  [rp_q];
  assign j_epoch_o  = q_epoch[rp_q];
  assign j_src_id_o = q_src  [rp_q];
  assign j_crc_o    = q_crc  [rp_q];

  assign push_c = ev_valid_i && !full_c;
  assign pop_c  = j_valid_o && j_ready_i;

  assign level_o = lvl_q;
  assign idle_o  = (lvl_q == '0);

  function automatic logic [31:0] sat_inc(input logic [31:0] a);
    sat_inc = (a == 32'hFFFF_FFFF) ? a : a + 32'd1;
  endfunction

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      wp_q       <= '0;
      rp_q       <= '0;
      lvl_q      <= '0;
      requests_o <= 32'd0;
      issued_o   <= 32'd0;
      drops_o    <= 32'd0;
      for (int unsigned i = 0; i < DEPTH; i++) begin
        q_slot[i]  <= '0;
        q_gen[i]   <= '0;
        q_epoch[i] <= 32'd0;
        q_src[i]   <= 32'd0;
        q_crc[i]   <= 32'd0;
      end
    end else begin
      if (ev_valid_i) requests_o <= sat_inc(requests_o);

      if (push_c) begin
        q_slot [wp_q] <= ev_slot_i;
        q_gen  [wp_q] <= ev_gen_i;
        q_epoch[wp_q] <= ev_epoch_i;
        q_src  [wp_q] <= ev_src_id_i;
        q_crc  [wp_q] <= ev_crc_i;
        wp_q          <= wp_q + PTRW'(1);
      end else if (ev_valid_i) begin
        // A REQUEST LOST BECAUSE THE QUEUE IS FULL.  Counted rather than
        // dropped in silence: the consequence is a page that is loaded,
        // CRC-verified, sitting in its slot, and never called ground -- which
        // has no other symptom anywhere in the machine.
        drops_o <= sat_inc(drops_o);
      end

      if (pop_c) begin
        rp_q     <= rp_q + PTRW'(1);
        issued_o <= sat_inc(issued_o);
      end

      unique case ({push_c, pop_c})
        2'b10:   lvl_q <= lvl_q + (PTRW+1)'(1);
        2'b01:   lvl_q <= lvl_q - (PTRW+1)'(1);
        default: lvl_q <= lvl_q;
      endcase
    end
  end

endmodule : zhao_terrain_mipreq
