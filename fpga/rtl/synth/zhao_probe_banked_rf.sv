// zhao_probe_banked_rf.sv — CHARACTERIZATION PROBE, not a console block.
//
// The owner directive of 2026-08-25 (`reports/PIPELINEINGHINTS`) proposes a
// barrel Field engine whose register file is BANKED rather than walked:
//
//   * bank = register[1:0], so the four banks are the residues mod 4;
//   * `a, a+1, a+2` touch three DIFFERENT banks, and so do `b, b+1, b+2`;
//     `c` adds at most one more read to one bank. So no bank ever needs more
//     than THREE reads for one instruction;
//   * therefore 4 banks x 3 replicated read copies = 12 memories serve all
//     seven source operands in ONE cycle, replacing the current three-state
//     operand walk;
//   * every write is broadcast to the three replicas of its bank;
//   * a multi-output instruction writes at most three CONSECUTIVE registers,
//     which necessarily land in three different mod-4 banks -- so all output
//     lanes may write in one cycle, killing the Q_WB1/Q_WB2 walk too.
//
// The directive's own estimate is ~12 M10Ks for 16 contexts, and it says
// plainly: "Don't trust my count until Quartus proves it." That is what this
// file is for. It is NOT the shipped register file and implements no Field
// semantics -- it is the exact storage shape, wired so nothing can be optimised
// away, so a fit reports the real M10K and ALM cost.
//
// WHY A PROBE RATHER THAN AN ESTIMATE. The earlier register-file conversion
// (wave 3) was the largest single win in this subsystem -- 8.59 -> 33.98 MHz,
// 8,901 -> 4,821 ALM -- and it was MEASURED, not predicted. The prediction that
// preceded it was directionally right and numerically wrong, as every
// prediction in this subsystem has been.
//
// PER-BANK GEOMETRY, stated so the fit can be checked against intent:
//   CONTEXTS x REGS_PER_BANK x 32 bits = 16 x 16 x 32 = 8,192 bits per bank.
//   An M10K is 10,240 bits, so ONE M10K per replica is the hope, and
//   4 banks x 3 replicas = 12 M10Ks total. If Quartus instead splits each
//   replica across two M10Ks the answer is 24, which is still nothing against
//   553 -- but it is a different number and the point is to learn which.
module zhao_probe_banked_rf #(
    parameter int CONTEXTS = 16,
    parameter int REGS     = 64,   // logical registers per context
    parameter int BANKS    = 4,
    parameter int COPIES   = 3     // read replicas per bank
) (
    input  logic        clk,

    // One write, broadcast to every replica of its bank.
    input  logic        wr_en_i,
    input  logic [$clog2(CONTEXTS)-1:0] wr_ctx_i,
    input  logic [$clog2(REGS)-1:0]     wr_reg_i,
    input  logic signed [31:0]          wr_data_i,

    // Seven source reads: a, a+1, a+2, b, b+1, b+2, c.
    input  logic [$clog2(CONTEXTS)-1:0] rd_ctx_i,
    input  logic [$clog2(REGS)-1:0]     rd_a_i,
    input  logic [$clog2(REGS)-1:0]     rd_b_i,
    input  logic [$clog2(REGS)-1:0]     rd_c_i,

    output logic signed [31:0] a0_o, a1_o, a2_o,
    output logic signed [31:0] b0_o, b1_o, b2_o,
    output logic signed [31:0] c_o
);

  localparam int RPB   = REGS / BANKS;                 // registers per bank
  localparam int AW    = $clog2(CONTEXTS * RPB);       // address into one bank

  // The address within a bank is {context, register >> 2}; the low two bits of
  // the register select the BANK and are not part of the address. Both halves
  // are written inline rather than as helper functions: a function taking a
  // register number and ignoring two of its bits is exactly the kind of thing
  // that reads as a mistake later, and the SPLIT is the whole idea here.
  localparam int RSEL = $clog2(REGS);

  // ---- storage: BANKS x COPIES synchronous memories ------------------------
  // Synchronous read, no reset touching the array, no byte enables -- the shape
  // QUARTUS_GOTCHAS §10 says infers. Anything else here would be measuring the
  // wrong thing.
  logic signed [31:0] mem [BANKS][COPIES][0:(1<<AW)-1];
  logic signed [31:0] q   [BANKS][COPIES];

  // Read addresses: copy 0 serves the `a` family, copy 1 the `b` family, and
  // copy 2 serves `c`. That is the worst case the directive describes -- three
  // reads landing on one bank -- expressed structurally so the fit sees it.
  logic [AW-1:0] ra [BANKS][COPIES];

  always_comb begin
    for (int bk = 0; bk < BANKS; bk++) begin
      ra[bk][0] = AW'({rd_ctx_i, rd_a_i[RSEL-1:2]});
      ra[bk][1] = AW'({rd_ctx_i, rd_b_i[RSEL-1:2]});
      ra[bk][2] = AW'({rd_ctx_i, rd_c_i[RSEL-1:2]});
    end
  end

  always_ff @(posedge clk) begin
    for (int bk = 0; bk < BANKS; bk++) begin
      for (int cp = 0; cp < COPIES; cp++) begin
        // The write is broadcast to every replica of the addressed bank.
        if (wr_en_i && (wr_reg_i[1:0] == 2'(bk)))
          mem[bk][cp][AW'({wr_ctx_i, wr_reg_i[RSEL-1:2]})] <= wr_data_i;
        q[bk][cp] <= mem[bk][cp][ra[bk][cp]];
      end
    end
  end

  // ---- the seven operands, selected by the bank of each request ------------
  // Registered request bits, so the select lines up with the synchronous read.
  logic [1:0] ba_q, bb_q, bc_q;
  always_ff @(posedge clk) begin
    ba_q <= rd_a_i[1:0];
    bb_q <= rd_b_i[1:0];
    bc_q <= rd_c_i[1:0];
  end

  // a, a+1, a+2 land in three consecutive banks, mod 4. Same for b.
  assign a0_o = q[ba_q][0];
  assign a1_o = q[(ba_q + 2'd1) & 2'd3][0];
  assign a2_o = q[(ba_q + 2'd2) & 2'd3][0];
  assign b0_o = q[bb_q][1];
  assign b1_o = q[(bb_q + 2'd1) & 2'd3][1];
  assign b2_o = q[(bb_q + 2'd2) & 2'd3][1];
  assign c_o  = q[bc_q][2];

endmodule : zhao_probe_banked_rf
