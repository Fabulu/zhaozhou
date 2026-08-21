// zhao_field_seq.sv — the Field IR sequencer: the register file and the
// instruction walk that turns a decoded program into a run.
//
// A submodule of the FIELD.SEQ.* family — in fact it is the *whole* of it. The
// five ledger blocks FIELD.SEQ.EARTH / WARP / FLOW / FORMATION / STAMP are this
// sequencer wearing five different profiles; the profile decides which ops a
// program may contain, and the decoder is what enforces that.
//
// Reference: `zfield::interpret` (reference/src/zfield/zfield_interpret.cpp).
//
// ---------------------------------------------------------------------------
// THE LAW
// ---------------------------------------------------------------------------
//     reg[0..63] = 0
//     reg[in_lanes[i].reg] = in[i]        for each declared input lane
//     pc = 0; walk instructions until OP_END
//     out[i] = reg[out_lanes[i].reg]      for each declared output lane
//
// ---------------------------------------------------------------------------
// THIS BLOCK DOES NOT VALIDATE, AND THAT IS THE DESIGN
// ---------------------------------------------------------------------------
// `interpret` runs only on DECODED programs, and its `default:` case is
// `__builtin_unreachable()`. The decoder is the validator, and it is thorough:
//
//   * every source register and every destination lane is in range — `dst + k`
//     and `src + k` are checked against REG_COUNT, so NOTHING WRAPS here;
//   * a register is never read before it is written;
//   * a destination never overlaps an input lane, and never overlaps its own
//     sources — so an op may write `dst` while reading `a`/`b`/`c` with no
//     hazard, which is why the write-back below needs no bypass network;
//   * there is exactly one OP_END and it is the last instruction;
//   * unused operand fields are zero, and every immediate is in range for its
//     op class.
//
// So this block assumes all of it. That is not laziness: re-checking here would
// be a SECOND implementation of the rules, and two implementations of a rule is
// how they drift apart. The one that runs on untrusted bytes is the decoder.
//
// ---------------------------------------------------------------------------
// THREE READ PORTS, WALKED
// ---------------------------------------------------------------------------
// The widest op reads seven registers: `a`, `a+1`, `a+2`, `b`, `b+1`, `b+2` and
// `c`. Seven combinational read ports on a 64-entry file is seven 64:1 muxes of
// 32 bits — several thousand ALMs to save four cycles, in a design that has
// already failed a fit once.
//
// So there are THREE ports and the operand groups are walked:
//
//     cycle 1   a+0   b+0   c
//     cycle 2   a+1   b+1
//     cycle 3   a+2   b+2
//
// The register file itself is flops rather than M10K, because a 64x32 file with
// three read ports and one write port does not map onto a block RAM without
// duplication, and 2,048 flops is the cheaper answer at this size.
//
// ---------------------------------------------------------------------------
// DISPATCH: THE ARITHMETIC CORE, PLUS THE COMBINATIONAL UNITS
// ---------------------------------------------------------------------------
// `zhao_field_alu` (fifteen opcodes) plus `zhao_field_rcp` and
// `zhao_field_sin`, which are COMBINATIONAL blocks whose own headers say the
// sequencer owns their pipeline. Here that means there is no pipeline: they
// sit beside the ALU in Q_EXEC, cost no extra state and no extra cycle, and
// are selected by opcode.
//
// That is the whole reason they are wired first. The remaining blocks —
// length, normalise, table, noise, rotation, ring — are ready/valid and
// multi-cycle, and several of them write two or three CONSECUTIVE registers
// through a file with one write port. They need states this walk does not
// have yet, so they are a separate increment rather than a bigger version of
// this one.
//
// RCP BRINGS TWO LEDGER LANES THE WALK DID NOT HAVE. `sat_rcp` is a genuine
// saturation. `rcp0` is NOT — it records that a reciprocal was asked for
// zero, which has a defined answer, and the reference keeps it in its own
// field precisely so that a defined answer does not read as an overflow.
// They are carried out of here separately for the same reason.
//
// An op outside the core is REFUSED: `status_o` reports it and the run stops.
// It does not return zero and it does not skip the instruction, because a
// sequencer that quietly ignores an opcode produces a plausible field and a
// wrong world.
module zhao_field_seq (
    input logic clk,
    input logic rst_n,

    // ---- host access to the register file (before start, after done) -------
    input  logic        rf_we_i,
    input  logic [ 5:0] rf_waddr_i,
    input  logic signed [31:0] rf_wdata_i,
    input  logic [ 5:0] rf_raddr_i,
    output logic signed [31:0] rf_rdata_o,

    // ---- run control --------------------------------------------------------
    input  logic clear_i,   // zero the whole file (the law's `reg[..] = {0}`)
    input  logic start_i,
    output logic busy_o,
    output logic done_o,
    output logic [7:0] status_o,   // 0 = ran to END; else why it stopped

    // ---- instruction memory: registered read, per the M10K rules ------------
    // `instr_count_i` is a LIVENESS bound, not a semantic check. The decoder
    // guarantees exactly one OP_END and that it is last, so a lawful program
    // never reaches this limit. But the instruction MEMORY is the shell's to
    // load, and a walk with no bound turns a mis-loaded memory into a machine
    // that hangs forever instead of one that reports a status. A hang is the
    // worse failure, and it is the one nobody can debug from a frame capture.
    input  logic [ 7:0] instr_count_i,
    output logic [ 7:0] pc_o,
    input  logic [ 7:0] ins_op_i,
    input  logic [ 5:0] ins_dst_i,
    input  logic [ 5:0] ins_a_i,
    input  logic [ 5:0] ins_b_i,
    input  logic [ 5:0] ins_c_i,
    input  logic [31:0] ins_imm_i,

    // ---- the SatLedger, accumulated across the WHOLE program ---------------
    output logic sat_add_o,
    output logic sat_mul_o,
    output logic sat_rescale_o,
    output logic sat_rcp_o,        // SatLedger::rcp — a real saturation
    output logic rcp0_o,           // SatLedger::rcp0 — NOT one; see the header
    output logic instr_retired_o   // one pulse per executed instruction
);

  localparam logic [7:0] ST_OK = 8'd0;
  localparam logic [7:0] ST_UNSUPPORTED_OP = 8'd1;
  localparam logic [7:0] ST_PC_OVERRUN = 8'd2;

  localparam logic [2:0] Q_IDLE = 3'd0;
  localparam logic [2:0] Q_FETCH = 3'd1;   // pc presented; the word lands next
  localparam logic [2:0] Q_LATCH = 3'd2;   // the instruction word is here
  localparam logic [2:0] Q_RD0 = 3'd3;     // a+0, b+0, c
  localparam logic [2:0] Q_RD1 = 3'd4;     // a+1, b+1
  localparam logic [2:0] Q_RD2 = 3'd5;     // a+2, b+2
  localparam logic [2:0] Q_EXEC = 3'd6;
  localparam logic [2:0] Q_DONE = 3'd7;

  logic [2:0] state;
  logic [7:0] pc;

  // The register file. Flops, not M10K -- see the header.
  logic signed [31:0] rf [0:63];

  // Latched instruction fields, so the walk does not depend on the instruction
  // memory holding its answer.
  logic [ 7:0] i_op;
  logic [ 5:0] i_dst, i_a, i_b, i_c;
  logic [31:0] i_imm;

  logic signed [31:0] a0, a1, a2, b0, b1, b2, cv;

  // ---- the three read ports ----------------------------------------------
  // The file is flops, so a read is COMBINATIONAL: the address driven in a
  // state is answered in that same state, and each group is captured on its own
  // edge. The decoder has already proved `a + 2` and `b + 2` are in range for
  // every op that reads them, so these adds cannot wrap into another register.
  logic [5:0] ra, rb, rc;
  always_comb begin
    case (state)
      Q_RD0: begin
        ra = i_a;
        rb = i_b;
        rc = i_c;
      end
      Q_RD1: begin
        ra = i_a + 6'd1;
        rb = i_b + 6'd1;
        rc = 6'd0;
      end
      default: begin
        ra = i_a + 6'd2;
        rb = i_b + 6'd2;
        rc = 6'd0;
      end
    endcase
  end

  logic signed [31:0] rd_a, rd_b, rd_c;
  assign rd_a = rf[ra];
  assign rd_b = rf[rb];
  assign rd_c = rf[rc];
  assign rf_rdata_o = rf[rf_raddr_i];

  // ---- the arithmetic core ------------------------------------------------
  logic signed [31:0] alu_result;
  logic               alu_is_end, alu_writes, alu_unsupported;
  logic               alu_sat_add, alu_sat_mul, alu_sat_rescale;

  zhao_field_alu u_alu (
      .op_i             (i_op),
      .imm_i            (i_imm),
      .a0_i             (a0),
      .a1_i             (a1),
      .a2_i             (a2),
      .b0_i             (b0),
      .b1_i             (b1),
      .b2_i             (b2),
      .c_i              (cv),
      .result_o         (alu_result),
      .is_end_o         (alu_is_end),
      .writes_o         (alu_writes),
      .op_unsupported_o (alu_unsupported),
      .sat_add_o        (alu_sat_add),
      .sat_mul_o        (alu_sat_mul),
      .sat_rescale_o    (alu_sat_rescale)
  );

  // ---- the combinational op units -----------------------------------------
  // Both blocks are pure combinational logic whose headers say "the sequencer
  // owns the pipeline". They are fed the SAME latched operands the ALU sees and
  // are selected in Q_EXEC, so RCP, SIN and COS cost exactly what an ADD costs.
  localparam logic [7:0] OP_RCP = 8'h17;
  localparam logic [7:0] OP_SIN = 8'h18;
  localparam logic [7:0] OP_COS = 8'h19;

  logic signed [31:0] rcp_result;
  logic               rcp_sat, rcp_zero;

  zhao_field_rcp u_rcp (
      .a_i       (a0),
      .result_o  (rcp_result),
      .sat_rcp_o (rcp_sat),
      .rcp0_o    (rcp_zero)
  );

  // Law: `fx_sin(angle16{(uint16_t)reg[a]})`. The angle is the LOW HALF of the
  // register and the upper half is IGNORED, not rejected — the same defined
  // answer the software gives for a caller that leaves rubbish up there.
  /* verilator lint_off UNUSEDSIGNAL */
  logic signed [31:0] sin_result;
  /* verilator lint_on UNUSEDSIGNAL */

  zhao_field_sin u_sin (
      .angle_i  (a0[15:0]),
      .is_cos_i (i_op == OP_COS),
      .result_o (sin_result)
  );

  // ---- opcode selection ---------------------------------------------------
  // The ALU reports these three as unsupported, because to the ALU they ARE.
  // The selection below is the only place that knows otherwise, so an opcode
  // that no unit claims still reaches the refusal path untouched.
  logic op_is_rcp, op_is_sin_cos, unit_handled;
  assign op_is_rcp     = (i_op == OP_RCP);
  assign op_is_sin_cos = (i_op == OP_SIN) || (i_op == OP_COS);
  assign unit_handled  = op_is_rcp || op_is_sin_cos;

  logic signed [31:0] exec_result;
  logic               exec_writes, exec_unsupported;
  logic               exec_sat_add, exec_sat_mul, exec_sat_rescale;
  logic               exec_sat_rcp, exec_rcp0;

  always_comb begin
    if (op_is_rcp) begin
      exec_result      = rcp_result;
      exec_sat_rcp     = rcp_sat;
      exec_rcp0        = rcp_zero;
    end else if (op_is_sin_cos) begin
      exec_result      = sin_result;
      exec_sat_rcp     = 1'b0;
      exec_rcp0        = 1'b0;
    end else begin
      exec_result      = alu_result;
      exec_sat_rcp     = 1'b0;
      exec_rcp0        = 1'b0;
    end
    // ASSUMPTION, and the assumption is about ANOTHER BLOCK. The three units
    // report saturation only in the rcp lane, so the add/mul/rescale lanes
    // belong to the ALU alone — and `zhao_field_alu`'s `default:` case already
    // leaves those three at their block-initialised zero for an opcode it does
    // not claim. That makes this mask PROVABLY REDUNDANT today, and the
    // mutation sweep records all three removals as surviving equivalents
    // rather than pretending they are covered.
    //
    // It stays because the redundancy is a fact about the ALU's default case,
    // not about this block, and depending on another module's unstated
    // behaviour is how the `abs` defect survived weeks of green tests.
    // ENFORCED-BY: tests/differential/field_seq_directed.cpp
    exec_sat_add     = unit_handled ? 1'b0 : alu_sat_add;
    exec_sat_mul     = unit_handled ? 1'b0 : alu_sat_mul;
    exec_sat_rescale = unit_handled ? 1'b0 : alu_sat_rescale;
    exec_writes      = unit_handled ? 1'b1 : alu_writes;
    exec_unsupported = unit_handled ? 1'b0 : alu_unsupported;
  end

  assign busy_o = (state != Q_IDLE) && (state != Q_DONE);
  assign pc_o = pc;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state <= Q_IDLE;
      pc <= 8'd0;
      i_op <= 8'd0;
      i_dst <= 6'd0; i_a <= 6'd0; i_b <= 6'd0; i_c <= 6'd0;
      i_imm <= 32'd0;
      a0 <= '0; a1 <= '0; a2 <= '0; b0 <= '0; b1 <= '0; b2 <= '0; cv <= '0;
      done_o <= 1'b0;
      status_o <= ST_OK;
      sat_add_o <= 1'b0;
      sat_mul_o <= 1'b0;
      sat_rescale_o <= 1'b0;
      sat_rcp_o <= 1'b0;
      rcp0_o <= 1'b0;
      instr_retired_o <= 1'b0;
      for (int i = 0; i < 64; i++) rf[i] <= '0;
    end else begin
      done_o <= 1'b0;
      instr_retired_o <= 1'b0;

      // ---- the register file has exactly ONE writer at a time ------------
      // The law's `int32_t reg[REG_COUNT] = {0}` is `clear_i`, kept separate
      // from `start_i` so the host can zero the file, load its input lanes and
      // only then run -- which is the order the reference does it in. Host
      // writes are refused while the walk owns the file: there is no
      // arbitration to get wrong because there is no concurrency to allow.
      if (clear_i) begin
        for (int i = 0; i < 64; i++) rf[i] <= '0;
      end else if (busy_o) begin
        // The walk's write-back. The decoder has proved `dst` never overlaps
        // this instruction's own sources, which is why there is no bypass
        // network here and does not need to be one.
        if ((state == Q_EXEC) && exec_writes && !alu_is_end && !exec_unsupported) begin
          rf[i_dst] <= exec_result;
        end
      end else if (rf_we_i) begin
        rf[rf_waddr_i] <= rf_wdata_i;
      end

      case (state)
        Q_IDLE: begin
          if (start_i) begin
            pc <= 8'd0;
            status_o <= ST_OK;
            sat_add_o <= 1'b0;
            sat_mul_o <= 1'b0;
            sat_rescale_o <= 1'b0;
            sat_rcp_o <= 1'b0;
            rcp0_o <= 1'b0;
            state <= Q_FETCH;
          end
        end

        // The instruction word is a registered read: the index is presented
        // during this state and answered in the next.
        Q_FETCH: begin
          if (pc >= instr_count_i) begin
            // The liveness bound. A lawful program never gets here.
            status_o <= ST_PC_OVERRUN;
            state <= Q_DONE;
          end else begin
            state <= Q_LATCH;
          end
        end

        Q_LATCH: begin
          i_op <= ins_op_i;
          i_dst <= ins_dst_i;
          i_a <= ins_a_i;
          i_b <= ins_b_i;
          i_c <= ins_c_i;
          i_imm <= ins_imm_i;
          state <= Q_RD0;
        end

        // The operand groups, walked. Reads are combinational, so each state
        // drives its addresses and captures its answers on the same edge.
        Q_RD0: begin
          a0 <= rd_a;
          b0 <= rd_b;
          cv <= rd_c;
          state <= Q_RD1;
        end

        Q_RD1: begin
          a1 <= rd_a;
          b1 <= rd_b;
          state <= Q_RD2;
        end

        Q_RD2: begin
          a2 <= rd_a;
          b2 <= rd_b;
          state <= Q_EXEC;
        end

        Q_EXEC: begin
          if (exec_unsupported) begin
            // REFUSED, not skipped and not zero. A sequencer that quietly
            // ignores an opcode produces a plausible field and a wrong world.
            status_o <= ST_UNSUPPORTED_OP;
            state <= Q_DONE;
          end else if (alu_is_end) begin
            status_o <= ST_OK;
            state <= Q_DONE;
          end else begin
            // The ledger accumulates across the WHOLE program, exactly as the
            // reference's single SatLedger does -- not per instruction.
            sat_add_o <= sat_add_o || exec_sat_add;
            sat_mul_o <= sat_mul_o || exec_sat_mul;
            sat_rescale_o <= sat_rescale_o || exec_sat_rescale;
            sat_rcp_o <= sat_rcp_o || exec_sat_rcp;
            rcp0_o <= rcp0_o || exec_rcp0;
            instr_retired_o <= 1'b1;
            pc <= pc + 8'd1;
            state <= Q_FETCH;
          end
        end

        Q_DONE: begin
          done_o <= 1'b1;
          state <= Q_IDLE;
        end

        default: state <= Q_IDLE;
      endcase
    end
  end

endmodule : zhao_field_seq
