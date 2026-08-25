// zhao_probe_compose_pipe.sv — CHARACTERIZATION PROBE, not a console block.
//
// The owner directive of 2026-08-25 (`reports/PIPELINEINGHINTS`) proposes
// turning TERRAIN.PATCH's compose lane from a per-vertex WALK into an ordered
// PIPELINE:
//
//   today      vertex A: field0 -> field1 -> ... -> field15, then vertex B
//              => 1 + n clocks per vertex
//
//   proposed   stage i applies field i; different VERTICES occupy different
//              stages at once
//              => one composed vertex per clock after fill, latency ~n
//
// THE REFERENCE ALREADY PERMITS THIS, which is why it is safe rather than
// merely fast. `zref::terrain::compose_vertex` says so in as many words:
//
//     "compose_lattice iterates apps OUTER and vertices INNER ... this function
//      iterates lanes for ONE vertex. Those agree bit-for-bit because fx_add is
//      saturating and therefore order-dependent, but THE ORDER THAT MATTERS IS
//      THE ORDER OF ADDS AT A GIVEN VERTEX, and both forms apply the lanes to a
//      vertex in command order. Nothing about the transpose changes a single
//      add's operands."
//
// A pipeline is that transpose taken one step further: every vertex still sees
// its lanes in command order, one add per stage, in sequence. What changes is
// only that stage k is working on an older vertex than stage k-1.
//
// WHAT THIS FILE IS FOR. The directive asks to "characterize 4/8/16 stages and
// see what it costs", and notes these are add/compare/select logic rather than
// DSP farms. This measures that and nothing else: it implements the saturating
// add chain and the per-stage footprint skip, with no intake, no residency, no
// ledger plumbing. It is NOT the shipped compose lane.
//
// THE FOOTPRINT SKIP IS MODELLED, not omitted. `covers()` in the reference
// SKIPS a lane whose footprint misses the vertex rather than adding zero --
// "identical in value, and identical in SatLedger records too". A stage that
// could not skip would be a different circuit, so each stage carries a
// per-vertex enable.
module zhao_probe_compose_pipe #(
    parameter int STAGES = 16   // one per accepted field lane
) (
    input  logic        clk,
    input  logic        rst_n,

    input  logic               v_valid_i,
    input  logic signed [31:0] top_i,        // compose_top, already clamped
    // One value per stage, presented by that stage's result FIFO.
    input  logic signed [31:0] lane_i [STAGES],
    input  logic [STAGES-1:0]  covers_i,     // per-lane footprint test
    input  logic signed [31:0] bottom_i,
    input  logic               dual_i,

    output logic               v_valid_o,
    output logic signed [31:0] live_top_o,
    output logic               sat_o         // any stage saturated
);

  // fx_add: saturating 32-bit add. One rounding per result means none here --
  // the up-conversion is an exact shift and the add is the only operation.
  function automatic logic signed [31:0] fx_add(input logic signed [31:0] a,
                                                input logic signed [31:0] b,
                                                output logic fired);
    logic signed [32:0] s;
    begin
      s = {a[31], a} + {b[31], b};
      if (s > 33'sh0_7FFFFFFF)       begin fired = 1'b1; fx_add = 32'sh7FFF_FFFF; end
      else if (s < -33'sh0_80000000) begin fired = 1'b1; fx_add = 32'sh8000_0000; end
      else                           begin fired = 1'b0; fx_add = s[31:0];        end
    end
  endfunction

  // The travelling state: one accumulator per stage, plus the per-vertex
  // metadata that must arrive at the end WITH its own vertex rather than with
  // whatever vertex happens to be leaving.
  logic signed [31:0] acc   [STAGES+1];
  logic               vld   [STAGES+1];
  logic signed [31:0] bot   [STAGES+1];
  logic               dual  [STAGES+1];
  logic               sat   [STAGES+1];
  logic [STAGES-1:0]  cov   [STAGES+1];

  // LANE VALUES DO NOT TRAVEL, and the first version of this probe had them
  // doing so. Carrying `lane[STAGES]` through every stage is O(n^2) storage --
  // 17 x 16 x 32 = 8,704 bits at sixteen stages -- and Quartus duly turned it
  // into THIRTEEN M10Ks, which measured my testbench rather than the design.
  //
  // The directive already says how the real thing is fed: "small per-field
  // result FIFOs/batch RAM so Field itself can evaluate samples in whatever
  // order maximizes throughput". Stage i pops FIFO i. The value for the vertex
  // currently in stage i arrives at stage i; nothing is carried.

  always_comb begin
    acc[0]  = top_i;
    vld[0]  = v_valid_i;
    bot[0]  = bottom_i;
    dual[0] = dual_i;
    sat[0]  = 1'b0;
    cov[0]  = covers_i;
  end

  genvar s;
  generate
    for (s = 0; s < STAGES; s++) begin : g_stage
      // The add is COMBINATIONAL and the register takes its result: a blocking
      // assignment inside always_ff would describe the same hardware and read
      // like a race, which is the sort of thing that survives review and then
      // teaches somebody the wrong lesson.
      logic signed [31:0] sum;
      logic               fired;
      always_comb sum = fx_add(acc[s], lane_i[s], fired);

      always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
          acc[s+1]  <= '0;
          vld[s+1]  <= 1'b0;
          bot[s+1]  <= '0;
          dual[s+1] <= 1'b0;
          sat[s+1]  <= 1'b0;
          cov[s+1]  <= '0;
        end else begin
          // command order preserved: stage s applies lane s, and only lane s.
          acc[s+1]  <= cov[s][s] ? sum : acc[s];          // skip, not add-zero
          sat[s+1]  <= sat[s] || (cov[s][s] && fired);
          vld[s+1]  <= vld[s];
          bot[s+1]  <= bot[s];
          dual[s+1] <= dual[s];
          cov[s+1]  <= cov[s];
        end
      end
    end
  endgenerate

  // The ONE clamp after the chain (§3.4): a transient wave can never punch
  // below the underside, so it can never fake a breach.
  assign live_top_o = (dual[STAGES] && acc[STAGES] < bot[STAGES]) ? bot[STAGES] : acc[STAGES];
  assign v_valid_o  = vld[STAGES];
  assign sat_o      = sat[STAGES];

endmodule : zhao_probe_compose_pipe
