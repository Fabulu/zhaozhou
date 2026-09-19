// zhao_field_engine.sv — FIELD.SEQ.CORE as a console organ: one sequencer, its
// program store, its residency directory, and the arbiter that lets more than
// one profile take turns on it.
//
// Contract: design/contracts/FIELD.SEQ.CORE.md
// Reference: `zfield::interpret` (reference/src/zfield/zfield_interpret.cpp),
//            reached through `zhao_field_seq`, which is the exact serial law.
//
// ---------------------------------------------------------------------------
// WHY THIS FILE EXISTS, AND WHY IT IS ONE ENGINE RATHER THAN THREE
// ---------------------------------------------------------------------------
// `zhao_console_core.sv` carried three separate boundary entries -- I5
// (PART.UPDATE's field sample), I31 (SURFACE.STAMP's brush) and I34
// (TERRAIN.PATCH's field-height lane) -- each of which named a DIFFERENT absent
// owner: FIELD.SEQ.FLOW, FIELD.SEQ.STAMP and FIELD.SEQ.EARTH. Each entry says
// its owner "is not built".
//
// THAT IS NOT WHAT THE LEDGER SAYS, and the correction is the whole reason this
// block has the shape it has. `design/contracts/FIELD.SEQ.EARTH.md`,
// `.FLOW.md` and `.STAMP.md` are identical on the point and say it in as many
// words:
//
//   > Owner ruling, 2026-08-22: **one engine, five profiles.** This contract
//   > describes a CONFIGURATION of `FIELD.SEQ.CORE` ... There is no separate
//   > `FIELD.SEQ.EARTH` sequencer in hardware and there is not going to be one.
//
//   > `zhao_field_seq` has no profile input and no profile-specific port. The
//   > thing that would distinguish a profile -- which registers the input and
//   > output lanes bind to -- is carried by the DECODED PROGRAM ... So a
//   > profile is a program set plus shell wiring, not a hardware variant.
//
// `design/blocks.yml` records all five as `kind: profile` with
// `implemented_by: FIELD.SEQ.CORE`, rule V21. So the three entries were each
// waiting for a block that is RULED never to exist, while the block that DOES
// implement all three -- `zhao_field_seq`, maturity RTL_VERIFIED, fit-measured
// at 4,494 ALM / 3,725 regs / 5 M10K / **3 DSP** / 58.99 MHz on a CLEAN tree --
// sat outside the console entirely. This file is that engine, composed once.
//
// WHAT THE SHARING SAVES, stated so it is checkable rather than asserted:
//
//   * THREE PROFILES, ONE SEQUENCER. Wiring EARTH, FLOW and STAMP each to
//     their own engine would be 3 x 4,494 = 13,482 ALM and 9 DSP. One engine
//     with an arbiter is 4,494 ALM plus the arbiter, so the sharing saves
//     ~8,988 ALM and 6 DSP against the obvious composition. ALMs bind here
//     (47,582 measured against a 41,910 budget), so this is the difference
//     between the organ being affordable and not.
//   * ELEVEN OP UNITS, ONE MULTIPLIER. That saving is already inside
//     `zhao_field_exec_shared` and is quoted here because it is the reason the
//     4,494 figure is small enough to compose at all: the first synthesis of
//     this engine measured **79 DSPs of 112** with each op unit holding its own
//     multiplier. Sharing took it to 3.
//   * NO SECOND PROGRAM STORE, NO SECOND DIRECTORY. One instruction memory, one
//     table memory and one `zhao_field_progcache` serve every profile, and the
//     store is M10K rather than logic -- the owner's ruling that memory is the
//     slack and ALMs are the debt.
//
// ---------------------------------------------------------------------------
// WHAT IS HERE AND WHAT IS DELIBERATELY NOT
// ---------------------------------------------------------------------------
// HERE: the sequencer, the program store (the sequencer's own header is
// explicit that "the shell owns the memory" for both the instruction and the
// table port), the residency directory, and a round-robin arbiter.
//
// NOT HERE:
//
//   * VALIDATION. `zfield::decode` is the single implementation of the
//     spec/form/field-ir.md 4/5 law. The loader decodes and reports one bit to
//     the directory, exactly as `FIELD.PROGCACHE`'s contract requires. Nothing
//     in this file re-derives a rule.
//   * THE LANE MAP. Which registers a program's inputs and outputs bind to is
//     per-program metadata, and it is written into this block's program header
//     by the loader rather than wired in. `out_base` is a header field for that
//     reason: a profile that reads its outputs from R12 is a program property,
//     not a variant of this module.
//   * A PROFILE. There is no profile input. The adapters that generate and
//     consume the varying lanes are separate files, which is what
//     FIELD.SEQ.CORE.md calls a "STREAM ADAPTER" and permits explicitly:
//     "Adapters generate and consume streams; they NEVER re-implement an op,
//     and there are not five engines."
//
// ---------------------------------------------------------------------------
// THE ENGINE IS v1, AND THAT IS A STATED CHOICE, NOT AN OVERSIGHT
// ---------------------------------------------------------------------------
// `design/prod_manifest.yml` names FIELD v3 the production generation. v3's
// EXECUTOR is not built: it exists as `fpga/rtl/synth/zhao_probe_v3_exec.sv`,
// a probe, and the only composition of it -- `zhao_probe_v3_full.sv` -- carries
// a live deadlock in its own header ("a program containing SPLINE or RING PARKS
// THAT CONTEXT FOREVER"), whose repair that file says is not an agent's to
// make. No production `zhao_field_v3_*` module has a fit row at all; every v3
// number on disk is a probe number.
//
// Composing a machine that is known to park a context forever would be
// composing a circuit already known to be wrong. So this organ is built on the
// engine that is COMPLETE, EXACT and MEASURED -- v1, which
// `design/contracts/FIELD.SEQ.CORE.md` names "the exact serial reference and
// differential oracle" and which `zhao_field_seq.sv`'s own freeze header names
// "the FALLBACK". A fallback that is not composed is not a fallback.
//
// v1 is ~7x short of the Earth60 THROUGHPUT target and that is measured, not
// hoped (reports/EARTH60_CAPACITY.md). It is not short of the SEMANTICS by one
// bit. When the v3 executor is promoted out of `synth/` and its dispatcher
// disagreement is settled, it replaces `u_seq` inside this file and nothing
// above this line changes -- which is the other reason the arbiter, the store
// and the directory live here rather than inside a profile.
//
// ---------------------------------------------------------------------------
// THE ONE THING THAT IS ABSENT, NAMED PRECISELY
// ---------------------------------------------------------------------------
// Nothing inside the console loads a program. `ld_*` is a real port on this
// module and on the core, and its owner is CMD.EXEC's TerrainField 0x0200 arm
// (spec/commands.zidl: `handle32[program] program` -> cartridge PROGRAM page,
// spec/cartridge.md 3 kind 0) together with the software decoder. That arm is
// not built. It is ONE named absent producer for a port that is loadable by the
// board and by every test, which is a smaller and more actionable gap than
// three entries each waiting on a block that will never exist.
//
// ENFORCED-BY: tests/field/field_engine_directed.cpp:main

`default_nettype none

module zhao_field_engine #(
    // How many profile clients take turns on the sequencer. There is no fixed
    // priority: a profile that can be starved by another profile is a frame
    // that drops one subsystem's field and not the other's, and that is
    // invisible in a framebuffer.
    parameter int unsigned CLIENTS = 2,
    // Resident programs. Matches `zhao_field_progcache`'s ENTRIES exactly: the
    // directory hands out the slot this store is addressed by, so two different
    // sizes would be two different address spaces wearing one name.
    parameter int unsigned PROGS = 8,
    // Instructions per program. `zhao_field_seq.instr_count_i` is 8 bits, so
    // 256 is the ceiling the sequencer itself sets; 64 is what the committed
    // Earth programs need with headroom, and it is a knob rather than a
    // constant because it is the store's dominant term.
    parameter int unsigned INSTR_N = 64,
    // Constant tables per program, and entries per table. CURVE, DCURVE and
    // SPLINE read `prog.tables[imm]`; `zhao_field_seq.tbl_idx_o` is 6 bits, so
    // 64 entries is that port's own ceiling.
    parameter int unsigned TABLES = 2,
    parameter int unsigned TBL_N = 64,
    // The input lanes written into R0.. before the walk, and the output lanes
    // read from R[out_base].. after it. Twelve in is the ratified E record of
    // spec/form/field-ir.md 7.1 -- x, z, age, phase, p0..p7 -- and it is the
    // widest of the five profiles, so it is the width the shared port carries.
    //
    // WRITING A LANE A PROGRAM DID NOT DECLARE IS BIT-EXACT AND IS NOT A
    // SHORTCUT. The law's first line is `reg[0..63] = 0`, which this block
    // performs with `clear_i` before any lane is written, so a zero written
    // into an undeclared lane leaves the file in the state the law demands.
    parameter int unsigned IN_LANES = 12,
    parameter int unsigned OUT_LANES = 4,

    // ---- DERIVED. Present in the parameter list only because the port list
    // ---- needs them and a localparam is not visible there. DO NOT OVERRIDE;
    // ---- the elaboration guards below refuse an override that disagrees.
    parameter int unsigned SLOTW  = (PROGS   > 1) ? $clog2(PROGS)   : 1,
    parameter int unsigned PCW    = (INSTR_N > 1) ? $clog2(INSTR_N) : 1,
    parameter int unsigned TSELW  = (TABLES  > 1) ? $clog2(TABLES)  : 1,
    parameter int unsigned TIDXW  = (TBL_N   > 1) ? $clog2(TBL_N)   : 1,
    parameter int unsigned IADDRW = SLOTW + PCW,
    parameter int unsigned TADDRW = SLOTW + TSELW + TIDXW,
    // The load address is WITHIN a slot -- `ld_slot_i` names the slot for every
    // kind, including the two that write a RAM. Carrying the slot twice, once
    // in `ld_slot_i` and once in the high bits of an address, is two places
    // that must agree with nothing forcing them to.
    parameter int unsigned LDADDRW = (PCW > (TSELW + TIDXW)) ? PCW : (TSELW + TIDXW)
) (
    input  logic clk,
    input  logic rst_n,

    // -----------------------------------------------------------------------
    // THE PROGRAM LOAD PORT -- the console's one field-program producer seam
    // -----------------------------------------------------------------------
    // Accepted only while the sequencer is IDLE. That is not a convenience: the
    // instruction and table memories are read by a running walk, and
    // `zhao_dc_sdp_ram`'s header states that read-during-write at one address
    // is outside its protocol and must be made unreachable BY OWNERSHIP. This
    // is that ownership rule, and it is structural rather than advisory --
    // `ld_ready_o` is low for the whole of a run.
    //
    // A LOAD OFFERED DURING A RUN DEFERS THE NEXT GRANT rather than queueing
    // behind an unbounded stream of client requests. Without that, a profile
    // issuing back-to-back points would starve the loader for a whole frame.
    input  logic                       ld_valid_i,
    output logic                       ld_ready_o,
    input  logic [              1:0]   ld_kind_i,   // 0 instr, 1 table, 2 header
    input  logic [SLOTW-1:0]           ld_slot_i,
    input  logic [LDADDRW-1:0]         ld_addr_i,
    input  logic [             95:0]   ld_data_i,

    // -----------------------------------------------------------------------
    // FIELD.PROGCACHE -- the residency directory, exported whole
    // -----------------------------------------------------------------------
    // Both phases face outward because the decode that a miss requires is
    // software's, exactly as that block's contract says. What is INTERNAL, and
    // is the reason the directory lives in here rather than at the console
    // edge, is the insert: it invalidates this store's slot, so a profile can
    // never run a program whose microcode has been displaced.
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
    // Request data is per client because two clients may offer at once. The
    // RESPONSE is one shared bus with a one-hot valid, because exactly one run
    // is in flight and replicating the result per client would be paying for a
    // concurrency the sequencer does not have.
    input  logic [CLIENTS-1:0]                 req_valid_i,
    output logic [CLIENTS-1:0]                 req_ready_o,
    input  logic [CLIENTS*SLOTW-1:0]           req_slot_i,
    // 1 = the caller already knows this slot holds no program for it; answer
    // ST_NO_PROGRAM without consulting the store. The Earth adapter raises it
    // for a rectangle whose program hash missed the directory.
    input  logic [CLIENTS-1:0]                 req_noprog_i,
    input  logic [CLIENTS*IN_LANES*32-1:0]     req_in_i,

    output logic [CLIENTS-1:0]                 resp_valid_o,
    input  logic [CLIENTS-1:0]                 resp_ready_i,
    output logic [OUT_LANES*32-1:0]            resp_out_o,
    output logic [7:0]                         resp_status_o,

    // -----------------------------------------------------------------------
    // counters and traces
    // -----------------------------------------------------------------------
    output logic [31:0] runs_o,             // walks that reached OP_END
    output logic [31:0] run_faults_o,       // walks that stopped on a status
    output logic [31:0] noprog_o,           // refused: slot holds no program
    output logic [31:0] instr_retired_o,    // instructions, all runs
    output logic [31:0] loads_o,            // accepted load words
    output logic [31:0] load_defers_o,      // grants deferred to the loader
    output logic [31:0] grants_o,           // client grants, all clients
    output logic [31:0] contended_grants_o, // grants taken with >1 offering
    // A header whose instruction count exceeds this store's INSTR_N is
    // CLAMPED at load and counted here. Without the clamp the sequencer's
    // 8-bit `pc_o` would walk past the slot's window and fetch the NEXT
    // slot's microcode -- a different program, executing silently, producing a
    // perfectly well-formed field. Clamping turns that into a bounded run that
    // ends on the sequencer's own ST_PC_OVERRUN.
    output logic [31:0] hdr_clamped_o,
    // A running program named a constant table this store does not hold
    // (`tbl_sel` above TABLES). Such a program is decoder-rejected upstream, so
    // this counter reads zero on lawful input; it exists because the read it
    // guards is silently wrapped rather than refused, and a wrapped table read
    // is a wrong curve with no other symptom.
    output logic [31:0] tbl_oob_o,
    // The sequencer addressed an instruction outside the slot's window. The
    // clamp above makes this unreachable through the load port, which is why it
    // is a counter and not an assumption.
    output logic [31:0] pc_oob_o,
    output logic        sat_add_o,
    output logic        sat_mul_o,
    output logic        sat_rescale_o,
    output logic        sat_rcp_o,
    output logic        rcp0_o
);

  localparam int unsigned CIDW  = (CLIENTS   > 1) ? $clog2(CLIENTS)   : 1;
  localparam int unsigned LANEW = (IN_LANES  > 1) ? $clog2(IN_LANES)  : 1;
  localparam int unsigned OUTW  = (OUT_LANES > 1) ? $clog2(OUT_LANES) : 1;

  localparam logic [1:0] LdInstr  = 2'd0;
  localparam logic [1:0] LdTable  = 2'd1;
  localparam logic [1:0] LdHeader = 2'd2;

  // A status this block adds ON TOP of the sequencer's own. It starts at 0xF0
  // so it can never collide with a `zhao_field_seq` status, whose values are
  // small and are the ratified ones.
  localparam logic [7:0] StNoProgram = 8'hF0;

  // Quartus 17.0 rejects a module-scope `if`; the elaboration guards live in an
  // `initial begin ... end` per the house rule. `--lint-only` does not run
  // these, so a clean lint says nothing about them -- they are fired by
  // tests/field/field_engine_directed.cpp's parameterisation cases.
  initial begin
    if (IN_LANES > 64) begin
      $fatal(1, "zhao_field_engine: IN_LANES=%0d exceeds the 64-entry file", IN_LANES);
    end
    if (OUT_LANES > 64) begin
      $fatal(1, "zhao_field_engine: OUT_LANES=%0d exceeds the 64-entry file", OUT_LANES);
    end
    if (INSTR_N > 256) begin
      $fatal(1, "zhao_field_engine: INSTR_N=%0d exceeds instr_count_i's 8 bits", INSTR_N);
    end
    if (TBL_N > 64) begin
      $fatal(1, "zhao_field_engine: TBL_N=%0d exceeds tbl_idx_o's 6 bits", TBL_N);
    end
    if (CLIENTS < 1) begin
      $fatal(1, "zhao_field_engine: CLIENTS must be at least 1");
    end
    if (SLOTW != ((PROGS > 1) ? $clog2(PROGS) : 1)) begin
      $fatal(1, "zhao_field_engine: SLOTW is derived and was overridden");
    end
    if (IADDRW != (SLOTW + PCW)) begin
      $fatal(1, "zhao_field_engine: IADDRW is derived and was overridden");
    end
    if (TADDRW != (SLOTW + TSELW + TIDXW)) begin
      $fatal(1, "zhao_field_engine: TADDRW is derived and was overridden");
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
  // THE PROGRAM HEADER, one row per slot
  // ==========================================================================
  // `hdr_loaded` is the interlock between the directory and the store, and it
  // is the reason both live in this file. An INSERTION marks the slot unloaded
  // -- the directory has promised the slot to a new hash, and its microcode has
  // not arrived yet, so anything that ran from it now would run the DISPLACED
  // program under the new program's name. That is the stale-prepared-values
  // failure class the PROGCACHE contract names, and it is exactly the failure a
  // directory without this edge would have had.
  logic [PROGS-1:0]  hdr_loaded;
  logic [7:0]        hdr_count   [0:PROGS-1];
  logic [5:0]        hdr_outbase [0:PROGS-1];
  logic [6:0]        hdr_tbl_n   [0:(PROGS*TABLES)-1];

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
    // Round robin: walk from rr_ptr upward, take the first offering client.
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
  // THE RUN STATE MACHINE
  // ==========================================================================
  localparam logic [2:0] E_IDLE  = 3'd0;
  localparam logic [2:0] E_CLEAR = 3'd1;
  localparam logic [2:0] E_WRITE = 3'd2;
  localparam logic [2:0] E_START = 3'd3;
  localparam logic [2:0] E_RUN   = 3'd4;
  localparam logic [2:0] E_READ  = 3'd5;
  localparam logic [2:0] E_RESP  = 3'd6;

  logic [2:0]         state;
  logic [CIDW-1:0]    cur_id;
  logic [SLOTW-1:0]   cur_slot;
  logic [LANEW:0]     lane_i;
  logic [OUTW+1:0]    out_i;
  logic signed [31:0] cur_in  [0:IN_LANES-1];
  logic signed [31:0] cur_out [0:OUT_LANES-1];
  logic [7:0]         cur_status;
  logic               cur_noprog;

  // ---- the sequencer's ports ----------------------------------------------
  logic               seq_clear;
  logic               seq_start;
  logic               seq_rf_we;
  logic [5:0]         seq_rf_waddr;
  logic signed [31:0] seq_rf_wdata;
  logic [5:0]         seq_rf_raddr;
  logic signed [31:0] seq_rf_rdata;
  logic               seq_busy;
  logic               seq_done;
  logic [7:0]         seq_status;
  logic [7:0]         seq_pc;
  logic [31:0]        seq_tbl_sel;
  logic [5:0]         seq_tbl_idx;
  logic               seq_retired;

  // ---- the two guards that read the bits nothing else does ----------------
  // `seq_pc` is eight bits because `instr_count_i` is; this store's window is
  // PCW. `seq_tbl_sel` is the whole 32-bit immediate; this store holds TABLES.
  // Both surplus fields are READ HERE rather than waived, because a wrapped
  // fetch and a wrapped table read are both silent, both well-formed and both
  // produce a plausible field -- exactly the class this composition exists to
  // refuse.
  wire pc_oob_c  = seq_busy && (seq_pc >= 8'(INSTR_N));
  wire tbl_oob_c = seq_busy && (|seq_tbl_sel[31:TSELW]);

  // ---- the stores ----------------------------------------------------------
  logic [63:0] instr_word;
  logic [95:0] table_word;

  wire [IADDRW-1:0] instr_rd_addr = {cur_slot, seq_pc[PCW-1:0]};
  wire [IADDRW-1:0] instr_wr_addr = {ld_slot_i, ld_addr_i[PCW-1:0]};

  // The table select is the instruction's immediate, held for the whole
  // instruction by the sequencer, so the low bits of it name the table. A
  // program that names a table this store does not have reads the table at
  // (imm mod TABLES); such a program is DECODER-rejected by
  // spec/form/field-ir.md 5, and re-checking it here would be a second
  // implementation of the validation law.
  wire [TADDRW-1:0] table_rd_addr =
      {cur_slot, seq_tbl_sel[TSELW-1:0], seq_tbl_idx[TIDXW-1:0]};
  wire [TADDRW-1:0] table_wr_addr = {ld_slot_i, ld_addr_i[(TSELW+TIDXW)-1:0]};

  wire ld_fire = ld_valid_i && ld_ready_o;

  zhao_dc_sdp_ram #(
      .DATA_W(64),
      .ADDR_W(IADDRW)
  ) u_instr_ram (
      .wr_clk (clk),
      .wr_en  (ld_fire && (ld_kind_i == LdInstr)),
      .wr_addr(instr_wr_addr),
      .wr_data(ld_data_i[63:0]),
      .rd_clk (clk),
      .rd_en  (1'b1),
      .rd_addr(instr_rd_addr),
      .rd_data(instr_word)
  );

  zhao_dc_sdp_ram #(
      .DATA_W(96),
      .ADDR_W(TADDRW)
  ) u_table_ram (
      .wr_clk (clk),
      .wr_en  (ld_fire && (ld_kind_i == LdTable)),
      .wr_addr(table_wr_addr),
      .wr_data(ld_data_i),
      .rd_clk (clk),
      .rd_en  (1'b1),
      .rd_addr(table_rd_addr),
      .rd_data(table_word)
  );

  // The instruction word's field order is this block's choice and it is
  // declared here rather than left to be inferred from the loader: the sixty-
  // four bits of `zhao_field_seq`'s five instruction ports pack exactly, with
  // no padding.
  //   [ 7: 0] op   [13: 8] dst   [19:14] a   [25:20] b   [31:26] c
  //   [63:32] imm
  wire [7:0]  ins_op  = instr_word[7:0];
  wire [5:0]  ins_dst = instr_word[13:8];
  wire [5:0]  ins_a   = instr_word[19:14];
  wire [5:0]  ins_b   = instr_word[25:20];
  wire [5:0]  ins_c   = instr_word[31:26];
  wire [31:0] ins_imm = instr_word[63:32];

  wire signed [31:0] tbl_x  = table_word[31:0];
  wire signed [31:0] tbl_y  = table_word[63:32];
  wire signed [31:0] tbl_dy = table_word[95:64];

  wire [6:0] tbl_n_sel =
      hdr_tbl_n[(int'(cur_slot) * int'(TABLES)) + int'(seq_tbl_sel[TSELW-1:0])];

  zhao_field_seq u_seq (
      .clk  (clk),
      .rst_n(rst_n),

      .rf_we_i   (seq_rf_we),
      .rf_waddr_i(seq_rf_waddr),
      .rf_wdata_i(seq_rf_wdata),
      .rf_raddr_i(seq_rf_raddr),
      .rf_rdata_o(seq_rf_rdata),

      .clear_i (seq_clear),
      .start_i (seq_start),
      .busy_o  (seq_busy),
      .done_o  (seq_done),
      .status_o(seq_status),

      .instr_count_i(hdr_count[cur_slot]),
      .pc_o         (seq_pc),
      .ins_op_i     (ins_op),
      .ins_dst_i    (ins_dst),
      .ins_a_i      (ins_a),
      .ins_b_i      (ins_b),
      .ins_c_i      (ins_c),
      .ins_imm_i    (ins_imm),

      .tbl_sel_o(seq_tbl_sel),
      .tbl_idx_o(seq_tbl_idx),
      .tbl_n_i  (tbl_n_sel),
      .tbl_x_i  (tbl_x),
      .tbl_y_i  (tbl_y),
      .tbl_dy_i (tbl_dy),

      .sat_add_o      (sat_add_o),
      .sat_mul_o      (sat_mul_o),
      .sat_rescale_o  (sat_rescale_o),
      .sat_rcp_o      (sat_rcp_o),
      .rcp0_o         (rcp0_o),
      .instr_retired_o(seq_retired)
  );

  // ---- handshakes ----------------------------------------------------------
  assign ld_ready_o = (state == E_IDLE);

  always_comb begin
    req_ready_o = '0;
    if ((state == E_IDLE) && !ld_valid_i && pick_any) begin
      req_ready_o[pick_id] = 1'b1;
    end
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
  assign resp_status_o = cur_status;

  // ---- the register-file port ---------------------------------------------
  // Writes happen only in E_WRITE, which is entered after E_CLEAR and left
  // before E_START, so `zhao_field_seq`'s own `host_wen = !clear_i && !busy_o`
  // guard is satisfied structurally rather than by timing luck.
  wire [LANEW-1:0] lane_sel =
      (lane_i < (LANEW+1)'(IN_LANES)) ? lane_i[LANEW-1:0] : '0;
  // E_READ captures the answer to the address it presented one cycle earlier,
  // so the destination index is one behind the counter and is never taken at
  // `out_i == 0`.
  wire [OUTW-1:0] out_cap = OUTW'(out_i - (OUTW+2)'(1));

  always_comb begin
    seq_rf_we    = (state == E_WRITE) && (lane_i < (LANEW+1)'(IN_LANES));
    seq_rf_waddr = 6'(lane_i);
    seq_rf_wdata = cur_in[lane_sel];
    seq_rf_raddr = 6'(int'(hdr_outbase[cur_slot]) + int'(out_i));
    seq_clear    = (state == E_CLEAR);
    seq_start    = (state == E_START);
  end

  integer k;
  integer li;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state      <= E_IDLE;
      rr_ptr     <= '0;
      cur_id     <= '0;
      cur_slot   <= '0;
      lane_i     <= '0;
      out_i      <= '0;
      cur_status <= 8'd0;
      cur_noprog <= 1'b0;
      hdr_loaded <= '0;
      for (k = 0; k < int'(PROGS); k = k + 1) begin
        hdr_count[k]   <= 8'd0;
        hdr_outbase[k] <= 6'd0;
      end
      for (k = 0; k < int'(PROGS*TABLES); k = k + 1) hdr_tbl_n[k] <= 7'd0;
      for (k = 0; k < int'(IN_LANES); k = k + 1) cur_in[k] <= 32'sd0;
      for (k = 0; k < int'(OUT_LANES); k = k + 1) cur_out[k] <= 32'sd0;
      runs_o             <= 32'd0;
      run_faults_o       <= 32'd0;
      noprog_o           <= 32'd0;
      instr_retired_o    <= 32'd0;
      loads_o            <= 32'd0;
      load_defers_o      <= 32'd0;
      grants_o           <= 32'd0;
      contended_grants_o <= 32'd0;
      hdr_clamped_o      <= 32'd0;
      tbl_oob_o          <= 32'd0;
      pc_oob_o           <= 32'd0;
    end else begin
      if (seq_retired && (instr_retired_o != 32'hFFFF_FFFF)) begin
        instr_retired_o <= instr_retired_o + 32'd1;
      end
      if (pc_oob_c && (pc_oob_o != 32'hFFFF_FFFF)) pc_oob_o <= pc_oob_o + 32'd1;
      if (tbl_oob_c && (tbl_oob_o != 32'hFFFF_FFFF)) tbl_oob_o <= tbl_oob_o + 32'd1;

      // An insertion invalidates the slot's microcode. See the note on
      // `hdr_loaded`.
      if (pc_cm_valid_i && pc_cm_ready_o && pc_cm_ok_i) begin
        hdr_loaded[pc_cm_slot_c] <= 1'b0;
      end

      case (state)
        E_IDLE: begin
          if (ld_valid_i) begin
            // The loader wins. `req_ready_o` is already low for everyone this
            // cycle, so no client is accepted and then abandoned.
            if (pick_any && (load_defers_o != 32'hFFFF_FFFF)) begin
              load_defers_o <= load_defers_o + 32'd1;
            end
            if (loads_o != 32'hFFFF_FFFF) loads_o <= loads_o + 32'd1;
            if (ld_kind_i == LdHeader) begin
              if (ld_data_i[7:0] > 8'(INSTR_N)) begin
                hdr_count[ld_slot_i] <= 8'(INSTR_N);
                if (hdr_clamped_o != 32'hFFFF_FFFF) begin
                  hdr_clamped_o <= hdr_clamped_o + 32'd1;
                end
              end else begin
                hdr_count[ld_slot_i] <= ld_data_i[7:0];
              end
              hdr_outbase[ld_slot_i] <= ld_data_i[13:8];
              for (li = 0; li < int'(TABLES); li = li + 1) begin
                hdr_tbl_n[(int'(ld_slot_i) * int'(TABLES)) + li] <=
                    ld_data_i[(16 + li*8) +: 7];
              end
              // The header is written LAST by the loader and is what marks the
              // slot runnable, so a partially written program can never be
              // executed. Stated as a protocol rule here because nothing in the
              // RAMs could enforce it, and enforced from the other side: any
              // instruction or table write clears the bit again.
              hdr_loaded[ld_slot_i] <= 1'b1;
            end else begin
              hdr_loaded[ld_slot_i] <= 1'b0;
            end
          end else if (pick_any) begin
            cur_id     <= pick_id;
            cur_slot   <= req_slot_i[(int'(pick_id)*int'(SLOTW)) +: SLOTW];
            cur_noprog <= req_noprog_i[pick_id] ||
                          !hdr_loaded[req_slot_i[(int'(pick_id)*int'(SLOTW)) +: SLOTW]];
            for (li = 0; li < int'(IN_LANES); li = li + 1) begin
              cur_in[li] <= req_in_i[((int'(pick_id)*int'(IN_LANES) + li)*32) +: 32];
            end
            rr_ptr <= CIDW'((int'(pick_id) + 1) % int'(CLIENTS));
            lane_i <= '0;
            out_i  <= '0;
            if (grants_o != 32'hFFFF_FFFF) grants_o <= grants_o + 32'd1;
            if (offers_many && (contended_grants_o != 32'hFFFF_FFFF)) begin
              contended_grants_o <= contended_grants_o + 32'd1;
            end
            state <= E_CLEAR;
          end
        end

        E_CLEAR: begin
          for (li = 0; li < int'(OUT_LANES); li = li + 1) cur_out[li] <= 32'sd0;
          if (cur_noprog) begin
            // REFUSED, never faked. The caller gets a status and zeroed lanes,
            // and the counter below is what says so. A field result is exactly
            // the place a plausible constant proves nothing, so the STATUS is
            // the load-bearing part of this answer and every adapter is
            // required to read it.
            cur_status <= StNoProgram;
            if (noprog_o != 32'hFFFF_FFFF) noprog_o <= noprog_o + 32'd1;
            state      <= E_RESP;
          end else begin
            cur_status <= 8'd0;
            state      <= E_WRITE;
          end
        end

        E_WRITE: begin
          if (lane_i < (LANEW+1)'(IN_LANES)) begin
            lane_i <= lane_i + 1'b1;
          end else begin
            state <= E_START;
          end
        end

        E_START: state <= E_RUN;

        E_RUN: begin
          if (seq_done) begin
            cur_status <= seq_status;
            if (seq_status == 8'd0) begin
              if (runs_o != 32'hFFFF_FFFF) runs_o <= runs_o + 32'd1;
            end else begin
              if (run_faults_o != 32'hFFFF_FFFF) run_faults_o <= run_faults_o + 32'd1;
            end
            out_i <= '0;
            state <= E_READ;
          end
        end

        // The file's read is SYNCHRONOUS (FIELD.SEQ.CORE.md, 2026-08-24): the
        // address is presented on one edge and answers on the next. So this
        // state presents `out_i` and captures the answer to `out_i - 1`, and it
        // runs one step past OUT_LANES to collect the last one. Missing this is
        // the documented symptom "every chained result reads back as the
        // PREVIOUS instruction's answer", one level up.
        E_READ: begin
          if (out_i > 0) cur_out[out_cap] <= seq_rf_rdata;
          if (out_i == (OUTW+2)'(OUT_LANES)) begin
            state <= E_RESP;
          end else begin
            out_i <= out_i + 1'b1;
          end
        end

        E_RESP: begin
          if (resp_ready_i[cur_id]) state <= E_IDLE;
        end

        default: state <= E_IDLE;
      endcase
    end
  end

endmodule : zhao_field_engine

`default_nettype wire
