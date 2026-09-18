// zhao_console_core.sv -- THE CONNECTED MACHINE.
//
// Not a census. `zhao_prod_top` is the census and stays one: it puts blocks
// side by side under independent LFSR stimulus and is labelled
// RESOURCE_CENSUS_DISCONNECTED for exactly that reason. This file is the other
// thing the completion plan asks for --
//
//   reports/Zhaozhou_True_Console_Completion_Plan_2026-09-18.txt S1.3:
//     "zhao_console_core: the connected machine, with platform transaction
//      interfaces."
//
// and S13.3's warning is the reason it had to be a NEW file rather than a
// re-labelled old one: "a resource-top selection change alone does not close
// P2."
//
// ---------------------------------------------------------------------------
// WHAT THIS FILE IS, IN ONE SENTENCE
// ---------------------------------------------------------------------------
// `zhao_shell_top_v2` -- which already wires a real console (CMD front end,
// MEM, VIDEO, INPUT, AUDIO, DEBUG, and the geometry/raster front door) -- with
// the organs that were BUILT AND ADOPTED NOWHERE joined to it and to each
// other: the four particle blocks as one ring, the geometry group sequencer
// closed onto the shared projector it was written to drive, the compositor and
// the measurement histogram on the shell's real frame boundary.
//
// It does NOT rewrite the shell. Plan S14.4 priority 8: "keep existing hardware
// that already meets its contract rather than rewrite everything to a new
// generic framework." Every one of the shell's 217 ports is carried straight
// through, declaration and comment verbatim, so the sibling shell remains the
// authority on its own seams and this file adds only what it composes.
//
// SEEDED ONCE, THEN HAND-MAINTAINED, which is the same standing
// `zhao_shell_top_v2.sv` has and for the same reason: 217 port names carried
// across mechanically cannot be retyped without a swapped name, and a generator
// kept alive afterwards would be the stale-generated-file trap. Nothing
// regenerates this file. If the shell's port list changes, the compiler says
// so -- `--top-module zhao_console_core` reports PINMISSING, which is a gate,
// not a guess.
//
// ---------------------------------------------------------------------------
// WHAT IS GENUINELY CONNECTED HERE -- producer -> consumer, no stimulus
// ---------------------------------------------------------------------------
// The plan's standard is quoted in full because it is the whole point:
//
//   "The simulation harness supplies external clocks, input events, memory
//    behavior and host packets. It does NOT supply missing lighting, FIELD
//    results, particle updates, prepared triangles or fake material records."
//
// So nothing below is an LFSR, a constant pattern or a counter pretending to be
// a workload. These are real wires between real blocks:
//
//   1. THE PARTICLE RING (four blocks, one closed loop)
//        PART.STATE.prt      -> PART.UPDATE.in
//        PART.UPDATE.out     -> [fork] -> PART.COLLIDE.p
//                                      -> PART.SPAWN.par  (+ its events)
//        PART.COLLIDE.c      -> PART.STATE.vrd   (record AND survive verdict)
//        PART.SPAWN.chl      -> PART.STATE.chl
//      The fork is real glue and is documented at its declaration; it is the
//      only arithmetic-free thing standing between two real ports.
//
//   2. THE FRAME BOUNDARY IS THE PARTICLE TICK
//        SHELL.gpu_tick_o           -> PART.STATE.tick_start_i
//                                   -> PART.SPAWN.tick_start_i
//        SHELL.gpu_tick_frame_id_o  -> PART.SPAWN.tick_i
//      The particle generation advances on the console's real frame edge, not
//      on a testbench pulse.
//
//   3. THE GEOMETRY CLIENT, CLOSED ONTO ITS SERVICE
//        GEOM.SKIN.o          -> GEOM.GROUP_SEQ.v
//        GEOM.GROUP_SEQ.a     -> PROJ_SUBSYSTEM client A
//        PROJ_SUBSYSTEM.a_*_o -> GEOM.PROJ_LANE (the arena fill)
//        GEOM.PROJ_LANE       -> GEOM.GROUP_SEQ  (open_gen, rider_payload,
//                                fill_landed/fill_arena -- the seal evidence)
//      `design/prod_manifest.yml` records why the shared projector could not be
//      selected: "the producer is still absent, so this is not yet adoptable on
//      its own". `zhao_geom_group_seq` IS that producer and this is the first
//      composition in which it drives the thing it was written for.
//
//   4. THE COMPOSITOR ON THE REAL VIDEO MODE
//        SHELL.gpu_tick_o -> POST.COMPOSITE.frame_start_i
//        SHELL.mode_act_o -> the pass width/height (Z60 384x240, Storm 320x240,
//                            Duo 256x192 per VIEW -- zhao_pkg's own numbers)
//
//   5. THE MEASUREMENT INTERVAL IS THE FRAME
//        SHELL.gpu_tick_o -> MEASURE.HISTOGRAM.snapshot_i
//
// ---------------------------------------------------------------------------
// INCOMPLETE -- TIED OFF, AND WHY
// ---------------------------------------------------------------------------
// A hidden tie-off is the failure this file exists to stop, so every one of
// them is here, by name, with the owner that is missing. "Boundary" means the
// signal leaves this module as a port: the harness or the board top drives it,
// and it is NOT constant-folded away -- which matters, because a constant on a
// wide data input deletes the logic behind it and produces a resource number
// that is confidently too small.
//
//  I1. PART.STATE's generation store (`part_rd_*`, `part_wr_*`) -- BOUNDARY.
//      The plan lets the harness supply "memory behavior", and this is that.
//      But the real provider is named nowhere: MEM.HPS.BRIDGE is instantiated
//      inside the shell and has no particle client port, so no route from this
//      store to that bridge exists yet. Connecting them is a shell change.
//
//  I2. THE SPECIES DESCRIPTOR TABLE (`part_upd_spc_*`, `part_spw_spc_*`,
//      `part_col_d_*`) -- BOUNDARY. NO OWNER EXISTS IN THE TREE.
//      PART.COLLIDE's contract says "this block owns no memory"; PART.UPDATE's
//      says the table "belongs to PART.STATE"; and `zhao_part_state.sv` has no
//      descriptor port and no such memory. All three blocks read a table that
//      nothing implements. A PART.TABLE owner must be built; when it is, these
//      ports become internal and this entry is deleted.
//
//  I3. THE SIZE/COLOUR CURVE TABLE (`part_crv_*`) -- BOUNDARY, same gap as I2.
//
//  I4. PART.UPDATE STEP 6, THE COLLISION RESPONSE (`col_valid_i`, `col_vx_i`,
//      `col_vy_i`, `col_vz_i`) -- TIED TO ZERO, and this one is a real
//      INTERFACE MISMATCH rather than a missing block.
//      PART.UPDATE expects a resolved velocity triple. PART.COLLIDE applies
//      the response to the RECORD and emits `c_record_o`; it has no velocity
//      output at all. The two step-6 contracts do not meet, so there is
//      nothing legitimate to wire and an adapter here would be arithmetic
//      invented in the composer -- exactly what this file must not contain.
//      TWO CONSEQUENCES, STATED SO NOBODY QUOTES THEM AS EVIDENCE LATER:
//        (a) `part_collisions_applied_o` is STRUCTURALLY STUCK AT ZERO in this
//            core. It is not a working counter reading zero; it is a counter
//            that cannot move. Do not read it as "no collisions occurred".
//        (b) synthesis will constant-fold PART.UPDATE's step-6 datapath, so
//            this core UNDER-COUNTS PART.UPDATE's area. The resource number is
//            a floor for that block, not its cost.
//
//  I5. PART.UPDATE's field sample (`part_fld_*`) -- BOUNDARY. FIELD.SEQ.FLOW
//      is not composed; `zhao_field_seq.sv` exists but exposes no bounded
//      acceleration sample of this shape.
//
//  I6. PART.COLLIDE's terrain sample (`part_ter_*`) -- BOUNDARY.
//      `zhao_terrain_patch.sv` exists and is the named owner, but it emits
//      heights (top/bottom/compose_top) and NO SURFACE NORMAL, and the
//      collision test needs {height, nx, ny, nz}. Wiring height alone and
//      inventing a normal would be the hidden-adapter failure.
//
//  I7. PART.COLLIDE's plane (`part_plane_*`) -- BOUNDARY. A per-frame owner
//      value by the block's own design; CMD.SCHEDULER has no path to it.
//
//  I8. PART.SPAWN's capacity backstop (`part_cap_full_i`) -- BOUNDARY.
//      PART.STATE knows when the generation is full and exposes no such
//      output; it only counts `children_dropped_capacity_o` after the fact.
//      A one-bit addition to PART.STATE would close this properly.
//
//  I9. PART.SPAWN's parent id (`par_id_i`) -- NOT a tie-off: the core assigns
//      it. The particle128 record (amendment C2) carries no id field, so the
//      only identity available is the particle's ORDINAL within the
//      generation, and this file counts it. Listed here because it is a
//      decision taken in the composer: if ids must survive compaction, a real
//      PART.ID owner is needed and this counter is wrong.
//
// I10. GEOM.SKIN's vertex and bone-matrix inputs (`geom_skin_v_*`,
//      `geom_skin_a_m_i`, `geom_skin_b_m_i`) -- BOUNDARY. GEOM.POSE owns the
//      matrix palette and GEOM.VDECODE the vertex stream; neither is composed
//      in this tree (the shell's own source list records that its geometry
//      front end elaborated nowhere and was struck).
//
// I11. GEOM.GROUP_SEQ's job port and its sealed-group output (`geom_job_*`,
//      `geom_grp_*`, `geom_rel_*`) -- BOUNDARY. The replay customer is
//      GEOM.SETUP, and the shell "starts at the binner and has no vertex front
//      end at all" (its own source list says so).
//
// I12. GEOM.PROJ_LANE's lookup/reply and arena origin (`geom_look_*`,
//      `geom_rep_*`, `geom_org_*`) -- BOUNDARY, same absent customer as I11.
//
// I13. PROJ_SUBSYSTEM's CLIENT B, its reference port and its triangle output
//      (`proj_b_*`, `proj_ref_*`, `proj_out_*`) -- BOUNDARY.
//      `fpga/rtl/terrain/zhao_terrain_group_seq.sv` EXISTS and is the real
//      producer for client B; composing it is the obvious next packet and was
//      out of this one's scope. Client B being on pins means the shared
//      projector is measured here with ONE of its two clients live.
//
// I14. PROJ_SUBSYSTEM's matrix bank (`proj_cfg_*`, `proj_en_i`) -- BOUNDARY.
//      The camera matrices are host/CMD state and CMD.SCHEDULER has no
//      projection-config path.
//
// I15. POST.COMPOSITE's SOURCE PIXELS (`post_s_*`) -- BOUNDARY, and this is
//      the largest honest gap in the file. RASTER.RESOLVE's output is INTERNAL
//      to `zhao_geom_bin_pipe_v2`, and inside the shell RASTER.FBWRITE is
//      already fed from it directly. The compositor therefore sits BESIDE the
//      render path, not in it. Interposing it is a change to the shell's
//      render chain and to `zhao_geom_bin_pipe_v2`'s port list; it is not
//      wiring and it was not attempted here.
//
// I16. POST.COMPOSITE's output and echo tap (`post_o_*`, `post_echo_*`) --
//      BOUNDARY, the other end of I15.
//
// I17. POST.COMPOSITE's gather planes, atmosphere sheet, HUD, grading table,
//      flash and ink (`post_gd_*`, `post_gg_*`, `post_atm_*`, `post_hud_*`,
//      `post_pv_*`, `post_bias_*`, `post_flash_*`, `post_ink_*`,
//      `post_bloom_gain_i`) -- BOUNDARY. TWOD.PLANE and the HUD source are not
//      built. The grading curves are generated ASSETS by design, so their load
//      port is legitimately external; the plane ports are not, and are a gap.
//
// I18. MEASURE.HISTOGRAM's event ingress (`hist_ev_*`) -- BOUNDARY. Nothing in
//      the console produces an error-magnitude stream; the block measures a
//      difference against a reference and the console has no reference. Its
//      INTERVAL is real (I5 of the connected list), its EVENTS are not.
//
// I19. MEASURE.HISTOGRAM's host read window (`hist_rd_*`) -- BOUNDARY. The
//      host is the HPS; no register path from HPS to this block exists.
//
// I20. Everything `zhao_shell_top_v2` already declares provisional at its own
//      edge -- the triangle port, `fb_writer_i`, the FRAME_RING view, the
//      geometry memory clients -- is UNCHANGED and still provisional. This
//      file adds no opinion about them; read that file's header.
//
// ---------------------------------------------------------------------------
// LIGHTING SEAM -- DELIBERATELY NOT CONNECTED
// ---------------------------------------------------------------------------
// `zhao_geom_light.sv` and every `zhao_light_*` file are being refactored into
// a lighting service while this file is written, so they are EXCLUDED from this
// composition on purpose and none of them appears in its source closure. This
// core therefore contains ZERO lighting logic and its resource number contains
// none either.
//
// `light_seam_connected_o` is that statement made machine-readable: it is tied
// low and a hierarchy census can see it. WHAT MUST BE CONNECTED HERE LATER:
//
//   * the lighting service's VERTEX/NORMAL input, from GEOM.SKIN -- the
//     skinned-normal sibling `zhao_geom_skin_norm.sv` is its producer, and it
//     is deliberately NOT instantiated here for the same exclusion reason;
//   * the service's per-light parameter load, from the same host/CMD path that
//     I14 describes for the projection matrices;
//   * the service's RGB term OUT, into the raster material stage inside
//     `zhao_geom_bin_pipe_v2` -- which is a shell-side change, so the seam is
//     not purely additive and should be planned with I15;
//   * the tie-low below becomes a real driven level once all three exist.
//
// Nothing here should be read as "lighting fits in the remaining area". It has
// not been measured in this core at all.
//
// ---------------------------------------------------------------------------
// WHAT THIS CORE IS NOT
// ---------------------------------------------------------------------------
// A lint-clean Verilator run is NOT synthesizability. This file obeys the two
// Quartus 17.0 forms this repository has been bitten by -- `$fatal` only inside
// `initial begin ... end`, explicit `generate`/`endgenerate` -- but a block
// that has never been through `quartus_map` has not been shown to synthesize,
// however clean its lint, and at the time of writing this one had not.
//
// It is also not `zhao_console_board`: there is no PLL, no physical pin
// assignment and no board framework here. Plan S1.3 keeps those separate.
//
// Conservative SystemVerilog subset (charter S2).

module zhao_console_core
  import zhao_pkg::*, zhao_abi_pkg::*, zhao_fb_tuple_pkg::*;
#(
  // ---- the shell's own knobs, carried through ------------------------------
  parameter int unsigned FRAMER_Q = 8,
  parameter int unsigned WFIFO_W  = 64,

  // ---- PARTICLES -----------------------------------------------------------
  parameter int unsigned PART_REC_W    = 128,     // particle128, amendment C2
  parameter int unsigned PART_CAPACITY = 32768,   // the required tier
  parameter int unsigned PART_CHILD_D  = 64,
  parameter int unsigned PART_SPECIES_N= 128,
  parameter int unsigned PART_AGE_W    = 10,
  parameter int unsigned PART_POS_W    = 18,
  parameter int unsigned PART_VEL_W    = 11,
  parameter int unsigned PART_NRM_W    = 12,
  parameter int unsigned PART_FX_W     = 16,
  parameter int unsigned PART_PID_W    = 16,
  parameter int unsigned PART_TICK_W   = 32,

  // ---- GEOMETRY: the client-A side of the shared projector -----------------
  parameter int unsigned GEOM_ARENAS   = 4,
  parameter int unsigned GEOM_DEPTH    = 1089,
  parameter int unsigned GEOM_NVIEWS   = 2,
  parameter int unsigned GEOM_GEN_W    = 8,
  parameter int unsigned GEOM_PAY_A_W  = 16,
  parameter int unsigned GEOM_PAYLOAD_W= 106,
  parameter int unsigned GEOM_INDEX_W  = $clog2(GEOM_DEPTH) + 1,
  parameter int unsigned GEOM_ARENA_W  = $clog2(GEOM_ARENAS) + 1,
  parameter int unsigned GEOM_MUL_LANES= 3,

  // ---- GEOMETRY: the client-B/terrain side of the same projector ----------
  parameter int unsigned PROJ_T_ARENAS = 4,
  parameter int unsigned PROJ_T_DEPTH  = 81,
  parameter int unsigned PROJ_T_INDEX_W= $clog2(PROJ_T_DEPTH) + 1,
  parameter int unsigned PROJ_T_ARENA_W= $clog2(PROJ_T_ARENAS) + 1,

  // ---- COMPOSITOR ----------------------------------------------------------
  parameter int unsigned POST_LINE_W   = 384,     // Z60 is the widest view
  parameter int unsigned POST_MAX_H    = 240,
  parameter int unsigned POST_NLINE    = 9,
  parameter int unsigned POST_LAG_LINES= 4,
  parameter int unsigned POST_LAG_PX   = 9,
  parameter int unsigned POST_XW       = $clog2(POST_LINE_W + 1),
  parameter int unsigned POST_YW       = $clog2(POST_MAX_H + 1),

  // ---- MEASURE -------------------------------------------------------------
  parameter int unsigned HIST_EW       = 32,
  parameter int unsigned HIST_SUB_BITS = 1,
  parameter int unsigned HIST_LANES    = 4,
  parameter int unsigned HIST_CW       = 24,
  parameter int unsigned HIST_BINW     = $clog2((HIST_EW - HIST_SUB_BITS + 1) << HIST_SUB_BITS)
) (
  // ==========================================================================
  // THE ADOPTED ORGANS' OWN BOUNDARY.
  // Every port in this section is listed in the header's
  // "INCOMPLETE -- TIED OFF, AND WHY" table with the owner that is missing.
  // ==========================================================================

  // ---- I1: PART.STATE's generation store (harness = memory) ---------------
  input  logic                    part_rd_valid_i,
  output logic                    part_rd_ready_o,
  input  logic [PART_REC_W-1:0]   part_rd_record_i,
  input  logic                    part_rd_last_i,
  output logic                    part_wr_valid_o,
  input  logic                    part_wr_ready_i,
  output logic [PART_REC_W-1:0]   part_wr_record_o,

  // ---- I2: PART.UPDATE's species descriptor (NO OWNER EXISTS) -------------
  output logic [6:0]              part_upd_spc_index_o,
  input  logic [3:0]              part_upd_spc_recipe_i,
  input  logic [PART_AGE_W-1:0]   part_upd_spc_lifetime_i,
  input  logic [PART_AGE_W-1:0]   part_upd_spc_age_mark_i,
  input  logic [7:0]              part_upd_spc_drag_i,
  input  logic signed [10:0]      part_upd_spc_grav_i,
  input  logic signed [10:0]      part_upd_spc_strength_i,
  input  logic signed [17:0]      part_upd_spc_cx_i,
  input  logic signed [17:0]      part_upd_spc_cy_i,
  input  logic signed [17:0]      part_upd_spc_cz_i,
  input  logic signed [10:0]      part_upd_spc_p0_i,
  input  logic signed [10:0]      part_upd_spc_p1_i,
  input  logic signed [10:0]      part_upd_spc_p2_i,

  // ---- I3: the size/colour curve table (NO OWNER EXISTS) ------------------
  output logic [3:0]              part_crv_index_o,
  input  logic [5:0]              part_crv_size_i,
  input  logic [7:0]              part_crv_colour_i,

  // ---- I5: the bounded FIELD/FLOW acceleration sample ---------------------
  input  logic                    part_fld_valid_i,
  input  logic signed [10:0]      part_fld_ax_i,
  input  logic signed [10:0]      part_fld_ay_i,
  input  logic signed [10:0]      part_fld_az_i,

  // ---- I2: PART.COLLIDE's slice of the same missing descriptor ------------
  input  logic [2:0]              part_col_d_response_i,
  input  logic signed [PART_FX_W-1:0] part_col_d_restitution_i,
  input  logic signed [PART_FX_W-1:0] part_col_d_friction_i,
  input  logic signed [PART_FX_W-1:0] part_col_d_damping_i,

  // ---- I6: the live deformed terrain sample -------------------------------
  input  logic                    part_ter_valid_i,
  input  logic signed [PART_POS_W-1:0] part_ter_height_i,
  input  logic signed [PART_NRM_W-1:0] part_ter_nx_i,
  input  logic signed [PART_NRM_W-1:0] part_ter_ny_i,
  input  logic signed [PART_NRM_W-1:0] part_ter_nz_i,

  // ---- I7: the one plane --------------------------------------------------
  input  logic                    part_plane_en_i,
  input  logic signed [PART_NRM_W-1:0] part_plane_nx_i,
  input  logic signed [PART_NRM_W-1:0] part_plane_ny_i,
  input  logic signed [PART_NRM_W-1:0] part_plane_nz_i,
  input  logic signed [31:0]      part_plane_c_i,

  // ---- I2: PART.SPAWN's slice of the same missing descriptor --------------
  output logic [6:0]              part_spw_spc_species_o,
  output logic [1:0]              part_spw_spc_event_o,
  input  logic                    part_spw_spc_known_i,
  input  logic [6:0]              part_spw_spc_child_spc_i,
  input  logic [4:0]              part_spw_spc_count_i,

  // ---- I8: the capacity backstop ------------------------------------------
  input  logic                    part_cap_full_i,

  // ---- PARTICLE evidence (every counter leaves the module) ----------------
  output logic                    part_tick_busy_o,
  output logic                    part_tick_done_o,
  output logic [31:0]             part_survivors_o,
  output logic [31:0]             part_children_written_o,
  output logic [31:0]             part_children_dropped_capacity_o,
  output logic [31:0]             part_staging_stall_cycles_o,
  output logic [31:0]             part_species_refused_o,
  output logic [31:0]             part_updated_o,
  output logic [31:0]             part_upd_refused_o,
  output logic [31:0]             part_died_by_age_o,
  output logic                    part_upd_beat_refused_o,
  output logic [31:0]             part_velocity_saturations_o,
  output logic [31:0]             part_position_saturations_o,
  // I4: STRUCTURALLY STUCK AT ZERO. Not a working counter reading zero.
  output logic [31:0]             part_collisions_applied_o,
  input  logic [3:0]              part_hist_sel_i,
  output logic [31:0]             part_hist_val_o,
  output logic                    part_colour_en_o,
  output logic [7:0]              part_colour_o,
  output logic                    part_contact_o,
  output logic [2:0]              part_response_o,
  output logic                    part_col_refused_o,
  output logic [31:0]             part_contacts_ignore_o,
  output logic [31:0]             part_contacts_die_o,
  output logic [31:0]             part_contacts_stick_o,
  output logic [31:0]             part_contacts_slide_o,
  output logic [31:0]             part_contacts_bounce_o,
  output logic [31:0]             part_contacts_terrain_o,
  output logic [31:0]             part_contacts_plane_o,
  output logic [31:0]             part_already_inside_at_entry_o,
  output logic [31:0]             part_terrain_sample_unavailable_o,
  output logic [31:0]             part_response_refused_o,
  output logic [31:0]             part_field_clamps_o,
  output logic [31:0]             part_children_requested_o,
  output logic [31:0]             part_children_emitted_o,
  output logic [31:0]             part_children_refused_o,
  output logic [31:0]             part_spawn_by_event0_o,
  output logic [31:0]             part_spawn_by_event1_o,
  output logic [31:0]             part_spawn_by_event2_o,
  output logic [31:0]             part_spawn_by_event3_o,
  output logic [31:0]             part_refused_count_gt_max_o,
  output logic [31:0]             part_refused_unknown_species_o,
  output logic [31:0]             part_refused_capacity_o,
  output logic [31:0]             part_max_children_in_tick_o,

  // ---- I10: GEOM.SKIN's vertex and bone matrices --------------------------
  input  logic                    geom_skin_v_valid_i,
  output logic                    geom_skin_v_ready_o,
  input  logic signed [31:0]      geom_skin_v_x_i,
  input  logic signed [31:0]      geom_skin_v_y_i,
  input  logic signed [31:0]      geom_skin_v_z_i,
  input  logic [6:0]              geom_skin_v_w0_i,
  input  logic                    geom_skin_v_rigid_i,
  input  logic [15:0]             geom_skin_v_src_id_i,
  input  logic signed [31:0]      geom_skin_a_m_i [0:11],
  input  logic signed [31:0]      geom_skin_b_m_i [0:11],
  output logic [15:0]             geom_skin_src_id_o,
  output logic [31:0]             geom_skin_vertices_transformed_o,

  // ---- I11: GEOM.GROUP_SEQ's job in, sealed group out ---------------------
  input  logic                    geom_job_valid_i,
  output logic                    geom_job_ready_o,
  input  logic [GEOM_INDEX_W-1:0] geom_job_count_i,
  input  logic [GEOM_NVIEWS-1:0]  geom_job_view_mask_i,
  input  logic [15:0]             geom_job_src_id_i,
  output logic                    geom_grp_valid_o,
  input  logic                    geom_grp_ready_i,
  output logic [GEOM_ARENA_W-1:0] geom_grp_arena_o,
  output logic [GEOM_GEN_W-1:0]   geom_grp_gen_o,
  output logic [GEOM_INDEX_W-1:0] geom_grp_count_o,
  output logic                    geom_grp_view_o,
  output logic [15:0]             geom_grp_src_id_o,
  input  logic                    geom_rel_valid_i,
  input  logic [GEOM_ARENA_W-1:0] geom_rel_arena_i,

  // ---- I12: GEOM.PROJ_LANE's arena origin and lookup port -----------------
  input  logic                    geom_org_we_i,
  input  logic [GEOM_ARENA_W-1:0] geom_org_arena_i,
  input  logic signed [31:0]      geom_org_x_i,
  input  logic signed [31:0]      geom_org_y_i,
  input  logic signed [31:0]      geom_org_z_i,
  input  logic                    geom_look_valid_i,
  output logic                    geom_look_ready_o,
  input  logic [GEOM_ARENA_W-1:0] geom_look_arena_i,
  input  logic [GEOM_GEN_W-1:0]   geom_look_gen_i,
  input  logic [GEOM_INDEX_W-1:0] geom_look_index_i,
  output logic                    geom_rep_valid_o,
  output logic                    geom_rep_hit_o,
  output logic                    geom_rep_refuse_o,
  output logic [GEOM_PAYLOAD_W-1:0] geom_rep_payload_o,
  output logic signed [31:0]      geom_rep_org_x_o,
  output logic signed [31:0]      geom_rep_org_y_o,
  output logic signed [31:0]      geom_rep_org_z_o,

  // ---- GEOMETRY evidence ---------------------------------------------------
  output logic [31:0]             geom_groups_opened_o,
  output logic [31:0]             geom_groups_sealed_o,
  output logic [31:0]             geom_vertices_sent_o,
  output logic [31:0]             geom_landings_o,
  output logic [31:0]             geom_jobs_refused_o,
  output logic [31:0]             geom_alloc_stall_cycles_o,
  output logic [31:0]             geom_rel_unheld_o,
  output logic                    geom_seal_early_o,
  output logic [31:0]             geom_arena_hits_o,
  output logic [31:0]             geom_arena_misses_o,
  output logic [31:0]             geom_arena_refusals_o,
  output logic                    geom_arena_overflow_o,

  // ---- I14: the shared projector's matrix bank ----------------------------
  input  logic                    proj_cfg_we_i,
  input  logic                    proj_cfg_view_i,
  input  logic [4:0]              proj_cfg_addr_i,
  input  logic [31:0]             proj_cfg_data_i,
  input  logic                    proj_en_i,

  // ---- I13: the projector's CLIENT B (terrain), reference and output ------
  input  logic                    proj_b_valid_i,
  output logic                    proj_b_ready_o,
  input  logic signed [31:0]      proj_b_vx_i,
  input  logic signed [31:0]      proj_b_vy_i,
  input  logic signed [31:0]      proj_b_vz_i,
  input  logic                    proj_b_view_i,
  input  logic [PROJ_T_ARENA_W-1:0] proj_b_arena_i,
  input  logic [PROJ_T_INDEX_W-1:0] proj_b_index_i,
  output logic                    proj_fill_landed_o,
  output logic [PROJ_T_ARENA_W-1:0] proj_fill_arena_o,
  input  logic                    proj_open_i,
  input  logic [PROJ_T_ARENA_W-1:0] proj_open_arena_i,
  output logic [GEOM_GEN_W-1:0]   proj_open_gen_o,
  input  logic                    proj_seal_i,
  input  logic [PROJ_T_ARENA_W-1:0] proj_seal_arena_i,
  input  logic                    proj_ref_valid_i,
  output logic                    proj_ref_ready_o,
  input  logic [PROJ_T_ARENA_W-1:0] proj_ref_arena_i,
  input  logic [GEOM_GEN_W-1:0]   proj_ref_gen_i,
  input  logic [PROJ_T_INDEX_W-1:0] proj_ref_ia_i,
  input  logic [PROJ_T_INDEX_W-1:0] proj_ref_ib_i,
  input  logic [PROJ_T_INDEX_W-1:0] proj_ref_ic_i,
  input  logic [15:0]             proj_ref_src_id_i,
  input  logic                    proj_ref_view_i,
  input  logic [7:0]              proj_ref_mat_a_i,
  input  logic [7:0]              proj_ref_mat_b_i,
  input  logic [7:0]              proj_ref_weight_i,
  output logic                    proj_out_valid_o,
  input  logic                    proj_out_ready_i,
  output logic signed [20:0]      proj_out_ax_o,
  output logic signed [20:0]      proj_out_ay_o,
  output logic signed [20:0]      proj_out_bx_o,
  output logic signed [20:0]      proj_out_by_o,
  output logic signed [20:0]      proj_out_cx_o,
  output logic signed [20:0]      proj_out_cy_o,
  output logic [2:0]              proj_out_behind_o,
  output logic [15:0]             proj_out_src_id_o,
  output logic signed [31:0]      proj_out_ad_o,
  output logic signed [31:0]      proj_out_bd_o,
  output logic signed [31:0]      proj_out_cd_o,
  output logic [30:0]             proj_out_aw_o,
  output logic [30:0]             proj_out_bw_o,
  output logic [30:0]             proj_out_cw_o,
  output logic                    proj_out_view_o,
  output logic [7:0]              proj_out_mat_a_o,
  output logic [7:0]              proj_out_mat_b_o,
  output logic [7:0]              proj_out_weight_o,
  output logic                    proj_out_refused_o,
  output logic                    proj_out_missed_o,

  // ---- PROJECTOR evidence --------------------------------------------------
  // `proj_a_view_o` is client A's result VIEW tag. GEOM.PROJ_LANE takes no
  // view (a group holds one view's results), so it has no consumer inside this
  // core and leaves the module named rather than left dangling.
  output logic                    proj_a_view_o,
  output logic [31:0]             proj_replay_triangles_o,
  output logic [31:0]             proj_replay_refused_o,
  output logic [31:0]             proj_replay_missed_o,
  output logic [31:0]             proj_corner_hits_o,
  output logic [31:0]             proj_corner_refusals_o,
  output logic [31:0]             proj_corner_misses_o,
  output logic                    proj_arena_overflow_o,
  output logic                    proj_arena_seal_short_o,
  output logic                    proj_shell_idle_o,
  output logic                    proj_svc_busy_o,
  output logic [31:0]             proj_a_grants_o,
  output logic [31:0]             proj_b_grants_o,
  output logic [31:0]             proj_contended_o,
  output logic [31:0]             proj_mat_refused_o,

  // ---- I15/I16/I17: the compositor's absent neighbours --------------------
  input  logic                    post_view_sel_i,
  input  logic                    post_s_valid_i,
  output logic                    post_s_ready_o,
  input  logic [15:0]             post_s_rgb_i,
  output logic                    post_gd_req_v_o,
  output logic                    post_gd_view_o,
  output logic [POST_XW-3:0]      post_gd_cx_o,
  output logic [POST_YW-3:0]      post_gd_cy_o,
  input  logic                    post_gd_present_i,
  input  logic signed [7:0]       post_gd_dx_i,
  input  logic signed [7:0]       post_gd_dy_i,
  output logic                    post_gg_req_v_o,
  output logic                    post_gg_view_o,
  output logic [POST_XW-3:0]      post_gg_cx_o,
  output logic [POST_YW-3:0]      post_gg_cy_o,
  input  logic                    post_gg_present_i,
  input  logic [15:0]             post_gg_glow_i,
  input  logic                    post_gg_ink_i,
  output logic                    post_atm_req_v_o,
  output logic [POST_XW-1:0]      post_atm_req_x_o,
  output logic [POST_YW-1:0]      post_atm_req_y_o,
  input  logic                    post_atm_en_i,
  input  logic                    post_atm_valid_i,
  input  logic [15:0]             post_atm_rgb_i,
  input  logic [7:0]              post_atm_opacity_i,
  input  logic                    post_atm_add_i,
  input  logic [7:0]              post_bloom_gain_i,
  input  logic                    post_grade_valid_i,
  input  logic                    post_pv_we_i,
  input  logic [1:0]              post_pv_sel_i,
  input  logic [5:0]              post_pv_addr_i,
  input  logic [71:0]             post_pv_data_i,
  input  logic signed [8:0]       post_bias_r_i,
  input  logic signed [8:0]       post_bias_g_i,
  input  logic signed [8:0]       post_bias_b_i,
  input  logic [15:0]             post_flash_rgb_i,
  input  logic [7:0]              post_flash_amt_i,
  input  logic [15:0]             post_ink_rgb_i,
  output logic                    post_hud_req_v_o,
  output logic [POST_XW-1:0]      post_hud_req_x_o,
  output logic [POST_YW-1:0]      post_hud_req_y_o,
  input  logic                    post_hud_valid_i,
  input  logic [15:0]             post_hud_rgb_i,
  output logic                    post_o_valid_o,
  input  logic                    post_o_ready_i,
  output logic [15:0]             post_o_rgb_o,
  output logic [POST_XW-1:0]      post_o_x_o,
  output logic [POST_YW-1:0]      post_o_y_o,
  output logic                    post_o_last_o,
  output logic                    post_echo_valid_o,
  output logic [15:0]             post_echo_rgb_o,

  // ---- COMPOSITOR evidence -------------------------------------------------
  output logic [31:0]             post_displacement_edge_clamps_o,
  output logic [31:0]             post_bloom_cells_contributing_o,
  output logic [31:0]             post_passes_completed_o,
  output logic [31:0]             post_grading_table_missing_o,
  output logic [31:0]             post_plane_missing_o,
  output logic [31:0]             post_line_fill_writes_o,
  output logic [31:0]             post_output_writes_o,
  output logic [31:0]             post_plane_reads_o,
  output logic [31:0]             post_ring_hazard_o,

  // ---- I18/I19: the histogram's events and its host window ----------------
  input  logic                    hist_ev_valid_i,
  input  logic [HIST_LANES-1:0]   hist_ev_lane_valid_i,
  input  logic [HIST_LANES*HIST_EW-1:0] hist_ev_err_i,
  input  logic [15:0]             hist_ev_src_id_i,
  output logic                    hist_ev_ready_o,
  input  logic                    hist_rd_valid_i,
  input  logic [HIST_BINW-1:0]    hist_rd_bin_i,
  output logic                    hist_rd_ready_o,
  output logic                    hist_rd_data_valid_o,
  output logic [HIST_CW-1:0]      hist_rd_count_o,
  output logic                    hist_snap_valid_o,
  output logic [HIST_CW-1:0]      hist_snap_total_o,
  output logic [15:0]             hist_snap_src_id_o,
  output logic [HIST_CW-1:0]      hist_snap_index_o,
  output logic [HIST_CW-1:0]      hist_events_o,
  output logic [HIST_CW-1:0]      hist_updates_o,
  output logic [HIST_CW-1:0]      hist_stall_cycles_o,
  output logic [HIST_CW-1:0]      hist_bin_sat_o,
  output logic [HIST_CW-1:0]      hist_fwd_hits_o,
  output logic [HIST_CW-1:0]      hist_host_conflict_o,
  output logic [HIST_CW-1:0]      hist_snapshots_o,
  output logic [HIST_CW-1:0]      hist_frozen_write_o,

  // ---- I20/LIGHTING SEAM: tied low, see the header ------------------------
  output logic                    light_seam_connected_o,

  // ==========================================================================
  // zhao_shell_top_v2's DECLARATION, CARRIED THROUGH VERBATIM.
  //
  // Comments and all, because the shell is the authority on its own seams and
  // a paraphrase here would be a second, drifting description of them. Every
  // one of these is connected straight to `u_shell` below; this core neither
  // renames nor reinterprets any of them.
  // ==========================================================================
  // ---- clocks + reset (harness-driven, frozen ratios: vid = gpu/2,
  // ---- audio = gpu/4, fixed phase — plan R1) -----------------------------
  input  logic gpu_clk,
  input  logic vid_clk,
  input  logic audio_clk,
  input  logic rst_n,

  // ---- PACKET-H: THE V3 PROGRAMMING CHANNEL -----------------------------
  // Twenty inputs. The historical shell has a command scheduler and an HPS
  // bridge and NO V3 programming channel at all, so these are not a rename
  // of anything -- they are the binding/palette/page path arriving at the
  // shell boundary for the first time. A command-stream decoder would be a
  // second design with its own ABI and tests that this packet's gate does
  // not ask for, and one inserted later sits BEHIND these ports and changes
  // nothing the V2 blocks see.
  input  logic        cfg_valid_i,
  output logic        cfg_ready_o,
  input  logic [1:0]  cfg_op_i,
  input  logic [7:0]  cfg_page_generation_i,
  input  logic [7:0]  cfg_selector_i,
  input  logic [74:0] cfg_row_i,
  input  logic [31:0] cfg_crc32_i,
  output logic        cfg_rsp_valid_o,
  input  logic        cfg_rsp_ready_i,
  output logic [1:0]  cfg_rsp_op_o,
  output logic [3:0]  cfg_rsp_status_o,
  output logic [7:0]  cfg_rsp_page_generation_o,
  output logic [7:0]  active_page_generation_o,
  input  logic        pal_load_valid_i,
  output logic        pal_load_ready_o,
  input  logic [1:0]  pal_load_op_i,
  input  logic [1:0]  pal_load_slot_i,
  input  logic [7:0]  pal_load_gen_i,
  input  logic [7:0]  pal_load_idx_i,
  input  logic [15:0] pal_load_rgb565_i,
  input  logic        pal_load_crc_ok_i,

  // ---- PACKET-H: attribute carriage, ENGINE1 share, clear, sheet --------
  input  logic [46:0]  tri_area2_i,
  input  logic [239:0] tri_invw_plane_i,
  input  logic [239:0] tri_u_over_w_plane_i,
  input  logic [239:0] tri_v_over_w_plane_i,
  input  logic [297:0] tri_flat_request_i,
  input  logic [47:0]  tri_continuation_tail_i,
  input  logic [31:0]  tri_fragment_state_i,
  input  logic         fill_req_ready_i,
  output logic         fill_req_valid_o,
  output logic [31:0]  fill_req_addr_o,
  input  logic         fill_data_valid_i,
  input  logic [15:0]  fill_data_i,
  input  logic         fill_refused_i,
  input  logic [63:0]  frame_clear_word_i,
  input  logic         sheet_req_ready_i,
  output logic         sheet_req_valid_o,
  output logic [1:0]   sheet_req_op_o,
  output logic [31:0]  sheet_req_handle_o,
  output logic [11:0]  sheet_req_texel_o,
  output logic [15:0]  sheet_req_src_id_o,

  // ---- PACKET-H: the video-domain barrier and echo ----------------------
  // `lease_open` is produced by zhao_video_ready_bridge_v2 and the two
  // `barrier_done` levels by zhao_fb_ready_cdc_v2, so the reset-epoch
  // barrier is self-driven and nothing outside declares it complete.
  input  logic        blank_cmd_i,
  input  logic        scanout_ack_i,
  input  logic        frame_swap_valid_i,
  input  logic        frame_swap_slot_i,
  output logic        blank_ack_o,
  output logic        blank_active_o,
  output logic        lease_open_o,
  output logic [1:0]  frame_slot_ready_o,

  // ---- PACKET-H: lifecycle evidence -------------------------------------
  // The gate asks for exact owner and hierarchy census; these are how a
  // reader gets it without a waveform.
  output logic [31:0] v2_requests_accepted_o,
  output logic [31:0] v2_responses_accepted_o,
  output logic [31:0] v2_leases_granted_o,
  output logic [31:0] v2_leases_refused_o,
  output logic [31:0] v2_faults_latched_o,
  output logic [31:0] v2_publications_o,
  output logic [31:0] v2_releases_o,
  output logic [31:0] v2_ready_events_o,
  output logic [31:0] v2_swaps_o,
  output logic [31:0] v2_contentions_o,
  output logic [31:0] v2_clear_handshakes_o,
  output logic [31:0] v2_frames_admitted_o,
  output logic [31:0] v2_blit_leases_acquired_o,
  output logic [31:0] v2_blit_leases_refused_o,

  // ---- FRAME_RING view (harness = HPS, D10; memory_rules.md 4.1) ---------
  input  logic [1:0]  hps_state_i [0:2],
  input  logic [31:0] hps_byte_len_i [0:2],
  output logic        ring_wr_valid_o,
  output logic [1:0]  ring_wr_slot_o,
  output logic [1:0]  ring_wr_state_o,
  input  logic        ring_wr_ready_i,

  // ---- HPS bridge, harness side (memory_rules.md 3) ----------------------
  output logic        hps_req_valid_o,
  output logic        hps_req_write_o,
  output logic [31:0] hps_req_addr_o,
  output logic [6:0]  hps_req_len_o,
  input  logic        hps_req_grant_i,
  output logic        hps_wr_valid_o,
  output logic [63:0] hps_wr_data_o,
  output logic        hps_wr_last_o,
  input  logic        hps_rd_valid_i,
  input  logic [63:0] hps_rd_data_i,
  input  logic        hps_rd_last_i,

  // ---- raw decoded pad state (input_rules.md 1/4) ------------------------
  input  logic [3:0]  pad_present_i,
  input  logic [31:0] pad_buttons_i [0:3],
  input  logic [15:0] pad_lx_i [0:3],
  input  logic [15:0] pad_ly_i [0:3],
  input  logic [15:0] pad_rx_i [0:3],
  input  logic [15:0] pad_ry_i [0:3],

  // ---- audio: ring-read client seam (pairs in) + PCM out -----------------
  input  logic        aud_wr_valid_i,
  input  logic [15:0] aud_wr_l_i,
  input  logic [15:0] aud_wr_r_i,
  output logic        aud_wr_ready_o,
  output logic        aud_refill_req_o,
  output logic [11:0] aud_occupancy_o,
  output logic        pcm_valid_o,
  output logic [15:0] pcm_l_o,
  output logic [15:0] pcm_r_o,
  output logic        underrun_status_o,
  output logic [31:0] audio_underruns_o,

  // ---- displayed pixel stream (vid domain, post-scaler) ------------------
  output logic        px_valid_o,
  output logic [15:0] px_rgb_o,
  output logic [9:0]  px_x_o,
  output logic [7:0]  px_y_o,
  output logic        px_hsync_o,
  output logic        px_vsync_o,
  output logic        px_hblank_o,
  output logic        px_vblank_o,
  output logic        scaler_violation_o,

  // ---- DEBUG.CRC (gpu domain): the displayed-stream CRC ------------------
  output logic [31:0] crc_frame_o,
  output logic        crc_valid_o,
  output logic [31:0] crc_bytes_o,
  output logic        crc_size_err_o,

  // ---- frame boundary observability --------------------------------------
  output logic        gpu_tick_o,
  output logic [31:0] gpu_tick_frame_id_o,
  output logic        gpu_tick_repeated_o,
  output logic [0:0]  gpu_complete_slot_o,
  output logic [63:0] deadline_faults_o,     // FRAMECTL (vid)
  output logic [63:0] frame_cycles_o,        // FRAMECTL (vid)

  // ---- CMD observability --------------------------------------------------
  output logic [2:0]  slot_state_o [0:2],
  output logic        fence_valid_o,
  output logic [1:0]  fence_slot_o,
  output logic        fence_ok_o,
  output logic [7:0]  fence_status_o,
  output logic [1:0]  mode_act_o,
  output logic        dma_done_o,
  output logic [7:0]  dma_status_o,
  output logic        blit_done_o,
  output logic [7:0]  blit_status_o,

  // ---- INPUT observability ------------------------------------------------
  output logic [639:0] pad_frame_flat_o,
  output logic [15:0]  pad_sequence_o [0:3],
  output logic [63:0]  input_gaps_o,
  output logic [7:0]   rumble_duty_o [0:3],
  output logic [3:0]   rumble_active_o,
  output logic [3:0]   rumble_pwm_o,
  output logic [63:0]  rumble_drops_o,

  // ---- DEBUG.COUNTERS read window ----------------------------------------
  input  logic        cnt_snap_ready_i,
  output logic        cnt_snap_valid_o,
  output logic [15:0] cnt_snap_id_o,
  output logic [63:0] cnt_snap_value_o,
  output logic        cnt_window_open_o,
  output logic        cnt_cat_violation_o,

  // ---- MEM observability + shell integrity tripwires ---------------------
  output logic [31:0] guard_violations_o,    // both guards, summed
  output logic [63:0] starvation_o,
  output logic        init_done_o,
  output logic [31:0] refresh_stalls_o,
  output logic [31:0] bank_conflicts_o,
  output logic [31:0] scanout_preempted_o,
  output logic [31:0] hps_err_count_o,
  output logic        shell_err_wfifo_o,     // write queue over/underflow
  output logic        shell_err_route_o,     // burst from an impossible client
  output logic        shell_err_cdc_o,       // starvation sample moved at tick
  output logic        shell_err_framer_o,    // record queue overflow (glue 3)

  // ---- RENDER: the geometry front door ----------------------------------
  // The console's first render path. GEOM.BINNER -> RASTER.TILE_PIPE ->
  // RASTER.FBWRITE -> MEM.GUARD -> the arbiter ENGINE0 port, which zhao_pkg has
  // always called a "reserved guaranteed slot" and which was tied to zero until
  // now.
  //
  // The triangle port sits at the SHELL edge because CMD.SCHEDULER does not
  // feed it yet. That is provisional and says so: when the command front end
  // grows a draw path these become internal and nothing else here changes.
  //
  // What it draws is FLAT-shaded. zhao_raster_tile_pipe carries one colour,
  // alpha, depth and texel across a triangle because interpolating them is
  // GEOM.SETUP work and GEOM.SETUP has no attribute input yet. This is the
  // path, not the picture.
  input  logic        render_frame_begin_i,
  input  logic        render_frame_end_i,
  input  logic [5:0]  render_grid_w_i,
  input  logic [5:0]  render_grid_h_i,

  input  logic               render_tri_valid_i,
  output logic               render_tri_ready_o,
  // ---- D22 TREAD 10: the geometry memory clients -----------------------------
  // The last thing the bench still PLAYED was memory itself. Every earlier
  // tread took something the bench supplied and gave it to a composed block;
  // GEOM.MESHFETCH and GEOM.ASSETFETCH still had their guard grants answered
  // and their beats fabricated by hand, so the whole staircase rested on a
  // memory that granted immediately and answered in one cycle.
  //
  // These two ports put those fetchers behind the REAL MEM.GUARD and
  // VRAM.ARBITER that this shell already instantiates, on the arbiter's two
  // previously unused client slots. The bench keeps the fetchers -- relocating
  // them into production is a separate concern and is recorded as such -- but
  // it stops inventing the answers.
  //
  // Contention is the point. Everything measured in treads 6 through 9 assumed
  // a memory that never says no, and `prefetch_stall_o` was connected before
  // this tread precisely so its uncontended reading (27) exists to compare
  // against.
  input  var zhao_guard_req_t geom_guard_req_i,
  output var zhao_guard_rsp_t geom_guard_rsp_o,
  // ...and the beats coming back. Until this tread the shell had ONE reader,
  // so read data was wired straight to the scanout packer. Now it has two, and
  // which one a returning word belongs to is a fact that has to be tracked
  // rather than assumed.
  output var logic            geom_beat_valid_o,
  output var logic [63:0]     geom_beat_data_o,
  output var logic            geom_beat_last_o,
  input  logic signed [22:0] render_kx0_i, render_ky0_i,
  input  logic signed [47:0] render_kc0_i,
  input  logic signed [22:0] render_kx1_i, render_ky1_i,
  input  logic signed [47:0] render_kc1_i,
  input  logic signed [22:0] render_kx2_i, render_ky2_i,
  input  logic signed [47:0] render_kc2_i,
  input  logic        [ 2:0] render_tl_i,
  input  logic signed [20:0] render_ax_i, render_ay_i,
  input  logic signed [20:0] render_bx_i, render_by_i,
  input  logic signed [20:0] render_cx_i, render_cy_i,
  input  logic signed [11:0] render_min_x_i, render_max_x_i,
  input  logic signed [11:0] render_min_y_i, render_max_y_i,
  input  logic        [15:0] render_src_id_i,

  input  logic [63:0] render_fill_word_i,
  input  logic [63:0] render_clear_word_i,
  input  logic [31:0] render_state_i,
  input  logic [ 7:0] render_src_a_i,
  input  logic [23:0] render_texel_rgb_i,
  input  logic [ 7:0] render_texel_a_i,
  input  logic [ 7:0] render_texel_idx_i,

  input  logic [26:0] render_fb_base_i,
  input  logic [15:0] render_fb_stride_i,

  // WHO HOLDS THE FRAMEBUFFER-WRITE LEASE THIS FRAME.
  // 0 = DEBUG.FRAMEBLIT, 1 = RASTER.FBWRITE.
  //
  // ONE SIGNAL, BOTH GUARDS. The first version of this wiring hardwired
  // `fb_writer` to 0 inside the blit guard and 1 inside the render guard, so
  // each compared the client against its OWN constant and BOTH writers passed
  // at once -- which is precisely the corruption the lease exists to prevent,
  // reintroduced by the wiring of the block that prevents it. The owner is one
  // value, and both guards are told the same one.
  //
  // Provisional at the shell edge: VIDEO.SLOTMGR already owns one lease at a
  // time with a generation, and this becomes that lease's owner field once
  // CMD.SCHEDULER selects the writer. Until then it is an input so a bench can
  // exercise either writer, and it defaults to the blit at the caller.
  input  logic        fb_writer_i,

  output logic        render_drain_done_o,
  output logic        render_busy_o,
  output logic [31:0] render_pixels_o,
  output logic [31:0] render_bursts_o,
  output logic        render_stream_error_o,
  // The frame transaction. `render_drained_o` is the ONLY signal a frame
  // controller may publish a slot on: it means every word handed to the guard
  // has been RETIRED by the arbiter. `render_busy_o` falls when the last beat
  // is merely accepted, several stages earlier.
  output logic        render_drained_o,
  output logic        render_fatal_o,
  output logic [31:0] render_issued_words_o,
  output logic [31:0] render_retired_words_o,
  output logic        render_overflow_o,
  output logic        render_fragment_error_o,

  // ---- SDR PHY pins (behavioural model in the tb wrapper; D2) ------------
  output logic        phy_cs_n_o,
  output logic        phy_ras_n_o,
  output logic        phy_cas_n_o,
  output logic        phy_we_n_o,
  output logic [12:0] phy_a_o,
  output logic [1:0]  phy_ba_o,
  output logic [15:0] phy_dq_o,
  output logic        phy_dq_oe_o,
  output logic [1:0]  phy_dqm_o,
  input  logic [15:0] phy_dq_i
);

  // ==========================================================================
  // ELABORATION GUARDS.
  //
  // Inside `initial begin ... end` because Quartus 17.0 rejects a bare
  // module-scope `if` with "syntax error near text: `if`; expecting
  // `endmodule`" -- and Verilator's `--lint-only` accepts the bare form with 0
  // diagnostics, so a clean lint says nothing whatever about this. See
  // CLAUDE.md, 2026-09-08.
  // ==========================================================================
  initial begin
    if (GEOM_ARENA_W + GEOM_INDEX_W > GEOM_PAY_A_W)
      $fatal(1, "zhao_console_core: the geometry rider is %0d bits (ARENA_W %0d + INDEX_W %0d) but GEOM_PAY_A_W is %0d",
             GEOM_ARENA_W + GEOM_INDEX_W, GEOM_ARENA_W, GEOM_INDEX_W, GEOM_PAY_A_W);
    if (POST_LINE_W < 384)
      $fatal(1, "zhao_console_core: POST_LINE_W is %0d, narrower than the Z60 view (384)", POST_LINE_W);
    if (PART_REC_W != 128)
      $fatal(1, "zhao_console_core: PART_REC_W is %0d; particle128 (amendment C2) is 128", PART_REC_W);
  end

  // ==========================================================================
  // THE LIGHTING SEAM. Tied low on purpose -- see the header for exactly what
  // must be connected here, and why none of it is.
  // ==========================================================================
  assign light_seam_connected_o = 1'b0;

  // ==========================================================================
  // GLUE 1: THE VIDEO MODE IS THE COMPOSITOR'S PASS GEOMETRY.  REAL.
  //
  // `mode_act_o` is the shell's LATCHED active mode, so the compositor's pass
  // size follows the console's real mode instead of a constant. In Duo the
  // block runs once per 256x192 VIEW (its own header), which is why the Duo
  // arm is the view and not the 512-wide canvas.
  //
  // The numbers are zhao_pkg's: ZHAO_TIMING[Z60].h_active = 384,
  // [STORM].h_active = 320, and ZHAO_DUO_VIEW_W/H = 256/192.
  // ==========================================================================
  localparam logic [1:0]         MODE_Z60_C     = ZHAO_MODE_Z60;
  localparam logic [1:0]         MODE_STORM_C   = ZHAO_MODE_STORM;
  localparam logic [POST_XW-1:0] POST_W_Z60_C   = 384;
  localparam logic [POST_XW-1:0] POST_W_STORM_C = 320;
  localparam logic [POST_XW-1:0] POST_W_DUO_C   = 256;
  localparam logic [POST_YW-1:0] POST_H_FULL_C  = 240;
  localparam logic [POST_YW-1:0] POST_H_DUO_C   = 192;

  logic [POST_XW-1:0] post_frame_w_c;
  logic [POST_YW-1:0] post_frame_h_c;

  always_comb begin
    case (mode_act_o)
      MODE_Z60_C: begin
        post_frame_w_c = POST_W_Z60_C;
        post_frame_h_c = POST_H_FULL_C;
      end
      MODE_STORM_C: begin
        post_frame_w_c = POST_W_STORM_C;
        post_frame_h_c = POST_H_FULL_C;
      end
      default: begin
        post_frame_w_c = POST_W_DUO_C;
        post_frame_h_c = POST_H_DUO_C;
      end
    endcase
  end

  // ==========================================================================
  // GLUE 2: THE FRAME EDGE IS THE TICK.  REAL.
  //
  // `gpu_tick_o` is FRAMECTL's frame boundary as the shell already publishes
  // it. The particle generation, the compositor pass and the measurement
  // interval all advance on it, so all three are on the console's real cadence
  // rather than on three private pulses.
  // ==========================================================================
  wire core_tick_c = gpu_tick_o;

  // ==========================================================================
  // PARTICLES: STATE -> UPDATE -> {COLLIDE, SPAWN} -> STATE
  // ==========================================================================
  wire                  ps_prt_valid, ps_prt_ready;
  wire [PART_REC_W-1:0] ps_prt_record;

  wire                  pu_out_valid;
  wire [PART_REC_W-1:0] pu_out_record;
  wire                  pu_out_survive;
  wire [3:0]            pu_out_events;

  wire                  pc_p_ready;
  wire                  pc_c_valid, pc_c_alive;
  wire [PART_REC_W-1:0] pc_c_record;
  wire                  ps_vrd_ready;

  wire                  sp_par_ready;
  wire                  sp_chl_valid, sp_chl_ready;
  wire [PART_REC_W-1:0] sp_chl_record;

  // --------------------------------------------------------------------------
  // GLUE 3: THE ONE-TO-TWO FORK ON PART.UPDATE'S VERDICT.
  //
  // PART.UPDATE emits ONE stream that TWO blocks need: the record goes to
  // PART.COLLIDE and the record-plus-events goes to PART.SPAWN. A plain
  // `ready & ready` fork stalls both consumers whenever either is busy AND
  // presents the beat twice to whichever accepted first. This keeps one
  // "already took it" bit per branch instead, so each consumer sees the beat
  // exactly once and the beat retires when both have taken it.
  //
  // Neither branch's VALID reads the other branch's READY, so there is no
  // combinational loop through the consumers -- which is the failure mode this
  // shape exists to avoid, not a property to be argued about afterwards.
  // --------------------------------------------------------------------------
  logic fork_col_done_q, fork_spw_done_q;

  wire fork_col_valid_c = pu_out_valid && !fork_col_done_q;
  wire fork_spw_valid_c = pu_out_valid && !fork_spw_done_q;
  wire fork_col_take_c  = fork_col_valid_c && pc_p_ready;
  wire fork_spw_take_c  = fork_spw_valid_c && sp_par_ready;
  wire fork_col_held_c  = fork_col_done_q || fork_col_take_c;
  wire fork_spw_held_c  = fork_spw_done_q || fork_spw_take_c;
  wire pu_out_ready_c   = fork_col_held_c && fork_spw_held_c;

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      fork_col_done_q <= 1'b0;
      fork_spw_done_q <= 1'b0;
    end else if (pu_out_valid && pu_out_ready_c) begin
      fork_col_done_q <= 1'b0;
      fork_spw_done_q <= 1'b0;
    end else begin
      if (fork_col_take_c) fork_col_done_q <= 1'b1;
      if (fork_spw_take_c) fork_spw_done_q <= 1'b1;
    end
  end

  // --------------------------------------------------------------------------
  // GLUE 4: THE SURVIVE VERDICT ACROSS PART.COLLIDE.
  //
  // PART.STATE's write-back needs ONE survive bit and TWO blocks decide it:
  // PART.UPDATE kills on lifetime (`out_survive_o`) and PART.COLLIDE kills on a
  // DIE response (`c_alive_o`). PART.COLLIDE has no survive input and no
  // passthrough for one, so the update's verdict has to cross it beside the
  // record.
  //
  // THE ALIGNMENT IS THE WHOLE POINT, and it is structural rather than argued:
  // PART.COLLIDE is "one beat in, one beat out, fixed latency" with
  // `p_ready_o = !c_valid_o || c_ready_i`, so exactly one beat is ever in
  // flight and its record register is loaded on `p_valid_i && p_ready_o`. This
  // register is loaded on THE SAME condition, rebuilt from the same two wires,
  // so the survive bit cannot separate from the record it describes -- there is
  // no second enable for them to drift across.
  // --------------------------------------------------------------------------
  logic pc_survive_q;

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n)              pc_survive_q <= 1'b0;
    else if (fork_col_take_c) pc_survive_q <= pu_out_survive;
  end

  wire ps_vrd_survive_c = pc_c_alive && pc_survive_q;

  // --------------------------------------------------------------------------
  // GLUE 5: THE PARENT ID.
  //
  // PART.SPAWN seeds its hash with a parent id and the particle128 record
  // (amendment C2) has no id field, so the console assigns one: the particle's
  // ORDINAL within the generation, cleared at every tick. See the header entry
  // I9 -- this is a decision taken here, not a port that was tied off, and it
  // is wrong if ids must survive compaction.
  // --------------------------------------------------------------------------
  logic [PART_PID_W-1:0] part_ordinal_q;

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n)                              part_ordinal_q <= '0;
    else if (core_tick_c)                    part_ordinal_q <= '0;
    else if (pu_out_valid && pu_out_ready_c) part_ordinal_q <= part_ordinal_q + 1'b1;
  end

  zhao_part_state #(
    .CAPACITY  (PART_CAPACITY),
    .CHILD_D   (PART_CHILD_D),
    .SPECIES_N (PART_SPECIES_N),
    .REC_W     (PART_REC_W)
  ) u_part_state (
    .clk          (gpu_clk),
    .rst_n        (rst_n),
    .tick_start_i (core_tick_c),
    .tick_busy_o  (part_tick_busy_o),
    .tick_done_o  (part_tick_done_o),

    // I1: the generation store. Harness = memory; MEM.HPS.BRIDGE has no
    // particle client port, so there is no route to it inside this core.
    .rd_valid_i   (part_rd_valid_i),
    .rd_ready_o   (part_rd_ready_o),
    .rd_record_i  (part_rd_record_i),
    .rd_last_i    (part_rd_last_i),

    // REAL: straight into PART.UPDATE.
    .prt_valid_o  (ps_prt_valid),
    .prt_ready_i  (ps_prt_ready),
    .prt_record_o (ps_prt_record),

    // REAL: the verdict comes back from PART.COLLIDE, with the survive bit
    // that crossed it (glue 4).
    .vrd_valid_i   (pc_c_valid),
    .vrd_ready_o   (ps_vrd_ready),
    .vrd_survive_i (ps_vrd_survive_c),
    .vrd_record_i  (pc_c_record),

    // REAL: children from PART.SPAWN.
    .chl_valid_i  (sp_chl_valid),
    .chl_ready_o  (sp_chl_ready),
    .chl_record_i (sp_chl_record),

    .wr_valid_o   (part_wr_valid_o),
    .wr_ready_i   (part_wr_ready_i),
    .wr_record_o  (part_wr_record_o),

    .survivors_o                  (part_survivors_o),
    .children_written_o           (part_children_written_o),
    .children_dropped_capacity_o  (part_children_dropped_capacity_o),
    .staging_stall_cycles_o       (part_staging_stall_cycles_o),
    .species_refused_o            (part_species_refused_o)
  );

  zhao_part_update #(
    .SPECIES_N (PART_SPECIES_N),
    .REC_W     (PART_REC_W),
    .AGE_W     (PART_AGE_W)
  ) u_part_update (
    .clk        (gpu_clk),
    .rst_n      (rst_n),

    // REAL: from PART.STATE.
    .in_valid_i (ps_prt_valid),
    .in_ready_o (ps_prt_ready),
    .in_record_i(ps_prt_record),

    // I2: the species descriptor table has no owner in this tree.
    .spc_index_o   (part_upd_spc_index_o),
    .spc_recipe_i  (part_upd_spc_recipe_i),
    .spc_lifetime_i(part_upd_spc_lifetime_i),
    .spc_age_mark_i(part_upd_spc_age_mark_i),
    .spc_drag_i    (part_upd_spc_drag_i),
    .spc_grav_i    (part_upd_spc_grav_i),
    .spc_strength_i(part_upd_spc_strength_i),
    .spc_cx_i      (part_upd_spc_cx_i),
    .spc_cy_i      (part_upd_spc_cy_i),
    .spc_cz_i      (part_upd_spc_cz_i),
    .spc_p0_i      (part_upd_spc_p0_i),
    .spc_p1_i      (part_upd_spc_p1_i),
    .spc_p2_i      (part_upd_spc_p2_i),

    // I5: FIELD.SEQ.FLOW is not composed.
    .fld_valid_i(part_fld_valid_i),
    .fld_ax_i   (part_fld_ax_i),
    .fld_ay_i   (part_fld_ay_i),
    .fld_az_i   (part_fld_az_i),

    // I4: TIED TO ZERO -- PART.COLLIDE emits no velocity triple, so step 6
    // has no producer. `collisions_applied_o` below CANNOT MOVE in this core,
    // and synthesis will fold this datapath away: read the header entry before
    // quoting either the counter or this block's area.
    .col_valid_i(1'b0),
    .col_vx_i   ({PART_VEL_W{1'b0}}),
    .col_vy_i   ({PART_VEL_W{1'b0}}),
    .col_vz_i   ({PART_VEL_W{1'b0}}),

    // I3: the curve table has no owner in this tree.
    .crv_index_o (part_crv_index_o),
    .crv_size_i  (part_crv_size_i),
    .crv_colour_i(part_crv_colour_i),

    // REAL: the verdict, forked to PART.COLLIDE and PART.SPAWN (glue 3).
    .out_valid_o  (pu_out_valid),
    .out_ready_i  (pu_out_ready_c),
    .out_record_o (pu_out_record),
    .out_survive_o(pu_out_survive),
    .out_refused_o(part_upd_beat_refused_o),
    .out_events_o (pu_out_events),
    .out_colour_en_o(part_colour_en_o),
    .out_colour_o (part_colour_o),

    .particles_updated_o     (part_updated_o),
    .particles_refused_o     (part_upd_refused_o),
    .particles_died_by_age_o (part_died_by_age_o),
    .velocity_saturations_o  (part_velocity_saturations_o),
    .position_saturations_o  (part_position_saturations_o),
    .collisions_applied_o    (part_collisions_applied_o),
    .hist_sel_i              (part_hist_sel_i),
    .hist_val_o              (part_hist_val_o)
  );

  zhao_part_collide #(
    .REC_W (PART_REC_W),
    .POS_W (PART_POS_W),
    .VEL_W (PART_VEL_W),
    .NRM_W (PART_NRM_W),
    .FX_W  (PART_FX_W)
  ) u_part_collide (
    .clk      (gpu_clk),
    .rst_n    (rst_n),

    // REAL: branch A of the fork.
    .p_valid_i(fork_col_valid_c),
    .p_ready_o(pc_p_ready),
    .p_record_i(pu_out_record),

    // I2: the same missing descriptor table.
    .d_response_i   (part_col_d_response_i),
    .d_restitution_i(part_col_d_restitution_i),
    .d_friction_i   (part_col_d_friction_i),
    .d_damping_i    (part_col_d_damping_i),

    // I6: TERRAIN.PATCH emits heights and no surface normal.
    .t_valid_i (part_ter_valid_i),
    .t_height_i(part_ter_height_i),
    .t_nx_i    (part_ter_nx_i),
    .t_ny_i    (part_ter_ny_i),
    .t_nz_i    (part_ter_nz_i),

    // I7: the one plane, a per-frame owner value with no CMD path.
    .pl_en_i(part_plane_en_i),
    .pl_nx_i(part_plane_nx_i),
    .pl_ny_i(part_plane_ny_i),
    .pl_nz_i(part_plane_nz_i),
    .pl_c_i (part_plane_c_i),

    // REAL: straight back into PART.STATE's write-back channel.
    .c_valid_o  (pc_c_valid),
    .c_ready_i  (ps_vrd_ready),
    .c_record_o (pc_c_record),
    .c_alive_o  (pc_c_alive),
    .c_contact_o(part_contact_o),
    .c_response_o(part_response_o),
    .c_refused_o(part_col_refused_o),

    .contacts_ignore_o           (part_contacts_ignore_o),
    .contacts_die_o              (part_contacts_die_o),
    .contacts_stick_o            (part_contacts_stick_o),
    .contacts_slide_o            (part_contacts_slide_o),
    .contacts_bounce_o           (part_contacts_bounce_o),
    .contacts_terrain_o          (part_contacts_terrain_o),
    .contacts_plane_o            (part_contacts_plane_o),
    .already_inside_at_entry_o   (part_already_inside_at_entry_o),
    .terrain_sample_unavailable_o(part_terrain_sample_unavailable_o),
    .response_refused_o          (part_response_refused_o),
    .field_clamps_o              (part_field_clamps_o)
  );

  zhao_part_spawn #(
    .REC_W     (PART_REC_W),
    .SPECIES_N (PART_SPECIES_N),
    .PID_W     (PART_PID_W),
    .TICK_W    (PART_TICK_W)
  ) u_part_spawn (
    .clk         (gpu_clk),
    .rst_n       (rst_n),
    .tick_start_i(core_tick_c),

    // REAL: branch B of the fork, with PART.UPDATE's own event bits and the
    // shell's real frame id as the tick seed.
    .par_valid_i (fork_spw_valid_c),
    .par_ready_o (sp_par_ready),
    .par_record_i(pu_out_record),
    .par_id_i    (part_ordinal_q),
    .par_events_i(pu_out_events),
    .tick_i      (gpu_tick_frame_id_o),

    // I2: the same missing descriptor table.
    .spc_species_o  (part_spw_spc_species_o),
    .spc_event_o    (part_spw_spc_event_o),
    .spc_known_i    (part_spw_spc_known_i),
    .spc_child_spc_i(part_spw_spc_child_spc_i),
    .spc_count_i    (part_spw_spc_count_i),

    // REAL: children straight into PART.STATE's staging channel.
    .chl_valid_o(sp_chl_valid),
    .chl_ready_i(sp_chl_ready),
    .chl_record_o(sp_chl_record),

    // I8: PART.STATE exposes no capacity-full level.
    .cap_full_i(part_cap_full_i),

    .children_requested_o     (part_children_requested_o),
    .children_emitted_o       (part_children_emitted_o),
    .children_refused_o       (part_children_refused_o),
    .spawn_by_event0_o        (part_spawn_by_event0_o),
    .spawn_by_event1_o        (part_spawn_by_event1_o),
    .spawn_by_event2_o        (part_spawn_by_event2_o),
    .spawn_by_event3_o        (part_spawn_by_event3_o),
    .refused_count_gt_max_o   (part_refused_count_gt_max_o),
    .refused_unknown_species_o(part_refused_unknown_species_o),
    .refused_capacity_o       (part_refused_capacity_o),
    .max_children_in_tick_o   (part_max_children_in_tick_o)
  );

  // ==========================================================================
  // GEOMETRY: SKIN -> GROUP_SEQ -> the SHARED PROJECTOR -> the LANE -> back.
  //
  // This is the composition `design/prod_manifest.yml` says the shared
  // projector was waiting for: "the producer is still absent, so this is not
  // yet adoptable on its own". `zhao_geom_group_seq` is that producer, and the
  // loop below closes -- job in, vertices through client A, landings counted,
  // arena sealed, handle out.
  // ==========================================================================
  wire                    gs_v_valid, gs_v_ready;
  wire signed [31:0]      gs_v_x, gs_v_y, gs_v_z;

  wire                    gs_a_valid, gs_a_ready;
  wire signed [31:0]      gs_a_vx, gs_a_vy, gs_a_vz;
  wire                    gs_a_view;
  wire [GEOM_PAY_A_W-1:0] gs_a_payload;

  wire                     pj_a_valid;
  wire signed [20:0]       pj_a_x, pj_a_y;
  wire signed [31:0]       pj_a_d;
  wire [30:0]              pj_a_w;
  wire                     pj_a_behind;
  wire [GEOM_PAY_A_W-1:0]  pj_a_payload;

  wire                     gs_open, gs_seal;
  wire [GEOM_ARENA_W-1:0]  gs_open_arena, gs_seal_arena;
  wire [GEOM_GEN_W-1:0]    ln_open_gen;
  wire [GEOM_ARENA_W-1:0]  gs_rider_arena;
  wire [GEOM_INDEX_W-1:0]  gs_rider_index;
  wire [GEOM_PAY_A_W-1:0]  ln_rider_payload;
  wire                     ln_fill_landed;
  wire [GEOM_ARENA_W-1:0]  ln_fill_arena;

  zhao_geom_skin #(
    .MUL_LANES (GEOM_MUL_LANES)
  ) u_geom_skin (
    .clk       (gpu_clk),
    .rst_n     (rst_n),

    // I10: GEOM.POSE and GEOM.VDECODE are not composed in this tree.
    .v_valid_i (geom_skin_v_valid_i),
    .v_ready_o (geom_skin_v_ready_o),
    .v_x_i     (geom_skin_v_x_i),
    .v_y_i     (geom_skin_v_y_i),
    .v_z_i     (geom_skin_v_z_i),
    .v_w0_i    (geom_skin_v_w0_i),
    .v_rigid_i (geom_skin_v_rigid_i),
    .v_src_id_i(geom_skin_v_src_id_i),
    .a_m_i     (geom_skin_a_m_i),
    .b_m_i     (geom_skin_b_m_i),

    // REAL: skinned, view-independent vertices into the group sequencer.
    .o_valid_o (gs_v_valid),
    .o_ready_i (gs_v_ready),
    .o_x_o     (gs_v_x),
    .o_y_o     (gs_v_y),
    .o_z_o     (gs_v_z),
    .o_src_id_o(geom_skin_src_id_o),

    .vertices_transformed_o (geom_skin_vertices_transformed_o)
  );

  zhao_geom_group_seq #(
    .ARENAS      (GEOM_ARENAS),
    .DEPTH       (GEOM_DEPTH),
    .NVIEWS      (GEOM_NVIEWS),
    .GEN_W       (GEOM_GEN_W),
    .PAYLOAD_A_W (GEOM_PAY_A_W)
  ) u_geom_group_seq (
    .clk             (gpu_clk),
    .rst_n           (rst_n),

    // I11: the job port; GEOM.SETUP, the replay customer, is not composed.
    .job_valid_i     (geom_job_valid_i),
    .job_ready_o     (geom_job_ready_o),
    .job_count_i     (geom_job_count_i),
    .job_view_mask_i (geom_job_view_mask_i),
    .job_src_id_i    (geom_job_src_id_i),

    // REAL: from GEOM.SKIN.
    .v_valid_i (gs_v_valid),
    .v_ready_o (gs_v_ready),
    .v_x_i     (gs_v_x),
    .v_y_i     (gs_v_y),
    .v_z_i     (gs_v_z),

    // REAL: client A of the shared projection service.
    .a_valid_o  (gs_a_valid),
    .a_ready_i  (gs_a_ready),
    .a_vx_o     (gs_a_vx),
    .a_vy_o     (gs_a_vy),
    .a_vz_o     (gs_a_vz),
    .a_view_o   (gs_a_view),
    .a_payload_o(gs_a_payload),

    // REAL: the lane owns the rider layout and answers with the generation.
    .open_o        (gs_open),
    .open_arena_o  (gs_open_arena),
    .open_gen_i    (ln_open_gen),
    .rider_arena_o (gs_rider_arena),
    .rider_index_o (gs_rider_index),
    .rider_payload_i(ln_rider_payload),
    .seal_o        (gs_seal),
    .seal_arena_o  (gs_seal_arena),

    // REAL: landings, so the seal waits for arrival and not acceptance.
    .fill_landed_i (ln_fill_landed),
    .fill_arena_i  (ln_fill_arena),

    // I11: the sealed handle and its release.
    .grp_valid_o (geom_grp_valid_o),
    .grp_ready_i (geom_grp_ready_i),
    .grp_arena_o (geom_grp_arena_o),
    .grp_gen_o   (geom_grp_gen_o),
    .grp_count_o (geom_grp_count_o),
    .grp_view_o  (geom_grp_view_o),
    .grp_src_id_o(geom_grp_src_id_o),
    .rel_valid_i (geom_rel_valid_i),
    .rel_arena_i (geom_rel_arena_i),

    .groups_opened_o     (geom_groups_opened_o),
    .groups_sealed_o     (geom_groups_sealed_o),
    .vertices_sent_o     (geom_vertices_sent_o),
    .landings_o          (geom_landings_o),
    .jobs_refused_o      (geom_jobs_refused_o),
    .alloc_stall_cycles_o(geom_alloc_stall_cycles_o),
    .rel_unheld_o        (geom_rel_unheld_o),
    .seal_early_o        (geom_seal_early_o)
  );

  zhao_proj_subsystem #(
    .PAYLOAD_A_W (GEOM_PAY_A_W),
    .ARENAS      (PROJ_T_ARENAS),
    .DEPTH       (PROJ_T_DEPTH),
    .GEN_W       (GEOM_GEN_W)
  ) u_proj_subsystem (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    // I14: the matrix bank; CMD.SCHEDULER has no projection-config path.
    .cfg_we_i  (proj_cfg_we_i),
    .cfg_view_i(proj_cfg_view_i),
    .cfg_addr_i(proj_cfg_addr_i),
    .cfg_data_i(proj_cfg_data_i),
    .en_i      (proj_en_i),

    // REAL: client A in, from GEOM.GROUP_SEQ.
    .a_valid_i  (gs_a_valid),
    .a_ready_o  (gs_a_ready),
    .a_vx_i     (gs_a_vx),
    .a_vy_i     (gs_a_vy),
    .a_vz_i     (gs_a_vz),
    .a_view_i   (gs_a_view),
    .a_payload_i(gs_a_payload),

    // REAL: client A's results out, into the geometry arena lane.
    .a_valid_o  (pj_a_valid),
    .a_x_o      (pj_a_x),
    .a_y_o      (pj_a_y),
    .a_d_o      (pj_a_d),
    .a_w_o      (pj_a_w),
    .a_behind_o (pj_a_behind),
    .a_view_o   (proj_a_view_o),
    .a_payload_o(pj_a_payload),

    // I13: client B is TERRAIN, and zhao_terrain_group_seq is NOT composed
    // here. The shared projector is therefore measured with one live client.
    .b_valid_i(proj_b_valid_i),
    .b_ready_o(proj_b_ready_o),
    .b_vx_i   (proj_b_vx_i),
    .b_vy_i   (proj_b_vy_i),
    .b_vz_i   (proj_b_vz_i),
    .b_view_i (proj_b_view_i),
    .b_arena_i(proj_b_arena_i),
    .b_index_i(proj_b_index_i),
    .fill_landed_o(proj_fill_landed_o),
    .fill_arena_o (proj_fill_arena_o),
    .open_i       (proj_open_i),
    .open_arena_i (proj_open_arena_i),
    .open_gen_o   (proj_open_gen_o),
    .seal_i       (proj_seal_i),
    .seal_arena_i (proj_seal_arena_i),
    .ref_valid_i  (proj_ref_valid_i),
    .ref_ready_o  (proj_ref_ready_o),
    .ref_arena_i  (proj_ref_arena_i),
    .ref_gen_i    (proj_ref_gen_i),
    .ref_ia_i     (proj_ref_ia_i),
    .ref_ib_i     (proj_ref_ib_i),
    .ref_ic_i     (proj_ref_ic_i),
    .ref_src_id_i (proj_ref_src_id_i),
    .ref_view_i   (proj_ref_view_i),
    .ref_mat_a_i  (proj_ref_mat_a_i),
    .ref_mat_b_i  (proj_ref_mat_b_i),
    .ref_weight_i (proj_ref_weight_i),
    .out_valid_o  (proj_out_valid_o),
    .out_ready_i  (proj_out_ready_i),
    .out_ax_o     (proj_out_ax_o),
    .out_ay_o     (proj_out_ay_o),
    .out_bx_o     (proj_out_bx_o),
    .out_by_o     (proj_out_by_o),
    .out_cx_o     (proj_out_cx_o),
    .out_cy_o     (proj_out_cy_o),
    .out_behind_o (proj_out_behind_o),
    .out_src_id_o (proj_out_src_id_o),
    .out_ad_o     (proj_out_ad_o),
    .out_bd_o     (proj_out_bd_o),
    .out_cd_o     (proj_out_cd_o),
    .out_aw_o     (proj_out_aw_o),
    .out_bw_o     (proj_out_bw_o),
    .out_cw_o     (proj_out_cw_o),
    .out_view_o   (proj_out_view_o),
    .out_mat_a_o  (proj_out_mat_a_o),
    .out_mat_b_o  (proj_out_mat_b_o),
    .out_weight_o (proj_out_weight_o),
    .out_refused_o(proj_out_refused_o),
    .out_missed_o (proj_out_missed_o),

    .replay_triangles_o(proj_replay_triangles_o),
    .replay_refused_o  (proj_replay_refused_o),
    .replay_missed_o   (proj_replay_missed_o),
    .corner_hits_o     (proj_corner_hits_o),
    .corner_refusals_o (proj_corner_refusals_o),
    .corner_misses_o   (proj_corner_misses_o),
    .arena_overflow_o  (proj_arena_overflow_o),
    .arena_seal_short_o(proj_arena_seal_short_o),
    .shell_idle_o      (proj_shell_idle_o),
    .svc_busy_o        (proj_svc_busy_o),
    .a_grants_o        (proj_a_grants_o),
    .b_grants_o        (proj_b_grants_o),
    .contended_o       (proj_contended_o),
    .mat_refused_o     (proj_mat_refused_o)
  );

  zhao_geom_proj_lane #(
    .ARENAS      (GEOM_ARENAS),
    .DEPTH       (GEOM_DEPTH),
    .GEN_W       (GEOM_GEN_W),
    .PAYLOAD_A_W (GEOM_PAY_A_W),
    .PAYLOAD_W   (GEOM_PAYLOAD_W)
  ) u_geom_proj_lane (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    // REAL: client A's result port, exactly as the service presents it.
    .a_valid_i  (pj_a_valid),
    .a_x_i      (pj_a_x),
    .a_y_i      (pj_a_y),
    .a_d_i      (pj_a_d),
    .a_w_i      (pj_a_w),
    .a_behind_i (pj_a_behind),
    .a_payload_i(pj_a_payload),

    // REAL: the rider layout lives in the lane; the sequencer hands it the
    // {arena, index} and takes the packed word back.
    .rider_arena_i  (gs_rider_arena),
    .rider_index_i  (gs_rider_index),
    .rider_payload_o(ln_rider_payload),

    // REAL: the geometry arena's lifetime, driven by the sequencer.
    .open_i      (gs_open),
    .open_arena_i(gs_open_arena),
    .open_gen_o  (ln_open_gen),
    .seal_i      (gs_seal),
    .seal_arena_i(gs_seal_arena),

    // I12: the arena origin and the lookup port; same absent customer.
    .org_we_i   (geom_org_we_i),
    .org_arena_i(geom_org_arena_i),
    .org_x_i    (geom_org_x_i),
    .org_y_i    (geom_org_y_i),
    .org_z_i    (geom_org_z_i),

    // REAL: landings back to the sequencer.
    .fill_landed_o(ln_fill_landed),
    .fill_arena_o (ln_fill_arena),

    .look_valid_i(geom_look_valid_i),
    .look_ready_o(geom_look_ready_o),
    .look_arena_i(geom_look_arena_i),
    .look_gen_i  (geom_look_gen_i),
    .look_index_i(geom_look_index_i),
    .rep_valid_o (geom_rep_valid_o),
    .rep_hit_o   (geom_rep_hit_o),
    .rep_refuse_o(geom_rep_refuse_o),
    .rep_payload_o(geom_rep_payload_o),
    .rep_org_x_o (geom_rep_org_x_o),
    .rep_org_y_o (geom_rep_org_y_o),
    .rep_org_z_o (geom_rep_org_z_o),

    .arena_hits_o    (geom_arena_hits_o),
    .arena_misses_o  (geom_arena_misses_o),
    .arena_refusals_o(geom_arena_refusals_o),
    .arena_overflow_o(geom_arena_overflow_o)
  );

  // ==========================================================================
  // COMPOSITOR. On the real frame edge and the real video mode (glue 1 and 2),
  // and BESIDE the render path rather than in it -- header entry I15 says why.
  // ==========================================================================
  zhao_post_composite #(
    .LINE_W    (POST_LINE_W),
    .MAX_H     (POST_MAX_H),
    .NLINE     (POST_NLINE),
    .LAG_LINES (POST_LAG_LINES),
    .LAG_PX    (POST_LAG_PX)
  ) u_post_composite (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    // REAL: the shell's frame boundary and its latched mode.
    .frame_start_i(core_tick_c),
    .frame_w_i    (post_frame_w_c),
    .frame_h_i    (post_frame_h_c),
    .view_sel_i   (post_view_sel_i),

    // I15: RASTER.RESOLVE's stream is internal to zhao_geom_bin_pipe_v2.
    .s_valid_i(post_s_valid_i),
    .s_ready_o(post_s_ready_o),
    .s_rgb_i  (post_s_rgb_i),

    // I17: TWOD.PLANE and the HUD source are not built.
    .gd_req_v_o  (post_gd_req_v_o),
    .gd_view_o   (post_gd_view_o),
    .gd_cx_o     (post_gd_cx_o),
    .gd_cy_o     (post_gd_cy_o),
    .gd_present_i(post_gd_present_i),
    .gd_dx_i     (post_gd_dx_i),
    .gd_dy_i     (post_gd_dy_i),
    .gg_req_v_o  (post_gg_req_v_o),
    .gg_view_o   (post_gg_view_o),
    .gg_cx_o     (post_gg_cx_o),
    .gg_cy_o     (post_gg_cy_o),
    .gg_present_i(post_gg_present_i),
    .gg_glow_i   (post_gg_glow_i),
    .gg_ink_i    (post_gg_ink_i),
    .atm_req_v_o (post_atm_req_v_o),
    .atm_req_x_o (post_atm_req_x_o),
    .atm_req_y_o (post_atm_req_y_o),
    .atm_en_i    (post_atm_en_i),
    .atm_valid_i (post_atm_valid_i),
    .atm_rgb_i   (post_atm_rgb_i),
    .atm_opacity_i(post_atm_opacity_i),
    .atm_add_i   (post_atm_add_i),
    .bloom_gain_i(post_bloom_gain_i),
    .grade_valid_i(post_grade_valid_i),
    .pv_we_i     (post_pv_we_i),
    .pv_sel_i    (post_pv_sel_i),
    .pv_addr_i   (post_pv_addr_i),
    .pv_data_i   (post_pv_data_i),
    .bias_r_i    (post_bias_r_i),
    .bias_g_i    (post_bias_g_i),
    .bias_b_i    (post_bias_b_i),
    .flash_rgb_i (post_flash_rgb_i),
    .flash_amt_i (post_flash_amt_i),
    .ink_rgb_i   (post_ink_rgb_i),
    .hud_req_v_o (post_hud_req_v_o),
    .hud_req_x_o (post_hud_req_x_o),
    .hud_req_y_o (post_hud_req_y_o),
    .hud_valid_i (post_hud_valid_i),
    .hud_rgb_i   (post_hud_rgb_i),

    // I16: the framebuffer writer inside the shell is already fed by
    // RASTER.FBWRITE, so this output has no consumer here.
    .o_valid_o(post_o_valid_o),
    .o_ready_i(post_o_ready_i),
    .o_rgb_o  (post_o_rgb_o),
    .o_x_o    (post_o_x_o),
    .o_y_o    (post_o_y_o),
    .o_last_o (post_o_last_o),
    .echo_valid_o(post_echo_valid_o),
    .echo_rgb_o  (post_echo_rgb_o),

    .displacement_edge_clamps_o(post_displacement_edge_clamps_o),
    .bloom_cells_contributing_o(post_bloom_cells_contributing_o),
    .passes_completed_o        (post_passes_completed_o),
    .grading_table_missing_o   (post_grading_table_missing_o),
    .plane_missing_o           (post_plane_missing_o),
    .line_fill_writes_o        (post_line_fill_writes_o),
    .output_writes_o           (post_output_writes_o),
    .plane_reads_o             (post_plane_reads_o),
    .ring_hazard_o             (post_ring_hazard_o)
  );

  // ==========================================================================
  // MEASURE. The INTERVAL is the console's real frame (glue 2). The EVENTS are
  // not: nothing here produces an error magnitude -- header entry I18.
  // ==========================================================================
  zhao_measure_histogram #(
    .EW       (HIST_EW),
    .SUB_BITS (HIST_SUB_BITS),
    .LANES    (HIST_LANES),
    .CW       (HIST_CW)
  ) u_measure_histogram (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    // I18: no error-event producer exists in this console.
    .ev_valid_i     (hist_ev_valid_i),
    .ev_lane_valid_i(hist_ev_lane_valid_i),
    .ev_err_i       (hist_ev_err_i),
    .ev_src_id_i    (hist_ev_src_id_i),
    .ev_ready_o     (hist_ev_ready_o),

    // REAL: one measurement interval per console frame.
    .snapshot_i(core_tick_c),

    // I19: no HPS register path to this block.
    .rd_valid_i     (hist_rd_valid_i),
    .rd_bin_i       (hist_rd_bin_i),
    .rd_ready_o     (hist_rd_ready_o),
    .rd_data_valid_o(hist_rd_data_valid_o),
    .rd_count_o     (hist_rd_count_o),

    .snap_valid_o (hist_snap_valid_o),
    .snap_total_o (hist_snap_total_o),
    .snap_src_id_o(hist_snap_src_id_o),
    .snap_index_o (hist_snap_index_o),

    .events_o       (hist_events_o),
    .updates_o      (hist_updates_o),
    .stall_cycles_o (hist_stall_cycles_o),
    .bin_sat_o      (hist_bin_sat_o),
    .fwd_hits_o     (hist_fwd_hits_o),
    .host_conflict_o(hist_host_conflict_o),
    .snapshots_o    (hist_snapshots_o),
    .frozen_write_o (hist_frozen_write_o)
  );

  // ==========================================================================
  // THE SHELL. Every port straight through; nothing renamed, nothing
  // reinterpreted. Its own header is the authority on its seams.
  // ==========================================================================
  zhao_shell_top_v2 #(
    .FRAMER_Q (FRAMER_Q),
    .WFIFO_W  (WFIFO_W)
  ) u_shell (
    .gpu_clk                   (gpu_clk),
    .vid_clk                   (vid_clk),
    .audio_clk                 (audio_clk),
    .rst_n                     (rst_n),
    .cfg_valid_i               (cfg_valid_i),
    .cfg_ready_o               (cfg_ready_o),
    .cfg_op_i                  (cfg_op_i),
    .cfg_page_generation_i     (cfg_page_generation_i),
    .cfg_selector_i            (cfg_selector_i),
    .cfg_row_i                 (cfg_row_i),
    .cfg_crc32_i               (cfg_crc32_i),
    .cfg_rsp_valid_o           (cfg_rsp_valid_o),
    .cfg_rsp_ready_i           (cfg_rsp_ready_i),
    .cfg_rsp_op_o              (cfg_rsp_op_o),
    .cfg_rsp_status_o          (cfg_rsp_status_o),
    .cfg_rsp_page_generation_o (cfg_rsp_page_generation_o),
    .active_page_generation_o  (active_page_generation_o),
    .pal_load_valid_i          (pal_load_valid_i),
    .pal_load_ready_o          (pal_load_ready_o),
    .pal_load_op_i             (pal_load_op_i),
    .pal_load_slot_i           (pal_load_slot_i),
    .pal_load_gen_i            (pal_load_gen_i),
    .pal_load_idx_i            (pal_load_idx_i),
    .pal_load_rgb565_i         (pal_load_rgb565_i),
    .pal_load_crc_ok_i         (pal_load_crc_ok_i),
    .tri_area2_i               (tri_area2_i),
    .tri_invw_plane_i          (tri_invw_plane_i),
    .tri_u_over_w_plane_i      (tri_u_over_w_plane_i),
    .tri_v_over_w_plane_i      (tri_v_over_w_plane_i),
    .tri_flat_request_i        (tri_flat_request_i),
    .tri_continuation_tail_i   (tri_continuation_tail_i),
    .tri_fragment_state_i      (tri_fragment_state_i),
    .fill_req_ready_i          (fill_req_ready_i),
    .fill_req_valid_o          (fill_req_valid_o),
    .fill_req_addr_o           (fill_req_addr_o),
    .fill_data_valid_i         (fill_data_valid_i),
    .fill_data_i               (fill_data_i),
    .fill_refused_i            (fill_refused_i),
    .frame_clear_word_i        (frame_clear_word_i),
    .sheet_req_ready_i         (sheet_req_ready_i),
    .sheet_req_valid_o         (sheet_req_valid_o),
    .sheet_req_op_o            (sheet_req_op_o),
    .sheet_req_handle_o        (sheet_req_handle_o),
    .sheet_req_texel_o         (sheet_req_texel_o),
    .sheet_req_src_id_o        (sheet_req_src_id_o),
    .blank_cmd_i               (blank_cmd_i),
    .scanout_ack_i             (scanout_ack_i),
    .frame_swap_valid_i        (frame_swap_valid_i),
    .frame_swap_slot_i         (frame_swap_slot_i),
    .blank_ack_o               (blank_ack_o),
    .blank_active_o            (blank_active_o),
    .lease_open_o              (lease_open_o),
    .frame_slot_ready_o        (frame_slot_ready_o),
    .v2_requests_accepted_o    (v2_requests_accepted_o),
    .v2_responses_accepted_o   (v2_responses_accepted_o),
    .v2_leases_granted_o       (v2_leases_granted_o),
    .v2_leases_refused_o       (v2_leases_refused_o),
    .v2_faults_latched_o       (v2_faults_latched_o),
    .v2_publications_o         (v2_publications_o),
    .v2_releases_o             (v2_releases_o),
    .v2_ready_events_o         (v2_ready_events_o),
    .v2_swaps_o                (v2_swaps_o),
    .v2_contentions_o          (v2_contentions_o),
    .v2_clear_handshakes_o     (v2_clear_handshakes_o),
    .v2_frames_admitted_o      (v2_frames_admitted_o),
    .v2_blit_leases_acquired_o (v2_blit_leases_acquired_o),
    .v2_blit_leases_refused_o  (v2_blit_leases_refused_o),
    .hps_state_i               (hps_state_i),
    .hps_byte_len_i            (hps_byte_len_i),
    .ring_wr_valid_o           (ring_wr_valid_o),
    .ring_wr_slot_o            (ring_wr_slot_o),
    .ring_wr_state_o           (ring_wr_state_o),
    .ring_wr_ready_i           (ring_wr_ready_i),
    .hps_req_valid_o           (hps_req_valid_o),
    .hps_req_write_o           (hps_req_write_o),
    .hps_req_addr_o            (hps_req_addr_o),
    .hps_req_len_o             (hps_req_len_o),
    .hps_req_grant_i           (hps_req_grant_i),
    .hps_wr_valid_o            (hps_wr_valid_o),
    .hps_wr_data_o             (hps_wr_data_o),
    .hps_wr_last_o             (hps_wr_last_o),
    .hps_rd_valid_i            (hps_rd_valid_i),
    .hps_rd_data_i             (hps_rd_data_i),
    .hps_rd_last_i             (hps_rd_last_i),
    .pad_present_i             (pad_present_i),
    .pad_buttons_i             (pad_buttons_i),
    .pad_lx_i                  (pad_lx_i),
    .pad_ly_i                  (pad_ly_i),
    .pad_rx_i                  (pad_rx_i),
    .pad_ry_i                  (pad_ry_i),
    .aud_wr_valid_i            (aud_wr_valid_i),
    .aud_wr_l_i                (aud_wr_l_i),
    .aud_wr_r_i                (aud_wr_r_i),
    .aud_wr_ready_o            (aud_wr_ready_o),
    .aud_refill_req_o          (aud_refill_req_o),
    .aud_occupancy_o           (aud_occupancy_o),
    .pcm_valid_o               (pcm_valid_o),
    .pcm_l_o                   (pcm_l_o),
    .pcm_r_o                   (pcm_r_o),
    .underrun_status_o         (underrun_status_o),
    .audio_underruns_o         (audio_underruns_o),
    .px_valid_o                (px_valid_o),
    .px_rgb_o                  (px_rgb_o),
    .px_x_o                    (px_x_o),
    .px_y_o                    (px_y_o),
    .px_hsync_o                (px_hsync_o),
    .px_vsync_o                (px_vsync_o),
    .px_hblank_o               (px_hblank_o),
    .px_vblank_o               (px_vblank_o),
    .scaler_violation_o        (scaler_violation_o),
    .crc_frame_o               (crc_frame_o),
    .crc_valid_o               (crc_valid_o),
    .crc_bytes_o               (crc_bytes_o),
    .crc_size_err_o            (crc_size_err_o),
    .gpu_tick_o                (gpu_tick_o),
    .gpu_tick_frame_id_o       (gpu_tick_frame_id_o),
    .gpu_tick_repeated_o       (gpu_tick_repeated_o),
    .gpu_complete_slot_o       (gpu_complete_slot_o),
    .deadline_faults_o         (deadline_faults_o),
    .frame_cycles_o            (frame_cycles_o),
    .slot_state_o              (slot_state_o),
    .fence_valid_o             (fence_valid_o),
    .fence_slot_o              (fence_slot_o),
    .fence_ok_o                (fence_ok_o),
    .fence_status_o            (fence_status_o),
    .mode_act_o                (mode_act_o),
    .dma_done_o                (dma_done_o),
    .dma_status_o              (dma_status_o),
    .blit_done_o               (blit_done_o),
    .blit_status_o             (blit_status_o),
    .pad_frame_flat_o          (pad_frame_flat_o),
    .pad_sequence_o            (pad_sequence_o),
    .input_gaps_o              (input_gaps_o),
    .rumble_duty_o             (rumble_duty_o),
    .rumble_active_o           (rumble_active_o),
    .rumble_pwm_o              (rumble_pwm_o),
    .rumble_drops_o            (rumble_drops_o),
    .cnt_snap_ready_i          (cnt_snap_ready_i),
    .cnt_snap_valid_o          (cnt_snap_valid_o),
    .cnt_snap_id_o             (cnt_snap_id_o),
    .cnt_snap_value_o          (cnt_snap_value_o),
    .cnt_window_open_o         (cnt_window_open_o),
    .cnt_cat_violation_o       (cnt_cat_violation_o),
    .guard_violations_o        (guard_violations_o),
    .starvation_o              (starvation_o),
    .init_done_o               (init_done_o),
    .refresh_stalls_o          (refresh_stalls_o),
    .bank_conflicts_o          (bank_conflicts_o),
    .scanout_preempted_o       (scanout_preempted_o),
    .hps_err_count_o           (hps_err_count_o),
    .shell_err_wfifo_o         (shell_err_wfifo_o),
    .shell_err_route_o         (shell_err_route_o),
    .shell_err_cdc_o           (shell_err_cdc_o),
    .shell_err_framer_o        (shell_err_framer_o),
    .render_frame_begin_i      (render_frame_begin_i),
    .render_frame_end_i        (render_frame_end_i),
    .render_grid_w_i           (render_grid_w_i),
    .render_grid_h_i           (render_grid_h_i),
    .render_tri_valid_i        (render_tri_valid_i),
    .render_tri_ready_o        (render_tri_ready_o),
    .geom_guard_req_i          (geom_guard_req_i),
    .geom_guard_rsp_o          (geom_guard_rsp_o),
    .geom_beat_valid_o         (geom_beat_valid_o),
    .geom_beat_data_o          (geom_beat_data_o),
    .geom_beat_last_o          (geom_beat_last_o),
    .render_kx0_i              (render_kx0_i),
    .render_ky0_i              (render_ky0_i),
    .render_kc0_i              (render_kc0_i),
    .render_kx1_i              (render_kx1_i),
    .render_ky1_i              (render_ky1_i),
    .render_kc1_i              (render_kc1_i),
    .render_kx2_i              (render_kx2_i),
    .render_ky2_i              (render_ky2_i),
    .render_kc2_i              (render_kc2_i),
    .render_tl_i               (render_tl_i),
    .render_ax_i               (render_ax_i),
    .render_ay_i               (render_ay_i),
    .render_bx_i               (render_bx_i),
    .render_by_i               (render_by_i),
    .render_cx_i               (render_cx_i),
    .render_cy_i               (render_cy_i),
    .render_min_x_i            (render_min_x_i),
    .render_max_x_i            (render_max_x_i),
    .render_min_y_i            (render_min_y_i),
    .render_max_y_i            (render_max_y_i),
    .render_src_id_i           (render_src_id_i),
    .render_fill_word_i        (render_fill_word_i),
    .render_clear_word_i       (render_clear_word_i),
    .render_state_i            (render_state_i),
    .render_src_a_i            (render_src_a_i),
    .render_texel_rgb_i        (render_texel_rgb_i),
    .render_texel_a_i          (render_texel_a_i),
    .render_texel_idx_i        (render_texel_idx_i),
    .render_fb_base_i          (render_fb_base_i),
    .render_fb_stride_i        (render_fb_stride_i),
    .fb_writer_i               (fb_writer_i),
    .render_drain_done_o       (render_drain_done_o),
    .render_busy_o             (render_busy_o),
    .render_pixels_o           (render_pixels_o),
    .render_bursts_o           (render_bursts_o),
    .render_stream_error_o     (render_stream_error_o),
    .render_drained_o          (render_drained_o),
    .render_fatal_o            (render_fatal_o),
    .render_issued_words_o     (render_issued_words_o),
    .render_retired_words_o    (render_retired_words_o),
    .render_overflow_o         (render_overflow_o),
    .render_fragment_error_o   (render_fragment_error_o),
    .phy_cs_n_o                (phy_cs_n_o),
    .phy_ras_n_o               (phy_ras_n_o),
    .phy_cas_n_o               (phy_cas_n_o),
    .phy_we_n_o                (phy_we_n_o),
    .phy_a_o                   (phy_a_o),
    .phy_ba_o                  (phy_ba_o),
    .phy_dq_o                  (phy_dq_o),
    .phy_dq_oe_o               (phy_dq_oe_o),
    .phy_dqm_o                 (phy_dqm_o),
    .phy_dq_i                  (phy_dq_i)
  );

endmodule : zhao_console_core
