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
// THE CAUSE, FOUND -- AND QUARTUS NAMED IT ITSELF
// ---------------------------------------------------------------------------
// The console map carries the answer in one line:
//
//   Info (276007): RAM logic "...zhao_field_v3_exec:u_exec|store" is
//   uninferred due to ASYNCHRONOUS READ LOGIC
//
// My first hypothesis -- that the 32-bit SIGNED int' index expression was the
// blocker -- IS REFUTED by this probe: STYLE=0 reproduces the shipped
// addressing exactly and infers a Simple Dual Port 256-deep memory. So neither
// the typedef nor the cast prevents inference.
//
// THE REAL BLOCKER IS A COMBINATIONAL LOOP THROUGH THE READ ENABLE. Three lines
// of zhao_field_v3_exec.sv:
//
//   :378   issue_c = |ready_c && !dot_inflight_c && !hold_c && ...
//   :429   assign dot_inflight_c = (s1_v_r && is_dot(s1_uop_r.op)) || ...
//   :1200  if (issue_c) s1_uop_r <= store[...];
//
// s1_uop_r is the store's read-data register. Its .op field feeds
// dot_inflight_c, which gates issue_c, which is the store's READ ENABLE. An
// M10K read port cannot be enabled by a term derived from the value it is about
// to deliver, so the array stays in flip-flops.
//
// THIS IS ARCHITECTURAL, NOT COSMETIC. The issue decision depends on the opcode
// of the instruction currently in S1. Banking the store means breaking that
// loop -- decoding the hazard bit from a narrow side-table indexed by the same
// address so it is available without reading the wide store, or accepting
// another stage before the hazard check. Either is a schedule change with a
// latency consequence, which is precisely R4's point that state reorganisation
// must be priced inside a schedule rather than counted from declarations.
//
//   STYLE=0  shipped addressing, int' casts            -> INFERS (hypothesis refuted)
//   STYLE=1  narrow unsigned {ctx,pc} address          -> INFERS
//   STYLE=2  plain packed-vector array, POSITIVE CONTROL -> MUST infer
//   STYLE=3  READ ENABLE DERIVED FROM READ DATA        -> MUST **NOT** infer
//
// STYLE=3 is the control for the DIAGNOSIS rather than for the flow: if it
// infers, the explanation above is wrong. Read STYLE=2 first, then STYLE=3.
//
// WHAT THIS PROBE DOES NOT CLAIM. Not that the repair is free -- see the
// architectural note above. Not 15,360/4 ALM of saving: bits/4 is capacity
// arithmetic, not an integrated area measurement, which is the error R4
// corrected in R3. And the probe's own first version was UNFAITHFUL: writing
// dst/a/b/c as constants let Quartus fold 20 of the 60 bits away and report
// width=40, so the stored fields are now all data-dependent.

module zhao_probe_uopstore #(
    parameter int unsigned STYLE = 0,
    parameter int          CTX   = 8,
    parameter int          PLAN  = 32,
    parameter int          REGS  = 32
) (
    input  logic        clk,
    input  logic        rst_n,     // STYLE=4 only
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
    if (STYLE > 4)
      $fatal(1, "zhao_probe_uopstore: STYLE=%0d is not 0..4", STYLE);
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
    if (STYLE != 4) begin : g_tie_rst
      // rst_n exists in every style so the PORT LIST is identical across
      // the maps; differing I/O would make the rows incomparable.
      wire unused_rst = &{1'b0, rst_n};
    end
  endgenerate

  generate
    if (STYLE == 0) begin : g_production_int_cast
      // EXACTLY the shipped addressing: a 32-bit SIGNED index expression.
      uop_t store[0:(CTX*PLAN)-1];
      uop_t rd_r;
      always_ff @(posedge clk) begin
        if (we_i)
          store[(int'(ctx_i) * PLAN) + int'(pc_i)] <=
              '{op: op_i, dst: op_i[4:0], a: pc_i, b: RW'(ctx_i), c: op_i[7:3], imm: imm_i};
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
          store[addr_c] <= '{op: op_i, dst: op_i[4:0], a: pc_i, b: RW'(ctx_i), c: op_i[7:3], imm: imm_i};
        rd_r <= store[addr_c];
      end
      assign uop_o = rd_r;
    end else if (STYLE == 3) begin : g_enable_feedback
      // THE DIAGNOSIS, REPRODUCED. This is the production structure:
      //
      //   :378   issue_c = |ready_c && !dot_inflight_c && ...
      //   :429   assign dot_inflight_c = (s1_v_r && is_dot(s1_uop_r.op)) || ...
      //   :1200  if (issue_c) s1_uop_r <= store[...];
      //
      // The array's READ ENABLE depends combinationally on the array's own
      // READ DATA. An M10K's read port cannot be enabled by a term derived
      // from the value it is about to deliver, so Quartus reports
      //   Info (276007): RAM logic "...|store" is uninferred due to
      //   asynchronous read logic
      // THIS STYLE MUST FAIL TO INFER. It is the positive control for the
      // blocker itself: if it infers, the diagnosis is wrong.
      uop_t store[0:(CTX*PLAN)-1];
      uop_t rd_r;
      wire [AW-1:0] addr_c = {ctx_i, pc_i};
      // the feedback: enable derived from the previously-read opcode
      wire hazard_c = (rd_r.op[7] == 1'b1);
      wire issue_c  = we_i | ~hazard_c;
      always_ff @(posedge clk) begin
        if (we_i)
          store[addr_c] <= '{op: op_i, dst: op_i[4:0], a: pc_i, b: RW'(ctx_i),
                             c: op_i[7:3], imm: imm_i};
        if (issue_c) rd_r <= store[addr_c];
      end
      assign uop_o = rd_r;
    end else if (STYLE == 4) begin : g_async_reset_process
      // THIRD HYPOTHESIS, and it is THIS REPOSITORY'S OWN DOCUMENTED CAUSE.
      // check_ram_inference.py flags arrays "written from an ASYNC-RESET
      // process" as a RAM-inference hazard, with measured false positives. The
      // production always_ff is `if (!rst_n) <clear many things> else <write
      // store>`; `store` itself is never cleared, but it lives inside an
      // asynchronously reset process. This probe's other styles have NO reset
      // at all, which is why they infer -- so the reset is the one structural
      // difference left untested.
      //
      // MUST NOT INFER if the hypothesis is right.
      uop_t store[0:(CTX*PLAN)-1];
      uop_t rd_r;
      logic [7:0] other_r;
      wire [AW-1:0] addr_c = {ctx_i, pc_i};
      always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
          other_r <= 8'd0;
          rd_r    <= '0;
        end else begin
          other_r <= op_i;
          if (we_i)
            store[addr_c] <= '{op: op_i, dst: op_i[4:0], a: pc_i,
                               b: RW'(ctx_i), c: op_i[7:3], imm: imm_i};
          rd_r <= store[addr_c];
        end
      end
      assign uop_o = rd_r ^ {52'd0, other_r};
    end else begin : g_control_plain
      // POSITIVE CONTROL -- plain packed vector array. MUST infer.
      logic [UW-1:0] store[0:(CTX*PLAN)-1];
      logic [UW-1:0] rd_r;
      wire [AW-1:0] addr_c = {ctx_i, pc_i};
      always_ff @(posedge clk) begin
        if (we_i) store[addr_c] <= {op_i, op_i[4:0], pc_i, RW'(ctx_i), op_i[7:3], imm_i};
        rd_r <= store[addr_c];
      end
      assign uop_o = rd_r;
    end
  endgenerate

endmodule
