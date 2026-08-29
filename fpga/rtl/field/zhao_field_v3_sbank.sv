// zhao_field_v3_sbank.sv — THE UNIFORM (SCALAR) BANK.
//
// ENFORCED-BY: tests/differential/field_v3_sbank_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHAT THIS IS FOR
// ---------------------------------------------------------------------------
// Some Field values are the same for every point in an association. The
// prepared ring is the clearest case: its midpoint and its two smoothstep
// reciprocals depend only on the two radii, so computing them per point would
// repeat a reciprocal 1,089 times for one answer.
//
// The reference already models this. `zfield::plan` splits a program into a
// PREP block of uniform instructions and a vector body, and
// `spec/form/cost-model.md` is explicit that `uniform_ops` are "executed ONCE
// per association ON THE ARM". So the preparation is host work and this block
// is only the storage: the host writes the answers in, and the services read
// them out.
//
// That makes this the same shape as the curve service's table cache, which is
// already built and closed at 29/29. Following a proven pattern rather than
// inventing one is the point.
//
// ---------------------------------------------------------------------------
// WHY SIXTY-FOUR, AND WHY THAT NUMBER IS NOT A GUESS
// ---------------------------------------------------------------------------
// `vreg_hwm` is capped at 32 by the cost model. `sreg_hwm` is not capped
// anywhere, so the depth had to be DERIVED. `tools/field/measure_sreg_hwm.cpp`
// plans every program in the corpus under six varying masks; that probe is
// committed so the number can be rechecked rather than trusted.
//
//     WORST sreg_hwm = 41      impact_wave, mask 0 (everything uniform)
//     Earth mask               crater_ring 29, impact_wave 23, wave_pool 19
//     all inputs varying       6 - 8
//
// THE MASK SWEEP IS THE FINDING. The bank is worst when EVERYTHING is uniform,
// which is the opposite of the natural guess that a busier program needs more
// scalars: a value that varies lives in a vector register, and a value that
// does not becomes a scalar slot. Measuring only the Earth mask reports 29 and
// undersizes this bank by 40%.
//
// 64 is 1.5x the observed worst and a power of two, so the index is 6 bits.
// That matters beyond tidiness: the prepared ring needs FOUR slot indices in
// one instruction, and 4 x 6 = 24 bits fits inside the existing 32-bit
// immediate with 8 to spare, so the instruction word does not have to grow.
// At 128 slots it would have.
//
// ---------------------------------------------------------------------------
// OUT OF RANGE REFUSES, IT DOES NOT WRAP
// ---------------------------------------------------------------------------
// A program needing more than 64 uniform slots is not admissible on the hot
// path, and this block says so out loud rather than folding the index. That is
// the same law the opcode table already follows -- a width of zero means
// REFUSE -- and it is chosen for the same reason: a refusal fails loudly and
// visibly, where a wrapped index silently reads somebody else's number and
// produces an answer that is individually plausible and completely wrong.
//
// The address is 6 bits wide, so wrapping is not expressible on the read side.
// The guard that matters is on the WRITE side, where the host supplies a slot
// number that may legitimately exceed the bank.
//
// ---------------------------------------------------------------------------
// ONE READ PORT, ON PURPOSE
// ---------------------------------------------------------------------------
// The prepared ring wants four scalars. Four read ports would mean four copies
// of the storage, because a 64x32 array has no four-port form on this fabric.
// Instead the reader walks the four slots over four cycles and holds them.
//
// Four cycles is paid ONCE PER GROUP against a ring that already spends nine
// multiplier slots per group, so it is not on the critical path. This is the
// same trade the curve service makes when it spends twelve cycles searching a
// table before it computes anything.
module zhao_field_v3_sbank #(
    parameter int SLOTS = 64,
    parameter int AW    = 6
) (
    input var logic clk,
    input var logic rst_n,

    // ---- the host's write port ---------------------------------------------
    // The ARM runs the prep block and writes the answers here, once per
    // association. `we_bad_o` latches if it ever names a slot this bank does
    // not have.
    input  var logic                  we_i,
    input  var logic [15:0]           waddr_i,   // WIDE on purpose -- see below
    input  var logic signed [31:0]    wdata_i,
    output var logic                  we_bad_o,

    // ---- one registered read port ------------------------------------------
    input  var logic [AW-1:0]         raddr_i,
    output var logic signed [31:0]    rdata_o
);

  // THE WRITE ADDRESS IS SIXTEEN BITS AND THE READ ADDRESS IS SIX.
  //
  // That asymmetry is deliberate and it is the whole out-of-range guard. The
  // planner's slot numbers are uint16_t, so a program CAN name slot 4,000; the
  // host hands over exactly what the plan said. Narrowing the port to 6 bits
  // would make the overflow unrepresentable at the boundary -- the wrap would
  // happen in the wiring, silently, before this block ever saw it.
  //
  // Taking the full width and refusing here is the only place the fault can be
  // both detected and reported.
  logic signed [31:0] mem [0:SLOTS-1];

  logic addr_ok_c;
  assign addr_ok_c = (waddr_i < 16'(SLOTS));

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      we_bad_o <= 1'b0;
    end else if (we_i && !addr_ok_c) begin
      // LATCHED, not a pulse. A single bad write is the entire finding, and a
      // level would be missed by any test that samples.
      we_bad_o <= 1'b1;
    end
  end

  always_ff @(posedge clk) begin
    // The refused write does not land. A program that overflows the bank gets
    // no answer rather than a wrong one.
    if (we_i && addr_ok_c) mem[waddr_i[AW-1:0]] <= wdata_i;
  end

  // Registered read, matching the table cache: the datum for the address
  // presented on cycle T arrives on T+1. Every consumer of this block has to
  // account for that one cycle, and the neighbour phase in the curve service
  // is the standing example of what happens when it does not -- the phase
  // declared itself finished on the cycle its last read was still arriving,
  // and handed the arithmetic a value that had not been written yet.
  always_ff @(posedge clk) begin
    rdata_o <= mem[raddr_i];
  end

endmodule : zhao_field_v3_sbank
