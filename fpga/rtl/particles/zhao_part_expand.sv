// zhao_part_expand.sv — PART.EXPAND: polygon particles into geometry packets.
//
// Contract: design/contracts/PART.EXPAND.md
// Reference: `zref::part::expand_polygon` (reference/include/zref/zref_particle.hpp),
// which is the `tris` branch of `zref::render::draw_population`
// (reference/src/zrender/sprites.cpp) restated — and checked against it, see the
// reference header.
//
// The ledger's `zref::ParticleExpand` never existed; this is one of the
// twenty-five phantoms, and one of the cheap ones, because the law was already
// shipped inline inside the renderer.
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK IS
// ---------------------------------------------------------------------------
// One projected particle in, one three-vertex screen-space fan out, one per
// clock. It is the whole of "expand polygon-particle instances into normal
// geometry packets for the setup stage" and none of anything else:
//
//   * PROJECTION is GEOM.PROJECT's. This block is handed a screen vertex and a
//     behind-the-eye verdict; it never sees a matrix.
//   * RASTERISATION is GEOM.SETUP's and RASTER's. The output packet is a
//     triangle, not pixels.
//   * THE LADDER DECISION is PART.LADDER's. A particle arriving here has already
//     been chosen as a polygon particle; this block does not second-guess it.
//
// ---------------------------------------------------------------------------
// THE LAW, and the three parts of it that are load-bearing
// ---------------------------------------------------------------------------
//     side_sub = size << 4
//     a = { x,                   y - side_sub   }
//     b = { x - (side_sub*3)/4,  y + side_sub/2 }
//     c = { x + (side_sub*3)/4,  y + side_sub/2 }
//
// 1. THE FAN IS NOT EQUILATERAL AND MUST NOT BE "CORRECTED". Half-width is 3/4
//    of a side, the drop is half a side, both integer-divided and both
//    asymmetric about the centre. A true equilateral triangle would be better
//    geometry and would change every particle on screen.
// 2. `size << 4`, NOT `<< 8`. Particle size is U 0.4.4 pixels (qformats §10) --
//    sixteenths of a pixel -- so a shift of 4 lands it in S 12.8 subpixels. A
//    shift of 8 would treat the byte as whole pixels and make every particle
//    sixteen times too large.
// 3. DEPTH IS TESTED, NEVER WRITTEN. `draw_population` sets depth_write false
//    with the comment "pass-7 law: test only, no write". Particles occlude
//    nothing behind them, and one that wrote depth would carve a hole in
//    whatever drew after it. The output carries the mode bits so the stage
//    downstream cannot get it wrong either.
//
// A particle behind the eye produces NOTHING -- `draw_population` `continue`s
// past it. It is not emitted as a degenerate triangle, because a degenerate
// triangle is work for the setup stage and a silent zero-area primitive in every
// capture.
//
// ---------------------------------------------------------------------------
// WIDTHS
// ---------------------------------------------------------------------------
// Screen coordinates are S 12.8 in 21 bits, already inside the ±2048 px guard
// band GEOM.PROJECT clamps to. `side_sub` is `size << 4` with size a byte, so at
// most 4,080 -- twelve bits. A vertex is therefore at most 21 bits plus a
// twelve-bit offset: 22 bits signed covers it with room, and the output is
// widened to 22 rather than silently wrapping a 21-bit port.
//
// **The expanded fan is NOT re-clamped to the guard band.** A large particle
// near the edge can put a vertex outside ±2048 px, and that is correct: the
// software does exactly the same and lets the rasteriser's scan box scissor it.
// Clamping here would deform the triangle instead of clipping it, which moves
// the particle rather than cropping it.
module zhao_part_expand (
    input logic clk,
    input logic rst_n,

    // ---- projected particle in --------------------------------------------
    input  logic               p_valid_i,
    output logic               p_ready_o,
    input  logic               p_in_i,      // GEOM.PROJECT's verdict: 0 = behind the eye
    input  logic signed [20:0] p_x_i,       // S 12.8 canvas
    input  logic signed [20:0] p_y_i,
    input  logic signed [31:0] p_d_i,       // Q16.16 1/w
    input  logic        [ 7:0] p_size_i,    // U 0.4.4 px
    input  logic        [ 7:0] p_r_i,
    input  logic        [ 7:0] p_g_i,
    input  logic        [ 7:0] p_b_i,
    input  logic        [15:0] p_src_id_i,

    // ---- geometry packet out ----------------------------------------------
    output logic               t_valid_o,
    input  logic               t_ready_i,
    output logic signed [21:0] t_ax_o,
    output logic signed [21:0] t_ay_o,
    output logic signed [21:0] t_bx_o,
    output logic signed [21:0] t_by_o,
    output logic signed [21:0] t_cx_o,
    output logic signed [21:0] t_cy_o,
    output logic signed [31:0] t_d_o,        // all three vertices share it
    output logic        [ 7:0] t_r_o,
    output logic        [ 7:0] t_g_o,
    output logic        [ 7:0] t_b_o,
    output logic               t_depth_test_o,   // constant 1, carried explicitly
    output logic               t_depth_write_o,  // constant 0, carried explicitly
    output logic        [15:0] t_src_id_o,

    output logic [31:0] polygon_particles_o
);

  // side_sub = size << 4: a byte of sixteenths becomes S 12.8 subpixels.
  logic signed [12:0] side_sub;
  assign side_sub = $signed({1'b0, p_size_i, 4'b0});

  // The two offsets: (side_sub*3)/4 and side_sub/2.
  //
  // BOTH DIVISIONS ARE EXACT, and it is worth knowing why rather than assuming
  // the truncation matters. `side_sub` is `size << 4`, so it is always a
  // multiple of 16; `side_sub*3` is a multiple of 48, and both 48 and 16 are
  // divisible by 4 and 2. Nothing is ever discarded for ANY size byte.
  //
  // That was established by mutation, not by inspection: a variant that rounded
  // the half-width instead of truncating passed all 565 directed checks and the
  // random lane, because there is no input on which the two differ. It is an
  // equivalent mutant, not a hole in the test -- and the earlier version of this
  // comment, which said the divisions truncate, was simply wrong.
  //
  // The shifts stay because they are what the reference's `/` compiles to and
  // because `size` is unsigned, so the negative-operand question never arises.
  logic signed [12:0] half_w;
  logic signed [11:0] half_drop;
  always_comb begin
    half_w = 13'((($signed({side_sub, 2'b0}) - $signed({{2{side_sub[12]}}, side_sub})) >>> 2));
    half_drop = 12'(side_sub >>> 1);
  end

  logic signed [21:0] ex_ax, ex_ay, ex_bx, ex_by, ex_cx, ex_cy;
  always_comb begin
    ex_ax = 22'($signed(p_x_i));
    ex_ay = 22'($signed(p_y_i)) - 22'($signed(side_sub));
    ex_bx = 22'($signed(p_x_i)) - 22'($signed(half_w));
    ex_by = 22'($signed(p_y_i)) + 22'($signed(half_drop));
    ex_cx = 22'($signed(p_x_i)) + 22'($signed(half_w));
    ex_cy = ex_by;
  end

  // A behind-the-eye particle is consumed and produces nothing, so it must not
  // occupy an output beat.
  logic take, emits;
  assign p_ready_o = !t_valid_o || t_ready_i;
  assign take = p_valid_i && p_ready_o;
  assign emits = take && p_in_i;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      t_valid_o <= 1'b0;
      t_ax_o <= '0; t_ay_o <= '0; t_bx_o <= '0; t_by_o <= '0; t_cx_o <= '0; t_cy_o <= '0;
      t_d_o <= '0;
      t_r_o <= '0; t_g_o <= '0; t_b_o <= '0;
      t_depth_test_o <= 1'b1;
      t_depth_write_o <= 1'b0;
      t_src_id_o <= '0;
      polygon_particles_o <= '0;
    end else begin
      if (t_valid_o && t_ready_i) t_valid_o <= 1'b0;
      if (emits) begin
        t_ax_o <= ex_ax; t_ay_o <= ex_ay;
        t_bx_o <= ex_bx; t_by_o <= ex_by;
        t_cx_o <= ex_cx; t_cy_o <= ex_cy;
        t_d_o <= p_d_i;
        t_r_o <= p_r_i; t_g_o <= p_g_i; t_b_o <= p_b_i;
        // Constants, and carried on the packet on purpose: the pass-7 law is a
        // property of a PARTICLE, and a downstream stage that had to remember it
        // would eventually forget.
        t_depth_test_o <= 1'b1;
        t_depth_write_o <= 1'b0;
        t_src_id_o <= p_src_id_i;
        t_valid_o <= 1'b1;
        // Counts particles EXPANDED, not offered: a behind-the-eye particle was
        // never a polygon particle.
        if (polygon_particles_o != 32'hFFFF_FFFF) begin
          polygon_particles_o <= polygon_particles_o + 32'd1;
        end
      end
    end
  end

endmodule : zhao_part_expand
