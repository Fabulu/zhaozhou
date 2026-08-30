// zhao_field_v3_wbarb.sv — merge several writeback streams onto the register
// file's ONE write port.
//
// ENFORCED-BY: tests/differential/field_v3_wbarb_directed.cpp:main
//
// Design: reports/FIELD_V3_DISPATCH.md. `zhao_field_v3_rf` has a single write
// port -- one write, one context, one register, per clock -- and by the time
// the services are attached there are three things wanting it:
//
//     claimant 0   the ALU lanes, one write per retiring instruction
//     claimant 1   the curve service's dispatcher, 4 registers per group
//     claimant 2   the noise unit's dispatcher, 8 per group (NOISE2)
//
// ---------------------------------------------------------------------------
// THE PRIORITY IS A PARAMETER BECAUSE IT IS A MEASUREMENT, NOT A GUESS
// ---------------------------------------------------------------------------
// The obvious move is to copy `zhao_field_v3_mulbank` and put services first,
// so the two read as one decision. That reasoning does NOT transfer, and it is
// worth writing down why.
//
// On the multiplier bank, services-first is a REQUIREMENT: a claimant that
// could not be told "no" would advance as though its multiply had happened.
// (That was true of the curve service until it gained `mul_ready_i`, and it is
// the defect that cost six attempts in the executor.) Here every claimant has
// real backpressure, so nobody is corrupted by losing. The question is only
// who waits, and both answers are defensible:
//
//   * SERVICES FIRST. A drain holds contexts OUT of the ready set until it
//     finishes, so making it wait costs issue slots. But a single NOISE2 drain
//     is eight consecutive writes, and blocking the ALU for eight clocks is a
//     real stall.
//   * ALU FIRST. The ALU can always use the port, so a busy program could
//     starve a drain -- except that a stalled drain holds contexts out of the
//     ready set, which REDUCES the ALU's own supply of work. It is
//     self-limiting rather than a deadlock, but "self-limiting" is a claim
//     about a feedback loop and those are exactly the claims that turn out to
//     be wrong under measurement.
//
// So both are built and `served_o` counts each claimant separately.
//
// ---------------------------------------------------------------------------
// MEASURED 2026-08-28, AND THE ANSWER IS THE DRAIN FIRST
// ---------------------------------------------------------------------------
// tests/differential/field_v3_svcpath_directed.cpp, same traffic, one model,
// an ALU asking every clock against one four-point NOISE2 drain:
//
//     ALU first     drain never finished     drain served 0, stalled 3985
//     drain first   drain done in 31 clocks  drain served 8, ALU stalled 8
//     round robin   drain done in 38 clocks  drain served 8, ALU stalled 8
//
// ALU-FIRST STARVES THE DRAIN OUTRIGHT. The argument above for why it might be
// acceptable -- that a stalled drain holds contexts out of the ready set and so
// reduces the ALU's own supply of work -- is a claim about a FEEDBACK LOOP, and
// this file said in advance that such claims are the ones measurement
// overturns. It did, on the first run.
//
// DRAIN FIRST COSTS THE ALU EXACTLY THE DRAIN'S LENGTH and not one clock more:
// eight stalls for eight writes, once per four-point NOISE2 group. That is what
// makes services-first cheap here rather than a trade-off.
//
// Round robin also works and is strictly worse: the same eight ALU stalls, and
// seven more clocks before the drain finishes.
//
// So the engine should tie `policy_i` to 1. The knob stays because the
// measurement was worth having and will be worth repeating when a second
// service is attached -- and synthesis prunes the unreachable arms once the
// input is a constant.
//
// ROUND ROBIN IS ALSO OFFERED, and unlike on the multiplier bank it is safe
// here for the same reason the choice is open at all: everyone can be refused.
//
// THE POLICY IS AN INPUT, NOT A PARAMETER, and that is a testability decision
// taken deliberately. As a parameter it would need a separate elaborated model
// per policy, so a test could compare two policies only by comparing two
// binaries -- and the whole reason the choice is open is that it has to be
// MEASURED against itself on the same traffic. As an input, one model answers
// the question.
//
// It costs a three-way mux on a three-claimant arbiter, which is nothing, and
// it costs nothing at all once the answer is known: tie `policy_i` to a
// constant in the engine and synthesis prunes the arms that cannot be reached.
// So the knob survives into silicon only if somebody wants it there.
// LANES only changes how WIDE a granted write is, never who wins it. The
// arbitration is about the single port, and one port carrying four points is
// still one port.
module zhao_field_v3_wbarb #(
    parameter int LANES = 1,
    parameter int CLAIMANTS = 3,
    parameter int CONTEXTS  = 8,
    parameter int REGS      = 32
) (
    input var logic clk,
    input var logic rst_n,

    // 0 = claimant 0 (the ALU) first, ascending
    // 1 = highest claimant first, so services outrank the lanes
    // 2 = round robin
    input var logic [1:0] policy_i,

    input  var logic [CLAIMANTS-1:0]                 req_valid_i,
    output var logic [CLAIMANTS-1:0]                 req_ready_o,
    input  var logic [$clog2(CONTEXTS)-1:0]          req_ctx_i  [CLAIMANTS],
    input  var logic [$clog2(REGS)-1:0]              req_reg_i  [CLAIMANTS],
    input  var logic signed [32*LANES-1:0]           req_data_i [CLAIMANTS],

    output var logic                                 wr_en_o,
    output var logic [$clog2(CONTEXTS)-1:0]          wr_ctx_o,
    output var logic [$clog2(REGS)-1:0]              wr_reg_o,
    output var logic signed [32*LANES-1:0]           wr_data_o,

    // ---- evidence ----------------------------------------------------------
    // PER CLAIMANT, because "the port was busy" is not the same finding as
    // "claimant 1 never got it". A single total would hide exactly the
    // starvation this parameter exists to measure.
    output var logic [31:0]                          served_o [CLAIMANTS],
    output var logic [31:0]                          stalled_o [CLAIMANTS]
);

  localparam int CW = (CLAIMANTS <= 1) ? 1 : $clog2(CLAIMANTS);

  logic [CW-1:0] rr_r;      // round robin's rotating start
  logic [CW-1:0] winner_c;
  logic          any_c;

  always_comb begin
    winner_c = '0;
    any_c    = 1'b0;
    if (policy_i == 2'd0) begin
      for (int c = CLAIMANTS - 1; c >= 0; c--) begin
        if (req_valid_i[c]) begin
          winner_c = CW'(c);
          any_c    = 1'b1;
        end
      end
    end else if (policy_i == 2'd1) begin
      for (int c = 0; c < CLAIMANTS; c++) begin
        if (req_valid_i[c]) begin
          winner_c = CW'(c);
          any_c    = 1'b1;
        end
      end
    end else begin
      // Round robin: start at rr_r and take the first asserted claimant going
      // up, wrapping. Written as a descending scan over the ROTATED index so
      // the last assignment wins, which is the same shape as the two fixed
      // policies above rather than a second idiom to read.
      for (int k = CLAIMANTS - 1; k >= 0; k--) begin
        // Narrow, not `int`: an `int` here is 32 bits of which two are used,
        // and the linter is right to say so. The width is the claimant index's
        // width because that is what it IS.
        automatic logic [CW-1:0] c = CW'((int'(rr_r) + k) % CLAIMANTS);
        if (req_valid_i[c]) begin
          winner_c = c;
          any_c    = 1'b1;
        end
      end
    end
  end

  // The write port has no backpressure of its own -- the register file always
  // takes the write -- so the winner is served every clock it asks.
  assign wr_en_o   = any_c;
  assign wr_ctx_o  = req_ctx_i[winner_c];
  assign wr_reg_o  = req_reg_i[winner_c];
  assign wr_data_o = req_data_i[winner_c];

  always_comb begin
    for (int c = 0; c < CLAIMANTS; c++) begin
      req_ready_o[c] = any_c && (winner_c == CW'(c));
    end
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      rr_r <= '0;
      for (int c = 0; c < CLAIMANTS; c++) begin
        served_o[c]  <= 32'd0;
        stalled_o[c] <= 32'd0;
      end
    end else begin
      for (int c = 0; c < CLAIMANTS; c++) begin
        if (req_valid_i[c] && req_ready_o[c]) served_o[c] <= served_o[c] + 32'd1;
        // ASKED AND LOST, not merely asked. A counter that counts requests
        // reads the same as one that counts losses in every test where the
        // claimant loses every clock -- which is exactly the shape a priority
        // test has. That equivalence let a mutant survive the multiplier
        // bank's first sweep (M08), and the fix there was a case where the
        // claimant asks and WINS. The same case belongs in this block's test.
        if (req_valid_i[c] && !req_ready_o[c]) stalled_o[c] <= stalled_o[c] + 32'd1;
      end
      // Advance past the claimant just served, so the next scan starts after
      // it. Only under round robin: leaving it moving under a fixed policy
      // would be dead state that reads as if it did something.
      if ((policy_i == 2'd2) && any_c) begin
        rr_r <= (int'(winner_c) + 1 >= CLAIMANTS) ? '0 : CW'(int'(winner_c) + 1);
      end
    end
  end

endmodule : zhao_field_v3_wbarb
