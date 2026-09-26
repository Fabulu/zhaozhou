// zhao_forge_assemble_early_read_mutant.sv -- A COMMITTED MUTANT. NOT PRODUCTION RTL.
//
// WHAT WAS CHANGED, and it is ONE line
// ------------------------------------
// A_DRAIN's release condition loses its `inv_land_q` half:
//     if ((pos_land_q == vcount_q) && (inv_land_q == vcount_q))   // production
//     if  (pos_land_q == vcount_q)                                // here
// so the triangle walk is released into A_TRIS while the canonical depths are
// still in flight. The module is renamed so no source list can elaborate it by
// mistake.
//
// WHY IT HAS TO EXIST
// -------------------
// FLOPARRAY deleted this block's asynchronous reset loop over `pos_q`/`inv_q`
// -- the single cause of 34,840 bits sitting in flip-flops, measured by
// `tests/probes/zhao_floparray_probe.sv` arms p0/p1. Deleting it means the
// arrays no longer read as zero after reset, and what replaces that guarantee
// is a PROOF: `t_ready_o` is gated on A_TRIS, and A_DRAIN will not release into
// A_TRIS until BOTH landing counters agree with `vcount_q`, so every slot a
// triple can name has been written this job.
//
// A proof is an argument. The barrier assertion in the production block is the
// instrument that holds it to account -- and that assertion is UNREACHABLE WITH
// LEGAL STIMULUS, because no input sequence can make a correct A_DRAIN release
// early. CLAUDE.md is explicit about what that owes: "A guard you cannot reach
// with legal stimulus needs a COMMITTED MUTANT", not a sentence, and not a
// temporary edit to production RTL -- that is a live-tree hazard AND it leaves
// the next person the same argument with no evidence.
//
// So this file is evidence ABOUT THE INSTRUMENT, not about the design. Its
// driver has INVERTED POLARITY: the ctest passes when the assertion FIRES.
//
// DO NOT WRITE A TEST THAT ASSERTS THE BUG. The positive statement -- that the
// store holds and every corner comes from its own index -- is asserted by
// `forge_assemble_directed` against PRODUCTION. This file is kept separate for
// exactly the reason CLAUDE.md gives: a test that asserts the defect passes
// only while the defect exists.
//
// REGENERATE IT if `zhao_forge_assemble.sv` changes shape. This is a COPY, and
// a copy of an old version is a positive control for a block that no longer
// exists. `tools/budget/mutant_copy_drift.py` watches for exactly that.
//// zhao_forge_assemble.sv -- the forge meshlet's terminal stage: world vertices
// and index triples in, SCREEN TRIANGLES at GEOM.CLIP's door out.
//
// Law: owner ruling R187 -- "the honest door is at GEOM.CLIP's INPUT, not
//      GEOM.SETUP's"
//      owner ruling R197 -- the declared-untextured profile
//      owner ruling R234 D1 -- the Gouraud planes carry an untextured
//      primitive's PRE-LIT colour
//      owner ruling D-4 -- "all downstream consumers receive only the canonical
//      invw24. No consumer performs its own profile conversion."
//      owner ruling R3 -- client A of the projector stays a TIME MULTIPLEX
//
// ===========================================================================
// WHAT THIS BLOCK IS FOR
// ===========================================================================
// `zhao_forge_prim` emits INDEX TRIPLES and no coordinate. `zhao_forge_prim_eval`
// (ribbon) and `zhao_forge_ring_eval` (fan, tube, shell, billboard sheet) emit
// WORLD fx16 POSITIONS and no topology. `zref_forge_page.hpp` says why they are
// shaped that way -- "two halves of one meshlet rather than two stages of a
// chain" -- and names the ONLY thing that joins them: the ring-major ORDERING
// CONVENTION. Vertex n of the position stream is index n of the topology
// stream, and nothing else states the relation.
//
// So this block is the join, and it is the reason a forge primitive could not
// be composed before: there was no vertex it could look an index up in.
//
// ===========================================================================
// THE ROUTE IS THE PARTICLE'S, NOT TERRAIN'S -- AND THAT WAS MEASURED, NOT
// ASSUMED
// ===========================================================================
// FINDINGS-shadowsub's follow-up WITHDREW its own claim that a client-A client
// needs an arena-fill path on the result port, and the withdrawal is load
// bearing here: `zhao_part_project` takes its particle results STRAIGHT OUT on
// `q_*` as `signed [20:0]` canvas coordinates with NO ARENA ANYWHERE on that
// path. A client-A client is not obliged to land in an arena; it is obliged to
// carry an owner in the RIDER and take its results back on a DEMUX ARM.
// Particles do exactly that, today, in composed silicon, and the widths already
// agree with `zhao_geom_clip.tri_ax_i`.
//
// This block is therefore the THIRD owner on `zhao_part_project`'s front mux --
// `OWNER_FORGE = 2'd2`, the value that block's encoding has RESERVED since
// R68 sub-build 4 -- and it takes its results back on that block's third demux
// arm. Owner ruling R3 is not touched: R3 withholds a third PORT on
// `zhao_project_service`, not a third CLIENT, and it NAMES the time multiplex
// as the thing to keep.
//
// THE RESULT SIDE HAS NO BACKPRESSURE. `zhao_project_service`'s `a_valid_o`
// fires unconditionally 36 clocks after the accept and there is no `a_ready_i`
// anywhere on that side. So this block must ALWAYS be able to take a result,
// and the way it is always able to is that it never offers more than it has
// room to land: `f_valid_o` is gated on the in-flight count plus the depth
// queue's occupancy against `INFLIGHT`. That is the same self-throttle
// `zhao_part_project`'s slot store performs for particles, for the same reason,
// and it is a PREMISE rather than a hope -- the elaboration guard below
// requires INFLIGHT to cover the service's stated latency.
//
// ===========================================================================
// THE CANONICAL DEPTH, AND WHY A LOCAL PAIR IS NOT A SECOND LAW
// ===========================================================================
// GEOM.CLIP's attribute slot 0 is invw24. The projector returns `w`, and owner
// ruling D-4 says every consumer receives only the CANONICAL invw24 and that
// "no consumer performs its own profile conversion". This block therefore does
// not convert anything: it instantiates `zhao_geom_depthquant_stream` and the
// `zhao_raster_rcp24_v4` it calls, exactly as `zhao_geom_vattr` does, and the
// pair IS the canonical law. A second INSTANCE of one law is not a second law;
// a second expression of it would be, and there is none here.
//
// `zhao_console_core.sv`'s own header already contemplates and prices this
// pair ("a second `zhao_geom_depthquant_stream` beside a second
// `zhao_raster_rcp24_v4`"), so the cost is recorded rather than discovered.
//
// AND IT IS WHY THIS BLOCK CAN COMPOSE WHERE `zhao_part_expand` CANNOT. The
// clipdoor packet measured PART.EXPAND's two remaining blockers and the first
// was "A CANONICAL DEPTH ... `w` ... exists upstream at `zhao_part_project`'s
// `a_w_i`/`h_w_o` and is DROPPED AT THAT BLOCK'S LADDER QUEUE, so it never
// reaches `p_d_i`". The forge arm has no ladder queue: `w` comes back on the
// demux arm and goes straight into the converter. The blocker is a property of
// the particle PATH, not of the projector.
//
// ===========================================================================
// THE ATTRIBUTE PACKET -- WHAT IS RATIFIED, AND THE ONE THING THAT IS AUTHORED
// ===========================================================================
// Seven slots. Their dispositions here, each with its authority:
//
//   0 INVW     the canonical invw24, per corner, from the converter above.
//   1 U_OVER_W ZERO. R197: a declared-untextured primitive's u/w and v/w slots
//   2 V_OVER_W are never read -- `zhao_geom_attrpack` branches on `tri_untex_i`
//              and substitutes the zero operand for both. Writing zero here is
//              agreeing with the block that will overwrite it, not relying on
//              it. A forge primitive has no u/v BY LAW, which is what R197
//              sanctions and what `o_untex_o` declares at the door.
//   3 R        THE PRE-LIT COLOUR. R234 D1's Gouraud planes are NOT branched on
//   4 G        `tri_untex_i`, deliberately: "an untextured primitive is still
//   5 B        lit, and in the reference oracle it is the UNTEXTURED case that
//   6 ALPHA    carries pre-lit colour on the Gouraud lanes". So these slots are
//              the right home for a forge primitive's colour and the mechanism
//              is ratified.
//
// **BUT THE VALUE IS AUTHORED AND THE PAGE DOES NOT CARRY IT.** The frozen
// FORGE_PROGRAM record has no colour field -- `zref_forge_page.hpp`'s offset
// table has anchors, axes, radii, a seed and a phase, and nothing else. So the
// colour arrives here as FOUR NAMED, EDITABLE INPUTS (`art_r_i`, `art_g_i`,
// `art_b_i`, `art_alpha_i`), driven from named constants at the composer.
//
// THAT IS THE R48 NAMED-SEAM SHAPE AND IT IS THE TREATMENT THE OWNER HAS
// ALREADY ACCEPTED FOR THIS EXACT PROBLEM -- R133's D-FORGESHADOW-A accepted
// `zhao_forge_shadow`'s `cast_strength_i` "from a named constant" on the same
// grounds. It is also CLAUDE.md rule 6 in its plainest form: an art value
// belongs in a named, editable constant, and "this is generated from the
// reference, so it is not a knob" is how a wrong number becomes an unadjustable
// wrong number. It is NOT a derived quantity, it must not become one, and the
// day the page grows a colour field these four inputs are the only lines that
// change.
//
// ONE DECLARED LIMITATION, NAMED SO IT IS NOT READ AS AN OVERSIGHT: the colour
// is PER PRIMITIVE, so the three corners carry the same value and the Gouraud
// plane a forge triangle produces is FLAT. R234 D1's planes exist because lit
// colour must be able to vary across a primitive; a forge primitive's cannot,
// because nothing authors a per-vertex colour for it. The mechanism is not
// narrowed -- a per-corner colour would need only three inputs instead of one
// -- and the day the page carries one, that is the change.
//
// ===========================================================================
// THE MATERIAL, AND AN OWNER DECISION THIS BLOCK SURFACES
// ===========================================================================
// `zhao_material_window` is keyed by a PAIR: `t_material_set_i` (handle32) and
// `t_material_id_i` (u16, the entry within that set). Every draw in the ABI
// carries `handle32[material_set] material_set` and gets its id per meshlet
// from the mesh data -- except one.
//
// **THE OWNER RULED IT, 2026-09-22 (completion ruling 2), AND THE DOCKET IS
// CLOSED.** `DrawProcedural` now carries the pair for real: `material_set` is
// the COMPLETE handle32 and `material_id` is an independent u16 in its own
// record bytes. So this block no longer INTERPRETS anything -- it receives both
// halves from `zhao_forge_pagebank` as one held sideband and carries them to
// the door.
//
// WHAT WAS HERE BEFORE, because the ruling requires the difference disclosed
// rather than quietly replaced: the draw's 32-bit word was presented as the set
// AND its low sixteen bits travelled the triangle stream as the id, with a
// `FORGE_MATERIAL_ID` parameter substituted when those bits happened to be
// zero. That parameter is GONE, and its removal is the docket being paid rather
// than a knob being taken away: a handle32 is {index[31:8], generation[7:0]},
// so the id it produced was partly the GENERATION, and the owner forbade the
// reading outright. Where a draw's set handle has a nonzero generation or
// nonzero index bits 8..15, this block now selects a DIFFERENT record than it
// used to -- and for a legacy record whose new `material_id` bytes are zero, it
// selects RECORD 0, which the ruling makes a valid index and not a sentinel.
//
// THE PAIR IS LATCHED BY ONE ENABLE. `mset_q` and `mid_q` are both loaded at
// the job's FIRST VERTEX, from the bank's held level, and `busy_o` rises with
// them so the bank cannot replace either underneath. Latching the two on
// different events is the metadata-swap fault this repository has a chapter
// about, and it is what this block used to do -- the set at vertex 0, the id at
// triple 0, off two different streams.
//
// AND `t_material_i` IS NOW A CHECK RATHER THAN A SOURCE. `zhao_forge_prim`
// still carries the id per triangle, and it arrives here THROUGH A DIFFERENT
// PATH AND A DIFFERENT CADENCE than the held sideband -- which is exactly what
// makes comparing them a real detector and not two operands moving together.
// A disagreement means a primitive's material moved while its triangles were in
// flight; `mat_skew_o` counts it and THE JOB'S OWN ID WINS, because "a later
// draw must not replace an earlier primitive's material".
//
// AND THE R197 DOOR STILL JUDGES IT. `zhao_console_core`'s `cl_in_refuse_c`
// refuses a declared-untextured primitive whenever the published material's
// `sample_count != 0`, and counts it on `geom_untex_refused_o`. So a game that
// points a procedural draw at a sampling material gets a counted refusal rather
// than texel (0,0) smeared across a lightning bolt. Nothing here has to
// re-check that, and nothing here may: one door, R187.

`default_nettype none

module zhao_forge_assemble_early_read_mutant #(
    // (MAX_SEGMENTS + 1) * MAX_SIDES = 65 * 8, `zhao_forge_prim`'s own frozen
    // bound restated as a capacity rather than re-decided as a policy.
    parameter int unsigned MAX_VERTS = 520,
    parameter int unsigned IDW       = 16,
    parameter int unsigned ATTRS     = 7,
    // In-flight projector requests. `zhao_project_core`'s stated latency is 36
    // clocks and the result side has NO backpressure, so this must cover it
    // with room for the depth queue behind it. Guarded below.
    parameter int unsigned INFLIGHT  = 64,
    parameter int unsigned DQ_SLOTS  = 16,
    parameter int unsigned RCP_NCTX  = 8,
    // The attribute slot map, restated from `zhao_console_core`'s own
    // parameters rather than assumed, and checked against ATTRS at elaboration.
    parameter int unsigned SLOT_INVW  = 0,
    parameter int unsigned SLOT_UOW   = 1,
    parameter int unsigned SLOT_VOW   = 2,
    parameter int unsigned SLOT_R     = 3,
    parameter int unsigned SLOT_G     = 4,
    parameter int unsigned SLOT_B     = 5,
    parameter int unsigned SLOT_ALPHA = 6
) (
    input var logic clk,
    input var logic rst_n,

    // ---- the WORLD vertex stream, from the family's evaluator --------------
    // Muxed at the composer by family: `zhao_forge_prim_eval` for the ribbon,
    // `zhao_forge_ring_eval` for the four swept-ring families. This block does
    // not read the family and must not: the ordering convention is the whole of
    // the contract between the two halves, and a block that behaved differently
    // per family would be a second topology law.
    input  var logic               v_valid_i,
    output var logic               v_ready_o,
    input  var logic signed [31:0] v_x_i,
    input  var logic signed [31:0] v_y_i,
    input  var logic signed [31:0] v_z_i,
    input  var logic               v_last_i,

    // ---- the INDEX stream, from zhao_forge_prim ----------------------------
    input  var logic               t_valid_i,
    output var logic               t_ready_o,
    input  var logic [15:0]        t_i0_i,
    input  var logic [15:0]        t_i1_i,
    input  var logic [15:0]        t_i2_i,
    input  var logic [15:0]        t_material_i,
    input  var logic [IDW-1:0]     t_src_id_i,
    input  var logic               t_last_i,

    // ---- the job's per-primitive sideband, from zhao_forge_pagebank --------
    // HANDSHAKED, and that is the repair `mat_skew_o` bought on its first run.
    // See the header: as a pure level, latched at this block's first vertex,
    // the pair could be replaced by the NEXT draw in the window where this
    // block is not yet busy. `j_valid_i` rises with the bank's topology and
    // position jobs and the bank does not retire the job until this half is
    // taken too, so the capture is an event that belongs to the primitive.
    input  var logic               j_valid_i,
    output var logic               j_ready_o,
    // A LEVEL held for the whole primitive, and the reason the bank holds its
    // next draw until `busy_o` falls. It is latched here at the job's FIRST
    // vertex; carrying it on the triangle stream instead would need 32 bits
    // through `zhao_forge_prim`, which has a 16-bit material port and no room.
    input  var logic [31:0]        j_material_set_i,
    // The second half of the SAME sideband, from the same held registers in
    // `zhao_forge_pagebank`, latched here by the SAME enable. Owner completion
    // ruling 2 (2026-09-22): `material_id` is an independent u16 record index,
    // never a slice of the set handle.
    input  var logic [15:0]        j_material_id_i,
    // ---- THE PER-JOB DECLARATION, added 2026-09-23 (SHADOWRIDE) -----------
    // These three were COMPOSER CONSTANTS until this block gained a second
    // producer.  `zhao_console_core` states the rule they obey and it is the
    // reason they are ports and not parameters: "never a constant chosen here,
    // because a mode chosen at a composer is the inferred mode the ruling
    // forbids" (`:13519-13525`).  With FORGE.PRIM and FORGE.SHADOW sharing this
    // block, one constant cannot be true of both: a forge primitive carries a
    // real {set, id} and expects it resolved (MATERIAL_BACKED), and a shadow
    // hull carries a ZERO pair under MATMODE_NONE, which
    // `zhao_material_window.sv:354` REFUSES to pair with a non-zero one.
    //
    // They are latched by the SAME ENABLE as the material pair and move to the
    // in-flight registers on the SAME first-vertex enable, so a job cannot take
    // its material from one draw and its declaration from another.
    input  var logic [ 1:0]        j_material_mode_i,
    // R89's FLAT PER-PRIMITIVE ALPHA, unit8, bound for
    // `tri_continuation_tail_i`'s `vertex_alpha` field and from there to
    // `zhao_raster_blend_prod.a_i`.  `zhao_forge_shadow.sv:295` latches
    // `vtx_alpha_o = strength_q` PER CASTER, so this is per-primitive by the
    // producer's own construction and not by a convention invented here.
    input  var logic [ 7:0]        j_vertex_alpha_i,
    // THE RASTER STATE WORD, `zhao_raster_fragment.sv:213-236`'s layout, bound
    // for `tri_fragment_state_i`.  IT IS LOAD-BEARING FOR THE ALPHA AND THAT IS
    // NOT OBVIOUS: `zhao_raster_blend_fin`'s BL_REPLACE arm reads `src_i` and
    // THROWS THE PRODUCT AWAY, so a flat alpha delivered to `a_i` under the
    // default state changes not one pixel.  A shadow needs BLEND=ALPHA in
    // `[4:3]`, and it needs Z_WRITE_DIS in `[1]` because a transparent hull
    // must not write depth.  Zero is the plain opaque write and is what every
    // other producer declares.
    input  var logic [31:0]        j_frag_state_i,

    // ---- the authored art values (see the header) --------------------------
    input  var logic signed [31:0] art_r_i,
    input  var logic signed [31:0] art_g_i,
    input  var logic signed [31:0] art_b_i,
    input  var logic signed [31:0] art_alpha_i,
    input  var logic [ 7:0]        art_quality_tier_i,
    input  var logic [ 1:0]        art_cull_mode_i,

    // ---- the projector client: owner 2'd2 on zhao_part_project's front mux --
    output var logic               f_valid_o,
    input  var logic               f_ready_i,
    output var logic signed [31:0] f_vx_o,
    output var logic signed [31:0] f_vy_o,
    output var logic signed [31:0] f_vz_o,
    output var logic               f_view_o,
    output var logic [14:0]        f_slot_o,     // the rider's payload half
    // The demux arm. NO READY -- the service has none and neither may this.
    input  var logic               rs_valid_i,
    input  var logic signed [20:0] rs_x_i,
    input  var logic signed [20:0] rs_y_i,
    input  var logic [30:0]        rs_w_i,
    input  var logic               rs_behind_i,
    input  var logic [ 1:0]        rs_profile_i,
    input  var logic [14:0]        rs_slot_i,

    // Which camera this primitive is projected through.
    input  var logic               view_sel_i,

    // ---- the SCREEN triangle, into zhao_geom_clipdoor's client arm ---------
    output var logic               o_valid_o,
    input  var logic               o_ready_i,
    output var logic signed [20:0] o_ax_o,
    output var logic signed [20:0] o_ay_o,
    output var logic signed [20:0] o_bx_o,
    output var logic signed [20:0] o_by_o,
    output var logic signed [20:0] o_cx_o,
    output var logic signed [20:0] o_cy_o,
    output var logic [ 2:0]        o_behind_o,
    output var logic [IDW-1:0]     o_src_id_o,
    output var logic               o_untex_o,
    output var logic [ 1:0]        o_cull_mode_o,
    output var logic [ATTRS*32-1:0] o_attr_a_o,
    output var logic [ATTRS*32-1:0] o_attr_b_o,
    output var logic [ATTRS*32-1:0] o_attr_c_o,
    output var logic [31:0]        o_material_set_o,
    output var logic [15:0]        o_material_id_o,
    // The job's declaration, out on the same beat as the triangle it belongs
    // to, so `zhao_geom_clipdoor` can grant them as ONE record.
    output var logic [ 1:0]        o_material_mode_o,
    output var logic [ 7:0]        o_vertex_alpha_o,
    output var logic [31:0]        o_frag_state_o,
    output var logic [ 7:0]        o_quality_tier_o,

    // ---- the bank's interlock ----------------------------------------------
    // High from the first vertex of a primitive until its last triangle has
    // been taken. `zhao_forge_pagebank` holds its next draw against this, so
    // `j_material_set_i` cannot change under a primitive in flight -- the
    // metadata-swap fault with the material rather than the counters.
    output var logic               busy_o,

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0] jobs_o,           // primitives retired
    output var logic [31:0] vertices_o,       // vertices projected and landed
    output var logic [31:0] triangles_o,      // triangles offered at the door
    output var logic [31:0] index_oor_o,      // a triple naming a vertex the job never emitted
    output var logic [31:0] vtx_overflow_o,   // more vertices than MAX_VERTS
    output var logic [31:0] slot_pressure_o,  // a vertex offered and refused for room
    output var logic [31:0] dq_refused_o,     // the depth converter refused a w
    output var logic [31:0] dq_stray_o,       // a depth token nothing was waiting for
    output var logic [31:0] proj_stray_o,     // a projector result for a slot not in flight
    // THE CARRIAGE DETECTOR (owner completion ruling 2, 2026-09-22). A
    // triangle whose carried material id disagrees with the JOB'S latched
    // id -- i.e. a primitive's material moved while its triangles were in
    // flight. The two operands reach this block by DIFFERENT PATHS at
    // DIFFERENT CADENCES (the held sideband from the bank, the per-triangle
    // copy through FORGE.PRIM's pipeline), which is what makes the
    // comparison able to fire at all -- CLAUDE.md: a detector whose two
    // operands are loaded by one enable is structurally blind.
    output var logic [31:0] mat_skew_o
);

  localparam int unsigned VW  = (MAX_VERTS <= 2) ? 1 : $clog2(MAX_VERTS);
  localparam int unsigned CW  = VW + 1;                 // counts reach MAX_VERTS
  localparam int unsigned POSW = 1 + 21 + 21;           // {behind, y, x}

  // Quartus 17.0 requires an elaboration check inside `initial begin`; a bare
  // module-scope `if` is a syntax error there (QUARTUS_GOTCHAS, and CLAUDE.md
  // records the exact diagnostic). `--lint-only` does NOT run these, so a clean
  // lint says nothing about them.
  // synthesis translate_off
  initial begin
    if (ATTRS < 7)
      $fatal(1, "zhao_forge_assemble_early_read_mutant: GEOM.CLIP's ruling-5 packet is SEVEN slots; ATTRS=%0d", ATTRS);
    if ((SLOT_INVW >= ATTRS) || (SLOT_UOW >= ATTRS) || (SLOT_VOW >= ATTRS)
        || (SLOT_R >= ATTRS) || (SLOT_G >= ATTRS) || (SLOT_B >= ATTRS)
        || (SLOT_ALPHA >= ATTRS))
      $fatal(1, "zhao_forge_assemble_early_read_mutant: an attribute slot index is outside ATTRS");
    // The projector's result port has no backpressure, so the only thing
    // standing between a burst of offers and a dropped result is this bound.
    // 36 is `zhao_project_core`'s stated GEOM latency.
    if (INFLIGHT < 36)
      $fatal(1, "zhao_forge_assemble_early_read_mutant: INFLIGHT (%0d) must cover the projector's 36-clock latency", INFLIGHT);
    // INFLIGHT MUST BE A POWER OF TWO, and until 2026-09-26 nothing said so.
    //
    // `DQD = INFLIGHT` sizes `dqf_slot_q`/`dqf_w_q`/`dqf_prof_q`, and the
    // pointers are masked with `dqf_wp_q[DQW-1:0]` where `DQW = $clog2(DQD)`.
    // That masking spans 0 .. 2**DQW-1, which equals 0 .. DQD-1 ONLY when DQD
    // is a power of two. At DQD = 40, `DQW = 6`, so the write index reaches 63
    // and the top 24 entries of a 40-deep array DO NOT EXIST: those vertices'
    // (slot, w) are dropped, the converter is handed an out-of-range read, and
    // the canonical depth for those vertices never reaches `inv_q`.
    // `dqf_occ_c = dqf_wp_q - dqf_rp_q` has the same requirement.
    //
    // FOUND BY MEASUREMENT, not by reading. `forge_assemble_directed`'s
    // full-range differential drove all MAX_VERTS slots for the first time and
    // the depths came back ZERO for every index >= 40 -- exactly INFLIGHT --
    // while every counter in the block balanced perfectly: 64 offered, 64
    // landed, 64 `vertices_o`, `dq_refused_o` and `dq_stray_o` both zero. It is
    // CLAUDE.md's own law: no counter looks at the field that moved.
    //
    // The shipping console passes INFLIGHT = 64 and is unaffected. The BENCH
    // passed 40 and had been measuring this fault since it was written.
    if ((INFLIGHT & (INFLIGHT - 1)) != 0)
      $fatal(1, "zhao_forge_assemble_early_read_mutant: INFLIGHT (%0d) must be a POWER OF TWO -- the depth queue's pointers are masked to $clog2(INFLIGHT) bits", INFLIGHT);
    if (MAX_VERTS > 32768)
      $fatal(1, "zhao_forge_assemble_early_read_mutant: MAX_VERTS exceeds the 15-bit rider payload the owner field leaves");
  end
  // synthesis translate_on

  // ==========================================================================
  // THE VERTEX STORES. Two, not one, because the two halves of a vertex arrive
  // on DIFFERENT paths with different latencies and different orderings: the
  // screen position comes back in order on the projector's rigid pipeline, the
  // canonical depth comes back by TOKEN out of the converter and may not. A
  // single store written by both would need one of them to wait for the other.
  // ==========================================================================
  logic [POSW-1:0] pos_q [MAX_VERTS];
  logic [23:0]     inv_q [MAX_VERTS];

  logic [CW-1:0] issued_q;      // offered to the projector
  logic [CW-1:0] pos_land_q;    // screen positions landed
  logic [CW-1:0] inv_land_q;    // canonical depths landed
  logic [CW-1:0] vcount_q;      // the job's vertex count, known at v_last

  typedef enum logic [1:0] { A_COLLECT, A_DRAIN, A_TRIS, A_RETIRE } astate_e;
  astate_e st_q;

  logic [31:0] mset_q;          // the job's material set, in flight
  logic [15:0] mid_q;           // ... and its material id, the SAME latch
  logic [ 1:0] mmode_q;         // ... its material-mode declaration
  logic [ 7:0] mvalpha_q;       // ... its flat per-primitive alpha
  logic [31:0] mstate_q;        // ... and its raster state word
  // The captured sideband, held between the JOB'S ISSUE and its first vertex.
  logic [31:0] jset_q;
  logic [15:0] jid_q;
  logic [ 1:0] jmode_q;
  logic [ 7:0] jvalpha_q;
  logic [31:0] jstate_q;
  logic        jfull_q;
  logic [IDW-1:0] src_q;

  // ==========================================================================
  // THE DEPTH QUEUE -- the projector's un-backpressured result, buffered for a
  // converter that DOES have a ready.
  // ==========================================================================
  localparam int unsigned DQD = INFLIGHT;
  localparam int unsigned DQW = $clog2(DQD);

  logic [VW-1:0]  dqf_slot_q [DQD];
  logic [30:0]    dqf_w_q    [DQD];
  logic [ 1:0]    dqf_prof_q [DQD];
  logic [DQW:0]   dqf_wp_q, dqf_rp_q;

  wire [DQW:0] dqf_occ_c   = dqf_wp_q - dqf_rp_q;
  wire         dqf_empty_c = (dqf_wp_q == dqf_rp_q);

  // In flight at the projector: offered and not yet landed.
  wire [CW-1:0] inflight_c = issued_q - pos_land_q;

  // ROOM IS CHECKED AGAINST BOTH, and that conjunction is the whole safety
  // argument: every request now in flight WILL land, unconditionally, and each
  // one needs a queue entry when it does. Checking the queue's free space alone
  // would let a burst of offers land into a queue that filled behind them.
  wire room_c = ((32'(dqf_occ_c) + 32'(inflight_c)) < 32'(DQD))
             && (32'(issued_q) < 32'(MAX_VERTS));

  // ==========================================================================
  // THE CANONICAL DEPTH. A SECOND INSTANCE OF ONE LAW (ruling D-4), not a
  // second law -- `zhao_geom_vattr` instantiates this exact pair for the mesh
  // path and this block calls the same two modules with the same wiring.
  // ==========================================================================
  wire         dq_v_valid_c = !dqf_empty_c;
  logic        dq_v_ready;
  wire [VW-1:0] dq_v_slot_c = dqf_slot_q[dqf_rp_q[DQW-1:0]];

  logic        dq_d_valid;
  logic [23:0] dq_invw;
  // TAGW is 16 because that is the converter's parameter; the slot this block
  // puts in is VW bits and the rest are zeros it sent itself. The converter
  // ECHOES the tag verbatim, so the high bits are known-zero by construction
  // rather than merely unread -- a counter watching them would be a detector
  // wired to a quantity that cannot move, which is the shape CLAUDE.md's
  // metadata chapter says not to ship.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [15:0] dq_tag;
  /* verilator lint_on UNUSEDSIGNAL */

  logic        rcp_v_valid, rcp_v_ready, rcp_r_valid, rcp_r_ready;
  logic [23:0] rcp_d, rcp_r;
  logic [ 7:0] rcp_v_tok, rcp_r_tok;
  logic [ 5:0] rcp_k;

  /* verilator lint_off UNUSEDSIGNAL */
  // The converter's and the reciprocal's own census. They are read by
  // `dq_refused_o`/`dq_stray_o` where this block owes an answer and left
  // unread where `zhao_geom_vattr` already publishes the same quantity for the
  // same law -- a second copy of a counter is a second number to reconcile.
  logic [31:0] dq_vertices, dq_near, dq_far, dq_sat;
  logic        dq_idle;
  logic [31:0] rcp_accepted, rcp_completed, rcp_mul_jobs, rcp_zero_jobs,
               rcp_phase_jobs, rcp_negcorr_jobs;
  logic [ 5:0] rcp_occupancy;
  logic        rcp_qerr;
  logic        rcp_d_zero, rcp_idle;
  /* verilator lint_on UNUSEDSIGNAL */

  zhao_geom_depthquant_stream #(
      .TAGW (16),
      .NSLOT(DQ_SLOTS)
  ) u_dq (
      .clk           (clk),
      .rst_n         (rst_n),
      .v_valid_i     (dq_v_valid_c),
      .v_ready_o     (dq_v_ready),
      // `v_w_i` is 40 bits and the projector's guarded w is 31. Widened, not
      // converted: the value is placed in the low bits and the rest are zero,
      // which is what `zhao_geom_vattr` does with the same quantity.
      .v_w_i         ({9'd0, dqf_w_q[dqf_rp_q[DQW-1:0]]}),
      .v_profile_i   (dqf_prof_q[dqf_rp_q[DQW-1:0]]),
      .v_tag_i       ({{(16-VW){1'b0}}, dq_v_slot_c}),
      .d_valid_o     (dq_d_valid),
      // ALWAYS READY, and it is not a shortcut: the landing is a write into
      // `inv_q` at the tag's address, which no other writer contends for and
      // which cannot stall. A ready that could fall would need a second queue
      // for a value that already has a home.
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

  // ==========================================================================
  // THE OFFER TO THE PROJECTOR
  //
  // `f_valid_o` READS ONLY THIS BLOCK'S OWN STATE AND `v_valid_i` -- never
  // `f_ready_i`. `zhao_part_project`'s header is emphatic about why: the
  // service's `a_ready_o` is a function of its own `a_valid_i`, so a valid
  // that read its ready would close a combinational loop through two modules.
  // ==========================================================================
  assign f_valid_o = (st_q == A_COLLECT) && v_valid_i && room_c;
  assign v_ready_o = (st_q == A_COLLECT) && room_c && f_ready_i;
  assign f_vx_o    = v_x_i;
  assign f_vy_o    = v_y_i;
  assign f_vz_o    = v_z_i;
  assign f_view_o  = view_sel_i;
  assign f_slot_o  = {{(15-VW){1'b0}}, issued_q[VW-1:0]};

  wire v_take_c = v_valid_i && v_ready_o;

  // ==========================================================================
  // THE TRIANGLE WALK. Three reads of one store, then one offer.
  // ==========================================================================
  typedef enum logic [2:0] { T_IDLE, T_R0, T_R1, T_R2, T_OFFER } tstate_e;
  tstate_e tst_q;

  logic signed [20:0] ax_q, ay_q, bx_q, by_q, cx_q, cy_q;
  logic [2:0]         behind_q;
  logic [23:0]        iw_a_q, iw_b_q, iw_c_q;
  logic               tlast_q;
  logic [VW-1:0]      rd_a_q;
  // The triple's second and third indices, LATCHED at the accept. The index
  // stream's handshake completes on that cycle, so `t_i1_i`/`t_i2_i` are the
  // next triple's by the time the walk needs them -- reading the port here
  // instead of the register is the classic one-cycle-late fault and it would
  // build a triangle from three different triples' corners.
  logic [VW-1:0]      i1_q, i2_q;

  // THE READ REGISTER MOVED FROM THE ADDRESS TO THE DATA (FLOPARRAY, 2026-09-26).
  //
  // This used to be
  //     wire [POSW-1:0] pos_rd_c = pos_q[rd_a_q];
  //     wire [23:0]     inv_rd_c = inv_q[rd_a_q];
  // -- a continuous assignment through a dynamic index, which is
  // `check_ram_inference.py` rule 2 and forces a per-bit mux the width of the
  // array. With the asynchronous reset loop over `pos_q`/`inv_q` it held 34,840
  // bits in flip-flops, about 21% of the shipping part.
  //
  // WHY THIS COSTS NOTHING IN CLOCKS, which is NOT true of every such
  // conversion and is the whole reason this one was safe to make. The address
  // was ALREADY registered a full cycle ahead of its use: `rd_a_q` is loaded in
  // T_IDLE/T_R0/T_R1 and `pos_rd_c` was consumed on the FOLLOWING edge. So the
  // register is not being ADDED, it is being MOVED across the array -- from the
  // address side to the data side. `pos_q[rd_a_c]` sampled on edge N is by
  // construction the same element `pos_q[rd_a_q]` would have presented
  // combinationally on edge N+1, because `rd_a_c` IS `rd_a_q`'s next value.
  //
  // A block whose address is NOT already registered cannot do this: it would
  // have to insert a stage. `zhao_geom_lodstate` is exactly that case and pays
  // a clock for it. Do not read this comment as a general licence.
  logic [POSW-1:0] pos_rd_q;
  logic [23:0]     inv_rd_q;

  // A triple naming a vertex this job never emitted is REFUSED, never wrapped.
  // `zhao_forge_prim` builds its indices from its own `eff_seg_c`/`ring_q`, so
  // this can only fire if the two halves disagreed about the topology -- which
  // is precisely the ordering-convention fault nothing else in the tree can
  // see, and the reason this counter exists.
  wire idx_bad_c = (32'(t_i0_i) >= 32'(vcount_q))
                || (32'(t_i1_i) >= 32'(vcount_q))
                || (32'(t_i2_i) >= 32'(vcount_q));

  // `&& !tlast_q` IS A REPAIR, AND `mat_skew_o` IS WHAT FOUND IT. The walk
  // leaves A_TRIS on the cycle AFTER the last triple retires, so without this
  // term there is a one-cycle window in which the job's last triangle has
  // already been taken and `t_ready_o` is STILL HIGH. The page bank may have
  // issued the next job by then -- it is allowed to, this block is busy -- so
  // FORGE.PRIM offers the NEXT primitive's first triple into that window and
  // this block reads it against THE PREVIOUS JOB'S VERTEX STORE, under the
  // previous job's material. Two such triangles per five-draw run, measured by
  // `procmat_acceptance`; every handshake balanced, because a stolen triple is
  // a perfectly well-formed beat.
  assign t_ready_o = (st_q == A_TRIS) && (tst_q == T_IDLE) && !tlast_q;

  assign o_valid_o = (tst_q == T_OFFER);
  assign o_ax_o = ax_q;
  assign o_ay_o = ay_q;
  assign o_bx_o = bx_q;
  assign o_by_o = by_q;
  assign o_cx_o = cx_q;
  assign o_cy_o = cy_q;
  assign o_behind_o = behind_q;
  assign o_src_id_o = src_q;

  // R197: a forge primitive has NO u/v BY LAW. This is the declaration the
  // core's door judges against the published material's sample count, and it
  // is a constant here because it is a fact about this producer rather than a
  // property of a particular primitive.
  assign o_untex_o     = 1'b1;
  assign o_cull_mode_o = art_cull_mode_i;

  assign o_material_set_o  = mset_q;
  assign o_material_id_o   = mid_q;
  assign o_material_mode_o = mmode_q;
  assign o_vertex_alpha_o  = mvalpha_q;
  assign o_frag_state_o    = mstate_q;
  assign o_quality_tier_o  = art_quality_tier_i;

  // The ruling-5 packet, per corner. Slot 0 in the LOW 32 bits, which is the
  // order `zhao_console_core` builds GEOM.REPLAY's packet in and asserts with
  // its own elaboration guard.
  function automatic logic [ATTRS*32-1:0] pack_attr(input logic [23:0] invw);
    logic [ATTRS*32-1:0] w;
    begin
      w = '0;
      w[32*SLOT_INVW  +: 32] = {8'd0, invw};
      w[32*SLOT_UOW   +: 32] = 32'd0;   // R197: replaced by the zero operand
      w[32*SLOT_VOW   +: 32] = 32'd0;   // downstream; agreed with, not relied on
      w[32*SLOT_R     +: 32] = art_r_i;
      w[32*SLOT_G     +: 32] = art_g_i;
      w[32*SLOT_B     +: 32] = art_b_i;
      w[32*SLOT_ALPHA +: 32] = art_alpha_i;
      pack_attr = w;
    end
  endfunction

  assign o_attr_a_o = pack_attr(iw_a_q);
  assign o_attr_b_o = pack_attr(iw_b_q);
  assign o_attr_c_o = pack_attr(iw_c_q);

  assign busy_o = (st_q != A_COLLECT) || (32'(issued_q) != 32'd0);

  // THE SIDEBAND'S READY. Two terms, and the second is what keeps a
  // VIEW-SKIPPED job from wedging the bank: such a job produces no vertices at
  // all, so its captured pair is never consumed, and a strict `!jfull_q` would
  // refuse every sideband after it forever. While this block is NOT BUSY the
  // held pair belongs to no primitive and may simply be replaced.
  //
  // AND THE DANGEROUS CASE CANNOT ARISE: the bank reaches its next S_ISSUE only
  // when BOTH evaluators are ready, and an evaluator is ready only once it has
  // emitted the previous job's last vertex -- by which time this block has
  // accepted vertex 0, is busy, and has already consumed the pair.
  assign j_ready_o = !jfull_q || !busy_o;

  wire o_take_c = o_valid_o && o_ready_i;

  // ==========================================================================
  // THE VERTEX STORES' MEMORY PORTS.
  //
  // In their OWN clocked block, with NO RESET ON THE ARRAYS, because an
  // asynchronous clear over an array is what stops Quartus inferring M10K --
  // `zhao_geom_drawjob.sv:402`, "The array itself has NO reset, which is what
  // lets it infer M10K", and this block used to do precisely the thing that
  // comment warns about.
  //
  // THE VALIDITY DISCIPLINE THAT REPLACES THE RESET LOOP. The loop was doing
  // real work -- it made every slot readable after reset -- so deleting it
  // needs an argument, not a shrug. Here it is a PROOF that no read can precede
  // the write of the element it reads, and it rests on two barriers that were
  // already in the block for other reasons:
  //
  //   1. `t_ready_o` (:601) is gated on `st_q == A_TRIS`, and A_DRAIN (:792)
  //      refuses to enter A_TRIS until `(pos_land_q == vcount_q) && (inv_land_q
  //      == vcount_q)` -- BOTH halves of every vertex this job issued have
  //      landed. So when the walk reads, slots 0..vcount_q-1 have all been
  //      written THIS job.
  //   2. `idx_bad_c` (:587) refuses, and counts, any triple naming an index
  //      >= `vcount_q`, so no read is ever issued outside that written range.
  //
  // `zhao_geom_drawjob` keeps per-row valid bits because it genuinely can be
  // asked for a transform nobody programmed; this block cannot, because the
  // barrier is structural. The assertion below is the instrument that holds
  // that claim to account rather than leaving it as prose.
  //
  // WHY AN ASSERTION AND NOT A COUNTER PORT, which is the shape this tree
  // normally uses. A new `output var logic [31:0]` costs its whole
  // instantiation chain -- `zhao_prod_top`, the console core, the board, the
  // paired-diff generator and every bench that elaborates this module -- and
  // the brief's equivalence fence is "same refusal semantics, same counters,
  // same handshake". The barrier it watches is checked on EVERY run of
  // `forge_assemble_directed` this way, which is more often than any counter
  // in this block is read, and the committed mutant
  // `tests/mutants/zhao_forge_assemble_early_read_mutant.sv` breaks A_DRAIN so
  // the assertion can be SEEN to fire rather than argued about.
  //
  // The same barrier is why the inferred RAM's read-during-write behaviour is
  // not a hazard: writes land in A_COLLECT/A_DRAIN and reads happen in A_TRIS,
  // so the read and write addresses cannot be live on the same edge.
  wire [VW-1:0] rd_a_c =
        ((tst_q == T_IDLE) && t_valid_i && t_ready_o && !idx_bad_c) ? t_i0_i[VW-1:0]
      : (tst_q == T_R0) ? i1_q
      : (tst_q == T_R1) ? i2_q
      :                   rd_a_q;

  always_ff @(posedge clk) begin
    if (rs_valid_i && (32'(rs_slot_i) < 32'(MAX_VERTS)))
      pos_q[rs_slot_i[VW-1:0]] <= {rs_behind_i, rs_y_i, rs_x_i};
    if (dq_d_valid)
      inv_q[dq_tag[VW-1:0]] <= dq_invw;
    pos_rd_q <= pos_q[rd_a_c];
    inv_rd_q <= inv_q[rd_a_c];
  end

  // THE BARRIER, ASSERTED. Note that `// synthesis translate_off` does NOT
  // cause a skip of the block it wraps -- CLAUDE.md proved that by planting a
  // syntax error inside one and watching lint reject it -- so this guard is
  // LIVE in every simulation while costing the fitter nothing.
  //
  // (And a comment line whose FIRST word is the simulator's name is read as a
  // pragma and rejected, which is why this paragraph is worded around it. That
  // trap is in CLAUDE.md and this packet walked into it anyway.)
  // synthesis translate_off
  // `$error`, NOT `$fatal`, AND THE REASON IS MEASURED. `$fatal` aborts, and an
  // abort on this toolchain blocks with a fault dialog when stdout is a pipe:
  // the mutant control below ran to completion standalone and TIMED OUT AT 300
  // SECONDS under ctest, which is a gate that reports "hung" instead of
  // "caught it". `$error` prints the same sentence, the run terminates
  // normally, and ctest can read the output -- which is the whole point of
  // having the guard.
  //
  // ONE-SHOT: this sits in a clocked block and the condition persists for the
  // whole walk, so an unlatched report would bury the log in thousands of
  // copies of one fault.
  logic barrier_reported_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      barrier_reported_q <= 1'b0;
    end else begin
      // No `rst_n` term in the condition: reset drives `tst_q` to T_IDLE, so
      // the first conjunct already excludes the reset state.
      if (!barrier_reported_q && (tst_q != T_IDLE) && (tst_q != T_OFFER)
          && ((pos_land_q != vcount_q) || (inv_land_q != vcount_q))) begin
        barrier_reported_q <= 1'b1;
        // MUTATION 2: $error -> $display, IN THE MUTANT ONLY. Verilator treats
        // $error under --assert as an assertion failure, calls $stop and ABORTS,
        // and an abort here blocks on a fault dialog when stdout is a pipe -- the
        // ctest reported "Timeout 300.02 sec" instead of catching anything.
        $display("zhao_forge_assemble_early_read_mutant: the vertex store was READ BEFORE THE BARRIER RELEASED (tst=%0d pos_land=%0d inv_land=%0d vcount=%0d). pos_q/inv_q have no reset, so an early read returns undefined data rather than zero.",
               tst_q, pos_land_q, inv_land_q, vcount_q);
      end
    end
  end
  // synthesis translate_on

  // ==========================================================================
  // THE MACHINE
  // ==========================================================================
  // (`integer k` used to live here, and existed ONLY to drive the reset loop
  // over `pos_q`/`inv_q`. The loop is gone, so it is too.)
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_q            <= A_COLLECT;
      tst_q           <= T_IDLE;
      issued_q        <= '0;
      pos_land_q      <= '0;
      inv_land_q      <= '0;
      vcount_q        <= '0;
      dqf_wp_q        <= '0;
      dqf_rp_q        <= '0;
      mset_q          <= 32'd0;
      mid_q           <= 16'd0;
      mmode_q         <= 2'd0;
      mvalpha_q       <= 8'd0;
      mstate_q        <= 32'd0;
      src_q           <= '0;
      ax_q <= 21'sd0; ay_q <= 21'sd0;
      bx_q <= 21'sd0; by_q <= 21'sd0;
      cx_q <= 21'sd0; cy_q <= 21'sd0;
      behind_q        <= 3'd0;
      iw_a_q          <= 24'd0;
      iw_b_q          <= 24'd0;
      iw_c_q          <= 24'd0;
      tlast_q         <= 1'b0;
      rd_a_q          <= '0;
      i1_q            <= '0;
      i2_q            <= '0;
      jobs_o          <= 32'd0;
      vertices_o      <= 32'd0;
      triangles_o     <= 32'd0;
      index_oor_o     <= 32'd0;
      vtx_overflow_o  <= 32'd0;
      slot_pressure_o <= 32'd0;
      proj_stray_o    <= 32'd0;
      mat_skew_o      <= 32'd0;
      jset_q          <= 32'd0;
      jid_q           <= 16'd0;
      jmode_q         <= 2'd0;
      jvalpha_q       <= 8'd0;
      jstate_q        <= 32'd0;
      jfull_q         <= 1'b0;
      // THE RESET LOOP OVER `pos_q`/`inv_q` IS GONE, deliberately. It used to
      // read
      //     for (k = 0; k < MAX_VERTS; k = k + 1) begin
      //       pos_q[k] <= '0;
      //       inv_q[k] <= 24'd0;
      //     end
      // and it is the reason 34,840 bits sat in flip-flops. What replaces it is
      // the A_DRAIN / `idx_bad_c` barrier documented at the memory ports above,
      // plus `vtx_read_early_o`, which counts any attempt to read before that
      // barrier says the data is there.
    end else begin

      // ---- a vertex offered and refused for room ---------------------------
      if ((st_q == A_COLLECT) && v_valid_i && !room_c) begin
        if (32'(issued_q) >= 32'(MAX_VERTS)) begin
          // MORE VERTICES THAN THE STORE HOLDS. The evaluators' own bounds make
          // this unreachable with a legal job -- (MAX_SEGMENTS+1) * MAX_SIDES
          // is exactly MAX_VERTS -- so it is a guard against a FUTURE bound
          // being raised on one side only, which is how two frozen numbers
          // drift apart.
          //
          // CORRECTED 2026-09-26. This comment used to say the counter "owes a
          // committed mutant rather than an argument, and
          // `tests/mutants/zhao_forge_assemble_mutant.sv` is it." THAT FILE HAS
          // NEVER EXISTED -- the name appears nowhere in this tree except in
          // the sentence asserting it. It is also not owed one: unlike the
          // barrier assertion above, this counter IS reachable with legal
          // stimulus, and `forge_assemble_directed` section 6 both fires it
          // (one vertex past MAX_VERTS) and holds it silent at exactly
          // MAX_VERTS, which is the discrimination pair a mutant would only
          // half provide.
          if (vtx_overflow_o != 32'hffff_ffff)
            vtx_overflow_o <= vtx_overflow_o + 32'd1;
        end else if (slot_pressure_o != 32'hffff_ffff) begin
          slot_pressure_o <= slot_pressure_o + 32'd1;
        end
      end

      // ---- the projector's un-refusable result -----------------------------
      if (rs_valid_i) begin
        if (32'(rs_slot_i) < 32'(MAX_VERTS)) begin
          // `pos_q` is written at the memory ports above, under this exact
          // condition. The counters stay here; only the ARRAY moved, so that it
          // lives in a block with no asynchronous reset.
          dqf_slot_q[dqf_wp_q[DQW-1:0]] <= rs_slot_i[VW-1:0];
          dqf_w_q   [dqf_wp_q[DQW-1:0]] <= rs_w_i;
          dqf_prof_q[dqf_wp_q[DQW-1:0]] <= rs_profile_i;
          dqf_wp_q   <= dqf_wp_q + {{DQW{1'b0}}, 1'b1};
          pos_land_q <= pos_land_q + {{VW{1'b0}}, 1'b1};
        end else if (proj_stray_o != 32'hffff_ffff) begin
          // A result carrying a slot this block never issued. The rider is the
          // only thing that says whose result this is, so a stray one is the
          // demux having sent us somebody else's -- a real fault, counted where
          // it is visible rather than dropped where it is not.
          proj_stray_o <= proj_stray_o + 32'd1;
        end
      end

      // ---- the depth queue's drain ------------------------------------------
      if (dq_v_valid_c && dq_v_ready) begin
        dqf_rp_q <= dqf_rp_q + {{DQW{1'b0}}, 1'b1};
      end

      // ---- the canonical depth's landing ------------------------------------
      if (dq_d_valid) begin
        // `inv_q` is written at the memory ports above, under this exact
        // condition. See the note on `pos_q`.
        inv_land_q <= inv_land_q + {{VW{1'b0}}, 1'b1};
        if (vertices_o != 32'hffff_ffff) vertices_o <= vertices_o + 32'd1;
      end

      // ---- the outer phase ---------------------------------------------------
      unique case (st_q)
        A_COLLECT: begin
          if (v_take_c) begin
            issued_q <= issued_q + {{VW{1'b0}}, 1'b1};
            if (32'(issued_q) == 32'd0) begin
              // THE JOB'S SIDEBAND IS LATCHED HERE, at the first vertex, and
              // `busy_o` rises with it so the bank cannot replace it underneath.
              // ONE ENABLE, BOTH HALVES, AND BOTH FROM THE CAPTURED
              // SIDEBAND -- not from the bank's live ports, which by now may
              // already describe the next draw. See the header.
              mset_q    <= jset_q;
              mid_q     <= jid_q;
              mmode_q   <= jmode_q;
              mvalpha_q <= jvalpha_q;
              mstate_q  <= jstate_q;
              jfull_q   <= 1'b0;
            end
            if (v_last_i) begin
              vcount_q <= issued_q + {{VW{1'b0}}, 1'b1};
              st_q     <= A_DRAIN;
            end
          end
        end

        A_DRAIN: begin
          // BOTH halves of every vertex, not one. The screen position and the
          // canonical depth come back on different paths and a triangle built
          // from a landed position and an unlanded depth would carry the
          // PREVIOUS job's invw24 -- the stale-copy fault, per corner.
          if (pos_land_q == vcount_q) begin   // MUTATION 1: the inv_land_q half of the barrier is DROPPED
            st_q <= A_TRIS;
          end
        end

        A_TRIS: begin
          if (tlast_q && (tst_q == T_IDLE)) begin
            st_q <= A_RETIRE;
          end
        end

        A_RETIRE: begin
          issued_q   <= '0;
          pos_land_q <= '0;
          inv_land_q <= '0;
          vcount_q   <= '0;
          tlast_q    <= 1'b0;
          if (jobs_o != 32'hffff_ffff) jobs_o <= jobs_o + 32'd1;
          st_q <= A_COLLECT;
        end

        default: st_q <= A_COLLECT;
      endcase

      // ---- the triangle walk -------------------------------------------------
      // The read address register. It is loaded UNCONDITIONALLY from `rd_a_c`,
      // whose default arm is `rd_a_q` itself, so this is exactly the hold-else-
      // load behaviour the three per-state assignments used to express. Those
      // assignments moved into `rd_a_c` so that the ARRAY could be indexed by
      // the next address rather than the current one -- see the memory ports.
      rd_a_q <= rd_a_c;

      unique case (tst_q)
        T_IDLE: begin
          if (t_valid_i && t_ready_o) begin
            // THE DETECTOR, NOT THE SOURCE. `mid_q` is the job's own id and
            // is NOT written here -- the triangle stream's copy travelled a
            // different path with a different cadence, so a disagreement is a
            // real skew and the job's value is the one that wins.
            //
            // R95, discrimination: this counter moves ONLY on a mismatch, and
            // `forge_assemble_directed` asserts it stays PUT across a job whose
            // ids agree, INCLUDING a job whose id is zero -- zero is a valid
            // record under ruling 2 and must not read as "unset".
            if (t_material_i != mid_q) begin
              if (mat_skew_o != 32'hffff_ffff) mat_skew_o <= mat_skew_o + 32'd1;
            end
            src_q   <= t_src_id_i;
            tlast_q <= t_last_i;
            if (idx_bad_c) begin
              // REFUSED AND COUNTED, never wrapped into the store. A wrapped
              // index reads a real vertex of the same job and draws a triangle
              // nobody authored, which no downstream check could see.
              if (index_oor_o != 32'hffff_ffff)
                index_oor_o <= index_oor_o + 32'd1;
            end else begin
              // `rd_a_q` is loaded from `rd_a_c`, whose T_IDLE arm is this very
              // expression under this very condition.
              i1_q   <= t_i1_i[VW-1:0];
              i2_q   <= t_i2_i[VW-1:0];
              tst_q  <= T_R0;
            end
          end
        end

        T_R0: begin
          ax_q     <= $signed(pos_rd_q[20:0]);
          ay_q     <= $signed(pos_rd_q[41:21]);
          behind_q[0] <= pos_rd_q[42];
          iw_a_q   <= inv_rd_q;
          tst_q    <= T_R1;
        end

        T_R1: begin
          bx_q     <= $signed(pos_rd_q[20:0]);
          by_q     <= $signed(pos_rd_q[41:21]);
          behind_q[1] <= pos_rd_q[42];
          iw_b_q   <= inv_rd_q;
          tst_q    <= T_R2;
        end

        T_R2: begin
          cx_q     <= $signed(pos_rd_q[20:0]);
          cy_q     <= $signed(pos_rd_q[41:21]);
          behind_q[2] <= pos_rd_q[42];
          iw_c_q   <= inv_rd_q;
          tst_q    <= T_OFFER;
        end

        T_OFFER: begin
          if (o_take_c) begin
            if (triangles_o != 32'hffff_ffff) triangles_o <= triangles_o + 32'd1;
            tst_q <= T_IDLE;
          end
        end

        default: tst_q <= T_IDLE;
      endcase

      // ---- the job sideband's capture ---------------------------------------
      // LAST in the block on purpose. If a capture and a vertex-0 consumption
      // ever landed on one edge, the consumption must see the OLD pair (it
      // does -- non-blocking) and the register must end holding the NEW one.
      // The ordering argument above says they cannot coincide; this makes the
      // block correct even if that argument is ever broken by a faster
      // evaluator, rather than merely unlikely to be wrong.
      if (j_valid_i && j_ready_o) begin
        jset_q    <= j_material_set_i;
        jid_q     <= j_material_id_i;
        jmode_q   <= j_material_mode_i;
        jvalpha_q <= j_vertex_alpha_i;
        jstate_q  <= j_frag_state_i;
        jfull_q   <= 1'b1;
      end
    end
  end

endmodule : zhao_forge_assemble_early_read_mutant

`default_nettype wire
