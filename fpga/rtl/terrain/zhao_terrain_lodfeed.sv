// zhao_terrain_lodfeed.sv -- THE PAGE-LOAD DEVIATION PASS.  Owner rulings
// R24 (when) and R8/R22 (which reading).
//
// ===========================================================================
// WHAT IT IS FOR
// ===========================================================================
// `zhao_terrain_loddev` computes TERRAIN.LOD's three per-subpatch deviations
// and takes its lattice through a RANDOM-ACCESS port -- request (vi, vj) this
// cycle, the height next -- because that is TERRAIN.COMPCACHE's read shape.
// Ruling R24 says the deviations are computed AT PAGE LOAD, which is a
// different place in the chain: at page load the lattice is a STREAM, not a
// random-access cache.
//
// This block is the twenty-odd lines between the two: one lattice of the mip
// pass's fine stream, buffered, then walked by `zhao_terrain_loddev`, with the
// sixteen records handed to `zhao_terrain_devstore` keyed by the PAGE SLOT.
//
// It is a separate file and not wires in a composer for the reason
// `zhao_terrain_mipfeed` gives about itself: the buffer below is STATE, and
// the surface selection is a DECISION.
//
// ===========================================================================
// IT TAPS THE MIP PASS, AND THAT IS THE WHOLE ARCHITECTURAL IDEA
// ===========================================================================
// TERRAIN.MIPFEED already streams a freshly loaded page's lattice past
// TERRAIN.MIPGEN, once per surface, at page load, with no extra page traffic
// and no compose-cache port contention -- exactly the event and exactly the
// data ruling R24 asks for.  So this block OBSERVES `mg_fine_*` rather than
// asking for a stream of its own.  Three consequences, all deliberate:
//
//   * THE SURFACE DECISION IS NOT REMADE HERE.  Whatever plane
//     TERRAIN.MIPFEED calls surface 0, this block calls surface 0.  MIPFEED's
//     header records that choice as an OPEN OWNER RULING (layer A today, the
//     compose lane when TERRAIN.PATCH's output can be routed there) and one
//     open ruling with two implementations is how two blocks come to disagree.
//     When MIPFEED is rewired, the deviations follow with no edit here.
//
//   * IT NEVER BACK-PRESSURES.  `f_valid_i` is `mg_fine_valid && mg_fine_ready`
//     -- an observation of a handshake that has already happened.  Inserting a
//     ready here would stall the mip pass, which stalls TERRAIN.PAGESTREAM,
//     which holds a MEM.GUARD burst open.  If a new lattice begins while the
//     walk is still running the lattice is DROPPED and counted on
//     `lattices_dropped_o`; the slot then reads DEV_MAX out of the store
//     (full detail) and `read_unwritten_o` fires beside it.  A dropped
//     deviation costs triangles; a stalled paging spine costs the frame.
//
//   * ONLY SURFACE 0 IS BUFFERED, and that is ruling R59's smaller form made
//     concrete.  `zhao_terrain_lod` law 7 -- "THE UNDERSIDE TAKES THE TOP'S
//     LEVEL" -- emits the underside job from the top's descriptor and its
//     `sp_*` port has no surface field, so the underside's records can never
//     be read.  MIPGEN's surface counter runs across both passes, so samples
//     0..1,088 are surface 0 and 1,089..2,177 are surface 1; the second
//     lattice is counted on `surface1_samples_o` and dropped.  Storing it
//     would double a 116 M10K store to produce records nothing reads.
//
// ===========================================================================
// THE BUFFER
// ===========================================================================
// 1,089 x 16 bits of height16, addressed by the sample ordinal, which IS
// `vj * 33 + vi` because TERRAIN.PAGESTREAM emits the lattice in that order
// (`v_vi_o` stride 1, `v_vj_o` stride EDGE) and TERRAIN.MIPGEN's decimation
// already depends on it.  `vj * 33` is `(vj << 5) + vj`: no divider, no DSP.
//
// 17,424 bits.  On Cyclone V that is ~4 M10K (a 1,089-deep 16-bit array does
// not pack tightly; the theoretical floor is 2).  TERRAIN.MIPFEED refused
// exactly this buffer -- "17,424 bits, which is the buffering
// TERRAIN.PAGESTREAM was arranged specifically to avoid" -- and was right for
// its own job, which was to hand MIPGEN one plane at a time and could stream
// the page twice instead.  This job cannot: `zhao_terrain_loddev` reads the
// lattice out of order, 16 subpatches x 3 levels deep, and re-streaming the
// page 3,888 times is not an alternative.
//
// HEIGHT16 -> FX16 IS `raw << 8`, EXACT (spec/qformats.md 9, and
// `zhao_terrain_patch.sv` 297 says the same).  No rounding, no saturation.
//
// ===========================================================================
// WHAT IT COSTS, MEASURED RATHER THAN ASSERTED
// ===========================================================================
// MEASURED, not estimated.  `tests/terrain/terrain_lodpath_directed.cpp`
// reports the walk for the page it streams: 2,316 vertices measured, 6,948
// lattice reads (exactly three per vertex) and 9,766 clocks for all sixteen
// subpatches at DEV_INCLUDE_BOUNDARY = 1.  `terrain_loddev_directed` reports
// 10,860 clocks per surface walk over its own fourteen lattices.  Call it
// ~11k clocks per page.
//
// A page load is ~6,726 clocks and the two mip passes ~7,088, so this adds
// about 80% to the wall time of a page load-to-resident path -- to ~24,600
// clocks.  Against `spec/terrain_rules.md` 7 s 32 patches/frame sustained
// streaming that is ~790k of the frame s 1.67M gpu clocks, and it is off the
// critical path because nothing waits on a load.  The directed test PRINTS
// the number every run rather than pinning it, because a pinned clock count
// goes stale silently.
//
// ===========================================================================
// THE RESIDENCY HANDLE, AND WHY THIS BLOCK CHECKS IT -- core entry I27
// ===========================================================================
// ADDED 2026-09-22.  `zhao_console_core.sv` entry I27 has said since 2026-09-20
// that the directory's handle check (`terr_chk_*`) has no honest caller until
// `zhao_terrain_devstore` composes, and that "whoever composes the store owes
// this check in the same commit".  The store composed on 2026-09-21 with the
// subpatch decision chain and the check did not come with it.  This block is
// the caller the entry named, and it is the caller because it is the only
// thing in the console that holds a page's residency slot across time while
// something else writes that slot's records.
//
// WHAT IT DOES, in one sentence: it remembers the {slot, generation, epoch}
// the walk was started with, asks the directory at the moment the store
// commits the sixteenth record whether that handle is still the page it was,
// and INVALIDATES the slot's records if it is not.
//
// WHAT IT DOES NOT DO: stall.  The answer arrives after the records are
// already filed, so the check can only withdraw them.  An unanswered check
// costs an invalidation, never a beat of the paging spine, and the wait is
// counted rather than bounded by a promise in this comment.
//
// Conservative SystemVerilog subset (charter 2); no package dependencies.
// Lint gate: lint_terrain_lodfeed.
`default_nettype none

module zhao_terrain_lodfeed #(
    parameter int unsigned SLOTW = 10,
    parameter int unsigned EDGE  = 33,
    // The directory's generation width.  Only the handle check below uses it.
    parameter int unsigned GENW  = 8,
    // Owner ruling R22: the MESH reading.  Passed to `zhao_terrain_loddev`,
    // whose software twin is `zref::terrain::kLodDevIncludeBoundary`.
    parameter bit DEV_INCLUDE_BOUNDARY = 1'b1
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- the mip pass, OBSERVED (see the header) --------------------------
    input  var logic             f_start_i,     // TERRAIN.MIPFEED's `mg_start_o`
    input  var logic [SLOTW-1:0] f_slot_i,      // ... and its `mg_job_slot_o`
    // THE REST OF THE RESIDENCY HANDLE, ON THE SAME PULSE AS THE SLOT.  These
    // are TERRAIN.MIPFEED's `mg_job_gen_o` and `mg_job_epoch_o`, published by
    // the same block on the same cycle as `mg_job_slot_o`, so the three arrive
    // as ONE fact and not as three wires that happen to agree.  They exist for
    // the handle check below and for nothing else: this block does not read a
    // generation and never compares one itself.
    input  var logic [GENW-1:0]  f_gen_i,
    input  var logic [31:0]      f_epoch_i,
    input  var logic [15:0]      f_src_id_i,
    input  var logic             f_valid_i,     // `mg_fine_valid && mg_fine_ready`
    input  var logic signed [15:0] f_h_i,       // `mg_fine_h`, height16

    // ---- `zhao_terrain_devstore`'s write port -----------------------------
    output var logic             w_valid_o,
    input  var logic             w_ready_i,
    output var logic [SLOTW-1:0] w_slot_o,
    output var logic [3:0]       w_sp_o,
    output var logic [23:0]      w_dev1_o,
    output var logic [23:0]      w_dev2_o,
    output var logic [23:0]      w_dev3_o,
    // The subpatch CENTRE HEIGHT, read out of this same lattice at the vertex
    // (ox+4, oz+4) as it streams past.  zhao_terrain_lod measures its camera
    // distance to the subpatch centre; x and z come free from TERRAIN.PLACE's
    // placed column/row stream at serve time, and only the height has to be
    // carried across, because the lattice is gone by then.
    output var logic signed [15:0] w_cy_o,
    // THE SOURCE ID OF THE PAGE THIS RECORD CAME FROM.  Added 2026-09-20 for
    // owner ruling R70, whose first requirement is "the interval's `src_id`
    // recording which source an interval came from" -- MEASURE.HISTOGRAM's
    // `ev_src_id_i` needs a value and the obvious one is wrong.
    //
    // IT IS NOT `src_q` AND THE DIFFERENCE IS THE WHOLE POINT.  `src_q` is the
    // id of the lattice currently FILLING; the walk that emits these records
    // runs behind it and a new page may have started (that is what
    // `lattices_dropped_o` counts).  Taking `src_q` -- or TERRAIN.MIPFEED's
    // `ps_src_id_o`, which is the same hazard one block up -- would stamp
    // page B's id on page A's deviations, and nothing downstream could see it:
    // the histogram is metric-agnostic by design and would bucket a correct
    // magnitude under a wrong source forever.
    //
    // `zhao_terrain_loddev`'s `dev_src_id_o` is the echo of the `start_src_id_i`
    // it was STARTED with, so it travels with the walk by construction.  That
    // is the value, and it costs no register here.
    output var logic [15:0]      w_src_id_o,

    // ---- the store's invalidation: this slot's records are now stale ------
    // Raised on the START of a lattice, not its end: from that moment the
    // records in the store describe a page that is being replaced.
    //
    // IT HAS A SECOND SOURCE SINCE 2026-09-22 -- the stale verdict of the
    // handle check below -- and the two are ARBITRATED HERE rather than ORed
    // outside.  An OR would silently drop one of them on the cycle they
    // coincide, and the one it dropped would be invisible: both are "clear a
    // slot", and nothing downstream can tell a slot that was cleared once from
    // a slot that should have been cleared twice.
    output var logic             inv_valid_o,
    output var logic [SLOTW-1:0] inv_slot_o,

    // ---- THE DIRECTORY'S HANDLE CHECK -- core entry I27 --------------------
    // WHY THIS BLOCK IS THE CALLER, in the words `zhao_console_core.sv` entry
    // I27 used before the caller existed: "this port's first honest caller is
    // lodfeed-with-the-store, not the subpatch issuer, and whoever composes
    // the store owes this check in the same commit."  The store composed on
    // 2026-09-21 and the check did not come with it; this is that debt.
    //
    // THE HAZARD, STATED AS A SEQUENCE RATHER THAN AS A RISK.  This block
    // holds `slot_q` across a ~9,700-clock walk and `zhao_terrain_devstore`
    // files the sixteen records BY SLOT.  If the page in that slot is evicted
    // and the slot reloaded while the walk runs, the new page's own start is
    // DROPPED here (`lattices_dropped_o`) but its `inv` still clears the slot
    // -- and this walk then re-fills it with the OLD page's deviations, under
    // the NEW page's handle.  Every counter balances: the records are real,
    // the count is right, and `r_fresh_o` reads high.  TERRAIN.LOD would then
    // decide the new page's tessellation from the old page's terrain.
    //
    // AND THE CHECK IS NOT BLIND, which is the property CLAUDE.md's
    // metadata-swap chapter says to establish before a detector is trusted.
    // The two sides of the comparison are clocked by DIFFERENT THINGS: the
    // held handle is this block's register, enabled by an accepted start; the
    // answer is `zhao_terrain_residency_v2`'s key RAM, written by the
    // directory's own claim/evict FSM.  No enable drives both, so a swap
    // cannot move them together.
    //
    // IT IS A QUERY AND MAY GO UNANSWERED.  The directory's address port is
    // shared between a mutation, a lookup and a check, and a check that loses
    // is simply not answered that clock.  So `chk_valid_o` is a LEVEL held
    // until `chk_valid_i` arrives, not a pulse, and `chk_unanswered_clocks_o`
    // says how long it waited.  Nothing waits on the answer -- the records are
    // already committed -- so a slow directory costs accuracy of an
    // invalidation, never a stall.
    output var logic             chk_valid_o,
    output var logic [SLOTW-1:0] chk_slot_o,
    output var logic [GENW-1:0]  chk_gen_o,
    output var logic [31:0]      chk_epoch_o,
    input  var logic             chk_valid_i,
    input  var logic             chk_stale_i,

    // ---- evidence ---------------------------------------------------------
    output var logic [31:0] lattices_seen_o,
    output var logic [31:0] lattices_walked_o,
    output var logic [31:0] lattices_dropped_o,   // a start while busy
    output var logic [31:0] surface1_samples_o,   // counted and dropped (law 7)
    output var logic [31:0] stray_samples_o,      // a sample with no start
    output var logic [31:0] walk_clocks_o,        // the measured cost
    output var logic [31:0] dev_records_o,
    output var logic [31:0] dev_clipped_o,
    output var logic [31:0] dev_vertices_o,
    output var logic [31:0] dev_lattice_reads_o,
    // ---- the handle check's evidence --------------------------------------
    // `handles_checked_o` is the POSITIVE CONTROL for `handles_stale_o`: a
    // stale counter reading zero means nothing unless the checks that produced
    // it happened at all.  `terrain_lodpath_directed` fires BOTH -- a live
    // handle that must not invalidate and a stale one that must -- because a
    // detector's silence is a claim and this file will not let one be quoted
    // unexamined.
    output var logic [31:0] handles_checked_o,
    output var logic [31:0] handles_stale_o,
    output var logic [31:0] chk_unanswered_clocks_o,
    // A commit arriving while a check is still outstanding.  It cannot happen
    // at one walk per ~9,700 clocks against a directory that answers in two,
    // and it is counted rather than asserted so that the day the walk gets
    // faster or the directory gets busier, the console says so instead of
    // quietly checking the wrong handle.
    output var logic [31:0] chk_overrun_o,
    // An answer with no outstanding request.  The request is a LEVEL and the
    // directory is pipelined, so a held request can be accepted more than once
    // and answered more than once; the extra answers land here.
    output var logic [31:0] chk_stray_o,
    output var logic        busy_o
);

  // The ratified TERRAIN.PATCH arithmetic, imported in MODULE scope rather
  // than $unit scope -- an import::* outside a module raises IMPORTSTAR under
  // -Wall and would put these names in every file compiled beside this one.
  // Same placement, and for the same reason, as zhao_terrain_patch_acc.sv.
  import zhao_terrain_patch_law_pkg::*;

  localparam int unsigned VERTS = EDGE * EDGE;    // 1,089
  localparam int unsigned AW    = $clog2(VERTS);  // 11

  // synthesis translate_off
  initial begin
    if (EDGE != 33)
      $fatal(1, "zhao_terrain_lodfeed: EDGE must be 33 (the Island Patch v1 lattice)");
  end
  // synthesis translate_on

  // ---- the lattice buffer -------------------------------------------------
  logic signed [15:0] lat_mem [VERTS];
  logic [AW-1:0]      fill_q;        // samples taken of the current surface
  logic               fill_active_q; // a lattice is being buffered
  logic               surf1_q;       // surface 0 is done; the rest is dropped
  logic [SLOTW-1:0]   slot_q;
  // The rest of the walk's residency handle, loaded by the SAME enable as
  // `slot_q`.  That is deliberate and it is not the lockstep trap: these three
  // are ONE fact about ONE page, and the thing they are compared against lives
  // in the directory, not here.
  logic [GENW-1:0]    gen_q;
  logic [31:0]        epoch_q;
  logic [15:0]        src_q;
  logic               have_q;        // the buffer holds a complete surface 0
  // The fill cursor's (vi, vj), kept explicitly rather than divided out of
  // ill_q: 33 is not a power of two and this costs two counters.
  logic [5:0]         vi_q, vj_q;
  logic signed [15:0] cy_q [16];

  // ---- the walker ---------------------------------------------------------
  logic        dv_start_v;
  logic        dv_start_r;
  logic        dv_busy;
  logic        dv_lat_req;
  logic [5:0]  dv_lat_vi, dv_lat_vj;
  logic        dv_lat_surface;
  logic        dv_done;

  // The read is registered: `zhao_terrain_loddev` asks this cycle and reads
  // `lat_h_i` on the next, which is exactly TERRAIN.COMPCACHE's contract.
  // vj * 33 = (vj << 5) + vj.
  // 11 bits is exactly enough and the bound is not an assumption: the walker's
  // cursors are 0..32 by construction, so the largest address is
  // 32*33 + 32 = 1,088 < 2,048.  `zhao_terrain_devstore`'s DEV_MAX answer is
  // what covers a lattice that was never filled; there is no wrap to hide.
  wire [AW-1:0] dv_addr_c = (({5'd0, dv_lat_vj} << 5) + {5'd0, dv_lat_vj}) + {5'd0, dv_lat_vi};
  logic signed [15:0] lat_h_q;
  logic signed [31:0] lat_fx_c;
  // qformats 9: raw << 8, EXACT.  R176: this was the THIRD statement of the
  // conversion in this directory (with zhao_terrain_patch.sv and the package);
  // it is now the package's, which is the one the spec ratifies.  The literal
  // it replaces was `{{8{lat_h_q[15]}}, lat_h_q, 8'd0}` -- the package writes
  // the same eight zero bits as `8'b0`, which is the same value and the same
  // width.
  //
  // WHAT THE EVIDENCE FOR THIS ONE LINE IS.  The two texts are byte-identical
  // after that one declared literal substitution, and `terrain_lodpath_directed`
  // (286 checks) and `terrain_lodhist_directed` (158) drive the walk that reads
  // it and pass at this commit.  A pin-level differential against the
  // pre-factoring module also ran 500,000 vectors with no disagreement, BUT
  // THAT NULL IS WEAK HERE and it is worth saying why rather than quoting it:
  // shifting the package's conversion to `raw << 9` -- wrong on 65,535 of
  // 65,536 inputs -- did NOT make that bench disagree, because `have_q` is set
  // only when `fill_q` reaches VERTS-1, so a random stream that keeps
  // restarting its fill never starts the walk and `lat_h_q` is never read.
  // The bench exercised 196,848 clocks of output movement and none of them
  // touched this line.  A gate that cannot reach the state is not evidence
  // about the state.
  assign lat_fx_c = zhao_tp_h16_to_fx(lat_h_q);

  // The walker only ever reads surface 0 -- there is one buffer -- so its
  // surface request and its surface echo are constants it produces and this
  // block does not act on.  Declared and waived AT the declaration rather than
  // left as empty pins, so a reader can see they were considered.
  /* verilator lint_off UNUSEDSIGNAL */
  wire        dv_surface_req  = dv_lat_surface;
  wire        dv_surface_echo_w;
  /* verilator lint_on UNUSEDSIGNAL */
  wire        dv_surface_echo;
  wire [15:0] dv_src_echo;
  assign dv_surface_echo_w = dv_surface_echo;
  // THE SOURCE ECHO STOPPED BEING WASTE on 2026-09-20 (ruling R70). It used to
  // be waived beside the surface echo with the comment "the slot is the key,
  // not the src id" -- true of `zhao_terrain_devstore`, which keys on the slot,
  // and NOT true of MEASURE.HISTOGRAM, which keys intervals on the src id. The
  // waiver is narrowed to the surface echo rather than left covering both,
  // because a lint waiver that covers a signal somebody later uses stops being
  // a statement about anything.

  zhao_terrain_loddev #(
    .DEV_INCLUDE_BOUNDARY(DEV_INCLUDE_BOUNDARY)
  ) u_loddev (
    .clk  (clk),
    .rst_n(rst_n),

    .start_valid_i  (dv_start_v),
    .start_ready_o  (dv_start_r),
    .start_surface_i(1'b0),          // law 7: only the top surface is stored
    .start_src_id_i (src_q),

    .lat_req_o    (dv_lat_req),
    .lat_vi_o     (dv_lat_vi),
    .lat_vj_o     (dv_lat_vj),
    .lat_surface_o(dv_lat_surface),
    .lat_h_i      (lat_fx_c),

    .dev_valid_o  (w_valid_o),
    .dev_ready_i  (w_ready_i),
    .dev_sp_o     (w_sp_o),
    .dev_surface_o(dv_surface_echo),  // one surface; the echo is always 0
    .dev1_o       (w_dev1_o),
    .dev2_o       (w_dev2_o),
    .dev3_o       (w_dev3_o),
    .dev_src_id_o (dv_src_echo),      // -> w_src_id_o; see its port comment
    .done_o       (dv_done),

    .vertices_measured_o(dev_vertices_o),
    .lattice_reads_o    (dev_lattice_reads_o),
    .records_o          (dev_records_o),
    .clipped_o          (dev_clipped_o),
    .busy_o             (dv_busy)
  );

  assign w_slot_o   = slot_q;
  assign w_cy_o     = cy_q[w_sp_o];
  assign w_src_id_o = dv_src_echo;
  assign busy_o   = dv_busy || fill_active_q || dv_start_v || have_q;

  // ---- THE ACCEPT LAW, WRITTEN ONCE ---------------------------------------
  // The drop guard below and the invalidation arbitration both need to know
  // whether a start was TAKEN.  Deriving it twice is how two laws for one
  // event get into a file; `busy_o` is NOT it (it also carries `fill_active_q`,
  // which a start is allowed to interrupt), so the test is named here and
  // referred to.
  wire start_accept_c = f_start_i && !(dv_busy || dv_start_v || have_q);

  // ---- the handle check ---------------------------------------------------
  // THE COMMIT IS THE LAST RECORD'S ACCEPTANCE, not the walk's `done`.  The
  // store commits the row on subpatch 15 (`w_patch_done_o`), so that is the
  // instant the records become readable under this slot and the instant the
  // handle they were filed under has to be confirmed.
  wire commit_c = w_valid_o && w_ready_i && (w_sp_o == 4'd15);

  logic             ck_v_q;
  logic [SLOTW-1:0] ck_slot_q;
  logic [GENW-1:0]  ck_gen_q;
  logic [31:0]      ck_epoch_q;
  // The handle is COPIED at the commit rather than read live off `slot_q`,
  // because a new lattice may be accepted while the answer is in flight and
  // would carry `slot_q` away with it.  Checking a handle the walk no longer
  // owns is the defect this port exists to catch, performed by the checker.
  assign chk_valid_o = ck_v_q;
  assign chk_slot_o  = ck_slot_q;
  assign chk_gen_o   = ck_gen_q;
  assign chk_epoch_o = ck_epoch_q;

  wire stale_fire_c = ck_v_q && chk_valid_i && chk_stale_i;

  // The one-deep hold for the cycle both invalidation sources fire together.
  logic             pend_v_q;
  logic [SLOTW-1:0] pend_slot_q;
  // BUSY IS EVERY STATE A NEW PAGE WOULD DISTURB, not just the walk.  The
  // handover window -- a surface complete (have_q), the start presented and
  // not yet taken (dv_start_v) -- is exactly where the drop guard above
  // needs it, and a usy_o that went low there would tell a caller the block
  // was idle one cycle before it started work.

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      fill_q        <= '0;
      vi_q          <= '0;
      vj_q          <= '0;
      fill_active_q <= 1'b0;
      surf1_q       <= 1'b0;
      slot_q        <= '0;
      gen_q         <= '0;
      epoch_q       <= '0;
      src_q         <= '0;
      have_q        <= 1'b0;
      lat_h_q       <= '0;
      dv_start_v    <= 1'b0;
      inv_valid_o   <= 1'b0;
      inv_slot_o    <= '0;
      ck_v_q        <= 1'b0;
      ck_slot_q     <= '0;
      ck_gen_q      <= '0;
      ck_epoch_q    <= '0;
      pend_v_q      <= 1'b0;
      pend_slot_q   <= '0;
      handles_checked_o       <= '0;
      handles_stale_o         <= '0;
      chk_unanswered_clocks_o <= '0;
      chk_overrun_o           <= '0;
      chk_stray_o             <= '0;
      lattices_seen_o    <= '0;
      lattices_walked_o  <= '0;
      lattices_dropped_o <= '0;
      surface1_samples_o <= '0;
      stray_samples_o    <= '0;
      walk_clocks_o      <= '0;
    end else begin
      inv_valid_o <= 1'b0;

      if (dv_busy) walk_clocks_o <= walk_clocks_o + 32'd1;

      // The walk has finished with the buffer.  Cleared BEFORE the start arm
      // below so that a page arriving on the very cycle a walk retires still
      // opens the buffer rather than being cleared out from under itself.
      if (dv_done) fill_active_q <= 1'b0;

      // ---- a new lattice begins -----------------------------------------
      if (f_start_i) begin
        lattices_seen_o <= lattices_seen_o + 32'd1;
        // have_q IS PART OF THE BUSY TEST and its absence was a real hole the
        // directed test found: a surface completes, have_q goes high, and the
        // walk has not started yet.  A page arriving in that one-cycle window
        // passed the guard, overwrote slot_q, and the previous page's sixteen
        // records were then committed UNDER THE NEW PAGE'S SLOT -- with every
        // counter still balancing, because the records were real and the count
        // was right.  Exactly the record-swap shape CLAUDE.md records.
        if (!start_accept_c) begin
          // The previous page's walk has not finished.  Drop this lattice
          // rather than stall the paging spine; the store answers DEV_MAX.
          //
          // AND THIS IS THE DROP THE HANDLE CHECK EXISTS FOR.  The lattice is
          // dropped, but the page behind it is real and the directory has
          // already reassigned the slot; the walk in progress will finish and
          // file its records under a handle that has moved.
          lattices_dropped_o <= lattices_dropped_o + 32'd1;
          fill_active_q <= 1'b0;
        end else begin
          fill_active_q <= 1'b1;
          fill_q        <= '0;
          vi_q          <= '0;
          vj_q          <= '0;
          surf1_q       <= 1'b0;
          have_q        <= 1'b0;
          slot_q        <= f_slot_i;
          gen_q         <= f_gen_i;
          epoch_q       <= f_epoch_i;
          src_q         <= f_src_id_i;
        end
      end

      // ---- the handle check, and the invalidation it can raise ------------
      // ARBITRATION FIRST, so there is exactly one place `inv_valid_o` is
      // written and the precedence is visible.  An accepted start wins the
      // cycle, because its invalidation is about a page ALREADY being
      // replaced and delaying it would let the new walk's first records land
      // beside the old page's; the stale verdict is about records already
      // committed and loses nothing by waiting a clock.
      if (start_accept_c) begin
        inv_valid_o <= 1'b1;
        inv_slot_o  <= f_slot_i;
        if (stale_fire_c) begin
          pend_v_q    <= 1'b1;
          pend_slot_q <= ck_slot_q;
        end
      end else if (stale_fire_c) begin
        inv_valid_o <= 1'b1;
        inv_slot_o  <= ck_slot_q;
      end else if (pend_v_q) begin
        inv_valid_o <= 1'b1;
        inv_slot_o  <= pend_slot_q;
        pend_v_q    <= 1'b0;
      end

      if (!ck_v_q) begin
        if (commit_c) begin
          ck_v_q     <= 1'b1;
          ck_slot_q  <= slot_q;
          ck_gen_q   <= gen_q;
          ck_epoch_q <= epoch_q;
        end
        // An answer nobody is waiting for: a repeat of an already-taken
        // request, which the level-held query makes possible.  Counted, not
        // acted on.
        if (chk_valid_i) chk_stray_o <= chk_stray_o + 32'd1;
      end else begin
        if (commit_c) chk_overrun_o <= chk_overrun_o + 32'd1;
        if (chk_valid_i) begin
          ck_v_q            <= 1'b0;
          handles_checked_o <= handles_checked_o + 32'd1;
          if (chk_stale_i) handles_stale_o <= handles_stale_o + 32'd1;
        end else begin
          chk_unanswered_clocks_o <= chk_unanswered_clocks_o + 32'd1;
        end
      end

      // ---- samples --------------------------------------------------------
      if (f_valid_i) begin
        if (!fill_active_q) begin
          stray_samples_o <= stray_samples_o + 32'd1;
        end else if (surf1_q) begin
          surface1_samples_o <= surface1_samples_o + 32'd1;
        end else begin
          lat_mem[fill_q] <= f_h_i;
          // A subpatch centre is (vi, vj) = (ox + 4, oz + 4) with ox, oz in
          // {0, 8, 16, 24}, i.e. vi[2:0] == 4 and vi < 32, and likewise vj.
          // The subpatch ordinal is {vj[4:3], vi[4:3]}, which is the same
          // x-fastest order zhao_terrain_loddev emits in.
          if ((vi_q[2:0] == 3'd4) && !vi_q[5] && (vj_q[2:0] == 3'd4) && !vj_q[5])
            cy_q[{vj_q[4:3], vi_q[4:3]}] <= f_h_i;
          if (vi_q == 6'(EDGE - 1)) begin
            vi_q <= 6'd0;
            vj_q <= vj_q + 6'd1;
          end else begin
            vi_q <= vi_q + 6'd1;
          end
          if (fill_q == AW'(VERTS - 1)) begin
            surf1_q <= 1'b1;
            have_q  <= 1'b1;
          end else begin
            fill_q <= fill_q + AW'(1);
          end
        end
      end

      // ---- start the walk the cycle the surface completes -----------------
      if (have_q && !dv_busy && !dv_start_v) begin
        dv_start_v <= 1'b1;
        have_q     <= 1'b0;
      end else if (dv_start_v && dv_start_r) begin
        dv_start_v <= 1'b0;
        lattices_walked_o <= lattices_walked_o + 32'd1;
      end

      // ---- the registered lattice read ------------------------------------
      if (dv_lat_req) lat_h_q <= lat_mem[dv_addr_c[AW-1:0]];
    end
  end

endmodule

`default_nettype wire
