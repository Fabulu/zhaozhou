// tb_zhao_geom_paramarena.sv -- GEOM.PARAMBUF's ARENA, END TO END, THROUGH REAL
// MEMORY.
//
// ===========================================================================
// WHAT THIS COMPOSES, AND WHY EVERY PIECE OF IT IS THE PRODUCTION FILE
// ===========================================================================
//
//   zhao_geom_paramarena  (the PRODUCER, writes) -+
//                                                 +-> zhao_mem_share_wr
//   zhao_geom_paramwalk   (the READER,  reads)  -+     #(.N(2),.CLIENT_ID(3),.RQ(4))
//                                                         |
//                                                         v
//                                                   zhao_mem_guard   (REAL)
//                                                         |
//                                                         v
//                                                   zhao_vram_arbiter (REAL)
//                                                         |
//                                                         v
//                                                   zhao_sdram_ctrl   (REAL)
//                                                         |
//                                                         v
//                                                   zhao_sdram_model  (sim)
//
// THERE IS NO PLAYED FABRIC HERE, and that is the whole point of the file.
// `tb_terrain_lodpath.sv` plays the guard's accept/beat engine because it has
// to construct a DENIAL, a SHORT BURST and a STRAY BEAT -- faults a correct
// fabric never produces.  This bench asks the opposite question: the owner's
// completion ruling of 2026-09-22 item 4 says "do not pack fields into a byte
// vector merely to unpack them again and count that as external-memory
// integration".  A stub memory here would be exactly that.  So the bytes go
// out through the real guard's verdict, the real arbiter's credit law, the
// real controller's SDRAM burst and a behavioural DRAM, and come back the same
// way.  If any of those refuses, mis-addresses or mis-routes, this bench sees
// it as wrong FIELD VALUES and as a counter moving -- not as a picture.
//
// ===========================================================================
// THE GLUE IS TRANSCRIBED FROM `zhao_shell_top_v2.sv`, NOT INVENTED
// ===========================================================================
// Three pieces of the shell's slot-3 (ENGINE1) socket are reproduced here, each
// because the fabric does not work without it and the bench must not be a
// second, kinder design:
//
//   * THE WRITE-DATA QUEUE (`wq`, the shell's `gq`).  `zhao_sdram_ctrl` raises
//     `wr_beat` in S_RW -- the GRANT CYCLE ITSELF on a row hit -- so a queue
//     that was merely "going to be filled" hands the DRAM garbage.
//   * THE WRITE GATE (`wq_room_for_req`, the shell's `gq_room_for_req`).  The
//     arbiter must not accept a write whose words are not ALL already queued.
//     Without it this bench writes garbage and the round trip fails for a
//     reason that has nothing to do with the blocks under test.
//   * THE WRITE-OWNER MUX (`wr_owner_geom_r` / `wr_sel_geom`).  Only one write
//     client exists here, so this could have been a constant -- it is written
//     out in the shell's shape anyway, because a bench whose glue is SIMPLER
//     than the composition it stands for is measuring a machine nobody ships.
//
//   * THE READ-BEAT PACKER and its per-request `last`.  Four 16-bit controller
//     words become one 64-bit beat; `last` marks the end of the GUARD REQUEST
//     (len >> 3 beats), never the end of an arbiter burst -- a 64-byte read is
//     four bursts and the walker must not see four `last` pulses.
//
// ===========================================================================
// NO KNOBS.  THE LEASE IS THE ARENA'S OWN OUTPUT AND NOTHING ELSE
// ===========================================================================
// The guard's `pb_lease_valid`, `pb_wr_view` and `pb_scratch_valid` are driven
// from `zhao_geom_paramarena`'s three output ports, directly, with no bench
// term ORed in anywhere.  That is deliberate and it is load-bearing.
//
// This file briefly carried a `cfg_lease_fix_i` knob that ORed the arena's
// `busy_o` into the lease, because the first composition measured here COULD
// NOT PUBLISH A SINGLE FRAME: `pb_lease_valid_o` was
// `frame_open_q || pub_valid_q`, and the directory write is issued from the
// publication arm, which is reachable only after `frame_end_i` has cleared
// `frame_open_q`.  The guard refused it -- correctly -- at exactly
// SCRATCH_BASE.  Production was repaired (`|| pub_pending_q`, plus releasing
// `scr_mine_q` on the fault path) and THE KNOB WAS DELETED IN THE SAME PASS.
//
// It was deleted rather than left at zero because `busy_o` is a STRICT
// SUPERSET of the real lease: a knob that can widen the permission under test
// is a knob that can hide the next defect in it, and a workaround port nobody
// sets is a knob for a bug that no longer exists which the next person has to
// work out is dead.  If the lease term ever regresses,
// `geom_paramarena_directed` case 1a goes red on the first frame and names the
// file, the line and the repair.
// ===========================================================================
`default_nettype none

module tb_zhao_geom_paramarena
  import zhao_pkg::*;
#(
    // The slot-3 write-data queue, in 16-bit words.  A 64-byte chunk is 32 of
    // them, so 64 holds one whole record with room for the four-word push
    // granularity.  Power of two: the pointer's low bits ARE the index.
    parameter int unsigned WQ_W = 64,
    // The arena's own capacity parameters, exposed so a test can shrink the
    // arena without editing this file.  DEFAULTED TO PRODUCTION'S VALUES so
    // the directed test measures the shipping configuration.
    parameter int unsigned MAX_VERTS    = 65535,
    parameter int unsigned MAX_TRIS     = 16384,
    parameter int unsigned MAX_CHUNKS   = 16384,
    parameter int unsigned CHUNK_IDS    = 14,
    parameter int unsigned WALK_MAX     = 4096,
    // ---- THE ON-CHIP HALF IS OPTIONAL, AND THAT IS THE EVIDENCE ------------
    // BINARENA, 2026-09-26, console entry I55. With `HAVE_ONCHIP = 0` the
    // identity queue, `zhao_geom_binner_v2` and `zhao_geom_chunkser` are NOT
    // ELABORATED, so the elaboration does not need those files at all and the
    // `geom_arenabin_directed` ctest compiles from a source list that does not
    // contain them.
    //
    // That is a stronger claim than "their counters read zero". A counter at
    // zero is a claim about a run; a module absent from the closure is a claim
    // about the CIRCUIT, and it is the claim owner directive section 4 is
    // actually making when it refuses "a parallel legacy on-chip frame arena
    // that still supplies the actual pixels". Both are checked -- the directed
    // test also asserts the on-chip counters are zero in the composed case --
    // but only this one cannot be satisfied by a path that merely happened not
    // to run.
    parameter bit          HAVE_ONCHIP  = 1'b1
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- the arena's frame control ----------------------------------------
    input  var logic        seal_valid_i,
    output var logic        seal_ready_o,
    input  var logic [17:0] seal_verts_i,
    input  var logic [17:0] seal_tris_i,
    input  var logic [17:0] seal_chunks_i,
    // SEALPLAN 2026-09-26: the giant's reservation, in two distinct units.
    // Default-free on purpose -- the driver sets them for every case, so a
    // case that forgets reads zero and its reserve assertions fail loudly
    // rather than inheriting the previous case's number.
    input  var logic [17:0] seal_giant_refs_i,
    input  var logic [17:0] seal_giant_chunks_i,
    input  var logic [15:0] frame_gen_i,
    input  var logic        frame_end_i,

    // ---- the three record intakes, AS FIELDS -------------------------------
    input  var logic               pv_valid_i,
    output var logic               pv_ready_o,
    input  var logic signed [31:0] pv_x_i,
    input  var logic signed [31:0] pv_y_i,
    input  var logic [23:0]        pv_invw_i,
    input  var logic [7:0]         pv_status_i,
    input  var logic signed [31:0] pv_uow_i,
    input  var logic signed [31:0] pv_vow_i,
    input  var logic [31:0]        pv_rgba_i,

    input  var logic        td_valid_i,
    output var logic        td_ready_o,
    input  var logic [15:0] td_v0_i,
    input  var logic [15:0] td_v1_i,
    input  var logic [15:0] td_v2_i,
    input  var logic [15:0] td_material_i,
    input  var logic [31:0] td_raster_i,
    input  var logic [31:0] td_source_i,

    input  var logic        ck_valid_i,
    output var logic        ck_ready_o,
    input  var logic [31:0] ck_next_i,
    input  var logic [15:0] ck_count_i,
    // Fourteen u32 triangle ids.  Verilator presents this as a word array, so
    // `ck_ids_i[k]` in the driver IS id k -- no packing arithmetic in C++.
    input  var logic [CHUNK_IDS*32-1:0] ck_ids_i,

    // ---- the chain patch (BINARENA 2026-09-26, console entry I55) ----------
    // Driven straight from the bench so `geom_paramarena_directed` can exercise
    // the patch and FIRE `link_illegal_o` with legal stimulus -- an index at or
    // above the allocation cursor is an ordinary input value, so this guard
    // needs no committed mutant.
    input  var logic        lk_valid_i,
    output var logic        lk_ready_o,
    input  var logic [17:0] lk_index_i,
    input  var logic [31:0] lk_next_i,
    input  var logic [15:0] lk_count_i,

    // ---- the guard lease the arena OWNS ------------------------------------
    output var logic pb_lease_valid_o,
    output var logic pb_wr_view_o,
    output var logic pb_scratch_valid_o,
    output var logic scr_grant_o,

    // ---- the published frame ------------------------------------------------
    output var logic        publish_valid_o,
    output var logic        publish_view_o,
    output var logic [15:0] publish_gen_o,
    output var logic [26:0] publish_vert_base_o,
    output var logic [26:0] publish_tri_base_o,
    output var logic [26:0] publish_chunk_base_o,
    output var logic [17:0] publish_verts_o,
    output var logic [17:0] publish_tris_o,
    output var logic [17:0] publish_chunks_o,

    // ---- the arena's evidence ----------------------------------------------
    output var logic [31:0] verts_written_o,
    output var logic [31:0] tris_written_o,
    output var logic [31:0] chunks_written_o,
    output var logic [31:0] links_written_o,
    output var logic [31:0] link_illegal_o,
    output var logic [31:0] frames_published_o,
    output var logic [31:0] arena_guard_denied_o,
    output var logic [31:0] quota_overflow_o,
    output var logic [17:0] q_giant_refs_o,
    output var logic [17:0] q_giant_chunks_o,
    output var logic [31:0] giant_reserve_breach_o,
    output var logic [31:0] records_discarded_o,
    output var logic [31:0] records_unsealed_o,
    output var logic [31:0] arena_overrun_o,
    output var logic [31:0] view_flip_blocked_o,
    output var logic [31:0] publish_blocked_o,
    output var logic [31:0] addr_view_bad_o,
    // THE BURST-ALIGNMENT TRIPWIRES, one per block. Named apart because the
    // two answer different questions: the arena's says its own allocator
    // computed a misaligned address, the walker's says it was HANDED one.
    output var logic [31:0] arena_burst_unaligned_o,
    output var logic [31:0] scr_contend_o,
    output var logic [31:0] retire_underflow_o,
    output var logic [15:0] fault_source_o,
    output var logic        frame_fault_o,
    output var logic        arena_busy_o,

    // ---- the walk ------------------------------------------------------------
    input  var logic        walk_valid_i,
    output var logic        walk_ready_o,
    input  var logic [31:0] walk_head_i,
    output var logic        walk_done_o,
    output var logic        walk_failed_o,

    output var logic        t_valid_o,
    input  var logic        t_ready_i,
    output var logic [15:0] t_v0_o,
    output var logic [15:0] t_v1_o,
    output var logic [15:0] t_v2_o,
    output var logic [15:0] t_material_o,
    output var logic [31:0] t_raster_o,
    output var logic [31:0] t_source_o,
    output var logic        t_illegal_o,

    // ---- the walker's evidence ----------------------------------------------
    output var logic [31:0] dirs_read_o,
    output var logic [31:0] dir_mismatch_o,
    output var logic [31:0] chunks_walked_o,
    output var logic [31:0] chunks_stale_o,
    output var logic [31:0] chunks_illegal_o,
    output var logic [31:0] tris_emitted_o,
    output var logic [31:0] tris_illegal_o,
    output var logic [31:0] walk_cut_o,
    output var logic [31:0] walk_guard_denied_o,
    output var logic [31:0] short_burst_o,
    output var logic [31:0] stray_beat_o,
    output var logic [31:0] gen_race_o,
    output var logic [31:0] walk_burst_unaligned_o,
    output var logic [15:0] walk_depth_max_o,
    output var logic        walk_busy_o,

    // ---- what the REAL guard said -------------------------------------------
    output var logic [31:0] guard_violations_o,
    // The last refused request, so a violation names an ADDRESS rather than
    // only a count.  A denial with no address sends the next person guessing.
    output var logic [26:0] guard_viol_addr_o,
    output var logic        guard_viol_write_o,
    output var logic [6:0]  guard_viol_len_o,

    // ---- the share's evidence ------------------------------------------------
    output var logic [31:0] share_denied_o,
    output var logic [31:0] share_err_short_o,
    output var logic [31:0] share_err_long_o,
    output var logic [31:0] share_err_unowned_o,
    output var logic [31:0] share_retire_unowned_o,
    output var logic [31:0] share_wbeat_unowned_o,
    output var logic [31:0] share_ledger_full_o,

    // ---- the bench's own observations ----------------------------------------
    // WHERE THE ARENA'S WRITES WENT, counted ONE PER REQUEST at the share's
    // accept (not one per stalled cycle).  Case 5 asks that every write of
    // frame N land in view N's range; this answers it without reading a single
    // memory word, and the memory peek beside it is the independent second
    // opinion.
    output var logic [31:0] wr_in_view0_o,
    output var logic [31:0] wr_in_view1_o,
    output var logic [31:0] wr_in_scratch_o,
    output var logic [31:0] wr_elsewhere_o,
    // Cycles in which the walker held the scratch WHILE the arena's directory
    // write was in flight.  The scratch has one owner (item 4); this is the
    // bench's independent witness to that, built from ports alone.
    output var logic [31:0] scr_overlap_o,
    // Cycles `pb_scratch_valid_o` was high at all.  Release is an ACT: a
    // scratch that never falls is a region standing permanently open.
    output var logic [31:0] scratch_open_clocks_o,
    // GUARD REQUESTS WHOSE START IS NOT 16-BYTE (8-WORD) ALIGNED.
    // `zhao_sdram_ctrl` issues a JEDEC BL8 burst at `req.addr[26:1]` as the
    // COLUMN, and a real SDR SDRAM's BL8 SEQUENTIAL burst wraps within its
    // eight-column block -- so a burst whose start column is not a multiple of
    // eight comes back ROTATED on hardware.  `sim/models/zhao_sdram_model.sv`
    // walks `rd_col + rd_beat` LINEARLY and therefore cannot see it, which is
    // exactly why this is counted here rather than left to the model.  It is a
    // MEASUREMENT and not an assertion: spec/memory_rules.md states no
    // alignment rule for the local SDRAM client ports, so the number is
    // reported and the ruling is the owner's.
    output var logic [31:0] req_unaligned_o,

    // ---- the write queue's tripwire ------------------------------------------
    output var logic wq_err_o,

    // ---- the DRAM, for placing and reading fixtures --------------------------
    // ========================================================================
    // THE CHUNK SERIALISER'S LIVE CHAIN WAS HERE AND IS RETIRED
    // ========================================================================
    // ARENACOMPOSE, 2026-09-26. `cs_enable_i` put `zhao_geom_chunkser` on the
    // arena's chunk intake, fed by `zhao_geom_binner_v2`'s serialise pass.
    // `zhao_console_core` composes `zhao_geom_arenabin` instead, the pass and
    // the serialiser are gone from the tree, and a bench arm for a producer
    // that no longer exists is a test of nothing. The ON-CHIP BINNER STAYS --
    // it still supplies every pixel and its raster drain is one half of the
    // price this bench reports.

    // ======================= GEOM.ARENABIN, THE INDEPENDENT PRODUCER =======
    // BINARENA, 2026-09-26, console entry I55. With `ab_enable_i` HIGH,
    // `zhao_geom_arenabin` owns the arena's chunk and chain-patch intakes and
    // NOTHING ELSE FILLS THEM. It is fed the same post-clip geometry the
    // binner's port block below takes, plus the arena's own descriptor index,
    // and it writes the chunk region through the same real guard, arbiter,
    // controller and SDRAM model. `zhao_geom_paramwalk` then reads the chain
    // back with no on-chip structure anywhere in the path.
    input  var logic        ab_enable_i,
    input  var logic        ab_geom_done_i,
    input  var logic [5:0]  ab_grid_w_i,
    input  var logic [5:0]  ab_grid_h_i,
    input  var logic        ab_tri_valid_i,
    output var logic        ab_tri_ready_o,
    input  var logic signed [22:0] ab_kx0_i,
    input  var logic signed [22:0] ab_ky0_i,
    input  var logic signed [47:0] ab_kc0_i,
    input  var logic signed [22:0] ab_kx1_i,
    input  var logic signed [22:0] ab_ky1_i,
    input  var logic signed [47:0] ab_kc1_i,
    input  var logic signed [22:0] ab_kx2_i,
    input  var logic signed [22:0] ab_ky2_i,
    input  var logic signed [47:0] ab_kc2_i,
    input  var logic [2:0]  ab_tl_i,
    input  var logic signed [11:0] ab_min_x_i,
    input  var logic signed [11:0] ab_max_x_i,
    input  var logic signed [11:0] ab_min_y_i,
    input  var logic signed [11:0] ab_max_y_i,
    input  var logic [17:0] ab_arena_id_i,
    input  var logic        ab_id_ok_i,
    input  var logic [9:0]  ab_head_tile_i,
    output var logic [31:0] ab_head_chunk_o,
    output var logic        ab_head_valid_o,
    output var logic        ab_frame_done_o,
    output var logic [31:0] ab_tris_binned_o,
    output var logic [31:0] ab_tris_unnamed_o,
    output var logic [31:0] ab_refs_binned_o,
    output var logic [31:0] ab_chunks_emitted_o,
    output var logic [31:0] ab_links_patched_o,
    output var logic [31:0] ab_tiles_with_refs_o,
    output var logic [31:0] ab_chunk_refused_o,
    output var logic [31:0] ab_intake_stall_o,
    output var logic [31:0] ab_flush_cut_o,
    output var logic [15:0] ab_max_tile_chunks_o,
    output var logic        ab_overflow_o,
    output var logic        ab_busy_o,
    // The producer's own busy-clock meter, so the price this entry asks for is
    // measured in the SAME unit on both sides -- clocks per tile reference.
    output var logic [31:0] ab_busy_clocks_o,

    // ---- the binner's frame and triangle intake ----------------------------
    input  var logic        bin_frame_begin_i,
    input  var logic        bin_frame_end_i,
    input  var logic [5:0]  bin_grid_w_i,
    input  var logic [5:0]  bin_grid_h_i,
    input  var logic        bin_tri_valid_i,
    output var logic        bin_tri_ready_o,
    input  var logic signed [22:0] bin_kx0_i,
    input  var logic signed [22:0] bin_ky0_i,
    input  var logic signed [47:0] bin_kc0_i,
    input  var logic signed [22:0] bin_kx1_i,
    input  var logic signed [22:0] bin_ky1_i,
    input  var logic signed [47:0] bin_kc1_i,
    input  var logic signed [22:0] bin_kx2_i,
    input  var logic signed [22:0] bin_ky2_i,
    input  var logic signed [47:0] bin_kc2_i,
    input  var logic [2:0]  bin_tl_i,
    input  var logic signed [20:0] bin_ax_i,
    input  var logic signed [20:0] bin_ay_i,
    input  var logic signed [20:0] bin_bx_i,
    input  var logic signed [20:0] bin_by_i,
    input  var logic signed [20:0] bin_cx_i,
    input  var logic signed [20:0] bin_cy_i,
    input  var logic signed [11:0] bin_min_x_i,
    input  var logic signed [11:0] bin_max_x_i,
    input  var logic signed [11:0] bin_min_y_i,
    input  var logic signed [11:0] bin_max_y_i,
    input  var logic [15:0] bin_src_id_i,

    // ---- THE GROUND TRUTH. The binner's own raster drain, observed. --------
    // This is what a tile's reference order IS, straight out of the block that
    // owns it, with no model of it anywhere. The acceptance test captures this
    // stream and then walks the same tiles out of SDRAM; the two must agree.
    input  var logic        bin_job_ready_i,
    output var logic        bin_job_valid_o,
    output var logic signed [11:0] bin_job_tile_x_o,
    output var logic signed [11:0] bin_job_tile_y_o,
    output var logic        bin_job_first_o,
    output var logic        bin_job_last_o,
    output var logic [15:0] bin_job_src_id_o,
    output var logic        bin_drain_done_o,
    output var logic [31:0] bin_tile_references_o,
    output var logic [31:0] bin_triangles_culled_o,
    output var logic        bin_overflow_o,

    // ---- the identity queue, driven as GEOM.VERTID drives it ---------------
    // `vid_retire_i` is the DESCRIPTOR STEP RETIRING, accepted or not, and
    // `vid_id_ok_i` is the acceptance. Driving them separately is what lets the
    // test fire the misalignment this queue exists to survive.
    input  var logic        vid_retire_i,
    input  var logic        vid_id_ok_i,
    input  var logic [17:0] vid_id_i,
    output var logic [31:0] tidq_underflow_o,
    output var logic [31:0] tidq_overflow_o,
    output var logic [31:0] tidq_unnamed_o,

    // ---- the chunk as it is OFFERED, before SDRAM -------------------------
    // This is how a test separates a wrong id the PRODUCER built from a wrong
    // id the arena stored -- two faults that look identical in the record
    // that comes back.
    output var logic        ck_tap_fire_o,
    output var logic [15:0] ck_tap_count_o,
    output var logic [31:0] ck_tap_id0_o,
    output var logic [31:0] ck_tap_id1_o,
    output var logic [31:0] ck_tap_next_o,

    // The arena's own chunk cursor.
    output var logic        ck_accept_o,
    output var logic [17:0] ck_alloc_id_o,

    input  var logic        peek_en_i,
    input  var logic [25:0] peek_waddr_i,
    output var logic [15:0] peek_data_o,
    // ---- THE WALKER'S PUBLISHED-CHUNK-BASE BUMP -----------------------------
    // A BENCH INPUT, and the reason it exists is that `walk_burst_unaligned_o`
    // must be SEEN to fire and it is NOT unreachable -- the walker does not
    // compute its bases, it is TOLD them.  Claiming "reachable, so no mutant
    // is owed" without firing it is exactly the claim CLAUDE.md says to check
    // hardest, so this is the stimulus that fires it.
    //
    // It adds a byte offset to the chunk base the WALKER sees.  On its own
    // that would be caught one state earlier -- `dir_agrees_c` compares the
    // directory that travelled through SDRAM against `pub_chunk_base_i`, and a
    // bumped base disagrees, so the walk would fail at W_DIR_CHECK with
    // `dir_mismatch_o` and never reach a chunk read.  The driver therefore
    // POKES the directory's chunk-base word to match, which is why this is a
    // two-part stimulus and not a knob that hides a check.
    //
    // NOTHING IN PRODUCTION HAS THIS.  It cannot widen a permission: the
    // request still goes through the real `zhao_mem_guard`, and a bumped base
    // that left the view would be REFUSED rather than counted.
    input  var logic [26:0] wcfg_chunk_base_bump_i,

    input  var logic        poke_en_i,
    input  var logic [25:0] poke_waddr_i,
    input  var logic [15:0] poke_data_i,
    // ---- DIAGNOSTIC: the socket's owed-word ledger, instrumented -------------
    // Exported so the phase-53 publication stall is a MEASUREMENT and not an
    // argument.  See the always_ff beside the write queue for what they mean.
    output var logic [15:0] dbg_wq_occ_o,
    output var logic [15:0] dbg_wq_owed_o,
    output var logic [31:0] dbg_issued_words_o,
    output var logic [31:0] dbg_retired_words_o,
    output var logic [31:0] dbg_client_credits_o,
    output var logic [31:0] dbg_collide_o,
    output var logic [31:0] dbg_collide_words_o,
    // ---- THROUGHPUT, BOTH PATHS, ONE STIMULUS -------------------------------
    // Entry I55 asks what the raster-path swap COSTS. These measure the two
    // producers on the same scene in the same bench: the binner's on-chip job
    // drain (what the raster path eats today) and the external SDRAM walk.
    output var logic [31:0] dbg_drain_refs_o,    // job handshakes
    output var logic [31:0] dbg_drain_cycles_o,  // first handshake -> last
    output var logic [31:0] dbg_walk_cycles_o,   // clocks the walker was busy
    output var logic [31:0] dbg_walk_reqs_o,     // guard requests the walk issued
    output var logic        model_error_o,
    output var logic        init_done_o
);

  // ==========================================================================
  // THE TWO BLOCKS UNDER TEST
  // ==========================================================================
  zhao_guard_req_t arena_req, walk_req;
  zhao_guard_rsp_t arena_rsp, walk_rsp;
  logic [63:0]     arena_wdata;
  logic            arena_wvalid, arena_wlast, arena_wready;
  logic [7:0]      arena_retire;

  logic            walk_beat_valid, walk_beat_last;
  logic [63:0]     share_beat_data;

  logic            scr_req_w;

  logic pb_lease_arena, pb_wr_view_w, pb_scratch_w;

  // THE MUTANT SEAM.  A plain `ifdef selecting the MODULE NAME -- which a
  // command-line -D does reach.  CLAUDE.md records that `-D` cannot override a
  // FUNCTION-LIKE `define and says nothing when it fails to, so the seam is a
  // bare `ifdef and `geom_paramarena_drainmut.cpp` is built BOTH ways from one
  // source: with the macro it requires `addr_view_bad_o` to FIRE, without it it
  // requires SILENCE.  That pair is the negative control that shows the
  // selector engaged rather than compiling production twice.
`ifdef ZHAO_PARAMARENA_DRAIN_MUT
  zhao_geom_paramarena_drain_mutant #(
`elsif ZHAO_PARAMARENA_ALIGN_MUT
  // The SECOND positive control, selected the same way and for the same
  // reason: `burst_unaligned_o` is unreachable while the allocator is
  // correct, so the only demonstration is an allocator that is not.
  zhao_geom_paramarena_align_mutant #(
`elsif ZHAO_PARAMARENA_RESERVE_MUT
  // THE THIRD, SEALPLAN 2026-09-26. `giant_reserve_breach_o` watches for an
  // ordinary chunk allocation reaching into the giant's reserved region, which
  // is unreachable while `ck_fits_c` and the plan validator are both correct.
  // The mutant is `ck_fits_c`'s `<` become `<=`: one chunk past the ordinary
  // quota, which with a seal at exactly MAX_CHUNKS - reservation is the first
  // chunk of the reserve.
  zhao_geom_paramarena_reserve_mutant #(
`else
  zhao_geom_paramarena #(
`endif
      .MAX_VERTS  (MAX_VERTS),
      .MAX_TRIS   (MAX_TRIS),
      .MAX_CHUNKS (MAX_CHUNKS),
      .CHUNK_IDS  (CHUNK_IDS)
  ) u_arena (
      .clk              (clk),
      .rst_n            (rst_n),
      .cfg_vram_client_i(ZHAO_CLIENT_ENGINE1),

      .seal_valid_i  (seal_valid_i),
      .seal_ready_o  (seal_ready_o),
      .seal_verts_i  (seal_verts_i),
      .seal_tris_i   (seal_tris_i),
      .seal_chunks_i (seal_chunks_i),
      .seal_giant_refs_i   (seal_giant_refs_i),
      .seal_giant_chunks_i (seal_giant_chunks_i),
      .frame_gen_i   (frame_gen_i),
      // I54: with the serialiser live the frame ends when the CHUNKS ARE
      // IN, not when the producer stops. The arena publishes as soon as
      // its frame ends and its writes retire, so the raw edge would
      // publish a frame whose tile lists are empty -- correct-looking and
      // wrong. This is exactly what `zhao_console_core` does.
      .frame_end_i   (ab_enable_i ? ab_frame_done_o : frame_end_i),
      // THE READER'S OWN BUSY, not a bench input.  The drain precondition is
      // "no reader owns the view this seal is about to make the build target",
      // and the only thing that knows is the walker.
      .reader_busy_i (walk_busy_o),

      .pb_lease_valid_o  (pb_lease_arena),
      .pb_wr_view_o      (pb_wr_view_w),
      .pb_scratch_valid_o(pb_scratch_w),

      .pv_valid_i (pv_valid_i),
      .pv_ready_o (pv_ready_o),
      .pv_x_i     (pv_x_i),
      .pv_y_i     (pv_y_i),
      .pv_invw_i  (pv_invw_i),
      .pv_status_i(pv_status_i),
      .pv_uow_i   (pv_uow_i),
      .pv_vow_i   (pv_vow_i),
      .pv_rgba_i  (pv_rgba_i),

      .td_valid_i   (td_valid_i),
      .td_ready_o   (td_ready_o),
      .td_v0_i      (td_v0_i),
      .td_v1_i      (td_v1_i),
      .td_v2_i      (td_v2_i),
      .td_material_i(td_material_i),
      .td_raster_i  (td_raster_i),
      .td_source_i  (td_source_i),

      // ONE PRODUCER AT A TIME. `ab_enable_i` hands the intake to
      // GEOM.ARENABIN; low, it is the hand-driven pair the arena's own
      // directed cases use. The third arm -- `zhao_geom_chunkser` behind
      // `cs_enable_i` -- is RETIRED (ARENACOMPOSE): two producers on one
      // intake is the "parallel legacy arena" shape owner directive section 4
      // refuses, and the console now has exactly one.
      .ck_valid_i(ab_enable_i ? ab_ck_valid_w : ck_valid_i),
      .ck_ready_o(arena_ck_ready_w),
      .ck_next_i (ab_enable_i ? ab_ck_next_w  : ck_next_i),
      .ck_count_i(ab_enable_i ? ab_ck_count_w : ck_count_i),
      .ck_ids_i  (ab_enable_i ? ab_ck_ids_w   : ck_ids_i),
      .lk_valid_i(ab_enable_i ? ab_lk_valid_w : lk_valid_i),
      .lk_ready_o(arena_lk_ready_w),
      .lk_index_i(ab_enable_i ? ab_lk_index_w : lk_index_i),
      .lk_next_i (ab_enable_i ? ab_lk_next_w  : lk_next_i),
      .lk_count_i(ab_enable_i ? ab_lk_count_w : lk_count_i),

      .ck_accept_o   (ck_accept_o),
      .ck_alloc_id_o (ck_alloc_id_o),

      // ARENAID 2026-09-25: the allocation index rides its own acceptance, so
      // a producer can NAME the vertex it just published. This bench does not
      // exercise the identity space -- `geom_vertid_directed` does, with a
      // model of these very ports -- so they leave open. Declared rather than
      // omitted: a missing pin is what the next fit finds.
      /* verilator lint_off PINCONNECTEMPTY */   // ARENAID: no consumer here
      .pv_accept_o (),
      .pv_id_o     (),
      .td_accept_o (),
      .td_id_o     (),
      // I54: the clock the arena cursors actually move. The identity queue
      // and the serialiser head table are both cleared on it, exactly as
      // zhao_console_core clears them.
      .seal_fire_o (pa_seal_fire_w),
      /* verilator lint_on PINCONNECTEMPTY */

      .scr_req_i  (scr_req_w),
      .scr_grant_o(scr_grant_o),

      .publish_valid_o     (publish_valid_o),
      .publish_view_o      (publish_view_o),
      .publish_gen_o       (publish_gen_o),
      .publish_vert_base_o (publish_vert_base_o),
      .publish_tri_base_o  (publish_tri_base_o),
      .publish_chunk_base_o(publish_chunk_base_o),
      .publish_verts_o     (publish_verts_o),
      .publish_tris_o      (publish_tris_o),
      .publish_chunks_o    (publish_chunks_o),

      .guard_req_o    (arena_req),
      .guard_rsp_i    (arena_rsp),
      .guard_wdata_o  (arena_wdata),
      .guard_wvalid_o (arena_wvalid),
      .guard_wready_i (arena_wready),
      .guard_wlast_o  (arena_wlast),
      .retire_words_i (arena_retire),

      .verts_written_o    (verts_written_o),
      .tris_written_o     (tris_written_o),
      .chunks_written_o   (chunks_written_o),
      .links_written_o    (links_written_o),
      .link_illegal_o     (link_illegal_o),
      .frames_published_o (frames_published_o),
      .guard_denied_o     (arena_guard_denied_o),
      .quota_overflow_o   (quota_overflow_o),
      .q_giant_refs_o        (q_giant_refs_o),
      .q_giant_chunks_o      (q_giant_chunks_o),
      .giant_reserve_breach_o(giant_reserve_breach_o),
      .records_discarded_o(records_discarded_o),
      .records_unsealed_o (records_unsealed_o),
      .arena_overrun_o    (arena_overrun_o),
      .view_flip_blocked_o(view_flip_blocked_o),
      .publish_blocked_o  (publish_blocked_o),
      .addr_view_bad_o    (addr_view_bad_o),
      .burst_unaligned_o  (arena_burst_unaligned_o),
      .scr_contend_o      (scr_contend_o),
      .retire_underflow_o (retire_underflow_o),
      .fault_source_o     (fault_source_o),
      .frame_fault_o      (frame_fault_o),
      .busy_o             (arena_busy_o)
  );

  assign pb_wr_view_o       = pb_wr_view_w;
  assign pb_scratch_valid_o = pb_scratch_w;
  assign pb_lease_valid_o   = pb_lease_arena;

  zhao_geom_paramwalk #(
      .MAX_WALK     (WALK_MAX),
      .CHUNK_IDS    (CHUNK_IDS),
      .ARENA_CHUNKS (MAX_CHUNKS)
  ) u_walk (
      .clk  (clk),
      .rst_n(rst_n),
      .cfg_vram_client_i(ZHAO_CLIENT_ENGINE1),

      // THE PUBLISHED FRAME, STRAIGHT FROM THE ARENA'S REGISTERS.  This is one
      // of the two independent paths `dir_mismatch_o` differences; the other is
      // 64 bytes that went out to DRAM and came back.
      .pub_valid_i     (publish_valid_o),
      .pub_gen_i       (publish_gen_o),
      .pub_vert_base_i (publish_vert_base_o),
      .pub_tri_base_i  (publish_tri_base_o),
      .pub_chunk_base_i(publish_chunk_base_o + wcfg_chunk_base_bump_i),
      .pub_verts_i     (publish_verts_o),
      .pub_tris_i      (publish_tris_o),
      .pub_chunks_i    (publish_chunks_o),

      .scr_req_o  (scr_req_w),
      .scr_grant_i(scr_grant_o),

      .walk_valid_i (walk_valid_i),
      .walk_ready_o (walk_ready_o),
      .walk_head_i  (walk_head_i),
      .walk_done_o  (walk_done_o),
      .walk_failed_o(walk_failed_o),

      .t_valid_o   (t_valid_o),
      .t_ready_i   (t_ready_i),
      .t_v0_o      (t_v0_o),
      .t_v1_o      (t_v1_o),
      .t_v2_o      (t_v2_o),
      .t_material_o(t_material_o),
      .t_raster_o  (t_raster_o),
      .t_source_o  (t_source_o),
      .t_illegal_o (t_illegal_o),

      .guard_req_o  (walk_req),
      .guard_rsp_i  (walk_rsp),
      .beat_valid_i (walk_beat_valid),
      .beat_data_i  (share_beat_data),
      .beat_last_i  (walk_beat_last),

      .dirs_read_o     (dirs_read_o),
      .dir_mismatch_o  (dir_mismatch_o),
      .chunks_walked_o (chunks_walked_o),
      .chunks_stale_o  (chunks_stale_o),
      .chunks_illegal_o(chunks_illegal_o),
      .tris_emitted_o  (tris_emitted_o),
      .tris_illegal_o  (tris_illegal_o),
      .walk_cut_o      (walk_cut_o),
      .guard_denied_o  (walk_guard_denied_o),
      .short_burst_o   (short_burst_o),
      .stray_beat_o    (stray_beat_o),
      .gen_race_o      (gen_race_o),
      .burst_unaligned_o(walk_burst_unaligned_o),
      .walk_depth_max_o(walk_depth_max_o),
      .busy_o          (walk_busy_o)
  );

  // ==========================================================================
  // THE SHARE: TWO REQUESTERS, ONE ENGINE1 CLIENT
  // ==========================================================================
  // Requester 0 is the arena (the only WRITER), requester 1 the walker.  This
  // is `zhao_mem_share_wr` and not `zhao_mem_share2` because a socket with a
  // writer on it needs the two things the read share does not have: write-data
  // ORDER (nothing else may be taken between a write's accept and its last data
  // beat) and retirement ATTRIBUTION (the arena's `retire_words_i` must be ITS
  // credits, not the walker's).  Getting the second wrong is exactly how "data
  // must not be published before its writes retire" turns into publishing
  // early, which is the flattering direction.
  zhao_guard_req_t [1:0]       sh_req;
  zhao_guard_rsp_t [1:0]       sh_rsp;
  logic            [1:0]       sh_beat_valid;
  logic            [1:0]       sh_beat_last;
  logic            [1:0][63:0] sh_wdata;
  logic            [1:0]       sh_wvalid, sh_wlast, sh_wready;
  logic            [1:0][7:0]  sh_retire;
  /* verilator lint_off UNUSEDSIGNAL */
  logic            [1:0][31:0] sh_jobs;
  logic            [31:0]      sh_contention;
  /* verilator lint_on UNUSEDSIGNAL */

  assign sh_req[0] = arena_req;
  assign sh_req[1] = walk_req;
  assign arena_rsp = sh_rsp[0];
  assign walk_rsp  = sh_rsp[1];
  assign arena_retire = sh_retire[0];
  // The arena never reads, so its beat ports are unconnected by construction;
  // the walker never writes, so its write channel is tied off at the share.
  assign sh_wdata[0]  = arena_wdata;
  assign sh_wvalid[0] = arena_wvalid;
  assign sh_wlast[0]  = arena_wlast;
  assign arena_wready = sh_wready[0];
  assign sh_wdata[1]  = 64'd0;   // TIE: the walker has no write channel
  assign sh_wvalid[1] = 1'b0;    // TIE: the walker has no write channel
  assign sh_wlast[1]  = 1'b0;    // TIE: the walker has no write channel

  assign walk_beat_valid = sh_beat_valid[1];
  assign walk_beat_last  = sh_beat_last[1];

  // The arena's beat ports: it issues no reads, so a beat routed to it is a
  // fault in the share and there is nothing downstream to see it.  Counted
  // here rather than left dangling.
  logic [31:0] arena_beat_stray_q;
  /* verilator lint_off UNUSEDSIGNAL */
  logic unused_arena_beats;
  /* verilator lint_on UNUSEDSIGNAL */
  assign unused_arena_beats = sh_beat_last[0];

  zhao_guard_req_t g_req;
  zhao_guard_rsp_t g_rsp;
  logic [63:0]     g_wdata;
  logic            g_wvalid, g_wlast, g_wready;
  logic            g_beat_valid, g_beat_last;
  logic [63:0]     g_beat_data;
  logic [7:0]      g_credits;

  zhao_mem_share_wr #(
      .N        (2),
      .CLIENT_ID(3),          // ZHAO_CLIENT_ENGINE1
      .RQ       (4)
  ) u_share (
      .clk  (clk),
      .rst_n(rst_n),

      .req_i       (sh_req),
      .rsp_o       (sh_rsp),
      .beat_valid_o(sh_beat_valid),
      .beat_data_o (share_beat_data),
      .beat_last_o (sh_beat_last),
      .wdata_i     (sh_wdata),
      .wvalid_i    (sh_wvalid),
      .wlast_i     (sh_wlast),
      .wready_o    (sh_wready),
      .retire_o    (sh_retire),

      .m_req_o       (g_req),
      .m_rsp_i       (g_rsp),
      .m_beat_valid_i(g_beat_valid),
      .m_beat_data_i (g_beat_data),
      .m_beat_last_i (g_beat_last),
      .m_wdata_o     (g_wdata),
      .m_wvalid_o    (g_wvalid),
      .m_wlast_o     (g_wlast),
      .m_wready_i    (g_wready),
      .m_credits_i   (g_credits),

      .jobs_o          (sh_jobs),
      .denied_o        (share_denied_o),
      .contention_o    (sh_contention),
      .err_short_o     (share_err_short_o),
      .err_long_o      (share_err_long_o),
      .err_unowned_o   (share_err_unowned_o),
      .retire_unowned_o(share_retire_unowned_o),
      .wbeat_unowned_o (share_wbeat_unowned_o),
      .ledger_full_o   (share_ledger_full_o)
  );

  // ==========================================================================
  // THE REAL GUARD -- IN THE PATH, NOT AS AN OBSERVER
  // ==========================================================================
  // `tb_terrain_lodpath.sv` instantiates a guard that answers nobody, because
  // its fabric is played.  This one IS the fabric: a refused request is refused
  // for the blocks under test, not merely noted.  That is what makes case 9
  // ("the arena never generates a request the guard refuses") a statement about
  // the composition and not about a shadow.
  zhao_arb_req_t   geom_arb_req;
  logic            g_viol_pulse;
  zhao_guard_req_t g_viol_req;
  zhao_arb_rsp_t   [6:0] client_rsp;
  zhao_arb_req_t   [6:0] client_req;

  zhao_mem_guard u_guard (
      .clk   (clk),
      .rst_n (rst_n),
      .req   (g_req),
      .rsp   (g_rsp),
      // TIE: ENGINE1 holds no framebuffer lease; the guard's blit/fb arms never
      // name it, and leaving them live would let a framebuffer map admit a
      // geometry write.
      .map_valid(1'b0),
      .blit_slot(1'b0),
      .blit_span(32'd0),
      .fb_writer(1'b0),
      // TIE: this client is not MEM.UPLOAD; R32's resource-write arm names
      // TERRAIN_BUILD alone.
      .res_valid(1'b0),
      .res_base (32'd0),
      .res_span (32'd0),
      // ITEM 4'S LEASE, FROM THE ARENA'S OWN OUTPUTS.  This is the one guard
      // instance in the console whose client is ENGINE1, so it is the one place
      // 5c can be open at all.
      .pb_lease_valid  (pb_lease_arena),
      .pb_wr_view      (pb_wr_view_w),
      .pb_scratch_valid(pb_scratch_w),
      .arb_req (geom_arb_req),
      .arb_rsp (client_rsp[3]),
      .guard_violation    (g_viol_pulse),
      .guard_violations   (guard_violations_o),
      .guard_violation_req(g_viol_req)
  );

  assign guard_viol_addr_o  = g_viol_req.addr;
  assign guard_viol_write_o = g_viol_req.write;
  assign guard_viol_len_o   = g_viol_req.len;

  // ==========================================================================
  // SLOT 3'S WRITE-DATA QUEUE AND ITS GATE -- `zhao_shell_top_v2.sv`'s `gq`
  // ==========================================================================
  // words a request of `len` bytes occupies: the arbiter's own rounding, the
  // same function `zhao_mem_share_wr` and the shell both use, so the gate owes
  // exactly what the arbiter will credit.
  function automatic logic [6:0] words_of(input logic [6:0] len_b);
    words_of = 7'((len_b + 7'd1) >> 1);
  endfunction

  localparam int unsigned QPW = $clog2(WQ_W);

  logic [15:0]   wq [0:WQ_W-1];
  logic [QPW:0]  wq_wp, wq_rp, wq_owed;
  logic [QPW:0]  wq_occ;
  logic          wq_room_for_req;
  logic          wr_beat_ctrl;
  logic [15:0]   wdata_ctrl;

  assign wq_occ  = wq_wp - wq_rp;
  // One 64-bit beat becomes four 16-bit words, so the queue is ready exactly
  // when four fit.
  assign g_wready = (wq_occ <= (QPW+1)'(WQ_W - 4));

  // THE GATE.  `wq_free` is the queue's words NOT YET OWED to a request the
  // arbiter has already accepted, so a write is offered only when every one of
  // its words is already here.  Exact, not a race.
  assign wq_room_for_req =
      ({1'b0, wq_occ} - {1'b0, wq_owed}) >= (QPW+2)'(words_of(geom_arb_req.len));

  always_comb begin
    for (int k = 0; k < 7; k++) client_req[k] = '0;
    client_req[3]       = geom_arb_req;
    client_req[3].valid = geom_arb_req.valid
                       && (!geom_arb_req.write || wq_room_for_req);
  end

  zhao_arb_req_t ctrl_req;
  zhao_arb_rsp_t ctrl_rsp;
  logic          hold_refresh;
  /* verilator lint_off UNUSEDSIGNAL */
  logic [6:0][31:0] vram_bytes, vram_bytes_shadow;
  logic [31:0]      scanout_preempted;
  /* verilator lint_on UNUSEDSIGNAL */

  zhao_vram_arbiter u_arb (
      .clk              (clk),
      .rst_n            (rst_n),
      .client_req       (client_req),
      .client_rsp       (client_rsp),
      .ctrl_req         (ctrl_req),
      .hold_refresh     (hold_refresh),
      .ctrl_rsp         (ctrl_rsp),
      .frame_tick       (1'b0),
      .vram_bytes       (vram_bytes),
      .vram_bytes_shadow(vram_bytes_shadow),
      .scanout_preempted(scanout_preempted)
  );

  assign g_credits = client_rsp[3].credits;

  // THE WRITE-OWNER MUX, in the shell's shape.  The owner is known at GRANT and
  // word 0 can be needed IN THE GRANT CYCLE (`zhao_sdram_ctrl` raises `wr_beat`
  // in S_RW, which is cycle G itself on a row hit), so during G it is read
  // straight off `ctrl_req` and registered for the burst's remaining beats.
  logic wr_owner_geom_r;
  logic wr_sel_geom;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) wr_owner_geom_r <= 1'b0;
    else if (ctrl_rsp.grant && ctrl_req.write)
      wr_owner_geom_r <= (ctrl_req.client == ZHAO_CLIENT_ENGINE1);
  end
  assign wr_sel_geom = (ctrl_rsp.grant && ctrl_req.write)
                       ? (ctrl_req.client == ZHAO_CLIENT_ENGINE1)
                       : wr_owner_geom_r;

  assign wdata_ctrl = wq[wq_rp[QPW-1:0]];

  wire wq_pop     = wr_beat_ctrl && wr_sel_geom;
  wire wq_promise = client_rsp[3].grant && geom_arb_req.write;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      wq_wp    <= '0;
      wq_rp    <= '0;
      wq_owed  <= '0;
      wq_err_o <= 1'b0;
    end else begin
      if (g_wvalid && g_wready) begin
        for (int j = 0; j < 4; j++)
          wq[QPW'(wq_wp + (QPW+1)'(j))] <= g_wdata[16*j +: 16];
        wq_wp <= wq_wp + (QPW+1)'(4);
      end
      if (wq_pop) begin
        // A pop from an empty queue is a garbage word written to DRAM.  The
        // write gate makes it unreachable; this is the tripwire that says so,
        // and the directed test asserts it stays low.
        if (wq_occ == '0) wq_err_o <= 1'b1;
        else wq_rp <= wq_rp + (QPW+1)'(1);
      end
      wq_owed <= wq_owed
               + (wq_promise ? (QPW+1)'(words_of(geom_arb_req.len)) : '0)
               - ((wq_pop && (wq_owed != '0)) ? (QPW+1)'(1) : '0);
    end
  end

  // ==========================================================================
  // DIAGNOSTIC: THE SOCKET'S WORDS, AT THREE POINTS ON ONE PATH
  // ==========================================================================
  // `wq_occ` and `wq_owed` are the write queue's STATE, exported because the
  // first hypothesis for the phase-53 publication stall was this gate -- that
  // `wq_promise` pairs a REGISTERED grant with a LIVE length and so could
  // over-owe the queue and hold the arbiter off forever. Measuring it killed
  // it: at the wedge both read ZERO, the queue is drained and owes nothing.
  // The counters that tested that hypothesis are not kept, because after it
  // was disproved they could not be made to fire and a counter asserted zero
  // and never seen to move is the thing this repository does not ship.
  assign dbg_wq_occ_o  = {{(16-(QPW+1)){1'b0}}, wq_occ};
  assign dbg_wq_owed_o = {{(16-(QPW+1)){1'b0}}, wq_owed};

  // ---------------------------------------------- THROUGHPUT, BOTH PATHS ----
  // `dbg_drain_cycles_o` is the span from the FIRST job handshake to the LAST,
  // so it excludes the binner's clear phase and any idle before the scene
  // arrives -- it is the drain itself, which is what the raster path pays.
  // `dbg_walk_cycles_o` accumulates every clock `busy_o` is high on the walker,
  // which is the request, the SDRAM round trip and the decode together, and is
  // what an external producer would pay for the same triangles.
  logic        drain_started_q;
  logic [31:0] drain_run_q;
  wire         job_hs_c = bin_job_valid_o && bin_job_ready_i;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      drain_started_q    <= 1'b0;
      drain_run_q        <= 32'd0;
      dbg_drain_refs_o   <= 32'd0;
      dbg_drain_cycles_o <= 32'd0;
      dbg_walk_cycles_o  <= 32'd0;
      dbg_walk_reqs_o    <= 32'd0;
    end else begin
      if (drain_started_q) drain_run_q <= drain_run_q + 32'd1;
      if (job_hs_c) begin
        drain_started_q    <= 1'b1;
        dbg_drain_refs_o   <= dbg_drain_refs_o + 32'd1;
        dbg_drain_cycles_o <= drain_run_q;
      end
      if (walk_busy_o) dbg_walk_cycles_o <= dbg_walk_cycles_o + 32'd1;
      if (walk_req.valid && walk_rsp.ready)
        dbg_walk_reqs_o <= dbg_walk_reqs_o + 32'd1;
    end
  end

  // WHERE THE WORDS GO.  Three totals over one frame, at three points on the
  // one path: what the ARENA was told it owed (the guard's accept, the same
  // event and the same function `wr_words_q` grows on), what the ARBITER
  // credited back to the ENGINE1 CLIENT, and what the SHARE handed to the
  // arena.  Issued-vs-client localises a loss to the memory fabric;
  // client-vs-retired localises it to the share's attribution ledger.
  // The arena adds to wr_words_q on the GUARD'S ok, using the length it
  // LATCHED when the request was taken -- not a live one.  Mirrored exactly
  // here, or this probe would measure a different machine than the one that
  // wedges.
  logic [6:0] arena_len_words_q;
  logic       arena_len_wr_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      arena_len_words_q <= 7'd0;
      arena_len_wr_q    <= 1'b0;
    end else if (arena_req.valid && arena_rsp.ready) begin
      arena_len_words_q <= words_of(arena_req.len);
      arena_len_wr_q    <= arena_req.write;
    end
  end
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      dbg_issued_words_o   <= 32'd0;
      dbg_retired_words_o  <= 32'd0;
      dbg_client_credits_o <= 32'd0;
      dbg_collide_o        <= 32'd0;
      dbg_collide_words_o  <= 32'd0;
    end else begin
      if (arena_rsp.ok && arena_len_wr_q)
        dbg_issued_words_o <= dbg_issued_words_o
                            + 32'({25'd0, arena_len_words_q});
      dbg_retired_words_o  <= dbg_retired_words_o  + 32'({24'd0, arena_retire});
      dbg_client_credits_o <= dbg_client_credits_o + 32'({24'd0, g_credits});
      // THE COLLISION.  `zhao_geom_paramarena` grows `wr_words_q` on the
      // guard's `ok` and shrinks it on `retire_words_i`, with BOTH as
      // non-blocking assignments in ONE always_ff.  Textual order does not
      // merge them: on a clock where a retirement and an acceptance coincide
      // the later statement simply overwrites the earlier, and the retired
      // words are lost from the arena's ledger while the socket's own totals
      // still balance.  This counts that clock and the words it drops.
      if (arena_rsp.ok && (arena_retire != 8'd0)) begin
        dbg_collide_o       <= dbg_collide_o + 32'd1;
        dbg_collide_words_o <= dbg_collide_words_o + 32'({24'd0, arena_retire});
      end
    end
  end

  // ==========================================================================
  // THE CONTROLLER AND THE DRAM
  // ==========================================================================
  logic        phy_cs_n, phy_ras_n, phy_cas_n, phy_we_n, phy_dq_oe;
  logic [12:0] phy_a;
  logic [1:0]  phy_ba, phy_dqm;
  logic [15:0] phy_dq_o, phy_dq_i;
  logic [15:0] ctrl_rdata;
  logic        ctrl_rdata_valid;
  /* verilator lint_off UNUSEDSIGNAL */
  logic [31:0] refresh_stalls, bank_conflicts;
  logic        refresh_pulse;
  logic [5:0]  model_err_kind;
  /* verilator lint_on UNUSEDSIGNAL */

  zhao_sdram_ctrl u_ctrl (
      .clk         (clk),
      .rst_n       (rst_n),
      .req         (ctrl_req),
      .rsp         (ctrl_rsp),
      .hold_refresh(hold_refresh),
      .wdata       (wdata_ctrl),
      .wr_beat     (wr_beat_ctrl),
      .rdata       (ctrl_rdata),
      .rdata_valid (ctrl_rdata_valid),
      .phy_cs_n    (phy_cs_n),
      .phy_ras_n   (phy_ras_n),
      .phy_cas_n   (phy_cas_n),
      .phy_we_n    (phy_we_n),
      .phy_a       (phy_a),
      .phy_ba      (phy_ba),
      .phy_dq_o    (phy_dq_o),
      .phy_dq_oe   (phy_dq_oe),
      .phy_dqm     (phy_dqm),
      .phy_dq_i    (phy_dq_i),
      .init_done      (init_done_o),
      .refresh_stalls (refresh_stalls),
      .bank_conflicts (bank_conflicts),
      .refresh_pulse  (refresh_pulse)
  );

  zhao_sdram_model u_model (
      .clk       (clk),
      .phy_cs_n  (phy_cs_n),
      .phy_ras_n (phy_ras_n),
      .phy_cas_n (phy_cas_n),
      .phy_we_n  (phy_we_n),
      .phy_a     (phy_a),
      .phy_ba    (phy_ba),
      .phy_dq_o  (phy_dq_o),
      .phy_dq_oe (phy_dq_oe),
      .phy_dqm   (phy_dqm),
      .phy_dq_i  (phy_dq_i),
      .peek_en   (peek_en_i),
      .peek_waddr(peek_waddr_i),
      .peek_data (peek_data_o),
      // THE BACKDOOR.  Case 2's negative half needs the frame directory
      // corrupted BEHIND THE BLOCK'S BACK -- a fault no legal request can
      // produce, and the only way to show `dir_mismatch_o` is an instrument
      // rather than a claim.
      .poke_en   (poke_en_i),
      .poke_waddr(poke_waddr_i),
      .poke_data (poke_data_i),
      .err_trcd            (model_err_kind[0]),
      .err_trp             (model_err_kind[1]),
      .err_trc             (model_err_kind[2]),
      .err_refresh_interval(model_err_kind[3]),
      .err_protocol        (model_err_kind[4]),
      .err_mrs             (model_err_kind[5]),
      .model_error         (model_error_o)
  );

  // ==========================================================================
  // THE READ-BEAT PACKER, AND `last` FROM THE REQUEST'S OWN LENGTH
  // ==========================================================================
  // Four 16-bit controller words become one 64-bit beat.  `last` marks the end
  // of the GUARD REQUEST, not of an arbiter burst: a 64-byte read is four
  // 8-word bursts and the walker's beat counter must not see four `last`
  // pulses.  The count comes from the accepted request's own `len >> 3`, never
  // from a constant -- the walker issues BOTH 64-byte (chunk, directory) and
  // 16-byte (descriptor) reads, so a constant eight would never fire `last` on
  // a descriptor and would carry the count into the next request.
  logic [47:0] pack_lo;
  logic [1:0]  pack_cnt;
  logic [3:0]  rd_expect_r, rd_beat_cnt_r;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) rd_beat_cnt_r <= 4'd0;
    // The reset is the guard HANDSHAKE (`valid && ready`), not `ready && ok`.
    // Those two never coincide: `rsp.ready` is the LEVEL `!fwd_active` and
    // `rsp.ok` pulses one cycle AFTER the accept, by which time ready has
    // already dropped.  A condition that cannot be true is a reset that never
    // happens.
    else if (g_req.valid && g_rsp.ready) rd_beat_cnt_r <= 4'd0;
    else if (g_beat_valid)               rd_beat_cnt_r <= rd_beat_cnt_r + 4'd1;
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) rd_expect_r <= 4'd8;
    else if (g_req.valid && g_rsp.ready) rd_expect_r <= 4'(g_req.len >> 3);
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      pack_lo      <= '0;
      pack_cnt     <= 2'd0;
      g_beat_valid <= 1'b0;
      g_beat_data  <= '0;
    end else begin
      g_beat_valid <= 1'b0;
      if (ctrl_rdata_valid) begin
        if (pack_cnt == 2'd3) begin
          g_beat_data  <= {ctrl_rdata, pack_lo};
          g_beat_valid <= 1'b1;
          pack_cnt     <= 2'd0;
        end else begin
          pack_lo[16*pack_cnt +: 16] <= ctrl_rdata;
          pack_cnt                   <= pack_cnt + 2'd1;
        end
      end
    end
  end

  assign g_beat_last = g_beat_valid && (rd_beat_cnt_r + 4'd1 == rd_expect_r);

  // ==========================================================================
  // THE BENCH'S OWN WITNESSES
  // ==========================================================================
  // ONE OBSERVATION PER REQUEST, at the share's accept, not one per stalled
  // cycle -- the arena holds `valid` until the share picks it, which under a
  // busy socket is many cycles, and a per-cycle count would report the stall
  // rather than the traffic.
  wire        arena_take_c  = arena_req.valid && arena_rsp.ready;
  wire [31:0] arena_addr32_c = {5'b0, arena_req.addr};
  wire arena_in_v0_c =
      (arena_addr32_c >= ZHAO_PARAMBUF_VIEW0_BASE)
   && (arena_addr32_c <  ZHAO_PARAMBUF_VIEW0_BASE + ZHAO_PARAMBUF_VIEW_SPAN);
  wire arena_in_v1_c =
      (arena_addr32_c >= ZHAO_PARAMBUF_VIEW1_BASE)
   && (arena_addr32_c <  ZHAO_PARAMBUF_VIEW1_BASE + ZHAO_PARAMBUF_VIEW_SPAN);
  wire arena_in_scr_c =
      (arena_addr32_c >= ZHAO_PARAMBUF_SCRATCH_BASE)
   && (arena_addr32_c <  ZHAO_PARAMBUF_SCRATCH_BASE + ZHAO_PARAMBUF_SCRATCH_SPAN);

  // THE DIRECTORY WRITE, TRACKED FROM PORTS ALONE.  No hierarchical reference
  // into the DUT: a witness that reaches inside the block it is watching stops
  // being independent of it.  It opens at the scratch-addressed write request
  // and closes at that write's last data beat.
  logic dirw_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) dirw_q <= 1'b0;
    else if (arena_take_c && arena_req.write && arena_in_scr_c) dirw_q <= 1'b1;
    else if (arena_wvalid && arena_wready && arena_wlast)       dirw_q <= 1'b0;
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      req_unaligned_o       <= 32'd0;
      wr_in_view0_o         <= 32'd0;
      wr_in_view1_o         <= 32'd0;
      wr_in_scratch_o       <= 32'd0;
      wr_elsewhere_o        <= 32'd0;
      scr_overlap_o         <= 32'd0;
      scratch_open_clocks_o <= 32'd0;
      arena_beat_stray_q    <= 32'd0;
    end else begin
      // ONE OBSERVATION PER REQUEST, at the guard's accept, covering BOTH
      // requesters -- the walker's 16-byte descriptor reads are as exposed to
      // this as the arena's 24-byte vertex writes.
      if (g_req.valid && g_rsp.ready && (g_req.addr[3:0] != 4'd0))
        req_unaligned_o <= req_unaligned_o + 32'd1;
      if (arena_take_c) begin
        if      (arena_in_v0_c)  wr_in_view0_o   <= wr_in_view0_o + 32'd1;
        else if (arena_in_v1_c)  wr_in_view1_o   <= wr_in_view1_o + 32'd1;
        else if (arena_in_scr_c) wr_in_scratch_o <= wr_in_scratch_o + 32'd1;
        else                     wr_elsewhere_o  <= wr_elsewhere_o + 32'd1;
      end
      // TWO OWNERS, ONE AT A TIME.  If this ever moves, the walker was reading
      // the directory while the producer was writing it.
      if (dirw_q && scr_grant_o) scr_overlap_o <= scr_overlap_o + 32'd1;
      if (pb_scratch_w) scratch_open_clocks_o <= scratch_open_clocks_o + 32'd1;
      if (sh_beat_valid[0]) arena_beat_stray_q <= arena_beat_stray_q + 32'd1;
    end
  end

  /* verilator lint_off UNUSEDSIGNAL */
  logic [31:0] unused_arena_beat_stray;
  /* verilator lint_on UNUSEDSIGNAL */
  assign unused_arena_beat_stray = arena_beat_stray_q;

  // ===========================================================================
  // ENTRY I54's LIVE CHAIN
  // ===========================================================================
  logic        pa_seal_fire_w;
  logic        arena_ck_ready_w, arena_lk_ready_w;
  logic [17:0] tidq_id_w;

  assign ck_ready_o    = arena_ck_ready_w;
  // THE CHUNK TAP FOLLOWS THE PRODUCER THAT OWNS THE INTAKE. With
  // `ab_enable_i` high these four ports describe GEOM.ARENABIN's chunk as it
  // is OFFERED, so a test can check a record before it has been through SDRAM
  // and come back; with it low they describe the hand-driven `ck_*` pair the
  // arena's own directed cases use.
  assign ck_tap_fire_o  = ab_enable_i ? (ab_ck_valid_w && ab_ck_ready_w)
                                      : (ck_valid_i && arena_ck_ready_w);
  assign ck_tap_count_o = ab_enable_i ? ab_ck_count_w      : ck_count_i;
  assign ck_tap_id0_o   = ab_enable_i ? ab_ck_ids_w[31:0]  : ck_ids_i[31:0];
  assign ck_tap_id1_o   = ab_enable_i ? ab_ck_ids_w[63:32] : ck_ids_i[63:32];
  assign ck_tap_next_o  = ab_enable_i ? ab_ck_next_w       : ck_next_i;

  // THE IDENTITY QUEUE, exactly as `zhao_console_core` composes it: pushed on
  // the RETIRE beat with the acceptance as a payload bit, popped on the beat
  // the binner takes the triangle (the console's shell door), flushed on the
  // arena's own seal fire.
  // ======================= GEOM.ARENABIN, THE INDEPENDENT PRODUCER =========
  // ALWAYS ELABORATED, unlike the on-chip half below. It has no dependency on
  // the binner, the identity queue or the serialiser, which is the point.
  logic                    ab_ck_valid_w, ab_ck_ready_w;
  logic [31:0]             ab_ck_next_w;
  logic [15:0]             ab_ck_count_w;
  logic [CHUNK_IDS*32-1:0] ab_ck_ids_w;
  logic                    ab_lk_valid_w, ab_lk_ready_w;
  logic [17:0]             ab_lk_index_w;
  logic [31:0]             ab_lk_next_w;
  logic [15:0]             ab_lk_count_w;

  // The arena's two grants, routed to whichever producer owns the intake.
  assign ab_ck_ready_w = arena_ck_ready_w && ab_enable_i;
  assign ab_lk_ready_w = arena_lk_ready_w && ab_enable_i;
  assign lk_ready_o    = arena_lk_ready_w && !ab_enable_i;

  zhao_geom_arenabin #(
      .GRID_W(24), .GRID_H(24), .TILES(576), .TIDX_W(10),
      .STAGE_IDS(CHUNK_IDS), .ID_W(18), .CHIDX_W(18), .ACC_W(36)
  ) u_arenabin (
      .clk(clk), .rst_n(rst_n),
      // THE ARENA'S OWN SEAL, not a bench pulse. `pa_seal_fire_w` is the clock
      // the frame's quota and generation actually take effect on, which is
      // what `zhao_geom_chunkser` takes below and what the console wires. A
      // separate bench edge would let this block's frame and the arena's drift
      // apart in simulation in a way the composed console cannot reproduce.
      .frame_start_i(pa_seal_fire_w),
      .geom_done_i  (ab_geom_done_i),
      .grid_w_i(ab_grid_w_i), .grid_h_i(ab_grid_h_i),
      .tri_valid_i(ab_tri_valid_i), .tri_ready_o(ab_tri_ready_o),
      .tri_kx0_i(ab_kx0_i), .tri_ky0_i(ab_ky0_i), .tri_kc0_i(ab_kc0_i),
      .tri_kx1_i(ab_kx1_i), .tri_ky1_i(ab_ky1_i), .tri_kc1_i(ab_kc1_i),
      .tri_kx2_i(ab_kx2_i), .tri_ky2_i(ab_ky2_i), .tri_kc2_i(ab_kc2_i),
      .tri_tl_i(ab_tl_i),
      .tri_min_x_i(ab_min_x_i), .tri_max_x_i(ab_max_x_i),
      .tri_min_y_i(ab_min_y_i), .tri_max_y_i(ab_max_y_i),
      .tri_arena_id_i(ab_arena_id_i),
      .tri_id_ok_i   (ab_id_ok_i),
      .ck_valid_o   (ab_ck_valid_w),
      .ck_ready_i   (ab_ck_ready_w),
      .ck_next_o    (ab_ck_next_w),
      .ck_count_o   (ab_ck_count_w),
      .ck_ids_o     (ab_ck_ids_w),
      .ck_alloc_id_i(ck_alloc_id_o),
      .ck_accept_i  (ck_accept_o),
      .lk_valid_o(ab_lk_valid_w),
      .lk_ready_i(ab_lk_ready_w),
      .lk_index_o(ab_lk_index_w),
      .lk_next_o (ab_lk_next_w),
      .lk_count_o(ab_lk_count_w),
      .head_tile_i (ab_head_tile_i),
      .head_chunk_o(ab_head_chunk_o),
      .head_valid_o(ab_head_valid_o),
      .bin_frame_done_o(ab_frame_done_o),
      .tris_binned_o    (ab_tris_binned_o),
      .tris_unnamed_o   (ab_tris_unnamed_o),
      .refs_binned_o    (ab_refs_binned_o),
      .chunks_emitted_o (ab_chunks_emitted_o),
      .links_patched_o  (ab_links_patched_o),
      .tiles_with_refs_o(ab_tiles_with_refs_o),
      .chunk_refused_o  (ab_chunk_refused_o),
      .intake_stall_o   (ab_intake_stall_o),
      .flush_cut_o      (ab_flush_cut_o),
      .max_tile_chunks_o(ab_max_tile_chunks_o),
      .overflow_o       (ab_overflow_o),
      .busy_o           (ab_busy_o)
  );

  // THE PRODUCER'S PRICE, IN THE SAME UNIT AS THE OTHER SIDE. Every clock the
  // producer is busy, accumulated -- so `ab_busy_clocks_o / ab_refs_binned_o`
  // is clocks per TILE REFERENCE, which is what `price_the_swap` divides on
  // the on-chip side. Accumulated rather than latched-at-last-handshake,
  // because this block's cost is dominated by SDRAM round trips that happen
  // BETWEEN references and a span measurement would attribute them correctly
  // only by accident.
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n)            ab_busy_clocks_o <= 32'd0;
    else if (ab_busy_o)    ab_busy_clocks_o <= ab_busy_clocks_o + 32'd1;
  end

  // ======================= THE ON-CHIP HALF, WHICH IS OPTIONAL =============
  // Explicit `generate` / `endgenerate` and an explicit `else`: Quartus 17
  // rejects the implicit forms, `check_quartus17_syntax.py` scans `tests` too,
  // and this file has no licence to be the exception.
  generate
  if (HAVE_ONCHIP) begin : g_onchip

  zhao_geom_tidq #(.ID_W(18), .DEPTH(8)) u_tidq (
      .clk        (clk),
      .rst_n      (rst_n),
      .flush_i    (pa_seal_fire_w),
      .push_i     (vid_retire_i),
      .id_ok_i    (vid_id_ok_i),
      .id_i       (vid_id_i),
      .pop_i      (bin_tri_valid_i && bin_tri_ready_o),
      .id_o       (tidq_id_w),
      .underflow_o(tidq_underflow_o),
      .overflow_o (tidq_overflow_o),
      .unnamed_o  (tidq_unnamed_o),
      .level_o    ()
  );

  // THE METADATA IMAGE IS THE CONSOLE'S, BIT FOR BIT. `tri_flat_request_i`
  // occupies [297:0] and `tri_continuation_tail_i` [345:298]; the tail's
  // `vertex_rgb` is its own [47:24], so the arena index lands at 322. Building
  // it here rather than taking METAW=1157's default is the difference between
  // testing the shipped placement and testing a placement nobody uses.
  // ARENACOMPOSE: the binner no longer SLICES that field -- `ARENA_ID_LO` /
  // `ARENA_ID_W` went with the serialise pass -- but the field is still in the
  // shipped metadata and is still built here, because the point of this image
  // is that it is the console's.
  localparam int unsigned CS_METAW       = 1877;
  wire [CS_METAW-1:0] cs_meta_w = {
      {(CS_METAW - 346){1'b0}},   // the six attribute planes, area2, min_x
      6'd0, tidq_id_w,            // tail [47:24]: the dead vertex_rgb field
      24'd0,                      // tail [23:0]: alpha, effect tag, stencil ref
      298'd0                      // tri_flat_request_i
  };

  zhao_geom_binner_v2 #(
      .METAW(CS_METAW)
  ) u_cs_binner (
      .clk(clk), .rst_n(rst_n),
      .frame_begin_i(bin_frame_begin_i),
      .frame_end_i  (bin_frame_end_i),
      .grid_w_i(bin_grid_w_i), .grid_h_i(bin_grid_h_i),
      .tri_valid_i(bin_tri_valid_i), .tri_ready_o(bin_tri_ready_o),
      .tri_kx0_i(bin_kx0_i), .tri_ky0_i(bin_ky0_i), .tri_kc0_i(bin_kc0_i),
      .tri_kx1_i(bin_kx1_i), .tri_ky1_i(bin_ky1_i), .tri_kc1_i(bin_kc1_i),
      .tri_kx2_i(bin_kx2_i), .tri_ky2_i(bin_ky2_i), .tri_kc2_i(bin_kc2_i),
      .tri_tl_i(bin_tl_i),
      .tri_ax_i(bin_ax_i), .tri_ay_i(bin_ay_i),
      .tri_bx_i(bin_bx_i), .tri_by_i(bin_by_i),
      .tri_cx_i(bin_cx_i), .tri_cy_i(bin_cy_i),
      .tri_min_x_i(bin_min_x_i), .tri_max_x_i(bin_max_x_i),
      .tri_min_y_i(bin_min_y_i), .tri_max_y_i(bin_max_y_i),
      .tri_src_id_i(bin_src_id_i),
      .tri_meta_i(cs_meta_w),
      .tok_req_o(), .tok_grant_i(1'b1),
      .job_valid_o(bin_job_valid_o), .job_ready_i(bin_job_ready_i),
      .job_ax_o(), .job_ay_o(), .job_bx_o(), .job_by_o(),
      .job_cx_o(), .job_cy_o(),
      .job_first_o(bin_job_first_o), .job_last_o(bin_job_last_o),
      .job_tile_x_o(bin_job_tile_x_o), .job_tile_y_o(bin_job_tile_y_o),
      .job_src_id_o(bin_job_src_id_o), .job_meta_o(),
      .job_profile_bad_o(),
      .drain_busy_o(), .drain_done_o(bin_drain_done_o),
      .tile_references_o(bin_tile_references_o),
      .max_tile_list_depth_o(),
      .triangles_culled_o(bin_triangles_culled_o),
      .overflow_o(bin_overflow_o),
      .arena_full_o(), .arena_used_o()
  );

  end else begin : g_no_onchip
    // THE ON-CHIP HALF IS ABSENT. Its outputs are driven to their inert values
    // so the port list does not change shape between the two elaborations --
    // a bench whose ports move with a parameter makes two different C++ APIs
    // out of one header, and the driver would then silently read a field that
    // has moved.
    assign bin_tri_ready_o       = 1'b0;
    assign bin_job_valid_o       = 1'b0;
    assign bin_job_tile_x_o      = 12'sd0;
    assign bin_job_tile_y_o      = 12'sd0;
    assign bin_job_first_o       = 1'b0;
    assign bin_job_last_o        = 1'b0;
    assign bin_job_src_id_o      = 16'd0;
    assign bin_drain_done_o      = 1'b0;
    assign bin_tile_references_o = 32'd0;
    assign bin_triangles_culled_o= 32'd0;
    assign bin_overflow_o        = 1'b0;
    assign tidq_underflow_o      = 32'd0;
    assign tidq_overflow_o       = 32'd0;
    assign tidq_unnamed_o        = 32'd0;
    assign tidq_id_w             = 18'd0;
  end
  endgenerate

endmodule

`default_nettype wire
