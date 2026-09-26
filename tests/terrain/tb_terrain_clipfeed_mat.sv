// tb_terrain_clipfeed_mat.sv -- `zhao_terrain_clipfeed` with its ports
// flattened, so `tests/terrain/terrain_clipfeed_mat_directed.cpp` names
// fields. Testbench only.
//
// WHY THIS BENCH IS SCOPED TO THE MATERIAL IDENTITY, said plainly so the
// scope is not mistaken for the whole debt. Entry I13 records that
// `zhao_terrain_clipfeed` has NO directed test at all and that the debt is
// owed. This bench does NOT pay all of it: it says nothing about the
// perspective multiply, the palette ladder, the depth converter's tag
// discipline or the three-way join's `src_id` guard, all of which are that
// block's other subjects. It pays the part TERRAINMAT created -- the material
// identity's derivation and its fault -- because a packet that adds a counter
// owes a place where that counter is SEEN TO FIRE, and because a guard
// reachable with legal stimulus must be fired by stimulus rather than by a
// committed mutant.
//
// The triangle stimulus is therefore the SIMPLEST ONE THAT COMPLETES: a
// well-formed, non-degenerate triangle with legal `w`, a mid-range shade and
// unity tints, repeated. It exists to carry a beat through the block, not to
// be interesting.
`default_nettype none

module tb_terrain_clipfeed_mat (
    input  var logic clk,
    input  var logic rst_n,

    // ---- the three-way join, offered together ------------------------------
    input  var logic        t_valid,
    output var logic        t_ready,
    input  var logic [15:0] t_src,
    input  var logic [30:0] t_w,

    input  var logic        l_valid,
    output var logic        l_ready,
    input  var logic [31:0] l_shade,
    input  var logic        l_degenerate,

    input  var logic        u_valid,
    output var logic        u_ready,

    // ---- THE FIELDS UNDER TEST --------------------------------------------
    input  var logic [31:0] mat_set,
    input  var logic [15:0] mat_id,

    // ---- the door ----------------------------------------------------------
    output var logic        o_valid,
    input  var logic        o_ready,
    output var logic [31:0] o_material_set,
    output var logic [15:0] o_material_id,
    output var logic [ 1:0] o_material_mode,
    output var logic [15:0] o_src_id,
    output var logic        o_untex,

    // ---- the census and the fault -----------------------------------------
    output var logic [31:0] triangles,
    output var logic [31:0] emitted,
    output var logic [31:0] src_id_mismatch,
    output var logic [31:0] mat_backed,
    output var logic [31:0] mat_id_orphan
);

  // A triangle with REAL AREA and three DISTINCT corners. A degenerate one
  // would still traverse the block, but a bench whose stimulus is degenerate
  // cannot later be extended to say anything about the attribute packet, and
  // the corners cost nothing.
  localparam logic signed [20:0] AX = 21'sd1000, AY = 21'sd1000;
  localparam logic signed [20:0] BX = 21'sd2000, BY = 21'sd1200;
  localparam logic signed [20:0] CX = 21'sd1400, CY = 21'sd2200;

  // Q16.16 tile units, well inside the S8.24 rail so `uv_sat_o` stays silent
  // and this bench is not quietly measuring the saturate as well.
  localparam logic signed [31:0] AU = 32'sd65536,  AV = 32'sd0;
  localparam logic signed [31:0] BU = 32'sd131072, BV = 32'sd65536;
  localparam logic signed [31:0] CU = 32'sd65536,  CV = 32'sd131072;

  // Layer H's ratified ABSENT IDENTITY, the same value the composer drives.
  localparam logic [16:0] TINT_ID  = 17'd65536;
  localparam logic [16:0] SHEET_ID = 17'd65536;

  zhao_terrain_clipfeed #(
      .ATTRS      (7),
      .IDW        (16),
      .SLOT_INVW  (0),
      .SLOT_UOW   (1),
      .SLOT_VOW   (2),
      .SLOT_R     (3),
      .SLOT_G     (4),
      .SLOT_B     (5),
      .SLOT_ALPHA (6)
  ) u_dut (
      .clk   (clk),
      .rst_n (rst_n),

      .t_valid_i  (t_valid),
      .t_ready_o  (t_ready),
      .t_ax_i     (AX),
      .t_ay_i     (AY),
      .t_bx_i     (BX),
      .t_by_i     (BY),
      .t_cx_i     (CX),
      .t_cy_i     (CY),
      .t_behind_i (3'd0),
      .t_src_id_i (t_src),
      .t_aw_i     (t_w),
      .t_bw_i     (t_w),
      .t_cw_i     (t_w),
      .t_profile_i(2'd0),

      .l_valid_i     (l_valid),
      .l_ready_o     (l_ready),
      .l_shade_i     (l_shade),
      .l_degenerate_i(l_degenerate),
      .l_src_id_i    (t_src),

      .u_valid_i (u_valid),
      .u_ready_o (u_ready),
      .u_au_i    (AU),
      .u_av_i    (AV),
      .u_bu_i    (BU),
      .u_bv_i    (BV),
      .u_cu_i    (CU),
      .u_cv_i    (CV),
      .u_src_id_i(t_src),

      .a_tint_r_i(TINT_ID), .a_tint_g_i(TINT_ID), .a_tint_b_i(TINT_ID),
      .b_tint_r_i(TINT_ID), .b_tint_g_i(TINT_ID), .b_tint_b_i(TINT_ID),
      .c_tint_r_i(TINT_ID), .c_tint_g_i(TINT_ID), .c_tint_b_i(TINT_ID),
      .sheet_i   (SHEET_ID),

      .mat_set_i (mat_set),
      .mat_id_i  (mat_id),

      .o_valid_o        (o_valid),
      .o_ready_i        (o_ready),
      /* verilator lint_off PINCONNECTEMPTY */
      .o_ax_o           (),
      .o_ay_o           (),
      .o_bx_o           (),
      .o_by_o           (),
      .o_cx_o           (),
      .o_cy_o           (),
      .o_behind_o       (),
      .o_attr_a_o       (),
      .o_attr_b_o       (),
      .o_attr_c_o       (),
      .o_vertex_alpha_o (),
      .o_frag_state_o   (),
      .o_quality_tier_o (),
      .o_cull_mode_o    (),
      .uv_sat_o         (),
      .shade_clamped_o  (),
      .degenerate_o     (),
      .dq_refused_o     (),
      .dq_stray_o       (),
      /* verilator lint_on PINCONNECTEMPTY */
      .o_src_id_o       (o_src_id),
      .o_untex_o        (o_untex),
      .o_material_set_o (o_material_set),
      .o_material_id_o  (o_material_id),
      .o_material_mode_o(o_material_mode),

      .triangles_o      (triangles),
      .emitted_o        (emitted),
      .src_id_mismatch_o(src_id_mismatch),
      .mat_backed_o     (mat_backed),
      .mat_id_orphan_o  (mat_id_orphan)
  );

endmodule : tb_terrain_clipfeed_mat

`default_nettype wire
