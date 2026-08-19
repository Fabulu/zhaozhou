// zhao_forge_cliff.sv — FORGE.CLIFF: the rim plan of spec/terrain_rules.md 5,
// including BOTH steps of the frozen degrade order (phase 6, ZH-067).
//
// Law, in citation order:
//   design/contracts/FORGE.CLIFF.md — the block contract.
//   design/blocks.yml — `inputs: [terrain_mesh]`, `outputs: [forge_primitives]`,
//       `upstream: [TERRAIN.TESS]`, `downstream: [GEOM.SETUP]`,
//       `backpressure: ready_valid`, `latency: variable`, "1 skirt vertex per
//       clock", counter `triangles_submitted`, `source_ids: true`, maturity
//       REFERENCE_COMPLETE, `reference_model: zref::forge::rim_plan`, and the
//       note "the TIGHT checkerboard worst case is 2,048 rim edges (2,112
//       counts all adjacency edges, 64 with void owners)".
//   spec/terrain_rules.md 5 — the rim-edge definition, the per-page budget
//       (provisional 512) and the FROZEN degrade order, quoted below.
//   reference/src/zterrain/terrain_core.cpp `zref::forge::rim_plan`,
//       `is_rim_edge`, `build_runs` — THE law. This block is
//       REFERENCE_COMPLETE, so the oracle is not a guide, it is the answer.
//   reference/include/zref/zref_terrain.hpp — `RimEdge`, `RimPlan`,
//       `kRimBudgetPerPage`, and the doc comment stating the degrade order.
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK IS, AND WHAT IT IS NOT — stated before anything else
// ---------------------------------------------------------------------------
// The ledger's `reference_model` for FORGE.CLIFF is `zref::forge::rim_plan`,
// which answers exactly one question: WHICH RIM EDGES GET A WALL, and with
// what span. This block is that function, complete — enumeration, the merge
// degrade and the priority degrade, all three bit-for-bit.
//
// It does NOT turn a rim edge into wall VERTICES. That second half needs three
// things this block's ledger inputs do not give it: the composed top lattice
// "exactly as emitted by the tessellator's stitched edge set at the owning
// subpatch's LOD level" (5), the bottom lattice of the same two vertices, and
// the accumulated rim length that drives the strata U (6.6). The first is
// TERRAIN.TESS's stitched output, the second is TERRAIN.PATCH's layer C, and
// neither is a port this block has. So the emission stage is NOT WRITTEN, and
// design/contracts/FORGE.CLIFF.md says so in its own words rather than leaving
// a reader to infer it from a missing test. What lands here is the frozen,
// reference-complete half, and it is complete.
//
// ---------------------------------------------------------------------------
// THE LAW, QUOTED
// ---------------------------------------------------------------------------
// spec/terrain_rules.md 5, verbatim:
//
//   "Rim edge = a lattice edge between a SOLID cell and a void/OUT neighbour
//    (4-neighbourhood, axis-aligned in v1)."
//
//   "FORGE.CLIFF must clamp emission to a declared per-patch budget
//    (provisional 512 quads) and degrade by merging collinear spans...
//    Degrade order (frozen 2026-08-16, reference-tested): (1) merge
//    CONTIGUOUS collinear rim edges (same side, same lattice line, sharing a
//    vertex) into single spans until inside budget - a merge never bridges a
//    notch, so the silhouette keeps every hole; (2) if still over budget, keep
//    the edges with the greatest max-vertex 1/w (nearest the camera; ties by
//    scan order), count the dropped edges in the clamp counter."
//
// ---------------------------------------------------------------------------
// THREE THINGS THE REFERENCE DOES THAT A REIMPLEMENTATION GETS WRONG
// ---------------------------------------------------------------------------
// R1. A MERGE SHEDS THE MINIMUM, NOT WHOLE RUNS. `take = best_len; if (take -
//     1 > need) take = need + 1;` — the last merge takes a PREFIX of its run.
//     tests/forge/forge_cliff_directed.cpp pins it: two 20-edge runs under a
//     need of 31 give one 20-span and one 13-span, not two 20-spans.
// R2. `dropped` COUNTS BODIES, NOT ENTRIES. A dropped merged span takes its
//     whole span with it (`plan.dropped += es[order[k]].e.span`), which is what
//     makes `emitted_bodies + dropped == enumerated` an identity — the one
//     tests/forge/forge_cliff_random.cpp checks on every random mask.
// R3. A RUN IS CONSECUTIVE IN THE EMITTED ARRAY, NOT MERELY COLLINEAR IN THE
//     LATTICE. `build_runs` compares es[j] with es[j-1] and breaks on a side
//     change, so two same-side edges separated in scan order by an edge of
//     another side are NOT a run. In the (cj, ci, side) scan order that means a
//     run forms only where each cell along the line contributes EXACTLY ONE
//     edge, of the same side — a straight wall. It is why the 32x32
//     checkerboard, whose every solid cell contributes four edges, merges
//     NOTHING at all.
//
// ---------------------------------------------------------------------------
// THE MERGE LOOP, AND WHY ONE PASS REPLACES ITS QUADRATIC RESCAN
// ---------------------------------------------------------------------------
// The reference rebuilds EVERY run from scratch on every iteration:
//     while (need > 0) { build_runs(...); pick the strictly longest, ties
//                        earliest; merge a prefix of it; }
// With `need` up to 1,536 and each `build_runs` linear in 2,048 entries that is
// ~3 M steps per page — twice a 1.67 M-clock frame, for one page.
//
// It is also unnecessary, and the reason is a small theorem worth writing down,
// because the equivalence is the entire basis of this block's control:
//
//   THE SET OF MAXIMAL RUNS DOES NOT CHANGE WHEN ONE OF THEM IS MERGED.
//   Merging the run at index b with `take` entries kills b+1 .. b+take-1 and
//   sets es[b].span = take. Those dead entries lie strictly INSIDE a run that
//   was already maximal, so they break no adjacency that was not already
//   broken. The head gains no new neighbour either: for the entry BEFORE b the
//   contiguity test reads `prev.ci + prev.span` of the PREVIOUS entry, which is
//   unchanged; for the entry after the run's end the test becomes
//   `cur.ci == es[b].ci + take`, numerically identical to the pre-merge test
//   `cur.ci == es[b+take-1].ci + 1` that maximality already failed. And a
//   PARTIAL merge (take < len) can only happen on the LAST iteration, because
//   it drives `need` to zero.
//
// So the runs are built ONCE, and the loop becomes "process runs in descending
// length, ties by ascending start" — done here by walking the length from 32
// down to 2 and scanning the run table in order at each length. That is a
// counting sort with 31 buckets, O(31 x runs) instead of O(need x edges), and
// it selects the same run in the same order every time.
//
// ---------------------------------------------------------------------------
// THE PRIORITY DEGRADE IS A THRESHOLD SEARCH, NOT A SORT
// ---------------------------------------------------------------------------
// The reference `stable_sort`s up to 2,048 entries by a 32-bit key descending
// and keeps a prefix of `Budget`. The SET it keeps is determined by two numbers:
// the cut key T and how many of the edges tied at T fit. So:
//
//   thr = 0; for b = 31 downto 0: if count(live && key >= thr|1<<b) >= Budget
//                                    then thr |= 1<<b;
//
// leaves `thr` the largest key with count(key >= thr) >= Budget, because that
// count is monotone non-increasing in the key. Then one pass in SCAN ORDER
// keeps every edge above `thr` and the first (Budget - count(key > thr)) edges
// equal to it — which is exactly a stable descending sort followed by a prefix.
// 33 linear passes over one RAM port and one comparator, against 66 stages of
// 1,024 compare-exchanges for a bitonic sorter of the same width.
//
// The key is the priority BIASED by 2^31 so a single unsigned comparator spans
// the signed range; vdist is a Q16.16 1/w and the reference compares it signed.
//
// ---------------------------------------------------------------------------
// LAWS FOUND (not invented)
// ---------------------------------------------------------------------------
// F1. OUT AND VOID ARE THE SAME THING TO THE PREDICATE. `is_rim_edge` answers
//     TRUE for a neighbour off the cell grid and TRUE for a neighbour that is
//     not kSolid, and the two branches are never distinguished afterwards. So
//     ONE BIT PER CELL — "is this cell SOLID" — is a FAITHFUL encoding of the
//     predicate's whole input, which is why the load window below is a bitmap
//     and not a substance plane, and why a cell outside the lattice loads as 0
//     and is exactly right.
//     ENFORCED-BY: tests/forge/forge_cliff_directed.cpp:test_rtl_enumeration
// F2. THE BUDGET AND THE DEGRADE ARE PER PAGE, AND A PAGE IS 32x32 CELLS.
//     `rim_plan` loops `pj`/`pi` in steps of 32 and builds a fresh working set
//     inside each. A PAGE is therefore the unit of this block's law, and the
//     block processes one page per command; the whole-patch loop belongs to
//     whoever issues the commands.
// F3. THE SCAN ORDER IS cj OUTER, ci INNER, side 0..3, AND IT IS LOAD-BEARING
//     TWICE: it decides which edges are adjacent for R3's run test, and it is
//     the tie-break for the priority degrade.
// F4. THE PRIORITY IS THE GREATER OF THE EDGE'S TWO ENDPOINT vdist VALUES, with
//     the renderer's own side -> vertex map (sides 0/1 run along +x, 2/3 along
//     +z), spans included. A NULL vdist means prio 0 EVERYWHERE, which under a
//     stable sort is exactly "keep scan order" — so this block takes a
//     `vdist_en` bit and skips the whole priority machine when it is low,
//     rather than reading a table of zeros.
//     ENFORCED-BY: tests/forge/forge_cliff_directed.cpp:test_rtl_priority
// F5. THE BUDGET IS 512 (`zref::forge::kRimBudgetPerPage`) and it is a constant
//     of the law rather than a knob: the committed directed tests assert 512
//     survivors and 1,536 drops on the checkerboard page.
//
// ---------------------------------------------------------------------------
// LAWS CHOSEN (no spec states these; recorded with the rejected alternative)
// ---------------------------------------------------------------------------
// C1. THE CELL SUBSTANCE ARRIVES AS A 34x34 SOLID BITMAP — the page plus a
//     ONE-CELL HALO — streamed row-major at the start of a page. F1 is what
//     makes one bit faithful, and the halo is what makes the predicate need no
//     second memory port: every neighbour of a page cell is inside the window.
//     REJECTED ALTERNATIVE: a read port into TERRAIN.PATCH's layer-D plane,
//     queried four times per cell. It is 16,384 reads per page against 1,156,
//     it puts this block in contention with TERRAIN.PATCH's own consumers, and
//     it makes this block's timing depend on a memory it does not own.
// C2. THE PRIORITY IS COMPUTED ONCE PER EDGE AND STORED. The threshold search
//     is 32 counting passes and each must compare every live edge's priority;
//     recomputing costs two vdist reads per edge per pass (32 x 2,048 x 2 =
//     131 k reads per degraded page) against 4,096 reads once. The price is a
//     2,048 x 32-bit table, about 7 M10K.
//     REJECTED ALTERNATIVE: recomputing per pass. It saves the M10Ks and spends
//     the cycles in the one path that is already the pathological one.
// C3. `triangles_submitted` COUNTS TWO PER EMITTED EDGE. Section 5 says "one
//     quad per rim edge" and "the governor sees wall quads as ordinary
//     triangles_submitted"; a quad is two triangles.
//     REJECTED ALTERNATIVE: counting edges, which would make this block's
//     contribution to a shared counter mean something different from every
//     other block's contribution to it.
// C4. `page_merged_o` AND `page_dropped_o` ARE STATUS OUTPUTS, NOT COUNTERS.
//     They are `RimPlan.merged` and `RimPlan.dropped` for the page just
//     emitted, held from `page_done_o` until the next page starts. The ledger
//     gives this block one counter and the counter catalog is a frozen index
//     space (spec/counters.md 2), so minting two more counter ids here would be
//     a ledger edit dressed as an RTL edit.
// C5. ONE PAGE IN FLIGHT. `cmd_ready_o` is `st == StIdle`, so a page is
//     enumerated, degraded and emitted before the next is accepted. The working
//     set is 2,048 edges; a second in flight would double every table.
//     REJECTED ALTERNATIVE: double-buffering the edge table to overlap emission
//     with the next page's enumeration. It doubles ~13 M10K to hide a load and
//     an enumeration that together are ~5 k cycles against a page whose walls
//     the rasterizer will spend far longer drawing.
//
// Conservative SystemVerilog subset only (charter 2). Lint: clean under
// `-Wall` (lint_forge_cliff).

module zhao_forge_cliff (
    input logic clk,
    input logic rst_n,

    // -----------------------------------------------------------------------
    // one PAGE of the lattice (F2). The issuer walks the pages; this block owns
    // the law inside one.
    // -----------------------------------------------------------------------
    input  logic        cmd_valid_i,
    output logic        cmd_ready_o,
    input  logic [15:0] cmd_page_ci_i,   // absolute cell index of page cell (0,0)
    input  logic [15:0] cmd_page_cj_i,
    input  logic [ 5:0] cmd_cw_i,        // cells in this page on x, 1..32
    input  logic [ 5:0] cmd_ch_i,        // cells in this page on z, 1..32
    input  logic [15:0] cmd_lat_w_i,     // lattice VERTEX width (the vdist stride)
    input  logic        cmd_vdist_en_i,  // 0 = the reference's null vdist (F4)
    input  logic [15:0] cmd_src_id_i,

    // -----------------------------------------------------------------------
    // the SOLID window (C1): 34*34 bits, row-major, window cell (0,0) is page
    // cell (-1,-1). Anything outside the lattice loads as 0 (F1).
    // -----------------------------------------------------------------------
    input  logic ld_valid_i,
    output logic ld_ready_o,
    input  logic ld_solid_i,

    // -----------------------------------------------------------------------
    // the vdist read master: one Q16.16 1/w per lattice VERTEX, synchronous,
    // data valid the cycle after the address. Never read when vdist_en is low.
    // -----------------------------------------------------------------------
    output logic        vd_en_o,
    output logic [31:0] vd_addr_o,
    input  logic [31:0] vd_data_i,

    // -----------------------------------------------------------------------
    // forge_primitives out — one rim edge, i.e. one wall quad (5).
    // -----------------------------------------------------------------------
    output logic        edge_valid_o,
    input  logic        edge_ready_i,
    output logic [15:0] edge_ci_o,     // ABSOLUTE, so it IS zref::forge::RimEdge
    output logic [15:0] edge_cj_o,
    output logic [ 1:0] edge_side_o,   // 0 = -z, 1 = +z, 2 = -x, 3 = +x
    output logic [ 5:0] edge_span_o,
    output logic [15:0] edge_src_id_o,

    // -----------------------------------------------------------------------
    // the page's plan status (C4): RimPlan.merged / RimPlan.dropped.
    // -----------------------------------------------------------------------
    output logic        page_done_o,  // 1-cycle pulse: the page finished emitting
    output logic [11:0] page_merged_o,
    output logic [11:0] page_dropped_o,

    output logic        idle_o,
    output logic [31:0] triangles_submitted_o
);

  // F5: the budget is a constant of the law (zref::forge::kRimBudgetPerPage).
  localparam int unsigned Budget = 512;
  // The TIGHT worst case: a 32x32 checkerboard is 512 solid cells x 4 sides.
  localparam int unsigned MaxEdges = 2048;
  localparam int unsigned EIW = 12;  // holds 0..2048 inclusive
  // A run needs at least two entries, so there can be at most MaxEdges/2.
  localparam int unsigned MaxRuns = 1024;
  localparam int unsigned RIW = 11;
  // The load window is the page plus a one-cell halo (C1).
  localparam int unsigned WinDim = 34;
  localparam int unsigned WinCells = WinDim * WinDim;  // 1156

  localparam logic [31:0] CntMax = 32'hFFFF_FFFF;

  localparam logic [3:0] StIdle = 4'd0;
  localparam logic [3:0] StLoad = 4'd1;
  localparam logic [3:0] StEnum = 4'd2;
  localparam logic [3:0] StAfterEnum = 4'd3;
  localparam logic [3:0] StRuns = 4'd4;
  localparam logic [3:0] StMsel = 4'd5;
  localparam logic [3:0] StMdead = 4'd6;
  localparam logic [3:0] StPrio = 4'd7;
  localparam logic [3:0] StBsCount = 4'd8;
  localparam logic [3:0] StBsStep = 4'd9;
  localparam logic [3:0] StGtCount = 4'd10;
  localparam logic [3:0] StKeep = 4'd11;
  localparam logic [3:0] StEmit = 4'd12;

  // ===========================================================================
  // state
  // ===========================================================================
  logic [3:0] st_r;

  logic [15:0] pg_ci_r, pg_cj_r, lat_w_r, src_r;
  logic [ 5:0] cw_r, ch_r;
  logic        vden_r;

  logic [WinCells-1:0] solid_r;
  logic [ 10:0]        ld_cnt_r;

  // the edge table: {cj[4:0], ci[4:0], side[1:0], span[5:0]} — page-local
  // indices; the page origin is added at emit time so the OUTPUT is exactly
  // zref::forge::RimEdge.
  logic [ 17:0]        edge_mem_r  [0:MaxEdges-1];
  logic [MaxEdges-1:0] alive_r;
  logic [ 31:0]        prio_mem_r  [0:MaxEdges-1];
  logic [EIW-1:0]      cnt_r;

  // the run table: {start[10:0], len[5:0]}
  logic [16:0]    run_mem_r[0:MaxRuns-1];
  logic [RIW-1:0] runs_r;

  logic [5:0]     sc_ci_r, sc_cj_r;
  logic [1:0]     sc_side_r;
  logic [EIW-1:0] idx_r;
  logic [RIW-1:0] ridx_r;

  logic [ 5:0]    mlen_r;
  logic [EIW-1:0] need_r;
  logic [10:0]    mhead_r;
  logic [ 5:0]    mtake_r;
  logic [ 5:0]    mstep_r;
  logic [ 5:0]    rlen_r;      // the run under construction
  logic [10:0]    rstart_r;
  logic [17:0]    run_first_r;
  logic [17:0]    run_prev_r;

  logic [11:0]    merged_r, dropped_r;
  logic [11:0]    merged_q_r, dropped_q_r;  // held for the consumer (C4)

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
  // combinational reads and the two predicates
  // ===========================================================================
  logic [17:0] edge_rd_c;
  logic [16:0] run_rd_c;
  logic [31:0] prio_rd_c;
  assign edge_rd_c = edge_mem_r[idx_r[10:0]];
  assign run_rd_c  = run_mem_r[ridx_r[9:0]];
  assign prio_rd_c = prio_mem_r[idx_r[10:0]];

  // ---- the rim-edge predicate (F1), read from the window --------------------
  // The window index of page cell (ci, cj) is (cj + 1) * 34 + (ci + 1); the
  // four neighbours sit at fixed offsets, so one base index serves all five.
  logic [10:0] win_base_c;
  logic        self_solid_c, nb_solid_c, is_rim_c;
  always_comb begin
    win_base_c   = (11'({1'b0, sc_cj_r}) + 11'd1) * 11'(WinDim) + (11'({1'b0, sc_ci_r}) + 11'd1);
    self_solid_c = solid_r[win_base_c];
    // sides: 0 = -z, 1 = +z, 2 = -x, 3 = +x — the reference's `noff` table.
    case (sc_side_r)
      2'd0:    nb_solid_c = solid_r[win_base_c - 11'(WinDim)];
      2'd1:    nb_solid_c = solid_r[win_base_c + 11'(WinDim)];
      2'd2:    nb_solid_c = solid_r[win_base_c - 11'd1];
      default: nb_solid_c = solid_r[win_base_c + 11'd1];
    endcase
    is_rim_c = self_solid_c && !nb_solid_c;
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
    // build_runs: the same side as the run's FIRST edge, and contiguous with
    // the PREVIOUS edge's span. Sides 0/1 advance ci, sides 2/3 advance cj.
    if (c_side_c != f_side_c) begin
      contig_c = 1'b0;
    end else if (!f_side_c[1]) begin
      contig_c = (c_cj_c == f_cj_c) && ({1'b0, c_ci_c} == ({1'b0, p_ci_c} + p_span_c));
    end else begin
      contig_c = (c_ci_c == f_ci_c) && ({1'b0, c_cj_c} == ({1'b0, p_cj_c} + p_span_c));
    end
  end

  // ---- the vdist addresses for the edge at idx_r (F4) ----------------------
  // base = cj * lat_w + ci with ABSOLUTE cell indices, exactly as the reference
  // computes it; the side -> vertex map is the renderer's own.
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

  // `int64_t p = vdist[va]; if (vdist[vb] > p) p = vdist[vb];` on two int32s IS
  // a signed max.
  logic [31:0] prio_max_c;
  assign prio_max_c = ($signed(vd_data_i) > $signed(pr_va_r)) ? vd_data_i : pr_va_r;
  // The search runs on the BIASED key so one unsigned comparator spans the
  // signed range.
  logic [31:0] prio_key_c;
  logic [31:0] bs_cand_c;
  assign prio_key_c = prio_rd_c ^ 32'h8000_0000;
  assign bs_cand_c  = thr_r | (32'd1 << bs_bit_r);

  // The keep decision of the final pass, as one expression. `vden_r` low is
  // the reference's null-vdist path, where every priority is 0 and a stable
  // sort therefore keeps the first `Budget` live edges in scan order.
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

  // ===========================================================================
  // handshakes. Hygiene: every outgoing valid/ready is a function of registers
  // only, never of an incoming ready.
  // ===========================================================================
  assign cmd_ready_o   = (st_r == StIdle);
  assign ld_ready_o    = (st_r == StLoad);
  assign idle_o        = (st_r == StIdle);
  assign edge_valid_o  = (st_r == StEmit) && emit_live_r;
  assign edge_ci_o     = pg_ci_r + {11'd0, emit_e_r[12:8]};
  assign edge_cj_o     = pg_cj_r + {11'd0, emit_e_r[17:13]};
  assign edge_side_o   = emit_e_r[7:6];
  assign edge_span_o   = emit_e_r[5:0];
  assign edge_src_id_o = src_r;
  assign page_done_o   = page_done_r;
  assign page_merged_o = merged_q_r;
  assign page_dropped_o = dropped_q_r;
  assign vd_en_o       = (st_r == StPrio);
  assign vd_addr_o     = (pr_ph_r == 2'd0) ? va_c : vb_c;

  // The window's four halo rows/columns are loaded but only read through
  // `win_base_c`'s neighbour offsets; nothing else consumes them.
  logic unused_ok;
  always_comb begin
    unused_ok = |cur_span_c | |c_cj_c | |run_first_r[5:0] | |run_prev_r[7:6];
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
      solid_r     <= {WinCells{1'b0}};
      ld_cnt_r    <= 11'd0;
      alive_r     <= {MaxEdges{1'b0}};
      cnt_r       <= {EIW{1'b0}};
      runs_r      <= {RIW{1'b0}};
      sc_ci_r     <= 6'd0;
      sc_cj_r     <= 6'd0;
      sc_side_r   <= 2'd0;
      idx_r       <= {EIW{1'b0}};
      ridx_r      <= {RIW{1'b0}};
      mlen_r      <= 6'd0;
      need_r      <= {EIW{1'b0}};
      mhead_r     <= 11'd0;
      mtake_r     <= 6'd0;
      mstep_r     <= 6'd0;
      rlen_r      <= 6'd0;
      rstart_r    <= 11'd0;
      run_first_r <= 18'd0;
      run_prev_r  <= 18'd0;
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
    end else begin
      page_done_r <= 1'b0;

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
            ld_cnt_r  <= 11'd0;
            cnt_r     <= {EIW{1'b0}};
            merged_r  <= 12'd0;
            dropped_r <= 12'd0;
            alive_r   <= {MaxEdges{1'b0}};
            st_r      <= StLoad;
          end
        end

        // -------------------------------------------------------------------
        StLoad: begin
          if (ld_valid_i) begin
            solid_r[ld_cnt_r] <= ld_solid_i;
            if (ld_cnt_r == 11'(WinCells - 1)) begin
              ld_cnt_r  <= 11'd0;
              sc_ci_r   <= 6'd0;
              sc_cj_r   <= 6'd0;
              sc_side_r <= 2'd0;
              // StEnum uses `idx_r` as its WRITE pointer and every later phase
              // uses it as a read cursor, so it has to start at zero for each
              // page. Leaving it where the previous page's emission stopped
              // writes a page's edges above its own `cnt_r` and the page then
              // emits nothing at all — which is exactly what the second page
              // of the 96x96 fixture did before this line existed.
              idx_r     <= {EIW{1'b0}};
              st_r      <= StEnum;
            end else begin
              ld_cnt_r <= ld_cnt_r + 11'd1;
            end
          end
        end

        // -------------------------------------------------------------------
        // F3: cj outer, ci inner, side 0..3 — one side per clock.
        StEnum: begin
          if (is_rim_c && cnt_r != EIW'(MaxEdges)) begin
            edge_mem_r[idx_r[10:0]] <= {sc_cj_r[4:0], sc_ci_r[4:0], sc_side_r, 6'd1};
            alive_r[idx_r[10:0]] <= 1'b1;
            idx_r             <= idx_r + {{(EIW - 1) {1'b0}}, 1'b1};
            cnt_r             <= cnt_r + {{(EIW - 1) {1'b0}}, 1'b1};
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
        // cnt_r has settled; decide whether the page needs degrading.
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
        // ONE linear pass building every maximal run (see THE MERGE LOOP).
        StRuns: begin
          if (idx_r == cnt_r) begin
            if (rlen_r >= 6'd2 && runs_r != RIW'(MaxRuns)) begin
              run_mem_r[runs_r[9:0]] <= {rstart_r, rlen_r};
              runs_r            <= runs_r + {{(RIW - 1) {1'b0}}, 1'b1};
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
                run_mem_r[runs_r[9:0]] <= {rstart_r, rlen_r};
                runs_r            <= runs_r + {{(RIW - 1) {1'b0}}, 1'b1};
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
            idx_r <= {EIW{1'b0}};
            if ((cnt_r - {{(EIW - 12) {1'b0}}, merged_r}) > EIW'(Budget)) begin
              if (vden_r) begin
                pr_ph_r <= 2'd0;
                st_r    <= StPrio;
              end else begin
                // F4: a null vdist is prio 0 everywhere, so a stable sort keeps
                // scan order and the first `Budget` live edges survive. No
                // threshold, no reads.
                thr_r      <= 32'd0;
                tie_left_r <= EIW'(Budget);
                st_r       <= StKeep;
              end
            end else begin
              st_r <= StEmit;
            end
          end else if (ridx_r == runs_r) begin
            mlen_r <= mlen_r - 6'd1;
            ridx_r <= {RIW{1'b0}};
          end else if (run_rd_c[5:0] == mlen_r) begin
            mhead_r <= run_rd_c[16:6];
            // R1: shed the MINIMUM — take = min(len, need + 1).
            mtake_r <= ({{(EIW - 6) {1'b0}}, mlen_r} > (need_r + {{(EIW - 1) {1'b0}}, 1'b1}))
                       ? (need_r[5:0] + 6'd1) : mlen_r;
            mstep_r <= 6'd1;
            ridx_r  <= ridx_r + {{(RIW - 1) {1'b0}}, 1'b1};
            st_r    <= StMdead;
          end else begin
            ridx_r <= ridx_r + {{(RIW - 1) {1'b0}}, 1'b1};
          end
        end

        // -------------------------------------------------------------------
        StMdead: begin
          if (mstep_r == mtake_r) begin
            edge_mem_r[mhead_r][5:0] <= mtake_r;
            merged_r <= merged_r + {6'd0, mtake_r} - 12'd1;
            need_r   <= need_r - ({{(EIW - 6) {1'b0}}, mtake_r} - {{(EIW - 1) {1'b0}}, 1'b1});
            st_r     <= StMsel;
          end else begin
            alive_r[(mhead_r + {5'd0, mstep_r})] <= 1'b0;
            mstep_r <= mstep_r + 6'd1;
          end
        end

        // -------------------------------------------------------------------
        // C2: one priority per edge, computed once and stored. Three clocks per
        // edge: issue va, capture va and issue vb, capture vb and store the max.
        StPrio: begin
          if (idx_r == cnt_r) begin
            thr_r      <= 32'd0;
            bs_bit_r   <= 6'd31;
            bs_count_r <= {EIW{1'b0}};
            idx_r      <= {EIW{1'b0}};
            st_r       <= StBsCount;
          end else if (pr_ph_r == 2'd0) begin
            pr_ph_r <= 2'd1;
          end else if (pr_ph_r == 2'd1) begin
            pr_va_r <= vd_data_i;
            pr_ph_r <= 2'd2;
          end else begin
            prio_mem_r[idx_r[10:0]] <= prio_max_c;
            pr_ph_r           <= 2'd0;
            idx_r             <= idx_r + {{(EIW - 1) {1'b0}}, 1'b1};
          end
        end

        // -------------------------------------------------------------------
        // count(live && key >= thr | 1<<bit)
        StBsCount: begin
          if (idx_r == cnt_r) begin
            st_r <= StBsStep;
          end else begin
            if (alive_r[idx_r[10:0]] && prio_key_c >= bs_cand_c) begin
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
            // thr_r is now the largest key with count(key >= thr) >= Budget.
            // Next: count(key > thr) so the tie allowance is known.
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
            st_r       <= StKeep;
          end else begin
            if (alive_r[idx_r[10:0]] && prio_key_c > thr_r) begin
              bs_count_r <= bs_count_r + {{(EIW - 1) {1'b0}}, 1'b1};
            end
            idx_r <= idx_r + {{(EIW - 1) {1'b0}}, 1'b1};
          end
        end

        // -------------------------------------------------------------------
        // One pass in SCAN ORDER: keep everything above the cut plus as many
        // ties as the budget allows — exactly a stable descending sort followed
        // by a prefix. R2: a dropped span takes its whole body count with it.
        StKeep: begin
          if (idx_r == cnt_r) begin
            idx_r       <= {EIW{1'b0}};
            emit_live_r <= 1'b0;
            st_r        <= StEmit;
          end else begin
            if (alive_r[idx_r[10:0]]) begin
              if (keep_c) begin
                // An edge ABOVE the cut is kept outright; one AT the cut spends
                // the tie allowance. An edge BELOW the cut must never spend it,
                // however early it comes in scan order — that is the whole
                // difference between "a stable sort then a prefix" and "the
                // first Budget survivors", and it is what `keep_c` encodes.
                if (spend_c) tie_left_r <= tie_left_r - {{(EIW - 1) {1'b0}}, 1'b1};
              end else begin
                alive_r[idx_r[10:0]] <= 1'b0;
                dropped_r            <= dropped_r + {6'd0, edge_rd_c[5:0]};
              end
            end
            idx_r <= idx_r + {{(EIW - 1) {1'b0}}, 1'b1};
          end
        end

        // -------------------------------------------------------------------
        StEmit: begin
          if (emit_live_r) begin
            if (edge_ready_i) begin
              emit_live_r <= 1'b0;
              if (triangles_submitted_o < (CntMax - 32'd1)) begin
                triangles_submitted_o <= triangles_submitted_o + 32'd2;  // C3
              end
              idx_r <= idx_r + {{(EIW - 1) {1'b0}}, 1'b1};
            end
          end else if (idx_r == cnt_r) begin
            merged_q_r  <= merged_r;
            dropped_q_r <= dropped_r;
            page_done_r <= 1'b1;
            st_r        <= StIdle;
          end else if (alive_r[idx_r[10:0]]) begin
            emit_e_r    <= edge_rd_c;
            emit_live_r <= 1'b1;
          end else begin
            idx_r <= idx_r + {{(EIW - 1) {1'b0}}, 1'b1};
          end
        end

        default: st_r <= StIdle;
      endcase
    end
  end

endmodule
