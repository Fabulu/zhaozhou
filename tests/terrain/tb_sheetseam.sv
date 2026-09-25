// tb_sheetseam.sv -- THE SHEET SEAM, END TO END, AGAINST THE REAL STORE.
//
// ---------------------------------------------------------------------------
// WHAT IS REAL AND WHAT IS PLAYED
// ---------------------------------------------------------------------------
// REAL, all three, wired exactly as `zhao_console_core` would wire them:
//
//   zhao_surface_sheet       the store itself, `Slots = 2`, with its own
//                            residency directory and its own 4,096-cycle
//                            clear sweep.  NOT a played model: the whole
//                            point of this bench is that `ST_HIT` /
//                            `ST_MISS` come from the block that defines
//                            them, so the seam's mirrored `StHit` localparam
//                            is checked against the store's rather than
//                            against a copy of itself.
//   zhao_surface_sheetshare  the three-client share (client C idle here).
//   zhao_terrain_sheetseam   the DUT.
//
// PLAYED: the two ENDS.  Client A stands where SURFACE.STAMP stands and is
// driven straight from C++ -- ACQUIRE, READ, RELEASE and the write port -- and
// the dig face stands where `zhao_terrain_bake_v2` stands.  Both are driven
// rather than instantiated on purpose: this bench's question is what the seam
// does, and putting the real stamp and the real bake in it would make every
// failure a three-way argument about whose timing was wrong.  The REAL bake is
// already held to the real law by `terrain_bake_v2_sheet_directed`, and the
// address law by `terrain_stampdepth_directed`; what neither of those can see
// is the store, which is what this one holds.
//
// ---------------------------------------------------------------------------
// THE SECOND SHARE INSTANCE, AND WHY IT IS HERE
// ---------------------------------------------------------------------------
// `u_probe_share` is a SECOND `zhao_surface_sheetshare`, connected to nothing
// but the bench.  `pg_orphan_o` and `pg_op_mismatch_o` are fault counters that
// read zero forever in the real chain -- a response cannot be orphaned while
// the store only answers requests -- and CLAUDE.md is explicit that a detector
// reading zero is a claim and the claim to check hardest.  At the SHARE'S OWN
// PORT those are ordinary inputs and a bench owns them, which is exactly the
// distinction `zhao_terrain_psmux` draws for `stray_v_o` ("legal at this
// module's port, because these are inputs and a bench owns them") and the one
// `zhao_terrain_pageio` draws between its five stimulus-fired counters and
// `wq_overflow_o`.  So no mutant is needed and none is written.
//
// Conservative SystemVerilog subset only.

`default_nettype none

module tb_sheetseam (
    input var logic clk,
    input var logic rst_n,

    // =======================================================================
    // CLIENT A -- where SURFACE.STAMP stands
    // =======================================================================
    input  var logic        a_req_valid_i,
    output var logic        a_req_ready_o,
    input  var logic [ 1:0] a_req_op_i,
    input  var logic [31:0] a_req_handle_i,
    input  var logic [11:0] a_req_texel_i,
    input  var logic [15:0] a_req_src_id_i,
    input  var logic        a_pg_ready_i,
    output var logic        a_pg_valid_o,

    // the store's response, broadcast (the share routes only the valid)
    output var logic [ 1:0] pg_status_o,
    output var logic [ 7:0] pg_tag_o,
    output var logic [ 7:0] pg_strength_o,
    output var logic [15:0] pg_src_id_o,

    // =======================================================================
    // THE STORE'S WRITE PORT -- SURFACE.STAMP's, driven from C++ so the sheet
    // can be given real content before a bake reads it
    // =======================================================================
    input  var logic        wr_valid_i,
    output var logic        wr_ready_o,
    input  var logic [31:0] wr_handle_i,
    input  var logic [11:0] wr_texel_i,
    input  var logic [ 7:0] wr_tag_i,
    input  var logic [ 7:0] wr_strength_i,
    input  var logic        wr_we_tag_i,
    input  var logic        wr_we_strength_i,
    input  var logic [15:0] wr_src_id_i,
    output var logic        wr_miss_o,

    output var logic [1:0] res_occupancy_o,
    output var logic       res_busy_o,
    output var logic       res_overflow_o,

    // =======================================================================
    // THE SEAM'S JOB FACE -- where the bake-record producer stands
    // =======================================================================
    input  var logic        job_valid_i,
    output var logic        job_ready_o,
    input  var logic [31:0] job_handle_i,
    input  var logic        job_want_sheet_i,
    input  var logic [15:0] job_src_id_i,

    // =======================================================================
    // THE SEAM'S BAKE FACE -- where zhao_terrain_bake_v2's cmd port stands
    // =======================================================================
    output var logic bk_valid_o,
    input  var logic bk_ready_i,
    output var logic bk_depth_sheet_o,
    output var logic bk_fallback_o,

    // =======================================================================
    // THE DIG FACE -- where bake's vertex spine stands
    // =======================================================================
    input  var logic [11:0] sheet_texel_i,
    output var logic [ 7:0] sheet_strength_o,
    output var logic [ 7:0] sheet_before_o,
    output var logic        str_valid_o,
    input  var logic        dig_ready_i,
    input  var logic        bake_done_i,

    // ---- the `stamp_results` sink, OWNER RULING R231 ----------------------
    // Driven from C++ standing where `zhao_surface_stamp.res_*` stands.
    input  var logic        sr_valid_i,
    output var logic        sr_ready_o,
    input  var logic [31:0] sr_handle_i,
    input  var logic [11:0] sr_texel_i,
    input  var logic [ 7:0] sr_before_i,

    // =======================================================================
    // EVIDENCE
    // =======================================================================
    output var logic [31:0] jobs_o,
    output var logic [31:0] sheet_served_o,
    output var logic [31:0] fallbacks_o,
    output var logic [31:0] miss_texels_o,
    output var logic [31:0] prefetch_beats_o,
    output var logic [31:0] refetches_o,
    output var logic [31:0] dig_stall_cycles_o,
    output var logic [31:0] bad_texels_o,
    output var logic [31:0] stray_done_o,
    output var logic [31:0] before_texels_o,
    output var logic [31:0] sr_dropped_o,
    output var logic [31:0] before_torn_o,
    output var logic        seam_idle_o,

    output var logic        share_busy_o,
    output var logic [ 1:0] share_owner_o,
    output var logic [31:0] share_a_reqs_o,
    output var logic [31:0] share_b_reqs_o,
    output var logic [31:0] share_pg_orphan_o,
    output var logic [31:0] share_pg_op_mismatch_o,

    // ---- the seam's OWN request wires, exposed so ITEM 1 is MEASURED ------
    // The block's header claims it issues `OP_READ` and nothing else -- never
    // `OP_ACQUIRE`, which would allocate a blank sheet and serve R221's
    // refused "dig zero", and never `OP_RELEASE`. A claim in a header is not
    // evidence; `sheetseam_rtl_directed` samples these on every cycle of
    // every case and refuses any other opcode.
    output var logic        b_req_valid_o,
    output var logic [ 1:0] b_req_op_o,

    // ---- the standalone share, for its two fault counters -----------------
    input  var logic        p_s_pg_valid_i,
    input  var logic [ 1:0] p_s_pg_op_i,
    input  var logic        p_a_req_valid_i,
    input  var logic [ 1:0] p_a_req_op_i,
    input  var logic        p_s_req_ready_i,
    input  var logic        p_a_pg_ready_i,
    output var logic [31:0] p_pg_orphan_o,
    output var logic [31:0] p_pg_op_mismatch_o,

    // ---- the address law, exposed so the C++ never respells it ------------
    // `zhao_terrain_stampdepth` IS section 9.3(b).  The test drives
    // `law_vi_i`/`law_vj_i` and reads `law_texel_o` instead of writing
    // `2*vi` in C++, because a bench that recomputes the law it is checking
    // agrees with itself for free.
    input  var logic [ 5:0] law_vi_i,
    input  var logic [ 5:0] law_vj_i,
    output var logic [11:0] law_texel_o
);


  // ---- pins that exist on a real block and have no consumer HERE ----------
  // Named rather than left empty: `verilator --lint-only -Wall` rejects
  // `.port()` with PINCONNECTEMPTY, and an empty pin is also the shape that
  // hides a port somebody meant to connect.
  logic [15:0] nc_wr_miss_src_id;
  logic [31:0] nc_sheet_touched;
  logic        nc_sheet_idle;
  logic        nc_p_a_req_ready;
  logic        nc_c_req_ready, nc_c_pg_valid;
  logic [31:0] nc_c_reqs;
  logic        nc_p_c_req_ready, nc_p_c_pg_valid;
  logic [31:0] nc_p_c_reqs;
  logic        nc_p_a_pg_valid;
  logic        nc_p_b_req_ready;
  logic        nc_p_b_pg_valid;
  logic        nc_p_s_req_valid;
  logic [ 1:0] nc_p_s_req_op;
  logic [31:0] nc_p_s_req_handle;
  logic [11:0] nc_p_s_req_texel;
  logic [15:0] nc_p_s_req_src_id;
  logic        nc_p_s_pg_ready;
  logic        nc_p_busy;
  logic [ 1:0] nc_p_owner;
  logic [31:0] nc_p_a_reqs;
  logic [31:0] nc_p_b_reqs;
  logic signed [31:0] nc_law_fx16;
  logic signed [31:0] nc_law_h16;
  logic        nc_law_cov;

  // ---- store <-> share ----------------------------------------------------
  logic        s_req_valid;
  logic        s_req_ready;
  logic [ 1:0] s_req_op;
  logic [31:0] s_req_handle;
  logic [11:0] s_req_texel;
  logic [15:0] s_req_src_id;
  logic        s_pg_valid;
  logic        s_pg_ready;
  logic [ 1:0] s_pg_op;

  // ---- share <-> seam (client B) ------------------------------------------
  logic        b_req_valid;
  logic        b_req_ready;
  logic [ 1:0] b_req_op;
  logic [31:0] b_req_handle;
  logic [11:0] b_req_texel;
  logic [15:0] b_req_src_id;
  logic        b_pg_valid;
  logic        b_pg_ready;

  assign b_req_valid_o = b_req_valid;
  assign b_req_op_o    = b_req_op;

  // =========================================================================
  // THE REAL STORE
  // =========================================================================
  zhao_surface_sheet #(
      .Slots(2)
  ) u_sheet (
      .clk(clk),
      .rst_n(rst_n),

      .req_valid_i(s_req_valid),
      .req_ready_o(s_req_ready),
      .req_op_i(s_req_op),
      .req_handle_i(s_req_handle),
      .req_texel_i(s_req_texel),
      .req_src_id_i(s_req_src_id),

      .pg_valid_o(s_pg_valid),
      .pg_ready_i(s_pg_ready),
      .pg_op_o(s_pg_op),
      .pg_status_o(pg_status_o),
      .pg_tag_o(pg_tag_o),
      .pg_strength_o(pg_strength_o),
      .pg_src_id_o(pg_src_id_o),

      .wr_valid_i(wr_valid_i),
      .wr_ready_o(wr_ready_o),
      .wr_handle_i(wr_handle_i),
      .wr_texel_i(wr_texel_i),
      .wr_tag_i(wr_tag_i),
      .wr_strength_i(wr_strength_i),
      .wr_we_tag_i(wr_we_tag_i),
      .wr_we_strength_i(wr_we_strength_i),
      .wr_src_id_i(wr_src_id_i),
      .wr_miss_o(wr_miss_o),
      .wr_miss_src_id_o(nc_wr_miss_src_id),

      .res_occupancy_o(res_occupancy_o),
      .res_busy_o(res_busy_o),
      .res_overflow_o(res_overflow_o),

      .surface_texels_touched_o(nc_sheet_touched),
      .idle_o(nc_sheet_idle)
  );

  // =========================================================================
  // THE REAL SHARE
  // =========================================================================
  zhao_surface_sheetshare u_share (
      .clk(clk),
      .rst_n(rst_n),

      .a_req_valid_i(a_req_valid_i),
      .a_req_ready_o(a_req_ready_o),
      .a_req_op_i(a_req_op_i),
      .a_req_handle_i(a_req_handle_i),
      .a_req_texel_i(a_req_texel_i),
      .a_req_src_id_i(a_req_src_id_i),
      .a_pg_valid_o(a_pg_valid_o),
      .a_pg_ready_i(a_pg_ready_i),

      .b_req_valid_i(b_req_valid),
      .b_req_ready_o(b_req_ready),
      .b_req_op_i(b_req_op),
      .b_req_handle_i(b_req_handle),
      .b_req_texel_i(b_req_texel),
      .b_req_src_id_i(b_req_src_id),
      .b_pg_valid_o(b_pg_valid),
      .b_pg_ready_i(b_pg_ready),

      // CLIENT C -- TEXTURE.AUX.V2 (share widened 2 -> 3 on 2026-09-25).
      // This bench is TERRAIN.SHEETSEAM's and drives it IDLE, which is the
      // point: the seam's measured 1,089 texels and 1,091 prefetch cycles
      // must not move when a third client exists and never asks. The third
      // client is exercised where it lives, in
      // tests/prod/terrainaux_acceptance.cpp.
      .c_req_valid_i(1'b0),
      .c_req_ready_o(nc_c_req_ready),
      .c_req_op_i(2'd0),
      .c_req_handle_i(32'd0),
      .c_req_texel_i(12'd0),
      .c_req_src_id_i(16'd0),
      .c_pg_valid_o(nc_c_pg_valid),
      .c_pg_ready_i(1'b1),

      .s_req_valid_o(s_req_valid),
      .s_req_ready_i(s_req_ready),
      .s_req_op_o(s_req_op),
      .s_req_handle_o(s_req_handle),
      .s_req_texel_o(s_req_texel),
      .s_req_src_id_o(s_req_src_id),
      .s_pg_valid_i(s_pg_valid),
      .s_pg_ready_o(s_pg_ready),
      .s_pg_op_i(s_pg_op),

      .busy_o(share_busy_o),
      .owner_o(share_owner_o),
      .a_reqs_o(share_a_reqs_o),
      .b_reqs_o(share_b_reqs_o),
      .c_reqs_o(nc_c_reqs),
      .pg_orphan_o(share_pg_orphan_o),
      .pg_op_mismatch_o(share_pg_op_mismatch_o)
  );

  // =========================================================================
  // THE DUT
  // =========================================================================
  zhao_terrain_sheetseam u_seam (
      .clk(clk),
      .rst_n(rst_n),

      .job_valid_i(job_valid_i),
      .job_ready_o(job_ready_o),
      .job_handle_i(job_handle_i),
      .job_want_sheet_i(job_want_sheet_i),
      .job_src_id_i(job_src_id_i),

      .bk_valid_o(bk_valid_o),
      .bk_ready_i(bk_ready_i),
      .bk_depth_sheet_o(bk_depth_sheet_o),
      .bk_fallback_o(bk_fallback_o),

      .sheet_texel_i(sheet_texel_i),
      .sheet_strength_o(sheet_strength_o),
      .sheet_before_o(sheet_before_o),
      .str_valid_o(str_valid_o),
      .dig_ready_i(dig_ready_i),
      .bake_done_i(bake_done_i),

      .sr_valid_i(sr_valid_i),
      .sr_ready_o(sr_ready_o),
      .sr_handle_i(sr_handle_i),
      .sr_texel_i(sr_texel_i),
      .sr_before_i(sr_before_i),

      .req_valid_o(b_req_valid),
      .req_ready_i(b_req_ready),
      .req_op_o(b_req_op),
      .req_handle_o(b_req_handle),
      .req_texel_o(b_req_texel),
      .req_src_id_o(b_req_src_id),

      .pg_valid_i(b_pg_valid),
      .pg_ready_o(b_pg_ready),
      .pg_status_i(pg_status_o),
      .pg_strength_i(pg_strength_o),

      .jobs_o(jobs_o),
      .sheet_served_o(sheet_served_o),
      .fallbacks_o(fallbacks_o),
      .miss_texels_o(miss_texels_o),
      .prefetch_beats_o(prefetch_beats_o),
      .refetches_o(refetches_o),
      .dig_stall_cycles_o(dig_stall_cycles_o),
      .bad_texels_o(bad_texels_o),
      .stray_done_o(stray_done_o),
      .before_texels_o(before_texels_o),
      .sr_dropped_o(sr_dropped_o),
      .before_torn_o(before_torn_o),
      .idle_o(seam_idle_o)
  );

  // =========================================================================
  // THE STANDALONE SHARE -- fault stimulus only, drives nothing real
  // =========================================================================
  zhao_surface_sheetshare u_probe_share (
      .clk(clk),
      .rst_n(rst_n),

      .a_req_valid_i(p_a_req_valid_i),
      .a_req_ready_o(nc_p_a_req_ready),
      .a_req_op_i(p_a_req_op_i),
      .a_req_handle_i(32'd0),
      .a_req_texel_i(12'd0),
      .a_req_src_id_i(16'd0),
      .a_pg_valid_o(nc_p_a_pg_valid),
      .a_pg_ready_i(p_a_pg_ready_i),

      .b_req_valid_i(1'b0),
      .b_req_ready_o(nc_p_b_req_ready),
      .b_req_op_i(2'd0),
      .b_req_handle_i(32'd0),
      .b_req_texel_i(12'd0),
      .b_req_src_id_i(16'd0),
      .b_pg_valid_o(nc_p_b_pg_valid),
      .b_pg_ready_i(1'b1),

      .c_req_valid_i(1'b0),
      .c_req_ready_o(nc_p_c_req_ready),
      .c_req_op_i(2'd0),
      .c_req_handle_i(32'd0),
      .c_req_texel_i(12'd0),
      .c_req_src_id_i(16'd0),
      .c_pg_valid_o(nc_p_c_pg_valid),
      .c_pg_ready_i(1'b1),

      .s_req_valid_o(nc_p_s_req_valid),
      .s_req_ready_i(p_s_req_ready_i),
      .s_req_op_o(nc_p_s_req_op),
      .s_req_handle_o(nc_p_s_req_handle),
      .s_req_texel_o(nc_p_s_req_texel),
      .s_req_src_id_o(nc_p_s_req_src_id),
      .s_pg_valid_i(p_s_pg_valid_i),
      .s_pg_ready_o(nc_p_s_pg_ready),
      .s_pg_op_i(p_s_pg_op_i),

      .busy_o(nc_p_busy),
      .owner_o(nc_p_owner),
      .a_reqs_o(nc_p_a_reqs),
      .b_reqs_o(nc_p_b_reqs),
      .c_reqs_o(nc_p_c_reqs),
      .pg_orphan_o(p_pg_orphan_o),
      .pg_op_mismatch_o(p_pg_op_mismatch_o)
  );

  // =========================================================================
  // SECTION 9.3(b), FROM THE MODULE THAT STATES IT
  // =========================================================================
  zhao_terrain_stampdepth u_law (
      .vi_i(law_vi_i),
      .vj_i(law_vj_i),
      .texel_o(law_texel_o),
      .strength_i(8'd0),
      .depth_fx16_o(nc_law_fx16),
      .depth_h16_o(nc_law_h16),
      .covered_o(nc_law_cov)
  );

  // Every `nc_*` above is genuinely unread; this states it in one place
  // rather than leaving twenty individually-unused warnings.
  /* verilator lint_off UNUSEDSIGNAL */
  wire _unused_nc = &{1'b0, nc_wr_miss_src_id, nc_sheet_touched, nc_sheet_idle,
                      nc_p_a_req_ready, nc_p_a_pg_valid, nc_p_b_req_ready,
                      nc_p_b_pg_valid, nc_p_s_req_valid, nc_p_s_req_op,
                      nc_p_s_req_handle, nc_p_s_req_texel, nc_p_s_req_src_id,
                      nc_p_s_pg_ready, nc_p_busy, nc_p_owner, nc_p_a_reqs,
                      nc_p_b_reqs, nc_law_fx16, nc_law_h16, nc_law_cov,
                      nc_c_req_ready, nc_c_pg_valid, nc_c_reqs,
                      nc_p_c_req_ready, nc_p_c_pg_valid, nc_p_c_reqs};
  /* verilator lint_on UNUSEDSIGNAL */

endmodule

`default_nettype wire
