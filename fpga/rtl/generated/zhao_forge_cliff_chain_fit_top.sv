// zhao_forge_cliff_chain_fit_top.sv -- FORGE.CLIFF's composed subsystem, shaped
// for ONE fit, to answer TWO questions that are NAMED HERE BEFORE IT RUNS.
//
// Owner vacation directive 2026-09-23 section 6: "Recheck its real timing in
// the applicable composed context; a past leaf resource saving is not a current
// console saving."
//
// (Despite the directory, this file is HAND WRITTEN, not generated. It lives
// here beside `zhao_shell_fit_top.sv`, `zhao_raster_texture_v3_fit_top.sv` and
// `zhao_terrain_pipe_rpp3_matw18_fit_top.sv` because that is where this tree
// keeps fit wrappers, and it has no `.manifest.json` because nothing generates
// it. It is NOT a production path and instantiates nothing a console does not.)
//
// ===========================================================================
// QUESTION 1 -- TIMING, and it is section 6's question
// ===========================================================================
// Does the adopted evaluator still show a HOLD violation when its vdist read
// master is left as the composed console leaves it?
//
// WHY THAT IS THE QUESTION, measured before it was asked. The two labelled
// rows the campaign compares are `zhao_forge_cliff@golden-for-F-CLIFF1`
// (hold +0.263 ns) and `zhao_forge_cliff_ram@first-measurement` (hold
// -4.140 ns), and the sign change has been read as the one real cost of the
// swap. Three things about that pair, all read off the committed receipts:
//
//   * NO HOLD REPORT EXISTS ON DISK FOR EITHER ROW. `blockpaths/` carries
//     `.setup.margin.rpt` and `.setup.summary.rpt` for both and no hold report
//     at all, so neither hold number has a single path endpoint behind it.
//     A number with no endpoint is a claim, and CLAUDE.md says a claim about
//     an instrument is the one to check hardest.
//
//   * BOTH BLOCKS' WORST SETUP PATH IS THE SAME PATH, and it ENDS ON THE
//     VDIST PORT: `edge_key_r_rtl_0|...~PORT_B_WRITE_ENABLE_REG` ->
//     `vd_addr_o[*]`, at -15.259 ns for the golden and -15.174 ns for the
//     candidate. The candidate is 0.085 ns BETTER on the path both share. It
//     is a property of the design in either shape, not a difference between
//     them -- which is exactly what R142 established for the latch and the
//     four RAM warnings, arriving a third time.
//
//   * THE TWO SUMMARIES ARE NOT LIKE FOR LIKE. Of the candidate's 314
//     reported paths, 228 touch `vd_addr_o` or `vd_data_i` -- 72.6%. Of the
//     golden's 2,000, just 32 do -- 1.6%. The candidate's timing summary is
//     dominated by one port; the golden's is not. Comparing their worst
//     numbers compares two differently-shaped populations, which is CLAUDE.md's
//     "compare like with like, or do not compare" in a fitter's clothes.
//
// AND IN THE COMPOSED CONSOLE THAT PORT CARRIES NOTHING. `CLIFF_VDIST_EN` is
// 1'b0, so `vd_addr_o` has NO LOAD and `vd_data_i` is a constant -- this
// wrapper reproduces both exactly -- and the whole cone should constant-fold
// away. A leaf fit cannot see that, because a leaf fit turns every port into a
// virtual pin with a full I/O budget. THAT is the boundary artefact this
// wrapper removes, and removing it is the only way to find out whether the
// hold violation was ever about the design.
//
// WHAT WOULD FALSIFY THE HYPOTHESIS: a hold violation that survives here, on a
// path whose endpoint is an internal register. That would make the sign change
// real, and it would be worth the follow-up the evaluator's header already
// names -- making the four bare-`assign` RAM reads explicitly synchronous.
//
// ===========================================================================
// QUESTION 2 -- AREA, which nobody has ever measured for this subsystem
// ===========================================================================
// What does the WHOLE of FORGE.CLIFF cost on `5CSEBA6U23I7`?
//
// The campaign's 976 ALM prices the EVALUATOR ALONE. Closing the capability
// took three more blocks -- two sharer instances, the window feed and the
// emission stage -- and their cost is hand-counted and unfitted. On a device
// at 97% of its ALM budget a hand count is an estimate, and this is the
// arithmetic question a map cannot answer and a fit can. The five resource
// categories stay APART: ALUTs, registers, estimated ALMs, placed ALMs and
// block memory bits are five numbers and never a sum.
//
// ===========================================================================
// WHAT IS AT THE BOUNDARY, AND WHY
// ===========================================================================
// The two compose-cache read services and the assembler-facing vertex and
// triple streams. Everything the console keeps internal is internal here too:
// the page command, the 1,156-beat window load and the rim-edge stream never
// reach a pin, which is the whole point -- in the leaf fits they were pins.
//
// Every census counter is XOR-FOLDED into one output bit, which is
// `zhao_prod_top`'s own pattern. Folding keeps the counters from being pruned
// as dead logic (so their area is really measured) without spending ~640
// virtual pins on them and reintroducing the boundary paths this wrapper
// exists to remove.

module zhao_forge_cliff_chain_fit_top #(
    // Fixed at the composed console's value so the fitter constant-folds the
    // vdist cone exactly as it will there. This is the "applicable composed
    // context" in one parameter.
    parameter logic        CLIFF_VDIST_EN       = 1'b0,
    parameter logic [ 1:0] CLIFF_HALO_SUBSTANCE = 2'd0,
    parameter logic        CLIFF_ARM            = 1'b1,
    parameter logic [15:0] CLIFF_MATERIAL_ID    = 16'd0
) (
    input var logic clk,
    input var logic rst_n,

    // ---- the compose cache's residency face ---------------------------------
    input var logic        serve_valid_i,
    input var logic [15:0] serve_src_id_i,

    // ---- the INCUMBENTS on the two shared services --------------------------
    input  var logic        o0_cs_req_i,
    input  var logic [ 9:0] o0_cs_addr_i,
    output var logic [ 1:0] o0_cs_sub_o,
    input  var logic        o0_lat_req_i,
    input  var logic [12:0] o0_lat_addr_i,
    output var logic [95:0] o0_lat_rsp_o,

    // ---- the served ports, out to zhao_terrain_compcache_front -------------
    output var logic        cs_req_o,
    output var logic [ 9:0] cs_addr_o,
    input  var logic [ 1:0] cs_substance_i,
    output var logic        lat_req_o,
    output var logic [12:0] lat_addr_o,
    input  var logic [95:0] lat_rsp_i,

    // ---- the assembler-facing streams, out to zhao_forge_jobarb ------------
    output var logic               v_valid_o,
    input  var logic               v_ready_i,
    output var logic signed [31:0] v_x_o,
    output var logic signed [31:0] v_y_o,
    output var logic signed [31:0] v_z_o,
    output var logic               v_last_o,
    output var logic               t_valid_o,
    input  var logic               t_ready_i,
    output var logic        [15:0] t_i0_o,
    output var logic        [15:0] t_i1_o,
    output var logic        [15:0] t_i2_o,
    output var logic        [15:0] t_material_o,
    output var logic        [15:0] t_src_id_o,
    output var logic               t_last_o,

    // ---- every counter, folded (see the header) -----------------------------
    output var logic census_fold_o
);

  // ---- the two sharers ------------------------------------------------------
  logic       cs_c1_req, cs_c1_grant, cs_c1_rsp_valid;
  logic [4:0] cs_c1_ci, cs_c1_cj;
  logic [1:0] cs_c1_sub;
  logic [31:0] cs_g0, cs_g1, cs_d1, cs_p1;

  zhao_forge_cliff_srvshare #(
      .REQ_W(10), .RSP_W(2), .POISON_EN(1'b1), .CENSUS_W(32)
  ) u_cs_share (
      .clk(clk), .rst_n(rst_n),
      .poison_value_i(2'd3),
      .o0_req_i(o0_cs_req_i),
      .o0_req_payload_i(o0_cs_addr_i),
      .o0_rsp_payload_o(o0_cs_sub_o),
      .c1_req_i(cs_c1_req),
      .c1_req_payload_i({cs_c1_cj, cs_c1_ci}),
      .c1_grant_o(cs_c1_grant),
      .c1_rsp_valid_o(cs_c1_rsp_valid),
      .c1_rsp_payload_o(cs_c1_sub),
      .srv_req_o(cs_req_o),
      .srv_req_payload_o(cs_addr_o),
      .srv_rsp_payload_i(cs_substance_i),
      .grants0_o(cs_g0), .grants1_o(cs_g1), .denied1_o(cs_d1), .poison1_o(cs_p1)
  );

  logic        lat_c1_req, lat_c1_grant, lat_c1_rsp_valid, lat_c1_surface;
  logic [ 5:0] lat_c1_vi, lat_c1_vj;
  logic [95:0] lat_c1_rsp;
  logic [31:0] lat_g0, lat_g1, lat_d1, lat_p1;

  zhao_forge_cliff_srvshare #(
      .REQ_W(13), .RSP_W(96), .POISON_EN(1'b0), .CENSUS_W(32)
  ) u_lat_share (
      .clk(clk), .rst_n(rst_n),
      .poison_value_i(96'd0),
      .o0_req_i(o0_lat_req_i),
      .o0_req_payload_i(o0_lat_addr_i),
      .o0_rsp_payload_o(o0_lat_rsp_o),
      .c1_req_i(lat_c1_req),
      .c1_req_payload_i({lat_c1_surface, lat_c1_vj, lat_c1_vi}),
      .c1_grant_o(lat_c1_grant),
      .c1_rsp_valid_o(lat_c1_rsp_valid),
      .c1_rsp_payload_o(lat_c1_rsp),
      .srv_req_o(lat_req_o),
      .srv_req_payload_o(lat_addr_o),
      .srv_rsp_payload_i(lat_rsp_i),
      .grants0_o(lat_g0), .grants1_o(lat_g1), .denied1_o(lat_d1), .poison1_o(lat_p1)
  );

  // ---- the producer ---------------------------------------------------------
  logic        cmd_valid, cmd_ready, cmd_vdist_en;
  logic [15:0] cmd_page_ci, cmd_page_cj, cmd_lat_w, cmd_src_id;
  logic [ 5:0] cmd_cw, cmd_ch;
  logic        ld_valid, ld_ready, ld_solid;
  logic        feed_busy;
  logic [31:0] f_pages, f_windows, f_reads, f_denied, f_solid, f_degc, f_degp;

  zhao_forge_cliff_feed #(
      .LAT_W(33), .LAT_H(33), .CENSUS_W(32)
  ) u_feed (
      .clk(clk), .rst_n(rst_n),
      .arm_i(CLIFF_ARM),
      .halo_substance_i(CLIFF_HALO_SUBSTANCE),
      .vdist_en_i(CLIFF_VDIST_EN),
      .serve_valid_i(serve_valid_i),
      .serve_src_id_i(serve_src_id_i),
      .cs_req_o(cs_c1_req),
      .cs_ci_o(cs_c1_ci),
      .cs_cj_o(cs_c1_cj),
      .cs_grant_i(cs_c1_grant),
      .cs_rsp_valid_i(cs_c1_rsp_valid),
      .cs_substance_i(cs_c1_sub),
      .cmd_valid_o(cmd_valid),
      .cmd_ready_i(cmd_ready),
      .cmd_page_ci_o(cmd_page_ci),
      .cmd_page_cj_o(cmd_page_cj),
      .cmd_cw_o(cmd_cw),
      .cmd_ch_o(cmd_ch),
      .cmd_lat_w_o(cmd_lat_w),
      .cmd_vdist_en_o(cmd_vdist_en),
      .cmd_src_id_o(cmd_src_id),
      .ld_valid_o(ld_valid),
      .ld_ready_i(ld_ready),
      .ld_solid_o(ld_solid),
      .busy_o(feed_busy),
      .pages_issued_o(f_pages),
      .windows_done_o(f_windows),
      .cs_reads_o(f_reads),
      .cs_denied_o(f_denied),
      .solid_cells_o(f_solid),
      .cells_degraded_o(f_degc),
      .pages_degraded_o(f_degp)
  );

  // ---- the adopted evaluator, with the vdist port AS THE CONSOLE LEAVES IT --
  // `vd_en_o` and `vd_addr_o` have NO LOAD and `vd_data_i` is a constant.
  // QUESTION 1 is entirely about what the fitter does with that.
  logic        edge_valid, edge_ready;
  logic [15:0] edge_ci, edge_cj, edge_src_id;
  logic [ 1:0] edge_side;
  logic [ 5:0] edge_span;
  logic        page_done, cliff_idle;
  logic [11:0] page_merged, page_dropped;
  logic [31:0] tri_submitted;
  logic [ 7:0] walk_fault;

  zhao_forge_cliff_ram u_cliff (
      .clk(clk), .rst_n(rst_n),
      .cmd_valid_i(cmd_valid),
      .cmd_ready_o(cmd_ready),
      .cmd_page_ci_i(cmd_page_ci),
      .cmd_page_cj_i(cmd_page_cj),
      .cmd_cw_i(cmd_cw),
      .cmd_ch_i(cmd_ch),
      .cmd_lat_w_i(cmd_lat_w),
      .cmd_vdist_en_i(cmd_vdist_en),
      .cmd_src_id_i(cmd_src_id),
      .ld_valid_i(ld_valid),
      .ld_ready_o(ld_ready),
      .ld_solid_i(ld_solid),
      .vd_en_o(),
      .vd_addr_o(),
      .vd_data_i(32'd0),
      .edge_valid_o(edge_valid),
      .edge_ready_i(edge_ready),
      .edge_ci_o(edge_ci),
      .edge_cj_o(edge_cj),
      .edge_side_o(edge_side),
      .edge_span_o(edge_span),
      .edge_src_id_o(edge_src_id),
      .page_done_o(page_done),
      .page_merged_o(page_merged),
      .page_dropped_o(page_dropped),
      .idle_o(cliff_idle),
      .triangles_submitted_o(tri_submitted),
      .walk_fault_o(walk_fault)
  );

  // ---- the emission stage ---------------------------------------------------
  logic        emit_busy;
  logic [31:0] e_edges, e_quads, e_tris, e_reads, e_denied, e_clamped;

  zhao_forge_cliff_emit #(
      .LAT_W(33), .LAT_H(33), .CENSUS_W(32)
  ) u_emit (
      .clk(clk), .rst_n(rst_n),
      .edge_valid_i(edge_valid),
      .edge_ready_o(edge_ready),
      .edge_ci_i(edge_ci),
      .edge_cj_i(edge_cj),
      .edge_side_i(edge_side),
      .edge_span_i(edge_span),
      .edge_src_id_i(edge_src_id),
      .lat_req_o(lat_c1_req),
      .lat_vi_o(lat_c1_vi),
      .lat_vj_o(lat_c1_vj),
      .lat_surface_o(lat_c1_surface),
      .lat_grant_i(lat_c1_grant),
      .lat_rsp_valid_i(lat_c1_rsp_valid),
      .lat_h_i($signed(lat_c1_rsp[31:0])),
      .lat_wx_i($signed(lat_c1_rsp[63:32])),
      .lat_wz_i($signed(lat_c1_rsp[95:64])),
      .v_valid_o(v_valid_o),
      .v_ready_i(v_ready_i),
      .v_x_o(v_x_o),
      .v_y_o(v_y_o),
      .v_z_o(v_z_o),
      .v_last_o(v_last_o),
      .t_valid_o(t_valid_o),
      .t_ready_i(t_ready_i),
      .t_i0_o(t_i0_o),
      .t_i1_o(t_i1_o),
      .t_i2_o(t_i2_o),
      .t_material_o(t_material_o),
      .t_src_id_o(t_src_id_o),
      .t_last_o(t_last_o),
      .material_id_i(CLIFF_MATERIAL_ID),
      .busy_o(emit_busy),
      .edges_taken_o(e_edges),
      .quads_emitted_o(e_quads),
      .tris_emitted_o(e_tris),
      .lat_reads_o(e_reads),
      .lat_denied_o(e_denied),
      .endpoint_clamped_o(e_clamped)
  );

  // ---- the fold: keeps every counter alive for one pin ----------------------
  logic fold_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      fold_q <= 1'b0;
    end else begin
      fold_q <= (^cs_g0) ^ (^cs_g1) ^ (^cs_d1) ^ (^cs_p1) ^
                (^lat_g0) ^ (^lat_g1) ^ (^lat_d1) ^ (^lat_p1) ^
                (^f_pages) ^ (^f_windows) ^ (^f_reads) ^ (^f_denied) ^
                (^f_solid) ^ (^f_degc) ^ (^f_degp) ^ feed_busy ^
                (^tri_submitted) ^ (^walk_fault) ^ (^page_merged) ^
                (^page_dropped) ^ page_done ^ cliff_idle ^
                (^e_edges) ^ (^e_quads) ^ (^e_tris) ^ (^e_reads) ^
                (^e_denied) ^ (^e_clamped) ^ emit_busy;
    end
  end

  assign census_fold_o = fold_q;

endmodule
