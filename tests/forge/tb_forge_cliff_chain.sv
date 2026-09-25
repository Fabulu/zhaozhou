// tb_forge_cliff_chain.sv -- the WHOLE FORGE.CLIFF producer-to-consumer chain in
// one elaboration, so "a cliff evaluated from real terrain reaching a consumer"
// is a thing that can be measured rather than a thing that is argued.
//
//   terrain planes (this file)                 <- a real zref ComposedLattice,
//     |                                           loaded by the C++ driver
//     +-- cs service  --> zhao_forge_cliff_srvshare --> zhao_forge_cliff_feed
//     |                                                    |  cmd + 34x34 window
//     |                                                    v
//     |                                            zhao_forge_cliff_ram
//     |                                                    |  rim edges
//     |                                                    v
//     +-- lat service --> zhao_forge_cliff_srvshare --> zhao_forge_cliff_emit
//                                                          |  vertices + triples
//                                                          v
//                                                   the driver, acting as
//                                                   zhao_forge_assemble
//
// THE TERRAIN PLANES ARE MODELLED ON THE REAL ONES, PORT-TIMING FIRST. Both
// services reproduce `zhao_terrain_compcache_front`'s actual behaviour rather
// than a convenient one:
//   * ONE CLOCK, registered, no handshake, no tag (`:484`, `:542`, `:562`);
//   * the cell-state answer is gated by `serve_valid` and returns the block's
//     own refusal encoding `2'd3` when it is low (`:562`), which is what makes
//     the sharer's `poison1_o` reachable from this bench;
//   * the incumbent may ask on ANY cycle, which is what makes `cs_denied_o` and
//     `lat_denied_o` reachable.
// A bench that answered instantly, or always, would be a bench that cannot see
// the two faults this chain was designed against.

module tb_forge_cliff_chain (
    input var logic clk,
    input var logic rst_n,

    // ---- loading the terrain (driver side, not a console path) --------------
    input var logic               fill_lat_we_i,
    input var logic        [10:0] fill_lat_idx_i,   // vj*33 + vi, 0..1088
    input var logic signed [31:0] fill_lat_top_i,
    input var logic signed [31:0] fill_lat_bot_i,
    input var logic signed [31:0] fill_lat_wx_i,
    input var logic signed [31:0] fill_lat_wz_i,
    input var logic               fill_cs_we_i,
    input var logic        [ 9:0] fill_cs_idx_i,    // cj*32 + ci, 0..1023
    input var logic        [ 1:0] fill_cs_sub_i,

    // ---- the compose cache's residency face ---------------------------------
    input var logic        serve_valid_i,
    input var logic [15:0] serve_src_id_i,

    // ---- the owner's switches ------------------------------------------------
    input var logic        arm_i,
    input var logic [ 1:0] halo_substance_i,
    input var logic        vdist_en_i,
    input var logic [15:0] material_id_i,

    // ---- the INCUMBENT, so contention is real and not assumed ---------------
    input var logic o0_cs_req_i,
    input var logic o0_lat_req_i,

    // ---- the consumer face (the driver plays zhao_forge_assemble) -----------
    input  var logic               v_ready_i,
    output var logic               v_valid_o,
    output var logic signed [31:0] v_x_o,
    output var logic signed [31:0] v_y_o,
    output var logic signed [31:0] v_z_o,
    output var logic               v_last_o,
    input  var logic               t_ready_i,
    output var logic               t_valid_o,
    output var logic        [15:0] t_i0_o,
    output var logic        [15:0] t_i1_o,
    output var logic        [15:0] t_i2_o,
    output var logic        [15:0] t_material_o,
    output var logic        [15:0] t_src_id_o,
    output var logic               t_last_o,

    // ---- evidence -------------------------------------------------------------
    output var logic [31:0] feed_pages_issued_o,
    output var logic [31:0] feed_windows_done_o,
    output var logic [31:0] feed_cs_reads_o,
    output var logic [31:0] feed_cs_denied_o,
    output var logic [31:0] feed_solid_cells_o,
    output var logic [31:0] feed_cells_degraded_o,
    output var logic [31:0] feed_pages_degraded_o,
    output var logic        feed_busy_o,

    output var logic [31:0] cs_grants0_o,
    output var logic [31:0] cs_grants1_o,
    output var logic [31:0] cs_denied1_o,
    output var logic [31:0] cs_poison1_o,
    output var logic [31:0] lat_grants1_o,
    output var logic [31:0] lat_denied1_o,

    output var logic [31:0] emit_edges_taken_o,
    output var logic [31:0] emit_quads_o,
    output var logic [31:0] emit_tris_o,
    output var logic [31:0] emit_lat_reads_o,
    // The emit block's own denial count and the sharer's `lat_denied1_o` are
    // the SAME event seen from the two ends. Both are published so the driver
    // can compare them: one number twice would be no evidence at all.
    output var logic [31:0] emit_lat_denied_o,
    output var logic [31:0] emit_clamped_o,
    output var logic        emit_busy_o,

    output var logic [31:0] cliff_tri_submitted_o,
    output var logic [ 7:0] cliff_walk_fault_o,
    output var logic [11:0] cliff_page_merged_o,
    output var logic [11:0] cliff_page_dropped_o,
    output var logic        cliff_page_done_o,
    output var logic        cliff_idle_o,

    // REAL EVIDENCE, not a lint sink: with `vdist_en_i` low the evaluator must
    // never assert its vdist read enable. The driver asserts this stays 0 for
    // the whole run, which is what turns "the block never reads the port we
    // have no producer for" from a claim into a measurement.
    output var logic        cliff_vd_en_o,
    output var logic [31:0] lat_grants0_o,
    output var logic [31:0] lat_poison_o,
    // The incumbent's returned payloads, folded. The incumbent is driven here
    // only to CONTEND, so its answers are not examined -- but they must be
    // observable or the sharer's pass-through would be untested dead code.
    output var logic        o0_fold_o
);

  localparam int unsigned LatW  = 33;
  localparam int unsigned Verts = LatW * LatW;   // 1089
  localparam int unsigned Cells = 32 * 32;       // 1024

  // Named sinks for outputs this bench does not examine. An empty pin
  // connection trips PINCONNECTEMPTY under the bare `-Wall` the lint gates use.
  logic [ 1:0] cs_o0_rsp_c;
  logic [95:0] lat_o0_rsp_c;
  logic [31:0] vd_addr_c;
  assign o0_fold_o = (^cs_o0_rsp_c) ^ (^lat_o0_rsp_c) ^ (^vd_addr_c);

  // ---- the terrain planes ---------------------------------------------------
  logic signed [31:0] lat_top_m [Verts];
  logic signed [31:0] lat_bot_m [Verts];
  logic signed [31:0] lat_wx_m  [Verts];
  logic signed [31:0] lat_wz_m  [Verts];
  logic        [ 1:0] sub_m     [Cells];

  always_ff @(posedge clk) begin
    if (fill_lat_we_i) begin
      lat_top_m[fill_lat_idx_i] <= fill_lat_top_i;
      lat_bot_m[fill_lat_idx_i] <= fill_lat_bot_i;
      lat_wx_m[fill_lat_idx_i]  <= fill_lat_wx_i;
      lat_wz_m[fill_lat_idx_i]  <= fill_lat_wz_i;
    end
    if (fill_cs_we_i) begin
      sub_m[fill_cs_idx_i] <= fill_cs_sub_i;
    end
  end

  // ---- the cell-state service (zhao_terrain_compcache_front's shape) --------
  logic       cs_srv_req_c;
  logic [9:0] cs_srv_payload_c;   // {cj, ci}
  logic [1:0] cs_srv_rsp_c;

  logic [1:0] cs_sub_q;
  logic       cs_ok_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      cs_sub_q <= 2'd0;
      cs_ok_q  <= 1'b0;
    end else begin
      cs_sub_q <= sub_m[{cs_srv_payload_c[9:5], cs_srv_payload_c[4:0]}];
      cs_ok_q  <= cs_srv_req_c && serve_valid_i;
    end
  end

  // `2'd3` is the serve block's own refusal encoding, returned when no patch is
  // being served. This is the one thing the sharer's poison detector watches.
  assign cs_srv_rsp_c = cs_ok_q ? cs_sub_q : 2'd3;

  // ---- the lattice service --------------------------------------------------
  logic        lat_srv_req_c;
  logic [12:0] lat_srv_payload_c;  // {surface, vj[5:0], vi[5:0]}
  logic [95:0] lat_srv_rsp_c;      // {wz, wx, h}

  logic signed [31:0] lat_h_q, lat_wx_q, lat_wz_q;
  logic [10:0] lat_addr_c;
  assign lat_addr_c = 11'(lat_srv_payload_c[11:6]) * 11'(LatW) + 11'(lat_srv_payload_c[5:0]);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      lat_h_q  <= '0;
      lat_wx_q <= '0;
      lat_wz_q <= '0;
    end else if (lat_srv_req_c) begin
      // A read ENABLE, as an M10K has: the output register holds when nobody
      // asks. Modelled rather than left free-running, because a free-running
      // read would answer a question that was never posed and would hide a
      // client that reads without asking.
      lat_h_q  <= lat_srv_payload_c[12] ? lat_bot_m[lat_addr_c] : lat_top_m[lat_addr_c];
      lat_wx_q <= lat_wx_m[lat_addr_c];
      lat_wz_q <= lat_wz_m[lat_addr_c];
    end
  end

  assign lat_srv_rsp_c = {lat_wz_q, lat_wx_q, lat_h_q};

  // ---- the two sharers ------------------------------------------------------
  logic       fd_cs_req_c;
  logic [4:0] fd_cs_ci_c, fd_cs_cj_c;
  logic       fd_cs_grant_c, fd_cs_rsp_valid_c;
  logic [1:0] fd_cs_sub_c;

  zhao_forge_cliff_srvshare #(
      .REQ_W(10), .RSP_W(2), .POISON_EN(1'b1), .CENSUS_W(32)
  ) u_cs_share (
      .clk(clk), .rst_n(rst_n),
      .poison_value_i(2'd3),
      .o0_req_i(o0_cs_req_i),
      .o0_req_payload_i(10'd0),
      .o0_rsp_payload_o(cs_o0_rsp_c),
      .c1_req_i(fd_cs_req_c),
      .c1_req_payload_i({fd_cs_cj_c, fd_cs_ci_c}),
      .c1_grant_o(fd_cs_grant_c),
      .c1_rsp_valid_o(fd_cs_rsp_valid_c),
      .c1_rsp_payload_o(fd_cs_sub_c),
      .srv_req_o(cs_srv_req_c),
      .srv_req_payload_o(cs_srv_payload_c),
      .srv_rsp_payload_i(cs_srv_rsp_c),
      .grants0_o(cs_grants0_o),
      .grants1_o(cs_grants1_o),
      .denied1_o(cs_denied1_o),
      .poison1_o(cs_poison1_o)
  );

  logic        em_lat_req_c, em_lat_surface_c;
  logic [ 5:0] em_lat_vi_c, em_lat_vj_c;
  logic        em_lat_grant_c, em_lat_rsp_valid_c;
  logic [95:0] em_lat_rsp_c;

  zhao_forge_cliff_srvshare #(
      .REQ_W(13), .RSP_W(96), .POISON_EN(1'b0), .CENSUS_W(32)
  ) u_lat_share (
      .clk(clk), .rst_n(rst_n),
      .poison_value_i(96'd0),
      .o0_req_i(o0_lat_req_i),
      .o0_req_payload_i(13'd0),
      .o0_rsp_payload_o(lat_o0_rsp_c),
      .c1_req_i(em_lat_req_c),
      .c1_req_payload_i({em_lat_surface_c, em_lat_vj_c, em_lat_vi_c}),
      .c1_grant_o(em_lat_grant_c),
      .c1_rsp_valid_o(em_lat_rsp_valid_c),
      .c1_rsp_payload_o(em_lat_rsp_c),
      .srv_req_o(lat_srv_req_c),
      .srv_req_payload_o(lat_srv_payload_c),
      .srv_rsp_payload_i(lat_srv_rsp_c),
      .grants0_o(lat_grants0_o),
      .grants1_o(lat_grants1_o),
      .denied1_o(lat_denied1_o),
      .poison1_o(lat_poison_o)
  );

  // ---- the producer ---------------------------------------------------------
  logic        cmd_valid_c, cmd_ready_c, cmd_vdist_en_c;
  logic [15:0] cmd_page_ci_c, cmd_page_cj_c, cmd_lat_w_c, cmd_src_id_c;
  logic [ 5:0] cmd_cw_c, cmd_ch_c;
  logic        ld_valid_c, ld_ready_c, ld_solid_c;

  zhao_forge_cliff_feed #(
      .LAT_W(33), .LAT_H(33), .CENSUS_W(32)
  ) u_feed (
      .clk(clk), .rst_n(rst_n),
      .arm_i(arm_i),
      .halo_substance_i(halo_substance_i),
      .vdist_en_i(vdist_en_i),
      .serve_valid_i(serve_valid_i),
      .serve_src_id_i(serve_src_id_i),
      .cs_req_o(fd_cs_req_c),
      .cs_ci_o(fd_cs_ci_c),
      .cs_cj_o(fd_cs_cj_c),
      .cs_grant_i(fd_cs_grant_c),
      .cs_rsp_valid_i(fd_cs_rsp_valid_c),
      .cs_substance_i(fd_cs_sub_c),
      .cmd_valid_o(cmd_valid_c),
      .cmd_ready_i(cmd_ready_c),
      .cmd_page_ci_o(cmd_page_ci_c),
      .cmd_page_cj_o(cmd_page_cj_c),
      .cmd_cw_o(cmd_cw_c),
      .cmd_ch_o(cmd_ch_c),
      .cmd_lat_w_o(cmd_lat_w_c),
      .cmd_vdist_en_o(cmd_vdist_en_c),
      .cmd_src_id_o(cmd_src_id_c),
      .ld_valid_o(ld_valid_c),
      .ld_ready_i(ld_ready_c),
      .ld_solid_o(ld_solid_c),
      .busy_o(feed_busy_o),
      .pages_issued_o(feed_pages_issued_o),
      .windows_done_o(feed_windows_done_o),
      .cs_reads_o(feed_cs_reads_o),
      .cs_denied_o(feed_cs_denied_o),
      .solid_cells_o(feed_solid_cells_o),
      .cells_degraded_o(feed_cells_degraded_o),
      .pages_degraded_o(feed_pages_degraded_o)
  );

  // ---- the adopted evaluator, UNCHANGED -------------------------------------
  logic        ed_valid_c, ed_ready_c;
  logic [15:0] ed_ci_c, ed_cj_c, ed_src_id_c;
  logic [ 1:0] ed_side_c;
  logic [ 5:0] ed_span_c;

  zhao_forge_cliff_ram u_cliff (
      .clk(clk), .rst_n(rst_n),
      .cmd_valid_i(cmd_valid_c),
      .cmd_ready_o(cmd_ready_c),
      .cmd_page_ci_i(cmd_page_ci_c),
      .cmd_page_cj_i(cmd_page_cj_c),
      .cmd_cw_i(cmd_cw_c),
      .cmd_ch_i(cmd_ch_c),
      .cmd_lat_w_i(cmd_lat_w_c),
      .cmd_vdist_en_i(cmd_vdist_en_c),
      .cmd_src_id_i(cmd_src_id_c),
      .ld_valid_i(ld_valid_c),
      .ld_ready_o(ld_ready_c),
      .ld_solid_i(ld_solid_c),
      // vdist is OFF in this chain (the console's decision record says why), so
      // the block never asserts vd_en_o and never samples this. It is zero
      // because an unread input has to be driven, not because a producer is
      // missing behind it.
      .vd_en_o(cliff_vd_en_o),
      .vd_addr_o(vd_addr_c),
      .vd_data_i(32'd0),
      .edge_valid_o(ed_valid_c),
      .edge_ready_i(ed_ready_c),
      .edge_ci_o(ed_ci_c),
      .edge_cj_o(ed_cj_c),
      .edge_side_o(ed_side_c),
      .edge_span_o(ed_span_c),
      .edge_src_id_o(ed_src_id_c),
      .page_done_o(cliff_page_done_o),
      .page_merged_o(cliff_page_merged_o),
      .page_dropped_o(cliff_page_dropped_o),
      .idle_o(cliff_idle_o),
      .triangles_submitted_o(cliff_tri_submitted_o),
      .walk_fault_o(cliff_walk_fault_o)
  );

  // ---- the emission stage ---------------------------------------------------
  zhao_forge_cliff_emit #(
      .LAT_W(33), .LAT_H(33), .CENSUS_W(32)
  ) u_emit (
      .clk(clk), .rst_n(rst_n),
      .edge_valid_i(ed_valid_c),
      .edge_ready_o(ed_ready_c),
      .edge_ci_i(ed_ci_c),
      .edge_cj_i(ed_cj_c),
      .edge_side_i(ed_side_c),
      .edge_span_i(ed_span_c),
      .edge_src_id_i(ed_src_id_c),
      .lat_req_o(em_lat_req_c),
      .lat_vi_o(em_lat_vi_c),
      .lat_vj_o(em_lat_vj_c),
      .lat_surface_o(em_lat_surface_c),
      .lat_grant_i(em_lat_grant_c),
      .lat_rsp_valid_i(em_lat_rsp_valid_c),
      .lat_h_i($signed(em_lat_rsp_c[31:0])),
      .lat_wx_i($signed(em_lat_rsp_c[63:32])),
      .lat_wz_i($signed(em_lat_rsp_c[95:64])),
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
      .material_id_i(material_id_i),
      .busy_o(emit_busy_o),
      .edges_taken_o(emit_edges_taken_o),
      .quads_emitted_o(emit_quads_o),
      .tris_emitted_o(emit_tris_o),
      .lat_reads_o(emit_lat_reads_o),
      .lat_denied_o(emit_lat_denied_o),
      .endpoint_clamped_o(emit_clamped_o)
  );

endmodule
