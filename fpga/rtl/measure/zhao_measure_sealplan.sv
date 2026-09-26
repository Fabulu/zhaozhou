// zhao_measure_sealplan.sv -- THE PER-VIEW PRE-FRAME ADMISSION PLAN, validated
// in hardware and turned into the immutable quota-seal record that
// GEOM.PARAMBUF consumes.
//
// Owner vacation directive 2026-09-23 section 5 is this file's specification
// and is quoted where it decides something, because four passes over console
// entry I56 re-derived the same architecture and none of them built it.
//
// ---------------------------------------------------------------------------
// THE SENTENCE THIS FILE EXISTS TO ANSWER
// ---------------------------------------------------------------------------
//   > "A constant equal to arena capacity is not an admission plan."
//
// Before this block, `zhao_console_core.sv` drove the arena's seal with three
// literals -- `18'(GEOM_PA_MAX_VERTS)`, `18'(GEOM_PA_MAX_TRIS)`,
// `18'(GEOM_PA_MAX_CHUNKS)` -- and entry I56 said so plainly: R7's giant quota
// "is therefore NOT IN FORCE". The arena's own header agrees about whose job
// it is: it "does not allocate the GIANT QUOTA reservation ... that is a
// decision made by whoever computes `seal_chunks_i`, not here."
//
// THIS BLOCK IS WHOEVER. It is the Measure side of the seam, it owns the
// validated plan, and it is the only producer of the arena's seal.
//
// ---------------------------------------------------------------------------
// WHERE THE PLAN COMES FROM, AND WHY IT IS NOT COMPUTED HERE
// ---------------------------------------------------------------------------
//   > "The Measure owns the validated plan. The architect may compute the plan
//   >  on the authorized HPS path and validate/seal it in hardware; it is not
//   >  required to duplicate a large policy engine merely to avoid adding a
//   >  command or mailbox field."
//
// The plan's per-object cost -- how many tile references a given instance at a
// given rung will generate -- is the one quantity this console does not hold
// and cannot cheaply derive: it is a screen-space property of a mesh that has
// not been transformed yet. The HPS holds the scene graph and can price it.
// So the plan ARRIVES as a command record (`SealFramePlan`, spec/commands.zidl)
// and this block VALIDATES and SEALS it. That division is the directive's, not
// this file's invention.
//
// WHAT VALIDATION MEANS HERE, precisely: every number the HPS offers is
// checked against the hardware capacity it will be spent against, IN ITS OWN
// UNIT, and against the guaranteed giant's reservation. A plan that fails any
// check is REFUSED BEFORE THE SEAL and the frame is never opened --
//
//   > "Illegal/overflowing plans are refused before any partial publication,
//   >  with the existing whole-frame fallback."
//
// -- which is not a new fallback: `zhao_geom_paramarena`'s `publish_*_o` keeps
// describing the PRIOR COMPLETE FRAME for exactly as long as no new frame is
// sealed. A refusal here therefore repeats the last good frame, by the
// mechanism that already shipped, rather than by a promise made in a comment.
//
// ---------------------------------------------------------------------------
// FOUR UNITS, AND THE 14x CONFUSION THAT FLATTERS
// ---------------------------------------------------------------------------
//   > "The seal names frame/view/resource generation, capacities, vertex/
//   >  triangle/reference/chunk limits, and the designated giant's
//   >  reservation. Keep those units distinct."
//   > "Do not confuse references with chunks or assume a reference budget pays
//   >  for every other structure."
//
// The four units and where each is spent:
//
//   VERTICES        `zhao_geom_paramarena` vertex slots, MAX_VERTS  = 65,536
//   TRIANGLES       arena triangle descriptors,          MAX_TRIS   = 16,384
//   CHUNKS          arena chunk payload units,           MAX_CHUNKS = 16,384
//   TILE REFERENCES `zhao_geom_binner_v2.ref_ram`,       MAX_REFS   = 32,768
//
// R7's guaranteed giant is 32,768 TILE REFERENCES. At CHUNK_IDS = 14 that is
// ceil(32768/14) = 2,341 CHUNKS of payload. The two numbers are 14x apart and
// BOTH confusions are refused here, by DIFFERENT rules, which is the point:
//
//   * 32,768 written into the CHUNK field is 200% of MAX_CHUNKS and fails
//     R_CHUNKS_CAP. This is the LOUD direction and REFPUSH already noted it
//     refuses itself.
//   * 2,341 written into the giant's REFERENCE reservation is the FLATTERING
//     direction -- it looks like a giant paid for and is 14x short. It fails
//     R_GIANT_TRIM, because this block requires the reservation to be R7's
//     number EXACTLY. The directive's list of what the delegation does NOT
//     cover has "shrink the guaranteed giant" on it, so a plan that trims it
//     is not a smaller plan, it is an illegal one.
//
// THE CEIL IS AN ELABORATION CONSTANT, NOT A DIVIDER. Because GIANT_REFS is
// fixed by R7 and a plan may not move it, ceil(GIANT_REFS/CHUNK_IDS) is known
// at elaboration and costs no silicon. A dynamic divide would be arithmetic
// bought to support a flexibility the directive forbids.
//
// ---------------------------------------------------------------------------
// THE RESERVATION, AND EXACTLY WHAT IT GUARANTEES
// ---------------------------------------------------------------------------
//   > "Reserve the guaranteed giant's 32,768 TILE REFERENCES before ordinary
//   >  kMesh allocation, including ceil(32768/14) chunk payload units ..."
//
// "Before ordinary kMesh allocation" is an ALLOCATION-ORDER statement about
// the PLAN, and that is where it is enforced: the giant's reservation is
// subtracted from every capacity FIRST, and the plan's ordinary demand is
// checked against what remains. `plan_chunks_i` and `plan_refs_i` are
// therefore ORDINARY-ONLY budgets by definition, and the sealed record carries
// the reservation beside them as a distinct field.
//
// WHAT THIS DOES **NOT** DO, stated here rather than left to be discovered.
// It does not give the arena a per-chunk "this one is the giant's" class bit.
// GIANTQUOTA measured why that is not implementable and the measurement
// stands: a chunk is a SPATIAL object, `zhao_geom_binner_v2` drains a tile's
// FIFO in frame-wide triangle submission order, and the 14-reference chunk
// that straddles two instances belongs to both. A class bit there would have
// to be fabricated. So the guarantee this block delivers is stated exactly:
//
//   A frame that declares a guaranteed giant is SEALED AT A QUOTA STRICTLY
//   BELOW ARENA CAPACITY, by the reservation. The ordinary stream faults at
//   its sealed quota (`ck_fits_c`), at which moment the reserved units are
//   still PHYSICALLY UNALLOCATED. The giant is whole because the ordinary
//   budget provably cannot reach it -- not because the arena can tell the
//   chunks apart.
//
// That is weaker than a runtime partition and it is the strongest property
// this seam can carry. It is also enough for R7's actual contract, which is
// that the giant is never SILENTLY TRUNCATED: an overrun faults the whole
// frame and publishes nothing.
//
// AND A CAPACITY FINDING THAT FALLS OUT OF THE ARITHMETIC, recorded because
// it is the next packet's problem and it is not visible from either side
// alone. The composed binner holds MAX_REFS = 32,768 tile references
// (`zhao_shell_top_v2`: RENDER_CHUNKS=8192 x RENDER_CHUNK_REFS=4) and R7's
// giant reserves 32,768. THE RESERVE EQUALS THE ENTIRE CAPACITY. So a plan
// that declares a guaranteed giant must declare ZERO ordinary tile references,
// and any positive ordinary reference demand is refused at R_REF_RESV. That is
// correct enforcement of the ruled number and it is also a statement about the
// machine: at today's binner capacity a giant frame admits no ordinary
// geometry. The answer is more reference capacity, which is a fit; it is NOT
// trimming the giant, which the delegation does not cover. In CHUNKS there is
// real room -- 2,341 reserved of 16,384, leaving 14,043 ordinary -- which is
// why the chunk arm is the one with a non-degenerate pressure case.
//
// ---------------------------------------------------------------------------
// THE SELECTOR: ONE COMPARATOR, AND DELIBERATELY NOT A HEAP
// ---------------------------------------------------------------------------
//   > "Select the guaranteed giant by declared priority, then stable instance
//   >  identity for ties."
//
// `semantic_weight` is the declared priority and it already ships:
// spec/commands.zidl gives it to DrawForm, DrawPopulation, DrawPosedForm and
// DrawWarpedForm, its own comment says it "feeds the Measure policy (degrade
// order)", and `zhao_cmd_exec.draw_semantic_weight_o` decodes it. Nothing has
// ever consulted it for policy. This block is its first policy consumer.
//
// The selection is a STREAMING MAX over {semantic_weight, instance_id}:
// highest weight wins, LOWEST instance id breaks the tie. One comparator, one
// pair of registers, no ordering structure. Charter section 9 forbids a
// priority heap and `zhao_measure_tokens` refuses one by name; a running max
// is not a heap and is not a step toward one, because it can never answer
// "the second largest".
//
// WHAT THE SELECTOR IS FOR, and it is not to choose the number. The plan is
// sealed at the frame's BEGIN edge and the draw stream arrives after it, so
// the selector cannot supply the giant's identity to its own frame's seal --
// the HPS declares it. The selector is the INDEPENDENT CHECK: at the frame's
// end, the stream's own max must be the instance the seal named, and a
// disagreement counts at `giant_mismatch_o`.
//
// READ WHAT CLOCKS THE TWO SIDES, because CLAUDE.md's law about detectors says
// to. `sel_weight_q`/`sel_inst_q` are enabled by `dw_valid_i` -- the draw
// stream. `seal_giant_inst_q` is enabled by `seal_fire_c` -- the frame edge.
// Two different enables, so the comparison is NOT structurally blind: a seal
// naming an instance the frame never drew, and a frame whose heaviest draw is
// not the one that was paid for, both move one side and not the other. This is
// the failure the metadata bank shipped (two operands moved by one enable) and
// it is checked for here rather than discovered later.
//
// ---------------------------------------------------------------------------
// IMMUTABILITY, AND THE DEFAULT PLAN
// ---------------------------------------------------------------------------
//   > "Do not renegotiate a sealed frame or borrow from the giant's
//   >  reservation because an early primitive happens to fit."
//
// A `SealFramePlan` record arriving while a frame is open STAGES for the next
// frame and does not touch the sealed record. There is exactly one write port
// to the sealed registers and its enable is `seal_fire_c`. Renegotiation is
// not refused by policy, it is absent by construction.
//
//   > "A frame explicitly containing no guaranteed giant may release that
//   >  reservation BEFORE sealing."
//
// `plan_flags_i[0]` is that explicit declaration. Clear, the reservation is
// released before sealing and the full capacity is available to ordinary
// allocation. Set, the reservation binds.
//
// THE DEFAULT PLAN is what is sealed when software has published none, and it
// is a NO-GIANT plan at capacity. That deserves its objection answered,
// because it is the shape the directive names: a frame with no published plan
// has made no declaration of a guaranteed giant, so the directive's own
// release clause applies to it, and sealing it at capacity is that clause
// rather than the absence of a plan. What has changed is that it is now a
// CASE of a mechanism instead of the only behaviour: it is reached through the
// same validator, it is counted at `default_seals_o` so its use is visible,
// and the moment a plan declares a giant the reservation binds. A console that
// never publishes a plan can read `default_seals_o` and see that it never did.
//
// WHY THE DEFAULT RESERVES NOTHING RATHER THAN RESERVING THE CHUNK HALF. A
// half-applied reservation is worse than none: it looks enforced. R7 rules a
// number for TILE REFERENCES only -- the directive's "and all required
// vertices/descriptors/metadata" names the obligation and rules no figure for
// it, which REFPUSH found and which is the real argument for the plan coming
// from the HPS at all. Reserving the two units that can be derived while
// leaving the two that cannot at capacity would be a reservation that is
// enforced in half the units it names. The plan is where all four are stated
// together, so the plan is where the reservation binds.
//
// ---------------------------------------------------------------------------
// EVERY COUNTER HERE HAS BEEN SEEN TO FIRE
// ---------------------------------------------------------------------------
// tests/measure/measure_sealplan_directed.cpp drives each refusal reason, the
// absent-giant release, both pressure arms, both halves of the 14x confusion,
// the selector's tie rule and the mismatch detector. No counter in this file
// is asserted zero without a case that moves it.
// ---------------------------------------------------------------------------
`default_nettype none

module zhao_measure_sealplan #(
    // ---- the capacities, in four distinct units --------------------------
    // These are the CONSUMERS' own numbers and are passed in from the
    // composition rather than restated, so a capacity change moves one place.
    parameter int unsigned MAX_VERTS  = 65536,   // VERTICES        (arena)
    parameter int unsigned MAX_TRIS   = 16384,   // TRIANGLES       (arena)
    parameter int unsigned MAX_CHUNKS = 16384,   // CHUNKS          (arena)
    parameter int unsigned MAX_REFS   = 32768,   // TILE REFERENCES (binner)
    parameter int unsigned CHUNK_IDS  = 14,      // references per chunk

    // R7'S GUARANTEED GIANT, IN TILE REFERENCES. It is a named constant
    // because CLAUDE.md rule 6 says every value belongs in one -- NOT because
    // it is negotiable. The delegation does not cover shrinking it and the
    // elaboration guard below refuses a build that tries.
    parameter int unsigned GIANT_REFS = 32768,

    // The seal field width. It is the arena's, so the seal cannot be widened
    // here and silently truncated there.
    parameter int unsigned QW         = 18,

    // How many views this console has. A plan naming a view that does not
    // exist is refused rather than aliased onto view 0.
    parameter int unsigned VIEWS      = 2
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- THE PLAN, from the authorized HPS path (CMD.EXEC) ---------------
    // `plan_valid_i` is a ONE-CYCLE pulse: a SealFramePlan record has been
    // committed by a packet that passed its verdict. Everything beside it is
    // that record's fields, held stable for the pulse.
    input  var logic          plan_valid_i,
    input  var logic [7:0]    plan_view_i,
    input  var logic [7:0]    plan_flags_i,      // [0] giant_present, [7:1] MBZ
    input  var logic [15:0]   plan_res_gen_i,
    input  var logic [15:0]   plan_view_gen_i,
    input  var logic [15:0]   plan_giant_inst_i,
    input  var logic [QW-1:0] plan_verts_i,      // VERTICES,   ordinary
    input  var logic [QW-1:0] plan_tris_i,       // TRIANGLES,  ordinary
    input  var logic [QW-1:0] plan_chunks_i,     // CHUNKS,     ordinary
    input  var logic [QW-1:0] plan_refs_i,       // REFERENCES, ordinary
    input  var logic [QW-1:0] plan_giant_refs_i, // REFERENCES, the reservation

    // ---- the live console generations the plan is validated against ------
    input  var logic [15:0]   res_gen_i,
    input  var logic [15:0]   view_gen_i,

    // ---- the draw stream, for the selector -------------------------------
    input  var logic          dw_valid_i,
    input  var logic [7:0]    dw_weight_i,       // semantic_weight
    input  var logic [15:0]   dw_inst_i,         // stable instance identity

    // ---- the frame edges -------------------------------------------------
    input  var logic          frame_begin_i,
    input  var logic          frame_end_i,

    // ---- THE SEAL, to GEOM.PARAMBUF --------------------------------------
    input  var logic          seal_ready_i,
    output var logic          seal_valid_o,
    output var logic [QW-1:0] seal_verts_o,
    output var logic [QW-1:0] seal_tris_o,
    output var logic [QW-1:0] seal_chunks_o,
    output var logic [QW-1:0] seal_refs_o,
    output var logic [QW-1:0] seal_giant_refs_o,
    output var logic [QW-1:0] seal_giant_chunks_o,
    output var logic [15:0]   seal_frame_gen_o,
    output var logic [15:0]   seal_view_gen_o,
    output var logic [15:0]   seal_res_gen_o,
    output var logic [7:0]    seal_view_o,
    output var logic          seal_giant_present_o,
    output var logic [15:0]   seal_giant_inst_o,

    // ---- evidence --------------------------------------------------------
    output var logic [31:0]   plans_staged_o,    // records accepted for staging
    output var logic [31:0]   plans_sealed_o,    // plans that became a seal
    output var logic [31:0]   plans_refused_o,   // plans refused before the seal
    output var logic [7:0]    refuse_reason_o,   // the LAST refusal's reason
    output var logic [31:0]   default_seals_o,   // frames sealed with no plan
    output var logic [31:0]   seal_lost_o,       // frame edge, arena not ready
    output var logic [31:0]   giant_mismatch_o,  // stream max != sealed giant
    output var logic [31:0]   draws_seen_o       // selector stream depth
);

  // ---- the refusal reasons, one presented, priority order -----------------
  // Capacity first (a number that cannot be spent at all), then the giant
  // fence (a plan that redefines what it may not), then the reservations
  // (a plan that is individually legal and collectively is not), then
  // identity. The order is deterministic so a test can name a reason.
  localparam logic [7:0] R_NONE        = 8'd0;
  localparam logic [7:0] R_VERTS_CAP   = 8'd1;
  localparam logic [7:0] R_TRIS_CAP    = 8'd2;
  localparam logic [7:0] R_CHUNKS_CAP  = 8'd3;
  localparam logic [7:0] R_REFS_CAP    = 8'd4;
  localparam logic [7:0] R_GIANT_TRIM  = 8'd5;
  localparam logic [7:0] R_FLAGS_MBZ   = 8'd6;
  localparam logic [7:0] R_VIEW_ID     = 8'd7;
  localparam logic [7:0] R_CHUNK_RESV  = 8'd8;
  localparam logic [7:0] R_REF_RESV    = 8'd9;
  localparam logic [7:0] R_RES_GEN     = 8'd10;
  localparam logic [7:0] R_VIEW_GEN    = 8'd11;
  localparam logic [7:0] R_GIANT_STRAY = 8'd12;

  localparam logic [31:0] CNT_MAX = 32'hFFFF_FFFF;

  // THE CEIL, AT ELABORATION. See the header: GIANT_REFS is fixed by R7 and a
  // plan may not move it, so the chunk payload the reservation costs is a
  // constant and no divider is bought.
  localparam int unsigned GIANT_CHUNKS = (GIANT_REFS + CHUNK_IDS - 1) / CHUNK_IDS;

  // Quartus 17.0 requires an elaboration check inside `initial begin ... end`;
  // a bare module-scope `if` is a syntax error there however clean the lint
  // (CLAUDE.md, "Verilator lint-clean is not Quartus-synthesizable"). And
  // `--lint-only` does not run this block, so a clean lint says nothing about
  // it -- each of these is fired by parameter override in the directed test.
  initial begin
    if (GIANT_REFS > MAX_REFS)
      $fatal(1, "zhao_measure_sealplan: GIANT_REFS exceeds the binner's whole reference capacity");
    if (GIANT_CHUNKS > MAX_CHUNKS)
      $fatal(1, "zhao_measure_sealplan: the giant's chunk payload exceeds MAX_CHUNKS");
    // `>=`, not `>`. A capacity of exactly 2^QW truncates to ZERO in a QW-bit
    // field, which is the flattering direction: every plan would "fit".
    if (MAX_VERTS  >= (1 << QW)) $fatal(1, "zhao_measure_sealplan: MAX_VERTS does not fit the seal field");
    if (MAX_TRIS   >= (1 << QW)) $fatal(1, "zhao_measure_sealplan: MAX_TRIS does not fit the seal field");
    if (MAX_CHUNKS >= (1 << QW)) $fatal(1, "zhao_measure_sealplan: MAX_CHUNKS does not fit the seal field");
    if (MAX_REFS   >= (1 << QW)) $fatal(1, "zhao_measure_sealplan: MAX_REFS does not fit the seal field");
    if (CHUNK_IDS == 0)         $fatal(1, "zhao_measure_sealplan: CHUNK_IDS of zero has no ceil");
    if (VIEWS == 0)             $fatal(1, "zhao_measure_sealplan: a console with no views cannot seal one");
  end

  // ---- the staged plan ----------------------------------------------------
  // A record lands here and waits for the next frame edge. It is NOT the
  // sealed record: staging is how "do not renegotiate a sealed frame" is
  // satisfied structurally rather than by a rule.
  logic            st_v_q;
  logic [7:0]      st_view_q;
  logic [7:0]      st_flags_q;
  logic [15:0]     st_res_gen_q;
  logic [15:0]     st_view_gen_q;
  logic [15:0]     st_giant_inst_q;
  logic [QW-1:0]   st_verts_q;
  logic [QW-1:0]   st_tris_q;
  logic [QW-1:0]   st_chunks_q;
  logic [QW-1:0]   st_refs_q;
  logic [QW-1:0]   st_giant_refs_q;

  // ---- the plan under validation this cycle -------------------------------
  // With a staged plan, it is that plan. Without one, it is the DEFAULT plan:
  // a no-giant plan at capacity, reached through the same validator so there
  // is one admission path and not two.
  wire             have_plan_c = st_v_q;
  wire [7:0]       p_view_c      = have_plan_c ? st_view_q      : 8'd0;
  wire [7:0]       p_flags_c     = have_plan_c ? st_flags_q     : 8'd0;
  wire [15:0]      p_res_gen_c   = have_plan_c ? st_res_gen_q   : res_gen_i;
  wire [15:0]      p_view_gen_c  = have_plan_c ? st_view_gen_q  : view_gen_i;
  wire [15:0]      p_ginst_c     = have_plan_c ? st_giant_inst_q: 16'd0;
  wire [QW-1:0]    p_verts_c     = have_plan_c ? st_verts_q     : QW'(MAX_VERTS);
  wire [QW-1:0]    p_tris_c      = have_plan_c ? st_tris_q      : QW'(MAX_TRIS);
  wire [QW-1:0]    p_chunks_c    = have_plan_c ? st_chunks_q    : QW'(MAX_CHUNKS);
  wire [QW-1:0]    p_refs_c      = have_plan_c ? st_refs_q      : QW'(MAX_REFS);
  wire [QW-1:0]    p_grefs_c     = have_plan_c ? st_giant_refs_q: QW'(0);

  wire             p_giant_c     = p_flags_c[0];

  // The reservation, released or bound, BEFORE ordinary allocation is checked
  // against anything. Two units, computed separately and never from each
  // other at runtime.
  wire [QW-1:0]    resv_refs_c   = p_giant_c ? QW'(GIANT_REFS)   : QW'(0);
  wire [QW-1:0]    resv_chunks_c = p_giant_c ? QW'(GIANT_CHUNKS) : QW'(0);

  // ---- validation ---------------------------------------------------------
  // One bit per rule, then one priority encoder, so every rule is readable on
  // its own and the presented reason is deterministic. The sums are widened
  // by one bit: a reservation check that overflowed its own comparison would
  // report "fits" in the flattering direction.
  wire [QW:0] chunk_total_c = {1'b0, p_chunks_c} + {1'b0, resv_chunks_c};
  wire [QW:0] ref_total_c   = {1'b0, p_refs_c}   + {1'b0, resv_refs_c};

  wire bad_verts_c   = (p_verts_c  > QW'(MAX_VERTS));
  wire bad_tris_c    = (p_tris_c   > QW'(MAX_TRIS));
  wire bad_chunks_c  = (p_chunks_c > QW'(MAX_CHUNKS));
  wire bad_refs_c    = (p_refs_c   > QW'(MAX_REFS));
  // THE FENCE. A plan that declares a giant must reserve R7's number exactly.
  // Too small is the flattering half of the 14x confusion; too large is a plan
  // inventing a bigger guarantee than the console ships.
  wire bad_gtrim_c   = p_giant_c && (p_grefs_c != QW'(GIANT_REFS));
  // And a plan that declares NO giant must not smuggle a reservation in
  // anyway, or "released" would be a flag with no consequence.
  wire bad_gstray_c  = !p_giant_c && (p_grefs_c != QW'(0));
  wire bad_flags_c   = (p_flags_c[7:1] != 7'd0);
  wire bad_view_c    = (32'(p_view_c) >= 32'(VIEWS));
  wire bad_ckresv_c  = (chunk_total_c > (QW+1)'(MAX_CHUNKS));
  wire bad_refresv_c = (ref_total_c   > (QW+1)'(MAX_REFS));
  wire bad_rgen_c    = (p_res_gen_c  != res_gen_i);
  wire bad_vgen_c    = (p_view_gen_c != view_gen_i);

  logic [7:0] reason_c;
  always_comb begin
    if      (bad_verts_c)   reason_c = R_VERTS_CAP;
    else if (bad_tris_c)    reason_c = R_TRIS_CAP;
    else if (bad_chunks_c)  reason_c = R_CHUNKS_CAP;
    else if (bad_refs_c)    reason_c = R_REFS_CAP;
    else if (bad_gtrim_c)   reason_c = R_GIANT_TRIM;
    else if (bad_gstray_c)  reason_c = R_GIANT_STRAY;
    else if (bad_flags_c)   reason_c = R_FLAGS_MBZ;
    else if (bad_view_c)    reason_c = R_VIEW_ID;
    else if (bad_ckresv_c)  reason_c = R_CHUNK_RESV;
    else if (bad_refresv_c) reason_c = R_REF_RESV;
    else if (bad_rgen_c)    reason_c = R_RES_GEN;
    else if (bad_vgen_c)    reason_c = R_VIEW_GEN;
    else                    reason_c = R_NONE;
  end

  wire plan_ok_c = (reason_c == R_NONE);

  // ---- the seal edge ------------------------------------------------------
  // The pulse semantics of the previous composition are preserved EXACTLY:
  // one cycle at the frame's begin edge, never held. Holding the request
  // across a busy arena would inflate `zhao_geom_paramarena`'s per-cycle
  // `seal_reject` counter, and changing what an existing counter means while
  // changing what feeds it is how two packets disagree about a number.
  wire seal_try_c  = frame_begin_i && plan_ok_c;
  wire seal_fire_c = seal_try_c && seal_ready_i;

  assign seal_valid_o = seal_try_c;

  // The offered record is combinational so the arena latches the same values
  // this block validated, on the same cycle, from the same wires. A
  // registered copy would be a second source that can drift.
  assign seal_verts_o        = p_verts_c;
  assign seal_tris_o         = p_tris_c;
  assign seal_chunks_o       = p_chunks_c;
  assign seal_refs_o         = p_refs_c;
  assign seal_giant_refs_o   = resv_refs_c;
  assign seal_giant_chunks_o = resv_chunks_c;
  assign seal_view_gen_o     = p_view_gen_c;
  assign seal_res_gen_o      = p_res_gen_c;
  assign seal_view_o         = p_view_c;

  // ---- the frame generation ----------------------------------------------
  // Advanced on every ACCEPTED seal, so no two consecutive frames share a
  // stamp. It moved here from `zhao_console_core.sv` unchanged, including its
  // reset value, because the seal's generation belongs with the seal's
  // producer -- a composer may write a join and may not own a counter.
  logic [15:0] frame_gen_q;
  assign seal_frame_gen_o = frame_gen_q;

  // ---- the sealed record -------------------------------------------------
  // ONE write port, enable `seal_fire_c`. Immutability is structural.
  logic          sealed_giant_q;
  logic [15:0]   sealed_ginst_q;
  assign seal_giant_present_o = sealed_giant_q;
  assign seal_giant_inst_o    = sealed_ginst_q;

  // ---- the selector: a streaming max, NOT a heap -------------------------
  // Highest `semantic_weight` wins; on a tie the LOWEST instance id wins,
  // which is the directive's "stable instance identity". `sel_any_q` exists
  // so the first draw of a frame is taken unconditionally rather than having
  // to beat a reset value -- with weight 0 and instance 0 as the reset, a
  // first draw of weight 0 instance 5 would otherwise lose to nothing.
  logic        sel_any_q;
  logic [7:0]  sel_weight_q;
  logic [15:0] sel_inst_q;

  wire sel_better_c = !sel_any_q
                    || (dw_weight_i >  sel_weight_q)
                    || ((dw_weight_i == sel_weight_q) && (dw_inst_i < sel_inst_q));
  wire sel_take_c   = dw_valid_i && sel_better_c;

  // The mismatch test, read at the frame's end. Its two sides are enabled by
  // DIFFERENT things -- see the header -- so it is not one of the detectors
  // that cannot fire.
  wire mismatch_c = frame_end_i && sealed_giant_q && sel_any_q
                    && (sel_inst_q != sealed_ginst_q);
  // A sealed giant that the frame never drew at all is the same disagreement
  // with the other operand missing, and is counted the same way.
  wire absent_c   = frame_end_i && sealed_giant_q && !sel_any_q;

  always_ff @(posedge clk) begin
    if (!rst_n) begin
      st_v_q          <= 1'b0;
      st_view_q       <= 8'd0;
      st_flags_q      <= 8'd0;
      st_res_gen_q    <= 16'd0;
      st_view_gen_q   <= 16'd0;
      st_giant_inst_q <= 16'd0;
      st_verts_q      <= '0;
      st_tris_q       <= '0;
      st_chunks_q     <= '0;
      st_refs_q       <= '0;
      st_giant_refs_q <= '0;

      frame_gen_q     <= 16'd1;
      sealed_giant_q  <= 1'b0;
      sealed_ginst_q  <= 16'd0;

      sel_any_q       <= 1'b0;
      sel_weight_q    <= 8'd0;
      sel_inst_q      <= 16'd0;

      plans_staged_o  <= 32'd0;
      plans_sealed_o  <= 32'd0;
      plans_refused_o <= 32'd0;
      refuse_reason_o <= R_NONE;
      default_seals_o <= 32'd0;
      seal_lost_o     <= 32'd0;
      giant_mismatch_o<= 32'd0;
      draws_seen_o    <= 32'd0;
    end else begin
      // ---- staging. A record arriving mid-frame stages for the NEXT frame;
      // it cannot reach the sealed registers, which have one enable.
      if (plan_valid_i) begin
        st_v_q          <= 1'b1;
        st_view_q       <= plan_view_i;
        st_flags_q      <= plan_flags_i;
        st_res_gen_q    <= plan_res_gen_i;
        st_view_gen_q   <= plan_view_gen_i;
        st_giant_inst_q <= plan_giant_inst_i;
        st_verts_q      <= plan_verts_i;
        st_tris_q       <= plan_tris_i;
        st_chunks_q     <= plan_chunks_i;
        st_refs_q       <= plan_refs_i;
        st_giant_refs_q <= plan_giant_refs_i;
        if (plans_staged_o != CNT_MAX) plans_staged_o <= plans_staged_o + 32'd1;
      end

      // ---- the selector, over the whole frame
      if (sel_take_c) begin
        sel_any_q    <= 1'b1;
        sel_weight_q <= dw_weight_i;
        sel_inst_q   <= dw_inst_i;
      end
      if (dw_valid_i && draws_seen_o != CNT_MAX)
        draws_seen_o <= draws_seen_o + 32'd1;

      // ---- the frame edge: validate, then seal or refuse
      if (frame_begin_i) begin
        // The selector restarts with the frame it is about to measure. Placed
        // after the take above in source order so a draw and a frame edge on
        // the same cycle belong to the NEW frame, which is the same direction
        // the arena's own frame-open logic takes.
        sel_any_q    <= 1'b0;
        sel_weight_q <= 8'd0;
        sel_inst_q   <= 16'd0;

        if (!plan_ok_c) begin
          // REFUSED BEFORE THE SEAL. No frame is opened, the arena goes on
          // publishing the prior complete frame, and the plan is consumed so
          // an illegal record cannot be re-offered every frame forever.
          if (plans_refused_o != CNT_MAX) plans_refused_o <= plans_refused_o + 32'd1;
          refuse_reason_o <= reason_c;
          st_v_q          <= 1'b0;
        end else if (seal_fire_c) begin
          sealed_giant_q <= p_giant_c;
          sealed_ginst_q <= p_ginst_c;
          frame_gen_q    <= frame_gen_q + 16'd1;
          st_v_q         <= 1'b0;
          if (have_plan_c) begin
            if (plans_sealed_o != CNT_MAX) plans_sealed_o <= plans_sealed_o + 32'd1;
          end else begin
            if (default_seals_o != CNT_MAX) default_seals_o <= default_seals_o + 32'd1;
          end
        end else begin
          // The arena could not take it. The plan is KEPT staged -- it
          // described this view and is still the right plan for the next
          // edge -- and the lost edge is counted rather than absorbed.
          if (seal_lost_o != CNT_MAX) seal_lost_o <= seal_lost_o + 32'd1;
        end
      end

      // ---- the independent check, at the frame's end
      if ((mismatch_c || absent_c) && giant_mismatch_o != CNT_MAX)
        giant_mismatch_o <= giant_mismatch_o + 32'd1;
    end
  end

endmodule

`default_nettype wire
