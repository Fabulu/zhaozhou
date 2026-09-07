// zhao_raster_fog.sv — RASTER.FOG: the deterministic fog mix, applied to the
// FINAL SOURCE COLOUR.
//
// ENFORCED-BY: tests/raster/raster_fog_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHY THIS BLOCK EXISTS AND WHY IT SITS WHERE IT SITS
// ---------------------------------------------------------------------------
// Owner ruling D-5 (2026-09-03), written into spec/qformats.md §8:
//
//   | 1 | lighting                          |
//   | 2 | interpolate lighting + fog factor |
//   | 3 | toon quantisation                 |
//   | 4 | texture/material combination      |
//   | 5 | FOG THE FINAL SOURCE RGB          |  <- this block
//   | 6 | framebuffer blend                 |
//
// So it goes AFTER `zhao_raster_toon` and after TEXTURE.COMBINE, and BEFORE
// `zhao_raster_blend`. The order is the whole ruling, not a detail of it.
//
// THE REASON, because a later reader will be tempted to fold this into an
// earlier stage and save a pipeline slot: a toon ramp is a QUANTISER. Colour
// handed to it with fog already mixed in arrives simply as "darker", which is
// indistinguishable from "less lit" — both snap to the same band edge. A smooth
// depth gradient becomes a staircase, and because the fog factor changes as the
// object moves, THE STAIRCASE MOVES. That is what makes it read as a bug rather
// than a style. Fogging after the quantiser lets the bands describe lighting,
// which is what bands are for, and lets fog fade smoothly across them.
//
// D-5 names a second error in the old order too: texture modulation multiplying
// the fog colour itself. Sitting after combination fixes that one as well.
//
// ---------------------------------------------------------------------------
// THE ARITHMETIC IS THE REFERENCE'S, NOT A NEW ONE
// ---------------------------------------------------------------------------
// `reference/src/zrender/rast.cpp` is the oracle and this block reproduces it
// exactly:
//
//     f8   = clamp((f + 128) >> 8, 0, 255)   // fx16 -> unit8, §2 round-half-up
//     amt8 = 255 - f8                        // weight TOWARD the fog colour
//     c'   = sat_u8( c + ((fog_c - c) * amt8 + 128) >> 8 )
//
// POLARITY, which is the one thing easy to get backwards. §8's surviving factor
// law says f = 0x10000 is CLEAR and f = 0 is FULL FOG. §8's own mix formula
// weights by f8 instead of its complement, which taken literally returns the fog
// colour at "clear" — inverted fog. D-5 REPLACED that mix ("everything from
// Mix (frozen) to the end of this subsection"), keeping it only as the record,
// so the complement here is the reading consistent with the surviving law.
//
// UNIT8 IS /256, NOT /255. zhao_raster_blend.sv's header argues this at length
// and notes the ratified fog mix behaves identically: amt8 = 255 is 255/256,
// NOT 1.0, so full fog lands one step short of the pure fog colour. That is the
// unit8 law and it is reproduced rather than worked around.
//
// ONE ROUNDING per channel. Not two, not a multiply-then-round-twice.
`default_nettype none

module zhao_raster_fog (
    input var logic clk,
    input var logic rst_n,

    // ---- per-draw fog state --------------------------------------------------
    // cfg_en_i low passes the fragment through UNTOUCHED, which is both the
    // fog-off case and §8's frozen EXEMPT LIST: the sky family, the under-plane,
    // the sun quad and cloud sheet, additive emissive (beams, flares, glints,
    // souls) and the HUD planes. Exemption is a per-class property decided
    // upstream, so this block does not try to infer it.
    input var logic       cfg_en_i,
    input var logic [7:0] cfg_fog_r_i,
    input var logic [7:0] cfg_fog_g_i,
    input var logic [7:0] cfg_fog_b_i,

    // ---- one fragment's final source colour ---------------------------------
    input  var logic        v_valid_i,
    output var logic        v_ready_o,
    input  var logic [7:0]  r_i,
    input  var logic [7:0]  g_i,
    input  var logic [7:0]  b_i,
    // The interpolated fog factor, Q16.16 as ATTRSTEP delivers it. D-5 made
    // this a separate interpolant; before D-5 there was no such port, because
    // the colour arrived pre-fogged.
    input  var logic signed [31:0] fogf_i,
    input  var logic [15:0] tag_i,

    output var logic        r_valid_o,
    input  var logic        r_ready_i,
    output var logic [7:0]  r_o,
    output var logic [7:0]  g_o,
    output var logic [7:0]  b_o,
    output var logic [15:0] tag_o,

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0] fragments_o,
    output var logic [31:0] fogged_fragments_o,  // cfg_en_i high AND amt8 != 0
    output var logic [31:0] clear_fragments_o    // amt8 == 0: the mix is identity
);

  // ---- f8: fx16 -> unit8, §2 round-half-up, saturating ----------------------
  // The rounding add happens at full width and the clamp comes after, so a
  // factor outside [0, 0x10000] cannot wrap into a plausible-looking small
  // number.
  logic signed [33:0] f_rnd_c;
  logic signed [33:0] f_sh_c;
  logic        [7:0]  f8_c;
  assign f_rnd_c = 34'sd128 + 34'(fogf_i);
  assign f_sh_c  = f_rnd_c >>> 8;
  always_comb begin
    if (f_sh_c <= 34'sd0)        f8_c = 8'd0;
    else if (f_sh_c >= 34'sd255) f8_c = 8'd255;
    else                         f8_c = f_sh_c[7:0];
  end

  // amt8 is the weight TOWARD the fog colour: 0 = clear, 255 = fogged.
  logic [7:0] amt8_c;
  assign amt8_c = cfg_en_i ? (8'd255 - f8_c) : 8'd0;

  // ---- the mix, one rounding per channel -----------------------------------
  // (fog_c - c) is signed and spans [-255, 255]; times amt8 spans
  // [-65025, 65025]; plus 128 and arithmetic-shifted by 8. An 18-bit signed
  // accumulator holds every intermediate exactly, and the final add-and-clamp
  // is the same saturating rail zhao_raster_blend uses.
  function automatic logic [7:0] fog_ch(input logic [7:0] c, input logic [7:0] fogc,
                                        input logic [7:0] amt);
    logic signed [17:0] diff;
    logic signed [17:0] prod;
    logic signed [17:0] delta;
    logic signed [17:0] sum;
    begin
      diff  = signed'(18'(fogc)) - signed'(18'(c));
      prod  = diff * signed'(18'(amt));
      delta = (prod + 18'sd128) >>> 8;
      sum   = signed'(18'(c)) + delta;
      if (sum < 18'sd0)        fog_ch = 8'd0;
      else if (sum > 18'sd255) fog_ch = 8'd255;
      else                     fog_ch = sum[7:0];
    end
  endfunction

  // ---- registered output boundary ------------------------------------------
  // QUARTUS_GOTCHAS 14, and §16.2's lesson from the DONE queue: the output is a
  // real register with a skid, not a combinational read out of an array. The
  // mix is cheap; the boundary is where the timing goes.
  assign v_ready_o = !r_valid_o || r_ready_i;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      r_valid_o          <= 1'b0;
      r_o                <= 8'd0;
      g_o                <= 8'd0;
      b_o                <= 8'd0;
      tag_o              <= 16'd0;
      fragments_o        <= 32'd0;
      fogged_fragments_o <= 32'd0;
      clear_fragments_o  <= 32'd0;
    end else begin
      if (r_valid_o && r_ready_i) r_valid_o <= 1'b0;
      if (v_valid_i && v_ready_o) begin
        r_valid_o   <= 1'b1;
        r_o         <= fog_ch(r_i, cfg_fog_r_i, amt8_c);
        g_o         <= fog_ch(g_i, cfg_fog_g_i, amt8_c);
        b_o         <= fog_ch(b_i, cfg_fog_b_i, amt8_c);
        tag_o       <= tag_i;
        fragments_o <= fragments_o + 32'd1;
        if (amt8_c != 8'd0) fogged_fragments_o <= fogged_fragments_o + 32'd1;
        else                clear_fragments_o  <= clear_fragments_o  + 32'd1;
      end
    end
  end

`ifndef SYNTHESIS
  logic armed_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) armed_q <= 1'b0;
    else        armed_q <= 1'b1;
  end
  always_ff @(posedge clk) begin
    if (armed_q && v_valid_i && v_ready_o) begin
      // A clear fragment must be bit-identical to its input. The mix IS the
      // identity at amt8 == 0, and a block that "almost" passed through would
      // shift every unfogged pixel by a rounding step -- invisible in one frame
      // and a golden-CRC change in every frame.
      if (amt8_c == 8'd0) begin
        a_fog_clear_identity_r : assert (fog_ch(r_i, cfg_fog_r_i, 8'd0) == r_i);
        a_fog_clear_identity_g : assert (fog_ch(g_i, cfg_fog_g_i, 8'd0) == g_i);
        a_fog_clear_identity_b : assert (fog_ch(b_i, cfg_fog_b_i, 8'd0) == b_i);
      end
      // Disabled means untouched, which is how the frozen exempt list is
      // honoured. An exempt class that got fogged anyway is what this catches.
      if (!cfg_en_i) a_fog_disabled_passes_through : assert (amt8_c == 8'd0);
    end
  end
`endif

endmodule : zhao_raster_fog

`default_nettype wire
