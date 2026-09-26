// zhao_material_window.sv -- MATERIAL.RESOLVE's REQUEST ISSUE POINT and its
// RESPONSE JOIN.  `zhao_console_core` entry I49, 2026-09-20 (texmat2 packet).
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK IS FOR
// ---------------------------------------------------------------------------
// MATERIAL.RESOLVE was built, tested and COMPOSED on 2026-09-19, with its
// directory (MEM.UPLOAD's 5f.1 publication) and its fetch (requester C of the
// ENGINE1 adapter) both real.  What it did not have was a producer for its
// REQUEST and a consumer for its RESPONSE: entry I49 said, in as many words,
// "a request must be issued per meshlet (or per triangle) and its answer
// joined back to the triangles that asked".  This is that block.
//
// It sits in the triangle stream between GEOM.REPLAY and GEOM.CLIP and does
// three things:
//
//   1. it reads the triangle's OWN {material_set, material_id} -- both carried
//      with the meshlet from GEOM.DRAWJOB through GEOM.ASSETFETCH,
//      GEOM.ASSEMBLE and GEOM.REPLAY, so the two halves are joined BY
//      CONSTRUCTION and not by their timing (the fault entry I39 refuses by
//      name);
//   2. when that pair differs from the one currently PUBLISHED it issues a
//      resolve and holds the triangle until the answer is in hand;
//   3. it publishes the answer's MATERIAL-OWNED fields as the flat request's
//      material half, for every triangle it then lets through.
//
// ---------------------------------------------------------------------------
// WHY THE ANSWER AT THE DOOR BELONGS TO THE TRIANGLE AT THE DOOR
// ---------------------------------------------------------------------------
// This is the whole correctness argument and it is STRUCTURAL, not a latency
// coincidence.  The published material is a single register read combinationally
// by the shell's triangle door, several pipeline stages downstream of here --
// exactly the shape that produced the metadata-swap defect CLAUDE.md has a
// chapter about.  What makes it sound here is the INTERLOCK:
//
//   * the window never changes what it publishes while ANY triangle is between
//     GEOM.CLIP's input and the door.  `d_enter_i`, `d_reject_i` and
//     `d_leave_i` are the three disposal events of that span -- a triangle is
//     accepted by GEOM.CLIP, and is then either retired by GEOM.CLIP with a
//     non-ACCEPT verdict or taken by the door.  There is no fourth outcome, so
//     `occupancy_q` returns to zero and the drain always completes;
//   * and the window holds the triangle that asked for the new material until
//     the drain AND the answer are both done.
//
// So the span downstream of this block is, at every instant, occupied by
// triangles of ONE material, and that material is the published one.  The
// argument does not depend on GEOM.CLIP's latency, on GEOM.SETUP's, on whether
// GEOM.CLIP drops a triangle, or on the order in which it does -- only on the
// fact that every triangle that enters leaves.  `err_unpublished_o` watches the
// one thing that would falsify it.
//
// THE COST IS A DRAIN PER MATERIAL CHANGE, and it is measured rather than
// argued: `drain_stall_cycles_o` and `answer_stall_cycles_o` are counted
// separately, because they have different cures.  A repeat of the SAME
// {set, id} -- which the oracle's own header says is the common case ("the
// same material serves every triangle of a meshlet and usually many
// meshlets") -- costs nothing at all: no drain, no request, no stall.
//
// ---------------------------------------------------------------------------
// WHAT IT PUBLISHES, AND THE THREE FIELDS IT DOES *NOT* OWN
// ---------------------------------------------------------------------------
// `zhao_material_resolve`'s own header is the authority and this block obeys
// it.  The flat request's MATERIAL-OWNED fields are `sample_count`,
// `material_recipe`, `recipe_weight` and `base_binding_selector`, and those
// four come straight off the response.
//
// `palette_slot`, `palette_generation` and `response_class` are the BINDING
// PAGE's, and `zhao_texture_binding_resolver_v2` takes the request's copies as
// WITNESSES and CHECKS them.  Two of the three have a LAW rather than a
// producer and one is an OWNER DECISION:
//
//   * palette_slot / palette_generation.  For a DIRECT format (RGB565,
//     ARGB1555, ARGB4444) `binding_row_legal` REQUIRES
//     `{palette_generation, palette_slot} == 0` -- so publishing zero is the
//     binding page's own law, not an invented value, and the witness is exact.
//
//     FOR A CLUT FORMAT THE PAIR IS NOW PRODUCED, 2026-09-26 (I13CLOSE).  This
//     header used to end "and nothing in this console produces it: no ratified
//     material field carries the palette's identity".  The first clause was
//     true and the second was NOT: `MaterialRecord.palette_base` is the
//     palette's ratified name and the oracle reads the palette from it -- what
//     was missing was a block that made that address RESIDENT.
//     `zhao_texture_palette_load` is that block, and this one asks it, on the
//     resolved record's own `palette_base`, before it publishes.
//
//     THE ASK IS A STATE, NOT A WIRE, and that is the whole reason it is safe.
//     ST_PAL is entered from ST_WAIT only for a CLUT record, only after the
//     span has already DRAINED, and `t_valid_o` is gated on ST_RUN -- so no
//     triangle can be emitted against a half-formed material.  The pair is
//     latched by the SAME enable that sets `pub_valid_q`, one more field of one
//     published record.
//
//     `clut_unowned_o` SURVIVES AND NARROWS.  It used to count every CLUT
//     material, because none of them had an identity; it now counts a CLUT
//     material whose palette could not be made resident -- a null, misaligned
//     or out-of-VRAM `palette_base`, or a denied fetch.  `clut_owned_o` is its
//     companion, and the pair is what makes a ZERO on either side a
//     measurement instead of a silence.
//   * response_class.  `spec/commands.zidl` ratifies
//     `MaterialSample.modes[3:0]` as "tmu_mode u4 nearest/bilinear/CLUT/direct"
//     and NEVER ASSIGNS THE NUMBERS.  Searched: `spec/`, `reference/`,
//     `design/contracts/`, `tests/` and `tools/` for `tmu_mode` -- four hits,
//     none of them an encoding (the directed tests use 1, 2 and 3 as opaque
//     bytes and check only that the field unpacks).  So the mapping is an
//     OWNER DECISION, it is recorded as one, and it lives HERE in a single
//     editable parameter so the owner's pick is a one-line change
//     (CLAUDE.md rule 6).  The default is the one that makes the witness
//     MEANINGFUL: tmu_mode[1:0] is read as the class in the binding resolver's
//     own numbering (0 CLUT, 1 NEAR, 2 BIL), so the check differences the
//     MATERIAL RECORD in VRAM against the BINDING PAGE from the config stream
//     -- two independent sources, which is what lets `witness_mismatch_o` fire
//     at all.  A mapping derived from the binding page would have been the
//     detector-wired-to-two-operands-that-move-together defect.
//
// `lod_q4_4` is excluded by MATERIAL.RESOLVE's contract in terms ("it returns
// the mip policy; the sampler picks the level"), and `base_rgb` / `base_alpha`
// are the VERTEX's, not the material's.  Neither is published here, and the
// composer says what it drives them with and why.
//
// ---------------------------------------------------------------------------
// A DENIED OR MISSING RECORD RESOLVES, AND IS COUNTED.  IT NEVER HANGS.
// ---------------------------------------------------------------------------
// Owner ruling R20's law, carried one seam further.  `zhao_material_resolve`
// always answers -- a miss, a not-resident set, an illegal record and a denied
// fetch all produce `rsp_valid_o` with `rsp_has_record_o` low.  This block
// publishes the DEFINED FAULT MATERIAL for that case (sample_count 0, which is
// the legal "this surface takes no texture sample" profile) and counts it on
// `no_record_o`.  The stream never stops; the fault is visible; nothing is
// invented to cover it.
//
// ---------------------------------------------------------------------------
// `NO_MATERIAL` IS A LAWFUL MODE -- owner ruling 1, 2026-09-22 (PARTMAT)
// ---------------------------------------------------------------------------
// The ruling's own words: *"Introduce an explicit material mode distinguishing
// a material-backed primitive from a primitive intentionally carrying no
// material. Do not infer no-material from a failed lookup, an arbitrary
// sentinel handle, the previous span's material, or merely the untextured
// attribute bit."*
//
// So `t_material_mode_i` is a DECLARATION the producer makes, beside its
// `{set, id}` on the same beat, and it is the ONLY thing this block reads to
// decide which of the two modes a span is in.  Each of the four inferences the
// ruling forbids is refused by construction here and it is worth writing down
// which line does it:
//
//   * a failed lookup       -> `no_record_o`'s arm is reached only from
//                              ST_WAIT, and a NO_MATERIAL span never enters
//                              ST_REQ, so it cannot arrive there;
//   * a sentinel handle     -> nothing compares `{set, id}` against a magic
//                              value anywhere in this file;
//   * the previous span's   -> the mode is part of `match_c`, so a mode change
//     material                 DRAINS exactly as a `{set, id}` change does;
//   * the untextured bit    -> this block has no `untex` port.  R197's gate is
//                              downstream and stays there, and the two
//                              questions stay separate: "no texture
//                              coordinates" is a statement about the PRIMITIVE,
//                              "no material" is a statement about the PRODUCER.
//
// WHAT THE MODE DOES.  In `MATMODE_NONE` the window publishes the DEFINED
// NO-SAMPLING PROFILE below, issues NO resolve (so MATERIAL.RESOLVE never sees
// a request, and no texture or palette residency is touched), and moves NO
// fault counter.  `no_material_spans_o` counts the spans, and it is CENSUS, not
// a fault -- the ruling is explicit that this "is a lawful mode, not the
// missing-material fallback disguised as success".
//
// THE PROFILE IS DEFINED HERE AND IS NOT R20's FAULT MATERIAL.  The two happen
// to carry the same field values -- there is only one way to say "this surface
// takes no texture sample" in a 298-bit flat request -- and they are
// deliberately written as two separate named constants reached by two separate
// arms, because they mean opposite things and only one of them is a fault.
// Collapsing them into one assignment is precisely the "fallback disguised as
// success" the ruling refuses.
//
// MODE IS PART OF THE SPAN'S IDENTITY, AND THERE IS NO SECOND QUEUE.  The
// ruling: *"Changing MATERIAL_BACKED -> NO_MATERIAL -> MATERIAL_BACKED must
// honor the existing drain/ordering mechanism so that in-flight triangles
// retain their own profile.  Do not add an independently advancing metadata
// queue."*  That last sentence is this repository's own worst defect and
// CLAUDE.md has a chapter about it.  The mode is therefore carried in
// `pub_mode_q`, ONE MORE FIELD OF THE SAME PUBLISHED RECORD, loaded by the same
// enable in the same state as `pub_count_q` -- so the interlock argument above
// covers it word for word and no new ordering claim is made.  A mode change is
// a `match_c` miss, which is a drain, which is the mechanism that already
// exists.
//
// AND CONTRADICTORY DECLARATIONS ARE REFUSED, NOT REPAIRED.  The ruling:
// *"Reject internally contradictory declarations rather than silently repairing
// them."*  Two declarations are contradictory at this port:
//
//   * an UNDEFINED mode encoding (2'd2, 2'd3 -- reserved, no producer may use
//     them); and
//   * `MATMODE_NONE` presented with a non-zero `{set, id}`.  A producer that
//     says "I carry no material" while handing over a material identity it
//     expects to be resolved has said two incompatible things on one beat.
//     Publishing the no-sampling profile for it would be silently repairing
//     the declaration; resolving the pair would be ignoring it.
//
// A refused beat is CONSUMED and COUNTED on `mode_refused_o` and never enters
// the span -- the same shape as R197's untextured door, which consumes a
// refused triangle before `d_enter_i` can fire, so the drain accounting is
// untouched.  `mode_refused_o` is reachable with legal stimulus AT THIS BLOCK'S
// OWN PORTS (drive the mode input), so it is the `t_ack_i` shape and owes no
// committed mutant.
//
// `t_quality_tier_i` is deliberately NOT part of the contradiction test.  The
// resolver ECHOES the tier and reads it nowhere, so it is a label travelling
// with a request that a NO_MATERIAL span does not make; requiring it to be zero
// would be inventing a rule the ruling does not state.
//
// Conservative SystemVerilog subset only; Quartus 17.0 syntax rules apply
// (elaboration checks inside `initial begin ... end`, explicit generate, no
// inline `for (genvar ...)`).
`default_nettype none

module zhao_material_window #(
    // THE OWNER'S KNOB for the tmu_mode -> response_class mapping described
    // above, four two-bit entries packed low-to-high: mode 0 in [1:0], mode 1
    // in [3:2], mode 2 in [5:4], mode 3 in [7:6].  The default is the identity
    // on the binding resolver's own class numbering (0 CLUT, 1 NEAR, 2 BIL);
    // entry 3 is the reserved mode and maps to the reserved class 3, which the
    // binding resolver refuses rather than samples.
    parameter logic [7:0] TMU_MODE_CLASS = 8'b11_10_01_00,
    // Deepest downstream occupancy the drain counter must represent.  The span
    // it watches is GEOM.CLIP (3 stages), GEOM.SETUP and GEOM.ATTRPACK, so a
    // dozen is generous; the counter REFUSES to wrap and says so.
    parameter int unsigned OCCW = 8
) (
    input  wire                     clk,
    input  wire                     rst_n,

    // ---- the triangle stream, in: GEOM.REPLAY -----------------------------
    input  wire                     t_valid_i,
    output wire                     t_ready_o,
    input  wire        [31:0]       t_material_set_i,
    input  wire        [15:0]       t_material_id_i,
    // THE PRODUCER'S MATERIAL-MODE DECLARATION (owner ruling 1, 2026-09-22).
    // `MATMODE_BACKED_C` or `MATMODE_NONE_C`; anything else is refused. It
    // travels on the SAME beat as the pair it qualifies, which is what stops it
    // being a second live wire (entry I39).
    input  wire        [ 1:0]       t_material_mode_i,
    // ---- THE PRIMITIVE'S RASTER DECLARATION, 2026-09-23 (SHADOWRIDE) ------
    // R89's FLAT per-primitive alpha and the raster state word, arriving on the
    // SAME granted beat as the material pair and the mode, from
    // `zhao_geom_clipdoor`.  They are latched into the published record by the
    // SAME enable as `pub_mode_q` and they are part of `match_c`, for the
    // reason `t_material_mode_i` is: a span is a run of primitives that agree,
    // and two primitives that disagree about their alpha are two spans.  A
    // field latched by a different enable, or left out of the identity, is the
    // independently-advancing metadata queue this block exists to refuse.
    input  wire        [ 7:0]       t_vertex_alpha_i,
    // TERRAIN.NORMALMAP's DETAIL DECLARATION, from the granted client's own
    // port (NORMALMAP, 2026-09-26). It joins `match_c` below for the same
    // reason `t_frag_state_i` and `t_vertex_alpha_i` do: this block publishes
    // ONE record for a whole span, so a primitive whose declaration differs
    // from the running span's must DRAIN rather than inherit it. Without that
    // term a terrain triangle and a particle -- both lawfully `MATMODE_NONE`
    // with a zero pair -- would share a span and one declaration, and the
    // wrong one would reach half the fragments with every counter balancing.
    input  wire                     t_detail_i,
    input  wire        [31:0]       t_frag_state_i,
    // The draw's semantic weight, carried as the resolve's quality tier.  The
    // resolver ECHOES it (`tier_q`) and reads it nowhere, so this is a label
    // travelling with its request, not a policy this block invents.
    input  wire        [ 7:0]       t_quality_tier_i,

    // ---- the triangle stream, out: GEOM.CLIP ------------------------------
    output wire                     t_valid_o,
    input  wire                     t_ready_i,

    // ---- the downstream span's three disposal events ----------------------
    input  wire                     d_enter_i,
    input  wire                     d_reject_i,
    input  wire                     d_leave_i,

    // ---- MATERIAL.RESOLVE's request ---------------------------------------
    output logic                    req_valid_o,
    input  wire                     req_ready_i,
    output logic       [31:0]       req_material_set_o,
    output logic       [15:0]       req_material_id_o,
    output logic       [ 7:0]       req_quality_tier_o,

    // ---- MATERIAL.RESOLVE's response --------------------------------------
    input  wire                     rsp_valid_i,
    output logic                    rsp_ready_o,
    input  wire        [ 2:0]       rsp_status_i,
    input  wire                     rsp_has_record_i,
    input  wire        [ 1:0]       rsp_sample_count_i,
    input  wire        [ 2:0]       rsp_material_recipe_i,
    input  wire        [ 7:0]       rsp_recipe_weight_i,
    input  wire        [ 7:0]       rsp_base_binding_i,
    input  wire                     rsp_selector_overflow_i,
    input  wire        [ 7:0]       rsp_sample0_modes_i,
    // THE PALETTE'S RATIFIED NAME, MaterialRecord.palette_base -- an ADDRESS,
    // and the field the oracle reads the palette from.  Zero when no sample is
    // a CLUT mode, which is the record's own law and is why a zero base is a
    // refusal here rather than a slot.
    input  wire        [31:0]       rsp_palette_base_i,
    // ---- THE MATERIAL'S FRAGMENT PROFILE (FRAGSTATE, 2026-09-25) ----------
    // MATERIAL.RESOLVE's projection of `MaterialRecord.fragment_state` and
    // `fragment_decl`.  `rsp_frag_declared_i` is the AUTHORITY SELECTOR and it
    // is not a "payload is non-zero" test: the all-zero state word is the legal
    // opaque profile, so the declaration cannot be inferred from the payload.
    input  wire                     rsp_frag_declared_i,
    input  wire        [31:0]       rsp_frag_state_i,
    input  wire        [ 7:0]       rsp_effect_tag_i,
    input  wire        [ 7:0]       rsp_stencil_ref_i,

    // ---- the published material, into the flat request --------------------
    output logic                    pub_valid_o,
    output logic       [ 1:0]       pub_sample_count_o,
    output logic       [ 2:0]       pub_material_recipe_o,
    output logic       [ 7:0]       pub_recipe_weight_o,
    output logic       [ 7:0]       pub_base_binding_o,
    output logic       [ 1:0]       pub_response_class_o,
    // The published span's MODE, beside the published record it qualifies.
    // A consumer that needs to know whether the surface it is shading has a
    // material at all reads this rather than inferring it from
    // `pub_sample_count_o == 0`, which is true of a legal non-sampling MATERIAL
    // too (the `page == 255` creature).
    output logic       [ 1:0]       pub_material_mode_o,
    // The span's own alpha and raster state, for entry I20's
    // `tri_continuation_tail_i` and `tri_fragment_state_i`.
    output logic       [ 7:0]       pub_vertex_alpha_o,
    output logic                    pub_detail_o,
    output logic       [31:0]       pub_frag_state_o,
    // THE MATERIAL'S HALF OF THE SAME THREE QUANTITIES, published beside the
    // producer's so the CORE can resolve the authority by NAME rather than by
    // OR-ing two owners together.  `pub_frag_declared_o` says which of the two
    // is authoritative for this span; the owner directive of 2026-09-23 forbids
    // merging them ("Do not OR two overlapping field owners together to avoid
    // resolving which source is authoritative"), and this is the field that
    // makes the resolution possible.
    //
    // All four are loaded by the SAME enable as `pub_count_q` and the rest of
    // the published record, in ST_WAIT, so they cannot advance independently of
    // the material they describe.  They are NOT terms of `match_c`, and that is
    // correct rather than an omission: they are a pure function of
    // {pub_set_q, pub_id_q}, which ARE terms of it, so two primitives that share
    // a span necessarily share this profile.
    output logic                    pub_frag_declared_o,
    output logic       [31:0]       pub_mat_frag_state_o,
    output logic       [ 7:0]       pub_effect_tag_o,
    output logic       [ 7:0]       pub_stencil_ref_o,

    // ---- THE PALETTE IDENTITY (I13CLOSE, 2026-09-26) -----------------------
    // The resolved record's `palette_base` goes out on `pal_req_base_o` and the
    // owner of the island's four palette slots answers with the {slot,
    // generation} pair the binding row's witness has to equal.  Every outcome
    // is an ANSWER -- R20's law, which is why this cannot deadlock the span.
    output logic                    pal_req_valid_o,
    input  logic                    pal_req_ready_i,
    output logic       [31:0]       pal_req_base_o,
    input  logic                    pal_rsp_valid_i,
    output logic                    pal_rsp_ready_o,
    input  logic                    pal_rsp_owned_i,
    input  logic       [ 1:0]       pal_rsp_slot_i,
    input  logic       [ 7:0]       pal_rsp_gen_i,

    // The published pair.  ZERO for every non-CLUT span, which is the DIRECT
    // row's own legality law rather than a default -- `binding_row_legal`
    // refuses a direct row whose pair is non-zero.
    output logic       [ 1:0]       pub_palette_slot_o,
    output logic       [ 7:0]       pub_palette_generation_o,

    // ---- evidence ---------------------------------------------------------
    output logic       [31:0]       resolves_o,
    output logic       [31:0]       switches_o,
    output logic       [31:0]       drain_stall_cycles_o,
    output logic       [31:0]       answer_stall_cycles_o,
    output logic       [31:0]       occupancy_max_o,
    output logic       [31:0]       no_record_o,
    output logic       [31:0]       selector_overflow_o,
    // A FAULT, NARROWED 2026-09-26: a CLUT material whose palette could not be
    // made resident.  Before the producer existed this counted every CLUT
    // material, because none of them had an identity at all.
    output logic       [31:0]       clut_unowned_o,
    // ITS COMPANION, and the reason `clut_unowned_o` reading zero is evidence:
    // a CLUT material that DID get a real {slot, generation}.  Two counters
    // that must sum to the CLUT spans resolved, moved by two arms of one
    // decision -- so a zero on either side is a measurement and not a silence.
    output logic       [31:0]       clut_owned_o,
    // CENSUS, NOT A FAULT: spans published in `MATMODE_NONE`. The ruling names
    // this mode lawful, so the number that matters beside it is `resolves_o`
    // and `no_record_o` NOT moving for those spans -- three independent
    // quantities, which is what makes the claim checkable.
    output logic       [31:0]       no_material_spans_o,
    // A FAULT: an undefined mode encoding, or `MATMODE_NONE` presented with a
    // non-zero `{set, id}`. Refused, consumed, never entered.
    output logic       [31:0]       mode_refused_o,
    // THE TWO STRUCTURAL GUARDS.  Both are zero in any correct composition and
    // both are reachable with LEGAL STIMULUS AT THIS BLOCK'S OWN PORTS -- the
    // disposal events are inputs, so a directed test fires them by pulsing a
    // departure that never had an arrival.  That is the `t_ack_i` shape, not
    // the `wq_overflow_o` shape, so neither owes a committed mutant.
    output logic       [31:0]       err_unpublished_o,
    output logic       [31:0]       err_occupancy_underflow_o
);

  // The binding resolver's own class numbering, repeated here only so the
  // parameter's default is readable.  It is NOT a second definition: nothing
  // below compares against these names, they name the parameter's nibbles.
  localparam logic [1:0] CLS_CLUT_C = 2'd0;

  // ---- THE TWO LAWFUL MATERIAL MODES (owner ruling 1, 2026-09-22) ---------
  // 2'd2 and 2'd3 are RESERVED and refused. They are not "don't care": a
  // producer presenting one has made a declaration this block does not
  // understand, and the ruling says to reject rather than repair.
  localparam logic [1:0] MATMODE_BACKED_C = 2'd0;
  localparam logic [1:0] MATMODE_NONE_C   = 2'd1;

  // THE DEFINED NO-SAMPLING PROFILE. Written as five named constants reached by
  // the NO_MATERIAL arm alone. R20's fault material below carries the same
  // field values and is a DIFFERENT statement; see the header.
  localparam logic [1:0] NOMAT_SAMPLE_COUNT_C = 2'd0;
  localparam logic [2:0] NOMAT_RECIPE_C       = 3'd0;
  localparam logic [7:0] NOMAT_WEIGHT_C       = 8'd0;
  localparam logic [7:0] NOMAT_BINDING_C      = 8'd0;
  localparam logic [1:0] NOMAT_CLASS_C        = 2'd0;

  localparam logic [2:0] ST_RUN    = 3'd0;   // pass triangles of the published material
  localparam logic [2:0] ST_DRAIN  = 3'd1;   // hold, waiting for the span to empty
  localparam logic [2:0] ST_REQ    = 3'd2;   // hold, offering the resolve
  localparam logic [2:0] ST_WAIT   = 3'd3;   // hold, waiting for the answer
  localparam logic [2:0] ST_PAL    = 3'd4;   // hold, resolving a CLUT palette

  logic [2:0]  st_q;
  logic        pub_valid_q;
  logic [31:0] pub_set_q;
  logic [15:0] pub_id_q;
  logic [1:0]  pub_count_q;
  logic [2:0]  pub_recipe_q;
  logic [7:0]  pub_weight_q;
  logic [7:0]  pub_binding_q;
  logic [1:0]  pub_class_q;
  logic [1:0]  pub_mode_q;
  logic [7:0]  pub_valpha_q;
  logic        pub_detail_q;
  logic [31:0] pub_state_q;
  // The MATERIAL's half of the published span (FRAGSTATE, 2026-09-25). There is
  // deliberately no `ask_*` twin for these four: an `ask_*` register exists for
  // a field the UPSTREAM offered and the window must remember across the drain,
  // and these arrive in the RESPONSE, after the drain has already finished.
  logic        pub_frag_decl_q;
  logic [31:0] pub_mat_state_q;
  logic [7:0]  pub_tag_q;
  logic [7:0]  pub_sref_q;

  // The palette half of the published record, and the ask it came from.
  logic [1:0]  pub_pslot_q;
  logic [7:0]  pub_pgen_q;
  logic [31:0] pal_base_q;
  logic        pal_asked_q;

  // The pending request, captured from the triangle that asked for it.  It is
  // captured ONCE, on the transition out of ST_RUN, while that triangle is
  // still being offered and its data is therefore stable -- the stream is
  // held from the same clock, so nothing can move underneath it.
  logic [31:0] ask_set_q;
  logic [15:0] ask_id_q;
  logic [7:0]  ask_tier_q;
  logic [1:0]  ask_mode_q;
  logic [7:0]  ask_valpha_q;
  logic        ask_detail_q;
  logic [31:0] ask_state_q;

  logic [OCCW-1:0] occupancy_q;

  // ---- the stream ---------------------------------------------------------
  // `match_c` is a function of the OFFERED data and of registered state, never
  // of `t_valid_i`, so the ready handed upstream is not a function of the valid
  // handed downstream and the pair cannot lock.
  //
  // `refuse_c` is the contradictory-declaration test and it obeys the same
  // rule: a function of the OFFERED declaration only, never of `t_valid_i` and
  // never of `t_ready_i`, so the ready handed upstream still does not depend on
  // the valid handed downstream.
  wire mode_defined_c = (t_material_mode_i == MATMODE_BACKED_C) ||
                        (t_material_mode_i == MATMODE_NONE_C);
  wire mode_contra_c  = (t_material_mode_i == MATMODE_NONE_C) &&
                        ((t_material_set_i != 32'd0) || (t_material_id_i != 16'd0));
  wire refuse_c       = !mode_defined_c || mode_contra_c;

  // THE MODE IS PART OF THE IDENTITY. This one added term is what makes
  // BACKED -> NONE -> BACKED honour the drain: a mode change is a `match_c`
  // miss, and a `match_c` miss is the existing mechanism.
  wire match_c = pub_valid_q &&
                 (t_material_mode_i == pub_mode_q) &&
                 (t_vertex_alpha_i  == pub_valpha_q) &&
                 (t_detail_i        == pub_detail_q) &&
                 (t_frag_state_i    == pub_state_q) &&
                 (t_material_set_i  == pub_set_q) &&
                 (t_material_id_i   == pub_id_q);
  wire pass_c  = (st_q == ST_RUN) && match_c && !refuse_c;

  assign t_valid_o = t_valid_i && pass_c;
  // A refused beat is CONSUMED without being passed -- R197's door in this
  // block's own port list. It never enters the span, so `d_enter_i` cannot fire
  // for it and the drain accounting does not see it at all.
  assign t_ready_o = refuse_c || (t_ready_i && pass_c);

  wire drained_c = (occupancy_q == {OCCW{1'b0}});

  // ---- the published material --------------------------------------------
  assign pub_valid_o           = pub_valid_q;
  assign pub_sample_count_o    = pub_count_q;
  assign pub_material_recipe_o = pub_recipe_q;
  assign pub_recipe_weight_o   = pub_weight_q;
  assign pub_base_binding_o    = pub_binding_q;
  assign pub_response_class_o  = pub_class_q;
  assign pub_material_mode_o   = pub_mode_q;
  assign pub_vertex_alpha_o    = pub_valpha_q;
  assign pub_detail_o          = pub_detail_q;
  assign pub_frag_state_o      = pub_state_q;
  assign pub_frag_declared_o   = pub_frag_decl_q;
  assign pub_mat_frag_state_o  = pub_mat_state_q;
  assign pub_effect_tag_o      = pub_tag_q;
  assign pub_stencil_ref_o     = pub_sref_q;
  assign pub_palette_slot_o       = pub_pslot_q;
  assign pub_palette_generation_o = pub_pgen_q;

  // ---- the palette ask ----------------------------------------------------
  // One outstanding lookup, offered once and then awaited: `pal_asked_q` is the
  // whole of the sequencing, and both sides are functions of the state and of
  // that one bit, never of each other, so the pair cannot lock.
  assign pal_req_valid_o = (st_q == ST_PAL) && !pal_asked_q;
  assign pal_req_base_o  = pal_base_q;
  assign pal_rsp_ready_o = (st_q == ST_PAL) && pal_asked_q;

  // ---- the request --------------------------------------------------------
  assign req_valid_o        = (st_q == ST_REQ);
  assign req_material_set_o = ask_set_q;
  assign req_material_id_o  = ask_id_q;
  assign req_quality_tier_o = ask_tier_q;
  assign rsp_ready_o        = (st_q == ST_WAIT);

  // The answer's class, through the owner's editable mapping.  `modes[3:2]`
  // are the reserved half of the ratified u4 and take no part in it.
  wire [1:0] rsp_mode_c  = rsp_sample0_modes_i[1:0];
  wire [1:0] rsp_class_c = (rsp_mode_c == 2'd0) ? TMU_MODE_CLASS[1:0] :
                           (rsp_mode_c == 2'd1) ? TMU_MODE_CLASS[3:2] :
                           (rsp_mode_c == 2'd2) ? TMU_MODE_CLASS[5:4] :
                                                  TMU_MODE_CLASS[7:6];

  wire rsp_take_c = rsp_valid_i && (st_q == ST_WAIT);

  // ---- the occupancy of the span this block protects ----------------------
  // Arrivals and departures on the same clock cancel, which is why the two are
  // summed rather than sequenced.
  wire        occ_up_c   = d_enter_i;
  wire [1:0]  occ_down_c = {1'b0, d_reject_i} + {1'b0, d_leave_i};

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      occupancy_q               <= {OCCW{1'b0}};
      occupancy_max_o           <= 32'd0;
      err_occupancy_underflow_o <= 32'd0;
      err_unpublished_o         <= 32'd0;
    end else begin
      // A departure with nothing outstanding is a broken accounting, not a
      // wrap: it is COUNTED and the counter is clamped at zero, so one fault
      // does not turn the drain condition into a lie for the rest of the frame.
      if (({1'b0, occupancy_q} + {{OCCW{1'b0}}, occ_up_c}) <
          {{(OCCW-1){1'b0}}, occ_down_c}) begin
        err_occupancy_underflow_o <= err_occupancy_underflow_o + 32'd1;
        occupancy_q               <= {OCCW{1'b0}};
      end else begin
        occupancy_q <= occupancy_q + {{(OCCW-1){1'b0}}, occ_up_c}
                                   - {{(OCCW-2){1'b0}}, occ_down_c};
      end

      if ({24'd0, occupancy_q} > occupancy_max_o)
        occupancy_max_o <= {24'd0, occupancy_q};

      // THE GUARD THE WHOLE INTERLOCK RESTS ON.  A triangle may only reach the
      // door through this block, and this block only passes one when something
      // is published -- so a departure with `pub_valid_q` low means a triangle
      // is being shaded with a material nobody resolved.
      if (d_leave_i && !pub_valid_q)
        err_unpublished_o <= err_unpublished_o + 32'd1;
    end
  end

  // ---- the state machine and the published registers ----------------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_q                  <= ST_RUN;
      pub_valid_q           <= 1'b0;
      pub_set_q             <= 32'd0;
      pub_id_q              <= 16'd0;
      pub_count_q           <= 2'd0;
      pub_recipe_q          <= 3'd0;
      pub_weight_q          <= 8'd0;
      pub_binding_q         <= 8'd0;
      pub_class_q           <= 2'd0;
      pub_mode_q            <= MATMODE_BACKED_C;
      pub_valpha_q          <= 8'd0;
      pub_detail_q          <= 1'b0;
      pub_state_q           <= 32'd0;
      pub_frag_decl_q       <= 1'b0;
      pub_mat_state_q       <= 32'd0;
      pub_tag_q             <= 8'd0;
      pub_sref_q            <= 8'd0;
      ask_set_q             <= 32'd0;
      ask_id_q              <= 16'd0;
      ask_tier_q            <= 8'd0;
      ask_mode_q            <= MATMODE_BACKED_C;
      ask_valpha_q          <= 8'd0;
      ask_detail_q          <= 1'b0;
      ask_state_q           <= 32'd0;
      no_material_spans_o   <= 32'd0;
      mode_refused_o        <= 32'd0;
      resolves_o            <= 32'd0;
      switches_o            <= 32'd0;
      drain_stall_cycles_o  <= 32'd0;
      answer_stall_cycles_o <= 32'd0;
      no_record_o           <= 32'd0;
      selector_overflow_o   <= 32'd0;
      clut_unowned_o        <= 32'd0;
      clut_owned_o          <= 32'd0;
      pub_pslot_q           <= 2'd0;
      pub_pgen_q            <= 8'd0;
      pal_base_q            <= 32'd0;
      pal_asked_q           <= 1'b0;
    end else begin
      // THE CONTRADICTORY DECLARATION, COUNTED. The ready is high on this
      // clock (`t_ready_o` is `refuse_c || ...`), so this is one count per
      // refused beat and not one per stalled cycle. It is written OUTSIDE the
      // case because a refusal is not a state transition -- the beat is
      // consumed and the window's span accounting never learns of it, in
      // whatever state the window happens to be.
      if (t_valid_i && refuse_c && (mode_refused_o != 32'hffff_ffff))
        mode_refused_o <= mode_refused_o + 32'd1;

      case (st_q)
        ST_RUN: begin
          if (t_valid_i && !refuse_c && !match_c) begin
            ask_set_q  <= t_material_set_i;
            ask_id_q   <= t_material_id_i;
            ask_tier_q <= t_quality_tier_i;
            ask_mode_q   <= t_material_mode_i;
            ask_valpha_q <= t_vertex_alpha_i;
            ask_detail_q <= t_detail_i;
            ask_state_q  <= t_frag_state_i;
            switches_o <= switches_o + 32'd1;
            st_q       <= ST_DRAIN;
          end
        end

        ST_DRAIN: begin
          // Counted only while the span is ACTUALLY occupied, so a switch that
          // costs nothing reads as nothing. A counter that charges one cycle
          // for the unavoidable state transition would report a permanent
          // floor and hide the moment the drain starts to matter.
          if (!drained_c) drain_stall_cycles_o <= drain_stall_cycles_o + 32'd1;
          if (drained_c) begin
            if (ask_mode_q == MATMODE_NONE_C) begin
              // THE LAWFUL NO-MATERIAL ARM. It publishes the DEFINED
              // no-sampling profile and goes straight back to ST_RUN: ST_REQ
              // and ST_WAIT are never entered, so `req_valid_o` -- which is
              // literally `(st_q == ST_REQ)` -- cannot assert and
              // MATERIAL.RESOLVE never sees a request. `no_record_o`'s only
              // assignment lives in ST_WAIT, so it cannot move either. That is
              // the ruling's "issue no material/texture/palette resolution
              // request, and increment no missing-material fault counter", as
              // a property of the state graph rather than a promise.
              //
              // The drain still happened. That is what keeps an in-flight
              // MATERIAL_BACKED triangle on its own profile across the switch.
              pub_valid_q <= 1'b1;
              pub_mode_q   <= MATMODE_NONE_C;
              pub_valpha_q <= ask_valpha_q;
              pub_detail_q <= ask_detail_q;
              pub_state_q  <= ask_state_q;
              pub_set_q   <= ask_set_q;   // zero, enforced by `mode_contra_c`
              pub_id_q    <= ask_id_q;    // zero, enforced by `mode_contra_c`
              // NO MATERIAL MEANS NO MATERIAL DECLARATION, said explicitly.
              // A producer that declared MATMODE_NONE has no record to read a
              // profile out of, so the PRODUCER's door declaration is
              // authoritative for this span -- which is what `pub_frag_decl_q`
              // low means downstream. Particles and shadows live here.
              pub_frag_decl_q <= 1'b0;
              pub_mat_state_q <= 32'd0;
              pub_tag_q       <= 8'd0;
              pub_sref_q      <= 8'd0;
              pub_count_q   <= NOMAT_SAMPLE_COUNT_C;
              pub_recipe_q  <= NOMAT_RECIPE_C;
              pub_weight_q  <= NOMAT_WEIGHT_C;
              pub_binding_q <= NOMAT_BINDING_C;
              pub_class_q   <= NOMAT_CLASS_C;
              if (no_material_spans_o != 32'hffff_ffff)
                no_material_spans_o <= no_material_spans_o + 32'd1;
              st_q <= ST_RUN;
            end else begin
              st_q <= ST_REQ;
            end
          end
        end

        ST_REQ: begin
          answer_stall_cycles_o <= answer_stall_cycles_o + 32'd1;
          if (req_ready_i) begin
            resolves_o <= resolves_o + 32'd1;
            st_q       <= ST_WAIT;
          end
        end

        ST_WAIT: begin
          answer_stall_cycles_o <= answer_stall_cycles_o + 32'd1;
          if (rsp_valid_i) begin
            // `pub_valid_q` IS DECIDED AT THE BOTTOM OF THIS ARM, not here: a
            // CLUT record goes to ST_PAL first and must not be published until
            // its palette identity exists.  It used to be raised on this line.
            // Only a MATERIAL_BACKED span reaches this state, so the mode
            // published here is that one. It is loaded by the SAME enable as
            // the record beside it -- one more field of one published record,
            // which is the whole of "do not add an independently advancing
            // metadata queue".
            pub_mode_q   <= MATMODE_BACKED_C;
            pub_valpha_q <= ask_valpha_q;
            pub_detail_q <= ask_detail_q;
            pub_state_q  <= ask_state_q;
            pub_set_q   <= ask_set_q;
            pub_id_q    <= ask_id_q;
            if (rsp_has_record_i) begin
              pub_count_q   <= rsp_sample_count_i;
              pub_recipe_q  <= rsp_material_recipe_i;
              pub_weight_q  <= rsp_recipe_weight_i;
              pub_binding_q <= rsp_base_binding_i;
              pub_class_q   <= rsp_class_c;
              // The material's fragment profile, on the same enable as the
              // four fields above it.
              pub_frag_decl_q <= rsp_frag_declared_i;
              pub_mat_state_q <= rsp_frag_state_i;
              pub_tag_q       <= rsp_effect_tag_i;
              pub_sref_q      <= rsp_stencil_ref_i;
              // THE PALETTE HALF.  A non-CLUT record publishes the DIRECT
              // row's own legality law -- `binding_row_legal` REFUSES a direct
              // row whose {palette_generation, palette_slot} is not zero -- and
              // a CLUT record goes to ST_PAL to be given a real one.
              if (rsp_class_c == CLS_CLUT_C) begin
                pal_base_q  <= rsp_palette_base_i;
                pal_asked_q <= 1'b0;
              end else begin
                pub_pslot_q <= 2'd0;
                pub_pgen_q  <= 8'd0;
              end
            end else begin
              // R20's defined fault material: a surface that takes no sample.
              // It is published, so the stream runs; it is counted, so the
              // fault is not a silent black triangle.
              //
              // THE FIELD VALUES MATCH `NOMAT_*` ABOVE AND THE TWO ARE NOT THE
              // SAME STATEMENT. This arm means "a MATERIAL_BACKED primitive
              // asked for a record and there was none" and it is a FAULT; that
              // arm means "a producer declared it has no material" and it is
              // LAWFUL. There is only one way to spell "takes no sample" in the
              // flat request, so the values coincide; writing them once and
              // sharing the arm is exactly the "fallback disguised as success"
              // owner ruling 1 refuses, and `pub_mode_q` is what tells the two
              // apart downstream.
              pub_count_q   <= 2'd0;
              pub_recipe_q  <= 3'd0;
              pub_weight_q  <= 8'd0;
              pub_binding_q <= 8'd0;
              pub_class_q   <= 2'd0;
              // A FAULT MATERIAL DECLARES NOTHING. There is no record, so there
              // is no profile to take, and the producer's declaration stands --
              // the same answer as the lawful no-material arm, reached for a
              // different reason, and `pub_mode_q` is still what tells the two
              // apart. Taking the PREVIOUS material's profile here would be the
              // stale-metadata fault this block exists to refuse.
              pub_frag_decl_q <= 1'b0;
              pub_mat_state_q <= 32'd0;
              pub_tag_q       <= 8'd0;
              pub_sref_q      <= 8'd0;
              no_record_o   <= no_record_o + 32'd1;
            end
            if (rsp_selector_overflow_i)
              selector_overflow_o <= selector_overflow_o + 32'd1;
            // A CLUT record is NOT published here.  `pub_valid_q` stays low
            // through ST_PAL so the span cannot be read half-formed -- the
            // drain has already emptied it and `t_valid_o` is gated on ST_RUN,
            // so this is belt and braces rather than the only guard, and it is
            // cheaper than arguing that the window nobody can look through is
            // safe to leave open.
            if (rsp_has_record_i && (rsp_class_c == CLS_CLUT_C)) begin
              pub_valid_q <= 1'b0;
              st_q        <= ST_PAL;
            end else begin
              pub_valid_q <= 1'b1;
              st_q        <= ST_RUN;
            end
          end
        end

        // THE PALETTE ASK.  Counted on the SAME counter as ST_WAIT, because it
        // is the same thing from the span's point of view: the window is
        // holding the stream while the answer it will publish is assembled.
        ST_PAL: begin
          answer_stall_cycles_o <= answer_stall_cycles_o + 32'd1;
          if (pal_req_valid_o && pal_req_ready_i) pal_asked_q <= 1'b1;
          if (pal_rsp_valid_i && pal_rsp_ready_o) begin
            // AN UNOWNED PALETTE PUBLISHES ZERO AND IS COUNTED.  It does NOT
            // publish the previous span's pair, and it does not hold the
            // stream: the witness will then fail at the binding resolver and
            // the fragment takes the typed refusal, which is loud, rather than
            // sampling somebody else's colours, which is not.
            pub_pslot_q <= pal_rsp_owned_i ? pal_rsp_slot_i : 2'd0;
            pub_pgen_q  <= pal_rsp_owned_i ? pal_rsp_gen_i  : 8'd0;
            if (pal_rsp_owned_i) clut_owned_o   <= clut_owned_o   + 32'd1;
            else                 clut_unowned_o <= clut_unowned_o + 32'd1;
            pal_asked_q <= 1'b0;
            pub_valid_q <= 1'b1;
            st_q        <= ST_RUN;
          end
        end

        default: st_q <= ST_RUN;
      endcase
    end
  end

  // `rsp_status_i` and `rsp_take_c` are carried for the waveform and for the
  // bench; the block's own decisions are made on `rsp_has_record_i`, which is
  // the resolver's single answer to "is there a record", and on the counted
  // fields beside it.  Reading `status` here as well would be a second opinion
  // about the same question.
  /* verilator lint_off UNUSEDSIGNAL */
  wire [2:0] unused_status_c = rsp_status_i;
  wire       unused_take_c   = rsp_take_c;
  // `modes[3:2]` is the reserved half of the ratified u4 tmu_mode and takes no
  // part in the class; `modes[5:4]` is WRAP and `modes[7:6]` is MIP POLICY,
  // and both are the SAMPLER's, not the flat request's -- the 298-bit request
  // has no field for either. Named rather than masked away, so the day one of
  // them gains a consumer this line is where it is found.
  wire [5:0] unused_modes_c  = rsp_sample0_modes_i[7:2];
  /* verilator lint_on UNUSEDSIGNAL */

  // synthesis translate_off
  initial begin
    if (OCCW < 4)
      $fatal(1, "zhao_material_window: OCCW=%0d cannot represent the GEOM.CLIP..door span", OCCW);
  end
  // synthesis translate_on

endmodule

`default_nettype wire
