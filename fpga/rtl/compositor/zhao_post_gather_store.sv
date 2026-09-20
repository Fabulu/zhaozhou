// zhao_post_gather_store.sv -- the quarter-resolution EFFECT PLANE, between
// POST.GATHER's tile flush and POST.COMPOSITE's random access.
//
// ===========================================================================
// THE GAP THIS CLOSES, AND WHO NAMED IT
// ===========================================================================
// `zhao_console_core.sv` entry I17 refused POST.GATHER three times. Two of
// the three reasons are now gone -- the input-side objection was WITHDRAWN
// (it named the wrong stream), and the art law was RATIFIED by owner ruling
// R195. The third was found by packet POSTMEAS on 2026-09-20 and is the one
// R195 does not touch:
//
//   "THE FLUSH STREAM CARRIES NO ADDRESS. `c_index_o` is FOUR BITS, 0..15,
//    'within the tile'. `u_post_composite` reads a plane by {`gd_view_o`,
//    `gd_cx_o`, `gd_cy_o`} -- absolute, per view. `zhao_post_gather` has NO
//    tile-x, NO tile-y and NO view port at all ... And the contract does not
//    name the mechanism either: its whole statement of this seam is one
//    sentence, 'Writes out for POST.COMPOSITE'."
//
// That is correct and this file is the answer. The mechanism is named here,
// in the block that owns it: POST.GATHER stays tile-local by R5 and knows
// nothing about the screen, and the ADDRESS is formed HERE, from the tile
// origin the raster already publishes.
//
// ===========================================================================
// WHERE THE ORIGIN COMES FROM, AND WHY IT IS NOT A NEW SIGNAL
// ===========================================================================
// `zhao_raster_tile_pipe` already emits `fb_x_o`/`fb_y_o` -- "SURFACE pixel x
// of this beat" -- beside `fb_addr_o` = {row[3:0], col[3:0]} in-tile. So on
// EVERY accepted beat the tile origin is
//
//     (fb_x - fb_addr[3:0], fb_y - fb_addr[7:4])
//
// a four-bit subtract, and it needs no tile-start handshake, no side channel
// and no new counter in the raster. Entry I17 proposed latching the origin at
// a tile-start pulse; this is the same number obtained without the pulse, and
// therefore without a second thing that can be one cycle out.
//
// ===========================================================================
// THE ORIGIN PIPELINE IS TWO DEEP, AND THAT IS THE WHOLE CORRECTNESS ARGUMENT
// ===========================================================================
// POST.GATHER ping-pongs. `tile_start_i` swaps banks; `tile_flush_i` drains
// the OTHER bank while the NEW tile accumulates into the fresh one. So at the
// moment cells are written to this plane, the tile whose origin they belong
// to is ALREADY CLOSED and a new one may be streaming.
//
// Writing them at the CURRENT origin is therefore exactly `CLAUDE.md`'s
// metadata-swap defect: the right cells at the wrong address, one tile late,
// with every counter balancing because no counter looks at the field that
// moved. It would read on screen as every halo displaced by one tile -- and
// it would read as nothing at all on a frame with one bright tile in it.
//
// So there are two registers:
//
//     org_cur_q     the tile currently STREAMING   (loaded by `org_valid_i`)
//     org_flush_q   the tile currently FLUSHING    (loaded by `org_close_i`)
//
// and writes use `org_flush_q`. `org_close_i` is the SAME pulse that swaps
// POST.GATHER's banks, which is what keeps the two blocks agreeing about
// which tile is which.
//
// AND THE DETECTOR FOR IT IS WIRED TO TWO OPERANDS THAT DO NOT MOVE TOGETHER,
// which `CLAUDE.md` says is the question to ask of any checker. `w_busy_i` is
// POST.GATHER's own `flush_busy_o` -- it advances on the flush walk, sixteen
// clocks. `org_close_i` comes from the RASTER's tile cadence, 256 pixels plus
// whatever the framebuffer writer does to them. Nothing clocks both. If a
// tile ever closes while a flush is still draining, `org_flush_q` would move
// out from under the burst, and `flush_overrun_o` is the instrument that
// says so. It reads zero in the composed console because 256 >> 16, and that
// is a claim the counter checks rather than a comment that asserts it.
//
// ===========================================================================
// THE ADDRESS MAP, AND WHY IT IS A MULTIPLY AND NOT A CONCATENATION
// ===========================================================================
// A concatenated {cy, cx} address is free and needs a power-of-two row
// stride. The largest cell-x this console forms is 95 (Z60, 384 px wide), so
// that stride would be 128, the plane would be 128 x 96 = 12,288 cells, and
// at 33 bits it would take FIFTY-FOUR M10K against `POST.GATHER.md`'s ceiling
// of thirty. A 7x7 multiply by the row stride costs a few dozen ALMs and
// brings the plane to what it actually occupies:
//
//   | mode  | canvas cells      | used  |
//   |-------|-------------------|-------|
//   | Z60   | 96 x 60           | 5,760 |
//   | Storm | 80 x 60           | 4,800 |
//   | Duo   | 64 x 96 (2 views) | 6,144 |
//
// so `CELLS` = 8,192 covers every mode with headroom. Split across the two
// read clients that is 8,192 x 16 = 13 M10K for displacement and 8,192 x 17 =
// 14 M10K for glow+ink -- TWENTY-SEVEN, under the ceiling.
//
// THE SPLIT IS NOT COSMETIC. One memory with one write port and TWO read
// ports is not an M10K; `gd_*` and `gg_*` are separate clients reading
// DIFFERENT coordinates on the same beat (composite's own comment: the glow
// plane is addressed by the DISPLACED coordinate, "if this ever becomes
// x1_q/y1_q the outline stops following the creature"). Two simple dual-port
// memories infer cleanly; one three-ported one does not infer at all.
//
// ===========================================================================
// DUO PUTS THE SECOND VIEW BELOW THE FIRST, NOT BESIDE IT
// ===========================================================================
// Easy to get backwards, and getting it backwards puts player two's bloom on
// player one's screen. `zhao_post_lease` reads ONE TALL SOURCE per frame --
// `rd_h_c = duo_i ? (frame_h << 1) : frame_h` -- so the canvas is 256 wide by
// 384 tall and view 1 occupies rows 192..383. `zhao_pkg`'s
// ZHAO_CANVAS_BYTES_DUO = 2*256*192*2 agrees.
//
// The WRITE side is in canvas coordinates already (the raster renders into
// that canvas), so it needs no view at all. The READ side is VIEW-LOCAL --
// POST.COMPOSITE's "the pass IS a view" -- so it adds `view_rows_i` for view
// 1 and nothing for view 0. One add, on the read side only.
//
// ===========================================================================
// ONE PLANE, NOT TWO, AND THE INSTRUMENT THAT CHECKS IT
// ===========================================================================
// The raster phase WRITES this plane and the post phase READS it, and
// `zhao_post_lease` makes those disjoint: `raster_done_c` gates `start_c`,
// and the shell's `rpx_ready` is `!post_phase_w && fbw_px_ready`. So a single
// plane is correct and a second one would be 27 M10K spent on a hazard that
// cannot occur.
//
// THAT IS A CLAIM, AND `rdw_collide_o` IS THE CLAIM'S INSTRUMENT. It counts
// any clock on which a write and a read name the same cell. A detector
// reading zero is the claim to check hardest, so it is fired deliberately in
// the directed bench rather than quoted from the composed machine -- where it
// SHOULD read zero, and where a non-zero value means the phase interlock has
// broken and the bloom is sampling a plane being rewritten under it.
//
// ===========================================================================
// WHAT THIS BLOCK IS NOT
// ===========================================================================
// PART A, the separable blur, is NOT here. `POST.COMPOSITE.md` calls it "a
// separable blur over the compact glow plane: one horizontal and one vertical
// quarter-res pass", `zhao_post_composite.sv`'s header calls it OPTIONAL and
// names the seam precisely -- "THE SEAM IS `gg_*`: a blur module sits between
// POST.GATHER's plane and that port, or nothing does". Today nothing does,
// the glow reaches composite CELL-QUANTISED, and that is DECLARED in the
// core's INCOMPLETE block rather than left to be discovered by looking at a
// frame. R195 ratified TWO passes as the law; the module that performs them
// is owed and is not this one.
// ===========================================================================
`default_nettype none

module zhao_post_gather_store #(
    // Plane capacity in cells. 8,192 covers every mode (see the table in the
    // header) with 2,048 spare. It is a knob because the day a mode gets
    // wider, this is the number that has to move and nothing else does.
    parameter int unsigned CELLS = 8192,
    // Cell-coordinate width. 7 bits reaches 127, and the widest canvas this
    // console forms is 96 cells across by 96 down.
    parameter int unsigned CW    = 7,
    // Derived; declared here because a port list cannot see a localparam.
    parameter int unsigned AW    = $clog2(CELLS)
) (
    input  var logic          clk,
    input  var logic          rst_n,

    // ---- plane geometry, from the video mode --------------------------------
    // Cells, not pixels. The core divides the view's pixel size by four here
    // rather than inside the block, so that the one place the quarter-
    // resolution ruling is applied is the one place it can be read.
    input  var logic [CW-1:0] plane_w_cells_i,  // cells per CANVAS row
    input  var logic [CW-1:0] plane_rows_i,     // cell rows in the whole canvas
    input  var logic [CW-1:0] view_rows_i,      // cell rows in ONE view

    // ---- the tile origin, from the raster's own surface coordinates ---------
    // `org_valid_i` is an accepted fragment beat; `org_cx_i`/`org_cy_i` are
    // its tile's origin IN CELLS. `org_close_i` is the pulse that also swaps
    // POST.GATHER's banks -- see the header: the two blocks must agree about
    // which tile is closing, and this is how.
    input  var logic          org_valid_i,
    input  var logic [CW-1:0] org_cx_i,
    input  var logic [CW-1:0] org_cy_i,
    input  var logic          org_close_i,
    input  var logic          w_busy_i,         // POST.GATHER's `flush_busy_o`

    // ---- the flushed cells, one per clock, from POST.GATHER -----------------
    input  var logic          w_valid_i,
    input  var logic [3:0]    w_index_i,        // {cy_in_tile[1:0], cx_in_tile[1:0]}
    input  var logic [15:0]   w_glow_i,         // RGB565
    input  var logic signed [7:0] w_dx_i,       // integer pixels, [-8,+8]
    input  var logic signed [7:0] w_dy_i,       // integer pixels, [-4,+4]
    input  var logic          w_ink_i,

    // ---- the plane's lifetime ----------------------------------------------
    // A plane that has not been gathered yet must answer NOT PRESENT rather
    // than answer zero, because POST.COMPOSITE counts the difference and
    // because an absent output must not look like a zero result (W10). The
    // first frame of a run therefore has no bloom, honestly, instead of a
    // bloom built from whatever the memory powered up holding.
    input  var logic          plane_open_i,     // a new frame's raster begins
    input  var logic          plane_commit_i,   // the frame's gather is complete

    // ---- read A: DISPLACEMENT, POST.COMPOSITE stage 0 -> stage 1 -----------
    // "address out in cycle N, data in cycle N+1", and when the pipeline
    // stalls the address holds, so the registered response holds with it. A
    // synchronous memory does this for free.
    input  var logic          gd_req_v_i,
    input  var logic          gd_view_i,
    input  var logic [CW-1:0] gd_cx_i,
    input  var logic [CW-1:0] gd_cy_i,
    output var logic          gd_present_o,
    output var logic signed [7:0] gd_dx_o,
    output var logic signed [7:0] gd_dy_o,

    // ---- read B: GLOW and INK, at the DISPLACED coordinate ------------------
    input  var logic          gg_req_v_i,
    input  var logic          gg_view_i,
    input  var logic [CW-1:0] gg_cx_i,
    input  var logic [CW-1:0] gg_cy_i,
    output var logic          gg_present_o,
    output var logic [15:0]   gg_glow_o,
    output var logic          gg_ink_o,

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0]   cells_written_o,
    output var logic [31:0]   oob_writes_o,     // a cell outside the plane
    output var logic [31:0]   gd_reads_o,
    output var logic [31:0]   gg_reads_o,
    output var logic [31:0]   gd_miss_o,        // answered NOT PRESENT
    output var logic [31:0]   gg_miss_o,
    output var logic [31:0]   flush_overrun_o,  // a tile closed mid-flush
    output var logic [31:0]   rdw_collide_o,    // the phase-interlock instrument
    output var logic [31:0]   plane_commits_o
);

  // ---- the two planes -----------------------------------------------------
  // One write port and one read port each, which is what infers a simple
  // dual-port M10K. Read-during-write returns OLD data; the header says why
  // that is irrelevant here and `rdw_collide_o` is the check on the reason.
  logic [15:0] disp_mem [0:CELLS-1];   // {dy, dx}
  logic [16:0] glow_mem [0:CELLS-1];   // {ink, glow565}

  // ---- the origin pipeline ------------------------------------------------
  logic [CW-1:0] org_cur_cx_q, org_cur_cy_q;
  logic [CW-1:0] org_fl_cx_q,  org_fl_cy_q;
  logic          plane_valid_q;

  // ---- write address ------------------------------------------------------
  // `w_index_i` is POST.GATHER's own {y[1:0], x[1:0]} within the 4x4 cell
  // block a 16x16 tile covers, so the cell is the flushing tile's origin plus
  // those two pairs of bits. No shift: the origin is already in cells.
  logic [CW:0]   w_cx_c, w_cy_c;
  logic [CW+CW:0] w_lin_c;
  logic [AW-1:0] w_addr_c;
  logic          w_in_range_c;

  always_comb begin
    w_cx_c  = {1'b0, org_fl_cx_q} + (CW+1)'(w_index_i[1:0]);
    w_cy_c  = {1'b0, org_fl_cy_q} + (CW+1)'(w_index_i[3:2]);
    w_lin_c = ((CW+CW+1)'(w_cy_c) * (CW+CW+1)'(plane_w_cells_i))
            +  (CW+CW+1)'(w_cx_c);
    w_in_range_c = (w_cx_c < {1'b0, plane_w_cells_i})
                && (w_cy_c < {1'b0, plane_rows_i})
                && (w_lin_c < (CW+CW+1)'(CELLS));
    w_addr_c = w_lin_c[AW-1:0];
  end

  // ---- read addresses -----------------------------------------------------
  // The view offset is added HERE and only here. View 0 is rows 0..view_rows-1
  // of the canvas and view 1 is the rows above that; see the header for why
  // Duo stacks and does not tile side by side.
  function automatic logic [CW:0] view_row(input logic v,
                                           input logic [CW-1:0] cy,
                                           input logic [CW-1:0] vrows);
    view_row = {1'b0, cy} + (v ? {1'b0, vrows} : (CW+1)'(0));
  endfunction

  logic [CW:0]    gd_cy_e_c, gg_cy_e_c;
  logic [CW+CW:0] gd_lin_c,  gg_lin_c;
  logic [AW-1:0]  gd_addr_c, gg_addr_c;
  logic           gd_in_c,   gg_in_c;

  always_comb begin
    gd_cy_e_c = view_row(gd_view_i, gd_cy_i, view_rows_i);
    gd_lin_c  = ((CW+CW+1)'(gd_cy_e_c) * (CW+CW+1)'(plane_w_cells_i))
              +  (CW+CW+1)'(gd_cx_i);
    gd_in_c   = (gd_cx_i < plane_w_cells_i)
             && (gd_cy_e_c < {1'b0, plane_rows_i})
             && (gd_lin_c < (CW+CW+1)'(CELLS));
    gd_addr_c = gd_lin_c[AW-1:0];

    gg_cy_e_c = view_row(gg_view_i, gg_cy_i, view_rows_i);
    gg_lin_c  = ((CW+CW+1)'(gg_cy_e_c) * (CW+CW+1)'(plane_w_cells_i))
              +  (CW+CW+1)'(gg_cx_i);
    gg_in_c   = (gg_cx_i < plane_w_cells_i)
             && (gg_cy_e_c < {1'b0, plane_rows_i})
             && (gg_lin_c < (CW+CW+1)'(CELLS));
    gg_addr_c = gg_lin_c[AW-1:0];
  end

  logic          w_go_c;
  assign w_go_c = w_valid_i && w_in_range_c;

  // ---- the memories -------------------------------------------------------
  logic [15:0] disp_rd_q;
  logic [16:0] glow_rd_q;

  always_ff @(posedge clk) begin
    if (w_go_c) disp_mem[w_addr_c] <= {w_dy_i, w_dx_i};
    disp_rd_q <= disp_mem[gd_addr_c];
  end

  always_ff @(posedge clk) begin
    if (w_go_c) glow_mem[w_addr_c] <= {w_ink_i, w_glow_i};
    glow_rd_q <= glow_mem[gg_addr_c];
  end

  assign gd_dx_o   = disp_rd_q[7:0];
  assign gd_dy_o   = disp_rd_q[15:8];
  assign gg_glow_o = glow_rd_q[15:0];
  assign gg_ink_o  = glow_rd_q[16];

  // ---- control, lifetime and evidence -------------------------------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      org_cur_cx_q <= '0;
      org_cur_cy_q <= '0;
      org_fl_cx_q  <= '0;
      org_fl_cy_q  <= '0;
      plane_valid_q <= 1'b0;
      gd_present_o  <= 1'b0;
      gg_present_o  <= 1'b0;
      cells_written_o <= '0;
      oob_writes_o    <= '0;
      gd_reads_o      <= '0;
      gg_reads_o      <= '0;
      gd_miss_o       <= '0;
      gg_miss_o       <= '0;
      flush_overrun_o <= '0;
      rdw_collide_o   <= '0;
      plane_commits_o <= '0;
    end else begin
      // The streaming tile's origin, refreshed on every accepted beat. It is
      // the same value all 256 times, which is what makes it safe to take
      // from a fragment instead of from a handshake.
      if (org_valid_i) begin
        org_cur_cx_q <= org_cx_i;
        org_cur_cy_q <= org_cy_i;
      end

      // The tile closes: what was streaming is now what is flushing.
      if (org_close_i) begin
        org_fl_cx_q <= org_cur_cx_q;
        org_fl_cy_q <= org_cur_cy_q;
        // ...and if a flush was still running, that move corrupts it. Two
        // operands, two clocks, nothing in common -- see the header.
        if (w_busy_i) flush_overrun_o <= flush_overrun_o + 32'd1;
      end

      // Lifetime. `plane_open_i` wins over `plane_commit_i` on the same clock
      // because a frame that has just started gathering has NOT got a plane,
      // and the flattering direction here is the wrong one.
      if (plane_open_i) begin
        plane_valid_q <= 1'b0;
      end else if (plane_commit_i) begin
        plane_valid_q   <= 1'b1;
        plane_commits_o <= plane_commits_o + 32'd1;
      end

      // Writes.
      if (w_valid_i) begin
        if (w_in_range_c) cells_written_o <= cells_written_o + 32'd1;
        else              oob_writes_o    <= oob_writes_o    + 32'd1;
      end

      // Reads, and the presence answer that rides with the memory's own
      // one-clock latency. `plane_valid_q` is sampled on the REQUEST clock,
      // beside the address, so the answer and the address describe the same
      // moment.
      gd_present_o <= gd_req_v_i && gd_in_c && plane_valid_q;
      gg_present_o <= gg_req_v_i && gg_in_c && plane_valid_q;

      if (gd_req_v_i) begin
        gd_reads_o <= gd_reads_o + 32'd1;
        if (!(gd_in_c && plane_valid_q)) gd_miss_o <= gd_miss_o + 32'd1;
      end
      if (gg_req_v_i) begin
        gg_reads_o <= gg_reads_o + 32'd1;
        if (!(gg_in_c && plane_valid_q)) gg_miss_o <= gg_miss_o + 32'd1;
      end

      // THE PHASE-INTERLOCK INSTRUMENT. One count per colliding clock, not
      // one per client, because what it reports is "the phases overlapped",
      // and that is one event however many ports saw it.
      if (w_go_c && ((gd_req_v_i && gd_in_c && (gd_addr_c == w_addr_c))
                  || (gg_req_v_i && gg_in_c && (gg_addr_c == w_addr_c))))
        rdw_collide_o <= rdw_collide_o + 32'd1;
    end
  end

  // ---- elaboration guards -------------------------------------------------
  // Inside `initial`, because Quartus 17.0 rejects a bare module-scope `if`
  // and Verilator lints it clean -- CLAUDE.md's build chapter. And note that
  // `--lint-only` never RUNS this, so a clean lint is not evidence about it.
  initial begin
    if (CELLS < 6144)
      $fatal(1, "zhao_post_gather_store: CELLS %0d cannot hold Duo's 64 x 96 = 6,144 cells; the plane would silently drop the bottom of player two's view.", CELLS);
    if ((1 << CW) < 96)
      $fatal(1, "zhao_post_gather_store: CW %0d cannot express cell-x 95 (Z60's 384-pixel row); the right of the screen would wrap onto the left.", CW);
  end

endmodule : zhao_post_gather_store

`default_nettype wire
