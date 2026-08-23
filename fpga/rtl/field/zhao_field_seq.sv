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
// THREE READ PORTS, WALKED — AND THE WALK IS NOW ALSO THE MULTIPLIER SCHEDULE
// ---------------------------------------------------------------------------
// The widest op reads seven registers: `a`, `a+1`, `a+2`, `b`, `b+1`, `b+2` and
// `c`. Seven combinational read ports on a 64-entry file is seven 64:1 muxes of
// 32 bits — several thousand ALMs to save four cycles, in a design that has
// already failed a fit once.
//
// So there are THREE ports and the operand groups are walked. What changed on
// 2026-08-23 is WHERE the walk starts: the first group is read in Q_LATCH, from
// the instruction memory's own combinational outputs, rather than a cycle later
// from the latched fields.
//
//     Q_LATCH   ins_a+0   ins_b+0   ins_c      -> issue reg[a] x reg[b]
//     Q_RD1     i_a+1     i_b+1                -> issue reg[a+1] x reg[b+1]
//     Q_RD2     i_a+2     i_b+2                -> issue reg[a+2] x reg[b+2]
//     Q_GATH    (the shared accumulator closes)
//     Q_EXEC    prod_ab, dot2 and dot3 are standing ready
//
// THAT ONE CYCLE IS THE WHOLE TRICK. The engine now has ONE multiplier
// (`zhao_field_mul`, in `zhao_field_exec_shared`), input- and output-registered,
// so a product lands two cycles after it is issued. Starting the issue stream in
// Q_LATCH instead of Q_RD0 puts the LAST of the three products in Q_EXEC — the
// state that consumes it — so MUL, MAD, DOT2 and DOT3 still retire in SIX
// CLOCKS on a machine with a single multiplier. Reading a group earlier costs no
// extra port: it is the same three ports, addressed from `ins_*_i` for one cycle
// instead of from `i_*`.
//
// The register file itself is flops rather than M10K, because a 64x32 file with
// three read ports and one write port does not map onto a block RAM without
// duplication, and 2,048 flops is the cheaper answer at this size.
//
// ---------------------------------------------------------------------------
// DISPATCH: ONE ARITHMETIC ENGINE, NOT TEN IDLE CALCULATORS
// ---------------------------------------------------------------------------
// This block used to instantiate the ALU, the reciprocal, the sine table, the
// integer root, length, normalise, curve, noise, ring and rotation units SIDE BY
// SIDE. Measured at 10,623 ALMs and 79 DSPs of a 112-DSP device — for a
// sequencer that retires one instruction at a time, so nine of the ten were idle
// at every instant while holding multipliers.
//
// They now live in `zhao_field_exec_shared`, which owns ONE of everything and
// muxes on the executing opcode. What stayed here is what this block is FOR: the
// register file, the address walk, the state machine, the write-back and the
// ledger. The seam is deliberately narrow — the operands go out, a result and a
// width come back — so that the arithmetic can be rescheduled again without
// touching the walk.
//
// An op outside the engine is REFUSED: `status_o` reports it and the run stops.
// It does not return zero and it does not skip the instruction, because a
// sequencer that quietly ignores an opcode produces a plausible field and a
// wrong world.
//
// ---------------------------------------------------------------------------
// THE COST TABLE, and MAX_OP_CYCLES
// ---------------------------------------------------------------------------
// Sharing one lane lengthens the ops that used to hold their own. The bound
// below is DERIVED from the longest of them rather than chosen, because the
// anti-hang proof needs a number it can defend:
//
//   MOV / ADD / MUL / MAD / DOT2 / DOT3 / SIN / COS ......  6
//   RCP ................................................. ~13
//   LEN2 / LEN3 / DIST2 ................................. ~48
//   CURVE / DCURVE ...................................... ~30
//   SPLINE .............................................. ~50
//   NOISE2 / RIDGE ...................................... ~30
//   ROT2 / ROT3 ......................................... ~24
//   RING ................................................ ~64
//   NORMALIZE2 / NORMALIZE3 ............................. ~76
//
// `MAX_OP_CYCLES` is the ceiling on ONE instruction, measured from the cycle it
// is fetched to the cycle it retires, and `tests/formal/field_seq_bound.sby`
// proves it for an ARBITRARY instruction memory rather than for the programs
// anyone thought of. It is exported as a localparam so the harness derives its
// window from the design instead of restating a magic constant.
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

    // ---- table memory: the same shape as the instruction memory ------------
    // CURVE, DCURVE and SPLINE read a table chosen by the instruction's
    // immediate, exactly as `zfield::interpret` does with `prog.tables[imm]`.
    // The sequencer does not hold the tables any more than it holds the
    // program: it names one and reads it, and the shell owns the memory.
    //
    // A REGISTERED READ, per the M10K rules: `tbl_idx_o` is presented for a
    // whole cycle and the three lanes answer on the NEXT one. That is the
    // curve block's contract with its table, carried straight through.
    output logic [31:0] tbl_sel_o,      // which table: the instruction's imm
    output logic [ 5:0] tbl_idx_o,      // which entry inside it
    input  logic [ 6:0] tbl_n_i,        // entry count of the selected table
    input  logic signed [31:0] tbl_x_i,
    input  logic signed [31:0] tbl_y_i,
    input  logic signed [31:0] tbl_dy_i,

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

  // ---- THE ANTI-HANG BOUND, DERIVED ---------------------------------------
  // The ceiling on ONE instruction, from the cycle it is fetched to the cycle
  // it retires. NORMALIZE3 is the longest: six walk states, the handshake, the
  // three squares, the 34-clock root, four dependent reciprocal-correction
  // products at three clocks each, three pipelined lane products, and two
  // write-back lanes. Measured at 76 in simulation
  // (tests/differential/field_seq_directed.cpp reports the worst it saw); the
  // margin here is deliberate and small enough that a regression trips it.
  //
  // NOT A MAGIC NUMBER, and not one the formal harness restates: it is exported
  // so `tests/formal/field_seq_bound.sby` derives its own window from it.
  // Read by tests/formal/field_seq_bound_harness.sv, which is the whole point
  // of exporting it, and by nothing inside this module -- so the linter is
  // right that the DESIGN does not consume it, and wrong that it is dead.
  /* verilator lint_off UNUSEDPARAM */
  localparam int unsigned MAX_OP_CYCLES = 96;
  /* verilator lint_on UNUSEDPARAM */

  // FOUR BITS. The walk needs a gather state on top of the read states, and the
  // multi-cycle ops need an issue state, a wait state and a write-back walk,
  // because the register file has ONE write port and NORMALIZE3 and ROT3 each
  // produce three lanes.
  localparam logic [3:0] Q_IDLE = 4'd0;
  localparam logic [3:0] Q_FETCH = 4'd1;   // pc presented; the word lands next
  localparam logic [3:0] Q_LATCH = 4'd2;   // the word is here: read a+0, b+0, c
  localparam logic [3:0] Q_RD1 = 4'd3;     // a+1, b+1
  localparam logic [3:0] Q_RD2 = 4'd4;     // a+2, b+2
  localparam logic [3:0] Q_GATH = 4'd5;    // the shared accumulator closes
  localparam logic [3:0] Q_EXEC = 4'd6;
  localparam logic [3:0] Q_DONE = 4'd7;
  // ---- the multi-cycle path ----
  localparam logic [3:0] Q_MISS = 4'd8;    // hold v_valid until the unit takes it
  localparam logic [3:0] Q_MWAIT = 4'd9;   // hold r_ready until the unit answers
  localparam logic [3:0] Q_WB1 = 4'd10;    // second output lane, dst+1
  localparam logic [3:0] Q_WB2 = 4'd11;    // third output lane, dst+2

  logic [3:0] state;
  logic [7:0] pc;

  // The register file. Flops, not M10K -- see the header.
  logic signed [31:0] rf [0:63];

  // Latched instruction fields, so the walk does not depend on the instruction
  // memory holding its answer.
  logic [ 7:0] i_op;
  // `i_c` is gone: reg[c] is now read in Q_LATCH from `ins_c_i`, on the same
  // cycle the word arrives, so the latched copy has no reader.
  logic [ 5:0] i_dst, i_a, i_b;
  logic [31:0] i_imm;

  logic signed [31:0] a0, a1, a2, b0, b1, b2, cv;

  // Output lanes 1 and 2 of a multi-cycle op, latched at the accepting edge so
  // the write-back walk does not depend on the unit holding its outputs.
  logic signed [31:0] m_o1, m_o2;

  // ---- the three read ports ----------------------------------------------
  // The file is flops, so a read is COMBINATIONAL: the address driven in a
  // state is answered in that same state, and each group is captured on its own
  // edge. The decoder has already proved `a + 2` and `b + 2` are in range for
  // every op that reads them, so these adds cannot wrap into another register.
  //
  // Q_LATCH addresses from `ins_*_i` because `i_a`/`i_b`/`i_c` are only latched
  // at the END of that state. That is what buys the arithmetic slot; see the
  // header.
  logic [5:0] ra, rb, rc;
  always_comb begin
    case (state)
      Q_LATCH: begin
        ra = ins_a_i;
        rb = ins_b_i;
        rc = ins_c_i;
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

  // ---- the free arithmetic slots ------------------------------------------
  // The three read states hand their pairs straight to the shared lane. Which
  // pair belongs to which slot is decided HERE, by the state, and nothing
  // downstream needs to know the state encoding.
  logic       slot_issue;
  logic [1:0] slot_idx;
  always_comb begin
    slot_issue = 1'b0;
    slot_idx   = 2'd0;
    case (state)
      Q_LATCH: begin slot_issue = 1'b1; slot_idx = 2'd0; end
      Q_RD1:   begin slot_issue = 1'b1; slot_idx = 2'd1; end
      Q_RD2:   begin slot_issue = 1'b1; slot_idx = 2'd2; end
      default: begin slot_issue = 1'b0; slot_idx = 2'd0; end
    endcase
  end

  // ---- the arithmetic, all of it ------------------------------------------
  logic signed [31:0] exec_result;
  logic               exec_is_end, exec_writes, exec_unsupported;
  logic               exec_sat_add, exec_sat_mul, exec_sat_rescale;

  logic               multi_op, multi_vready, multi_rvalid;
  logic signed [31:0] multi_o0, multi_o1, multi_o2;
  logic [1:0]         multi_width;
  logic               multi_sat_add, multi_sat_mul, multi_sat_rescale;
  logic               multi_sat_rcp, multi_rcp0;

  // The table selector is the immediate, held for the whole instruction, so
  // the shell sees a stable table while the unit walks its entries.
  assign tbl_sel_o = i_imm;

  zhao_field_exec_shared u_exec (
      .clk                (clk),
      .rst_n              (rst_n),
      .op_i               (i_op),
      .imm_i              (i_imm),
      .slot_issue_i       (slot_issue),
      .slot_idx_i         (slot_idx),
      .slot_a_i           (rd_a),
      .slot_b_i           (rd_b),
      .a0_i               (a0),
      .a1_i               (a1),
      .a2_i               (a2),
      .b0_i               (b0),
      .b1_i               (b1),
      .b2_i               (b2),
      .c_i                (cv),
      .exec_result_o      (exec_result),
      .exec_is_end_o      (exec_is_end),
      .exec_writes_o      (exec_writes),
      .exec_unsupported_o (exec_unsupported),
      .exec_sat_add_o     (exec_sat_add),
      .exec_sat_mul_o     (exec_sat_mul),
      .exec_sat_rescale_o (exec_sat_rescale),
      .multi_op_o         (multi_op),
      .v_valid_i          (state == Q_MISS),
      .v_ready_o          (multi_vready),
      .r_ready_i          (state == Q_MWAIT),
      .r_valid_o          (multi_rvalid),
      .o0_o               (multi_o0),
      .o1_o               (multi_o1),
      .o2_o               (multi_o2),
      .width_o            (multi_width),
      .sat_add_o          (multi_sat_add),
      .sat_mul_o          (multi_sat_mul),
      .sat_rescale_o      (multi_sat_rescale),
      .sat_rcp_o          (multi_sat_rcp),
      .rcp0_o             (multi_rcp0),
      .tbl_idx_o          (tbl_idx_o),
      .tbl_n_i            (tbl_n_i),
      .tbl_x_i            (tbl_x_i),
      .tbl_y_i            (tbl_y_i),
      .tbl_dy_i           (tbl_dy_i)
  );

  assign busy_o = (state != Q_IDLE) && (state != Q_DONE);
  assign pc_o = pc;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state <= Q_IDLE;
      pc <= 8'd0;
      i_op <= 8'd0;
      i_dst <= 6'd0; i_a <= 6'd0; i_b <= 6'd0;
      i_imm <= 32'd0;
      a0 <= '0; a1 <= '0; a2 <= '0; b0 <= '0; b1 <= '0; b2 <= '0; cv <= '0;
      m_o1 <= '0; m_o2 <= '0;
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
        if ((state == Q_EXEC) && exec_writes && !exec_is_end && !exec_unsupported
             && !multi_op) begin
          rf[i_dst] <= exec_result;
        end else if ((state == Q_MWAIT) && multi_rvalid) begin
          rf[i_dst] <= multi_o0;          // lane 0, on the accepting edge
        end else if (state == Q_WB1) begin
          rf[i_dst + 6'd1] <= m_o1;       // lane 1
        end else if (state == Q_WB2) begin
          rf[i_dst + 6'd2] <= m_o2;       // lane 2
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

        // The word is here. It is latched AND its first operand group is read
        // and multiplied in the same cycle -- see the header.
        Q_LATCH: begin
          i_op <= ins_op_i;
          i_dst <= ins_dst_i;
          i_a <= ins_a_i;
          i_b <= ins_b_i;
          i_imm <= ins_imm_i;
          a0 <= rd_a;
          b0 <= rd_b;
          cv <= rd_c;
          state <= Q_RD1;
        end

        // The remaining operand groups, walked. Reads are combinational, so
        // each state drives its addresses and captures its answers on the same
        // edge.
        Q_RD1: begin
          a1 <= rd_a;
          b1 <= rd_b;
          state <= Q_RD2;
        end

        Q_RD2: begin
          a2 <= rd_a;
          b2 <= rd_b;
          state <= Q_GATH;
        end

        // ONE CYCLE OF NOTHING, AND IT IS NOT SLACK. The shared lane answers
        // two cycles after an issue, so this is where the second product lands
        // and the accumulator closes over it. Removing this state does not save
        // a clock; it costs DOT3 its third term.
        Q_GATH: state <= Q_EXEC;

        Q_EXEC: begin
          if (exec_unsupported) begin
            // REFUSED, not skipped and not zero. A sequencer that quietly
            // ignores an opcode produces a plausible field and a wrong world.
            status_o <= ST_UNSUPPORTED_OP;
            state <= Q_DONE;
          end else if (exec_is_end) begin
            status_o <= ST_OK;
            state <= Q_DONE;
          end else if (multi_op) begin
            // The slow path. Nothing is written and no counter moves here --
            // the instruction has not executed yet, it has only been handed
            // over. Retirement happens once, at the end of the write-back.
            state <= Q_MISS;
          end else begin
            // The ledger accumulates across the WHOLE program, exactly as the
            // reference's single SatLedger does -- not per instruction.
            sat_add_o <= sat_add_o || exec_sat_add;
            sat_mul_o <= sat_mul_o || exec_sat_mul;
            sat_rescale_o <= sat_rescale_o || exec_sat_rescale;
            instr_retired_o <= 1'b1;
            pc <= pc + 8'd1;
            state <= Q_FETCH;
          end
        end

        // ---- the multi-cycle path ------------------------------------------
        // Two handshakes and a write-back walk. The unit may stall on either
        // side, so both are held rather than pulsed: `v_valid` stays up until
        // `v_ready`, `r_ready` stays up until `r_valid`.
        //
        // THE HANDSHAKE IS STILL NOT EXERCISED AS A PROTOCOL, and it is worth
        // saying so twice. The sequencer issues one op and drains it before
        // issuing another, so `v_ready` is always high when Q_MISS asks and
        // `r_valid` persists until consumed. Four mutations of these two states
        // survived the sweep for that reason before the rearchitecture, and the
        // reason has not changed.
        //
        // What DID change is that the units now share one multiplier, so the
        // claim "only one op is live" stopped being a scheduling convenience
        // and became the safety argument for the whole engine. It is tested as
        // one: field_seq_directed runs every op alone and then in hostile
        // sequences and requires every answer and every ledger lane to match.
        // ENFORCED-BY: tests/differential/field_seq_directed.cpp
        Q_MISS: begin
          if (multi_vready) state <= Q_MWAIT;
        end

        Q_MWAIT: begin
          if (multi_rvalid) begin
            // Latch every lane on the accepting edge. The unit is free to drop
            // its outputs once the handshake completes, so the walk below must
            // read these registers and not the unit.
            m_o1 <= multi_o1;
            m_o2 <= multi_o2;
            sat_add_o <= sat_add_o || multi_sat_add;
            sat_mul_o <= sat_mul_o || multi_sat_mul;
            sat_rescale_o <= sat_rescale_o || multi_sat_rescale;
            sat_rcp_o <= sat_rcp_o || multi_sat_rcp;
            rcp0_o <= rcp0_o || multi_rcp0;
            // lane 0 is written by the file's writer below, this same edge
            if (multi_width > 2'd1) begin
              state <= Q_WB1;
            end else begin
              instr_retired_o <= 1'b1;
              pc <= pc + 8'd1;
              state <= Q_FETCH;
            end
          end
        end

        Q_WB1: begin
          if (multi_width > 2'd2) begin
            state <= Q_WB2;
          end else begin
            instr_retired_o <= 1'b1;
            pc <= pc + 8'd1;
            state <= Q_FETCH;
          end
        end

        Q_WB2: begin
          instr_retired_o <= 1'b1;
          pc <= pc + 8'd1;
          state <= Q_FETCH;
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
