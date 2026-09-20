// zhao_field_host.sv — THE CONSOLE'S FIELD FRONT: FIELD v3 made usable: the prepared vector
// fabric, its residency directory, and the arbiter that lets the five profiles
// take turns on ONE engine.
//
// Contract: design/contracts/FIELD.SEQ.CORE.md
// Reference: `zfield::interpret` (reference/src/zfield/zfield_interpret.cpp),
//            reached through the v3 executor, whose differential is
//            tests/differential/field_v3_full_directed.cpp.
//
// ---------------------------------------------------------------------------
// WHY THIS FILE EXISTS, AND WHY IT IS ONE ENGINE RATHER THAN THREE
// ---------------------------------------------------------------------------
// `zhao_console_core.sv` carried three separate boundary entries -- I5
// (PART.UPDATE's field sample), I31 (SURFACE.STAMP's brush) and I34
// (TERRAIN.PATCH's field-height lane) -- each naming a DIFFERENT absent owner:
// FIELD.SEQ.FLOW, FIELD.SEQ.STAMP, FIELD.SEQ.EARTH. Each said its owner "is
// not built".
//
// THAT IS NOT WHAT THE LEDGER SAYS. `design/contracts/FIELD.SEQ.EARTH.md`,
// `.FLOW.md` and `.STAMP.md` are identical on the point:
//
//   > Owner ruling, 2026-08-22: **one engine, five profiles.** This contract
//   > describes a CONFIGURATION of `FIELD.SEQ.CORE` ... There is no separate
//   > `FIELD.SEQ.EARTH` sequencer in hardware and there is not going to be one.
//
// `design/blocks.yml` records all five as `kind: profile` with
// `implemented_by: FIELD.SEQ.CORE`, rule V21. So a profile is a program set
// plus a STREAM ADAPTER, and what every one of those seams lacked is the
// ENGINE. This file is that engine, composed once.
//
// ---------------------------------------------------------------------------
// IT IS v3, AND THE OWNER'S RULE IS WHY
// ---------------------------------------------------------------------------
//   > "YOU ONLY GET TO FIT THE LATEST VERSION. IF IT IS BROKEN YOU FIX IT."
//
// `design/prod_manifest.yml` names FIELD v3 the production generation. An
// earlier revision of this file composed FIELD v1 (`zhao_field_seq`) because v1
// is complete, measured and frozen-as-fallback. That was wrong in the way the
// rule exists to forbid: fitting v1 spends the ALM and DSP budget on a machine
// that is not being shipped, and the number it produces describes the wrong
// design. v1 is gone from this file and from the console's closure.
//
// WHAT v3 NEEDED, since "it does not build yet" was the standing excuse: it
// BUILDS. `zhao_field_v3_engine` elaborates over the v3 family with three
// pre-existing VARHIDDEN warnings and no errors, and
// `tests/differential/field_v3_full_directed.cpp` drives it against
// `zfield::interpret`. What it did not have was a CONSOLE-SHAPED FRONT: a way
// to hand it a point and get that point's output lanes back. That front is
// this file, and it is the whole of what was missing.
//
// THE FAMOUS SPLINE/RING DEADLOCK IS FIXED, and the reason it took a search to
// find that out is worth recording, because it is what sent an earlier revision
// of this file to v1 in the first place. `zhao_field_v3_engine.sv` still
// carried thirty-eight lines titled "A DISAGREEMENT THE COMPOSITION EXPOSES,
// AND IT DEADLOCKS", ending "IT IS NOT FIXED HERE", and two more modules said
// the same thing in their own words. All of it is stale.
//
// It is fixed, and it is fixed STRUCTURALLY rather than by patching two lists:
// `zhao_field_ops_pkg` was written to BE the one table, and today both sides
// call it. The executor's `is_long` is
// `zhao_field_ops_pkg::field_is_long(op)`; the dispatcher's `dst_width_of` is
// `zhao_field_ops_pkg::field_long_width(op)`. Neither keeps a list of its own,
// so an op cannot be offered by one and refused by the other. SPLINE (0x1B) and
// UOP_RING_PREP (0xF1) are both in the table with width 1; OP_RING (0x21) is
// deliberately absent, and it is REFUSED rather than parked.
//
// A COMMENT DESCRIBING A BUG THAT NO LONGER EXISTS IS THE ONE KIND OF BUG THAT
// NEVER SHOWS UP RED. This one cost a composition built on the wrong
// generation, so the stale prose is repaired rather than worked around.
//
// THE FABRIC'S FILES WERE PROMOTED OUT OF `fpga/rtl/synth/` in the same pass:
// `zhao_field_v3_engine` (the whole composition), `zhao_field_v3_core`, and
// `zhao_field_v3_exec`, with `zhao_field_v3_curve` for the curve service. They
// had carried `zhao_probe_*` names since they were written as synthesis probes
// and were never renamed when they became the engine, and a `probe` in the
// machine is exactly what `design/prod_manifest.yml` says must never happen.
//
// ---------------------------------------------------------------------------
// WHAT THE SHARING SAVES
// ---------------------------------------------------------------------------
//   * FIVE PROFILES, ONE FABRIC. The alternative composition is one engine per
//     consuming seam. Every profile here is a program set and a stream adapter
//     instead, so the executor, the service path, both multiplier banks, the
//     register file and the directory are paid for once.
//   * NO PROGRAM RAM AT ALL, which the v1 version of this file needed. The v3
//     uop store lives inside the executor and is written through `up_*`, so the
//     64-bit instruction memory and its address decode are gone from here.
//   * ONE DIRECTORY. `zhao_field_progcache` serves every profile, and its slot
//     IS the executor's context index -- one namespace, not two that must agree.
//
// ---------------------------------------------------------------------------
// WHAT IS DELIBERATELY NOT HERE
// ---------------------------------------------------------------------------
//   * VALIDATION. `zfield::decode` is the single implementation of the
//     spec/form/field-ir.md 4/5 law. The loader decodes and reports one bit to
//     the directory, exactly as FIELD.PROGCACHE's contract requires.
//   * THE LANE MAP. Which registers a program's outputs land in is per-program
//     metadata, written into the header by the loader. `out_base` is a header
//     field for that reason.
//   * A PROFILE. There is no profile input. The stream adapters are separate
//     files, which FIELD.SEQ.CORE.md permits by name: "Adapters generate and
//     consume streams; they NEVER re-implement an op, and there are not five
//     engines."
//   * THE BANK RIVAL. `zhao_field_v3_engine`'s rival_req_i exists so a test can make
//     the multiplier bank refuse. It is tied LOW here, which is a tie-off of a
//     TEST STIMULUS rather than of a producer: with it low, bank claimant 0 is
//     never asked and the refusal path is exercised by the real services.
//
// ---------------------------------------------------------------------------
// THE FABRIC'S PARAMETERISATION IS THE CONSOLE'S CHOICE, AND UNTIL 2026-09-19
// IT WAS NOT ANYBODY'S
// ---------------------------------------------------------------------------
// `zhao_field_v3_engine.sv` names the SHIPPED configuration in its own header:
//
//   CTX=32 OUTSTANDING=16 LANES=4 LONGQ=16 DIST_BANKS=8 RING_UNITS=8 REGS=64
//
// and `tests/CMakeLists.txt` proves it -- `field_v3_earth_quad`, the gate the
// engine names, verilates at exactly those seven `-G` values. The engine's
// defaults are the SCALAR bench point and its header says so: "a fit that does
// not override them is measuring the bench."
//
// THIS FILE WAS OVERRIDING NOTHING. Two of the seven (CTX, REGS) arrived from
// `PROGS` and `REGS` here; the other FIVE were LITERALS in the instantiation --
// `.OUTSTANDING(4) .LANES(1) .LONGQ(4) .DIST_BANKS(2) .RING_UNITS(2)`. So the
// shipped configuration could not be selected from the console by anyone, not
// merely by nobody, and a console fit could only ever have measured a one-lane
// eight-context machine. That is the FLATTERING direction: a smaller number
// than the design ships, reported as the design's number.
//
// All seven are parameters now (`FAB_*` for the five that were literals). The
// defaults are the values this file already hard-coded, so exposing them
// changed no elaborated circuit -- the change is that there is now a knob.
//
// WHAT THE KNOB CANNOT FIX, SAID PLAINLY. The width is not free and this front
// cannot spend it. The run state machine below grants ONE client, zeroes ONE
// context, preloads ONE point's E record and answers ONCE:
//
//   E_IDLE -> E_ZERO (REGS clocks) -> E_WRITE (IN_LANES clocks) -> E_START
//          -> E_RUN -> E_RESP
//
// There is exactly one point in flight and exactly one context active, which
// the port comment on `resp_out_o` already states as a design fact. So:
//
//   * FAB_LANES>1 computes the same point on every lane and discards all but
//     lane 0 -- the linter says so itself at the `fab_wr_data` declaration.
//   * PROGS>8 buys RESIDENCY (more programs cached, fewer directory misses),
//     which is real, but not CONCURRENCY, because the front runs one at a time.
//   * REGS=64 is the uop encoding's native size -- the 64-bit word packs four
//     SIX-bit register fields -- but it also makes E_ZERO cost 64 clocks per
//     point instead of 32, on the critical path of every point this front
//     answers.
//
// The engine is not the thing that needs changing; THE FRONT IS. A front that
// gathers FAB_LANES points per grant, and keeps several contexts in flight, is
// what makes the shipped width pay, and it is not built. Until it is, the
// console's setting is an engineering argument rather than a default, and that
// argument lives at the instantiation in `zhao_console_core.sv`.
//
// ---------------------------------------------------------------------------
// THE ONE THING THAT IS ABSENT, NAMED PRECISELY
// ---------------------------------------------------------------------------
// Nothing inside the console loads a field program. `ld_*` is a real port on
// this module and on the core, and its owner is CMD.EXEC's TerrainField 0x0200
// arm (spec/commands.zidl: `handle32[program] program` -> cartridge PROGRAM
// page, spec/cartridge.md 3 kind 0) with the software decoder and planner
// beside it. That arm is not built.
//
// ---------------------------------------------------------------------------
// WHY IT IS A FRONT AND NOT AN ENGINE
// ---------------------------------------------------------------------------
// This file was called `zhao_field_engine` for most of its life and the name
// was wrong twice over. It is not an engine -- `zhao_field_v3_engine` is, and
// this is the loader, the directory, the arbiter and the lane front around it.
// And the two names are VERSION SIBLINGS to any tool that reads them, which is
// not a theory: `tools/quartus/check_console_inventory.py` G3 fired on
// exactly that pair -- "the console elaborates zhao_field_engine, but
// zhao_field_v3_engine exists" -- and it was right to.
//
// THE SECOND NAME WAS WRONG THE SAME WAY, which is the part worth recording.
// Renaming it `zhao_field_front` moved the collision rather than removing it:
// `zhao_field_v2_front` exists, so the gate fired again, on a different pair.
// The FIELD family has version-suffixed modules named core, engine, exec,
// front, curve, dispatch, len, mulbank, noise, normalize, rf, ring, rot, sbank,
// spline, svcpath, trig, wbarb and lanemux, and an unversioned module sharing
// ANY of those words reads as a superseded sibling. `host` is outside that
// set, and it is what the executor's own port comments already call this
// block: "A host that ignores it loses writes."
//
// ENFORCED-BY: tests/field/field_host_directed.cpp:main

`default_nettype none

module zhao_field_host #(
    // How many profile clients take turns on the fabric. There is no fixed
    // priority: a profile that can be starved by another is a frame that drops
    // one subsystem's field and not the other's, and that is invisible in a
    // framebuffer.
    parameter int unsigned CLIENTS = 2,
    // Resident programs. This is ONE number wearing three hats and that is the
    // point: it is `zhao_field_progcache`'s ENTRIES, it is the executor's
    // CONTEXT count, and it is this block's slot space. The v3 uop store is
    // indexed by context, so a program IS a context -- keeping a separate slot
    // namespace would be two indices that must agree with nothing forcing them.
    parameter int unsigned PROGS = 8,
    // Uops per program. The executor's own PLAN depth.
    parameter int unsigned INSTR_N = 32,
    // Registers per context, the executor's REGS.
    parameter int unsigned REGS = 32,
    // Constant (knot) tables the curve service holds, and entries per table.
    parameter int unsigned TABLES = 2,
    parameter int unsigned TBL_N = 64,
    // The input lanes preloaded into R0.. before the run, and the output lanes
    // captured from the write port during it. Twelve in is the ratified E
    // record of spec/form/field-ir.md 7.1 -- x, z, age, phase, p0..p7 -- and it
    // is the widest of the five profiles, so it is what the shared port
    // carries.
    parameter int unsigned IN_LANES = 12,
    parameter int unsigned OUT_LANES = 4,

    // ---- THE FABRIC'S OWN KNOBS, FORWARDED ----------------------------------
    // Until 2026-09-19 these were LITERALS in the `zhao_field_v3_engine`
    // instantiation below, which meant the engine's shipped parameterisation
    // could not be selected from the console BY ANYONE -- not merely that
    // nobody had. `field_v3_earth_quad` gates the engine at
    //
    //   CTX=32 OUTSTANDING=16 LANES=4 LONGQ=16 DIST_BANKS=8 RING_UNITS=8 REGS=64
    //
    // and `zhao_field_v3_engine.sv`'s own header says so in as many words,
    // adding "a fit that does not override them is measuring the bench". Five
    // of those seven had no route through this module at all, so the console's
    // fit could only ever have measured the scalar bench point. They are
    // parameters now, prefixed `FAB_` because `LANES` next to `IN_LANES` and
    // `OUT_LANES` in one parameter list means three different things.
    //
    // THE DEFAULTS HERE ARE THE VALUES THIS FILE ALREADY HARD-CODED, so
    // exposing them changes no elaborated circuit. What the console chooses is
    // `zhao_console_core.sv`'s business and the argument lives there.
    //
    // FAB_LANES is POINTS PER CONTEXT -- one instruction, FAB_LANES ALUs, one
    // register write carrying all of the results. It is not a lane of the E
    // record; `IN_LANES`/`OUT_LANES` are those.
    parameter int unsigned FAB_LANES = 1,
    // Long ops in flight, the depth of the long-op queue, and how many the
    // dispatcher gathers into one service request.
    parameter int unsigned FAB_OUTSTANDING = 4,
    parameter int unsigned FAB_LONGQ = 4,
    parameter int unsigned FAB_GATHERS = 4,
    // Root banks for LEN/DIST2, ring units, and the ring descriptor cache.
    parameter int unsigned FAB_DIST_BANKS = 2,
    // Points per long-op GROUP, 1..4: the dispatcher's cap and the distance
    // service's width together (zhao_field_v3_svcpath). Four is the shipped
    // configuration's; a front that holds one point in flight -- this one
    // does, see the run state machine -- can never fill a second lane.
    parameter int unsigned FAB_GROUP_PTS = 4,
    parameter int unsigned FAB_RING_UNITS = 2,
    parameter int unsigned FAB_RING_DESC = 2,

    // ---- DERIVED, AND WRITTEN OUT AS LITERALS ON PURPOSE. --------------------
    // These belong in the parameter list because the PORT list needs them and a
    // body localparam is not visible there. They are plain integers rather than
    // `$clog2` expressions because `tools/quartus/gen_prod_top.py` cannot
    // evaluate a parameter expression when it sizes a port, and a module it
    // cannot size is SKIPPED from the generated production top -- silently, and
    // in the flattering direction, since the manifest then reports the block as
    // accounted while no fit ever elaborates it. That was measured here, not
    // guessed: the generator printed "SKIPPED zhao_field_host ... unresolved
    // parameter expression: SLOTW-1" and went on to write a top without it.
    //
    // The cost of a literal is that it can disagree with the parameter it is
    // derived from, so the elaboration guards below check EVERY ONE against its
    // own expression and `$fatal` on a mismatch. DO NOT OVERRIDE these;
    // override the parameter each is derived from.
    parameter int unsigned SLOTW   = 3,  // $clog2(PROGS)   at PROGS   = 8
    parameter int unsigned PCW     = 5,  // $clog2(INSTR_N) at INSTR_N = 32
    parameter int unsigned REGW    = 5,  // $clog2(REGS)    at REGS    = 32
    parameter int unsigned TSELW   = 1,  // $clog2(TABLES)  at TABLES  = 2
    parameter int unsigned TIDXW   = 6,  // $clog2(TBL_N)   at TBL_N   = 64
    parameter int unsigned LDADDRW = 7   // max(PCW, TSELW + TIDXW)
) (
    input  logic clk,
    input  logic rst_n,

    // -----------------------------------------------------------------------
    // THE PROGRAM LOAD PORT -- the console's one field-program producer seam
    // -----------------------------------------------------------------------
    // Accepted only while the fabric is IDLE, so a load can never race a run
    // that is reading the uop store or a knot table.
    //
    // A LOAD OFFERED DURING A RUN DEFERS THE NEXT GRANT rather than queueing
    // behind an unbounded stream of client requests. Without that, a profile
    // issuing back-to-back points would starve the loader for a whole frame.
    //
    // `ld_kind_i`: 0 UOP, 1 TABLE ENTRY, 2 HEADER, 3 UNIFORM. The HEADER is
    // written LAST: it carries `out_base`, the REQUIRED-OUTPUT MASK, commits
    // both knot tables with their entry counts, and is the write that marks the
    // slot runnable, so a partially written program can never execute.
    //
    // THE HEADER WORD LAYOUT, which was written down nowhere until R101:
    //   [13:8]          out_base          (REGW bits)
    //   [22:16]         knot entry count
    //   [32 +: OUT_LANES] required-output mask; 0 = not declared
    //   everything else reserved, must be zero
    input  logic                       ld_valid_i,
    output logic                       ld_ready_o,
    input  logic [              1:0]   ld_kind_i,
    input  logic [SLOTW-1:0]           ld_slot_i,
    input  logic [LDADDRW-1:0]         ld_addr_i,
    input  logic [             95:0]   ld_data_i,

    // -----------------------------------------------------------------------
    // FIELD.PROGCACHE -- the residency directory, exported whole
    // -----------------------------------------------------------------------
    // Both phases face outward because the decode a miss requires is
    // `zfield::decode`'s and lives in software -- that block's contract says
    // the caller decodes and reports one bit. What is INTERNAL, and is why the
    // directory is composed in here rather than left at the console's edge, is
    // the insert: it invalidates this slot's program, so a profile can never
    // run microcode the directory has already promised to another hash.
    input  logic                       pc_lu_valid_i,
    output logic                       pc_lu_ready_o,
    input  logic [             31:0]   pc_lu_hash_i,
    output logic                       pc_lu_resp_valid_o,
    input  logic                       pc_lu_resp_ready_i,
    output logic                       pc_lu_hit_o,
    output logic [SLOTW-1:0]           pc_lu_slot_o,

    input  logic                       pc_cm_valid_i,
    output logic                       pc_cm_ready_o,
    input  logic [             31:0]   pc_cm_hash_i,
    input  logic                       pc_cm_ok_i,
    output logic                       pc_cm_resp_valid_o,
    input  logic                       pc_cm_resp_ready_i,
    output logic                       pc_cm_inserted_o,
    output logic                       pc_cm_evicted_o,
    output logic [SLOTW-1:0]           pc_cm_slot_o,

    output logic [             31:0]   pc_hits_o,
    output logic [             31:0]   pc_misses_o,
    output logic [             31:0]   pc_rejected_o,
    output logic [             31:0]   pc_evictions_o,
    output logic [SLOTW:0]             pc_occupancy_o,

    // -----------------------------------------------------------------------
    // THE PROFILE CLIENTS
    // -----------------------------------------------------------------------
    // Request data is per client because two may offer at once. The RESPONSE is
    // one shared bus with a one-hot valid, because exactly one point is in
    // flight through this front and replicating the result per client would be
    // paying for a concurrency it does not have.
    input  logic [CLIENTS-1:0]                 req_valid_i,
    output logic [CLIENTS-1:0]                 req_ready_o,
    input  logic [CLIENTS*SLOTW-1:0]           req_slot_i,
    // 1 = the caller already knows this slot holds no program for it; answer
    // ST_NO_PROGRAM without running. The Earth adapter will raise it for a
    // rectangle whose program hash missed the directory.
    input  logic [CLIENTS-1:0]                 req_noprog_i,
    input  logic [CLIENTS*IN_LANES*32-1:0]     req_in_i,

    output logic [CLIENTS-1:0]                 resp_valid_o,
    input  logic [CLIENTS-1:0]                 resp_ready_i,
    output logic [OUT_LANES*32-1:0]            resp_out_o,
    output logic [7:0]                         resp_status_o,

    // -----------------------------------------------------------------------
    // counters and traces
    // -----------------------------------------------------------------------
    output logic [31:0] runs_o,             // points that reached OP_END
    output logic [31:0] run_faults_o,       // points that ended on an alarm
    output logic [31:0] noprog_o,           // refused: slot holds no program
    output logic [31:0] instr_retired_o,    // the executor's uops issued
    output logic [31:0] loads_o,            // accepted load words
    output logic [31:0] load_defers_o,      // grants deferred to the loader
    output logic [31:0] grants_o,           // client grants, all clients
    output logic [31:0] contended_grants_o, // grants taken with >1 offering
    // A load word addressed past the uop store or a knot table. It is CLAMPED
    // rather than wrapped: a wrapped uop write lands on another instruction of
    // the same program, which is silent and produces a plausible field.
    output logic [31:0] ld_oob_o,
    // A point whose run produced no write to its declared output window. The
    // lanes then answer with the zeroes this block cleared them to, and a
    // caller reading only the lanes could not tell that from a field of zero.
    output logic [31:0] no_result_o,
    // A point whose run wrote SOME but not ALL of the lanes its header
    // declared required. Separate from `no_result_o` on purpose: that one
    // means the window was untouched, this one means the answer is a MIXTURE
    // of real values and cleared zeroes, which is the failure a caller cannot
    // see. Zero while no header declares a mask; fired with legal stimulus by
    // `tests/field/field_host_directed.cpp` case 1b.
    output logic [31:0] out_incomplete_o,
    // EVERY ALARM THE FABRIC OWNS, UNMERGED. `zhao_field_v3_engine`'s own header
    // is right that five faults reduced to one bit is a bit that says
    // "something, somewhere"; these are counted separately for the same reason.
    output logic [31:0] exec_desync_o,
    output logic [31:0] bank_desync_o,
    output logic [31:0] svc_bank_desync_o,
    output logic [31:0] tag_mismatch_o,
    output logic [31:0] wrong_op_o,
    output logic [31:0] unsupported_o,
    output logic [31:0] skid_overflow_o,
    output logic [31:0] uniform_bad_o,
    // {sat_rescale, sat_mul, sat_add} -- the op ledger, latched over the run.
    output logic [ 2:0] sat_o
);

  localparam int unsigned CIDW  = (CLIENTS   > 1) ? $clog2(CLIENTS)   : 1;
  localparam int unsigned LANEW = (IN_LANES  > 1) ? $clog2(IN_LANES)  : 1;
  localparam int unsigned OUTW  = (OUT_LANES > 1) ? $clog2(OUT_LANES) : 1;

  localparam logic [1:0] LdUop     = 2'd0;
  localparam logic [1:0] LdTable   = 2'd1;
  localparam logic [1:0] LdHeader  = 2'd2;
  localparam logic [1:0] LdUniform = 2'd3;

  // Statuses this block adds ON TOP of the fabric's own. They start at 0xF0 so
  // they can never collide with a ratified field-ir status.
  localparam logic [7:0] StNoProgram = 8'hF0;
  localparam logic [7:0] StNoResult  = 8'hF1;
  localparam logic [7:0] StAlarm     = 8'hF2;
  // A run that wrote SOME but not ALL of the lanes its header declared
  // REQUIRED. See `THE REQUIRED-OUTPUT MASK` below; this is the status W10
  // ("Do not make an absent output look like a zero result") asks for, and it
  // is distinct from StNoResult because "wrote nothing" and "wrote four of
  // seven" are different faults and merging them is the five-faults-one-bit
  // shape `zhao_field_v3_engine`'s header already argues against.
  localparam logic [7:0] StPartial   = 8'hF3;

  // ==========================================================================
  // THE REQUIRED-OUTPUT MASK -- header word bits [32 +: OUT_LANES]
  // ==========================================================================
  // WHY IT EXISTS. Until 2026-09-20 a run was called successful when
  // `cur_out_seen != '0` -- ANY write into the declared window. A point that
  // wrote five of its six declared lanes reported 8'h00 SUCCESS and the sixth
  // lane answered with the zero this block cleared it to at grant. That is
  // exactly the hazard the StNoResult comment beside it states, generalised
  // from "wrote nothing" to "wrote some", and the reasoning there covers it
  // without changing a word. Owner ruling R101.
  //
  // WHY THE GUARD COULD NOT HAVE BEEN WRITTEN BEFORE. "Required" was not a
  // quantity this interface carried: the header word gives `instr_count`,
  // `out_base` and the table counts, and nothing says how many lanes the
  // program's profile declares. `spec/form/field-ir.md` 7.1 DOES -- earth 4,
  // warp 6, flow 7, formation 6, stamp 3 -- and 5.3's I/O map names the
  // registers. So the mask is that ratified fact TRANSPORTED, not a new law.
  // The defect was upstream of the guard, which is why patching the guard
  // alone would have sent the next person to the wrong line.
  //
  // WHY IT IS ADDITIVE. Bits 32..95 of the 96-bit header word are unread by
  // the header path: the LdHeader decode touches only `ld_data_i[8 +: REGW]`
  // (out_base, bits 8..13 at the ceiling REGW=6) and `ld_data_i[22:16]` (the
  // table count). Sixty-four free bits, and `OUT_LANES <= REGS <= 64` by the
  // guards above, so the mask always fits. **`mask == 0` MEANS "NOT DECLARED"
  // AND KEEPS THE OLD TEST EXACTLY**, so no program written against the
  // previous contract changes meaning -- today no software emits a header word
  // at all (`zhao_field_doorbell` forwards the HPS's 96 bits verbatim), so the
  // legacy clause is genuinely inert rather than merely compatible.
  //
  // THE TWO SIDES OF THE COMPARISON ARE CLOCKED BY DIFFERENT THINGS, which is
  // the first question CLAUDE.md's metadata-swap chapter says to ask of any
  // checker. `cur_out_seen` is set from the FABRIC's arbiter write port during
  // E_RUN; `hdr_outreq` is written by the LOADER, which is accepted only in
  // E_IDLE. No register enable drives both, so the difference cannot be
  // corrupted in lockstep -- and the counter is fired by legal stimulus in
  // `tests/field/field_host_directed.cpp` case 1b rather than argued.

  // Quartus 17.0 rejects a module-scope `if`; the elaboration guards live in an
  // `initial begin ... end` per the house rule. `--lint-only` does not run
  // these, so a clean lint says nothing whatever about them.
  initial begin
    if (IN_LANES > REGS) begin
      $fatal(1, "zhao_field_host: IN_LANES=%0d exceeds REGS=%0d", IN_LANES, REGS);
    end
    if (OUT_LANES > REGS) begin
      $fatal(1, "zhao_field_host: OUT_LANES=%0d exceeds REGS=%0d", OUT_LANES, REGS);
    end
    // THE REQUIRED-OUTPUT MASK'S HOME, CHECKED RATHER THAN ASSERTED IN PROSE.
    // It occupies `ld_data_i[32 +: OUT_LANES]` of the 96-bit header word, so
    // OUT_LANES > 64 would run the mask off the end of the word -- silently,
    // since a part-select past the top of a vector reads zero and a mask of
    // zero means "nothing declared", i.e. the guard would switch itself OFF in
    // the flattering direction. That is the failure this line exists to stop.
    // It is implied by OUT_LANES <= REGS <= 64 today; it is stated anyway
    // because the two ceilings are set by different arguments and one of them
    // could move without the other.
    if (OUT_LANES > 64) begin
      $fatal(1, "zhao_field_host: OUT_LANES=%0d; the required-output mask lives in header bits [32 +: OUT_LANES] of a 96-bit word, so 64 is the ceiling", OUT_LANES);
    end
    if (CLIENTS < 1) $fatal(1, "zhao_field_host: CLIENTS must be at least 1");
    // A replication of zero is illegal and a fabric of zero lanes has no
    // result to read; see `fab_pre_data`.
    if (FAB_LANES < 1) $fatal(1, "zhao_field_host: FAB_LANES must be at least 1");
    if (FAB_OUTSTANDING < 1 || FAB_LONGQ < 1 || FAB_GATHERS < 1 ||
        FAB_DIST_BANKS < 1 || FAB_RING_UNITS < 1 || FAB_RING_DESC < 1) begin
      $fatal(1, "zhao_field_host: every fabric knob must be at least 1");
    end
    // THE 64-BIT UOP WORD IS WHY REGS=64 IS THE CEILING AND NOT A PREFERENCE.
    // The loader unpacks `ld_data_i` as
    //   [7:0] op  [13:8] dst  [19:14] a  [25:20] b  [31:26] c  [63:32] imm
    // which is FOUR SIX-BIT register fields, packed edge to edge with the
    // immediate. So the encoding's native register file is 64 deep: at REGS=32
    // the top bit of each field is read as zero and wasted, and at REGS=128
    // REGW becomes 7 and each `[N +: REGW]` slice OVERLAPS the next field --
    // `dst` would eat bit 14, which is `a`'s bit 0. That corruption is silent
    // and produces a program that runs and computes the wrong thing, which is
    // the worst available failure for a field. The shipped REGS=64 is exactly
    // the layout's size, and this guard is what says so at elaboration.
    if (REGW > 6) begin
      $fatal(1, "zhao_field_host: REGS=%0d needs REGW=%0d, but the 64-bit uop word packs four 6-bit register fields; REGS>64 overlaps them", REGS, REGW);
    end
    // Every derived width, checked against the expression it stands for. These
    // are the price of writing them as literals so the production-top generator
    // can size the ports; see the note in the parameter list.
    if (SLOTW != ((PROGS > 1) ? $clog2(PROGS) : 1)) begin
      $fatal(1, "zhao_field_host: SLOTW=%0d disagrees with clog2(PROGS=%0d)", SLOTW, PROGS);
    end
    if (PCW != ((INSTR_N > 1) ? $clog2(INSTR_N) : 1)) begin
      $fatal(1, "zhao_field_host: PCW=%0d disagrees with clog2(INSTR_N=%0d)", PCW, INSTR_N);
    end
    if (REGW != ((REGS > 1) ? $clog2(REGS) : 1)) begin
      $fatal(1, "zhao_field_host: REGW=%0d disagrees with clog2(REGS=%0d)", REGW, REGS);
    end
    if (TSELW != ((TABLES > 1) ? $clog2(TABLES) : 1)) begin
      $fatal(1, "zhao_field_host: TSELW=%0d disagrees with clog2(TABLES=%0d)", TSELW, TABLES);
    end
    if (TIDXW != ((TBL_N > 1) ? $clog2(TBL_N) : 1)) begin
      $fatal(1, "zhao_field_host: TIDXW=%0d disagrees with clog2(TBL_N=%0d)", TIDXW, TBL_N);
    end
    if (LDADDRW != ((PCW > (TSELW + TIDXW)) ? PCW : (TSELW + TIDXW))) begin
      $fatal(1, "zhao_field_host: LDADDRW=%0d disagrees with max(PCW, TSELW+TIDXW)", LDADDRW);
    end
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
  // THE PROGRAM HEADER, one row per slot (= per executor context)
  // ==========================================================================
  // `hdr_loaded` is the interlock between the directory and the uop store, and
  // it is why both live in this file. An INSERT marks the slot unloaded -- the
  // directory has promised it to a new hash and the new uops have not arrived,
  // so anything that ran from it now would run the DISPLACED program under the
  // new program's name. That is the stale-prepared-values failure class the
  // PROGCACHE contract names.
  logic [PROGS-1:0] hdr_loaded;
  logic [REGW-1:0]  hdr_outbase [0:PROGS-1];
  // The required-output mask, one per slot. See the chapter above the status
  // localparams. Read LIVE at completion, exactly as `hdr_outbase` is read
  // live by `out_hit_c`, and for the same reason: a load is accepted only in
  // E_IDLE, so no header write can race a run that is reading this.
  logic [OUT_LANES-1:0] hdr_outreq [0:PROGS-1];

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
  // Every port of `zhao_field_v3_engine` is driven or read here. The debug taps
  // and the per-stage clock buckets are declared and left unread on purpose:
  // they are the fabric's own evidence ports and dropping them from the
  // instantiation would be a PINCONNECTEMPTY that reads like a decision.
  logic                     fab_up_we;
  logic [SLOTW-1:0]         fab_up_ctx;
  logic [PCW-1:0]           fab_up_pc;
  logic [7:0]               fab_up_op;
  logic [REGW-1:0]          fab_up_dst, fab_up_a, fab_up_b, fab_up_c;
  logic [31:0]              fab_up_imm;

  logic                     fab_pre_we;
  logic [SLOTW-1:0]         fab_pre_ctx;
  logic [REGW-1:0]          fab_pre_reg;
  // THE FABRIC'S DATA PORTS ARE LANE-WIDE. A context holds FAB_LANES points and
  // one register write carries all of their results, so these are 32*FAB_LANES
  // however many points THIS FRONT has. At FAB_LANES=1 they are the 32 bits
  // they have always been and nothing below changes.
  logic signed [32*FAB_LANES-1:0] fab_pre_data;

  // `pre_ready_o` is READ, not merely observed -- see the E_ZERO comment.
  logic                     fab_pre_ready;
  logic                     fab_start;
  logic [SLOTW-1:0]         fab_start_ctx;

  logic                     fab_tl_we, fab_tl_commit;
  logic [1:0]               fab_tl_tbl;  // the fabric's port is 2 bits
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
  // THE UNUSED BITS HERE ARE THE MEASUREMENT, NOT AN OVERSIGHT. At FAB_LANES=4
  // the linter reports "Bits of signal are not used: 'fab_wr_data'[127:32]" --
  // ninety-six of the fabric's one hundred and twenty-eight result bits, three
  // quarters of a quad machine's output, discarded because this front has one
  // point in flight and reads lane 0. That warning is the clearest single piece
  // of evidence that the width cannot pay behind a scalar front, so it is
  // recorded here in the waiver rather than deleted by silencing it globally.
  // At FAB_LANES=1 there is nothing to waive and the warning does not arise.
  /* verilator lint_off UNUSEDSIGNAL */
  logic signed [32*FAB_LANES-1:0] fab_wr_data;
  /* verilator lint_on UNUSEDSIGNAL */

  logic                     fab_unsupported, fab_exec_desync, fab_bank_desync;
  logic                     fab_svc_bank_desync, fab_tag_mismatch, fab_wrong_op;
  logic                     fab_sk_overflow, fab_sb_bad, fab_imm_bad;
  logic                     fab_sat_add, fab_sat_mul, fab_sat_rescale;
  logic [31:0]              fab_uops_issued;

  /* verilator lint_off UNUSEDSIGNAL */
  // The fabric's per-stage evidence. Declared and unread HERE: these are read
  // by the differential that owns the fabric, and re-exporting all of them
  // through the console's edge would add forty ports nothing in the console can
  // interpret. They are named rather than left empty so a reader can see
  // exactly what is being declined.
  logic [PROGS-1:0] fab_active;
  logic [31:0] fab_idle_clocks, fab_hold_clocks, fab_blocked_clocks;
  logic [31:0] fab_denied_clocks, fab_dot_clocks, fab_skid_clocks, fab_rf_writes;
  logic [31:0] fab_groups, fab_partial, fab_drain_writes;
  logic [31:0] fab_mul_grants, fab_mul_stall_lanes;
  logic [31:0] fab_bank_both, fab_bank_engine_only, fab_bank_svc_only, fab_bank_neither;
  logic [31:0] fab_svc_taken [7];
  logic [31:0] fab_svc_refused [7];
  logic [31:0] fab_ring_req_taken, fab_ring_fetch_clocks, fab_ring_hand_wait;
  logic [31:0] fab_ring_desc_hit, fab_ring_desc_miss;
  logic [31:0] fab_wb_served [2];
  logic [31:0] fab_wb_stalled [2];
  logic fab_dbg_long_valid, fab_dbg_long_ready, fab_dbg_s2_v;
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

      // THE BANK RIVAL IS TEST STIMULUS, NOT A PRODUCER. It exists so a
      // differential can make the executor's multiplier bank refuse. Tied low,
      // claimant 0 is never asked and the refusal path is driven by the real
      // services instead.
      .rival_req_i(1'b0),

      // Drain-first. `zhao_field_v3_wbarb`'s measurement is already in and it
      // is not symmetric: ALU-first STARVES the drain outright, drain-first
      // costs the ALU exactly eight clocks per four-point group. A starved
      // drain is a long op whose answer never lands, which is a wrong field
      // rather than a slow one.
      .wb_policy_i(2'd1),

      .done_valid_o(fab_done_valid),
      .done_ctx_o  (fab_done_ctx),
      .active_o    (fab_active),
      .unsupported_o(fab_unsupported),
      .sat_add_o    (fab_sat_add),
      .sat_mul_o    (fab_sat_mul),
      .sat_rescale_o(fab_sat_rescale),

      .wr_en_o  (fab_wr_en),
      .wr_ctx_o (fab_wr_ctx),
      .wr_reg_o (fab_wr_reg),
      .wr_data_o(fab_wr_data),

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

      .mul_grants_o     (fab_mul_grants),
      .mul_stall_lanes_o(fab_mul_stall_lanes),
      .bank_both_o        (fab_bank_both),
      .bank_engine_only_o (fab_bank_engine_only),
      .bank_svc_only_o    (fab_bank_svc_only),
      .bank_neither_o     (fab_bank_neither),

      .svc_taken_o        (fab_svc_taken),
      .svc_refused_o      (fab_svc_refused),
      .ring_req_taken_o   (fab_ring_req_taken),
      .ring_fetch_clocks_o(fab_ring_fetch_clocks),
      .ring_hand_wait_o   (fab_ring_hand_wait),
      .ring_desc_hit_o    (fab_ring_desc_hit),
      .ring_desc_miss_o   (fab_ring_desc_miss),
      .wb_served_o        (fab_wb_served),
      .wb_stalled_o       (fab_wb_stalled),

      .exec_desync_o    (fab_exec_desync),
      .bank_desync_o    (fab_bank_desync),
      .svc_bank_desync_o(fab_svc_bank_desync),
      .tag_mismatch_o   (fab_tag_mismatch),
      .wrong_op_o       (fab_wrong_op),

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
  localparam logic [2:0] E_IDLE  = 3'd0;
  localparam logic [2:0] E_ZERO  = 3'd1;  // the law's `reg[0..REGS-1] = 0`
  localparam logic [2:0] E_WRITE = 3'd2;  // the declared input lanes
  localparam logic [2:0] E_START = 3'd3;
  localparam logic [2:0] E_RUN   = 3'd4;
  localparam logic [2:0] E_RESP  = 3'd5;

  logic [2:0]         state;
  logic [CIDW-1:0]    cur_id;
  logic [SLOTW-1:0]   cur_slot;
  logic [REGW:0]      zero_i;
  logic [LANEW:0]     lane_i;
  logic signed [31:0] cur_in  [0:IN_LANES-1];
  logic signed [31:0] cur_out [0:OUT_LANES-1];
  logic [OUT_LANES-1:0] cur_out_seen;
  logic [7:0]         cur_status;
  logic [2:0]         cur_sat;

  // The write port is watched, not reconstructed. `wr_*` is the ARBITER's
  // output, so it is the register file's write as it actually happens -- the
  // ALU's and the long-op drain's alike. A front that latched the ALU's request
  // instead would record writes the arbiter refused.
  wire out_hit_c = fab_wr_en && (fab_wr_ctx == cur_slot) &&
                   (fab_wr_reg >= hdr_outbase[cur_slot]) &&
                   ((int'(fab_wr_reg) - int'(hdr_outbase[cur_slot])) < int'(OUT_LANES));
  wire [OUTW-1:0] out_idx_c = OUTW'(int'(fab_wr_reg) - int'(hdr_outbase[cur_slot]));

  // The running point's declared-required set, and the two verdicts drawn from
  // it. `req_mask_c == 0` is "the header declared nothing", which is the ONLY
  // case that keeps the pre-R101 test -- and it keeps it exactly.
  // The two verdicts are DISJOINT and neither changes `no_result_o`'s meaning.
  // "Wrote nothing" stays StNoResult whatever the mask says -- it is the more
  // specific description and it is the one that counter's header documents.
  // StPartial is the NEW state: some lanes landed and a required one did not.
  wire [OUT_LANES-1:0] req_mask_c = hdr_outreq[cur_slot];
  wire out_none_c       = (cur_out_seen == '0);
  wire out_incomplete_c = (req_mask_c != '0) && (cur_out_seen != '0) &&
                          ((cur_out_seen & req_mask_c) != req_mask_c);

  wire alarm_c = fab_unsupported || fab_exec_desync || fab_bank_desync ||
                 fab_svc_bank_desync || fab_tag_mismatch || fab_wrong_op ||
                 fab_sk_overflow || fab_sb_bad || fab_imm_bad;

  // ONLY THE UOP ADDRESS CAN BE OUT OF RANGE, and the first version of this
  // check got that wrong in a way worth recording. It also compared the table
  // address against `LDADDRW'(TABLES * TBL_N)` -- which at the defaults is
  // 7'(128), and 128 does not fit in seven bits, so the limit was ZERO and the
  // comparison was constant TRUE. Every knot-table write would have been
  // counted out of range and DROPPED, and the curve service would have read an
  // empty table and answered faithfully with the curve of nothing.
  //
  // The repair is not a wider limit: the table address field is
  // {table, entry} = TSELW + TIDXW bits, which is EXACTLY the table space, so
  // no value of it is out of range and there is nothing to check. Verilator's
  // UNSIGNED warning is what found this; it was one line of "comparison is
  // constant due to unsigned arithmetic" in a file that otherwise linted clean.
  wire [LDADDRW-1:0] ld_uop_limit = LDADDRW'(INSTR_N);
  wire ld_oob_c = ld_valid_i && ld_ready_o &&
                  (ld_kind_i == LdUop) && (ld_addr_i >= ld_uop_limit);

  // ---- handshakes ----------------------------------------------------------
  assign ld_ready_o = (state == E_IDLE);

  always_comb begin
    req_ready_o = '0;
    if ((state == E_IDLE) && !ld_valid_i && pick_any) req_ready_o[pick_id] = 1'b1;
  end

  always_comb begin
    resp_valid_o = '0;
    if (state == E_RESP) resp_valid_o[cur_id] = 1'b1;
  end

  integer oi;
  always_comb begin
    resp_out_o = '0;
    for (oi = 0; oi < int'(OUT_LANES); oi = oi + 1) begin
      resp_out_o[(oi*32) +: 32] = cur_out[oi];
    end
  end
  assign resp_status_o   = cur_status;
  assign sat_o           = cur_sat;
  assign instr_retired_o = fab_uops_issued;

  // ---- the fabric's load and preload ports --------------------------------
  wire [LANEW-1:0] lane_sel = (lane_i < (LANEW+1)'(IN_LANES)) ? lane_i[LANEW-1:0] : '0;

  always_comb begin
    // The uop word packs exactly into the fabric's five instruction ports, no
    // padding, and it is the SAME 64-bit layout the v1 front used -- the
    // executor takes CANONICAL opcodes, so the loader's encoding did not have
    // to change when the engine did.
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

    // A knot table entry. `ld_addr_i` is {table, entry}.
    fab_tl_we  = ld_valid_i && ld_ready_o && (ld_kind_i == LdTable) && !ld_oob_c;
    fab_tl_tbl = 2'(ld_addr_i[(TSELW+TIDXW)-1 -: TSELW]);
    fab_tl_idx = ld_addr_i[TIDXW-1:0];
    fab_tl_x   = ld_data_i[31:0];
    fab_tl_y   = ld_data_i[63:32];
    fab_tl_dy  = ld_data_i[95:64];

    // A uniform. The plan's PREP block writes these once per association.
    fab_sb_we    = ld_valid_i && ld_ready_o && (ld_kind_i == LdUniform);
    fab_sb_waddr = ld_data_i[95:80];
    fab_sb_wdata = ld_data_i[31:0];

    // THE HEADER COMMITS THE KNOT TABLE, which is why the commit is not a
    // fourth load kind: a table whose entry count arrived separately could be
    // read with the previous program's length, and the header is already the
    // write that makes a slot runnable.
    fab_tl_commit = ld_valid_i && ld_ready_o && (ld_kind_i == LdHeader);
    fab_tl_n      = ld_data_i[22:16];

    // The preload port: zeroes first, then the declared lanes.
    fab_pre_we   = (state == E_ZERO) ||
                   ((state == E_WRITE) && (lane_i < (LANEW+1)'(IN_LANES)));
    fab_pre_ctx  = cur_slot;
    fab_pre_reg  = (state == E_ZERO) ? zero_i[REGW-1:0] : REGW'(lane_i);
    // THE POINT IS REPLICATED ACROSS THE FABRIC'S LANES, AND THAT IS THE COST
    // OF A QUAD FABRIC BEHIND A SCALAR FRONT, SAID OUT LOUD. This front has
    // EXACTLY ONE POINT IN FLIGHT -- `req_in_i` carries one E record, the run
    // state machine grants one client, zeroes one context and answers once --
    // so at FAB_LANES>1 the other lanes recompute the same point and their
    // results are discarded.
    //
    // They are fed the REAL point rather than zero on purpose. A fabricated
    // zero point is stimulus this front never received, and it would reach the
    // saturation ledger and the service alarms, which are latched over the run
    // and reported to the caller as `sat_o` and `resp_status_o`. Padding with a
    // value that can raise an alarm makes the status describe the padding.
    // Replication cannot: every lane computes what lane 0 computes.
    //
    // This is a WASTE, not a fix, and it is the reason FAB_LANES>1 is not the
    // console's setting -- see the argument at the instantiation in
    // `zhao_console_core.sv`. A front that gathers FAB_LANES points per grant
    // is the thing that makes the width pay, and it is not built.
    fab_pre_data = (state == E_ZERO) ? '0
                                     : {FAB_LANES{cur_in[lane_sel]}};

    fab_start     = (state == E_START);
    fab_start_ctx = cur_slot;
  end

  integer k;
  integer li;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state      <= E_IDLE;
      rr_ptr     <= '0;
      cur_id     <= '0;
      cur_slot   <= '0;
      zero_i     <= '0;
      lane_i     <= '0;
      cur_status <= 8'd0;
      cur_sat    <= 3'd0;
      cur_out_seen <= '0;
      hdr_loaded <= '0;
      for (k = 0; k < int'(PROGS); k = k + 1) hdr_outbase[k] <= '0;
      for (k = 0; k < int'(PROGS); k = k + 1) hdr_outreq[k] <= '0;
      for (k = 0; k < int'(IN_LANES); k = k + 1) cur_in[k] <= 32'sd0;
      for (k = 0; k < int'(OUT_LANES); k = k + 1) cur_out[k] <= 32'sd0;
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
      exec_desync_o      <= 32'd0;
      bank_desync_o      <= 32'd0;
      svc_bank_desync_o  <= 32'd0;
      tag_mismatch_o     <= 32'd0;
      wrong_op_o         <= 32'd0;
      unsupported_o      <= 32'd0;
      skid_overflow_o    <= 32'd0;
      uniform_bad_o      <= 32'd0;
    end else begin
      // ---- the alarms, counted separately and never OR-ed ------------------
      if (fab_exec_desync && (exec_desync_o != 32'hFFFF_FFFF)) begin
        exec_desync_o <= exec_desync_o + 32'd1;
      end
      if (fab_bank_desync && (bank_desync_o != 32'hFFFF_FFFF)) begin
        bank_desync_o <= bank_desync_o + 32'd1;
      end
      if (fab_svc_bank_desync && (svc_bank_desync_o != 32'hFFFF_FFFF)) begin
        svc_bank_desync_o <= svc_bank_desync_o + 32'd1;
      end
      if (fab_tag_mismatch && (tag_mismatch_o != 32'hFFFF_FFFF)) begin
        tag_mismatch_o <= tag_mismatch_o + 32'd1;
      end
      if (fab_wrong_op && (wrong_op_o != 32'hFFFF_FFFF)) begin
        wrong_op_o <= wrong_op_o + 32'd1;
      end
      if (fab_unsupported && (unsupported_o != 32'hFFFF_FFFF)) begin
        unsupported_o <= unsupported_o + 32'd1;
      end
      if (fab_sk_overflow && (skid_overflow_o != 32'hFFFF_FFFF)) begin
        skid_overflow_o <= skid_overflow_o + 32'd1;
      end
      if ((fab_sb_bad || fab_imm_bad) && (uniform_bad_o != 32'hFFFF_FFFF)) begin
        uniform_bad_o <= uniform_bad_o + 32'd1;
      end
      if (ld_oob_c && (ld_oob_o != 32'hFFFF_FFFF)) ld_oob_o <= ld_oob_o + 32'd1;

      // An insert invalidates the slot's program. See the note on hdr_loaded.
      if (pc_cm_valid_i && pc_cm_ready_o && pc_cm_ok_i) begin
        hdr_loaded[pc_cm_slot_c] <= 1'b0;
      end

      // ---- the output window, watched on the ARBITER's write port ----------
      // Outside E_IDLE only: between runs `cur_slot` still names the last
      // point's context, and a late drain write landing then belongs to the run
      // that has already answered.
      if ((state == E_RUN) && out_hit_c) begin
        // Lane 0 is this front's point. The rest are the replicas the preload
        // wrote; see the note there.
        cur_out[out_idx_c]      <= fab_wr_data[31:0];
        cur_out_seen[out_idx_c] <= 1'b1;
      end
      if (state == E_RUN) begin
        cur_sat <= cur_sat | {fab_sat_rescale, fab_sat_mul, fab_sat_add};
      end

      case (state)
        E_IDLE: begin
          if (ld_valid_i) begin
            if (pick_any && (load_defers_o != 32'hFFFF_FFFF)) begin
              load_defers_o <= load_defers_o + 32'd1;
            end
            if (loads_o != 32'hFFFF_FFFF) loads_o <= loads_o + 32'd1;
            if (ld_kind_i == LdHeader) begin
              hdr_outbase[ld_slot_i] <= ld_data_i[8 +: REGW];
              // The required-output mask. Zero means the program declared
              // nothing and the pre-R101 completion test applies unchanged.
              hdr_outreq[ld_slot_i]  <= ld_data_i[32 +: OUT_LANES];
              // The header is written LAST by the loader and is what marks the
              // slot runnable, so a partially written program can never be
              // executed. Enforced from the other side too: any uop, table or
              // uniform write clears the bit again.
              hdr_loaded[ld_slot_i] <= 1'b1;
            end else begin
              hdr_loaded[ld_slot_i] <= 1'b0;
            end
          end else if (pick_any) begin
            cur_id     <= pick_id;
            cur_slot   <= req_slot_i[(int'(pick_id)*int'(SLOTW)) +: SLOTW];
            for (li = 0; li < int'(IN_LANES); li = li + 1) begin
              cur_in[li] <= req_in_i[((int'(pick_id)*int'(IN_LANES) + li)*32) +: 32];
            end
            for (li = 0; li < int'(OUT_LANES); li = li + 1) cur_out[li] <= 32'sd0;
            cur_out_seen <= '0;
            cur_sat      <= 3'd0;
            rr_ptr       <= CIDW'((int'(pick_id) + 1) % int'(CLIENTS));
            zero_i       <= '0;
            lane_i       <= '0;
            if (grants_o != 32'hFFFF_FFFF) grants_o <= grants_o + 32'd1;
            if (offers_many && (contended_grants_o != 32'hFFFF_FFFF)) begin
              contended_grants_o <= contended_grants_o + 32'd1;
            end
            if (req_noprog_i[pick_id] ||
                !hdr_loaded[req_slot_i[(int'(pick_id)*int'(SLOTW)) +: SLOTW]]) begin
              // REFUSED, never faked. A field result is exactly the place a
              // plausible constant proves nothing, so the STATUS is the
              // load-bearing part of this answer and every adapter reads it.
              if (noprog_o != 32'hFFFF_FFFF) noprog_o <= noprog_o + 32'd1;
              cur_status <= StNoProgram;
              state      <= E_RESP;
            end else begin
              cur_status <= 8'd0;
              state      <= E_ZERO;
            end
          end
        end

        // The law's first line is `reg[0..REGS-1] = 0`. The v3 preload port
        // writes one register per clock and has no bulk clear, so the zeroing
        // is explicit -- REGS clocks, once per point. It is not optional: a
        // context holds the PREVIOUS point's registers, and a program that
        // reads a register it did not write would read that point's value.
        //
        // `pre_ready_o` IS HONOURED, and it is not decoration.
        // `zhao_field_v3_exec.sv:208` says it in as many words: "The preload
        // landed this clock. A host that ignores it loses writes." A front that
        // advanced its counter on every clock would silently drop whichever
        // registers fell on a busy cycle -- and a dropped ZERO is the worst of
        // them, because the register then holds the previous point's value and
        // the field is plausible everywhere.
        E_ZERO: begin
          if (fab_pre_ready) begin
            if (zero_i < (REGW+1)'(REGS - 1)) begin
              zero_i <= zero_i + 1'b1;
            end else begin
              state <= E_WRITE;
            end
          end
        end

        E_WRITE: begin
          if (fab_pre_ready) begin
            if (lane_i < (LANEW+1)'(IN_LANES)) begin
              lane_i <= lane_i + 1'b1;
            end else begin
              state <= E_START;
            end
          end
        end

        E_START: state <= E_RUN;

        E_RUN: begin
          if (fab_done_valid && (fab_done_ctx == cur_slot)) begin
            if (alarm_c) begin
              cur_status <= StAlarm;
              if (run_faults_o != 32'hFFFF_FFFF) run_faults_o <= run_faults_o + 32'd1;
            end else if (out_none_c) begin
              // The run completed and wrote NOTHING into its declared output
              // window. The lanes are the zeroes cleared at grant, and a caller
              // reading only the lanes could not tell that from a field whose
              // value is zero. So it is a status and a counter.
              cur_status <= StNoResult;
              if (no_result_o != 32'hFFFF_FFFF) no_result_o <= no_result_o + 32'd1;
            end else if (out_incomplete_c) begin
              // THE SAME HAZARD, ONE STEP ALONG, and the step the guard above
              // used to stop short of. Some declared lanes carry real values
              // and at least one required lane carries the zero cleared at
              // grant -- an answer a caller reading only the lanes cannot tell
              // from a field that happens to be zero there, which is W10's
              // "Do not make an absent output look like a zero result".
              // REFUSED, not clamped and not patched: a silent wrong field
              // value is worse than a refusal because the consumer has no way
              // to know. Owner ruling R101.
              cur_status <= StPartial;
              if (out_incomplete_o != 32'hFFFF_FFFF) begin
                out_incomplete_o <= out_incomplete_o + 32'd1;
              end
            end else begin
              cur_status <= 8'd0;
              if (runs_o != 32'hFFFF_FFFF) runs_o <= runs_o + 32'd1;
            end
            state <= E_RESP;
          end
        end

        E_RESP: begin
          if (resp_ready_i[cur_id]) state <= E_IDLE;
        end

        default: state <= E_IDLE;
      endcase
    end
  end

endmodule : zhao_field_host

`default_nettype wire
