// tb_compcache_front.sv -- TERRAIN.COMPCACHE's patch front, ports flattened for
// the C++ side, instantiated TWICE.
//
// `u_dut` is the PRODUCTION geometry (33 x 33 vertices, 32 x 32 cells) and is
// what every exhaustive comparison in the differential runs against: the block
// is a store, so the oracle is its contents and addressing, and a store checked
// at a shrunken size is a different store.
//
// `u_small` is the same module at LAT_W = LAT_H = 9. It exists for ONE property
// that the production geometry cannot express: `cs_ci_i`/`cs_cj_i` are 5-bit
// ports and the production cell plane is 32 x 32, so EVERY value a consumer can
// put on those ports is in range and `cs_oob_o` is structurally unreachable at
// 33 x 33. A counter that cannot be made to count is a detector that has never
// fired, so the small instance -- whose 8 x 8 cell plane leaves ci = 8..31 out
// of range -- is where that alarm is proved to work. It also exercises the
// dual_i = 0 legacy page (bottom == top) and, incidentally, proves the LAT_W /
// LAT_H parameters the module header offers a test are real.
//
// No behaviour is modelled here. Unlike tb_island_dir, which models a residency
// store because that test is about an outcome MAPPING, this block IS the store
// under test; anything modelled in the bench would be the thing being checked.
module tb_compcache_front (
    input logic clk,
    input logic rst_n,

    // ---- production instance: fill -----------------------------------------
    input  logic        fill_start,
    output logic        fill_accept,
    output logic        fill_busy,
    input  logic        st_valid,
    output logic        st_ready,
    input  logic [31:0] st_top,
    input  logic [31:0] st_bottom,
    input  logic [15:0] st_src_id,
    input  logic        pos_we,
    input  logic        pos_axis,
    input  logic [ 5:0] pos_idx,
    input  logic [31:0] pos_val,
    input  logic        cs_we,
    input  logic [ 4:0] cs_w_ci,
    input  logic [ 4:0] cs_w_cj,
    input  logic [ 1:0] cs_w_substance,
    input  logic        dual,

    // ---- production instance: swap -----------------------------------------
    output logic fill_done,
    input  logic serve_release,
    output logic serve_valid,

    // ---- production instance: serve ----------------------------------------
    input  logic        lat_req,
    input  logic [ 5:0] lat_vi,
    input  logic [ 5:0] lat_vj,
    input  logic        lat_surface,
    output logic [31:0] lat_h,
    output logic [31:0] lat_wx,
    output logic [31:0] lat_wz,
    input  logic        cs_req,
    input  logic [ 4:0] cs_ci,
    input  logic [ 4:0] cs_cj,
    output logic [ 1:0] cs_substance,

    // ---- production instance: counters -------------------------------------
    output logic [31:0] fill_records,
    output logic [31:0] patches_filled,
    output logic [31:0] patches_served,
    output logic [31:0] fill_overrun,
    output logic [31:0] lat_oob,
    output logic [31:0] cs_oob,

    // ---- the 9 x 9 instance ------------------------------------------------
    input  logic        s_fill_start,
    input  logic        s_st_valid,
    output logic        s_st_ready,
    input  logic [31:0] s_st_top,
    input  logic [31:0] s_st_bottom,
    input  logic        s_dual,
    input  logic        s_cs_we,
    input  logic [ 4:0] s_cs_w_ci,
    input  logic [ 4:0] s_cs_w_cj,
    input  logic [ 1:0] s_cs_w_substance,
    input  logic        s_serve_release,
    output logic        s_serve_valid,
    input  logic        s_lat_req,
    input  logic [ 5:0] s_lat_vi,
    input  logic [ 5:0] s_lat_vj,
    input  logic        s_lat_surface,
    output logic [31:0] s_lat_h,
    input  logic        s_cs_req,
    input  logic [ 4:0] s_cs_ci,
    input  logic [ 4:0] s_cs_cj,
    output logic [ 1:0] s_cs_substance,
    output logic [31:0] s_fill_records,
    output logic [31:0] s_lat_oob,
    output logic [31:0] s_cs_oob
);

  zhao_terrain_compcache_front #(
      .LAT_W(33),
      .LAT_H(33)
  ) u_dut (
      .clk(clk),
      .rst_n(rst_n),

      .fill_start_i (fill_start),
      .fill_accept_o(fill_accept),
      .fill_busy_o  (fill_busy),

      .st_valid_i (st_valid),
      .st_ready_o (st_ready),
      .st_top_i   ($signed(st_top)),
      .st_bottom_i($signed(st_bottom)),
      .st_src_id_i(st_src_id),

      .pos_we_i  (pos_we),
      .pos_axis_i(pos_axis),
      .pos_idx_i (pos_idx),
      .pos_val_i ($signed(pos_val)),

      .cs_we_i         (cs_we),
      .cs_w_ci_i       (cs_w_ci),
      .cs_w_cj_i       (cs_w_cj),
      .cs_w_substance_i(cs_w_substance),

      .dual_i(dual),

      .fill_done_o    (fill_done),
      .serve_release_i(serve_release),
      .serve_valid_o  (serve_valid),

      .lat_req_i    (lat_req),
      .lat_vi_i     (lat_vi),
      .lat_vj_i     (lat_vj),
      .lat_surface_i(lat_surface),
      .lat_h_o      (lat_h),
      .lat_wx_o     (lat_wx),
      .lat_wz_o     (lat_wz),

      .cs_req_i      (cs_req),
      .cs_ci_i       (cs_ci),
      .cs_cj_i       (cs_cj),
      .cs_substance_o(cs_substance),

      .fill_records_o  (fill_records),
      .patches_filled_o(patches_filled),
      .patches_served_o(patches_served),
      .fill_overrun_o  (fill_overrun),
      .lat_oob_o       (lat_oob),
      .cs_oob_o        (cs_oob)
  );

  // The 9 x 9 instance. Ports the C++ side does not need land on named dangling
  // nets rather than being flattened out, so the bench surface stays the ports
  // the test actually drives. Named, not `()`: an empty pin connection is a
  // PINCONNECTEMPTY under -Wall, and a bench that cannot be linted is the same
  // hole as RTL that cannot be linted.
  /* verilator lint_off UNUSEDSIGNAL */
  logic        s_nc_fill_accept, s_nc_fill_busy, s_nc_fill_done;
  logic [31:0] s_nc_lat_wx, s_nc_lat_wz;
  logic [31:0] s_nc_patches_filled, s_nc_patches_served, s_nc_fill_overrun;
  /* verilator lint_on UNUSEDSIGNAL */
  zhao_terrain_compcache_front #(
      .LAT_W(9),
      .LAT_H(9)
  ) u_small (
      .clk(clk),
      .rst_n(rst_n),

      .fill_start_i (s_fill_start),
      .fill_accept_o(s_nc_fill_accept),
      .fill_busy_o  (s_nc_fill_busy),

      .st_valid_i (s_st_valid),
      .st_ready_o (s_st_ready),
      .st_top_i   ($signed(s_st_top)),
      .st_bottom_i($signed(s_st_bottom)),
      .st_src_id_i(16'd0),

      .pos_we_i  (1'b0),
      .pos_axis_i(1'b0),
      .pos_idx_i (6'd0),
      .pos_val_i (32'sd0),

      .cs_we_i         (s_cs_we),
      .cs_w_ci_i       (s_cs_w_ci),
      .cs_w_cj_i       (s_cs_w_cj),
      .cs_w_substance_i(s_cs_w_substance),

      .dual_i(s_dual),

      .fill_done_o    (s_nc_fill_done),
      .serve_release_i(s_serve_release),
      .serve_valid_o  (s_serve_valid),

      .lat_req_i    (s_lat_req),
      .lat_vi_i     (s_lat_vi),
      .lat_vj_i     (s_lat_vj),
      .lat_surface_i(s_lat_surface),
      .lat_h_o      (s_lat_h),
      .lat_wx_o     (s_nc_lat_wx),
      .lat_wz_o     (s_nc_lat_wz),

      .cs_req_i      (s_cs_req),
      .cs_ci_i       (s_cs_ci),
      .cs_cj_i       (s_cs_cj),
      .cs_substance_o(s_cs_substance),

      .fill_records_o  (s_fill_records),
      .patches_filled_o(s_nc_patches_filled),
      .patches_served_o(s_nc_patches_served),
      .fill_overrun_o  (s_nc_fill_overrun),
      .lat_oob_o       (s_lat_oob),
      .cs_oob_o        (s_cs_oob)
  );

endmodule
