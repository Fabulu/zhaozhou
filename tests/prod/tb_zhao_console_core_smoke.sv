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
  // HOW MANY OF THOSE LANES THE R70 MAPPING ACTUALLY DRIVES. `zhao_terrain_
  // loddev` emits THREE 24-bit deviations, so `zhao_console_core.sv:10023-10025`
  // fills lanes 0..2 and holds lane 3's `lane_valid` LOW -- "so lane 3
  // contributes no event rather than contributing a zero one". Named here
  // because the events-per-record ratio below depends on it, and a bare `3`
  // beside a `HIST_LANES = 4` is exactly the kind of number that goes wrong
  // silently when somebody adds a fourth deviation.
  localparam int unsigned HIST_R70_LANES = 3;
  // The Island Patch v1 fine lattice: 33 x 33 vertices, one mip-pass surface.
  // `zhao_terrain_lodfeed`'s own EDGE/VERTS, restated here because the terrain
  // verdict bounds its surplus against TERRAIN.MIPFEED's `samples_sent`.
  localparam int unsigned TERR_LATTICE_VERTS = 33 * 33;   // 1,089
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
  // EIGHT BITS SINCE 2026-09-25 (EDGECLOSE): the terrain HPS arbiter gained
  // client 7, the PREPARE walker's sealed-list re-read. A 7-bit declaration
  // here would SILENTLY TRUNCATE the new client's bit, so a dropped request
  // from the one client whose failure is a counted fallback would be the one
  // the bench could not see.
  logic [7:0]              terr_hps_pend_dropped_mask_o;
  // ---- ENTRY I21's PRODUCER CHAIN (EDGECLOSE, 2026-09-25) ---------------
  // The core binds by `.*`, so a port with no declaration here is an
  // elaboration error rather than a silent miss -- which is the one good
  // property of `.*` and the reason this list is exhaustive.
  logic [31:0]              terr_hps_c7_bursts_o;
  logic [31:0]              terr_hps_c7_wait_cycles_o;
  logic [31:0]              terr_isl_headers_checked_o;
  logic [31:0]              terr_isl_seals_o;
  logic [31:0]              terr_isl_reseals_o;
  logic [31:0]              terr_isl_pitch_illegal_o;
  logic [31:0]              terr_isl_pitch_mismatch_o;
  logic [31:0]              terr_isl_island_bounced_o;
  logic [31:0]              terr_isl_envelope_bad_o;
  logic [31:0]              terr_pw_walks_completed_o;
  logic [31:0]              terr_pw_patches_prepared_o;
  logic [31:0]              terr_pw_descriptors_emitted_o;
  logic [31:0]              terr_pw_skipped_not_resident_o;
  logic [31:0]              terr_pw_list_crc_mismatch_o;
  logic [31:0]              terr_pw_freeze_broken_o;
  logic [31:0]              terr_pw_pitch_illegal_o;
  logic [31:0]              terr_pw_jobs_refused_o;
  logic [31:0]              terr_pw_place_range_o;
  logic [31:0]              terr_pw_bridge_errs_o;
  logic [31:0]              terr_pw_sub_order_bad_o;
  logic [31:0]              terr_pw_store_wait_clocks_o;
  logic [31:0]              terr_ps_b_dev_grants_o;
  logic [31:0]              terr_ps_a_dev_blocked_clocks_o;
  logic [31:0]              terr_ps_b_dev_blocked_clocks_o;
  logic [31:0]              terr_ps_a_lu_blocked_clocks_o;
  logic [31:0]              terr_ps_b_lu_blocked_clocks_o;
  logic [31:0]              terr_ps_lu_ans_unowned_o;
  logic [31:0]              terr_ps_dev_contended_o;
  logic [31:0]              terr_ls_prep_decisions_o;
  logic [31:0]              terr_ls_emit_decisions_o;
  logic [31:0]              terr_ls_ident_mismatch_o;
  logic [31:0]              terr_ls_hist_leak_o;
  logic [31:0]              terr_ls_sel_midpatch_o;
  logic [31:0]              terr_ls_idq_overflow_o;
  logic [31:0]              terr_ls_idq_full_stalls_o;
  logic [31:0]              terr_ls_freeze_drift_o;
  logic [31:0]              terr_ls_freezes_o;
  logic [31:0]              terr_er_records_filed_o;
  logic [31:0]              terr_er_lanes_filed_o;
  logic [31:0]              terr_er_collisions_o;
  logic [31:0]              terr_er_queries_o;
  logic [31:0]              terr_er_edges_real_o;
  logic [31:0]              terr_er_edges_fallback_o;
  logic [31:0]              terr_er_query_own_missing_o;
  logic [31:0]              terr_er_file_out_of_phase_o;
  logic [31:0]              terr_er_query_out_of_phase_o;
  logic [31:0]              terr_eq_door_refused_o;
  logic [31:0]              terr_eq_serve_no_door_o;
  logic [31:0]              terr_eq_serve_src_mismatch_o;
  logic [31:0]              terr_eq_queries_answered_o;
  logic [31:0]              terr_eq_edges_real_o;
  logic [31:0]              terr_eq_fallback_patches_o;
  logic [31:0]              terr_eq_query_abandoned_o;
  logic [31:0]              terr_eq_descriptor_unarmed_o;
  logic [31:0]              terr_eq_gate_wait_clocks_o;

  logic [31:0]             terr_hps_c3_bursts_o;
  logic [31:0]             terr_hps_c4_bursts_o, terr_hps_c4_wait_cycles_o;
  logic [31:0]             terr_hps_c5_bursts_o, terr_hps_c5_wait_cycles_o;
  logic [31:0]             terr_hps_c6_bursts_o, terr_hps_c6_wait_cycles_o;
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
  // (the four `part_fld_*` INPUTS were here. Entry I5 closed 2026-09-20 under
  //  owner ruling R40: `zhao_field_flow_adapter` computes the acceleration
  //  inside the core from the FLOW program's own velocity output, so these
  //  nets are GONE rather than held at zero. What the adapter reports is
  //  declared with the rest of the field evidence below.)
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

  // I50 CLOSED, 2026-09-20 (owner rulings R58 and R69): GEOM.LOOM's node
  // stream no longer arrives at the console edge as twelve wide fields. The
  // 2026-08-31 6.4 ruling puts the PRODUCER on the ARM, and `u_geom_loomfeed`
  // is the carrier that brings its bytes in from HPS DDR. So this bench still
  // plays that ARM -- ONE ROOT node carrying the identity, the same stimulus
  // it always sent -- but it plays it THE WAY THE HARDWARE WILL SEE IT: the
  // record is staged in the played DDR at the frozen 64-byte layout, and what
  // crosses the console edge is a POINTER and a TICKET.
  //
  // That is a strictly stronger fixture than the old one. The old bench drove
  // the loom's port directly, so the record layout, the burst alignment and
  // the carrier's framing were all untested in composition; now every node the
  // loom composes has been through a 64-byte burst on arbiter client 4.
  logic [31:0]             geom_loom_db_plan_base_i;
  logic                    geom_loom_db_post_valid_i;
  logic                    geom_loom_db_post_ready_o;
  logic [31:0]             geom_loom_db_post_base_i;
  logic [31:0]             geom_loom_db_post_ticket_i;
  logic                    geom_loom_db_ret_valid_o;
  logic                    geom_loom_db_ret_ready_i;
  logic [31:0]             geom_loom_db_ret_ticket_o;
  logic                    geom_loom_db_ret_ok_o;
  logic                    geom_loom_db_ret_refused_o;
  logic [2:0]              geom_loom_db_ret_reason_o;
  logic [2:0]              geom_loom_db_ret_loom_reason_o;
  logic [15:0]             geom_loom_db_ret_nodes_o;
  logic [31:0]             geom_loom_db_ret_plan_o;
  logic [31:0]             geom_loom_feed_posts_o, geom_loom_feed_streams_o;
  logic [31:0]             geom_loom_feed_nodes_o, geom_loom_feed_bursts_o;
  logic [31:0]             geom_loom_feed_align_refused_o, geom_loom_feed_hdr_refused_o;
  logic [31:0]             geom_loom_feed_refused_o, geom_loom_feed_faulted_o;
  logic [31:0]             geom_loom_feed_replayed_o, geom_loom_feed_post_stalls_o;
  logic [31:0]             geom_loom_feed_bridge_errs_o, geom_loom_feed_wait_cycles_o;
  logic [31:0]             geom_loom_feed_ret_overflow_o;
  // What the carrier's return said, latched so the checks can read it after
  // the frame rather than racing the one-cycle handshake.
  logic                    loom_ret_seen_q;
  logic [31:0]             loom_ret_ticket_q;
  logic                    loom_ret_ok_q, loom_ret_refused_q;
  logic [2:0]              loom_ret_reason_q, loom_ret_loom_reason_q;
  logic [15:0]             loom_ret_nodes_q;
  logic [31:0]             loom_ret_plan_q;
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
  // ---- GEOM.PARAMBUF (owner completion ruling ITEM 4, 2026-09-22) ---------
  // The bench connects the core with `.*`, so a core port with no net here is
  // a VERILATION FAILURE, not a warning -- RC 1 in about a second, which is
  // the tell the packet brief names and which this sweep hit on its first run.
  logic [31:0] geom_pa_verts_o;
  logic [31:0] geom_pa_tris_o;
  logic [31:0] geom_pa_chunks_o;
  logic [31:0] geom_pa_frames_o;
  logic [31:0] geom_pa_denied_o;
  logic [31:0] geom_pa_overflow_o;
  logic [31:0] geom_pa_discarded_o;
  logic [31:0] geom_pa_unsealed_o;
  logic [31:0] geom_pa_overrun_o;
  logic [31:0] geom_pa_flipblock_o;
  logic [31:0] geom_pa_pubblock_o;
  logic [31:0] geom_pa_addrbad_o;
  logic [31:0] geom_pa_scrcontend_o;
  logic [31:0] geom_pa_retireunder_o;
  logic [31:0] geom_pa_unaligned_o;
  logic [15:0] geom_pa_fault_src_o;
  logic        geom_pa_fault_o;
  logic        geom_pa_busy_o;
  logic        geom_pa_seal_ready_o;
  // GEOM.VERTID -- the one geometry identity space (ARENAID, 2026-09-25).
  logic [31:0] geom_vid_tris_o;
  logic [31:0] geom_vid_refs_o;
  logic [31:0] geom_vid_published_o;
  logic [31:0] geom_vid_reused_o;
  logic [31:0] geom_vid_unshared_o;
  logic [31:0] geom_vid_sunk_o;
  logic [31:0] geom_vid_opens_o;
  logic [31:0] geom_vid_stall_o;
  // Entry I54: the chunk serialiser and its identity queue, in the
  // COMPOSED console. `geom_cs_head_tile_i` is the only input of the
  // group and is held at tile 0 -- this bench does not walk, it only
  // asserts that the producer ran and that its detectors stayed quiet.
  logic [31:0] geom_tidq_underflow_o;
  logic [31:0] geom_tidq_overflow_o;
  logic [31:0] geom_tidq_unnamed_o;
  logic [31:0] geom_cs_chunks_o;
  logic [31:0] geom_cs_refs_o;
  logic [31:0] geom_cs_tiles_o;
  logic [31:0] geom_cs_chain_break_o;
  logic [31:0] geom_cs_head_clash_o;
  logic [31:0] geom_cs_truncated_o;
  logic [31:0] geom_cs_sunk_o;
  logic [ 9:0] geom_cs_head_tile_i = 10'd0;
  logic [31:0] geom_cs_head_chunk_o;
  logic        geom_cs_head_valid_o;
  logic [31:0] geom_pw_dirs_o;
  logic [31:0] geom_pw_dirmiss_o;
  logic [31:0] geom_pw_chunks_o;
  logic [31:0] geom_pw_stale_o;
  logic [31:0] geom_pw_illegal_o;
  logic [31:0] geom_pw_tris_o;
  logic [31:0] geom_pw_trisbad_o;
  logic [31:0] geom_pw_cut_o;
  logic [31:0] geom_pw_denied_o;
  logic [31:0] geom_pw_short_o;
  logic [31:0] geom_pw_stray_o;
  logic [31:0] geom_pw_genrace_o;
  logic [31:0] geom_pw_unaligned_o;
  logic [15:0] geom_pw_depth_o;
  logic [31:0] geom_ws_denied_o;
  logic [31:0] geom_ws_contention_o;
  logic [31:0] geom_ws_short_o;
  logic [31:0] geom_ws_long_o;
  logic [31:0] geom_ws_unowned_o;
  logic [31:0] geom_ws_retire_unowned_o;
  logic [31:0] geom_ws_wbeat_unowned_o;
  logic [31:0] geom_ws_ledger_full_o;
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

  // ---- I29 IS CLOSED: GEOM.POSE's clip page and skeleton bake --------------
  // GEOM.SKIN's two matrix arrays used to be here and the bench drove them with
  // an identity; they went when `zhao_geom_pose_palette` was composed (I10).
  // THE DECODER'S SOURCE WENT THE SAME WAY 2026-09-22. Fourteen inputs are
  // gone from this bench -- start, bone count, root displacement, parent, the
  // three rest translations, the four quaternion lanes and the twelve-element
  // inverse-rest matrix -- because `zhao_geom_clipread` and
  // `zhao_geom_bonesrc` are composed in the core and fill them from real
  // memory through requester G.
  //
  // SO THIS BENCH NO LONGER DRIVES A SYNTHETIC SKELETON, AND THAT IS A GAIN
  // RATHER THAN A LOSS OF COVERAGE. The note that stood here said driving one
  // "would be this bench inventing an asset"; it no longer can, and what it
  // reads instead is the reader's own refusal counters. With no creature page
  // published, `geom_cr_not_resident_o` is what a posed draw produces and the
  // palette still substitutes the identity bind pose per bone, exactly as
  // before -- so the skinned counts below do not move, and the reason they do
  // not has moved one step further from a harness constant.
  logic [4:0]              geom_pose_bone_idx_o;
  logic                    geom_pose_busy_o;
  logic                    geom_pose_done_o;
  logic [31:0]             geom_pose_palettes_decoded_o;
  // The instance walk: posed draws captured, and cycles a posed draw waited
  // for the reader's one request slot.
  logic [31:0]             geom_pose_requests_o;
  logic [31:0]             geom_pose_walk_holds_o;
  // The page reader's resource and form identities (the ownership ruling's
  // section 5), its census and its seventeen refusals.
  logic [23:0]             geom_cr_body_index_o;
  logic [15:0]             geom_cr_body_gen_o;
  logic [23:0]             geom_cr_clip_index_o;
  logic [15:0]             geom_cr_clip_gen_o;
  logic [23:0]             geom_cr_body_owner_o;
  logic [23:0]             geom_cr_clip_owner_o;
  logic [31:0]             geom_cr_bodies_o;
  logic [31:0]             geom_cr_clips_o;
  logic [31:0]             geom_cr_frames_o;
  logic [31:0]             geom_cr_pages_dropped_o;
  logic [31:0]             geom_cr_bad_magic_o;
  logic [31:0]             geom_cr_truncated_o;
  logic [31:0]             geom_cr_misaligned_o;
  logic [31:0]             geom_cr_bad_bone_count_o;
  logic [31:0]             geom_cr_bone_mismatch_o;
  logic [31:0]             geom_cr_overflow_o;
  logic [31:0]             geom_cr_not_rigid_o;
  logic [31:0]             geom_cr_reserved_nz_o;
  logic [31:0]             geom_cr_denied_o;
  logic [31:0]             geom_cr_clip_miss_o;
  logic [31:0]             geom_cr_frame_oob_o;
  logic [31:0]             geom_cr_not_resident_o;
  logic [31:0]             geom_cr_owner_mismatch_o;
  logic                    geom_cr_busy_o;
  // The store.
  logic [31:0]             geom_bs_prefetch_late_o;
  logic [31:0]             geom_bs_rest_nonrigid_o;
  logic [31:0]             geom_bs_reserved_nz_o;
  logic [31:0]             geom_bs_fills_o;
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
  // THE SUBPATCH JOB PORT IS AN OVERRIDE NOW, NOT THE ONLY PRODUCER (entry
  // I21, 2026-09-21). `zhao_terrain_jobissue` issues these thirteen fields
  // inside the core, from `zhao_terrain_lod`'s decisions, from descriptors
  // `zhao_terrain_spdesc` assembles out of `zhao_terrain_devstore`. THE BENCH
  // STILL DRIVES THEM, and that is deliberate rather than left over.
  //
  // WHY THIS BENCH CANNOT USE THE INTERNAL PRODUCER, and it is upstream of
  // everything the composition did: no compose job is issued here, so the
  // compose cache never fills, never serves, and never opens the door the
  // assembler and the issuer wait at. The injection below is therefore the
  // ONLY thing here that makes TERRAIN.TESS run, and SIX assertions stand on
  // it.
  //
  // >> THE REASON GIVEN HERE WAS STALE AND IS CORRECTED, 2026-09-22
  // >> (gz/devsdram).  It said "every terrain page this bench plays FAILS ITS
  // >> CRC -- the directory reports `crc_fail=3`".  IT DOES NOT, and this
  // >> bench's own output says so: `pl loaded=3 faulted=0 crc_fails=0`.  The
  // >> page-header loop that fixed it landed 2026-09-20 (terrain9) and is a
  // >> few thousand lines below, where its own note ALSO records that the
  // >> faults were `hdr_ident`, not CRC, even while they existed -- "a stated
  // >> cause that the machine's own counters refute is the shape CLAUDE.md
  // >> keeps finding".  The sentence here survived both corrections.
  // >>
  // >> IT MATTERS BECAUSE IT IS QUOTED.  This sentence is what
  // >> tests/CMakeLists.txt cites for "the console smoke cannot show it", and
  // >> the DEVSDRAM packet nearly wrote it into an owner-facing report as the
  // >> reason owner ruling R242's SDRAM store is unexercised here.  THREE
  // >> PAGES LOAD AND 48 DEVIATION RECORDS REACH THE STORE: the WRITE path is
  // >> exercised by real console stimulus.  What is genuinely absent is the
  // >> COMPOSE job, and that -- not a CRC -- is why the READ path is not.
  //
  // Deleting it was tried and reverted inside this packet. It cost exactly
  // those six assertions and bought nothing: the completion register moves on
  // MODULES becoming connected, and a port is not a module. The real hole is
  // the CRC failure, and this injection was hiding it -- it is now named in
  // the core's entry I21 instead of being papered over here.
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
  // `terr_job_mat_a_i`, `terr_job_mat_b_i` and `terr_job_weight_i` WERE HERE
  // and are gone with the core ports, 2026-09-22 (LAYERE). Ruling R13 ruled
  // them the wrong carrier; the material is now read per triangle from layer E
  // inside TERRAIN.TESS and rides the ModeRef beat.
  //
  // THE DECLARATIONS HAD TO GO, not just the drives. `.*` binds by NAME, so a
  // net whose port no longer exists is silently not bound -- and the three
  // assignments below it would have gone on being written, by a bench, into
  // nothing, forever. That is exactly the `-BadAttribute` failure this
  // script's own param block records: a stimulus that binds nothing, says
  // nothing, and leaves a green run indistinguishable from a targeted one.
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

  // I34, NARROWED AGAIN 2026-09-23 (EARTHADAPT): the HEIGHT RETURN LANE's
  // three ports have LEFT the core's edge too. `zhao_field_earth_adapter`
  // drives them inside the module from the one field engine, so this bench no
  // longer holds `fld_valid_i` low -- there is nothing here to hold.
  //
  // WHAT THIS BENCH THEREFORE NO LONGER PROVES, said rather than left implicit:
  // the old `terr_pt_fld_valid_i = '0` was a positive statement that the smoke
  // ran section 3.4 with an EMPTY program list. It still does -- no smoke form
  // issues a TerrainField, so `fields_active_o` stays 0 and the adapter is
  // never asked for a lane -- but that is now a property of the STIMULUS and
  // not of a wire this file drives. `fld_earth_records_o` reading 0 is the
  // assertion that says so, and it is checked below.
  logic                    terr_pt_fld_add_accept_o;
  logic                    terr_pt_fld_add_reject_o;
  logic                    terr_pt_fld_covers_o;
  // TERRAIN.FIELDLIST's evidence, and CMD.EXEC's TerrainField arm's, which was
  // wholly unconnected until the same commit.
  logic [31:0]             terr_fl_records_sealed_o;
  logic [31:0]             terr_fl_tail_rejected_o;
  logic [31:0]             terr_fl_unresolved_o;
  logic [31:0]             terr_fl_replays_o;
  logic [31:0]             terr_fl_entries_replayed_o;
  logic [31:0]             terr_fl_open_at_patch_o;
  logic [4:0]              terr_fl_records_o;
  logic                    terr_fl_sealed_o;
  logic                    terr_fl_idle_o;
  // FIELD.EARTH_ADAPTER's evidence (EARTHADAPT, 2026-09-23).
  logic [31:0]             fld_earth_records_o;
  logic [31:0]             fld_earth_tail_rejected_o;
  logic [31:0]             fld_earth_runs_o;
  logic [31:0]             fld_earth_skipped_uncovered_o;
  logic [31:0]             fld_earth_not_begun_o;
  logic [31:0]             fld_earth_noprog_o;
  logic [31:0]             fld_earth_faults_o;
  // R168's arm on the Earth record, NEW 2026-09-23 (PATCHV2). Declared so the
  // `.*` binding stays total. This bench admits no field program, so no Earth
  // run retires at all and this must read 0 -- asserted below with the rest of
  // the Earth evidence, where a zero is the ABSENCE of a run and not a claim
  // that short records cannot happen. The positive control is
  // `field_earth_adapter_directed` case 14, which fires it from the block's own
  // boundary with legal stimulus.
  logic [31:0]             fld_earth_short_record_o;
  logic [31:0]             fld_earth_lane_desync_o;
  logic [31:0]             fld_earth_stall_cycles_o;
  logic                    fld_earth_idle_o;
  logic [31:0]             cmd_exec_tflds_o;
  logic [31:0]             cmd_exec_tfld_overflow_o;
  logic [31:0]             cmd_exec_tfld_src_truncated_o;
  logic [4:0]              terr_pt_fields_active_o;
  logic [15:0]             terr_pt_trace_patch_id_o;
  logic [31:0]             terr_pt_trace_hash_o;
  logic [15:0]             terr_pt_trace_cmd_o;
  logic [31:0]             terr_pt_programs_rejected_o;

  // (`terr_cc_cs_*` LEFT THE CORE's EDGE 2026-09-21, core entry I32: the
  // compose cache's layer-D write is driven by TERRAIN.BAKE inside the core.)
  // `terr_cc_serve_release_i` is OR-ed with `zhao_terrain_jobissue`'s release
  // inside the core: a harness that drives a job by hand must be able to
  // retire the patch it drove.
  logic                    terr_cc_serve_release_i;

  logic [31:0]             terr_ps_lattices_o;
  logic [31:0]             terr_ps_lattices_refused_o;
  logic [31:0]             terr_ps_vertices_o;
  logic [31:0]             terr_ps_bursts_o;
  logic [31:0]             terr_ps_guard_denied_o;
  logic [31:0]             terr_ps_incomplete_o;
  // R13's layer E. DECLARED AND NOT CHECKED, and the honest reason is in the
  // bench's own note about the terrain path: every terrain page this bench
  // plays FAILS ITS CRC, so no page becomes resident, TERRAIN.PAGESTREAM is
  // never given a job, and this counter cannot move here however correct the
  // block is. It is fired where a test can watch it -- `pagestream_rtl_
  // directed` -- which is R95 applied rather than quoted.
  logic [31:0]             terr_ps_cells_o;
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
  // TERRAIN.VELJOIN + TERRAIN.VELOCITY, composed 2026-09-26 (TERRVEL).
  // Declared here because this bench connects the core by `.*`, so a core port
  // with no matching signal is an elaboration error and not a warning. The
  // velocity chain's own numbers are asserted in section VEL below.
  logic [31:0]             terr_vj_lanes_joined_o;
  logic [31:0]             terr_vj_sweeps_started_o;
  logic [31:0]             terr_vj_sweeps_aborted_o;
  logic [31:0]             terr_vj_vtx_mismatch_o;
  logic [31:0]             terr_vj_arm_stall_o;
  logic [31:0]             terr_tv_samples_o;
  logic [31:0]             terr_tv_add_sats_o;
  logic [31:0]             terr_tv_rescale_sats_o;
  logic [15:0]             terr_tv_moving_mask_o;
  logic [31:0]             part_ter_moving_o;
  logic [31:0]             part_ter_vel_sats_o;
  logic [31:0]             part_col_moving_ground_o;
  logic [31:0]             terr_cc_vel_words_o;
  logic [31:0]             terr_cc_vel_orphan_o;
  logic [31:0]             terr_cc_vel_done_mm_o;
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
  // Same: the compose cache never fills here, so neither of these can move.
  // `compcache_front_rtl_directed` fires both on its 9 x 9 instance.
  logic [31:0]             terr_cc_mat_oob_o;
  logic [31:0]             terr_cc_mat_cells_o;

  // (`terr_dm_*` LEFT THE CORE's EDGE 2026-09-21, core entry I27's first half:
  // `zhao_terrain_pageio` holds the slot, generation and epoch the patch was
  // served under and marks the directory itself.)

  // I27's handle check left this edge on 2026-09-22: `u_terrain_lodfeed` is
  // its caller now and the exchange is internal.  What crosses is the pair of
  // counters, and they are declared TOGETHER because the stale count means
  // nothing without the checked count beside it -- a lane reading
  // `handles_stale_o == 0` off a bench that never loaded a page would be
  // quoting the silence of an instrument that was never switched on.
  logic [31:0]             terr_lodfeed_handles_checked_o;
  logic [31:0]             terr_lodfeed_handles_stale_o;
  // Core entry I21's view-mask narrowing.  It is EXPECTED ZERO here and that
  // is not evidence of anything: every terrain page this bench plays fails its
  // CRC, so the compose door never opens and the counter cannot move.  Its
  // real control is `terrain_jobissue_directed` case 12, which fires it and
  // shows it flat beside the firing.  Declared so the `.*` binding is total.
  logic [31:0]             terr_ji_view_mask_high_o;

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
  // I49, 2026-09-20: the REQUEST and the response's ready are gone from the
  // core's port list -- `u_material_window` issues the resolve from the
  // triangle's own material and consumes the answer. This bench OBSERVES the
  // response, which is the whole point: it no longer plays the seam it is
  // supposed to be measuring.
  logic         mat_rsp_valid_o;
  logic [31:0]  mat_win_resolves_o, mat_win_switches_o;
  logic [31:0]  mat_win_drain_stall_o, mat_win_answer_stall_o;
  logic [31:0]  mat_win_occupancy_max_o, mat_win_no_record_o;
  logic [31:0]  mat_win_selector_overflow_o, mat_win_clut_unowned_o;
  // Owner ruling 1, 2026-09-22: the NO_MATERIAL mode's census and its fault.
  logic [31:0]  mat_win_no_material_spans_o, mat_win_mode_refused_o;
  logic [31:0]  mat_win_err_unpublished_o, mat_win_err_underflow_o;
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
  // ITEM 5 (owner ruling 2026-09-22, packet EDGERECON): jobs whose
  // sparse-fill request was refused because this composition carries a
  // dense-seal shell. Declared here because `dut (.*)` cannot bind a port
  // the bench has no signal for -- a new core output makes THIS FILE fail
  // to verilate, which is how this one was found: the smoke died in ~1 s
  // with "Can't find" at the instantiation, the RC-1-in-one-second tell.
  //
  // IT STAYS ZERO HERE AND THAT IS NOT EVIDENCE ABOUT THE GUARD. This
  // bench drives `terr_sparse_fill_i` LOW (see the terrain job below), so
  // no job ever asks for sparse fill, so nothing can be refused -- and the
  // compose door never opens in this smoke anyway, because every terrain
  // page fails its CRC. The counter is FIRED where legal stimulus reaches
  // it: terrain_pipe_differential, which runs the same sequencer against
  // a dense shell with the request set and asserts the refusal, the
  // absence of a short seal, and an IDENTICAL RENDER (R95).
  logic [31:0]             terr_sparse_refused_o;
  logic [31:0]             terr_refs_forwarded_o;
  logic [31:0]             terr_release_unsafe_o;
  logic [31:0]             terr_tess_vertices_o;
  logic [31:0]             terr_tess_refs_o;
  logic [31:0]             terr_tess_rejected_o;
  logic [31:0]             terr_tess_lod_clamped_o;
  // THIS ONE THE BENCH COULD ALMOST REACH, and it is worth being exact about
  // why it does not. The injected subpatch job DOES make TERRAIN.TESS run --
  // six assertions stand on that -- but only in ModeTri, and the layer-E read
  // is issued on ModeRef beats alone. `terrain_tess_directed` fires it, with
  // its negative control.
  logic [31:0]             terr_tess_mat_unarmed_o;
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
  logic [31:0]             terr_light_degen_mismatch_o;
  // TERRAIN.UV's coordinate packet, added 2026-09-23. Declared here for the
  // same reason the light's ports are: `.*` cannot bind what the bench does
  // not declare. The smoke CANNOT exercise this lane -- every terrain page
  // it plays fails its CRC, so no page becomes resident and terrain emits no
  // triangle, so no reference ever reaches the lane. These nets prove the
  // core still ELABORATES and still renders its 14 GEOMETRY triangles; the
  // lane's own evidence is tests/terrain/terrain_uvlane_directed.cpp.
  logic                    terr_uv_valid_o;
  logic                    terr_uv_ready_i;
  logic signed [31:0]      terr_uv_au_o;
  logic signed [31:0]      terr_uv_av_o;
  logic signed [31:0]      terr_uv_bu_o;
  logic signed [31:0]      terr_uv_bv_o;
  logic signed [31:0]      terr_uv_cu_o;
  logic signed [31:0]      terr_uv_cv_o;
  logic [15:0]             terr_uv_src_id_o;
  logic [31:0]             terr_uv_refs_taken_o;
  logic [31:0]             terr_uv_emitted_o;
  logic [31:0]             terr_uv_stale_reads_o;
  logic [31:0]             terr_uv_pitch_clamped_o;
  logic [31:0]             terr_uv_pitch_illegal_o;
  // `post_gd_*` and `post_gg_*` ARE GONE, 2026-09-21 (owner ruling R195).
  // POST.GATHER, its R195 tag law and its plane store are composed inside the
  // core, so the gather planes are no longer nets this bench has to invent.
  // Their stimulus here was six lines of zero -- which is precisely what W10
  // warns about, an absent output that looks like a zero result, and the
  // bench could not tell the two apart either.
  logic                    post_atm_req_v_o;
  logic [POST_XW-1:0]      post_atm_req_x_o;
  logic [POST_YW-1:0]      post_atm_req_y_o;
  logic                    post_atm_en_i;
  logic                    post_atm_valid_i;
  logic [15:0]             post_atm_rgb_i;
  logic [7:0]              post_atm_opacity_i;
  logic                    post_atm_add_i;
  // post_bloom_gain_i .. post_ink_rgb_i are GONE (R36): CMD.EXEC drives them.
  // `post_hud_*` IS GONE FROM THE CORE'S EDGE (2026-09-21, owner rulings R233
  // and R235): TWOD.BAND holds the HUD band inside the core and answers the
  // compositor's raster sweep. Its evidence is the `twod_band_*` group below.
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

  // ---- POST.GATHER evidence (composed 2026-09-21, owner ruling R195) ------
  logic [31:0]             gather_frag_untagged_o;
  logic [31:0]             gather_frag_below_knee_o;
  logic [31:0]             gather_frag_lit_o;
  logic [31:0]             gather_reserved_channel_o;
  logic [31:0]             gather_fragments_o;
  logic [31:0]             gather_glow_saturations_o;
  logic [31:0]             gather_disp_clamps_o;
  logic [31:0]             gather_cells_flushed_o;
  logic [31:0]             gather_cells_written_o;
  logic [31:0]             gather_oob_writes_o;
  logic [31:0]             gather_gd_reads_o;
  logic [31:0]             gather_gg_reads_o;
  logic [31:0]             gather_gd_miss_o;
  logic [31:0]             gather_gg_miss_o;
  logic [31:0]             gather_flush_overrun_o;
  logic [31:0]             gather_rdw_collide_o;
  logic [31:0]             gather_plane_commits_o;
  // `hist_ev_*` IS GONE from the core's edge too (entry I18 closed 2026-09-20,
  // owner ruling R70): `zhao_terrain_lodfeed` is composed inside the core and
  // its deviation records ARE the events. This bench used to drive five ports
  // here with zeros; it now drives none, and the four counters below are how it
  // reads what the internal producer did.
  logic [31:0]             terr_lodfeed_lattices_walked_o;
  logic [31:0]             terr_lodfeed_lattices_dropped_o;
  logic [31:0]             terr_lodfeed_dev_records_o;
  logic [31:0]             terr_lodfeed_stray_samples_o;
  // THE SUBPATCH DECISION CHAIN'S EVIDENCE (entry I21, composed 2026-09-21).
  // Declared, not asserted on: see the note at the terrain job above for why
  // every one of them reads ZERO in this bench and why that zero is the
  // upstream CRC failure rather than anything about these five blocks. The
  // bench prints them on the `lod` line so the zero is visible and explained
  // instead of absent.
  logic [31:0]             terr_ds_patches_read_o;
  logic [31:0]             terr_ds_read_unwritten_o;
  logic [31:0]             terr_ds_hist_step_bad_o;
  logic [31:0]             terr_sp_descriptors_o;
  logic [31:0]             terr_sp_door_refused_o;
  logic [31:0]             terr_sp_serve_no_door_o;
  logic [31:0]             terr_sp_door_src_mismatch_o;
  logic [31:0]             terr_sp_order_bad_o;
  logic [31:0]             terr_sp_patches_unfresh_o;
  logic [31:0]             terr_lod_rep_count0_o;
  logic [31:0]             terr_lod_rep_count1_o;
  logic [31:0]             terr_lod_rep_count2_o;
  logic [31:0]             terr_lod_rep_count3_o;
  logic [31:0]             terr_lod_triangles_emitted_o;
  logic [31:0]             terr_ji_jobs_issued_o;
  logic [31:0]             terr_ji_patches_dropped_o;
  logic [31:0]             terr_ji_ctx_refused_o;
  logic [31:0]             terr_ji_serve_no_ctx_o;
  logic [31:0]             terr_ji_ctx_src_mismatch_o;
  logic [31:0]             terr_ji_lod_src_mismatch_o;
  logic [31:0]             meas_gov_rep_count0_o;
  logic [31:0]             meas_gov_rep_count1_o;
  logic [31:0]             meas_gov_rep_count2_o;
  logic [31:0]             meas_gov_rep_count3_o;
  // PROJ.CFGVALID (entry I14's closing half). `proj_cfg_armed_o` is the ARM's
  // positive control and `proj_en_held_offers_o` the vertices the gate
  // withheld; both are asserted where the tokens are, below.
  logic [31:0]             proj_cfg_armed_o;
  logic [31:0]             proj_en_held_offers_o;
  logic [31:0]             meas_starve_denials_o;
  logic [31:0]             meas_starve_frames0_o;
  logic [31:0]             meas_starve_frames1_o;
  logic [31:0]             view_kx_saturated_o;
  logic [31:0]             view_proj_saturations_o;
  logic [31:0]             cmd_view_count_refused_o;
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
  // 	ri_flat_request_i is no longer a core port (entry I49): the console
  // builds it from MATERIAL.RESOLVE's published answer.
  // `tri_continuation_tail_i` and `tri_fragment_state_i` are no longer core
  // ports either (entry I20, FRAGSTATE 2026-09-25), and their DECLARATIONS go
  // with them for the reason this file has already written down twice: `.*`
  // binds by NAME, so a net whose port no longer exists is silently not bound,
  // and a bench that went on driving it would look like stimulus while
  // supplying nothing. The console builds both from named owners now -- the
  // MATERIAL's `fragment_state`/`fragment_decl` when it declares a profile,
  // the PRODUCER's door declaration otherwise.
  logic         fill_req_ready_i;
  logic         fill_req_valid_o;
  logic [31:0]  fill_req_addr_o;
  logic         fill_data_valid_i;
  logic [15:0]  fill_data_i;
  logic         fill_refused_i;
  logic [63:0]  frame_clear_word_i;
  // `sheet_req_*` LEFT THE CORE'S PORT LIST 2026-09-25 (TERRAINAUX). The
  // texture island's AUX read lands on `u_surface_sheetshare`'s CLIENT C
  // INSIDE the core now, and both halves of the loop are internal. These six
  // declarations are deleted rather than left: a `logic` named after a port
  // that no longer exists binds to nothing under `.*` and reads, to the next
  // person, as a port the bench forgot to drive.
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

  // ---- INPUT.SNAC's physical edge (owner ruling R7) ------------------------
  // The bench is the SNAC CONNECTOR, exactly as it is the HPS for the burst
  // bridge: nothing is plugged in, so DAT and /ACK idle high and the adapter
  // reads every slot absent and passes `pad_*_i` through unchanged. That is
  // the merge's identity case, and it is what keeps this bench's pad numbers
  // the same as they were before the adapter existed. The decode itself is
  // exercised in tests/input/input_snac_directed.cpp, where a modelled PS1
  // pad actually answers.
  localparam int SMOKE_SNAC_PORTS = 2;
  logic [SMOKE_SNAC_PORTS-1:0] snac_dat_i;
  logic [SMOKE_SNAC_PORTS-1:0] snac_ack_n_i;
  logic [SMOKE_SNAC_PORTS-1:0] snac_att_n_o;
  logic                        snac_clk_o;
  logic                        snac_cmd_o;
  logic [3:0]                  snac_present_o;
  logic [63:0]                 snac_polls_o;
  logic [63:0]                 snac_timeouts_o;
  logic [63:0]                 snac_bad_header_o;
  logic [63:0]                 snac_overrides_o;
  logic [63:0]                 snac_seq_gaps_o;
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
  logic [31:0] geom_va_done_stall_o;   // R88's watchdog
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
  logic [31:0]             geom_untex_refused_o;

  // ---- THE FORGE CHAIN's evidence (composed 2026-09-21, FORGECOMP) --------
  // The bench binds with `.*`, so every core port must be declared here or
  // the build fails with `Can't find definition of variable` -- which is the
  // good failure: a port that appeared and was never declared would otherwise
  // be a silently unread output.
  logic        forge_pb_busy_o;
  // PARTMAT 2026-09-22: three clients now (mesh 0, forge 1, particles 2).
  logic [95:0] geom_clipdoor_granted_o;
  logic [31:0] forge_pb_pages_o;
  logic [31:0] forge_pb_draws_o;
  logic [31:0] forge_pb_bad_magic_o;
  logic [31:0] forge_pb_page_overflow_o;
  logic [31:0] forge_pb_truncated_o;
  logic [31:0] forge_pb_lookup_miss_o;
  logic [31:0] forge_pb_refused_kind_o;
  logic [31:0] forge_pb_refused_cliff_o;
  logic [31:0] forge_pb_bad_record_o;
  logic [31:0] forge_pb_refused_nopage_o;
  logic [31:0] forge_pb_denied_o;
  logic [31:0] forge_prim_jobs_o;
  logic [31:0] forge_prim_triangles_o;
  logic [31:0] forge_prim_refused_family_o;
  logic [31:0] forge_prim_refused_limit_o;
  logic [31:0] forge_prim_skipped_view_o;
  logic [31:0] forge_eval_jobs_o;
  logic [31:0] forge_eval_points_o;
  logic [31:0] forge_eval_vertices_o;
  logic [31:0] forge_eval_refused_limit_o;
  logic [31:0] forge_eval_skipped_view_o;
  logic [31:0] forge_eval_sat_events_o;
  logic [31:0] forge_eval_walk_overrun_o;
  logic [31:0] forge_ring_jobs_o;
  logic [31:0] forge_ring_rings_o;
  logic [31:0] forge_ring_vertices_o;
  logic [31:0] forge_ring_refused_family_o;
  logic [31:0] forge_ring_refused_elsewhere_o;
  logic [31:0] forge_ring_refused_limit_o;
  logic [31:0] forge_ring_skipped_view_o;
  logic [31:0] forge_ring_sat_events_o;
  logic [31:0] forge_asm_jobs_o;
  logic [31:0] forge_asm_vertices_o;
  logic [31:0] forge_asm_triangles_o;
  logic [31:0] forge_asm_index_oor_o;
  logic [31:0] forge_asm_vtx_overflow_o;
  logic [31:0] forge_asm_slot_pressure_o;
  logic [31:0] forge_asm_dq_refused_o;
  logic [31:0] forge_asm_dq_stray_o;
  logic [31:0] forge_asm_proj_stray_o;
  // The carriage detector of owner completion ruling 2 (2026-09-22). This
  // smoke DRIVES NO PROCEDURAL DRAW, so it reads zero here for the dullest
  // possible reason -- the forge chain is quiescent in it. Its positive
  // control is `forge_assemble_directed` case 5b, at the block's own ports.
  logic [31:0] forge_asm_mat_skew_o;
  logic [31:0] cmd_exec_forges_o;
  logic [31:0] cmd_exec_forge_overflow_o;
  logic [31:0] cmd_exec_forge_src_truncated_o;
  logic [31:0] geom_ma_jobs_f_o;
  logic [31:0] geom_ma_jobs_g_o;
  // FORGE.SHADOW's chain, composed 2026-09-23 (SHADOWRIDE). THIS BENCH DOES
  // NOT REACH IT and says so rather than implying otherwise: the smoke drives
  // DrawForm and never publishes a kind-8 CREATURE_FORM page, so
  // `zhao_geom_ladderbank` adopts nothing, every ladder query is a miss, no
  // caster is emitted and no hull is drawn. What a green run here DOES show is
  // that the composition ELABORATES, that the shared assembler still serves
  // FORGE.PRIM unchanged, and that the three counters below stay at zero --
  // which is the correct answer for a frame with no creature forms in it.
  logic [31:0] geom_ma_jobs_h_o;
  logic [31:0] geom_lb_pages_o, geom_lb_records_o, geom_lb_pages_dropped_o;
  logic [31:0] geom_lb_bad_magic_o, geom_lb_truncated_o, geom_lb_bad_record_o;
  logic [31:0] geom_lb_overflow_o, geom_lb_denied_o, geom_lb_lookup_miss_o;
  logic [31:0] geom_ls_ticks_o, geom_ls_skipped_repeat_o, geom_ls_bank_miss_o;
  logic [31:0] geom_ls_no_radius_o, geom_ls_dropped_o, geom_ls_out_of_range_o;
  logic [127:0] geom_ls_rung_counts_o;
  logic [31:0] geom_ls_rad_evaluations_o, geom_ls_rad_behind_o;
  logic [31:0] geom_ls_rad_bad_bound_o, geom_ls_rad_saturated_o;
  logic [63:0] terr_tsh_grants_o;
  logic [31:0] terr_tsh_contended_o, terr_tsh_stray_rsp_o;
  logic [15:0] forge_shadow_emitted_o, forge_shadow_no_ground_o;
  logic [15:0] forge_shadow_zero_radius_o, forge_shadow_far_rung_o;
  logic [15:0] forge_shadow_tap_protocol_o;
  logic [31:0] forge_shadow_skipped_view_o;
  logic [31:0] forge_fanidx_hulls_o, forge_fanidx_triangles_o;
  logic [31:0] forge_fanidx_short_ring_o, forge_fanidx_ring_overflow_o;
  logic [127:0] forge_fanidx_hulls_rung_o;
  logic [31:0] forge_jobarb_grant_prim_o, forge_jobarb_grant_shadow_o;
  logic [31:0] forge_jobarb_switches_o, forge_jobarb_wait_prim_o;
  logic [31:0] forge_jobarb_wait_shadow_o, forge_jobarb_no_desc_o;
  logic [31:0] geom_clipdoor_switches_o;
  logic [31:0] geom_clipdoor_idle_offered_o;
  logic [31:0] geom_clipdoor_err_hold_broken_o;
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
  // ---- TEXTURE EVIDENCE (entry I49). The composed console's answer to 'did
  // the island sample'. Eight counters that were dangling or sunk until
  // 2026-09-20; render_texture_samples_o is the one that says a texel
  // reached a fragment, and the other seven say why a zero is a zero.
  logic [31:0] render_texture_fragments_o;
  logic [31:0] render_texture_cache_hits_o;
  logic [31:0] render_texture_cache_misses_o;
  logic [31:0] render_texture_palette_lookups_o;
  logic [31:0] render_texture_plan_accepted_o;
  logic [31:0] render_texture_dispatch_accepted_o;
  logic [31:0] render_texture_combine_refused_o;
  logic [31:0] render_texture_samples_o;
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
  // THE LOADER AND BOTH DIRECTORY PHASES ARE BEHIND THE DOORBELL as of
  // 2026-09-20 (entry I42 closed, owner ruling R43), so what this bench drives
  // is the HPS side of a contract rather than a leaf port: posts in, ticketed
  // returns out. `zhao_field_doorbell` is the R14 pattern with this seam's
  // fields.
  logic        [31:0] fld_cfg_plan_base_i;
  // FH08's differential control, new with zhao_field_host_v2.
  logic               fld_cfg_slow_clear_i;
  logic               fld_db_post_valid_i;
  logic               fld_db_post_ready_o;
  logic        [ 1:0] fld_db_post_op_i;
  // THREE BITS since packet C1. zhao_field_host_v2 decodes eight load kinds
  // and the doorbell now carries all three bits of them; the FH2 sub-decode
  // still reads the low two. See zhao_console_core.sv's port note.
  logic        [ 2:0] fld_db_post_kind_i;
  logic        [ 2:0] fld_db_post_slot_i;
  // EIGHT BITS since 2026-09-20 (packet D1). The mailbox's address field is
  // the FH2 control verb's and is deliberately wider than the loader's
  // LDADDRW = 7 (directive 10.2). A legacy LOAD that does not fit is refused
  // and counted, never narrowed.
  logic        [ 7:0] fld_db_post_addr_i;
  logic        [95:0] fld_db_post_data_i;
  logic        [31:0] fld_db_post_hash_i;
  logic               fld_db_post_ok_i;
  logic        [31:0] fld_db_post_ticket_i;
  logic               fld_db_ret_valid_o;
  logic               fld_db_ret_ready_i;
  logic        [31:0] fld_db_ret_ticket_o;
  logic        [ 1:0] fld_db_ret_op_o;
  logic               fld_db_ret_ok_o;
  logic               fld_db_ret_refused_o;
  logic               fld_db_ret_inserted_o;
  logic               fld_db_ret_evicted_o;
  logic        [ 2:0] fld_db_ret_slot_o;
  logic        [31:0] fld_db_ret_plan_o;
  logic        [31:0] fld_db_posts_o;
  logic        [31:0] fld_db_load_words_o;
  logic        [31:0] fld_db_lookups_o;
  logic        [31:0] fld_db_commits_o;
  logic        [31:0] fld_db_commits_refused_o;
  logic        [31:0] fld_db_post_stalls_o;
  logic        [31:0] fld_db_ret_overflow_o;
  // FH14 / FH2, packet D1. `ret_verdict_o` names WHICH refusal; `ret_handle_o`
  // is the generation-bearing binding the loader issues.
  logic        [ 3:0] fld_db_ret_verdict_o;
  logic        [31:0] fld_db_ret_handle_o;
  logic        [31:0] fld_db_fh2_posts_o;
  logic        [31:0] fld_db_addr_refused_o;

  // ---- THE FH2 TRANSACTIONAL LOADER (FH13 / FH15 / FH16) ------------------
  // Declared because `.*` binds by name and a port with no net is a compile
  // error. This bench does not exercise the install transaction -- that is
  // `field_loader_directed`'s job, with real packer bytes through a played
  // bridge; what the bench checks is that the composed machine comes out of
  // reset with the loader attached and publishing NOTHING, which is the
  // correct state before any capsule is offered.
  logic        [31:0] fld_ldr_stage_base_i;
  logic        [31:0] fld_ldr_stage_bytes_i;
  logic        [ 7:0] fld_ldr_pub_ready_o;
  logic        [ 7:0] fld_ldr_pub_pinned_o;
  logic        [31:0] fld_ldr_pub_handle_o;
  logic        [31:0] fld_ldr_pub_prog_hash_o;
  logic        [ 7:0] fld_ldr_pub_gen_o;
  logic        [31:0] fld_ldr_installs_ok_o;
  logic        [31:0] fld_ldr_installs_failed_o;
  logic        [31:0] fld_ldr_binds_ok_o;
  logic        [31:0] fld_ldr_binds_failed_o;
  logic        [31:0] fld_ldr_controls_ok_o;
  logic        [31:0] fld_ldr_bad_operation_o;
  logic        [31:0] fld_ldr_bad_envelope_o;
  logic        [31:0] fld_ldr_bad_range_o;
  logic        [31:0] fld_ldr_bad_section_o;
  logic        [31:0] fld_ldr_bad_crc_o;
  logic        [31:0] fld_ldr_bad_meta_o;
  logic        [31:0] fld_ldr_bridge_errs_o;
  logic        [31:0] fld_ldr_no_capacity_o;
  logic        [31:0] fld_ldr_evictions_o;
  logic        [31:0] fld_ldr_pin_forced_victim_o;
  logic        [31:0] fld_ldr_load_bytes_o;
  logic        [ 2:0] fld_stamp_slot_i;
  logic               fld_stamp_slot_valid_i;
  // The F profile's arm and its four program parameters (entry I5, R40).
  logic        [ 2:0] fld_flow_slot_i;
  logic               fld_flow_slot_valid_i;
  logic [127:0]       fld_flow_par_i;
  logic        [31:0] part_fld_samples_o;
  logic        [31:0] part_fld_bypassed_o;
  logic        [31:0] part_fld_noprog_o;
  logic        [31:0] part_fld_faults_o;
  logic        [31:0] part_fld_saturations_o;
  logic        [31:0] part_fld_stall_cycles_o;
  logic        [31:0] part_fld_rec_changed_o;
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
  logic        [31:0] fld_out_incomplete_o;
  // ---- zhao_field_host_v2's own evidence, new with the composition --------
  logic        [31:0] fld_prep_bad_o;
  logic        [31:0] fld_bad_image_o;
  logic        [31:0] fld_zero_mask_o;
  logic        [31:0] fld_late_write_o;
  logic        [31:0] fld_fence_writes_o;
  logic        [31:0] fld_uniform_runs_o;
  logic        [31:0] fld_credit_stall_o;
  logic        [31:0] fld_fast_path_o;
  logic        [31:0] fld_slow_path_o;
  logic        [31:0] fld_exec_desync_o;
  logic        [31:0] fld_bank_desync_o;
  logic        [31:0] fld_svc_bank_desync_o;
  logic        [31:0] fld_tag_mismatch_o;
  logic        [31:0] fld_wrong_op_o;
  logic        [31:0] fld_unsupported_o;
  logic        [31:0] fld_skid_overflow_o;
  logic        [31:0] fld_uniform_bad_o;
  // FOUR CAUSES since packet C1: {rcp0, sat_rescale, sat_mul, sat_add}.
  // Owner ruling R145 carried rcp0 out of the service path and
  // zhao_field_host_v2 reports it as its own family, not as a saturation.
  logic        [ 3:0] fld_sat_o;
  logic        [31:0] fld_pc_hits_o;
  logic        [31:0] fld_pc_misses_o;
  logic        [31:0] fld_pc_rejected_o;
  logic        [31:0] fld_pc_evictions_o;
  logic        [ 3:0] fld_pc_occupancy_o;
  logic               surf_res_valid_o;
  logic               surf_res_taken_o;   // the ACCEPT, exported 2026-09-21
  logic        [11:0] surf_res_texel_o;
  logic        [ 7:0] surf_res_tag_o;
  logic        [ 7:0] surf_res_strength_o;
  logic        [ 7:0] surf_res_before_o;
  logic        [31:0] surf_res_handle_o;   // OWNER RULING R231
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
  logic [31:0] cmd_exec_viewport_refused_o;
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

  // R229 (POSECMD): DrawPosedForm 0x0305's key and its two counters. The core
  // is bound with `.*`, so a port this bench does not DECLARE is not merely
  // unchecked -- verilation FAILS, and every form of this smoke stops running.
  // That is how these six were found: the ten-form sweep returned RC 1 in one
  // second flat, which is not a smoke run. Declared here in the same branch as
  // the ports.
  logic        cmd_draw_posed_o;
  logic [15:0] cmd_draw_clip_id_o;
  logic [15:0] cmd_draw_frame_no_o;
  logic [ 7:0] cmd_draw_sub_o;
  logic [31:0] cmd_exec_posed_draws_o;
  logic [31:0] cmd_exec_pose_clip_refused_o;

  // FORMOWN (owner ruling of 2026-09-21, section 3): the ACCEPTED job's 24-bit
  // form index and its lifetime, `zhao_geom_drawjob`'s `form_idx_q` exposed at
  // the core's edge. Declared here for the reason the six above were: the core
  // is bound with `.*`, so a port this bench does not DECLARE fails VERILATION
  // and every form of this smoke stops running -- which is what a one-second
  // RC 1 means and it is not a smoke run.
  logic        geom_job_valid_o;
  logic [23:0] geom_job_form_idx_o;
  // W04's twelve, declared for the SAME reason and found the same way: the
  // core is bound with `.*`, so an undeclared port is not an unchecked port --
  // verilation FAILS and every form of this smoke returns RC 1 in about a
  // second, which is the duration rather than the exit code giving it away.
  logic        cmd_draw_warp_en_o;
  logic [31:0] cmd_draw_warp_program_o;
  logic [31:0] cmd_draw_warp_time_o;
  logic [127:0] cmd_draw_warp_par_o;
  logic [127:0] cmd_draw_warp_attr_o;
  logic [31:0] cmd_draw_warp_attr_res_o;
  logic [ 7:0] cmd_draw_warp_attr_mode_o;
  logic signed [31:0] cmd_draw_warp_bx_o;
  logic signed [31:0] cmd_draw_warp_by_o;
  logic signed [31:0] cmd_draw_warp_bz_o;
  logic [31:0] cmd_exec_warp_draws_o;
  logic [31:0] cmd_exec_warp_draw_refused_o;

  // ---- GEOM.WARP, COMPOSED 2026-09-22 (packet WARPCOMP) ------------------
  // The block and its Field adapter are inside the core now, so every one of
  // their counters is a core port and `.*` needs a net for each. This bench
  // admits NO field program, so `fld_warp_slot_valid_i` is low below and the
  // lane is expected to take W09's bypass throughout -- the negative
  // assertions say so rather than leaving it to be assumed.
  logic [31:0] geom_warp_vertices_transformed_o;
  logic [31:0] geom_warp_bypassed_o;
  logic [31:0] geom_warp_app_saturations_o;
  logic [31:0] geom_warp_normal_reduced_o;
  logic [31:0] geom_warp_degenerate_o;
  logic [31:0] geom_warp_bound_violations_o;
  logic [31:0] geom_warp_negative_bounds_o;
  logic [31:0] geom_warp_normal_width_faults_o;
  logic [31:0] geom_warp_profile_mismatches_o;
  logic [31:0] geom_warp_field_faults_o;
  logic [31:0] geom_warp_p_accepts_o;
  logic [31:0] geom_warp_n_accepts_o;
  logic        geom_warp_poison_valid_o;
  logic [15:0] geom_warp_poison_src_id_o;
  logic [ 2:0] geom_warp_poison_cause_o;
  logic signed [31:0] geom_warp_poison_dx_o;
  logic signed [31:0] geom_warp_poison_dy_o;
  logic signed [31:0] geom_warp_poison_dz_o;
  logic [31:0] geom_warp_desc_allocated_o;
  logic [31:0] geom_warp_desc_hits_o;
  logic [31:0] geom_warp_desc_stale_o;
  logic [31:0] fld_warp_vertices_o;
  logic [31:0] fld_warp_identities_o;
  logic [31:0] fld_warp_bypassed_o;
  logic [31:0] fld_warp_noprog_o;
  logic [31:0] fld_warp_sig_refused_o;
  logic [31:0] fld_warp_faults_o;
  logic [31:0] fld_warp_absent_outputs_o;
  logic [31:0] fld_warp_stall_cycles_o;
  logic [31:0] fld_warp_vtx_changed_o;
  logic [ 2:0] fld_warp_slot_i;
  logic        fld_warp_slot_valid_i;
  logic [ 7:0] fld_warp_prog_profile_i;

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
  logic [31:0] part_prj_owner_unroutable_o;   // R68 sub-build 4
  logic [31:0] part_prj_ladder_unexpected_o;
  logic [31:0] part_lad_decisions_o;
  logic [31:0] part_lad_changes_o;
  logic [31:0] part_lad_held_o;
  logic [31:0] part_lad_gov_forced_o;
  logic [31:0] part_exp_polygons_o;
  // PART.CLIPFEED's evidence (owner ruling 1, 2026-09-22).
  logic [31:0] part_cf_particles_o, part_cf_triangles_o;
  logic [31:0] part_cf_range_refused_o, part_cf_stall_full_o;
  logic [31:0] part_cf_dq_refused_o, part_cf_dq_stray_o;
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
  // ALL FIFTY OF THEM LEFT THE CORE'S EDGE on 2026-09-22. The paragraph
  // above is preserved because its warning outlived the ports: a green smoke
  // run STILL does not prove the 2D path, and now the reason is different --
  // the descriptors have a producer, but this bench's command stream does
  // not carry a SetPlane or a DrawSprite. The path that does is
  // `tests/compositor/twod_cmd_chain_directed.cpp`, which drives real packet
  // bytes through CMD.DECODER/CMD.EXEC to composited pixels.
  logic         [31:0]                 twod_cmd_planes_staged_o;
  logic         [31:0]                 twod_cmd_sprites_staged_o;
  logic         [31:0]                 twod_cmd_plane_refused_o;
  logic         [31:0]                 twod_cmd_sprite_refused_o;
  logic         [31:0]                 twod_cmd_list_overflow_o;
  logic         [31:0]                 twod_cmd_packets_committed_o;
  logic         [31:0]                 twod_cmd_packets_abandoned_o;
  logic         [31:0]                 twod_cmd_frames_sealed_o;
  logic         [31:0]                 twod_cmd_planes_published_o;
  logic         [31:0]                 twod_cmd_sprites_published_o;
  logic         [31:0]                 twod_cmd_slots_auto_disabled_o;
  logic         [31:0]                 twod_cmd_bind_conflict_o;
  logic         [31:0]                 twod_cmd_seal_overrun_o;
  logic         [31:0]                 twod_asset_loads_started_o;
  logic         [31:0]                 twod_asset_loads_done_o;
  logic         [31:0]                 twod_asset_words_written_o;
  logic         [31:0]                 twod_asset_slot_refused_o;
  logic         [31:0]                 twod_asset_len_refused_o;
  logic         [31:0]                 twod_asset_addr_refused_o;
  logic         [31:0]                 twod_asset_epoch_refused_o;
  logic         [31:0]                 twod_asset_crc_fails_o;
  logic         [31:0]                 twod_asset_regions_zeroed_o;
  logic         [31:0]                 twod_asset_bridge_errs_o;
  logic         [31:0]                 twod_asset_loads_during_pass_o;
  logic         [31:0]                 twod_asset_bursts_o;
  logic         [31:0]                 cmd_exec_twod_planes_staged_o;
  logic         [31:0]                 cmd_exec_twod_sprites_staged_o;
  logic         [31:0]                 cmd_exec_twod_dropped_o;
  logic         [31:0]                 cmd_exec_twod_loads_issued_o;
  // `twod_sc_*` IS GONE FROM THE CORE'S EDGE (2026-09-21): the sprite colour
  // now has a consumer inside the core, which is what closed entry I17 item 1.
  logic         [31:0]                 twod_band_descriptors_o;
  logic         [31:0]                 twod_band_desc_overflow_o;
  logic         [31:0]                 twod_band_sprites_admitted_o;
  logic         [31:0]                 twod_band_sprites_refused_budget_o;
  logic         [31:0]                 twod_band_slices_emitted_o;
  logic         [31:0]                 twod_band_pixels_written_o;
  logic         [31:0]                 twod_band_pixels_clipped_o;
  logic         [31:0]                 twod_band_write_oob_o;
  logic         [31:0]                 twod_band_underrun_o;
  logic         [31:0]                 twod_band_scan_addr_mismatch_o;
  logic         [31:0]                 twod_band_tint_dropped_o;
  logic         [31:0]                 twod_band_blend_dropped_o;
  logic         [31:0]                 twod_band_order_inversion_o;
  logic         [31:0]                 twod_band_bands_o;
  logic         [31:0]                 twod_band_desc_mid_sweep_o;
  logic         [31:0]                 twod_plane_pixels_o;
  logic         [31:0]                 twod_plane_refused_role_o;
  logic         [31:0]                 twod_plane_refused_blend_o;
  logic         [31:0]                 twod_plane_skipped_view_o;
  logic         [31:0]                 twod_plane_wrap_fail_o;
  logic         [31:0]                 twod_plane_disabled_o;
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
`elsif ZHAO_MUT_UNTEX_DECL
  // The SECOND wrapper mutant, same shape (owner ruling R197): it sets
  // GEOM_REPLAY_UNTEX_DECL to ONE, so every triangle GEOM.REPLAY presents
  // DECLARES it has no texture coordinates while the smoke's material takes
  // a sample -- the one combination the untextured door must refuse. Its
  // header carries the argument; this bench asserts `geom_untex_refused_o`
  // reaches the reference's replayed count and NOTHING enters GEOM.CLIP.
  zhao_console_core_untex_decl_mutant dut (.*);
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
      // THE ACCEPT, NOT THE OFFER. `surf_res_ready_i` left this edge when
      // TERRAIN.SHEETSEAM became the consumer; `surf_res_taken_o` is the core's
      // own `valid && ready` in its place, so this count still measures a beat
      // that MOVED. Counting `surf_res_valid_o` alone would have turned the
      // `records == texels_touched` check below into a check of the offer,
      // which is a gate quietly losing its meaning.
      if (surf_res_taken_o)
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
  // D0: the plan's epoch identity. Trace only -- it rides every return so a
  // return can be tied to the plan that produced it, and this bench checks
  // nothing about its value. A recognisable constant is easier to see in a
  // waveform than a zero that could be a floating net.
  assign fld_cfg_plan_base_i    = 32'hF1E1_D000;
  // LOW: ask for the fast path. It is a REQUEST and not a permission -- the
  // host gates the no-clear path on the image's INIT_PROOF (`hdr_ipok`) and
  // NOT on this bit being low, so a bench cannot reach the fast path by
  // driving this. Low is therefore the honest default: it says "this bench
  // does not force the legacy walk", which is true, rather than asserting
  // anything about which path a point takes. This bench admits no field
  // program at all, so neither path is exercised and `fld_fast_path_o` and
  // `fld_slow_path_o` are both expected to stay at zero -- see the negative
  // assertions below, which say so rather than leaving it to be assumed.
  assign fld_cfg_slow_clear_i   = 1'b0;
  assign fld_stamp_slot_i       = 3'd0;
  assign fld_stamp_slot_valid_i = 1'b0;
  // GEOM.WARP's Field binding. LOW for the same reason the stamp's is: this
  // bench admits no field program, so there is no resident Warp slot to name,
  // and a number here would be this bench asserting a residency it did not
  // create. The profile id is the W profile's own (`field-ir.md` 7.1: earth
  // 0, warp 1) so that `geom_warp_profile_mismatches_o` staying at zero means
  // "no mismatch" rather than "no comparison" -- a zero from a check that
  // never ran is the reading this campaign exists to remove.
  assign fld_warp_slot_i        = 3'd0;
  assign fld_warp_slot_valid_i  = 1'b0;
  assign fld_warp_prog_profile_i = 8'd1;
  assign fld_db_ret_ready_i     = 1'b1;
  assign fld_db_post_kind_i     = 3'd0;
  assign fld_db_post_addr_i     = 8'd0;
  // THE LOADER'S STAGING WINDOW. A real base and a real extent rather than
  // zero: a zero-length window would make every install refuse for the same
  // reason, and a bench that cannot tell "refused because the window is empty"
  // from "refused because the capsule is bad" is not watching anything. This
  // bench offers no capsule, so nothing installs -- what it checks is that the
  // composed loader comes out of reset publishing NOTHING.
  assign fld_ldr_stage_base_i   = 32'h1000_0000;
  assign fld_ldr_stage_bytes_i  = 32'h0010_0000;
  assign fld_db_post_data_i     = 96'd0;

  // ---- THE F PROFILE IS ARMED AT A SLOT NOTHING WAS LOADED INTO ------------
  // Entry I5's closure, exercised the only way a smoke bench honestly can.
  // Arming it means EVERY particle offered walks the whole join: PART.STATE's
  // offer, the adapter's request, the arbiter's grant, the `hdr_loaded`
  // interlock, the shared response bus, the status, and the answer joined back
  // to the record it belongs to. LOADING a program instead would mean choosing
  // one, and a composed engine that returns a plausible number is exactly what
  // a smoke bench cannot tell from a composed engine returning a plausible
  // number for the wrong reason. A refusal has a named status and a counter.
  assign fld_flow_slot_i       = 3'd0;
  assign fld_flow_slot_valid_i = 1'b1;
  assign fld_flow_par_i        = 128'd0;

  // ---- THE TWO THINGS THIS BENCH ASKS THE DOORBELL -------------------------
  // Entry I42's closure. Both posts are answered, and both answers can only
  // come back if the whole path exists -- the core's port, the mailbox, the
  // directory, the return queue and the ticket.
  //
  //   post 0  a LOOKUP of a hash nothing was ever loaded. The right answer is
  //           ok=1, inserted=0: the directory ANSWERED and the hash is not
  //           resident. Those two bits apart is what separates "not resident"
  //           from "nobody asked", which is the silence ruling R20 forbids.
  //   post 1  a COMMIT for slot 0, whose HEADER was never written. The right
  //           answer is REFUSED -- the directory never sees the hash -- and it
  //           fires `fld_db_commits_refused_o`, which is this bench's proof
  //           that the order law is a guard and not a paragraph.
  //
  // The tickets are distinct so the returns cannot be confused for each other.
  localparam logic [31:0] FLD_TICKET_LU = 32'hFD10_0001;
  localparam logic [31:0] FLD_TICKET_CM = 32'hFD10_0002;

  logic [1:0]  fld_probe_step_q;      // 0 post LU, 1 post CM, 2 done
  logic [31:0] fld_ret_lu_ok_q;
  logic [31:0] fld_ret_lu_hit_q;
  logic [31:0] fld_ret_cm_refused_q;
  logic [31:0] fld_ret_seen_q;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      fld_db_post_valid_i  <= 1'b0;
      fld_db_post_op_i     <= 2'd0;
      fld_db_post_slot_i   <= 3'd0;
      fld_db_post_hash_i   <= 32'd0;
      fld_db_post_ok_i     <= 1'b0;
      fld_db_post_ticket_i <= 32'd0;
      fld_probe_step_q     <= 2'd0;
      fld_ret_lu_ok_q      <= 32'd0;
      fld_ret_lu_hit_q     <= 32'd0;
      fld_ret_cm_refused_q <= 32'd0;
      fld_ret_seen_q       <= 32'd0;
    end else begin
      if (fld_db_post_valid_i && fld_db_post_ready_o) begin
        fld_db_post_valid_i <= 1'b0;
        fld_probe_step_q    <= fld_probe_step_q + 2'd1;
      end else if (!fld_db_post_valid_i && (fld_probe_step_q != 2'd2)) begin
        fld_db_post_valid_i <= 1'b1;
        if (fld_probe_step_q == 2'd0) begin
          fld_db_post_op_i     <= 2'd2;                  // LOOKUP
          fld_db_post_hash_i   <= 32'hFEED_BEEF;
          fld_db_post_slot_i   <= 3'd0;
          fld_db_post_ok_i     <= 1'b0;
          fld_db_post_ticket_i <= FLD_TICKET_LU;
        end else begin
          fld_db_post_op_i     <= 2'd1;                  // COMMIT, no header
          fld_db_post_hash_i   <= 32'hFEED_BEEF;
          fld_db_post_slot_i   <= 3'd0;
          fld_db_post_ok_i     <= 1'b1;
          fld_db_post_ticket_i <= FLD_TICKET_CM;
        end
      end

      // The returns, latched by TICKET rather than by arrival order: an order
      // assumption is exactly the thing a join gets wrong, and the ticket is
      // the field that cannot be right by accident.
      if (fld_db_ret_valid_o && fld_db_ret_ready_i) begin
        fld_ret_seen_q <= fld_ret_seen_q + 32'd1;
        if (fld_db_ret_ticket_o == FLD_TICKET_LU) begin
          if (fld_db_ret_ok_o)       fld_ret_lu_ok_q  <= fld_ret_lu_ok_q + 32'd1;
          if (fld_db_ret_inserted_o) fld_ret_lu_hit_q <= fld_ret_lu_hit_q + 32'd1;
        end
        if ((fld_db_ret_ticket_o == FLD_TICKET_CM) && fld_db_ret_refused_o) begin
          fld_ret_cm_refused_q <= fld_ret_cm_refused_q + 32'd1;
        end
      end
    end
  end

  // I32 CLOSED 2026-09-21. TERRAIN.BAKE IS COMPOSED, so this bench no longer
  // plays its consumer: `res_ready` comes from `zhao_terrain_sheetseam`'s
  // `before`-plane sink inside the core, which is constant high by that
  // block's own decision -- the stamp is the player's own action and a seam
  // that backpressured it would drop frames to protect a bake. The paragraph
  // that stood here warned that a LOW ready would stall `s2_accept` and read
  // both texel counters as zero; that hazard is unchanged and is now the
  // core's to honour rather than the bench's. `surf_res_taken_o` is the ACCEPT
  // the core exports in the input's place, and the check at the end of this
  // bench counts it rather than the offer.

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
  // HOW MANY TIMES THE SET IS SUBMITTED. TWO since 2026-09-25 (TERRAINAUX).
  // `zhao_terrain_seq` walks a submitted set ONCE and SKIPS the compose issue
  // for any patch that is not yet resident, so a single submission pages the
  // patches in and composes NOTHING -- measured, `seq skipped=3 issued=0`.
  // A real frame loop submits every frame; this is the second frame, and it
  // is what turns the compose cache from POISON into a served patch and the
  // terrain triangles from 128-of-128 degenerate into 0-of-256.
  localparam int unsigned N_TERR_SUBMITS = 2;
  localparam int unsigned LIST_BYTES_C = N_TERR_REC * 32;
  localparam int unsigned PAGE0_OFF_C  = 4096;
  // THE WINDOW IS DERIVED, NOT TYPED. It was a hand-written 64 KiB while the
  // records shared one page; a per-record page makes the arena a function of
  // N_TERR_REC, and a constant somebody has to remember to raise alongside it
  // is a constant that will be wrong. The last page ends exactly at the arena
  // end, which TERRAIN.PAGELOADER allows: its test is `src_hi > arena_hi`.
  localparam int unsigned ARENA_BYTES_C = PAGE0_OFF_C + N_TERR_REC * PAGE_BYTES_C;
  localparam int unsigned HPS_WORDS     = (ARENA_BYTES_C + 7) / 8;

  // THE PAGE CRC WINDOW, from `spec/terrain_rules.md` 2.1 line 144: the
  // `page_crc32c` at header +32 covers bytes [64, 21320) -- the BODY, never the
  // header. That is why the CRC can be written INTO the header after it is
  // computed without invalidating itself, and it is the same window
  // `zhao_terrain_pageloader`'s CRC_LO/CRC_HI parameters carry (its lines
  // 106-107). Named here rather than open-coded so the two cannot drift apart
  // silently -- a bench CRC window one beat different from the loader's is the
  // "wrong CRC over a page that is otherwise perfect" defect the loader's own
  // header calls the worst shape.
  localparam int unsigned PAGE_CRC_LO_C = 64;
  localparam int unsigned PAGE_CRC_HI_C = 21320;

  logic [63:0] hps_mem [0:HPS_WORDS-1];

  // The body CRC of each played page, computed at time zero from the bytes
  // actually in `hps_mem` (see the layout loop) and then written into BOTH the
  // page header's +32 word and the T5 record's `expected_page_crc32c`, because
  // TERRAIN.PAGELOADER checks the page against both (`CHECK_HEADER_CRC` is 1).
  logic [31:0] page_crc_q [0:N_TERR_REC-1];

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

  // ---- THE LOOM NODE STREAM, in the played DDR (entry I50 closed) --------
  // SW.STREAM's staged stream, at `design/contracts/GEOM.LOOM.STREAM.md`'s
  // frozen 64-byte record. The bench is the ARM, so it owns this region and
  // writes it before the machine starts; after that every node GEOM.LOOM
  // composes has come OUT of this array through the bridge socket.
  // 0x3200_0000, above the particle buffers, so `hps_region` separates them.
  localparam logic [31:0] LOOM_HPS_BASE  = 32'h3200_0000;
  localparam int unsigned LOOM_HPS_WORDS = 16;   // two 64-byte records
  logic [63:0] loom_mem [0:LOOM_HPS_WORDS-1];

  function automatic bit in_loom_region(input logic [31:0] a);
    return (a >= LOOM_HPS_BASE) && (a < LOOM_HPS_BASE + 32'(LOOM_HPS_WORDS * 8));
  endfunction

  function automatic logic [63:0] hps_read(input logic [31:0] byte_addr);
    int unsigned w;
    if (in_part_region(byte_addr)) return part_mem[(byte_addr - PART_HPS_BASE0) >> 3];
    if (in_loom_region(byte_addr)) return loom_mem[(byte_addr - LOOM_HPS_BASE) >> 3];
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
  localparam int unsigned PKT_MAX_C     = 768;   // records + header + CRC; the `ro` accumulator below is the arithmetic (SetView 112 B since R63)
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
  // The shell's and the core's paths: EVERY wrapper-mutant build's DUT is a
  // wrapper, so every such define must be listed here -- a mutant added
  // without this line fails 47 hierarchical probes at elaboration and looks
  // like a broken core (found 2026-09-20 adding the R197 wrapper).
`ifdef ZHAO_MUT_SLOT_OVERFLOW
  `define PC_CORE dut.u_dut
`elsif ZHAO_MUT_UNTEX_DECL
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
  logic [255:0] upl_rec1;         // record 1 -- the one the MESHLET names
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
    if (in_loom_region(a)) return 5;
    if ((a >= HPS_BASE) && (a < HPS_BASE + 32'(HPS_WORDS * 8))) return 4;
    return 0;   // 5 is the loom stream; it reads through `hps_read` like 4
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
  // ==========================================================================
  // THE TEXTURE PAGE: BOUND, SEALED AND SERVED (entry I49's second half)
  // ==========================================================================
  // Until 2026-09-20 every one of the V3 programming channels was held at zero
  // in this bench and the island could not have sampled whatever the flat
  // request said. These are BOUNDARY ports of `zhao_console_core` -- the V3
  // binding page and the texture fill socket -- so driving them here is
  // stimulus at an edge, which is what a bench is for, and NOT a producer
  // invented inside the console.
  //
  // THE ROW IS A DIRECT FORMAT ON PURPOSE. `binding_row_legal` REQUIRES a
  // direct row's {palette_generation, palette_slot} to be zero, so the two
  // witnesses the console publishes are the page's own LAW rather than a value
  // anybody chose -- and no palette has to be resident for a texel to arrive.
  // A CLUT row would need a palette identity that nothing in this console
  // produces (FINDINGS-texmat2, owner decision D2).
  //
  // THE SEAL IS NOT HAND-ROLLED. 32'hB37C_0807 is
  // `zhao_binding_seal::page_crc(1, {selector 3 = this row}, ...)` from
  // `tests/harness/zhao_binding_seal.hpp`, the tree's ONE model of the fold --
  // the header exists precisely so a second test does not write the CRC again.
  // A wrong seal is LOUD, not silent: the page is refused with CFG_BAD_CRC and
  // the status check below fires.
  localparam logic [ 7:0] TEX_PAGE_GEN_C = 8'd1;
  localparam logic [ 7:0] TEX_SELECTOR_C = 8'd3;
  localparam logic [31:0] TEX_BASE_C     = 32'h0000_2000;
  // fmt 1 (RGB565), filter 0, wrap 0/0, log2w 4, log2h 4, max_level 0, mip 0
  localparam logic [31:0] TEX_MODE_C     = 32'h0000_4401;
  localparam logic [74:0] TEX_ROW_C      = {11'h400, TEX_MODE_C, TEX_BASE_C};
  localparam logic [31:0] TEX_SEAL_C     = 32'hB37C_0807;
  localparam logic [15:0] TEX_TEXEL_C    = 16'h07E0;   // saturated green, RGB565

  localparam int unsigned TBS_BEGIN  = 0;
  localparam int unsigned TBS_ROW    = 1;
  localparam int unsigned TBS_END    = 2;
  localparam int unsigned TBS_ACTIVE = 3;
  int unsigned  tbind_st_q;
  logic         tbind_sent_q;
  logic [3:0]   tbind_status_q;
  logic [31:0]  tbind_acks_q;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      tbind_st_q            <= TBS_BEGIN;
      tbind_sent_q          <= 1'b0;
      tbind_status_q        <= 4'hF;
      tbind_acks_q          <= 32'd0;
      cfg_valid_i           <= 1'b0;
      cfg_op_i              <= 2'd0;
      cfg_page_generation_i <= 8'd0;
      cfg_selector_i        <= 8'd0;
      cfg_row_i             <= 75'd0;
      cfg_crc32_i           <= 32'd0;
      cfg_rsp_ready_i       <= 1'b1;
    end else begin
      if (cfg_valid_i && cfg_ready_o) begin
        cfg_valid_i  <= 1'b0;
        tbind_sent_q <= 1'b1;
      end
      if (cfg_rsp_valid_o && cfg_rsp_ready_i) begin
        tbind_status_q <= cfg_rsp_status_o;
        tbind_acks_q   <= tbind_acks_q + 32'd1;
        tbind_sent_q   <= 1'b0;
        // A BAD STATUS STOPS THE WALK rather than driving on: the next
        // command would be refused for a second reason and the first one
        // would be lost. The check at the end of the run reads the status.
        if (cfg_rsp_status_o == 4'd0 && tbind_st_q < TBS_ACTIVE)
          tbind_st_q <= tbind_st_q + 1;
        else if (cfg_rsp_status_o != 4'd0)
          tbind_st_q <= TBS_ACTIVE;
      end
      if (!cfg_valid_i && !tbind_sent_q) begin
        case (tbind_st_q)
          TBS_BEGIN: begin
            cfg_valid_i <= 1'b1; cfg_op_i <= 2'd0;
            cfg_page_generation_i <= TEX_PAGE_GEN_C;
            cfg_selector_i <= 8'd0; cfg_row_i <= 75'd0; cfg_crc32_i <= 32'd0;
          end
          TBS_ROW: begin
            cfg_valid_i <= 1'b1; cfg_op_i <= 2'd1;
            cfg_page_generation_i <= TEX_PAGE_GEN_C;
            cfg_selector_i <= TEX_SELECTOR_C; cfg_row_i <= TEX_ROW_C;
            cfg_crc32_i <= 32'd0;
          end
          TBS_END: begin
            cfg_valid_i <= 1'b1; cfg_op_i <= 2'd2;
            cfg_page_generation_i <= TEX_PAGE_GEN_C;
            cfg_selector_i <= 8'd0; cfg_row_i <= 75'd0;
            cfg_crc32_i <= TEX_SEAL_C;
          end
          default: begin end
        endcase
      end
    end
  end

  // THE FILL SOCKET. `zhao_texture_cache_pipe_v2`'s protocol, followed to the
  // letter: the request is accepted on one edge, the eight 16-bit beats begin
  // on the NEXT one (presenting data in the offer cycle is a protocol fault the
  // block counts), and a line is exactly eight beats with no partial refusal.
  // Every texel is the same green, because what is being proven here is that a
  // texel ARRIVES -- the value it carries is the island's own directed tests'
  // business and re-checking it here would be a second opinion.
  logic       tfill_busy_q;
  logic [3:0] tfill_beat_q;
  logic [31:0] tfill_lines_q, tfill_beats_q;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      tfill_busy_q      <= 1'b0;
      tfill_beat_q      <= 4'd0;
      tfill_lines_q     <= 32'd0;
      tfill_beats_q     <= 32'd0;
      fill_req_ready_i  <= 1'b1;
      fill_data_valid_i <= 1'b0;
      fill_data_i       <= 16'd0;
      fill_refused_i    <= 1'b0;
    end else begin
      fill_data_valid_i <= 1'b0;
      if (!tfill_busy_q) begin
        if (fill_req_valid_o && fill_req_ready_i) begin
          tfill_busy_q     <= 1'b1;
          tfill_beat_q     <= 4'd0;
          tfill_lines_q    <= tfill_lines_q + 32'd1;
          fill_req_ready_i <= 1'b0;
        end
      end else begin
        fill_data_valid_i <= 1'b1;
        fill_data_i       <= TEX_TEXEL_C;
        tfill_beats_q     <= tfill_beats_q + 32'd1;
        if (tfill_beat_q == 4'd7) begin
          tfill_busy_q     <= 1'b0;
          tfill_beat_q     <= 4'd0;
          fill_req_ready_i <= 1'b1;
        end else begin
          tfill_beat_q <= tfill_beat_q + 4'd1;
        end
      end
    end
  end
  // ---- THE MATERIAL RESOLVE, OBSERVED (entry I49, CLOSED 2026-09-20) -------
  // The bench no longer issues the request. `u_material_window` does, from the
  // triangle's own {material_set, material_id, semantic weight} -- the DRAW's
  // handle, carried with the meshlet. So all four seams are now real here: the
  // DIRECTORY (MEM.UPLOAD's publication), the FETCH (requester C through the
  // real MEM.GUARD), the REQUEST and the RESPONSE. What this block does is
  // WATCH, which is the only thing a bench should do at a closed seam.
  logic         mat_rsp_v_q;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) mat_rsp_v_q <= 1'b0;
    else        mat_rsp_v_q <= mat_rsp_valid_o;
  end
  logic         mat_fired_q;
  int unsigned  mat_rsp_seen_q;
  logic [2:0]   mat_status_seen_q;
  logic         mat_rec_has_q;
  logic [255:0] mat_rec_q;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      mat_fired_q            <= 1'b0;
      mat_rsp_seen_q         <= 0;
      mat_status_seen_q      <= 3'd7;
      mat_rec_has_q          <= 1'b0;
      mat_rec_q              <= '0;
    end else begin
      // THE RESPONSE, OBSERVED ON ITS RISING EDGE. The window is ready only
      // in the clock it takes the answer and leaves ST_WAIT on the same edge,
      // so a bench that waited to see valid AND ready on a later clock would
      // never see the acceptance at all -- the same trap the window's own
      // directed test records.
      if (mat_rsp_valid_o && !mat_rsp_v_q) begin
        mat_fired_q       <= 1'b1;
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

  // ---- GEOM.BINNER's COUNTERS, CAUGHT IN THE COMPOSED CONSOLE (GIANTREFS) --
  // Entry I56's item (5): "whatever counter the reservation exports must be
  // READ BY AN ASSERTION IN THE COMPOSED SMOKE, not merely connected." The
  // binner's instruments were connected at the leaf, proven to fire at the
  // leaf, and thrown into wires named `_unused` by the shell -- so the console
  // could not say a frame had been truncated nor by how much.
  //
  // They are now DEBUG.COUNTERS providers at the ids `design/blocks.yml` gives
  // GEOM.BINNER, and this is the reader. The sweep streams (id, u64) ascending
  // once per frame tick; these registers latch the two ids this packet cares
  // about out of that stream, off the console's REAL top ports.
  localparam logic [15:0] SMOKE_CNT_TILE_REFERENCES     = 16'd18;
  localparam logic [15:0] SMOKE_CNT_MAX_TILE_LIST_DEPTH = 16'd19;

  logic [63:0] cnt_tile_refs_q, cnt_tile_depth_q;
  logic        cnt_tile_refs_seen_q, cnt_tile_depth_seen_q;
  int unsigned cnt_beats_seen_q;

  always @(posedge gpu_clk) begin
    if (!rst_n) begin
      cnt_tile_refs_q       <= 64'd0;
      cnt_tile_depth_q      <= 64'd0;
      cnt_tile_refs_seen_q  <= 1'b0;
      cnt_tile_depth_seen_q <= 1'b0;
      cnt_beats_seen_q      <= 0;
    end else if (cnt_snap_valid_o && cnt_snap_ready_i) begin
      cnt_beats_seen_q <= cnt_beats_seen_q + 1;
      if (cnt_snap_id_o == SMOKE_CNT_TILE_REFERENCES) begin
        cnt_tile_refs_q      <= cnt_snap_value_o;
        cnt_tile_refs_seen_q <= 1'b1;
      end
      if (cnt_snap_id_o == SMOKE_CNT_MAX_TILE_LIST_DEPTH) begin
        cnt_tile_depth_q      <= cnt_snap_value_o;
        cnt_tile_depth_seen_q <= 1'b1;
      end
    end
  end

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
  // ==========================================================================
  // -GlowTag: THE FIRST LIT FRAGMENT THE COMPOSED CONSOLE HAS EVER CARRIED
  // ==========================================================================
  // Added 2026-09-21 (tagprod). POSTGATHER composed POST.GATHER and its smoke
  // read `frags=2560 [untagged=2560 below_knee=0 lit=0 reserved=0]` -- CORRECT,
  // because the effect tag is `tri_continuation_tail_i[15:8]` and that port is
  // entry I20's open boundary, driven `'0` by this bench. So R195's law was
  // verified EXHAUSTIVELY at block level (2^24 pairs) and the SEAM was verified
  // in the console, and NO TEST IN THIS TREE DID BOTH AT ONCE. A green smoke on
  // an untagged stream is not evidence about bloom.
  //
  // This form is that evidence. It changes TWO FIELDS OF ONE BOUNDARY PORT and
  // nothing else in the whole bench -- which is the point: if the console were
  // not really carrying the tail, changing it could not change the picture.
  //
  // WHY A BENCH MAY DRIVE THIS AND THE COMPOSER MAY NOT. Driving a boundary
  // port from a testbench is STIMULUS; driving it from `zhao_console_core.sv`
  // would be a TIE-OFF moved inward. Entry I20 says so and packet FORGESHADOW
  // refused exactly that on 2026-09-20. Nothing here closes I20; it makes the
  // gap's CONSEQUENCE measurable, so the day a producer exists the evidence
  // path is already in place.
  //
  // ---- the tag, and why 8'h7F is not a plausible number --------------------
  // `spec/stars_and_flares.md` 1, FROZEN: `tag = (channel << 6) | strength`,
  // GLOW = 2'b01. R195: `kGlowKnee` 24, `kGlowSlope` 0x1C. Strength 63 is the
  // largest the field holds, so `glow_gain(63) = ((63-24)*28)>>4 = 68` is the
  // law's own MAXIMUM gain -- the value furthest from the knee, which is what a
  // control wants. 8'h7F = {2'b01, 6'd63}.
  localparam logic [7:0] SMK_GLOW_TAG_C = 8'h7F;
  // ---- the colour, and why it is NOT white ---------------------------------
  // The glow BORROWS the fragment's own resolved colour (R195 decision 1), so a
  // BLACK fragment has a black halo and lights nothing. The bench's raster
  // pixels WERE black only because this same port's `vertex_rgb` field is zero:
  // with FRAGMENT STATE 0, `zhao_raster_fragment.sv`'s SHADE_MOD is off and
  // `s1_src_rgb_r <= s0_vrgb_r` -- the vertex colour IS the pixel. So the tag
  // alone would have produced `lit=2560` and a plane of zeros, which is a
  // counter moving and a picture that cannot change. Both halves are set.
  //
  // ---------------------------------------------------------------------------
  // AND AS OF 2026-09-21 THE COLOUR NO LONGER ARRIVES, WHICH IS THE POINT
  // ---------------------------------------------------------------------------
  // Owner decision R234 D1 reconnected the lit per-vertex colour:
  // `zhao_raster_tile_pipe_v2` now OVERWRITES `continuation_w.post_earlyz
  // .vertex_rgb` per fragment from attribute lanes 3..5, so the tail's bits are
  // dead from that block onward. This form still writes `SMK_GLOW_RGB_C` onto
  // the tail, and the assertion on `smk_glow_px_q` is INVERTED rather than
  // deleted: it must now come out ZERO in every form. That is D1's severance of
  // the stand-in, measured from the other side -- if one word came out 0xB556
  // the tail would still be reaching the fragment shader and the reconnection
  // would be a comment rather than a change.
  //
  // The fragments are not black any more either, so the glow has a real colour
  // to borrow. What it borrows is now GEOM.LIGHT's own output, interpolated, so
  // it is NOT a value this bench can predict -- `smk_lit_px_q` below counts
  // COLOURED pixels instead of pixels of a chosen colour. That is the same
  // correction the sentinel-overwrite count already had to make once: the
  // predicted constant was the defect.
  //
  // THREE CONSTRAINTS PICKED THIS VALUE, and each one would have produced a
  // flaky or blind test on its own:
  //  1. IT SHOULD SURVIVE THE ORDERED DITHER. `resolve.cpp`'s law is
  //     `r5 = min(31, (r*31 + (B*16+8))/255)` over sixteen Bayer phases, and
  //     181*31 mod 255 = 1, 170*63 mod 255 = 0 -- both floor to the same index
  //     at every phase on paper (r5=22, g6=42, b5=22, i.e. 0xB556).
  //     MEASURED, and the arithmetic holds: exactly 1,062 snapshot words are
  //     0xB556 and every one of the other 1,498 drawn words is 0x0000. What
  //     was wrong was the SENTINEL-OVERWRITE count's use of it -- see below --
  //     not the value.
  //  2. IT MUST NOT COLLIDE WITH THE SENTINEL. The frame is pre-filled with
  //     `16'((w*40503) ^ 32'h5A3C) | 16'h0001`, which is ALWAYS ODD, and every
  //     channel of this colour lands on an even 565 word -- so a drawn pixel
  //     can never be mistaken for an undrawn one.
  //  3. IT MUST NOT SATURATE. White would have made the bloom invisible -- add
  //     anything to 0xFFFF and it is still 0xFFFF -- which is the crayon-grain
  //     failure from CLAUDE.md's art chapter: mathematically present, visually
  //     nothing, and it would have read as "the glow does not reach the frame".
  localparam logic [23:0] SMK_GLOW_RGB_C   = 24'hB5_AA_B5;   // (181,170,181)
  localparam logic [15:0] SMK_GLOW_PX565_C = 16'hB556;       // its dithered resolve
  logic [15:0] post_fb_snap [0:POST_TB_WORDS-1];
  // How many words of the RASTER's frame carry the -GlowTag colour. Counted in
  // the post block and read by the gather block much later, so it lives here.
  // It is the bench's OWN, independent count of how many pixels the
  // continuation tail reached, and the whole point is that it is arrived at by
  // walking memory rather than by reading a counter the DUT maintains.
  int unsigned smk_glow_px_q;
  // How many words of the RASTER's frame carry ANY colour -- drawn (not the
  // sentinel) and not black. Before R234 D1 this was structurally zero in every
  // form but -GlowTag, because `vertex_rgb` was a boundary constant at '0 and
  // fragment state 0 makes the vertex colour the pixel. It is now the count of
  // covered pixels carrying GEOM.LIGHT's interpolated output, and it is the one
  // number in this bench that says the Gouraud delivery is live END TO END in
  // the composed console rather than in a directed harness.
  int unsigned smk_lit_px_q;
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
  // The shell's path: every wrapper-mutant build's DUT is a wrapper around
  // the core (same list as `PC_CORE` above; keep the two in step).
`ifdef ZHAO_MUT_SLOT_OVERFLOW
  `define PC_SHELL dut.u_dut.u_shell
`elsif ZHAO_MUT_UNTEX_DECL
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
      geom_loom_db_post_valid_i <= 1'b0;
      geom_loom_sent_q          <= 1'b0;
      loom_ret_seen_q           <= 1'b0;
      loom_ret_ticket_q         <= 32'd0;
      loom_ret_ok_q             <= 1'b0;
      loom_ret_refused_q        <= 1'b0;
      loom_ret_reason_q         <= 3'd0;
      loom_ret_loom_reason_q    <= 3'd0;
      loom_ret_nodes_q          <= 16'd0;
      loom_ret_plan_q           <= 32'd0;
    end else begin
      if (geom_loom_db_post_valid_i && geom_loom_db_post_ready_o) begin
        geom_loom_db_post_valid_i <= 1'b0;
        geom_loom_sent_q          <= 1'b1;
      end else if (reset_released_q && !geom_loom_sent_q) begin
        geom_loom_db_post_valid_i <= 1'b1;
      end
      // The ARM's side of D2. LATCHED, because the return is a one-cycle
      // handshake and the checks run long after the frame -- a check that
      // sampled the live port would read zero and call it a failure.
      if (geom_loom_db_ret_valid_o && geom_loom_db_ret_ready_i) begin
        loom_ret_seen_q        <= 1'b1;
        loom_ret_ticket_q      <= geom_loom_db_ret_ticket_o;
        loom_ret_ok_q          <= geom_loom_db_ret_ok_o;
        loom_ret_refused_q     <= geom_loom_db_ret_refused_o;
        loom_ret_reason_q      <= geom_loom_db_ret_reason_o;
        loom_ret_loom_reason_q <= geom_loom_db_ret_loom_reason_o;
        loom_ret_nodes_q       <= geom_loom_db_ret_nodes_o;
        loom_ret_plan_q        <= geom_loom_db_ret_plan_o;
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
  // THE IMAGE IT SERVES IS ZEROS, AND AS OF 2026-09-20 IT IS REACHED.
  // WHAT THIS PARAGRAPH USED TO SAY, struck because the stimulus moved under
  // it: "Every page in this run fails its CRC (that is asserted, at
  // `terr_res_crc_failures_o == N_TERR_REC`), so no page reaches
  // RESIDENT_CLEAN, TERRAIN.SEQ issues no patch, and the compose engine sits
  // idle." The pages now carry a spec 2.1 header and their own body CRC and
  // they LOAD (see the arena layout loop and the R70 chain check in the terrain
  // verdict). The BODY is still all zeros, so the lattice this window serves is
  // a flat zero height field -- which is why this model is still not evidence
  // about composed terrain VALUES, only about the path reaching it. The
  // engine's own evidence is
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
    // (I34's `terr_pt_fld_valid_i`/`_height_i` were driven low here until
    // 2026-09-23. They are no longer this bench's to drive: the lane has a
    // producer inside the core. The property they asserted -- section 3.4 with
    // an empty program list -- is now asserted directly, on
    // `fld_earth_records_o`, at the end of the run.)
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
    terr_cc_serve_release_i = '0;
    // (I32: layer D's writer and I27's deformation mark are both INSIDE the
    // core from 2026-09-21, so there is nothing here to hold at zero. The
    // plane still reads SOLID through this run for a different reason: no
    // page this bench loads passes the header identity check, so no bake
    // record ever gets a page identity and none is ever issued.)
    // TERRAIN.WRITEBACK IS COMPOSED (entry I28 closed) and this bench STILL
    // CANNOT REACH IT, for a reason that CHANGED on 2026-09-21 and is worth
    // stating rather than inheriting: the mark is no longer a harness input
    // that is simply never raised -- `zhao_terrain_pageio` raises it, and
    // `dm_f_o` is low on a bake anyway because a bake dirties B and D and does
    // not touch the F sheet. What still stops a dirty-F eviction here is the
    // older half: no page this bench loads passes the header identity check,
    // so none becomes resident to be dirtied at all. So
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
    terr_uv_ready_i    = 1'b1;
    render_frame_open_q = 1'b0;
    // The fourteen `geom_pose_*_i` initialisers that stood here are gone with
    // the ports: I29 closed and the decoder's source is `zhao_geom_bonesrc`,
    // filled by `zhao_geom_clipread` out of the asset pool.
    proj_cfg_we_i = '0;
    proj_cfg_view_i = '0;
    proj_cfg_addr_i = '0;
    proj_cfg_data_i = '0;
    proj_en_i = '0;
    terr_job_view_mask_i = '0;
    terr_sparse_fill_i = '0;
    proj_out_ready_i = '0;
    post_atm_en_i = '0;
    post_atm_valid_i = '0;
    post_atm_rgb_i = '0;
    post_atm_opacity_i = '0;
    post_atm_add_i = '0;
    // The four `hist_ev_*` drives that stood here are GONE with the ports:
    // entry I18 closed and the histogram's events come from TERRAIN.LODFEED
    // inside the core. There is nothing for this bench to initialise.
    cfg_valid_i = '0;
    cfg_op_i = '0;
    cfg_page_generation_i = '0;
    cfg_selector_i = '0;
    cfg_row_i = '0;
    cfg_crc32_i = '0;
    cfg_rsp_ready_i = '0;
    // (`hist_rd_valid_i`/`hist_rd_bin_i` were initialised here on the texmat2
    //  branch and are DELIBERATELY NOT carried across. The host/debug merge
    //  moved MEASURE.HISTOGRAM's read behind `zhao_host_reg_hist` and the
    //  aperture, so the core no longer HAS those ports -- zero occurrences in
    //  `zhao_console_core.sv`. Keeping the two lines would have been an older
    //  version of the seam, driving signals that no longer exist.)
    pal_load_valid_i = '0;
    pal_load_op_i = '0;
    pal_load_slot_i = '0;
    pal_load_gen_i = '0;
    pal_load_idx_i = '0;
    pal_load_rgb565_i = '0;
    pal_load_crc_ok_i = '0;
    tri_area2_i = '0;
    // -GlowTag NO LONGER DRIVES A PORT FROM HERE, and that is the point of the
    // change rather than a side effect of it. The tag is set on the MATERIAL
    // RECORD this bench uploads -- see `build_material1` -- so it travels
    // PublishResource -> MEM.UPLOAD -> the residency row -> MATERIAL.RESOLVE ->
    // the material window's published span -> the composer -> the bin pipe ->
    // the tile pipe -> Early-Z -> RASTER.FRAGMENT -> POST.GATHER -> the frame.
    // The owner directive's "Debug-only injection is not the sole producer" is
    // satisfied by it no longer being a producer AT ALL.
    frame_clear_word_i = '0;
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
    // the SNAC connector, unpopulated: both lines idle HIGH
    snac_dat_i   = '1;
    snac_ack_n_i = '1;
    aud_wr_valid_i = '0;
    aud_wr_l_i = '0;
    aud_wr_r_i = '0;
    // GIANTREFS: THE READ WINDOW IS OPEN NOW. It was tied to zero, so
    // DEBUG.COUNTERS swept nothing and every counter the console publishes was
    // unobservable from this bench by construction -- the sweep stalls on
    // `snap_ready_i` and the window simply never advances. A consumer that is
    // never ready is not a consumer.
    cnt_snap_ready_i = '1;
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

    // ---- entry I50 CLOSED: the ARM's Loom stream, STAGED IN DDR -----------
    // ONE ROOT node, the same stimulus this bench always sent. ROOT takes its
    // twelve elements straight from `param` (the block's own kind table), so
    // this IS the instance transform the draw names: the identity, which keeps
    // the descriptor's object bound its world bound and leaves GEOM.CULL
    // seeing the sphere the fixture placed.
    //
    // WRITTEN OUT BY HAND against `design/contracts/GEOM.LOOM.STREAM.md`, not
    // derived from the RTL -- so a change to either side is a disagreement
    // rather than a silent agreement, which is the whole point of freezing a
    // layout.
    for (int unsigned w = 0; w < LOOM_HPS_WORDS; w = w + 1) loom_mem[w] = 64'd0;

    // record 0, the STREAM HEADER: magic, the node count, the camera basis.
    loom_mem[0] = {16'd0, 16'd1, 32'h4D4F_4F4C};           // count 1, "LOOM"
    // The basis is the identity: no BILLBOARD node is sent, and zeros would be
    // a matrix this bench invented for a kind it never uses. Two elements per
    // beat, element 2i in the low half.
    loom_mem[1] = {32'd0, FX16_ONE};                       // cam 0, 1
    loom_mem[2] = 64'd0;                                   // cam 2, 3
    loom_mem[3] = {FX16_ONE, 32'd0};                       // cam 4, 5
    loom_mem[4] = 64'd0;                                   // cam 6, 7
    loom_mem[5] = {32'd0, FX16_ONE};                       // cam 8

    // record 1, the NODE. `first` and `last` are NOT in the record: the
    // carrier derives them from the index against the header's count, so a
    // staged stream cannot carry framing that disagrees with its own length.
    loom_mem[8] = {16'h00A1,                               // src_id   [63:48]
                   16'd0,                                  // angle16  [47:32]
                   5'd0, 1'b0, 2'd0,                       // pad, bodypatch, axis
                   4'd0,                                   // kind ROOT
                   10'd0,                                  // parent
                   10'(SMK_XFORM_NODE_C)};                 // node index
    loom_mem[9]  = {32'd0, FX16_ONE};                      // param 0, 1
    loom_mem[10] = 64'd0;                                  // param 2, 3
    loom_mem[11] = {FX16_ONE, 32'd0};                      // param 4, 5
    loom_mem[12] = 64'd0;                                  // param 6, 7
    loom_mem[13] = 64'd0;                                  // param 8, 9
    loom_mem[14] = {32'd0, FX16_ONE};                      // param 10, 11

    geom_loom_db_plan_base_i   = 32'h5100_0000;   // the plan's epoch identity
    geom_loom_db_post_base_i   = LOOM_HPS_BASE;
    geom_loom_db_post_ticket_i = 32'h0000_5010;
    geom_loom_db_post_valid_i  = 1'b0;
    geom_loom_db_ret_ready_i   = 1'b1;

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
    // THE LITERAL 1 IS GONE, 2026-09-22 (PROJCLOSE), AND THAT IS THE POINT.
    // This line used to read `proj_en_i = 1'b1;` and entry I14 cited it, three
    // passes running, as the evidence that the shared projector's
    // rigid-pipeline enable had no producer: "the smoke bench drives it with a
    // literal 1". It has one now -- `u_proj_cfgvalid` inside the core arms the
    // projector when a view's matrix bank completes -- so this bench holds the
    // HOST OVERRIDE at zero and lets the console's own producer do it.
    //
    // THIS IS THE END-TO-END PROOF AND IT IS FALSIFIABLE. `geom_write_camera()`
    // below is the only thing that can now enable the projector. If the
    // producer were wrong, the projector would never run and `raster pixels`
    // would be 0 instead of 2,560 -- so the unchanged pixel count IS the
    // evidence, rather than a separate claim beside it.
    proj_en_i               = 1'b0;
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
    // AND SINCE 2026-09-22 IT NO LONGER HANDS THE DECODER A SYNTHETIC
    // SKELETON EITHER. The five assignments that stood here -- an identity
    // quaternion and an identity inverse-rest matrix -- went with entry I29's
    // fourteen inputs. `zhao_geom_clipread` reads a real kind-8 BODY and a
    // real kind-9 CLIP FRAME through requester G; this bench publishes
    // neither, so the reader is NOT RESIDENT and refuses, which is a counted
    // behaviour of the design rather than a harness constant. The palette
    // CONTENT is still geom_pose_palette_directed's job against thirty-two
    // distinct matrices, and the reader's own layout handling is
    // geom_clipread_directed's against two committed goldens.

    // ---- PACKET P-TERRAIN: lay out the HPS arena BEFORE reset lifts -------
    // The memory is the board's, not the frame's: it exists before the console
    // starts and nothing in the DUT may depend on when it was written.
    for (int unsigned w = 0; w < HPS_WORDS; w++) hps_mem[w] = 64'd0;

    // ---- THE PAGE HEADERS, SO A PAGE CAN ACTUALLY LOAD --------------------
    // NEW 2026-09-20 (terrain9). Until this loop existed, every page this bench
    // played was 21,376 bytes of ZERO -- header included -- against a record
    // declaring island 0x42 and patch coordinates (r+3, r+7). The measured
    // consequence at `c55e0417`, quoted from this bench's own output:
    //
    //   pl    loaded=0 faulted=3 crc_fails=0 ... hdr_ident_fails=3
    //   mip   mipreq requests=0 issued=0 | mipfeed pages_mipped=0 samples_sent=0
    //   lodfd lattices_walked=0 dropped=0 dev_records=0 -> hist events=0
    //
    // AND NOTE WHICH COUNTER MOVED, because the note that stood here named the
    // wrong one. It said the pages fault "because a zero page's CRC cannot match
    // the record's declared expected_page_crc32c" -- but `crc_fails=0`. The page
    // never reached the CRC: it failed TERRAIN.PAGELOADER's IDENTITY test first
    // (`hdr_ident_fails=3`, verdict 8), on {format_version, island_id, patch_ix,
    // patch_iz} against the record the job came from. A stated cause that the
    // machine's own counters refute is the shape CLAUDE.md keeps finding, and it
    // matters here because fixing only the CRC would have changed nothing.
    //
    // WHY IT IS WORTH FIXING RATHER THAN DOCUMENTING. Owner ruling R70 made the
    // terrain page-load LOD deviation MEASURE.HISTOGRAM's v1 metric, and the
    // ruling's own third requirement is "before quoting a traverse, check that
    // the smoke's stimulus drives MIPFEED's fine stream at all". It did not.
    // TERRAIN.MIPREQ triggers on a page load that FINISHED OK, so with every
    // page faulting, the whole chain behind it -- MIPREQ, MIPFEED's fine stream,
    // TERRAIN.LODFEED's lattice walk, `zhao_terrain_loddev`'s deviations and the
    // histogram's events -- sat at exactly zero while the join between them
    // passed every check written against it. A gate that cannot reach the state
    // is not evidence about the state.
    //
    // The header is `spec/terrain_rules.md` 2.1, little-endian, page-relative,
    // written field by field from that table -- not as a blob -- so a layout
    // change fails here loudly rather than producing a plausible page:
    //
    //     +0  u16 format_version = 1     +16 rectfx envelope x0,z0,x1,z1
    //     +2  i8  pitch_log2             +32 u32 page_crc32c
    //     +3  u8  flags                  +36 u8  rsv[28] (must be 0)
    //     +4  u32 island_id
    //     +8  i16 patch_ix, patch_iz     +12 u32 tileset_id
    //
    // The envelope obeys 2.1's own redundancy law -- "must equal origin +
    // coords x 32 x pitch exactly" -- with the island datum at the origin and
    // pitch_log2 = 0, so pitch is 1 m and the patch spans 32 m. It is written
    // correctly even though TERRAIN.HDRREAD does not check it (that is
    // TERRAIN.PLACE's `place_env_mismatch_o`), because a bench that satisfies
    // only the checks that happen to run today is a fixture that breaks the
    // moment the next block composes.
    for (int unsigned r = 0; r < N_TERR_REC; r++) begin
      automatic int unsigned pw    = (PAGE0_OFF_C + r * PAGE_BYTES_C) >> 3;
      automatic int          ix_i  = int'(r) + 3;
      automatic int          iz_i  = int'(r) + 7;
      // fx16 is Q16.18-free Q16.16 here (FX16_ONE = 32'sh0001_0000), so one
      // metre is 65,536 raw and a 32 m patch edge is 32 * 65,536.
      automatic logic signed [31:0] ex0 = 32'( ix_i      * 32 * 65536);
      automatic logic signed [31:0] ez0 = 32'( iz_i      * 32 * 65536);
      automatic logic signed [31:0] ex1 = 32'((ix_i + 1) * 32 * 65536);
      automatic logic signed [31:0] ez1 = 32'((iz_i + 1) * 32 * 65536);
      hps_mem[pw + 0] = {32'h0000_0042,     // +4  island_id
                         8'h00,             // +3  flags (no C, no D layer)
                         8'h00,             // +2  pitch_log2 = 0 -> 1 m
                         16'h0001};         // +0  format_version = 1
      hps_mem[pw + 1] = {32'h0000_0000,     // +12 tileset_id
                         16'(iz_i),         // +10 patch_iz
                         16'(ix_i)};        // +8  patch_ix
      hps_mem[pw + 2] = {ez0, ex0};         // +16 x0, +20 z0
      hps_mem[pw + 3] = {ez1, ex1};         // +24 x1, +28 z1
      // +32 page_crc32c is written by the fold below, once the body is final.
    end

    // ---- AND ITS BODY CRC, FOLDED OVER THE BYTES THAT ARE ACTUALLY THERE ---
    // Same standing caveat as the record list's fold, stated again rather than
    // cross-referenced because it is the caveat people skip: this bench seals
    // the page with a CRC it computed over the page it wrote, so it is NO
    // EVIDENCE WHATEVER about the CRC law. `tests/terrain/tb_terrain_pageloader`
    // is where that is tested. What it buys is the thing it can honestly buy --
    // a page that LOADS, so the chain behind the load can be observed at all.
    //
    // It is folded with `zhao_abi_pkg::zhao_crc32c_step` rather than with
    // `u_bench_fold`, for two reasons. It is a pure function, so 3 x 21,256
    // bytes cost ZERO simulation time, where the folder's `#1ns` per beat would
    // push ~8 us of dead time in front of a reset the rest of this bench is
    // timed against. And it is a different expression of the same law from the
    // one the list uses, which is worth a little on a page the DUT's own
    // `crc_r` register is independently computing.
    for (int unsigned r = 0; r < N_TERR_REC; r++) begin
      automatic int unsigned pbase = PAGE0_OFF_C + r * PAGE_BYTES_C;
      automatic logic [31:0] c     = 32'hFFFF_FFFF;
      for (int unsigned b = PAGE_CRC_LO_C; b < PAGE_CRC_HI_C; b++) begin
        automatic int unsigned a = pbase + b;
        c = zhao_abi_pkg::zhao_crc32c_step(c, hps_mem[a >> 3][8*(a % 8) +: 8]);
      end
      page_crc_q[r]        = ~c;
      hps_mem[(pbase >> 3) + 4] = {32'd0, page_crc_q[r]};   // +32, +36 rsv
    end

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
                        page_crc_q[r]};      // expected_page_crc32c bytes 16..19
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
      if (hps_mem[b + 2] !== {8'd0, 8'h01, 16'h0001, page_crc_q[r]})
        $fatal(1, "SMOKE: arena record %0d holds crc/flags/mask/prio %016x -- the list layout aliased", r, hps_mem[b + 2]);
      // AND THE PAGE ITSELF IS READ BACK, same independent-operand discipline
      // as the record above. The identity fields are what TERRAIN.PAGELOADER
      // tests the job against, so a header written to the wrong word is LOUD at
      // time zero instead of arriving later as `hdr_ident_fails`.
      if (hps_mem[(PAGE0_OFF_C + r * PAGE_BYTES_C) >> 3] !==
          {32'h0000_0042, 8'h00, 8'h00, 16'h0001})
        $fatal(1, "SMOKE: page %0d header word 0 is %016x -- version/pitch/flags/island not where 2.1 puts them",
               r, hps_mem[(PAGE0_OFF_C + r * PAGE_BYTES_C) >> 3]);
      if (hps_mem[((PAGE0_OFF_C + r * PAGE_BYTES_C) >> 3) + 1] !==
          {32'h0000_0000, 16'(r + 7), 16'(r + 3)})
        $fatal(1, "SMOKE: page %0d header word 1 is %016x -- patch_ix/patch_iz do not match record %0d",
               r, hps_mem[((PAGE0_OFF_C + r * PAGE_BYTES_C) >> 3) + 1], r);
      if (hps_mem[((PAGE0_OFF_C + r * PAGE_BYTES_C) >> 3) + 4][31:0] !== page_crc_q[r])
        $fatal(1, "SMOKE: page %0d header carries crc %08x, record carries %08x -- CHECK_HEADER_CRC will refuse it",
               r, hps_mem[((PAGE0_OFF_C + r * PAGE_BYTES_C) >> 3) + 4][31:0], page_crc_q[r]);
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
    // RECORD 1 IS THE ONE THE MESHLET ACTUALLY NAMES, and it is a separate
    // record ON PURPOSE. `zhao_geom_meshfetch` reads the material id from
    // descriptor bytes 4-5 (`dh(4)`), and this fixture's descriptors carry
    // 16'h0001 there -- so a resolve issued from the TRIANGLE asks for record
    // 1, and a window that quietly defaulted to record 0 would get a material
    // with `tmu_mode 0` (CLUT) and a different weight. That is a POSITIVE
    // DISCRIMINATOR rather than a convenience: the two records differ in every
    // field the flat request carries, so "it asked for the triangle's material"
    // and "it asked for the first one" cannot produce the same answer.
    //
    // It is also the record that lets the TEXTURE ISLAND SAMPLE: one sample,
    // recipe 0 (PASSTHRU, which `material_count_legal` pairs with count 1),
    // binding selector 3 -- the selector the binding page below programs --
    // and `tmu_mode 1`, which is NEAREST under the encoding
    // `zhao_material_window.TMU_MODE_CLASS` holds (FINDINGS-texmat2, D1).
    begin : build_material1
      zhao_abi_pkg::zhao_material_record_t mr1;
      mr1 = '0;
      mr1.control                    = 8'h01;         // count 1, recipe 0 (PASSTHRU)
      mr1.recipe_weight              = 8'h5A;
      mr1.sample0.binding_slot       = 16'(TEX_SELECTOR_C);
      mr1.sample0.binding_generation = 8'h01;
      mr1.sample0.modes              = 8'h01;         // tmu_mode 1 = NEAREST, wrap 0
      mr1.palette_base               = 32'h0000_0000; // direct format: no CLUT
      mr1.raster_state               = 32'h0000_0000;
`ifdef ZHAO_SMOKE_GLOW_TAG
      // -GlowTag, 2026-09-25 (FRAGSTATE). THE TAG IS A FIELD OF THE MATERIAL
      // RECORD NOW, not a value this bench forces onto a boundary port.
      //
      // ONE VARIABLE MOVES. The declared state word is
      // `frag_profile_opaque_geometry()` -- all zero, the plain opaque write --
      // which is EXACTLY what this console already drew under, so the blend,
      // the depth behaviour and every pixel count are unchanged and the ONLY
      // difference between this form and the plain one is that the fragments
      // arrive TAGGED. An additive profile would have changed the picture for
      // three reasons at once and proved none of them.
      //
      // AND IT IS THE POSITIVE CONTROL FOR THE EXPLICIT SELECTOR. This record
      // declares a state word that is bit-identical to a record declaring
      // NOTHING; only `fragment_decl` bit 0 separates them. A composer that had
      // tested `fragment_state != 0` instead of reading the flag would treat
      // this material as undeclared, the tag would never leave the record, and
      // this form would fail. That is the one case the cheap test misses, and
      // it is the case this bench runs.
      mr1.fragment_state = 32'h0000_0000;  // frag_profile_opaque_geometry()
      mr1.fragment_decl  = {8'h00,             // [31:24] reserved 0
                            8'h00,             // [23:16] stencil_reference
                            SMK_GLOW_TAG_C,    // [15:8]  effect_tag
                            8'h01};            // [7:1] reserved 0, [0] DECLARED
`endif
      upl_rec1 = zhao_abi_pkg::zhao_pack_material_record(mr1);
      for (int unsigned w = 0; w < 4; w++) upl_mem[4 + w] = upl_rec1[64*w +: 64];
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
      // THE HANDLE THE PUBLICATION ACTUALLY CREATED. Its generation byte is
      // the LOW BYTE OF `new_generation`, exactly as pr2/the MESH_STREAM page
      // above already notes -- "that is what GEOM.DRAWJOB compares, the same
      // law zhao_material_resolve applies to the same handle shape". It used
      // to read 8'h2A, which was `pr.resource`'s own (unused) generation byte,
      // and no resolve issued from the DRAW could ever have hit the directory
      // with it. Nothing noticed while the bench issued the request itself.
      df.material_set  = {UPL_INDEX_C, UPL_GEN_C[7:0]};
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

    // ---- PACKET P-TERRAIN, SECOND SUBMISSION: THE COMPOSE PASS ------------
    // TERRAINAUX, 2026-09-25. THIS IS THE FIXTURE REPAIR, and it is a bench
    // change because the console was never at fault.
    //
    // `zhao_terrain_seq` walks a submitted set ONCE. On the first walk it finds
    // every patch ABSENT, claims a slot, issues a load -- and SKIPS the compose
    // issue, because there is nothing resident to compose. The measured line
    // says exactly that:
    //
    //   SMOKE:  seq  consumed=3 issued_patches=0 claims=3/0 loads=3 SKIPPED=3
    //   SMOKE:  res  hits=0 misses=6 claims=3 crc_fail=0 resident=3
    //
    // Residency arrives AFTER that walk. With one submission the compose door
    // therefore never opens: `hdr_headers=0`, TERRAIN.PLACE places nothing,
    // `zhao_terrain_compcache_front` never leaves `serve_valid_q = 0`, and it
    // answers every lattice read with POISON (32'h5BADF00D, its line 532). All
    // 81 of TERRAIN.TESS's window vertices are then the SAME poison position,
    // which is why all 128 triangles are exactly degenerate.
    //
    // A real frame loop submits the set EVERY FRAME -- the first frame pages
    // in, later frames compose. This is that second frame, and it is the
    // smallest stimulus that reaches the path under test. A new `sequence` so
    // it is a new set and not a replay of the accepted one.
    //
    // THE THREE NUMBERS THIS IS ASSERTED ON ARE AT THE FOOT OF THE RUN, not
    // here: `patches_served`, `place_patches` and `terrlight degenerate`.
    guard = 0;
    // ONE resident patch is enough for the compose door, and waiting for all
    // three costs 400,000 cycles of simulation for no extra evidence -- the
    // remaining two land later in the run anyway (`res resident=3` at the
    // foot). Measured: the second completion is TERRAIN.MIPGEN's, and the mip
    // pass is 6,534 samples a page.
    while ((terr_res_resident_o < 1) && (guard < 400000)) begin
      @(posedge gpu_clk);
      guard++;
    end
    $display("SMOKE: terrcompose WAIT resident=%0d/%0d after %0d cycles | pl loaded=%0d faulted=%0d | mipfeed pages_mipped=%0d | seq skipped=%0d issued=%0d",
             terr_res_resident_o, N_TERR_REC, guard,
             terr_pl_pages_loaded_o, terr_pl_pages_faulted_o,
             terr_mip_pages_mipped_o, terr_seq_skipped_not_resident_o,
             terr_seq_patches_issued_o);

    terr_cmd_epoch_i       = 32'd9;
    terr_cmd_list_off_i    = 32'(LIST_OFF_C);
    terr_cmd_list_bytes_i  = 32'(LIST_BYTES_C);
    terr_cmd_patch_count_i = 16'(N_TERR_REC);
    terr_cmd_sequence_i    = 32'd2;
    terr_cmd_src_id_i      = 32'd778;
    terr_cmd_valid_i       = 1'b1;
    guard = 0;
    while (!(terr_cmd_valid_i && terr_cmd_ready_o) && (guard < 1000)) begin
      @(posedge gpu_clk);
      guard++;
    end
    @(posedge gpu_clk);
    terr_cmd_valid_i = 1'b0;
    if (guard >= 1000)
      $fatal(1, "SMOKE: TERRAIN.CMD never accepted the COMPOSE-pass command");

    // Let the compose door run: header read, place, page stream, cache fill.
    // Bounded, and the bound is generous -- a 33x33 lattice is 1,089 records
    // and the streamer is one vertex a burst.
    guard = 0;
    while ((terr_cc_patches_served_o == 32'd0) && (guard < 400000)) begin
      @(posedge gpu_clk);
      guard++;
    end

    // ---- the terrain job, INJECTED THROUGH THE OVERRIDE -------------------
    // One subpatch, one view, level 0, top surface, no geomorph. The tess
    // turns this into 81 window vertices (client B's fill) and then its
    // triangles (the replay), so both halves of client B are exercised by ONE
    // job. `sparse_fill_i` stays LOW: the subsystem here carries a dense shell,
    // and a sparse fill against a dense shell is a seal-short fault the terrain
    // differential fires on purpose -- not something a wiring smoke bench
    // should provoke.
    //
    // SINCE 2026-09-21 THIS IS AN OVERRIDE AND NOT THE ONLY PRODUCER.
    // `zhao_terrain_jobissue` issues the same thirteen fields inside the core
    // and cannot be reached from HERE -- see the declaration block above for
    // why, and note that the reason is the CRC failure and not the
    // composition. While this bench drives `terr_job_valid_i` the boundary
    // wins the cycle, exactly as the host's `proj_cfg_we_i` wins over
    // CMD.EXEC's lowering onto the projector bank.
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
      automatic logic [15:0] sentinel_c = '0;
      automatic logic [15:0] first_drawn_px = '0;
      for (int unsigned w = 0; w < POST_TB_WORDS; w++) begin
        if (u_geom_sdram.mem[w] !== post_fb_snap[w]) begin
          if (fb_bad == 0) first_fb = w;
          fb_bad++;
        end
        if (u_geom_sdram.mem[POST_ECHO_WBASE + w] !== post_fb_snap[w]) begin
          if (cap_bad == 0) first_cap = w;
          cap_bad++;
        end
        // THE RASTER'S OWN PIXELS, counted as "this word is no longer the
        // sentinel" rather than "this word equals <colour>". It used to be
        // `== 16'h0000`, which is the same thing only while the fragments are
        // black -- and they are black only because `tri_continuation_tail_i`'s
        // `vertex_rgb` is at its boundary zero (entry I20). The first -GlowTag
        // run gave 1,062 against 2,560 with a predicted constant, so the
        // predicted constant was the defect: this form asks the question the
        // check is actually about and is right for any raster colour.
        sentinel_c = 16'((w * 40503) ^ 32'h5A3C) | 16'h0001;
        if (post_fb_snap[w] !== sentinel_c) begin
          if (drawn == 0) first_drawn_px = post_fb_snap[w];
          drawn++;
        end
        if (post_fb_snap[w] === SMK_GLOW_PX565_C) smk_glow_px_q++;
        if ((post_fb_snap[w] !== sentinel_c) && (post_fb_snap[w] !== 16'h0000))
          smk_lit_px_q++;
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
      // TWOD.BAND (owner rulings R233, R235, core entry I17 item 1). This
      // bench sends NO sprite descriptors, so the HUD is legitimately empty and
      // every one of these reads zero -- which is exactly the shape CLAUDE.md
      // calls a claim. The zeros are worth something here ONLY because
      // `tests/compositor/twod_band_directed.cpp` fires all four with legal
      // stimulus at the block's own ports, and because
      // `tests/mutants/zhao_twod_band_burst_mutant.sv` makes `band_underrun`
      // move with a healthy sampler once the admission law is removed. The
      // bands DO advance, and that is the part of this line that is not a zero:
      // the filler sweeps the frame whether or not there is anything to draw.
      $display("SMOKE: twod band  descs=%0d admitted=%0d refused_budget=%0d desc_overflow=%0d bands=%0d px=%0d clipped=%0d oob=%0d underrun=%0d addr_mismatch=%0d order_inv=%0d tint_dropped=%0d blend_dropped=%0d",
               twod_band_descriptors_o, twod_band_sprites_admitted_o,
               twod_band_sprites_refused_budget_o, twod_band_desc_overflow_o,
               twod_band_bands_o, twod_band_pixels_written_o,
               twod_band_pixels_clipped_o, twod_band_write_oob_o,
               twod_band_underrun_o, twod_band_scan_addr_mismatch_o,
               twod_band_order_inversion_o, twod_band_tint_dropped_o,
               twod_band_blend_dropped_o);
      if (twod_band_write_oob_o != 32'd0 || twod_band_scan_addr_mismatch_o != 32'd0
          || twod_band_sprites_refused_budget_o != 32'd0
          || twod_band_desc_overflow_o != 32'd0)
        $fatal(1, "SMOKE: TWOD.BAND guard fired in the composed console -- oob=%0d addr_mismatch=%0d refused=%0d overflow=%0d. None of these is reachable here: the band chooses its own rows, `hud_req_*` is POST.COMPOSITE's own monotonic sweep, and no descriptor is sent.",
               twod_band_write_oob_o, twod_band_scan_addr_mismatch_o,
               twod_band_sprites_refused_budget_o, twod_band_desc_overflow_o);
      // `band_underrun_o` IS NOT ASSERTED AT ZERO, and the bound is the point.
      // A pass restarts the fill at band 0 with NO LEAD -- buying one would
      // mean a second frame of storage, which is the structure R233 refused at
      // 704 of 553 M10K -- so the reader outruns the filler for as long as it
      // takes to close band 0 and never again. That is at most one band's
      // worth of reads. A number ABOVE that is the schedule failing, not the
      // transient, and this is where the two are told apart.
      if (twod_band_underrun_o > 32'd1536)
        $fatal(1, "SMOKE: TWOD.BAND underrun=%0d exceeds one band of reads (B*LINE_W = 1536). That is not the start-of-pass transient; the filler is not keeping up with the sweep.",
               twod_band_underrun_o);
      $display("SMOKE: echo       complete=%0d torn=%0d written=%0d dropped=%0d fault=%0d",
               echo_passes_complete_o, echo_passes_torn_o, echo_pixels_written_o,
               echo_pixels_dropped_o, echo_fault_o);
      $display("SMOKE: post       raster-overwritten=%0d (first drawn pixel 0x%04h) | framebuffer after post differs in %0d word(s) | capture differs in %0d word(s)",
               drawn, first_drawn_px, fb_bad, cap_bad);
      $display("SMOKE: gouraud    %0d of %0d drawn word(s) carry a COLOUR (the rest are the tile clear) -- GEOM.LIGHT lit %0d vertex/vertices and GEOM.VATTR stored %0d colour(s)",
               smk_lit_px_q, drawn, geom_light_vertices_lit_o,
               geom_va_colours_written_o);
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
`ifdef ZHAO_SMOKE_GLOW_TAG
      // -GlowTag INVERTS THIS ONE CHECK, and does not remove it. In every other
      // form the post pass IS the identity and `fb_bad` must be 0; here the
      // glow plane is lit, `bloom_gain` is 0x5A, and stage 4 must therefore
      // CHANGE pixels. A zero here would mean the tag reached POST.GATHER's
      // counters and never reached the PICTURE -- a counter moving with nothing
      // behind it, which is the one outcome this form exists to exclude.
      //
      // The bound is the argument. A lit cell is quarter-res, so it can only
      // touch output pixels inside the 4x4 block it covers, and the raster drew
      // 2,560 pixels into 160 cells -- so a change wider than the cells those
      // pixels occupy would mean the plane is being sampled at the wrong
      // coordinate, not that the bloom is working.
      if (fb_bad == 0)
        $fatal(1, "SMOKE: -GlowTag lit %0d fragment(s) and the framebuffer after the post pass is IDENTICAL to the raster's frame. R195's law moved its counters and no bloom reached the picture -- the glow plane, its flush, or POST.COMPOSITE's stage 4 is not carrying.",
               gather_frag_lit_o);
      $display("SMOKE: -GlowTag  the post pass CHANGED %0d word(s), first at word %0d (the identity assertion is deliberately inverted in this form)",
               fb_bad, first_fb);
`else
      if (fb_bad != 0)
        $fatal(1, "SMOKE: the framebuffer after the IDENTITY post pass differs from the raster's frame in %0d word(s), first at word %0d -- the read-back or the write-back moved or changed pixels",
               fb_bad, first_fb);
`endif
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
`ifdef ZHAO_SMOKE_GLOW_TAG
      // POST.ECHO taps the COMPOSITOR'S OUTPUT (`zhao_post_composite`'s
      // `echo_valid_o` sits beside `o_valid_o`), not its source. Under every
      // other form the pass is the identity so output == source and `cap_bad`
      // is 0 either way -- which means the existing check could never tell the
      // two taps apart. Here they differ, and the capture must follow the
      // OUTPUT. `echo_vs_fb` below is the assertion that actually says so, and
      // it holds in BOTH forms; this one is its companion, inverted.
      if (cap_bad == 0)
        $fatal(1, "SMOKE: -GlowTag changed %0d framebuffer word(s) and POST.ECHO's capture still equals the PRE-pass frame. The echo is tapping the compositor's SOURCE, not its output.",
               fb_bad);
`else
      if (cap_bad != 0)
        $fatal(1, "SMOKE: POST.ECHO's capture differs from the frame in %0d word(s), first at word %0d",
               cap_bad, first_cap);
`endif
      // WHICH FRAME THE ECHO HOLDS. Added 2026-09-21 with -GlowTag, and it sits
      // inside the ARMED branch because a DISARMED echo writes nothing at all,
      // so comparing its window to anything would be comparing zeros.
      //
      // Until a pass changed a pixel this question had NO OBSERVABLE ANSWER:
      // with an identity pass the compositor's source and its output are the
      // same pixels, so `cap_bad == 0` above was equally consistent with the
      // echo tapping either one. It is not a new rule -- ruling R7 always said
      // the echo carries the RETIRED frame -- it is the first stimulus under
      // which the rule can fail, which is why it was worth writing down.
      begin
        automatic int unsigned echo_vs_fb = 0, first_evf = 0;
        for (int unsigned w = 0; w < POST_TB_WORDS; w++)
          if (u_geom_sdram.mem[POST_ECHO_WBASE + w] !== u_geom_sdram.mem[w]) begin
            if (echo_vs_fb == 0) first_evf = w;
            echo_vs_fb++;
          end
        if (echo_vs_fb != 0)
          $fatal(1, "SMOKE: POST.ECHO's capture differs from the WRITTEN-BACK framebuffer in %0d word(s), first at word %0d -- the tap and the write-back saw different pixels",
                 echo_vs_fb, first_evf);
      end
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

    // ======================================================================
    // THE PARTICLE PATH TO THE RASTER -- owner ruling 1, 2026-09-22 (PARTMAT)
    // ======================================================================
    // The ruling's closing sentence is aimed at exactly this bench: "an
    // otherwise green smoke whose upstream fixture never reaches the new path
    // does not prove the path."
    //
    // So the chain is PRINTED END TO END, and what follows it is an
    // IMPLICATION rather than a demand: it fires only if this fixture DOES
    // reach PART.EXPAND and the new path then drops the beat.
    //
    // A DISPLAY WITH NO ASSERTION WOULD BE THE FLATTERING SHAPE -- a number
    // nobody has to justify. A DEMAND WITH NO REACHABILITY would be a red
    // smoke on a fixture that was never asked to spawn a polygon particle.
    // The implication is the honest instrument, and the NOTE on the other
    // branch is the disclosure TERRABAKE made unprompted about its own work:
    // it says IN THE OUTPUT that this smoke is not evidence about that path,
    // so nobody can quote the green.
    $display("SMOKE: particle->raster  projected=%0d behind=%0d ladder=%0d expanded=%0d | clipfeed particles=%0d triangles=%0d refused_range=%0d stall_full=%0d dq[refused/stray]=[%0d %0d] | window no_material_spans=%0d mode_refused=%0d no_record=%0d | untex_refused=%0d",
             part_prj_projected_o, part_prj_behind_o, part_lad_decisions_o,
             part_exp_polygons_o,
             part_cf_particles_o, part_cf_triangles_o, part_cf_range_refused_o,
             part_cf_stall_full_o, part_cf_dq_refused_o, part_cf_dq_stray_o,
             mat_win_no_material_spans_o, mat_win_mode_refused_o,
             mat_win_no_record_o, geom_untex_refused_o);

    if (part_exp_polygons_o != 32'd0) begin
      // THE PATH IS REACHED BY THIS FIXTURE, so it must carry.
      if (part_cf_particles_o == 32'd0)
        $fatal(1, "SMOKE: PART.EXPAND emitted %0d polygon particle(s) and PART.CLIPFEED took NONE -- the AND-fork at part_exp_ready_i is not carrying",
               part_exp_polygons_o);
      if (part_cf_triangles_o == 32'd0)
        $fatal(1, "SMOKE: PART.CLIPFEED took %0d particle(s) and emitted NO triangle -- the depth converter or the ring's head-of-line rule is stuck",
               part_cf_particles_o);
      if (mat_win_no_material_spans_o == 32'd0)
        $fatal(1, "SMOKE: particles reached the door and NO NO_MATERIAL span was published -- owner ruling 1's lawful mode was not selected");
      if (part_cf_range_refused_o != 32'd0)
        $fatal(1, "SMOKE: %0d particle fan(s) refused for range -- to_screen_xy's clamp premise is broken upstream of PART.CLIPFEED",
               part_cf_range_refused_o);
      if (mat_win_mode_refused_o != 32'd0)
        $fatal(1, "SMOKE: %0d contradictory material declaration(s) -- a producer is declaring NO_MATERIAL with a live {set, id}",
               mat_win_mode_refused_o);
      $display("SMOKE: NOTE the particle-to-raster path IS EXERCISED by this fixture -- %0d particle(s) expanded, %0d triangle(s) offered at GEOM.CLIP's door, %0d NO_MATERIAL span(s) published, %0d missing-material fault(s), %0d untextured refusal(s).",
               part_exp_polygons_o, part_cf_triangles_o,
               mat_win_no_material_spans_o, mat_win_no_record_o,
               geom_untex_refused_o);
    end else begin
      $display("SMOKE: NOTE the particle-to-raster path is QUIESCENT in this fixture: PART.PROJECT projected %0d, PART.LADDER made %0d decision(s), and PART.EXPAND emitted NO polygon particle -- so PART.CLIPFEED, the NO_MATERIAL span and GEOM.CLIPDOOR's third client were NEVER REACHED. THIS SMOKE IS NOT EVIDENCE ABOUT THAT PATH. tests/prod/partmat_acceptance.cpp and tests/particles/part_clipfeed_directed.cpp are.",
               part_prj_projected_o, part_lad_decisions_o);
    end
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
    // GEOM.PARAMBUF, and EVERY counter is printed rather than a chosen few.
    // DEVSDRAM's own finding, two days ago: "the smoke declared eight counters
    // and printed none of them" -- a counter a bench declares and never shows
    // is a counter nobody can read a zero off, and a zero nobody reads is the
    // most comfortable number in the repository.
    //
    // WHAT THE NUMBERS BELOW ACTUALLY MEAN IN THIS COMPOSITION, so nobody
    // reads a zero as a pass. Console entries I53/I54 tie the vertex and chunk
    // intakes, and I55 ties the walk request. So in THIS bench:
    //   verts         is LIVE as of 2026-09-25 (ARENAID, entry I53 CLOSED):
    //                 `u_geom_vertid` publishes GEOM.CLIP's post-clip corners,
    //                 once per {arena, generation, index} identity
    //   tris          is LIVE and now POST-CLIP: the descriptors name the ids
    //                 the ALLOCATOR handed back, not GEOM.ASSEMBLE's
    //                 arena-local indices
    //   chunks        is EXPECTED ZERO -- no producer port yet (entry I54)
    //   dirs/walk/*   are EXPECTED ZERO -- nothing asks for a walk (entry I55)
    // The detectors below them -- overrun, addrbad, retireunder, dirmiss,
    // genrace, stray, ws_unowned -- are the ones that must be zero for a
    // reason rather than for lack of traffic, and `tris` non-zero beside
    // them is what makes their zero worth anything at all.
    // GEOM.VERTID -- the one geometry identity space (ARENAID, entry I53).
    // `reused` is the property the whole scheme exists for: a corner answered
    // from the identity map instead of published a second time. It is printed
    // beside `refs` and `published` because the three are one statement --
    // refs == published + reused + sunk -- and a `published` nobody can
    // compare `refs` against is a number, not evidence.
    $display("SMOKE: vertid     tris=%0d refs=%0d published=%0d reused=%0d unshared=%0d",
             geom_vid_tris_o, geom_vid_refs_o, geom_vid_published_o,
             geom_vid_reused_o, geom_vid_unshared_o);
    $display("SMOKE: vertid     opens=%0d sunk=%0d stall=%0d",
             geom_vid_opens_o, geom_vid_sunk_o, geom_vid_stall_o);
    if (geom_vid_refs_o !=
        (geom_vid_published_o + geom_vid_reused_o + geom_vid_sunk_o)) begin
      $display("SMOKE FAIL: vertid refs %0d != published %0d + reused %0d + sunk %0d",
               geom_vid_refs_o, geom_vid_published_o, geom_vid_reused_o,
               geom_vid_sunk_o);
      $fatal(1, "GEOM.VERTID: every corner reference is published, reused or sunk");
    end
    $display("SMOKE: paramarena verts=%0d tris=%0d chunks=%0d frames=%0d unsealed=%0d",
             geom_pa_verts_o, geom_pa_tris_o, geom_pa_chunks_o,
             geom_pa_frames_o, geom_pa_unsealed_o);
    $display("SMOKE: paramarena denied=%0d overflow=%0d discarded=%0d overrun=%0d fault=%0d src=%0d",
             geom_pa_denied_o, geom_pa_overflow_o, geom_pa_discarded_o,
             geom_pa_overrun_o, geom_pa_fault_o, geom_pa_fault_src_o);
    $display("SMOKE: paramarena flipblock=%0d pubblock=%0d addrbad=%0d scrcontend=%0d retireunder=%0d",
             geom_pa_flipblock_o, geom_pa_pubblock_o, geom_pa_addrbad_o,
             geom_pa_scrcontend_o, geom_pa_retireunder_o);
    // THE TWO ALIGNMENT TRIPWIRES. Printed apart from the rest because their
    // zero means something different: the behavioural SDRAM model reads and
    // writes LINEARLY where a JEDEC BL8 sequential burst wraps inside its
    // aligned sixteen-byte block, so these two CANNOT be made to fire by any
    // functional stimulus and their silence here is not evidence that the
    // addresses are right. It is the committed align mutant
    // (geom_paramarena_alignmut_fires) that makes them evidence.
    $display("SMOKE: paramalign arena_unaligned=%0d walk_unaligned=%0d",
             geom_pa_unaligned_o, geom_pw_unaligned_o);
    $display("SMOKE: paramwalk  dirs=%0d dirmiss=%0d chunks=%0d stale=%0d illegal=%0d depth=%0d",
             geom_pw_dirs_o, geom_pw_dirmiss_o, geom_pw_chunks_o,
             geom_pw_stale_o, geom_pw_illegal_o, geom_pw_depth_o);
    $display("SMOKE: paramwalk  tris=%0d trisbad=%0d cut=%0d denied=%0d short=%0d stray=%0d genrace=%0d",
             geom_pw_tris_o, geom_pw_trisbad_o, geom_pw_cut_o,
             geom_pw_denied_o, geom_pw_short_o, geom_pw_stray_o,
             geom_pw_genrace_o);
    $display("SMOKE: geomwshare denied=%0d contention=%0d err[short/long/unowned]=[%0d %0d %0d] retireunowned=%0d wbeatunowned=%0d ledgerfull=%0d",
             geom_ws_denied_o, geom_ws_contention_o, geom_ws_short_o,
             geom_ws_long_o, geom_ws_unowned_o, geom_ws_retire_unowned_o,
             geom_ws_wbeat_unowned_o, geom_ws_ledger_full_o);
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
    // GIANTREFS: GEOM.BINNER's instruments, read out of the console's own
    // counter window rather than out of a wire the shell throws away.
    $display("SMOKE: binrefs    sweep_beats=%0d tile_references=%0d (seen=%0d) max_tile_list_depth=%0d (seen=%0d) overflow=%0d",
             cnt_beats_seen_q, cnt_tile_refs_q, cnt_tile_refs_seen_q,
             cnt_tile_depth_q, cnt_tile_depth_seen_q, render_overflow_o);
    $display("SMOKE: renderlease leases_granted=%0d refused=%0d clears=%0d frames_admitted=%0d",
             v2_leases_granted_o, v2_leases_refused_o,
             v2_clear_handshakes_o, v2_frames_admitted_o);
    $display("SMOKE: terrain    tess_vertices=%0d tess_refs=%0d fills_forwarded=%0d refs_forwarded=%0d groups_opened=%0d b_grants=%0d",
             terr_tess_vertices_o, terr_tess_refs_o, terr_fills_forwarded_o,
             terr_refs_forwarded_o, terr_groups_opened_o, proj_b_grants_o);
    $display("SMOKE: projector  a_grants=%0d b_grants=%0d contended=%0d replay_triangles=%0d",
             proj_a_grants_o, proj_b_grants_o, proj_contended_o,
             proj_replay_triangles_o);
    // ---- THE COMPOSE SPINE, which is WHY the triangles are degenerate ------
    // TERRAINAUX, 2026-09-25. `SMOKE: terrlight degenerate=128` has been read
    // three different ways by three packets and the bench's own note below
    // still offers a WRONG cause ("the pages are all-zero BODIES, so the
    // lattice is flat and the cross product is exactly zero"). A flat lattice
    // with DISTINCT world x/z has an UP-facing normal, not a zero one; zero
    // heights alone cannot make a cross product vanish.
    //
    // The real cause is one hop upstream and this line is how a reader sees
    // it without a waveform: TERRAIN.TESS reads its lattice through
    // TERRAIN.HEIGHTTAP from `zhao_terrain_compcache_front`, and that block
    // answers `lat_h_o`/`lat_wx_o`/`lat_wz_o` with POISON (32'h5BADF00D,
    // compcache_front.sv:532) on every cycle `serve_valid_q` is low. With no
    // patch served, all 81 window vertices are the SAME poison position, so
    // every one of the 128 triangles is exactly degenerate.
    //
    // So the number to read here is `patches_served`, not `degenerate`.
    $display("SMOKE: terrcompose hdr_headers=%0d hdr_refused=%0d ps_lattices=%0d ps_vertices=%0d ps_cells=%0d place_patches=%0d pt_samples=%0d | cc_filled=%0d cc_served=%0d cc_records=%0d cc_serving=%0d cc_overrun=%0d lat_oob=%0d mat_cells=%0d",
             terr_hr_headers_o, terr_hr_refused_o,
             terr_ps_lattices_o, terr_ps_vertices_o, terr_ps_cells_o,
             terr_place_patches_o, terr_pt_samples_o,
             terr_cc_patches_filled_o, terr_cc_patches_served_o,
             terr_cc_fill_records_o, terr_cc_serve_valid_o,
             terr_cc_fill_overrun_o, terr_cc_lat_oob_o, terr_cc_mat_cells_o);
    // ---- TERRAIN.VELJOIN / TERRAIN.VELOCITY, composed 2026-09-26 (TERRVEL) --
    //
    // WHAT THIS SMOKE CAN AND CANNOT SAY, stated before the numbers so neither
    // is over-read. It issues NO TerrainField, so `fields_active_o` is 0 at
    // every patch and law V2 makes every lattice word exactly zero. So this is
    // NOT evidence that a field moves the ground -- that is
    // `terrain_veljoin_directed`'s job, against the oracle. What it IS evidence
    // of is that the composed machine RUNS the sweep on the real walk: the join
    // arms on real patches, TERRAIN.VELOCITY completes real 1,089-vertex
    // lattices, and the compose cache takes the words.
    //
    // THE THREE ZEROES BELOW ARE ASSERTED AND EACH ONE HAS BEEN SEEN TO FIRE
    // ELSEWHERE, because a detector reading zero is a claim:
    //   vtx_mismatch   fired by terrain_veljoin_directed section 5, which
    //                  offers a walk address that skips a vertex.
    //   vel_orphan     fired by the same file's section 6 (a word with no
    //                  fill buffer held).
    //   vel_done_mm    fired by section 7 (a done pulse off the 1,089th word).
    $display("SMOKE: terrvel   sweeps_started=%0d aborted=%0d lanes_joined=%0d arm_stall=%0d | tv_samples=%0d add_sats=%0d resc_sats=%0d moving_mask=%04h | cc_vel_words=%0d",
             terr_vj_sweeps_started_o, terr_vj_sweeps_aborted_o,
             terr_vj_lanes_joined_o, terr_vj_arm_stall_o,
             terr_tv_samples_o, terr_tv_add_sats_o, terr_tv_rescale_sats_o,
             terr_tv_moving_mask_o, terr_cc_vel_words_o);
    $display("SMOKE: terrvelc  vtx_mismatch=%0d vel_orphan=%0d vel_done_mm=%0d | part_ter_moving=%0d part_vel_sats=%0d part_col_moving_ground=%0d",
             terr_vj_vtx_mismatch_o, terr_cc_vel_orphan_o, terr_cc_vel_done_mm_o,
             part_ter_moving_o, part_ter_vel_sats_o, part_col_moving_ground_o);
    if (terr_vj_vtx_mismatch_o != 32'd0)
      $fatal(1, "SMOKE: terrvel vtx_mismatch=%0d -- TERRAIN.VELOCITY's own sweep address disagreed with the pagestream walk",
             terr_vj_vtx_mismatch_o);
    if (terr_cc_vel_orphan_o != 32'd0)
      $fatal(1, "SMOKE: terrvel vel_orphan=%0d -- a velocity word arrived with no fill buffer held",
             terr_cc_vel_orphan_o);
    if (terr_cc_vel_done_mm_o != 32'd0)
      $fatal(1, "SMOKE: terrvel vel_done_mm=%0d -- sweep-complete did not land on the 1,089th word",
             terr_cc_vel_done_mm_o);
    // The POSITIVE half, and it is the half that would catch a composition
    // that elaborates and never runs: with patches served, the sweep must have
    // started and TERRAIN.VELOCITY must have evaluated samples. A silent
    // velocity lane beside a working height lane is exactly the failure the
    // entry this closes spent five packets on.
    if ((terr_cc_patches_served_o != 32'd0) && (terr_vj_sweeps_started_o == 32'd0))
      $fatal(1, "SMOKE: terrvel patches were served but no velocity sweep ever started");
    if ((terr_vj_sweeps_started_o != 32'd0) && (terr_tv_samples_o == 32'd0))
      $fatal(1, "SMOKE: terrvel a sweep started but TERRAIN.VELOCITY evaluated no samples");

    // ---- TERRAIN's TEXTURE COORDINATES, measured on the DUT's own edge ----
    // This bench CAN reach this lane, and the entry that said otherwise was
    // wrong: the terrain spine loads 3 pages with crc_fails=0 and replays 128
    // triangles, which `SMOKE: terrlight` beside this line has been reporting
    // all along. What the smoke canNOT do is check a coordinate against the
    // oracle -- that is `tests/terrain/terrain_uvlane_directed.cpp`'s job, with
    // two independent oracles. What it CAN do is show the lane taking real
    // references in the composed machine under real backpressure, which is the
    // difference between "it elaborates" and "the value traverses".
    $display("SMOKE: terruv    refs_taken=%0d emitted=%0d stale=%0d pitch_clamped=%0d pitch_illegal=%0d",
             terr_uv_refs_taken_o, terr_uv_emitted_o, terr_uv_stale_reads_o,
             terr_uv_pitch_clamped_o, terr_uv_pitch_illegal_o);
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
    if ((terr_hps_pend_dropped_o != 0) || (terr_hps_pend_dropped_mask_o != 8'd0))
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
    // THE DEGENERACY IS REPAIRED, AND THE PARAGRAPH THAT STOOD HERE WAS WRONG
    // ABOUT ITS CAUSE. It read: "the pages this bench plays are all-zero
    // BODIES ... so the lattice TERRAIN.TESS emits is a flat zero height
    // field, the cross product is exactly zero ... it was never residency
    // that caused it, it was the ZERO HEIGHTS."
    //
    // A FLAT LATTICE WITH DISTINCT WORLD x/z HAS AN UP-FACING NORMAL, NOT A
    // ZERO ONE. Zero heights alone cannot make a cross product vanish, so that
    // could not have been the cause, and nobody checked.
    //
    // The cause was one hop upstream: TERRAIN.TESS reads its lattice through
    // TERRAIN.HEIGHTTAP from `zhao_terrain_compcache_front`, which answers
    // `lat_h_o`/`lat_wx_o`/`lat_wz_o` with POISON (32'h5BADF00D, its line 532)
    // on every cycle `serve_valid_q` is low. With ONE SubmitTerrainSet the
    // compose door never opened -- `zhao_terrain_seq` walks a set once and
    // skips a patch that is not yet resident -- so all 81 window vertices were
    // the SAME poison position and every triangle was exactly degenerate.
    //
    // The second submission above opens it. Measured 2026-09-25 (TERRAINAUX):
    // `hdr_headers=1 place_patches=1 cc_filled=1 cc_records=1089 cc_serving=1`
    // and `terrlight ... degenerate=0` of 256. THIS CHECK IS THE REPAIR'S
    // GATE: a regression that closes the compose door again puts it back to
    // 256-of-256 and this line fires.
    //
    // COMPILED OUT UNDER THE SLOT-OVERFLOW MUTANT, 2026-09-26 (TERRTRI), FOR
    // THE REASON THIS BENCH ALREADY WROTE DOWN 400 LINES BELOW AND THEN WALKED
    // PAST. The `ifdef ZHAO_MUT_SLOT_OVERFLOW guarding the production terrain
    // verdict says it exactly: "the mutation legitimately breaks those checks
    // ... Running them against a deliberately broken machine would be asserting
    // the bug." This gate arrived later (TERRAINAUX, 2026-09-25) and landed
    // ABOVE that guarded region, so it was never covered by it.
    //
    // The mutation halves TERR_POOL_SLOTS, which makes TERRAIN.PAGELOADER
    // REFUSE every job the directory places above its range -- that refusal is
    // the whole point, it is what moves `terr_pl_slot_overflow_o`. A refused
    // page never becomes resident, so the compose door never opens and
    // `zhao_terrain_compcache_front` serves its 32'h5BADF00D poison, so every
    // triangle is degenerate BY CONSTRUCTION. Measured: 12 of 12.
    //
    // So this line killed the mutant at [7589420000] before its inverted-
    // polarity verdict at the `ifdef below could be read -- which made
    // -Mutant neither a passing control nor a failing one but an ABSENT one,
    // while the campaign went on quoting it as evidence that the counter fires.
    // CLAUDE.md states the remedy exactly: "when a mutant trips a SIMULATION
    // assertion before the synthesizable counter can be read, disable the
    // assertion IN THE MUTANT ONLY, with the reason beside it."
    //
    // NOTHING IS WEAKENED FOR PRODUCTION. Every non-mutant build -- the plain
    // run and all four other controls -- still asserts degenerate == 0.
`ifndef ZHAO_MUT_SLOT_OVERFLOW
    if (terr_light_degenerate_count_o != 0)
      $fatal(1, "SMOKE: %0d of %0d terrain triangles are DEGENERATE -- the compose cache is serving POISON again (see `SMOKE: terrcompose`: patches_filled and serve_valid are the numbers to read)",
             terr_light_degenerate_count_o, terr_light_shaded_o);
`endif
    // WHAT THIS STILL DOES NOT PROVE: that the shade VALUE is right. That is
    // proved bit-for-bit against zref in
    // tests/terrain/terrain_lightlane_directed.cpp over a random sub-metre
    // lattice. What it proves is that the reference reached the store, the
    // normal and the shade, came back, and described a triangle with AREA.
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
    //
    // AND THE FLOOR IS NOT THE LAW -- CORRECTED 2026-09-26 (TERRTRI), AND THIS
    // IS THE SAME DISAGREEMENT THE PARAGRAPH ABOVE FIXED ONCE ALREADY.
    // The loop waited for `< N_TERR_REC` -- THREE -- while every check below
    // asserts against `terr_lq_issued_o`, which the SECOND SubmitTerrainSet
    // made FIVE. So the wait stopped two jobs early and the checks then read a
    // pipeline that was still in flight.
    //
    // The tell was printed on every run and read by nobody: `guard=0`. Three
    // completions have long since happened by the time this line is reached,
    // so the loop exited on its FIRST evaluation and never waited at all.
    // Whether the remaining two jobs had finished then depended entirely on
    // how many cycles the REST of the frame happened to burn -- so the plain
    // run and -BadTraceArm passed by LUCK, and -BadVertex (pixels=0, a short
    // raster) and -NoEchoArm (no echo pass) failed with "jobs are stuck in the
    // queue" and "one job, one completion is broken" against RTL that was
    // doing exactly what it should. A green that depends on unrelated run
    // length is not evidence, and the red named the wrong block.
    //
    // THE WAIT NOW WAITS FOR WHAT THE CHECKS ASSERT, which STRENGTHENS them:
    // the equalities below stop meaning "the queue happened to be empty at an
    // arbitrary moment" and start meaning "the queue DRAINS". The N_TERR_REC
    // floor stays a floor -- without it a spine that never started satisfies
    // 0 == 0 == 0 on the first evaluation and the loop proves nothing.
    while (!(((terr_pl_pages_loaded_o + terr_pl_pages_faulted_o +
               terr_pl_pages_refused_o) >= N_TERR_REC) &&
             (terr_lq_accepted_o == terr_seq_loads_issued_o) &&
             (terr_lq_issued_o   == terr_lq_accepted_o) &&
             ((terr_pl_pages_loaded_o + terr_pl_pages_faulted_o +
               terr_pl_pages_refused_o) == terr_lq_issued_o)) &&
    //
    // AND IT WAS FOUND TWICE, INDEPENDENTLY, FROM OPPOSITE SIDES -- recorded
    // at the merge, 2026-09-26. CHUNKSER hit the same defect at `50714814`
    // while composing I54, wrote a local repair, MEASURED WITH IT (raster
    // pixels=2560, frames_admitted=1, and `paramarena chunks=10 frames=1`,
    // which is I54 producing real chunks in the composed console), and then
    // REVERTED it on purpose rather than duplicate a landed repair on a
    // shared bench -- leaving a note saying "TAKE TERRTRI's VERSION". Its
    // independent tell was the same one: `guard=0`.
    //
    // Two packets reaching the same `guard=0` from different subsystems is
    // the strongest evidence this bench has that the fault was the WAIT and
    // not the RTL underneath it.
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
    // THE MIP LANE, PRINTED BECAUSE A RULING TURNS ON IT (R70, terrain6).
    // `samples_sent` is TERRAIN.MIPFEED's count of fine-lattice samples pushed
    // onto `mg_fine_valid_o` -- the stream ruling R70's histogram metric is
    // derived from. It was NOT printed before, and R70 says in terms: "a
    // traverse quoted against a stream that never moves is the gap re-opened
    // under a green gate." These six numbers are how that stops being an
    // argument. Read `mipreq requests` FIRST: TERRAIN.MIPREQ's trigger is
    // `tpl_fin_valid && tpl_fin_ready && tpl_fin_ok`, so a spine whose pages
    // all fault on CRC issues no mip job, and every number to its right is
    // then zero for that reason and not because the mip lane is broken.
    $display("SMOKE:   mip   mipreq requests=%0d issued=%0d drops=%0d | mipfeed pages_mipped=%0d faulted=%0d samples_sent=%0d | mipgen m17_writes=%0d m9_writes=%0d aborts=%0d",
             terr_mipreq_requests_o, terr_mipreq_issued_o, terr_mipreq_drops_o,
             terr_mip_pages_mipped_o, terr_mip_pages_faulted_o, terr_mip_samples_sent_o,
             terr_mg_m17_writes_o, terr_mg_m9_writes_o, terr_mg_aborts_o);
    // TERRAIN.LODFEED rides the line above, so it is printed beside it. These
    // four and `hist events` below are ONE READING: lodfeed's records ARE the
    // histogram's events (entry I18, ruling R70), so `dev_records` and
    // `hist_events` must move together or something between them is dropping.
    $display("SMOKE:   lodfd lattices_walked=%0d dropped=%0d dev_records=%0d stray_samples=%0d -> hist events=%0d updates=%0d stalls=%0d",
             terr_lodfeed_lattices_walked_o, terr_lodfeed_lattices_dropped_o,
             terr_lodfeed_dev_records_o, terr_lodfeed_stray_samples_o,
             hist_events_o, hist_updates_o, hist_stall_cycles_o);
    // ---- THE SUBPATCH DECISION CHAIN, ACTUALLY PRINTED --------------------
    // Added 2026-09-22 (gz/devsdram). The declaration of these eight counters
    // has said since 2026-09-21 that "the bench prints them on the `lod` line
    // so the zero is visible and explained instead of absent" -- AND IT DID
    // NOT. Eight counters were wired out of the core and dropped on the floor
    // under a comment asserting they were read. Found while looking for a
    // witness that owner ruling R242's SDRAM deviation store is exercised
    // here; the answer was sitting in a counter nothing displayed.
    //
    // WHAT THE ZEROES MEAN, so they are explained rather than merely visible.
    // `ds_reads` is the store's per-patch READ, which only happens when the
    // compose door opens -- and no compose job is issued in this fixture, so
    // it reads ZERO and every `sp` counter with it. The store's WRITE side is
    // exercised: the `lodfd` line above reports 48 deviation records, which is
    // twelve 64-byte MEM.GUARD write bursts into TERRAIN.DEVSTORE through the
    // real guard on the five-requester TERRAIN.BUILD socket.
    //
    // NOT ASSERTED, deliberately. A zero here is a statement about the
    // FIXTURE, not about these blocks, and asserting it would pin the fixture
    // in place: the day a compose job is issued these become nonzero and the
    // assertion would read as a regression in the wrong five blocks.
    $display("SMOKE:   lodch ds_reads=%0d ds_unwritten=%0d ds_histstep=%0d | sp descriptors=%0d door_refused=%0d serve_no_door=%0d src_mismatch=%0d order_bad=%0d unfresh=%0d | lod reps=%0d/%0d/%0d/%0d",
             terr_ds_patches_read_o, terr_ds_read_unwritten_o, terr_ds_hist_step_bad_o,
             terr_sp_descriptors_o, terr_sp_door_refused_o, terr_sp_serve_no_door_o,
             terr_sp_door_src_mismatch_o, terr_sp_order_bad_o, terr_sp_patches_unfresh_o,
             terr_lod_rep_count0_o, terr_lod_rep_count1_o,
             terr_lod_rep_count2_o, terr_lod_rep_count3_o);
    // ---- `stray_samples_o` IS A KNOWN-MISLABELLED COUNTER. READ THIS -------
    // REWRITTEN 2026-09-20 (terrain9), on the first run in which this stream
    // ever moved. What stood here asserted `stray_samples_o == 0` and called a
    // non-zero value "mg_start_o and mg_fine_valid_o out of step ... it must
    // read zero on a spine that pages successfully too". BOTH HALVES WERE
    // UNTESTED CLAIMS: the counter had never been anything but zero, because
    // the fixture never loaded a page (see the arena layout loop).
    //
    // MEASURED, the first time it moved: 2,726 of 3,267 surplus samples.
    // THE SPINE IS HEALTHY AND THE COUNTER IS MISLABELLED. Why:
    //
    //   * `zhao_terrain_mipfeed.sv:258` sends ONE `mg_start_o` for BOTH passes
    //     by design (MIPGEN's surface counter runs across them), then streams
    //     2 x 1,089 samples: pass 0 is SURF0_PLANE (layer A) and pass 1 is
    //     SURF1_PLANE (layer C, the underside);
    //   * `zhao_terrain_lodfeed` buffers surface 0 and drops surface 1 by
    //     law 7, counting it on `surface1_samples_o` -- correct;
    //   * but its classifier tests `!fill_active_q` BEFORE `surf1_q`, and
    //     `fill_active_q` is cleared by the DEVIATION WALK RETIRING (`dv_done`)
    //     -- an event with nothing to do with where the stream is. So every
    //     surface-1 sample arriving after the walk finished is filed as "a
    //     sample with no start". In the console the walk (~9.8k clocks)
    //     retires part-way through pass 1, so ~909 of every 1,089 land wrong.
    //
    // TWO CASES SEPARATED ON A FLAG THAT NEITHER OF THEM OWNS -- CLAUDE.md's
    // detector-wired-to-the-wrong-operand, and it reads as a paging fault.
    //
    // WHY IT IS NOT REPAIRED IN THIS COMMIT, stated so the next packet does not
    // assume it was an oversight. The block CANNOT distinguish the two cases:
    // a surface-1 sample and a genuine orphan both arrive with `surf1_q` high
    // and `fill_active_q` low, and the only separator is a COUNT. Every
    // correct reclassification therefore changes what the counter means -- and
    // `terrain_lodhist_directed` check 3 is a COMMITTED POSITIVE CONTROL that
    // fires this counter with eight samples presented AFTER a walk has retired,
    // i.e. it encodes the present meaning directly. I tried the bounded fix,
    // it worked in the console, and it turned that control red (expected 8,
    // got 0). Re-authoring another lane's committed positive control is not a
    // thing to do inside a packet scoped elsewhere, so it is MEASURED and
    // HANDED OVER instead. RECOMMENDED next packet: make `stray_samples_o`
    // mean "a sample before any start was ever seen", move everything else to
    // `surface1_samples_o`, and MOVE check 3's control to the top of the test
    // where that condition is reachable.
    //
    // WHAT IS ASSERTED MEANWHILE IS A LAW THAT IS TRUE OF THE SHIPPED BLOCK:
    // the surplus this block reports can never exceed the surplus TERRAIN.
    // MIPFEED actually sent. `samples_sent` is every sample offered; this block
    // takes VERTS of them per lattice it walked; everything else is surplus,
    // split between the two counters by the walk's timing. If `mg_start_o` and
    // `mg_fine_valid_o` really were out of step -- a stream with no job behind
    // it -- this bound is what breaks.
    if ((terr_lodfeed_stray_samples_o +
         TERR_LATTICE_VERTS * terr_lodfeed_lattices_walked_o) >
        terr_mip_samples_sent_o)
      $fatal(1, "SMOKE: TERRAIN.LODFEED accounted for %0d stray + %0d buffered = %0d sample(s) against TERRAIN.MIPFEED's %0d sent -- it saw samples the mip pass never offered, so mg_start_o and mg_fine_valid_o ARE out of step",
             terr_lodfeed_stray_samples_o,
             TERR_LATTICE_VERTS * terr_lodfeed_lattices_walked_o,
             terr_lodfeed_stray_samples_o +
               TERR_LATTICE_VERTS * terr_lodfeed_lattices_walked_o,
             terr_mip_samples_sent_o);
    if (terr_lodfeed_stray_samples_o != 0)
      $display("SMOKE: NOTE TERRAIN.LODFEED reported %0d 'stray' sample(s) of the %0d surplus the second mip pass sends. THIS IS THE MISLABELLING DESCRIBED ABOVE, NOT A PAGING FAULT -- see zhao_terrain_lodfeed's classifier. Do not chase it as a spine defect.",
               terr_lodfeed_stray_samples_o,
               terr_mip_samples_sent_o -
                 TERR_LATTICE_VERTS * terr_lodfeed_lattices_walked_o);
    // AND THE CHAIN IS ASSERTED CONSISTENT RATHER THAN ASSERTED BUSY. Whatever
    // the stimulus reaches, every deviation lodfeed emitted must have been
    // accepted by the histogram: the two are joined by one handshake and
    // nothing sits between them.
    //
    // CORRECTED 2026-09-20 (terrain9). THE TWO SIDES WERE IN DIFFERENT UNITS
    // and the check could only ever have held at zero. It read
    // `hist_events_o != terr_lodfeed_dev_records_o`, and its comment argued
    // "this holds at zero AND at a thousand, so it is a check that survives
    // the fixture getting better". It does not. `dev_records_o` counts
    // RECORDS; `events_o` counts EVENTS, and this composition presents each
    // record on THREE LANES -- dev1, dev2, dev3, with lane 3's `lane_valid`
    // held low (the R70 width adaptation, `zhao_console_core.sv:10023-10025`).
    // MEASURED the first time the stream ever moved: 48 records, 144 events.
    // Exactly 3x, and the check fired as "the I18 join is dropping events" on
    // a join that was dropping nothing.
    //
    // It is the same shape as the defect it sits next to and as the one this
    // packet repaired in `zhao_terrain_lodfeed`: an equality written while both
    // operands were ZERO is not a law, it is 0 == 0, and it reads as a passing
    // check for as long as nothing exercises it.
    if (hist_events_o != HIST_CW'(terr_lodfeed_dev_records_o * HIST_R70_LANES))
      $fatal(1, "SMOKE: TERRAIN.LODFEED emitted %0d deviation record(s), so MEASURE.HISTOGRAM should have accepted %0d event(s) on %0d valid lanes, and it accepted %0d -- the I18 join is dropping events",
             terr_lodfeed_dev_records_o,
             terr_lodfeed_dev_records_o * HIST_R70_LANES,
             HIST_R70_LANES, hist_events_o);
    // AND THE AGGREGATION IS BOUNDED, which is the other half of the histogram's
    // own stated contract ("`updates_o` counts memory updates while `events_o`
    // counts events, and their ratio IS the aggregation",
    // `zhao_measure_histogram.sv:124-126`). One record's three deviations land
    // in one bin when they agree and in up to three when they do not, so the
    // memory updates must sit between one per record and one per event. On the
    // flat zero lattice this bench plays, all three agree and it is the lower
    // bound -- asserted as a RANGE rather than pinned to that, because pinning
    // it would assert the fixture's flatness rather than the block's law.
    if ((hist_updates_o < HIST_CW'(terr_lodfeed_dev_records_o)) ||
        (hist_updates_o > hist_events_o))
      $fatal(1, "SMOKE: MEASURE.HISTOGRAM made %0d memory update(s) for %0d event(s) over %0d record(s) -- outside [records, events], so the lane aggregation is not doing what its contract says",
             hist_updates_o, hist_events_o, terr_lodfeed_dev_records_o);
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
    // `$finish` runs this block to the end of its time step, and the `else`
    // arm below ends at the TERRAIN verdict's `endif` -- every production
    // check after that still executes against the mutant. This arm survived
    // that only because production's geometry counters happen to agree on
    // its design; the R197 arm does not (nothing enters GEOM.CLIP by design)
    // and printed nine %Fatal lines under a PASSING exit code on 2026-09-20.
    // `disable run` leaves the named block NOW, so a mutant verdict is the
    // last thing this bench says.
    disable run;
`elsif ZHAO_MUT_UNTEX_DECL
    // R197's door, INVERTED polarity. With the mesh producer declaring
    // UNTEXTURED against a material that samples (this smoke's record 1 --
    // the plain run asserts `render_texture_samples_o != 0` on it), every
    // triangle the window releases must be consumed at the door and counted,
    // and NONE may enter GEOM.CLIP. The count is compared to the SAME
    // reference number the plain run compares `geom_clip_submitted_o` to, so
    // "the counter fired" cannot be satisfied by a stray increment.
    $display("SMOKE: MUTANT zhao_console_core_untex_decl_mutant -- geom_untex_refused_o=%0d (want %0d) clip_submitted=%0d setup_submitted=%0d raster_pixels=%0d matwin[unpub/underflow]=[%0d %0d]",
             geom_untex_refused_o, SGF_EXP_REPLAYED, geom_clip_submitted_o,
             geom_setup_triangles_submitted_o, render_pixels_o,
             mat_win_err_unpublished_o, mat_win_err_underflow_o);
    if (geom_untex_refused_o == 0)
      $fatal(1, "MUTANT FAILED: geom_untex_refused_o stayed 0 with GEOM_REPLAY_UNTEX_DECL=1 -- the untextured door did not refuse, so its zero in production is not evidence");
    if (geom_untex_refused_o != SGF_EXP_REPLAYED)
      $fatal(1, "MUTANT FAILED: geom_untex_refused_o=%0d but the reference replays %0d triangles -- the door refused the wrong number",
             geom_untex_refused_o, SGF_EXP_REPLAYED);
    if (geom_clip_submitted_o != 0 || geom_setup_triangles_submitted_o != 0)
      $fatal(1, "MUTANT FAILED: %0d triangle(s) entered GEOM.CLIP (%0d reached SETUP) past a refusal -- a refused primitive was sampled after all",
             geom_clip_submitted_o, geom_setup_triangles_submitted_o);
    // The refusal happens BEFORE the material window's span, so the window's
    // structural guards must stay silent: a refusal that leaked into the
    // occupancy would show here as an underflow or an unpublished departure.
    if (mat_win_err_unpublished_o != 0 || mat_win_err_underflow_o != 0)
      $fatal(1, "MUTANT FAILED: the material window's guards fired (unpublished %0d, underflow %0d) -- the door's refusal entered the accounted span",
             mat_win_err_unpublished_o, mat_win_err_underflow_o);
    $display("SMOKE: MUTANT PASS -- geom_untex_refused_o fired %0d time(s), nothing entered GEOM.CLIP, the window's accounting held. The detector works; production's zero is a measurement.",
             geom_untex_refused_o);
    $finish;
    disable run;   // see the slot-overflow arm above: $finish alone is not a stop
`else

    // ---- 1. THE COMMAND WAS READ OVER THE BRIDGE -------------------------
    // TWO SETS SINCE 2026-09-25 (TERRAINAUX), not one: the first pages the
    // patches in and the second COMPOSES them, which is what a real frame
    // loop does and the only stimulus that reaches the compose door. See
    // the `PACKET P-TERRAIN, SECOND SUBMISSION` block above for why one
    // submission could never open it.
    if (terr_cmd_sets_accepted_o != N_TERR_SUBMITS)
      $fatal(1, "SMOKE: TERRAIN.CMD accepted %0d sets and refused %0d (verdict %0d, crc seen %08x against %08x) -- the host packet was rejected",
             terr_cmd_sets_accepted_o, terr_cmd_sets_refused_o,
             terr_cmd_done_verdict_o, terr_cmd_done_crc_seen_o, terr_cmd_list_crc_i);
    if (terr_cmd_crc_fails_o != 0)
      $fatal(1, "SMOKE: TERRAIN.CMD folded a different CRC than the bench did -- the played bridge is not returning the bytes that were written");
    if (terr_cmd_bridge_errs_o != 0)
      $fatal(1, "SMOKE: TERRAIN.CMD saw %0d bridge errors -- the played HPS engine is malforming bursts",
             terr_cmd_bridge_errs_o);
    if (terr_cmd_records_emitted_o != N_TERR_REC * N_TERR_SUBMITS)
      $fatal(1, "SMOKE: TERRAIN.CMD emitted %0d of %0d records -- it read the list and did not produce it",
             terr_cmd_records_emitted_o, N_TERR_REC * N_TERR_SUBMITS);

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
    // THE SECOND SUBMISSION CLAIMS AGAIN for any patch not yet resident when
    // it walks, so the claim count is a LOWER bound of N_TERR_REC rather than
    // an equality. What stays an equality is the SEAM above -- SEQ's count
    // against the directory's -- which is the thing this check was for.
    if (terr_seq_claims_issued_o < N_TERR_REC)
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
    // A BOUND AND NOT AN EQUALITY SINCE 2026-09-25 (TERRAINAUX), and the
    // reason is the SECOND SubmitTerrainSet the compose pass needs: a patch
    // that is still loading when the second walk reaches it is claimed and
    // loaded again, so every cumulative spine counter is now "at least once
    // per patch" rather than "exactly once". The SEAM equalities above --
    // SEQ's count against the directory's, LOADQ's against SEQ's -- are
    // untouched, and they are what these checks were actually for: a dropped
    // or duplicated handshake still fails there.
    if (terr_seq_loads_issued_o < N_TERR_REC)
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
    // RESTATED AS THE FRESH COUNT, 2026-09-25 (TERRAINAUX). The second
    // submission legitimately claims a patch the directory already holds,
    // so `claims_same_o` is no longer expected to be zero. What the check
    // was FOR survives exactly: N_TERR_REC records with distinct
    // {island, ix, iz} must produce N_TERR_REC DISTINCT fresh claims, and
    // two records presenting one key still cannot.
    if ((terr_seq_claims_issued_o - terr_seq_claims_same_o) < N_TERR_REC)
      $fatal(1, "SMOKE: %0d claims of which %0d came back SAME -- fewer than %0d were FRESH, so two records with distinct patch coordinates are presenting one key",
             terr_seq_claims_issued_o, terr_seq_claims_same_o, N_TERR_REC);
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
    // AGAINST THE JOBS THAT REACHED THE LOADER, not against N_TERR_REC:
    // with two submissions the loader is handed more than one job per patch
    // and the law it states is ONE JOB, ONE COMPLETION. `terr_lq_issued_o`
    // is the count of jobs handed on, so this is now the law itself rather
    // than a number that happened to equal it.
    if ((terr_pl_pages_loaded_o + terr_pl_pages_faulted_o + terr_pl_pages_refused_o) != terr_lq_issued_o)
      $fatal(1, "SMOKE: %0d jobs produced %0d loaded + %0d faulted + %0d refused completions -- 'one job, one completion' is broken (or the wait timed out at guard=%0d)",
             // WAS N_TERR_REC -- CORRECTED 2026-09-26 (TERRTRI). The check
             // compares against terr_lq_issued_o; the message printed a
             // DIFFERENT quantity, so a genuine shortfall of 4 completions
             // against 5 jobs read as "3 jobs produced 4 loaded" -- which
             // looks like MORE completions than jobs, the opposite of the
             // fault, and sends a reader hunting a duplicate instead of a
             // straggler. A message must print the operands it compared.
             terr_lq_issued_o, terr_pl_pages_loaded_o, terr_pl_pages_faulted_o,
             terr_pl_pages_refused_o, guard);

    // EVERY COMPLETION REACHED THE DIRECTORY, AND EVERY ONE WAS GOOD.
    //
    // REWRITTEN 2026-09-20 (terrain9), AND THE OLD FORM IS THE INSTRUCTIVE
    // PART. It read `if (terr_res_crc_failures_o != N_TERR_REC) $fatal(...
    // "fin_* is not reaching TERRAIN.RESIDENCY")` -- it used the count of CRC
    // FAILURES as a proxy for the count of completions that ARRIVED, which is
    // only a proxy while every page is guaranteed to fail. So the check
    // asserted the stimulus's defect: repair the fixture and it goes RED on a
    // machine that has strictly improved, which is CLAUDE.md's "do not write a
    // test that asserts the bug" with the bug living in the bench rather than
    // in the RTL. It is now two checks that mean what they say.
    // A BOUND AND NOT AN EQUALITY SINCE 2026-09-25 (TERRAINAUX), and the
    // reason is the SECOND SubmitTerrainSet the compose pass needs: a patch
    // that is still loading when the second walk reaches it is claimed and
    // loaded again, so every cumulative spine counter is now "at least once
    // per patch" rather than "exactly once". The SEAM equalities above --
    // SEQ's count against the directory's, LOADQ's against SEQ's -- are
    // untouched, and they are what these checks were actually for: a dropped
    // or duplicated handshake still fails there.
    if (terr_res_claims_o < N_TERR_REC)
      $fatal(1, "SMOKE: the directory recorded %0d claims for %0d jobs -- TERRAIN.SEQ's claims are not reaching TERRAIN.RESIDENCY",
             terr_res_claims_o, N_TERR_REC);
    if (terr_res_crc_failures_o != 0)
      $fatal(1, "SMOKE: the directory rejected %0d of %0d page completion(s) on CRC -- the bench seals each page with the CRC of the bytes it wrote, so a mismatch is a transport fault, not a fixture one",
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

    // ---- 5. THE PAGE LOADS, AND THE CHAIN BEHIND IT MOVES ----------------
    // REPLACED 2026-09-20 (terrain9). What stood here gave TWO declared reasons
    // for `terr_res_resident_o == 0`, and BOTH had expired:
    //   * "the pages this bench plays are ZEROS, so their CRC cannot match" --
    //     the pages are no longer zeros (they carry a 2.1 header and their own
    //     body CRC), and the stated cause was wrong even then: the loader's
    //     own counters read `crc_fails=0, hdr_ident_fails=3`, so the refusal
    //     was the IDENTITY test, not the CRC;
    //   * "even a clean page would stop in ST_MIPGEN, because ... the second's
    //     producer (TERRAIN.MIPFEED) is not composed" -- TERRAIN.MIPFEED IS
    //     composed, at `zhao_console_core.sv:15281`, since 2026-09-19. A bench
    //     comment naming a block as absent is exactly the false-absence shape
    //     this campaign has now found sixteen times, and this one was load
    //     bearing: it is the sentence that made a zero look expected.
    //
    // These are the R70 chain, asserted as ONE reading. TERRAIN.MIPREQ triggers
    // only on a load that finished OK, so each link below is unreachable while
    // the one above it is zero -- which is why the whole chain sat at zero and
    // no check could see it.
    // A BOUND AND NOT AN EQUALITY SINCE 2026-09-25 (TERRAINAUX), and the
    // reason is the SECOND SubmitTerrainSet the compose pass needs: a patch
    // that is still loading when the second walk reaches it is claimed and
    // loaded again, so every cumulative spine counter is now "at least once
    // per patch" rather than "exactly once". The SEAM equalities above --
    // SEQ's count against the directory's, LOADQ's against SEQ's -- are
    // untouched, and they are what these checks were actually for: a dropped
    // or duplicated handshake still fails there.
    if (terr_pl_pages_loaded_o < N_TERR_REC)
      $fatal(1, "SMOKE: %0d of %0d page(s) loaded (faulted=%0d refused=%0d, verdict=%0d, hdr_ident_fails=%0d) -- the bench now writes a spec 2.1 header and the page's own body CRC, so a page that does not load is a spine fault",
             terr_pl_pages_loaded_o, N_TERR_REC, terr_pl_pages_faulted_o,
             terr_pl_pages_refused_o, terr_pl_fault_verdict_o,
             terr_pl_hdr_ident_fails_o);
    if (terr_mip_samples_sent_o == 0)
      $fatal(1, "SMOKE: TERRAIN.MIPFEED pushed ZERO fine-lattice samples after %0d page load(s) (mipreq requests=%0d issued=%0d) -- ruling R70's metric has no stream behind it",
             terr_pl_pages_loaded_o, terr_mipreq_requests_o, terr_mipreq_issued_o);
    if (terr_lodfeed_lattices_walked_o == 0)
      $fatal(1, "SMOKE: TERRAIN.LODFEED walked ZERO lattices against %0d fine sample(s) -- mg_start_o is not reaching it",
             terr_mip_samples_sent_o);
    if (terr_lodfeed_dev_records_o == 0)
      $fatal(1, "SMOKE: TERRAIN.LODFEED walked %0d lattice(s) and emitted ZERO deviation records -- zhao_terrain_loddev produced nothing for MEASURE.HISTOGRAM",
             terr_lodfeed_lattices_walked_o);
    if (hist_events_o == 0)
      $fatal(1, "SMOKE: MEASURE.HISTOGRAM accepted ZERO events while TERRAIN.LODFEED emitted %0d record(s) -- the entry I18 / ruling R70 join is not carrying a value",
             terr_lodfeed_dev_records_o);
    $display("SMOKE: NOTE the R70 chain CARRIES A VALUE: %0d page(s) loaded -> mipreq issued=%0d -> mipfeed samples_sent=%0d -> lodfeed walked=%0d dev_records=%0d -> histogram events=%0d, resident=%0d. Before 2026-09-20 every one of these was ZERO because the bench played a headerless page and TERRAIN.MIPREQ triggers only on a load that finished OK.",
             terr_pl_pages_loaded_o, terr_mipreq_issued_o,
             terr_mip_samples_sent_o, terr_lodfeed_lattices_walked_o,
             terr_lodfeed_dev_records_o, hist_events_o, terr_res_resident_o);

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
    $display("SMOKE: vattr join uv_waits=%0d poison=%0d done_stall=%0d",
             geom_va_uv_waits_o, geom_va_poison_o, geom_va_done_stall_o);
    if (geom_va_poison_o)
      $fatal(1, "SMOKE: GEOM.VATTR poisoned a clean batch");
    // OWNER RULING R88. The watchdog is the negative control here and the
    // POSITIVE control lives in geom_vattr_directed cases L and M, where legal
    // stimulus fires it. A non-zero here means the geometry front end spent
    // STALL_LIMIT clocks owing a row or a colour with nothing moving -- the
    // wedge that has no timeout -- and the smoke must not pass through it.
    if (geom_va_done_stall_o != 0)
      $fatal(1, "SMOKE: GEOM.VATTR's done_o watchdog fired %0d time(s) -- the front end stalled",
             geom_va_done_stall_o);
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
    // R197's untextured door. The mesh producer declares TEXTURED (format 0
    // carries u/v), so the door must refuse NOTHING here. This zero is a
    // measurement, not an invariant restated: `-UntexMutant` flips the
    // declaration and asserts the same counter reaches SGF_EXP_REPLAYED.
    if (geom_untex_refused_o != 0)
      $fatal(1, "SMOKE: the untextured door refused %0d triangle(s) from a producer that declares TEXTURED", geom_untex_refused_o);

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
    if (geom_attrpack_planes_o != 6 * geom_attrpack_triangles_o)
      $fatal(1, "SMOKE: GEOM.ATTRPACK packed %0d plane(s) for %0d triangle(s) and SIX lanes per triangle is the contract (owner decision R234 D1 added the three Gouraud lanes) -- a lane stopped asking and its plane is the PREVIOUS triangle's",
             geom_attrpack_planes_o, geom_attrpack_triangles_o);
    // ---- TEXTURE EVIDENCE (entry I49, 2026-09-20) -------------------------
    // MEASURED, NOT READ OFF THE SOURCE. Until this commit the composed
    // console exported NO texture counter at all: seven dangled at the shell's
    // instantiation of the bin pipe and the eighth was sunk inside the raster
    // tile pipe as an unused wire. So "the island samples nothing" was a
    // reading of the RTL, never a measurement. It is a measurement now, and
    // the seven beside it are what stops a zero from being ambiguous: no
    // fragment reached the island at all, versus fragments that reached it
    // and were refused.
    $display("SMOKE: texture  fragments=%0d samples=%0d cache[hit/miss]=[%0d %0d] palette_lookups=%0d plan_accepted=%0d dispatch_accepted=%0d combine_refused=%0d",
             render_texture_fragments_o, render_texture_samples_o,
             render_texture_cache_hits_o, render_texture_cache_misses_o,
             render_texture_palette_lookups_o, render_texture_plan_accepted_o,
             render_texture_dispatch_accepted_o, render_texture_combine_refused_o);
    // ======================================================================
    // POST.GATHER, composed 2026-09-21 (entry I17 (c), owner ruling R195)
    // ======================================================================
    // WHAT THIS PROVES AND WHAT IT DOES NOT. It proves the SEAM carries: the
    // shell's resolved-fragment tap reaches R195's law, the law reaches R5's
    // accumulator, the accumulator's tile flush reaches the plane, and the
    // plane answers POST.COMPOSITE. It does NOT prove the bloom looks right;
    // that is `reports/post-gather-law/gather_law_contact.png`, which the
    // owner has already judged, and it is not a thing a bench can assert.
    //
    // THE FOUR TAG COUNTERS ARE A PARTITION, and the sum is checked rather
    // than the parts. Every accepted fragment lands in exactly one of
    // untagged / below-knee / lit / reserved-channel, so if the four do not
    // add up to `gather_fragments_o` a branch is wrong -- and no single
    // counter read on its own could have told anyone. CLAUDE.md: a test that
    // checks WHAT came out cannot see HOW MANY TIMES the machine did it.
    $display("SMOKE: gather   frags=%0d [untagged=%0d below_knee=%0d lit=%0d reserved=%0d] sat=%0d clamps=%0d cells_flushed=%0d",
             gather_fragments_o, gather_frag_untagged_o,
             gather_frag_below_knee_o, gather_frag_lit_o,
             gather_reserved_channel_o, gather_glow_saturations_o,
             gather_disp_clamps_o, gather_cells_flushed_o);
    $display("SMOKE: gather   plane written=%0d oob=%0d commits=%0d | reads[gd/gg]=[%0d %0d] miss[gd/gg]=[%0d %0d] | overrun=%0d rdw=%0d",
             gather_cells_written_o, gather_oob_writes_o,
             gather_plane_commits_o, gather_gd_reads_o, gather_gg_reads_o,
             gather_gd_miss_o, gather_gg_miss_o,
             gather_flush_overrun_o, gather_rdw_collide_o);
    if (gather_fragments_o == 0)
      $fatal(1, "SMOKE: POST.GATHER accumulated NOTHING while the raster resolved %0d pixel(s). The shell's gth_* tap, entry I17's own named obstacle, is not carrying.",
             render_pixels_o);
    if ((gather_frag_untagged_o + gather_frag_below_knee_o
       + gather_frag_lit_o + gather_reserved_channel_o) != gather_fragments_o)
      $fatal(1, "SMOKE: the tag law's four counters do not partition the stream: %0d + %0d + %0d + %0d != %0d. A fragment was counted twice or not at all.",
             gather_frag_untagged_o, gather_frag_below_knee_o,
             gather_frag_lit_o, gather_reserved_channel_o, gather_fragments_o);
    // ----------------------------------------------------------------------
    // THE TAG ITSELF, both polarities (tagprod, 2026-09-21)
    // ----------------------------------------------------------------------
    // POST.COMPOSITE's `bloom_cells_contributing_o` was connected in the core
    // and read by NOTHING -- an unwatched counter, which is the shape
    // CLAUDE.md's uncashed-cheque chapter is about. It is the bloom stage's own
    // account of how many quarter-res cells had anything in them, so it
    // separates "the plane was lit" from "the plane was lit and the compositor
    // looked at it", and the two forms below are its negative and positive
    // control.
    $display("SMOKE: bloom    cells_contributing=%0d disp_edge_clamps=%0d",
             post_bloom_cells_contributing_o, post_displacement_edge_clamps_o);
`ifdef ZHAO_SMOKE_GLOW_TAG
    // POSITIVE, AND THE EQUALITY IS NOT THE ONE IT LOOKS LIKE IT SHOULD BE.
    // The obvious assertion -- "the tag was on every triangle, so all
    // `gather_fragments_o` must be LIT" -- was written first and FAILED at
    // 1,062 of 2,560. It was wrong, and what it taught is worth more than the
    // assertion was:
    //
    //   `gather_fragments_o` IS NOT THE COVERED-FRAGMENT COUNT. RASTER.RESOLVE
    //   sweeps a touched TILE WHOLE, so the gather sees all 256 pixels of every
    //   tile a triangle entered, covered or not. This bench touches 10 tiles
    //   (cells_flushed=160, sixteen cells per tile) = 2,560 pixels, of which
    //   1,062 are actually covered. The other 1,498 carry the TILE CLEAR --
    //   colour 0x0000 and tag 0x00 -- so UNTAGGED is the correct answer for
    //   them and an all-lit frame would have been the wrong one.
    //
    // Three independent counters agree on that reading and none of them is
    // this block's: RASTER.FBWRITE issued 94,720 words = 92,160 post + 2,560
    // raster; 160 bursts x 16 = 2,560; and the texture island shaded 1,190
    // fragments into 1,062 distinct addresses, the difference being overdraw
    // (state 0 is REPLACE with the depth test OFF).
    //
    // SO THE ASSERTION IS A CROSS-CHECK BETWEEN TWO INSTRUMENTS THAT SHARE NO
    // LOGIC: `gather_frag_lit_o` is a counter inside R195's law, and
    // `smk_glow_px_q` is this bench walking the framebuffer for the tail's own
    // colour. Every pixel that received the tail's COLOUR must also have
    // received its TAG, and no other pixel may be lit. A tail that reached only
    // some fragments, or a tag leaking onto the tile clear, breaks it -- and a
    // `> 0` test would have passed through both.
    // INVERTED BY R234 D1, NOT DELETED. The tail still carries 0x%06h and it
    // must no longer arrive: `zhao_raster_tile_pipe_v2` overwrites
    // `vertex_rgb` per fragment from the Gouraud lanes, so a single word of the
    // tail's colour in the frame would mean the overwrite is not happening and
    // the reconnection is cosmetic.
    if (smk_glow_px_q != 0)
      $fatal(1, "SMOKE: -GlowTag put 0x%06h on the continuation tail and %0d framebuffer word(s) came out 0x%04h. Owner decision R234 D1 makes `vertex_rgb` the INTERPOLATED lit colour off attribute lanes 3..5, so the tail's colour must not reach the fragment shader at all.",
             SMK_GLOW_RGB_C, smk_glow_px_q, SMK_GLOW_PX565_C);
    if (smk_lit_px_q == 0)
      $fatal(1, "SMOKE: -GlowTag tagged the stream and NOT ONE framebuffer word carries a colour. The glow BORROWS the fragment's own colour, so with a black frame this form would be testing a tag against a plane of zeros -- which is what it existed to exclude.");
    // THE CROSS-CHECK SURVIVES D1, over a different quantity. It used to
    // difference `gather_frag_lit_o` against the count of pixels carrying the
    // TAIL's colour; it now differences it against the count of pixels carrying
    // ANY colour. Both are still two instruments sharing no logic --
    // `gather_frag_lit_o` is a counter inside R195's law, `smk_lit_px_q` is this
    // bench walking memory -- and both still say the same thing: every COVERED
    // fragment got the tag, and no tile-clear word did.
    if (gather_frag_lit_o != smk_lit_px_q)
      $fatal(1, "SMOKE: %0d pixel(s) carry a COLOUR and %0d fragment(s) came out LIT. Every covered fragment carries both the tail's tag and the lanes' colour, so they must agree exactly -- [untagged=%0d below_knee=%0d reserved=%0d] of %0d resolved.",
             smk_lit_px_q, gather_frag_lit_o, gather_frag_untagged_o,
             gather_frag_below_knee_o, gather_reserved_channel_o, gather_fragments_o);
    if (gather_frag_below_knee_o != 0 || gather_reserved_channel_o != 0)
      $fatal(1, "SMOKE: tag 0x%02h is channel GLOW at strength 63, whose ramp gain is 68 -- it is neither below the knee (%0d) nor a reserved channel (%0d). R195's classifier is decoding the tag differently from the frozen `(channel << 6) | strength`.",
             SMK_GLOW_TAG_C, gather_frag_below_knee_o, gather_reserved_channel_o);
    if (post_bloom_cells_contributing_o == 0)
      $fatal(1, "SMOKE: %0d LIT fragment(s) reached R195's law and POST.COMPOSITE's bloom stage found NO cell contributing. The gather's plane flush and the compositor's plane read disagree about where the cells are.",
             gather_frag_lit_o);
    $display("SMOKE: -GlowTag  END TO END FROM THE ABI: tag 0x%02h in the uploaded MaterialRecord's `fragment_decl` -> MEM.UPLOAD -> MATERIAL.RESOLVE -> the window's published span -> %0d covered pixel(s) of %0d resolved -> %0d LIT by R195's law -> %0d bloom cell(s) -> the frame. No bench port is involved anywhere in that chain.",
             SMK_GLOW_TAG_C, smk_lit_px_q, gather_fragments_o,
             gather_frag_lit_o, post_bloom_cells_contributing_o);
`else
    // NEGATIVE, and it is the half that makes the positive mean anything.
    //
    // REWORDED 2026-09-25 (FRAGSTATE), because the REASON changed even though
    // the number did not, and a check whose stated cause has gone stale is the
    // shape this repository keeps getting caught by. It used to read "with
    // `tri_continuation_tail_i` at its boundary value every fragment carries
    // tag 0" -- but that port no longer exists at this edge. The tag now comes
    // from the MATERIAL, and in this form the uploaded record leaves
    // `fragment_decl` bit 0 CLEAR, so the material declares no profile, the
    // composer takes `TAIL_EFFECT_TAG_DEFAULT_C` -- the owner directive's named
    // default of 0 -- and every fragment is untagged.
    //
    // SO THIS PAIR IS NOW A REAL A/B ON ONE ABI FIELD. The two forms differ in
    // exactly one byte of one uploaded MaterialRecord, and everything else
    // about the run is identical. Untagged here, lit there.
    if (gather_frag_untagged_o != gather_fragments_o)
      $fatal(1, "SMOKE: the uploaded material leaves `fragment_decl` bit 0 clear, so the composer takes the default effect tag 0 and all %0d fragment(s) must be UNTAGGED -- %0d were [below_knee=%0d lit=%0d reserved=%0d]. Something is putting a tag on the stream.",
             gather_fragments_o, gather_frag_untagged_o, gather_frag_below_knee_o,
             gather_frag_lit_o, gather_reserved_channel_o);
    // And the bloom must find nothing, for the same reason. If this fired while
    // the material declared nothing, the tag would be arriving from somewhere
    // that is not the record -- which after the port retirement means somewhere
    // nobody authored.
    if (post_bloom_cells_contributing_o != 0)
      $fatal(1, "SMOKE: no fragment was tagged and POST.COMPOSITE's bloom stage still found %0d contributing cell(s) -- the plane is not being cleared between frames, or the read is off the flush's coordinate",
             post_bloom_cells_contributing_o);
    // The other half of -GlowTag's cross-check. It used to read "with the port
    // at its boundary zero no pixel may carry that colour"; since R234 D1 the
    // tail's `vertex_rgb` reaches nothing in EITHER form, so this now says the
    // same thing in both and is kept in both for exactly that reason.
    if (smk_glow_px_q != 0)
      $fatal(1, "SMOKE: `tri_continuation_tail_i` is '0 and the tile pipe overwrites `vertex_rgb` from the Gouraud lanes regardless -- and %0d framebuffer word(s) came out 0x%04h anyway.",
             smk_glow_px_q, SMK_GLOW_PX565_C);
    // AND THE D1 EVIDENCE, IN THE FORM THAT HAS NO TAG AND NO GLOW.
    //
    // This number was STRUCTURALLY ZERO in the plain form until 2026-09-21.
    // `frag_vert_rgb_i` came from `tri_continuation_tail_i`'s `vertex_rgb`, a
    // boundary port this bench drives '0, and fragment state 0 makes the vertex
    // colour the pixel -- so every covered pixel resolved to 0x0000. The
    // -GlowTag form measured it from the other side: 1,062 glow words and
    // "every one of the other 1,498 drawn words is 0x0000".
    //
    // Owner decision R234 D1 made `vertex_rgb` the interpolated lit colour off
    // attribute lanes 3..5. GEOM.LIGHT in this same run reports lit vertices
    // with `last_rgb` matching its reference, and GEOM.VATTR stores them -- so a
    // frame in which nothing carries a colour means the chain is severed again
    // somewhere between `zhao_geom_vattr` and `zhao_raster_fragment`, and every
    // other counter in this bench would still read healthy. That is the failure
    // this one number exists to catch, and it is the only check in the tree that
    // can see it in the COMPOSED console.
    //
    // It deliberately does not predict the colour. The per-pixel oracle is
    // `geom_bin_pipe_v2_directed`, which walks it through `current_rast_attr`
    // and `lit_unit8`; a predicted constant here would be the defect the
    // sentinel-overwrite count above already had to be rescued from once.
    if (smk_lit_px_q == 0)
      $fatal(1, "SMOKE: every drawn pixel is black. GEOM.LIGHT lit %0d vertex/vertices and GEOM.VATTR stored %0d colour(s), and none of it reached the framebuffer -- the Gouraud delivery of owner decision R234 D1 is severed again.",
             geom_light_vertices_lit_o, geom_va_colours_written_o);
`endif
    // Every tile writes all SIXTEEN cells including zeros -- that is R5's
    // reason there is no giant reset loop -- so a flush that is not a
    // multiple of sixteen means a burst was cut short.
    if ((gather_cells_flushed_o % 32'd16) != 32'd0)
      $fatal(1, "SMOKE: %0d cells flushed, which is not a whole number of tiles. A flush burst was interrupted.",
             gather_cells_flushed_o);
    if (gather_cells_written_o == 0)
      $fatal(1, "SMOKE: POST.GATHER flushed %0d cell(s) and the plane store accepted NONE (oob=%0d). The tile origin or the plane geometry is wrong, not the gather.",
             gather_cells_flushed_o, gather_oob_writes_o);
    // TWO TRIPWIRES. Both are DELIBERATELY FIRED in
    // `tests/compositor/post_gather_store_directed.cpp`, so quoting their
    // silence here is quoting an instrument that has been seen to work --
    // which is the whole of CLAUDE.md's rule about a detector reading zero.
    if (gather_flush_overrun_o != 32'd0)
      $fatal(1, "SMOKE: a tile closed %0d time(s) while a flush was still draining. The store's origin moved out from under a burst, so cells landed at the wrong screen address -- the metadata-swap shape, in the plane.",
             gather_flush_overrun_o);
    if (gather_rdw_collide_o != 32'd0)
      $fatal(1, "SMOKE: the plane was read and written on the same cell %0d time(s). The raster and post phases OVERLAPPED, and the single-plane decision rests on them not doing so.",
             gather_rdw_collide_o);
    if (geom_attrpack_triangles_o == 0)
      $fatal(1, "SMOKE: GEOM.ATTRPACK never saw a triangle, so every plane the shell read was its reset value");
    // THE SAMPLE IS A HARD GATE, and it is asserted several ways because a
    // single number can be right for the wrong reason.
    if (render_texture_fragments_o == 0)
      $fatal(1, "SMOKE: no fragment reached the texture island at all -- this is a RASTER failure, not a texture one");
    if (render_texture_samples_o == 0)
      $fatal(1, "SMOKE: %0d fragment(s) reached the island and NOT ONE TMU SAMPLE was published. plan_accepted=%0d dispatch_accepted=%0d combine_refused=%0d -- a zero plan count means the flat request asked for no sample; a non-zero plan with zero samples means the binding page or the witnesses refused it",
             render_texture_fragments_o, render_texture_plan_accepted_o,
             render_texture_dispatch_accepted_o, render_texture_combine_refused_o);
    if (render_texture_combine_refused_o != 32'd0)
      $fatal(1, "SMOKE: the combiner refused %0d fragment(s) -- recipe and sample_count disagree, which is material_count_legal's law",
             render_texture_combine_refused_o);
    // A SAMPLE WITH NO CACHE TRAFFIC WOULD BE A SAMPLE OF NOTHING. The fill
    // socket is the only place a texel can come from in this console, so the
    // line count and the beat count are what say a real page was READ: eight
    // 16-bit beats per line, exactly, or the cache would have counted a
    // protocol fault instead.
    if (tfill_lines_q == 0 || tfill_beats_q != tfill_lines_q * 32'd8)
      $fatal(1, "SMOKE: the texture fill socket served %0d line(s) in %0d beat(s) -- a line is exactly eight",
             tfill_lines_q, tfill_beats_q);

    $display("SMOKE: NOTE raster pixels=%0d over %0d burst(s), every issued word retired by the arbiter, from %0d triangle(s) in %0d admitted frame(s), and %0d of those fragments CARRIED A TEXEL. As of 2026-09-20 (entry I49) this bench no longer plays the material seam and the console no longer needs it to: CMD.EXEC lowers a PublishResource, MEM.UPLOAD lands the MATERIAL_SET in RENDER.ASSET_POOL and publishes 5f.1's row, GEOM.DRAWJOB puts the draw's material_set in the job's sideband, it rides the meshlet through GEOM.MESHFETCH, GEOM.ASSETFETCH, GEOM.ASSEMBLE and GEOM.REPLAY beside the meshlet's own material id and the draw's semantic weight, and zhao_material_window reads all three off ONE triangle record and issues the resolve. MATERIAL.RESOLVE finds the set in the directory, fetches RECORD 1 -- the record the MESHLET names, which differs from record 0 in every field the flat request carries -- as ENGINE1 through the adapter's third requester, and the window publishes the answer as the material half of tri_flat_request. The binding page this bench seals (a DIRECT RGB565 row, whose palette slot and generation are ZERO BY THE ROW'S OWN LEGALITY LAW rather than by anyone's choice) accepts the witnesses, the cache misses %0d line(s), the fill socket serves each as eight 16-bit beats, and the island publishes a TMU sample into every fragment. WHAT IS STILL NOT DRIVEN, and is named rather than left to be re-derived: base_rgb is the VERTEX's colour and GEOM.VATTR holds a PER-VERTEX one, so the flat base colour stays a named constant (entry I20's remaining half); tri_continuation_tail_i and tri_fragment_state_i are still boundary ports -- though the tail's vertex_rgb FIELD NO LONGER REACHES THE FRAGMENT, because owner decision R234 D1 (2026-09-21) made zhao_raster_tile_pipe_v2 overwrite it per fragment from attribute lanes 3..5, so what the tail still supplies is vertex_alpha, effect_tag and stencil_reference; and a CLUT material's palette slot and generation have no producer in this console at all, which mat_win_clut_unowned_o counts rather than hides.",
             render_pixels_o, render_bursts_o,
             geom_setup_triangles_submitted_o, v2_frames_admitted_o,
             render_texture_samples_o, render_texture_cache_misses_o);

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

    // 5. THE FIELD ENGINE IS COMPOSED AND REACHABLE (entries I5 and I42).
    //    The F profile is ARMED at a slot nothing was loaded into, so every
    //    particle offered walked the whole join. Each of these fails for a
    //    DIFFERENT reason, which is why they are separate: a grant proves the
    //    arbiter accepted the adapter's request, a noprog proves the
    //    `hdr_loaded` interlock answered, and the adapter's own noprog proves
    //    the status came back down the shared response bus to the client that
    //    asked.
    if (fld_grants_o == 32'd0)
      $fatal(1, "SMOKE: FIELD's arbiter granted nothing while the FLOW adapter was armed -- client 1 does not reach the engine");
    if (fld_noprog_o == 32'd0)
      $fatal(1, "SMOKE: FIELD refused no run at an unloaded slot -- the program store's loaded interlock is not consulted, so a slot with no microcode would have been WALKED");
    if (part_fld_noprog_o != fld_noprog_o)
      $fatal(1, "SMOKE: the FLOW adapter saw %0d refusals against the engine's %0d -- the status did not reach the client that asked, and a caller reading only the lanes would have taken zeroes for a field",
             part_fld_noprog_o, fld_noprog_o);
    if (part_fld_samples_o != 32'd0)
      $fatal(1, "SMOKE: the FLOW adapter delivered %0d real accelerations with no program resident",
             part_fld_samples_o);
    if (fld_runs_o != 32'd0)
      $fatal(1, "SMOKE: FIELD completed %0d walks against zero loaded programs -- the sequencer ran something, and there is nothing in the store for it to have run",
             fld_runs_o);
    //    THE IDENTITY GUARD. It differences a record captured at request time
    //    against the live wire, so it CAN fire; a zero here is a measurement.
    if (part_fld_rec_changed_o != 32'd0)
      $fatal(1, "SMOKE: the FLOW adapter answered %0d records with an acceleration computed from a DIFFERENT record -- the join is carrying A's field onto B's particle",
             part_fld_rec_changed_o);

    // 5b. THE PROGRAM DOORBELL ANSWERS BOTH POSTS (entry I42, rulings R43/R20).
    if (fld_db_posts_o != 32'd2)
      $fatal(1, "SMOKE: the field doorbell consumed %0d posts against two offered", fld_db_posts_o);
    if (fld_db_lookups_o != 32'd1)
      $fatal(1, "SMOKE: the field doorbell handed %0d lookups to the directory against one posted -- the lookup phase has no producer",
             fld_db_lookups_o);
    if (fld_ret_seen_q != 32'd2)
      $fatal(1, "SMOKE: %0d returns came back for two answerable posts -- a post was consumed and never answered, which is the hang ruling R20 forbids",
             fld_ret_seen_q);
    if (fld_ret_lu_ok_q != 32'd1)
      $fatal(1, "SMOKE: the lookup's return did not say the directory answered");
    if (fld_ret_lu_hit_q != 32'd0)
      $fatal(1, "SMOKE: the directory reported a HIT for a hash nothing ever loaded");
    //    The order law FIRES. This is the positive control for
    //    `fld_db_commits_refused_o`, with legal stimulus, in the smoke itself.
    if (fld_db_commits_refused_o != 32'd1)
      $fatal(1, "SMOKE: a COMMIT for a slot whose header was never written was NOT refused (%0d refusals) -- the directory was offered a hash for microcode that is not there",
             fld_db_commits_refused_o);
    if (fld_ret_cm_refused_q != 32'd1)
      $fatal(1, "SMOKE: the refused commit's return did not carry the refusal back to the HPS -- refused and unanswered are the same thing from there");
    if (fld_db_commits_o != 32'd0)
      $fatal(1, "SMOKE: the field doorbell handed %0d commits to the directory, and the only one posted was refused",
             fld_db_commits_o);
    if (fld_db_ret_overflow_o != 32'd0)
      $fatal(1, "SMOKE: the field doorbell's return queue overflowed -- the credit reservation is wrong");

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
    // ---- THE CARRIER, entry I50 (owner rulings R58 and R69) ---------------
    // The node above did not arrive at a port. It came out of the played DDR
    // as a 64-byte burst on arbiter client 4, through `u_geom_loomfeed`, and
    // these are the numbers that say so. A carrier that composed nothing while
    // GEOM.LOOM's own counters read 1/1 is impossible -- there is no other
    // producer -- but the reverse is not: a stream could reach the loom while
    // the mailbox was never answered, which is exactly the silence R20 forbids.
    $display("SMOKE: loomfeed  posts=%0d streams=%0d nodes=%0d bursts=%0d c4_bursts=%0d c4_wait=%0d refused[align/hdr/loom]=[%0d %0d %0d] faulted=%0d replayed=%0d stalls=%0d errs=%0d wait=%0d overflow=%0d ret[seen/ok/refused/reason/lreason/nodes/ticket/plan]=[%0b %0b %0b %0d %0d %0d %08x %08x]",
             geom_loom_feed_posts_o, geom_loom_feed_streams_o, geom_loom_feed_nodes_o,
             geom_loom_feed_bursts_o, terr_hps_c4_bursts_o, terr_hps_c4_wait_cycles_o,
             geom_loom_feed_align_refused_o, geom_loom_feed_hdr_refused_o,
             geom_loom_feed_refused_o, geom_loom_feed_faulted_o,
             geom_loom_feed_replayed_o, geom_loom_feed_post_stalls_o,
             geom_loom_feed_bridge_errs_o, geom_loom_feed_wait_cycles_o,
             geom_loom_feed_ret_overflow_o,
             loom_ret_seen_q, loom_ret_ok_q, loom_ret_refused_q, loom_ret_reason_q,
             loom_ret_loom_reason_q, loom_ret_nodes_q, loom_ret_ticket_q,
             loom_ret_plan_q);
    if (geom_loom_feed_posts_o != 32'd1 || geom_loom_feed_streams_o != 32'd1 ||
        geom_loom_feed_nodes_o != 32'd1)
      $fatal(1, "SMOKE: the loom carrier consumed %0d post(s), delivered %0d stream(s) and handed over %0d node beat(s), expected 1/1/1",
             geom_loom_feed_posts_o, geom_loom_feed_streams_o, geom_loom_feed_nodes_o);
    // TWO bursts: the header plus the one node. THE NUMBER IS THE LAYOUT --
    // one record is one 64-byte burst, and a record that straddled a boundary
    // would show up here as four.
    if (geom_loom_feed_bursts_o != 32'd2)
      $fatal(1, "SMOKE: the loom carrier issued %0d 64-byte burst(s) for a header and one node, expected 2 -- the record is not one burst",
             geom_loom_feed_bursts_o);
    // The arbiter's own count of client 4 must agree with the carrier's. Two
    // counters on opposite sides of one handshake: if they disagree, a burst
    // was served to somebody else or counted twice.
    if (terr_hps_c4_bursts_o != geom_loom_feed_bursts_o)
      $fatal(1, "SMOKE: arbiter client 4 served %0d burst(s) while the carrier counted %0d -- the fifth client is not the carrier's",
             terr_hps_c4_bursts_o, geom_loom_feed_bursts_o);
    if ((geom_loom_feed_align_refused_o | geom_loom_feed_hdr_refused_o |
         geom_loom_feed_refused_o | geom_loom_feed_faulted_o |
         geom_loom_feed_replayed_o | geom_loom_feed_bridge_errs_o |
         geom_loom_feed_ret_overflow_o) != 32'd0)
      $fatal(1, "SMOKE: the loom carrier refused, faulted, replayed or overran on a stream this bench staged correctly -- see the line above");
    // THE MAILBOX WAS ANSWERED. Every post owes exactly one return; a stream
    // that composed without one would leave the ARM unable to recycle the
    // staging buffer, which is the failure the doorbell contract exists for.
    if (!loom_ret_seen_q || !loom_ret_ok_q || loom_ret_refused_q ||
        loom_ret_reason_q != 3'd0 || loom_ret_nodes_q != 16'd1 ||
        loom_ret_ticket_q != 32'h0000_5010 || loom_ret_plan_q != 32'h5100_0000)
      $fatal(1, "SMOKE: the loom carrier's return is not the post's answer -- seen=%0b ok=%0b refused=%0b reason=%0d nodes=%0d ticket=%08x plan=%08x",
             loom_ret_seen_q, loom_ret_ok_q, loom_ret_refused_q, loom_ret_reason_q,
             loom_ret_nodes_q, loom_ret_ticket_q, loom_ret_plan_q);
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
    // ---- PROJ.CFGVALID (entry I14's closing half) --------------------------
    // THE PROJECTOR'S ENABLE HAS A PRODUCER, AND THIS IS THE POSITIVE CONTROL
    // FOR IT. `proj_en_i` is held at 0 by this bench (see the note where it is
    // driven), so the only thing that can have enabled the shared projector is
    // `u_proj_cfgvalid` arming on `geom_write_camera()`'s sixteen matrix
    // words. The 2,560 raster pixels above are the end-to-end evidence; this
    // is the instrument saying the same thing from a structure that shares no
    // logic with the raster's counters.
    //
    // A ZERO HERE ON A CONSOLE THAT DREW PIXELS WOULD BE THE INTERESTING
    // READING, and it is why the check is `!= 1` rather than `== 0`: it would
    // mean the projector was enabled by something this bench does not know
    // about, which is exactly the fake-stimulus shape entry I14 was carrying.
    $display("SMOKE: projector arm=%0d held_offers=%0d",
             proj_cfg_armed_o, proj_en_held_offers_o);
    if (proj_cfg_armed_o != 32'd1)
      $fatal(1, "SMOKE: PROJ.CFGVALID armed %0d time(s), expected exactly 1 -- the projector's enable did not come from its producer",
             proj_cfg_armed_o);
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
    if (mat_rsp_seen_q < 1)
      $fatal(1, "SMOKE: MATERIAL.RESOLVE never answered -- entry I49's window issued no resolve at all");
    if (mat_not_resident_o != 0)
      $fatal(1, "SMOKE: the published MATERIAL_SET was NOT RESIDENT -- MEM.UPLOAD's publication did not reach the directory");
    // (The record-0 check that stood here until 2026-09-20 is GONE ON PURPOSE,
    //  and its removal is I49 closing rather than a check being dropped. It
    //  asserted the record THIS BENCH asked for; the window now issues the
    //  request from the triangle's own material id, so the right answer is
    //  record 1 and the check below is strictly stronger.)
    if (mat_fetch_denied_o != 32'd0 || mat_refused_o != 32'd0 || geom_ma_denied_o != 32'd0)
      $fatal(1, "SMOKE: MATERIAL.RESOLVE refused %0d, fetch_denied %0d, adapter_denied %0d -- R32 puts the upload where ENGINE1 reads and record 1 is a legal MaterialRecord",
             mat_refused_o, mat_fetch_denied_o, geom_ma_denied_o);
    // THE RECORD IS RECORD **1**, NOT RECORD 0, AND THAT IS THE POINT. The
    // request is no longer this bench's: `u_material_window` issued it from the
    // TRIANGLE's own material id, which the meshlet descriptor carries as 1.
    // A window that defaulted to 0 -- or that paired the draw's material_set
    // with some other meshlet's id -- would return record 0, and the two
    // records differ in every field the flat request carries.
    if (!mat_rec_has_q || mat_rec_q != upl_rec1)
      $fatal(1, "SMOKE: MATERIAL.RESOLVE's record (has=%b) %064x is not record 1 %064x -- the resolve did not ask for the TRIANGLE's material",
             mat_rec_has_q, mat_rec_q, upl_rec1);
    $display("SMOKE: matwin   resolves=%0d switches=%0d stall[drain/answer]=[%0d %0d] occ_max=%0d no_record=%0d sel_ovf=%0d clut_unowned=%0d err[unpub/underflow]=[%0d %0d]",
             mat_win_resolves_o, mat_win_switches_o, mat_win_drain_stall_o,
             mat_win_answer_stall_o, mat_win_occupancy_max_o, mat_win_no_record_o,
             mat_win_selector_overflow_o, mat_win_clut_unowned_o,
             mat_win_err_unpublished_o, mat_win_err_underflow_o);
    if (mat_win_err_unpublished_o != 32'd0 || mat_win_err_underflow_o != 32'd0)
      $fatal(1, "SMOKE: the material window's structural guards fired (unpublished %0d, underflow %0d) -- a triangle reached the door under a material nobody resolved",
             mat_win_err_unpublished_o, mat_win_err_underflow_o);
    if (mat_win_resolves_o != mat_win_switches_o)
      $fatal(1, "SMOKE: the material window issued %0d resolve(s) for %0d switch(es) -- a re-resolve of a material it already held is invisible in the picture and doubles the meshlet loop's cost",
             mat_win_resolves_o, mat_win_switches_o);
    // ---- THE BINDING PAGE AND THE FILL SOCKET (entry I49's second half) ----
    $display("SMOKE: binding  page_gen=%0d acks=%0d last_status=%0d fill[lines/beats]=[%0d %0d]",
             active_page_generation_o, tbind_acks_q, tbind_status_q,
             tfill_lines_q, tfill_beats_q);
    if (tbind_status_q != 4'd0)
      $fatal(1, "SMOKE: the binding page was refused with status %0d (2 BAD_GENERATION, 3 BAD_ROW, 5 BAD_CRC) after %0d ack(s)",
             tbind_status_q, tbind_acks_q);
    if (active_page_generation_o != TEX_PAGE_GEN_C)
      $fatal(1, "SMOKE: the binding page sealed but never ACTIVATED (generation %0d, want %0d) -- the bank swap waits on data quiet",
             active_page_generation_o, TEX_PAGE_GEN_C);
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

    // ---- FIELD.EARTH_ADAPTER (EARTHADAPT, 2026-09-23) --------------------
    // WHAT THIS SMOKE CAN AND CANNOT SAY ABOUT THE EARTH SEAM, stated so the
    // green is not quoted for more than it covers.
    //
    // It CAN say the adapter is inert on a console that issues no
    // TerrainField: the bench's packet is BeginFrame / PublishResource /
    // EndFrame, so `zhao_cmd_exec` stages no TerrainField record, the frame
    // list seals empty, `fields_active_o` is 0 and the consumer never raises
    // its field lane. `fld_earth_records_o == 0` is that, asserted rather than
    // assumed -- and it replaces the `terr_pt_fld_valid_i = '0` this bench used
    // to drive, which said the same thing about a wire instead of about the
    // machine.
    //
    // It CANNOT say the adapter WORKS. The console smoke fails every terrain
    // page's CRC, so terrain's composed door never opens and no vertex ever
    // reaches `zhao_terrain_patch`'s compose lane. Every counter past that door
    // is unreachable from here BY CONSTRUCTION, and a zero on one of them is
    // evidence about the stimulus and not about the block. The evidence that
    // they discriminate is `tests/field/field_earth_adapter_directed.cpp`,
    // which fires each of them on purpose (R95).
    if (fld_earth_records_o != 32'd0)
      $fatal(1, "SMOKE: FIELD.EARTH_ADAPTER banked %0d TerrainField uniform record(s) -- this bench issues none, so either the packet changed or the two-consumer join is taking records the field list is not",
             fld_earth_records_o);
    // R168's arm on this record, NEW 2026-09-23 (PATCHV2). It is in the SECOND
    // category the paragraph above sets out and not the first: no Earth run
    // retires on this bench at all, so a short record is unreachable here BY
    // CONSTRUCTION and this zero is evidence about the stimulus. It is asserted
    // anyway, because the counter is new and a new counter that nothing reads
    // is how a wiring mistake survives -- a nonzero here would mean a run
    // retired on a console that issues no TerrainField, which is a louder
    // failure than the number itself. The evidence that it DISCRIMINATES is
    // `field_earth_adapter_directed` case 14, which fires it on purpose (R95).
    if (fld_earth_short_record_o != 32'd0)
      $fatal(1, "SMOKE: FIELD.EARTH_ADAPTER counted %0d short Earth record(s) -- this bench retires no Earth run at all, so this counter cannot legitimately have moved",
             fld_earth_short_record_o);
    // The shadow guard, which IS reachable here and is not two zeros agreeing.
    // `lane_desync_o` differences `vtx_live` against the consumer's own
    // `fld_ready_o` on EVERY clock of the run, not only inside a patch -- so an
    // adapter whose vertex state were inverted, or whose `ans_ready_i` were
    // wired to the wrong block's ready, would move it on the first cycle after
    // reset with no terrain traffic whatever. That is what makes this check
    // worth its line in a smoke that cannot open terrain's door.
    if (fld_earth_lane_desync_o != 32'd0)
      $fatal(1, "SMOKE: FIELD.EARTH_ADAPTER lane shadow desynchronised %0d time(s) against TERRAIN.PATCH's own busy, with no terrain traffic at all",
             fld_earth_lane_desync_o);

    // ---- GEOM.BINNER's INSTRUMENTS, ASSERTED (GIANTREFS) ------------------
    // Entry I56 item (5). These are not a $display: a connected counter nobody
    // reads is the broken-instrument law with a port list, and that is exactly
    // what `render_overflow_o` was.
    //
    // The values are the measured truth of THIS bench's fixture -- 14
    // triangles in 1 admitted frame produce 36 tile references with a deepest
    // single tile list of 5. To re-derive after a fixture change, read
    // `SMOKE: binrefs` from a run and pin it here; do not relax the check to a
    // range, because a range cannot see the machine doing several times the
    // work for the same picture.
    if (cnt_beats_seen_q == 0)
      $fatal(1, "SMOKE: DEBUG.COUNTERS never streamed a beat -- the read window is shut, so every console counter is unobservable");
    if (!cnt_tile_refs_seen_q)
      $fatal(1, "SMOKE: catalog id 18 (tile_references) never appeared in the counter sweep -- GEOM.BINNER's instrument does not reach the console's mailbox");
    if (!cnt_tile_depth_seen_q)
      $fatal(1, "SMOKE: catalog id 19 (max_tile_list_depth) never appeared in the counter sweep");
    if (cnt_tile_refs_q != 64'd36)
      $fatal(1, "SMOKE: tile_references=%0d, expected 36 from this fixture's 14 triangles", cnt_tile_refs_q);
    if (cnt_tile_depth_q != 64'd5)
      $fatal(1, "SMOKE: max_tile_list_depth=%0d, expected 5", cnt_tile_depth_q);
    // TWO QUANTITIES THAT DO NOT MOVE TOGETHER. `tile_references` is a
    // frame-wide push count; `max_tile_list_depth` is a per-tile peak taken
    // from a different register on a different condition. A frame's total
    // cannot be smaller than its deepest single list, so this catches a swap
    // or a shared shadow that equal-value checks cannot.
    if (cnt_tile_refs_q < cnt_tile_depth_q)
      $fatal(1, "SMOKE: tile_references (%0d) is below max_tile_list_depth (%0d) -- the two shadows are crossed", cnt_tile_refs_q, cnt_tile_depth_q);
    // THE WALL IS SILENT, AND NOW THAT MEANS SOMETHING. The composed binner
    // holds 32,768 tile references (R7's giant, RENDER_CHUNKS=8192 x
    // RENDER_CHUNK_REFS=4), so a zero here is a frame that fitted rather than a
    // counter nobody wired. The positive control that the same counter still
    // FIRES is a separate build and lives in
    // `tests/geometry/geom_binner_v2_cntw_wrap.cpp` phase 2, which fills the
    // triangle store on purpose and requires overflow_o to rise.
    if (render_overflow_o !== 1'b0)
      $fatal(1, "SMOKE: the binner walled off this frame -- render_overflow_o is high");

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
