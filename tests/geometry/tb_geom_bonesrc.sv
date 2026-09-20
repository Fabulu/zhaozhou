// tb_geom_bonesrc.sv — `zhao_geom_bonesrc` wired to the REAL
// `zhao_geom_pose_decode`, which is the only arrangement that can test the
// claim this packet rests on.
//
// The claim is that the decoder's COMBINATIONAL source contract can be met by a
// prefetch in front of synchronous memories, so the ~17.6 kbit asynchronous
// store owner ruling R90 priced is not required. That is a statement about the
// join between two blocks and about TIMING WITHIN A CYCLE, so a bench that
// stubs either side cannot see it: a stub that answers instantly proves
// nothing, and a stub that answers late proves nothing either.
//
// So both real blocks are here, joined exactly as the console would join them,
// and the palette that comes out is compared against `zref::creature` by the
// C++ side. If the prefetch is ever a cycle late the decoder latches a stale
// bone and the palette is wrong — which is the failure this whole arrangement
// has to be shown not to have.
`default_nettype none

module tb_geom_bonesrc #(
    parameter int MAX_BONES = 32,
    parameter int SRC_STYLE = 0
) (
    input  logic clk,
    input  logic rst_n,

    // ---- fill --------------------------------------------------------------
    input  logic        fill_we_i,
    input  logic        fill_sel_i,
    input  logic [ 4:0] fill_bone_i,
    input  logic [ 3:0] fill_word_i,
    input  logic [63:0] fill_data_i,

    // ---- run one palette ----------------------------------------------------
    input  logic        req_i,
    input  logic [ 5:0] bone_count_i,
    input  logic signed [31:0] root_dx_i,
    input  logic signed [31:0] root_dy_i,
    input  logic signed [31:0] root_dz_i,

    // ---- the palette out ----------------------------------------------------
    input  logic        out_ready_i,
    output logic        out_valid_o,
    output logic [ 4:0] out_bone_o,
    output logic signed [31:0] out_m0_o,
    output logic signed [31:0] out_m1_o,
    output logic signed [31:0] out_m2_o,
    output logic signed [31:0] out_m3_o,
    output logic signed [31:0] out_m4_o,
    output logic signed [31:0] out_m5_o,
    output logic signed [31:0] out_m6_o,
    output logic signed [31:0] out_m7_o,
    output logic signed [31:0] out_m8_o,
    output logic signed [31:0] out_m9_o,
    output logic signed [31:0] out_m10_o,
    output logic signed [31:0] out_m11_o,

    output logic        busy_o,
    output logic        done_o,
    output logic [31:0] palettes_decoded_o,

    // ---- the source's detectors --------------------------------------------
    output logic [31:0] bone_prefetch_late_o,
    output logic [31:0] bone_rest_nonrigid_o,
    output logic [31:0] bone_reserved_nz_o,
    output logic [31:0] bone_fills_o,

    // ---- observability the C++ side needs to see the JOIN, not just the end -
    // `bone_idx_o` and the parent the source answers with, sampled every cycle,
    // so a stale answer is visible AT the fetch rather than only in the palette.
    output logic [ 4:0] obs_bone_idx_o,
    output logic [ 4:0] obs_bone_parent_o,
    output logic signed [31:0] obs_bone_tx_o,
    output logic signed [15:0] obs_quat_w_o,
    output logic signed [31:0] obs_inv_rest3_o
  );

  logic        start_w;
  logic [ 4:0] bone_idx_w;
  logic [ 4:0] bone_parent_w;
  logic signed [31:0] bone_tx_w, bone_ty_w, bone_tz_w;
  logic signed [15:0] quat_w_w, quat_x_w, quat_y_w, quat_z_w;
  logic signed [31:0] inv_rest_w [12];
  logic signed [31:0] out_m_w [12];
  logic        ready_w;

  zhao_geom_bonesrc #(
      .MAX_BONES (MAX_BONES),
      .SRC_STYLE (SRC_STYLE)
  ) u_src (
      .clk   (clk),
      .rst_n (rst_n),

      .fill_we_i   (fill_we_i),
      .fill_sel_i  (fill_sel_i),
      .fill_bone_i (fill_bone_i),
      .fill_word_i (fill_word_i),
      .fill_data_i (fill_data_i),

      .req_i        (req_i),
      .bone_count_i (bone_count_i),
      .start_o      (start_w),
      .ready_o      (ready_w),

      .bone_idx_i    (bone_idx_w),
      .bone_parent_o (bone_parent_w),
      .bone_tx_o     (bone_tx_w),
      .bone_ty_o     (bone_ty_w),
      .bone_tz_o     (bone_tz_w),
      .quat_w_o      (quat_w_w),
      .quat_x_o      (quat_x_w),
      .quat_y_o      (quat_y_w),
      .quat_z_o      (quat_z_w),
      .inv_rest_o    (inv_rest_w),

      .bone_prefetch_late_o (bone_prefetch_late_o),
      .bone_rest_nonrigid_o (bone_rest_nonrigid_o),
      .bone_reserved_nz_o   (bone_reserved_nz_o),
      .bone_fills_o         (bone_fills_o)
  );

  zhao_geom_pose_decode #(
      .MAX_BONES (MAX_BONES)
  ) u_dec (
      .clk   (clk),
      .rst_n (rst_n),

      .start_i      (start_w),
      .busy_o       (busy_o),
      .bone_count_i (bone_count_i),
      .root_dx_i    (root_dx_i),
      .root_dy_i    (root_dy_i),
      .root_dz_i    (root_dz_i),

      .bone_idx_o    (bone_idx_w),
      .bone_parent_i (bone_parent_w),
      .bone_tx_i     (bone_tx_w),
      .bone_ty_i     (bone_ty_w),
      .bone_tz_i     (bone_tz_w),
      .quat_w_i      (quat_w_w),
      .quat_x_i      (quat_x_w),
      .quat_y_i      (quat_y_w),
      .quat_z_i      (quat_z_w),
      .inv_rest_i    (inv_rest_w),

      .out_valid_o (out_valid_o),
      .out_ready_i (out_ready_i),
      .out_bone_o  (out_bone_o),
      .out_m_o     (out_m_w),

      .done_o             (done_o),
      .palettes_decoded_o (palettes_decoded_o)
  );

  assign out_m0_o  = out_m_w[0];
  assign out_m1_o  = out_m_w[1];
  assign out_m2_o  = out_m_w[2];
  assign out_m3_o  = out_m_w[3];
  assign out_m4_o  = out_m_w[4];
  assign out_m5_o  = out_m_w[5];
  assign out_m6_o  = out_m_w[6];
  assign out_m7_o  = out_m_w[7];
  assign out_m8_o  = out_m_w[8];
  assign out_m9_o  = out_m_w[9];
  assign out_m10_o = out_m_w[10];
  assign out_m11_o = out_m_w[11];

  assign obs_bone_idx_o    = bone_idx_w;
  assign obs_bone_parent_o = bone_parent_w;
  assign obs_bone_tx_o     = bone_tx_w;
  assign obs_quat_w_o      = quat_w_w;
  assign obs_inv_rest3_o   = inv_rest_w[3];

  // `ready_o` is observed through the fill sequencer's behaviour rather than
  // exported; naming it here keeps the lint quiet about an intentionally
  // unread output without a waiver that would also hide a real one.
  wire unused_ok = ready_w;

endmodule : tb_geom_bonesrc
