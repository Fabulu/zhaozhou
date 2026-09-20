// zhao_geom_lodstate.sv -- THE PER-INSTANCE LADDER STATE, and the owner
// `zhao_geom_lod` has been waiting for since phase 8. Owner ruling R68,
// sub-build 3.
//
// ENFORCED-BY: tests/geometry/geom_lodstate_directed.cpp:main
// REFERENCE:   zref::creature::lod_update / zref::creature::LodState
//              (reference/include/zref/zref_creature.hpp)
//
// ---------------------------------------------------------------------------
// THE FIFTEENTH FALSE-ABSENCE CLAIM IS WHAT THIS BLOCK ANSWERS
// ---------------------------------------------------------------------------
// `fpga/rtl/prod/zhao_console_core.sv` records it in full, and it is worth
// restating because it is the reason this file exists:
//
//   "THERE IS NO LodState IN `zhao_geom_meshfetch`. The strings `rung`, `lod`
//    and `LodState` occur exactly ONCE in all 505 lines of that file, in a
//    header comment at line 18 pointing AT `zhao_geom_lod.sv` ... So the rung
//    is not unported state: NOBODY HOLDS LADDER STATE AT ALL, and
//    `zhao_geom_lod` is a built, stateless evaluator waiting for an owner."
//
// `zhao_geom_lod`'s own header says the same thing from the other side: "LADDER
// STATE IS THE CALLER'S. GEOM.MESHFETCH holds one LodState per live instance,
// so this block takes the state in and hands the stepped state back rather than
// owning an array whose size it cannot know." There was no such caller. This is
// it, and the array's size is `INSTANCES`, which is a parameter for exactly the
// reason that sentence gives.
//
// ---------------------------------------------------------------------------
// WHERE THE TICK COMES FROM, AND WHY IT IS A TAP AND NOT A STAGE
// ---------------------------------------------------------------------------
// The ladder wants, once per creature instance per frame: the instance's WORLD
// CENTRE, its creature TYPE, and its previous rung. All three are together in
// exactly one place in this console -- the job `zhao_geom_drawjob` hands
// `zhao_geom_meshfetch`. That packet carries `j_instance_id`, the draw's form
// handle, and `j_xform[12]`, whose translation column (elements 3, 7 and 11) IS
// the instance's world position.
//
// So this block OBSERVES that handshake rather than sitting in it. The
// alternative -- joining GEOM.MESHFETCH's `cull_c{x,y,z}_o` at S_CULL to its
// `r_instance_id_o` at S_EMIT -- is a join across pipeline stages, and while
// that particular one is provable (the block is a strict single-in-flight FSM),
// core entry I39 refuses exactly this shape by name and the job packet makes
// the join unnecessary: one handshake, all three fields, no timing argument.
//
// A TAP CANNOT STALL WHAT IT WATCHES. `j_fire_i` is an observation, never a
// term in the producer's `ready`. That is deliberate and it has a price: a job
// arriving while an evaluation is in flight is DROPPED AND COUNTED
// (`dropped_o`), not queued. See the rate argument below for why the drop
// counter is expected to read zero and is still not allowed to be silent.
//
// ---------------------------------------------------------------------------
// ONE TICK PER INSTANCE PER FRAME, ESTABLISHED STRUCTURALLY
// ---------------------------------------------------------------------------
// `zref::creature::kLodHoldTicks` is FIFTEEN TICKS, and `creature_sim` calls
// `lod_update` once per instance per frame -- so a tick is a FRAME, and a block
// that ticked per MESHLET would burn the whole minimum hold in a single frame
// of one creature and turn charter section 9's stability law into noise.
//
// `zhao_geom_drawjob` emits every meshlet of one draw consecutively (it walks
// the mesh stream's descriptors to completion before accepting the next
// DrawForm), so the FIRST job of a draw is recognisable without a frame-scoped
// bitmap: it is the one whose instance id differs from the previous accepted
// job's. That is a property of the producer's FSM, not of a workload, which is
// why `skipped_repeat_o` counts the rest rather than a filter claiming to.
//
// A game that issues two DrawForms for one instance in a frame ticks twice.
// That is the game's statement that the instance was drawn twice and this block
// does not second-guess it; `frame_i` exists to clear the run marker at the
// frame boundary so the first draw of the NEXT frame always ticks, even when it
// names the same instance as the last draw of this one.
//
// ---------------------------------------------------------------------------
// ONE LodState PER INSTANCE, NOT PER CAMERA -- AND THAT IS A DEVIATION
// ---------------------------------------------------------------------------
// `zref::creature::CreatureInstance` holds ONE `LodState`, and `lod_update`
// takes one threshold. PART.LADDER's ruling of 2026-08-31 section 2.5 says
// particle representation selection is PER CAMERA, and `zhao_part_project`
// carries `p_prev_rung_i`/`p_hold_i` per (particle, camera) accordingly.
//
// The creature reference does NOT say that, and nothing ratifies it for
// creatures. This block follows the reference -- one state per instance, and
// `view_i` selects which camera's threshold that single ladder is measured
// against -- and the disagreement is REPORTED rather than resolved here. It is
// an owner decision, it is written up in the packet's findings, and the cost of
// changing it is one more index bit on the store.
//
// ---------------------------------------------------------------------------
// WHAT IT INSTANTIATES AND WHAT IT TAKES AS A PORT
// ---------------------------------------------------------------------------
// INSIDE: `zhao_geom_lod` (the ladder) and `zhao_geom_projradius` (the
// projected bound radius). Both have exactly one customer in this machine and
// no other, so a port for either would be a wire with one end.
//
// OUTSIDE: the ladder-constant bank (`zhao_geom_ladderbank`), because it is a
// RESIDENT STORE fed by MEM.UPLOAD's publication and that is a different
// concern with a different lifetime; and the CENTRE PROJECTION, because
// `w` for the instance centre comes from the shared projector's client A and
// owner ruling R3 keeps that a time-multiplex rather than a third port -- so
// the multiplexer is the composer's, not this block's.
//
// ---------------------------------------------------------------------------
// THE RATE
// ---------------------------------------------------------------------------
// One evaluation is: a bank lookup (2 clocks), the centre projection (the
// projector's 36-clock core plus arbitration), the radius (121 clocks) and the
// ladder (5 clocks) -- call it 200. At `zhao_geom_drawjob`'s content tier of
// 256 instance transforms that is 51,200 clocks of the frame's 1,666,666, or
// 3.1%. `zhao_geom_meshfetch`'s own meshlet loop is 305 clocks per meshlet and
// a creature is many meshlets, so the tap is not the thing that can fall
// behind. `dropped_o` is the number that says whether that reasoning held.
//
// Conservative SystemVerilog subset only (charter section 2).
`default_nettype none

module zhao_geom_lodstate #(
    // Live creature instances whose ladder state is held. NAMED AND EDITABLE
    // (CLAUDE.md rule 6). 256 matches `zhao_geom_drawjob`'s XFORMS, which owner
    // ruling R29's content tier sets; an instance id at or above this is
    // REFUSED and counted, never wrapped -- wrapping would give two creatures
    // one ladder and make each other's hysteresis look like a bug.
    parameter int unsigned INSTANCES = 256,
    parameter int unsigned IIDW = $clog2(INSTANCES)
) (
    input var logic clk,
    input var logic rst_n,

    // ---- the frame boundary -------------------------------------------------
    // A one-cycle pulse. It clears the run marker so the first draw of a new
    // frame always ticks, even when it names the instance the last draw of the
    // previous frame named.
    input var logic frame_i,

    // ---- the job tap: GEOM.DRAWJOB -> GEOM.MESHFETCH, OBSERVED --------------
    // `j_fire_i` is that handshake's (valid && ready). Nothing here is a term
    // in it.
    input var logic               j_fire_i,
    input var logic        [15:0] j_instance_id_i,
    input var logic        [23:0] j_form_index_i,
    // The instance transform's TRANSLATION COLUMN -- xform[3], xform[7],
    // xform[11] of the row-major 3x4 -- which is the instance's world position.
    input var logic signed [31:0] j_cx_i,
    input var logic signed [31:0] j_cy_i,
    input var logic signed [31:0] j_cz_i,
    // Which camera's threshold this instance's ladder is measured against.
    input var logic               j_view_i,

    // ---- the ladder-constant bank (zhao_geom_ladderbank) -------------------
    output var logic               q_valid_o,
    output var logic        [23:0] q_form_o,
    input  var logic               a_valid_i,
    input  var logic               a_hit_i,
    input  var logic signed [31:0] a_bound_i,
    input  var logic signed [31:0] a_micro_i,
    input  var logic signed [31:0] a_splat_i,
    input  var logic signed [31:0] a_glint_i,

    // ---- the instance centre, through the shared projector's client A ------
    // Owner ruling R3 keeps client A a TIME-MULTIPLEX, so the multiplexer is
    // the composer's and this is a request/answer pair, not a projector.
    output var logic               pr_valid_o,
    input  var logic               pr_ready_i,
    output var logic signed [31:0] pr_x_o,
    output var logic signed [31:0] pr_y_o,
    output var logic signed [31:0] pr_z_o,
    output var logic               pr_view_o,
    input  var logic               pr_ans_valid_i,
    input  var logic        [30:0] pr_w_i,
    input  var logic               pr_behind_i,

    // ---- the view terms, from zhao_view_projscale --------------------------
    input var logic [31:0] kx0_i,
    input var logic [31:0] kx1_i,
    input var logic [11:0] vw0_i,
    input var logic [11:0] vw1_i,

    // ---- the governor's per-camera threshold (owner ruling R26) ------------
    input var logic signed [31:0] thresh0_i,
    input var logic signed [31:0] thresh1_i,

    // ---- the caster out: FORGE.SHADOW's {world x, z, radius, rung, src_id} --
    // `c_radius_o` is the creature TYPE's bind-pose bound radius in fx16 world
    // metres -- the same number the ladder divides by. It is NOT a contact
    // footprint: turning a bounding sphere into the ellipse a shadow occupies
    // is an ART decision, and CLAUDE.md rule 6 puts it in a named, editable
    // constant at FORGE.SHADOW's own composition, beside the depth biases that
    // already live there. This block emits the measured quantity and invents
    // no factor.
    output var logic               c_valid_o,
    input  var logic               c_ready_i,
    output var logic        [15:0] c_instance_id_o,
    output var logic signed [31:0] c_x_o,
    output var logic signed [31:0] c_z_o,
    output var logic signed [31:0] c_radius_o,
    output var logic        [ 1:0] c_rung_o,
    output var logic               c_view_o,

    // ---- evidence -----------------------------------------------------------
    output var logic [31:0] ticks_o,            // ladder evaluations completed
    output var logic [31:0] skipped_repeat_o,   // a later meshlet of the same draw
    output var logic [31:0] bank_miss_o,        // the form has no ladder row
    output var logic [31:0] no_radius_o,        // behind the eye, or a bad bound
    output var logic [31:0] dropped_o,          // a new instance while busy
    output var logic [31:0] out_of_range_o,     // instance id at or above INSTANCES
    output var logic [31:0] rung_counts_o [4],  // ticks that settled at each rung
    // The radius service's own census, EXPORTED rather than tied to empty pins.
    // A census inside a census top is still evidence, and `rad_behind_o` in
    // particular is the number that separates "the ladder did not tick because
    // the creature is behind the camera" from "it did not tick because the bank
    // had no row" -- two different absences that look identical in `ticks_o`.
    output var logic [31:0] rad_evaluations_o,
    output var logic [31:0] rad_behind_o,
    output var logic [31:0] rad_bad_bound_o,
    output var logic [31:0] rad_saturated_o,
    output var logic        busy_o
);

  // Quartus 17.0 needs an elaboration check inside `initial begin ... end`; a
  // bare module-scope `if` is a syntax error there however clean the lint, and
  // `--lint-only` does not run this, so it is fired by parameter override in
  // the directed test rather than claimed by the lint.
  initial begin
    if (INSTANCES < 2 || INSTANCES > 65536)
      $fatal(1, "zhao_geom_lodstate: INSTANCES is %0d; the instance id is 16 bits",
             INSTANCES);
    if (IIDW != $clog2(INSTANCES))
      $fatal(1, "zhao_geom_lodstate: IIDW is %0d but INSTANCES %0d needs %0d",
             IIDW, INSTANCES, $clog2(INSTANCES));
  end

  // ---- the ladder state: one {rung, hold} per instance ---------------------
  // `zref::creature::LodState` is `{LodRung rung = kMesh; uint16_t hold = 0;}`
  // and the reset below is that initialiser, not a convenient zero: kMesh IS
  // rung 0, and a creature that has never been evaluated is at the FINEST rung
  // rather than the coarsest. The other way round, a creature would pop into
  // existence as a splat and take fifteen ticks to become itself.
  localparam int unsigned STW = 18;   // {rung[1:0], hold[15:0]}
  logic [STW-1:0] st_q [INSTANCES];

  // ---- the walk ------------------------------------------------------------
  localparam logic [2:0] S_IDLE  = 3'd0;
  localparam logic [2:0] S_LOOK  = 3'd1;   // ask the bank for this form's row
  localparam logic [2:0] S_WAITA = 3'd2;   // its answer is registered one clock later
  localparam logic [2:0] S_PROJ  = 3'd3;   // offer the centre to client A
  localparam logic [2:0] S_WAITW = 3'd4;   // ...and wait for its w
  localparam logic [2:0] S_RAD   = 3'd5;   // the projected bound radius
  localparam logic [2:0] S_LOD   = 3'd6;   // the ladder itself
  localparam logic [2:0] S_EMIT  = 3'd7;   // offer the caster

  logic [2:0]        st_w;
  logic [15:0]       iid_q;
  logic [IIDW-1:0]   idx_q;
  logic [23:0]       form_q;
  logic signed [31:0] cx_q, cy_q, cz_q;
  logic               view_q;
  logic signed [31:0] bnd_q, mic_q, spl_q, gli_q;
  logic [30:0]        w_q;
  logic               behind_q;
  logic signed [31:0] rad_q;

  // The previous accepted job's instance, and whether there IS one this frame.
  logic [15:0] last_iid_q;
  logic        have_last_q;

  assign busy_o = (st_w != S_IDLE);

  // A job is this block's business when it names an instance that is not the
  // one the previous accepted job named. See the header: that is a property of
  // `zhao_geom_drawjob`'s FSM, not of a workload.
  wire new_inst_c = j_fire_i && (!have_last_q || (j_instance_id_i != last_iid_q));
  wire in_range_c = (j_instance_id_i < 16'(INSTANCES));

  // ---- the bank lookup -----------------------------------------------------
  assign q_valid_o = (st_w == S_LOOK);
  assign q_form_o  = form_q;

  // ---- the centre projection ----------------------------------------------
  assign pr_valid_o = (st_w == S_PROJ);
  assign pr_x_o     = cx_q;
  assign pr_y_o     = cy_q;
  assign pr_z_o     = cz_q;
  assign pr_view_o  = view_q;

  // ---- the projected bound radius -----------------------------------------
  logic               rr_req_valid_c, rr_req_ready_w;
  logic               rr_ans_valid_w, rr_ans_ok_w;
  logic signed [31:0] rr_radius_w;
  /* verilator lint_off UNUSEDSIGNAL */
  // The service's own tag is not used: this block has exactly one evaluation in
  // flight, so an answer can only belong to the request that started it. The
  // port is tied to zero rather than removed because it is the service's
  // interface, and a caller with several in flight will want it.
  logic [15:0] rr_tag_w;
  /* verilator lint_on UNUSEDSIGNAL */

  // The request is OFFERED once and taken down on the accept, rather than held
  // as a level for the whole evaluation. Holding it would work today -- the
  // service's `req_ready_o` is high only in its idle state -- but it makes this
  // block's correctness depend on a detail of the service's handshake instead
  // of on the handshake itself, and `rr_issued_q` costs one flip-flop.
  logic rr_issued_q;
  assign rr_req_valid_c = (st_w == S_RAD) && !rr_issued_q;

  zhao_geom_projradius #(
      .TAGW(16)
  ) u_radius (
      .clk  (clk),
      .rst_n(rst_n),

      .req_valid_i   (rr_req_valid_c),
      .req_ready_o   (rr_req_ready_w),
      .kx_i          (view_q ? kx1_i : kx0_i),
      .vw_i          (view_q ? vw1_i : vw0_i),
      .bound_radius_i(bnd_q),
      .w_i           (w_q),
      .behind_i      (behind_q),
      .tag_i         (iid_q),

      .ans_valid_o(rr_ans_valid_w),
      .ans_ready_i((st_w == S_RAD)),
      .radius_q8_o(rr_radius_w),
      .ans_ok_o   (rr_ans_ok_w),
      .tag_o      (rr_tag_w),

      .evaluations_o(rad_evaluations_o),
      .behind_o     (rad_behind_o),
      .bad_bound_o  (rad_bad_bound_o),
      .saturated_o  (rad_saturated_o)
  );

  // ---- the ladder ----------------------------------------------------------
  logic               lod_tick_c;
  logic               lod_ready_w, lod_valid_w;
  logic [ 1:0]        lod_rung_w, lod_raw_w;
  logic [15:0]        lod_hold_w;
  /* verilator lint_off UNUSEDSIGNAL */
  // `raw_o` is the rung BEFORE the stability law, which is evidence about the
  // hysteresis rather than a value anything acts on. It is read by the directed
  // test through the bench's own port and not by this block.
  logic [1:0] raw_unused_w;
  /* verilator lint_on UNUSEDSIGNAL */
  assign raw_unused_w = lod_raw_w;

  logic signed [31:0] thresh_c;
  assign thresh_c = view_q ? thresh1_i : thresh0_i;

  assign lod_tick_c = (st_w == S_LOD) && lod_ready_w && !lod_valid_w;

  zhao_geom_lod u_lod (
      .clk  (clk),
      .rst_n(rst_n),

      .tick_i(lod_tick_c),

      .proj_radius_q8_i(rad_q),
      .thresh_q8_i     (thresh_c),

      .bound_radius_i(bnd_q),
      .micro_error_i (mic_q),
      .splat_error_i (spl_q),
      .glint_error_i (gli_q),

      .rung_i(st_q[idx_q][17:16]),
      .hold_i(st_q[idx_q][15:0]),

      .rung_o (lod_rung_w),
      .hold_o (lod_hold_w),
      .raw_o  (lod_raw_w),
      .valid_o(lod_valid_w),
      .ready_o(lod_ready_w)
  );

  // ---- the caster ----------------------------------------------------------
  assign c_valid_o = (st_w == S_EMIT);

  integer i;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_w        <= S_IDLE;
      iid_q       <= 16'd0;
      idx_q       <= '0;
      form_q      <= 24'd0;
      cx_q        <= 32'sd0;
      cy_q        <= 32'sd0;
      cz_q        <= 32'sd0;
      view_q      <= 1'b0;
      bnd_q       <= 32'sd0;
      mic_q       <= 32'sd0;
      spl_q       <= 32'sd0;
      gli_q       <= 32'sd0;
      w_q         <= 31'd0;
      behind_q    <= 1'b0;
      rad_q       <= 32'sd0;
      rr_issued_q <= 1'b0;
      last_iid_q  <= 16'd0;
      have_last_q <= 1'b0;
      for (i = 0; i < INSTANCES; i = i + 1) st_q[i] <= 18'd0;  // {kMesh, hold 0}
      c_instance_id_o  <= 16'd0;
      c_x_o            <= 32'sd0;
      c_z_o            <= 32'sd0;
      c_radius_o       <= 32'sd0;
      c_rung_o         <= 2'd0;
      c_view_o         <= 1'b0;
      ticks_o          <= 32'd0;
      skipped_repeat_o <= 32'd0;
      bank_miss_o      <= 32'd0;
      no_radius_o      <= 32'd0;
      dropped_o        <= 32'd0;
      out_of_range_o   <= 32'd0;
      for (i = 0; i < 4; i = i + 1) rung_counts_o[i] <= 32'd0;
    end else begin
      // The frame boundary clears the run marker. It does NOT clear the ladder
      // state -- that is the whole point of holding it.
      if (frame_i) have_last_q <= 1'b0;

      // The radius request's own accept, tracked so the offer is taken down.
      if (rr_req_valid_c && rr_req_ready_w) rr_issued_q <= 1'b1;

      // Every observed job updates the run marker, whether or not it is taken:
      // the marker is "which instance the previous job named", not "which
      // instance was last evaluated". Conflating the two would make a DROPPED
      // job's instance eligible again on the very next meshlet.
      if (j_fire_i) begin
        last_iid_q  <= j_instance_id_i;
        have_last_q <= 1'b1;
        if (!new_inst_c) skipped_repeat_o <= skipped_repeat_o + 32'd1;
        else if (!in_range_c) out_of_range_o <= out_of_range_o + 32'd1;
        else if (st_w != S_IDLE) dropped_o <= dropped_o + 32'd1;
      end

      case (st_w)
        S_IDLE: begin
          if (new_inst_c && in_range_c) begin
            iid_q  <= j_instance_id_i;
            idx_q  <= j_instance_id_i[IIDW-1:0];
            form_q <= j_form_index_i;
            cx_q   <= j_cx_i;
            cy_q   <= j_cy_i;
            cz_q   <= j_cz_i;
            view_q <= j_view_i;
            st_w   <= S_LOOK;
          end
        end

        S_LOOK: begin
          // The bank accepts a query every cycle and answers one cycle later.
          st_w <= S_WAITA;
        end

        S_WAITA: begin
          if (a_valid_i) begin
            if (!a_hit_i) begin
              // A form with no ladder row. The ladder is NOT ticked: running it
              // against a zero bound radius would pick a rung
              // `zref::creature::lod_raw` never picks, and nothing downstream
              // could tell. The instance keeps the state it had.
              bank_miss_o <= bank_miss_o + 32'd1;
              st_w        <= S_IDLE;
            end else begin
              bnd_q <= a_bound_i;
              mic_q <= a_micro_i;
              spl_q <= a_splat_i;
              gli_q <= a_glint_i;
              st_w  <= S_PROJ;
            end
          end
        end

        S_PROJ: begin
          if (pr_ready_i) st_w <= S_WAITW;
        end

        S_WAITW: begin
          if (pr_ans_valid_i) begin
            w_q         <= pr_w_i;
            behind_q    <= pr_behind_i;
            rr_issued_q <= 1'b0;
            st_w        <= S_RAD;
          end
        end

        S_RAD: begin
          if (rr_ans_valid_w) begin
            if (!rr_ans_ok_w) begin
              // Behind the eye, or a bound radius the bank could not supply.
              // The reference SKIPS the creature entirely in this case, so the
              // ladder is not ticked and the state stands.
              no_radius_o <= no_radius_o + 32'd1;
              st_w        <= S_IDLE;
            end else begin
              rad_q <= rr_radius_w;
              st_w  <= S_LOD;
            end
          end
        end

        S_LOD: begin
          if (lod_valid_w) begin
            st_q[idx_q]              <= {lod_rung_w, lod_hold_w};
            ticks_o                  <= ticks_o + 32'd1;
            rung_counts_o[lod_rung_w] <= rung_counts_o[lod_rung_w] + 32'd1;
            c_instance_id_o          <= iid_q;
            c_x_o                    <= cx_q;
            c_z_o                    <= cz_q;
            c_radius_o               <= bnd_q;
            c_rung_o                 <= lod_rung_w;
            c_view_o                 <= view_q;
            st_w                     <= S_EMIT;
          end
        end

        default: begin  // S_EMIT
          if (c_ready_i) st_w <= S_IDLE;
        end
      endcase
    end
  end

endmodule : zhao_geom_lodstate

`default_nettype wire
