// zhao_texture_tmu.sv — TEXTURE.TMU: THE primary texture unit — nearest /
// bilinear / mip / CLUT / direct / wrap — and, by charter §26, the only
// unrestricted sampler this machine will ever have (phase 5, ZH-027).
//
// Law (in citation order):
//   design/contracts/TEXTURE.TMU.md — the block contract.
//   design/blocks.yml — `inputs: [texture_requests, cached_texels,
//       mosaic_pick]`, `outputs: [texture_samples]`, `upstream:
//       [RASTER.FRAGMENT, TEXTURE.CACHE, TEXTURE.MOSAIC]`, `latency:
//       variable_bounded:16`, counter `texture_samples`, and the note this
//       file honours literally: "Sample modes are spec constants; secondary
//       decal modes are cut-order 7 (§26). Star ramp discipline
//       (spec/stars_and_flares.md §1): … nearest sampling mandatory —
//       bilinear must never touch a palette."
//   ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md §26 — "a second unrestricted TMU"
//       is in the REFUSED list. See §26 IS AN ARCHITECTURAL CONSTRAINT below.
//   ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md §15 — the format order
//       (CLUT8, RGB565, CLUT4, ARGB1555, ARGB4444, then block compression),
//       "mandatory mipmaps", "explicit material and palette IDs", and the
//       Primary TMU capability list: "nearest fast path; bilinear filtered
//       path; mip selection; palette and direct colour; wrap/clamp/mirror".
//       Those five bullets are exactly the five things this block does.
//   ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md §8 — "mip selection" and "texture
//       wrap/clamp/mirror" are named non-negotiable 3D basics.
//   spec/qformats.md §2/§3/§4 — `unit8`, the single-rounding law, `rescale`.
//   spec/qformats.md §8 — perspective UV: "interpolate `u_over_w`,
//       `v_over_w` (S 8.24) and `invw24` by plane equation; per pixel
//       `u = rescale((s64)u_over_w · rcp_u24(invw24_interp))`". That divide
//       is GEOM.SETUP's / the interpolator's; the U/V that arrive here are
//       already through it. See THE UV FORMAT.
//   spec/terrain_rules.md §6.2, via design/contracts/TEXTURE.MOSAIC.md and
//       `zref::terrain::mirror_texel` — the FROZEN mirrored-repeat fold.
//       WRAP_MIRROR below is that law generalised to any power-of-two size
//       and nothing else; the directed test asserts the two agree on the
//       64-texel case the frozen helper covers.
//   spec/stars_and_flares.md §1 — "Nearest mandatory — bilinear must never
//       touch a palette", and the CLUT page is 64 entries of RGB565.
//   spec/sky_and_beams.md §1.1/§2 — which recipe needs which mode, quoted at
//       THE RECIPES THIS BLOCK MUST SERVE.
//
// ---------------------------------------------------------------------------
// §26 IS AN ARCHITECTURAL CONSTRAINT, AND IT SHAPES THIS FILE
// ---------------------------------------------------------------------------
// The charter refuses "a second unrestricted TMU" and the ledger repeats the
// refusal twice — in this block's purpose line and in TEXTURE.AUX's ("a
// restricted aux texel source … deliberately NOT a general second TMU").
// The consequence is not a comment, it is a design rule: EVERY sampling mode
// the machine will ever have has to fit through THIS request channel, because
// there is nowhere else for one to live. So the block is built as one
// datapath with a mode WORD rather than as a family of samplers:
//
//   · one address generator that serves nearest and bilinear (bilinear is
//     the same generator run on a footprint instead of a point);
//   · one wrap function, shared by u and v and by every tap;
//   · one mip-level selector in front of all of it;
//   · one format decoder, which is where CLUT and direct colour meet.
//
// A second sampler would have been the easy way to add bilinear, and it is
// precisely what §26 forbids. TEXTURE.AUX (phase 6, cut-order 2) is the
// RESTRICTED aux lane and is not this block; TEXTURE.MOSAIC (phase 6) picks a
// terrain candidate and is not this block either.
//
// ---------------------------------------------------------------------------
// THE RECIPES THIS BLOCK MUST SERVE, AND THE MODE EACH ONE PINS
// ---------------------------------------------------------------------------
// RASTER.FRAGMENT's contract already records which sampling each ratified
// recipe requires; this is the other half of that pair.
//
//   beam_additive_fade  (sky_and_beams §2) — "Bilinear TMU mandatory (nearest
//                       16-texel ramp = visible stairs). Texture 16×64 direct
//                       colour RGB565/ARGB4444 — deliberately not CLUT, so
//                       bilinear never touches a palette."
//                       → FILTER_BILINEAR + FMT_RGB565 (or FMT_ARGB4444), a
//                       non-square texture, which is why LOG2W and LOG2H are
//                       separate fields.
//   star_disc_masked /
//   star_halo_additive  (stars §1) — "CLUT8 nearest+mips … Nearest mandatory
//                       — bilinear must never touch a palette."
//                       → FMT_CLUT8 + FILTER_NEAREST + MIP_EN. The index is
//                       returned RAW on `smp_idx_o` because the fragment's
//                       alpha test is an INDEX test and its glow-tag strength
//                       is "the texel's CLUT intensity" (stars §1) — the
//                       palette colour alone cannot answer either.
//   sky drum bands      (sky_and_beams §1.1) — "1024×128 CLUT8, u-mirror,
//                       v-clamp, +mips" → per-axis wrap modes, which is why
//                       WRAP_U and WRAP_V are separate fields.
//   sky cloud sheet     (§1.1) — "256×256 ARGB4444, u/v-repeat, +mips", and
//                       the recipe is `a = tex.a × vertex.a`, so ARGB4444's
//                       alpha must survive the filter as a fourth channel.
//   sun quad            (§1.1) — "64×64 ARGB4444, alpha pre-baked".
//   terrain Mosaic tile (terrain_rules §6.2) — 64×64 CLUT8, MIRRORED repeat,
//                       nearest. WRAP_MIRROR is that frozen fold.
//
// ---------------------------------------------------------------------------
// THE UV FORMAT — found, not invented
// ---------------------------------------------------------------------------
// spec/qformats.md §8 fixes the perspective divide but never names the format
// of its RESULT. The repository does, in the only place a texture coordinate
// is actually consumed today: `zref::terrain::mirror_texel(int32_t u_raw)`
// (reference/include/zref/zref_terrain.hpp, frozen 2026-08-16) documents
// "u is Q16.16 TILE units" and computes `m = u_raw >> 10` for a 64-texel
// tile — i.e. `floor(u · 64)` with u = u_raw/65536. So a texture coordinate
// in this machine is **S 15.16 in TEXTURE units, 1.0 = one full wrap**, and
// this block adopts exactly that rather than inventing a texel-space format.
//
// Converting to texels at level L is then a SHIFT and not a multiply: with
// `size = 1 << log2s`, the texel coordinate in Q16.16 is `u_raw << log2s`
// (value = u · size), whose integer part for log2s = 6 is `u_raw >> 10` —
// bit-for-bit the frozen helper. NEAREST takes that floor directly, so
// nearest sampling IS the §6.2 law and the directed test asserts it against
// the frozen helper rather than against a restatement.
//
// THE HALF-TEXEL BIAS (a CHOICE; no spec states one). A texel owns the span
// [k, k+1) under that floor, so its CENTRE is at k + ½. Bilinear therefore
// samples at `texel_q16 − 0x8000` before taking its floor and fraction, which
// makes the two filters AGREE at every texel centre. Without the bias a
// bilinear ramp would sit half a texel away from every nearest-sampled thing
// beside it — and sky_and_beams §1.1 puts a bilinear beam and a nearest CLUT
// drum in the same frame. The alternative (no bias) is the other defensible
// convention and is what a naive implementation does; it is rejected here for
// that reason and the contract records the choice.
//
// ---------------------------------------------------------------------------
// MIP SELECTION — the LOD ARRIVES, it is not derived here
// ---------------------------------------------------------------------------
// Nothing in this repository defines a texture LOD formula, and the machinery
// the usual one needs does not exist: screen-space derivatives require 2×2
// pixel quads, and RASTER.EDGEWALK emits per-pixel coverage with no quad
// structure anywhere in its contract. What DOES exist is charter §9, the
// Measure — screen-space LOD as "the console's central law", computed
// upstream with hysteresis. So the request carries an explicit `req_lod_i`
// (U 4.4) and this block selects
//
//     level = min(lod >> 4, max_level, min(log2w, log2h))
//
// — floor, no trilinear blend. Floor because `rescale` is a rounding
// primitive for VALUES and a level is an INDEX; rounding a level to nearest
// would make a surface flip mip one texel earlier on one side of a triangle
// than the other for no gain anyone asked for. No trilinear because no spec
// asks for it, it doubles the fetch, and charter §26's cut-order exists for
// exactly this class of "nice but uncosted". Both are choices and both are
// recorded in the contract.
//
// THE THIRD CLAMP TERM IS NOT COSMETIC. The mip chain stops at the level
// where the SMALLER dimension reaches 1 texel: a 16×64 beam ramp has levels
// 0..4 and no more. The closed-form level offset below is exact only inside
// that range, so `min(log2w, log2h)` is a correctness bound, not a taste.
//
// ---------------------------------------------------------------------------
// WHERE A LEVEL LIVES — the closed form, and why it is not a loop
// ---------------------------------------------------------------------------
// Levels are packed in order after level 0. The first texel of level L sits
// at texel offset
//
//     Σ_{i<L} (W>>i)(H>>i) = W·H · (4^L − 1)/(3·4^(L−1))
//                          = REP4[L] << (log2w + log2h − 2(L−1)),  L ≥ 1
//
// with `REP4[L] = (4^L − 1)/3` = 0, 1, 5, 21, 85, 341, … — the base-4
// repunits. A 16-entry constant table and one variable shift replace an
// eleven-deep add chain, and the value is bounded by (4/3)·W·H < 2^21 for
// every legal texture, so the whole computation fits a 32-bit lane. The
// directed test checks the closed form against an explicit summation loop at
// every legal (log2w, log2h, level).
//
// ---------------------------------------------------------------------------
// WHAT AN UNSUPPORTED STATE DOES — it is LOUD
// ---------------------------------------------------------------------------
// The charter's phase-5 gate says "no unsupported state silently falls back".
// So the three malformed states below produce a DEFINED sample AND a
// one-cycle `mode_error_o` pulse; none of them is silent and none of them
// corrupts an address (charter §8: "overflow stays correct and becomes slower
// rather than corrupting memory"):
//
//   1. BILINEAR ON A PALETTE — stars §1's "bilinear must never touch a
//      palette" is a hard law, so it is ENFORCED IN THE HARDWARE rather than
//      trusted to the caller: a CLUT format with FILTER_BILINEAR samples
//      NEAREST and pulses the error. Forcing nearest (rather than refusing
//      the request) is the fail-safe direction — a palette read through a
//      filter is a banded smear of unrelated colours, which is the visible
//      corruption the law exists to prevent.
//   2. A RESERVED MODE BIT SET — bits [31:21] are reserved; a set bit means
//      the caller believes in a mode this block does not have.
//   3. MAX_LEVEL BEYOND THE CHAIN — clamped to min(log2w, log2h) as above.
//
// An undefined FORMAT code (5..7) decodes as RGB565 and pulses the error too.
//
// ---------------------------------------------------------------------------
// THE THREE THINGS THIS BLOCK DOES NOT DO, AND WHERE THEY LIVE
// ---------------------------------------------------------------------------
// · THE PERSPECTIVE DIVIDE. spec/qformats.md §8's per-pixel
//   `rcp_u24(invw24_interp)` happens upstream; U and V arrive divided. Doing
//   it here would put a reciprocal in the sampler and duplicate the one the
//   interpolator already owns.
// · TEXTURE.MOSAIC's `mosaic_pick`. The ledger lists it as an input and this
//   block does NOT grow a port for it, for the same reason RASTER.FRAGMENT
//   grew none for `soft_particles`: a Mosaic pick selects WHICH candidate
//   tile a texel comes from, and a tile's identity in this machine is its
//   BASE ADDRESS — `req_base_i`. So the pick rides the base, a mux in front
//   of this block, and TEXTURE.MOSAIC (phase 6) owns that mux. Putting the
//   two-candidate choice inside the sampler would give this block terrain's
//   material policy, which belongs to a phase-6 contract.
// · THE VRAM FETCH. Misses are TEXTURE.CACHE's; this block only ever asks.
//
// ALSO NOT BUILT, so the next wave knows: no trilinear, no anisotropic
// (charter §26 refuses it by name), no block-compressed formats (charter §15
// items 6/7, "if resources permit" — last in the order and asked for by no
// recipe), no texture writes, no LOD DERIVATION, no border colour (the three
// wrap modes are the charter's three), no per-request scissor, and no
// `texture_stalls` counter (not in the catalog).
//
// ---------------------------------------------------------------------------
// THE SAMPLER MODE WORD (32 bits, layout defined HERE and in the contract)
// ---------------------------------------------------------------------------
// Encoded so that **mode == 0 is `CLUT8, nearest, repeat/repeat, level 0`** —
// a 1×1-wrap 8-bit paletted point sample, which is this machine's most common
// texel by a wide margin (terrain Mosaic tiles, the sky drum, star discs) and
// the first format in charter §15's implementation order.
//
//   [2:0]    FORMAT      0 CLUT8, 1 RGB565, 2 CLUT4, 3 ARGB1555, 4 ARGB4444
//                        (charter §15's order; 5..7 undefined → error)
//   [3]      FILTER      0 nearest, 1 bilinear
//   [5:4]    WRAP_U      0 repeat, 1 clamp, 2 mirror (3 = repeat, reserved)
//   [7:6]    WRAP_V      same
//   [11:8]   LOG2W       level-0 width  = 1 << LOG2W
//   [15:12]  LOG2H       level-0 height = 1 << LOG2H
//   [19:16]  MAX_LEVEL   highest legal mip level
//   [20]     MIP_EN      0 = level 0 always (`req_lod_i` ignored)
//   [31:21]  reserved — must be zero
//
// ---------------------------------------------------------------------------
// FILT_LANES — THE FILTER'S RESOURCE FRONTIER, AND WHY IT IS A PARAMETER
// ---------------------------------------------------------------------------
// Every multiplier this block has ever had is in zhao_texture_bilerp. Checked
// by regex rather than by intention: there is no `*` operator anywhere in THIS
// file (the `32*k` / `16*k` that appear are constant-folded `+:` part-select
// bases), and the fit agreed to the digit — bilerp 7 DSP, this block 28, which
// is 4 × 7 with no discount.
//
// THE ADDRESSING COSTS NOTHING BECAUSE LOG2W/LOG2H EXIST. Texel conversion is
// `u_raw << log2s`, the mip level offset is a base-4-repunit table and ONE
// variable shift, and the row-major index is `(v << log2w) + u`. Every one of
// those would be a MULTIPLY if texture dimensions were arbitrary. That is the
// concrete reason non-power-of-two support is refused here and pushed to the
// asset pipeline: it would put a multiplier on the per-sample address path of
// the block that exists to have as few as possible.
//
// So the frontier axis is the filter, and it is the number of bilerp instances:
//
//     FILT_LANES │ instances │ products │ passes │ direct II │ DSPs (fitted)
//     ───────────┼───────────┼──────────┼────────┼───────────┼──────────────
//          4     │     4     │    12    │   1    │     4     │      12
//     ==>  2     │     2     │     6    │   2    │     5     │       6
//          1     │     1     │     3    │   4    │     7     │       3
//
// ONE DSP BLOCK PER PRODUCT, measured. Quartus 17.0.2 Lite packs nothing here:
// the twelve 9x9-and-18x9 products of FILT_LANES = 4 fit at twelve DSP blocks,
// not the five a Cyclone V's three-9x9-or-two-18x19 modes would allow. Plan
// cuts by counting `*` operators, not by counting the DSP-sized multipliers the
// operands would fit into. (QUARTUS_GOTCHAS.md 5 is the converse: width can
// still make it WORSE.)
//
// The four channels (R, G, B, A) are time-multiplexed through the lanes in
// `4 / FILT_LANES` passes. Passes 0 .. PASSES−2 run in ST_FILT and register
// their bytes; the LAST pass runs combinationally in ST_OUT, exactly as the
// whole filter used to. So at FILT_LANES = 4 there is no ST_FILT, no pass
// counter movement and no extra cycle — the timing is identical to the shape
// this file shipped with.
//
// CLUT NEVER FILTERS (spec/stars_and_flares.md §1, enforced in the fabric
// below), so a CLUT sample skips ST_FILT entirely and its II does not move at
// any setting.
//
// THE FRONTIER IS COVERAGE, NOT JUST DATA. At FILT_LANES = 4 there is one
// pass, `pass_c` is the constant PASSES−1, and the channel mux degenerates to
// a wire — so a mutation in the pass counter or the mux selector is textually
// live and behaviourally INVISIBLE there. It is visible at 2 and at 1. And one
// mutant is stronger still: forcing LAST_FILT_PASS to 0 is what that localparam
// ALREADY evaluates to at BOTH 4 and 2, so it is a real defect only at 1.
// NO SINGLE SETTING REACHES ALL SIX. zhao_surface_sq's S03/S04 were the
// identical shape on SQ_RADIX.
//
// Conservative SystemVerilog subset only (charter §2). Depends on
// zhao_texture_bilerp. Lint: clean under `-Wall` (lint_texture_tmu).

module zhao_texture_tmu #(
  // RESIDENT PALETTE PAGES. Each is 256 RGB565 entries = 4,096 bits, so two
  // slots is 8 Kbit and eight is 32 Kbit. Two is the shipping default because
  // it covers a terrain page plus one other without thrashing; the sweep can
  // argue for more once real traces exist. See "resident palette" below.
  parameter int unsigned PAL_SLOTS = 2,
  // 1, 2 or 4 — see FILT_LANES above. Enforced by a generate-if static
  // assertion below, wrapped in `generate`/`endgenerate` because
  // QUARTUS_GOTCHAS.md §8 records that Quartus 17.0.2 rejects a module-scope
  // `if` generate without them while three other frontends accept it.
  //
  // THE DEFAULT IS 2, AND IT WAS 4 UNTIL THE FIT ANSWERED. Recorded because the
  // reasoning that picked 4 was sound and the premise under it was false.
  //
  // The argument for 4 was that this block's demand is NOT met (the CLUT path
  // runs at 0.33× the derived 850,000 samples/frame), so throughput is the
  // scarce resource and DSPs are not — the opposite of SURFACE.STAMP, which
  // defaulted to its cheapest setting because its demand was met 26.9× over.
  // That argument still holds. What was wrong was the estimate it rested on:
  // FILT_LANES = 4 was predicted to cost 4–8 DSPs, on the reasoning that a
  // Cyclone V variable-precision block does three 9×9 or two 18×19 and the
  // twelve products would pack into about five.
  //
  // MEASURED: FILT_LANES = 4 is **12 DSPs**. Quartus 17.0.2 Lite packed
  // NOTHING — the twelve products became twelve DSP blocks, one each. (The
  // same tool behaviour this block's contract already recorded from the other
  // side: the four instances' identical weight products "did not share".) So
  // the frontier is 12 / 6 / 3 at FILT_LANES 4 / 2 / 1.
  //
  // STATED CAREFULLY, because the loose version of this is wrong and someone
  // will plan a cut from it: each nonconstant `*` creates ONE physical
  // multiplier structure, and Quartus will not fuse two of them however small
  // — but the COST of that structure depends discontinuously on operand width
  // and signedness. QUARTUS_GOTCHAS.md §5 has the same zhao_geom_lod source
  // costing 28 DSPs at 72-bit operands and 18 at 64-bit, which is impossible if
  // width were irrelevant. **The operator count is a LOWER BOUND**, exact only
  // while every operand stays inside one block's native width — which is the
  // case here (9x9 and 18x9) and was NOT the case for the 25x25 form this
  // replaced.
  //
  // 12 misses the 6–9 target the DSP campaign set; 6 is the bottom of it. The
  // cycle that buys it falls entirely on the DIRECT-COLOUR path (II 4 → 5) and
  // not at all on CLUT, which is the demand-critical one because terrain is
  // CLUT8. FILT_LANES = 1 is measured and available and is NOT the default:
  // it would spend two more cycles of a rate already short to save three DSPs
  // below a target already met, which is the same over-provisioning error the
  // 28 DSPs came from, pointed the other way.
  parameter int unsigned FILT_LANES = 2
) (
  input  logic clk,
  input  logic rst_n,

  // ---- texture_requests in (RASTER.FRAGMENT) ----------------------------
  input  logic        req_valid_i,
  // ---- palette residency invalidate ---------------------------------------
  // TEXTURE.CACHE already carries `inv_valid_i`/`inv_all_i`/`inv_addr_i` and
  // its header says why: "hot palette entries: a palette-page invalidate per
  // upload". The resident palette below is the same data with the same
  // lifetime, so it takes the same strobe from the same source. A cache that
  // cannot be invalidated is a way to serve last frame's palette.
  //
  // The rule here is deliberately COARSER than the texture cache's: ANY
  // invalidate, line or all, drops every resident palette entry. A palette
  // page is uploaded rarely and refilling it costs one access per index that
  // is actually used again, so the coarse rule cannot be wrong and the fine
  // one can. The Field engine's descriptor cache took the same decision for
  // the same reason.
  //
  // ENFORCED-BY: tests/texture/texture_tmu_directed.cpp:main
  input  logic        pal_inv_valid_i,

  output logic        req_ready_o,
  input  logic [31:0] req_u_i,         // S 15.16, texture units (see THE UV FORMAT)
  input  logic [31:0] req_v_i,
  input  logic [31:0] req_base_i,      // byte address of level 0
  input  logic [31:0] req_pal_base_i,  // byte address of the CLUT page (RGB565 entries)
  input  logic [31:0] req_mode_i,      // the mode word above
  input  logic [7:0]  req_lod_i,       // U 4.4 (charter §9's Measure, upstream)
  input  logic [15:0] req_src_id_i,

  // ---- cached_texels: the TEXTURE.CACHE access master -------------------
  output logic        cac_valid_o,
  input  logic        cac_ready_i,
  output logic [3:0]  cac_en_o,
  output logic [127:0] cac_addr_o,     // lane k at [32*k +: 32]
  output logic [15:0] cac_src_id_o,
  input  logic        cac_valid_i,
  output logic        cac_ready_o,
  input  logic [63:0] cac_data_i,      // lane k halfword at [16*k +: 16]

  // ---- texture_samples out ----------------------------------------------
  // These three fields ARE zhao_raster_fragment's `frag_texel_rgb_i`,
  // `frag_texel_a_i` and `frag_texel_idx_i`. That interface was designed for
  // this block and is not changed by its arrival.
  output logic        smp_valid_o,
  input  logic        smp_ready_i,
  output logic [23:0] smp_rgb_o,
  output logic [7:0]  smp_a_o,
  output logic [7:0]  smp_idx_o,       // CLUT index; 0 for direct colour
  output logic [15:0] smp_src_id_o,

  // ---- mode_error: a malformed request, never a silent fallback ---------
  output logic        mode_error_o,

  // ---- status -----------------------------------------------------------
  output logic        idle_o,

  // ---- counters ---------------------------------------------------------
  output logic [31:0] texture_samples_o
);

  // Illegal settings die in analysis and synthesis rather than silently
  // elaborating something that does not filter four channels. The construct is
  // an unresolved module reference inside a generate-if: it errors in every
  // tool when the condition holds and is never elaborated when it does not.
  generate
    if (!(FILT_LANES == 1 || FILT_LANES == 2 || FILT_LANES == 4)) begin : g_illegal
      ZHAO_TEXTURE_TMU_FILT_LANES_MUST_BE_1_2_OR_4 u_static_assert ();
    end
  endgenerate

  // 4 / FILT_LANES, and log2(FILT_LANES) — both exact for the three legal
  // settings, which is why they are localparams and not $clog2 of a variable.
  localparam int unsigned PASSES     = 4 / FILT_LANES;
  localparam int unsigned LANE_SHIFT = (FILT_LANES == 4) ? 2 : ((FILT_LANES == 2) ? 1 : 0);
  // The first channel index the LAST pass covers. Channels below it were
  // filtered on an earlier pass and come out of `fres_r`; channels at or above
  // it are combinational in ST_OUT.
  // (PASSES − 1)·FILT_LANES, written as 4 − FILT_LANES so that no `*` appears
  // on a DATAPATH expression anywhere in this file: QUARTUS_GOTCHAS.md §3 says
  // the only symptom of an ignored multstyle directive is a DSP count that will
  // not fall, so "there is no multiply operator here" is a property worth
  // keeping checkable by regex rather than by reading.
  //
  // Stated exactly, because a claim that is nearly true is worse than none: the
  // file contains ONE `*` outside comments, in `logic [FILT_LANES*8-1:0]
  // bl_out` — a packed WIDTH, folded at elaboration into a constant, which
  // infers nothing. Every arithmetic `*` in this block's cone lives in
  // zhao_texture_bilerp, where there are exactly three.
  localparam int unsigned LAST_BASE  = 4 - FILT_LANES;
  // The last pass ST_FILT itself runs. Guarded so PASSES = 1 does not compute
  // an unsigned 0 − 2; ST_FILT is unreachable there and the value is unused.
  localparam int unsigned LAST_FILT_PASS = (PASSES > 1) ? (PASSES - 2) : 0;

  localparam logic [31:0] CNT_MAX = 32'hFFFF_FFFF;

  localparam logic [2:0] FMT_CLUT8    = 3'd0;
  localparam logic [2:0] FMT_RGB565   = 3'd1;
  localparam logic [2:0] FMT_CLUT4    = 3'd2;
  localparam logic [2:0] FMT_ARGB1555 = 3'd3;
  localparam logic [2:0] FMT_ARGB4444 = 3'd4;

  localparam logic [1:0] WRAP_REPEAT = 2'd0;
  localparam logic [1:0] WRAP_CLAMP  = 2'd1;
  localparam logic [1:0] WRAP_MIRROR = 2'd2;

  // Three bits, because ST_FILT joins the original four. ST_FILT is entered
  // only when PASSES > 1 AND the sample is direct colour; at FILT_LANES = 4 it
  // is unreachable and the state graph is exactly the one this file shipped
  // with.
  localparam logic [2:0] ST_IDLE = 3'd0;
  localparam logic [2:0] ST_TEX  = 3'd1;
  localparam logic [2:0] ST_PAL  = 3'd2;
  localparam logic [2:0] ST_OUT  = 3'd3;
  localparam logic [2:0] ST_FILT = 3'd4;

  // (4^L − 1)/3, the base-4 repunits — see WHERE A LEVEL LIVES.
  localparam logic [31:0] REP4 [0:15] = '{
    32'd0,       32'd1,        32'd5,        32'd21,
    32'd85,      32'd341,      32'd1365,     32'd5461,
    32'd21845,   32'd87381,    32'd349525,   32'd1398101,
    32'd5592405, 32'd22369621, 32'd89478485, 32'd357913941
  };

  // ======================================================= mode decode =====
  logic [2:0] m_fmt;
  logic       m_filter, m_mip_en;
  logic [1:0] m_wrap_u, m_wrap_v;
  logic [3:0] m_log2w, m_log2h, m_maxlvl;
  logic [10:0] m_rsvd;
  always_comb begin
    m_fmt    = req_mode_i[2:0];
    m_filter = req_mode_i[3];
    m_wrap_u = req_mode_i[5:4];
    m_wrap_v = req_mode_i[7:6];
    m_log2w  = req_mode_i[11:8];
    m_log2h  = req_mode_i[15:12];
    m_maxlvl = req_mode_i[19:16];
    m_mip_en = req_mode_i[20];
    m_rsvd   = req_mode_i[31:21];
  end

  logic is_clut, is_16bpp, fmt_bad;
  always_comb begin
    is_clut  = (m_fmt == FMT_CLUT8) || (m_fmt == FMT_CLUT4);
    is_16bpp = (m_fmt == FMT_RGB565) || (m_fmt == FMT_ARGB1555) || (m_fmt == FMT_ARGB4444);
    fmt_bad  = !is_clut && !is_16bpp;  // 5..7
  end

  // ---- the three malformed states (see WHAT AN UNSUPPORTED STATE DOES) ---
  logic chain_max_lt, err_c, filter_eff;
  logic [3:0] chain_max, lvl_cap;
  always_comb begin
    chain_max    = (m_log2w < m_log2h) ? m_log2w : m_log2h;
    chain_max_lt = (m_maxlvl > chain_max);
    lvl_cap      = chain_max_lt ? chain_max : m_maxlvl;
    // stars §1 enforced in fabric: a palette is never filtered.
    filter_eff   = m_filter && !is_clut;
    err_c        = (m_filter && is_clut) || (m_rsvd != 11'd0) || chain_max_lt || fmt_bad;
  end

  // ---- level selection ---------------------------------------------------
  logic [3:0] lvl_req, level;
  always_comb begin
    lvl_req = m_mip_en ? req_lod_i[7:4] : 4'd0;
    level   = (lvl_req > lvl_cap) ? lvl_cap : lvl_req;
  end

  logic [3:0] log2w_l, log2h_l;
  logic [31:0] mask_u, mask_v, size_u;
  always_comb begin
    log2w_l = m_log2w - level;
    log2h_l = m_log2h - level;
    size_u  = 32'd1 << log2w_l;
    mask_u  = size_u - 32'd1;
    mask_v  = (32'd1 << log2h_l) - 32'd1;
  end

  // ---- where the level lives (the closed form) ---------------------------
  logic [31:0] lvl_off;
  logic [5:0]  lvl_shift;
  always_comb begin
    lvl_shift = 6'({4'd0, m_log2w} + {4'd0, m_log2h}) - 6'({4'd0, (level - 4'd1)} << 1);
    lvl_off   = (level == 4'd0) ? 32'd0 : (REP4[level] << lvl_shift);
  end

  // ======================================================= texel coords ====
  // texel_q16 = u_raw << log2s  — an exact shift, no rounding (THE UV FORMAT).
  // 48 bits because u is S 15.16 and the shift is up to 10.
  logic signed [47:0] tu_q, tv_q, tu_b, tv_b;
  always_comb begin
    tu_q = $signed({{16{req_u_i[31]}}, req_u_i}) <<< log2w_l;
    tv_q = $signed({{16{req_v_i[31]}}, req_v_i}) <<< log2h_l;
    // The half-texel bias: bilinear samples about the texel CENTRE so that it
    // agrees with nearest there. Nearest keeps the frozen §6.2 floor.
    tu_b = filter_eff ? (tu_q - 48'sd32768) : tu_q;
    tv_b = filter_eff ? (tv_q - 48'sd32768) : tv_q;
  end

  logic signed [31:0] iu0, iv0;
  logic [7:0]         fu, fv;
  always_comb begin
    iu0 = tu_b[47:16];
    iv0 = tv_b[47:16];
    fu  = filter_eff ? tu_b[15:8] : 8'd0;
    fv  = filter_eff ? tv_b[15:8] : 8'd0;
  end

  // ---- the wrap function -------------------------------------------------
  // REPEAT: `t & (S−1)` — two's complement AND is the FLOORED modulo, so a
  //         negative u wraps correctly with no correction.
  // CLAMP : to [0, S−1].
  // MIRROR: spec/terrain_rules.md §6.2's frozen fold, generalised:
  //         per = floored t mod 2S; texel = per < S ? per : 2S−1−per.
  //         With S a power of two, `per & (S−1)` is `2S−1−per` bit-inverted
  //         in the low bits, so the whole fold is a mask, a bit test and a
  //         subtract. At S = 64 this is `zref::terrain::mirror_texel` exactly.
  function automatic logic [31:0] wrap_coord(input logic signed [31:0] t,
                                             input logic [1:0]         mode,
                                             input logic [31:0]        mask);
    logic [31:0] tu_, per, lo_;
    begin
      tu_ = $unsigned(t);
      case (mode)
        WRAP_CLAMP:  wrap_coord = t[31] ? 32'd0 : ((tu_ > mask) ? mask : tu_);
        WRAP_MIRROR: begin
          per = tu_ & ((mask << 1) | 32'd1);
          lo_ = per & mask;
          wrap_coord = (per > mask) ? (mask - lo_) : lo_;
        end
        WRAP_REPEAT: wrap_coord = tu_ & mask;
        default:     wrap_coord = tu_ & mask;  // the reserved code 3 repeats
      endcase
    end
  endfunction

  // ---- the four taps -----------------------------------------------------
  // Tap k = (iu0 + k[0], iv0 + k[1]) — TEXTURE.CACHE lane k, which is what
  // makes a bilinear sample ONE cache access (see that block's FOUR LANES).
  logic [31:0] tap_u  [0:3];
  logic [31:0] tap_v  [0:3];
  logic [31:0] texel  [0:3];  // the row-major texel index within the level
  logic [31:0] total  [0:3];  // ...plus the level offset
  logic [31:0] addr   [0:3];
  always_comb begin
    for (int unsigned k = 0; k < 4; k++) begin
      tap_u[k] = wrap_coord(iu0 + 32'(k[0]), m_wrap_u, mask_u);
      tap_v[k] = wrap_coord(iv0 + 32'(k[1]), m_wrap_v, mask_v);
      // ROW-MAJOR, and that is a CHOICE recorded rather than a citation:
      // charter §15's Layout bullet asks for "swizzled/Morton-order small
      // blocks", but no Morton formula is ratified anywhere in this
      // repository, while the only concrete texture layout that exists —
      // zref::render::TerrainTileset (`tiles[t][(ty<<6)+tx]`, consumed by
      // reference/src/zrender/rast.cpp) — is row-major. Matching the shipping
      // reference beats inventing a swizzle the asset compiler does not emit.
      texel[k] = (tap_v[k] << log2w_l) + tap_u[k];
      total[k] = lvl_off + texel[k];
      addr[k]  = is_16bpp ? (req_base_i + (total[k] << 1))
               : (m_fmt == FMT_CLUT4) ? (req_base_i + (total[k] >> 1))
               : (req_base_i + total[k]);
    end
  end


  // ======================================================= the request =====
  // Four states plus ST_FILT, and NO PIPELINE. A sample is one texel access
  // (four lanes for bilinear, one for nearest) plus, for the CLUT formats
  // only, a second access for the palette entry — the index is not known until
  // the first one answers, so the two are unavoidably serial.
  //
  // WHAT THIS COSTS, measured rather than papered over. Against a cache that
  // answers in one cycle: accept in cycle N, sample retired in N+3 for direct
  // colour (plus PASSES−1 for the filter's early passes) and N+5 for CLUT, and
  // the next request is accepted the cycle after that, because `req_ready_o`
  // is `st_r == ST_IDLE` and nothing overlaps. So the sustained rate is ONE
  // SAMPLE PER FOUR CLOCKS at FILT_LANES = 4 (five at 2, seven at 1) for
  // direct colour, and PER SIX for CLUT at every setting — not the ledger's
  // "1 sample per clock". The half of that line this block DOES meet is
  // "bilinear = 1 request": four taps are one request beat and one cache
  // access, never four serialised lookups.
  //
  // AND IT IS SHORT OF THE DERIVED DEMAND, which is a separate defect from the
  // DSP one and is NOT fixed by the FILT_LANES rearchitecture. docs/
  // OWNER_DOCKET.md's "THE THREE DEMAND NUMBERS" derives 850,000 samples per
  // frame from Sacrifice's layered terrain (tile + detail + lightmap, so ≥3
  // samples a terrain pixel; 92,160 pixels at 3× overdraw = 829,440). Against
  // design/budgets/latency.md's compute budget of 1,666,667 clocks a frame —
  // NOT the 251,520 raster period, which is 6.6× smaller and is also called
  // "gpu cycles" — that is one sample every two clocks. This block does one
  // every four (direct) or six (CLUT), and terrain is CLUT8, so the
  // demand-critical figure is 277,778 samples/frame against 850,000: 0.33×.
  //
  // Reaching one per two clocks needs a pipelined address → fetch → filter
  // chain with two requests in flight and an arbiter over the single cache
  // access port (a CLUT sample needs two accesses and zhao_texture_cache
  // accepts one per clock, so II = 2 is the port's own floor and matches the
  // demand exactly). That is a change to THIS FILE only — the ports, the mode
  // word and zref::Tmu are all unaffected — and it is stated in
  // design/contracts/TEXTURE.TMU.md's Target throughput section rather than
  // left for a reader to discover.
  // ENFORCED-BY: tests/texture/texture_tmu_directed.cpp:test_backpressure_and_latency
  // (the worst accept-to-retire is measured and asserted ≤ 16, the ledger's
  // `variable_bounded:16`).
  logic [2:0]   st_r;
  logic [1:0]   pass_r;      // which filter pass ST_FILT is on (0 .. PASSES−2)
  logic [31:0]  fres_r;      // channel c's filtered byte at [8*c +: 8]
  logic         sent_r;      // this state's cache request has been taken
  logic [127:0] q_addr_r;
  logic [3:0]   q_en_r;
  logic [2:0]   q_fmt_r;
  logic         q_clut_r;
  logic [7:0]   q_fu_r, q_fv_r;
  logic         q_bytesel_r, q_nib_r;
  logic [31:0]  q_palbase_r;
  logic [15:0]  q_src_r;
  logic [15:0]  hw_r [0:3];
  logic [7:0]   idx_r;

  assign req_ready_o = (st_r == ST_IDLE);
  assign idle_o      = (st_r == ST_IDLE);

  logic req_go;
  assign req_go = req_valid_i && req_ready_o;

  // ---- the cache master --------------------------------------------------
  // Hygiene: `cac_valid_o` is a function of registers only, never of
  // `cac_ready_i`; `req_ready_o` and `smp_valid_o` likewise.
  assign cac_valid_o  = ((st_r == ST_TEX) || (st_r == ST_PAL)) && !sent_r;
  assign cac_ready_o  = ((st_r == ST_TEX) || (st_r == ST_PAL)) && sent_r;
  assign cac_en_o     = q_en_r;
  assign cac_addr_o   = q_addr_r;
  assign cac_src_id_o = q_src_r;

  logic cac_rsp;
  assign cac_rsp = cac_valid_i && cac_ready_o;

  // ======================================================= format decode ===
  // Per-tap direct-colour expansion. The 5- and 6-bit laws are the frozen
  // ones (`c8 = (c5<<3)|(c5>>2)`, `(c6<<2)|(c6>>4)` — spec/stars_and_flares.md
  // §2, implemented as zref::sky::rgb565::to_rgb888 and, in arithmetically
  // identical form, as `(c·255 + half)/max` in reference/src/zrender/rast.cpp
  // and tools/reel). The 4-bit and 1-bit laws are UNWRITTEN anywhere; bit
  // replication is the consistent extension of the two that are written — it
  // is the unique expansion that round-trips both 0 and full scale exactly —
  // and the contract records it as a choice.
  function automatic logic [31:0] decode16(input logic [15:0] h, input logic [2:0] fmt);
    logic [7:0] a_, r_, g_, b_;
    begin
      case (fmt)
        FMT_ARGB1555: begin
          a_ = h[15] ? 8'd255 : 8'd0;
          r_ = {h[14:10], h[14:12]};
          g_ = {h[9:5],   h[9:7]};
          b_ = {h[4:0],   h[4:2]};
        end
        FMT_ARGB4444: begin
          a_ = {h[15:12], h[15:12]};
          r_ = {h[11:8],  h[11:8]};
          g_ = {h[7:4],   h[7:4]};
          b_ = {h[3:0],   h[3:0]};
        end
        default: begin  // RGB565, and the undefined codes that decode as it
          a_ = 8'd255;
          r_ = {h[15:11], h[15:13]};
          g_ = {h[10:5],  h[10:9]};
          b_ = {h[4:0],   h[4:2]};
        end
      endcase
      decode16 = {a_, r_, g_, b_};
    end
  endfunction

  // One packed 4-tap footprint per CHANNEL, in the order the filter lanes are
  // multiplexed over: 0 = R, 1 = G, 2 = B, 3 = A. Tap k sits at [8*k +: 8], so
  // a lane's four inputs are one 32-bit select away.
  logic [31:0] dec_c   [0:3];
  logic [31:0] ch_pack [0:3];
  always_comb begin
    for (int unsigned k = 0; k < 4; k++) begin
      dec_c[k] = decode16(hw_r[k], q_fmt_r);
      ch_pack[0][8*k +: 8] = dec_c[k][23:16];  // R
      ch_pack[1][8*k +: 8] = dec_c[k][15:8];   // G
      ch_pack[2][8*k +: 8] = dec_c[k][7:0];    // B
      ch_pack[3][8*k +: 8] = dec_c[k][31:24];  // A
    end
  end

  // ---- the filter: FILT_LANES channels at a time -------------------------
  // The arithmetic, its factored form and its single rounding live in
  // zhao_texture_bilerp, which is a separate file for the same reason
  // zhao_raster_blend is: tests/formal/texture_bilerp.sby proves the SHIPPING
  // filter, not a copy of it. This block instantiates FILT_LANES of it and
  // walks the four channels past them; the module itself is unparameterised
  // and unchanged, so the proof covers every lane at every setting.
  //
  // `pass_c` is the pass the COMBINATIONAL lanes are computing right now:
  // ST_OUT is always the LAST pass, which is what makes FILT_LANES = 4 cost
  // exactly zero extra cycles.
  logic [1:0] pass_c, sel_base;
  always_comb begin
    pass_c   = (st_r == ST_OUT) ? 2'(PASSES - 1) : pass_r;
    sel_base = 2'(pass_c << LANE_SHIFT);
  end

  logic [FILT_LANES*8-1:0] bl_out;
  genvar gj;
  generate
    for (gj = 0; gj < int'(FILT_LANES); gj++) begin : g_lane
      logic [31:0] tsel;
      assign tsel = ch_pack[sel_base | 2'(gj)];
      zhao_texture_bilerp u_bl (.t00_i(tsel[7:0]),
                                .t10_i(tsel[15:8]),
                                .t01_i(tsel[23:16]),
                                .t11_i(tsel[31:24]),
                                .fu_i (q_fu_r),
                                .fv_i (q_fv_r),
                                .out_o(bl_out[8*gj +: 8]));
    end
  endgenerate

  // The whole filtered texel: the last pass's channels straight off the lanes,
  // the earlier passes' out of `fres_r`. Both selectors are elaboration-time
  // constants, so this is wiring, not a mux.
  logic [7:0] fin [0:3];
  genvar gc;
  generate
    for (gc = 0; gc < 4; gc++) begin : g_fin
      if (gc >= int'(LAST_BASE)) begin : g_now
        assign fin[gc] = bl_out[8*(gc - int'(LAST_BASE)) +: 8];
      end else begin : g_held
        assign fin[gc] = fres_r[8*gc +: 8];
      end
    end
  endgenerate

  // With FILTER_NEAREST the fractions are forced to 0, so A = t00<<8 and
  // S = t00<<16 and the filter is the exact identity on tap 0 (see
  // zhao_texture_bilerp's ENDPOINTS). Nearest therefore takes the SAME
  // datapath as bilinear rather than a parallel one, and the same number of
  // passes — one sampler, which is the §26 shape.
  // ENFORCED-BY: tests/texture/texture_tmu_directed.cpp:test_nearest_is_the_bilinear_identity

  // ---- the CLUT index, taken off the cache bus ---------------------------
  // The index must be captured the cycle the TEXEL halfword arrives, because
  // the palette access reuses `hw_r[0]` for the palette entry.
  logic [7:0] bus_byte, bus_idx;
  always_comb begin
    bus_byte = q_bytesel_r ? cac_data_i[15:8] : cac_data_i[7:0];
    bus_idx  = (q_fmt_r == FMT_CLUT4)
             ? (q_nib_r ? {4'd0, bus_byte[7:4]} : {4'd0, bus_byte[3:0]})
             : bus_byte;
  end

  // The palette page holds RGB565 entries (spec/stars_and_flares.md §1: a
  // 64-entry page is "≤512 B", i.e. 2 B an entry; zref::render::TerrainTileset
  // carries `uint16_t palette[256]` likewise).
  logic [31:0] pal_addr_c;
  assign pal_addr_c = q_palbase_r + {23'd0, bus_idx, 1'b0};

  // =================================================== resident palette ====
  // A CLUT sample used to cost TWO dependent texture-cache accesses: one for
  // the index, one for the palette entry that index names. That second access
  // is the whole reason CLUT runs at 6 clocks a sample -- 277,778 samples a
  // frame against a demand of 850,000, 0.33x -- and it is the reason this
  // file's own throughput note concluded that "II = 2 is the port's own floor".
  // It is not the port's floor. It is the floor of asking the port twice.
  //
  // The palette does not belong in the texture cache. A cache lane is
  // LINES x LINE_BYTES = 256 bytes and a 256-entry RGB565 page is 512, so a
  // page cannot even reside in one lane; the mapping is direct, so entries
  // alias; there is one fill engine and no hit-under-miss, so a palette miss
  // blocks the texel access sitting behind it. Palettes are tiny, they are
  // named explicitly by `req_pal_base_i`, and they are rewritten once per
  // upload -- exactly the wrong thing to demand-page through a general cache.
  //
  // So they live here instead. SLOTS pages, each 256 RGB565 entries, tagged by
  // the palette base address the request already carries. On a hit the palette
  // entry is in hand the same cycle the index arrives and the sample never
  // enters ST_PAL at all. On a miss it takes the access it always took and
  // fills the entry on the way past, so the SECOND use of any index is free.
  //
  // No contract changed to do this. The request still names a VRAM palette
  // base; nothing was added to the mode word; `zref::Tmu` has not been touched.
  // The residency is an implementation of the same law, which is why the whole
  // existing directed and random suite is the evidence that it is exact.
  //
  // ENFORCED-BY: tests/texture/texture_tmu_directed.cpp:main
  logic [31:0]  pal_tag_r  [PAL_SLOTS];
  logic         pal_ten_r  [PAL_SLOTS];              // this slot holds a page
  logic [255:0] pal_val_r  [PAL_SLOTS];              // per-entry residency
  logic [15:0]  pal_dat_r  [PAL_SLOTS][256];
  logic [PAL_SLOTS == 1 ? 0 : $clog2(PAL_SLOTS)-1:0] pal_vic_r;

  // Which slot holds this request's page, and is the entry it wants resident?
  logic pal_slot_hit_c, pal_entry_hit_c;
  logic [PAL_SLOTS == 1 ? 0 : $clog2(PAL_SLOTS)-1:0] pal_way_c;
  always_comb begin
    pal_slot_hit_c  = 1'b0;
    pal_entry_hit_c = 1'b0;
    pal_way_c       = '0;
    for (int unsigned w = 0; w < PAL_SLOTS; w++)
      if (pal_ten_r[w] && (pal_tag_r[w] == q_palbase_r)) begin
        pal_slot_hit_c  = 1'b1;
        pal_way_c       = $bits(pal_way_c)'(w);
        pal_entry_hit_c = pal_val_r[w][bus_idx];
      end
  end

  // ---- the sample --------------------------------------------------------
  // Driven combinationally off registered state in ST_OUT: no extra cycle,
  // and no dependence on `smp_ready_i`.
  logic [31:0] pal_dec;
  assign pal_dec = decode16(hw_r[0], FMT_RGB565);

  // Three deliberate dead ends, sunk explicitly rather than by a lint waiver
  // in the style of zhao_raster_blend's own `unused_ok`:
  //   · `req_lod_i[3:0]` — the U 4.4 LOD's FRACTION. Mip selection is a floor
  //     to an integer level (see MIP SELECTION), so the fraction is what a
  //     trilinear blend would consume and this block deliberately has none.
  //     The field is carried in the request so adding trilinear later is a
  //     datapath change and not an ABI one.
  //   · `tu_b[7:0]` / `tv_b[7:0]` — sub-texel precision below the unit8
  //     fraction. spec/qformats.md §2 makes a filter weight a `unit8`, so the
  //     low 8 bits of a Q16.16 texel coordinate are below the weight's LSB.
  //   · `pal_dec[31:24]` — the alpha lane of the palette decode. A CLUT
  //     texel's alpha is its index's business, never the palette entry's.
  //   · `fres_r` and `pass_r` AT FILT_LANES = 4 ONLY. There is one filter
  //     pass there, so ST_FILT is unreachable and the held-channel register
  //     bank has no reader. It is still DRIVEN, deliberately, rather than
  //     generated away: keeping one sequential body for all three settings is
  //     what makes the mutation sweep's frontier builds comparable, and a
  //     register with no reader costs nothing after synthesis.
  logic unused_ok;
  always_comb begin
    unused_ok = |req_lod_i[3:0] | |tu_b[7:0] | |tv_b[7:0] | |pal_dec[31:24]
              | |fres_r | |pass_r;
    unused_ok = unused_ok & 1'b0;
  end

  assign smp_valid_o  = (st_r == ST_OUT);
  assign smp_src_id_o = q_src_r;
  // A CLUT texel's alpha is 255: its transparency is a property of its INDEX,
  // tested by RASTER.FRAGMENT (its contract's THE ALPHA TEST IS AN INDEX
  // TEST), never of a palette alpha the RGB565 page has no room for.
  assign smp_rgb_o    = q_clut_r ? pal_dec[23:0] : {fin[0], fin[1], fin[2]};
  assign smp_a_o      = q_clut_r ? 8'd255 : fin[3];
  // A direct-colour texel HAS no index. It is reported as 0, and the two
  // state bits that read it (RASTER.FRAGMENT's ATEST_EN and TAG_FROM_TEXEL)
  // are set by no direct-colour recipe.
  assign smp_idx_o    = q_clut_r ? idx_r : 8'd0;

  // ---- sequential --------------------------------------------------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_r              <= ST_IDLE;
      pass_r            <= 2'd0;
      pal_vic_r         <= '0;
      for (int unsigned w = 0; w < PAL_SLOTS; w++) begin
        pal_ten_r[w] <= 1'b0;
        pal_tag_r[w] <= 32'd0;
        pal_val_r[w] <= 256'd0;
      end
      fres_r            <= 32'd0;
      sent_r            <= 1'b0;
      q_addr_r          <= 128'd0;
      q_en_r            <= 4'd0;
      q_fmt_r           <= FMT_CLUT8;
      q_clut_r          <= 1'b0;
      q_fu_r            <= 8'd0;
      q_fv_r            <= 8'd0;
      q_bytesel_r       <= 1'b0;
      q_nib_r           <= 1'b0;
      q_palbase_r       <= 32'd0;
      q_src_r           <= 16'd0;
      idx_r             <= 8'd0;
      mode_error_o      <= 1'b0;
      texture_samples_o <= 32'd0;
      for (int unsigned k = 0; k < 4; k++) hw_r[k] <= 16'd0;
    end else begin
      mode_error_o <= 1'b0;

      case (st_r)
        // ---- accept, and freeze everything the request implies ----------
        ST_IDLE: begin
          if (req_go) begin
            st_r         <= ST_TEX;
            sent_r       <= 1'b0;
            q_en_r       <= filter_eff ? 4'b1111 : 4'b0001;
            q_fmt_r      <= fmt_bad ? FMT_RGB565 : m_fmt;
            q_clut_r     <= is_clut;
            q_fu_r       <= fu;
            q_fv_r       <= fv;
            q_bytesel_r  <= addr[0][0];
            q_nib_r      <= total[0][0];
            q_palbase_r  <= req_pal_base_i;
            q_src_r      <= req_src_id_i;
            for (int unsigned k = 0; k < 4; k++) q_addr_r[32*k +: 32] <= addr[k];
            mode_error_o <= err_c;
          end
        end

        // ---- the texel access -------------------------------------------
        ST_TEX: begin
          if (cac_valid_o && cac_ready_i) sent_r <= 1'b1;
          if (cac_rsp) begin
            for (int unsigned k = 0; k < 4; k++) hw_r[k] <= cac_data_i[16*k +: 16];
            if (q_clut_r) begin
              // A second access for the palette entry — lane 0 only, because
              // a palette is never filtered (stars §1), so there is exactly
              // one index to look up.
              idx_r             <= bus_idx;
              if (pal_entry_hit_c) begin
                // RESIDENT. The colour is in hand in the same cycle the index
                // arrived, so the second cache access does not happen and the
                // sample goes straight out. This is the clock the CLUT path
                // exists to save, and it is saved on the acceptance side --
                // the request after this one starts one state earlier.
                hw_r[0] <= pal_dat_r[pal_way_c][bus_idx];
                st_r    <= ST_OUT;
              end else begin
                // A second access for the palette entry -- lane 0 only,
                // because a palette is never filtered (stars section 1), so
                // there is exactly one index to look up.
                q_en_r            <= 4'b0001;
                q_addr_r[31:0]    <= pal_addr_c;
                sent_r            <= 1'b0;
                st_r              <= ST_PAL;
              end
            end else begin
              // Direct colour: the texels are here, so the filter can run. At
              // FILT_LANES = 4 there is exactly one pass and it is ST_OUT's,
              // so this goes straight to ST_OUT and the cycle count does not
              // move from the shape this file shipped with.
              pass_r <= 2'd0;
              st_r   <= (PASSES > 1) ? ST_FILT : ST_OUT;
            end
          end
        end

        // ---- the filter's early passes ----------------------------------
        // PASSES − 1 cycles, registering FILT_LANES channels each. The LAST
        // pass is not run here: it is combinational in ST_OUT.
        ST_FILT: begin
          for (int unsigned j = 0; j < FILT_LANES; j++) begin
            // channel index (sel_base | j), byte-aligned by the 3'd0 concat —
            // a shift, so no `*` reaches the RTL.
            fres_r[{(sel_base | 2'(j)), 3'd0} +: 8] <= bl_out[8*j +: 8];
          end
          if (pass_r == 2'(LAST_FILT_PASS)) begin
            st_r <= ST_OUT;
          end else begin
            pass_r <= pass_r + 2'd1;
          end
        end

        // ---- the palette access -----------------------------------------
        // A CLUT texel is never filtered (stars §1), so this path does not
        // visit ST_FILT at any FILT_LANES setting and its II does not move.
        ST_PAL: begin
          if (cac_valid_o && cac_ready_i) sent_r <= 1'b1;
          if (cac_rsp) begin
            hw_r[0] <= cac_data_i[15:0];
            st_r    <= ST_OUT;
            // FILL ON THE WAY PAST. The entry is being paid for anyway, so
            // record it: the next sample that lands on this index skips the
            // access entirely. A page not yet resident claims a slot here, and
            // claiming it clears that slot's entries -- a slot holds ONE page
            // and a stale bit from the previous occupant would be served as
            // this page's colour.
            if (pal_slot_hit_c) begin
              pal_dat_r[pal_way_c][idx_r] <= cac_data_i[15:0];
              pal_val_r[pal_way_c][idx_r] <= 1'b1;
            end else begin
              pal_tag_r[pal_vic_r]        <= q_palbase_r;
              pal_ten_r[pal_vic_r]        <= 1'b1;
              pal_val_r[pal_vic_r]        <= 256'd0;
              pal_val_r[pal_vic_r][idx_r] <= 1'b1;
              pal_dat_r[pal_vic_r][idx_r] <= cac_data_i[15:0];
              pal_vic_r <= (PAL_SLOTS == 1)
                             ? '0
                             : (($bits(pal_vic_r)'(pal_vic_r) == $bits(pal_vic_r)'(PAL_SLOTS - 1))
                                    ? '0
                                    : pal_vic_r + $bits(pal_vic_r)'(1));
            end
          end
        end

        // ---- present ----------------------------------------------------
        ST_OUT: begin
          if (smp_ready_i) begin
            st_r <= ST_IDLE;
            if (texture_samples_o != CNT_MAX) texture_samples_o <= texture_samples_o + 32'd1;
          end
        end

        default: st_r <= ST_IDLE;
      endcase

      // ANY INVALIDATE DROPS EVERY RESIDENT PAGE. Placed after the case so it
      // beats a fill landing in the same clock: a fill that raced an upload
      // would install the value the upload has just replaced, which is the one
      // failure a resident palette can have and the one no throughput number
      // would ever show.
      if (pal_inv_valid_i)
        for (int unsigned w = 0; w < PAL_SLOTS; w++) begin
          pal_ten_r[w] <= 1'b0;
          pal_val_r[w] <= 256'd0;
        end
    end
  end

endmodule : zhao_texture_tmu
