// sat_add_harness.sv — formal harness for tests/formal/sat_add.sby.
// Testbench component, NEVER synthesis or the Verilator ctests.
//
// THE LAW UNDER PROOF is spec/counters.md §4: the D9 counters saturate, they
// never wrap. Five sites implement it (CMD.DMA's sat_add, INPUT.SNAPSHOT's gap
// accumulator, two arms of HPS.BRIDGE, one of VRAM.ARBITER) and all five now
// call one definition, `zhao_pkg::zhao_sat_add{64,32}`.
//
// WHY FORMAL, AND NOT ANOTHER DIFFERENTIAL. The saturating arm is UNREACHABLE
// IN SIMULATION. These are u64 counters incremented by small amounts, so the
// rail is ~2^64 events away, and nothing external can preload them. Measured,
// not supposed: breaking the headroom test outright -- `~b` replaced by plain
// `b` -- passes cmd_dma_directed AND its 5,000-packet random lane. A test that
// cannot fail is not evidence, and five hand-written copies of a law no test
// can reach is how a design acquires a defect nobody can find.
//
// So the arm that simulation cannot reach is proven instead, over EVERY input
// pair at once rather than over the pairs somebody thought to write down.
//
// THE ORACLE IS THE EXACT SUM, NOT A SECOND COPY OF THE CODE. `wide` is the
// (W+1)-bit sum, which cannot overflow and is therefore the true arithmetic
// answer; the properties say the W-bit result equals that answer CLAMPED. That
// is the specification, independent of how the module spells it -- the
// distinction the ABS defect in zhao_field_alu is this project's standing
// reminder about, where a test restated the law and agreed with a wrong
// implementation forever.
`default_nettype none

module zhao_sat_add_harness (
    input wire [63:0] a64_i,
    input wire [63:0] b64_i,
    input wire [31:0] a32_i,
    input wire [31:0] b32_i
);

  localparam logic [63:0] MAX64 = 64'hFFFF_FFFF_FFFF_FFFF;
  localparam logic [31:0] MAX32 = 32'hFFFF_FFFF;

  logic [63:0] s64;
  logic [31:0] s32;
  always_comb s64 = zhao_pkg::zhao_sat_add64(a64_i, b64_i);
  always_comb s32 = zhao_pkg::zhao_sat_add32(a32_i, b32_i);

  // the exact answers, one bit wider so they cannot overflow
  logic [64:0] wide64;
  logic [32:0] wide32;
  always_comb wide64 = {1'b0, a64_i} + {1'b0, b64_i};
  always_comb wide32 = {1'b0, a32_i} + {1'b0, b32_i};

`ifdef FORMAL
  always_comb begin
    // ---- the law itself: the exact sum, clamped ---------------------------
    a_sat64_is_clamped_sum : assert (s64 == ((wide64 > {1'b0, MAX64})
                                             ? MAX64 : wide64[63:0]));
    a_sat32_is_clamped_sum : assert (s32 == ((wide32 > {1'b0, MAX32})
                                             ? MAX32 : wide32[31:0]));

    // ---- NEVER WRAP, stated separately ------------------------------------
    // This is the property the counters exist for and the one a wrapped
    // implementation violates most visibly: a monitoring counter that goes
    // BACKWARDS reports a drop rate lower than the truth, which is worse than
    // one that sticks. It follows from the clamp law above, and it is asserted
    // on its own anyway because it is what a reader of spec/counters.md §4 is
    // actually promised.
    a_sat64_never_wraps : assert (s64 >= a64_i && s64 >= b64_i);
    a_sat32_never_wraps : assert (s32 >= a32_i && s32 >= b32_i);

    // ---- adding nothing changes nothing -----------------------------------
    a_sat64_identity : assert ((b64_i != 64'd0) || (s64 == a64_i));
    a_sat32_identity : assert ((b32_i != 32'd0) || (s32 == a32_i));

    // ---- the rail is absorbing --------------------------------------------
    // Once a counter has saturated it must STAY saturated. INPUT.SNAPSHOT used
    // to spend a second full-width compare on this case; dropping that guard
    // is only sound because the add itself has the property.
    a_sat64_rail_absorbs : assert ((a64_i != MAX64) || (s64 == MAX64));
    a_sat32_rail_absorbs : assert ((a32_i != MAX32) || (s32 == MAX32));
  end

  // ---- non-vacuity (V16): both arms are REACHABLE -------------------------
  // Every property above is either an implication or an equality that a
  // never-saturating adder satisfies for small inputs. These demand the solver
  // actually visit the arm no simulation can: the rail, the exact boundary
  // where a + b is one short of it, and the ordinary carry-free case.
  always_comb begin
    c_saturates      : cover (s64 == MAX64 && a64_i != MAX64 && b64_i != 64'd0);
    c_exact_boundary : cover (wide64 == {1'b0, MAX64} && a64_i != 64'd0);
    c_one_past       : cover (wide64 == {1'b0, MAX64} + 65'd1);
    c_ordinary       : cover (s64 == wide64[63:0] && a64_i != 64'd0 && b64_i != 64'd0);
    c_sat32          : cover (s32 == MAX32 && a32_i != MAX32 && b32_i != 32'd0);
  end
`endif

endmodule

`default_nettype wire
