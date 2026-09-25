// tb_part_terrain_chain.sv -- the I6 chain, three REAL blocks and one fixture:
//
//     [compose-cache read ports, fixture]
//            ^ c_lat_* / c_cs_*
//     TERRAIN.HEIGHTTAP  <-- tap_req/tap_rsp -->  PART.TERRAIN_TAP  --t_*-->  PART.COLLIDE
//                                                  ^ p_* (the bench, as PART.UPDATE)
//
// The fixture is `zhao_terrain_compcache_front`'s two READ ports and nothing
// else -- the same one-cycle registered read `terrain_heighttap_directed.cpp`
// models -- driven from C++ so the lattice can be anything a case needs.
// TERRAIN.TESS, the owner of those ports, is held quiet: its starvation of the
// tap is `terrain_heighttap_directed.cpp` case 8's subject, not this bench's.
//
// Everything the three blocks compose is WIRED HERE EXACTLY AS
// `zhao_console_core` wires it, so this is the composed seam under a
// microscope rather than a second arrangement of it.
`default_nettype none

module tb_part_terrain_chain (
    input  var logic               clk,
    input  var logic               rst_n,

    input  var logic signed [7:0]  pitch_log2_i,
    input  var logic signed [31:0] origin_x_i,
    input  var logic signed [31:0] origin_y_i,
    input  var logic signed [31:0] origin_z_i,
    input  var logic               inval_i,

    // PART.UPDATE's side
    input  var logic               p_valid_i,
    output var logic               p_ready_o,
    input  var logic [127:0]       p_record_i,
    input  var logic [3:0]         p_events_i,
    input  var logic [15:0]        p_side_i,
    output var logic [15:0]        q_side_o,     // the sideband, on the collider's take

    // the compose cache's read ports (fixture)
    output var logic               c_lat_req_o,
    output var logic        [ 5:0] c_lat_vi_o,
    output var logic        [ 5:0] c_lat_vj_o,
    output var logic               c_lat_surface_o,
    input  var logic signed [31:0] c_lat_h_i,
    input  var logic signed [31:0] c_lat_wx_i,
    input  var logic signed [31:0] c_lat_wz_i,
    output var logic               c_cs_req_o,
    output var logic        [ 4:0] c_cs_ci_o,
    output var logic        [ 4:0] c_cs_cj_o,
    input  var logic        [ 1:0] c_cs_substance_i,

    // PART.COLLIDE's descriptor (the bench is PART.TABLE)
    output wire             [ 6:0] d_index_o,
    input  var logic        [ 2:0] d_response_i,

    // the sample, observed on the seam between the two particle blocks
    output var logic               t_valid_o,
    output var logic signed [17:0] t_height_o,
    output var logic signed [11:0] t_nx_o,
    output var logic signed [11:0] t_ny_o,
    output var logic signed [11:0] t_nz_o,
    output var logic               t_take_o,   // PART.COLLIDE took the beat this cycle

    // PART.COLLIDE's output
    output var logic               c_valid_o,
    input  var logic               c_ready_i,
    output var logic [127:0]       c_record_o,
    output var logic               c_contact_o,

    // censuses
    output var logic [31:0] particles_o,
    output var logic [31:0] samples_ground_o,
    output var logic [31:0] samples_no_ground_o,
    output var logic [31:0] samples_missed_o,
    output var logic [31:0] cell_mismatch_o,
    output var logic [31:0] out_of_range_o,
    output var logic [31:0] height_sats_o,
    output var logic [31:0] fills_issued_o,
    output var logic [31:0] fills_landed_o,
    output var logic [31:0] fills_discarded_o,
    output var logic [31:0] invalidations_o,
    output var logic [31:0] contacts_terrain_o,
    output var logic [31:0] terrain_sample_unavailable_o,
    output var logic [31:0] taps_answered_o,
    output var logic [31:0] taps_void_o,
    output var logic [31:0] taps_off_patch_o
);

  // ---- the tap -------------------------------------------------------------
  logic               tq_valid, tq_ready, tq_surface;
  logic signed [31:0] tq_x, tq_z;
  logic               tr_valid, tr_no_ground;
  logic signed [31:0] tr_h00, tr_h10, tr_h01, tr_h11, tr_wx00, tr_wz00;
  logic        [ 4:0] tr_sh;
  logic signed [31:0] tr_na_x, tr_na_y, tr_na_z, tr_nb_x, tr_nb_y, tr_nb_z;
  /* verilator lint_off UNUSEDSIGNAL */
  logic signed [31:0] tr_height, tr_nx, tr_ny, tr_nz;
  logic signed [31:0] o_h, o_wx, o_wz;
  logic        [ 1:0] o_sub;
  logic [31:0]        tp_fault_pl, tp_fault_pb, tp_ovf, tp_stall, tp_nsat;
  /* verilator lint_on UNUSEDSIGNAL */

  zhao_terrain_heighttap u_tap (
      .clk(clk), .rst_n(rst_n),
      .pitch_log2_i(pitch_log2_i),
      .req_valid_i(tq_valid), .req_ready_o(tq_ready),
      .req_x_i(tq_x), .req_z_i(tq_z), .req_surface_i(tq_surface),
      .rsp_valid_o(tr_valid), .rsp_height_o(tr_height), .rsp_no_ground_o(tr_no_ground),
      .rsp_nx_o(tr_nx), .rsp_ny_o(tr_ny), .rsp_nz_o(tr_nz),
      .rsp_h00_o(tr_h00), .rsp_h10_o(tr_h10), .rsp_h01_o(tr_h01), .rsp_h11_o(tr_h11),
      .rsp_wx00_o(tr_wx00), .rsp_wz00_o(tr_wz00), .rsp_sh_o(tr_sh),
      .rsp_na_x_o(tr_na_x), .rsp_na_y_o(tr_na_y), .rsp_na_z_o(tr_na_z),
      .rsp_nb_x_o(tr_nb_x), .rsp_nb_y_o(tr_nb_y), .rsp_nb_z_o(tr_nb_z),
      // TERRAIN.TESS is quiet in this bench.
      .o_lat_req_i(1'b0), .o_lat_vi_i(6'd0), .o_lat_vj_i(6'd0), .o_lat_surface_i(1'b0),
      .o_lat_h_o(o_h), .o_lat_wx_o(o_wx), .o_lat_wz_o(o_wz),
      .o_cs_req_i(1'b0), .o_cs_ci_i(5'd0), .o_cs_cj_i(5'd0), .o_cs_substance_o(o_sub),
      .c_lat_req_o(c_lat_req_o), .c_lat_vi_o(c_lat_vi_o), .c_lat_vj_o(c_lat_vj_o),
      .c_lat_surface_o(c_lat_surface_o),
      .c_lat_h_i(c_lat_h_i), .c_lat_wx_i(c_lat_wx_i), .c_lat_wz_i(c_lat_wz_i),
      .c_lat_vel_i        (16'sd0),
      .c_lat_vel_present_i(1'b0),
      .rsp_v00_o          (),
      .rsp_v10_o          (),
      .rsp_v01_o          (),
      .rsp_v11_o          (),
      .rsp_vel_present_o  (),
      .c_cs_req_o(c_cs_req_o), .c_cs_ci_o(c_cs_ci_o), .c_cs_cj_o(c_cs_cj_o),
      .c_cs_substance_i(c_cs_substance_i),
      .taps_answered_o(taps_answered_o), .taps_void_o(taps_void_o),
      .taps_off_patch_o(taps_off_patch_o), .place_mismatch_o(tp_fault_pl),
      .pitch_bad_o(tp_fault_pb), .interp_overflow_o(tp_ovf),
      .tap_stall_clocks_o(tp_stall), .normal_sats_o(tp_nsat)
  );

  // ---- the sampler -----------------------------------------------------------
  logic         s_valid, s_ready;
  logic [127:0] s_record;
  logic [3:0]   s_events;

  zhao_part_terrain_tap #(.SIDE_W(16)) u_ptt (
      .clk(clk), .rst_n(rst_n),
      .origin_x_i(origin_x_i), .origin_y_i(origin_y_i), .origin_z_i(origin_z_i),
      .pitch_log2_i(pitch_log2_i), .inval_i(inval_i),
      .p_valid_i(p_valid_i), .p_ready_o(p_ready_o),
      .p_record_i(p_record_i), .p_events_i(p_events_i), .p_side_i(p_side_i),
      .q_valid_o(s_valid), .q_ready_i(s_ready), .q_record_o(s_record), .q_events_o(s_events),
      .q_side_o(q_side_o),
      .t_valid_o(t_valid_o), .t_height_o(t_height_o),
      .t_nx_o(t_nx_o), .t_ny_o(t_ny_o), .t_nz_o(t_nz_o),
      .tap_req_valid_o(tq_valid), .tap_req_ready_i(tq_ready),
      .tap_req_x_o(tq_x), .tap_req_z_o(tq_z), .tap_req_surface_o(tq_surface),
      .tap_rsp_valid_i(tr_valid), .tap_rsp_no_ground_i(tr_no_ground),
      .tap_rsp_h00_i(tr_h00), .tap_rsp_h10_i(tr_h10), .tap_rsp_h01_i(tr_h01),
      .tap_rsp_h11_i(tr_h11), .tap_rsp_wx00_i(tr_wx00), .tap_rsp_wz00_i(tr_wz00),
      .tap_rsp_sh_i(tr_sh),
      .tap_rsp_na_x_i(tr_na_x), .tap_rsp_na_y_i(tr_na_y), .tap_rsp_na_z_i(tr_na_z),
      .tap_rsp_nb_x_i(tr_nb_x), .tap_rsp_nb_y_i(tr_nb_y), .tap_rsp_nb_z_i(tr_nb_z),
      .particles_o(particles_o), .samples_ground_o(samples_ground_o),
      .samples_no_ground_o(samples_no_ground_o), .samples_missed_o(samples_missed_o),
      .cell_mismatch_o(cell_mismatch_o), .out_of_range_o(out_of_range_o),
      .height_sats_o(height_sats_o), .fills_issued_o(fills_issued_o),
      .fills_landed_o(fills_landed_o), .fills_discarded_o(fills_discarded_o),
      .invalidations_o(invalidations_o)
  );

  assign t_take_o = s_valid && s_ready;

  // ---- the collider ------------------------------------------------------------
  /* verilator lint_off UNUSEDSIGNAL */
  logic         c_alive, c_refused;
  logic [2:0]   c_response;
  logic [3:0]   c_events;
  logic [127:0] c_spawn;
  logic [31:0]  k_ign, k_die, k_stk, k_sld, k_bnc, k_pln, k_ins, k_ref, k_evt, k_clamp;
  /* verilator lint_on UNUSEDSIGNAL */

  zhao_part_collide u_collide (
      .clk(clk), .rst_n(rst_n),
      .p_valid_i(s_valid), .p_ready_o(s_ready),
      .p_record_i(s_record), .p_events_i(s_events),
      .d_index_o(d_index_o), .d_response_i(d_response_i),
      .d_restitution_i(16'sd0), .d_friction_i(16'sd0), .d_damping_i(16'sd0),
      .t_valid_i(t_valid_o), .t_height_i(t_height_o),
      .t_nx_i(t_nx_o), .t_ny_i(t_ny_o), .t_nz_i(t_nz_o),
      .pl_en_i(1'b0), .pl_nx_i(12'sd0), .pl_ny_i(12'sd0), .pl_nz_i(12'sd0), .pl_c_i(32'sd0),
      .c_valid_o(c_valid_o), .c_ready_i(c_ready_i), .c_record_o(c_record_o),
      .c_alive_o(c_alive), .c_contact_o(c_contact_o), .c_response_o(c_response),
      .c_refused_o(c_refused), .c_events_o(c_events), .c_spawn_record_o(c_spawn),
      .contacts_ignore_o(k_ign), .contacts_die_o(k_die), .contacts_stick_o(k_stk),
      .contacts_slide_o(k_sld), .contacts_bounce_o(k_bnc),
      .contacts_terrain_o(contacts_terrain_o), .contacts_plane_o(k_pln),
      .already_inside_at_entry_o(k_ins),
      .terrain_sample_unavailable_o(terrain_sample_unavailable_o),
      .response_refused_o(k_ref), .collision_events_o(k_evt), .field_clamps_o(k_clamp)
  );

endmodule : tb_part_terrain_chain

`default_nettype wire
