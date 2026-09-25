// zhao_field_earth_adapter_intake_race_mutant.sv
// ===========================================================================
// A DELIBERATELY BROKEN COPY of `fpga/rtl/field/zhao_field_earth_adapter.sv`.
// IT IS NOT PRODUCTION RTL AND NOTHING SHIPS IT. The module is RENAMED so that
// no source list can elaborate it by mistake.
//
// WHAT IT PROVES, and why it had to be committed rather than argued.
// ---------------------------------------------------------------------------
// Packet EARTHLOCK repaired the intake/replay race COMPOSEPUB found: the
// adapter's `rec_ready_o` is the FIRST beat of its intake, the banks landed
// EIGHTEEN CLOCKS LATER at I_WR, and the joined handshake had already told the
// rest of the console that record N was in. After the repair THERE IS NO RACE
// TO MISS -- which is exactly why a test asserting the race would have been a
// test that passes only while the defect exists. CLAUDE.md forbids that test,
// so the demonstration lives here instead, with its polarity INVERTED.
//
// `tests/mutants/field_earth_adapter_intake_race_control.cpp` drives this file
// and PASSES WHEN THE RACE IS OBSERVED. It is evidence about the repair, not
// about the design.
//
// THE FIVE HUNKS, AND ALL FIVE ARE SUBSTANTIVE. This is the repair reverted,
// nothing more and nothing less:
//
//   1. the module name (the rename; required so nothing elaborates it)
//   2. `cap_en_c` loses `&& !entry_busy_c` -- the RAM read enable no longer
//      holds off the entry under construction
//   3. the `E_IDLE` branch loses `&& !entry_busy_c` -- the lane stream no
//      longer holds either, so `skip_c` may read a half-written `b_begun`
//   4. the stale-binding clear LEAVES I_TAKE
//   5. ...and returns to I_WR, where the per-patch replay's `add_fire_i` set
//      can land in front of it and be WIPED
//
// `entry_busy_c` and `intake_a_c` are left DECLARED and unused, deliberately:
// the diff against production is then exactly the five hunks that matter, and
// a reader can see that the wires were not what was wrong -- their USE was.
//
// WHAT THE DRIVER SEES, measured against this file on 2026-09-25:
//   the loud half   -- `req_noprog_o` 1 where production gives 0, and
//                      `req_slot_o` 0 where production gives the replay's own
//                      object: the intake's late clear wiped the replay
//   the silent half -- the engine handed the PREVIOUS frame's age, phase and
//                      parameters with resident already set, and NOT ONE
//                      COUNTER MOVING
//
// The silent half is the one no instrument in the tree could see: `noprog_o`
// is silent on it, both arms of the lane shadow difference entry COUNTS and
// the count is right, and arm (c) pairs the read's address with itself.
// ===========================================================================
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
// OPEN DEFECT -- `rec_ready_o` IS NOT THE RECORD BEING BANKED, AND THE REPLAY
// CAN LAND IN THE GAP. FOUND 2026-09-25 (COMPOSEPUB), NOT REPAIRED HERE.
// ---------------------------------------------------------------------------
// THE ORDERING. `rec_ready_o` is `(in_st == I_TAKE)`, so the handshake is the
// FIRST beat of the intake, not its last. The sequencer then walks
// I_TAKE -> I_DIV (seventeen restoring-division steps, and a REJECTED record
// is walked through it too) -> I_WR, and it is I_WR that writes the banks:
//
//     b_begun[wr_a] <= h_begun;
//     b_res  [wr_a] <= 1'b0;      // "not resident until the replay says so"
//     b_obj  [wr_a] <= '0;
//
// So the banks for entry N land EIGHTEEN CLOCKS AFTER the handshake that the
// rest of the console treats as "record N is in" -- and that handshake is
// JOINED with `zhao_terrain_fieldlist`'s seal, which is the whole argument for
// `wr_a` and the list's entry index being the same number.
//
// THE CONSEQUENCE. `b_res` HAS TWO WRITERS ON TWO DIFFERENT COUNTERS: the
// intake clears `b_res[wr_a]` at I_WR, the per-patch replay sets
// `b_res[rep_idx]` on `add_fire_i`. Nothing interlocks their TIMING -- this
// module cannot backpressure a lane it only observes, and says so. If a patch
// job's replay of entry N lands inside entry N's eighteen-clock window, the
// replay sets the resident flag and the intake's late clear WIPES IT.
//
// MEASURED, not argued. `tests/terrain/composepub_acceptance.cpp` presented
// exactly that ordering -- a record handshake followed about six clocks later
// by the section 9.1 replay -- and the console-shaped chain returned:
//
//     engine runs=1089  runs_o=0  noprog=1089  skipped=0  not_begun=0
//     faults=0  short=0   live_top == compose_top at every vertex
//
// A FIELD THAT RAN, COST THE ENGINE EVERY CYCLE IT SHOULD, AND MOVED NOTHING,
// with every other census balancing. That is CLAUDE.md's metadata-swap shape
// in its racing form: the flag and the record it describes are loaded by
// different enables, and `noprog_o` is the ONLY symptom.
//
// WHY NOTHING CATCHES IT. There is no assertion in this file (deliberately --
// see `no_rw_check` below, "buy area by asserting an interlock this module
// does not own"). `lane_desync_o` cannot see it: both its arms difference
// ENTRY COUNTS, and the count is right -- only the flag's value is wrong.
// `tce_job_take` is not gated on `idle_o`. And FIELDARM's `tfl_patch_stall`
// interlock guards a DIFFERENT hazard (replay vs vertex), not intake vs replay.
//
// WHETHER THE SHIPPED CONSOLE REACHES IT IS A SCHEDULING PROPERTY, NOT A
// STRUCTURAL GUARANTEE: it depends on the gap between CMD.EXEC's last
// TerrainField record and the first `tce_job_take`, which is a property of the
// command packet's ordering. Safety today therefore rests on an unverified
// promise about command order -- the exact shape the composer rejects two
// lines from `tpt_vtx_valid` ("a comment asserting the page burst is long
// enough would be the unverified promise directive 13.4 names").
//
// THE RELATED SILENT RISK, STATED BUT NOT DEMONSTRATED. `b_begun` and `b_uni`
// are also written at I_WR and are read by the lane stream. A vertex that
// fires after the replay but BEFORE I_WR reads the PREVIOUS FRAME's age, phase
// and parameters with the resident flag still set -- a real engine run on
// stale uniforms, which unlike the noprog path moves no counter at all. This
// packet did not reach that ordering and does not claim it; it is the same
// missing interlock and belongs in the same repair.
//
// THE SHAPE OF THE REPAIR, for the packet that owns this block. Move the
// `b_res`/`b_obj` clear from I_WR to I_TAKE. Its stated purpose -- "a fresh
// entry is NOT resident until the replay says so ... inheriting the previous
// frame's answer would be a stale binding" -- is served exactly as well at
// ACCEPTANCE, and acceptance is the beat the rest of the console is
// synchronised to. That makes "the same number by construction" true in TIME
// as well as in VALUE. Do NOT assert the bug: the test to keep is that a
// replay's resident flag SURVIVES an in-flight intake.
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

module zhao_field_earth_adapter_intake_race_mutant #(
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
    // DIRECTIVE 8.1's `output_present_mask`, ORDINAL-INDEXED: bit j is set when
    // ordinal j is a VALUE and clear when it is a HOLE. Owner ruling R168 and
    // decision W10. `zhao_field_warp_adapter` has read this port since
    // 2026-09-20 and THIS ADAPTER DID NOT until this commit, although the
    // composer had the mask on a named wire (`fld_resp_present_c`) the whole
    // time -- so the gap was one hop, not the producer rebuild entry I34
    // recorded.
    //
    // WHY IT IS NOT REDUNDANT WITH `resp_status_i`, which is the reading that
    // left it unconnected here. A STATUS IS A VERDICT ON THE RUN; THE PRESENCE
    // MASK IS A STATEMENT ABOUT EACH ORDINAL. `zhao_field_host_v2` retires
    // StOk when every DECLARED ordinal landed -- `complete_c` is
    // `((seen_next_c & req_mask_c) == req_mask_c)`, a test against what the
    // header DECLARED and not against this profile's four. So a program whose
    // header declares only ordinals 0 and 1 returns 8'h00 with ordinals 2 and 3
    // clear, and the host's own comment beside StPartial says what those words
    // then hold: "the absent ones read as the zero cleared at grant -- an
    // answer a caller reading only the words cannot tell from a field that
    // happens to be zero there". Gating on the status alone therefore publishes
    // a HOLE as a VALUE, and on THIS seam the hole is a terrain material or a
    // nav cost, which is the flattering direction: a plausible number from a
    // lane no program wrote.
    //
    // Owner directive section 13.3 is the same law stated for this record: "A
    // genuinely absent optional lane under an explicit compatible program
    // signature is not a write of zero. Do not conflate those to make material
    // reduction plausible." The reducer 13.2 commissions for material is "the
    // LAST field in command order that covers the vertex AND WRITES the
    // material lane wins" -- UNIMPLEMENTABLE without this mask. That is why the
    // mask is read and republished here rather than left for the consumer to
    // go and find: the consumer cannot, because by then the response is gone.
    // Ordinals 4..6 belong to the wider profiles sharing this seam and are not
    // this record's, exactly as for `resp_out_i` three lines up.
    /* verilator lint_off UNUSEDSIGNAL */
    input  var logic [OUT_LANES-1:0]    resp_present_i,
    /* verilator lint_on UNUSEDSIGNAL */

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

    // THE PER-ORDINAL PRESENCE OF THIS ANSWER -- {nav_cost, material, velocity,
    // height} in ordinal order, bit j set when lane j is a VALUE this
    // evaluation wrote and clear when it is a HOLE. Directive 20.8's "route
    // height, velocity, material and nav outputs from the same evaluation to
    // their real owners" is not satisfiable without it: an owner handed four
    // words and no presence cannot apply material's writer-selection law, and
    // applying it anyway makes every non-writing field the last writer.
    //
    // IT IS LATCHED WITH THE WORDS IT DESCRIBES, not read live beside them, and
    // that is CLAUDE.md's metadata-swap law rather than a style choice. The
    // answer registers hold across `E_ANS` until the consumer accepts; a
    // presence read live off `resp_present_i` in that window would describe
    // whatever response the host was offering NEXT -- answer A's four words
    // with answer B's presence, and `runs_o`, `faults_o` and `noprog_o` all
    // still balancing, because not one of them looks at the field that moved.
    // The host itself already designed this out one level up: `resp_present_o`
    // is `rsp_pres[head_idx_c]`, stored per reservation at retirement, exactly
    // as `rsp_stat` is. This port keeps that property across this seam.
    //
    // ZERO ON EVERY ANSWER THAT IS NOT A RUN -- uncovered, not begun, no
    // program, faulted -- because each of those is the oracle's `continue`, and
    // a `continue` writes no lane at all. `ans_field_o` low and
    // `ans_present_o` zero are then the same statement made twice, which is
    // correct: the first says no evaluation happened, the second says no lane
    // was written, and a future record could have one without the other.
    //
    // IT HAS NO CONSUMER IN THIS CONSOLE YET, deliberately, and that is the
    // shape `velocity_o`/`material_o`/`nav_cost_o` already have beside it: the
    // consumer is directive 13.2's `zhao_terrain_patch_v2`. An honestly open
    // output on a producer is the smallest of the three available shapes and is
    // the one entry I34 chose for CMD.EXEC's uniforms for this seam's whole
    // life.
    output var logic [3:0]         ans_present_o,

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
    // R168's ARM, AND THE ONLY REASON IT CAN BE REACHED IS THAT PRESENCE IS NOW
    // READ. A run the host called OK whose record was SHORT -- at least one of
    // the four canonical Earth ordinals came back a hole. It is counted apart
    // from `faults_o` because it is not a fault: section 13.3 rules an absent
    // optional lane legitimate under a compatible signature. It is counted at
    // all because "legitimate" is not "invisible" -- a console composing
    // material off a stream of short records would reduce holes, and the only
    // number that could ever say so is this one.
    output var logic [31:0] short_record_o,
    output var logic [31:0] lane_desync_o,        // THE SHADOW GUARD; see the header
    output var logic [31:0] stall_cycles_o,       // THE COST: consumer waiting, no answer
    output var logic        idle_o
);

  // The two statuses `zhao_field_host` adds above the ratified field-ir set.
  // 0xF1 (no result) and 0xF2 (alarm) are counted together on `faults_o`: both
  // mean the run produced nothing a vertex may be moved by, and neither is
  // separable from this side without a second status port.
  localparam logic [7:0] StNoProgram = 8'hF0;

  // The four canonical Earth ordinals (field-ir.md 7.1). A response carrying
  // all four is COMPLETE; anything less is SHORT.
  //
  // A SHORT RECORD IS COUNTED AND PUBLISHED, NOT REFUSED, AND THE DIFFERENCE
  // FROM THE WARP ADAPTER IS RATIFIED RATHER THAN CHOSEN. `zhao_field_warp_
  // adapter` voids the whole answer on a short record, because decision W10
  // says "publish no partially warped meshlet" -- a vertex displaced on two
  // axes of three is a broken vertex. Directive 13.3 says the opposite about
  // THIS record: "a genuinely absent optional lane under an explicit
  // compatible program signature is not a write of zero". An Earth program
  // that moves the ground and declares no material is a legal, ordinary
  // program, and refusing its height because it wrote no material would
  // discard a real evaluation. So the present lanes are published, the absent
  // ones are published as absent, and the consumer decides per lane.
  localparam logic [3:0] EarthOrdinals = 4'b1111;

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
    // THE RAM'S ADDRESS MUST COVER THE BANK. `BankAw` is derived from
    // `MAX_FIELDS` two lines up, so this cannot fire at the ratified 16 -- it
    // is here because the payload is now addressed memory rather than a mux,
    // and a bank a parameter made bigger than its address would alias entries
    // onto each other silently, which a mux never could.
    if ((32'd1 << BankAw) < MAX_FIELDS) begin
      $fatal(1, "zhao_field_earth_adapter: BankAw=%0d cannot address MAX_FIELDS=%0d", BankAw, MAX_FIELDS);
    end
    if (OBJW != SLOTW) begin
      $fatal(1, "zhao_field_earth_adapter: OBJW=%0d != SLOTW=%0d; the loader object index IS the engine slot", OBJW, SLOTW);
    end
  end

  // ==========================================================================
  // THE UNIFORM BANK -- one entry per sealed TerrainField record
  // ==========================================================================
  // {age, phase, params} per entry, MAX_FIELDS * (32 + 32 + 256) = 5,120 bits
  // of PAYLOAD plus 80 bits of control.
  //
  // IT DID NOT INFER, AND THAT WAS MEASURED RATHER THAN FEARED. This paragraph
  // used to say a synthesiser "is free to infer MLAB/M10K -- but if it does
  // NOT, that is 5,136 registers plus a 16-to-1 321-bit read mux, order 2,000
  // ALM". A leaf `quartus_map` on 2026-09-25 settled it, and the estimate was
  // LOW: 6,274 registers, 2,419 combinational ALUTs, 4,326 ESTIMATED ALMs and
  // `Total block memory bits = 0`.
  //
  // NOTE HOW IT FAILED, because it is the easier signal to misread. Quartus
  // says "uninferred due to ..." when it CONSIDERED an array and refused. It
  // said nothing at all here -- the array never presented as a RAM candidate,
  // so an absence of complaints was not evidence of success, and the only
  // number that could say so is the block-memory-bits one.
  //
  // THE THREE BLOCKERS, and two of them had to go together.
  // `tools/quartus/check_ram_inference.py` named them: a COMBINATIONAL read
  // through the dynamic index `lane_a`, which forces a per-bit mux the width of
  // the array; TWO dynamic write addresses, `[i]` and `[wr_a]`, where `[i]` was
  // the per-element reset loop the island brief's S5.3 forbids by name; and an
  // async-reset process, which that tool marks a WEAK SIGNAL with measured
  // false positives. The read is now synchronous and the reset loop is gone
  // from the payload. Either repair alone leaves the array in flip-flops.
  //
  // AND `MAX_FIELDS` STAYS 16. This paragraph used to end "it is the reason
  // `MAX_FIELDS` is a parameter: a console that ruled the per-patch bound down
  // to 4 would recover three quarters of it." THAT SENTENCE IS SUPERSEDED and
  // is not a lever anyone may pull: owner ruling R244/D-EARTH-A -- "keep
  // MAX_FIELDS=16 for now. Do not cut it to 4. Sixteen is a ratified
  // gameplay/capacity law" -- and `reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt`
  // section 8, which authorises "synchronous banks" and names cutting 16 fields
  // to 4 as outside the standing delegation entirely. `MAX_FIELDS` remains a
  // parameter because three hard-coded 16s would be one law with three halves,
  // which is what its own declaration says; it is NOT an area knob.
  //
  // `b_obj`/`b_res` carry their VALUE from the REPLAY, not from the intake,
  // because the resolution is what the field list's publication sweep produces
  // and the replay is the handshake on which it is aligned with the consumer's
  // own list slot. The intake's only touch is the stale-binding CLEAR, and
  // that clear is at I_TAKE -- the accepted beat -- so the replay can never
  // land on the far side of it.
  //
  // THIS PARAGRAPH USED TO READ "written on the REPLAY, not on the intake",
  // FULL STOP, AND THE CODE TWO HUNDRED LINES BELOW CONTRADICTED IT: I_WR
  // wrote both, eighteen clocks after the handshake the rest of the console
  // treats as "record N is in". The comment was describing the design that was
  // intended and the code was implementing a race. EARTHLOCK moved the code to
  // match the comment rather than the comment to match the code, because the
  // comment was the one that was right.
  // THE PAYLOAD IS ONE 320-BIT RAM WORD PER ENTRY, and its layout is NAMED
  // rather than implied, because a part-select on a concatenation is exactly
  // where a silent field swap lives.
  localparam int unsigned UniAgeLo   = 0;                // age:u32   R2
  localparam int unsigned UniPhaseLo = UniAgeLo + 32;    // phase:fx  R3
  localparam int unsigned UniParLo   = UniPhaseLo + 32;  // p0..p7    R4..R11
  localparam int unsigned UniW       = UniParLo + 256;   // 320

  // `ramstyle` IS A HINT AND A HINT IS NOT AN INFERENCE -- the phrasing
  // `zhao_part_table.sv` uses for the same attribute, and the reason the two
  // committed leaf maps in this packet's receipt exist. The number that says
  // whether this worked is `Total block memory bits`, not this line.
  //
  // NO `no_rw_check`, AND THE REASON HAS CHANGED WITHOUT THE ANSWER CHANGING.
  // The intake writes `wr_a` from one process and the lane stream reads
  // `lane_a` from another. That used to mean a same-cycle same-address
  // collision was not provably impossible, because NOTHING in this module
  // interlocked them. EARTHLOCK's `entry_busy_c` now does: `cap_en_c` is held
  // low for exactly the entry the intake is writing, so the collision is in
  // fact unreachable, and the attribute would now be TRUE rather than hopeful.
  //
  // IT IS STILL NOT DECLARED, DELIBERATELY. `no_rw_check` changes what the
  // fitter may infer, and this packet's whole obligation on the memory is that
  // EARTHRAM's measured M10K and its numbers SURVIVE a correctness repair. A
  // second, unrelated change to the same array in the same map pair would make
  // that comparison unreadable -- two variables, one measurement. The
  // nonblocking read below still yields the OLD word, which is byte-identical
  // to the combinational flop read it replaced, and that equality is still the
  // whole equivalence argument. Taking the attribute is an area question for a
  // packet that can measure it alone.
  (* ramstyle = "M10K" *) logic [UniW-1:0] b_uni [0:MAX_FIELDS-1];
  // THE READ REGISTER, WHICH IS THE 15.1 CAPTURE LATCH ITSELF. It is not an
  // extra pipeline stage bolted in front of one: `held_in` is GONE, and this
  // register holds its job. See THE CAPTURE below for why that costs no cycle.
  logic [UniW-1:0] b_uni_q;

  // THE CONTROL WORD STAYS IN FLOPS, DELIBERATELY, AND THAT IS NOT A LEFTOVER.
  // Eighty bits against the payload's 5,120. All three are read on the
  // DECISION cycle -- `b_begun` decides `skip_c`, and the decision is what
  // chooses whether to read the payload at all -- so a synchronous read could
  // not have answered in time. Moving them would cost a state to delete a
  // 16-to-1 mux five bits wide. They keep their reset loop for the same
  // reason: a reset loop is only a problem for an array that must infer.
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
  // ---- THE 15.1 CAPTURE REGISTERS, AND THEY CARRY NO RESET ------------------
  // Every one is loaded by `cap_en_c` and read only from `E_REQ` onward, and
  // `cap_en_c` is the sole gate on reaching `E_REQ`, so an unreset value can
  // never be published on a port. A reset on `b_uni_q` would additionally cost
  // the M10K its own output register and put 320 flops back.
  logic signed [31:0] cap_wx, cap_wz;
  // The address the payload was ACTUALLY read at, kept so arm (c) of the
  // shadow guard has something to difference `cur_lane` against. See there.
  logic [BankAw-1:0] cap_a;
  logic signed [31:0] a_height, a_velocity, a_nav;
  logic [31:0] a_material;
  logic a_field;
  // Latched beside the four words, for the reason `ans_present_o`'s port
  // comment gives: a live read here is the metadata-swap defect.
  logic [3:0] a_present;
  logic [SLOTW-1:0] cap_slot;
  // The engine's own "there is no program here". `zhao_field_host_v2` ORs it
  // with its `!hdr_loaded[slot]` test, so a lane whose handle resolved to no
  // ready object and a lane whose object holds no header BOTH come back 0xF0
  // and are counted in one place.
  logic cap_noprog;

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
  // IT IS ASSEMBLED FROM THE CAPTURE REGISTERS AND FROM NO LIVE PIN. The bank
  // is NOT re-read here: `b_uni_q` was loaded once, by `cap_en_c`, on the
  // cycle the state machine left `E_IDLE`, and it is held for the whole run.
  // That is what makes this wiring and not a second read.
  logic [IN_LANES*32-1:0] req_in_c;
  always_comb begin
    req_in_c = '0;
    req_in_c[(0*32) +: 32] = cap_wx;                          // x:fx -- VERTEX
    req_in_c[(1*32) +: 32] = cap_wz;                          // z:fx
    req_in_c[(2*32) +: 32] = b_uni_q[UniAgeLo   +: 32];       // age:u32  R2
    req_in_c[(3*32) +: 32] = b_uni_q[UniPhaseLo +: 32];       // phase:fx R3
    req_in_c[(4*32) +: 32] = b_uni_q[UniParLo + (0*32) +: 32];   // p0   R4
    req_in_c[(5*32) +: 32] = b_uni_q[UniParLo + (1*32) +: 32];   // p1   R5
    req_in_c[(6*32) +: 32] = b_uni_q[UniParLo + (2*32) +: 32];   // p2   R6
    req_in_c[(7*32) +: 32] = b_uni_q[UniParLo + (3*32) +: 32];   // p3   R7
    req_in_c[(8*32) +: 32] = b_uni_q[UniParLo + (4*32) +: 32];   // p4   R8
    req_in_c[(9*32) +: 32] = b_uni_q[UniParLo + (5*32) +: 32];   // p5   R9
    req_in_c[(10*32) +: 32] = b_uni_q[UniParLo + (6*32) +: 32];  // p6   R10
    req_in_c[(11*32) +: 32] = b_uni_q[UniParLo + (7*32) +: 32];  // p7   R11
    // lane 12 is the HOST's thirteenth, which the earth profile does not have.
    // It is zero rather than absent because the port is IN_LANES wide for every
    // client; the engine takes the profile's arity from the program.
  end

  assign req_in_o = req_in_c;
  assign req_slot_o = cap_slot;
  assign req_noprog_o = cap_noprog;
  assign req_valid_o = (state == E_REQ);
  assign resp_ready_o = (state == E_WAIT);

  assign ans_valid_o = (state == E_ANS);
  assign ans_field_o = a_field;
  assign ans_present_o = a_present;
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

  // The four canonical ordinals' presence, read ONLY in E_WAIT beside the words
  // it describes -- the same cycle, the same response, the same `resp_valid_i`.
  // Ordinals 4..6 exist on the shared seven-wide seam and belong to other
  // profiles; this adapter's own guard refuses OUT_LANES < 4 at elaboration, so
  // the slice is total rather than hopeful.
  wire [3:0] present_c = resp_present_i[3:0];

  // R168's case: the host called the run OK and at least one declared-by-this-
  // profile ordinal is a hole. NOT a fault (13.3), so it is counted apart from
  // `faults_o` -- and it is a genuinely reachable state under legal stimulus,
  // because `complete_c` in the host tests the HEADER's declared mask and not
  // these four, so a two-ordinal Earth program retires StOk by construction.
  wire short_record_c = status_ok && (present_c != EarthOrdinals);

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

  // ==========================================================================
  // THE INTAKE/REPLAY INTERLOCK (EARTHLOCK, 2026-09-25). THE ENTRY UNDER
  // CONSTRUCTION IS NOT READABLE BY THE LANE STREAM.
  // ==========================================================================
  // `rec_ready_o` is the FIRST beat of the intake and `b_begun`/`b_uni` land
  // EIGHTEEN CLOCKS LATER at I_WR, because `phase` is the divider's result and
  // cannot exist before the divide ends. So there is a window in which the
  // console believes record N is in -- the joined handshake told it so -- while
  // entry N still holds the PREVIOUS frame's age, phase and parameters. A
  // vertex reading it in that window is a real engine run on stale uniforms,
  // and it moves NO counter: `noprog_o` is silent, both arms of the lane shadow
  // difference entry COUNTS and the count is right, and `lane_desync_o`'s arm
  // (c) pairs the read's own address with itself. NOTHING IN THE TREE SEES IT.
  // That is the half of this defect that had to be closed structurally rather
  // than watched.
  //
  // THE CLOSE IS TO MAKE THE ENTRY UNREADABLE WHILE IT IS BEING WRITTEN, which
  // is a HOLD and not a wrong answer. The consumer already waits on this module
  // for order 80-100 clocks per covered vertex, so at most eighteen more is
  // inside the protocol it already speaks, and the cost lands in
  // `stall_cycles_o` -- the counter that already exports exactly this number.
  //
  // IT IS PER-ENTRY AND NOT A GLOBAL BAR, and that is a liveness requirement
  // rather than an optimisation. A global `in_st != I_TAKE` would stop the lane
  // on EVERY record, and a saturated record stream -- which is legal, the tail
  // reject is what bounds it -- holds `in_st` off `I_TAKE` for eighteen of
  // every nineteen clocks. Per-entry, the intake walks `n_rec` forward, so any
  // one entry is blocked for at most one record's worth of the sweep.
  //
  // A REJECTED RECORD HOLDS NOTHING, and this is the trap in the obvious
  // version. `wr_a` is `n_rec[BankAw-1:0]` with `BankAw` = 4 and `n_rec` five
  // bits, so the rejecting value `n_rec == 16` ALIASES TO ADDRESS 0. A hold
  // that ignored the reject would starve entry 0 for as long as the tail
  // rejects kept arriving. A reject writes no bank, so it owes no hold.
  //
  // AND THE ADDRESS IS `eff_n_c`, NOT `wr_a`, on the take cycle. `wr_a` reads
  // `n_rec`, which the take edge itself resets to zero when the list reopens --
  // so during that one cycle `wr_a` still names the PREVIOUS list's tail.
  // `eff_n_c` is the value `would_reject_c` is already judged on, and it is the
  // entry the clear below actually lands in.
  wire [BankAw-1:0] intake_a_c = take_now_c ? eff_n_c[BankAw-1:0] : wr_a;
  wire intake_writes_c = take_now_c ? !would_reject_c : ((in_st != I_TAKE) && !h_reject);
  wire entry_busy_c = intake_writes_c && (lane_a == intake_a_c);

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

  // ==========================================================================
  // THE BANK ITSELF: ONE SYNCHRONOUS-READ RAM, ONE CAPTURE ENABLE, NO RESET
  // ==========================================================================
  // WHY THIS COSTS NO EXTRA STATE, which is the one thing about this change a
  // reader will not believe. A synchronous read hands its data over one cycle
  // after the address, so the obvious shape is a new state between `E_IDLE`
  // and `E_REQ`. It is not needed, because THE CYCLE THE ADDRESS IS PRESENTED
  // IS ALREADY A CYCLE IN WHICH NOTHING IS OFFERED: `req_valid_o` is
  // `(state == E_REQ)`, and `state` leaves `E_IDLE` on the very edge that
  // loads `b_uni_q`. So the data lands exactly as `req_valid_o` rises. The
  // owner's estimate allowed "one more state on an 80-100 cycle run"; the
  // measured cost is ZERO, and the directed test's cycle census says so.
  //
  // THE WHOLE REQUEST IS ONE ATOMIC CAPTURE, and that is CLAUDE.md's
  // metadata-swap law applied on the generation side rather than the checking
  // side. The payload, the vertex, the slot and the no-program bit are loaded
  // by ONE enable on ONE edge. There is no arrangement of stalls in which the
  // engine can be handed one lane's uniforms with another lane's slot, because
  // nothing downstream re-reads anything: the defect that law describes needed
  // a bank whose read was registered UNCONDITIONALLY, and `cap_en_c` is the
  // opposite of unconditional.
  //
  // NO RESET ON THIS PROCESS, and that is required rather than tidy: an M10K
  // has no reset port, and the per-element reset loop that used to zero this
  // payload was itself the SECOND WRITE ADDRESS that
  // `tools/quartus/check_ram_inference.py` names as a blocker. Removing it is
  // half the change; making the read synchronous is the other half, and either
  // alone leaves the array in flip-flops.
  // `!entry_busy_c` is here for the SAME reason it is on the `E_IDLE` branch
  // below, and the two must stay the same expression: this wire is that
  // branch's condition restated, and the header's atomicity argument depends on
  // that being literally true.
  wire cap_en_c = (state == E_IDLE) && lanes_left_c && !skip_c;  // MUTANT: no interlock
  wire uni_we_c = (in_st == I_WR) && !h_reject;

  // The oracle: `duration == 0 ? 1.0fx : the divide`. The divider ran anyway
  // (see the reject note in I_WR) and its answer on a zero denominator is
  // discarded here rather than guarded there, so the divide has one shape and
  // no special case inside the loop. Assigned to a 32-bit unsigned target,
  // which is the identical context the old `b_phase[wr_a] <= ...` had.
  logic [31:0] uni_phase_wd_c;
  always_comb begin
    uni_phase_wd_c = h_dur_zero ? PHASE_ONE : {15'd0, dv_q};
  end
  wire [UniW-1:0] uni_wd_c = {h_par, uni_phase_wd_c, h_age};

  always_ff @(posedge clk) begin
    // ONE write address. `wr_a` and nothing else -- see the reset block.
    if (uni_we_c) begin
      b_uni[wr_a] <= uni_wd_c;
    end
    // ONE read address, registered. Nonblocking, so a same-cycle write to the
    // same address yields the OLD word: exactly what the combinational read of
    // a flop array did, which is why this is an equivalence and not a change.
    if (cap_en_c) begin
      b_uni_q    <= b_uni[lane_a];
      cap_wx     <= held_wx;
      cap_wz     <= held_wz;
      cap_slot   <= b_obj[lane_a];
      cap_noprog <= !b_res[lane_a];
      cap_a      <= lane_a;
    end
  end

  integer i;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      // ONLY THE CONTROL WORD IS RESET, AND THE OMISSION IS THE POINT.
      // A per-element reset loop is a second dynamic write address, which no
      // single-port memory on this device has, so `[i]` beside `[wr_a]` is
      // what kept the payload in flip-flops however synchronous the read was.
      // Nothing reads an unwritten payload entry: `cur_lane < held_lanes`, and
      // `held_lanes` is the consumer's `fields_active_o`, which counts entries
      // this module watched replayed -- every one of which was banked first.
      // Arm (b) of the shadow guard is the check that says so if it stops.
      for (i = 0; i < MAX_FIELDS; i = i + 1) begin
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
      // `held_in`, `held_slot` and `held_noprog` are gone from here: they are
      // the capture registers above, in a process with no reset, for the
      // reason stated at their declaration.
      a_height <= 32'sd0;
      a_velocity <= 32'sd0;
      a_material <= 32'd0;
      a_nav <= 32'sd0;
      a_field <= 1'b0;
      a_present <= 4'd0;
      short_record_o <= 32'd0;
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
      // ARM (c), NEW WITH THE SYNCHRONOUS BANK, AND IT IS THE ARM THAT LAW
      // EXISTS FOR. A registered read means the payload arrives a cycle after
      // its address, which is the exact shape of CLAUDE.md's metadata-swap
      // defect -- "response A's data, A's token, and B's metadata", with every
      // accepted/emitted counter balancing because none of them looks at the
      // field that moved. This one looks at it.
      //
      // ASK WHAT CLOCKS EACH SIDE, which is that chapter's first instruction.
      // `cap_a` is loaded by `cap_en_c`, in the RESET-FREE RAM PROCESS above.
      // `cur_lane` is loaded IN THIS PROCESS, by the consumer's `ans_ready_i`
      // handshake in `E_ANS` and by `vtx_fire_i`. Two processes, two
      // separately authored enables, and NO register enable drives both -- so
      // this is not the blind detector that chapter is about, and it can see a
      // TIMING fault and not merely a value one.
      //
      // IT IS SILENT WHILE THE DESIGN IS CORRECT, because `cur_lane` cannot
      // move while `state` is `E_REQ`. It is nonetheless reachable from the
      // block's own boundary -- a consumer that fires a vertex underneath a
      // request in flight resets `cur_lane` while `cap_a` holds the lane the
      // payload was actually read at -- so it owes no committed mutant, and
      // `tests/field/field_earth_adapter_directed.cpp` case 9d fires it.
      if ((state == E_REQ) && (cap_a != lane_a)) begin
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

            // THE STALE-BINDING CLEAR HAPPENS HERE, AT ACCEPTANCE, AND NOT AT
            // I_WR EIGHTEEN CLOCKS LATER (EARTHLOCK, 2026-09-25).
            //
            // Its purpose is unchanged and is stated at the declaration: a
            // fresh entry is NOT resident until the replay says so, because
            // inheriting the previous frame's answer for this slot would
            // evaluate a program the cartridge replaced. That purpose is served
            // exactly as well at acceptance -- and acceptance is the beat the
            // REST OF THE CONSOLE IS SYNCHRONISED TO, because `rec_ready_o` is
            // joined with `zhao_terrain_fieldlist`'s `cmd_ready_o` and that
            // join is the whole argument for the two blocks' entry indices
            // being one number. Clearing at I_WR made that argument true in
            // VALUE and false in TIME: the field list would seal, the patch
            // replay would set `b_res[N]`, and this module's late clear would
            // WIPE IT -- measured as 1,089 engine runs with `noprog_o` == 1,089
            // and every other census balancing perfectly.
            //
            // `eff_n_c` AND NOT `wr_a`: on a reopened list the take edge is the
            // edge that resets `n_rec`, so `wr_a` still names the previous
            // list's tail for this one cycle. See `intake_a_c` above.
            //
            // GATED ON THE REJECT, because `eff_n_c == 16` aliases to address 0
            // and a tail reject must not clear entry 0's binding. A rejected
            // record writes no bank at I_WR either; it is walked through the
            // divide for the timing reason below and banks nothing.
            //
            // THE REPLAY OUTRANKS THIS CLEAR ON A TIE. The per-patch replay
            // block sits LATER IN THIS SAME PROCESS, so if `add_fire_i` lands
            // on this very edge for this very entry, its nonblocking assignment
            // is the one that survives -- and that is the correct precedence,
            // since the replay carries the resolution this clear is waiting
            // for.
            // MUTANT: the clear is not here; it is back at I_WR.

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
            // THE PAYLOAD IS WRITTEN BY THE RAM PROCESS ABOVE, off `uni_we_c`,
            // which is this branch's own condition restated as a wire. It is
            // not written twice and it is not written there conditionally on
            // something else: `uni_we_c` is `(in_st == I_WR) && !h_reject`,
            // and this is the `!h_reject` arm of `I_WR`.
            b_begun[wr_a] <= h_begun;
            // MUTANT: the pre-repair late clear, restored. This is the defect.
            b_res[wr_a] <= 1'b0;
            b_obj[wr_a] <= {OBJW{1'b0}};
            // `b_res[wr_a] <= 1'b0;` AND `b_obj[wr_a] <= '0;` USED TO BE HERE
            // AND HAVE MOVED TO I_TAKE. They are not gone and their purpose is
            // not withdrawn -- see the block comment at the clear's new home
            // for why acceptance is the correct beat and this one was not.
            // Nothing else is written here that was not written here before:
            // this arm now banks ONLY what the divide had to finish for, which
            // is `b_begun` and the payload `uni_we_c` writes.
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
          // `!entry_busy_c` HOLDS THE LANE while this entry's own intake is in
          // flight. It gates the WHOLE branch and not just the request arm,
          // because `skip_c` reads `b_begun[lane_a]` and that flop is written
          // at I_WR too -- a skip decided on a half-written entry is the same
          // stale read wearing the oracle's `continue` as a disguise.
          if (lanes_left_c) begin  // MUTANT: no per-entry hold
            // 15.1's CAPTURE HAPPENS IN THE RAM PROCESS ABOVE, off `cap_en_c`,
            // which is `(state == E_IDLE) && lanes_left_c && !skip_c` -- this
            // branch's own condition. One enable loads the payload, the
            // vertex, the slot and the no-program bit together, so nothing
            // downstream re-reads the bank and the consumer advancing its lane
            // counter cannot move this run's operands.
            if (skip_c) begin
              // The oracle's `continue`, expressed as the additive zero the
              // consumer's fx_add chain treats identically. Counted by CAUSE.
              a_field <= 1'b0;
              a_present <= 4'd0;  // a `continue` writes no lane at all
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
              // A HOLE IS PUBLISHED AS A ZERO *AND* DECLARED ABSENT, which are
              // two different statements and both are needed. The zero is
              // belt-and-braces over the host's grant clear, so this module's
              // output cannot carry a stale word from an earlier response even
              // if that clear ever changed; `a_present` is the part a consumer
              // reads to tell the zero from a value. Publishing the word
              // without the bit is the defect this commit repairs, and
              // suppressing the word without the bit would merely move it.
              a_present <= present_c;
              a_height <= present_c[0] ? out_height : 32'sd0;
              a_velocity <= present_c[1] ? out_velocity : 32'sd0;
              a_material <= present_c[2] ? out_material : 32'd0;
              a_nav <= present_c[3] ? out_nav : 32'sd0;
              if (runs_o != 32'hFFFF_FFFF) runs_o <= runs_o + 32'd1;
              // Counted on the SAME condition that shaped the words above, so
              // the number and the behaviour cannot drift apart.
              if (short_record_c && (short_record_o != 32'hFFFF_FFFF)) begin
                short_record_o <= short_record_o + 32'd1;
              end
            end else begin
              // A refused run contributes NOTHING, which is the oracle's
              // `prog == nullptr -> continue`, not a zero height standing in
              // for a height that was never computed.
              a_field <= 1'b0;
              a_present <= 4'd0;  // a refused run wrote no lane at all
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
