// zhao_texture_mosaic.sv — TEXTURE.MOSAIC: the terrain Mosaic selector, i.e.
// the two frozen laws of spec/terrain_rules.md 6.2 in hardware (phase 6,
// ZH-030).
//
// Law, in citation order:
//   design/contracts/TEXTURE.MOSAIC.md — the block contract.
//   design/blocks.yml — `inputs: [mosaic_candidates]`, `outputs:
//       [mosaic_pick]`, `upstream: [TERRAIN.PROJECT]`, `downstream:
//       [TEXTURE.TMU]`, `backpressure: ready_valid`, `latency: fixed:2`,
//       "1 candidate pick per clock", counter `texture_samples`,
//       `source_ids: true`, maturity REFERENCE_COMPLETE, and the note
//       "Determinism of the pattern is a capture-exact requirement."
//   spec/terrain_rules.md 6.2 (FROZEN 2026-08-16, capture-exact) — both laws,
//       quoted at THE TWO LAWS below.
//   spec/terrain_rules.md 5 and 6.6 — walls sample the rim strata tile
//       (id 240) and the underside samples id 241, both "mirrored repeat" and
//       both with NO per-texel pick. That is why `req_mosaic_i` exists.
//   reference/include/zref/zref_terrain.hpp `zref::terrain::mirror_texel` and
//       `zref::terrain::mosaic_pick` — THE oracle. This block is
//       REFERENCE_COMPLETE, so the oracle is not a guide, it is the answer:
//       every bit this block emits must equal what those two functions return.
//   reference/src/zrender/rast.cpp raster_tri, the TextureSpan branch — the
//       one site in the machine that calls both, and therefore the site that
//       fixes how they COMPOSE (see THE >>10 APPEARS TWICE).
//   reference/src/zrender/terrain.cpp — the three TextureSpan construction
//       sites: `span.mosaic = true` on tops (line 613), `false` on the rim
//       walls (561, tile 240) and the underside (661, tile 241).
//   ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md 15/26 — one primary TMU plus one
//       restricted aux, no second unrestricted sampler. This block is neither:
//       it emits no texel and touches no memory. It CHOOSES an id.
//
// ---------------------------------------------------------------------------
// THE TWO LAWS, QUOTED
// ---------------------------------------------------------------------------
// spec/terrain_rules.md 6.2, verbatim:
//
//   "Mirrored-repeat texel fold. With `u` in Q16.16 tile units ... the sampled
//    texel index is `m = floor(u x 64)` (arithmetic shift `u_raw >> 10`),
//    `per = m mod 128` (floored), `texel = per < 64 ? per : 127 - per`."
//
//   "Stable world-space pick. With `tx, ty` the UNFOLDED world texel indices
//    (`floor(u x 64)`, `floor(v x 64)` - the pattern lives in world space,
//    immune to cell borders and LOD): `h = (u32(tx) . 73856093) XOR
//    (u32(ty) . 19349663)`, `p = h mod 255`, `pick = (p < weight) ? matA :
//    matB`. Weight 0 selects matB everywhere, 255 selects matA everywhere, 128
//    dithers ~50/255 ... The constants are frozen: changing one changes every
//    capture's pixels."
//
// ---------------------------------------------------------------------------
// THE >>10 APPEARS TWICE, AND THAT IS THE WHOLE SHAPE OF THIS BLOCK
// ---------------------------------------------------------------------------
// `mirror_texel` opens with `m = u_raw >> 10` and `raster_tri` passes
// `u >> 10` as the pick's world index. So both laws consume the SAME quantity
// — the unfolded world texel index m — and the fold is nothing but m's low
// seven bits with a conditional complement:
//
//     per   = m mod 128 (floored) = m[6:0]   (two's complement IS floored mod)
//     texel = per[6] ? (127 - per) : per     = per[5:0] ^ {6{per[6]}}
//
// because for per in [64,127], 127 - per is 127 XOR per, and its bit 6 is 0.
// Six XOR gates. And since m = u_raw >>> 10, m[6:0] is literally u_raw[16:10]:
// the fold contains no shifter and no subtractor at all.
//
// The consequence is that this block owns both laws or neither. The contract's
// Scalar reference function section names BOTH functions, so it owns both: it
// emits the winning tile id AND the folded texel pair, from one shift-free
// decode of the same word.
//
// ---------------------------------------------------------------------------
// LAWS FOUND (not invented)
// ---------------------------------------------------------------------------
// F1. THE FRACTION BITS ARE DISCARDED, NOT ROUNDED. `mirror_texel` is an
//     arithmetic shift and 6.2 says "floor", so u_raw[9:0] — the sub-texel
//     fraction — never reaches the output. This LOOKS like a lost bit of
//     precision and is not: rounding here would move the mirror turn by half a
//     texel and every committed capture's terrain would shift. The bits are
//     sunk explicitly below rather than silently, so a reader sees the choice.
// F2. THE PRODUCTS WRAP BEFORE THE XOR. `mosaic_pick` multiplies in
//     `uint32_t`, so both products are mod 2^32 and only then combined. The
//     oracle's own random test records what happens if you widen them: "the
//     wrap is the law - the oracle originally skipped it and disagreed on 25
//     percent of samples" (tests/texture/texture_mosaic_random.cpp). Here the
//     multiplies are evaluated in a 32-bit context and truncate by
//     construction; nothing is ever computed at 64 bits and narrowed.
//     ENFORCED-BY: tests/texture/texture_mosaic_random.cpp:rtl_lane_limit
//     (40k packets uniform over the WHOLE int32 UV domain plus the
//     INT32_MIN/MAX rail grid, differentiated against the wrapping oracle;
//     a widened product disagrees on roughly a quarter of them)
// F3. THE SIGN SURVIVES THE SHIFT. `u_raw >> 10` on `int32_t` is arithmetic,
//     and `static_cast<uint32_t>(tx)` then SIGN-EXTENDS the negative index
//     into the multiplier. Terrain west or north of the world origin has
//     negative world texel indices and its pattern is decided by those
//     sign-extended words. `m_u` is therefore `logic signed [31:0]` and enters
//     the multiplier through `$unsigned`, which reinterprets and does not
//     widen.
// F4. p IS COMPARED, NOT THE HASH. `p = h mod 255` lies in [0,254], which is
//     why weight 255 means "always A" and weight 0 means "always B" — an
//     asymmetry that is deliberate and load-bearing (a fully-A cell must be
//     expressible, and 255 is the only code that can say it).
// F5. `req_mosaic_i` IS A REAL INPUT, NOT A CONVENIENCE. terrain.cpp builds
//     three TextureSpans: tops pick per texel, the rim walls are always tile
//     240 and the underside is always tile 241. All three still FOLD. So the
//     enable gates the pick and never the fold, and this block serves every
//     terrain fragment the machine draws rather than only the tops.
//     ENFORCED-BY: tests/texture/texture_mosaic_directed.cpp:test_rtl_fold_only_spans
//
// ---------------------------------------------------------------------------
// LAWS CHOSEN (no spec states these; recorded as decisions, with the
// alternative that was rejected)
// ---------------------------------------------------------------------------
// C1. NO TRANSITION-GROUP RESTRICTION PORT. design/contracts/TEXTURE.MOSAIC.md
//     says "Authored transition groups (MAPG heir, terrain_rules 6.3) MAY
//     restrict the pick to a group's members", and 6.3 itself says Mosaic
//     "may restrict". "May" is a permission, not a law, and the ratified
//     oracle settles it: `mosaic_pick` has five parameters and none of them is
//     a group. Since this block is REFERENCE_COMPLETE, a group port would be
//     hardware no differential test could ever exercise.
//     REJECTED ALTERNATIVE: carrying the 256 B group table (16 groups x
//     {count u8, detail u8, members u8[14]}) and searching it per texel. It
//     costs an M10K page and a 14-way compare in the per-texel path, it
//     changes `p`'s meaning from "a residue" to "an index into a variable-
//     length member list", and no committed capture could tell a correct
//     implementation from a broken one. When 6.3 is ratified into an oracle it
//     arrives as a new input port and a second `pick_tile_o` term; nothing
//     here has to move.
// C2. THE FOLDED TEXEL PAIR IS AN OUTPUT, EVEN THOUGH TEXTURE.TMU COULD REFOLD
//     IT. zhao_texture_tmu's WRAP_MIRROR is 6.2's fold generalised to any
//     power-of-two size, so a TMU handed the raw u/v reproduces `pick_tx_o`
//     exactly. Emitting it anyway makes the whole of this block's stated
//     reference function observable at its own boundary, which is what lets
//     the directed test pin `mirror_texel` here instead of inferring it three
//     blocks downstream through a cache and a palette.
//     REJECTED ALTERNATIVE: emitting only the tile id. Twelve fewer wires, and
//     `mirror_texel` — half the contract's Scalar reference function — would
//     have no port at which it could be checked at all.
//     ENFORCED-BY: tests/texture/texture_mosaic_directed.cpp:test_rtl_fold_exhaustive
// C3. A RIGID TWO-STAGE PIPELINE, WHICH IS WHAT MAKES `latency: fixed:2` TRUE.
//     Stage A is the pair of 32-bit constant multiplies and the XOR; stage B
//     is the mod-255 fold, the compare and the select. Every stage advances
//     together or none does, so a stalled TEXTURE.TMU freezes the block and
//     nothing is dropped, reordered or squeezed. The split is where the logic
//     depth is: a constant multiply on one side, a four-byte adder tree and an
//     8-bit compare on the other.
//     REJECTED ALTERNATIVE: one combinational stage with a skid buffer. It
//     would make the ledger's `fixed:2` a lie (latency would be 1, or 2 only
//     when the skid was occupied), and it puts the multiply, the fold and the
//     compare in one path — the deepest path in a block that is supposed to
//     keep up with one fragment per clock.
// C4. `pick_tile_o` IS THE TILE ID, NOT AN ADDRESS. Turning an id into a base
//     address needs the tileset base and the 5,461 B/tile mip stride
//     (terrain_rules 6.1), which are TEXTURE.CACHE's and TEXTURE.TMU's
//     business and are not in this block's ledger inputs.
//     REJECTED ALTERNATIVE: an address adder here, which would put a second
//     copy of the tile-stride constant in the tree and violate charter 29-6.
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK DOES NOT DO
// ---------------------------------------------------------------------------
// It reads no memory, holds no table and produces no texel. It has no cache
// port and no VRAM port. The mip level, the palette, the filter and the format
// are TEXTURE.TMU's; the modulation (shade x layer-H tint x sheet strength) is
// composed once per primitive upstream and never enters here.
//
// Depends on zhao_texture_mod255. Conservative SystemVerilog subset only
// (charter 2). Lint: clean under `-Wall` (lint_texture_mosaic).

module zhao_texture_mosaic (
    input logic clk,
    input logic rst_n,

    // -----------------------------------------------------------------------
    // mosaic_candidates in — the per-cell layer-E triple that TERRAIN.PROJECT
    // forwards unselected (`out_mat_a_o`/`out_mat_b_o`/`out_weight_o`,
    // "forwarded, never selected"), joined to the fragment's interpolated UV.
    // u and v are Q16.16 TILE units, one tile period per cell on tops and per
    // STRATA_M on walls/underside (terrain_rules 6.2/6.6).
    // -----------------------------------------------------------------------
    input  logic               req_valid_i,
    output logic               req_ready_o,
    input  logic signed [31:0] req_u_i,
    input  logic signed [31:0] req_v_i,
    input  logic        [ 7:0] req_mat_a_i,
    input  logic        [ 7:0] req_mat_b_i,
    input  logic        [ 7:0] req_weight_i,
    input  logic               req_mosaic_i,  // TextureSpan::mosaic (F5)
    input  logic        [15:0] req_src_id_i,

    // -----------------------------------------------------------------------
    // mosaic_pick out — exactly one primary sample's worth of decision.
    // -----------------------------------------------------------------------
    output logic        pick_valid_o,
    input  logic        pick_ready_i,
    output logic [ 7:0] pick_tile_o,  // zref::terrain::mosaic_pick
    output logic [ 5:0] pick_tx_o,    // zref::terrain::mirror_texel(u)
    output logic [ 5:0] pick_ty_o,    // zref::terrain::mirror_texel(v)
    output logic [15:0] pick_src_id_o,

    // status and counters
    output logic        idle_o,
    output logic [31:0] texture_samples_o
);

  // The FROZEN constants of 6.2. Changing one changes every capture's pixels.
  localparam logic [31:0] MOSAIC_CX = 32'd73856093;
  localparam logic [31:0] MOSAIC_CY = 32'd19349663;

  localparam logic [31:0] CNT_MAX = 32'hFFFF_FFFF;

  // ---------------------------------------------------------------------------
  // the combinational front: the fold and the hash, both from one word
  // ---------------------------------------------------------------------------
  // m = floor(u * 64) = u_raw >>> 10, ARITHMETIC (F3). The sign is carried all
  // the way into the multiplier because `static_cast<uint32_t>(tx)` in the
  // oracle sign-extends.
  logic signed [31:0] m_u;
  logic signed [31:0] m_v;
  assign m_u = req_u_i >>> 10;
  assign m_v = req_v_i >>> 10;

  // per = m mod 128, floored — which for two's complement IS the low 7 bits,
  // and m's low 7 bits are u_raw[16:10]. No shifter, no modulo, no correction
  // for negatives.
  logic [6:0] per_u;
  logic [6:0] per_v;
  assign per_u = req_u_i[16:10];
  assign per_v = req_v_i[16:10];

  // texel = per < 64 ? per : 127 - per. For per in [64,127] the subtraction is
  // a complement, so both arms are one XOR with the mirror bit.
  logic [5:0] tx_c;
  logic [5:0] ty_c;
  assign tx_c = per_u[5:0] ^ {6{per_u[6]}};
  assign ty_c = per_v[5:0] ^ {6{per_v[6]}};

  // h = (u32(m_u) * CX) XOR (u32(m_v) * CY). Both products are evaluated in a
  // 32-bit context and therefore WRAP (F2) — the law, not an overflow.
  logic [31:0] hx_c;
  logic [31:0] hy_c;
  logic [31:0] h_c;
  always_comb begin
    hx_c = $unsigned(m_u) * MOSAIC_CX;
    hy_c = $unsigned(m_v) * MOSAIC_CY;
    h_c  = hx_c ^ hy_c;
  end

  // F1: the sub-texel fraction is DISCARDED by law. Sunk explicitly, in the
  // style of zhao_texture_tmu's own `unused_ok`, so the truncation is visible
  // in the source rather than inferred from a lint waiver. u_raw[9:0] is the
  // position WITHIN a texel; the fold floors and the hash consumes m, so
  // neither law can ever read it. Rounding instead of flooring would move the
  // mirror turn by half a texel and shift every committed capture.
  logic unused_ok;
  always_comb begin
    unused_ok = |req_u_i[9:0] | |req_v_i[9:0];
    unused_ok = unused_ok & 1'b0;
  end

  // ---------------------------------------------------------------------------
  // stage A — the hash and the folded texels, registered (C3)
  // ---------------------------------------------------------------------------
  logic        a_valid_r;
  logic [31:0] a_h_r;
  logic [ 7:0] a_mat_a_r;
  logic [ 7:0] a_mat_b_r;
  logic [ 7:0] a_weight_r;
  logic        a_mosaic_r;
  logic [ 5:0] a_tx_r;
  logic [ 5:0] a_ty_r;
  logic [15:0] a_src_r;

  // ---------------------------------------------------------------------------
  // stage B — the residue, the compare and the select, registered
  // ---------------------------------------------------------------------------
  logic        b_valid_r;
  logic [ 7:0] b_tile_r;
  logic [ 5:0] b_tx_r;
  logic [ 5:0] b_ty_r;
  logic [15:0] b_src_r;

  logic [7:0] p_c;
  zhao_texture_mod255 u_mod255 (
      .h_i(a_h_r),
      .p_o(p_c)
  );

  // pick = (p < weight) ? matA : matB, and the fold-only spans (F5) take matA
  // unconditionally. p <= 254, so weight 255 always picks A and weight 0 never
  // does (F4).
  logic       pick_a_c;
  logic [7:0] tile_c;
  assign pick_a_c = (p_c < a_weight_r);
  assign tile_c   = (!a_mosaic_r || pick_a_c) ? a_mat_a_r : a_mat_b_r;

  // ---------------------------------------------------------------------------
  // the rigid-pipeline advance (C3)
  // ---------------------------------------------------------------------------
  // `advance` is a function of registers and of `pick_ready_i` only, and
  // `req_ready_o` never depends on `req_valid_i` — the house handshake
  // hygiene rule (zhao_texture_tmu, "a function of registers only").
  logic advance;
  assign advance = !b_valid_r || pick_ready_i;

  assign req_ready_o   = advance;
  assign pick_valid_o  = b_valid_r;
  assign pick_tile_o   = b_tile_r;
  assign pick_tx_o     = b_tx_r;
  assign pick_ty_o     = b_ty_r;
  assign pick_src_id_o = b_src_r;
  assign idle_o        = !a_valid_r && !b_valid_r;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      a_valid_r         <= 1'b0;
      a_h_r             <= 32'd0;
      a_mat_a_r         <= 8'd0;
      a_mat_b_r         <= 8'd0;
      a_weight_r        <= 8'd0;
      a_mosaic_r        <= 1'b0;
      a_tx_r            <= 6'd0;
      a_ty_r            <= 6'd0;
      a_src_r           <= 16'd0;
      b_valid_r         <= 1'b0;
      b_tile_r          <= 8'd0;
      b_tx_r            <= 6'd0;
      b_ty_r            <= 6'd0;
      b_src_r           <= 16'd0;
      texture_samples_o <= 32'd0;
    end else begin
      // the counter counts RETIRED picks, so a stalled consumer never
      // double-counts one and the count equals what TEXTURE.TMU received
      if (pick_valid_o && pick_ready_i && texture_samples_o != CNT_MAX) begin
        texture_samples_o <= texture_samples_o + 32'd1;
      end
      if (advance) begin
        a_valid_r  <= req_valid_i;
        a_h_r      <= h_c;
        a_mat_a_r  <= req_mat_a_i;
        a_mat_b_r  <= req_mat_b_i;
        a_weight_r <= req_weight_i;
        a_mosaic_r <= req_mosaic_i;
        a_tx_r     <= tx_c;
        a_ty_r     <= ty_c;
        a_src_r    <= req_src_id_i;

        b_valid_r  <= a_valid_r;
        b_tile_r   <= tile_c;
        b_tx_r     <= a_tx_r;
        b_ty_r     <= a_ty_r;
        b_src_r    <= a_src_r;
      end
    end
  end

endmodule
