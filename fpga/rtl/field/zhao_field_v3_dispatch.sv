// zhao_field_v3_dispatch.sv — one service's long-op dispatcher: gather four
// contexts into a four-point request, hold what the reply needs, and drain the
// results back one register per clock.
//
// ENFORCED-BY: tests/differential/field_v3_dispatch_directed.cpp:main
//
// Design: reports/FIELD_V3_DISPATCH.md. This is piece 1 of that document --
// the in-flight slot and the writeback stream -- and it is deliberately ONE
// SERVICE's worth. Each service gets an instance; a separate arbiter merges
// their writeback streams with the ALU's own writes. Keeping them separate is
// what makes each testable alone, and the composition is where the interesting
// failures live rather than inside either.
//
// ---------------------------------------------------------------------------
// WHY GATHERING IS NEEDED AT ALL
// ---------------------------------------------------------------------------
// The services take FOUR POINTS. The executor's four-wide register-file group
// is four MEMBERS of one vector. Those are different axes, and this block
// crosses them: a four-point request is built from FOUR CONTEXTS that have
// reached the same instruction, not from one context's operand group.
//
// That is cheap because every context runs the same program at a different
// point, so four of them arrive at a long op within a few clocks. It is not
// free, because "usually four" is not "always four".
//
// ---------------------------------------------------------------------------
// THE TWO RULES THAT COME FROM "USUALLY IS NOT ALWAYS"
// ---------------------------------------------------------------------------
// 1. ISSUE ON "FOUR GATHERED **OR** NOBODY ELSE CAN JOIN". One context alone
//    executing a CURVE is a legal program -- zhao_probe_ctx_fifo supports a
//    single active context and the barrel test runs exactly that case for ALU
//    ops. Waiting for a fourth context that has already finished its program
//    is a DEADLOCK, so `flush_i` exists and the executor must raise it when no
//    further context can join this group.
//
//    `flush_i` is an input rather than a timeout on purpose. A timeout would
//    turn a liveness bug into a slow path that still passes, which is the
//    worst of both: it hides the condition and costs clocks. The executor
//    knows the answer -- it knows which contexts are active -- so it says so.
//
// 2. PAD UNUSED LANES WITH A RECOGNISABLE VALUE, NEVER ZERO. Zero is a
//    plausible coordinate and a plausible result, so a routing bug that let a
//    padded lane reach a writeback would look correct. zhao_probe_v3_engine
//    ties its unused bank lanes to 3 and 5 for this reason and the same
//    constants are reused here, so the two read as one decision rather than
//    two coincidences.
//
//    `used_r` is what makes the rule enforceable at the far end: a padded
//    lane's result is DISCARDED rather than written, and a test can assert the
//    padded context's registers did not move.
//
// ---------------------------------------------------------------------------
// THE WRITEBACK IS SERIAL, AND THAT IS THE REGISTER FILE'S DOING
// ---------------------------------------------------------------------------
// zhao_field_v3_rf has ONE write port: one write, one context, one register,
// per clock. A reply carries four points, so it drains over
//
//     (used lanes) x (dst_width) clocks
//
// which is 4 for CURVE, RIDGE and DCURVE, 8 for NOISE2 / ROT2 / NORMALIZE2,
// and 12 for ROT3 / NORMALIZE3. A four-point NOISE2 is 20 clocks in its unit
// and eight more draining here -- a 40% tail that competes with the ALU's own
// writes.
//
// That is the first real argument for a second write port and this block does
// NOT assume one. The measurement that should decide it is the composed
// engine's occupancy, which does not exist yet.
//
// DST WIDTH COMES FROM THE OP TABLE, not from a special case per opcode:
// `dst_width` in reference/include/zfield/generated/zfield_optable.hpp is 1
// for CURVE/DCURVE/RIDGE, 2 for NOISE2/ROT2/NORMALIZE2, 3 for
// ROT3/NORMALIZE3. The decode below mirrors that table and nothing else, and
// an opcode it does not know is REFUSED rather than guessed -- an unknown
// width would write the wrong number of registers, which is a corruption
// rather than an error.
module zhao_field_v3_dispatch #(
    parameter int CONTEXTS = 8,
    parameter int REGS     = 32,
    parameter int TAGW     = 8
) (
    input var logic clk,
    input var logic rst_n,

    // ---- from the executor: one context's long op --------------------------
    input  var logic                          long_valid_i,
    output var logic                          long_ready_o,
    input  var logic [$clog2(CONTEXTS)-1:0]   long_ctx_i,
    input  var logic [7:0]                    long_op_i,
    input  var logic [$clog2(REGS)-1:0]       long_dst_i,
    input  var logic signed [31:0]            long_s0_i,
    input  var logic signed [31:0]            long_s1_i,
    input  var logic signed [31:0]            long_s2_i,
    // A FOURTH SOURCE, because ROT3 needs one. Found the same way the
    // immediate was: by wiring the executor to this block and discovering it
    // could not describe the op. The generated op table says ROT3 has n_src 4
    // -- groups {3, 1}, three components AND an angle -- and three scalars
    // cannot carry four.
    //
    // Four is the maximum any long op needs: ROT3 four, RING and NORMALIZE3
    // three, NOISE2 and RIDGE two, CURVE and SPLINE one.
    input  var logic signed [31:0]            long_s3_i,
    // THE INSTRUCTION'S IMMEDIATE, which is an OPERAND to several long ops and
    // was missing until the first attempt to compose this with a service.
    // NOISE2 and RIDGE take their seed from it, CURVE and SPLINE their table
    // index, ROT3 its axis. Without it the dispatcher can gather a group it
    // cannot describe.
    //
    // It is part of the GROUP KEY, not just cargo: four contexts at the same
    // instruction share it, but nothing in this block knows they are at the
    // same instruction -- it only sees op and destination, and two different
    // NOISE2 instructions with different seeds match on both. Letting those
    // share a request would hand four points one seed and silently answer
    // three of them for a different program point.
    input  var logic        [31:0]            long_imm_i,
    // "No further context can join this group." See rule 1.
    input  var logic                          flush_i,

    // ---- to the service: one four-point request ----------------------------
    output var logic                          svc_valid_o,
    input  var logic                          svc_ready_i,
    output var logic [7:0]                    svc_op_o,
    output var logic signed [31:0]            svc_s0_o [4],
    output var logic signed [31:0]            svc_s1_o [4],
    output var logic signed [31:0]            svc_s2_o [4],
    output var logic signed [31:0]            svc_s3_o [4],
    output var logic        [31:0]            svc_imm_o,
    output var logic [TAGW-1:0]               svc_tag_o,

    // ---- from the service: four results, in accept order -------------------
    input  var logic                          rsp_valid_i,
    output var logic                          rsp_ready_o,
    input  var logic [TAGW-1:0]               rsp_tag_i,
    input  var logic signed [31:0]            rsp_r0_i [4],
    input  var logic signed [31:0]            rsp_r1_i [4],
    input  var logic signed [31:0]            rsp_r2_i [4],

    // ---- to the register file, one register per clock ----------------------
    output var logic                          wb_valid_o,
    input  var logic                          wb_ready_i,
    output var logic [$clog2(CONTEXTS)-1:0]   wb_ctx_o,
    output var logic [$clog2(REGS)-1:0]       wb_reg_o,
    output var logic signed [31:0]            wb_data_o,

    // ---- back to the context FIFO ------------------------------------------
    // A context that issued a long op LEFT the ready set. This is how it comes
    // back, and it pulses once per context after that context's LAST register
    // has been written -- never before, or the context could re-issue and read
    // a register the drain has not reached.
    output var logic                          rel_valid_o,
    output var logic [$clog2(CONTEXTS)-1:0]   rel_ctx_o,

    // ---- evidence ----------------------------------------------------------
    output var logic [31:0]                   groups_o,       // groups issued
    output var logic [31:0]                   partial_o,      // issued short
    output var logic [31:0]                   writes_o,       // registers written
    // A reply whose tag is not the one outstanding. The service replies in
    // accept order and only one group is outstanding, so this can never fire.
    // It is an output rather than an assertion because the same choice caught
    // a real pipeline bug in zhao_probe_v3_exec on its first run.
    output var logic                          tag_mismatch_o
);

  localparam int CTXW = $clog2(CONTEXTS);
  localparam int REGW = $clog2(REGS);

  // The pad constants are zhao_probe_v3_engine's, deliberately. See rule 2.
  localparam logic signed [31:0] PAD_A = 32'sd3;
  localparam logic signed [31:0] PAD_B = 32'sd5;
  localparam logic signed [31:0] PAD_C = 32'sd7;
  localparam logic signed [31:0] PAD_D = 32'sd11;

  // ---- the op table's dst_width, mirrored and nothing else ----------------
  // The opcode constants that stood here are GONE, not moved twice: they
  // live in zhao_field_ops_pkg.sv with the width table that uses them.
  // Keeping a private copy is how this block and the executor came to
  // disagree about SPLINE and RING in the first place.

  // 0 means "not a long op this block knows", and it is REFUSED rather than
  // guessed: a wrong width writes the wrong number of registers, which is a
  // corruption rather than an error.
  //
  // TWO OPS HAVE FOUR-POINT BLOCKS BUILT AND CLOSED AND ARE STILL ABSENT FROM
  // THIS LIST. That is worth saying out loud, because absence here looks
  // exactly like an omission and one of the two IS deliberate. A reader who
  // finds no OP_SPLINE case, knowing zhao_field_v3_spline.sv is closed at
  // 21/21, has every reason to think a case was forgotten.
  //
  //   OP_SPLINE (0x1B) -- DELIBERATE. Fieldv3.md section 6 puts spline on the
  //     COLD SERVICE LANE: keep the complete exact scalar implementation,
  //     classify it as exact but not certified for the maximum live-field
  //     workload. zhao_probe_curve_svc.sv states the same in its own header --
  //     "MODES: CURVE (0) and DCURVE (1) only. SPLINE is COLD by the brief's
  //     own service split and is not barreled." The scalar path in
  //     zhao_field_curve.sv implements the whole op, lookup included, and is
  //     live in four test targets. A SPLINE arriving here SHOULD be refused.
  //
  //     What is not settled is whether that stays true. The four-point block
  //     exists and its shape only pays if SPLINE becomes hot -- a decision
  //     recorded in STATUS.md and reports/FIELD_V3_REMAINING_OPS.md, not one
  //     this file may make. If it goes hot, this case list is the first thing
  //     that changes, and it needs a four-point table lookup that does not
  //     exist yet.
  //
  //   UOP_RING_PREP (0xF1) -- NOT YET WIRED, and NOT for the cold-lane reason.
  //     The brief cools "unprepared ring" only; the PREPARED ring is its hot
  //     path, costed there at "approximately nine vector-multiplier issue
  //     slots, not four scalar runs through a 50-clock FSM", which is exactly
  //     what zhao_field_v3_ring.sv implements and what its 23/23 sweep scores.
  //     So this one is a genuine gap rather than a decision: the block is
  //     ready and the dispatcher cannot reach it.
  //
  //     Note it is 0xF1, a UOP, not OP_RING (0x21) -- the canonical opcode a
  //     varying-radius ring would arrive as, which stays cold. Adding the
  //     wrong one of those two would route the expensive form into a block
  //     that does not implement it, which is the mistake wrong_op_o in
  //     zhao_field_v3_svcpath.sv exists to catch.
  // DERIVED, NOT DECLARED -- see zhao_field_ops_pkg.sv. The executor asks
  // the same table whether to offer an op that this one asks whether to
  // accept, so the two cannot disagree. They did, and it deadlocked.
  function automatic logic [1:0] dst_width_of(input logic [7:0] op);
    dst_width_of = zhao_field_ops_pkg::field_long_width(op);
  endfunction

  // ---- the gather ---------------------------------------------------------
  logic [2:0]                fill_r;      // 0..4 points gathered
  logic [7:0]                g_op_r;
  logic [REGW-1:0]           g_dst_r;
  logic [31:0]               g_imm_r;
  logic [CTXW-1:0]           g_ctx_r [4];
  logic signed [31:0]        g_s0_r [4], g_s1_r [4], g_s2_r [4], g_s3_r [4];

  // ---- the one outstanding slot -------------------------------------------
  typedef enum logic [1:0] {D_GATHER, D_ISSUE, D_WAIT, D_DRAIN} state_e;
  state_e state_r;

  logic [7:0]                s_op_r;
  logic [REGW-1:0]           s_dst_r;
  logic [31:0]               s_imm_r;
  logic [CTXW-1:0]           s_ctx_r [4];
  logic [2:0]                s_used_r;    // how many lanes were real
  logic [1:0]                s_width_r;
  logic [TAGW-1:0]           s_tag_r;
  logic [TAGW-1:0]           next_tag_r;

  logic signed [31:0]        r0_r [4], r1_r [4], r2_r [4];

  // The drain walks (lane, member) in that order: all of a point's registers
  // land together, so the release pulse for that context can follow its last
  // write immediately.
  logic [2:0]                d_lane_r;
  logic [1:0]                d_memb_r;

  logic same_group_c;
  // A context joins the group only if it is running the SAME op with the SAME
  // destination base. Different ops cannot share a request -- the service is
  // told one opcode -- and different destinations cannot share a drain.
  assign same_group_c = (fill_r == 3'd0) ||
                        ((long_op_i == g_op_r) && (long_dst_i == g_dst_r) &&
                         (long_imm_i == g_imm_r));

  // `!flush_i` IS LOAD-BEARING AND IT CLOSES A LOST-CONTEXT HOLE. Without it,
  // a context offered on the same clock a partial group flushes would be
  // ACCEPTED into g_* and then thrown away: the snapshot takes the pre-accept
  // `fill_r`, so the new point is outside `s_used_r`, and D_ISSUE then clears
  // fill_r. The context would have handshaked and vanished -- a lost
  // instruction, not a slow one.
  //
  // Refusing instead is both correct and lossless. `flush_i` means "nobody
  // else can join", so an offer arriving with it is a contradiction on the
  // executor's side; dropping ready leaves the offer standing and it joins the
  // NEXT group. A design that is robust to its own contract being broken beats
  // one that is merely right about who broke it.
  assign long_ready_o = (state_r == D_GATHER) && (fill_r < 3'd4) && !flush_i &&
                        (dst_width_of(long_op_i) != 2'd0) && same_group_c;

  // Issue when the group is full, when the executor says nobody else can join
  // and there is at least one point to send, OR WHEN SOMEBODY IS ASKING AND
  // CANNOT JOIN. Rule 1, plus the case that rule 1 did not cover.
  //
  // ---------------------------------------------------------------------------
  // THE THIRD TERM FIXES A REAL DEADLOCK, FOUND 2026-08-29
  // ---------------------------------------------------------------------------
  // Two contexts running DIFFERENT long ops at the same time hung the machine
  // forever. Not slowly -- forever, with nothing timing out and no flag raised.
  //
  //   1. Context A offers NOISE2. fill_r goes to 1 and A is marked `waiting`.
  //   2. Context B offers RIDGE. `same_group_c` is false, so `long_ready_o` is
  //      low and B is refused -- correctly, a group carries ONE opcode.
  //   3. But `waiting_r` is set only for a context the dispatcher ACCEPTED, so
  //      B stays active-and-not-waiting.
  //   4. `flush_o = ~|(active_r & ~waiting_r)` therefore never asserts.
  //   5. The group of one never issues, so A is never released, so B is never
  //      accepted. Both wait on each other.
  //
  // THE FIFTH SEAM DEFECT IN THIS ENGINE AND THE SAME SHAPE AS THE OTHER FOUR:
  // two blocks that must agree, with nothing forcing them to. The dispatcher
  // decides who may join; the executor decides whether anyone else might; and
  // "refused because the op differs" was invisible to the side computing the
  // second answer.
  //
  // It survived nine mutation sweeps and two closed compositions because every
  // test in the suite gave all contexts the SAME op, so a group always either
  // filled or was flushed. It is not exotic traffic: eight contexts run eight
  // independent programs, so they reach different long ops as a matter of
  // course.
  //
  // The repair is local, which is why it is this one. The dispatcher already
  // knows both halves -- `long_valid_i` says somebody is asking and
  // `same_group_c` says they cannot join -- so closing the group here needs no
  // new agreement with anybody. Widening `flush_o` instead would have meant
  // teaching the executor why the dispatcher refused, which is another copy of
  // the same seam.
  //
  // `!same_group_c` already implies `fill_r != 0` (an empty group accepts
  // anyone), so no fill test is repeated here.
  logic issue_now_c;
  assign issue_now_c = (state_r == D_GATHER) &&
                       ((fill_r == 3'd4) || (flush_i && (fill_r != 3'd0)) ||
                        (long_valid_i && !same_group_c));

  // ---- the request ports --------------------------------------------------
  assign svc_valid_o = (state_r == D_ISSUE);
  assign svc_op_o    = s_op_r;
  assign svc_imm_o   = s_imm_r;
  assign svc_tag_o   = s_tag_r;

  always_comb begin
    for (int l = 0; l < 4; l++) begin
      // Rule 2: a lane nobody filled carries a value that is obviously not a
      // coordinate, so a routing bug looks wrong rather than convincing.
      if (3'(l) < s_used_r) begin
        svc_s0_o[l] = g_s0_r[l];
        svc_s1_o[l] = g_s1_r[l];
        svc_s2_o[l] = g_s2_r[l];
        svc_s3_o[l] = g_s3_r[l];
      end else begin
        svc_s0_o[l] = PAD_A;
        svc_s1_o[l] = PAD_B;
        svc_s2_o[l] = PAD_C;
        svc_s3_o[l] = PAD_D;
      end
    end
  end

  assign rsp_ready_o = (state_r == D_WAIT);

  // ---- the drain ----------------------------------------------------------
  logic signed [31:0] wb_data_c;
  always_comb begin
    unique case (d_memb_r)
      2'd0:    wb_data_c = r0_r[d_lane_r[1:0]];
      2'd1:    wb_data_c = r1_r[d_lane_r[1:0]];
      default: wb_data_c = r2_r[d_lane_r[1:0]];
    endcase
  end

  assign wb_valid_o = (state_r == D_DRAIN);
  assign wb_ctx_o   = s_ctx_r[d_lane_r[1:0]];
  assign wb_reg_o   = s_dst_r + REGW'(d_memb_r);
  assign wb_data_o  = wb_data_c;

  // The release follows a context's LAST register, on the same clock it is
  // accepted. Earlier and the context could re-issue and read a register the
  // drain has not written yet.
  assign rel_valid_o = wb_valid_o && wb_ready_i &&
                       (d_memb_r == 2'(s_width_r - 2'd1));
  assign rel_ctx_o   = s_ctx_r[d_lane_r[1:0]];

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state_r    <= D_GATHER;
      fill_r     <= 3'd0;
      g_op_r     <= 8'd0;
      g_dst_r    <= '0;
      g_imm_r    <= 32'd0;
      s_op_r     <= 8'd0;
      s_dst_r    <= '0;
      s_imm_r    <= 32'd0;
      s_used_r   <= 3'd0;
      s_width_r  <= 2'd0;
      s_tag_r    <= '0;
      next_tag_r <= '0;
      d_lane_r   <= 3'd0;
      d_memb_r   <= 2'd0;
      groups_o   <= 32'd0;
      partial_o  <= 32'd0;
      writes_o   <= 32'd0;
      tag_mismatch_o <= 1'b0;
      for (int l = 0; l < 4; l++) begin
        g_ctx_r[l] <= '0;
        g_s0_r[l]  <= '0;
        g_s1_r[l]  <= '0;
        g_s2_r[l]  <= '0;
        g_s3_r[l]  <= '0;
        s_ctx_r[l] <= '0;
        r0_r[l]    <= '0;
        r1_r[l]    <= '0;
        r2_r[l]    <= '0;
      end
    end else begin
      // ---- accept one context into the group ------------------------------
      if (long_valid_i && long_ready_o) begin
        g_op_r  <= long_op_i;
        g_dst_r <= long_dst_i;
        g_imm_r <= long_imm_i;
        g_ctx_r[fill_r[1:0]] <= long_ctx_i;
        g_s0_r[fill_r[1:0]]  <= long_s0_i;
        g_s1_r[fill_r[1:0]]  <= long_s1_i;
        g_s2_r[fill_r[1:0]]  <= long_s2_i;
        g_s3_r[fill_r[1:0]]  <= long_s3_i;
        fill_r <= fill_r + 3'd1;
      end

      case (state_r)
        D_GATHER: begin
          // `issue_now_c` reads fill_r BEFORE this clock's accept, so a group
          // that fills and flushes on the same clock issues next clock with
          // the newly accepted point included. That is why the snapshot below
          // uses the post-accept count.
          if (issue_now_c) begin
            s_op_r    <= g_op_r;
            s_dst_r   <= g_dst_r;
            s_imm_r   <= g_imm_r;
            s_width_r <= dst_width_of(g_op_r);
            s_used_r  <= fill_r;
            s_tag_r   <= next_tag_r;
            for (int l = 0; l < 4; l++) s_ctx_r[l] <= g_ctx_r[l];
            state_r   <= D_ISSUE;
            if (fill_r != 3'd4) partial_o <= partial_o + 32'd1;
          end
        end

        D_ISSUE: begin
          if (svc_ready_i) begin
            next_tag_r <= next_tag_r + TAGW'(1);
            groups_o   <= groups_o + 32'd1;
            fill_r     <= 3'd0;
            state_r    <= D_WAIT;
          end
        end

        D_WAIT: begin
          if (rsp_valid_i) begin
            // ONE GROUP IS OUTSTANDING AND THE SERVICE REPLIES IN ACCEPT
            // ORDER, so the tag can only be this one. Checked anyway: a guard
            // that never fires costs a comparator, and the same choice caught
            // a real bug in the executor on its first run.
            if (rsp_tag_i != s_tag_r) tag_mismatch_o <= 1'b1;
            for (int l = 0; l < 4; l++) begin
              r0_r[l] <= rsp_r0_i[l];
              r1_r[l] <= rsp_r1_i[l];
              r2_r[l] <= rsp_r2_i[l];
            end
            d_lane_r <= 3'd0;
            d_memb_r <= 2'd0;
            state_r  <= D_DRAIN;
          end
        end

        D_DRAIN: begin
          if (wb_ready_i) begin
            writes_o <= writes_o + 32'd1;
            if (d_memb_r == 2'(s_width_r - 2'd1)) begin
              d_memb_r <= 2'd0;
              // A PADDED LANE IS NEVER DRAINED. The loop runs to s_used_r, not
              // to four, so a lane nobody filled writes nothing and its
              // context -- which does not exist -- is never released.
              if (d_lane_r + 3'd1 >= s_used_r) begin
                state_r <= D_GATHER;
              end else begin
                d_lane_r <= d_lane_r + 3'd1;
              end
            end else begin
              d_memb_r <= d_memb_r + 2'd1;
            end
          end
        end

        default: state_r <= D_GATHER;
      endcase
    end
  end

endmodule : zhao_field_v3_dispatch
