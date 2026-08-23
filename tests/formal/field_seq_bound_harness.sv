// field_seq_bound_harness.sv — formal harness for
// tests/formal/field_seq_bound.sby. Testbench component, NEVER synthesis or
// the Verilator ctests.
//
// THE LAW UNDER PROOF is stated twice in the design already — once in
// design/contracts/FIELD.SEQ.CORE.md and once beside the port itself:
//
//   "`instr_count_i` is a LIVENESS bound, not a semantic check. The decoder
//    guarantees exactly one OP_END and that it is last, so a lawful program
//    never reaches this limit. But the instruction MEMORY is the shell's to
//    load, and a walk with no bound turns a mis-loaded memory into a machine
//    that hangs forever instead of one that reports a status. A hang is the
//    worse failure, and it is the one nobody can debug from a frame capture."
//
// The differential cannot prove that. It runs programs it thought of, and a
// hang is what happens on the program nobody thought of — a memory whose
// contents the decoder never blessed, because the shell mis-loaded it. So the
// interesting question is not "does a lawful program finish" but "can ANY
// instruction memory, however malformed, get this block stuck".
//
// EVERY INSTRUCTION WORD IS A FREE FORMAL INPUT. `ins_op_i` is unconstrained,
// so the solver may hand the sequencer opcodes that do not exist, operands
// that point anywhere, and an immediate of any value, changing them every
// cycle. The table lanes are free too. That is the adversarial model the
// property needs: a mis-loaded memory is exactly an arbitrary one.
//
// TWO PROPERTIES, and the second is the one with teeth:
//
//   a_pc_bounded    the walk never steps past the bound it was given
//   a_progress      the sequencer is never stuck: from any busy state it
//                   reaches Q_DONE within the bound
//
// The cover task is not optional. Both are implications over a busy machine,
// and a model that never starts satisfies them while proving nothing — which
// is the shape MEM.GUARD's formal lane once failed in, recorded in this
// project's own notes. The covers prove the machine both RUNS and OVERRUNS.
`default_nettype none

module zhao_field_seq_bound_harness
  import zhao_field_seq_pkg::MAX_OP_CYCLES;
(
    input logic clk,
    input logic rst_n,

    input logic        rf_we_i,
    input logic [ 5:0] rf_waddr_i,
    input logic signed [31:0] rf_wdata_i,
    input logic [ 5:0] rf_raddr_i,

    input logic clear_i,
    input logic start_i,

    // The bound itself is free, so the proof is not about one convenient
    // value. It is constrained ONLY to the small range a bounded model can
    // walk to the end of; see the assume below, which says why.
    input logic [7:0] instr_count_i,

    // the instruction memory: ARBITRARY, every cycle
    input logic [ 7:0] ins_op_i,
    input logic [ 5:0] ins_dst_i,
    input logic [ 5:0] ins_a_i,
    input logic [ 5:0] ins_b_i,
    input logic [ 5:0] ins_c_i,
    input logic [31:0] ins_imm_i,

    // the table memory: arbitrary too
    input logic [ 6:0] tbl_n_i,
    input logic signed [31:0] tbl_x_i,
    input logic signed [31:0] tbl_y_i,
    input logic signed [31:0] tbl_dy_i
);

  logic        busy, done;
  logic [ 7:0] status, pc;
  logic signed [31:0] rf_rdata;
  logic [31:0] tbl_sel;
  logic [ 5:0] tbl_idx;
  logic        sat_add, sat_mul, sat_rescale, sat_rcp, rcp0, retired;

  zhao_field_seq u_seq (
      .clk(clk),
      .rst_n(rst_n),
      .rf_we_i(rf_we_i),
      .rf_waddr_i(rf_waddr_i),
      .rf_wdata_i(rf_wdata_i),
      .rf_raddr_i(rf_raddr_i),
      .rf_rdata_o(rf_rdata),
      .clear_i(clear_i),
      .start_i(start_i),
      .busy_o(busy),
      .done_o(done),
      .status_o(status),
      .instr_count_i(instr_count_i),
      .pc_o(pc),
      .ins_op_i(ins_op_i),
      .ins_dst_i(ins_dst_i),
      .ins_a_i(ins_a_i),
      .ins_b_i(ins_b_i),
      .ins_c_i(ins_c_i),
      .ins_imm_i(ins_imm_i),
      .tbl_sel_o(tbl_sel),
      .tbl_idx_o(tbl_idx),
      .tbl_n_i(tbl_n_i),
      .tbl_x_i(tbl_x_i),
      .tbl_y_i(tbl_y_i),
      .tbl_dy_i(tbl_dy_i),
      .sat_add_o(sat_add),
      .sat_mul_o(sat_mul),
      .sat_rescale_o(sat_rescale),
      .sat_rcp_o(sat_rcp),
      .rcp0_o(rcp0),
      .instr_retired_o(retired)
  );

`ifdef FORMAL
  logic f_past_valid = 1'b0;
  always_ff @(posedge clk) f_past_valid <= 1'b1;
  always_ff @(posedge clk) begin
    if (f_past_valid && $past(rst_n)) assume (rst_n);
  end

  // THE RUN MUST START FROM RESET, and leaving this out cost two iterations.
  //
  // `zhao_field_seq` declares `logic [3:0] state;` with no initialiser -- which
  // is correct RTL, because the reset assigns it. But a bounded model with a
  // free `rst_n` may simply never assert reset, and then the solver picks the
  // initial state itself: it started the machine mid-execute with a pc already
  // past the bound and reported a_pc_bounded reachable at k = 2.
  //
  // That counterexample was TRUE of the model and said nothing about the
  // design. A machine that was never reset has no obligations.
  // `initial assume (!rst_n)` is rejected -- "reading net state during design
  // initialization unsupported" -- so the first cycle is constrained through
  // the same f_past_valid the other harnesses already use.
  always_ff @(posedge clk) if (!f_past_valid) assume (!rst_n);

  // THE BOUND IS SMALL, AND THAT IS A TRACTABILITY CHOICE, NOT A WEAKENING.
  //
  // The property is about the walk reaching its end. A LEN instruction alone
  // costs 34 clocks in the integer root, so a bound of 200 would need a BMC
  // depth in the thousands and the proof would never finish. With the count
  // held to 0..2 the machine can still be handed arbitrary opcodes, including
  // the slow multi-cycle ones, and the end of the walk is reachable inside the
  // depth.
  //
  // What this does NOT weaken: the bound is still FREE within that range, the
  // instruction words are still arbitrary, and the overrun path is exercised
  // (count 0 overruns immediately). What it does not cover is a LONG program,
  // and that is a depth limit, said out loud rather than papered over.
  always_ff @(posedge clk) assume (instr_count_i <= 8'd2);

  // The host owns the register file only outside a run (the contract's own
  // words), so writing it mid-run is not a case the block promises anything
  // about.
  always_ff @(posedge clk) if (busy) assume (!rf_we_i && !clear_i && !start_i);

  // AND THE BOUND ITSELF IS STABLE DURING A RUN.
  //
  // This assumption was missing on the first attempt and BMC found the hole
  // immediately: property a_pc_bounded was reachable at k = 2. The
  // counterexample is not a design defect -- it is the solver doing exactly
  // what it should with an under-constrained environment. `instr_count_i` was
  // free EVERY cycle, so it simply lowered the bound out from under an
  // advancing pc.
  //
  // The real contract is that the shell loads the instruction memory and its
  // count before `start` and holds both for the run; the sequencer reads that
  // memory as it walks. A count that moves mid-walk is a shell that rewrote
  // the program under a running engine, which nothing promises anything about.
  //
  // Recorded rather than quietly added, because "the property failed so I
  // assumed the failure away" is the single easiest way to turn a formal lane
  // into decoration. The distinction: this assumption is a statement the
  // CONTRACT already makes, not one invented to make red go green.
  always_ff @(posedge clk) begin
    if (f_past_valid && busy) assume (instr_count_i == $past(instr_count_i));
  end

  // HOW LONG THE MACHINE HAS BEEN BUSY WITHOUT FINISHING.
  //
  // The natural way to write "once busy, done arrives" is
  // `busy |-> ##[1:120] done`, and the frontend refuses it:
  //
  //   error: encountered unsupported SVA feature
  //
  // So the same law is written as a counter and a bound, which is a SAFETY
  // property rather than a bounded-liveness one and is exactly as strong for
  // this purpose: if the sequencer can hang, this counter runs away, and the
  // solver only has to find the input sequence that makes it.
  logic [15:0] busy_cnt;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) busy_cnt <= 16'd0;
    else if (!busy) busy_cnt <= 16'd0;
    else busy_cnt <= busy_cnt + 16'd1;
  end

  // AND HOW LONG THE CURRENT INSTRUCTION HAS BEEN RUNNING, which is the bound
  // the DESIGN states. `MAX_OP_CYCLES` is `zhao_field_seq_pkg`'s, imported
  // above rather than copied here: there is one definition of this number in
  // the tree and the proof reads it.
  //
  // This counter restarts at every retirement and at every start, so it
  // measures exactly what the design's cost table measures -- fetch to retire.
  logic [15:0] op_cnt;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) op_cnt <= 16'd0;
    else if (!busy || retired) op_cnt <= 16'd0;
    else op_cnt <= op_cnt + 16'd1;
  end

  // THE RUN-LEVEL BOUND IS DERIVED, NOT CHOSEN. The count shrink below holds
  // `instr_count_i <= 2`, so a run walks at most two instructions; add the
  // fetch that discovers the overrun and the Q_DONE that ends it, with six
  // clocks of slack for the walk states either side.
  localparam int unsigned MAX_FORMAL_INSTRS = 2;
  localparam int unsigned MAX_RUN_CYCLES = MAX_FORMAL_INSTRS * MAX_OP_CYCLES + 8;

  always_ff @(posedge clk) begin
    if (f_past_valid && rst_n) begin
      // ---- the walk never steps past the bound it was given ---------------
      // If this fails, the "bound" is not one and the anti-hang argument in
      // the contract is decoration.
      //
      // GATED ON `busy`, and the ungated version was wrong rather than
      // stronger. `pc` is zeroed by `start_i` and then KEPT after the run
      // ends, so an idle sequencer holds the pc of the program it just
      // finished. `instr_count_i` is only assumed stable while busy -- the
      // shell is free to load a different program between runs -- so an
      // ungated assertion compares last run's pc against next run's count and
      // fails at k = 10 on a machine that is behaving perfectly.
      //
      // The law is about THE WALK. Outside one there is no walk to bound.
      if (busy) a_pc_bounded: assert (pc <= instr_count_i);

      // ---- THE ANTI-HANG LAW ITSELF ---------------------------------------
      // TWO ASSERTIONS NOW, AND THE FIRST IS THE ONE WITH MEANING.
      //
      // `a_op_bounded` is the design's own claim: NO INSTRUCTION, whatever
      // opcode the solver invents and whatever the operands point at, runs
      // longer than `MAX_OP_CYCLES`. That is the number
      // `fpga/rtl/field/zhao_field_seq.sv` states in its cost table and that
      // section 12 of the differential measures; here it is proven for the
      // instruction memory nobody thought of.
      //
      // It matters more since the DSP rearchitecture than it did before. Every
      // op now waits on shared resources -- one multiplier, one integer root,
      // one reciprocal -- and every wait is an `if (valid)` that a mis-timed
      // schedule turns into a spin. The old design's units could not wait on
      // each other at all.
      //
      // `a_progress` is then the run-level consequence, DERIVED above rather
      // than picked: two instructions at the per-instruction bound, plus the
      // fetch that overruns and the state that finishes. It used to be a bare
      // 120 with a paragraph explaining why 120 was generous, which is what a
      // magic constant looks like when it is behaving.
      a_op_bounded: assert (op_cnt <= 16'(MAX_OP_CYCLES));
      a_progress: assert (busy_cnt <= 16'(MAX_RUN_CYCLES));
    end
  end

  // ---- V19 scope guard (the a_horizon_is_refresh_free pattern) ------------
  // These assertions are proven at depth 140 UNDER THE COUNT SHRINK above
  // (instr_count_i <= 2). That shrink is what makes 140 enough: two
  // instructions, the slowest spending 34 clocks in the integer root, plus the
  // walk and the handshake states. Raising `depth` does NOT extend the proof to
  // longer programs -- the count bound and the depth have to be re-derived
  // together -- so this guard PINS the proven window and FIRES the moment the
  // depth goes past it, forcing that re-derivation instead of a silent
  // re-scope of what "PASS" means here.
  //
  // THE WINDOW IS DERIVED TOO. `MAX_RUN_CYCLES` plus the reset cycles and the
  // idle cycle before `start_i`, rounded up to the depth the .sby asks for. If
  // `MAX_OP_CYCLES` grows, this grows with it and the .sby's `depth` must be
  // raised to match -- which is the forced re-derivation, rather than a silent
  // re-scope of what PASS means here.
  localparam int unsigned SCOPE_STEPS = MAX_RUN_CYCLES + 22;
  logic [15:0] f_steps = 16'd0;
  always_ff @(posedge clk) begin
    if (f_steps != 16'hFFFF) f_steps <= f_steps + 16'd1;
  end
  always_comb begin
    a_scope_short_program_window : assert (f_steps <= 16'(SCOPE_STEPS));
  end

  // ---- non-vacuity (V16): the antecedents are REACHABLE -------------------
  // Both assertions above are about a running machine. A model that never
  // starts satisfies them and proves nothing -- the shape MEM.GUARD's lane
  // failed in. These demand that it RUNS, FINISHES, OVERRUNS, and REFUSES.
  always_ff @(posedge clk) begin
    if (f_past_valid && rst_n) begin
      c_runs:        cover (busy);
      c_done_ok:     cover (done && (status == 8'd0));
      c_overrun:     cover (done && (status == 8'd2));
      c_unsupported: cover (done && (status == 8'd1));
    end
  end
`endif

endmodule

`default_nettype wire
