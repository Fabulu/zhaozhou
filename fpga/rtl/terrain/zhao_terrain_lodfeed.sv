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
// Conservative SystemVerilog subset (charter 2); no package dependencies.
// Lint gate: lint_terrain_lodfeed.
`default_nettype none

module zhao_terrain_lodfeed #(
    parameter int unsigned SLOTW = 10,
    parameter int unsigned EDGE  = 33,
    // Owner ruling R22: the MESH reading.  Passed to `zhao_terrain_loddev`,
    // whose software twin is `zref::terrain::kLodDevIncludeBoundary`.
    parameter bit DEV_INCLUDE_BOUNDARY = 1'b1
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- the mip pass, OBSERVED (see the header) --------------------------
    input  var logic             f_start_i,     // TERRAIN.MIPFEED's `mg_start_o`
    input  var logic [SLOTW-1:0] f_slot_i,      // ... and its `mg_job_slot_o`
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

    // ---- the store's invalidation: this slot's records are now stale ------
    // Raised on the START of a lattice, not its end: from that moment the
    // records in the store describe a page that is being replaced.
    output var logic             inv_valid_o,
    output var logic [SLOTW-1:0] inv_slot_o,

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
    output var logic        busy_o
);

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
  assign lat_fx_c = {{8{lat_h_q[15]}}, lat_h_q, 8'd0};   // qformats 9: raw << 8, EXACT

  // The walker only ever reads surface 0 -- there is one buffer -- so its
  // surface request and its surface echo are constants it produces and this
  // block does not act on.  Declared and waived AT the declaration rather than
  // left as empty pins, so a reader can see they were considered.
  /* verilator lint_off UNUSEDSIGNAL */
  wire        dv_surface_req  = dv_lat_surface;
  wire        dv_surface_echo_w;
  wire [15:0] dv_src_echo_w;
  /* verilator lint_on UNUSEDSIGNAL */
  wire        dv_surface_echo;
  wire [15:0] dv_src_echo;
  assign dv_surface_echo_w = dv_surface_echo;
  assign dv_src_echo_w     = dv_src_echo;

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
    .dev_src_id_o (dv_src_echo),      // the slot is the key, not the src id
    .done_o       (dv_done),

    .vertices_measured_o(dev_vertices_o),
    .lattice_reads_o    (dev_lattice_reads_o),
    .records_o          (dev_records_o),
    .clipped_o          (dev_clipped_o),
    .busy_o             (dv_busy)
  );

  assign w_slot_o = slot_q;
  assign w_cy_o   = cy_q[w_sp_o];
  assign busy_o   = dv_busy || fill_active_q || dv_start_v || have_q;
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
      src_q         <= '0;
      have_q        <= 1'b0;
      lat_h_q       <= '0;
      dv_start_v    <= 1'b0;
      inv_valid_o   <= 1'b0;
      inv_slot_o    <= '0;
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
        if (dv_busy || dv_start_v || have_q) begin
          // The previous page's walk has not finished.  Drop this lattice
          // rather than stall the paging spine; the store answers DEV_MAX.
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
          src_q         <= f_src_id_i;
          // The records for this slot describe the page being replaced.
          inv_valid_o <= 1'b1;
          inv_slot_o  <= f_slot_i;
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
