// zhao_geom_lodstate.sv -- THE PER-INSTANCE LADDER STATE, and the owner
// `zhao_geom_lod` has been waiting for since phase 8. Owner ruling R68,
// sub-build 3.
//
// ENFORCED-BY: tests/geometry/geom_lodstate_directed.cpp:main
// REFERENCE:   zref::creature::lod_update / zref::creature::LodState
//              (reference/include/zref/zref_creature.hpp)
//
// ---------------------------------------------------------------------------
// THE FIFTEENTH FALSE-ABSENCE CLAIM IS WHAT THIS BLOCK ANSWERS
// ---------------------------------------------------------------------------
// `fpga/rtl/prod/zhao_console_core.sv` records it in full, and it is worth
// restating because it is the reason this file exists:
//
//   "THERE IS NO LodState IN `zhao_geom_meshfetch`. The strings `rung`, `lod`
//    and `LodState` occur exactly ONCE in all 505 lines of that file, in a
//    header comment at line 18 pointing AT `zhao_geom_lod.sv` ... So the rung
//    is not unported state: NOBODY HOLDS LADDER STATE AT ALL, and
//    `zhao_geom_lod` is a built, stateless evaluator waiting for an owner."
//
// `zhao_geom_lod`'s own header says the same thing from the other side: "LADDER
// STATE IS THE CALLER'S. GEOM.MESHFETCH holds one LodState per live instance,
// so this block takes the state in and hands the stepped state back rather than
// owning an array whose size it cannot know." There was no such caller. This is
// it, and the array's size is `INSTANCES`, which is a parameter for exactly the
// reason that sentence gives.
//
// ---------------------------------------------------------------------------
// WHERE THE TICK COMES FROM, AND WHY IT IS A TAP AND NOT A STAGE
// ---------------------------------------------------------------------------
// The ladder wants, once per creature instance per frame: the instance's WORLD
// CENTRE, its creature TYPE, and its previous rung. All three are together in
// exactly one place in this console -- the job `zhao_geom_drawjob` hands
// `zhao_geom_meshfetch`. That packet carries `j_instance_id`, the draw's form
// handle, and `j_xform[12]`, whose translation column (elements 3, 7 and 11) IS
// the instance's world position.
//
// So this block OBSERVES that handshake rather than sitting in it. The
// alternative -- joining GEOM.MESHFETCH's `cull_c{x,y,z}_o` at S_CULL to its
// `r_instance_id_o` at S_EMIT -- is a join across pipeline stages, and while
// that particular one is provable (the block is a strict single-in-flight FSM),
// core entry I39 refuses exactly this shape by name and the job packet makes
// the join unnecessary: one handshake, all three fields, no timing argument.
//
// A TAP CANNOT STALL WHAT IT WATCHES. `j_fire_i` is an observation, never a
// term in the producer's `ready`. That is deliberate and it has a price: a job
// arriving while an evaluation is in flight is DROPPED AND COUNTED
// (`dropped_o`), not queued. See the rate argument below for why the drop
// counter is expected to read zero and is still not allowed to be silent.
//
// ---------------------------------------------------------------------------
// ONE TICK PER INSTANCE PER FRAME, ESTABLISHED STRUCTURALLY
// ---------------------------------------------------------------------------
// `zref::creature::kLodHoldTicks` is FIFTEEN TICKS, and `creature_sim` calls
// `lod_update` once per instance per frame -- so a tick is a FRAME, and a block
// that ticked per MESHLET would burn the whole minimum hold in a single frame
// of one creature and turn charter section 9's stability law into noise.
//
// `zhao_geom_drawjob` emits every meshlet of one draw consecutively (it walks
// the mesh stream's descriptors to completion before accepting the next
// DrawForm), so the FIRST job of a draw is recognisable without a frame-scoped
// bitmap: it is the one whose instance id differs from the previous accepted
// job's. That is a property of the producer's FSM, not of a workload, which is
// why `skipped_repeat_o` counts the rest rather than a filter claiming to.
//
// A game that issues two DrawForms for one instance in a frame ticks twice.
// That is the game's statement that the instance was drawn twice and this block
// does not second-guess it; `frame_i` exists to clear the run marker at the
// frame boundary so the first draw of the NEXT frame always ticks, even when it
// names the same instance as the last draw of this one.
//
// ---------------------------------------------------------------------------
// ONE LodState PER (INSTANCE, CAMERA) -- OWNER RULING R74 / D-LADDER-A
// ---------------------------------------------------------------------------
// THIS BLOCK USED TO HOLD ONE LodState PER INSTANCE AND SAY SO AS A DOCKED
// DEVIATION: "`zref::creature::CreatureInstance` holds ONE `LodState`, and
// `lod_update` takes one threshold ... The creature reference does NOT say
// that, and nothing ratifies it for creatures ... It is an owner decision, and
// the cost of changing it is one more index bit on the store."
//
// **THE OWNER RULED, AND THE RULING IS NOW BUILT.**
// `reports/OWNER-RULINGS-20260919-EVENING.md` R74 and its D-LADDER-A section:
//
//   "D-LADDER-A -- the per-instance ladder PAYS THE CAMERA INDEX BIT.
//    Decided: one more bit x INSTANCES, so each creature's ladder measures
//    against the right camera ... For `active_mask == 2'b11` there was no
//    honest answer in the tree ... SHADOWSUB's recommendation was to pay the
//    bit, and the owner agreed."
//
// The failure mode it buys off, in the ruling's own words, is "a creature that
// pops LOD rungs in the second view for reasons nothing records" -- one shared
// ladder lets player 1's camera coarsen player 2's creature, a Duo FAIRNESS
// defect under charter section 9. It also makes creatures select LOD the same
// way particles already do (PART.LADDER 2026-08-31 section 2.5), which is one
// fewer place where two subsystems do the same thing differently.
//
// WHAT CHANGED, CONCRETELY:
//
//   * `st_q` is `INSTANCES * 2` deep, indexed `{view, instance}`. That IS the
//     "one more bit x INSTANCES" the ruling priced.
//   * THE PORT IS THE MASK, NOT A BIT. `j_view_mask_i [1:0]` replaces
//     `j_view_i`, because `zhao_geom_drawjob` emits `j_active_mask_o [1:0]`
//     and there is no single-bit field on that seam to read. A composer
//     narrowing the mask to a bit would be making the very choice this ruling
//     took away from it.
//   * A DUAL-VIEW JOB EVALUATES TWICE, once per set bit, each against its own
//     threshold, its own `kx`/`vw` and its own stored ladder. The second pass
//     re-enters at S_PROJ -- the bank row is the same form and is already
//     latched, but the CENTRE PROJECTION and the RADIUS are per-camera and
//     must be redone. Skipping them would be measuring camera 1's creature
//     with camera 0's arithmetic, which is the defect wearing the fix's
//     clothes.
//   * TWO CASTERS COME OUT, each tagged with its `c_view_o`. That is correct
//     rather than duplicated: if the two cameras chose different rungs the
//     shadow hulls are genuinely different geometry, and `c_view_o` is the
//     field that says which view each belongs to. It is why that port exists.
//
// THE RATE, RE-DERIVED RATHER THAN INHERITED (see below for the single-view
// figure this replaces): a dual-view instance is two evaluations, so 256
// instances all drawn in both views is 512 evaluations and 512 client-A
// requests. `design/prod_manifest.yml:1531` already prices the radius service
// at exactly that -- "512 at the content tier" -- so this ruling brings the
// block into line with a figure the manifest had already assumed.
// `reports/R3-CLIENT-A-SCHEDULE-PROOF-20260923.md` measures what those 512
// requests cost the shared projector: 0.031% of the frame, and a worst-case
// arbitration wait of 4 clocks against a fully saturated machine.
//
// A MASK OF `2'b00` EVALUATES NOTHING. `zhao_geom_drawjob` does not emit a job
// for a fully masked draw (its own `masked_o` counts them), so this is a
// defensive case rather than a live one -- but it is written as a case instead
// of assumed away, because "the producer never does that" is how a block
// acquires an unreachable state nobody can name.
//
// ---------------------------------------------------------------------------
// WHAT IT INSTANTIATES AND WHAT IT TAKES AS A PORT
// ---------------------------------------------------------------------------
// INSIDE: `zhao_geom_lod` (the ladder) and `zhao_geom_projradius` (the
// projected bound radius). Both have exactly one customer in this machine and
// no other, so a port for either would be a wire with one end.
//
// OUTSIDE: the ladder-constant bank (`zhao_geom_ladderbank`), because it is a
// RESIDENT STORE fed by MEM.UPLOAD's publication and that is a different
// concern with a different lifetime; and the CENTRE PROJECTION, because
// `w` for the instance centre comes from the shared projector's client A and
// owner ruling R3 keeps that a time-multiplex rather than a third port -- so
// the multiplexer is the composer's, not this block's.
//
// ---------------------------------------------------------------------------
// THE RATE
// ---------------------------------------------------------------------------
// One evaluation is: a bank lookup (2 clocks), the centre projection (the
// projector's 36-clock core plus arbitration), the radius (121 clocks) and the
// ladder (5 clocks) -- call it 200. At `zhao_geom_drawjob`'s content tier of
// 256 instance transforms that is 51,200 clocks of the frame's 1,666,666, or
// 3.1%. `zhao_geom_meshfetch`'s own meshlet loop is 305 clocks per meshlet and
// a creature is many meshlets, so the tap is not the thing that can fall
// behind. `dropped_o` is the number that says whether that reasoning held.
//
// UNDER D-LADDER-A THAT FIGURE IS THE SINGLE-VIEW CASE AND IS NO LONGER THE
// WORST ONE. A dual-view job re-enters at S_PROJ, so its second pass costs the
// projection + radius + ladder but NOT the bank lookup -- about 164 clocks
// against the first pass's ~166. So 256 instances all drawn in BOTH views is
// roughly 256 * 330 = 84,480 clocks, **5.1% of the frame**, against 3.1% for
// the single-view case. Both are comfortably inside the tap's headroom, and
// `dropped_o` remains the counter that would say otherwise.
//
// Conservative SystemVerilog subset only (charter section 2).
`default_nettype none

module zhao_geom_lodstate #(
    // Live creature instances whose ladder state is held. NAMED AND EDITABLE
    // (CLAUDE.md rule 6). 256 matches `zhao_geom_drawjob`'s XFORMS, which owner
    // ruling R29's content tier sets; an instance id at or above this is
    // REFUSED and counted, never wrapped -- wrapping would give two creatures
    // one ladder and make each other's hysteresis look like a bug.
    parameter int unsigned INSTANCES = 256,
    parameter int unsigned IIDW = $clog2(INSTANCES)
) (
    input var logic clk,
    input var logic rst_n,

    // ---- the frame boundary -------------------------------------------------
    // A one-cycle pulse. It clears the run marker so the first draw of a new
    // frame always ticks, even when it names the instance the last draw of the
    // previous frame named.
    input var logic frame_i,

    // ---- the job tap: GEOM.DRAWJOB -> GEOM.MESHFETCH, OBSERVED --------------
    // `j_fire_i` is that handshake's (valid && ready). Nothing here is a term
    // in it.
    input var logic               j_fire_i,
    input var logic        [15:0] j_instance_id_i,
    input var logic        [23:0] j_form_index_i,
    // The instance transform's TRANSLATION COLUMN -- xform[3], xform[7],
    // xform[11] of the row-major 3x4 -- which is the instance's world position.
    input var logic signed [31:0] j_cx_i,
    input var logic signed [31:0] j_cy_i,
    input var logic signed [31:0] j_cz_i,
    // WHICH CAMERAS DRAW THIS INSTANCE -- the two-view MASK, exactly as
    // `zhao_geom_drawjob` emits it on `j_active_mask_o [1:0]`. Owner ruling
    // R74 / D-LADDER-A: one ladder per (instance, camera), so a job with both
    // bits set is evaluated TWICE, each pass against its own camera's
    // threshold and its own stored ladder. See the header.
    //
    // IT IS THE MASK AND NOT A BIT BECAUSE THE SEAM HAS NO BIT. The job
    // handshake carries `j_active_mask_o` and nothing else about views; a
    // single-bit port here would oblige the composer to narrow two bits to one
    // and that narrowing IS the decision D-LADDER-A removed from it.
    input var logic        [ 1:0] j_view_mask_i,

    // ---- the ladder-constant bank (zhao_geom_ladderbank) -------------------
    output var logic               q_valid_o,
    output var logic        [23:0] q_form_o,
    input  var logic               a_valid_i,
    input  var logic               a_hit_i,
    input  var logic signed [31:0] a_bound_i,
    input  var logic signed [31:0] a_micro_i,
    input  var logic signed [31:0] a_splat_i,
    input  var logic signed [31:0] a_glint_i,

    // ---- the instance centre, through the shared projector's client A ------
    // Owner ruling R3 keeps client A a TIME-MULTIPLEX, so the multiplexer is
    // the composer's and this is a request/answer pair, not a projector.
    output var logic               pr_valid_o,
    input  var logic               pr_ready_i,
    output var logic signed [31:0] pr_x_o,
    output var logic signed [31:0] pr_y_o,
    output var logic signed [31:0] pr_z_o,
    output var logic               pr_view_o,
    input  var logic               pr_ans_valid_i,
    input  var logic        [30:0] pr_w_i,
    input  var logic               pr_behind_i,

    // ---- the view terms, from zhao_view_projscale --------------------------
    input var logic [31:0] kx0_i,
    input var logic [31:0] kx1_i,
    input var logic [11:0] vw0_i,
    input var logic [11:0] vw1_i,

    // ---- the governor's per-camera threshold (owner ruling R26) ------------
    input var logic signed [31:0] thresh0_i,
    input var logic signed [31:0] thresh1_i,

    // ---- the caster out: FORGE.SHADOW's {world x, z, radius, rung, src_id} --
    // `c_radius_o` is the creature TYPE's bind-pose bound radius in fx16 world
    // metres -- the same number the ladder divides by. It is NOT a contact
    // footprint: turning a bounding sphere into the ellipse a shadow occupies
    // is an ART decision, and CLAUDE.md rule 6 puts it in a named, editable
    // constant at FORGE.SHADOW's own composition, beside the depth biases that
    // already live there. This block emits the measured quantity and invents
    // no factor.
    output var logic               c_valid_o,
    input  var logic               c_ready_i,
    output var logic        [15:0] c_instance_id_o,
    output var logic signed [31:0] c_x_o,
    output var logic signed [31:0] c_z_o,
    output var logic signed [31:0] c_radius_o,
    output var logic        [ 1:0] c_rung_o,
    output var logic               c_view_o,

    // ---- evidence -----------------------------------------------------------
    output var logic [31:0] ticks_o,            // ladder evaluations completed
    output var logic [31:0] skipped_repeat_o,   // a later meshlet of the same draw
    output var logic [31:0] bank_miss_o,        // the form has no ladder row
    output var logic [31:0] no_radius_o,        // behind the eye, or a bad bound
    output var logic [31:0] dropped_o,          // a new instance while busy
    output var logic [31:0] out_of_range_o,     // instance id at or above INSTANCES
    output var logic [31:0] rung_counts_o [4],  // ticks that settled at each rung
    // The radius service's own census, EXPORTED rather than tied to empty pins.
    // A census inside a census top is still evidence, and `rad_behind_o` in
    // particular is the number that separates "the ladder did not tick because
    // the creature is behind the camera" from "it did not tick because the bank
    // had no row" -- two different absences that look identical in `ticks_o`.
    output var logic [31:0] rad_evaluations_o,
    output var logic [31:0] rad_behind_o,
    output var logic [31:0] rad_bad_bound_o,
    output var logic [31:0] rad_saturated_o,
    output var logic        busy_o
);

  // Quartus 17.0 needs an elaboration check inside `initial begin ... end`; a
  // bare module-scope `if` is a syntax error there however clean the lint, and
  // `--lint-only` does not run this, so it is fired by parameter override in
  // the directed test rather than claimed by the lint.
  initial begin
    if (INSTANCES < 2 || INSTANCES > 65536)
      $fatal(1, "zhao_geom_lodstate: INSTANCES is %0d; the instance id is 16 bits",
             INSTANCES);
    if (IIDW != $clog2(INSTANCES))
      $fatal(1, "zhao_geom_lodstate: IIDW is %0d but INSTANCES %0d needs %0d",
             IIDW, INSTANCES, $clog2(INSTANCES));
    // ------------------------------------------------------------------
    // THERE IS DELIBERATELY NO GUARD ON THE PER-CAMERA SLOT INDEX, AND THAT
    // IS THE INTERESTING PART OF THIS BLOCK'S 2026-09-23 CHANGE.
    // ------------------------------------------------------------------
    // Owner ruling R74 / D-LADDER-A doubled the store to `INSTANCES * 2`, and
    // TWO guards were written for it before the right answer was to write
    // none. Both were DEAD, and each looked like protection:
    //
    //   1. `INSTANCES*2 != (1 << (IIDW+1)) && (1 << IIDW) == INSTANCES`
    //      -- unsatisfiable for every legal INSTANCES. Meanwhile the indexing
    //      it was guarding genuinely WAS wrong: `{view, idx}` overflows the
    //      array for any INSTANCES that is not a power of two (at 200, camera
    //      1's instance 199 addresses slot 455 of 400). The guard could not
    //      fire, and the bug it should have caught was real and present.
    //   2. `(1 << SIDX_W) < SLOTS_C` with `SIDX_W = $clog2(SLOTS_C)`
    //      -- a tautology. `1 << $clog2(N) >= N` by definition.
    //
    // WHAT MADE THE FAULT GO AWAY WAS THE ARITHMETIC INDEX, NOT A CHECK.
    // `slot_c = idx + view*INSTANCES` with `SIDX_W = $clog2(2*INSTANCES)` is
    // bounded by construction: `in_range_c` already refuses `idx >= INSTANCES`
    // and counts it on `out_of_range_o`, so the largest reachable slot is
    // `2*INSTANCES - 1`, which SIDX_W holds for every INSTANCES. There is
    // nothing left for an elaboration check to say.
    //
    // CLAUDE.md's law is that a detector reading zero is the claim to check
    // hardest, and a guard that CANNOT fire is the degenerate case of it: it
    // reads as evidence and is not. Shipping a third unfireable `$fatal` here
    // to look thorough would be worse than this comment, because the comment
    // cannot be mistaken for a test. `--lint-only` does not execute `initial`
    // blocks anyway, so a clean lint would never have distinguished the two
    // dead guards from live ones.
  end

  // ---- the ladder state: one {rung, hold} per (INSTANCE, CAMERA) -----------
  // `zref::creature::LodState` is `{LodRung rung = kMesh; uint16_t hold = 0;}`
  // and the reset below is that initialiser, not a convenient zero: kMesh IS
  // rung 0, and a creature that has never been evaluated is at the FINEST rung
  // rather than the coarsest. The other way round, a creature would pop into
  // existence as a splat and take fifteen ticks to become itself.
  //
  // THE STORE IS `INSTANCES * 2` AND THE EXTRA BIT IS THE CAMERA -- owner
  // ruling R74 / D-LADDER-A, "one more bit x INSTANCES". Camera 0's whole
  // ladder bank is the low half and camera 1's the high half; that ordering is
  // arbitrary but it is NAMED here rather than implied, because a reader
  // debugging a rung needs to know which half they are looking at.
  //
  // THE INDEX IS ARITHMETIC (`idx + view*INSTANCES`) AND NOT A CONCATENATION
  // (`{view, idx}`), AND THE FIRST DRAFT OF THIS CHANGE USED THE
  // CONCATENATION AND WAS WRONG. `{view, idx}` is correct only when INSTANCES
  // is a power of two. At INSTANCES = 200, `idx_q` is 8 bits, `in_range_c`
  // admits idx up to 199, and `{1'b1, 8'd199}` is 455 against an array of 400
  // -- an out-of-bounds write on camera 1 for every instance past the halfway
  // point. INSTANCES is a PARAMETER whose own comment calls it "NAMED AND
  // EDITABLE", so "it is 256 today" is not an argument, it is the reason the
  // fault would have shipped.
  //
  // THE DIRECTED TEST RUNS AT INSTANCES = 8 AND WOULD NOT HAVE CAUGHT IT --
  // at a power of two the two indexings are identical. It was found by reading
  // the expression against the parameter's declared range, not by a bench, and
  // that is worth saying rather than implying coverage this block does not
  // have. The arithmetic form is correct by CONSTRUCTION for every INSTANCES,
  // which is why no test is owed for it; see the `initial` block's note on why
  // it carries no guard either.
  localparam int unsigned STW     = 18;                  // {rung[1:0], hold[15:0]}
  localparam int unsigned SLOTS_C = INSTANCES * 2;
  localparam int unsigned SIDX_W  = $clog2(SLOTS_C);
  logic [STW-1:0] st_q [SLOTS_C];

  // The slot the CURRENT evaluation reads and writes. One expression, used at
  // both the `zhao_geom_lod` read ports and the `S_LOD` write, because the two
  // disagreeing is precisely the fault a per-camera store introduces and the
  // single-camera store could not have.
  logic [SIDX_W-1:0] slot_c;

  // ---- the walk ------------------------------------------------------------
  localparam logic [2:0] S_IDLE  = 3'd0;
  localparam logic [2:0] S_LOOK  = 3'd1;   // ask the bank for this form's row
  localparam logic [2:0] S_WAITA = 3'd2;   // its answer is registered one clock later
  localparam logic [2:0] S_PROJ  = 3'd3;   // offer the centre to client A
  localparam logic [2:0] S_WAITW = 3'd4;   // ...and wait for its w
  localparam logic [2:0] S_RAD   = 3'd5;   // the projected bound radius
  localparam logic [2:0] S_LOD   = 3'd6;   // the ladder itself
  localparam logic [2:0] S_EMIT  = 3'd7;   // offer the caster

  logic [2:0]        st_w;
  logic [15:0]       iid_q;
  logic [IIDW-1:0]   idx_q;
  logic [23:0]       form_q;
  logic signed [31:0] cx_q, cy_q, cz_q;
  logic               view_q;
  // The job's view mask, and which of its set bits have been evaluated. Both
  // are needed: `mask_q` says what is owed and `done_q` says what has been
  // paid, and a single "second pass pending" flag would be the same two facts
  // compressed into one that cannot express a mask of `2'b10`.
  logic        [ 1:0] mask_q;
  logic        [ 1:0] done_q;
  logic signed [31:0] bnd_q, mic_q, spl_q, gli_q;
  logic [30:0]        w_q;
  logic               behind_q;
  logic signed [31:0] rad_q;

  // The previous accepted job's instance, and whether there IS one this frame.
  logic [15:0] last_iid_q;
  logic        have_last_q;

  assign busy_o = (st_w != S_IDLE);

  // A job is this block's business when it names an instance that is not the
  // one the previous accepted job named. See the header: that is a property of
  // `zhao_geom_drawjob`'s FSM, not of a workload.
  wire new_inst_c = j_fire_i && (!have_last_q || (j_instance_id_i != last_iid_q));
  wire in_range_c = (j_instance_id_i < 16'(INSTANCES));
  // A draw no camera sees is not this block's business. DRAWJOB does not emit
  // one (its `masked_o` counts them), so this guard is defensive -- see the
  // header's note about not assuming a producer's behaviour away.
  wire any_view_c = (j_view_mask_i != 2'b00);

  // The slot the current pass owns. See the declaration for why this is a sum
  // and not a concatenation.
  assign slot_c = SIDX_W'(idx_q) + (view_q ? SIDX_W'(INSTANCES) : SIDX_W'(0));

  // THE PASS BOOKKEEPING, AS TWO NAMED COMBINATIONAL FACTS rather than as an
  // expression repeated at each exit. `nx_done_c` is what will have been paid
  // once the current pass retires; `nx_owed_c` is what the mask still asks for
  // after that. There are two exits that retire a pass -- the caster handover
  // and the per-camera `no_radius` skip -- and writing the same two
  // expressions twice is how one of them acquires a different meaning.
  logic [1:0] nx_done_c, nx_owed_c;
  assign nx_done_c = done_q | (view_q ? 2'b10 : 2'b01);
  assign nx_owed_c = mask_q & ~nx_done_c;

  // ---- the bank lookup -----------------------------------------------------
  assign q_valid_o = (st_w == S_LOOK);
  assign q_form_o  = form_q;

  // ---- the centre projection ----------------------------------------------
  assign pr_valid_o = (st_w == S_PROJ);
  assign pr_x_o     = cx_q;
  assign pr_y_o     = cy_q;
  assign pr_z_o     = cz_q;
  assign pr_view_o  = view_q;

  // ---- the projected bound radius -----------------------------------------
  logic               rr_req_valid_c, rr_req_ready_w;
  logic               rr_ans_valid_w, rr_ans_ok_w;
  logic signed [31:0] rr_radius_w;
  /* verilator lint_off UNUSEDSIGNAL */
  // The service's own tag is not used: this block has exactly one evaluation in
  // flight, so an answer can only belong to the request that started it. The
  // port is tied to zero rather than removed because it is the service's
  // interface, and a caller with several in flight will want it.
  logic [15:0] rr_tag_w;
  /* verilator lint_on UNUSEDSIGNAL */

  // The request is OFFERED once and taken down on the accept, rather than held
  // as a level for the whole evaluation. Holding it would work today -- the
  // service's `req_ready_o` is high only in its idle state -- but it makes this
  // block's correctness depend on a detail of the service's handshake instead
  // of on the handshake itself, and `rr_issued_q` costs one flip-flop.
  logic rr_issued_q;
  assign rr_req_valid_c = (st_w == S_RAD) && !rr_issued_q;

  zhao_geom_projradius #(
      .TAGW(16)
  ) u_radius (
      .clk  (clk),
      .rst_n(rst_n),

      .req_valid_i   (rr_req_valid_c),
      .req_ready_o   (rr_req_ready_w),
      .kx_i          (view_q ? kx1_i : kx0_i),
      .vw_i          (view_q ? vw1_i : vw0_i),
      .bound_radius_i(bnd_q),
      .w_i           (w_q),
      .behind_i      (behind_q),
      .tag_i         (iid_q),

      .ans_valid_o(rr_ans_valid_w),
      .ans_ready_i((st_w == S_RAD)),
      .radius_q8_o(rr_radius_w),
      .ans_ok_o   (rr_ans_ok_w),
      .tag_o      (rr_tag_w),

      .evaluations_o(rad_evaluations_o),
      .behind_o     (rad_behind_o),
      .bad_bound_o  (rad_bad_bound_o),
      .saturated_o  (rad_saturated_o)
  );

  // ---- the ladder ----------------------------------------------------------
  logic               lod_tick_c;
  logic               lod_ready_w, lod_valid_w;
  logic [ 1:0]        lod_rung_w, lod_raw_w;
  logic [15:0]        lod_hold_w;
  /* verilator lint_off UNUSEDSIGNAL */
  // `raw_o` is the rung BEFORE the stability law, which is evidence about the
  // hysteresis rather than a value anything acts on. It is read by the directed
  // test through the bench's own port and not by this block.
  logic [1:0] raw_unused_w;
  /* verilator lint_on UNUSEDSIGNAL */
  assign raw_unused_w = lod_raw_w;

  logic signed [31:0] thresh_c;
  assign thresh_c = view_q ? thresh1_i : thresh0_i;

  assign lod_tick_c = (st_w == S_LOD) && lod_ready_w && !lod_valid_w;

  // ---- the slot store's read side (the ports themselves are below) ---------
  // Declared HERE because the ladder instantiation consumes `st_eff_c`, and a
  // net used before its declaration is an error under `default_nettype none`.
  logic [STW-1:0] st_rd_q;      // the registered array read
  logic           st_v_q [SLOTS_C];
  logic           st_v_rd_q;    // ... and its valid bit, read in lockstep
  localparam logic [STW-1:0] ST_INIT = 18'd0;   // {kMesh, hold 0}

  wire lod_wr_c = (st_w == S_LOD) && lod_valid_w;

  // What the ladder actually sees: the stored row if this slot has ever been
  // written, otherwise the value the old reset loop used to leave there.
  wire [STW-1:0] st_eff_c = st_v_rd_q ? st_rd_q : ST_INIT;

  zhao_geom_lod u_lod (
      .clk  (clk),
      .rst_n(rst_n),

      .tick_i(lod_tick_c),

      .proj_radius_q8_i(rad_q),
      .thresh_q8_i     (thresh_c),

      .bound_radius_i(bnd_q),
      .micro_error_i (mic_q),
      .splat_error_i (spl_q),
      .glint_error_i (gli_q),

      // FED FROM THE REGISTERED READ, not from `st_q[slot_c]` directly. See
      // the memory port below for why that costs nothing here.
      .rung_i(st_eff_c[17:16]),
      .hold_i(st_eff_c[15:0]),

      .rung_o (lod_rung_w),
      .hold_o (lod_hold_w),
      .raw_o  (lod_raw_w),
      .valid_o(lod_valid_w),
      .ready_o(lod_ready_w)
  );

  // ==========================================================================
  // THE SLOT STORE'S MEMORY PORT.
  //
  // `st_q` used to be read COMBINATIONALLY at `st_q[slot_c]` straight into the
  // ladder's inputs, evaluated, and written back to the SAME address on the
  // same edge, with an asynchronous reset loop over the array besides. Measured
  // (`tests/probes/zhao_floparray_probe.sv`, arms l0-l3): those two properties
  // are a CONJUNCTION -- removing either one ALONE leaves all 9,216 bits in
  // flip-flops, and only removing BOTH infers a 512x18 Simple Dual Port M10K.
  // That is PALRAM's shape exactly, and it is NOT the shape `zhao_forge_assemble`
  // turned out to have, where the reset loop alone was the whole cause.
  //
  // WHY THE REGISTERED READ COSTS NOTHING HERE, which is the part that had to
  // be measured rather than assumed. A registered read normally inserts a
  // pipeline stage. It does not here because `slot_c` is already STABLE FOR
  // SEVERAL STATES before the ladder needs it: `idx_q` is latched on the way
  // out of S_IDLE and `view_q` only ever changes on a transition that goes to
  // S_PROJ or S_IDLE -- never on S_RAD -> S_LOD. So between the address
  // becoming valid and S_LOD consuming it there are at minimum the S_PROJ,
  // S_WAITW and S_RAD states, and `st_rd_q` has long since settled. The
  // bench-measured clock count is unchanged; see the packet's findings.
  //
  // THE VALIDITY DISCIPLINE replacing the reset loop: the array no longer
  // clears, so "every slot reads {kMesh, hold 0} before its first write" is now
  // owed to `st_v_q`, a separate 1-bit-per-slot vector that IS cleared by
  // reset. That is `zhao_geom_drawjob`'s pal_v_q template verbatim, and its
  // comment states the reason: the valid bits must be CLEARED by reset and an
  // asynchronous clear on the payload array would destroy the inference. Unlike
  // `zhao_forge_assemble` there is no structural barrier to lean on here -- a
  // creature's FIRST evaluation genuinely reads a slot nobody has written --
  // so this block needs real valid bits rather than a proof.
  // (`st_rd_q`, `st_v_q`, `st_v_rd_q`, `ST_INIT`, `lod_wr_c` and `st_eff_c` are
  // declared just above the ladder instantiation, which consumes `st_eff_c`.)
  //
  // NO RESET ON `st_q`, which is what lets it infer. The read and the write
  // live in one clocked block so Quartus sees a single memory with two ports.
  // THE VALID MUX IS AFTER THE REGISTER, NEVER BEFORE IT. Writing
  //     st_rd_q <= st_v_q[slot_c] ? st_q[slot_c] : ST_INIT;
  // puts a mux between the array and its read register, which is no longer the
  // plain `rd_q <= mem[addr]` Quartus infers from -- it would cost the whole
  // 9,216-bit conversion to save one wire. Keep the array read raw and apply
  // validity downstream.
  always_ff @(posedge clk) begin
    st_rd_q <= st_q[slot_c];
    if (lod_wr_c) st_q[slot_c] <= {lod_rung_w, lod_hold_w};
  end

  // The valid bits ARE reset, and they are separate flops for exactly that
  // reason. SLOTS_C of them at one bit each -- 512 flops against the 9,216 the
  // array gives back.
  integer v;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      for (v = 0; v < SLOTS_C; v = v + 1) st_v_q[v] <= 1'b0;
      st_v_rd_q <= 1'b0;
    end else begin
      st_v_rd_q <= st_v_q[slot_c];
      if (lod_wr_c) st_v_q[slot_c] <= 1'b1;
    end
  end

  // ---- the caster ----------------------------------------------------------
  assign c_valid_o = (st_w == S_EMIT);

  integer i;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_w        <= S_IDLE;
      iid_q       <= 16'd0;
      idx_q       <= '0;
      form_q      <= 24'd0;
      cx_q        <= 32'sd0;
      cy_q        <= 32'sd0;
      cz_q        <= 32'sd0;
      view_q      <= 1'b0;
      mask_q      <= 2'b00;
      done_q      <= 2'b00;
      bnd_q       <= 32'sd0;
      mic_q       <= 32'sd0;
      spl_q       <= 32'sd0;
      gli_q       <= 32'sd0;
      w_q         <= 31'd0;
      behind_q    <= 1'b0;
      rad_q       <= 32'sd0;
      rr_issued_q <= 1'b0;
      last_iid_q  <= 16'd0;
      have_last_q <= 1'b0;
      // THE RESET LOOP OVER `st_q` IS GONE. It used to read
      //     for (i = 0; i < SLOTS_C; i = i + 1) st_q[i] <= 18'd0;
      // and, together with the combinational read, it was the CONJUNCTION that
      // held 9,216 bits in flip-flops. `st_v_q` above carries the "never
      // written, so read {kMesh, hold 0}" meaning instead, and IT is reset.
      c_instance_id_o  <= 16'd0;
      c_x_o            <= 32'sd0;
      c_z_o            <= 32'sd0;
      c_radius_o       <= 32'sd0;
      c_rung_o         <= 2'd0;
      c_view_o         <= 1'b0;
      ticks_o          <= 32'd0;
      skipped_repeat_o <= 32'd0;
      bank_miss_o      <= 32'd0;
      no_radius_o      <= 32'd0;
      dropped_o        <= 32'd0;
      out_of_range_o   <= 32'd0;
      for (i = 0; i < 4; i = i + 1) rung_counts_o[i] <= 32'd0;
    end else begin
      // The frame boundary clears the run marker. It does NOT clear the ladder
      // state -- that is the whole point of holding it.
      if (frame_i) have_last_q <= 1'b0;

      // The radius request's own accept, tracked so the offer is taken down.
      if (rr_req_valid_c && rr_req_ready_w) rr_issued_q <= 1'b1;

      // Every observed job updates the run marker, whether or not it is taken:
      // the marker is "which instance the previous job named", not "which
      // instance was last evaluated". Conflating the two would make a DROPPED
      // job's instance eligible again on the very next meshlet.
      if (j_fire_i) begin
        last_iid_q  <= j_instance_id_i;
        have_last_q <= 1'b1;
        if (!new_inst_c) skipped_repeat_o <= skipped_repeat_o + 32'd1;
        else if (!in_range_c) out_of_range_o <= out_of_range_o + 32'd1;
        else if (st_w != S_IDLE) dropped_o <= dropped_o + 32'd1;
      end

      case (st_w)
        S_IDLE: begin
          if (new_inst_c && in_range_c && any_view_c) begin
            iid_q  <= j_instance_id_i;
            idx_q  <= j_instance_id_i[IIDW-1:0];
            form_q <= j_form_index_i;
            cx_q   <= j_cx_i;
            cy_q   <= j_cy_i;
            cz_q   <= j_cz_i;
            // THE LOWEST SET BIT FIRST, and the order is a DECLARED
            // convention rather than an accident of the expression: view 0
            // before view 1, so two consecutive frames of the same dual-view
            // instance produce the two casters in the same order and a
            // capture CRC does not move for a reason nobody authored.
            mask_q <= j_view_mask_i;
            done_q <= 2'b00;
            view_q <= !j_view_mask_i[0];
            st_w   <= S_LOOK;
          end
        end

        S_LOOK: begin
          // The bank accepts a query every cycle and answers one cycle later.
          st_w <= S_WAITA;
        end

        S_WAITA: begin
          if (a_valid_i) begin
            if (!a_hit_i) begin
              // A form with no ladder row. The ladder is NOT ticked: running it
              // against a zero bound radius would pick a rung
              // `zref::creature::lod_raw` never picks, and nothing downstream
              // could tell. The instance keeps the state it had.
              bank_miss_o <= bank_miss_o + 32'd1;
              st_w        <= S_IDLE;
            end else begin
              bnd_q <= a_bound_i;
              mic_q <= a_micro_i;
              spl_q <= a_splat_i;
              gli_q <= a_glint_i;
              st_w  <= S_PROJ;
            end
          end
        end

        S_PROJ: begin
          if (pr_ready_i) st_w <= S_WAITW;
        end

        S_WAITW: begin
          if (pr_ans_valid_i) begin
            w_q         <= pr_w_i;
            behind_q    <= pr_behind_i;
            rr_issued_q <= 1'b0;
            st_w        <= S_RAD;
          end
        end

        S_RAD: begin
          if (rr_ans_valid_w) begin
            if (!rr_ans_ok_w) begin
              // Behind the eye, or a bound radius the bank could not supply.
              // The reference SKIPS the creature entirely in this case, so the
              // ladder is not ticked and the state stands.
              //
              // BUT IT SKIPS IT FOR **THIS CAMERA**, NOT FOR THE JOB. "Behind
              // the eye" is a per-camera fact -- a creature behind camera 0
              // can be squarely in front of camera 1 -- so the other view is
              // still owed and is taken next. Abandoning the job here would
              // reintroduce exactly the Duo defect D-LADDER-A was ruled to
              // remove, in the one path where it is least visible: the second
              // player's creature would keep a stale rung whenever the first
              // player turned away from it.
              no_radius_o <= no_radius_o + 32'd1;
              done_q      <= nx_done_c;
              if (nx_owed_c != 2'b00) begin
                view_q <= !nx_owed_c[0];
                st_w   <= S_PROJ;
              end else begin
                st_w <= S_IDLE;
              end
            end else begin
              rad_q <= rr_radius_w;
              st_w  <= S_LOD;
            end
          end
        end

        S_LOD: begin
          if (lod_valid_w) begin
            // `st_q[slot_c]` is written at the memory port above, under this
            // exact condition (`lod_wr_c`). Only the ARRAY moved; every counter
            // and every caster field below is untouched.
            ticks_o                   <= ticks_o + 32'd1;
            rung_counts_o[lod_rung_w] <= rung_counts_o[lod_rung_w] + 32'd1;
            c_instance_id_o           <= iid_q;
            c_x_o                     <= cx_q;
            c_z_o                     <= cz_q;
            c_radius_o                <= bnd_q;
            c_rung_o                  <= lod_rung_w;
            c_view_o                  <= view_q;
            st_w                      <= S_EMIT;
          end
        end

        default: begin  // S_EMIT
          // THE CASTER IS HANDED OVER BEFORE THE SECOND VIEW STARTS, not
          // after both are done. The consumer takes one caster per camera and
          // `c_view_o` says which; holding the first while the second is
          // computed would put a ~164-clock bubble between them for no reason
          // and would need a second set of output registers to hold it in.
          if (c_ready_i) begin
            done_q <= nx_done_c;
            if (nx_owed_c != 2'b00) begin
              // The other camera is owed. Re-enter at S_PROJ: the bank row is
              // this form's and is still latched, but the centre projection
              // and the radius are PER CAMERA and must be redone. See the
              // header.
              view_q <= !nx_owed_c[0];
              st_w   <= S_PROJ;
            end else begin
              st_w <= S_IDLE;
            end
          end
        end
      endcase
    end
  end

endmodule : zhao_geom_lodstate

`default_nettype wire
