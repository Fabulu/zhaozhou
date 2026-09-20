// tb_zhao_console_core_smoke.sv -- THE WIRING SMOKE BENCH for
// `zhao_console_core`.
//
// WHAT IT IS FOR, and equally what it is NOT for.
//
// It answers one question: does the connected machine come out of reset and do
// the newly adopted organs actually carry traffic, or are they instantiated
// scenery? Every check below is a PATH check -- a count that can only move if a
// beat crossed a wire between two blocks. None of them is a correctness check:
// the particle arithmetic, the projection numbers and the compositor's output
// all have their own directed tests and this bench deliberately re-verifies
// none of them.
//
// THE CHECKS, AND THE WIRE EACH ONE PROVES:
//
//   1. `gpu_tick_o` pulses                  -- the shell leaves reset and
//                                              FRAMECTL reaches a frame edge.
//   2. `part_tick_busy_o` rises after it    -- SHELL.gpu_tick -> PART.STATE.
//   3. a record lands in the played DDR     -- a record went DDR -> the
//                                              store -> STATE -> UPDATE ->
//                                              COLLIDE -> STATE -> the store
//                                              -> DDR (entry I1 closed), no
//                                              stimulus in between.
//   4. `geom_view_vertices_sent_o` moves         -- GEOM.SKIN -> GROUP_SEQ ->
//                                              client A of the projector.
//   5. `proj_a_grants_o` moves              -- the shared projector GRANTED
//                                              that client: the wire the
//                                              manifest says had no producer.
//   6. `geom_landings_o` moves              -- results came back through
//                                              GEOM.PROJ_LANE into the
//                                              sequencer. THE LOOP CLOSES.
//   7. `hist_snapshots_o` moves             -- SHELL.gpu_tick -> MEASURE.
//   8. `terr_tess_vertices_o` moves         -- TERRAIN.GROUP_SEQ ->
//                                              TERRAIN.TESS and back.
//   9. `terr_fills_forwarded_o` moves       -- TERRAIN.GROUP_SEQ -> client B.
//  10. `proj_b_grants_o` moves              -- the shared projector's ARBITER
//                                              granted its SECOND client.
//                                              Before the terrain sequencer
//                                              was composed this counter could
//                                              not move at all, so the whole
//                                              reason the projector is SHARED
//                                              was an argument and not a
//                                              measurement.
//  11. `terr_groups_opened_o` moves         -- the open/gen handshake with the
//                                              subsystem's terrain arena.
//  12. `terr_release_unsafe_o` STAYS ZERO   -- the sequencer's own safety
//                                              detector, which has a committed
//                                              mutant proving it can fire.
//
// WHAT IT CANNOT SHOW, said here rather than left as a silence: nothing flows
// through POST.COMPOSITE, because its source pixels have no producer in the
// core (entry I15 of the DUT's header). A bench that fabricated them would be
// supplying exactly what the completion plan forbids a harness to supply.
//
// The harness supplies clocks, reset, the particle generation store (memory)
// and the geometry vertex stream (host content). It supplies no lighting, no
// FIELD result, no particle update and no prepared triangle.

// No `timescale directive here: the RTL closure carries none, and adding one
// makes Verilator report TIMESCALEMOD against every module in it. The unit
// comes from --timescale on the command line instead.

module tb_zhao_console_core_smoke
  import zhao_pkg::*, zhao_abi_pkg::*, zhao_fb_tuple_pkg::*;
();

  // The DUT's parameter defaults, mirrored so the declarations below can use
  // them. Overriding one here without overriding it on the instance is a width
  // mismatch the compiler catches, which is why they are not magic numbers.
  // TWOD widths, mirrored from zhao_console_core's own parameter defaults so
  // the `.*` binding can size its ports. DERIVED, not typed: each is the same
  // $clog2 expression the core uses, so a change to a page or slot count there
  // cannot leave a hand-written width here quietly disagreeing.
  localparam int unsigned TWOD_PAGE_WORDS = 8192;
  localparam int unsigned TWOD_PAL_SLOTS  = 4;
  localparam int unsigned TWOD_BIND_SLOTS = 8;
  localparam int unsigned TWOD_PAW        = $clog2(TWOD_PAGE_WORDS);
  localparam int unsigned TWOD_PALAW      = $clog2(TWOD_PAL_SLOTS * 256);
  localparam int unsigned TWOD_BSW        = $clog2(TWOD_BIND_SLOTS);

  localparam int unsigned PART_REC_W     = 128;
  localparam int unsigned PART_AGE_W     = 10;
  localparam int unsigned PART_POS_W     = 18;
  localparam int unsigned PART_NRM_W     = 12;
  localparam int unsigned PART_FX_W      = 16;
  localparam int unsigned GEOM_ARENAS    = 4;
  localparam int unsigned GEOM_DEPTH     = 1089;
  localparam int unsigned GEOM_NVIEWS    = 2;
  localparam int unsigned GEOM_GEN_W     = 8;
  localparam int unsigned GEOM_PAYLOAD_W = 106;
  localparam int unsigned GEOM_INDEX_W   = $clog2(GEOM_DEPTH) + 1;
  localparam int unsigned GEOM_ARENA_W   = $clog2(GEOM_ARENAS) + 1;
  localparam int unsigned GEOM_ASM_VIDW  = 16;
  localparam int unsigned PROJ_T_ARENAS  = 4;
  localparam int unsigned PROJ_T_DEPTH   = 81;
  localparam int unsigned PROJ_T_INDEX_W = $clog2(PROJ_T_DEPTH) + 1;
  localparam int unsigned PROJ_T_ARENA_W = $clog2(PROJ_T_ARENAS) + 1;
  localparam int unsigned POST_LINE_W    = 384;
  localparam int unsigned POST_MAX_H     = 240;
  localparam int unsigned POST_XW        = $clog2(POST_LINE_W + 1);
  localparam int unsigned POST_YW        = $clog2(POST_MAX_H + 1);
  localparam int unsigned HIST_EW        = 32;
  localparam int unsigned HIST_SUB_BITS  = 1;
  localparam int unsigned HIST_LANES     = 4;
  localparam int unsigned HIST_CW        = 24;
  localparam int unsigned HIST_BINW      = $clog2((HIST_EW - HIST_SUB_BITS + 1) << HIST_SUB_BITS);
  // SURFACE.SHEET's resident slot count, mirrored so `surf_res_occupancy_o`
  // can be declared. Overriding SURF_SLOTS on the DUT without changing this
  // is a width mismatch the compiler catches, which is the point.
  localparam int unsigned SURF_SLOTS     = 2;

  localparam int unsigned N_PART_RECORDS = 6;

  // ---- the surface the particles are driven into (see the acceptance below) --
  // Every record this bench offers sits at y = 0, so a surface at +100
  // position LSBs puts all six INSIDE it and every one of them makes a
  // contact. From 2026-09-19 that surface is the PLANE (entry I7, still a
  // boundary input) and not a hand-driven terrain sample: I6 closed and the
  // terrain sample is produced INSIDE the core now, from a compose cache this
  // bench never fills (its pages fault by design -- see the TERRAIN spine
  // note), so the terrain answers "no ground" and the plane is what the
  // particles land on. The terrain path is asserted separately, below. `PART_CLEAR_EPS` is `zhao_part_collide`'s own default; it is
  // written here rather than read, so a change to that parameter fails this
  // bench loudly instead of silently agreeing with itself.
  localparam int signed   PART_TER_H     = 100;
  localparam int unsigned PART_CLEAR_EPS = 2;
  // THE GEOMETRY FIXTURE AND EVERY NUMBER ASSERTED ABOUT IT come from ONE
  // generated header, written by `tests/prod/smoke_geom_fixture_gen.cpp` from
  // the REFERENCE (project_vertex -> Clip -> Setup -> Binner) and held fresh by
  // the ctest `smoke_geom_fixture_fresh`. Nothing below re-derives a count.
`include "smoke_geom_fixture.svh"
  localparam int unsigned N_GEOM_VERTS   = SGF_N_VERTS;
  // R57: the fixture is SGF_N_MESHLETS meshlets over one vertex run. Every
  // meshlet declares all the vertices and its own contiguous index run, so the
  // frame decodes the vertex run once PER MESHLET -- which is what gives the
  // loop something to overlap with, and what every per-vertex count below is
  // measured against.
  localparam int unsigned N_GEOM_MESHLETS = SGF_N_MESHLETS;
  localparam int unsigned N_GEOM_DECODED  = SGF_N_MESHLETS * SGF_N_VERTS;
  // Whole 64-byte lines of eight beats, PER MESHLET: its index run starts
  // line-aligned and so does the shared vertex run, 32 B per vertex.
  function automatic int unsigned geom_beats_of(input int unsigned ntri);
    geom_beats_of = 8 * (((3 * ntri) + 63) / 64) + 8 * (((32 * SGF_N_VERTS) + 63) / 64);
  endfunction
  function automatic int unsigned geom_footprint_beats();
    int unsigned s;
    s = 0;
    for (int unsigned mi = 0; mi < SGF_N_MESHLETS; mi = mi + 1)
      s = s + geom_beats_of(SGF_MESH_NTRI[mi]);
    return s;
  endfunction
  localparam int unsigned GEOM_FOOTPRINT_BEATS = geom_footprint_beats();
  localparam int unsigned CYCLE_LIMIT    = 1_500_000;
  localparam logic signed [31:0] FX16_ONE = 32'sh0001_0000;

  // ==========================================================================
  // The DUT's ports, declared from its own port list. `.*` then binds them by
  // name, so a port this bench forgets is a compile error rather than a
  // silently floating input.
  // ==========================================================================
  // PART.STATE's generation store (entry I1, CLOSED 2026-09-19). The seven
  // record ports are gone: the core streams both generations through HPS DDR
  // itself (`u_part_hps`, client 3 of the terrain HPS arbiter), and what this
  // bench supplies is what the HPS supplies -- two buffer bases and a seed.
  logic [31:0]             part_cfg_base0_i;
  logic [31:0]             part_cfg_base1_i;
  // (I1's four part_seed_* ports are GONE, R41/R46: the store's first
  //  generation is seeded by SetPopulation's active_count through
  //  u_part_pop, from the command packet this bench builds below.)
  logic [31:0]             part_pop_taken_o;
  logic [31:0]             part_pop_refused_normal_o;
  logic [31:0]             part_pop_refused_count_o;
  logic [31:0]             part_pop_refused_flags_o;
  logic [31:0]             part_pop_seeds_issued_o;
  logic [31:0]             part_pop_handle_o;
  logic [31:0]             cmd_exec_pops_o;
  logic                    part_hps_cur_buf_o;
  logic [15:0]             part_hps_cur_count_o;
  logic [31:0]             part_hps_ticks_o;
  logic [31:0]             part_hps_ticks_dropped_o;
  logic [31:0]             part_hps_ticks_unseeded_o;
  logic [31:0]             part_hps_seeds_o;
  logic [31:0]             part_hps_seeds_refused_o;
  logic [31:0]             part_hps_rd_bursts_o;
  logic [31:0]             part_hps_wr_bursts_o;
  logic [31:0]             part_hps_records_read_o;
  logic [31:0]             part_hps_records_written_o;
  // R54 / R55: the two silences this composition is now able to read. The
  // store's bridge refusal and the arbiter's dropped pending request are both
  // argued to be unreachable HERE -- and both are asserted zero below, which
  // is a check on the argument rather than a restatement of it. Each is fired
  // on purpose in its own directed test.
  logic [31:0]             part_hps_bridge_errs_o;
  logic [31:0]             part_hps_ticks_faulted_o;
  logic [31:0]             part_hps_records_discarded_o;
  logic [31:0]             terr_hps_pend_dropped_o;
  logic [3:0]              terr_hps_pend_dropped_mask_o;
  logic [31:0]             terr_hps_c3_bursts_o;
  logic [31:0]             terr_hps_c3_wait_cycles_o;
  // PART.TABLE's per-frame load (core entry I33). THE TWENTY-FIVE DESCRIPTOR
  // PORTS THAT USED TO BE DECLARED HERE ARE GONE: the core instantiates
  // `zhao_part_table` and answers its own descriptor reads (entries I2 and I3,
  // closed 2026-09-19). This bench now has to LOAD the table to get the same
  // stimulus it used to drive straight onto the consumers' inputs -- which is
  // the point, because a descriptor that reaches PART.COLLIDE now has to have
  // travelled through the table to get there.
  localparam int unsigned PART_TBL_LD_W = 12 + (2 * PART_AGE_W) + (5 * 11) + (3 * 18);
  // (I33's six part_tbl_ld_* ports are GONE, R42: the descriptors are a
  //  SPECIES_TABLE page this bench STAGES and PUBLISHES, and
  //  u_part_table_loader carries them. These are its evidence.)
  logic [31:0]             part_tbl_pages_o;
  logic [31:0]             part_tbl_entries_o;
  logic [31:0]             part_tbl_pages_dropped_o;
  logic [31:0]             part_tbl_bad_magic_o;
  logic [31:0]             part_tbl_truncated_o;
  logic [31:0]             part_tbl_denied_o;
  logic [31:0]             geom_ma_jobs_e_o;
  logic [31:0]             part_tbl_loads_update_o;
  logic [31:0]             part_tbl_loads_collide_o;
  logic [31:0]             part_tbl_loads_spawn_o;
  logic [31:0]             part_tbl_loads_curve_o;
  logic [31:0]             part_tbl_load_refused_o;
  logic                    part_fld_valid_i;
  logic signed [10:0]      part_fld_ax_i;
  logic signed [10:0]      part_fld_ay_i;
  logic signed [10:0]      part_fld_az_i;
  // I6 CLOSED: the terrain sample is the core's own now. Its census and the
  // population origin (I7, widened) take the five retired inputs' place.
  logic [31:0]             part_ter_particles_o;
  logic [31:0]             part_ter_ground_o;
  logic [31:0]             part_ter_no_ground_o;
  logic [31:0]             part_ter_missed_o;
  logic [31:0]             part_ter_faults_o;
  logic [31:0]             part_ter_fills_landed_o;
  logic [31:0]             terr_tap_answered_o;
  logic [31:0]             terr_tap_off_patch_o;
  logic [31:0]             terr_tap_faults_o;
  // (I7's eight part_pop_origin_* / part_plane_* ports are GONE, R41: the
  //  population descriptor arrives as a SetPopulation 0x0303 record in the
  //  command packet and is held by u_part_pop.)
  logic                    part_cap_full_i;
  logic                    part_tick_busy_o;
  logic                    part_tick_done_o;
  logic [31:0]             part_survivors_o;
  logic [31:0]             part_children_written_o;
  logic [31:0]             part_children_dropped_capacity_o;
  logic [31:0]             part_staging_stall_cycles_o;
  logic [31:0]             part_species_refused_o;
  logic [31:0]             part_updated_o;
  logic [31:0]             part_upd_refused_o;
  logic [31:0]             part_died_by_age_o;
  logic                    part_upd_beat_refused_o;
  logic [31:0]             part_velocity_saturations_o;
  logic [31:0]             part_position_saturations_o;
  logic [31:0]             part_collisions_applied_o;
  logic [3:0]              part_hist_sel_i;
  logic [31:0]             part_hist_val_o;
  logic                    part_colour_en_o;
  logic [7:0]              part_colour_o;
  logic                    part_contact_o;
  logic [2:0]              part_response_o;
  logic                    part_col_refused_o;
  logic [31:0]             part_contacts_ignore_o;
  logic [31:0]             part_contacts_die_o;
  logic [31:0]             part_contacts_stick_o;
  logic [31:0]             part_contacts_slide_o;
  logic [31:0]             part_contacts_bounce_o;
  logic [31:0]             part_contacts_terrain_o;
  logic [31:0]             part_contacts_plane_o;
  logic [31:0]             part_already_inside_at_entry_o;
  logic [31:0]             part_terrain_sample_unavailable_o;
  logic [31:0]             part_response_refused_o;
  logic [31:0]             part_field_clamps_o;
  logic [31:0]             part_children_requested_o;
  logic [31:0]             part_children_emitted_o;
  logic [31:0]             part_children_refused_o;
  logic [31:0]             part_spawn_by_event0_o;
  logic [31:0]             part_spawn_by_event1_o;
  logic [31:0]             part_spawn_by_event2_o;
  logic [31:0]             part_spawn_by_event3_o;
  logic [31:0]             part_refused_count_gt_max_o;
  logic [31:0]             part_refused_unknown_species_o;
  logic [31:0]             part_refused_capacity_o;
  logic [31:0]             part_max_children_in_tick_o;
  // ---- THE GEOMETRY ASSET PATH (core connected item 11) -------------------
  // `geom_vd_v_*` IS GONE from the core's port list: GEOM.ASSETFETCH drives
  // GEOM.VDECODE inside the module now. These fifty-two nets are what the
  // path still asks of a harness, and every width below was generated from
  // the core's own port list rather than typed -- a width that disagrees
  // binds silently in one tool and loudly in another, and either way the
  // bench is then measuring a different machine.
  // I36/I41 ARE CLOSED (2026-09-20, owner ruling R29): the nine job ports this
  // bench used to drive are GONE, and the job is built inside the core by
  // GEOM.DRAWJOB out of a real DrawForm in the command packet. What the bench
  // declares now is that block's EVIDENCE.
  logic [31:0] geom_dj_draws_o, geom_dj_jobs_o, geom_dj_masked_o, geom_dj_empty_o;
  logic [31:0] geom_dj_pal_writes_o, geom_dj_pal_dropped_o;
  logic [31:0] geom_dj_refused_cull_o, geom_dj_refused_resident_o;
  logic [31:0] geom_dj_refused_stale_o, geom_dj_refused_xform_o;
  logic [31:0] geom_dj_refused_denied_o, geom_dj_refused_format_o;
  logic [31:0] geom_dj_refused_crc_o, geom_dj_refused_reserved_o;
  logic [31:0] geom_dj_refused_layout_o;
  logic [31:0] geom_dj_hdr_reads_o, geom_dj_hdr_crc_fail_o, geom_dj_hdr_framing_o;
  logic [31:0] geom_ma_jobs_d_o;

  // I50, BOUNDARY: GEOM.LOOM's node stream and camera basis, which the
  // 2026-08-31 6.4 ruling puts on the ARM. This bench plays that ARM: ONE ROOT
  // node carrying the identity, which is the instance transform the draw then
  // names -- so the descriptor's object bound is its world bound and the cull
  // sees the sphere the fixture placed.
  logic                    geom_loom_valid_i;
  logic                    geom_loom_ready_o;
  logic [9:0]              geom_loom_node_index_i;
  logic [9:0]              geom_loom_parent_index_i;
  logic [3:0]              geom_loom_kind_i;
  logic signed [31:0]      geom_loom_param_i [0:11];
  logic [15:0]             geom_loom_angle_i;
  logic [1:0]              geom_loom_axis_i;
  logic                    geom_loom_bodypatch_i;
  logic [15:0]             geom_loom_src_id_i;
  logic                    geom_loom_first_i;
  logic                    geom_loom_last_i;
  logic signed [31:0]      geom_loom_cam_basis_i [0:8];
  logic [31:0]             geom_loom_nodes_o, geom_loom_streams_o;
  logic [31:0]             geom_loom_refused_sorted_o, geom_loom_refused_parent_o;
  logic [31:0]             geom_loom_refused_overflow_o, geom_loom_refused_kind_o;
  logic [31:0]             geom_loom_refused_shear_o, geom_loom_refused_framing_o;
  // I37 CLOSED: the CRC walker is inside the core; its evidence comes out.
  logic [31:0]             geom_mf_crc_descriptors_o;
  logic [31:0]             geom_mf_crc_fail_o;
  logic [31:0]             geom_mf_crc_framing_o;
  // I38 and I11 CLOSED, I39 narrowed to the raster word: GEOM.REPLAY owns the
  // release, the handle and the TriangleDescriptor inside the core now.
  logic [31:0] geom_mf_meshlets_considered_o;
  logic [31:0] geom_mf_culled_all_cameras_o;
  logic [31:0] geom_mf_descriptors_fetched_o;
  logic [31:0] geom_mf_guard_denied_o;
  logic [31:0] geom_mf_refused_format_o;
  logic [31:0] geom_mf_refused_crc_o;
  logic [31:0] geom_mf_refused_generation_o;
  logic [31:0] geom_mf_refused_vertex_count_o;
  logic [31:0] geom_mf_refused_triangle_count_o;
  logic [31:0] geom_mf_refused_reserved_o;
  logic [31:0] geom_mf_refused_zero_bound_o;
  logic [31:0] geom_ma_jobs_a_o;
  logic [31:0] geom_ma_jobs_b_o;
  logic [31:0] geom_ma_denied_o;
  logic [31:0] geom_ma_contention_o;
  logic [31:0] geom_ma_err_short_o;
  logic [31:0] geom_ma_err_long_o;
  logic [31:0] geom_ma_err_unowned_o;
  logic [31:0] geom_af_meshlets_fetched_o;
  logic [31:0] geom_af_beats_read_o;
  logic [31:0] geom_af_guard_denied_o;
  logic [31:0] geom_af_refused_footprint_o;
  logic [31:0] geom_af_prefetch_stall_o;
  logic [31:0] geom_af_err_beat_truncated_o;
  logic [31:0] geom_af_err_beat_overrun_o;
  logic [31:0] geom_af_err_beat_unowned_o;
  logic [31:0] geom_asm_meshlets_o;
  logic [31:0] geom_asm_triangles_o;
  logic [31:0] geom_asm_refused_limits_o;
  logic [31:0] geom_asm_refused_index_o;
  logic signed [7:0]       geom_vd_d_nx_o, geom_vd_d_ny_o, geom_vd_d_nz_o;
  logic signed [15:0]      geom_vd_d_u_o, geom_vd_d_v_o;
  logic                    geom_vd_refused_o, geom_vd_reserved_nz_o;
  logic                    geom_vd_w0_illegal_o, geom_vd_format_bad_o;
  logic [31:0]             geom_vd_vertices_o;
  logic [31:0]             geom_vd_reserved_nz_count_o;
  logic [31:0]             geom_vd_w0_illegal_count_o;
  logic [31:0]             geom_vd_format_bad_count_o;
  logic [15:0]             geom_skin_src_id_o;
  logic [31:0]             geom_skin_vertices_transformed_o;

  // ---- GEOM.SKIN.NORM (core entry I43, CLOSED): its normal feeds GEOM.LIGHT
  // inside the core now; what leaves is a TAP of it, still differenced below.
  logic                    geom_sn_n_valid_o;
  // ---- GEOM.LIGHT = zhao_light_stream (owner ruling R2) --------------------
  // The descriptor bank is loaded by COMMAND (owner ruling R25, I48 closed):
  // this bench's packet carries a SetEnvironment and CMD.EXEC lowers it through
  // GEOM.LIGHT.ENV. The lit RGB's consumer is GEOM.VATTR (I46, closed).
  logic                    geom_light_cfg_gen_o;
  logic [31:0]             geom_light_env_loads_o, geom_light_env_records_o;
  logic [31:0]             geom_light_env_superseded_o;
  logic [31:0]             cmd_exec_envs_o;
  logic                    geom_light_valid_o, geom_light_ready_o;
  logic [16:0]             geom_light_r_o, geom_light_g_o, geom_light_b_o;
  logic                    geom_light_degenerate_vtx_o;
  logic [15:0]             geom_light_src_id_o;
  logic [31:0] geom_light_vertices_lit_o, geom_light_degenerate_o, geom_light_cfg_refused_o;
  logic [31:0] geom_light_epoch_refusals_o, geom_light_seam_mismatch_o, geom_light_tag_mismatch_o;
  logic [31:0] geom_light_root_queue_overflow_o, geom_light_rgb_sat_o, geom_light_nlights_clamped_o;
  logic [31:0] geom_light_adapter_refused_o;
  logic signed [63:0]      geom_sn_n_x_o, geom_sn_n_y_o, geom_sn_n_z_o;
  logic                    geom_sn_n_degenerate_o;
  logic [15:0]             geom_sn_n_src_id_o;
  logic [31:0]             geom_sn_vertices_o;
  logic [31:0]             geom_sn_degenerate_o;
  logic [31:0]             geom_sn_reduced_o;
  logic [31:0]             geom_sn_fork_stall_o;

  // The hand-computed world normal the fixture must produce. A NAMED constant
  // rather than a literal in the check, because it is derived from
  // `vdec_record`'s normal byte and w0 and must move if either does: it is
  // w0 * ONE_FX16 * nx = 64 * 65536 * 127. The derivation is written out at
  // the assertion.
  localparam logic signed [63:0] SN_EXPECT_NX = 64'sd532676608;

  // ---- I29: GEOM.POSE's clip page and skeleton bake ------------------------
  // GEOM.SKIN's two matrix arrays USED TO BE HERE and the bench drove them with
  // an identity. They are gone because zhao_geom_pose_palette is composed in
  // the core and drives them from a real store -- which is what closing entry
  // I10 means. What this bench can reach is one level up: the DECODER's source.
  logic                    geom_pose_start_i;
  logic [5:0]              geom_pose_bone_count_i;
  logic signed [31:0]      geom_pose_root_dx_i;
  logic signed [31:0]      geom_pose_root_dy_i;
  logic signed [31:0]      geom_pose_root_dz_i;
  logic [4:0]              geom_pose_bone_idx_o;
  logic [4:0]              geom_pose_bone_parent_i;
  logic signed [31:0]      geom_pose_bone_tx_i;
  logic signed [31:0]      geom_pose_bone_ty_i;
  logic signed [31:0]      geom_pose_bone_tz_i;
  logic signed [15:0]      geom_pose_quat_w_i;
  logic signed [15:0]      geom_pose_quat_x_i;
  logic signed [15:0]      geom_pose_quat_y_i;
  logic signed [15:0]      geom_pose_quat_z_i;
  logic signed [31:0]      geom_pose_inv_rest_i [0:11];
  logic                    geom_pose_busy_o;
  logic                    geom_pose_done_o;
  logic [31:0]             geom_pose_palettes_decoded_o;
  logic [31:0]             geom_pal_vertices_served_o;
  logic [31:0]             geom_pal_bones_written_o;
  logic [31:0]             geom_pal_bone_oob_o;
  logic [31:0]             geom_pal_bone_unset_o;
  logic [31:0]             geom_groups_opened_o;
  logic [31:0]             geom_groups_sealed_o;
  logic [31:0]             geom_view_vertices_sent_o;
  logic [31:0]             geom_landings_o;
  logic [31:0]             geom_jobs_refused_o;
  logic [31:0]             geom_alloc_stall_cycles_o;
  logic [31:0]             geom_rel_unheld_o;
  logic                    geom_seal_early_o;
  logic [31:0]             geom_holes_o, geom_groups_poisoned_o, geom_holes_early_o;
  logic [31:0]             geom_arena_hits_o;
  logic [31:0]             geom_arena_misses_o;
  logic [31:0]             geom_arena_refusals_o;
  logic                    geom_arena_overflow_o;
  logic                    proj_cfg_we_i;
  logic                    proj_cfg_view_i;
  logic [4:0]              proj_cfg_addr_i;
  logic [31:0]             proj_cfg_data_i;
  logic                    proj_en_i;
  // Client B, its arena lifetime and its reference port are NO LONGER PORTS:
  // TERRAIN.GROUP_SEQ and TERRAIN.TESS drive them inside the DUT. What the
  // bench declares instead is the terrain producer's OWN boundary.
  logic                    terr_job_valid_i;
  logic                    terr_job_ready_o;
  logic [5:0]              terr_job_ox_i;
  logic [5:0]              terr_job_oz_i;
  logic [1:0]              terr_job_level_i;
  logic [1:0]              terr_job_lvl_nz_i;
  logic [1:0]              terr_job_lvl_pz_i;
  logic [1:0]              terr_job_lvl_nx_i;
  logic [1:0]              terr_job_lvl_px_i;
  logic [16:0]             terr_job_morph_i;
  logic                    terr_job_surface_i;
  logic                    terr_job_dual_i;
  logic [15:0]             terr_job_src_id_i;
  logic [1:0]              terr_job_view_mask_i;
  logic [7:0]              terr_job_mat_a_i;
  logic [7:0]              terr_job_mat_b_i;
  logic [7:0]              terr_job_weight_i;
  logic                    terr_sparse_fill_i;
  // TERRAIN.TESS's lattice and cell-state ports are GONE from the DUT, and the
  // flat-lattice memory model this bench used to play with them went with them
  // (core header entry I22, closed 2026-09-19). `zhao_terrain_compcache_front`
  // is composed inside the core now and serves those reads for real.

  // ---- PACKET P-TERRAIN: the paging spine's boundary ----------------------
  // TERRAIN.CMD's command is a HOST PACKET (T5's SubmitTerrainSet from
  // SW.STREAM), and the HPS/guard ports are MEMORY BEHAVIOUR. Both are in the
  // completion plan's own list of what a harness may supply, which is why this
  // bench may drive them and may not drive, say, a composed patch_state.
  localparam int unsigned TERR_SLOTW_C   = 10;   // $clog2(256 sets x 4 ways)
  localparam int unsigned TERR_GENW_C    = 8;
  localparam int unsigned TERR_CSLOTW_C  = 8;    // $clog2(256 compose slots)

  logic                    terr_cmd_valid_i;
  logic                    terr_cmd_ready_o;
  logic [31:0]             terr_cmd_epoch_i;
  logic [31:0]             terr_cmd_list_off_i;
  logic [31:0]             terr_cmd_list_bytes_i;
  logic [31:0]             terr_cmd_list_crc_i;
  logic [15:0]             terr_cmd_patch_count_i;
  logic [31:0]             terr_cmd_sequence_i;
  logic [31:0]             terr_cmd_src_id_i;
  logic                    terr_cmd_done_valid_o;
  logic                    terr_cmd_done_ready_i;
  logic                    terr_cmd_done_ok_o;
  logic [3:0]              terr_cmd_done_verdict_o;
  logic [31:0]             terr_cmd_done_src_id_o;
  logic [31:0]             terr_cmd_done_crc_seen_o;
  logic [31:0]             terr_cfg_epoch_i;
  logic [31:0]             terr_cfg_arena_base_i;
  logic [31:0]             terr_cfg_arena_bytes_i;
  logic [15:0]             terr_cfg_load_budget_i;



  // ---- THE TERRAIN COMPOSE ENGINE's boundary (core header item 10) --------
  // The compose DOOR and the UNPIN are gone from the DUT: TERRAIN.SEQ's issue
  // reaches TERRAIN.PAGESTREAM and TERRAIN.PLACE inside the core, and the
  // streamer's completion unpins the page. What is left here is the engine's
  // own edge, and every one of these is a thing the completion plan lets a
  // harness supply -- memory behaviour, or a field result this console has no
  // producer for and must therefore NOT invent.

  // TERRAIN.HDRREAD and the compose path's guard read share, composed item 13.
  // These replace `terr_place_pitch_log2_i` / `terr_place_env_x0_i` /
  // `terr_place_env_z0_i`, which were entry I35's boundary and are gone: the
  // pitch and the envelope come off the page's own header inside the core now,
  // so there is nothing for a bench to drive and five more things to read.
  logic [31:0]             terr_hr_headers_o;
  logic [31:0]             terr_hr_refused_o;
  logic [31:0]             terr_hr_guard_denied_o;
  logic [31:0]             terr_hr_incomplete_o;
  logic [31:0]             terr_hr_ident_fails_o;
  logic                    terr_hr_idle_o;
  logic [31:0]             terr_rdshare_jobs_a_o;
  logic [31:0]             terr_rdshare_jobs_b_o;
  logic [31:0]             terr_rdshare_denied_o;
  logic [31:0]             terr_rdshare_contention_o;
  logic [31:0]             terr_rdshare_err_short_o;
  logic [31:0]             terr_rdshare_err_long_o;
  logic [31:0]             terr_rdshare_err_unowned_o;

  logic                    terr_pt_fld_valid_i;
  logic                    terr_pt_fld_ready_o;
  logic signed [31:0]      terr_pt_fld_height_i;
  logic                    terr_pt_fld_add_valid_i;
  logic                    terr_pt_fld_add_ready_o;
  logic signed [31:0]      terr_pt_fld_add_x0_i;
  logic signed [31:0]      terr_pt_fld_add_z0_i;
  logic signed [31:0]      terr_pt_fld_add_x1_i;
  logic signed [31:0]      terr_pt_fld_add_z1_i;
  logic [31:0]             terr_pt_fld_add_hash_i;
  logic [15:0]             terr_pt_fld_add_cmd_i;
  logic                    terr_pt_fld_add_accept_o;
  logic                    terr_pt_fld_add_reject_o;
  logic                    terr_pt_fld_covers_o;
  logic [4:0]              terr_pt_fields_active_o;
  logic [15:0]             terr_pt_trace_patch_id_o;
  logic [31:0]             terr_pt_trace_hash_o;
  logic [15:0]             terr_pt_trace_cmd_o;
  logic [31:0]             terr_pt_programs_rejected_o;

  logic                    terr_cc_cs_we_i;
  logic [4:0]              terr_cc_cs_ci_i;
  logic [4:0]              terr_cc_cs_cj_i;
  logic [1:0]              terr_cc_cs_substance_i;
  logic                    terr_cc_serve_release_i;

  logic [31:0]             terr_ps_lattices_o;
  logic [31:0]             terr_ps_lattices_refused_o;
  logic [31:0]             terr_ps_vertices_o;
  logic [31:0]             terr_ps_bursts_o;
  logic [31:0]             terr_ps_guard_denied_o;
  logic [31:0]             terr_ps_incomplete_o;
  logic                    terr_ps_idle_o;
  logic                    terr_ps_done_valid_o;
  logic                    terr_ps_done_ok_o;
  logic [3:0]              terr_ps_done_verdict_o;
  logic                    terr_place_valid_o;
  logic [15:0]             terr_place_src_id_o;
  logic [15:0]             terr_place_env_mismatch_o;
  logic [15:0]             terr_place_pitch_bad_o;
  logic [15:0]             terr_place_range_o;
  logic [15:0]             terr_place_patches_o;
  logic [31:0]             terr_pt_samples_o;
  logic [15:0]             terr_pt_subpatch_dirty_o;
  logic                    terr_pt_idle_o;
  logic                    terr_cc_fill_busy_o;
  logic                    terr_cc_fill_done_o;
  logic                    terr_cc_serve_valid_o;
  logic [15:0]             terr_cc_serve_src_id_o;
  logic [31:0]             terr_cc_fill_records_o;
  logic [31:0]             terr_cc_patches_filled_o;
  logic [31:0]             terr_cc_patches_served_o;
  logic [31:0]             terr_cc_fill_overrun_o;
  logic [31:0]             terr_cc_lat_oob_o;
  logic [31:0]             terr_cc_cs_oob_o;

  logic                    terr_dm_valid_i;
  logic                    terr_dm_ready_o;
  logic [TERR_SLOTW_C-1:0] terr_dm_slot_i;
  logic [TERR_GENW_C-1:0]  terr_dm_gen_i;
  logic [31:0]             terr_dm_epoch_i;
  logic                    terr_dm_bd_i;
  logic                    terr_dm_f_i;
  logic                    terr_dm_mips_i;

  logic                    terr_chk_valid_i;
  logic [TERR_SLOTW_C-1:0] terr_chk_slot_i;
  logic [TERR_GENW_C-1:0]  terr_chk_gen_i;
  logic [31:0]             terr_chk_epoch_i;
  logic                    terr_chk_valid_o;
  logic                    terr_chk_stale_o;

  // SW.STREAM's journal doorbell (owner ruling R14): the HPS's own words.
  logic [31:0]             terr_cfg_journal_base_i;
  logic [31:0]             terr_cfg_journal_bytes_i;
  logic                    terr_jdb_post_valid_i;
  logic                    terr_jdb_post_ready_o;
  logic [15:0]             terr_jdb_post_slot_i;
  logic [31:0]             terr_jdb_post_ticket_i;
  logic                    terr_jdb_ret_valid_o;
  logic                    terr_jdb_ret_ready_i;
  logic [31:0]             terr_jdb_ret_ticket_o;
  logic                    terr_jdb_ret_final_o;
  logic                    terr_jdb_ret_ok_o;
  logic [3:0]              terr_jdb_ret_verdict_o;
  logic                    terr_jdb_ack_valid_i;
  logic                    terr_jdb_ack_ready_o;
  logic [31:0]             terr_jdb_ack_ticket_i;
  logic                    terr_jdb_ack_ok_i;
  logic [31:0]             terr_wb_sheets_written_o;
  logic [31:0]             terr_wb_sheets_refused_o;
  logic [31:0]             terr_wb_sheets_faulted_o;
  logic [31:0]             terr_wb_guard_denied_o;
  logic [31:0]             terr_wb_acks_unmatched_o;
  logic [31:0]             terr_wb_acks_overdue_o;
  logic [31:0]             terr_jdb_starved_cycles_o;
  logic [31:0]             terr_jdb_ret_overflow_o;
  // I26 closed (terrain3): the terrain spine's bridge and guard ports are
  // internal now; the TERRAIN.BUILD socket share's evidence crosses instead.
  logic [31:0]             terr_bsock_contention_o;
  logic [31:0]             terr_bsock_retire_unowned_o;
  logic [31:0]             terr_bsock_wbeat_unowned_o;
  logic [31:0]             terr_hps_c2_bursts_o;
  logic [31:0]             terr_hps_c2_wait_cycles_o;
  logic [31:0]             terr_rdshare_jobs_wb_o;

  logic [31:0]             terr_cmd_sets_accepted_o;
  logic [31:0]             terr_cmd_sets_refused_o;
  logic [31:0]             terr_cmd_records_emitted_o;
  logic [31:0]             terr_cmd_crc_fails_o;
  logic [31:0]             terr_cmd_bridge_errs_o;
  logic                    terr_seq_busy_o;
  logic                    terr_seq_done_o;
  logic [31:0]             terr_seq_records_consumed_o;
  logic [31:0]             terr_seq_patches_issued_o;
  logic [31:0]             terr_seq_claims_issued_o;
  logic [31:0]             terr_seq_claims_refused_o;
  logic [31:0]             terr_seq_claims_same_o;
  logic [31:0]             terr_seq_loads_issued_o;
  logic [31:0]             terr_seq_skipped_not_resident_o;
  logic [31:0]             terr_seq_frame_faults_o;
  logic                    terr_seq_err_stray_ans_o;
  logic [31:0]             terr_res_hits_o;
  logic [31:0]             terr_res_misses_o;
  logic [31:0]             terr_res_claims_o;
  logic [31:0]             terr_res_evictions_o;
  logic [31:0]             terr_res_crc_failures_o;
  logic [31:0]             terr_res_resident_o;

  // THE SECOND COMPLETION, 2026-09-19 (core composition item 12).  Declared
  // here rather than by hand at the bottom because this bench binds with `.*`:
  // every core port needs an identically-named net, and a port with none is a
  // bind error rather than a silent zero.
  logic [31:0]             terr_mip_pages_mipped_o;
  logic [31:0]             terr_mip_pages_faulted_o;
  logic [31:0]             terr_mip_samples_sent_o;
  logic [31:0]             terr_mipreq_requests_o;
  logic [31:0]             terr_mipreq_issued_o;
  logic [31:0]             terr_mipreq_drops_o;
  logic [31:0]             terr_psmux_a_jobs_o;
  logic [31:0]             terr_psmux_b_jobs_o;
  logic [31:0]             terr_psmux_stray_v_o;
  logic [31:0]             terr_psmux_stray_done_o;
  logic [31:0]             terr_mg_m17_writes_o;
  logic [31:0]             terr_mg_m9_writes_o;
  logic [31:0]             terr_mg_aborts_o;

  logic [31:0]             terr_lq_accepted_o;
  logic [31:0]             terr_lq_issued_o;
  logic [31:0]             terr_lq_high_water_o;
  logic [31:0]             terr_pl_pages_loaded_o;
  logic [31:0]             terr_pl_pages_faulted_o;
  logic [31:0]             terr_pl_crc_fails_o;
  logic [31:0]             terr_pl_load_bytes_o;
  logic [31:0]             terr_pl_guard_denied_o;
  logic [31:0]             terr_pl_bridge_errs_o;
  logic [31:0]             terr_pl_slot_overflow_o;
  logic [31:0]             terr_pl_pages_refused_o;
  logic [3:0]              terr_pl_fault_verdict_o;
  logic [31:0]             terr_pl_fault_island_o;
  logic signed [15:0]      terr_pl_fault_ix_o;
  logic signed [15:0]      terr_pl_fault_iz_o;
  logic [31:0]             terr_pl_fault_src_id_o;
  logic [31:0]             terr_pl_incomplete_o;
  logic [31:0]             terr_pl_hdr_ident_fails_o;
  logic [31:0]             terr_hps_c0_bursts_o;
  logic [31:0]             terr_hps_c1_bursts_o;
  logic [31:0]             terr_hps_c1_wait_cycles_o;

  // ---- MEM.UPLOAD on the TERRAIN.BUILD socket (2026-09-19, cmdmem) --------
  // ---- MATERIAL.RESOLVE (R20): directory + fetch internal, request at I49 --
  logic         mat_req_valid_i, mat_req_ready_o;
  logic [31:0]  mat_req_material_set_i;
  logic [15:0]  mat_req_material_id_i;
  logic [ 7:0]  mat_req_quality_tier_i;
  logic         mat_rsp_valid_o, mat_rsp_ready_i;
  logic [ 2:0]  mat_rsp_status_o;
  logic         mat_rsp_has_record_o;
  logic [255:0] mat_rsp_record_o;
  logic [ 7:0]  mat_rsp_quality_tier_o;
  logic [ 1:0]  mat_rsp_sample_count_o;
  logic [ 2:0]  mat_rsp_material_recipe_o;
  logic [ 7:0]  mat_rsp_recipe_weight_o, mat_rsp_base_binding_o;
  logic         mat_rsp_selector_overflow_o;
  logic [31:0]  mat_rsp_palette_base_o, mat_rsp_raster_state_o;
  logic [ 7:0]  mat_rsp_flags_o, mat_rsp_sample0_modes_o, mat_rsp_sample1_modes_o, mat_rsp_sample2_modes_o;
  logic [31:0]  mat_hits_o, mat_misses_o, mat_refused_o, mat_refused_id_o, mat_refused_record_o;
  logic [31:0]  mat_not_resident_o, mat_selector_overflow_o, mat_recipe_count_mismatch_o, mat_fetch_denied_o;
  logic [31:0]  geom_ma_jobs_c_o;  logic [31:0]             cmd_exec_uploads_o;
  logic [31:0]             cmd_exec_upload_overflow_o;
  logic [31:0]             cmd_exec_post_looks_o, cmd_exec_grade_entries_o;
  logic [31:0]             cmd_exec_post_refused_o, cmd_exec_grade_overflow_o;
  logic [31:0]             upl_cfg_region_base_i;
  logic [31:0]             upl_cfg_region_bytes_i;
  logic [63:0]             upl_cfg_arena_base_i;
  logic [31:0]             upl_cfg_arena_bytes_i;
  logic [15:0]             upl_cfg_epoch_i;
  logic                    upl_publish_valid_o;
  logic [ 7:0]             upl_publish_slot_o;
  logic [15:0]             upl_publish_generation_o;
  logic [ 7:0]             upl_publish_tag_o;
  logic [23:0]             upl_publish_index_o;
  logic [31:0]             upl_publish_base_o;
  logic [31:0]             upl_publish_extent_o;
  logic                    upl_done_o;
  logic [ 7:0]             upl_status_o;
  logic [15:0]             upl_published_o;
  logic [127:0]            upl_refused_o;
  logic [31:0]             upl_hps_wait_o;

  logic [PROJ_T_ARENAS-1:0] terr_held_o;
  logic                    terr_busy_o;
  logic [31:0]             terr_jobs_accepted_o;
  logic [31:0]             terr_jobs_no_view_o;
  logic [31:0]             terr_jobs_rejected_o;
  logic [31:0]             terr_jobs_empty_o;
  logic [31:0]             terr_groups_opened_o;
  logic [31:0]             terr_groups_released_o;
  logic [31:0]             terr_fills_forwarded_o;
  logic [31:0]             terr_fills_dropped_o;
  logic [31:0]             terr_refs_forwarded_o;
  logic [31:0]             terr_release_unsafe_o;
  logic [31:0]             terr_tess_vertices_o;
  logic [31:0]             terr_tess_refs_o;
  logic [31:0]             terr_tess_rejected_o;
  logic [31:0]             terr_tess_lod_clamped_o;
  logic [31:0]             terr_tess_mode_invalid_o;
  logic                    terr_tess_idle_o;
  logic                    proj_out_valid_o;
  logic                    proj_out_ready_i;
  logic signed [20:0]      proj_out_ax_o;
  logic signed [20:0]      proj_out_ay_o;
  logic signed [20:0]      proj_out_bx_o;
  logic signed [20:0]      proj_out_by_o;
  logic signed [20:0]      proj_out_cx_o;
  logic signed [20:0]      proj_out_cy_o;
  logic [2:0]              proj_out_behind_o;
  logic [15:0]             proj_out_src_id_o;
  logic signed [31:0]      proj_out_ad_o;
  logic signed [31:0]      proj_out_bd_o;
  logic signed [31:0]      proj_out_cd_o;
  logic [30:0]             proj_out_aw_o;
  logic [30:0]             proj_out_bw_o;
  logic [30:0]             proj_out_cw_o;
  logic                    proj_out_view_o;
  logic [7:0]              proj_out_mat_a_o;
  logic [7:0]              proj_out_mat_b_o;
  logic [7:0]              proj_out_weight_o;
  logic                    proj_out_refused_o;
  logic                    proj_out_missed_o;
  logic                    proj_a_view_o;
  logic [1:0]              proj_a_profile_o;
  logic [1:0]              proj_fill_profile_o;
  logic [31:0]             proj_replay_triangles_o;
  logic [31:0]             proj_replay_refused_o;
  logic [31:0]             proj_replay_missed_o;
  logic [31:0]             proj_corner_hits_o;
  logic [31:0]             proj_corner_refusals_o;
  logic [31:0]             proj_corner_misses_o;
  logic                    proj_arena_overflow_o;
  logic                    proj_arena_seal_short_o;
  logic                    proj_shell_idle_o;
  logic                    proj_svc_busy_o;
  logic [31:0]             proj_a_grants_o;
  logic [31:0]             proj_b_grants_o;
  logic [31:0]             proj_contended_o;
  logic [31:0]             proj_mat_refused_o;
  // R21: TERRAIN's lit normals leave with the triangle they belong to (entry
  // I13). The bench takes every light -- the far side of that edge is the
  // absent GEOM.CLIP merge, and a harness that stalled it would measure its
  // own backpressure instead of the lane.
  logic                    terr_light_valid_o;
  logic                    terr_light_ready_i;
  logic signed [31:0]      terr_light_base_o;
  logic                    terr_light_degenerate_o;
  logic [15:0]             terr_light_src_id_o;
  logic [31:0]             terr_light_refs_taken_o;
  logic [31:0]             terr_light_emitted_o;
  logic [31:0]             terr_light_stale_reads_o;
  logic [31:0]             terr_light_normals_o;
  logic [31:0]             terr_light_shaded_o;
  logic [31:0]             terr_light_degenerate_count_o;
  logic [31:0]             terr_light_base_sat_o;
  logic [31:0]             terr_light_degen_mismatch_o;  logic                    post_gd_req_v_o;
  logic                    post_gd_view_o;
  logic [POST_XW-3:0]      post_gd_cx_o;
  logic [POST_YW-3:0]      post_gd_cy_o;
  logic                    post_gd_present_i;
  logic signed [7:0]       post_gd_dx_i;
  logic signed [7:0]       post_gd_dy_i;
  logic                    post_gg_req_v_o;
  logic                    post_gg_view_o;
  logic [POST_XW-3:0]      post_gg_cx_o;
  logic [POST_YW-3:0]      post_gg_cy_o;
  logic                    post_gg_present_i;
  logic [15:0]             post_gg_glow_i;
  logic                    post_gg_ink_i;
  logic                    post_atm_req_v_o;
  logic [POST_XW-1:0]      post_atm_req_x_o;
  logic [POST_YW-1:0]      post_atm_req_y_o;
  logic                    post_atm_en_i;
  logic                    post_atm_valid_i;
  logic [15:0]             post_atm_rgb_i;
  logic [7:0]              post_atm_opacity_i;
  logic                    post_atm_add_i;
  // post_bloom_gain_i .. post_ink_rgb_i are GONE (R36): CMD.EXEC drives them.
  logic                    post_hud_req_v_o;
  logic [POST_XW-1:0]      post_hud_req_x_o;
  logic [POST_YW-1:0]      post_hud_req_y_o;
  logic                    post_hud_valid_i;
  logic [15:0]             post_hud_rgb_i;
  // POST.COMPOSITE's lease and POST.ECHO (I15/I16 closed 2026-09-19): the
  // source, the output and the echo tap are internal now; this is their
  // evidence.
  logic                    post_busy_o;
  logic [31:0]             post_passes_o;
  logic [31:0]             post_frames_o;
  logic                    post_fault_o;
  logic [31:0]             post_src_reads_o;
  logic [31:0]             post_src_pixels_o;
  logic [31:0]             post_retire_unowned_o;
  logic [31:0]             post_share_contention_o;
  logic [31:0]             echo_passes_complete_o;
  logic [31:0]             echo_passes_torn_o;
  logic [31:0]             echo_pixels_written_o;
  logic [31:0]             echo_pixels_dropped_o;
  logic                    echo_fault_o;
  logic [31:0]             post_displacement_edge_clamps_o;
  logic [31:0]             post_bloom_cells_contributing_o;
  logic [31:0]             post_passes_completed_o;
  logic [31:0]             post_grading_table_missing_o;
  logic [31:0]             post_plane_missing_o;
  logic [31:0]             post_line_fill_writes_o;
  logic [31:0]             post_output_writes_o;
  logic [31:0]             post_plane_reads_o;
  logic [31:0]             post_ring_hazard_o;
  logic                    hist_ev_valid_i;
  logic [HIST_LANES-1:0]   hist_ev_lane_valid_i;
  logic [HIST_LANES*HIST_EW-1:0] hist_ev_err_i;
  logic [15:0]             hist_ev_src_id_i;
  logic                    hist_ev_ready_o;
  // `hist_rd_*` is GONE from the core's edge (entry I19 closed 2026-09-20):
  // the histogram's read port is driven inside the core by the HPS register
  // aperture's tenant 0. This bench reads bins through `hostreg_*` instead.
  logic                    hist_snap_valid_o;
  logic [HIST_CW-1:0]      hist_snap_total_o;
  logic [15:0]             hist_snap_src_id_o;
  logic [HIST_CW-1:0]      hist_snap_index_o;
  logic [HIST_CW-1:0]      hist_events_o;
  logic [HIST_CW-1:0]      hist_updates_o;
  logic [HIST_CW-1:0]      hist_stall_cycles_o;
  logic [HIST_CW-1:0]      hist_bin_sat_o;
  logic [HIST_CW-1:0]      hist_fwd_hits_o;
  logic [HIST_CW-1:0]      hist_host_conflict_o;
  logic [HIST_CW-1:0]      hist_snapshots_o;
  logic [HIST_CW-1:0]      hist_frozen_write_o;
  logic                    light_seam_connected_o;
  logic gpu_clk;
  logic vid_clk;
  logic audio_clk;
  logic rst_n;
  logic        cfg_valid_i;
  logic        cfg_ready_o;
  logic [1:0]  cfg_op_i;
  logic [7:0]  cfg_page_generation_i;
  logic [7:0]  cfg_selector_i;
  logic [74:0] cfg_row_i;
  logic [31:0] cfg_crc32_i;
  logic        cfg_rsp_valid_o;
  logic        cfg_rsp_ready_i;
  logic [1:0]  cfg_rsp_op_o;
  logic [3:0]  cfg_rsp_status_o;
  logic [7:0]  cfg_rsp_page_generation_o;
  logic [7:0]  active_page_generation_o;
  logic        pal_load_valid_i;
  logic        pal_load_ready_o;
  logic [1:0]  pal_load_op_i;
  logic [1:0]  pal_load_slot_i;
  logic [7:0]  pal_load_gen_i;
  logic [7:0]  pal_load_idx_i;
  logic [15:0] pal_load_rgb565_i;
  logic        pal_load_crc_ok_i;
  logic [46:0]  tri_area2_i;
  // THE THREE ATTRIBUTE PLANES ARE GONE FROM HERE, and their absence is the
  // whole point. They were core INPUTS this bench drove to zero; the core now
  // contains their producer (GEOM.ATTRPACK), so a net here would bind to
  // nothing under `.*` and, worse, would read as though this bench were still
  // supplying them.
  logic [297:0] tri_flat_request_i;
  logic [47:0]  tri_continuation_tail_i;
  logic [31:0]  tri_fragment_state_i;
  logic         fill_req_ready_i;
  logic         fill_req_valid_o;
  logic [31:0]  fill_req_addr_o;
  logic         fill_data_valid_i;
  logic [15:0]  fill_data_i;
  logic         fill_refused_i;
  logic [63:0]  frame_clear_word_i;
  logic         sheet_req_ready_i;
  logic         sheet_req_valid_o;
  logic [1:0]   sheet_req_op_o;
  logic [31:0]  sheet_req_handle_o;
  logic [11:0]  sheet_req_texel_o;
  logic [15:0]  sheet_req_src_id_o;
  logic        blank_cmd_i;
  logic        scanout_ack_i;
  logic        frame_swap_valid_i;
  logic        frame_swap_slot_i;
  logic        blank_ack_o;
  logic        blank_active_o;
  logic        lease_open_o;
  logic [1:0]  frame_slot_ready_o;
  logic [31:0] v2_requests_accepted_o;
  logic [31:0] v2_responses_accepted_o;
  logic [31:0] v2_leases_granted_o;
  logic [31:0] v2_leases_refused_o;
  logic [31:0] v2_faults_latched_o;
  logic [31:0] v2_publications_o;
  logic [31:0] v2_releases_o;
  logic [31:0] v2_ready_events_o;
  logic [31:0] v2_swaps_o;
  logic [31:0] v2_contentions_o;
  logic [31:0] v2_clear_handshakes_o;
  logic [31:0] v2_frames_admitted_o;
  logic [31:0] v2_blit_leases_acquired_o;
  logic [31:0] v2_blit_leases_refused_o;
  logic [1:0]  hps_state_i [0:2];
  logic [31:0] hps_byte_len_i [0:2];
  logic        ring_wr_valid_o;
  logic [1:0]  ring_wr_slot_o;
  logic [1:0]  ring_wr_state_o;
  logic        ring_wr_ready_i;
  logic        hps_req_valid_o;
  logic        hps_req_write_o;
  logic [31:0] hps_req_addr_o;
  logic [6:0]  hps_req_len_o;
  logic        hps_req_grant_i;
  logic        hps_wr_valid_o;
  logic [63:0] hps_wr_data_o;
  logic        hps_wr_last_o;
  logic        hps_rd_valid_i;
  logic [63:0] hps_rd_data_i;
  logic        hps_rd_last_i;
  logic [3:0]  pad_present_i;
  logic [31:0] pad_buttons_i [0:3];
  logic [15:0] pad_lx_i [0:3];
  logic [15:0] pad_ly_i [0:3];
  logic [15:0] pad_rx_i [0:3];
  logic [15:0] pad_ry_i [0:3];
  logic        aud_wr_valid_i;
  logic [15:0] aud_wr_l_i;
  logic [15:0] aud_wr_r_i;
  logic        aud_wr_ready_o;
  logic        aud_refill_req_o;
  logic [11:0] aud_occupancy_o;
  logic        pcm_valid_o;
  logic [15:0] pcm_l_o;
  logic [15:0] pcm_r_o;
  logic        underrun_status_o;
  logic [31:0] audio_underruns_o;
  logic        px_valid_o;
  logic [15:0] px_rgb_o;
  logic [9:0]  px_x_o;
  logic [7:0]  px_y_o;
  logic        px_hsync_o;
  logic        px_vsync_o;
  logic        px_hblank_o;
  logic        px_vblank_o;
  logic        scaler_violation_o;
  logic [31:0] crc_frame_o;
  logic        crc_valid_o;
  logic [31:0] crc_bytes_o;
  logic        crc_size_err_o;
  logic        gpu_tick_o;
  logic [31:0] gpu_tick_frame_id_o;
  logic        gpu_tick_repeated_o;
  logic [0:0]  gpu_complete_slot_o;
  logic [63:0] deadline_faults_o;
  logic [63:0] frame_cycles_o;
  logic [2:0]  slot_state_o [0:2];
  logic        fence_valid_o;
  logic [1:0]  fence_slot_o;
  logic        fence_ok_o;
  logic [7:0]  fence_status_o;
  logic [1:0]  mode_act_o;
  logic        dma_done_o;
  logic [7:0]  dma_status_o;
  logic        blit_done_o;
  logic [7:0]  blit_status_o;
  logic [639:0] pad_frame_flat_o;
  logic [15:0]  pad_sequence_o [0:3];
  logic [63:0]  input_gaps_o;
  logic [7:0]   rumble_duty_o [0:3];
  logic [3:0]   rumble_active_o;
  logic [3:0]   rumble_pwm_o;
  logic [63:0]  rumble_drops_o;
  logic        cnt_snap_ready_i;
  logic        cnt_snap_valid_o;
  logic [15:0] cnt_snap_id_o;
  logic [63:0] cnt_snap_value_o;
  logic        cnt_window_open_o;
  logic        cnt_cat_violation_o;
  logic [31:0] guard_violations_o;
  logic [63:0] starvation_o;
  logic        init_done_o;
  logic [31:0] refresh_stalls_o;
  logic [31:0] bank_conflicts_o;
  logic [31:0] scanout_preempted_o;
  logic [31:0] hps_err_count_o;
  logic        shell_err_wfifo_o;
  logic        shell_err_route_o;
  logic        shell_err_cdc_o;
  logic        shell_err_framer_o;
  logic        render_frame_begin_i;
  logic        render_frame_end_i;
  logic [5:0]  render_grid_w_i;
  logic [5:0]  render_grid_h_i;
  // The triangle door is no longer at the core's edge -- GEOM.SETUP drives it
  // from inside. What this bench presents instead is GEOM.CLIP's input, one
  // block further up (entry I24).
  // `geom_guard_req_i`, `geom_guard_rsp_o` and the three `geom_beat_*_o`
  // ARE GONE from the core's port list too. They were the seam where this
  // bench answered the geometry fetchers' grants and fabricated their beats;
  // GEOM.MEM_ADAPTER drives the shell's socket now, so what this bench
  // supplies is one level lower and is real memory -- see the SDRAM model
  // and the asset fixture below.
  // ---- GEOM.CLIP's cull mode (entry I24, narrowed) and the CLIP/SETUP evidence
  // THE TRIANGLE DOOR IS GONE: GEOM.CLIP is fed by GEOM.REPLAY inside the core.
  localparam int unsigned GEOM_CLIP_ATTRW = 7 * 32;
  // ---- GEOM.VATTR's census (entry I46 CLOSED: the store is INSIDE the core now,
  // so the bench's edge model of it is gone -- see the note where it stood)
  logic [31:0] geom_va_landings_o, geom_va_rows_written_o, geom_va_colours_written_o;
  logic [31:0] geom_va_uv_staged_o, geom_va_lq_overflow_o, geom_va_index_oob_o;
  logic [31:0] geom_va_look_oob_o, geom_va_profile_mixed_o, geom_va_dq_refused_o;
  logic [31:0] geom_va_dq_stray_o;
  logic [31:0] geom_va_uv_waits_o;
  logic        geom_va_poison_o;
  // ---- GEOM.REPLAY's evidence
  logic [31:0] geom_rp_meshlets_o, geom_rp_groups_o, geom_rp_triangles_in_o;
  logic [31:0] geom_rp_triangles_out_o, geom_rp_refused_o, geom_rp_missed_o;
  logic [31:0] geom_rp_att_skew_o, geom_rp_view_bad_o;
  logic [31:0] geom_rp_poisoned_o;
  logic [31:0] geom_rp_triq_stall_o;   // R57: the descriptor queue's backpressure
  // GEOM.ATTRPACK's evidence, out of the core because a counter nobody can
  // read is not evidence. The RATIO is what gets asserted below.
  logic [31:0]             geom_attrpack_triangles_o;
  logic [31:0]             geom_attrpack_planes_o;
  logic [GEOM_CLIP_ATTRW-1:0] geom_clip_attr_a_o, geom_clip_attr_b_o, geom_clip_attr_c_o;
  logic                    geom_clip_flip_o;
  logic                    geom_clip_ret_valid_o;
  logic [2:0]              geom_clip_ret_verdict_o;
  logic [31:0]             geom_clip_submitted_o;
  logic [31:0]             geom_clip_clipped_o;
  logic [31:0]             geom_clip_culled_o;
  logic signed [47:0]      geom_setup_area2_o;
  logic [31:0]             geom_setup_triangles_submitted_o;
  logic [63:0] render_fill_word_i;
  logic [63:0] render_clear_word_i;
  logic [31:0] render_state_i;
  logic [ 7:0] render_src_a_i;
  logic [23:0] render_texel_rgb_i;
  logic [ 7:0] render_texel_a_i;
  logic [ 7:0] render_texel_idx_i;
  logic [26:0] render_fb_base_i;
  logic [15:0] render_fb_stride_i;
  logic        fb_writer_i;
  logic        render_drain_done_o;
  logic        render_busy_o;
  logic [31:0] render_pixels_o;
  logic [31:0] render_bursts_o;
  logic        render_stream_error_o;
  logic        render_drained_o;
  logic        render_fatal_o;
  logic [31:0] render_issued_words_o;
  logic [31:0] render_retired_words_o;
  logic        render_overflow_o;
  logic        render_fragment_error_o;
  // ---- PACKET P-SURFACE, 2026-09-19 -------------------------------------
  // The three ends of the composed SURFACE pair that have no owner in the
  // tree (DUT entries I30, I31, I32), plus its evidence. The request, page
  // and write channels between STAMP and SHEET are INTERNAL to the DUT and
  // deliberately do not appear here -- if they did, this bench would be
  // playing the seam it is supposed to be measuring.
  logic               surf_cmd_valid_i;
  logic               surf_cmd_ready_o;
  logic        [31:0] surf_cmd_handle_i;
  logic        [ 7:0] surf_cmd_operation_i;
  logic        [ 7:0] surf_cmd_tag_i;
  logic        [15:0] surf_cmd_strength_i;
  logic signed [31:0] surf_cmd_tx_i;
  logic signed [31:0] surf_cmd_ty_i;
  logic signed [31:0] surf_cmd_radius_i;
  logic signed [31:0] surf_cmd_ring_width_i;
  // (I30's nine open ports are GONE, R45: u_surface_dispatch resolves the
  //  patch envelope from the stamp's own translation by the world->patch law,
  //  and the policy is three named parameters. These are its evidence.)
  logic [31:0]        surf_disp_dispatched_o;
  logic [31:0]        surf_disp_pitch_refused_o;
  logic [31:0]        surf_disp_env_clamped_o;
  logic signed [15:0] surf_disp_patch_ix_o;
  logic signed [15:0] surf_disp_patch_iz_o;
  logic signed [31:0] surf_disp_env_x0_o;
  logic signed [31:0] surf_disp_env_x1_o;
  logic        [15:0] surf_cmd_src_id_i;
  // I31 CLOSED 2026-09-19: `surf_fld_*` is no longer at the DUT's edge. The
  // brush is driven inside the core by `u_field_stamp_adapter`, so these four
  // nets are GONE rather than held at zero. What the adapter reports is below.
  logic        [31:0] surf_fld_stamps_o;
  logic        [31:0] surf_fld_texels_o;
  logic        [31:0] surf_fld_faults_o;
  logic        [31:0] surf_fld_restarts_o;
  logic               surf_fld_busy_o;

  // ---- THE FIELD ENGINE'S EDGE (core entry I42) ---------------------------
  // Widths generated from the core's own port list, including the two wide
  // ones: `fld_req_in_i` is IN_LANES x 32 = 384 and `fld_resp_out_o` is
  // OUT_LANES x 32 = 128, at the parameters the core instantiates.
  logic               fld_ld_valid_i;
  logic               fld_ld_ready_o;
  logic        [ 1:0] fld_ld_kind_i;
  logic        [ 2:0] fld_ld_slot_i;
  logic        [ 6:0] fld_ld_addr_i;
  logic        [95:0] fld_ld_data_i;
  logic               fld_pc_lu_valid_i;
  logic               fld_pc_lu_ready_o;
  logic        [31:0] fld_pc_lu_hash_i;
  logic               fld_pc_lu_resp_valid_o;
  logic               fld_pc_lu_resp_ready_i;
  logic               fld_pc_lu_hit_o;
  logic        [ 2:0] fld_pc_lu_slot_o;
  logic               fld_pc_cm_valid_i;
  logic               fld_pc_cm_ready_o;
  logic        [31:0] fld_pc_cm_hash_i;
  logic               fld_pc_cm_ok_i;
  logic               fld_pc_cm_resp_valid_o;
  logic               fld_pc_cm_resp_ready_i;
  logic               fld_pc_cm_inserted_o;
  logic               fld_pc_cm_evicted_o;
  logic        [ 2:0] fld_pc_cm_slot_o;
  logic        [ 2:0] fld_stamp_slot_i;
  logic               fld_stamp_slot_valid_i;
  logic               fld_req_valid_i;
  logic               fld_req_ready_o;
  logic        [ 2:0] fld_req_slot_i;
  logic               fld_req_noprog_i;
  logic [383:0]       fld_req_in_i;
  logic               fld_resp_valid_o;
  logic               fld_resp_ready_i;
  logic [127:0]       fld_resp_out_o;
  logic        [ 7:0] fld_resp_status_o;
  logic        [31:0] fld_runs_o;
  logic        [31:0] fld_run_faults_o;
  logic        [31:0] fld_noprog_o;
  logic        [31:0] fld_instr_retired_o;
  logic        [31:0] fld_loads_o;
  logic        [31:0] fld_load_defers_o;
  logic        [31:0] fld_grants_o;
  logic        [31:0] fld_contended_grants_o;
  logic        [31:0] fld_ld_oob_o;
  logic        [31:0] fld_no_result_o;
  logic        [31:0] fld_exec_desync_o;
  logic        [31:0] fld_bank_desync_o;
  logic        [31:0] fld_svc_bank_desync_o;
  logic        [31:0] fld_tag_mismatch_o;
  logic        [31:0] fld_wrong_op_o;
  logic        [31:0] fld_unsupported_o;
  logic        [31:0] fld_skid_overflow_o;
  logic        [31:0] fld_uniform_bad_o;
  logic        [ 2:0] fld_sat_o;
  logic        [31:0] fld_pc_hits_o;
  logic        [31:0] fld_pc_misses_o;
  logic        [31:0] fld_pc_rejected_o;
  logic        [31:0] fld_pc_evictions_o;
  logic        [ 3:0] fld_pc_occupancy_o;
  logic               surf_res_valid_o;
  logic               surf_res_ready_i;
  logic        [11:0] surf_res_texel_o;
  logic        [ 7:0] surf_res_tag_o;
  logic        [ 7:0] surf_res_strength_o;
  logic        [ 7:0] surf_res_before_o;
  logic        [15:0] surf_res_src_id_o;
  logic        [ 1:0] surf_pg_op_o;
  logic        [ 7:0] surf_pg_tag_o;
  logic        [15:0] surf_pg_src_id_o;
  logic [SURF_SLOTS-1:0] surf_res_occupancy_o;
  logic               surf_res_busy_o;
  logic               surf_res_overflow_o;
  logic               surf_sheet_wr_miss_o;
  logic        [15:0] surf_sheet_wr_miss_src_id_o;
  logic               surf_sheet_idle_o;
  logic        [31:0] surf_sheet_texels_touched_o;
  logic               surf_stamp_done_o;
  logic               surf_stamp_rejected_o;
  logic               surf_stamp_idle_o;
  logic        [31:0] surf_stamps_o;
  logic        [31:0] surf_stamp_texels_touched_o;

  logic        phy_cs_n_o;
  logic        phy_ras_n_o;
  logic        phy_cas_n_o;
  logic        phy_we_n_o;
  logic [12:0] phy_a_o;
  logic [1:0]  phy_ba_o;
  logic [15:0] phy_dq_o;
  logic        phy_dq_oe_o;
  logic [1:0]  phy_dqm_o;
  logic [15:0] phy_dq_i;

  // ---- THE COMMAND FRONT (core sections 7b and 7c) -----------------------
  // CMD.DECODER's record headers and verdict, and CMD.EXEC's evidence. All
  // outputs; this bench declares them because `.*` binds by name and a port
  // with no net here is a compile error rather than a floating wire -- which
  // is the whole reason the comment at the head of this block says so.
  //
  // `cmd_exec_*` is what says a packet BECAME CONSOLE STATE rather than merely
  // validating: `cmd_exec_views_o` moves when SetView's mat4fx reached the
  // shared projector's matrix bank, `cmd_exec_stamps_o` when a SurfaceStamp
  // reached SURFACE.STAMP. `cmd_exec_unsupported_o` is expected to be LARGE --
  // it counts every ABI record the executor has no arm for, which today is
  // most of them, and reading it as a fault would be reading the gap as a bug.
  logic        cmd_rec_valid_o;
  logic [15:0] cmd_rec_opcode_o;
  logic [15:0] cmd_rec_bytes_o;
  logic [31:0] cmd_rec_source_id_o;
  logic [31:0] cmd_rec_index_o;
  logic        cmd_decode_done_o;
  logic [ 7:0] cmd_decode_error_o;
  logic [31:0] cmd_bytes_consumed_o;
  logic [31:0] cmd_commands_o;

  // ---- DEBUG.TRACE's evidence --------------------------------------------
  // The arming and the readout that used to be driven from HERE are gone from
  // the core's edge, and their absence is the point of this pass. Entry I45 is
  // closed: arming arrives as a `DebugTraceArm` 0xF003 record in the packet
  // this bench already builds (owner ruling R52), and the readout is tenant 1
  // of the HPS register aperture (ruling R51). THE BENCH NO LONGER STANDS IN
  // FOR A PRODUCER; it is the host at a real host port, which is a different
  // thing and the whole distinction this register measures.
  //
  // The check the old comment argued for is UNCHANGED and is still the one
  // worth having: the ring must store EXACTLY as many events as the decoder
  // reports records walked, less the arming record itself, which by
  // construction is offered to the ring before the arm lands. A count that is
  // merely non-zero would pass with the source id and the sequence swapped.
  logic [ 6:0] dbg_trace_armed_o;
  logic [31:0] dbg_trace_count_o;
  logic [31:0] dbg_trace_dropped_o;

  // ---- HOST.REGWIN, the HPS lightweight bridge (ruling R51) ---------------
  // The bench is the HPS here exactly as it is for the burst bridge and the
  // FRAME_RING word view. `host_read` below is the whole protocol.
  logic        hostreg_valid_i;
  logic        hostreg_write_i;
  logic [15:0] hostreg_addr_i;
  logic [31:0] hostreg_wdata_i;
  logic        hostreg_ready_o;
  logic        hostreg_rvalid_o;
  logic [31:0] hostreg_rdata_o;
  logic        hostreg_err_o;
  logic [31:0] hostreg_reads_o;
  logic [31:0] hostreg_writes_o;
  logic [31:0] hostreg_refused_unmapped_o;
  logic [31:0] hostreg_refused_misaligned_o;
  logic [31:0] hostreg_refused_tenant_o;
  logic [31:0] hostreg_refused_timeout_o;
  logic [31:0] hostreg_stall_cycles_o;

  logic [31:0] cmd_exec_committed_o;
  logic [31:0] cmd_exec_abandoned_o;
  logic [31:0] cmd_exec_views_o;
  logic [31:0] cmd_exec_stamps_o;
  logic [31:0] cmd_exec_stamp_overflow_o;
  logic [31:0] cmd_exec_view_refused_o;
  logic [31:0] cmd_exec_trace_arms_o;
  logic [31:0] cmd_exec_trace_arm_refused_o;
  logic [31:0] cmd_exec_src_truncated_o;
  logic [31:0] cmd_exec_unsupported_o;

  // ---- MEASURE.TOKENS (R18/R33), composed on its budget side -------------
  // The request/return side is the core's BOUNDARY (entry I18): held idle
  // here, so every pool the bench reads is exactly what the packet loaded.
  logic        tok_req_valid_i = 1'b0, tok_req_view_i = 1'b0, tok_req_class_i = 1'b0;
  logic        tok_req_essential_i = 1'b0;
  logic [ 2:0] tok_req_rep_i = 3'd0;
  logic [31:0] tok_req_cost_i = 32'd0;
  logic [15:0] tok_req_src_id_i = 16'd0;
  logic        tok_ret_valid_i = 1'b0, tok_ret_view_i = 1'b0, tok_ret_class_i = 1'b0;
  logic        tok_ret_shared_i = 1'b0;
  logic [31:0] tok_ret_cost_i = 32'd0;
  logic        tok_grant_o, tok_shared_o;
  logic        tok_den_valid_o, tok_den_view_o, tok_den_class_o;
  logic [ 2:0] tok_den_rep_o;
  logic [ 1:0] tok_den_reason_o;
  logic [15:0] tok_den_src_id_o;
  logic [31:0] tok_den_cost_o;
  logic [31:0] tok_avail_geom0_o, tok_avail_geom1_o, tok_avail_frag0_o, tok_avail_frag1_o;
  logic [31:0] tok_avail_shared_o;
  logic [31:0] tok_rep_count0_o, tok_rep_count1_o, tok_rep_count2_o, tok_rep_count3_o;
  logic [31:0] tok_rep_count4_o, tok_rep_count5_o, tok_rep_count6_o, tok_rep_count7_o;
  logic [31:0] tok_triangles_culled_o, tok_vreq_clamped_o, cmd_exec_contracts_o;

  // ---- THE DRAW DISPATCH (core entry I41), added 2026-09-19 --------------
  // `DrawForm 0x0300` lowered by CMD.EXEC's draw arm. Declared here for the
  // `.*` reason above.
  //
  // `cmd_draw_ready_i` IS HELD HIGH AND THAT IS A MODELLED CONSUMER, NOT A
  // TIE-OFF DRESSED UP. The dispatch is the LAST phase of CMD.EXEC's commit,
  // so a ready held low would park the executor in EX_DRAW forever: the next
  // packet's bytes would never be accepted, `cmd_exec_committed_o` would stay
  // at zero, and this bench's command-path checks would fail with no hint of
  // where. High is what an always-available consumer looks like, and the
  // REFUSING consumer is exercised where it belongs -- three ready patterns in
  // `tests/command/cmd_exec_directed.cpp` case 12.

  logic [31:0] cmd_exec_draws_o;
  logic [31:0] cmd_exec_draw_overflow_o;
  logic [31:0] cmd_exec_draw_src_truncated_o;

  // ---- THE PARTICLE DRAW PATH (core entry I24) ---------------------------
  // PART.PROJECT -> PART.LADDER -> {PART.EXPAND, PART.SOFT}, composed
  // 2026-09-19. Declared here for the same reason the command front above is:
  // `.*` binds by name and a port with no net is a compile error.
  //
  // **THE THREE `_ready_i` HERE ARE NOT DECORATION AND MUST NOT BE TIED LOW.**
  // PART.PROJECT takes a THIRD BRANCH of the fork on PART.COLLIDE's output, so
  // a draw path that never drains holds its slots, refuses at `p_ready_o`, and
  // STALLS THE WHOLE PARTICLE TICK -- the ring would stop and this bench's
  // survivor and child checks would fail with no hint of where. That is the
  // correct behaviour (backpressure, never a drop), and it is exactly why the
  // consumer has to be modelled rather than grounded. They are held HIGH: this
  // bench is a wiring smoke and the endpoints' real customer is the absent GEOM
  // replay/setup path the core's I24 names.
  logic signed [31:0] part_prj_base_radius_i;
  logic               part_prj_view_i;
  logic        [15:0] part_prj_trail_i;
  logic               part_prj_narrow_i;
  logic               part_prj_protected_i;
  logic        [ 2:0] part_prj_gov_floor_i;
  logic        [ 2:0] part_prj_prev_rung_i;
  logic        [ 3:0] part_prj_hold_i;
  logic               part_prj_first_i;
  logic        [ 7:0] part_prj_r_i;
  logic        [ 7:0] part_prj_g_i;
  logic        [ 7:0] part_prj_b_i;
  logic        [15:0] part_prj_src_id_i;

  logic               part_rung_valid_o;
  logic               part_rung_ready_i;
  logic        [ 2:0] part_rung_o;
  logic        [ 3:0] part_rung_hold_o;
  logic               part_rung_changed_o;
  logic        [15:0] part_rung_src_id_o;

  logic               part_exp_valid_o;
  logic               part_exp_ready_i;
  logic signed [21:0] part_exp_ax_o;
  logic signed [21:0] part_exp_ay_o;
  logic signed [21:0] part_exp_bx_o;
  logic signed [21:0] part_exp_by_o;
  logic signed [21:0] part_exp_cx_o;
  logic signed [21:0] part_exp_cy_o;
  logic signed [31:0] part_exp_d_o;
  logic        [ 7:0] part_exp_r_o;
  logic        [ 7:0] part_exp_g_o;
  logic        [ 7:0] part_exp_b_o;
  logic               part_exp_depth_test_o;
  logic               part_exp_depth_write_o;
  logic        [15:0] part_exp_src_id_o;

  logic               part_sft_valid_o;
  logic               part_sft_ready_i;
  logic signed [12:0] part_sft_min_x_o;
  logic signed [12:0] part_sft_max_x_o;
  logic signed [12:0] part_sft_min_y_o;
  logic signed [12:0] part_sft_max_y_o;
  logic signed [31:0] part_sft_d_o;
  logic        [ 7:0] part_sft_r_o;
  logic        [ 7:0] part_sft_g_o;
  logic        [ 7:0] part_sft_b_o;
  logic               part_sft_depth_test_o;
  logic               part_sft_depth_write_o;
  logic        [15:0] part_sft_src_id_o;

  logic [31:0] part_prj_projected_o;
  logic [31:0] part_prj_behind_o;
  logic [31:0] part_prj_geom_grants_o;
  logic [31:0] part_prj_part_grants_o;
  logic [31:0] part_prj_contended_o;
  logic [31:0] part_prj_size_sat_o;
  logic [31:0] part_prj_slot_pressure_o;
  logic [31:0] part_prj_tag_collision_o;
  logic [31:0] part_prj_ladder_unexpected_o;
  logic [31:0] part_lad_decisions_o;
  logic [31:0] part_lad_changes_o;
  logic [31:0] part_lad_held_o;
  logic [31:0] part_lad_gov_forced_o;
  logic [31:0] part_exp_polygons_o;
  logic [31:0] part_sft_sprites_o;


  // ---------------------------------------------------------------------
  // TWOD.PLANE / TWOD.SPRITE / TWOD.SAMPLER -- added 2026-09-19.
  // ---------------------------------------------------------------------
  // `zhao_console_core u_core (.*)` binds by NAME, so every port the core
  // grows needs an identically-named net here or the bench will not
  // elaborate at all. The compositor packet added 86 of them and the bench
  // could not be built for the rest of the session -- two other packets
  // each reported it as "not mine", correctly, and it stayed broken
  // because it belonged to whoever looked last.
  //
  // These are DECLARED AND NOT DRIVEN, deliberately. The smoke bench does
  // not exercise the 2D path: the inputs sit at their reset values and the
  // outputs are observed rather than checked. That is the honest claim for
  // a smoke bench -- it proves the composition ELABORATES and the frame
  // path still runs, NOT that a plane or a sprite draws anything. Reading
  // a green smoke run as evidence about the 2D path would be exactly the
  // mistake CLAUDE.md records about gates that cannot reach the state.
  //
  // Generated from the core`s own port list rather than typed, so a width
  // here cannot disagree with the port it binds.
  logic                                twod_pd_valid_i;
  logic                                twod_pd_ready_o;
  logic                                twod_pd_slot_i;
  logic         [1:0]                  twod_pd_role_i;
  logic         [1:0]                  twod_pd_blend_i;
  logic         [7:0]                  twod_pd_opacity_i;
  logic                                twod_pd_format_i;
  logic         [15:0]                 twod_pd_width_i;
  logic         [15:0]                 twod_pd_height_i;
  logic                                twod_pd_wrap_u_i;
  logic                                twod_pd_wrap_v_i;
  logic signed  [31:0]                 twod_pd_a_i;
  logic signed  [31:0]                 twod_pd_b_i;
  logic signed  [31:0]                 twod_pd_c_i;
  logic signed  [31:0]                 twod_pd_d_i;
  logic signed  [31:0]                 twod_pd_u0_i;
  logic signed  [31:0]                 twod_pd_v0_i;
  logic         [1:0]                  twod_pd_view_mask_i;
  logic         [7:0]                  twod_pd_palette_i;
  logic                                twod_sd_valid_i;
  logic                                twod_sd_ready_o;
  logic signed  [15:0]                 twod_sd_x_i;
  logic signed  [15:0]                 twod_sd_y_i;
  logic         [15:0]                 twod_sd_w_i;
  logic         [15:0]                 twod_sd_h_i;
  logic signed  [31:0]                 twod_sd_u_i;
  logic signed  [31:0]                 twod_sd_v_i;
  logic signed  [31:0]                 twod_sd_a00_i;
  logic signed  [31:0]                 twod_sd_a01_i;
  logic signed  [31:0]                 twod_sd_a10_i;
  logic signed  [31:0]                 twod_sd_a11_i;
  logic         [2:0]                  twod_sd_format_i;
  logic         [7:0]                  twod_sd_palette_i;
  logic         [15:0]                 twod_sd_tint_i;
  logic         [1:0]                  twod_sd_blend_i;
  logic         [1:0]                  twod_sd_view_mask_i;
  logic         [7:0]                  twod_sd_order_i;
  logic         [15:0]                 twod_sd_src_id_i;
  logic                                twod_ld_page_we_i;
  logic         [TWOD_PAW-1:0]         twod_ld_page_addr_i;
  logic         [15:0]                 twod_ld_page_data_i;
  logic                                twod_ld_pal_we_i;
  logic         [TWOD_PALAW-1:0]       twod_ld_pal_addr_i;
  logic         [15:0]                 twod_ld_pal_data_i;
  logic                                twod_ld_bind_we_i;
  logic         [TWOD_BSW-1:0]         twod_ld_bind_sel_i;
  logic         [TWOD_PAW-1:0]         twod_ld_bind_base_i;
  logic         [3:0]                  twod_ld_bind_lstride_i;
  logic         [3:0]                  twod_ld_bind_lheight_i;
  logic                                twod_atm_slot_i;
  logic signed  [31:0]                 twod_line_scroll_i;
  logic                                twod_sc_valid_o;
  logic                                twod_sc_ready_i;
  logic         [15:0]                 twod_sc_rgb_o;
  logic signed  [15:0]                 twod_sc_x_o;
  logic signed  [15:0]                 twod_sc_y_o;
  logic         [15:0]                 twod_sc_tint_o;
  logic         [1:0]                  twod_sc_blend_o;
  logic         [7:0]                  twod_sc_order_o;
  logic         [15:0]                 twod_sc_src_id_o;
  logic                                twod_sc_last_o;
  logic         [31:0]                 twod_plane_pixels_o;
  logic         [31:0]                 twod_plane_refused_role_o;
  logic         [31:0]                 twod_plane_refused_blend_o;
  logic         [31:0]                 twod_plane_skipped_view_o;
  logic         [31:0]                 twod_plane_wrap_fail_o;
  logic         [31:0]                 twod_sprite_descriptors_o;
  logic         [31:0]                 twod_sprite_skipped_view_o;
  logic         [31:0]                 twod_sprite_refused_o;
  logic         [31:0]                 twod_sprite_pixels_o;
  logic         [31:0]                 twod_samples_o;
  logic         [31:0]                 twod_plane_samples_o;
  logic         [31:0]                 twod_sprite_samples_o;
  logic         [31:0]                 twod_clut8_samples_o;
  logic         [31:0]                 twod_rgb565_samples_o;
  logic         [31:0]                 twod_texel_wrapped_o;
  logic         [31:0]                 twod_page_oob_o;
  logic         [31:0]                 twod_bind_missing_o;
  logic         [31:0]                 twod_fmt_refused_o;
  logic         [31:0]                 twod_pal_refused_o;
  logic         [31:0]                 twod_skipped_fill_o;
  logic         [31:0]                 twod_atm_underrun_o;
  logic         [31:0]                 twod_walk_stalls_o;
  logic         [31:0]                 twod_sprite_stalls_o;
  logic         [31:0]                 twod_tint_unapplied_o;
  logic         [31:0]                 twod_pair_lost_o;
  // ---- THE DUT, OR ITS POSITIVE CONTROL --------------------------------
  // A PLAIN `ifdef`, SELECTED BY A PLAIN `-D`, AND THAT SHAPE IS DELIBERATE.
  // CLAUDE.md: Verilator's `-D` cannot override a FUNCTION-LIKE `define` and
  // says NOTHING when it fails to, so two mutants once passed while measuring
  // unmutated production. A bare `ifdef` selecting between two instantiations
  // is the form `-D` does reach, and the negative control is built in: without
  // the define this is production, and production must read the counter ZERO
  // under the identical stimulus.
  //
  // `tests/mutants/zhao_console_core_slot_overflow_mutant.sv` is a WRAPPER --
  // it instantiates `zhao_console_core` with TERR_POOL_SLOTS halved -- so it
  // has no copied body to go stale. Its own header carries the argument.
`ifdef ZHAO_MUT_SLOT_OVERFLOW
  zhao_console_core_slot_overflow_mutant dut (.*);
`else
  zhao_console_core dut (.*);
`endif

  // ==========================================================================
  // PACKET P-SURFACE: ONE SurfaceStamp COMMAND, AND NOTHING ELSE
  // ==========================================================================
  // WHAT THIS HARNESS IS ALLOWED TO BE, and it is a narrower licence than the
  // terrain engine below claims. The completion plan lets a harness supply
  // "host packets"; `spec/commands.zidl` carries SurfaceStamp with exactly the
  // fields driven here, so presenting one is presenting a host packet whose
  // decoder does not exist yet (DUT entry I30). What this bench does NOT do is
  // touch the seam under test: SURFACE.STAMP's request, page and write
  // channels are internal to the DUT, so every texel read and every texel
  // written below happens between two real blocks with this file nowhere in
  // the loop.
  //
  // THE NUMBERS ARE THE COMMITTED TEST'S OWN. `tests/surface/
  // surface_stamp_chain.cpp` runs this identical pair against the reference
  // with a 64x64 m envelope centred on the origin and a crack ring, and its
  // `settle()` wires the three channels port for port the same way the DUT
  // does. Reusing its envelope means a disagreement here is about the
  // COMPOSITION rather than about a stimulus nobody has validated.
  //
  // ONE metre per texel: the envelope is 64 m across a 64-texel sheet. A ring
  // of outer radius 6 and width 2 covers an annulus of a few dozen texels --
  // small on purpose, because at SQ_RADIX = 1 each texel costs ~39 cycles and
  // this is a wiring bench, not a throughput measurement.
  localparam int signed SURF_M      = 32'sh0001_0000;  // fx16 one metre
  localparam int signed SURF_ENV_LO = -32 * SURF_M;
  localparam int signed SURF_ENV_HI =  32 * SURF_M;
  localparam logic [31:0] SURF_PATCH_C = 32'h0000_2C01;  // the chain test's handle

  // `stamp_rejected_o`, `res_overflow_o` and `wr_miss_o` are ONE-CYCLE PULSES,
  // so the verdict cannot read them directly -- by the time it runs they are
  // long back at zero, and a zero read at the wrong moment is indistinguishable
  // from a pulse that never happened. They are made sticky here so a single
  // event survives to be asserted on.
  logic surf_rejected_seen_q, surf_overflow_seen_q, surf_wr_miss_seen_q;
  logic surf_done_seen_q;
  logic [31:0] surf_res_records_q;   // stamp_results beats this bench accepted

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      surf_rejected_seen_q <= 1'b0;
      surf_overflow_seen_q <= 1'b0;
      surf_wr_miss_seen_q  <= 1'b0;
      surf_done_seen_q     <= 1'b0;
      surf_res_records_q   <= 32'd0;
    end else begin
      if (surf_stamp_rejected_o) surf_rejected_seen_q <= 1'b1;
      if (surf_res_overflow_o)   surf_overflow_seen_q <= 1'b1;
      if (surf_sheet_wr_miss_o)  surf_wr_miss_seen_q  <= 1'b1;
      if (surf_stamp_done_o)     surf_done_seen_q     <= 1'b1;
      if (surf_res_valid_o && surf_res_ready_i)
        surf_res_records_q <= surf_res_records_q + 32'd1;
    end
  end

  // ---- THE FIELD ENGINE (core entry I42) ----------------------------------
  // NO PROGRAM IS LOADED HERE, and the stamp brush is therefore DISARMED:
  // `fld_stamp_slot_valid_i` is low, the core ANDs it into the stamp's
  // `cmd_field_en_i`, and the stamp runs its ABI path exactly as it did before
  // this composition. That is deliberate -- this bench's surface acceptance
  // below must measure the same stamp it measured yesterday, or the composition
  // would be hiding a regression behind a new feature.
  assign fld_ld_valid_i         = 1'b0;
  assign fld_ld_kind_i          = 2'd0;
  assign fld_ld_slot_i          = 3'd0;
  assign fld_ld_addr_i          = 7'd0;
  assign fld_ld_data_i          = 96'd0;
  assign fld_pc_lu_valid_i      = 1'b0;
  assign fld_pc_lu_hash_i       = 32'd0;
  assign fld_pc_lu_resp_ready_i = 1'b1;
  assign fld_pc_cm_valid_i      = 1'b0;
  assign fld_pc_cm_hash_i       = 32'd0;
  assign fld_pc_cm_ok_i         = 1'b0;
  assign fld_pc_cm_resp_ready_i = 1'b1;
  assign fld_stamp_slot_i       = 3'd0;
  assign fld_stamp_slot_valid_i = 1'b0;
  assign fld_req_slot_i         = 3'd0;
  assign fld_req_noprog_i       = 1'b0;
  assign fld_req_in_i           = 384'd0;
  assign fld_resp_ready_i       = 1'b1;

  // ---- THE ONE THING THIS BENCH ASKS THE ENGINE -----------------------------
  // A single request on the edge client, at a slot nothing was ever loaded
  // into. The right answer is a REFUSAL, and the point of asking is that a
  // refusal can only come back if the whole path exists: the core's port, the
  // arbiter, the slot's `hdr_loaded` bit, the response bus and the status.
  //
  // IT IS DELIBERATELY NOT A SUCCESSFUL RUN. Loading a program here would mean
  // choosing one, and a composed engine that returns a plausible number is
  // exactly the thing a smoke bench cannot tell apart from a composed engine
  // that returns a plausible number for the wrong reason. A refusal has a
  // named status and a counter, and neither can be produced by accident.
  logic fld_probe_done_q;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      fld_req_valid_i  <= 1'b0;
      fld_probe_done_q <= 1'b0;
    end else if (!fld_probe_done_q) begin
      if (fld_req_valid_i && fld_req_ready_o) begin
        fld_req_valid_i  <= 1'b0;
        fld_probe_done_q <= 1'b1;
      end else begin
        fld_req_valid_i <= 1'b1;
      end
    end
  end

  // I32: TERRAIN.BAKE does not exist, so this bench plays its consumer and is
  // ALWAYS READY. That is not a convenience: `s2_accept` inside SURFACE.STAMP
  // requires the RESULT beat to be taken as well as the write, so a low
  // `res_ready` would stall the pair and both texel counters would read zero
  // for a reason that has nothing to do with the seam under test.
  assign surf_res_ready_i = 1'b1;

  // The command's payload is constant; only `valid` moves. Holding the fields
  // steady is what the block's contract expects of a dispatch and it keeps the
  // handshake below the only moving part.
  assign surf_cmd_handle_i     = SURF_PATCH_C;
  assign surf_cmd_operation_i  = 8'd0;        // ABI operation 0
  assign surf_cmd_tag_i        = 8'd1;
  assign surf_cmd_strength_i   = 16'hD000;    // the low byte is discarded by law
  assign surf_cmd_tx_i         = 32'sd0;
  assign surf_cmd_ty_i         = 32'sd0;
  assign surf_cmd_radius_i     = 6 * SURF_M;
  assign surf_cmd_ring_width_i = 2 * SURF_M;  // a crack ring, not a filled disc
  // (the bench drove the envelope and the policy here. It does not any more:
  //  u_surface_dispatch derives both, and SURF_ENV_LO/HI below are kept only
  //  as the numbers the stamp's own geometry comment is written against.)
  assign surf_cmd_src_id_i     = 16'd4242;

  // ONE command, offered once and withdrawn on its ACCEPTANCE -- the same
  // shape every other producer in this bench uses, and for the reason recorded
  // in CLAUDE.md: a `valid` held across a whole offer window re-submitted one
  // meshlet fifteen times and every result still matched. `surf_stamps_o` is
  // asserted to be exactly 1 below, which is what catches the inverse.
  logic surf_cmd_offered_q;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      surf_cmd_valid_i   <= 1'b0;
      surf_cmd_offered_q <= 1'b0;
    end else begin
      if (surf_cmd_valid_i && surf_cmd_ready_o) surf_cmd_valid_i <= 1'b0;
      else if (reset_released_q && !surf_cmd_offered_q) begin
        surf_cmd_valid_i   <= 1'b1;
        surf_cmd_offered_q <= 1'b1;
      end
    end
  end

  // ==========================================================================
  // PACKET P-TERRAIN: THE PLAYED HPS BRIDGE AND MEM.GUARD
  // ==========================================================================
  // WHAT THIS IS ALLOWED TO BE. The completion plan's sentence is the licence
  // and also the limit: the harness supplies "external clocks, input events,
  // MEMORY BEHAVIOR and host packets. It does NOT supply missing lighting,
  // FIELD results, particle updates, prepared triangles or fake material
  // records." A bridge that returns the bytes at an address is memory
  // behaviour. Nothing below invents a patch, a height or a completion: every
  // terrain counter this bench checks moves only because a real block read a
  // real byte and handed a real record to the next real block.
  //
  // ONE ENGINE, because the DUT presents ONE bridge port: TERRAIN.CMD and
  // TERRAIN.PAGELOADER are merged inside the core by the real
  // `zhao_hps_arbiter`. That the two of them share this port WITHOUT either
  // starving is itself part of what this bench exercises.
  localparam logic [31:0] HPS_BASE    = 32'h2000_0000; // the staging arena
  localparam int unsigned PAGE_BYTES_C = 21376;        // terrain_rules sec 2/7
  localparam int unsigned LIST_OFF_C   = 0;
  // ==========================================================================
  // THREE RECORDS, AND THE REASON IT WAS ONE -- THE DEFECT WAS IN THIS FILE
  // ==========================================================================
  // THE HISTORY, KEPT BECAUSE THE WRONG DIAGNOSIS IS THE INSTRUCTIVE PART.
  // This bench shipped at N_TERR_REC = 1 on 2026-09-19 with a note saying that
  // at 2 or more "every record after the first arrives all-zero", and locating
  // the loss "at or before TERRAIN.SEQ's capture of the record stream, i.e. in
  // the TERRAIN.CMD -> TERRAIN.SEQ seam". The measurements quoted were real:
  //
  //   N=3:  cmd records=3, seq consumed=3, claims=3/1(same),
  //         pl loaded=0 faulted=1 refused=2, fault island=0 ix=0 iz=0 src_id=0
  //
  // and so was the reasoning from them -- a claim key is {epoch, island, ix,
  // iz}, SEQ builds it from the record, two zero keys collide, hence the extra
  // `same`. Every step of that is correct. The CONCLUSION was wrong, because
  // it took one fact for granted: that the bytes were in memory to begin with.
  //
  // THEY WERE NOT. The arena writer below declared its word index as
  // `int unsigned b = ...` inside the loop body of an `initial` block. A
  // variable declared in a begin/end of a STATIC scope has static lifetime, so
  // its initialiser runs ONCE before time zero and never again: `b` was 0 for
  // every r, all three records were written over each other at word 0, and the
  // arena held the LAST record followed by zeros. Traced at the DUT's own CMD
  // port, the very first burst of pass one reads
  //
  //   20000000: 0009000500000042 0000000020006380 00010001deadbeef 00000000000003ea
  //   20000020: 0000000000000000 0000000000000000 0000000000000000 0000000000000000
  //
  // -- one record (ix=5, iz=9, src=1002, which is r=2) and then nothing.
  // TERRAIN.CMD read exactly what was there and emitted exactly what it read;
  // TERRAIN.SEQ claimed exactly what it was handed; TERRAIN.PAGELOADER refused
  // a job whose page address was 0 with verdict 6 (V_SRC_ARENA), which is the
  // CORRECT refusal for an address outside the staging arena. Every block on
  // the spine behaved correctly on bad input, and the bad input was ours.
  //
  // AND `crc_fails=0` IS WHY IT LOOKED LIKE THE TREE'S FAULT. That zero was
  // quoted as proof that CMD "had every byte of every record in hand". It is
  // not: this bench seals the list with the SAME `zhao_crc32c_fold` over the
  // SAME words it wrote, so the two sides of that comparison were corrupted
  // together and it could not fire. The declaration below already says this
  // bench is "NO EVIDENCE WHATEVER about the CRC itself"; the note that shipped
  // used it as evidence about the RECORDS anyway. A detector wired to two
  // operands that move together cannot fire (CLAUDE.md) -- and the reassuring
  // reading is what let the fault be handed to the spine.
  //
  // WHAT GUARDS IT NOW: `automatic` on the index, a page address per record
  // rather than a shared one, and an independent read-back of the arena that
  // $fatals at time zero naming the record. See the layout loop.
  //
  // N_TERR_REC IS THE KNOB AND THREE IS ITS FLOOR, not a decoration: one record
  // is what hid this, and it takes three to make the directory's `same` answer
  // and the second re-request of an abandoned burst both happen.
  localparam int unsigned N_TERR_REC   = 3;
  localparam int unsigned LIST_BYTES_C = N_TERR_REC * 32;
  localparam int unsigned PAGE0_OFF_C  = 4096;
  // THE WINDOW IS DERIVED, NOT TYPED. It was a hand-written 64 KiB while the
  // records shared one page; a per-record page makes the arena a function of
  // N_TERR_REC, and a constant somebody has to remember to raise alongside it
  // is a constant that will be wrong. The last page ends exactly at the arena
  // end, which TERRAIN.PAGELOADER allows: its test is `src_hi > arena_hi`.
  localparam int unsigned ARENA_BYTES_C = PAGE0_OFF_C + N_TERR_REC * PAGE_BYTES_C;
  localparam int unsigned HPS_WORDS     = (ARENA_BYTES_C + 7) / 8;

  logic [63:0] hps_mem [0:HPS_WORDS-1];

  // The frozen sim profile the bridge's own comment names: a latency to first
  // beat, then one beat per cycle. Held as a small state machine rather than a
  // combinational echo, because a zero-latency bridge hides every handshake
  // bug this composition could contain.
  localparam int unsigned HPS_LAT = 16;

  int unsigned  hps_state_qq;    // 0 idle, 1 waiting, 2 streaming
  int unsigned  hps_bursts_served_q;

  // ---- THE PARTICLE BUFFERS, in the played DDR (entry I1 closed) ----------
  // PART.STATE.md: "Dense sequential ping-pong in HPS DDR". The bench is the
  // HPS, so it owns the two buffers, places the first generation in buffer 0
  // before the machine starts, and seeds {buffer 0, N_PART_RECORDS}. After
  // that every record PART.STATE sees has come OUT of this array through the
  // bridge socket, and every record it writes goes back IN -- which is what
  // the write arm below is for. It is the only region the played bridge
  // accepts a write into; a write anywhere else is still answered with `err`.
  // 0x3100_0000, not 0x3000_0000: that is MEM.UPLOAD's staging arena, and
  // since entry I26 closed both are served by the ONE shell bridge below.
  localparam logic [31:0] PART_HPS_BASE0 = 32'h3100_0000;
  localparam logic [31:0] PART_HPS_BASE1 = 32'h3100_1000;
  localparam int unsigned PART_HPS_WORDS = 1024;          // 2 x 256 records
  logic [63:0] part_mem [0:PART_HPS_WORDS-1];

  function automatic bit in_part_region(input logic [31:0] a);
    return (a >= PART_HPS_BASE0) && (a < PART_HPS_BASE0 + 32'(PART_HPS_WORDS * 8));
  endfunction


  // What crossed the socket, as whole records -- the evidence that replaces
  // the old `part_wr_*` / `part_rd_*` ports.
  logic                  part_ddr_wr_rec_valid_q;
  logic [PART_REC_W-1:0] part_ddr_wr_rec_q;
  logic [63:0]           part_ddr_wr_lo_q;
  logic                  part_ddr_wr_half_q;
  int unsigned           part_ddr_rd_beats_q;

  function automatic logic [63:0] hps_read(input logic [31:0] byte_addr);
    int unsigned w;
    if (in_part_region(byte_addr)) return part_mem[(byte_addr - PART_HPS_BASE0) >> 3];
    if (byte_addr < HPS_BASE) return 64'd0;
    w = (byte_addr - HPS_BASE) >> 3;
    if (w >= HPS_WORDS) return 64'd0;
    return hps_mem[w];
  endfunction

  // ---- the SHELL's HPS port: the FRAME RING and the upload arena ------------
  // Until 2026-09-19 the shell's own HPS pins were tied low, so no command
  // packet ever reached this console and CMD.DECODER / CMD.EXEC / DEBUG.TRACE
  // walked nothing (the note at the DEBUG.TRACE check said so). The bench now
  // plays the far side of the shell's REAL `zhao_hps_bridge`, exactly as it
  // plays the terrain spine's: a grant, a fixed latency, then len/8 beats.
  //
  // It serves TWO regions and says so loudly for anything else:
  //   * FRAME_RING slot 0's body at RING_BASE (0) + the 4-KiB descriptor table
  //     -- one sealed packet, built below from the GENERATED packers;
  //   * MEM.UPLOAD's staging arena -- the bytes that packet's PublishResource
  //     names.
  // A write, or a read anywhere else, is something this bench has no bytes
  // for, and serving zeros would be inventing them.
  localparam logic [31:0] RING_SLOT0_C  = 32'h0000_1000;   // RING_BASE + DESC_TABLE
  localparam int unsigned PKT_MAX_C     = 768;   // records + header + CRC; the `ro` accumulator below is the arithmetic (thirteen records, SetView 112 B since R63)
  // The token counts the packet carries (R18/R33): the CONTRACT's ceiling per
  // view and class, and view 1's SetView REQUEST -- geometry ABOVE its ceiling
  // (so it is clamped, and counted) and fragment BELOW it (so it lowers the
  // allowance). Counts, as authored; nothing converts them.
  localparam logic [31:0] TOK_G0_C = 32'd40000, TOK_G1_C = 32'd30000;
  localparam logic [31:0] TOK_F0_C = 32'd90000, TOK_F1_C = 32'd80000;
  localparam logic [31:0] TOK_SH_C = 32'd5000;
  localparam logic [31:0] TOK_REQ_G1_C = 32'd50000, TOK_REQ_F1_C = 32'd20000;
  // THE LOOK THE PACKET CARRIES (R36): every value distinctive, the picture
  // still the identity (see build_packet).
  localparam logic [7:0]        SP_GAIN_C   = 8'h5A;
  localparam logic signed [8:0] SP_BIAS_R_C = -9'sd256;
  localparam logic signed [8:0] SP_BIAS_G_C = 9'sd37;
  localparam logic signed [8:0] SP_BIAS_B_C = 9'sd255;
  localparam logic [15:0]       SP_FLASH_C  = 16'hF81F;
  localparam logic [15:0]       SP_INK_C    = 16'h07E0;
  // Byte k of the SetGradeTable's 72 vector bytes: distinctive, never 0.
  function automatic logic [7:0] sm_grade_byte(input int unsigned k);
    sm_grade_byte = 8'(8'h11 + 8'(k * 37));
  endfunction
  // The shell's and the core's paths: the mutant build's DUT is a wrapper.
`ifdef ZHAO_MUT_SLOT_OVERFLOW
  `define PC_CORE dut.u_dut
`else
  `define PC_CORE dut
`endif
  localparam logic [31:0] UPL_ARENA_C   = 32'h3000_0000;
  localparam int unsigned UPL_WORDS_C   = 32;               // 256 B, four bursts
  // The top 4 KiB of RENDER.ASSET_POOL (0x06A0_0000 + 0x0160_0000), which
  // MEM.GUARD's TERRAIN_BUILD arm may WRITE inside this one region since owner
  // ruling R32 -- and which ENGINE1, MATERIAL.RESOLVE's fetch identity, may
  // read. Well clear of the geometry assets the replay reads.
  localparam logic [31:0] UPL_REGION_C  = 32'h07FF_F000;
  localparam logic [31:0] UPL_REGION_SZ = 32'h0000_1000;
  // The request the packet carries, named once so the checks can compare the
  // published row against it.
  localparam logic [23:0] UPL_INDEX_C   = 24'h00_ABCD;
  localparam logic [ 7:0] UPL_KIND_C    = 8'd11;            // spec/cartridge.md 4a: MATERIAL_SET
  // Slot 1: MATERIAL.RESOLVE's directory has SETS = 4 entries and the arena
  // slot IS the entry, so a MATERIAL_SET must land in slot 0..3 to be found.
  localparam logic [ 7:0] UPL_SLOT_C    = 8'd1;
  localparam logic [15:0] UPL_GEN_C     = 16'h0102;
  localparam logic [15:0] UPL_EPOCH_C   = 16'd9;
  // ---- the MESH_STREAM page, owner ruling R29 -----------------------------
  // A SECOND PublishResource in the same packet, of kind 12, carrying the
  // geometry fixture: header, descriptor, index run and vertex records, in one
  // immutable page. It lands 1 KiB into the same writable region as the
  // MATERIAL_SET, 64-byte aligned because the descriptor table must be, and
  // GEOM.DRAWJOB resolves the draw's `form` handle to it through the 5f.1
  // directory MEM.UPLOAD publishes.
  localparam logic [23:0] MSH_INDEX_C   = 24'h00_1234;
  localparam logic [ 7:0] MSH_KIND_C    = 8'd12;            // MESH_STREAM
  localparam logic [ 7:0] MSH_SLOT_C    = 8'd2;
  localparam logic [15:0] MSH_GEN_C     = 16'h0055;
  localparam logic [31:0] MSH_DST_C     = UPL_REGION_C + 32'h0000_0400;
  // R57: 704 B = 64 header + 3 x 64 descriptor table + 3 x 64 index runs +
  // 8 x 32 vertex records. The three meshlets SHARE the vertex run -- each
  // descriptor names the same `vertex_offset` -- so the page grew by the
  // descriptor table and the index runs only.
  localparam int unsigned MSH_WORDS_C   = 88;
  localparam int unsigned MSH_ARENA_W   = UPL_WORDS_C;      // words into the arena
  // ---- the SPECIES_TABLE page, owner ruling R42 (core entry I33) -----------
  // A THIRD PublishResource, of kind 13, carrying the four species descriptors
  // this bench used to write straight into PART.TABLE's load port. The port is
  // gone: the descriptors are DATA in a page now, and `u_part_table_loader`
  // carries them. Layout frozen by `zref::species_page`: a 64-byte header then
  // 32-byte entries, TWO per line.
  localparam logic [23:0] SPT_INDEX_C   = 24'h00_5EED;
  localparam logic [ 7:0] SPT_KIND_C    = 8'd13;            // SPECIES_TABLE
  localparam logic [ 7:0] SPT_SLOT_C    = 8'd3;
  localparam logic [15:0] SPT_GEN_C     = 16'h0007;
  localparam logic [31:0] SPT_DST_C     = UPL_REGION_C + 32'h0000_0800;   // MOVED 2026-09-20 (coordinator merge): the 704-byte MESH_STREAM page at +0x400 runs to +0x6C0, so +0x600 landed INSIDE it and overwrote vertex records -- the smoke reported 18 degenerate normals, six blocks downstream
  localparam int unsigned SPT_ENTRIES_C = 4;
  localparam int unsigned SPT_WORDS_C   = 8 + (4 * SPT_ENTRIES_C);  // 64 B + 32 B each
  localparam int unsigned SPT_ARENA_W   = UPL_WORDS_C + MSH_WORDS_C;
  localparam logic [31:0] SPT_MAGIC_C   = 32'h5450_535A;    // 'ZSPT' little-endian
  localparam int unsigned UPL_ALL_WORDS = UPL_WORDS_C + MSH_WORDS_C + SPT_WORDS_C;
  // The Loom node the draw's transform handle names, and the draw's own
  // identity. `SMK_DRAW_SRC_C` becomes the meshlet's src_id, which is what the
  // vertex records are attributed to downstream -- it was the job port's
  // `instance_id` before R29 and is the DrawForm record's source id now.
  localparam int unsigned SMK_XFORM_NODE_C = 5;
  localparam logic [15:0] SMK_DRAW_SRC_C   = 16'h00A1;
  // R41: the handle SetPopulation names the pool by. Arbitrary and DISTINCT,
  // so part_pop_handle_o reading it back proves the record travelled rather
  // than the bank powering up in a state that happens to match.
  localparam logic [31:0] SMK_POP_HANDLE_C = 32'h0051_C0DE;
  logic [63:0] upl_mem [0:UPL_ALL_WORDS-1];
  logic [255:0] upl_rec0;         // record 0 of the uploaded MATERIAL_SET
  logic [31:0]  upl_crc_material_q, upl_crc_mesh_q, upl_crc_species_q;
  logic [ 7:0] pkt_mem [0:PKT_MAX_C-1];
  int unsigned pkt_len_q;
  logic        pkt_armed_q;       // set by the initial block once the packet is built

  int unsigned  sh_state_q, sh_wait_q, sh_beats_q, sh_bursts_q, sh_pkt_bursts_q;
  logic [31:0]  sh_addr_q;
  logic         sh_is_pkt_q;

  function automatic logic [63:0] ring_read(input logic [31:0] a);
    logic [63:0] w;
    w = '0;
    for (int unsigned k = 0; k < 8; k++)
      if ((a - RING_SLOT0_C + 32'(k)) < 32'(PKT_MAX_C))
        w[8*k +: 8] = pkt_mem[(a - RING_SLOT0_C) + 32'(k)];
    return w;
  endfunction

  // ONE ENGINE SINCE ENTRY I26 CLOSED (terrain3, 2026-09-19). The terrain
  // spine's bridge port used to leave the core and be played by its own engine
  // above; it is now the TERRAIN.BUILD socket's HPS client 1, merged with
  // CMD.DMA, DEBUG.FRAMEBLIT and MEM.UPLOAD by the shell's real
  // `zhao_hps_arbiter_n` and carried by the shell's real `zhao_hps_bridge`. So
  // the far side of that bridge now plays FOUR regions -- the ring slot, the
  // upload arena, the terrain staging arena (TERRAIN.CMD's list and the pages)
  // and the particle ping-pong -- and accepts WRITES into the particle region
  // only, which is the only thing in this console that writes HPS DDR here.
  // A write anywhere else, or a read of bytes this bench does not have, is
  // still $fatal: serving zeros would be inventing them.
  //
  // The engine's state encodings are the old terrain engine's (0 idle, 1 wait,
  // 2 stream, 3 write wait, 4 write accept), so `hps_state_qq`,
  // `hps_bursts_served_q` and the particle evidence keep their meanings.
  function automatic int unsigned hps_region(input logic [31:0] a);
    if ((a >= RING_SLOT0_C) && (a < RING_SLOT0_C + 32'(PKT_MAX_C))) return 1;
    if ((a >= UPL_ARENA_C) && (a < UPL_ARENA_C + 32'(UPL_ALL_WORDS * 8))) return 2;   // both PublishResource arenas (geom3's MESH_STREAM page)
    if (in_part_region(a)) return 3;
    if ((a >= HPS_BASE) && (a < HPS_BASE + 32'(HPS_WORDS * 8))) return 4;
    return 0;
  endfunction

  int unsigned sh_region_q;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      sh_state_q      <= 0;
      sh_wait_q       <= 0;
      sh_beats_q      <= 0;
      sh_bursts_q     <= 0;
      sh_pkt_bursts_q <= 0;
      sh_addr_q       <= 32'd0;
      sh_is_pkt_q     <= 1'b0;
      sh_region_q     <= 0;
      hps_state_qq    <= 0;
      hps_bursts_served_q <= 0;
      hps_req_grant_i <= 1'b0;
      hps_rd_valid_i  <= 1'b0;
      hps_rd_data_i   <= 64'd0;
      hps_rd_last_i   <= 1'b0;
      part_ddr_wr_rec_valid_q <= 1'b0;
      part_ddr_wr_rec_q       <= '0;
      part_ddr_wr_lo_q        <= 64'd0;
      part_ddr_wr_half_q      <= 1'b0;
      part_ddr_rd_beats_q     <= 0;
    end else begin
      hps_req_grant_i <= 1'b0;
      hps_rd_valid_i  <= 1'b0;
      hps_rd_last_i   <= 1'b0;
      part_ddr_wr_rec_valid_q <= 1'b0;
      case (sh_state_q)
        0: if (hps_req_valid_o) begin
             if ((hps_req_len_o == 7'd0) || (hps_req_len_o[2:0] != 3'd0))
               $fatal(1, "SMOKE: a malformed HPS burst reached the bridge's far side (addr %08x len %0d)",
                      hps_req_addr_o, hps_req_len_o);
             if (hps_req_write_o) begin
               if (!in_part_region(hps_req_addr_o))
                 $fatal(1, "SMOKE: the shell's HPS port was asked for a WRITE at %08x -- only PART.STATE's generation buffers are writable here",
                        hps_req_addr_o);
               hps_req_grant_i <= 1'b1;
               sh_addr_q       <= hps_req_addr_o;
               sh_region_q     <= 3;
               sh_state_q      <= 4;
               hps_state_qq    <= 4;
             end else begin
               if (hps_region(hps_req_addr_o) == 0)
                 $fatal(1, "SMOKE: the shell's HPS port was asked for a read at %08x -- this bench plays FRAME_RING slot 0, MEM.UPLOAD's arena, the terrain staging arena and the particle buffers",
                        hps_req_addr_o);
               sh_is_pkt_q     <= (hps_region(hps_req_addr_o) == 1);
               sh_region_q     <= hps_region(hps_req_addr_o);
               hps_req_grant_i <= 1'b1;
               sh_addr_q       <= hps_req_addr_o;
               sh_beats_q      <= hps_req_len_o >> 3;
               sh_wait_q       <= HPS_LAT;
               sh_state_q      <= 1;
               hps_state_qq    <= 1;
             end
           end
        1: if (sh_wait_q > 1) sh_wait_q <= sh_wait_q - 1;
           else begin sh_state_q <= 2; hps_state_qq <= 2; end
        2: begin
             hps_rd_valid_i <= 1'b1;
             hps_rd_data_i  <= (sh_region_q == 1) ? ring_read(sh_addr_q)
                             : (sh_region_q == 2) ? upl_mem[(sh_addr_q - UPL_ARENA_C) >> 3]
                             : hps_read(sh_addr_q);
             hps_rd_last_i  <= (sh_beats_q == 1);
             if (sh_region_q == 3) part_ddr_rd_beats_q <= part_ddr_rd_beats_q + 1;
             sh_addr_q      <= sh_addr_q + 32'd8;
             sh_beats_q     <= sh_beats_q - 1;
             if (sh_beats_q == 1) begin
               sh_state_q   <= 0;
               hps_state_qq <= 0;
               case (sh_region_q)
                 1: sh_pkt_bursts_q <= sh_pkt_bursts_q + 1;
                 2: sh_bursts_q     <= sh_bursts_q + 1;
                 default: hps_bursts_served_q <= hps_bursts_served_q + 1;
               endcase
             end
           end
        4: if (hps_wr_valid_o) begin
             // The bridge streams a granted write burst one beat per cycle while
             // its own `wr_ready` level is high; the far side takes each beat.
             part_mem[(sh_addr_q - PART_HPS_BASE0) >> 3] <= hps_wr_data_o;
             sh_addr_q <= sh_addr_q + 32'd8;
             // two beats, low half first, make one particle128 record
             if (!part_ddr_wr_half_q) begin
               part_ddr_wr_lo_q   <= hps_wr_data_o;
               part_ddr_wr_half_q <= 1'b1;
             end else begin
               part_ddr_wr_rec_q       <= {hps_wr_data_o, part_ddr_wr_lo_q};
               part_ddr_wr_rec_valid_q <= 1'b1;
               part_ddr_wr_half_q      <= 1'b0;
             end
             if (hps_wr_last_o) begin
               sh_state_q          <= 0;
               hps_state_qq        <= 0;
               hps_bursts_served_q <= hps_bursts_served_q + 1;
             end
           end
        default: begin sh_state_q <= 0; hps_state_qq <= 0; end
      endcase
    end
  end

  // ---- the FRAME_RING's descriptor word view (harness-as-HPS, plan D10) -----
  // Slot 0 becomes READY once the packet is built and the console is out of
  // reset. The FPGA's own ring writes (DONE, then FREE) are applied to the
  // word view as HPS DDR would hold them, so the slot is claimed ONCE and is
  // not re-executed at the next tick -- the upload would publish twice.
  int unsigned ring_writes_q;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      hps_state_i     <= '{default: '0};
      hps_byte_len_i  <= '{default: '0};
      ring_wr_ready_i <= 1'b1;
      ring_writes_q   <= 0;
    end else begin
      // The HPS producer's own walk, charter 7.4: FREE -> ARM_WRITING while the
      // body is written, then READY once it is sealed. CMD.SCHEDULER follows
      // the word forward-only and does not take FREE -> READY in one step.
      if (pkt_armed_q && reset_released_q && (ring_writes_q == 0)) begin
        if (hps_state_i[0] == 2'd0) begin
          hps_state_i[0] <= 2'd1;               // ARM_WRITING
        end else if (hps_state_i[0] == 2'd1) begin
          hps_state_i[0]    <= 2'd2;            // READY
          hps_byte_len_i[0] <= 32'(pkt_len_q);
        end
      end
      if (ring_wr_valid_o && ring_wr_ready_i) begin
        hps_state_i[ring_wr_slot_o] <= ring_wr_state_o;
        ring_writes_q <= ring_writes_q + 1;
      end
    end
  end

  // ---- what the packet's upload produced ------------------------------------
  int unsigned  upl_done_seen_q, upl_pub_seen_q;
  logic [7:0]   upl_status_seen_q;
  logic [7:0]   upl_pub_slot_q, upl_pub_tag_q;
  logic [15:0]  upl_pub_gen_q;
  logic [23:0]  upl_pub_index_q;
  logic [31:0]  upl_pub_base_q, upl_pub_extent_q;
  // TWO publications now (R29): the MATERIAL_SET and the MESH_STREAM page the
  // draw names. They are latched SEPARATELY, by kind, because one pair of
  // registers holding "the last row" would let either check pass on the other
  // resource's publication.
  logic [7:0]   msh_pub_slot_q, msh_pub_tag_q;
  logic [15:0]  msh_pub_gen_q;
  logic [23:0]  msh_pub_index_q;
  logic [31:0]  msh_pub_base_q, msh_pub_extent_q;
  int unsigned  msh_pub_seen_q;

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      upl_done_seen_q   <= 0;
      upl_pub_seen_q    <= 0;
      upl_status_seen_q <= 8'hFF;
      upl_pub_slot_q    <= '0;
      upl_pub_tag_q     <= '0;
      upl_pub_gen_q     <= '0;
      upl_pub_index_q   <= '0;
      upl_pub_base_q    <= '0;
      upl_pub_extent_q  <= '0;
      msh_pub_seen_q    <= 0;
      msh_pub_slot_q    <= '0;
      msh_pub_tag_q     <= '0;
      msh_pub_gen_q     <= '0;
      msh_pub_index_q   <= '0;
      msh_pub_base_q    <= '0;
      msh_pub_extent_q  <= '0;
    end else begin
      if (upl_done_o) begin
        upl_done_seen_q   <= upl_done_seen_q + 1;
        upl_status_seen_q <= upl_status_o;
      end
      // TWO publications now (R29), latched SEPARATELY BY KIND: one pair of
      // registers holding 'the last row' would let either check pass on the
      // other resource's publication.
      if (upl_publish_valid_o && (upl_publish_tag_o == MSH_KIND_C)) begin
        msh_pub_seen_q   <= msh_pub_seen_q + 1;
        msh_pub_slot_q   <= upl_publish_slot_o;
        msh_pub_tag_q    <= upl_publish_tag_o;
        msh_pub_gen_q    <= upl_publish_generation_o;
        msh_pub_index_q  <= upl_publish_index_o;
        msh_pub_base_q   <= upl_publish_base_o;
        msh_pub_extent_q <= upl_publish_extent_o;
      end
      if (upl_publish_valid_o && (upl_publish_tag_o == UPL_KIND_C)) begin
        upl_pub_seen_q   <= upl_pub_seen_q + 1;
        upl_pub_slot_q   <= upl_publish_slot_o;
        upl_pub_tag_q    <= upl_publish_tag_o;
        upl_pub_gen_q    <= upl_publish_generation_o;
        upl_pub_index_q  <= upl_publish_index_o;
        upl_pub_base_q   <= upl_publish_base_o;
        upl_pub_extent_q <= upl_publish_extent_o;
      end
    end
  end
  // ---- ONE MATERIAL RESOLVE, once the MATERIAL_SET has been published -------
  // Request = the handle the PublishResource named ({index, low 8 bits of the
  // new residency generation}) and material 0. Its REQUEST is a boundary
  // (I49); what is real is the DIRECTORY it hits (MEM.UPLOAD's publication)
  // and the FETCH it issues (requester C, the real MEM.GUARD).
  logic         mat_fired_q;
  int unsigned  mat_rsp_seen_q;
  logic [2:0]   mat_status_seen_q;
  logic         mat_rec_has_q;
  logic [255:0] mat_rec_q;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      mat_req_valid_i        <= 1'b0;
      mat_req_material_set_i <= '0;
      mat_req_material_id_i  <= '0;
      mat_req_quality_tier_i <= '0;
      mat_rsp_ready_i        <= 1'b1;
      mat_fired_q            <= 1'b0;
      mat_rsp_seen_q         <= 0;
      mat_status_seen_q      <= 3'd7;
      mat_rec_has_q          <= 1'b0;
      mat_rec_q              <= '0;
    end else begin
      if ((upl_pub_seen_q != 0) && !mat_fired_q && !mat_req_valid_i) begin
        mat_req_valid_i        <= 1'b1;
        mat_req_material_set_i <= {UPL_INDEX_C, UPL_GEN_C[7:0]};
        mat_req_material_id_i  <= 16'd0;
      end
      if (mat_req_valid_i && mat_req_ready_o) begin
        mat_req_valid_i <= 1'b0;
        mat_fired_q     <= 1'b1;
      end
      if (mat_rsp_valid_o && mat_rsp_ready_i) begin
        mat_rsp_seen_q    <= mat_rsp_seen_q + 1;
        mat_status_seen_q <= mat_rsp_status_o;
        mat_rec_has_q     <= mat_rsp_has_record_o;
        mat_rec_q         <= mat_rsp_record_o;
      end
    end
  end
  // ---- the played MEM.GUARD write window -----------------------------------
  // TERRAIN.PAGELOADER writes the page into TERRAIN.PAGE_POOL through a guard
  // client. The real `zhao_mem_guard` gives ZHAO_CLIENT_TERRAIN_BUILD a
  // write-only window over that pool; this model accepts inside the pool and
  // DENIES outside it, rather than accepting everything -- an always-ok guard
  // would make `terr_pl_guard_denied_o` a counter that cannot move, which is
  // the decoration this project's own rules forbid.
  localparam logic [ZHAO_VRAM_ADDR_BITS-1:0] POOL_BASE_C  = 27'h400_0000;
  localparam int unsigned                    POOL_SLOTS_C = 1024;

  // THE PLAYED GUARD WINDOW THAT STOOD HERE IS GONE (terrain3, entry I26).
  // TERRAIN.PAGELOADER's writes now reach the shell's REAL `zhao_mem_guard`
  // through `u_build_share`, land in the REAL SDRAM model through the real
  // arbiter, and the compose path reads them back the same way. The history
  // of this model's verdict-timing bug is kept in the git log; the real guard
  // implements the two-cycle law the note described.
  // ---- WHERE THE SPINE GOT TO, watched at the DUT's own edge ---------------
  // The terrain spine's own bridge and guard ports are internal since entry I26
  // closed, so these probes watch the one bridge's far side and the socket
  // share's counters instead: HPS request cycles and read beats across ALL
  // bridge clients, and the socket's contention.
  int unsigned pr_hps_req_cy, pr_hps_beats;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      pr_hps_req_cy <= 0;
      pr_hps_beats  <= 0;
    end else begin
      if (hps_req_valid_o) pr_hps_req_cy <= pr_hps_req_cy + 1;
      if (hps_rd_valid_i)  pr_hps_beats  <= pr_hps_beats + 1;
    end
  end

  // ---- folding the list's CRC with the PRODUCTION folder -------------------
  // WHAT THIS IS AND IS NOT EVIDENCE FOR. `zhao_terrain_cmd` verifies the list
  // in a first pass and emits records only in a second, so a list whose CRC is
  // wrong produces NO records and this bench would prove nothing about the
  // spine. The harness therefore has to hand it a well-formed packet, and it
  // folds the bytes with the SAME `zhao_crc32c_fold` the block uses.
  //
  // That makes this bench NO EVIDENCE WHATEVER about the CRC itself -- both
  // sides would agree on an identical mistake. `tests/terrain/tb_terrain_cmd.sv`
  // is where the CRC is tested. What this bench shows is the thing it can
  // honestly show: a VALID list produces records, and those records reach
  // TERRAIN.SEQ.
  logic [31:0] fold_c_i;
  logic [63:0] fold_d_i;
  logic [3:0]  fold_n_i;
  logic [31:0] fold_c_o;
  zhao_crc32c_fold u_bench_fold (.c_i(fold_c_i), .d_i(fold_d_i), .n_i(fold_n_i), .c_o(fold_c_o));

  // ==========================================================================
  // CLOCKS. The frozen ratios (plan R1): vid = gpu/2, audio = gpu/4, with
  // COINCIDENT POSEDGES. Written as three explicit waveforms rather than as a
  // divider, because a divider built out of always_ff puts each derived edge a
  // delta after the gpu edge and the shell's CDC law is written against
  // coincident ones.
  //
  //   gpu   posedge at 4, 12, 20, 28, 36, ...   (period 8)
  //   vid   posedge at 4, 20, 36, ...           (every 2nd gpu posedge)
  //   audio posedge at 4, 36, ...               (every 4th gpu posedge)
  // ==========================================================================
  initial begin
    gpu_clk = 1'b0;
    vid_clk = 1'b0;
    audio_clk = 1'b0;
  end
  always begin #4ns gpu_clk   = 1'b1;  #4ns gpu_clk   = 1'b0; end
  always begin #4ns vid_clk   = 1'b1;  #8ns vid_clk   = 1'b0; #4ns; end
  always begin #4ns audio_clk = 1'b1; #16ns audio_clk = 1'b0; #12ns; end

  // ==========================================================================
  // OBSERVATION. Every one of these only moves when a real beat crosses a real
  // wire; none of them is written by the bench.
  // ==========================================================================
  int unsigned cycles_q;
  int unsigned ticks_seen_q;
  int unsigned part_records_written_q;
  logic        render_frame_open_q;
  bit          part_tick_seen_q;
  bit          reset_released_q;

  // ---- the spawn-on-collision acceptance, watched on the WRITE STREAM -------
  // Ruling I4 (`reports/RULING-I4-COLLISION-SPAWN-20260919.md`) says a
  // collision-spawned child is placed POST-CONTACT. On a flat heightfield with
  // a STICK response the post-contact position is exact -- h + CLEAR_EPS,
  // vertically, with no rounding at all -- so the expected value is a hand
  // computed literal and not a re-implementation of the collider.
  //
  // The child is recognised by kPartBornThisTick, which PART.SPAWN writes and
  // nothing else in this core does.
  localparam int unsigned PART_KEY_BORN_BIT = 118;  // OFF_FLG 116 + kPartBornThisTick
  localparam int signed   PART_CONTACT_Y    = PART_TER_H + PART_CLEAR_EPS;

  int unsigned part_children_seen_q;
  int unsigned part_children_at_contact_q;

  always @(posedge gpu_clk) begin
    cycles_q <= cycles_q + 1;
    if (reset_released_q) begin
      if (gpu_tick_o)                             ticks_seen_q     <= ticks_seen_q + 1;
      if (part_tick_busy_o)                       part_tick_seen_q <= 1'b1;
      // WATCHED IN DDR, since entry I1 closed: a record counts as written when
      // its second beat lands in the played buffer, not when it leaves
      // PART.STATE -- so this is also the evidence that the store delivered it.
      if (part_ddr_wr_rec_valid_q) begin
        part_records_written_q <= part_records_written_q + 1;
        if (part_ddr_wr_rec_q[PART_KEY_BORN_BIT]) begin
          part_children_seen_q <= part_children_seen_q + 1;
          // pos.y is the second 18-bit field of the ratified record.
          if ($signed(part_ddr_wr_rec_q[35:18]) == PART_POS_W'(PART_CONTACT_Y))
            part_children_at_contact_q <= part_children_at_contact_q + 1;
        end
      end
    end
  end

  // ==========================================================================
  // PART.TABLE'S LOAD -- THE BENCH IS THE HOST, AND SAYS SO.
  //
  // Core entries I2 and I3 closed on 2026-09-19: the console instantiates
  // `zhao_part_table` and answers its own descriptor reads. Nothing in the core
  // FILLS that table -- CMD.SCHEDULER has no path to it (entry I33) -- so the
  // load is a boundary and THIS BENCH IS STANDING IN FOR THE HOST. That is a
  // declared limit, not a hidden one: what is proven below is that a descriptor
  // written through `part_tbl_ld_*` comes back out of three different consumers,
  // and NOT that any real agent writes it.
  //
  // It handshakes on `ld_ready_o` rather than assuming it. The port is constant
  // high by design and its own file says so -- but a bench that ignores a ready
  // it was given is a bench that would not notice the day it stopped being
  // constant, and this one is the only host the table has.
  //
  // THE WORD MAP IS A COPY, and copies go stale in the flattering direction, so
  // the authority is named: `fpga/rtl/particles/zhao_part_table.sv`, section
  // "LOAD WORD MAP". What protects this copy is not diligence -- it is that a
  // mispacked word puts the wrong value in every field, and the three
  // behavioural checks below (a STICK contact, one child per collision, and an
  // exact colour byte at the boundary) all fail loudly when it does. A packing
  // bug here cannot pass quietly.
  // ==========================================================================
  localparam logic [1:0] TBL_SEL_UPD = 2'd0;
  localparam logic [1:0] TBL_SEL_COL = 2'd1;
  localparam logic [1:0] TBL_SEL_SPW = 2'd2;
  localparam logic [1:0] TBL_SEL_CRV = 2'd3;

  // update slice: recipe 4 | lifetime 10 | age_mark 10 | drag 8 | grav 11 |
  //               strength 11 | cx 18 | cy 18 | cz 18 | p0 11 | p1 11 | p2 11
  localparam int unsigned TBL_U_OFF_RCP = 0;
  localparam int unsigned TBL_U_OFF_LIF = TBL_U_OFF_RCP + 4;
  localparam int unsigned TBL_U_OFF_MRK = TBL_U_OFF_LIF + PART_AGE_W;
  localparam int unsigned TBL_U_OFF_DRG = TBL_U_OFF_MRK + PART_AGE_W;
  // collide slice: response 3 | restitution 16 | friction 16 | damping 16
  localparam int unsigned TBL_C_OFF_RSP = 0;
  // spawn rule: child species 7 | count 5 | known 1
  localparam int unsigned TBL_S_OFF_CHD = 0;
  localparam int unsigned TBL_S_OFF_CNT = TBL_S_OFF_CHD + 7;
  localparam int unsigned TBL_S_OFF_KNW = TBL_S_OFF_CNT + 5;
  // curve entry: size 6 | colour 8
  localparam int unsigned TBL_V_OFF_SIZ = 0;
  localparam int unsigned TBL_V_OFF_CLR = TBL_V_OFF_SIZ + 6;

  // R_COLOUR, the tenth id of owner ruling 2026-08-31's CLOSED vocabulary. It
  // is chosen because it is the one recipe whose visible effect is a TABLE READ
  // and not arithmetic: its multiplier operand stays at zero, so the particle's
  // motion is identical to the all-zero descriptor this bench used to drive,
  // and the ONLY difference at the boundary is that `part_colour_o` now carries
  // the curve's colour byte. That makes the I3 check a read-back rather than an
  // inference.
  localparam logic [3:0] TBL_RECIPE_COLOUR = 4'd10;
  localparam logic [7:0] TBL_CURVE_COLOUR  = 8'hA5;
  localparam logic [5:0] TBL_CURVE_SIZE    = 6'h2A;

  // ONE ENTRY OF THE SPECIES_TABLE PAGE, in zref::species_page's frozen
  // layout. The bench WRITES a page now instead of driving a load port, which
  // is the whole of R42: the descriptors are data the owner authors and the
  // console reads, not wires a harness holds.
  function automatic logic [255:0] spt_entry(input logic [1:0] sel,
                                             input logic [6:0] idx,
                                             input logic [1:0] ev,
                                             input logic [PART_TBL_LD_W-1:0] data);
    logic [255:0] e;
    begin
      e = '0;
      e[1:0]   = sel;
      e[3:2]   = ev;
      e[14:8]  = idx;
      e[32 +: PART_TBL_LD_W] = data;
      spt_entry = e;
    end
  endfunction

  // The 1-BASED SLOT of the DebugTraceArm record in the packet built below.
  // Every record at or before it is offered to DEBUG.TRACE before CMD.EXEC's
  // arm lands, so the ring stores `records - TRACE_SKIP_C`. It is also the
  // 0-based index the FIRST STORED event must carry in its `command_seq` word,
  // which is why one constant serves both checks.
  localparam int unsigned TRACE_SKIP_C = 2;

  // ---- HOST.REGWIN, the bench AS THE HPS (ruling R51) ----------------------
  // The whole lightweight-bridge protocol: offer while `h_ready_o` is high,
  // then wait for the one-cycle `h_rvalid_o`. It NEVER HANGS by design -- every
  // refusal is answered with err -- so a guard count here is a real bug rather
  // than a benign timeout, and this task fatals rather than returning silence.
  logic [31:0] hr_data;
  logic [31:0] hr_hsnap;
  logic        hr_err;

  task automatic host_access(input logic wr,
                             input logic [15:0] addr,
                             input logic [31:0] wdata,
                             output logic [31:0] rdata,
                             output logic err);
    int unsigned g;
    g = 0;
    while (!hostreg_ready_o && (g < 1000)) begin
      @(posedge gpu_clk);
      g++;
    end
    if (g >= 1000)
      $fatal(1, "SMOKE: HOST.REGWIN never raised h_ready_o -- the aperture is stuck, not busy");
    hostreg_addr_i  = addr;
    hostreg_wdata_i = wdata;
    hostreg_write_i = wr;
    hostreg_valid_i = 1'b1;
    @(posedge gpu_clk);
    hostreg_valid_i = 1'b0;
    hostreg_write_i = 1'b0;
    g = 0;
    while (!hostreg_rvalid_o && (g < 1000)) begin
      @(posedge gpu_clk);
      g++;
    end
    if (g >= 1000)
      $fatal(1, "SMOKE: HOST.REGWIN gave no response to %s 0x%04x in 1000 cycles -- a host bus that stops answering is a console that locks up",
             wr ? "write" : "read", addr);
    rdata = hostreg_rdata_o;
    err   = hostreg_err_o;
    @(posedge gpu_clk);
  endtask

  task automatic host_read(input logic [15:0] addr,
                           output logic [31:0] rdata,
                           output logic err);
    host_access(1'b0, addr, 32'd0, rdata, err);
  endtask

  task automatic host_write(input logic [15:0] addr,
                            input logic [31:0] wdata,
                            output logic err);
    logic [31:0] d;
    host_access(1'b1, addr, wdata, d, err);
  endtask

  // (`tbl_load` USED TO BE HERE and is deliberately gone. It drove the six
  //  `part_tbl_ld_*` ports directly; ruling R42 retired them, and the bench
  //  now WRITES a species_table page with `spt_entry` above. The hostdbg
  //  branch predated that, so merging it back in restored a task that calls
  //  ports the module no longer has -- and nothing called it. Composing an
  //  older version, in a bench rather than in RTL.)

  // The colour byte, captured where it leaves the module, ONE SAMPLE PER
  // PARTICLE.
  //
  // `part_colour_en_o` and `part_colour_o` are REGISTERED LEVELS, not pulses:
  // PART.UPDATE loads them on a retire and they stand until the next one. The
  // first version of this counter sampled the level every cycle and reported
  // 253,517 "beats" for six particles -- a number that is not wrong so much as
  // about the wrong thing, and it would have made a single correct particle
  // look like a quarter of a million confirmations. So the sample is qualified
  // on `part_updated_o` MOVING, which is exactly one event per updated
  // particle, and the count that comes out is comparable with the six records
  // this bench offers.
  int unsigned part_colour_beats_q;
  int unsigned part_colour_wrong_q;
  logic [7:0]  part_colour_last_q;
  logic [31:0] part_updated_prev_q;

  always @(posedge gpu_clk) begin
    if (!rst_n) begin
      part_colour_beats_q <= 0;
      part_colour_wrong_q <= 0;
      part_colour_last_q  <= '0;
      part_updated_prev_q <= '0;
    end else begin
      part_updated_prev_q <= part_updated_o;
      if (part_updated_o !== part_updated_prev_q) begin
        if (part_colour_en_o) begin
          part_colour_beats_q <= part_colour_beats_q + 1;
          part_colour_last_q  <= part_colour_o;
          if (part_colour_o !== TBL_CURVE_COLOUR)
            part_colour_wrong_q <= part_colour_wrong_q + 1;
        end else begin
          // A retire with the enable LOW is a particle whose recipe did not
          // come back as R_COLOUR. Counted as wrong rather than skipped: a
          // silent skip is how "it was right every time" becomes "it was right
          // the one time it happened to be looked at".
          part_colour_wrong_q <= part_colour_wrong_q + 1;
        end
      end
    end
  end

  // --------------------------------------------------------------------------
  // THE GENERATION STORE (entry I1 CLOSED 2026-09-19): THE BENCH IS THE HPS.
  //
  // The first generation is placed in buffer 0 of the played DDR BEFORE the
  // machine runs -- N_PART_RECORDS particle128 records, distinct so a swap
  // would be visible, and DATA rather than a workload: every field the blocks
  // act on comes from the species descriptor, which this bench holds at its
  // benign values. Then the HPS seeds {buffer 0, N_PART_RECORDS} once, and
  // from there the core owns both buffers: it reads this generation back out
  // through the bridge socket and writes the next one into buffer 1.
  // --------------------------------------------------------------------------
  initial begin : part_ddr_image
    for (int w = 0; w < PART_HPS_WORDS; w++) part_mem[w] = 64'hDEAD_BEEF_DEAD_BEEF;
    for (int r = 0; r < N_PART_RECORDS; r++) begin
      // position X = the ordinal, so the records are distinguishable;
      // everything else zero, which is a legal particle128.
      part_mem[2 * r]     = 64'(18'(r + 1));
      part_mem[2 * r + 1] = 64'd0;
    end
  end

  // (The bench used to seed the store itself here, from `reset_released_q`.
  //  It does not any more: the seed is SetPopulation.active_count, and it
  //  reaches the store through CMD.DECODER's verdict, CMD.EXEC's commit and
  //  u_part_pop. That is the whole point of R41 -- the value now traverses
  //  a real chain from a real producer instead of arriving on a pin.)
  // --------------------------------------------------------------------------
  // THE GEOMETRY VERTEX STREAM.
  //
  // MOVED ONE BLOCK UPSTREAM, 2026-09-19. This used to drive `geom_skin_v_*`
  // directly, because GEOM.VDECODE was not composed and GEOM.SKIN's vertex port
  // was at the module edge. It is now composed, so the harness presents what a
  // harness is allowed to present -- BYTES -- and the decoder makes the vertex.
  // The path under test is therefore VDECODE -> SKIN -> GROUP_SEQ -> the shared
  // projector, and `geom_vd_vertices_o` and `geom_skin_vertices_transformed_o`
  // must BOTH move or the new block is passing nothing on.
  //
  // THE RECORD IS THE REAL FORMAT-0 LAYOUT (zhao_geom_vdecode.sv's header):
  //   off  0  12  position s32 x3, fx16
  //   off 12   3  normal s8 x3
  //   off 15   1  w0, 1/64 quanta (64 == rigid)
  //   off 16   4  UV, 2 x s16
  //   off 20   2  bone0
  //   off 22   2  bone1
  //   off 24   8  reserved, MUST BE ZERO -- a nonzero byte here is a REFUSAL,
  //               which is why this is packed field by field rather than
  //               smeared with a pattern.
  // Little-endian, byte 0 at bit 0, so the concatenation runs high byte first.
  // --------------------------------------------------------------------------
  function automatic logic [255:0] vdec_record(input int unsigned n);
    logic signed [31:0] px, py, pz;
    begin
      // The fixture's positions (smoke_geom_fixture.svh), fx16 world = model:
      // no pose is decoded (I29), so the identity bind pose applies.
      px = SGF_VX[n];
      py = SGF_VY[n];
      pz = SGF_VZ[n];
      vdec_record = {64'd0,          // off 24..31 reserved, MUST be zero
                     16'd0,          // off 22 bone1 (== bone0 -> rigid)
                     16'd0,          // off 20 bone0
                     SGF_VV[n],      // off 18 v (the fixture's own UV: GEOM.VATTR's
                     SGF_VU[n],      // off 16 u  u/v_over_w carry real gradients)
                     8'd64,          // off 15 w0 == 64 -> rigid
                     8'sd0,          // off 14 nz
                     8'sd0,          // off 13 ny
                     8'sd127,        // off 12 nx
                     pz, py, px};    // off 0..11 position
    end
  endfunction

  // ==========================================================================
  // THE SAME RECORDS, THROUGH THE MACHINE INSTEAD OF AROUND IT.
  //
  // `vdec_record` above is unchanged and is still the fixture. What changed is
  // WHERE IT IS PUT: it used to be driven onto `geom_vd_v_bytes_i`, a port that
  // no longer exists, and it is now written into SDRAM, fetched by
  // GEOM.ASSETFETCH through the real MEM.GUARD, MEM.VRAM.ARBITER and
  // `zhao_sdram_ctrl`, and handed to GEOM.VDECODE inside the core.
  //
  // THAT IS THE WHOLE POINT AND IT IS WHY THE BYTES DID NOT CHANGE. If the
  // decoded count still comes out at N_GEOM_VERTS with the identical records,
  // the difference between the old reading and the new one is exactly the
  // memory path -- a descriptor read, a cull, an index run and a vertex run --
  // and nothing else. A new fixture would have made the comparison useless.
  //
  // THE MAP, all pool-relative to ZHAO_RENDER_ASSET_BASE = 0x06A0_0000:
  //     +0x000   64 B  the meshlet descriptor        (64-byte aligned, ruled)
  //     +0x080   64 B  the index run, 3 u8           ( 8-byte aligned, ruled)
  //     +0x100  128 B  four 32-byte vertex records   (32-byte aligned, ruled)
  // The three do not overlap, and the descriptor names the other two in its
  // own `vertex_offset`/`index_offset` fields rather than the bench naming them
  // twice: a fixture that agrees with itself by construction cannot drift.
  // ==========================================================================
  localparam int unsigned GEOM_POOL_BASE = 32'h06A0_0000;
  // PAGE-RELATIVE since 2026-09-20 (owner ruling R29): these are offsets into
  // the MESH_STREAM page, which now begins with its frozen 64-byte header.
  // The header is at 0, the descriptor table at 64, and the descriptor's own
  // vertex/index offsets are page-relative too -- GEOM.MESHFETCH adds the
  // page's pool-relative base once, so what reaches GEOM.ASSETFETCH is still
  // pool-relative and that block is unchanged.
  // R57: the descriptor table is SGF_N_MESHLETS x 64 B, then one 64-byte index
  // run per meshlet (each holds 3 x ntri <= 21 bytes and must be 8-byte
  // aligned), then the ONE vertex run every descriptor names.
  localparam int unsigned GEOM_HDR_OFF   = 32'h0000_0000;
  localparam int unsigned GEOM_DESC_OFF  = 32'h0000_0040;
  localparam int unsigned GEOM_IX_OFF    = 32'h0000_0100;
  localparam int unsigned GEOM_IX_STRIDE = 32'h0000_0040;
  localparam int unsigned GEOM_VX_OFF    = 32'h0000_01C0;
  // The bound, in the meshlet's own space, and the identity instance transform
  // that carries it to world. It sits at the clip origin under the identity
  // camera written below, so GEOM.CULL must call it VISIBLE -- and its radius
  // is NONZERO because GEOM.MESHFETCH's seventh refusal row rejects a zero
  // radius on a meshlet that claims triangles.
  localparam logic [31:0] GEOM_BOUND_R   = 32'h0000_4000;   // 0.25 in fx16

  logic        geom_poke_en;
  logic [25:0] geom_poke_waddr;
  logic [15:0] geom_poke_data;
  logic        geom_peek_en;
  logic [25:0] geom_peek_waddr;
  logic [15:0] geom_peek_data;
  logic [5:0]  geom_model_err;
  logic        geom_model_error;
  bit          geom_fixture_ready_q;
  bit          geom_camera_ready_q;
  bit          geom_loom_sent_q;

  // ONE 16-BIT WORD PER CLOCK, at `byte_addr >> 1`, low byte of the word first
  // -- `zhao_sdram_ctrl` computes `waddr = req.addr[26:1]` and the shell's read
  // packer rebuilds four words per 64-bit beat low group first, so this is the
  // same little-endian order in both directions.
  task automatic geom_poke_w(input int unsigned byte_addr, input logic [15:0] d);
    begin
      geom_poke_en    = 1'b1;
      geom_poke_waddr = 26'(byte_addr >> 1);
      geom_poke_data  = d;
      @(posedge gpu_clk);
      geom_poke_en    = 1'b0;
    end
  endtask

  // CRC32C (reflected Castagnoli, all-ones seed, final complement) over the
  // first 60 bytes of an eight-word descriptor, low byte of word 0 first. A
  // BENCH-SIDE bitwise loop, deliberately NOT the RTL fold, so the fixture and
  // `zhao_geom_desc_crc` are two implementations that must agree.
  function automatic logic [31:0] fixture_desc_crc(input logic [63:0] w [8]);
    logic [31:0] c;
    logic [7:0]  b;
    c = 32'hFFFF_FFFF;
    for (int unsigned i = 0; i < 60; i = i + 1) begin
      b = w[i / 8][8 * (i % 8) +: 8];
      c = c ^ {24'd0, b};
      for (int unsigned k = 0; k < 8; k = k + 1)
        c = c[0] ? ((c >> 1) ^ 32'h82F6_3B78) : (c >> 1);
    end
    return ~c;
  endfunction

  task automatic geom_poke_q(input int unsigned byte_addr, input logic [63:0] d);
    begin
      geom_poke_w(byte_addr + 0, d[15:0]);
      geom_poke_w(byte_addr + 2, d[31:16]);
      geom_poke_w(byte_addr + 4, d[47:32]);
      geom_poke_w(byte_addr + 6, d[63:48]);
    end
  endtask

  // The camera, written into the ONE matrix bank the projector and GEOM.CULL
  // share. It is IDENTITY, and it is written rather than left at zero because a
  // cull verdict that happens to come out VISIBLE under an unwritten bank is an
  // accident this bench would be resting on. Under identity the clip point is
  // the world point and w is 1.0, so a bound at the origin is inside every
  // plane by construction and the check below is about the WIRE.
  // BOTH VIEWS, from the fixture: the w = z camera (SGF_MAT) and each view's
  // viewport rectangle at cfg 16/17 -- the host port is entry I14's boundary,
  // so this bench stands in for the host exactly as it did for view 0 alone.
  // The profile (cfg 18) is left at its reset WORLD_LONG, which is what the
  // reference-derived depths assume.

  // Every lit vertex, checked as it leaves, PER CHANNEL, against the reference
  // applied to the SetEnvironment record this bench's packet carries:
  // zref::light_env::bank_of(record) -> zref::creature::lambert_from_world_normal
  // -> the stream's gain * ndl + ambient (SGF_EXP_LIT_R/G/B, fixture generator).
  // ---- R31: THE GEOMETRY RATE, MEASURED IN THE COMPOSED MACHINE -----------
  // First and last clock each stage's counter moved, and how many times. The
  // steady interval is (last - first) / (moves - 1): it excludes the pipeline
  // fill, which is latency, not rate. Printed on the `SMOKE: rate` line; the
  // per-block benches (skin_norm_rtl_directed, geom_replay_directed) carry the
  // long-run figures, this is the same quantity with every real neighbour live.
  longint unsigned rt_clk_q;
  longint unsigned rt_skin_first_q, rt_skin_last_q, rt_land_first_q, rt_land_last_q;
  longint unsigned rt_tri_first_q, rt_tri_last_q, rt_vd_first_q, rt_rel_q;
  longint unsigned rt_rel_first_q, rt_afrel_first_q, rt_afrel_last_q, rt_vd_m2_q;
  longint unsigned rt_skin_m1_last_q;
  int unsigned     rt_rel_n_q, rt_afrel_n_q;
  int unsigned     rt_skin_n_q, rt_land_n_q, rt_tri_n_q;
  logic [31:0]     rt_skin_prev_q, rt_land_prev_q, rt_tri_prev_q, rt_vd_prev_q, rt_rel_prev_q;
  always @(posedge gpu_clk) begin
    if (!rst_n) begin
      rt_clk_q <= 0;
      rt_skin_first_q <= 0; rt_skin_last_q <= 0; rt_skin_n_q <= 0; rt_skin_prev_q <= 0;
      rt_land_first_q <= 0; rt_land_last_q <= 0; rt_land_n_q <= 0; rt_land_prev_q <= 0;
      rt_tri_first_q  <= 0; rt_tri_last_q  <= 0; rt_tri_n_q  <= 0; rt_tri_prev_q  <= 0;
      rt_vd_first_q   <= 0; rt_vd_prev_q   <= 0; rt_rel_q    <= 0; rt_rel_prev_q  <= 0;
      rt_rel_first_q  <= 0; rt_rel_n_q     <= 0; rt_vd_m2_q  <= 0; rt_skin_m1_last_q <= 0;
      rt_afrel_first_q <= 0; rt_afrel_last_q <= 0; rt_afrel_n_q <= 0;
    end else begin
      rt_clk_q <= rt_clk_q + 1;
      if (geom_vd_vertices_o != rt_vd_prev_q) begin
        if (rt_vd_prev_q == 0) rt_vd_first_q <= rt_clk_q;
        rt_vd_prev_q <= geom_vd_vertices_o;
      end
      if (geom_skin_vertices_transformed_o != rt_skin_prev_q) begin
        if (rt_skin_n_q == 0) rt_skin_first_q <= rt_clk_q;
        rt_skin_last_q <= rt_clk_q;
        rt_skin_n_q    <= rt_skin_n_q + 1;
        rt_skin_prev_q <= geom_skin_vertices_transformed_o;
      end
      if (geom_landings_o != rt_land_prev_q) begin
        if (rt_land_n_q == 0) rt_land_first_q <= rt_clk_q;
        rt_land_last_q <= rt_clk_q;
        rt_land_n_q    <= rt_land_n_q + 1;
        rt_land_prev_q <= geom_landings_o;
      end
      if (geom_rp_triangles_out_o != rt_tri_prev_q) begin
        if (rt_tri_n_q == 0) rt_tri_first_q <= rt_clk_q;
        rt_tri_last_q <= rt_clk_q;
        rt_tri_n_q    <= rt_tri_n_q + 1;
        rt_tri_prev_q <= geom_rp_triangles_out_o;
      end
      if (geom_rp_meshlets_o != rt_rel_prev_q) begin
        if (rt_rel_n_q == 0) rt_rel_first_q <= rt_clk_q;
        rt_rel_q      <= rt_clk_q;
        rt_rel_n_q    <= rt_rel_n_q + 1;
        rt_rel_prev_q <= geom_rp_meshlets_o;
      end
      // R57: the ASSET BUFFER release, which is a different moment from the
      // meshlet retiring and is the one that lets the NEXT meshlet's vertex
      // phase start. Tapped on the core's own net rather than inferred from a
      // counter, because the whole claim is about WHEN it happens.
      if (`PC_CORE.rp_af_release) begin
        if (rt_afrel_n_q == 0) rt_afrel_first_q <= rt_clk_q;
        rt_afrel_last_q <= rt_clk_q;
        rt_afrel_n_q    <= rt_afrel_n_q + 1;
      end
      // The clock the FIRST vertex record of meshlet 2 is decoded: if the loop
      // overlaps, this is BEFORE meshlet 1 has retired, and the two timestamps
      // printed side by side are the evidence.
      //
      // N_GEOM_VERTS + 1, NOT N_GEOM_VERTS. The counter reaching N_GEOM_VERTS is
      // meshlet 1's LAST record, not meshlet 2's first, and the off-by-one made
      // the overlap assertion below pass on a serial machine -- a detector
      // reading the flattering answer, which is the shape to check hardest.
      if ((geom_vd_vertices_o == N_GEOM_VERTS + 1) && (rt_vd_m2_q == 0))
        rt_vd_m2_q <= rt_clk_q;
      // The per-vertex rate INSIDE one meshlet, which is the quantity R57 says
      // is the next lever. The frame-wide `SMOKE: rate` interval now spans the
      // gaps BETWEEN meshlets and is a different number.
      if ((geom_skin_vertices_transformed_o == N_GEOM_VERTS) && (rt_skin_m1_last_q == 0))
        rt_skin_m1_last_q <= rt_clk_q;
    end
  end

  // ---- R57: WHERE GEOM.ASSETFETCH'S FETCH CLOCKS GO -----------------------
  // The loop measurement says the fetch is the largest term; this says what the
  // fetch is made of, because "84 clocks per 64-byte line" is a number somebody
  // will size an outstanding-request engine against and it must not be quoted
  // without its parts. Three disjoint counts over every line ASSETFETCH asks
  // for, from the clock it raises `valid` to the clock its last beat lands:
  //
  //   offered  the request is up and MEM.GUARD has not accepted it -- this is
  //            CONTENTION (somebody else holds the forwarding stage), not
  //            latency of ours;
  //   waiting  accepted, and no beat has arrived yet -- the guard's verdict
  //            cycle plus the arbiter and the SDRAM's own turnaround. THIS is
  //            what a second outstanding read would hide;
  //   beating  from the first beat to the last -- the DATA, eight beats, which
  //            no amount of pipelining removes.
  int unsigned af_lines_q, af_offered_q, af_waiting_q, af_beating_q;
  bit          af_inflight_q, af_seen_beat_q;
  always @(posedge gpu_clk) begin
    if (!rst_n) begin
      af_lines_q <= 0; af_offered_q <= 0; af_waiting_q <= 0; af_beating_q <= 0;
      af_inflight_q <= 1'b0; af_seen_beat_q <= 1'b0;
    end else begin
      if (`PC_CORE.af_guard_req.valid && !af_inflight_q) begin
        af_inflight_q  <= 1'b1;
        af_seen_beat_q <= 1'b0;
        af_lines_q     <= af_lines_q + 1;
      end
      if (af_inflight_q || `PC_CORE.af_guard_req.valid) begin
        if (`PC_CORE.af_guard_req.valid && !`PC_CORE.af_guard_rsp.ready)
          af_offered_q <= af_offered_q + 1;
        else if (!af_seen_beat_q && !`PC_CORE.af_beat_valid)
          af_waiting_q <= af_waiting_q + 1;
        else
          af_beating_q <= af_beating_q + 1;
      end
      if (`PC_CORE.af_beat_valid) af_seen_beat_q <= 1'b1;
      if (`PC_CORE.af_beat_valid && `PC_CORE.af_beat_last) af_inflight_q <= 1'b0;
    end
  end

  int unsigned geom_lit_seen_q, geom_lit_bad_q;
  always @(posedge gpu_clk) begin
    if (!rst_n) begin
      geom_lit_seen_q <= 0;
      geom_lit_bad_q  <= 0;
    end else if (geom_light_valid_o && geom_light_ready_o) begin
      geom_lit_seen_q <= geom_lit_seen_q + 1;
      if ((geom_light_r_o != SGF_EXP_LIT_R) || (geom_light_g_o != SGF_EXP_LIT_G) ||
          (geom_light_b_o != SGF_EXP_LIT_B) || geom_light_degenerate_vtx_o)
        geom_lit_bad_q <= geom_lit_bad_q + 1;
    end
  end

  task automatic geom_write_camera();
    int unsigned i;
    int unsigned v;
    begin
      for (v = 0; v < 2; v = v + 1) begin
        for (i = 0; i < 18; i = i + 1) begin
          proj_cfg_we_i   = 1'b1;
          proj_cfg_view_i = v[0];
          proj_cfg_addr_i = 5'(i);
          if (i < 16)       proj_cfg_data_i = SGF_MAT[i];
          else if (i == 16) proj_cfg_data_i = (v == 0) ? SGF_VP0_ORG : SGF_VP1_ORG;
          else              proj_cfg_data_i = (v == 0) ? SGF_VP0_EXT : SGF_VP1_EXT;
          @(posedge gpu_clk);
        end
      end
      proj_cfg_we_i = 1'b0;
    end
  endtask

  initial begin
    geom_poke_en         = 1'b0;
    geom_poke_waddr      = '0;
    geom_poke_data       = '0;
    geom_peek_en         = 1'b0;
    geom_peek_waddr      = '0;
    geom_fixture_ready_q = 1'b0;
    @(posedge gpu_clk);

    // THE FIXTURE'S BYTES ARE NO LONGER POKED INTO VRAM (2026-09-20, R29).
    // The descriptor, the index run and the vertex records are now the body of
    // a MESH_STREAM page that MEM.UPLOAD copies from the HPS staging arena and
    // publishes -- built in the packet block below, beside the command that
    // names it, so the page and its CRC cannot drift apart. What stays here is
    // the frame sentinel, which is about the POST pass and not about geometry.
    // THE FRAME IS PRE-FILLED WITH A SENTINEL, and without it the post checks
    // below would compare zeros. MEASURED 2026-09-19: the raster's 2,560 pixels
    // are ALL 0x0000 -- the material is still unbound, so the surface samples
    // nothing (the NOTE the verdict prints) -- and the model's memory starts at
    // zero, so an all-zero frame went out through the post source and came back
    // "byte-identical" having proved nothing. With a nonzero pattern in every
    // word of the view: the raster's pixels (black) REPLACE it where it draws,
    // which is itself evidence the raster wrote; the identity post pass must
    // bring every word back unchanged; and the echo must carry every one of
    // them to the capture -- which starts at zero, so an echo that wrote
    // nothing cannot match.
    for (int unsigned w = 0; w < 384 * 240; w++)    // POST_TB_WORDS, declared below
      geom_poke_w(2 * w, 16'((w * 40503) ^ 32'h5A3C) | 16'h0001);

    geom_fixture_ready_q = 1'b1;
  end

  // ==========================================================================
  // POST.COMPOSITE AND POST.ECHO: THE VALUE TRAVERSES (I15, I16, R7)
  // ==========================================================================
  // The back buffer is snapshotted at the instant the post source hands its
  // FIRST pixel to the compositor. By then the raster has drained and every
  // raster word has RETIRED (the lease's start condition), and no write-back
  // can have happened yet (the compositor writes line y only after reading
  // line y+4). So the snapshot is exactly the raster's frame.
  //
  // With every effect input at zero the compositor is the IDENTITY (no
  // displacement, no glow, no atmosphere, bloom gain 0, ungraded, flash 0,
  // no ink, no HUD), so after the pass:
  //   * the framebuffer must equal the snapshot word for word -- the frame
  //     went out through the source reader and came back through RASTER.
  //     FBWRITE without a word moving or changing;
  //   * the capture must equal the snapshot word for word -- the echo tap
  //     carried the same pixels to POST.ECHO's window.
  // A reader that read the wrong rows, a writer that wrote them back
  // displaced, or an echo that dropped or misplaced a chunk each breaks one
  // of the two equalities.
  localparam int unsigned POST_TB_W = 384;          // Z60, the smoke's mode
  localparam int unsigned POST_TB_H = 240;
  localparam int unsigned POST_TB_WORDS = POST_TB_W * POST_TB_H;
  localparam int unsigned POST_ECHO_WBASE = 32'h05C0_0000 >> 1;
  logic [15:0] post_fb_snap [0:POST_TB_WORDS-1];
  bit          post_snap_taken_q;
  // The pass's duration: every cycle the post lease is busy (armed at the
  // raster's frame_end, done when the last written-back word has retired).
  // POST.COMPOSITE.md prices the stream at ~103,680 work items per Z60 frame and
  // warns that a number near 461,000 means it has quietly become passes again.
  int unsigned post_busy_cycles_q;
  // THE POST CENSUS (owner ruling R38, post pass 2): where the lease's busy
  // clocks go, read hierarchically -- bench-only, nothing here is a port. The
  // SDRAM controller's command state says whether the memory is working; the
  // arbiter's grant stream says for whom; the ENGINE0 share's state says how
  // long a read holds the one logical request slot; and the two FBWRITEs and
  // the reader say who is waiting on whom.
  // The shell's path: the mutant build's DUT is a wrapper around the core.
`ifdef ZHAO_MUT_SLOT_OVERFLOW
  `define PC_SHELL dut.u_dut.u_shell
`else
  `define PC_SHELL dut.u_shell
`endif
  int unsigned pc_ctrl_busy_q, pc_bursts_rd_q, pc_bursts_wr_q, pc_bursts_other_q;
  int unsigned pc_share_fill_q, pc_fbw_stall_q, pc_src_starve_q, pc_src_block_q;
  int unsigned pc_conflicts0_q, pc_refresh0_q;
  always @(posedge gpu_clk) begin
    if (!rst_n) begin
      pc_ctrl_busy_q <= 0; pc_bursts_rd_q <= 0; pc_bursts_wr_q <= 0; pc_bursts_other_q <= 0;
      pc_share_fill_q <= 0; pc_fbw_stall_q <= 0; pc_src_starve_q <= 0; pc_src_block_q <= 0;
      pc_conflicts0_q <= 0; pc_refresh0_q <= 0;
    end else if (post_busy_o) begin
      if (pc_ctrl_busy_q == 0 && pc_bursts_rd_q == 0 && pc_bursts_wr_q == 0) begin
        pc_conflicts0_q <= bank_conflicts_o;
        pc_refresh0_q   <= `PC_SHELL.refresh_stalls_o;
      end
      if (`PC_SHELL.u_ctrl.state != 4'd7) pc_ctrl_busy_q <= pc_ctrl_busy_q + 1;
      if (`PC_SHELL.ctrl_rsp.grant) begin
        if (`PC_SHELL.ctrl_req.client != ZHAO_CLIENT_ENGINE0) pc_bursts_other_q <= pc_bursts_other_q + 1;
        else if (`PC_SHELL.ctrl_req.write) pc_bursts_wr_q <= pc_bursts_wr_q + 1;
        else pc_bursts_rd_q <= pc_bursts_rd_q + 1;
      end
      // A DEAD CENSUS FIELD, REPAIRED 2026-09-20 (mergefix). This read
      // `u_engine0_share.st_q == 3'd3`, the old A_FILL state -- and R38's own
      // rework DELETED A_FILL and narrowed `st_q` to two bits, so the comparison
      // became structurally unsatisfiable and the field printed 0 for a pass
      // that issues 11,520 ENGINE0 reads. A zero that cannot be anything else is
      // the broken-instrument shape, and it reads as "the share never holds".
      // The successor quantity is the in-flight table's occupancy.
      if (`PC_SHELL.u_post_lease.u_engine0_share.fl_n_q != '0) pc_share_fill_q <= pc_share_fill_q + 1;
      if (`PC_SHELL.fbw_px_valid && !`PC_SHELL.fbw_px_ready) pc_fbw_stall_q <= pc_fbw_stall_q + 1;
      if (!`PC_SHELL.post_src_valid_o && `PC_SHELL.post_src_ready_i) pc_src_starve_q <= pc_src_starve_q + 1;
      if (`PC_SHELL.post_src_valid_o && !`PC_SHELL.post_src_ready_i) pc_src_block_q <= pc_src_block_q + 1;
    end
  end
  always @(posedge gpu_clk) begin
    if (!rst_n) begin
      post_snap_taken_q  <= 1'b0;
      post_busy_cycles_q <= 0;
    end else begin
      if (post_busy_o) post_busy_cycles_q <= post_busy_cycles_q + 1;
      if (!post_snap_taken_q && (post_src_pixels_o != 32'd0)) begin
        for (int unsigned w = 0; w < POST_TB_WORDS; w++)
          post_fb_snap[w] = u_geom_sdram.mem[w];
        post_snap_taken_q <= 1'b1;
      end
    end
  end

  // THE BEHAVIOURAL SDRAM. Core header entry I23's refusal said this did not
  // exist; it is `sim/models/zhao_sdram_model.sv`, 219 lines, cycle-true, and
  // its poke backdoor's own comment was written for exactly this use. Nine of
  // the core's ten PHY ports carry an `_o` the model's do not, so every
  // connection is named rather than shorthanded.
  zhao_sdram_model u_geom_sdram (
    .clk       (gpu_clk),
    .phy_cs_n  (phy_cs_n_o),
    .phy_ras_n (phy_ras_n_o),
    .phy_cas_n (phy_cas_n_o),
    .phy_we_n  (phy_we_n_o),
    .phy_a     (phy_a_o),
    .phy_ba    (phy_ba_o),
    .phy_dq_o  (phy_dq_o),
    .phy_dq_oe (phy_dq_oe_o),
    .phy_dqm   (phy_dqm_o),
    .phy_dq_i  (phy_dq_i),
    .peek_en   (geom_peek_en),
    .peek_waddr(geom_peek_waddr),
    .peek_data (geom_peek_data),
    .poke_en   (geom_poke_en),
    .poke_waddr(geom_poke_waddr),
    .poke_data (geom_poke_data),
    .err_trcd            (geom_model_err[0]),
    .err_trp             (geom_model_err[1]),
    .err_trc             (geom_model_err[2]),
    .err_refresh_interval(geom_model_err[3]),
    .err_protocol        (geom_model_err[4]),
    .err_mrs             (geom_model_err[5]),
    .model_error         (geom_model_error)
  );

  // THE DRAW IS NO LONGER THIS BENCH'S (2026-09-20, owner ruling R29). It used
  // to poke a six-field job straight at GEOM.MESHFETCH, which is exactly the
  // fake stimulus entry I36 was open about: every field was a value this file
  // chose. The job now comes from a REAL DrawForm 0x0300 in the command packet
  // below, resolved by GEOM.DRAWJOB against a REAL MESH_STREAM page that
  // MEM.UPLOAD wrote and published into RENDER.ASSET_POOL.
  //
  // WHAT THIS BENCH STILL PLAYS is the ARM's side of entry I50: ONE ROOT node
  // into GEOM.LOOM, carrying the identity, because that stream's producer is
  // host software BY RULING (2026-08-31 6.4) and not a block anyone deleted.
  // It is sent once, before the frame; the palette holds the row from then on,
  // and a draw whose transform row was never written is REFUSED and counted.
  always @(posedge gpu_clk) begin
    if (!rst_n) begin
      geom_loom_valid_i <= 1'b0;
      geom_loom_sent_q  <= 1'b0;
    end else begin
      if (geom_loom_valid_i && geom_loom_ready_o) begin
        geom_loom_valid_i <= 1'b0;
        geom_loom_sent_q  <= 1'b1;
      end else if (reset_released_q && !geom_loom_sent_q) begin
        geom_loom_valid_i <= 1'b1;
      end
    end
  end

  // --------------------------------------------------------------------------
  // THE TRIANGLE FRONT DOOR IS GONE (2026-09-19, geom packet). It presented
  // sixteen copies of one hand-placed SCREEN triangle at GEOM.CLIP's input
  // because the replay customer did not exist. GEOM.REPLAY does now, so the
  // triangles GEOM.CLIP sees are the fixture meshlet's own, projected by the
  // shared projector in BOTH views and replayed out of the arena.
  //
  // THE VERTEX-ATTRIBUTE STORE THAT WAS MODELLED HERE IS GONE (2026-09-19, geom2
  // packet; core entry I46 CLOSED). It answered GEOM.REPLAY's lookups with
  // index-derived stand-in values because the store's WRITER did not exist.
  // `zhao_geom_vattr` is that writer and store, composed INSIDE the core on the
  // real producers' nets, so the bench no longer plays it: every u/v_over_w,
  // colour and depth the replay now reads was written by the machine from the
  // fixture's own records (SGF_VU/SGF_VV above) and landings.
  //
  // `-BadAttribute` RETIRED WITH IT. It made this bench-side store answer one
  // lookup a clock late so that GEOM.REPLAY's `att_skew_o` could be seen to
  // fire. There is no bench-side store to delay now; the detector is still
  // fired by stimulus in tests/geometry/geom_replay_directed.cpp (case I), and
  // this bench still asserts it zero in composition below.
  // --------------------------------------------------------------------------

  // --------------------------------------------------------------------------
  // THE PLAYED MEM.GUARD READ WINDOW, for TERRAIN.PAGESTREAM.
  //
  // THE FLAT-LATTICE MODEL THAT USED TO BE HERE IS GONE. It answered
  // TERRAIN.TESS's `lat_*` port because entry I22 had no owner composed;
  // `zhao_terrain_compcache_front` is composed in the core now and answers it
  // for real, so a harness playing that port would be a second store beside the
  // one under test.
  //
  // What the harness plays instead is one hop further out: the streamer reading
  // the page back out of TERRAIN.PAGE_POOL. Same shape as the WRITE window
  // below -- `ready` is a level with the request, the verdict PULSES one cycle
  // later -- because it is the same block's contract.
  //
  // THE IMAGE IT SERVES IS ZEROS, AND THIS BENCH CANNOT REACH IT ANYWAY. Every
  // page in this run fails its CRC (that is asserted, at `terr_res_crc_failures_o
  // == N_TERR_REC`), so no page reaches RESIDENT_CLEAN, TERRAIN.SEQ issues no
  // patch, and the compose engine sits idle with `terr_ps_idle_o` high. The
  // model exists so that the day a page does become resident this bench reports
  // a lattice rather than a hang, and it is documented as UNEXERCISED here
  // rather than quoted as evidence. The engine's own evidence is
  // `tests/terrain/tb_terrain_compose.sv` (four blocks on real page bytes) and
  // `tests/terrain/tb_terrain_place_cache.sv` (the placement seam this packet
  // added).
  // --------------------------------------------------------------------------
  // (The played read window that stood here went with entry I26: the compose
  // path's read share is requester 2 of the core's TERRAIN.BUILD socket share
  // and reads the REAL SDRAM the loader wrote.)

  // ==========================================================================
  // THE RUN.
  // ==========================================================================
  initial begin : run
    int unsigned guard;

    // Every DUT input to a defined value first. Generated from the DUT's own
    // port list, so a new input cannot arrive here undriven and X-propagate
    // into a pass.
    part_cfg_base0_i = PART_HPS_BASE0;
    part_cfg_base1_i = PART_HPS_BASE1;


    part_fld_valid_i = '0;
    part_fld_ax_i = '0;
    part_fld_ay_i = '0;
    part_fld_az_i = '0;
    // ---- PACKET P-TERRAIN: the spine's inputs, all defined before reset ----
    terr_cmd_valid_i = '0;
    terr_cmd_epoch_i = '0;
    terr_cmd_list_off_i = '0;
    terr_cmd_list_bytes_i = '0;
    terr_cmd_list_crc_i = '0;
    terr_cmd_patch_count_i = '0;
    terr_cmd_sequence_i = '0;
    terr_cmd_src_id_i = '0;
    terr_cmd_done_ready_i = 1'b1;
    terr_cfg_epoch_i = '0;
    terr_cfg_arena_base_i = '0;
    terr_cfg_arena_bytes_i = '0;
    terr_cfg_load_budget_i = '0;
    // ---- THE COMPOSE ENGINE'S EDGE (core header item 10) ------------------
    // The door is gone: TERRAIN.SEQ's issue is consumed inside the core now.
    // What this bench drives is what the engine itself cannot produce.
    //
    // THE PITCH AND THE ENVELOPE ARE NO LONGER DRIVEN HERE, and the three lines
    // that did it are deleted rather than left at zero. They used to read:
    //
    //     terr_place_pitch_log2_i = 8'sd1;   // the canonical 2.0 m
    //     terr_place_env_x0_i = '0;          // ... against a nonzero origin,
    //     terr_place_env_z0_i = '0;          //     so a deliberate refusal
    //
    // and the comment above them called that pair "entry I35 seen from the far
    // end". That entry is CLOSED: `zhao_terrain_hdrread` reads the page's own
    // 64-byte header on the compose path, so the pitch and the envelope are
    // internal and there is nothing here to define. This bench still never
    // issues a page (no page passes its CRC), so `terr_hr_headers_o` and the
    // rest of composed item 13's counters stay at zero for the same reason
    // every other compose-engine counter does, and none of them is quoted as
    // evidence of anything.
    // I34: the field lane. LOW, never a constant height -- a constant on
    // `fld_height_i` is a field program that moves every vertex of every patch
    // by the same amount, and section 3.4 would still produce a real composed
    // height. Absent and faked are different things.
    terr_pt_fld_valid_i = '0;
    terr_pt_fld_height_i = '0;
    terr_pt_fld_add_valid_i = '0;
    terr_pt_fld_add_x0_i = '0;
    terr_pt_fld_add_z0_i = '0;
    terr_pt_fld_add_x1_i = '0;
    terr_pt_fld_add_z1_i = '0;
    terr_pt_fld_add_hash_i = '0;
    terr_pt_fld_add_cmd_i = '0;
    // I32: layer D. No writer is composed, so the plane is never written and
    // every cell reads SOLID, which terrain_rules 3.3 makes the zero encoding.
    terr_cc_cs_we_i = '0;
    terr_cc_cs_ci_i = '0;
    terr_cc_cs_cj_i = '0;
    terr_cc_cs_substance_i = '0;
    // I21: the served patch's retirement. Never pulsed here, because the block
    // that would pulse it is the absent subpatch issuer.
    terr_cc_serve_release_i = '0;
    terr_dm_valid_i = '0;
    terr_dm_slot_i = '0;
    terr_dm_gen_i = '0;
    terr_dm_epoch_i = '0;
    terr_dm_bd_i = '0;
    terr_dm_f_i = '0;
    terr_dm_mips_i = '0;
    terr_chk_valid_i = '0;
    terr_chk_slot_i = '0;
    terr_chk_gen_i = '0;
    terr_chk_epoch_i = '0;
    // TERRAIN.WRITEBACK IS COMPOSED (entry I28 closed) and this bench CANNOT
    // REACH IT: a writeback job needs a dirty-F eviction, and `terr_dm_f_i` is
    // never raised here -- nor could a page become resident to be dirtied,
    // since every page this bench loads fails the header identity check. So
    // the played SW.STREAM posts NO grants and sends NO ACKs, and the end of
    // the run asserts that no job ever reached the doorbell. The traversal is
    // tests/terrain/world_composed_directed.cpp's, which composes the same
    // sequencer -> doorbell -> writeback -> directory chain.
    terr_cfg_journal_base_i  = 32'h3000_0000;
    terr_cfg_journal_bytes_i = 32'd16 * 32'd8192;
    terr_jdb_post_valid_i  = 1'b0;
    terr_jdb_post_slot_i   = '0;
    terr_jdb_post_ticket_i = '0;
    terr_jdb_ret_ready_i   = 1'b1;
    terr_jdb_ack_valid_i   = 1'b0;
    terr_jdb_ack_ticket_i  = '0;
    terr_jdb_ack_ok_i      = 1'b0;

    // The population sits at the island datum: a legal origin, and the one
    // under which a local position IS a world position.

    part_cap_full_i = '0;
    part_hist_sel_i = '0;

    // THE PARTICLE DRAW PATH (core entry I24). The three readies are HIGH, and
    // the comment at their declaration says why holding them low would stall
    // the whole particle tick rather than merely leaving the draw path idle.
    part_rung_ready_i = 1'b1;
    part_exp_ready_i  = 1'b1;
    part_sft_ready_i  = 1'b1;

    // THE DRAW DISPATCH (core entry I41). HIGH, and the declaration comment
    // says why: the draw drain is the LAST phase of CMD.EXEC's commit, so a
    // low ready parks the executor and stops the command path outright.

    // DEBUG.TRACE's arming is NO LONGER STIMULUS THIS BENCH OWNS. It rides the
    // packet as a `DebugTraceArm` 0xF003 record (ruling R52), built below with
    // the other ten, and CMD.EXEC lowers it. The bench's only debug-surface
    // job now is to be the HOST on the register aperture.
    hostreg_valid_i = 1'b0;
    hostreg_write_i = 1'b0;
    hostreg_addr_i  = '0;
    hostreg_wdata_i = '0;
    // One world unit of base radius, fx16. A VALUE, not a law: no species
    // radius table exists anywhere in the tree and the reference says that is
    // properly the owner's, so this bench picks one so that the projected size
    // is non-zero and the ladder has something to decide about.
    part_prj_base_radius_i = 32'sh0001_0000;
    part_prj_view_i       = 1'b0;
    part_prj_trail_i      = '0;
    part_prj_narrow_i     = 1'b0;
    part_prj_protected_i  = 1'b0;
    part_prj_gov_floor_i  = '0;
    part_prj_prev_rung_i  = '0;
    part_prj_hold_i       = '0;
    // No hold state exists off chip (I23's absent DDR), so every particle is a
    // first sighting. That is the honest setting here, not a convenience: it
    // makes PART.LADDER take its `p_first_i` path, which is the only one whose
    // inputs this bench can actually supply.
    part_prj_first_i      = 1'b1;
    part_prj_r_i          = 8'hC0;
    part_prj_g_i          = 8'h80;
    part_prj_b_i          = 8'h40;
    part_prj_src_id_i     = 16'h0BAD;
    // THE RULING-5 PACKET IS NO LONGER DRIVEN HERE. Slot 0 (invw24) is
    // GEOM.DEPTHQUANT's, inside GEOM.REPLAY, and slots 1..6 are the modelled
    // attribute store's (I46) -- see the store above.
    // R21: always take the terrain light (see its declaration).
    terr_light_ready_i = 1'b1;
    render_frame_open_q = 1'b0;
    geom_pose_start_i = 1'b0;
    geom_pose_bone_count_i = '0;
    geom_pose_root_dx_i = '0;
    geom_pose_root_dy_i = '0;
    geom_pose_root_dz_i = '0;
    geom_pose_bone_parent_i = '0;
    geom_pose_bone_tx_i = '0;
    geom_pose_bone_ty_i = '0;
    geom_pose_bone_tz_i = '0;
    geom_pose_quat_w_i = '0;
    geom_pose_quat_x_i = '0;
    geom_pose_quat_y_i = '0;
    geom_pose_quat_z_i = '0;
    geom_pose_inv_rest_i = '{default: '0};
    proj_cfg_we_i = '0;
    proj_cfg_view_i = '0;
    proj_cfg_addr_i = '0;
    proj_cfg_data_i = '0;
    proj_en_i = '0;
    terr_job_valid_i = '0;
    terr_job_ox_i = '0;
    terr_job_oz_i = '0;
    terr_job_level_i = '0;
    terr_job_lvl_nz_i = '0;
    terr_job_lvl_pz_i = '0;
    terr_job_lvl_nx_i = '0;
    terr_job_lvl_px_i = '0;
    terr_job_morph_i = '0;
    terr_job_surface_i = '0;
    terr_job_dual_i = '0;
    terr_job_src_id_i = '0;
    terr_job_view_mask_i = '0;
    terr_job_mat_a_i = '0;
    terr_job_mat_b_i = '0;
    terr_job_weight_i = '0;
    terr_sparse_fill_i = '0;
    proj_out_ready_i = '0;
    post_gd_present_i = '0;
    post_gd_dx_i = '0;
    post_gd_dy_i = '0;
    post_gg_present_i = '0;
    post_gg_glow_i = '0;
    post_gg_ink_i = '0;
    post_atm_en_i = '0;
    post_atm_valid_i = '0;
    post_atm_rgb_i = '0;
    post_atm_opacity_i = '0;
    post_atm_add_i = '0;
    post_hud_valid_i = '0;
    post_hud_rgb_i = '0;
    hist_ev_valid_i = '0;
    hist_ev_lane_valid_i = '0;
    hist_ev_err_i = '0;
    hist_ev_src_id_i = '0;
    cfg_valid_i = '0;
    cfg_op_i = '0;
    cfg_page_generation_i = '0;
    cfg_selector_i = '0;
    cfg_row_i = '0;
    cfg_crc32_i = '0;
    cfg_rsp_ready_i = '0;
    pal_load_valid_i = '0;
    pal_load_op_i = '0;
    pal_load_slot_i = '0;
    pal_load_gen_i = '0;
    pal_load_idx_i = '0;
    pal_load_rgb565_i = '0;
    pal_load_crc_ok_i = '0;
    tri_area2_i = '0;
    tri_flat_request_i = '0;
    tri_continuation_tail_i = '0;
    tri_fragment_state_i = '0;
    fill_req_ready_i = '0;
    fill_data_valid_i = '0;
    fill_data_i = '0;
    fill_refused_i = '0;
    frame_clear_word_i = '0;
    sheet_req_ready_i = '0;
    blank_cmd_i = '0;
    scanout_ack_i = '0;
    frame_swap_valid_i = '0;
    frame_swap_slot_i = '0;
    // hps_state_i, hps_byte_len_i and ring_wr_ready_i are driven by the
    // FRAME_RING word-view model (harness-as-HPS), not tied here.
    pkt_armed_q = 1'b0;
    // `hps_req_grant_i` / `hps_rd_*_i` are no longer tied here: the shell's HPS
    // port is served by the upload-arena model below (MEM.UPLOAD's socket).
    pad_present_i = '0;
    pad_buttons_i = '{default: '0};
    pad_lx_i = '{default: '0};
    pad_ly_i = '{default: '0};
    pad_rx_i = '{default: '0};
    pad_ry_i = '{default: '0};
    aud_wr_valid_i = '0;
    aud_wr_l_i = '0;
    aud_wr_r_i = '0;
    cnt_snap_ready_i = '0;
    render_frame_begin_i = '0;
    render_frame_end_i = '0;
    render_grid_w_i = '0;
    render_grid_h_i = '0;

    render_fill_word_i = '0;
    render_clear_word_i = '0;
    render_state_i = '0;
    render_src_a_i = '0;
    render_texel_rgb_i = '0;
    render_texel_a_i = '0;
    render_texel_idx_i = '0;
    render_fb_base_i = '0;
    render_fb_stride_i = '0;
    fb_writer_i = '0;
    // `phy_dq_i = '0` USED TO BE HERE. The SDRAM model drives it now, and a
    // bench that also drove it would be two drivers on the data bus with the
    // harness winning -- which reads as a memory that answers zero.

    // ---- entry I50: the ARM's Loom stream, ONE ROOT node -------------------
    // ROOT takes its twelve elements straight from `param` (the block's own
    // kind table), so this IS the instance transform the draw names: the
    // identity, which keeps the descriptor's object bound its world bound and
    // leaves GEOM.CULL seeing the sphere the fixture placed.
    geom_loom_node_index_i   = 10'(SMK_XFORM_NODE_C);
    geom_loom_parent_index_i = 10'd0;
    geom_loom_kind_i         = 4'd0;        // ROOT
    for (int unsigned i = 0; i < 12; i = i + 1)
      geom_loom_param_i[i] = ((i == 0) || (i == 5) || (i == 10)) ? FX16_ONE : 32'sd0;
    geom_loom_angle_i     = 16'd0;
    geom_loom_axis_i      = 2'd0;
    geom_loom_bodypatch_i = 1'b0;
    geom_loom_src_id_i    = 16'h00A1;
    geom_loom_first_i     = 1'b1;
    geom_loom_last_i      = 1'b1;           // a one-node stream, framed
    // The camera basis is the identity: no BILLBOARD node is sent, and zeros
    // would be a matrix this bench invented for a kind it never uses.
    for (int unsigned i = 0; i < 9; i = i + 1)
      geom_loom_cam_basis_i[i] = ((i == 0) || (i == 4) || (i == 8)) ? FX16_ONE : 32'sd0;

    // ---- reset ------------------------------------------------------------
    rst_n            = 1'b0;
    reset_released_q = 1'b0;
    cycles_q               = 0;
    ticks_seen_q           = 0;
    part_records_written_q = 0;
    part_tick_seen_q       = 1'b0;
    part_children_seen_q       = 0;
    part_children_at_contact_q = 0;

    // Benign, NON-ZERO configuration, chosen so nothing is refused for a
    // reason unrelated to wiring:
    //   species lifetime 0 = UNBOUNDED, so no particle dies of age and the
    //     survive verdict reaches PART.STATE;
    //   collision response STICK (see the spawn-on-collision block below),
    //     a KNOWN response, so PART.COLLIDE reports alive rather than refusing;
    //   the projector enabled and a rigid, unit-weight skin, so client A is
    //     offered real vertices.
    //
    // ALL THREE OF THOSE USED TO BE WIRES. Until 2026-09-19 this bench drove
    // the lifetime, the collision response and the spawn rule straight onto the
    // consumers' descriptor inputs, because the table they belong to had no
    // owner (entries I2/I3). It does now, so they are LOADED below instead --
    // and every particle behaviour this bench already asserted is now evidence
    // that the load reached the consumer through `zhao_part_table`.

    // ---- SPAWN ON COLLISION, the acceptance for owner ruling I4 -------------
    // Before the ruling this bench held the collider quiet (response IGNORE, no
    // terrain sample) and PRINTED two counters it knew could not move. Both of
    // them CAN move now, and making them move is the point of this stimulus.
    //
    //   * a PLANE at +100 with an up normal (n.p = c, c in Q NRM_Q), so every
    //     record -- all of which sit at y = 0 -- is inside the surface and
    //     makes a contact. It was a hand-driven heightfield until I6 closed;
    //     a unit up normal makes the plane's placement the same exact
    //     vertical h + CLEAR_EPS the heightfield's was, so the acceptance
    //     value below did not move;
    //   * STICK (3'd2), the simplest PLACING response: both coefficients are
    //     zero, so the velocity result needs no coefficient arithmetic and the
    //     placement is the exact vertical h + CLEAR_EPS;
    //   * a spawn descriptor that resolves, with one child per fired event.
    //     ONLY event 2 fires here -- the records carry no born flag, the age
    //     marker is disabled and the lifetime is unbounded -- so the other
    //     three spawn counters staying at zero is a specificity check, not an
    //     oversight.
    // (the plane is a SetPopulation record now -- see PART_PLANE_* below and
    //  the packet builder. Its values did not change: a unit +Y normal at
    //  y = PART_TER_H, so the acceptance value further down did not move.)
    // (the STICK response and the spawn rule are now LOADED INTO PART.TABLE
    //  after reset lifts -- see the load sequence below.)
    proj_en_i               = 1'b1;
    // THE MATRIX BANK IS WRITTEN NOW, and it was not before. GEOM.CULL takes
    // the same sixteen words at the same addresses as the projector -- its
    // own port comment says so -- so leaving the bank at zero would have made
    // the cull's verdict on the meshlet below an accident rather than a
    // result. It is written AFTER `proj_en_i` rises, from a task, so the
    // sixteen writes are one per clock on the real configuration port.
    // The replayed terrain triangle has no consumer in this core (entry I13),
    // so the bench SINKS it. Holding it low instead would back the replay up
    // into the sequencer and the resulting stall would read as a wiring fault.
    proj_out_ready_i        = 1'b1;
    // THE BENCH NO LONGER HANDS GEOM.SKIN AN IDENTITY MATRIX. It cannot:
    // zhao_geom_pose_palette owns those ports now (entry I10, closed). The
    // vertices still skin against the identity, because a palette holding no
    // decoded pose substitutes the identity bind pose per bone reference and
    // SAYS SO on geom_pal_bone_unset_o. So the skinned counts below are
    // unchanged, and the reason they are unchanged has moved from a harness
    // constant to a stated, counted behaviour of the design.
    //
    // WHAT THIS BENCH THEREFORE DOES NOT EXERCISE: a real decoded palette. The
    // decoder's source is entry I29 and has no producer in this tree; driving a
    // synthetic skeleton here would be this bench inventing an asset. The
    // palette PATH is checked below; the palette CONTENT is
    // geom_pose_palette_directed's job, against thirty-two distinct matrices.
    geom_pose_quat_w_i = 16'sd16384;      // quat16 is S1.0.14: this is 1.0
    for (int i = 0; i < 12; i++) geom_pose_inv_rest_i[i] = '0;
    geom_pose_inv_rest_i[0]  = FX16_ONE;  // row-major 3x4, the identity
    geom_pose_inv_rest_i[5]  = FX16_ONE;
    geom_pose_inv_rest_i[10] = FX16_ONE;

    // ---- PACKET P-TERRAIN: lay out the HPS arena BEFORE reset lifts -------
    // The memory is the board's, not the frame's: it exists before the console
    // starts and nothing in the DUT may depend on when it was written.
    for (int unsigned w = 0; w < HPS_WORDS; w++) hps_mem[w] = 64'd0;

    // T5's 32-byte record, four whole beats, little-endian, in the field order
    // `zhao_terrain_cmd`'s own header tabulates. Written field by field from
    // that table rather than as an opaque blob, so a layout change fails here
    // loudly instead of producing plausible terrain in the wrong place.
    //
    // N_TERR_REC records with DIFFERENT patch coordinates AND DIFFERENT page
    // addresses, and both differences are load-bearing. The directory keys on
    // {epoch, island, ix, iz}, so records that differed only in source id would
    // land in one entry and the later ones would report SAME rather than a
    // fresh claim; and a page address shared between records would let a swap
    // between two jobs go unseen on the loader's side. One claim and one page
    // per record is the evidence that each crossed every seam individually.
    //
    // `automatic`, AND IT IS THE WHOLE DEFECT THIS BENCH SPENT A PASS ON.
    // A variable declared inside a begin/end of a STATIC scope -- which an
    // `initial` block is -- has static lifetime, so its initialiser runs ONCE,
    // before time zero, and NOT on each pass of the loop. Written without
    // `automatic`, `b` stayed 0 for every r and all N_TERR_REC records were
    // written on top of each other at word 0: the arena held the LAST record
    // and then zeros. See the note at N_TERR_REC for what that looked like from
    // the far end of the spine.
    for (int unsigned r = 0; r < N_TERR_REC; r++) begin
      automatic int unsigned b = (LIST_OFF_C >> 3) + r * 4;
      hps_mem[b + 0] = {16'(r + 7),          // patch_iz   i16  bytes 6..7
                        16'(r + 3),          // patch_ix   i16  bytes 4..5
                        32'h0000_0042};      // island_id  u32  bytes 0..3
      hps_mem[b + 1] = 64'(HPS_BASE) +
                       64'(PAGE0_OFF_C + r * PAGE_BYTES_C); // hps_page_addr
      hps_mem[b + 2] = {8'd0,                // priority   u8   byte 23
                        8'h01,               // view_mask  u8   byte 22
                        16'h0001,            // flags      u16  bytes 20..21 (REQUIRED)
                        32'hDEAD_BEEF};      // expected_page_crc32c bytes 16..19
      hps_mem[b + 3] = {32'd0,               // reserved   u32  bytes 28..31
                        32'(1000 + r)};      // source_id  u32  bytes 24..27
    end

    // ---- THE ARENA IS READ BACK BEFORE IT IS BELIEVED ---------------------
    // MAKE THE HARNESS'S OWN LOSS COUNTABLE. The defect above wrote every
    // record to one address and was SILENT everywhere a check could have seen
    // it: the list CRC is folded by this bench over the same bytes the bench
    // wrote, so both sides agreed on the identical mistake and `crc_fails=0`
    // was then quoted as proof that the bridge had handed over every record.
    // A checker whose two operands are corrupted together cannot fire
    // (CLAUDE.md), and a bench that seals its own list is exactly that shape.
    //
    // This loop is the independent operand. It re-reads the arena field by
    // field against the intent stated above, so an aliasing, an off-by-one or
    // a widened record is LOUD at time zero and names the record -- instead of
    // arriving 5 ms later as "the console lost a record".
    for (int unsigned r = 0; r < N_TERR_REC; r++) begin
      automatic int unsigned b = (LIST_OFF_C >> 3) + r * 4;
      if (hps_mem[b + 0][31:0] !== 32'h0000_0042)
        $fatal(1, "SMOKE: arena record %0d holds island %08x, not 00000042 -- the list layout aliased", r, hps_mem[b + 0][31:0]);
      if (hps_mem[b + 0][47:32] !== 16'(r + 3))
        $fatal(1, "SMOKE: arena record %0d holds ix %0d, not %0d -- the list layout aliased", r, hps_mem[b + 0][47:32], r + 3);
      if (hps_mem[b + 0][63:48] !== 16'(r + 7))
        $fatal(1, "SMOKE: arena record %0d holds iz %0d, not %0d -- the list layout aliased", r, hps_mem[b + 0][63:48], r + 7);
      if (hps_mem[b + 1] !== 64'(HPS_BASE) + 64'(PAGE0_OFF_C + r * PAGE_BYTES_C))
        $fatal(1, "SMOKE: arena record %0d holds page address %016x, not %016x -- the list layout aliased",
               r, hps_mem[b + 1], 64'(HPS_BASE) + 64'(PAGE0_OFF_C + r * PAGE_BYTES_C));
      if (hps_mem[b + 2] !== {8'd0, 8'h01, 16'h0001, 32'hDEAD_BEEF})
        $fatal(1, "SMOKE: arena record %0d holds crc/flags/mask/prio %016x -- the list layout aliased", r, hps_mem[b + 2]);
      if (hps_mem[b + 3][31:0] !== 32'(1000 + r))
        $fatal(1, "SMOKE: arena record %0d holds source id %0d, not %0d -- the list layout aliased", r, hps_mem[b + 3][31:0], 1000 + r);
    end

    // Fold the list with the PRODUCTION folder. See the declaration for why
    // this makes the bench no evidence about the CRC and good evidence about
    // the spine. Bytes are folded eight at a time, low byte first, which is
    // the folder's own convention and the order the bridge returns them in.
    fold_c_i = 32'hFFFF_FFFF;
    fold_n_i = 4'd8;
    for (int unsigned w = 0; w < (LIST_BYTES_C / 8); w++) begin
      fold_d_i = hps_mem[(LIST_OFF_C >> 3) + w];
      #1ns;
      fold_c_i = fold_c_o;
    end
    terr_cmd_list_crc_i = ~fold_c_i;

    terr_cfg_epoch_i       = 32'd9;
    terr_cfg_arena_base_i  = HPS_BASE;
    terr_cfg_arena_bytes_i = 32'(HPS_WORDS * 8);
    terr_cfg_load_budget_i = 16'd32;   // T7's per-frame page budget

    // ---- ONE COMMAND PACKET: BeginFrame, PublishResource, SetEnvironment,
    // EndFrame. SetEnvironment (owner ruling R25) is the record whose bank the
    // geometry below is lit with; its values come from the fixture generator,
    // which derives the expected lit colour from the same record.
    // Built from the GENERATED record packers and sealed with the generated
    // CRC-32C step -- the bench writes no layout by hand. It travels the whole
    // command path: FRAME_RING slot 0 -> CMD.SCHEDULER claim -> CMD.DMA fetch
    // over the shell's REAL HPS bridge -> CMD.DECODER's verdict and CMD.EXEC's
    // PublishResource arm (owner ruling R17) -> MEM.UPLOAD -> the TERRAIN.BUILD
    // socket. The payload it names is sealed with the SAME production folder
    // as the terrain list above.
    for (int unsigned w = 0; w < UPL_WORDS_C; w++)
      upl_mem[w] = 64'hC0DE_5EED_0000_0000 + 64'(w * 32'h0101_0101);
    // Record 0 of the MATERIAL_SET is a LEGAL MaterialRecord from the generated
    // packer (one sample, recipe 0), so MATERIAL.RESOLVE's answer can be
    // compared bit for bit against what the command uploaded. Records 1-7
    // stay the fill pattern; nothing requests them.
    begin : build_material
      zhao_abi_pkg::zhao_material_record_t mr;
      mr = '0;
      mr.control                    = 8'h01;          // count 1, recipe 0
      mr.recipe_weight              = 8'h80;
      mr.sample0.binding_slot       = 16'h0003;
      mr.sample0.binding_generation = 8'h05;
      mr.sample0.modes              = 8'h00;          // wrap 0: legal
      mr.palette_base               = 32'h0000_1200;
      mr.raster_state               = 32'h0000_0007;
      upl_rec0 = zhao_abi_pkg::zhao_pack_material_record(mr);
      for (int unsigned w = 0; w < 4; w++) upl_mem[w] = upl_rec0[64*w +: 64];
    end
    // ---- THE MESH_STREAM PAGE (owner ruling R29) --------------------------
    // Words MSH_ARENA_W..+MSH_WORDS_C of the same staging arena: the frozen
    // 64-byte header, the 64-byte meshlet descriptor, the index run and the
    // vertex records. Every byte is the fixture's own -- the same descriptor
    // fields, the same `SGF_IX` triplets and the same `vdec_record(n)` this
    // bench poked into VRAM before R29 -- so what changed is HOW THE BYTES GET
    // THERE (MEM.UPLOAD writes them and publishes the row) and not what they
    // are. The two `ifdef` controls still mutate the page here, and the CRC is
    // taken AFTERWARDS, so a deliberately broken descriptor is uploaded
    // faithfully and refused by the block that should refuse it.
    begin : build_mesh_page
      logic [63:0] hw [8];
      logic [63:0] dw [8];
      logic [191:0] ixw;
      for (int unsigned w = 0; w < MSH_WORDS_C; w++) upl_mem[MSH_ARENA_W + w] = 64'd0;
      // the header: format 1, SGF_N_MESHLETS meshlets, generation 1, table at 64
      // byte 0 format, byte 1 reserved, [3:2] meshlet count, [5:4] generation.
      hw[0] = {16'h0000, 16'd1, 16'(N_GEOM_MESHLETS), 8'd0, 8'd1};
      hw[1] = {32'd0, GEOM_DESC_OFF};
      for (int unsigned k = 2; k < 8; k++) hw[k] = 64'd0;
      hw[7][63:32] = fixture_desc_crc(hw);   // the SAME fold, over bytes 0..59
      for (int unsigned k = 0; k < 8; k++) upl_mem[MSH_ARENA_W + k] = hw[k];
      // ONE DESCRIPTOR PER MESHLET (R57), field for field as before, with
      // PAGE-relative offsets. Every descriptor names the SAME vertex run and
      // its OWN index run, so the partition costs the page one table entry and
      // one 64-byte index line per meshlet and nothing else.
      for (int unsigned mi = 0; mi < N_GEOM_MESHLETS; mi = mi + 1) begin
        dw[0] = {16'h0000, 16'h0001, 8'(SGF_MESH_NTRI[mi]), 8'(N_GEOM_VERTS), 8'd0, 8'd1};
        dw[1] = 64'd0;
        dw[2] = {GEOM_BOUND_R, 32'h0001_8000};
        dw[3] = {GEOM_IX_OFF + GEOM_IX_STRIDE * mi, GEOM_VX_OFF};
        dw[4] = {32'd0, 16'd0, 16'd1};
`ifdef ZHAO_SMOKE_BAD_DESC
        // POSITIVE CONTROL, INVERTED POLARITY (`-BadDescriptor`), unchanged in
        // meaning: byte 40 is inside the descriptor's reserved span, which
        // GEOM.MESHFETCH's sixth refusal row requires to be zero. Only the
        // FIRST descriptor is broken, so the refusal is attributable.
        dw[5] = (mi == 0) ? 64'h0000_0000_0000_0001 : 64'd0;
`else
        dw[5] = 64'd0;
`endif
        dw[6] = 64'd0;
        dw[7] = 64'd0;
        dw[7][63:32] = fixture_desc_crc(dw);
        for (int unsigned k = 0; k < 8; k++)
          upl_mem[MSH_ARENA_W + 8 + 8 * mi + k] = dw[k];
        // ...and its index run, at its own line.
        ixw = '0;
        for (int unsigned k = 0; k < 3 * SGF_MESH_NTRI[mi]; k = k + 1)
          ixw[8 * k +: 8] = SGF_IX[3 * SGF_MESH_FIRST[mi] + k];
        upl_mem[MSH_ARENA_W + ((GEOM_IX_OFF + GEOM_IX_STRIDE * mi) >> 3) + 0] = ixw[63:0];
        upl_mem[MSH_ARENA_W + ((GEOM_IX_OFF + GEOM_IX_STRIDE * mi) >> 3) + 1] = ixw[127:64];
        upl_mem[MSH_ARENA_W + ((GEOM_IX_OFF + GEOM_IX_STRIDE * mi) >> 3) + 2] = ixw[191:128];
      end
      // the vertex records
      for (int unsigned nv = 0; nv < N_GEOM_VERTS; nv = nv + 1) begin
        automatic logic [255:0] rec;
        rec = vdec_record(nv);
`ifdef ZHAO_SMOKE_BAD_VERTEX
        // THE R31 CONTROL (`-BadVertex`), unchanged: record 3's first reserved
        // byte is nonzero, so GEOM.VDECODE refuses exactly that record and the
        // batch must be DROPPED while the frame completes.
        if (nv == 3) rec[192 +: 8] = 8'h01;
`endif
        for (int unsigned k = 0; k < 4; k++)
          upl_mem[MSH_ARENA_W + (GEOM_VX_OFF >> 3) + 4 * nv + k] = rec[64*k +: 64];
      end
    end
    fold_c_i = 32'hFFFF_FFFF;
    fold_n_i = 4'd8;
    for (int unsigned w = 0; w < UPL_WORDS_C; w++) begin
      fold_d_i = upl_mem[w];
      #1ns;
      fold_c_i = fold_c_o;
    end
    upl_crc_material_q = ~fold_c_i;
    // ...and the MESH_STREAM page's own CRC, over its 704 bytes, folded with
    // the SAME production block and AFTER the two controls have had their say,
    // so a deliberately broken descriptor is uploaded faithfully and refused by
    // the block that should refuse it rather than by the uploader.
    fold_c_i = 32'hFFFF_FFFF;
    for (int unsigned w = 0; w < MSH_WORDS_C; w++) begin
      fold_d_i = upl_mem[MSH_ARENA_W + w];
      #1ns;
      fold_c_i = fold_c_o;
    end
    upl_crc_mesh_q = ~fold_c_i;
    // ---- THE SPECIES_TABLE PAGE (R42, core entry I33) --------------------
    // Four species-0 descriptors, in zref::species_page's frozen layout: a
    // 64-byte header then 32-byte entries, TWO per 64-byte line. These are the
    // SAME four words this bench used to drive into part_tbl_ld_*, byte for
    // byte -- what changed is who carries them.
    begin : spt_image
      automatic logic [511:0] ln;
      automatic logic [255:0] e0, e1;
      // the header line
      ln = '0;
      ln[31:0]  = SPT_MAGIC_C;
      ln[47:32] = 16'd1;                       // version
      ln[63:48] = 16'(SPT_ENTRIES_C);
`ifdef ZHAO_SMOKE_BAD_SPECIES_PAGE
      // THE NEGATIVE CONTROL FOR EVERY PART.TABLE CHECK IN THIS FILE, and it
      // is a committed switch rather than an edit somebody made once and
      // reverted. One byte of the page's magic, wrong. The loader must refuse
      // the page WHOLE, `part_tbl_bad_magic_o` must fire, and every table
      // check at the foot of the run must go RED -- otherwise those checks are
      // passing on something other than the page's contents.
      //
      // It is a PLAIN `ifdef` on purpose: CLAUDE.md records that a
      // command-line `-D` cannot override a FUNCTION-LIKE `define` and says
      // nothing when it fails to, so a macro-selected control has to be a form
      // `-D` reaches. Run it by adding the define to the verilate step in
      // tests/prod/run_console_core_smoke.ps1.
      ln[7:0] = 8'hFF;
`endif
      for (int unsigned k = 0; k < 8; k++) upl_mem[SPT_ARENA_W + k] = ln[64*k +: 64];
      // entries 0 and 1
      e0 = spt_entry(TBL_SEL_UPD, 7'd0, 2'd0,
                     PART_TBL_LD_W'(TBL_RECIPE_COLOUR) << TBL_U_OFF_RCP);
      e1 = spt_entry(TBL_SEL_COL, 7'd0, 2'd0,
                     PART_TBL_LD_W'(3'd2) << TBL_C_OFF_RSP);
      ln = {e1, e0};
      for (int unsigned k = 0; k < 8; k++) upl_mem[SPT_ARENA_W + 8 + k] = ln[64*k +: 64];
      // entries 2 and 3
      e0 = spt_entry(TBL_SEL_SPW, 7'd0, 2'd2,
                     (PART_TBL_LD_W'(7'd0) << TBL_S_OFF_CHD) |
                     (PART_TBL_LD_W'(5'd1) << TBL_S_OFF_CNT) |
                     (PART_TBL_LD_W'(1'b1) << TBL_S_OFF_KNW));
      e1 = spt_entry(TBL_SEL_CRV, 7'd0, 2'd0,
                     (PART_TBL_LD_W'(TBL_CURVE_SIZE)   << TBL_V_OFF_SIZ) |
                     (PART_TBL_LD_W'(TBL_CURVE_COLOUR) << TBL_V_OFF_CLR));
      ln = {e1, e0};
      for (int unsigned k = 0; k < 8; k++) upl_mem[SPT_ARENA_W + 16 + k] = ln[64*k +: 64];
    end
    fold_c_i = 32'hFFFF_FFFF;
    for (int unsigned w = 0; w < SPT_WORDS_C; w++) begin
      fold_d_i = upl_mem[SPT_ARENA_W + w];
      #1ns;
      fold_c_i = fold_c_o;
    end
    upl_crc_species_q = ~fold_c_i;
    begin : build_packet
      zhao_abi_pkg::zhao_rec_begin_frame_t      bf;
      zhao_abi_pkg::zhao_rec_set_presentation_contract_t pc;
      zhao_abi_pkg::zhao_rec_set_view_t         sv;
      zhao_abi_pkg::zhao_rec_publish_resource_t pr;
      zhao_abi_pkg::zhao_rec_publish_resource_t pr2;
      zhao_abi_pkg::zhao_rec_publish_resource_t pr3;
      zhao_abi_pkg::zhao_rec_draw_form_t        df;
      zhao_abi_pkg::zhao_rec_end_frame_t        ef;
      zhao_abi_pkg::zhao_rec_set_environment_t  se;
      zhao_abi_pkg::zhao_rec_set_population_t   spop;
      logic [255:0] bfv, efv;
      logic [383:0] prv, pr2v, pr3v, pcv, sev, spopv;
      logic [255:0] dfv;
      logic [8*zhao_abi_pkg::ZHAO_SET_VIEW_BYTES-1:0] svv;   // R63 grew SetView 96 -> 112; take the width from the package
      logic [511:0] matv;
      logic [31:0]  c;
      int unsigned  o;
      // THE RUNNING RECORD OFFSET, added 2026-09-20 with ruling R63. The ten
      // offsets below used to be literals, and when SetView grew from 96 to 112
      // bytes every record after it landed 16 bytes early -- with a VALID CRC
      // over the wrong bytes, so the decoder refused the packet and the smoke
      // reported "GEOM.REPLAY released no meshlet", which reads as a geometry
      // fault and is a layout one. A running sum cannot make that mistake, and
      // `ro` at the end IS the byte count the header must declare.
      int unsigned  ro;
      // ...and `nrec` at the end IS the record count the header must declare,
      // for exactly the same reason. Added 2026-09-20 after a review found the
      // header carrying 13 and the build line printing 10.
      int unsigned  nrec;
      zhao_abi_pkg::zhao_rec_set_post_t         sp;
      zhao_abi_pkg::zhao_rec_set_grade_table_t  gt;
      logic [255:0] spv;
      logic [767:0] gtv2;
      // R52: DebugTraceArm 0xF003, the real producer of DEBUG.TRACE's arming.
      zhao_abi_pkg::zhao_rec_debug_trace_arm_t  ta;
      logic [255:0] tav;
      // EVERY RECORD IS CLEARED BEFORE ANY OF THEM IS FILLED, and the order is
      // load-bearing rather than tidy. The 2026-09-20 merge of the geom and post
      // packets moved this line BELOW the SetPresentationContract and SetView
      // blocks, so two fully populated records were zeroed again before they
      // were packed: CMD.DMA's record walk then read opcode 0x0000 / 0 bytes at
      // offset 32, stamped ST_BAD_LENGTH, and no record ever reached CMD.EXEC.
      // The visible symptom was six subsystems away -- the SetEnvironment never
      // landed, so the smoke's geometry job (gated on `geom_light_env_loads_o
      // >= 2`, ruling R25) was never issued and GEOM.REPLAY released no meshlet.
      bf = '0; pc = '0; sv = '0; pr = '0; ef = '0; se = '0; pr2 = '0; pr3 = '0; df = '0; sp = '0; gt = '0; spop = '0; ta = '0;
      // SetPresentationContract: mode 0 (VIDEO_Z60, the mode the scheduler
      // already runs), two views, and the five token CEILINGS.
      pc.h_opcode = zhao_abi_pkg::ZHAO_OP_SET_PRESENTATION_CONTRACT; pc.h_record_bytes = 16'd48;
      pc.mode = 8'd0; pc.view_count = 8'd2;
      pc.geometry_tokens_0 = TOK_G0_C; pc.geometry_tokens_1 = TOK_G1_C;
      pc.fragment_tokens_0 = TOK_F0_C; pc.fragment_tokens_1 = TOK_F1_C;
      pc.shared_tokens     = TOK_SH_C;
      // SetView, view 1, carrying THE SAME camera the host already wrote
      // (SGF_MAT, profile 0) so the raster is unchanged -- and view 1's REQUEST.
      for (int unsigned k = 0; k < 16; k++) matv[32*k +: 32] = SGF_MAT[k];
      sv.h_opcode = zhao_abi_pkg::ZHAO_OP_SET_VIEW;
      // FROM THE PACKAGE, NOT A LITERAL. This was `16'd96` until ruling R63
      // grew the record to 112 bytes; taking it from the generated constant is
      // what stops the bench and the ABI from disagreeing again.
      sv.h_record_bytes = 16'(zhao_abi_pkg::ZHAO_SET_VIEW_BYTES);
      sv.view_id = 8'd1; sv.viewport_id = 8'd1; sv.flags = 16'd0;
      sv.view_projection = zhao_abi_pkg::zhao_mat4fx_t'(matv);
      sv.pixel_error     = 32'h0001_0000;
      sv.geometry_tokens = TOK_REQ_G1_C;
      sv.fragment_tokens = TOK_REQ_F1_C;
      // THE EYE (ruling R63), carried but NOT YET OBSERVED IN THIS BENCH, and
      // said so rather than left to be inferred. `zhao_cmd_exec` commits these
      // three words to projector configuration addresses 19/20/21, and the block
      // that decodes them -- `zhao_view_eye` -- is not composed into
      // `zhao_console_core` yet because its consumer `zhao_terrain_lod` is not
      // (core entry I21). The producer is proven at `tests/command/
      // cmd_exec_directed.cpp` cases 22-24 and the store at `view_eye_directed`.
      // A DISTINCTIVE, NON-ZERO value is used anyway: zero is a legal eye, so a
      // zero here would prove nothing the day the consumer lands, and a value
      // that is already flowing is one less thing to add then.
      sv.eye_0 = 32'h0004_0000;   // x = +4.0 m, fx16
      sv.eye_1 = 32'h0002_8000;   // y = +2.5 m
      sv.eye_2 = 32'hFFF6_0000;   // z = -10.0 m, signed on purpose

      bf.h_opcode = zhao_abi_pkg::ZHAO_OP_BEGIN_FRAME;       bf.h_record_bytes = 16'd32;
      bf.frame_id = 32'd1;
      pr.h_opcode = zhao_abi_pkg::ZHAO_OP_PUBLISH_RESOURCE;  pr.h_record_bytes = 16'd48;
      pr.h_source_id    = 32'd77;
      pr.resource       = {UPL_INDEX_C, 8'h2A};              // {index:24, generation:8}, index HIGH
      pr.hps_addr_lo    = UPL_ARENA_C;
      pr.hps_addr_hi    = 32'd0;
      pr.vram_dst       = UPL_REGION_C;
      pr.length         = 32'(UPL_WORDS_C * 8);
      pr.crc32c         = upl_crc_material_q;
      pr.new_generation = UPL_GEN_C;
      pr.epoch          = UPL_EPOCH_C;
      pr.dst_slot       = UPL_SLOT_C;
      pr.kind           = UPL_KIND_C;
      // THE SECOND PUBLICATION: the MESH_STREAM page the draw below names.
      // Its handle generation is the low byte of the generation it is
      // published with, because that is what GEOM.DRAWJOB compares -- the same
      // law `zhao_material_resolve` applies to the same handle shape.
      pr2.h_opcode = zhao_abi_pkg::ZHAO_OP_PUBLISH_RESOURCE; pr2.h_record_bytes = 16'd48;
      pr2.h_source_id    = 32'd79;
      pr2.resource       = {MSH_INDEX_C, MSH_GEN_C[7:0]};
      pr2.hps_addr_lo    = UPL_ARENA_C + 32'(MSH_ARENA_W * 8);
      pr2.hps_addr_hi    = 32'd0;
      pr2.vram_dst       = MSH_DST_C;
      pr2.length         = 32'(MSH_WORDS_C * 8);
      pr2.crc32c         = upl_crc_mesh_q;
      pr2.new_generation = MSH_GEN_C;
      pr2.epoch          = UPL_EPOCH_C;
      pr2.dst_slot       = MSH_SLOT_C;
      pr2.kind           = MSH_KIND_C;
      // THE THIRD PUBLICATION (R42): the SPECIES_TABLE page. Its kind is what
      // u_part_table_loader watches for, so nothing else in the console has
      // to know a species table exists.
      pr3.h_opcode = zhao_abi_pkg::ZHAO_OP_PUBLISH_RESOURCE; pr3.h_record_bytes = 16'd48;
      pr3.h_source_id    = 32'd83;
      pr3.resource       = {SPT_INDEX_C, SPT_GEN_C[7:0]};
      pr3.hps_addr_lo    = UPL_ARENA_C + 32'(SPT_ARENA_W * 8);
      pr3.hps_addr_hi    = 32'd0;
      pr3.vram_dst       = SPT_DST_C;
      pr3.length         = 32'(SPT_WORDS_C * 8);
      pr3.crc32c         = upl_crc_species_q;
      pr3.new_generation = SPT_GEN_C;
      pr3.epoch          = UPL_EPOCH_C;
      pr3.dst_slot       = SPT_SLOT_C;
      pr3.kind           = SPT_KIND_C;
      // THE DRAW ITSELF (`DrawForm 0x0300`). `form` names the page above,
      // `transform` names the Loom node this bench streams, and `flags` is
      // ZERO -- cull mode NONE, the double-sided law every capture was
      // recorded under, so the fixture's triangles are not culled by winding.
      df.h_opcode = zhao_abi_pkg::ZHAO_OP_DRAW_FORM;         df.h_record_bytes = 16'd32;
      df.h_source_id   = 32'(SMK_DRAW_SRC_C);
      df.form          = {MSH_INDEX_C, MSH_GEN_C[7:0]};
      df.material_set  = {UPL_INDEX_C, 8'h2A};
      df.transform     = {24'(SMK_XFORM_NODE_C), 8'h00};
      df.viewport_mask = 8'h03;                 // BOTH cameras, as before
      df.semantic_weight = 8'd7;
      df.flags         = 16'd0;
      se.h_opcode = zhao_abi_pkg::ZHAO_OP_SET_ENVIRONMENT;   se.h_record_bytes = 16'd48;
      se.h_source_id = 32'd78;
      se.sun_yaw     = SGF_ENV_YAW;
      se.sun_pitch   = SGF_ENV_PITCH;
      se.sun_colour  = SGF_ENV_SUN;
      se.ambient     = SGF_ENV_AMB;
      // R41: THE POPULATION DESCRIPTOR, from the command packet. Every value
      // here used to be a board pin on zhao_console_core (entry I7) or a
      // provisional seed port (I1's, R46), and they are the SAME values --
      // origin at the island datum, a unit +Y plane at y = PART_TER_H, and a
      // first generation of N_PART_RECORDS staged in buffer 0. What changed is
      // that they now TRAVEL: decoder -> executor -> PART.POP -> PART.COLLIDE,
      // PART.TERRAIN_TAP and the generation store.
      spop.h_opcode = zhao_abi_pkg::ZHAO_OP_SET_POPULATION;    spop.h_record_bytes = 16'd48;
      spop.h_source_id   = 32'd81;
      spop.population    = SMK_POP_HANDLE_C;
      spop.origin_x      = 32'sd0;
      spop.origin_y      = 32'sd0;
      spop.origin_z      = 32'sd0;
      spop.active_count  = 32'(N_PART_RECORDS);
      spop.plane_c       = 32'(PART_TER_H) <<< 10;   // y = +100 LSBs, Q NRM_Q
      spop.plane_nx      = 16'sd0;
      spop.plane_ny      = 16'sd1024;                // NRM_Q = 10: a unit +Y normal
      spop.plane_nz      = 16'sd0;
      spop.flags         = 16'h0003;                 // b0 seed, b1 plane_enable
      ef.h_opcode = zhao_abi_pkg::ZHAO_OP_END_FRAME;         ef.h_record_bytes = 16'd32;
      // SetPost (R36): POST.ECHO ARMED (R35) -- the capture this bench checks
      // word for word exists only because this record arms it -- and a look
      // whose every value is DISTINCTIVE and whose picture is still the
      // identity: no grade, no flash amount, no glow for the gain to scale,
      // no ink bit for the ink colour to paint. Each value is read back at
      // POST.COMPOSITE's own port below, so the carrier is proven end to end.
      sp.h_opcode = zhao_abi_pkg::ZHAO_OP_SET_POST;          sp.h_record_bytes = 16'd32;
      sp.bloom_gain   = SP_GAIN_C;
`ifdef ZHAO_SMOKE_NO_ECHO_ARM
      sp.flags        = 8'h00;                               // NEGATIVE CONTROL: disarmed
`else
      sp.flags        = 8'h02;                               // echo ARM, grade off
`endif
      sp.flash_amount = 8'd0;
      sp.bias_r       = 16'(SP_BIAS_R_C);
      sp.bias_g       = 16'(SP_BIAS_G_C);
      sp.bias_b       = 16'(SP_BIAS_B_C);
      sp.flash        = SP_FLASH_C;
      sp.ink          = SP_INK_C;
      // SetGradeTable (R36): eight B-curve entries at 8..15, distinctive s24s,
      // read back out of POST.COMPOSITE's own table below.
      gt.h_opcode = zhao_abi_pkg::ZHAO_OP_SET_GRADE_TABLE;   gt.h_record_bytes = 16'd96;
      gt.curve = 8'd2; gt.first = 8'd8; gt.count = 8'd8;
      // DebugTraceArm (R52): ARM STAGE 0 -- `zref::trace::kCommandDecoder` --
      // and nothing else, because stage 0 is the only one this console has a
      // producer for (core entry I18). `flags` bit 0 clears the ring first, so
      // the count this bench checks describes THIS packet and nothing before
      // it. It is placed SECOND, right after BeginFrame, because the arming
      // record is itself offered to the ring before the arm lands -- so every
      // record after it is traced and it is not.
      ta.h_opcode = zhao_abi_pkg::ZHAO_OP_DEBUG_TRACE_ARM; ta.h_record_bytes = 16'd32;
`ifdef ZHAO_SMOKE_BAD_TRACE_ARM
      // NEGATIVE CONTROL for `cmd_exec_trace_arm_refused_o`: bit 7 of
      // stage_mask is unassigned, so the record is refused WHOLE and nothing
      // is armed. REFUSE, NEVER MASK.
      ta.stage_mask = 8'b1000_0001;
`else
      ta.stage_mask = 8'b0000_0001;
`endif
      ta.flags      = 8'h01;
      bfv = zhao_abi_pkg::zhao_pack_begin_frame(bf);
      prv  = zhao_abi_pkg::zhao_pack_publish_resource(pr);
      pr2v = zhao_abi_pkg::zhao_pack_publish_resource(pr2);
      pr3v = zhao_abi_pkg::zhao_pack_publish_resource(pr3);
      dfv  = zhao_abi_pkg::zhao_pack_draw_form(df);
      sev = zhao_abi_pkg::zhao_pack_set_environment(se);
      spopv = zhao_abi_pkg::zhao_pack_set_population(spop);
      efv = zhao_abi_pkg::zhao_pack_end_frame(ef);
      pcv = zhao_abi_pkg::zhao_pack_set_presentation_contract(pc);
      svv = zhao_abi_pkg::zhao_pack_set_view(sv);

      spv = zhao_abi_pkg::zhao_pack_set_post(sp);
      tav = zhao_abi_pkg::zhao_pack_debug_trace_arm(ta);
      gtv2 = zhao_abi_pkg::zhao_pack_set_grade_table(gt);
      for (int unsigned k = 0; k < 72; k++)
        gtv2[8*(zhao_abi_pkg::ZHAO_SET_GRADE_TABLE_OFF_VECTORS_0 + k) +: 8] = sm_grade_byte(k);
      for (int unsigned k = 0; k < PKT_MAX_C; k++) pkt_mem[k] = 8'd0;
      o = zhao_abi_pkg::ZHAO_FRAME_HEADER_BYTES;
      ro   = 0;
      // COUNTED, NOT DECLARED, for the same reason `ro` is. Review 2026-09-20
      // found the header field saying 13 while the build line three screens
      // down still printed `records=10` -- two literals for one fact, and the
      // one that was wrong is the one that gets READ, because it is the
      // evidence line. Both now come from this counter, so they cannot
      // disagree with each other or with the records actually laid down.
      nrec = 0;
      for (int unsigned k = 0; k < 32; k++) pkt_mem[o + ro + k] = bfv[8*k +: 8];
      ro = ro + 32;  nrec = nrec + 1;
      // DebugTraceArm is SECOND (R52): every record the ring is meant to see comes
      // after the arm, which is what TRACE_SKIP_C counts.
      for (int unsigned k = 0; k < 32; k++) pkt_mem[o + ro + k] = tav[8*k +: 8];
      ro = ro + 32;  nrec = nrec + 1;
      for (int unsigned k = 0; k < 48; k++) pkt_mem[o + ro + k] = pcv[8*k +: 8];
      ro = ro + 48;  nrec = nrec + 1;
      for (int unsigned k = 0; k < zhao_abi_pkg::ZHAO_SET_VIEW_BYTES; k++)
        pkt_mem[o + ro + k] = svv[8*k +: 8];
      ro = ro + zhao_abi_pkg::ZHAO_SET_VIEW_BYTES;  nrec = nrec + 1;
      for (int unsigned k = 0; k < 48; k++) pkt_mem[o + ro + k] = prv[8*k +: 8];
      ro = ro + 48;  nrec = nrec + 1;
      for (int unsigned k = 0; k < 48; k++) pkt_mem[o + ro + k] = pr2v[8*k +: 8];
      ro = ro + 48;  nrec = nrec + 1;
      for (int unsigned k = 0; k < 48; k++) pkt_mem[o + ro + k] = pr3v[8*k +: 8];
      ro = ro + 48;  nrec = nrec + 1;
      for (int unsigned k = 0; k < 48; k++) pkt_mem[o + ro + k] = sev[8*k +: 8];
      ro = ro + 48;  nrec = nrec + 1;
      for (int unsigned k = 0; k < 32; k++) pkt_mem[o + ro + k] = spv[8*k +: 8];
      ro = ro + 32;  nrec = nrec + 1;
      for (int unsigned k = 0; k < 96; k++) pkt_mem[o + ro + k] = gtv2[8*k +: 8];
      ro = ro + 96;  nrec = nrec + 1;
      for (int unsigned k = 0; k < 48; k++) pkt_mem[o + ro + k] = spopv[8*k +: 8];
      ro = ro + 48;  nrec = nrec + 1;
      for (int unsigned k = 0; k < 32; k++) pkt_mem[o + ro + k] = dfv[8*k +: 8];
      ro = ro + 32;  nrec = nrec + 1;
      for (int unsigned k = 0; k < 32; k++) pkt_mem[o + ro + k] = efv[8*k +: 8];
      ro = ro + 32;  nrec = nrec + 1;
      // THE PACKET MUST FIT. `pkt_mem` is PKT_MAX_C bytes and an out-of-range
      // write to an unpacked array is silently DISCARDED, so adding a record
      // without raising the literal produces a truncated packet and a failure
      // three subsystems downstream. This cannot un-write what has already
      // been discarded -- the records are laid before `ro` is final -- but it
      // names the cause instead of leaving a garbled packet to be debugged.
      // Q010, 2026-09-20: there was no bound check at all.
      if (o + ro + 4 > PKT_MAX_C)
        $fatal(1, "SMOKE: the command packet needs %0d bytes and PKT_MAX_C is %0d -- raise it (header %0d + body %0d + CRC 4)",
               o + ro + 4, PKT_MAX_C, o, ro);
      // header: magic, abi version, flags, frame id 1, sequence 1, epoch 0,
      // deadline 0 (the mode's period), `nrec` records, `ro` bytes of them --
      // both counted by the packing above, never declared beside it.
      {pkt_mem[3], pkt_mem[2], pkt_mem[1], pkt_mem[0]}     = zhao_abi_pkg::ZHAO_FRAME_MAGIC;
      {pkt_mem[5], pkt_mem[4]}                             = 16'(zhao_abi_pkg::ZHAO_ABI_VERSION);
      // FLAGS BIT 0 IS `ZH_ABI_DEBUG_FLAG_REQUIRED` AND IT IS NOT OPTIONAL
      // HERE: this packet carries DebugTraceArm 0xF003, and every 0xF0nn
      // opcode requires it. The comment above used to say "flags 0", which was
      // true before R52 and is the kind of stale sentence that gets copied
      // into the next bench.
      {pkt_mem[zhao_abi_pkg::ZHAO_OFF_FLAGS + 1], pkt_mem[zhao_abi_pkg::ZHAO_OFF_FLAGS]} = 16'h0001;
      {pkt_mem[11], pkt_mem[10], pkt_mem[9], pkt_mem[8]}   = 32'd1;
      {pkt_mem[15], pkt_mem[14], pkt_mem[13], pkt_mem[12]} = 32'd1;
      {pkt_mem[27], pkt_mem[26], pkt_mem[25], pkt_mem[24]} = 32'(nrec);
      // `ro` IS THE BYTE COUNT -- the same running sum that placed the records,
      // so the header cannot declare a length the packing did not produce.
      {pkt_mem[31], pkt_mem[30], pkt_mem[29], pkt_mem[28]} = 32'(ro);
      c = 32'hFFFF_FFFF;
      for (int unsigned k = 0; k < 32; k++) c = zhao_abi_pkg::zhao_crc32c_step(c, pkt_mem[k]);
      {pkt_mem[35], pkt_mem[34], pkt_mem[33], pkt_mem[32]} = ~c;
      c = 32'hFFFF_FFFF;
      for (int unsigned k = 0; k < ro; k++) c = zhao_abi_pkg::zhao_crc32c_step(c, pkt_mem[o + k]);
      {pkt_mem[o+ro+3], pkt_mem[o+ro+2], pkt_mem[o+ro+1], pkt_mem[o+ro]} = ~c;
      pkt_len_q   = o + ro + 4;
      pkt_armed_q = 1'b1;
      // Printed at BUILD time, not at the end: when the packet is malformed the
      // bench dies inside GEOM.REPLAY's watchdog and every summary line below is
      // unreachable, so the one fact that would have identified a layout fault
      // is the one fact you cannot see. Ruling R63's SetView growth cost a whole
      // debugging pass to exactly that.
      $display("SMOKE: packet    records=%0d command_bytes=%0d pkt_len=%0d setview=%0d",
               nrec, ro, pkt_len_q, zhao_abi_pkg::ZHAO_SET_VIEW_BYTES);
      // EVERY RECORD'S OWN HEADER, AGAINST THE LAWFUL SIZE FOR ITS OPCODE.
      // This walks the bytes that will actually be fetched, not the variables
      // that wrote them, and it exists because of a real defect: a lost edit
      // left `sv.h_record_bytes` at zero, and the ONLY symptom anywhere in the
      // console was `zhao_cmd_dma` returning ST_BAD_LENGTH -- which surfaces
      // 200,000 cycles later as "GEOM.REPLAY released no meshlet", a sentence
      // about the wrong subsystem entirely. Four lines here name the record.
      begin : dump_records
        int unsigned wo;
        int unsigned rbv;
        wo = 0;
        while (wo < ro) begin
          rbv = 32'({pkt_mem[o+wo+3], pkt_mem[o+wo+2]});
          $display("SMOKE: record    off=%0d opcode=%04h record_bytes=%0d lawful=%0d",
                   wo, {pkt_mem[o+wo+1], pkt_mem[o+wo]}, rbv,
                   zhao_abi_pkg::zhao_opcode_record_bytes({pkt_mem[o+wo+1], pkt_mem[o+wo]}));
          // A malformed record_bytes would otherwise spin this loop forever and
          // the bench would hang instead of reporting -- a hung test is neither
          // a pass nor a fail, which is the worst of the three.
          if (rbv < 16) begin
            $display("SMOKE: record    MALFORMED -- record_bytes < 16, walk abandoned");
            wo = ro;
          end else begin
            wo = wo + rbv;
          end
        end
      end
    end    upl_cfg_region_base_i  = UPL_REGION_C;
    upl_cfg_region_bytes_i = UPL_REGION_SZ;
    upl_cfg_arena_base_i   = 64'(UPL_ARENA_C);
    // The arena holds BOTH staged resources now: the MATERIAL_SET and, after
    // it, the MESH_STREAM page. Sized short, MEM.UPLOAD refuses the second
    // request with kUploadSourceOutsideArena (verdict 6) -- which it did, and
    // which is the block correctly refusing a source it was not given.
    upl_cfg_arena_bytes_i  = 32'(UPL_ALL_WORDS * 8);
    upl_cfg_epoch_i        = 16'd9;

    // THE FIXTURE MUST BE IN MEMORY BEFORE THE MACHINE STARTS, and this wait
    // is new because the thing that used to enforce it is gone. Until
    // 2026-09-20 the DRAW was this bench's, gated on `geom_fixture_ready_q`,
    // and that gate happened to hold the frame back until the 92,160-word POST
    // sentinel had been poked. The draw is the command packet's now, so nothing
    // waited, and the snapshot caught a half-filled frame: 44,972 words read as
    // "overwritten by the raster" against the 2,560 pixels it actually wrote.
    // The raster was right and the sentinel was late.
    wait (geom_fixture_ready_q);
    repeat (20) @(posedge gpu_clk);
    rst_n = 1'b1;
    repeat (4) @(posedge gpu_clk);
    reset_released_q = 1'b1;

    // The camera, into the bank GEOM.PROJECT and GEOM.CULL share. Sixteen
    // clocks on the real `proj_cfg_*` port; `geom_camera_ready_q` gates the
    // meshlet draw so no descriptor can be culled against an unwritten bank.
    geom_write_camera();
    geom_camera_ready_q = 1'b1;

    // ---- PART.TABLE IS LOADED FROM A PUBLISHED PAGE NOW (R42) -------------
    // This block used to drive part_tbl_ld_* four times from a task. Entry
    // I33 is CLOSED and the port is gone: the four species-0 descriptors are
    // staged into the arena as a SPECIES_TABLE page (see spt_image above),
    // the command packet PUBLISHES it, and u_part_table_loader reads it back
    // out of the asset window and writes them.
    //
    // THE ORDERING CHECK MOVED WITH THE OWNER. The bench could assert "the
    // table was loaded before any particle was offered" while IT did the
    // loading. It cannot now, because the load happens when MEM.UPLOAD
    // publishes -- so the ordering is asserted at the FOOT of the run instead,
    // against what the particle path actually did with the descriptors. If the
    // load lost its race, contacts_stick and spawn_by_event go to zero and
    // the checks down there fail loudly.
    // ---- PACKET P-TERRAIN: one SubmitTerrainSet, then let the spine run ---
    // A HOST PACKET, which is what the plan lets a harness present. Everything
    // after it is the console's own work: TERRAIN.CMD reads the list over the
    // bridge, emits records; TERRAIN.SEQ looks each up in the directory,
    // claims a slot, issues a load; TERRAIN.LOADQ queues it; TERRAIN.PAGELOADER
    // fetches 21,376 bytes and reports. Not one of those steps is written here.
    terr_cmd_epoch_i       = 32'd9;
    terr_cmd_list_off_i    = 32'(LIST_OFF_C);
    terr_cmd_list_bytes_i  = 32'(LIST_BYTES_C);
    terr_cmd_patch_count_i = 16'(N_TERR_REC);
    terr_cmd_sequence_i    = 32'd1;
    terr_cmd_src_id_i      = 32'd777;
    terr_cmd_valid_i       = 1'b1;
    guard = 0;
    while (!(terr_cmd_valid_i && terr_cmd_ready_o) && (guard < 1000)) begin
      @(posedge gpu_clk);
      guard++;
    end
    @(posedge gpu_clk);
    terr_cmd_valid_i = 1'b0;
    if (guard >= 1000)
      $fatal(1, "SMOKE: TERRAIN.CMD never accepted the command -- j_ready_o stayed low");

    // ---- the geometry job -------------------------------------------------
    // One meshlet, one view. The vertices follow from the always block above.
    // ---- OPEN A RENDER FRAME ----------------------------------------------
    // The triangle door needs one, and nothing in this bench opened one before
    // 2026-09-19 because nothing offered a triangle: `render_tri_valid_i` was
    // tied low here and the shell's rasteriser was never asked to do anything.
    // The protocol is `tests/shell/shell_harness.hpp`'s and is copied from it
    // rather than guessed: grid size, then a ONE-CYCLE `frame_begin` pulse.
    //
    // 4x4 tiles is 64x64 pixels. The triangle offered to GEOM.CLIP sits inside
    // it AND inside the Z60 scissor the core derives from `mode_act_o`, so the
    // clipper passing it and the binner binning it are consistent facts rather
    // than a coincidence of two different rectangles.
    //
    // `fb_writer_i` is 1: RASTER.FBWRITE holds the lease, not DEBUG.FRAMEBLIT.
    // With it at 0 the guard denies every render burst and `render_pixels_o`
    // stays at zero for a reason that has nothing to do with this packet.
    @(posedge gpu_clk);
    fb_writer_i          <= 1'b1;
    render_fb_base_i     <= 27'd0;
    // THE STORED-SURFACE STRIDE, and it is a law, not a choice:
    // spec/video_rules.md 1 -- "Z60 is 384x240 at 768 bytes/row" -- with no
    // caller-supplied stride. It was 128 here (a 64-pixel grid's rows) while
    // nothing read the frame back; POST.COMPOSITE now does, in raster order,
    // at the mode's geometry, and a 128-byte stride under a 768-byte read
    // would read six raster rows as one. The raster's pixels are unchanged:
    // the same 2560 land at their (x, y), one row per 768 bytes.
    render_fb_stride_i   <= 16'd768;      // Z60: 384 px * 2 bytes
    render_fill_word_i   <= 64'hA5A5_A5A5_A5A5_A5A5;
    render_clear_word_i  <= 64'h5A5A_5A5A_5A5A_5A5A;
    render_src_a_i       <= 8'hFF;
    render_texel_rgb_i   <= 24'hFF00FF;
    render_texel_a_i     <= 8'hFF;
    render_texel_idx_i   <= 8'd1;
    render_grid_w_i      <= 6'd4;
    render_grid_h_i      <= 6'd4;
    // THE FRAME REQUEST IS HELD UNTIL IT IS ACCEPTED, NOT PULSED.
    //
    // `render_frame_begin_i` is `zhao_renderer_lease_v2`'s `frame_req_valid_i`,
    // and that is a ready/valid port with a real READY: the lease withholds it
    // while the reset-epoch barrier is closed, while a lease is already live
    // and while neither slot is FREE. A one-cycle pulse offered to it is simply
    // LOST, and nothing retries.
    //
    // This bench pulsed it, and the pulse landed at cycle 12 against a gate
    // that opens at cycle 16 -- measured with a bind-in probe:
    // `frame_req valid first=12 cycles=1, ready first=16, fire first=never`.
    // Four cycles, and the entire render path downstream read as absent. It
    // later began passing by ACCIDENT, when unrelated stimulus added ahead of
    // it pushed the pulse past cycle 16; a gate that passes because of where
    // somebody else's code sits is not a gate.
    //
    // `frame_req_ready_o` is not observable here -- `zhao_shell_top_v2` leaves
    // it an empty pin connection -- so the acceptance is read from
    // `v2_frames_admitted_o`, which is the lease's own count of frames it let
    // through. Holding valid past the fire is harmless: `frame_req_ready_o`
    // requires `!lease_valid_i`, so a live lease cannot admit a second frame.
    @(posedge gpu_clk);
    render_frame_begin_i <= 1'b1;
    guard = 0;
    while ((v2_frames_admitted_o == 0) && (guard < 20000)) begin
      @(posedge gpu_clk);
      guard = guard + 1;
    end
    render_frame_begin_i <= 1'b0;
    if (v2_frames_admitted_o == 0)
      $fatal(1, "SMOKE: the renderer lease admitted no frame in %0d cycles with the request HELD -- granted=%0d refused=%0d clears=%0d. A held request that is never accepted is the barrier or the slot state, not the stimulus",
             guard, v2_leases_granted_o, v2_leases_refused_o,
             v2_clear_handshakes_o);
    render_frame_open_q  <= 1'b1;         // releases the meshlet draw above

    // THE RENDER FRAME IS OPENED, FILLED AND CLOSED IN ONE SEQUENCE, and that
    // is a correction rather than a style choice. The first version opened it
    // here and closed it after the run's two-frame wait -- half a million gpu
    // cycles and two `gpu_tick_o` edges later. Every counter read clean
    // (`drained=1 busy=0 fatal=0 stream_err=0`) and `render_pixels_o` was 0,
    // which reads exactly like "the triangles never reached the binner" and is
    // not that: a render frame does not survive the console's frame boundary.
    // Reading the reassuring zeros as evidence about the door would have sent
    // the next person to re-check wiring that was already correct.
    // WAIT FOR THE MESHLET TO BE RELEASED: GEOM.REPLAY releases it only after
    // its last triangle has been emitted in every view, which is the whole
    // frame's geometry here. Descriptor fetch, footprint, skin, projection and
    // replay all run inside this window.
    guard = 0;
    while ((geom_rp_meshlets_o < N_GEOM_MESHLETS) && (guard < 200000)) begin
      @(posedge gpu_clk);
      guard = guard + 1;
    end
    // THE COMMAND FRONT END, PRINTED BEFORE THE GEOMETRY VERDICT. Everything
    // this watchdog reports is downstream of a decoded packet, so when the
    // packet itself is refused the message describes the wrong subsystem: it
    // says "GEOM.REPLAY released no meshlet" and sends the reader into the
    // replay path. Ruling R63's SetView growth produced exactly that reading.
    // These four counters separate "the front end never delivered" from "the
    // front end delivered and geometry lost it", and they cost one line.
    if (geom_rp_meshlets_o == 0)
      $display("SMOKE: front-end dma_done=%b dma_status=%0d decoder_records=%0d decoder_err=%0d exec_committed=%0d exec_abandoned=%0d",
               dma_done_o, dma_status_o, cmd_commands_o, cmd_decode_error_o,
               cmd_exec_committed_o, cmd_exec_abandoned_o);
    if (geom_rp_meshlets_o == 0)
      $fatal(1, "SMOKE: GEOM.REPLAY released no meshlet in %0d cycles -- replayed %0d of %0d view-triangles, handles=%0d, fetched=%0d meshlet(s), skinned=%0d, landings=%0d, descriptor refused[fmt/crc/gen/vc/tc/resv/bound]=[%0d %0d %0d %0d %0d %0d %0d]; THE DRAW: draws=%0d jobs=%0d masked=%0d empty=%0d hdr_reads=%0d hdr_crc_fail=%0d refused[cull/resident/stale/xform/denied/fmt/crc/resv/layout]=[%0d %0d %0d %0d %0d %0d %0d %0d %0d]; THE LOOM: nodes=%0d streams=%0d pal_writes=%0d pal_dropped=%0d loom_refused[sorted/parent/ovf/kind/shear/framing]=[%0d %0d %0d %0d %0d %0d]; THE UPLOAD: published=%0d status=%0d",
             guard, geom_rp_triangles_out_o, SGF_EXP_REPLAYED, geom_rp_groups_o,
             geom_af_meshlets_fetched_o, geom_skin_vertices_transformed_o, geom_landings_o,
             geom_mf_refused_format_o, geom_mf_refused_crc_o,
             geom_mf_refused_generation_o, geom_mf_refused_vertex_count_o,
             geom_mf_refused_triangle_count_o, geom_mf_refused_reserved_o,
             geom_mf_refused_zero_bound_o,
             geom_dj_draws_o, geom_dj_jobs_o, geom_dj_masked_o, geom_dj_empty_o,
             geom_dj_hdr_reads_o, geom_dj_hdr_crc_fail_o,
             geom_dj_refused_cull_o, geom_dj_refused_resident_o,
             geom_dj_refused_stale_o, geom_dj_refused_xform_o,
             geom_dj_refused_denied_o, geom_dj_refused_format_o,
             geom_dj_refused_crc_o, geom_dj_refused_reserved_o,
             geom_dj_refused_layout_o,
             geom_loom_nodes_o, geom_loom_streams_o,
             geom_dj_pal_writes_o, geom_dj_pal_dropped_o,
             geom_loom_refused_sorted_o, geom_loom_refused_parent_o,
             geom_loom_refused_overflow_o, geom_loom_refused_kind_o,
             geom_loom_refused_shear_o, geom_loom_refused_framing_o,
             upl_published_o, upl_status_o,
             geom_mf_refused_zero_bound_o);
    // Let the last accepted triangles clear GEOM.SETUP and the binner.
    repeat (200) @(posedge gpu_clk);

    @(posedge gpu_clk);
    render_frame_end_i <= 1'b1;
    @(posedge gpu_clk);
    render_frame_end_i <= 1'b0;

    guard = 0;
    while (!render_drain_done_o && (guard < 200000)) begin
      @(posedge gpu_clk);
      guard = guard + 1;
    end
    if (!render_drain_done_o)
      $display("SMOKE: NOTE render_drain_done_o never rose after %0d cycles", guard);
    repeat (4000) @(posedge gpu_clk);

    // (GEOM.GROUP_SEQ's job is no longer injected here: the meshlet dispatcher
    // fork inside the core issues it, from the meshlet the draw above fetched.)

    // ---- the terrain job --------------------------------------------------
    // One subpatch, one view, level 0, top surface, no geomorph. The tess
    // turns this into 81 window vertices (client B's fill) and then its
    // triangles (the replay), so both halves of client B are exercised by ONE
    // job. `sparse_fill_i` stays LOW: the subsystem here carries a dense
    // shell, and a sparse fill against a dense shell is a seal-short fault the
    // terrain differential fires on purpose -- not something a wiring smoke
    // bench should provoke.
    @(posedge gpu_clk);
    terr_job_ox_i        <= 6'd0;
    terr_job_oz_i        <= 6'd0;
    terr_job_level_i     <= 2'd0;
    terr_job_lvl_nz_i    <= 2'd0;
    terr_job_lvl_pz_i    <= 2'd0;
    terr_job_lvl_nx_i    <= 2'd0;
    terr_job_lvl_px_i    <= 2'd0;
    terr_job_morph_i     <= 17'd0;
    terr_job_surface_i   <= 1'b0;
    terr_job_dual_i      <= 1'b0;
    terr_job_src_id_i    <= 16'h5678;
    terr_job_view_mask_i <= 2'b01;
    terr_job_mat_a_i     <= 8'h11;
    terr_job_mat_b_i     <= 8'h22;
    terr_job_weight_i    <= 8'hFF;
    terr_job_valid_i     <= 1'b1;
    guard = 0;
    while (!(terr_job_valid_i && terr_job_ready_o) && (guard < 1000)) begin
      @(posedge gpu_clk);
      guard = guard + 1;
    end
    @(posedge gpu_clk);
    terr_job_valid_i <= 1'b0;
    if (guard >= 1000)
      $fatal(1, "SMOKE: TERRAIN.GROUP_SEQ never accepted a job (job_ready_o stuck low)");

    // ---- run until the shell reaches two frame edges ----------------------
    // Two rather than one: the first proves FRAMECTL runs, the second gives
    // the particle generation a whole tick to drain.
    while ((ticks_seen_q < 2) && (cycles_q < CYCLE_LIMIT)) @(posedge gpu_clk);

    if (ticks_seen_q < 2)
      $fatal(1, "SMOKE: no frame edge after %0d gpu cycles -- the shell never left reset", cycles_q);

    // let the tick's generation drain
    repeat (2000) @(posedge gpu_clk);

    // ---- the post pass: bounded, then every counter and both equalities ----
    guard = 0;
    while ((post_frames_o == 0) && (guard < 3000000)) begin
      @(posedge gpu_clk);
      guard = guard + 1;
    end
    begin
      automatic int unsigned fb_bad = 0, cap_bad = 0, drawn = 0;
      automatic int unsigned first_fb = 0, first_cap = 0;
      for (int unsigned w = 0; w < POST_TB_WORDS; w++) begin
        if (u_geom_sdram.mem[w] !== post_fb_snap[w]) begin
          if (fb_bad == 0) first_fb = w;
          fb_bad++;
        end
        if (u_geom_sdram.mem[POST_ECHO_WBASE + w] !== post_fb_snap[w]) begin
          if (cap_bad == 0) first_cap = w;
          cap_bad++;
        end
        if (post_fb_snap[w] == 16'h0000) drawn++;   // the raster's (black) pixels
      end
      $display("SMOKE: post       frames=%0d passes=%0d src_reads=%0d src_px=%0d line_fill=%0d out=%0d fault=%0d unowned=%0d share_waits=%0d waited=%0d",
               post_frames_o, post_passes_o, post_src_reads_o, post_src_pixels_o,
               post_line_fill_writes_o, post_output_writes_o, post_fault_o,
               post_retire_unowned_o, post_share_contention_o, guard);
      $display("SMOKE: post       lease busy %0d gpu cycles, frame_end to last retired write-back word (contract: ~103,680 work items for Z60; ~461,000 = five passes)",
               post_busy_cycles_q);
      $display("SMOKE: post census sdram_busy=%0d (%0d%%) bursts[e0 rd/wr, other]=[%0d/%0d, %0d] conflicts=%0d refresh_stalls=%0d share_reads_in_flight_clks=%0d fbw_px_stall=%0d src_starved=%0d src_blocked=%0d",
               pc_ctrl_busy_q, (post_busy_cycles_q == 0) ? 0 : (pc_ctrl_busy_q * 100) / post_busy_cycles_q,
               pc_bursts_rd_q, pc_bursts_wr_q, pc_bursts_other_q,
               bank_conflicts_o - pc_conflicts0_q, `PC_SHELL.refresh_stalls_o - pc_refresh0_q,
               pc_share_fill_q, pc_fbw_stall_q, pc_src_starve_q, pc_src_block_q);
      $display("SMOKE: echo       complete=%0d torn=%0d written=%0d dropped=%0d fault=%0d",
               echo_passes_complete_o, echo_passes_torn_o, echo_pixels_written_o,
               echo_pixels_dropped_o, echo_fault_o);
      $display("SMOKE: post       raster-overwritten=%0d | framebuffer after post differs in %0d word(s) | capture differs in %0d word(s)",
               drawn, fb_bad, cap_bad);
      if (post_frames_o != 1 || post_passes_o != 1)
        $fatal(1, "SMOKE: POST.COMPOSITE's lease completed %0d frame(s) / %0d pass(es) after %0d cycles, not 1/1 -- busy=%0d fault=%0d src_px=%0d out=%0d",
               post_frames_o, post_passes_o, guard, post_busy_o, post_fault_o,
               post_src_pixels_o, post_output_writes_o);
      if (post_src_pixels_o != POST_TB_WORDS || post_line_fill_writes_o != POST_TB_WORDS
          || post_output_writes_o != POST_TB_WORDS)
        $fatal(1, "SMOKE: the post pass moved %0d source / %0d filled / %0d output pixels, not %0d each",
               post_src_pixels_o, post_line_fill_writes_o, post_output_writes_o, POST_TB_WORDS);
      if (post_src_reads_o != POST_TB_H * (POST_TB_W / 32))
        $fatal(1, "SMOKE: the post source issued %0d reads, not %0d (one 64-byte read per 32 pixels)",
               post_src_reads_o, POST_TB_H * (POST_TB_W / 32));
      if (post_fault_o || post_retire_unowned_o != 0)
        $fatal(1, "SMOKE: post lease tripwire: fault=%0d retire_unowned=%0d", post_fault_o, post_retire_unowned_o);
      // The raster replaced EXACTLY its own pixels' worth of sentinel: every
      // pixel it wrote is 0x0000 and the sentinel never is.
      if (drawn != render_pixels_o)
        $fatal(1, "SMOKE: %0d word(s) of the sentinel were overwritten by the raster, but it wrote %0d pixel(s) -- the snapshot is not the raster's frame",
               drawn, render_pixels_o);
      if (fb_bad != 0)
        $fatal(1, "SMOKE: the framebuffer after the IDENTITY post pass differs from the raster's frame in %0d word(s), first at word %0d -- the read-back or the write-back moved or changed pixels",
               fb_bad, first_fb);
`ifdef ZHAO_SMOKE_NO_ECHO_ARM
      // THE NEGATIVE CONTROL (R35): the same stimulus with the SetPost's arm
      // OFF must capture NOTHING -- no pass opened, no pixel written. If it did,
      // the capture checked in the armed run would not be evidence of the arm.
      if (echo_passes_complete_o != 0 || echo_passes_torn_o != 0 || echo_pixels_written_o != 0
          || echo_pixels_dropped_o != 0)
        $fatal(1, "SMOKE: DISARMED POST.ECHO still ran: complete=%0d torn=%0d written=%0d dropped=%0d (disarmed is not starved)",
               echo_passes_complete_o, echo_passes_torn_o, echo_pixels_written_o,
               echo_pixels_dropped_o);
      $display("SMOKE: NEGATIVE CONTROL echo disarmed: complete=0 written=0; post busy %0d gpu cycles", post_busy_cycles_q);
`else
      if (echo_passes_complete_o != 1 || echo_passes_torn_o != 0 || echo_pixels_dropped_o != 0
          || echo_fault_o || echo_pixels_written_o != POST_TB_WORDS)
        $fatal(1, "SMOKE: POST.ECHO did not capture the pass whole: complete=%0d torn=%0d written=%0d dropped=%0d fault=%0d",
               echo_passes_complete_o, echo_passes_torn_o, echo_pixels_written_o,
               echo_pixels_dropped_o, echo_fault_o);
      if (cap_bad != 0)
        $fatal(1, "SMOKE: POST.ECHO's capture differs from the frame in %0d word(s), first at word %0d",
               cap_bad, first_cap);
`endif
      if (render_retired_words_o != render_issued_words_o)
        $fatal(1, "SMOKE: after the post pass RASTER.FBWRITE issued %0d words and retired %0d",
               render_issued_words_o, render_retired_words_o);
      if (!render_drained_o)
        $fatal(1, "SMOKE: the render frame (raster + post) never drained -- nothing could publish it");
    end

    // ---- R35/R36: THE LOOK AND THE GRADING TABLE, at POST.COMPOSITE's ports ----
    // The command packet's SetPost and SetGradeTable, through CMD.DMA, CMD.DECODER,
    // CMD.EXEC's EX_POST door and the core, read back at the CONSUMER: every look
    // value on POST.COMPOSITE's own input, the eight product vectors in its own
    // B-curve table, and the echo ARM on the shell's lease -- whose capture above
    // exists only because this arm reached it (the identity look arms nothing).
    begin
      automatic int unsigned pv_bad = 0;
      $display("SMOKE: look      looks=%0d grade_entries=%0d refused=%0d overflow=%0d | gain=%02h bias=%0d/%0d/%0d flash=%04h amt=%0d ink=%04h grade=%b echo_arm=%b",
               cmd_exec_post_looks_o, cmd_exec_grade_entries_o, cmd_exec_post_refused_o,
               cmd_exec_grade_overflow_o, `PC_CORE.u_post_composite.bloom_gain_i,
               $signed(`PC_CORE.u_post_composite.bias_r_i), $signed(`PC_CORE.u_post_composite.bias_g_i),
               $signed(`PC_CORE.u_post_composite.bias_b_i), `PC_CORE.u_post_composite.flash_rgb_i,
               `PC_CORE.u_post_composite.flash_amt_i, `PC_CORE.u_post_composite.ink_rgb_i,
               `PC_CORE.u_post_composite.grade_valid_i, `PC_CORE.post_look_echo_arm_w);
      for (int unsigned k = 0; k < 8; k++) begin
        automatic logic [71:0] want = '0;
        for (int unsigned b = 0; b < 9; b++) want[8*b +: 8] = sm_grade_byte(9*k + b);
        if (`PC_CORE.u_post_composite.pv_b_q[8 + k] !== want) pv_bad++;
      end
      if (cmd_exec_post_looks_o != 32'd1 || cmd_exec_grade_entries_o != 32'd8 ||
          cmd_exec_post_refused_o != 32'd0 || cmd_exec_grade_overflow_o != 32'd0)
        $fatal(1, "SMOKE: CMD.EXEC applied %0d look(s) and %0d table entr(y/ies), refused %0d, overflowed %0d -- expected 1 / 8 / 0 / 0",
               cmd_exec_post_looks_o, cmd_exec_grade_entries_o, cmd_exec_post_refused_o,
               cmd_exec_grade_overflow_o);
      if (`PC_CORE.u_post_composite.bloom_gain_i != SP_GAIN_C ||
          `PC_CORE.u_post_composite.bias_r_i != SP_BIAS_R_C ||
          `PC_CORE.u_post_composite.bias_g_i != SP_BIAS_G_C ||
          `PC_CORE.u_post_composite.bias_b_i != SP_BIAS_B_C ||
          `PC_CORE.u_post_composite.flash_rgb_i != SP_FLASH_C ||
          `PC_CORE.u_post_composite.flash_amt_i != 8'd0 ||
          `PC_CORE.u_post_composite.ink_rgb_i != SP_INK_C ||
          `PC_CORE.u_post_composite.grade_valid_i != 1'b0)
        $fatal(1, "SMOKE: POST.COMPOSITE's look inputs are not the SetPost the packet carried");
      if (pv_bad != 0)
        $fatal(1, "SMOKE: %0d of the 8 SetGradeTable product vectors are not in POST.COMPOSITE's B table at 8..15", pv_bad);
`ifndef ZHAO_SMOKE_NO_ECHO_ARM
      if (`PC_CORE.post_look_echo_arm_w !== 1'b1)
        $fatal(1, "SMOKE: POST.ECHO's arm did not arrive from the SetPost");
`endif
    end


    // ======================================================================
    // THE VERDICT. Each line names the wire it is evidence for.
    // ======================================================================
    $display("SMOKE: cycles=%0d frame_edges=%0d", cycles_q, ticks_seen_q);
    $display("SMOKE: particles  read=%0d written=%0d survivors=%0d updated=%0d",
             part_hps_records_read_o, part_records_written_q,
             part_survivors_o, part_updated_o);
    $display("SMOKE: assetpath  considered=%0d fetched=%0d culled=%0d refused[fmt/crc/gen/vc/tc/resv/bound]=[%0d %0d %0d %0d %0d %0d %0d]",
             geom_mf_meshlets_considered_o, geom_mf_descriptors_fetched_o,
             geom_mf_culled_all_cameras_o,
             geom_mf_refused_format_o, geom_mf_refused_crc_o,
             geom_mf_refused_generation_o, geom_mf_refused_vertex_count_o,
             geom_mf_refused_triangle_count_o, geom_mf_refused_reserved_o,
             geom_mf_refused_zero_bound_o);
    $display("SMOKE: assetpath  meshlets=%0d beats=%0d denied[mf/af]=[%0d %0d] footprint_refused=%0d beat_err[trunc/over/unowned]=[%0d %0d %0d]",
             geom_af_meshlets_fetched_o, geom_af_beats_read_o,
             geom_mf_guard_denied_o, geom_af_guard_denied_o,
             geom_af_refused_footprint_o, geom_af_err_beat_truncated_o,
             geom_af_err_beat_overrun_o, geom_af_err_beat_unowned_o);
    // `contention` READS ZERO HERE FOR A STRUCTURAL REASON, and it is
    // printed rather than asserted because of it. GEOM.MESHFETCH reads a
    // descriptor and only THEN hands the meshlet to GEOM.ASSETFETCH, so with
    // ONE draw in flight the two requesters can never want the shared client
    // in the same cycle. It becomes reachable with a second draw overlapping
    // the first -- MESHFETCH on descriptor N+1 while ASSETFETCH is still
    // walking N's footprint -- which needs entry I38's release owner, so it
    // is not reachable in this composition at all. Quoting the zero as
    // "sharing costs nothing" would be quoting a workload, not a result.
    $display("SMOKE: desc crc   descriptors=%0d fail=%0d framing=%0d",
             geom_mf_crc_descriptors_o, geom_mf_crc_fail_o, geom_mf_crc_framing_o);
    $display("SMOKE: memadapter jobs[a/b]=[%0d %0d] denied=%0d contention=%0d err[short/long/unowned]=[%0d %0d %0d]",
             geom_ma_jobs_a_o, geom_ma_jobs_b_o, geom_ma_denied_o,
             geom_ma_contention_o, geom_ma_err_short_o, geom_ma_err_long_o,
             geom_ma_err_unowned_o);
    $display("SMOKE: assemble   meshlets=%0d triangles=%0d refused[limits/index]=[%0d %0d]",
             geom_asm_meshlets_o, geom_asm_triangles_o,
             geom_asm_refused_limits_o, geom_asm_refused_index_o);
    $display("SMOKE: sdram      model_error=%0d kinds[trcd/trp/trc/refresh/protocol/mrs]=%b",
             geom_model_error, geom_model_err);
    $display("SMOKE: vdecode    records_expected=%0d decoded=%0d refused[reserved/w0/format]=[%0d %0d %0d]",
             N_GEOM_DECODED, geom_vd_vertices_o,
             geom_vd_reserved_nz_count_o, geom_vd_w0_illegal_count_o,
             geom_vd_format_bad_count_o);
    $display("SMOKE: geometry   skinned=%0d vertices_sent=%0d a_grants=%0d landings=%0d groups_opened=%0d groups_sealed=%0d",
             geom_skin_vertices_transformed_o, geom_view_vertices_sent_o,
             proj_a_grants_o, geom_landings_o,
             geom_groups_opened_o, geom_groups_sealed_o);    $display("SMOKE: pose pal   served=%0d bones_written=%0d bone_unset=%0d bone_oob=%0d palettes_decoded=%0d",
             geom_pal_vertices_served_o, geom_pal_bones_written_o,
             geom_pal_bone_unset_o, geom_pal_bone_oob_o,
             geom_pose_palettes_decoded_o);
    $display("SMOKE: rate       skin %0d moves, steady %0d.%02d clk/vertex | landings %0d, steady %0d.%02d clk/landing | replay %0d view-tris, steady %0d.%02d clk/view-tri | frame span %0d clk (first decode -> last release)",
             rt_skin_n_q,
             (rt_skin_n_q > 1) ? (rt_skin_last_q - rt_skin_first_q) / (rt_skin_n_q - 1) : 0,
             (rt_skin_n_q > 1) ? (((rt_skin_last_q - rt_skin_first_q) * 100) / (rt_skin_n_q - 1)) % 100 : 0,
             rt_land_n_q,
             (rt_land_n_q > 1) ? (rt_land_last_q - rt_land_first_q) / (rt_land_n_q - 1) : 0,
             (rt_land_n_q > 1) ? (((rt_land_last_q - rt_land_first_q) * 100) / (rt_land_n_q - 1)) % 100 : 0,
             rt_tri_n_q,
             (rt_tri_n_q > 1) ? (rt_tri_last_q - rt_tri_first_q) / (rt_tri_n_q - 1) : 0,
             (rt_tri_n_q > 1) ? (((rt_tri_last_q - rt_tri_first_q) * 100) / (rt_tri_n_q - 1)) % 100 : 0,
             rt_rel_q - rt_vd_first_q);
    // ---- R47: WHERE THE MESHLET LOOP'S CLOCKS GO ---------------------------
    // The same five timestamps the rate line already holds, printed as a
    // PROFILE rather than as averages. R47 asks for the overlapped loop proved
    // in clocks; this is the measurement of the serial one it is compared to,
    // and it is what says which stage the overlap would have to break.
    // All figures are clocks since the first decoded vertex of the meshlet.
    $display("SMOKE: loop       decode[%0d..%0d] skin[%0d..%0d] land[%0d..%0d] replay[%0d..%0d] release=%0d | total=%0d clk",
             0, rt_skin_last_q - rt_vd_first_q,
             rt_skin_first_q - rt_vd_first_q, rt_skin_last_q - rt_vd_first_q,
             rt_land_first_q - rt_vd_first_q, rt_land_last_q - rt_vd_first_q,
             rt_tri_first_q - rt_vd_first_q, rt_tri_last_q - rt_vd_first_q,
             rt_rel_q - rt_vd_first_q, rt_rel_q - rt_vd_first_q);
    // ---- R57: THE MESHLET LOOP, ACROSS MESHLETS ----------------------------
    // The steady loop PERIOD is what R47 measured at 305 clocks with one
    // meshlet in flight, and it is the number the overlap has to move. It is
    // (last retirement - first retirement) / (retirements - 1), the same
    // steady-interval form as the rate line: the first meshlet's period
    // includes the pipeline's fill, which is latency and not rate.
    //
    // `af_release` is the asset buffer going back, which is what LETS the next
    // meshlet start. Printed beside the first retirement so the two moments can
    // be compared: before R57 they were the same clock, by construction.
    $display("SMOKE: aflat      GEOM.ASSETFETCH %0d line request(s): contention=%0d clk, verdict+SDRAM wait=%0d clk, beats=%0d clk | %0d.%02d clk per 64-byte line",
             af_lines_q, af_offered_q, af_waiting_q, af_beating_q,
             (af_lines_q > 0) ? (af_offered_q + af_waiting_q + af_beating_q) / af_lines_q : 0,
             (af_lines_q > 0) ? (((af_offered_q + af_waiting_q + af_beating_q) * 100) / af_lines_q) % 100 : 0);
    $display("SMOKE: invtx      IN-MESHLET vertex rate %0d.%02d clk/vertex over %0d vertices of meshlet 1 (the frame-wide figure on the rate line spans the gaps BETWEEN meshlets and is a different quantity)",
             (rt_skin_m1_last_q > rt_skin_first_q) ? (rt_skin_m1_last_q - rt_skin_first_q) / (N_GEOM_VERTS - 1) : 0,
             (rt_skin_m1_last_q > rt_skin_first_q) ? (((rt_skin_m1_last_q - rt_skin_first_q) * 100) / (N_GEOM_VERTS - 1)) % 100 : 0,
             N_GEOM_VERTS);
    $display("SMOKE: loop2      meshlets=%0d retire[first=%0d last=%0d] steady_period=%0d.%02d clk | af_release[n=%0d first=%0d last=%0d] | meshlet2_first_decode=%0d",
             rt_rel_n_q,
             rt_rel_first_q - rt_vd_first_q, rt_rel_q - rt_vd_first_q,
             (rt_rel_n_q > 1) ? (rt_rel_q - rt_rel_first_q) / (rt_rel_n_q - 1) : 0,
             (rt_rel_n_q > 1) ? (((rt_rel_q - rt_rel_first_q) * 100) / (rt_rel_n_q - 1)) % 100 : 0,
             rt_afrel_n_q,
             rt_afrel_first_q - rt_vd_first_q, rt_afrel_last_q - rt_vd_first_q,
             rt_vd_m2_q - rt_vd_first_q);
    $display("SMOKE: skin norm  vertices=%0d degenerate=%0d reduced=%0d fork_stall_cycles=%0d",
             geom_sn_vertices_o, geom_sn_degenerate_o, geom_sn_reduced_o,
             geom_sn_fork_stall_o);
    $display("SMOKE: light      lit=%0d seen=%0d bad=%0d last_rgb=%0d/%0d/%0d (reference %0d/%0d/%0d) adapter_refused=%0d cfg_refused=%0d",
             geom_light_vertices_lit_o, geom_lit_seen_q, geom_lit_bad_q,
             geom_light_r_o, geom_light_g_o, geom_light_b_o,
             SGF_EXP_LIT_R, SGF_EXP_LIT_G, SGF_EXP_LIT_B,
             geom_light_adapter_refused_o, geom_light_cfg_refused_o);
    $display("SMOKE: light env  cmd_envs=%0d records=%0d loads=%0d superseded=%0d gen=%0d",
             cmd_exec_envs_o, geom_light_env_records_o, geom_light_env_loads_o,
             geom_light_env_superseded_o, geom_light_cfg_gen_o);
    $display("SMOKE: replay     meshlets=%0d handles=%0d tri_in=%0d tri_out=%0d refused=%0d missed=%0d att_skew=%0d view_bad=%0d",
             geom_rp_meshlets_o, geom_rp_groups_o, geom_rp_triangles_in_o,
             geom_rp_triangles_out_o, geom_rp_refused_o, geom_rp_missed_o,
             geom_rp_att_skew_o, geom_rp_view_bad_o);
    $display("SMOKE: vattr      landings=%0d rows=%0d colours=%0d uv=%0d lq_overflow=%0d index_oob=%0d look_oob=%0d profile_mixed=%0d dq_refused=%0d dq_stray=%0d",
             geom_va_landings_o, geom_va_rows_written_o, geom_va_colours_written_o,
             geom_va_uv_staged_o, geom_va_lq_overflow_o, geom_va_index_oob_o,
             geom_va_look_oob_o, geom_va_profile_mixed_o, geom_va_dq_refused_o,
             geom_va_dq_stray_o);
    $display("SMOKE: r31        holes=%0d groups_poisoned=%0d holes_early=%0d replay_poisoned=%0d",
             geom_holes_o, geom_groups_poisoned_o, geom_holes_early_o, geom_rp_poisoned_o);
    $display("SMOKE: clip       submitted=%0d clipped=%0d culled=%0d setup_submitted=%0d (reference: %0d / %0d / %0d / %0d)",
             geom_clip_submitted_o, geom_clip_clipped_o, geom_clip_culled_o,
             geom_setup_triangles_submitted_o, SGF_EXP_REPLAYED, SGF_EXP_CLIPPED,
             SGF_EXP_CULLED, SGF_EXP_ACCEPTED);
    $display("SMOKE: raster     pixels=%0d bursts=%0d issued=%0d retired=%0d busy=%0d drained=%0d fatal=%0d stream_err=%0d overflow=%0d",
             render_pixels_o, render_bursts_o, render_issued_words_o,
             render_retired_words_o, render_busy_o, render_drained_o,
             render_fatal_o, render_stream_error_o, render_overflow_o);
    $display("SMOKE: renderlease leases_granted=%0d refused=%0d clears=%0d frames_admitted=%0d",
             v2_leases_granted_o, v2_leases_refused_o,
             v2_clear_handshakes_o, v2_frames_admitted_o);
    $display("SMOKE: terrain    tess_vertices=%0d tess_refs=%0d fills_forwarded=%0d refs_forwarded=%0d groups_opened=%0d b_grants=%0d",
             terr_tess_vertices_o, terr_tess_refs_o, terr_fills_forwarded_o,
             terr_refs_forwarded_o, terr_groups_opened_o, proj_b_grants_o);
    $display("SMOKE: projector  a_grants=%0d b_grants=%0d contended=%0d replay_triangles=%0d",
             proj_a_grants_o, proj_b_grants_o, proj_contended_o,
             proj_replay_triangles_o);
    // ---- R21: TERRAIN's LIT NORMALS, measured on the DUT's own edge --------
    $display("SMOKE: terrlight refs_taken=%0d lights=%0d shaded=%0d normals=%0d stale=%0d degenerate=%0d sat=%0d degen_mismatch=%0d",
             terr_light_refs_taken_o, terr_light_emitted_o, terr_light_shaded_o,
             terr_light_normals_o, terr_light_stale_reads_o,
             terr_light_degenerate_count_o, terr_light_base_sat_o,
             terr_light_degen_mismatch_o);
    $display("SMOKE: measure    snapshots=%0d", hist_snapshots_o);
    // Entry I4's two formerly-stuck counters. Both were structurally incapable
    // of moving before owner ruling 2026-09-19; both are asserted below.
    $display("SMOKE: spawn-on-collision: collisions_applied=%0d contacts_stick=%0d spawn_by_event=[%0d %0d %0d %0d]",
             part_collisions_applied_o, part_contacts_stick_o,
             part_spawn_by_event0_o, part_spawn_by_event1_o,
             part_spawn_by_event2_o, part_spawn_by_event3_o);
    $display("SMOKE: children written=%0d, of which at the POST-CONTACT position (y=%0d)=%0d",
             part_children_seen_q, PART_CONTACT_Y, part_children_at_contact_q);
    $display("SMOKE: child accounting: requested=%0d emitted=%0d refused=%0d written=%0d dropped_cap=%0d staging_stalls=%0d max_in_tick=%0d",
             part_children_requested_o, part_children_emitted_o, part_children_refused_o,
             part_children_written_o, part_children_dropped_capacity_o,
             part_staging_stall_cycles_o, part_max_children_in_tick_o);

    if (!part_tick_seen_q)
      $fatal(1, "SMOKE: PART.STATE never went busy -- SHELL.gpu_tick_o does not reach it");
    if (part_records_written_q == 0)
      $fatal(1, "SMOKE: no particle reached the write-back -- the STATE/UPDATE/COLLIDE ring does not carry a beat");

    // ---- THE GENERATION STORE (entry I1): every record crossed the socket ----
    // The records PART.STATE judged came OUT of the played DDR, and the ones it
    // wrote went back IN -- counted by the bench on the socket's beats, against
    // the store's own counters, so neither side can agree with itself.
    $display("SMOKE: particle store: seeds=%0d refused=%0d ticks=%0d dropped=%0d unseeded=%0d rd_bursts=%0d wr_bursts=%0d read=%0d written=%0d ddr_rd_beats=%0d ddr_records_landed=%0d cur_buf=%0d cur_count=%0d c3_bursts=%0d c3_wait=%0d",
             part_hps_seeds_o, part_hps_seeds_refused_o, part_hps_ticks_o,
             part_hps_ticks_dropped_o, part_hps_ticks_unseeded_o,
             part_hps_rd_bursts_o, part_hps_wr_bursts_o,
             part_hps_records_read_o, part_hps_records_written_o,
             part_ddr_rd_beats_q, part_records_written_q,
             part_hps_cur_buf_o, part_hps_cur_count_o,
             terr_hps_c3_bursts_o, terr_hps_c3_wait_cycles_o);
    if ((part_hps_seeds_o != 1) || (part_hps_seeds_refused_o != 0))
      $fatal(1, "SMOKE: the HPS seed was not taken exactly once (seeds=%0d refused=%0d)",
             part_hps_seeds_o, part_hps_seeds_refused_o);
    if (part_hps_ticks_o == 0)
      $fatal(1, "SMOKE: no particle tick completed through the generation store");
    if (part_hps_records_read_o < N_PART_RECORDS)
      $fatal(1, "SMOKE: the store handed PART.STATE %0d records; the seeded generation holds %0d",
             part_hps_records_read_o, N_PART_RECORDS);
    if (part_ddr_rd_beats_q < 2 * part_hps_records_read_o)
      $fatal(1, "SMOKE: PART.STATE judged %0d records but only %0d beats came out of DDR -- a record reached it that the socket never carried",
             part_hps_records_read_o, part_ddr_rd_beats_q);
    if (part_hps_records_written_o != part_records_written_q)
      $fatal(1, "SMOKE: the store took %0d records from PART.STATE but %0d landed in DDR",
             part_hps_records_written_o, part_records_written_q);
    if (terr_hps_c3_bursts_o != part_hps_rd_bursts_o + part_hps_wr_bursts_o)
      $fatal(1, "SMOKE: the arbiter granted client 3 %0d bursts; the store asked for %0d",
             terr_hps_c3_bursts_o, part_hps_rd_bursts_o + part_hps_wr_bursts_o);
    if ((part_hps_ticks_o >= 2) && (part_hps_records_read_o <= N_PART_RECORDS))
      $fatal(1, "SMOKE: %0d store ticks but no generation was read back out of DDR", part_hps_ticks_o);
    // R54 / R55. These two zeros are CLAIMS about this composition -- that the
    // bridge never refuses the store (every burst aligned, the arbiter pulses
    // only from idle) and that no client offers a second different request
    // while one is pending (all four are holders). Both are instruments that
    // have been seen to fire elsewhere, so their zero here means something.
    $display("SMOKE: particle store faults: bridge_errs=%0d ticks_faulted=%0d discarded=%0d ; arbiter pend_dropped=%0d mask=%b",
             part_hps_bridge_errs_o, part_hps_ticks_faulted_o,
             part_hps_records_discarded_o, terr_hps_pend_dropped_o,
             terr_hps_pend_dropped_mask_o);
    if ((part_hps_bridge_errs_o != 0) || (part_hps_ticks_faulted_o != 0) ||
        (part_hps_records_discarded_o != 0))
      $fatal(1, "SMOKE: the bridge refused the particle store (errs=%0d faulted=%0d discarded=%0d) -- the composition's own argument says it cannot",
             part_hps_bridge_errs_o, part_hps_ticks_faulted_o, part_hps_records_discarded_o);
    if ((terr_hps_pend_dropped_o != 0) || (terr_hps_pend_dropped_mask_o != 4'd0))
      $fatal(1, "SMOKE: the HPS arbiter dropped a pending request (count=%0d mask=%b) -- every client on it is a holder",
             terr_hps_pend_dropped_o, terr_hps_pend_dropped_mask_o);
    // ---- THE ASSET PATH FIRST, because everything geometric below it is
    // downstream of a meshlet arriving. Firing the `-BadDescriptor` control
    // with these checks placed AFTER the ones below stopped the run at
    // "GEOM.SKIN transformed nothing" -- true, and three blocks away from
    // the refused descriptor that caused it.
    // ---- THE ASSET PATH, which is what makes the line below mean anything.
    // Each check names the wire it is evidence for, and they are ordered so
    // the FIRST one to fail is the furthest upstream: a dead descriptor read
    // and a dead vertex run look identical from the decoded count alone.
    if (geom_model_error)
      $fatal(1, "SMOKE: the SDRAM model raised a timing/protocol error (kinds=%b) -- every number below describes a machine talking to a memory it is abusing",
             geom_model_err);
    if (geom_mf_descriptors_fetched_o == 0)
      $fatal(1, "SMOKE: GEOM.MESHFETCH fetched no descriptor -- MESHFETCH -> MEM_ADAPTER -> the shell's guard -> SDRAM does not carry a read");
    if ((geom_mf_refused_format_o | geom_mf_refused_crc_o |
         geom_mf_refused_generation_o | geom_mf_refused_vertex_count_o |
         geom_mf_refused_triangle_count_o | geom_mf_refused_reserved_o |
         geom_mf_refused_zero_bound_o) != 0)
      $fatal(1, "SMOKE: GEOM.MESHFETCH refused the fixture descriptor [fmt/crc/gen/vc/tc/resv/bound]=[%0d %0d %0d %0d %0d %0d %0d] -- the bench's descriptor packing is wrong, so nothing below this line means anything",
             geom_mf_refused_format_o, geom_mf_refused_crc_o,
             geom_mf_refused_generation_o, geom_mf_refused_vertex_count_o,
             geom_mf_refused_triangle_count_o, geom_mf_refused_reserved_o,
             geom_mf_refused_zero_bound_o);
    // I37: the CRC WALKER, composed. One descriptor burst per meshlet ended,
    // every CRC matched and every burst was eight beats -- so the verdict the
    // fetcher latched was COMPUTED, and the refusal check above passing is no
    // longer a tie-off agreeing with itself.
    if (geom_mf_crc_descriptors_o != N_GEOM_MESHLETS || geom_mf_crc_fail_o != 0 ||
        geom_mf_crc_framing_o != 0)
      $fatal(1, "SMOKE: the descriptor CRC walker saw descriptors=%0d fail=%0d framing=%0d (want %0d/0/0) -- the fold over the returning beats does not agree with the fixture's CRC word",
             geom_mf_crc_descriptors_o, geom_mf_crc_fail_o, geom_mf_crc_framing_o,
             N_GEOM_MESHLETS);
    // GEOM.CULL is a real block on the real matrix bank now. Under the
    // identity camera the fixture's bound is at the clip origin, so a cull
    // that rejects it is a wiring or configuration fault and not a verdict.
    if (geom_mf_culled_all_cameras_o != 0)
      $fatal(1, "SMOKE: GEOM.CULL rejected %0d meshlet(s) whose bound sits at the clip origin -- the shared matrix bank or the cull service is not carrying what it should",
             geom_mf_culled_all_cameras_o);
    if ((geom_mf_guard_denied_o != 0) || (geom_af_guard_denied_o != 0) ||
        (geom_ma_denied_o != 0))
      $fatal(1, "SMOKE: MEM.GUARD denied a geometry read (mf=%0d af=%0d adapter=%0d) -- the asset pool window or the ENGINE1 client identity is wrong",
             geom_mf_guard_denied_o, geom_af_guard_denied_o, geom_ma_denied_o);
    if (geom_af_refused_footprint_o != 0)
      $fatal(1, "SMOKE: GEOM.ASSETFETCH refused the footprint %0d time(s) -- the descriptor's offsets are misaligned or outside the pool. They are POOL-RELATIVE; adding the base twice is the documented way to get this",
             geom_af_refused_footprint_o);
    // THE FOOTPRINT IN BEATS, from the fixture: whole 64-byte lines of eight
    // beats -- the index run (3 bytes per triangle, line-aligned at
    // GEOM_IX_OFF) and the vertex run (32 bytes per vertex). A COUNT and not a
    // nonzero test -- a block that re-reads a line delivers the right bytes and
    // the wrong number of them, and only the count can see that.
    if (geom_af_beats_read_o != GEOM_FOOTPRINT_BEATS)
      $fatal(1, "SMOKE: GEOM.ASSETFETCH read %0d beats against the %0d this footprint is -- the machine did a different amount of work than the fixture describes",
             geom_af_beats_read_o, GEOM_FOOTPRINT_BEATS);
    if ((geom_af_err_beat_truncated_o | geom_af_err_beat_overrun_o |
         geom_af_err_beat_unowned_o | geom_ma_err_short_o |
         geom_ma_err_long_o | geom_ma_err_unowned_o) != 0)
      $fatal(1, "SMOKE: a beat-protocol fault on the shared geometry client [af trunc/over/unowned=%0d %0d %0d, adapter short/long/unowned=%0d %0d %0d]",
             geom_af_err_beat_truncated_o, geom_af_err_beat_overrun_o,
             geom_af_err_beat_unowned_o, geom_ma_err_short_o,
             geom_ma_err_long_o, geom_ma_err_unowned_o);
    // BOTH requesters used the ONE ENGINE1 client. If either is zero the
    // sharing is not being exercised and `geom_ma_contention_o` below is a
    // number about a machine with one requester in it.
    if ((geom_ma_jobs_a_o == 0) || (geom_ma_jobs_b_o == 0))
      $fatal(1, "SMOKE: GEOM.MEM_ADAPTER served a=%0d b=%0d logical requests -- one of the two fetchers never reached the shared client",
             geom_ma_jobs_a_o, geom_ma_jobs_b_o);
    // ONE draw, so ONE meshlet served -- and (entry I38, CLOSED) it was
    // RELEASED: GEOM.REPLAY's meshlet count moves only on the release it
    // proves, which the replay block above already asserted to be 1.
    if (geom_af_meshlets_fetched_o != N_GEOM_MESHLETS)
      $fatal(1, "SMOKE: GEOM.ASSETFETCH served %0d meshlets for the ONE draw this bench issued",
             geom_af_meshlets_fetched_o);
    // GEOM.ASSEMBLE walked the index run the fetcher served: every triplet of
    // the fixture, none refused (every index is below the vertex count).
    if (geom_asm_triangles_o != SGF_N_TRIS)
      $fatal(1, "SMOKE: GEOM.ASSEMBLE emitted %0d triangles against the %0d the descriptor declares -- the ix_* index service between it and GEOM.ASSETFETCH is not carrying triplets",
             geom_asm_triangles_o, SGF_N_TRIS);
    if ((geom_asm_refused_limits_o != 0) || (geom_asm_refused_index_o != 0))
      $fatal(1, "SMOKE: GEOM.ASSEMBLE refused the fixture meshlet (limits=%0d index=%0d)",
             geom_asm_refused_limits_o, geom_asm_refused_index_o);

    if (geom_skin_vertices_transformed_o == 0)
      $fatal(1, "SMOKE: GEOM.SKIN transformed nothing");
    if (geom_view_vertices_sent_o == 0)
      $fatal(1, "SMOKE: GEOM.GROUP_SEQ sent no vertex to client A -- SKIN -> GROUP_SEQ is dead");
    if (proj_a_grants_o == 0)
      $fatal(1, "SMOKE: the shared projector granted client A zero times -- GROUP_SEQ -> PROJ_SUBSYSTEM is dead");
    if (geom_landings_o == 0)
      $fatal(1, "SMOKE: no landing reached GROUP_SEQ -- PROJ_SUBSYSTEM -> PROJ_LANE -> GROUP_SEQ does not close");
    if (hist_snapshots_o == 0)
      $fatal(1, "SMOKE: MEASURE.HISTOGRAM never snapped -- SHELL.gpu_tick_o does not reach it");

    // ---- CLIENT B, the thing this composition exists to prove -------------
    // Each of these can only move if a beat crossed a wire between two real
    // blocks. `proj_b_grants_o` is the one that matters most: it is the
    // ARBITER inside the shared projector granting its second client, and
    // before TERRAIN.GROUP_SEQ was composed it could not move at all.
    if (terr_tess_vertices_o == 0)
      $fatal(1, "SMOKE: TERRAIN.TESS emitted no window vertex -- GROUP_SEQ -> TESS job port is dead");
    if (terr_fills_forwarded_o == 0)
      $fatal(1, "SMOKE: TERRAIN.GROUP_SEQ forwarded no vertex to client B -- TESS -> GROUP_SEQ -> PROJ_SUBSYSTEM is dead");
    if (proj_b_grants_o == 0)
      $fatal(1, "SMOKE: the shared projector granted client B zero times -- the second client is still not live");
    // R21: and the SAME references that reached the replay shell reached the
    // light lane, were shaded, and came out as base lights. This is the check
    // that says TERRAIN.NORMALS and TERRAIN.SHADE are in the machine rather
    // than merely elaborated: a light can only exist if a world vertex was
    // stored on the projector's fill beat and read back by a reference.
    if (terr_light_emitted_o == 0)
      $fatal(1, "SMOKE: the terrain light lane emitted no base light (refs_taken=%0d shaded=%0d normals=%0d stale=%0d) -- TERRAIN.NORMALS -> TERRAIN.SHADE carried nothing",
             terr_light_refs_taken_o, terr_light_shaded_o, terr_light_normals_o,
             terr_light_stale_reads_o);
    if (terr_light_emitted_o != terr_light_shaded_o)
      $fatal(1, "SMOKE: %0d lights left the lane against %0d triangles shaded -- the lane dropped or duplicated one",
             terr_light_emitted_o, terr_light_shaded_o);
    if (terr_light_degen_mismatch_o != 0)
      $fatal(1, "SMOKE: the shade law and TERRAIN.NORMALS disagreed about degeneracy %0d time(s)",
             terr_light_degen_mismatch_o);
    // WHAT THIS BENCH DOES NOT PROVE ABOUT THE LIGHT, said before somebody
    // quotes `degenerate=128` as a defect or as a pass. Every page in this run
    // fails its CRC by construction, so no page is resident and the lattice
    // TERRAIN.TESS emits is flat zero: all three corners of every triangle are
    // the same point, the cross product is exactly zero, and the LAW's answer
    // to that is degenerate with shade 0. The counters above are therefore
    // evidence that the reference reached the store, the normal and the shade
    // and came back -- not that the shade VALUE is right. The value is proved
    // bit-for-bit against zref in tests/terrain/terrain_lightlane_directed.cpp
    // over a random sub-metre lattice.
    if (terr_light_stale_reads_o != 0)
      $fatal(1, "SMOKE: the light lane refused %0d reference(s) as stale -- the world store and the projector's arena disagree about a generation",
             terr_light_stale_reads_o);
    if (terr_groups_opened_o == 0)
      $fatal(1, "SMOKE: TERRAIN.GROUP_SEQ opened no arena -- the open/gen handshake with the subsystem is dead");
    if (terr_release_unsafe_o != 0)
      $fatal(1, "SMOKE: TERRAIN.GROUP_SEQ released an arena with work outstanding (release_unsafe=%0d)",
             terr_release_unsafe_o);

    // ---- SPAWN ON COLLISION, owner ruling I4 ------------------------------
    // THE ASSERTION IS THE CORRECT BEHAVIOUR: a collision produces a child, and
    // the child is at the RULED position. The counters are kept beside it as a
    // separate POSITIVE CONTROL, because a test that asserts only "the counter
    // moved" is a test of the instrument and a test that asserts the defect
    // stops meaning anything the moment the defect is repaired.
    //
    // Every record this bench offers sits at y = 0 inside a heightfield at
    // +100, so all six contact and all six spawn one child each.
    if (part_children_seen_q == 0)
      $fatal(1, "SMOKE: a collision produced no child -- PART.COLLIDE -> PART.SPAWN does not carry the collision event");
    if (part_children_at_contact_q != part_children_seen_q)
      $fatal(1, "SMOKE: %0d of %0d collision-spawned children are NOT at the post-contact position y=%0d -- ruling I4 S3 says they must be",
             part_children_seen_q - part_children_at_contact_q,
             part_children_seen_q, PART_CONTACT_Y);
    if (part_children_written_o != part_children_seen_q)
      $fatal(1, "SMOKE: PART.STATE wrote %0d children but %0d carry kPartBornThisTick",
             part_children_written_o, part_children_seen_q);

    // THE CONSERVATION LAW, which the counters implied and did not enforce.
    // `reports/DEFECT-PART-STATE-LAST-CHILD-20260919.md` asked for exactly this
    // once the tick boundary was repaired: every child PART.SPAWN got ACCEPTED
    // into staging is written, or dropped at capacity and counted. It used to
    // be a $display of a known defect -- six emitted, five written, and every
    // counter reading zero -- and it is a $fatal now because there is no third
    // outcome left. A child that is REFUSED at the boundary was never accepted
    // and never counted emitted, so it does not appear on either side here; it
    // shows up as `staging_stalls`, printed above.
    //
    // This is the composed statement of the law. The block-level gate, which
    // sweeps the child's arrival cycle instead of taking the one arrival this
    // stimulus happens to produce, is tests/particles/part_state_tick_boundary.cpp.
    if (part_children_emitted_o !=
        (part_children_written_o + part_children_dropped_capacity_o))
      $fatal(1, "SMOKE: child conservation broken: emitted=%0d written=%0d dropped_cap=%0d -- %0d children left the machine with no counter naming them (see reports/DEFECT-PART-STATE-LAST-CHILD-20260919.md)",
             part_children_emitted_o, part_children_written_o,
             part_children_dropped_capacity_o,
             part_children_emitted_o - part_children_written_o -
             part_children_dropped_capacity_o);

    // THE POSITIVE CONTROLS, separate on purpose. These two counters could not
    // move at all before 2026-09-19; `spawn_by_event2_o` is the one the ruling
    // names as its acceptance.
    if (part_spawn_by_event2_o == 0)
      $fatal(1, "SMOKE: part_spawn_by_event2_o (COLLISION) is still zero -- the counter ruling I4 exists to unstick has not moved");
    if (part_collisions_applied_o == 0)
      $fatal(1, "SMOKE: part_collisions_applied_o is still zero -- PART.COLLIDE's collision_events_o does not reach the boundary");
    if (part_contacts_stick_o == 0)
      $fatal(1, "SMOKE: no STICK contact was counted -- the plane does not reach PART.COLLIDE");

    // ======================================================================
    // THE TERRAIN SAMPLE IS THE CORE'S OWN. ENTRY I6, composed 2026-09-19.
    //
    // Every particle now passes through PART.TERRAIN_TAP on its way to the
    // collider, and the first one over an uncached cell sends a real fill
    // through TERRAIN.HEIGHTTAP into the REAL compose cache's read ports. This
    // bench never fills that cache -- its pages fault by design -- so the
    // honest answers are: the fill LANDS (the round trip exists), the tap
    // answers OFF-PATCH (nothing is served), no particle is handed ground, and
    // PART.COLLIDE's "unavailable" is exactly the particles that were not.
    // The values themselves are differenced against the reference in
    // tests/particles/part_terrain_tap_directed.cpp; this is the composition.
    // ======================================================================
    $display("SMOKE: terrain sample  particles=%0d ground=%0d no_ground=%0d missed=%0d faults=%0d fills_landed=%0d | tap answered=%0d off_patch=%0d faults=%0d | collide unavailable=%0d",
             part_ter_particles_o, part_ter_ground_o, part_ter_no_ground_o,
             part_ter_missed_o, part_ter_faults_o, part_ter_fills_landed_o,
             terr_tap_answered_o, terr_tap_off_patch_o, terr_tap_faults_o,
             part_terrain_sample_unavailable_o);
    if (part_ter_particles_o == 0)
      $fatal(1, "SMOKE: no particle passed through PART.TERRAIN_TAP -- it is not on the PART.UPDATE -> PART.COLLIDE path");
    if (part_ter_particles_o != part_ter_ground_o + part_ter_no_ground_o +
                                part_ter_missed_o + part_ter_faults_o)
      $fatal(1, "SMOKE: terrain-sample census does not balance: %0d particles, %0d accounted for",
             part_ter_particles_o, part_ter_ground_o + part_ter_no_ground_o +
                                   part_ter_missed_o + part_ter_faults_o);
    if (part_ter_fills_landed_o == 0)
      $fatal(1, "SMOKE: no terrain cell fill came back -- the TERRAIN.HEIGHTTAP round trip through the compose cache is broken");
    if (terr_tap_off_patch_o == 0)
      $fatal(1, "SMOKE: the tap never answered OFF-PATCH, but this bench serves no patch -- it is reading something that is not the compose cache");
    if (part_ter_ground_o != 0)
      $fatal(1, "SMOKE: %0d particle(s) were handed ground from a compose cache this bench never filled", part_ter_ground_o);
    if (part_terrain_sample_unavailable_o != part_ter_particles_o - part_ter_ground_o)
      $fatal(1, "SMOKE: PART.COLLIDE counted %0d unavailable samples; the tap delivered %0d of %0d",
             part_terrain_sample_unavailable_o, part_ter_ground_o, part_ter_particles_o);
    if (part_ter_faults_o != 0 || terr_tap_faults_o != 0)
      $fatal(1, "SMOKE: terrain-sample FAULTS (sampler %0d, tap %0d) on a bench that places nothing wrong",
             part_ter_faults_o, terr_tap_faults_o);

    // ======================================================================
    // PART.TABLE ANSWERED. ENTRIES I2 AND I3, composed 2026-09-19.
    //
    // The three checks above are ALREADY table evidence and this is the one
    // place to say so plainly, because it is easy to read them as unchanged:
    // the STICK response, the "known" spawn rule and the child count are no
    // longer wires from this bench. Each was written into `zhao_part_table`
    // through `part_tbl_ld_*` and had to come back out through a DIFFERENT
    // consumer's descriptor port to move those counters. A table that
    // elaborated and was never asked reads as all-zero, and all-zero is
    // response IGNORE (no contact counted, no collision event) and known = 0
    // (no child). So `part_contacts_stick_o`, `part_spawn_by_event2_o` and
    // `part_children_seen_q` are each a read that returned a REAL VALUE.
    //
    // THE COLOUR BYTE IS THE EXACT ONE, and it is here because the three above
    // prove a non-zero descriptor arrived without pinning down WHICH. Species 0
    // was loaded with recipe R_COLOUR and curve bucket 0 with an arbitrary
    // 0xA5; PART.UPDATE reads the recipe from the table's update slice, looks
    // the curve up in its curve slice on the SAME cycle, and publishes the
    // result at this module's edge. Two slices, one byte, no adapter in
    // between. An off-by-one in the load word map, a swapped slice or a
    // wrong-cycle read all land somewhere other than 0xA5.
    if (part_colour_beats_q == 0)
      $fatal(1, "SMOKE: PART.UPDATE never raised out_colour_en_o -- the update slice's recipe field did not come back from PART.TABLE (entry I2), so nothing below it means anything");
    if (part_colour_wrong_q != 0)
      $fatal(1, "SMOKE: %0d of %0d retires carried a colour byte other than %02x, or none at all (last seen %02x) -- PART.TABLE's curve slice is not serving what was loaded into it (entry I3)",
             part_colour_wrong_q, part_colour_beats_q, TBL_CURVE_COLOUR, part_colour_last_q);
    if (part_colour_beats_q != part_updated_o)
      $fatal(1, "SMOKE: %0d colour samples against %0d particles updated -- the descriptor did not reach every beat",
             part_colour_beats_q, part_updated_o);
    $display("SMOKE: PART.TABLE -- loads[upd/col/spw/crv]=[%0d %0d %0d %0d] refused=%0d; %0d of %0d retires carried %02x from curve bucket 0; STICK contacts=%0d, collision spawns=%0d.",
             part_tbl_loads_update_o, part_tbl_loads_collide_o,
             part_tbl_loads_spawn_o, part_tbl_loads_curve_o,
             part_tbl_load_refused_o, part_colour_beats_q, part_updated_o,
             part_colour_last_q, part_contacts_stick_o, part_spawn_by_event2_o);
    // ---- R42 / entry I33: THE DESCRIPTORS CAME FROM A PUBLISHED PAGE ------
    // The four words above used to be driven into `part_tbl_ld_*` by this
    // bench. They are a SPECIES_TABLE page now -- staged into the arena,
    // published by the command packet's third PublishResource, read back out
    // of the asset window by `u_part_table_loader` through requester E. The
    // checks above are what prove they ARRIVED (a5 on every retire, six STICK
    // contacts, six collision spawns); these are what prove they arrived THIS
    // WAY and that nothing was refused on the road.
    $display("SMOKE: species    pages=%0d entries=%0d dropped=%0d bad_magic=%0d truncated=%0d denied=%0d adapter_jobs_e=%0d",
             part_tbl_pages_o, part_tbl_entries_o, part_tbl_pages_dropped_o,
             part_tbl_bad_magic_o, part_tbl_truncated_o, part_tbl_denied_o,
             geom_ma_jobs_e_o);
    if (part_tbl_pages_o != 32'd1)
      $fatal(1, "SMOKE: the species loader finished %0d page(s); the packet publishes one", part_tbl_pages_o);
    if (part_tbl_entries_o != 32'(SPT_ENTRIES_C))
      $fatal(1, "SMOKE: the species loader handed over %0d entries against the page's %0d",
             part_tbl_entries_o, SPT_ENTRIES_C);
    if ((part_tbl_bad_magic_o != 0) || (part_tbl_truncated_o != 0) ||
        (part_tbl_denied_o != 0) || (part_tbl_pages_dropped_o != 0))
      $fatal(1, "SMOKE: the species loader refused the page (magic=%0d truncated=%0d denied=%0d dropped=%0d)",
             part_tbl_bad_magic_o, part_tbl_truncated_o, part_tbl_denied_o,
             part_tbl_pages_dropped_o);
    // Three lines per page: the header and two entry lines. Asserted exactly,
    // because a loader that re-read a line would load the same descriptor
    // twice and every check above would still pass.
    if (geom_ma_jobs_e_o != 32'd3)
      $fatal(1, "SMOKE: requester E served %0d reads for a %0d-entry page; the header plus two entry lines is three",
             geom_ma_jobs_e_o, SPT_ENTRIES_C);
    // `part_tbl_load_refused_o` IS NOT CHECKED FOR A FIRING and its zero is not
    // quoted as evidence: at the console's PART_SPECIES_N = 128 a seven-bit
    // index cannot address outside the table, so the refusal is structurally
    // unreachable here. It is reachable and fired at SPECIES_N = 8 in
    // tests/particles/part_table_directed.cpp.

    // SPECIFICITY. Only event 2 fires in this stimulus: the records carry no
    // born flag, the age marker is disabled and the lifetime is unbounded. If
    // another spawn counter moves, the event vector is not what it claims.
    if (part_spawn_by_event0_o != 0 || part_spawn_by_event1_o != 0 ||
        part_spawn_by_event3_o != 0)
      $fatal(1, "SMOKE: a non-collision event fired (%0d/%0d/%0d) -- the event vector crossing PART.COLLIDE is wrong",
             part_spawn_by_event0_o, part_spawn_by_event1_o, part_spawn_by_event3_o);

    // ======================================================================
    // PACKET P-TERRAIN, 2026-09-19: the paging spine must be SEEN TO CARRY
    // DATA, block to block, not merely to elaborate.
    //
    // WAIT FOR THE WORK, DO NOT GUESS AT IT. Each page is 334 bursts and
    // each burst costs the played bridge's latency plus its beats, so a fixed
    // `repeat` would either waste time or -- far worse -- expire early and
    // read counters mid-flight, which looks exactly like a seam that does not
    // carry. The loop watches the block's own completion law instead: every
    // job produces exactly one completion, loaded or faulted.
    // ======================================================================
    guard = 0;
    //
    // THREE TERMS, MATCHING THE COMPLETION LAW THE CHECKS BELOW ASSERT. The
    // wait used `loaded + faulted` while the "one job, one completion" check
    // uses `loaded + faulted + refused`; a refusal IS a completion, so the two
    // disagreed about when the work was over and the wait would sit out its
    // whole 200,000-cycle budget on any run that refuses a job. That costs
    // nothing but time in production -- and it is exactly the case the slot-
    // overflow mutant creates deliberately.
    while (((terr_pl_pages_loaded_o + terr_pl_pages_faulted_o +
             terr_pl_pages_refused_o) < N_TERR_REC) &&
           (guard < 200000)) begin
      @(posedge gpu_clk);
      guard++;
    end
    repeat (64) @(posedge gpu_clk);

    // EVIDENCE BEFORE VERDICT. Printed unconditionally and BEFORE the first
    // check, because a $fatal on check 4 hides the eight numbers that say
    // which seam actually failed -- and those numbers are the reason to run
    // the bench at all.
    $display("SMOKE: TERRAIN spine counters after guard=%0d cycles:", guard);
    $display("SMOKE:   cmd   accepted=%0d refused=%0d verdict=%0d records=%0d crc_fails=%0d bridge_errs=%0d",
             terr_cmd_sets_accepted_o, terr_cmd_sets_refused_o, terr_cmd_done_verdict_o,
             terr_cmd_records_emitted_o, terr_cmd_crc_fails_o, terr_cmd_bridge_errs_o);
    $display("SMOKE:   seq   consumed=%0d issued_patches=%0d claims=%0d/%0d(same) refused=%0d loads=%0d skipped=%0d busy=%0d faults=%0d stray=%0d",
             terr_seq_records_consumed_o, terr_seq_patches_issued_o,
             terr_seq_claims_issued_o, terr_seq_claims_same_o, terr_seq_claims_refused_o,
             terr_seq_loads_issued_o, terr_seq_skipped_not_resident_o,
             terr_seq_busy_o, terr_seq_frame_faults_o, terr_seq_err_stray_ans_o);
    $display("SMOKE:   res   hits=%0d misses=%0d claims=%0d evictions=%0d crc_fail=%0d resident=%0d",
             terr_res_hits_o, terr_res_misses_o, terr_res_claims_o,
             terr_res_evictions_o, terr_res_crc_failures_o, terr_res_resident_o);
    $display("SMOKE:   loadq accepted=%0d issued=%0d high_water=%0d",
             terr_lq_accepted_o, terr_lq_issued_o, terr_lq_high_water_o);
    $display("SMOKE:   pl    loaded=%0d faulted=%0d crc_fails=%0d bytes=%0d guard_denied=%0d bridge_errs=%0d overflow=%0d",
             terr_pl_pages_loaded_o, terr_pl_pages_faulted_o, terr_pl_crc_fails_o,
             terr_pl_load_bytes_o, terr_pl_guard_denied_o, terr_pl_bridge_errs_o,
             terr_pl_slot_overflow_o);
    $display("SMOKE:   pl2   refused=%0d verdict=%0d incomplete=%0d hdr_ident_fails=%0d",
             terr_pl_pages_refused_o, terr_pl_fault_verdict_o, terr_pl_incomplete_o, terr_pl_hdr_ident_fails_o);
    $display("SMOKE:   fault island=%0d ix=%0d iz=%0d src_id=%0d (verdict 1=UNALIGNED 3=SLOT 4=STALE 5=CRC 6=SRC_ARENA 7=UNREACH 8=HDR_IDENT 9=INCOMPLETE)",
             terr_pl_fault_island_o, terr_pl_fault_ix_o, terr_pl_fault_iz_o, terr_pl_fault_src_id_o);
    $display("SMOKE:   arb   c0_bursts=%0d c1_bursts=%0d c1_wait_cycles=%0d c2_bursts=%0d c2_wait_cycles=%0d",
             terr_hps_c0_bursts_o, terr_hps_c1_bursts_o, terr_hps_c1_wait_cycles_o,
             terr_hps_c2_bursts_o, terr_hps_c2_wait_cycles_o);
    $display("SMOKE:   wb    sheets written=%0d refused=%0d faulted=%0d guard_denied=%0d acks_unmatched=%0d overdue=%0d | doorbell starved=%0d ret_overflow=%0d | rdshare jobs A=%0d B=%0d WB=%0d",
             terr_wb_sheets_written_o, terr_wb_sheets_refused_o, terr_wb_sheets_faulted_o,
             terr_wb_guard_denied_o, terr_wb_acks_unmatched_o, terr_wb_acks_overdue_o,
             terr_jdb_starved_cycles_o, terr_jdb_ret_overflow_o,
             terr_rdshare_jobs_a_o, terr_rdshare_jobs_b_o, terr_rdshare_jobs_wb_o);
    // Nothing here can dirty a page, so no writeback job may exist. A job
    // that appeared anyway would be the sequencer evicting a page it was
    // never told was dirty -- and with no grants posted it would sit in the
    // doorbell, visible as starvation, rather than go out with a made-up ticket.
    if ((terr_jdb_starved_cycles_o != 0) || (terr_wb_sheets_written_o != 0) ||
        (terr_wb_sheets_refused_o != 0) || (terr_wb_sheets_faulted_o != 0) ||
        (terr_rdshare_jobs_wb_o != 0) || (terr_hps_c2_bursts_o != 0) ||
        (terr_jdb_ret_overflow_o != 0))
      $fatal(1, "SMOKE: a writeback job appeared with no dirty page in the core (starved=%0d written=%0d refused=%0d faulted=%0d)",
             terr_jdb_starved_cycles_o, terr_wb_sheets_written_o, terr_wb_sheets_refused_o,
             terr_wb_sheets_faulted_o);
    $display("SMOKE:   probe bridge hps_req_cycles=%0d hps_beats_in=%0d | socket contention=%0d retire_unowned=%0d wbeat_unowned=%0d",
             pr_hps_req_cy, pr_hps_beats, terr_bsock_contention_o,
             terr_bsock_retire_unowned_o, terr_bsock_wbeat_unowned_o);
    // THE SOCKET SHARE's TRIPWIRES must read zero on legal traffic. They were
    // FIRED by stimulus in tests/memory/mem_share_wr_directed.cpp; here, with
    // MEM.UPLOAD's publication and TERRAIN.PAGELOADER's pages sharing slot 6
    // for real, a nonzero value is a misattributed credit or a data word from
    // a writer that did not own the channel.
    if ((terr_bsock_retire_unowned_o != 0) || (terr_bsock_wbeat_unowned_o != 0))
      $fatal(1, "SMOKE: the TERRAIN.BUILD socket share tripped: retire_unowned=%0d wbeat_unowned=%0d",
             terr_bsock_retire_unowned_o, terr_bsock_wbeat_unowned_o);
    $display("SMOKE:   bridge bursts served by the played engine=%0d", hps_bursts_served_q);

    // ======================================================================
    // THE POSITIVE CONTROL, AND ITS POLARITY IS INVERTED ON PURPOSE
    // ======================================================================
    // Under -DZHAO_MUT_SLOT_OVERFLOW the DUT is the wrapper mutant and the
    // whole production terrain verdict below is COMPILED OUT, not merely
    // jumped over: the mutation legitimately breaks those checks -- a pool of
    // 512 slots refuses every job the directory places above 511, and an
    // illegal completion is never accepted, so the spine stops. Running them
    // against a deliberately broken machine would be asserting the bug. An
    // `if` plus `\$finish` is not enough here, because Verilator defers the
    // finish to the end of the time step and the straight-line statements
    // after it still execute -- which printed six %Fatal lines under a PASSING
    // exit code the first time this was written. `\ifdef`/`\else` removes
    // them from the build instead.
    //
    // What is asserted instead is the one thing this build exists to show:
    // THE COUNTER MOVES. It reads 0 in production under the identical
    // stimulus, which is the other half of the same measurement.
`ifdef ZHAO_MUT_SLOT_OVERFLOW
    $display("SMOKE: MUTANT zhao_console_core_slot_overflow_mutant -- terr_pl_slot_overflow_o=%0d (pages_refused=%0d verdict=%0d)",
             terr_pl_slot_overflow_o, terr_pl_pages_refused_o, terr_pl_fault_verdict_o);
    if (terr_pl_slot_overflow_o == 0)
      $fatal(1, "MUTANT FAILED: terr_pl_slot_overflow_o stayed 0 with TERR_POOL_SLOTS halved -- the counter could not be made to fire, so its zero in production is not evidence");
    $display("SMOKE: MUTANT PASS -- terr_pl_slot_overflow_o fired %0d time(s). The detector works; production's zero is a measurement.",
             terr_pl_slot_overflow_o);
    $finish;
`else

    // ---- 1. THE COMMAND WAS READ OVER THE BRIDGE -------------------------
    if (terr_cmd_sets_accepted_o != 1)
      $fatal(1, "SMOKE: TERRAIN.CMD accepted %0d sets and refused %0d (verdict %0d, crc seen %08x against %08x) -- the host packet was rejected",
             terr_cmd_sets_accepted_o, terr_cmd_sets_refused_o,
             terr_cmd_done_verdict_o, terr_cmd_done_crc_seen_o, terr_cmd_list_crc_i);
    if (terr_cmd_crc_fails_o != 0)
      $fatal(1, "SMOKE: TERRAIN.CMD folded a different CRC than the bench did -- the played bridge is not returning the bytes that were written");
    if (terr_cmd_bridge_errs_o != 0)
      $fatal(1, "SMOKE: TERRAIN.CMD saw %0d bridge errors -- the played HPS engine is malforming bursts",
             terr_cmd_bridge_errs_o);
    if (terr_cmd_records_emitted_o != N_TERR_REC)
      $fatal(1, "SMOKE: TERRAIN.CMD emitted %0d of %0d records -- it read the list and did not produce it",
             terr_cmd_records_emitted_o, N_TERR_REC);

    // ---- 2. THE CMD -> SEQ SEAM ------------------------------------------
    // The check that separates "CMD produced records" from "SEQ received
    // them". These are two counters in two blocks and only a real handshake
    // on a real wire can make them agree.
    if (terr_seq_records_consumed_o != terr_cmd_records_emitted_o)
      $fatal(1, "SMOKE: TERRAIN.CMD emitted %0d records and TERRAIN.SEQ consumed %0d -- the frame-ring/record seam is not carrying",
             terr_cmd_records_emitted_o, terr_seq_records_consumed_o);

    // ---- 3. THE SEQ <-> RESIDENCY SEAM, BOTH DIRECTIONS ------------------
    // A lookup that was ASKED and a lookup that was ANSWERED are different
    // facts in different blocks. An empty directory must miss both records,
    // and `terr_res_misses_o` moving is the directory saying it saw them.
    if (terr_res_misses_o < N_TERR_REC)
      $fatal(1, "SMOKE: the directory recorded %0d misses for %0d fresh patches -- TERRAIN.SEQ's lookups are not reaching it",
             terr_res_misses_o, N_TERR_REC);
    if (terr_seq_claims_issued_o != terr_res_claims_o)
      $fatal(1, "SMOKE: TERRAIN.SEQ issued %0d claims and the directory recorded %0d -- the claim seam drops or duplicates",
             terr_seq_claims_issued_o, terr_res_claims_o);
    if (terr_seq_claims_issued_o != N_TERR_REC)
      $fatal(1, "SMOKE: %0d claims for %0d distinct patches -- the answer path back into TERRAIN.SEQ is wrong",
             terr_seq_claims_issued_o, N_TERR_REC);

    // THE TRIPWIRE, QUOTED ONLY BECAUSE IT HAS BEEN SEEN TO FIRE ELSEWHERE.
    // `err_stray_ans_o` latches when a directory answer arrives with nothing
    // waiting for one -- the shape of an out-of-order or stale answer, whose
    // every consequence is silent (another island's ground drawn in this
    // island's place, with every count agreeing). It caught a real shim bug in
    // `tb_terrain_world.sv`, so its silence here is a measurement rather than
    // a decoration. This composition drives the directory WITHOUT that shim,
    // which is precisely the arrangement the tripwire is watching.
    if (terr_seq_err_stray_ans_o != 1'b0)
      $fatal(1, "SMOKE: TERRAIN.SEQ latched err_stray_ans -- a directory answer arrived with nothing waiting for it");
    if (terr_seq_frame_faults_o != 0)
      $fatal(1, "SMOKE: TERRAIN.SEQ raised %0d frame faults on an N_TERR_REC-record set against a 1,024-slot directory",
             terr_seq_frame_faults_o);

    // ---- 4. THE SEQ -> LOADQ -> PAGELOADER CHAIN -------------------------
    if (terr_seq_loads_issued_o != N_TERR_REC)
      $fatal(1, "SMOKE: TERRAIN.SEQ issued %0d loads for %0d non-resident patches", terr_seq_loads_issued_o, N_TERR_REC);
    if (terr_lq_accepted_o != terr_seq_loads_issued_o)
      $fatal(1, "SMOKE: TERRAIN.SEQ issued %0d load jobs and TERRAIN.LOADQ accepted %0d -- the queue's job port is not carrying",
             terr_seq_loads_issued_o, terr_lq_accepted_o);
    if (terr_lq_issued_o != terr_lq_accepted_o)
      $fatal(1, "SMOKE: TERRAIN.LOADQ took %0d jobs and handed on %0d -- jobs are stuck in the queue",
             terr_lq_accepted_o, terr_lq_issued_o);

    // THE LOADER ACTUALLY MOVED BYTES. This is the one check that cannot be
    // satisfied by handshakes alone: `load_bytes_o` counts bytes retired off
    // the bridge, so it can only move if the played engine's beats reached the
    // loader through the real `zhao_hps_arbiter`.
    if (terr_pl_load_bytes_o == 0)
      $fatal(1, "SMOKE: TERRAIN.PAGELOADER retired 0 bytes -- the HPS arbiter is not passing client 1's beats");
    if (terr_pl_bridge_errs_o != 0)
      $fatal(1, "SMOKE: TERRAIN.PAGELOADER saw %0d bridge errors", terr_pl_bridge_errs_o);

    // A WELL-FORMED RECORD IS NEVER REFUSED. Every record this bench writes is
    // 64-B aligned, inside the declared staging arena, on the live epoch and
    // names a slot the directory issued, so `pages_refused_o` must be zero.
    // This is the tripwire for the multi-record defect documented at
    // N_TERR_REC. It asserts the CORRECT behaviour -- a well-formed record is
    // never refused -- rather than the defect, so it survived the repair
    // instead of inverting, and it is what went red while the arena writer was
    // aliasing its records on top of each other.
    if (terr_pl_pages_refused_o != 0)
      $fatal(1, "SMOKE: TERRAIN.PAGELOADER REFUSED %0d well-formed job(s), verdict %0d, identity island=%0d ix=%0d iz=%0d src_id=%0d -- a job reached the loader with no payload. See the note at N_TERR_REC.",
             terr_pl_pages_refused_o, terr_pl_fault_verdict_o,
             terr_pl_fault_island_o, terr_pl_fault_ix_o, terr_pl_fault_iz_o,
             terr_pl_fault_src_id_o);

    // AND EVERY CLAIM WAS A FRESH ONE. `claims_same_o` moving on records with
    // distinct {island, ix, iz} is the other face of the same defect: two
    // records that both present a zero key collide in the directory.
    if (terr_seq_claims_same_o != 0)
      $fatal(1, "SMOKE: %0d of %0d claims came back SAME for records with distinct patch coordinates -- two records are presenting one key",
             terr_seq_claims_same_o, terr_seq_claims_issued_o);
    if (terr_pl_guard_denied_o != 0)
      $fatal(1, "SMOKE: TERRAIN.PAGELOADER was denied %0d guard requests -- it is writing outside TERRAIN.PAGE_POOL",
             terr_pl_guard_denied_o);

    // ONE JOB, ONE COMPLETION -- the loader's own stated law.
    // THREE TERMS, NOT TWO, and the third is the interesting one. The block's
    // own comment draws the line: "A refusal here has moved ZERO bytes, which
    // is what separates `pages_refused_o` from `pages_faulted_o`: the first is
    // a bad request, the second is a bad page." A check written as
    // loaded + faulted silently treats a REFUSED job as a lost one, which is
    // the flattering direction for a spine that is dropping work -- and it
    // read exactly that way on the first run here.
    if ((terr_pl_pages_loaded_o + terr_pl_pages_faulted_o + terr_pl_pages_refused_o) != N_TERR_REC)
      $fatal(1, "SMOKE: %0d jobs produced %0d loaded + %0d faulted + %0d refused completions -- 'one job, one completion' is broken (or the wait timed out at guard=%0d)",
             N_TERR_REC, terr_pl_pages_loaded_o, terr_pl_pages_faulted_o,
             terr_pl_pages_refused_o, guard);

    // EVERY COMPLETION REACHED THE DIRECTORY. This is the return leg of the
    // spine and the one a counter inside the loader cannot witness: the
    // directory validates the CRC on every completion it accepts, so a
    // completion that never arrived and one that arrived bad are told apart
    // here and nowhere else.
    if (terr_res_crc_failures_o != N_TERR_REC)
      $fatal(1, "SMOKE: the directory recorded %0d completions for %0d jobs -- TERRAIN.PAGELOADER's fin_* is not reaching TERRAIN.RESIDENCY",
             terr_res_crc_failures_o, N_TERR_REC);

    // THE WIDTH STEP HELD. The pool index is one bit wider than the
    // directory's handle; this composition drives it from a 10-bit wire, so
    // the extra bit must never be set on the way back. NOTE HONESTLY: this
    // counter is STRUCTURALLY unreachable in this arrangement, so its zero is
    // an invariant restated, not a detector that has been seen to fire. It
    // owes a committed mutant, and the core's header says so at its
    // declaration.
    if (terr_pl_slot_overflow_o != 0)
      $fatal(1, "SMOKE: %0d completions carried a pool slot outside the directory's range", terr_pl_slot_overflow_o);

    // ---- 5. WHAT IS *NOT* CLAIMED, stated so nobody reads more in --------
    // `terr_res_resident_o` is ZERO here and that is the CORRECT reading of
    // this composition, for two independent declared reasons:
    //   * the pages this bench plays are ZEROS, so their CRC cannot match the
    //     `expected_page_crc32c` in the record and ruling T7 says a CRC-failed
    //     page is never rendered. The fault is the required behaviour on the
    //     stimulus given, not a defect;
    //   * even a clean page would stop in ST_MIPGEN, because the directory
    //     publishes on TWO completions and the second's producer
    //     (TERRAIN.MIPFEED) is not composed -- see the refusal list in
    //     zhao_console_core.sv.
    // Asserting `resident == 0` would be asserting the gap, so this is a
    // $display. Asserting the CRC fault would be asserting a bug. What IS
    // asserted is the refusal's own law: every faulted page was counted.
    if (terr_pl_pages_faulted_o > 0)
      $display("SMOKE: NOTE %0d page(s) FAULTED as required -- the bench plays a zero page whose CRC cannot match the record's declared expected_page_crc32c, and ruling T7 forbids rendering it. The spine is proven by bytes retired (%0d) and by the record/claim seams above, not by residency.",
               terr_pl_pages_faulted_o, terr_pl_load_bytes_o);
    if (terr_res_resident_o != 0)
      $display("SMOKE: NOTE %0d page(s) reached RESIDENT, which this composition did not expect -- TERRAIN.MIPFEED is not composed, so check what produced the second completion.",
               terr_res_resident_o);

    $display("SMOKE: TERRAIN spine -- %0d records emitted and %0d consumed, %0d claims, %0d loads, %0d bytes retired over %0d played bursts.",
             terr_cmd_records_emitted_o, terr_seq_records_consumed_o,
             terr_res_claims_o, terr_lq_issued_o, terr_pl_load_bytes_o,
             hps_bursts_served_q);
`endif  // ZHAO_MUT_SLOT_OVERFLOW -- end of the production terrain verdict

    // ======================================================================
    // PACKET P-GEOM, 2026-09-19: the three blocks composed this pass must be
    // seen to CARRY DATA, not merely to elaborate.
    //
    // THESE CHECKS HAVE BEEN SEEN TO FIRE. A check nobody has watched fail is
    // a claim, so both were fired deliberately on the tree they guard and then
    // reverted:
    //
    //   * one reserved byte in `vdec_record` set nonzero ->
    //     "records_offered=4 decoded=0 refused[reserved/w0/format]=[4 0 0]"
    //     and the run stopped. GEOM.VDECODE's refusal path is live and it is
    //     the ONLY path to GEOM.SKIN -- the skinner transformed nothing.
    //
    //   * the triangle moved to x ~ 1172 px, outside the Z60 canvas ->
    //     "clip_submitted=16 clipped=16 culled=0 setup_submitted=0" and the
    //     scissor check below fired. That control is worth three things at
    //     once: the check fires, the CLIP -> SETUP seam carries the RIGHT
    //     triangles rather than any traffic (nothing reached setup), and the
    //     scissor really is the mode-derived 384-wide canvas rather than a
    //     constant or a stuck value -- a triangle at 4 px passes and one at
    //     1172 px does not.
    //
    // The shape of these checks is deliberate. Each one names the wire it is
    // evidence for and each is a CONSERVATION statement rather than "a counter
    // moved" -- `n offered == n decoded` cannot be satisfied by a block that
    // passes some records and swallows others, which a nonzero test can.
    // ======================================================================

    // GEOM.VDECODE -> GEOM.SKIN (entry I10's vertex half).
    // The records this bench offers are well-formed format-0, so every one must
    // decode and NONE may be refused. The three refusal counters are read as a
    // group for a specific reason: if the reserved-byte packing in
    // `vdec_record` were wrong, the block would refuse every record, the
    // decoded count would be zero, and "the wire is dead" and "the bench builds
    // a bad record" would look identical from the decoded count alone.
    // ---- and now the same conservation statement as before, end to end.
    // The records are byte-identical to the ones this bench used to hand the
    // decoder directly, so this equality holding means the descriptor read,
    // the cull, the index run, the vertex run and the whole memory path
    // under them delivered what the fixture put in.
`ifdef ZHAO_SMOKE_BAD_VERTEX
    // ---- R31, the DIRECT-polarity control: one refused record ---------------
    // Reaching this line at all is the first half of the evidence: the wait
    // above for GEOM.REPLAY's release is where the pre-fix machine died. The
    // second half is that every counter on the refusal's path moved by exactly
    // what one hole in a two-view, SGF_N_TRIS-triangle batch implies, and that
    // the render frame still closed.
    // R57: the vertex run is SHARED by every meshlet, so the one broken record
    // is refused ONCE PER MESHLET and every count below scales by
    // N_GEOM_MESHLETS -- except `geom_rp_poisoned_o`, which counts TRIANGLES and
    // the partition does not change how many there are.
    if ((geom_vd_reserved_nz_count_o != N_GEOM_MESHLETS) ||
        (geom_vd_vertices_o != N_GEOM_DECODED - N_GEOM_MESHLETS))
      $fatal(1, "SMOKE BAD_VERTEX: GEOM.VDECODE refused %0d / decoded %0d -- want exactly %0d / %0d",
             geom_vd_reserved_nz_count_o, geom_vd_vertices_o, N_GEOM_MESHLETS,
             N_GEOM_DECODED - N_GEOM_MESHLETS);
    if ((geom_holes_o != N_GEOM_MESHLETS) || (geom_groups_poisoned_o != 2 * N_GEOM_MESHLETS) ||
        (geom_holes_early_o != 0))
      $fatal(1, "SMOKE BAD_VERTEX: GEOM.GROUP_SEQ holes=%0d poisoned=%0d orphan=%0d -- want %0d / %0d / 0",
             geom_holes_o, geom_groups_poisoned_o, geom_holes_early_o,
             N_GEOM_MESHLETS, 2 * N_GEOM_MESHLETS);
    if ((geom_rp_meshlets_o != N_GEOM_MESHLETS) || (geom_rp_poisoned_o != SGF_N_TRIS) ||
        (geom_rp_triangles_out_o != 0) || (geom_rp_refused_o != 0) || (geom_rp_missed_o != 0))
      $fatal(1, "SMOKE BAD_VERTEX: GEOM.REPLAY meshlets=%0d poisoned=%0d out=%0d refused=%0d missed=%0d -- want %0d / %0d / 0 / 0 / 0",
             geom_rp_meshlets_o, geom_rp_poisoned_o, geom_rp_triangles_out_o,
             geom_rp_refused_o, geom_rp_missed_o, N_GEOM_MESHLETS, SGF_N_TRIS);
    if ((geom_landings_o != 2 * N_GEOM_MESHLETS * (N_GEOM_VERTS - 1)) ||
        (geom_groups_sealed_o != 2 * N_GEOM_MESHLETS))
      $fatal(1, "SMOKE BAD_VERTEX: landings=%0d sealed=%0d -- want %0d / %0d (a hole never lands; the poisoned arenas still seal)",
             geom_landings_o, geom_groups_sealed_o, 2 * N_GEOM_MESHLETS * (N_GEOM_VERTS - 1),
             2 * N_GEOM_MESHLETS);
    if ((v2_frames_admitted_o != 1) || (render_pixels_o != 0) || (render_issued_words_o != render_retired_words_o))
      $fatal(1, "SMOKE BAD_VERTEX: frames_admitted=%0d pixels=%0d issued=%0d retired=%0d -- want 1 / 0 / equal (the batch drops, the frame completes)",
             v2_frames_admitted_o, render_pixels_o, render_issued_words_o, render_retired_words_o);
    $display("SMOKE: BAD_VERTEX PASS -- one refused record dropped its batch (holes=1, groups_poisoned=2, replay_poisoned=%0d) and the frame completed",
             geom_rp_poisoned_o);
    $finish;
`else
    // Everything from here to the PASS line is the CLEAN fixture's verdict. It is
    // compiled out under ZHAO_SMOKE_BAD_VERTEX rather than jumped over: Verilator
    // defers `\$finish` to the end of the time step, so the straight-line checks
    // after it still run (the ZHAO_MUT_SLOT_OVERFLOW note above says the same).
    if (geom_vd_vertices_o != N_GEOM_DECODED)
      $fatal(1, "SMOKE: GEOM.VDECODE decoded %0d of the %0d records the descriptors declare -- the vertex stream does not cross from GEOM.ASSETFETCH into the decoder",
             geom_vd_vertices_o, N_GEOM_DECODED);
    if ((geom_vd_reserved_nz_count_o != 0) || (geom_vd_w0_illegal_count_o != 0) ||
        (geom_vd_format_bad_count_o != 0))
      $fatal(1, "SMOKE: GEOM.VDECODE refused a well-formed record (reserved=%0d w0=%0d format=%0d) -- the harness's format-0 packing is wrong, so nothing below this line means anything",
             geom_vd_reserved_nz_count_o, geom_vd_w0_illegal_count_o,
             geom_vd_format_bad_count_o);
    if (geom_skin_vertices_transformed_o != geom_vd_vertices_o)
      $fatal(1, "SMOKE: GEOM.SKIN transformed %0d of GEOM.VDECODE's %0d vertices -- the decoder's output does not reach the skinner",
             geom_skin_vertices_transformed_o, geom_vd_vertices_o);

    // GEOM.VDECODE -> GEOM.POSE's PALETTE STORE -> GEOM.SKIN. This is entry
    // I10's other half, the one that was a missing BLOCK rather than missing
    // wiring. The store is now the ONLY path between those two blocks, so the
    // check above cannot pass unless this one did -- and asserting it
    // separately is what tells a dead store apart from a dead decoder.
    if (geom_pal_vertices_served_o != geom_vd_vertices_o)
      $fatal(1, "SMOKE: the pose palette served %0d of GEOM.VDECODE's %0d vertices -- the store between the decoder and the skinner passes no traffic",
             geom_pal_vertices_served_o, geom_vd_vertices_o);
    // NO POSE WAS DECODED (entry I29), so every bone reference must have taken
    // the identity substitution: two per vertex, since these records are rigid
    // and name bone 0 twice. A ZERO here would mean the store handed out
    // whatever its memory happened to hold, which is the one outcome that must
    // never be silent -- and it is also this check's own positive control,
    // because the number can only be right if the substitution really ran.
    if (geom_pal_bone_unset_o != (geom_vd_vertices_o * 2))
      $fatal(1, "SMOKE: the pose palette counted %0d non-resident bone references against %0d expected -- with no decoded pose every reference must be substituted AND counted",
             geom_pal_bone_unset_o, geom_vd_vertices_o * 2);
    if (geom_pal_bone_oob_o != 0)
      $fatal(1, "SMOKE: the pose palette saw %0d out-of-range bone indices -- every record this bench builds names bone 0",
             geom_pal_bone_oob_o);

    // ---- GEOM.SKIN.NORM, the OTHER branch of the palette store's fork -------
    // (core entry I43.) The fork is an AND, so the normal path and the position
    // path accept the SAME beat on the SAME clock. That is the whole claim this
    // check exists to test, and it is testable precisely because the two blocks
    // count independently: `geom_sn_vertices_o` is GEOM.SKIN.NORM's own accept
    // counter and `geom_skin_vertices_transformed_o` is GEOM.SKIN's completion
    // counter, and NO single register enable drives both. A fork that let one
    // branch drop a beat separates them.
    if (geom_sn_vertices_o != geom_vd_vertices_o)
      $fatal(1, "SMOKE: GEOM.SKIN.NORM accepted %0d of the palette store's %0d vertices -- the normal branch of the fork is not carrying the traffic the position branch is",
             geom_sn_vertices_o, geom_vd_vertices_o);

    // AND THE WHOLE RESULT IS COMPUTABLE BY HAND, so this is a DIFFERENTIAL and
    // not an "it moved" check. Every term is a fixture constant:
    //
    //   `vdec_record` packs the normal (nx, ny, nz) = (127, 0, 0) and w0 = 64,
    //   which is the RIGID case (w1 = 64 - w0 = 0, so bone B contributes
    //   nothing). No pose is decoded (entry I29), so the palette substitutes
    //   the IDENTITY for both bones, whose row 0 is [ONE_FX16, 0, 0, 0] with
    //   ONE_FX16 = 65536. So, following `zhao_geom_skin_norm`'s own law:
    //
    //     A_row0 . N = 65536 * 127            = 8,323,072
    //     n[0] = w0 * (A_row0 . N) = 64 * that = 532,676,608 = 2^22 * 127
    //     n[1] = n[2] = 0                       (rows 1 and 2 select ny, nz)
    //     |n|  = isqrt(n[0]^2) = n[0]           (a perfect square, floor-exact)
    //
    // Since owner ruling R31 that root is GEOM.LIGHT's own II8 root, not a
    // service beside this block, so it is no longer tapped here: the lit value
    // below is compared against `zref::creature::lambert_from_world_normal`,
    // whose quotient divides by exactly this |n| -- an approximate or missing
    // root would move every channel off the reference.
    if (geom_sn_degenerate_o != 0)
      $fatal(1, "SMOKE: GEOM.SKIN.NORM called %0d normals degenerate -- the fixture's packed normal is (127,0,0) and blends to a real direction through either bone",
             geom_sn_degenerate_o);
    if (geom_sn_n_x_o != SN_EXPECT_NX)
      $fatal(1, "SMOKE: GEOM.SKIN.NORM's world normal x is %0d, expected %0d (64 * 65536 * 127) -- the two-bone blend or the identity substitution is wrong",
             geom_sn_n_x_o, SN_EXPECT_NX);
    if ((geom_sn_n_y_o != 0) || (geom_sn_n_z_o != 0))
      $fatal(1, "SMOKE: GEOM.SKIN.NORM's world normal is (%0d, %0d, %0d) -- rows 1 and 2 select ny and nz, both zero in the fixture, so a nonzero lane means the row indexing is wrong",
             geom_sn_n_x_o, geom_sn_n_y_o, geom_sn_n_z_o);


    // `geom_sn_reduced_o` READS ZERO AND THE REASON IS STRUCTURAL, not untested.
    // The range reduction fires when max|n| >= 2^30, and the value computed
    // above is 532,676,608 against 1,073,741,824 -- just under half. It cannot
    // be reached from this bench at all while entry I29 is open, because a
    // palette NOTHING FILLS substitutes the identity for every bone, so the
    // blend is always 2^22 times a signed byte and is bounded by 2^22 * 128 =
    // 536,870,912 whatever the record carries. The counter's positive control
    // is `tests/geometry/skin_norm_rtl_directed.cpp`, which drives the block
    // directly with real matrices large enough to trip it. Asserted zero here
    // WITH that reason, rather than quietly not looked at.
    // ---- GEOM.LIGHT (owner ruling R2), the normal's consumer ----------------
    // Every skinned normal is lit, once, and every lit colour is the reference's.
    if ((geom_light_vertices_lit_o != geom_vd_vertices_o) || (geom_lit_seen_q != geom_vd_vertices_o))
      $fatal(1, "SMOKE: GEOM.LIGHT lit %0d (%0d seen leaving) of %0d normals -- the SKIN.NORM -> adapter -> light_stream seam does not carry",
             geom_light_vertices_lit_o, geom_lit_seen_q, geom_vd_vertices_o);
    if (geom_lit_bad_q != 0)
      $fatal(1, "SMOKE: GEOM.LIGHT emitted %0d colour(s) that are not the reference's %0d/%0d/%0d (last r/g/b = %0d/%0d/%0d)",
             geom_lit_bad_q, SGF_EXP_LIT_R, SGF_EXP_LIT_G, SGF_EXP_LIT_B,
             geom_light_r_o, geom_light_g_o, geom_light_b_o);
    // R25: the packet's ONE SetEnvironment was committed, handed over once,
    // taken once, and loaded after the power-on default -- two loads, nothing
    // superseded.
    if ((cmd_exec_envs_o != 32'd1) || (geom_light_env_records_o != 32'd1) ||
        (geom_light_env_loads_o != 32'd2) || (geom_light_env_superseded_o != 32'd0))
      $fatal(1, "SMOKE: SetEnvironment did not reach the bank: cmd_envs=%0d records=%0d loads=%0d superseded=%0d (want 1/1/2/0)",
             cmd_exec_envs_o, geom_light_env_records_o, geom_light_env_loads_o,
             geom_light_env_superseded_o);
    if ((geom_light_adapter_refused_o | geom_light_cfg_refused_o | geom_light_epoch_refusals_o |
         geom_light_seam_mismatch_o | geom_light_tag_mismatch_o | geom_light_root_queue_overflow_o |
         geom_light_degenerate_o | geom_light_nlights_clamped_o | geom_light_rgb_sat_o) != 0)
      $fatal(1, "SMOKE: GEOM.LIGHT faulted: adapter_refused=%0d cfg_refused=%0d epoch=%0d seam=%0d tag=%0d rootq=%0d degen=%0d nl_clamp=%0d rgb_sat=%0d",
             geom_light_adapter_refused_o, geom_light_cfg_refused_o, geom_light_epoch_refusals_o,
             geom_light_seam_mismatch_o, geom_light_tag_mismatch_o, geom_light_root_queue_overflow_o,
             geom_light_degenerate_o, geom_light_nlights_clamped_o, geom_light_rgb_sat_o);

    if (geom_sn_reduced_o != 0)
      $fatal(1, "SMOKE: GEOM.SKIN.NORM range-reduced %0d vertices -- with identity bones the blend is bounded by 2^29 and cannot reach the 2^30 threshold",
             geom_sn_reduced_o);

    // ---- GEOM.REPLAY: the fixture meshlet, replayed into BOTH views --------
    // Every number here is the REFERENCE's (smoke_geom_fixture.svh): the replay
    // takes each of the meshlet's SGF_N_TRIS triangles once and emits it once
    // per visible view, drops nothing (every corner is written and every handle
    // current), and releases the meshlet exactly once.
    if ((geom_rp_meshlets_o != N_GEOM_MESHLETS) || (geom_rp_groups_o != 2 * N_GEOM_MESHLETS) ||
        (geom_rp_triangles_in_o != SGF_N_TRIS) ||
        (geom_rp_triangles_out_o != SGF_EXP_REPLAYED))
      $fatal(1, "SMOKE: GEOM.REPLAY meshlets=%0d handles=%0d tri_in=%0d tri_out=%0d -- the reference wants %0d / %0d / %0d / %0d",
             geom_rp_meshlets_o, geom_rp_groups_o, geom_rp_triangles_in_o,
             geom_rp_triangles_out_o, N_GEOM_MESHLETS, 2 * N_GEOM_MESHLETS,
             SGF_N_TRIS, SGF_EXP_REPLAYED);
    if ((geom_rp_refused_o | geom_rp_missed_o | geom_rp_view_bad_o) != 0)
      $fatal(1, "SMOKE: GEOM.REPLAY dropped or faulted: refused=%0d missed=%0d view_bad=%0d",
             geom_rp_refused_o, geom_rp_missed_o, geom_rp_view_bad_o);
    if (geom_rp_triq_stall_o != 0)
      $fatal(1, "SMOKE: GEOM.REPLAY's descriptor queue backpressured %0d time(s) -- TRIQ_DEPTH no longer holds the two meshlets in flight",
             geom_rp_triq_stall_o);
    // ---- OWNER RULING R57: THE LOOP OVERLAPS, ASSERTED ----------------------
    // The whole ruling in one inequality. Meshlet 2's FIRST vertex record is
    // decoded BEFORE meshlet 1 retires, which is impossible in a serial loop:
    // before R57 GEOM.ASSETFETCH could not take meshlet 2 until GEOM.REPLAY had
    // released its buffer, and the release was the retirement.
    //
    // It is a HARD check and not a printed number because a rate regression is
    // exactly the kind of fault that reads as "a bit slower" in a log nobody
    // diffs. `rt_vd_m2_q` is zero only if meshlet 2 never decoded at all, which
    // the decode census above has already refused.
    if ((rt_rel_n_q != N_GEOM_MESHLETS) || (rt_afrel_n_q != N_GEOM_MESHLETS))
      $fatal(1, "SMOKE R57: %0d retirement(s) and %0d asset release(s) for %0d meshlet(s)",
             rt_rel_n_q, rt_afrel_n_q, N_GEOM_MESHLETS);
    if (rt_afrel_first_q >= rt_rel_first_q)
      $fatal(1, "SMOKE R57: the asset buffer went back at clock %0d and the meshlet retired at %0d -- the release is not EARLY, so GEOM.REPLAY has stopped pipelining and the loop is serial again",
             rt_afrel_first_q - rt_vd_first_q, rt_rel_first_q - rt_vd_first_q);
    // AND THE OTHER HALF IS NOT ASSERTED YET, DELIBERATELY. `meshlet2_first_decode`
    // on the loop2 line is where meshlet 2's vertex phase actually begins, and it
    // is still AFTER meshlet 1 retires. That is not GEOM.REPLAY: the buffer goes
    // back 50 clocks before the retirement and GEOM.ASSETFETCH then spends about
    // 420 clocks FETCHING meshlet 2 (its descriptor, its index line and four
    // vertex lines through MEM.GUARD, the VRAM arbiter and the SDRAM model)
    // against a vertex-plus-replay phase of about 234. The single-bank fetch is
    // the remaining gate and it is now the LARGEST term in the loop.
    // Asserting the overlap here before that bank exists would be asserting the
    // bug's absence in a place that cannot show it; the number is printed and
    // the finding is in the run's FINDINGS-geom4.md.
    // ---- GEOM.VATTR, the store every replayed corner read (entry I46) --------
    // Every landing wrote its row (both views of every vertex), every decoded
    // vertex was staged and lit into both views' arenas, and nothing was
    // dropped, out of the store, mixed or refused. A row NOT written would be
    // read by the replay as the previous occupant's attributes -- no pixel count
    // could see it -- which is why the census is exact rather than nonzero.
    if ((geom_va_landings_o != geom_landings_o) || (geom_va_rows_written_o != geom_landings_o) ||
        (geom_va_uv_staged_o != N_GEOM_DECODED) || (geom_va_colours_written_o != 2 * N_GEOM_DECODED))
      $fatal(1, "SMOKE: GEOM.VATTR landings=%0d rows=%0d uv=%0d colours=%0d -- want %0d / %0d / %0d / %0d",
             geom_va_landings_o, geom_va_rows_written_o, geom_va_uv_staged_o,
             geom_va_colours_written_o, geom_landings_o, geom_landings_o, N_GEOM_DECODED,
             2 * N_GEOM_DECODED);
    if ((geom_va_lq_overflow_o | geom_va_index_oob_o | geom_va_look_oob_o |
         geom_va_profile_mixed_o | geom_va_dq_refused_o | geom_va_dq_stray_o) != 0)
      $fatal(1, "SMOKE: GEOM.VATTR faulted: lq_overflow=%0d index_oob=%0d look_oob=%0d profile_mixed=%0d dq_refused=%0d dq_stray=%0d",
             geom_va_lq_overflow_o, geom_va_index_oob_o, geom_va_look_oob_o,
             geom_va_profile_mixed_o, geom_va_dq_refused_o, geom_va_dq_stray_o);
    // Review of d52ae6c0: in composition decode precedes projection, so no
    // depth result should have had to wait for its u/v, and a clean batch
    // loses no row. (Both are fired in geom_vattr_directed cases I, C, G.)
    $display("SMOKE: vattr join uv_waits=%0d poison=%0d", geom_va_uv_waits_o, geom_va_poison_o);
    if (geom_va_poison_o)
      $fatal(1, "SMOKE: GEOM.VATTR poisoned a clean batch");
    // The attribute store and the arena answered on the SAME clock every time.
    // R31: a clean fixture has no hole, poisons nothing and orphans nothing.
    // `-BadVertex` is the positive control that moves all four.
    if ((geom_holes_o | geom_groups_poisoned_o | geom_holes_early_o | geom_rp_poisoned_o) != 0)
      $fatal(1, "SMOKE: R31 counters moved on a clean fixture: holes=%0d poisoned=%0d orphan=%0d replay_poisoned=%0d",
             geom_holes_o, geom_groups_poisoned_o, geom_holes_early_o, geom_rp_poisoned_o);
    if (geom_rp_att_skew_o != 0)
      $fatal(1, "SMOKE: GEOM.REPLAY saw the attribute store answer out of step with the arena %0d time(s) -- slot 1..6 would belong to a different lookup",
             geom_rp_att_skew_o);

    // ---- GEOM.CLIP -> GEOM.SETUP, EXACTLY the reference's split -------------
    // `clipped` is the behind-the-eye triangle in both views -- the near plane
    // is a whole-primitive rejection -- so this equality holding means the
    // projector's behind verdict crossed the arena and the replay intact.
    if ((geom_clip_submitted_o != SGF_EXP_REPLAYED) ||
        (geom_clip_clipped_o != SGF_EXP_CLIPPED) ||
        (geom_clip_culled_o != SGF_EXP_CULLED))
      $fatal(1, "SMOKE: GEOM.CLIP submitted=%0d clipped=%0d culled=%0d -- the reference wants %0d / %0d / %0d",
             geom_clip_submitted_o, geom_clip_clipped_o, geom_clip_culled_o,
             SGF_EXP_REPLAYED, SGF_EXP_CLIPPED, SGF_EXP_CULLED);
    if (geom_setup_triangles_submitted_o != SGF_EXP_ACCEPTED)
      $fatal(1, "SMOKE: GEOM.SETUP took %0d of the reference's %0d accepted triangles -- the clip->setup seam or the shell's triangle door is not carrying",
             geom_setup_triangles_submitted_o, SGF_EXP_ACCEPTED);

    // A PIXEL NOW TRAVERSES THE RENDER PATH, so this is a CHECK and no longer
    // a note. What stood here said "raster pixels=0 because frames_admitted=0
    // -- the V2 renderer lease is not driven by this bench ... driving the
    // lease belongs with VIDEO.SLOTMGR, which the completion register still
    // lists as not connected", and it was wrong three times over. Kept as
    // quotation rather than deleted, because each error is a different way to
    // be wrong and all three are cheap to make again:
    //
    //   * THE LEASE WAS ALWAYS DRIVEN. `render_frame_begin_i` is this bench's,
    //     and a bind-in probe measured `lease_open` opening at cycle 16 and
    //     staying open for all 500,391 cycles. Nothing about the barrier, the
    //     CDC, the bridge or the manager was missing.
    //   * THE REGISTER DOES NOT LIST VIDEO.SLOTMGR. `completion_register.py`'s
    //     BUILT-BUT-NOT-CONNECTED list has 36 names and no slot manager in it.
    //     A sentence about another tool's output is a claim, and this one had
    //     never been read back.
    //   * WHAT WAS ACTUALLY MISSING WAS TWO WIRES, neither of them a lease:
    //     GEOM.SETUP's `out_area2_o` never reached `tri_area2_i` (so the tile
    //     pipe read PROFILE AREA BAD and SANK all 72 jobs), and
    //     `u_guard_render` was handed the BLITTER's window (so MEM.GUARD
    //     deny-all'd every burst and FBWRITE latched `fatal_error_o`).
    //
    // The moral is the one this file already keeps for the triangle door: a
    // confident sentence naming an absent owner is the most expensive kind of
    // wrong, because it sends the next reader somewhere else entirely.
    //
    // `retired_words_o` is checked and not just `pixels_written_o`: retirement
    // is the VRAM arbiter's credit stream, which is the only thing that means
    // the words LANDED. The block's own issue count says nothing about that.
    if (v2_frames_admitted_o == 0)
      $fatal(1, "SMOKE: no frame was ADMITTED -- the renderer lease granted %0d and refused %0d; the request is held until admission, so this is the lease or the barrier, not the stimulus",
             v2_leases_granted_o, v2_leases_refused_o);
    if (render_pixels_o == 0)
      $fatal(1, "SMOKE: %0d frame(s) were ADMITTED and the shell still rasterised 0 pixels from %0d triangles -- that is a real render-path fault. Check `tri_area2_i` (0 is PROFILE AREA BAD and sinks every job) and the render guard's window before anything else",
             v2_frames_admitted_o, geom_setup_triangles_submitted_o);
    // EXACTLY THE REFERENCE'S PIXELS. `render_pixels_o` counts pixels WRITTEN
    // and the pipeline resolves WHOLE tiles, so the count is the union of the
    // tiles `zref::Binner` names across both views, times 256 -- derived in
    // smoke_geom_fixture_gen.cpp from project_vertex, Clip and Setup, never read
    // off a run. It was 1536 when sixteen copies of one hand-placed triangle
    // came through the bench's door (6 tiles); it is SGF_EXP_PIXELS now because
    // the triangles are the meshlet's own, in two views.
    if (render_pixels_o != SGF_EXP_PIXELS)
      $fatal(1, "SMOKE: the render path wrote %0d pixels and the reference names %0d tiles = %0d pixels",
             render_pixels_o, SGF_EXP_TILES, SGF_EXP_PIXELS);
    if (render_fatal_o)
      $fatal(1, "SMOKE: the render path wrote %0d pixel(s) and latched fatal_error_o -- a guard denial or a broken pixel stream; the frame is unpublishable",
             render_pixels_o);
    if (render_retired_words_o != render_issued_words_o)
      $fatal(1, "SMOKE: the render path issued %0d word(s) and the arbiter retired %0d -- issued-but-not-landed is not a rendered frame",
             render_issued_words_o, render_retired_words_o);
    // ----------------------------------------------------------------------
    // GEOM.ATTRPACK, 2026-09-19. THE PLANES HAVE A PRODUCER.
    //
    // THE RATIO IS THE CHECK, not the absolute numbers. One shared
    // `zhao_geom_attrsetup` core runs three lanes per triangle, so `planes`
    // must be exactly three times `triangles`. A lane that stopped asking --
    // the specific way a time-multiplexed front end goes wrong -- keeps the
    // previous triangle's plane, breaks nothing else, and leaves every
    // handshake, every pixel count and every other counter looking healthy.
    if (geom_attrpack_triangles_o != geom_setup_triangles_submitted_o)
      $fatal(1, "SMOKE: GEOM.ATTRPACK counted %0d triangle(s) and GEOM.SETUP counted %0d -- the fork off GEOM.CLIP is no longer handing both of them one ready",
             geom_attrpack_triangles_o, geom_setup_triangles_submitted_o);
    if (geom_attrpack_planes_o != 3 * geom_attrpack_triangles_o)
      $fatal(1, "SMOKE: GEOM.ATTRPACK packed %0d plane(s) for %0d triangle(s) and three lanes per triangle is the contract -- a lane stopped asking and its plane is the PREVIOUS triangle's",
             geom_attrpack_planes_o, geom_attrpack_triangles_o);
    if (geom_attrpack_triangles_o == 0)
      $fatal(1, "SMOKE: GEOM.ATTRPACK never saw a triangle, so every plane the shell read was its reset value");

    $display("SMOKE: NOTE raster pixels=%0d over %0d burst(s), every issued word retired by the arbiter, from %0d triangle(s) in %0d admitted frame(s). The path is proven END TO END, and the three Packet-D attribute planes now have a PRODUCER: GEOM.ATTRPACK packed %0d plane(s) for %0d triangle(s) -- three lanes through one shared GEOM.ATTRSETUP -- from GEOM.CLIP's own winding-flipped vertex attributes, so depth and both texture coordinates vary across the surface instead of interpolating to zero. What is STILL not proven is the MATERIAL ON A TRIANGLE. The record half is now proven end to end (2026-09-19, cmdmem, rulings R17/R20/R32): a PublishResource in the command packet lands a MATERIAL_SET in RENDER.ASSET_POOL through MEM.UPLOAD on the TERRAIN.BUILD socket, MATERIAL.RESOLVE finds it in the 5f.1 directory, fetches record 0 as ENGINE1 through the adapter's third requester, and answers the uploaded record bit for bit (the `SMOKE: material` line). ONE seam remains, and it is smaller than it was. (1) The resolve REQUEST's two halves are now JOINED BY CONSTRUCTION (2026-09-20, R29): the draw's material_set rides the job's sideband through GEOM.MESHFETCH and GEOM.ASSETFETCH and is offered on the same handshake as the meshlet's own material_id, so the pairing entry I39 refused -- two live wires joined by their timing -- is no longer what anyone would be doing. What is still missing is the issue point and the response join, which is entry I49 and the texture lane's; this bench still drives the request itself, which is why the proof stops at the record. (2) `tri_flat_request_i` wants the binding page's palette slot, palette generation and response class besides, which MATERIAL.RESOLVE's own header says it does not own. So `tri_flat_request_i` still has no producer, sample_count, material_recipe and base_binding_selector are all still zero, and the texture island still samples nothing. Perspective-correct interpolation is composed; the surface it would sample is not bound.",
             render_pixels_o, render_bursts_o,
             geom_setup_triangles_submitted_o, v2_frames_admitted_o,
             geom_attrpack_planes_o, geom_attrpack_triangles_o);

    // ======================================================================
    // PACKET P-SURFACE, 2026-09-19: SURFACE.STAMP <-> SURFACE.SHEET.
    //
    // WAIT FOR THE WORK, DO NOT GUESS AT IT -- the same rule the terrain wait
    // above is written against. A ring of a few dozen texels at SQ_RADIX = 1
    // costs a few thousand cycles plus SURFACE.SHEET's 4,096-cycle clear
    // sweep, and a fixed `repeat` that expired early would read the counters
    // mid-flight and look exactly like a seam that does not carry.
    //
    // THE CONSERVATION CHECK HAS BEEN SEEN TO FIRE, on the tree it guards and
    // then reverted: `zhao_console_core`'s `.wr_handle_i` was driven with a
    // literal instead of `surf_wr_handle`, and the run stopped with
    //
    //     texels[stamp/sheet]=[60 0] ... wr_miss_seen=1
    //     "SURFACE.SHEET wrote 0 texels against SURFACE.STAMP's 60 retired"
    //
    // which is worth three things at once. The check fires. `wr_miss_o` and
    // its sticky latch are live. And -- the reason this control was chosen
    // over an easier one -- SURFACE.STAMP still completed its stamp and still
    // retired all sixty texels while SURFACE.SHEET wrote none, so the two
    // counters are demonstrably NOT two operands moving together. CLAUDE.md's
    // rule is that a detector whose operands share one enable cannot fire;
    // this is the measurement that says these two do not.
    // ======================================================================
    guard = 0;
    while (!surf_done_seen_q && (guard < 200000)) begin
      @(posedge gpu_clk);
      guard++;
    end
    repeat (16) @(posedge gpu_clk);

    // EVIDENCE BEFORE VERDICT.
    $display("SMOKE: surface   stamps=%0d done_seen=%0d rejected_seen=%0d idle[stamp/sheet]=[%0d %0d] guard=%0d",
             surf_stamps_o, surf_done_seen_q, surf_rejected_seen_q,
             surf_stamp_idle_o, surf_sheet_idle_o, guard);
    $display("SMOKE: surface   texels[stamp/sheet]=[%0d %0d] results_taken=%0d occupancy=%b busy=%0d overflow_seen=%0d wr_miss_seen=%0d",
             surf_stamp_texels_touched_o, surf_sheet_texels_touched_o,
             surf_res_records_q, surf_res_occupancy_o, surf_res_busy_o,
             surf_overflow_seen_q, surf_wr_miss_seen_q);

    // 1. THE COMMAND WAS TAKEN AND COMPLETED EXACTLY ONCE. Not ">= 1": a
    //    producer whose `valid` outlives its acceptance re-submits, and this
    //    repository has shipped that defect with byte-identical output. One
    //    command offered, one stamp counted.
    if (surf_stamps_o != 32'd1)
      $fatal(1, "SMOKE: SURFACE.STAMP completed %0d stamps against exactly one command offered -- 0 means the dispatch never crossed, >1 means the bench re-submitted",
             surf_stamps_o);
    if (surf_rejected_seen_q)
      $fatal(1, "SMOKE: SURFACE.STAMP rejected the stamp (residency overflow) -- SURFACE.SHEET refused the ACQUIRE, so the pair's control channel carries but its allocator does not");

    // 2. THE ACQUIRE REACHED THE SHEET AND THE SHEET ANSWERED IT. Occupancy is
    //    SURFACE.SHEET's own state, set only by an ACQUIRE it allocated. The
    //    request channel is internal to the DUT, so a live bit here can only
    //    have come from STAMP's `req_op_o`.
    if (surf_res_occupancy_o == '0)
      $fatal(1, "SMOKE: SURFACE.SHEET holds no resident slot after a completed stamp -- STAMP.req_* -> SHEET.req_* does not carry the ACQUIRE");
    if (surf_overflow_seen_q)
      $fatal(1, "SMOKE: SURFACE.SHEET raised res_overflow on a single stamp against %0d free slots", SURF_SLOTS);

    // 3. THE LOOP CLOSED -- and this is the check the composition exists for.
    //    The two counters are incremented by DIFFERENT BLOCKS on DIFFERENT
    //    events: SURFACE.STAMP counts a texel retiring out of its stage 2,
    //    SURFACE.SHEET counts a write that HIT a resident handle. They cannot
    //    both be moved by one wire, so their agreement is evidence rather than
    //    a tautology -- which is exactly the property CLAUDE.md says a checker
    //    needs, and exactly what a detector whose two operands share one
    //    register enable does not have.
    //
    //    A handle that failed to cross would leave STAMP's count high and
    //    SHEET's at zero (the block drops a non-resident write and says so on
    //    `wr_miss_o`), so this is a conservation statement and not "a counter
    //    moved".
    // ---- R45 / entry I30: THE DISPATCH RESOLVED THE PATCH -------------------
    // The envelope and the policy were BENCH CONSTANTS before this ruling.
    // They are `u_surface_dispatch`'s now, from the stamp's own translation
    // and the live pitch, by the world->patch law the height tap inverts.
    $display("SMOKE: dispatch  stamps=%0d patch=(%0d,%0d) env_x=[%0d,%0d] fx16 (%0d m wide) pitch_refused=%0d env_clamped=%0d",
             surf_disp_dispatched_o, surf_disp_patch_ix_o, surf_disp_patch_iz_o,
             surf_disp_env_x0_o, surf_disp_env_x1_o,
             (surf_disp_env_x1_o - surf_disp_env_x0_o) / SURF_M,
             surf_disp_pitch_refused_o, surf_disp_env_clamped_o);
    if (surf_disp_dispatched_o != surf_stamps_o)
      $fatal(1, "SMOKE: the dispatch placed %0d stamps and SURFACE.STAMP completed %0d -- the envelope and the command are not the same event",
             surf_disp_dispatched_o, surf_stamps_o);
    if (surf_disp_pitch_refused_o != 0)
      $fatal(1, "SMOKE: the dispatch refused the pitch %0d time(s); the staged page's pitch_log2 is outside the ratified set",
             surf_disp_pitch_refused_o);
    if (surf_disp_env_clamped_o != 0)
      $fatal(1, "SMOKE: the dispatch clamped the envelope %0d time(s); the stamp is outside the +-4,096 m domain",
             surf_disp_env_clamped_o);
    if (surf_stamp_texels_touched_o == 0)
      $fatal(1, "SMOKE: SURFACE.STAMP visited no texel -- a ring of outer radius 6 m on a 64 m envelope covers a few dozen, so the command's geometry never reached the walker");
    if (surf_sheet_texels_touched_o != surf_stamp_texels_touched_o)
      $fatal(1, "SMOKE: SURFACE.SHEET wrote %0d texels against SURFACE.STAMP's %0d retired -- the STAMP.wr_* -> SHEET.wr_* seam loses writes (wr_miss_seen=%0d)",
             surf_sheet_texels_touched_o, surf_stamp_texels_touched_o,
             surf_wr_miss_seen_q);
    if (surf_wr_miss_seen_q)
      $fatal(1, "SMOKE: SURFACE.SHEET dropped at least one write as non-resident -- the handle on the write port is not the handle the ACQUIRE made resident");

    // 4. THE RESULT STREAM CARRIES (entry I32's port). One beat per retired
    //    texel by SURFACE.STAMP's own retirement law: stage 2 cannot free
    //    until BOTH the write and the result have been accepted.
    if (surf_res_records_q != surf_stamp_texels_touched_o)
      $fatal(1, "SMOKE: stamp_results delivered %0d beats against %0d texels retired -- the result port and the write port disagree about how many texels this stamp touched",
             surf_res_records_q, surf_stamp_texels_touched_o);

    // 5. THE FIELD ENGINE IS COMPOSED AND REACHABLE (entry I42). One request
    //    was offered at the edge, at a slot nothing was loaded into. All three
    //    of these must hold together: a grant proves the arbiter accepted it, a
    //    noprog proves the `hdr_loaded` interlock answered, and the status
    //    proves the answer came back down the shared response bus. Any one of
    //    them alone could be produced by a stuck signal.
    if (fld_grants_o != 32'd1)
      $fatal(1, "SMOKE: FIELD's arbiter granted %0d requests against exactly one offered -- 0 means the edge client does not reach the engine, >1 means the bench re-offered",
             fld_grants_o);
    if (fld_noprog_o != 32'd1)
      $fatal(1, "SMOKE: FIELD refused %0d runs against one request at an unloaded slot -- 0 means the program store's loaded interlock is not consulted, so a slot with no microcode would have been WALKED",
             fld_noprog_o);
    if (fld_resp_status_o != 8'hF0)
      $fatal(1, "SMOKE: FIELD answered status %02x, not ST_NO_PROGRAM -- the refusal did not reach the shared response bus, and a caller reading only the lanes would have taken four zeroes for a field",
             fld_resp_status_o);
    if (fld_runs_o != 32'd0)
      $fatal(1, "SMOKE: FIELD completed %0d walks against zero loaded programs -- the sequencer ran something, and there is nothing in the store for it to have run",
             fld_runs_o);

    // 6. THE STAMP BRUSH STAYED DISARMED, which is what makes check 3 above
    //    still a measurement of the same stamp. `fld_stamp_slot_valid_i` is low
    //    here, so the adapter must never have started a walk.
    if (surf_fld_stamps_o != 32'd0)
      $fatal(1, "SMOKE: the field stamp adapter started %0d walks with no program resident -- the console's arm gate is not holding, and the stamp consumed field records nobody authored",
             surf_fld_stamps_o);
    if (surf_fld_texels_o != 32'd0)
      $fatal(1, "SMOKE: the field stamp adapter delivered %0d records with no program resident",
             surf_fld_texels_o);

    // ---- MEM.UPLOAD across the TERRAIN.BUILD socket (entry I47) ----------
    // End to end through REAL blocks: the shell's `zhao_hps_arbiter_n` and
    // `zhao_hps_bridge` fetch the staged bytes, the REAL `zhao_mem_guard`
    // admits slot 6 into TERRAIN.PAGE_POOL, `zhao_vram_arbiter` and
    // `zhao_sdram_ctrl` write them and return credits, and only then does the
    // block publish. Every field of the published row is checked against the
    // request that caused it, because a publication naming the wrong base is
    // a well-formed row for the wrong surface.
    $display("SMOKE: ring      slot0_state=%0d ring_writes=%0d pkt_bursts=%0d pkt_len=%0d dma_done=%b dma_status=%0d decoder_records=%0d exec_committed=%0d exec_abandoned=%0d uploads=%0d fence_ok=%b",
             hps_state_i[0], ring_writes_q, sh_pkt_bursts_q, pkt_len_q, dma_done_o, dma_status_o, cmd_commands_o, cmd_exec_committed_o, cmd_exec_abandoned_o, cmd_exec_uploads_o, fence_ok_o);
    $display("SMOKE: upload    done=%0d status=%0d published=%0d rows=%0d bursts=%0d wait=%0d slot=%0d gen=%04x tag=%0d index=%06x base=%08x extent=%0d",
             upl_done_seen_q, upl_status_seen_q, upl_published_o, upl_pub_seen_q,
             sh_bursts_q, upl_hps_wait_o, upl_pub_slot_q, upl_pub_gen_q,
             upl_pub_tag_q, upl_pub_index_q, upl_pub_base_q, upl_pub_extent_q);
    // THREE uploads since 2026-09-19 evening (owner ruling R42): the
    // MATERIAL_SET, the MESH_STREAM page the DrawForm names, and the
    // SPECIES_TABLE page PART.TABLE is loaded from.
    if (upl_done_seen_q != 3 || upl_status_seen_q != 8'd0)
      $fatal(1, "SMOKE: MEM.UPLOAD finished %0d time(s) with last status %0d, expected THREE TIMES with 0 (kUploadOk) -- refused=%032x",
             upl_done_seen_q, upl_status_seen_q, upl_refused_o);
    if (upl_pub_seen_q != 1 || msh_pub_seen_q != 1 || upl_published_o != 16'd3)
      $fatal(1, "SMOKE: MEM.UPLOAD published %0d MATERIAL_SET and %0d MESH_STREAM row(s) (census %0d), expected one of each and three rows in all",
             upl_pub_seen_q, msh_pub_seen_q, upl_published_o);
    if (sh_bursts_q != (UPL_ALL_WORDS / 8))
      $fatal(1, "SMOKE: the shell's bridge served %0d HPS bursts for %0d bytes of upload, expected %0d",
             sh_bursts_q, UPL_ALL_WORDS * 8, UPL_ALL_WORDS / 8);
    if (upl_pub_index_q != UPL_INDEX_C || upl_pub_slot_q != UPL_SLOT_C ||
        upl_pub_gen_q != UPL_GEN_C || upl_pub_tag_q != UPL_KIND_C ||
        upl_pub_base_q != UPL_REGION_C || upl_pub_extent_q != 32'(UPL_WORDS_C * 8))
      $fatal(1, "SMOKE: MEM.UPLOAD published a MATERIAL_SET row that is not the PublishResource's -- 5f.1's directory would name the wrong surface");
    // The MESH_STREAM row is checked the same way and for the same reason: it
    // is what GEOM.DRAWJOB resolves the draw's `form` handle against, so a row
    // naming the wrong base would fetch descriptors out of another resource.
    if (msh_pub_index_q != MSH_INDEX_C || msh_pub_slot_q != MSH_SLOT_C ||
        msh_pub_gen_q != MSH_GEN_C || msh_pub_tag_q != MSH_KIND_C ||
        msh_pub_base_q != MSH_DST_C || msh_pub_extent_q != 32'(MSH_WORDS_C * 8))
      $fatal(1, "SMOKE: MEM.UPLOAD published a MESH_STREAM row that is not the PublishResource's -- index=%06x slot=%0d gen=%04x tag=%0d base=%08x extent=%0d",
             msh_pub_index_q, msh_pub_slot_q, msh_pub_gen_q, msh_pub_tag_q,
             msh_pub_base_q, msh_pub_extent_q);
    // ---- THE DRAW, end to end (entries I36/I41/I39/I24; owner ruling R29) --
    // Counters, not a picture. The frame above could look identical while the
    // machine did the work twice, resolved the wrong page, or drew a meshlet
    // whose transform nobody wrote -- this repository has a chapter about each.
    $display("SMOKE: draw      draws=%0d jobs=%0d masked=%0d empty=%0d hdr[reads/crc_fail/framing]=[%0d %0d %0d] refused[cull/resident/stale/xform/denied/fmt/crc/resv/layout]=[%0d %0d %0d %0d %0d %0d %0d %0d %0d] adapterD=%0d",
             geom_dj_draws_o, geom_dj_jobs_o, geom_dj_masked_o, geom_dj_empty_o,
             geom_dj_hdr_reads_o, geom_dj_hdr_crc_fail_o, geom_dj_hdr_framing_o,
             geom_dj_refused_cull_o, geom_dj_refused_resident_o, geom_dj_refused_stale_o,
             geom_dj_refused_xform_o, geom_dj_refused_denied_o, geom_dj_refused_format_o,
             geom_dj_refused_crc_o, geom_dj_refused_reserved_o, geom_dj_refused_layout_o,
             geom_ma_jobs_d_o);
    $display("SMOKE: loom      nodes=%0d streams=%0d pal_writes=%0d pal_dropped=%0d refused[sorted/parent/ovf/kind/shear/framing]=[%0d %0d %0d %0d %0d %0d]",
             geom_loom_nodes_o, geom_loom_streams_o, geom_dj_pal_writes_o,
             geom_dj_pal_dropped_o, geom_loom_refused_sorted_o, geom_loom_refused_parent_o,
             geom_loom_refused_overflow_o, geom_loom_refused_kind_o,
             geom_loom_refused_shear_o, geom_loom_refused_framing_o);
    if (geom_loom_streams_o != 32'd1 || geom_loom_nodes_o != 32'd1)
      $fatal(1, "SMOKE: GEOM.LOOM composed %0d node(s) in %0d stream(s), expected 1 and 1 -- the ARM stream this bench plays is one ROOT node",
             geom_loom_nodes_o, geom_loom_streams_o);
    if ((geom_loom_refused_sorted_o | geom_loom_refused_parent_o |
         geom_loom_refused_overflow_o | geom_loom_refused_kind_o |
         geom_loom_refused_shear_o | geom_loom_refused_framing_o) != 32'd0)
      $fatal(1, "SMOKE: GEOM.LOOM refused this bench's stream -- the palette row the draw names was never written");
    if (geom_dj_pal_writes_o != 32'd1 || geom_dj_pal_dropped_o != 32'd0)
      $fatal(1, "SMOKE: the instance transform palette took %0d write(s) and dropped %0d -- the draw's xform[12] comes from a row GEOM.LOOM composed, or it comes from nowhere",
             geom_dj_pal_writes_o, geom_dj_pal_dropped_o);
    if (geom_dj_draws_o != 32'd1)
      $fatal(1, "SMOKE: GEOM.DRAWJOB accepted %0d draw(s), expected the packet's ONE DrawForm",
             geom_dj_draws_o);
    // EXACTLY ONE JOB PER MESHLET the header declares, and no more. A machine
    // that re-offered a job would raster the identical triangles and this is
    // the only line that would notice.
    if (geom_dj_jobs_o != 32'(N_GEOM_MESHLETS))
      $fatal(1, "SMOKE: GEOM.DRAWJOB emitted %0d job(s) for a %0d-meshlet stream",
             geom_dj_jobs_o, N_GEOM_MESHLETS);
    if ((geom_dj_refused_cull_o | geom_dj_refused_resident_o | geom_dj_refused_stale_o |
         geom_dj_refused_xform_o | geom_dj_refused_denied_o | geom_dj_refused_format_o |
         geom_dj_refused_crc_o | geom_dj_refused_reserved_o | geom_dj_refused_layout_o |
         geom_dj_masked_o | geom_dj_empty_o) != 32'd0)
      $fatal(1, "SMOKE: GEOM.DRAWJOB refused or skipped the packet's draw -- see the line above for which of the nine reasons");
    // The header was READ, once, through the adapter's fourth requester, and
    // its CRC verdict came from the same walker the descriptor's does.
    if (geom_dj_hdr_reads_o != 32'd1 || geom_dj_hdr_crc_fail_o != 32'd0 ||
        geom_dj_hdr_framing_o != 32'd0)
      $fatal(1, "SMOKE: the MESH_STREAM header was read %0d time(s), crc_fail=%0d framing=%0d",
             geom_dj_hdr_reads_o, geom_dj_hdr_crc_fail_o, geom_dj_hdr_framing_o);
    if (geom_ma_jobs_d_o != 32'd1)
      $fatal(1, "SMOKE: the geometry adapter served %0d request(s) on requester D, expected the one header read",
             geom_ma_jobs_d_o);
    // I24: the cull mode reached GEOM.CLIP on the TRIANGLE'S OWN raster word.
    // The packet's DrawForm carries flags 0 -- cull NONE, double-sided -- so no
    // triangle may be culled by winding. It is the one end of that wire this
    // bench can see; the other end (the word's composition from the draw's
    // flags) is differenced against `zref::raster_state` in the DRAWJOB test.
    if (geom_clip_culled_o != 32'd0)
      $fatal(1, "SMOKE: GEOM.CLIP culled %0d triangle(s) under a draw whose cull mode is NONE -- the raster word that reached it is not this draw's",
             geom_clip_culled_o);
    // THE COMMAND PATH, end to end (R17): the packet was fetched over the
    // shell's bridge, walked by both consumers, committed, and CMD.EXEC handed
    // exactly one request to MEM.UPLOAD.
    $display("SMOKE: command   pkt_bursts=%0d decoder_records=%0d exec_committed=%0d exec_abandoned=%0d uploads=%0d overflow=%0d",
             sh_pkt_bursts_q, cmd_commands_o, cmd_exec_committed_o, cmd_exec_abandoned_o,
             cmd_exec_uploads_o, cmd_exec_upload_overflow_o);
    if (cmd_commands_o != 32'd13 || cmd_exec_committed_o != 32'd1 || cmd_exec_uploads_o != 32'd3)
      $fatal(1, "SMOKE: the command packet did not travel: %0d records walked, %0d committed, %0d uploads handed to MEM.UPLOAD (expected 13, 1, 3)",
             cmd_commands_o, cmd_exec_committed_o, cmd_exec_uploads_o);
    // ---- R41 / entry I7: THE POPULATION DESCRIPTOR CAME FROM THE PACKET ----
    // Every one of these was a BOARD PIN before this ruling. The chain is
    // decoder -> CMD.EXEC -> PART.POP -> PART.COLLIDE / PART.TERRAIN_TAP and
    // the generation store, and the handle is the cheapest proof it is the
    // record's value rather than a power-on state: nothing else in this bench
    // writes 0x0051C0DE.
    $display("SMOKE: population handle=%08x taken=%0d seeds=%0d lowered=%0d refused[normal/count/flags]=[%0d %0d %0d]",
             part_pop_handle_o, part_pop_taken_o, part_pop_seeds_issued_o,
             cmd_exec_pops_o, part_pop_refused_normal_o, part_pop_refused_count_o,
             part_pop_refused_flags_o);
    if (cmd_exec_pops_o != 32'd1)
      $fatal(1, "SMOKE: CMD.EXEC lowered %0d SetPopulation records; the packet carries one", cmd_exec_pops_o);
    if (part_pop_taken_o != 32'd1)
      $fatal(1, "SMOKE: PART.POP took %0d descriptors; the packet carries one", part_pop_taken_o);
    if (part_pop_handle_o != SMK_POP_HANDLE_C)
      $fatal(1, "SMOKE: PART.POP holds population %08x, the packet named %08x", part_pop_handle_o, SMK_POP_HANDLE_C);
    if ((part_pop_refused_normal_o != 0) || (part_pop_refused_count_o != 0) ||
        (part_pop_refused_flags_o != 0))
      $fatal(1, "SMOKE: PART.POP refused the packet's descriptor (normal=%0d count=%0d flags=%0d)",
             part_pop_refused_normal_o, part_pop_refused_count_o, part_pop_refused_flags_o);
    if (part_pop_seeds_issued_o != 32'd1)
      $fatal(1, "SMOKE: the store took %0d seeds from PART.POP; SetPopulation asked for one", part_pop_seeds_issued_o);
    // ---- MEASURE.TOKENS (R18/R33): the CEILING and the REQUEST ----------
    // Counts off the wire, unchanged. View 0 sent no SetView, so it keeps the
    // contract's ceiling; view 1 asked for MORE geometry than its ceiling (cut
    // to it, and counted once) and LESS fragment (its allowance drops to it).
    $display("SMOKE: tokens    contracts=%0d views=%0d avail g0=%0d g1=%0d f0=%0d f1=%0d shared=%0d clamped=%0d",
             cmd_exec_contracts_o, cmd_exec_views_o, tok_avail_geom0_o, tok_avail_geom1_o,
             tok_avail_frag0_o, tok_avail_frag1_o, tok_avail_shared_o, tok_vreq_clamped_o);
    if (cmd_exec_contracts_o != 32'd1 || cmd_exec_views_o != 32'd1)
      $fatal(1, "SMOKE: CMD.EXEC applied %0d contract(s) and %0d view(s), expected 1 and 1",
             cmd_exec_contracts_o, cmd_exec_views_o);
    if (tok_avail_geom0_o != TOK_G0_C || tok_avail_frag0_o != TOK_F0_C || tok_avail_shared_o != TOK_SH_C)
      $fatal(1, "SMOKE: view 0 / shared pools %0d/%0d/%0d are not the contract's ceiling %0d/%0d/%0d",
             tok_avail_geom0_o, tok_avail_frag0_o, tok_avail_shared_o, TOK_G0_C, TOK_F0_C, TOK_SH_C);
    if (tok_avail_geom1_o != TOK_G1_C || tok_avail_frag1_o != TOK_REQ_F1_C || tok_vreq_clamped_o != 32'd1)
      $fatal(1, "SMOKE: view 1's request did not clamp: geom %0d (want the ceiling %0d), frag %0d (want the request %0d), clamps %0d (want 1)",
             tok_avail_geom1_o, TOK_G1_C, tok_avail_frag1_o, TOK_REQ_F1_C, tok_vreq_clamped_o);
    if (upl_refused_o != '0)
      $fatal(1, "SMOKE: MEM.UPLOAD's refusal census moved (%032x) on a legal upload", upl_refused_o);
    if (shell_err_wfifo_o || shell_err_route_o)
      $fatal(1, "SMOKE: the shell's write-queue (%b) or routing (%b) tripwire fired with the slot-6 socket live",
             shell_err_wfifo_o, shell_err_route_o);

    // ---- MATERIAL.RESOLVE (R20 + R32): found, fetched, resolved ------------
    // The published MATERIAL_SET is found (not NOT_RESIDENT): the directory is
    // MEM.UPLOAD's publication. Its record fetch goes to the REAL MEM.GUARD as
    // ENGINE1 through requester C, and is ADMITTED, because since owner ruling
    // R32 MEM.UPLOAD writes the published region inside RENDER.ASSET_POOL.
    // The answer is a first-touch MISS carrying the record, and the record is
    // the one the PublishResource uploaded, bit for bit -- bytes that crossed
    // the HPS bridge, the slot-6 write queue and VRAM, and came back as ENGINE1.
    // (The denied path, kFetchDenied, is fired by the block's own bench, R20.)
    $display("SMOKE: material  responses=%0d status=%0d hits=%0d misses=%0d not_resident=%0d fetch_denied=%0d adapter_jobs_c=%0d adapter_denied=%0d",
             mat_rsp_seen_q, mat_status_seen_q, mat_hits_o, mat_misses_o, mat_not_resident_o,
             mat_fetch_denied_o, geom_ma_jobs_c_o, geom_ma_denied_o);
    if (mat_rsp_seen_q != 1)
      $fatal(1, "SMOKE: MATERIAL.RESOLVE answered %0d time(s) to one request -- expected exactly one answer", mat_rsp_seen_q);
    if (mat_not_resident_o != 0)
      $fatal(1, "SMOKE: the published MATERIAL_SET was NOT RESIDENT -- MEM.UPLOAD's publication did not reach the directory");
    if (mat_status_seen_q != 3'd1 || mat_fetch_denied_o != 32'd0 || mat_misses_o != 32'd1
        || mat_refused_o != 32'd0 || geom_ma_denied_o != 32'd0)
      $fatal(1, "SMOKE: MATERIAL.RESOLVE status %0d, misses %0d, refused %0d, fetch_denied %0d, adapter_denied %0d -- expected one kMiss (1) with the record: R32 puts the upload where ENGINE1 reads",
             mat_status_seen_q, mat_misses_o, mat_refused_o, mat_fetch_denied_o, geom_ma_denied_o);
    if (!mat_rec_has_q || mat_rec_q != upl_rec0)
      $fatal(1, "SMOKE: MATERIAL.RESOLVE's record (has=%b) %064x is not the uploaded record %064x",
             mat_rec_has_q, mat_rec_q, upl_rec0);
    // ---- DEBUG.TRACE against its producer, and its ARMING producer (R52) ---
    // The composition check the ring's contract asks for, and it is an
    // EQUALITY on purpose. `cmd_commands_o` is CMD.DECODER's own count of
    // records walked; `dbg_trace_count_o` is what the ring stored with stage 0
    // armed. They must agree EXCEPT for the records that were offered to the
    // ring BEFORE the arm landed, and that is exactly the first two:
    //
    //   record 1  BeginFrame      offered while the mask is still 0
    //   record 2  DebugTraceArm   offered at its byte 15; CMD.EXEC applies the
    //                             arm at its LAST byte, sixteen bytes later
    //   records 3..11             armed, and stored
    //
    // TRACE_SKIP_C IS DERIVED FROM WHERE THE RECORD SITS, not fitted to the
    // answer. Move the DebugTraceArm record and this constant moves with it;
    // that is the whole reason it is named rather than written as a literal
    // minus two. (It was minus ONE on first writing, which forgot BeginFrame,
    // and the check caught it -- which is the check working.)
    //
    // The ring drops only when full (64 events) and this packet carries far
    // fewer, so any other difference is a lost event, a stage byte that is not
    // 0, or an arming gate that does not gate.
    $display("SMOKE: trace     armed=%b stored=%0d dropped=%0d against decoder records=%0d arms=%0d arm_refused=%0d",
             dbg_trace_armed_o, dbg_trace_count_o, dbg_trace_dropped_o,
             cmd_commands_o, cmd_exec_trace_arms_o, cmd_exec_trace_arm_refused_o);
`ifdef ZHAO_SMOKE_BAD_TRACE_ARM
    // R52 CONTROL, DIRECT POLARITY: the record carries an unassigned
    // stage_mask bit, so it is REFUSED WHOLE and NOTHING is armed. This is the
    // positive control for `trace_arm_refused_o` and, just as importantly, the
    // proof that the refusal does not MASK: a machine that dropped bit 7 and
    // armed stage 0 anyway would pass every check in the plain run and would
    // have armed a set of stages nobody asked for.
    if (cmd_exec_trace_arm_refused_o != 32'd1)
      $fatal(1, "SMOKE/BAD_TRACE_ARM: refusals read %0d, expected exactly 1",
             cmd_exec_trace_arm_refused_o);
    if (cmd_exec_trace_arms_o != 32'd0)
      $fatal(1, "SMOKE/BAD_TRACE_ARM: %0d arm(s) were APPLIED from a record with a reserved bit set -- the guard masked instead of refusing",
             cmd_exec_trace_arms_o);
    if (dbg_trace_armed_o != 7'd0 || dbg_trace_count_o != 32'd0)
      $fatal(1, "SMOKE/BAD_TRACE_ARM: the ring reads armed=%b stored=%0d after a REFUSED arm -- expected 0/0",
             dbg_trace_armed_o, dbg_trace_count_o);
    $display("SMOKE: PASS/BAD_TRACE_ARM -- the reserved bit was refused whole and nothing was armed.");
    // NO `$finish` HERE, and the absence is deliberate. Verilator's `$finish`
    // does not stop the CURRENT process -- it raises a flag that is honoured
    // when the eval returns -- so a `$finish` written here printed this line
    // and then fell straight through to the run's own PASS line, giving TWO
    // verdicts for one run. A stop that does not stop is worse than no stop.
    // The control's polarity is DIRECT: its assertions above are the gate, and
    // the run finishes normally.
`else
    if (cmd_exec_trace_arms_o != 32'd1)
      $fatal(1, "SMOKE: CMD.EXEC applied %0d DebugTraceArm record(s), expected exactly 1 -- the packet's 0xF003 record did not reach the executor",
             cmd_exec_trace_arms_o);
    if (cmd_exec_trace_arm_refused_o != 32'd0)
      $fatal(1, "SMOKE: CMD.EXEC REFUSED %0d DebugTraceArm record(s) -- the clean record is being read as having a reserved bit set",
             cmd_exec_trace_arm_refused_o);
    if (dbg_trace_armed_o != 7'b000_0001)
      $fatal(1, "SMOKE: DEBUG.TRACE armed mask reads %b, expected 000_0001 -- the command's arm did not land",
             dbg_trace_armed_o);
    if (dbg_trace_dropped_o != 32'd0)
      $fatal(1, "SMOKE: DEBUG.TRACE dropped %0d event(s) into a 64-deep ring from %0d records",
             dbg_trace_dropped_o, cmd_commands_o);
    if (dbg_trace_count_o != (cmd_commands_o - TRACE_SKIP_C))
      $fatal(1, "SMOKE: DEBUG.TRACE stored %0d event(s) against %0d records walked by CMD.DECODER (expected records-%0d: everything up to and including the DebugTraceArm record at slot %0d is offered before the arm lands) -- the record port and the ring disagree",
             dbg_trace_count_o, cmd_commands_o, TRACE_SKIP_C, TRACE_SKIP_C);

    // ---- HOST.REGWIN (ruling R51): the host reads both tenants -------------
    // THIS IS THE CLOSURE OF ENTRIES I19 AND I45, measured rather than argued.
    // Every value below crossed the HPS lightweight bridge as a 32-bit register
    // read, was decoded to a tenant by address bits alone, was answered by the
    // block that owns it, and came back. Nothing here reads a core output port
    // directly -- that would prove the block, not the carrier.
    host_read(16'h1804, hr_data, hr_err);          // DEBUG.TRACE: count
    if (hr_err || hr_data != dbg_trace_count_o)
      $fatal(1, "SMOKE: aperture 0x1804 returned err=%b %0d, DEBUG.TRACE's own count_o is %0d",
             hr_err, hr_data, dbg_trace_count_o);
    host_read(16'h1800, hr_data, hr_err);          // DEBUG.TRACE: armed
    if (hr_err || hr_data != 32'(dbg_trace_armed_o))
      $fatal(1, "SMOKE: aperture 0x1800 returned err=%b %08x, DEBUG.TRACE's own armed_o is %b",
             hr_err, hr_data, dbg_trace_armed_o);
    // A RING WORD. Word 3 of event 0 is {rsv[3]=0, stage}, and stage 0 is the
    // only stage this console arms, so the whole word must read 0 -- a value
    // that is CHECKABLE rather than merely present. Its neighbour, word 7, is
    // `command_seq`, which must be the index of the first record the ring saw:
    // record 2 of the packet, the SetPresentationContract after the arm.
    host_read(16'h100C, hr_data, hr_err);          // event 0, word 3
    if (hr_err || hr_data != 32'd0)
      $fatal(1, "SMOKE: aperture 0x100C (event 0 stage word) returned err=%b %08x, expected 0",
             hr_err, hr_data);
    host_read(16'h101C, hr_data, hr_err);          // event 0, word 7
    if (hr_err || hr_data != 32'(TRACE_SKIP_C))
      $fatal(1, "SMOKE: aperture 0x101C (event 0 command_seq) returned err=%b %0d, expected record index %0d -- the first record AFTER the arm",
             hr_err, hr_data, TRACE_SKIP_C);
    // MEASURE.HISTOGRAM, tenant 0. `snapshots` is a LIVE counter driven by the
    // console's own frame tick, so a non-zero read is the interval machinery
    // seen through the aperture. Bin 0 is a bin read, which is the two-cycle
    // handshake path rather than a register mux -- a different route through
    // the same tenant, and it must ANSWER (err low) whatever the count is.
    host_read(16'h0124, hr_hsnap, hr_err);         // snapshots
    if (hr_err)
      $fatal(1, "SMOKE: aperture 0x0124 (histogram snapshots) was refused");
    host_read(16'h0000, hr_data, hr_err);          // bin 0
    if (hr_err)
      $fatal(1, "SMOKE: aperture 0x0000 (histogram bin 0) was refused -- the bin handshake did not complete");
    // THE GUARDS, FIRED WITH LEGAL STIMULUS. Each must REFUSE and be counted;
    // a detector that has not been seen to fire has not been tested.
    host_read(16'h2000, hr_data, hr_err);          // tenant 2: unpopulated
    if (!hr_err)
      $fatal(1, "SMOKE: aperture 0x2000 names an UNMAPPED tenant and was answered, not refused");
    host_read(16'h0002, hr_data, hr_err);          // misaligned
    if (!hr_err)
      $fatal(1, "SMOKE: aperture 0x0002 is misaligned and was answered, not refused");
    host_read(16'h0800, hr_data, hr_err);          // tenant 0, no such register
    if (!hr_err)
      $fatal(1, "SMOKE: aperture 0x0800 is no register of tenant 0 and was answered, not refused");
    host_write(16'h1800, 32'h7F, hr_err);          // write to a read-only tenant
    if (!hr_err)
      $fatal(1, "SMOKE: a WRITE to DEBUG.TRACE's armed register was accepted -- ruling R52 makes the command stream its only writer");
    $display("SMOKE: hostreg   reads=%0d writes=%0d unmapped=%0d misaligned=%0d tenant=%0d timeout=%0d stall=%0d snapshots=%0d",
             hostreg_reads_o, hostreg_writes_o, hostreg_refused_unmapped_o,
             hostreg_refused_misaligned_o, hostreg_refused_tenant_o,
             hostreg_refused_timeout_o, hostreg_stall_cycles_o, hr_hsnap);
    if (hostreg_refused_unmapped_o != 32'd1 || hostreg_refused_misaligned_o != 32'd1
        || hostreg_refused_tenant_o != 32'd2 || hostreg_writes_o != 32'd1)
      $fatal(1, "SMOKE: aperture guards read unmapped=%0d misaligned=%0d tenant=%0d writes=%0d, expected 1/1/2/1",
             hostreg_refused_unmapped_o, hostreg_refused_misaligned_o,
             hostreg_refused_tenant_o, hostreg_writes_o);
    if (hostreg_refused_timeout_o != 32'd0)
      $fatal(1, "SMOKE: a tenant failed to answer %0d time(s) -- the aperture timed out on a live block",
             hostreg_refused_timeout_o);
`endif  // ZHAO_SMOKE_BAD_TRACE_ARM
    // THE EQUALITY ABOVE IS NO LONGER TWO ZEROS AGREEING. Until 2026-09-19 this
    // bench submitted no command packet, CMD.DECODER walked nothing and the
    // ring stored nothing, and this comment said so rather than let a green
    // check be quoted for a seam it could not see. The bench now plays
    // FRAME_RING slot 0 and submits one sealed packet (BeginFrame,
    // PublishResource, EndFrame), so the ring must store exactly the three
    // records the decoder walked -- and the zero-guard below makes a packet
    // that silently stopped arriving FAIL rather than fall back to the note.
    if (cmd_commands_o == 32'd0)
      $fatal(1, "SMOKE: CMD.DECODER walked 0 records -- the bench's command packet no longer arrives, so DEBUG.TRACE's equality above is two zeros agreeing");
    $display("SMOKE: PASS -- the connected core carries traffic on every wire this bench can reach.");
    $finish;
`endif  // ZHAO_SMOKE_BAD_VERTEX -- the clean verdict above is compiled OUT under the R31 control
  end

  // A bench that hangs must say so rather than be killed by a wrapper.
  initial begin
    #40ms;
    $fatal(1, "SMOKE: wall-clock timeout");
  end

endmodule : tb_zhao_console_core_smoke
