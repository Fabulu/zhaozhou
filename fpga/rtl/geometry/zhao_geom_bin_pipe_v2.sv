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

    // ---- I55's RASTER DOOR (DOORCOST, 2026-09-27) ------------------------
    //
    // WHICH SOURCE DRIVES `u_tile`'s `job_*`. 0 = the binner's on-chip drain,
    // which is what this console has always done. 1 = the SDRAM walk, through
    // the `walk_job_*` port set below.
    //
    // THE SELECTION IS AT ELABORATION AND THAT IS THE POINT. A run-time 2:1 mux
    // on this bus is 2,047 bits wide and was PRICED before this parameter was
    // written -- `zhao_probe_doorcost_jobmux` measures it, because a refusal
    // without a number is what this packet exists to replace. A `generate`
    // select costs ZERO logic and keeps the old arrangement as a complete,
    // buildable oracle, which is what OWNER_VACATION_DIRECTIVE_2026-09-23.txt
    // section 7 asks for ("retain the correct complete oracle").
    //
    // WHY THIS IS A KNOB AND NOT A REWRITE: CLAUDE.md, "never remove the
    // owner's control in the name of fidelity". Both arrangements build, both
    // are tested, and the console names the one it ships.
    parameter int unsigned JOB_SRC    = 0
    // ARENA_ID_W WAS HERE AND IS RETIRED (ARENACOMPOSE, 2026-09-26). It sized
    // the binner's `ser_tri_id_o`; the serialise pass it belonged to is gone,
    // and `zhao_geom_arenabin` now takes the arena identity straight from
    // `zhao_geom_tidq` at the shell door instead of extracting it from the
    // opaque metadata on the far side of the binner. THE FIELD ITSELF STAYS
    // WHERE IT IS -- `zhao_console_core` still puts I54's arena id in the
    // continuation tail's dead vertex_rgb bytes; this block simply stopped
    // having an opinion about it.
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

    // ---- I55's RASTER DOOR -- THE NARROW HALF ----------------------------
    //
    // THE THING SIX PACKETS GOT WRONG, AND IT IS WORTH SPELLING OUT. Every
    // version of entry I55 and both 2026-09-27 decision records speak of
    // "taking `job_*` from the walk path" as though `job_*` were a stream that
    // had to be transported to this module. MUXBUILD opened the file, found it
    // internal, and priced the transport at 2,065 WIRES THROUGH TWO MODULE
    // BOUNDARIES.
    //
    // 2,037 of those 2,065 wires ARE ALREADY PORTS ON THIS MODULE. The time
    // multiplex feeds the SAME `u_geom_setup` / `u_geom_attrpack` pair from the
    // walk, and that pair's output arrives here on `tri_*_plane_i`,
    // `tri_ax_i..tri_cy_i`, `tri_src_id_i`, `tri_area2_i`, `tri_min_x_i`,
    // `tri_fragment_state_i` and `tri_continuation_tail_i` -- the same ports,
    // on the same silicon, whichever source fed it. `tri_meta_w` below is built
    // from them THIRTY LINES FROM HERE. The 1,877-bit metadata never needed
    // transporting because it was never anywhere else.
    //
    // So the door is what the walk knows and the triangle record does not: WHICH
    // TILE this reference is for, whether it opens or closes that tile's list,
    // and its own handshake. Twenty-eight bits.
    //
    // NOTE THE DIRECTION OF THE ERROR, because it is this repository's own law
    // running the other way for once: the inherited estimate made the remaining
    // work look SEVENTY-THREE TIMES BIGGER than it is, and a 2,065-wire price on
    // the tightest block in the design is exactly the shape a packet refuses
    // without measuring. LANESCOST predicted +90,000 ALUTs and measured +11,979.
    // An estimate is wrong in whichever direction nobody is checking.
    input  logic               walk_job_valid_i,
    output logic               walk_job_ready_o,
    input  logic signed [11:0] walk_job_tile_x_i,
    input  logic signed [11:0] walk_job_tile_y_i,
    input  logic               walk_job_first_i,
    input  logic               walk_job_last_i,
    // The door's own traffic count. It reads a structural zero in
    // `JOB_SRC == 0` -- the door is not built in that arrangement, so zero is
    // the truth and not a silent instrument. In `JOB_SRC == 1` it is the number
    // that must move off zero before any claim about I55 is worth reading, and
    // `raster_jobs_started_o` beside it says whether the tile pipe agreed.
    output logic        [31:0] walk_jobs_taken_o,

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

    // TERRAIN.NORMALMAP's config and tile-upload write port (NORMALMAP,
    // 2026-09-26), carried unchanged toward `zhao_texture_island_v3_top`. The
    // DETAIL DECLARATION is not here: it rides the candidate as
    // `detail_required` inside the texture request, so it cannot separate from
    // the fragment it describes.
    input  logic        dtl_we_i,
    input  logic        dtl_sel_i,
    input  logic [12:0] dtl_addr_i,
    input  logic [31:0] dtl_data_i,
    output logic [31:0] cnt_detail_fragments_o,
    output logic [31:0] cnt_detail_zeroed_o,
    output logic [31:0] cnt_detail_railed_o,
    output logic [31:0] cnt_detail_cold_o,
    output logic [31:0] cnt_detail_published_o,
    output logic [31:0] cnt_detail_applied_o,
    output logic [31:0] err_detail_lost_o,
    output logic        dtl_table_ready_o,

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

    // ---- THE BINNER'S SERIALISE PASS IS RETIRED (ARENACOMPOSE) -----------
    // `ser_req_i` and seven `ser_*` outputs crossed here, out through
    // `zhao_shell_top_v2` as `render_ser_*`, to `zhao_geom_chunkser`. Console
    // entry I55: that made the on-chip arena the thing that FILLED the
    // external one, so the two were in SERIES and the SDRAM path could never
    // be the sole producer. `zhao_geom_arenabin` fills the arena from the
    // post-clip stream directly, so nothing asks for the pass and the ports
    // are REMOVED rather than tied off.

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
    output logic       [490:0] stage_candidate_data_o,
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
  // THE THREE OFFSET LOCALPARAMS WENT WITH THE SERIALISE PASS (ARENACOMPOSE).
  // `META_TAIL_LO` / `TAIL_ARENA_ID_LO` / `ARENA_ID_LO` existed only to tell
  // `zhao_geom_binner_v2` where to slice I54's arena id back out of the
  // metadata. Nothing slices it here any more. The ELABORATION GUARD on the
  // tail's own width is kept below, because that contract is about the
  // continuation tail and not about the pass.

  initial begin : p_packet_d_meta_contract
    if ($bits(tri_meta_w) != METAW)
      $fatal(1, "zhao_geom_bin_pipe_v2: metadata width changed");
    if (METAW != 1877)
      $fatal(1, "zhao_geom_bin_pipe_v2: METAW is not the ratified 1877");
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

  // ---- THE BINNER'S DRAIN, AND WHAT `u_tile` ACTUALLY CONSUMES ----------
  // These were ONE bundle until DOORCOST. They are two now because the drain is
  // no longer the only thing that can offer a job, and a single bundle is how a
  // second source comes to be spliced in with an OR -- the arrangement six
  // packets have refused and which is still refused here.
  //
  // `bin_job_*` is what `zhao_geom_binner_v2` offers. `job_*` is what the tile
  // pipe sees. In `JOB_SRC == 0` they are the same wires and Quartus sees one
  // net; the rename costs nothing.
  logic bin_job_valid_w, bin_job_ready_w;
  logic signed [20:0] bin_job_ax_w, bin_job_ay_w, bin_job_bx_w,
                      bin_job_by_w, bin_job_cx_w, bin_job_cy_w;
  logic bin_job_first_w, bin_job_last_w;
  logic signed [11:0] bin_job_tile_x_w, bin_job_tile_y_w;
  logic [15:0] bin_job_source_w;
  logic [METAW-1:0] bin_job_meta_w;
  // bit 0 aux-profile bad, bit 1 area-profile bad -- decided by the binner at
  // WRITE and carried in the metadata bank's pad, so the tile pipe reads a
  // verdict instead of reducing 272 bits of it off the RAM output.
  logic [1:0] bin_job_profile_bad_w;
  // THE SAME VERDICT ON THE WRITE EDGE, for a job that never entered the bank.
  // One expression in the binner, two readers; see that port's comment for why
  // it is not recomputed here.
  logic [1:0] bin_write_profile_bad_w;

  logic job_valid_w, job_ready_w;
  logic signed [20:0] job_ax_w, job_ay_w, job_bx_w, job_by_w, job_cx_w, job_cy_w;
  logic job_first_w, job_last_w;
  logic signed [11:0] job_tile_x_w, job_tile_y_w;
  logic [15:0] job_source_w;
  logic [METAW-1:0] job_meta_w;
  logic [1:0] job_profile_bad_w;
  logic [15:0] job_tile_index_w;

  zhao_geom_binner_v2 #(
      .GRID_W(GRID_W), .GRID_H(GRID_H), .TILES(TILES), .TIDX_W(TIDX_W),
      .TRI_CAP(TRI_CAP), .TRI_W(TRI_W), .CHUNKS(CHUNKS),
      .CHUNK_W(CHUNK_W), .CHUNK_REFS(CHUNK_REFS), .METAW(METAW)
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
      .job_valid_o(bin_job_valid_w),
      .job_ready_i(bin_job_ready_w),
      .job_ax_o(bin_job_ax_w), .job_ay_o(bin_job_ay_w),
      .job_bx_o(bin_job_bx_w), .job_by_o(bin_job_by_w),
      .job_cx_o(bin_job_cx_w), .job_cy_o(bin_job_cy_w),
      .job_first_o(bin_job_first_w), .job_last_o(bin_job_last_w),
      .job_tile_x_o(bin_job_tile_x_w), .job_tile_y_o(bin_job_tile_y_w),
      .job_src_id_o(bin_job_source_w),
      .job_meta_o(bin_job_meta_w),
      .job_profile_bad_o(bin_job_profile_bad_w),
      .write_profile_bad_o(bin_write_profile_bad_w),
      .drain_busy_o(drain_busy_o),
      .drain_done_o(drain_done_o),
      .tile_references_o(binner_tile_references_o),
      .max_tile_list_depth_o(binner_max_tile_list_depth_o),
      .triangles_culled_o(binner_triangles_culled_o),
      .overflow_o(binner_overflow_o),
      .arena_full_o(binner_arena_full_o),
      .arena_used_o(binner_arena_used_o)
  );

  // ---- THE DOOR ---------------------------------------------------------
  //
  // ONE `generate` SELECT, NO MUX, NO OR. The two arrangements are alternatives
  // and never coexist, which is the whole reason this is an elaboration
  // parameter: `OWNER_VACATION_DIRECTIVE_2026-09-23.txt` section 4 rules that
  // "a parallel legacy on-chip frame arena that still supplies the actual
  // pixels is not closure", so an arrangement in which BOTH sources can reach
  // `u_tile` in one frame is not a transitional convenience -- it is the thing
  // the directive forbids, wearing a select line.
  //
  // `JOB_SRC == 1` sources the wide fields from `tri_*`, which is the SAME
  // silicon's output: `zhao_geom_setup` and `zhao_geom_attrpack` are provably
  // idle for the whole drain window (`zhao_geom_binner_v2.sv`'s `tri_ready_o`
  // and the `drain_req_r` priority test), so the time multiplex feeds them from
  // the walk and their planes arrive here unchanged. The planes are therefore
  // bit-identical BY CONSTRUCTION rather than by a verification claim, which is
  // `reports/DECISION-20260927-I55-SWAP-ARCHITECTURE.md`'s decisive reason and
  // survives intact here.
  initial begin : p_job_src_domain
    if (JOB_SRC > 1)
      $fatal(1, "zhao_geom_bin_pipe_v2: JOB_SRC=%0d is not a defined arrangement (0=binner drain, 1=SDRAM walk)",
             JOB_SRC);
  end

  generate
    if (JOB_SRC == 0) begin : g_job_from_binner
      assign job_valid_w         = bin_job_valid_w;
      assign bin_job_ready_w     = job_ready_w;
      assign job_ax_w            = bin_job_ax_w;
      assign job_ay_w            = bin_job_ay_w;
      assign job_bx_w            = bin_job_bx_w;
      assign job_by_w            = bin_job_by_w;
      assign job_cx_w            = bin_job_cx_w;
      assign job_cy_w            = bin_job_cy_w;
      assign job_first_w         = bin_job_first_w;
      assign job_last_w          = bin_job_last_w;
      assign job_tile_x_w        = bin_job_tile_x_w;
      assign job_tile_y_w        = bin_job_tile_y_w;
      assign job_source_w        = bin_job_source_w;
      assign job_meta_w          = bin_job_meta_w;
      assign job_profile_bad_w   = bin_job_profile_bad_w;
      // THE DOOR IS NOT BUILT IN THIS ARRANGEMENT. `walk_job_ready_o` is a
      // standing refusal, not a tie-off of a live path: there is no consumer
      // for a walk job here, and a ready that lied would take a triangle and
      // drop it -- which is the "sequencer with dangling outputs counts
      // triangles and drops them" failure the walk's own briefs forbid.
      assign walk_job_ready_o    = 1'b0;
      assign walk_jobs_taken_o   = 32'd0;
    end else begin : g_job_from_walk
      assign job_valid_w         = walk_job_valid_i;
      assign walk_job_ready_o    = job_ready_w;
      // The corners, the identity and the whole 1,877-bit metadata come off
      // this module's OWN INPUT PORTS. Nothing is transported and nothing is
      // recomputed.
      assign job_ax_w            = tri_ax_i;
      assign job_ay_w            = tri_ay_i;
      assign job_bx_w            = tri_bx_i;
      assign job_by_w            = tri_by_i;
      assign job_cx_w            = tri_cx_i;
      assign job_cy_w            = tri_cy_i;
      assign job_first_w         = walk_job_first_i;
      assign job_last_w          = walk_job_last_i;
      assign job_tile_x_w        = walk_job_tile_x_i;
      assign job_tile_y_w        = walk_job_tile_y_i;
      assign job_source_w        = tri_src_id_i;
      assign job_meta_w          = tri_meta_w;
      assign job_profile_bad_w   = bin_write_profile_bad_w;

      // THE BINNER'S DRAIN IS STILL BUILT, AND THAT IS AN AREA SAVING THIS
      // ARRANGEMENT HAS NOT YET TAKEN -- declared here rather than left for a
      // reader to discover. Its jobs are accepted and discarded so the frame
      // protocol still reaches `drain_done_o`; the tile pipe's own post-abort
      // sink already establishes that accepting a job without starting a tile
      // is a defined act. The pixels come from the walk, so directive section
      // 4's "parallel legacy arena that still supplies the actual pixels" does
      // not apply -- but the binner's four RAMs and its drain FSM are dead
      // weight in this mode, and retiring them is a SUBSYSTEM retirement on a
      // 1,131-line block with the bin phase and the drain sharing one FSM and
      // four memories. Named, measured (see the FINDINGS), and not taken here.
      assign bin_job_ready_w     = 1'b1;

      logic [31:0] walk_taken_q;
      always_ff @(posedge clk) begin
        if (!rst_n)                             walk_taken_q <= 32'd0;
        else if (walk_job_valid_i && job_ready_w) walk_taken_q <= walk_taken_q + 32'd1;
      end
      assign walk_jobs_taken_o = walk_taken_q;
    end
  endgenerate

  // Stable capture identity derived from the SELECTED source's row-major tile
  // origin. Correct for both arrangements without a second expression: the walk
  // supplies the same tile coordinates the binner did.
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
