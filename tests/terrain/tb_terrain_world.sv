// tb_terrain_world.sv - STEP 8: the composed terrain world path.
//
// TERRAIN.SEQ, TERRAIN.RESIDENCY (v2), TERRAIN.PAGELOADER, TERRAIN.WRITEBACK
// and MEM.HPS.ARBITER in ONE top, with the harness supplying only the frame
// ring, the HPS memory and the fabric it rides on. Every block here is the
// REAL RTL. Nothing that exists is modelled.
//
// ---------------------------------------------------------------------------
// WHAT IS COMPOSED, AND WHAT IS NOT, SAID PLAINLY
// ---------------------------------------------------------------------------
// Composed, RTL to RTL, for the first time:
//
//   frame ring (harness)
//     -> zhao_terrain_seq          lookup / claim / pin  -> zhao_terrain_residency_v2
//                                  writeback job         -> zhao_terrain_writeback
//                                  load job              -> zhao_terrain_pageloader
//     zhao_terrain_pageloader      fin                   -> zhao_terrain_residency_v2
//     zhao_terrain_writeback       barrier release       -> zhao_terrain_residency_v2
//     zhao_terrain_pageloader  \
//                               >- zhao_hps_arbiter -> played MEM.HPS.BRIDGE
//     zhao_terrain_writeback   /
//     zhao_terrain_pageloader  writes the page pool -\
//                                                     >- one played VRAM image
//     zhao_terrain_writeback   reads  the page pool  -/
//
// NOT composed, and the reason is a MISSING BLOCK rather than a choice:
//
//   TERRAIN.SEQ's `is_*` patch issue cannot reach TERRAIN.PATCH, because
//   nothing in the tree turns a resident page slot into TERRAIN.PATCH's
//   `vtx_*` lattice intake. TERRAIN.PATCH has no memory port at all (its
//   ports are a vertex stream in and a composed vertex stream out), and the
//   only RTL that drives `vtx_valid_i` anywhere in `fpga/rtl/` is
//   `zhao_prod_top.sv`, from an LFSR. So the chain PATCH -> COMPCACHE -> LOD
//   -> TESS -> NORMALS is reachable only from a harness on BOTH ends, which
//   is decoration rather than composition. The issue port is taken by a
//   harness sink here, its every field is checked, and the missing intake is
//   reported as a finding.
//
//   The compose-slot half of TERRAIN.COMPCACHE *is* composed: the frame-scoped
//   allocator lives in TERRAIN.SEQ and its `is_cslot_valid_o` / `is_cslot_o`
//   are checked against the reference on every issue.
//
// ---------------------------------------------------------------------------
// THE LOOKUP SHIM, AND WHY IT HAD TO BE INVENTED
// ---------------------------------------------------------------------------
// `zhao_terrain_seq` offers a lookup for exactly ONE cycle -- `lu_valid_o` is
// `(st == S_LOOKUP)` and S_LOOKUP advances unconditionally -- and then waits in
// S_WAIT_LU for an answer that has no `ready` and no timeout.
//
// `zhao_terrain_residency_v2` accepts a lookup only when NO mutation is
// present that cycle (`ev_c == EV_NONE`) and no same-set read-during-write
// hazard is in flight. A lookup that loses is not backpressured and not
// answered: "a query that loses is simply not answered this clock".
//
// In isolation those two are consistent, because TERRAIN.SEQ's own bench
// answers every lookup on a fixed latency. In composition they are not: the
// loader's `fin`, the writeback's barrier release, a dirty mark and an unpin
// all arrive on cycles nobody chose, and one landing on the same clock as
// `lu_valid_o` deadlocks the frame forever.
//
// THE OBVIOUS GLUE ALSO DEADLOCKS, and that is worth recording. "Defer every
// outside mutation to the frame boundary" makes the lookup safe and starves
// the loader: TERRAIN.PAGELOADER holds `fin_valid_o` until the directory takes
// it and refuses the next job while it does, and TERRAIN.SEQ blocks in S_LOAD
// on `ld_ready_i`. That arrangement was built and measured here first: it got
// two records into an eight-record frame and then stopped forever.
//
// What works is holding the LOOKUP rather than deferring the world -- see
// `lu_hold_q` below. `cfg_dir_gate_i` clears the shim, and phase A of the
// suite runs with it clear so the underlying defect is reproduced on demand
// rather than described.
//
// ---------------------------------------------------------------------------
// THE WITNESS THAT MATTERS: BYTES, NOT JOB ORDER
// ---------------------------------------------------------------------------
// T4's barrier says a dirty victim's sheet must be journalled BEFORE the page
// that displaces it is written. TERRAIN.SEQ's own suite proves it emits the
// writeback JOB before the load JOB, and both counters read 1 either way. But
// a job is not a byte. `wat_*` below watches ONE page-pool slot and records the
// cycle of the FIRST loader write beat into it and the cycle of the LAST
// writeback read beat out of it. Those two numbers are the barrier; the job
// order is only its intention.
//
// ---------------------------------------------------------------------------
// PLAYED MODELS ARE TRANSCRIBED, AND THE REAL GUARD WATCHES
// ---------------------------------------------------------------------------
// `tools/rtl/check_guard_verdict.py` records two geometry fetchers reading
// every guard PASS as a denial for months, because every bench that played a
// guard raised `ready` and `ok` together. Both played guards here are copied
// from `zhao_mem_guard.sv` line by line -- `ready` is the LEVEL `!fwd_active`,
// `ok` and `violation` are REGISTERED PULSES one cycle later -- and the REAL
// `zhao_mem_guard` is instantiated on both request streams as an observer.
// It must PASS every loader write (TERRAIN.PAGE_POOL is write-only to
// TERRAIN.BUILD) and REFUSE every writeback read (the read arm is reported and
// not merged). Both directions are counted here, in the composed setting.
`default_nettype none

module tb_terrain_world
  import zhao_pkg::*;
(
    input var logic clk,
    input var logic rst_n,

    // ---- backing memories, a word at a time from the C++ side --------------
    input  var logic        mw_en,
    input  var logic [ 1:0] mw_sel,    // 0 = HPS staging, 1 = page pool, 2 = journal
    input  var logic [31:0] mw_addr,   // 64-bit word index within the selected image
    input  var logic [63:0] mw_data,
    input  var logic [ 1:0] mr_sel,
    input  var logic [31:0] mr_addr,
    output var logic [63:0] mr_data,

    // ---- configuration ------------------------------------------------------
    input var logic [31:0] cfg_epoch_i,
    input var logic [31:0] cfg_hps_arena_base_i,
    input var logic [31:0] cfg_hps_arena_bytes_i,
    input var logic [31:0] cfg_journal_base_i,
    input var logic [31:0] cfg_journal_bytes_i,
    input var logic [15:0] cfg_load_budget_i,

    // THE MISSING GLUE, MADE A KNOB. See the header.
    input var logic cfg_dir_gate_i,

    // THE MISSING BLOCK, MADE A KNOB. A claim sets `mips_stale`, the loader's
    // `fin` moves the entry RESERVED -> MIPGEN, and only a SECOND completion
    // moves MIPGEN -> RESIDENT_CLEAN -- which is the only state a lookup hits
    // on. Nothing in the tree can send that second completion:
    // `zhao_terrain_mipgen` has no slot, no generation, no epoch and no
    // completion port at all, just a bare `done_o` pulse with no page identity.
    // So the composed world layer never calls any page ground. With this set
    // the harness plays the missing completion; with it clear the machine is
    // exactly as assembled, and phase C proves it draws nothing.
    input var logic cfg_mipgen_fin_i,

    // THE BARRIER THAT IS NOT THERE, MADE A KNOB. T4 says a dirty victim's F
    // sheet reaches the journal BEFORE the page that displaces it is written.
    // TERRAIN.SEQ emits the writeback job before the load job and then stops
    // caring: S_WB -> S_LOAD advances on `wb_ready_i`, which is the writeback
    // ACCEPTING the job, not completing it. With this set the harness holds the
    // load job until TERRAIN.WRITEBACK's `done_valid_o` for the outstanding
    // sheet, which is what the barrier actually requires. It is a knob so the
    // suite can measure the machine BOTH ways: with it clear the defect
    // reproduces, with it set the barrier holds and the journal gets the right
    // bytes. A one-sided check that only ever fails is not evidence.
    input var logic cfg_wb_barrier_i,

    // ARM THE BARRIER WITNESS ON THE VICTIM THE DIRECTORY ACTUALLY PICKS.
    // The victim slot is not known until the writeback job is accepted, and by
    // the time the C++ side has read it out of the action log the reads it
    // wanted to time have already happened. With this set the watch latches
    // `wb_slot_o` on the job's acceptance and starts recording from that
    // cycle, which is the first cycle at which anything can touch the slot.
    input var logic cfg_wat_auto_i,

    // The engine's unpin, which no engine exists to send (TERRAIN.SEQ pins and
    // deliberately never unpins). 0 = never unpin, so pin pressure and T9's
    // rule-5 refusal are reachable.
    input var logic [15:0] cfg_unpin_delay_i,

    // SW.STREAM's journal doorbell, played.
    input var logic [15:0] cfg_ack_delay_i,
    input var logic        cfg_ack_ok_i,

    // ---- played fabric timing ----------------------------------------------
    input var logic [7:0] cfg_req_latency_i,   // bridge accept -> first beat
    input var logic [7:0] cfg_beat_gap_i,      // idle cycles between bridge beats
    input var logic [7:0] cfg_grant_hold_i,    // guard `ready` low after an accept
    input var logic [7:0] cfg_wready_gap_i,    // idle cycles between pool write beats
    input var logic [7:0] cfg_rd_latency_i,    // pool read accept -> first beat
    input var logic [7:0] cfg_rd_gap_i,        // idle cycles between pool read beats

    // ---- the frame ring -----------------------------------------------------
    input  var logic        fr_start,
    input  var logic [31:0] fr_epoch,
    input  var logic [15:0] fr_patch_count,
    input  var logic [31:0] fr_sequence,
    output var logic        fr_busy,
    output var logic        fr_done,

    // ---- T5's sealed patch-list record -------------------------------------
    input  var logic        rec_valid,
    output var logic        rec_ready,
    input  var logic [31:0] rec_island,
    input  var logic [15:0] rec_ix,
    input  var logic [15:0] rec_iz,
    input  var logic [63:0] rec_hps_addr,
    input  var logic [31:0] rec_crc,
    input  var logic [15:0] rec_flags,
    input  var logic [ 7:0] rec_view_mask,
    input  var logic [ 7:0] rec_priority,
    input  var logic [31:0] rec_src_id,

    // ---- the engine's ready, and the writeback completion sink -------------
    input var logic is_ready,
    input var logic wbdone_ready,

    // ---- SURFACE.STAMP's dirty mark, played --------------------------------
    input  var logic        dm_valid,
    output var logic        dm_ready,
    input  var logic [15:0] dm_slot,
    input  var logic [ 7:0] dm_gen,
    input  var logic [31:0] dm_epoch,
    input  var logic        dm_bd,
    input  var logic        dm_f,
    input  var logic        dm_mips,

    // ---- what TERRAIN.SEQ did ----------------------------------------------
    output var logic        lu_valid,
    output var logic [31:0] lu_island,
    output var logic [15:0] lu_ix,
    output var logic [15:0] lu_iz,

    output var logic        cl_valid,
    output var logic        cl_ready,
    output var logic [31:0] cl_island,
    output var logic [15:0] cl_ix,
    output var logic [15:0] cl_iz,
    output var logic [31:0] cl_expect_crc,

    output var logic        pin_valid,
    output var logic        pin_ready,
    output var logic [15:0] pin_slot,
    output var logic [ 7:0] pin_gen,

    output var logic        wb_valid,
    output var logic        wb_ready,
    output var logic [15:0] wb_slot,
    output var logic [ 7:0] wb_gen,
    output var logic [31:0] wb_island,
    output var logic [15:0] wb_ix,
    output var logic [15:0] wb_iz,
    output var logic [31:0] wb_src_id,

    output var logic        ld_valid,
    output var logic        ld_ready,
    output var logic [15:0] ld_slot,
    output var logic [ 7:0] ld_gen,
    output var logic [31:0] ld_island,
    output var logic [15:0] ld_ix,
    output var logic [15:0] ld_iz,
    output var logic [63:0] ld_hps_addr,
    output var logic [31:0] ld_expect_crc,
    output var logic [31:0] ld_src_id,

    output var logic        is_valid,
    output var logic [15:0] is_slot,
    output var logic [ 7:0] is_gen,
    output var logic [31:0] is_island,
    output var logic [15:0] is_ix,
    output var logic [15:0] is_iz,
    output var logic        is_cslot_valid,
    output var logic [ 7:0] is_cslot,
    output var logic [15:0] is_flags,
    output var logic [ 7:0] is_view_mask,
    output var logic [ 7:0] is_priority,
    output var logic [31:0] is_src_id,

    output var logic        seq_frame_fault,
    output var logic [31:0] seq_fault_src_id,
    output var logic [31:0] seq_fault_island,
    output var logic [15:0] seq_fault_ix,
    output var logic [15:0] seq_fault_iz,
    output var logic        seq_err_stray_ans,

    // ---- what the REAL directory answered (fed to the oracle) --------------
    output var logic        ra_lu_valid,
    output var logic        ra_lu_hit,
    output var logic [15:0] ra_lu_slot,
    output var logic [ 7:0] ra_lu_gen,
    output var logic        ra_cl_valid,
    output var logic        ra_cl_same,
    output var logic        ra_cl_refused,
    output var logic [15:0] ra_cl_slot,
    output var logic [ 7:0] ra_cl_gen,
    output var logic        ra_cl_evicted,
    output var logic        ra_cl_ev_dirty,
    output var logic [31:0] ra_cl_ev_island,
    output var logic [15:0] ra_cl_ev_ix,
    output var logic [15:0] ra_cl_ev_iz,
    output var logic [ 7:0] ra_cl_ev_gen,

    // ---- TERRAIN.SEQ's fourteen counters -----------------------------------
    output var logic [31:0] s_records_consumed,
    output var logic [31:0] s_patches_issued,
    output var logic [31:0] s_prefetch_resident,
    output var logic [31:0] s_skipped_not_resident,
    output var logic [31:0] s_claims_issued,
    output var logic [31:0] s_claims_refused,
    output var logic [31:0] s_claims_same,
    output var logic [31:0] s_loads_issued,
    output var logic [31:0] s_loads_deferred,
    output var logic [31:0] s_writebacks_issued,
    output var logic [31:0] s_compose_slots_used,
    output var logic [31:0] s_pins_issued,
    output var logic [31:0] s_drained,
    output var logic [31:0] s_frame_faults,

    // ---- TERRAIN.RESIDENCY's evidence --------------------------------------
    output var logic        res_ready,
    output var logic [31:0] r_hits,
    output var logic [31:0] r_misses,
    output var logic [31:0] r_claims,
    output var logic [31:0] r_evictions,
    output var logic [31:0] r_dirty_evictions,
    output var logic [31:0] r_refused_all_pinned,
    output var logic [31:0] r_stale_events,
    output var logic [31:0] r_crc_failures,
    output var logic [31:0] r_resident,

    // ---- TERRAIN.PAGELOADER's evidence -------------------------------------
    output var logic        pl_fin_valid,
    output var logic        pl_fin_ready,
    output var logic [15:0] pl_fin_slot,
    output var logic [ 7:0] pl_fin_gen,
    output var logic        pl_fin_ok,
    output var logic [ 3:0] pl_fin_verdict,
    output var logic [31:0] pl_pages_loaded,
    output var logic [31:0] pl_pages_faulted,
    output var logic [31:0] pl_pages_refused,
    output var logic [31:0] pl_crc_fails,
    output var logic [31:0] pl_hdr_ident_fails,
    output var logic [31:0] pl_incomplete,
    output var logic [31:0] pl_guard_denied,
    output var logic [31:0] pl_bridge_errs,
    output var logic [31:0] pl_load_bytes,

    // ---- TERRAIN.WRITEBACK's evidence --------------------------------------
    output var logic        wbrel_valid,       // barrier release into the directory
    output var logic        wbrel_ready,
    output var logic [15:0] wbrel_slot,
    output var logic [ 7:0] wbrel_gen,
    output var logic        wbdone_valid,
    output var logic        wbdone_ok,
    output var logic [ 3:0] wbdone_verdict,
    output var logic [15:0] wbdone_slot,
    output var logic [31:0] wbdone_seq,
    output var logic [31:0] wb_sheets_written,
    output var logic [31:0] wb_sheets_refused,
    output var logic [31:0] wb_sheets_faulted,
    output var logic [31:0] wb_hdr_ident_fails,
    output var logic [31:0] wb_guard_denied,
    output var logic [31:0] wb_bridge_errs,
    output var logic [31:0] wb_acks_ok,
    output var logic [31:0] wb_acks_nak,
    output var logic [31:0] wb_acks_unmatched,
    output var logic [31:0] wb_acks_after_epoch,
    output var logic [31:0] wb_acks_overdue,
    output var logic [31:0] wb_seq_conflicts,
    output var logic [31:0] wb_bytes,
    output var logic [31:0] wb_outstanding_hwm,

    // ---- MEM.HPS.ARBITER's evidence ----------------------------------------
    output var logic [31:0] arb_c0_bursts,
    output var logic [31:0] arb_c1_bursts,
    output var logic [31:0] arb_c1_wait_cycles,

    // ---- the real MEM.GUARD, observing both terrain clients ----------------
    output var logic [31:0] gobs_wr_ok,       // loader writes the guard passed
    output var logic [31:0] gobs_wr_viol,     // ...and refused
    output var logic [31:0] gobs_rd_ok,       // writeback reads the guard passed
    output var logic [31:0] gobs_rd_viol,     // ...and refused

    // ---- the harness's own witnesses ---------------------------------------
    input  var logic        wat_arm,        // pulse: clear and start watching
    input  var logic [15:0] wat_slot,
    output var logic [31:0] wat_wr_first,   // cycle of 1st loader write beat into it
    output var logic [31:0] wat_wr_count,
    output var logic [31:0] wat_rd_last,    // cycle of last writeback read beat from it
    output var logic [31:0] wat_rd_count,
    output var logic [31:0] cyc,

    input  var logic        stat_clear,
    output var logic [31:0] h_pool_writes,   // write beats that landed in the pool
    output var logic [31:0] h_pool_oob,      // ...and outside it
    output var logic [31:0] h_jnl_writes,
    output var logic [31:0] h_jnl_oob,
    output var logic [31:0] h_unpins,
    output var logic [31:0] h_acks_sent,
    output var logic [31:0] h_lu_offers,     // lookups TERRAIN.SEQ presented
    output var logic [31:0] h_lu_dropped,    // ...that the directory did not take
    output var logic [31:0] h_mipgen_fins,   // mip completions the harness had to play
    output var logic [31:0] h_pins,          // pins the played engine took responsibility for
    output var logic [31:0] h_pin_drops,     // ...and pins its mirror could not hold
    output var logic [31:0] h_barrier_stalls, // cycles the load job was held for the barrier
    output var logic [31:0] h_wb_ticket,      // the journal ticket the NEXT sheet will use
    output var logic [15:0] h_wat_slot        // the slot the witness settled on
);

  // =========================================================================
  // THE SHAPES. Every one is a law with a citation, not a tuning knob.
  // =========================================================================
  localparam int unsigned PAGE_BYTES  = 21376;                 // terrain_rules 2 / 7
  localparam int unsigned PAGE_BEATS  = PAGE_BYTES / 8;        // 2,672
  localparam int unsigned POOL_SLOTS  = 1024;                  // T2 / terrain_rules 8
  localparam logic [ZHAO_VRAM_ADDR_BITS-1:0] POOL_BASE = 27'h400_0000;

  localparam int unsigned F_OFF   = 10694;                     // layer F, page-relative
  localparam int unsigned F_BYTES = 8192;                      // 64x64 x {tag, strength}

  // The whole pool is modelled, because the directory's set hash decides which
  // slot a key lands in and a bench that only modelled the slots it expected
  // would report a hash disagreement as a memory fault.
  localparam int unsigned VWORDS = POOL_SLOTS * PAGE_BEATS;    // 2,735,104
  localparam int unsigned VW     = $clog2(VWORDS);

  localparam int unsigned STAGE_PAGES = 64;
  localparam int unsigned HWORDS      = STAGE_PAGES * PAGE_BEATS;  // 171,008
  localparam int unsigned HW          = $clog2(HWORDS);

  localparam int unsigned JNL_ENTRIES = 16;
  localparam int unsigned JWORDS      = JNL_ENTRIES * (F_BYTES / 8);  // 16,384
  localparam int unsigned JW          = $clog2(JWORDS);

  localparam int unsigned WR_BURSTS = F_BYTES / 64;  // 128 journal bursts per sheet

  localparam int unsigned SLOTW   = 10;  // 256 sets x 4 ways
  localparam int unsigned MEMSLOT = 11;  // the pool clients' one-wider slot index
  localparam int unsigned GENW    = 8;
  localparam int unsigned SEQW    = 16;
  localparam int unsigned CSLOTS  = 16;  // T6's number is 256; the law is the same

  logic [63:0] vram_mem [VWORDS];
  logic [63:0] hps_mem  [HWORDS];
  logic [63:0] jnl_mem  [JWORDS];

  // =========================================================================
  // TERRAIN.SEQ
  // =========================================================================
  // Outputs this composition does not consume. Named rather than left empty:
  // an empty pin connection is a lint warning here and a silent drop elsewhere.
  logic [31:0] seq_lu_epoch, seq_cl_epoch;
  logic        nc_chk_valid, nc_chk_stale;

  logic [SLOTW-1:0] q_pin_slot, q_wb_slot, q_ld_slot, q_is_slot;
  logic [3:0]       q_is_cslot;

  assign pin_slot = {{(16-SLOTW){1'b0}}, q_pin_slot};
  assign wb_slot  = {{(16-SLOTW){1'b0}}, q_wb_slot};
  assign ld_slot  = {{(16-SLOTW){1'b0}}, q_ld_slot};
  assign is_slot  = {{(16-SLOTW){1'b0}}, q_is_slot};
  assign is_cslot = {4'd0, q_is_cslot};

  logic               d_lu_ans_valid, d_lu_ans_hit;
  logic [SLOTW-1:0]   d_lu_ans_slot;
  logic [GENW-1:0]    d_lu_ans_gen;
  logic               d_cl_ans_valid, d_cl_ans_same, d_cl_ans_refused;
  logic [SLOTW-1:0]   d_cl_ans_slot;
  logic [GENW-1:0]    d_cl_ans_gen;
  logic               d_cl_evicted, d_cl_ev_dirty;
  logic [31:0]        d_cl_ev_island;
  logic signed [15:0] d_cl_ev_ix, d_cl_ev_iz;
  logic [GENW-1:0]    d_cl_ev_gen;

  logic [SEQW-1:0]    q_cl_seq;
  logic signed [15:0] q_lu_ix, q_lu_iz, q_cl_ix, q_cl_iz;
  logic signed [15:0] q_wb_ix, q_wb_iz, q_ld_ix, q_ld_iz, q_is_ix, q_is_iz;
  logic signed [15:0] q_fault_ix, q_fault_iz;
  logic [31:0]        q_pin_epoch, q_wb_epoch, q_ld_epoch, q_is_epoch;

  assign lu_ix = q_lu_ix;
  assign lu_iz = q_lu_iz;
  assign cl_ix = q_cl_ix;
  assign cl_iz = q_cl_iz;
  assign wb_ix = q_wb_ix;
  assign wb_iz = q_wb_iz;
  assign ld_ix = q_ld_ix;
  assign ld_iz = q_ld_iz;
  assign is_ix = q_is_ix;
  assign is_iz = q_is_iz;
  assign seq_fault_ix = q_fault_ix;
  assign seq_fault_iz = q_fault_iz;

  zhao_terrain_seq #(
      .COMPOSE_SLOTS(CSLOTS),
      .SLOTW(SLOTW),
      .GENW(GENW),
      .SEQW(SEQW)
  ) u_seq (
      .clk  (clk),
      .rst_n(rst_n),

      .fr_start_i      (fr_start),
      .fr_epoch_i      (fr_epoch),
      .fr_patch_count_i(fr_patch_count),
      .fr_sequence_i   (fr_sequence),
      .fr_busy_o       (fr_busy),
      .fr_done_o       (fr_done),

      .cfg_load_budget_i(cfg_load_budget_i),

      .rec_valid_i    (rec_valid),
      .rec_ready_o    (rec_ready),
      .rec_island_i   (rec_island),
      .rec_ix_i       (signed'(rec_ix)),
      .rec_iz_i       (signed'(rec_iz)),
      .rec_hps_addr_i (rec_hps_addr),
      .rec_crc_i      (rec_crc),
      .rec_flags_i    (rec_flags),
      .rec_view_mask_i(rec_view_mask),
      .rec_priority_i (rec_priority),
      .rec_src_id_i   (rec_src_id),

      .lu_valid_o    (lu_valid),

      .lu_ready_i    (lu_ready_real),
      .lu_epoch_o    (seq_lu_epoch),
      .lu_island_o   (lu_island),
      .lu_ix_o       (q_lu_ix),
      .lu_iz_o       (q_lu_iz),
      .lu_ans_valid_i(d_lu_ans_valid),
      .lu_ans_hit_i  (d_lu_ans_hit),
      .lu_ans_slot_i (d_lu_ans_slot),
      .lu_ans_gen_i  (d_lu_ans_gen),

      .cl_valid_o        (cl_valid),
      .cl_ready_i        (cl_ready),
      .cl_epoch_o        (seq_cl_epoch),
      .cl_island_o       (cl_island),
      .cl_ix_o           (q_cl_ix),
      .cl_iz_o           (q_cl_iz),
      .cl_expect_crc_o   (cl_expect_crc),
      .cl_seq_o          (q_cl_seq),
      .cl_ans_valid_i    (d_cl_ans_valid),
      .cl_ans_same_i     (d_cl_ans_same),
      .cl_ans_refused_i  (d_cl_ans_refused),
      .cl_ans_slot_i     (d_cl_ans_slot),
      .cl_ans_gen_i      (d_cl_ans_gen),
      .cl_ans_ev_dirty_i (d_cl_ev_dirty),
      .cl_ans_ev_island_i(d_cl_ev_island),
      .cl_ans_ev_ix_i    (d_cl_ev_ix),
      .cl_ans_ev_iz_i    (d_cl_ev_iz),
      .cl_ans_ev_gen_i   (d_cl_ev_gen),

      .pin_valid_o(pin_valid),
      .pin_ready_i(pin_ready),
      .pin_slot_o (q_pin_slot),
      .pin_gen_o  (pin_gen),
      .pin_epoch_o(q_pin_epoch),

      .wb_valid_o (wb_valid),
      .wb_ready_i (wb_ready),
      // THE WIDTH STEP IS DELIBERATE AND MUST NOT BE TRUNCATED AWAY.
      // TERRAIN.WRITEBACK's slot is one bit wider than TERRAIN.SEQ's on
      // purpose, so a computed slot of 1024 refuses instead of aliasing onto
      // slot 0. Passing the low ten bits alone would hand exactly that alias
      // back and release the barrier for a slot SEQ never evicted.
      //
      // So the completion is QUALIFIED rather than narrowed: a slot outside
      // SEQ's range cannot match anything it asked for, and is not offered.
      .wb_done_valid_i (wbdone_valid && !wbdone_slot_w[SLOTW]),
      .wb_done_slot_i  (wbdone_slot_w[SLOTW-1:0]),
      .wb_wait_cycles_o(seq_wb_wait),
      .wb_slot_o  (q_wb_slot),
      .wb_gen_o   (wb_gen),
      .wb_epoch_o (q_wb_epoch),
      .wb_island_o(wb_island),
      .wb_ix_o    (q_wb_ix),
      .wb_iz_o    (q_wb_iz),
      .wb_src_id_o(wb_src_id),

      .ld_valid_o     (ld_valid),
      .ld_ready_i     (ld_ready),
      .ld_slot_o      (q_ld_slot),
      .ld_gen_o       (ld_gen),
      .ld_epoch_o     (q_ld_epoch),
      .ld_island_o    (ld_island),
      .ld_ix_o        (q_ld_ix),
      .ld_iz_o        (q_ld_iz),
      .ld_hps_addr_o  (ld_hps_addr),
      .ld_expect_crc_o(ld_expect_crc),
      .ld_src_id_o    (ld_src_id),

      .is_valid_o      (is_valid),
      .is_ready_i      (is_ready),
      .is_slot_o       (q_is_slot),
      .is_gen_o        (is_gen),
      .is_epoch_o      (q_is_epoch),
      .is_island_o     (is_island),
      .is_ix_o         (q_is_ix),
      .is_iz_o         (q_is_iz),
      .is_cslot_valid_o(is_cslot_valid),
      .is_cslot_o      (q_is_cslot),
      .is_flags_o      (is_flags),
      .is_view_mask_o  (is_view_mask),
      .is_priority_o   (is_priority),
      .is_src_id_o     (is_src_id),

      .frame_fault_o  (seq_frame_fault),
      .fault_src_id_o (seq_fault_src_id),
      .fault_island_o (seq_fault_island),
      .fault_ix_o     (q_fault_ix),
      .fault_iz_o     (q_fault_iz),
      .err_stray_ans_o(seq_err_stray_ans),

      .records_consumed_o    (s_records_consumed),
      .patches_issued_o      (s_patches_issued),
      .prefetch_resident_o   (s_prefetch_resident),
      .skipped_not_resident_o(s_skipped_not_resident),
      .claims_issued_o       (s_claims_issued),
      .claims_refused_o      (s_claims_refused),
      .claims_same_o         (s_claims_same),
      .loads_issued_o        (s_loads_issued),
      .loads_deferred_o      (s_loads_deferred),
      .writebacks_issued_o   (s_writebacks_issued),
      .compose_slots_used_o  (s_compose_slots_used),
      .pins_issued_o         (s_pins_issued),
      .drained_o             (s_drained),
      .frame_faults_o        (s_frame_faults)
  );

  // =========================================================================
  // THE HARNESS LOOKUP SHIM -- the glue step 8 turns out to need
  // =========================================================================
  // TERRAIN.SEQ offers a lookup for one cycle and then waits forever;
  // TERRAIN.RESIDENCY drops a lookup that collides with any mutation. Something
  // has to reconcile those, and it is worth recording what did NOT work,
  // because the failure is itself a finding.
  //
  // THE OBVIOUS GLUE DEADLOCKS. "Hold every outside mutation until the frame
  // boundary" makes the lookup safe and starves the loader: TERRAIN.PAGELOADER
  // holds `fin_valid_o` until the directory takes it and refuses the next job
  // while it does, and TERRAIN.SEQ blocks in S_LOAD on `ld_ready_i`. Frame one
  // loads exactly one page and then stops forever. Measured, not reasoned:
  // that arrangement got 2 records into an 8-record frame.
  //
  // SO THE SHIM HOLDS THE LOOKUP INSTEAD OF DEFERRING THE WORLD. It latches the
  // request TERRAIN.SEQ pulses, blocks outside mutations only for the few
  // cycles the answer is outstanding, and RE-PRESENTS the request once if a
  // mutation accepted on the previous cycle would have raised the directory's
  // same-set hazard. That is enough to make acceptance certain: with mutations
  // blocked from the offer cycle onward, the only way to lose is a hazard from
  // the cycle before, and the hazard lasts exactly one clock.
  //
  // `cfg_dir_gate_i` clears the shim, and phase A of the suite runs with it
  // clear so the underlying defect is reproduced rather than described.
  // THE RETRY IS TIMED, NOT PREDICTED, and the first version was wrong in a way
  // worth keeping. It re-presented whenever a mutation had been accepted on the
  // previous cycle, reasoning that that is what raises the directory's hazard.
  // It is not: the hazard needs the mutation to have been in the SAME SET, and
  // the set is a CRC-8 the shim cannot compute. So every mutation into another
  // set produced a re-presentation of a lookup that had already been accepted,
  // the directory answered TWICE, and TERRAIN.SEQ's own `err_stray_ans_o`
  // tripwire caught the second one. The tripwire earning its keep on the
  // bench's bug is the same evidence it would give on a real one.
  //
  // The timed version cannot duplicate: an accepted lookup answers exactly two
  // cycles later, so the shim waits four and only then asks again.
  logic               lu_hold_q;
  logic [2:0]         lu_timer_q;
  logic [31:0]        lu_isl_q, lu_ep_q;
  logic signed [15:0] lu_ix_q, lu_iz_q;

  logic mut_open;
  assign mut_open = !cfg_dir_gate_i || (!lu_hold_q && !lu_valid);

  logic               shim_lu_valid;
  // The directory's real lookup ready, now that it has one. The shim below
  // no longer has to guess whether an offer landed.
  logic lu_ready_real;
  logic [31:0]        shim_lu_island, shim_lu_epoch;
  logic signed [15:0] shim_lu_ix, shim_lu_iz;

  assign shim_lu_valid  = lu_valid
                        || (cfg_dir_gate_i && lu_hold_q && (lu_timer_q == 3'd0));
  assign shim_lu_island = lu_valid ? lu_island    : lu_isl_q;
  assign shim_lu_epoch  = lu_valid ? seq_lu_epoch : lu_ep_q;
  assign shim_lu_ix     = lu_valid ? q_lu_ix      : lu_ix_q;
  assign shim_lu_iz     = lu_valid ? q_lu_iz      : lu_iz_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      lu_hold_q  <= 1'b0;
      lu_timer_q <= 3'd0;
      lu_isl_q   <= 32'd0;
      lu_ep_q    <= 32'd0;
      lu_ix_q    <= 16'sd0;
      lu_iz_q    <= 16'sd0;
    end else begin
      if (lu_valid) begin
        lu_hold_q  <= 1'b1;
        lu_timer_q <= 3'd4;
        lu_isl_q   <= lu_island;
        lu_ep_q    <= seq_lu_epoch;
        lu_ix_q    <= q_lu_ix;
        lu_iz_q    <= q_lu_iz;
      end else if (d_lu_ans_valid) begin
        lu_hold_q  <= 1'b0;
        lu_timer_q <= 3'd0;
      end else if (lu_hold_q) begin
        if (lu_timer_q == 3'd0) lu_timer_q <= 3'd4;   // asked again this cycle
        else                    lu_timer_q <= lu_timer_q - 3'd1;
      end
    end
  end

  // =========================================================================
  // TERRAIN.RESIDENCY (v2) -- the real directory
  // =========================================================================
  logic               g_fin_valid, g_fin_ready;
  logic [SLOTW-1:0]   g_fin_slot;
  logic [GENW-1:0]    g_fin_gen;
  logic [31:0]        g_fin_epoch, g_fin_crc;
  logic               g_fin_ok;

  logic             g_wb_valid, g_wb_ready;
  // T4's barrier is now driven by TERRAIN.WRITEBACK's OWN completion, which
  // this bench already taps as wbdone_valid/wbdone_slot_w. Introducing a
  // second, modelled completion beside the real block would be exactly the
  // decoration this composed test exists to avoid.
  logic [31:0]       seq_wb_wait;

  logic [SLOTW-1:0] g_wb_slot;
  logic [GENW-1:0]  g_wb_gen;
  logic [31:0]      g_wb_epoch;

  logic             g_unpin_valid, g_unpin_ready;
  logic [SLOTW-1:0] g_unpin_slot;
  logic [GENW-1:0]  g_unpin_gen;
  logic [31:0]      g_unpin_epoch;

  logic             g_dm_valid, g_dm_ready;

  zhao_terrain_residency_v2 #(
      .SETS(256),
      .WAYS(4),
      .GENW(GENW),
      .PINW(6),
      .SEQW(SEQW)
  ) u_res (
      .clk  (clk),
      .rst_n(rst_n),
      .ready_o(res_ready),

      .lu_valid_i (shim_lu_valid),

      .lu_ready_o (lu_ready_real),
      .lu_epoch_i (shim_lu_epoch),
      .lu_island_i(shim_lu_island),
      .lu_ix_i    (shim_lu_ix),
      .lu_iz_i    (shim_lu_iz),
      .lu_valid_o (d_lu_ans_valid),
      .lu_hit_o   (d_lu_ans_hit),
      .lu_slot_o  (d_lu_ans_slot),
      .lu_gen_o   (d_lu_ans_gen),

      .cl_valid_i     (cl_valid),
      .cl_ready_o     (cl_ready),
      .cl_epoch_i     (seq_cl_epoch),
      .cl_island_i    (cl_island),
      .cl_ix_i        (q_cl_ix),
      .cl_iz_i        (q_cl_iz),
      .cl_expect_crc_i(cl_expect_crc),
      .cl_seq_i       (q_cl_seq),

      .cl_valid_o         (d_cl_ans_valid),
      .cl_same_o          (d_cl_ans_same),
      .cl_refused_o       (d_cl_ans_refused),
      .cl_slot_o          (d_cl_ans_slot),
      .cl_gen_o           (d_cl_ans_gen),
      .cl_evicted_o       (d_cl_evicted),
      .cl_evicted_dirty_o (d_cl_ev_dirty),
      .cl_evicted_island_o(d_cl_ev_island),
      .cl_evicted_ix_o    (d_cl_ev_ix),
      .cl_evicted_iz_o    (d_cl_ev_iz),
      .cl_evicted_gen_o   (d_cl_ev_gen),

      .fin_valid_i(g_fin_valid),
      .fin_ready_o(g_fin_ready),
      .fin_slot_i (g_fin_slot),
      .fin_gen_i  (g_fin_gen),
      .fin_epoch_i(g_fin_epoch),
      .fin_ok_i   (g_fin_ok),
      .fin_crc_i  (g_fin_crc),

      .dm_valid_i(g_dm_valid),
      .dm_ready_o(g_dm_ready),
      .dm_slot_i (dm_slot[SLOTW-1:0]),
      .dm_gen_i  (dm_gen),
      .dm_epoch_i(dm_epoch),
      .dm_bd_i   (dm_bd),
      .dm_f_i    (dm_f),
      .dm_mips_i (dm_mips),

      .pin_valid_i(pin_valid),
      .pin_ready_o(pin_ready),
      .pin_slot_i (q_pin_slot),
      .pin_gen_i  (pin_gen),
      .pin_epoch_i(q_pin_epoch),

      .unpin_valid_i(g_unpin_valid),
      .unpin_ready_o(g_unpin_ready),
      .unpin_slot_i (g_unpin_slot),
      .unpin_gen_i  (g_unpin_gen),
      .unpin_epoch_i(g_unpin_epoch),

      .wb_valid_i(g_wb_valid),
      .wb_ready_o(g_wb_ready),
      .wb_slot_i (g_wb_slot),
      .wb_gen_i  (g_wb_gen),
      .wb_epoch_i(g_wb_epoch),

      .chk_valid_i(1'b0),
      .chk_slot_i ({SLOTW{1'b0}}),
      .chk_gen_i  ({GENW{1'b0}}),
      .chk_epoch_i(32'd0),
      .chk_valid_o(nc_chk_valid),
      .chk_stale_o(nc_chk_stale),

      .hits_o              (r_hits),
      .misses_o            (r_misses),
      .claims_o            (r_claims),
      .evictions_o         (r_evictions),
      .dirty_evictions_o   (r_dirty_evictions),
      .refused_all_pinned_o(r_refused_all_pinned),
      .stale_events_o      (r_stale_events),
      .crc_failures_o      (r_crc_failures),
      .resident_o          (r_resident)
  );

  assign g_dm_valid = dm_valid && mut_open;
  assign dm_ready   = g_dm_ready && mut_open;

  // The observed answers, widened for the C++ side.
  assign ra_lu_valid   = d_lu_ans_valid;
  assign ra_lu_hit     = d_lu_ans_hit;
  assign ra_lu_slot    = {{(16-SLOTW){1'b0}}, d_lu_ans_slot};
  assign ra_lu_gen     = d_lu_ans_gen;
  assign ra_cl_valid   = d_cl_ans_valid;
  assign ra_cl_same    = d_cl_ans_same;
  assign ra_cl_refused = d_cl_ans_refused;
  assign ra_cl_slot    = {{(16-SLOTW){1'b0}}, d_cl_ans_slot};
  assign ra_cl_gen     = d_cl_ans_gen;
  assign ra_cl_evicted = d_cl_evicted;
  assign ra_cl_ev_dirty  = d_cl_ev_dirty;
  assign ra_cl_ev_island = d_cl_ev_island;
  assign ra_cl_ev_ix     = d_cl_ev_ix;
  assign ra_cl_ev_iz     = d_cl_ev_iz;
  assign ra_cl_ev_gen    = d_cl_ev_gen;

  // =========================================================================
  // THE DROPPED-LOOKUP WITNESS
  // =========================================================================
  // A lookup the directory did not take produces nothing at all -- no answer,
  // no counter, no error bit, on either side. It is invisible from both blocks
  // and is therefore counted here, outside them.
  // The directory's answer is two clocks behind the offer -- one to capture the
  // bank address, one to decide -- so the window is a COUNTDOWN and not a
  // single-cycle expectation. A tripwire that assumed the answer arrived the
  // next clock would have reported every healthy lookup as a drop, which is the
  // "broken instrument" failure in its loud direction rather than its quiet one.
  logic [3:0] lu_wait_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      lu_wait_q    <= 4'd0;
      h_lu_offers  <= 32'd0;
      h_lu_dropped <= 32'd0;
    end else begin
      if (stat_clear) begin
        h_lu_offers  <= 32'd0;
        h_lu_dropped <= 32'd0;
      end
      if (shim_lu_valid) begin
        lu_wait_q   <= 4'd8;
        h_lu_offers <= (stat_clear ? 32'd0 : h_lu_offers) + 32'd1;
      end else if (lu_wait_q != 4'd0) begin
        if (d_lu_ans_valid) begin
          lu_wait_q <= 4'd0;
        end else if (lu_wait_q == 4'd1) begin
          lu_wait_q    <= 4'd0;
          h_lu_dropped <= (stat_clear ? 32'd0 : h_lu_dropped) + 32'd1;
        end else begin
          lu_wait_q <= lu_wait_q - 4'd1;
        end
      end
    end
  end

  // =========================================================================
  // TERRAIN.PAGELOADER -- the real loader
  // =========================================================================
  // TERRAIN.SEQ's slot is SLOTW = 10 wide; the pool clients carry an index ONE
  // BIT WIDER on purpose, so a computed slot of 1024 arrives as a refusal
  // rather than as slot 0 overwriting a live page. The zero extension is the
  // integration's, and it is written here rather than assumed.
  logic [MEMSLOT-1:0] pl_j_slot;
  assign pl_j_slot = {1'b0, q_ld_slot};

  // ---- the missing barrier, as a switchable harness gate ------------------
  // `wb_out_q` is high from the cycle TERRAIN.WRITEBACK accepts a job until it
  // reports the sheet done. While it is high and the knob is set, the load job
  // is not offered to the loader at all.
  logic       wb_out_q, ld_gate, pl_j_ready;
  assign ld_gate = !cfg_wb_barrier_i || !wb_out_q;
  assign ld_ready = pl_j_ready && ld_gate;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      wb_out_q         <= 1'b0;
      h_barrier_stalls <= 32'd0;
    end else begin
      if (stat_clear) h_barrier_stalls <= 32'd0;
      if (wb_valid && wb_ready)                    wb_out_q <= 1'b1;
      else if (wbdone_valid && wbdone_ready)       wb_out_q <= 1'b0;
      if (ld_valid && !ld_gate)
        h_barrier_stalls <= (stat_clear ? 32'd0 : h_barrier_stalls) + 32'd1;
    end
  end

  zhao_hps_burst_req_t pl_hps_req;
  logic                pl_hps_grant;
  zhao_hps_burst_rsp_t pl_hps_rsp;

  zhao_guard_req_t pl_guard_req;
  zhao_guard_rsp_t pl_guard_rsp;
  logic [63:0]     pl_wdata;
  logic            pl_wvalid, pl_wready, pl_wlast;

  logic [MEMSLOT-1:0] pl_fin_slot_w;
  logic signed [15:0] pl_fault_ix, pl_fault_iz;
  logic [31:0]        pl_fault_island, pl_fault_src_id, pl_fault_crc_seen, pl_fault_crc_expect;
  logic [3:0]         pl_fault_verdict;
  logic [31:0]        pl_fin_epoch, pl_fin_crc, pl_fin_src_id;

  zhao_terrain_pageloader #(
      .PAGE_BYTES(PAGE_BYTES),
      .REGION_BASE(POOL_BASE),
      .REGION_SLOTS(POOL_SLOTS),
      .GENW(GENW)
  ) u_pl (
      .clk  (clk),
      .rst_n(rst_n),

      .cfg_vram_client_i    (ZHAO_CLIENT_TERRAIN_BUILD),
      .cfg_hps_client_i     (ZHAO_CLIENT_TERRAIN_BUILD),
      .cfg_hps_arena_base_i (cfg_hps_arena_base_i),
      .cfg_hps_arena_bytes_i(cfg_hps_arena_bytes_i),
      .cfg_epoch_i          (cfg_epoch_i),

      .j_valid_i     (ld_valid && ld_gate),
      .j_ready_o     (pl_j_ready),
      .j_slot_i      (pl_j_slot),
      .j_gen_i       (ld_gen),
      .j_epoch_i     (q_ld_epoch),
      .j_island_i    (ld_island),
      .j_ix_i        (q_ld_ix),
      .j_iz_i        (q_ld_iz),
      .j_hps_addr_i  (ld_hps_addr),
      .j_expect_crc_i(ld_expect_crc),
      .j_src_id_i    (ld_src_id),

      .hps_req_o      (pl_hps_req),
      .hps_req_grant_i(pl_hps_grant),
      .hps_rsp_i      (pl_hps_rsp),

      .guard_req_o   (pl_guard_req),
      .guard_rsp_i   (pl_guard_rsp),
      .guard_wdata_o (pl_wdata),
      .guard_wvalid_o(pl_wvalid),
      .guard_wready_i(pl_wready),
      .guard_wlast_o (pl_wlast),

      .fin_valid_o  (pl_fin_valid),
      .fin_ready_i  (pl_fin_ready),
      .fin_slot_o   (pl_fin_slot_w),
      .fin_gen_o    (pl_fin_gen),
      .fin_epoch_o  (pl_fin_epoch),
      .fin_ok_o     (pl_fin_ok),
      .fin_crc_o    (pl_fin_crc),
      .fin_verdict_o(pl_fin_verdict),
      .fin_src_id_o (pl_fin_src_id),

      .fault_island_o    (pl_fault_island),
      .fault_ix_o        (pl_fault_ix),
      .fault_iz_o        (pl_fault_iz),
      .fault_src_id_o    (pl_fault_src_id),
      .fault_verdict_o   (pl_fault_verdict),
      .fault_crc_seen_o  (pl_fault_crc_seen),
      .fault_crc_expect_o(pl_fault_crc_expect),

      .pages_loaded_o   (pl_pages_loaded),
      .pages_faulted_o  (pl_pages_faulted),
      .pages_refused_o  (pl_pages_refused),
      .crc_fails_o      (pl_crc_fails),
      .hdr_ident_fails_o(pl_hdr_ident_fails),
      .incomplete_o     (pl_incomplete),
      .guard_denied_o   (pl_guard_denied),
      .bridge_errs_o    (pl_bridge_errs),
      .load_bytes_o     (pl_load_bytes)
  );

  assign pl_fin_slot = {{(16-MEMSLOT){1'b0}}, pl_fin_slot_w};

  // The completion, back into the directory. The extra pool bit is dropped
  // here and its being zero is the integration's obligation, checked below.
  // ---- the completion port, shared with the missing mip generator ---------
  // The loader has priority; the played mip completion follows behind it. See
  // `cfg_mipgen_fin_i` above for why the second completion has to exist at all
  // and why nothing in the tree can produce it.
  logic             mg_pend_q;
  logic [SLOTW-1:0] mg_slot_q;
  logic [GENW-1:0]  mg_gen_q;
  logic [31:0]      mg_epoch_q, mg_crc_q;

  assign g_fin_valid  = (pl_fin_valid || mg_pend_q) && mut_open;
  assign pl_fin_ready = pl_fin_valid && g_fin_ready && mut_open;
  assign g_fin_slot   = pl_fin_valid ? pl_fin_slot_w[SLOTW-1:0] : mg_slot_q;
  assign g_fin_gen    = pl_fin_valid ? pl_fin_gen   : mg_gen_q;
  assign g_fin_epoch  = pl_fin_valid ? pl_fin_epoch : mg_epoch_q;
  assign g_fin_ok     = pl_fin_valid ? pl_fin_ok    : 1'b1;
  assign g_fin_crc    = pl_fin_valid ? pl_fin_crc   : mg_crc_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      mg_pend_q     <= 1'b0;
      mg_slot_q     <= {SLOTW{1'b0}};
      mg_gen_q      <= {GENW{1'b0}};
      mg_epoch_q    <= 32'd0;
      mg_crc_q      <= 32'd0;
      h_mipgen_fins <= 32'd0;
    end else begin
      if (stat_clear) h_mipgen_fins <= 32'd0;
      if (pl_fin_valid && pl_fin_ready) begin
        mg_pend_q  <= cfg_mipgen_fin_i && pl_fin_ok;
        mg_slot_q  <= pl_fin_slot_w[SLOTW-1:0];
        mg_gen_q   <= pl_fin_gen;
        mg_epoch_q <= pl_fin_epoch;
        mg_crc_q   <= pl_fin_crc;
      end else if (mg_pend_q && g_fin_valid && g_fin_ready) begin
        mg_pend_q     <= 1'b0;
        h_mipgen_fins <= (stat_clear ? 32'd0 : h_mipgen_fins) + 32'd1;
      end
    end
  end

  // =========================================================================
  // TERRAIN.WRITEBACK -- the real evacuator
  // =========================================================================
  // TWO FIELDS TERRAIN.SEQ DOES NOT PRODUCE. The writeback job wants a journal
  // ADDRESS and a journal TICKET (`j_journal_addr_i`, `j_seq_i`), and
  // TERRAIN.SEQ's writeback port carries neither -- it emits
  // {slot, gen, epoch, island, ix, iz, src_id}. The two are minted here, in
  // glue, and that glue is a finding rather than a convenience: nothing in any
  // contract says who owns the journal arena allocator.
  logic [31:0] wb_ticket_q;
  logic [63:0] wb_jnl_addr_c;
  assign h_wb_ticket = wb_ticket_q;
  assign wb_jnl_addr_c = {32'd0, cfg_journal_base_i}
                       + 64'(wb_ticket_q[$clog2(JNL_ENTRIES)-1:0]) * 64'(F_BYTES);

  logic [MEMSLOT-1:0] wb_j_slot;
  assign wb_j_slot = {1'b0, q_wb_slot};

  zhao_guard_req_t wbg_req;
  zhao_guard_rsp_t wbg_rsp;
  logic            wbg_beat_valid, wbg_beat_last;
  logic [63:0]     wbg_beat_data;

  zhao_hps_burst_req_t wb_hps_req;
  logic                wb_hps_grant;
  zhao_hps_burst_rsp_t wb_hps_rsp;
  logic [63:0]         wb_hps_wdata;
  logic                wb_hps_wvalid, wb_hps_wready, wb_hps_wlast;

  logic        ack_valid, ack_ready, ack_ok;
  logic [31:0] ack_seq;

  logic [MEMSLOT-1:0] wbrel_slot_w, wbdone_slot_w;
  logic [31:0]        wbrel_epoch, wbdone_epoch, wbdone_src_id;
  logic [GENW-1:0]    wbdone_gen;
  logic signed [15:0] wb_fault_ix, wb_fault_iz;
  logic [31:0]        wb_fault_island, wb_fault_seq, wb_fault_src_id;
  logic [3:0]         wb_fault_verdict;
  logic [31:0]        wb_ack_wait_max, wb_jobs_stall_cycles;

  zhao_terrain_writeback #(
      .PAGE_BYTES(PAGE_BYTES),
      .F_OFF(F_OFF),
      .F_BYTES(F_BYTES),
      .REGION_BASE(POOL_BASE),
      .REGION_SLOTS(POOL_SLOTS),
      .GENW(GENW),
      .ACK_SLOTS(4),
      .ACK_DEADLINE_CYCLES(200000)
  ) u_wb (
      .clk  (clk),
      .rst_n(rst_n),

      .cfg_vram_client_i  (ZHAO_CLIENT_TERRAIN_BUILD),
      .cfg_hps_client_i   (ZHAO_CLIENT_TERRAIN_BUILD),
      .cfg_journal_base_i (cfg_journal_base_i),
      .cfg_journal_bytes_i(cfg_journal_bytes_i),
      .cfg_epoch_i        (cfg_epoch_i),

      .j_valid_i       (wb_valid),
      .j_ready_o       (wb_ready),
      .j_slot_i        (wb_j_slot),
      .j_gen_i         (wb_gen),
      .j_epoch_i       (q_wb_epoch),
      .j_island_i      (wb_island),
      .j_ix_i          (q_wb_ix),
      .j_iz_i          (q_wb_iz),
      .j_journal_addr_i(wb_jnl_addr_c),
      .j_seq_i         (wb_ticket_q),
      .j_src_id_i      (wb_src_id),

      .guard_req_o (wbg_req),
      .guard_rsp_i (wbg_rsp),
      .beat_valid_i(wbg_beat_valid),
      .beat_data_i (wbg_beat_data),
      .beat_last_i (wbg_beat_last),

      .hps_req_o      (wb_hps_req),
      .hps_req_grant_i(wb_hps_grant),
      .hps_rsp_i      (wb_hps_rsp),
      .hps_wdata_o    (wb_hps_wdata),
      .hps_wvalid_o   (wb_hps_wvalid),
      .hps_wready_i   (wb_hps_wready),
      .hps_wlast_o    (wb_hps_wlast),

      .ack_valid_i(ack_valid),
      .ack_ready_o(ack_ready),
      .ack_seq_i  (ack_seq),
      .ack_ok_i   (ack_ok),

      .wb_valid_o(wbrel_valid),
      .wb_ready_i(wbrel_ready),
      .wb_slot_o (wbrel_slot_w),
      .wb_gen_o  (wbrel_gen),
      .wb_epoch_o(wbrel_epoch),

      .done_valid_o  (wbdone_valid),
      .done_ready_i  (wbdone_ready),
      .done_slot_o   (wbdone_slot_w),
      .done_gen_o    (wbdone_gen),
      .done_epoch_o  (wbdone_epoch),
      .done_ok_o     (wbdone_ok),
      .done_verdict_o(wbdone_verdict),
      .done_seq_o    (wbdone_seq),
      .done_src_id_o (wbdone_src_id),

      .fault_island_o (wb_fault_island),
      .fault_ix_o     (wb_fault_ix),
      .fault_iz_o     (wb_fault_iz),
      .fault_seq_o    (wb_fault_seq),
      .fault_src_id_o (wb_fault_src_id),
      .fault_verdict_o(wb_fault_verdict),

      .sheets_written_o     (wb_sheets_written),
      .sheets_refused_o     (wb_sheets_refused),
      .sheets_faulted_o     (wb_sheets_faulted),
      .hdr_ident_fails_o    (wb_hdr_ident_fails),
      .guard_denied_o       (wb_guard_denied),
      .bridge_errs_o        (wb_bridge_errs),
      .acks_ok_o            (wb_acks_ok),
      .acks_nak_o           (wb_acks_nak),
      .acks_unmatched_o     (wb_acks_unmatched),
      .acks_after_epoch_o   (wb_acks_after_epoch),
      .acks_overdue_o       (wb_acks_overdue),
      .seq_conflicts_o      (wb_seq_conflicts),
      .wb_bytes_o           (wb_bytes),
      .outstanding_hwm_o    (wb_outstanding_hwm),
      .ack_wait_max_cycles_o(wb_ack_wait_max),
      .jobs_stall_cycles_o  (wb_jobs_stall_cycles)
  );

  assign wbrel_slot  = {{(16-MEMSLOT){1'b0}}, wbrel_slot_w};
  assign wbdone_slot = {{(16-MEMSLOT){1'b0}}, wbdone_slot_w};

  assign g_wb_valid  = wbrel_valid && mut_open;
  assign wbrel_ready = g_wb_ready && mut_open;
  assign g_wb_slot   = wbrel_slot_w[SLOTW-1:0];
  assign g_wb_gen    = wbrel_gen;
  assign g_wb_epoch  = wbrel_epoch;

  // The journal ticket advances on every ACCEPTED writeback job, so a job and
  // its ticket are minted by the same handshake.
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) wb_ticket_q <= 32'd1;
    else if (wb_valid && wb_ready) wb_ticket_q <= wb_ticket_q + 32'd1;
  end

  // =========================================================================
  // MEM.HPS.ARBITER -- the real two-client arbiter, first use by two terrain
  // clients. c0 = the loader's reads, c1 = the writeback's journal writes.
  // =========================================================================
  zhao_hps_burst_req_t br_req;
  logic                br_grant;
  logic                br_wvalid, br_wlast;
  logic [63:0]         br_wdata;
  zhao_hps_burst_rsp_t br_rsp;

  zhao_hps_arbiter u_arb (
      .clk  (clk),
      .rst_n(rst_n),

      .c0_req_i      (pl_hps_req),
      .c0_req_grant_o(pl_hps_grant),
      .c0_wr_valid_i (1'b0),
      .c0_wr_data_i  (64'd0),
      .c0_wr_last_i  (1'b0),
      .c0_rsp_o      (pl_hps_rsp),

      .c1_req_i      (wb_hps_req),
      .c1_req_grant_o(wb_hps_grant),
      .c1_wr_valid_i (wb_hps_wvalid),
      .c1_wr_data_i  (wb_hps_wdata),
      .c1_wr_last_i  (wb_hps_wlast),
      .c1_rsp_o      (wb_hps_rsp),

      .b_req_o     (br_req),
      .b_req_grant_i(br_grant),
      .b_wr_valid_o(br_wvalid),
      .b_wr_data_o (br_wdata),
      .b_wr_last_o (br_wlast),
      .b_rsp_i     (br_rsp),

      .c0_bursts_o     (arb_c0_bursts),
      .c1_bursts_o     (arb_c1_bursts),
      .c1_wait_cycles_o(arb_c1_wait_cycles)
  );

  // =========================================================================
  // THE PLAYED MEM.HPS.BRIDGE -- one port, address routed
  // =========================================================================
  // Reads come out of the staging arena, writes land in the journal. The
  // profile is MEM.HPS.BRIDGE.md's: a registered accept pulse, then the sim
  // latency to the first beat and one beat per cycle after it.
  logic        brb_busy, brb_write;
  logic [7:0]  brb_wait;
  logic [2:0]  brb_beat;
  logic [31:0] brb_base;
  logic [7:0]  brb_wrgap;

  logic [31:0] brd_byte, brd_word;
  logic [HW-1:0] brd_idx;
  assign brd_byte = brb_base + {26'd0, brb_beat, 3'd0};
  assign brd_word = (brd_byte - cfg_hps_arena_base_i) >> 3;
  assign brd_idx  = brd_word[HW-1:0];

  logic        jw_take;
  logic [31:0] jw_byte, jw_word;
  logic        jw_in;
  assign jw_byte = brb_base + {26'd0, brb_beat, 3'd0};
  assign jw_word = (jw_byte - cfg_journal_base_i) >> 3;
  assign jw_in   = (jw_byte >= cfg_journal_base_i) && (jw_word < 32'(JWORDS));

  // The write-acceptance LEVEL the bridge computes internally and does not
  // expose. TERRAIN.WRITEBACK's contract asks for it; it is played here so the
  // stall is explicit rather than a silent loss of the first beats.
  assign wb_hps_wready = brb_busy && brb_write && (brb_wait == 8'd0)
                       && (brb_wrgap >= cfg_wready_gap_i);
  assign jw_take       = br_wvalid && wb_hps_wready;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      brb_busy   <= 1'b0;
      brb_write  <= 1'b0;
      brb_wait   <= 8'd0;
      brb_beat   <= 3'd0;
      brb_base   <= 32'd0;
      brb_wrgap  <= 8'd0;
      br_grant   <= 1'b0;
      br_rsp     <= '0;
      h_jnl_writes <= 32'd0;
      h_jnl_oob    <= 32'd0;
    end else begin
      if (stat_clear) begin
        h_jnl_writes <= 32'd0;
        h_jnl_oob    <= 32'd0;
      end
      br_grant         <= 1'b0;
      br_rsp.beat_valid <= 1'b0;
      br_rsp.last       <= 1'b0;
      br_rsp.err        <= 1'b0;

      if (!brb_busy) begin
        if (br_req.valid) begin
          brb_busy  <= 1'b1;
          brb_write <= br_req.write;
          br_grant  <= 1'b1;
          brb_wait  <= cfg_req_latency_i;
          brb_beat  <= 3'd0;
          brb_base  <= br_req.addr;
          brb_wrgap <= 8'd0;
        end
      end else if (brb_wait != 8'd0) begin
        brb_wait <= brb_wait - 8'd1;
      end else if (!brb_write) begin
        br_rsp.beat_valid <= 1'b1;
        br_rsp.data       <= hps_mem[brd_idx];
        br_rsp.last       <= (brb_beat == 3'd7);
        brb_wait          <= cfg_beat_gap_i;
        if (brb_beat == 3'd7) brb_busy <= 1'b0;
        else                  brb_beat <= brb_beat + 3'd1;
      end

      // write pacing and retirement
      if (brb_busy && brb_write) begin
        if (jw_take) begin
          brb_wrgap <= 8'd0;
          h_jnl_writes <= (stat_clear ? 32'd0 : h_jnl_writes) + 32'd1;
          if (!jw_in) h_jnl_oob <= (stat_clear ? 32'd0 : h_jnl_oob) + 32'd1;
          if (brb_beat == 3'd7) begin
            brb_busy  <= 1'b0;
            brb_write <= 1'b0;
          end else begin
            brb_beat <= brb_beat + 3'd1;
          end
        end else if (brb_wrgap != 8'hFF) begin
          brb_wrgap <= brb_wrgap + 8'd1;
        end
      end
    end
  end

  // =========================================================================
  // THE PLAYED PAGE POOL: one guard port for the loader's writes, one for the
  // writeback's reads. Both transcribed from zhao_mem_guard.sv: `ready` is the
  // LEVEL, `ok`/`violation` are REGISTERED PULSES one cycle later.
  // =========================================================================
  // ---- write side (TERRAIN.PAGELOADER) ----
  logic       wg_fwd, wg_ok_q, wg_viol_q;
  logic [7:0] wg_hold, wg_gap;
  logic [ZHAO_VRAM_ADDR_BITS-1:0] wg_addr;
  logic [2:0] wg_beat;

  assign pl_guard_rsp.ready     = !wg_fwd;
  assign pl_guard_rsp.ok        = wg_ok_q;
  assign pl_guard_rsp.violation = wg_viol_q;
  assign pl_wready              = (wg_gap >= cfg_wready_gap_i);

  logic        pw_take;
  logic [31:0] pw_byte, pw_word;
  logic        pw_in;
  assign pw_take = pl_wvalid && pl_wready;
  assign pw_byte = {5'd0, wg_addr} + {26'd0, wg_beat, 3'd0};
  assign pw_word = (pw_byte - {5'd0, POOL_BASE}) >> 3;
  assign pw_in   = (pw_byte >= {5'd0, POOL_BASE}) && (pw_word < 32'(VWORDS));

  // ---- read side (TERRAIN.WRITEBACK) ----
  logic       rg_fwd, rg_ok_q, rg_viol_q;
  logic [7:0] rg_hold, rg_wait;
  logic [2:0] rg_beat;
  logic [31:0] rg_word;
  logic [VW-1:0] rg_idx;

  assign wbg_rsp.ready     = !rg_fwd;
  assign wbg_rsp.ok        = rg_ok_q;
  assign wbg_rsp.violation = rg_viol_q;
  assign rg_idx = rg_word[VW-1:0] + {{(VW-3){1'b0}}, rg_beat};

  logic rg_busy;

  // ONE DRIVING BLOCK PER MEMORY: the C++ preload and the captured write beats
  // both land in `vram_mem`, and splitting them would let a preload and a beat
  // write the same word in one cycle with no defined winner.
  always_ff @(posedge clk) begin
    if (mw_en && (mw_sel == 2'd1))   vram_mem[mw_addr[VW-1:0]] <= mw_data;
    else if (pw_take && pw_in)       vram_mem[pw_word[VW-1:0]] <= pl_wdata;
    if (mw_en && (mw_sel == 2'd0))   hps_mem[mw_addr[HW-1:0]]  <= mw_data;
    if (mw_en && (mw_sel == 2'd2))   jnl_mem[mw_addr[JW-1:0]]  <= mw_data;
    else if (jw_take && jw_in)       jnl_mem[jw_word[JW-1:0]]  <= br_wdata;
    case (mr_sel)
      2'd0:    mr_data <= hps_mem[mr_addr[HW-1:0]];
      2'd1:    mr_data <= vram_mem[mr_addr[VW-1:0]];
      default: mr_data <= jnl_mem[mr_addr[JW-1:0]];
    endcase
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      wg_fwd    <= 1'b0;
      wg_hold   <= 8'd0;
      wg_ok_q   <= 1'b0;
      wg_viol_q <= 1'b0;
      wg_addr   <= '0;
      wg_beat   <= 3'd0;
      wg_gap    <= 8'd0;
      h_pool_writes <= 32'd0;
      h_pool_oob    <= 32'd0;
    end else begin
      if (stat_clear) begin
        h_pool_writes <= 32'd0;
        h_pool_oob    <= 32'd0;
      end
      wg_ok_q   <= 1'b0;
      wg_viol_q <= 1'b0;

      if (wg_fwd) begin
        if (wg_hold != 8'd0) wg_hold <= wg_hold - 8'd1;
        else                 wg_fwd  <= 1'b0;
      end else if (pl_guard_req.valid) begin
        wg_ok_q <= 1'b1;
        wg_fwd  <= 1'b1;
        wg_hold <= cfg_grant_hold_i;
        wg_addr <= pl_guard_req.addr;
        wg_beat <= 3'd0;
      end

      if (pw_take) wg_gap <= 8'd0;
      else if (wg_gap != 8'hFF) wg_gap <= wg_gap + 8'd1;

      if (pw_take) begin
        h_pool_writes <= (stat_clear ? 32'd0 : h_pool_writes) + 32'd1;
        if (!pw_in) h_pool_oob <= (stat_clear ? 32'd0 : h_pool_oob) + 32'd1;
        wg_beat <= wg_beat + 3'd1;
      end
    end
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      rg_fwd    <= 1'b0;
      rg_hold   <= 8'd0;
      rg_ok_q   <= 1'b0;
      rg_viol_q <= 1'b0;
      rg_busy   <= 1'b0;
      rg_wait   <= 8'd0;
      rg_beat   <= 3'd0;
      rg_word   <= 32'd0;
      wbg_beat_valid <= 1'b0;
      wbg_beat_data  <= 64'd0;
      wbg_beat_last  <= 1'b0;
    end else begin
      rg_ok_q        <= 1'b0;
      rg_viol_q      <= 1'b0;
      wbg_beat_valid <= 1'b0;
      wbg_beat_last  <= 1'b0;

      if (rg_fwd) begin
        if (rg_hold != 8'd0) rg_hold <= rg_hold - 8'd1;
        else                 rg_fwd  <= 1'b0;
      end else if (wbg_req.valid) begin
        rg_ok_q  <= 1'b1;
        rg_fwd   <= 1'b1;
        rg_hold  <= cfg_grant_hold_i;
        rg_busy  <= 1'b1;
        rg_wait  <= cfg_rd_latency_i;
        rg_beat  <= 3'd0;
        rg_word  <= ({5'd0, wbg_req.addr} - {5'd0, POOL_BASE}) >> 3;
      end

      if (rg_busy) begin
        if (rg_wait != 8'd0) begin
          rg_wait <= rg_wait - 8'd1;
        end else begin
          wbg_beat_valid <= 1'b1;
          wbg_beat_data  <= vram_mem[rg_idx];
          wbg_beat_last  <= (rg_beat == 3'd7);
          rg_wait        <= cfg_rd_gap_i;
          if (rg_beat == 3'd7) rg_busy <= 1'b0;
          else                 rg_beat <= rg_beat + 3'd1;
        end
      end
    end
  end

  // =========================================================================
  // THE BARRIER WITNESS -- bytes, not job order
  // =========================================================================
  logic in_watch_wr, in_watch_rd;
  logic [31:0] wat_lo, wat_hi;
  assign wat_lo = {5'd0, POOL_BASE} + 32'(wat_slot_q) * 32'(PAGE_BYTES);
  assign wat_hi = wat_lo + 32'(PAGE_BYTES);
  assign in_watch_wr = (pw_byte >= wat_lo) && (pw_byte < wat_hi);
  assign in_watch_rd = ((({5'd0, POOL_BASE} + (rg_word << 3)) >= wat_lo)
                     && (({5'd0, POOL_BASE} + (rg_word << 3)) < wat_hi));

  logic [15:0] wat_slot_q;
  assign h_wat_slot = wat_slot_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      cyc          <= 32'd0;
      wat_wr_first <= 32'hFFFF_FFFF;
      wat_wr_count <= 32'd0;
      wat_rd_last  <= 32'd0;
      wat_rd_count <= 32'd0;
      wat_slot_q   <= 16'd0;
    end else begin
      cyc <= cyc + 32'd1;
      if (cfg_wat_auto_i && wb_valid && wb_ready) begin
        wat_slot_q   <= {{(16-SLOTW){1'b0}}, q_wb_slot};
        wat_wr_first <= 32'hFFFF_FFFF;
        wat_wr_count <= 32'd0;
        wat_rd_last  <= 32'd0;
        wat_rd_count <= 32'd0;
      end else if (wat_arm) begin
        wat_slot_q   <= wat_slot;
        wat_wr_first <= 32'hFFFF_FFFF;
        wat_wr_count <= 32'd0;
        wat_rd_last  <= 32'd0;
        wat_rd_count <= 32'd0;
      end else begin
        if (pw_take && in_watch_wr) begin
          if (wat_wr_first == 32'hFFFF_FFFF) wat_wr_first <= cyc;
          wat_wr_count <= wat_wr_count + 32'd1;
        end
        if (wbg_beat_valid && in_watch_rd) begin
          wat_rd_last  <= cyc;
          wat_rd_count <= wat_rd_count + 32'd1;
        end
      end
    end
  end

  // =========================================================================
  // THE ENGINE'S UNPIN, PLAYED -- TERRAIN.SEQ pins and never unpins, and no
  // engine exists to. A four-deep mirror of accepted pins, released after a
  // programmable delay; delay 0 means never, so T9's rule-5 refusal is
  // reachable.
  // =========================================================================
  localparam int unsigned UPD = 16;
  logic [SLOTW-1:0] up_slot [UPD];
  logic [GENW-1:0]  up_gen  [UPD];
  logic [31:0]      up_epoch[UPD];
  logic [31:0]      up_due  [UPD];
  logic [UPD-1:0]   up_busy;
  logic [3:0]       up_wr, up_rd;

  logic up_have;
  assign up_have = up_busy[up_rd] && (cyc >= up_due[up_rd]);
  assign g_unpin_valid = up_have && (cfg_unpin_delay_i != 16'd0) && mut_open;
  assign g_unpin_slot  = up_slot[up_rd];
  assign g_unpin_gen   = up_gen[up_rd];
  assign g_unpin_epoch = up_epoch[up_rd];

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      up_busy <= '0;
      up_wr   <= 4'd0;
      up_rd   <= 4'd0;
      h_unpins <= 32'd0;
      h_pins   <= 32'd0;
      h_pin_drops <= 32'd0;
      for (int unsigned i = 0; i < UPD; i++) begin
        up_slot[i] <= '0; up_gen[i] <= '0; up_epoch[i] <= '0; up_due[i] <= '0;
      end
    end else begin
      if (stat_clear) begin
        h_unpins    <= 32'd0;
        h_pins      <= 32'd0;
        h_pin_drops <= 32'd0;
      end
      if (pin_valid && pin_ready) begin
        h_pins <= (stat_clear ? 32'd0 : h_pins) + 32'd1;
        // A PIN THE MIRROR CANNOT HOLD IS COUNTED, NOT SWALLOWED. The first
        // version dropped it silently, the page stayed pinned forever, and the
        // dirty eviction the suite was built to reach became unreachable while
        // every check still passed. A harness that leaks quietly is a harness
        // that decides the result.
        if (!up_busy[up_wr]) begin
          up_slot[up_wr]  <= q_pin_slot;
          up_gen[up_wr]   <= pin_gen;
          up_epoch[up_wr] <= q_pin_epoch;
          up_due[up_wr]   <= cyc + {16'd0, cfg_unpin_delay_i};
          up_busy[up_wr]  <= 1'b1;
          up_wr           <= up_wr + 4'd1;
        end else begin
          h_pin_drops <= (stat_clear ? 32'd0 : h_pin_drops) + 32'd1;
        end
      end
      if (g_unpin_valid && g_unpin_ready) begin
        up_busy[up_rd] <= 1'b0;
        up_rd          <= up_rd + 4'd1;
        h_unpins       <= (stat_clear ? 32'd0 : h_unpins) + 32'd1;
      end
    end
  end

  // The tickets waiting to be acknowledged, in job-acceptance order.
  localparam int unsigned TKD = 8;
  logic [31:0] tk_q [TKD];
  logic [2:0]  tk_wr, tk_rd;
  logic [31:0] wb_ack_seq_q;
  assign wb_ack_seq_q = tk_q[tk_rd];

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      tk_wr <= 3'd0;
      tk_rd <= 3'd0;
      for (int unsigned i = 0; i < TKD; i++) tk_q[i] <= 32'd0;
    end else begin
      if (wb_valid && wb_ready) begin
        tk_q[tk_wr] <= wb_ticket_q;
        tk_wr       <= tk_wr + 3'd1;
      end
      if (jw_take && (brb_beat == 3'd7) && (jnl_burst_q == 32'(WR_BURSTS - 1)))
        tk_rd <= tk_rd + 3'd1;
    end
  end

  // =========================================================================
  // SW.STREAM'S JOURNAL DOORBELL, PLAYED
  // =========================================================================
  // A sheet is 128 journal bursts. When the last one retires the sheet's bytes
  // are in the journal, and the ticket that named it is echoed back after a
  // programmable delay. Tickets are acknowledged in the order the jobs were
  // accepted, which is the order TERRAIN.WRITEBACK processes them in.
  localparam int unsigned AKD = 8;
  logic [31:0] ak_seq [AKD];
  logic [AKD-1:0] ak_busy;
  logic [31:0] ak_due [AKD];
  logic [2:0] ak_wr, ak_rd;
  logic [31:0] jnl_burst_q;

  assign ack_valid = ak_busy[ak_rd] && (cyc >= ak_due[ak_rd]);
  assign ack_seq   = ak_seq[ak_rd];
  assign ack_ok    = cfg_ack_ok_i;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      ak_busy     <= '0;
      ak_wr       <= 3'd0;
      ak_rd       <= 3'd0;
      jnl_burst_q <= 32'd0;
      h_acks_sent <= 32'd0;
      for (int unsigned i = 0; i < AKD; i++) begin
        ak_seq[i] <= 32'd0; ak_due[i] <= 32'd0;
      end
    end else begin
      if (stat_clear) h_acks_sent <= 32'd0;
      if (jw_take && (brb_beat == 3'd7)) begin
        if (jnl_burst_q == 32'(WR_BURSTS - 1)) begin
          jnl_burst_q <= 32'd0;
          if (!ak_busy[ak_wr]) begin
            ak_seq[ak_wr]  <= wb_ack_seq_q;
            ak_due[ak_wr]  <= cyc + {16'd0, cfg_ack_delay_i};
            ak_busy[ak_wr] <= 1'b1;
            ak_wr          <= ak_wr + 3'd1;
          end
        end else begin
          jnl_burst_q <= jnl_burst_q + 32'd1;
        end
      end
      if (ack_valid && ack_ready) begin
        ak_busy[ak_rd] <= 1'b0;
        ak_rd          <= ak_rd + 3'd1;
        h_acks_sent    <= (stat_clear ? 32'd0 : h_acks_sent) + 32'd1;
      end
    end
  end

  // =========================================================================
  // THE REAL MEM.GUARD, OBSERVING BOTH TERRAIN CLIENTS
  // =========================================================================
  // It must PASS every loader write (TERRAIN.PAGE_POOL is write-only to
  // TERRAIN.BUILD) and REFUSE every writeback read (the read arm is reported
  // and not merged). Both directions are counted, in the composed setting.
  zhao_guard_req_t obs_wr_req, obs_rd_req;
  zhao_guard_rsp_t obs_wr_rsp, obs_rd_rsp;
  zhao_arb_req_t   obs_wr_areq, obs_rd_areq;
  zhao_arb_rsp_t   obs_arsp;
  logic            obs_wr_viol_pulse, obs_rd_viol_pulse;
  logic [31:0]     obs_wr_viol_tot, obs_rd_viol_tot;
  zhao_guard_req_t obs_wr_viol_req, obs_rd_viol_req;

  assign obs_arsp.grant   = 1'b1;
  assign obs_arsp.credits = 8'd32;

  // Presented only on the cycle the played guard actually takes the request,
  // so the observer sees the request stream the client issued rather than one
  // observation per stalled cycle.
  always_comb begin
    obs_wr_req       = pl_guard_req;
    obs_wr_req.valid = pl_guard_req.valid && pl_guard_rsp.ready;
    obs_rd_req       = wbg_req;
    obs_rd_req.valid = wbg_req.valid && wbg_rsp.ready;
  end

  zhao_mem_guard u_obs_wr (
      .clk(clk), .rst_n(rst_n),
      .req(obs_wr_req), .rsp(obs_wr_rsp),
      .map_valid(1'b0), .blit_slot(1'b0), .blit_span(32'd0), .fb_writer(1'b0),
      .arb_req(obs_wr_areq), .arb_rsp(obs_arsp),
      .guard_violation(obs_wr_viol_pulse),
      .guard_violations(obs_wr_viol_tot),
      .guard_violation_req(obs_wr_viol_req)
  );

  zhao_mem_guard u_obs_rd (
      .clk(clk), .rst_n(rst_n),
      .req(obs_rd_req), .rsp(obs_rd_rsp),
      .map_valid(1'b0), .blit_slot(1'b0), .blit_span(32'd0), .fb_writer(1'b0),
      .arb_req(obs_rd_areq), .arb_rsp(obs_arsp),
      .guard_violation(obs_rd_viol_pulse),
      .guard_violations(obs_rd_viol_tot),
      .guard_violation_req(obs_rd_viol_req)
  );

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      gobs_wr_ok   <= 32'd0;
      gobs_wr_viol <= 32'd0;
      gobs_rd_ok   <= 32'd0;
      gobs_rd_viol <= 32'd0;
    end else begin
      if (stat_clear) begin
        gobs_wr_ok <= 32'd0; gobs_wr_viol <= 32'd0;
        gobs_rd_ok <= 32'd0; gobs_rd_viol <= 32'd0;
      end
      if (obs_wr_rsp.ok)        gobs_wr_ok   <= (stat_clear ? 32'd0 : gobs_wr_ok) + 32'd1;
      if (obs_wr_viol_pulse)    gobs_wr_viol <= (stat_clear ? 32'd0 : gobs_wr_viol) + 32'd1;
      if (obs_rd_rsp.ok)        gobs_rd_ok   <= (stat_clear ? 32'd0 : gobs_rd_ok) + 32'd1;
      if (obs_rd_viol_pulse)    gobs_rd_viol <= (stat_clear ? 32'd0 : gobs_rd_viol) + 32'd1;
    end
  end

  /* verilator lint_off UNUSEDSIGNAL */
  logic unused;
  assign unused = (|obs_wr_areq) | (|obs_rd_areq) | (|obs_wr_viol_req) | (|obs_rd_viol_req)
                | (|obs_wr_viol_tot) | (|obs_rd_viol_tot)
                | obs_wr_rsp.ready | obs_wr_rsp.violation
                | obs_rd_rsp.ready | obs_rd_rsp.violation
                | (|pl_fault_island) | (|pl_fault_ix) | (|pl_fault_iz) | (|pl_fault_src_id)
                | (|pl_fault_verdict) | (|pl_fault_crc_seen) | (|pl_fault_crc_expect)
                | (|pl_fin_crc) | (|pl_fin_src_id)
                | (|wb_fault_island) | (|wb_fault_ix) | (|wb_fault_iz) | (|wb_fault_seq)
                | (|wb_fault_src_id) | (|wb_fault_verdict)
                | (|wb_ack_wait_max) | (|wb_jobs_stall_cycles)
                | (|wbdone_epoch) | (|wbdone_gen) | (|wbdone_src_id)
                | (|q_is_epoch) | (|dm_slot[15:SLOTW])
                | (|wat_slot[15:11]) | (|wat_slot_q[15:11])
                | (|brd_word[31:HW]) | (|jw_word[31:JW]) | (|pw_word[31:VW])
                | (|mw_addr[31:VW]) | (|mr_addr[31:VW])
                | (|rg_word[31:VW]) | (|pl_hps_req) | (|wb_hps_req) | (|br_req)
                | br_wlast | pl_wlast | wb_hps_wlast
                | nc_chk_valid | nc_chk_stale | (|lu_ep_q);
  /* verilator lint_on UNUSEDSIGNAL */

endmodule

`default_nettype wire
