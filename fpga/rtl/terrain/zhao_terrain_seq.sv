// zhao_terrain_seq.sv - the terrain command path: one sealed patch list in,
// residency / writeback / load / compose-slot / patch-issue work out.
//
// Law: reference/include/zref/zref_terrain_seq.hpp (zref::terrain::seq::Sequencer)
//      design/contracts/TERRAIN.SEQ.md
//      reports/OWNER-RULINGS-BUILDABILITY-20260902.md  T4, T5, T6, T7, T9, T10
//      reports/TERRAIN_WORLD_LAYER_ARCHITECTURE.md     2.6
//
// ===========================================================================
// THE HOLE THIS FILLS, IN THE OWNER'S OWN WORDS
// ===========================================================================
// reports/Missingterrain, on why the shipped world is "a little spot":
//
//     "Nothing currently does: camera moved -> inspect island directory ->
//      determine visible patch coordinates -> union the two players' working
//      sets -> prefetch missing pages -> allocate local-SDRAM residency slots
//      -> preserve dirty scars from evicted pages -> issue all visible
//      patches to the terrain engine."
//
// Everything before "prefetch" is built: TERRAIN.ISLAND answers about a patch,
// TERRAIN.VISIBLE walks the window, and zref::swstream::WorldStreamer unions
// the views and SEALS the list. Everything after "issue" is built: LOD, TESS,
// NORMALS and PROJECT are port-compatible and tested pairwise. This block is
// the verb in the middle -- the pump that takes the sealed list and makes the
// rest of the sentence happen.
//
// ===========================================================================
// ONE SubmitTerrainSet, NOT ONE DrawProcedural PER PATCH
// ===========================================================================
// Ruling T5 settles the ABI that architecture 2.6 left open: two versioned
// commands, `TerrainEpoch @ 0x0220` and `SubmitTerrainSet @ 0x0230`, and "ONE
// SubmitTerrainSet covers the whole required+prefetch set. Do not emit one
// DrawProcedural per patch and do not overload an existing terrain field
// command."
//
// So this block's input is a HEADER plus a stream of T5's 32-byte patch-list
// records, already in T5's canonical order, already CRC-sealed upstream. It
// does not sort, does not deduplicate and does not reorder: architecture 2.6
// forbids reordering and 4's determinism table names list order as the anchor
// for the entire world layer. Painter order is semantically observable
// downstream, so a permutation here is a rendering difference no counter would
// show.
//
// ===========================================================================
// A MISS IS SKIPPED, NEVER WAITED ON
// ===========================================================================
// Architecture 2.6: "A patch that is not resident when its turn comes is
// SKIPPED and counted, not stalled on -- the frame must never wait on an 80
// microsecond page load mid-walk; prefetch exists so this is rare, and the
// counter makes it visible."
//
// That is why there is no completion wait anywhere in this FSM. A miss claims
// a slot, emits a writeback job if the victim is dirty and a load job for the
// page, and moves to the next record. The page arrives for a LATER frame. One
// frame of missing ground at a streaming edge, loudly counted, beats a
// deadline fault.
//
// ===========================================================================
// TWO OVERFLOWS THAT ARE NOT THE SAME OVERFLOW
// ===========================================================================
// T6 and T7 both describe running out of something and they mean opposite
// things, so they are kept apart here by construction:
// ENFORCED-BY: tests/terrain/seq_rtl_directed.cpp
//
//   T6  more than 256 REQUIRED DYNAMIC patches needing live composition
//       -> FAULT THE FRAME, drain the rest of the list, record the rejected
//          source id and key. `frame_fault_o`, `fault_*_o`, `frame_faults_o`.
//
//   T7  more than `cfg_load_budget_i` whole pages wanted in one frame
//       -> DEFER THE LOAD and keep going. Proxy-and-continue, RECORDED, and
//          explicitly NOT a frame fault. `loads_deferred_o`.
//
// Reusing T6's fault for T7's case would fault frames the rulings say to
// render, at exactly the moment the player is traversing fastest.
//
// ===========================================================================
// EVERY COUNTER HERE COUNTS EVENTS. NOT ONE OF THEM COUNTS CYCLES.
// ===========================================================================
// Written down because a sibling terrain block shipped two counters whose
// names claimed events and whose bodies counted cycles, and one of them
// reported the producer's patience -- 115 offers spread over 1,783 cycles --
// instead of the refusals it was named for. Every counter below increments on
// exactly one accepted handshake or one consumed record. A stalled consumer
// changes how LONG this block takes and changes no number it reports.
//
// Conservative SystemVerilog subset only (charter section 2).
module zhao_terrain_seq #(
    // T6: the composed height/velocity cache is 256 slots.
    parameter int unsigned COMPOSE_SLOTS = 256,
    // T9/T10: 256 sets x 4 ways = 1,024 slots.
    parameter int unsigned SLOTW = 10,
    // T10: "generation u8 minimum".
    parameter int unsigned GENW = 8,
    // Loader claim sequence, matching zhao_terrain_residency_v2's SEQW.
    parameter int unsigned SEQW = 16,
    // Derived: wide enough to hold COMPOSE_SLOTS itself, so the allocator's
    // exhausted state is representable rather than wrapping onto slot zero.
    parameter int unsigned CSLOTW = $clog2(COMPOSE_SLOTS)
) (
    input var logic clk,
    input var logic rst_n,

    // ---- the frame, from CMD.SCHEDULER + the SubmitTerrainSet header ------
    // CMD.SCHEDULER keeps what it owns (FRAME_RING state and per-frame guard
    // grants); this is the terrain draw path it dispatches to, not a
    // replacement for it.
    input  var logic        fr_start_i,        // 1-cycle pulse: begin this set
    input  var logic [31:0] fr_epoch_i,        // T5 resource_epoch
    input  var logic [15:0] fr_patch_count_i,  // T5 patch_count
    input  var logic [31:0] fr_sequence_i,     // T5 sequence
    output var logic        fr_busy_o,
    output var logic        fr_done_o,         // 1-cycle pulse: set complete

    // ---- knobs -----------------------------------------------------------
    // T7's ceiling as an editable input rather than a literal. SW.STREAM owns
    // the POLICY -- it defers PREFETCH records before sealing -- and this is
    // the hardware backstop underneath it. Board counters may reduce it
    // immediately (T7); raising it wants measured bridge evidence.
    input var logic [15:0] cfg_load_budget_i,

    // ---- T5's 32-byte patch-list record, one per beat --------------------
    input  var logic               rec_valid_i,
    output var logic               rec_ready_o,
    input  var logic [31:0]        rec_island_i,
    input  var logic signed [15:0] rec_ix_i,
    input  var logic signed [15:0] rec_iz_i,
    input  var logic [63:0]        rec_hps_addr_i,
    input  var logic [31:0]        rec_crc_i,
    input  var logic [15:0]        rec_flags_i,   // REQUIRED/PREFETCH/DYNAMIC/DUAL/HAS_SAVED_F
    input  var logic [7:0]         rec_view_mask_i,
    input  var logic [7:0]         rec_priority_i,
    input  var logic [31:0]        rec_src_id_i,

    // ---- TERRAIN.RESIDENCY lookup (master) -------------------------------
    // The directory answers a lookup with fixed latency and no ready, so the
    // request is a single-cycle offer and the answer is awaited.
    output var logic               lu_valid_o,
    // The directory arbitrates one address per clock and a losing query is not
    // answered. Without this the offer was fire-and-forget into a state that
    // waits for ever.
    input  var logic               lu_ready_i,
    output var logic [31:0]        lu_epoch_o,
    output var logic [31:0]        lu_island_o,
    output var logic signed [15:0] lu_ix_o,
    output var logic signed [15:0] lu_iz_o,
    input  var logic               lu_ans_valid_i,
    input  var logic               lu_ans_hit_i,
    input  var logic [SLOTW-1:0]   lu_ans_slot_i,
    input  var logic [GENW-1:0]    lu_ans_gen_i,

    // ---- TERRAIN.RESIDENCY claim (master) --------------------------------
    output var logic               cl_valid_o,
    input  var logic               cl_ready_i,
    output var logic [31:0]        cl_epoch_o,
    output var logic [31:0]        cl_island_o,
    output var logic signed [15:0] cl_ix_o,
    output var logic signed [15:0] cl_iz_o,
    output var logic [31:0]        cl_expect_crc_o,
    output var logic [SEQW-1:0]    cl_seq_o,
    input  var logic               cl_ans_valid_i,
    input  var logic               cl_ans_same_i,
    input  var logic               cl_ans_refused_i,
    input  var logic [SLOTW-1:0]   cl_ans_slot_i,
    input  var logic [GENW-1:0]    cl_ans_gen_i,
    input  var logic               cl_ans_ev_dirty_i,
    input  var logic [31:0]        cl_ans_ev_island_i,
    input  var logic signed [15:0] cl_ans_ev_ix_i,
    input  var logic signed [15:0] cl_ans_ev_iz_i,
    input  var logic [GENW-1:0]    cl_ans_ev_gen_i,

    // ---- TERRAIN.RESIDENCY pin (master) ----------------------------------
    // T10: "no slot reuse before pin count zero". A patch issued to the engine
    // holds its page until the engine is done with it; the unpin is the
    // engine's, on job completion, and is deliberately not this block's -- a
    // pump that unpinned at issue would be promising the page is free while
    // TESS is still reading it.
    output var logic             pin_valid_o,
    input  var logic             pin_ready_i,
    output var logic [SLOTW-1:0] pin_slot_o,
    output var logic [GENW-1:0]  pin_gen_o,
    output var logic [31:0]      pin_epoch_o,

    // ---- writeback job out (T4's F-sheet barrier) ------------------------
    output var logic               wb_valid_o,
    input  var logic               wb_ready_i,
    output var logic [SLOTW-1:0]   wb_slot_o,
    output var logic [GENW-1:0]    wb_gen_o,
    output var logic [31:0]        wb_epoch_o,
    output var logic [31:0]        wb_island_o,
    output var logic signed [15:0] wb_ix_o,
    output var logic signed [15:0] wb_iz_o,
    output var logic [31:0]        wb_src_id_o,

    // ---- load job out (TERRAIN.PAGELOADER's j_* port) --------------------
    output var logic               ld_valid_o,
    input  var logic               ld_ready_i,
    output var logic [SLOTW-1:0]   ld_slot_o,
    output var logic [GENW-1:0]    ld_gen_o,
    output var logic [31:0]        ld_epoch_o,
    output var logic [31:0]        ld_island_o,
    output var logic signed [15:0] ld_ix_o,
    output var logic signed [15:0] ld_iz_o,
    output var logic [63:0]        ld_hps_addr_o,
    output var logic [31:0]        ld_expect_crc_o,
    output var logic [31:0]        ld_src_id_o,

    // ---- patch issue out (to the terrain engine) -------------------------
    // TERRAIN.PATCH + the field engine compose into the slot named by
    // `is_cslot_o`; LOD then reads that patch, TESS reads the front, and the
    // chain from there is already composed and green.
    output var logic               is_valid_o,
    input  var logic               is_ready_i,
    output var logic [SLOTW-1:0]   is_slot_o,
    output var logic [GENW-1:0]    is_gen_o,
    output var logic [31:0]        is_epoch_o,
    output var logic [31:0]        is_island_o,
    output var logic signed [15:0] is_ix_o,
    output var logic signed [15:0] is_iz_o,
    output var logic               is_cslot_valid_o,  // 0 = static/baked: no slot (T6)
    output var logic [CSLOTW-1:0]  is_cslot_o,
    output var logic [15:0]        is_flags_o,
    output var logic [7:0]         is_view_mask_o,
    output var logic [7:0]         is_priority_o,
    output var logic [31:0]        is_src_id_o,

    // ---- T6's frame fault ------------------------------------------------
    // Latched for the rest of the set, cleared by the next `fr_start_i`. The
    // ruling says "record rejected source IDs and keys", so the FIRST rejected
    // identity is held: it is the one that names where the frame's demand
    // exceeded the cache, and overwriting it with later ones would leave the
    // tail of the list rather than the cause.
    output var logic               frame_fault_o,
    output var logic [31:0]        fault_src_id_o,
    output var logic [31:0]        fault_island_o,
    output var logic signed [15:0] fault_ix_o,
    output var logic signed [15:0] fault_iz_o,

    // ---- a tripwire, not a decoration ------------------------------------
    // Latches if a lookup or claim answer arrives while nothing is waiting for
    // one. That is the shape of a directory answering out of order or of a
    // stale answer from a previous record, and every consequence of it is
    // silent: the wrong slot handle attached to the right patch draws another
    // island's ground in this island's place.
    output var logic err_stray_ans_o,

    // ---- counters (events, never cycles) ---------------------------------
    output var logic [31:0] records_consumed_o,
    output var logic [31:0] patches_issued_o,
    output var logic [31:0] prefetch_resident_o,
    output var logic [31:0] skipped_not_resident_o,
    output var logic [31:0] claims_issued_o,
    output var logic [31:0] claims_refused_o,
    output var logic [31:0] claims_same_o,
    output var logic [31:0] loads_issued_o,
    output var logic [31:0] loads_deferred_o,
    output var logic [31:0] writebacks_issued_o,
    output var logic [31:0] compose_slots_used_o,
    output var logic [31:0] pins_issued_o,
    output var logic [31:0] drained_o,
    output var logic [31:0] frame_faults_o
);

  // ---- T5's flag bits, named ----------------------------------------------
  // flags:u16 (REQUIRED, PREFETCH, DYNAMIC, DUAL, HAS_SAVED_F), bit 0 upward,
  // matching zref::swstream::PatchFlags field for field.
  localparam int unsigned FLAG_REQUIRED = 0;
  localparam int unsigned FLAG_DYNAMIC  = 2;

  // ---- states -------------------------------------------------------------
  localparam logic [3:0] S_IDLE    = 4'd0;
  localparam logic [3:0] S_FETCH   = 4'd1;
  localparam logic [3:0] S_LOOKUP  = 4'd2;
  localparam logic [3:0] S_WAIT_LU = 4'd3;
  localparam logic [3:0] S_PIN     = 4'd4;
  localparam logic [3:0] S_ISSUE   = 4'd5;
  localparam logic [3:0] S_CLAIM   = 4'd6;
  localparam logic [3:0] S_WAIT_CL = 4'd7;
  localparam logic [3:0] S_WB      = 4'd8;
  localparam logic [3:0] S_LOAD    = 4'd9;
  localparam logic [3:0] S_DONE    = 4'd10;

  logic [3:0] st, st_n;

  // ---- frame state --------------------------------------------------------
  logic [31:0] epoch_q;
  logic [15:0] want_q;      // patch_count for this set
  logic [31:0] seq_q;       // claim sequence, one per claim, frame-scoped
  logic [15:0] taken_q;     // records accepted so far this set
  logic [CSLOTW:0] cslot_next_q;   // one bit wider than a slot index: holds COMPOSE_SLOTS
  logic [15:0] loads_q;     // pages requested this frame (T7)
  logic        fault_q;

  // ---- the record under service ------------------------------------------
  // CAPTURED AT ACCEPTANCE, every field of it. The record source is a DMA'd
  // ring the frame may still be writing behind us, and reading it live across
  // the four cycles this FSM spends on one record is the ingress-capture rule
  // tools/rtl/check_ingress_capture.py exists to enforce.
  logic [31:0]        r_island_q;
  logic signed [15:0] r_ix_q, r_iz_q;
  logic [63:0]        r_addr_q;
  logic [31:0]        r_crc_q;
  logic [15:0]        r_flags_q;
  logic [7:0]         r_view_q, r_prio_q;
  logic [31:0]        r_src_q;

  wire r_required = r_flags_q[FLAG_REQUIRED];
  wire r_dynamic  = r_flags_q[FLAG_DYNAMIC];

  // ---- captured residency answers ----------------------------------------
  logic [SLOTW-1:0]   a_slot_q;
  logic [GENW-1:0]    a_gen_q;
  logic [31:0]        a_ev_island_q;
  logic signed [15:0] a_ev_ix_q, a_ev_iz_q;
  logic [GENW-1:0]    a_ev_gen_q;
  logic               cs_valid_q;   // this record got a composed slot
  logic [CSLOTW-1:0]  cs_slot_q;

  // ---- counters -----------------------------------------------------------
  logic [31:0] c_rec, c_iss, c_pfr, c_skip, c_clm, c_clr, c_cls;
  logic [31:0] c_ld, c_lddef, c_wb, c_cs, c_pin, c_drain, c_flt;

  logic [31:0]        f_src_q, f_isl_q;
  logic signed [15:0] f_ix_q, f_iz_q;
  logic               err_q;

  // ---- combinational outputs ---------------------------------------------
  // A record is accepted only in S_FETCH, and only while the set still wants
  // records. Accepting a 65,536th record for a set that declared 4 is how a
  // list overrun becomes a silent extra patch.
  wire more_wanted = (taken_q < want_q);

  assign rec_ready_o = (st == S_FETCH) && more_wanted;

  assign lu_valid_o  = (st == S_LOOKUP);
  assign lu_epoch_o  = epoch_q;
  assign lu_island_o = r_island_q;
  assign lu_ix_o     = r_ix_q;
  assign lu_iz_o     = r_iz_q;

  assign cl_valid_o      = (st == S_CLAIM);
  assign cl_epoch_o      = epoch_q;
  assign cl_island_o     = r_island_q;
  assign cl_ix_o         = r_ix_q;
  assign cl_iz_o         = r_iz_q;
  assign cl_expect_crc_o = r_crc_q;
  assign cl_seq_o        = seq_q[SEQW-1:0];

  assign pin_valid_o = (st == S_PIN);
  assign pin_slot_o  = a_slot_q;
  assign pin_gen_o   = a_gen_q;
  assign pin_epoch_o = epoch_q;

  assign wb_valid_o  = (st == S_WB);
  assign wb_slot_o   = a_slot_q;
  assign wb_gen_o    = a_ev_gen_q;
  assign wb_epoch_o  = epoch_q;
  assign wb_island_o = a_ev_island_q;
  assign wb_ix_o     = a_ev_ix_q;
  assign wb_iz_o     = a_ev_iz_q;
  assign wb_src_id_o = r_src_q;

  assign ld_valid_o      = (st == S_LOAD);
  assign ld_slot_o       = a_slot_q;
  assign ld_gen_o        = a_gen_q;
  assign ld_epoch_o      = epoch_q;
  assign ld_island_o     = r_island_q;
  assign ld_ix_o         = r_ix_q;
  assign ld_iz_o         = r_iz_q;
  assign ld_hps_addr_o   = r_addr_q;
  assign ld_expect_crc_o = r_crc_q;
  assign ld_src_id_o     = r_src_q;

  assign is_valid_o       = (st == S_ISSUE);
  assign is_slot_o        = a_slot_q;
  assign is_gen_o         = a_gen_q;
  assign is_epoch_o       = epoch_q;
  assign is_island_o      = r_island_q;
  assign is_ix_o          = r_ix_q;
  assign is_iz_o          = r_iz_q;
  assign is_cslot_valid_o = cs_valid_q;
  assign is_cslot_o       = cs_slot_q;
  assign is_flags_o       = r_flags_q;
  assign is_view_mask_o   = r_view_q;
  assign is_priority_o    = r_prio_q;
  assign is_src_id_o      = r_src_q;

  assign fr_busy_o = (st != S_IDLE);

  assign frame_fault_o  = fault_q;
  assign fault_src_id_o = f_src_q;
  assign fault_island_o = f_isl_q;
  assign fault_ix_o     = f_ix_q;
  assign fault_iz_o     = f_iz_q;
  assign err_stray_ans_o = err_q;

  assign records_consumed_o     = c_rec;
  assign patches_issued_o       = c_iss;
  assign prefetch_resident_o    = c_pfr;
  assign skipped_not_resident_o = c_skip;
  assign claims_issued_o        = c_clm;
  assign claims_refused_o       = c_clr;
  assign claims_same_o          = c_cls;
  assign loads_issued_o         = c_ld;
  assign loads_deferred_o       = c_lddef;
  assign writebacks_issued_o    = c_wb;
  assign compose_slots_used_o   = c_cs;
  assign pins_issued_o          = c_pin;
  assign drained_o              = c_drain;
  assign frame_faults_o         = c_flt;

  // ---- the composed-cache allocator, in one line --------------------------
  // FRAME-SCOPED BY CONSTRUCTION. `cslot_next_q` resets on every fr_start_i,
  // so slot n is the n-th record of THIS frame that needed composition and
  // nothing about the previous frame can reach it.
  // ENFORCED-BY: tests/terrain/seq_rtl_directed.cpp
  //
  // "By construction" earned its enforcer the hard way here: the differential's
  // FIRST run found the slot index held under a low valid bit, so a static
  // issue's slot number was a function of which patch composed EARLIER --
  // history reaching a frame-scoped allocator, and a frame replayed after a
  // different previous frame drove different bits. 16 divergences. It is
  // mutation M14, and it fires.
  // ENFORCED-BY: tests/terrain/seq_rtl_directed.cpp Architecture 2.5 records
  // the rejected alternative -- a coordinate-hashed persistent cache that
  // would let unchanged patches skip re-composition -- and rejects it because
  // terrain_rules 4.2's law is already "produced once per frame" and a
  // persistent cache adds cross-frame state replay must reconstruct.
  //
  // THE CURSOR IS ONE BIT WIDER THAN A SLOT INDEX, and that is not tidiness.
  // COMPOSE_SLOTS is 256 and CSLOTW is 8, so a CSLOTW-wide cursor compared
  // against COMPOSE_SLOTS would be comparing against zero: the allocator would
  // wrap onto slot 0 and hand the 257th patch the first patch's composed
  // lattice. Every height would be a real composed height and every counter
  // would agree; the frame would simply draw one patch's ground in another
  // patch's place. The extra bit makes "exhausted" representable, which is
  // what T6's fault needs to be able to see.
  wire cslot_free = (cslot_next_q < (CSLOTW+1)'(COMPOSE_SLOTS));

  // ---- next state ---------------------------------------------------------
  always_comb begin
    st_n = st;
    case (st)
      S_IDLE:    if (fr_start_i)                st_n = S_FETCH;
      S_FETCH: begin
        if (!more_wanted)                       st_n = S_DONE;
        else if (rec_valid_i)                   st_n = fault_q ? S_FETCH : S_LOOKUP;
      end
      // HOLD UNTIL THE LOOKUP IS TAKEN. This used to advance unconditionally,
      // so a lookup that lost the directory's one-address-per-clock
      // arbitration was never answered and the frame waited for ever --
      // silently, since err_stray_ans_o only fires on an answer that ARRIVES
      // unexpectedly, never on one that never comes.
      //
      // The directory's own comment said queries need no ready because "both
      // callers already tolerate that". This one did not; it now waits for the
      // ready rather than assuming the offer landed.
      S_LOOKUP:  if (lu_ready_i)                st_n = S_WAIT_LU;
      S_WAIT_LU: if (lu_ans_valid_i) begin
        if (lu_ans_hit_i) begin
          // prefetch-only, or a required patch that could not get a slot:
          // neither reaches the engine.
          if (!r_required)                      st_n = S_FETCH;
          else if (r_dynamic && !cslot_free)    st_n = S_FETCH;  // T6 fault
          else                                  st_n = S_PIN;
        end else begin
          if (loads_q >= cfg_load_budget_i)     st_n = S_FETCH;  // T7 defer
          else                                  st_n = S_CLAIM;
        end
      end
      S_PIN:     if (pin_ready_i)               st_n = S_ISSUE;
      S_ISSUE:   if (is_ready_i)                st_n = S_FETCH;
      S_CLAIM:   if (cl_ready_i)                st_n = S_WAIT_CL;
      S_WAIT_CL: if (cl_ans_valid_i) begin
        if (cl_ans_refused_i)                   st_n = S_FETCH;
        else if (cl_ans_ev_dirty_i)             st_n = S_WB;
        else                                    st_n = S_LOAD;
      end
      S_WB:      if (wb_ready_i)                st_n = S_LOAD;
      S_LOAD:    if (ld_ready_i)                st_n = S_FETCH;
      S_DONE:                                   st_n = S_IDLE;
      default:                                  st_n = S_IDLE;
    endcase
  end

  // ---- sequential ---------------------------------------------------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st           <= S_IDLE;
      epoch_q      <= 32'd0;
      want_q       <= 16'd0;
      seq_q        <= 32'd0;
      taken_q      <= 16'd0;
      cslot_next_q <= {(CSLOTW+1){1'b0}};
      loads_q      <= 16'd0;
      fault_q      <= 1'b0;
      r_island_q   <= 32'd0;
      r_ix_q       <= 16'sd0;
      r_iz_q       <= 16'sd0;
      r_addr_q     <= 64'd0;
      r_crc_q      <= 32'd0;
      r_flags_q    <= 16'd0;
      r_view_q     <= 8'd0;
      r_prio_q     <= 8'd0;
      r_src_q      <= 32'd0;
      a_slot_q     <= {SLOTW{1'b0}};
      a_gen_q      <= {GENW{1'b0}};
      a_ev_island_q<= 32'd0;
      a_ev_ix_q    <= 16'sd0;
      a_ev_iz_q    <= 16'sd0;
      a_ev_gen_q   <= {GENW{1'b0}};
      cs_valid_q   <= 1'b0;
      cs_slot_q    <= {CSLOTW{1'b0}};
      f_src_q      <= 32'd0;
      f_isl_q      <= 32'd0;
      f_ix_q       <= 16'sd0;
      f_iz_q       <= 16'sd0;
      err_q        <= 1'b0;
      fr_done_o    <= 1'b0;
      c_rec        <= 32'd0;
      c_iss        <= 32'd0;
      c_pfr        <= 32'd0;
      c_skip       <= 32'd0;
      c_clm        <= 32'd0;
      c_clr        <= 32'd0;
      c_cls        <= 32'd0;
      c_ld         <= 32'd0;
      c_lddef      <= 32'd0;
      c_wb         <= 32'd0;
      c_cs         <= 32'd0;
      c_pin        <= 32'd0;
      c_drain      <= 32'd0;
      c_flt        <= 32'd0;
    end else begin
      st        <= st_n;
      fr_done_o <= (st == S_DONE);

      // A stray answer is one arriving in a state that did not ask. Latched,
      // never cleared inside a set -- a tripwire that resets itself is a
      // tripwire that reports the last event rather than whether there was one.
      if (lu_ans_valid_i && (st != S_WAIT_LU)) err_q <= 1'b1;
      if (cl_ans_valid_i && (st != S_WAIT_CL)) err_q <= 1'b1;

      case (st)
        // ---- a new set ----------------------------------------------------
        // EVERY frame-scoped register is cleared here and not one of them
        // survives. The allocator, the load budget, the claim sequence, the
        // fault latch and the record cursor are all properties of THIS set.
        S_IDLE: if (fr_start_i) begin
          epoch_q      <= fr_epoch_i;
          want_q       <= fr_patch_count_i;
          seq_q        <= fr_sequence_i;
          taken_q      <= 16'd0;
          cslot_next_q <= {(CSLOTW+1){1'b0}};
          loads_q      <= 16'd0;
          fault_q      <= 1'b0;
          err_q        <= 1'b0;
          f_src_q      <= 32'd0;
          f_isl_q      <= 32'd0;
          f_ix_q       <= 16'sd0;
          f_iz_q       <= 16'sd0;
        end

        S_FETCH: if (more_wanted && rec_valid_i) begin
          taken_q    <= taken_q + 16'd1;
          c_rec      <= c_rec + 32'd1;
          r_island_q <= rec_island_i;
          r_ix_q     <= rec_ix_i;
          r_iz_q     <= rec_iz_i;
          r_addr_q   <= rec_hps_addr_i;
          r_crc_q    <= rec_crc_i;
          r_flags_q  <= rec_flags_i;
          r_view_q   <= rec_view_mask_i;
          r_prio_q   <= rec_priority_i;
          r_src_q    <= rec_src_id_i;
          cs_valid_q <= 1'b0;
          // AND THE INDEX WITH IT, which the first version did not do.
          //
          // Holding the last dynamic patch's slot index under a low valid bit
          // looks harmless -- the consumer is told the slot is not valid --
          // and it is not. It makes `is_cslot_o` on a STATIC issue a function
          // of which patch composed earlier in the frame, so the same frame
          // replayed after a different previous frame drives different bits on
          // that port. The whole point of a frame-scoped allocator is that no
          // history reaches it, and a held index is history. Worse, a consumer
          // that dropped the valid bit would compose into the previous
          // patch's slot and overwrite a lattice something else is about to
          // read: real composed heights, in the wrong patch's place, with
          // every counter agreeing.
          //
          // Zero is a legal slot, so this is not a poison value -- there is no
          // spare encoding in CSLOTW bits when COMPOSE_SLOTS is 2**CSLOTW,
          // exactly as TERRAIN.COMPCACHE found for its two-bit substance. The
          // guarantee is that the port is a pure function of THIS frame, and
          // `is_cslot_valid_o` remains the only thing that says whether to
          // look at it.
          cs_slot_q  <= {CSLOTW{1'b0}};
          // T6's drain. The rest of the list is still consumed -- a sealed
          // list is a unit and leaving half of it in the ring strands the next
          // frame behind it -- but nothing further is issued.
          if (fault_q) c_drain <= c_drain + 32'd1;
        end

        S_WAIT_LU: if (lu_ans_valid_i) begin
          a_slot_q <= lu_ans_slot_i;
          a_gen_q  <= lu_ans_gen_i;
          if (lu_ans_hit_i) begin
            if (!r_required) begin
              // A record without REQUIRED wants the page RESIDENT, not DRAWN.
              // It is already resident, so there is nothing at all to do.
              c_pfr <= c_pfr + 32'd1;
            end else if (r_dynamic && !cslot_free) begin
              // T6: "If more than 256 REQUIRED dynamic patches remain after
              // legal degradation: fault the frame, drain, repeat the previous
              // complete frame, record rejected source IDs and keys."
              //
              // The degradation ladder is SW.STREAM's and has already run by
              // the time a list is sealed; architecture 2.6 forbids this block
              // inventing degrade policy. So an overflow here IS the ruling's
              // case and nothing else.
              fault_q <= 1'b1;
              f_src_q <= r_src_q;
              f_isl_q <= r_island_q;
              f_ix_q  <= r_ix_q;
              f_iz_q  <= r_iz_q;
              c_flt   <= c_flt + 32'd1;
            end else if (r_dynamic) begin
              cs_valid_q   <= 1'b1;
              cs_slot_q    <= cslot_next_q[CSLOTW-1:0];
              cslot_next_q <= cslot_next_q + {{CSLOTW{1'b0}}, 1'b1};
              c_cs         <= c_cs + 32'd1;
            end
            // A required STATIC patch falls through with cs_valid_q low: T6's
            // "static/baked visible pages render from resident page layers and
            // consume no dynamic slot."
          end else begin
            // Only a REQUIRED miss is a SKIP. A prefetch record that missed is
            // a prefetch record doing its job, and counting it here would bury
            // "ground the player should be seeing is absent" underneath "the
            // streamer is streaming".
            if (r_required) c_skip <= c_skip + 32'd1;
            // T7's ceiling, checked BEFORE the claim: claiming and then
            // declining to load leaves a slot in LOADING that nobody fills.
            // NOT a fault -- proxy-and-continue, recorded.
            if (loads_q >= cfg_load_budget_i) c_lddef <= c_lddef + 32'd1;
          end
        end

        S_PIN: if (pin_ready_i) c_pin <= c_pin + 32'd1;

        S_ISSUE: if (is_ready_i) c_iss <= c_iss + 32'd1;

        S_CLAIM: if (cl_ready_i) begin
          c_clm <= c_clm + 32'd1;
          seq_q <= seq_q + 32'd1;
        end

        S_WAIT_CL: if (cl_ans_valid_i) begin
          a_slot_q      <= cl_ans_slot_i;
          a_gen_q       <= cl_ans_gen_i;
          a_ev_island_q <= cl_ans_ev_island_i;
          a_ev_ix_q     <= cl_ans_ev_ix_i;
          a_ev_iz_q     <= cl_ans_ev_iz_i;
          a_ev_gen_q    <= cl_ans_ev_gen_i;
          // T9 rule 5: "all pinned: backpressure and count."
          if (cl_ans_refused_i) c_clr <= c_clr + 32'd1;
          // The lookup said miss and the claim said already present. That is
          // the directory disagreeing with itself across two transactions; it
          // is counted rather than smoothed over, and the load still goes out
          // because a slot that did not answer a lookup is not a slot this
          // frame may draw from.
          if (cl_ans_same_i && !cl_ans_refused_i) c_cls <= c_cls + 32'd1;
        end

        // T4: layer F has no canonical HPS mirror, so the victim's sheet must
        // reach the journal, and T10 forbids reusing a dirty_F slot before the
        // writeback ACK. The job goes out BEFORE the load for the same slot;
        // that ordering IS the barrier this block is responsible for.
        S_WB: if (wb_ready_i) c_wb <= c_wb + 32'd1;

        S_LOAD: if (ld_ready_i) begin
          c_ld    <= c_ld + 32'd1;
          loads_q <= loads_q + 16'd1;
        end

        default: ;
      endcase
    end
  end

endmodule
