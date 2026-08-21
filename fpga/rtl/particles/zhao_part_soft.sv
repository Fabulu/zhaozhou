// zhao_part_soft.sv — PART.SOFT: the soft/point sprite endpoint.
//
// Contract: design/contracts/PART.SOFT.md
// Reference: `zref::part::soft_rect` (reference/include/zref/zref_particle_soft.hpp),
// which restates `zref::render::draw_population`'s `points` branch and the
// `blit_pattern_block` it calls — and is checked against them, see the test.
//
// The ledger's `zref::SoftParticles` never existed; one of the twenty-five
// phantoms, and a cheap one, because the law was already shipped inline.
//
// PART.EXPAND is the `tris` branch of the same function; this is the `points`
// branch. They are genuinely different laws: one emits a TRIANGLE for the setup
// stage, this one emits a scissored PIXEL RECTANGLE for the fragment stage.
//
// ---------------------------------------------------------------------------
// THE LAW
// ---------------------------------------------------------------------------
//     side_sub = size << 4
//     x0_sub   = sx - side_sub/2,  y0_sub = sy - side_sub/2
//     min_x = max( (x0_sub + 255) >>> 8,        vp_x0 )
//     max_x = min( (x0_sub + side_sub) >>> 8,   vp_x0 + vp_w - 1 )
//     min_y = max( (y0_sub + 255) >>> 8,        vp_y0 )
//     max_y = min( (y0_sub + side_sub) >>> 8,   vp_y0 + vp_h - 1 )
//
// **THE TWO EDGES ROUND DIFFERENTLY, AND THAT IS THE WHOLE RULE.** The low edge
// CEILS (`+255 >>> 8`), the high edge FLOORS (`>>> 8`). That is pixel-centre
// coverage: a pixel is inside iff its centre is inside the rectangle. Rounding
// both the same way is the obvious tidy-up, and it makes every sprite a pixel
// too wide or too narrow on ONE side only — an asymmetry that reads as a sprite
// drifting as it moves rather than as an obvious break.
//
// The shifts are written ARITHMETIC because that is what the reference's `>>`
// on a signed int does, and `x0_sub` is routinely negative for a sprite
// straddling the left or top edge.
//
// Worth recording honestly: at THESE widths a logical shift would give the same
// answer, because narrowing the 22-bit result to 14 bits keeps exactly bits
// [21:8] either way. A mutation to `>>` survives the whole suite -- an
// equivalent mutant, not a gap. The arithmetic form stays because it is what the
// law says and because the equivalence is an accident of the current widths.
//
// **A ZERO EXTENT DRAWS NOTHING.** `blit_pattern_block` returns before any
// clamping on `w_sub <= 0`, so `size == 0` produces no pixels at all rather than
// a one-pixel dot.
//
// **AN EMPTY RECTANGLE IS REPORTED, NOT EMITTED.** After scissoring, `min > max`
// on either axis means the reference's loops simply do not run. This block
// raises no output beat in that case: a fragment span covering nothing is work
// for RASTER.FRAGMENT and a zero-pixel primitive in every capture.
//
// **DEPTH IS TESTED, NEVER WRITTEN** (charter §8 pass 7) — carried on the packet
// for the same reason PART.EXPAND carries it: the law belongs to the particle,
// and a downstream stage that had to remember it would eventually forget.
//
// ---------------------------------------------------------------------------
// WIDTHS
// ---------------------------------------------------------------------------
// `sx`/`sy` are S 12.8 in 21 bits, inside GEOM.PROJECT's ±2048 px guard band.
// `side_sub` is at most 4,080. `x0_sub + side_sub` is therefore at most about
// 2^19 + 2^12, comfortably inside signed 22. The pixel results are 13-bit signed
// canvas coordinates, which spans ±4096 px — wider than the guard band, so a
// clamped edge can never wrap.
module zhao_part_soft (
    input logic clk,
    input logic rst_n,

    // ---- viewport, canvas-local -------------------------------------------
    input logic [11:0] vp_x0_i,
    input logic [11:0] vp_y0_i,
    input logic [11:0] vp_w_i,
    input logic [11:0] vp_h_i,

    // ---- projected particle in --------------------------------------------
    input  logic               p_valid_i,
    output logic               p_ready_o,
    input  logic               p_in_i,     // GEOM.PROJECT's verdict
    input  logic signed [20:0] p_x_i,      // S 12.8 canvas
    input  logic signed [20:0] p_y_i,
    input  logic signed [31:0] p_d_i,      // Q16.16 1/w
    input  logic        [ 7:0] p_size_i,   // U 0.4.4 px
    input  logic        [ 7:0] p_r_i,
    input  logic        [ 7:0] p_g_i,
    input  logic        [ 7:0] p_b_i,
    input  logic        [15:0] p_src_id_i,

    // ---- soft sprite out: a scissored whole-pixel span ---------------------
    output logic               s_valid_o,
    input  logic               s_ready_i,
    output logic signed [12:0] s_min_x_o,
    output logic signed [12:0] s_max_x_o,
    output logic signed [12:0] s_min_y_o,
    output logic signed [12:0] s_max_y_o,
    output logic signed [31:0] s_d_o,
    output logic        [ 7:0] s_r_o,
    output logic        [ 7:0] s_g_o,
    output logic        [ 7:0] s_b_o,
    output logic               s_depth_test_o,
    output logic               s_depth_write_o,
    output logic        [15:0] s_src_id_o,

    output logic [31:0] soft_particles_o
);

  logic signed [12:0] side_sub;
  logic signed [11:0] half_sub;
  assign side_sub = $signed({1'b0, p_size_i, 4'b0});
  assign half_sub = side_sub[12:1];  // exact: side_sub is a multiple of 16

  logic signed [21:0] x0_sub, y0_sub, x1_sub, y1_sub;
  always_comb begin
    x0_sub = 22'($signed(p_x_i)) - 22'($signed(half_sub));
    y0_sub = 22'($signed(p_y_i)) - 22'($signed(half_sub));
    x1_sub = x0_sub + 22'($signed(side_sub));
    y1_sub = y0_sub + 22'($signed(side_sub));
  end

  // Ceiling on the low edge, floor on the high edge. Arithmetic shifts, because
  // these are routinely negative for a sprite straddling the left or top edge.
  logic signed [13:0] lo_x, hi_x, lo_y, hi_y;
  always_comb begin
    lo_x = 14'((x0_sub + 22'sd255) >>> 8);
    hi_x = 14'(x1_sub >>> 8);
    lo_y = 14'((y0_sub + 22'sd255) >>> 8);
    hi_y = 14'(y1_sub >>> 8);
  end

  logic signed [13:0] vx0, vy0, vx1, vy1;
  always_comb begin
    vx0 = 14'($signed({2'b0, vp_x0_i}));
    vy0 = 14'($signed({2'b0, vp_y0_i}));
    vx1 = 14'($signed({2'b0, vp_x0_i})) + 14'($signed({2'b0, vp_w_i})) - 14'sd1;
    vy1 = 14'($signed({2'b0, vp_y0_i})) + 14'($signed({2'b0, vp_h_i})) - 14'sd1;
  end

  logic signed [13:0] cl_min_x, cl_max_x, cl_min_y, cl_max_y;
  always_comb begin
    cl_min_x = (lo_x > vx0) ? lo_x : vx0;
    cl_max_x = (hi_x < vx1) ? hi_x : vx1;
    cl_min_y = (lo_y > vy0) ? lo_y : vy0;
    cl_max_y = (hi_y < vy1) ? hi_y : vy1;
  end

  logic nonzero_extent, covered;
  assign nonzero_extent = (side_sub > 13'sd0);
  assign covered = nonzero_extent && (cl_min_x <= cl_max_x) && (cl_min_y <= cl_max_y);

  logic take, emits;
  assign p_ready_o = !s_valid_o || s_ready_i;
  assign take = p_valid_i && p_ready_o;
  // Behind the eye, zero extent, or scissored to nothing: consumed, not emitted.
  assign emits = take && p_in_i && covered;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      s_valid_o <= 1'b0;
      s_min_x_o <= '0; s_max_x_o <= '0; s_min_y_o <= '0; s_max_y_o <= '0;
      s_d_o <= '0;
      s_r_o <= '0; s_g_o <= '0; s_b_o <= '0;
      s_depth_test_o <= 1'b1;
      s_depth_write_o <= 1'b0;
      s_src_id_o <= '0;
      soft_particles_o <= '0;
    end else begin
      if (s_valid_o && s_ready_i) s_valid_o <= 1'b0;
      if (emits) begin
        s_min_x_o <= cl_min_x[12:0];
        s_max_x_o <= cl_max_x[12:0];
        s_min_y_o <= cl_min_y[12:0];
        s_max_y_o <= cl_max_y[12:0];
        s_d_o <= p_d_i;
        s_r_o <= p_r_i; s_g_o <= p_g_i; s_b_o <= p_b_i;
        s_depth_test_o <= 1'b1;
        s_depth_write_o <= 1'b0;
        s_src_id_o <= p_src_id_i;
        s_valid_o <= 1'b1;
        // Counts sprites that COVER something. One scissored away entirely was
        // never a soft particle on this screen.
        if (soft_particles_o != 32'hFFFF_FFFF) soft_particles_o <= soft_particles_o + 32'd1;
      end
    end
  end

endmodule : zhao_part_soft
