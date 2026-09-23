// tb_projshare.sv -- THE COMPOSED CLIENT-A CONTENTION BENCH, which this tree
// has never had.
//
// ENFORCED-BY: tests/common/projshare_contention.cpp:main
//
// ---------------------------------------------------------------------------
// WHY IT EXISTS
// ---------------------------------------------------------------------------
// Owner ruling R3 owes "a written schedule proof that geometry, particles and
// FORGE.SHADOW's instance-centre 1/w share client A's bandwidth within the
// frame at the guaranteed content tier".
// `design/contracts/FORGE.SHADOW.md` records why half of it could not be
// produced:
//
//   "The RATE half needs a bench holding `zhao_part_project` against the real
//    `zhao_proj_subsystem` with every arm saturated. `tb_part_project` drives
//    the block STANDALONE -- its `verilate()` sources are
//    `zhao_part_project.sv` and `zhao_part_record.sv`, no service and no core
//    -- so the composed multi-client throughput has never been measured."
//
// Verified still true at 220c67fd: `tests/CMakeLists.txt:15956-15962`. THIS IS
// THAT BENCH. It measures the arbitration, which is the only thing R3's
// question is about.
//
// ---------------------------------------------------------------------------
// WHY `zhao_project_service` AND NOT `zhao_proj_subsystem`
// ---------------------------------------------------------------------------
// The contract names the subsystem. The subsystem passes BOTH client arms
// STRAIGHT THROUGH to the service, unmodified and unregistered --
// `zhao_proj_subsystem.sv:203-234` connects `a_valid_i`/`a_ready_o`/`a_*` and
// `b_valid_i`/`b_ready_o`/`b_*` to `u_svc`'s identically named ports, and the
// only transformation anywhere is `b_payload_i({b_arena_i, b_index_i})`, a
// concatenation. The replay shell beside it touches the CORNER-REFERENCE path
// and never the request arms.
//
// So the arbitration measured here is bit-identical to the arbitration inside
// the composed console, and instantiating the shell would add fifty tie-offs
// that can only obscure it. THAT IS AN ARGUMENT, NOT A CONVENIENCE: if the
// shell ever registers a client arm, this bench is measuring the wrong thing
// and the citation above is where to check.
//
// ---------------------------------------------------------------------------
// WHAT IS MODELLED HERE AND WHY THAT IS HONEST
// ---------------------------------------------------------------------------
// ONE thing: PART.LADDER's loop. `zhao_part_project` hands a projected
// particle out on `lad_*` and takes its rung back on `rng_*`, and a slot is
// not freed until that round trip completes -- so an open ladder loop would
// wedge the particle arm at `SLOTS` in flight and the bench would measure a
// stall it invented. The model is a ONE-CLOCK skid, which is PART.LADDER's own
// declared shape (`zhao_part_project.sv:240-243`: "PART.LADDER is a one-deep
// skid"). Its VALUE (the rung it returns) is never read by anything this bench
// asserts; only its TIMING matters, and the timing is the block's own.
//
// Everything else is REAL RTL: the front multiplex's rotating priority, the
// service's A/B round-robin, and one `zhao_project_core` behind both.
//
// Conservative SystemVerilog subset only (charter section 2).
`default_nettype none

module tb_projshare #(
    // The console's values, named rather than defaulted, so a bench measuring
    // a different machine than the console runs is visible as a diff here.
    // `zhao_console_core.sv:7061` GEOM_PAY_A_W, `:19276` PART_PROJ_SLOTS_C.
    parameter int unsigned PAY_W = 17,
    parameter int unsigned SLOTS = 8,
    // Client B's rider inside the subsystem is {ARENA_W, INDEX_W} = 3 + 12.
    // `zhao_console_core.sv:7053-7060`.
    parameter int unsigned PAY_B_W = 15
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- the projector's configuration bus --------------------------------
    input  var logic        cfg_we_i,
    input  var logic        cfg_view_i,
    input  var logic [ 4:0] cfg_addr_i,
    input  var logic [31:0] cfg_data_i,
    input  var logic        en_i,

    // ---- the four request arms, driven by the bench -----------------------
    // ARM G -- geometry, OWNER_GEOM 2'd0
    input  var logic               g_valid_i,
    output var logic               g_ready_o,
    input  var logic signed [31:0] g_vx_i,
    input  var logic signed [31:0] g_vy_i,
    input  var logic signed [31:0] g_vz_i,
    input  var logic               g_view_i,
    input  var logic [PAY_W-1:0]   g_payload_i,

    // ARM P -- particles, OWNER_PART 2'd1. The RECORD's contents do not reach
    // the arbiter: `pv_valid_c = p_valid_i && !slot_full_c`
    // (`zhao_part_project.sv:623`). The bench drives a fixed plausible record
    // and asserts nothing about the arithmetic, which `part_project_directed`
    // already owns.
    input  var logic               p_valid_i,
    output var logic               p_ready_o,
    input  var logic [127:0]       p_record_i,

    // ARM F -- FORGE.PRIM, OWNER_FORGE 2'd2
    input  var logic               f_valid_i,
    output var logic               f_ready_o,
    input  var logic signed [31:0] f_vx_i,
    input  var logic signed [31:0] f_vy_i,
    input  var logic signed [31:0] f_vz_i,
    input  var logic               f_view_i,
    input  var logic [PAY_W-3:0]   f_slot_i,

    // ---- client B -- terrain, the OTHER service arm -----------------------
    input  var logic               b_valid_i,
    output var logic               b_ready_o,
    input  var logic signed [31:0] b_vx_i,
    input  var logic signed [31:0] b_vy_i,
    input  var logic signed [31:0] b_vz_i,
    input  var logic               b_view_i,
    input  var logic [PAY_B_W-1:0] b_payload_i,

    // ---- what the bench reads ---------------------------------------------
    // The service's own census. `a_grants_o` + `b_grants_o` is the total work
    // the one core did; `contended_o` is the clocks both asked and one waited.
    output var logic [31:0] svc_a_grants_o,
    output var logic [31:0] svc_b_grants_o,
    output var logic [31:0] svc_contended_o,
    // The front multiplex's own census, per arm.
    output var logic [31:0] mux_geom_grants_o,
    output var logic [31:0] mux_part_grants_o,
    output var logic [31:0] mux_contended_o,
    // The FOUR detectors this bench must show silent, because a composed
    // arbitration bench that fires one is measuring a routing fault and
    // reporting it as a rate.
    output var logic [31:0] geom_tag_collision_o,
    output var logic [31:0] owner_unroutable_o,
    output var logic [31:0] ladder_unexpected_o,
    output var logic [31:0] mat_refused_o,
    // The service's raw client-A ready, so the bench can time the front mux's
    // wait against the cycles client A was actually offered.
    output var logic        a_ready_o,
    output var logic        a_valid_o
);

  // ---- the front multiplex's request into client A ------------------------
  logic               pa_valid;
  logic               pa_ready;
  logic signed [31:0] pa_vx, pa_vy, pa_vz;
  logic               pa_view;
  logic [PAY_W-1:0]   pa_payload;

  // ---- client A's result, back into the front multiplex -------------------
  logic               sv_a_valid;
  logic signed [20:0] sv_a_x, sv_a_y;
  logic signed [31:0] sv_a_d;
  logic        [30:0] sv_a_w;
  logic               sv_a_behind;
  logic               sv_a_view;
  logic        [ 1:0] sv_a_profile;
  logic [PAY_W-1:0]   sv_a_payload;

  assign a_ready_o = pa_ready;
  assign a_valid_o = pa_valid;

  // ---- PART.LADDER's loop, modelled as the one-deep skid it is ------------
  // See the header. `lad_ready_i` is unconditional and `rng_valid_i` is the
  // accepted beat delayed one clock, which is the shortest legal round trip.
  logic       lad_valid, lad_ready;
  logic [2:0] lad_prev_rung;
  logic [3:0] lad_hold;
  // The governor floor the block hands PART.LADDER with the particle. It is
  // read by nothing here because this bench models the LADDER's TIMING and not
  // its POLICY -- `part_ladder_directed` owns the floor's law, and a second
  // opinion about it inside a contention bench would be a second
  // implementation of a settled rule.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [2:0] lad_gov_floor;
  /* verilator lint_on UNUSEDSIGNAL */
  logic       rng_valid, rng_ready;
  logic [2:0] rng_rung;
  logic [3:0] rng_hold;
  logic       rng_changed;

  assign lad_ready = 1'b1;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      rng_valid   <= 1'b0;
      rng_rung    <= 3'd0;
      rng_hold    <= 4'd0;
      rng_changed <= 1'b0;
    end else begin
      rng_valid   <= lad_valid && lad_ready;
      // The rung handed back is the one that was asked about, so the block's
      // own `ladder_unexpected_o` stays a real detector rather than being
      // tripped by a model that answers a question nobody asked.
      rng_rung    <= lad_prev_rung;
      rng_hold    <= lad_hold;
      rng_changed <= 1'b0;
    end
  end

  // ---- unread outputs, named rather than left on empty pins ---------------
  // Everything below is a real output of a real block whose CONSUMER is not
  // part of the arbitration question. Naming them is this tree's law: a port
  // connected to nothing reads as an oversight, a named net reads as a choice.
  /* verilator lint_off UNUSEDSIGNAL */
  logic               h_valid;
  logic signed [20:0] h_x, h_y;
  logic signed [31:0] h_d;
  logic        [30:0] h_w;
  logic               h_behind;
  logic [PAY_W-1:0]   h_payload;
  logic               rf_valid;
  logic signed [20:0] rf_x, rf_y;
  logic        [30:0] rf_w;
  logic               rf_behind;
  logic [PAY_W-3:0]   rf_slot;
  logic [15:0]        lad_size, lad_trail;
  logic               lad_narrow, lad_protected, lad_first;
  logic               q_in, q_changed;
  logic signed [20:0] q_x, q_y;
  logic signed [31:0] q_d;
  logic [ 7:0]        q_size, q_r, q_g, q_b;
  logic [15:0]        q_size16, q_src_id;
  logic [30:0]        q_w;
  logic [ 1:0]        q_profile;
  logic [ 2:0]        q_rung;
  logic [ 3:0]        q_hold_new;
  logic [31:0]        particles_projected, particles_behind;
  logic [31:0]        size_saturations, slot_pressure;
  logic               svc_busy;
  logic               b_res_valid, b_res_behind, b_res_view;
  logic signed [20:0] b_res_x, b_res_y;
  logic signed [31:0] b_res_d;
  logic        [30:0] b_res_w;
  logic        [ 1:0] b_res_profile;
  logic [PAY_B_W-1:0] b_res_payload;
  /* verilator lint_on UNUSEDSIGNAL */

  logic q_valid;

  // =========================================================================
  // THE FRONT MULTIPLEX -- REAL RTL
  // =========================================================================
  zhao_part_project #(
      .PAY_W(PAY_W),
      .SLOTS(SLOTS)
  ) u_part_project (
      .clk  (clk),
      .rst_n(rst_n),

      .cfg_base_radius_i(32'sd65536),
      .cfg_view_i       (1'b0),

      .p_valid_i    (p_valid_i),
      .p_ready_o    (p_ready_o),
      .p_record_i   (p_record_i),
      .p_trail_i    (16'd0),
      .p_narrow_i   (1'b0),
      .p_protected_i(1'b0),
      .p_gov_floor_i(3'd0),
      .p_prev_rung_i(3'd0),
      .p_hold_i     (4'd0),
      .p_first_i    (1'b0),
      .p_r_i        (8'd255),
      .p_g_i        (8'd255),
      .p_b_i        (8'd255),
      .p_src_id_i   (16'd0),

      .g_valid_i  (g_valid_i),
      .g_ready_o  (g_ready_o),
      .g_vx_i     (g_vx_i),
      .g_vy_i     (g_vy_i),
      .g_vz_i     (g_vz_i),
      .g_view_i   (g_view_i),
      .g_payload_i(g_payload_i),

      .f_valid_i(f_valid_i),
      .f_ready_o(f_ready_o),
      .f_vx_i   (f_vx_i),
      .f_vy_i   (f_vy_i),
      .f_vz_i   (f_vz_i),
      .f_view_i (f_view_i),
      .f_slot_i (f_slot_i),

      .a_valid_o  (pa_valid),
      .a_ready_i  (pa_ready),
      .a_vx_o     (pa_vx),
      .a_vy_o     (pa_vy),
      .a_vz_o     (pa_vz),
      .a_view_o   (pa_view),
      .a_payload_o(pa_payload),

      .a_valid_i  (sv_a_valid),
      .a_x_i      (sv_a_x),
      .a_y_i      (sv_a_y),
      .a_d_i      (sv_a_d),
      .a_w_i      (sv_a_w),
      .a_profile_i(sv_a_profile),
      .a_behind_i (sv_a_behind),
      .a_payload_i(sv_a_payload),

      .h_valid_o  (h_valid),
      .h_x_o      (h_x),
      .h_y_o      (h_y),
      .h_d_o      (h_d),
      .h_w_o      (h_w),
      .h_behind_o (h_behind),
      .h_payload_o(h_payload),

      .rf_valid_o (rf_valid),
      .rf_x_o     (rf_x),
      .rf_y_o     (rf_y),
      .rf_w_o     (rf_w),
      .rf_behind_o(rf_behind),
      .rf_slot_o  (rf_slot),

      .lad_valid_o    (lad_valid),
      .lad_ready_i    (lad_ready),
      .lad_size_o     (lad_size),
      .lad_trail_o    (lad_trail),
      .lad_narrow_o   (lad_narrow),
      .lad_protected_o(lad_protected),
      .lad_gov_floor_o(lad_gov_floor),
      .lad_prev_rung_o(lad_prev_rung),
      .lad_hold_o     (lad_hold),
      .lad_first_o    (lad_first),
      .rng_valid_i    (rng_valid),
      .rng_ready_o    (rng_ready),
      .rng_rung_i     (rng_rung),
      .rng_hold_i     (rng_hold),
      .rng_changed_i  (rng_changed),

      .q_valid_o   (q_valid),
      .q_ready_i   (1'b1),
      .q_in_o      (q_in),
      .q_x_o       (q_x),
      .q_y_o       (q_y),
      .q_d_o       (q_d),
      .q_size_o    (q_size),
      .q_size16_o  (q_size16),
      .q_r_o       (q_r),
      .q_g_o       (q_g),
      .q_b_o       (q_b),
      .q_src_id_o  (q_src_id),
      .q_w_o       (q_w),
      .q_profile_o (q_profile),
      .q_rung_o    (q_rung),
      .q_hold_new_o(q_hold_new),
      .q_changed_o (q_changed),

      .particles_projected_o(particles_projected),
      .particles_behind_o   (particles_behind),
      .geom_grants_o        (mux_geom_grants_o),
      .part_grants_o        (mux_part_grants_o),
      .contended_o          (mux_contended_o),
      .size_saturations_o   (size_saturations),
      .slot_pressure_o      (slot_pressure),
      .geom_tag_collision_o (geom_tag_collision_o),
      .owner_unroutable_o   (owner_unroutable_o),
      .ladder_unexpected_o  (ladder_unexpected_o)
  );

  // =========================================================================
  // THE ONE SHARED PROJECTOR -- REAL RTL
  // =========================================================================
  zhao_project_service #(
      .PAYLOAD_A_W  (PAY_W),
      .PAYLOAD_B_W  (PAY_B_W),
      .ROWS_PER_PASS(3),
      .MATW         (32)
  ) u_svc (
      .clk  (clk),
      .rst_n(rst_n),

      .cfg_we_i  (cfg_we_i),
      .cfg_view_i(cfg_view_i),
      .cfg_addr_i(cfg_addr_i),
      .cfg_data_i(cfg_data_i),
      .en_i      (en_i),

      .a_valid_i  (pa_valid),
      .a_ready_o  (pa_ready),
      .a_vx_i     (pa_vx),
      .a_vy_i     (pa_vy),
      .a_vz_i     (pa_vz),
      .a_view_i   (pa_view),
      .a_payload_i(pa_payload),
      .a_valid_o  (sv_a_valid),
      .a_x_o      (sv_a_x),
      .a_y_o      (sv_a_y),
      .a_d_o      (sv_a_d),
      .a_w_o      (sv_a_w),
      .a_behind_o (sv_a_behind),
      .a_view_o   (sv_a_view),
      .a_profile_o(sv_a_profile),
      .a_payload_o(sv_a_payload),

      .b_valid_i  (b_valid_i),
      .b_ready_o  (b_ready_o),
      .b_vx_i     (b_vx_i),
      .b_vy_i     (b_vy_i),
      .b_vz_i     (b_vz_i),
      .b_view_i   (b_view_i),
      .b_payload_i(b_payload_i),
      .b_valid_o  (b_res_valid),
      .b_x_o      (b_res_x),
      .b_y_o      (b_res_y),
      .b_d_o      (b_res_d),
      .b_w_o      (b_res_w),
      .b_behind_o (b_res_behind),
      .b_view_o   (b_res_view),
      .b_profile_o(b_res_profile),
      .b_payload_o(b_res_payload),

      .busy_o(svc_busy),

      .a_grants_o  (svc_a_grants_o),
      .b_grants_o  (svc_b_grants_o),
      .contended_o (svc_contended_o),
      .mat_refused_o(mat_refused_o)
  );

  // `q_valid` and `rng_ready` are read by nothing this bench asserts; they are
  // named so the ports are not empty and so the next reader can see that the
  // particle egress is unconditionally drained, which is what keeps the slot
  // store from becoming the thing being measured.
  /* verilator lint_off UNUSEDSIGNAL */
  wire _unused_ok = &{1'b0, q_valid, rng_ready, sv_a_view, 1'b0};
  /* verilator lint_on UNUSEDSIGNAL */

endmodule : tb_projshare

`default_nettype wire
