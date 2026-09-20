// zhao_view_projscale.sv -- the two views' PROJECTION SCALE TERMS, snooped off
// the projector configuration bus. Owner ruling R68 sub-build 4.
//
// ENFORCED-BY: tests/geometry/geom_projradius_directed.cpp:main
//
// WHAT THIS IS FOR
//
// `zref::creature::projected_bound_radius_q8` -- THE law GEOM.LOD's
// `proj_radius_q8_i` comes from -- is:
//
//     kx = max(|vp.m[0][0]|, |vp.m[0][1]|, |vp.m[0][2]|)
//     radius_q8 = round_half_up( kx * bound_radius * viewport_w * 128
//                                / (clip.w << 16) )
//
// Three of those four operands already exist in this console: `bound_radius`
// comes from `zhao_geom_ladderbank`, and `clip.w` is the shared projector's
// own `a_w_i` for the instance centre. The other two -- `kx` and
// `viewport_w` -- are FUNCTIONS OF THE VIEW, they change only when a SetView
// lands, and they are both already on the projector configuration bus.
//
// WHY IT SNOOPS RATHER THAN BEING THREE MORE OUTPUTS ON `zhao_project_core`
//
// Exactly `zhao_view_eye`'s argument, which is the precedent this file
// follows: `zhao_project_core` is a LEAF with seven instantiation sites plus a
// committed mutant copy, NOT ONE of which wants these values, and CLAUDE.md
// records that "a port on a leaf costs its WHOLE instantiation chain plus every
// bench". So this block sits on the same bus and decodes the addresses it
// needs. `zhao_project_core` drives nothing here and is not stalled by it.
//
// It reads addresses the projector ALSO decodes (0/1/2 and 17), which is the
// one difference from `zhao_view_eye` and is deliberate: these are not a hole
// in the address map, they are the matrix and the viewport the projector is
// already using. Two READERS of one write is not two writers -- and it is the
// only arrangement in which this block cannot disagree with the projector
// about what the current view is, because there is no second path for the
// value to arrive by.
//
// WHY kx IS A MAX OF THREE AND NOT m[0][0]
//
// Because the reference says so, and the reference is right for a reason worth
// writing down: `vp` is a FUSED view-projection, so a camera that is not
// looking down an axis puts its horizontal scale into a combination of the
// three row-0 elements. `m[0][0]` alone reads ZERO for a camera looking along
// world X, which would make every creature on screen report a zero projected
// radius and collapse the whole ladder to its coarsest rung -- silently, and
// only for some camera angles. The max is a BOUND on the row's contribution,
// which is the conservative choice for a level-of-detail test: it can only
// make the ladder finer than the truth, never coarser.
//
// |INT32_MIN| SATURATES AND IS COUNTED. `std::abs` on it is undefined in C++
// and wrapping is wrong in hardware, so the magnitude saturates at INT32_MAX
// and `abs_saturated_o` says it happened. A matrix element at exactly -2^31 is
// -32768.0 in fx16 and is not a camera anyone authored; the counter is here
// because a number that cannot occur is exactly the kind that occurs.
//
// It performs no arithmetic beyond three magnitudes and two compares (0 DSP),
// infers no memory (0 M10K) and back-pressures nothing.
//
// Conservative SystemVerilog subset only (charter section 2).
`default_nettype none

module zhao_view_projscale #(
    // The projector's row-0 matrix addresses. NAMED AND EDITABLE (CLAUDE.md
    // rule 6): `zhao_project_core`'s port-list comment is the address map and
    // says "addr 0..15 : matrix row-major m[0..15]", so row 0 is 0/1/2 -- but
    // the map is the projector's to change and this block should move by
    // parameter when it does, not by rewrite.
    parameter int unsigned M00_ADDR = 0,
    parameter int unsigned M01_ADDR = 1,
    parameter int unsigned M02_ADDR = 2,
    // "addr 17 : { h [27:16], w [11:0] }".
    parameter int unsigned RECT_WH_ADDR = 17
) (
    input  wire clk,
    input  wire rst_n,

    // ---- the projector configuration bus, SNOOPED -------------------------
    // Identical wires to `zhao_project_core`'s `cfg_*`. This block never drives
    // them and never stalls them.
    input  wire        cfg_we_i,
    input  wire        cfg_view_i,
    input  wire [ 4:0] cfg_addr_i,
    input  wire [31:0] cfg_data_i,

    // ---- per view: the row-0 magnitude bound and the viewport width -------
    // Registered configuration, not pipeline data: each output is whatever the
    // last SetView for that view wrote, and it changes only when one does.
    output logic [31:0] kx0_o,
    output logic [31:0] kx1_o,
    output logic [11:0] vw0_o,
    output logic [11:0] vw1_o,

    // ---- evidence ----------------------------------------------------------
    output logic [31:0] abs_saturated_o
);

  // Quartus 17.0 needs an elaboration check inside `initial begin ... end`; a
  // bare module-scope `if` is a syntax error there however clean the lint
  // (CLAUDE.md, "Verilator lint-clean is not Quartus-synthesizable"). And
  // `--lint-only` does not run this block, so a clean lint is NOT evidence
  // about it -- it is fired by parameter override in the directed test.
  initial begin
    if (M00_ADDR > 31 || M01_ADDR > 31 || M02_ADDR > 31 || RECT_WH_ADDR > 31)
      $fatal(1, "zhao_view_projscale: cfg_addr_i is five bits; an address is out of range");
    if (M00_ADDR == M01_ADDR || M01_ADDR == M02_ADDR || M00_ADDR == M02_ADDR
        || M00_ADDR == RECT_WH_ADDR || M01_ADDR == RECT_WH_ADDR || M02_ADDR == RECT_WH_ADDR)
      $fatal(1, "zhao_view_projscale: the four snooped addresses must be distinct");
  end

  // The three row-0 MAGNITUDES, per view. Stored as magnitudes rather than as
  // the raw words because the only consumer is their maximum, and storing the
  // absolute value at write time keeps the read path to two compares.
  logic [31:0] a0 [0:1];
  logic [31:0] a1 [0:1];
  logic [31:0] a2 [0:1];
  logic [11:0] vw [0:1];

  // |x|, saturating at INT32_MAX. The saturating case is x == INT32_MIN, whose
  // magnitude is 2^31 and does not fit a signed 32-bit result.
  logic [31:0] mag_c;
  logic        mag_sat_c;
  assign mag_sat_c = (cfg_data_i == 32'h8000_0000);
  // The sign bit alone decides the negation, so the test is `cfg_data_i[31]`
  // rather than a signed compare against zero: a `$signed` copy of the whole
  // word would leave thirty-one bits unread, which -Wall reports and which
  // would be true.
  assign mag_c     = mag_sat_c ? 32'h7FFF_FFFF
                               : (cfg_data_i[31] ? (32'd0 - cfg_data_i) : cfg_data_i);

  wire hit_m_c = cfg_we_i && ((cfg_addr_i == 5'(M00_ADDR)) || (cfg_addr_i == 5'(M01_ADDR))
                              || (cfg_addr_i == 5'(M02_ADDR)));

  integer v;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      for (v = 0; v < 2; v = v + 1) begin
        // ZERO IS THE RESET, AND IT IS THE HONEST ONE. A view with no matrix
        // has no projection, so its scale term is zero and every creature in
        // it reports a zero projected radius -- which the ladder reads as "too
        // small to see", the correct answer for a camera that has not been
        // configured. The alternative, an invented unity, would put creatures
        // on screen at a scale nobody wrote.
        a0[v] <= 32'd0;
        a1[v] <= 32'd0;
        a2[v] <= 32'd0;
        vw[v] <= 12'd0;
      end
      abs_saturated_o <= 32'd0;
    end else if (cfg_we_i) begin
      // One address, one register. A write to any other address is not this
      // block's business and is silently ignored -- exactly as
      // `zhao_project_core` ignores 19..21.
      if (cfg_addr_i == 5'(M00_ADDR)) a0[cfg_view_i] <= mag_c;
      else if (cfg_addr_i == 5'(M01_ADDR)) a1[cfg_view_i] <= mag_c;
      else if (cfg_addr_i == 5'(M02_ADDR)) a2[cfg_view_i] <= mag_c;
      else if (cfg_addr_i == 5'(RECT_WH_ADDR)) vw[cfg_view_i] <= cfg_data_i[11:0];
      if (hit_m_c && mag_sat_c) abs_saturated_o <= abs_saturated_o + 32'd1;
    end
  end

  // kx = max of the three magnitudes. Two compares, no arithmetic.
  function automatic logic [31:0] max3(input logic [31:0] x, input logic [31:0] y,
                                       input logic [31:0] z);
    logic [31:0] m;
    begin
      m = (x >= y) ? x : y;
      max3 = (m >= z) ? m : z;
    end
  endfunction

  assign kx0_o = max3(a0[0], a1[0], a2[0]);
  assign kx1_o = max3(a0[1], a1[1], a2[1]);
  assign vw0_o = vw[0];
  assign vw1_o = vw[1];

endmodule

`default_nettype wire
