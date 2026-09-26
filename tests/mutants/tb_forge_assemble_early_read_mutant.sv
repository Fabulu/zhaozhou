// tb_forge_assemble_early_read_mutant.sv -- THE MUTANT'S BENCH WRAPPER.
// NOT PRODUCTION, and not a test of the design.
//
// Identical to `tests/forge/tb_forge_assemble.sv` except that it instantiates
// `zhao_forge_assemble_early_read_mutant` instead of `zhao_forge_assemble`, and
// is renamed so no source list elaborates it by mistake. It exists only so the
// mutant can be driven by the same stimulus the real block gets.
//
// See `tests/mutants/zhao_forge_assemble_early_read_mutant.sv` for what was
// mutated and why. REGENERATE BOTH if the production bench changes shape.
//// tb_forge_assemble.sv -- the bench wrapper for `zhao_forge_assemble`, with a
// FAKE PROJECTOR standing in for `zhao_part_project`'s third arm.
//
// WHY A FAKE PROJECTOR AND NOT A REAL ONE. The property under test is the
// ASSEMBLER's: that it never offers a vertex it has no room to land, that it
// joins index triples to the vertices the ORDERING CONVENTION says they name,
// and that a triple naming a vertex the job never emitted is refused rather
// than wrapped. None of that is a statement about projection arithmetic, and
// instantiating the real projector would put 36 clocks of a different block's
// pipeline between the stimulus and the check -- a bench measuring two things
// tells you which one failed only by luck.
//
// WHAT THE FAKE MUST REPRODUCE EXACTLY, because these are the properties the
// assembler is built against and a fake that softened them would test nothing:
//
//   1. THE RESULT PORT HAS NO READY. The fake returns unconditionally, LAT
//      cycles after the accept, whatever the DUT is doing. If the DUT ever has
//      nowhere to put a result, the result is LOST and the job's counts do not
//      reconcile -- which is exactly the failure the in-flight throttle exists
//      to prevent, and it shows up as a hang in A_DRAIN rather than as a
//      silently wrong picture.
//   2. THE RIDER COMES BACK VERBATIM. The slot the DUT sent is the slot the
//      fake returns; nothing re-derives it. `zhao_project_service`'s own header
//      is the reason -- the rider "cannot drift from the data it describes".
//   3. THE ORDER IS FIFO, because the real service is a rigid pipeline.
//
// THE FAKE'S ARITHMETIC IS DELIBERATELY TRIVIAL AND INVERTIBLE: screen x is
// the world x's low bits and screen y the world y's, so the test can assert
// that corner B of a triangle carries VERTEX i1's coordinates and not i0's or
// the previous triple's. A fake returning the same numbers for every vertex
// would let a completely broken join pass every check.
`default_nettype none

module tb_forge_assemble_early_read_mutant #(
    // Small on purpose. The join, the throttle and the refusal are all
    // exercised in tens of vertices, and a 520-entry store makes every run
    // slower without making any check discriminate more. The PRODUCTION value
    // lives in `zhao_console_core`'s FORGE_MAX_VERTS; this is the bench's.
// RAISED 64 -> 128 ON 2026-09-26, and it must stay ABOVE INFLIGHT. The
    // throttle checks below only mean something when the store is bigger than
    // the in-flight bound; with MAX_VERTS == INFLIGHT the stream never stalls,
    // `slot_pressure_o` never moves, and two real checks silently stop testing
    // anything. This is the bench's number, NOT the capacity law -- the
    // shipping value lives in zhao_console_core's FORGE_MAX_VERTS and is
    // untouched.
    parameter int unsigned MAX_VERTS = 128,
    // INFLIGHT MUST BE A POWER OF TWO (zhao_forge_assemble's elaboration check,
    // added 2026-09-26). This bench said 40 from the day it was written, which
    // is illegal: the DUT's depth-queue pointers are masked to $clog2(INFLIGHT)
    // bits, so at 40 the write index reached 63 into a 40-deep array and the
    // canonical depth of every vertex from index 40 up was silently dropped.
    // Every counter still balanced. 64 is the shipping console's own value.
    parameter int unsigned INFLIGHT  = 64,
    // The projector's round trip, in clocks. 36 is `zhao_project_core`'s
    // stated GEOM latency and the number the DUT's elaboration guard is
    // written against.
    parameter int unsigned LAT = 36
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- the world vertex stream ------------------------------------------
    input  var logic               v_valid_i,
    output var logic               v_ready_o,
    input  var logic signed [31:0] v_x_i,
    input  var logic signed [31:0] v_y_i,
    input  var logic signed [31:0] v_z_i,
    input  var logic               v_last_i,

    // ---- the index stream --------------------------------------------------
    input  var logic               t_valid_i,
    output var logic               t_ready_o,
    input  var logic [15:0]        t_i0_i,
    input  var logic [15:0]        t_i1_i,
    input  var logic [15:0]        t_i2_i,
    input  var logic [15:0]        t_material_i,
    input  var logic [15:0]        t_src_id_i,
    input  var logic               t_last_i,

    // THE JOB'S MATERIAL PAIR, both halves on this bench's own ports so the
    // carriage detector below can be FIRED WITH LEGAL STIMULUS AT THESE PORTS
    // -- drive `t_material_i` differently from `j_material_id_i` and the skew
    // is real, with no mutant needed.
    input  var logic [31:0]        j_material_set_i,
    input  var logic [15:0]        j_material_id_i,
    // The per-job DECLARATION, added 2026-09-23 (SHADOWRIDE) when this block
    // gained a second producer and one composer constant stopped being true of
    // both. See the RTL's port comments.
    input  var logic [ 1:0]        j_material_mode_i,
    input  var logic [ 7:0]        j_vertex_alpha_i,
    input  var logic [31:0]        j_frag_state_i,
    // THE SIDEBAND'S HANDSHAKE, on this bench's own ports. In the console it
    // is the page bank's job issue; here the driver performs it, which is what
    // lets a test present a pair and THEN change the ports underneath -- the
    // exact fault `mat_skew_o` exists to catch.
    input  var logic               j_valid_i,
    output var logic               j_ready_o,

    input  var logic signed [31:0] art_r_i,
    input  var logic signed [31:0] art_g_i,
    input  var logic signed [31:0] art_b_i,
    input  var logic signed [31:0] art_alpha_i,
    input  var logic [ 7:0]        art_quality_tier_i,
    input  var logic [ 1:0]        art_cull_mode_i,

    input  var logic               view_sel_i,

    // ---- the triangle out ---------------------------------------------------
    output var logic               o_valid_o,
    input  var logic               o_ready_i,
    output var logic signed [20:0] o_ax_o,
    output var logic signed [20:0] o_ay_o,
    output var logic signed [20:0] o_bx_o,
    output var logic signed [20:0] o_by_o,
    output var logic signed [20:0] o_cx_o,
    output var logic signed [20:0] o_cy_o,
    output var logic [ 2:0]        o_behind_o,
    output var logic [15:0]        o_src_id_o,
    output var logic               o_untex_o,
    output var logic [ 1:0]        o_cull_mode_o,
    // The seven-slot packet, UNFLATTENED to the three slots the test reads.
    // The rest are not exposed because a bench that read all 21 words would be
    // asserting the packer's shape rather than its content.
    output var logic [31:0]        o_a_invw_o,
    output var logic [31:0]        o_a_uow_o,
    output var logic [31:0]        o_a_r_o,
    output var logic [31:0]        o_a_alpha_o,
    output var logic [31:0]        o_b_invw_o,
    output var logic [31:0]        o_c_invw_o,
    output var logic [31:0]        o_material_set_o,
    output var logic [15:0]        o_material_id_o,
    output var logic [ 1:0]        o_material_mode_o,
    output var logic [ 7:0]        o_vertex_alpha_o,
    output var logic [31:0]        o_frag_state_o,
    output var logic [ 7:0]        o_quality_tier_o,

    output var logic               busy_o,

    // ---- the DUT's census ---------------------------------------------------
    output var logic [31:0] jobs_o,
    output var logic [31:0] vertices_o,
    output var logic [31:0] triangles_o,
    output var logic [31:0] index_oor_o,
    output var logic [31:0] vtx_overflow_o,
    output var logic [31:0] slot_pressure_o,
    output var logic [31:0] dq_refused_o,
    output var logic [31:0] dq_stray_o,
    output var logic [31:0] proj_stray_o,
    output var logic [31:0] mat_skew_o,

    // ---- the fake projector's own evidence ----------------------------------
    // MAX IN FLIGHT EVER, which is the throttle's measurement. It must never
    // exceed INFLIGHT, and a bench that did not publish it could not tell a
    // working throttle from a lucky one.
    output var logic [31:0] fake_max_inflight_o,
    output var logic [31:0] fake_accepted_o,
    output var logic [31:0] fake_returned_o
);

  localparam int unsigned ATTRS = 7;
  localparam int unsigned S_INVW = 0, S_UOW = 1, S_VOW = 2;
  localparam int unsigned S_R = 3, S_G = 4, S_B = 5, S_ALPHA = 6;

  logic               f_valid, f_ready;
  logic signed [31:0] f_vx, f_vy, f_vz;
  logic               f_view;
  logic [14:0]        f_slot;

  logic               rs_valid;
  logic signed [20:0] rs_x, rs_y;
  logic [30:0]        rs_w;
  logic               rs_behind;
  logic [14:0]        rs_slot;

  logic [ATTRS*32-1:0] attr_a, attr_b, attr_c;

  assign o_a_invw_o  = attr_a[32*S_INVW  +: 32];
  assign o_a_uow_o   = attr_a[32*S_UOW   +: 32];
  assign o_a_r_o     = attr_a[32*S_R     +: 32];
  assign o_a_alpha_o = attr_a[32*S_ALPHA +: 32];
  assign o_b_invw_o  = attr_b[32*S_INVW  +: 32];
  assign o_c_invw_o  = attr_c[32*S_INVW  +: 32];

  // ==========================================================================
  // THE FAKE PROJECTOR
  //
  // A shift register LAT deep. An accepted vertex enters at the head and its
  // result leaves the tail LAT clocks later, UNCONDITIONALLY. `f_ready` is
  // constant 1: the real service's ready is a function of its own valid and
  // this bench must not be the thing limiting the DUT, or the throttle under
  // test would never be exercised.
  // ==========================================================================
  assign f_ready = 1'b1;

  logic               pv_q   [LAT];
  logic signed [20:0] px_q   [LAT];
  logic signed [20:0] py_q   [LAT];
  logic [30:0]        pw_q   [LAT];
  logic [14:0]        psl_q  [LAT];
  logic               pbh_q  [LAT];

  logic [31:0] inflight_q;

  integer i;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      for (i = 0; i < LAT; i = i + 1) begin
        pv_q[i]  <= 1'b0;
        px_q[i]  <= 21'sd0;
        py_q[i]  <= 21'sd0;
        pw_q[i]  <= 31'd0;
        psl_q[i] <= 15'd0;
        pbh_q[i] <= 1'b0;
      end
      inflight_q          <= 32'd0;
      fake_max_inflight_o <= 32'd0;
      fake_accepted_o     <= 32'd0;
      fake_returned_o     <= 32'd0;
    end else begin
      for (i = LAT - 1; i > 0; i = i - 1) begin
        pv_q[i]  <= pv_q[i-1];
        px_q[i]  <= px_q[i-1];
        py_q[i]  <= py_q[i-1];
        pw_q[i]  <= pw_q[i-1];
        psl_q[i] <= psl_q[i-1];
        pbh_q[i] <= pbh_q[i-1];
      end
      pv_q[0] <= f_valid && f_ready;
      // INVERTIBLE ON PURPOSE: the test asserts that corner B carries vertex
      // i1's numbers, which it can only do if the projection is a function of
      // the vertex. Returning a constant would let a broken join pass.
      px_q[0]  <= $signed(f_vx[20:0]);
      py_q[0]  <= $signed(f_vy[20:0]);
      // A non-zero w, so `zhao_geom_depthquant_stream` has something legal to
      // convert. Derived from z so different vertices get different depths.
      pw_q[0]  <= {15'd0, f_vz[15:0]} + 31'd65536;
      psl_q[0] <= f_slot;
      pbh_q[0] <= 1'b0;

      if (f_valid && f_ready) begin
        inflight_q      <= inflight_q + 32'd1 - (pv_q[LAT-1] ? 32'd1 : 32'd0);
        fake_accepted_o <= fake_accepted_o + 32'd1;
      end else if (pv_q[LAT-1]) begin
        inflight_q <= inflight_q - 32'd1;
      end
      if (pv_q[LAT-1]) fake_returned_o <= fake_returned_o + 32'd1;
      if (inflight_q > fake_max_inflight_o) fake_max_inflight_o <= inflight_q;
    end
  end

  assign rs_valid  = pv_q[LAT-1];
  assign rs_x      = px_q[LAT-1];
  assign rs_y      = py_q[LAT-1];
  assign rs_w      = pw_q[LAT-1];
  assign rs_behind = pbh_q[LAT-1];
  assign rs_slot   = psl_q[LAT-1];

  zhao_forge_assemble_early_read_mutant #(
      .MAX_VERTS (MAX_VERTS),
      .IDW       (16),
      .ATTRS     (ATTRS),
      .INFLIGHT  (INFLIGHT),
      .SLOT_INVW (S_INVW),
      .SLOT_UOW  (S_UOW),
      .SLOT_VOW  (S_VOW),
      .SLOT_R    (S_R),
      .SLOT_G    (S_G),
      .SLOT_B    (S_B),
      .SLOT_ALPHA(S_ALPHA)
  ) u_dut (
      .clk   (clk),
      .rst_n (rst_n),

      .v_valid_i(v_valid_i),
      .v_ready_o(v_ready_o),
      .v_x_i    (v_x_i),
      .v_y_i    (v_y_i),
      .v_z_i    (v_z_i),
      .v_last_i (v_last_i),

      .t_valid_i   (t_valid_i),
      .t_ready_o   (t_ready_o),
      .t_i0_i      (t_i0_i),
      .t_i1_i      (t_i1_i),
      .t_i2_i      (t_i2_i),
      .t_material_i(t_material_i),
      .t_src_id_i  (t_src_id_i),
      .t_last_i    (t_last_i),

      .j_material_set_i(j_material_set_i),
      .j_material_id_i (j_material_id_i),
      .j_material_mode_i(j_material_mode_i),
      .j_vertex_alpha_i (j_vertex_alpha_i),
      .j_frag_state_i   (j_frag_state_i),
      .j_valid_i       (j_valid_i),
      .j_ready_o       (j_ready_o),

      .art_r_i           (art_r_i),
      .art_g_i           (art_g_i),
      .art_b_i           (art_b_i),
      .art_alpha_i       (art_alpha_i),
      .art_quality_tier_i(art_quality_tier_i),
      .art_cull_mode_i   (art_cull_mode_i),

      .f_valid_o (f_valid),
      .f_ready_i (f_ready),
      .f_vx_o    (f_vx),
      .f_vy_o    (f_vy),
      .f_vz_o    (f_vz),
      .f_view_o  (f_view),
      .f_slot_o  (f_slot),
      .rs_valid_i (rs_valid),
      .rs_x_i     (rs_x),
      .rs_y_i     (rs_y),
      .rs_w_i     (rs_w),
      .rs_behind_i(rs_behind),
      .rs_profile_i(2'd0),
      .rs_slot_i  (rs_slot),

      .view_sel_i(view_sel_i),

      .o_valid_o       (o_valid_o),
      .o_ready_i       (o_ready_i),
      .o_ax_o          (o_ax_o),
      .o_ay_o          (o_ay_o),
      .o_bx_o          (o_bx_o),
      .o_by_o          (o_by_o),
      .o_cx_o          (o_cx_o),
      .o_cy_o          (o_cy_o),
      .o_behind_o      (o_behind_o),
      .o_src_id_o      (o_src_id_o),
      .o_untex_o       (o_untex_o),
      .o_cull_mode_o   (o_cull_mode_o),
      .o_attr_a_o      (attr_a),
      .o_attr_b_o      (attr_b),
      .o_attr_c_o      (attr_c),
      .o_material_set_o(o_material_set_o),
      .o_material_id_o (o_material_id_o),
      .o_material_mode_o(o_material_mode_o),
      .o_vertex_alpha_o (o_vertex_alpha_o),
      .o_frag_state_o   (o_frag_state_o),
      .o_quality_tier_o(o_quality_tier_o),

      .busy_o(busy_o),

      .jobs_o         (jobs_o),
      .vertices_o     (vertices_o),
      .triangles_o    (triangles_o),
      .index_oor_o    (index_oor_o),
      .vtx_overflow_o (vtx_overflow_o),
      .slot_pressure_o(slot_pressure_o),
      .dq_refused_o   (dq_refused_o),
      .dq_stray_o     (dq_stray_o),
      .proj_stray_o   (proj_stray_o),
      .mat_skew_o     (mat_skew_o)
  );

  /* verilator lint_off UNUSEDSIGNAL */
  wire _unused = &{1'b0, f_view, attr_a[32*S_VOW +: 32], attr_a[32*S_G +: 32],
                   attr_a[32*S_B +: 32], attr_b[ATTRS*32-1:32], attr_c[ATTRS*32-1:32]};
  /* verilator lint_on UNUSEDSIGNAL */

endmodule : tb_forge_assemble_early_read_mutant

`default_nettype wire
