// zhao_console_core_slot_overflow_mutant.sv -- the positive control for
// `terr_pl_slot_overflow_o`, which no legal stimulus can fire.
//
// ---------------------------------------------------------------------------
// WHY THIS FILE EXISTS
// ---------------------------------------------------------------------------
// `zhao_console_core` counts completions from TERRAIN.PAGELOADER whose pool
// slot has the pool's EXTRA REFUSAL BIT set:
//
//     wire tpl_fin_over = tpl_fin_slot_w[TERR_MEMSLOT-1];
//
// TERR_MEMSLOT is $clog2(TERR_POOL_SLOTS)+1 = 11 and the producer of the job
// slot is a TERR_SLOTW = 10 bit directory handle zero-extended at the port
// (`.j_slot_i({1'b0, tlq_q_slot})`). Bit 10 is therefore STRUCTURALLY ZERO and
// the counter cannot move under any legal stimulus -- the `wq_overflow_o` shape
// CLAUDE.md describes. A counter asserted zero and never seen to move is a
// claim, and it is the claim to check hardest.
//
// ---------------------------------------------------------------------------
// IT IS A WRAPPER, NOT A COPY -- AND THAT IS THE WHOLE POINT
// ---------------------------------------------------------------------------
// CLAUDE.md: "a wrapper that instantiates the production module cannot drift,
// and must not be counted as a copy". A mutant COPY of a 4,000-line composer
// would be stale the next time anything in it moved, and would then go on
// passing. There is no copied BODY here: everything below the port list is one
// instantiation of `zhao_console_core`, with every parameter passed by name.
//
// THE ONE SUBSTANTIVE CHANGE:   TERR_POOL_SLOTS = 1024  ->  512
//
// That is legal to write because TERR_POOL_SLOTS is a real design knob -- the
// size of TERRAIN.PAGE_POOL -- and shrinking it is what makes the guarded state
// reachable, for a reason that is arithmetic rather than lucky:
//
//   * TERR_MEMSLOT becomes $clog2(512)+1 = 10, so `tpl_fin_over` now reads
//     bit 9 of the completion's slot -- a bit a REAL directory handle carries;
//   * the directory is still 256 sets x 4 ways = 1,024 slots, so it still hands
//     out slots above 511;
//   * TERRAIN.PAGELOADER's own `pre_slot_bad` (slot >= REGION_SLOTS) refuses
//     such a job, and `S_CHECK` publishes `fin_slot_o <= job_slot` on the
//     refusal completion -- so the offending index reaches the counter.
//
// MEASURED, NOT HOPED FOR. Under `tests/prod/run_console_core_smoke.ps1`'s
// stimulus the three completions carry slots 592, 212 and 216 (592 =
// 0b01001010000). One of them has bit 9 set, so the mutant fires EXACTLY ONCE
// and does so deterministically -- the slot is a function of the record key,
// not of timing. If TERRAIN.RESIDENCY's set index ever changes shape the mutant
// goes RED rather than quietly green, which is the correct failure direction
// for an inverted-polarity control.
//
// THE PORT LIST IS GENERATED FROM PRODUCTION'S, VERBATIM. Regenerate it if
// `zhao_console_core`'s parameter or port block changes:
//
//   the header + parameter block + port block of fpga/rtl/prod/zhao_console_core.sv
//   (module line through the closing `);`), with the module renamed and the one
//   TERR_POOL_SLOTS line edited.
//
// It cannot go stale SILENTLY: `.*` binds every production port to a wrapper
// port of the same name, so a port gained or lost in production fails to
// elaborate here and says so. That is the opposite of a stale copy, which fails
// by continuing to pass.
//
// THE DRIVER IS tests/prod/run_console_core_smoke.ps1 -Mutant, and its polarity
// is INVERTED: it passes when `terr_pl_slot_overflow_o` is NON-ZERO and fails
// when it reads 0. It is evidence about the instrument, not about the design.
// The same script without -Mutant is the negative control: the identical
// stimulus against unmutated production must read 0.
//
// No simulation assertion is disabled here; the wrapper contains none, and the
// composer's own elaboration guards are unaffected by the mutation.
// ---------------------------------------------------------------------------
`default_nettype none
module zhao_console_core_slot_overflow_mutant
  import zhao_pkg::*, zhao_abi_pkg::*, zhao_fb_tuple_pkg::*;
#(
  // ---- the shell's own knobs, carried through ------------------------------
  parameter int unsigned FRAMER_Q = 8,
  parameter int unsigned WFIFO_W  = 64,

  // ---- CMD.EXEC (section 7c) -----------------------------------------------
  // How many SurfaceStamps one packet may carry. It is the ONE number in the
  // executor that can refuse an otherwise legal packet, so it is a knob and not
  // a constant: a packet with more stamps than this is refused WHOLE and
  // counted on `cmd_exec_stamp_overflow_o`, never applied in part. Eight is
  // chosen against the sheet, not against the ABI -- each stamp walks 4,096
  // texels, so a frame that wants more than eight is asking the surface stage
  // for more work than a frame has.
  parameter int unsigned CMD_EXEC_STAMP_Q = 8,

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
  // PART.TABLE's load bus. DERIVED, NOT A KNOB, and restated here for the same
  // reason `zhao_part_table` carries it in its own parameter list: a PORT WIDTH
  // CANNOT REFER TO A BODY LOCALPARAM. It is the widest descriptor slice --
  // recipe 4 + lifetime + age_mark + drag 8 + grav + strength + cx,cy,cz +
  // p0,p1,p2 -- and the table's own elaboration guard $fatals if this does not
  // equal its UPD_W, so a divergence between these two expressions is loud at
  // elaboration rather than a silently truncated descriptor. It is NOT
  // arithmetic invented here: it is the table's own expression, copied with its
  // owner named, and the guard is what makes the copy safe.
  parameter int unsigned PART_TBL_LD_W = 12 + (2 * PART_AGE_W) + (5 * PART_VEL_W)
                                            + (3 * PART_POS_W),

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

  // ---- GEOMETRY: the asset fetch path -------------------------------------
  // ONE pair of limits for TWO blocks, written once for the same reason
  // TWOD_LINE_W is: GEOM.ASSETFETCH sizes the private buffer it fills from
  // these, and GEOM.ASSEMBLE decides which local index is legal against the
  // same numbers. Two blocks disagreeing about how big a meshlet may be is a
  // walk off the end of a buffer that every handshake calls legal, so they
  // are one expression rather than two literals. Both are the owning blocks'
  // own defaults (GEOM.MESHFETCH.md's ruling limits: a u8 local index cannot
  // address past 255, and 126 triangles x 3 indices is 378 bytes).
  parameter int unsigned GEOM_ASSET_MAX_VERTICES  = 64,
  parameter int unsigned GEOM_ASSET_MAX_TRIANGLES = 126,
  // GEOM.ASSEMBLE's vertex-id width. 16 is NOT a free choice and is named
  // here so it reads as the constraint it is: it is GEOM.PARAMBUF's
  // TriangleDescriptor field (`vertex_id[3] u16`, ruling R7), so widening it
  // would emit a descriptor the record layer cannot store.
  parameter int unsigned GEOM_ASM_VIDW = 16,

  // ---- GEOMETRY: the clip/setup triangle front door ------------------------
  // `zhao_geom_clip`'s ruling-5 attribute packet: invw24, u_over_w, v_over_w,
  // lit r/g/b and alpha. The block never interprets them; it only keeps them
  // with their vertices across the winding flip. ATTRW is the flattened width
  // and exists because a port list cannot call $clog2 on another port.
  parameter int unsigned GEOM_CLIP_ATTRS = 7,
  parameter int unsigned GEOM_CLIP_ATTRW = GEOM_CLIP_ATTRS * 32,

  // ---- GEOMETRY: the client-B/terrain side of the same projector ----------
  parameter int unsigned PROJ_T_ARENAS = 4,
  parameter int unsigned PROJ_T_DEPTH  = 81,
  parameter int unsigned PROJ_T_INDEX_W= $clog2(PROJ_T_DEPTH) + 1,
  parameter int unsigned PROJ_T_ARENA_W= $clog2(PROJ_T_ARENAS) + 1,
  // TERRAIN.TESS's own window index: 81 vertices need 7 bits. The sequencer
  // widens it to PROJ_T_INDEX_W, which carries a refusal bit beside it.
  parameter int unsigned PROJ_T_IDX_W  = 7,

  // ---- COMPOSITOR ----------------------------------------------------------
  parameter int unsigned POST_LINE_W   = 384,     // Z60 is the widest view
  parameter int unsigned POST_MAX_H    = 240,
  parameter int unsigned POST_NLINE    = 9,
  parameter int unsigned POST_LAG_LINES= 4,
  parameter int unsigned POST_LAG_PX   = 9,
  parameter int unsigned POST_XW       = $clog2(POST_LINE_W + 1),
  parameter int unsigned POST_YW       = $clog2(POST_MAX_H + 1),

  // ---- TWOD: the plane, the sprite walker and the sampler between them -----
  // TWOD_LINE_W and TWOD_MAX_H are NOT separate numbers -- they are
  // POST_LINE_W and POST_MAX_H, because the ring the sampler prepares is
  // addressed by the compositor's own raster pointer and two blocks that
  // disagree about what a line is would produce a picture sheared by the
  // difference. They are written as expressions rather than repeated literals
  // so the agreement cannot be broken by editing one of them.
  parameter int unsigned TWOD_PAGE_WORDS = 8192,   // 16 KiB of texel page
  parameter int unsigned TWOD_PAL_SLOTS  = 4,
  parameter int unsigned TWOD_BIND_SLOTS = 8,
  parameter int unsigned TWOD_ATM_LINES  = 4,
  parameter int unsigned TWOD_PAW        = $clog2(TWOD_PAGE_WORDS),
  parameter int unsigned TWOD_PALAW      = $clog2(TWOD_PAL_SLOTS * 256),
  parameter int unsigned TWOD_BSW        = $clog2(TWOD_BIND_SLOTS),

  // ---- MEASURE -------------------------------------------------------------
  parameter int unsigned HIST_EW       = 32,
  parameter int unsigned HIST_SUB_BITS = 1,
  parameter int unsigned HIST_LANES    = 4,
  parameter int unsigned HIST_CW       = 24,
  parameter int unsigned HIST_BINW     = $clog2((HIST_EW - HIST_SUB_BITS + 1) << HIST_SUB_BITS),

  // ---- SURFACE: the scar substrate and the engine that writes it -----------
  // Both are the owning block's own default, named here so the day one moves
  // the thing that has to move with it is greppable, and so the owner keeps
  // control of a value that is a measured frontier rather than a law.
  //   SURF_SLOTS   -- resident 64x64 sheets. Each slot is 65,536 bits (about
  //                   seven M10K), so this is SURFACE.SHEET's whole memory
  //                   bill and the block's own header asks the first fit to
  //                   retune it.
  //   SURF_SQ_RADIX-- SURFACE.STAMP's squarer radix. All three settings meet
  //                   the 20,000 texel/frame demand; the wall is Fmax on a
  //                   SHARED gpu_clk, which is exactly why 1 is the default
  //                   and this is a knob rather than a constant.
  parameter int unsigned SURF_SLOTS    = 2,
  parameter int unsigned SURF_SQ_RADIX = 1,

  // ---- TERRAIN: the paging spine (CMD -> SEQ -> RESIDENCY/LOADQ -> LOADER) --
  // Every one of these is the value the block that owns it already defaults to;
  // they are named here rather than left implicit so the day one moves, the
  // thing that has to move with it is greppable. TERR_SLOTW is DERIVED from the
  // directory's own geometry and must not be set independently: it is the width
  // of a {set, way} handle and $clog2(SETS*WAYS) is what the directory emits.
  parameter int unsigned TERR_SETS     = 256,   // ruling T9/T10: 256 sets...
  parameter int unsigned TERR_WAYS     = 4,     //   ...x 4 ways = 1,024 slots
  parameter int unsigned TERR_SLOTW    = $clog2(TERR_SETS * TERR_WAYS),
  parameter int unsigned TERR_GENW     = 8,     // T10: "generation u8 minimum"
  parameter int unsigned TERR_SEQW     = 16,    // the loader claim sequence
  parameter int unsigned TERR_PINW     = 6,     // 63 concurrent pins on a page
  parameter int unsigned TERR_CSLOTS   = 256,   // T6's composed height cache
  parameter int unsigned TERR_LOADQ_D  = 32,    // T7's per-frame page budget

  // TERRAIN.PAGE_POOL (ruling T2). The pool can move to any unmapped range, so
  // the base and the slot count are knobs and every width below is derived from
  // them rather than restated. TERR_MEMSLOT is ONE BIT WIDER than the pool needs
  // and that extra bit is load-bearing -- see the width note at the pageloader
  // instance, which is the one place in this file the step is crossed.
  parameter logic [ZHAO_VRAM_ADDR_BITS-1:0] TERR_POOL_BASE = 27'h400_0000,
  parameter int unsigned TERR_POOL_SLOTS = 512,  // <<< THE MUTATION, and the only one
  parameter int unsigned TERR_MEMSLOT     = $clog2(TERR_POOL_SLOTS) + 1,
  parameter int unsigned TERR_PAGE_BYTES  = 21376  // terrain_rules sec 2 / sec 7
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

  // ---- I33: PART.TABLE's PER-FRAME LOAD -----------------------------------
  // I2 and I3 ARE CLOSED and their twenty-five ports are GONE from this list
  // rather than driven -- `zhao_part_table` is instantiated below and answers
  // all four reads inside this module. What is left is the host that fills it,
  // and this is that seam. One word per clock; the table never refuses for
  // backpressure (`ld_ready_o` is constant high and says so in its own file).
  input  logic                    part_tbl_ld_valid_i,
  output logic                    part_tbl_ld_ready_o,
  input  logic [1:0]              part_tbl_ld_sel_i,
  input  logic [6:0]              part_tbl_ld_index_i,
  input  logic [1:0]              part_tbl_ld_event_i,
  input  logic [PART_TBL_LD_W-1:0] part_tbl_ld_data_i,

  // ---- I5: the bounded FIELD/FLOW acceleration sample ---------------------
  input  logic                    part_fld_valid_i,
  input  logic signed [10:0]      part_fld_ax_i,
  input  logic signed [10:0]      part_fld_ay_i,
  input  logic signed [10:0]      part_fld_az_i,

  // (I2's PART.COLLIDE slice -- `part_col_d_response_i` and the three
  //  coefficients -- was here. CLOSED: PART.TABLE serves it below, addressed by
  //  PART.COLLIDE's own new `d_index_o`.)

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

  // (I2's PART.SPAWN slice was here. CLOSED: PART.TABLE serves it below.)

  // ---- PART.TABLE's own evidence (I33's other half) -----------------------
  // Five counters, out at the boundary like every other particle counter, so a
  // bench can say WHICH slice a load landed in and a silent load path is
  // visible rather than inferred from a descriptor read that happens to work.
  // `part_tbl_load_refused_o` is STRUCTURALLY UNREACHABLE at PART_SPECIES_N =
  // 128 and PART.TABLE's CRV_N = 16 -- seven index bits cannot address outside
  // a 128-entry table -- so its zero here is arithmetic, not a measurement.
  // The reachable case is proven at SPECIES_N = 8 by
  // `tests/particles/part_table_directed.cpp`, which is where the refusal has a
  // positive control. Quoting this port's zero as evidence of anything would be
  // the broken-instrument law.
  output logic [31:0]             part_tbl_loads_update_o,
  output logic [31:0]             part_tbl_loads_collide_o,
  output logic [31:0]             part_tbl_loads_spawn_o,
  output logic [31:0]             part_tbl_loads_curve_o,
  output logic [31:0]             part_tbl_load_refused_o,

  // ---- I8: the capacity backstop ------------------------------------------
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
  // WAS structurally stuck at zero (old header entry I4). Since the ruling of
  // 2026-09-19 it is driven by PART.COLLIDE's `collision_events_o` and moves.
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

  // ---- THE GEOMETRY ASSET PATH (connected item 11) -------------------------
  // I23's four ports -- `geom_vd_v_valid_i`, `geom_vd_v_ready_o`,
  // `geom_vd_v_bytes_i`, `geom_vd_v_src_id_i` -- LEFT THIS LIST on 2026-09-19
  // rather than being driven: GEOM.ASSETFETCH is composed below and is that
  // port's real producer. What follows is what the path still asks of the
  // outside, and each group is one numbered entry in the header.

  // ---- I36: GEOM.MESHFETCH's DRAW JOB -------------------------------------
  input  logic                    geom_mf_job_valid_i,
  output logic                    geom_mf_job_ready_o,
  input  logic [15:0]             geom_mf_job_instance_id_i,
  input  logic [26:0]             geom_mf_job_desc_addr_i,
  input  logic [7:0]              geom_mf_job_format_i,
  input  logic [15:0]             geom_mf_job_generation_i,
  input  logic [1:0]              geom_mf_job_active_mask_i,
  input  logic signed [31:0]      geom_mf_job_xform_i [0:11],

  // ---- I37: the descriptor's CRC VERDICT ----------------------------------
  input  logic                    geom_mf_crc_ok_i,

  // ---- I38: GEOM.ASSETFETCH's meshlet RELEASE -----------------------------
  input  logic                    geom_af_release_i,

  // ---- I39: GEOM.ASSEMBLE's three descriptor fields -----------------------
  input  logic [GEOM_ASM_VIDW-1:0] geom_asm_vertex_offset_i,
  input  logic [15:0]              geom_asm_material_id_i,
  input  logic [31:0]              geom_asm_raster_state_i,

  // ---- I39: GEOM.ASSEMBLE's TriangleDescriptor, out to the absent replay --
  output logic                     geom_asm_t_valid_o,
  input  logic                     geom_asm_t_ready_i,
  output logic [GEOM_ASM_VIDW-1:0] geom_asm_t_v0_o,
  output logic [GEOM_ASM_VIDW-1:0] geom_asm_t_v1_o,
  output logic [GEOM_ASM_VIDW-1:0] geom_asm_t_v2_o,
  output logic [15:0]              geom_asm_t_material_o,
  output logic [31:0]              geom_asm_t_raster_o,
  output logic [15:0]              geom_asm_t_src_id_o,
  output logic                     geom_asm_t_last_o,

  // ---- the asset path's evidence ------------------------------------------
  // GEOM.MESHFETCH's seven refusal rows are exported SEPARATELY rather than
  // as the block's `refused_o [7]`, in the block's own documented order
  // (format, crc, generation, vertex_count, triangle_count, reserved,
  // zero_bound). One counter for all seven would name none of them, which is
  // that block's own argument for keeping them apart.
  output logic [31:0] geom_mf_meshlets_considered_o,
  output logic [31:0] geom_mf_culled_all_cameras_o,
  output logic [31:0] geom_mf_descriptors_fetched_o,
  output logic [31:0] geom_mf_guard_denied_o,
  output logic [31:0] geom_mf_refused_format_o,
  output logic [31:0] geom_mf_refused_crc_o,
  output logic [31:0] geom_mf_refused_generation_o,
  output logic [31:0] geom_mf_refused_vertex_count_o,
  output logic [31:0] geom_mf_refused_triangle_count_o,
  output logic [31:0] geom_mf_refused_reserved_o,
  output logic [31:0] geom_mf_refused_zero_bound_o,

  // GEOM.MEM_ADAPTER: `geom_ma_contention_o` is the number that says what
  // sharing ONE ENGINE1 client between two fetchers actually costs, and it
  // could not move until both of them were behind it.
  output logic [31:0] geom_ma_jobs_a_o,
  output logic [31:0] geom_ma_jobs_b_o,
  output logic [31:0] geom_ma_denied_o,
  output logic [31:0] geom_ma_contention_o,
  output logic [31:0] geom_ma_err_short_o,
  output logic [31:0] geom_ma_err_long_o,
  output logic [31:0] geom_ma_err_unowned_o,

  output logic [31:0] geom_af_meshlets_fetched_o,
  output logic [31:0] geom_af_beats_read_o,
  output logic [31:0] geom_af_guard_denied_o,
  output logic [31:0] geom_af_refused_footprint_o,
  output logic [31:0] geom_af_prefetch_stall_o,
  output logic [31:0] geom_af_err_beat_truncated_o,
  output logic [31:0] geom_af_err_beat_overrun_o,
  output logic [31:0] geom_af_err_beat_unowned_o,

  output logic [31:0] geom_asm_meshlets_o,
  output logic [31:0] geom_asm_triangles_o,
  output logic [31:0] geom_asm_refused_limits_o,
  output logic [31:0] geom_asm_refused_index_o,

  // ---- GEOM.VDECODE's side-channels and evidence ---------------------------
  // These leave the module because nothing composed here consumes them, and an
  // output left open is an output nobody reads.
  //
  // `geom_vd_bone0_o` / `bone1_o` USED TO BE HERE, described as "the address
  // the absent GEOM.POSE palette store needs". The store is no longer absent
  // (`zhao_geom_pose_palette`, composed below), so the two bone indices are now
  // INTERNAL wires with a real consumer and they have left this port list. The
  // attribute channel below still has none.
  output logic signed [7:0]       geom_vd_d_nx_o,
  output logic signed [7:0]       geom_vd_d_ny_o,
  output logic signed [7:0]       geom_vd_d_nz_o,
  output logic signed [15:0]      geom_vd_d_u_o,
  output logic signed [15:0]      geom_vd_d_v_o,
  output logic                    geom_vd_refused_o,
  output logic                    geom_vd_reserved_nz_o,
  output logic                    geom_vd_w0_illegal_o,
  output logic                    geom_vd_format_bad_o,
  output logic [31:0]             geom_vd_vertices_o,
  output logic [31:0]             geom_vd_reserved_nz_count_o,
  output logic [31:0]             geom_vd_w0_illegal_count_o,
  output logic [31:0]             geom_vd_format_bad_count_o,

  // ---- GEOM.SKIN's evidence -----------------------------------------------
  // I10 IS CLOSED. `geom_skin_a_m_i` / `geom_skin_b_m_i` used to be here, two
  // twelve-element boundary arrays that only a harness had ever driven. They
  // are gone rather than driven: `zhao_geom_pose_palette` below stores the
  // decoded palette and answers the two reads, so GEOM.SKIN's matrix ports are
  // internal and this module's edge is two arrays smaller.
  output logic [15:0]             geom_skin_src_id_o,
  output logic [31:0]             geom_skin_vertices_transformed_o,

  // ---- I29: GEOM.POSE's clip page and skeleton bake ------------------------
  // The palette store closed I10 by giving GEOM.POSE's decoder a consumer; the
  // decoder's own SOURCE is what is now missing, and this is it. See I29.
  input  logic                    geom_pose_start_i,
  input  logic [5:0]              geom_pose_bone_count_i,
  input  logic signed [31:0]      geom_pose_root_dx_i,
  input  logic signed [31:0]      geom_pose_root_dy_i,
  input  logic signed [31:0]      geom_pose_root_dz_i,
  output logic [4:0]              geom_pose_bone_idx_o,
  input  logic [4:0]              geom_pose_bone_parent_i,
  input  logic signed [31:0]      geom_pose_bone_tx_i,
  input  logic signed [31:0]      geom_pose_bone_ty_i,
  input  logic signed [31:0]      geom_pose_bone_tz_i,
  input  logic signed [15:0]      geom_pose_quat_w_i,
  input  logic signed [15:0]      geom_pose_quat_x_i,
  input  logic signed [15:0]      geom_pose_quat_y_i,
  input  logic signed [15:0]      geom_pose_quat_z_i,
  input  logic signed [31:0]      geom_pose_inv_rest_i [0:11],
  output logic                    geom_pose_busy_o,
  output logic                    geom_pose_done_o,
  output logic [31:0]             geom_pose_palettes_decoded_o,

  // ---- GEOM.POSE's palette store: its evidence ----------------------------
  // `geom_pal_bone_unset_o` is the one to watch. It is the store's own report
  // that a vertex named a bone the current palette has not been given, which is
  // what a vertex stream that was not drained before a new decode looks like.
  output logic [31:0]             geom_pal_vertices_served_o,
  output logic [31:0]             geom_pal_bones_written_o,
  output logic [31:0]             geom_pal_bone_oob_o,
  output logic [31:0]             geom_pal_bone_unset_o,

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

  // ---- I24: GEOM.CLIP's projected triangle, attributes and cull mode -------
  // The three SCREEN corners with GEOM.PROJECT's behind verdicts. The absent
  // replay customer specified at I11 is what drives these. The SCISSOR is NOT
  // here because it is real -- see GLUE 1.
  input  logic                    geom_clip_tri_valid_i,
  output logic                    geom_clip_tri_ready_o,
  input  logic signed [20:0]      geom_clip_tri_ax_i,
  input  logic signed [20:0]      geom_clip_tri_ay_i,
  input  logic signed [20:0]      geom_clip_tri_bx_i,
  input  logic signed [20:0]      geom_clip_tri_by_i,
  input  logic signed [20:0]      geom_clip_tri_cx_i,
  input  logic signed [20:0]      geom_clip_tri_cy_i,
  input  logic [2:0]              geom_clip_tri_behind_i,
  input  logic [15:0]             geom_clip_tri_src_id_i,
  input  logic [GEOM_CLIP_ATTRW-1:0] geom_clip_attr_a_i,
  input  logic [GEOM_CLIP_ATTRW-1:0] geom_clip_attr_b_i,
  input  logic [GEOM_CLIP_ATTRW-1:0] geom_clip_attr_c_i,
  input  logic [1:0]              geom_clip_cull_mode_i,

  // ---- GEOM.CLIP / GEOM.SETUP evidence and carried attributes --------------
  // The attributes and the flip leave the module for the same reason I23's
  // side-channels do: GEOM.ATTRSETUP is not composed, and dropping the swapped
  // packets here would lose the one thing GEOM.CLIP does to them.
  output logic [GEOM_CLIP_ATTRW-1:0] geom_clip_attr_a_o,
  output logic [GEOM_CLIP_ATTRW-1:0] geom_clip_attr_b_o,
  output logic [GEOM_CLIP_ATTRW-1:0] geom_clip_attr_c_o,
  output logic                    geom_clip_flip_o,
  output logic                    geom_clip_ret_valid_o,
  output logic [2:0]              geom_clip_ret_verdict_o,
  output logic [31:0]             geom_clip_submitted_o,
  output logic [31:0]             geom_clip_clipped_o,
  output logic [31:0]             geom_clip_culled_o,
  output logic signed [47:0]      geom_setup_area2_o,
  output logic [31:0]             geom_setup_triangles_submitted_o,

  // ---- I14: the shared projector's matrix bank ----------------------------
  input  logic                    proj_cfg_we_i,
  input  logic                    proj_cfg_view_i,
  input  logic [4:0]              proj_cfg_addr_i,
  input  logic [31:0]             proj_cfg_data_i,
  input  logic                    proj_en_i,

  // ---- TERRAIN: the subpatch job that drives client B (I21) ---------------
  // TERRAIN.GROUP_SEQ's own job port. TERRAIN.LOD exists and its header says
  // its output is "EXACTLY zhao_terrain_tess's job port" -- but it emits no
  // view mask and no material riders, and its own `sp_*` producer
  // (TERRAIN.PATCH's patch_state) is not composed here. See entry I21.
  input  logic                    terr_job_valid_i,
  output logic                    terr_job_ready_o,
  input  logic [5:0]              terr_job_ox_i,
  input  logic [5:0]              terr_job_oz_i,
  input  logic [1:0]              terr_job_level_i,
  input  logic [1:0]              terr_job_lvl_nz_i,
  input  logic [1:0]              terr_job_lvl_pz_i,
  input  logic [1:0]              terr_job_lvl_nx_i,
  input  logic [1:0]              terr_job_lvl_px_i,
  input  logic [16:0]             terr_job_morph_i,
  input  logic                    terr_job_surface_i,
  input  logic                    terr_job_dual_i,
  input  logic [15:0]             terr_job_src_id_i,
  input  logic [1:0]              terr_job_view_mask_i,
  input  logic [7:0]              terr_job_mat_a_i,
  input  logic [7:0]              terr_job_mat_b_i,
  input  logic [7:0]              terr_job_weight_i,
  input  logic                    terr_sparse_fill_i,

  // TERRAIN.TESS's lattice and cell-state read ports USED TO BE HERE, as entry
  // I22.  `zhao_terrain_compcache_front` is composed below and its serve side
  // drives them, so the eleven ports are gone from this list rather than being
  // driven by a harness.  The retirement pulse the cache's serve side needs --
  // `terr_cc_serve_release_i` -- is further down with the rest of the compose
  // engine's boundary, because its owner is the block that issues the subpatch
  // jobs (entry I21) and not the cache.

  // ==========================================================================
  // THE TERRAIN PAGING SPINE'S OWN BOUNDARY (composed item 8 in the header)
  // ==========================================================================

  // ---- TERRAIN.CMD's command and its configuration ------------------------
  // NOT a tie-off, and in the same standing as I9 and I25. This is T5's
  // `SubmitTerrainSet`, already unpacked -- a HOST PACKET from SW.STREAM, which
  // the completion plan names in its own list of what a harness may supply:
  // "external clocks, input events, memory behavior and host packets". The
  // arena base/bytes and the live epoch ride the same path and are host state
  // for the same reason. If CMD.SCHEDULER later owns the terrain draw, these
  // become internal and this note goes with them; it is NOT counted as a gap
  // because nothing is missing, the producer is simply outside the console.
  input  logic                    terr_cmd_valid_i,
  output logic                    terr_cmd_ready_o,
  input  logic [31:0]             terr_cmd_epoch_i,
  input  logic [31:0]             terr_cmd_list_off_i,
  input  logic [31:0]             terr_cmd_list_bytes_i,
  input  logic [31:0]             terr_cmd_list_crc_i,
  input  logic [15:0]             terr_cmd_patch_count_i,
  input  logic [31:0]             terr_cmd_sequence_i,
  input  logic [31:0]             terr_cmd_src_id_i,
  output logic                    terr_cmd_done_valid_o,
  input  logic                    terr_cmd_done_ready_i,
  output logic                    terr_cmd_done_ok_o,
  output logic [3:0]              terr_cmd_done_verdict_o,
  output logic [31:0]             terr_cmd_done_src_id_o,
  output logic [31:0]             terr_cmd_done_crc_seen_o,

  input  logic [31:0]             terr_cfg_epoch_i,
  input  logic [31:0]             terr_cfg_arena_base_i,
  input  logic [31:0]             terr_cfg_arena_bytes_i,
  input  logic [15:0]             terr_cfg_load_budget_i,

  // ---- I26: the spine's MEM.HPS.BRIDGE and MEM.GUARD clients --------------
  // ONE bridge port, because the two terrain readers go through the REAL
  // `zhao_hps_arbiter` instantiated below and not through anything invented
  // here. See entry I26 for why the port stops at this module's edge.
  output zhao_hps_burst_req_t     terr_hps_req_o,
  input  logic                    terr_hps_grant_i,
  output logic                    terr_hps_wr_valid_o,
  output logic [63:0]             terr_hps_wr_data_o,
  output logic                    terr_hps_wr_last_o,
  input  zhao_hps_burst_rsp_t     terr_hps_rsp_i,

  output zhao_guard_req_t         terr_guard_req_o,
  input  zhao_guard_rsp_t         terr_guard_rsp_i,
  output logic [63:0]             terr_guard_wdata_o,
  output logic                    terr_guard_wvalid_o,
  input  logic                    terr_guard_wready_i,
  output logic                    terr_guard_wlast_o,

  // ---- I27 (narrowed): the directory's deformation and handle-check ports --
  //      The COMPOSE DOOR (`terr_is_*`) and the UNPIN (`terr_unpin_*`) left this
  //      list on 2026-09-19: TERRAIN.SEQ's issue now reaches TERRAIN.PAGESTREAM
  //      and TERRAIN.PLACE inside this module, and the streamer's own completion
  //      is what unpins the page.  What is left here is the deformation mark,
  //      whose writer is TERRAIN.BAKE (entry I32), and the handle check, whose
  //      caller is the same absent subpatch issuer as entry I21.
  input  logic                    terr_dm_valid_i,
  output logic                    terr_dm_ready_o,
  input  logic [TERR_SLOTW-1:0]   terr_dm_slot_i,
  input  logic [TERR_GENW-1:0]    terr_dm_gen_i,
  input  logic [31:0]             terr_dm_epoch_i,
  input  logic                    terr_dm_bd_i,
  input  logic                    terr_dm_f_i,
  input  logic                    terr_dm_mips_i,

  input  logic                    terr_chk_valid_i,
  input  logic [TERR_SLOTW-1:0]   terr_chk_slot_i,
  input  logic [TERR_GENW-1:0]    terr_chk_gen_i,
  input  logic [31:0]             terr_chk_epoch_i,
  output logic                    terr_chk_valid_o,
  output logic                    terr_chk_stale_o,

  // ---- I28: TERRAIN.SEQ's F-sheet writeback job and its barrier release ---
  output logic                    terr_wb_valid_o,
  input  logic                    terr_wb_ready_i,
  output logic [TERR_SLOTW-1:0]   terr_wb_slot_o,
  output logic [TERR_GENW-1:0]    terr_wb_gen_o,
  output logic [31:0]             terr_wb_epoch_o,
  output logic [31:0]             terr_wb_island_o,
  output logic signed [15:0]      terr_wb_ix_o,
  output logic signed [15:0]      terr_wb_iz_o,
  output logic [31:0]             terr_wb_src_id_o,
  input  logic                    terr_wb_done_valid_i,
  input  logic [TERR_SLOTW-1:0]   terr_wb_done_slot_i,

  // ---- I28 (other end): TERRAIN.RESIDENCY's writeback-ACK barrier --------
  input  logic                    terr_wback_valid_i,
  output logic                    terr_wback_ready_o,
  input  logic [TERR_SLOTW-1:0]   terr_wback_slot_i,
  input  logic [TERR_GENW-1:0]    terr_wback_gen_i,
  input  logic [31:0]             terr_wback_epoch_i,

  // ==========================================================================
  // THE TERRAIN COMPOSE ENGINE'S OWN BOUNDARY (connected item 10)
  // ==========================================================================

  // ---- I26 (extended): TERRAIN.PAGESTREAM's MEM.GUARD READ client ---------
  // A SECOND guard client, not a second opinion about the first.
  // TERRAIN.PAGELOADER's client above WRITES a page into the pool; this one
  // READS the same page back out, so it needs the return path
  // (`beat_valid/data/last`) a write client has no use for.  The shell exposes
  // one guard socket and it is named for GEOM, so both stop here -- see entry
  // I26 for why that is a REACHABLE boundary and not I23's refusal.
  output zhao_guard_req_t         terr_ps_guard_req_o,
  input  zhao_guard_rsp_t         terr_ps_guard_rsp_i,
  input  logic                    terr_ps_beat_valid_i,
  input  logic [63:0]             terr_ps_beat_data_i,
  input  logic                    terr_ps_beat_last_i,

  // ---- I35: TERRAIN.PLACE's patch header, the two fields nothing reads -----
  // The patch COORDINATE and the source id come from TERRAIN.SEQ inside this
  // module.  The PITCH and the ENVELOPE do not; see entry I35.  They are NOT
  // tied off, and a harness that leaves them zero sees the placement REFUSE
  // every patch on `terr_place_env_mismatch_o` -- loudly, which is the correct
  // standing for an unfed corruption check.
  input  logic signed [7:0]       terr_place_pitch_log2_i,
  input  logic signed [31:0]      terr_place_env_x0_i,
  input  logic signed [31:0]      terr_place_env_z0_i,

  // ---- I34: TERRAIN.PATCH's field lane and its 9.1 list intake ------------
  input  logic                    terr_pt_fld_valid_i,
  output logic                    terr_pt_fld_ready_o,
  input  logic signed [31:0]      terr_pt_fld_height_i,
  input  logic                    terr_pt_fld_add_valid_i,
  output logic                    terr_pt_fld_add_ready_o,
  input  logic signed [31:0]      terr_pt_fld_add_x0_i,
  input  logic signed [31:0]      terr_pt_fld_add_z0_i,
  input  logic signed [31:0]      terr_pt_fld_add_x1_i,
  input  logic signed [31:0]      terr_pt_fld_add_z1_i,
  input  logic [31:0]             terr_pt_fld_add_hash_i,
  input  logic [15:0]             terr_pt_fld_add_cmd_i,
  output logic                    terr_pt_fld_add_accept_o,
  output logic                    terr_pt_fld_add_reject_o,
  output logic                    terr_pt_fld_covers_o,
  output logic [4:0]              terr_pt_fields_active_o,
  output logic [15:0]             terr_pt_trace_patch_id_o,
  output logic [31:0]             terr_pt_trace_hash_o,
  output logic [15:0]             terr_pt_trace_cmd_o,
  output logic [31:0]             terr_pt_programs_rejected_o,

  // ---- I32 (extended): TERRAIN.COMPCACHE's layer-D cell-state write -------
  input  logic                    terr_cc_cs_we_i,
  input  logic [4:0]              terr_cc_cs_ci_i,
  input  logic [4:0]              terr_cc_cs_cj_i,
  input  logic [1:0]              terr_cc_cs_substance_i,

  // ---- I21 (extended): the served patch's RETIREMENT pulse ---------------
  // "TESS is finished with the served patch", one patch per RISING EDGE.  The
  // block that knows is the one that issued the subpatch jobs, and that is the
  // absent owner entry I21 already names.
  input  logic                    terr_cc_serve_release_i,

  // ---- THE COMPOSE ENGINE'S EVIDENCE --------------------------------------
  // Events, never cycles.  These are what say a PAGE became a LATTICE rather
  // than four blocks having elaborated next to each other.
  output logic [31:0]             terr_ps_lattices_o,
  output logic [31:0]             terr_ps_lattices_refused_o,
  output logic [31:0]             terr_ps_vertices_o,
  output logic [31:0]             terr_ps_bursts_o,
  output logic [31:0]             terr_ps_guard_denied_o,
  output logic [31:0]             terr_ps_incomplete_o,
  output logic                    terr_ps_idle_o,
  // A TAP on the streamer's completion, not a handshake: the READY belongs to
  // TERRAIN.RESIDENCY's unpin port inside this module.  Exported so a refusal
  // can be READ rather than only counted.
  output logic                    terr_ps_done_valid_o,
  output logic                    terr_ps_done_ok_o,
  output logic [3:0]              terr_ps_done_verdict_o,

  output logic                    terr_place_valid_o,
  output logic [15:0]             terr_place_src_id_o,
  output logic [15:0]             terr_place_env_mismatch_o,
  output logic [15:0]             terr_place_pitch_bad_o,
  output logic [15:0]             terr_place_range_o,
  output logic [15:0]             terr_place_patches_o,

  output logic [31:0]             terr_pt_samples_o,
  output logic [15:0]             terr_pt_subpatch_dirty_o,
  output logic                    terr_pt_idle_o,

  output logic                    terr_cc_fill_busy_o,
  output logic                    terr_cc_fill_done_o,
  output logic                    terr_cc_serve_valid_o,
  output logic [15:0]             terr_cc_serve_src_id_o,
  output logic [31:0]             terr_cc_fill_records_o,
  output logic [31:0]             terr_cc_patches_filled_o,
  output logic [31:0]             terr_cc_patches_served_o,
  output logic [31:0]             terr_cc_fill_overrun_o,
  output logic [31:0]             terr_cc_lat_oob_o,
  output logic [31:0]             terr_cc_cs_oob_o,

  // ---- TERRAIN PAGING evidence -------------------------------------------
  // Events, never cycles, except where the name says otherwise. These are the
  // instrument that says the spine carried a beat rather than merely
  // elaborating, which is the whole question this composition has to answer.
  output logic [31:0]             terr_cmd_sets_accepted_o,
  output logic [31:0]             terr_cmd_sets_refused_o,
  output logic [31:0]             terr_cmd_records_emitted_o,
  output logic [31:0]             terr_cmd_crc_fails_o,
  output logic [31:0]             terr_cmd_bridge_errs_o,
  output logic                    terr_seq_busy_o,
  output logic                    terr_seq_done_o,
  output logic [31:0]             terr_seq_records_consumed_o,
  output logic [31:0]             terr_seq_patches_issued_o,
  output logic [31:0]             terr_seq_claims_issued_o,
  output logic [31:0]             terr_seq_claims_refused_o,
  output logic [31:0]             terr_seq_claims_same_o,
  output logic [31:0]             terr_seq_loads_issued_o,
  output logic [31:0]             terr_seq_skipped_not_resident_o,
  output logic [31:0]             terr_seq_frame_faults_o,
  // A tripwire, not a decoration: an answer arrived with nothing waiting for
  // one. `zhao_terrain_seq`'s own header explains why every consequence of it
  // is silent, and the composed bench for that block has already caught a real
  // shim bug with it, so it is a detector that has been SEEN to fire.
  output logic                    terr_seq_err_stray_ans_o,
  output logic [31:0]             terr_res_hits_o,
  output logic [31:0]             terr_res_misses_o,
  output logic [31:0]             terr_res_claims_o,
  output logic [31:0]             terr_res_evictions_o,
  output logic [31:0]             terr_res_crc_failures_o,
  output logic [31:0]             terr_res_resident_o,
  output logic [31:0]             terr_lq_accepted_o,
  output logic [31:0]             terr_lq_issued_o,
  output logic [31:0]             terr_lq_high_water_o,
  output logic [31:0]             terr_pl_pages_loaded_o,
  output logic [31:0]             terr_pl_pages_faulted_o,
  output logic [31:0]             terr_pl_crc_fails_o,
  output logic [31:0]             terr_pl_load_bytes_o,
  output logic [31:0]             terr_pl_guard_denied_o,
  output logic [31:0]             terr_pl_bridge_errs_o,
  // THE INTEGRATION'S OWN OBLIGATION, MADE MEASURABLE. The loader carries a
  // slot ONE BIT WIDER than the directory's handle so a computed 1,024 refuses
  // instead of aliasing onto slot 0. This composition drives that port from a
  // TERR_SLOTW producer, so the extra bit can only ever be zero -- and a
  // counter that proves it is better than a comment that asserts it.
  output logic [31:0]             terr_pl_slot_overflow_o,
  // A REFUSAL IS NOT A FAULT AND MUST NOT LOOK LIKE SILENCE. `pages_refused_o`
  // counts jobs the loader judged BEFORE touching memory (bad slot, unaligned
  // or unreachable source, outside the staging arena, stale epoch) and
  // `fault_verdict_o` names which. Leaving them unexposed cost a diagnosis
  // once already: with loaded=0, faulted=0 and bytes=0 there is no way to tell
  // a refused job from a loader that never started.
  output logic [31:0]             terr_pl_pages_refused_o,
  output logic [3:0]              terr_pl_fault_verdict_o,
  // THE FAILING PAGE, LATCHED. The block emits this trio precisely so that a
  // refusal names WHICH page it refused -- its own header calls it
  // "MEASURE.HISTOGRAM's refuse-loudly lane". Dropping it turns every fault
  // into an anonymous count, which is a refusal that is not loud at all.
  output logic [31:0]             terr_pl_fault_island_o,
  output logic signed [15:0]      terr_pl_fault_ix_o,
  output logic signed [15:0]      terr_pl_fault_iz_o,
  output logic [31:0]             terr_pl_fault_src_id_o,
  output logic [31:0]             terr_pl_incomplete_o,
  output logic [31:0]             terr_pl_hdr_ident_fails_o,
  // THE ARBITER'S OWN STARVATION INSTRUMENT. Rule 5 says starvation must be
  // visible, and `c1_wait_cycles_o` is how. This composition is the first to
  // put two terrain clients on it, so the number it reports is evidence about
  // a fairness contract that had never carried two live clients before.
  output logic [31:0]             terr_hps_c0_bursts_o,
  output logic [31:0]             terr_hps_c1_bursts_o,
  output logic [31:0]             terr_hps_c1_wait_cycles_o,

  // ---- TERRAIN evidence: the sequencer's and the tessellator's ------------
  output logic [PROJ_T_ARENAS-1:0] terr_held_o,
  output logic                    terr_busy_o,
  output logic [31:0]             terr_jobs_accepted_o,
  output logic [31:0]             terr_jobs_no_view_o,
  output logic [31:0]             terr_jobs_rejected_o,
  output logic [31:0]             terr_jobs_empty_o,
  output logic [31:0]             terr_groups_opened_o,
  output logic [31:0]             terr_groups_released_o,
  output logic [31:0]             terr_fills_forwarded_o,
  output logic [31:0]             terr_fills_dropped_o,
  output logic [31:0]             terr_refs_forwarded_o,
  output logic [31:0]             terr_release_unsafe_o,
  output logic [31:0]             terr_tess_vertices_o,
  output logic [31:0]             terr_tess_refs_o,
  output logic [31:0]             terr_tess_rejected_o,
  output logic [31:0]             terr_tess_lod_clamped_o,
  output logic [31:0]             terr_tess_mode_invalid_o,
  output logic                    terr_tess_idle_o,

  // ---- I13: the projector's TRIANGLE OUTPUT -------------------------------
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
  // The `atm_*` GROUP IS GONE FROM THIS EDGE, 2026-09-19. It is now internal:
  // TWOD.PLANE, TWOD.SAMPLER and POST.COMPOSITE are composed at the end of
  // this module and the atmosphere sheet never leaves. Entry I17 records what
  // changed and what did not.
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
  // `tri_area2_i` USED TO BE HERE and is now driven internally by
  // `zhao_geom_setup`, which is composed below -- see the wire `st_area2` and
  // header entry 7. It was the twenty-first field of a twenty-one-field
  // packet whose other twenty were already internal, and leaving it at the
  // edge held every pixel out of the framebuffer.
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

  // THE TRIANGLE PORT IS NO LONGER AT THIS EDGE. `render_tri_valid_i`,
  // `render_tri_ready_o`, the nine edge-function words, the top-left mask, the
  // six corners, the scan box and the source id were boundary inputs here and
  // are now driven INTERNALLY by `zhao_geom_setup`, which is composed below
  // and is their real producer. The paragraph above says "when the command
  // front end grows a draw path these become internal" -- that is half true and
  // the half that mattered was different: what was missing was not CMD but the
  // SETUP block itself, and it existed all along. The door is now fed; what
  // feeds GEOM.CLIP in front of it is entry I24.
  //
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
  // THESE FIVE PORTS ARE GONE, 2026-09-19, and the sentence above is why the
  // removal is the point rather than a tidy-up. `geom_guard_req_i`,
  // `geom_guard_rsp_o` and the three `geom_beat_*_o` were this module's edge:
  // the bench answered the grants and fabricated the beats, so "the whole
  // staircase rested on a memory that granted immediately and answered in one
  // cycle" was still true of the CONSOLE even after it stopped being true of
  // the shell. `u_geom_mem_adapter` drives that socket now (connected item
  // 11), so the fetchers are behind the real guard and the real controller
  // and the only memory left for a harness to supply is the SDRAM itself, at
  // `phy_*`, where the completion plan puts it.

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

  // ==========================================================================
  // SURFACE. The pair below is composed and CLOSED ON ITSELF -- SURFACE.STAMP
  // is SURFACE.SHEET's only client and SURFACE.SHEET is SURFACE.STAMP's only
  // store, so the request, page and write channels are all internal wires and
  // none of them appears here. What DOES appear is the three ends that have no
  // owner in this tree (entries I30, I31, I32) plus the pair's evidence.
  // ==========================================================================

  // I30: the SurfaceStamp dispatch. CMD.SCHEDULER has no path to it.
  input  logic               surf_cmd_valid_i,
  output logic               surf_cmd_ready_o,
  input  logic        [31:0] surf_cmd_handle_i,
  input  logic        [ 7:0] surf_cmd_operation_i,
  input  logic        [ 7:0] surf_cmd_tag_i,
  input  logic        [15:0] surf_cmd_strength_i,
  input  logic signed [31:0] surf_cmd_tx_i,
  input  logic signed [31:0] surf_cmd_ty_i,
  input  logic signed [31:0] surf_cmd_radius_i,
  input  logic signed [31:0] surf_cmd_ring_width_i,
  input  logic signed [31:0] surf_cmd_env_x0_i,
  input  logic signed [31:0] surf_cmd_env_z0_i,
  input  logic signed [31:0] surf_cmd_env_x1_i,
  input  logic signed [31:0] surf_cmd_env_z1_i,
  input  logic               surf_cmd_blend_en_i,
  input  logic        [ 2:0] surf_cmd_blend_i,
  input  logic        [ 2:0] surf_cmd_age_shift_i,
  input  logic               surf_cmd_field_en_i,
  input  logic        [15:0] surf_cmd_src_id_i,

  // I31: the field-driven brush. FIELD.SEQ.STAMP is not built.
  input  logic        surf_fld_valid_i,
  output logic        surf_fld_ready_o,
  input  logic [31:0] surf_fld_tag_op_i,
  input  logic [15:0] surf_fld_strength_i,

  // I32: `stamp_results` -> TERRAIN.BAKE, which is not composed.
  output logic        surf_res_valid_o,
  input  logic        surf_res_ready_i,
  output logic [11:0] surf_res_texel_o,
  output logic [ 7:0] surf_res_tag_o,
  output logic [ 7:0] surf_res_strength_o,
  output logic [ 7:0] surf_res_before_o,
  output logic [15:0] surf_res_src_id_o,

  // SURFACE.SHEET's spare response fields. NOT a gap: SURFACE.STAMP consumes
  // the two it needs (`status`, `strength`) and these three are the block's
  // own evidence, which leaves the module rather than being dropped.
  output logic [ 1:0] surf_pg_op_o,
  output logic [ 7:0] surf_pg_tag_o,
  output logic [15:0] surf_pg_src_id_o,

  // residency_status and the pair's counters
  output logic [SURF_SLOTS-1:0] surf_res_occupancy_o,
  output logic        surf_res_busy_o,
  output logic        surf_res_overflow_o,
  output logic        surf_sheet_wr_miss_o,
  output logic [15:0] surf_sheet_wr_miss_src_id_o,
  output logic        surf_sheet_idle_o,
  output logic [31:0] surf_sheet_texels_touched_o,
  output logic        surf_stamp_done_o,
  output logic        surf_stamp_rejected_o,
  output logic        surf_stamp_idle_o,
  output logic [31:0] surf_stamps_o,
  output logic [31:0] surf_stamp_texels_touched_o,

  // ==========================================================================
  // I24: THE PARTICLE DRAW PATH'S TWO ENDS.
  //
  // The MIDDLE is now internal -- PART.COLLIDE's records fork into
  // PART.PROJECT, which shares the one projector, and on into PART.LADDER and
  // the two endpoints. What leaves the module is:
  //
  //   * the owner values PART.PROJECT and the ladder need and nothing here
  //     produces: the per-(particle, camera) hold state (I23's absent DDR), the
  //     ladder's species/governor inputs, and the particle's RGB. PART.TABLE's
  //     `v_colour_o` is an INDEX and no palette block exists to turn it into a
  //     colour, which is why the colour is a value here rather than a lookup;
  //   * the two endpoints' packets, whose customer is the same absent GEOM
  //     replay/setup path I11, I12 and I13 name;
  //   * the rung write-back, which is where the hold state has to GO for the
  //     next frame to have any.
  //
  // The SCISSOR is NOT here: PART.SOFT takes the same mode-derived rectangle
  // GEOM.CLIP does (GLUE 1), because it is the console's own pass geometry and
  // not a second opinion about it.
  // ==========================================================================
  input  logic signed [31:0] part_prj_base_radius_i,
  input  logic               part_prj_view_i,
  input  logic        [15:0] part_prj_trail_i,
  input  logic               part_prj_narrow_i,
  input  logic               part_prj_protected_i,
  input  logic        [ 2:0] part_prj_gov_floor_i,
  input  logic        [ 2:0] part_prj_prev_rung_i,
  input  logic        [ 3:0] part_prj_hold_i,
  input  logic               part_prj_first_i,
  input  logic        [ 7:0] part_prj_r_i,
  input  logic        [ 7:0] part_prj_g_i,
  input  logic        [ 7:0] part_prj_b_i,
  input  logic        [15:0] part_prj_src_id_i,

  // the rung and the hold state it produced, for the next frame's store
  output logic               part_rung_valid_o,
  input  logic               part_rung_ready_i,
  output logic        [ 2:0] part_rung_o,
  output logic        [ 3:0] part_rung_hold_o,
  output logic               part_rung_changed_o,
  output logic        [15:0] part_rung_src_id_o,

  // PART.EXPAND's three-vertex screen fan
  output logic               part_exp_valid_o,
  input  logic               part_exp_ready_i,
  output logic signed [21:0] part_exp_ax_o,
  output logic signed [21:0] part_exp_ay_o,
  output logic signed [21:0] part_exp_bx_o,
  output logic signed [21:0] part_exp_by_o,
  output logic signed [21:0] part_exp_cx_o,
  output logic signed [21:0] part_exp_cy_o,
  output logic signed [31:0] part_exp_d_o,
  output logic        [ 7:0] part_exp_r_o,
  output logic        [ 7:0] part_exp_g_o,
  output logic        [ 7:0] part_exp_b_o,
  output logic               part_exp_depth_test_o,
  output logic               part_exp_depth_write_o,
  output logic        [15:0] part_exp_src_id_o,

  // PART.SOFT's scissored whole-pixel span
  output logic               part_sft_valid_o,
  input  logic               part_sft_ready_i,
  output logic signed [12:0] part_sft_min_x_o,
  output logic signed [12:0] part_sft_max_x_o,
  output logic signed [12:0] part_sft_min_y_o,
  output logic signed [12:0] part_sft_max_y_o,
  output logic signed [31:0] part_sft_d_o,
  output logic        [ 7:0] part_sft_r_o,
  output logic        [ 7:0] part_sft_g_o,
  output logic        [ 7:0] part_sft_b_o,
  output logic               part_sft_depth_test_o,
  output logic               part_sft_depth_write_o,
  output logic        [15:0] part_sft_src_id_o,

  // the draw path's evidence
  output logic [31:0] part_prj_projected_o,
  output logic [31:0] part_prj_behind_o,
  output logic [31:0] part_prj_geom_grants_o,
  output logic [31:0] part_prj_part_grants_o,
  output logic [31:0] part_prj_contended_o,
  output logic [31:0] part_prj_size_sat_o,
  output logic [31:0] part_prj_slot_pressure_o,
  output logic [31:0] part_prj_tag_collision_o,
  output logic [31:0] part_prj_ladder_unexpected_o,
  output logic [31:0] part_lad_decisions_o,
  output logic [31:0] part_lad_changes_o,
  output logic [31:0] part_lad_held_o,
  output logic [31:0] part_lad_gov_forced_o,
  output logic [31:0] part_exp_polygons_o,
  output logic [31:0] part_sft_sprites_o,

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
  input  logic [15:0] phy_dq_i,

  // --------------------------------------------------------------------------
  // TWOD: the plane descriptors, the sprite descriptors, the sampler's assets
  // and the sprite colour stream.  Added 2026-09-19 with TWOD.SAMPLER.
  // --------------------------------------------------------------------------
  // WHAT IS AND IS NOT A GAP HERE, because the group is large and it would be
  // easy to read all of it as one:
  //   * the two DESCRIPTOR groups are the CMD seam. SetPlane and the sprite
  //     display list are commands, `zhao_cmd_decoder` emits record headers and
  //     not decoded descriptors, and the executor that would turn one into the
  //     other is the same absent path entries I14 and I30 describe. GAP, and
  //     it is the SAME gap those two already name rather than a new one.
  //   * the three LOAD groups are ASSETS. Entry I17's own sentence about the
  //     grading curves -- "generated ASSETS by design, so their load port is
  //     legitimately external" -- covers a texture page, a palette and a
  //     binding exactly. NOT a gap.
  //   * `twod_sc_*` is the sprite colour, and it has no consumer HERE because
  //     POST.COMPOSITE's `hud_*` port is a raster-order random access and
  //     TWOD.SPRITE walks in descriptor order. GAP, and a NEW one -- see I17.
  input  logic                    twod_pd_valid_i,
  output logic                    twod_pd_ready_o,
  input  logic                    twod_pd_slot_i,
  input  logic [1:0]              twod_pd_role_i,
  input  logic [1:0]              twod_pd_blend_i,
  input  logic [7:0]              twod_pd_opacity_i,
  input  logic                    twod_pd_format_i,
  input  logic [15:0]             twod_pd_width_i,
  input  logic [15:0]             twod_pd_height_i,
  input  logic                    twod_pd_wrap_u_i,
  input  logic                    twod_pd_wrap_v_i,
  input  logic signed [31:0]      twod_pd_a_i,
  input  logic signed [31:0]      twod_pd_b_i,
  input  logic signed [31:0]      twod_pd_c_i,
  input  logic signed [31:0]      twod_pd_d_i,
  input  logic signed [31:0]      twod_pd_u0_i,
  input  logic signed [31:0]      twod_pd_v0_i,
  input  logic [1:0]              twod_pd_view_mask_i,
  input  logic [7:0]              twod_pd_palette_i,

  input  logic                    twod_sd_valid_i,
  output logic                    twod_sd_ready_o,
  input  logic signed [15:0]      twod_sd_x_i,
  input  logic signed [15:0]      twod_sd_y_i,
  input  logic [15:0]             twod_sd_w_i,
  input  logic [15:0]             twod_sd_h_i,
  input  logic signed [31:0]      twod_sd_u_i,
  input  logic signed [31:0]      twod_sd_v_i,
  input  logic signed [31:0]      twod_sd_a00_i,
  input  logic signed [31:0]      twod_sd_a01_i,
  input  logic signed [31:0]      twod_sd_a10_i,
  input  logic signed [31:0]      twod_sd_a11_i,
  input  logic [2:0]              twod_sd_format_i,
  input  logic [7:0]              twod_sd_palette_i,
  input  logic [15:0]             twod_sd_tint_i,
  input  logic [1:0]              twod_sd_blend_i,
  input  logic [1:0]              twod_sd_view_mask_i,
  input  logic [7:0]              twod_sd_order_i,
  input  logic [15:0]             twod_sd_src_id_i,

  input  logic                    twod_ld_page_we_i,
  input  logic [TWOD_PAW-1:0]     twod_ld_page_addr_i,
  input  logic [15:0]             twod_ld_page_data_i,
  input  logic                    twod_ld_pal_we_i,
  input  logic [TWOD_PALAW-1:0]   twod_ld_pal_addr_i,
  input  logic [15:0]             twod_ld_pal_data_i,
  input  logic                    twod_ld_bind_we_i,
  input  logic [TWOD_BSW-1:0]     twod_ld_bind_sel_i,
  input  logic [TWOD_PAW-1:0]     twod_ld_bind_base_i,
  input  logic [3:0]              twod_ld_bind_lstride_i,
  input  logic [3:0]              twod_ld_bind_lheight_i,
  input  logic                    twod_atm_slot_i,
  input  logic signed [31:0]      twod_line_scroll_i,

  output logic                    twod_sc_valid_o,
  input  logic                    twod_sc_ready_i,
  output logic [15:0]             twod_sc_rgb_o,
  output logic signed [15:0]      twod_sc_x_o,
  output logic signed [15:0]      twod_sc_y_o,
  output logic [15:0]             twod_sc_tint_o,
  output logic [1:0]              twod_sc_blend_o,
  output logic [7:0]              twod_sc_order_o,
  output logic [15:0]             twod_sc_src_id_o,
  output logic                    twod_sc_last_o,

  // ---- TWOD evidence -------------------------------------------------------
  output logic [31:0]             twod_plane_pixels_o,
  output logic [31:0]             twod_plane_refused_role_o,
  output logic [31:0]             twod_plane_refused_blend_o,
  output logic [31:0]             twod_plane_skipped_view_o,
  output logic [31:0]             twod_plane_wrap_fail_o,
  output logic [31:0]             twod_sprite_descriptors_o,
  output logic [31:0]             twod_sprite_skipped_view_o,
  output logic [31:0]             twod_sprite_refused_o,
  output logic [31:0]             twod_sprite_pixels_o,
  output logic [31:0]             twod_samples_o,
  output logic [31:0]             twod_plane_samples_o,
  output logic [31:0]             twod_sprite_samples_o,
  output logic [31:0]             twod_clut8_samples_o,
  output logic [31:0]             twod_rgb565_samples_o,
  output logic [31:0]             twod_texel_wrapped_o,
  output logic [31:0]             twod_page_oob_o,
  output logic [31:0]             twod_bind_missing_o,
  output logic [31:0]             twod_fmt_refused_o,
  output logic [31:0]             twod_pal_refused_o,
  output logic [31:0]             twod_skipped_fill_o,
  output logic [31:0]             twod_atm_underrun_o,
  output logic [31:0]             twod_walk_stalls_o,
  output logic [31:0]             twod_sprite_stalls_o,
  output logic [31:0]             twod_tint_unapplied_o,
  output logic [31:0]             twod_pair_lost_o,

  // --------------------------------------------------------------------------
  // CMD.DECODER's record headers and verdict.  Added 2026-09-19.
  // --------------------------------------------------------------------------
  // OUTPUTS ONLY, and every one of them is driven by real logic inside this
  // module -- see section 7b. They are on the edge because the decoder's
  // consumer (the command executor) does not exist yet, and a stream that ends
  // in a wire is pruned dead logic wearing a port's name, which this campaign
  // counts as an absent function rather than a present one.
  output logic        cmd_rec_valid_o,
  output logic [15:0] cmd_rec_opcode_o,
  output logic [15:0] cmd_rec_bytes_o,
  output logic [31:0] cmd_rec_source_id_o,
  output logic [31:0] cmd_rec_index_o,
  output logic        cmd_decode_done_o,
  output logic [ 7:0] cmd_decode_error_o,
  output logic [31:0] cmd_bytes_consumed_o,
  output logic [31:0] cmd_commands_o,

  // --------------------------------------------------------------------------
  // CMD.EXEC's evidence.  Added 2026-09-19 with section 7c.
  // --------------------------------------------------------------------------
  // OUTPUTS ONLY, driven by real logic below. The executor writes the
  // projector's matrix bank and dispatches SURFACE.STAMP, and BOTH of those go
  // to internal consumers -- so without these ports the only thing observable
  // about whether a command was executed would be a downstream side effect two
  // subsystems away. These are the numbers a bench reads to say "the packet
  // became console state", and every one of them is fired by a named case in
  // tests/command/cmd_exec_directed.cpp.
  //
  // `cmd_exec_unsupported_o` IS THE HONEST ONE. It counts records the ABI
  // defines and this executor has no arm for -- BeginFrame, EndFrame,
  // DrawForm, every reserved opcode. It is the distance between the command
  // surface and the executor expressed as a NUMBER rather than as prose in a
  // header, and it is expected to be large today.
  output logic [31:0] cmd_exec_committed_o,
  output logic [31:0] cmd_exec_abandoned_o,
  output logic [31:0] cmd_exec_views_o,
  output logic [31:0] cmd_exec_stamps_o,
  output logic [31:0] cmd_exec_stamp_overflow_o,
  output logic [31:0] cmd_exec_view_refused_o,
  output logic [31:0] cmd_exec_src_truncated_o,
  output logic [31:0] cmd_exec_unsupported_o
);

  // Every parameter forwarded BY NAME, including TERR_MEMSLOT, which this
  // wrapper derives from the mutated TERR_POOL_SLOTS above. Forwarding the
  // whole set rather than relying on production defaults is what keeps the
  // mutation a SINGLE line: change one knob here and every width that depends
  // on it follows, in the wrapper and in the instance alike.
  zhao_console_core #(
      .FRAMER_Q(FRAMER_Q),
      .WFIFO_W(WFIFO_W),
      .PART_REC_W(PART_REC_W),
      .PART_CAPACITY(PART_CAPACITY),
      .PART_CHILD_D(PART_CHILD_D),
      .PART_SPECIES_N(PART_SPECIES_N),
      .PART_AGE_W(PART_AGE_W),
      .PART_POS_W(PART_POS_W),
      .PART_VEL_W(PART_VEL_W),
      .PART_NRM_W(PART_NRM_W),
      .PART_FX_W(PART_FX_W),
      .PART_PID_W(PART_PID_W),
      .PART_TICK_W(PART_TICK_W),
      .PART_TBL_LD_W(PART_TBL_LD_W),
      .GEOM_ARENAS(GEOM_ARENAS),
      .GEOM_DEPTH(GEOM_DEPTH),
      .GEOM_NVIEWS(GEOM_NVIEWS),
      .GEOM_GEN_W(GEOM_GEN_W),
      .GEOM_PAY_A_W(GEOM_PAY_A_W),
      .GEOM_PAYLOAD_W(GEOM_PAYLOAD_W),
      .GEOM_INDEX_W(GEOM_INDEX_W),
      .GEOM_ARENA_W(GEOM_ARENA_W),
      .GEOM_MUL_LANES(GEOM_MUL_LANES),
      .GEOM_ASSET_MAX_VERTICES(GEOM_ASSET_MAX_VERTICES),
      .GEOM_ASSET_MAX_TRIANGLES(GEOM_ASSET_MAX_TRIANGLES),
      .GEOM_ASM_VIDW(GEOM_ASM_VIDW),
      .GEOM_CLIP_ATTRS(GEOM_CLIP_ATTRS),
      .GEOM_CLIP_ATTRW(GEOM_CLIP_ATTRW),
      .PROJ_T_ARENAS(PROJ_T_ARENAS),
      .PROJ_T_DEPTH(PROJ_T_DEPTH),
      .PROJ_T_INDEX_W(PROJ_T_INDEX_W),
      .PROJ_T_ARENA_W(PROJ_T_ARENA_W),
      .PROJ_T_IDX_W(PROJ_T_IDX_W),
      .POST_LINE_W(POST_LINE_W),
      .POST_MAX_H(POST_MAX_H),
      .POST_NLINE(POST_NLINE),
      .POST_LAG_LINES(POST_LAG_LINES),
      .POST_LAG_PX(POST_LAG_PX),
      .POST_XW(POST_XW),
      .POST_YW(POST_YW),
      .HIST_EW(HIST_EW),
      .HIST_SUB_BITS(HIST_SUB_BITS),
      .HIST_LANES(HIST_LANES),
      .HIST_CW(HIST_CW),
      .HIST_BINW(HIST_BINW),
      .SURF_SLOTS(SURF_SLOTS),
      .SURF_SQ_RADIX(SURF_SQ_RADIX),
      .TERR_SETS(TERR_SETS),
      .TERR_WAYS(TERR_WAYS),
      .TERR_SLOTW(TERR_SLOTW),
      .TERR_GENW(TERR_GENW),
      .TERR_SEQW(TERR_SEQW),
      .TERR_PINW(TERR_PINW),
      .TERR_CSLOTS(TERR_CSLOTS),
      .TERR_LOADQ_D(TERR_LOADQ_D),
      .TERR_POOL_BASE(TERR_POOL_BASE),
      .TERR_POOL_SLOTS(TERR_POOL_SLOTS),
      .TERR_MEMSLOT(TERR_MEMSLOT),
      .TERR_PAGE_BYTES(TERR_PAGE_BYTES)
  ) u_dut (.*);

endmodule : zhao_console_core_slot_overflow_mutant

`default_nettype wire
