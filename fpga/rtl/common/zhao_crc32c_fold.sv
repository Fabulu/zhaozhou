// zhao_crc32c_fold.sv — fold up to eight bytes of CRC-32C in ONE shallow
// XOR tree instead of sixty-four dependent levels.
//
// WHY THIS EXISTS, measured rather than assumed. The composed shell fit at
// commit d67621d fits the device comfortably (9,167 ALMs of 41,910) and misses
// its 10 ns gpu_clk target by 55 ns. `setup_paths.rpt` names the paths, and two
// of the three worst families are CRC accumulators:
//
//   -55.199 ns  zhao_cmd_dma|hdr_win[28][4] -> zhao_cmd_dma|crc_pay_r[3]
//   -28.778 ns  zhao_hps_arbiter|state      -> zhao_debug_frameblit|crc_acc
//
// The cause is one shared function. `zhao_abi_pkg::zhao_crc32c_step` is
// BIT-SERIAL — eight dependent XOR levels per byte:
//
//   crc = c ^ {24'b0, d};
//   for (int i = 0; i < 8; i++)
//     crc = (crc >> 1) ^ (crc[0] ? 32'h82F63B78 : 32'b0);
//
// So folding a bridge beat costs 8 x 8 = 64 chained levels (~38 ns measured),
// and CMD.DMA's 28-byte payload-CRC seed costs 224 (~64 ns measured). Against
// a 10 ns budget, at roughly 0.6 ns per level, the affordable depth is about
// sixteen levels — two bytes. SPREADING THE WORK OVER MORE CYCLES CANNOT FIX
// IT: DEBUG.FRAMEBLIT already folds only 8 bytes per cycle and still costs
// 38 ns, and the streaming payload CRC has to keep up with 8 bytes per beat.
//
// THE ALGEBRA. CRC-32C is a LINEAR function over GF(2) with no constant term
// (fold(0, 0) = 0), so folding N bytes is a fixed binary matrix:
//
//   fold_N(c, d) = XOR over set bits i of c of fold_N(1<<i, 0)
//              XOR XOR over set bits j of d of fold_N(0, 1<<j)
//
// Each column is a compile-time constant, so the runtime logic is 96 masked
// 32-bit XORs — a balanced tree about seven levels deep rather than 64. The
// columns are derived HERE, at elaboration, by calling the bit-serial function
// itself, so this module cannot drift from the definition it replaces: change
// the polynomial and the columns change with it.
//
// `n_i` selects how many leading bytes of `d_i` participate, because none of
// the callers always has eight: the seed walks a byte range that ends wherever
// command_bytes says, and a final bridge beat can be partial. Each byte count
// is a different matrix, so all nine are elaborated and muxed.
//
// EQUIVALENCE IS THE WHOLE CONTRACT and it is checked, not asserted:
// tests/differential/crc32c_fold_directed.cpp drives this against the shipped
// `zhao_crc32c_step` chained n times.
`default_nettype none

module zhao_crc32c_fold (
    input  wire [31:0] c_i,   // running CRC state
    input  wire [63:0] d_i,   // up to eight bytes, LOW byte folded FIRST
    input  wire [3:0]  n_i,   // how many leading bytes to fold, 0..8
    output logic [31:0] c_o
);

  // The DEFINITION, used only at elaboration to derive the columns. This is a
  // copy of zhao_abi_pkg::zhao_crc32c_step's body rather than a call to it,
  // because this module is deliberately usable without the generated package;
  // the differential is what keeps the two honest.
  function automatic logic [31:0] step1(input logic [31:0] c, input logic [7:0] d);
    logic [31:0] crc;
    begin
      crc = c ^ {24'b0, d};
      for (int i = 0; i < 8; i++) begin
        crc = (crc >> 1) ^ (crc[0] ? 32'h82F63B78 : 32'b0);
      end
      step1 = crc;
    end
  endfunction

  // fold exactly n bytes of d, low byte first
  function automatic logic [31:0] foldn(input logic [31:0] c, input logic [63:0] d,
                                        input int unsigned n);
    logic [31:0] acc;
    begin
      acc = c;
      for (int unsigned k = 0; k < 8; k++) begin
        if (k < n) acc = step1(acc, d[8*k +: 8]);
      end
      foldn = acc;
    end
  endfunction

  // Nine matrices, one per byte count. Every foldn() call below has constant
  // arguments, so each is a compile-time constant and what remains in logic is
  // the masked XOR reduction.
  logic [31:0] res [0:8];

  // The genvar is declared SEPARATELY rather than inline in the for header.
  // Quartus 17.0.2's Verilog parser rejects `for (genvar N = ...)` with
  //   Error (10170): syntax error near text "genvar"
  // while Verilator and slang both accept it, so this only surfaced in the
  // composed fit -- which is the argument for running it rather than trusting
  // that three frontends agree.
  genvar N;
  generate
    for (N = 0; N <= 8; N++) begin : g_fold
      always_comb begin
        logic [31:0] acc;
        acc = 32'd0;
        for (int i = 0; i < 32; i++) begin
          if (c_i[i]) acc = acc ^ foldn(32'd1 << i, 64'd0, N);
        end
        for (int j = 0; j < 8 * N; j++) begin
          if (d_i[j]) acc = acc ^ foldn(32'd0, 64'd1 << j, N);
        end
        res[N] = acc;
      end
    end
  endgenerate

  // n_i above 8 is not a legal request; the callers all bound it, and folding
  // nothing is the safe reading rather than folding a wrong count.
  always_comb begin
    c_o = (n_i <= 4'd8) ? res[n_i] : c_i;
  end

endmodule

`default_nettype wire
