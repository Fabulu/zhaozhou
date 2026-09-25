// zhao_forge_cliff_feed.sv -- THE PRODUCER FORGE.CLIFF NEVER HAD.
//
// It issues the page command and streams the 34x34 SOLID window for
// `zhao_forge_cliff_ram` FROM THE CANONICAL TERRAIN CELL-STATE PLANE -- layer D,
// `zref::terrain::ComposedLattice::cell_state`, staged on chip by
// `zhao_terrain_compcache_front` and read through `zhao_forge_cliff_srvshare`.
//
// Owner vacation directive 2026-09-23 section 6: "Build its real
// occupancy/distance/edge producer from the canonical terrain data and share
// existing placement/geometry machinery where semantics permit. Neither an LFSR
// nor disconnected top-level test ports are production producers."
//
// ===========================================================================
// DECISION RECORD 1 -- THE PAGE IS THE STAGED PATCH, IN PATCH-LOCAL COORDINATES
// ===========================================================================
// QUESTION. `cmd_page_ci_i`/`cmd_page_cj_i` are 16-bit ABSOLUTE cell
// coordinates and `cmd_lat_w_i` is the whole lattice's vertex stride, because
// `zref::forge::rim_plan` loops pj/pi in steps of 32 across an entire island
// lattice. What does a hardware producer put there?
//
// CHOSEN. The page is the patch the compose cache is serving, expressed in
// PATCH-LOCAL coordinates: `page_ci = page_cj = 0`, `cw = ch = 32`,
// `lat_w = LAT_W = 33`. Which patch it was travels as `cmd_src_id_o`, taken
// from `serve_src_id_i` and held for the whole page.
//
// REASON. `zhao_terrain_compcache_front` stages EXACTLY ONE patch -- 33x33
// lattice vertices, 32x32 cells -- because the full 256-patch store is 161% of
// the device's M10K. One staged patch is therefore EXACTLY ONE FORGE.CLIFF
// PAGE, with no remainder and no partial page, and the cell-state read port is
// 5+5 bits, which can address that patch and nothing else. Every downstream
// reader of a rim edge (the emission stage) has to come back to the same
// 6-bit-indexed `lat_*` port for the vertex it names, so patch-local is the
// space every consumer already speaks. World position is NOT lost by this
// choice: it is carried by `lat_wx_o`/`lat_wz_o`, which are TERRAIN.PLACE's
// placed coordinates, so the placement law is CONSUMED, not re-derived -- there
// is no second opinion about where a lattice vertex is.
//
// ALTERNATIVE REJECTED. Absolute island cell coordinates, i.e. `page_ci =
// patch_ix * 32`. It requires a patch (ix, iz) that the serve face does not
// carry -- `serve_src_id_o` is an OPAQUE 16-bit id forwarded from the fill
// producer, not a structured coordinate (zhao_terrain_compcache_front.sv:
// 115-123, 305). Synthesising one here would be a second, unratified opinion
// about patch identity sitting beside the real one, and the first time the two
// disagreed the edges would be attributed to the wrong patch with every
// handshake legal. If absolute coordinates are ever wanted, the honest route is
// for the FILL side to publish (ix, iz) and for this block to take it as a
// port; that is a port addition, not a rewrite.
//
// CONSTRAINTS / COST. Rim edges are patch-local, so a consumer must not add
// them across patches. `cmd_src_id_o` makes the owning patch explicit on every
// edge, which is the mitigation and is why it is wired rather than tied.
//
// CODE / TEST / COMPATIBILITY. No change to `zhao_forge_cliff_ram`, whose ports
// and law are untouched; `lat_w_r` arithmetic inside it is exercised at 33
// rather than at an island stride, which the differential already covers.
//
// ===========================================================================
// DECISION RECORD 2 -- THE ONE-CELL HALO IS A KNOB, AND ITS DEFAULT IS SOLID
// ===========================================================================
// QUESTION. Law C1 gives the evaluator a 34x34 window: the 32x32 page plus a
// ONE-CELL HALO, "window (0,0) is page cell (-1,-1); off-lattice loads as 0".
// The halo of a staged patch is the BORDER CELLS OF THE FOUR NEIGHBOURING
// PATCHES, and nothing in fpga/rtl stages a neighbour -- the 5-bit cell address
// cannot even express one, it silently aliases into the staged patch. So what
// goes in the ring?
//
// CHOSEN. A PORT, `halo_substance_i[1:0]`, defaulted by the console to SOLID
// (2'd0). The ring is filled with it; no read is issued for a halo cell.
//
// REASON. The two candidate constants are not close in consequence, and the
// arithmetic decides it:
//   * halo = VOID (the literal reading of "off-lattice loads as 0"): every one
//     of the 128 border cells of every patch faces a non-solid neighbour, so
//     every patch emits a wall around its whole perimeter -- a visible grid of
//     spurious walls along every interior seam, 128 edges per page of pure
//     artefact, against a budget of 512.
//   * halo = SOLID: no rim is emitted at a patch seam at all. The cost is the
//     opposite and much smaller -- a genuine cliff that happens to lie exactly
//     ON a seam is not emitted by this producer.
// "Off-lattice loads as 0" is written for a cell outside the ISLAND, and a
// patch seam is not outside the island; reading it as though it were is the
// mistake this record exists to avoid.
//
// AND IT IS A PORT RATHER THAN A CONSTANT because CLAUDE.md's art law applies
// to a rule that decides what is drawn: "never remove the owner's control in
// the name of fidelity -- every shape, colour and timing value belongs in a
// named, editable constant." Both policies are one console-side literal apart,
// and a bench can drive either.
//
// ALTERNATIVE REJECTED (recorded, not built). Asking
// `zhao_terrain_island_dir` whether the neighbouring patch exists, and using
// VOID for an absent neighbour (a true island edge, where a cliff IS wanted)
// and SOLID for a present one. That is the right answer and it is NOT
// reachable today: the query is keyed on (ix, iz), which DECISION RECORD 1
// establishes this block does not have. It becomes a two-line change the day
// the fill side publishes a patch coordinate, and `halo_substance_i` is already
// the port it would drive.
//
// ===========================================================================
// DECISION RECORD 3 -- ONE COUNTER, SO NOTHING CAN DRIFT APART
// ===========================================================================
// QUESTION. The cell-state read is one clock, registered, with no tag and no
// back channel, and this block's client may be DENIED any cycle the incumbent
// asks. CLAUDE.md: "a producer feeding a RAM-backed evaluator is the shape
// where a stall pairs one record's geometry with another's occupancy while
// every counter balances." How is that made impossible rather than detected?
//
// CHOSEN. Removed, not detected. This block contains exactly ONE piece of
// position state -- `wr_r`/`wc_r`, the window row and column. The read address
// is computed COMBINATIONALLY from it, and it advances only on an ACCEPTED
// output beat (`ld_valid_o && ld_ready_i`). While a read is in flight the
// position is frozen, and only one read is ever in flight. There is therefore
// no second quantity for the first to disagree with: a denied cycle, a stalled
// cycle and a late answer all produce the same thing, which is nothing
// happening.
//
// REASON / ALTERNATIVE REJECTED. The obvious shape -- a free-running address
// counter feeding the read, and a separate write pointer filling the window --
// is faster (it pipelines) and is exactly the defect repaired in
// `zhao_field_earth_adapter`: on a stall the write pointer and the address
// separate, cell A's index takes cell B's occupancy, and every counter still
// balances because no counter looks at the field that moved. The throughput
// this costs is affordable and was measured against the budget, not assumed:
// two clocks per interior cell is 2,048 clocks plus 132 halo beats per page,
// against a 1.67 M-clock frame. It is not close.
//
// CODE / TEST CONSEQUENCE. `cs_req_o` is held up until granted, so the block
// tolerates an incumbent that asks on every cycle for an unbounded time; the
// bench drives exactly that and `cs_denied_o` records it.
//
// ===========================================================================
// DECISION RECORD 4 -- A PAGE ALWAYS COMPLETES, EVEN WHEN THE PATCH IS PULLED
// ===========================================================================
// QUESTION. `zhao_forge_cliff_ram` asserts `ld_ready_o` for exactly 1,156 beats
// and cannot be told a page was abandoned (`zhao_forge_cliff_ram.sv:599`). If
// the compose cache releases the staged patch mid-window, what does this block
// do?
//
// CHOSEN. It finishes the page, substituting `halo_substance_i` for every
// remaining cell, and counts `cells_degraded_o` and `pages_degraded_o`.
//
// REASON. Stopping would hang the evaluator in StLoad forever, which is a
// deadlock in the console for a transient condition. Substituting the halo
// policy degrades toward "no spurious wall" rather than toward "a wall
// everywhere", so a degraded page is visually quiet rather than loud, and the
// two counters say plainly that it happened. A degraded page is REAL terrain
// for the cells that were read and a declared default for the rest; it is not
// noise, and it is not silent.
//
// ALTERNATIVE REJECTED. Holding the stream until the patch returns. The next
// patch to be staged is a DIFFERENT patch, so resuming would splice two
// patches' occupancy into one window -- precisely the record-swapping defect
// DECISION RECORD 3 is about, reintroduced at page scale.
//
// ---------------------------------------------------------------------------
// TRIGGERING
// ---------------------------------------------------------------------------
// One page per STAGED PATCH, self-triggered: a page starts when `arm_i` is high
// and the compose cache is serving a patch whose `serve_src_id_i` this block
// has not yet planned. No external sequencer is introduced, because the
// residency handshake already IS the sequence -- TERRAIN.TESS consumes a patch
// and releases it, and the next one arrives. `arm_i` is the owner's off switch
// for the whole path.
//
// Conservative SystemVerilog subset (charter 2): elaboration checks inside
// `initial begin`, no bare module-scope `if`, explicit generate where used.

module zhao_forge_cliff_feed #(
    // The compose cache's lattice. Elaboration-checked against the window the
    // evaluator declares, so the two cannot disagree about the page size.
    parameter int unsigned LAT_W    = 33,
    parameter int unsigned LAT_H    = 33,
    parameter int unsigned CENSUS_W = 32
) (
    input var logic clk,
    input var logic rst_n,

    // ---- the owner's switches ------------------------------------------------
    input var logic       arm_i,
    // DECISION RECORD 2. 2'd0 = SOLID (spec/terrain_rules 3.3), the default.
    input var logic [1:0] halo_substance_i,
    // Passed to the evaluator unchanged. See the console's wiring note: the
    // priority degrade needs a per-vertex 1/w store, and what drives this port
    // is the console's business, not this block's.
    input var logic       vdist_en_i,

    // ---- the compose cache's residency face ----------------------------------
    input var logic        serve_valid_i,
    input var logic [15:0] serve_src_id_i,

    // ---- the cell-state client (zhao_forge_cliff_srvshare) --------------------
    output var logic       cs_req_o,
    output var logic [4:0] cs_ci_o,
    output var logic [4:0] cs_cj_o,
    input  var logic       cs_grant_i,
    input  var logic       cs_rsp_valid_i,
    input  var logic [1:0] cs_substance_i,

    // ---- the page command, into zhao_forge_cliff_ram -------------------------
    output var logic        cmd_valid_o,
    input  var logic        cmd_ready_i,
    output var logic [15:0] cmd_page_ci_o,
    output var logic [15:0] cmd_page_cj_o,
    output var logic [ 5:0] cmd_cw_o,
    output var logic [ 5:0] cmd_ch_o,
    output var logic [15:0] cmd_lat_w_o,
    output var logic        cmd_vdist_en_o,
    output var logic [15:0] cmd_src_id_o,

    // ---- the 34x34 SOLID window, row-major, window (0,0) first ---------------
    output var logic ld_valid_o,
    input  var logic ld_ready_i,
    output var logic ld_solid_o,

    // ---- evidence -------------------------------------------------------------
    output var logic                busy_o,
    output var logic [CENSUS_W-1:0] pages_issued_o,     // page commands accepted
    output var logic [CENSUS_W-1:0] windows_done_o,     // windows fully streamed
    output var logic [CENSUS_W-1:0] cs_reads_o,         // interior cells read
    output var logic [CENSUS_W-1:0] cs_denied_o,        // cycles lost to the incumbent
    output var logic [CENSUS_W-1:0] solid_cells_o,      // cells that read SOLID
    output var logic [CENSUS_W-1:0] cells_degraded_o,   // FAULT: halo substituted
    output var logic [CENSUS_W-1:0] pages_degraded_o    // FAULT: pages with any of the above
);

  localparam int unsigned WinDim = 34;  // page + one-cell halo (law C1)
  localparam int unsigned WinAW  = 6;   // holds 0..33
  localparam logic [1:0]  SubSolid = 2'd0;  // spec/terrain_rules 3.3

  initial begin
    if (LAT_W != 33 || LAT_H != 33) begin
      $fatal(1, "zhao_forge_cliff_feed: only the 33x33 staged patch is supported (%0dx%0d)",
             LAT_W, LAT_H);
    end
    if ((LAT_W - 1) + 2 != int'(WinDim)) begin
      $fatal(1, "zhao_forge_cliff_feed: window %0d does not equal cells+halo %0d",
             WinDim, (LAT_W - 1) + 2);
    end
    if (CENSUS_W < 8) begin
      $fatal(1, "zhao_forge_cliff_feed: CENSUS_W must be >= 8 (is %0d)", CENSUS_W);
    end
  end

  // ---- the page geometry, DECISION RECORD 1 ---------------------------------
  localparam logic [15:0] PageCi = 16'd0;
  localparam logic [15:0] PageCj = 16'd0;
  localparam logic [ 5:0] PageCw = 6'(LAT_W - 1);
  localparam logic [ 5:0] PageCh = 6'(LAT_H - 1);
  localparam logic [15:0] PageLatW = 16'(LAT_W);

  // ---- state ---------------------------------------------------------------
  localparam logic [1:0] StIdle = 2'd0;
  localparam logic [1:0] StCmd  = 2'd1;
  localparam logic [1:0] StLoad = 2'd2;

  logic [1:0] st_r;

  // DECISION RECORD 3: this pair is the block's ONLY position state.
  logic [WinAW-1:0] wr_r, wc_r;

  logic [15:0] src_id_r;
  logic [15:0] last_src_id_r;
  logic        seen_r;
  logic        page_degraded_r;

  // ---- where we are in the window ------------------------------------------
  logic is_halo_c;
  assign is_halo_c = (wr_r == '0) || (wr_r == WinAW'(WinDim - 1)) ||
                     (wc_r == '0) || (wc_r == WinAW'(WinDim - 1));

  // The interior cell this window position names. Combinational from the one
  // position register; there is no second address anywhere.
  logic [4:0] cell_ci_c, cell_cj_c;
  assign cell_ci_c = wc_r[4:0] - 5'd1;
  assign cell_cj_c = wr_r[4:0] - 5'd1;

  // A cell we cannot read honestly: the patch is gone. DECISION RECORD 4.
  logic patch_lost_c;
  assign patch_lost_c = !serve_valid_i || (serve_src_id_i != src_id_r);

  // ---- the read client ------------------------------------------------------
  // Asked only for an interior cell of a page whose patch is still there, and
  // only while no answer is outstanding AND none is being held.
  //
  // The held copy (`have_r`/`sub_r`) is not optional. The sharer's
  // `c1_rsp_valid_o` is high for EXACTLY ONE CYCLE, so an answer presented
  // while `ld_ready_i` happens to be low would be lost and the window would
  // silently take the NEXT cell's occupancy at this cell's index -- the
  // record-swapping shape, arriving through the back door. The evaluator does
  // hold `ld_ready_o` high for all of StLoad (`zhao_forge_cliff_ram.sv:599`),
  // so this cannot bite in the composed console; it is registered anyway,
  // because a correctness argument that depends on a remote block's current
  // implementation is a correctness argument with a future in it.
  logic       inflight_r;
  logic       have_r;
  logic [1:0] sub_r;
  logic want_read_c;
  assign want_read_c = (st_r == StLoad) && !is_halo_c && !patch_lost_c &&
                       !inflight_r && !have_r;

  assign cs_req_o = want_read_c;
  assign cs_ci_o  = cell_ci_c;
  assign cs_cj_o  = cell_cj_c;

  // ---- the output beat ------------------------------------------------------
  // A halo cell, or a cell whose patch vanished, is presented immediately from
  // the policy; an interior cell is presented on the beat its answer lands.
  logic use_policy_c;
  assign use_policy_c = is_halo_c || patch_lost_c;

  logic beat_c;
  assign beat_c = (st_r == StLoad) && (use_policy_c || have_r);

  logic [1:0] beat_sub_c;
  assign beat_sub_c = use_policy_c ? halo_substance_i : sub_r;

  assign ld_valid_o = beat_c;
  assign ld_solid_o = (beat_sub_c == SubSolid);

  logic accept_c;
  assign accept_c = ld_valid_o && ld_ready_i;

  logic last_col_c, last_row_c;
  assign last_col_c = (wc_r == WinAW'(WinDim - 1));
  assign last_row_c = (wr_r == WinAW'(WinDim - 1));

  // ---- the command ----------------------------------------------------------
  assign cmd_valid_o    = (st_r == StCmd);
  assign cmd_page_ci_o  = PageCi;
  assign cmd_page_cj_o  = PageCj;
  assign cmd_cw_o       = PageCw;
  assign cmd_ch_o       = PageCh;
  assign cmd_lat_w_o    = PageLatW;
  assign cmd_vdist_en_o = vdist_en_i;
  assign cmd_src_id_o   = src_id_r;

  assign busy_o = (st_r != StIdle);

  // ---- a new patch to plan --------------------------------------------------
  logic start_c;
  assign start_c = (st_r == StIdle) && arm_i && serve_valid_i &&
                   (!seen_r || (serve_src_id_i != last_src_id_r));

  // ---- evidence -------------------------------------------------------------
  logic [CENSUS_W-1:0] pages_issued_r, windows_done_r, cs_reads_r, cs_denied_r;
  logic [CENSUS_W-1:0] solid_cells_r, cells_degraded_r, pages_degraded_r;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_r            <= StIdle;
      wr_r            <= '0;
      wc_r            <= '0;
      src_id_r        <= '0;
      last_src_id_r   <= '0;
      seen_r          <= 1'b0;
      page_degraded_r <= 1'b0;
      inflight_r      <= 1'b0;
      have_r          <= 1'b0;
      sub_r           <= '0;

      pages_issued_r   <= '0;
      windows_done_r   <= '0;
      cs_reads_r       <= '0;
      cs_denied_r      <= '0;
      solid_cells_r    <= '0;
      cells_degraded_r <= '0;
      pages_degraded_r <= '0;
    end else begin
      // The read client's single-flight bit and the held answer. A grant puts
      // one read in the air; the landing takes it out of the air and into
      // `sub_r`; an accepted beat frees `sub_r` for the next cell. Only one of
      // the three can happen to a given cell, and the window position does not
      // move until the third, which is DECISION RECORD 3 in force.
      if (cs_grant_i) begin
        inflight_r <= 1'b1;
        if (!(&cs_reads_r)) begin
          cs_reads_r <= cs_reads_r + CENSUS_W'(1);
        end
      end else if (cs_rsp_valid_i && inflight_r) begin
        inflight_r <= 1'b0;
        have_r     <= 1'b1;
        sub_r      <= cs_substance_i;
      end

      if (accept_c && !use_policy_c) begin
        have_r <= 1'b0;
      end

      if (want_read_c && !cs_grant_i && !(&cs_denied_r)) begin
        cs_denied_r <= cs_denied_r + CENSUS_W'(1);
      end

      case (st_r)
        StIdle: begin
          if (start_c) begin
            src_id_r        <= serve_src_id_i;
            wr_r            <= '0;
            wc_r            <= '0;
            page_degraded_r <= 1'b0;
            inflight_r      <= 1'b0;
            have_r          <= 1'b0;
            st_r            <= StCmd;
          end
        end

        StCmd: begin
          if (cmd_ready_i) begin
            st_r <= StLoad;
            if (!(&pages_issued_r)) begin
              pages_issued_r <= pages_issued_r + CENSUS_W'(1);
            end
          end
        end

        StLoad: begin
          if (accept_c) begin
            if (ld_solid_o && !(&solid_cells_r)) begin
              solid_cells_r <= solid_cells_r + CENSUS_W'(1);
            end
            // DECISION RECORD 4: a cell the policy had to answer for.
            if (!is_halo_c && patch_lost_c) begin
              page_degraded_r <= 1'b1;
              if (!(&cells_degraded_r)) begin
                cells_degraded_r <= cells_degraded_r + CENSUS_W'(1);
              end
            end

            if (last_col_c) begin
              wc_r <= '0;
              if (last_row_c) begin
                // The window is complete. Retire the patch id so the same patch
                // is not planned twice, and only then go idle.
                last_src_id_r <= src_id_r;
                seen_r        <= 1'b1;
                st_r          <= StIdle;
                if (!(&windows_done_r)) begin
                  windows_done_r <= windows_done_r + CENSUS_W'(1);
                end
                if ((page_degraded_r || (!is_halo_c && patch_lost_c)) &&
                    !(&pages_degraded_r)) begin
                  pages_degraded_r <= pages_degraded_r + CENSUS_W'(1);
                end
              end else begin
                wr_r <= wr_r + WinAW'(1);
              end
            end else begin
              wc_r <= wc_r + WinAW'(1);
            end
          end
        end

        default: st_r <= StIdle;
      endcase
    end
  end

  assign pages_issued_o   = pages_issued_r;
  assign windows_done_o   = windows_done_r;
  assign cs_reads_o       = cs_reads_r;
  assign cs_denied_o      = cs_denied_r;
  assign solid_cells_o    = solid_cells_r;
  assign cells_degraded_o = cells_degraded_r;
  assign pages_degraded_o = pages_degraded_r;

endmodule
