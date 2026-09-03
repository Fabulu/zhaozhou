// zhao_post_gather.sv — the effect-plane gather, tile-local, ruling R5.
//
// ---------------------------------------------------------------------------
// WHAT IT DOES, AND THE ONE RULE THAT SHAPES ALL OF IT
// ---------------------------------------------------------------------------
// Resolved fragments carry effect tags: glow from emissive material, a
// displacement from refraction/shockwave/heat-haze, and an exterior-ink bit
// from the creature outline. This block collects them into a quarter-
// resolution plane for POST.COMPOSITE.
//
//   > POST.GATHER must never backpressure RASTER.RESOLVE.   (R5)
//
// Everything below follows from that. If this block could stall resolve it
// would stop being a side channel and become a throughput term in the
// renderer, and the renderer's rate is the console's rate.
//
// ---------------------------------------------------------------------------
// TWO LEVELS, WHICH IS WHAT THE RULING FIXED
// ---------------------------------------------------------------------------
// The contract's storage arithmetic used to be internally contradictory: one
// accumulation format, one storage format and one ceiling that could not all
// be true together. R5 separates them.
//
// LEVEL 1, tile-local, IN REGISTERS. A 16x16 pixel tile maps to exactly 4x4
// effect cells. Two ping-pong banks of sixteen cells. Per cell:
//
//     glow_r/g/b        u16, SATURATING
//     displacement_x/y  signed 8.8 in a WIDE saturating s16
//     ink               1 bit, OR
//
// A resolved fragment updates AT MOST ONE CELL PER PLANE.
//
//   > No global M10K read-modify-write on the resolve path.   (R5)
//
// That clause is what makes "never backpressure resolve" structural rather
// than aspirational: an M10K read-modify-write at resolve rate would put the
// gather inside the renderer's timing no matter what the ready line said.
//
// LEVEL 2, the global effect cell, 33 bits: glow RGB565, displacement X and Y
// as signed i8, exterior ink 1 bit.
//
// AT TILE FLUSH: glow rounds and clamps ONCE into RGB565; displacement rounds
// ONCE to integer pixels; X clamps to [-8, +8]; Y clamps to [-4, +4]; ink is
// copied.
//
// Those two clamps are not arbitrary. They are the bound POST.COMPOSITE's line
// ring is built against -- nine complete source lines, horizontal +/-8 -- so
// widening them here silently breaks a block that is not this one.
//
// ---------------------------------------------------------------------------
// ACCUMULATE, DO NOT AVERAGE
// ---------------------------------------------------------------------------
// Glow is a SATURATING ADD. One very bright fragment should light the cell;
// averaging would dilute it by however many neighbours happen to be dark.
// Sixteen bits per channel is the headroom that makes bloom look like light
// rather than like clipping, and the single rounding into RGB565 happens at
// flush and nowhere else.
//
// ---------------------------------------------------------------------------
// EVERY TILE WRITES ALL SIXTEEN CELLS, INCLUDING ZEROS
// ---------------------------------------------------------------------------
// So the frame overwrites the active plane and there is no giant reset loop.
// A cell that received nothing is a cell that is zero, and it is written as
// zero rather than left holding last frame's light.
// ---------------------------------------------------------------------------
`default_nettype none

module zhao_post_gather #(
    parameter int unsigned TILE  = 16,   // pixels on a side
    parameter int unsigned CELLS = 4,    // effect cells on a side (TILE / 4)
    // Derived, and declared here because a port list cannot see a localparam.
    // Not intended to be overridden: TILE is the tile, and PXW is how many
    // bits a coordinate inside it needs.
    parameter int unsigned PXW   = $clog2(TILE)
) (
    input var logic clk,
    input var logic rst_n,

    // ---- from RASTER.RESOLVE, and NEVER backpressured ------------------------
    // There is no `ready`. That is the interface saying what the ruling says.
    input  var logic                 f_valid_i,
    // Pixel within the tile. Only the TOP bits choose the cell -- a 16x16 tile
    // over 4x4 cells means the low two bits say where inside the cell the
    // fragment landed, and the gather does not care: R5 says a fragment
    // updates at most one cell per plane, not that it weights it.
    /* verilator lint_off UNUSEDSIGNAL */
    input  var logic [PXW-1:0]       f_x_i,
    input  var logic [PXW-1:0]       f_y_i,
    /* verilator lint_on UNUSEDSIGNAL */
    input  var logic [7:0]           f_glow_r_i,
    input  var logic [7:0]           f_glow_g_i,
    input  var logic [7:0]           f_glow_b_i,
    input  var logic signed [15:0]   f_disp_x_i,   // signed 8.8
    input  var logic signed [15:0]   f_disp_y_i,
    input  var logic                 f_ink_i,

    // ---- tile control --------------------------------------------------------
    // `tile_start_i` swaps banks and clears the one now being accumulated into.
    input  var logic                 tile_start_i,
    input  var logic                 tile_flush_i,
    output var logic                 flush_busy_o,

    // ---- the flushed global cells, one per clock -----------------------------
    output var logic                 c_valid_o,
    output var logic [3:0]           c_index_o,    // 0..15 within the tile
    output var logic [15:0]          c_glow_o,     // RGB565
    output var logic signed [7:0]    c_disp_x_o,   // integer pixels, [-8,+8]
    output var logic signed [7:0]    c_disp_y_o,   // integer pixels, [-4,+4]
    output var logic                 c_ink_o,

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0]          fragments_o,
    output var logic [31:0]          glow_saturations_o,
    output var logic [31:0]          disp_clamps_o,
    output var logic [31:0]          cells_flushed_o
);

  localparam int unsigned NCELL = CELLS * CELLS;      // 16
  localparam int unsigned CIW   = $clog2(NCELL);      // 4
  localparam int unsigned CDW   = $clog2(CELLS);      // 2, bits that pick a cell

  // Displacement clamps, from R5 and from POST.COMPOSITE's line ring.
  localparam int signed DX_MAX =  8;
  localparam int signed DY_MAX =  4;

  // ---- level 1: two banks of sixteen REGISTER cells -----------------------
  logic [15:0]        g_r  [2][NCELL];
  logic [15:0]        g_g  [2][NCELL];
  logic [15:0]        g_b  [2][NCELL];
  logic signed [15:0] d_x  [2][NCELL];
  logic signed [15:0] d_y  [2][NCELL];
  logic               ink  [2][NCELL];

  logic bank_q;          // the bank being accumulated into
  logic flush_v_q;
  logic [CIW:0] flush_i_q;

  assign flush_busy_o = flush_v_q;

  // ---- which cell does this fragment land in? -----------------------------
  // A 16x16 tile maps to 4x4 cells, so the cell is the top two bits of each
  // coordinate. At most ONE cell per plane, per R5.
  logic [CIW-1:0] cell_c;
  assign cell_c = {f_y_i[PXW-1 -: CDW], f_x_i[PXW-1 -: CDW]};

  // ---- saturating adds ----------------------------------------------------
  function automatic logic [16:0] sat_add16(input logic [15:0] a,
                                            input logic [7:0]  b);
    sat_add16 = {1'b0, a} + 17'(b);
  endfunction

  // Signed saturating add in the WIDE lane. The accumulator is s16 and the
  // contributions are s16, so the sum needs s17 before it is clamped back --
  // clamping to the accumulator's own width is what "wide saturating" means
  // and is why three overlapping effects cannot wrap into each other.
  function automatic logic signed [15:0] sat_add_s16(input logic signed [15:0] a,
                                                     input logic signed [15:0] b,
                                                     output logic clamped);
    logic signed [16:0] s;
    begin
      s = $signed({a[15], a}) + $signed({b[15], b});
      clamped = (s > 17'sd32767) || (s < -17'sd32768);
      sat_add_s16 = (s > 17'sd32767) ? 16'sh7FFF
                  : (s < -17'sd32768) ? 16'sh8000
                  : s[15:0];
    end
  endfunction

  // ---- flush: ONE rounding into RGB565, ONE into integer pixels -----------
  // Glow accumulates in u16 and is packed once, here. Accumulating in the
  // packed format would lose the headroom that makes bloom read as light.
  function automatic logic [4:0] to5(input logic [15:0] v);
    to5 = (v > 16'd255) ? 5'd31 : v[7:3];
  endfunction
  function automatic logic [5:0] to6(input logic [15:0] v);
    to6 = (v > 16'd255) ? 6'd63 : v[7:2];
  endfunction

  // signed 8.8 -> integer pixels, round-half-up, then clamp.
  function automatic logic signed [7:0] to_px(input logic signed [15:0] v,
                                              input int signed lim,
                                              output logic clamped);
    logic signed [16:0] r;
    begin
      r = ($signed({v[15], v}) + 17'sd128) >>> 8;
      clamped = (r > 17'(lim)) || (r < -17'(lim));
      to_px = (r > 17'(lim)) ? 8'(lim) : (r < -17'(lim)) ? 8'(-lim) : r[7:0];
    end
  endfunction

  logic [CIW-1:0] fi_c;
  assign fi_c = flush_i_q[CIW-1:0];

  logic dxc_c, dyc_c;
  logic signed [7:0] px_c, py_c;
  always_comb begin
    px_c = to_px(d_x[~bank_q][fi_c], DX_MAX, dxc_c);
    py_c = to_px(d_y[~bank_q][fi_c], DY_MAX, dyc_c);
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      bank_q    <= 1'b0;
      flush_v_q <= 1'b0;
      flush_i_q <= '0;
      c_valid_o <= 1'b0;
      fragments_o        <= '0;
      glow_saturations_o <= '0;
      disp_clamps_o      <= '0;
      cells_flushed_o    <= '0;
      for (int b = 0; b < 2; b++)
        for (int i = 0; i < NCELL; i++) begin
          g_r[b][i] <= '0; g_g[b][i] <= '0; g_b[b][i] <= '0;
          d_x[b][i] <= '0; d_y[b][i] <= '0; ink[b][i] <= 1'b0;
        end
    end else begin
      c_valid_o <= 1'b0;

      // ---- a new tile: swap banks and clear the new one -------------------
      // Clearing sixteen register cells in one clock is affordable precisely
      // because they ARE registers -- the thing the ruling insisted on.
      if (tile_start_i) begin
        bank_q <= ~bank_q;
        for (int i = 0; i < NCELL; i++) begin
          g_r[~bank_q][i] <= '0; g_g[~bank_q][i] <= '0; g_b[~bank_q][i] <= '0;
          d_x[~bank_q][i] <= '0; d_y[~bank_q][i] <= '0; ink[~bank_q][i] <= 1'b0;
        end
      end

      // ---- accumulate -----------------------------------------------------
      if (f_valid_i) begin
        automatic logic [16:0] sr = sat_add16(g_r[bank_q][cell_c], f_glow_r_i);
        automatic logic [16:0] sg = sat_add16(g_g[bank_q][cell_c], f_glow_g_i);
        automatic logic [16:0] sb = sat_add16(g_b[bank_q][cell_c], f_glow_b_i);
        automatic logic cx, cy;
        automatic logic signed [15:0] nx = sat_add_s16(d_x[bank_q][cell_c], f_disp_x_i, cx);
        automatic logic signed [15:0] ny = sat_add_s16(d_y[bank_q][cell_c], f_disp_y_i, cy);

        g_r[bank_q][cell_c] <= sr[16] ? 16'hFFFF : sr[15:0];
        g_g[bank_q][cell_c] <= sg[16] ? 16'hFFFF : sg[15:0];
        g_b[bank_q][cell_c] <= sb[16] ? 16'hFFFF : sb[15:0];
        d_x[bank_q][cell_c] <= nx;
        d_y[bank_q][cell_c] <= ny;
        ink[bank_q][cell_c] <= ink[bank_q][cell_c] | f_ink_i;

        fragments_o <= fragments_o + 32'd1;
        // ONE add of the saturation count, never one per channel in a loop:
        // three nonblocking increments of the same counter keep only the last.
        if (sr[16] || sg[16] || sb[16])
          glow_saturations_o <= glow_saturations_o + 32'd1;
        if (cx || cy) disp_clamps_o <= disp_clamps_o + 32'd1;
      end

      // ---- flush the OTHER bank, one cell a clock -------------------------
      // Every tile writes all sixteen cells INCLUDING ZEROS, so the frame
      // overwrites the plane and needs no reset pass. A cell that received
      // nothing is written as nothing.
      if (tile_flush_i && !flush_v_q) begin
        flush_v_q <= 1'b1;
        flush_i_q <= '0;
      end else if (flush_v_q) begin
        c_valid_o  <= 1'b1;
        c_index_o  <= fi_c;
        c_glow_o   <= {to5(g_r[~bank_q][fi_c]),
                       to6(g_g[~bank_q][fi_c]),
                       to5(g_b[~bank_q][fi_c])};
        c_disp_x_o <= px_c;
        c_disp_y_o <= py_c;
        c_ink_o    <= ink[~bank_q][fi_c];
        cells_flushed_o <= cells_flushed_o + 32'd1;
        // The clamp that MATTERS is this one, not the accumulator's: [-8,+8]
        // and [-4,+4] are the bound POST.COMPOSITE's nine-line ring is built
        // against, so a frame that clamps a lot here is a frame asking for a
        // displacement the compositor structurally cannot serve.
        if (dxc_c || dyc_c) disp_clamps_o <= disp_clamps_o + 32'd1;

        if (flush_i_q == (CIW+1)'(NCELL - 1)) flush_v_q <= 1'b0;
        flush_i_q <= flush_i_q + (CIW+1)'(1);
      end
    end
  end

endmodule : zhao_post_gather

`default_nettype wire
