// zhao_terrain_mipfeed.sv -- the adapter that makes a loaded page into ground.
//
// ===========================================================================
// WHAT IT IS FOR
// ===========================================================================
// `zhao_terrain_residency_v2` will not call a page RESIDENT until it has TWO
// completions: the loader's, and a second one saying the page's mips are built.
// `zhao_terrain_mipgen` produces that second completion and consumes a lattice;
// `zhao_terrain_pagestream` produces a lattice and consumes a page slot. This
// block is the twenty lines of sequencing between them, and until it existed
// the composed world layer fetched eight CRC-verified pages and called none of
// them ground.
//
// It is a separate file rather than a few wires in a top because it makes a
// DECISION -- which plane is which surface -- and a decision that lives in a
// wiring diagram is a decision nobody can find.
//
// ===========================================================================
// TWO PASSES, NOT A BUFFER
// ===========================================================================
// TERRAIN.MIPGEN wants FINE*FINE samples per surface, surfaces in order, on one
// 16-bit port. TERRAIN.PAGESTREAM emits FINE*FINE vertices carrying THREE
// planes each. So one lattice pass produces both surfaces' worth of data at
// once and the port can only take one of them.
//
// The choice is between buffering 1,089 x 16 bits of the surface not being sent
// -- 17,424 bits, which is the buffering TERRAIN.PAGESTREAM was arranged
// specifically to avoid -- or streaming the page TWICE and selecting a
// different plane each time. The second pass costs about 3,544 clocks. The
// page load that preceded it cost 6,726, and neither is on the frame's critical
// path because the sequencer never waits for a load to complete.
//
// So: two passes. The block issues PAGESTREAM job, plane A; then PAGESTREAM
// job, plane C; and MIPGEN is started ONCE, before the first, because its
// surface counter runs across both.
//
// ===========================================================================
// WHICH PLANE IS SURFACE 0 -- AN OPEN OWNER RULING, NOT A CHOICE MADE HERE
// ===========================================================================
// Ruling T8 gives the mip law exactly:
//
//     mip17[i,j] = fine33[2*i, 2*j]     mip9[i,j] = fine33[4*i, 4*j]
//     Top and bottom.
//
// and says a page becomes RESIDENT only after "resident mips complete". It does
// NOT say what "top" is at load time, and the two readings are not equivalent:
//
//   * LAYER A, the authored base height. What this block sends today. A page
//     carrying scars would then show UNSCARRED ground at coarse LOD, and the
//     seam between a scarred fine patch and its coarse neighbour would open.
//   * COMPOSE_TOP, `max(fx(base) + fx(scar), fx(bottom))` per terrain_rules
//     3.4. Correct-looking, and it is TERRAIN.PATCH's arithmetic -- so the
//     honest implementation is not to compute it here but to take surface 0's
//     samples FROM TERRAIN.PATCH's compose lane instead of from the page. That
//     is a rewiring of this block's `fine_h_o` source, not a new law in it.
//
// Layer A is what is sent because it is the reading that requires no arithmetic
// this block has no business owning, and because sending SOMETHING is what
// makes the page resident at all. It is flagged in the contract as requiring a
// ruling, and the day the ruling lands the change is one mux, here, named.
//
// Surface 1 is layer C. That one is not in doubt: C IS the bottom height.
// ===========================================================================
`default_nettype none

module zhao_terrain_mipfeed #(
    parameter int unsigned SLOTW = 11,
    parameter int unsigned GENW  = 8
) (
    input var logic clk,
    input var logic rst_n,

    // ---- job in (from TERRAIN.RESIDENCY's mips-stale entry) -----------------
    input  var logic             j_valid_i,
    output var logic             j_ready_o,
    input  var logic [SLOTW-1:0] j_slot_i,
    input  var logic [GENW-1:0]  j_gen_i,
    input  var logic [31:0]      j_epoch_i,
    input  var logic [31:0]      j_src_id_i,
    // THE PAGE'S CRC, CARRIED AND NOT COMPUTED. This block has no opinion about
    // it -- TERRAIN.PAGELOADER checked the page before anyone called it loaded,
    // and nothing here re-reads the body. It is here because
    // TERRAIN.RESIDENCY validates the CRC on EVERY completion it accepts, not
    // only the loader's, so a second `fin` carrying zero is a CRC FAILURE.
    //
    // Measured on 2026-09-07, not anticipated: the first version left it out
    // and the composed suite reported 16 lattices streamed, 17,424 samples
    // delivered, 4,624 mip17 writes -- and eight CRC failures with zero pages
    // resident. Every block had done its job and the page still was not ground.
    input  var logic [31:0]      j_crc_i,

    // ---- TERRAIN.PAGESTREAM ------------------------------------------------
    output var logic             ps_valid_o,
    input  var logic             ps_ready_i,
    output var logic [SLOTW-1:0] ps_slot_o,
    output var logic [GENW-1:0]  ps_gen_o,
    output var logic [31:0]      ps_epoch_o,
    output var logic [31:0]      ps_src_id_o,

    input  var logic               v_valid_i,
    output var logic               v_ready_o,
    input  var logic signed [15:0] v_base_i,
    input  var logic signed [15:0] v_scar_i,
    input  var logic signed [15:0] v_bottom_i,
    input  var logic               v_last_i,

    input  var logic       ps_done_valid_i,
    output var logic       ps_done_ready_o,
    input  var logic       ps_done_ok_i,

    // ---- TERRAIN.MIPGEN ----------------------------------------------------
    output var logic             mg_start_o,
    output var logic [SLOTW-1:0] mg_job_slot_o,
    output var logic [GENW-1:0]  mg_job_gen_o,
    output var logic [31:0]      mg_job_epoch_o,
    output var logic             mg_fine_valid_o,
    input  var logic             mg_fine_ready_i,
    output var logic [15:0]      mg_fine_h_o,
    input  var logic             mg_done_i,

    // ---- the second completion, to TERRAIN.RESIDENCY ------------------------
    output var logic             fin_valid_o,
    input  var logic             fin_ready_i,
    output var logic [SLOTW-1:0] fin_slot_o,
    output var logic [GENW-1:0]  fin_gen_o,
    output var logic [31:0]      fin_epoch_o,
    output var logic             fin_ok_o,
    output var logic [31:0]      fin_crc_o,   // the token above, returned unaltered
    output var logic [31:0]      fin_src_id_o,

    // ---- counters -----------------------------------------------------------
    output var logic [31:0] pages_mipped_o,
    output var logic [31:0] pages_faulted_o,   // a pass the streamer refused
    output var logic [31:0] samples_sent_o,
    output var logic        idle_o
);

  // NO `EDGE` PARAMETER, DELIBERATELY. This block never counts vertices: the
  // end of a pass is TERRAIN.PAGESTREAM's `v_last_i`, which is that block's own
  // idea of how long its lattice is. A second copy of 33 here would be a second
  // thing that has to agree, and its first disagreement would be a pass that
  // ended early with MIPGEN's surface counter half way through -- terrain, in
  // the right shape, decimated from the wrong samples.

  // WHICH PLANE FEEDS WHICH SURFACE. Named constants rather than literals, so
  // the open ruling above has somewhere to land.
  localparam logic [1:0] SURF0_PLANE = 2'd0;   // layer A -- see the header
  localparam logic [1:0] SURF1_PLANE = 2'd2;   // layer C, and this one is not in doubt

  typedef enum logic [2:0] {
    S_IDLE,
    S_START,     // one `start_i` pulse to MIPGEN, before either pass
    S_REQ,       // hand the streamer a job
    S_PASS,      // relay its vertices into MIPGEN
    S_PSDONE,    // take the streamer's completion
    S_WAITMG,    // wait for MIPGEN's done after the second pass
    S_FIN
  } state_e;

  state_e      state_q;
  logic        pass_q;        // 0 = surface 0, 1 = surface 1
  logic        ok_q;

  // MIPGEN'S `done_o` IS A PULSE THAT ARRIVES BEFORE ANYONE IS LOOKING, and
  // this flag is what catches it. Measured, not anticipated: the first version
  // waited for `mg_done_i` in S_WAITMG and hung on the very first page. The
  // last fine sample is taken on the same cycle TERRAIN.PAGESTREAM raises
  // `v_last`, so MIPGEN pulses its completion while this block is still in
  // S_PASS/S_PSDONE collecting the streamer's -- two states before anything
  // reads it. The composed suite reported eight pages loaded, two lattice
  // passes done, 578 mip17 writes and ZERO pages mipped, which is exactly the
  // shape of a pulse nobody caught.
  //
  // Cleared at S_START, which is the only cycle at which no scan can be
  // running, and set BEFORE the case so that clear wins on that cycle.
  logic        mg_done_seen_q;

  logic [SLOTW-1:0] slot_q;
  logic [GENW-1:0]  gen_q;
  logic [31:0]      epoch_q, src_q, crc_q;

  assign j_ready_o = (state_q == S_IDLE);
  assign idle_o    = (state_q == S_IDLE);

  assign ps_valid_o  = (state_q == S_REQ);
  assign ps_slot_o   = slot_q;
  assign ps_gen_o    = gen_q;
  assign ps_epoch_o  = epoch_q;
  assign ps_src_id_o = src_q;

  assign mg_start_o     = (state_q == S_START);
  assign mg_job_slot_o  = slot_q;
  assign mg_job_gen_o   = gen_q;
  assign mg_job_epoch_o = epoch_q;

  // THE SELECT IS COMBINATIONAL OFF `pass_q`, and the plane it picks is a
  // named constant. A `pass_q ? v_bottom_i : v_base_i` would be the same
  // hardware and would put the ruling in an expression nobody greps for.
  logic signed [15:0] sel_c;
  always_comb begin
    unique case (pass_q ? SURF1_PLANE : SURF0_PLANE)
      2'd0:    sel_c = v_base_i;
      2'd1:    sel_c = v_scar_i;
      default: sel_c = v_bottom_i;
    endcase
  end

  assign mg_fine_h_o     = sel_c;
  assign mg_fine_valid_o = (state_q == S_PASS) && v_valid_i;
  // THE STREAMER IS ONLY READY WHEN MIPGEN IS. No skid buffer: MIPGEN is
  // storage-free and takes a sample every cycle it is busy, so a buffer here
  // would exist for a stall that the block it feeds cannot produce. If that
  // ever stops being true this is where it changes.
  assign v_ready_o       = (state_q == S_PASS) && mg_fine_ready_i;

  assign ps_done_ready_o = (state_q == S_PSDONE);

  assign fin_valid_o  = (state_q == S_FIN);
  assign fin_slot_o   = slot_q;
  assign fin_gen_o    = gen_q;
  assign fin_epoch_o  = epoch_q;
  assign fin_ok_o     = ok_q;
  assign fin_crc_o    = crc_q;
  assign fin_src_id_o = src_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state_q         <= S_IDLE;
      pass_q          <= 1'b0;
      ok_q            <= 1'b0;
      mg_done_seen_q  <= 1'b0;
      slot_q          <= '0;
      gen_q           <= '0;
      epoch_q         <= 32'd0;
      src_q           <= 32'd0;
      crc_q           <= 32'd0;
      pages_mipped_o  <= 32'd0;
      pages_faulted_o <= 32'd0;
      samples_sent_o  <= 32'd0;
    end else begin
      if (mg_done_i) mg_done_seen_q <= 1'b1;

      unique case (state_q)

        S_IDLE: begin
          if (j_valid_i) begin
            slot_q  <= j_slot_i;
            gen_q   <= j_gen_i;
            epoch_q <= j_epoch_i;
            src_q   <= j_src_id_i;
            crc_q   <= j_crc_i;
            pass_q  <= 1'b0;
            ok_q    <= 1'b1;
            state_q <= S_START;
          end
        end

        // ONE START FOR BOTH PASSES. MIPGEN's surface counter runs across the
        // whole 2 x FINE*FINE scan and `start_i` RESTARTS it -- a pulse between
        // the passes would send surface 1's samples into surface 0's mip and
        // count an abort, which is the shape of fault that renders as terrain.
        S_START: begin
          mg_done_seen_q <= 1'b0;
          state_q        <= S_REQ;
        end

        S_REQ: if (ps_ready_i) state_q <= S_PASS;

        S_PASS: begin
          if (v_valid_i && v_ready_o) begin
            samples_sent_o <= samples_sent_o + 32'd1;
            if (v_last_i) state_q <= S_PSDONE;
          end
        end

        S_PSDONE: begin
          if (ps_done_valid_i) begin
            // A REFUSED PASS IS STILL A COMPLETION. The directory's entry must
            // not be left parked, so the job finishes with `ok = 0` rather than
            // going quiet -- the same rule TERRAIN.PAGELOADER's contract states
            // and for the same reason.
            if (!ps_done_ok_i) begin
              ok_q            <= 1'b0;
              pages_faulted_o <= pages_faulted_o + 32'd1;
              state_q         <= S_FIN;
            end else if (!pass_q) begin
              pass_q  <= 1'b1;
              state_q <= S_REQ;
            end else begin
              state_q <= S_WAITMG;
            end
          end
        end

        S_WAITMG: begin
          if (mg_done_seen_q || mg_done_i) begin
            pages_mipped_o <= pages_mipped_o + 32'd1;
            state_q        <= S_FIN;
          end
        end

        S_FIN: if (fin_ready_i) state_q <= S_IDLE;

        default: state_q <= S_IDLE;
      endcase
    end
  end

`ifndef SYNTHESIS
  // ENFORCED-BY: tests/terrain/world_composed_directed.cpp phase C
  logic armed_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) armed_q <= 1'b0;
    else armed_q <= 1'b1;
  end

  always_ff @(posedge clk) begin
    if (armed_q) begin
      a_one_start :
      assert (!(mg_start_o && (state_q != S_START)))
      else $error("mipfeed: start pulsed outside S_START");

      a_no_sample_when_idle :
      assert (!(mg_fine_valid_o && (state_q != S_PASS)))
      else $error("mipfeed: offered a sample outside a pass");
    end
  end
`endif

endmodule
