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
//     zhao_field_v3_exec.sv   is_long()       routed TEN opcodes
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
// SPLINE JOINED THIS TABLE ON 2026-08-29, and the ORDER it joined in is the
// point. Fieldv3.md section 6 had put spline on the cold lane; the owner chose
// the hot path instead ("spend the work", 2026-08-28), so 0x1B belongs here --
// but ONLY once something can answer it.
//
// Adding the opcode is what makes the executor OFFER the op. Adding it before
// a service existed would have rebuilt, exactly, the deadlock this file was
// written to prevent: an instruction offered by one block and answerable by
// nobody, parked forever, with nothing timing out. So the service was wired
// onto the path FIRST and the table entry is the LAST step, not the first.
//
// Its width is 1, and that is READ FROM THE ORACLE rather than reasoned about:
// zfield_decode.cpp gives OP_SPLINE the same shape as OP_CURVE and OP_DCURVE,
// m = {1, {1,0,0}, 1, 3}, whose leading field is the destination width.
//
// UOP_RING_PREP JOINED ON 2026-08-29, and in the same ORDER as SPLINE did: the
// service was wired onto the path FIRST and this entry is the LAST step. The
// entry is what makes the executor OFFER the op; adding it before something
// could answer would rebuild exactly the park-forever deadlock this file was
// written to prevent.
//
// Its width is 1, read from zfield_plan.cpp rather than reasoned about --
// `(v.op == UOP_RING_PREP) ? 1 : optable::shape_of(v.op)->dst_width`, because
// a synthetic uop has no entry in the canonical shape table at all.
//
// The varying-radius OP_RING (0x21) is still absent. RE-ASKED 2026-09-20 and
// THE STATED CAUSE STILL HOLDS, verified rather than inherited: per point it
// needs `m = ring_mid(r0,r1)` and then TWO reciprocals, where the prepared form
// has all of that computed ONCE -- and computed in SOFTWARE, by
// `zfield::prepare()`, then loaded into the scalar bank as uniforms
// (reference/src/zfield/zfield_plan.cpp:137-151 emits `PrepUop{OP_RCP, ...}`
// twice). `zhao_field_v3_ring.sv` opens with "nine separately-rounded products
// on the shared bank, and NO reciprocal" and states the deferral deliberately
// at its lines 32-37: a per-point reciprocal is a second shared resource with
// its own arbitration, refusal and starvation questions.
//
// CANONICAL OP_RCP (0x17) IS ABSENT FROM THIS TABLE FOR THE SAME REASON, and
// the two are one piece of work. 0x17 is a canonical Field opcode -- it is in
// `reference/include/zfield/zfield.hpp:74`, its oracle shape is
// `{1, {1,0,0}, 1, 0}`, IDENTICAL to OP_SIN and OP_COS which ARE in the table
// below, and `zhao_field_alu.sv:34` says RCP is deliberately not in the ALU, so
// it can only be a long op. The exact leaf exists and is differentially tested:
// `zhao_field_rcp.sv`, which implements `zref::field_rcp` -- NOT the 24-bit
// two-step `zhao_field_rcp24_rom` used inside normalize, and NOT the raster or
// projector reciprocal, whose widths merely look similar.
//
// SO THE ENTRY IS THE LAST STEP, NOT THE FIRST, exactly as SPLINE and
// UOP_RING_PREP were. Adding an opcode here is what makes the executor OFFER
// it; adding it before something can answer rebuilds the park-forever deadlock
// this file was written to prevent. The owner directive of 2026-09-20 states
// the same rule at its line 2269: the support table may advertise an operation
// only when its request, service, result AND STATUS all work.
//
// CORRECTED 2026-09-20: this header used to say "The service path now has TWO
// services, the noise unit and the curve service." IT HAS SEVEN -- noise,
// curve, normalize, rot, ring, trig and len, all instantiated in
// `zhao_field_v3_svcpath.sv`. The fourteen non-zero widths in the table below
// and those seven services' opcode sets agree exactly, so every advertised
// opcode does have a live request/service/result path today. The stale sentence
// is worth more as a correction than as a deletion: it was true when written,
// nobody re-asked, and it would have told the next reader that a route they
// needed did not exist.
//
// `wrong_op_o` is still the wire that says an op reached no service -- but note
// it cannot presently fire: `svc_ready` is 1'b0 in exactly the default case, so
// an unroutable op can never complete the handshake the detector watches. It is
// a backstop for a future service with an independently driven ready, and its
// silence is not evidence about op routing.
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
  localparam logic [7:0] OP_LEN2       = 8'h12;
  localparam logic [7:0] OP_LEN3       = 8'h13;
  localparam logic [7:0] OP_DIST2      = 8'h14;
  localparam logic [7:0] OP_NORMALIZE2 = 8'h15;
  localparam logic [7:0] OP_NORMALIZE3 = 8'h16;
  localparam logic [7:0] OP_SIN        = 8'h18;
  localparam logic [7:0] OP_COS        = 8'h19;
  localparam logic [7:0] OP_CURVE      = 8'h1A;
  localparam logic [7:0] OP_SPLINE     = 8'h1B;
  localparam logic [7:0] OP_NOISE2     = 8'h1C;
  localparam logic [7:0] OP_DCURVE     = 8'h1D;
  localparam logic [7:0] OP_RIDGE      = 8'h22;
  localparam logic [7:0] OP_ROT2       = 8'h28;
  localparam logic [7:0] OP_ROT3       = 8'h29;

  // A PLAN-INTERNAL MICRO-OP, not a canonical opcode. The lowerer emits it in
  // place of RING when both radii are uniform, so it appears in the uop stream
  // the executor runs and never in a .zprog. Deliberately above 0xF0, outside
  // the canonical space, which tops out at 0x29.
  localparam logic [7:0] UOP_RING_PREP = 8'hF1;

  // Registers written back per point. ZERO MEANS NOT A LONG OP, and it is the
  // safe default on purpose: an opcode nobody has classified is refused rather
  // than guessed at, because a wrong width corrupts a register while a refusal
  // merely fails.
  function automatic logic [1:0] field_long_width(input logic [7:0] op);
    case (op)
      OP_CURVE, OP_DCURVE, OP_RIDGE,
      OP_SPLINE, UOP_RING_PREP,
      OP_SIN, OP_COS,
      OP_LEN2, OP_LEN3, OP_DIST2:         field_long_width = 2'd1;
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
