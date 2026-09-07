// zhao_terrain_residency_v2.sv — the terrain page directory, set-associative.
//
// ---------------------------------------------------------------------------
// WHY THERE IS A V2, AND WHY THE V1 FILE IS STILL THERE
// ---------------------------------------------------------------------------
// `zhao_terrain_residency.sv` is a 1,024-slot DIRECT MAP keyed on `{px, py}`.
// It works, it has a real hazard fix in it, and its randomized lane found that
// fix. Owner ruling T9/T10 (2026-09-02) nonetheless **rejects it as
// production**, for two reasons that are worth keeping visible:
//
//   1. **The key is wrong.** T1 makes the canonical key
//      `{resource_epoch, island_id, patch_ix, patch_iz}`, and states that TWO
//      ISLANDS MAY LEGALLY OVERLAP IN LOCAL PATCH COORDINATES. A directory
//      keyed on `{px, py}` alone cannot tell them apart: island B's ground
//      answers a lookup for island A's, and the picture is wrong in a way no
//      single frame reveals.
//
//   2. **The mapping is wrong.** A direct map on the low bits of each axis has
//      a 2,048 m period. Two patches collide only when they differ by 32 in
//      BOTH axes -- which sounds rare and is exactly the traversal an 8 km
//      world does. T9: "determinism does not require direct mapping. A
//      deterministic set-associative cache is deterministic under a canonical
//      request order and avoids the 2,048-m periodic thrash."
//
// This file is built BESIDE the prototype, as T10 directs. The prototype is
// not integrated and not deleted: it is the thing this is measured against.
//
// ---------------------------------------------------------------------------
// THE SHAPE
// ---------------------------------------------------------------------------
//   256 sets x 4 ways = 1,024 slots            (T9)
//   set index = CRC-8/ATM(poly 0x07, init 0) over the little-endian bytes of
//               {island_id, patch_ix, patch_iz}, with resource_epoch[7:0]
//               xored into the final byte                                (T9)
//   handle    = {resource_epoch:u32, slot:u10, generation:u8}           (T10)
//
// **The full key is stored in every way.** A set index is a hash; a hash
// collision is normal; the key is what answers.
//
// ---------------------------------------------------------------------------
// ONE READING OF THE RULING IS AN INTERPRETATION, AND IT IS FLAGGED
// ---------------------------------------------------------------------------
// T9 says "xor `resource_epoch[7:0]` into the final byte". Two readings: the
// final byte of the MESSAGE (before CRC), or the CRC's single output byte
// (after). This file takes the first -- `SET_EPOCH_IN_MESSAGE = 1` -- because
// the sentence describes the message's bytes and then says "the final byte" of
// them. The other reading is one parameter away and the directed suite covers
// both, so if the owner meant the output byte it is a one-line change and not
// a rewrite. It is called out because a silently chosen hash is the kind of
// decision that becomes unquestionable by being invisible.
//
// ---------------------------------------------------------------------------
// SYNCHRONOUS METADATA, AND THE RESET SWEEP
// ---------------------------------------------------------------------------
// T10: "use synchronous metadata RAM banks, one per way, with registered
// lookup/capture. On power reset perform a 1,024-entry init sweep while ready
// is low -- do not async-reset an inferred RAM."
//
// That is why this block has a `ready` at all, and why it is low for 256
// clocks after reset. The v1 file resets 1,024 slots of flops in one clock,
// which is correct in simulation and is a 1,024-entry asynchronous reset fan-
// out that no memory block can implement. A directory that infers registers
// instead of M10K is a directory that will not fit.
//
// ---------------------------------------------------------------------------
// MUTATION IS SERIALIZED, WITH AN EXPLICIT PRIORITY
// ---------------------------------------------------------------------------
// T10 requires that "all mutation events [are] serialized or with explicit
// atomic priority", and that "same-cycle old FIN/DIRTY/UNPIN can never modify
// a newly reserved occupant".
//
// Both are met the same way: every mutation goes through ONE port, in a fixed
// priority, and every event except a claim carries a HANDLE that is checked
// against the stored `{epoch, generation}` before it is allowed to write. A
// stale event does not lose a race -- it is rejected on identity and counted.
//
//     priority 0  writeback ACK   (unblocks EVICT_PENDING; nothing may pass it)
//     priority 1  claim           (the only event that advances a generation)
//     priority 2  loader finish   (success or failure)
//     priority 3  dirty mark
//     priority 4  unpin
//     priority 5  pin
//
// A rejected-for-priority event is BACKPRESSURED, never dropped: each input has
// its own `ready`. Dropping a pin would leak a reference; dropping an unpin
// would pin a slot forever.
// ---------------------------------------------------------------------------
`default_nettype none

module zhao_terrain_residency_v2 #(
    parameter int unsigned SETS  = 256,
    parameter int unsigned WAYS  = 4,
    parameter int unsigned GENW  = 8,     // T10: "generation u8 minimum"
    parameter int unsigned PINW  = 6,     // 63 concurrent pins on one page
    parameter int unsigned SEQW  = 16,    // loader sequence
    // See the interpretation note above.
    parameter bit          SET_EPOCH_IN_MESSAGE = 1'b1
) (
    input var logic clk,
    input var logic rst_n,

    // Low during the post-reset init sweep. Nothing may be presented until it
    // rises; the sweep is what replaces an asynchronous reset of the RAMs.
    output var logic                      ready_o,

    // ---- lookup ------------------------------------------------------------
    // Hits ONLY on RESIDENT_CLEAN or RESIDENT_DIRTY_F (T10). A page that is
    // RESERVED, LOADING or in MIPGEN is not ground yet, and answering with it
    // composes an unwritten height lattice.
    input  var logic                      lu_valid_i,
    // See the assignment for why a QUERY has a ready: a caller that pulses for
    // one cycle and waits cannot tolerate losing the arbitration.
    output var logic                      lu_ready_o,
    input  var logic [31:0]               lu_epoch_i,
    input  var logic [31:0]               lu_island_i,
    input  var logic signed [15:0]        lu_ix_i,
    input  var logic signed [15:0]        lu_iz_i,
    output var logic                      lu_valid_o,
    output var logic                      lu_hit_o,
    output var logic [$clog2(SETS*WAYS)-1:0] lu_slot_o,
    output var logic [GENW-1:0]           lu_gen_o,

    // ---- claim -------------------------------------------------------------
    input  var logic                      cl_valid_i,
    output var logic                      cl_ready_o,
    input  var logic [31:0]               cl_epoch_i,
    input  var logic [31:0]               cl_island_i,
    input  var logic signed [15:0]        cl_ix_i,
    input  var logic signed [15:0]        cl_iz_i,
    input  var logic [31:0]               cl_expect_crc_i,
    input  var logic [SEQW-1:0]           cl_seq_i,

    output var logic                      cl_valid_o,
    output var logic                      cl_same_o,       // already present: no new generation
    output var logic                      cl_refused_o,    // every way pinned (T9 rule 5)
    output var logic [$clog2(SETS*WAYS)-1:0] cl_slot_o,
    output var logic [GENW-1:0]           cl_gen_o,
    // The victim, named so it can be written back BEFORE the loader overwrites
    // it. A dirty_F victim enters EVICT_PENDING and the slot is NOT reusable
    // until `wb_ack` arrives.
    output var logic                      cl_evicted_o,
    output var logic                      cl_evicted_dirty_o,
    output var logic [31:0]               cl_evicted_island_o,
    output var logic signed [15:0]        cl_evicted_ix_o,
    output var logic signed [15:0]        cl_evicted_iz_o,
    output var logic [GENW-1:0]           cl_evicted_gen_o,

    // ---- loader completion -------------------------------------------------
    // Carries success/failure and the CRC it actually saw (T10). A CRC that
    // does not match what the claim declared is a FAULTED page, not a resident
    // one: "a half-loaded or CRC-failed page is never rendered" (T7).
    input  var logic                      fin_valid_i,
    output var logic                      fin_ready_o,
    input  var logic [$clog2(SETS*WAYS)-1:0] fin_slot_i,
    input  var logic [GENW-1:0]           fin_gen_i,
    input  var logic [31:0]               fin_epoch_i,
    input  var logic                      fin_ok_i,
    input  var logic [31:0]               fin_crc_i,

    // ---- deformation marks a page dirty ------------------------------------
    // Three flags, not one (T4): `modified_BD` is a counter only and never
    // written back, `dirty_F` is a writeback BARRIER, `mips_stale` is a
    // regeneration barrier. A single generic dirty bit is insufficient.
    input  var logic                      dm_valid_i,
    output var logic                      dm_ready_o,
    input  var logic [$clog2(SETS*WAYS)-1:0] dm_slot_i,
    input  var logic [GENW-1:0]           dm_gen_i,
    input  var logic [31:0]               dm_epoch_i,
    input  var logic                      dm_bd_i,
    input  var logic                      dm_f_i,
    input  var logic                      dm_mips_i,

    // ---- pin / unpin -------------------------------------------------------
    input  var logic                      pin_valid_i,
    output var logic                      pin_ready_o,
    input  var logic [$clog2(SETS*WAYS)-1:0] pin_slot_i,
    input  var logic [GENW-1:0]           pin_gen_i,
    input  var logic [31:0]               pin_epoch_i,

    input  var logic                      unpin_valid_i,
    output var logic                      unpin_ready_o,
    input  var logic [$clog2(SETS*WAYS)-1:0] unpin_slot_i,
    input  var logic [GENW-1:0]           unpin_gen_i,
    input  var logic [31:0]               unpin_epoch_i,

    // ---- writeback acknowledgement -----------------------------------------
    // The F-sheet journal has taken the scars. Only now may the slot load.
    input  var logic                      wb_valid_i,
    output var logic                      wb_ready_o,
    input  var logic [$clog2(SETS*WAYS)-1:0] wb_slot_i,
    input  var logic [GENW-1:0]           wb_gen_i,
    input  var logic [31:0]               wb_epoch_i,

    // ---- handle check ------------------------------------------------------
    input  var logic                      chk_valid_i,
    input  var logic [$clog2(SETS*WAYS)-1:0] chk_slot_i,
    input  var logic [GENW-1:0]           chk_gen_i,
    input  var logic [31:0]               chk_epoch_i,
    output var logic                      chk_valid_o,
    output var logic                      chk_stale_o,

    // ---- evidence ----------------------------------------------------------
    output var logic [31:0]               hits_o,
    output var logic [31:0]               misses_o,
    output var logic [31:0]               claims_o,
    output var logic [31:0]               evictions_o,
    output var logic [31:0]               dirty_evictions_o,
    output var logic [31:0]               refused_all_pinned_o,
    output var logic [31:0]               stale_events_o,
    output var logic [31:0]               crc_failures_o,
    output var logic [31:0]               resident_o
);

  localparam int unsigned SETW  = $clog2(SETS);
  localparam int unsigned WAYW  = $clog2(WAYS);
  localparam int unsigned SLOTW = SETW + WAYW;

  // ---- states, exactly the eight T10 names ---------------------------------
  localparam logic [2:0] ST_INVALID          = 3'd0;
  localparam logic [2:0] ST_RESERVED         = 3'd1;
  localparam logic [2:0] ST_EVICT_PENDING    = 3'd2;
  localparam logic [2:0] ST_LOADING          = 3'd3;
  localparam logic [2:0] ST_MIPGEN           = 3'd4;
  localparam logic [2:0] ST_RESIDENT_CLEAN   = 3'd5;
  localparam logic [2:0] ST_RESIDENT_DIRTY_F = 3'd6;
  localparam logic [2:0] ST_FAULTED          = 3'd7;

  // =========================================================================
  // THE SET INDEX
  // =========================================================================
  // CRC-8/ATM: polynomial 0x07, initial 0, MSB-first, no reflection, no final
  // xor. Written as the byte-at-a-time law rather than a table so it reads as
  // the standard it names.
  function automatic logic [7:0] crc8_byte(input logic [7:0] crc,
                                           input logic [7:0] data);
    logic [7:0] c;
    begin
      c = crc ^ data;
      for (int unsigned b = 0; b < 8; b++)
        c = c[7] ? ((c << 1) ^ 8'h07) : (c << 1);
      crc8_byte = c;
    end
  endfunction

  /* verilator lint_off UNUSEDSIGNAL */
  function automatic logic [SETW-1:0] set_of(input logic [31:0] island,
                                             input logic signed [15:0] ix,
                                             input logic signed [15:0] iz,
                                             input logic [31:0] epoch);
    logic [7:0] c;
    logic [7:0] msg [8];
    begin
      // little-endian bytes of {island_id, patch_ix, patch_iz}
      msg[0] = island[7:0];   msg[1] = island[15:8];
      msg[2] = island[23:16]; msg[3] = island[31:24];
      msg[4] = ix[7:0];       msg[5] = ix[15:8];
      msg[6] = iz[7:0];       msg[7] = iz[15:8];
      if (SET_EPOCH_IN_MESSAGE) msg[7] = msg[7] ^ epoch[7:0];
      c = 8'h00;
      for (int unsigned i = 0; i < 8; i++) c = crc8_byte(c, msg[i]);
      if (!SET_EPOCH_IN_MESSAGE) c = c ^ epoch[7:0];
      set_of = c[SETW-1:0];
    end
  endfunction
  /* verilator lint_on UNUSEDSIGNAL */

  // =========================================================================
  // METADATA
  // =========================================================================
  // One synchronous bank per way (T10). Split in two so a claim's key write
  // and an event's flag write do not contend for the same port on the common
  // path -- the arbitration below still serialises them, but the split keeps
  // each bank narrow enough to infer sensibly.
  //
  //   KEY bank : island(32) ix(16) iz(16) epoch(32) gen(GENW) state(3)
  //   STAT bank: pin(PINW) modified_BD dirty_F mips_stale crc(32) seq(SEQW)
  localparam int unsigned KEYW  = 32 + 16 + 16 + 32 + GENW + 3;
  localparam int unsigned STATW = PINW + 3 + 32 + SEQW;

  // ONE FLAT ARRAY PER WAY, INSIDE A GENERATE. These were `[WAYS][SETS]`, and
  // that alone kept all 167,936 bits in flip-flops: Quartus says so in as many
  // words -- "EDA Netlist Writer cannot regroup multidimensional array" -- and
  // emits no "Inferred RAM" line at all. It muxes across the outer dimension
  // because the outer selection is dynamic, and the whole array falls into
  // flops however correct the writes are. The outer index has to be a genvar.
  //
  // At 167,936 bits this one pair of arrays was 60% of the entire production
  // overage measured in reports/RAM-INFERENCE-RANKED-20260904.md.
  genvar gw;
  generate
    for (gw = 0; gw < int'(WAYS); gw++) begin : g_bank
      logic [KEYW-1:0]  keyram  [SETS];
      logic [STATW-1:0] statram [SETS];
    end
  endgenerate

  // registered read data, one clock after the address
  logic [KEYW-1:0]  key_q  [WAYS];
  logic [STATW-1:0] stat_q [WAYS];

  // ---- the write port, decided combinationally -----------------------------
  // The three write addresses the checker flagged -- `[w][sweep_q]`,
  // `[victim_c][s0_set]` and `[ev_way_c][s0_set]` -- are MUTUALLY EXCLUSIVE
  // per way, which is not obvious from the list and is what makes this a
  // single-port memory after all:
  //
  //   * the sweep is the `if (sweeping_q)` arm and the pipeline is the `else`,
  //     so they cannot both write in a cycle;
  //   * a claim and an event are different arms of the same `unique case`;
  //   * and a claim and an event write the SAME address, `s0_set`.
  //
  // So per way there is one address and one enable. What looked like a design
  // question -- "it asks for a memory shape the device does not have" -- was a
  // consequence of the [WAYS][SETS] shape hiding the per-way view.
  //
  // READ-DURING-WRITE at one address never happens, and that is load-bearing
  // rather than lucky: `hazard_c` already blocks an access whose `addr_c`
  // equals `s0_set` for exactly the events that write. In flip-flops this
  // decided nothing (a non-blocking read is always the old value); in an M10K
  // mixed-port read-during-write is device behaviour, so the guard is now what
  // makes the conversion sound.
  logic [SETW-1:0]  wr_set_c;
  logic [WAYS-1:0]  kwe_c, swe_c;
  logic [KEYW-1:0]  kwd_c;
  logic [STATW-1:0] swd_c;

  generate
    for (gw = 0; gw < int'(WAYS); gw++) begin : g_port
      always_ff @(posedge clk) begin
        // Read every cycle, no read enable: an unconditional read is the shape
        // that infers most cleanly, and during the sweep the values it lands
        // are never consulted.
        //
        // That is provable rather than observed. Reset sets `sweeping_q <= 1`
        // and `s0_v <= 0`; `s0_v` is assigned ONLY inside the non-sweep arm of
        // the process below, and `sweeping_q` clears only on the sweep's last
        // set. So `s0_v` holds 0 for every cycle of the sweep, and stage 1 --
        // the only reader of key_q/stat_q -- is gated on `s0_v`. The garbage
        // read at whatever `addr_c` happens to be during the sweep cannot
        // reach an output.
        //
        // The old code read only inside that same arm, so this is a change in
        // when the registers are WRITTEN and not in what is ever observed.
        key_q[gw]  <= g_bank[gw].keyram [addr_c];
        stat_q[gw] <= g_bank[gw].statram[addr_c];
        if (kwe_c[gw]) g_bank[gw].keyram [wr_set_c] <= kwd_c;
        if (swe_c[gw]) g_bank[gw].statram[wr_set_c] <= swd_c;
      end
    end
  endgenerate

  // field accessors, so no bit range is written twice. Each reads ONE field,
  // so Verilator correctly observes that the other bits of its argument are
  // unused; that is the point of an accessor, not a defect.
  /* verilator lint_off UNUSEDSIGNAL */
  function automatic logic [31:0]        k_island(input logic [KEYW-1:0] k); k_island = k[KEYW-1 -: 32];                     endfunction
  function automatic logic signed [15:0] k_ix    (input logic [KEYW-1:0] k); k_ix     = k[KEYW-33 -: 16];                    endfunction
  function automatic logic signed [15:0] k_iz    (input logic [KEYW-1:0] k); k_iz     = k[KEYW-49 -: 16];                    endfunction
  function automatic logic [31:0]        k_epoch (input logic [KEYW-1:0] k); k_epoch  = k[KEYW-65 -: 32];                    endfunction
  function automatic logic [GENW-1:0]    k_gen   (input logic [KEYW-1:0] k); k_gen    = k[GENW+2 -: GENW];                   endfunction
  function automatic logic [2:0]         k_state (input logic [KEYW-1:0] k); k_state  = k[2:0];                              endfunction

  function automatic logic [KEYW-1:0] k_pack(input logic [31:0] island,
                                             input logic signed [15:0] ix,
                                             input logic signed [15:0] iz,
                                             input logic [31:0] epoch,
                                             input logic [GENW-1:0] gen,
                                             input logic [2:0] state);
    k_pack = {island, ix, iz, epoch, gen, state};
  endfunction

  function automatic logic [PINW-1:0] s_pin  (input logic [STATW-1:0] s); s_pin   = s[STATW-1 -: PINW];        endfunction
  function automatic logic            s_bd   (input logic [STATW-1:0] s); s_bd    = s[STATW-PINW-1];           endfunction
  function automatic logic            s_f    (input logic [STATW-1:0] s); s_f     = s[STATW-PINW-2];           endfunction
  function automatic logic            s_mips (input logic [STATW-1:0] s); s_mips  = s[STATW-PINW-3];           endfunction
  function automatic logic [31:0]     s_crc  (input logic [STATW-1:0] s); s_crc   = s[SEQW+31 -: 32];          endfunction
  function automatic logic [SEQW-1:0] s_seq  (input logic [STATW-1:0] s); s_seq   = s[SEQW-1:0];               endfunction

  function automatic logic [STATW-1:0] s_pack(input logic [PINW-1:0] pin,
                                              input logic bd, input logic f,
                                              input logic mips,
                                              input logic [31:0] crc,
                                              input logic [SEQW-1:0] seq);
    s_pack = {pin, bd, f, mips, crc, seq};
  endfunction

  /* verilator lint_on UNUSEDSIGNAL */

  // per-set round-robin victim pointer (T9 replacement rule 3/4)
  logic [WAYW-1:0] rr_q [SETS];

  // =========================================================================
  // RESET SWEEP — 256 clocks, ready low, no asynchronous RAM reset
  // =========================================================================
  logic            sweeping_q;
  logic [SETW-1:0] sweep_q;
  assign ready_o = !sweeping_q;

  // =========================================================================
  // EVENT ARBITRATION — one mutation per clock, fixed priority
  // =========================================================================
  typedef enum logic [2:0] {
    EV_NONE = 3'd0, EV_WB = 3'd1, EV_CLAIM = 3'd2, EV_FIN = 3'd3,
    EV_DIRTY = 3'd4, EV_UNPIN = 3'd5, EV_PIN = 3'd6
  } ev_e;

  ev_e ev_c;
  always_comb begin
    if      (wb_valid_i)    ev_c = EV_WB;
    else if (cl_valid_i)    ev_c = EV_CLAIM;
    else if (fin_valid_i)   ev_c = EV_FIN;
    else if (dm_valid_i)    ev_c = EV_DIRTY;
    else if (unpin_valid_i) ev_c = EV_UNPIN;
    else if (pin_valid_i)   ev_c = EV_PIN;
    else                    ev_c = EV_NONE;
  end

  // Each input is backpressured rather than dropped. A dropped pin leaks a
  // reference; a dropped unpin pins a page forever.
  assign wb_ready_o    = ready_o && !hazard_c && (ev_c == EV_WB);
  assign cl_ready_o    = ready_o && !hazard_c && (ev_c == EV_CLAIM);
  assign fin_ready_o   = ready_o && !hazard_c && (ev_c == EV_FIN);
  assign dm_ready_o    = ready_o && !hazard_c && (ev_c == EV_DIRTY);
  assign unpin_ready_o = ready_o && !hazard_c && (ev_c == EV_UNPIN);

  // A LOOKUP GETS A READY TOO, because the assumption below it turned out to
  // be false of a caller written later.
  //
  // The note above says a losing query "is simply not answered this clock" and
  // that "both callers already tolerate that -- they are queries, not
  // transactions". TERRAIN.SEQ does not tolerate it: it asserts lu_valid_o for
  // exactly one cycle (`assign lu_valid_o = (st == S_LOOKUP)`) and then moves
  // unconditionally to S_WAIT_LU to wait for an answer that will never come.
  // The composed terrain test injected ONE mutation on the offer cycle and the
  // frame never completed -- silently, with err_stray_ans_o low, because
  // neither block believed anything had gone wrong.
  //
  // This is exactly the shape of defect the repository keeps meeting: a
  // documented assumption about callers, correct when written, quietly
  // violated by the next caller. The fix is to stop assuming and say it on a
  // wire, in the same form the mutation ports already use.
  //
  // Identical condition to `s0_is_lookup`'s accept below, written once so the
  // two cannot drift.
  assign lu_ready_o    = ready_o && !hazard_c && (ev_c == EV_NONE);
  assign pin_ready_o   = ready_o && !hazard_c && (ev_c == EV_PIN);

  // ---- stage 0: address the banks -----------------------------------------
  // ONE ADDRESS PORT. The banks are single-ported synchronous RAM because T10
  // says they are, so a mutation, a lookup and a handle check compete for one
  // address each clock: mutation, then lookup, then check.
  //
  // A query that loses is simply not answered this clock (`lu_valid_o` /
  // `chk_valid_o` stay low). Both callers already tolerate that -- they are
  // queries, not transactions. A MUTATION that loses is BACKPRESSURED, which
  // is not the same thing, and is why the mutation ports have `ready`.
  //
  // The handle check used to read `keyram` combinationally. That reads well
  // and is a lie about the storage: an asynchronous read of 1,024 entries does
  // not infer as memory, it infers as registers and a 1,024-way mux, which is
  // exactly what the synchronous-bank ruling exists to prevent. It goes
  // through the pipeline like everything else.
  logic            s0_v;
  logic            s0_is_lookup;
  logic            s0_is_check;
  ev_e             s0_ev;
  logic [SETW-1:0] s0_set;
  logic [31:0]     s0_island, s0_epoch, s0_crc;
  logic signed [15:0] s0_ix, s0_iz;
  logic [SLOTW-1:0]   s0_slot;
  logic [GENW-1:0]    s0_gen;
  logic [SLOTW-1:0]   s0_chk_slot;
  logic [GENW-1:0]    s0_chk_gen;
  logic [31:0]        s0_chk_epoch;
  logic [SEQW-1:0]    s0_seq;
  logic               s0_ok, s0_bd, s0_f, s0_mips;

  logic [SETW-1:0] addr_c;
  always_comb begin
    if (ev_c == EV_CLAIM)      addr_c = set_of(cl_island_i, cl_ix_i, cl_iz_i, cl_epoch_i);
    else if (ev_c == EV_WB)    addr_c = wb_slot_i[SLOTW-1 -: SETW];
    else if (ev_c == EV_FIN)   addr_c = fin_slot_i[SLOTW-1 -: SETW];
    else if (ev_c == EV_DIRTY) addr_c = dm_slot_i[SLOTW-1 -: SETW];
    else if (ev_c == EV_UNPIN) addr_c = unpin_slot_i[SLOTW-1 -: SETW];
    else if (ev_c == EV_PIN)   addr_c = pin_slot_i[SLOTW-1 -: SETW];
    else if (lu_valid_i)       addr_c = set_of(lu_island_i, lu_ix_i, lu_iz_i, lu_epoch_i);
    else if (chk_valid_i)      addr_c = chk_slot_i[SLOTW-1 -: SETW];
    else                       addr_c = '0;
  end

  // ---- THE SAME-SET READ-DURING-WRITE HAZARD ------------------------------
  // Stage 1 writes a bank at `s0_set` on the same clock stage 0 reads at
  // `addr_c`. If they are the same set, the read returns the value BEFORE the
  // write, and the next decision is made against stale metadata -- a claim
  // could pick a victim the previous claim just took.
  //
  // Two ways out: bypass the write into the read, or refuse to accept for one
  // clock. This takes the second. A bypass is four more comparators and four
  // more wide muxes on the block's widest path; the stall costs one clock, and
  // only when two consecutive events land in the same set out of 256. This
  // directory answers a few hundred claims a frame, not one a clock.
  logic hazard_c;
  assign hazard_c = s0_v && !s0_is_lookup && !s0_is_check && (addr_c == s0_set);

  // way named by a slot handle. A slot IS {set, way}; this reads the way half,
  // so the set half is deliberately unused here.
  /* verilator lint_off UNUSEDSIGNAL */
  function automatic logic [WAYW-1:0] way_of(input logic [SLOTW-1:0] s);
    way_of = s[WAYW-1:0];
  endfunction
  /* verilator lint_on UNUSEDSIGNAL */

  // =========================================================================
  // THE BANK WRITE DECISION
  // =========================================================================
  // This mirrors the arm structure of the sequential block below, one arm for
  // one arm, and produces {address, per-way enable, data} instead of writing
  // the arrays directly. It has to be combinational: the writes it replaces
  // were non-blocking assignments in that block, so registering the intent
  // here would add a clock and change every latency the tests pin.
  //
  // IT IS A SECOND COPY OF THOSE CONDITIONS, and that is the cost of the
  // conversion. It is paid deliberately: the alternative is 167,936 bits in
  // flip-flops, which is 60% of the console's overage. The guard is the pair
  // of tests -- 37 directed checks and a randomized run against the model --
  // captured before this change and required to be identical after it. If a
  // condition here ever drifts from the one below, a page loads into the wrong
  // slot and those tests are what say so.
  always_comb begin
    wr_set_c = sweep_q;
    kwe_c    = '0;
    swe_c    = '0;
    kwd_c    = k_pack('0, '0, '0, '0, '0, ST_INVALID);
    swd_c    = s_pack('0, 1'b0, 1'b0, 1'b0, '0, '0);

    if (sweeping_q) begin
      // Every way, one set per clock: 256 clocks of ordinary synchronous
      // writes instead of a 1,024-entry asynchronous reset.
      kwe_c = {WAYS{1'b1}};
      swe_c = {WAYS{1'b1}};
    end else if (s0_v) begin
      wr_set_c = s0_set;
      if (s0_is_lookup || s0_is_check) begin
        // A lookup and a handle check are questions, not edits.
      end else if (s0_ev == EV_CLAIM) begin
        // T9 rule 1 (a matching key) and rule 5 (all ways pinned) both answer
        // without touching the banks.
        if (!any_hit_c && victim_found_c) begin
          kwe_c[victim_c] = 1'b1;
          swe_c[victim_c] = 1'b1;
          kwd_c = k_pack(s0_island, s0_ix, s0_iz, s0_epoch,
                         k_gen(key_q[victim_c]) + GENW'(1),
                         victim_dirty_c ? ST_EVICT_PENDING : ST_RESERVED);
          swd_c = s_pack('0, 1'b0, victim_dirty_c, 1'b1, s0_crc, s0_seq);
        end
      end else begin
        automatic logic [KEYW-1:0]  k = key_q[ev_way_c];
        automatic logic [STATW-1:0] s = stat_q[ev_way_c];
        // Identity first: a stale event is counted and changes nothing.
        if (ident_ok(k, s0_gen, s0_epoch)) begin
          unique case (s0_ev)
            EV_WB: begin
              kwe_c[ev_way_c] = 1'b1;
              swe_c[ev_way_c] = 1'b1;
              kwd_c = k_pack(k_island(k), k_ix(k), k_iz(k),
                             k_epoch(k), k_gen(k), ST_RESERVED);
              swd_c = s_pack(s_pin(s), s_bd(s), 1'b0,
                             s_mips(s), s_crc(s), s_seq(s));
            end
            EV_FIN: begin
              if (!s0_ok || s0_crc != s_crc(s)) begin
                kwe_c[ev_way_c] = 1'b1;
                kwd_c = k_pack(k_island(k), k_ix(k), k_iz(k),
                               k_epoch(k), k_gen(k), ST_FAULTED);
              end else if (k_state(k) == ST_RESERVED || k_state(k) == ST_LOADING) begin
                kwe_c[ev_way_c] = 1'b1;
                kwd_c = k_pack(k_island(k), k_ix(k), k_iz(k), k_epoch(k), k_gen(k),
                               s_mips(s) ? ST_MIPGEN : ST_RESIDENT_CLEAN);
              end else if (k_state(k) == ST_MIPGEN) begin
                kwe_c[ev_way_c] = 1'b1;
                swe_c[ev_way_c] = 1'b1;
                kwd_c = k_pack(k_island(k), k_ix(k), k_iz(k),
                               k_epoch(k), k_gen(k), ST_RESIDENT_CLEAN);
                swd_c = s_pack(s_pin(s), s_bd(s), s_f(s), 1'b0,
                               s_crc(s), s_seq(s));
              end else begin
                // A FIN IN ANY OTHER STATE WAS SILENTLY DISCARDED, and the
                // composed terrain test found what that costs: a page loaded
                // into a slot still in EVICT_PENDING passes the identity check
                // -- the key and generation are right -- so it is not counted
                // stale either. The load completed, the bytes are in the pool,
                // and the slot is held for ever by a key whose page will never
                // be called ground. Re-submitting the same key twice returned
                // resident=0 both times.
                //
                // COUNTED, NOT ADVANCED. Whether a load into an evicting slot
                // should WIN is a policy question -- it races the writeback
                // this directory itself ordered -- and inventing an answer
                // here would be inventing residency policy in the arm that
                // discards things. What is certainly wrong is silence, so the
                // event now reaches `stale_events_o` and a reader can see that
                // a completion arrived somewhere it could do nothing.
                // (counted in the sequential block below, where every other
                // event is counted, rather than through a second flag that
                // could disagree with this arm about what happened)
              end
            end
            EV_DIRTY: begin
              swe_c[ev_way_c] = 1'b1;
              swd_c = s_pack(s_pin(s), s_bd(s) | s0_bd, s_f(s) | s0_f,
                             s_mips(s) | s0_mips, s_crc(s), s_seq(s));
              if (s0_f && k_state(k) == ST_RESIDENT_CLEAN) begin
                kwe_c[ev_way_c] = 1'b1;
                kwd_c = k_pack(k_island(k), k_ix(k), k_iz(k),
                               k_epoch(k), k_gen(k), ST_RESIDENT_DIRTY_F);
              end
            end
            EV_PIN: begin
              if (s_pin(s) != {PINW{1'b1}}) begin
                swe_c[ev_way_c] = 1'b1;
                swd_c = s_pack(s_pin(s) + PINW'(1), s_bd(s),
                               s_f(s), s_mips(s), s_crc(s), s_seq(s));
              end
            end
            EV_UNPIN: begin
              if (s_pin(s) != '0) begin
                swe_c[ev_way_c] = 1'b1;
                swd_c = s_pack(s_pin(s) - PINW'(1), s_bd(s),
                               s_f(s), s_mips(s), s_crc(s), s_seq(s));
              end
            end
            default: ;
          endcase
        end
      end
    end
  end

  // =========================================================================
  // STAGE 1 — the banks answer, and the decision is made
  // =========================================================================
  logic [WAYS-1:0] hit_c;
  logic [WAYW-1:0] hit_way_c;
  logic            any_hit_c;

  always_comb begin
    hit_c     = '0;
    hit_way_c = '0;
    for (int unsigned w = 0; w < WAYS; w++) begin
      if (k_state(key_q[w]) != ST_INVALID
          && k_island(key_q[w]) == s0_island
          && k_ix(key_q[w])     == s0_ix
          && k_iz(key_q[w])     == s0_iz
          && k_epoch(key_q[w])  == s0_epoch)
        hit_c[w] = 1'b1;
    end
    for (int unsigned w = 0; w < WAYS; w++)
      if (hit_c[w]) hit_way_c = WAYW'(w);
    any_hit_c = |hit_c;
  end

  // ---- replacement, T9's five rules in order ------------------------------
  //   1 matching key
  //   2 invalid way
  //   3 clean unpinned way, per-set round robin
  //   4 dirty_F unpinned way, same order, entering EVICT_PENDING
  //   5 all pinned: backpressure and count
  logic [WAYW-1:0] victim_c;
  logic            victim_found_c;
  logic            victim_dirty_c;

  always_comb begin
    victim_c       = '0;
    victim_found_c = 1'b0;
    victim_dirty_c = 1'b0;

    // 2: an invalid way, lowest index first -- deterministic, not arbitrary
    for (int unsigned w = 0; w < WAYS; w++)
      if (!victim_found_c && k_state(key_q[w]) == ST_INVALID) begin
        victim_c = WAYW'(w); victim_found_c = 1'b1;
      end

    // 3: a clean unpinned way, starting at the set's round-robin pointer.
    //
    // The wrap is a compare-and-subtract, NOT `% WAYS`. `WAYW'(WAYS)` truncates
    // 4 to two bits and is ZERO, so the modulo was a divide by zero and every
    // walk returned way 0 -- which passed nine of eleven cases, because way 0
    // is very often the right answer. The one case that caught it was the one
    // asking whether a CLEAN way is taken before a dirty one.
    if (!victim_found_c)
      for (int unsigned i = 0; i < WAYS; i++) begin
        automatic logic [WAYW:0] sum = {1'b0, rr_q[s0_set]} + (WAYW+1)'(i);
        automatic logic [WAYW-1:0] w =
            WAYW'((sum >= (WAYW+1)'(WAYS)) ? (sum - (WAYW+1)'(WAYS)) : sum);
        if (!victim_found_c && s_pin(stat_q[w]) == '0 && !s_f(stat_q[w])
            && k_state(key_q[w]) != ST_EVICT_PENDING) begin
          victim_c = w; victim_found_c = 1'b1;
        end
      end

    // 4: a dirty_F unpinned way, same order -- it will enter EVICT_PENDING and
    //    the slot is NOT reusable until the journal acknowledges.
    if (!victim_found_c)
      for (int unsigned i = 0; i < WAYS; i++) begin
        automatic logic [WAYW:0] sum = {1'b0, rr_q[s0_set]} + (WAYW+1)'(i);
        automatic logic [WAYW-1:0] w =
            WAYW'((sum >= (WAYW+1)'(WAYS)) ? (sum - (WAYW+1)'(WAYS)) : sum);
        if (!victim_found_c && s_pin(stat_q[w]) == '0 && s_f(stat_q[w])) begin
          victim_c = w; victim_found_c = 1'b1; victim_dirty_c = 1'b1;
        end
      end
    // 5: nothing found -- every way pinned or already evicting. Refuse.
  end

  // The identity check every non-claim event must pass. This is what makes a
  // stale FIN/DIRTY/UNPIN arriving on the same clock as a new reservation
  // harmless: it is rejected on identity, not beaten on timing.
  function automatic logic ident_ok(input logic [KEYW-1:0] k,
                                    input logic [GENW-1:0] gen,
                                    input logic [31:0] epoch);
    ident_ok = (k_state(k) != ST_INVALID) && (k_gen(k) == gen) && (k_epoch(k) == epoch);
  endfunction

  // =========================================================================
  // OUTPUTS AND STATE
  // =========================================================================
  logic [WAYW-1:0] ev_way_c;
  assign ev_way_c = way_of(s0_slot);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      sweeping_q <= 1'b1;
      sweep_q    <= '0;
      s0_v       <= 1'b0;
      s0_ev      <= EV_NONE;
      s0_is_lookup <= 1'b0;
      s0_is_check  <= 1'b0;
      lu_valid_o <= 1'b0;
      cl_valid_o <= 1'b0;
      chk_valid_o<= 1'b0;
      hits_o <= '0; misses_o <= '0; claims_o <= '0; evictions_o <= '0;
      dirty_evictions_o <= '0; refused_all_pinned_o <= '0; stale_events_o <= '0;
      crc_failures_o <= '0; resident_o <= '0;
    end else begin
      lu_valid_o  <= 1'b0;
      cl_valid_o  <= 1'b0;
      chk_valid_o <= 1'b0;

      // ---- the init sweep ------------------------------------------------
      // 256 clocks of ordinary synchronous writes. No 1,024-entry asynchronous
      // reset, which is the thing that would refuse to infer as memory.
      if (sweeping_q) begin
        // The bank writes are in `wr_decide_c` below; only rr_q is here.
        rr_q[sweep_q] <= '0;
        if (sweep_q == SETW'(SETS - 1)) sweeping_q <= 1'b0;
        sweep_q <= sweep_q + SETW'(1);
      end else begin
        // ---- stage 0: capture, and read the banks ------------------------
        s0_v         <= !hazard_c && ((ev_c != EV_NONE) || lu_valid_i || chk_valid_i);
        s0_ev        <= ev_c;
        s0_is_lookup <= (ev_c == EV_NONE) && lu_valid_i;
        s0_is_check  <= (ev_c == EV_NONE) && !lu_valid_i && chk_valid_i;
        s0_set       <= addr_c;
        if (chk_valid_i) begin
          s0_chk_slot  <= chk_slot_i;
          s0_chk_gen   <= chk_gen_i;
          s0_chk_epoch <= chk_epoch_i;
        end

        unique case (ev_c)
          EV_CLAIM: begin
            s0_island <= cl_island_i; s0_ix <= cl_ix_i; s0_iz <= cl_iz_i;
            s0_epoch  <= cl_epoch_i;  s0_crc <= cl_expect_crc_i; s0_seq <= cl_seq_i;
          end
          EV_WB:    begin s0_slot <= wb_slot_i;    s0_gen <= wb_gen_i;    s0_epoch <= wb_epoch_i;    end
          EV_FIN:   begin s0_slot <= fin_slot_i;   s0_gen <= fin_gen_i;   s0_epoch <= fin_epoch_i;
                          s0_ok   <= fin_ok_i;     s0_crc <= fin_crc_i;                              end
          EV_DIRTY: begin s0_slot <= dm_slot_i;    s0_gen <= dm_gen_i;    s0_epoch <= dm_epoch_i;
                          s0_bd   <= dm_bd_i;      s0_f   <= dm_f_i;      s0_mips  <= dm_mips_i;     end
          EV_UNPIN: begin s0_slot <= unpin_slot_i; s0_gen <= unpin_gen_i; s0_epoch <= unpin_epoch_i; end
          EV_PIN:   begin s0_slot <= pin_slot_i;   s0_gen <= pin_gen_i;   s0_epoch <= pin_epoch_i;   end
          default:  begin
            s0_island <= lu_island_i; s0_ix <= lu_ix_i; s0_iz <= lu_iz_i;
            s0_epoch  <= lu_epoch_i;
          end
        endcase

        // ---- stage 1: decide and write ------------------------------------
        if (s0_v) begin
          if (s0_is_lookup) begin
            // A lookup hits ONLY on resident state (T10).
            automatic logic res = any_hit_c
                && ((k_state(key_q[hit_way_c]) == ST_RESIDENT_CLEAN)
                 || (k_state(key_q[hit_way_c]) == ST_RESIDENT_DIRTY_F));
            lu_valid_o <= 1'b1;
            lu_hit_o   <= res;
            lu_slot_o  <= {s0_set, hit_way_c};
            lu_gen_o   <= k_gen(key_q[hit_way_c]);
            if (res) hits_o <= hits_o + 32'd1;
            else     misses_o <= misses_o + 32'd1;
          end else if (s0_ev == EV_CLAIM) begin
            claims_o   <= claims_o + 32'd1;
            cl_valid_o <= 1'b1;
            cl_evicted_o       <= 1'b0;
            cl_evicted_dirty_o <= 1'b0;
            cl_refused_o       <= 1'b0;
            cl_same_o          <= 1'b0;

            if (any_hit_c) begin
              // T9 rule 1: a matching key. Re-claiming a page that is already
              // here must NOT advance the generation -- a visible-set rebuild
              // re-submits resident patches every frame, and advancing here
              // would tell every in-flight job it is stale, every frame.
              cl_same_o <= 1'b1;
              cl_slot_o <= {s0_set, hit_way_c};
              cl_gen_o  <= k_gen(key_q[hit_way_c]);
            end else if (!victim_found_c) begin
              // T9 rule 5.
              cl_refused_o <= 1'b1;
              refused_all_pinned_o <= refused_all_pinned_o + 32'd1;
            end else begin
              automatic logic [GENW-1:0] ng = k_gen(key_q[victim_c]) + GENW'(1);
              automatic logic live = (k_state(key_q[victim_c]) != ST_INVALID);
              cl_slot_o <= {s0_set, victim_c};
              cl_gen_o  <= ng;
              cl_evicted_o        <= live;
              cl_evicted_dirty_o  <= victim_dirty_c;
              cl_evicted_island_o <= k_island(key_q[victim_c]);
              cl_evicted_ix_o     <= k_ix(key_q[victim_c]);
              cl_evicted_iz_o     <= k_iz(key_q[victim_c]);
              cl_evicted_gen_o    <= k_gen(key_q[victim_c]);
              if (live) evictions_o <= evictions_o + 32'd1;
              if (victim_dirty_c) dirty_evictions_o <= dirty_evictions_o + 32'd1;

              // A dirty_F victim goes to EVICT_PENDING and does NOT load until
              // the journal acknowledges (T4's barrier). Anything else is
              // RESERVED and may load at once.
              // the bank writes for this arm are in `wr_decide_c`
              // Replacement state advances on CLAIM ACCEPTANCE, never on an
              // asynchronous fill completion (T9). That is what makes the
              // victim order a function of the canonical request order alone.
              rr_q[s0_set] <= (victim_c == WAYW'(WAYS-1)) ? '0 : victim_c + WAYW'(1);
            end
          end else if (s0_is_check) begin
            // A handle is stale exactly when the slot's stored identity has
            // moved on. Answered from the REGISTERED bank read, one clock
            // after the address, like every other question here.
            chk_valid_o <= 1'b1;
            chk_stale_o <= !ident_ok(key_q[way_of(s0_chk_slot)], s0_chk_gen, s0_chk_epoch);
          end else begin
            // ---- every other event: identity first --------------------------
            automatic logic [KEYW-1:0]  k = key_q[ev_way_c];
            automatic logic [STATW-1:0] s = stat_q[ev_way_c];
            if (!ident_ok(k, s0_gen, s0_epoch)) begin
              stale_events_o <= stale_events_o + 32'd1;
            end else if (s0_ev == EV_FIN && s0_ok && (s0_crc == s_crc(s)) &&
                         !(k_state(k) == ST_RESERVED || k_state(k) == ST_LOADING ||
                           k_state(k) == ST_MIPGEN)) begin
              // A GOOD COMPLETION THAT LANDED SOMEWHERE IT CAN DO NOTHING.
              // The identity is right, the CRC is right, and the state is not
              // one the FIN arm advances -- EVICT_PENDING is the case the
              // composed terrain test found. It used to fall through in
              // silence: not counted stale, because the key matched; not
              // acted on, because no arm claimed it. The load completed, the
              // bytes are in the pool, and the slot is held for ever by a key
              // whose page will never be called ground.
              //
              // Counted here rather than advanced: whether a load into an
              // evicting slot should WIN races the writeback this directory
              // itself ordered, and that is residency policy, not something to
              // invent in the arm that drops things.
              stale_events_o <= stale_events_o + 32'd1;
            end else begin
              unique case (s0_ev)
                EV_WB: begin
                  // The journal has the scars. Now the slot may load.
                end
                EV_FIN: begin
                  if (!s0_ok || s0_crc != s_crc(s)) begin
                    // "A half-loaded or CRC-failed page is never rendered."
                    crc_failures_o <= crc_failures_o + 32'd1;
                  end else if (k_state(k) == ST_RESERVED || k_state(k) == ST_LOADING) begin
                    // Mips are still stale, so the page is not ground yet.
                    if (!s_mips(s)) resident_o <= resident_o + 32'd1;
                  end else if (k_state(k) == ST_MIPGEN) begin
                    resident_o <= resident_o + 32'd1;
                  end
                end
                EV_DIRTY: begin
                  // Three separate flags. `modified_BD` never causes a
                  // writeback; `dirty_F` is the barrier; `mips_stale` is the
                  // regeneration barrier.
                end
                EV_PIN: begin
                end
                EV_UNPIN: begin
                end
                default: ;
              endcase
            end
          end
        end

      end
    end
  end

endmodule : zhao_terrain_residency_v2

`default_nettype wire
