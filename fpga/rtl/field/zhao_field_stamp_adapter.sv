// zhao_field_stamp_adapter.sv — the S profile's STREAM ADAPTER: the stencil
// walk that turns one SurfaceStamp into SHEET_W*SHEET_H field records.
//
// Contract: design/contracts/FIELD.SEQ.STAMP.md, which is a CONFIGURATION of
//           FIELD.SEQ.CORE and not a second engine.
// Consumer: `fpga/rtl/surface/zhao_surface_stamp.sv`, port `fld_*` (its S2).
//
// ---------------------------------------------------------------------------
// WHAT AN ADAPTER IS ALLOWED TO BE
// ---------------------------------------------------------------------------
// `design/contracts/FIELD.SEQ.CORE.md` permits exactly this file and describes
// it in advance:
//
//   > **Profile adapters are PERMITTED and are the v3 front end.** A profile is
//   > a program set plus a STREAM ADAPTER -- a small generator that produces the
//   > varying input lanes directly (Earth: lattice x,z from patch origin +
//   > pitch; ... Stamp: stencil u,v) and consumes the outputs directly from
//   > snooped export registers. Adapters generate and consume streams; they
//   > NEVER re-implement an op, and there are not five engines.
//
// So there is no arithmetic here beyond the two declared UNIT CONVERSIONS
// below, no op, no coverage test and no blend. There is a counter, a
// handshake, and the lane binding.
//
// ===========================================================================
// FH17 -- THE ONE RESULT CONTRACT, STATED ONCE, HERE AND IN THE FLOW ADAPTER
// ===========================================================================
// Owner directive `reports/Zhaozhou_SHARED_FIELD_Repair_Architecture_2026-09-20.txt`
// FH17: "ALL CONSUMERS SPEAK THE SAME RESULT CONTRACT ... Semantic profile
// support and physical client-port count are separately declared."
//
// Those are TWO different quantities and conflating them is what this file got
// wrong until 2026-09-20. They are now separately named:
//
//   PHYSICAL CLIENT PORT -- `IN_LANES` / `OUT_LANES`. How many 32-bit lanes the
//     SHARED bus between this adapter and `zhao_field_host` carries. It is the
//     MAXIMUM over every composed profile, not this profile's own arity, and
//     every client on the bus must be given the SAME pair or the concatenated
//     bus truncates silently. The console composes 13/7 today (flow's 13 in,
//     flow's 7 out); R103's P1 widens the input half to warp's 15.
//
//   SEMANTIC PROFILE ARITY -- the S profile is 8 in / 3 out, from
//     `spec/form/field-ir.md` 7.1 line 526, verbatim:
//       | stamp | 4 | u,v:unit, age:u32, strength:unit, p0..p3:fx (8)
//                  | tag_op:u32, strength:unit, emissive:unit (3) |
//     This adapter never asserts the physical width as the profile's arity and
//     never the other way round.
//
// **AND THE INDEX SPACE OF `resp_out_i` IS THE CANONICAL OUTPUT ORDINAL, NOT
// THE PHYSICAL CAPTURE WINDOW.** This is the whole of FH17 in one sentence and
// it is the thing every consumer must agree on. Lane j of `resp_out_i` is
// canonical output ordinal j of the bound profile -- ordinal 0 is tag_op,
// ordinal 1 is strength, ordinal 2 is emissive -- whatever physical register
// the program happened to write. The window-to-ordinal compaction is the
// HOST's act through `OUTPUT_MAP` (owner directive 7.1 and 7.3; R101's window
// mask and FH05's required mask are different named fields with different
// widths and are never assigned to one another). An adapter that read a
// PHYSICAL WINDOW POSITION here would be reading whichever register happened to
// sit at that offset, which is exactly the silent wrong value R111 measured:
// the three shipped Earth programs leave three of the seven window lanes
// unwritten, so HOLES ARE THE NORMAL CASE.
//
// The flow adapter states the identical rule for ordinals 3,4,5. One contract.
//
// ===========================================================================
// FH26 -- TWO NAMED BINDINGS, AND THE PERMISSIVE ONE IS NEVER THE DEFAULT
// ===========================================================================
// Owner directive 15.2, verbatim: "The current adapter drives two raw integer
// texel indices, zeros other transport lanes, reads tag_op and a 16-bit
// strength, and describes that as the S record. The canonical Field table
// describes eight inputs and three outputs, with unit fx16 strength/emissive.
// **These are not identical contracts.**"
//
// They are now two NAMED bindings, selected by `STAMP_BINDING`, and there is
// no safe default:
//
//   STAMP_BINDING_UNBOUND (0) -- REFUSED AT ELABORATION. This is FH26's
//     requirement made structural. "No production program is made successful by
//     omitting its output contract or taking an implicit zero-mask
//     compatibility escape." A parameter whose default silently selected the
//     permissive bridge is precisely the R111 shape -- a mechanism that reads
//     as protection while providing none -- and a composer who omits the
//     binding must get a LOUD elaboration failure, not a quiet legacy machine.
//     Every instantiation therefore states which contract it is speaking.
//
//   STAMP_BINDING_LEGACY_BRUSH (1) -- the EXACT pre-2026-09-20 behaviour, bit
//     for bit: R0/R1 carry raw integer texel indices, `tag_op` is ordinal 0 and
//     `strength` is the LOW 16 BITS of ordinal 1. It exists so the existing
//     physical fixture and its programs keep producing the existing stamp
//     pictures during the migration, and for no other reason. 15.2: "The legacy
//     bridge is not evidence that the full canonical Stamp binding works."
//
//   STAMP_BINDING_CANONICAL (2) -- the full S signature, with the two
//     conversions 15.2 writes out longhand. See below.
//
// **WHY THE TWO CANNOT SHARE A TEST, IN ONE CASE.** 15.2: "Do not break the
// existing stamp pictures by silently interpreting raw texel indices as Q16.16
// units, or by taking the low 16 bits of Q16.16 1.0 (WHICH IS ZERO)."
// fx16 1.0 is 32'h0001_0000. Its low 16 bits are 16'h0000. So a response
// carrying unit strength 1.0 delivers **65535 under CANONICAL and 0 under
// LEGACY** -- full brush against no brush at all, from the same response. That
// single case discriminates the two bindings, and the directed test names it.
//
// ---------------------------------------------------------------------------
// THE TWO CANONICAL CONVERSIONS, WRITTEN OUT BECAUSE 15.2 WRITES THEM OUT
// ---------------------------------------------------------------------------
// Both are NEW EXPLICIT ADAPTER CONVERSIONS. Neither is a change to Field
// arithmetic, and neither exists in the legacy binding.
//
// 1. TEXEL CENTRE, on the way IN. 15.2: "u/v sample centers follow the surface
//    sheet's actual texel-center convention; a 64-wide unit center (2*i+1)/128
//    maps exactly to fx16 (2*i+1)*512."
//      (2i+1)/(2*SHEET_W) in Q16.16 = (2i+1) * 65536 / (2*SHEET_W)
//                                   = (2i+1) << (15 - log2(SHEET_W))
//    For SHEET_W = 64 that is (2i+1) << 9 = (2i+1)*512, which is the ruling's
//    own worked example, EXACTLY and with no divider. The sheet dimensions are
//    therefore required to be powers of two, checked at elaboration -- a
//    non-power-of-two sheet would need a divide and there is no divider here.
//    i = 0  -> 512 (not 0), i = 63 -> 65024 (not 65536): a texel CENTRE is
//    never on a sheet edge, which is the whole point of the convention.
//
// 2. UNIT -> u16, on the way OUT. 15.2, verbatim:
//      u = clamp(signed_fx16, 0, 65536)
//      strength16 = floor((u * 65535 + 32768) / 65536)
//    "using a wide intermediate", and it maps fx16 0 -> 0 and fx16 1 -> 65535.
//    `u * 65535` is computed as `(u << 16) - u`, a shift and a subtract: the
//    console is already over its DSP budget and a literal multiplier here would
//    be spent for a constant that is one less than a power of two.
//    The four cases the ruling names -- both endpoints, the half case and out
//    of range -- are 0 -> 0, 65536 -> 65535, 32768 -> 32768 exactly, and
//    anything outside [0, 65536] clamped with `canon_clamps_o` moving.
//
// `fld_emissive_o` is canonical ordinal 2, returned because the profile
// declares three outputs. 15.2: "the host returns emissive as the third
// declared output, even where the present surface consumer has no emissive
// input ... Do not claim the surface renderer uses the emissive output if it
// does not. Retaining and correctly returning it is shared-host conformance;
// commissioning its eventual surface effect is a separate application policy."
// **So it is stated here as an honest exclusion rather than discarded silently
// inside the host: `zhao_surface_stamp` HAS NO EMISSIVE INPUT TODAY.** This
// port is the declared lane; wiring it to a consumer is a later application
// decision and is not this file's to take.
//
// ---------------------------------------------------------------------------
// THE ORDER IS THE CONSUMER'S, AND THAT IS THE WHOLE ALIGNMENT ARGUMENT
// ---------------------------------------------------------------------------
// `zhao_surface_stamp`'s S2 is explicit, and it chose its shape precisely so
// that this file could exist without knowing anything about circles:
//
//   > S2. THE FIELD BRUSH DELIVERS ONE RESULT PER VISITED TEXEL -- all 4,096,
//   > in the same j-outer/i-inner scan order -- not one per COVERED texel. A
//   > result whose texel is not covered is consumed and DISCARDED.
//   > REJECTED ALTERNATIVE: results only for covered texels, which makes the
//   > consumption rate depend on a coverage test that lives in THIS block, so
//   > FIELD.SEQ.STAMP would have to reproduce this geometry bit-for-bit or the
//   > pair deadlocks.
//
// So this adapter walks a plain raster and never computes coverage. The two
// cursors are kept in step by TWO things, and it needs both:
//
//   1. A SHARED START. `cmd_fire_i` is the cycle the stamp's own command port
//      accepts a command -- the same edge the consumer starts on. Free-running
//      and trusting "4,096 per stamp" would misalign forever the first time a
//      stamp ABORTS, and S4 says an ACQUIRE that overflows aborts the whole
//      stamp before any write, consuming ZERO records.
//   2. THE HANDSHAKE. One record leaves per `fld_valid_o && fld_ready_i`, so
//      the consumer's cursor and this counter advance on the same cycle by
//      construction rather than by agreement.
//
// A second command arriving while a walk is still running is the fault this
// pair can actually have, and `restarts_o` counts it rather than leaving it to
// be inferred from a wrong sheet. The walk restarts, because the consumer has
// restarted: following it is the only alignment that can be right.
//
// Owner directive 15.2's traversal law is unchanged by the binding split and
// is the reason the restart path below is a state and not a wire: "The stamp
// traversal still owes exactly one terminal field record per texel its consumer
// commits to visiting, in the existing order ... Restart/abort must drain old
// requests without delivering their results into the next stamp."
//
// ---------------------------------------------------------------------------
// WHAT HAPPENS WHEN THERE IS NO PROGRAM, AND WHY IT IS NOT DECIDED HERE
// ---------------------------------------------------------------------------
// `arm_ready_o` is high only when a stamp program is resident. The CONSOLE ANDs
// it into the stamp's `cmd_field_en_i`, so a stamp that asks for the brush with
// no program loaded runs as a plain ABI stamp instead of stalling forever on
// records that cannot come. That is a policy decision and it is taken in the
// composer, beside the other console policy inputs, rather than buried here.
// 15.2: "Missing program before start and a fault mid-walk remain distinct
// cases" -- they are `arm_ready_o` low and `faults_o` respectively, and they
// have never been merged.
//
// A run that FAULTS mid-walk is different and is handled here, because by then
// the consumer is already committed to SHEET_W*SHEET_H records: the record is
// delivered with the engine's zeroed lanes and `faults_o` counts it. A stalled
// walk would hang the stamp, and `zhao_field_seq`'s own header is right that a
// hang is the worse failure and the one nobody can debug from a frame capture.
//
// Conservative SystemVerilog subset (Quartus 17.0): the elaboration checks live
// inside `initial begin ... end` because Quartus 17.0 rejects a module-scope
// `if`, and there is no `generate` here at all.
//
// ENFORCED-BY: tests/field/field_stamp_adapter_directed.cpp:main

`default_nettype none

module zhao_field_stamp_adapter #(
    // The sheet is 64 x 64 (spec/terrain_rules.md 7, layer F, 4,096 texels).
    // It is a parameter so the directed test can walk a small sheet and still
    // reach the end-of-walk and restart cases without 4,096 engine runs.
    // MUST BE A POWER OF TWO -- the texel-centre conversion is a shift.
    parameter int unsigned SHEET_W = 64,
    parameter int unsigned SHEET_H = 64,
    parameter int unsigned SLOTW = 3,

    // ---- THE PHYSICAL CLIENT PORT (FH17), not the S profile's 8/3 ----------
    // The SHARED bus width, which every client on it must agree on. Corrected
    // 2026-09-20 from 12/4 -- which was neither this profile's arity (8/3) nor
    // the composed bus (13/7) but EARTH's record, copied. A default that
    // disagrees with the composition is a trap for the next composer, and this
    // one had already sprung: the generated census top `zhao_prod_top.sv`
    // instantiates this module with NO parameter override, so it was measuring
    // a 12/4 machine while `zhao_console_core.sv` composed a 13/7 one.
    parameter int unsigned IN_LANES = 13,
    parameter int unsigned OUT_LANES = 7,

    // ---- FH26: WHICH CONTRACT THIS INSTANCE SPEAKS -------------------------
    // 0 = UNBOUND, refused at elaboration. 1 = LEGACY_STAMP_BRUSH.
    // 2 = CANONICAL_STAMP. There is deliberately no safe default: see the
    // header. The codes are integers rather than a package enum so that this
    // file adds nothing to any source list; they are named below and in
    // design/contracts/FIELD.SEQ.STAMP.md.
    parameter int unsigned STAMP_BINDING = 0
) (
    input  logic clk,
    input  logic rst_n,

    // ---- the consumer's start edge ------------------------------------------
    input  logic                  cmd_fire_i,      // the stamp accepted a command
    input  logic                  cmd_field_en_i,  // ... and it wants the brush

    // ---- console policy: which resident program is the brush ---------------
    input  logic [SLOTW-1:0]      slot_i,
    input  logic                  slot_valid_i,
    output logic                  arm_ready_o,

    // ---- stamp_field_results, into zhao_surface_stamp's fld_* --------------
    output logic                  fld_valid_o,
    input  logic                  fld_ready_i,
    output logic [31:0]           fld_tag_op_o,
    output logic [15:0]           fld_strength_o,
    // CANONICAL ORDINAL 2. Declared, converted and exported; NOT consumed by
    // `zhao_surface_stamp`, which has no emissive input. Held at zero by the
    // legacy binding, which declares two outputs and not three.
    output logic [15:0]           fld_emissive_o,

    // ---- the shared engine --------------------------------------------------
    output logic                     req_valid_o,
    input  logic                     req_ready_i,
    output logic [SLOTW-1:0]         req_slot_o,
    output logic                     req_noprog_o,
    output logic [IN_LANES*32-1:0]   req_in_o,
    input  logic                     resp_valid_i,
    output logic                     resp_ready_o,
    // THE LANES THIS ADAPTER READS ARE CANONICAL OUTPUT ORDINALS, NOT PHYSICAL
    // WINDOW POSITIONS -- see the FH17 section of the header. The S profile
    // declares three of them; the shared bus carries OUT_LANES because the
    // widest composed profile (flow) declares seven. The lanes above the
    // profile's arity are not fields of the S record and reading them would be
    // this block inventing outputs the profile does not declare.
    /* verilator lint_off UNUSEDSIGNAL */
    input  logic [OUT_LANES*32-1:0]  resp_out_i,
    /* verilator lint_on UNUSEDSIGNAL */
    input  logic [7:0]               resp_status_i,

    // ---- counters ------------------------------------------------------------
    output logic [31:0] stamps_o,      // walks started
    output logic [31:0] texels_o,      // records delivered
    output logic [31:0] faults_o,      // runs that came back with a status
    output logic [31:0] restarts_o,    // a command arrived mid-walk
    // A canonical unit output arrived outside [0.0, 1.0] and was clamped. Zero
    // on the legacy binding by construction (it performs no conversion), so it
    // is also the cheapest live discriminator between the two bindings.
    output logic [31:0] canon_clamps_o,
    output logic        busy_o
);

  // ---- FH26's named binding codes ------------------------------------------
  localparam int unsigned STAMP_BINDING_UNBOUND      = 0;
  localparam int unsigned STAMP_BINDING_LEGACY_BRUSH = 1;
  localparam int unsigned STAMP_BINDING_CANONICAL    = 2;

  // ---- the S profile's own arity (field-ir.md 7.1 line 526) ----------------
  localparam int unsigned S_CANONICAL_INPUTS  = 8;
  localparam int unsigned S_CANONICAL_OUTPUTS = 3;

  localparam int unsigned TEXELS = SHEET_W * SHEET_H;
  localparam int unsigned IW = (SHEET_W > 1) ? $clog2(SHEET_W) : 1;
  localparam int unsigned JW = (SHEET_H > 1) ? $clog2(SHEET_H) : 1;
  localparam int unsigned TW = (TEXELS > 1) ? $clog2(TEXELS) : 1;

  // The texel-centre shift of the header's conversion 1: (2i+1) << (15 - log2 W).
  localparam int unsigned U_SHIFT = 15 - IW;
  localparam int unsigned V_SHIFT = 15 - JW;

  initial begin
    if (STAMP_BINDING == STAMP_BINDING_UNBOUND) begin
      // FH26 made structural. A composer that omits the binding gets this,
      // never a quiet legacy machine.
      $fatal(1, "zhao_field_stamp_adapter: STAMP_BINDING is UNBOUND. Owner directive FH26 forbids an implicit legacy default: state 1 (LEGACY_STAMP_BRUSH) or 2 (CANONICAL_STAMP) at the instantiation.");
    end
    if ((STAMP_BINDING != STAMP_BINDING_LEGACY_BRUSH) &&
        (STAMP_BINDING != STAMP_BINDING_CANONICAL)) begin
      $fatal(1, "zhao_field_stamp_adapter: STAMP_BINDING=%0d is not a named binding (1 = LEGACY_STAMP_BRUSH, 2 = CANONICAL_STAMP)", STAMP_BINDING);
    end
    // The PHYSICAL port must be able to carry the profile's SEMANTIC arity.
    // Two separate declarations, checked against each other rather than
    // assumed equal (FH17).
    if (IN_LANES < S_CANONICAL_INPUTS) begin
      $fatal(1, "zhao_field_stamp_adapter: IN_LANES=%0d cannot carry the S profile's %0d canonical inputs (field-ir.md 7.1)", IN_LANES, S_CANONICAL_INPUTS);
    end
    if (OUT_LANES < S_CANONICAL_OUTPUTS) begin
      $fatal(1, "zhao_field_stamp_adapter: OUT_LANES=%0d cannot carry the S profile's %0d canonical outputs (tag_op, strength, emissive)", OUT_LANES, S_CANONICAL_OUTPUTS);
    end
    if (SLOTW < 1) $fatal(1, "zhao_field_stamp_adapter: SLOTW must be at least 1");
    // Conversion 1 is a shift, so the sheet must be a power of two and must
    // leave room for the half-texel: (2*(W-1)+1) << (15-log2 W) < 65536.
    if ((SHEET_W & (SHEET_W - 1)) != 0) begin
      $fatal(1, "zhao_field_stamp_adapter: SHEET_W=%0d is not a power of two; the texel-centre conversion is a shift and there is no divider here", SHEET_W);
    end
    if ((SHEET_H & (SHEET_H - 1)) != 0) begin
      $fatal(1, "zhao_field_stamp_adapter: SHEET_H=%0d is not a power of two; the texel-centre conversion is a shift and there is no divider here", SHEET_H);
    end
    if (IW > 15) $fatal(1, "zhao_field_stamp_adapter: SHEET_W=%0d exceeds the fx16 unit range", SHEET_W);
    if (JW > 15) $fatal(1, "zhao_field_stamp_adapter: SHEET_H=%0d exceeds the fx16 unit range", SHEET_H);
  end

  localparam logic [2:0] S_IDLE = 3'd0;
  localparam logic [2:0] S_REQ  = 3'd1;  // offering the point to the engine
  localparam logic [2:0] S_WAIT = 3'd2;  // waiting for its answer
  localparam logic [2:0] S_OUT  = 3'd3;  // offering the record to the stamp
  // S_DROP is the restart's landing state and it is not a formality. A restart
  // taken straight back to S_REQ would do two illegal things at once:
  //
  //   * ABANDON AN IN-FLIGHT ANSWER. The engine parks in its response state
  //     until the claimant takes the result. Walking away from one leaves it
  //     holding a response nobody will ever read, and it accepts no further
  //     request -- a deadlock of the whole shared engine, caused by one stamp
  //     being aborted, with no counter anywhere that would have moved.
  //   * CHANGE AN OFFER THAT IS STILL STANDING. `req_valid_o` is high in S_REQ,
  //     so resetting the texel counter under it would move the operands of a
  //     request the engine has not yet accepted.
  //
  // So a restart always spends at least one cycle here, with the offer dropped
  // and the response port draining. This IS 15.2's "restart/abort must drain
  // old requests without delivering their results into the next stamp".
  localparam logic [2:0] S_DROP = 3'd4;

  logic [2:0]      state;
  logic            outstanding;  // a request was accepted; its answer is owed
  logic [TW:0]     tex;          // one bit wider than TEXELS so the end is a compare
  logic [31:0]     rec_tag_op;
  logic [15:0]     rec_strength;
  logic [15:0]     rec_emissive;

  // j-outer, i-inner: the consumer's order, restated as arithmetic rather than
  // as an assumption. `i` is the fast axis.
  wire [IW-1:0] cur_i = tex[IW-1:0];
  wire [JW-1:0] cur_j = tex[(IW+JW)-1:IW];

  assign arm_ready_o = slot_valid_i;
  assign busy_o      = (state != S_IDLE);

  // A walk starts when the CONSUMER starts one. `cmd_field_en_i` is the
  // console's already-gated policy bit, so a stamp that did not ask for the
  // brush produces no records at all and the consumer consumes none.
  wire start_c = cmd_fire_i && cmd_field_en_i;

  // ==========================================================================
  // CONVERSION 1 -- the texel centre, on the way IN
  // ==========================================================================
  // (2i+1) << (15 - log2 SHEET_W). Exact, divider-free, and never on an edge.
  wire [31:0] u_centre_c = 32'(({{(31 - IW){1'b0}}, cur_i, 1'b1}) << U_SHIFT);
  wire [31:0] v_centre_c = 32'(({{(31 - JW){1'b0}}, cur_j, 1'b1}) << V_SHIFT);

  always_comb begin
    req_in_o = '0;
    if (STAMP_BINDING == STAMP_BINDING_CANONICAL) begin
      // R0 = u, R1 = v, as UNIT texel centres. Lanes 2..7 of the S record
      // (age, strength, p0..p3) are UNIFORM for a whole stamp and are the
      // HOST's prepared scalars under FH06/FH09 -- they are not varying inputs
      // and so are not this stream generator's to drive. Owner directive 7.3:
      // "For PREPARED_SCALAR output j, validate the scalar's written bit and
      // preparation identity at association seal, then seed the export when a
      // point/context starts."
      req_in_o[(0*32) +: 32] = u_centre_c;
      req_in_o[(1*32) +: 32] = v_centre_c;
    end else begin
      // LEGACY_STAMP_BRUSH: raw integer texel indices, exactly as before.
      // Every other declared lane is zero, which is bit-exact: the engine's
      // first act is the law's `reg[0..63] = 0`, so writing a zero into a lane
      // the program never declared leaves the file where the law wants it.
      req_in_o[(0*32) +: 32] = {{(32 - IW){1'b0}}, cur_i};
      req_in_o[(1*32) +: 32] = {{(32 - JW){1'b0}}, cur_j};
    end
  end

  assign req_valid_o  = (state == S_REQ);
  assign req_slot_o   = slot_i;
  // The engine is told rather than guessed at: if the console has no resident
  // brush program the request is refused at source and comes back with a
  // status, instead of this block inventing a texel record.
  assign req_noprog_o = !slot_valid_i;
  assign resp_ready_o = (state == S_WAIT) || (state == S_DROP);

  assign fld_valid_o    = (state == S_OUT);
  assign fld_tag_op_o   = rec_tag_op;
  assign fld_strength_o = rec_strength;
  assign fld_emissive_o = rec_emissive;

  // ==========================================================================
  // CONVERSION 2 -- unit fx16 -> the consumer's u16, on the way OUT
  // ==========================================================================
  // Owner directive 15.2, verbatim:
  //   u = clamp(signed_fx16, 0, 65536)
  //   strength16 = floor((u * 65535 + 32768) / 65536)
  // `u * 65535` is `(u << 16) - u`: no multiplier is spent on a constant one
  // less than a power of two, on a console already over its DSP budget.
  // The intermediate is 34 bits because `65536 << 16` needs 33.
  localparam logic signed [31:0] FX16_ONE = 32'sh0001_0000;

  function automatic logic [15:0] unit_to_u16(input logic signed [31:0] s,
                                              output logic clamped);
    logic [33:0] u;
    // BOTH UNUSED SLICES OF `num` ARE DELIBERATE AND ARE THE POINT OF THE
    // WIDTH, which is why this is a pragma and not a narrower declaration:
    //   [15:0]  is the DISCARDED FRACTION. Dropping it IS `floor(... / 65536)`.
    //   [33:32] is the HEADROOM that proves the sum cannot wrap: `65536 << 16`
    //           alone needs 33 bits. The directive says "using a wide
    //           intermediate"; narrowing it to the bits that are read would
    //           delete the evidence that it is wide enough.
    /* verilator lint_off UNUSEDSIGNAL */
    logic [33:0] num;
    /* verilator lint_on UNUSEDSIGNAL */
    begin
      clamped = 1'b0;
      if (s <= 32'sh0000_0000) begin
        if (s < 32'sh0000_0000) clamped = 1'b1;
        u = 34'd0;
      end else if (s >= FX16_ONE) begin
        // The endpoint itself is IN range; only strictly above it is a clamp.
        if (s > FX16_ONE) clamped = 1'b1;
        u = 34'd65536;
      end else begin
        u = {{(34 - 32){1'b0}}, s};
      end
      num = ((u << 16) - u) + 34'd32768;
      unit_to_u16 = num[31:16];
    end
  endfunction

  // Canonical ordinals 0, 1 and 2 of `resp_out_i`. NOT window positions.
  wire        [31:0] out_tag_op_c   = resp_out_i[(0*32) +: 32];
  wire signed [31:0] out_strength_c = resp_out_i[(1*32) +: 32];
  wire signed [31:0] out_emissive_c = resp_out_i[(2*32) +: 32];

  logic [15:0] conv_strength_c;
  logic [15:0] conv_emissive_c;
  logic        clamp_strength_c;
  logic        clamp_emissive_c;
  always_comb begin
    conv_strength_c = unit_to_u16(out_strength_c, clamp_strength_c);
    conv_emissive_c = unit_to_u16(out_emissive_c, clamp_emissive_c);
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state          <= S_IDLE;
      outstanding    <= 1'b0;
      tex            <= '0;
      rec_tag_op     <= 32'd0;
      rec_strength   <= 16'd0;
      rec_emissive   <= 16'd0;
      stamps_o       <= 32'd0;
      texels_o       <= 32'd0;
      faults_o       <= 32'd0;
      restarts_o     <= 32'd0;
      canon_clamps_o <= 32'd0;
    end else begin
      if (req_valid_o && req_ready_i) outstanding <= 1'b1;
      else if (resp_valid_i && resp_ready_o) outstanding <= 1'b0;

      if (start_c) begin
        // Follow the consumer, always. It has restarted its own cursor, so a
        // walk that kept its place would be delivering texel 900's record for
        // texel 0 -- well formed, plausible, and wrong everywhere.
        if ((state != S_IDLE) && (restarts_o != 32'hFFFF_FFFF)) begin
          restarts_o <= restarts_o + 32'd1;
        end
        if (stamps_o != 32'hFFFF_FFFF) stamps_o <= stamps_o + 32'd1;
        tex   <= '0;
        state <= S_DROP;
      end else begin
        case (state)
          S_IDLE: begin
            // nothing to do
          end

          // Leave only when nothing is owed. `outstanding` is cleared by the
          // drain in the same cycle the answer is taken, so this costs one
          // cycle when there was nothing in flight and two when there was.
          S_DROP: begin
            if (!outstanding || (resp_valid_i && resp_ready_o)) state <= S_REQ;
          end

          S_REQ: begin
            if (req_ready_i) state <= S_WAIT;
          end

          S_WAIT: begin
            if (resp_valid_i) begin
              rec_tag_op <= out_tag_op_c;
              // `tag_op`'s tag/blend/age_shift interpretation is the
              // consumer's and is IDENTICAL in both bindings. 15.2: "preserve
              // tag_op's exact existing tag/blend/age-shift interpretation."
              // `zhao_surface_stamp` unpacks tag = [7:0], blend = [10:8],
              // age_shift = [14:12], and nothing here touches those fields.
              if (STAMP_BINDING == STAMP_BINDING_CANONICAL) begin
                rec_strength <= conv_strength_c;
                rec_emissive <= conv_emissive_c;
                if ((clamp_strength_c || clamp_emissive_c) &&
                    (canon_clamps_o != 32'hFFFF_FFFF)) begin
                  canon_clamps_o <= canon_clamps_o + 32'd1;
                end
              end else begin
                // LEGACY: the low 16 bits, exactly as before. This is the
                // behaviour 15.2 names as the one that must not be applied to
                // a canonical program -- fx16 1.0's low half is zero.
                rec_strength <= out_strength_c[15:0];
                rec_emissive <= 16'd0;
              end
              if (resp_status_i != 8'd0) begin
                // Delivered anyway, and counted. By now the consumer is
                // committed to exactly TEXELS records and withholding one
                // hangs the stamp.
                rec_tag_op   <= 32'd0;
                rec_strength <= 16'd0;
                rec_emissive <= 16'd0;
                if (faults_o != 32'hFFFF_FFFF) faults_o <= faults_o + 32'd1;
              end
              state <= S_OUT;
            end
          end

          S_OUT: begin
            if (fld_ready_i) begin
              if (texels_o != 32'hFFFF_FFFF) texels_o <= texels_o + 32'd1;
              if (tex == (TW+1)'(TEXELS - 1)) begin
                tex   <= '0;
                state <= S_IDLE;
              end else begin
                tex   <= tex + 1'b1;
                state <= S_REQ;
              end
            end
          end

          default: state <= S_IDLE;
        endcase
      end
    end
  end

endmodule : zhao_field_stamp_adapter

`default_nettype wire
