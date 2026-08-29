// zhao_probe_v3_exec.sv — Field v3 Phase 4: the vector executor DATAPATH,
// one lane.
//
// WHERE THIS SITS
// ---------------
//   prepared descriptor -> walker -> [THIS] -> patch accumulator -> cache
//
// The walker (zhao_probe_walk_earth) generates the points. The accumulator
// (zhao_probe_patch_acc) reduces the results. This is the middle: it runs a
// context's uops to END and produces that point's four output lanes.
//
// ONE LANE, AND THAT IS THE POINT. The engine is four-wide because a vector
// group is four points, and the four lanes are INDEPENDENT REPLICAS of this
// datapath sharing one instruction stream. Measuring one lane and multiplying
// is honest for storage and near-honest for logic; measuring four lanes is
// the next fit, not this one. Nothing here is written as though four lanes
// were free.
//
// WHY THE OPERANDS ARRIVE THE WAY THEY DO
// ----------------------------------------
// `zhao_field_alu` consumes SEVEN operands -- a, a+1, a+2, b, b+1, b+2, c --
// and `zhao_probe_banked_rf` delivers exactly those seven in one clock by
// banking on `register[1:0]` with three read replicas. That correspondence is
// not a coincidence and it is the argument for the banking: `a, a+1, a+2`
// necessarily land in three DIFFERENT residues, so no bank is ever asked for
// more than three reads.
//
// It is also the argument AGAINST the alternative sizing in
// `reports/Fieldv3.md`, which cuts the file as four lanes x three readers.
// Three readers per lane cannot serve seven operands in a clock, so every
// three-member op would have to be sequenced. See
// `reports/FIELD_V3_EXECUTOR_REGFILE.md` -- both shapes cost twelve memories
// of 8,192 bits, so this is capability at equal price, not a trade.
//
// WHAT THIS INCREMENT DOES NOT DO, STATED SO IT IS NOT MISTAKEN FOR DONE
// -----------------------------------------------------------------------
//   * NO DOT2/DOT3. Those need two and three products, and the brief budgets
//     "four 33-bit lanes at about 12 DSPs" -- one multiplier per lane. So a
//     dot product must be SEQUENCED over two or three clocks, and how much
//     that costs is a measurement, not a guess. `op_unsupported_o` from the
//     ALU is surfaced rather than hidden, so an unsupported op is loud.
//   * NO SCHEDULER. `zhao_probe_ctx_fifo` is the measured ready-context FIFO
//     and belongs in the composition, not in the datapath. This probe uses a
//     plain lowest-ready scan so that what is being measured is the DATAPATH.
//   * NO SERVICES. Curve and distance have their own probes and their own
//     queues.
//
// THE BARREL PROPERTY, which is the whole reason for contexts. One
// instruction is in flight per context (the brief's rule), and the datapath
// is five stages deep. So a single context issues one uop every five clocks,
// and the pipeline is only full when at least five contexts are ready. That
// is not a defect to be bypassed away -- it is why the engine is barrelled,
// and the probe reports occupancy so the effect is measured rather than
// assumed.
//
// Law:
//   reports/Fieldv3.md              Phase 4; prepared vector fabric
//   reports/PIPELINEINGHINTS        the banked register file directive
//   design/contracts/FIELD.SEQ.CORE.md   one semantic engine, profile
//                                   adapters permitted; v3 prepared fabric
//   spec/form/field-ir.md           canonical op semantics
//   reference/include/zfield/zfield_plan.hpp  execute_point, the oracle

`default_nettype none

module zhao_probe_v3_exec #(
    parameter int CTX  = 8,   // contexts in flight
    parameter int REGS = 32,  // vector registers per context
    parameter int PLAN = 32   // uops per context
) (
    input var logic clk,
    input var logic rst_n,

    // ---- uop store write (host loads a context's program) -----------------
    input var logic                       up_we_i,
    input var logic [$clog2(CTX)-1:0]     up_ctx_i,
    input var logic [$clog2(PLAN)-1:0]    up_pc_i,
    input var logic [7:0]                 up_op_i,
    input var logic [$clog2(REGS)-1:0]    up_dst_i,
    input var logic [$clog2(REGS)-1:0]    up_a_i,
    input var logic [$clog2(REGS)-1:0]    up_b_i,
    input var logic [$clog2(REGS)-1:0]    up_c_i,
    input var logic [31:0]                up_imm_i,

    // ---- register preload (host writes a context's input registers) -------
    input var logic                    pre_we_i,
    input var logic [$clog2(CTX)-1:0]  pre_ctx_i,
    input var logic [$clog2(REGS)-1:0] pre_reg_i,
    input var logic signed [31:0]      pre_data_i,

    // ---- run control -------------------------------------------------------
    input var logic                   start_i,
    input var logic [$clog2(CTX)-1:0] start_ctx_i,

    // ---- the register-file write, as a REQUEST ------------------------------
    // `wb_valid_o` was pure observation until 2026-08-28: a bare assign that
    // mirrored a write this block had already committed to its own file.
    //
    // THAT SHAPE CANNOT BE COMPOSED. zhao_field_v3_svcpath arbitrates one write
    // port between this ALU and the long-op drain, and it REFUSES the ALU by
    // design -- its own measurement is that the ALU loses exactly eight clocks
    // to the drain on every four-point group. Wired to an output that cannot be
    // refused, each of those eight would be a LOST REGISTER WRITE.
    //
    // So the write is a request now, and `wb_ready_i` is the grant. This is the
    // fourth time this engine has met an open-loop producer facing a consumer
    // that can refuse -- after the executor's DOT, the curve service's hang and
    // the dispatcher's missing operands -- and the first one caught BEFORE the
    // composition was built rather than minutes after.
    //
    // TIE IT HIGH AND THE BEHAVIOUR IS EXACTLY WHAT IT WAS. Every existing test
    // does that, which is why the mutants for the hold have to drive it low.
    output var logic                    wb_valid_o,   // a result WANTS to be written
    input  var logic                    wb_ready_i,   // ... and may

    // HOW MANY TIMES THE REGISTER FILE WAS ACTUALLY WRITTEN, preload excluded.
    //
    // Added because mutant X36 -- "the register file is written without a
    // grant" -- SURVIVED. Nothing in the test observes the file: the harness
    // rebuilds a shadow from the writeback STREAM, so a write that lands with
    // no grant is invisible to it, and a write that lands twice looks like one.
    //
    // The shadow cannot be fixed to see this. It is built from the same signals
    // the mutant leaves untouched. The only way to catch a write nobody
    // authorised is to count the writes the file really takes, which is what
    // this is.
    //
    // The law it enables is exact and worth stating: the file is written once
    // per GRANTED request and never otherwise, so this must equal the number of
    // transfers the arbiter saw. Not "about equal" -- equal.
    // ENFORCED-BY: tests/differential/field_v3_exec_directed.cpp:test_results_survive_contention
    output var logic [31:0]             rf_writes_o,

    // THE GRANTED WRITE, COMING BACK IN.
    //
    // This block asks for the port through `wb_valid_o` and the answer arrives
    // on `wb_ready_i`, but the REGISTER FILE IS IN HERE -- so whoever wins the
    // port has to be able to reach it. `zhao_field_v3_svcpath` arbitrates this
    // ALU against the long-op drain and its `wr_*_o` IS the write port; those
    // wires land here.
    //
    // The alternative was lifting the register file out of the executor, which
    // is a larger change to a block that is closed at 42/42, for no behaviour
    // that this does not already give.
    //
    // `zhao_probe_v3_engine` loops this straight back from the ALU's own
    // request, so an engine with nothing attached behaves exactly as it did --
    // which is what keeps every existing test meaningful rather than merely
    // passing.
    input  var logic                    wr_en_i,
    input  var logic [$clog2(CTX)-1:0]  wr_ctx_i,
    input  var logic [$clog2(REGS)-1:0] wr_reg_i,
    input  var logic signed [31:0]      wr_data_i,

    // THE SKID OVERFLOWED: a write was dropped for want of room. Sticky, and
    // an output rather than an assertion, because the depth is DERIVED and a
    // derivation can be wrong -- mine was, by one, and it cost 11 of 12
    // programs precisely because the overflow was silent.
    // ENFORCED-BY: tests/differential/field_v3_exec_directed.cpp:test_writes_survive_a_refusing_port
    output var logic                    sk_overflow_o,
    output var logic [$clog2(CTX)-1:0]  wb_ctx_o,
    output var logic [$clog2(REGS)-1:0] wb_reg_o,
    output var logic signed [31:0]      wb_data_o,

    output var logic                   done_valid_o,  // a context hit END
    output var logic [$clog2(CTX)-1:0] done_ctx_o,

    output var logic [CTX-1:0] active_o,
    // ---- the LONG-OP PATH, to zhao_field_v3_dispatch -----------------------
    // An op the ALU does not implement leaves here instead of raising
    // `unsupported_o`, and its CONTEXT PARKS until the service answers.
    //
    // That parking is the whole change. zhao_probe_ctx_fifo's header describes
    // the lifecycle -- "long op enters the service ... service completion
    // RE-ENQUEUES it" -- and this block used to hold a context for a FIXED
    // PIPELINE DEPTH, which is right for short ops and wrong for a wait whose
    // length nobody can predict.
    output var logic                      long_valid_o,
    input  var logic                      long_ready_i,
    output var logic [CW-1:0]             long_ctx_o,
    output var logic [7:0]                long_op_o,
    output var logic [RW-1:0]             long_dst_o,
    output var logic signed [31:0]        long_s0_o,
    output var logic signed [31:0]        long_s1_o,
    output var logic signed [31:0]        long_s2_o,
    output var logic signed [31:0]        long_s3_o,
    // OPERAND B'S SECOND MEMBER. Added 2026-08-29 for DIST2, which is the last
    // opcode the shipped Earth programs need and this machine did not serve.
    //
    // Its decode shape is {1, {2, 2, 0}, 2, 0}: operand a has TWO members and
    // operand b has TWO. Every long op before it had a single-member b, so
    // three ports for a and one for b covered them all -- and DIST2 simply
    // could not be expressed. The register already existed here; it was feeding
    // the DOT sequencer and was never exported.
    output var logic signed [31:0]        long_s4_o,
    output var logic [31:0]               long_imm_o,
    // "No further context can join this group." See the comment at its
    // assignment: an EAGER flush costs group size, a LATE one deadlocks.
    output var logic                      flush_o,
    // A parked context comes back.
    input  var logic                      rel_valid_i,
    input  var logic [CW-1:0]             rel_ctx_i,

    output var logic           unsupported_o,  // an op this increment omits

    // ---- saturation ledger, ORed across the run ---------------------------
    output var logic sat_add_o,
    output var logic sat_mul_o,
    output var logic sat_rescale_o,

    // ---- counters ----------------------------------------------------------
    output var logic [31:0] uops_issued_o,
    output var logic [31:0] idle_clocks_o,  // no context was ready to issue

    // ---- the shared multiplier bank (this lane's claimant port) -----------
    // One lane of a four-wide bank. The other three belong to the other three
    // points of a vector group, which are replicas of this datapath.
    output var logic               mul_req_valid_o,
    input  var logic               mul_req_ready_i,
    output var logic signed [32:0] mul_req_a_o,
    output var logic signed [32:0] mul_req_b_o,
    input  var logic               mul_rsp_valid_i,
    input  var logic signed [65:0] mul_rsp_p_i,

    // The multiplier's own valid must arrive in the same clock as S3. If it
    // ever does not, the product being fed to the ALU belongs to a DIFFERENT
    // instruction, which is a wrong answer rather than a slow one -- so it is
    // latched and reported instead of being assumed away.
    output var logic desync_o
);

  localparam int CW = $clog2(CTX);
  localparam int RW = $clog2(REGS);
  localparam int PW = $clog2(PLAN);

  // ---- the uop store -----------------------------------------------------
  // One entry per (context, pc). Written by the host, read at issue.
  typedef struct packed {
    logic [7:0]    op;
    logic [RW-1:0] dst;
    logic [RW-1:0] a;
    logic [RW-1:0] b;
    logic [RW-1:0] c;
    logic [31:0]   imm;
  } uop_t;

  uop_t store[0:(CTX*PLAN)-1];

  // ---- per-context state -------------------------------------------------
  logic [PW-1:0] pc_r[0:CTX-1];
  logic [CTX-1:0] active_r;    // started, not yet ENDed
  logic [CTX-1:0] inflight_r;  // has an instruction in the pipe
  // PARKED ON A SERVICE. A parked context is still `inflight` -- it must not
  // re-issue -- but it is not in the pipe, and it comes back on `rel_valid_i`
  // rather than on a schedule.
  logic [CTX-1:0] waiting_r;

  // The ops this block hands to a service rather than executing. Every one of
  // them is a four-point unit that borrows the shared bank; the list is the
  // generated op table's, not a guess.
  // DERIVED, NOT DECLARED. This used to be its own case list of ten
  // opcodes while the dispatcher kept a different list of eight -- and a
  // program using one of the two they disagreed about parked its context
  // forever. One table now answers both questions; see
  // zhao_field_ops_pkg.sv for what that cost and why it is not a patch.
  function automatic logic is_long(input logic [7:0] op);
    is_long = zhao_field_ops_pkg::field_is_long(op);
  endfunction

  logic long_at_s4_c;
  logic long_hold_c;

  assign active_o = active_r;

  // ---- issue: lowest ready context ---------------------------------------
  logic           issue_c;
  logic [CW-1:0]  issue_ctx_c;
  logic [CTX-1:0] ready_c;

  // ---- DOT sequencing: one multiplier, two or three products -------------
  // A DOT2 needs two products and a DOT3 three, against a budget of ONE
  // multiplier per lane (reports/Fieldv3.md: four 33-bit lanes at ~12 DSPs).
  // So they are sequenced, and the schedule is in
  // reports/FIELD_V3_DOT_SEQUENCING.md.
  //
  // THE ONE RULE THAT MAKES THIS SIMPLE: a DOT anywhere in the pipe freezes
  // ISSUE. Nothing can enter behind it, so no two instructions ever want the
  // multiplier in the same clock and there is no arbiter. Instructions AHEAD
  // of the DOT are unaffected -- each issued its own a0*b0 at its own S2, and
  // back-to-back issues are exactly what this multiplier supports.
  function automatic logic is_dot(input logic [7:0] op);
    is_dot = (op == 8'h10) || (op == 8'h11);  // OP_DOT2, OP_DOT3
  endfunction

  // The bank refused a request made this clock.
  logic mul_denied_c;
  assign mul_denied_c = mul_req_valid_o && !mul_req_ready_i;

  logic dot_inflight_c;
  assign dot_inflight_c = (s1_v_r && is_dot(s1_uop_r.op)) || (s2_v_r && is_dot(s2_op_r)) ||
                          (s3_v_r && is_dot(s3_op_r))     || (s4_v_r && is_dot(s4_op_r));

  always_comb begin
    ready_c = active_r & ~inflight_r;
    // A REFUSED REQUEST STALLS ISSUE. The register file's operands are live
    // for exactly one clock, so an instruction whose product the bank
    // declined to start cannot carry on -- its operands are gone next clock.
    issue_c = |ready_c && !dot_inflight_c && !hold_c && !mul_denied_c && !sk_busy_c;
    issue_ctx_c = '0;
    for (int i = CTX - 1; i >= 0; i--) if (ready_c[i]) issue_ctx_c = CW'(i);
  end

  // ---- S1: the fetched uop ------------------------------------------------
  logic          s1_v_r;
  logic [CW-1:0] s1_ctx_r;
  uop_t          s1_uop_r;

  // ---- S2: operands presented (RF read lands this clock) ------------------
  // Like S3, S2 carries only what survives the register-file read: the
  // operand register NUMBERS were spent addressing the file at S1.
  logic          s2_v_r;
  logic [CW-1:0] s2_ctx_r;
  logic [7:0]    s2_op_r;
  logic [RW-1:0] s2_dst_r;
  logic [31:0]   s2_imm_r;

  // ---- S3: product available ---------------------------------------------
  // S3 carries only what is still needed: the operand registers were consumed
  // at S1 by the register-file read and carrying them further would be dead
  // width in every pipeline register.
  logic          s3_v_r;
  logic [CW-1:0] s3_ctx_r;
  logic [7:0]    s3_op_r;
  logic [RW-1:0] s3_dst_r;
  logic [31:0]   s3_imm_r;
  logic signed [31:0] s3_a0_r, s3_a1_r, s3_a2_r;
  logic signed [31:0] s3_b0_r, s3_b1_r, s3_b2_r;
  logic signed [31:0] s3_c_r;

  // ---- S4: the product lands ----------------------------------------------
  // MEASURED, not assumed: zhao_field_mul is TWO clocks deep -- issue_i
  // registers the operands, and the product appears the clock after that. The
  // first version of this datapath put the ALU at S3, one clock early, and fed
  // it the PREVIOUS instruction's product. desync_o caught it on the first
  // run, which is the whole reason that signal is a port rather than a
  // comment. The operands are carried a second clock to meet the product.
  logic          s4_v_r;
  logic [CW-1:0] s4_ctx_r;
  logic [7:0]    s4_op_r;
  logic [RW-1:0] s4_dst_r;
  logic [31:0]   s4_imm_r;
  logic signed [31:0] s4_a0_r, s4_a1_r, s4_a2_r;
  logic signed [31:0] s4_b0_r, s4_b1_r, s4_b2_r;
  logic signed [31:0] s4_c_r;

  // ---- the register file, banked on register[1:0] -------------------------
  logic signed [31:0] rf_a0, rf_a1, rf_a2, rf_b0, rf_b1, rf_b2, rf_c;

  logic          rf_we_c;
  logic [CW-1:0] rf_wctx_c;
  logic [RW-1:0] rf_wreg_c;
  logic signed [31:0] rf_wdata_c;

  // THE FUNCTIONAL register file, not the fit probe. `zhao_probe_banked_rf`
  // measures the storage shape and says in its own header that it implements
  // no Field semantics: it addresses every bank with the SAME row, which
  // cannot read a group that crosses a multiple of four. This block used it
  // until 2026-08-28 and the differential passed 440 programs, because scalar
  // ops never read a+1 or a+2. The first DOT2 disagreed with the interpreter
  // on exactly the group starts that are 2 or 3 modulo 4.
  zhao_field_v3_rf #(
      .CONTEXTS(CTX),
      .REGS    (REGS)
  ) u_rf (
      .clk    (clk),
      .wr_en_i(rf_we_c),
      .wr_ctx_i(rf_wctx_c),
      .wr_reg_i(rf_wreg_c),
      .wr_data_i(rf_wdata_c),
      .rd_ctx_i(s1_ctx_r),
      .rd_a_i (s1_uop_r.a),
      .rd_b_i (s1_uop_r.b),
      .rd_c_i (s1_uop_r.c),
      .a0_o(rf_a0), .a1_o(rf_a1), .a2_o(rf_a2),
      .b0_o(rf_b0), .b1_o(rf_b1), .b2_o(rf_b2),
      .c_o (rf_c)
  );

  // ---- one multiplier, registered both sides ------------------------------
  // Its operand ports are MULTIPLEXED by which stage currently needs a
  // product. Ordinary ops issue a0*b0 at S2. A DOT additionally issues
  // a1*b1 at S3 and, for DOT3, a2*b2 at S4 -- taking each pair from the
  // stage where it is still live, because the register file's outputs are
  // valid for exactly one clock.
  logic signed [65:0] prod_ab;
  logic               prod_valid;

  logic               mul_issue_c;
  logic signed [32:0] mul_a_c, mul_b_c;

  // How many products this instruction still owes, counted down at S4.
  logic [1:0] dot_cnt_r;
  logic signed [65:0] dot_acc_r;

  logic dot2_at_s4_c, dot3_at_s4_c, hold_c;
  logic dot_at_s4_c;
  logic [1:0] dot_need_c;    // products this op needs: 2 for DOT2, 3 for DOT3
  logic [1:0] dot_issue_r;   // products ISSUED AND GRANTED so far
  // A two-deep shadow of "a multiply was granted", matching the multiplier's
  // two-clock latency. This is the invariant desync_o should always have been
  // checking.
  logic issued_s1_r, issued_s2_r;
  assign dot2_at_s4_c = s4_v_r && (s4_op_r == 8'h10);
  assign dot3_at_s4_c = s4_v_r && (s4_op_r == 8'h11);
  // DOT2 owes one more product after its first, DOT3 owes two. The hold ends
  // when the last one has been accumulated.
  assign dot_at_s4_c = dot2_at_s4_c || dot3_at_s4_c;
  assign dot_need_c  = dot3_at_s4_c ? 2'd3 : 2'd2;
  // Hold until every product this op needs has ARRIVED and been accumulated.
  // THE WRITEBACK REFUSAL IS NOT IN `hold_c`, AND THAT IS THE FINDING.
  //
  // It was, briefly, on 2026-08-28. The reasoning: a refused write must be
  // retried with the same operands, and freezing the pipe makes that true --
  // the argument written out for `mul_denied_c` above.
  //
  // THIS BLOCK ALREADY LEARNED THAT ARGUMENT IS WRONG, one screen further
  // down, in the paragraph introducing the downstream region:
  //
  //     THE MULTIPLIER IS A FIXED-LATENCY PIPE AND CANNOT BE STALLED. A
  //     product issued at T arrives at T+2 whatever this block does.
  //     Freezing the whole datapath on a denial therefore held an
  //     instruction back while its product still arrived on schedule, and
  //     the two desynchronised -- measured as 2 of 12 programs wrong.
  //
  // Adding a writeback hold to `hold_c` freezes that same downstream region.
  // Measured, with the port refusing and the rival contending together:
  // 1 OF 12 PROGRAMS WRONG. The same defect, re-created for a different
  // reason, in the file that already carried the warning.
  //
  // It hid because each contention alone is harmless. Refusal with the rival
  // silent passed 35 checks -- no product is ever in flight across the
  // freeze. Two sources of back-pressure, and testing them one at a time
  // proves nothing about either.
  //
  // WHAT A CORRECT VERSION NEEDS, so the next attempt does not restart from
  // nothing: S4 must stall without the downstream stalling, which means a
  // SKID -- the retiring write moves to a holding register, S4 drains, and
  // ISSUE stops while the skid is occupied. The part that makes it more than
  // an afternoon is DEPTH: between the skid filling and issue actually
  // stopping, the instructions already in S1..S3 still arrive, so one entry
  // is not obviously enough and the bound must be derived, not assumed.
  //
  // UNTIL THEN: `wb_ready_i` gates the WRITE and not the pipe, so nothing is
  // written without a grant, but a refusal LOSES the result. The engine ties
  // it high -- the behaviour that shipped and is verified. It must not be
  // wired to an arbiter that can say no until the skid exists.
  assign retire_hold_c = (dot_at_s4_c && (dot_cnt_r < dot_need_c)) || long_hold_c;
  assign hold_c        = retire_hold_c;

  // A LONG OP AT S4 HOLDS THE PIPE UNTIL THE DISPATCHER TAKES IT. The operands
  // live in the s4_* registers and the next instruction would overwrite them,
  // so the alternative is a per-context parking copy of seven values. Holding
  // costs a clock or two and no state at all; the dispatcher accepts unless it
  // is mid-group with a different op.
  assign long_at_s4_c = s4_v_r && is_long(s4_op_r);
  assign long_hold_c  = long_at_s4_c && !(long_valid_o && long_ready_i);

  assign long_valid_o = long_at_s4_c;
  assign long_ctx_o   = s4_ctx_r;
  assign long_op_o    = s4_op_r;
  assign long_dst_o   = s4_dst_r;
  assign long_s0_o    = s4_a0_r;
  assign long_s1_o    = s4_a1_r;
  assign long_s2_o    = s4_a2_r;
  assign long_s3_o    = s4_b0_r;
  assign long_s4_o    = s4_b1_r;
  assign long_imm_o   = s4_imm_r;

  // AN EAGER FLUSH COSTS GROUP SIZE; A LATE ONE DEADLOCKS. The safe rule is
  // "no active context can still join", and a context can still join exactly
  // while it is running rather than parked -- every context runs the same
  // program, so a running one will reach this instruction.
  //
  // So flush when every active context is parked. With eight contexts the
  // group fills long before that; with one, it is the only thing that ever
  // issues the group at all.
  assign flush_o = ~|(active_r & ~waiting_r);

  always_comb begin
    // `!hold_c` IS THE SAME LAW THE DOT SEQUENCE BELOW WAS REWRITTEN FOR,
    // APPLIED TO THE SCALAR PATH TOO.
    //
    // The note below states it: an instruction CANNOT BE STALLED between its
    // multiply issue and its product arrival, because the product lands two
    // clocks later on a fixed schedule. The DOT sequence was moved to S4 to
    // obey that. The scalar issue stayed at S2 and UNGATED, so a long-op
    // handover holding the pipe re-issued the same multiply on every held
    // clock -- one instruction, k+1 products, and only the count was ever
    // checked. `desync_o` compares how MANY products came back, never which
    // instruction they belonged to, so the surplus was invisible.
    //
    // Issuing only on a clock the instruction will actually advance makes it
    // exactly one product per multiply, arriving exactly when the instruction
    // reaches the stage that consumes it.
    //
    // Found by the composed Earth gate: a MUL wrote a wrong product from two
    // operands it had read CORRECTLY, and only at six or more contexts --
    // which is where long-op holds last more than a single clock.
    mul_issue_c = s2_v_r && !is_dot(s2_op_r) && !hold_c;
    // THE SAME HELD OPERANDS THE PIPE USES. The multiply is issued from S2,
    // whose operands come from the register file's moving read -- so a retry
    // after a denial would multiply the SUCCESSOR's numbers and hand the
    // product to the stalled instruction. Fixing only the S3 capture left 5
    // of 48 wrong; this is the other half of the same defect.
    mul_a_c     = 33'(use_a0_c);
    mul_b_c     = 33'(use_b0_c);
    // THE WHOLE DOT SEQUENCE IS ISSUED FROM S4, and that is the fix.
    //
    // An instruction CANNOT BE STALLED between its multiply issue and its
    // product arrival: the product lands two clocks later on a fixed
    // schedule, so any delay makes the instruction miss it. Issuing a0*b0 at
    // S2 and a1*b1 at S3 spread ONE sequence across THREE MOVING STAGES, and
    // a refusal at any of them broke it on one of two horns -- advance and
    // consume a product that was never issued, or hold and miss one that
    // arrived anyway. Five attempts failed alternately on each.
    //
    // At S4 the operands sit in registers that do not move for the whole
    // sequence. Each product is issued, retried on refusal, and accumulated
    // when it lands. Nothing can miss anything, by construction, and a
    // refusal costs only a clock. That reasoning was wrong five times before
    // it was right, so it is checked rather than trusted: the named test
    // drives the engine's rival on a pseudo-random schedule so the bank
    // really refuses, and requires the answers not to move. With the rival
    // silent this path is dead code -- which is how an earlier sweep of it
    // scored 4 of 11.
    //
    // ENFORCED-BY: tests/differential/field_v3_exec_directed.cpp:test_results_survive_contention
    if (dot_at_s4_c && (dot_issue_r < dot_need_c)) begin
      mul_issue_c = 1'b1;
      unique case (dot_issue_r)
        2'd0:    begin mul_a_c = 33'(s4_a0_r); mul_b_c = 33'(s4_b0_r); end
        2'd1:    begin mul_a_c = 33'(s4_a1_r); mul_b_c = 33'(s4_b1_r); end
        default: begin mul_a_c = 33'(s4_a2_r); mul_b_c = 33'(s4_b2_r); end
      endcase
    end
  end

  // THE MULTIPLIER IS NOT OURS. It lives in zhao_field_v3_mulbank, shared
  // with the curve and distance services, because zhao_field_exec_shared
  // measured what happens when every op unit owns one: 79 DSPs of a 112-DSP
  // device, with nine units idle at any instant.
  //
  // This block was written with a private multiplier as a STATED
  // simplification. Removing it is what lets the composition exist at all.
  assign mul_req_valid_o = mul_issue_c;
  assign mul_req_a_o     = mul_a_c;
  assign mul_req_b_o     = mul_b_c;
  assign prod_ab         = mul_rsp_p_i;
  assign prod_valid      = mul_rsp_valid_i;

  // The sum is formed at the FULL 66-bit product width and rescaled ONCE by
  // the ALU. Rescaling each product and adding is a different answer, and
  // zfield's dot2/dot3 are the single-rounding form.
  logic signed [65:0] dot_sum_c;
  logic signed [65:0] dot_total_c;
  // The adder takes the product arriving this clock; the ALU takes the
  // finished total. Keeping these separate is what removes the old
  // requirement that the LAST product arrive on exactly the release clock.
  assign dot_sum_c   = dot_acc_r + prod_ab;
  assign dot_total_c = dot_acc_r;

  // ---- the op law, reused verbatim from the v2 engine ---------------------
  logic signed [31:0] alu_result;
  logic               alu_is_end, alu_writes, alu_unsupported;
  logic               alu_sat_add, alu_sat_mul, alu_sat_rescale;

  zhao_field_alu u_alu (
      .op_i  (s4_op_r),
      .imm_i (s4_imm_r),
      .a0_i  (s4_a0_r), .a1_i(s4_a1_r), .a2_i(s4_a2_r),
      .b0_i  (s4_b0_r), .b1_i(s4_b1_r), .b2_i(s4_b2_r),
      .c_i   (s4_c_r),
      .prod_ab_i(prod_ab),
      // The accumulated sum, at full product width. The ALU rescales it once.
      // Only one of these is read per op, and both carry the same accumulator
      // because only one DOT is ever in flight -- the issue freeze guarantees
      // it.
      .dot2_i(dot_total_c),
      .dot3_i(dot_total_c),
      .result_o(alu_result),
      .is_end_o(alu_is_end),
      .writes_o(alu_writes),
      .op_unsupported_o(alu_unsupported),
      .sat_add_o(alu_sat_add),
      .sat_mul_o(alu_sat_mul),
      .sat_rescale_o(alu_sat_rescale)
  );

  // A DOT op reaching this increment is unsupported even though the ALU could
  // name it, because the products it needs were never computed.
  //
  // AND IT MUST NOT WRITE. The ALU KNOWS OP_DOT2/OP_DOT3 -- they are real arms
  // of its decode, not the `default` refusal -- so it leaves writes_o HIGH and
  // produces a result computed from the zero I hand it on dot2_i/dot3_i. Left
  // to itself the block would therefore flag the op as unsupported AND write
  // the garbage anyway, which is the worst of both: a wrong value in a live
  // register, under a flag that says it was refused.
  //
  // Found 2026-08-28 by the test written to close mutant X11, which had
  // survived precisely because nothing checked that a refused op leaves the
  // register file alone. The header of this file already CLAIMED the write was
  // refused; the claim was wrong until this line existed.
  // DOT USED TO BE REFUSED HERE and is now IMPLEMENTED, so this term is gone
  // from the write enable. What remains refused is an opcode the ALU itself
  // does not know, via its own `default` arm clearing writes_o -- a different
  // gate, and the one mutant X11 attacks.
  logic dot_here_c;
  assign dot_here_c = 1'b0;

  logic wb_req_c, retire_hold_c;

  // ---- OPERANDS HELD ACROSS A DENIAL --------------------------------------
  //
  // THE REGISTER FILE'S READ IS A PIPELINE STAGE, AND A FREEZE DOES NOT FREEZE
  // IT. The address is driven from S1 and the data arrives one clock later,
  // while the instruction is at S2. Freezing S1 and S2 on a denial stops the
  // instructions moving -- it does NOT un-issue the read that is already in
  // flight, so the data that arrives on the next clock belongs to the
  // instruction BEHIND the stalled one.
  //
  // S3 then captures the stalled instruction's control (ctx, op, dst) together
  // with its SUCCESSOR's operands, and computes a perfectly plausible wrong
  // answer for the right destination.
  //
  // The comment on the stall says "the refused instruction retries next clock
  // with the same operands, because the S1 read address is unchanged and the
  // register file re-presents them". The address is indeed unchanged. The DATA
  // is one clock behind it, and that is the half that was missed.
  //
  // WHY IT HID: with ONE context the pipe is nearly empty -- 13 uops in 69
  // clocks -- so S1 is usually a bubble during a denial, the address has not
  // moved, and the data that arrives is the stalled instruction's own. Four
  // contexts fill the pipe and every denial lands behind a real instruction:
  // 21 of 48 context-programs wrong, and the single-context test green
  // throughout.
  // The upstream stage registers do not move on these clocks -- and the
  // register file's read does. Exactly the condition guarding the upstream
  // block below, named once so the two cannot drift apart.
  logic               upstream_frozen_c;
  assign upstream_frozen_c = hold_c || mul_denied_c;

  logic               opnd_held_r;
  logic signed [31:0] h_a0_r, h_a1_r, h_a2_r, h_b0_r, h_b1_r, h_b2_r, h_c_r;

  logic signed [31:0] use_a0_c, use_a1_c, use_a2_c, use_b0_c, use_b1_c, use_b2_c, use_c_c;
  always_comb begin
    // On the denial clock the data IS the stalled instruction's, so take it
    // and remember it. On the clocks after, the file has moved on: use what
    // was remembered.
    // EVERY UPSTREAM FREEZE, NOT JUST A DENIAL.
    //
    // The first version of this held operands only across `mul_denied_c`, and
    // that was half the problem. `hold_c` freezes the upstream too -- for a
    // DOT accumulating, for a long op waiting to be handed over -- and the
    // register file's read does not stop for either. Same defect, second
    // freeze.
    //
    // It surfaced in the composed machine as the PENULTIMATE context started
    // receiving the LAST-started context's answer, and only with eight
    // contexts: the long-op handover wait is the freeze, and it takes a full
    // second group for one to sit behind another.
    use_a0_c = (opnd_held_r && !upstream_frozen_c) ? h_a0_r : rf_a0;
    use_a1_c = (opnd_held_r && !upstream_frozen_c) ? h_a1_r : rf_a1;
    use_a2_c = (opnd_held_r && !upstream_frozen_c) ? h_a2_r : rf_a2;
    use_b0_c = (opnd_held_r && !upstream_frozen_c) ? h_b0_r : rf_b0;
    use_b1_c = (opnd_held_r && !upstream_frozen_c) ? h_b1_r : rf_b1;
    use_b2_c = (opnd_held_r && !upstream_frozen_c) ? h_b2_r : rf_b2;
    use_c_c  = (opnd_held_r && !upstream_frozen_c) ? h_c_r  : rf_c;
  end




  // ---- THE WRITEBACK SKID -------------------------------------------------
  //
  // S4 CANNOT BE STALLED. The multiplier is a fixed-latency pipe: a product
  // issued at T arrives at T+2 whatever this block does, so any scheme that
  // holds an instruction back while its product arrives on schedule
  // desynchronises the two. Measured twice: 2 of 12 programs wrong when a
  // freeze was tried for multiplier denials, and 1 of 12 when it was tried
  // again for the write port. The write therefore LEAVES on time and waits.
  //
  // DEPTH IS DERIVED. Issue stops the moment the skid is non-empty. At that
  // instant S1, S2 and S3 may each hold an instruction, and they drain into S4
  // over the next three clocks whatever the port does. One entry for the write
  // that could not go out plus three retiring behind it is FOUR --
  //
  // -- AND THE GATE MUST BE COMBINATIONAL FOR THAT TO HOLD. `sk_ne_c` alone is
  // registered, so issue would stop a clock late and a FIFTH instruction would
  // already be entering S1. That is one more than the depth allows and it
  // overflowed in silence, because a dropped write changes a VALUE and not a
  // COUNT -- no transfer-count law can see it. `sk_overflow_o` exists so the
  // derivation can never fail quietly again.
  //
  // A BYPASS KEEPS THE COMMON CASE FREE: when the skid is empty and the port
  // grants, the write goes straight out and nothing is pushed, so an
  // uncontended engine never stalls issue. The barrel numbers are unchanged at
  // 69 and 190 clocks, which is how that claim is checked rather than asserted.
  //
  // AND THE READ-AFTER-WRITE HAZARD ANSWERS ITSELF: a context whose write is
  // still in the skid has retired, so it could re-issue and read its own stale
  // register -- but issue is stopped while the skid is non-empty, so the
  // earliest it can issue again is the clock after its write lands.
  localparam int SKD = 4;

  logic [$clog2(SKD+1)-1:0] sk_n_r;
  logic [CW-1:0]            sk_ctx_r  [SKD];
  logic [RW-1:0]            sk_reg_r  [SKD];
  logic signed [31:0]       sk_data_r [SKD];
  logic [$clog2(SKD)-1:0]   sk_hd_r, sk_tl_r;

  logic sk_ne_c, sk_push_c, sk_pop_c, sk_busy_c;
  assign sk_ne_c = (sk_n_r != '0);

  assign wb_valid_o = sk_ne_c || wb_req_c;
  assign wb_ctx_o   = sk_ne_c ? sk_ctx_r[sk_hd_r]  : s4_ctx_r;
  assign wb_reg_o   = sk_ne_c ? sk_reg_r[sk_hd_r]  : s4_dst_r;
  assign wb_data_o  = sk_ne_c ? sk_data_r[sk_hd_r] : alu_result;

  assign sk_pop_c  = sk_ne_c && wb_ready_i;
  assign sk_push_c = wb_req_c && (sk_ne_c || !wb_ready_i);
  assign sk_busy_c = sk_ne_c || sk_push_c;

  // ---- writeback ----------------------------------------------------------
  // The host preload wins the port when it is asserted; the machine is not
  // running during preload, so there is no contention to arbitrate.
  always_comb begin
    if (pre_we_i) begin
      rf_we_c    = 1'b1;
      rf_wctx_c  = pre_ctx_i;
      rf_wreg_c  = pre_reg_i;
      rf_wdata_c = pre_data_i;
    end else begin
      // GATED ON THE GRANT. Without `&& wb_ready_i` the block would commit the
      // write to its own file while telling the arbiter it still wants the
      // port -- and then, once granted, write it a SECOND time through the
      // shared port. A duplicated write is invisible for an idempotent value
      // and wrong for every accumulating one.
      // THE FILE IS WRITTEN BY WHOEVER WON THE PORT, not by this block's own
      // request. With the engine looping it back these are the same wires and
      // the same clock; with an arbiter in between they are not, and that is
      // the entire point -- the drain's writes have to be able to land here
      // too.
      rf_we_c    = wr_en_i;
      rf_wctx_c  = wr_ctx_i;
      rf_wreg_c  = wr_reg_i;
      rf_wdata_c = wr_data_i;
    end
  end

  // ONE EXPRESSION, TWO READERS: the request and the register file's write
  // enable must be the same condition, or the block can write without being
  // granted -- the one failure that corrupts a register rather than merely
  // losing a result.
  //
  // THE WRITE FIRES ONCE, ON THE CLOCK THE INSTRUCTION RETIRES.
  //
  // It used to fire on EVERY clock the instruction sat at S4 -- so a DOT
  // writing its destination while accumulating wrote that register three or
  // four times, once per hold clock, and a long op held awaiting handoff wrote
  // on every clock of the wait. The last write carried the right value, so
  // every check in this file passed and the defect was invisible.
  //
  // It stopped being invisible the moment the port could REFUSE, because then
  // the duplicates are not free: they are port slots. The count that found it
  // is "the same number of writes transfers whether or not the port refuses",
  // which failed at 20 granted against 16 refused -- the granted figure was
  // the inflated one.
  //
  // With a shared port this is not cosmetic. Every duplicate steals a slot the
  // drain wanted, and each one momentarily publishes a PARTIAL accumulation to
  // a register another context can read.
  // NOT GATED ON `mul_denied_c`, AND THAT TERM WAS A LOST WRITE.
  //
  // A denial is BACK-PRESSURE UPSTREAM ONLY -- the downstream region, retire
  // included, keeps draining, as the paragraph introducing it says in as many
  // words. So an instruction at S4 RETIRES during a denial. Suppressing its
  // write on that clock does not delay the write, it DELETES it: the
  // instruction is gone by the next clock.
  //
  // I added the term while fixing the opposite bug -- the write firing on
  // EVERY clock an instruction sat at S4 -- and reached for both freeze
  // conditions without checking that they differ. `retire_hold_c` really does
  // hold S4. `mul_denied_c` does not.
  //
  // It survived every check for hours. It took four contexts and burst
  // refusals to land a denial and a retiring write on the same clock often
  // enough to see: one context with coin-flip grants almost never does.
  assign wb_req_c   = s4_v_r && alu_writes && !alu_is_end && !dot_here_c &&
                      !retire_hold_c;



  assign done_valid_o = s4_v_r && alu_is_end;
  assign done_ctx_o   = s4_ctx_r;

  // ---- the pipe -----------------------------------------------------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      s1_v_r        <= 1'b0;
      s2_v_r        <= 1'b0;
      s4_v_r        <= 1'b0;
      active_r      <= '0;
      inflight_r    <= '0;
      waiting_r     <= '0;
      sk_n_r        <= '0;
      sk_hd_r       <= '0;
      sk_tl_r       <= '0;
      sk_overflow_o <= 1'b0;
      for (int i = 0; i < SKD; i++) begin
        sk_ctx_r[i]  <= '0;
        sk_reg_r[i]  <= '0;
        sk_data_r[i] <= 32'sd0;
      end
      opnd_held_r   <= 1'b0;
      h_a0_r        <= 32'sd0;
      h_a1_r        <= 32'sd0;
      h_a2_r        <= 32'sd0;
      h_b0_r        <= 32'sd0;
      h_b1_r        <= 32'sd0;
      h_b2_r        <= 32'sd0;
      h_c_r         <= 32'sd0;
      rf_writes_o   <= 32'd0;
      unsupported_o <= 1'b0;
      desync_o      <= 1'b0;
      issued_s1_r   <= 1'b0;
      issued_s2_r   <= 1'b0;
      dot_acc_r     <= '0;
      dot_cnt_r     <= 2'd0;
      dot_issue_r   <= 2'd0;
      sat_add_o     <= 1'b0;
      sat_mul_o     <= 1'b0;
      sat_rescale_o <= 1'b0;
      uops_issued_o <= 32'd0;
      idle_clocks_o <= 32'd0;
      for (int i = 0; i < CTX; i++) pc_r[i] <= '0;
    end else begin
      if (up_we_i)
        store[(int'(up_ctx_i) * PLAN) + int'(up_pc_i)] <=
            '{op: up_op_i, dst: up_dst_i, a: up_a_i, b: up_b_i, c: up_c_i, imm: up_imm_i};

      if (start_i) begin
        active_r[start_ctx_i] <= 1'b1;
        pc_r[start_ctx_i]     <= '0;
      end

      // THE DOT HOLD, and it lives OUTSIDE the freeze below on purpose: this
      // is the counter that ENDS the hold, so gating it with `!hold_c` would
      // deadlock the pipe -- which is exactly what the first version did, and
      // the barrel test caught it as every context failing to finish.
      // TWO SEPARATE EVENTS, TRACKED SEPARATELY. `dot_issue_r` counts products
      // the bank GRANTED; `dot_cnt_r` counts products that ARRIVED. Conflating
      // them is what made the third product never get issued in an earlier
      // attempt: the first arrival advanced the counter and the issue
      // condition, keyed on that same counter, went false.
      if (dot_at_s4_c && (dot_issue_r < dot_need_c) && !mul_denied_c)
        dot_issue_r <= dot_issue_r + 2'd1;

      if (dot_at_s4_c && prod_valid && (dot_cnt_r < dot_need_c)) begin
        dot_acc_r <= dot_sum_c;
        dot_cnt_r <= dot_cnt_r + 2'd1;
      end else if (s4_v_r && !dot_at_s4_c) begin
        dot_acc_r   <= '0;
        dot_cnt_r   <= 2'd0;
        dot_issue_r <= 2'd0;
      end

      // Clear on the clock the DOT actually retires, so the next one starts
      // from zero whatever the timing was.
      if (dot_at_s4_c && !hold_c) begin
        dot_acc_r   <= '0;
        dot_cnt_r   <= 2'd0;
        dot_issue_r <= 2'd0;
      end

      // THE SKID MOVES EVERY CLOCK, outside every hold: it exists so the pipe
      // can keep draining while a write waits.
      if (sk_push_c) begin
        sk_ctx_r[sk_tl_r]  <= s4_ctx_r;
        sk_reg_r[sk_tl_r]  <= s4_dst_r;
        sk_data_r[sk_tl_r] <= alu_result;
        sk_tl_r            <= (sk_tl_r == $clog2(SKD)'(SKD - 1)) ? '0 : sk_tl_r + 1'b1;
      end
      if (sk_pop_c) sk_hd_r <= (sk_hd_r == $clog2(SKD)'(SKD - 1)) ? '0 : sk_hd_r + 1'b1;
      if (sk_push_c && !sk_pop_c && (sk_n_r == $clog2(SKD+1)'(SKD))) sk_overflow_o <= 1'b1;
      if (sk_push_c && !sk_pop_c)      sk_n_r <= sk_n_r + 1'b1;
      else if (sk_pop_c && !sk_push_c) sk_n_r <= sk_n_r - 1'b1;

      // CAPTURE ON THE FIRST DENIED CLOCK, RELEASE WHEN THE INSTRUCTION MOVES.
      // On the denial clock the file is still presenting the stalled
      // instruction's operands, so that is the moment to keep them.
      if (upstream_frozen_c) begin
        if (!opnd_held_r) begin
          opnd_held_r <= 1'b1;
          h_a0_r <= rf_a0;
          h_a1_r <= rf_a1;
          h_a2_r <= rf_a2;
          h_b0_r <= rf_b0;
          h_b1_r <= rf_b1;
          h_b2_r <= rf_b2;
          h_c_r  <= rf_c;
        end
      end else begin
        opnd_held_r <= 1'b0;
      end

      // THE MULTIPLIER'S ACCOUNTING LIVES OUT HERE, WITH THE MULTIPLIER.
      //
      // It used to sit inside the upstream block, which a denial freezes. The
      // multiplier is not frozen by anything -- a product issued at T arrives
      // at T+2 regardless -- so during a denial these registers stopped
      // shifting while products kept arriving, and the two lost each other.
      //
      // With ONE context the pipe is mostly empty (13 uops in 69 clocks) and a
      // denial almost never lands on a clock with a product in flight. With
      // FOUR it is constant: desync latched on 12 of 12 programs and 21 of 48
      // context-programs came out wrong, with no write port involved at all.
      //
      // This is the same law the region below is introduced with, applied to
      // the bookkeeping as well as the datapath: a denial is BACK-PRESSURE
      // UPSTREAM ONLY.
      issued_s1_r <= mul_issue_c && !mul_denied_c;
      issued_s2_r <= issued_s1_r;
      if (prod_valid != issued_s2_r) desync_o <= 1'b1;

      // COUNTED HERE rather than beside `rf_we_c`, because `rf_we_c` is
      // combinational and this must tick once per CLOCK on which the file is
      // written. Preload excluded: that is the host filling the file, not the
      // machine writing a result.
      if (rf_we_c && !pre_we_i) rf_writes_o <= rf_writes_o + 32'd1;  // whoever won it

      // Every stage advance below is gated on `!hold_c` AND `!mul_denied_c`.
      //
      // THE DENIAL GATE WAS MISSING AND IT WAS A REAL BUG. Suppressing ISSUE
      // alone stops new instructions entering, but the instruction already at
      // S2 when the bank refused it carried on to S3 and S4 and consumed a
      // product that was never computed -- its operands are gone by then,
      // because the register file holds them for exactly one clock.
      //
      // Found by the contention test: 5 of 12 programs gave wrong answers and
      // desync_o latched. Freezing the WHOLE pipe makes the refused
      // instruction retry next clock with the same operands, because the S1
      // read address is unchanged and the register file re-presents them.
      if (!hold_c && !mul_denied_c) begin
      // S0 -> S1: issue and fetch
      s1_v_r <= issue_c;
      if (issue_c) begin
        s1_ctx_r              <= issue_ctx_c;
        s1_uop_r              <= store[(int'(issue_ctx_c) * PLAN) + int'(pc_r[issue_ctx_c])];
        inflight_r[issue_ctx_c] <= 1'b1;
        uops_issued_o         <= uops_issued_o + 32'd1;
      end else if (|active_r) begin
        idle_clocks_o <= idle_clocks_o + 32'd1;
      end

      // S1 -> S2: RF read is in flight
      s2_v_r   <= s1_v_r;
      s2_ctx_r <= s1_ctx_r;
      s2_op_r  <= s1_uop_r.op;
      s2_dst_r <= s1_uop_r.dst;
      s2_imm_r <= s1_uop_r.imm;


      // DESYNC IS ABOUT THE MULTIPLIER'S CONTRACT, not about stage occupancy.
      //
      // This used to compare s4_v_r against prod_valid, which assumed exactly
      // one product per instruction, aligned with S4. True until a DOT began
      // issuing two or three products during a hold -- and then the guard
      // fired on entirely correct behaviour, which is worse than not having
      // it, because a guard that cries wolf gets read as noise.
      //
      // The real invariant is the multiplier's own: a product arrives exactly
      // two clocks after a GRANTED issue and at no other time. That holds for
      // every op, contended or not, and needs no knowledge of the pipeline.


      end  // upstream: !hold_c && !mul_denied_c

      // ---- DOWNSTREAM: held only by hold_c, NEVER by a denial -------------
      //
      // THE MULTIPLIER IS A FIXED-LATENCY PIPE AND CANNOT BE STALLED. A
      // product issued at T arrives at T+2 whatever this block does. Freezing
      // the whole datapath on a denial therefore held an instruction back
      // while its product still arrived on schedule, and the two
      // desynchronised -- measured as 2 of 12 programs wrong with desync_o
      // latched, after freezing everything had already improved it from 5.
      //
      // So a denial is BACK-PRESSURE UPSTREAM ONLY. S1 and S2 hold, so the
      // refused instruction retries next clock with the same operands. S3, S4
      // and retire keep draining, so instructions whose products are already
      // in flight still meet them.
      if (!hold_c) begin
      // S2 -> S3, in the DOWNSTREAM region with a BUBBLE on denial.
      //
      // A STALL NEEDS A BUBBLE, NOT A FREEZE. Holding S3 while S4 still read
      // it duplicated the instruction -- S4 consumed the same one twice, and
      // that measured WORSE (5 of 12 wrong) than freezing everything (2 of
      // 12). On a denial S2 holds its instruction and S3 is driven INVALID,
      // so nothing advances twice and nothing consumes a product that was
      // never issued.
      s3_v_r   <= mul_denied_c ? 1'b0 : s2_v_r;
      s3_ctx_r <= s2_ctx_r;
      s3_op_r  <= s2_op_r;
      s3_dst_r <= s2_dst_r;
      s3_imm_r <= s2_imm_r;
      s3_a0_r  <= use_a0_c;
      s3_a1_r  <= use_a1_c;
      s3_a2_r  <= use_a2_c;
      s3_b0_r  <= use_b0_c;
      s3_b1_r  <= use_b1_c;
      s3_b2_r  <= use_b2_c;
      s3_c_r   <= use_c_c;

      // S3 -> S4: carry a second clock so the operands meet their product
      s4_v_r   <= s3_v_r;
      s4_ctx_r <= s3_ctx_r;
      s4_op_r  <= s3_op_r;
      s4_dst_r <= s3_dst_r;
      s4_imm_r <= s3_imm_r;
      s4_a0_r  <= s3_a0_r;
      s4_a1_r  <= s3_a1_r;
      s4_a2_r  <= s3_a2_r;
      s4_b0_r  <= s3_b0_r;
      s4_b1_r  <= s3_b1_r;
      s4_b2_r  <= s3_b2_r;
      s4_c_r   <= s3_c_r;

      // S4: retire -- or PARK, if the op belongs to a service.
      //
      // A parked context keeps `inflight_r` so it cannot re-issue, gains
      // `waiting_r` so the flush rule can see it, and does NOT advance its pc:
      // the instruction has not finished, it has been handed over.
      if (s4_v_r && long_at_s4_c) begin
        if (long_ready_i) begin
          waiting_r[s4_ctx_r] <= 1'b1;
        end
      end else if (s4_v_r) begin
        inflight_r[s4_ctx_r] <= 1'b0;
        if (alu_is_end) begin
          active_r[s4_ctx_r] <= 1'b0;
        end else begin
          pc_r[s4_ctx_r] <= pc_r[s4_ctx_r] + PW'(1);
        end
        if (alu_unsupported || dot_here_c) unsupported_o <= 1'b1;
        if (alu_sat_add) sat_add_o <= 1'b1;
        if (alu_sat_mul) sat_mul_o <= 1'b1;
        if (alu_sat_rescale) sat_rescale_o <= 1'b1;
      end

      end  // downstream: !hold_c

      // THE SERVICE ANSWERED. The context leaves the parked set, leaves the
      // pipe, and only NOW advances -- the write has already landed through
      // the writeback arbiter, which is why the release is the dispatcher's to
      // send and not this block's to guess.
      //
      // OUTSIDE EVERY HOLD, AND THAT IS NOT A DETAIL. `rel_valid_i` is a
      // PULSE from the dispatcher's drain; it is not a handshake and nothing
      // re-sends it. Gated by `!hold_c` this block is DEAF to it exactly when
      // it is holding -- and one of the things it holds for is waiting to hand
      // over the NEXT long op.
      //
      // That deadlocks with five contexts and up, and only with five: four fit
      // in one group, so the executor never has a fifth to hand over while the
      // first four are outstanding. Measured in the composed machine --
      // n=4 finishes in 42 clocks, n=5 never finishes at all, and the rival
      // makes no difference either way.
      //
      // The comment fifty lines above already says a parked context "is not in
      // the pipe". Its bookkeeping therefore has no business being gated by a
      // PIPELINE hold, which is the same argument that moved the multiplier's
      // accounting out of the upstream region earlier today.
      if (rel_valid_i) begin
        waiting_r[rel_ctx_i]  <= 1'b0;
        inflight_r[rel_ctx_i] <= 1'b0;
        pc_r[rel_ctx_i]       <= pc_r[rel_ctx_i] + PW'(1);
      end
    end
  end

endmodule

`default_nettype wire
