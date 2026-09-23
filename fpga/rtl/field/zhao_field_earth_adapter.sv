// zhao_field_earth_adapter.sv - THE E PROFILE'S STREAM ADAPTER: one lattice
// vertex and one section 9.1 list lane in, ONE Earth evaluation out, through
// the ONE field engine.
//
// Entry I34 of `zhao_console_core.sv`, build item (c). Owner directive section
// 20.8 ("Commit G -- Earth production path and one reducer").
//
// ---------------------------------------------------------------------------
// WHAT WAS ACTUALLY MISSING, AND WHY IT IS THIS FILE AND NOT A WIRE
// ---------------------------------------------------------------------------
// `zhao_terrain_patch` offers a per-vertex field lane -- `fld_valid_i`,
// `fld_ready_o`, `fld_height_i` -- and NOTHING IN THIS CONSOLE ANSWERED IT.
// With the lane low, section 3.4 collapses to `compose_top`: every TerrainField
// command the cartridge issues reaches CMD.EXEC, is sealed into
// `zhao_terrain_fieldlist`, is replayed into the patch's section 9.1 list, and
// then moves not one vertex, because no block evaluates the program.
//
// Three producers had to exist before this one could, and all three now do:
//
//   * `zhao_cmd_exec`'s TerrainField 0x0200 arm carries the UNIFORMS --
//     `tfld_start_tick_o` (R2's origin), `tfld_duration_o` (R3's span) and
//     `tfld_params_o` (256 bits, p0..p7 Q16.16 LE, R4..R11). Those three
//     outputs were connected to nothing, deliberately, with a header naming
//     THIS FILE as their only reader. They are read here.
//   * `zhao_terrain_fieldlist` seals the frame's list in command order and
//     replays it per patch job, and it resolves handle -> program hash by
//     sweeping FIELD.LOADER's publication port. It now also carries out of
//     that same sweep the OBJECT INDEX it matched, which is the binding
//     `zhao_field_host` executes from -- the identical value a BIND reply puts
//     on `fh2_resp_slot_o` and the identical one `zhao_geom_warp` resolves its
//     own `d_slot_i` from.
//   * `zhao_field_host_v2` is composed and already carries three profile
//     adapters. This is the fourth client and the FIRST Earth one.
//
// ---------------------------------------------------------------------------
// THE LAW IS THE ORACLE'S, VERBATIM, AND IT IS NOT INVENTED HERE
// ---------------------------------------------------------------------------
// `reference/src/zrender/terrain.cpp:compose_lattice` is the shipped renderer's
// TerrainField application and it is the ONE definition of the Earth input
// record. Transcribed, because every line of it is a decision this file would
// otherwise have had to make:
//
//     if (frame_tick < cmd.start_tick) continue;             // not begun yet
//     span = frame_tick - start_tick
//     age  = min(span, duration_ticks)                       // SATURATED
//     in[0] = cx                                             // vertex world x
//     in[1] = cz                                             // vertex world z
//     in[2] = age                                            // u32
//     in[3] = (duration == 0) ? 1.0fx
//                             : (age*65536 + duration/2) / duration
//     in[4..11] = parameters[0..31], four bytes each, LE
//     interpret(...)
//     lat.top[idx] = fx_add(lat.top[idx], out[0])            // HEIGHT, lane 0
//     velocity_out->push_back(field_velocity_lane(out))      // VELOCITY, lane 1
//
// FOUR THINGS IN THAT TRANSCRIPTION ARE LOAD-BEARING AND EACH IS HANDLED BELOW:
//
//  1. `age` is SATURATED at `duration_ticks`, not wrapped and not free-running.
//     A field whose span has elapsed keeps evaluating at its FINAL age forever,
//     which is what makes a crater persist instead of oscillating.
//  2. `phase` is an EXACT ROUNDED INTEGER DIVIDE, not a reciprocal multiply.
//     `(age*65536 + duration/2) / duration` is what the oracle computes, so a
//     reciprocal-and-multiply here would disagree with it at some ages and
//     agree at most -- the worst shape of numeric defect there is. The divider
//     below is therefore exact and sequential, and it runs ONCE PER RECORD PER
//     FRAME rather than once per vertex, because age and phase are UNIFORM
//     (field-ir.md section 7.1 says so in as many words: "uniform -- field
//     descriptor").
//  3. `continue` IS AN ADDITIVE ZERO ON THIS SEAM, and that is the one place in
//     this file where a zero is honest. The reference SKIPS a not-yet-begun or
//     unresolved field; `zhao_terrain_patch` consumes one word per accepted
//     list entry per vertex whether or not it contributes. A height of 0 into
//     `zhao_tp_fx_add_sat(acc, 0)` is `acc` exactly -- identical in value AND
//     identical in SatLedger records, because an add of zero cannot saturate.
//     So the skip and the zero are the same arithmetic, which is why this is
//     not the thing entry I34 forbids. What I34 forbids is a CONSTANT on the
//     lane standing in for a missing evaluation; what this is, is the
//     evaluation answering "this field does not act yet".
//  4. The out record is FOUR LANES. `spec/form/field-ir.md` section 7.1:
//     earth out = {height:fx, velocity:fx, material:u32, nav_cost:fx}. All four
//     leave this module on named ports, because directive 20.8's own sentence
//     is "Route height, velocity, material and nav outputs from the same
//     evaluation to their real owners" -- and an adapter that produced only the
//     one its current consumer can receive would have made that impossible to
//     do later without re-opening the engine seam.
//
// ---------------------------------------------------------------------------
// THE CADENCE, WHICH IS WHAT THIS SUBSYSTEM PUNISHES
// ---------------------------------------------------------------------------
// TWO SEPARATE CADENCES MEET HERE AND NEITHER MAY BE ASSUMED FROM THE OTHER:
//
//   * THE UNIFORM INTAKE is ONCE PER COMMAND PACKET -- one frame. It is the
//     same handshake `zhao_terrain_fieldlist` takes, JOINED: the composer ANDs
//     the two `ready`s, so a record is accepted when BOTH blocks accept it and
//     never otherwise. That is not a convenience. It is what makes this
//     module's bank index and the field list's entry index THE SAME NUMBER by
//     construction rather than by two counters that happen to agree -- and
//     "two counters that happen to agree" is precisely the defect entry I34
//     records `zhao_terrain_fieldlist` as having been built to prevent.
//     Neither `ready` depends on the other's `valid`, so the AND is not a loop.
//
//   * THE LANE STREAM is per VERTEX per LIST ENTRY. `zhao_terrain_patch`
//     captures a vertex on `vtx_valid_i && vtx_ready_o`, then raises
//     `fld_ready_o` and consumes exactly `fields_active_o` words in LIST ORDER.
//     This module shadows that lane counter off THE SAME TWO EVENTS -- the
//     vertex accept and the lane handshake -- so the shadow cannot drift by
//     construction either.
//
// AND THE SHADOW IS CHECKED ANYWAY, BY A DETECTOR WHOSE TWO OPERANDS ARE
// CLOCKED BY DIFFERENT THINGS. `lane_desync_o` differences THIS module's belief
// that lanes remain (`cur_lane < held_lanes`, loaded from `lanes_i` at the
// vertex accept) against the CONSUMER's own `fld_ready_o`, which is its
// internal `busy` and is loaded by the consumer's state machine. Nothing loads
// both. That is the first question `CLAUDE.md`'s metadata-swap chapter says to
// ask of any checker, and the answer here is structural: if the consumer
// finishes a vertex while this module still has lanes to answer, or holds
// `fld_ready_o` high after this module believes the vertex is done, the counter
// moves. A second arm of the same counter differences the number of entries
// this module SAW REPLAYED for the patch against the consumer's
// `fields_active_o`, which is a register in the other block.
//
// ---------------------------------------------------------------------------
// SECTION 9.1 IS DECIDED ONCE, BY THE BLOCK THAT OWNS THE LIST
// ---------------------------------------------------------------------------
// `lane_covers_i` is `zhao_terrain_patch`'s `fld_covers_o` -- the closed-
// interval footprint answer for the lane currently being offered, its chosen
// law 2. It is consumed here for ONE purpose: a lane that does not cover this
// vertex is answered with zero and NO RUN. That is the whole of section 9.1's
// value on this seam; without it a field with a nine-vertex footprint would
// cost a full 1,089-vertex walk of engine runs.
//
// HOLDING A SECOND COPY OF THE SIXTEEN RECTANGLES HERE WAS REJECTED, for the
// reason `zhao_terrain_velocity`'s header already gives for the same seam:
// "duplicating it would be a second implementation of one law and 2,048 flops"
// (charter section 29-6). The cost of consuming it instead is that the answer
// must belong to the lane this module is on -- which is the shadow above, and
// which is why the shadow has a detector rather than a paragraph.
//
// ---------------------------------------------------------------------------
// THE COST, SAID OUT LOUD, BECAUSE IT IS THE NUMBER THAT DECIDES THE NEXT BUILD
// ---------------------------------------------------------------------------
// `zhao_field_host`'s front holds ONE point in flight and a run is REGS +
// IN_LANES + the program's own length. At this console's parameterisation that
// is order 80-100 clocks PER COVERED VERTEX PER FIELD, against a patch of 1,089
// vertices. A single field covering a whole patch is therefore order 10^5
// clocks, and entry I34 records the frame allowance for an association as
// 10,416. THIS ADAPTER IS CORRECT AND IT IS FAR SLOWER THAN THE FRAME BUDGET,
// exactly as `zhao_field_flow_adapter` is for particles, and for the identical
// reason: a scalar front.
//
// `stall_cycles_o` is that cost MEASURED rather than argued -- cycles in which
// the consumer was waiting on this module with no answer ready. It is exported
// rather than kept private because it is the number that decides whether the
// field-major machine (`zhao_terrain_field_walk` + `zhao_terrain_patch_acc`,
// directive 13.2's `zhao_terrain_patch_v2`) has to be built before terrain
// fields can run at frame rate. It is NOT a reason to withhold this block:
// with no TerrainField ever issued the list is empty, `fields_active_o` is 0,
// the consumer never raises `fld_ready_o`, and this module costs the console
// nothing at all -- which is byte-for-byte its behaviour before the block
// existed.
//
// ---------------------------------------------------------------------------
// WHAT THIS MODULE DOES NOT DO
// ---------------------------------------------------------------------------
// No second evaluator (terrain_rules section 4.1 forbids one; the ONE engine is
// `u_field_host`). No footprint list (section 9.1's owner is the consumer).
// No fx_add chain and no clamp (section 3.4's owner is the consumer; this
// module hands over a raw out-lane 0 and the consumer accumulates it). No
// lattice walk (the consumer hands over the vertex). No velocity lattice store
// (spec/terrain_rules.md section 4.2's 2 B/vertex page belongs to whoever owns
// the VRAM page, and nobody does yet -- see `velocity_o`'s note).
//
// Conservative SystemVerilog subset only (charter section 2). Quartus 17.0:
// every elaboration check is inside `initial begin ... end` (R212), because a
// bare module-scope `if` is rejected with "syntax error near text: `if`" and
// `verilator --lint-only` does not run `initial` blocks at all.
// ENFORCED-BY: tests/field/field_earth_adapter_directed.cpp:main
`default_nettype none

module zhao_field_earth_adapter #(
    // terrain_rules section 9.1's MAX_PATCH_FIELDS, frozen 2026-08-16. The
    // SAME knob `zhao_terrain_patch` and `zhao_terrain_fieldlist` carry: three
    // hard-coded 16s would be one law with three halves.
    parameter int unsigned MAX_FIELDS = 16,
    // `zhao_field_loader`'s OBJECTS / OBJW, and `zhao_field_host`'s SLOTW.
    // They are the same namespace: a BIND reply's `fh2_resp_slot_o` IS the
    // object index, and it is what an adapter puts on `req_slot_o`.
    parameter int unsigned OBJW       = 3,
    parameter int unsigned SLOTW      = 3,
    // The shared field port's lane counts, which are the HOST's arity and NOT
    // this profile's. The earth record is 12 in / 4 out (field-ir.md 7.1); the
    // shipped shared pair is FIFTEEN in / SEVEN out (decision W01, the W
    // profile's width, which every client on the one engine has to carry). So
    // input lanes 12..14 are PADDED and output ordinals 4..6 are IGNORED, and
    // the guards below are the profile's minimum rather than an equality: a
    // client that demanded exactly its own arity could never share a bus.
    parameter int unsigned IN_LANES   = 15,
    parameter int unsigned OUT_LANES  = 7,
    // The oracle's `duration_ticks == 0` answer: phase is 1.0, Q16.16. Named
    // because it is a LAW READ OFF THE REFERENCE and the owner may not rewrite
    // the reference to change it (CLAUDE.md rule 6 -- every value is a knob).
    parameter logic signed [31:0] PHASE_ONE = 32'sh0001_0000
) (
    input var logic clk,
    input var logic rst_n,

    // ---- the frame tick, the R2 uniform's other operand ---------------------
    // `core_tick_c` in the console -- the shell's frame boundary counted, the
    // same one PART.SPAWN's `tick_i` consumes. The oracle's `frame_tick`.
    input var logic [31:0] tick_i,

    // ---- the UNIFORM intake: CMD.EXEC's three deliberately-open outputs ------
    // JOINED to `zhao_terrain_fieldlist`'s intake of the same record: the
    // composer ANDs the two readies. See THE CADENCE above for why that join
    // is the thing that makes the two blocks' entry indices one number.
    input  var logic         rec_valid_i,
    output var logic         rec_ready_o,
    input  var logic [ 31:0] rec_start_tick_i,  // tfld_start_tick_o, R2's origin
    input  var logic [ 31:0] rec_duration_i,    // tfld_duration_o,   R3's span
    input  var logic [255:0] rec_params_i,      // tfld_params_o, p0..p7 Q16.16 LE
    input  var logic         rec_last_i,        // tfld_last_o, the frame's seal

    // ---- the per-patch lane binding, OBSERVED on the list replay ------------
    // `patch_open_i` is the consumer's own `list_clear_i` pulse, so this
    // module's per-patch state and the consumer's empty at the same instant.
    // `add_fire_i` is the replay handshake `zhao_terrain_fieldlist` ->
    // `zhao_terrain_patch`; this module does not consume it, it WATCHES it, so
    // the entry index it records is the slot the consumer just filled.
    input var logic                patch_open_i,
    input var logic                add_fire_i,
    input var logic [OBJW-1:0]     add_obj_i,       // the resolved object index
    input var logic                add_resident_i,  // the handle resolved at all

    // ---- the vertex, captured on the CONSUMER's own accept ------------------
    input var logic               vtx_fire_i,  // vtx_valid_i && vtx_ready_o
    input var logic signed [31:0] vtx_wx_i,    // placed world x, fx16 raw
    input var logic signed [31:0] vtx_wz_i,    // placed world z, fx16 raw
    input var logic        [ 4:0] lanes_i,     // fields_active_o, 0..MAX_FIELDS

    // ---- the section 9.1 answer for the lane being offered -------------------
    // `zhao_terrain_patch`'s `fld_covers_o`, its chosen law 2. DECIDED THERE,
    // never re-decided here.
    input var logic lane_covers_i,

    // ---- the field engine client ---------------------------------------------
    output var logic                   req_valid_o,
    input  var logic                   req_ready_i,
    output var logic [SLOTW-1:0]       req_slot_o,
    output var logic                   req_noprog_o,
    output var logic [IN_LANES*32-1:0] req_in_o,
    input  var logic                   resp_valid_i,
    output var logic                   resp_ready_o,
    // THE LANES READ HERE ARE CANONICAL OUTPUT ORDINALS, NOT PHYSICAL WINDOW
    // POSITIONS -- FH17's one result contract, stated identically in
    // `zhao_field_flow_adapter.sv`, `zhao_field_stamp_adapter.sv` and
    // `zhao_field_warp_adapter.sv`. Ordinal 0/1/2/3 is height/velocity/
    // material/nav_cost of the EARTH record whatever physical register the
    // program wrote; the window-to-ordinal compaction is the HOST's act through
    // OUTPUT_MAP (owner directive 7.1/7.3). R111 measured that the shipped
    // programs' output registers are never contiguous, so an adapter reading a
    // window POSITION here would read whichever register happened to sit at
    // that offset -- and on THIS seam that is a plausible-looking terrain
    // deformation rather than an obvious failure.
    /* verilator lint_off UNUSEDSIGNAL */
    input  var logic [OUT_LANES*32-1:0] resp_out_i,
    /* verilator lint_on UNUSEDSIGNAL */
    input  var logic [7:0]              resp_status_i,

    // ---- the answer: FOUR out-lanes from ONE evaluation ----------------------
    // `ans_valid_o`/`ans_ready_i` is `zhao_terrain_patch`'s
    // `fld_valid_i`/`fld_ready_o`. `height_o` is `fld_height_i`.
    //
    // THE OTHER THREE HAVE NO CONSUMER IN THIS CONSOLE AND THEY ARE PRODUCED
    // ANYWAY. Directive 20.8 commissions routing all four "from the same
    // evaluation to their real owners", and an adapter that answered only the
    // one lane its consumer happens to have a port for would have re-created
    // the exact blocker it was built to remove. They are honestly open outputs
    // on a producer -- the smallest of the available shapes, and the one entry
    // I34 chose for `zhao_cmd_exec`'s uniforms for a year of this seam's life.
    //
    //   velocity_o -- out-lane 1. `zhao_terrain_velocity` is its real owner and
    //     is NOT composed: see that block's V3 (it drives its OWN 33x33 sweep,
    //     so joining it to the consumer's vertex stream is a scheduler and a
    //     composer may not write one) and its own header (its 545 KiB/frame
    //     `spec/memory_rules.md` 5b destination TERRAIN.COMPOSED_VELOCITY has
    //     no writer anywhere in this tree). Composing it today would discard
    //     every word it produced.
    //   material_o -- out-lane 2, u32. No block in this repository has an input
    //     for a field-produced material.
    //   nav_cost_o -- out-lane 3. Likewise.
    output var logic               ans_valid_o,
    input  var logic               ans_ready_i,
    output var logic signed [31:0] height_o,
    output var logic signed [31:0] velocity_o,
    output var logic        [31:0] material_o,
    output var logic signed [31:0] nav_cost_o,
    // The answer carries a REAL evaluation. Low means the word is the oracle's
    // `continue` expressed as an additive zero -- uncovered, not begun, or the
    // program is not resident. The consumer adds `height_o` either way and 0 is
    // its identity, so this is evidence and not control; it is a port rather
    // than a counter because the three causes below are already counted apart
    // and a consumer that one day wants "did a field act here" needs the bit.
    output var logic               ans_field_o,

    // ---- evidence -------------------------------------------------------------
    // R95: every one of these is asserted silent and then FIRED by
    // `tests/field/field_earth_adapter_directed.cpp`. A counter whose zero
    // nobody has seen move is a claim, not a measurement.
    output var logic [31:0] records_o,            // uniform records banked
    output var logic [31:0] tail_rejected_o,      // beyond MAX_FIELDS, section 9.1 law 2
    output var logic [31:0] runs_o,               // evaluations that returned a value
    output var logic [31:0] skipped_uncovered_o,  // section 9.1 miss: zero, no run
    output var logic [31:0] not_begun_o,          // tick < start_tick: zero, no run
    output var logic [31:0] noprog_o,             // handle unresolved, or engine said so
    output var logic [31:0] faults_o,             // the run ended on an alarm
    output var logic [31:0] lane_desync_o,        // THE SHADOW GUARD; see the header
    output var logic [31:0] stall_cycles_o,       // THE COST: consumer waiting, no answer
    output var logic        idle_o
);

  // The two statuses `zhao_field_host` adds above the ratified field-ir set.
  // 0xF1 (no result) and 0xF2 (alarm) are counted together on `faults_o`: both
  // mean the run produced nothing a vertex may be moved by, and neither is
  // separable from this side without a second status port.
  localparam logic [7:0] StNoProgram = 8'hF0;

  // The Q16.16 scale, as a SHIFT COUNT. The oracle writes `age * (1 << 16)`.
  localparam int unsigned FxShift = 16;
  // The oracle's quotient is bounded: `age <= duration` is enforced two lines
  // after it is read, so `(age<<16 + duration/2)/duration <= 65536`. Seventeen
  // bits hold 65536 exactly, which is why the divider below runs seventeen
  // steps over the low bits instead of forty-nine over all of them.
  localparam int unsigned PhaseW = 17;  // w=17, quotient bits

  // Index widths. `records_o`-style counts are 5 bits because MAX_FIELDS is
  // bounded at 16 by section 9.1 and `fields_active_o` next door is 5 bits.
  localparam int unsigned IdxW = 5;  // w=5, 0..MAX_FIELDS inclusive
  localparam int unsigned BankAw = (MAX_FIELDS <= 1) ? 1 : $clog2(MAX_FIELDS);

  initial begin
    if (MAX_FIELDS < 1 || MAX_FIELDS > 16) begin
      $fatal(1, "zhao_field_earth_adapter: MAX_FIELDS must be 1..16 (terrain_rules 9.1)");
    end
    if (IN_LANES < 12) begin
      $fatal(1, "zhao_field_earth_adapter: IN_LANES=%0d; field-ir.md 7.1 earth is 12", IN_LANES);
    end
    if (OUT_LANES < 4) begin
      $fatal(1, "zhao_field_earth_adapter: OUT_LANES=%0d; field-ir.md 7.1 earth is 4", OUT_LANES);
    end
    // THE OBJECT INDEX AND THE PROGRAM SLOT ARE ONE NAMESPACE, NOT TWO THAT
    // HAPPEN TO SHARE A WIDTH. `zhao_field_loader`'s BIND reply puts the object
    // index on `fh2_resp_slot_o`; that is the value `zhao_geom_warp` resolves
    // its own `d_slot_i` from and hands an adapter for `req_slot_o`. Equality
    // is asserted rather than padded, so a console that widened one of them
    // stops here instead of silently zero-extending a binding.
    if (OBJW != SLOTW) begin
      $fatal(1, "zhao_field_earth_adapter: OBJW=%0d != SLOTW=%0d; the loader object index IS the engine slot", OBJW, SLOTW);
    end
  end

  // ==========================================================================
  // THE UNIFORM BANK -- one entry per sealed TerrainField record
  // ==========================================================================
  // {age, phase, params} per entry. WHAT IT COSTS, HAND COUNTED IN THE
  // UNFLATTERING DIRECTION (R236): MAX_FIELDS * (32 + 32 + 256 + 1) = 5,136
  // bits. Written one per record per frame, read one per lane, so a synthesiser
  // is free to infer MLAB/M10K -- but if it does NOT, that is 5,136 registers
  // plus a 16-to-1 321-bit read mux, order 2,000 ALM. THAT IS THE NUMBER TO
  // QUOTE. It is the largest single cost of this adapter by a wide margin and
  // it is the reason `MAX_FIELDS` is a parameter: a console that ruled the
  // per-patch bound down to 4 would recover three quarters of it.
  //
  // `b_obj`/`b_res` are written on the REPLAY, not on the intake, because the
  // resolution is what the field list's publication sweep produces and the
  // replay is the handshake on which it is aligned with the consumer's own
  // list slot.
  logic [31:0] b_age [0:MAX_FIELDS-1];
  logic [31:0] b_phase[0:MAX_FIELDS-1];
  logic [255:0] b_par [0:MAX_FIELDS-1];
  logic b_begun[0:MAX_FIELDS-1];
  logic [OBJW-1:0] b_obj[0:MAX_FIELDS-1];
  logic b_res[0:MAX_FIELDS-1];

  // ---- the intake sequencer -------------------------------------------------
  localparam logic [1:0] I_TAKE = 2'd0;  // offering `rec_ready_o`
  localparam logic [1:0] I_DIV  = 2'd1;  // the exact rounded divide for `phase`
  localparam logic [1:0] I_WR   = 2'd2;  // commit or tail-reject

  logic [1:0] in_st;
  logic [IdxW-1:0] n_rec;      // entries in the current list, 0..MAX_FIELDS
  logic list_open;             // records are still arriving for this commit

  logic [31:0] h_age;
  logic [255:0] h_par;
  logic h_begun;
  logic h_last;
  logic h_reject;
  logic h_dur_zero;

  // ---- the exact rounded divide ---------------------------------------------
  // `(age * 65536 + duration/2) / duration`, an INTEGER divide with the oracle's
  // own pre-added half. Restoring long division, seventeen steps.
  //
  // WHY SEVENTEEN AND NOT FORTY-NINE, stated because it is the kind of
  // shortening that is wrong when the bound it rests on stops holding: the
  // numerator N is 49 bits, but `age` is clamped to `duration` on the cycle it
  // is captured, so Q = N/D <= 65536 < 2^17, hence N < D * 2^17, hence
  // N >> 17 < D. Seeding the remainder with N[48:17] therefore skips exactly
  // the leading run of zero quotient bits and no information. If the clamp on
  // `age` were ever removed this shortening would silently truncate the
  // quotient, which is why the clamp and this comment are four lines apart.
  logic [PhaseW-1:0] dv_num;  // the low numerator bits, shifted out MSB first
  logic [31:0] dv_den;
  // THIRTY-TWO BITS, NOT THIRTY-THREE, AND THE DROPPED BIT IS PROVED RATHER
  // THAN ASSUMED. A restoring divider's remainder is strictly less than the
  // divisor at every step: before the subtract `dv_shift` is at most 2*D-1 and
  // needs 33 bits, after it the value kept is either `dv_shift - D` (below D,
  // because `dv_shift` was below 2*D) or `dv_shift` itself on the branch where
  // it was already below D. Either way the result is below D, which is 32 bits,
  // so bit 32 is structurally zero and holding a flop for it would be holding a
  // flop for a constant.
  logic [31:0] dv_rem;
  logic [PhaseW-1:0] dv_q;
  logic [4:0] dv_cnt;

  // The bit is taken from the MSB and the register shifts left, rather than
  // being indexed by a running counter. The index expression that form needs
  // (`PhaseW-1-dv_cnt`) mixes an int with a 5-bit logic, which is exactly the
  // shape R212 records as lint-clean and synthesis-rejected. A shift has no
  // index at all.
  wire [32:0] dv_shift = {dv_rem, dv_num[PhaseW-1]};
  wire dv_ge = dv_shift >= {1'b0, dv_den};
  // THE SUBTRACT IS THIRTY-TWO BITS WIDE AND THAT IS EXACT, not a truncation.
  // It is evaluated only on the `dv_ge` branch, where `dv_shift - dv_den` is
  // below D and therefore below 2^32; two's complement subtraction agrees with
  // the true difference modulo 2^32, and a value below 2^32 is its own
  // residue. Carrying a thirty-third bit would be carrying a bit that is zero
  // on every cycle it is read -- which Verilator says out loud as UNUSEDSIGNAL
  // and which a wider register would merely have hidden.
  wire [31:0] dv_sub = dv_shift[31:0] - dv_den;

  // ---- the per-patch lane shadow ---------------------------------------------
  logic [IdxW-1:0] rep_idx;    // entries this module saw replayed for the patch
  logic [IdxW-1:0] cur_lane;   // which list lane the consumer is on
  logic [IdxW-1:0] held_lanes; // its `fields_active_o`, captured at the vertex
  logic vtx_live;              // a vertex is under evaluation
  logic signed [31:0] held_wx, held_wz;

  wire [BankAw-1:0] lane_a = cur_lane[BankAw-1:0];
  wire [BankAw-1:0] wr_a   = n_rec[BankAw-1:0];

  // ---- the run ----------------------------------------------------------------
  localparam logic [1:0] E_IDLE = 2'd0;
  localparam logic [1:0] E_REQ  = 2'd1;
  localparam logic [1:0] E_WAIT = 2'd2;
  localparam logic [1:0] E_ANS  = 2'd3;

  logic [1:0] state;
  logic [IN_LANES*32-1:0] held_in;
  logic signed [31:0] a_height, a_velocity, a_nav;
  logic [31:0] a_material;
  logic a_field;
  logic [SLOTW-1:0] held_slot;
  // The engine's own "there is no program here". `zhao_field_host_v2` ORs it
  // with its `!hdr_loaded[slot]` test, so a lane whose handle resolved to no
  // ready object and a lane whose object holds no header BOTH come back 0xF0
  // and are counted in one place.
  logic held_noprog;

  // ==========================================================================
  // THE CANDIDATE PAYLOAD, FORMED FROM THE LIVE PINS AND NEVER SENT
  // ==========================================================================
  // Owner directive 15.1: "Latch parameters, origin, dt, frame and the actual
  // particle record at request capture. Derive no later result from unrelated
  // live pins." The earth seam's equivalent operands are the vertex, the lane's
  // uniforms and the lane's slot, and ALL of them are latched in one act below.
  // `req_in_o` is driven from the latch, so an offer standing in E_REQ cannot
  // have its operands moved underneath it while the engine has not yet accepted
  // -- and on THIS seam the mover would be the consumer advancing to the next
  // lane, which happens on a handshake this module drives. The latch makes that
  // impossible rather than merely unlikely.
  logic [IN_LANES*32-1:0] cap_in_c;
  always_comb begin
    cap_in_c = '0;
    cap_in_c[(0*32) +: 32] = held_wx;                  // x:fx   -- the VERTEX
    cap_in_c[(1*32) +: 32] = held_wz;                  // z:fx
    cap_in_c[(2*32) +: 32] = b_age[lane_a];            // age:u32   R2, uniform
    cap_in_c[(3*32) +: 32] = b_phase[lane_a];          // phase:fx  R3, uniform
    cap_in_c[(4*32) +: 32] = b_par[lane_a][(0*32) +: 32];   // p0    R4
    cap_in_c[(5*32) +: 32] = b_par[lane_a][(1*32) +: 32];   // p1    R5
    cap_in_c[(6*32) +: 32] = b_par[lane_a][(2*32) +: 32];   // p2    R6
    cap_in_c[(7*32) +: 32] = b_par[lane_a][(3*32) +: 32];   // p3    R7
    cap_in_c[(8*32) +: 32] = b_par[lane_a][(4*32) +: 32];   // p4    R8
    cap_in_c[(9*32) +: 32] = b_par[lane_a][(5*32) +: 32];   // p5    R9
    cap_in_c[(10*32) +: 32] = b_par[lane_a][(6*32) +: 32];  // p6    R10
    cap_in_c[(11*32) +: 32] = b_par[lane_a][(7*32) +: 32];  // p7    R11
    // lane 12 is the HOST's thirteenth, which the earth profile does not have.
    // It is zero rather than absent because the port is IN_LANES wide for every
    // client; the engine takes the profile's arity from the program.
  end

  assign req_in_o = held_in;
  assign req_slot_o = held_slot;
  assign req_noprog_o = held_noprog;
  assign req_valid_o = (state == E_REQ);
  assign resp_ready_o = (state == E_WAIT);

  assign ans_valid_o = (state == E_ANS);
  assign ans_field_o = a_field;
  assign height_o = a_height;
  assign velocity_o = a_velocity;
  assign material_o = a_material;
  assign nav_cost_o = a_nav;

  assign rec_ready_o = (in_st == I_TAKE);
  assign idle_o = (in_st == I_TAKE) && (state == E_IDLE) && !vtx_live;

  // THE LANE IS OUTSTANDING when the consumer has taken a vertex and this
  // module has not yet answered all of its list lanes.
  wire lanes_left_c = vtx_live && (cur_lane < held_lanes);

  // The out-lane ordinals, FH17. Read only in E_WAIT.
  wire signed [31:0] out_height = resp_out_i[(0*32) +: 32];
  wire signed [31:0] out_velocity = resp_out_i[(1*32) +: 32];
  wire [31:0] out_material = resp_out_i[(2*32) +: 32];
  wire signed [31:0] out_nav = resp_out_i[(3*32) +: 32];

  // `zhao_field_host` answers 8'h00 for a run that reached OP_END with a result
  // and 0xF0/0xF1/0xF2 for no-program / no-result / alarm. Anything else is the
  // fabric's own ratified status and is equally not a field value. So OK is the
  // exact zero, not "not one of the three I remembered".
  wire status_ok = (resp_status_i == 8'h00);

  // A_SKIP: the three causes that answer ZERO WITHOUT A RUN, evaluated on the
  // bank entry the shadow says the consumer is offering. Each is counted apart,
  // because a merged "no field here" total cannot tell an unarmed console from
  // a field that has not started from a program that failed to load.
  // A_SKIP IS TWO CAUSES, NOT THREE. An unresolved handle is NOT answered
  // locally: it is offered to the engine with `req_noprog_o` raised, so the
  // refusal is the ENGINE's ratified one (status 0xF0) and `noprog_o` has
  // exactly one place it can move from. Answering it here would have made
  // `req_noprog_o` a constant zero on a port -- a tie-off wearing a signal's
  // name -- and would have hidden the host's own `!hdr_loaded` refusal, which
  // catches the case this module cannot see: an object that IS published and
  // whose header the fabric never loaded.
  wire skip_uncovered_c = !lane_covers_i;
  wire skip_notbegun_c = !b_begun[lane_a];
  wire skip_c = skip_uncovered_c || skip_notbegun_c;

  // The take of a uniform record, and section 9.1 law 2's tail reject. A record
  // arriving on a SEALED list opens a new one: the previous frame's list is
  // finished with, and the bound is PER LIST. This is `zhao_terrain_fieldlist`'s
  // own rule, restated rather than shared, because the two blocks are joined at
  // the handshake and MUST reach the same verdict on the same record -- and the
  // handshake join is what guarantees they see the same records to judge.
  wire take_now_c = rec_valid_i && rec_ready_o;
  wire reopening_c = take_now_c && !list_open;
  wire [IdxW-1:0] eff_n_c = reopening_c ? {IdxW{1'b0}} : n_rec;
  wire would_reject_c = ({27'd0, eff_n_c} >= MAX_FIELDS);

  // The oracle's saturated age, computed on the cycle the record is taken.
  // `span` is 32-bit and cannot borrow, because `begun` gates it.
  wire begun_c = (tick_i >= rec_start_tick_i);
  wire [31:0] span_c = tick_i - rec_start_tick_i;
  wire [31:0] age_c = (span_c > rec_duration_i) ? rec_duration_i : span_c;
  wire [31:0] age_eff_c = begun_c ? age_c : 32'd0;

  // The oracle's numerator: `age * (1 << 16) + duration / 2`. The half is ADDED
  // and not ORed -- `duration/2` is thirty-one bits and overlaps the shifted
  // age wherever the duration exceeds 65,535, so an OR would be right on short
  // fields and quietly wrong on long ones. The widest value is
  // (2^32-1) * 2^16 + 2^31, which is below 2^48, so 49 bits cannot carry.
  wire [48:0] num_c = ({17'd0, age_eff_c} << FxShift) + {18'd0, rec_duration_i[31:1]};

  integer i;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      for (i = 0; i < MAX_FIELDS; i = i + 1) begin
        b_age[i] <= 32'd0;
        b_phase[i] <= 32'd0;
        b_par[i] <= 256'd0;
        b_begun[i] <= 1'b0;
        b_obj[i] <= {OBJW{1'b0}};
        b_res[i] <= 1'b0;
      end
      in_st <= I_TAKE;
      n_rec <= 5'd0;
      list_open <= 1'b0;
      h_age <= 32'd0;
      h_par <= 256'd0;
      h_begun <= 1'b0;
      h_last <= 1'b0;
      h_reject <= 1'b0;
      h_dur_zero <= 1'b0;
      dv_num <= {PhaseW{1'b0}};
      dv_den <= 32'd0;
      dv_rem <= 32'd0;
      dv_q <= {PhaseW{1'b0}};
      dv_cnt <= 5'd0;
      rep_idx <= 5'd0;
      cur_lane <= 5'd0;
      held_lanes <= 5'd0;
      vtx_live <= 1'b0;
      held_wx <= 32'sd0;
      held_wz <= 32'sd0;
      state <= E_IDLE;
      held_in <= '0;
      held_slot <= {SLOTW{1'b0}};
      held_noprog <= 1'b0;
      a_height <= 32'sd0;
      a_velocity <= 32'sd0;
      a_material <= 32'd0;
      a_nav <= 32'sd0;
      a_field <= 1'b0;
      records_o <= 32'd0;
      tail_rejected_o <= 32'd0;
      runs_o <= 32'd0;
      skipped_uncovered_o <= 32'd0;
      not_begun_o <= 32'd0;
      noprog_o <= 32'd0;
      faults_o <= 32'd0;
      lane_desync_o <= 32'd0;
      stall_cycles_o <= 32'd0;
    end else begin

      // ====================================================================
      // THE COST, MEASURED. A cycle in which the consumer is holding
      // `fld_ready_o` high for a lane this module has not yet answered.
      // ====================================================================
      if (ans_ready_i && (state != E_ANS)) begin
        if (stall_cycles_o != 32'hFFFF_FFFF) stall_cycles_o <= stall_cycles_o + 32'd1;
      end

      // ====================================================================
      // THE SHADOW GUARD (R95). Two arms, and NEITHER of them differences two
      // things one enable loads.
      //
      //  (a) this module's belief that lanes remain, against the CONSUMER's
      //      `fld_ready_o` -- its internal `busy`, loaded by its own state
      //      machine from its own vertex accept. If the two ever disagree
      //      while a vertex is live, one of them has lost count.
      //  (b) the number of entries this module watched the field list replay
      //      into the consumer, against the consumer's own `fields_active_o`
      //      register, sampled at the vertex accept. Those are two different
      //      blocks' registers loaded by two different events.
      // ====================================================================
      // Arm (a) is UNGATED, and that is the strong form. `zhao_terrain_patch`
      // raises `busy` on the same vertex accept this module raises `vtx_live`
      // on, and clears it on the same last-lane handshake this module clears
      // `vtx_live` on -- so the two are equal at EVERY cycle if and only if the
      // shadow is right. Gating the check on `vtx_live` would have made it
      // blind to the one direction that matters most: the consumer holding a
      // vertex open that this module believes is finished.
      if (vtx_live != ans_ready_i) begin
        if (lane_desync_o != 32'hFFFF_FFFF) lane_desync_o <= lane_desync_o + 32'd1;
      end
      if (vtx_fire_i && (rep_idx != lanes_i)) begin
        if (lane_desync_o != 32'hFFFF_FFFF) lane_desync_o <= lane_desync_o + 32'd1;
      end

      // ====================================================================
      // THE UNIFORM INTAKE
      // ====================================================================
      case (in_st)
        I_TAKE: begin
          if (take_now_c) begin
            if (reopening_c) n_rec <= 5'd0;
            list_open <= 1'b1;
            h_last <= rec_last_i;
            h_reject <= would_reject_c;
            h_par <= rec_params_i;
            h_begun <= begun_c;
            h_age <= age_eff_c;
            h_dur_zero <= (rec_duration_i == 32'd0);

            // The oracle's numerator, with its own pre-added half. Formed from
            // the SATURATED age, which is what bounds the quotient at 65536 and
            // licenses the seventeen-step divide above.
            // THE SEED IS THE SHORTENING. `num_c[48:17]` is the remainder
            // after the leading thirty-two quotient bits, every one of which is
            // zero because `age_eff_c <= duration` bounds the quotient at
            // 65,536. Nothing is discarded; the leading zeros are skipped.
            dv_num <= num_c[PhaseW-1:0];
            dv_den <= rec_duration_i;
            dv_rem <= num_c[48:PhaseW];
            dv_q <= {PhaseW{1'b0}};
            dv_cnt <= 5'd0;

            // A rejected record is still WALKED through the divide it does not
            // need, deliberately and for `zhao_terrain_fieldlist`'s own stated
            // reason: a reject that skipped the work would make the intake's
            // TIMING depend on the list's fullness, and the joined handshake
            // beside it would then desynchronise exactly on a full frame.
            in_st <= I_DIV;
          end
        end

        I_DIV: begin
          dv_rem <= dv_ge ? dv_sub : dv_shift[31:0];
          dv_q   <= {dv_q[PhaseW-2:0], dv_ge};
          dv_num <= {dv_num[PhaseW-2:0], 1'b0};
          if (({27'd0, dv_cnt} + 32'd1) >= PhaseW) begin
            in_st <= I_WR;
          end else begin
            dv_cnt <= dv_cnt + 5'd1;
          end
        end

        I_WR: begin
          if (h_reject) begin
            tail_rejected_o <= tail_rejected_o + 32'd1;
          end else begin
            b_age[wr_a] <= h_age;
            // The oracle: `duration == 0 ? 1.0fx : the divide`. The divider ran
            // anyway (see the reject note) and its answer on a zero denominator
            // is discarded here rather than guarded there, so the divide has
            // one shape and no special case inside the loop.
            b_phase[wr_a] <= h_dur_zero ? PHASE_ONE : {15'd0, dv_q};
            b_par[wr_a] <= h_par;
            b_begun[wr_a] <= h_begun;
            // A fresh entry is NOT resident until the replay says so. The
            // resolution belongs to the field list's publication sweep, and
            // inheriting the previous frame's answer for this slot would be a
            // stale binding that evaluates a program the cartridge replaced.
            b_res[wr_a] <= 1'b0;
            b_obj[wr_a] <= {OBJW{1'b0}};
            n_rec <= n_rec + 5'd1;
            records_o <= records_o + 32'd1;
          end
          if (h_last) list_open <= 1'b0;
          in_st <= I_TAKE;
        end

        default: in_st <= I_TAKE;
      endcase

      // ====================================================================
      // THE PER-PATCH REPLAY WATCH
      // ====================================================================
      // `patch_open_i` is the consumer's `list_clear_i`; the field list holds
      // the vertex lane until its replay is done, so every `add_fire_i` for
      // this patch lands before the first `vtx_fire_i` of it. That interlock is
      // `zhao_terrain_fieldlist`'s (`patch_stall_o`) and this module depends on
      // it rather than restating it -- which is why arm (b) of the shadow guard
      // above exists: if the interlock ever stopped holding, `rep_idx` would be
      // short at the vertex accept and the counter would move.
      if (patch_open_i) begin
        rep_idx <= 5'd0;
      end else if (add_fire_i) begin
        if ({27'd0, rep_idx} < MAX_FIELDS) begin
          b_obj[rep_idx[BankAw-1:0]] <= add_obj_i;
          b_res[rep_idx[BankAw-1:0]] <= add_resident_i;
          rep_idx <= rep_idx + 5'd1;
        end
      end

      // ====================================================================
      // THE LANE STREAM
      // ====================================================================
      if (vtx_fire_i) begin
        held_wx <= vtx_wx_i;
        held_wz <= vtx_wz_i;
        held_lanes <= lanes_i;
        cur_lane <= 5'd0;
        vtx_live <= (lanes_i != 5'd0);
      end

      case (state)
        E_IDLE: begin
          // `lanes_left_c` uses `vtx_live`, a register, so the first lane of a
          // vertex starts the cycle AFTER its accept. The consumer raises
          // `fld_ready_o` in that same later cycle (its `busy` is a register
          // too), so nothing is lost -- and the one cycle of skew is why the
          // shadow guard is gated on `vtx_live` rather than on `vtx_fire_i`.
          if (lanes_left_c) begin
            // 15.1's CAPTURE, and it is ONE act: the vertex is already held,
            // and the lane's uniforms, its slot and the whole payload are
            // latched here from this cycle's bank read. Nothing downstream
            // re-reads the bank, so the consumer advancing its lane counter
            // cannot move this run's operands.
            held_in <= cap_in_c;
            held_slot <= b_obj[lane_a];
            held_noprog <= !b_res[lane_a];
            if (skip_c) begin
              // The oracle's `continue`, expressed as the additive zero the
              // consumer's fx_add chain treats identically. Counted by CAUSE.
              a_field <= 1'b0;
              a_height <= 32'sd0;
              a_velocity <= 32'sd0;
              a_material <= 32'd0;
              a_nav <= 32'sd0;
              if (skip_uncovered_c) begin
                if (skipped_uncovered_o != 32'hFFFF_FFFF)
                  skipped_uncovered_o <= skipped_uncovered_o + 32'd1;
              end else if (skip_notbegun_c) begin
                if (not_begun_o != 32'hFFFF_FFFF) not_begun_o <= not_begun_o + 32'd1;
              end else begin
                if (noprog_o != 32'hFFFF_FFFF) noprog_o <= noprog_o + 32'd1;
              end
              state <= E_ANS;
            end else begin
              state <= E_REQ;
            end
          end
        end

        E_REQ: begin
          if (req_ready_i) state <= E_WAIT;
        end

        E_WAIT: begin
          if (resp_valid_i) begin
            if (status_ok) begin
              a_field <= 1'b1;
              a_height <= out_height;
              a_velocity <= out_velocity;
              a_material <= out_material;
              a_nav <= out_nav;
              if (runs_o != 32'hFFFF_FFFF) runs_o <= runs_o + 32'd1;
            end else begin
              // A refused run contributes NOTHING, which is the oracle's
              // `prog == nullptr -> continue`, not a zero height standing in
              // for a height that was never computed.
              a_field <= 1'b0;
              a_height <= 32'sd0;
              a_velocity <= 32'sd0;
              a_material <= 32'd0;
              a_nav <= 32'sd0;
              if (resp_status_i == StNoProgram) begin
                if (noprog_o != 32'hFFFF_FFFF) noprog_o <= noprog_o + 32'd1;
              end else begin
                if (faults_o != 32'hFFFF_FFFF) faults_o <= faults_o + 32'd1;
              end
            end
            state <= E_ANS;
          end
        end

        E_ANS: begin
          if (ans_ready_i) begin
            cur_lane <= cur_lane + 5'd1;
            if (({27'd0, cur_lane} + 32'd1) >= {27'd0, held_lanes}) vtx_live <= 1'b0;
            state <= E_IDLE;
          end
        end

        default: state <= E_IDLE;
      endcase
    end
  end

endmodule

`default_nettype wire
