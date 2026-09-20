// zhao_field_alu_vec.sv — the Field ALU, LANES points wide.
//
// ENFORCED-BY: tests/differential/field_alu_vec_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHY THIS EXISTS
// ---------------------------------------------------------------------------
// The service path is FOUR POINTS WIDE: the dispatcher gathers a four-point
// group, the services compute four answers, the drain returns four results.
// The executor is SCALAR -- one context, one instruction, one ALU result, one
// register write per clock -- so the whole machine is a four-wide back end fed
// by a one-wide front end.
//
// The composed Earth gate measured what that costs:
//
//     register writes 4096 in 4464 clocks = 92% of the ONE write port
//
// 16 uops per point means a four-point group needs 64 writes, so at least 64
// clocks against an admission budget of 24.3. Across the 128-association stress
// frame that is 2,236,416 writes: 2.6x over 850,000 FROM WRITES ALONE, whatever
// the services do. And the traffic is overwhelmingly the ALU's -- 11 of 13, 14
// of 16 and 14 of 17 uops across the three shipped Earth programs -- so
// widening only the long-op drain would address about 15% of it.
//
// ---------------------------------------------------------------------------
// WHAT IT DOES NOT DO, WHICH IS THE POINT
// ---------------------------------------------------------------------------
// It adds NO arithmetic. `zhao_field_alu` is swept and closed; this instantiates
// LANES of it and nothing else. Every value this block produces is produced by
// the same combinational block the scalar machine has always used, so there is
// no second opinion about what an op means and no new saturation behaviour to
// re-derive.
//
// The four contexts of a quad run the SAME program at the SAME pc, so `op_i`
// and `imm_i` are shared and only the operands differ. That is why one control
// path drives four datapaths, and it is why this is a wrapper rather than a
// redesign.
//
// ---------------------------------------------------------------------------
// THE FLAGS, AND WHY THEY ARE NOT ALL THE SAME KIND
// ---------------------------------------------------------------------------
// `is_end_o`, `writes_o` and `op_unsupported_o` are properties of the OPCODE.
// Every lane sees the same opcode, so every lane answers identically and lane 0
// speaks for all of them. Taking lane 0 rather than OR-ing them says that out
// loud -- an OR would imply they could disagree, and if they ever did, the bug
// would be hidden by the reduction instead of caught by it. The differential
// checks that they agree.
//
// The SATURATION lanes are the opposite: they are properties of the DATA, so
// lane 2 can saturate while the other three do not.
//
// ---------------------------------------------------------------------------
// WHY THE SATURATION FLAGS ARE NO LONGER ONLY OR-ED (FH11, 2026-09-20)
// ---------------------------------------------------------------------------
// This file used to end the paragraph above with: "They are OR-ed, because the
// ledger records that the group saturated. A per-lane ledger would be a
// different contract and the reference does not have one."
//
// That was true about the v1 ledger and it is the wrong contract for a machine
// that returns a Status PER POINT. The owner directive of 2026-09-20 names this
// file and this reduction (§2.8, [S11]):
//
//     "The wrapper has per-lane saturation flags internally, then ORs them into
//      group flags. That is adequate for a group aggregate, not a returned
//      Status for each individual Warp or Flow point. Preserve the narrow flags
//      before reduction and carry them with actual context/lane identity
//      through retirement."
//
// and §8.3: "The existing vector ALU's l_sadd/l_smul/l_srescale arrays provide
// an extraction point BEFORE group reduction."
//
// THE DEFECT THE OR PRODUCES IS NOT A MISSING FEATURE, IT IS A WRONG ANSWER.
// An aggregate cannot say WHICH lane saturated, so one point's overflow labels
// its three neighbours as having saturated too. Every neighbour then carries a
// numeric status it did not earn -- a plausible, unfalsifiable wrong value of
// exactly the kind R101 and R110 were written about, where a reduction destroys
// the one bit of information that identifies the culprit.
//
// So the narrow flags leave this block, and the group flag is REBUILT FROM THEM
// rather than computed beside them. `sat_add_o` is now literally `|sat_add_lane_o`,
// which means the aggregate and the per-lane view cannot disagree: there is one
// source of truth and one reduction of it, not two parallel computations that
// could drift.
//
// ---------------------------------------------------------------------------
// THE LIVE MASK, AND WHY A PADDING LANE MUST NOT VOTE
// ---------------------------------------------------------------------------
// `lane_live_i` marks the lanes carrying a real point. The directive is explicit
// that a quad is not always full (§8.4):
//
//     "A quad contains 1..4 live points. Invalid lanes must not create results,
//      advance logical point counters, change terrain material/dirty state, or
//      contribute flags to a live lane. ... A group-level aggregate may OR only
//      LIVE lane statuses."
//
// A padded lane is fed replicated or stale operands. Those operands can saturate
// -- they are real numbers going through a real adder -- and the old reduction
// would have published that saturation as the GROUP's, contaminating every live
// point in the quad with a flag produced by a point that does not exist. That is
// the same false-attribution defect as the OR above, arriving from the other
// side, and it is why masking is part of this change rather than a later one.
//
// The mask is applied ONCE, to the narrow flags, and the aggregate is reduced
// from the masked result. Masking only the aggregate would leave a dead lane's
// flag visible in the per-lane view; masking only the per-lane view would leave
// it visible in the aggregate. Both were available and both are wrong.
//
// `lane_live_i` is an INPUT because this block cannot know it: liveness is a
// property of how the caller gathered the group, not of the data. See the
// driver in `zhao_field_v3_exec.sv` for what supplies it and under what
// argument.
//
// The OPCODE flags are deliberately NOT masked. They are decoded from `op_i`
// alone, which every lane shares, so a dead lane answers them identically to a
// live one; masking them would only hide a miswiring that `lane_desync_o`
// exists to catch.
`default_nettype none

module zhao_field_alu_vec #(
    parameter int LANES = 4
) (
    // Shared control: one instruction across the whole quad.
    input var logic [ 7:0] op_i,
    input var logic [31:0] imm_i,

    // WHICH LANES CARRY A REAL POINT. A quad holds 1..LANES live points; the
    // rest are padding fed replicated or stale operands. Padding arithmetic is
    // still real arithmetic and can still saturate, so its flags are masked
    // here rather than downstream -- see the header. Driven by the caller,
    // which is the only place that knows how the group was gathered.
    input var logic [LANES-1:0] lane_live_i,

    // Per-lane operands, packed. Lane l occupies bits [32*l +: 32].
    input var logic signed [32*LANES-1:0] a0_i, a1_i, a2_i,
    input var logic signed [32*LANES-1:0] b0_i, b1_i, b2_i,
    input var logic signed [32*LANES-1:0] c_i,

    // Per-lane products from the shared multiplier bank, which is already
    // four-wide -- this is the side of the machine that never needed widening.
    input var logic signed [66*LANES-1:0] prod_ab_i,
    input var logic signed [66*LANES-1:0] dot2_i,
    input var logic signed [66*LANES-1:0] dot3_i,

    output var logic signed [32*LANES-1:0] result_o,

    // Opcode properties: identical in every lane, taken from lane 0.
    output var logic is_end_o,
    output var logic writes_o,
    output var logic op_unsupported_o,

    // ---- DATA properties, PER LANE -----------------------------------------
    //
    // The narrow flags, masked by `lane_live_i` and otherwise untouched. Bit l
    // means "live lane l saturated in this lane's own arithmetic" and says
    // nothing whatever about its neighbours. This is the extraction point the
    // directive's §8.3 names; the caller attaches context/lane identity.
    //
    // ENFORCED-BY: tests/differential/field_alu_vec_directed.cpp:main
    output var logic [LANES-1:0] sat_add_lane_o,
    output var logic [LANES-1:0] sat_mul_lane_o,
    output var logic [LANES-1:0] sat_rescale_lane_o,

    // ---- DATA properties, aggregated ---------------------------------------
    //
    // The group ledger, KEPT so no existing consumer loses a signal, and now
    // REDUCED FROM THE PER-LANE PORTS ABOVE rather than computed alongside
    // them. One source of truth, one reduction: the aggregate cannot disagree
    // with the detail, and a dead lane cannot reach either.
    //
    // Section 3 of the differential drives exactly one lane into overflow and
    // requires the group flag to rise while the other three keep their own
    // correct answers -- which is what tells a DATA flag from an OPCODE one.
    // FT039 additionally requires the per-lane flags to stay DISTINCT and the
    // aggregate to equal their masked OR.
    //
    // ENFORCED-BY: tests/differential/field_alu_vec_directed.cpp:main
    output var logic sat_add_o,
    output var logic sat_mul_o,
    output var logic sat_rescale_o,

    // A lane disagreeing with lane 0 about an OPCODE property is impossible by
    // construction -- they all see the same opcode -- so if it ever happens the
    // wrapper is miswired. Brought out rather than assumed, because "impossible
    // by construction" is a claim and this engine has paid for several.
    //
    // ENFORCED-BY: tests/differential/field_alu_vec_directed.cpp:main
    output var logic lane_desync_o
);

  logic [LANES-1:0] l_end, l_writes, l_unsup;
  logic [LANES-1:0] l_sadd, l_smul, l_srescale;

  // The genvar is declared SEPARATELY and the loop is wrapped in explicit
  // generate/endgenerate. Quartus 17.0.2's Verilog parser rejects
  // `for (genvar N = ...)` with
  //   Error (10170): Verilog HDL syntax error near text: "for";
  //                  expecting "endmodule"
  // and then Error (10112) ignores the whole design unit, while Verilator
  // and slang both accept it.
  //
  // FOUND 2026-09-09, and the way it was found is the point: this file has
  // been in the tree since 2026-08-30 with a passing lint and a passing
  // differential test, and it has NEVER been through quartus_map -- zero
  // rows in the fit ledger and zero in the map ledger. CLAUDE.md says it
  // exactly: a block that has never been through quartus_map has not been
  // shown to be synthesizable, however clean its lint.
  //
  // AND IT BROKE EVERY MAP IN THE REPOSITORY, not just its own.
  // run_block_map.ps1 compiles every .sv under fpga/rtl, so one unparseable
  // file fails Analysis & Synthesis for whatever module is being mapped.
  // A probe of an unrelated block died here and that is how this surfaced.
  //
  // Seven other files already carry this warning as a comment -- crc32c_fold,
  // field_v3_len, field_v3_mulbank, field_v3_normalize, field_v3_ring_svc,
  // raster_attrdiv_svc, raster_toon_div. The lesson was learned and fixed in
  // all of them, and this file was written afterwards without it. Same
  // half-fixed shape as the core.autocrlf guard found the same day.
  // Named `gl`, not `l`: hoisting the genvar out of the for header puts it in
  // module scope, where it would shadow the `int l` in the lane-desync loop
  // below. Verilator flags that as VARHIDDEN and it is a real hazard, not a
  // style complaint -- two different loop variables with one name.
  genvar gl;
  generate
  for (gl = 0; gl < LANES; gl++) begin : gen_lane
    zhao_field_alu u_alu (
        .op_i (op_i),
        .imm_i(imm_i),
        .a0_i (a0_i[32*gl+:32]),
        .a1_i (a1_i[32*gl+:32]),
        .a2_i (a2_i[32*gl+:32]),
        .b0_i (b0_i[32*gl+:32]),
        .b1_i (b1_i[32*gl+:32]),
        .b2_i (b2_i[32*gl+:32]),
        .c_i  (c_i[32*gl+:32]),
        .prod_ab_i(prod_ab_i[66*gl+:66]),
        .dot2_i   (dot2_i[66*gl+:66]),
        .dot3_i   (dot3_i[66*gl+:66]),
        .result_o (result_o[32*gl+:32]),
        .is_end_o        (l_end[gl]),
        .writes_o        (l_writes[gl]),
        .op_unsupported_o(l_unsup[gl]),
        .sat_add_o    (l_sadd[gl]),
        .sat_mul_o    (l_smul[gl]),
        .sat_rescale_o(l_srescale[gl])
    );
  end
  endgenerate

  assign is_end_o         = l_end[0];
  assign writes_o         = l_writes[0];
  assign op_unsupported_o = l_unsup[0];

  // THE MASK IS APPLIED ONCE, HERE, TO THE NARROW FLAGS. Everything else reads
  // these three ports. A padded lane's saturation stops at this line and cannot
  // reach a live point's status or the group ledger.
  assign sat_add_lane_o     = l_sadd     & lane_live_i;
  assign sat_mul_lane_o     = l_smul     & lane_live_i;
  assign sat_rescale_lane_o = l_srescale & lane_live_i;

  // The aggregate is a REDUCTION OF THE PUBLISHED DETAIL, not a second opinion
  // about it. Written as `|sat_*_lane_o` and deliberately NOT as
  // `|(l_* & lane_live_i)`: the two are equal today, and only the first stays
  // equal by construction if the masking above is ever changed.
  assign sat_add_o     = |sat_add_lane_o;
  assign sat_mul_o     = |sat_mul_lane_o;
  assign sat_rescale_o = |sat_rescale_lane_o;

  always_comb begin
    lane_desync_o = 1'b0;
    for (int l = 1; l < LANES; l++)
      if ((l_end[l] != l_end[0]) || (l_writes[l] != l_writes[0]) ||
          (l_unsup[l] != l_unsup[0]))
        lane_desync_o = 1'b1;
  end

endmodule : zhao_field_alu_vec

`default_nettype wire
