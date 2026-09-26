// zhao_raster_tile_pipe_v2.sv -- Packet-D attribute/texture raster composition.
//
// One accepted binner record starts EDGEWALK and exactly SIX current-oracle
// attribute-gradient lanes.  Coverage rows are captured once and delivered to
// every lane through an explicit per-lane mask.  Joined {invw,U/W,V/W} values
// form the typed 490-bit Packet-C request, and the joined {R,G,B} values become
// the continuation tail's per-fragment `vertex_rgb`; no Packet-B field is
// reconstructed by a numeric slice after the frozen 1,877-bit metadata boundary
// is unpacked.
//
// IT HAD THREE LANES UNTIL 2026-09-21. Owner decision R234 D1 ((owner,
// explicit)) bought the other three, reconnecting the lit per-vertex colour
// that `zhao_light_stream` computes and `zhao_geom_attrpack` had been dropping.
// THIS FILE IS WHERE THE DECISION'S PRICE LIVES: ~1,420 ALM and +24 DSP, three
// whole `zhao_raster_attrgrad_v2` lanes, because a plane must be evaluated per
// PIXEL and therefore cannot be time-multiplexed the way GEOM.ATTRPACK's shared
// setup core is.
//
// Recoverable local attribute/profile faults and Packet-C sequence faults are
// terminating for the current frame.  Producers and already-admitted owners are
// drained, while every unadmitted skid entry is synchronously popped into a sink
// and counted.  The skid reset remains exactly rst_n.
//
// AUTHORITY: reports/PACKET-D-ATTRIBUTE-RASTER-ABI-20260914.md
//            reports/SHELL-TEXTURE-V3-COMPOSITION-ARCHITECTURE-20260913.md
`default_nettype none

// Committed inverse controls override one expression each before this exact RTL
// is compiled.  Production definitions preserve all coordinates/quiet terms and
// make abort a synchronous consumer of the skid head.
`ifndef ZHAO_PACKET_D_LANE1_COL
`define ZHAO_PACKET_D_LANE1_COL(col) (col)
`endif
`ifndef ZHAO_PACKET_D_PIPE_V3_QUIET
`define ZHAO_PACKET_D_PIPE_V3_QUIET(quiet) (quiet)
`endif
`ifndef ZHAO_PACKET_D_SKID_DN_READY
`define ZHAO_PACKET_D_SKID_DN_READY(aborting, stage_ready) \
    ((aborting) ? 1'b1 : (stage_ready))
`endif

module zhao_raster_tile_pipe_v2 #(
    parameter bit ATTR_DSP3 = 1'b0,
    parameter bit BILERP_DSP2 = 1'b0
) (
    input  logic clk,
    input  logic rst_n,

    // One binner drain job.  Geometry/source/first-last are the old job fields;
    // all Packet-D attributes and flat material state are in job_meta_i.
    input  logic                job_valid_i,
    output logic                job_ready_o,
    input  logic signed [20:0]  job_ax_i,
    input  logic signed [20:0]  job_ay_i,
    input  logic signed [20:0]  job_bx_i,
    input  logic signed [20:0]  job_by_i,
    input  logic signed [20:0]  job_cx_i,
    input  logic signed [20:0]  job_cy_i,
    input  logic                job_first_i,
    input  logic                job_last_i,
    input  logic signed [11:0]  job_tile_x_i,
    input  logic signed [11:0]  job_tile_y_i,
    input  logic         [15:0] job_tile_index_i,
    input  logic         [15:0] job_src_id_i,
    input  logic       [1876:0] job_meta_i,

    // THE PROFILE VERDICTS, ALREADY DECIDED. bit 0 aux bad, bit 1 area bad.
    //
    // These used to be reduced here, off `job_meta_i`, on the edge the binner's
    // metadata bank delivers it: a 225-bit OR and a 47-bit zero-compare, which
    // `@packet-h-satstage` measured as 4.85 ns of an 11.37 ns path through
    // `local_fault_pulse_o` into the attribute walker's queue enable. The
    // binner now decides both when it WRITES the job and carries them in the
    // bank's pad bits, which were already allocated and already read out.
    //
    // Same edge, same word, so the abort comment further down still holds --
    // the verdict is not a cycle late, it simply stopped being computed twice.
    // The original expressions survive below as a simulation-only equivalence
    // check, so the two statements of the Packet-D layout cannot drift apart.
    input  logic           [1:0] job_profile_bad_i,
    input  logic         [63:0] frame_clear_word_i,

    // Recoverable frame-fault clear.  Ready is exposed only at complete local,
    // Packet-C, fragment and resolve quiet.
    input  logic                frame_fault_clear_valid_i,
    output logic                frame_fault_clear_ready_o,
    output logic                frame_fault_o,
    output logic                lifetime_structural_fault_o,

    // Packet-B sealed binding loader.
    input  logic                cfg_valid_i,
    output logic                cfg_ready_o,
    input  logic          [1:0] cfg_op_i,
    input  logic          [7:0] cfg_page_generation_i,
    input  logic          [7:0] cfg_selector_i,
    input  logic         [74:0] cfg_row_i,
    input  logic         [31:0] cfg_crc32_i,
    output logic                cfg_rsp_valid_o,
    input  logic                cfg_rsp_ready_i,
    output logic          [1:0] cfg_rsp_op_o,
    output logic          [3:0] cfg_rsp_status_o,
    output logic          [7:0] cfg_rsp_page_generation_o,
    output logic          [7:0] active_page_generation_o,

    // External texture-cache fill model/port.
    output logic                fill_req_valid_o,
    input  logic                fill_req_ready_i,
    output logic         [31:0] fill_req_addr_o,
    input  logic                fill_data_valid_i,
    input  logic         [15:0] fill_data_i,
    input  logic                fill_refused_i,

    // Palette programming.
    input  logic                pal_load_valid_i,
    output logic                pal_load_ready_o,
    input  logic          [1:0] pal_load_op_i,
    input  logic          [1:0] pal_load_slot_i,
    input  logic          [7:0] pal_load_gen_i,
    input  logic          [7:0] pal_load_idx_i,
    input  logic         [15:0] pal_load_rgb565_i,
    input  logic                pal_load_crc_ok_i,

    // TERRAIN.NORMALMAP's config and tile-upload write port, carried unchanged
    // to `zhao_raster_texture_stage_v3` and thence to the island. The DETAIL
    // DECLARATION is not here: it rides the candidate as `detail_required`.
    input  logic                dtl_we_i,
    input  logic                dtl_sel_i,
    input  logic         [12:0] dtl_addr_i,
    input  logic         [31:0] dtl_data_i,
    output logic         [31:0] cnt_detail_fragments_o,
    output logic         [31:0] cnt_detail_zeroed_o,
    output logic         [31:0] cnt_detail_railed_o,
    output logic         [31:0] cnt_detail_cold_o,
    output logic         [31:0] cnt_detail_published_o,
    output logic         [31:0] cnt_detail_applied_o,
    output logic         [31:0] err_detail_lost_o,
    output logic                dtl_table_ready_o,

    // Packet-B Surface Sheet port.  Packet-D's selected profile never issues it.
    output logic                sheet_req_valid_o,
    input  logic                sheet_req_ready_i,
    output logic          [1:0] sheet_req_op_o,
    output logic         [31:0] sheet_req_handle_o,
    output logic         [11:0] sheet_req_texel_o,
    output logic         [15:0] sheet_req_src_id_o,
    input  logic                pg_valid_i,
    output logic                pg_ready_o,
    input  logic          [1:0] pg_op_i,
    input  logic          [1:0] pg_status_i,
    input  logic          [7:0] pg_tag_i,
    input  logic          [7:0] pg_strength_i,
    input  logic         [15:0] pg_src_id_i,

    // Resolved framebuffer stream.
    output logic                fb_valid_o,
    input  logic                fb_ready_i,
    output logic         [15:0] fb_rgb565_o,
    output logic          [7:0] fb_tag_o,
    output logic          [7:0] fb_addr_o,
    output logic signed [11:0]  fb_x_o,
    output logic signed [11:0]  fb_y_o,
    output logic                fb_last_o,
    output logic         [15:0] fb_src_id_o,

    // Per-tile completion and retained raster observability.
    output logic         [31:0] tile_crc_o,
    output logic         [15:0] tile_crc_index_o,
    output logic                tile_done_o,
    output logic          [8:0] tile_cov_count_o,
    output logic                tile_degenerate_o,
    output logic                front_bank_o,
    output logic         [31:0] tilestore_references_o,
    output logic         [31:0] resolved_tiles_o,
    output logic         [31:0] early_z_rejects_o,
    output logic         [31:0] early_z_covered_o,
    output logic         [31:0] fragment_covered_o,
    output logic         [31:0] blended_fragments_o,
    output logic          [7:0] bin_mask_o,
    output logic         [23:0] z_floor_o,
    output logic                fragment_error_o,

    // Packet-D structural status and exact terminal accounting.
    output logic                quiet_o,
    output logic                raster_abort_o,
    output logic                local_attribute_abort_o,
    output logic                local_fault_pulse_o,
    output logic         [31:0] local_fault_count_o,
    output logic         [31:0] coordinate_fault_count_o,
    output logic         [31:0] range_fault_count_o,
    output logic         [31:0] aux_profile_fault_count_o,
    output logic         [31:0] candidate_cancel_count_o,
    output logic         [31:0] local_drop_count_o,
    output logic         [31:0] jobs_started_o,
    output logic         [31:0] jobs_sunk_o,

    // Packet-C sequence identity/accounting.
    output logic                sequence_abort_o,
    output logic                sequence_mismatch_o,
    output logic         [31:0] sequence_drop_count_o,
    output logic         [31:0] admission_sequence_o,
    output logic         [31:0] expected_sequence_o,
    output logic         [31:0] returned_sequence_o,
    output logic                packet_c_cand_fire_o,
    output logic                packet_c_fragment_fire_o,
    output logic                packet_c_drop_fire_o,

    // Selected Packet-B counters retained at the composition boundary.
    output logic         [31:0] texture_fragments_o,
    output logic         [31:0] texture_cache_hits_o,
    output logic         [31:0] texture_cache_misses_o,
    output logic         [31:0] texture_palette_lookups_o,
    output logic         [31:0] texture_plan_accepted_o,
    output logic         [31:0] texture_dispatch_accepted_o,
    output logic         [31:0] texture_combine_refused_o,
    // R9 / entry I49: the TMU samples zhao_texture_v3own actually PUBLISHED into
    // a fragment (its ev_texture_samples_o, gated on the commit's generation
    // check). It was sunk here as unused_cnt_texture_samples while every level
    // above it had no way to ask whether the island had sampled anything at
    // all. A counter nobody wired is not evidence about the thing it watches.
    output logic         [31:0] texture_samples_o,

    // Focused structural probes used by the committed Packet-D gate.
    output logic                coverage_hold_valid_o,
    output logic          [5:0] coverage_delivered_mask_o,
    output logic          [7:0] start_delivered_mask_o,
    output logic          [5:0] attribute_idle_o,
    output logic                earlyz_hold_valid_o,
    output logic          [1:0] skid_level_o,
    output logic                stage_candidate_valid_o,
    output logic       [490:0] stage_candidate_data_o,
    output logic                stage_fragment_valid_o,
    output logic         [7:0] stage_fragment_addr_o,
    output logic        [23:0] stage_fragment_depth_o,
    output logic        [31:0] stage_fragment_state_o,
    output logic        [15:0] stage_fragment_src_id_o,
    output logic        [23:0] stage_fragment_texel_rgb_o,
    output logic         [7:0] stage_fragment_texel_a_o,
    output logic         [7:0] stage_fragment_texel_idx_o,
    output logic         [7:0] stage_fragment_status_o,
    output logic                texture_quiet_o,
    output logic                fragment_idle_o
`ifdef ZHAO_PACKET_D_TEST_HOOKS
    , input logic         [7:0] test_start_enable_i
    , input logic         [5:0] test_attr_cov_enable_i
    , input logic               test_stage_admit_enable_i
`endif
);
  import zhao_render_texture_pkg::*;

  localparam logic [1:0] RS_IDLE   = 2'd0;
  localparam logic [1:0] RS_START  = 2'd1;
  localparam logic [1:0] RS_ACTIVE = 2'd2;
  localparam logic [1:0] RS_SWAP   = 2'd3;

  // The terminal state reuses RS_IDLE plus the sticky abort level.  No producer
  // is reset and jobs offered by a draining binner are accepted into the sink.
  logic [1:0] rs_state_q;

  // -------------------------------------------------------------------------
  // THE LANE COUNT AND THE DESTINATION MAP, as named constants rather than the
  // literals that used to be scattered through this file. Six lanes, eight
  // start destinations (EDGEWALK, six attribute lanes, the tilestore clear).
  //
  // `ATTR_LANES` is the one number to change; every array bound, loop limit,
  // mask width, reset constant and elaboration check below reads it. When this
  // block carried the three-lane literals, adding the Gouraud lanes meant
  // finding nineteen separate `3`s and two `5`s.
  localparam int unsigned ATTR_LANES  = 6;
  localparam int unsigned START_CLEAR = ATTR_LANES + 1;  // the tilestore clear
  localparam int unsigned START_N     = ATTR_LANES + 2;  // + EDGEWALK at 0

  // LANE -> ATTRIBUTE. 0 invw24, 1 u/w, 2 v/w, 3 R, 4 G, 5 B, matching
  // `zhao_geom_attrpack`'s own lane order and therefore the plane order
  // `zhao_geom_bin_pipe_v2` concatenates. Lanes 3..5 are the Gouraud lanes.
  localparam int unsigned LANE_INVW = 0;
  localparam int unsigned LANE_UOW  = 1;
  localparam int unsigned LANE_VOW  = 2;
  localparam int unsigned LANE_R    = 3;
  localparam int unsigned LANE_G    = 4;
  localparam int unsigned LANE_B    = 5;

  // -------------------------------------------------------------------------
  // Frozen 1,877-bit metadata boundary.  These are the only raw Packet-D job
  // slices in this module; all downstream packet handling is through types.
  localparam int unsigned META_PLANE_W  = 240;
  localparam int unsigned META_PLANE_LO = 437;
  localparam int unsigned META_W = META_PLANE_LO + ATTR_LANES * META_PLANE_W;

  initial begin : p_packet_d_contract
    if (($bits(job_meta_i) != META_W) ||
        ($bits(zhao_texture_v3_request_v2_t) != 362) ||
        ($bits(zhao_raster_continuation_v2_t) != 128) ||
        ($bits(zhao_raster_earlyz_payload_v2_t) != 410) ||
        ($bits(zhao_raster_pretex_v2_t) != 490))
      $fatal(1, "zhao_raster_tile_pipe_v2: Packet-D width contract changed");
  end

  // The 1877 is asserted against the composed number as well as against the
  // arithmetic, so a lane count and a port width cannot drift apart silently.
  initial begin : p_packet_d_meta_width
    if (META_W != 1877)
      $fatal(1, "zhao_raster_tile_pipe_v2: METAW is not the ratified 1877");
  end

  logic        [297:0] flat_request_q;
  logic         [47:0] continuation_tail_bits_q;
  logic         [31:0] fragment_state_q;
  logic         [46:0] area2_q;
  logic signed  [11:0] min_x_q;
  logic signed  [95:0] plane_n0_q   [0:ATTR_LANES-1];
  logic signed  [71:0] plane_dndx_q [0:ATTR_LANES-1];
  logic signed  [71:0] plane_dndy_q [0:ATTR_LANES-1];

  // One and only one raw unpack of the frozen metadata ABI.  Pre-admission
  // profile checks and accepted registers both consume these named values.
  logic        [297:0] incoming_flat_request_w;
  logic         [47:0] incoming_continuation_tail_w;
  logic         [31:0] incoming_fragment_state_w;
  logic         [46:0] incoming_area2_w;
  logic signed  [11:0] incoming_min_x_w;
  logic signed  [95:0] incoming_plane_n0_w   [0:ATTR_LANES-1];
  logic signed  [71:0] incoming_plane_dndx_w [0:ATTR_LANES-1];
  logic signed  [71:0] incoming_plane_dndy_w [0:ATTR_LANES-1];

  assign incoming_flat_request_w = job_meta_i[297:0];
  assign incoming_continuation_tail_w = job_meta_i[345:298];
  assign incoming_fragment_state_w = job_meta_i[377:346];
  assign incoming_area2_w = job_meta_i[424:378];
  assign incoming_min_x_w = $signed(job_meta_i[436:425]);
  // Plane k is dndy, dndx, n0 ascending from `META_PLANE_LO + 240*k`. Written
  // as a loop rather than eighteen hand-written ranges: the three-lane version
  // listed nine, and three more sets of hand-arithmetic is exactly where an
  // off-by-72 hides. Lane 0's slices are bit-identical to the ranges this
  // replaced -- [508:437], [580:509], [676:581].
  genvar gp;
  generate
    for (gp = 0; gp < ATTR_LANES; gp = gp + 1) begin : g_meta_plane
      assign incoming_plane_dndy_w[gp] =
          $signed(job_meta_i[META_PLANE_LO + META_PLANE_W*gp +   0 +: 72]);
      assign incoming_plane_dndx_w[gp] =
          $signed(job_meta_i[META_PLANE_LO + META_PLANE_W*gp +  72 +: 72]);
      assign incoming_plane_n0_w[gp] =
          $signed(job_meta_i[META_PLANE_LO + META_PLANE_W*gp + 144 +: 96]);
    end
  endgenerate

  // Accepted old geometric fields and tile lifecycle.
  logic signed [20:0] ax_q, ay_q, bx_q, by_q, cx_q, cy_q;
  logic signed [11:0] tile_x_q, tile_y_q;
  logic        [15:0] tile_index_q, source_id_q;
  logic               first_q, last_q;
  logic        [63:0] clear_word_q;

  logic job_accept_w;
  logic new_job_accept_w;
  logic job_metadata_capture_w;
  logic terminal_prior_w;
  logic abort_now_w;
  logic profile_aux_bad_w, profile_area_bad_w;
  assign profile_aux_bad_w  = job_profile_bad_i[0];
  assign profile_area_bad_w = job_profile_bad_i[1];

  // THE EQUIVALENCE CHECK THAT KEEPS ONE FACT FROM BECOMING TWO.
  //
  // The Packet-D bit positions below are also stated in `zhao_geom_binner_v2`,
  // which is where the verdicts are now decided. A layout stated twice and
  // checked nowhere is the failure this repository has written down more than
  // once, so the original expressions stay here as the reference and are
  // asserted against the delivered bits on every offered job.
  //
  // `synthesis translate_off` keeps this out of the fabric. It does NOT keep it
  // out of Verilator -- that was proven by planting a syntax error inside one
  // and watching lint reject it -- which is exactly what is wanted: the check
  // is live in every simulation and absent from the silicon.
  //
  // synthesis translate_off
  logic profile_aux_bad_ref_c, profile_area_bad_ref_c;
  assign profile_aux_bad_ref_c = incoming_flat_request_w[268] ||
                                 (incoming_flat_request_w[267:44] != 224'd0);
  assign profile_area_bad_ref_c = (incoming_area2_w == 47'd0);
  always_ff @(posedge clk) begin
    if (rst_n && job_valid_i) begin
      a_profile_bad_matches_meta : assert
          ((profile_aux_bad_w  == profile_aux_bad_ref_c) &&
           (profile_area_bad_w == profile_area_bad_ref_c))
        else $fatal(1,
            "profile verdict disagrees with job_meta_i: aux %0b vs %0b, area %0b vs %0b -- the Packet-D layout has drifted between zhao_geom_binner_v2 and this file",
            profile_aux_bad_w, profile_aux_bad_ref_c,
            profile_area_bad_w, profile_area_bad_ref_c);
    end
  end
  // synthesis translate_on

  // -------------------------------------------------------------------------
  // EDGEWALK and the one-entry, three-destination row broadcaster.
  logic ew_job_valid_w, ew_job_ready_w;
  logic ew_cov_valid_w, ew_cov_ready_w, ew_cov_last_w;
  logic [3:0] ew_cov_row_w;
  logic [15:0] ew_cov_mask_w, ew_cov_source_w;
  logic ew_done_w, ew_degenerate_w;
  logic [8:0] ew_count_w;

  logic row_hold_valid_q;
  logic [3:0] row_hold_row_q;
  logic [15:0] row_hold_mask_q;
  logic row_hold_last_q;
  logic [ATTR_LANES-1:0] row_delivered_q;
  logic [ATTR_LANES-1:0] row_lane_fire_w;
  logic [ATTR_LANES-1:0] row_delivered_next_w;
  logic row_retire_w;
  logic saw_coverage_q;
  logic ew_done_q;
  logic [8:0] ew_count_q;
  logic ew_degenerate_q;

  logic [START_N-1:0] start_gate_w;
  logic [ATTR_LANES-1:0] attr_cov_gate_w;
  logic stage_admit_gate_w;
`ifdef ZHAO_PACKET_D_TEST_HOOKS
  assign start_gate_w = test_start_enable_i;
  assign attr_cov_gate_w = test_attr_cov_enable_i;
  assign stage_admit_gate_w = test_stage_admit_enable_i;
`else
  assign start_gate_w = {START_N{1'b1}};
  assign attr_cov_gate_w = {ATTR_LANES{1'b1}};
  assign stage_admit_gate_w = 1'b1;
`endif

  assign coverage_hold_valid_o = row_hold_valid_q;
  assign coverage_delivered_mask_o = row_delivered_q;

  // -------------------------------------------------------------------------
  // Exactly SIX frozen attribute-gradient lanes.
  logic [ATTR_LANES-1:0] attr_job_ready_w;
  logic [ATTR_LANES-1:0] attr_cov_ready_w;
  logic [ATTR_LANES-1:0] attr_q_valid_w;
  logic [ATTR_LANES-1:0] attr_q_ready_w;
  logic signed [31:0] attr_q_w [0:ATTR_LANES-1];
  logic [3:0] attr_row_w [0:ATTR_LANES-1];
  logic [3:0] attr_col_w [0:ATTR_LANES-1];
  logic [ATTR_LANES-1:0] attr_last_w;
  logic [ATTR_LANES-1:0] attr_sat_w, attr_error_w;
  logic [ATTR_LANES-1:0] attr_idle_w;
  logic [31:0] attr_pixels_w [0:ATTR_LANES-1];
  logic [31:0] attr_divides_w [0:ATTR_LANES-1];
  logic [31:0] attr_saturations_w [0:ATTR_LANES-1];
  logic [31:0] attr_divide_errors_w [0:ATTR_LANES-1];

  assign attribute_idle_o = attr_idle_w;

  // Held coordinated job-start fanout.  The accepted job record is already in
  // registers; each destination sees valid until its one acceptance.  No valid
  // is a function of that destination's ready.
  logic [START_N-1:0] start_delivered_q;
  logic [START_N-1:0] start_valid_w, start_fire_w, start_delivered_next_w;
  logic start_complete_w;
  assign start_delivered_mask_o = start_delivered_q;
  assign start_valid_w[0] = (rs_state_q == RS_START) &&
                            !start_delivered_q[0] && start_gate_w[0];
  assign start_fire_w[0] = start_valid_w[0] && ew_job_ready_w;
  // Destinations 1..ATTR_LANES are the attribute lanes; their valid/fire pair
  // is driven inside `g_attr` beside the lane it belongs to, so a seventh lane
  // cannot be instantiated without its start strobe.
  assign start_valid_w[START_CLEAR] = (rs_state_q == RS_START) && first_q &&
                            !start_delivered_q[START_CLEAR] &&
                            start_gate_w[START_CLEAR];
  assign start_fire_w[START_CLEAR] = start_valid_w[START_CLEAR] &&
                                     ts_clear_ready_w;
  assign start_delivered_next_w = start_delivered_q | start_fire_w;
  assign start_complete_w = &start_delivered_next_w;
  assign ew_job_valid_w = start_valid_w[0];

  genvar ga;
  generate
    for (ga = 0; ga < ATTR_LANES; ga = ga + 1) begin : g_attr
      assign start_valid_w[ga+1] = (rs_state_q == RS_START) &&
                                   !start_delivered_q[ga+1] &&
                                   start_gate_w[ga+1];
      assign start_fire_w[ga+1] = start_valid_w[ga+1] && attr_job_ready_w[ga];
      if (ATTR_DSP3) begin : g_dsp3
        zhao_raster_attrgrad_dsp3 u_attrgrad (
            .clk(clk),
            .rst_n(rst_n),
            .job_valid_i(start_valid_w[ga+1]),
            .job_ready_o(attr_job_ready_w[ga]),
            .job_n0_i(plane_n0_q[ga]),
            .job_dndx_i(plane_dndx_q[ga]),
            .job_dndy_i(plane_dndy_q[ga]),
            .job_area2_i(area2_q),
            .job_min_x_i(min_x_q),
            .job_tile_x_i(tile_x_q),
            .job_tile_y_i(tile_y_q),
            .cov_valid_i(row_hold_valid_q && !row_delivered_q[ga] &&
                         attr_cov_gate_w[ga]),
            .cov_ready_o(attr_cov_ready_w[ga]),
            .cov_row_i(row_hold_row_q),
            .cov_mask_i(row_hold_mask_q),
            .cov_last_i(row_hold_last_q),
            .q_valid_o(attr_q_valid_w[ga]),
            .q_ready_i(attr_q_ready_w[ga]),
            .q_o(attr_q_w[ga]),
            .q_row_o(attr_row_w[ga]),
            .q_col_o(attr_col_w[ga]),
            .q_last_o(attr_last_w[ga]),
            .q_saturated_o(attr_sat_w[ga]),
            .q_error_o(attr_error_w[ga]),
            .idle_o(attr_idle_w[ga]),
            .pixels_o(attr_pixels_w[ga]),
            .divides_o(attr_divides_w[ga]),
            .saturations_o(attr_saturations_w[ga]),
            .divide_errors_o(attr_divide_errors_w[ga])
        );
      end else begin : g_v2
        zhao_raster_attrgrad_v2 u_attrgrad (
            .clk(clk),
            .rst_n(rst_n),
            .job_valid_i(start_valid_w[ga+1]),
            .job_ready_o(attr_job_ready_w[ga]),
            .job_n0_i(plane_n0_q[ga]),
            .job_dndx_i(plane_dndx_q[ga]),
            .job_dndy_i(plane_dndy_q[ga]),
            .job_area2_i(area2_q),
            .job_min_x_i(min_x_q),
            .job_tile_x_i(tile_x_q),
            .job_tile_y_i(tile_y_q),
            .cov_valid_i(row_hold_valid_q && !row_delivered_q[ga] &&
                         attr_cov_gate_w[ga]),
            .cov_ready_o(attr_cov_ready_w[ga]),
            .cov_row_i(row_hold_row_q),
            .cov_mask_i(row_hold_mask_q),
            .cov_last_i(row_hold_last_q),
            .q_valid_o(attr_q_valid_w[ga]),
            .q_ready_i(attr_q_ready_w[ga]),
            .q_o(attr_q_w[ga]),
            .q_row_o(attr_row_w[ga]),
            .q_col_o(attr_col_w[ga]),
            .q_last_o(attr_last_w[ga]),
            .q_saturated_o(attr_sat_w[ga]),
            .q_error_o(attr_error_w[ga]),
            .idle_o(attr_idle_w[ga]),
            .pixels_o(attr_pixels_w[ga]),
            .divides_o(attr_divides_w[ga]),
            .saturations_o(attr_saturations_w[ga]),
            .divide_errors_o(attr_divide_errors_w[ga])
        );
      end
      assign row_lane_fire_w[ga] = row_hold_valid_q &&
                                   !row_delivered_q[ga] &&
                                   attr_cov_gate_w[ga] &&
                                   attr_cov_ready_w[ga];
    end
  endgenerate

  assign row_delivered_next_w = row_delivered_q | row_lane_fire_w;
  assign row_retire_w = row_hold_valid_q && (&row_delivered_next_w);
  // Deliberately no simultaneous retire/refill: the held row is the explicit
  // ownership boundary, and one bubble is cheaper than an ambiguous replacement.
  assign ew_cov_ready_w = !row_hold_valid_q;

  zhao_raster_edgewalk u_edgewalk (
      .clk(clk),
      .rst_n(rst_n),
      .job_valid_i(ew_job_valid_w),
      .job_ready_o(ew_job_ready_w),
      .job_ax_i(ax_q), .job_ay_i(ay_q),
      .job_bx_i(bx_q), .job_by_i(by_q),
      .job_cx_i(cx_q), .job_cy_i(cy_q),
      .job_tile_x_i(tile_x_q),
      .job_tile_y_i(tile_y_q),
      .job_src_id_i(source_id_q),
      .cov_valid_o(ew_cov_valid_w),
      .cov_ready_i(ew_cov_ready_w),
      .cov_row_o(ew_cov_row_w),
      .cov_mask_o(ew_cov_mask_w),
      .cov_last_o(ew_cov_last_w),
      .cov_src_id_o(ew_cov_source_w),
      .job_done_o(ew_done_w),
      .job_degenerate_o(ew_degenerate_w),
      .cov_count_o(ew_count_w)
  );

  // -------------------------------------------------------------------------
  // One elastic registered join between the three ATTR lanes and Early-Z.
  // Timing2 measured the unregistered lane-1 col -> fault/cancel -> V3-admit
  // path at 11.086 ns.  The head below captures the complete atomic lane bundle,
  // so both fault classification and Early-Z start from registers.
  //
  // All equations use pre-edge state.  A held head is consumed by either the
  // abort/drop sink or an accepting Early-Z; that same edge may refill it:
  //
  //   consume = head_valid && (abort || head_fault || earlyz_ready)
  //   room    = !head_valid || consume
  //   capture = all_source_valid && room
  //   valid'  = capture || (head_valid && !consume)
  //
  // Thus stalls hold every bit, malformed bundles never enter Early-Z, and the
  // steady state consumes and captures one complete bundle per clock.
  logic attr_source_valid_w;
  logic attr_bundle_valid_w;
  logic attr_join_valid_q;
  logic attr_coordinate_bad_w, attr_range_bad_w, attr_bundle_fault_w;
  logic attr_coordinate_bad_q, attr_range_bad_q;
  logic attr_join_room_w, attr_join_capture_w, attr_join_consume_w;
  logic earlyz_frag_valid_w, earlyz_frag_ready_w;
  logic signed [31:0] attr_join_q_q [0:ATTR_LANES-1];
  logic [3:0] attr_join_row_q [0:ATTR_LANES-1];
  logic [3:0] attr_join_col_q [0:ATTR_LANES-1];
  logic [ATTR_LANES-1:0] attr_join_last_q;
  logic [ATTR_LANES-1:0] attr_join_sat_q, attr_join_error_q;
  logic [3:0] lane1_col_checked_w;

  assign attr_source_valid_w = &attr_q_valid_w;
  assign attr_bundle_valid_w = attr_join_valid_q;

  // TIMING4 D2. THE VERDICTS ARE CAPTURED WITH THE PAYLOAD, NOT DERIVED AFTER
  // IT.
  //
  // MEASURED: these two comparisons used to be computed from the join
  // registers, so a six-way coordinate agreement plus a range test sat in front
  // of everything abort_now_w reaches. The Timing3 census names that cone in
  // the tile-control family.
  //
  // They are pure functions of the values the join register loads, computed
  // here from the INCOMING lanes and registered by the SAME enable, from the
  // SAME transfer. That is not the lockstep-blindness the repo warns about --
  // it is its opposite requirement: a verdict must describe the payload it
  // travels with, so the two MUST move together. Moving them apart is what E1's
  // committed mutant exists to punish.
  //
  // The result is bit-identical at every cycle: on a capture both sides load
  // from the same sources, and on a hold both sides hold. What changes is only
  // where the comparison sits relative to the register.
  wire [3:0] incoming_lane1_col_c = `ZHAO_PACKET_D_LANE1_COL(attr_col_w[1]);
  // EVERY lane must agree with lane 0 about which pixel it is describing, and
  // the loop is over ATTR_LANES rather than a hand-written list so the three
  // Gouraud lanes are checked exactly as strictly as the three that were here
  // before. Lane 1 keeps its inverse-control indirection; that macro is how E1's
  // committed coordinate mutant reaches this comparison, and it must stay on
  // lane 1 specifically.
  //
  // The loop variable is declared INSIDE the block. A module-scope `int` driven
  // from an `always_comb` under a conditional is a latch Quartus 17.0 refuses
  // the whole design for, while Verilator lints it clean
  // (`tools/quartus/check_quartus17_syntax.py` FORM 7).
  logic incoming_coordinate_bad_c;
  always_comb begin : p_attr_incoming_coord
    logic [3:0] lane_col_c;
    incoming_coordinate_bad_c = 1'b0;
    for (int lane = 1; lane < ATTR_LANES; lane++) begin
      lane_col_c = (lane == LANE_UOW) ? incoming_lane1_col_c
                                      : attr_col_w[lane];
      if ((attr_row_w[0] != attr_row_w[lane]) ||
          (attr_col_w[0] != lane_col_c) ||
          (attr_last_w[0] != attr_last_w[lane]))
        incoming_coordinate_bad_c = 1'b1;
    end
  end
  // THE RANGE TEST IS LANE 0's AND STAYS LANE 0's. It asserts the depth lane's
  // quotient fits invw24, which is a property of THAT attribute; u/w, v/w and
  // the three colour channels have no such 24-bit law and never did. A colour
  // lane whose interpolant leaves [0,1] is handled where it is consumed, by the
  // saturating `lit_unit8` conversion at `vertex_rgb`, because a frame-
  // terminating fault for an over-bright pixel would be worse than the pixel.
  wire incoming_range_bad_c = (|attr_error_w) || attr_q_w[LANE_INVW][31] ||
                              (attr_q_w[LANE_INVW][31:24] != 8'd0);

  // Retained so the join registers still read as the authority they are: these
  // are what the captured verdicts were computed from, and the assertion below
  // checks the two agree on every valid bundle.
  assign lane1_col_checked_w =
      `ZHAO_PACKET_D_LANE1_COL(attr_join_col_q[1]);

  assign attr_coordinate_bad_w = attr_coordinate_bad_q;
  assign attr_range_bad_w = attr_range_bad_q;
  assign attr_bundle_fault_w = attr_bundle_valid_w &&
                               (attr_coordinate_bad_w || attr_range_bad_w);
  assign attr_join_consume_w = attr_bundle_valid_w &&
      (abort_now_w || attr_bundle_fault_w || earlyz_frag_ready_w);
  assign attr_join_room_w = !attr_bundle_valid_w || attr_join_consume_w;
  assign attr_join_capture_w = attr_source_valid_w && attr_join_room_w;

  always_comb begin : p_attr_q_ready
    attr_q_ready_w = {ATTR_LANES{attr_join_capture_w}};
  end

  always_ff @(posedge clk or negedge rst_n) begin : p_attr_join
    if (!rst_n) begin
      attr_join_valid_q <= 1'b0;
      attr_coordinate_bad_q <= 1'b0;
      attr_range_bad_q <= 1'b0;
      for (int lane = 0; lane < ATTR_LANES; lane++) begin
        attr_join_q_q[lane] <= 32'sd0;
        attr_join_row_q[lane] <= 4'd0;
        attr_join_col_q[lane] <= 4'd0;
        attr_join_last_q[lane] <= 1'b0;
        attr_join_sat_q[lane] <= 1'b0;
        attr_join_error_q[lane] <= 1'b0;
      end
    end else begin
      attr_join_valid_q <= attr_join_capture_w ||
                           (attr_bundle_valid_w && !attr_join_consume_w);
      if (attr_join_capture_w) begin
        // D2: the verdicts ride the same enable as the payload they describe.
        attr_coordinate_bad_q <= incoming_coordinate_bad_c;
        attr_range_bad_q      <= incoming_range_bad_c;
        for (int lane = 0; lane < ATTR_LANES; lane++) begin
          attr_join_q_q[lane] <= attr_q_w[lane];
          attr_join_row_q[lane] <= attr_row_w[lane];
          attr_join_col_q[lane] <= attr_col_w[lane];
          attr_join_last_q[lane] <= attr_last_w[lane];
          attr_join_sat_q[lane] <= attr_sat_w[lane];
          attr_join_error_q[lane] <= attr_error_w[lane];
        end
      end
    end
  end

  // ---------------------------------------------------------------------------
  // THE ONE CONVERSION THE GOURAUD LANES NEED, AND WHERE ITS SCALES COME FROM.
  //
  // A lane carries the interpolated light in the scale GEOM.LIGHT emits:
  // Q0.16 with `NDL_ONE = 17'h1_0000` as 1.0, asserted in range at
  // `zhao_light_stream`'s own output. `zhao_raster_fragment` wants unit8 --
  // `frag_vert_rgb_i` is {r[23:16], g[15:8], b[7:0]} and 255 is full -- because
  // it feeds `unit_mul(texel, vertex)` when the fragment is textured and is
  // taken as the source colour directly when it is not. That matches the
  // oracle's own account of the lanes exactly
  // (`reference/src/zrender/internal.hpp`: "per-channel light GAIN, unity =
  // 1<<16" textured; "pre-lit COLOUR on the 255 scale" untextured, which this
  // console produces as `unit_mul(base_rgb, gain)` because ruling R11 already
  // makes `base_rgb` the VERTEX's colour).
  //
  // SATURATING, NOT FAULTING. 1.0 maps to 255 and anything above it clamps.
  // An out-of-range colour is a bright pixel; making it a frame-terminating
  // range fault -- which is what lane 0's invw24 test does -- would trade a
  // slightly wrong pixel for a lost frame. Negative clamps to 0 for the same
  // reason: an unlit lane may legitimately carry a negative interpolant, which
  // `zhao_raster_toon`'s header says outright.
  //
  // IT IS A FUNCTION, ON ITS OWN, BECAUSE RASTER.TOON GOES IN FRONT OF IT.
  // `zhao_raster_toon` takes `r_i`/`g_i`/`b_i` as `signed [31:0]` -- exactly
  // `attr_join_q_q[]`'s shape, in exactly this Q0.16 scale, which is why its
  // authored thresholds read {43000, 57000} against a 65536 unity. When that
  // block and `zhao_raster_fog` are composed they belong BETWEEN the join and
  // this conversion, and nothing else here moves. Neither is instantiated by
  // any composed top today; `design/prod_manifest.yml` says so.
  function automatic logic [7:0] lit_unit8(input logic signed [31:0] v);
    begin
      if (v <= 32'sd0)            lit_unit8 = 8'd0;
      else if (v >= 32'sd65536)   lit_unit8 = 8'd255;
      else                        lit_unit8 = v[15:8];
    end
  endfunction

  zhao_texture_v3_request_v2_t request_w;
  zhao_raster_continuation_v2_t continuation_w;
  zhao_raster_pretex_v2_t pretex_w;
  logic [410:0] earlyz_payload_in_w;

  // The low 298 bits preserve the package request's existing low-field layout.
  // Each frozen field is named once here; U/W and V/W come only from their lanes.
  always_comb begin
    request_w = '0;
    request_w.palette_generation    = flat_request_q[7:0];
    request_w.palette_slot          = flat_request_q[9:8];
    request_w.response_class        = flat_request_q[11:10];
    request_w.base_alpha            = flat_request_q[19:12];
    request_w.base_rgb              = flat_request_q[43:20];
    request_w.aux_surface_ctx       = zhao_aux_surface_ctx_v2_t'(flat_request_q[267:44]);
    request_w.aux_required          = flat_request_q[268];
    request_w.recipe_weight         = flat_request_q[276:269];
    request_w.material_recipe       = flat_request_q[279:277];
    request_w.lod_q4_4              = flat_request_q[287:280];
    request_w.base_binding_selector = flat_request_q[295:288];
    request_w.sample_count          = flat_request_q[297:296];
    request_w.u_over_w              = attr_join_q_q[LANE_UOW];
    request_w.v_over_w              = attr_join_q_q[LANE_VOW];
    // TERRAIN.NORMALMAP's declaration (NORMALMAP, 2026-09-26). It rides the
    // metadata in the DEAD TOP BIT of the continuation tail's `vertex_rgb`
    // field -- the same 24 bits owner decision R234 D1 made this module
    // overwrite per fragment three statements below, and of which entry I54's
    // arena index already uses eighteen. Bit 47 is the tail's most significant,
    // which is the furthest from that 18-bit field: if the arena index ever
    // widens it grows upward from bit 24 and meets this last.
    //
    // THE SAME BARGAIN, AND IT IS STATED AGAIN BECAUSE IT IS NOT FREE SPACE.
    // These bits are dead by ONE owner decision. If R234 D1 is ever reversed
    // and `vertex_rgb` becomes live from the tail again, this carrier and I54's
    // must both move before that happens. `zhao_geom_bin_pipe_v2` refuses at
    // elaboration if the tail layout moves, which is what keeps that honest.
    request_w.detail_required       = continuation_tail_bits_q[47];

    continuation_w = '0;
    continuation_w.earlyz.in_tile_addr = {attr_join_row_q[LANE_INVW],
                                          attr_join_col_q[LANE_INVW]};
    continuation_w.earlyz.invw24 = attr_join_q_q[LANE_INVW][23:0];
    continuation_w.earlyz.fragment_state = fragment_state_q;
    continuation_w.earlyz.source_id = source_id_q;
    // THE FLAT STAND-IN FOR THE GOURAUD TINT, NAMED AS ONE (gz/attrlane,
    // 2026-09-21, discharging the instruction `zhao_console_core.sv` entry I13
    // leaves open: "it must be NAMED a stand-in in the RTL, with terrain_rules
    // 6.5 cited beside it, or the next reader inherits a Gouraud law silently
    // implemented as a constant").
    //
    // READ THE ASSIGNMENTS ABOVE TOGETHER. `invw24`, `u_over_w` and `v_over_w`
    // come from `attr_join_q_q[0..2]` -- PER FRAGMENT, this pixel's own
    // interpolated value off its attribute lane. `post_earlyz` comes from
    // `continuation_tail_bits_q`, loaded ONCE PER TRIANGLE from
    // `job_meta_i[345:298]`, so its fields are per-primitive while their
    // neighbours are per-pixel. That asymmetry used to be silent and is now
    // stated here, because it is the whole reason `vertex_rgb` was wrong.
    //
    // AND `vertex_rgb` IS NO LONGER ONE OF THEM, as of owner decision R234 D1.
    // The tail still arrives per triangle and still supplies `vertex_alpha`,
    // `effect_tag` and `stencil_reference` -- those three ARE per-primitive
    // (R89, R48) -- but the colour is overwritten below from lanes 3..5, which
    // are this pixel's own interpolated light. The two statements are ordered
    // deliberately: the cast first, then the one field that has a better
    // producer than the tail's constant.
    //
    // THE CHAIN IT COMPLETES. `zhao_light_stream` produces lit per-vertex
    // r/g/b, `zhao_geom_vattr` stores it, `zhao_geom_clip` carries it
    // winding-flipped in packet slots 3..5, `zhao_geom_attrpack` turns those
    // three slots into three planes, `zhao_geom_bin_pipe_v2` carries them in
    // metadata bits [1876:1157], and the three lanes above evaluate them per
    // pixel. See `zhao_geom_attrpack.sv`'s waiver comment for the other end.
    //
    // `spec/terrain_rules.md` 6.5 says "tint moved to vertices"; the reference
    // oracle interpolates it for real (`reference/src/zrender/rast.cpp`, the
    // `m.gouraud` lanes `cr`/`cg`/`cb`, full barycentric re-evaluation per row,
    // with `reference/src/zrender/internal.hpp` calling it "the ordinary
    // Gouraud path"). This is now that, in silicon.
    //
    // NOTHING DOWNSTREAM OF THIS MODULE CHANGED to make it happen, which is
    // what ATTRLANE predicted and what held: the carrier was already 24 bits,
    // already per-fragment assembled, and already traverses Early-Z, the
    // texture round trip and the fragment leaf.
    continuation_w.post_earlyz =
        zhao_raster_continuation_tail_v2_t'(continuation_tail_bits_q);
    continuation_w.post_earlyz.vertex_rgb = {lit_unit8(attr_join_q_q[LANE_R]),
                                             lit_unit8(attr_join_q_q[LANE_G]),
                                             lit_unit8(attr_join_q_q[LANE_B])};

    pretex_w = make_raster_pretex(continuation_w, request_w);
    earlyz_payload_in_w = pack_earlyz_payload(pretex_w.payload);
  end

  logic earlyz_cand_valid_w, earlyz_cand_ready_w;
  logic [7:0] earlyz_cand_addr_w;
  logic [23:0] earlyz_cand_depth_w;
  logic [31:0] earlyz_cand_state_w;
  logic [15:0] earlyz_cand_source_w;
  logic [410:0] earlyz_cand_payload_w;
  logic [2:0] earlyz_cand_bin_w;
  logic earlyz_reject_w;
  logic [7:0] earlyz_reject_addr_w;

  assign earlyz_frag_valid_w = attr_bundle_valid_w &&
                               !attr_bundle_fault_w && !abort_now_w;

  assign earlyz_hold_valid_o = earlyz_cand_valid_w;

  zhao_raster_earlyz #(.PAYLOAD_W(411)) u_earlyz (
      .clk(clk),
      .rst_n(rst_n),
      .tile_begin_i(start_fire_w[START_CLEAR]),
      .tile_clear_depth_i(clear_word_q[31:8]),
      .frag_valid_i(earlyz_frag_valid_w),
      .frag_ready_o(earlyz_frag_ready_w),
      .frag_addr_i(pretex_w.earlyz.in_tile_addr),
      .frag_depth_i(pretex_w.earlyz.invw24),
      .frag_state_i(pretex_w.earlyz.fragment_state),
      .frag_src_id_i(pretex_w.earlyz.source_id),
      .frag_payload_i(earlyz_payload_in_w),
      .cand_valid_o(earlyz_cand_valid_w),
      .cand_ready_i(earlyz_cand_ready_w),
      .cand_addr_o(earlyz_cand_addr_w),
      .cand_depth_o(earlyz_cand_depth_w),
      .cand_state_o(earlyz_cand_state_w),
      .cand_src_id_o(earlyz_cand_source_w),
      .cand_payload_o(earlyz_cand_payload_w),
      .cand_bin_o(earlyz_cand_bin_w),
      .z_reject_o(earlyz_reject_w),
      .z_reject_addr_o(earlyz_reject_addr_w),
      .bin_mask_o(bin_mask_o),
      .z_floor_o(z_floor_o),
      .early_z_rejects_o(early_z_rejects_o),
      .covered_fragments_o(early_z_covered_o)
  );

  zhao_raster_earlyz_payload_v2_t earlyz_payload_out_w;
  zhao_raster_continuation_v2_t continuation_after_earlyz_w;
  zhao_raster_pretex_v2_t pretex_after_earlyz_w;
  logic [490:0] skid_up_data_w;
  always_comb begin
    earlyz_payload_out_w = unpack_earlyz_payload(earlyz_cand_payload_w);
    continuation_after_earlyz_w = '0;
    continuation_after_earlyz_w.earlyz.in_tile_addr = earlyz_cand_addr_w;
    continuation_after_earlyz_w.earlyz.invw24 = earlyz_cand_depth_w;
    continuation_after_earlyz_w.earlyz.fragment_state = earlyz_cand_state_w;
    continuation_after_earlyz_w.earlyz.source_id = earlyz_cand_source_w;
    continuation_after_earlyz_w.post_earlyz =
        earlyz_payload_out_w.raster_continuation;
    pretex_after_earlyz_w = make_raster_pretex(
        continuation_after_earlyz_w, earlyz_payload_out_w.texture_request);
    skid_up_data_w = pack_raster_pretex(pretex_after_earlyz_w);
  end

  // -------------------------------------------------------------------------
  // Real 490-bit skid, Packet-C stage, and real fragment leaf.
  logic skid_up_ready_w, skid_dn_valid_w, skid_dn_ready_w;
  logic [490:0] skid_dn_data_w;
  logic stage_cand_ready_w;
  logic stage_frame_fault_w;
  logic stage_clear_valid_w, stage_clear_ready_w;
  logic skid_cancel_fire_w;

  assign earlyz_cand_ready_w = abort_now_w ? 1'b1 : skid_up_ready_w;
  assign skid_dn_ready_w = `ZHAO_PACKET_D_SKID_DN_READY(
      abort_now_w, (stage_cand_ready_w && stage_admit_gate_w));
  assign skid_cancel_fire_w = skid_dn_valid_w && skid_dn_ready_w && abort_now_w;

  // TRIED AND REJECTED, 2026-09-16: dropping the sequence terms from this valid.
  //
  // Packet C does re-apply them to its own admission (cand_ready_o and
  // v3_frag_valid_w are each qualified by `!sequence_abort_q &&
  // !sequence_mismatch_w`), so sending the stage's deep returned-sequence
  // compare out through this reduction and back into its own candidate capture
  // looks redundant, and removing it looks free.
  //
  // It is not. skid_dn_ready_w and skid_cancel_fire_w below keep the full
  // abort_now_w, so on an abort edge the skid CANCELS and reloads underneath an
  // offer whose valid would now still be asserted -- the 490-bit payload changes
  // while an unaccepted valid is high, which section 5 of the Timing4 brief
  // forbids outright ("ready/valid offers hold identity and payload under
  // backpressure").
  //
  // The committed control catches it exactly:
  //   geom_bin_pipe_v2_identity_cancel_control ->
  //   "490-bit stage candidate changed under backpressure" at cycle 1468.
  //
  // Making the cancel terms match would delay sequence-abort cancellation of
  // skid contents and move the drop counts, which is a different change with a
  // different contract. So the sequence terms stay here, and the tile-control
  // relief comes from the metadata bank below instead.
  assign stage_candidate_valid_o = skid_dn_valid_w && !abort_now_w &&
                                   stage_admit_gate_w;
  assign stage_candidate_data_o = skid_dn_data_w;

  zhao_skid2 #(.W(491)) u_candidate_skid (
      .clk(clk),
      .rst_n(rst_n),
      .up_valid_i(earlyz_cand_valid_w && !abort_now_w),
      .up_ready_o(skid_up_ready_w),
      .up_data_i(skid_up_data_w),
      .dn_valid_o(skid_dn_valid_w),
      .dn_ready_i(skid_dn_ready_w),
      .dn_data_o(skid_dn_data_w),
      .level_o(skid_level_o)
  );

  logic stage_frag_ready_w;
  logic [23:0] stage_frag_vert_rgb_w;
  logic [7:0] stage_frag_vert_a_w;
  logic [7:0] stage_frag_tag_w;
  logic [7:0] stage_frag_stencil_w;

  // Packet-B compatibility/evidence signals not promoted as top-level counters.
  logic unused_err_fragrob_wq_overflow, unused_err_fragrob_id_error;
  logic unused_err_aux_degenerate, unused_err_rcp_q;
  logic [31:0] unused_cnt_reorder_held, unused_cnt_live_peak;
  logic [31:0] unused_cnt_bilerp_jobs, unused_cnt_mosaic_samples;
  logic [31:0] unused_cnt_aux_accepted, unused_cnt_combine_phases;
  logic [31:0] unused_cnt_rcp_completed, unused_cnt_persp_fragments;
  logic [31:0] unused_cnt_fragrob_id_errors;
  logic unused_shadow_present;
  logic [31:0] unused_meta_shadow_mismatch, unused_meta_shadow_reads;
  logic [31:0] unused_meta_align_err, unused_meta_align_chk;
  logic [31:0] unused_meta_bil_err, unused_meta_bil_chk;
  logic [31:0] unused_meta_near_err, unused_meta_near_chk;
  logic [20:0] unused_meta_bil_first_q, unused_meta_bil_first_t;
  logic [17:0] unused_meta_bil_first_tok;
  logic [31:0] unused_meta_genmis;
  logic [31:0] unused_cnt_combine_jobs [0:7];
  logic [31:0] unused_cnt_palette_stale, unused_cnt_palette_cold;
  logic unused_err_rsp_dropped, unused_err_bil_chan;
  logic [31:0] unused_cnt_near_refused;
  logic [31:0] unused_err_unknown_class, unused_err_class_invalid;
  logic [31:0] unused_err_palette_unusable, unused_err_class_mismatch;
  logic unused_err_plan_mode;

  zhao_raster_texture_stage_v3 #(
      .MIGRATION_SHADOWS(1'b0),
      .BILERP_DSP2(BILERP_DSP2)
  ) u_texture_stage (
      .clk(clk),
      .rst_n(rst_n),
      .cand_valid_i(stage_candidate_valid_o),
      .cand_ready_o(stage_cand_ready_w),
      .cand_data_i(skid_dn_data_w),
      .frame_fault_clear_valid_i(stage_clear_valid_w),
      .frame_fault_clear_ready_o(stage_clear_ready_w),
      .frame_fault_o(stage_frame_fault_w),
      .lifetime_structural_fault_o(lifetime_structural_fault_o),
      .cfg_valid_i(cfg_valid_i),
      .cfg_ready_o(cfg_ready_o),
      .cfg_op_i(cfg_op_i),
      .cfg_page_generation_i(cfg_page_generation_i),
      .cfg_selector_i(cfg_selector_i),
      .cfg_row_i(cfg_row_i),
      .cfg_crc32_i(cfg_crc32_i),
      .cfg_rsp_valid_o(cfg_rsp_valid_o),
      .cfg_rsp_ready_i(cfg_rsp_ready_i),
      .cfg_rsp_op_o(cfg_rsp_op_o),
      .cfg_rsp_status_o(cfg_rsp_status_o),
      .cfg_rsp_page_generation_o(cfg_rsp_page_generation_o),
      .active_page_generation_o(active_page_generation_o),
      .fill_req_valid_o(fill_req_valid_o),
      .fill_req_ready_i(fill_req_ready_i),
      .fill_req_addr_o(fill_req_addr_o),
      .fill_data_valid_i(fill_data_valid_i),
      .fill_data_i(fill_data_i),
      .fill_refused_i(fill_refused_i),
      .pal_load_valid_i(pal_load_valid_i),
      .pal_load_ready_o(pal_load_ready_o),
      .pal_load_op_i(pal_load_op_i),
      .pal_load_slot_i(pal_load_slot_i),
      .pal_load_gen_i(pal_load_gen_i),
      .pal_load_idx_i(pal_load_idx_i),
      .pal_load_rgb565_i(pal_load_rgb565_i),
      .pal_load_crc_ok_i(pal_load_crc_ok_i),
      .dtl_we_i(dtl_we_i),
      .dtl_sel_i(dtl_sel_i),
      .dtl_addr_i(dtl_addr_i),
      .dtl_data_i(dtl_data_i),
      .cnt_detail_fragments_o(cnt_detail_fragments_o),
      .cnt_detail_zeroed_o(cnt_detail_zeroed_o),
      .cnt_detail_railed_o(cnt_detail_railed_o),
      .cnt_detail_cold_o(cnt_detail_cold_o),
      .cnt_detail_published_o(cnt_detail_published_o),
      .cnt_detail_applied_o(cnt_detail_applied_o),
      .err_detail_lost_o(err_detail_lost_o),
      .dtl_table_ready_o(dtl_table_ready_o),
      .sheet_req_valid_o(sheet_req_valid_o),
      .sheet_req_ready_i(sheet_req_ready_i),
      .sheet_req_op_o(sheet_req_op_o),
      .sheet_req_handle_o(sheet_req_handle_o),
      .sheet_req_texel_o(sheet_req_texel_o),
      .sheet_req_src_id_o(sheet_req_src_id_o),
      .pg_valid_i(pg_valid_i),
      .pg_ready_o(pg_ready_o),
      .pg_op_i(pg_op_i),
      .pg_status_i(pg_status_i),
      .pg_tag_i(pg_tag_i),
      .pg_strength_i(pg_strength_i),
      .pg_src_id_i(pg_src_id_i),
      .frag_valid_o(stage_fragment_valid_o),
      .frag_ready_i(stage_frag_ready_w),
      .frag_addr_o(stage_fragment_addr_o),
      .frag_depth_o(stage_fragment_depth_o),
      .frag_state_o(stage_fragment_state_o),
      .frag_src_id_o(stage_fragment_src_id_o),
      .frag_vert_rgb_o(stage_frag_vert_rgb_w),
      .frag_vert_a_o(stage_frag_vert_a_w),
      .frag_tag_o(stage_frag_tag_w),
      .frag_sten_ref_o(stage_frag_stencil_w),
      .frag_texel_rgb_o(stage_fragment_texel_rgb_o),
      .frag_texel_a_o(stage_fragment_texel_a_o),
      .frag_texel_idx_o(stage_fragment_texel_idx_o),
      .frag_status_o(stage_fragment_status_o),
      .quiet_o(texture_quiet_o),
      .sequence_abort_o(sequence_abort_o),
      .sequence_drop_count_o(sequence_drop_count_o),
      .sequence_mismatch_o(sequence_mismatch_o),
      .admission_sequence_o(admission_sequence_o),
      .expected_sequence_o(expected_sequence_o),
      .returned_sequence_o(returned_sequence_o),
      .cand_fire_o(packet_c_cand_fire_o),
      .fragment_fire_o(packet_c_fragment_fire_o),
      .drop_fire_o(packet_c_drop_fire_o),
      .err_fragrob_wq_overflow_o(unused_err_fragrob_wq_overflow),
      .err_fragrob_id_error_o(unused_err_fragrob_id_error),
      .err_aux_degenerate_o(unused_err_aux_degenerate),
      .err_rcp_q_o(unused_err_rcp_q),
      .cnt_reorder_held_o(unused_cnt_reorder_held),
      .cnt_live_peak_o(unused_cnt_live_peak),
      .cnt_fragments_o(texture_fragments_o),
      .cnt_cache_hits_o(texture_cache_hits_o),
      .cnt_cache_misses_o(texture_cache_misses_o),
      .cnt_palette_lookups_o(texture_palette_lookups_o),
      .cnt_bilerp_jobs_o(unused_cnt_bilerp_jobs),
      .cnt_mosaic_samples_o(unused_cnt_mosaic_samples),
      .cnt_texture_samples_o(texture_samples_o),
      .cnt_aux_accepted_o(unused_cnt_aux_accepted),
      .cnt_combine_refused_o(texture_combine_refused_o),
      .cnt_combine_phases_o(unused_cnt_combine_phases),
      .cnt_rcp_completed_o(unused_cnt_rcp_completed),
      .cnt_persp_fragments_o(unused_cnt_persp_fragments),
      .cnt_dispatch_accepted_o(texture_dispatch_accepted_o),
      .cnt_plan_accepted_o(texture_plan_accepted_o),
      .cnt_fragrob_id_errors_o(unused_cnt_fragrob_id_errors),
      .shadow_present_o(unused_shadow_present),
      .meta_shadow_mismatch_o(unused_meta_shadow_mismatch),
      .meta_shadow_reads_o(unused_meta_shadow_reads),
      .meta_align_err_o(unused_meta_align_err),
      .meta_align_chk_o(unused_meta_align_chk),
      .meta_bil_err_o(unused_meta_bil_err),
      .meta_bil_chk_o(unused_meta_bil_chk),
      .meta_near_err_o(unused_meta_near_err),
      .meta_near_chk_o(unused_meta_near_chk),
      .meta_bil_first_q_o(unused_meta_bil_first_q),
      .meta_bil_first_t_o(unused_meta_bil_first_t),
      .meta_bil_first_tok_o(unused_meta_bil_first_tok),
      .meta_genmis_o(unused_meta_genmis),
      .cnt_combine_jobs_o(unused_cnt_combine_jobs),
      .cnt_palette_stale_o(unused_cnt_palette_stale),
      .cnt_palette_cold_o(unused_cnt_palette_cold),
      .err_rsp_dropped_o(unused_err_rsp_dropped),
      .err_bil_chan_o(unused_err_bil_chan),
      .cnt_near_refused_o(unused_cnt_near_refused),
      .err_unknown_class_o(unused_err_unknown_class),
      .err_class_invalid_o(unused_err_class_invalid),
      .err_palette_unusable_o(unused_err_palette_unusable),
      .err_class_mismatch_o(unused_err_class_mismatch),
      .err_plan_mode_o(unused_err_plan_mode)
  );

  // -------------------------------------------------------------------------
  // Real fragment, TILESTORE, and RESOLVE path retained from the old tile pipe.
  logic ts_clear_w, ts_clear_ready_w;
  logic ts_wr_w, ts_wr_ready_w;
  logic [7:0] ts_wr_addr_w;
  logic [63:0] ts_wr_data_w;
  logic ts_rd_w, ts_rd_ready_w, ts_rd_valid_w;
  logic [7:0] ts_rd_addr_w;
  logic [15:0] ts_rd_source_in_w, ts_rd_source_w;
  logic [63:0] ts_rd_data_w;
  logic ts_swap_w, ts_swap_ready_w;

  assign stage_frag_ready_w = fragment_input_ready_w;
  logic fragment_input_ready_w;

  zhao_raster_fragment u_fragment (
      .clk(clk),
      .rst_n(rst_n),
      .frag_valid_i(stage_fragment_valid_o),
      .frag_ready_o(fragment_input_ready_w),
      .frag_addr_i(stage_fragment_addr_o),
      .frag_depth_i(stage_fragment_depth_o),
      .frag_state_i(stage_fragment_state_o),
      .frag_src_id_i(stage_fragment_src_id_o),
      .frag_vert_rgb_i(stage_frag_vert_rgb_w),
      .frag_vert_a_i(stage_frag_vert_a_w),
      .frag_tag_i(stage_frag_tag_w),
      .frag_sten_ref_i(stage_frag_stencil_w),
      .frag_texel_rgb_i(stage_fragment_texel_rgb_o),
      .frag_texel_a_i(stage_fragment_texel_a_o),
      .frag_texel_idx_i(stage_fragment_texel_idx_o),
      .rd_valid_o(ts_rd_w),
      .rd_ready_i(ts_rd_ready_w),
      .rd_addr_o(ts_rd_addr_w),
      .rd_src_id_o(ts_rd_source_in_w),
      .rd_valid_i(ts_rd_valid_w),
      .rd_data_i(ts_rd_data_w),
      .wr_valid_o(ts_wr_w),
      .wr_ready_i(ts_wr_ready_w),
      .wr_addr_o(ts_wr_addr_w),
      .wr_data_o(ts_wr_data_w),
      .fragment_error_o(fragment_error_o),
      .idle_o(fragment_idle_o),
      .covered_fragments_o(fragment_covered_o),
      .blended_fragments_o(blended_fragments_o)
  );

  logic tr_valid_w, tr_ready_w, tr_data_valid_w;
  logic [7:0] tr_addr_w;
  logic [63:0] tr_data_w;
  logic resolve_start_w, resolve_ready_w;

  zhao_raster_tilestore u_tilestore (
      .clk(clk),
      .rst_n(rst_n),
      .clear_valid_i(ts_clear_w),
      .clear_ready_o(ts_clear_ready_w),
      .clear_data_i(clear_word_q),
      .wr_valid_i(ts_wr_w),
      .wr_ready_o(ts_wr_ready_w),
      .wr_addr_i(ts_wr_addr_w),
      .wr_data_i(ts_wr_data_w),
      .rd_valid_i(ts_rd_w),
      .rd_ready_o(ts_rd_ready_w),
      .rd_addr_i(ts_rd_addr_w),
      .rd_src_id_i(ts_rd_source_in_w),
      .rd_valid_o(ts_rd_valid_w),
      .rd_data_o(ts_rd_data_w),
      .rd_src_id_o(ts_rd_source_w),
      .res_valid_i(tr_valid_w),
      .res_ready_o(tr_ready_w),
      .res_addr_i(tr_addr_w),
      .res_valid_o(tr_data_valid_w),
      .res_data_o(tr_data_w),
      .swap_valid_i(ts_swap_w),
      .swap_ready_o(ts_swap_ready_w),
      .front_bank_o(front_bank_o),
      .tile_references_o(tilestore_references_o)
  );

  logic signed [11:0] resolve_tile_x_q, resolve_tile_y_q;
  logic [8:0] tile_coverage_acc_q, resolve_coverage_q;
  logic resolve_degenerate_q;

  assign resolve_start_w = (rs_state_q == RS_SWAP) && !abort_now_w;
  assign ts_swap_w = resolve_start_w && resolve_ready_w;

  zhao_raster_resolve u_resolve (
      .clk(clk),
      .rst_n(rst_n),
      .start_valid_i(resolve_start_w),
      .start_ready_o(resolve_ready_w),
      .start_tile_x_i(tile_x_q),
      .start_tile_y_i(tile_y_q),
      .start_tile_index_i(tile_index_q),
      .start_src_id_i(source_id_q),
      .tr_valid_o(tr_valid_w),
      .tr_ready_i(tr_ready_w),
      .tr_addr_o(tr_addr_w),
      .tr_data_valid_i(tr_data_valid_w),
      .tr_data_i(tr_data_w),
      .fb_valid_o(fb_valid_o),
      .fb_ready_i(fb_ready_i),
      .fb_rgb565_o(fb_rgb565_o),
      .fb_tag_o(fb_tag_o),
      .fb_addr_o(fb_addr_o),
      .fb_last_o(fb_last_o),
      .fb_src_id_o(fb_src_id_o),
      .tile_crc_o(tile_crc_o),
      .tile_crc_index_o(tile_crc_index_o),
      .tile_crc_valid_o(tile_done_o),
      .tile_references_o(resolved_tiles_o)
  );

  logic [11:0] fb_x_raw_w, fb_y_raw_w;
  always_comb begin
    fb_x_raw_w = resolve_tile_x_q + {8'd0, fb_addr_o[3:0]};
    fb_y_raw_w = resolve_tile_y_q + {8'd0, fb_addr_o[7:4]};
  end
  assign fb_x_o = $signed(fb_x_raw_w);
  assign fb_y_o = $signed(fb_y_raw_w);
  assign tile_cov_count_o = resolve_coverage_q;
  assign tile_degenerate_o = resolve_degenerate_q;

  assign ts_clear_w = start_valid_w[START_CLEAR];

  // -------------------------------------------------------------------------
  // Complete drain law, fault/clear policy and lifecycle.
  logic producer_quiet_w, ordinary_pipe_empty_w, complete_own_quiet_w;
  logic clear_fire_w;
  logic local_abort_q;
  logic local_fault_event_w, coordinate_fault_event_w, range_fault_event_w;
  logic aux_profile_fault_event_w, joined_attr_drop_w, earlyz_abort_drop_w;

  assign terminal_prior_w = local_abort_q || sequence_abort_o ||
                            sequence_mismatch_o;
  assign new_job_accept_w = job_valid_i && (rs_state_q == RS_IDLE) &&
                            !frame_fault_clear_valid_i && !terminal_prior_w;
  assign coordinate_fault_event_w = attr_bundle_fault_w && attr_coordinate_bad_w &&
                                    !terminal_prior_w;
  assign range_fault_event_w =
      ((attr_bundle_fault_w && attr_range_bad_w && !terminal_prior_w) ||
       (new_job_accept_w && profile_area_bad_w));
  assign aux_profile_fault_event_w =
      new_job_accept_w && profile_aux_bad_w;
  assign local_fault_event_w = coordinate_fault_event_w || range_fault_event_w ||
                               aux_profile_fault_event_w;

  // Include the detecting cycle so same-edge skid work is cancelled and no later
  // useful candidate can be admitted before the sticky level lands.
  assign abort_now_w = local_abort_q || local_fault_event_w ||
                       sequence_abort_o || sequence_mismatch_o;
  assign raster_abort_o = local_abort_q || sequence_abort_o || sequence_mismatch_o;
  assign local_attribute_abort_o = local_abort_q;
  assign local_fault_pulse_o = local_fault_event_w;
  assign frame_fault_o = local_abort_q || local_fault_event_w || stage_frame_fault_w;

  assign producer_quiet_w = (rs_state_q != RS_START) && ew_job_ready_w &&
                            !row_hold_valid_q &&
                            (&attr_idle_w) && !(|attr_q_valid_w) &&
                            !attr_bundle_valid_w;
  assign ordinary_pipe_empty_w = ew_done_q && producer_quiet_w &&
                                 !earlyz_cand_valid_w &&
                                 (skid_level_o == 2'd0) &&
                                 !stage_candidate_valid_o &&
                                 `ZHAO_PACKET_D_PIPE_V3_QUIET(texture_quiet_o) &&
                                 !stage_fragment_valid_o && fragment_idle_o;
  assign complete_own_quiet_w =
      ((rs_state_q == RS_IDLE) || abort_now_w) &&
      producer_quiet_w && !earlyz_cand_valid_w &&
      (skid_level_o == 2'd0) && !stage_candidate_valid_o &&
      texture_quiet_o && !stage_fragment_valid_o && fragment_idle_o &&
      resolve_ready_w;
  assign quiet_o = complete_own_quiet_w;

  // The caller (zhao_geom_bin_pipe_v2) additionally gates this with complete
  // binner drain.  Packet C receives no clear until this tile owns no work.
  assign stage_clear_valid_w = frame_fault_clear_valid_i && complete_own_quiet_w;
  assign frame_fault_clear_ready_o = complete_own_quiet_w && stage_clear_ready_w;
  assign clear_fire_w = frame_fault_clear_valid_i && frame_fault_clear_ready_o;

  assign job_ready_o = abort_now_w ? 1'b1 :
                       ((rs_state_q == RS_IDLE) && !frame_fault_clear_valid_i);
  assign job_accept_w = job_valid_i && job_ready_o;

  // THE WIDE FROZEN-IDENTITY BANK IS NOT QUALIFIED BY A COMBINATIONAL VERDICT.
  //
  // MEASURED: the Timing3 census reports a tile-control family whose endpoints
  // are accepted-job metadata register enables, reached from the PREVIOUS job's
  // attribute verdicts. The chain was
  //
  //   attr_coordinate_bad / attr_range_bad -> local_fault_event_w
  //     -> abort_now_w -> job_ready_o -> job_accept_w -> ~25 wide enables
  //
  // and it put a deep comparison cone in front of every coordinate, plane,
  // tile-index and clear-word register in the bank.
  //
  // The metadata is frozen identity: it is READ only by work that has actually
  // started, and the state/counter branch below still decides that separately.
  // So the bank is written on the ordinary acceptance condition alone. A job
  // that is sunk on a fault edge now writes dead payload which no started job
  // can observe, and which the next accepted job overwrites.
  //
  // BOTH STICKY LEVELS ARE RETAINED, so the freeze during an abort is exactly
  // what it was: local_abort_q is this tile's latched fault and sequence_abort_o
  // is Packet C's. Only the two COMBINATIONAL terms are dropped -- this cycle's
  // local_fault_event_w and the same-edge sequence_mismatch_o -- and only for
  // this bank. Every start strobe, state transition and counter below keeps the
  // full abort_now_w, so no bad job starts work and no count moves.
  assign job_metadata_capture_w =
      job_valid_i && (rs_state_q == RS_IDLE) && !frame_fault_clear_valid_i &&
      !local_abort_q && !sequence_abort_o;
  assign joined_attr_drop_w = attr_bundle_valid_w && attr_join_consume_w &&
                              abort_now_w;
  assign earlyz_abort_drop_w = earlyz_cand_valid_w && earlyz_cand_ready_w &&
                               abort_now_w;

  function automatic logic [31:0] sat_add_drop2(
      input logic [31:0] current,
      input logic  [1:0] delta);
    logic [32:0] sum;
    begin
      sum = {1'b0, current} + {{31{1'b0}}, delta};
      sat_add_drop2 = (sum >= 33'h0ffff_ffff) ? 32'hffff_ffff : sum[31:0];
    end
  endfunction

  // -------------------------------------------------------------------------
  // Sequential ownership.  Counters are reset-zero, saturating and never clear
  // on a recoverable frame rebase.
  always_ff @(posedge clk or negedge rst_n) begin : p_packet_d_state
    if (!rst_n) begin
      rs_state_q <= RS_IDLE;
      start_delivered_q <= {START_N{1'b1}};
      ax_q <= 21'sd0; ay_q <= 21'sd0;
      bx_q <= 21'sd0; by_q <= 21'sd0;
      cx_q <= 21'sd0; cy_q <= 21'sd0;
      tile_x_q <= 12'sd0; tile_y_q <= 12'sd0;
      tile_index_q <= 16'd0; source_id_q <= 16'd0;
      first_q <= 1'b1; last_q <= 1'b1;
      clear_word_q <= 64'd0;
      flat_request_q <= 298'd0;
      continuation_tail_bits_q <= 48'd0;
      fragment_state_q <= 32'd0;
      area2_q <= 47'd0;
      min_x_q <= 12'sd0;
      for (int lane = 0; lane < ATTR_LANES; lane++) begin
        plane_n0_q[lane] <= 96'sd0;
        plane_dndx_q[lane] <= 72'sd0;
        plane_dndy_q[lane] <= 72'sd0;
      end

      row_hold_valid_q <= 1'b0;
      row_hold_row_q <= 4'd0;
      row_hold_mask_q <= 16'd0;
      row_hold_last_q <= 1'b0;
      row_delivered_q <= '0;
      saw_coverage_q <= 1'b0;
      ew_done_q <= 1'b1;
      ew_count_q <= 9'd0;
      ew_degenerate_q <= 1'b0;

      tile_coverage_acc_q <= 9'd0;
      resolve_tile_x_q <= 12'sd0;
      resolve_tile_y_q <= 12'sd0;
      resolve_coverage_q <= 9'd0;
      resolve_degenerate_q <= 1'b0;

      local_abort_q <= 1'b0;
      local_fault_count_o <= 32'd0;
      coordinate_fault_count_o <= 32'd0;
      range_fault_count_o <= 32'd0;
      aux_profile_fault_count_o <= 32'd0;
      candidate_cancel_count_o <= 32'd0;
      local_drop_count_o <= 32'd0;
      jobs_started_o <= 32'd0;
      jobs_sunk_o <= 32'd0;
    end else begin
      // One held coverage row, with exact per-lane delivery ownership.
      if (row_hold_valid_q) begin
        if (row_retire_w) begin
          row_hold_valid_q <= 1'b0;
          row_delivered_q <= '0;
        end else begin
          row_delivered_q <= row_delivered_next_w;
        end
      end else if (ew_cov_valid_w && ew_cov_ready_w) begin
        row_hold_valid_q <= 1'b1;
        row_hold_row_q <= ew_cov_row_w;
        row_hold_mask_q <= ew_cov_mask_w;
        row_hold_last_q <= ew_cov_last_w;
        row_delivered_q <= '0;
        saw_coverage_q <= 1'b1;
      end else if (ew_done_w && !saw_coverage_q) begin
        // Empty/degenerate walks have no EDGEWALK beat.  A terminal zero-mask row
        // retires all six frozen attr jobs without producing a candidate.
        row_hold_valid_q <= 1'b1;
        row_hold_row_q <= 4'd0;
        row_hold_mask_q <= 16'd0;
        row_hold_last_q <= 1'b1;
        row_delivered_q <= '0;
      end

      if (ew_done_w) begin
        ew_done_q <= 1'b1;
        ew_count_q <= ew_count_w;
        ew_degenerate_q <= ew_degenerate_w;
      end

      // Exact frozen metadata unpack, once, on the accepted job identity. This
      // is payload only: it starts nothing and counts nothing, which is why it
      // needs no combinational abort verdict in its enable.
      if (job_metadata_capture_w) begin
        ax_q <= job_ax_i; ay_q <= job_ay_i;
        bx_q <= job_bx_i; by_q <= job_by_i;
        cx_q <= job_cx_i; cy_q <= job_cy_i;
        tile_x_q <= job_tile_x_i; tile_y_q <= job_tile_y_i;
        tile_index_q <= job_tile_index_i;
        source_id_q <= job_src_id_i;
        first_q <= job_first_i;
        last_q <= job_last_i;
        clear_word_q <= frame_clear_word_i;
        flat_request_q <= incoming_flat_request_w;
        continuation_tail_bits_q <= incoming_continuation_tail_w;
        fragment_state_q <= incoming_fragment_state_w;
        area2_q <= incoming_area2_w;
        min_x_q <= incoming_min_x_w;
        for (int lane = 0; lane < ATTR_LANES; lane++) begin
          plane_dndy_q[lane] <= incoming_plane_dndy_w[lane];
          plane_dndx_q[lane] <= incoming_plane_dndx_w[lane];
          plane_n0_q[lane] <= incoming_plane_n0_w[lane];
        end
      end

      if (job_accept_w && abort_now_w) begin
        if (jobs_sunk_o != 32'hffff_ffff)
          jobs_sunk_o <= jobs_sunk_o + 32'd1;
      end else if (job_accept_w) begin
        row_hold_valid_q <= 1'b0;
        row_delivered_q <= '0;
        // A non-first job pre-marks ONLY the tilestore-clear destination as
        // delivered: the clear is issued once per tile, not once per job.
        start_delivered_q <= job_first_i
            ? {START_N{1'b0}}
            : (START_N'(1) << START_CLEAR);
        saw_coverage_q <= 1'b0;
        ew_count_q <= 9'd0;
        ew_degenerate_q <= 1'b0;
        if (profile_aux_bad_w || profile_area_bad_w) begin
          ew_done_q <= 1'b1;
          rs_state_q <= RS_IDLE;
        end else begin
          ew_done_q <= 1'b0;
          rs_state_q <= RS_START;
          if (jobs_started_o != 32'hffff_ffff)
            jobs_started_o <= jobs_started_o + 32'd1;
          if (job_first_i) tile_coverage_acc_q <= 9'd0;
        end
      end

      case (rs_state_q)
        RS_START: begin
          start_delivered_q <= start_delivered_next_w;
          if (start_complete_w) rs_state_q <= RS_ACTIVE;
        end

        RS_ACTIVE: begin
          if (ordinary_pipe_empty_w && !abort_now_w) begin
            tile_coverage_acc_q <= tile_coverage_acc_q + ew_count_q;
            rs_state_q <= last_q ? RS_SWAP : RS_IDLE;
          end
        end

        RS_SWAP: begin
          if (ts_swap_w && ts_swap_ready_w) begin
            resolve_tile_x_q <= tile_x_q;
            resolve_tile_y_q <= tile_y_q;
            resolve_coverage_q <= tile_coverage_acc_q;
            resolve_degenerate_q <= ew_degenerate_q;
            rs_state_q <= RS_IDLE;
          end
        end

        default: begin end
      endcase

      // Any terminal indication preempts ordinary completion/swap.  Producer
      // state is not reset; RS_IDLE plus abort_now is the draining/sink state.
      if (abort_now_w && !clear_fire_w &&
          ((rs_state_q != RS_START) || start_complete_w))
        rs_state_q <= RS_IDLE;

      if (local_fault_event_w) local_abort_q <= 1'b1;
      else if (clear_fire_w) local_abort_q <= 1'b0;

      if (local_fault_event_w && local_fault_count_o != 32'hffff_ffff)
        local_fault_count_o <= local_fault_count_o + 32'd1;
      if (coordinate_fault_event_w && coordinate_fault_count_o != 32'hffff_ffff)
        coordinate_fault_count_o <= coordinate_fault_count_o + 32'd1;
      if (range_fault_event_w && range_fault_count_o != 32'hffff_ffff)
        range_fault_count_o <= range_fault_count_o + 32'd1;
      if (aux_profile_fault_event_w && aux_profile_fault_count_o != 32'hffff_ffff)
        aux_profile_fault_count_o <= aux_profile_fault_count_o + 32'd1;
      if (skid_cancel_fire_w && candidate_cancel_count_o != 32'hffff_ffff)
        candidate_cancel_count_o <= candidate_cancel_count_o + 32'd1;
      if (joined_attr_drop_w || earlyz_abort_drop_w)
        local_drop_count_o <= sat_add_drop2(
            local_drop_count_o,
            {1'b0, joined_attr_drop_w} + {1'b0, earlyz_abort_drop_w});
    end
  end

  // synthesis translate_off
  logic held_row_q;
  logic [20:0] held_row_payload_q;
  logic held_attr_source_q;
  logic [ATTR_LANES*43-1:0] held_attr_source_payload_q;
  logic held_attr_bundle_q;
  logic [ATTR_LANES*43-1:0] held_attr_bundle_payload_q;
  logic held_earlyz_q;
  logic [490:0] held_earlyz_payload_q;
  logic held_stage_fragment_q;
  logic [175:0] held_stage_fragment_payload_q;
  logic held_tile_write_q;
  logic [71:0] held_tile_write_payload_q;
  // 43 bits a lane: {q[31:0], row[3:0], col[3:0], last, sat, error}.
  logic [ATTR_LANES*43-1:0] attr_source_payload_w;
  logic [ATTR_LANES*43-1:0] attr_bundle_payload_w;
  logic [490:0] earlyz_hold_payload_w;
  logic [175:0] stage_fragment_payload_w;

  // The registered restatement of `incoming_coordinate_bad_c`, over the join
  // registers rather than the incoming lanes. It exists only so the D2 refactor
  // guard below can difference the two; keeping it as one loop rather than
  // fifteen hand-written comparisons is what stops it drifting from the
  // expression it is supposed to mirror.
  logic joined_coordinate_bad_c;
  always_comb begin : p_attr_joined_coord
    logic [3:0] lane_col_c;
    joined_coordinate_bad_c = 1'b0;
    for (int lane = 1; lane < ATTR_LANES; lane++) begin
      lane_col_c = (lane == LANE_UOW) ? lane1_col_checked_w
                                      : attr_join_col_q[lane];
      if ((attr_join_row_q[0] != attr_join_row_q[lane]) ||
          (attr_join_col_q[0] != lane_col_c) ||
          (attr_join_last_q[0] != attr_join_last_q[lane]))
        joined_coordinate_bad_c = 1'b1;
    end
  end

  always_comb begin : p_attr_hold_payloads
    attr_source_payload_w = '0;
    attr_bundle_payload_w = '0;
    for (int lane = 0; lane < ATTR_LANES; lane++) begin
      attr_source_payload_w[43*(ATTR_LANES-1-lane) +: 43] = {
          attr_q_w[lane], attr_row_w[lane], attr_col_w[lane],
          attr_last_w[lane], attr_sat_w[lane], attr_error_w[lane]};
      attr_bundle_payload_w[43*(ATTR_LANES-1-lane) +: 43] = {
          attr_join_q_q[lane], attr_join_row_q[lane], attr_join_col_q[lane],
          attr_join_last_q[lane], attr_join_sat_q[lane],
          attr_join_error_q[lane]};
    end
  end
  assign earlyz_hold_payload_w = {
      earlyz_cand_addr_w, earlyz_cand_depth_w, earlyz_cand_state_w,
      earlyz_cand_source_w, earlyz_cand_payload_w};
  assign stage_fragment_payload_w = {
      stage_fragment_addr_o, stage_fragment_depth_o,
      stage_fragment_state_o, stage_fragment_src_id_o,
      stage_frag_vert_rgb_w, stage_frag_vert_a_w, stage_frag_tag_w,
      stage_frag_stencil_w, stage_fragment_texel_rgb_o,
      stage_fragment_texel_a_o, stage_fragment_texel_idx_o,
      stage_fragment_status_o};

  always_ff @(posedge clk or negedge rst_n) begin : p_packet_d_assertions
    if (!rst_n) begin
      held_row_q <= 1'b0;
      held_row_payload_q <= 21'd0;
      held_attr_source_q <= 1'b0;
      held_attr_source_payload_q <= '0;
      held_attr_bundle_q <= 1'b0;
      held_attr_bundle_payload_q <= '0;
      held_earlyz_q <= 1'b0;
      held_earlyz_payload_q <= 491'd0;
      held_stage_fragment_q <= 1'b0;
      held_stage_fragment_payload_q <= 176'd0;
      held_tile_write_q <= 1'b0;
      held_tile_write_payload_q <= 72'd0;
    end else begin
      if (held_row_q && (!row_hold_valid_q ||
          ({row_hold_row_q, row_hold_mask_q, row_hold_last_q} != held_row_payload_q)))
        $fatal(1, "Packet-D coverage row changed before all lanes accepted");
      if (held_attr_source_q && (!attr_source_valid_w ||
          (attr_source_payload_w != held_attr_source_payload_q)))
        $fatal(1, "Packet-D source attribute bundle changed before join capture");
      if (held_attr_bundle_q && (!attr_bundle_valid_w ||
          (attr_bundle_payload_w != held_attr_bundle_payload_q)))
        $fatal(1, "Packet-D registered attribute join changed under backpressure");
      // TIMING4 D2 invariant, stated for what it is: this cannot fire while
      // the two enables are the one expression they are today, because the
      // captured verdict and the captured payload are loaded together from the
      // same sources. It is not a fault detector and is not offered as one.
      // It guards the REFACTOR: the moment a later edit gives the verdict its
      // own enable, or moves one of these loads, the registered verdict stops
      // describing the payload beside it and this says so on the first bundle.
      if (attr_bundle_valid_w &&
          ((attr_coordinate_bad_q != joined_coordinate_bad_c) ||
           (attr_range_bad_q !=
            ((|attr_join_error_q) || attr_join_q_q[LANE_INVW][31] ||
             (attr_join_q_q[LANE_INVW][31:24] != 8'd0)))))
        $fatal(1, "Packet-D captured attribute verdict disagrees with the join it describes");
      if (held_earlyz_q && (!earlyz_cand_valid_w ||
          (earlyz_hold_payload_w != held_earlyz_payload_q)))
        $fatal(1, "Packet-D Early-Z candidate changed under backpressure");
      if (held_stage_fragment_q && (!stage_fragment_valid_o ||
          (stage_fragment_payload_w != held_stage_fragment_payload_q)))
        $fatal(1, "Packet-D stage fragment changed under backpressure");
      if (held_tile_write_q && (!ts_wr_w ||
          ({ts_wr_addr_w, ts_wr_data_w} != held_tile_write_payload_q)))
        $fatal(1, "Packet-D tile write changed under backpressure");
      if (|(start_fire_w & start_delivered_q))
        $fatal(1, "Packet-D job-start destination accepted twice");
      if (start_fire_w[START_CLEAR] && !first_q)
        $fatal(1, "Packet-D issued clear for a non-first tile job");
      if (skid_cancel_fire_w && !abort_now_w)
        $fatal(1, "Packet-D counted a non-abort skid transfer as cancellation");
      if (ts_swap_w && abort_now_w)
        $fatal(1, "Packet-D started a swap during terminal drain");
      if (unused_shadow_present)
        $fatal(1, "Packet-D MIGRATION_SHADOWS=0 exposed shadow state");

      held_row_q <= row_hold_valid_q && !row_retire_w;
      if (row_hold_valid_q && !row_retire_w)
        held_row_payload_q <= {row_hold_row_q, row_hold_mask_q, row_hold_last_q};
      held_attr_source_q <= attr_source_valid_w && !attr_join_capture_w;
      if (attr_source_valid_w && !attr_join_capture_w)
        held_attr_source_payload_q <= attr_source_payload_w;
      held_attr_bundle_q <= attr_bundle_valid_w && !attr_join_consume_w;
      if (attr_bundle_valid_w && !attr_join_consume_w)
        held_attr_bundle_payload_q <= attr_bundle_payload_w;
      held_earlyz_q <= earlyz_cand_valid_w && !earlyz_cand_ready_w;
      if (earlyz_cand_valid_w && !earlyz_cand_ready_w)
        held_earlyz_payload_q <= earlyz_hold_payload_w;
      held_stage_fragment_q <= stage_fragment_valid_o && !stage_frag_ready_w;
      if (stage_fragment_valid_o && !stage_frag_ready_w)
        held_stage_fragment_payload_q <= stage_fragment_payload_w;
      held_tile_write_q <= ts_wr_w && !ts_wr_ready_w;
      if (ts_wr_w && !ts_wr_ready_w)
        held_tile_write_payload_q <= {ts_wr_addr_w, ts_wr_data_w};
    end
  end
  // synthesis translate_on

  // Explicit sink for diagnostic-only leaf outputs.  Their durable selected
  // counters are promoted above; this reduction has no datapath authority.
  logic unused_ok;
  logic unused_lane_ok;
  always_comb begin : p_unused_lane_counters
    unused_lane_ok = 1'b0;
    for (int lane = 0; lane < ATTR_LANES; lane++)
      unused_lane_ok = unused_lane_ok ^ ^{attr_pixels_w[lane],
                                          attr_divides_w[lane],
                                          attr_saturations_w[lane],
                                          attr_divide_errors_w[lane]};
  end
  always_comb begin
    unused_ok = ^{1'b0, ew_cov_source_w, attr_sat_w, attr_join_sat_q,
                  unused_lane_ok,
                  earlyz_cand_bin_w, earlyz_reject_w, earlyz_reject_addr_w,
                  ts_clear_ready_w, ts_wr_ready_w, ts_rd_ready_w, ts_rd_source_w,
                  ts_swap_ready_w, unused_err_fragrob_wq_overflow,
                  unused_err_fragrob_id_error, unused_err_aux_degenerate,
                  unused_err_rcp_q, unused_cnt_reorder_held, unused_cnt_live_peak,
                  unused_cnt_bilerp_jobs, unused_cnt_mosaic_samples,
                  unused_cnt_aux_accepted, unused_cnt_combine_phases,
                  unused_cnt_rcp_completed, unused_cnt_persp_fragments,
                  unused_cnt_fragrob_id_errors, unused_shadow_present,
                  unused_meta_shadow_mismatch, unused_meta_shadow_reads,
                  unused_meta_align_err, unused_meta_align_chk,
                  unused_meta_bil_err, unused_meta_bil_chk,
                  unused_meta_near_err, unused_meta_near_chk,
                  unused_meta_bil_first_q, unused_meta_bil_first_t,
                  unused_meta_bil_first_tok, unused_meta_genmis,
                  unused_cnt_combine_jobs[0], unused_cnt_combine_jobs[1],
                  unused_cnt_combine_jobs[2], unused_cnt_combine_jobs[3],
                  unused_cnt_combine_jobs[4], unused_cnt_combine_jobs[5],
                  unused_cnt_combine_jobs[6], unused_cnt_combine_jobs[7],
                  unused_cnt_palette_stale, unused_cnt_palette_cold,
                  unused_err_rsp_dropped, unused_err_bil_chan,
                  unused_cnt_near_refused, unused_err_unknown_class,
                  unused_err_class_invalid, unused_err_palette_unusable,
                  unused_err_class_mismatch, unused_err_plan_mode};
  end

endmodule : zhao_raster_tile_pipe_v2

`undef ZHAO_PACKET_D_LANE1_COL
`undef ZHAO_PACKET_D_PIPE_V3_QUIET
`undef ZHAO_PACKET_D_SKID_DN_READY
`default_nettype wire
