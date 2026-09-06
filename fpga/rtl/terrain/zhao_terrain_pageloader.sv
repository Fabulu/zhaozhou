// zhao_terrain_pageloader.sv - one whole terrain page, HPS DDR -> local SDRAM,
// CRC-checked, reported to TERRAIN.RESIDENCY.
//
// ---------------------------------------------------------------------------
// IT MOVES A PAGE. IT DOES NOT PUBLISH ONE.
// ---------------------------------------------------------------------------
// Step 3 of reports/TERRAIN_WORLD_LAYER_ARCHITECTURE.md sec 5. Everything this
// block does is already written down somewhere else, and the whole discipline
// of the file is to execute those laws rather than to add any:
//
//   * the page is 21,376 B in 334 x 64-B bursts and streams WHOLE
//     (spec/terrain_rules.md sec 2 table, sec 7);
//   * `page_crc32c` covers bytes [64, 21320) (spec/terrain_rules.md:126);
//   * the destination is TERRAIN.PAGE_POOL at 0x0400_0000, 1,024 slots of
//     21,376 B (ruling T2 / spec/memory_rules.md sec 5b);
//   * every VRAM write goes through MEM.GUARD (spec/memory_rules.md sec 5);
//   * bursts ride MEM.HPS.BRIDGE's frozen port, 64-B aligned, 64-bit beats
//     (design/contracts/MEM.HPS.BRIDGE.md);
//   * "a half-loaded or CRC-failed page is never rendered" (ruling T7);
//   * "loader completion carries success/failure and CRC identity" (T10).
//
// The last two together are the shape of the block. It NEVER decides residency:
// it hands TERRAIN.RESIDENCY a `fin` with the CRC it actually saw and an `ok`
// bit, and the directory publishes or FAULTs. A partial page cannot become
// resident because a partial page reports `ok = 0`, and the slot it wrote into
// is unreachable until the directory says otherwise.
//
// THE ARCHITECTURE SKETCH SAID "WITHHOLD fin ON MISMATCH". THAT IS SUPERSEDED.
// The sketch predates `zhao_terrain_residency_v2`, whose `fin_*` port carries
// `fin_ok_i` and `fin_crc_i` precisely so that a bad load is REPORTED. A loader
// that stays silent leaves the slot in LOADING forever: the page never becomes
// ground, which is safe, and the slot never becomes reusable, which is not.
// A refusal here is always a counted, addressed completion.
//
// ---------------------------------------------------------------------------
// THE DESTINATION IS COMPUTED, NEVER ACCEPTED
// ---------------------------------------------------------------------------
// The job names a residency SLOT, not an address. The address is
// REGION_BASE + slot * PAGE_BYTES, formed here, with `slot >= REGION_SLOTS`
// refused before the arithmetic runs. So "write outside the granted region" is
// not a rule this block obeys, it is a state it cannot reach -- and MEM.GUARD
// remains the independent check rather than the only one.
//
// The multiply is not a multiply: PAGE_BYTES is an elaboration-time constant,
// so `slot_scaled` sums one shifted copy of `slot` per set bit of it. For the
// canonical 21,376 = 2^14 + 2^12 + 2^9 + 2^8 + 2^7 that is five shifts and four
// adders, and no DSP.
//
// ---------------------------------------------------------------------------
// THE GUARD ANSWERS IN TWO CYCLES AND THE TWO BITS ARE NEVER BOTH HIGH
// ---------------------------------------------------------------------------
// `zhao_mem_guard` drives `rsp.ready = !fwd_active` as a LEVEL and pulses
// `rsp.ok` the cycle AFTER it accepts. Testing them in one arm reads every pass
// as a denial, silently, with the denial counter stuck at zero -- the defect
// found in BOTH geometry fetchers on 2026-09-06 and the reason
// `tools/rtl/check_guard_verdict.py` exists. So S_GREQ waits on `ready` and
// S_GVERD, a separate state one cycle later, reads `ok` / `violation`.
//
// S_GVERD WAITS WHEN NEITHER BIT IS SET. The real guard always answers on that
// exact cycle, so the wait is unreachable against the real block; it is there
// because the alternative -- treating "no answer yet" as a denial -- is the
// same mistake in a different costume, and this port will one day face a guard
// with a deeper pipe.
//
// ---------------------------------------------------------------------------
// WHAT IS NOT WRITTEN, AND IS THEREFORE A PARAMETER OR A PORT
// ---------------------------------------------------------------------------
// The client identity is RULED (T3: `ZHAO_CLIENT_TERRAIN_BUILD = 6`) and, since
// 2026-09-06, ENACTED. `zhao_pkg::zhao_client_e` declares 6 (with 5 left as the
// deliberate hole T3 reserves); `zhao_vram_arbiter` carries seven client ports
// rather than five, because the ARRAY INDEX is cast straight to the enum there
// and slot identity and client identity are one fact; and MEM.GUARD has a
// bank-2 window at last -- TERRAIN.PAGE_POOL, [0x0400_0000, 0x054E_0000),
// WRITE-ONLY, client 6 alone, constant bounds. This paragraph used to say the
// opposite of all three, and it was right when it said it.
//
// THE IDENTITY STAYS AN INPUT ANYWAY, and `guard_denied_o` stays a fault
// counter. Which client a deployment presents is configuration, not a property
// of moving a page; hard-wiring the constant would remove a knob to save
// nothing. And with the window in place a denial no longer means "the machine
// has no room for this block" -- it means this block computed an address the
// pool does not contain, which is a bug and should be counted as one.
//
// WHAT THE WINDOW DOES NOT DO. T2 wants a STATE-AWARE permission -- "a loader
// may write only a LOADING slot" -- and the guard's is spatial: the whole pool,
// for this client, write-only. The guard has one muxed request port and no
// residency context, and the interface that would carry slot state to it is
// ruled nowhere. So the last line of defence against writing the WRONG slot is
// in this file: `SLOTW` is one bit wider than the pool needs precisely so an
// out-of-range slot ARRIVES as out-of-range and is refused instead of being
// truncated into a live page. That was already true; it now matters more.
//
// Conservative SystemVerilog subset (charter sec 2). Lint gate:
// `lint_terrain_pageloader`.
`default_nettype none

module zhao_terrain_pageloader
  import zhao_pkg::*;
#(
    // THE PAGE. spec/terrain_rules.md sec 2 / sec 7. Named constants, not
    // knobs: a different stride is a different format.
    parameter int unsigned PAGE_BYTES  = 21376,
    parameter int unsigned CRC_LO      = 64,
    parameter int unsigned CRC_HI      = 21320,
    parameter int unsigned BURST_BYTES = 64,

    // TERRAIN.PAGE_POOL (ruling T2). MEM.GUARD now HAS this region, and the
    // amendment confirmed these defaults rather than discovering them --
    // `zhao_pkg::ZHAO_TERRAIN_PAGE_POOL_BASE` is 0x0400_0000 and the window
    // spans exactly REGION_SLOTS * PAGE_BYTES. They stay PARAMETERS: the pool
    // can move to any unmapped range, and a block that hard-codes an address
    // the guard also hard-codes gives the owner one knob with two halves that
    // have to be moved together and no gate that says so.
    parameter logic [ZHAO_VRAM_ADDR_BITS-1:0] REGION_BASE  = 27'h400_0000,
    parameter int unsigned                    REGION_SLOTS = 1024,

    // ONE BIT WIDER THAN THE POOL NEEDS, and that is the whole reason the
    // out-of-range refusal is reachable at all. At exactly $clog2(1024) = 10
    // bits a slot index CANNOT express 1024, so a producer that computed a bad
    // slot would arrive TRUNCATED -- slot 1024 presenting as slot 0 and
    // overwriting a live page. "A refusal is not a clamp"
    // (zref_mem_upload.hpp), and a silent truncation is a clamp with no
    // counter. The extra wire buys the refusal.
    parameter int unsigned SLOTW = $clog2(REGION_SLOTS) + 1,

    parameter int unsigned GENW = 8,  // T10: "generation u8 minimum"

    // Both default ON. They are parameters because the page header's identity
    // fields and its own CRC word are a REDUNDANCY (spec/terrain_rules.md
    // sec 2.1), and a redundancy that cannot be switched off cannot be measured.
    parameter bit CHECK_HEADER_IDENT = 1'b1,
    parameter bit CHECK_HEADER_CRC   = 1'b1
) (
    input var logic clk,
    input var logic rst_n,

    // ---- configuration ------------------------------------------------------
    // Frame-scoped, from the epoch command and SW.STREAM's staging arena.
    // Read live rather than captured: they are the machine's state, not the
    // job's payload. The JOB's fields are captured at acceptance (below).
    input var zhao_client_e cfg_vram_client_i,
    input var zhao_client_e cfg_hps_client_i,
    input var logic [31:0]  cfg_hps_arena_base_i,
    input var logic [31:0]  cfg_hps_arena_bytes_i,
    input var logic [31:0]  cfg_epoch_i,          // the live resource_epoch

    // ---- job in -------------------------------------------------------------
    input  var logic                            j_valid_i,
    output var logic                            j_ready_o,
    input  var logic [SLOTW-1:0]                j_slot_i,
    input  var logic [GENW-1:0]                 j_gen_i,
    input  var logic [31:0]                     j_epoch_i,
    input  var logic [31:0]                     j_island_i,
    input  var logic signed [15:0]              j_ix_i,
    input  var logic signed [15:0]              j_iz_i,
    input  var logic [63:0]                     j_hps_addr_i,
    input  var logic [31:0]                     j_expect_crc_i,
    input  var logic [31:0]                     j_src_id_i,

    // ---- MEM.HPS.BRIDGE client ---------------------------------------------
    output var zhao_hps_burst_req_t hps_req_o,
    input  var logic                hps_req_grant_i,
    input  var zhao_hps_burst_rsp_t hps_rsp_i,

    // ---- MEM.GUARD write client --------------------------------------------
    output var zhao_guard_req_t guard_req_o,
    input  var zhao_guard_rsp_t guard_rsp_i,
    output var logic [63:0]     guard_wdata_o,
    output var logic            guard_wvalid_o,
    input  var logic            guard_wready_i,
    output var logic            guard_wlast_o,

    // ---- completion -> TERRAIN.RESIDENCY fin_* ------------------------------
    output var logic                            fin_valid_o,
    input  var logic                            fin_ready_i,
    output var logic [SLOTW-1:0]                fin_slot_o,
    output var logic [GENW-1:0]                 fin_gen_o,
    output var logic [31:0]                     fin_epoch_o,
    output var logic                            fin_ok_o,
    output var logic [31:0]                     fin_crc_o,
    output var logic [3:0]                      fin_verdict_o,
    output var logic [31:0]                     fin_src_id_o,

    // ---- the failing page, latched (MEASURE.HISTOGRAM's refuse-loudly lane) --
    output var logic [31:0]        fault_island_o,
    output var logic signed [15:0] fault_ix_o,
    output var logic signed [15:0] fault_iz_o,
    output var logic [31:0]        fault_src_id_o,
    output var logic [3:0]         fault_verdict_o,
    output var logic [31:0]        fault_crc_seen_o,
    output var logic [31:0]        fault_crc_expect_o,

    // ---- evidence ----------------------------------------------------------
    output var logic [31:0] pages_loaded_o,
    output var logic [31:0] pages_faulted_o,
    output var logic [31:0] pages_refused_o,
    output var logic [31:0] crc_fails_o,
    output var logic [31:0] hdr_ident_fails_o,
    output var logic [31:0] incomplete_o,
    output var logic [31:0] guard_denied_o,
    output var logic [31:0] bridge_errs_o,
    output var logic [31:0] load_bytes_o
);

  // ------------------------------------------------------------- geometry ---
  localparam int unsigned BURSTS      = PAGE_BYTES / BURST_BYTES;  // 334
  localparam int unsigned BEATS_PER_B = BURST_BYTES / 8;           // 8
  localparam int unsigned CRC_BEAT_LO = CRC_LO / 8;                // 8
  localparam int unsigned CRC_BEAT_HI = CRC_HI / 8;                // 2,665
  localparam int unsigned BURSTW      = $clog2(BURSTS);            // 9
  localparam int unsigned BIDXW       = BURSTW + 3;                // 12

  // The verdict codes ARE zref::terrain::PageLoadVerdict, which are in turn
  // zref::mem::UploadVerdict for 0..7. One law, two languages.
  localparam logic [3:0] V_OK        = 4'd0;
  localparam logic [3:0] V_UNALIGNED = 4'd1;
  localparam logic [3:0] V_OUTSIDE   = 4'd3;
  localparam logic [3:0] V_STALE     = 4'd4;
  localparam logic [3:0] V_CRC       = 4'd5;
  localparam logic [3:0] V_SRC_ARENA = 4'd6;
  localparam logic [3:0] V_SRC_UNRCH = 4'd7;
  localparam logic [3:0] V_HDR_IDENT = 4'd8;
  localparam logic [3:0] V_INCOMPLETE= 4'd9;

  // slot * PAGE_BYTES with no multiplier: one shifted copy of `slot` per set
  // bit of the constant. See the header note.
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

  // ---------------------------------------------------------------- state ---
  typedef enum logic [2:0] {
    S_IDLE,
    S_CHECK,
    S_HREQ,
    S_HDATA,
    S_GREQ,
    S_GVERD,
    S_WBEAT,
    S_FIN
  } state_e;

  state_e state;

  // ---- the job, CAPTURED at acceptance (tools/rtl/check_ingress_capture.py) --
  logic [SLOTW-1:0]     job_slot;
  logic [GENW-1:0]      job_gen;
  logic [31:0]          job_epoch;
  logic [31:0]          job_island;
  logic signed [15:0]   job_ix;
  logic signed [15:0]   job_iz;
  logic [63:0]          job_hps;
  logic [31:0]          job_crc;
  logic [31:0]          job_src;

  logic [ZHAO_VRAM_ADDR_BITS-1:0] page_base;

  logic [BURSTW-1:0] burst;
  logic [2:0]        rbeat;   // beat within the HPS read burst
  logic [2:0]        wbeat;   // beat within the guarded write burst
  logic [63:0]       chunk [BEATS_PER_B];

  logic [31:0] crc_r;         // running CRC-32C register (init all ones)

  // the page header, captured from the first three beats of burst 0
  logic [15:0]        hdr_ver;
  logic [31:0]        hdr_island;
  logic signed [15:0] hdr_ix;
  logic signed [15:0] hdr_iz;
  logic [31:0]        hdr_crc;

  // ------------------------------------------------------- the CRC window ---
  // {burst, rbeat} is the page-global beat index. The window is whole-beat, so
  // no partial fold exists anywhere in this block.
  // ENFORCED-BY: tests/terrain/pageloader_rtl_directed.cpp
  //
  // It was "by construction (64 and 21320 are both multiples of 8)", which is a
  // claim about PARAMETERS that anyone may override. The divides above truncate
  // silently, so a window of 60 would fold from byte 56 and produce a confident
  // wrong CRC over a page that is otherwise perfect -- the worst shape of defect
  // this block has, since a correct page is then refused or a corrupt one
  // accepted with every other signal agreeing. Refused at elaboration instead.
  // ENFORCED-BY: tests/terrain/pageloader_rtl_directed.cpp
`ifndef SYNTHESIS
  initial begin
    if ((CRC_LO % 8) != 0)
      $fatal(1, "pageloader: CRC_LO=%0d is not a whole beat; the /8 above would truncate it", CRC_LO);
    if ((CRC_HI % 8) != 0)
      $fatal(1, "pageloader: CRC_HI=%0d is not a whole beat; the /8 above would truncate it", CRC_HI);
    if (CRC_HI <= CRC_LO)
      $fatal(1, "pageloader: empty CRC window [%0d, %0d)", CRC_LO, CRC_HI);
  end
`endif
  logic [BIDXW-1:0] bidx;
  logic             crc_covered;
  assign bidx        = {burst, rbeat};
  assign crc_covered = (bidx >= BIDXW'(CRC_BEAT_LO)) && (bidx < BIDXW'(CRC_BEAT_HI));

  logic [31:0] fold_out;
  zhao_crc32c_fold u_fold (
      .c_i(crc_r),
      .d_i(hps_rsp_i.data),
      .n_i(4'd8),
      .c_o(fold_out)
  );

  // ------------------------------------------------------- the pre-verdict --
  // Same tests, same ORDER, as zref::terrain::page_pre_verdict, which in turn
  // delegates to zref::mem::upload_verdict. The two absent arms are absent for
  // a reason, not by oversight:
  //   * kUploadZeroLength cannot arise -- PAGE_BYTES is a nonzero constant;
  //   * kUploadOutsideGuard for the DESTINATION cannot arise once the slot is
  //     in range, because the address is computed from the slot rather than
  //     supplied. The slot-range test carries that verdict code instead.
  logic        pre_slot_bad, pre_unaligned, pre_unreach, pre_arena_bad, pre_stale;
  logic [32:0] arena_lo, arena_hi, src_lo, src_hi;
  logic [3:0]  pre_verdict;

  assign arena_lo = {1'b0, cfg_hps_arena_base_i};
  assign arena_hi = arena_lo + {1'b0, cfg_hps_arena_bytes_i};
  assign src_lo   = {1'b0, job_hps[31:0]};
  assign src_hi   = src_lo + 33'(PAGE_BYTES);

  assign pre_slot_bad  = (32'(job_slot) >= 32'(REGION_SLOTS));
  assign pre_unaligned = (job_hps[5:0] != 6'd0);
  assign pre_unreach   = (job_hps[63:32] != 32'd0);
  assign pre_arena_bad = (src_lo < arena_lo) || (src_hi > arena_hi);
  assign pre_stale     = (job_epoch != cfg_epoch_i);

  always_comb begin
    if      (pre_slot_bad)  pre_verdict = V_OUTSIDE;
    else if (pre_unaligned) pre_verdict = V_UNALIGNED;
    else if (pre_unreach)   pre_verdict = V_SRC_UNRCH;
    else if (pre_arena_bad) pre_verdict = V_SRC_ARENA;
    else if (pre_stale)     pre_verdict = V_STALE;
    else                    pre_verdict = V_OK;
  end

  // ------------------------------------------------- the post-transfer test --
  logic [31:0] crc_final;
  logic        ident_bad, crc_bad;
  assign crc_final = ~crc_r;
  // Identity before integrity: another patch's page has a perfectly valid CRC.
  assign ident_bad = CHECK_HEADER_IDENT
                   && ((hdr_ver != 16'd1) || (hdr_island != job_island)
                       || (hdr_ix != job_ix) || (hdr_iz != job_iz));
  // TWO DECLARED HOLDERS OF ONE NUMBER. The job carries SW.STREAM's
  // `expected_page_crc32c` (T5); the page carries its own `page_crc32c`
  // (sec 2.1) over the identical range. Nothing rules which governs, so
  // neither governs alone and a disagreement is itself a corruption.
  assign crc_bad = (crc_final != job_crc)
                 || (CHECK_HEADER_CRC && (crc_final != hdr_crc));

  // --------------------------------------------------------------- outputs --
  assign j_ready_o = (state == S_IDLE);

  always_comb begin
    hps_req_o        = '0;
    hps_req_o.valid  = (state == S_HREQ);
    hps_req_o.write  = 1'b0;
    hps_req_o.client = cfg_hps_client_i;
    hps_req_o.addr   = job_hps[31:0] + ({23'd0, burst} << 6);
    hps_req_o.len    = 7'(BURST_BYTES);
  end

  always_comb begin
    guard_req_o        = '0;
    guard_req_o.valid  = (state == S_GREQ);
    guard_req_o.write  = 1'b1;
    guard_req_o.client = cfg_vram_client_i;
    guard_req_o.addr   = page_base + ZHAO_VRAM_ADDR_BITS'({23'd0, burst} << 6);
    guard_req_o.len    = 7'(BURST_BYTES);
    // The guard requires `be` to be the FULL contiguous mask over len bytes,
    // and every burst here is a whole 64.
    guard_req_o.be     = '1;
  end

  // Data and `last` are a function of the beat counter alone, so a stalled
  // consumer sees a HELD beat -- the payload-stability half of ready/valid,
  // which is where a sibling block lost answers.
  assign guard_wvalid_o = (state == S_WBEAT);
  assign guard_wdata_o  = chunk[wbeat];
  assign guard_wlast_o  = (wbeat == 3'(BEATS_PER_B - 1));

  // ------------------------------------------------------------- sequential --
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state              <= S_IDLE;
      job_slot           <= '0;
      job_gen            <= '0;
      job_epoch          <= '0;
      job_island         <= '0;
      job_ix             <= '0;
      job_iz             <= '0;
      job_hps            <= '0;
      job_crc            <= '0;
      job_src            <= '0;
      page_base          <= '0;
      burst              <= '0;
      rbeat              <= '0;
      wbeat              <= '0;
      crc_r              <= 32'hFFFF_FFFF;
      hdr_ver            <= '0;
      hdr_island         <= '0;
      hdr_ix             <= '0;
      hdr_iz             <= '0;
      hdr_crc            <= '0;
      fin_valid_o        <= 1'b0;
      fin_slot_o         <= '0;
      fin_gen_o          <= '0;
      fin_epoch_o        <= '0;
      fin_ok_o           <= 1'b0;
      fin_crc_o          <= '0;
      fin_verdict_o      <= V_OK;
      fin_src_id_o       <= '0;
      fault_island_o     <= '0;
      fault_ix_o         <= '0;
      fault_iz_o         <= '0;
      fault_src_id_o     <= '0;
      fault_verdict_o    <= V_OK;
      fault_crc_seen_o   <= '0;
      fault_crc_expect_o <= '0;
      pages_loaded_o     <= '0;
      pages_faulted_o    <= '0;
      pages_refused_o    <= '0;
      crc_fails_o        <= '0;
      hdr_ident_fails_o  <= '0;
      incomplete_o       <= '0;
      guard_denied_o     <= '0;
      bridge_errs_o      <= '0;
      load_bytes_o       <= '0;
      for (int unsigned k = 0; k < BEATS_PER_B; k++) chunk[k] <= 64'd0;
    end else begin
      case (state)

        // -------------------------------------------------------------------
        S_IDLE: begin
          if (j_valid_i) begin
            job_slot   <= j_slot_i;
            job_gen    <= j_gen_i;
            job_epoch  <= j_epoch_i;
            job_island <= j_island_i;
            job_ix     <= j_ix_i;
            job_iz     <= j_iz_i;
            job_hps    <= j_hps_addr_i;
            job_crc    <= j_expect_crc_i;
            job_src    <= j_src_id_i;
            state      <= S_CHECK;
          end
        end

        // -------------------------------------------------------------------
        // One cycle to judge the captured job. A refusal here has moved ZERO
        // bytes, which is what separates `pages_refused_o` from
        // `pages_faulted_o`: the first is a bad request, the second is a bad
        // page.
        S_CHECK: begin
          fin_slot_o   <= job_slot;
          fin_gen_o    <= job_gen;
          fin_epoch_o  <= job_epoch;
          fin_src_id_o <= job_src;
          if (pre_verdict != V_OK) begin
            fin_valid_o        <= 1'b1;
            fin_ok_o           <= 1'b0;
            fin_crc_o          <= 32'd0;
            fin_verdict_o      <= pre_verdict;
            pages_refused_o    <= pages_refused_o + 32'd1;
            fault_island_o     <= job_island;
            fault_ix_o         <= job_ix;
            fault_iz_o         <= job_iz;
            fault_src_id_o     <= job_src;
            fault_verdict_o    <= pre_verdict;
            fault_crc_seen_o   <= 32'd0;
            fault_crc_expect_o <= job_crc;
            state              <= S_FIN;
          end else begin
            page_base <= ZHAO_VRAM_ADDR_BITS'(32'(REGION_BASE) + slot_scaled(job_slot));
            burst     <= '0;
            rbeat     <= '0;
            crc_r     <= 32'hFFFF_FFFF;
            state     <= S_HREQ;
          end
        end

        // -------------------------------------------------------------------
        // An `err` may arrive against the request itself -- the bridge answers
        // a malformed burst with `err` and issues nothing -- so the abort is
        // spelled out in BOTH read states rather than only where beats arrive.
        // Every burst this block emits is 64-B aligned at len 64, so on the
        // real bridge this arm is unreachable; the harness reaches it, which is
        // the point of it existing.
        S_HREQ: begin
          if (hps_rsp_i.err) begin
            bridge_errs_o      <= bridge_errs_o + 32'd1;
            fin_valid_o        <= 1'b1;
            fin_ok_o           <= 1'b0;
            fin_crc_o          <= ~crc_r;
            fin_verdict_o      <= V_INCOMPLETE;
            pages_faulted_o    <= pages_faulted_o + 32'd1;
            incomplete_o       <= incomplete_o + 32'd1;
            fault_island_o     <= job_island;
            fault_ix_o         <= job_ix;
            fault_iz_o         <= job_iz;
            fault_src_id_o     <= job_src;
            fault_verdict_o    <= V_INCOMPLETE;
            fault_crc_seen_o   <= ~crc_r;
            fault_crc_expect_o <= job_crc;
            state              <= S_FIN;
          end else if (hps_req_grant_i) begin
            rbeat <= '0;
            state <= S_HDATA;
          end
        end

        // -------------------------------------------------------------------
        S_HDATA: begin
          if (hps_rsp_i.err) begin
            bridge_errs_o      <= bridge_errs_o + 32'd1;
            fin_valid_o        <= 1'b1;
            fin_ok_o           <= 1'b0;
            fin_crc_o          <= ~crc_r;
            fin_verdict_o      <= V_INCOMPLETE;
            pages_faulted_o    <= pages_faulted_o + 32'd1;
            incomplete_o       <= incomplete_o + 32'd1;
            fault_island_o     <= job_island;
            fault_ix_o         <= job_ix;
            fault_iz_o         <= job_iz;
            fault_src_id_o     <= job_src;
            fault_verdict_o    <= V_INCOMPLETE;
            fault_crc_seen_o   <= ~crc_r;
            fault_crc_expect_o <= job_crc;
            state              <= S_FIN;
          end else if (hps_rsp_i.beat_valid) begin
            chunk[rbeat] <= hps_rsp_i.data;
            if (crc_covered) crc_r <= fold_out;
            // the 64-byte header, little-endian (spec/terrain_rules.md sec 2.1)
            if (burst == '0) begin
              if (rbeat == 3'd0) begin
                hdr_ver    <= hps_rsp_i.data[15:0];
                hdr_island <= hps_rsp_i.data[63:32];
              end
              if (rbeat == 3'd1) begin
                hdr_ix <= $signed(hps_rsp_i.data[15:0]);
                hdr_iz <= $signed(hps_rsp_i.data[31:16]);
              end
              if (rbeat == 3'd4) hdr_crc <= hps_rsp_i.data[31:0];
            end
            if (rbeat == 3'(BEATS_PER_B - 1)) begin
              rbeat <= '0;
              wbeat <= '0;
              state <= S_GREQ;
            end else begin
              rbeat <= rbeat + 3'd1;
            end
          end
        end

        // -------------------------------------------------------------------
        // `ready` is a LEVEL and moves the machine on. `ok` is NOT tested here.
        S_GREQ: begin
          if (guard_rsp_i.ready) state <= S_GVERD;
        end

        // -------------------------------------------------------------------
        // ...it is tested HERE, one cycle later, where the guard pulses it.
        S_GVERD: begin
          if (guard_rsp_i.violation) begin
            guard_denied_o     <= guard_denied_o + 32'd1;
            fin_valid_o        <= 1'b1;
            fin_ok_o           <= 1'b0;
            fin_crc_o          <= ~crc_r;
            fin_verdict_o      <= V_INCOMPLETE;
            pages_faulted_o    <= pages_faulted_o + 32'd1;
            incomplete_o       <= incomplete_o + 32'd1;
            fault_island_o     <= job_island;
            fault_ix_o         <= job_ix;
            fault_iz_o         <= job_iz;
            fault_src_id_o     <= job_src;
            fault_verdict_o    <= V_INCOMPLETE;
            fault_crc_seen_o   <= ~crc_r;
            fault_crc_expect_o <= job_crc;
            state              <= S_FIN;
          end else if (guard_rsp_i.ok) begin
            wbeat <= '0;
            state <= S_WBEAT;
          end
        end

        // -------------------------------------------------------------------
        S_WBEAT: begin
          if (guard_wready_i) begin
            load_bytes_o <= zhao_sat_add32(load_bytes_o, 32'd8);
            if (wbeat == 3'(BEATS_PER_B - 1)) begin
              if (32'(burst) == 32'(BURSTS - 1)) begin
                // -------- the whole page has landed ------------------------
                fin_valid_o  <= 1'b1;
                fin_crc_o    <= crc_final;
                if (ident_bad) begin
                  fin_ok_o           <= 1'b0;
                  fin_verdict_o      <= V_HDR_IDENT;
                  pages_faulted_o    <= pages_faulted_o + 32'd1;
                  hdr_ident_fails_o  <= hdr_ident_fails_o + 32'd1;
                  fault_verdict_o    <= V_HDR_IDENT;
                  fault_island_o     <= job_island;
                  fault_ix_o         <= job_ix;
                  fault_iz_o         <= job_iz;
                  fault_src_id_o     <= job_src;
                  fault_crc_seen_o   <= crc_final;
                  fault_crc_expect_o <= job_crc;
                end else if (crc_bad) begin
                  fin_ok_o           <= 1'b0;
                  fin_verdict_o      <= V_CRC;
                  pages_faulted_o    <= pages_faulted_o + 32'd1;
                  crc_fails_o        <= crc_fails_o + 32'd1;
                  fault_verdict_o    <= V_CRC;
                  fault_island_o     <= job_island;
                  fault_ix_o         <= job_ix;
                  fault_iz_o         <= job_iz;
                  fault_src_id_o     <= job_src;
                  fault_crc_seen_o   <= crc_final;
                  fault_crc_expect_o <= job_crc;
                end else begin
                  fin_ok_o       <= 1'b1;
                  fin_verdict_o  <= V_OK;
                  pages_loaded_o <= pages_loaded_o + 32'd1;
                end
                state <= S_FIN;
              end else begin
                burst <= burst + BURSTW'(1);
                rbeat <= '0;
                state <= S_HREQ;
              end
            end else begin
              wbeat <= wbeat + 3'd1;
            end
          end
        end

        // -------------------------------------------------------------------
        // HELD until the directory takes it. A completion that evaporates
        // because the consumer was busy is a slot stuck in LOADING forever.
        S_FIN: begin
          if (fin_ready_i) begin
            fin_valid_o <= 1'b0;
            state       <= S_IDLE;
          end
        end

        default: state <= S_IDLE;
      endcase
    end
  end

  // `hps_rsp_i.last` is not consumed: the burst length is a constant 64 bytes,
  // so the beat counter already knows where the burst ends, and trusting a
  // `last` that disagreed with the count would let a short burst masquerade as
  // a whole one. Keep the frozen port type and silence the field-unused lint.
  /* verilator lint_off UNUSEDSIGNAL */
  logic unused_last;
  assign unused_last = hps_rsp_i.last;
  /* verilator lint_on UNUSEDSIGNAL */

endmodule

`default_nettype wire
