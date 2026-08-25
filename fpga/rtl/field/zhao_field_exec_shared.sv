// zhao_field_exec_shared.sv — the Field IR engine's arithmetic, all of it, once.
//
// A submodule of the FIELD.SEQ.* family. It holds no law of its own: every
// operation's semantics still live in the op controller that owns them, and
// through them in `zfield::interpret`. What lives HERE is the silicon those
// controllers take turns on.
//
// ---------------------------------------------------------------------------
// WHY THIS BLOCK EXISTS — MEASURED, 2026-08-23
// ---------------------------------------------------------------------------
// The first synthesis this project ever ran on the Field engine measured
// `zhao_field_seq` plus its fourteen dependencies at 10,623 ALMs and **79 DSPs**
// against a device with 112. Seventy-one per cent of the chip, for one
// subsystem, because the sequencer instantiated the ALU, reciprocal, sine,
// integer root, length, normalise, curve, noise, ring and rotation units SIDE
// BY SIDE — each owning its own physical multiplier — while retiring ONE
// INSTRUCTION AT A TIME on a six-clock walk. Nine of ten units were idle at
// every instant, holding multipliers.
//
// The owner's ruling of 2026-08-23 states the rule this block implements:
//
//   > Give each major subsystem the smallest local multiplier farm its
//   > SUSTAINED RATE actually needs, and share only operations that are
//   > MUTUALLY EXCLUSIVE inside that subsystem. DSP allocation is justified by
//   > sustained frame demand, not by preserving one-clock placeholder
//   > throughputs.
//
// Inside the Field engine every operation is mutually exclusive with every
// other one, so the smallest farm is ONE of everything:
//
//   * one `zhao_field_mul`      — the signed 33x33 lane, registered both sides
//   * one `zhao_field_isqrt`    — LEN and NORMALIZE take turns
//   * one `zhao_field_sin`      — OP_SIN, OP_COS and ROT's two reads
//   * one `zhao_field_rcp`      — OP_RCP and RING's two smoothstep spans,
//                                 carrying the one ordinary seed ROM
//   * one `zhao_field_rcp24_rom`— inside NORMALIZE, the OTHER seed table
//
// No production op unit keeps a private nonconstant multiplier. The only `*`
// on a nonconstant pair in the whole Field cone is the one inside
// `zhao_field_mul`; CURVE's multiplies by three and five stay where they are,
// because a multiply by three is an add and a shift.
//
// ---------------------------------------------------------------------------
// THE FREE ARITHMETIC SLOTS, which are why simple ops still cost six clocks
// ---------------------------------------------------------------------------
// The sequencer's walk was FETCH, LATCH, three register reads, EXEC. The reads
// were three cycles in which nothing was computed. They are now three issue
// slots on the shared lane:
//
//   Q_LATCH  issue reg[a+0] x reg[b+0]        product lands in Q_RD2
//   Q_RD1    issue reg[a+1] x reg[b+1]        product lands in Q_GATH
//   Q_RD2    issue reg[a+2] x reg[b+2]        product lands in Q_EXEC
//   Q_GATH   (the accumulator closes)
//   Q_EXEC   prod_ab, dot2 and dot3 are all standing ready
//
// So MUL, MAD, DOT2 and DOT3 are still SIX-CLOCK instructions on a machine with
// one multiplier, and DOT3 — three products — costs exactly what MOV costs.
// That is the whole reason the lane is two cycles deep rather than one or
// three: one would put a 33x33 multiply, a 66-bit accumulate and a saturating
// rescale in a single combinational path, and three would cost DOT3 a seventh
// clock because the last pair is not read until Q_RD2.
//
// THE ACCUMULATOR IS LOADED BY THE FIRST PRODUCT AND ADDED TO BY THE OTHER TWO.
// Loaded, not accumulated into whatever was there: a shared accumulator that
// carried a previous instruction's partial sum is precisely the defect a
// parallel design could not have had and a shared one can, and it is invisible
// to any test that runs one op at a time.
// ENFORCED-BY: tests/differential/field_seq_directed.cpp (section 9)
//
// ---------------------------------------------------------------------------
// THERE IS NO ARBITER, AND THAT IS THE SAFETY ARGUMENT
// ---------------------------------------------------------------------------
// Ten controllers can drive the lane and none of them can collide, because
// `zhao_field_seq` has exactly one instruction in flight: an op is handed over
// in Q_MISS and drained in Q_MWAIT before the next fetch, and the read slots
// finish issuing in Q_RD2 — two cycles before the earliest a multi-cycle unit
// can be accepted. So the mux below selects on the EXECUTING OPCODE and needs
// no grant, no round robin and no backpressure.
//
// Everything rests on that being true, so it is written as a mux that CANNOT
// pass an unselected unit's request rather than as a comment, and it is tested
// as a fact: each operation is run alone, then in hostile sequences, and every
// answer and every saturation lane must equal its isolated result.
//
// The one nested case is RING, which calls the shared reciprocal, which itself
// uses the lane. While RING waits on `zhao_field_rcp` it issues nothing, so
// there is still exactly one requester at every instant; the priority order
// below is therefore documentation of that fact rather than a tie-break.
module zhao_field_exec_shared (
    input logic clk,
    input logic rst_n,

    // ---- the decoded instruction, as the sequencer latched it -------------
    input logic [ 7:0] op_i,
    input logic [31:0] imm_i,

    // ---- the free arithmetic slots in the register-read walk --------------
    // `slot_a_i`/`slot_b_i` are the register file's answers for group
    // `slot_idx_i`, driven combinationally by the sequencer in the same cycle
    // it captures them.
    input logic        slot_issue_i,
    input logic [ 1:0] slot_idx_i,
    input logic signed [31:0] slot_a_i,
    input logic signed [31:0] slot_b_i,

    // ---- the operands, as the walk captured them --------------------------
    input logic signed [31:0] a0_i,
    input logic signed [31:0] a1_i,
    input logic signed [31:0] a2_i,
    input logic signed [31:0] b0_i,
    input logic signed [31:0] b1_i,
    input logic signed [31:0] b2_i,
    input logic signed [31:0] c_i,

    // ---- the single-cycle path, consumed in Q_EXEC ------------------------
    output logic signed [31:0] exec_result_o,
    output logic               exec_is_end_o,
    output logic               exec_writes_o,
    output logic               exec_unsupported_o,
    output logic               exec_sat_add_o,
    output logic               exec_sat_mul_o,
    output logic               exec_sat_rescale_o,

    // ---- the multi-cycle path ---------------------------------------------
    output logic               multi_op_o,
    input  logic               v_valid_i,   // Q_MISS
    output logic               v_ready_o,
    input  logic               r_ready_i,   // Q_MWAIT
    output logic               r_valid_o,
    output logic signed [31:0] o0_o,
    output logic signed [31:0] o1_o,
    output logic signed [31:0] o2_o,
    output logic        [ 1:0] width_o,     // output lanes: 1, 2 or 3
    output logic               sat_add_o,
    output logic               sat_mul_o,
    output logic               sat_rescale_o,
    output logic               sat_rcp_o,
    output logic               rcp0_o,

    // ---- the table memory, CURVE's second memory --------------------------
    output logic [ 5:0] tbl_idx_o,
    input  logic [ 6:0] tbl_n_i,
    input  logic signed [31:0] tbl_x_i,
    input  logic signed [31:0] tbl_y_i,
    input  logic signed [31:0] tbl_dy_i
);

  // ---- the opcodes this block routes ---------------------------------------
  localparam logic [7:0] OP_LEN2 = 8'h12;
  localparam logic [7:0] OP_LEN3 = 8'h13;
  localparam logic [7:0] OP_DIST2 = 8'h14;
  localparam logic [7:0] OP_NORMALIZE2 = 8'h15;
  localparam logic [7:0] OP_NORMALIZE3 = 8'h16;
  localparam logic [7:0] OP_RCP = 8'h17;
  localparam logic [7:0] OP_SIN = 8'h18;
  localparam logic [7:0] OP_COS = 8'h19;
  localparam logic [7:0] OP_CURVE = 8'h1A;
  localparam logic [7:0] OP_SPLINE = 8'h1B;
  localparam logic [7:0] OP_NOISE2 = 8'h1C;
  localparam logic [7:0] OP_DCURVE = 8'h1D;
  localparam logic [7:0] OP_RING = 8'h21;
  localparam logic [7:0] OP_RIDGE = 8'h22;
  localparam logic [7:0] OP_ROT2 = 8'h28;
  localparam logic [7:0] OP_ROT3 = 8'h29;

  logic op_is_len, op_is_norm, op_is_rcp, op_is_sin_cos;
  logic op_is_curve, op_is_noise, op_is_rot, op_is_ring;

  assign op_is_len     = (op_i == OP_LEN2) || (op_i == OP_LEN3) || (op_i == OP_DIST2);
  assign op_is_norm    = (op_i == OP_NORMALIZE2) || (op_i == OP_NORMALIZE3);
  assign op_is_rcp     = (op_i == OP_RCP);
  assign op_is_sin_cos = (op_i == OP_SIN) || (op_i == OP_COS);
  assign op_is_curve   = (op_i == OP_CURVE) || (op_i == OP_DCURVE) || (op_i == OP_SPLINE);
  assign op_is_noise   = (op_i == OP_NOISE2) || (op_i == OP_RIDGE);
  assign op_is_rot     = (op_i == OP_ROT2) || (op_i == OP_ROT3);
  assign op_is_ring    = (op_i == OP_RING);

  // ==========================================================================
  // THE ONE MULTIPLIER
  // ==========================================================================
  logic               mul_issue;
  logic signed [32:0] mul_a, mul_b;
  logic signed [65:0] mul_p;
  logic               mul_valid;

  zhao_field_mul u_mul (
      .clk      (clk),
      .rst_n    (rst_n),
      .issue_i  (mul_issue),
      .a_i      (mul_a),
      .b_i      (mul_b),
      .p_o      (mul_p),
      .p_valid_o(mul_valid)
  );

  // ---- the read-walk accumulator -------------------------------------------
  // A two-stage shadow of the issue pipeline, so this block knows WHICH slot's
  // product is on `mul_p` without knowing anything about the sequencer's state
  // encoding. `sp1_v` is `mul_valid` restricted to products the read walk
  // asked for, which is what keeps a unit's product out of the accumulator.
  logic       sp0_v, sp1_v;
  logic [1:0] sp0_idx, sp1_idx;

  logic signed [65:0] p_ab, acc;
  logic signed [65:0] dot3_w;
  assign dot3_w = acc + mul_p;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      sp0_v <= 1'b0;
      sp1_v <= 1'b0;
      sp0_idx <= 2'd0;
      sp1_idx <= 2'd0;
      p_ab <= '0;
      acc <= '0;
    end else begin
      sp0_v   <= slot_issue_i;
      sp0_idx <= slot_idx_i;
      sp1_v   <= sp0_v;
      sp1_idx <= sp0_idx;

      if (sp1_v) begin
        if (sp1_idx == 2'd0) begin
          // LOADED, not accumulated. See the header.
          p_ab <= mul_p;
          acc  <= mul_p;
        end else begin
          acc <= acc + mul_p;
        end
      end
    end
  end

  // ==========================================================================
  // THE ONE INTEGER SQUARE ROOT — LEN and NORMALIZE take turns
  // ==========================================================================
  logic        sqrt_valid, sqrt_ready, sqrt_rvalid, sqrt_rready;
  logic [63:0] sqrt_n, sqrt_r;

  zhao_field_isqrt u_isqrt (
      .clk      (clk),
      .rst_n    (rst_n),
      .n_valid_i(sqrt_valid),
      .n_ready_o(sqrt_ready),
      .n_i      (sqrt_n),
      .r_valid_o(sqrt_rvalid),
      .r_ready_i(sqrt_rready),
      .r_o      (sqrt_r)
  );

  // ==========================================================================
  // THE ONE SINE TABLE — OP_SIN, OP_COS and ROT's two reads
  // ==========================================================================
  logic        [15:0] sin_angle;
  logic               sin_is_cos;
  logic signed [31:0] sin_result;

  // WAVE 8: the table answers one clock after the angle is presented. OP_SIN
  // and OP_COS pay nothing for it -- `a0` latches at the Q_RD1 -> Q_RD2 edge
  // and Q_GATH is the ONLY entry to Q_EXEC, three states later, so the request
  // has been standing for three cycles by the time the result is read. ROT
  // pays one wait state per table read; see zhao_field_rot.sv.
  zhao_field_sin u_sin (
      .clk     (clk),
      .angle_i (sin_angle),
      .is_cos_i(sin_is_cos),
      .result_o(sin_result)
  );

  // ---- the op units ---------------------------------------------------------
  logic               len_vready, len_rvalid;
  logic signed [31:0] len_result;
  logic               len_sat_add, len_sat_rescale;
  logic               len_mul_issue;
  logic signed [32:0] len_mul_a, len_mul_b;
  logic               len_sq_valid, len_sq_rready;
  logic        [63:0] len_sq_n;

  logic [1:0] len_mode;
  always_comb begin
    case (op_i)
      OP_LEN3:  len_mode = 2'd1;
      OP_DIST2: len_mode = 2'd2;
      default:  len_mode = 2'd0;   // OP_LEN2
    endcase
  end

  zhao_field_len u_len (
      .clk           (clk),
      .rst_n         (rst_n),
      .v_valid_i     (v_valid_i && op_is_len),
      .v_ready_o     (len_vready),
      .mode_i        (len_mode),
      .a0_i          (a0_i),
      .a1_i          (a1_i),
      .a2_i          (a2_i),
      .b0_i          (b0_i),
      .b1_i          (b1_i),
      .r_valid_o     (len_rvalid),
      .r_ready_i     (r_ready_i && op_is_len),
      .result_o      (len_result),
      .sat_add_o     (len_sat_add),
      .sat_rescale_o (len_sat_rescale),
      .mul_issue_o   (len_mul_issue),
      .mul_a_o       (len_mul_a),
      .mul_b_o       (len_mul_b),
      .mul_p_i       (mul_p),
      .mul_valid_i   (mul_valid),
      .sqrt_valid_o  (len_sq_valid),
      .sqrt_ready_i  (sqrt_ready && op_is_len),
      .sqrt_n_o      (len_sq_n),
      .sqrt_rvalid_i (sqrt_rvalid && op_is_len),
      .sqrt_rready_o (len_sq_rready),
      .sqrt_r_i      (sqrt_r)
  );

  logic               nrm_vready, nrm_rvalid;
  logic signed [31:0] nrm_o0, nrm_o1, nrm_o2;
  logic               nrm_rcp0, nrm_sat_rescale;
  logic               nrm_mul_issue;
  logic signed [32:0] nrm_mul_a, nrm_mul_b;
  logic               nrm_sq_valid, nrm_sq_rready;
  logic        [63:0] nrm_sq_n;

  zhao_field_normalize u_norm (
      .clk           (clk),
      .rst_n         (rst_n),
      .v_valid_i     (v_valid_i && op_is_norm),
      .v_ready_o     (nrm_vready),
      .is3_i         (op_i == OP_NORMALIZE3),
      .a0_i          (a0_i),
      .a1_i          (a1_i),
      .a2_i          (a2_i),
      .r_valid_o     (nrm_rvalid),
      .r_ready_i     (r_ready_i && op_is_norm),
      .o0_o          (nrm_o0),
      .o1_o          (nrm_o1),
      .o2_o          (nrm_o2),
      .rcp0_o        (nrm_rcp0),
      .sat_rescale_o (nrm_sat_rescale),
      .mul_issue_o   (nrm_mul_issue),
      .mul_a_o       (nrm_mul_a),
      .mul_b_o       (nrm_mul_b),
      .mul_p_i       (mul_p),
      .mul_valid_i   (mul_valid),
      .sqrt_valid_o  (nrm_sq_valid),
      .sqrt_ready_i  (sqrt_ready && op_is_norm),
      .sqrt_n_o      (nrm_sq_n),
      .sqrt_rvalid_i (sqrt_rvalid && op_is_norm),
      .sqrt_rready_o (nrm_sq_rready),
      .sqrt_r_i      (sqrt_r)
  );

  // The root's mux. Only one of the two can be running, so this selects on the
  // opcode rather than arbitrating.
  assign sqrt_valid  = op_is_len ? len_sq_valid  : (op_is_norm && nrm_sq_valid);
  assign sqrt_n      = op_is_len ? len_sq_n      : nrm_sq_n;
  assign sqrt_rready = op_is_len ? len_sq_rready : (op_is_norm && nrm_sq_rready);

  // ---- the reciprocal, shared by OP_RCP and RING ----------------------------
  logic               rcp_valid, rcp_ready, rcp_rvalid, rcp_rready;
  logic signed [31:0] rcp_a, rcp_result;
  logic               rcp_sat, rcp_zero;
  logic               rcp_mul_issue;
  logic signed [32:0] rcp_mul_a, rcp_mul_b;

  zhao_field_rcp u_rcp (
      .clk        (clk),
      .rst_n      (rst_n),
      .v_valid_i  (rcp_valid),
      .v_ready_o  (rcp_ready),
      .a_i        (rcp_a),
      .r_valid_o  (rcp_rvalid),
      .r_ready_i  (rcp_rready),
      .result_o   (rcp_result),
      .sat_rcp_o  (rcp_sat),
      .rcp0_o     (rcp_zero),
      .mul_issue_o(rcp_mul_issue),
      .mul_a_o    (rcp_mul_a),
      .mul_b_o    (rcp_mul_b),
      .mul_p_i    (mul_p),
      .mul_valid_i(mul_valid)
  );

  logic               rg_vready, rg_rvalid;
  logic signed [31:0] rg_result;
  logic               rg_sat_add, rg_sat_mul, rg_sat_rescale, rg_sat_rcp, rg_rcp0;
  logic               rg_mul_issue;
  logic signed [32:0] rg_mul_a, rg_mul_b;
  logic               rg_rcp_valid, rg_rcp_rready;
  logic signed [31:0] rg_rcp_a;

  zhao_field_ring u_ring (
      .clk           (clk),
      .rst_n         (rst_n),
      .v_valid_i     (v_valid_i && op_is_ring),
      .v_ready_o     (rg_vready),
      .d_i           (a0_i),
      .r0_i          (b0_i),
      .r1_i          (c_i),
      .r_valid_o     (rg_rvalid),
      .r_ready_i     (r_ready_i && op_is_ring),
      .result_o      (rg_result),
      .sat_add_o     (rg_sat_add),
      .sat_mul_o     (rg_sat_mul),
      .sat_rescale_o (rg_sat_rescale),
      .sat_rcp_o     (rg_sat_rcp),
      .rcp0_o        (rg_rcp0),
      .rcp_valid_o   (rg_rcp_valid),
      .rcp_ready_i   (rcp_ready && op_is_ring),
      .rcp_a_o       (rg_rcp_a),
      .rcp_rvalid_i  (rcp_rvalid && op_is_ring),
      .rcp_rready_o  (rg_rcp_rready),
      .rcp_result_i  (rcp_result),
      .rcp_sat_i     (rcp_sat),
      .rcp_zero_i    (rcp_zero),
      .mul_issue_o   (rg_mul_issue),
      .mul_a_o       (rg_mul_a),
      .mul_b_o       (rg_mul_b),
      .mul_p_i       (mul_p),
      .mul_valid_i   (mul_valid)
  );

  // The reciprocal's mux. OP_RCP drives it straight from the sequencer's own
  // handshake; RING drives it from inside its walk. Nothing else asks.
  assign rcp_valid  = op_is_ring ? rg_rcp_valid  : (v_valid_i && op_is_rcp);
  assign rcp_a      = op_is_ring ? rg_rcp_a      : a0_i;
  assign rcp_rready = op_is_ring ? rg_rcp_rready : (r_ready_i && op_is_rcp);

  logic               nz_vready, nz_rvalid;
  logic signed [31:0] nz_o0, nz_o1;
  logic               nz_sat_add, nz_sat_rescale;
  logic               nz_mul_issue;
  logic signed [32:0] nz_mul_a, nz_mul_b;

  zhao_field_noise u_noise (
      .clk           (clk),
      .rst_n         (rst_n),
      .v_valid_i     (v_valid_i && op_is_noise),
      .v_ready_o     (nz_vready),
      .is_ridge_i    (op_i == OP_RIDGE),
      .a0_i          (a0_i),
      // RIDGE takes its second lane from reg[b], NOT reg[a+1]. The interpreter
      // is explicit and this is the one mapping that would silently produce a
      // plausible field if it were assumed instead of read.
      .a1_i          ((op_i == OP_RIDGE) ? b0_i : a1_i),
      .seed_i        (imm_i),
      .r_valid_o     (nz_rvalid),
      .r_ready_i     (r_ready_i && op_is_noise),
      .o0_o          (nz_o0),
      .o1_o          (nz_o1),
      .sat_add_o     (nz_sat_add),
      .sat_rescale_o (nz_sat_rescale),
      .mul_issue_o   (nz_mul_issue),
      .mul_a_o       (nz_mul_a),
      .mul_b_o       (nz_mul_b),
      .mul_p_i       (mul_p),
      .mul_valid_i   (mul_valid)
  );

  logic               rt_vready, rt_rvalid;
  logic signed [31:0] rt_o0, rt_o1, rt_o2;
  logic               rt_sat_add, rt_sat_mul;
  logic               rt_mul_issue;
  logic signed [32:0] rt_mul_a, rt_mul_b;
  logic        [15:0] rt_sin_angle;
  logic               rt_sin_is_cos;

  zhao_field_rot u_rot (
      .clk          (clk),
      .rst_n        (rst_n),
      .v_valid_i    (v_valid_i && op_is_rot),
      .v_ready_o    (rt_vready),
      .is_rot3_i    (op_i == OP_ROT3),
      .axis_i       (imm_i[1:0]),
      .ang_i        (b0_i),
      .a0_i         (a0_i),
      .a1_i         (a1_i),
      .a2_i         (a2_i),
      .r_valid_o    (rt_rvalid),
      .r_ready_i    (r_ready_i && op_is_rot),
      .o0_o         (rt_o0),
      .o1_o         (rt_o1),
      .o2_o         (rt_o2),
      .sat_add_o    (rt_sat_add),
      .sat_mul_o    (rt_sat_mul),
      .sin_angle_o  (rt_sin_angle),
      .sin_is_cos_o (rt_sin_is_cos),
      .sin_result_i (sin_result),
      .mul_issue_o  (rt_mul_issue),
      .mul_a_o      (rt_mul_a),
      .mul_b_o      (rt_mul_b),
      .mul_p_i      (mul_p),
      .mul_valid_i  (mul_valid)
  );

  // The sine table's mux. OP_SIN and OP_COS read `reg[a]`'s LOW HALF — the
  // upper half is IGNORED, not rejected, which is the defined answer the
  // software gives a caller that leaves rubbish up there.
  assign sin_angle  = op_is_rot ? rt_sin_angle : a0_i[15:0];
  assign sin_is_cos = op_is_rot ? rt_sin_is_cos : (op_i == OP_COS);

  logic               cv_vready, cv_rvalid;
  logic signed [31:0] cv_result;
  logic        [ 5:0] cv_seg;
  logic               cv_sat_add, cv_sat_mul, cv_sat_rescale;
  logic               cv_mul_issue;
  logic signed [32:0] cv_mul_a, cv_mul_b;

  logic [1:0] curve_mode;
  always_comb begin
    case (op_i)
      OP_DCURVE: curve_mode = 2'd1;
      OP_SPLINE: curve_mode = 2'd2;
      default:   curve_mode = 2'd0;   // OP_CURVE
    endcase
  end

  /* verilator lint_off UNUSEDSIGNAL */
  logic [5:0] cv_seg_unused;
  /* verilator lint_on UNUSEDSIGNAL */
  assign cv_seg_unused = cv_seg;

  zhao_field_curve u_curve (
      .clk           (clk),
      .rst_n         (rst_n),
      .v_valid_i     (v_valid_i && op_is_curve),
      .v_ready_o     (cv_vready),
      .mode_i        (curve_mode),
      .a_i           (a0_i),
      .tbl_n_i       (tbl_n_i),
      .tbl_idx_o     (tbl_idx_o),
      .tbl_x_i       (tbl_x_i),
      .tbl_y_i       (tbl_y_i),
      .tbl_dy_i      (tbl_dy_i),
      .r_valid_o     (cv_rvalid),
      .r_ready_i     (r_ready_i && op_is_curve),
      .result_o      (cv_result),
      .seg_idx_o     (cv_seg),
      .sat_add_o     (cv_sat_add),
      .sat_mul_o     (cv_sat_mul),
      .sat_rescale_o (cv_sat_rescale),
      .mul_issue_o   (cv_mul_issue),
      .mul_a_o       (cv_mul_a),
      .mul_b_o       (cv_mul_b),
      .mul_p_i       (mul_p),
      .mul_valid_i   (mul_valid)
  );

  // ==========================================================================
  // THE LANE MUX
  // ==========================================================================
  // Read from the top: the read walk owns the lane while it is walking, then
  // the shared reciprocal (which only ever runs inside OP_RCP or RING), then
  // the executing opcode's own controller. An UNSELECTED unit's request cannot
  // reach the lane through this mux even if it were to assert one.
  logic               unit_issue;
  logic signed [32:0] unit_a, unit_b;

  always_comb begin
    unit_issue = 1'b0;
    unit_a     = '0;
    unit_b     = '0;
    if (op_is_len) begin
      unit_issue = len_mul_issue;
      unit_a     = len_mul_a;
      unit_b     = len_mul_b;
    end else if (op_is_norm) begin
      unit_issue = nrm_mul_issue;
      unit_a     = nrm_mul_a;
      unit_b     = nrm_mul_b;
    end else if (op_is_noise) begin
      unit_issue = nz_mul_issue;
      unit_a     = nz_mul_a;
      unit_b     = nz_mul_b;
    end else if (op_is_rot) begin
      unit_issue = rt_mul_issue;
      unit_a     = rt_mul_a;
      unit_b     = rt_mul_b;
    end else if (op_is_ring) begin
      unit_issue = rg_mul_issue;
      unit_a     = rg_mul_a;
      unit_b     = rg_mul_b;
    end else if (op_is_curve) begin
      unit_issue = cv_mul_issue;
      unit_a     = cv_mul_a;
      unit_b     = cv_mul_b;
    end
  end

  always_comb begin
    if (slot_issue_i) begin
      mul_issue = 1'b1;
      // The register file's answers are s32 and SIGN-extend into the lane.
      mul_a     = $signed({slot_a_i[31], slot_a_i});
      mul_b     = $signed({slot_b_i[31], slot_b_i});
    end else if (rcp_mul_issue) begin
      mul_issue = 1'b1;
      mul_a     = rcp_mul_a;
      mul_b     = rcp_mul_b;
    end else begin
      mul_issue = unit_issue;
      mul_a     = unit_a;
      mul_b     = unit_b;
    end
  end

  // ==========================================================================
  // THE ALU — everything that finishes in Q_EXEC
  // ==========================================================================
  logic signed [31:0] alu_result;
  logic               alu_is_end, alu_writes, alu_unsupported;
  logic               alu_sat_add, alu_sat_mul, alu_sat_rescale;

  zhao_field_alu u_alu (
      .op_i             (op_i),
      .imm_i            (imm_i),
      .a0_i             (a0_i),
      .a1_i             (a1_i),
      .a2_i             (a2_i),
      .b0_i             (b0_i),
      .b1_i             (b1_i),
      .b2_i             (b2_i),
      .c_i              (c_i),
      .prod_ab_i        (p_ab),
      .dot2_i           (acc),
      .dot3_i           (dot3_w),
      .result_o         (alu_result),
      .is_end_o         (alu_is_end),
      .writes_o         (alu_writes),
      .op_unsupported_o (alu_unsupported),
      .sat_add_o        (alu_sat_add),
      .sat_mul_o        (alu_sat_mul),
      .sat_rescale_o    (alu_sat_rescale)
  );

  // OP_SIN and OP_COS are the only ops that still finish in Q_EXEC without the
  // ALU: the sine table is combinational, so they cost exactly what an ADD
  // costs. OP_RCP no longer joins them — its two products walk the lane.
  always_comb begin
    if (op_is_sin_cos) begin
      exec_result_o      = sin_result;
      exec_sat_add_o     = 1'b0;
      exec_sat_mul_o     = 1'b0;
      exec_sat_rescale_o = 1'b0;
      exec_writes_o      = 1'b1;
    end else begin
      exec_result_o      = alu_result;
      exec_sat_add_o     = alu_sat_add;
      exec_sat_mul_o     = alu_sat_mul;
      exec_sat_rescale_o = alu_sat_rescale;
      exec_writes_o      = alu_writes;
    end
  end

  assign exec_is_end_o = alu_is_end;

  // An op that NO unit claims is refused: `status_o` reports it and the run
  // stops. It does not return zero and it does not skip the instruction,
  // because a sequencer that quietly ignores an opcode produces a plausible
  // field and a wrong world. The ALU reports SIN, COS and every multi-cycle op
  // as unsupported because to the ALU they are; this is the only place that
  // knows otherwise.
  assign multi_op_o = op_is_len || op_is_norm || op_is_rcp || op_is_noise ||
                      op_is_rot || op_is_ring || op_is_curve;
  assign exec_unsupported_o = (op_is_sin_cos || multi_op_o) ? 1'b0 : alu_unsupported;

  // ==========================================================================
  // THE RESULT MUX for the multi-cycle path
  // ==========================================================================
  always_comb begin
    if (op_is_curve) begin
      v_ready_o     = cv_vready;
      r_valid_o     = cv_rvalid;
      o0_o          = cv_result;
      o1_o          = '0;
      o2_o          = '0;
      width_o       = 2'd1;        // CURVE, DCURVE and SPLINE all write one
      sat_add_o     = cv_sat_add;
      sat_mul_o     = cv_sat_mul;
      sat_rescale_o = cv_sat_rescale;
      sat_rcp_o     = 1'b0;
      rcp0_o        = 1'b0;
    end else if (op_is_noise) begin
      v_ready_o     = nz_vready;
      r_valid_o     = nz_rvalid;
      o0_o          = nz_o0;
      o1_o          = nz_o1;
      o2_o          = '0;
      // dstW from field-ir 2: NOISE2 writes two lanes, RIDGE one.
      width_o       = (op_i == OP_NOISE2) ? 2'd2 : 2'd1;
      sat_add_o     = nz_sat_add;
      sat_mul_o     = 1'b0;
      sat_rescale_o = nz_sat_rescale;
      sat_rcp_o     = 1'b0;
      rcp0_o        = 1'b0;
    end else if (op_is_rot) begin
      v_ready_o     = rt_vready;
      r_valid_o     = rt_rvalid;
      o0_o          = rt_o0;
      o1_o          = rt_o1;
      o2_o          = rt_o2;
      // ROT2 writes TWO lanes. The block drives o2 to zero for ROT2 (its law
      // 5), but writing it would still clobber a register the decoder counts
      // as untouched -- the width, not the value, is what protects it.
      width_o       = (op_i == OP_ROT3) ? 2'd3 : 2'd2;
      sat_add_o     = rt_sat_add;
      sat_mul_o     = rt_sat_mul;
      sat_rescale_o = 1'b0;
      sat_rcp_o     = 1'b0;
      rcp0_o        = 1'b0;
    end else if (op_is_ring) begin
      v_ready_o     = rg_vready;
      r_valid_o     = rg_rvalid;
      o0_o          = rg_result;
      o1_o          = '0;
      o2_o          = '0;
      width_o       = 2'd1;
      sat_add_o     = rg_sat_add;
      sat_mul_o     = rg_sat_mul;
      sat_rescale_o = rg_sat_rescale;
      sat_rcp_o     = rg_sat_rcp;
      rcp0_o        = rg_rcp0;
    end else if (op_is_norm) begin
      v_ready_o     = nrm_vready;
      r_valid_o     = nrm_rvalid;
      o0_o          = nrm_o0;
      o1_o          = nrm_o1;
      o2_o          = nrm_o2;
      // dstW from field-ir 2, the same table the decoder enforces: 2 lanes for
      // NORMALIZE2, 3 for NORMALIZE3. Writing a lane the decoder considers
      // untouched would clobber a live register.
      width_o       = (op_i == OP_NORMALIZE3) ? 2'd3 : 2'd2;
      sat_add_o     = 1'b0;
      sat_mul_o     = 1'b0;
      sat_rescale_o = nrm_sat_rescale;
      sat_rcp_o     = 1'b0;
      // NORMALIZE2 alone can report it -- law 3 of the block, not a quirk.
      rcp0_o        = nrm_rcp0;
    end else if (op_is_rcp) begin
      v_ready_o     = rcp_ready;
      r_valid_o     = rcp_rvalid;
      o0_o          = rcp_result;
      o1_o          = '0;
      o2_o          = '0;
      width_o       = 2'd1;
      sat_add_o     = 1'b0;
      sat_mul_o     = 1'b0;
      sat_rescale_o = 1'b0;
      // `sat_rcp` is a genuine saturation. `rcp0` is NOT -- it records that a
      // reciprocal was asked for zero, which has a defined answer, and the
      // reference keeps it in its own field precisely so that a defined answer
      // does not read as an overflow.
      sat_rcp_o     = rcp_sat;
      rcp0_o        = rcp_zero;
    end else begin
      v_ready_o     = len_vready;
      r_valid_o     = len_rvalid;
      o0_o          = len_result;
      o1_o          = '0;
      o2_o          = '0;
      width_o       = 2'd1;          // every LEN op writes one lane
      sat_add_o     = len_sat_add;
      sat_mul_o     = 1'b0;
      sat_rescale_o = len_sat_rescale;
      sat_rcp_o     = 1'b0;
      rcp0_o        = 1'b0;
    end
  end

endmodule : zhao_field_exec_shared
