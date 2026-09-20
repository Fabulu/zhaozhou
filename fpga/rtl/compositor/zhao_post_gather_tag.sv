// zhao_post_gather_tag.sv -- the TAG-TO-GATHER LAW in RTL. Owner ruling R195.
//
// ===========================================================================
// WHAT THIS BLOCK IS, AND WHY IT IS A SEPARATE FILE
// ===========================================================================
// `zhao_raster_resolve` emits ONE 8-bit effect tag per resolved fragment:
//
//     tag = (channel << 6) | strength      spec/stars_and_flares.md 1, FROZEN
//     GLOW = 0b01, strength = the source texel's CLUT intensity, 0..63
//
// `zhao_post_gather` consumes THREE eight-bit glow channels, a signed 8.8
// displacement pair and an ink bit. The step between those two is an ART LAW,
// and `zhao_console_core.sv` entry I17 refused to compose the gather three
// times because that law did not exist. It exists now.
//
//   Fabian, 2026-09-20, looking at reports/post-gather-law/gather_law_contact.png:
//     "Everything but before looks basically the same. Pick cheapest."
//
//   Ruling R195 (reports/OWNER-RULINGS-20260919-EVENING.md): RATIFIED AS
//   PROPOSED. kGlowKnee 24, kGlowSlope 0x1C, tint 255/236/224, kGlowMaster
//   255. NOT ONE COEFFICIENT MOVES.
//
// It is its OWN FILE rather than a few lines inside the composer because
// CLAUDE.md's art chapter, rule 6, is explicit: *"never remove the owner's
// control in the name of fidelity. Every shape, colour and timing value
// belongs in a named, editable constant."* A law buried in a composer is a
// law nobody can turn a knob on. Every coefficient below is a PARAMETER with
// the ratified value as its default, and the render that judged them is
// reproduced by `tools/post/gather_law_render.cpp`.
//
// ===========================================================================
// THE THREE DECISIONS THAT ARE NOT NUMBERS
// ===========================================================================
// 1. THE GLOW'S COLOUR IS THE FRAGMENT'S OWN COLOUR. A star's halo is the
//    colour of the star. The tag carries an INTENSITY and nothing else; the
//    palette has already colourised the fragment by the time resolve emits
//    it, so the bloom BORROWS `fb_rgb565_o` on the same beat rather than
//    inventing a second palette.
//
// 2. STRENGTH IS A KNEE, NOT A SCALE. Every CLUT texel carries some
//    intensity. Without a knee the whole image hazes, and the contact sheet
//    measured exactly that: the `knee 16` row has 907 of 5,760 cells
//    contributing against 74 at knee 24 -- twelve times the work for a hazier
//    frame. R195 spells this out because "pick cheapest" reads like "pick the
//    smallest number in every column", and on THIS axis the smallest number
//    is both uglier and more expensive.
//
// 3. DISPLACEMENT AND INK ARE NOT INVENTED. Channels 0b10 and 0b11 are
//    UNALLOCATED in the frozen spec. A fragment carrying one contributes
//    NOTHING and is COUNTED (`reserved_channel_o`), so on the day refraction
//    is specified that counter says whether anything was already drawing it.
//    `disp_x_o`, `disp_y_o` and `ink_o` are therefore ZERO under this law.
//
//    THAT IS A STATEMENT, NOT AN OMISSION, and it is the sentence a packet
//    read past once already: POSTMEAS concluded from a correct measurement
//    ("the spec defines one channel") that R37 could not unblock this block,
//    filed it as a new owner decision, then retracted it -- it had read the
//    contract through a `grep -B4 -A12` window that skipped the three
//    numbered decisions (ruling R186). The zeros are ruled, not missing.
//
// ===========================================================================
// BIT-EXACT AGAINST zref::post::gather, INCLUDING THE ROUNDING ORDER
// ===========================================================================
// `reference/include/zref/zref_post.hpp` is the law's model and this block
// differences against it in `tests/compositor/post_gather_tag_directed.cpp`
// over ALL 8,388,608 (tag, rgb565) pairs -- exhaustive, because the input
// space is small enough that a sample would be a worse instrument than a
// sweep.
//
// THE ROUNDING ORDER IS LOAD-BEARING AND IS NOT AN ACCIDENT OF TRANSCRIPTION.
// The model computes `unit_mul(unit_mul(channel, gain), tint)` -- TWO
// roundings, in that order. Folding `gain` and `tint` together first
// (`unit_mul(channel, unit_mul(gain, tint))`) is algebraically the same and
// ARITHMETICALLY DIFFERENT, because round-half-up does not commute across a
// product. It would also look correct, pass a spot check and drift by one LSB
// on a fraction of inputs -- the shape of defect the exhaustive sweep exists
// to catch.
//
// ===========================================================================
// ONE REGISTER STAGE, AND EVERYTHING IN THE BEAT MOVES IN IT TOGETHER
// ===========================================================================
// The law is two chained 8x8 multiplies per channel, so the output is
// REGISTERED. That means the fragment's POSITION and the tile pulses have to
// be delayed by exactly as much, and they are -- in the SAME `always_ff`,
// with no enable on any of them.
//
// This is the one thing `CLAUDE.md`'s metadata chapter says to check, read in
// the positive direction. There, a bank registered its row and its generation
// under one ungated assignment, so a stall produced A's data with B's
// metadata and every counter balanced. Here the requirement is the OPPOSITE
// and the same mechanism supplies it: colour, tag, position and tile phase
// are ONE BEAT, they enter this block together, and one unconditional
// register keeps them together. There is no enable to skew them and no join
// to mis-pair. If a `ready` is ever added to this path, THIS is the register
// that must gain it -- all of it or none of it.
// ===========================================================================
`default_nettype none

module zhao_post_gather_tag #(
    // ---- THE RATIFIED LAW, R195. Every one is an ART value: change it and
    // ---- re-render `tools/post/gather_law_render.cpp`, do not reason about it.
    //
    // Below this strength a texel is LIT, not a LIGHT: it contributes no glow
    // at all. 0..63, the tag's own units. The picture is most sensitive to
    // this one, and lowering it is the expensive direction (decision 2 above).
    parameter int unsigned GLOW_KNEE   = 24,
    // How fast a texel becomes a light above the knee, Q4.4 (0x10 = 1.0 per
    // unit of strength). 63 - 24 = 39 units of headroom, so 0x1C saturates
    // near strength 55.
    parameter int unsigned GLOW_SLOPE  = 32'h1C,
    // A per-channel weight on the BORROWED colour, unit8 (255 = keep). The
    // warm bias a halo has: leave R alone, pull G and B back a little.
    parameter int unsigned GLOW_TINT_R = 255,
    parameter int unsigned GLOW_TINT_G = 236,
    parameter int unsigned GLOW_TINT_B = 224,
    // The whole law's master gain, unit8, applied last -- the one most likely
    // to want a slider. `SetPost.bloom_gain` scales the RESULT again, per
    // frame, at composite stage 4, so this is the STILL value and that is the
    // live one.
    parameter int unsigned GLOW_MASTER = 255
) (
    input  var logic         clk,
    input  var logic         rst_n,

    // ---- one ACCEPTED resolved fragment ------------------------------------
    // `f_valid_i` is the ACCEPTED beat (`fb_valid_o && fb_ready_i`), never the
    // raw valid. A stalled beat held across three clocks is ONE fragment, and
    // accumulating it three times is the defect this port's name guards
    // against: POST.GATHER has no `ready` by law, so the gate has to be here.
    input  var logic         f_valid_i,
    input  var logic [7:0]   f_tag_i,       // (channel << 6) | strength
    input  var logic [15:0]  f_rgb565_i,    // the fragment's OWN colour
    input  var logic [3:0]   f_x_i,         // pixel within the 16x16 tile
    input  var logic [3:0]   f_y_i,
    input  var logic         tile_start_i,
    input  var logic         tile_flush_i,

    // ---- POST.GATHER's per-fragment input, one clock later -----------------
    output var logic         g_valid_o,
    output var logic [7:0]   g_glow_r_o,
    output var logic [7:0]   g_glow_g_o,
    output var logic [7:0]   g_glow_b_o,
    // ZERO under this law, and ruled so (decision 3). They are PORTS rather
    // than constants at the instantiation site so that the day a refraction
    // channel is allocated, the producer is already here.
    output var logic signed [15:0] g_disp_x_o,
    output var logic signed [15:0] g_disp_y_o,
    output var logic         g_ink_o,
    output var logic [3:0]   g_x_o,
    output var logic [3:0]   g_y_o,
    output var logic         g_tile_start_o,
    output var logic         g_tile_flush_o,

    // ---- evidence ----------------------------------------------------------
    // Three counters over the same stream, partitioning it: a fragment is
    // either not tagged, tagged GLOW below the knee, tagged GLOW above it, or
    // carrying a reserved channel. Each is reachable with LEGAL stimulus --
    // there is no unreachable guard here and therefore no mutant is owed.
    output var logic [31:0]  frag_untagged_o,     // channel 0b00
    output var logic [31:0]  frag_below_knee_o,   // GLOW, strength <= knee
    output var logic [31:0]  frag_lit_o,          // GLOW, contributes
    output var logic [31:0]  reserved_channel_o   // 0b10 / 0b11, R195 decision 3
);

  localparam logic [1:0] CH_NONE      = 2'b00;
  localparam logic [1:0] CH_GLOW      = 2'b01;   // stars_and_flares.md 1, frozen
  // 2'b10 and 2'b11 are UNALLOCATED. They are not named as constants because
  // naming them would suggest a meaning the spec does not give them; they are
  // "not none and not glow", which is what the counter below says.

  // ---- unit8 multiply, qformats 2: round-half-up, saturating at 255 --------
  // `zref::post::gather::unit_mul`, and the saturate is real rather than
  // decorative: 255 * 255 + 128 = 65,153, whose top bits are 254, so the
  // clamp only fires on a product this law cannot actually form. It is kept
  // because the model has it and because a parameter change can reach it.
  function automatic logic [7:0] unit_mul(input logic [7:0] a, input logic [7:0] b);
    /* verilator lint_off UNUSEDSIGNAL */
    logic [16:0] p;   // p[7:0] is the discarded fraction: the >> 8 IS the law
    /* verilator lint_on UNUSEDSIGNAL */
    begin
      p = ({9'd0, a} * {9'd0, b}) + 17'd128;
      unit_mul = (p[16:8] > 9'd255) ? 8'd255 : p[15:8];
    end
  endfunction

  // ---- RGB565 -> 888, replicate the high bits -----------------------------
  // The same expansion POST.COMPOSITE runs (`exp5`/`exp6`, its line 527) and
  // the same one `zref::sky::rgb565::to_rgb888` defines. Not a new colour law.
  function automatic logic [7:0] exp5(input logic [4:0] v);
    exp5 = {v, v[4:2]};
  endfunction
  function automatic logic [7:0] exp6(input logic [5:0] v);
    exp6 = {v, v[5:4]};
  endfunction

  // ---- the knee ramp: strength (0..63) -> unit8 gain -----------------------
  // A 64-entry function of the parameters, so Quartus folds it to a small LUT
  // rather than a multiplier. It is a FUNCTION of the knobs, not a table of
  // baked numbers -- moving GLOW_KNEE moves the ramp, which is what rule 6
  // asks for.
  function automatic logic [7:0] glow_gain(input logic [5:0] strength);
    logic [15:0] g;
    begin
      if ({10'd0, strength} <= 16'(GLOW_KNEE)) begin
        glow_gain = 8'd0;
      end else begin
        g = ((16'(strength) - 16'(GLOW_KNEE)) * 16'(GLOW_SLOPE)) >> 4;
        glow_gain = (g > 16'd255) ? 8'd255 : g[7:0];
      end
    end
  endfunction

  // ---- the law, combinationally -------------------------------------------
  logic [1:0] ch_c;
  logic [5:0] st_c;
  logic [7:0] gain_c;
  logic [7:0] r_c, g_c, b_c;
  logic [7:0] lit_r_c, lit_g_c, lit_b_c;
  logic       is_glow_c, is_res_c, is_none_c, above_knee_c;

  assign ch_c        = f_tag_i[7:6];
  assign st_c        = f_tag_i[5:0];
  assign is_none_c   = (ch_c == CH_NONE);
  assign is_glow_c   = (ch_c == CH_GLOW);
  assign is_res_c    = !is_none_c && !is_glow_c;
  assign gain_c      = unit_mul(glow_gain(st_c), 8'(GLOW_MASTER));
  assign above_knee_c = is_glow_c && (gain_c != 8'd0);

  assign r_c = exp5(f_rgb565_i[15:11]);
  assign g_c = exp6(f_rgb565_i[10:5]);
  assign b_c = exp5(f_rgb565_i[4:0]);

  // TWO roundings, in the model's order. See the header: folding them is a
  // one-LSB drift that looks right.
  assign lit_r_c = unit_mul(unit_mul(r_c, gain_c), 8'(GLOW_TINT_R));
  assign lit_g_c = unit_mul(unit_mul(g_c, gain_c), 8'(GLOW_TINT_G));
  assign lit_b_c = unit_mul(unit_mul(b_c, gain_c), 8'(GLOW_TINT_B));

  // ---- one register stage, and the whole beat moves in it -----------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      g_valid_o      <= 1'b0;
      g_glow_r_o     <= 8'd0;
      g_glow_g_o     <= 8'd0;
      g_glow_b_o     <= 8'd0;
      g_disp_x_o     <= 16'sd0;
      g_disp_y_o     <= 16'sd0;
      g_ink_o        <= 1'b0;
      g_x_o          <= 4'd0;
      g_y_o          <= 4'd0;
      g_tile_start_o <= 1'b0;
      g_tile_flush_o <= 1'b0;
      frag_untagged_o    <= 32'd0;
      frag_below_knee_o  <= 32'd0;
      frag_lit_o         <= 32'd0;
      reserved_channel_o <= 32'd0;
    end else begin
      // The tile pulses travel with the fragments and are delayed by the same
      // one clock, so a tile boundary still falls between the same two
      // fragments it fell between at resolve. Delaying the data and not the
      // pulses would put the last fragment of a tile into the next tile's
      // bank -- a one-cycle skew that shows up as a halo displaced by four
      // pixels at a tile edge, sixteen times per row.
      g_tile_start_o <= tile_start_i;
      g_tile_flush_o <= tile_flush_i;

      g_valid_o  <= f_valid_i;
      g_x_o      <= f_x_i;
      g_y_o      <= f_y_i;
      // Decision 3: ZERO, and ruled so. Driven every clock rather than tied at
      // the instantiation site so the seam has a real producer the day a
      // channel is allocated for it.
      g_disp_x_o <= 16'sd0;
      g_disp_y_o <= 16'sd0;
      g_ink_o    <= 1'b0;

      g_glow_r_o <= (is_glow_c && f_valid_i) ? lit_r_c : 8'd0;
      g_glow_g_o <= (is_glow_c && f_valid_i) ? lit_g_c : 8'd0;
      g_glow_b_o <= (is_glow_c && f_valid_i) ? lit_b_c : 8'd0;

      if (f_valid_i) begin
        if (is_none_c)          frag_untagged_o    <= frag_untagged_o    + 32'd1;
        else if (is_res_c)      reserved_channel_o <= reserved_channel_o + 32'd1;
        else if (above_knee_c)  frag_lit_o         <= frag_lit_o         + 32'd1;
        else                    frag_below_knee_o  <= frag_below_knee_o  + 32'd1;
      end
    end
  end

  // ---- elaboration guards -------------------------------------------------
  // INSIDE an `initial`, deliberately. A bare module-scope `if (...) $fatal`
  // lints clean under Verilator with ZERO diagnostics and fails `quartus_map`
  // with "syntax error near text: `if`; expecting `endmodule`" -- CLAUDE.md's
  // build chapter, and `tools/quartus/check_quartus17_syntax.py` gates it.
  // And `--lint-only` does not RUN an initial block, so a clean lint says
  // nothing whatever about these.
  initial begin
    if (GLOW_KNEE > 63)
      $fatal(1, "zhao_post_gather_tag: GLOW_KNEE %0d is outside the tag's 0..63 strength range; the ramp would never leave zero.", GLOW_KNEE);
    if (GLOW_SLOPE == 0)
      $fatal(1, "zhao_post_gather_tag: GLOW_SLOPE 0 makes every glow fragment contribute nothing. If that is what you want, set GLOW_MASTER to 0 -- it says so.");
    if (GLOW_TINT_R > 255 || GLOW_TINT_G > 255 || GLOW_TINT_B > 255 || GLOW_MASTER > 255)
      $fatal(1, "zhao_post_gather_tag: a unit8 knob is above 255; unit_mul would saturate on every fragment and the tint would stop being a tint.");
  end

endmodule : zhao_post_gather_tag

`default_nettype wire
