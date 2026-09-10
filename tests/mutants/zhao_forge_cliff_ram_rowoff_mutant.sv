// zhao_forge_cliff_ram_rowoff_mutant.sv -- A DELIBERATELY BROKEN COPY. NOT SHIPPED.
//
// This exists to show the storage differential FAILING on the class of fault
// the bitmap-RAM rewrite can introduce and the golden could not: a ROW-BUFFER
// MISALIGNMENT. The candidate replaces a 1,156-bit window with three live row
// registers and a prefetched fourth; if the prefetch fetches the wrong row,
// every handshake still completes, every counter still balances, and only
// the south-neighbour bit of every cell from page row 1 onward is wrong. The
// one substantive change:
//
//     pf_row_c = WinAW'(sc_cj_r) + WinAW'(3);   ->   ... + WinAW'(2);
//
// so the row rotated in at each row end is the CURRENT row again, not the
// next one. A one-row page (ch == 1) is untouched by this -- which is why the
// control also runs one, to show the break passing a weak fixture.
//
// INVERTED POLARITY: driven by tests/forge/forge_cliff_ram_rowoff_control.cpp
// against the same zref oracle the suites use; the control PASSES when the
// differential comparison FAILS on a multi-row page, and it also asserts that
// walk_fault_o stays SILENT -- that instrument is not this fault's, and a
// counter that fired here would be a counter that fires on everything.
//
// The module is RENAMED so a source-list mistake cannot elaborate it in
// place of the real one. REGENERATE IT if zhao_forge_cliff_ram.sv changes
// shape.

// zhao_forge_cliff_ram.sv — FORGE.CLIFF, the bitmap-RAM CANDIDATE beside the
// golden `zhao_forge_cliff.sv` (ALM-Liberation Roadmap §6 / §14 Commit6;
// reports/FORGE-CLIFF-REARCH-ARCHITECTURE-20260909.md §2, §3, §8 step D1).
//
// SAME LAW, SAME PORTS (plus one diagnostic counter), SAME ALGORITHM. Every
// law quoted in the golden's header — the rim predicate, the frozen degrade
// order, R1/R2/R3, the run theorem, the 32-pass threshold search, F1–F5,
// C1–C5 — is unchanged and is NOT restated here; read the golden first. This
// file changes STORAGE ONLY. If the differential moves, it is the storage.
//
// ---------------------------------------------------------------------------
// WHAT MOVED, AND WHY
// ---------------------------------------------------------------------------
// The golden's map row (reports/synthesis/zhao_block_map.json, Quartus 17.0.2
// map-only, rtlCleanAtHead) reports 3,875 registers. Two declarations are
// 3,204 of them:
//
//   solid_r [1155:0]  — the 34x34 SOLID window as flip-flops, with a 1,156-way
//                       write decoder and five dynamic bit-selects off an
//                       11-bit computed index (self + four neighbours);
//   alive_r [2047:0]  — one liveness bit per edge, three dynamically indexed
//                       write sites, 2048:1 read cones, a bulk clear per page.
//
// S1. THE SOLID WINDOW BECOMES ONE RAM PLUS FOUR ROW REGISTERS. The window is
//     stored as WinDim words of WinDim bits (row-major; word 0 is the north
//     halo row). StLoad shifts the 1,156-bit stream through a 33-bit assembly
//     register and writes one whole 34-bit word per 34 accepted bits — same
//     handshake, same bit order, same 1,156-cycle load. Enumeration consults
//     only three rows at a time (north = window row cj, current = cj+1,
//     south = cj+2), so those live in three 34-bit registers and the RAM's
//     own 34-bit output register PREFETCHES row cj+3 during the row's first
//     cycles. A cell row is at least 4 clocks (cw >= 1, four sides), the
//     synchronous read takes one, so the prefetch never costs a bubble. The
//     five consulted bits are 34:1 selects on 6-bit indices instead of
//     1156:1 selects on an 11-bit product.
//     REJECTED (recorded, not knobbed): shifting the three live rows one bit
//     per cell so all five bits sit at fixed positions. It removes five small
//     muxes but needs a holding copy per row for the rotate (or three extra
//     RAM reads) and a partial-page (cw < 32) special case. Not worth its own
//     bugs at ~10 ALUTs a mux; revisit only if the map shows the selects.
//
// S2. THE ALIVE BITMAP IS DELETED, NOT MOVED. A merge writes `take` into the
//     head's span, and the entries it kills are EXACTLY the take-1 entries
//     following the head (runs are disjoint and each merges at most once — the
//     golden's own one-pass theorem). So after the merge phase the span field
//     IS the liveness information: a walker doing `rd += span[rd]` visits
//     every live entry in scan order and no dead one. So:
//       - StEmit and StKeep WALK the sparse table by span (`sparse_r` = a
//         merge happened) — no compaction pass at all when the merge alone
//         brought the page inside budget, or when vdist is off. This is the
//         golden's own cost or better: it skipped a dead entry in one cycle,
//         the walk skips it in zero. (The first cut compacted unconditionally
//         and the differential's cycle table showed it +512 on every
//         merge-only page; that table is the reason this rule exists.)
//       - Only the 33 threshold passes need a DENSE table, so only when a
//         priority degrade follows (over budget after merging, vdist on) does
//         StCompact run: read `rd`, write `wr`, `wr <= rd` always, the write
//         SUPPRESSED while `wr == rd` so no same-address read/write ever
//         occurs, and the priority computed in the same pass from the FINAL
//         span endpoints (C2 preserved; the golden's three clocks per edge).
//         The old StPrio IS this pass; the old StMdead's take-1 clear cycles
//         are gone. StBsCount/StGtCount/StKeep then run on the dense prefix
//         with no liveness check and no dead-entry iterations.
//       - StKeep drops by compacting (kept entries slide down to `wr`, same
//         suppression rule; `dropped` still counts BODIES, R2) and StEmit then
//         walks 0..cnt''-1 densely.
//     The roadmap's §6.2 sheet keeps a 2048x1 alive RAM; this route needs
//     none, which is one M10K fewer than that sheet.
//
// S3. TABLE WRITES ARE ONE SITE PER TABLE. The golden's four payload tables
//     ALREADY infer as Simple Dual Port RAM (the map row's inferredMemories,
//     119,808 bits — they are not a saving and are not claimed as one). They
//     are kept in the same geometry, but each now has exactly one write
//     statement in its own clock-only process, fed by explicit
//     `*_we_c/_wa_c/_wd_c` signals decoded from the phase. The reads stay the
//     golden's `assign x = mem[idx]` form — Quartus rescued that shape with
//     a read-address register on the golden and the shape is deliberately
//     NOT changed here, so this commit changes one axis only. Making the
//     reads explicitly synchronous is the named follow-up, not this commit.
//
// ---------------------------------------------------------------------------
// MEMORY SHEET (roadmap §3.1 — one owner each, no partial writes, no
// same-address read/write, NO RESET TOUCHES ANY ARRAY; count and phase
// authorise every read)
// ---------------------------------------------------------------------------
//   memory       geometry   W port (phase)          R port (phase)        init / authorisation
//   win_mem      34 x 34    StLoad, whole word      StPrime, StEnum       none; a row is read only after all 34 rows of THIS page were written
//   edge_key_r   2048 x 12  StEnum(idx) / StCompact(wr) / StKeep(wr)
//                                                    every phase, idx < cnt  none; idx < cnt_r always, cnt_r counts accepted writes
//   edge_span_r  2048 x 6   StEnum(idx) / StMwrite(head) / StCompact(wr) / StKeep(wr)
//                                                    every phase, idx < cnt  same
//   prio_mem_r   2048 x 32  StCompact(wr)           StBsCount/StGtCount/StKeep, idx < cnt'
//                                                                          none; written for every dense entry before any read
//   run_mem_r    1024 x 17  StRuns(runs)            StMsel, ridx < runs   none; ridx < runs_r always
//   Collision rule: StCompact and StKeep write `wr` while reading `rd`, and
//   write only when wr != rd. StEnum writes idx and reads nothing. Nothing
//   else writes and reads the same table in one phase.
//   Release: cnt_r/runs_r reset to 0 per page (StIdle / StAfterEnum). No
//   payload row is ever cleared. Epoch: none needed — no reader is ever
//   authorised at an address this page has not written.
//   Expected blocks (arithmetic, NOT a fitter count): 34x34 -> 1 M10K;
//   2048x12 -> 3; 2048x6 -> 2; 2048x32 -> 7; 1024x17 -> 2. Total 15.
//
// ---------------------------------------------------------------------------
// THE ONE NEW INSTRUMENT
// ---------------------------------------------------------------------------
// `walk_fault_o` counts a span walk (StCompact, or StKeep/StEmit while
// `sparse_r`) landing PAST `cnt_r`, or reading a span of ZERO (which would
// stall the walk forever; the RTL advances by one and counts instead). Both
// states are unreachable while S2's theorem holds and every span was written
// with 1 or a take >= 2, so no legal stimulus can move it. Its positive
// controls are two committed mutants, each ONE line in the merge's span write
// (tests/mutants/): zhao_forge_cliff_ram_mutant.sv writes 0 (the zero-span
// trigger and the no-hang guard) and zhao_forge_cliff_ram_over_mutant.sv
// writes take+1 (the overshoot trigger); both are driven with inverted
// polarity by tests/forge/forge_cliff_ram_mutant_control.cpp. A first cut
// broke the ENUMERATION span instead and the counter stayed 0: with every
// span 0 no run forms, nothing merges, and no walk is ever entered — a
// positive control that never reaches the detector is not a control.
//
// Conservative SystemVerilog subset only (charter 2): explicit
// generate/endgenerate where used, elaboration checks inside `initial begin`,
// no bare module-scope `if`. Lint: clean under `-Wall`.

module zhao_forge_cliff_ram_rowoff_mutant (
    input logic clk,
    input logic rst_n,

    // one PAGE of the lattice (golden F2)
    input  logic        cmd_valid_i,
    output logic        cmd_ready_o,
    input  logic [15:0] cmd_page_ci_i,
    input  logic [15:0] cmd_page_cj_i,
    input  logic [ 5:0] cmd_cw_i,
    input  logic [ 5:0] cmd_ch_i,
    input  logic [15:0] cmd_lat_w_i,
    input  logic        cmd_vdist_en_i,
    input  logic [15:0] cmd_src_id_i,

    // the SOLID window (golden C1): 34*34 bits, row-major, window (0,0) is
    // page cell (-1,-1); off-lattice loads as 0.
    input  logic ld_valid_i,
    output logic ld_ready_o,
    input  logic ld_solid_i,

    // the vdist read master: synchronous, data the cycle after the address.
    output logic        vd_en_o,
    output logic [31:0] vd_addr_o,
    input  logic [31:0] vd_data_i,

    // forge_primitives out — one rim edge per beat.
    output logic        edge_valid_o,
    input  logic        edge_ready_i,
    output logic [15:0] edge_ci_o,
    output logic [15:0] edge_cj_o,
    output logic [ 1:0] edge_side_o,
    output logic [ 5:0] edge_span_o,
    output logic [15:0] edge_src_id_o,

    // page status (golden C4)
    output logic        page_done_o,
    output logic [11:0] page_merged_o,
    output logic [11:0] page_dropped_o,

    output logic        idle_o,
    output logic [31:0] triangles_submitted_o,

    // NEW: the span-walk fault counter (see THE ONE NEW INSTRUMENT).
    output logic [ 7:0] walk_fault_o
);

  // ---- the law's constants (identical to the golden) -------------------------
  localparam int unsigned Budget   = 512;   // F5, zref::forge::kRimBudgetPerPage
  localparam int unsigned MaxEdges = 2048;  // the TIGHT checkerboard worst case
  localparam int unsigned EIW      = 12;    // holds 0..2048 inclusive
  localparam int unsigned MaxRuns  = 1024;  // a run has >= 2 entries
  localparam int unsigned RIW      = 11;
  localparam int unsigned WinDim   = 34;    // page + one-cell halo (C1)

  // ---- the storage knobs (S1) -----------------------------------------------
  localparam int unsigned RowW     = WinDim;  // one window ROW per RAM word
  localparam int unsigned WinAW    = 6;       // holds 0..33
  localparam int unsigned PrimeLen = 4;       // 3 reads; data lands a cycle later, rotates on 1..3
  localparam int unsigned WalkFW   = 8;       // walk_fault_o width (saturating)

  localparam logic [31:0] CntMax = 32'hFFFF_FFFF;

  localparam logic [3:0] StIdle      = 4'd0;
  localparam logic [3:0] StLoad      = 4'd1;
  localparam logic [3:0] StPrime     = 4'd2;   // NEW: fill north/current/south from the RAM
  localparam logic [3:0] StEnum      = 4'd3;
  localparam logic [3:0] StAfterEnum = 4'd4;
  localparam logic [3:0] StRuns      = 4'd5;
  localparam logic [3:0] StMsel      = 4'd6;
  localparam logic [3:0] StMwrite    = 4'd7;   // was StMdead: now ONE cycle, span only
  localparam logic [3:0] StCompact   = 4'd8;   // was StPrio: span-walk compaction + priority
  localparam logic [3:0] StBsCount   = 4'd9;
  localparam logic [3:0] StBsStep    = 4'd10;
  localparam logic [3:0] StGtCount   = 4'd11;
  localparam logic [3:0] StKeep      = 4'd12;
  localparam logic [3:0] StEmit      = 4'd13;

  initial begin
    if (WinDim > (1 << WinAW)) $fatal(1, "zhao_forge_cliff_ram: WinAW too narrow for WinDim");
    if (RowW != WinDim) $fatal(1, "zhao_forge_cliff_ram: RowW must equal WinDim (one row per word)");
    if (MaxEdges > (1 << (EIW - 1))) $fatal(1, "zhao_forge_cliff_ram: EIW too narrow for MaxEdges");
  end

  // ===========================================================================
  // state
  // ===========================================================================
  logic [3:0] st_r;

  logic [15:0] pg_ci_r, pg_cj_r, lat_w_r, src_r;
  logic [ 5:0] cw_r, ch_r;
  logic        vden_r;

  // S1: the window RAM, its assembly shifter and the three live rows.
  logic [RowW-1:0]  win_mem [0:WinDim-1];   // NO reset, own process, one W site
  logic [RowW-1:0]  win_q_r;                // the RAM's output register = the prefetch row
  logic [RowW-2:0]  ld_asm_r;               // 33 bits: the 34th arrives with the write
  logic [WinAW-1:0] ld_wi_r, ld_wj_r;
  logic [RowW-1:0]  row_n_r, row_c_r, row_s_r;
  logic [2:0]       prime_k_r;

  // the payload tables — same geometry as the golden, one write site each.
  logic [ 11:0]   edge_key_r  [0:MaxEdges-1];  // {cj[4:0], ci[4:0], side[1:0]}
  logic [  5:0]   edge_span_r [0:MaxEdges-1];  // span[5:0]
  logic [ 31:0]   prio_mem_r  [0:MaxEdges-1];
  logic [ 16:0]   run_mem_r   [0:MaxRuns-1];   // {start[10:0], len[5:0]}
  logic [EIW-1:0] cnt_r;
  logic [RIW-1:0] runs_r;

  logic [5:0]     sc_ci_r, sc_cj_r;
  logic [1:0]     sc_side_r;
  logic [EIW-1:0] idx_r;   // read cursor (rd) in every phase, write cursor in StEnum
  logic [EIW-1:0] wr_r;    // S2: compaction write cursor
  logic [RIW-1:0] ridx_r;

  logic [ 5:0]    mlen_r;
  logic [EIW-1:0] need_r;
  logic [10:0]    mhead_r;
  logic [ 5:0]    mtake_r;
  logic [ 5:0]    rlen_r;
  logic [10:0]    rstart_r;
  logic [17:0]    run_first_r;
  logic [17:0]    run_prev_r;
  logic           sparse_r;   // S2: a merge happened, the table has dead entries — walk by span

  logic [11:0]    merged_r, dropped_r;
  logic [11:0]    merged_q_r, dropped_q_r;

  logic [ 1:0]    pr_ph_r;
  logic [31:0]    pr_va_r;
  logic [31:0]    thr_r;
  logic [ 5:0]    bs_bit_r;
  logic [EIW-1:0] bs_count_r;
  logic [EIW-1:0] tie_left_r;

  logic [17:0] emit_e_r;
  logic        emit_live_r;
  logic        page_done_r;

  // ===========================================================================
  // combinational reads (the golden's shape, deliberately — see S3)
  // ===========================================================================
  logic [17:0] edge_rd_c;
  logic [16:0] run_rd_c;
  logic [31:0] prio_rd_c;
  assign edge_rd_c = {edge_key_r[idx_r[10:0]], edge_span_r[idx_r[10:0]]};
  assign run_rd_c  = run_mem_r[ridx_r[9:0]];
  assign prio_rd_c = prio_mem_r[idx_r[10:0]];

  // ---- S1: the rim-edge predicate, read from the three live rows -----------
  // Window row for page row cj is cj+1; window column for page column ci is
  // ci+1. north = window row cj, current = cj+1, south = cj+2.
  logic [WinAW-1:0] ci0_c, ci1_c, ci2_c;
  logic             self_solid_c, nb_solid_c, is_rim_c;
  always_comb begin
    ci0_c        = WinAW'(sc_ci_r);
    ci1_c        = WinAW'(sc_ci_r) + WinAW'(1);
    ci2_c        = WinAW'(sc_ci_r) + WinAW'(2);
    self_solid_c = row_c_r[ci1_c];
    // sides: 0 = -z, 1 = +z, 2 = -x, 3 = +x — the reference's `noff` table.
    case (sc_side_r)
      2'd0:    nb_solid_c = row_n_r[ci1_c];
      2'd1:    nb_solid_c = row_s_r[ci1_c];
      2'd2:    nb_solid_c = row_c_r[ci0_c];
      default: nb_solid_c = row_c_r[ci2_c];
    endcase
    is_rim_c = self_solid_c && !nb_solid_c;
  end

  // ---- S1: window RAM port control --------------------------------------------
  logic             row_start_c, row_end_c, rotate_c;
  logic             win_we_c, win_re_c;
  logic [WinAW-1:0] win_wa_c, win_ra_c;
  logic [RowW-1:0]  win_wd_c;
  logic [WinAW-1:0] pf_row_c;   // the row to prefetch: window row cj + 3
  logic             pf_ok_c;
  always_comb begin
    row_start_c = (st_r == StEnum) && (sc_ci_r == 6'd0) && (sc_side_r == 2'd0);
    row_end_c   = (st_r == StEnum) && (sc_side_r == 2'd3) &&
                  !(({1'b0, sc_ci_r} + 7'd1) < {1'b0, cw_r});
    pf_row_c    = WinAW'(sc_cj_r) + WinAW'(2);  // MUTANT: prefetch row cj+2 (was cj+3)
    pf_ok_c     = ({1'b0, sc_cj_r} + 7'd3) < 7'(WinDim);

    win_we_c = (st_r == StLoad) && ld_valid_i && (ld_wi_r == WinAW'(WinDim - 1));
    win_wa_c = ld_wj_r;
    win_wd_c = {ld_solid_i, ld_asm_r};

    if (st_r == StPrime) begin
      win_re_c = (prime_k_r < 3'd3);
      win_ra_c = WinAW'(prime_k_r);
    end else begin
      win_re_c = row_start_c && pf_ok_c;
      win_ra_c = pf_row_c;
    end
    // rotate north <- current <- south <- prefetch: at every row end, and on
    // prime cycles 1..3 — the read issued on cycle k lands in win_q_r during
    // cycle k+1, so rows 0, 1, 2 rotate in on cycles 1, 2, 3.
    rotate_c = row_end_c || ((st_r == StPrime) && (prime_k_r >= 3'd1));
  end

  always_ff @(posedge clk) begin
    if (win_we_c) win_mem[win_wa_c] <= win_wd_c;
    if (win_re_c) win_q_r <= win_mem[win_ra_c];
  end

  // ---- run adjacency (R3), between edge idx_r-1 (held) and edge idx_r -------
  logic [4:0] p_ci_c, p_cj_c, c_ci_c, c_cj_c, f_ci_c, f_cj_c;
  logic [1:0] c_side_c, f_side_c;
  logic [5:0] p_span_c;
  logic       contig_c;
  always_comb begin
    p_cj_c   = run_prev_r[17:13];
    p_ci_c   = run_prev_r[12:8];
    p_span_c = run_prev_r[5:0];
    c_cj_c   = edge_rd_c[17:13];
    c_ci_c   = edge_rd_c[12:8];
    c_side_c = edge_rd_c[7:6];
    f_cj_c   = run_first_r[17:13];
    f_ci_c   = run_first_r[12:8];
    f_side_c = run_first_r[7:6];
    if (c_side_c != f_side_c) begin
      contig_c = 1'b0;
    end else if (!f_side_c[1]) begin
      contig_c = (c_cj_c == f_cj_c) && ({1'b0, c_ci_c} == ({1'b0, p_ci_c} + p_span_c));
    end else begin
      contig_c = (c_ci_c == f_ci_c) && ({1'b0, c_cj_c} == ({1'b0, p_cj_c} + p_span_c));
    end
  end

  // ---- the vdist addresses for the edge at idx_r (F4) ----------------------
  logic [15:0] abs_ci_c, abs_cj_c;
  logic [ 5:0] cur_span_c;
  logic [ 1:0] cur_side_c;
  logic [31:0] vbase_c, vspan_c, va_c, vb_c;
  always_comb begin
    abs_ci_c   = pg_ci_r + {11'd0, edge_rd_c[12:8]};
    abs_cj_c   = pg_cj_r + {11'd0, edge_rd_c[17:13]};
    cur_side_c = edge_rd_c[7:6];
    cur_span_c = edge_rd_c[5:0];
    vbase_c    = ({16'd0, abs_cj_c} * {16'd0, lat_w_r}) + {16'd0, abs_ci_c};
    vspan_c    = {26'd0, cur_span_c} * {16'd0, lat_w_r};
    case (cur_side_c)
      2'd0: begin
        va_c = vbase_c;
        vb_c = vbase_c + {26'd0, cur_span_c};
      end
      2'd1: begin
        va_c = vbase_c + {16'd0, lat_w_r} + {26'd0, cur_span_c};
        vb_c = vbase_c + {16'd0, lat_w_r};
      end
      2'd2: begin
        va_c = vbase_c + vspan_c;
        vb_c = vbase_c;
      end
      default: begin
        va_c = vbase_c + 32'd1;
        vb_c = vbase_c + vspan_c + 32'd1;
      end
    endcase
  end

  logic [31:0] prio_max_c;
  assign prio_max_c = ($signed(vd_data_i) > $signed(pr_va_r)) ? vd_data_i : pr_va_r;
  logic [31:0] prio_key_c;
  logic [31:0] bs_cand_c;
  assign prio_key_c = prio_rd_c ^ 32'h8000_0000;
  assign bs_cand_c  = thr_r | (32'd1 << bs_bit_r);

  // the keep decision of the final pass (identical to the golden)
  logic keep_c, spend_c;
  always_comb begin
    if (!vden_r) begin
      spend_c = (tie_left_r != {EIW{1'b0}});
      keep_c  = spend_c;
    end else if (prio_key_c > thr_r) begin
      spend_c = 1'b0;
      keep_c  = 1'b1;
    end else if (prio_key_c == thr_r) begin
      spend_c = (tie_left_r != {EIW{1'b0}});
      keep_c  = spend_c;
    end else begin
      spend_c = 1'b0;
      keep_c  = 1'b0;
    end
  end

  // ---- S2: the span walk ------------------------------------------------------
  // A span of zero can never be read (every entry below cnt was written with
  // 1 or a take >= 2), but a walk that trusted it would never advance; so the
  // advance is at least one and the event is counted.
  logic           walk_done_c, walk_over_c, span_zero_c;
  logic [EIW-1:0] walk_adv_c, step_c;
  logic           compact_adv_c;   // StCompact: this cycle consumes the entry at idx_r
  logic           compact_copy_c;  // ... and it moves (wr != rd)
  logic           walk_sparse_c;   // a span walk is in progress in this state
  logic           walk_step_c;     // ... and idx_r advances by span this cycle
  logic           walk_fault_inc_c;
  logic           run_close_c;
  logic           over_c;
  always_comb begin
    walk_done_c   = (idx_r >= cnt_r);
    walk_over_c   = (idx_r > cnt_r);
    span_zero_c   = (cur_span_c == 6'd0);
    walk_adv_c    = span_zero_c ? EIW'(1) : {{(EIW - 6) {1'b0}}, cur_span_c};
    compact_adv_c = (st_r == StCompact) && !walk_done_c && (pr_ph_r == 2'd2);
    compact_copy_c = compact_adv_c && (wr_r != idx_r);
    walk_sparse_c = (st_r == StCompact) || (sparse_r && (st_r == StKeep || st_r == StEmit));
    // the cursor step: by span while sparse, by one on a dense table
    step_c = walk_sparse_c ? walk_adv_c : EIW'(1);
    walk_step_c = compact_adv_c ||
                  (sparse_r && st_r == StKeep && !walk_done_c) ||
                  (sparse_r && st_r == StEmit && emit_live_r && edge_ready_i);
    // walk_over_c can only be true on the single cycle a walk terminates
    // (idx_r is reset on leaving), so it is counted exactly once.
    walk_fault_inc_c = walk_sparse_c && ((walk_step_c && span_zero_c) || walk_over_c);
    // StRuns closes the run under construction at the table's end, or when
    // the current entry breaks contiguity (the golden's two write sites).
    run_close_c = (idx_r == cnt_r) ||
                  ((idx_r != {EIW{1'b0}}) && !(contig_c && rlen_r != 6'd63));
    over_c = (cnt_r - {{(EIW - 12) {1'b0}}, merged_r}) > EIW'(Budget);
  end

  // ---- S3: one write site per table, decoded from the phase ------------------
  logic        ek_we_c, es_we_c, pm_we_c, rm_we_c;
  logic [10:0] ek_wa_c, es_wa_c, pm_wa_c;
  logic [ 9:0] rm_wa_c;
  logic [11:0] ek_wd_c;
  logic [ 5:0] es_wd_c;
  logic [31:0] pm_wd_c;
  logic [16:0] rm_wd_c;
  always_comb begin
    ek_we_c = 1'b0;
    ek_wa_c = wr_r[10:0];
    ek_wd_c = edge_rd_c[17:6];
    es_we_c = 1'b0;
    es_wa_c = wr_r[10:0];
    es_wd_c = edge_rd_c[5:0];
    pm_we_c = 1'b0;
    pm_wa_c = wr_r[10:0];
    pm_wd_c = prio_max_c;
    rm_we_c = 1'b0;
    rm_wa_c = runs_r[9:0];
    rm_wd_c = {rstart_r, rlen_r};
    case (st_r)
      StEnum: begin
        if (is_rim_c && cnt_r != EIW'(MaxEdges)) begin
          ek_we_c = 1'b1;
          ek_wa_c = idx_r[10:0];
          ek_wd_c = {sc_cj_r[4:0], sc_ci_r[4:0], sc_side_r};
          es_we_c = 1'b1;
          es_wa_c = idx_r[10:0];
          es_wd_c = 6'd1;
        end
      end
      StRuns: begin
        rm_we_c = run_close_c && (rlen_r >= 6'd2) && (runs_r != RIW'(MaxRuns));
      end
      StMwrite: begin
        es_we_c = 1'b1;
        es_wa_c = mhead_r;
        es_wd_c = mtake_r;
      end
      StCompact: begin
        ek_we_c = compact_copy_c;
        es_we_c = compact_copy_c;
        pm_we_c = compact_adv_c;
      end
      StKeep: begin
        if (!walk_done_c && keep_c && (wr_r != idx_r)) begin
          ek_we_c = 1'b1;
          es_we_c = 1'b1;
        end
      end
      default: begin
      end
    endcase
  end

  always_ff @(posedge clk) begin
    if (ek_we_c) edge_key_r[ek_wa_c]  <= ek_wd_c;
    if (es_we_c) edge_span_r[es_wa_c] <= es_wd_c;
    if (pm_we_c) prio_mem_r[pm_wa_c]  <= pm_wd_c;
    if (rm_we_c) run_mem_r[rm_wa_c]   <= rm_wd_c;
  end

  // ===========================================================================
  // handshakes — every outgoing valid/ready is a function of registers only.
  // ===========================================================================
  assign cmd_ready_o    = (st_r == StIdle);
  assign ld_ready_o     = (st_r == StLoad);
  assign idle_o         = (st_r == StIdle);
  assign edge_valid_o   = (st_r == StEmit) && emit_live_r;
  assign edge_ci_o      = pg_ci_r + {11'd0, emit_e_r[12:8]};
  assign edge_cj_o      = pg_cj_r + {11'd0, emit_e_r[17:13]};
  assign edge_side_o    = emit_e_r[7:6];
  assign edge_span_o    = emit_e_r[5:0];
  assign edge_src_id_o  = src_r;
  assign page_done_o    = page_done_r;
  assign page_merged_o  = merged_q_r;
  assign page_dropped_o = dropped_q_r;
  assign vd_en_o        = (st_r == StCompact) && vden_r;
  assign vd_addr_o      = (pr_ph_r == 2'd0) ? va_c : vb_c;

  logic unused_ok;
  always_comb begin
    unused_ok = |c_cj_c | |run_first_r[5:0] | |run_prev_r[7:6];
    unused_ok = unused_ok & 1'b0;
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_r        <= StIdle;
      pg_ci_r     <= 16'd0;
      pg_cj_r     <= 16'd0;
      lat_w_r     <= 16'd0;
      src_r       <= 16'd0;
      cw_r        <= 6'd0;
      ch_r        <= 6'd0;
      vden_r      <= 1'b0;
      ld_asm_r    <= {(RowW - 1) {1'b0}};
      ld_wi_r     <= {WinAW{1'b0}};
      ld_wj_r     <= {WinAW{1'b0}};
      row_n_r     <= {RowW{1'b0}};
      row_c_r     <= {RowW{1'b0}};
      row_s_r     <= {RowW{1'b0}};
      prime_k_r   <= 3'd0;
      cnt_r       <= {EIW{1'b0}};
      runs_r      <= {RIW{1'b0}};
      sc_ci_r     <= 6'd0;
      sc_cj_r     <= 6'd0;
      sc_side_r   <= 2'd0;
      idx_r       <= {EIW{1'b0}};
      wr_r        <= {EIW{1'b0}};
      ridx_r      <= {RIW{1'b0}};
      mlen_r      <= 6'd0;
      need_r      <= {EIW{1'b0}};
      mhead_r     <= 11'd0;
      mtake_r     <= 6'd0;
      rlen_r      <= 6'd0;
      rstart_r    <= 11'd0;
      run_first_r <= 18'd0;
      run_prev_r  <= 18'd0;
      sparse_r    <= 1'b0;
      merged_r    <= 12'd0;
      dropped_r   <= 12'd0;
      merged_q_r  <= 12'd0;
      dropped_q_r <= 12'd0;
      pr_ph_r     <= 2'd0;
      pr_va_r     <= 32'd0;
      thr_r       <= 32'd0;
      bs_bit_r    <= 6'd0;
      bs_count_r  <= {EIW{1'b0}};
      tie_left_r  <= {EIW{1'b0}};
      emit_e_r    <= 18'd0;
      emit_live_r <= 1'b0;
      page_done_r <= 1'b0;
      triangles_submitted_o <= 32'd0;
      walk_fault_o <= {WalkFW{1'b0}};
    end else begin
      page_done_r <= 1'b0;

      // S2: the one instrument (see header). Saturating.
      if (walk_fault_inc_c && walk_fault_o != {WalkFW{1'b1}}) begin
        walk_fault_o <= walk_fault_o + {{(WalkFW - 1) {1'b0}}, 1'b1};
      end

      // S1: the row rotate is shared by StPrime and StEnum.
      if (rotate_c) begin
        row_n_r <= row_c_r;
        row_c_r <= row_s_r;
        row_s_r <= win_q_r;
      end

      case (st_r)
        // -------------------------------------------------------------------
        StIdle: begin
          if (cmd_valid_i) begin
            pg_ci_r   <= cmd_page_ci_i;
            pg_cj_r   <= cmd_page_cj_i;
            cw_r      <= cmd_cw_i;
            ch_r      <= cmd_ch_i;
            lat_w_r   <= cmd_lat_w_i;
            vden_r    <= cmd_vdist_en_i;
            src_r     <= cmd_src_id_i;
            ld_wi_r   <= {WinAW{1'b0}};
            ld_wj_r   <= {WinAW{1'b0}};
            cnt_r     <= {EIW{1'b0}};
            merged_r  <= 12'd0;
            dropped_r <= 12'd0;
            sparse_r  <= 1'b0;
            st_r      <= StLoad;
          end
        end

        // -------------------------------------------------------------------
        // S1: 1,156 bits in, 34 whole-row RAM writes out (win_we_c above).
        StLoad: begin
          if (ld_valid_i) begin
            ld_asm_r <= {ld_solid_i, ld_asm_r[RowW-2:1]};
            if (ld_wi_r == WinAW'(WinDim - 1)) begin
              ld_wi_r <= {WinAW{1'b0}};
              if (ld_wj_r == WinAW'(WinDim - 1)) begin
                ld_wj_r   <= {WinAW{1'b0}};
                prime_k_r <= 3'd0;
                sc_ci_r   <= 6'd0;
                sc_cj_r   <= 6'd0;
                sc_side_r <= 2'd0;
                idx_r     <= {EIW{1'b0}};
                st_r      <= StPrime;
              end else begin
                ld_wj_r <= ld_wj_r + {{(WinAW - 1) {1'b0}}, 1'b1};
              end
            end else begin
              ld_wi_r <= ld_wi_r + {{(WinAW - 1) {1'b0}}, 1'b1};
            end
          end
        end

        // -------------------------------------------------------------------
        // S1: read window rows 0, 1, 2 into north, current, south. Four cycles
        // per page: reads on cycles 0, 1, 2, each landing one cycle later and
        // rotating in on cycles 1, 2, 3 (rotate_c). Found by the differential
        // on its first run: rotating on 2..4 left north = page row 0 and every
        // row-0 north edge missing — the checker fired before any mutant did.
        StPrime: begin
          if (prime_k_r == 3'(PrimeLen - 1)) begin
            st_r <= StEnum;
          end else begin
            prime_k_r <= prime_k_r + 3'd1;
          end
        end

        // -------------------------------------------------------------------
        // F3: cj outer, ci inner, side 0..3 — one side per clock. The prefetch
        // read of row cj+3 is issued at the row's first cycle (win_re_c) and
        // rotated in at its last (rotate_c). The table write is ek/es_we_c.
        StEnum: begin
          if (is_rim_c && cnt_r != EIW'(MaxEdges)) begin
            idx_r <= idx_r + {{(EIW - 1) {1'b0}}, 1'b1};
            cnt_r <= cnt_r + {{(EIW - 1) {1'b0}}, 1'b1};
          end
          if (sc_side_r != 2'd3) begin
            sc_side_r <= sc_side_r + 2'd1;
          end else begin
            sc_side_r <= 2'd0;
            if (({1'b0, sc_ci_r} + 7'd1) < {1'b0, cw_r}) begin
              sc_ci_r <= sc_ci_r + 6'd1;
            end else begin
              sc_ci_r <= 6'd0;
              if (({1'b0, sc_cj_r} + 7'd1) < {1'b0, ch_r}) begin
                sc_cj_r <= sc_cj_r + 6'd1;
              end else begin
                st_r <= StAfterEnum;
              end
            end
          end
        end

        // -------------------------------------------------------------------
        StAfterEnum: begin
          idx_r       <= {EIW{1'b0}};
          ridx_r      <= {RIW{1'b0}};
          runs_r      <= {RIW{1'b0}};
          emit_live_r <= 1'b0;
          if (cnt_r > EIW'(Budget)) begin
            need_r <= cnt_r - EIW'(Budget);
            st_r   <= StRuns;
          end else begin
            st_r <= StEmit;
          end
        end

        // -------------------------------------------------------------------
        // ONE linear pass building every maximal run (the golden's theorem).
        // The run-table write itself is rm_we_c; only the cursors live here.
        StRuns: begin
          if (idx_r == cnt_r) begin
            if (rlen_r >= 6'd2 && runs_r != RIW'(MaxRuns)) begin
              runs_r <= runs_r + {{(RIW - 1) {1'b0}}, 1'b1};
            end
            mlen_r <= 6'd32;
            ridx_r <= {RIW{1'b0}};
            st_r   <= StMsel;
          end else if (idx_r == {EIW{1'b0}}) begin
            run_first_r <= edge_rd_c;
            run_prev_r  <= edge_rd_c;
            rstart_r    <= 11'd0;
            rlen_r      <= 6'd1;
            idx_r       <= idx_r + {{(EIW - 1) {1'b0}}, 1'b1};
          end else begin
            if (contig_c && rlen_r != 6'd63) begin
              run_prev_r <= edge_rd_c;
              rlen_r     <= rlen_r + 6'd1;
            end else begin
              if (rlen_r >= 6'd2 && runs_r != RIW'(MaxRuns)) begin
                runs_r <= runs_r + {{(RIW - 1) {1'b0}}, 1'b1};
              end
              run_first_r <= edge_rd_c;
              run_prev_r  <= edge_rd_c;
              rstart_r    <= idx_r[10:0];
              rlen_r      <= 6'd1;
            end
            idx_r <= idx_r + {{(EIW - 1) {1'b0}}, 1'b1};
          end
        end

        // -------------------------------------------------------------------
        // Descending length, ties by ascending start (the counting sort).
        StMsel: begin
          if (need_r == {EIW{1'b0}} || mlen_r < 6'd2) begin
            // The merge phase is over. S2: the table is sparse iff something
            // merged; only the threshold passes need it dense, so StCompact
            // runs only on the way to them. Otherwise StKeep / StEmit walk
            // the sparse table by span at the golden's own cost or better.
            idx_r    <= {EIW{1'b0}};
            wr_r     <= {EIW{1'b0}};
            pr_ph_r  <= 2'd0;
            sparse_r <= (merged_r != 12'd0);
            if (over_c && vden_r) begin
              st_r <= StCompact;
            end else if (over_c) begin
              thr_r      <= 32'd0;
              tie_left_r <= EIW'(Budget);
              st_r       <= StKeep;
            end else begin
              emit_live_r <= 1'b0;
              st_r        <= StEmit;
            end
          end else if (ridx_r == runs_r) begin
            mlen_r <= mlen_r - 6'd1;
            ridx_r <= {RIW{1'b0}};
          end else if (run_rd_c[5:0] == mlen_r) begin
            mhead_r <= run_rd_c[16:6];
            // R1: shed the MINIMUM — take = min(len, need + 1).
            mtake_r <= ({{(EIW - 6) {1'b0}}, mlen_r} > (need_r + {{(EIW - 1) {1'b0}}, 1'b1}))
                       ? (need_r[5:0] + 6'd1) : mlen_r;
            ridx_r  <= ridx_r + {{(RIW - 1) {1'b0}}, 1'b1};
            st_r    <= StMwrite;
          end else begin
            ridx_r <= ridx_r + {{(RIW - 1) {1'b0}}, 1'b1};
          end
        end

        // -------------------------------------------------------------------
        // S2: the merge is ONE write — span[head] <= take (es_we_c). The
        // take-1 interior entries need no clearing: the span now says they
        // are dead, and the compaction walk steps over them.
        StMwrite: begin
          merged_r <= merged_r + {6'd0, mtake_r} - 12'd1;
          need_r   <= need_r - ({{(EIW - 6) {1'b0}}, mtake_r} - {{(EIW - 1) {1'b0}}, 1'b1});
          st_r     <= StMsel;
        end

        // -------------------------------------------------------------------
        // S2: the span-walk compaction, fused with the priority build (C2).
        // Entered only when a priority degrade follows (vdist on, still over
        // budget). rd = idx_r walks live entries (rd += span); wr trails it.
        // Three clocks per live entry exactly as the golden's StPrio: issue
        // va, capture va and issue vb, capture vb and store the max at wr.
        // The copies and the priority write are ek/es/pm_we_c.
        StCompact: begin
          if (walk_done_c) begin
            cnt_r      <= wr_r;   // dense from here on
            sparse_r   <= 1'b0;
            idx_r      <= {EIW{1'b0}};
            wr_r       <= {EIW{1'b0}};
            pr_ph_r    <= 2'd0;
            thr_r      <= 32'd0;
            bs_bit_r   <= 6'd31;
            bs_count_r <= {EIW{1'b0}};
            st_r       <= StBsCount;
          end else if (pr_ph_r == 2'd0) begin
            pr_ph_r <= 2'd1;
          end else if (pr_ph_r == 2'd1) begin
            pr_va_r <= vd_data_i;
            pr_ph_r <= 2'd2;
          end else begin
            // compact_adv_c: consume the entry at rd.
            pr_ph_r <= 2'd0;
            wr_r    <= wr_r + {{(EIW - 1) {1'b0}}, 1'b1};
            idx_r   <= idx_r + walk_adv_c;
          end
        end

        // -------------------------------------------------------------------
        // count(key >= thr | 1<<bit) over the DENSE table — no liveness test.
        StBsCount: begin
          if (idx_r == cnt_r) begin
            st_r <= StBsStep;
          end else begin
            if (prio_key_c >= bs_cand_c) begin
              bs_count_r <= bs_count_r + {{(EIW - 1) {1'b0}}, 1'b1};
            end
            idx_r <= idx_r + {{(EIW - 1) {1'b0}}, 1'b1};
          end
        end

        StBsStep: begin
          if (bs_count_r >= EIW'(Budget)) thr_r <= bs_cand_c;
          bs_count_r <= {EIW{1'b0}};
          idx_r      <= {EIW{1'b0}};
          if (bs_bit_r == 6'd0) begin
            st_r <= StGtCount;
          end else begin
            bs_bit_r <= bs_bit_r - 6'd1;
            st_r     <= StBsCount;
          end
        end

        // -------------------------------------------------------------------
        StGtCount: begin
          if (idx_r == cnt_r) begin
            tie_left_r <= EIW'(Budget) - bs_count_r;
            idx_r      <= {EIW{1'b0}};
            wr_r       <= {EIW{1'b0}};
            st_r       <= StKeep;
          end else begin
            if (prio_key_c > thr_r) begin
              bs_count_r <= bs_count_r + {{(EIW - 1) {1'b0}}, 1'b1};
            end
            idx_r <= idx_r + {{(EIW - 1) {1'b0}}, 1'b1};
          end
        end

        // -------------------------------------------------------------------
        // One pass in SCAN ORDER — by span while the table is sparse (vdist
        // off, so no StCompact ran), by one when dense: a kept entry slides
        // down to wr (ek/es_we_c, suppressed while wr == rd); a dropped one
        // adds its whole BODY count (R2). cnt' <- wr at the end, dense.
        StKeep: begin
          if (walk_done_c) begin
            cnt_r       <= wr_r;
            sparse_r    <= 1'b0;
            idx_r       <= {EIW{1'b0}};
            emit_live_r <= 1'b0;
            st_r        <= StEmit;
          end else begin
            if (keep_c) begin
              if (spend_c) tie_left_r <= tie_left_r - {{(EIW - 1) {1'b0}}, 1'b1};
              wr_r <= wr_r + {{(EIW - 1) {1'b0}}, 1'b1};
            end else begin
              dropped_r <= dropped_r + {6'd0, cur_span_c};
            end
            idx_r <= idx_r + step_c;
          end
        end

        // -------------------------------------------------------------------
        // Every live entry below cnt_r is emitted, in order: by span while
        // sparse (the merge alone brought the page inside budget), by one on
        // a dense table. edge_rd_c still addresses the emitted entry while
        // emit_live_r, so its span is the step.
        StEmit: begin
          if (emit_live_r) begin
            if (edge_ready_i) begin
              emit_live_r <= 1'b0;
              if (triangles_submitted_o < (CntMax - 32'd1)) begin
                triangles_submitted_o <= triangles_submitted_o + 32'd2;  // C3
              end
              idx_r <= idx_r + step_c;
            end
          end else if (walk_done_c) begin
            merged_q_r  <= merged_r;
            dropped_q_r <= dropped_r;
            page_done_r <= 1'b1;
            st_r        <= StIdle;
          end else begin
            emit_e_r    <= edge_rd_c;
            emit_live_r <= 1'b1;
          end
        end

        default: st_r <= StIdle;
      endcase
    end
  end

`ifndef SYNTHESIS
  // Simulation-only invariants (Verilator: --assert). Both are the memory
  // sheet's collision rule and S2's theorem made visible; neither is a
  // substitute for walk_fault_o, which is the instrument that ships.
  logic armed_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) armed_q <= 1'b0;
    else        armed_q <= 1'b1;
  end
  always_ff @(posedge clk) begin
    if (armed_q) begin
      // No table is ever written at the address being read in the same
      // compaction cycle — the suppression that keeps the in-place walk legal
      // for a Simple Dual Port RAM.
      if (st_r == StCompact || st_r == StKeep) begin
        a_compact_no_same_addr : assert (!(ek_we_c && ek_wa_c == idx_r[10:0]) &&
                                         !(es_we_c && es_wa_c == idx_r[10:0]));
        a_compact_wr_le_rd : assert (wr_r <= idx_r);
      end
      // Every span walk lands exactly on cnt_r and never reads a zero span.
      if (walk_sparse_c) begin
        a_walk_exact : assert (!walk_over_c);
        a_span_nonzero : assert (walk_done_c || !span_zero_c);
      end
      // The window RAM is never written and read in the same cycle.
      a_win_no_collision : assert (!(win_we_c && win_re_c));
    end
  end
`endif

endmodule
