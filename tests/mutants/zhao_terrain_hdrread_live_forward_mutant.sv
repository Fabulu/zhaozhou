// zhao_terrain_hdrread_live_forward_mutant.sv -- A POSITIVE CONTROL. NOT RTL.
//
// WHAT WAS CHANGED, AND IT IS FIVE ASSIGNMENTS AND NOTHING ELSE. The forwarded
// job's five identity fields are driven from the LIVE job port
// (`j_slot_i`, `j_gen_i`, `j_epoch_i`, `j_src_id_i`, `j_flags_i`) instead of
// from this block's own latched copy (`job_*_q`).
//
// WHY THAT PARTICULAR MUTATION. It is not an invented fault -- it is the
// IMPLEMENTATION THE COMPOSER ALREADY HAD. Before `zhao_terrain_hdrread`
// existed, `zhao_console_core` wired TERRAIN.PSMUX client A's five job fields
// straight to `tis_*`, TERRAIN.SEQ's live output, and that was correct while
// the header and the streamer's job were taken on the SAME CYCLE. The header
// reader puts a memory burst in between, which is exactly what makes the live
// port stale by the time the job is forwarded. So this file is the version of
// the block that a careful person writes by transcribing the old wiring, and
// the question it answers is whether
// `terrain_hdrread_directed.cpp:test_the_job_port_may_advance_during_the_burst`
// can tell the two apart.
//
// IT IS EVIDENCE ABOUT THE INSTRUMENT, NOT ABOUT THE DESIGN. Its driver
// (`tests/terrain/terrain_hdrread_mutant_control.cpp`) has INVERTED POLARITY:
// it PASSES when the ordering check FAILS. A green run here means the check in
// the real test is load-bearing; a red one means that check would pass on a
// block with the fault in it, and the check is worthless.
//
// REGENERATE IT if `zhao_terrain_hdrread.sv` changes shape. This is a COPY, and
// a copy of an old version is a positive control for a block that no longer
// exists. `tools/budget/mutant_copy_drift.py` watches for exactly that: if the
// production file has been committed since this one was, this file cannot
// contain what production gained. Refresh it with a three-way merge against the
// revision it was cut from.
//
// THE MODULE IS RENAMED so no source list can elaborate it by mistake.
`default_nettype none

module zhao_terrain_hdrread_live_forward_mutant
  import zhao_pkg::*;
#(
    parameter int unsigned PAGE_BYTES = 21376,   // terrain_rules 2 / 7

    parameter logic [ZHAO_VRAM_ADDR_BITS-1:0] REGION_BASE  = 27'h400_0000,
    parameter int unsigned                    REGION_SLOTS = 1024,

    // ONE BIT WIDER THAN THE POOL NEEDS, the same way every other pool client
    // is: a computed slot of 1,024 must arrive as a REFUSAL rather than as slot
    // 0 with somebody else's page in it.
    parameter int unsigned SLOTW = $clog2(REGION_SLOTS) + 1,
    parameter int unsigned GENW  = 8,

    // The established read shape. 64 bytes, eight packed 64-bit beats, and the
    // whole header fits in one.
    parameter int unsigned BURST_BYTES = 64,

    // The header's restatement of {ver, island, ix, iz} against the record the
    // job came from. Default ON; a parameter for the same reason
    // `zhao_terrain_writeback`'s CHECK_HEADER_IDENT is one.
    parameter bit CHECK_IDENT = 1'b1
) (
    input var logic clk,
    input var logic rst_n,

    // ---- configuration -------------------------------------------------------
    input var zhao_client_e cfg_vram_client_i,
    input var logic [31:0]  cfg_epoch_i,

    // ---- job in: TERRAIN.SEQ's compose door ----------------------------------
    input  var logic             j_valid_i,
    output var logic             j_ready_o,
    input  var logic [SLOTW-1:0] j_slot_i,
    input  var logic [GENW-1:0]  j_gen_i,
    input  var logic [31:0]      j_epoch_i,
    input  var logic [31:0]      j_src_id_i,
    input  var logic [15:0]      j_flags_i,
    input  var logic [7:0]       j_view_mask_i,
    // The record's own identity, for the header's corruption check. These are
    // what the header is compared AGAINST; they are never forwarded in place of
    // what the page says.
    input  var logic [31:0]        j_island_i,
    input  var logic signed [15:0] j_ix_i,
    input  var logic signed [15:0] j_iz_i,

    // ---- MEM.GUARD read client ------------------------------------------------
    output var zhao_guard_req_t guard_req_o,
    input  var zhao_guard_rsp_t guard_rsp_i,
    input  var logic            beat_valid_i,
    input  var logic [63:0]     beat_data_i,
    input  var logic            beat_last_i,

    // ---- the header out, to TERRAIN.PLACE -------------------------------------
    output var logic               h_valid_o,
    input  var logic               h_ready_i,
    output var logic signed [ 7:0] h_pitch_log2_o,   // spec 2.1 +2, or REFUSE
    output var logic signed [15:0] h_patch_ix_o,     // spec 2.1 +8
    output var logic signed [15:0] h_patch_iz_o,     // spec 2.1 +10
    output var logic signed [31:0] h_env_x0_o,       // spec 2.1 +16
    output var logic signed [31:0] h_env_z0_o,
    output var logic        [15:0] h_src_id_o,
    // The verdict rides with the record so a trace can say WHY a patch was
    // refused, rather than only that the pitch was impossible.
    output var logic               h_ok_o,
    output var logic [3:0]         h_verdict_o,

    // ---- the job forwarded, to TERRAIN.PAGESTREAM's job port ------------------
    // Every field is this block's own latched copy. See the header.
    output var logic             f_valid_o,
    input  var logic             f_ready_i,
    output var logic [SLOTW-1:0] f_slot_o,
    output var logic [GENW-1:0]  f_gen_o,
    output var logic [31:0]      f_epoch_o,
    output var logic [31:0]      f_src_id_o,
    output var logic [15:0]      f_flags_o,
    output var logic [7:0]       f_view_mask_o,

    // ---- counters -------------------------------------------------------------
    output var logic [31:0] headers_read_o,      // a header returned and passed
    output var logic [31:0] headers_refused_o,   // ...and did not
    output var logic [31:0] guard_denied_o,
    output var logic [31:0] incomplete_o,        // the burst ended short
    output var logic [31:0] ident_fails_o,
    output var logic        idle_o
);

  localparam int unsigned BEATS  = BURST_BYTES / 8;        // 8
  localparam int unsigned BEATCW = $clog2(BEATS + 1);      // 4

  localparam logic [3:0] V_OK         = 4'd0;
  localparam logic [3:0] V_SLOT_OOR   = 4'd1;   // slot >= REGION_SLOTS
  localparam logic [3:0] V_EPOCH      = 4'd2;   // job epoch != cfg_epoch_i
  localparam logic [3:0] V_GUARD      = 4'd3;   // MEM.GUARD refused the read
  localparam logic [3:0] V_INCOMPLETE = 4'd4;   // the burst returned short
  localparam logic [3:0] V_IDENT      = 4'd5;   // the header names another page

  // NOT A LEGAL PITCH AND THAT IS ITS ONLY JOB. spec/terrain_rules.md 1.3
  // freezes pitch_log2 to {-1, 0, +1, +2}; 127 is outside it by 125.
  localparam logic signed [7:0] HDR_PITCH_REFUSE = 8'sd127;

`ifndef SYNTHESIS
  initial begin
    if ((BURST_BYTES % 8) != 0) begin
      $fatal(1, "hdrread: BURST_BYTES must be whole 64-bit beats");
    end
    // The whole header must arrive in the ONE burst this block issues. A
    // shorter burst would silently return a header with the envelope missing,
    // and the envelope's absence reads as zero, which is a real fx16 world
    // coordinate.
    if (BURST_BYTES < 64) begin
      $fatal(1, "hdrread: spec/terrain_rules.md 2.1's header is 64 bytes and this block reads it in one burst");
    end
    // THE PAGE BASE IS HELD AT THE GUARD'S OWN WIDTH, so this is the check that
    // makes that safe rather than a truncation. With the slot pre-check below,
    // the highest address this block can compute is
    // REGION_BASE + (REGION_SLOTS-1) * PAGE_BYTES; if that does not fit the
    // 27-bit VRAM map, a high slot would WRAP to a low address and read another
    // region's bytes as a patch header. Refused at elaboration.
    if ((32'(REGION_BASE) + (32'(REGION_SLOTS) * 32'(PAGE_BYTES)))
        > (32'd1 << ZHAO_VRAM_ADDR_BITS)) begin
      $fatal(1, "hdrread: the pool runs past the %0d-bit VRAM map", ZHAO_VRAM_ADDR_BITS);
    end
  end
`endif

  // slot * PAGE_BYTES with no multiplier -- see the header's note about this
  // being the third copy. 21,376 = 2^14 + 2^12 + 2^9 + 2^8 + 2^7.
  function automatic logic [31:0] slot_scaled(input logic [SLOTW-1:0] s);
    logic [31:0] acc;
    begin
      acc = 32'd0;
      for (int unsigned b = 0; b < 32; b++) begin
        if (((PAGE_BYTES >> b) & 32'd1) != 32'd0) begin
          acc = acc + ({{(32 - SLOTW) {1'b0}}, s} << b);
        end
      end
      slot_scaled = acc;
    end
  endfunction

  // ---- the job, latched ----------------------------------------------------
  logic [SLOTW-1:0] job_slot_q;
  logic [GENW-1:0]  job_gen_q;
  logic [31:0]      job_epoch_q, job_src_q;
  logic [15:0]      job_flags_q;
  logic [7:0]       job_view_mask_q;
  logic [31:0]      job_island_q;
  logic signed [15:0] job_ix_q, job_iz_q;
  logic [ZHAO_VRAM_ADDR_BITS-1:0] page_base_q;

  // ---- the header, as it comes off the wire --------------------------------
  logic [15:0]        hdr_ver_q;
  logic signed [7:0]  hdr_pitch_q;
  logic [31:0]        hdr_island_q;
  logic signed [15:0] hdr_ix_q, hdr_iz_q;
  logic signed [31:0] hdr_x0_q, hdr_z0_q;

  logic [3:0]        verdict_q;
  logic [BEATCW-1:0] beat_q;

  typedef enum logic [2:0] {
    S_IDLE,
    S_REQ,
    S_VERD,
    S_WAIT,
    S_CHECK,
    S_EMIT,
    S_FWD
  } state_e;

  state_e state_q;

  assign j_ready_o = (state_q == S_IDLE);
  assign idle_o    = (state_q == S_IDLE);

  // THE REFUSAL MUST NOT ISSUE THE READ IT IS REFUSING -- the same shape
  // TERRAIN.PAGESTREAM and TERRAIN.WRITEBACK both use, so `guard_denied_o`
  // stays a measurement of the GUARD rather than of this block's bookkeeping.
  always_comb begin
    guard_req_o        = '0;
    guard_req_o.valid  = (state_q == S_REQ);
    guard_req_o.write  = 1'b0;
    guard_req_o.client = cfg_vram_client_i;
    guard_req_o.addr   = page_base_q;
    guard_req_o.len    = 7'(BURST_BYTES);
    guard_req_o.be     = '1;
  end

  wire ok_c = (verdict_q == V_OK);

  assign h_valid_o      = (state_q == S_EMIT);
  assign h_ok_o         = ok_c;
  assign h_verdict_o    = verdict_q;
  assign h_pitch_log2_o = ok_c ? hdr_pitch_q : HDR_PITCH_REFUSE;
  // The coordinate and the envelope come from the PAGE when the page was read
  // and from the JOB's own record when it was not, so a refused header never
  // presents a coordinate the machine has not seen. The pitch above is what
  // makes the patch refuse either way.
  assign h_patch_ix_o   = ok_c ? hdr_ix_q : job_ix_q;
  assign h_patch_iz_o   = ok_c ? hdr_iz_q : job_iz_q;
  assign h_env_x0_o     = ok_c ? hdr_x0_q : 32'sd0;
  assign h_env_z0_o     = ok_c ? hdr_z0_q : 32'sd0;
  assign h_src_id_o     = job_src_q[15:0];

  assign f_valid_o  = (state_q == S_FWD);
  assign f_slot_o   = j_slot_i;    // MUTATION
  assign f_gen_o    = j_gen_i;     // MUTATION
  assign f_epoch_o  = j_epoch_i;   // MUTATION
  assign f_src_id_o = j_src_id_i;  // MUTATION
  assign f_flags_o  = j_flags_i;   // MUTATION
  // NOT MUTATED. The mutation is the five fields this file's header names, and
  // widening it to a sixth would make the control measure something bigger
  // than the defect it is the control for. The view mask is latched here
  // exactly as production latches it.
  assign f_view_mask_o = job_view_mask_q;

  wire pre_slot_bad_c  = (32'({{(32-SLOTW){1'b0}}, j_slot_i}) >= 32'(REGION_SLOTS));
  wire pre_epoch_bad_c = (j_epoch_i != cfg_epoch_i);

  // The header's restatement against the record it was issued from.
  wire ident_bad_c = CHECK_IDENT
                   && ((hdr_ver_q != 16'd1) || (hdr_island_q != job_island_q)
                       || (hdr_ix_q != job_ix_q) || (hdr_iz_q != job_iz_q));

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state_q           <= S_IDLE;
      job_slot_q        <= '0;
      job_gen_q         <= '0;
      job_epoch_q       <= 32'd0;
      job_src_q         <= 32'd0;
      job_flags_q       <= 16'd0;
      job_view_mask_q   <= 8'd0;
      job_island_q      <= 32'd0;
      job_ix_q          <= 16'sd0;
      job_iz_q          <= 16'sd0;
      page_base_q       <= '0;
      hdr_ver_q         <= 16'd0;
      hdr_pitch_q       <= 8'sd0;
      hdr_island_q      <= 32'd0;
      hdr_ix_q          <= 16'sd0;
      hdr_iz_q          <= 16'sd0;
      hdr_x0_q          <= 32'sd0;
      hdr_z0_q          <= 32'sd0;
      verdict_q         <= V_OK;
      beat_q            <= '0;
      headers_read_o    <= 32'd0;
      headers_refused_o <= 32'd0;
      guard_denied_o    <= 32'd0;
      incomplete_o      <= 32'd0;
      ident_fails_o     <= 32'd0;
    end else begin
      unique case (state_q)

        S_IDLE: begin
          if (j_valid_i) begin
            job_slot_q   <= j_slot_i;
            job_gen_q    <= j_gen_i;
            job_epoch_q  <= j_epoch_i;
            job_src_q    <= j_src_id_i;
            job_flags_q  <= j_flags_i;
            job_view_mask_q <= j_view_mask_i;
            job_island_q <= j_island_i;
            job_ix_q     <= j_ix_i;
            job_iz_q     <= j_iz_i;
            page_base_q  <= ZHAO_VRAM_ADDR_BITS'(32'(REGION_BASE) + slot_scaled(j_slot_i));
            beat_q       <= '0;
            // EVERY CAPTURED FIELD IS CLEARED PER JOB. Carrying the previous
            // page's header into a job whose read then fails is exactly the
            // stale-copy fault this block was built to remove, one level in.
            hdr_ver_q    <= 16'd0;
            hdr_pitch_q  <= 8'sd0;
            hdr_island_q <= 32'd0;
            hdr_ix_q     <= 16'sd0;
            hdr_iz_q     <= 16'sd0;
            hdr_x0_q     <= 32'sd0;
            hdr_z0_q     <= 32'sd0;
            if (pre_slot_bad_c) begin
              verdict_q <= V_SLOT_OOR;
              state_q   <= S_EMIT;
            end else if (pre_epoch_bad_c) begin
              verdict_q <= V_EPOCH;
              state_q   <= S_EMIT;
            end else begin
              verdict_q <= V_OK;
              state_q   <= S_REQ;
            end
          end
        end

        S_REQ: begin
          // TWO CYCLES, LEVEL THEN PULSE -- MEM.GUARD's protocol. `rsp.ready`
          // is a LEVEL and `rsp.ok` a PULSE one cycle after the accept, so the
          // verdict gets its own state. A block that tested both on the same
          // cycle would read every pass as a denial.
          // ENFORCED-BY: tools/rtl/check_guard_verdict.py
          if (guard_rsp_i.violation) begin
            guard_denied_o <= guard_denied_o + 32'd1;
            verdict_q      <= V_GUARD;
            state_q        <= S_EMIT;
          end else if (guard_rsp_i.ready) begin
            state_q <= S_VERD;
          end
        end

        S_VERD: begin
          if (guard_rsp_i.violation) begin
            guard_denied_o <= guard_denied_o + 32'd1;
            verdict_q      <= V_GUARD;
            state_q        <= S_EMIT;
          end else if (guard_rsp_i.ok) begin
            state_q <= S_WAIT;
          end
        end

        S_WAIT: begin
          if (guard_rsp_i.violation) begin
            guard_denied_o <= guard_denied_o + 32'd1;
            verdict_q      <= V_GUARD;
            state_q        <= S_EMIT;
          end else if (beat_valid_i) begin
            // spec/terrain_rules.md 2.1, little-endian. Beats 5..7 are the
            // reserved tail and carry nothing this block wants.
            if (beat_q == BEATCW'(0)) begin
              hdr_ver_q    <= beat_data_i[15:0];
              hdr_pitch_q  <= signed'(beat_data_i[23:16]);
              hdr_island_q <= beat_data_i[63:32];
            end
            if (beat_q == BEATCW'(1)) begin
              hdr_ix_q <= signed'(beat_data_i[15:0]);
              hdr_iz_q <= signed'(beat_data_i[31:16]);
            end
            if (beat_q == BEATCW'(2)) begin
              hdr_x0_q <= signed'(beat_data_i[31:0]);
              hdr_z0_q <= signed'(beat_data_i[63:32]);
            end
            if (beat_last_i) begin
              // A BURST THAT ENDED EARLY IS NOT A BURST. The same refusal
              // TERRAIN.PAGESTREAM makes, and here it matters more: a header
              // cut off before beat 2 has NO envelope, and a missing envelope
              // reads as the world origin, which is a real place.
              if (beat_q != BEATCW'(BEATS - 1)) begin
                incomplete_o <= incomplete_o + 32'd1;
                verdict_q    <= V_INCOMPLETE;
                state_q      <= S_EMIT;
              end else begin
                state_q <= S_CHECK;
              end
            end else begin
              beat_q <= beat_q + BEATCW'(1);
            end
          end
        end

        S_CHECK: begin
          // The identity test runs on the cycle AFTER the last beat landed, so
          // every captured field is settled before it is compared.
          if (ident_bad_c) begin
            ident_fails_o <= ident_fails_o + 32'd1;
            verdict_q     <= V_IDENT;
          end
          state_q <= S_EMIT;
        end

        S_EMIT: begin
          if (h_ready_i) begin
            if (verdict_q == V_OK) headers_read_o    <= headers_read_o + 32'd1;
            else                   headers_refused_o <= headers_refused_o + 32'd1;
            state_q <= S_FWD;
          end
        end

        S_FWD: begin
          // The page streams whatever the header said, because the streamer's
          // completion is the page's unpin.
          if (f_ready_i) state_q <= S_IDLE;
        end

        default: state_q <= S_IDLE;
      endcase
    end
  end

`ifndef SYNTHESIS
  // ONE HEADER OUT PER JOB IN, AND ONE JOB FORWARDED PER JOB IN. The two are
  // asserted separately because they fail differently: a missing header is a
  // patch placed from stale fields, and a missing forward is a page that never
  // unpins.
  // ENFORCED-BY: tests/terrain/terrain_hdrread_directed.cpp
  int unsigned dbg_jobs_in, dbg_hdrs_out, dbg_fwd_out;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      dbg_jobs_in  <= 0;
      dbg_hdrs_out <= 0;
      dbg_fwd_out  <= 0;
    end else begin
      if (j_valid_i && j_ready_o) dbg_jobs_in  <= dbg_jobs_in + 1;
      if (h_valid_o && h_ready_i) dbg_hdrs_out <= dbg_hdrs_out + 1;
      if (f_valid_o && f_ready_i) dbg_fwd_out  <= dbg_fwd_out + 1;
      a_one_header_per_job: assert (dbg_hdrs_out <= dbg_jobs_in)
        else $error("hdrread: more headers emitted than jobs accepted");
      a_one_forward_per_job: assert (dbg_fwd_out <= dbg_hdrs_out)
        else $error("hdrread: a job was forwarded without its header");
    end
  end
`endif

endmodule

`default_nettype wire
