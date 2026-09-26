// zhao_geom_binner_v2.sv — Packet-D versioned GEOM.BINNER.
//
// This is the exact unversioned binner transport plus one opaque metadata bank.
// Source ID and six vertices stay in the original 142-bit triangle entry; the
// metadata is written by the SAME tri_we/tri_wa event, read by the SAME tri_ra
// transaction, and captured beside d_tri_r before job_valid_o is asserted.
//
// AUTHORITY: reports/PACKET-D-ATTRIBUTE-RASTER-ABI-20260914.md §3 and the exact
// behaviour of fpga/rtl/geometry/zhao_geom_binner.sv.
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
//   tile_ram [TILES]  27b = {count[CNT_W-1:0], tail[CHUNK_W-1:0],
//            head[CHUNK_W-1:0]} — per tile. 27 bits at the DEFAULT parameters
//            (11+8+8); every one of those three widths is DERIVED from the
//            capacity parameters, so this row is a worked example, not a law.
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

// Test-only selector hook. The committed mutant source is compiled immediately
// before this exact RTL and overrides only the metadata read address expression.
`ifndef ZHAO_GEOM_BINNER_V2_META_RA
`define ZHAO_GEOM_BINNER_V2_META_RA(addr) (addr)
`endif

// Test-only WIDTH hook, same pattern and same reason as the one above
// (GIANTREFS). `CNT_W` shipped as a hardcoded 11 sized by hand to the DEFAULT
// arena, and the fault it hides is a per-tile count that WRAPS instead of
// overflowing — silent, and invisible to `overflow_o` by construction. The
// committed mutant `tests/mutants/zhao_geom_binner_v2_mutants.sv` defines this
// back to 11 so that wrap can be SEEN to happen at a parameterisation the
// derivation makes safe. It is a positive control for the derivation; it is
// never defined by any production source list.
`ifndef ZHAO_GEOM_BINNER_V2_CNT_W
`define ZHAO_GEOM_BINNER_V2_CNT_W $clog2(REF_CAP + 1)
`endif

module zhao_geom_binner_v2 #(
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
  parameter int unsigned CHUNK_REFS = 4,                 // references per chunk
  parameter int unsigned METAW      = 1157               // Packet-D opaque metadata
  // ---- THE ARENA ID SLICE WAS HERE AND IT IS RETIRED (ARENACOMPOSE) ------
  // `ARENA_ID_LO` / `ARENA_ID_W` existed for ONE reader: `ser_tri_id_o`, the
  // serialise pass's output. Entry I54's arena identity still rides in the
  // Packet-D continuation tail -- `zhao_console_core` puts it there and
  // nothing in this file needs to know -- but this block no longer slices it
  // back out, because the block that consumed it is retired. Carrying a
  // parameter for a port that no longer exists is the shape CLAUDE.md calls
  // an uncashed cheque; removing the slice does not remove the FIELD.
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
  // Packet-D metadata is opaque here. Its field layout belongs to the ABI; this
  // block preserves every bit under the accepted triangle identity.
  input  logic       [METAW-1:0] tri_meta_i,

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
  // ---- where this reference sits in its tile's list ----------------------
  // The drain already walks a tile list head to tail with `d_rem_r` counting
  // down, so first and last are free. RASTER.TILE_PIPE uses them to clear the
  // bank ONCE per tile and resolve it ONCE, instead of once per triangle --
  // which is what stops a shared tile being rendered twice with the second
  // clear erasing the first triangle.
  //
  // A tile with exactly one reference asserts BOTH, which is the old
  // behaviour and why nothing downstream had to change to keep working.
  //
  // ENFORCED-BY: tests/render/render_pipe_directed.cpp:main
  output logic               job_first_o,
  output logic               job_last_o,
  output logic signed [11:0] job_tile_x_o,
  output logic signed [11:0] job_tile_y_o,
  output logic        [15:0] job_src_id_o,
  output logic       [METAW-1:0] job_meta_o,

  // THE PROFILE VERDICTS, DECIDED AT WRITE AND CARRIED IN THE PAD.
  //
  // bit 0 = aux profile bad, bit 1 = area profile bad. `zhao_raster_tile_pipe_v2`
  // used to derive both from `job_meta_i` on the edge this bank delivers it --
  // a 225-bit OR reduction and a 47-bit zero-compare hanging straight off the
  // RAM output. `@packet-h-satstage` measured that at 4.85 ns of an 11.37 ns
  // path, four LUT levels, and it gated 30 paths into the attribute walker's
  // queue because the fault term also drives admission.
  //
  // Deciding them HERE costs nothing that was not already paid for: the bank is
  // META_SLICES x 40 bits and carries METAW, so the top `META_PAD_W` bits are
  // storage this design already owns and already reads out every cycle. Two of
  // the three are now used.
  //
  // Detection stays SAME-EDGE, which is the property the consumer's abort
  // comment depends on -- the verdict arrives on the edge the metadata does,
  // because it comes out of the same word.
  output logic               [1:0] job_profile_bad_o,
  output logic               drain_busy_o,
  output logic               drain_done_o,   // one-cycle pulse: frame drained

  // ---- THE SERIALISE PASS IS RETIRED -- ARENACOMPOSE, 2026-09-26 ---------
  // This block used to carry a SECOND read walk over its own tile lists
  // (`ser_req_i` and seven `ser_*` outputs, exported through
  // `zhao_shell_top_v2` as `render_ser_*`) so that `zhao_geom_chunkser` could
  // build R7's 64-byte chunk records for `zhao_geom_paramarena`. That made the
  // on-chip arena the thing that FILLS the external one -- console entry I55's
  // BLOCKER 1, measured by WALKSWAP -- and the two were in SERIES, so the
  // external path could never become the sole producer by subtraction.
  //
  // `zhao_geom_arenabin` (GEOM.ARENABIN) now bins the post-clip stream against
  // the arena's own TriangleDescriptor indices and writes the chunks itself,
  // reading NO binner RAM. The serialise pass therefore has no consumer, and a
  // live path left with no consumer is a set of dangling ports rather than a
  // capability. It is REMOVED rather than tied off, which is the only form of
  // retirement that actually returns the logic.
  //
  // WHAT IS **NOT** REMOVED, said explicitly because a reader will ask: the
  // raster drain (`job_*`), the tile lists, `tile_references_o` and every other
  // binning behaviour are untouched. This block still supplies every pixel.

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
  // ---- THE COUNT AND SLOT WIDTHS ARE DERIVED, NOT CHOSEN (GIANTREFS) ------
  // Both of these were hardcoded — `CNT_W = 11` with the comment "0…1024" and
  // `SLOT_W = 2` with the comment "$clog2(CHUNK_REFS)". Each was correct FOR
  // THE DEFAULT PARAMETERS and neither was tied to them, so every capacity
  // parameter of this block was a knob with a silent, undeclared ceiling. A
  // width sized to a parameter by hand is a width that stops being right the
  // first time the parameter moves, and the failure is a WRAP — a corruption,
  // not an overflow, so the safe-overflow wall (LAWS CHOSEN D) does not fire
  // and `overflow_o` stays low while references are lost.
  //
  // REF_CAP is the arena's whole reference capacity and is the conservative
  // bound on a per-tile count: a tile cannot hold more references than the
  // arena holds in total.
  //
  // THE TIGHTER BOUND IS TRI_CAP, AND IT IS DELIBERATELY NOT USED. The bin
  // cursor walks each triangle's tile range strictly row-major and visits each
  // (tx,ty) exactly once (`adv` below, :1071-1087), and a triangle past
  // TRI_CAP is dropped WHOLE in S_IDLE and never enumerated — so a tile's
  // count cannot exceed TRI_CAP either, and min(TRI_CAP, REF_CAP) would be
  // exact. That bound is a property of the ENUMERATION, not of the storage;
  // if a later pass ever re-enumerates a triangle, sizing to it would turn a
  // structural change into a silent corruption. REF_CAP is a property of the
  // memory the count indexes, cannot be invalidated from outside this block,
  // and costs TILES × (CNT_W - $clog2(TRI_CAP+1)) bits to be safe — 1,728 bits
  // at the shipped parameters. That is the right trade in a design where the
  // binding constraint is ALMs and the slack is memory.
  //
  // AND THE DERIVATION IS A NO-OP AT THE SHIPPED PARAMETERS: CHUNKS=256,
  // CHUNK_REFS=4 gives REF_CAP=1024 and $clog2(1025) = 11, exactly the
  // hardcoded value, so this changes no shipped bit and the block's `-MapOnly`
  // row is a like-for-like against the one taken before it.
  localparam int unsigned REF_CAP    = CHUNKS * CHUNK_REFS;
  localparam int unsigned CNT_W      = `ZHAO_GEOM_BINNER_V2_CNT_W;
  localparam int unsigned SLOT_W     = $clog2(CHUNK_REFS);
  // `max_tile_list_depth_o` is a 16-bit instrument port, so a count wider than
  // 16 bits could not be reported even though it could be stored. Refuse that
  // parameterisation at elaboration rather than truncate an instrument, which
  // is the broken-instrument law's own direction: a depth that reads LOW.
  localparam int unsigned DEPTH_W    = 16;
  localparam int unsigned REF_AW     = CHUNK_W + SLOT_W;
  localparam int unsigned TILE_ENT_W = CNT_W + CHUNK_W + CHUNK_W;
  localparam int unsigned TRI_ENT_W  = 16 + 6*21;
  localparam int unsigned META_SLICE_W = 40;
  localparam int unsigned META_SLICES  = (METAW + META_SLICE_W - 1) / META_SLICE_W;
  localparam int unsigned META_PHYS_W  = META_SLICES * META_SLICE_W;
  localparam int unsigned META_PAD_W   = META_PHYS_W - METAW;

  // Quartus 17 accepts elaboration guards inside initial blocks, not bare
  // module-scope conditional statements. Keep the admissible surface simple:
  // metadata must be non-empty and the computed physical image must cover it.
  initial begin
    if (METAW == 0) $fatal(1, "zhao_geom_binner_v2: METAW must be positive");
    if (META_PHYS_W < METAW)
      $fatal(1, "zhao_geom_binner_v2: metadata physical width underflow");
    // ---- the derived-width guards (GIANTREFS) ---------------------------
    // These do not defend against a bad edit of the two localparams above;
    // they defend against a PARAMETERISATION the derivation cannot express.
    if (CHUNK_REFS != (32'd1 << SLOT_W))
      $fatal(1, "zhao_geom_binner_v2: CHUNK_REFS must be a power of two");
    // CHUNK_W addresses the arena; CHUNKS need not be a power of two (the
    // Packet-D pair bench runs CHUNKS=6, CHUNK_W=3 deliberately), but it must
    // fit, or `push_chunk` cannot name every chunk the arena will grant.
    if (CHUNKS > (32'd1 << CHUNK_W))
      $fatal(1, "zhao_geom_binner_v2: CHUNK_W cannot address CHUNKS");
    if (CNT_W > DEPTH_W)
      $fatal(1, "zhao_geom_binner_v2: reference capacity exceeds the 16-bit max_tile_list_depth_o instrument");
    // The count must be able to COUNT the arena it indexes. This is the guard a
    // hand-set CNT_W fails, and it is disabled in the committed mutant ONLY --
    // an elaboration $fatal would stop the simulation before the drain could be
    // measured, and the drain is the evidence that ships. The guard firing on
    // the mutant is independent corroboration, not the measurement. (And
    // `--lint-only` does not run initial blocks, so a clean lint says nothing
    // whatever about this line; CLAUDE.md, the committed-mutant section.)
`ifndef ZHAO_GEOM_BINNER_V2_NO_CNTW_GUARD
    if (CNT_W < $clog2(REF_CAP + 1))
      $fatal(1, "zhao_geom_binner_v2: CNT_W cannot count REF_CAP references without wrapping");
`endif
  end

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
  logic [TRI_W-1:0]      ref_ram  [0:REF_CAP-1];
  logic [CHUNK_W-1:0]    next_ram [0:CHUNKS-1];
  logic [TILE_ENT_W-1:0] tile_ram [0:TILES-1];

  // The Packet-D bank is physically META_SLICES independent 40-bit RAMs. At
  // the default METAW=1157 this is exactly 29 ascending slices and the image is
  // {3'b0, metadata1157}; no pointer or caller cookie is stored beside it.
  //
  // THE COMPOSED CONSOLE NO LONGER PASSES THE DEFAULT. Owner decision R234 D1
  // (2026-09-21) gave `zhao_geom_bin_pipe_v2` three more attribute planes, so
  // it instantiates this block with METAW = 1877: 47 slices of 40 x TRI_CAP,
  // and the pad is still exactly three bits (1880 - 1877), which is why the
  // waiver below and the two-bit profile-verdict guard both hold unchanged.
  // The DEFAULT stays 1157 deliberately -- it is what this block's own directed
  // bench and `tests/tools/test_render_texture_packet_d.py` pin, and a
  // parameter default that tracks its one caller stops being a parameter.
  // The top physical pad has no ABI-visible consumer. Keep the full vector so
  // each generated slice is exactly 40 bits, and waive only the deliberate
  // unexported pad bits (three bits at the Packet-D default).
  //
  // This said "by construction" until 2026-09-18 and named nothing, which is
  // the phrasing the ledger's V20 rule exists to refuse -- it is the same
  // sentence shape as the two claims in this repository that turned out false.
  // The enforcement is real and was simply never pointed at: the consumer
  // elaborates `p_packet_d_contract`, which `$fatal`s unless
  // `$bits(job_meta_i) == 1157`. The pad is `META_PHYS_W - METAW`, so it sits
  // above the width that check pins, and widening the ABI to reach it fails
  // elaboration rather than silently exporting the pad.
  // ENFORCED-BY: fpga/rtl/raster/zhao_raster_tile_pipe_v2.sv:p_packet_d_contract
  logic [META_PHYS_W-1:0] meta_wd;
  /* verilator lint_off UNUSEDSIGNAL */
  logic [META_PHYS_W-1:0] meta_q;
  /* verilator lint_on UNUSEDSIGNAL */
  logic [TRI_W-1:0]       meta_ra;

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

  // Explicit generate keeps Quartus 17 away from a zero-repeat concatenation
  // when a legal METAW is already aligned to the 40-bit physical slice width.
  // THE TWO PROFILE VERDICTS, COMPUTED ON THE WRITE SIDE.
  //
  // These bit positions are the Packet-D layout and they are stated in exactly
  // one other place, `zhao_raster_tile_pipe_v2`'s `profile_aux_bad_w` and
  // `profile_area_bad_w`. Two copies of one fact is the shape this repository
  // keeps finding gone stale, so the consumer keeps its original expressions
  // as a SIMULATION-ONLY equivalence assertion against the bits this port
  // delivers. The indices cannot drift apart without a test failing on the
  // first job that exercises them.
  logic meta_aux_bad_c, meta_area_bad_c;
  assign meta_aux_bad_c  = tri_meta_i[268] || (tri_meta_i[267:44] != 224'd0);
  assign meta_area_bad_c = (tri_meta_i[424:378] == 47'd0);

  // Two pad bits are needed. The elaboration guard is not decorative: at a
  // METAW that happens to align to the 40-bit slice there is no pad at all,
  // and this feature would silently write into the ABI.
  initial begin
    if (META_PAD_W < 2)
      $fatal(1, "zhao_geom_binner_v2: METAW=%0d leaves META_PAD_W=%0d; the profile verdicts need 2 pad bits",
             METAW, META_PAD_W);
  end

  generate
    if (META_PAD_W == 2) begin : g_meta_pad_exact
      assign meta_wd = {meta_area_bad_c, meta_aux_bad_c, tri_meta_i};
    end else begin : g_meta_pad
      assign meta_wd = {{(META_PAD_W-2){1'b0}},
                        meta_area_bad_c, meta_aux_bad_c, tri_meta_i};
    end
  endgenerate
  assign meta_ra = `ZHAO_GEOM_BINNER_V2_META_RA(tri_ra);

  genvar gm;
  generate
    for (gm = 0; gm < META_SLICES; gm = gm + 1) begin : g_meta_slice
      logic [META_SLICE_W-1:0] meta_ram [0:TRI_CAP-1];
      always_ff @(posedge clk) begin
        // Exact same accepted-and-stored triangle event as tri_ram.
        if (tri_we)
          meta_ram[tri_wa] <= meta_wd[gm*META_SLICE_W +: META_SLICE_W];
        // Exact same transaction address as tri_ram in production. The selector
        // affects only this expression in the committed inverse-control build.
        meta_q[gm*META_SLICE_W +: META_SLICE_W] <= meta_ram[meta_ra];
      end
    end
  endgenerate

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
  logic [METAW-1:0]     d_meta_r;
  // Registered on the SAME edge as d_meta_r, out of the SAME word, so the
  // verdict and the metadata it describes can never be one job apart.
  logic [1:0]           d_profile_bad_r;
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

  // THE EDGE-SLOPE TIMES TILE-OFFSET PRODUCT, MULTIPLIED AT ITS REAL WIDTH.
  //
  // `kx_r`/`ky_r` are ACC_W = 36 bits wide because they are ACCUMULATORS, and
  // they must be. But the slope they hold is `ext23()` of a 23-bit input and is
  // assigned NOWHERE ELSE, so bits [35:23] are sign extension and carry no
  // information. Multiplying the 36-bit register handed Quartus a 36x11 shape.
  //
  // MEASURED 2026-08-24, asymmetric calibration grid:
  //     23 x 11 -> 1 DSP        32 x 27 -> 3 DSPs
  // and this block mapped 12 DSPs for four such products, i.e. 3 each. At the
  // real width they are 1 each: 12 -> 4.
  //
  // EXACT, not an approximation. The register's VALUE is by construction equal
  // to the sign-extension of its low 23 bits, so `k * t` and `k[22:0] * t` are
  // the same integer, and every downstream truncation applies identically.
  // This is width HYGIENE -- no bound is being assumed, no domain narrowed, and
  // nothing here depends on world size or any owner ruling.
  //
  // ENFORCED-BY: tests/geometry/geom_binner_directed.cpp
  // Takes the slope at its REAL 23-bit width rather than the accumulator
  // width, so the narrowing is visible at every call site instead of hidden
  // inside the function -- and so `-Wall` cannot object that 13 bits of an
  // argument go unread, which is exactly what it did when this took ACC_W.
  function automatic logic signed [ACC_W-1:0] k_mul_tile(
      input logic signed [22:0] k,
      input logic signed [10:0] t);
    logic signed [33:0] p;
    p = k * t;
    k_mul_tile = $signed({{(ACC_W-34){p[33]}}, p});
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
  // The raster job port is SILENT during a serialise pass: the same walk, the
  // same registers, one consumer at a time.
  assign job_valid_o  = d_job_v;
  assign job_ax_o     = $signed(d_tri_r[20:0]);
  assign job_ay_o     = $signed(d_tri_r[41:21]);
  assign job_bx_o     = $signed(d_tri_r[62:42]);
  assign job_by_o     = $signed(d_tri_r[83:63]);
  assign job_cx_o     = $signed(d_tri_r[104:84]);
  assign job_cy_o     = $signed(d_tri_r[125:105]);
  assign job_src_id_o = d_tri_r[141:126];
  assign job_meta_o   = d_meta_r;
  assign job_profile_bad_o = d_profile_bad_r;
  // RASTER.EDGEWALK's `job_tile_x_i` is "the tile origin — the top-left PIXEL
  // of the 16×16 tile" (its contract, Input packet layouts), NOT a tile index.
  // The drain port is that port field for field, so the index is scaled here,
  // once, and the pixel is what leaves the block. (Emitting the index instead
  // is a silent 16× error that no differential against a tile-indexed oracle
  // would ever see; it took the zhao_geom_bin_pipe composition — a real
  // rasterized picture — to catch it, which is exactly why that composition
  // was built. tests/geometry/geom_binner_directed.cpp:test_pixel_origin now
  // pins it directly.)
  // `d_rem_r` is the number of references LEFT in this tile including the one
  // being emitted, so 1 means last. `d_first_r` is set when a list is opened
  // and cleared on its first accepted emit.
  assign job_first_o  = d_first_r;
  assign job_last_o   = (d_rem_r == {{(CNT_W-1){1'b0}}, 1'b1});
  // THE WALK HAS ONE CONSUMER AGAIN. `take_c` used to be
  // `ser_mode_r ? ser_ready_i : job_ready_i`; with the serialise pass retired
  // the raster drain is the only reader of this walk, so the mux is gone and
  // with it the register that selected it.
  assign take_c       = job_ready_i;
  assign job_tile_x_o = $signed({2'd0, d_jx_r, 4'd0});
  assign job_tile_y_o = $signed({2'd0, d_jy_r, 4'd0});
  assign drain_busy_o = (state >= D_TILE) && (state <= D_EMIT);
  assign drain_done_o = drain_done_r;

  // ---------------------------------------------------------- sequential ---
  // The two cursors advance in exactly one place each, driven by these two
  // predicates, so no register is written from two different case arms.
  logic d_first_r; // this emit is the first reference of its tile
  logic adv;       // finish this tile and move the BIN cursor
  logic nxt_tile;  // finish this tile and move the DRAIN cursor
  logic take_c;    // the consumer accepted, whichever consumer it is

  always_comb begin
    adv      = (state == S_TILE) ? !tile_keep : ((state == S_PUSH) && push_ok);
    nxt_tile = ((state == D_HEAD) && (cur_count == {CNT_W{1'b0}})) ||
               ((state == D_EMIT) && d_job_v && take_c &&
                (d_rem_r == {{(CNT_W-1){1'b0}}, 1'b1}));
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state        <= S_CLEAR;
      d_first_r    <= 1'b1;
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
      d_meta_r     <= {METAW{1'b0}};
      d_profile_bad_r <= 2'b00;
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
              ep_r[k]  <= ep_r[k] + k_mul_tile($signed(kx_r[k][22:0]), $signed({1'b0, tx0_r, 4'd0})) +
                                    k_mul_tile($signed(ky_r[k][22:0]), $signed({1'b0, ty0_r, 4'd0}));
              epr_r[k] <= ep_r[k] + k_mul_tile($signed(kx_r[k][22:0]), $signed({1'b0, tx0_r, 4'd0})) +
                                    k_mul_tile($signed(ky_r[k][22:0]), $signed({1'b0, ty0_r, 4'd0}));
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
              // WAS `{5'd0, cur_count}`, a zero-extension whose pad width
              // encoded CNT_W == 11 in a place no reader of the localparam
              // would look. Derive the extension from the instrument's own
              // width; the elaboration guard above refuses CNT_W > DEPTH_W, so
              // this cast is never a truncation. (GIANTREFS)
              if ((DEPTH_W'(cur_count) + 16'd1) > max_depth)
                max_depth <= DEPTH_W'(cur_count) + 16'd1;
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
            d_first_r <= 1'b1;
            state     <= D_REF;
          end

          D_REF: state <= D_TRI;   // ref_ram read issued combinationally

          D_TRI: state <= D_EMIT;  // ref_q valid; tri_ram read issued

          D_EMIT: begin
            if (!d_job_v) begin
              d_tri_r  <= tri_q;
              d_meta_r <= meta_q[METAW-1:0];
              d_profile_bad_r <= meta_q[METAW+1 -: 2];
              d_job_v  <= 1'b1;
            end else if (take_c) begin
              d_job_v   <= 1'b0;
              d_first_r <= 1'b0;
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

          // THE SERIALISE PASS WAS ARMED HERE AND IS RETIRED (ARENACOMPOSE).
          // `ser_req_i` was sampled at this state and nowhere else, so a pass
          // could only ever follow a completed raster drain. With the pass
          // gone the drain simply finishes.
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

endmodule : zhao_geom_binner_v2

`undef ZHAO_GEOM_BINNER_V2_META_RA
