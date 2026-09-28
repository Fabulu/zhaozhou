// zhao_probe_uopstore -- WHY DOES THE FIELD EXEC'S uop STORE NOT INFER?
//
// ---------------------------------------------------------------------------
// THE FINDING THIS PROBE EXISTS TO TEST
// ---------------------------------------------------------------------------
// `zhao_field_v3_exec.sv:302` declares
//
//     uop_t store[0:(CTX*PLAN)-1];        // CTX=8, PLAN=32 -> 256 entries
//
// with `uop_t` = {op[7:0], dst[4:0], a[4:0], b[4:0], c[4:0], imm[31:0]} = 60
// bits, so the array is 256 x 60 = 15,360 bits. It is written at ONE address
// (`:1090`) and read at ONE address into a register (`:1200`), is never reset,
// and is therefore a textbook single-port-write / registered-read shape that
// ought to become an M10K.
//
// IT DOES NOT. Measured on the 2026-09-28 console map:
//
//     zhao_field_v3_exec:u_exec     own reg 24,795   subtree mem 25,344
//     zhao_field_v3_rf:u_rf         own reg      6   subtree mem 24,576
//
// so only 768 memory bits exist in the exec outside its register file, and no
// `altsyncram` under `u_exec` carries the name `store`. **The 15,360 bits are in
// flip-flops, and they are ~62% of the exec's own registers.**
//
// IT IS ALSO INVISIBLE TO OUR INSTRUMENT. `check_ram_inference.py`'s declaration
// recogniser is `^\s*(?:logic|reg|bit)\s*...`, so an array declared with a
// user-defined type never enters the scan at all. The external R4 review found
// that blind spot by reading the regex; this is a real instance of it, in the
// slice both candidates independently chose for the first replacement
// experiment. A census that cannot see a 15,360-bit array is not a bound on
// remaining storage, in either direction.
//
// ---------------------------------------------------------------------------
// THE HYPOTHESIS UNDER TEST
// ---------------------------------------------------------------------------
// Production addresses the array with a WIDE SIGNED expression:
//
//     store[(int'(up_ctx_i) * PLAN) + int'(up_pc_i)]
//
// `int'()` is 32-bit SIGNED. To infer a RAM, Quartus must prove the index lies
// within 0..255; a signed 32-bit product-plus-sum is a shape it has been seen
// to decline. The candidate repair is a NARROW UNSIGNED address of exactly
// CW+PW bits, which for a power-of-two PLAN is a plain concatenation and is
// bit-identical arithmetic.
//
//   STYLE=0  PRODUCTION SHAPE -- `int'()` casts, exactly as shipped.
//   STYLE=1  NARROW UNSIGNED ADDRESS -- {ctx, pc}, same entries, same widths.
//   STYLE=2  POSITIVE CONTROL -- plain `logic [59:0]` array, narrow address.
//            This MUST infer. If it does not, styles 0 and 1 say nothing and
//            the flow or the device is at fault, not the address expression.
//
// Read the result with tools/quartus/check_ram_inference.py or the map's
// inferred-memory table -- and read STYLE=2 FIRST, always.
//
// WHAT THIS PROBE DOES NOT CLAIM. It does not claim the repair is free: moving
// the store to an M10K adds a read-latency stage that `zhao_field_v3_exec`'s
// issue path must absorb, and that is a schedule change, not a rename. It does
// not claim 15,360 bits of ALM savings -- bits/4 is capacity arithmetic, not an
// integrated area measurement, which is exactly the error R4 corrected. It
// establishes one thing: whether the address expression is the blocker.

module zhao_probe_uopstore #(
    parameter int unsigned STYLE = 0,
    parameter int          CTX   = 8,
    parameter int          PLAN  = 32,
    parameter int          REGS  = 32
) (
    input  logic        clk,
    input  logic        we_i,
    input  logic [ 2:0] ctx_i,
    input  logic [ 4:0] pc_i,
    input  logic [ 7:0] op_i,
    input  logic [31:0] imm_i,
    output logic [59:0] uop_o
);

  // Derived exactly as production derives them, so the probe tracks the
  // relationship rather than a transcribed constant.
  localparam int RW = $clog2(REGS);       // 5 at the shipped 32
  localparam int CW = $clog2(CTX);        // 3 at the shipped 8
  localparam int PW = $clog2(PLAN);       // 5 at the shipped 32
  localparam int AW = CW + PW;            // 8 -> 256 entries
  localparam int UW = 8 + 4*RW + 32;      // 60

  // Quartus 17.0.2 rejects a bare module-scope elaboration check; it must sit
  // inside `initial begin ... end`. QUARTUS_GOTCHAS.md carries the case.
  initial begin
    if (STYLE > 2)
      $fatal(1, "zhao_probe_uopstore: STYLE=%0d is not 0..2", STYLE);
    if (UW != 60)
      $fatal(1, "zhao_probe_uopstore: UW=%0d but the probe's port is 60", UW);
  end

  typedef struct packed {
    logic [ 7:0]    op;
    logic [RW-1:0]  dst;
    logic [RW-1:0]  a;
    logic [RW-1:0]  b;
    logic [RW-1:0]  c;
    logic [31:0]    imm;
  } uop_t;

  // Explicit generate/endgenerate: Quartus 17.0.2 needs the keywords, while
  // both Verilator and slang accept the implicit form.
  generate
    if (STYLE == 0) begin : g_production_int_cast
      // EXACTLY the shipped addressing: a 32-bit SIGNED index expression.
      uop_t store[0:(CTX*PLAN)-1];
      uop_t rd_r;
      always_ff @(posedge clk) begin
        if (we_i)
          store[(int'(ctx_i) * PLAN) + int'(pc_i)] <=
              '{op: op_i, dst: '0, a: '0, b: '0, c: '0, imm: imm_i};
        rd_r <= store[(int'(ctx_i) * PLAN) + int'(pc_i)];
      end
      assign uop_o = rd_r;
    end else if (STYLE == 1) begin : g_narrow_unsigned
      // Same entries, same widths, same arithmetic -- narrow UNSIGNED address.
      uop_t store[0:(CTX*PLAN)-1];
      uop_t rd_r;
      wire [AW-1:0] addr_c = {ctx_i, pc_i};
      always_ff @(posedge clk) begin
        if (we_i)
          store[addr_c] <= '{op: op_i, dst: '0, a: '0, b: '0, c: '0, imm: imm_i};
        rd_r <= store[addr_c];
      end
      assign uop_o = rd_r;
    end else begin : g_control_plain
      // POSITIVE CONTROL -- plain packed vector array. MUST infer.
      logic [UW-1:0] store[0:(CTX*PLAN)-1];
      logic [UW-1:0] rd_r;
      wire [AW-1:0] addr_c = {ctx_i, pc_i};
      always_ff @(posedge clk) begin
        if (we_i) store[addr_c] <= {op_i, {(4*RW){1'b0}}, imm_i};
        rd_r <= store[addr_c];
      end
      assign uop_o = rd_r;
    end
  endgenerate

endmodule
