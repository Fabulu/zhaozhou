// zhao_field_v3_rf.sv — the Field v3 vector register file, FUNCTIONAL.
//
// WHY THIS EXISTS SEPARATELY FROM THE PROBE
// ------------------------------------------
// `fpga/rtl/synth/zhao_probe_banked_rf.sv` measures the STORAGE SHAPE of the
// owner's residue-banked directive so Quartus can price it. Its own header is
// explicit: "It is NOT the shipped register file and implements no Field
// semantics." It addresses every bank with the SAME row —
//
//     ra[bk][0] = {rd_ctx_i, rd_a_i[RSEL-1:2]}   // same row for all four
//
// — which is right for measuring memories and WRONG for reading a register
// group, because `a, a+1, a+2` do not share a row when the group crosses a
// multiple of four. Register 3's successor is register 4, and that is a
// different row of a different bank.
//
// Found 2026-08-28, and the way it was found is the point: the v3 executor
// was built on the probe and its differential passed 440 randomized programs.
// Every one of those used only scalar ops, which read `a0`, `b0` and `c` and
// never touch `a+1` or `a+2`. The first DOT2 exposed it — 30 of 400 programs
// disagreed with the interpreter, exactly those whose group start was 2 or 3
// modulo 4.
//
// THE ADDRESSING, which is the whole difference
// ----------------------------------------------
// Bank of register r is `r[1:0]`; its row is `r >> 2`. For the a-family we
// want r in {a, a+1, a+2}. Physical bank `bk` therefore serves the member at
// offset `w = (bk - a) mod 4`, and that member lives at row `(a + w) >> 2`.
// So each bank gets its OWN address, not a shared one.
//
// The bank ROTATION on the read side is unchanged from the directive:
// `a, a+1, a+2` are three different residues, so no bank is ever asked for
// more than three reads and three copies serve every operand in one clock.
//
// COST NOTE, stated because it is a real difference from the measured probe:
// this adds a small add-and-shift per bank per copy that the probe does not
// have. The probe's 372 ALM / 12 M10K / 93.14 MHz is therefore a LOWER BOUND
// on this module, not a measurement of it. Re-fit before quoting a number.
//
// Law:
//   reports/PIPELINEINGHINTS               the residue-banked directive
//   reports/FIELD_V3_EXECUTOR_REGFILE.md   why this shape rather than the
//                                          four-lanes-x-three-readers one

`default_nettype none

module zhao_field_v3_rf #(
    parameter int CONTEXTS = 8,
    parameter int REGS     = 32
) (
    input var logic clk,

    input var logic                          wr_en_i,
    input var logic [$clog2(CONTEXTS)-1:0]   wr_ctx_i,
    input var logic [$clog2(REGS)-1:0]       wr_reg_i,
    input var logic signed [31:0]            wr_data_i,

    input var logic [$clog2(CONTEXTS)-1:0]   rd_ctx_i,
    input var logic [$clog2(REGS)-1:0]       rd_a_i,
    input var logic [$clog2(REGS)-1:0]       rd_b_i,
    input var logic [$clog2(REGS)-1:0]       rd_c_i,

    output var logic signed [31:0] a0_o, a1_o, a2_o,
    output var logic signed [31:0] b0_o, b1_o, b2_o,
    output var logic signed [31:0] c_o
);

  localparam int BANKS = 4;
  localparam int COPIES = 3;               // copy 0 = a-family, 1 = b, 2 = c
  localparam int RPB = REGS / BANKS;       // registers per bank
  localparam int AW = $clog2(CONTEXTS * RPB);
  localparam int RSEL = $clog2(REGS);

  logic signed [31:0] mem[BANKS][COPIES][0:(1<<AW)-1];
  logic signed [31:0] q[BANKS][COPIES];
  logic [AW-1:0] ra[BANKS][COPIES];

  // The row a given bank must present for a group starting at `base`. `bk` is
  // the physical bank; the member it holds is at offset (bk - base) mod 4, and
  // that member's row is (base + offset) >> 2.
  function automatic logic [AW-1:0] row_for(input logic [$clog2(CONTEXTS)-1:0] ctx,
                                            input logic [RSEL-1:0] base, input logic [1:0] bk);
    logic [1:0] off;
    // Only the CARRY out of this sum is wanted -- its low two bits are the
    // bank selector, which the caller already knows and the address must not
    // contain. That is a real don't-care rather than an oversight, so it is
    // waived here with the reason instead of being disguised by an extra
    // slice that would read as if the bits mattered.
    /* verilator lint_off UNUSEDSIGNAL */
    logic [2:0] low_sum;
    /* verilator lint_on UNUSEDSIGNAL */
    logic [RSEL-3:0] row;
    // The offset of the member this bank holds.
    off = bk - base[1:0];
    // The row is the base row plus whatever the low two bits carry out. The
    // low bits themselves are the BANK selector and are deliberately not part
    // of the address -- computing the carry explicitly says so, where slicing
    // a wider sum would leave two bits looking accidentally unused.
    low_sum = {1'b0, base[1:0]} + {1'b0, off};
    row = base[RSEL-1:2] + {{(RSEL-3){1'b0}}, low_sum[2]};
    row_for = AW'({ctx, row});
  endfunction

  always_comb begin
    for (int bk = 0; bk < BANKS; bk++) begin
      ra[bk][0] = row_for(rd_ctx_i, rd_a_i, 2'(bk));
      ra[bk][1] = row_for(rd_ctx_i, rd_b_i, 2'(bk));
      // `c` is a single register, so only its own bank's address matters; the
      // others are driven to the same row to keep the memories symmetrical
      // and the fit honest.
      ra[bk][2] = AW'({rd_ctx_i, rd_c_i[RSEL-1:2]});
    end
  end

  always_ff @(posedge clk) begin
    for (int bk = 0; bk < BANKS; bk++) begin
      for (int cp = 0; cp < COPIES; cp++) begin
        // Every write is broadcast to all three replicas of its bank.
        if (wr_en_i && (wr_reg_i[1:0] == 2'(bk)))
          mem[bk][cp][AW'({wr_ctx_i, wr_reg_i[RSEL-1:2]})] <= wr_data_i;
        q[bk][cp] <= mem[bk][cp][ra[bk][cp]];
      end
    end
  end

  // Which bank each member came from, held to match the registered read.
  logic [1:0] ba_q, bb_q, bc_q;
  always_ff @(posedge clk) begin
    ba_q <= rd_a_i[1:0];
    bb_q <= rd_b_i[1:0];
    bc_q <= rd_c_i[1:0];
  end

  assign a0_o = q[ba_q][0];
  assign a1_o = q[(ba_q + 2'd1) & 2'd3][0];
  assign a2_o = q[(ba_q + 2'd2) & 2'd3][0];
  assign b0_o = q[bb_q][1];
  assign b1_o = q[(bb_q + 2'd1) & 2'd3][1];
  assign b2_o = q[(bb_q + 2'd2) & 2'd3][1];
  assign c_o  = q[bc_q][2];

endmodule

`default_nettype wire
