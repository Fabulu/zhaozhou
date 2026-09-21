// zhao_terrain_bakerec.sv -- THE PATCH-BAKE RECORD PRODUCER: the third block
// between CMD.EXEC's ratified SurfaceStamp arm and TERRAIN.BAKE's `cmd_*`, and
// the sequencer that starts the three page agents a bake needs together.
//
// ===========================================================================
// WHY THIS EXISTS -- entry I32 commissioned it in as many words
// ===========================================================================
// `zhao_console_core.sv` entry I32 measured the intake field by field and
// closed with: "WHAT IS MISSING IS A THIRD BLOCK BETWEEN THEM".  Both halves
// it names are present and neither is this:
//
//   * THE COMMAND RECORD EXISTS -- `spec/commands.zidl` SurfaceStamp 0x0210,
//     `implemented`, carrying `handle32[patch] patch`, the transform whose
//     translation is the centre, `fx16 radius`, `u16 strength`.
//   * THE EXECUTOR ARM EXISTS -- `zhao_cmd_exec`'s EX_STAMP drains the stamp
//     ring and drives nine ports off a CRC-validated packet.
//
// What neither of them has is a PATCH-BAKE RECORD: one {patch, centre, radius,
// envelope, layer flags} per patch per frame, and the page identity -- slot,
// generation, epoch -- that TERRAIN.PAGEIO needs to find the page at all.
//
// ===========================================================================
// THE THREE FIELDS I32 CALLED ABSENT, AND WHAT EACH ONE IS NOW
// ===========================================================================
// I32's table ends with four ABSENT rows; `cmd_depth_sheet_i` was given a
// producer by the seam on 2026-09-21 and three were left.  Taken in order:
//
//   `cmd_cells_i`  -- "layer D present".  It is a statement about THE
//       COMPOSITION, and the composition now makes it true:
//       `zhao_terrain_pageio` reads layer D out of the page into its own
//       buffer before it serves a single vertex, and writes it back after.
//       `CELLS_PRESENT` is the parameter that says so, and it is a parameter
//       and not a literal because a console that composed bake WITHOUT pageio
//       would have to set it low and lose the breach phase, which is a real
//       arrangement someone may want to measure.
//
//   `cmd_dual_i`   -- "layer C present".  TERRAIN.PAGESTREAM's own record
//       flag, `kFlagDual` at bit `DUAL_BIT`, carried here from
//       TERRAIN.HDRREAD's forwarded job rather than re-derived.  It is the
//       SAME bit `u_terrain_patch` already reads off `tps_v_flags`.
//
//   `cmd_depth_from_i` / `cmd_depth_to_i` -- THE DISC ARM's depths, and these
//       are the interesting ones.  READ THE NEXT SECTION BEFORE CHANGING THEM.
//
// ===========================================================================
// THE DISC DEPTHS, AND WHY THEY ARE ZERO -- owner rulings R194, R221, R231
// ===========================================================================
// A record this block produces is a SHEET record.  `job_want_sheet_o` is high
// on every one of them, because a stamp's dig law is owner ruling R194's
// per-vertex layer-F read as R231 amended it to a DELTA, and the disc is not
// a second opinion about the same stamp -- it is a DIFFERENT PRODUCER's law.
// I32 says where that producer lives and it is not here: "the disc arm's real
// producer is a cast's progress -- the FIELD subsystem, and section 9.2's
// `(to - from) x stencil so an interrupted cast un-applies`."
//
// So the only cycle on which bake reads `cmd_depth_from_i`/`cmd_depth_to_i`
// from THIS block is owner ruling R221's `ST_MISS` fallback: the seam could
// not read the sheet, drops `bk_depth_sheet_o`, and bake runs the ratified
// parametric disc instead.  There are exactly three things that fallback could
// be given and two of them are wrong:
//
//   REJECTED -- `depth_to = stamp_depth(strength)`, the art table applied to
//     the command's own strength.  It is a ratified law and it is the wrong
//     one HERE: `scar_sum = h_scar + delta16` ACCUMULATES, so an ABSOLUTE
//     depth added to it digs the full crater a second time.  That is exactly
//     the defect owner ruling R231 repaired on 2026-09-21, and reintroducing
//     it on the fallback path -- where it would be rare, unlogged and
//     indistinguishable from terrain -- is worse than having it everywhere.
//
//   REJECTED -- a fabricated `from`/`to` pair chosen to "look right".  The ABI
//     has no depth field; anything here would be an art value invented inside
//     a composition packet, which is the fabrication I32's original refusal
//     was right about.
//
//   TAKEN -- ZERO, WITH A RETRY.  A fallback record digs NOTHING, is counted
//     at both ends (`fallbacks_o` here at the seam, `records_retried_o` here),
//     and THE RECORD IS RE-QUEUED.  This is not a dropped deformation: the
//     seam's before-plane is NOT consumed by a fallback -- `bf_live_q` is
//     cleared only `if (serve_q)` and a `seen` bit is retired only by a served
//     read -- so the pre-blend strengths survive and the next issue digs the
//     whole delta, once.  A miss is a page being evicted or refilled, which is
//     TRANSIENT by construction.
//
//     THAT MAKES R221's FALLBACK A DEFERRAL HERE RATHER THAN A SECOND CRATER,
//     and section 9.2's deferral law is what licenses it: item 3's identity --
//     "applying from->mid then mid->to == from->to" -- is EXACT under the delta
//     law (`tests/terrain/bake_delta_idempotence_directed.cpp` case 4), which
//     is precisely what entry I32's D-TERRCMD-C says the absolute law could
//     not give.
//
//     `FALLBACK_DEPTH_FROM` and `FALLBACK_DEPTH_TO` are PARAMETERS and not
//     literals, because "the fallback digs nothing" is a decision the owner
//     may reverse in one edit, and a decision that is not a knob is how a
//     wrong number becomes an unadjustable wrong number.
//
//     BOUNDED.  After `RETRIES` re-issues the record is dropped and
//     `records_dropped_o` fires.  A stamp whose sheet never becomes resident
//     is a lost scar and must be LOUD, not a queue that never drains.
//
// ===========================================================================
// COALESCING IS THE DELTA LAW's OWN CONSEQUENCE, not a convenience
// ===========================================================================
// Two stamps on one patch inside one frame produce ONE record, and the second
// stamp updates the first record's geometry in place.  Under R231 that is not
// an approximation: layer F holds the ACCUMULATED `after` and the seam's plane
// holds the FIRST pre-blend `before` for every texel either stamp touched, so
// one dig of `after - before` is the sum of both stamps exactly.  Under the
// ABSOLUTE law it would have been wrong, which is worth stating because the
// same code would have looked correct.
//
// It is also what keeps `BAKE_PATCH_BUDGET` meaningful: a budget counted in
// patches is only a budget if a patch costs one record.
//
// ===========================================================================
// THE SEQUENCE, AND THE ONE INTERLOCK THAT IS NOT OPTIONAL
// ===========================================================================
// A bake needs THREE agents live at once and they do not start together:
//
//   1. TERRAIN.PAGEIO      the job, then ~1,200 clocks of layer-D read and
//                          no-bake scatter before its face is live;
//   2. TERRAIN.SHEETSEAM   the job, then 1,089 prefetch reads through
//                          `zhao_surface_sheetshare` before it admits the
//                          record to bake at all;
//   3. TERRAIN.PAGESTREAM  the lattice pass that pushes layers A/B/C.
//
// PAGEIO FIRST, AND WAIT FOR `serving_o`.  This is the interlock: `nb_o` is
// combinational on a shadow plane that does not exist until `S_SERVE`, and
// nothing else on that block's face says so -- `idle_o` drops at the job
// ACCEPT.  A dig started early reads an empty no-bake plane, section 3.3's
// corner shadow silently vanishes, every handshake agrees and every counter
// agrees.  That is why `serving_o` was added to PAGEIO in the same commit as
// this file.
//
// THE SEAM SECOND, and its `job_ready_o` IS the prefetch's completion: the
// block holds it low while filling, so this FSM's own handshake is the wait.
// THE STREAM LAST, once the seam has admitted the record, because bake pulls
// vertices and stalling it costs nothing while starting it early cannot help.
//
// ===========================================================================
// WHAT THIS BLOCK MUST NOT OWN
// ===========================================================================
//   * THE HANDLE.  `job_handle_o` is `st_handle_i` CARRIED.  SURFACE.SHEET's
//     choice C4 -- "the handle is the identity the ABI carries; using anything
//     else re-derives identity that was already stated" -- and entry I32 names
//     synthesising it from `cmd_patch_id_i` as the cheap mistake to avoid.
//   * THE ENVELOPE.  `zhao_surface_dispatch` computes it by ruling R45's
//     world->patch law and `zhao_surface_stamp` is already fed from the same
//     wires; this block carries those four fx16 and computes nothing.
//   * RESIDENCY.  It never looks a patch up and never pins one.  It WATCHES
//     TERRAIN.HDRREAD's forwarded job -- a page the compose engine has already
//     claimed, pinned and read the header of -- and takes the identity from
//     there.  A bake therefore only ever runs against a page the frame was
//     going to touch anyway, and a slot that moved underneath it is caught by
//     PAGEIO's own `stale_gen_o` rather than by a check invented here.
//   * ARBITRATION.  The lattice pass goes through a SECOND `zhao_terrain_psmux`
//     instance in the composer, not through a mux written inline.
//
// ===========================================================================
// COUNTERS (spec/counters.md S4: saturate, never wrap)
// ===========================================================================
// Every one of these moves under legal stimulus and
// `tests/terrain/bakerec_rtl_directed.cpp` fires each one deliberately; none
// of them is a guard that only a mutant could reach.
// ===========================================================================
module zhao_terrain_bakerec #(
    // The residency handle, ONE BIT WIDER than the pool needs -- TERRAIN.
    // PAGELOADER's reason, carried so a computed slot of 1,024 cannot alias.
    parameter int unsigned SLOTW = 11,
    parameter int unsigned GENW  = 8,
    // Pending records. Two is not a guess: stamps coalesce by patch, and a
    // frame that stamps three DIFFERENT patches before either can bake is the
    // case `overflow_o` exists to make visible rather than to absorb.
    parameter int unsigned DEPTH = 2,
    // Owner ruling R221's fallback re-issues before the scar is declared lost.
    parameter int unsigned RETRIES = 3,
    // Frames a queued record may wait for its patch to be composed. A stamp on
    // a patch the camera never looks at would otherwise hold a queue entry for
    // the life of the machine.
    parameter int unsigned MAX_AGE = 8,
    // THE DISC DEPTHS ON AN R221 FALLBACK. See the header. Zero digs nothing
    // and the record is RE-QUEUED; they are knobs because the owner may rule
    // otherwise in one edit.
    parameter logic signed [31:0] FALLBACK_DEPTH_FROM = 32'sd0,
    parameter logic signed [31:0] FALLBACK_DEPTH_TO   = 32'sd0,
    // `kFlagDual` in TERRAIN.PAGESTREAM's T5 record flags.
    parameter int unsigned DUAL_BIT = 3,
    // Layer D is present because TERRAIN.PAGEIO serves it. See the header.
    parameter logic CELLS_PRESENT = 1'b1
) (
    input var logic clk,
    input var logic rst_n,

    // ---- section 9.2's frame window, for the age-out only ------------------
    input var logic frame_start_i,

    // -----------------------------------------------------------------------
    // THE STAMP, exactly as CMD.EXEC and SURFACE.DISPATCH present it
    // -----------------------------------------------------------------------
    // `st_fire_i` is `cmd_valid && cmd_ready` at `zhao_surface_stamp` -- the
    // same accept `u_surface_dispatch.cmd_fire_i` already reads, so the
    // envelope and the command can never be one cycle apart.
    input var logic               st_fire_i,
    input var logic               st_patch_valid_i,  // the dispatch ratified the pitch
    input var logic        [31:0] st_handle_i,       // handle32, the ABI's identity (C4)
    input var logic signed [15:0] st_patch_ix_i,
    input var logic signed [15:0] st_patch_iz_i,
    input var logic signed [31:0] st_cx_i,           // world fx16, the transform's translation
    input var logic signed [31:0] st_cz_i,
    input var logic signed [31:0] st_radius_i,
    input var logic signed [31:0] st_env_x0_i,
    input var logic signed [31:0] st_env_z0_i,
    input var logic signed [31:0] st_env_x1_i,
    input var logic signed [31:0] st_env_z1_i,
    input var logic        [15:0] st_src_id_i,

    // -----------------------------------------------------------------------
    // THE PAGE IDENTITY, watched off TERRAIN.HDRREAD's forwarded job
    // -----------------------------------------------------------------------
    // `pg_fire_i` is that block's `f_valid_o && f_ready_i`; `pg_ix_i`/`pg_iz_i`
    // are the header's OWN `h_patch_ix_o`/`h_patch_iz_o`, so the key this
    // matches against is the page's, not a recomputation of it.
    input var logic               pg_fire_i,
    input var logic signed [15:0] pg_ix_i,
    input var logic signed [15:0] pg_iz_i,
    input var logic [SLOTW-1:0]   pg_slot_i,
    input var logic [GENW-1:0]    pg_gen_i,
    input var logic [31:0]        pg_epoch_i,
    input var logic [15:0]        pg_flags_i,

    // -----------------------------------------------------------------------
    // TERRAIN.PAGEIO -- the job, the live-face interlock and the completion
    // -----------------------------------------------------------------------
    output var logic             io_valid_o,
    input  var logic             io_ready_i,
    output var logic [SLOTW-1:0] io_slot_o,
    output var logic [GENW-1:0]  io_gen_o,
    output var logic [31:0]      io_epoch_o,
    output var logic [31:0]      io_src_id_o,
    input  var logic             io_serving_i,      // <- pageio.serving_o
    input  var logic             io_done_valid_i,
    output var logic             io_done_ready_o,

    // -----------------------------------------------------------------------
    // TERRAIN.SHEETSEAM -- the job face
    // -----------------------------------------------------------------------
    output var logic        job_valid_o,
    input  var logic        job_ready_i,
    output var logic [31:0] job_handle_o,
    output var logic        job_want_sheet_o,
    output var logic [15:0] job_src_id_o,
    // Valid with the seam's `bk_valid_o`; sampled on bake's accept.
    input  var logic        bk_valid_i,
    input  var logic        bk_ready_i,
    input  var logic        bk_fallback_i,

    // -----------------------------------------------------------------------
    // THE RECORD FIELDS TERRAIN.BAKE READS
    // -----------------------------------------------------------------------
    // Held stable from the seam's job offer until the record retires, which is
    // what the seam's ready/valid contract requires of its producer.
    output var logic        [15:0] cmd_patch_id_o,
    output var logic signed [31:0] cmd_cx_o,
    output var logic signed [31:0] cmd_cz_o,
    output var logic signed [31:0] cmd_radius_o,
    output var logic signed [31:0] cmd_depth_from_o,
    output var logic signed [31:0] cmd_depth_to_o,
    output var logic signed [31:0] cmd_env_x0_o,
    output var logic signed [31:0] cmd_env_z0_o,
    output var logic signed [31:0] cmd_env_x1_o,
    output var logic signed [31:0] cmd_env_z1_o,
    output var logic               cmd_dual_o,
    output var logic               cmd_cells_o,
    output var logic        [15:0] cmd_src_id_o,

    // -----------------------------------------------------------------------
    // TERRAIN.PAGESTREAM's lattice pass, through a psmux client
    // -----------------------------------------------------------------------
    output var logic             ps_j_valid_o,
    input  var logic             ps_j_ready_i,
    output var logic [SLOTW-1:0] ps_j_slot_o,
    output var logic [GENW-1:0]  ps_j_gen_o,
    output var logic [31:0]      ps_j_epoch_o,
    output var logic [31:0]      ps_j_src_id_o,
    output var logic [15:0]      ps_j_flags_o,
    input  var logic             ps_done_valid_i,
    output var logic             ps_done_ready_o,

    input var logic bake_done_i,   // <- bake.bake_done_o

    // -----------------------------------------------------------------------
    // evidence
    // -----------------------------------------------------------------------
    output var logic [31:0] stamps_seen_o,      // accepts offered to this block
    output var logic [31:0] records_queued_o,   // ... that opened a new record
    output var logic [31:0] coalesced_o,        // ... that folded into one
    output var logic [31:0] overflow_o,         // ... the queue could not hold
    output var logic [31:0] unplaced_o,         // ... the dispatch refused the pitch
    output var logic [31:0] records_issued_o,   // records that reached bake
    output var logic [31:0] records_retired_o,  // ... and dug on the sheet law
    output var logic [31:0] records_retried_o,  // ... R221 fallback: re-queued
    output var logic [31:0] records_dropped_o,  // ... RETRIES spent: a LOST scar
    output var logic [31:0] aged_out_o,         // never composed within MAX_AGE
    output var logic [31:0] stray_done_o,       // a completion with none in flight
    output var logic        idle_o
);

  // ==========================================================================
  // ELABORATION -- inside `initial`, which is what Quartus 17.0 accepts
  // (CLAUDE.md: a bare module-scope `if ... $fatal` lints clean and fails
  // `quartus_map` with "syntax error near text: if").
  // ==========================================================================
  // synthesis translate_off
  initial begin
    if (DEPTH < 1)
      $fatal(1, "zhao_terrain_bakerec: DEPTH must be >= 1");
    if (RETRIES < 1)
      // A record that cannot be re-issued turns every R221 fallback into a
      // lost scar, which is the one outcome this block exists to prevent.
      $fatal(1, "zhao_terrain_bakerec: RETRIES must be >= 1");
    if (MAX_AGE < 1)
      $fatal(1, "zhao_terrain_bakerec: MAX_AGE must be >= 1");
    if (DUAL_BIT > 15)
      $fatal(1, "zhao_terrain_bakerec: DUAL_BIT must index the 16-bit flags word");
  end
  // synthesis translate_on

  localparam int unsigned IdxW  = (DEPTH > 1) ? $clog2(DEPTH) : 1;
  localparam int unsigned TryW  = $clog2(RETRIES + 1);
  localparam int unsigned AgeW  = $clog2(MAX_AGE + 1);

  function automatic logic [31:0] sat_inc(input logic [31:0] a);
    sat_inc = (a == 32'hFFFF_FFFF) ? a : a + 32'd1;
  endfunction

  // ==========================================================================
  // THE PENDING RECORDS
  // ==========================================================================
  logic                e_v_q       [DEPTH];
  logic                e_page_q    [DEPTH];
  logic        [31:0]  e_handle_q  [DEPTH];
  logic signed [15:0]  e_ix_q      [DEPTH];
  logic signed [15:0]  e_iz_q      [DEPTH];
  logic signed [31:0]  e_cx_q      [DEPTH];
  logic signed [31:0]  e_cz_q      [DEPTH];
  logic signed [31:0]  e_rad_q     [DEPTH];
  logic signed [31:0]  e_ex0_q     [DEPTH];
  logic signed [31:0]  e_ez0_q     [DEPTH];
  logic signed [31:0]  e_ex1_q     [DEPTH];
  logic signed [31:0]  e_ez1_q     [DEPTH];
  logic        [15:0]  e_src_q     [DEPTH];
  logic [SLOTW-1:0]    e_slot_q    [DEPTH];
  logic [GENW-1:0]     e_gen_q     [DEPTH];
  logic        [31:0]  e_epoch_q   [DEPTH];
  logic        [15:0]  e_flags_q   [DEPTH];
  logic [TryW-1:0]     e_try_q     [DEPTH];
  logic [AgeW-1:0]     e_age_q     [DEPTH];

  // ==========================================================================
  // THE SEQUENCER
  // ==========================================================================
  localparam logic [2:0] S_IDLE   = 3'd0;
  localparam logic [2:0] S_IO     = 3'd1;  // offer PAGEIO its job
  localparam logic [2:0] S_SERVE  = 3'd2;  // wait for PAGEIO's face to be live
  localparam logic [2:0] S_SEAM   = 3'd3;  // offer the seam its job (it prefetches)
  localparam logic [2:0] S_STREAM = 3'd4;  // start the lattice pass
  localparam logic [2:0] S_RUN    = 3'd5;  // bake digs; collect the completions
  localparam logic [2:0] S_RETIRE = 3'd6;  // one cycle: keep or drop the record

  logic [2:0]      state_q;
  logic [IdxW-1:0] sel_q;
  logic            busy_c;
  assign busy_c = (state_q != S_IDLE);

  // The fallback verdict, LATCHED at bake's accept. `bk_fallback_i` is valid
  // only with `bk_valid_i`, and by the time `bake_done_i` arrives the seam has
  // already retired the record and dropped both.
  logic fb_q;
  logic ps_done_q, io_done_q, bake_done_q;

  // ---- selection: the first record whose page identity is known ------------
  logic            pick_v_c;
  logic [IdxW-1:0] pick_c;
  always_comb begin
    pick_v_c = 1'b0;
    pick_c   = '0;
    for (int unsigned k = 0; k < DEPTH; k++) begin
      if (!pick_v_c && e_v_q[k] && e_page_q[k]) begin
        pick_v_c = 1'b1;
        pick_c   = IdxW'(k);
      end
    end
  end

  // ---- intake: coalesce by patch key, else allocate ------------------------
  // The entry UNDER SERVICE is excluded from coalescing: its fields are being
  // held stable for the seam under the ready/valid contract, and a stamp that
  // rewrote them mid-prefetch would give bake a record the seam never saw.
  // Such a stamp opens a SECOND record instead, which the before-plane makes
  // correct -- the first dig retires only the `seen` bits it read.
  logic            co_v_c;
  logic [IdxW-1:0] co_c;
  logic            fr_v_c;
  logic [IdxW-1:0] fr_c;
  always_comb begin
    co_v_c = 1'b0;
    co_c   = '0;
    fr_v_c = 1'b0;
    fr_c   = '0;
    for (int unsigned k = 0; k < DEPTH; k++) begin
      if (!co_v_c && e_v_q[k] && (e_ix_q[k] == st_patch_ix_i)
          && (e_iz_q[k] == st_patch_iz_i)
          && !(busy_c && (sel_q == IdxW'(k)))) begin
        co_v_c = 1'b1;
        co_c   = IdxW'(k);
      end
      if (!fr_v_c && !e_v_q[k]) begin
        fr_v_c = 1'b1;
        fr_c   = IdxW'(k);
      end
    end
  end

  wire take_c  = st_fire_i && st_patch_valid_i;
  wire wr_v_c  = take_c && (co_v_c || fr_v_c);
  wire [IdxW-1:0] wr_ix_c = co_v_c ? co_c : fr_c;

  // ==========================================================================
  // THE OUTPUT FACES -- every field read out of the SELECTED entry
  // ==========================================================================
  assign io_valid_o  = (state_q == S_IO);
  assign io_slot_o   = e_slot_q[sel_q];
  assign io_gen_o    = e_gen_q[sel_q];
  assign io_epoch_o  = e_epoch_q[sel_q];
  assign io_src_id_o = {16'd0, e_src_q[sel_q]};

  assign job_valid_o      = (state_q == S_SEAM);
  assign job_handle_o     = e_handle_q[sel_q];
  assign job_want_sheet_o = 1'b1;   // see THE DISC DEPTHS in the header
  assign job_src_id_o     = e_src_q[sel_q];

  assign cmd_patch_id_o   = {e_ix_q[sel_q][7:0], e_iz_q[sel_q][7:0]};
  assign cmd_cx_o         = e_cx_q[sel_q];
  assign cmd_cz_o         = e_cz_q[sel_q];
  assign cmd_radius_o     = e_rad_q[sel_q];
  assign cmd_depth_from_o = FALLBACK_DEPTH_FROM;
  assign cmd_depth_to_o   = FALLBACK_DEPTH_TO;
  assign cmd_env_x0_o     = e_ex0_q[sel_q];
  assign cmd_env_z0_o     = e_ez0_q[sel_q];
  assign cmd_env_x1_o     = e_ex1_q[sel_q];
  assign cmd_env_z1_o     = e_ez1_q[sel_q];
  assign cmd_dual_o       = e_flags_q[sel_q][DUAL_BIT];
  assign cmd_cells_o      = CELLS_PRESENT;
  assign cmd_src_id_o     = e_src_q[sel_q];

  assign ps_j_valid_o  = (state_q == S_STREAM);
  assign ps_j_slot_o   = e_slot_q[sel_q];
  assign ps_j_gen_o    = e_gen_q[sel_q];
  assign ps_j_epoch_o  = e_epoch_q[sel_q];
  assign ps_j_src_id_o = {16'd0, e_src_q[sel_q]};
  assign ps_j_flags_o  = e_flags_q[sel_q];

  // The completions are collected from the moment the pass is started, because
  // TERRAIN.PAGESTREAM retires its job when the LATTICE ends -- which is the
  // end of bake's DIG phase, before the BREACH phase and therefore before
  // `bake_done_i`. A ready raised only in a later state would deadlock the
  // share against a block that is waiting to be taken.
  wire collect_c = (state_q == S_STREAM) || (state_q == S_RUN);
  assign ps_done_ready_o = collect_c && !ps_done_q;
  assign io_done_ready_o = collect_c && !io_done_q;

  assign idle_o = (state_q == S_IDLE);

  // ==========================================================================
  // THE ONE PROCESS
  // ==========================================================================
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state_q           <= S_IDLE;
      sel_q             <= '0;
      fb_q              <= 1'b0;
      ps_done_q         <= 1'b0;
      io_done_q         <= 1'b0;
      bake_done_q       <= 1'b0;
      stamps_seen_o     <= 32'd0;
      records_queued_o  <= 32'd0;
      coalesced_o       <= 32'd0;
      overflow_o        <= 32'd0;
      unplaced_o        <= 32'd0;
      records_issued_o  <= 32'd0;
      records_retired_o <= 32'd0;
      records_retried_o <= 32'd0;
      records_dropped_o <= 32'd0;
      aged_out_o        <= 32'd0;
      stray_done_o      <= 32'd0;
      for (int unsigned k = 0; k < DEPTH; k++) begin
        e_v_q[k]    <= 1'b0;
        e_page_q[k] <= 1'b0;
        e_try_q[k]  <= '0;
        e_age_q[k]  <= '0;
      end
    end else begin
      // ---- 1. INTAKE -----------------------------------------------------
      if (st_fire_i) stamps_seen_o <= sat_inc(stamps_seen_o);
      if (st_fire_i && !st_patch_valid_i) unplaced_o <= sat_inc(unplaced_o);
      if (take_c && !co_v_c && !fr_v_c) overflow_o <= sat_inc(overflow_o);
      if (wr_v_c) begin
        e_v_q     [wr_ix_c] <= 1'b1;
        e_handle_q[wr_ix_c] <= st_handle_i;
        e_ix_q    [wr_ix_c] <= st_patch_ix_i;
        e_iz_q    [wr_ix_c] <= st_patch_iz_i;
        e_cx_q    [wr_ix_c] <= st_cx_i;
        e_cz_q    [wr_ix_c] <= st_cz_i;
        e_rad_q   [wr_ix_c] <= st_radius_i;
        e_ex0_q   [wr_ix_c] <= st_env_x0_i;
        e_ez0_q   [wr_ix_c] <= st_env_z0_i;
        e_ex1_q   [wr_ix_c] <= st_env_x1_i;
        e_ez1_q   [wr_ix_c] <= st_env_z1_i;
        e_src_q   [wr_ix_c] <= st_src_id_i;
        e_age_q   [wr_ix_c] <= '0;
        if (co_v_c) begin
          coalesced_o <= sat_inc(coalesced_o);
        end else begin
          e_page_q[wr_ix_c] <= 1'b0;
          e_try_q [wr_ix_c] <= '0;
          records_queued_o  <= sat_inc(records_queued_o);
        end
      end

      // ---- 2. THE PAGE IDENTITY ------------------------------------------
      // Disjoint fields from the intake above, so both may land in one cycle.
      // The entry under service is excluded for the intake's reason.
      if (pg_fire_i) begin
        for (int unsigned k = 0; k < DEPTH; k++) begin
          if (e_v_q[k] && (e_ix_q[k] == pg_ix_i) && (e_iz_q[k] == pg_iz_i)
              && !(busy_c && (sel_q == IdxW'(k)))) begin
            e_page_q [k] <= 1'b1;
            e_slot_q [k] <= pg_slot_i;
            e_gen_q  [k] <= pg_gen_i;
            e_epoch_q[k] <= pg_epoch_i;
            e_flags_q[k] <= pg_flags_i;
          end
        end
      end

      // ---- 3. THE AGE-OUT ------------------------------------------------
      if (frame_start_i) begin
        for (int unsigned k = 0; k < DEPTH; k++) begin
          if (e_v_q[k] && !(busy_c && (sel_q == IdxW'(k)))) begin
            if (e_age_q[k] == AgeW'(MAX_AGE)) begin
              e_v_q[k]   <= 1'b0;
              aged_out_o <= sat_inc(aged_out_o);
            end else begin
              e_age_q[k] <= e_age_q[k] + AgeW'(1);
            end
          end
        end
      end

      // ---- 4. THE COMPLETION COLLECTORS ----------------------------------
      if (collect_c && ps_done_valid_i && !ps_done_q) ps_done_q <= 1'b1;
      if (collect_c && io_done_valid_i && !io_done_q) io_done_q <= 1'b1;
      if ((state_q == S_RUN) && bake_done_i) bake_done_q <= 1'b1;
      // A completion with nothing in flight. It CAN happen -- a page pass this
      // block did not start cannot reach here because the share demuxes, but a
      // `bake_done_i` after a retirement can, and a counter is better evidence
      // than a silent edge.
      if (!collect_c && (state_q != S_RUN) && bake_done_i)
        stray_done_o <= sat_inc(stray_done_o);

      // ---- 5. THE SEQUENCER ----------------------------------------------
      case (state_q)
        S_IDLE: begin
          fb_q        <= 1'b0;
          ps_done_q   <= 1'b0;
          io_done_q   <= 1'b0;
          bake_done_q <= 1'b0;
          if (pick_v_c) begin
            sel_q   <= pick_c;
            state_q <= S_IO;
          end
        end

        S_IO: begin
          if (io_ready_i) state_q <= S_SERVE;
        end

        S_SERVE: begin
          // PAGEIO's layer-D read and no-bake scatter. See the interlock note.
          if (io_serving_i) state_q <= S_SEAM;
        end

        S_SEAM: begin
          // The seam holds `job_ready_o` low for the whole 1,089-texel
          // prefetch, so this handshake IS the prefetch's completion.
          if (job_ready_i) begin
            state_q          <= S_STREAM;
            records_issued_o <= sat_inc(records_issued_o);
          end
        end

        S_STREAM: begin
          if (ps_j_ready_i) state_q <= S_RUN;
        end

        S_RUN: begin
          if (bk_valid_i && bk_ready_i) fb_q <= bk_fallback_i;
          if ((bake_done_q || bake_done_i)
              && (ps_done_q || (ps_done_valid_i && !ps_done_q))
              && (io_done_q || (io_done_valid_i && !io_done_q)))
            state_q <= S_RETIRE;
        end

        S_RETIRE: begin
          if (fb_q && (e_try_q[sel_q] != TryW'(RETRIES))) begin
            // R221's fallback is a DEFERRAL. The seam did not consume its
            // before-plane, so the delta survives to the next issue.
            e_try_q[sel_q]    <= e_try_q[sel_q] + TryW'(1);
            e_age_q[sel_q]    <= '0;
            records_retried_o <= sat_inc(records_retried_o);
          end else begin
            e_v_q[sel_q] <= 1'b0;
            if (fb_q) records_dropped_o <= sat_inc(records_dropped_o);
            else      records_retired_o <= sat_inc(records_retired_o);
          end
          state_q <= S_IDLE;
        end

        default: state_q <= S_IDLE;
      endcase
    end
  end

endmodule
