// zhao_field_loader.sv -- THE FH2 TRANSACTIONAL PROGRAM LOADER.
//
// Owner directive reports/Zhaozhou_SHARED_FIELD_Repair_Architecture_2026-09-20.txt,
// decisions FH13 / FH15 / FH16 and the install FSM of section 10.3.
//
//   FH13 -- AUTHORITATIVE RESERVATION BEFORE PUBLICATION.
//   > New loading uses reserve -> fill -> validate -> seal -> bind. The
//   > allocator chooses the slot before writes. A binding becomes executable
//   > only after its own bytes, maps and metadata are complete. The existing
//   > low-level commit/re-header behavior remains an explicitly versioned
//   > legacy protocol, not an accidental implementation of this new
//   > transaction.
//
//   FH15 -- BACKING STORAGE AND ACTIVE CACHES ARE DIFFERENT.
//   > Keep immutable canonical/physical program images and prepared
//   > associations in bounded, pinned HPS staging, reached through the real
//   > bridge. On-chip storage holds descriptors, the working code, active
//   > tables/scalars, contexts and credited queues.
//
//   FH16 -- NO PER-POINT DIRECTORY LOOKUP.
//   > Acquire and pin at association open. A live execution refers to a
//   > checked association generation; raw cache-slot numbers are not a
//   > game-facing binding.
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK IS, AGAINST WHAT ALREADY EXISTED
// ---------------------------------------------------------------------------
// `zhao_field_progdir.sv:22` ALREADY STATES FH16's POLICY, and this block does
// not re-derive it:
//
//   > "acquire ONCE per program/association and hold the slot; never per
//   >  point"
//
// The policy was present; the BINDING OBJECT was not, and that is the whole of
// the gap plan section 6 F5 names ("the POLICY is present; the BINDING OBJECT
// (P8) is not"). So this file supplies the object -- a catalogue of installed
// images with a GENERATION, a PIN COUNT and a READY bit -- and leaves the
// directory's residency law exactly where it is. Nothing here decides eviction
// order for `zhao_field_progcache`; this catalogue is the BACKING STORE's
// descriptor table, which is FH15's distinction, and the two are deliberately
// different objects with different lifetimes.
//
// ---------------------------------------------------------------------------
// WHY THE BYTES COME THROUGH THE BRIDGE AND NOT THROUGH A TEST PORT
// ---------------------------------------------------------------------------
// Directive section 20.5: "Exercise it with the production packer's bytes
// through the real bridge/arbiter shape. ... A testbench secretly writing the
// engine's private uop/table/scalar ports is not that path."
//
// So the only way bytes enter an object here is `hps_req_o`/`hps_rsp_i`, the
// same `zhao_hps_burst_req_t` client `zhao_terrain_pageloader` uses. The
// harness plays the bridge; it cannot reach around it.
//
// ---------------------------------------------------------------------------
// THE ONE ORDERING LAW, AND THE REASON IT IS THE WHOLE POINT
// ---------------------------------------------------------------------------
// Directive section 10.3: "CRC checked only after bytes have already made a
// slot executable is too late."
//
// RESERVE_OBJECT selects a slot and marks it STAGING. Nothing may execute from
// a STAGING slot: `obj_ready` is clear, and `obj_ready` is the only bit the
// publication port exports. SEAL is the single place `obj_ready` is set, and it
// is guarded by the conjunction of every check. A failure path clears the
// staging object and leaves every previously READY object untouched -- which is
// the directive's "retain prior ready objects unless explicitly replaced under
// the transaction".
//
// ---------------------------------------------------------------------------
// VERDICTS ARE NAMED, AND EVERY ONE OF THEM IS REACHABLE
// ---------------------------------------------------------------------------
// A refusal that cannot be told apart from a different refusal is a refusal
// nobody can act on. Each verdict below has a directed case in
// tests/field/field_loader_directed.cpp that reaches it with legal stimulus --
// none of these needs a mutant, because a malformed capsule IS legal stimulus
// for a loader. The counters are per-CLASS rather than one `refusals_o`,
// because "the install failed" is not a diagnosis.
//
// Conservative SystemVerilog subset (Quartus 17.0): no module-scope `if`, no
// implicit generate, elaboration guards inside `initial begin ... end`.
//
// ENFORCED-BY: tests/field/field_loader_directed.cpp:main
`default_nettype none

module zhao_field_loader
  // ONE import statement, not two. Quartus 17.0 takes a single item list in a
  // module header and ABORTS on a second `import` -- `quartus_map` died right
  // here, on the line below this one, and because run_block_map.ps1 compiles
  // every .sv under fpga/rtl that failure killed EVERY map in the tree, for
  // any block, until this was merged (owner ruling R212). Verilator parses the
  // rejected form without a murmur, so lint said nothing for as long as it
  // stood: "lint-clean is not Quartus-synthesizable", fourth exhibit.
  import zhao_pkg::*, zhao_field_host_image_pkg::*;
#(
    // The backing-store descriptor catalogue. FH15: this is NOT the active
    // program cache -- `zhao_field_progcache` owns residency, this owns the
    // installed immutable image objects and their lifetimes.
    parameter int unsigned OBJECTS = 8,
    parameter int unsigned OBJW    = 3,   // $clog2(OBJECTS) at OBJECTS = 8
    // Generation width. Directive FH16: "a live execution refers to a checked
    // association generation". A generation that wraps inside one session would
    // let two live objects alias, which is FT069's subject; 8 bits with the
    // quiescent-epoch rule is the directive's own minimum.
    parameter int unsigned GENW    = 8,
    // One bridge burst. The bridge's `len` field is 1..64 bytes.
    parameter int unsigned BURST_BYTES = 64,
    // The largest capsule this loader will accept. Bounded on purpose: FH15
    // says "bounded, pinned HPS staging", and an unbounded read is how a
    // malformed length becomes a denial of service.
    parameter int unsigned MAX_CAPSULE_BYTES = 65536,
    // Section directory bound. A capsule naming more sections than this is
    // refused rather than partially parsed.
    parameter int unsigned MAX_SECTIONS = 24,
    // Default ON, and a PARAMETER rather than a hard-coded law for the reason
    // `zhao_terrain_pageloader`'s CHECK_HEADER_IDENT gives: a redundancy that
    // cannot be switched off cannot be measured. With it on, a capsule stamped
    // for a different resource epoch is refused at install rather than
    // becoming a READY object that FH16's "epoch changes cannot silently
    // retarget an association" then has to defend against downstream.
    parameter bit CHECK_EPOCH = 1'b1
) (
    input var logic clk,
    input var logic rst_n,

    // ---- configuration ------------------------------------------------------
    // Read LIVE, not captured: these are the machine's state, not the request's
    // payload. The REQUEST's fields are captured at acceptance, below -- the
    // distinction `zhao_terrain_pageloader.sv:143` draws in the same words, and
    // the one the old doorbell got wrong (see that block's FT061).
    input var zhao_client_e cfg_hps_client_i,
    input var logic [31:0]  cfg_epoch_i,          // the live resource_epoch
    input var logic [31:0]  cfg_stage_base_i,     // allowed staging region base
    input var logic [31:0]  cfg_stage_bytes_i,    // allowed staging region extent

    // ---- FH2 request, from zhao_field_doorbell's op-3 arm -------------------
    input  var logic         fh2_valid_i,
    output var logic         fh2_ready_o,
    input  var logic [1:0]   fh2_kind_i,    // 0 INSTALL_CAPSULE, 1 BIND_PROGRAM, 2 CONTROL, 3 reserved
    input  var logic [7:0]   fh2_verb_i,    // CONTROL verb; directive 10.2 selects an 8-bit field
    input  var logic [95:0]  fh2_data_i,
    input  var logic [31:0]  fh2_hash_i,
    input  var logic [31:0]  fh2_ticket_i,
    input  var logic [31:0]  fh2_plan_i,    // captured by the doorbell AT ACCEPTANCE

    // ---- FH2 reply ----------------------------------------------------------
    // Exactly one terminal reply per accepted command (FT070). The doorbell
    // reserved the return record before handing the command over, so this
    // response can never be refused for want of somewhere to put it.
    output var logic         fh2_resp_valid_o,
    input  var logic         fh2_resp_ready_i,
    output var logic         fh2_resp_ok_o,
    output var logic [3:0]   fh2_resp_verdict_o,
    output var logic [31:0]  fh2_resp_ticket_o,
    output var logic [31:0]  fh2_resp_plan_o,
    // The binding handle: {object index, generation}. NOT a raw cache slot --
    // FH16: "raw cache-slot numbers are not a game-facing binding."
    output var logic [31:0]  fh2_resp_handle_o,
    output var logic [OBJW-1:0] fh2_resp_slot_o,
    output var logic         fh2_resp_evicted_o,

    // ---- MEM.HPS.BRIDGE client ---------------------------------------------
    output var zhao_hps_burst_req_t hps_req_o,
    input  var logic                hps_req_grant_i,
    input  var zhao_hps_burst_rsp_t hps_rsp_i,

    // ---- publication: what is READY, and what is PINNED (FH16) --------------
    // The ONLY export of executability. A staging object is absent from
    // `pub_ready_o` by construction, so a consumer cannot execute a half-filled
    // image even by guessing its index.
    output var logic [OBJECTS-1:0]  pub_ready_o,
    output var logic [OBJECTS-1:0]  pub_pinned_o,
    // An INDEXED read rather than an unpacked array port. Quartus 17.0 is
    // happier with it, every bench that instantiates this block avoids an
    // array-port connection, and it costs one mux the descriptor table needs
    // anyway.
    input  var logic [OBJW-1:0]     pub_sel_i,
    output var logic [31:0]         pub_handle_o,
    output var logic [31:0]         pub_prog_hash_o,
    output var logic [GENW-1:0]     pub_gen_o,

    // ---- evidence -----------------------------------------------------------
    output var logic [31:0] installs_ok_o,
    output var logic [31:0] installs_failed_o,
    output var logic [31:0] binds_ok_o,
    output var logic [31:0] binds_failed_o,
    output var logic [31:0] controls_ok_o,
    output var logic [31:0] bad_operation_o,    // unknown kind / unknown verb
    output var logic [31:0] bad_envelope_o,     // magic/version/extent/alignment
    output var logic [31:0] bad_range_o,        // unreachable or out-of-region address
    output var logic [31:0] bad_section_o,      // count/offset/stride/overlap/order
    output var logic [31:0] bad_crc_o,
    output var logic [31:0] bad_meta_o,         // required_mask == 0, counts out of range
    output var logic [31:0] bridge_errs_o,      // err beat, early last, missing/extra beat
    output var logic [31:0] no_capacity_o,      // catalogue full of PINNED objects
    output var logic [31:0] evictions_o,
    // FT057. Evictions whose victim was NOT the globally least-recently-used
    // object -- which can only happen because a PIN excluded the LRU. It is
    // deliberately NOT "the allocator disagreed with a software hint": FH2's
    // INSTALL carries no slot (directive 10.2 makes those operands reserved
    // zero), so there is no hint to disagree with, and a counter named for one
    // would be measuring something other than its own name.
    output var logic [31:0] pin_forced_victim_o,
    output var logic [31:0] load_bytes_o
);

  // ==========================================================================
  // VERDICTS
  // ==========================================================================
  localparam logic [3:0] V_OK            = 4'd0;
  localparam logic [3:0] V_BAD_ENVELOPE  = 4'd1;
  localparam logic [3:0] V_BAD_RANGE     = 4'd2;
  localparam logic [3:0] V_BAD_SECTION   = 4'd3;
  localparam logic [3:0] V_BAD_CRC       = 4'd4;
  localparam logic [3:0] V_BAD_META      = 4'd5;
  localparam logic [3:0] V_BRIDGE_ERR    = 4'd6;
  localparam logic [3:0] V_NO_CAPACITY   = 4'd7;
  localparam logic [3:0] V_BAD_OPERATION = 4'd8;
  localparam logic [3:0] V_BAD_BINDING   = 4'd9;

  localparam logic [1:0] K_INSTALL = 2'd0;
  localparam logic [1:0] K_BIND    = 2'd1;
  localparam logic [1:0] K_CONTROL = 2'd2;
  // Kind 3 under op 3 is RESERVED and answered BAD_OPERATION -- directive 10.2.

  // CONTROL verbs, directive 10.2.
  localparam logic [7:0] VERB_OPEN_ASSOCIATION   = 8'd1;
  localparam logic [7:0] VERB_CLOSE_ASSOCIATION  = 8'd2;
  localparam logic [7:0] VERB_CANCEL_ASSOCIATION = 8'd3;
  localparam logic [7:0] VERB_RELEASE_BINDING    = 8'd4;
  localparam logic [7:0] VERB_QUERY_OBJECT       = 8'd5;
  localparam logic [7:0] VERB_EPOCH_BARRIER      = 8'd6;

  localparam int unsigned BEATS_PER_BURST = BURST_BYTES / 8;

  initial begin
    if ((1 << OBJW) < int'(OBJECTS)) begin
      $fatal(1, "zhao_field_loader: OBJW=%0d cannot address OBJECTS=%0d", OBJW, OBJECTS);
    end
    if (OBJECTS < 2) $fatal(1, "zhao_field_loader: OBJECTS must be at least 2");
    if (GENW < 4) $fatal(1, "zhao_field_loader: GENW must be at least 4");
    if ((BURST_BYTES % 8) != 0) begin
      $fatal(1, "zhao_field_loader: BURST_BYTES=%0d must be a multiple of 8", BURST_BYTES);
    end
    if (BURST_BYTES > 64) $fatal(1, "zhao_field_loader: BURST_BYTES exceeds the bridge's 64");
    if ((MAX_CAPSULE_BYTES % ZFH_ALIGNMENT) != 0) begin
      $fatal(1, "zhao_field_loader: MAX_CAPSULE_BYTES must be %0d-byte aligned", ZFH_ALIGNMENT);
    end
  end

  // ==========================================================================
  // THE OBJECT CATALOGUE -- descriptors only (FH15)
  // ==========================================================================
  // `obj_ready` is the executability bit and SEAL is its only writer.
  // `obj_staging` marks the slot this transaction reserved; it is never
  // exported, so nothing downstream can see a half-built object at all.
  logic [OBJECTS-1:0]  obj_valid;
  logic [OBJECTS-1:0]  obj_ready;
  logic [OBJECTS-1:0]  obj_staging;
  logic [GENW-1:0]     obj_gen       [0:OBJECTS-1];
  logic [15:0]         obj_pins      [0:OBJECTS-1];
  logic [31:0]         obj_prog_hash [0:OBJECTS-1];
  logic [31:0]         obj_handle32  [0:OBJECTS-1];
  logic [31:0]         obj_epoch     [0:OBJECTS-1];
  logic [31:0]         obj_full_crc  [0:OBJECTS-1];
  logic [31:0]         obj_lru       [0:OBJECTS-1];
  logic [31:0]         lru_ctr;

  assign pub_handle_o    = obj_handle32[pub_sel_i];
  assign pub_prog_hash_o = obj_prog_hash[pub_sel_i];
  assign pub_gen_o       = obj_gen[pub_sel_i];

  logic [OBJECTS-1:0] pinned_mask;

  // A STAGING object is NOT ready, by construction and not by convention.
  assign pub_ready_o  = obj_ready & ~obj_staging;
  assign pub_pinned_o = pinned_mask;

  integer pi;
  always_comb begin
    pinned_mask = '0;
    for (pi = 0; pi < int'(OBJECTS); pi = pi + 1) begin
      pinned_mask[pi] = obj_valid[pi] && (obj_pins[pi] != 16'd0);
    end
  end

  // ---- THE ALLOCATOR -------------------------------------------------------
  // Free first, else the least-recently-used UNPINNED object. A catalogue whose
  // every valid object is pinned has NO victim, and that is NO_CAPACITY rather
  // than a silent overwrite of live backing bytes -- FH16's "program eviction
  // or epoch changes cannot silently retarget an association".
  //
  // The software hint plays no part in this. FT057 exists because the legacy
  // protocol let software guess the slot before the bytes were written; here
  // the allocator chooses and the reply reports what it chose.
  logic            free_any;
  logic [OBJW-1:0] free_idx;
  logic            victim_any;
  logic [OBJW-1:0] victim_idx;
  logic [31:0]     best_lru;
  // THE SAME SEARCH, BLIND TO PINS. It selects nothing and drives nothing; its
  // only job is to be DIFFERENCED against the real victim so the block can say
  // when a PIN changed the answer. Differencing the allocator against itself
  // would be the wired-to-two-operands-that-move-together defect, so this one
  // deliberately does not consult `obj_pins`.
  logic            lru_blind_any;
  logic [OBJW-1:0] lru_blind_idx;
  logic [31:0]     best_lru_blind;
  integer          ai;
  always_comb begin
    free_any   = 1'b0;
    free_idx   = '0;
    victim_any = 1'b0;
    victim_idx = '0;
    best_lru   = 32'hFFFF_FFFF;
    lru_blind_any  = 1'b0;
    lru_blind_idx  = '0;
    best_lru_blind = 32'hFFFF_FFFF;
    // Descending so the LOWEST qualifying index wins, matching the directory's
    // own loop order in zhao_field_progcache.sv:129.
    for (ai = int'(OBJECTS) - 1; ai >= 0; ai = ai - 1) begin
      if (!obj_valid[ai]) begin
        free_any = 1'b1;
        free_idx = OBJW'(ai);
      end
      if (obj_valid[ai] && (obj_pins[ai] == 16'd0) && (obj_lru[ai] <= best_lru)) begin
        best_lru   = obj_lru[ai];
        victim_any = 1'b1;
        victim_idx = OBJW'(ai);
      end
      if (obj_valid[ai] && (obj_lru[ai] <= best_lru_blind)) begin
        best_lru_blind = obj_lru[ai];
        lru_blind_any  = 1'b1;
        lru_blind_idx  = OBJW'(ai);
      end
    end
  end

  // THE BINDING HANDLE'S RESERVED BITS ARE CHECKED, NOT IGNORED.
  // The loader issues {8'd0, generation[7:0], 8'd0, 0.., index}, so bits
  // [31:24] and [15:OBJW] are zero by construction. `-Wall` reported them
  // unused once the allocator stopped (wrongly) masking the whole word, and
  // the right answer to "these bits are unused" on an identity field is to
  // VALIDATE them: a handle carrying rubbish in a reserved span is not a
  // handle this loader issued, and accepting it would let a caller reach a
  // real object with a value the loader never produced.
  wire bind_handle_ok = (rq_bind_handle[31:24] == 8'd0) &&
                        (rq_bind_handle[15:OBJW] == '0);
  wire ctrl_handle_ok = (rq_bind_prog[31:24] == 8'd0) &&
                        (rq_bind_prog[15:OBJW] == '0);

  wire            alloc_any = free_any || victim_any;
  wire [OBJW-1:0] alloc_idx = free_any ? free_idx : victim_idx;
  wire            alloc_evicts = !free_any && victim_any;

  // ==========================================================================
  // THE CAPTURED REQUEST -- every operand taken at ACCEPTANCE
  // ==========================================================================
  logic [1:0]  rq_kind;
  logic [7:0]  rq_verb;
  logic [63:0] rq_addr;        // 64-bit until the reachable-range check
  logic [31:0] rq_len;
  logic [31:0] rq_hash;
  logic [31:0] rq_ticket;
  logic [31:0] rq_plan;
  logic [31:0] rq_bind_handle;
  logic [31:0] rq_bind_prog;
  logic [31:0] rq_bind_epoch;

  // ==========================================================================
  // THE STAGING TRANSACTION
  // ==========================================================================
  logic [OBJW-1:0] st_idx;
  logic            st_evicted;
  logic [31:0]     st_total;       // total_bytes from the header
  logic [31:0]     st_got;         // bytes actually received -- exact accounting
  logic [31:0]     st_crc;         // running CRC32C
  logic [15:0]     st_sections;
  logic [31:0]     st_prog_hash;
  logic [31:0]     st_handle32;
  logic [31:0]     st_epoch;
  logic [31:0]     st_full_crc;
  logic [3:0]      st_verdict;
  logic            st_fail;

  // Burst bookkeeping. `beat_n` counts beats WITHIN the burst so an early
  // `last`, a missing beat and an extra beat are three distinguishable faults
  // rather than one "the burst went wrong" (FT052).
  logic [7:0]  beat_n;
  logic [31:0] burst_off;
  logic [6:0]  burst_len;

  // The 64-byte header, assembled beat by beat so the checks can read named
  // fields rather than indexing a blob at call sites.
  logic [7:0] hdr_b [0:ZFH_HDR_BYTES-1];

  typedef enum logic [3:0] {
    L_IDLE,
    L_ACCEPT,
    L_ENVELOPE,
    L_HDR_REQ,
    L_HDR_RCV,
    L_HDR_CHECK,
    L_RESERVE,
    L_BODY_REQ,
    L_BODY_RCV,
    L_CHECK,
    L_SEAL,
    L_RETURN,
    L_FAIL
  } lstate_e;
  lstate_e lstate;

  assign fh2_ready_o = (lstate == L_IDLE);

  // ---- the bridge request --------------------------------------------------
  // THE RANGE CHECK HAPPENS WIDE, AND THE NARROWING HAPPENS AFTER IT.
  // `L_ENVELOPE` has already proved `rq_addr[63:32] == 0` and that
  // `rq_addr + rq_len` lies inside the staging region, both evaluated in 64
  // bits so a sum that wraps 32 cannot present as a small in-range extent. The
  // narrowing below therefore discards only bits that were verified zero.
  // Doing it in this order is precisely what makes an out-of-range capsule a
  // REFUSAL rather than a truncation -- FT051's subject.
  wire [31:0] cur_addr_c = rq_addr[31:0] + burst_off;

  always_comb begin
    hps_req_o        = '0;
    hps_req_o.valid  = (lstate == L_HDR_REQ) || (lstate == L_BODY_REQ);
    hps_req_o.write  = 1'b0;
    hps_req_o.client = cfg_hps_client_i;
    hps_req_o.addr   = cur_addr_c;
    hps_req_o.len    = burst_len;
  end

  // ==========================================================================
  // CRC32C -- THE SHARED FOLD, NOT A SECOND IMPLEMENTATION
  // ==========================================================================
  // `zhao_crc32c_fold` already exists and is the ratified arithmetic. Writing
  // a bit-serial CRC here would be exactly the failure CLAUDE.md's sibling-
  // contract chapter names -- and it would also be the timing defect that
  // module was built to remove: its header measures the serial form at 64
  // chained XOR levels (~38 ns) against a 10 ns budget. Eight bytes per beat
  // is precisely its `n_i = 8` case.
  //
  // Directive 10.2: the expected value uses "the SAME zeroed-field rule as
  // header.body_crc32c (bytes 12..15 treated as zero)". Those four bytes live
  // in the beat whose base offset is 8, as its HIGH word -- so the zeroing is
  // one mux on the feed, stated once, here.
  // Derived from the CRC field's own generated offset rather than written as
  // 8, so a schema change moves the mask with it instead of silently leaving
  // it pointing at the wrong beat.
  localparam int unsigned CRC_BEAT_BASE = (ZFH_HDR_OFF_BODY_CRC32C / 8) * 8;
  localparam int unsigned CRC_BEAT_LO   = ZFH_HDR_OFF_BODY_CRC32C - CRC_BEAT_BASE;

  wire [31:0] beat_base_c = burst_off + (32'(beat_n) * 32'd8);
  wire [63:0] crc_feed_c  = (beat_base_c == 32'(CRC_BEAT_BASE))
                              ? (hps_rsp_i.data &
                                 ~(64'hFFFF_FFFF << (8 * CRC_BEAT_LO)))
                              : hps_rsp_i.data;
  wire [31:0] crc_next_c;

  zhao_crc32c_fold u_crc (
      .c_i(st_crc),
      .d_i(crc_feed_c),
      .n_i(4'd8),
      .c_o(crc_next_c)
  );

  // ==========================================================================
  // HEADER FIELD READERS -- little-endian, by generated offset
  // ==========================================================================
  function automatic logic [31:0] hdr_u32(input int unsigned off);
    begin
      hdr_u32 = {hdr_b[off+3], hdr_b[off+2], hdr_b[off+1], hdr_b[off]};
    end
  endfunction

  function automatic logic [15:0] hdr_u16(input int unsigned off);
    begin
      hdr_u16 = {hdr_b[off+1], hdr_b[off]};
    end
  endfunction

  integer j;
  integer b;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      lstate <= L_IDLE;
      obj_valid <= '0;
      obj_ready <= '0;
      obj_staging <= '0;
      lru_ctr <= 32'd0;
      for (j = 0; j < int'(OBJECTS); j = j + 1) begin
        obj_gen[j] <= '0;
        obj_pins[j] <= 16'd0;
        obj_prog_hash[j] <= 32'd0;
        obj_handle32[j] <= 32'd0;
        obj_epoch[j] <= 32'd0;
        obj_full_crc[j] <= 32'd0;
        obj_lru[j] <= 32'd0;
      end
      for (j = 0; j < ZFH_HDR_BYTES; j = j + 1) hdr_b[j] <= 8'd0;
      rq_kind <= 2'd0;
      rq_verb <= 8'd0;
      rq_addr <= 64'd0;
      rq_len <= 32'd0;
      rq_hash <= 32'd0;
      rq_ticket <= 32'd0;
      rq_plan <= 32'd0;
      rq_bind_handle <= 32'd0;
      rq_bind_prog <= 32'd0;
      rq_bind_epoch <= 32'd0;
      st_idx <= '0;
      st_evicted <= 1'b0;
      st_total <= 32'd0;
      st_got <= 32'd0;
      st_crc <= 32'hFFFF_FFFF;
      st_sections <= 16'd0;
      st_prog_hash <= 32'd0;
      st_handle32 <= 32'd0;
      st_epoch <= 32'd0;
      st_full_crc <= 32'd0;
      st_verdict <= V_OK;
      st_fail <= 1'b0;
      beat_n <= 8'd0;
      burst_off <= 32'd0;
      burst_len <= 7'd0;
      fh2_resp_valid_o <= 1'b0;
      fh2_resp_ok_o <= 1'b0;
      fh2_resp_verdict_o <= V_OK;
      fh2_resp_ticket_o <= 32'd0;
      fh2_resp_plan_o <= 32'd0;
      fh2_resp_handle_o <= 32'd0;
      fh2_resp_slot_o <= '0;
      fh2_resp_evicted_o <= 1'b0;
      installs_ok_o <= 32'd0;
      installs_failed_o <= 32'd0;
      binds_ok_o <= 32'd0;
      binds_failed_o <= 32'd0;
      controls_ok_o <= 32'd0;
      bad_operation_o <= 32'd0;
      bad_envelope_o <= 32'd0;
      bad_range_o <= 32'd0;
      bad_section_o <= 32'd0;
      bad_crc_o <= 32'd0;
      bad_meta_o <= 32'd0;
      bridge_errs_o <= 32'd0;
      no_capacity_o <= 32'd0;
      evictions_o <= 32'd0;
      pin_forced_victim_o <= 32'd0;
      load_bytes_o <= 32'd0;
    end else begin
      if (fh2_resp_valid_o && fh2_resp_ready_i) fh2_resp_valid_o <= 1'b0;

      case (lstate)
        // --------------------------------------------------------------------
        // ACCEPT: capture the FULL request. Nothing is re-read from a pin
        // later -- the live-pin pattern the old doorbell carried is exactly
        // what directive 10.2 forbids propagating into FH2.
        // --------------------------------------------------------------------
        L_IDLE: begin
          if (fh2_valid_i && fh2_ready_o) begin
            rq_kind   <= fh2_kind_i;
            rq_verb   <= fh2_verb_i;
            rq_hash   <= fh2_hash_i;
            rq_ticket <= fh2_ticket_i;
            rq_plan   <= fh2_plan_i;
            // INSTALL_CAPSULE payload, directive 10.2.
            rq_addr   <= {fh2_data_i[63:32], fh2_data_i[31:0]};
            rq_len    <= fh2_data_i[95:64];
            // BIND_PROGRAM payload, directive 10.2.
            rq_bind_prog   <= fh2_data_i[31:0];
            rq_bind_handle <= fh2_data_i[63:32];
            rq_bind_epoch  <= fh2_data_i[95:64];
            st_crc      <= 32'hFFFF_FFFF;
            st_got      <= 32'd0;
            st_fail     <= 1'b0;
            st_verdict  <= V_OK;
            st_evicted  <= 1'b0;
            beat_n      <= 8'd0;
            burst_off   <= 32'd0;
            lstate      <= L_ACCEPT;
          end
        end

        L_ACCEPT: begin
          // ---- THE REPLY NAMES *THIS* REQUEST'S OBJECT --------------------
          // `st_idx` is what `L_RETURN` puts on `fh2_resp_slot_o` and what it
          // builds `fh2_resp_handle_o` from. Until 2026-09-22 it was written
          // at EXACTLY ONE SITE -- `L_RESERVE`, on the INSTALL path -- so a
          // BIND or a CONTROL verb replied with the slot and the generation of
          // whatever object the LAST INSTALL reserved. It survived testing
          // because the natural stimulus is install-then-bind-what-you-just-
          // installed, where `st_idx` is right BY ACCIDENT; it diverges on
          // install(A), install(B), bind(A), and on any bind after reset.
          //
          // Found and written down by packet WARPBUILD, repaired here. It is
          // load-bearing for GEOM.WARP: `zhao_geom_warp.d_slot_i` is resolved
          // from a BIND reply, so a wrong slot in the reply is a draw
          // deforming against the wrong resident program with every counter
          // reading clean.
          //
          // `rq_bind_handle` / `rq_bind_prog` are latched in `L_IDLE` when the
          // post is taken, one state earlier, so both hold THIS request here.
          // INSTALL is untouched: it has no named object yet, and `L_RESERVE`
          // writes `st_idx` with the slot the allocator chose.
          case (rq_kind)
            K_INSTALL: lstate <= L_ENVELOPE;
            K_BIND: begin
              // BIND resolves that exact READY image object and verifies its
              // canonical handle, epoch and image identity. The canonical hash
              // comes from the MATCHED OBJECT, never reconstructed from a
              // physical slot, and BIND never chooses the first matching
              // canonical hash -- directive 10.2.
              //
              // The reply names the object the REQUEST named, whether or not
              // the six-way conjunction in `L_CHECK` holds. A refusal that
              // reported a different object's slot would send the caller to
              // re-examine an object it never asked about.
              st_idx <= rq_bind_handle[OBJW-1:0];
              lstate <= L_CHECK;
            end
            K_CONTROL: begin
              // The control verbs address their object through `rq_bind_prog`
              // (`ctrl_handle_ok` and every arm of `L_CHECK`'s K_CONTROL case
              // index by it), so that is the object this reply is about.
              st_idx <= rq_bind_prog[OBJW-1:0];
              lstate <= L_CHECK;
            end
            default: begin
              // Kind 3 is RESERVED. Answered BAD_OPERATION, counted, and no
              // byte is read and no object touched -- FT060's second half.
              // The reply names NO object: zero rather than the last install's
              // slot, because a reserved kind identified nothing to name.
              st_idx <= '0;
              st_verdict <= V_BAD_OPERATION;
              st_fail <= 1'b1;
              lstate <= L_FAIL;
            end
          endcase
        end

        // --------------------------------------------------------------------
        // VALIDATE_ENVELOPE: alignment, length, carry-safe range, region.
        // Every one of these refuses BEFORE a slot is reserved, so a bad
        // capsule cannot even displace a resident object.
        // --------------------------------------------------------------------
        L_ENVELOPE: begin
          if ((rq_len == 32'd0) || ((rq_len % ZFH_ALIGNMENT) != 0) ||
              (rq_len > MAX_CAPSULE_BYTES) || (rq_len < ZFH_HDR_BYTES)) begin
            st_verdict <= V_BAD_ENVELOPE;
            st_fail <= 1'b1;
            lstate <= L_FAIL;
          end else if ((rq_addr[63:32] != 32'd0) ||
                       // carry-safe: the sum is formed in 64 bits and compared
                       // there, so an address+length that wraps 32 bits cannot
                       // present as a small in-range extent.
                       ((rq_addr + {32'd0, rq_len}) > 64'(cfg_stage_base_i) + 64'(cfg_stage_bytes_i)) ||
                       (rq_addr < 64'(cfg_stage_base_i)) ||
                       ((rq_addr[5:0]) != 6'd0)) begin
            st_verdict <= V_BAD_RANGE;
            st_fail <= 1'b1;
            lstate <= L_FAIL;
          end else begin
            burst_off <= 32'd0;
            burst_len <= 7'(BURST_BYTES);
            beat_n <= 8'd0;
            lstate <= L_HDR_REQ;
          end
        end

        // --------------------------------------------------------------------
        // READ_HEADER: a real bridge transaction, with exact byte accounting.
        // --------------------------------------------------------------------
        L_HDR_REQ: begin
          if (hps_req_grant_i) begin
            beat_n <= 8'd0;
            lstate <= L_HDR_RCV;
          end
        end

        L_HDR_RCV: begin
          if (hps_rsp_i.err) begin
            st_verdict <= V_BRIDGE_ERR;
            st_fail <= 1'b1;
            if (bridge_errs_o != 32'hFFFF_FFFF) bridge_errs_o <= bridge_errs_o + 32'd1;
            lstate <= L_FAIL;
          end else if (hps_rsp_i.beat_valid) begin
            for (b = 0; b < 8; b = b + 1) begin
              hdr_b[(beat_n * 8) + b] <= hps_rsp_i.data[(b*8) +: 8];
            end
            beat_n <= beat_n + 8'd1;
            st_got <= st_got + 32'd8;
            if (load_bytes_o != 32'hFFFF_FFFF) load_bytes_o <= load_bytes_o + 32'd8;
            if (hps_rsp_i.last) begin
              // EARLY LAST is a distinct fault from a missing beat: the bridge
              // said the burst is over before it delivered what it promised.
              if (beat_n != 8'(BEATS_PER_BURST - 1)) begin
                st_verdict <= V_BRIDGE_ERR;
                st_fail <= 1'b1;
                if (bridge_errs_o != 32'hFFFF_FFFF) bridge_errs_o <= bridge_errs_o + 32'd1;
                lstate <= L_FAIL;
              end else begin
                lstate <= L_HDR_CHECK;
              end
            end else if (beat_n >= 8'(BEATS_PER_BURST - 1)) begin
              // An EXTRA beat: more data than the burst length allows.
              st_verdict <= V_BRIDGE_ERR;
              st_fail <= 1'b1;
              if (bridge_errs_o != 32'hFFFF_FFFF) bridge_errs_o <= bridge_errs_o + 32'd1;
              lstate <= L_FAIL;
            end
          end
        end

        // --------------------------------------------------------------------
        // The envelope checks that need the header's own bytes.
        // --------------------------------------------------------------------
        L_HDR_CHECK: begin
          st_total     <= hdr_u32(ZFH_HDR_OFF_TOTAL_BYTES);
          st_sections  <= hdr_u16(ZFH_HDR_OFF_SECTION_COUNT);
          st_prog_hash <= hdr_u32(ZFH_HDR_OFF_CANONICAL_PROGRAM_HASH);
          st_handle32  <= hdr_u32(ZFH_HDR_OFF_CANONICAL_PROGRAM_HANDLE32);
          st_epoch     <= hdr_u32(ZFH_HDR_OFF_RESOURCE_EPOCH);
          st_full_crc  <= hdr_u32(ZFH_HDR_OFF_CANONICAL_FULL_IMAGE_CRC32C);

          if ((hdr_b[ZFH_HDR_OFF_MAGIC + 0] != 8'(ZFH_MAGIC0)) ||
              (hdr_b[ZFH_HDR_OFF_MAGIC + 1] != 8'(ZFH_MAGIC1)) ||
              (hdr_b[ZFH_HDR_OFF_MAGIC + 2] != 8'(ZFH_MAGIC2)) ||
              (hdr_b[ZFH_HDR_OFF_MAGIC + 3] != 8'(ZFH_MAGIC3)) ||
              (hdr_u16(ZFH_HDR_OFF_IMAGE_VERSION) != 16'(ZFH_IMAGE_VERSION)) ||
              (hdr_u16(ZFH_HDR_OFF_HOST_PROTOCOL_VERSION) != 16'(ZFH_HOST_PROTOCOL_VERSION)) ||
              (hdr_u16(ZFH_HDR_OFF_HEADER_BYTES) != 16'(ZFH_HEADER_BYTES)) ||
              (hdr_b[ZFH_HDR_OFF_OBJECT_KIND] > 8'(ZFH_OBJECT_ASSOCIATION)) ||
              (hdr_b[ZFH_HDR_OFF_FLAGS] != 8'd0) ||
              (hdr_u16(ZFH_HDR_OFF_RESERVED_18) != 16'd0) ||
              (hdr_u32(ZFH_HDR_OFF_RESERVED_60) != 32'd0)) begin
            st_verdict <= V_BAD_ENVELOPE;
            st_fail <= 1'b1;
            if (bad_envelope_o != 32'hFFFF_FFFF) bad_envelope_o <= bad_envelope_o + 32'd1;
            lstate <= L_FAIL;
          end else if ((hdr_u32(ZFH_HDR_OFF_TOTAL_BYTES) != rq_len) ||
                       ((hdr_u32(ZFH_HDR_OFF_TOTAL_BYTES) % ZFH_ALIGNMENT) != 0)) begin
            // The capsule's own length must agree with the one the caller
            // declared. Believing either alone is how a short read becomes a
            // long object with stale RAM at the end of it.
            st_verdict <= V_BAD_ENVELOPE;
            st_fail <= 1'b1;
            if (bad_envelope_o != 32'hFFFF_FFFF) bad_envelope_o <= bad_envelope_o + 32'd1;
            lstate <= L_FAIL;
          end else if ((hdr_u16(ZFH_HDR_OFF_SECTION_COUNT) == 16'd0) ||
                       (hdr_u16(ZFH_HDR_OFF_SECTION_COUNT) > 16'(MAX_SECTIONS)) ||
                       // The directory must fit between the header and the
                       // first 64-byte-aligned body boundary it declares. The
                       // multiply is WIDENED BEFORE it happens, so a large
                       // section_count cannot wrap into a small product and
                       // present as a directory that fits.
                       ((32'(ZFH_HDR_BYTES) +
                         (32'(hdr_u16(ZFH_HDR_OFF_SECTION_COUNT)) * 32'(ZFH_SEC_BYTES))) >
                        hdr_u32(ZFH_HDR_OFF_TOTAL_BYTES))) begin
            st_verdict <= V_BAD_SECTION;
            st_fail <= 1'b1;
            if (bad_section_o != 32'hFFFF_FFFF) bad_section_o <= bad_section_o + 32'd1;
            lstate <= L_FAIL;
          end else if (CHECK_EPOCH &&
                       (hdr_u32(ZFH_HDR_OFF_RESOURCE_EPOCH) != cfg_epoch_i)) begin
            // A capsule stamped for a different resource epoch is STALE. It is
            // refused here, before a slot is reserved, so a stale image cannot
            // displace a live one on its way to being rejected.
            st_verdict <= V_BAD_META;
            st_fail <= 1'b1;
            if (bad_meta_o != 32'hFFFF_FFFF) bad_meta_o <= bad_meta_o + 32'd1;
            lstate <= L_FAIL;
          end else begin
            lstate <= L_RESERVE;
          end
        end

        // --------------------------------------------------------------------
        // RESERVE_OBJECT: select an UNPINNED slot; never publish it yet.
        // --------------------------------------------------------------------
        L_RESERVE: begin
          if (!alloc_any) begin
            // Every valid object is pinned. NO_CAPACITY is returned PROMPTLY so
            // a release queued behind this install is not starved -- directive
            // 10.3's "must not monopolize the maintenance head".
            st_verdict <= V_NO_CAPACITY;
            st_fail <= 1'b1;
            if (no_capacity_o != 32'hFFFF_FFFF) no_capacity_o <= no_capacity_o + 32'd1;
            lstate <= L_FAIL;
          end else begin
            st_idx <= alloc_idx;
            st_evicted <= alloc_evicts;
            obj_staging[alloc_idx] <= 1'b1;
            // The reserved object is NOT ready. Clearing it here rather than at
            // SEAL means an interrupted install leaves nothing executable
            // behind it (FT056), including the object it displaced.
            obj_ready[alloc_idx] <= 1'b0;
            obj_valid[alloc_idx] <= 1'b1;
            if (alloc_evicts) begin
              if (evictions_o != 32'hFFFF_FFFF) evictions_o <= evictions_o + 32'd1;
            end
            // FT057, AND THE NAME IS THE CORRECTION. This counter first read
            // `alloc_idx != (rq_bind_handle & OBJECTS-1)` and was called
            // `pin_forced_victim_o`. That was WRONG IN THE MISLEADING DIRECTION:
            // directive 10.2 makes INSTALL's slot/addr operands "reserved
            // zero", so the "hint" it differenced against was always 0 and the
            // counter really read "the allocator did not pick slot 0". It
            // could fire, so no blindness check caught it; it simply measured
            // something other than its own name -- the wrong diagnosis
            // attached to a working alarm.
            //
            // What is worth counting is the thing FH16 actually promises: that
            // a PIN can change which object is evicted. This fires when an
            // eviction chose a victim OTHER than the globally least-recently-
            // used one, which can only happen because the LRU was pinned.
            if (alloc_evicts && lru_blind_any && (alloc_idx != lru_blind_idx)) begin
              if (pin_forced_victim_o != 32'hFFFF_FFFF) begin
                pin_forced_victim_o <= pin_forced_victim_o + 32'd1;
              end
            end
            burst_off <= 32'd0;
            burst_len <= 7'(BURST_BYTES);
            beat_n <= 8'd0;
            st_got <= 32'd0;
            st_crc <= 32'hFFFF_FFFF;
            lstate <= L_BODY_REQ;
          end
        end

        // --------------------------------------------------------------------
        // READ_AND_CHECK: every byte of the capsule, CRC'd as it arrives.
        // --------------------------------------------------------------------
        L_BODY_REQ: begin
          if (hps_req_grant_i) begin
            beat_n <= 8'd0;
            lstate <= L_BODY_RCV;
          end
        end

        L_BODY_RCV: begin
          if (hps_rsp_i.err) begin
            st_verdict <= V_BRIDGE_ERR;
            st_fail <= 1'b1;
            if (bridge_errs_o != 32'hFFFF_FFFF) bridge_errs_o <= bridge_errs_o + 32'd1;
            lstate <= L_FAIL;
          end else if (hps_rsp_i.beat_valid) begin
            // One beat through the shared fold, with the body_crc32c field
            // masked to zero exactly as the packer's own rule requires.
            st_crc <= crc_next_c;

            beat_n <= beat_n + 8'd1;
            st_got <= st_got + 32'd8;
            if (load_bytes_o != 32'hFFFF_FFFF) load_bytes_o <= load_bytes_o + 32'd8;

            if (hps_rsp_i.last) begin
              if (beat_n != 8'(BEATS_PER_BURST - 1)) begin
                st_verdict <= V_BRIDGE_ERR;
                st_fail <= 1'b1;
                if (bridge_errs_o != 32'hFFFF_FFFF) bridge_errs_o <= bridge_errs_o + 32'd1;
                lstate <= L_FAIL;
              end else if ((burst_off + 32'(BURST_BYTES)) >= st_total) begin
                lstate <= L_CHECK;
              end else begin
                burst_off <= burst_off + 32'(BURST_BYTES);
                beat_n <= 8'd0;
                lstate <= L_BODY_REQ;
              end
            end else if (beat_n >= 8'(BEATS_PER_BURST - 1)) begin
              st_verdict <= V_BRIDGE_ERR;
              st_fail <= 1'b1;
              if (bridge_errs_o != 32'hFFFF_FFFF) bridge_errs_o <= bridge_errs_o + 32'd1;
              lstate <= L_FAIL;
            end
          end
        end

        // --------------------------------------------------------------------
        // The checks that need the whole image, and the two non-install kinds.
        // --------------------------------------------------------------------
        L_CHECK: begin
          if (rq_kind == K_CONTROL) begin
            case (rq_verb)
              VERB_OPEN_ASSOCIATION: begin
                // FH16: acquire and PIN at association open. The handle names a
                // READY object; a staging or absent object cannot be opened.
                if (ctrl_handle_ok &&
                    obj_ready[rq_bind_prog[OBJW-1:0]] &&
                    !obj_staging[rq_bind_prog[OBJW-1:0]] &&
                    (obj_gen[rq_bind_prog[OBJW-1:0]] == rq_bind_prog[16 +: GENW])) begin
                  obj_pins[rq_bind_prog[OBJW-1:0]] <= obj_pins[rq_bind_prog[OBJW-1:0]] + 16'd1;
                  st_verdict <= V_OK;
                  st_fail <= 1'b0;
                end else begin
                  st_verdict <= V_BAD_BINDING;
                  st_fail <= 1'b1;
                end
              end
              VERB_CLOSE_ASSOCIATION, VERB_CANCEL_ASSOCIATION, VERB_RELEASE_BINDING: begin
                // CLOSE means no more points will be offered, not "throw away
                // owed results" -- directive 10.2. Here that is exactly one
                // act: drop the pin. Owed results live in the host's own
                // retirement path and this block never touches them.
                if (ctrl_handle_ok &&
                    obj_valid[rq_bind_prog[OBJW-1:0]] &&
                    (obj_pins[rq_bind_prog[OBJW-1:0]] != 16'd0)) begin
                  obj_pins[rq_bind_prog[OBJW-1:0]] <= obj_pins[rq_bind_prog[OBJW-1:0]] - 16'd1;
                  st_verdict <= V_OK;
                  st_fail <= 1'b0;
                end else begin
                  st_verdict <= V_BAD_BINDING;
                  st_fail <= 1'b1;
                end
              end
              VERB_QUERY_OBJECT, VERB_EPOCH_BARRIER: begin
                st_verdict <= V_OK;
                st_fail <= 1'b0;
              end
              default: begin
                // An UNKNOWN VERB is a counted refusal and writes nothing --
                // FT060's "unknown kind/verb receives a refusal and no
                // uop/table/scalar write".
                st_verdict <= V_BAD_OPERATION;
                st_fail <= 1'b1;
              end
            endcase
            lstate <= L_RETURN;
          end else if (rq_kind == K_BIND) begin
            // Resolve that EXACT object. Equal canonical hash is not enough --
            // FT058: "equal canonical code/table hash but different I/O
            // metadata does not cause semantic object aliasing. Compare exact
            // stored image identity."
            if (bind_handle_ok &&
                obj_ready[rq_bind_handle[OBJW-1:0]] &&
                !obj_staging[rq_bind_handle[OBJW-1:0]] &&
                (obj_gen[rq_bind_handle[OBJW-1:0]] == rq_bind_handle[16 +: GENW]) &&
                (obj_handle32[rq_bind_handle[OBJW-1:0]] == rq_bind_prog) &&
                (obj_epoch[rq_bind_handle[OBJW-1:0]] == rq_bind_epoch) &&
                (obj_full_crc[rq_bind_handle[OBJW-1:0]] == rq_hash)) begin
              st_verdict <= V_OK;
              st_fail <= 1'b0;
            end else begin
              st_verdict <= V_BAD_BINDING;
              st_fail <= 1'b1;
            end
            lstate <= L_RETURN;
          end else begin
            // INSTALL. Exact received-byte accounting, then the CRC, then the
            // metadata rule. Order matters only in that all three must hold.
            if (st_got != st_total) begin
              st_verdict <= V_BRIDGE_ERR;
              st_fail <= 1'b1;
              if (bridge_errs_o != 32'hFFFF_FFFF) bridge_errs_o <= bridge_errs_o + 32'd1;
              lstate <= L_FAIL;
            end else if ((32'(ZFH_HDR_BYTES) + (32'(st_sections) * 32'(ZFH_SEC_BYTES))) >
                         st_got) begin
              // The BOUNDED-COUNT check the directive assigns to hardware
              // (11.3: "the hardware checks the bounded masks/counts, installed
              // identity and required accepted preload writes"). The deep
              // read-before-definition walk stays in the C++ validator, where
              // 11.3 puts it -- this is the cheap structural half, and it is
              // re-checked against the bytes ACTUALLY RECEIVED rather than
              // against the length the header claimed, because those are
              // different facts and only the second one was measured.
              st_verdict <= V_BAD_SECTION;
              st_fail <= 1'b1;
              if (bad_section_o != 32'hFFFF_FFFF) bad_section_o <= bad_section_o + 32'd1;
              lstate <= L_FAIL;
            end else if ((st_crc ^ 32'hFFFF_FFFF) != rq_hash) begin
              st_verdict <= V_BAD_CRC;
              st_fail <= 1'b1;
              if (bad_crc_o != 32'hFFFF_FFFF) bad_crc_o <= bad_crc_o + 32'd1;
              lstate <= L_FAIL;
            end else if (hdr_b[ZFH_HDR_OFF_OBJECT_KIND] == 8'(ZFH_OBJECT_PROGRAM) &&
                         (hdr_u32(ZFH_HDR_OFF_CANONICAL_PROGRAM_HANDLE32) == 32'd0)) begin
              // R111's standing hazard, in its FH2 form: a PROGRAM image with
              // no canonical handle cannot be bound to later, so installing it
              // would publish an object nothing can name.
              st_verdict <= V_BAD_META;
              st_fail <= 1'b1;
              if (bad_meta_o != 32'hFFFF_FFFF) bad_meta_o <= bad_meta_o + 32'd1;
              lstate <= L_FAIL;
            end else begin
              lstate <= L_SEAL;
            end
          end
        end

        // --------------------------------------------------------------------
        // SEAL: the ONE place obj_ready is set, and it is reached only when
        // every check above passed.
        // --------------------------------------------------------------------
        L_SEAL: begin
          obj_ready[st_idx]     <= 1'b1;
          obj_staging[st_idx]   <= 1'b0;
          obj_gen[st_idx]       <= obj_gen[st_idx] + 1'b1;
          obj_pins[st_idx]      <= 16'd0;
          obj_prog_hash[st_idx] <= st_prog_hash;
          obj_handle32[st_idx]  <= st_handle32;
          obj_epoch[st_idx]     <= st_epoch;
          obj_full_crc[st_idx]  <= st_full_crc;
          obj_lru[st_idx]       <= lru_ctr + 32'd1;
          lru_ctr               <= lru_ctr + 32'd1;
          st_verdict <= V_OK;
          st_fail <= 1'b0;
          lstate <= L_RETURN;
        end

        // --------------------------------------------------------------------
        // FAIL: discard the incomplete staging object. Prior READY objects are
        // untouched -- except the one this transaction legitimately displaced,
        // which is gone because the transaction claimed it and did not finish.
        // That is the directive's "retain prior ready objects unless explicitly
        // replaced under the transaction".
        // --------------------------------------------------------------------
        L_FAIL: begin
          if (obj_staging[st_idx]) begin
            obj_staging[st_idx] <= 1'b0;
            obj_ready[st_idx]   <= 1'b0;
            obj_valid[st_idx]   <= 1'b0;
            obj_pins[st_idx]    <= 16'd0;
          end
          lstate <= L_RETURN;
        end

        // --------------------------------------------------------------------
        // RETURN: exactly one terminal reply, with the caller's own ticket.
        // --------------------------------------------------------------------
        L_RETURN: begin
          if (!fh2_resp_valid_o) begin
            fh2_resp_valid_o   <= 1'b1;
            fh2_resp_ok_o      <= !st_fail;
            fh2_resp_verdict_o <= st_verdict;
            fh2_resp_ticket_o  <= rq_ticket;
            fh2_resp_plan_o    <= rq_plan;
            fh2_resp_slot_o    <= st_idx;
            fh2_resp_evicted_o <= st_evicted && !st_fail;
            // The handle is {generation, index}: a game-facing binding that
            // survives the slot being reused, which a raw slot number does not.
            fh2_resp_handle_o  <= st_fail ? 32'd0
                                          : {8'd0, obj_gen[st_idx], 8'd0, {(8-OBJW){1'b0}}, st_idx};

            case (rq_kind)
              K_INSTALL: begin
                if (st_fail) begin
                  if (installs_failed_o != 32'hFFFF_FFFF) begin
                    installs_failed_o <= installs_failed_o + 32'd1;
                  end
                end else if (installs_ok_o != 32'hFFFF_FFFF) begin
                  installs_ok_o <= installs_ok_o + 32'd1;
                end
              end
              K_BIND: begin
                if (st_fail) begin
                  if (binds_failed_o != 32'hFFFF_FFFF) binds_failed_o <= binds_failed_o + 32'd1;
                end else if (binds_ok_o != 32'hFFFF_FFFF) begin
                  binds_ok_o <= binds_ok_o + 32'd1;
                end
              end
              K_CONTROL: begin
                if (!st_fail && (controls_ok_o != 32'hFFFF_FFFF)) begin
                  controls_ok_o <= controls_ok_o + 32'd1;
                end
              end
              default: ;
            endcase

            if (st_fail && (st_verdict == V_BAD_OPERATION)) begin
              if (bad_operation_o != 32'hFFFF_FFFF) bad_operation_o <= bad_operation_o + 32'd1;
            end
            if (st_fail && (st_verdict == V_BAD_RANGE)) begin
              if (bad_range_o != 32'hFFFF_FFFF) bad_range_o <= bad_range_o + 32'd1;
            end
            // V_BAD_ENVELOPE, V_BAD_SECTION, V_BAD_CRC, V_BAD_META,
            // V_BRIDGE_ERR and V_NO_CAPACITY are each counted at the site that
            // DECIDED them, so a later reader can tell which check fired from
            // the counter alone rather than inferring it from the verdict.
          end else if (fh2_resp_ready_i) begin
            lstate <= L_IDLE;
          end
        end

        default: lstate <= L_IDLE;
      endcase
    end
  end

endmodule
