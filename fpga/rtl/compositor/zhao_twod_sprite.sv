// zhao_twod_sprite.sv — HUD sprite descriptors into sample requests.
//
// ---------------------------------------------------------------------------
// THE RULING IS MOSTLY A SET OF DELETIONS (owner 2026-08-31 §3.2)
// ---------------------------------------------------------------------------
//   > HUD and overlay elements are ORDINARY SPRITE DESCRIPTORS USING THE
//   > PRIMARY TMU. There is NO PRIVATE HUD SAMPLER and NO SPECIAL TEXT
//   > RASTERIZER.
//
//     text      = glyph sprites
//     windows   = tiled/stretched sprites or simple filled descriptors
//     cursors   = sprites
//     heat maps = debug sprites, or one of the restricted plane slots
//
//   > The game authors layout and text in software.
//
// That sentence removes a font engine, a text layout engine, a glyph cache and
// their asset formats from the console. So this block does not rasterize
// glyphs, does not lay out text, does not sample anything, and owns no
// texture memory. It walks descriptors and emits the sample requests the
// PRIMARY TMU will serve.
//
// The two player HUD regions are a COMPOSITING concern, not a second sampler:
// they appear here only as `view_mask`, which decides whether a descriptor is
// walked for this view at all.
//
// ---------------------------------------------------------------------------
// AFFINE BY STEPPING, NOT BY MULTIPLYING
// ---------------------------------------------------------------------------
// UV at pixel (px, py) is
//
//     u = u0 + a00*px + a01*py
//     v = v0 + a10*px + a11*py
//
// which is two multiplies per pixel if written that way, per axis, forever. It
// is also a plane, so the walk needs only adds: carry a ROW ORIGIN that steps
// by (a01, a11) once per row, and a running coordinate that steps by
// (a00, a10) once per column.
//
// The row origin is stepped from the row origin -- NOT from the last pixel of
// the previous row. Accumulating along the serpentine would make row N's error
// depend on the width of row N-1, and the whole point of exact fixed-point
// stepping is that it does not drift.
// ---------------------------------------------------------------------------
`default_nettype none

module zhao_twod_sprite #(
    parameter int unsigned UVW = 32   // UV in fx16 S15.16
) (
    input var logic clk,
    input var logic rst_n,

    // ---- one descriptor ------------------------------------------------------
    input  var logic                    d_valid_i,
    output var logic                    d_ready_o,
    input  var logic signed [15:0]      d_x_i,
    input  var logic signed [15:0]      d_y_i,
    input  var logic [15:0]             d_w_i,
    input  var logic [15:0]             d_h_i,
    input  var logic signed [UVW-1:0]   d_u_i,       // UV at the top-left pixel
    input  var logic signed [UVW-1:0]   d_v_i,
    input  var logic signed [UVW-1:0]   d_a00_i,     // du/dx
    input  var logic signed [UVW-1:0]   d_a01_i,     // du/dy
    input  var logic signed [UVW-1:0]   d_a10_i,     // dv/dx
    input  var logic signed [UVW-1:0]   d_a11_i,     // dv/dy
    input  var logic [2:0]              d_format_i,
    input  var logic [7:0]              d_palette_i,
    input  var logic [15:0]             d_tint_i,
    input  var logic [1:0]              d_blend_i,
    input  var logic [1:0]              d_view_mask_i,
    input  var logic [7:0]              d_order_i,
    input  var logic [15:0]             d_src_id_i,

    // which view is being composited now
    input  var logic [1:0]              view_sel_i,

    // ---- the sample requests -------------------------------------------------
    output var logic                    s_valid_o,
    input  var logic                    s_ready_i,
    output var logic signed [15:0]      s_x_o,
    output var logic signed [15:0]      s_y_o,
    output var logic signed [UVW-1:0]   s_u_o,
    output var logic signed [UVW-1:0]   s_v_o,
    output var logic [2:0]              s_format_o,
    output var logic [7:0]              s_palette_o,
    output var logic [15:0]             s_tint_o,
    output var logic [1:0]              s_blend_o,
    output var logic [7:0]              s_order_o,
    output var logic [15:0]             s_src_id_o,
    output var logic                    s_last_o,     // last pixel of a sprite

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0]             descriptors_o,
    output var logic [31:0]             skipped_view_o,   // not for this view
    output var logic [31:0]             refused_o,        // zero width or height
    output var logic [31:0]             pixels_o
);

  logic                  busy_q;
  logic [15:0]           px_q, py_q;
  logic [15:0]           w_q, h_q;
  logic signed [15:0]    x_q, y_q;
  logic signed [UVW-1:0] u_q, v_q;             // running, this pixel
  logic signed [UVW-1:0] row_u_q, row_v_q;     // the ROW ORIGIN
  logic signed [UVW-1:0] a00_q, a01_q, a10_q, a11_q;
  logic [2:0]            fmt_q;
  logic [7:0]            pal_q, ord_q;
  logic [15:0]           tint_q, src_q;
  logic [1:0]            blend_q;

  // A descriptor is accepted only when the walker is free. There is no queue:
  // one sprite at a time is what "ordinary descriptors" means, and a caller
  // that wants overlap has more descriptors, not a deeper pipe here.
  assign d_ready_o = !busy_q;

  // Malformed: a zero-width or zero-height sprite. Refused rather than walked
  // for zero pixels, because the two are indistinguishable downstream and only
  // one of them is a bug in the caller.
  logic degenerate_c;
  assign degenerate_c = (d_w_i == 16'd0) || (d_h_i == 16'd0);

  logic for_this_view_c;
  assign for_this_view_c = (d_view_mask_i & view_sel_i) != 2'd0;

  logic last_col_c, last_row_c;
  assign last_col_c = (px_q == w_q - 16'd1);
  assign last_row_c = (py_q == h_q - 16'd1);

  assign s_valid_o   = busy_q;
  // The sum is one bit wider than the port and is TRUNCATED deliberately: a
  // sprite placed so far off-screen that its pixel positions wrap s16 is a
  // caller error the compositor's own clip already handles, and widening the
  // port here would move that decision into this block.
  assign s_x_o       = 16'(x_q + $signed({1'b0, px_q}));
  assign s_y_o       = 16'(y_q + $signed({1'b0, py_q}));
  assign s_u_o       = u_q;
  assign s_v_o       = v_q;
  assign s_format_o  = fmt_q;
  assign s_palette_o = pal_q;
  assign s_tint_o    = tint_q;
  assign s_blend_o   = blend_q;
  assign s_order_o   = ord_q;
  assign s_src_id_o  = src_q;
  assign s_last_o    = busy_q && last_col_c && last_row_c;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      busy_q         <= 1'b0;
      descriptors_o  <= '0;
      skipped_view_o <= '0;
      refused_o      <= '0;
      pixels_o       <= '0;
    end else begin
      // ---- accept a descriptor ------------------------------------------
      if (d_valid_i && d_ready_o) begin
        descriptors_o <= descriptors_o + 32'd1;
        if (degenerate_c) begin
          refused_o <= refused_o + 32'd1;
        end else if (!for_this_view_c) begin
          // The two HUD regions are a compositing concern; here they are just
          // a mask. A descriptor for the other view is SKIPPED, and counted
          // separately from a refusal -- one is normal and one is a bug.
          skipped_view_o <= skipped_view_o + 32'd1;
        end else begin
          busy_q  <= 1'b1;
          px_q    <= 16'd0;
          py_q    <= 16'd0;
          w_q     <= d_w_i;
          h_q     <= d_h_i;
          x_q     <= d_x_i;
          y_q     <= d_y_i;
          u_q     <= d_u_i;
          v_q     <= d_v_i;
          row_u_q <= d_u_i;
          row_v_q <= d_v_i;
          a00_q   <= d_a00_i;
          a01_q   <= d_a01_i;
          a10_q   <= d_a10_i;
          a11_q   <= d_a11_i;
          fmt_q   <= d_format_i;
          pal_q   <= d_palette_i;
          tint_q  <= d_tint_i;
          blend_q <= d_blend_i;
          ord_q   <= d_order_i;
          src_q   <= d_src_id_i;
        end
      end

      // ---- walk -----------------------------------------------------------
      if (busy_q && s_ready_i) begin
        pixels_o <= pixels_o + 32'd1;
        if (!last_col_c) begin
          px_q <= px_q + 16'd1;
          u_q  <= u_q + a00_q;
          v_q  <= v_q + a10_q;
        end else if (!last_row_c) begin
          px_q <= 16'd0;
          py_q <= py_q + 16'd1;
          // The next row starts from the ROW ORIGIN stepped once, never from
          // the last pixel of this row. Accumulating along the serpentine
          // would make row N's coordinate depend on the width of row N-1.
          row_u_q <= row_u_q + a01_q;
          row_v_q <= row_v_q + a11_q;
          u_q     <= row_u_q + a01_q;
          v_q     <= row_v_q + a11_q;
        end else begin
          busy_q <= 1'b0;
        end
      end
    end
  end

endmodule : zhao_twod_sprite

`default_nettype wire
