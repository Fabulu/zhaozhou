// zhao_console_core_untex_decl_mutant.sv -- the positive control for
// `geom_untex_refused_o`, which no legal stimulus can fire.
//
// ---------------------------------------------------------------------------
// WHY THIS FILE EXISTS
// ---------------------------------------------------------------------------
// Owner ruling R197 (2026-09-20) sanctions a primitive entering GEOM.CLIP with
// u/w and v/w undefined PROVIDED IT DECLARES SO, and its law 3 says an
// untextured primitive arriving where a textured one is required is REFUSED
// AND COUNTED, never silently sampled. `zhao_console_core` implements that at
// GEOM.CLIP's input door:
//
//     wire cl_in_untex_c  = (GEOM_REPLAY_UNTEX_DECL != 0);
//     wire cl_in_refuse_c = cl_in_untex_c && (mw_pub_sample_count != 2'd0);
//
// and counts each refused triangle on `geom_untex_refused_o`. The only
// composed producer, GEOM.REPLAY, declares TEXTURED through the named seam
// `GEOM_REPLAY_UNTEX_DECL = 0` -- truthfully, because every one of its
// triangles is built from a format-0 vertex record and format 0 carries u/v.
// So `cl_in_untex_c` is STRUCTURALLY ZERO in production, the counter cannot
// move under any legal stimulus, and a counter asserted zero and never seen
// to move is a claim rather than a measurement (CLAUDE.md, "a guard you cannot
// reach with legal stimulus needs a COMMITTED MUTANT").
//
// ---------------------------------------------------------------------------
// IT IS A WRAPPER, NOT A COPY -- the shape `zhao_console_core_slot_overflow_mutant`
// established, for the same reason
// ---------------------------------------------------------------------------
// Everything below the port list is one instantiation of `zhao_console_core`
// by `.*`, with every parameter passed by name. There is no copied body to go
// stale; a port gained or lost in production fails to elaborate here and says
// so.
//
// THE ONE SUBSTANTIVE CHANGE:   GEOM_REPLAY_UNTEX_DECL = 0  ->  1
//
// That is legal to write because the parameter IS the seam R48's `ALPHA_C`
// precedent names: the value a producer without texture coordinates would
// present. Setting it makes every REPLAY triangle DECLARE itself untextured
// while the smoke's material (record 1, which the plain run proves takes a
// sample -- `render_texture_samples_o != 0`) still asks for one. That is the
// exact combination the door exists to refuse, so under
// `tests/prod/run_console_core_smoke.ps1`'s stimulus the counter must reach
// the reference's replayed count (`SGF_EXP_REPLAYED`), GEOM.CLIP must submit
// NOTHING, and the material window's structural guards must stay silent
// (the refusal happens BEFORE its accounted span). The bench asserts all
// three; a stray increment, a partial refusal or a leak into the occupancy
// each fail it.
//
// THE DRIVER IS tests/prod/run_console_core_smoke.ps1 -UntexMutant, and its
// polarity is INVERTED: it passes when `geom_untex_refused_o` is NON-ZERO and
// equal to the replayed count, and fails when it reads 0. It is evidence about
// the instrument, not about the design. The same script without the switch is
// the negative control: the identical stimulus against unmutated production
// must read 0.
//
// THE PORT LIST IS GENERATED FROM PRODUCTION'S, VERBATIM, exactly as the
// slot-overflow wrapper's is. Regenerate BOTH if `zhao_console_core`'s
// parameter or port block changes: the header + parameter block + port block
// of fpga/rtl/prod/zhao_console_core.sv (module line through the closing
// `);`), with the module renamed and the one GEOM_REPLAY_UNTEX_DECL line
// edited. It cannot go stale silently: `.*` fails to elaborate on any port
// gained or lost.
//
// No simulation assertion is disabled here; the wrapper contains none, and the
// composer's own elaboration guards are unaffected by the mutation.
// ---------------------------------------------------------------------------
`default_nettype none
module zhao_console_core_untex_decl_mutant
  import zhao_pkg::*, zhao_abi_pkg::*, zhao_fb_tuple_pkg::*;
#(
  // ---- the shell's own knobs, carried through ------------------------------
  parameter int unsigned FRAMER_Q = 8,
  parameter int unsigned WFIFO_W  = 64,

  // ---- INPUT.SNAC (owner ruling R7) ----------------------------------------
  // How many SNAC connectors the board carries. They drive canonical pad slots
  // 0..SNAC_PORTS-1; slots above that are always the incoming route's. Two is
  // the MiSTer SNAC shape. The bus rate, the /ACK timeout and the inter-poll
  // gap are the ADAPTER's knobs and stay there (spec/input_rules.md 7.1); this
  // one is here because it sets a PORT WIDTH on this module.
  parameter int unsigned SNAC_PORTS = 2,

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
  // R68 sub-build 4: 17, not 16. The low GEOM_ARENA_W+GEOM_INDEX_W = 15 bits
  // are the {arena, index} rider; the top TWO are `zhao_part_project`'s owner
  // FIELD. At 16 the field was one bit and client A could name exactly two
  // owners, which is the constraint that held GEOM.LOD's instance centre out.
  parameter int unsigned GEOM_PAY_A_W  = 17,
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
  // The vertex-attribute store's word (owner ruling R11): slots 1..6 of the
  // packet above -- everything but invw24, which is GEOM.DEPTHQUANT's alone.
  parameter int unsigned GEOM_ATTR_STORE_W = (GEOM_CLIP_ATTRS - 1) * 32,
  parameter int unsigned GEOM_REPLAY_UNTEX_DECL = 1,   // MUTANT: was 0
  // WHICH SLOT OF THAT PACKET CARRIES WHICH PACKET-D PLANE. Named constants
  // rather than literals inside `u_geom_attrpack`, because CLAUDE.md's rule is
  // that a ratified layout is still a knob: "this is generated from the
  // reference, so it is not a knob" is how a wrong number becomes an
  // unadjustable wrong number. The order is `zhao_geom_clip`'s ruling 5 and
  // `tests/geometry/geom_clip_attrswap_directed.cpp`'s own line 41.
  parameter int unsigned GEOM_ATTR_SLOT_INVW     = 0,
  parameter int unsigned GEOM_ATTR_SLOT_U_OVER_W = 1,
  parameter int unsigned GEOM_ATTR_SLOT_V_OVER_W = 2,

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
  parameter int unsigned TERR_POOL_SLOTS = 1024,
  parameter int unsigned TERR_MEMSLOT     = $clog2(TERR_POOL_SLOTS) + 1,
  parameter int unsigned TERR_PAGE_BYTES  = 21376  // terrain_rules sec 2 / sec 7
) (
  // ==========================================================================
  // THE ADOPTED ORGANS' OWN BOUNDARY.
  // Every port in this section is listed in the header's
  // "INCOMPLETE -- TIED OFF, AND WHY" table with the owner that is missing.
  // ==========================================================================

  // (I1's seven `part_rd_*` / `part_wr_*` ports were here. CLOSED 2026-09-19:
  //  the generation store is HPS DDR by contract and `u_part_hps` streams it
  //  through client 3 of `u_terr_hps_arb`. What crosses this edge now is the
  //  HPS's own configuration, in the `terr_cfg_*` shape, below.)

  // ---- PART.STATE's HPS DDR buffers: the HPS's configuration and seed ------
  // NOT A TIE-OFF. PART.STATE.md: "Owns the two particle buffers in HPS DDR";
  // the HPS allocates them (spec/memory_rules.md 5, "particle pools per the
  // charter allocator") and in Verilator the harness IS the HPS (plan D10),
  // exactly as for `terr_cfg_arena_*`. The seed says "buffer `buf` holds
  // `count` records" -- the population descriptor's `active_count`
  // (spec/qformats.md 10) -- and is taken only between ticks.
  input  logic [31:0]             part_cfg_base0_i,
  input  logic [31:0]             part_cfg_base1_i,
  // (I1's four provisional `part_seed_*` ports were here. CLOSED 2026-09-19
  //  under owner rulings R41/R46: the seed is `SetPopulation`'s `active_count`
  //  and the buffer is 0 by that ratification, so `u_part_pop` drives the
  //  store's seed handshake and the board drives neither. The two BASES stay:
  //  they are the HPS allocator's (spec/memory_rules.md 5), not a game-facing
  //  command field, and putting an allocator address in a ratified record is a
  //  decision nobody has made.)
  // The store's evidence. `cur_count` is the generation's length as the
  // hardware counted it; the rest are `zhao_part_hps`'s counters, each fired by
  // stimulus in tests/particles/part_hps_directed.cpp.
  output logic                    part_hps_cur_buf_o,
  output logic [$clog2(PART_CAPACITY):0] part_hps_cur_count_o,
  output logic [31:0]             part_hps_ticks_o,
  output logic [31:0]             part_hps_ticks_dropped_o,
  output logic [31:0]             part_hps_ticks_unseeded_o,
  output logic [31:0]             part_hps_seeds_o,
  output logic [31:0]             part_hps_seeds_refused_o,
  output logic [31:0]             part_hps_rd_bursts_o,
  output logic [31:0]             part_hps_wr_bursts_o,
  output logic [31:0]             part_hps_records_read_o,
  output logic [31:0]             part_hps_records_written_o,
  // R54, 2026-09-19 evening: the bridge refusal `zhao_part_hps` used to be
  // blind to. `bridge_errs` is expected to read ZERO here -- the arbiter
  // pulses the bridge only from A_IDLE and every burst is aligned -- and that
  // expectation is now an EXPECTATION with an instrument behind it rather than
  // an argument standing in place of one. It is fired by stimulus in
  // tests/particles/part_hps_directed.cpp CASE H/I.
  output logic [31:0]             part_hps_bridge_errs_o,
  output logic [31:0]             part_hps_ticks_faulted_o,
  output logic [31:0]             part_hps_records_discarded_o,

  // ---- I33: PART.TABLE's PER-FRAME LOAD -----------------------------------
  // I2 and I3 ARE CLOSED and their twenty-five ports are GONE from this list
  // rather than driven -- `zhao_part_table` is instantiated below and answers
  // all four reads inside this module. What is left is the host that fills it,
  // and this is that seam. One word per clock; the table never refuses for
  // backpressure (`ld_ready_o` is constant high and says so in its own file).
  // (I33's six part_tbl_ld_* ports were here. CLOSED 2026-09-19 under owner
  //  ruling R42: the descriptors travel as DATA in a SPECIES_TABLE page the
  //  owner authors, published by the command that publishes every other
  //  resource, and u_part_table_loader carries the load words from the page
  //  to the port. Nothing in this console chooses what a species IS, which is
  //  the whole reason the entry stayed open. The evidence below is that
  //  block's.)
  output logic [31:0]             part_tbl_pages_o,
  output logic [31:0]             part_tbl_entries_o,
  output logic [31:0]             part_tbl_pages_dropped_o,
  output logic [31:0]             part_tbl_bad_magic_o,
  output logic [31:0]             part_tbl_truncated_o,
  output logic [31:0]             part_tbl_denied_o,

  // ---- (I5's four `part_fld_*` inputs were here. CLOSED 2026-09-20 under
  //  owner ruling R40: `zhao_field_flow_adapter` is the F profile's stream
  //  adapter and it is composed below as client 1 of `u_field_host`. The
  //  acceleration is computed from the flow program's own velocity output by
  //  R40's law, joined to the record it belongs to in the same cycle. The
  //  evidence below is that adapter's.)
  //
  // WHICH RESIDENT PROGRAM IS THE WIND, and whether one is resident at all.
  // NOT A TIE-OFF and not I5 moved sideways: it is the same console-policy
  // shape as `fld_stamp_slot_i` below, for the same reason I30 records --
  // no ratified opcode carries it, so an executor filling it in would be
  // choosing a value the ABI does not contain. With `slot_valid` LOW the
  // adapter answers every record immediately with the sample ABSENT, which
  // PART.UPDATE already handles by not adding the term; that is a console
  // with no wind armed, not a wind of zero.
  input  logic [2:0]              fld_flow_slot_i,
  input  logic                    fld_flow_slot_valid_i,
  // p0..p3 of the flow profile's input record (spec/form/field-ir.md 7.1).
  // The PROGRAM's parameters, not the particle's -- SW.STREAM's, travelling
  // with the plan that named the program (owner ruling R43).
  input  logic [127:0]            fld_flow_par_i,

  output logic [31:0]             part_fld_samples_o,
  output logic [31:0]             part_fld_bypassed_o,
  output logic [31:0]             part_fld_noprog_o,
  output logic [31:0]             part_fld_faults_o,
  output logic [31:0]             part_fld_saturations_o,
  output logic [31:0]             part_fld_stall_cycles_o,
  // The identity guard: the record offered while an answer is held is not the
  // record that answer was computed from. Its two operands are clocked by
  // different things, which is what makes it able to fire at all.
  output logic [31:0]             part_fld_rec_changed_o,

  // (I2's PART.COLLIDE slice -- `part_col_d_response_i` and the three
  //  coefficients -- was here. CLOSED: PART.TABLE serves it below, addressed by
  //  PART.COLLIDE's own new `d_index_o`.)

  // (I6's five `part_ter_*` inputs were here. CLOSED 2026-09-19 under owner
  //  ruling R1: PART.TERRAIN_TAP produces the sample from the live compose
  //  cache through TERRAIN.HEIGHTTAP, inside this module. See item 14.)

  // ---- I6's evidence: the terrain sample's census --------------------------
  // Every particle lands in exactly one of the first four, so a bench can say
  // WHY a particle did or did not see ground -- a cold cell, a void or an
  // unstaged patch, or a fault -- instead of reading PART.COLLIDE's single
  // "unavailable" total. The tap's three are the service's own view of the
  // same traffic.
  output logic [31:0]             part_ter_particles_o,
  output logic [31:0]             part_ter_ground_o,
  output logic [31:0]             part_ter_no_ground_o,
  output logic [31:0]             part_ter_missed_o,
  output logic [31:0]             part_ter_faults_o,     // cell mismatch + out of range
  output logic [31:0]             part_ter_fills_landed_o,
  output logic [31:0]             terr_tap_answered_o,
  output logic [31:0]             terr_tap_off_patch_o,
  output logic [31:0]             terr_tap_faults_o,     // placement + pitch + overflow

  // (I7's eight `part_pop_origin_*` / `part_plane_*` inputs were here. CLOSED
  //  2026-09-19 under owner ruling R41: `SetPopulation` 0x0303 is ratified and
  //  carries all eight, CMD.EXEC lowers it, and `u_part_pop` holds the
  //  descriptor as the levels PART.COLLIDE and PART.TERRAIN_TAP read on every
  //  beat. The evidence below is that bank's.)
  output logic [31:0]             part_pop_taken_o,
  output logic [31:0]             part_pop_refused_normal_o,
  output logic [31:0]             part_pop_refused_count_o,
  output logic [31:0]             part_pop_refused_flags_o,
  output logic [31:0]             part_pop_seeds_issued_o,
  output logic [31:0]             part_pop_handle_o,     // the population it holds
  output logic [31:0]             cmd_exec_pops_o,       // records CMD.EXEC lowered

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

  // ---- I36 IS CLOSED: GEOM.DRAWJOB builds the job (owner ruling R29) -------
  // The nine job ports that stood here are GONE. `zhao_geom_drawjob` resolves
  // the ratified DrawForm -- the MESH_STREAM residency row, that page's frozen
  // header, and the instance transform palette GEOM.LOOM writes -- and drives
  // GEOM.MESHFETCH directly. What leaves is its evidence, one port per REASON,
  // because the nine refusals have nine different diagnoses.
  output logic [31:0]             geom_dj_draws_o,
  output logic [31:0]             geom_dj_jobs_o,
  output logic [31:0]             geom_dj_masked_o,
  output logic [31:0]             geom_dj_empty_o,
  output logic [31:0]             geom_dj_pal_writes_o,
  output logic [31:0]             geom_dj_pal_dropped_o,
  output logic [31:0]             geom_dj_refused_cull_o,
  output logic [31:0]             geom_dj_refused_resident_o,
  output logic [31:0]             geom_dj_refused_stale_o,
  output logic [31:0]             geom_dj_refused_xform_o,
  output logic [31:0]             geom_dj_refused_denied_o,
  output logic [31:0]             geom_dj_refused_format_o,
  output logic [31:0]             geom_dj_refused_crc_o,
  output logic [31:0]             geom_dj_refused_reserved_o,
  output logic [31:0]             geom_dj_refused_layout_o,
  output logic [31:0]             geom_dj_hdr_reads_o,
  output logic [31:0]             geom_dj_hdr_crc_fail_o,
  output logic [31:0]             geom_dj_hdr_framing_o,

  // ---- I50 IS CLOSED: GEOM.LOOM's NODE STREAM arrives by DOORBELL ---------
  // NOT A TIE-OFF, in the same standing and the same words as `terr_jdb_*`
  // (I28) and `fld_db_*` (I42): the HPS is genuinely on the other side of this
  // edge, and in Verilator the harness IS the HPS.
  //
  // The owner ruling of 2026-08-31 6.4 puts the stream's producer OUTSIDE this
  // console on purpose -- "The ARM/compiler supplies a parent-before-child
  // topologically sorted stream" -- so what was missing was never a block. It
  // was a CARRIER, and owner ruling R69 says so in terms: "not blocked, only
  // unbuilt". `u_geom_loomfeed` is that carrier: SW.STREAM stages the sorted
  // stream in HPS DDR at the frozen 64-byte record of
  // `design/contracts/GEOM.LOOM.STREAM.md`, this mailbox names it, and the
  // hardware answers by returning the ticket.
  //
  // WHAT LEFT THIS EDGE. Twelve wide stream fields and a nine-element camera
  // basis -- 672 bits of input a host would have had to present 9,000 times a
  // frame -- are replaced by a POINTER and a TICKET. The bulk rides the
  // HPS-DDR bridge as client 4 of `u_terr_hps_arb`.
  input  logic [31:0]             geom_loom_db_plan_base_i,
  input  logic                    geom_loom_db_post_valid_i,
  output logic                    geom_loom_db_post_ready_o,
  input  logic [31:0]             geom_loom_db_post_base_i,
  input  logic [31:0]             geom_loom_db_post_ticket_i,
  output logic                    geom_loom_db_ret_valid_o,
  input  logic                    geom_loom_db_ret_ready_i,
  output logic [31:0]             geom_loom_db_ret_ticket_o,
  output logic                    geom_loom_db_ret_ok_o,
  output logic                    geom_loom_db_ret_refused_o,
  output logic [2:0]              geom_loom_db_ret_reason_o,
  output logic [2:0]              geom_loom_db_ret_loom_reason_o,
  output logic [15:0]             geom_loom_db_ret_nodes_o,
  output logic [31:0]             geom_loom_db_ret_plan_o,
  // The carrier's own evidence, separate from the loom's below: a stream the
  // loom never saw and a stream the loom refused are different faults and are
  // counted apart.
  output logic [31:0]             geom_loom_feed_posts_o,
  output logic [31:0]             geom_loom_feed_streams_o,
  // Node BEATS handed to the loom, which is NOT `geom_loom_nodes_o` (nodes
  // TRANSFORMED). They differ by exactly the beats the loom swallowed into a
  // drain after refusing a stream, so the gap between them is a real reading
  // and collapsing them into one counter would delete it.
  output logic [31:0]             geom_loom_feed_nodes_o,
  output logic [31:0]             geom_loom_feed_bursts_o,
  output logic [31:0]             geom_loom_feed_align_refused_o,
  output logic [31:0]             geom_loom_feed_hdr_refused_o,
  output logic [31:0]             geom_loom_feed_refused_o,
  output logic [31:0]             geom_loom_feed_faulted_o,
  output logic [31:0]             geom_loom_feed_replayed_o,
  output logic [31:0]             geom_loom_feed_post_stalls_o,
  output logic [31:0]             geom_loom_feed_bridge_errs_o,
  output logic [31:0]             geom_loom_feed_wait_cycles_o,
  output logic [31:0]             geom_loom_feed_ret_overflow_o,
  output logic [31:0]             geom_loom_nodes_o,
  output logic [31:0]             geom_loom_streams_o,
  output logic [31:0]             geom_loom_refused_sorted_o,
  output logic [31:0]             geom_loom_refused_parent_o,
  output logic [31:0]             geom_loom_refused_overflow_o,
  output logic [31:0]             geom_loom_refused_kind_o,
  output logic [31:0]             geom_loom_refused_shear_o,
  output logic [31:0]             geom_loom_refused_framing_o,

  // ---- I37 IS CLOSED: the descriptor's CRC verdict is computed inside ------
  // `u_geom_desc_crc` walks the fold over the returning beats. What leaves is
  // its evidence, so a refused descriptor says WHY at the edge: a CRC that
  // mismatched and a burst that was not eight beats are different faults.
  output logic [31:0]             geom_mf_crc_descriptors_o,
  output logic [31:0]             geom_mf_crc_fail_o,
  output logic [31:0]             geom_mf_crc_framing_o,

  // ---- I38 IS CLOSED: GEOM.REPLAY releases the meshlet, by proof ---------

  // ---- I39 IS CLOSED: the raster word RIDES THE MESHLET -------------------
  // The word is built by GEOM.DRAWJOB from the draw's own flags (R28's cull
  // mode) and carried in the JOB'S handshake through GEOM.MESHFETCH and
  // GEOM.ASSETFETCH to GEOM.ASSEMBLE, so a meshlet's triangles cannot take
  // another draw's state. The port that stood here is GONE.

  // ---- I41 IS CLOSED: the draw dispatch has a consumer INSIDE -------------
  // `cmd_draw_*` no longer leaves the module: GEOM.DRAWJOB is the resolver
  // entry I36 said was missing, and the three handles are resolved against
  // real residency rather than shipped out unresolved. The COUNTERS stay --
  // they are evidence, not a boundary.
  output logic [31:0]              cmd_exec_draws_o,
  output logic [31:0]              cmd_exec_draw_overflow_o,
  output logic [31:0]              cmd_exec_draw_src_truncated_o,

  // ---- R229: DrawPosedForm 0x0305's animation key -- BOUNDARY, see I29 ----
  // Added to the WRAPPER because the real module gained them, never the other
  // way round (owner ruling R220). `.*` cannot bind a port the wrapper does
  // not declare, so without these six lines this control would not elaborate.
  output logic                     cmd_draw_posed_o,
  output logic [15:0]              cmd_draw_clip_id_o,
  output logic [15:0]              cmd_draw_frame_no_o,
  output logic [ 7:0]              cmd_draw_sub_o,
  output logic [31:0]              cmd_exec_posed_draws_o,
  output logic [31:0]              cmd_exec_pose_clip_refused_o,

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

  // ---- GEOM.SKIN.NORM's world normal and evidence --------------------------
  // GEOM.SKIN.NORM's world normal, OBSERVED. Entry I43 is CLOSED
  // (2026-09-19, owner ruling R2): its consumer is GEOM.LIGHT --
  // `zhao_light_stream`, through `zhao_light_skin_adapter` -- composed below.
  // The ready is the adapter's, so the port that stood here as `_ready_i`
  // is gone; the normal itself still leaves as a TAP, because the smoke
  // bench differences it against a hand computation and a tap costs nothing.
  output logic                    geom_sn_n_valid_o,
  output logic signed [63:0]      geom_sn_n_x_o,
  output logic signed [63:0]      geom_sn_n_y_o,
  output logic signed [63:0]      geom_sn_n_z_o,
  output logic                    geom_sn_n_degenerate_o,
  output logic [15:0]             geom_sn_n_src_id_o,
  output logic [31:0]             geom_sn_vertices_o,
  output logic [31:0]             geom_sn_degenerate_o,
  output logic [31:0]             geom_sn_reduced_o,
  // The fork's own cost, made visible rather than argued. It counts cycles in
  // which GEOM.POSE's palette held a vertex that GEOM.SKIN was ready for and
  // GEOM.SKIN.NORM was not. See I43 for why that number is expected to be
  // large and what it means.
  output logic [31:0]             geom_sn_fork_stall_o,

  // ---- GEOM.LIGHT (owner ruling R2: `zhao_light_stream` owns vertex light) --
  // The prepared descriptor bank is loaded by COMMAND since owner ruling R25
  // (entry I48, CLOSED 2026-09-19): SetEnvironment 0x0311 -> CMD.EXEC ->
  // GEOM.LIGHT.ENV (`zhao_light_env`) -> the bank, and the power-on default is
  // the same path applied to 4a's default record. The host ports that stood
  // here are gone; the published generation stays as evidence.
  output logic                    geom_light_cfg_gen_o,
  // GEOM.LIGHT.ENV's evidence: bank loads published (the power-on load
  // included), SetEnvironment records taken, and records replaced before they
  // were loaded (fired in tests/geometry/light_env_directed.cpp case 4).
  output logic [31:0]             geom_light_env_loads_o,
  output logic [31:0]             geom_light_env_records_o,
  output logic [31:0]             geom_light_env_superseded_o,
  // The lit vertex RGB, OBSERVED. Its consumer is GEOM.VATTR (entry I46,
  // CLOSED 2026-09-19): the ready is the store's, so the port that stood here
  // as `_ready_i` is gone and the store's ready leaves as a tap beside it, so
  // the smoke bench can count the handshakes it checks against the reference.
  output logic                    geom_light_valid_o,
  output logic                    geom_light_ready_o,
  output logic [16:0]             geom_light_r_o,
  output logic [16:0]             geom_light_g_o,
  output logic [16:0]             geom_light_b_o,
  output logic                    geom_light_degenerate_vtx_o,
  output logic [15:0]             geom_light_src_id_o,
  // Evidence: the lit count, the adapter's narrowing refusals, and every
  // FAULT counter the service has. Its throughput observers (slot and
  // backpressure clocks) stay inside; its directed test is where they are read.
  output logic [31:0]             geom_light_vertices_lit_o,
  output logic [31:0]             geom_light_degenerate_o,
  output logic [31:0]             geom_light_cfg_refused_o,
  output logic [31:0]             geom_light_epoch_refusals_o,
  output logic [31:0]             geom_light_seam_mismatch_o,
  output logic [31:0]             geom_light_tag_mismatch_o,
  output logic [31:0]             geom_light_root_queue_overflow_o,
  output logic [31:0]             geom_light_rgb_sat_o,
  output logic [31:0]             geom_light_nlights_clamped_o,
  output logic [31:0]             geom_light_adapter_refused_o,

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

  // ---- I11 IS CLOSED: GEOM.GROUP_SEQ's job, handle and release are INTERNAL.
  // The job comes from the meshlet dispatcher fork, the handle goes to
  // GEOM.REPLAY and the release comes back from it. Sixteen ports left this
  // list rather than being driven; see the closed ledger in the header.

  // ---- I12 IS CLOSED (owner ruling R27): no arena origin is owed in v1 -----
  // The eight geom_org_* / geom_rep_org_* ports left the list; see the ledger.

  // ---- GEOMETRY evidence ---------------------------------------------------
  output logic [31:0]             geom_groups_opened_o,
  output logic [31:0]             geom_groups_sealed_o,
  // client-A accepts, one per vertex PER VIEW (2x the vertices in dual view)
  output logic [31:0]             geom_view_vertices_sent_o,
  output logic [31:0]             geom_landings_o,
  output logic [31:0]             geom_jobs_refused_o,
  output logic [31:0]             geom_alloc_stall_cycles_o,
  output logic [31:0]             geom_rel_unheld_o,
  output logic                    geom_seal_early_o,
  // R31: GEOM.VDECODE's refusals as GEOM.GROUP_SEQ absorbs them. A hole is a
  // record that will never arrive; its batch is poisoned and dropped whole,
  // and an EARLY hole is one that arrived with no batch held and was carried
  // to the next (structurally excluded by ASSETFETCH's S_HAND -> S_SERVE
  // handshake, so 0 here; fired by stimulus in the directed test).
  output logic [31:0]             geom_holes_o,
  output logic [31:0]             geom_groups_poisoned_o,
  output logic [31:0]             geom_holes_early_o,
  output logic [31:0]             geom_arena_hits_o,
  output logic [31:0]             geom_arena_misses_o,
  output logic [31:0]             geom_arena_refusals_o,
  output logic                    geom_arena_overflow_o,

  // ---- I24 IS CLOSED: the cull mode is the TRIANGLE'S OWN -----------------
  // GEOM.CLIP takes `rp_o_raster[1:0]`, the word GEOM.REPLAY presents beside
  // the corners, which travelled from the draw with the meshlet. The port that
  // stood here is GONE.

  // ---- GEOM.VATTR's evidence (entry I46 CLOSED; owner rulings R11, R31) ----
  // The vertex-attribute store and its writer are INTERNAL: the eleven
  // `geom_att_*` ports that modelled the store at the edge are gone. What
  // leaves is the store's census and its faults, and the depth law's two
  // faults, which moved here from GEOM.REPLAY with the law itself.
  output logic [31:0]             geom_va_landings_o,
  output logic [31:0]             geom_va_rows_written_o,
  output logic [31:0]             geom_va_colours_written_o,
  output logic [31:0]             geom_va_uv_staged_o,
  output logic [31:0]             geom_va_lq_overflow_o,
  output logic [31:0]             geom_va_index_oob_o,
  output logic [31:0]             geom_va_look_oob_o,
  output logic [31:0]             geom_va_profile_mixed_o,
  output logic [31:0]             geom_va_dq_refused_o,
  output logic [31:0]             geom_va_dq_stray_o,
  // Review of d52ae6c0: depth results that WAITED for their u/v (a handshake,
  // not a fault), and the batch poison GEOM.VATTR adds to GROUP_SEQ's.
  output logic [31:0]             geom_va_uv_waits_o,
  output logic                    geom_va_poison_o,
  // OWNER RULING R88: `u_geom_vattr.done_o` gates BOTH sides of the
  // GROUP_SEQ -> REPLAY handshake (:7354 and :13722), and two of its six terms
  // are count equalities a producer can leave open forever -- a batch whose
  // vertices are never lit wedges the WHOLE geometry front end. There is no
  // timeout (releasing early would serve REPLAY rows that were never written)
  // so there is a WATCHDOG instead: one count per episode in which the store
  // owed something, every machine in it was idle, and nothing moved at any of
  // its inputs for its `STALL_LIMIT` clocks. Zero on every healthy frame.
  output logic [31:0]             geom_va_done_stall_o,

  // ---- GEOM.REPLAY's evidence ----------------------------------------------
  output logic [31:0]             geom_rp_meshlets_o,
  output logic [31:0]             geom_rp_groups_o,
  output logic [31:0]             geom_rp_triangles_in_o,
  output logic [31:0]             geom_rp_triangles_out_o,
  output logic [31:0]             geom_rp_refused_o,
  output logic [31:0]             geom_rp_missed_o,
  output logic [31:0]             geom_rp_att_skew_o,
  output logic [31:0]             geom_rp_view_bad_o,
  // R31: triangles GEOM.REPLAY dropped because their batch lost a record.
  output logic [31:0]             geom_rp_poisoned_o,
  // R57: a TriangleDescriptor refused because the two-meshlet descriptor queue
  // was full. Backpressure, never a drop -- and the instrument that says the
  // queue's sizing assumption (twice MAX_TRIANGLES) still holds.
  output logic [31:0]             geom_rp_triq_stall_o,

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
  output logic [31:0]             geom_untex_refused_o,
  // GEOM.ATTRPACK's two counters, out of the module for the same reason every
  // other block's are: a counter nobody can read is not evidence. Their RATIO
  // is the thing worth asserting -- `planes` must be exactly three times
  // `triangles`, because one shared attrsetup core runs three lanes per
  // triangle, and a lane that quietly stopped asking would leave every
  // handshake and every other counter looking perfectly healthy.
  output logic [31:0]             geom_attrpack_triangles_o,
  output logic [31:0]             geom_attrpack_planes_o,

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

  // ---- I26 CLOSED 2026-09-19 (terrain3): the spine is ON the TERRAIN.BUILD socket
  // The terrain HPS arbiter's one bridge port and the pool's two guard clients
  // (TERRAIN.PAGELOADER's writes, the compose path's read share) used to stop
  // at this edge. They now reach the shell's REAL `zhao_hps_bridge` and
  // `zhao_mem_guard` through `u_build_share` and the socket's HPS client 1 --
  // see `u_build_share` below. What crosses the edge is the socket share's
  // evidence: contention between its three requesters, and its two tripwires.
  output logic [31:0]             terr_bsock_contention_o,
  output logic [31:0]             terr_bsock_retire_unowned_o,
  output logic [31:0]             terr_bsock_wbeat_unowned_o,

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

  // ---- THE F-SHEET JOURNAL DOORBELL: SW.STREAM's own words (R14, D10) ------
  // NOT A TIE-OFF, and not entry I28 moved sideways: I28 is CLOSED. TERRAIN.SEQ
  // -> the doorbell -> TERRAIN.WRITEBACK -> TERRAIN.RESIDENCY is composed below,
  // and what crosses this edge is the HPS itself -- the journal descriptor, the
  // grants it posts, the tickets the hardware returns and the ACKs it sends.
  // In Verilator the harness IS the HPS (plan D10), exactly as it is for the
  // FRAME_RING view and `terr_cfg_*` above. Owner ruling R14 names SW.STREAM the
  // owner; design/contracts/TERRAIN.WRITEBACK.DOORBELL.md is the exchange.
  input  logic [31:0]             terr_cfg_journal_base_i,   // D0
  input  logic [31:0]             terr_cfg_journal_bytes_i,  // D0
  input  logic                    terr_jdb_post_valid_i,     // D1: a grant
  output logic                    terr_jdb_post_ready_o,
  input  logic [15:0]             terr_jdb_post_slot_i,
  input  logic [31:0]             terr_jdb_post_ticket_i,
  output logic                    terr_jdb_ret_valid_o,      // D2: a return
  input  logic                    terr_jdb_ret_ready_i,
  output logic [31:0]             terr_jdb_ret_ticket_o,
  output logic                    terr_jdb_ret_final_o,
  output logic                    terr_jdb_ret_ok_o,
  output logic [3:0]              terr_jdb_ret_verdict_o,
  input  logic                    terr_jdb_ack_valid_i,      // D3: the ACK
  output logic                    terr_jdb_ack_ready_o,
  input  logic [31:0]             terr_jdb_ack_ticket_i,
  input  logic                    terr_jdb_ack_ok_i,
  // ...and the evidence both blocks keep. Events and cycles named apart.
  output logic [31:0]             terr_wb_sheets_written_o,
  output logic [31:0]             terr_wb_sheets_refused_o,
  output logic [31:0]             terr_wb_sheets_faulted_o,
  output logic [31:0]             terr_wb_guard_denied_o,
  output logic [31:0]             terr_wb_acks_unmatched_o,
  output logic [31:0]             terr_wb_acks_overdue_o,
  output logic [31:0]             terr_jdb_starved_cycles_o,
  output logic [31:0]             terr_jdb_ret_overflow_o,

  // ==========================================================================
  // THE TERRAIN COMPOSE ENGINE'S OWN BOUNDARY (connected item 10)
  // ==========================================================================

  // ---- I26 (extended) CLOSED 2026-09-19: the compose path's read share is
  // requester 2 of `u_build_share`, on the shell's slot-6 socket. Its ports
  // left this list with the rest of I26.

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

  // ---- TERRAIN.HDRREAD's evidence (composed item 13) ----------------------
  // The patch header reader entry I35 named as the absent owner.  These are
  // the numbers that separate "the placement was fed" from "the placement was
  // fed SOMETHING": `terr_hr_headers_o` counts headers that returned and
  // passed their identity test, and every other counter here is a distinct
  // reason a patch was refused instead.  Their SUM against
  // `terr_place_patches_o` is the assertion worth making -- a header this
  // block refused arrives at TERRAIN.PLACE as an impossible pitch, so
  // `terr_hr_*` and `terr_place_pitch_bad_o` must move together or one of the
  // two is lying.
  output logic [31:0]             terr_hr_headers_o,
  output logic [31:0]             terr_hr_refused_o,
  output logic [31:0]             terr_hr_guard_denied_o,
  output logic [31:0]             terr_hr_incomplete_o,
  output logic [31:0]             terr_hr_ident_fails_o,
  output logic                    terr_hr_idle_o,

  // ---- THE READ SHARE's evidence (composed item 13) ----------------------
  // `zhao_mem_share2` joining TERRAIN.HDRREAD (A) and TERRAIN.PAGESTREAM (B)
  // onto the one guard read client.  `terr_rdshare_contention_o` is the number
  // that says what the sharing COST: it moves once per cycle in which both
  // readers asked and one was held.  It is exported rather than counted
  // privately because the decision to widen this to two outstanding requests
  // has to be made against a number, which is the block's own stated reason
  // for having it.
  output logic [31:0]             terr_rdshare_jobs_a_o,
  output logic [31:0]             terr_rdshare_jobs_b_o,
  output logic [31:0]             terr_rdshare_jobs_wb_o,  // 2: TERRAIN.WRITEBACK
  output logic [31:0]             terr_rdshare_denied_o,
  output logic [31:0]             terr_rdshare_contention_o,
  output logic [31:0]             terr_rdshare_err_short_o,
  output logic [31:0]             terr_rdshare_err_long_o,
  output logic [31:0]             terr_rdshare_err_unowned_o,

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
  // Client 2, TERRAIN.WRITEBACK's journal writes (owner ruling R4's N-client
  // arbiter). Its wait is the number that says what the loader costs it.
  output logic [31:0]             terr_hps_c2_bursts_o,
  output logic [31:0]             terr_hps_c2_wait_cycles_o,
  // Client 3, PART.STATE's generation store (`u_part_hps`, entry I1 closed).
  output logic [31:0]             terr_hps_c3_bursts_o,
  output logic [31:0]             terr_hps_c3_wait_cycles_o,
  // Client 4, GEOM.LOOM's node-stream carrier (`u_geom_loomfeed`, I50 closed).
  output logic [31:0]             terr_hps_c4_bursts_o,
  output logic [31:0]             terr_hps_c4_wait_cycles_o,
  // SIX CLIENTS SINCE 2026-09-20 (packet D1, owner decisions FH13/FH15).
  // FIELD's capsule loader takes index 5, the LOWEST, by the same argument
  // clients 2, 3 and 4 make and which the arbiter's own law states: a
  // continuously-asking lower index starves every higher one, so a burst of
  // page loads makes a program install wait -- visibly, in `c5_wait_cycles`.
  output logic [31:0]             terr_hps_c5_bursts_o,
  output logic [31:0]             terr_hps_c5_wait_cycles_o,
  // Rule 6c / R55: a second, DIFFERENT request offered by a client whose
  // pending slot is already occupied is DROPPED, and used to be dropped in
  // silence. These two are that reading -- a count of distinct dropped
  // offerings and a sticky mask naming the client. Expected zero here, and
  // the arbiter's header argues structurally why; the argument is no longer
  // the only thing standing where the instrument should be.
  output logic [31:0]             terr_hps_pend_dropped_o,
  output logic [5:0]              terr_hps_pend_dropped_mask_o,

  // ---- MEM.UPLOAD, composed on the shell's TERRAIN.BUILD socket ----------
  // Its REQUEST is internal: CMD.EXEC lowers the ratified `PublishResource`
  // onto it (owner ruling R17). These two are CMD.EXEC's upload evidence.
  output logic [31:0]             cmd_exec_uploads_o,
  output logic [31:0]             cmd_exec_upload_overflow_o,
  // R35/R36: SetPost / SetGradeTable, each fired by tests/command/cmd_exec_directed.cpp.
  output logic [31:0]             cmd_exec_post_looks_o,       // looks handed to POST.COMPOSITE
  output logic [31:0]             cmd_exec_grade_entries_o,    // product vectors written
  output logic [31:0]             cmd_exec_post_refused_o,     // records REFUSED (flags/bias/header)
  output logic [31:0]             cmd_exec_grade_overflow_o,   // entries refused for staging room
  // Host configuration, the terrain spine's `terr_cfg_*` shape: the
  // destination region MEM.GUARD's TERRAIN_BUILD arm must also admit (in
  // TERRAIN.PAGE_POOL always; in RENDER.ASSET_POOL through R32's arm, which is
  // bounded by exactly this region), the HPS
  // staging arena the active epoch registered, and that epoch.
  input  logic [31:0]             upl_cfg_region_base_i,
  input  logic [31:0]             upl_cfg_region_bytes_i,
  input  logic [63:0]             upl_cfg_arena_base_i,
  input  logic [31:0]             upl_cfg_arena_bytes_i,
  input  logic [15:0]             upl_cfg_epoch_i,
  // The PUBLICATION, `spec/memory_rules.md` 5f.1's directory row, and the
  // verdict. Observable here as well as consumed inside, so a harness can
  // difference a publication against `zref::mem` without reaching in.
  output logic                    upl_publish_valid_o,
  output logic [ 7:0]             upl_publish_slot_o,
  output logic [15:0]             upl_publish_generation_o,
  output logic [ 7:0]             upl_publish_tag_o,
  output logic [23:0]             upl_publish_index_o,
  output logic [31:0]             upl_publish_base_o,
  output logic [31:0]             upl_publish_extent_o,
  output logic                    upl_done_o,
  output logic [ 7:0]             upl_status_o,
  output logic [15:0]             upl_published_o,
  output logic [127:0]            upl_refused_o,
  output logic [31:0]             upl_hps_wait_o,

  // ---- MATERIAL.RESOLVE (composed 2026-09-19, cmdmem packet, ruling R20) ---
  // I49: its REQUEST and its RESPONSE -- BOUNDARY. Directory and fetch are
  // internal and real; see the entry for the one seam in the way.
  // I49, CLOSED 2026-09-20 (texmat2). The REQUEST and the RESPONSE's ready
  // are INTERNAL: `u_material_window` issues one resolve per distinct material
  // from the triangle's own {material_set, material_id, semantic weight} and
  // consumes the answer. Five ports left this list rather than being driven
  // from constants. The response FIELDS stay as outputs, because a harness
  // differencing a resolve against `zref::material` must be able to read them
  // without reaching inside.
  output logic                    mat_rsp_valid_o,
  output logic [ 2:0]             mat_rsp_status_o,
  output logic                    mat_rsp_has_record_o,
  output logic [255:0]            mat_rsp_record_o,
  output logic [ 7:0]             mat_rsp_quality_tier_o,
  output logic [ 1:0]             mat_rsp_sample_count_o,
  output logic [ 2:0]             mat_rsp_material_recipe_o,
  output logic [ 7:0]             mat_rsp_recipe_weight_o,
  output logic [ 7:0]             mat_rsp_base_binding_o,
  output logic                    mat_rsp_selector_overflow_o,
  output logic [31:0]             mat_rsp_palette_base_o,
  output logic [31:0]             mat_rsp_raster_state_o,
  output logic [ 7:0]             mat_rsp_flags_o,
  output logic [ 7:0]             mat_rsp_sample0_modes_o,
  output logic [ 7:0]             mat_rsp_sample1_modes_o,
  output logic [ 7:0]             mat_rsp_sample2_modes_o,
  output logic [31:0]             mat_hits_o,
  output logic [31:0]             mat_misses_o,
  output logic [31:0]             mat_refused_o,
  output logic [31:0]             mat_refused_id_o,
  output logic [31:0]             mat_refused_record_o,
  output logic [31:0]             mat_not_resident_o,
  output logic [31:0]             mat_selector_overflow_o,
  output logic [31:0]             mat_recipe_count_mismatch_o,
  output logic [31:0]             mat_fetch_denied_o,
  // ---- the WINDOW's evidence (entry I49) ---------------------------------
  // `mat_win_resolves_o` against `mat_win_switches_o` is the "counters see
  // what pictures cannot" reading: a window that re-resolved a material it
  // already held would produce a byte-identical frame and spend the meshlet
  // loop's clocks twice. The two stall counters are split because they have
  // different cures. The last two are STRUCTURAL guards and read zero in any
  // correct composition.
  output logic [31:0]             mat_win_resolves_o,
  output logic [31:0]             mat_win_switches_o,
  output logic [31:0]             mat_win_drain_stall_o,
  output logic [31:0]             mat_win_answer_stall_o,
  output logic [31:0]             mat_win_occupancy_max_o,
  output logic [31:0]             mat_win_no_record_o,
  output logic [31:0]             mat_win_selector_overflow_o,
  // Loud rather than silent: a CLUT material needs the binding page's palette
  // slot and generation as witnesses and NOTHING in this console produces
  // them. See FINDINGS-texmat2's owner decision.
  output logic [31:0]             mat_win_clut_unowned_o,
  output logic [31:0]             mat_win_err_unpublished_o,
  output logic [31:0]             mat_win_err_underflow_o,
  output logic [31:0]             geom_ma_jobs_c_o,
  output logic [31:0]             geom_ma_jobs_d_o,
  output logic [31:0]             geom_ma_jobs_e_o,

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
  // THE DEPTH PROFILE THE RESULT WAS PROJECTED UNDER -- NEW 2026-09-19, and it
  // closes entry I14's depth-profile item. `SetView`'s `flags[1:0]` is
  // the depth profile of the frozen 2026-08-31 ruling; `zhao_project_core` now
  // carries it on cfg address 18 and emits it beside the view, and CMD.EXEC's
  // SetView arm writes it as the seventeenth step of the view walk.
  //
  // IT LEAVES THIS MODULE RATHER THAN BEING CONSUMED HERE, exactly as
  // `proj_a_view_o` does and for the same reason: GEOM.DEPTHQUANT is the
  // consumer -- its `v_profile_i` is this port's width and meaning -- and that
  // block is not composed. `proj_fill_profile_o` is the terrain client's
  // per-VERTEX half; terrain's per-TRIANGLE profile is still owed, because the
  // replay arena carries no profile field and widening it is a change to
  // `zhao_vertex_arena`, not to a composer.
  output logic [1:0]              proj_a_profile_o,
  output logic [1:0]              proj_fill_profile_o,
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

  // ---- TERRAIN's LIT NORMALS: the per-triangle base light (R21) -----------
  // Part of entry I13's terrain triangle packet, not a new boundary: the
  // entry's own sentence says joining terrain to GEOM.CLIP needs "terrain's
  // own attribute packet (invw24 from GEOM.DEPTHQUANT for terrain w, and
  // TERRAIN.SHADE's light)". This is that light, computed here, leaving on the
  // same edge as the triangle it belongs to and tagged with the same src_id.
  // Its producer chain is REAL end to end: the world vertex is stored on the
  // projector's own fill beat, the face normal is `zhao_terrain_normals` and
  // the shade is `zhao_terrain_shade`, with the sun from SetEnvironment
  // through `zhao_light_env` (R25). The consumer is I13's absent merge.
  output logic                    terr_light_valid_o,
  input  logic                    terr_light_ready_i,
  output logic signed [31:0]      terr_light_base_o,
  output logic                    terr_light_degenerate_o,
  output logic [15:0]             terr_light_src_id_o,
  output logic [31:0]             terr_light_refs_taken_o,
  output logic [31:0]             terr_light_emitted_o,
  output logic [31:0]             terr_light_stale_reads_o,
  output logic [31:0]             terr_light_normals_o,
  output logic [31:0]             terr_light_shaded_o,
  output logic [31:0]             terr_light_degenerate_count_o,
  output logic [31:0]             terr_light_base_sat_o,
  output logic [31:0]             terr_light_degen_mismatch_o,
  output logic [31:0]             proj_contended_o,
  output logic [31:0]             proj_mat_refused_o,

  // ---- I17: the compositor's absent neighbours ----------------------------
  // `post_view_sel_i` and the source stream `post_s_*` are GONE FROM THIS EDGE
  // (I15, 2026-09-19): the pass, its view and its pixels come from the shell's
  // `zhao_post_lease`, which reads the back buffer in raster order.
  // THE FOURTEEN post_gd_* / post_gg_* PORTS WERE REMOVED HERE ON MERGE,
  // 2026-09-21 (owner ruling R220). They were entry I17's boundary tie-off;
  // packet POSTGATHER composed zhao_post_gather and they became INTERNAL, so
  // the core no longer has them and `u_dut (.*)` could not bind a name that
  // no longer exists. POSTGATHER updated the wrapper mutant it could see and
  // branched BEFORE this one existed -- the cross product of two lanes landing
  // in the same window.
  //
  // R162 EXACTLY: a wrapper cannot drift in its BODY, but its PORT LIST can,
  // and mutant_copy_drift is blind to that half BY DESIGN -- it passed RC 0
  // on this tree while this control would not elaborate. Caught by RUNNING
  // the control, not by a gate.
  // The `atm_*` GROUP IS GONE FROM THIS EDGE, 2026-09-19. It is now internal:
  // TWOD.PLANE, TWOD.SAMPLER and POST.COMPOSITE are composed at the end of
  // this module and the atmosphere sheet never leaves. Entry I17 records what
  // changed and what did not.
  // THE LOOK AND THE GRADING TABLE ARE GONE FROM THIS EDGE, 2026-09-19 (post
  // pass 2, owner rulings R35/R36): post_bloom_gain_i, post_grade_valid_i,
  // post_pv_*, post_bias_*, post_flash_* and post_ink_rgb_i are driven by
  // CMD.EXEC's SetPost / SetGradeTable arm (section 7c). Entry I17 (a)(b).
  output logic                    post_hud_req_v_o,
  output logic [POST_XW-1:0]      post_hud_req_x_o,
  output logic [POST_YW-1:0]      post_hud_req_y_o,
  input  logic                    post_hud_valid_i,
  input  logic [15:0]             post_hud_rgb_i,
  // `post_o_*` and `post_echo_*` are GONE FROM THIS EDGE (I16, 2026-09-19):
  // the composited stream is written back through RASTER.FBWRITE inside the
  // shell's post lease, and the echo tap feeds POST.ECHO there.

  // ---- POST.COMPOSITE's LEASE and POST.ECHO: evidence ----------------------
  output logic                    post_busy_o,             // armed/running: frame not publishable
  output logic [31:0]             post_passes_o,           // compositor passes written back
  output logic [31:0]             post_frames_o,           // render frames fully post-processed
  output logic                    post_fault_o,            // a refused source read
  output logic [31:0]             post_src_reads_o,        // 64-byte back-buffer reads
  output logic [31:0]             post_src_pixels_o,       // pixels handed to the compositor
  output logic [31:0]             post_retire_unowned_o,   // tripwire: must read 0
  output logic [31:0]             post_share_contention_o, // ENGINE0 share waits
  output logic [31:0]             echo_passes_complete_o,  // WHOLE captures
  output logic [31:0]             echo_passes_torn_o,
  output logic [31:0]             echo_pixels_written_o,
  output logic [31:0]             echo_pixels_dropped_o,
  output logic                    echo_fault_o,

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

  // ---- THE HISTOGRAM. Its EVENTS ARE NO LONGER HERE ------------------------
  // I18 CLOSED 2026-09-20 (owner ruling R70). `hist_ev_valid_i`,
  // `hist_ev_lane_valid_i`, `hist_ev_err_i`, `hist_ev_src_id_i` and
  // `hist_ev_ready_o` LEFT THIS PORT LIST rather than being driven from a
  // harness: `zhao_terrain_lodfeed` is composed below and its deviation
  // records are the events. Its HOST WINDOW went the same way one day earlier
  // -- `hist_rd_*` was entry I19 and is now driven inside this file by
  // `u_hostreg_hist` off the HPS register aperture (section 7b-iii, ruling
  // R51). What is left here is only the block's OUTPUT evidence.

  // ADDED BY THE COORDINATOR ON MERGE, 2026-09-21 (owner ruling R220).
  // POSTGATHER added these core ports and updated the wrapper mutants it
  // could see -- but it branched BEFORE this one existed, so the cross
  // product of two lanes landing in the same window left this port list
  // short and `u_dut (.*)` could not bind. That is R162 exactly: a wrapper
  // cannot drift in its BODY, but its PORT LIST can, and mutant_copy_drift
  // is blind to that half by design -- it passed RC 0 while this would not
  // elaborate. Caught by RUNNING the control, not by a gate.

  // ---- POST.GATHER evidence (composed 2026-09-21, ruling R195) -------------
  // THE FOUR TAG COUNTERS PARTITION THE RESOLVED STREAM, which is what makes
  // them readable together: every accepted fragment lands in exactly one of
  // untagged / below-knee / lit / reserved-channel, and the four must sum to
  // `gather_fragments_o`. A partition is a much stronger instrument than four
  // independent tallies -- one wrong branch breaks the sum, and no single
  // counter can hide a miscount by being read on its own.
  output logic [31:0]             gather_frag_untagged_o,
  output logic [31:0]             gather_frag_below_knee_o,
  output logic [31:0]             gather_frag_lit_o,
  // R195 decision 3's instrument: channels 0b10 and 0b11 are unallocated in
  // the frozen spec, so this reads the number of fragments that asked for a
  // displacement or an ink bit the law does not yet supply. The day
  // `stars_and_flares.md` allocates one, this says whether anything was
  // already drawing it.
  output logic [31:0]             gather_reserved_channel_o,
  output logic [31:0]             gather_fragments_o,
  output logic [31:0]             gather_glow_saturations_o,
  output logic [31:0]             gather_disp_clamps_o,
  output logic [31:0]             gather_cells_flushed_o,
  // ---- the plane store -----------------------------------------------------
  output logic [31:0]             gather_cells_written_o,
  output logic [31:0]             gather_oob_writes_o,     // a cell off the plane
  output logic [31:0]             gather_gd_reads_o,
  output logic [31:0]             gather_gg_reads_o,
  output logic [31:0]             gather_gd_miss_o,
  output logic [31:0]             gather_gg_miss_o,
  // TRIPWIRES, and they are named as such so that nobody quotes their silence
  // without firing them first. `flush_overrun_o` differences the gather's own
  // sixteen-clock flush walk against the raster's 256-pixel tile cadence --
  // two operands, two clocks, nothing in common. `rdw_collide_o` is the
  // instrument for the claim that one plane is enough, i.e. that the raster
  // and post phases never overlap. Both are fired deliberately in
  // `tests/compositor/post_gather_store_directed.cpp`; in the composed
  // console both must read ZERO.
  output logic [31:0]             gather_flush_overrun_o,
  output logic [31:0]             gather_rdw_collide_o,
  output logic [31:0]             gather_plane_commits_o,
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
  // AND THE THREE ATTRIBUTE PLANES LEFT THIS EDGE 2026-09-19, by the same act
  // and for the same reason: `zhao_geom_attrpack` is composed below and is
  // their producer. They were never a boundary in the sense the other entries
  // mean -- the arithmetic was in the tree the whole time, in
  // `zhao_geom_attrsetup`, with no block in front of it to ask three times.
  //
  // `tri_flat_request_i` STAYS AT THE EDGE and is not an oversight. It is the
  // MATERIAL RECORD, and its owner is MATERIAL.RESOLVE, which is BUILT
  // (`fpga/rtl/texture/zhao_material_resolve.sv`, UNIT_VERIFIED, 91 directed
  // checks) and NOT COMPOSED. This line read "`maturity: SPECIFIED` with both
  // tests PLANNED -- NOT WRITTEN and a note blocking it on a cartridge
  // decision" until 2026-09-19; all three clauses had gone stale, the cartridge
  // one by sixteen days. What it waits on is a `spec/memory_rules.md` 5f
  // sentence naming the residency directory's KEY. See entry I20.
  // `tri_flat_request_i` LEFT THIS LIST 2026-09-20 (entry I49). It is built a
  // few thousand lines below from MATERIAL.RESOLVE's published answer, exactly
  // as `tri_area2_i` and the three attribute planes were retired before it.
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

  // ---- INPUT.SNAC's physical edge (owner ruling R7, input_rules.md 7) -----
  // PINS OF THE PART, NOT A BOUNDARY. Exactly the class `pad_buttons_i` above
  // and `hps_req_*` are in, and for the same reason the HOST.REGWIN
  // composition states at length further down: the far end is a connector on
  // the board, not a block nobody has built. In Verilator the harness IS the
  // controller, as it is the HPS for the burst bridge.
  //
  // The adapter is composed below, BETWEEN these pads and the shell, so a
  // slot with a real PS1 pad on it is driven by that pad and every other slot
  // carries `pad_*_i` through untouched. With nothing plugged in -- DAT idles
  // high, every poll times out -- the merge is the identity and this console
  // behaves exactly as it did before the block existed.
  input  logic [SNAC_PORTS-1:0] snac_dat_i,
  input  logic [SNAC_PORTS-1:0] snac_ack_n_i,
  output logic [SNAC_PORTS-1:0] snac_att_n_o,
  output logic                  snac_clk_o,
  output logic                  snac_cmd_o,
  output logic [3:0]            snac_present_o,
  output logic [63:0]           snac_polls_o,
  output logic [63:0]           snac_timeouts_o,
  output logic [63:0]           snac_bad_header_o,
  output logic [63:0]           snac_overrides_o,
  output logic [63:0]           snac_seq_gaps_o,

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
  // ---- TEXTURE EVIDENCE (entry I49, 2026-09-20, texmat2) ------------------
  // Promoted from inside the raster tile pipe, where seven of these dangled at
  // the shell's instantiation and the eighth was sunk as an unused wire. The
  // composed console could not previously answer the only question that
  // matters at this seam: DID THE ISLAND SAMPLE. It can now, and the answer is
  // measured rather than argued.
  output logic [31:0] render_texture_fragments_o,
  output logic [31:0] render_texture_cache_hits_o,
  output logic [31:0] render_texture_cache_misses_o,
  output logic [31:0] render_texture_palette_lookups_o,
  output logic [31:0] render_texture_plan_accepted_o,
  output logic [31:0] render_texture_dispatch_accepted_o,
  output logic [31:0] render_texture_combine_refused_o,
  output logic [31:0] render_texture_samples_o,

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
  // (I30's OPEN HALF was here: surf_cmd_env_* -- the patch envelope, which
  //  entry I27 recorded as having no placement owner anywhere in the tree --
  //  and the three policy bits no opcode carries. CLOSED 2026-09-19 under
  //  owner ruling R45: u_surface_dispatch resolves the patch by the SAME
  //  world->patch law zhao_terrain_heighttap inverts, from the stamp's own
  //  translation and the live pitch, and carries the policy in three named
  //  parameters with blend_en = 0 as R45 ratifies. surf_cmd_field_en_i went
  //  with them: the policy was already "a stamp program is resident", so the
  //  residency IS the producer and the host had nothing to add.)
  input  logic        [15:0] surf_cmd_src_id_i,
  // The dispatch's evidence.
  output logic [31:0]        surf_disp_dispatched_o,
  output logic [31:0]        surf_disp_pitch_refused_o,
  output logic [31:0]        surf_disp_env_clamped_o,
  output logic signed [15:0] surf_disp_patch_ix_o,
  output logic signed [15:0] surf_disp_patch_iz_o,
  // The rectangle itself, because a patch index alone cannot be checked
  // against the stamp's own geometry and an unchecked envelope is how a stamp
  // lands somewhere plausible and wrong.
  output logic signed [31:0] surf_disp_env_x0_o,
  output logic signed [31:0] surf_disp_env_x1_o,

  // I31 CLOSED 2026-09-19. SURFACE.STAMP's field-driven brush is driven from
  // INSIDE this module now: `u_field_stamp_adapter` walks the stencil and
  // `u_field_host` runs the program. The four ports are GONE from this edge
  // rather than driven from it, which is the difference between a seam that
  // closed and a seam that acquired a producer.

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
  // An OWNER DECISION sits on this one port, found 2026-09-20 (post3) and
  // written here rather than only in a run folder, because a run folder is the
  // wrong home for anything durable.
  //
  // This is PART.LADDER's `p_gov_floor_i`, and PART.LADDER is COMPOSED -- so it
  // is the ONE composed consumer MEASURE.GOVERNOR has. It is nevertheless not
  // wired, because the two ends do not mean the same thing and making them
  // agree is a policy choice nobody has made:
  //
  //   * the governor emits `deg0/1_o`, a DEGRADE RUNG 0..3 whose law (G2) is
  //     "multiply the allowed pixel error by 2^deg";
  //   * this port is a FLOOR on a 0..5 particle ladder whose rungs are chosen
  //     by SCREEN SIZE thresholds, and `zhao_part_ladder.sv:108` says of its
  //     own reading "this reading of it is AN INTERPRETATION ... the contract
  //     says this block consumes 'governor targets' and does not say by what
  //     mechanism".
  //
  // A degrade rung is not a rung of that ladder, and the DERIVED mapping would
  // scale the ladder's size thresholds by 2^deg rather than clamp the result --
  // which this port cannot express. Writing `deg -> floor` here would be a
  // composer inventing the policy, and it would be invisible once written.
  //
  // CITATION REPAIRED 2026-09-20 (post3b). This line read "See FINDINGS-post3.md
  // for the evidence and the recommendation" and THAT FILE DOES NOT EXIST --
  // not in the run folder, not anywhere in the tree. A pointer to nothing reads
  // as coverage exactly the way a false `reference_model:` does (R94/R105), and
  // no gate looks at a citation in a comment. The evidence is in
  // `runs/CLAUDE-RUNS/RUN-20260919-1656-gaps-to-zero/FINDINGS-post3b.md`.
  //
  // AND THE DECISION IS SHARPER THAN "NOBODY CHOSE", which is how it was
  // recorded above. Searched 2026-09-20 (post3b), and the governor's OWN
  // RATIFIED CONTRACT SETTLES IT AGAINST THE WIRING:
  //
  //   * `design/contracts/MEASURE.GOVERNOR.md`'s output table has THREE columns
  //     -- port, width, and THE CONSUMER PORT IT DRIVES. Read down the third:
  //       `cam0_scale_o` `cam1_scale_o` | 16 | `cam0_scale_i` / `cam1_scale_i`
  //       `cam0_en_o` `cam1_en_o`       |  1 | `cam0_en_i` / `cam1_en_i`
  //       `hyst_o`                      | 16 | `hyst_i`
  //       `min_hold_o`                  |  8 | `min_hold_i`
  //       `morph_step_o`                | 17 | `morph_step_i`
  //       `src_id_o`                    | 16 | rides the decision
  //       `deg0_o` `deg1_o`             |  2 | -- (capture / post-mortem)
  //     EVERY policy output NAMES A CONSUMER PORT. `deg0_o`/`deg1_o` name NONE,
  //     and the dash is spelled out as "capture / post-mortem". Note also WHICH
  //     block each named consumer port belongs to: `cam*_scale_i`, `cam*_en_i`,
  //     `hyst_i`, `min_hold_i` and `morph_step_i` are all TERRAIN.LOD's. The
  //     governor's ratified output table is written ENTIRELY against TERRAIN.LOD
  //     and gives PART.LADDER NOTHING.
  //   * `design/contracts/PART.LADDER.md` never contains the words "deg",
  //     "floor" or "degrade" at all. Its `:20` in-packet names a
  //     `governor_target` with NO units, NO range and NO law, and its `:101`
  //     failure row ("governor target unattainable at the lowest rung")
  //     assumes a target in the LADDER's own currency, not a rung count.
  //   * `design/blocks.yml:1650` / `:5026` assert only the EDGE
  //     (MEASURE.GOVERNOR -> PART.LADDER) and the abstract packet name
  //     `lod_targets`. An edge is not a field mapping.
  //
  // SO THE FINDING IS SHARPER THAN "NOBODY CHOSE A MAPPING", which is how it
  // was recorded before. THE LEDGER ASSERTS AN EDGE THAT NEITHER CONTRACT
  // REALISES AT PORT LEVEL: the producer's table routes every policy output to
  // a DIFFERENT block and rules its remaining two ports out of policy
  // altogether, while the consumer's contract never names the quantity at all.
  // Neither the composer nor either block may invent it.
  //
  // RECOMMENDATION (owner's call; see the FINDINGS file above). The
  // dimensionally correct mapping is NOT a floor. `deg` multiplies the ALLOWED
  // PIXEL ERROR by 2^deg (governor law G2), and PART.LADDER picks its rung from
  // SCREEN-SIZE thresholds in U8.8 pixels -- so the faithful conversion shifts
  // `MESHLET_MIN..GLINT_MIN` LEFT by `deg` (a particle must be 2^deg times
  // bigger to earn the same rung), which degrades smoothly across all six rungs
  // and needs no rounding. `p_gov_floor_i` cannot express that: it is a clamp,
  // so it collapses a whole population onto one rung the moment it bites. That
  // argues for a new `p_deg_i[1:0]` on PART.LADDER rather than a deg->floor
  // table, and it is an ART knob (how hard particles coarsen under pressure),
  // so CLAUDE.md rule 6 puts it in a named editable constant and CLAUDE.md's
  // art law puts the value in the owner's eye, not in a derivation.
  //
  // NOTE ALSO, for whoever fixes it: `zhao_part_ladder.sv:85-86` says "The
  // rungs, coarse to fine ... 'coarser' is 'numerically smaller'", and BOTH
  // halves of that are backwards against the localparams directly beneath it
  // (MESHLET = 0 is the FINEST, CULLED = 5 the coarsest) and against the
  // `max()` on line 121, which forces COARSER. The arithmetic is right and the
  // prose is wrong -- the same prose/arithmetic inversion the governor's own
  // header reports inside TERRAIN.LOD. Reading the comment and wiring to it
  // would invert the policy.
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
  // R68 sub-build 4: a client-A result came back owned by neither GEOM nor
  // PART. Unreachable until a third owner is minted, and exported anyway --
  // the whole point of widening the field is that a third owner is coming, and
  // a drop that nothing counts would surface as a missing vertex in the arena.
  output logic [31:0] part_prj_owner_unroutable_o,
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
  // DEBUG.TRACE's evidence.  Added 2026-09-19 with the ring.
  // --------------------------------------------------------------------------
  // OUTPUTS ONLY as of 2026-09-20. The arming and the host readout that stood
  // here as entry I45's BOUNDARY are both closed INSIDE this file now:
  //
  //   arming   CMD.EXEC lowers `DebugTraceArm` 0xF003 (owner ruling R52) and
  //            drives `arm_we`/`arm_mask`/`clear` at section 7c.
  //   readout  `u_hostreg_trace` answers the HPS register aperture at section
  //            7b-iii (owner ruling R51), which is the same carrier that closed
  //            MEASURE.HISTOGRAM's I19 in the same pass -- I45's own text asked
  //            for exactly that: "Closing one closes both, and they should be
  //            closed together rather than twice."
  //
  // These three remain because they are what a BENCH reads. `armed_o` is also
  // readable by the host at aperture word 0x1800, which is the useful asymmetry:
  // a host can confirm what the command stream armed without being able to arm
  // behind its back (ruling R18, one authority per level).
  output logic [ 6:0] dbg_trace_armed_o,
  output logic [31:0] dbg_trace_count_o,
  output logic [31:0] dbg_trace_dropped_o,

  // --------------------------------------------------------------------------
  // HOST.REGWIN -- the HPS lightweight-bridge CSR aperture (owner ruling R51).
  // --------------------------------------------------------------------------
  // THE CONSOLE'S SECOND HOST PORT, and it is a physical edge of the part in
  // the same class as `hps_req_*` and `pad_buttons_i`, not a boundary standing
  // in for something unbuilt. On the Cyclone V SoC the HPS drives two bridges:
  // the h2f DATA bridge, which `zhao_hps_bridge` uses for 64-byte bursts, and
  // this narrow 32-bit lightweight bridge, whose entire purpose is the ARM
  // reading and writing FPGA registers one word at a time.
  //
  // The aperture is 64 KiB of byte address, sixteen 4 KiB tenant regions, and
  // the map is FROZEN in `spec/memory_rules.md` section 8. Two tenants are
  // populated: MEASURE.HISTOGRAM at 0x0000 and DEBUG.TRACE at 0x1000. Every
  // other region, every misaligned address and every write is REFUSED with a
  // response and counted -- it never hangs, ruling R20's law.
  //
  // No-escape is STRUCTURAL rather than checked: the word offset handed to a
  // tenant is TENANT_LSB-2 bits wide, so there is no wire on which one tenant
  // could be given another's address. See the block's own header.
  input  logic        hostreg_valid_i,
  input  logic        hostreg_write_i,
  input  logic [15:0] hostreg_addr_i,     // byte address within the aperture
  input  logic [31:0] hostreg_wdata_i,
  output logic        hostreg_ready_o,
  output logic        hostreg_rvalid_o,
  output logic [31:0] hostreg_rdata_o,
  output logic        hostreg_err_o,
  output logic [31:0] hostreg_reads_o,
  output logic [31:0] hostreg_writes_o,
  output logic [31:0] hostreg_refused_unmapped_o,
  output logic [31:0] hostreg_refused_misaligned_o,
  output logic [31:0] hostreg_refused_tenant_o,
  output logic [31:0] hostreg_refused_timeout_o,
  output logic [31:0] hostreg_stall_cycles_o,

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
  // A SetView whose viewport_id names no viewport in the mode the last
  // contract set (video_rules 3.2). Carried over verbatim 2026-09-20 with
  // the port block, as this wrapper's header requires.
  output logic [31:0] cmd_exec_viewport_refused_o,
  output logic [31:0] cmd_exec_src_truncated_o,
  output logic [31:0] cmd_exec_unsupported_o,
  // R25: committed SetEnvironment records handed to GEOM.LIGHT.ENV.
  output logic [31:0] cmd_exec_envs_o,
  // R52: committed DebugTraceArm records handed to DEBUG.TRACE, and the ones
  // REFUSED for a reserved bit set on the wire. The second is the interesting
  // one: it is the guard that keeps a stray bit from arming a set of stages
  // nobody asked for, and the smoke fires it deliberately.
  output logic [31:0] cmd_exec_trace_arms_o,
  output logic [31:0] cmd_exec_trace_arm_refused_o,

  // ==========================================================================
  // MEASURE.TOKENS (rulings R18/R33, 2026-09-19, cmdmem packet)
  // ==========================================================================
  // Its BUDGET side is composed: CMD.EXEC commits SetPresentationContract's
  // five counts as the CEILING and each SetView's two counts as that view's
  // REQUEST, which the guard clamps to the ceiling. Its REQUEST/RETURN side is
  // a BOUNDARY (entry I18): the one consumer that exists, GEOM.BINNER, offers a
  // ONE-BIT token client (`tok_req_o`, tied off inside the shell) against this
  // block's seven-field request, and the other five fields are policy nobody
  // produces yet. So the request and return enter here, and everything the
  // guard decides leaves here, where a bench -- or the next packet -- reads it.
  input  logic        tok_req_valid_i,
  input  logic        tok_req_view_i,
  input  logic        tok_req_class_i,
  input  logic        tok_req_essential_i,
  input  logic [ 2:0] tok_req_rep_i,
  input  logic [31:0] tok_req_cost_i,
  input  logic [15:0] tok_req_src_id_i,
  output logic        tok_grant_o,
  output logic        tok_shared_o,
  input  logic        tok_ret_valid_i,
  input  logic        tok_ret_view_i,
  input  logic        tok_ret_class_i,
  input  logic        tok_ret_shared_i,
  input  logic [31:0] tok_ret_cost_i,
  output logic        tok_den_valid_o,
  output logic        tok_den_view_o,
  output logic        tok_den_class_o,
  output logic [ 2:0] tok_den_rep_o,
  output logic [ 1:0] tok_den_reason_o,
  output logic [15:0] tok_den_src_id_o,
  output logic [31:0] tok_den_cost_o,
  output logic [31:0] tok_avail_geom0_o,
  output logic [31:0] tok_avail_geom1_o,
  output logic [31:0] tok_avail_frag0_o,
  output logic [31:0] tok_avail_frag1_o,
  output logic [31:0] tok_avail_shared_o,
  output logic [31:0] tok_rep_count0_o,
  output logic [31:0] tok_rep_count1_o,
  output logic [31:0] tok_rep_count2_o,
  output logic [31:0] tok_rep_count3_o,
  output logic [31:0] tok_rep_count4_o,
  output logic [31:0] tok_rep_count5_o,
  output logic [31:0] tok_rep_count6_o,
  output logic [31:0] tok_rep_count7_o,
  output logic [31:0] tok_triangles_culled_o,
  output logic [31:0] tok_vreq_clamped_o,
  output logic [31:0] cmd_exec_contracts_o,

  // ==========================================================================
  // TERRAIN.MIPFEED / TERRAIN.MIPGEN -- THE SECOND COMPLETION.  Added
  // 2026-09-19 with composition item 12.
  // ==========================================================================
  // The counters are here because this chain's whole purpose is a STATE
  // TRANSITION inside the directory, and a state transition has no other
  // symptom.  `terr_res_resident_o` rising from zero is the result; these say
  // which block produced it.
  output logic [31:0]  terr_mip_pages_mipped_o,
  output logic [31:0]  terr_mip_pages_faulted_o,
  output logic [31:0]  terr_mip_samples_sent_o,
  output logic [31:0]  terr_mipreq_requests_o,
  output logic [31:0]  terr_mipreq_issued_o,
  output logic [31:0]  terr_mipreq_drops_o,
  output logic [31:0]  terr_psmux_a_jobs_o,
  output logic [31:0]  terr_psmux_b_jobs_o,
  output logic [31:0]  terr_psmux_stray_v_o,
  output logic [31:0]  terr_psmux_stray_done_o,

  // THE COARSE-HEIGHT MIP PLANES ARE RETIRED, owner ruling R64, 2026-09-20.
  // Entry I44 carried them as a boundary. They are gone, and the argument is
  // in that entry's closure note; the short form is that ruling T8's
  // decimation is NESTED and UNROUNDED, so `mip17[i,j] == fine33[2i,2j]` bit
  // for bit and a coarse vertex IS a fine vertex. A plane store could
  // therefore never produce a number the fine lattice does not already
  // contain -- it is a DUPLICATE PROVIDER, not a source -- and the bit
  // identity is now a committed test (`tests/terrain/
  // terrain_mipgen_directed.cpp` case 3b) rather than prose in a contract.
  //
  // THE COUNTERS STAY, and that is deliberate rather than an oversight. They
  // are the only remaining evidence at this boundary that the decimation ran
  // at all, and without them the block's whole coarse path would be pruned
  // silently -- which is the shape this file's own I32 entry warns about for
  // layer D. TERRAIN.MIPGEN also stays composed: its `done_o` is
  // TERRAIN.RESIDENCY's SECOND COMPLETION, and without it `resident_o` is
  // structurally zero and no patch ever reaches the compose door.
  output logic [31:0]  terr_mg_m17_writes_o,
  output logic [31:0]  terr_mg_m9_writes_o,
  output logic [31:0]  terr_mg_aborts_o,

  // ---- TERRAIN.LODFEED, and these four are why entry I18 can be read at all
  // Composed 2026-09-20 under owner ruling R70: `zhao_terrain_lodfeed` observes
  // the mip pass's fine stream and its records are MEASURE.HISTOGRAM's events.
  // The chain is entirely internal, so WITHOUT THESE COUNTERS a console-level
  // bench could not tell "the histogram saw no events because the metric is
  // broken" from "because no page was ever mipped" -- and those two need
  // different repairs.
  //
  // THEY READ ZERO IN THE SMOKE AND THAT IS THE MEASURED, EXPLAINED ANSWER,
  // not an unexamined zero: every page the bench plays fails its CRC (the
  // directory reports `crc_fail=3`), so `tpl_fin_ok` never rises, so
  // TERRAIN.MIPREQ issues no job, so TERRAIN.MIPFEED never streams a lattice.
  // The smoke prints the whole chain of zeros on one line for exactly this
  // reason. The counters are FIRED, non-zero, by
  // `tests/terrain/terrain_lodhist_directed.cpp`, which drives the same
  // arrangement with a lattice that moves.
  output logic [31:0]  terr_lodfeed_lattices_walked_o,
  output logic [31:0]  terr_lodfeed_lattices_dropped_o,
  output logic [31:0]  terr_lodfeed_dev_records_o,
  output logic [31:0]  terr_lodfeed_stray_samples_o,

  // ==========================================================================
  // THE FIELD ENGINE'S EDGE. I42, and it is ONE entry where there were THREE.
  // ==========================================================================
  // THE FIELD PROGRAM DOORBELL -- SW.STREAM's own words, owner ruling R43.
  // NOT A TIE-OFF, and not entry I42 moved sideways: I42 is CLOSED. The
  // loader words and both directory phases are driven by
  // `zhao_field_doorbell` below, and what crosses THIS edge is the HPS
  // itself -- the plan's epoch identity, the posts it makes and the ticketed
  // returns hardware hands back. In Verilator the harness IS the HPS,
  // exactly as it is for `terr_jdb_*` above (owner ruling R14, the pattern
  // R43 names) and for the FRAME_RING view.
  //
  // `post_op_i` is 0 LOAD WORD, 1 COMMIT, 2 LOOKUP, 3 FH2. For a LOAD WORD,
  // `post_kind_i` is the host's own 0 uop / 1 table entry / 2 header /
  // 3 uniform, and the HEADER is written LAST because it is what marks a slot
  // runnable -- so a partially written program can never execute.
  //
  // OP 3 IS THE FH2 TRANSACTION (owner decision FH14, directive section 10.2).
  // It used to fall into the doorbell's catch-all LOAD arm and perform a real
  // loader write for an operation nobody had defined; it is now decoded by name
  // and routed to `u_field_loader` below, sub-decoded by `post_kind_i` as
  // 0 INSTALL_CAPSULE / 1 BIND_PROGRAM / 2 CONTROL / 3 reserved.
  input  logic [31:0]  fld_cfg_plan_base_i,   // D0: held, captured AT ACCEPTANCE
  // FH08's differential control, and NOT a test hook. HIGH forces the blanket
  // per-point register walk back on for every point regardless of the image's
  // INIT_PROOF; directive 6.1 keeps the slow clear "for differential testing
  // and explicit legacy unvalidated bench images only". It is host state on the
  // same footing as `fld_cfg_plan_base_i` -- the producer is SW.STREAM, outside
  // this console -- so it is a boundary input and not a gap.
  //
  // Production may NOT bypass validation to enter the fast path: the host gates
  // that on `hdr_ipok` and not on this bit being low, so driving this low is a
  // request, never a permission.
  input  logic         fld_cfg_slow_clear_i,
  input  logic         fld_db_post_valid_i,
  output logic         fld_db_post_ready_o,
  input  logic [ 1:0]  fld_db_post_op_i,
  // THREE BITS AS OF PACKET C1, and the two consumers want different widths
  // on purpose. `zhao_field_host_v2` decodes EIGHT load kinds (0 UOP, 1 TABLE,
  // 2 HEADER, 3 UNIFORM, 4 OUTMAP, 5 ASSOC, 6 INITPROOF, 7 PREPARED) and
  // implements all eight; at two bits, kinds 4..7 had no producer and the new
  // host's ordinal, association and init-proof machinery was unreachable from
  // this console. The FH2 sub-decode above still uses the LOW TWO BITS, which
  // is why the doorbell truncates rather than widening `fh2_kind_o`: the two
  // fields are separate quantities that happen to share a mailbox word, and
  // this file has been bitten once already by writing one onto the other
  // (see `fld_db_post_addr_i`'s eight-against-seven note below).
  input  logic [ 2:0]  fld_db_post_kind_i,
  input  logic [ 2:0]  fld_db_post_slot_i,
  // EIGHT BITS, NOT SEVEN, and the width is the FH2 control verb's. Directive
  // 10.2: "widen that mailbox field explicitly to at least 8 through its
  // wrappers ... Do not write [7:0] onto an unchanged 7-bit port." The LOADER's
  // own address stays LDADDRW = 7; a legacy LOAD whose address does not fit is
  // REFUSED by the doorbell and counted in `fld_db_addr_refused_o`, never
  // narrowed.
  input  logic [ 7:0]  fld_db_post_addr_i,
  input  logic [95:0]  fld_db_post_data_i,
  input  logic [31:0]  fld_db_post_hash_i,
  input  logic         fld_db_post_ok_i,
  input  logic [31:0]  fld_db_post_ticket_i,
  output logic         fld_db_ret_valid_o,
  input  logic         fld_db_ret_ready_i,
  output logic [31:0]  fld_db_ret_ticket_o,
  output logic [ 1:0]  fld_db_ret_op_o,
  output logic         fld_db_ret_ok_o,
  output logic         fld_db_ret_refused_o,
  output logic         fld_db_ret_inserted_o,
  output logic         fld_db_ret_evicted_o,
  output logic [ 2:0]  fld_db_ret_slot_o,
  output logic [31:0]  fld_db_ret_plan_o,
  // FH2 only. `ret_verdict_o` names WHICH refusal (BAD_CRC is not BAD_RANGE),
  // and `ret_handle_o` is the generation-bearing binding the caller uses
  // afterwards -- NOT a raw cache slot, which is FH16's distinction. Both read
  // zero on a legacy LOAD/COMMIT/LOOKUP return, so `ret_op_o` has to be read
  // alongside them.
  output logic [ 3:0]  fld_db_ret_verdict_o,
  output logic [31:0]  fld_db_ret_handle_o,
  output logic [31:0]  fld_db_posts_o,
  output logic [31:0]  fld_db_load_words_o,
  output logic [31:0]  fld_db_lookups_o,
  output logic [31:0]  fld_db_commits_o,
  // A COMMIT for a slot whose HEADER was not written since the last commit.
  // REFUSED, ANSWERED and COUNTED (owner ruling R20) -- the directory is never
  // offered a hash for microcode that is not there.
  output logic [31:0]  fld_db_commits_refused_o,
  // Cycles a post was offered into a full mailbox. HELD, never dropped: owner
  // ruling R55's shape, and the number that says whether POSTS is big enough.
  output logic [31:0]  fld_db_post_stalls_o,
  // Unreachable while the return credit is right, so its zero is an argument
  // and not a measurement. Fired by tests/mutants/zhao_field_doorbell_mutant.sv.
  output logic [31:0]  fld_db_ret_overflow_o,
  // Op-3 posts handed to the loader, and legacy LOAD posts whose address does
  // not fit LDADDRW. The second is reachable with ordinary stimulus (post an
  // address >= 128), so it needs no mutant -- `field_doorbell_directed` fires
  // it and its negative control shows 0x7F still reaching the loader.
  output logic [31:0]  fld_db_fh2_posts_o,
  output logic [31:0]  fld_db_addr_refused_o,

  // ---- THE FH2 TRANSACTIONAL LOADER (FH13 / FH15 / FH16) ------------------
  // `zhao_field_loader` owns the BACKING STORE's descriptor table: installed
  // immutable capsules, their generations, their pin counts and their READY
  // bits. That is deliberately NOT `zhao_field_progcache`, which owns
  // residency of the ACTIVE cache -- FH15's whole point is that the two are
  // different objects with different lifetimes.
  //
  // The staging window is a PARAMETER of the machine rather than a constant
  // here, for the reason `zhao_terrain_pageloader`'s REGION_BASE gives: a block
  // that hard-codes an address the guard also hard-codes gives the owner one
  // knob with two halves and no gate that says so.
  input  logic [31:0]  fld_ldr_stage_base_i,
  input  logic [31:0]  fld_ldr_stage_bytes_i,
  // Publication. A STAGING object is absent from `pub_ready_o` by construction,
  // so nothing downstream can execute a half-filled image even by guessing its
  // index. `pub_pinned_o` is FH16's "acquire and pin at association open".
  output logic [ 7:0]  fld_ldr_pub_ready_o,
  output logic [ 7:0]  fld_ldr_pub_pinned_o,
  input  logic [ 2:0]  fld_ldr_pub_sel_i,
  output logic [31:0]  fld_ldr_pub_handle_o,
  output logic [31:0]  fld_ldr_pub_prog_hash_o,
  output logic [ 7:0]  fld_ldr_pub_gen_o,
  // Evidence, PER CLASS. "The install failed" is not a diagnosis, so a reader
  // can tell an unreachable address from a bad checksum from a full catalogue
  // without a waveform.
  output logic [31:0]  fld_ldr_installs_ok_o,
  output logic [31:0]  fld_ldr_installs_failed_o,
  output logic [31:0]  fld_ldr_binds_ok_o,
  output logic [31:0]  fld_ldr_binds_failed_o,
  output logic [31:0]  fld_ldr_controls_ok_o,
  output logic [31:0]  fld_ldr_bad_operation_o,
  output logic [31:0]  fld_ldr_bad_envelope_o,
  output logic [31:0]  fld_ldr_bad_range_o,
  output logic [31:0]  fld_ldr_bad_section_o,
  output logic [31:0]  fld_ldr_bad_crc_o,
  output logic [31:0]  fld_ldr_bad_meta_o,
  output logic [31:0]  fld_ldr_bridge_errs_o,
  output logic [31:0]  fld_ldr_no_capacity_o,
  output logic [31:0]  fld_ldr_evictions_o,
  output logic [31:0]  fld_ldr_pin_forced_victim_o,
  output logic [31:0]  fld_ldr_load_bytes_o,

  // CONSOLE POLICY: which resident program is the stamp brush, and whether one
  // is resident at all. The same shape as `surf_cmd_field_en_i` beside it and
  // for the same reason -- no opcode carries either, and I30 already records
  // that an executor filling them in would be choosing values the ABI does not
  // contain.
  input  logic [ 2:0]  fld_stamp_slot_i,
  input  logic         fld_stamp_slot_valid_i,

  // (THE ENGINE'S SECOND CLIENT was here, as `fld_req_*` / `fld_resp_*`. It is
  //  CLOSED 2026-09-20: entry I42 said it "is the seam the FLOW and EARTH
  //  adapters take over when I5 and I34 close", and the FLOW adapter has taken
  //  it. `zhao_field_flow_adapter` is client 1 and `zhao_field_stamp_adapter`
  //  is client 0, so the arbiter's contention is still reachable with legal
  //  stimulus -- two REAL profiles offering in the same cycle, which is what
  //  the edge port was standing in for. `fld_contended_grants_o` below is the
  //  counter that was the entry's reason for keeping it.)

  output logic [31:0]  fld_runs_o,
  output logic [31:0]  fld_run_faults_o,
  output logic [31:0]  fld_noprog_o,
  output logic [31:0]  fld_instr_retired_o,
  output logic [31:0]  fld_loads_o,
  output logic [31:0]  fld_load_defers_o,
  output logic [31:0]  fld_grants_o,
  output logic [31:0]  fld_contended_grants_o,
  // A load word addressed past the uop store. CLAMPED, not wrapped: a wrapped
  // uop write lands on another instruction of the same program, which is silent
  // and produces a plausible field.
  output logic [31:0]  fld_ld_oob_o,
  // A point whose run wrote NOTHING into its declared output window. The lanes
  // then hold the zeroes the front cleared them to, and a caller reading only
  // the lanes could not tell that from a field whose value is zero.
  output logic [31:0]  fld_no_result_o,
  // A point whose run wrote SOME BUT NOT ALL of the lanes its program header
  // declared required (owner ruling R101). Counted apart from
  // `fld_no_result_o` because the answer is then a MIXTURE of real values and
  // cleared zeroes, which is the case a caller cannot see at all -- W10's "do
  // not make an absent output look like a zero result". Both composed clients
  // are exposed to it: the stamp adapter reads window lanes 0-1 and the flow
  // adapter reads lanes 3-5, and until R101 a program that skipped any of them
  // came back 8'h00 SUCCESS.
  output logic [31:0]  fld_out_incomplete_o,
  // ---- THE v2 HOST'S OWN EVIDENCE, new with the composition ---------------
  // These are siblings of the twenty counters around them and have the same
  // standing: the console's counter surface, read by the host. They are NOT
  // new gaps -- a boundary counter is only a tie-off if this file's own
  // "INCOMPLETE -- TIED OFF, AND WHY" block says so, and none of these is in
  // it. Each names one thing and only that thing, which is the discipline
  // R150 was written about.
  //
  // A PREPARED_SCALAR ordinal whose slot was invalid, or whose generation did
  // not match the running association. Apart from `fld_out_incomplete_o`
  // because "the preparation is wrong" and "the program did not write" send
  // the next person to different files.
  output logic [31:0]  fld_prep_bad_o,
  // A load-time refusal: an OUTPUT_MAP row naming a register outside the
  // capture window, which the window cannot observe, so the ordinal could
  // never be seen. Refused at LOAD rather than hanging a point later.
  output logic [31:0]  fld_bad_image_o,
  // R111's standing hazard, counted: a strict descriptor whose ordinal mask is
  // ZERO. A plan writer who omits the mask would otherwise silently restore the
  // R101 defect and pass every gate.
  output logic [31:0]  fld_zero_mask_o,
  // The retirement fence's own positive control. A granted write for the
  // retired context arriving after the fence released. IT MUST READ ZERO --
  // and a detector reading zero is a claim, so it is fired deliberately rather
  // than quoted.
  output logic [31:0]  fld_late_write_o,
  // Writes captured during drain: the ones the oracle loses. NOT a fault. This
  // is the measurement that the fence is doing work, which is the half that
  // separates "the fence never mattered" from "the fence was never tested".
  output logic [31:0]  fld_fence_writes_o,
  // Points that retired with no vector write at all because every declared
  // ordinal was a prepared scalar. FH06's terminal path, counted so that "the
  // uniform route is live" is a number rather than an argument.
  output logic [31:0]  fld_uniform_runs_o,
  // Grants refused for want of a reserved response entry (FH20's credit pool).
  output logic [31:0]  fld_credit_stall_o,
  // FH08's differential pair. Points that took the no-clear fast path, and
  // points that walked. Both are exported because the interesting number is
  // the RATIO, and one of them alone cannot give it.
  output logic [31:0]  fld_fast_path_o,
  output logic [31:0]  fld_slow_path_o,
  // EVERY ALARM THE v3 FABRIC OWNS, UNMERGED AND SEPARATELY COUNTED.
  // `zhao_field_v3_engine`'s own header is right that five faults reduced to
  // one bit is a bit that says "something, somewhere", and a guard that cannot
  // name its own failure gets read as noise.
  output logic [31:0]  fld_exec_desync_o,
  output logic [31:0]  fld_bank_desync_o,
  output logic [31:0]  fld_svc_bank_desync_o,
  output logic [31:0]  fld_tag_mismatch_o,
  output logic [31:0]  fld_wrong_op_o,
  output logic [31:0]  fld_unsupported_o,
  output logic [31:0]  fld_skid_overflow_o,
  output logic [31:0]  fld_uniform_bad_o,
  // {rcp0, sat_rescale, sat_mul, sat_add} -- the op ledger, latched over the
  // run. FOUR causes as of packet C1, not three: owner ruling R145 carried
  // `rcp0` out of the service path, and `zhao_field_host_v2.num_status_o` is
  // the four-bit port that receives it. rcp0 keeps its OWN family per directive
  // 8.1 -- a reciprocal of zero is a defined answer, not a clamp -- so it is a
  // fourth bit here rather than a fourth thing ORed into saturation.
  output logic [ 3:0]  fld_sat_o,
  output logic [31:0]  fld_pc_hits_o,
  output logic [31:0]  fld_pc_misses_o,
  output logic [31:0]  fld_pc_rejected_o,
  output logic [31:0]  fld_pc_evictions_o,
  output logic [ 3:0]  fld_pc_occupancy_o,
  output logic [31:0]  surf_fld_stamps_o,
  output logic [31:0]  surf_fld_texels_o,
  output logic [31:0]  surf_fld_faults_o,
  output logic [31:0]  surf_fld_restarts_o,
  // High while the stencil walk is running. Exported rather than dropped: it
  // is the one signal that separates "the brush produced nothing" from "the
  // brush never started", and those have different causes and different fixes.
  output logic         surf_fld_busy_o
);

  // Every parameter forwarded BY NAME, including the mutated
  // GEOM_REPLAY_UNTEX_DECL above (and TERR_MEMSLOT, which the sibling
  // slot-overflow wrapper derives from ITS knob). Forwarding the whole set
  // rather than relying on production defaults is what keeps the mutation a
  // SINGLE line: change one knob here and every width that depends
  // on it follows, in the wrapper and in the instance alike.
  zhao_console_core #(
      .FRAMER_Q(FRAMER_Q),
      .WFIFO_W(WFIFO_W),
      .SNAC_PORTS(SNAC_PORTS),
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
      .GEOM_REPLAY_UNTEX_DECL(GEOM_REPLAY_UNTEX_DECL),
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

endmodule : zhao_console_core_untex_decl_mutant

`default_nettype wire
