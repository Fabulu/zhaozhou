// zhao_terrain_writeback.sv - one dirty page's LAYER F, local SDRAM -> the HPS
// terrain journal, behind an acknowledgement barrier.
//
// ---------------------------------------------------------------------------
// IT EVACUATES A SHEET. IT DOES NOT FREE A SLOT.
// ---------------------------------------------------------------------------
// Step 6 of reports/TERRAIN_WORLD_LAYER_ARCHITECTURE.md sec 5, and the mirror
// image of TERRAIN.PAGELOADER running the other way. Everything it does is
// written down elsewhere:
//
//   * ruling T4 -- B and D are NEVER written back (the HPS keeps the canonical
//     mirror current from the same deterministic commands); layer F has no
//     canonical mirror, so it MUST be, and "wait for journal acknowledgement
//     before the slot may enter LOADING";
//   * layer F is 64x64 x {tag u8, strength u8} = 8,192 B and lives INSIDE the
//     21,376-byte page (spec/terrain_rules.md sec 2 table; T2: "no separate
//     permanent E/F/H pools");
//   * the 64-byte header restates {island_id, patch_ix, patch_iz} and
//     "redundancy is a corruption check" (spec/terrain_rules.md sec 2.1);
//   * the source region is TERRAIN.PAGE_POOL at 0x0400_0000, 1,024 x 21,376 B
//     (ruling T2 / spec/memory_rules.md sec 5b);
//   * every local-SDRAM access goes through MEM.GUARD (spec/memory_rules.md
//     sec 5); bursts ride MEM.HPS.BRIDGE's frozen port (its contract);
//   * "no dirty_F reuse before the writeback ACK" (T10, and
//     design/contracts/TERRAIN.RESIDENCY.md calls it the most important rule in
//     that file).
//
// WHY B AND D ARE ABSENT. They are not forgotten. T4 rules that the HPS owns
// them and keeps them current, so writing them back here would create a SECOND
// writer of a structure that already has one -- the law MEM.HPS.BRIDGE states
// as "no shared mutable structure ever has two writers" -- and would let an
// eviction CORRUPT the canonical copy rather than merely fail to save it.
//
// ---------------------------------------------------------------------------
// THE BARRIER IS THE BLOCK
// ---------------------------------------------------------------------------
// `wb_valid_o` is the directory's permission to reuse the slot, and there is no
// path in this file that raises it except a matched acknowledgement for a
// ticket this block allocated. Not a timeout, not a byte count, not the last
// bridge beat. A block that could manufacture a barrier release is a block that
// can silently heal terrain a player broke, and no counter anywhere would move.
//
// `done_*` is a different statement -- "this job is finished, one way or
// another" -- and every job produces exactly one, because a job that produces
// no completion strands whatever was waiting on it. Only `wb_*` is the barrier.
//
// ---------------------------------------------------------------------------
// THE SOURCE IS SIX BYTES OFF A BURST BOUNDARY
// ---------------------------------------------------------------------------
// Summing sec 2's layer table -- header 64, A/B/C 2,178 each, D 1,024,
// E 3,072 -- puts layer F at page byte 10,694, and 10,694 = 64 * 167 + 6. So no
// aligned 64-B read starts at F. The block reads the ALIGNED SUPERSET from
// CHUNK_START and realigns by a CONSTANT LANE as the beats stream:
//
//     out[j] = { src[j+1], src[j] } [ 8*LANE +: 64 ]
//
// which at lane 6 is {src[j+1][47:0], src[j][63:48]} -- a constant part-select
// of a 128-bit concatenation, i.e. wiring. No barrel shifter, no DSP.
//
// out[j] spans TWO source beats, so the pipeline prefetches one chunk and then
// reads chunk g+1 to write burst g: 129 chunks in, 128 bursts out. That single
// extra chunk is where the whole off-by-one class lives, which is why the
// counts are asserted EXACTLY -- 130 guard requests (1 header + 129 chunks),
// 128 bridge bursts, 1,024 write beats -- rather than derived at a call site.
//
// ---------------------------------------------------------------------------
// MEM.GUARD MUST GAIN A READ ARM, AND IT IS NOT MADE HERE
// ---------------------------------------------------------------------------
// `zhao_mem_guard` gives TERRAIN.BUILD exactly one window, TERRAIN.PAGE_POOL,
// WRITE-ONLY, and its own comment names this block as the reason the read was
// withheld: "that will one day need to READ this pool -- when the block that
// does it exists, it brings its own arm and its own proof."
//
// The arm this block needs is one direction bit on the arm that is already
// there -- same constant window, same single client, a separate `terrain_rd_ok`
// so the two directions stay two theorems. MEM.GUARD is formally proven and its
// proof was re-run today, so the amendment is REPORTED, not made:
// design/contracts/TERRAIN.WRITEBACK.md sets it out with the four narrower
// forms that were considered and why each is impossible or forbidden.
//
// Until it lands, every sheet faults as V_INCOMPLETE with `guard_denied_o`
// counting, and NO slot is released. That is the correct behaviour for an
// un-granted client and it is what the suite measures.
//
// AND THE ARM CREATES A HAZARD THIS BLOCK ANSWERS. With reads admitted, client
// 6 can read ANY page in the pool, so a faulty writeback could journal ANOTHER
// PATCH'S scars, and nothing in MEM.GUARD can see that. So the 64-byte page
// header is read FIRST and the job is refused (V_HDR_IDENT) before one journal
// byte moves. One extra read out of 130, spent on exactly the failure the
// amendment creates.
//
// ---------------------------------------------------------------------------
// THE GUARD ANSWERS IN TWO CYCLES AND THE TWO BITS ARE NEVER BOTH HIGH
// ---------------------------------------------------------------------------
// `zhao_mem_guard` drives `rsp.ready = !fwd_active` as a LEVEL and pulses
// `rsp.ok` the cycle AFTER it accepts. Testing them in one arm reads every pass
// as a denial, silently, with the denial counter stuck at zero -- the defect
// found in BOTH geometry fetchers on 2026-09-06. So S_HREQ/S_RREQ wait on
// `ready` and S_HVERD/S_RVERD, separate states one cycle later, read `ok` /
// `violation`. Registered in tools/rtl/check_guard_verdict.py's client list in
// the same change that created this file.
//
// ---------------------------------------------------------------------------
// COUNTERS: EVENTS AND CYCLES ARE NAMED APART
// ---------------------------------------------------------------------------
// Two counters were found in one block this week that counted CYCLES while
// claiming EVENTS; one reported the producer's patience as a throughput figure.
// So every counter here that is not an event count carries its unit in its
// name: `ack_wait_max_cycles_o`, `jobs_stall_cycles_o`, and `outstanding_hwm_o`
// which is a LEVEL high-water mark, not a total.
//
// Conservative SystemVerilog subset (charter sec 2). Lint gate:
// `lint_terrain_writeback`.
`default_nettype none

module zhao_terrain_writeback
  import zhao_pkg::*;
#(
    // THE PAGE AND THE SHEET. spec/terrain_rules.md sec 2. Named constants, not
    // knobs: a different layout is a different format. They are parameters only
    // so the bench can shrink the transfer, and every derived count below comes
    // from them rather than from a second sum done by hand.
    parameter int unsigned PAGE_BYTES  = 21376,
    parameter int unsigned F_OFF       = 10694,  // layer F, page-relative
    parameter int unsigned F_BYTES     = 8192,   // 64x64 x {tag, strength}
    parameter int unsigned BURST_BYTES = 64,

    // TERRAIN.PAGE_POOL (ruling T2). PARAMETERS, not hard-coded constants: the
    // pool can move to any unmapped range, and a block that hard-codes an
    // address the guard also hard-codes gives the owner one knob with two
    // halves that must move together and no gate that says so.
    parameter logic [ZHAO_VRAM_ADDR_BITS-1:0] REGION_BASE  = 27'h400_0000,
    parameter int unsigned                    REGION_SLOTS = 1024,

    // ONE BIT WIDER THAN THE POOL NEEDS, for TERRAIN.PAGELOADER's reason: at
    // exactly $clog2(1024) = 10 bits a slot index CANNOT express 1024, so a
    // producer that computed a bad slot would arrive TRUNCATED -- slot 1024
    // presenting as slot 0 -- and this block would journal a LIVE page's sheet
    // under an evicted page's key. "A refusal is not a clamp."
    parameter int unsigned SLOTW = $clog2(REGION_SLOTS) + 1,

    parameter int unsigned GENW = 8,  // T10: "generation u8 minimum"

    // HOW MANY SHEETS MAY AWAIT ACKNOWLEDGEMENT AT ONCE. This is the answer to
    // "eviction pressure while a writeback is outstanding": when the table is
    // full `j_ready_o` falls and the sequencer is BACKPRESSURED. It never
    // displaces a ticket -- a displaced ticket un-holds a slot whose scars are
    // not yet safe -- and never drops a job. T2's 64 x 8 KiB
    // TERRAIN.WRITEBACK_STAGING region is the eventual ceiling; 4 is a default
    // small enough for a suite to fill on purpose, and nothing has measured the
    // real pressure yet.
    parameter int unsigned ACK_SLOTS = 4,

    // THE WATCHDOG REPORTS AND DOES NOT ACT. An ACK that never comes leaves its
    // ticket waiting forever, by design: releasing the slot on a stopwatch is a
    // fabricated ACK, and FAULTING the ticket would retire one of 1,024 slots
    // permanently -- which may well be right, and is NOT RULED, so it is not
    // invented. When a ticket passes this many cycles still unacknowledged it is
    // counted ONCE in `acks_overdue_o` and goes on waiting.
    parameter int unsigned ACK_DEADLINE_CYCLES = 100000,

    // Default ON. A parameter because the header's identity fields are a
    // REDUNDANCY (sec 2.1) and a redundancy that cannot be switched off cannot
    // be measured.
    parameter bit CHECK_HEADER_IDENT = 1'b1
) (
    input var logic clk,
    input var logic rst_n,

    // ---- configuration -----------------------------------------------------
    input var zhao_client_e cfg_vram_client_i,   // the guard identity to present
    input var zhao_client_e cfg_hps_client_i,    // the bridge identity
    input var logic [31:0]  cfg_journal_base_i,  // SW.STREAM's F-sheet journal
    input var logic [31:0]  cfg_journal_bytes_i,
    input var logic [31:0]  cfg_epoch_i,         // the live resource_epoch

    // ---- job in (from TERRAIN.SEQ, on a dirty_F eviction) -------------------
    input  var logic                 j_valid_i,
    output var logic                 j_ready_o,
    input  var logic [SLOTW-1:0]     j_slot_i,
    input  var logic [GENW-1:0]      j_gen_i,
    input  var logic [31:0]          j_epoch_i,
    input  var logic [31:0]          j_island_i,
    input  var logic signed [15:0]   j_ix_i,
    input  var logic signed [15:0]   j_iz_i,
    input  var logic [63:0]          j_journal_addr_i,
    input  var logic [31:0]          j_seq_i,     // the ticket the journal echoes
    input  var logic [31:0]          j_src_id_i,

    // ---- MEM.GUARD read client ---------------------------------------------
    // The shape zhao_scanout_fetch established and zhao_geom_meshfetch copied:
    // one request at len 64 returns exactly eight 64-bit beats.
    output var zhao_guard_req_t guard_req_o,
    input  var zhao_guard_rsp_t guard_rsp_i,
    input  var logic            beat_valid_i,
    input  var logic [63:0]     beat_data_i,
    input  var logic            beat_last_i,

    // ---- MEM.HPS.BRIDGE write client ---------------------------------------
    output var zhao_hps_burst_req_t hps_req_o,
    input  var logic                hps_req_grant_i,
    input  var zhao_hps_burst_rsp_t hps_rsp_i,
    output var logic [63:0]         hps_wdata_o,
    output var logic                hps_wvalid_o,
    // THE BRIDGE'S WRITE CHANNEL HAS NO `wr_ready` AND IT NEEDS ONE.
    // zhao_hps_bridge consumes a beat only while `busy && busy_write && issued`,
    // and `issued` comes one or more cycles AFTER the client's grant pulse -- so
    // a client that streams on the grant loses its first beats SILENTLY. This
    // block takes the acceptance level as a sideband so the stall is explicit
    // and testable; the requested amendment is that the bridge expose the level
    // it already computes. Named in the contract's "not yet established".
    input  var logic                hps_wready_i,
    output var logic                hps_wlast_o,

    // ---- the journal's acknowledgement -------------------------------------
    // SW.STREAM's doorbell in production; the harness in Verilator. `ok = 0` is
    // the journal REFUSING the sheet, which is not the same as silence.
    input  var logic        ack_valid_i,
    output var logic        ack_ready_o,
    input  var logic [31:0] ack_seq_i,
    input  var logic        ack_ok_i,

    // ---- the barrier release, to TERRAIN.RESIDENCY's wb_* port --------------
    output var logic             wb_valid_o,
    input  var logic             wb_ready_i,
    output var logic [SLOTW-1:0] wb_slot_o,
    output var logic [GENW-1:0]  wb_gen_o,
    output var logic [31:0]      wb_epoch_o,

    // ---- completion, to the sequencer --------------------------------------
    output var logic             done_valid_o,
    input  var logic             done_ready_i,
    output var logic [SLOTW-1:0] done_slot_o,
    output var logic [GENW-1:0]  done_gen_o,
    output var logic [31:0]      done_epoch_o,
    output var logic             done_ok_o,
    output var logic [3:0]       done_verdict_o,
    output var logic [31:0]      done_seq_o,
    output var logic [31:0]      done_src_id_o,

    // ---- fault trace -------------------------------------------------------
    output var logic [31:0]        fault_island_o,
    output var logic signed [15:0] fault_ix_o,
    output var logic signed [15:0] fault_iz_o,
    output var logic [31:0]        fault_seq_o,
    output var logic [31:0]        fault_src_id_o,
    output var logic [3:0]         fault_verdict_o,

    // ---- counters: EVENTS --------------------------------------------------
    output var logic [31:0] sheets_written_o,    // 8,192 bytes retired on the bridge
    output var logic [31:0] sheets_refused_o,    // judged from the job; nothing touched
    output var logic [31:0] sheets_faulted_o,    // the block had already touched memory
    output var logic [31:0] hdr_ident_fails_o,
    output var logic [31:0] guard_denied_o,
    output var logic [31:0] bridge_errs_o,
    output var logic [31:0] acks_ok_o,
    output var logic [31:0] acks_nak_o,
    output var logic [31:0] acks_unmatched_o,
    output var logic [31:0] acks_after_epoch_o,
    output var logic [31:0] acks_overdue_o,
    output var logic [31:0] seq_conflicts_o,
    // ---- counters: BYTES ---------------------------------------------------
    output var logic [31:0] wb_bytes_o,
    // ---- counters: LEVELS and CYCLES, named as such ------------------------
    output var logic [31:0] outstanding_hwm_o,
    output var logic [31:0] ack_wait_max_cycles_o,
    output var logic [31:0] jobs_stall_cycles_o
);

  // ------------------------------------------------------------- geometry ---
  localparam int unsigned BEATS_PER_B = BURST_BYTES / 8;                    // 8
  localparam int unsigned CHUNK_START = (F_OFF / BURST_BYTES) * BURST_BYTES;// 10,688
  localparam int unsigned LANE        = F_OFF % 8;                          // 6
  localparam int unsigned WR_BURSTS   = F_BYTES / BURST_BYTES;              // 128
  // ONE MORE CHUNK THAN BURSTS, unconditionally: the pipeline prefetches chunk
  // 0 and then reads chunk g+1 to write burst g. At lane 0 the last chunk's
  // bytes go unused -- one wasted 64-B read out of 129 -- and the alternative
  // is a second pipeline shape for a layout that does not exist.
  // `zref::terrain::kSheetReadChunks` carries the same expression.
  localparam int unsigned RD_CHUNKS   = WR_BURSTS + 1;                      // 129
  localparam int unsigned RCW         = $clog2(RD_CHUNKS + 1);              // 8
  localparam int unsigned WBW         = $clog2(WR_BURSTS);                  // 7

  // The verdict codes ARE zref::terrain::SheetWritebackVerdict, whose 0..7 are
  // in turn zref::mem::UploadVerdict. One law, two languages. 2 is unreachable
  // (the length is a nonzero constant) and 5 is ABSENT: layer F carries no CRC
  // of its own and the page's CRC covers the body as it was at LOAD time, which
  // is precisely what a stamped sheet is no longer. There is nothing to check
  // the bytes against and this block does not pretend there is.
  localparam logic [3:0] V_OK          = 4'd0;
  localparam logic [3:0] V_UNALIGNED   = 4'd1;
  localparam logic [3:0] V_OUTSIDE     = 4'd3;
  localparam logic [3:0] V_STALE       = 4'd4;
  localparam logic [3:0] V_JOURNAL     = 4'd6;
  localparam logic [3:0] V_UNREACH     = 4'd7;
  localparam logic [3:0] V_HDR_IDENT   = 4'd8;
  localparam logic [3:0] V_INCOMPLETE  = 4'd9;
  localparam logic [3:0] V_SEQ_INFLGHT = 4'd10;
  localparam logic [3:0] V_NAK         = 4'd11;

  // slot * PAGE_BYTES with no multiplier: one shifted copy of `slot` per set bit
  // of the constant. For 21,376 = 2^14 + 2^12 + 2^9 + 2^8 + 2^7 that is five
  // shifts and four adders, and no DSP.
  function automatic logic [31:0] slot_scaled(input logic [SLOTW-1:0] s);
    logic [31:0] acc;
    begin
      acc = 32'd0;
      for (int unsigned b = 0; b < 32; b++) begin
        if (((PAGE_BYTES >> b) & 32'd1) != 32'd0) begin
          acc = acc + ({{(32 - SLOTW) {1'b0}}, s} << b);
        end
      end
      slot_scaled = acc;
    end
  endfunction

  // THE REALIGNMENT SCHEME ASSUMES THE SHEET BEGINS IN THE FIRST BEAT OF ITS
  // CHUNK. True of the v1 layout (10,694 % 64 = 6 < 8). The divides above
  // truncate silently, so an override that broke it would journal a sheet from
  // the wrong offset -- 8,192 bytes of somebody else's scars with every other
  // signal agreeing. Refused at elaboration instead, for the same reason
  // TERRAIN.PAGELOADER refuses a non-beat-aligned CRC window.
`ifndef SYNTHESIS
  initial begin
    if ((F_OFF % BURST_BYTES) >= 8)
      $fatal(1, "writeback: F_OFF=%0d does not begin in its chunk's first beat", F_OFF);
    if ((F_BYTES % BURST_BYTES) != 0)
      $fatal(1, "writeback: F_BYTES=%0d is not a whole number of bursts", F_BYTES);
    if ((F_OFF + F_BYTES) > PAGE_BYTES)
      $fatal(1, "writeback: layer F [%0d, %0d) leaves the page", F_OFF, F_OFF + F_BYTES);
    if (ACK_SLOTS < 1)
      $fatal(1, "writeback: ACK_SLOTS must be >= 1 or no sheet can ever be journalled");
  end
`endif

  // ---------------------------------------------------------------- state ---
  typedef enum logic [3:0] {
    S_IDLE,
    S_CHECK,
    S_HREQ,    // the 64-byte page header: request
    S_HVERD,   // ...verdict, one cycle later
    S_HDATA,   // ...beats, and the identity test
    S_RREQ,    // a 64-byte sheet chunk: request
    S_RVERD,
    S_RDATA,
    S_WREQ,    // a 64-byte journal burst: request
    S_WBEAT    // ...eight beats
  } state_e;

  state_e state;

  // ---- the job, CAPTURED at acceptance (tools/rtl/check_ingress_capture.py) --
  logic [SLOTW-1:0]   job_slot;
  logic [GENW-1:0]    job_gen;
  logic [31:0]        job_epoch;
  logic [31:0]        job_island;
  logic signed [15:0] job_ix;
  logic signed [15:0] job_iz;
  logic [63:0]        job_journal;
  logic [31:0]        job_seq;
  logic [31:0]        job_src;

  logic [ZHAO_VRAM_ADDR_BITS-1:0] page_base;

  logic [RCW-1:0] rchunk;
  logic [WBW-1:0] wburst;
  logic [2:0]     rbeat;
  logic [2:0]     wbeat;

  // Two chunk buffers. `cur` holds the chunk whose eight beats supply the LOW
  // half of every output beat; `nxt` holds the one after it, of which only beat
  // 0 is consumed (by output beat 7). Copying `cur <= nxt` at each burst
  // boundary is 8 x 64 flops of mux and keeps the addressing trivial.
  logic [63:0] cur [BEATS_PER_B];
  logic [63:0] nxt [BEATS_PER_B];

  // the page header, captured from the first two beats of the header burst
  logic [15:0]        hdr_ver;
  logic [31:0]        hdr_island;
  logic signed [15:0] hdr_ix;
  logic signed [15:0] hdr_iz;

  // ------------------------------------------------------- the ticket table --
  // Allocation is the LOWEST FREE index; retirement is the LOWEST ACKED index.
  // Both are pure functions of the table's state, which is the determinism the
  // console's replay verification wants: the same job and ACK stream produces
  // the same slot assignments and the same retirement order, every time.
  logic             t_valid  [ACK_SLOTS];
  logic             t_acked  [ACK_SLOTS];
  logic             t_ack_ok [ACK_SLOTS];
  logic             t_late   [ACK_SLOTS];
  logic [SLOTW-1:0] t_slot   [ACK_SLOTS];
  logic [GENW-1:0]  t_gen    [ACK_SLOTS];
  logic [31:0]      t_epoch  [ACK_SLOTS];
  logic [31:0]      t_seq    [ACK_SLOTS];
  logic [31:0]      t_src    [ACK_SLOTS];
  logic [31:0]      t_wait   [ACK_SLOTS];

  localparam int unsigned TIW = (ACK_SLOTS > 1) ? $clog2(ACK_SLOTS) : 1;

  logic           has_free;
  logic [TIW-1:0] free_idx;
  always_comb begin
    has_free = 1'b0;
    free_idx = '0;
    for (int unsigned i = 0; i < ACK_SLOTS; i++) begin
      if (!has_free && !t_valid[i]) begin
        has_free = 1'b1;
        free_idx = TIW'(i);
      end
    end
  end

  logic           has_acked;
  logic [TIW-1:0] acked_idx;
  always_comb begin
    has_acked = 1'b0;
    acked_idx = '0;
    for (int unsigned i = 0; i < ACK_SLOTS; i++) begin
      if (!has_acked && t_valid[i] && t_acked[i]) begin
        has_acked = 1'b1;
        acked_idx = TIW'(i);
      end
    end
  end

  // AN ACK MATCHES ONLY A TICKET THAT IS STILL WAITING. A second ACK for a
  // ticket already acknowledged, or for one already retired, matches nothing and
  // is counted as unmatched -- which is the only behaviour that keeps "an ACK
  // for a page you did not send" from releasing an arbitrary slot.
  logic           ack_hit;
  logic [TIW-1:0] ack_idx;
  always_comb begin
    ack_hit = 1'b0;
    ack_idx = '0;
    for (int unsigned i = 0; i < ACK_SLOTS; i++) begin
      if (!ack_hit && t_valid[i] && !t_acked[i] && (t_seq[i] == ack_seq_i)) begin
        ack_hit = 1'b1;
        ack_idx = TIW'(i);
      end
    end
  end

  // A DUPLICATE SEQUENCE IS REFUSED BEFORE ANY BYTE MOVES. Two live tickets with
  // one sequence make the match above ambiguous, and an ambiguous barrier is not
  // a barrier. Refusing the second is cheap; guessing is not.
  logic seq_in_flight;
  always_comb begin
    seq_in_flight = 1'b0;
    for (int unsigned i = 0; i < ACK_SLOTS; i++) begin
      if (t_valid[i] && (t_seq[i] == job_seq)) seq_in_flight = 1'b1;
    end
  end

  logic [31:0] outstanding_now;
  always_comb begin
    outstanding_now = 32'd0;
    for (int unsigned i = 0; i < ACK_SLOTS; i++) begin
      if (t_valid[i] && !t_acked[i]) outstanding_now = outstanding_now + 32'd1;
    end
  end

  // ----------------------------------------------------- the pre-verdict ----
  // Same tests, same ORDER, as zref::terrain::sheet_pre_verdict, which delegates
  // to zref::mem::upload_verdict with the roles reversed: the guard region holds
  // the SOURCE and the arena holds the DESTINATION. Two arms of that law are
  // absent for a reason rather than by oversight:
  //   * kUploadZeroLength cannot arise -- F_BYTES is a nonzero constant;
  //   * kUploadOutsideGuard for the SOURCE cannot arise once the slot is in
  //     range, because the address is computed from the slot rather than
  //     supplied, and F_OFF + F_BYTES <= PAGE_BYTES is an elaboration check.
  //     The slot-range test carries that verdict code instead.
  logic        pre_slot_bad, pre_unaligned, pre_unreach, pre_arena_bad, pre_stale;
  logic [32:0] arena_lo, arena_hi, dst_lo, dst_hi;
  logic [3:0]  pre_verdict;

  assign arena_lo = {1'b0, cfg_journal_base_i};
  assign arena_hi = arena_lo + {1'b0, cfg_journal_bytes_i};
  assign dst_lo   = {1'b0, job_journal[31:0]};
  assign dst_hi   = dst_lo + 33'(F_BYTES);

  assign pre_slot_bad  = (32'(job_slot) >= 32'(REGION_SLOTS));
  assign pre_unaligned = (job_journal[5:0] != 6'd0);
  assign pre_unreach   = (job_journal[63:32] != 32'd0);
  assign pre_arena_bad = (dst_lo < arena_lo) || (dst_hi > arena_hi);
  assign pre_stale     = (job_epoch != cfg_epoch_i);

  always_comb begin
    if      (pre_slot_bad)  pre_verdict = V_OUTSIDE;
    else if (pre_unaligned) pre_verdict = V_UNALIGNED;
    else if (pre_unreach)   pre_verdict = V_UNREACH;
    else if (pre_arena_bad) pre_verdict = V_JOURNAL;
    else if (pre_stale)     pre_verdict = V_STALE;
    else if (seq_in_flight) pre_verdict = V_SEQ_INFLGHT;
    else                    pre_verdict = V_OK;
  end

  // Identity, before any payload byte is even read.
  logic ident_bad;
  assign ident_bad = CHECK_HEADER_IDENT
                   && ((hdr_ver != 16'd1) || (hdr_island != job_island)
                       || (hdr_ix != job_ix) || (hdr_iz != job_iz));

  // --------------------------------------------------------- the datapath ---
  // out[j] = { src[j+1], src[j] } [ 8*LANE +: 64 ]. `src[j]` is cur[wbeat];
  // `src[j+1]` is cur[wbeat+1] except for the last beat of the burst, where it
  // is the first beat of the NEXT chunk. A constant part-select: wiring.
  logic [63:0]  this_src, next_src;
  assign this_src  = cur[wbeat];
  assign next_src  = (wbeat == 3'(BEATS_PER_B - 1)) ? nxt[0] : cur[wbeat + 3'd1];
  // THE DISCARDED BITS ARE THE POINT, so the lint waiver is narrow and named:
  // the window slides 8*LANE bits up the pair, which leaves the bottom LANE
  // bytes of `this_src` (they belong to the PREVIOUS output beat) and the top
  // 8-LANE bytes of `next_src` (they belong to the NEXT one) unread here. Every
  // one of those bytes IS read, on a different beat. A wider select would be
  // the bug this waiver would otherwise hide, which is why the exact ranges are
  // written down and the suite compares all 8,192 bytes.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [127:0] beat_pair;
  /* verilator lint_on UNUSEDSIGNAL */
  assign beat_pair = {next_src, this_src};

  // -------------------------------------------------- the immediate report --
  // A refusal or a fault produces a completion with no ticket and no barrier
  // release. It is parked here until the retire machine takes it, and the job
  // port stays closed meanwhile -- one outstanding immediate report, always.
  logic             imm_valid;
  logic [SLOTW-1:0] imm_slot;
  logic [GENW-1:0]  imm_gen;
  logic [31:0]      imm_epoch;
  logic [3:0]       imm_verdict;
  logic [31:0]      imm_seq;
  logic [31:0]      imm_src;

  // ------------------------------------------------------------- outputs ----
  assign j_ready_o = (state == S_IDLE) && has_free && !imm_valid;

  // THE IDENTITY REFUSAL MUST NOT ISSUE THE READ IT IS REFUSING. `guard_req_o
  // .valid` is a function of state, and the cycle the block decides the slot
  // holds another patch's page it is still IN S_RREQ -- so without this gate it
  // would offer chunk 0's request on the very cycle it refuses, and the exact
  // "1 guard request on the ident path" count would silently become 2.
  logic ident_stop;
  assign ident_stop = (state == S_RREQ) && (rchunk == '0) && ident_bad;

  always_comb begin
    guard_req_o        = '0;
    guard_req_o.valid  = (state == S_HREQ) || ((state == S_RREQ) && !ident_stop);
    guard_req_o.write  = 1'b0;
    guard_req_o.client = cfg_vram_client_i;
    guard_req_o.addr   = (state == S_HREQ)
                       ? page_base
                       : (page_base + ZHAO_VRAM_ADDR_BITS'(32'(CHUNK_START)
                                                           + ({24'd0, rchunk} << 6)));
    guard_req_o.len    = 7'(BURST_BYTES);
    // The guard requires `be` to be the FULL contiguous mask over len bytes,
    // and every request here is a whole 64.
    guard_req_o.be     = '1;
  end

  always_comb begin
    hps_req_o        = '0;
    hps_req_o.valid  = (state == S_WREQ);
    hps_req_o.write  = 1'b1;
    hps_req_o.client = cfg_hps_client_i;
    hps_req_o.addr   = job_journal[31:0] + ({25'd0, wburst} << 6);
    hps_req_o.len    = 7'(BURST_BYTES);
  end

  // Data and `last` are a function of the beat counter alone, so a stalled
  // consumer sees a HELD beat -- the payload-stability half of ready/valid,
  // which is where a sibling block lost answers.
  assign hps_wvalid_o = (state == S_WBEAT);
  assign hps_wdata_o  = beat_pair[8*LANE +: 64];
  assign hps_wlast_o  = (wbeat == 3'(BEATS_PER_B - 1));

  // An acknowledgement is a fact that already happened on the far side, and
  // refusing to hear it does not un-happen it. What an ACK can DO -- release a
  // barrier -- is gated by the ticket table, not by this port.
  assign ack_ready_o = 1'b1;

  // ---------------------------------------------------------- retire path ---
  typedef enum logic [1:0] { R_IDLE, R_WB, R_DONE } rstate_e;
  rstate_e rstate;

  assign wb_valid_o   = (rstate == R_WB);
  assign done_valid_o = (rstate == R_DONE);

  // ------------------------------------------------------------ sequential --
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state                 <= S_IDLE;
      rstate                <= R_IDLE;
      job_slot              <= '0;
      job_gen               <= '0;
      job_epoch             <= '0;
      job_island            <= '0;
      job_ix                <= '0;
      job_iz                <= '0;
      job_journal           <= '0;
      job_seq               <= '0;
      job_src               <= '0;
      page_base             <= '0;
      rchunk                <= '0;
      wburst                <= '0;
      rbeat                 <= '0;
      wbeat                 <= '0;
      hdr_ver               <= '0;
      hdr_island            <= '0;
      hdr_ix                <= '0;
      hdr_iz                <= '0;
      imm_valid             <= 1'b0;
      imm_slot              <= '0;
      imm_gen               <= '0;
      imm_epoch             <= '0;
      imm_verdict           <= V_OK;
      imm_seq               <= '0;
      imm_src               <= '0;
      wb_slot_o             <= '0;
      wb_gen_o              <= '0;
      wb_epoch_o            <= '0;
      done_slot_o           <= '0;
      done_gen_o            <= '0;
      done_epoch_o          <= '0;
      done_ok_o             <= 1'b0;
      done_verdict_o        <= V_OK;
      done_seq_o            <= '0;
      done_src_id_o         <= '0;
      fault_island_o        <= '0;
      fault_ix_o            <= '0;
      fault_iz_o            <= '0;
      fault_seq_o           <= '0;
      fault_src_id_o        <= '0;
      fault_verdict_o       <= V_OK;
      sheets_written_o      <= '0;
      sheets_refused_o      <= '0;
      sheets_faulted_o      <= '0;
      hdr_ident_fails_o     <= '0;
      guard_denied_o        <= '0;
      bridge_errs_o         <= '0;
      acks_ok_o             <= '0;
      acks_nak_o            <= '0;
      acks_unmatched_o      <= '0;
      acks_after_epoch_o    <= '0;
      acks_overdue_o        <= '0;
      seq_conflicts_o       <= '0;
      wb_bytes_o            <= '0;
      outstanding_hwm_o     <= '0;
      ack_wait_max_cycles_o <= '0;
      jobs_stall_cycles_o   <= '0;
      for (int unsigned k = 0; k < BEATS_PER_B; k++) begin
        cur[k] <= 64'd0;
        nxt[k] <= 64'd0;
      end
      for (int unsigned i = 0; i < ACK_SLOTS; i++) begin
        t_valid[i]  <= 1'b0;
        t_acked[i]  <= 1'b0;
        t_ack_ok[i] <= 1'b0;
        t_late[i]   <= 1'b0;
        t_slot[i]   <= '0;
        t_gen[i]    <= '0;
        t_epoch[i]  <= '0;
        t_seq[i]    <= '0;
        t_src[i]    <= '0;
        t_wait[i]   <= '0;
      end
    end else begin
      // ------------------------------------------------- pressure, in CYCLES
      if (j_valid_i && !j_ready_o) jobs_stall_cycles_o <= jobs_stall_cycles_o + 32'd1;
      if (outstanding_now > outstanding_hwm_o) outstanding_hwm_o <= outstanding_now;

      // ------------------------------------------------- the ACK watchdog --
      // It REPORTS. It never releases a slot and never faults a ticket -- see
      // the ACK_DEADLINE_CYCLES note above. One event per ticket, latched by
      // `t_late`, so a long wait is counted once rather than once per cycle.
      for (int unsigned i = 0; i < ACK_SLOTS; i++) begin
        if (t_valid[i] && !t_acked[i]) begin
          t_wait[i] <= t_wait[i] + 32'd1;
          if (!t_late[i] && (t_wait[i] + 32'd1 >= 32'(ACK_DEADLINE_CYCLES))) begin
            t_late[i]      <= 1'b1;
            acks_overdue_o <= acks_overdue_o + 32'd1;
          end
        end
      end

      // -------------------------------------------- the acknowledgement ----
      if (ack_valid_i) begin
        if (ack_hit) begin
          t_acked[ack_idx]  <= 1'b1;
          t_ack_ok[ack_idx] <= ack_ok_i;
          if (t_wait[ack_idx] > ack_wait_max_cycles_o)
            ack_wait_max_cycles_o <= t_wait[ack_idx];
          if (ack_ok_i) acks_ok_o  <= acks_ok_o  + 32'd1;
          else          acks_nak_o <= acks_nak_o + 32'd1;
          // AN ACK THAT OUTLIVED ITS EPOCH IS DELIVERED ANYWAY, with the
          // TICKET'S own epoch and generation -- never the live one. The
          // directory checks every non-claim event against the stored
          // {epoch, generation} and rejects a stale one on identity, counted;
          // that is ITS ledger. Rewriting the epoch here would forge an
          // identity the directory trusts, and dropping the ACK here would
          // strand an EVICT_PENDING slot the directory might still retire.
          if (t_epoch[ack_idx] != cfg_epoch_i)
            acks_after_epoch_o <= acks_after_epoch_o + 32'd1;
        end else begin
          // Nothing to hold it for, and nothing it may release.
          acks_unmatched_o <= acks_unmatched_o + 32'd1;
        end
      end

      // ----------------------------------------------------- retire path ---
      case (rstate)
        R_IDLE: begin
          if (imm_valid) begin
            done_slot_o    <= imm_slot;
            done_gen_o     <= imm_gen;
            done_epoch_o   <= imm_epoch;
            done_ok_o      <= 1'b0;
            done_verdict_o <= imm_verdict;
            done_seq_o     <= imm_seq;
            done_src_id_o  <= imm_src;
            imm_valid      <= 1'b0;
            rstate         <= R_DONE;
          end else if (has_acked) begin
            done_slot_o    <= t_slot[acked_idx];
            done_gen_o     <= t_gen[acked_idx];
            done_epoch_o   <= t_epoch[acked_idx];
            done_ok_o      <= t_ack_ok[acked_idx];
            done_verdict_o <= t_ack_ok[acked_idx] ? V_OK : V_NAK;
            done_seq_o     <= t_seq[acked_idx];
            done_src_id_o  <= t_src[acked_idx];
            wb_slot_o      <= t_slot[acked_idx];
            wb_gen_o       <= t_gen[acked_idx];
            wb_epoch_o     <= t_epoch[acked_idx];
            // A NAK IS A FAULT AND IT RELEASES NOTHING. The bytes went out and
            // the journal refused them, so the scars are not safe and the slot
            // stays EVICT_PENDING. Retry is not ruled anywhere; T11's ABORT is
            // the escape hatch that exists and it is not this block's to pull.
            if (!t_ack_ok[acked_idx]) begin
              sheets_faulted_o <= sheets_faulted_o + 32'd1;
              fault_island_o   <= 32'd0;
              fault_ix_o       <= 16'sd0;
              fault_iz_o       <= 16'sd0;
              fault_seq_o      <= t_seq[acked_idx];
              fault_src_id_o   <= t_src[acked_idx];
              fault_verdict_o  <= V_NAK;
            end
            t_valid[acked_idx] <= 1'b0;
            t_acked[acked_idx] <= 1'b0;
            t_late[acked_idx]  <= 1'b0;
            t_wait[acked_idx]  <= '0;
            rstate             <= t_ack_ok[acked_idx] ? R_WB : R_DONE;
          end
        end

        // HELD until the directory takes it. A dropped barrier release strands
        // a slot in EVICT_PENDING forever -- the exact mirror of the dropped
        // `fin` that TERRAIN.PAGELOADER guards against.
        R_WB: begin
          if (wb_ready_i) rstate <= R_DONE;
        end

        R_DONE: begin
          if (done_ready_i) rstate <= R_IDLE;
        end

        default: rstate <= R_IDLE;
      endcase

      // ---------------------------------------------------- the engine -----
      case (state)

        // -------------------------------------------------------------------
        S_IDLE: begin
          if (j_valid_i && j_ready_o) begin
            job_slot    <= j_slot_i;
            job_gen     <= j_gen_i;
            job_epoch   <= j_epoch_i;
            job_island  <= j_island_i;
            job_ix      <= j_ix_i;
            job_iz      <= j_iz_i;
            job_journal <= j_journal_addr_i;
            job_seq     <= j_seq_i;
            job_src     <= j_src_id_i;
            state       <= S_CHECK;
          end
        end

        // -------------------------------------------------------------------
        // One cycle to judge the captured job. A refusal here has touched
        // NOTHING -- zero guard requests, zero bridge bursts -- which is what
        // separates `sheets_refused_o` from `sheets_faulted_o`.
        S_CHECK: begin
          if (pre_verdict != V_OK) begin
            imm_valid        <= 1'b1;
            imm_slot         <= job_slot;
            imm_gen          <= job_gen;
            imm_epoch        <= job_epoch;
            imm_verdict      <= pre_verdict;
            imm_seq          <= job_seq;
            imm_src          <= job_src;
            sheets_refused_o <= sheets_refused_o + 32'd1;
            if (pre_verdict == V_SEQ_INFLGHT) seq_conflicts_o <= seq_conflicts_o + 32'd1;
            fault_island_o   <= job_island;
            fault_ix_o       <= job_ix;
            fault_iz_o       <= job_iz;
            fault_seq_o      <= job_seq;
            fault_src_id_o   <= job_src;
            fault_verdict_o  <= pre_verdict;
            state            <= S_IDLE;
          end else begin
            page_base <= ZHAO_VRAM_ADDR_BITS'(32'(REGION_BASE) + slot_scaled(job_slot));
            rchunk    <= '0;
            wburst    <= '0;
            rbeat     <= '0;
            state     <= S_HREQ;
          end
        end

        // -------------------------------------------------------------------
        // `ready` is a LEVEL and moves the machine on. `ok` is NOT tested here.
        S_HREQ: begin
          if (guard_rsp_i.ready) state <= S_HVERD;
        end

        // ...it is tested HERE, one cycle later, where the guard pulses it.
        // S_HVERD WAITS WHEN NEITHER BIT IS SET. The real guard always answers
        // on that exact cycle, so the wait is unreachable against the real
        // block; it exists because treating "no answer yet" as a denial is the
        // same mistake in a different costume.
        S_HVERD: begin
          if (guard_rsp_i.violation) begin
            guard_denied_o   <= guard_denied_o + 32'd1;
            sheets_faulted_o <= sheets_faulted_o + 32'd1;
            imm_valid        <= 1'b1;
            imm_slot         <= job_slot;
            imm_gen          <= job_gen;
            imm_epoch        <= job_epoch;
            imm_verdict      <= V_INCOMPLETE;
            imm_seq          <= job_seq;
            imm_src          <= job_src;
            fault_island_o   <= job_island;
            fault_ix_o       <= job_ix;
            fault_iz_o       <= job_iz;
            fault_seq_o      <= job_seq;
            fault_src_id_o   <= job_src;
            fault_verdict_o  <= V_INCOMPLETE;
            state            <= S_IDLE;
          end else if (guard_rsp_i.ok) begin
            rbeat <= '0;
            state <= S_HDATA;
          end
        end

        // -------------------------------------------------------------------
        S_HDATA: begin
          if (beat_valid_i) begin
            // the 64-byte header, little-endian (spec/terrain_rules.md sec 2.1)
            if (rbeat == 3'd0) begin
              hdr_ver    <= beat_data_i[15:0];
              hdr_island <= beat_data_i[63:32];
            end
            if (rbeat == 3'd1) begin
              hdr_ix <= $signed(beat_data_i[15:0]);
              hdr_iz <= $signed(beat_data_i[31:16]);
            end
            if (rbeat == 3'(BEATS_PER_B - 1)) begin
              rbeat <= '0;
              state <= S_RREQ;
            end else begin
              rbeat <= rbeat + 3'd1;
            end
          end
          // The identity test runs on the cycle AFTER the last header beat is
          // captured, in S_RREQ's entry below -- see the guard on S_RREQ.
        end

        // -------------------------------------------------------------------
        S_RREQ: begin
          // IDENTITY BEFORE PAYLOAD, and before a single journal byte. The
          // header was captured in the previous state; if it names another
          // patch the job is refused here, with ZERO journal bytes written.
          if (ident_stop) begin
            hdr_ident_fails_o <= hdr_ident_fails_o + 32'd1;
            sheets_faulted_o  <= sheets_faulted_o + 32'd1;
            imm_valid         <= 1'b1;
            imm_slot          <= job_slot;
            imm_gen           <= job_gen;
            imm_epoch         <= job_epoch;
            imm_verdict       <= V_HDR_IDENT;
            imm_seq           <= job_seq;
            imm_src           <= job_src;
            fault_island_o    <= job_island;
            fault_ix_o        <= job_ix;
            fault_iz_o        <= job_iz;
            fault_seq_o       <= job_seq;
            fault_src_id_o    <= job_src;
            fault_verdict_o   <= V_HDR_IDENT;
            state             <= S_IDLE;
          end else if (guard_rsp_i.ready) begin
            state <= S_RVERD;
          end
        end

        // -------------------------------------------------------------------
        S_RVERD: begin
          if (guard_rsp_i.violation) begin
            guard_denied_o   <= guard_denied_o + 32'd1;
            sheets_faulted_o <= sheets_faulted_o + 32'd1;
            imm_valid        <= 1'b1;
            imm_slot         <= job_slot;
            imm_gen          <= job_gen;
            imm_epoch        <= job_epoch;
            imm_verdict      <= V_INCOMPLETE;
            imm_seq          <= job_seq;
            imm_src          <= job_src;
            fault_island_o   <= job_island;
            fault_ix_o       <= job_ix;
            fault_iz_o       <= job_iz;
            fault_seq_o      <= job_seq;
            fault_src_id_o   <= job_src;
            fault_verdict_o  <= V_INCOMPLETE;
            state            <= S_IDLE;
          end else if (guard_rsp_i.ok) begin
            rbeat <= '0;
            state <= S_RDATA;
          end
        end

        // -------------------------------------------------------------------
        S_RDATA: begin
          if (beat_valid_i) begin
            nxt[rbeat] <= beat_data_i;
            if (rbeat == 3'(BEATS_PER_B - 1)) begin
              rbeat <= '0;
              if (rchunk == '0) begin
                // THE PREFETCH. Chunk 0 supplies the low halves of write burst
                // 0's beats; the block cannot write anything until chunk 1's
                // first beat exists, because out[7] spans both.
                for (int unsigned k = 0; k < BEATS_PER_B; k++) cur[k] <= nxt[k];
                cur[BEATS_PER_B-1] <= beat_data_i;
                rchunk <= rchunk + RCW'(1);
                state  <= S_RREQ;
              end else begin
                wbeat <= '0;
                state <= S_WREQ;
              end
            end else begin
              rbeat <= rbeat + 3'd1;
            end
          end
        end

        // -------------------------------------------------------------------
        // An `err` may arrive against the request itself -- the bridge answers a
        // malformed burst with `err` and issues nothing -- so the abort is
        // spelled out in BOTH write states rather than only where beats move.
        S_WREQ: begin
          if (hps_rsp_i.err) begin
            bridge_errs_o    <= bridge_errs_o + 32'd1;
            sheets_faulted_o <= sheets_faulted_o + 32'd1;
            imm_valid        <= 1'b1;
            imm_slot         <= job_slot;
            imm_gen          <= job_gen;
            imm_epoch        <= job_epoch;
            imm_verdict      <= V_INCOMPLETE;
            imm_seq          <= job_seq;
            imm_src          <= job_src;
            fault_island_o   <= job_island;
            fault_ix_o       <= job_ix;
            fault_iz_o       <= job_iz;
            fault_seq_o      <= job_seq;
            fault_src_id_o   <= job_src;
            fault_verdict_o  <= V_INCOMPLETE;
            state            <= S_IDLE;
          end else if (hps_req_grant_i) begin
            wbeat <= '0;
            state <= S_WBEAT;
          end
        end

        // -------------------------------------------------------------------
        S_WBEAT: begin
          if (hps_rsp_i.err) begin
            bridge_errs_o    <= bridge_errs_o + 32'd1;
            sheets_faulted_o <= sheets_faulted_o + 32'd1;
            imm_valid        <= 1'b1;
            imm_slot         <= job_slot;
            imm_gen          <= job_gen;
            imm_epoch        <= job_epoch;
            imm_verdict      <= V_INCOMPLETE;
            imm_seq          <= job_seq;
            imm_src          <= job_src;
            fault_island_o   <= job_island;
            fault_ix_o       <= job_ix;
            fault_iz_o       <= job_iz;
            fault_seq_o      <= job_seq;
            fault_src_id_o   <= job_src;
            fault_verdict_o  <= V_INCOMPLETE;
            state            <= S_IDLE;
          end else if (hps_wready_i) begin
            // ENFORCED-BY: tests/formal/sat_add.sby
            wb_bytes_o <= zhao_sat_add32(wb_bytes_o, 32'd8);
            if (wbeat == 3'(BEATS_PER_B - 1)) begin
              if (32'(wburst) == 32'(WR_BURSTS - 1)) begin
                // -------- the whole sheet has landed ------------------------
                // THE TICKET IS ALLOCATED HERE, on the cycle the last beat
                // retires, and not in a state of its own. A separate S_ARM
                // state would open a one-cycle window in which an ACK for this
                // very sequence could arrive and be counted UNMATCHED -- a race
                // the journal can genuinely produce, and one that would look
                // exactly like a lost acknowledgement.
                t_valid[free_idx]  <= 1'b1;
                t_acked[free_idx]  <= 1'b0;
                t_ack_ok[free_idx] <= 1'b0;
                t_late[free_idx]   <= 1'b0;
                t_wait[free_idx]   <= '0;
                t_slot[free_idx]   <= job_slot;
                t_gen[free_idx]    <= job_gen;
                t_epoch[free_idx]  <= job_epoch;
                t_seq[free_idx]    <= job_seq;
                t_src[free_idx]    <= job_src;
                sheets_written_o   <= sheets_written_o + 32'd1;
                state              <= S_IDLE;
              end else begin
                for (int unsigned k = 0; k < BEATS_PER_B; k++) cur[k] <= nxt[k];
                wburst <= wburst + WBW'(1);
                rchunk <= rchunk + RCW'(1);
                rbeat  <= '0;
                state  <= S_RREQ;
              end
            end else begin
              wbeat <= wbeat + 3'd1;
            end
          end
        end

        default: state <= S_IDLE;
      endcase
    end
  end

  // `beat_last_i` and `hps_rsp_i.beat_valid/data/last` are not consumed: every
  // burst is a constant 64 bytes, so the beat counter already knows where a
  // burst ends, and trusting a `last` that disagreed with the count would let a
  // short burst masquerade as a whole one. The response channel carries READ
  // data and this block only writes. Keep the frozen port types and silence the
  // field-unused lint.
  /* verilator lint_off UNUSEDSIGNAL */
  logic unused_bits;
  assign unused_bits = beat_last_i ^ hps_rsp_i.beat_valid ^ hps_rsp_i.last
                     ^ (^hps_rsp_i.data);
  /* verilator lint_on UNUSEDSIGNAL */

endmodule

`default_nettype wire
