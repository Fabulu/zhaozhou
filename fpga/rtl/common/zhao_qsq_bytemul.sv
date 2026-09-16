// zhao_qsq_bytemul.sv -- an exact 8x8 unsigned multiply in ONE M10K and no DSP.
//
// R1's "reusable quarter-square primitive", and it is an EXTRACTION, not a new
// arithmetic. The identity, the table, the cold fill and the exactness proof
// all already existed inside `zhao_terrain_shade`, which built them for its own
// nine 32x32 products and then kept them to itself. R1 has recorded the gap for
// weeks in exactly those words:
//
//     Embedded terrain quarter-square and dual-18 calibration exist. Reusable
//     quarter-square primitive, ... and the promised MapOnly bundle do not.
//
// That is the repository's own uncashed-cheque shape: the hard part was done,
// correctly, and the last step -- making it something another block could
// instantiate -- was never taken. R4 wants it ("The promised quarter-square /
// coefficient-memory replacements are absent"), and so do R5, R6 and FIELD.
//
// OWNER DIRECTION 2026-09-16: *"remember we have lot's of m10k memory, ALM's
// are over budget, so what you can you need to solve with memory."* Measured
// the same day: 40,591 ALM against a 41,910 DEVICE with whole domains still
// contributing zero, against 94 M10K used of 464 allocated. This block is the
// mechanism that direction asks for -- it converts COMPUTATION into LOOKUP,
// which is where the lever actually is, because `check_array_storage` finds no
// fit-rowed block holding 8 Kbit or more of array in flip-flops. The ALM
// breach is logic, not misplaced state.
//
// THE IDENTITY, and why it is EXACT rather than approximate:
//
//     Q[s] = floor(s*s / 4),   s in [0, 510]
//     a*b  = Q[a + b] - Q[|a - b|]
//
// `(a+b)` and `(a-b)` always share parity. If both are even, each square
// divides by four exactly and the difference is ((a+b)^2 - (a-b)^2)/4 = a*b.
// If both are odd, each floor drops exactly 1/4 and the two drops cancel in
// the subtraction, giving a*b again. So there is no rounding term for any byte
// pair, and `tests/common/qsq_bytemul_directed.cpp` drives all 65,536 of them
// through this RTL rather than citing this paragraph. A comment that proves an
// identity is still a comment.
//
// THE TABLE IS FILLED AT RESET, 512 cycles, by the `(s+1)^2 = s^2 + 2s + 1`
// recurrence -- pure adds, no init file, no elaboration multiply. The memory
// process is CLOCK-ONLY and is never touched by reset, which is what lets it
// infer an M10K (`zhao_terrain_normalmap.sv` is the committed precedent for
// fill-at-cold). `table_ready_o` is low until the last word lands and a client
// must not issue before it rises.
//
// SIZE. 512 words x 16 bits = 8,192 bits, one M10K of the 553 on the device.
// Q[510] = 65,025 and the largest product 255*255 = 65,025, so sixteen bits is
// exact at both ends and seventeen would be waste; the elaboration check below
// asserts it rather than trusting this sentence.
//
// `en_i` IS NOT OPTIONAL FOR A CLIENT THAT STALLS, and the reason is in
// CLAUDE.md as its own chapter. A metadata bank in this repository registered
// its read UNCONDITIONALLY, so on a stall it produced response A's data beside
// response B's metadata -- with every counter balancing, because no counter
// looks at the field that moved. Drive `en_i` with the SAME enable that gates
// the metadata travelling beside the product, and the two cannot separate. A
// client with a fixed-rate walk may tie it high; one with backpressure may not.
//
// LATENCY IS ONE CYCLE and there is no handshake: present `a_i`/`b_i` with
// `en_i` high at cycle N, read `p_o` at N+1. A deliberate choice -- the clients
// differ in how they accumulate byte products (a square walks ten pairs with
// the cross terms doubled; a full multiply walks sixteen) and pushing that
// shape in here would make the primitive fit exactly one of them.
module zhao_qsq_bytemul (
    input  logic        clk,
    input  logic        rst_n,

    // Low for the first 512 cycles after reset while the table is built.
    output logic        table_ready_o,

    // Capture (a_i, b_i) this cycle; p_o carries a_i*b_i on the next.
    input  logic        en_i,
    input  logic [ 7:0] a_i,
    input  logic [ 7:0] b_i,
    output logic [15:0] p_o
);

  localparam int unsigned QROM_DEPTH = 512;  // Q[s], s in [0, 511]
  localparam int unsigned QW         = 16;   // floor(510^2/4) = 65,025 < 2^16

  // Inside `initial begin ... end`, not at module scope: Quartus 17.0 rejects a
  // bare module-scope elaboration `if` with "syntax error near text: `if`;
  // expecting `endmodule`", and Verilator lints it clean, so the failure only
  // appears at the first quartus_map. CLAUDE.md records both halves.
  initial begin
    if ((510 * 510) / 4 > (1 << QW) - 1)
      $fatal(1, "zhao_qsq_bytemul: QW too narrow for the quarter-square table");
    if (255 * 255 > (1 << QW) - 1)
      $fatal(1, "zhao_qsq_bytemul: QW too narrow for the largest byte product");
    if (QROM_DEPTH < 511)
      $fatal(1, "zhao_qsq_bytemul: the table cannot address a+b = 510");
  end

  // ---- the one memory, true dual port -------------------------------------
  // Written only while filling (port A), read on both ports afterwards. No
  // reset anywhere in these two processes; that is the M10K inference rule.
  logic [QW-1:0] qmem [0:QROM_DEPTH-1];
  logic [QW-1:0] qa_q, qb_q;

  logic        filling_q;
  logic [ 8:0] fill_s_q;   // s, 0..511
  logic [17:0] fill_sq_q;  // s^2 by recurrence; 511^2 = 261,121 < 2^18

  wire [8:0] qaddr_a_c = {1'b0, a_i} + {1'b0, b_i};
  wire [7:0] qaddr_b_c = (a_i >= b_i) ? (a_i - b_i) : (b_i - a_i);

  always_ff @(posedge clk) begin
    if (filling_q) qmem[fill_s_q] <= fill_sq_q[17:2];  // floor(s^2/4)
    else if (en_i) qa_q           <= qmem[qaddr_a_c];
  end

  always_ff @(posedge clk) begin
    if (en_i) qb_q <= qmem[{1'b0, qaddr_b_c}];
  end

  // The subtraction is COMBINATIONAL off the two read registers, so the client
  // sees the product on the cycle after it issued rather than two after.
  assign p_o = qa_q - qb_q;

  // ---- cold fill -----------------------------------------------------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      filling_q     <= 1'b1;
      fill_s_q      <= 9'd0;
      fill_sq_q     <= 18'd0;
      table_ready_o <= 1'b0;
    end else if (filling_q) begin
      // At this edge qmem[s] takes floor(s^2/4) above; here s^2 advances to
      // (s+1)^2 = s^2 + 2s + 1 and s advances with it.
      fill_sq_q <= fill_sq_q + {8'd0, fill_s_q, 1'b0} + 18'd1;
      fill_s_q  <= fill_s_q + 9'd1;
      if (fill_s_q == 9'd511) begin
        filling_q     <= 1'b0;
        table_ready_o <= 1'b1;
      end
    end
  end

`ifndef SYNTHESIS
  // A client that issues before the table is built reads whatever the M10K
  // powered up holding, and gets a WRONG PRODUCT rather than an error -- the
  // worst shape, because the arithmetic downstream is exact and will carry it
  // faithfully. Simulation says so loudly; `table_ready_o` is the contract.
  // The guard is `!table_ready_o` ALONE. An earlier draft read
  // `en_i && !table_ready_o && !filling_q`, and `filling_q` is high for exactly
  // the 512 cycles the check exists to police -- so it could not fire in its
  // own window, which is this repository's own law about detectors wired to
  // operands that move together. There is no `rst_n` term either: reset drives
  // table_ready_o low, so the condition is already covered and reading rst_n
  // synchronously here would be a SYNCASYNCNET warning.
  always_ff @(posedge clk) begin
    if (en_i && !table_ready_o)
      $fatal(1, "zhao_qsq_bytemul: issued before table_ready_o -- the M10K holds whatever it powered up with and the product will be silently wrong");
  end
`endif

endmodule
