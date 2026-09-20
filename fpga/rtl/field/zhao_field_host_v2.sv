// zhao_field_host_v2.sv — THE ASSOCIATION-AWARE FIELD HOST.
//
// Contract: design/contracts/FIELD.SEQ.CORE.md
// Schema:   fpga/rtl/field/generated/zhao_field_host_image_pkg.sv (packet S1)
// Oracle:   fpga/rtl/field/zhao_field_host.sv -- RETAINED, UNTOUCHED, and it
//           lives in tests/ source lists ONLY. It is never in a production
//           source list, because `completion_register.py`'s
//           `superseded_in_closure()` would then fire on the v2/unversioned
//           pair and the "only the latest version composes" ruling would be
//           breached. That check reads 71 production roots CLEAN and must stay
//           that way.
//
// ENFORCED-BY: tests/field/field_host_v2_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHY A NEW FILE RATHER THAN AN EDIT
// ---------------------------------------------------------------------------
// `zhao_field_host` is the named oracle for this block (FH02). Its completion
// rule, its status namespace and its one-point-in-flight run loop are the
// behaviour every existing adapter and differential was written against. The
// repairs below change the MEANING of a successful response -- from "some
// write landed inside a contiguous window" to "every declared canonical output
// ordinal has a value that this point produced" -- and a change of meaning
// wants a second implementation standing beside the first, not a rewrite of it.
//
// ---------------------------------------------------------------------------
// THE ONE THING THIS FILE IS ABOUT: ORDINAL IS NOT WINDOW
// ---------------------------------------------------------------------------
// R101 shipped a required-output mask indexed by CONTIGUOUS CAPTURE-WINDOW
// POSITION: bit k means "physical register out_base + k was written". R111 then
// measured the three shipped Earth programs and found window masks
// 0x17 / 0x1D / 0x17 -- every one of them leaving three of the console's seven
// window lanes unwritten, because output registers are NOT contiguous and the
// capture window is. HOLES ARE THE NORMAL CASE.
//
// The quantity a caller actually asked for is the CANONICAL OUTPUT ORDINAL:
// bit j means "canonical output j of this profile is declared". crater_ring
// writes R13,R14,R15,R17 with out_base=13 -- window mask 0x17, ordinal mask
// 0x0F. A window mask can detect a MISSING write. It cannot compact window
// slots 0,1,2,4 into canonical result slots 0,1,2,3, and nothing that indexes
// by window position can return results in declared output order.
//
// So this host keeps BOTH and never confuses them:
//
//   * `hdr_winmask[slot]` is OUT_LANES wide, window-indexed. It survives from
//     R101 and is used for exactly one thing: telling a caller that the lanes
//     of the window it did not declare are PADDING rather than missing results
//     (FT017).
//   * `hdr_reqmask[slot]` is OUT_ORDINALS wide, ordinal-indexed. It is the
//     completion rule.
//   * `omap_kind[slot][j]` / `omap_index[slot][j]` ARE the translation -- the
//     OUTPUT_MAP of the ZFH2 image, one row per ordinal. Nothing else in this
//     file converts between the two spaces.
//
// The generated schema gives the two masks distinct TYPES and distinct WIDTHS
// (7 and 8) so a cross-assignment is a width diagnostic rather than a silent
// truncation. This file never places them in one expression.
//
// ---------------------------------------------------------------------------
// COMPLETION IS `ALL`, AND IT IS PER ORDINAL
// ---------------------------------------------------------------------------
//   seen_next  = seen | writes_granted_this_clock | valid_uniform_seeds
//   complete   = ((seen_next & required) == required) && fenced
//
// `cur_seen[j]` is set by a GRANTED register write whose register equals
// `omap_index[slot][j]`, which handles aliasing for free: two ordinals naming
// one register are both satisfied by one write (FT023). `cur_export[j]` is
// indexed by ORDINAL, so `resp_out_o` comes back in declared output order
// whatever the physical registers were (FT020).
//
// ---------------------------------------------------------------------------
// A UNIFORM OUTPUT IS A RESULT (FH06)
// ---------------------------------------------------------------------------
// The oracle computes `out_none_c = (cur_out_seen == '0)` and answers
// ST_NO_RESULT. That is correct for a vector program that wrote nothing and
// WRONG for a plan whose answer is a prepared scalar: an all-uniform program
// never generates a vector write, so under the oracle's rule it can only ever
// refuse, or hang waiting for a write that will never come.
//
// This is not an argument. Packet L1 lowered all three shipped Earth programs
// and measured it: crater_ring has 2 uniform outputs of 4, impact_wave 1,
// wave_pool 1, and every one of them declares ordinal mask 0x0F with strictly
// fewer window bits. A uniform output is a RESULT.
//
// So an ordinal whose OUTPUT_MAP row says PREPARED_SCALAR is SEEDED AT POINT
// START from the prepared-scalar file, and `cur_seen[j]` is set there -- under
// two conditions that are checked and not assumed:
//
//   * the prepared slot's `prep_valid` bit is set (directive 7.3: "validate the
//     scalar's written bit ... at association seal"). Zero is a fully valid
//     value if that bit holds, which is the whole reason validity is a
//     SEPARATE BIT and never inferred from the data (FT018 vs FT019);
//   * `prep_gen` matches this association's generation (directive 6.4: "do not
//     reuse a previously loaded uniform register unless its
//     association-generation tag matches. A matching program hash is not a
//     matching parameter set").
//
// A PREPARED_SCALAR ordinal that fails either check is NOT seeded, so it is
// never seen, so the point refuses with ST_BAD_PREPARATION. Zero-filled RAM is
// not a substitute for a preparation (FT038).
//
// ---------------------------------------------------------------------------
// A MASK ALONE CANNOT CATCH A MISSING FINAL OVERWRITE -- THE FENCE
// ---------------------------------------------------------------------------
// Directive 7.4 states it and the oracle demonstrates it. In
// `zhao_field_host.sv` the verdict is computed in the same `always_ff` clock as
// END is observed, reading `cur_out_seen` BEFORE the non-blocking update lands.
// Two consequences, and they are the two cases a mask can never see:
//
//   * a write arriving in the same clock as END is not counted (FT024);
//   * a write arriving AFTER END is ignored entirely, and because `seen` was
//     already set by an earlier write to the same register, the point publishes
//     the STALE EARLIER VALUE and reports success (FT025).
//
// The repair is a real drain fence and not a guessed delay -- "a fixed guessed
// 'wait five clocks after END' is not a fence and fails with service
// backpressure". On END the run enters E_DRAIN and KEEPS CAPTURING granted
// writes. It leaves E_DRAIN only when the fabric says this context has no work
// that can still write. The verdict is computed on the NEXT-STATE sets, so a
// write coincident with END is included exactly once and a delayed final write
// replaces the earlier value before anything is published.
//
// `late_write_o` is the fence's own positive control: it counts a granted write
// for the retired context arriving after the fence released. It must be zero,
// and "it reads zero" is a claim -- `tests/mutants/` carries the driver that
// makes it move.
//
// ---------------------------------------------------------------------------
// WHY THE PER-POINT CLEAR CAN GO, AND WHAT MAKES IT SAFE (FH08)
// ---------------------------------------------------------------------------
// The oracle walks REGS registers writing zero, once per point, and its comment
// says "It is not optional". It is optional -- under a proof, and only under a
// proof. Directive 6.1: for a validated canonical program every instruction
// source is an input or was defined by a preceding op, no instruction writes an
// input register, every declared output is defined, and the program is
// straight-line to a validated END. Therefore previous-point scratch cannot
// affect a legal execution.
//
// The proof is a PROPERTY OF THE IMAGE, so this host does not re-derive it: the
// ZFH2 INIT_PROOF record carries `initial_defined_mask` and `vector_write_mask`
// and the C++ validator walks them. What the hardware owns is the INTERLOCK --
// `hdr_ipok[slot]` is set only by an accepted INIT_PROOF load, and the fast
// path is taken only when it is set. `cfg_slow_clear_i` forces the walk back on
// for the differential, which is exactly what directive 6.5 asks for: the same
// legal corpus through both forms, from independently randomized initial RF
// contents.
//
// What still resets per point is the small control state directive 6.4 names:
// seen/export bookkeeping, point-local status, the pending terminal and the
// point identity. That is bookkeeping, not a 64-word payload clear.
//
// ---------------------------------------------------------------------------
// THREE IDENTITIES (FH03)
// ---------------------------------------------------------------------------
// The oracle has ONE: `cur_slot`, which the client supplies raw. This host
// separates the three the directive names:
//
//   PROGRAM     -- `slot`, the residency index; still the executor's context.
//   ASSOCIATION -- `hdr_assoc_gen[slot]`, the frozen-uniform generation. It is
//                  what makes "same program, new parameters" a DIFFERENT thing
//                  to run (FT036), and it is what the prepared-scalar file is
//                  checked against.
//   CONTEXT     -- `cur_*`, the running point, whose lifetime ends at the fence
//                  and NOT at response delivery.
//
// ---------------------------------------------------------------------------
// ONE ACTIVE PREPARED-DATA DOMAIN (FH09), AND ITS HONEST LIMIT
// ---------------------------------------------------------------------------
// `zhao_field_v3_sbank` is one flat 64-slot array with no program namespace, so
// two associations with the same scalar indices would silently read each
// other's numbers. Replicating the bank per context is the obvious fix and it
// is not affordable -- the console is already 5,672 ALM over.
//
// What is affordable and honest is EXCLUSIVE OWNERSHIP: the prepared file
// carries a generation tag per slot, and a read whose tag does not match the
// running association's generation is a refusal rather than a wrong number.
// That gives ONE ACTIVE DOMAIN with many running points, which is what FH09
// asks for -- and it is NOT per-context uniforms.
//
// SAID PLAINLY, BECAUSE THE DIFFERENCE MATTERS TO WARP: this satisfies R103's
// P5 only if Warp never interleaves with Earth inside a frame. P5 is DEFERRED
// WITH A MEASURED JUSTIFICATION, never closed.
//
// ---------------------------------------------------------------------------
// CREDIT BEFORE ACCEPTANCE (FH20)
// ---------------------------------------------------------------------------
// Directive 7.5: reserve an output slot BEFORE accepting a point; once the
// result is copied to that reserved slot and internal work is drained, the
// execution context may be reused even if the application has not consumed the
// response. The oracle holds the whole machine in E_RESP until the client takes
// its answer, which couples context lifetime to consumer behaviour.
//
// Here a response entry is reserved at grant and released at delivery. A client
// that stops consuming spends its own finite credits and stalls itself; it does
// not hold the fabric. `credit_stall_o` counts the grants that could not be
// made for want of a reserved slot, so the isolation is measured and not
// asserted.
//
// ---------------------------------------------------------------------------
// NUMERIC STATUS IS FOUR CAUSES, NOT THREE
// ---------------------------------------------------------------------------
// The oracle's `sat_o` is `[2:0]` = {sat_rescale, sat_mul, sat_add}. rcp0 has
// no bit there. `num_status_o` is four bits: {rcp0, sat_rescale, sat_mul,
// sat_add}, with rcp0 in its own family exactly as directive 8.1 requires
// (`numeric_rcp0` separate from `numeric_sat`) -- a reciprocal of zero is a
// DEFINED ANSWER, not a saturation and not a fault, and folding it into either
// is one of the two wrong things to do with it.
//
// THE PRODUCER CHAIN WAS INCOMPLETE WHEN THIS FILE WAS WRITTEN. IT IS COMPLETE
// NOW -- packet C1, owner ruling R145, 2026-09-20. This paragraph is amended
// rather than deleted because the shape of the repair is the useful part.
//
// As written, rcp0 had no port on `zhao_field_v3_svcpath`,
// `zhao_field_v3_dispatch` or `zhao_field_v3_engine`; it stopped dead inside
// svcpath as `nm_rcp0_unconsumed` behind an UNUSEDSIGNAL waiver. C1 carried it
// out along the `sat_rescale` template, and `.rcp0_o(fab_rcp0)` on `u_fabric`
// below is the connection.
//
// R145 SAID FOUR FILES AND IT IS THREE. `zhao_field_v3_core` and
// `zhao_field_v3_exec` contain no rcp0 signal at all, because the reciprocal
// that can be handed a zero lives in `zhao_field_v3_normalize` on the SERVICE
// path and never under core. Adding a port to core to make the two ledgers
// look symmetrical would have been a port with nothing driving it -- a tie-off
// created while closing a gap, which is the one move rule 1 forbids outright.
// Measured by grep across `fpga/rtl/field/`, not inferred from the ruling.
//
// `rcp0_i` IS RETAINED rather than removed. It was a real INPUT PORT so that
// connecting it would be a wiring act with a visible unconnected end; now that
// the end is connected, the port stays as the bench's way to force the cause
// without reaching inside the fabric, and the accumulator ORs the two. The
// console drives it low, so on the composed machine the fabric's own bit is
// what reports the event. See FINDINGS-fieldc1 for the chain as built.

`default_nettype none

module zhao_field_host_v2
  import zhao_field_host_image_pkg::*;
#(
    // ---- IDENTITY AND CAPACITY ---------------------------------------------
    parameter int unsigned CLIENTS = 2,
    // Resident programs = the executor's contexts = this block's slot space.
    parameter int unsigned PROGS = 8,
    parameter int unsigned INSTR_N = 32,
    parameter int unsigned REGS = 32,
    parameter int unsigned TABLES = 2,
    parameter int unsigned TBL_N = 64,

    // The canonical E record in, the CONTIGUOUS CAPTURE WINDOW, and the
    // CANONICAL OUTPUT ORDINALS. The second and third are different quantities
    // and this parameter list is the first place that has to say so.
    parameter int unsigned IN_LANES = 12,
    // WINDOW width. R126: this must equal ZFH_WINDOW_MASK_BITS, and the guard
    // below is the thing that was missing.
    parameter int unsigned OUT_LANES = 7,
    // ORDINAL count. Flow's seven is the widest profile.
    parameter int unsigned OUT_ORDINALS = 7,

    // The prepared-scalar file. Depth matches `zhao_field_v3_sbank`'s SLOTS;
    // `tools/field/measure_sreg_hwm.cpp` measured the worst plan at 41.
    parameter int unsigned PREP_SCALARS = 64,

    // FH20. Response entries reserved before acceptance.
    parameter int unsigned CREDITS = 2,

    // ---- THE FABRIC'S OWN KNOBS, FORWARDED ---------------------------------
    parameter int unsigned FAB_LANES = 1,
    parameter int unsigned FAB_OUTSTANDING = 4,
    parameter int unsigned FAB_LONGQ = 4,
    parameter int unsigned FAB_GATHERS = 4,
    parameter int unsigned FAB_DIST_BANKS = 2,
    parameter int unsigned FAB_GROUP_PTS = 4,
    parameter int unsigned FAB_RING_UNITS = 2,
    parameter int unsigned FAB_RING_DESC = 2,

    // ---- DERIVED, WRITTEN AS LITERALS ON PURPOSE ---------------------------
    // `tools/quartus/gen_prod_top.py` cannot evaluate a parameter expression
    // when it sizes a port, and a module it cannot size is SKIPPED from the
    // generated top -- silently, and in the flattering direction. Every one is
    // checked against its own expression in the elaboration block.
    parameter int unsigned SLOTW   = 3,  // $clog2(PROGS)        at 8
    parameter int unsigned PCW     = 5,  // $clog2(INSTR_N)      at 32
    parameter int unsigned REGW    = 5,  // $clog2(REGS)         at 32
    parameter int unsigned TSELW   = 1,  // $clog2(TABLES)       at 2
    parameter int unsigned TIDXW   = 6,  // $clog2(TBL_N)        at 64
    parameter int unsigned LDADDRW = 7,  // max(PCW, TSELW+TIDXW)
    parameter int unsigned ORDW    = 3,  // $clog2(OUT_ORDINALS) at 7
    parameter int unsigned PREPW   = 6,  // $clog2(PREP_SCALARS) at 64
    parameter int unsigned CRDW    = 1   // $clog2(CREDITS)      at 2
) (
    input  var logic clk,
    input  var logic rst_n,

    // -----------------------------------------------------------------------
    // CONFIGURATION
    // -----------------------------------------------------------------------
    // FH08's differential control. HIGH forces the blanket per-point register
    // walk back on for every point regardless of the image's INIT_PROOF. It is
    // a real production-visible mode, not a test hook: directive 6.1 keeps the
    // slow clear "for differential testing and explicit legacy unvalidated
    // bench images only". Production may not bypass validation to enter the
    // fast path, which is why the fast path is gated on `hdr_ipok` and NOT on
    // this bit being low.
    input  var logic cfg_slow_clear_i,

    // -----------------------------------------------------------------------
    // THE PROGRAM/ASSOCIATION LOAD PORT
    // -----------------------------------------------------------------------
    // Eight kinds, and `ld_kind_i` is THREE bits where the oracle's is two.
    // The oracle's four kinds are kept at their original encodings so a reader
    // comparing the two files is not also decoding a renumbering:
    //
    //   0 UOP        1 TABLE       2 HEADER      3 UNIFORM
    //   4 OUTMAP     5 ASSOC       6 INITPROOF   7 PREPARED
    //
    // HEADER is still written LAST and is still the write that marks the slot
    // runnable, so a partially described program can never execute. Every other
    // kind clears `hdr_loaded` again.
    input  var logic                       ld_valid_i,
    output var logic                       ld_ready_o,
    input  var logic [              2:0]   ld_kind_i,
    input  var logic [SLOTW-1:0]           ld_slot_i,
    input  var logic [LDADDRW-1:0]         ld_addr_i,
    input  var logic [             95:0]   ld_data_i,

    // -----------------------------------------------------------------------
    // FIELD.PROGCACHE -- the residency directory, exported whole
    // -----------------------------------------------------------------------
    input  var logic                       pc_lu_valid_i,
    output var logic                       pc_lu_ready_o,
    input  var logic [             31:0]   pc_lu_hash_i,
    output var logic                       pc_lu_resp_valid_o,
    input  var logic                       pc_lu_resp_ready_i,
    output var logic                       pc_lu_hit_o,
    output var logic [SLOTW-1:0]           pc_lu_slot_o,

    input  var logic                       pc_cm_valid_i,
    output var logic                       pc_cm_ready_o,
    input  var logic [             31:0]   pc_cm_hash_i,
    input  var logic                       pc_cm_ok_i,
    output var logic                       pc_cm_resp_valid_o,
    input  var logic                       pc_cm_resp_ready_i,
    output var logic                       pc_cm_inserted_o,
    output var logic                       pc_cm_evicted_o,
    output var logic [SLOTW-1:0]           pc_cm_slot_o,

    output var logic [             31:0]   pc_hits_o,
    output var logic [             31:0]   pc_misses_o,
    output var logic [             31:0]   pc_rejected_o,
    output var logic [             31:0]   pc_evictions_o,
    output var logic [SLOTW:0]             pc_occupancy_o,

    // -----------------------------------------------------------------------
    // THE PROFILE CLIENTS
    // -----------------------------------------------------------------------
    input  var logic [CLIENTS-1:0]                 req_valid_i,
    output var logic [CLIENTS-1:0]                 req_ready_o,
    input  var logic [CLIENTS*SLOTW-1:0]           req_slot_i,
    input  var logic [CLIENTS-1:0]                 req_noprog_i,
    input  var logic [CLIENTS*IN_LANES*32-1:0]     req_in_i,

    output var logic [CLIENTS-1:0]                 resp_valid_o,
    input  var logic [CLIENTS-1:0]                 resp_ready_i,
    // INDEXED BY CANONICAL OUTPUT ORDINAL, in declared output order -- NOT by
    // physical register and NOT by window position. That is the whole of FT020
    // and it is why this port is OUT_ORDINALS wide and the oracle's is
    // OUT_LANES wide.
    output var logic [OUT_ORDINALS*32-1:0]         resp_out_o,
    // Directive 8.1's `output_present_mask`, ordinal-indexed. A caller reading
    // only the words cannot tell a value from a hole; this is how it can.
    output var logic [OUT_ORDINALS-1:0]            resp_present_o,
    // THE WINDOW, ACCOUNTED FOR -- window-indexed, and the ONLY window-indexed
    // port on this module. Bit k is set when window lane k was either written
    // or is PADDING (a lane no declared ordinal names). FT017: "padding is not
    // required and is reported as padding, not missing results." A caller that
    // sees a clear bit here is looking at a window lane some ordinal claimed
    // and nothing wrote, which is a different thing from an undeclared lane.
    output var logic [OUT_LANES-1:0]               resp_window_o,
    // The retired point's DECLARED OUTPUT COUNT. FH05 names count, mask and
    // source map together, and the plan measured that this tree had the mask
    // and neither of the other two. An adapter cannot interpret `resp_out_o`
    // without the arity, and inferring it by counting present bits is wrong by
    // construction on a partial result -- which is the case where it matters.
    output var logic [3:0]                         resp_count_o,
    output var logic [7:0]                         resp_status_o,

    // -----------------------------------------------------------------------
    // NUMERIC STATUS FROM THE FABRIC
    // -----------------------------------------------------------------------
    // rcp0 arrives as a PORT because its producer chain above this file is
    // incomplete and an input with an unconnected end is visible, where a bit
    // this file invented would not be. See the header.
    input  var logic                       rcp0_i,

    // -----------------------------------------------------------------------
    // counters and traces
    // -----------------------------------------------------------------------
    output var logic [31:0] runs_o,             // points that retired complete
    output var logic [31:0] run_faults_o,
    output var logic [31:0] noprog_o,
    output var logic [31:0] instr_retired_o,
    output var logic [31:0] loads_o,
    output var logic [31:0] load_defers_o,
    output var logic [31:0] grants_o,
    output var logic [31:0] contended_grants_o,
    output var logic [31:0] ld_oob_o,
    // A point that produced NO declared ordinal at all.
    output var logic [31:0] no_result_o,
    // A point that produced SOME but not ALL declared ordinals. The failure a
    // caller cannot see, because the absent ones read as the zero this block
    // cleared them to.
    output var logic [31:0] out_incomplete_o,
    // A PREPARED_SCALAR ordinal whose prepared slot was invalid, or whose
    // generation did not match the running association. Distinct from
    // `out_incomplete_o` because "the preparation is wrong" and "the program
    // did not write" send the next person to different files.
    output var logic [31:0] prep_bad_o,
    // A load-time refusal: an OUTPUT_MAP row naming a VECTOR_REG outside
    // [out_base, out_base+OUT_LANES), which the capture window cannot observe,
    // so the ordinal could never be seen. Section 2 case 2 of the plan.
    output var logic [31:0] bad_image_o,
    // A strict descriptor whose ordinal required-mask is ZERO. R111's standing
    // hazard: a plan writer who omits the mask silently restores the R101
    // defect and passes every gate. Zero is a REFUSAL here, never "accept
    // whatever happened".
    output var logic [31:0] zero_mask_o,
    // A granted write for the retired context arriving AFTER the fence
    // released. This is the fence's own positive control and it must read zero.
    output var logic [31:0] late_write_o,
    // Writes captured during E_DRAIN -- the ones the oracle loses. Not a fault:
    // this is the measurement that the fence is doing work.
    output var logic [31:0] fence_writes_o,
    // Points that retired with no vector write at all because every declared
    // ordinal was a prepared scalar. FH06's terminal path, counted so that
    // "the uniform route is live" is a number rather than an argument.
    output var logic [31:0] uniform_runs_o,
    // Grants refused for want of a reserved response entry (FH20).
    output var logic [31:0] credit_stall_o,
    // Points that took the no-clear fast path, and points that walked.
    output var logic [31:0] fast_path_o,
    output var logic [31:0] slow_path_o,

    output var logic [31:0] exec_desync_o,
    output var logic [31:0] bank_desync_o,
    output var logic [31:0] svc_bank_desync_o,
    output var logic [31:0] tag_mismatch_o,
    output var logic [31:0] wrong_op_o,
    output var logic [31:0] unsupported_o,
    output var logic [31:0] skid_overflow_o,
    output var logic [31:0] uniform_bad_o,
    // {rcp0, sat_rescale, sat_mul, sat_add}. FOUR causes; see the header.
    output var logic [ 3:0] num_status_o
);

  localparam int unsigned CIDW  = (CLIENTS   > 1) ? $clog2(CLIENTS)   : 1;
  localparam int unsigned LANEW = (IN_LANES  > 1) ? $clog2(IN_LANES)  : 1;
  // The OUTPUT_MAP's source_index has to hold a physical register OR a
  // prepared-scalar slot, so it is as wide as the wider of the two.
  localparam int unsigned IDXW  = (REGW > PREPW) ? REGW : PREPW;
  // The WINDOW position's index width. Guarded against OUT_LANES = 1, where
  // `$clog2(1)` is zero and a zero-width part-select is not expressible.
  localparam int unsigned WINW  = (OUT_LANES > 1) ? $clog2(OUT_LANES) : 1;

  localparam logic [2:0] LdUop       = 3'd0;
  localparam logic [2:0] LdTable     = 3'd1;
  localparam logic [2:0] LdHeader    = 3'd2;
  localparam logic [2:0] LdUniform   = 3'd3;
  localparam logic [2:0] LdOutMap    = 3'd4;
  localparam logic [2:0] LdAssoc     = 3'd5;
  localparam logic [2:0] LdInitProof = 3'd6;
  localparam logic [2:0] LdPrepared  = 3'd7;

  // ==========================================================================
  // THE STATUS NAMESPACE
  // ==========================================================================
  // The oracle's 0xF0..0xF3 are PRESERVED AT THEIR VALUES, because two adapters
  // already decode them and directive 8.2's note that the legacy namespace must
  // be translated explicitly cuts both ways: renumbering them here would make
  // the bridge silently wrong. The new causes take fresh codes above them.
  localparam logic [7:0] StOk          = 8'h00;
  localparam logic [7:0] StNoProgram   = 8'hF0;
  localparam logic [7:0] StNoResult    = 8'hF1;
  localparam logic [7:0] StAlarm       = 8'hF2;
  localparam logic [7:0] StPartial     = 8'hF3;
  // A declared ordinal whose prepared scalar was absent, invalid, or belonged
  // to another association's preparation.
  localparam logic [7:0] StBadPrep     = 8'hF4;
  // The image could not be admitted: an ordinal the capture window cannot
  // observe, or a strict descriptor with a zero required mask.
  localparam logic [7:0] StBadImage    = 8'hF5;

  // ==========================================================================
  // ELABORATION GUARDS
  // ==========================================================================
  // Quartus 17.0 rejects a bare module-scope `if` -- "syntax error near text:
  // `if`; expecting `endmodule`" -- so every guard lives inside `initial begin
  // ... end`. AND `verilator --lint-only` DOES NOT RUN `initial` BLOCKS, so a
  // clean lint is no evidence whatever about any line below. Each of these was
  // fired with a wrong parameterisation and watched to fail; the R126 one has a
  // committed driver under tests/mutants/.
  initial begin
    // ------------------------------------------------------------------
    // R126. THE GUARD THAT WAS MISSING, AND WHY IT COULD NOT BE SEEN.
    // ------------------------------------------------------------------
    // Three places carry one quantity: `zhao_field_host.sv:218` defaults
    // OUT_LANES = 4, `zhao_console_core.sv:15910` composes .OUT_LANES(7), and
    // the generated schema fixes ZFH_WINDOW_MASK_BITS = 7. Nothing checked
    // that they agree, and the disagreement is INVISIBLE -- a mask of the
    // wrong width still packs, still transmits and still compares. It is the
    // same shape as the defect this whole packet exists to prevent, one level
    // up: two quantities that must agree with nothing making them.
    //
    // (The ruling cites `zhao_console_core.sv:15869` for the composition.
    // Re-asked at this commit: 15853 is the instantiation and the
    // `.OUT_LANES(7)` for `u_field_host` is at 15910. The claim is true; the
    // line number is not.)
    if (OUT_LANES != ZFH_WINDOW_MASK_BITS) begin
      $fatal(1, "zhao_field_host_v2: OUT_LANES=%0d but the generated schema fixes ZFH_WINDOW_MASK_BITS=%0d. The window mask would pack, transmit and compare at the wrong width, silently. Regenerate the schema or compose the matching width; do not widen one side.", OUT_LANES, ZFH_WINDOW_MASK_BITS);
    end
    // The ordinal mask is carried in a u8 by the schema and is meaningful only
    // to the profile's output count. A host with more ordinals than the
    // schema's carrier is a host whose top ordinals cannot be declared.
    if (OUT_ORDINALS > ZFH_MAX_CANONICAL_OUTPUTS) begin
      $fatal(1, "zhao_field_host_v2: OUT_ORDINALS=%0d exceeds the schema's ZFH_MAX_CANONICAL_OUTPUTS=%0d", OUT_ORDINALS, ZFH_MAX_CANONICAL_OUTPUTS);
    end
    if (OUT_ORDINALS > ZFH_REQUIRED_MASK_BITS) begin
      $fatal(1, "zhao_field_host_v2: OUT_ORDINALS=%0d exceeds ZFH_REQUIRED_MASK_BITS=%0d, so the top ordinals could never be declared", OUT_ORDINALS, ZFH_REQUIRED_MASK_BITS);
    end
    if (OUT_ORDINALS < 1) $fatal(1, "zhao_field_host_v2: OUT_ORDINALS must be at least 1");

    // The v2 header word's field plan. The window mask keeps R101's home at
    // [32 +: OUT_LANES]; the ordinal mask, output count, execution form and
    // init-proof bit sit above it from bit 48. So OUT_LANES may not run past
    // bit 47 or the two would overlap -- and an overlap here is the exact
    // ordinal/window confusion this file exists to prevent, expressed in bits.
    if (OUT_LANES > 16) begin
      $fatal(1, "zhao_field_host_v2: OUT_LANES=%0d; the v2 header word puts the WINDOW mask at [32 +: OUT_LANES] and the ORDINAL mask at [48 +: 8], so 16 is the ceiling before they overlap", OUT_LANES);
    end

    if (IN_LANES > REGS)   $fatal(1, "zhao_field_host_v2: IN_LANES=%0d exceeds REGS=%0d", IN_LANES, REGS);
    if (OUT_LANES > REGS)  $fatal(1, "zhao_field_host_v2: OUT_LANES=%0d exceeds REGS=%0d", OUT_LANES, REGS);
    if (CLIENTS < 1)       $fatal(1, "zhao_field_host_v2: CLIENTS must be at least 1");
    if (CREDITS < 1)       $fatal(1, "zhao_field_host_v2: CREDITS must be at least 1");
    if (PREP_SCALARS < 1)  $fatal(1, "zhao_field_host_v2: PREP_SCALARS must be at least 1");
    if (FAB_LANES < 1)     $fatal(1, "zhao_field_host_v2: FAB_LANES must be at least 1");
    if (FAB_OUTSTANDING < 1 || FAB_LONGQ < 1 || FAB_GATHERS < 1 ||
        FAB_DIST_BANKS < 1 || FAB_RING_UNITS < 1 || FAB_RING_DESC < 1) begin
      $fatal(1, "zhao_field_host_v2: every fabric knob must be at least 1");
    end
    // The 64-bit uop word packs four SIX-bit register fields edge to edge, so
    // REGS > 64 makes each [N +: REGW] slice overlap the next and `dst` eats
    // `a`'s bit 0 -- silent, and it produces a program that runs and computes
    // the wrong thing.
    if (REGW > 6) begin
      $fatal(1, "zhao_field_host_v2: REGS=%0d needs REGW=%0d, but the 64-bit uop word packs four 6-bit register fields", REGS, REGW);
    end
    if (SLOTW != ((PROGS > 1) ? $clog2(PROGS) : 1))
      $fatal(1, "zhao_field_host_v2: SLOTW=%0d disagrees with clog2(PROGS=%0d)", SLOTW, PROGS);
    if (PCW != ((INSTR_N > 1) ? $clog2(INSTR_N) : 1))
      $fatal(1, "zhao_field_host_v2: PCW=%0d disagrees with clog2(INSTR_N=%0d)", PCW, INSTR_N);
    if (REGW != ((REGS > 1) ? $clog2(REGS) : 1))
      $fatal(1, "zhao_field_host_v2: REGW=%0d disagrees with clog2(REGS=%0d)", REGW, REGS);
    if (TSELW != ((TABLES > 1) ? $clog2(TABLES) : 1))
      $fatal(1, "zhao_field_host_v2: TSELW=%0d disagrees with clog2(TABLES=%0d)", TSELW, TABLES);
    if (TIDXW != ((TBL_N > 1) ? $clog2(TBL_N) : 1))
      $fatal(1, "zhao_field_host_v2: TIDXW=%0d disagrees with clog2(TBL_N=%0d)", TIDXW, TBL_N);
    if (LDADDRW != ((PCW > (TSELW + TIDXW)) ? PCW : (TSELW + TIDXW)))
      $fatal(1, "zhao_field_host_v2: LDADDRW=%0d disagrees with max(PCW, TSELW+TIDXW)", LDADDRW);
    if (ORDW != ((OUT_ORDINALS > 1) ? $clog2(OUT_ORDINALS) : 1))
      $fatal(1, "zhao_field_host_v2: ORDW=%0d disagrees with clog2(OUT_ORDINALS=%0d)", ORDW, OUT_ORDINALS);
    if (PREPW != ((PREP_SCALARS > 1) ? $clog2(PREP_SCALARS) : 1))
      $fatal(1, "zhao_field_host_v2: PREPW=%0d disagrees with clog2(PREP_SCALARS=%0d)", PREPW, PREP_SCALARS);
    if (CRDW != ((CREDITS > 1) ? $clog2(CREDITS) : 1))
      $fatal(1, "zhao_field_host_v2: CRDW=%0d disagrees with clog2(CREDITS=%0d)", CRDW, CREDITS);
  end

  // ==========================================================================
  // THE RESIDENCY DIRECTORY
  // ==========================================================================
  logic [SLOTW-1:0] pc_cm_slot_c;

  zhao_field_progcache #(
      .ENTRIES(PROGS)
  ) u_progcache (
      .clk  (clk),
      .rst_n(rst_n),
      .lu_valid_i     (pc_lu_valid_i),
      .lu_ready_o     (pc_lu_ready_o),
      .lu_hash_i      (pc_lu_hash_i),
      .lu_resp_valid_o(pc_lu_resp_valid_o),
      .lu_resp_ready_i(pc_lu_resp_ready_i),
      .lu_hit_o       (pc_lu_hit_o),
      .lu_slot_o      (pc_lu_slot_o),
      .cm_valid_i     (pc_cm_valid_i),
      .cm_ready_o     (pc_cm_ready_o),
      .cm_hash_i      (pc_cm_hash_i),
      .cm_ok_i        (pc_cm_ok_i),
      .cm_resp_valid_o(pc_cm_resp_valid_o),
      .cm_resp_ready_i(pc_cm_resp_ready_i),
      .cm_inserted_o  (pc_cm_inserted_o),
      .cm_evicted_o   (pc_cm_evicted_o),
      .cm_slot_o      (pc_cm_slot_c),
      .hits_o             (pc_hits_o),
      .misses_o           (pc_misses_o),
      .programs_rejected_o(pc_rejected_o),
      .evictions_o        (pc_evictions_o),
      .occupancy_o        (pc_occupancy_o)
  );

  assign pc_cm_slot_o = pc_cm_slot_c;

  // ==========================================================================
  // PROGRAM / ASSOCIATION METADATA, one row per slot
  // ==========================================================================
  logic [PROGS-1:0]        hdr_loaded;
  logic [REGW-1:0]         hdr_outbase  [0:PROGS-1];
  // WINDOW-indexed. R101's mask, kept for padding reporting ONLY.
  logic [OUT_LANES-1:0]    hdr_winmask  [0:PROGS-1];
  // ORDINAL-indexed. THE COMPLETION RULE.
  logic [OUT_ORDINALS-1:0] hdr_reqmask  [0:PROGS-1];
  logic [3:0]              hdr_outcount [0:PROGS-1];
  logic [1:0]              hdr_form     [0:PROGS-1];
  logic [PROGS-1:0]        hdr_ipok;
  logic [7:0]              hdr_assoc_gen[0:PROGS-1];

  // THE OUTPUT_MAP -- one row per ordinal per slot. This IS the translation.
  logic [PROGS-1:0][OUT_ORDINALS-1:0]           omap_kind;   // 0 VECTOR_REG, 1 PREPARED_SCALAR
  logic [PROGS-1:0][OUT_ORDINALS-1:0]           omap_row_ok; // a row was loaded
  logic [IDXW-1:0] omap_index [0:PROGS-1][0:OUT_ORDINALS-1];

  // ==========================================================================
  // THE PREPARED-SCALAR FILE -- ONE ACTIVE DOMAIN (FH09)
  // ==========================================================================
  // `prep_valid` is a SEPARATE BIT from the data and that separation is the
  // whole of FT018-vs-FT019: a missing value and a zero value are
  // distinguished by independently driven validity, never by the data. A zero
  // with its valid bit set is a result; a zero without it is an absence.
  logic signed [31:0] prep_value [0:PREP_SCALARS-1];
  logic [PREP_SCALARS-1:0] prep_valid;
  logic [7:0]         prep_gen   [0:PREP_SCALARS-1];

  // ==========================================================================
  // THE ARBITER
  // ==========================================================================
  logic [CIDW-1:0] rr_ptr;
  logic [CIDW-1:0] pick_id;
  logic            pick_any;
  logic            offers_many;

  integer ai;
  integer aoff;
  logic [CIDW-1:0] acand;
  always_comb begin
    pick_id     = rr_ptr;
    pick_any    = 1'b0;
    aoff        = 0;
    acand       = '0;
    for (ai = 0; ai < int'(CLIENTS); ai = ai + 1) begin
      acand = CIDW'((int'(rr_ptr) + ai) % int'(CLIENTS));
      if (!pick_any && req_valid_i[acand]) begin
        pick_any = 1'b1;
        pick_id  = acand;
      end
      if (req_valid_i[ai]) aoff = aoff + 1;
    end
    offers_many = (aoff > 1);
  end

  // ==========================================================================
  // THE FABRIC
  // ==========================================================================
  logic                     fab_up_we;
  logic [SLOTW-1:0]         fab_up_ctx;
  logic [PCW-1:0]           fab_up_pc;
  logic [7:0]               fab_up_op;
  logic [REGW-1:0]          fab_up_dst, fab_up_a, fab_up_b, fab_up_c;
  logic [31:0]              fab_up_imm;

  logic                     fab_pre_we;
  logic [SLOTW-1:0]         fab_pre_ctx;
  logic [REGW-1:0]          fab_pre_reg;
  logic signed [32*FAB_LANES-1:0] fab_pre_data;

  logic                     fab_pre_ready;
  logic                     fab_start;
  logic [SLOTW-1:0]         fab_start_ctx;

  logic                     fab_tl_we, fab_tl_commit;
  logic [1:0]               fab_tl_tbl;
  logic [TIDXW-1:0]         fab_tl_idx;
  logic signed [31:0]       fab_tl_x, fab_tl_y, fab_tl_dy;
  logic [6:0]               fab_tl_n;

  logic                     fab_sb_we;
  logic [15:0]              fab_sb_waddr;
  logic signed [31:0]       fab_sb_wdata;

  logic                     fab_done_valid;
  logic [SLOTW-1:0]         fab_done_ctx;
  logic                     fab_wr_en;
  logic [SLOTW-1:0]         fab_wr_ctx;
  logic [REGW-1:0]          fab_wr_reg;
  /* verilator lint_off UNUSEDSIGNAL */
  logic signed [32*FAB_LANES-1:0] fab_wr_data;
  /* verilator lint_on UNUSEDSIGNAL */

  logic                     fab_unsupported, fab_exec_desync, fab_bank_desync;
  logic                     fab_svc_bank_desync, fab_tag_mismatch, fab_wrong_op;
  logic                     fab_sk_overflow, fab_sb_bad, fab_imm_bad;
  logic                     fab_sat_add, fab_sat_mul, fab_sat_rescale;
  // R145 landed by packet C1: the fabric now CARRIES rcp0, so this host reads
  // it from the engine instead of from its own input port. See `rcp0_i`.
  logic                     fab_rcp0;
  logic [31:0]              fab_uops_issued;
  // THE FENCE'S SOURCE. `active_o[ctx]` is the fabric's own statement that this
  // context still holds work. It is read, not sampled once -- see E_DRAIN.
  logic [PROGS-1:0]         fab_active;

  // READ BY THE FENCE, not decoration. See the fence chapter below.
  logic [31:0] fab_rf_writes;
  logic [31:0] fab_drain_writes;
  logic        fab_dbg_long_valid;

  /* verilator lint_off UNUSEDSIGNAL */
  logic [31:0] fab_idle_clocks, fab_hold_clocks, fab_blocked_clocks;
  logic [31:0] fab_denied_clocks, fab_dot_clocks, fab_skid_clocks;
  logic [31:0] fab_groups, fab_partial;
  logic [31:0] fab_mul_grants, fab_mul_stall_lanes;
  logic [31:0] fab_bank_both, fab_bank_engine_only, fab_bank_svc_only, fab_bank_neither;
  logic [31:0] fab_svc_taken [7];
  logic [31:0] fab_svc_refused [7];
  logic [31:0] fab_ring_req_taken, fab_ring_fetch_clocks, fab_ring_hand_wait;
  logic [31:0] fab_ring_desc_hit, fab_ring_desc_miss;
  logic [31:0] fab_wb_served [2];
  logic [31:0] fab_wb_stalled [2];
  logic fab_dbg_long_ready, fab_dbg_s2_v;
  logic [SLOTW-1:0] fab_dbg_long_ctx, fab_dbg_s2_ctx;
  logic [7:0] fab_dbg_long_op, fab_dbg_s2_op;
  logic signed [31:0] fab_dbg_long_s0, fab_dbg_use_a0, fab_dbg_rf_a0;
  /* verilator lint_on UNUSEDSIGNAL */

  zhao_field_v3_engine #(
      .CTX        (PROGS),
      .OUTSTANDING(FAB_OUTSTANDING),
      .LANES      (FAB_LANES),
      .LONGQ      (FAB_LONGQ),
      .GATHERS    (FAB_GATHERS),
      .DIST_BANKS (FAB_DIST_BANKS),
      .GROUP_PTS  (FAB_GROUP_PTS),
      .RING_UNITS (FAB_RING_UNITS),
      .RING_DESC  (FAB_RING_DESC),
      .REGS       (REGS),
      .PLAN       (INSTR_N),
      .TAGW       (8)
  ) u_fabric (
      .clk  (clk),
      .rst_n(rst_n),

      .up_we_i (fab_up_we),
      .up_ctx_i(fab_up_ctx),
      .up_pc_i (fab_up_pc),
      .up_op_i (fab_up_op),
      .up_dst_i(fab_up_dst),
      .up_a_i  (fab_up_a),
      .up_b_i  (fab_up_b),
      .up_c_i  (fab_up_c),
      .up_imm_i(fab_up_imm),

      .pre_we_i  (fab_pre_we),
      .pre_ctx_i (fab_pre_ctx),
      .pre_reg_i (fab_pre_reg),
      .pre_data_i(fab_pre_data),

      .start_i    (fab_start),
      .start_ctx_i(fab_start_ctx),

      // Test stimulus, tied low: with it low, bank claimant 0 is never asked
      // and the refusal path is exercised by the real services instead.
      .rival_req_i(1'b0),
      // Drain-first. ALU-first STARVES the drain outright; drain-first costs
      // eight clocks per four-point group. A starved drain is a long op whose
      // answer never lands, which is a wrong field rather than a slow one --
      // and under the fence below it would be a point that never retires.
      .wb_policy_i(2'd1),

      .done_valid_o(fab_done_valid),
      .done_ctx_o  (fab_done_ctx),
      .active_o    (fab_active),
      .unsupported_o(fab_unsupported),
      .sat_add_o    (fab_sat_add),
      .sat_mul_o    (fab_sat_mul),
      .sat_rescale_o(fab_sat_rescale),
      .rcp0_o       (fab_rcp0),

      .wr_en_o  (fab_wr_en),
      .wr_ctx_o (fab_wr_ctx),
      .wr_reg_o (fab_wr_reg),
      .wr_data_o(fab_wr_data),

      .exec_desync_o   (fab_exec_desync),
      .bank_desync_o   (fab_bank_desync),
      .svc_bank_desync_o(fab_svc_bank_desync),
      .tag_mismatch_o  (fab_tag_mismatch),
      .wrong_op_o      (fab_wrong_op),
      .uops_issued_o   (fab_uops_issued),

      .idle_clocks_o   (fab_idle_clocks),
      .hold_clocks_o   (fab_hold_clocks),
      .blocked_clocks_o(fab_blocked_clocks),
      .denied_clocks_o (fab_denied_clocks),
      .dot_clocks_o    (fab_dot_clocks),
      .skid_clocks_o   (fab_skid_clocks),
      .rf_writes_o     (fab_rf_writes),
      .groups_o        (fab_groups),
      .partial_o       (fab_partial),
      .drain_writes_o  (fab_drain_writes),
      .mul_grants_o    (fab_mul_grants),
      .mul_stall_lanes_o(fab_mul_stall_lanes),
      .bank_both_o     (fab_bank_both),
      .bank_engine_only_o(fab_bank_engine_only),
      .bank_svc_only_o (fab_bank_svc_only),
      .bank_neither_o  (fab_bank_neither),
      .svc_taken_o     (fab_svc_taken),
      .svc_refused_o   (fab_svc_refused),
      .ring_req_taken_o(fab_ring_req_taken),
      .ring_fetch_clocks_o(fab_ring_fetch_clocks),
      .ring_hand_wait_o(fab_ring_hand_wait),
      .ring_desc_hit_o (fab_ring_desc_hit),
      .ring_desc_miss_o(fab_ring_desc_miss),
      .wb_served_o     (fab_wb_served),
      .wb_stalled_o    (fab_wb_stalled),

      .tl_we_i    (fab_tl_we),
      .tl_tbl_i   (fab_tl_tbl),
      .tl_idx_i   (fab_tl_idx),
      .tl_x_i     (fab_tl_x),
      .tl_y_i     (fab_tl_y),
      .tl_dy_i    (fab_tl_dy),
      .tl_commit_i(fab_tl_commit),
      .tl_n_i     (fab_tl_n),

      .sb_we_i    (fab_sb_we),
      .sb_waddr_i (fab_sb_waddr),
      .sb_wdata_i (fab_sb_wdata),
      .sb_bad_o   (fab_sb_bad),
      .imm_bad_o  (fab_imm_bad),
      .sk_overflow_o(fab_sk_overflow),

      .dbg_long_valid_o(fab_dbg_long_valid),
      .dbg_long_ready_o(fab_dbg_long_ready),
      .dbg_long_ctx_o  (fab_dbg_long_ctx),
      .dbg_long_op_o   (fab_dbg_long_op),
      .dbg_long_s0_o   (fab_dbg_long_s0),
      .pre_ready_o     (fab_pre_ready),
      .dbg_s2_v_o      (fab_dbg_s2_v),
      .dbg_s2_ctx_o    (fab_dbg_s2_ctx),
      .dbg_s2_op_o     (fab_dbg_s2_op),
      .dbg_use_a0_o    (fab_dbg_use_a0),
      .dbg_rf_a0_o     (fab_dbg_rf_a0)
  );

  // ==========================================================================
  // THE RUN STATE MACHINE
  // ==========================================================================
  // E_DRAIN is the state the oracle does not have, and it is the whole repair
  // for FT024 and FT025.
  localparam logic [2:0] E_IDLE  = 3'd0;
  localparam logic [2:0] E_ZERO  = 3'd1;
  localparam logic [2:0] E_WRITE = 3'd2;
  localparam logic [2:0] E_START = 3'd3;
  localparam logic [2:0] E_RUN   = 3'd4;
  localparam logic [2:0] E_DRAIN = 3'd5;
  localparam logic [2:0] E_RETIRE= 3'd6;

  logic [2:0]         state;
  logic [SLOTW-1:0]   cur_slot;
  // The response entry this point RESERVED at grant. Remembered rather than
  // reconstructed from a wrapping tail pointer at retirement.
  logic [CRDW-1:0]    cur_rsv;
  logic [REGW:0]      zero_i;
  logic [LANEW:0]     lane_i;
  logic signed [31:0] cur_in  [0:IN_LANES-1];

  // ORDINAL-INDEXED. Not window-indexed. This is the point of the file.
  logic signed [31:0]      cur_export [0:OUT_ORDINALS-1];
  logic [OUT_ORDINALS-1:0] cur_seen;
  // Set when a declared PREPARED_SCALAR ordinal could not be seeded.
  logic                    cur_prep_bad;
  // Set when the run wrote at least one VECTOR_REG ordinal. Distinguishes a
  // genuine all-uniform retirement from a vector program that produced nothing.
  logic                    cur_any_vec;
  logic [OUT_LANES-1:0]    cur_winseen;
  logic [3:0]              cur_num;

  // ==========================================================================
  // THE OUTPUT_MAP LOOKUP -- the ONLY place ordinal and window meet
  // ==========================================================================
  // For each ordinal j of the running slot: does THIS granted write satisfy it?
  //
  // Note what is compared: the write's REGISTER against the ordinal's
  // `source_index`. Not the window position, not `out_base + k`. Aliasing is
  // therefore free -- two ordinals naming one register are both satisfied by
  // one write, which is FT023 -- and nothing needs to compact window slots into
  // result slots because the result slots were never window slots.
  // EVERY BOUND COMPARISON HERE IS DONE AT 32 BITS, AND THAT IS NOT STYLE.
  // `IDXW'(REGS)` is the trap: IDXW is 6 and the shipped REGS is 64, so
  // `6'(64)` is ZERO and `x < 0` is constant FALSE for unsigned x -- no ordinal
  // would ever be seen and the host would refuse every point, at exactly the
  // parameterisation the console ships. This file's oracle records the same
  // defect with the opposite polarity at `zhao_field_host.sv:870-882`:
  // `LDADDRW'(TABLES * TBL_N)` is `7'(128)` = 0, the comparison was constant
  // TRUE, and every knot-table write would have been dropped. Same cause, same
  // tool found it (Verilator's UNSIGNED warning), opposite direction.
  logic [OUT_ORDINALS-1:0] wr_hits_c;
  integer hj;
  always_comb begin
    wr_hits_c = '0;
    for (hj = 0; hj < int'(OUT_ORDINALS); hj = hj + 1) begin
      if (omap_row_ok[cur_slot][hj] &&
          (omap_kind[cur_slot][hj] == 1'b0) &&
          (32'(omap_index[cur_slot][hj]) < 32'(REGS)) &&
          (omap_index[cur_slot][hj][REGW-1:0] == fab_wr_reg)) begin
        wr_hits_c[hj] = 1'b1;
      end
    end
  end

  // The granted write this front may capture. `wr_*` is the ARBITER's output,
  // so it is the register file's write AS IT ACTUALLY HAPPENS -- a front that
  // latched the ALU's request would record writes the arbiter refused (FT026).
  wire wr_for_us_c = fab_wr_en && (fab_wr_ctx == cur_slot);
  wire capture_c   = wr_for_us_c && ((state == E_RUN) || (state == E_DRAIN));

  // The WINDOW position of this write, used ONLY for padding reporting.
  wire win_hit_c = wr_for_us_c &&
                   (fab_wr_reg >= hdr_outbase[cur_slot]) &&
                   ((int'(fab_wr_reg) - int'(hdr_outbase[cur_slot])) < int'(OUT_LANES));
  wire [WINW-1:0] win_idx_c = WINW'(int'(fab_wr_reg) - int'(hdr_outbase[cur_slot]));

  // ---- the completion rule, on NEXT-STATE sets ------------------------------
  // seen_next includes the write being granted THIS clock. The oracle reads the
  // pre-update register and therefore cannot count a write coincident with END;
  // that is FT024 and it is a one-line difference with a two-case consequence.
  wire [OUT_ORDINALS-1:0] seen_next_c = cur_seen | (capture_c ? wr_hits_c : '0);
  wire [OUT_ORDINALS-1:0] req_mask_c  = hdr_reqmask[cur_slot];
  wire complete_c   = ((seen_next_c & req_mask_c) == req_mask_c);
  wire nothing_c    = (seen_next_c == '0);

  wire alarm_c = fab_unsupported || fab_exec_desync || fab_bank_desync ||
                 fab_svc_bank_desync || fab_tag_mismatch || fab_wrong_op ||
                 fab_sk_overflow || fab_sb_bad || fab_imm_bad;

  // ---- THE FENCE ------------------------------------------------------------
  // Directive 7.4: publish only after "all prior ALU and long-op writes have
  // retired" and "no instruction/service completion can still modify its
  // outputs". A FIXED GUESSED "wait five clocks after END" is named there as
  // the wrong answer, because the skid and the drain are both gated on an
  // arbiter another claimant can hold for an unbounded number of clocks
  // (`zhao_field_v3_wbarb.sv:162-171`), so no fixed number is a bound.
  //
  // WHAT THE FABRIC ACTUALLY OFFERS, MEASURED RATHER THAN ASSUMED. There is no
  // per-context outstanding-work port anywhere in the v3 family -- searched at
  // port level for *_outstanding*, *_inflight*, *_pending*, *_busy*, *_drain*,
  // *_empty*, *_quiesc*: the only hits on the whole family are `idle_clocks_o`
  // and `drain_writes_o`, and both are 32-bit COUNTERS, not states. The state
  // exists (`inflight_r`, `waiting_r`, `lq_n_r`, `sk_n_r`, dispatcher
  // `count_r`) and none of it is exported.
  //
  // So the fence is built from the four things a host CAN see, and it is
  // deliberately over-determined:
  //
  //   * `active_o == '0` -- no context is between START and its END. This
  //     front starts one context at a time, so at most one bit is ever set.
  //     Taken alone this term is ALREADY sufficient by construction, and that
  //     is exactly why it is not taken alone: the sufficiency is EMERGENT from
  //     three gates in three different files (`zhao_field_v3_exec.sv:378`'s
  //     `!sk_busy_c`, `zhao_field_v3_dispatch.sv:694`'s release-with-last-write,
  //     and `zhao_field_v3_exec.sv:1373`'s un-park), it is declared nowhere,
  //     and no assertion covers it. A host that depends on an undeclared
  //     emergent property has a fence that works until somebody edits a file
  //     they have no reason to connect to this one.
  //   * `!dbg_long_valid_o` -- the executor's long-op queue is empty. Global,
  //     not per-context, which for a one-point-in-flight front is the same
  //     thing and for a future gathering front is strictly stronger.
  //   * `rf_writes_o` and `drain_writes_o` STABLE for a clock -- no register
  //     write and no drain write was granted on the clock just gone. These are
  //     counters, and a counter that did not move is the cheapest honest
  //     statement that the thing it counts did not happen.
  //
  // `late_write_o` below is the instrument that says whether this is enough.
  // It counts a granted write for our context arriving after the fence
  // released, and it must read zero -- which is a CLAIM, so it is fired
  // deliberately by `tests/mutants/zhao_field_v3_exec_issue_gate_mutant.sv`.
  logic [31:0] rf_writes_q, drain_writes_q;
  wire fab_quiet_c = (fab_active == '0) && !fab_dbg_long_valid;
  wire fence_clear_c = fab_quiet_c &&
                       (fab_rf_writes    == rf_writes_q) &&
                       (fab_drain_writes == drain_writes_q);

  // ==========================================================================
  // LOAD DECODE
  // ==========================================================================
  wire [LDADDRW-1:0] ld_uop_limit = LDADDRW'(INSTR_N);
  wire ld_oob_c = ld_valid_i && ld_ready_o &&
                  (ld_kind_i == LdUop) && (ld_addr_i >= ld_uop_limit);

  wire [15:0] omap_src_c  = ld_data_i[15:0];
  wire        omap_kind_c = ld_data_i[16];
  // R111's standing hazard, as a load-time refusal: a strict descriptor whose
  // ORDINAL mask is zero while it declares outputs. Zero never means "accept
  // whatever happened" here.
  wire [OUT_ORDINALS-1:0] hdr_req_c   = ld_data_i[48 +: OUT_ORDINALS];
  wire [3:0]              hdr_count_c = ld_data_i[56 +: 4];
  wire [1:0]              hdr_form_c  = ld_data_i[60 +: 2];
  wire        hdr_zeromask_c = ld_valid_i && ld_ready_o && (ld_kind_i == LdHeader) &&
                               (hdr_req_c == '0) && (hdr_count_c != 4'd0);

  // THE DECLARED OUTPUT COUNT AND THE DECLARED MASK ARE TWO QUANTITIES THAT
  // MUST AGREE, AND WITHOUT THIS NOTHING MAKES THEM. FH05 asks for both; a
  // descriptor carrying `output_count = 4` beside a mask with three bits set
  // is not a descriptor anybody can act on, and whichever of the two a later
  // reader happens to trust decides the behaviour. Counted, not silently
  // reconciled.
  logic [3:0] req_pop_c;
  integer pj;
  always_comb begin
    req_pop_c = 4'd0;
    for (pj = 0; pj < int'(OUT_ORDINALS); pj = pj + 1) begin
      if (hdr_req_c[pj]) req_pop_c = req_pop_c + 4'd1;
    end
  end
  wire hdr_countbad_c = ld_valid_i && ld_ready_o && (ld_kind_i == LdHeader) &&
                        (hdr_count_c != req_pop_c);

  // A UNIFORM_ONLY image whose declared ordinals are not ALL prepared scalars
  // is not that optimisation. The schema says so in as many words: the form
  // "requires a checked logical plan with no varying instructions, all output
  // sources valid prepared scalars, and ZERO physical uops. An empty arbitrary
  // physical program is NOT this optimisation." The OUTPUT_MAP rows are
  // already loaded when the header arrives, because the header is written
  // LAST, so this is checkable here and nowhere earlier.
  logic uniform_viol_c;
  integer uj;
  always_comb begin
    uniform_viol_c = 1'b0;
    for (uj = 0; uj < int'(OUT_ORDINALS); uj = uj + 1) begin
      if (hdr_req_c[uj] && !(omap_row_ok[ld_slot_i][uj] &&
                             (omap_kind[ld_slot_i][uj] == 1'b1))) begin
        uniform_viol_c = 1'b1;
      end
    end
  end
  wire hdr_formbad_c = ld_valid_i && ld_ready_o && (ld_kind_i == LdHeader) &&
                       (hdr_form_c == ZFH_FORM_UNIFORM_ONLY[1:0]) && uniform_viol_c;

  // ------------------------------------------------------------------------
  // AN ORDINAL THE CAPTURE WINDOW CANNOT OBSERVE -- CHECKED AT HEADER TIME,
  // AND THE REASON IS AN ORDERING THAT BIT ON FIRST RUN.
  // ------------------------------------------------------------------------
  // Section 2 case 2 of the repair plan: a VECTOR_REG source outside
  // [out_base, out_base + OUT_LANES) can never be seen, so the ordinal could
  // never be satisfied and the point would refuse for the WRONG REASON -- which
  // sends the next person to the completion logic instead of to the image.
  //
  // THE FIRST VERSION OF THIS CHECK RAN AT OUTPUT_MAP LOAD TIME AND WAS WRONG,
  // in the direction that refuses good images. `out_base` arrives in the
  // HEADER, and the header is deliberately written LAST -- it is the write that
  // makes a slot runnable, so a partially described program can never execute.
  // So at map-load time `hdr_outbase` still holds the PREVIOUS program's value,
  // and the window it describes is not this program's window. Measured, not
  // reasoned about afterwards: the directed test's FT020 maps ordinals to
  // R7/R4/R9 with out_base=4, and the check rejected R7 and R9 against a stale
  // base of 0. Two of three ordinals silently refused, and the point then
  // failed ST_PARTIAL -- a correct image reported as a broken program.
  //
  // The header is where a descriptor gets validated, because the header is the
  // first moment the descriptor is COMPLETE. Both other self-consistency checks
  // above already live here for the same reason.
  logic winobs_viol_c;
  integer wj;
  always_comb begin
    winobs_viol_c = 1'b0;
    for (wj = 0; wj < int'(OUT_ORDINALS); wj = wj + 1) begin
      if (omap_row_ok[ld_slot_i][wj] && (omap_kind[ld_slot_i][wj] == 1'b0) &&
          ((32'(omap_index[ld_slot_i][wj]) <  32'(ld_data_i[8 +: REGW])) ||
           (32'(omap_index[ld_slot_i][wj]) >= (32'(ld_data_i[8 +: REGW]) + 32'(OUT_LANES))))) begin
        winobs_viol_c = 1'b1;
      end
    end
  end
  wire hdr_winbad_c = ld_valid_i && ld_ready_o && (ld_kind_i == LdHeader) && winobs_viol_c;

  // ==========================================================================
  // THE RESPONSE RESERVATION (FH20)
  // ==========================================================================
  // A response entry is reserved at GRANT and released at DELIVERY, so the
  // execution context is free at the fence while the answer waits for its
  // client. `rsv_count` is the live reservation count; a grant needs one.
  logic [CRDW:0]           rsv_count;
  logic [CIDW-1:0]         rsp_id   [0:CREDITS-1];
  logic signed [31:0]      rsp_data [0:CREDITS-1][0:OUT_ORDINALS-1];
  logic [OUT_ORDINALS-1:0] rsp_pres [0:CREDITS-1];
  logic [OUT_LANES-1:0]    rsp_win  [0:CREDITS-1];
  logic [3:0]              rsp_cnt  [0:CREDITS-1];
  logic [7:0]              rsp_stat [0:CREDITS-1];
  logic [CRDW:0]           rsp_head, rsp_tail;

  wire rsp_full = (rsv_count == (CRDW+1)'(CREDITS));

  // A reserved-but-unfilled entry must NOT drive `resp_valid_o`, or the client
  // would read the previous point's answer out of a slot that has only been
  // promised to this one. Reservation and fill are two different acts and this
  // bit is the difference.
  logic [CREDITS-1:0] rsp_filled;

  wire [CRDW-1:0] head_idx_c = rsp_head[CRDW-1:0];
  wire            head_ok_c  = rsp_filled[head_idx_c];

  // RESERVE AND RELEASE CAN LAND ON THE SAME CLOCK -- a grant while the head
  // entry is being consumed -- so the count is computed once from both events
  // rather than by two `<=` statements, of which the later would silently win
  // and lose a credit. That loss is in the flattering direction: the pool would
  // appear to have more room than it has.
  wire rsv_take_c = (state == E_IDLE) && !ld_valid_i && pick_any && !rsp_full;
  wire rsv_give_c = head_ok_c && resp_ready_i[rsp_id[head_idx_c]];

  always_comb begin
    resp_valid_o = '0;
    if (head_ok_c) resp_valid_o[rsp_id[head_idx_c]] = 1'b1;
  end

  integer ro;
  always_comb begin
    resp_out_o = '0;
    for (ro = 0; ro < int'(OUT_ORDINALS); ro = ro + 1) begin
      resp_out_o[(ro*32) +: 32] = rsp_data[head_idx_c][ro];
    end
  end
  assign resp_present_o  = rsp_pres[head_idx_c];
  assign resp_window_o   = rsp_win[head_idx_c];
  assign resp_count_o    = rsp_cnt[head_idx_c];
  assign resp_status_o   = rsp_stat[head_idx_c];
  assign instr_retired_o = fab_uops_issued;
  assign num_status_o    = cur_num;

  // ---- handshakes ----------------------------------------------------------
  assign ld_ready_o = (state == E_IDLE);

  always_comb begin
    req_ready_o = '0;
    if ((state == E_IDLE) && !ld_valid_i && pick_any && !rsp_full) begin
      req_ready_o[pick_id] = 1'b1;
    end
  end

  // ==========================================================================
  // THE FABRIC'S LOAD AND PRELOAD PORTS
  // ==========================================================================
  wire [LANEW-1:0] lane_sel = (lane_i < (LANEW+1)'(IN_LANES)) ? lane_i[LANEW-1:0] : '0;

  always_comb begin
    //   [7:0] op  [13:8] dst  [19:14] a  [25:20] b  [31:26] c  [63:32] imm
    fab_up_we  = ld_valid_i && ld_ready_o && (ld_kind_i == LdUop) && !ld_oob_c;
    fab_up_ctx = ld_slot_i;
    fab_up_pc  = ld_addr_i[PCW-1:0];
    fab_up_op  = ld_data_i[7:0];
    fab_up_dst = ld_data_i[8 +: REGW];
    fab_up_a   = ld_data_i[14 +: REGW];
    fab_up_b   = ld_data_i[20 +: REGW];
    fab_up_c   = ld_data_i[26 +: REGW];
    fab_up_imm = ld_data_i[63:32];

    fab_tl_we  = ld_valid_i && ld_ready_o && (ld_kind_i == LdTable) && !ld_oob_c;
    fab_tl_tbl = 2'(ld_addr_i[(TSELW+TIDXW)-1 -: TSELW]);
    fab_tl_idx = ld_addr_i[TIDXW-1:0];
    fab_tl_x   = ld_data_i[31:0];
    fab_tl_y   = ld_data_i[63:32];
    fab_tl_dy  = ld_data_i[95:64];

    fab_sb_we    = ld_valid_i && ld_ready_o && (ld_kind_i == LdUniform);
    fab_sb_waddr = ld_data_i[95:80];
    fab_sb_wdata = ld_data_i[31:0];

    fab_tl_commit = ld_valid_i && ld_ready_o && (ld_kind_i == LdHeader);
    fab_tl_n      = ld_data_i[22:16];

    // THE PRELOAD PORT. Zeroes only on the slow path; the declared lanes
    // always. E_ZERO is skipped entirely under FH08, so the fast path's preload
    // is IN_LANES clocks where the oracle's is REGS + IN_LANES.
    fab_pre_we   = (state == E_ZERO) ||
                   ((state == E_WRITE) && (lane_i < (LANEW+1)'(IN_LANES)));
    fab_pre_ctx  = cur_slot;
    fab_pre_reg  = (state == E_ZERO) ? zero_i[REGW-1:0] : REGW'(lane_i);
    // Replication across the fabric's lanes: this front holds one point, so at
    // FAB_LANES>1 the other lanes recompute the same point and are discarded.
    // They are fed the REAL point rather than a fabricated zero, because a
    // fabricated zero is stimulus this front never received and it would reach
    // the saturation ledger and the service alarms.
    fab_pre_data = (state == E_ZERO) ? '0 : {FAB_LANES{cur_in[lane_sel]}};

    fab_start     = (state == E_START);
    fab_start_ctx = cur_slot;
  end

  // ==========================================================================
  // THE SEQUENTIAL BODY
  // ==========================================================================
  integer k;
  integer j;
  integer li;

  // GRANT-TIME QUANTITIES, as continuous wires rather than blocking temporaries
  // inside the clocked block. They are pure functions of the arbiter's pick and
  // the metadata arrays, so a wire is what they are.
  wire [SLOTW-1:0] gslot_c   = req_slot_i[(int'(pick_id)*int'(SLOTW)) +: SLOTW];
  wire [7:0]       ggen_c    = hdr_assoc_gen[gslot_c];
  wire             gnoprog_c = req_noprog_i[pick_id] || !hdr_loaded[gslot_c];
  wire [CRDW-1:0]  tail_idx_c = rsp_tail[CRDW-1:0];

  // Can ordinal j of the granted slot be seeded from the prepared file? Two
  // INDEPENDENT conditions, neither inferred from the data: the slot's VALID
  // bit, and its GENERATION against this association's. The bound is compared
  // at 32 bits -- see the note on `wr_hits_c`; `IDXW'(PREP_SCALARS)` is
  // `6'(64)` = 0, which made this constant FALSE and silently disabled every
  // uniform output in the file whose entire purpose is to deliver them.
  logic [OUT_ORDINALS-1:0] seed_ok_c;
  integer sj;
  always_comb begin
    seed_ok_c = '0;
    for (sj = 0; sj < int'(OUT_ORDINALS); sj = sj + 1) begin
      if (omap_row_ok[gslot_c][sj] && (omap_kind[gslot_c][sj] == 1'b1) &&
          (32'(omap_index[gslot_c][sj]) < 32'(PREP_SCALARS)) &&
          prep_valid[omap_index[gslot_c][sj][PREPW-1:0]] &&
          (prep_gen[omap_index[gslot_c][sj][PREPW-1:0]] == ggen_c)) begin
        seed_ok_c[sj] = 1'b1;
      end
    end
  end

  // A DECLARED prepared ordinal that cannot be seeded. Zero-filled RAM is not
  // a substitute for a preparation.
  logic [OUT_ORDINALS-1:0] seed_bad_c;
  integer bj;
  always_comb begin
    seed_bad_c = '0;
    for (bj = 0; bj < int'(OUT_ORDINALS); bj = bj + 1) begin
      if (omap_row_ok[gslot_c][bj] && (omap_kind[gslot_c][bj] == 1'b1) &&
          hdr_reqmask[gslot_c][bj] && !seed_ok_c[bj]) begin
        seed_bad_c[bj] = 1'b1;
      end
    end
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state        <= E_IDLE;
      rr_ptr       <= '0;
      cur_slot     <= '0;
      cur_rsv      <= '0;
      zero_i       <= '0;
      lane_i       <= '0;
      cur_seen     <= '0;
      cur_winseen  <= '0;
      cur_prep_bad <= 1'b0;
      cur_any_vec  <= 1'b0;
      cur_num      <= 4'd0;
      hdr_loaded   <= '0;
      hdr_ipok     <= '0;
      prep_valid   <= '0;
      rsv_count    <= '0;
      rsp_head     <= '0;
      rsp_tail     <= '0;
      rsp_filled   <= '0;
      rf_writes_q    <= 32'd0;
      drain_writes_q <= 32'd0;
      for (k = 0; k < int'(PROGS); k = k + 1) begin
        hdr_outbase[k]   <= '0;
        hdr_winmask[k]   <= '0;
        hdr_reqmask[k]   <= '0;
        hdr_outcount[k]  <= 4'd0;
        hdr_form[k]      <= 2'd0;
        hdr_assoc_gen[k] <= 8'd0;
        omap_kind[k]     <= '0;
        omap_row_ok[k]   <= '0;
        for (j = 0; j < int'(OUT_ORDINALS); j = j + 1) omap_index[k][j] <= '0;
      end
      for (k = 0; k < int'(PREP_SCALARS); k = k + 1) begin
        prep_value[k] <= 32'sd0;
        prep_gen[k]   <= 8'd0;
      end
      for (k = 0; k < int'(IN_LANES); k = k + 1) cur_in[k] <= 32'sd0;
      for (k = 0; k < int'(OUT_ORDINALS); k = k + 1) cur_export[k] <= 32'sd0;
      for (k = 0; k < int'(CREDITS); k = k + 1) begin
        rsp_id[k]   <= '0;
        rsp_stat[k] <= 8'd0;
        rsp_pres[k] <= '0;
        rsp_win[k]  <= '0;
        rsp_cnt[k]  <= 4'd0;
        for (j = 0; j < int'(OUT_ORDINALS); j = j + 1) rsp_data[k][j] <= 32'sd0;
      end
      runs_o             <= 32'd0;
      run_faults_o       <= 32'd0;
      noprog_o           <= 32'd0;
      loads_o            <= 32'd0;
      load_defers_o      <= 32'd0;
      grants_o           <= 32'd0;
      contended_grants_o <= 32'd0;
      ld_oob_o           <= 32'd0;
      no_result_o        <= 32'd0;
      out_incomplete_o   <= 32'd0;
      prep_bad_o         <= 32'd0;
      bad_image_o        <= 32'd0;
      zero_mask_o        <= 32'd0;
      late_write_o       <= 32'd0;
      fence_writes_o     <= 32'd0;
      uniform_runs_o     <= 32'd0;
      credit_stall_o     <= 32'd0;
      fast_path_o        <= 32'd0;
      slow_path_o        <= 32'd0;
      exec_desync_o      <= 32'd0;
      bank_desync_o      <= 32'd0;
      svc_bank_desync_o  <= 32'd0;
      tag_mismatch_o     <= 32'd0;
      wrong_op_o         <= 32'd0;
      unsupported_o      <= 32'd0;
      skid_overflow_o    <= 32'd0;
      uniform_bad_o      <= 32'd0;
    end else begin
      // ---- the alarms, counted separately and never OR-ed -----------------
      if (fab_exec_desync && (exec_desync_o != 32'hFFFF_FFFF))
        exec_desync_o <= exec_desync_o + 32'd1;
      if (fab_bank_desync && (bank_desync_o != 32'hFFFF_FFFF))
        bank_desync_o <= bank_desync_o + 32'd1;
      if (fab_svc_bank_desync && (svc_bank_desync_o != 32'hFFFF_FFFF))
        svc_bank_desync_o <= svc_bank_desync_o + 32'd1;
      if (fab_tag_mismatch && (tag_mismatch_o != 32'hFFFF_FFFF))
        tag_mismatch_o <= tag_mismatch_o + 32'd1;
      if (fab_wrong_op && (wrong_op_o != 32'hFFFF_FFFF))
        wrong_op_o <= wrong_op_o + 32'd1;
      if (fab_unsupported && (unsupported_o != 32'hFFFF_FFFF))
        unsupported_o <= unsupported_o + 32'd1;
      if (fab_sk_overflow && (skid_overflow_o != 32'hFFFF_FFFF))
        skid_overflow_o <= skid_overflow_o + 32'd1;
      if ((fab_sb_bad || fab_imm_bad) && (uniform_bad_o != 32'hFFFF_FFFF))
        uniform_bad_o <= uniform_bad_o + 32'd1;
      if (ld_oob_c && (ld_oob_o != 32'hFFFF_FFFF)) ld_oob_o <= ld_oob_o + 32'd1;

      // An insert invalidates the slot's program AND its init proof. The
      // directory has promised the slot to a new hash; a proof written for the
      // displaced program is not a proof about the new one, and carrying it
      // forward is exactly the "repair a bad proof by declaring registers
      // defined" failure directive 6.2 forbids.
      if (pc_cm_valid_i && pc_cm_ready_o && pc_cm_ok_i) begin
        hdr_loaded[pc_cm_slot_c] <= 1'b0;
        hdr_ipok[pc_cm_slot_c]   <= 1'b0;
      end

      // ---- capture, ordinal-indexed, on the ARBITER's granted write --------
      if (capture_c) begin
        for (j = 0; j < int'(OUT_ORDINALS); j = j + 1) begin
          if (wr_hits_c[j]) begin
            // A LATER WRITE REPLACES AN EARLIER VALUE. Directive 7.3: "the
            // result at retirement is the FINAL value". The oracle stops
            // capturing at END and therefore publishes the earlier one.
            cur_export[j] <= fab_wr_data[31:0];
            cur_seen[j]   <= 1'b1;
          end
        end
        if (wr_hits_c != '0) cur_any_vec <= 1'b1;
        if (win_hit_c) cur_winseen[win_idx_c] <= 1'b1;
        if ((state == E_DRAIN) && (fence_writes_o != 32'hFFFF_FFFF)) begin
          fence_writes_o <= fence_writes_o + 32'd1;
        end
      end

      // A granted write for our context arriving when we are no longer
      // capturing. THE FENCE'S POSITIVE CONTROL: if the fence released early
      // this moves, and a fence that cannot be shown to be late is not a fence.
      if (wr_for_us_c && (state != E_RUN) && (state != E_DRAIN) &&
          (state != E_IDLE) && (late_write_o != 32'hFFFF_FFFF)) begin
        late_write_o <= late_write_o + 32'd1;
      end

      if ((state == E_RUN) || (state == E_DRAIN)) begin
        // `rcp0_i` IS STILL ORed IN, and that is deliberate now that the
        // fabric supplies `fab_rcp0`. The port's header said it existed
        // because "an input with an unconnected end is visible"; C1 connected
        // the end, and the port is retained so a bench can force the cause
        // without reaching inside the fabric. Production drives it to zero and
        // the fabric's own bit is what reports the real event -- so this reads
        // as `fab_rcp0` on the console and as either on a bench.
        cur_num <= cur_num |
                   {rcp0_i | fab_rcp0, fab_sat_rescale, fab_sat_mul, fab_sat_add};
      end

      // ---- response delivery, independent of the run -----------------------
      // FH20 / directive 7.5. The context was freed at the fence; this is the
      // RESERVED SLOT being handed over, and it can happen while another point
      // is already running. That decoupling is the whole point: a stalled
      // client spends its own credits and does not hold the fabric.
      if (rsv_give_c) begin
        rsp_filled[head_idx_c] <= 1'b0;
        rsp_head <= (rsp_head == (CRDW+1)'(CREDITS - 1)) ? '0 : (rsp_head + 1'b1);
      end

      // ONE assignment to the count, from BOTH events. See the declaration.
      if (rsv_take_c && !rsv_give_c)      rsv_count <= rsv_count + 1'b1;
      else if (!rsv_take_c && rsv_give_c) rsv_count <= rsv_count - 1'b1;

      // The fence's counter snapshots. Registered every clock so that
      // `fence_clear_c` compares THIS clock's count against the previous one.
      rf_writes_q    <= fab_rf_writes;
      drain_writes_q <= fab_drain_writes;

      case (state)
        E_IDLE: begin
          if (ld_valid_i) begin
            if (pick_any && (load_defers_o != 32'hFFFF_FFFF)) begin
              load_defers_o <= load_defers_o + 32'd1;
            end
            if (loads_o != 32'hFFFF_FFFF) loads_o <= loads_o + 32'd1;

            case (ld_kind_i)
              LdHeader: begin
                hdr_outbase[ld_slot_i] <= ld_data_i[8 +: REGW];
                // The WINDOW mask keeps R101's home exactly.
                hdr_winmask[ld_slot_i] <= ld_data_i[32 +: OUT_LANES];
                // The ORDINAL mask, the output count and the execution form.
                hdr_reqmask[ld_slot_i]  <= hdr_req_c;
                hdr_outcount[ld_slot_i] <= hdr_count_c;
                hdr_form[ld_slot_i]     <= hdr_form_c;
                // R111: a strict descriptor declaring outputs with a ZERO
                // ordinal mask is REFUSED. It does not become runnable.
                if (hdr_zeromask_c) begin
                  hdr_loaded[ld_slot_i] <= 1'b0;
                  if (zero_mask_o != 32'hFFFF_FFFF) zero_mask_o <= zero_mask_o + 32'd1;
                end else if (hdr_countbad_c || hdr_formbad_c || hdr_winbad_c) begin
                  // The descriptor contradicts itself. Refused at LOAD, by
                  // name, rather than producing a point that refuses later for
                  // a reason that points at the wrong file.
                  hdr_loaded[ld_slot_i] <= 1'b0;
                  if (bad_image_o != 32'hFFFF_FFFF) bad_image_o <= bad_image_o + 32'd1;
                end else begin
                  hdr_loaded[ld_slot_i] <= 1'b1;
                end
              end

              LdOutMap: begin
                // One OUTPUT_MAP row. `ld_addr_i` is the canonical output
                // ORDINAL; the row carries source_kind and source_index.
                // The row is RECORDED here and VALIDATED at the header, which
                // is the first moment `out_base` is known. See the note on
                // `hdr_winbad_c`.
                if (ld_addr_i < LDADDRW'(OUT_ORDINALS)) begin
                  omap_row_ok[ld_slot_i][ld_addr_i[ORDW-1:0]] <= 1'b1;
                  omap_kind[ld_slot_i][ld_addr_i[ORDW-1:0]]   <= omap_kind_c;
                  omap_index[ld_slot_i][ld_addr_i[ORDW-1:0]]  <= omap_src_c[IDXW-1:0];
                end
                hdr_loaded[ld_slot_i] <= 1'b0;
              end

              LdAssoc: begin
                // The ASSOCIATION generation. This is the identity that makes
                // "same program, new parameters" a different thing to run.
                hdr_assoc_gen[ld_slot_i] <= ld_data_i[7:0];
                hdr_loaded[ld_slot_i]    <= 1'b0;
              end

              LdInitProof: begin
                // FH08's interlock. The PROOF is the image's, walked by the
                // C++ validator; what the hardware owns is this bit, and the
                // no-clear fast path is gated on it and on nothing else.
                hdr_ipok[ld_slot_i]   <= ld_data_i[0];
                hdr_loaded[ld_slot_i] <= 1'b0;
              end

              LdPrepared: begin
                // A prepared scalar: value, its VALIDITY BIT, and the
                // preparation generation it belongs to. The valid bit is
                // driven independently of the data on purpose -- see the
                // declaration.
                if (ld_addr_i < LDADDRW'(PREP_SCALARS)) begin
                  prep_value[ld_addr_i[PREPW-1:0]] <= ld_data_i[31:0];
                  prep_valid[ld_addr_i[PREPW-1:0]] <= ld_data_i[32];
                  prep_gen[ld_addr_i[PREPW-1:0]]   <= ld_data_i[47:40];
                end
                hdr_loaded[ld_slot_i] <= 1'b0;
              end

              default: begin
                // UOP, TABLE, UNIFORM: any of them invalidates the slot again,
                // so the HEADER is always the last write and a partially
                // described program can never execute.
                hdr_loaded[ld_slot_i] <= 1'b0;
              end
            endcase

          end else if (pick_any && !rsp_full) begin
            cur_slot <= gslot_c;
            cur_rsv  <= tail_idx_c;
            for (li = 0; li < int'(IN_LANES); li = li + 1) begin
              cur_in[li] <= req_in_i[((int'(pick_id)*int'(IN_LANES) + li)*32) +: 32];
            end
            rr_ptr <= CIDW'((int'(pick_id) + 1) % int'(CLIENTS));
            zero_i <= '0;
            lane_i <= '0;
            cur_winseen <= '0;
            cur_any_vec <= 1'b0;
            cur_num     <= 4'd0;

            // ---- RESERVE THE RESPONSE SLOT BEFORE ACCEPTING (FH20) --------
            // The COUNT is advanced centrally above, because a release may
            // land on this same clock. `cur_rsv` REMEMBERS which entry this
            // point owns, so retirement does not have to reconstruct it by
            // arithmetic on a wrapping tail pointer.
            rsp_id[tail_idx_c] <= pick_id;

            if (grants_o != 32'hFFFF_FFFF) grants_o <= grants_o + 32'd1;
            if (offers_many && (contended_grants_o != 32'hFFFF_FFFF)) begin
              contended_grants_o <= contended_grants_o + 32'd1;
            end

            if (gnoprog_c) begin
              if (noprog_o != 32'hFFFF_FFFF) noprog_o <= noprog_o + 32'd1;
              // REFUSED, never faked. A field result is exactly the place a
              // plausible constant proves nothing.
              rsp_stat[tail_idx_c] <= StNoProgram;
              rsp_pres[tail_idx_c] <= '0;
              rsp_win[tail_idx_c]  <= '0;
              rsp_cnt[tail_idx_c]  <= 4'd0;
              for (j = 0; j < int'(OUT_ORDINALS); j = j + 1) begin
                rsp_data[tail_idx_c][j] <= 32'sd0;
              end
              rsp_filled[tail_idx_c] <= 1'b1;
              rsp_tail <= (rsp_tail == (CRDW+1)'(CREDITS - 1)) ? '0 : (rsp_tail + 1'b1);
              state <= E_IDLE;
            end else begin
              // ---- SEED THE PREPARED-SCALAR ORDINALS (FH06) ---------------
              // A uniform output is a RESULT. It is seeded HERE, at point
              // start, because an all-uniform program never generates a vector
              // write and waiting for one is waiting forever.
              //
              // Two independent conditions, and neither is inferred from the
              // data: the slot's VALID bit, and its GENERATION against this
              // association's. A prepared zero with its valid bit set is a
              // result; a zero without it is an absence.
              cur_seen     <= seed_ok_c;
              cur_prep_bad <= (seed_bad_c != '0);
              for (j = 0; j < int'(OUT_ORDINALS); j = j + 1) begin
                cur_export[j] <= seed_ok_c[j]
                                 ? prep_value[omap_index[gslot_c][j][PREPW-1:0]]
                                 : 32'sd0;
              end

              // ---- THE ALL-UNIFORM TERMINAL PATH ---------------------------
              // Directive 7.3: "All-uniform programs need a defined terminal
              // path: prepare once, seed every required output and numeric
              // status, and produce one response for each accepted point. This
              // is exact constant-result execution, not a fake program result.
              // DO NOT START A CONTEXT THAT CAN NEVER EMIT END MERELY TO MAKE
              // A COUNTER MOVE."
              //
              // A UNIFORM_ONLY image has ZERO physical uops. Starting the
              // fabric for it would park a context waiting for an END that no
              // instruction will issue -- which is a hang, not a slow answer.
              // The seeds are already in `cur_export`, so the point is finished
              // the moment it is granted and it retires directly.
              //
              // The form is not taken on trust: `hdr_formbad_c` refused the
              // image at load unless every declared ordinal really is a
              // prepared scalar, so reaching here means the seeds exist.
              if (hdr_form[gslot_c] == ZFH_FORM_UNIFORM_ONLY[1:0]) begin
                state <= E_RETIRE;
              // ---- FH08: THE CLEAR IS SKIPPED ONLY UNDER THE PROOF ---------
              end else if (hdr_ipok[gslot_c] && !cfg_slow_clear_i) begin
                if (fast_path_o != 32'hFFFF_FFFF) fast_path_o <= fast_path_o + 32'd1;
                state <= E_WRITE;
              end else begin
                if (slow_path_o != 32'hFFFF_FFFF) slow_path_o <= slow_path_o + 32'd1;
                state <= E_ZERO;
              end
              rsp_tail <= (rsp_tail == (CRDW+1)'(CREDITS - 1)) ? '0 : (rsp_tail + 1'b1);
            end
          end else if (pick_any && rsp_full) begin
            // FH20: no reserved location, so the point is NOT accepted. The
            // client spends its own credits; it does not hold the fabric.
            if (credit_stall_o != 32'hFFFF_FFFF) credit_stall_o <= credit_stall_o + 32'd1;
          end
        end

        // `pre_ready_o` IS HONOURED. `zhao_field_v3_exec.sv:208`: "The preload
        // landed this clock. A host that ignores it loses writes." A dropped
        // ZERO is the worst of them, because the register then holds the
        // previous point's value and the field is plausible everywhere.
        E_ZERO: begin
          if (fab_pre_ready) begin
            if (zero_i < (REGW+1)'(REGS - 1)) zero_i <= zero_i + 1'b1;
            else state <= E_WRITE;
          end
        end

        E_WRITE: begin
          if (fab_pre_ready) begin
            if (lane_i < (LANEW+1)'(IN_LANES)) lane_i <= lane_i + 1'b1;
            else state <= E_START;
          end
        end

        E_START: state <= E_RUN;

        // END IS NOT SUFFICIENT BY ITSELF (directive 7.4). Observing the
        // terminal moves us to DRAIN, where capture continues; it does not
        // publish.
        E_RUN: begin
          if (fab_done_valid && (fab_done_ctx == cur_slot)) state <= E_DRAIN;
        end

        // THE FENCE. Capture continues (see `capture_c`), so a write coincident
        // with END was already counted in `seen_next_c` and a delayed final
        // write still replaces its predecessor. We leave only when the fabric
        // says this context holds no work that could still write.
        E_DRAIN: begin
          if (fence_clear_c) state <= E_RETIRE;
        end

        // One terminal event per running identity, whatever END did. Directive
        // 7.4: END held high over several clocks produces ONE result (FT028).
        E_RETIRE: begin
          for (j = 0; j < int'(OUT_ORDINALS); j = j + 1) begin
            rsp_data[cur_rsv][j] <= cur_export[j];
          end
          rsp_pres[cur_rsv]   <= cur_seen;
          rsp_filled[cur_rsv] <= 1'b1;
          // The WINDOW the run actually touched, beside the ORDINAL results.
          // FT017: a caller asking "were the other window lanes missing?" gets
          // PADDING as an answer rather than silence -- a window lane that no
          // ordinal names was never a result and is not a missing one.
          rsp_win[cur_rsv] <= cur_winseen | ~hdr_winmask[cur_slot];
          rsp_cnt[cur_rsv] <= hdr_outcount[cur_slot];

          if (alarm_c) begin
            rsp_stat[cur_rsv] <= StAlarm;
            if (run_faults_o != 32'hFFFF_FFFF) run_faults_o <= run_faults_o + 32'd1;
          end else if (cur_prep_bad) begin
            // The preparation, not the program. A different file to open.
            rsp_stat[cur_rsv] <= StBadPrep;
            if (prep_bad_o != 32'hFFFF_FFFF) prep_bad_o <= prep_bad_o + 32'd1;
          end else if (req_mask_c == '0) begin
            // No ordinal was ever declared for this slot. Under R111 a strict
            // header cannot reach here (LdHeader refuses a zero mask that
            // declares outputs), so this is the explicitly-named legacy
            // compatibility binding: an image that declares no outputs at all.
            // It is a REFUSAL, not "accept whatever happened".
            rsp_stat[cur_rsv] <= StBadImage;
            if (bad_image_o != 32'hFFFF_FFFF) bad_image_o <= bad_image_o + 32'd1;
          end else if (nothing_c) begin
            rsp_stat[cur_rsv] <= StNoResult;
            if (no_result_o != 32'hFFFF_FFFF) no_result_o <= no_result_o + 32'd1;
          end else if (!complete_c) begin
            // SOME declared ordinals landed and at least one did not. The
            // absent ones read as the zero cleared at grant -- an answer a
            // caller reading only the words cannot tell from a field that
            // happens to be zero there, which is W10's "Do not make an absent
            // output look like a zero result". REFUSED, not clamped.
            rsp_stat[cur_rsv] <= StPartial;
            if (out_incomplete_o != 32'hFFFF_FFFF) begin
              out_incomplete_o <= out_incomplete_o + 32'd1;
            end
          end else begin
            rsp_stat[cur_rsv] <= StOk;
            if (runs_o != 32'hFFFF_FFFF) runs_o <= runs_o + 32'd1;
            // A retirement with every declared ordinal satisfied and NO vector
            // write at all: the all-uniform terminal path. Counted so that
            // "the uniform route is live" is a number.
            if (!cur_any_vec && (uniform_runs_o != 32'hFFFF_FFFF)) begin
              uniform_runs_o <= uniform_runs_o + 32'd1;
            end
          end

          // THE CONTEXT IS FREE NOW, not at response delivery. Directive 7.5.
          state <= E_IDLE;
        end

        default: state <= E_IDLE;
      endcase
    end
  end

endmodule : zhao_field_host_v2

`default_nettype wire
