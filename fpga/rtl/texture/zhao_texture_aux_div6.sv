// zhao_texture_aux_div6.sv — the AUX divide as a six-stage pipeline, II=1.
//
// STANDALONE AND VERIFIED FIRST, then swapped into `zhao_texture_aux.sv`.
// Nothing instantiates it yet. The AUX block's FSM, sheet interface and miss
// handling are untouched by this file; only the divide changes shape.
//
// ---------------------------------------------------------------------------
// WHY
// ---------------------------------------------------------------------------
// reports/Addendum (owner, 2026-09-02), endorsing the rearchitecture brief:
//
//   > AUX becoming a literal six-stage restoring-divider pipeline is almost
//   > comically appropriate: the current thing already does six quotient bits;
//   > instead of doing them serially for one request, put one bit in each stage
//   > and have six requests walking through simultaneously. Same arithmetic,
//   > II=1.
//
// Today `zhao_texture_aux.sv` spends TWO states on the divide -- ST_DIV0 does
// bits 5,4,3 and ST_DIV1 does bits 2,1,0 -- so one request occupies the divider
// for two clocks and nothing else can use it. Worse for timing: three restoring
// steps are chained COMBINATIONALLY inside each state, so the critical path
// carries three dependent compare-and-subtract stages back to back.
//
// One step per stage fixes both at once. Six requests are in flight instead of
// one, and the longest combinational run drops from three chained steps to one.
// That is the shape this pass kept finding in the raster: latency may grow, and
// the initiation rate and the arithmetic may not regress.
//
// ---------------------------------------------------------------------------
// THE ARITHMETIC IS COPIED, NOT REDERIVED
// ---------------------------------------------------------------------------
// Bit-for-bit from `zhao_texture_aux.sv`'s `div_bit`/`div_sub`:
//
//     shifted = {7'b0, d} << k
//     bit     = (r >= shifted)
//     r'      = (r >= shifted) ? (r - shifted) : r
//
// for k = 5,4,3,2,1,0. `k` is a constant per stage, so `d << k` stays a wiring
// change rather than a barrel shifter -- the same reason the original gives.
//
// The caller still owns the two clamps the original documents: N < 0 answers 0,
// and N >= 64*D answers 63. Inside this pipeline the quotient is in [0,63] by
// construction, which is what makes six steps sufficient.
// ENFORCED-BY: fpga/rtl/texture/zhao_texture_aux_pipe.sv
// ENFORCED-BY: tests/texture/texture_aux_div6_directed.cpp Moving those clamps
// in here would change where a decision lives without changing the answer, so
// they stay where they are.
// ---------------------------------------------------------------------------
`default_nettype none

module zhao_texture_aux_div6 #(
    parameter int unsigned REM_W = 39,
    parameter int unsigned DEN_W = 32,
    parameter int unsigned TAGW  = 8
) (
    input var logic clk,
    input var logic rst_n,

    // No ready. A fixed-latency pipeline with no stalls cannot refuse, and
    // giving it a ready it never lowers would invite a caller to wait on it.
    input  var logic                 in_valid_i,
    input  var logic [REM_W-1:0]     in_ru_i,     // u numerator, pre-clamped
    input  var logic [DEN_W-1:0]     in_du_i,
    input  var logic [REM_W-1:0]     in_rv_i,
    input  var logic [DEN_W-1:0]     in_dv_i,
    input  var logic [TAGW-1:0]      in_tag_i,

    output var logic                 out_valid_o,
    output var logic [5:0]           out_qu_o,
    output var logic [5:0]           out_qv_o,
    output var logic [TAGW-1:0]      out_tag_o,

    // Evidence: how many requests walked through, and the high-water mark of
    // simultaneous occupancy. A pipeline that is never more than one-deep is
    // an expensive way to be serial, and this is how that shows up.
    output var logic [31:0]          issued_o,
    output var logic [3:0]           occupancy_o
);

  // Stage 0 consumes k=5 ... stage 5 consumes k=0. Six stages, latency 6.
  localparam int unsigned NSTAGE = 6;

  logic             v_q   [NSTAGE];
  logic [REM_W-1:0] ru_q  [NSTAGE];
  logic [REM_W-1:0] rv_q  [NSTAGE];
  logic [DEN_W-1:0] du_q  [NSTAGE];
  logic [DEN_W-1:0] dv_q  [NSTAGE];
  logic [5:0]       qu_q  [NSTAGE];
  logic [5:0]       qv_q  [NSTAGE];
  logic [TAGW-1:0]  tag_q [NSTAGE];

  // The single restoring step, identical to the original's two functions fused.
  function automatic logic [REM_W:0] step(input logic [REM_W-1:0] r,
                                          input logic [DEN_W-1:0] d,
                                          input int unsigned k);
    logic [REM_W-1:0] shifted;
    logic             hit;
    begin
      shifted = {{(REM_W - DEN_W){1'b0}}, d} << k;
      hit     = (r >= shifted);
      // {quotient bit, remainder}
      step    = {hit, (hit ? (r - shifted) : r)};
    end
  endfunction

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      for (int i = 0; i < NSTAGE; i++) begin
        v_q[i]  <= 1'b0;
        qu_q[i] <= 6'd0;
        qv_q[i] <= 6'd0;
      end
      issued_o <= 32'd0;
    end else begin
      // ---- stage 0 takes k = 5 -------------------------------------------
      begin
        automatic logic [REM_W:0] su = step(in_ru_i, in_du_i, 5);
        automatic logic [REM_W:0] sv = step(in_rv_i, in_dv_i, 5);
        v_q[0]   <= in_valid_i;
        ru_q[0]  <= su[REM_W-1:0];
        rv_q[0]  <= sv[REM_W-1:0];
        du_q[0]  <= in_du_i;
        dv_q[0]  <= in_dv_i;
        qu_q[0]  <= {su[REM_W], 5'd0};
        qv_q[0]  <= {sv[REM_W], 5'd0};
        tag_q[0] <= in_tag_i;
        if (in_valid_i) issued_o <= issued_o + 32'd1;
      end

      // ---- stages 1..5 take k = 4,3,2,1,0 --------------------------------
      for (int i = 1; i < NSTAGE; i++) begin
        automatic int unsigned k = NSTAGE - 1 - i;   // 4,3,2,1,0
        automatic logic [REM_W:0] su = step(ru_q[i - 1], du_q[i - 1], k);
        automatic logic [REM_W:0] sv = step(rv_q[i - 1], dv_q[i - 1], k);
        v_q[i]   <= v_q[i - 1];
        ru_q[i]  <= su[REM_W-1:0];
        rv_q[i]  <= sv[REM_W-1:0];
        du_q[i]  <= du_q[i - 1];
        dv_q[i]  <= dv_q[i - 1];
        qu_q[i]  <= qu_q[i - 1] | (6'(su[REM_W]) << k);
        qv_q[i]  <= qv_q[i - 1] | (6'(sv[REM_W]) << k);
        tag_q[i] <= tag_q[i - 1];
      end
    end
  end

  assign out_valid_o = v_q[NSTAGE-1];
  assign out_qu_o    = qu_q[NSTAGE-1];
  assign out_qv_o    = qv_q[NSTAGE-1];
  assign out_tag_o   = tag_q[NSTAGE-1];

  always_comb begin
    occupancy_o = 4'd0;
    for (int i = 0; i < NSTAGE; i++) occupancy_o = occupancy_o + 4'(v_q[i]);
  end

endmodule : zhao_texture_aux_div6

`default_nettype wire
