// zhao_geom_bin_pipe_v2.sv -- Packet-D GEOM.BINNER V2 to raster-tile V2.
//
// The 1,877-bit metadata record is constructed once from the accepted setup
// triangle's area/min-X/SIX planes and flat typed-material fields, then stored
// by zhao_geom_binner_v2 under the binner's own triangle identity.  Drained jobs
// splice that exact record into zhao_raster_tile_pipe_v2.  After raster abort,
// the tile pipe's sink-ready drains every remaining binner job without starting
// another tile.
//
// AUTHORITY: reports/PACKET-D-ATTRIBUTE-RASTER-ABI-20260914.md
`default_nettype none

module zhao_geom_bin_pipe_v2 #(
    parameter int unsigned GRID_W     = 24,
    parameter int unsigned GRID_H     = 24,
    parameter int unsigned TILES      = GRID_W * GRID_H,
    parameter int unsigned TIDX_W     = 10,
    parameter int unsigned TRI_CAP    = 128,
    parameter int unsigned TRI_W      = 7,
    parameter int unsigned CHUNKS     = 256,
    parameter int unsigned CHUNK_W    = 8,
    parameter int unsigned CHUNK_REFS = 4,
    parameter bit ATTR_DSP3           = 1'b0,
    parameter bit BILERP_DSP2         = 1'b0,
    // The arena's TriangleDescriptor index width (`td_id_o` is u18). Console
    // entry I54; see ARENA_ID_LO below for WHERE in the metadata it rides.
    parameter int unsigned ARENA_ID_W = 18
) (
    input  logic clk,
    input  logic rst_n,

    input  logic               frame_begin_i,
    input  logic               frame_end_i,
    input  logic        [5:0]  grid_w_i,
    input  logic        [5:0]  grid_h_i,
    input  logic        [63:0] frame_clear_word_i,

    // GEOM.SETUP triangle plus Packet-D attribute/material record.
    input  logic               tri_valid_i,
    output logic               tri_ready_o,
    input  logic signed [22:0] tri_kx0_i,
    input  logic signed [22:0] tri_ky0_i,
    input  logic signed [47:0] tri_kc0_i,
    input  logic signed [22:0] tri_kx1_i,
    input  logic signed [22:0] tri_ky1_i,
    input  logic signed [47:0] tri_kc1_i,
    input  logic signed [22:0] tri_kx2_i,
    input  logic signed [22:0] tri_ky2_i,
    input  logic signed [47:0] tri_kc2_i,
    input  logic        [2:0]  tri_tl_i,
    input  logic signed [20:0] tri_ax_i,
    input  logic signed [20:0] tri_ay_i,
    input  logic signed [20:0] tri_bx_i,
    input  logic signed [20:0] tri_by_i,
    input  logic signed [20:0] tri_cx_i,
    input  logic signed [20:0] tri_cy_i,
    input  logic signed [11:0] tri_min_x_i,
    input  logic signed [11:0] tri_max_x_i,
    input  logic signed [11:0] tri_min_y_i,
    input  logic signed [11:0] tri_max_y_i,
    input  logic        [15:0] tri_src_id_i,
    input  logic        [46:0] tri_area2_i,
    input  logic       [239:0] tri_invw_plane_i,
    input  logic       [239:0] tri_u_over_w_plane_i,
    input  logic       [239:0] tri_v_over_w_plane_i,
    // THE GOURAUD PLANES (owner decision R234 D1, 2026-09-21). GEOM.ATTRPACK's
    // lanes 3..5, carrying the lit per-vertex colour that GEOM.LIGHT computes,
    // GEOM.VATTR stores and GEOM.CLIP winding-flips. They widened METAW from
    // 1157 to 1877, which is 29 -> 47 forty-bit slices of the binner's
    // metadata bank -- the M10K half of the decision's price.
    input  logic       [239:0] tri_r_plane_i,
    input  logic       [239:0] tri_g_plane_i,
    input  logic       [239:0] tri_b_plane_i,
    input  logic       [297:0] tri_flat_request_i,
    input  logic        [47:0] tri_continuation_tail_i,
    input  logic        [31:0] tri_fragment_state_i,

    output logic               tok_req_o,
    input  logic               tok_grant_i,

    // Recoverable frame terminal clear, accepted only after binner and tile quiet.
    input  logic               frame_fault_clear_valid_i,
    output logic               frame_fault_clear_ready_o,
    output logic               frame_fault_o,
    output logic               lifetime_structural_fault_o,

    // Packet-B binding-page loader.
    input  logic               cfg_valid_i,
    output logic               cfg_ready_o,
    input  logic        [1:0]  cfg_op_i,
    input  logic        [7:0]  cfg_page_generation_i,
    input  logic        [7:0]  cfg_selector_i,
    input  logic        [74:0] cfg_row_i,
    input  logic        [31:0] cfg_crc32_i,
    output logic               cfg_rsp_valid_o,
    input  logic               cfg_rsp_ready_i,
    output logic        [1:0]  cfg_rsp_op_o,
    output logic        [3:0]  cfg_rsp_status_o,
    output logic        [7:0]  cfg_rsp_page_generation_o,
    output logic        [7:0]  active_page_generation_o,

    output logic               fill_req_valid_o,
    input  logic               fill_req_ready_i,
    output logic        [31:0] fill_req_addr_o,
    input  logic               fill_data_valid_i,
    input  logic        [15:0] fill_data_i,
    input  logic               fill_refused_i,

    input  logic               pal_load_valid_i,
    output logic               pal_load_ready_o,
    input  logic        [1:0]  pal_load_op_i,
    input  logic        [1:0]  pal_load_slot_i,
    input  logic        [7:0]  pal_load_gen_i,
    input  logic        [7:0]  pal_load_idx_i,
    input  logic        [15:0] pal_load_rgb565_i,
    input  logic               pal_load_crc_ok_i,

    output logic               sheet_req_valid_o,
    input  logic               sheet_req_ready_i,
    output logic        [1:0]  sheet_req_op_o,
    output logic        [31:0] sheet_req_handle_o,
    output logic        [11:0] sheet_req_texel_o,
    output logic        [15:0] sheet_req_src_id_o,
    input  logic               pg_valid_i,
    output logic               pg_ready_o,
    input  logic        [1:0]  pg_op_i,
    input  logic        [1:0]  pg_status_i,
    input  logic        [7:0]  pg_tag_i,
    input  logic        [7:0]  pg_strength_i,
    input  logic        [15:0] pg_src_id_i,

    // Resolved framebuffer stream.
    output logic               fb_valid_o,
    input  logic               fb_ready_i,
    output logic        [15:0] fb_rgb565_o,
    output logic        [7:0]  fb_tag_o,
    output logic        [7:0]  fb_addr_o,
    output logic signed [11:0] fb_x_o,
    output logic signed [11:0] fb_y_o,
    output logic               fb_last_o,
    output logic        [15:0] fb_src_id_o,

    output logic        [31:0] tile_crc_o,
    output logic        [15:0] tile_crc_index_o,
    output logic               tile_done_o,
    output logic        [8:0]  tile_cov_count_o,
    output logic               tile_degenerate_o,

    // Full binner and seam observability.
    output logic               drain_busy_o,
    output logic               drain_done_o,
    output logic               binner_initialized_o,
    output logic        [31:0] binner_tile_references_o,
    output logic        [15:0] binner_max_tile_list_depth_o,
    output logic        [31:0] binner_triangles_culled_o,
    output logic               binner_overflow_o,
    output logic               binner_arena_full_o,
    output logic [CHUNK_W:0]   binner_arena_used_o,
    output logic        [31:0] jobs_taken_o,
    output logic        [31:0] job_stall_clocks_o,

    // ---- the binner's SERIALISE PASS, exported (console entry I54) --------
    // The chunk serialiser lives beside `zhao_geom_paramarena` in the console,
    // not in here, so what crosses this boundary is the lean reference stream
    // and not a 448-bit chunk. See `zhao_geom_binner_v2`'s own port comment
    // for why the pass exists and why it is not a tap on `job_*`.
    input  logic               ser_req_i,
    output logic               ser_busy_o,
    output logic               ser_done_o,
    output logic               ser_valid_o,
    input  logic               ser_ready_i,
    output logic [ARENA_ID_W-1:0] ser_tri_id_o,
    output logic [TIDX_W-1:0]  ser_tile_o,
    output logic               ser_first_o,
    output logic               ser_last_o,

    // Raster/texture/fragment/resolve counters and terminal status.
    output logic               quiet_o,
    output logic               raster_abort_o,
    output logic               local_attribute_abort_o,
    output logic               local_fault_pulse_o,
    output logic        [31:0] local_fault_count_o,
    output logic        [31:0] coordinate_fault_count_o,
    output logic        [31:0] range_fault_count_o,
    output logic        [31:0] aux_profile_fault_count_o,
    output logic        [31:0] candidate_cancel_count_o,
    output logic        [31:0] local_drop_count_o,
    output logic        [31:0] raster_jobs_started_o,
    output logic        [31:0] raster_jobs_sunk_o,
    output logic               sequence_abort_o,
    output logic               sequence_mismatch_o,
    output logic        [31:0] sequence_drop_count_o,
    output logic        [31:0] admission_sequence_o,
    output logic        [31:0] expected_sequence_o,
    output logic        [31:0] returned_sequence_o,
    output logic               packet_c_cand_fire_o,
    output logic               packet_c_fragment_fire_o,
    output logic               packet_c_drop_fire_o,
    output logic        [31:0] tilestore_references_o,
    output logic        [31:0] resolved_tiles_o,
    output logic        [31:0] early_z_rejects_o,
    output logic        [31:0] early_z_covered_o,
    output logic        [31:0] fragment_covered_o,
    output logic        [31:0] blended_fragments_o,
    output logic        [31:0] texture_fragments_o,
    output logic        [31:0] texture_cache_hits_o,
    output logic        [31:0] texture_cache_misses_o,
    output logic        [31:0] texture_palette_lookups_o,
    output logic        [31:0] texture_plan_accepted_o,
    output logic        [31:0] texture_dispatch_accepted_o,
    output logic        [31:0] texture_combine_refused_o,
    // Entry I49: TMU samples PUBLISHED into a fragment, straight through from
    // the tile pipe. The one counter that answers 'did the island sample'.
    output logic        [31:0] texture_samples_o,
    output logic               fragment_error_o,

    // Structural probes for the directed gate and committed mutants.
    output logic               coverage_hold_valid_o,
    output logic        [5:0]  coverage_delivered_mask_o,
    output logic        [7:0]  start_delivered_mask_o,
    output logic        [5:0]  attribute_idle_o,
    output logic               earlyz_hold_valid_o,
    output logic        [1:0]  skid_level_o,
    output logic               stage_candidate_valid_o,
    output logic       [489:0] stage_candidate_data_o,
    output logic               stage_fragment_valid_o,
    output logic        [7:0]  stage_fragment_addr_o,
    output logic        [23:0] stage_fragment_depth_o,
    output logic        [31:0] stage_fragment_state_o,
    output logic        [15:0] stage_fragment_src_id_o,
    output logic        [23:0] stage_fragment_texel_rgb_o,
    output logic        [7:0]  stage_fragment_texel_a_o,
    output logic        [7:0]  stage_fragment_texel_idx_o,
    output logic        [7:0]  stage_fragment_status_o,
    output logic               texture_quiet_o,
    output logic               fragment_idle_o,
    output logic               front_bank_o,
    output logic        [7:0]  bin_mask_o,
    output logic        [23:0] z_floor_o
`ifdef ZHAO_PACKET_D_TEST_HOOKS
    , input logic        [7:0]  test_start_enable_i
    , input logic        [5:0]  test_attr_cov_enable_i
    , input logic               test_stage_admit_enable_i
`endif
);

  // THE METADATA ABI, and why this arithmetic is spelled out rather than
  // written as a literal. It was `1157` for three planes; R234 D1 bought three
  // more and it is `1877`. Stating the sum makes the next change one term, and
  // makes the 720-bit step visible to a reader instead of being a number that
  // moved for no stated reason.
  localparam int unsigned META_PLANES   = 6;
  localparam int unsigned META_PLANE_W  = 240;   // {n0[95:0], dndx[71:0], dndy[71:0]}
  localparam int unsigned META_FIXED_W  = 298    // tri_flat_request_i
                                        +  48    // tri_continuation_tail_i
                                        +  32    // tri_fragment_state_i
                                        +  47    // tri_area2_i
                                        +  12;   // tri_min_x_i
  localparam int unsigned METAW = META_FIXED_W + META_PLANES * META_PLANE_W;

  // ---- WHERE THE ARENA'S TRIANGLE INDEX RIDES, AND WHY IT COSTS NOTHING ---
  // Console entry I54 needs `zhao_geom_paramarena`'s TriangleDescriptor index
  // to reach the binner's triangle store, because a chunk of BINNER SLOTS
  // would decode cleanly into the wrong triangles with every range guard
  // passing. The 142-bit triangle record has no free field and `tri_src_id_i`
  // is a per-DRAW instance id, so the metadata is the only carrier.
  //
  // IT NEEDS NO NEW BITS. `tri_continuation_tail_i[47:24]` is the
  // `vertex_rgb` field, and owner decision R234 D1 made
  // `zhao_raster_tile_pipe_v2` OVERWRITE it per fragment from attribute lanes
  // 3..5 -- the console's own comment at `zhao_console_core.sv` says the 24
  // bits are "DEAD on arrival, whatever is put in them" and drives them with
  // an explicit zero constant. Eighteen of those twenty-four now carry the
  // arena index: METAW does not move, the ratified 1877 guard below still
  // holds, the bank gains no slice, and no port crosses the shell for it.
  //
  // THE TAIL SITS ABOVE `tri_flat_request_i` IN THE CONCATENATION, so the
  // offset is the flat-request width plus the field's own offset inside the
  // tail. Stated as a sum for the same reason METAW is: one term moves when
  // the layout does.
  localparam int unsigned META_TAIL_LO      = 298;  // above tri_flat_request_i
  localparam int unsigned TAIL_ARENA_ID_LO  = 24;   // the dead vertex_rgb field
  localparam int unsigned ARENA_ID_LO       = META_TAIL_LO + TAIL_ARENA_ID_LO;

  initial begin : p_packet_d_meta_contract
    if ($bits(tri_meta_w) != METAW)
      $fatal(1, "zhao_geom_bin_pipe_v2: metadata width changed");
    if (METAW != 1877)
      $fatal(1, "zhao_geom_bin_pipe_v2: METAW is not the ratified 1877");
    // The arena index must land inside the DEAD vertex_rgb field and nowhere
    // else. If the tail layout ever moves, this fails elaboration rather than
    // quietly slicing eighteen bits out of a live attribute -- which would
    // produce triangle ids that are wrong and in range, the exact fault
    // entry I54 is written against.
    if ((TAIL_ARENA_ID_LO + ARENA_ID_W) > 48)
      $fatal(1, "zhao_geom_bin_pipe_v2: arena id overruns the continuation tail");
    if ($bits(tri_continuation_tail_i) != 48)
      $fatal(1, "zhao_geom_bin_pipe_v2: continuation tail width changed");
  end

  // Exact ABI concatenation, MSB to LSB.  zhao_geom_binner_v2 samples it only on
  // the same tri_we edge that stores the corresponding 142-bit triangle.
  //
  // THE PLANE ORDER IS THE LANE ORDER and it is load-bearing: the tile pipe
  // unpacks plane k at `437 + 240*k`, so appending the Gouraud planes ABOVE
  // v/w keeps lanes 0..2 bit-identical to the three-plane layout. That is why
  // the r/g/b planes are the new MOST significant fields rather than being
  // inserted anywhere tidier.
  logic [METAW-1:0] tri_meta_w;
  assign tri_meta_w = {tri_b_plane_i,
                       tri_g_plane_i,
                       tri_r_plane_i,
                       tri_v_over_w_plane_i,
                       tri_u_over_w_plane_i,
                       tri_invw_plane_i,
                       tri_min_x_i,
                       tri_area2_i,
                       tri_fragment_state_i,
                       tri_continuation_tail_i,
                       tri_flat_request_i};

  logic [63:0] frame_clear_word_q;
  logic frame_inflight_q;

  // Binner drain splice.
  logic job_valid_w, job_ready_w;
  logic signed [20:0] job_ax_w, job_ay_w, job_bx_w, job_by_w, job_cx_w, job_cy_w;
  logic job_first_w, job_last_w;
  logic signed [11:0] job_tile_x_w, job_tile_y_w;
  logic [15:0] job_source_w;
  logic [METAW-1:0] job_meta_w;
  // bit 0 aux-profile bad, bit 1 area-profile bad -- decided by the binner at
  // WRITE and carried in the metadata bank's pad, so the tile pipe reads a
  // verdict instead of reducing 272 bits of it off the RAM output.
  logic [1:0] job_profile_bad_w;
  logic [15:0] job_tile_index_w;

  zhao_geom_binner_v2 #(
      .GRID_W(GRID_W), .GRID_H(GRID_H), .TILES(TILES), .TIDX_W(TIDX_W),
      .TRI_CAP(TRI_CAP), .TRI_W(TRI_W), .CHUNKS(CHUNKS),
      .CHUNK_W(CHUNK_W), .CHUNK_REFS(CHUNK_REFS), .METAW(METAW),
      .ARENA_ID_LO(ARENA_ID_LO), .ARENA_ID_W(ARENA_ID_W)
  ) u_binner (
      .clk(clk),
      .rst_n(rst_n),
      .frame_begin_i(frame_begin_i),
      .frame_end_i(frame_end_i),
      .grid_w_i(grid_w_i),
      .grid_h_i(grid_h_i),
      .tri_valid_i(tri_valid_i),
      .tri_ready_o(tri_ready_o),
      .tri_kx0_i(tri_kx0_i), .tri_ky0_i(tri_ky0_i), .tri_kc0_i(tri_kc0_i),
      .tri_kx1_i(tri_kx1_i), .tri_ky1_i(tri_ky1_i), .tri_kc1_i(tri_kc1_i),
      .tri_kx2_i(tri_kx2_i), .tri_ky2_i(tri_ky2_i), .tri_kc2_i(tri_kc2_i),
      .tri_tl_i(tri_tl_i),
      .tri_ax_i(tri_ax_i), .tri_ay_i(tri_ay_i),
      .tri_bx_i(tri_bx_i), .tri_by_i(tri_by_i),
      .tri_cx_i(tri_cx_i), .tri_cy_i(tri_cy_i),
      .tri_min_x_i(tri_min_x_i), .tri_max_x_i(tri_max_x_i),
      .tri_min_y_i(tri_min_y_i), .tri_max_y_i(tri_max_y_i),
      .tri_src_id_i(tri_src_id_i),
      .tri_meta_i(tri_meta_w),
      .tok_req_o(tok_req_o),
      .tok_grant_i(tok_grant_i),
      .job_valid_o(job_valid_w),
      .job_ready_i(job_ready_w),
      .job_ax_o(job_ax_w), .job_ay_o(job_ay_w),
      .job_bx_o(job_bx_w), .job_by_o(job_by_w),
      .job_cx_o(job_cx_w), .job_cy_o(job_cy_w),
      .job_first_o(job_first_w), .job_last_o(job_last_w),
      .job_tile_x_o(job_tile_x_w), .job_tile_y_o(job_tile_y_w),
      .job_src_id_o(job_source_w),
      .job_meta_o(job_meta_w),
      .job_profile_bad_o(job_profile_bad_w),
      .drain_busy_o(drain_busy_o),
      .drain_done_o(drain_done_o),
      .ser_req_i(ser_req_i),
      .ser_busy_o(ser_busy_o),
      .ser_done_o(ser_done_o),
      .ser_valid_o(ser_valid_o),
      .ser_ready_i(ser_ready_i),
      .ser_tri_id_o(ser_tri_id_o),
      .ser_tile_o(ser_tile_o),
      .ser_first_o(ser_first_o),
      .ser_last_o(ser_last_o),
      .tile_references_o(binner_tile_references_o),
      .max_tile_list_depth_o(binner_max_tile_list_depth_o),
      .triangles_culled_o(binner_triangles_culled_o),
      .overflow_o(binner_overflow_o),
      .arena_full_o(binner_arena_full_o),
      .arena_used_o(binner_arena_used_o)
  );

  // Stable capture identity derived from the binner's row-major tile origin.
  assign job_tile_index_w = {4'd0, job_tile_y_w[9:4], job_tile_x_w[9:4]};

  logic tile_quiet_w;
  logic tile_clear_valid_w, tile_clear_ready_w;
  logic tile_frame_fault_w;

  // A clear cannot rebase Packet C/local abort until every binner job has either
  // started normally or been accepted by the abort sink.
  assign tile_clear_valid_w = frame_fault_clear_valid_i &&
                              binner_initialized_o && !frame_inflight_q &&
                              !frame_begin_i;
  assign frame_fault_clear_ready_o = binner_initialized_o &&
                                     !frame_inflight_q && !frame_begin_i &&
                                     tile_clear_ready_w;
  assign frame_fault_o = tile_frame_fault_w;
  assign quiet_o = binner_initialized_o && !frame_inflight_q &&
                   !frame_begin_i && !drain_busy_o && tile_quiet_w;

  zhao_raster_tile_pipe_v2 #(
      .ATTR_DSP3(ATTR_DSP3),
      .BILERP_DSP2(BILERP_DSP2)
  ) u_tile (
      .clk(clk),
      .rst_n(rst_n),
      .job_valid_i(job_valid_w),
      .job_ready_o(job_ready_w),
      .job_ax_i(job_ax_w), .job_ay_i(job_ay_w),
      .job_bx_i(job_bx_w), .job_by_i(job_by_w),
      .job_cx_i(job_cx_w), .job_cy_i(job_cy_w),
      .job_first_i(job_first_w), .job_last_i(job_last_w),
      .job_tile_x_i(job_tile_x_w), .job_tile_y_i(job_tile_y_w),
      .job_tile_index_i(job_tile_index_w),
      .job_src_id_i(job_source_w),
      .job_meta_i(job_meta_w),
      .job_profile_bad_i(job_profile_bad_w),
      .frame_clear_word_i(frame_clear_word_q),
      .frame_fault_clear_valid_i(tile_clear_valid_w),
      .frame_fault_clear_ready_o(tile_clear_ready_w),
      .frame_fault_o(tile_frame_fault_w),
      .lifetime_structural_fault_o(lifetime_structural_fault_o),
      .cfg_valid_i(cfg_valid_i), .cfg_ready_o(cfg_ready_o),
      .cfg_op_i(cfg_op_i), .cfg_page_generation_i(cfg_page_generation_i),
      .cfg_selector_i(cfg_selector_i), .cfg_row_i(cfg_row_i),
      .cfg_crc32_i(cfg_crc32_i), .cfg_rsp_valid_o(cfg_rsp_valid_o),
      .cfg_rsp_ready_i(cfg_rsp_ready_i), .cfg_rsp_op_o(cfg_rsp_op_o),
      .cfg_rsp_status_o(cfg_rsp_status_o),
      .cfg_rsp_page_generation_o(cfg_rsp_page_generation_o),
      .active_page_generation_o(active_page_generation_o),
      .fill_req_valid_o(fill_req_valid_o), .fill_req_ready_i(fill_req_ready_i),
      .fill_req_addr_o(fill_req_addr_o), .fill_data_valid_i(fill_data_valid_i),
      .fill_data_i(fill_data_i), .fill_refused_i(fill_refused_i),
      .pal_load_valid_i(pal_load_valid_i), .pal_load_ready_o(pal_load_ready_o),
      .pal_load_op_i(pal_load_op_i), .pal_load_slot_i(pal_load_slot_i),
      .pal_load_gen_i(pal_load_gen_i), .pal_load_idx_i(pal_load_idx_i),
      .pal_load_rgb565_i(pal_load_rgb565_i),
      .pal_load_crc_ok_i(pal_load_crc_ok_i),
      .sheet_req_valid_o(sheet_req_valid_o),
      .sheet_req_ready_i(sheet_req_ready_i),
      .sheet_req_op_o(sheet_req_op_o),
      .sheet_req_handle_o(sheet_req_handle_o),
      .sheet_req_texel_o(sheet_req_texel_o),
      .sheet_req_src_id_o(sheet_req_src_id_o),
      .pg_valid_i(pg_valid_i), .pg_ready_o(pg_ready_o),
      .pg_op_i(pg_op_i), .pg_status_i(pg_status_i),
      .pg_tag_i(pg_tag_i), .pg_strength_i(pg_strength_i),
      .pg_src_id_i(pg_src_id_i),
      .fb_valid_o(fb_valid_o), .fb_ready_i(fb_ready_i),
      .fb_rgb565_o(fb_rgb565_o), .fb_tag_o(fb_tag_o),
      .fb_addr_o(fb_addr_o), .fb_x_o(fb_x_o), .fb_y_o(fb_y_o),
      .fb_last_o(fb_last_o), .fb_src_id_o(fb_src_id_o),
      .tile_crc_o(tile_crc_o), .tile_crc_index_o(tile_crc_index_o),
      .tile_done_o(tile_done_o), .tile_cov_count_o(tile_cov_count_o),
      .tile_degenerate_o(tile_degenerate_o),
      .front_bank_o(front_bank_o),
      .tilestore_references_o(tilestore_references_o),
      .resolved_tiles_o(resolved_tiles_o),
      .early_z_rejects_o(early_z_rejects_o),
      .early_z_covered_o(early_z_covered_o),
      .fragment_covered_o(fragment_covered_o),
      .blended_fragments_o(blended_fragments_o),
      .bin_mask_o(bin_mask_o), .z_floor_o(z_floor_o),
      .fragment_error_o(fragment_error_o),
      .quiet_o(tile_quiet_w), .raster_abort_o(raster_abort_o),
      .local_attribute_abort_o(local_attribute_abort_o),
      .local_fault_pulse_o(local_fault_pulse_o),
      .local_fault_count_o(local_fault_count_o),
      .coordinate_fault_count_o(coordinate_fault_count_o),
      .range_fault_count_o(range_fault_count_o),
      .aux_profile_fault_count_o(aux_profile_fault_count_o),
      .candidate_cancel_count_o(candidate_cancel_count_o),
      .local_drop_count_o(local_drop_count_o),
      .jobs_started_o(raster_jobs_started_o),
      .jobs_sunk_o(raster_jobs_sunk_o),
      .sequence_abort_o(sequence_abort_o),
      .sequence_mismatch_o(sequence_mismatch_o),
      .sequence_drop_count_o(sequence_drop_count_o),
      .admission_sequence_o(admission_sequence_o),
      .expected_sequence_o(expected_sequence_o),
      .returned_sequence_o(returned_sequence_o),
      .packet_c_cand_fire_o(packet_c_cand_fire_o),
      .packet_c_fragment_fire_o(packet_c_fragment_fire_o),
      .packet_c_drop_fire_o(packet_c_drop_fire_o),
      .texture_fragments_o(texture_fragments_o),
      .texture_cache_hits_o(texture_cache_hits_o),
      .texture_cache_misses_o(texture_cache_misses_o),
      .texture_palette_lookups_o(texture_palette_lookups_o),
      .texture_plan_accepted_o(texture_plan_accepted_o),
      .texture_dispatch_accepted_o(texture_dispatch_accepted_o),
      .texture_combine_refused_o(texture_combine_refused_o),
      .texture_samples_o(texture_samples_o),
      .coverage_hold_valid_o(coverage_hold_valid_o),
      .coverage_delivered_mask_o(coverage_delivered_mask_o),
      .start_delivered_mask_o(start_delivered_mask_o),
      .attribute_idle_o(attribute_idle_o),
      .earlyz_hold_valid_o(earlyz_hold_valid_o),
      .skid_level_o(skid_level_o),
      .stage_candidate_valid_o(stage_candidate_valid_o),
      .stage_candidate_data_o(stage_candidate_data_o),
      .stage_fragment_valid_o(stage_fragment_valid_o),
      .stage_fragment_addr_o(stage_fragment_addr_o),
      .stage_fragment_depth_o(stage_fragment_depth_o),
      .stage_fragment_state_o(stage_fragment_state_o),
      .stage_fragment_src_id_o(stage_fragment_src_id_o),
      .stage_fragment_texel_rgb_o(stage_fragment_texel_rgb_o),
      .stage_fragment_texel_a_o(stage_fragment_texel_a_o),
      .stage_fragment_texel_idx_o(stage_fragment_texel_idx_o),
      .stage_fragment_status_o(stage_fragment_status_o),
      .texture_quiet_o(texture_quiet_o),
      .fragment_idle_o(fragment_idle_o)
`ifdef ZHAO_PACKET_D_TEST_HOOKS
      , .test_start_enable_i(test_start_enable_i)
      , .test_attr_cov_enable_i(test_attr_cov_enable_i)
      , .test_stage_admit_enable_i(test_stage_admit_enable_i)
`endif
  );

  always_ff @(posedge clk or negedge rst_n) begin : p_frame_and_seam
    if (!rst_n) begin
      frame_clear_word_q <= 64'd0;
      frame_inflight_q <= 1'b0;
      binner_initialized_o <= 1'b0;
      jobs_taken_o <= 32'd0;
      job_stall_clocks_o <= 32'd0;
    end else begin
      if (tri_ready_o) binner_initialized_o <= 1'b1;
      if (frame_begin_i) begin
        frame_clear_word_q <= frame_clear_word_i;
        frame_inflight_q <= 1'b1;
      end else if (drain_done_o) begin
        frame_inflight_q <= 1'b0;
      end

      if (job_valid_w && job_ready_w && jobs_taken_o != 32'hffff_ffff)
        jobs_taken_o <= jobs_taken_o + 32'd1;
      if (job_valid_w && !job_ready_w && job_stall_clocks_o != 32'hffff_ffff)
        job_stall_clocks_o <= job_stall_clocks_o + 32'd1;
    end
  end

  // synthesis translate_off
  always_ff @(posedge clk) begin : p_geom_packet_d_assertions
    if (rst_n) begin
      if (frame_fault_clear_ready_o && frame_inflight_q)
        $fatal(1, "Packet-D clear became ready before binner drain completed");
      if (raster_abort_o && job_valid_w && !job_ready_w)
        $fatal(1, "Packet-D abort failed to sink a remaining binner job");
    end
  end
  // synthesis translate_on

endmodule : zhao_geom_bin_pipe_v2

`default_nettype wire
