// zhao_twod_plane.sv — the two world plane slots, on ONE engine.
//
// ---------------------------------------------------------------------------
// BOTH HALVES OF THE RULING ARE BINDING (owner 2026-08-31 §3.1)
// ---------------------------------------------------------------------------
//   > Two plane descriptors are a real v1 limit, not a placeholder. USE ONE
//   > TIME-MULTIPLEXED RESTRICTED PLANE ENGINE. Do not instantiate two full
//   > engines and do not let it become a second unrestricted TMU.
//
// Two slots is the INTERFACE; one engine is the IMPLEMENTATION, and they are
// separately binding -- building two engines would satisfy the first and
// violate the second. So there is one set of steppers here and a slot select,
// not two of anything.
//
// ---------------------------------------------------------------------------
// THE FEATURE LIST IS A CEILING, NOT A FLOOR
// ---------------------------------------------------------------------------
//   CLUT8 and RGB565 only
//   NEAREST SAMPLING ONLY -- "the single most important exclusion: bilinear is
//   what would make this a second TMU"
//   affine transform, line scroll, repeat and clamp, view masks
//
//   > Anything needing ordinary textured geometry uses the main renderer. If a
//   > request cannot be met by the list above, the answer is a triangle, not a
//   > wider plane engine.
//
// Nearest is enforced structurally: the texel coordinate leaves this block as
// an INTEGER. There is no fractional output port for a filter to use, so a
// future bilinear would have to change the interface, which is the point.
//
// ---------------------------------------------------------------------------
// ROLES, ADDED BY RULING R4 (2026-09-02)
// ---------------------------------------------------------------------------
//   0 BACKDROP    beneath the resolved world; blend MUST be REPLACE;
//                 no depth test, no depth write
//   1 ATMOSPHERE  post stage 4, over the displaced world, before bloom/grade;
//                 ALPHA or ADD
//   2, 3          reserved; the descriptor is REFUSED
//
// There is NO DEPTH PORT on this block, in either direction. R4 is explicit
// that v1 has no arbitrary depth test and no depth write, and the contract's
// purpose line used to name a "world-space depth plane" -- naming a typical
// use is how a restriction gets read as permission. Water, lava or anything
// that must INTERSECT ordinary geometry is triangles through the main
// renderer.
//
// ---------------------------------------------------------------------------
// WRAPPING WITHOUT A DIVIDER
// ---------------------------------------------------------------------------
// REPEAT is a modulo, and a modulo by an arbitrary width is a divider this
// block will not have. It does not need one: the coordinate is stepped, so it
// leaves the range by at most one step per pixel, and ONE conditional
// correction restores it.
//
// That is true only while |step| <= size, which is true of every real plane and
// is not true of a caller that asks for a plane scaled down by more than its
// own width per pixel. So it is COUNTED rather than assumed -- `wrap_fail_o`
// says the assumption was violated, instead of the picture quietly tiling
// wrongly.
// ---------------------------------------------------------------------------
`default_nettype none

module zhao_twod_plane #(
    parameter int unsigned CW = 32   // plane coordinates, fx16 S15.16
) (
    input var logic clk,
    input var logic rst_n,

    // ---- the descriptor for the slot being programmed ------------------------
    input  var logic                  d_valid_i,
    output var logic                  d_ready_o,
    input  var logic                  d_slot_i,        // 0 or 1: TWO slots
    input  var logic [1:0]            d_role_i,        // R4: 0 BACKDROP, 1 ATMOSPHERE
    input  var logic [1:0]            d_blend_i,       // 0 REPLACE, 1 ALPHA, 2 ADD
    input  var logic [7:0]            d_opacity_i,     // unit8, value = raw/256
    input  var logic                  d_format_i,      // 0 CLUT8, 1 RGB565
    input  var logic [15:0]           d_width_i,
    input  var logic [15:0]           d_height_i,
    input  var logic                  d_wrap_u_i,      // 0 repeat, 1 clamp
    input  var logic                  d_wrap_v_i,
    input  var logic signed [CW-1:0]  d_a_i,           // du/dx
    input  var logic signed [CW-1:0]  d_b_i,           // du/dy
    input  var logic signed [CW-1:0]  d_c_i,           // dv/dx
    input  var logic signed [CW-1:0]  d_d_i,           // dv/dy
    input  var logic signed [CW-1:0]  d_u0_i,          // u at (0,0)
    input  var logic signed [CW-1:0]  d_v0_i,
    input  var logic [1:0]            d_view_mask_i,
    input  var logic [7:0]            d_palette_i,

    // ---- pixel stream --------------------------------------------------------
    input  var logic                  p_valid_i,
    output var logic                  p_ready_o,
    input  var logic                  p_slot_i,        // which plane this pixel is for
    input  var logic [15:0]           p_x_i,
    input  var logic [15:0]           p_y_i,
    input  var logic signed [CW-1:0]  p_line_scroll_i, // added to u at this line
    input  var logic [1:0]            view_sel_i,

    // ---- the sample request --------------------------------------------------
    // INTEGER texel coordinates. There is no fractional port, which is what
    // makes "nearest only" structural rather than a promise.
    output var logic                  s_valid_o,
    input  var logic                  s_ready_i,
    output var logic [15:0]           s_texel_u_o,
    output var logic [15:0]           s_texel_v_o,
    output var logic                  s_format_o,
    output var logic [7:0]            s_palette_o,
    output var logic [1:0]            s_blend_o,
    output var logic [7:0]            s_opacity_o,
    output var logic [1:0]            s_role_o,

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0]           pixels_o,
    output var logic [31:0]           refused_role_o,     // role 2 or 3
    output var logic [31:0]           refused_blend_o,    // BACKDROP not REPLACE
    output var logic [31:0]           skipped_view_o,
    output var logic [31:0]           wrap_fail_o         // one correction was not enough
);

  localparam logic [1:0] ROLE_BACKDROP   = 2'd0;
  localparam logic [1:0] ROLE_ATMOSPHERE = 2'd1;
  localparam logic [1:0] BLEND_REPLACE   = 2'd0;

  // ---- two SLOTS of state, ONE engine ------------------------------------
  logic                  en_q     [2];
  logic [1:0]            role_q   [2];
  logic [1:0]            blend_q  [2];
  logic [7:0]            opac_q   [2];
  logic                  fmt_q    [2];
  logic [15:0]           w_q      [2];
  logic [15:0]           h_q      [2];
  logic                  wrapu_q  [2];
  logic                  wrapv_q  [2];
  logic signed [CW-1:0]  a_q [2], b_q [2], c_q [2], d_q [2], u0_q [2], v0_q [2];
  logic [1:0]            vmask_q  [2];
  logic [7:0]            pal_q    [2];

  assign d_ready_o = 1'b1;   // programming a slot never waits on a pixel
  assign p_ready_o = !s_valid_o || s_ready_i;

  // ---- descriptor validation, R4 -----------------------------------------
  logic role_bad_c, blend_bad_c;
  assign role_bad_c  = (d_role_i != ROLE_BACKDROP) && (d_role_i != ROLE_ATMOSPHERE);
  // A BACKDROP sits beneath the resolved world, so there is nothing under it
  // to blend with. An alpha-blended backdrop is malformed rather than merely
  // odd, and R4 says so.
  assign blend_bad_c = (d_role_i == ROLE_BACKDROP) && (d_blend_i != BLEND_REPLACE);

  // ---- the affine, evaluated per pixel ------------------------------------
  // Written as the closed form. This block sees an arbitrary (x, y) rather
  // than a raster walk -- the compositor owns the walk -- so there is no row
  // origin to step and nothing to be gained by pretending otherwise.
  logic sl;
  assign sl = p_slot_i;

  localparam int unsigned FW = CW + 17;   // room for coeff * 16-bit coordinate
  logic signed [FW-1:0] u_full_c, v_full_c;
  always_comb begin
    u_full_c = FW'($signed(u0_q[sl]))
             + FW'($signed(a_q[sl]) * $signed({1'b0, p_x_i}))
             + FW'($signed(b_q[sl]) * $signed({1'b0, p_y_i}))
             + FW'($signed(p_line_scroll_i));
    v_full_c = FW'($signed(v0_q[sl]))
             + FW'($signed(c_q[sl]) * $signed({1'b0, p_x_i}))
             + FW'($signed(d_q[sl]) * $signed({1'b0, p_y_i}));
  end

  // NEAREST: take the integer part and nothing else. There is no rounding
  // decision here because there is no filter -- the texel a coordinate falls
  // in is the texel, which is what nearest means.
  logic signed [CW-1:0] ui_c, vi_c;
  assign ui_c = CW'(u_full_c >>> 16);
  assign vi_c = CW'(v_full_c >>> 16);

  // ---- wrap: repeat by ONE conditional correction, or clamp --------------
  function automatic logic [15:0] wrap_coord(input logic signed [CW-1:0] t,
                                             input logic [15:0] size,
                                             input logic clamp_mode,
                                             output logic failed);
    logic signed [CW-1:0] r;
    // The size is widened to the coordinate's width ONCE, here, rather than at
    // each comparison: a 16-bit size compared against a signed 32-bit
    // coordinate is the kind of implicit widening that is right by accident
    // until the coordinate goes negative.
    logic signed [CW-1:0] sz;
    begin
      failed = 1'b0;
      sz = CW'({1'b0, size});
      if (clamp_mode) begin
        wrap_coord = (t < 0) ? 16'd0
                   : (t >= sz) ? (size - 16'd1)
                   : t[15:0];
      end else begin
        r = t;
        if (r < 0) r = r + sz;
        else if (r >= sz) r = r - sz;
        // ONE correction. If that was not enough the caller asked for a step
        // larger than the plane, which is counted rather than tiled wrongly.
        if (r < 0 || r >= sz) begin
          failed = 1'b1;
          wrap_coord = 16'd0;
        end else begin
          wrap_coord = r[15:0];
        end
      end
    end
  endfunction

  logic uf_c, vf_c;
  logic [15:0] tu_c, tv_c;
  always_comb begin
    tu_c = wrap_coord(ui_c, w_q[sl], wrapu_q[sl], uf_c);
    tv_c = wrap_coord(vi_c, h_q[sl], wrapv_q[sl], vf_c);
  end

  logic drawable_c;
  assign drawable_c = en_q[sl] && ((vmask_q[sl] & view_sel_i) != 2'd0);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      s_valid_o       <= 1'b0;
      pixels_o        <= '0;
      refused_role_o  <= '0;
      refused_blend_o <= '0;
      skipped_view_o  <= '0;
      wrap_fail_o     <= '0;
      for (int i = 0; i < 2; i++) en_q[i] <= 1'b0;
    end else begin
      // ---- program a slot -------------------------------------------------
      if (d_valid_i) begin
        if (role_bad_c) begin
          // Roles 2 and 3 are reserved; the descriptor is refused, and the
          // slot is DISABLED rather than left holding its previous contents --
          // a refused program that quietly kept drawing the old plane would be
          // worse than one that drew nothing.
          en_q[d_slot_i]  <= 1'b0;
          refused_role_o  <= refused_role_o + 32'd1;
        end else if (blend_bad_c) begin
          en_q[d_slot_i]  <= 1'b0;
          refused_blend_o <= refused_blend_o + 32'd1;
        end else begin
          en_q[d_slot_i]    <= 1'b1;
          role_q[d_slot_i]  <= d_role_i;
          blend_q[d_slot_i] <= d_blend_i;
          opac_q[d_slot_i]  <= d_opacity_i;
          fmt_q[d_slot_i]   <= d_format_i;
          w_q[d_slot_i]     <= d_width_i;
          h_q[d_slot_i]     <= d_height_i;
          wrapu_q[d_slot_i] <= d_wrap_u_i;
          wrapv_q[d_slot_i] <= d_wrap_v_i;
          a_q[d_slot_i]     <= d_a_i;
          b_q[d_slot_i]     <= d_b_i;
          c_q[d_slot_i]     <= d_c_i;
          d_q[d_slot_i]     <= d_d_i;
          u0_q[d_slot_i]    <= d_u0_i;
          v0_q[d_slot_i]    <= d_v0_i;
          vmask_q[d_slot_i] <= d_view_mask_i;
          pal_q[d_slot_i]   <= d_palette_i;
        end
      end

      // ---- one pixel ------------------------------------------------------
      if (!s_valid_o || s_ready_i) begin
        s_valid_o <= p_valid_i && drawable_c;
        if (p_valid_i) begin
          if (!drawable_c) begin
            skipped_view_o <= skipped_view_o + 32'd1;
          end else begin
            s_texel_u_o <= tu_c;
            s_texel_v_o <= tv_c;
            s_format_o  <= fmt_q[sl];
            s_palette_o <= pal_q[sl];
            s_blend_o   <= blend_q[sl];
            s_opacity_o <= opac_q[sl];
            s_role_o    <= role_q[sl];
            pixels_o    <= pixels_o + 32'd1;
            if (uf_c || vf_c) wrap_fail_o <= wrap_fail_o + 32'd1;
          end
        end
      end
    end
  end

endmodule : zhao_twod_plane

`default_nettype wire
