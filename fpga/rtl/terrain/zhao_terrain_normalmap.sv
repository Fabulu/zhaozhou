// zhao_terrain_normalmap.sv — detail normals on the heightfield.
//
// ---------------------------------------------------------------------------
// WHAT IT IS FOR
// ---------------------------------------------------------------------------
// Owner ruling 2026-09-03: build the real per-pixel normal path, measure it,
// and cut it afterwards if the production resource count says there is no room
// -- "we make it and see how bad it is ... Normal maps would be a huge gain
// though."
//
// An 8 km island lit only by face normals reads smooth and plasticky at the
// range the player actually stands at. This block is what puts surface detail
// under a MOVING sun, which is the thing a baked highlight cannot do.
//
// ---------------------------------------------------------------------------
// WHY THERE IS NO TANGENT FRAME HERE
// ---------------------------------------------------------------------------
// The textbook path interpolates a tangent basis per vertex and normalises per
// fragment. A HEIGHTFIELD does not need it: its frame is axis-aligned in world
// space, so a detail normal perturbs the surface normal in world XZ directly.
// Nothing extra is interpolated and no frame is built.
//
// ---------------------------------------------------------------------------
// THE SPLIT THAT MAKES IT AFFORDABLE
// ---------------------------------------------------------------------------
//     dot(n/|n| + s*d, L)  =  dot(n, L)/|n|  +  s*dot(d, L)
//                             \___________/     \__________/
//                              PER TRIANGLE      per fragment
//
// TERRAIN.NORMALS emits ONE face normal per triangle, so the left term -- the
// square root and the divide, all of the expensive arithmetic -- is computed
// once per triangle and reused by every fragment of it. What each fragment
// pays is two multiplies and an add.
//
// The dropped re-normalisation of `n/|n| + s*d` is an APPROXIMATION whose error
// grows with DETAIL_STRENGTH and is zero at zero. It is a look-tuned knob and
// stays one.
//
// ---------------------------------------------------------------------------
// Q FORMATS (spec/qformats.md)
// ---------------------------------------------------------------------------
//   n_[xyz]_i      Q16.16, UN-normalised, exactly as TERRAIN.NORMALS emits
//   sun_[xyz]_i    s1.15 unit, pointing FROM the surface TOWARD the light
//   d_texel_i      {s8 dz, s8 dx}, value raw/128, world X and Z
//   strength_i     u8, value raw/256
//   ambient_i      unit8, a FLOOR not an addend
//   shade_o        unit8 (qformats 2: value = raw/256, so 255 is the largest)
//
// STATUS: DRAFT, NOT IN THE PRODUCTION MANIFEST, AND KNOWN WRONG.
// The per-triangle divide runs 32 steps over a 64-bit numerator, so it yields
// quotient bits 63..32; the true quotient is always below 2^15 by
// Cauchy-Schwarz, so `base_o` comes out ZERO for every realistic triangle and
// the whole effect would silently do nothing. See
// reports/NORMALMAP-ARCHITECTURE.md for that and four more faults, and for the
// TERRAIN.SHADE / TERRAIN.NORMALMAP split that replaces this file.
//
// No test is cited here on purpose: none is written, and citing one that is
// not would be a fresh phantom citation on the same day as the audit.
`default_nettype none

module zhao_terrain_normalmap #(
    // How many suns are summed before the ambient floor. ONE by default,
    // because the identity asks for several and the resource count has not yet
    // said what several costs -- so the cheap case ships and the knob is here.
    parameter int unsigned SUNS = 1
) (
    input var logic clk,
    input var logic rst_n,

    // ---- per triangle: the expensive term, computed once -------------------
    input  var logic               tri_valid_i,
    output var logic               tri_ready_o,
    input  var logic signed [31:0] n_x_i,          // Q16.16, un-normalised
    input  var logic signed [31:0] n_y_i,
    input  var logic signed [31:0] n_z_i,
    input  var logic               degenerate_i,   // TERRAIN.NORMALS said so
    input  var logic signed [15:0] sun_x_i,        // s1.15 unit
    input  var logic signed [15:0] sun_y_i,
    input  var logic signed [15:0] sun_z_i,
    input  var logic        [15:0] tri_src_id_i,
    output var logic               base_valid_o,   // the triangle term is ready
    output var logic signed [15:0] base_o,         // s1.15, dot(n,L)/|n|

    // ---- per fragment: two multiplies and an add ---------------------------
    input  var logic               f_valid_i,
    output var logic               f_ready_o,
    input  var logic        [15:0] d_texel_i,      // {s8 dz, s8 dx}
    input  var logic        [7:0]  strength_i,
    input  var logic        [7:0]  ambient_i,
    output var logic               s_valid_o,
    input  var logic               s_ready_i,
    output var logic        [7:0]  shade_o,        // unit8
    output var logic        [15:0] s_src_id_o,

    // ---- evidence ----------------------------------------------------------
    output var logic [31:0] triangles_o,
    output var logic [31:0] fragments_o,
    output var logic [31:0] degenerate_o,   // no direction to light from
    output var logic [31:0] saturated_o,    // shade hit the unit8 ceiling
    output var logic [31:0] floored_o       // ambient was what came out
);

  // =========================================================================
  // PER TRIANGLE
  // =========================================================================
  // |n| by restoring square root, then one restoring divide. Both walk a fixed
  // number of steps, so the triangle term has a FIXED latency and the block
  // never needs a variable-length stall -- which is what keeps the fragment
  // side free of the divider's timing.
  localparam int unsigned SQRT_STEPS = 32;   // 64-bit radicand, 2 bits a step
  localparam int unsigned DIV_STEPS  = 32;

  typedef enum logic [1:0] { T_IDLE, T_SQRT, T_DIV, T_DONE } tstate_e;
  tstate_e t_st_q;

  logic [63:0]        sq_q;        // the radicand, consumed 2 bits a step
  logic [63:0]        rem_q;
  logic [31:0]        root_q;
  logic [5:0]         step_q;
  logic signed [63:0] dot_q;       // dot(n, L): Q16.16 * s1.15
  logic               dot_neg_q;
  logic [63:0]        num_q;       // |dot|, being divided
  logic [63:0]        dr_q;        // divide remainder
  logic [31:0]        quot_q;
  logic signed [15:0] sun_x_q, sun_y_q, sun_z_q;
  logic        [15:0] tri_src_q;
  logic               tri_degen_q;

  assign tri_ready_o = (t_st_q == T_IDLE);

  // ASSUMPTION, unenforced, and this file is a quarantined draft: the squares
  // are taken in 64 bits because a Q16.16 component near one world unit squares
  // to 2^32 and three of those do not fit in 32 bits. Nobody upholds this yet --
  // the replacement block specified in reports/NORMALMAP-ARCHITECTURE.md owns
  // it, and its oracle must be written before any RTL. Stated as an assumption
  // rather than an invariant precisely because there is no enforcer.
  logic [63:0] sq_c;
  always_comb begin
    automatic logic signed [63:0] xx = 64'(n_x_i) * 64'(n_x_i);
    automatic logic signed [63:0] yy = 64'(n_y_i) * 64'(n_y_i);
    automatic logic signed [63:0] zz = 64'(n_z_i) * 64'(n_z_i);
    sq_c = 64'(xx + yy + zz);
  end

  logic signed [63:0] dot_c;
  always_comb begin
    dot_c = 64'(n_x_i) * 64'(sun_x_i) +
            64'(n_y_i) * 64'(sun_y_i) +
            64'(n_z_i) * 64'(sun_z_i);
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      t_st_q       <= T_IDLE;
      base_valid_o <= 1'b0;
      base_o       <= '0;
      triangles_o  <= '0;
      degenerate_o <= '0;
      step_q       <= '0;
      root_q       <= '0;
      rem_q        <= '0;
      quot_q       <= '0;
      dr_q         <= '0;
    end else begin
      case (t_st_q)
        T_IDLE: begin
          if (tri_valid_i) begin
            triangles_o <= triangles_o + 32'd1;
            sun_x_q     <= sun_x_i;
            sun_y_q     <= sun_y_i;
            sun_z_q     <= sun_z_i;
            tri_src_q   <= tri_src_id_i;
            tri_degen_q <= degenerate_i || (sq_c == 64'd0);
            if (degenerate_i || (sq_c == 64'd0)) begin
              // A degenerate triangle has no direction to be lit from. It is
              // reported, and its base is zero -- so the ambient floor is what
              // its fragments get, which is a dark face and not a bright one.
              degenerate_o <= degenerate_o + 32'd1;
              base_o       <= '0;
              base_valid_o <= 1'b1;
              t_st_q       <= T_DONE;
            end else begin
              base_valid_o <= 1'b0;
              sq_q         <= sq_c;
              dot_q        <= dot_c;
              dot_neg_q    <= dot_c[63];
              rem_q        <= '0;
              root_q       <= '0;
              step_q       <= 6'(SQRT_STEPS);
              t_st_q       <= T_SQRT;
            end
          end
        end

        T_SQRT: begin
          // One restoring step: bring down two bits, trial-subtract 2*root+1.
          begin
            automatic logic [63:0] r2 = {rem_q[61:0],
                                         sq_q[63 -: 2]};
            automatic logic [63:0] trial = {32'd0, root_q, 1'b1};
            if (r2 >= trial) begin
              rem_q  <= r2 - trial;
              root_q <= {root_q[30:0], 1'b1};
            end else begin
              rem_q  <= r2;
              root_q <= {root_q[30:0], 1'b0};
            end
            sq_q <= {sq_q[61:0], 2'b00};
          end
          if (step_q == 6'd1) begin
            step_q <= 6'(DIV_STEPS);
            // |dot| enters the divide; the sign is reapplied at the end,
            // because a restoring divide is unsigned and a negative numerator
            // would otherwise come out as a very large positive one.
            num_q  <= dot_neg_q ? 64'(-dot_q) : 64'(dot_q);
            dr_q   <= '0;
            quot_q <= '0;
            t_st_q <= T_DIV;
          end else begin
            step_q <= step_q - 6'd1;
          end
        end

        T_DIV: begin
          begin
            automatic logic [63:0] r1 = {dr_q[62:0], num_q[63]};
            automatic logic [63:0] den = {32'd0, root_q};
            if (r1 >= den) begin
              dr_q   <= r1 - den;
              quot_q <= {quot_q[30:0], 1'b1};
            end else begin
              dr_q   <= r1;
              quot_q <= {quot_q[30:0], 1'b0};
            end
            num_q <= {num_q[62:0], 1'b0};
          end
          if (step_q == 6'd1) begin
            t_st_q <= T_DONE;
          end else begin
            step_q <= step_q - 6'd1;
          end
        end

        T_DONE: begin
          if (!tri_degen_q) begin
            // Saturate into s1.15. A face aimed straight at the light lands at
            // +32767 and one aimed away stays NEGATIVE on purpose: the clamp
            // belongs with the ambient floor, and clamping here would throw
            // away the sign the fragment term is added to.
            automatic logic [31:0] q = quot_q;
            automatic logic signed [16:0] sgned =
                (q > 32'd32767) ? 17'sd32767 : 17'(signed'({1'b0, q[15:0]}));
            base_o       <= dot_neg_q ? 16'(-sgned) : 16'(sgned);
            base_valid_o <= 1'b1;
          end
          t_st_q <= T_IDLE;
        end

        default: t_st_q <= T_IDLE;
      endcase
    end
  end

  // =========================================================================
  // PER FRAGMENT
  // =========================================================================
  // Two multiplies and an add. The format scaling is exact and free:
  // strength/256 * d/128 expressed in s1.15 is strength*d*32768/(256*128),
  // and 32768/(256*128) is 1. That the three Q formats cancel to a bare
  // product is why they were chosen.
  logic signed [7:0] d_x_c, d_z_c;
  assign d_x_c = signed'(d_texel_i[7:0]);
  assign d_z_c = signed'(d_texel_i[15:8]);

  logic signed [31:0] ddot_c;      // dot(d, L), s8 * s1.15
  assign ddot_c = 32'(d_x_c) * 32'(sun_x_q) + 32'(d_z_c) * 32'(sun_z_q);

  logic signed [47:0] detail_c;    // strength * dot(d, L), back to s1.15
  assign detail_c = (48'(ddot_c) * 48'({1'b0, strength_i})) >>> 15;

  logic signed [47:0] sum_c;
  assign sum_c = 48'(base_o) + detail_c;

  logic [31:0] u_c;
  assign u_c = (sum_c <= 0) ? 32'd0 : 32'((sum_c * 48'sd256) >>> 15);

  logic [7:0] shade_c;
  logic       sat_c, flr_c;
  assign sat_c   = (u_c > 32'd255);
  assign shade_c = sat_c ? 8'd255 : u_c[7:0];
  assign flr_c   = (shade_c < ambient_i);

  assign f_ready_o = !s_valid_o || s_ready_i;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      s_valid_o   <= 1'b0;
      shade_o     <= '0;
      s_src_id_o  <= '0;
      fragments_o <= '0;
      saturated_o <= '0;
      floored_o   <= '0;
    end else begin
      if (s_valid_o && s_ready_i) s_valid_o <= 1'b0;
      if (f_valid_i && f_ready_o) begin
        fragments_o <= fragments_o + 32'd1;
        // Ambient is a FLOOR, not an addend. Adding it would lift a lit face
        // past white and flatten exactly the shapes the detail normals were
        // put there to show.
        shade_o     <= flr_c ? ambient_i : shade_c;
        s_src_id_o  <= tri_src_q;
        s_valid_o   <= 1'b1;
        if (sat_c) saturated_o <= saturated_o + 32'd1;
        if (flr_c) floored_o   <= floored_o + 32'd1;
      end
    end
  end

  // SUNS is a knob for the cost of several suns, not yet a second accumulator:
  // the ruling is to measure the one-sun path first. Referenced so the
  // parameter cannot silently drift out of the fit.
  /* verilator lint_off UNUSEDPARAM */
  localparam int unsigned SUNS_DECLARED = SUNS;
  /* verilator lint_on UNUSEDPARAM */

endmodule : zhao_terrain_normalmap

`default_nettype wire
