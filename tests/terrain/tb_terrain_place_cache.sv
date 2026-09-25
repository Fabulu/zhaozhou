// tb_terrain_place_cache.sv -- the PLACEMENT seam, from the shifter to the
// tessellator's read port.
//
//     TERRAIN.PLACE.pos_*  ->  TERRAIN.COMPCACHE.pos_*  ->  lat_wx_o / lat_wz_o
//
// TWO BLOCKS, ONE SEAM, AND IT IS THE ONLY NEW SEAM IN THE COMPOSE ENGINE THAT
// NOTHING ELSE COVERS. `terrain_place_directed.cpp` checks TERRAIN.PLACE alone
// against spec/terrain_rules.md 1.3/2.1 and fires all four of its counters.
// `compcache_front_rtl_directed.cpp` checks the cache alone, with a harness
// writing `pos_*` by hand. `tb_terrain_compose.sv` runs PAGESTREAM -> PATCH ->
// COMPCACHE -> TESS on real page bytes -- and writes the placement FROM THE
// BENCH, because when it was written no block produced one.
//
// So the one thing no committed test has ever exercised is the pair: the
// shifter's 66-write burst landing in the store, and coming back out of the
// serve port at the right vertex. That is what `zhao_console_core` now depends
// on, and this bench is its evidence.
//
// ---------------------------------------------------------------------------
// WHY THE READBACK IS THE TEST AND THE BURST IS NOT
// ---------------------------------------------------------------------------
// Watching `pos_we_o` go past proves the shifter emitted 66 values. It does not
// prove any of them is READABLE at the vertex it belongs to, and the failure
// that matters here is an INDEXING one, not an arithmetic one:
//
//   * wx is stored BY COLUMN and wz BY ROW, in two 33-entry planes. Swap the
//     two and every value is still a real placement of a real vertex -- of the
//     wrong vertex. This tree has already shipped that exact transposition once
//     (`zhao_terrain_pagestream`'s vi/vj were reversed for a day, and what it
//     broke was TERRAIN.PATCH's subpatch mask, not any height).
//   * the planes are DOUBLE-BUFFERED by `fill_par_q`/`serve_par_q`, and
//     `pos_we_i` is NOT gated on `fill_active_q`. A placement written into the
//     wrong parity is terrain in the wrong place with every counter agreeing.
//
// Both are only visible from the SERVE side, after a fill has completed and the
// buffer has been handed over. Hence the bench drives a whole 1,089-record fill
// for every patch it places: the cheap version of this test, which writes the
// placement and reads it straight back, would answer out of a buffer nobody had
// swapped and would pass under both faults.
//
// THE HEIGHTS ARE THE BENCH'S OWN and are deliberately trivial. This is not a
// composition check -- `tb_terrain_compose.sv` is -- it is a placement check,
// and the record stream exists to complete a fill. What is asserted about the
// heights is only that the record at (vi, vj) reads back at (vi, vj), which is
// the same indexing question asked of the other plane.
`default_nettype none

module tb_terrain_place_cache (
    input var logic clk,
    input var logic rst_n,

    // ---- TERRAIN.PLACE's patch header ------------------------------------
    input  var logic               hdr_valid,
    output var logic               hdr_ready,
    input  var logic signed [ 7:0] hdr_pitch_log2,
    input  var logic signed [15:0] hdr_patch_ix,
    input  var logic signed [15:0] hdr_patch_iz,
    input  var logic signed [31:0] hdr_env_x0,
    input  var logic signed [31:0] hdr_env_z0,
    input  var logic        [15:0] hdr_src_id,

    // ---- TERRAIN.PLACE's verdict and census ------------------------------
    output var logic        place_valid,
    output var logic [15:0] place_env_mismatch,
    output var logic [15:0] place_pitch_bad,
    output var logic [15:0] place_range,
    output var logic [15:0] place_patches,
    output var logic [15:0] place_src_id,
    output var logic        pos_done,
    // The burst itself, exported so the test can say "NOT ONE WRITE" about a
    // refused patch rather than inferring it from the readback alone.
    output var logic        pos_we,
    output var logic        pos_axis,
    output var logic [ 5:0] pos_idx,

    // ---- TERRAIN.PLACE's per-vertex answer, the other consumer -----------
    // TERRAIN.PATCH reads this combinationally. It is exported so the test can
    // check that the two consumers of one placement AGREE: the value the patch
    // lane is handed for (vi, vj) and the value the cache serves for (vi, vj)
    // are the same number, which is the property the composition rests on.
    input  var logic        [ 5:0] vtx_vi,
    input  var logic        [ 5:0] vtx_vj,
    output var logic signed [31:0] vtx_wx,
    output var logic signed [31:0] vtx_wz,
    output var logic               vtx_placed,

    // ---- TERRAIN.COMPCACHE's fill ----------------------------------------
    input  var logic               fill_start,
    output var logic               fill_accept,
    output var logic               fill_busy,
    output var logic               fill_done,
    input  var logic               st_valid,
    output var logic               st_ready,
    input  var logic signed [31:0] st_top,
    input  var logic signed [31:0] st_bottom,
    input  var logic        [15:0] st_src_id,
    input  var logic               dual,

    // ---- TERRAIN.COMPCACHE's serve ---------------------------------------
    input  var logic               serve_release,
    output var logic               serve_valid,
    output var logic        [15:0] serve_src_id,
    input  var logic               lat_req,
    input  var logic        [ 5:0] lat_vi,
    input  var logic        [ 5:0] lat_vj,
    input  var logic               lat_surface,
    output var logic signed [31:0] lat_h,
    output var logic signed [31:0] lat_wx,
    output var logic signed [31:0] lat_wz,

    output var logic [31:0] fill_records,
    output var logic [31:0] patches_filled,
    output var logic [31:0] patches_served,
    output var logic [31:0] fill_overrun,
    output var logic [31:0] lat_oob
);

  // The 66-write burst, and the ONLY wires between the two blocks. Anything
  // this bench put in here would be the hidden adapter the composition is not
  // allowed to have either.
  logic               pos_we_w, pos_axis_w;
  logic [ 5:0]        pos_idx_w;
  logic signed [31:0] pos_val_w;

  assign pos_we   = pos_we_w;
  assign pos_axis = pos_axis_w;
  assign pos_idx  = pos_idx_w;

  // ---- THE POSITIVE CONTROL, and it lives in the BENCH's wiring ------------
  // The fault this bench exists to catch is a MIS-WIRING between two correct
  // blocks, so the mutation belongs on the wire and not inside either of them.
  // With `ZHAO_PLACE_CACHE_AXIS_SWAP` defined, the burst's axis bit is inverted
  // on its way into the store: every column x lands in the row plane and every
  // row z in the column plane. Both blocks are untouched, every counter in both
  // still reads exactly what it reads without it, and every value the serve
  // port returns is a real placement of a real vertex -- of the wrong one.
  //
  // `terrain_place_cache_axis_swap_control` builds this and the same C++ with
  // `ZHAO_EXPECT_FAIL`, so it passes only when the checks FAIL. It is evidence
  // about the INSTRUMENT: without it, "59 checks, 0 failed" is a claim that the
  // checks can tell the two planes apart, and nothing has tested that claim.
  //
  // A PLAIN `ifdef`, NOT AN `ifndef` AROUND A FUNCTION-LIKE DEFINE, because
  // CLAUDE.md records that a command-line `-D` cannot override the latter and
  // says nothing when it fails to -- two combiner mutants passed while
  // measuring unmutated production. The negative control for this selector is
  // the primary target: with the macro undefined the same C++ passes.
  logic cc_pos_axis;
`ifdef ZHAO_PLACE_CACHE_AXIS_SWAP
  assign cc_pos_axis = ~pos_axis_w;
`else
  assign cc_pos_axis = pos_axis_w;
`endif

  zhao_terrain_place #(
      .LAT_W   (33),
      .LAT_H   (33),
      .CENSUS_W(16)
  ) u_place (
      .clk  (clk),
      .rst_n(rst_n),

      .hdr_valid_i     (hdr_valid),
      .hdr_ready_o     (hdr_ready),
      .hdr_pitch_log2_i(hdr_pitch_log2),
      .hdr_patch_ix_i  (hdr_patch_ix),
      .hdr_patch_iz_i  (hdr_patch_iz),
      .hdr_env_x0_i    (hdr_env_x0),
      .hdr_env_z0_i    (hdr_env_z0),
      .hdr_src_id_i    (hdr_src_id),

      .pos_we_o  (pos_we_w),
      .pos_axis_o(pos_axis_w),
      .pos_idx_o (pos_idx_w),
      .pos_val_o (pos_val_w),
      .pos_done_o(pos_done),

      .vtx_vi_i    (vtx_vi),
      .vtx_vj_i    (vtx_vj),
      .vtx_wx_o    (vtx_wx),
      .vtx_wz_o    (vtx_wz),
      .vtx_placed_o(vtx_placed),

      .place_valid_o       (place_valid),
      .place_env_mismatch_o(place_env_mismatch),
      .place_pitch_bad_o   (place_pitch_bad),
      .place_range_o       (place_range),
      .place_patches_o     (place_patches),
      .place_src_id_o      (place_src_id)
  );

  /* verilator lint_off UNUSEDSIGNAL */
  // Layer D has no writer in this bench and the cache's substance answer is a
  // different question -- entry I32 in the core's header. Named rather than
  // left as an empty connection.
  logic [1:0]  cs_substance_w;
  logic [31:0] cs_oob_w;
  /* verilator lint_on UNUSEDSIGNAL */

  // Layer E is not exercised by this bench -- it is about TERRAIN.PLACE's
  // burst landing in the right buffer parity -- so the port is named and
  // grounded rather than left to a PINMISSING somebody discovers in a fit.
  /* verilator lint_off UNUSEDSIGNAL */
  wire [7:0]  mat_a_w, mat_b_w, mat_weight_w;
  wire        mat_valid_w;
  wire [31:0] mat_oob_w, mat_cells_w;
  /* verilator lint_on UNUSEDSIGNAL */

  zhao_terrain_compcache_front #(
      .LAT_W(33),
      .LAT_H(33)
  ) u_cache (
      .clk  (clk),
      .rst_n(rst_n),

      .fill_start_i (fill_start),
      .fill_accept_o(fill_accept),
      .fill_busy_o  (fill_busy),

      .st_valid_i (st_valid),
      .st_ready_o (st_ready),
      .st_top_i   (st_top),
      .st_bottom_i(st_bottom),
      .st_src_id_i(st_src_id),

      // THE SEAM. Port for port off TERRAIN.PLACE, with nothing between them.
      .pos_we_i  (pos_we_w),
      .pos_axis_i(cc_pos_axis),
      .pos_idx_i (pos_idx_w),
      .pos_val_i (pos_val_w),

      .mat_we_i      (1'b0),
      .mat_w_ci_i    (5'd0),
      .mat_w_cj_i    (5'd0),
      .mat_w_a_i     (8'd0),
      .mat_w_b_i     (8'd0),
      .mat_w_weight_i(8'd0),
      .vel_we_i    (1'b0),
      .vel_w_vi_i  (6'd0),
      .vel_w_vj_i  (6'd0),
      .vel_w_val_i (16'sd0),
      .vel_done_i  (1'b0),

      .cs_we_i         (1'b0),
      .cs_w_ci_i       (5'd0),
      .cs_w_cj_i       (5'd0),
      .cs_w_substance_i(2'd0),

      .dual_i(dual),

      .fill_done_o(fill_done),

      .serve_release_i(serve_release),
      .serve_valid_o  (serve_valid),
      .serve_src_id_o (serve_src_id),

      .lat_req_i    (lat_req),
      .lat_vi_i     (lat_vi),
      .lat_vj_i     (lat_vj),
      .lat_surface_i(lat_surface),
      .lat_h_o      (lat_h),
      .lat_wx_o     (lat_wx),
      .lat_wz_o     (lat_wz),

      .mat_req_i   (1'b0),
      .mat_ci_i    (5'd0),
      .mat_cj_i    (5'd0),
      .mat_a_o     (mat_a_w),
      .mat_b_o     (mat_b_w),
      .mat_weight_o(mat_weight_w),
      .mat_valid_o (mat_valid_w),

      .cs_req_i      (1'b0),
      .cs_ci_i       (5'd0),
      .cs_cj_i       (5'd0),
      .cs_substance_o(cs_substance_w),

      .fill_records_o  (fill_records),
      .patches_filled_o(patches_filled),
      .patches_served_o(patches_served),
      .fill_overrun_o  (fill_overrun),
      .lat_oob_o       (lat_oob),
      .cs_oob_o        (cs_oob_w),
      .mat_oob_o       (mat_oob_w),
      .lat_vel_o          (),
      .lat_vel_present_o  (),
      .vel_words_o        (),
      .vel_oob_o          (),
      .vel_orphan_o       (),
      .vel_done_mismatch_o(),
      .mat_cells_o     (mat_cells_w)
  );

endmodule

`default_nettype wire
