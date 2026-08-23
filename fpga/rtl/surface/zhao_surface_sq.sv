// zhao_surface_sq.sv — the sequential shift-add SQUARER shared by SURFACE.STAMP.
//
// WHY THIS EXISTS. `zhao_surface_stamp` used to compute its coverage geometry
// with six multiply operators — `r*r`, `r_inner*r_inner`, two 41-bit texel-centre
// products and `dx*dx + dz*dz` — and the fitter spent **28 of the device's 112
// DSP blocks** on them (measured, reports/synthesis/zhao_block_fit.json,
// 96c0394). Four of the six are *squares*, and a square is `|v|^2`, so one
// unsigned shift-add engine serves all four. The other two became accumulators
// and are gone entirely (see zhao_surface_stamp.sv, "the texel centre").
//
// WHY IT IS ALLOWED TO BE SLOW. The demand derived from Sacrifice's own SCAR
// system (docs/OWNER_DOCKET.md, "THE THREE DEMAND NUMBERS") is **20,000 stamp
// texels per frame** — one texel per ~83 clocks at the 100 MHz gpu_clk
// placeholder. The block it serves was provisioned for one texel per clock. At
// 83 clocks a texel there is no reason for a parallel multiplier to exist.
//
// THE ARITHMETIC, and why it is bit-identical rather than merely close:
//
//   m    = |a|                        (MAG_W bits unsigned; |a| <= 2^(MAG_W-1))
//   acc  = sum over set bits k of m of (m << k), accumulated MOD 2^ACC_W
//   sq_o = acc
//
// `a * a == |a| * |a|` over the integers, so dropping the sign is exact rather
// than an approximation. Accumulating modulo 2^ACC_W reproduces exactly the
// truncation `64'(dx) * 64'(dx)` already performs in the shipped block —
// including the corner a = -2^35, where the true square is 2^70 and both forms
// give 0. **So this module is a drop-in for the shipped multiply on EVERY
// input, inside the block's stated +-4,096 m domain and outside it.** No domain
// narrowing is taken here and none is needed: design/contracts/SURFACE.STAMP.md
// flags narrowing as "a real optimisation and a real risk", and since the DSPs
// come out without it, paying that risk would buy nothing.
//
// THE ADDEND SHIFTS; THE SHIFT AMOUNT IS NEVER A VARIABLE. `sh_q` is a register
// that shifts LEFT by SQ_RADIX each cycle, so `sh_q << b` below is a
// compile-time constant shift by a genvar — pure wiring, not a barrel shifter.
// That matters for area, and it matters more for the DSP count:
// reports/QUARTUS_GOTCHAS.md §3 records that `(* multstyle = "logic" *)` is
// accepted by Quartus 17.0.2 and **silently ignored**, with a DSP count that
// will not fall as the only symptom. The only reliable way not to get a DSP is
// not to write a `*`. There is no `*` operator anywhere in this file.
//
// LATENCY. `start_i` at cycle L; Steps = ceil(MAG_W / SQ_RADIX) step cycles at
// L+1 .. L+Steps; `vld_o` and `sq_o` are valid from L+Steps+1 and hold until the
// next `start_i`. Fixed and data-independent, deliberately: an early-out on
// `bits_q == 0` would be faster on typical data and would make the block's
// throughput a function of where the scar landed, which is not a number anyone
// could budget against.
//
// Conservative SystemVerilog subset only (charter 2).

module zhao_surface_sq #(
    // Magnitude width. SURFACE.STAMP squares dx/dz (signed 36), the radius
    // (signed 32) and r_inner (signed 33, always >= 0); 36 covers all three.
    parameter int unsigned MAG_W = 36,
    // Accumulate width = the width the shipped product was truncated to.
    parameter int unsigned ACC_W = 64,
    // Bits retired per cycle: the resource frontier axis. A chain of SQ_RADIX
    // conditional ACC_W-bit adds, so cycles fall as 1/SQ_RADIX while the
    // combinational cone grows linearly with it.
    parameter int unsigned SQ_RADIX = 1
) (
    input logic clk,
    input logic rst_n,

    input logic                    start_i,  // 1-cycle pulse: latch a_i and begin
    input logic signed [MAG_W-1:0] a_i,

    output logic             vld_o,  // LEVEL, not a pulse: holds until the next start
    output logic [ACC_W-1:0] sq_o
);

  // An illegal radix must stop elaboration, not degrade quietly. The same
  // portable static assertion zhao_geom_skin uses for MUL_LANES: an unresolved
  // module reference inside a generate-if. `$error` is deliberately avoided —
  // Quartus 17.0.2's support for elaboration system tasks is not something to
  // discover during a two-hour fit. THE `generate` KEYWORDS ARE NOT OPTIONAL at
  // module scope (QUARTUS_GOTCHAS §8).
  //
  // Only 1, 2 and 4 are legal: `bits_q >> SQ_RADIX` and `sh_q << SQ_RADIX` must
  // tile MAG_W without a partial final step, and 36 is divisible by all three.
  generate
    if (!(SQ_RADIX == 1 || SQ_RADIX == 2 || SQ_RADIX == 4)) begin : g_illegal
      ZHAO_SURFACE_SQ_RADIX_MUST_BE_1_2_OR_4 u_static_assert ();
    end
  endgenerate

  localparam int unsigned Steps = (MAG_W + SQ_RADIX - 1) / SQ_RADIX;

  // |a| as an unsigned magnitude. For a = -2^(MAG_W-1) the two's-complement
  // negation is 2^(MAG_W-1), exactly representable in MAG_W UNSIGNED bits —
  // this is the one input where a naive signed `-a` gives back a negative
  // number and the square comes out wrong.
  wire [MAG_W-1:0] a_u = a_i;
  wire [MAG_W-1:0] mag = a_i[MAG_W-1] ? (~a_u + {{(MAG_W - 1) {1'b0}}, 1'b1}) : a_u;

  logic [MAG_W-1:0] bits_q;  // remaining multiplier bits, shifted RIGHT
  logic [ACC_W-1:0] sh_q;  // the addend, shifted LEFT (dropping high bits IS the mod)
  logic [ACC_W-1:0] acc_q;
  logic [      7:0] cnt_q;  // Steps <= 36 for every legal MAG_W/SQ_RADIX pair here

  // SQ_RADIX conditional adds, chained. `b` is a genvar, so `sh_q << b` is a
  // constant shift and costs wiring rather than logic.
  logic [ACC_W-1:0] chain[SQ_RADIX+1];
  assign chain[0] = acc_q;
  genvar b;
  generate
    for (b = 0; b < int'(SQ_RADIX); b = b + 1) begin : g_chain
      assign chain[b+1] = chain[b] + (bits_q[b] ? (sh_q << b) : {ACC_W{1'b0}});
    end
  endgenerate

  assign sq_o = acc_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      bits_q <= {MAG_W{1'b0}};
      sh_q   <= {ACC_W{1'b0}};
      acc_q  <= {ACC_W{1'b0}};
      cnt_q  <= 8'd0;
      vld_o  <= 1'b0;
    end else if (start_i) begin
      // start_i takes priority over an in-flight square, so a stamp abandoned
      // mid-scan cannot strand the engine for the next one.
      bits_q <= mag;
      sh_q   <= {{(ACC_W - MAG_W) {1'b0}}, mag};
      acc_q  <= {ACC_W{1'b0}};
      cnt_q  <= 8'(Steps);
      vld_o  <= 1'b0;
    end else if (cnt_q != 8'd0) begin
      acc_q  <= chain[SQ_RADIX];
      sh_q   <= sh_q << SQ_RADIX;
      bits_q <= bits_q >> SQ_RADIX;
      cnt_q  <= cnt_q - 8'd1;
      if (cnt_q == 8'd1) vld_o <= 1'b1;
    end
  end

endmodule
