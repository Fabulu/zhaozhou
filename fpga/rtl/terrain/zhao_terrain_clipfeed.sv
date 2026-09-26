// zhao_terrain_clipfeed.sv -- TERRAIN's TRIANGLE, MADE INTO A GEOM.CLIPDOOR
// CLIENT. Console entry I13, items 1 and 2 of the TERRTRI list.
//
// ENFORCED-BY: tests/terrain/terrain_clipfeed_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHY THIS BLOCK EXISTS
// ---------------------------------------------------------------------------
// Entry I13 has been refused five times. Every refusal was correct at the time
// and the last two cleared the ground completely:
//
//   * CELLCARRY refused it because two arithmetic laws were, in its words,
//     "unsettled". SHADELADDER corrected that -- neither was ever unsettled,
//     both were UNBUILT -- and built them:
//       `zhao_terrain_shademod` (the palette ladder and the one rounding)
//       `zhao_geom_overw_sat`   (the S8.24 saturate on a s32 coordinate)
//     Both were registered as OPEN deferrals whose stated delete condition is
//     "the packet that lays the carriage instantiates this module". THIS IS
//     THAT PACKET and this is that instantiation, for both.
//
//   * The remaining items were `invw24`, the perspective multiply, and the
//     fourth `zhao_geom_clipdoor` client. All three are THIS FILE. They are
//     one block and not three because they are one beat: a terrain triangle
//     cannot be offered at the door until its seven attribute slots are
//     filled, and two of those slots are the perspective multiply of the third.
//
// ---------------------------------------------------------------------------
// THE THREE-WAY JOIN, AND WHY IT IS A JOIN RATHER THAN A FIFO TRIPLE
// ---------------------------------------------------------------------------
// Terrain leaves `zhao_console_core` on THREE streams that are rate-locked at
// their common source but are separate handshakes at this block's input:
//
//     proj_out_*   the replayed TRIANGLE: screen corners, raw w, behind, and
//                  the layer-E {mat_a, mat_b, weight} the projector forwards
//     terr_light_* the flat shade, ONE signed Q16.16 scalar per triangle
//     terr_uv_*    the per-corner u/v, Q16.16 in TILE units (terrain_rules 6.2)
//
// The core's own composer rate-locks the three by taking a terrain reference
// only when replay AND the light lane AND the coordinate lane can all accept
// it, so they issue in lockstep from one reference and carry the same
// `src_id`. THIS BLOCK DOES NOT TRUST THAT. It accepts a beat only when all
// three are offered, consumes all three on the SAME CLOCK, and counts a
// `src_id` disagreement on `src_id_mismatch_o`.
//
// THAT COUNTER IS NOT ONE OF THIS REPOSITORY'S BLIND ONES, and the reason is
// the one CLAUDE.md's chapter names: its three operands are loaded by THREE
// DIFFERENT ENABLES in THREE DIFFERENT BLOCKS -- `zhao_geom_replay`'s,
// `zhao_terrain_lightlane`'s and `zhao_terrain_uvlane`'s. No single register
// enable drives two sides of the comparison, so a skew is a state the
// comparison can actually reach. It is fired by stimulus in the directed test
// (section 5) rather than argued about.
//
// ---------------------------------------------------------------------------
// ONE TRIANGLE AT A TIME, AND WHY THAT IS THE RIGHT TRADE HERE
// ---------------------------------------------------------------------------
// `zhao_part_clipfeed` carries a RING because particles arrive continuously
// and `zhao_geom_depthquant_stream` answers in COMPLETION order, so a
// particle's three corners can be overtaken by the next particle's. This block
// is strictly serial instead: it accepts one triangle, converts its three
// corners, packs, emits, and only then accepts another.
//
// The trade is stated rather than assumed. A terrain frame in the console
// smoke replays 128 triangles; this block spends roughly one shademod
// (17 clocks), six perspective multiplies and the depthquant round trip on
// each, so the arm costs a few thousand clocks against a frame budget the
// smoke measures in tens of thousands. THE THROUGHPUT QUESTION IS NAMED HERE
// SO IT IS SPENT ON A QUESTION AND NOT A HUNCH: if terrain's triangle rate
// rises past what this serial form sustains, the repair is
// `zhao_part_clipfeed`'s ring, which is already written and already verified;
// it is not a rewrite of the arithmetic. Serial was chosen because a ring that
// reorders three separate quantities is where a metadata swap lives, and this
// entry has been refused five times for laying wire ahead of evidence.
//
// ---------------------------------------------------------------------------
// THE SEVEN SLOTS, AND THE OWNER OF EACH
// ---------------------------------------------------------------------------
//   slot 0  invw24    `zhao_geom_depthquant_stream` + `zhao_raster_rcp24_v4`,
//                     the same pair `zhao_geom_vattr`, `zhao_forge_assemble`
//                     and `zhao_part_clipfeed` instantiate. THE INPUT IS RAW
//                     `w`, NOT 1/w: `proj_out_aw_o` is [30:0] fx16 raw w and
//                     the reciprocal is the converter's own. `proj_out_ad_o`
//                     is the 1/w lane and is deliberately NOT used here.
//   slot 1  u/w       `zhao_geom_overw_sat`, SHADELADDER's block:
//   slot 2  v/w       `rescale_s(uv * invw24, 16)` with the saturation
//                     `spec/qformats.md:75` mandates in its bounds column.
//                     Terrain's coordinate is s32 Q16.16 in TILE units, so
//                     unlike the mesh path this multiply CAN leave the S8.24
//                     rail -- at 128 tiles from the origin -- and `sat_o` is
//                     counted on `uv_sat_o` rather than discarded.
//   slot 3  lit r     `zhao_terrain_shademod`, SHADELADDER's other block:
//   slot 4  lit g     `mod_of(shade, tint, sheet)`, bit-exact, FIVE rungs.
//   slot 5  lit b     See THE COLOUR below -- this is the part to read.
//   slot 6  alpha     owner ruling R48's named constant, opaque.
//
// ---------------------------------------------------------------------------
// THE COLOUR, AND WHY THIS IS NOT THE FLAT STAND-IN I13 REFUSED THREE TIMES
// ---------------------------------------------------------------------------
// This is the paragraph that matters, because the shape it is closest to is a
// shape the entry, the owner's directive and this packet's brief all forbid.
//
// WHAT IS FORBIDDEN: broadcasting `terr_light_base_o` -- one scalar -- into
// slots 3..5 as a flat colour. That would REMOVE GOURAUD, which the owner
// vacation directive of 2026-09-23 prohibits by name, and `terrain_rules` 6.5
// makes layer-H tint a PER-VERTEX quantity ("tint moved to vertices"). The
// oracle's own per-cell tint calls itself "the FLAT STAND-IN for the Gouraud
// tint", and composing a stand-in that names itself one would ratify it.
//
// WHAT THIS BLOCK DOES INSTEAD, and the difference is structural rather than
// cosmetic:
//
//   1. The value is computed THROUGH THE RATIFIED LAW. `mod_of` quantises the
//      shade to the five-rung palette ladder BEFORE modulating. The composed
//      path today has no ladder at all, and SHADELADDER measured what that
//      costs: the law's pixel differs from the composed path's on 599 of 1,248
//      operand triples, worst 32 LSB of 255. A flat broadcast would have
//      shipped that divergence as well as the flatness.
//
//   2. The tint is a PER-CORNER INPUT PORT -- `c*_tint_r/g/b_i`, nine ports --
//      not a constant folded inside. Layer H is UNAUTHORED, so the composer
//      drives all nine from `TERR_TINT_IDENTITY`. That value is not a
//      plausible number chosen here: RGB565 0xFFFF is layer H's RATIFIED
//      ABSENT IDENTITY, which `cell_tint` already defaults to and which is
//      exactly 65536 in Q16.16. The entry says so in terms -- "a unity tint is
//      NOT the stand-in this entry refused three times; it is an unauthored
//      layer sitting at its exact identity."
//
//   3. So the three corners carry EQUAL values today, and they do so because
//      the only per-vertex term in the law is unauthored -- NOT because this
//      block averaged, picked a corner or broadcast a scalar. The Gouraud
//      machinery is fully live underneath: `zhao_geom_attrpack`'s lanes 3..5,
//      six `zhao_raster_attrgrad_v2` lanes and the tile pipe's per-fragment
//      `vertex_rgb` build all run on these slots. When layer H is authored the
//      change is DATA at nine ports; no RTL here moves.
//
// THE SHEET IS DELIBERATELY NOT APPLIED HERE. `mod_of`'s third factor is the
// surface sheet, and the composed console already applies it PER FRAGMENT in
// `zhao_texture_sheetmod`, which TERRAINAUX composed on 2026-09-25 and proved
// with a pixel. Applying it again at the vertex would be a second home for one
// law and would double-modulate every terrain fragment. So `TERR_SHEET_IDENTITY`
// is unity and the composer says why at the port.
//
// WHAT IS STILL DIVERGENT, STATED RATHER THAN HIDDEN: this block puts the
// LAW's modulation into the slots, but everything downstream of the slots is
// the composed path's arithmetic -- `zhao_raster_tile_pipe_v2`'s `lit_unit8`
// and `zhao_raster_fragment`'s 8-bit `unit_mul`. The ladder is now present
// where it was absent; the two narrow roundings are still two where the law
// has one. That residual is NOT closed here, it is not this block's to close,
// and quoting "the ladder is composed" as "terrain is capture-exact" would be
// exactly the component-check-passing error CLAUDE.md's art law names.
//
// ---------------------------------------------------------------------------
// THE CLAMP THE LIGHT LANE DOES NOT DO, AND WHY IT IS HERE
// ---------------------------------------------------------------------------
// `zhao_terrain_shade`'s `base_o` is the UNCLAMPED shade -- its own port
// comment says "SIGN PRESERVED", and a unit sun keeps |base| <= ~0x10005. The
// oracle's clamp is one hop later, in `shade_flat_tri`
// (`reference/src/zrender/terrain.cpp:137`):
//
//     shade < 0 ? 0 : (shade > 0x10000 ? 0x10000 : shade)
//
// so the consumer owns it, and this block is the consumer. `clamp01_c` is that
// line transcribed. `zhao_terrain_shademod`'s `shade_domain_o` exists for a
// caller that forgot; because this block clamps, that counter reads zero here
// -- AND A ZERO IS A CLAIM, so the directed test fires it directly at the
// shademod's own port rather than letting the silence stand as evidence.
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK DECLARES AT THE DOOR
// ---------------------------------------------------------------------------
//   untex            0. Terrain IS textured -- that is the entire purpose of
//                    the u/v law this block multiplies. R197's gate in the
//                    composer is `untex && (sample_count != 0)`, so a textured
//                    declaration is never refused there.
//   material_mode    DERIVED FROM `mat_set_i`, not a constant. UPDATED
//                    2026-09-26 (TERRAINMAT) -- the sentence this block used
//                    to carry here said MATMODE_NONE "is the ONLY mode
//                    terrain can lawfully present", on the evidence that
//                    `material_set`/`material_id` return zero hits under
//                    `fpga/rtl/terrain/`. THAT ZERO MEASURED A NAMING
//                    BOUNDARY BETWEEN TWO LANES, NOT A STRUCTURAL ABSENCE
//                    (console entry I13's PATCHV2 paragraph, four independent
//                    counts, including ruling R13 REFUSING a subpatch-uniform
//                    material -- a refusal is evidence the thing exists to be
//                    refused). Terrain now presents an identity the host
//                    authored, and this block derives the mode from it:
//
//                      mat_set_i != 0                 -> MATMODE_BACKED, and
//                                                        the {set, id} pair
//                                                        goes to the door
//                      mat_set_i == 0, mat_id_i == 0  -> MATMODE_NONE, the
//                                                        zero pair the ruling
//                                                        REQUIRES; this is
//                                                        every pre-2026-09-26
//                                                        capture's behaviour,
//                                                        unchanged
//                      mat_set_i == 0, mat_id_i != 0  -> ILLEGAL. Counted on
//                                                        `mat_id_orphan_o`
//                                                        and declared as the
//                                                        zero pair. See THE
//                                                        ORPHAN RULE.
//
//                    ZERO KEEPS ITS MEANING, which is the whole of why this
//                    was affordable: the default is the old behaviour.
//   behind           the projector's own, per corner.
//   cull_mode        `TERR_CULL_MODE`, CULL_NONE by default. Terrain emits TOP
//                    and UNDERSIDE surfaces which face opposite ways, so a
//                    single winding rule cannot be right for both; the knob is
//                    named rather than assumed.
//   vertex_alpha     `TERR_VERTEX_ALPHA`, opaque, owner ruling R48.
//   frag_state       `TERR_FRAG_STATE`, the frame default, a named knob.
//   quality_tier     0.
//
// ---------------------------------------------------------------------------
// THE ORPHAN RULE, AND WHY IT IS NOT A SILENT REINTERPRETATION
// ---------------------------------------------------------------------------
// `zhao_material_window`'s `mode_contra_c` REFUSES a non-zero {set, id} under
// MATMODE_NONE -- it is a FAULT there, counted on `mode_refused_o`, and a
// refused triangle does not reach GEOM.CLIP. So a host that writes a
// `terrain_material_id` while leaving `terrain_material_set` zero must not be
// handed to the door as it stands: that would spend a real fault on a
// malformed environment and drop every terrain triangle in the frame.
//
// This block therefore declares the ZERO PAIR for that case -- the lawful
// no-material profile, which is what a zero SET means -- and COUNTS the
// discarded id on `mat_id_orphan_o`. The owner's vacation directive requires
// that an unresolved identity be "diagnosed and handled by the declared
// failure/fallback policy, never silently truncated or made token 0"; the
// counter is the diagnosis and this paragraph is the declaration. Nothing is
// narrowed: a non-zero SET carries its full 32 bits and its id's full 16 to
// the door, whatever their values.
//
// THAT COUNTER CAN FIRE ON LEGAL STIMULUS -- offer `{set = 0, id != 0}` --
// so it owes no committed mutant, and the directed test fires it by exact
// amount rather than asserting its silence.
//
// ---------------------------------------------------------------------------
// THE IDENTITY IS LATCHED ON THE TRIANGLE'S OWN BEAT
// ---------------------------------------------------------------------------
// `mat_set_i` and `mat_id_i` are a FRAME-scoped input that the composer drives
// from CMD.EXEC's SetEnvironment shadow. They are sampled into `mset_q`/
// `mid_q` by the SAME enable that latches the triangle's corners and
// `src_id_q`, and the door reads only the latched copies.
//
// This is not defensiveness about a wire that "cannot" move. It is CLAUDE.md's
// metadata-swap chapter applied before the fact: a combinational path from a
// frame-global register to a per-primitive output port is exactly the shape
// where a late environment update repaints a triangle that was accepted under
// the previous one, and the fault would be invisible -- every counter in the
// arm would balance, because no counter looks at the field that moved. One
// enable, one record, no second opinion.
//
// Conservative SystemVerilog subset only; Quartus 17.0 syntax rules apply
// (elaboration checks inside `initial begin ... end`, explicit generate, no
// inline `for (genvar ...)`, loop variables declared inside their block).
`default_nettype none

module zhao_terrain_clipfeed #(
    parameter int unsigned ATTRS      = 7,
    parameter int unsigned IDW        = 16,
    parameter int unsigned SLOT_INVW  = 0,
    parameter int unsigned SLOT_UOW   = 1,
    parameter int unsigned SLOT_VOW   = 2,
    parameter int unsigned SLOT_R     = 3,
    parameter int unsigned SLOT_G     = 4,
    parameter int unsigned SLOT_B     = 5,
    parameter int unsigned SLOT_ALPHA = 6,
    // The shared depthquant pool and its reciprocal, sized as every other
    // client in this console sizes them.
    parameter int unsigned DQ_SLOTS   = 16,
    parameter int unsigned RCP_NCTX   = 8,
    // Owner ruling R48: no ratified vertex format carries alpha, so OPAQUE is
    // a named editable constant and nothing is stubbed.
    parameter int signed   TERR_ALPHA        = 32'sd65536,
    parameter logic [ 7:0] TERR_VERTEX_ALPHA = 8'hFF,
    // CULL_NONE. See the header: tops and undersides face opposite ways.
    parameter logic [ 1:0] TERR_CULL_MODE    = 2'd0,
    // The frame default raster state, the same knob the other three door
    // clients carry.
    parameter logic [31:0] TERR_FRAG_STATE   = 32'd0,
    // See `o_detail_o`. 1 = terrain's primitives are heightfield surfaces and
    // may take TERRAIN.NORMALMAP's detail term; 0 withdraws the class from the
    // feature without touching the relief strength, and vice versa.
    parameter logic        TERR_DETAIL_ELIGIBLE = 1'b1
) (
    input var logic clk,
    input var logic rst_n,

    // ---- the TRIANGLE stream (`proj_out_*`) --------------------------------
    input  var logic               t_valid_i,
    output var logic               t_ready_o,
    input  var logic signed [20:0] t_ax_i,
    input  var logic signed [20:0] t_ay_i,
    input  var logic signed [20:0] t_bx_i,
    input  var logic signed [20:0] t_by_i,
    input  var logic signed [20:0] t_cx_i,
    input  var logic signed [20:0] t_cy_i,
    input  var logic        [ 2:0] t_behind_i,
    input  var logic [IDW-1:0]     t_src_id_i,
    // Raw `w`, fx16, [30:0] -- NOT 1/w. See slot 0 in the header.
    input  var logic        [30:0] t_aw_i,
    input  var logic        [30:0] t_bw_i,
    input  var logic        [30:0] t_cw_i,
    // The depth profile the result was projected under, per triangle.
    input  var logic        [ 1:0] t_profile_i,

    // ---- the LIGHT stream (`terr_light_*`) ---------------------------------
    input  var logic               l_valid_i,
    output var logic               l_ready_o,
    // The flat shade, Q16.16, UNCLAMPED and sign-preserved. See THE CLAMP.
    input  var logic signed [31:0] l_shade_i,
    input  var logic               l_degenerate_i,
    input  var logic [IDW-1:0]     l_src_id_i,

    // ---- the COORDINATE stream (`terr_uv_*`) -------------------------------
    input  var logic               u_valid_i,
    output var logic               u_ready_o,
    input  var logic signed [31:0] u_au_i,
    input  var logic signed [31:0] u_av_i,
    input  var logic signed [31:0] u_bu_i,
    input  var logic signed [31:0] u_bv_i,
    input  var logic signed [31:0] u_cu_i,
    input  var logic signed [31:0] u_cv_i,
    input  var logic [IDW-1:0]     u_src_id_i,

    // ---- LAYER H's PER-CORNER TINT, Q16.16 ---------------------------------
    // Nine ports because the law has nine operands: three corners x three
    // channels. They are PORTS and not constants so that authoring layer H is
    // a data change and no RTL here moves -- CLAUDE.md's "never remove the
    // owner's control in the name of fidelity". The composer drives all nine
    // from layer H's ratified ABSENT IDENTITY today and says so at the
    // instantiation.
    input  var logic        [16:0] a_tint_r_i,
    input  var logic        [16:0] a_tint_g_i,
    input  var logic        [16:0] a_tint_b_i,
    input  var logic        [16:0] b_tint_r_i,
    input  var logic        [16:0] b_tint_g_i,
    input  var logic        [16:0] b_tint_b_i,
    input  var logic        [16:0] c_tint_r_i,
    input  var logic        [16:0] c_tint_g_i,
    input  var logic        [16:0] c_tint_b_i,
    // The surface sheet's factor. Unity here on purpose -- the composed
    // console applies the sheet PER FRAGMENT in `zhao_texture_sheetmod`. See
    // THE SHEET IS DELIBERATELY NOT APPLIED HERE.
    input  var logic        [16:0] sheet_i,

    // ---- TERRAIN'S MATERIAL IDENTITY (TERRAINMAT, 2026-09-26) --------------
    // The host's `SetEnvironment.terrain_material_set` / `.terrain_material_id`,
    // decoded by CMD.EXEC and forked to this arm by the composer. TWO ports and
    // not three: the MODE is DERIVED here rather than presented, because the
    // composer is forbidden to choose it (entry I13, "a material-mode
    // declaration coming from a TERRAIN PORT rather than a constant chosen
    // here") and because a third `mode` port would let a caller present a
    // combination this block's own law refuses. The derivation is above.
    input  var logic        [31:0] mat_set_i,
    input  var logic        [15:0] mat_id_i,

    // ---- one GEOM.CLIPDOOR client ------------------------------------------
    output var logic                 o_valid_o,
    input  var logic                 o_ready_i,
    output var logic signed [20:0]   o_ax_o,
    output var logic signed [20:0]   o_ay_o,
    output var logic signed [20:0]   o_bx_o,
    output var logic signed [20:0]   o_by_o,
    output var logic signed [20:0]   o_cx_o,
    output var logic signed [20:0]   o_cy_o,
    output var logic [2:0]           o_behind_o,
    output var logic [IDW-1:0]       o_src_id_o,
    output var logic                 o_untex_o,
    // TERRAIN.NORMALMAP's DETAIL DECLARATION (NORMALMAP, 2026-09-26).
    //
    // THIS BLOCK IS ENTITLED TO MAKE IT AND NOTHING DOWNSTREAM IS. The detail
    // normal perturbs a surface normal in world XZ with no tangent frame, and
    // `zref_terrain_normalmap.hpp` says why that is legitimate: "a heightfield's
    // tangent frame is axis-aligned in world space". Every triangle this block
    // emits is a terrain heightfield surface; no mesh, particle or forge
    // primitive is. So the declaration is a fact about THIS PRODUCER'S output,
    // which is exactly the kind of statement owner ruling 1 says must come from
    // the producer's own port rather than be chosen at a composer or inferred
    // from a field that happens to be zero.
    //
    // IT IS A PARAMETER AND NOT A LITERAL so the owner keeps the switch. The
    // relief's AMOUNT is a separate knob and lives where it belongs -- in
    // TERRAIN.NORMALMAP's own `strength` config register, whose zero is
    // bit-exact off by that block's contract. Two knobs, two questions: this
    // one says "these primitives can take detail", that one says "how much".
    output var logic                 o_detail_o,
    output var logic [1:0]           o_cull_mode_o,
    output var logic [ATTRS*32-1:0]  o_attr_a_o,
    output var logic [ATTRS*32-1:0]  o_attr_b_o,
    output var logic [ATTRS*32-1:0]  o_attr_c_o,
    output var logic [31:0]          o_material_set_o,
    output var logic [15:0]          o_material_id_o,
    output var logic [1:0]           o_material_mode_o,
    output var logic [7:0]           o_vertex_alpha_o,
    output var logic [31:0]          o_frag_state_o,
    output var logic [7:0]           o_quality_tier_o,

    // ---- evidence ----------------------------------------------------------
    // Triangles ACCEPTED at the three-way join.
    output var logic [31:0] triangles_o,
    // Triangles HANDED TO THE DOOR. The pair discriminates: a block that
    // accepted and never emitted shows the first climbing with the second
    // pinned, which neither a stall nor a healthy run looks like.
    output var logic [31:0] emitted_o,
    // The three streams disagreed about which triangle this is. See THE
    // THREE-WAY JOIN for why this comparison is not blind.
    output var logic [31:0] src_id_mismatch_o,
    // `zhao_geom_overw_sat`'s saturate ENGAGED on a corner -- a patch more
    // than 128 tiles from the origin. Counted per multiply, so up to six per
    // triangle. The block that produces the flag holds no state and says the
    // composer must count it; this is that count.
    output var logic [31:0] uv_sat_o,
    // Triangles whose shade left the law's [0, 65536] domain and was clamped
    // by `clamp01_c`. NOT a fault: `zhao_terrain_shade` emits an unclamped
    // value by design and the oracle clamps at the same place this does.
    output var logic [31:0] shade_clamped_o,
    // Degenerate triangles admitted. `zhao_terrain_lightlane` shades a
    // degenerate triangle to zero by the ratified law, so these are drawn
    // black rather than dropped -- counted so the fixture regression
    // TERRAINAUX repaired is visible from this end too.
    output var logic [31:0] degenerate_o,
    // The converter pair's own two faults, re-exported so the composer can
    // name them.
    output var logic [31:0] dq_refused_o,
    output var logic [31:0] dq_stray_o,
    // Triangles EMITTED declaring MATMODE_BACKED -- the census that says the
    // identity actually traversed rather than merely elaborated. Counted at
    // the door's grant, not at acceptance, so it cannot run ahead of what the
    // window will see. It is a census; `mat_id_orphan_o` beside it is a fault.
    // Both are fired by stimulus in the directed test, by exact amount.
    output var logic [31:0] mat_backed_o,
    output var logic [31:0] mat_id_orphan_o
);

  // --------------------------------------------------------------------------
  // ELABORATION GUARDS
  // --------------------------------------------------------------------------
  // Quartus 17.0 needs these inside `initial begin ... end`; a bare
  // module-scope `if` is a syntax error there and Verilator's lint does not
  // say so. CLAUDE.md records both halves of that trap.
  // synthesis translate_off
  initial begin
    if (ATTRS < 7)
      $fatal(1, "zhao_terrain_clipfeed: ATTRS must be at least 7");
    if (SLOT_INVW >= ATTRS || SLOT_UOW >= ATTRS || SLOT_VOW >= ATTRS ||
        SLOT_R >= ATTRS || SLOT_G >= ATTRS || SLOT_B >= ATTRS ||
        SLOT_ALPHA >= ATTRS)
      $fatal(1, "zhao_terrain_clipfeed: a slot index is outside the packet");
    if (DQ_SLOTS < 3)
      $fatal(1, "zhao_terrain_clipfeed: DQ_SLOTS must hold three corners");
  end
  // synthesis translate_on

  // `zhao_material_window`'s own encoding, transcribed rather than imported so
  // this leaf does not depend on a texture package. Both arms are reachable --
  // see WHAT THIS BLOCK DECLARES for the derivation.
  localparam logic [1:0] MATMODE_BACKED_C = 2'd0;
  localparam logic [1:0] MATMODE_NONE_C   = 2'd1;

  // --------------------------------------------------------------------------
  // THE THREE-WAY JOIN
  // --------------------------------------------------------------------------
  // The identity this triangle was accepted under, and the combinational
  // legality filter that feeds it.
  logic [31:0] mset_q;
  logic [15:0] mid_q;
  // An id with no set is discarded rather than presented: `mode_contra_c`
  // would make it a FAULT and drop the frame's terrain. THE ORPHAN RULE.
  wire         mat_orphan_c = (mat_set_i == 32'd0) && (mat_id_i != 16'd0);
  wire [31:0]  mat_set_c    = mat_set_i;
  wire [15:0]  mat_id_c     = mat_orphan_c ? 16'd0 : mat_id_i;

  localparam logic [3:0] S_IDLE  = 4'd0,
                         S_ISSUE = 4'd1,
                         S_LAND  = 4'd2,
                         S_UV    = 4'd3,
                         S_SHADE = 4'd4,
                         S_EMIT  = 4'd5;

  logic [3:0] st_q;

  // A beat exists only when all three producers offer. Consuming fewer than
  // three would put this triangle's corners with the next triangle's shade,
  // which is the metadata-swap shape CLAUDE.md has a chapter about.
  wire join_c = t_valid_i && l_valid_i && u_valid_i;
  wire take_c = (st_q == S_IDLE) && join_c;

  assign t_ready_o = take_c;
  assign l_ready_o = take_c;
  assign u_ready_o = take_c;

  // --------------------------------------------------------------------------
  // THE LATCHED TRIANGLE
  // --------------------------------------------------------------------------
  logic signed [20:0] ax_q, ay_q, bx_q, by_q, cx_q, cy_q;
  logic        [ 2:0] behind_q;
  logic [IDW-1:0]     src_id_q;
  logic        [ 1:0] profile_q;
  logic signed [31:0] uv_q [6];      // au, av, bu, bv, cu, cv
  logic        [16:0] shade_q;       // clamped, Q16.16

  // --------------------------------------------------------------------------
  // THE CLAMP `zhao_terrain_shade` LEAVES TO ITS CONSUMER
  // --------------------------------------------------------------------------
  // `reference/src/zrender/terrain.cpp:137`, transcribed:
  //     shade < 0 ? 0 : (shade > 0x10000 ? 0x10000 : shade)
  logic [16:0] clamp01_c;
  logic        clamp_engaged_c;
  always_comb begin
    if (l_shade_i < 32'sd0) begin
      clamp01_c       = 17'd0;
      clamp_engaged_c = 1'b1;
    end else if (l_shade_i > 32'sd65536) begin
      clamp01_c       = 17'd65536;
      clamp_engaged_c = 1'b1;
    end else begin
      clamp01_c       = l_shade_i[16:0];
      clamp_engaged_c = 1'b0;
    end
  end

  // --------------------------------------------------------------------------
  // THE DEPTH CONVERTER -- one law, a fourth instance of it
  // --------------------------------------------------------------------------
  // `zhao_geom_vattr`, `zhao_forge_assemble` and `zhao_part_clipfeed` already
  // hold one each. There is no arithmetic in this file for it: no shift, no
  // rounding, no saturation. The tag is the CORNER INDEX, 0..2, and only one
  // triangle is ever in flight, so a completion-order answer still lands in
  // the right corner.
  logic        dq_v_valid_c;
  logic        dq_v_ready;
  logic [ 1:0] dq_issue_q;       // which corner is being offered, 0..2
  logic [ 1:0] dq_landed_q;      // how many answers have landed, 0..3

  // The corner's `w` is taken from the LATCHED triangle, not the live port:
  // the ports have already been handshaken away by the time S_ISSUE runs.
  logic [30:0] w_q [3];
  logic [30:0] w_issue_c;
  always_comb begin
    case (dq_issue_q)
      2'd0:    w_issue_c = w_q[0];
      2'd1:    w_issue_c = w_q[1];
      default: w_issue_c = w_q[2];
    endcase
  end

  assign dq_v_valid_c = (st_q == S_ISSUE);

  logic        dq_d_valid;
  logic [23:0] dq_invw;
  // Only the low two bits are a corner index; the converter's tag is 16 bits
  // because it is shared with clients that pool many more contexts. The waiver
  // is scoped to this signal with its reason rather than set on the file.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [15:0] dq_tag;
  /* verilator lint_on UNUSEDSIGNAL */
  logic [23:0] inv_q [3];

  // The converter's census. Only the two FAULTS reach this block's ports; the
  // rest are named, lint-waived and left unread rather than given ports
  // nobody differences -- the same choice `zhao_forge_assemble` makes.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [31:0] dq_vertices, dq_near, dq_far, dq_sat;
  logic        dq_idle;
  logic [31:0] rcp_accepted, rcp_completed, rcp_mul_jobs, rcp_zero_jobs;
  logic [31:0] rcp_phase_jobs, rcp_negcorr_jobs;
  logic [ 5:0] rcp_occupancy;
  logic        rcp_qerr, rcp_idle, rcp_d_zero;
  /* verilator lint_on UNUSEDSIGNAL */

  logic        rcp_v_valid, rcp_v_ready, rcp_r_valid, rcp_r_ready;
  logic [23:0] rcp_d, rcp_r;
  logic [ 7:0] rcp_v_tok, rcp_r_tok;
  logic [ 5:0] rcp_k;

  zhao_geom_depthquant_stream #(
      .TAGW (16),
      .NSLOT(DQ_SLOTS)
  ) u_dq (
      .clk           (clk),
      .rst_n         (rst_n),
      .v_valid_i     (dq_v_valid_c),
      .v_ready_o     (dq_v_ready),
      // Widened, not converted: the projector's guarded w is 31 bits and the
      // converter's port is 40, so the value sits in the low bits and the rest
      // are zero. `zhao_geom_vattr` does the same with the same quantity.
      .v_w_i         ({9'd0, w_issue_c}),
      .v_profile_i   (profile_q),
      .v_tag_i       ({14'd0, dq_issue_q}),
      .d_valid_o     (dq_d_valid),
      // ALWAYS READY, and it is not a shortcut: the landing is a write into
      // `inv_q` at the tag's address, which no other writer contends for and
      // which cannot stall.
      .d_ready_i     (1'b1),
      .d_invw24_o    (dq_invw),
      .d_tag_o       (dq_tag),
      .rcp_valid_o   (rcp_v_valid),
      .rcp_ready_i   (rcp_v_ready),
      .rcp_d_o       (rcp_d),
      .rcp_tok_o     (rcp_v_tok),
      .rcp_rvalid_i  (rcp_r_valid),
      .rcp_rready_o  (rcp_r_ready),
      .rcp_r_i       (rcp_r),
      .rcp_k_i       (rcp_k),
      .rcp_tok_i     (rcp_r_tok),
      .vertices_o    (dq_vertices),
      .clamped_near_o(dq_near),
      .clamped_far_o (dq_far),
      .saturated_o   (dq_sat),
      .refused_o     (dq_refused_o),
      .tok_stray_o   (dq_stray_o),
      .idle_o        (dq_idle)
  );

  zhao_raster_rcp24_v4 #(
      .NCTX(RCP_NCTX),
      .TOKW(8)
  ) u_rcp (
      .clk           (clk),
      .rst_n         (rst_n),
      .v_valid_i     (rcp_v_valid),
      .v_ready_o     (rcp_v_ready),
      .d_i           (rcp_d),
      .v_tok_i       (rcp_v_tok),
      .r_valid_o     (rcp_r_valid),
      .r_ready_i     (rcp_r_ready),
      .r_o           (rcp_r),
      .k_o           (rcp_k),
      .d_zero_o      (rcp_d_zero),
      .r_tok_o       (rcp_r_tok),
      .accepted_o    (rcp_accepted),
      .completed_o   (rcp_completed),
      .mul_jobs_o    (rcp_mul_jobs),
      .zero_jobs_o   (rcp_zero_jobs),
      .phase_jobs_o  (rcp_phase_jobs),
      .negcorr_jobs_o(rcp_negcorr_jobs),
      .occupancy_o   (rcp_occupancy),
      .qerr_o        (rcp_qerr),
      .idle_o        (rcp_idle)
  );

  // --------------------------------------------------------------------------
  // THE PERSPECTIVE MULTIPLY -- ONE instance, six steps
  // --------------------------------------------------------------------------
  // `zhao_geom_overw_sat` is combinational and holds no state, so ONE of them
  // walked six times costs one 32x24 multiplier instead of six. That block's
  // header names the fit question it leaves open -- "does the wide former
  // close on ALMs at terrain's vertex rate" -- and sharing one instance is the
  // answer that keeps the question small.
  logic [ 2:0] uv_step_q;           // 0..5
  logic signed [31:0] ow_q [6];
  logic signed [31:0] ow_uv_c;
  logic        [23:0] ow_invw_c;
  logic signed [31:0] ow_out_c;
  logic               ow_sat_c;

  always_comb begin
    ow_uv_c = uv_q[uv_step_q];
    // Steps 0,1 are corner A (u then v); 2,3 corner B; 4,5 corner C.
    case (uv_step_q[2:1])
      2'd0:    ow_invw_c = inv_q[0];
      2'd1:    ow_invw_c = inv_q[1];
      default: ow_invw_c = inv_q[2];
    endcase
  end

  zhao_geom_overw_sat u_overw (
      .uv_i     (ow_uv_c),
      .invw24_i (ow_invw_c),
      .over_w_o (ow_out_c),
      .sat_o    (ow_sat_c)
  );

  // --------------------------------------------------------------------------
  // THE MODULATION -- `zhao_terrain_shademod`, NINE operand triples
  // --------------------------------------------------------------------------
  // The law has NINE operands: three corners x three channels. This block
  // walks all nine, one per request, through ONE shademod instance.
  //
  // AN EARLIER DRAFT OF THIS FILE WALKED THREE AND COPIED CORNER A's ANSWER TO
  // B AND C, and that draft was WRONG IN THE EXACT WAY THIS ENTRY FORBIDS. The
  // reasoning that produced it was "all nine agree today because layer H is
  // unauthored, so computing one corner is the same answer for less silicon".
  // The answer is the same; THE STRUCTURE IS NOT. A block that computes one
  // corner and copies it is a FLAT BROADCAST, and authoring layer H would then
  // require new RTL here rather than new data -- which is precisely the
  // "Gouraud law silently implemented as a constant" this entry has refused
  // three times. It was caught by Verilator's UNUSEDSIGNAL on the six unread
  // tint ports, and it is worth noting WHICH instrument caught it: had this
  // file sat under the blanket directory waiver
  // `tests/shell/v3_closure_inherited.vlt` applies to `fpga/rtl/texture`,
  // `raster`, `geometry`, `common` and `video`, nothing would have said a word.
  //
  // So the walk is nine and the per-vertex path is live. Today the nine
  // answers happen to agree, because every tint port carries layer H's
  // RATIFIED ABSENT IDENTITY -- and that is a statement about the DATA, which
  // is what it should be.
  //
  // THE COST IS NAMED RATHER THAN HIDDEN: nine requests x 17 clocks is 153
  // clocks of modulation per triangle. Three instances (one per channel) would
  // cut it to 51 for three times the ALM, and the shademod is 0 DSP shift-add,
  // so the trade is pure ALM-for-clocks. ONE instance is chosen because ALM is
  // this device's binding constraint at 97% of the ceiling and clocks are not.
  // If a fit or a frame budget says otherwise, the repair is a parameter and
  // three instances, not a change to the arithmetic.
  logic [ 3:0] mstep_q;             // 0..8: corner = step/3, channel = step%3
  logic [19:0] mod_q [9];
  logic [16:0] tint_sel_c;
  logic        sm_req_valid_c, sm_req_ready, sm_rsp_valid, sm_rsp_ready_c;
  logic [19:0] sm_mod;
  /* verilator lint_off UNUSEDSIGNAL */
  logic [15:0] sm_issued, sm_shade_domain;
  /* verilator lint_on UNUSEDSIGNAL */

  always_comb begin
    case (mstep_q)
      4'd0:    tint_sel_c = a_tint_r_i;
      4'd1:    tint_sel_c = a_tint_g_i;
      4'd2:    tint_sel_c = a_tint_b_i;
      4'd3:    tint_sel_c = b_tint_r_i;
      4'd4:    tint_sel_c = b_tint_g_i;
      4'd5:    tint_sel_c = b_tint_b_i;
      4'd6:    tint_sel_c = c_tint_r_i;
      4'd7:    tint_sel_c = c_tint_g_i;
      default: tint_sel_c = c_tint_b_i;
    endcase
  end

  assign sm_req_valid_c = (st_q == S_SHADE) && !sm_rsp_valid;
  assign sm_rsp_ready_c = (st_q == S_SHADE);

  zhao_terrain_shademod u_shademod (
      .clk            (clk),
      .rst_n          (rst_n),
      .req_valid_i    (sm_req_valid_c),
      .req_ready_o    (sm_req_ready),
      .shade_i        (shade_q),
      .tint_i         (tint_sel_c),
      .sheet_i        (sheet_i),
      .rsp_valid_o    (sm_rsp_valid),
      .rsp_ready_i    (sm_rsp_ready_c),
      .mod_o          (sm_mod),
      .issued_o       (sm_issued),
      .shade_domain_o (sm_shade_domain)
  );

  // --------------------------------------------------------------------------
  // THE PACKET
  // --------------------------------------------------------------------------
  // `zhao_raster_tile_pipe_v2`'s `lit_unit8(v)` is `v[15:8]`, so a lane's
  // value is a Q0.16 scale whose TOP BYTE is the unit8 the fragment
  // multiplies by. `zhao_part_clipfeed`'s `lit_of_byte` is the same encoding
  // seen from the byte end.
  //
  // `mod_o` is Q16.16 over [0, 65536], so unity is 0x10000 and its [15:8] is
  // ZERO -- the one conversion that must not be done by dropping bits. The
  // clamp below maps the closed top of the law's range onto the closed top of
  // the lane's: 65536 -> 65535 -> byte 255 -> white, and 0 -> 0 -> black,
  // which is rung zero reaching the fragment as the law says it must.
  function automatic logic [31:0] lit_of_mod(input logic [19:0] m);
    logic [15:0] s;
    begin
      s = (m >= 20'd65536) ? 16'hFFFF : m[15:0];
      lit_of_mod = {16'd0, s};
    end
  endfunction

  function automatic logic [ATTRS*32-1:0] pack_attr(input logic [23:0] invw,
                                                    input logic signed [31:0] uow,
                                                    input logic signed [31:0] vow,
                                                    input logic [19:0] mr,
                                                    input logic [19:0] mg,
                                                    input logic [19:0] mb);
    logic [ATTRS*32-1:0] w;
    begin
      w = '0;
      w[32*SLOT_INVW  +: 32] = {8'd0, invw};
      w[32*SLOT_UOW   +: 32] = uow;
      w[32*SLOT_VOW   +: 32] = vow;
      w[32*SLOT_R     +: 32] = lit_of_mod(mr);
      w[32*SLOT_G     +: 32] = lit_of_mod(mg);
      w[32*SLOT_B     +: 32] = lit_of_mod(mb);
      w[32*SLOT_ALPHA +: 32] = 32'(TERR_ALPHA);
      pack_attr = w;
    end
  endfunction

  assign o_valid_o        = (st_q == S_EMIT);
  assign o_ax_o           = ax_q;
  assign o_ay_o           = ay_q;
  assign o_bx_o           = bx_q;
  assign o_by_o           = by_q;
  assign o_cx_o           = cx_q;
  assign o_cy_o           = cy_q;
  assign o_behind_o       = behind_q;
  assign o_src_id_o       = src_id_q;
  // Terrain IS textured. See WHAT THIS BLOCK DECLARES.
  assign o_untex_o        = 1'b0;
  assign o_detail_o       = TERR_DETAIL_ELIGIBLE;
  assign o_cull_mode_o    = TERR_CULL_MODE;
  assign o_attr_a_o       = pack_attr(inv_q[0], ow_q[0], ow_q[1],
                                      mod_q[0], mod_q[1], mod_q[2]);
  assign o_attr_b_o       = pack_attr(inv_q[1], ow_q[2], ow_q[3],
                                      mod_q[3], mod_q[4], mod_q[5]);
  assign o_attr_c_o       = pack_attr(inv_q[2], ow_q[4], ow_q[5],
                                      mod_q[6], mod_q[7], mod_q[8]);
  // THE LATCHED IDENTITY, not the live input. See THE IDENTITY IS LATCHED ON
  // THE TRIANGLE'S OWN BEAT. `mset_q` is zero exactly when the span is the
  // lawful no-material one, which is the zero pair `mode_contra_c` requires.
  assign o_material_set_o = mset_q;
  assign o_material_id_o  = mid_q;
  assign o_material_mode_o= (mset_q != 32'd0) ? MATMODE_BACKED_C : MATMODE_NONE_C;
  assign o_vertex_alpha_o = TERR_VERTEX_ALPHA;
  assign o_frag_state_o   = TERR_FRAG_STATE;
  assign o_quality_tier_o = 8'd0;

  // --------------------------------------------------------------------------
  // THE SEQUENCER
  // --------------------------------------------------------------------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      integer i;
      st_q              <= S_IDLE;
      dq_issue_q        <= 2'd0;
      dq_landed_q       <= 2'd0;
      uv_step_q         <= 3'd0;
      mstep_q           <= 4'd0;
      ax_q              <= 21'sd0;
      ay_q              <= 21'sd0;
      bx_q              <= 21'sd0;
      by_q              <= 21'sd0;
      cx_q              <= 21'sd0;
      cy_q              <= 21'sd0;
      behind_q          <= 3'd0;
      src_id_q          <= {IDW{1'b0}};
      mset_q            <= 32'd0;
      mid_q             <= 16'd0;
      mat_backed_o      <= 32'd0;
      mat_id_orphan_o   <= 32'd0;
      profile_q         <= 2'd0;
      shade_q           <= 17'd0;
      triangles_o       <= 32'd0;
      emitted_o         <= 32'd0;
      src_id_mismatch_o <= 32'd0;
      uv_sat_o          <= 32'd0;
      shade_clamped_o   <= 32'd0;
      degenerate_o      <= 32'd0;
      for (i = 0; i < 6; i = i + 1) begin
        uv_q[i] <= 32'sd0;
        ow_q[i] <= 32'sd0;
      end
      for (i = 0; i < 3; i = i + 1) begin
        w_q[i]   <= 31'd0;
        inv_q[i] <= 24'd0;
      end
      for (i = 0; i < 9; i = i + 1) begin
        mod_q[i] <= 20'd0;
      end
    end else begin
      // The converter's answers land at any time; only one triangle is ever in
      // flight, so a tag is a corner index and nothing else can own it.
      if (dq_d_valid) begin
        inv_q[dq_tag[1:0]] <= dq_invw;
        dq_landed_q        <= dq_landed_q + 2'd1;
      end

      case (st_q)
        S_IDLE: begin
          // case0: the three-way join.
          if (take_c) begin
            ax_q      <= t_ax_i;
            ay_q      <= t_ay_i;
            bx_q      <= t_bx_i;
            by_q      <= t_by_i;
            cx_q      <= t_cx_i;
            cy_q      <= t_cy_i;
            behind_q  <= t_behind_i;
            src_id_q  <= t_src_id_i;
            // ONE ENABLE, ONE RECORD: the identity is part of the triangle,
            // captured with its corners and not read live at the door.
            mset_q    <= mat_set_c;
            mid_q     <= mat_id_c;
            if (mat_orphan_c && (mat_id_orphan_o != 32'hffff_ffff))
              mat_id_orphan_o <= mat_id_orphan_o + 32'd1;
            profile_q <= t_profile_i;
            w_q[0]    <= t_aw_i;
            w_q[1]    <= t_bw_i;
            w_q[2]    <= t_cw_i;
            uv_q[0]   <= u_au_i;
            uv_q[1]   <= u_av_i;
            uv_q[2]   <= u_bu_i;
            uv_q[3]   <= u_bv_i;
            uv_q[4]   <= u_cu_i;
            uv_q[5]   <= u_cv_i;
            shade_q   <= clamp01_c;

            triangles_o <= triangles_o + 32'd1;
            if (clamp_engaged_c) shade_clamped_o <= shade_clamped_o + 32'd1;
            if (l_degenerate_i)  degenerate_o    <= degenerate_o + 32'd1;
            // THE JOIN'S OWN CHECK. Three producers, three enables, one
            // comparison -- see the header for why this one can actually fire.
            if ((t_src_id_i != l_src_id_i) || (t_src_id_i != u_src_id_i))
              src_id_mismatch_o <= src_id_mismatch_o + 32'd1;

            dq_issue_q  <= 2'd0;
            dq_landed_q <= 2'd0;
            st_q        <= S_ISSUE;
          end
        end

        S_ISSUE: begin
          // case1: offer the three corners' raw w to the converter.
          if (dq_v_ready) begin
            if (dq_issue_q == 2'd2) st_q <= S_LAND;
            else                    dq_issue_q <= dq_issue_q + 2'd1;
          end
        end

        S_LAND: begin
          // case2: wait for all three answers. `dq_landed_q` is incremented
          // above, outside the case, because a landing is not synchronous with
          // this state's own progress.
          if (dq_landed_q == 2'd3 || (dq_d_valid && dq_landed_q == 2'd2)) begin
            uv_step_q <= 3'd0;
            st_q      <= S_UV;
          end
        end

        S_UV: begin
          // case3: six perspective multiplies through the one shared instance.
          ow_q[uv_step_q] <= ow_out_c;
          if (ow_sat_c) uv_sat_o <= uv_sat_o + 32'd1;
          if (uv_step_q == 3'd5) begin
            mstep_q <= 4'd0;
            st_q    <= S_SHADE;
          end else begin
            uv_step_q <= uv_step_q + 3'd1;
          end
        end

        S_SHADE: begin
          // case4: nine operand triples through the ladder, three corners by
          // three channels. See THE MODULATION for why it is nine and not three.
          if (sm_rsp_valid) begin
            mod_q[mstep_q] <= sm_mod;
            if (mstep_q == 4'd8) st_q <= S_EMIT;
            else                 mstep_q <= mstep_q + 4'd1;
          end
        end

        S_EMIT: begin
          // case5: hold the beat until the door grants it.
          if (o_ready_i) begin
            emitted_o <= emitted_o + 32'd1;
            // The census is taken on the SAME grant as `emitted_o`, off the
            // LATCHED identity, so the two can never describe different
            // triangles.
            if ((mset_q != 32'd0) && (mat_backed_o != 32'hffff_ffff))
              mat_backed_o <= mat_backed_o + 32'd1;
            st_q      <= S_IDLE;
          end
        end

        default: begin
          // case6: unreachable; recover rather than latch.
          st_q <= S_IDLE;
        end
      endcase
    end
  end

  // `dq_v_ready` is read in S_ISSUE only; `sm_req_ready` is not read at all
  // because the shademod's own `req_ready_o` is high exactly when it is idle
  // and this block offers only while waiting for a response it has not had.
  /* verilator lint_off UNUSEDSIGNAL */
  wire _unused_ok = &{1'b0, sm_req_ready, 1'b0};
  /* verilator lint_on UNUSEDSIGNAL */

endmodule

`default_nettype wire
