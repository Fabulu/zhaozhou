// zhao_texture_combine.sv -- TEXTURE.COMBINE, the material combiner.
// Authored 2026-09-05 for roadmap gate G1-C.
//
// ---------------------------------------------------------------------------
// WHAT THIS IS AND WHY IT IS THE BLOCKING PIECE
// ---------------------------------------------------------------------------
// The texture island fetches up to three samples per fragment and combines
// NONE of them: the surviving TEXJOIN returns sample 0 for every recipe, and
// TEXTURE.FRAGROB explicitly refuses to own this arithmetic. Fetching three
// textures without implementing their combination is not three-sample material
// support, and terrain material work has been blocked on this block existing.
//
// Its law is `zref::material::combine`
// (reference/include/zref/zref_material.hpp, written 2026-09-05). This RTL is
// differentiated against that oracle -- the oracle is the authority, not this
// file.
//
// ---------------------------------------------------------------------------
// THE ONE ARITHMETIC RULE
// ---------------------------------------------------------------------------
// unit8: value = raw/256, so 255 is the largest representable value and NOT
// 1.0. The product is the frozen `((a*b) + 128) >> 8`, clamp 255
// (spec/qformats.md 3). ONE rounding per result.
//
// A consequence worth knowing before "fixing" anything here: modulate by 255 is
// IDENTITY for every input <= 128 and subtracts exactly 1 above it. It does not
// always darken. The directed suite sweeps that boundary across all 256 inputs.
//
// ---------------------------------------------------------------------------
// EXCLUSIONS, each a specific refusal
// ---------------------------------------------------------------------------
// No sampling (TEXTURE.TMU fetched them). No resolution (MATERIAL.RESOLVE
// looked the recipe up). No framebuffer blend -- that is RASTER.FRAGMENT's, and
// the distinction is that this combines SOURCES WITH EACH OTHER while that
// combines the RESULT WITH WHAT IS ALREADY THERE. No toon quantisation
// (RASTER.TOON). No fog: owner ruling D-5 puts fog on the final source colour
// AFTER material combination, i.e. downstream of here, and this block must not
// pre-empt that ordering.
//
// ---------------------------------------------------------------------------
// STRUCTURE, AND AN OPEN QUESTION THIS BLOCK DOES NOT SETTLE
// ---------------------------------------------------------------------------
// The contract says II = 1: one fragment per clock, fully pipelined. The
// architecture's 15.3 instead describes TWO product lanes accepting two jobs
// per clock, with continuation jobs queued -- which is not II = 1 for a
// four-channel multiplying recipe.
//
// This implementation takes the CONTRACT's II = 1 and spends four parallel
// byte products, because that is the shape that meets "one fragment per clock"
// and it is the cheaper thing to measure first. If the fit shows DSP > 2 (the
// 3.4 tripwire) or ALM > 650, that measurement is the evidence for moving to
// the two-lane scheduled form -- and it will be a decision made against a
// number rather than ahead of one.
//
// Conservative SystemVerilog subset (charter 2). No package dependencies.

module zhao_texture_combine (
    input  logic        clk,
    input  logic        rst_n,

    // ---- fragment in -------------------------------------------------------
    input  logic        f_valid_i,
    output logic        f_ready_o,
    input  logic [1:0]  f_sample_count_i,   // 0..3
    input  logic [2:0]  f_recipe_i,         // 0..5; anything else is refused
    input  logic [7:0]  f_weight_i,         // unit8, kLerp only
    input  logic [23:0] f_s0_rgb_i,
    input  logic [7:0]  f_s0_a_i,
    input  logic [23:0] f_s1_rgb_i,
    input  logic [7:0]  f_s1_a_i,
    // The untextured colour, used when sample_count == 0. Named `base` to match
    // the oracle: TEXTURE.COMBINE.md says "the fragment's vertex colour" but
    // its packet list has no such field, so the binding is made by the CALLER
    // rather than guessed at here.
    input  logic [23:0] f_base_rgb_i,
    input  logic [7:0]  f_base_a_i,
    input  logic [15:0] f_tag_i,

    // ---- fragment out ------------------------------------------------------
    output logic        o_valid_o,
    input  logic        o_ready_i,
    output logic [23:0] o_rgb_o,
    output logic [7:0]  o_a_o,
    output logic [15:0] o_tag_o,
    output logic        o_refused_o,        // malformed; see the counters

    // ---- counters ----------------------------------------------------------
    // Every refusal and every saturation is counted. Quietly accepting a bad
    // input is how a content bug becomes a shipped picture nobody questions,
    // and the saturation counts tell content authors when a recipe clips.
    output logic [31:0] refused_recipe_o,
    output logic [31:0] refused_missing_o,
    output logic [31:0] saturated_add_o,
    output logic [31:0] saturated_mul2x_o
);

  // ---- the ratified encodings, numbered to match texjoin_v2 and fragrob -----
  localparam logic [2:0] R_PASSTHRU   = 3'd0;
  localparam logic [2:0] R_MODULATE   = 3'd1;
  localparam logic [2:0] R_MODULATE2X = 3'd2;
  localparam logic [2:0] R_LERP       = 3'd3;
  localparam logic [2:0] R_ADDSAT     = 3'd4;
  localparam logic [2:0] R_MASK       = 3'd5;
  localparam logic [2:0] R_COUNT      = 3'd6;

  // ---- the frozen unit8 product --------------------------------------------
  // ((a*b) + 128) >> 8, clamp 255. The clamp is unreachable for 8x8 inputs and
  // is kept defensively exactly as the frozen pseudocode states it.
  function automatic logic [7:0] unit_mul(input logic [7:0] a, input logic [7:0] b);
    // p[7:0] is the DISCARDED REMAINDER of the >>8 -- the same shape
    // zhao_raster_div255 documents. The quotient is the only thing this
    // function exists to produce.
    /* verilator lint_off UNUSEDSIGNAL */
    logic [16:0] p;
    /* verilator lint_on UNUSEDSIGNAL */
    begin
      p = ({9'd0, a} * {9'd0, b}) + 17'd128;
      unit_mul = (p[16:8] > 9'd255) ? 8'd255 : p[15:8];
    end
  endfunction

  // ---- one input beat, registered ------------------------------------------
  // II = 1 with a single register stage: accept when the output slot is free or
  // is being drained this cycle. A skid is not needed because the arithmetic is
  // combinational between the two registers.
  logic        s_valid_q;
  logic [1:0]  s_cnt_q;
  logic [2:0]  s_recipe_q;
  logic [7:0]  s_w_q;
  logic [23:0] s0_rgb_q, s1_rgb_q, base_rgb_q;
  logic [7:0]  s0_a_q,   s1_a_q,   base_a_q;
  logic [15:0] s_tag_q;

  assign f_ready_o = (!s_valid_q) || (o_valid_o && o_ready_i) || (!o_valid_o);

  // ---- the arithmetic, combinational on the registered beat -----------------
  logic [7:0] r0, g0, b0, a0, r1, g1, b1, a1;
  assign {r0, g0, b0} = s0_rgb_q;
  assign a0 = s0_a_q;
  // A recipe needing two samples with only one supplied is REFUSED below, so
  // s1 is only ever consumed when it is real.
  assign {r1, g1, b1} = s1_rgb_q;
  assign a1 = s1_a_q;

  // Returns {overflow, value}. The overflow bit is the CARRY, not a test of the
  // result: 128 + 127 is exactly 255 and is NOT saturation. Inferring it from
  // `result == 255` over-counted by exactly those cases, which the oracle
  // differential caught at 233 against 231 -- and the directed suite already
  // asserted "128 + 127 is exactly 255, the last unsaturated value".
  function automatic logic [8:0] add_sat9(input logic [7:0] a, input logic [7:0] b);
    logic [8:0] s;
    begin
      s = {1'b0, a} + {1'b0, b};
      add_sat9 = s[8] ? 9'b1_11111111 : {1'b0, s[7:0]};
    end
  endfunction

  // s0*s1*2 with ONE rounding: double AFTER the frozen product. Rounding twice
  // would drift from the oracle by a least-significant bit on exactly the
  // values a directed test is least likely to try.
  // Returns {overflow, value}, same reasoning as add_sat9: a product that
  // doubles to exactly 255 has not saturated.
  function automatic logic [8:0] mul2x9(input logic [7:0] a, input logic [7:0] b);
    logic [8:0] d;
    begin
      d = {1'b0, unit_mul(a, b)} << 1;
      mul2x9 = d[8] ? 9'b1_11111111 : {1'b0, d[7:0]};
    end
  endfunction

  // lerp on a SIGNED difference with the magnitude rounded and the sign
  // reapplied, so a darkening lerp moves the same distance as its mirror.
  function automatic logic [7:0] lerp8(input logic [7:0] a, input logic [7:0] b,
                                       input logic [7:0] w);
    logic signed [9:0] d;
    logic [7:0]        mag;
    logic [7:0]        scaled;
    logic signed [10:0] r;
    begin
      d      = $signed({2'b0, b}) - $signed({2'b0, a});
      mag    = d[9] ? 8'(-d) : 8'(d);
      scaled = unit_mul(mag, w);
      r      = d[9] ? ($signed({3'b0, a}) - $signed({3'b0, scaled}))
                    : ($signed({3'b0, a}) + $signed({3'b0, scaled}));
      lerp8  = (r < 0) ? 8'd0 : ((r > 255) ? 8'd255 : 8'(r));
    end
  endfunction

  logic       bad_recipe_c, bad_missing_c, untextured_c;
  logic [7:0] cr, cg, cb, ca;
  logic [8:0] mr, mg, mb, ma;   // {overflow, value} from the saturating helpers
  logic       sat_add_c, sat_mul2x_c;

  always_comb begin
    untextured_c  = (s_cnt_q == 2'd0);
    bad_recipe_c  = (s_recipe_q >= R_COUNT) && !untextured_c;
    // PASSTHRU needs one sample; every other ratified recipe needs two.
    bad_missing_c = !untextured_c && !bad_recipe_c &&
                    ((s_recipe_q != R_PASSTHRU) && (s_cnt_q < 2'd2));

    cr = 8'd0; cg = 8'd0; cb = 8'd0; ca = 8'd0;
    mr = 9'd0; mg = 9'd0; mb = 9'd0; ma = 9'd0;
    sat_add_c = 1'b0; sat_mul2x_c = 1'b0;

    if (untextured_c) begin
      // An untextured surface is LEGAL and common; it must not need a dummy
      // sample and it is not a refusal.
      {cr, cg, cb} = base_rgb_q;
      ca           = base_a_q;
    end else if (bad_recipe_c || bad_missing_c) begin
      // A refused fragment does NOT silently degrade to passthrough, which is
      // exactly what the surviving TEXJOIN does and why a wrong material looks
      // plausible.
      cr = 8'd0; cg = 8'd0; cb = 8'd0; ca = 8'd0;
    end else begin
      unique case (s_recipe_q)
        R_PASSTHRU: begin
          cr = r0; cg = g0; cb = b0; ca = a0;
        end
        R_MODULATE: begin
          cr = unit_mul(r0, r1); cg = unit_mul(g0, g1);
          cb = unit_mul(b0, b1); ca = unit_mul(a0, a1);
        end
        R_MODULATE2X: begin
          mr = mul2x9(r0, r1); mg = mul2x9(g0, g1);
          mb = mul2x9(b0, b1); ma = mul2x9(a0, a1);
          cr = mr[7:0]; cg = mg[7:0]; cb = mb[7:0]; ca = ma[7:0];
          sat_mul2x_c = mr[8] | mg[8] | mb[8] | ma[8];
        end
        R_LERP: begin
          cr = lerp8(r0, r1, s_w_q); cg = lerp8(g0, g1, s_w_q);
          cb = lerp8(b0, b1, s_w_q); ca = lerp8(a0, a1, s_w_q);
        end
        R_ADDSAT: begin
          mr = add_sat9(r0, r1); mg = add_sat9(g0, g1);
          mb = add_sat9(b0, b1); ma = add_sat9(a0, a1);
          cr = mr[7:0]; cg = mg[7:0]; cb = mb[7:0]; ca = ma[7:0];
          sat_add_c = mr[8] | mg[8] | mb[8] | ma[8];
        end
        R_MASK: begin
          // The pass test is the mask's ALPHA. MASK exists so a shape's alpha
          // can cut a colour; testing RGB would let a black-but-opaque mask
          // erase what it was meant to keep.
          if (a1 != 8'd0) begin
            cr = r0; cg = g0; cb = b0; ca = a0;
          end
        end
        default: begin
          cr = 8'd0; cg = 8'd0; cb = 8'd0; ca = 8'd0;
        end
      endcase
    end
  end

  // ---- registers ------------------------------------------------------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      s_valid_q <= 1'b0;
      o_valid_o <= 1'b0;
      o_rgb_o   <= 24'd0;
      o_a_o     <= 8'd0;
      o_tag_o   <= 16'd0;
      o_refused_o       <= 1'b0;
      refused_recipe_o  <= 32'd0;
      refused_missing_o <= 32'd0;
      saturated_add_o   <= 32'd0;
      saturated_mul2x_o <= 32'd0;
    end else begin
      if (o_valid_o && o_ready_i) o_valid_o <= 1'b0;

      // stage 2: retire the registered beat
      if (s_valid_q && (!o_valid_o || o_ready_i)) begin
        o_valid_o   <= 1'b1;
        o_rgb_o     <= {cr, cg, cb};
        o_a_o       <= ca;
        o_tag_o     <= s_tag_q;   // rides through UNTOUCHED; retirement order
        o_refused_o <= bad_recipe_c | bad_missing_c;
        if (bad_recipe_c)  refused_recipe_o  <= refused_recipe_o  + 32'd1;
        if (bad_missing_c) refused_missing_o <= refused_missing_o + 32'd1;
        // counted once per FRAGMENT, not per channel
        if (sat_add_c)   saturated_add_o   <= saturated_add_o   + 32'd1;
        if (sat_mul2x_c) saturated_mul2x_o <= saturated_mul2x_o + 32'd1;
        s_valid_q <= 1'b0;
      end

      // stage 1: accept
      if (f_valid_i && f_ready_o) begin
        s_valid_q  <= 1'b1;
        s_cnt_q    <= f_sample_count_i;
        s_recipe_q <= f_recipe_i;
        s_w_q      <= f_weight_i;
        s0_rgb_q   <= f_s0_rgb_i;
        s0_a_q     <= f_s0_a_i;
        s1_rgb_q   <= f_s1_rgb_i;
        s1_a_q     <= f_s1_a_i;
        base_rgb_q <= f_base_rgb_i;
        base_a_q   <= f_base_a_i;
        s_tag_q    <= f_tag_i;
      end
    end
  end

endmodule
