// zhao_geom_binner.sv — GEOM.BINNER: setup triangles into 16×16 tile chunked
// lists with a bounded arena and safe overflow, then drained straight into
// RASTER.EDGEWALK's job port (phase 5, ZH-058 / ZH-026).
//
// Law (in citation order):
//   design/contracts/GEOM.BINNER.md — the block contract.
//   design/blocks.yml — `inputs: [setup_triangles, token_grant]`, `outputs:
//       [tile_lists]`, `upstream: [GEOM.SETUP, MEASURE.TOKENS]`, `downstream:
//       [RASTER.EDGEWALK]`, `target_throughput: 1 bin reference per clock`,
//       counters `tile_references` / `max_tile_list_depth` /
//       `triangles_culled`, formal `tests/formal/geom_binner_arena_bounds.sby`,
//       and the note this file exists to honour: **"Safe overflow: excess
//       triangles degrade to next-frame, never scribble."**
//   ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md §8 phase 5, build order item 3 —
//       "chunked tile lists and safe overflow" — and ZH-026, "Add chunked tile
//       lists and formal bounds".
//   spec/qformats.md §8 — the edge functions this block trivially-rejects
//       tiles with, and the ±2048 px guard band the coordinates live in.
//   spec/counters.md §4 — counters saturate, never wrap; and
//       `max_tile_list_depth` "is a high-water mark, not a count".
//   fpga/rtl/raster/zhao_raster_edgewalk.sv — the CONSUMER. Its job port is
//       `{6 × signed 21 vertex, signed 12 tile_x, signed 12 tile_y, src_id}`
//       and this block's drain port is exactly that, field for field —
//       including the units: `job_tile_x_o` is the tile's top-left PIXEL, as
//       that block's contract says, not a tile index.
//
// WHAT THIS BLOCK IS NOT: no clipping or scissoring (GEOM.CLIP hands over an
// already-scissored scan box), no edge setup (GEOM.SETUP), no coverage
// (RASTER.EDGEWALK — this block decides WHICH tiles get walked, never which
// pixels), no VRAM (the arena is on-chip and fixed), no frame scheduling, no
// re-submission of overflowed work, and no token POLICY (MEASURE.TOKENS owns
// that; see the token section).
//
// ---------------------------------------------------------------------------
// LAWS FOUND
// ---------------------------------------------------------------------------
// 1. THE TILE IS 16×16 PIXELS. Charter §8's "active tile storage" and
//    RASTER.TILESTORE / RASTER.EDGEWALK both fix it; `zref::EdgeWalk::kTile`
//    is 16. This block does not get to choose the pitch.
// 2. THE ENUMERATION RECTANGLE IS GEOM.CLIP's SCAN BOX, which is raster_tri's
//    own scissored pixel-centre bounding box (§8, the 2026-08-15 defect fix).
//    A pixel outside that box is never scanned by the software raster, so a
//    tile outside it can hold no coverage and is not a candidate at all.
// 3. THE TRIVIAL-REJECT PREDICATE IS THE §8 FILL RULE ITSELF. See below — the
//    module instantiated is `zhao_raster_fill`, the one the formal lane proves.
// 4. SUBMISSION ORDER IS PRESERVED WITHIN A TILE. The reference renderer is a
//    painter (plan W3.5/D7, restated at internal.hpp's raster_tri: "terrain
//    cells rasterize with depth_test = OFF and depth_write = ON — the painter
//    sort IS the ordering between terrain cells"), so the order triangles hit
//    a tile is part of the picture, not an implementation detail. The tile
//    list is therefore FIFO: appended at a TAIL pointer and drained head
//    first. A push-front singly-linked list — the cheap one — would reverse
//    every tile's draw order and quietly break the painter's algorithm on
//    exactly the geometry (flat terrain, constant 1/w) that has no depth test
//    to save it. That is why a tile entry carries a tail as well as a head.
//
// ---------------------------------------------------------------------------
// LAWS CHOSEN (no spec states these; decisions, recorded as such)
// ---------------------------------------------------------------------------
// A. THE TILE GRID IS ANCHORED AT SURFACE PIXEL (0,0), pitch 16, so tile
//    (tx,ty) owns pixels [16tx, 16tx+16) × [16ty, 16ty+16). Nothing states an
//    anchor. This one is chosen because it is the only one under which a tile
//    never straddles a viewport edge in ANY shipping mode: video_rules.md §1
//    gives 384×240 (Z60), 320×240 (Storm) and Duo's two 256×192 view blocks
//    STACKED at rows 0 and 192 (§3.1), and 384, 320, 256, 240 and 192 are all
//    multiples of 16. A grid anchored on the viewport origin instead would be
//    identical here and would differ the moment a canvas stops being
//    16-aligned; anchoring on the SURFACE keeps one grid for both Duo views.
//
// B. ENUMERATION ORDER IS ROW-MAJOR — ty ascending outer, tx ascending inner.
//    Nothing states an order and it is observable: it is the order tiles reach
//    RASTER.EDGEWALK. Chosen to match the framebuffer's own row-major
//    top-left-origin layout (video_rules.md §3) and RASTER.RESOLVE's tile
//    order, so a tile's work and its resolve run in the same direction and a
//    trace reads the same way in both places. Boustrophedon (serpentine) order
//    would halve the worst-case tile-to-tile distance for a future tile cache
//    and is the obvious alternative; it is NOT taken, because no tile cache
//    exists to benefit and the asymmetry would have to be undone later.
//    The DRAIN order is likewise row-major over the whole grid.
//
// C. THE TRIVIAL-REJECT IS THE AFFINE CORNER TEST, AND IT IS THE FILL RULE.
//    Each edge value `E'` is affine in the pixel position — it steps by `kx`
//    per pixel of x and `ky` per pixel of y (GEOM.SETUP) — so its MAXIMUM over
//    the 256 pixel centres of a tile is at a corner, the one selected by the
//    signs of `kx` and `ky`:
//        E'_max = E'(tile top-left centre) + (kx>0 ? 15·kx : 0)
//                                          + (ky>0 ? 15·ky : 0)
//    If `E'_max` fails the §8 fill test for ANY edge, no centre in the tile can
//    pass it and the tile is certainly empty. The test is SOUND (it never
//    rejects a tile that has coverage) and CONSERVATIVE (it may keep an empty
//    one); the directed and random lanes assert the soundness half against
//    `zref::EdgeWalk` over every tile of the grid, which is the property that
//    matters — a lost tile is a hole in the picture, a kept empty tile is only
//    a wasted 21-cycle edge walk.
//
//    The predicate is not re-derived: the module instantiated is
//    `zhao_raster_fill`, the same one RASTER.EDGEWALK instantiates 48× per row
//    and the same one tests/formal/raster_edgewalk_top_left.sby proves equal to
//    `E0 + bias ≥ 0`. The binner's reject rule and the rasterizer's accept rule
//    are the same bytes, which is why they cannot disagree.
//
//    A plain bbox-only binner is the alternative and it is what a naive
//    implementation does; on a thin diagonal spanning the screen it hands
//    RASTER.EDGEWALK the entire bounding rectangle of tiles — for a 24×15 grid
//    that is 360 tile jobs where ~24 have coverage, i.e. 15× the edge-walk work
//    for the same picture. Three 36-bit adders and three fill comparators buy
//    that back.
//
// D. SAFE OVERFLOW IS A WALL, NOT A SCRIBBLE, AND ITS EDGE IS NAMED.
//    Two arenas can run out: the triangle store (TRI_CAP triangles per frame)
//    and the chunk arena (CHUNKS chunks). On either:
//      · nothing outside the arena is ever written — `zhao_geom_arena` never
//        presents a grant when full, and that is the formal property;
//      · the current triangle is abandoned and `overflow_o` LATCHES;
//      · every subsequent triangle of the frame is dropped whole and counted
//        into `triangles_culled` — the WALL. Because submission order is
//        painter order, walling off the TAIL of the frame is exactly the
//        "degrade to next-frame" the ledger asks for: what is lost is the work
//        the next frame would carry anyway, not a random half of the scene.
//    ONE triangle per frame can be PARTIALLY binned — the one that hits the
//    wall mid-enumeration, which appears in a prefix of its tiles. That is
//    stated rather than hidden: making it atomic would need either a two-pass
//    count (doubling the enumeration cost of every triangle, for a case that
//    should never fire) or a rollback journal.
//    NOT BUILT, and named so the next wave knows: the re-submission half of
//    "degrade to next-frame". Nothing here remembers a dropped triangle or
//    hands it to the following frame; that is a frame-scheduler behaviour
//    (CMD.SCHEDULER / MEASURE.TOKENS) and this block only reports, through
//    `overflow_o` and `triangles_culled_o`, that it happened.
//
// E. THE TOKEN INTERFACE IS THE MINIMUM SURFACE THAT HONOURS THE LEDGER.
//    MEASURE.TOKENS is phase 8, its contract is still a stub, and no packet
//    layout for `token_grant` exists anywhere. The ledger nevertheless lists it
//    upstream of this block with `backpressure: credit`. So: one combinational
//    request/grant pair. `tok_req_o` pulses on the cycle a triangle is accepted
//    and `tok_grant_i` is sampled on that same edge; a denied triangle is
//    dropped and counted into `triangles_culled`. Tie `tok_grant_i` high and
//    the guard is absent, which is what every test that is not about tokens
//    does and what the reset state assumes. Deliberately NOT invented here:
//    the 45/45/10 Duo fairness split, any token WIDTH or cost model, and the
//    return path — all of those are MEASURE.TOKENS' law to write.
//
// ---------------------------------------------------------------------------
// WIDTHS — why ACC_W is 36
// ---------------------------------------------------------------------------
// With |v| ≤ 2^19 (the ±2048 px guard band in S 12.8), GEOM.SETUP's constants
// obey |kx|,|ky| ≤ 2^20 and |kc| ≤ 2^39. Then
//   · E0 at pixel (0,0)'s centre is kx·128 + ky·128 + kc, |·| < 2^40, so
//     E'_base = E0 >>> 8 has |·| ≤ 2^32;
//   · the walk adds kx·px + ky·py over the grid, |px| ≤ 16·(GRID_W−1) < 2^9
//     and |py| likewise, so |·| < 2^29 each;
//   · the corner offset is 15·(|kx| + |ky|) < 2^25.
// The largest value the accumulator ever carries is therefore below
// 2^32 + 2^29 + 2^29 + 2^25 < 2^33, and 36 bits signed (±2^35) holds it with
// two bits to spare. No saturation is needed or used: unlike RASTER.EDGEWALK,
// which narrows to a tile-local domain, this block evaluates one exact value
// per tile and needs the sign of the true number.
//
// ---------------------------------------------------------------------------
// MEMORIES AND THEIR SHAPE
// ---------------------------------------------------------------------------
//   tri_ram  [TRI_CAP]  142b = {src_id[15:0], cy,cx,by,bx,ay,ax}  — the frame's
//            triangle store. A tile list holds INDICES into it, which is the
//            whole point of binning: a triangle touching 40 tiles costs 40 × 7
//            bits, not 40 × 142.
//   ref_ram  [CHUNKS·CHUNK_REFS]  7b — the chunked reference array. One ref per
//            entry, so a push is a single write and never a read-modify-write.
//   next_ram [CHUNKS]  9b = {valid, chunk} — the chunk chain. ONE pointer per
//            CHUNK_REFS references: that is what "chunked" buys, and it is why
//            the pointer overhead is 9/(4·7) ≈ 32% instead of 9/7 ≈ 129%.
//   tile_ram [TILES]  27b = {count[10:0], tail[7:0], head[7:0]} — per tile.
// No read and write of the same RAM address ever occur in the same cycle:
// binning writes tile_ram one cycle after reading it and consecutive tiles of
// one triangle are distinct; the clear phase only writes; the drain only reads.
//
// ---------------------------------------------------------------------------
// TIMING — MEASURED, AND THE LEDGER TARGET IS NOT MET
// ---------------------------------------------------------------------------
// The ledger asks "1 bin reference per clock". THIS BLOCK DOES NOT MEET IT.
// A kept tile costs TWO cycles (one to evaluate the corner test and issue the
// tile_ram read, one to append the reference) and a rejected candidate costs
// ONE, plus 3 setup cycles per triangle. MEASURED by
// tests/geometry/geom_binner_directed.cpp:test_throughput on a full-canvas
// triangle over a 24×15 grid: **561 cycles for 198 references = 2.83 cycles
// per emitted reference**, a 2.83× shortfall against the ledger target.
// Closing it would mean pipelining the tile_ram read-modify-write behind a
// same-address forwarding path (consecutive tiles of one triangle are always
// distinct, so only the triangle boundary needs the forward); that is not
// built, and the number is stated here and in the contract rather than left to
// be discovered.
//
// The DRAIN is not a per-job cost at all — it is structural: two cycles for
// EVERY tile of the grid (the head read, whether or not the list is empty)
// plus four per emitted job. MEASURED on the same fixture: 1,946 cycles for
// 198 jobs over 360 tiles = 720 + ~4·198 + drain-end. That is not a
// bottleneck by construction: RASTER.EDGEWALK spends 21…37 cycles on the job
// it is handed, so behind it the drain is idle ~80% of the time, and the
// whole-grid scan is 720 cycles out of a 251,520-cycle frame (0.3%).
// ENFORCED-BY: tests/geometry/geom_binner_directed.cpp:test_throughput
//
// Conservative SystemVerilog subset only (charter §2); depends on
// zhao_raster_fill and zhao_geom_arena. No package deps.
// Lint: clean under `verilator_bin --lint-only -Wall` (lint_geom_binner).

module zhao_geom_binner #(
  // The tile grid. 24 × 24 tiles = 384 × 384 pixels, which covers every
  // shipping canvas: Z60 384×240, Storm 320×240, and Duo's two 256×192 view
  // blocks stacked into 256×384 (video_rules.md §1/§3.1).
  parameter int unsigned GRID_W     = 24,
  parameter int unsigned GRID_H     = 24,
  parameter int unsigned TILES      = GRID_W * GRID_H,   // 576
  parameter int unsigned TIDX_W     = 10,                // $clog2(576)
  parameter int unsigned TRI_CAP    = 128,               // triangles per frame
  parameter int unsigned TRI_W      = 7,                 // $clog2(TRI_CAP)
  parameter int unsigned CHUNKS     = 256,               // chunks in the arena
  parameter int unsigned CHUNK_W    = 8,                 // $clog2(CHUNKS)
  parameter int unsigned CHUNK_REFS = 4                  // references per chunk
) (
  input  logic clk,
  input  logic rst_n,

  // ---- frame boundaries --------------------------------------------------
  // `frame_begin_i` releases the whole arena and clears the tile heads (TILES
  // cycles, during which no triangle is accepted). `frame_end_i` closes the
  // bin phase; the drain starts as soon as the block is idle.
  input  logic               frame_begin_i,
  input  logic               frame_end_i,
  // Active grid, in tiles (1…GRID_W / 1…GRID_H). The caller MUST cover the
  // viewport: tile indices are `ty·grid_w + tx`, so a grid smaller than the
  // scissor rectangle would alias two tiles onto one list. The enumeration is
  // clamped to the grid so a caller that gets this wrong loses tiles instead
  // of corrupting lists.
  input  logic        [5:0]  grid_w_i,
  input  logic        [5:0]  grid_h_i,

  // ---- setup triangle in: GEOM.SETUP's packet ---------------------------
  input  logic               tri_valid_i,
  output logic               tri_ready_o,
  input  logic signed [22:0] tri_kx0_i,
  input  logic signed [22:0] tri_ky0_i,
  input  logic signed [47:0] tri_kc0_i,
  input  logic signed [22:0] tri_kx1_i,
  input  logic signed [22:0] tri_ky1_i,
  input  logic signed [47:0] tri_kc1_i,
  input  logic signed [22:0] tri_kx2_i,
  input  logic signed [22:0] tri_ky2_i,
  input  logic signed [47:0] tri_kc2_i,
  input  logic        [2:0]  tri_tl_i,
  input  logic signed [20:0] tri_ax_i,
  input  logic signed [20:0] tri_ay_i,
  input  logic signed [20:0] tri_bx_i,
  input  logic signed [20:0] tri_by_i,
  input  logic signed [20:0] tri_cx_i,
  input  logic signed [20:0] tri_cy_i,
  input  logic signed [11:0] tri_min_x_i,
  input  logic signed [11:0] tri_max_x_i,
  input  logic signed [11:0] tri_min_y_i,
  input  logic signed [11:0] tri_max_y_i,
  input  logic        [15:0] tri_src_id_i,

  // ---- MEASURE.TOKENS credit (see LAWS CHOSEN E) ------------------------
  output logic               tok_req_o,
  input  logic               tok_grant_i,

  // ---- drain: RASTER.EDGEWALK's job port, field for field ---------------
  output logic               job_valid_o,
  input  logic               job_ready_i,
  output logic signed [20:0] job_ax_o,
  output logic signed [20:0] job_ay_o,
  output logic signed [20:0] job_bx_o,
  output logic signed [20:0] job_by_o,
  output logic signed [20:0] job_cx_o,
  output logic signed [20:0] job_cy_o,
  output logic signed [11:0] job_tile_x_o,
  output logic signed [11:0] job_tile_y_o,
  output logic        [15:0] job_src_id_o,
  output logic               drain_busy_o,
  output logic               drain_done_o,   // one-cycle pulse: frame drained

  // ---- counters and status ----------------------------------------------
  output logic        [31:0] tile_references_o,
  output logic        [15:0] max_tile_list_depth_o,
  output logic        [31:0] triangles_culled_o,
  output logic               overflow_o,
  // arena observability: the tests drive the wall through these, and the
  // formal harness proves the same two signals on zhao_geom_arena itself.
  output logic               arena_full_o,
  output logic [CHUNK_W:0]   arena_used_o
);


  localparam int unsigned ACC_W      = 36;
  // A SIGNED zero. `{ACC_W{1'b0}}` is unsigned, and Verilog makes a comparison
  // unsigned if EITHER operand is — which would make every negative edge
  // coefficient test as positive and pick the wrong tile corner.
  localparam logic signed [ACC_W-1:0] ACC_ZERO = {ACC_W{1'b0}};
  localparam int unsigned CNT_W      = 11;   // per-tile reference count, 0…1024
  localparam int unsigned SLOT_W     = 2;    // $clog2(CHUNK_REFS)
  localparam int unsigned REF_AW     = CHUNK_W + SLOT_W;
  localparam int unsigned TILE_ENT_W = CNT_W + CHUNK_W + CHUNK_W;
  localparam int unsigned TRI_ENT_W  = 16 + 6*21;

  // ------------------------------------------------------------- states ----
  localparam logic [3:0] S_CLEAR  = 4'd0;   // clear tile_ram
  localparam logic [3:0] S_IDLE   = 4'd1;
  localparam logic [3:0] S_SETUP1 = 4'd2;   // E0 base, rnz, corner offsets
  localparam logic [3:0] S_SETUP2 = 4'd3;   // E' at the first tile of the range
  localparam logic [3:0] S_TILE   = 4'd4;   // corner test + tile_ram read
  localparam logic [3:0] S_PUSH   = 4'd5;   // append the reference
  localparam logic [3:0] D_TILE   = 4'd6;   // drain: issue tile_ram read
  localparam logic [3:0] D_HEAD   = 4'd7;   // drain: list head / count
  localparam logic [3:0] D_REF    = 4'd8;   // drain: issue ref_ram read
  localparam logic [3:0] D_TRI    = 4'd9;   // drain: issue tri_ram read
  localparam logic [3:0] D_EMIT   = 4'd10;  // drain: present the job
  localparam logic [3:0] D_DONE   = 4'd11;

  logic [3:0] state;

  // --------------------------------------------------------------- RAMs ----
  logic [TRI_ENT_W-1:0]  tri_ram  [0:TRI_CAP-1];
  logic [TRI_W-1:0]      ref_ram  [0:(CHUNKS*CHUNK_REFS)-1];
  logic [CHUNK_W-1:0]    next_ram [0:CHUNKS-1];
  logic [TILE_ENT_W-1:0] tile_ram [0:TILES-1];

  logic [TRI_ENT_W-1:0]  tri_q;
  logic [TRI_W-1:0]      ref_q;
  logic [CHUNK_W-1:0]    next_q;
  logic [TILE_ENT_W-1:0] tile_q;

  logic                  tile_we, ref_we, next_we, tri_we;
  logic [TIDX_W-1:0]     tile_wa, tile_ra;
  logic [TILE_ENT_W-1:0] tile_wd;
  logic [REF_AW-1:0]     ref_wa, ref_ra;
  logic [TRI_W-1:0]      ref_wd;
  logic [CHUNK_W-1:0]    next_wa, next_ra, next_wd;
  logic [TRI_W-1:0]      tri_wa, tri_ra;
  logic [TRI_ENT_W-1:0]  tri_wd;

  always_ff @(posedge clk) begin
    if (tile_we) tile_ram[tile_wa] <= tile_wd;
    if (ref_we)  ref_ram[ref_wa]   <= ref_wd;
    if (next_we) next_ram[next_wa] <= next_wd;
    if (tri_we)  tri_ram[tri_wa]   <= tri_wd;
    tile_q <= tile_ram[tile_ra];
    ref_q  <= ref_ram[ref_ra];
    next_q <= next_ram[next_ra];
    tri_q  <= tri_ram[tri_ra];
  end

  // ------------------------------------------------------------- arena ----
  logic               arena_alloc, arena_ok, arena_full;
  logic [CHUNK_W-1:0] arena_ptr;
  logic [CHUNK_W:0]   arena_used;

  zhao_geom_arena #(.CHUNKS(CHUNKS), .PTR_W(CHUNK_W)) u_arena (
    .clk        (clk),
    .rst_n      (rst_n),
    .release_i  (frame_begin_i),
    .alloc_i    (arena_alloc),
    .alloc_ok_o (arena_ok),
    .alloc_ptr_o(arena_ptr),
    .full_o     (arena_full),
    .used_o     (arena_used)
  );

  assign arena_full_o = arena_full;
  assign arena_used_o = arena_used;

  // ------------------------------------------------------- triangle state --
  logic signed [ACC_W-1:0] kx_r  [0:2];
  logic signed [ACC_W-1:0] ky_r  [0:2];
  logic signed [ACC_W-1:0] ep_r  [0:2];   // E' at the current tile's origin
  logic signed [ACC_W-1:0] epr_r [0:2];   // E' at the current ROW's first tile
  logic signed [ACC_W-1:0] off_r [0:2];   // the max-corner offset
  logic signed [47:0]      kc_r  [0:2];
  logic        [2:0]       rnz_r, tl_r;

  logic [5:0]        tx_r, ty_r, tx0_r, tx1_r, ty0_r, ty1_r;
  logic [TIDX_W-1:0] row_base_r;
  logic [TRI_W-1:0]  tri_idx_r;
  logic [TRI_W:0]    tri_count_r;
  logic              wall_r, overflow_r, drain_req_r;
  logic [TIDX_W-1:0] clear_i_r;

  // ------------------------------------------------------- drain registers -
  logic [TIDX_W-1:0]    d_idx_r;
  logic [5:0]           d_tx_r, d_ty_r;
  logic [CNT_W-1:0]     d_rem_r;
  logic [CHUNK_W-1:0]   d_chunk_r;
  logic [SLOT_W-1:0]    d_slot_r;
  logic [TRI_ENT_W-1:0] d_tri_r;
  logic [5:0]           d_jx_r, d_jy_r;
  logic                 d_job_v, drain_done_r;

  // ------------------------------------------------------------ counters ---
  logic [31:0] cnt_refs, cnt_culled;
  logic [15:0] max_depth;

  assign tile_references_o     = cnt_refs;
  assign max_tile_list_depth_o = max_depth;
  assign triangles_culled_o    = cnt_culled;
  assign overflow_o            = overflow_r;

  // --------------------------------------------- tile entry field access ---
  logic [CNT_W-1:0]   cur_count;
  logic [CHUNK_W-1:0] cur_tail, cur_head;
  assign cur_count = tile_q[TILE_ENT_W-1 -: CNT_W];
  assign cur_tail  = tile_q[2*CHUNK_W-1 -: CHUNK_W];
  assign cur_head  = tile_q[CHUNK_W-1 -: CHUNK_W];

  // ------------------------------------------------ the §8 corner test -----
  // E'_max per edge, then the SHIPPING fill predicate — zhao_raster_fill, the
  // module RASTER.EDGEWALK instantiates and the formal lane proves.
  logic signed [ACC_W-1:0] emax [0:2];
  logic [2:0] tile_accept;

  genvar ge;
  generate
    for (ge = 0; ge < 3; ge = ge + 1) begin : g_edge
      assign emax[ge] = ep_r[ge] + off_r[ge];
      zhao_raster_fill #(.W(ACC_W)) u_fill (
        .e_i     (emax[ge]),
        .rnz_i   (rnz_r[ge]),
        .tl_i    (tl_r[ge]),
        .accept_o(tile_accept[ge])
      );
    end
  endgenerate

  logic tile_keep;
  assign tile_keep = (tile_accept == 3'b111);

  // ------------------------------------------------------------ helpers ----
  function automatic logic signed [ACC_W-1:0] ext23(input logic signed [22:0] v);
    ext23 = $signed({{(ACC_W-23){v[22]}}, v});
  endfunction

  // The clamped tile row/column of a pixel coordinate: floor(p / 16), pinned
  // to [0, limit-1] so a caller's undersized grid can never alias a tile index
  // onto another tile's list (it loses tiles instead — LAWS CHOSEN B/D).
  function automatic logic [5:0] tile_of(input logic signed [11:0] p,
                                         input logic        [5:0]  limit);
    logic signed [11:0] q;
    logic signed [11:0] hi;
    begin
      q  = p >>> 4;                            // arithmetic >> 4 = floor(p/16)
      hi = $signed({6'd0, limit}) - 12'sd1;
      if (limit == 6'd0)   tile_of = 6'd0;
      else if (q[11])      tile_of = 6'd0;
      else if (q > hi)     tile_of = limit - 6'd1;
      else                 tile_of = q[5:0];
    end
  endfunction

  // 15·k for the corner offset: (k << 4) − k.
  function automatic logic signed [ACC_W-1:0] mul15(input logic signed [ACC_W-1:0] k);
    mul15 = (k <<< 4) - k;
  endfunction

  // E0 at the centre of pixel (0,0) for edge e, in the 48-bit setup domain:
  // kx·128 + ky·128 + kc. Both edge steps are multiples of 256, so this
  // value's LOW BYTE is the same `r` at every pixel centre of the screen —
  // one constant bit per edge, exactly RASTER.EDGEWALK's `rnz`.
  function automatic logic signed [47:0] e0_base(input logic [1:0] e);
    logic signed [47:0] kxw;
    logic signed [47:0] kyw;
    begin
      kxw = $signed({{(48-ACC_W){kx_r[e][ACC_W-1]}}, kx_r[e]});
      kyw = $signed({{(48-ACC_W){ky_r[e][ACC_W-1]}}, ky_r[e]});
      e0_base = (kxw <<< 7) + (kyw <<< 7) + kc_r[e];
    end
  endfunction

  // E' = floor(E0 / 256), the §8 decomposition. FLOOR, not truncation toward
  // zero: `>>>` on a signed value is the arithmetic shift, and the fill rule
  // is stated on `E0 = 256·E' + r` with `r ∈ [0,255]`, which only holds for
  // the flooring quotient. Truncating instead moves every NEGATIVE edge value
  // by one unit and turns the tile reject into a coin flip at the boundary.
  function automatic logic signed [ACC_W-1:0] ep_of(input logic signed [47:0] e0);
    ep_of = ACC_W'(e0 >>> 8);
  endfunction

  // tile index of a row. GRID_W is a parameter, so `ty·GRID_W` is a constant
  // multiply (24 = 16 + 8), not a multiplier. BOTH phases use it, so the bin
  // and the drain address the same list.
  function automatic logic [TIDX_W-1:0] tidx(input logic [5:0] ty);
    tidx = TIDX_W'(ty) * TIDX_W'(GRID_W);
  endfunction

  // ------------------------------------------------------- the push muxes --
  logic               need_chunk, push_ok;
  logic [CHUNK_W-1:0] push_chunk;
  logic [SLOT_W-1:0]  push_slot;

  always_comb begin
    push_slot   = cur_count[SLOT_W-1:0];
    need_chunk  = (push_slot == {SLOT_W{1'b0}});      // count % CHUNK_REFS == 0
    arena_alloc = (state == S_PUSH) && need_chunk;
    push_ok     = !need_chunk || arena_ok;
    push_chunk  = need_chunk ? arena_ptr : cur_tail;
  end

  always_comb begin
    // ---- tile_ram: read the entry in S_TILE, write it back in S_PUSH ------
    tile_ra = (state == S_TILE) ? (row_base_r + TIDX_W'(tx_r)) : d_idx_r;
    tile_we = (state == S_CLEAR) || ((state == S_PUSH) && push_ok);
    tile_wa = (state == S_CLEAR) ? clear_i_r : (row_base_r + TIDX_W'(tx_r));
    if (state == S_CLEAR) begin
      tile_wd = {TILE_ENT_W{1'b0}};
    end else begin
      tile_wd = {cur_count + {{(CNT_W-1){1'b0}}, 1'b1},
                 push_chunk,
                 (cur_count == {CNT_W{1'b0}}) ? push_chunk : cur_head};
    end

    // ---- ref_ram: one reference per entry, so a push is a single write ----
    ref_we = (state == S_PUSH) && push_ok;
    ref_wa = {push_chunk, push_slot};
    ref_wd = tri_idx_r;
    ref_ra = {d_chunk_r, d_slot_r};

    // ---- next_ram: written only when a NEW chunk is chained on -----------
    next_we = (state == S_PUSH) && push_ok && need_chunk &&
              (cur_count != {CNT_W{1'b0}});
    next_wa = cur_tail;
    next_wd = push_chunk;
    next_ra = d_chunk_r;

    // ---- tri_ram: the frame's triangle store ------------------------------
    tri_we = (state == S_IDLE) && tri_valid_i && tri_ready_o && tok_grant_i &&
             !wall_r && (tri_count_r != (TRI_W+1)'(TRI_CAP));
    tri_wa = tri_count_r[TRI_W-1:0];
    tri_wd = {tri_src_id_i, tri_cy_i, tri_cx_i, tri_by_i, tri_bx_i, tri_ay_i, tri_ax_i};
    tri_ra = (state == D_TRI) ? ref_q : tri_idx_r;
  end

  // ------------------------------------------------------------ handshake --
  assign tri_ready_o = (state == S_IDLE) && !drain_req_r;
  assign tok_req_o   = tri_valid_i && tri_ready_o;

  // ------------------------------------------------------- drain outputs ---
  assign job_valid_o  = d_job_v;
  assign job_ax_o     = $signed(d_tri_r[20:0]);
  assign job_ay_o     = $signed(d_tri_r[41:21]);
  assign job_bx_o     = $signed(d_tri_r[62:42]);
  assign job_by_o     = $signed(d_tri_r[83:63]);
  assign job_cx_o     = $signed(d_tri_r[104:84]);
  assign job_cy_o     = $signed(d_tri_r[125:105]);
  assign job_src_id_o = d_tri_r[141:126];
  // RASTER.EDGEWALK's `job_tile_x_i` is "the tile origin — the top-left PIXEL
  // of the 16×16 tile" (its contract, Input packet layouts), NOT a tile index.
  // The drain port is that port field for field, so the index is scaled here,
  // once, and the pixel is what leaves the block. (Emitting the index instead
  // is a silent 16× error that no differential against a tile-indexed oracle
  // would ever see; it took the zhao_geom_bin_pipe composition — a real
  // rasterized picture — to catch it, which is exactly why that composition
  // was built. tests/geometry/geom_binner_directed.cpp:test_pixel_origin now
  // pins it directly.)
  assign job_tile_x_o = $signed({2'd0, d_jx_r, 4'd0});
  assign job_tile_y_o = $signed({2'd0, d_jy_r, 4'd0});
  assign drain_busy_o = (state >= D_TILE) && (state <= D_EMIT);
  assign drain_done_o = drain_done_r;

  // ---------------------------------------------------------- sequential ---
  // The two cursors advance in exactly one place each, driven by these two
  // predicates, so no register is written from two different case arms.
  logic adv;       // finish this tile and move the BIN cursor
  logic nxt_tile;  // finish this tile and move the DRAIN cursor

  always_comb begin
    adv      = (state == S_TILE) ? !tile_keep : ((state == S_PUSH) && push_ok);
    nxt_tile = ((state == D_HEAD) && (cur_count == {CNT_W{1'b0}})) ||
               ((state == D_EMIT) && d_job_v && job_ready_i &&
                (d_rem_r == {{(CNT_W-1){1'b0}}, 1'b1}));
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state        <= S_CLEAR;
      clear_i_r    <= {TIDX_W{1'b0}};
      tx_r         <= 6'd0;
      ty_r         <= 6'd0;
      tx0_r        <= 6'd0;
      tx1_r        <= 6'd0;
      ty0_r        <= 6'd0;
      ty1_r        <= 6'd0;
      row_base_r   <= {TIDX_W{1'b0}};
      tri_idx_r    <= {TRI_W{1'b0}};
      tri_count_r  <= {(TRI_W+1){1'b0}};
      wall_r       <= 1'b0;
      overflow_r   <= 1'b0;
      drain_req_r  <= 1'b0;
      rnz_r        <= 3'd0;
      tl_r         <= 3'd0;
      cnt_refs     <= 32'd0;
      cnt_culled   <= 32'd0;
      max_depth    <= 16'd0;
      d_idx_r      <= {TIDX_W{1'b0}};
      d_tx_r       <= 6'd0;
      d_ty_r       <= 6'd0;
      d_rem_r      <= {CNT_W{1'b0}};
      d_chunk_r    <= {CHUNK_W{1'b0}};
      d_slot_r     <= {SLOT_W{1'b0}};
      d_tri_r      <= {TRI_ENT_W{1'b0}};
      d_jx_r       <= 6'd0;
      d_jy_r       <= 6'd0;
      d_job_v      <= 1'b0;
      drain_done_r <= 1'b0;
      for (int k = 0; k < 3; k++) begin
        kx_r[k]  <= {ACC_W{1'b0}};
        ky_r[k]  <= {ACC_W{1'b0}};
        ep_r[k]  <= {ACC_W{1'b0}};
        epr_r[k] <= {ACC_W{1'b0}};
        off_r[k] <= {ACC_W{1'b0}};
        kc_r[k]  <= 48'sd0;
      end
    end else begin
      drain_done_r <= 1'b0;

      if (frame_begin_i) begin
        // A new frame. The arena is released by the same pulse (u_arena), the
        // triangle store restarts, the wall drops, and the tile heads are
        // cleared entry by entry. Nothing survives a frame boundary.
        state       <= S_CLEAR;
        clear_i_r   <= {TIDX_W{1'b0}};
        tri_count_r <= {(TRI_W+1){1'b0}};
        wall_r      <= 1'b0;
        overflow_r  <= 1'b0;
        drain_req_r <= 1'b0;
        d_job_v     <= 1'b0;
      end else begin
        if (frame_end_i) drain_req_r <= 1'b1;

        case (state)
          // ---------------------------------------------------------------
          S_CLEAR: begin
            if (clear_i_r == TIDX_W'(TILES - 1)) state <= S_IDLE;
            else clear_i_r <= clear_i_r + TIDX_W'(1);
          end

          // ---------------------------------------------------------------
          S_IDLE: begin
            if (drain_req_r) begin
              d_idx_r <= {TIDX_W{1'b0}};
              d_tx_r  <= 6'd0;
              d_ty_r  <= 6'd0;
              state   <= D_TILE;
            end else if (tri_valid_i) begin
              if (!tok_grant_i || wall_r || (tri_count_r == (TRI_W+1)'(TRI_CAP))) begin
                // denied by MEASURE.TOKENS, walled off by an earlier overflow,
                // or the triangle store is full — dropped whole and counted;
                // in the store-full case the wall goes up (LAWS CHOSEN D).
                if (cnt_culled != 32'hFFFF_FFFF) cnt_culled <= cnt_culled + 32'd1;
                if (tok_grant_i && !wall_r &&
                    (tri_count_r == (TRI_W+1)'(TRI_CAP))) begin
                  wall_r     <= 1'b1;
                  overflow_r <= 1'b1;
                end
              end else begin
                tri_idx_r   <= tri_count_r[TRI_W-1:0];
                tri_count_r <= tri_count_r + {{TRI_W{1'b0}}, 1'b1};
                kx_r[0] <= ext23(tri_kx0_i);
                ky_r[0] <= ext23(tri_ky0_i);
                kx_r[1] <= ext23(tri_kx1_i);
                ky_r[1] <= ext23(tri_ky1_i);
                kx_r[2] <= ext23(tri_kx2_i);
                ky_r[2] <= ext23(tri_ky2_i);
                kc_r[0] <= tri_kc0_i;
                kc_r[1] <= tri_kc1_i;
                kc_r[2] <= tri_kc2_i;
                tl_r    <= tri_tl_i;
                tx0_r   <= tile_of(tri_min_x_i, grid_w_i);
                tx1_r   <= tile_of(tri_max_x_i, grid_w_i);
                ty0_r   <= tile_of(tri_min_y_i, grid_h_i);
                ty1_r   <= tile_of(tri_max_y_i, grid_h_i);
                state   <= S_SETUP1;
              end
            end
          end

          // ---------------------------------------------------------------
          S_SETUP1: begin
            for (int k = 0; k < 3; k++) begin
              // Quartus 17.0 rejects indexing a function call's return value
              // directly (`f(x)[7:0]`), which Verilator accepts. Bind it to a
              // temporary first. Found by synthesis, not by simulation: this
              // is exactly the class of defect a Verilator-only lane cannot
              // see, and it failed EVERY module in the sweep because every
              // file is compiled regardless of which one is the top.
              automatic logic signed [47:0] e0k = e0_base(2'(k));  // matches e0_base
              rnz_r[k] <= (e0k[7:0] != 8'd0);
              ep_r[k]  <= ep_of(e0k);
              off_r[k] <= ((kx_r[k] > ACC_ZERO) ? mul15(kx_r[k]) : ACC_ZERO) +
                          ((ky_r[k] > ACC_ZERO) ? mul15(ky_r[k]) : ACC_ZERO);
            end
            tx_r  <= tx0_r;
            ty_r  <= ty0_r;
            state <= S_SETUP2;
          end

          // E' at the FIRST tile of the range: E'(0,0) + kx·px0 + ky·py0.
          S_SETUP2: begin
            for (int k = 0; k < 3; k++) begin
              ep_r[k]  <= ep_r[k] + kx_r[k] * $signed({1'b0, tx0_r, 4'd0}) +
                                    ky_r[k] * $signed({1'b0, ty0_r, 4'd0});
              epr_r[k] <= ep_r[k] + kx_r[k] * $signed({1'b0, tx0_r, 4'd0}) +
                                    ky_r[k] * $signed({1'b0, ty0_r, 4'd0});
            end
            row_base_r <= tidx(ty0_r);
            state      <= S_TILE;
          end

          // ---------------------------------------------------------------
          // One cycle per candidate tile: the corner test, and (if it passes)
          // the tile_ram read whose data S_PUSH consumes.
          S_TILE: if (tile_keep) state <= S_PUSH;

          // ---------------------------------------------------------------
          S_PUSH: begin
            if (!push_ok) begin
              // The arena is exhausted: abandon the rest of THIS triangle and
              // wall off the rest of the frame (LAWS CHOSEN D).
              wall_r     <= 1'b1;
              overflow_r <= 1'b1;
              state      <= S_IDLE;
            end else begin
              if (cnt_refs != 32'hFFFF_FFFF) cnt_refs <= cnt_refs + 32'd1;
              if (({5'd0, cur_count} + 16'd1) > max_depth)
                max_depth <= {5'd0, cur_count} + 16'd1;
            end
          end

          // ---------------------------------------------------- the drain --
          D_TILE: state <= D_HEAD;

          D_HEAD: if (cur_count != {CNT_W{1'b0}}) begin
            d_rem_r   <= cur_count;
            d_chunk_r <= cur_head;
            d_slot_r  <= {SLOT_W{1'b0}};
            d_jx_r    <= d_tx_r;
            d_jy_r    <= d_ty_r;
            state     <= D_REF;
          end

          D_REF: state <= D_TRI;   // ref_ram read issued combinationally

          D_TRI: state <= D_EMIT;  // ref_q valid; tri_ram read issued

          D_EMIT: begin
            if (!d_job_v) begin
              d_tri_r <= tri_q;
              d_job_v <= 1'b1;
            end else if (job_ready_i) begin
              d_job_v <= 1'b0;
              if (d_rem_r != {{(CNT_W-1){1'b0}}, 1'b1}) begin
                d_rem_r <= d_rem_r - {{(CNT_W-1){1'b0}}, 1'b1};
                if (d_slot_r == SLOT_W'(CHUNK_REFS - 1)) begin
                  d_slot_r  <= {SLOT_W{1'b0}};
                  d_chunk_r <= next_q;
                end else begin
                  d_slot_r <= d_slot_r + {{(SLOT_W-1){1'b0}}, 1'b1};
                end
                state <= D_REF;
              end
            end
          end

          D_DONE: begin
            drain_done_r <= 1'b1;
            drain_req_r  <= 1'b0;
            state        <= S_IDLE;
          end

          default: state <= S_IDLE;
        endcase

        // ---- the BIN cursor: row-major, in one place (LAWS CHOSEN B) -----
        if (adv) begin
          if (tx_r != tx1_r) begin
            tx_r  <= tx_r + 6'd1;
            state <= S_TILE;
            for (int k = 0; k < 3; k++) ep_r[k] <= ep_r[k] + (kx_r[k] <<< 4);
          end else if (ty_r != ty1_r) begin
            ty_r       <= ty_r + 6'd1;
            tx_r       <= tx0_r;
            row_base_r <= row_base_r + TIDX_W'(GRID_W);
            state      <= S_TILE;
            for (int k = 0; k < 3; k++) begin
              epr_r[k] <= epr_r[k] + (ky_r[k] <<< 4);
              ep_r[k]  <= epr_r[k] + (ky_r[k] <<< 4);
            end
          end else begin
            state <= S_IDLE;       // the triangle is fully enumerated
          end
        end

        // ---- the DRAIN cursor: the same row-major order ------------------
        if (nxt_tile) begin
          if ((d_tx_r == grid_w_i - 6'd1) && (d_ty_r == grid_h_i - 6'd1)) begin
            state <= D_DONE;
          end else begin
            if (d_tx_r == grid_w_i - 6'd1) begin
              d_tx_r  <= 6'd0;
              d_ty_r  <= d_ty_r + 6'd1;
              d_idx_r <= d_idx_r + TIDX_W'(GRID_W) - TIDX_W'(grid_w_i) + TIDX_W'(1);
            end else begin
              d_tx_r  <= d_tx_r + 6'd1;
              d_idx_r <= d_idx_r + TIDX_W'(1);
            end
            state <= D_TILE;
          end
        end
      end
    end
  end

endmodule : zhao_geom_binner
