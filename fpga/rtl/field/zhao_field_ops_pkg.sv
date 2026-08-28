// zhao_field_ops_pkg.sv — WHICH FIELD OPS LEAVE THE PIPE, AND HOW WIDE THEY
// COME BACK. One table, because two copies of it deadlocked the machine.
//
// ENFORCED-BY: tests/differential/field_v3_full_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHY THIS FILE EXISTS
// ---------------------------------------------------------------------------
// The executor decided which opcodes to hand to the service path, and the
// dispatcher decided which opcodes it would accept, and they each kept their
// own list.
//
//     zhao_probe_v3_exec.sv   is_long()       routed TEN opcodes
//     zhao_field_v3_dispatch  dst_width_of()  knew EIGHT
//
// The two it did not know were SPLINE (0x1B) and RING (0x21), and a width of
// zero means REFUSE -- correct there, because a wrong width writes the wrong
// number of registers. So the executor handed over an instruction the
// dispatcher would never take, and the context PARKED FOREVER. Nothing timed
// out. Neither block was wrong on its own, which is why nine mutation sweeps
// and two closed compositions never saw it: it is only a defect in the PAIR.
//
// That is the fourth seam defect in this engine, and all four have the same
// shape -- two places that must agree, and no mechanism forcing them to.
// Patching the two lists to match would have fixed today's deadlock and left
// tomorrow's, because the next op added has to be remembered in both.
//
// ---------------------------------------------------------------------------
// WHAT "WIDTH" MEANS, SINCE ONE NUMBER NOW DOES TWO JOBS
// ---------------------------------------------------------------------------
// `field_long_width(op)` is the number of registers the op writes back per
// point, and zero means "not a long op". Both readers derive from it:
//
//     is_long(op)       := field_long_width(op) != 0
//     dst_width_of(op)  := field_long_width(op)
//
// So an op cannot be offered by one and refused by the other. Adding an op is
// ONE edit here; forgetting the other place is no longer possible.
//
// ---------------------------------------------------------------------------
// WHAT THIS DOES **NOT** DO
// ---------------------------------------------------------------------------
// It does not make SPLINE work. SPLINE and RING are absent from this table, so
// the executor no longer offers them and the ALU reports them through
// `unsupported_o` instead -- a LOUD refusal rather than a silent hang. Whether
// they should execute on the v3 engine at all is a separate question with a
// recorded answer: Fieldv3.md section 6 puts spline on the COLD service lane,
// and `zhao_field_curve.sv` already implements the whole op, lookup included,
// one point at a time.
//
// RING is the same shape with the opposite answer. The brief costs the
// PREPARED ring (`UOP_RING_PREP`, 0xF1) as its HOT path and leaves the
// varying-radius `OP_RING` (0x21) cold -- so 0xF1 is what eventually belongs
// in this table, once a ring service exists to answer it. Today the service
// path has one service, the noise unit, and `wrong_op_o` is the wire that says
// so.
//
// When that decision lands it is one line in this file rather than two edits
// that can fall out of step, which is the entire point.
package zhao_field_ops_pkg;

  // The canonical Field IR opcodes that leave the executor. Values are frozen
  // in design/ops.yml and reference/include/zfield/zfield.hpp; they are
  // repeated here rather than imported because the ABI package is generated
  // from spec/commands.zidl and does not carry them.
  //
  // A GENERATED VERSION WOULD BE BETTER and is deliberately not done yet:
  // design/ops.yml has `field_ir_opcode` but no destination widths, so it
  // would need a new field, a generator change and a ledger update. That is
  // worth doing on its own and not worth blocking a deadlock fix on.
  localparam logic [7:0] OP_NORMALIZE2 = 8'h15;
  localparam logic [7:0] OP_NORMALIZE3 = 8'h16;
  localparam logic [7:0] OP_CURVE      = 8'h1A;
  localparam logic [7:0] OP_NOISE2     = 8'h1C;
  localparam logic [7:0] OP_DCURVE     = 8'h1D;
  localparam logic [7:0] OP_RIDGE      = 8'h22;
  localparam logic [7:0] OP_ROT2       = 8'h28;
  localparam logic [7:0] OP_ROT3       = 8'h29;

  // Registers written back per point. ZERO MEANS NOT A LONG OP, and it is the
  // safe default on purpose: an opcode nobody has classified is refused rather
  // than guessed at, because a wrong width corrupts a register while a refusal
  // merely fails.
  function automatic logic [1:0] field_long_width(input logic [7:0] op);
    case (op)
      OP_CURVE, OP_DCURVE, OP_RIDGE:      field_long_width = 2'd1;
      OP_NOISE2, OP_ROT2, OP_NORMALIZE2:  field_long_width = 2'd2;
      OP_ROT3, OP_NORMALIZE3:             field_long_width = 2'd3;
      default:                            field_long_width = 2'd0;
    endcase
  endfunction

  // Sugar for the executor, so neither block spells the comparison itself.
  function automatic logic field_is_long(input logic [7:0] op);
    field_is_long = (field_long_width(op) != 2'd0);
  endfunction

endpackage : zhao_field_ops_pkg
