// tb_terrain_fieldmajor.sv — THE COMPOSED FIELD-MAJOR EARTH MACHINE, walker
// and accumulator in one elaboration, with the engine seam exposed.
//
// WHY THIS FILE EXISTS
// --------------------
// `reports/DECISION-20260926-I34-PATCH-V2-CHANNELS.md` measured the composed
// VERTEX-MAJOR Earth path as a LINE -- `clocks(L) = 4,431 + 1,089*L` -- and
// then priced the field-major alternative with ARITHMETIC, flagging every
// step after its two measured constants as "not built or benched". This bench
// benches it.
//
// `zhao_terrain_field_walk` and `zhao_terrain_patch_acc` are BUILT, each with
// its own differential test, and NEITHER has ever been elaborated beside the
// other. Each test drives its block from C++ with the other block's job done
// in C++ too, so the composed machine's cost has never been a measurement.
// That is this file's whole subject: the two blocks, one elaboration, the
// walker's group stream reaching the accumulator's update port.
//
// WHERE THE ENGINE IS, AND WHY IT IS MODELLED HERE EXACTLY AS IT IS THERE
// ----------------------------------------------------------------------
//     prepared descriptor -> [WALKER] -> (engine) -> [ACCUMULATOR] -> cache
//
// `composepub_acceptance` case 11 models FIELD.HOST at its client seam with a
// declared response latency (`Engine::latency`) and ONE point in flight. It
// does so against a real RTL port pair, because `zhao_field_earth_adapter`
// exists; the field-major machine's counterpart adapter DOES NOT EXIST, which
// is precisely the thing 13.1 commissions. So the engine is modelled on the
// C++ side here, with the SAME declared latency, at the SAME seam in the
// dataflow.
//
// THE COMPARISON IS THEREFORE LIKE FOR LIKE ON THE ONE AXIS THAT MATTERS --
// the stream order -- and CLAUDE.md's "compare like with like, or do not
// compare" is the reason the seam was placed here rather than anywhere more
// convenient. What the two measurements do NOT share is that the vertex-major
// number contains a real adapter's two clocks and this one contains no
// adapter at all. That is stated in the census output rather than buried:
// this bench measures the WALK and the REDUCTION in RTL and the engine as a
// declared latency, so it is a FLOOR for the field-major form and must be
// read as one.
//
// THE UPDATE PORT HAS NO BACKPRESSURE. `zhao_terrain_patch_acc`'s own header
// says so in capitals -- "no ready/valid and no backpressure on any phase" --
// and section 13.4 commissions that repair. So the driver must not offer two
// updates in one clock, and the walker's `out_ready_i` is the only throttle
// in the composition. That is wired to the engine model, which is the honest
// shape: a real engine that cannot take a group is the thing that stalls the
// walk.
//
// PHASE EXCLUSIVITY is a CALLER OBLIGATION the accumulator does not enforce
// (its header, again). The C++ driver honours it with idle gaps between
// phases, exactly as `field_patch_acc_directed.cpp` does.
`default_nettype none

module tb_terrain_fieldmajor #(
    parameter int LAT_W = 33,
    parameter int LAT_H = 33
) (
    input var logic clk,
    input var logic rst_n,

    // ======================= THE WALKER ====================================
    // prepared lattice tables (sel 0 = wx by i, sel 1 = wz by j)
    input var logic               lt_we_i,
    input var logic               lt_sel_i,
    input var logic        [ 5:0] lt_idx_i,
    input var logic signed [31:0] lt_val_i,

    // association intake
    input  var logic               as_valid_i,
    output var logic               as_ready_o,
    input  var logic signed [31:0] as_fp_x0_i,
    input  var logic signed [31:0] as_fp_x1_i,
    input  var logic signed [31:0] as_fp_z0_i,
    input  var logic signed [31:0] as_fp_z1_i,
    input  var logic        [ 5:0] as_box_i0_i,
    input  var logic        [ 5:0] as_box_i1_i,
    input  var logic        [ 5:0] as_box_j0_i,
    input  var logic        [ 5:0] as_box_j1_i,

    // THE ENGINE SEAM, walker side. The C++ engine model drives `wk_ready_i`
    // and reads the group; nothing in RTL sits between here and `up_*`.
    output var logic               wk_valid_o,
    input  var logic               wk_ready_i,
    output var logic        [10:0] wk_iv_o,
    output var logic        [ 3:0] wk_mask_o,
    output var logic signed [31:0] wk_z_o,
    output var logic signed [31:0] wk_x0_o,
    output var logic signed [31:0] wk_x1_o,
    output var logic signed [31:0] wk_x2_o,
    output var logic signed [31:0] wk_x3_o,
    output var logic               wk_last_o,

    output var logic [31:0] groups_emitted_o,
    output var logic [31:0] verts_covered_o,

    // ======================= THE ACCUMULATOR ===============================
    // INIT
    input var logic               in_valid_i,
    input var logic        [ 8:0] in_g_i,
    input var logic        [ 3:0] in_mask_i,
    input var logic signed [15:0] in_base_0_i,
    input var logic signed [15:0] in_base_1_i,
    input var logic signed [15:0] in_base_2_i,
    input var logic signed [15:0] in_base_3_i,
    input var logic signed [15:0] in_scar_0_i,
    input var logic signed [15:0] in_scar_1_i,
    input var logic signed [15:0] in_scar_2_i,
    input var logic signed [15:0] in_scar_3_i,
    input var logic signed [15:0] in_bot_0_i,
    input var logic signed [15:0] in_bot_1_i,
    input var logic signed [15:0] in_bot_2_i,
    input var logic signed [15:0] in_bot_3_i,
    input var logic        [ 3:0] in_dual_i,

    // ACCUM -- the engine seam, accumulator side. `up_iv_i` and `up_mask_i`
    // are driven by the C++ engine model from the group it took off the
    // walker, so the WALKER decides coverage and this bench cannot quietly
    // substitute its own mask.
    input var logic               up_valid_i,
    input var logic        [10:0] up_iv_i,
    input var logic        [ 3:0] up_mask_i,
    input var logic        [ 3:0] up_wmask_i,
    input var logic signed [31:0] up_h_0_i,
    input var logic signed [31:0] up_h_1_i,
    input var logic signed [31:0] up_h_2_i,
    input var logic signed [31:0] up_h_3_i,
    input var logic signed [31:0] up_v_0_i,
    input var logic signed [31:0] up_v_1_i,
    input var logic signed [31:0] up_v_2_i,
    input var logic signed [31:0] up_v_3_i,
    input var logic        [31:0] up_m_0_i,
    input var logic        [31:0] up_m_1_i,
    input var logic        [31:0] up_m_2_i,
    input var logic        [31:0] up_m_3_i,
    input var logic signed [31:0] up_n_0_i,
    input var logic signed [31:0] up_n_1_i,
    input var logic signed [31:0] up_n_2_i,
    input var logic signed [31:0] up_n_3_i,

    output var logic       sat_valid_o,
    output var logic [3:0] sat_h_o,
    output var logic [3:0] sat_v_o,
    output var logic [3:0] sat_n_o,

    // DRAIN
    input var logic               dr_valid_i,
    input var logic        [ 8:0] dr_g_i,
    input var logic        [ 3:0] dr_mask_i,
    input var logic signed [15:0] dr_base_0_i,
    input var logic signed [15:0] dr_base_1_i,
    input var logic signed [15:0] dr_base_2_i,
    input var logic signed [15:0] dr_base_3_i,
    input var logic signed [15:0] dr_bot_0_i,
    input var logic signed [15:0] dr_bot_1_i,
    input var logic signed [15:0] dr_bot_2_i,
    input var logic signed [15:0] dr_bot_3_i,
    input var logic        [ 3:0] dr_dual_i,

    output var logic               out_valid_o,
    output var logic        [ 8:0] out_g_o,
    output var logic        [ 3:0] out_mask_o,
    output var logic        [ 3:0] out_dirty_o,
    output var logic signed [31:0] out_top_0_o,
    output var logic signed [31:0] out_top_1_o,
    output var logic signed [31:0] out_top_2_o,
    output var logic signed [31:0] out_top_3_o,
    output var logic signed [31:0] out_bot_0_o,
    output var logic signed [31:0] out_bot_1_o,
    output var logic signed [31:0] out_bot_2_o,
    output var logic signed [31:0] out_bot_3_o,
    output var logic signed [31:0] out_vel_0_o,
    output var logic signed [31:0] out_vel_1_o,
    output var logic signed [31:0] out_vel_2_o,
    output var logic signed [31:0] out_vel_3_o,
    output var logic        [31:0] out_mat_0_o,
    output var logic        [31:0] out_mat_1_o,
    output var logic        [31:0] out_mat_2_o,
    output var logic        [31:0] out_mat_3_o,
    output var logic signed [31:0] out_nav_0_o,
    output var logic signed [31:0] out_nav_1_o,
    output var logic signed [31:0] out_nav_2_o,
    output var logic signed [31:0] out_nav_3_o
);

  zhao_terrain_field_walk #(
      .LAT_W(LAT_W),
      .LAT_H(LAT_H)
  ) u_walk (
      .clk  (clk),
      .rst_n(rst_n),

      .lt_we_i (lt_we_i),
      .lt_sel_i(lt_sel_i),
      .lt_idx_i(lt_idx_i),
      .lt_val_i(lt_val_i),

      .as_valid_i (as_valid_i),
      .as_ready_o (as_ready_o),
      .as_fp_x0_i (as_fp_x0_i),
      .as_fp_x1_i (as_fp_x1_i),
      .as_fp_z0_i (as_fp_z0_i),
      .as_fp_z1_i (as_fp_z1_i),
      .as_box_i0_i(as_box_i0_i),
      .as_box_i1_i(as_box_i1_i),
      .as_box_j0_i(as_box_j0_i),
      .as_box_j1_i(as_box_j1_i),

      .out_valid_o(wk_valid_o),
      .out_ready_i(wk_ready_i),
      .out_iv_o   (wk_iv_o),
      .out_mask_o (wk_mask_o),
      .out_z_o    (wk_z_o),
      .out_x0_o   (wk_x0_o),
      .out_x1_o   (wk_x1_o),
      .out_x2_o   (wk_x2_o),
      .out_x3_o   (wk_x3_o),
      .out_last_o (wk_last_o),

      .groups_emitted_o(groups_emitted_o),
      .verts_covered_o (verts_covered_o)
  );

  zhao_terrain_patch_acc u_acc (
      .clk  (clk),
      .rst_n(rst_n),

      .in_valid_i (in_valid_i),
      .in_g_i     (in_g_i),
      .in_mask_i  (in_mask_i),
      .in_base_0_i(in_base_0_i),
      .in_base_1_i(in_base_1_i),
      .in_base_2_i(in_base_2_i),
      .in_base_3_i(in_base_3_i),
      .in_scar_0_i(in_scar_0_i),
      .in_scar_1_i(in_scar_1_i),
      .in_scar_2_i(in_scar_2_i),
      .in_scar_3_i(in_scar_3_i),
      .in_bot_0_i (in_bot_0_i),
      .in_bot_1_i (in_bot_1_i),
      .in_bot_2_i (in_bot_2_i),
      .in_bot_3_i (in_bot_3_i),
      .in_dual_i  (in_dual_i),

      .up_valid_i(up_valid_i),
      .up_iv_i   (up_iv_i),
      .up_mask_i (up_mask_i),
      .up_wmask_i(up_wmask_i),
      .up_h_0_i  (up_h_0_i),
      .up_h_1_i  (up_h_1_i),
      .up_h_2_i  (up_h_2_i),
      .up_h_3_i  (up_h_3_i),
      .up_v_0_i  (up_v_0_i),
      .up_v_1_i  (up_v_1_i),
      .up_v_2_i  (up_v_2_i),
      .up_v_3_i  (up_v_3_i),
      .up_m_0_i  (up_m_0_i),
      .up_m_1_i  (up_m_1_i),
      .up_m_2_i  (up_m_2_i),
      .up_m_3_i  (up_m_3_i),
      .up_n_0_i  (up_n_0_i),
      .up_n_1_i  (up_n_1_i),
      .up_n_2_i  (up_n_2_i),
      .up_n_3_i  (up_n_3_i),

      .sat_valid_o(sat_valid_o),
      .sat_h_o    (sat_h_o),
      .sat_v_o    (sat_v_o),
      .sat_n_o    (sat_n_o),

      .dr_valid_i (dr_valid_i),
      .dr_g_i     (dr_g_i),
      .dr_mask_i  (dr_mask_i),
      .dr_base_0_i(dr_base_0_i),
      .dr_base_1_i(dr_base_1_i),
      .dr_base_2_i(dr_base_2_i),
      .dr_base_3_i(dr_base_3_i),
      .dr_bot_0_i (dr_bot_0_i),
      .dr_bot_1_i (dr_bot_1_i),
      .dr_bot_2_i (dr_bot_2_i),
      .dr_bot_3_i (dr_bot_3_i),
      .dr_dual_i  (dr_dual_i),

      .out_valid_o(out_valid_o),
      .out_g_o    (out_g_o),
      .out_mask_o (out_mask_o),
      .out_dirty_o(out_dirty_o),
      .out_top_0_o(out_top_0_o),
      .out_top_1_o(out_top_1_o),
      .out_top_2_o(out_top_2_o),
      .out_top_3_o(out_top_3_o),
      .out_bot_0_o(out_bot_0_o),
      .out_bot_1_o(out_bot_1_o),
      .out_bot_2_o(out_bot_2_o),
      .out_bot_3_o(out_bot_3_o),
      .out_vel_0_o(out_vel_0_o),
      .out_vel_1_o(out_vel_1_o),
      .out_vel_2_o(out_vel_2_o),
      .out_vel_3_o(out_vel_3_o),
      .out_mat_0_o(out_mat_0_o),
      .out_mat_1_o(out_mat_1_o),
      .out_mat_2_o(out_mat_2_o),
      .out_mat_3_o(out_mat_3_o),
      .out_nav_0_o(out_nav_0_o),
      .out_nav_1_o(out_nav_1_o),
      .out_nav_2_o(out_nav_2_o),
      .out_nav_3_o(out_nav_3_o)
  );

endmodule

`default_nettype wire
