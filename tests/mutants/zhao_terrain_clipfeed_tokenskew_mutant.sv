// zhao_terrain_clipfeed_tokenskew_mutant.sv -- THE POSITIVE CONTROL for the
// per-triangle material-token equality (`terrain_clipfeed_mat_directed`
// section 7, and `a_attrpack_setup_same_triangle` in the composed console).
//
// ---------------------------------------------------------------------------
// WHY THIS FILE EXISTS
// ---------------------------------------------------------------------------
// I34's carriage (MATCARRY, 2026-09-27) puts a per-triangle material token
// INSIDE the records that already travel from GEOM.CLIP's door to the shell's,
// precisely so it cannot drift from its triangle:
// `reports/DECISION-20260927-I34-MATERIAL-CARRIER.md` chose that over an
// aligned side FIFO because a side queue in lockstep with a pipeline IS the
// `u_geom_tidq` defect class -- permanently one behind, 74 of 75 triangles
// binned under their predecessor's descriptor, EVERY RANGE GUARD PASSING
// because the ids stayed in range and decoded cleanly.
//
// That argument is structural, and a structural argument is exactly the kind
// this repository has learned not to accept on its own. Section 7 asserts the
// token arrives with its own triangle; **no legal stimulus at these ports can
// make that assertion fail**, because the production block latches the token
// by the same enable as the corners. So its silence is a claim, and CLAUDE.md
// is explicit: "a guard you cannot reach with legal stimulus needs a COMMITTED
// MUTANT", and "a detector that has not been shown to FIRE has not been
// tested."
//
// ---------------------------------------------------------------------------
// IT IS A WRAPPER, NOT A COPY
// ---------------------------------------------------------------------------
// Everything below the port list is ONE instantiation of the real
// `zhao_terrain_clipfeed` by `.*`, with a single override. There is no copied
// body to go stale -- which is the failure `tools/budget/mutant_copy_drift.py`
// exists to catch and which that tool deliberately does not charge a wrapper
// with. A port gained or lost in production fails to elaborate here and says
// so, loudly, instead of quietly measuring a block that no longer exists.
//
// THE ONE SUBSTANTIVE CHANGE: the token handed to the production block is the
// PREVIOUS accepted triangle's, not this one's --
//
//     .t_material_token_i (skew_q)      instead of      t_material_token_i
//
// `skew_q` is loaded on the block's own accept beat, so the mutation is
// EXACTLY the tidq fault reproduced in this arm: token[k-1] arrives latched
// against triangle[k]. Nothing else changes. Every corner, every attribute,
// every counter and the `{set, id}` identity are untouched, so a failure
// cannot be blamed on broken stimulus.
//
// NOTE WHAT THE MUTATION DOES *NOT* DO, because it is what makes this a fair
// control: it does not produce an illegal value, an out-of-range index or a
// dropped beat. The skewed token is a perfectly well-formed v1 token with a
// correct tag, and the triangle count, the emitted count and
// `src_id_mismatch_o` all stay exactly as they are in the clean run. That is
// the whole point -- this is the fault class that passes every range check.
//
// ---------------------------------------------------------------------------
// THE DRIVER, AND ITS POLARITY IS INVERTED
// ---------------------------------------------------------------------------
// `tests/mutants/terrain_clipfeed_tokenskew_mutant.cpp`, registered as the
// `terrain_clipfeed_tokenskew_mutant` ctest. It PASSES when the per-triangle
// equality FAILS and fails when every pair matches. It is evidence about the
// INSTRUMENT, not about the design.
//
// The negative control is the same bench compiled WITHOUT
// `ZHAO_MUT_TOKEN_SKEW`: `terrain_clipfeed_mat_directed`, whose section 7 must
// read zero mismatches against unmutated production. CLAUDE.md records why
// that control is owed -- two combiner mutants once passed while measuring
// unmutated production, because a command-line `-D` cannot override a
// FUNCTION-LIKE `define` and said nothing when it failed to. The selector here
// is a plain `ifdef`, which `-D` does reach, and the two builds are shown to
// differ by the fact that one reports 12 mismatches and the other 0.
//
// REGENERATE THE PORT LIST if `zhao_terrain_clipfeed`'s parameter or port
// block changes. It is that block verbatim, with the module renamed; the `.*`
// below then does the rest. `tools/design/wrapper_port_parity.py` is what
// checks this half.
`default_nettype none
module zhao_terrain_clipfeed_tokenskew_mutant #(
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
    // ---- THE LAYER-E MATERIAL TOKEN, PER TRIANGLE (MATCARRY, 2026-09-27) ---
    // The v1 32-bit material token of `zhao_material_token_pkg` -- the
    // {matA, matB, weight} the projector forwards on `proj_out_*`, encoded
    // once at the composer rather than a second time here.
    //
    // OPAQUE. This block does not decode it, does not check its tag and does
    // not act on it, exactly as it does not read inside the attribute packet.
    // It exists here for ONE reason and it is alignment: the composer has the
    // triple on the projector's beat, and this block is strictly SERIAL --
    // "it accepts one triangle, converts its three corners, packs, emits, and
    // only then accepts another". A token read live at the door would
    // therefore be a DIFFERENT triangle's, which is the metadata-swap shape
    // CLAUDE.md has a chapter about. Latched with the corners, it cannot be.
    //
    // This is why entry I34's carriage is FOUR files and not the three
    // reports/DECISION-20260927-I34-MATERIAL-CARRIER.md names: the terrain
    // arm's per-triangle triple is not at the door, it is upstream of a
    // serial converter.
    input  var logic        [31:0] t_material_token_i,

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
    // The token this triangle was accepted with, granted at the door on the
    // triangle's OWN beat. See `t_material_token_i`.
    output var logic [31:0]          o_material_token_o,
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

  // ---- THE MUTATION, and it is these five lines -----------------------------
  // The token the production block sees is the one from the PREVIOUS accepted
  // triangle. Loaded on the block's own accept -- `t_valid_i && t_ready_o` --
  // so the skew is exactly one triangle, never a fraction of a handshake.
  logic [31:0] skew_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) skew_q <= 32'd0;
    else if (t_valid_i && t_ready_o) skew_q <= t_material_token_i;
  end

  zhao_terrain_clipfeed #(
      .ATTRS               (ATTRS),
      .IDW                 (IDW),
      .SLOT_INVW           (SLOT_INVW),
      .SLOT_UOW            (SLOT_UOW),
      .SLOT_VOW            (SLOT_VOW),
      .SLOT_R              (SLOT_R),
      .SLOT_G              (SLOT_G),
      .SLOT_B              (SLOT_B),
      .SLOT_ALPHA          (SLOT_ALPHA),
      .DQ_SLOTS            (DQ_SLOTS),
      .RCP_NCTX            (RCP_NCTX),
      .TERR_ALPHA          (TERR_ALPHA),
      .TERR_VERTEX_ALPHA   (TERR_VERTEX_ALPHA),
      .TERR_CULL_MODE      (TERR_CULL_MODE),
      .TERR_FRAG_STATE     (TERR_FRAG_STATE),
      .TERR_DETAIL_ELIGIBLE(TERR_DETAIL_ELIGIBLE)
  ) u_prod (
      .*,
      // THE OVERRIDE. Everything else binds by name to this wrapper's own
      // identically-named port.
      .t_material_token_i(skew_q)
  );

endmodule : zhao_terrain_clipfeed_tokenskew_mutant

`default_nettype wire
