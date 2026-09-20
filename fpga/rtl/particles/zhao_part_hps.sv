// zhao_part_hps.sv -- PART.STATE's HPS DDR face: the particle generation
// store's ping-pong streamer.
//
// ENFORCED-BY: tests/particles/part_hps_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHAT THIS IS, AND WHY IT IS A BLOCK RATHER THAN WIRING
// ---------------------------------------------------------------------------
// design/contracts/PART.STATE.md is explicit about where a generation lives:
// "Dense sequential ping-pong in HPS DDR" and "Owns the two particle buffers in
// HPS DDR and nothing else on chip" -- 512 KiB each at the required tier of
// 32,768 x 16 B, which is 4 Mbit per buffer against the device's ~5.5 Mbit of
// M10K. So the store is NOT on-chip state by contract and could not be: the
// owner's M10K preference does not reach a 1 MiB pair.
//
// `zhao_part_state` streams records (`rd_*` in, `wr_*` out) and deliberately
// knows nothing about memory. Until 2026-09-19 the console left both streams at
// its edge (entry I1) and a bench fed made-up records. This block is the part
// that was missing between them and the bridge:
//
//   * TWO BUFFERS, SWAPPED PER TICK. The tick reads generation N from buffer
//     `cur` and writes generation N+1 densely into buffer `!cur`; the swap and
//     the new count land only when the last write burst has gone out, so a
//     tick is atomic at the buffer level exactly as the contract requires ("the
//     read buffer is never written and the write buffer is never read within
//     one tick").
//   * THE COUNT IS HARDWARE'S. The next generation's length is the number of
//     records PART.STATE wrote, counted here, because `zhao_part_state` has no
//     `wr_last` and its running totals are not per tick.
//   * 64-BYTE BURSTS, FOUR RECORDS EACH, one in flight (the bridge's law,
//     `zhao_pkg` "HPS-DDR bridge bursts"). A record is two 64-bit beats, low
//     half first, at byte address base + 16 * index.
//   * WRITES ARE POSTED into a bounded staging FIFO and the tick continues; a
//     full FIFO is backpressure on PART.STATE, never a dropped particle (the
//     contract's "If staging fills, the tick stalls rather than dropping").
//
// ---------------------------------------------------------------------------
// THE HPS SIDE -- configuration and the seed, in the `terr_cfg_*` shape (D10)
// ---------------------------------------------------------------------------
// The HPS allocates the two buffers (spec/memory_rules.md 5 defers particle
// pools to "the charter allocator") and writes their bases; in Verilator the
// harness IS the HPS, exactly as for TERRAIN.CMD's arena.
//
// The SEED is the one exchange the contract does not spell out, and it is kept
// to its minimum: {buffer, count} -- "buffer `seed_buf_i` holds `seed_count_i`
// valid records", accepted only while no tick is running. It is the
// population descriptor's `active_count` (spec/qformats.md 10) arriving at the
// only block that can use it. Without it nothing could ever be born: children
// come from parents, so a store that starts empty stays empty. The bases are
// latched WITH the seed so a tick can never run against half-written
// configuration. Both must be 64-byte aligned or the seed is refused.
//
// Until the first accepted seed there is no population and ticks are not passed
// on (counted in `ticks_unseeded_o`) -- a tick against unconfigured bases would
// write children to address zero.
//
// ---------------------------------------------------------------------------
// A BRIDGE REFUSAL -- ruling R54, 2026-09-19 evening
// ---------------------------------------------------------------------------
// Until this repair B_REQ waited for `hps_grant_i` and NOTHING ELSE, and the
// header argued at length that `zhao_hps_bridge`'s refusal (`err`, no grant,
// nothing issued) could not reach this block. The argument is still believed
// and it is not a guard: `zhao_hps_arbiter_n` re-serves a held request, so a
// refusal this state cannot see is an unbounded spin with every counter here
// frozen -- exactly the S1 defect repaired in MEM.UPLOAD and DEBUG.FRAMEBLIT
// at 525a3f6d. What is built now instead:
//
//   * `err` at B_REQ is COUNTED (`bridge_errs_o`) and the request is taken
//     DOWN. The bridge refuses a burst that is malformed or that collides with
//     a busy port; the second is transient, so the burst is re-offered, and
//     dropping the request for a cycle is what makes the re-offer a new
//     request rather than the same one being re-served.
//   * A refusal that REPEATS `ERR_RETRY_N` times on one burst is permanent by
//     definition, and the tick is FAULTED rather than retried forever.
//   * A FAULTED TICK IS TRUNCATED, NEVER HUNG and never fabricated. The DDR
//     side stops; `ps_tick_abort_o` ends PART.STATE's read stream (its
//     `tick_abort_i`); records still offered by PART.STATE are accepted and
//     DISCARDED (`records_discarded_o`) so it can finish; and the new
//     generation's length becomes `wr_iss_q` -- EXACTLY the records that
//     reached DDR, densely, from index zero. The buffers still swap, because
//     replaying the previous generation would age no particle and is a worse
//     lie than a shorter one. `ticks_faulted_o` counts it; `ticks_o` does not,
//     so "ticks completed" stays the number it has always been.
//
// ENFORCED-BY: tests/particles/part_hps_directed.cpp -- CASE H refuses ONE
// request the real way and requires the retry to succeed; CASE I refuses every
// request and requires the tick to end, the counters to move, and the store to
// accept the next tick.
//
// Conservative SystemVerilog subset only (charter 2).
`default_nettype none

module zhao_part_hps #(
    parameter int unsigned CAPACITY = 32768,
    parameter int unsigned CNT_W    = $clog2(CAPACITY) + 1,
    // Consecutive refusals of ONE burst before the tick is abandoned. Named
    // and editable: it is the boundary between "the bridge was busy" and "this
    // burst will never be accepted", and nothing in the protocol distinguishes
    // them, so it is a judgement and belongs in a knob.
    parameter int unsigned ERR_RETRY_N = 4,
    // Staging depths, in RECORDS. Eight is two bursts: one landing while the
    // other drains, which is what keeps one-in-flight from serialising the
    // tick on round-trip latency more than it must.
    parameter int unsigned RD_D     = 8,
    parameter int unsigned WR_D     = 8,
    // The bridge client tag this traffic carries. A parameter so the composer
    // states it; see the console core for the choice.
    parameter zhao_pkg::zhao_client_e CLIENT = zhao_pkg::ZHAO_CLIENT_ENGINE1
) (
    input var logic clk,
    input var logic rst_n,

    // ---- HPS configuration and the seed (D10) --------------------------------
    input  var logic [31:0]      cfg_base0_i,
    input  var logic [31:0]      cfg_base1_i,
    input  var logic             seed_valid_i,
    output var logic             seed_ready_o,
    input  var logic             seed_buf_i,
    input  var logic [CNT_W-1:0] seed_count_i,

    // ---- the console tick, and PART.STATE's tick handshake -------------------
    input  var logic             tick_i,
    output var logic             ps_tick_start_o,
    output var logic             ps_rd_empty_o,
    // The store cannot deliver the rest of this generation: end the read
    // stream. Held for the whole fault, see the refusal chapter above.
    output var logic             ps_tick_abort_o,
    input  var logic             ps_tick_done_i,

    // ---- the previous generation, to PART.STATE ------------------------------
    output var logic             rd_valid_o,
    input  var logic             rd_ready_i,
    output var logic [127:0]     rd_record_o,
    output var logic             rd_last_o,

    // ---- the next generation, from PART.STATE --------------------------------
    input  var logic             wr_valid_i,
    output var logic             wr_ready_o,
    input  var logic [127:0]     wr_record_i,

    // ---- MEM.HPS.BRIDGE client -------------------------------------------------
    output zhao_pkg::zhao_hps_burst_req_t hps_req_o,
    input  var logic                      hps_grant_i,
    input  zhao_pkg::zhao_hps_burst_rsp_t hps_rsp_i,
    output var logic                      hps_wr_valid_o,
    output var logic [63:0]               hps_wr_data_o,
    output var logic                      hps_wr_last_o,
    // The bridge's write-acceptance LEVEL: a beat moves only while it is high.
    input  var logic                      hps_wr_ready_i,

    // ---- state and counters --------------------------------------------------
    output var logic             busy_o,
    output var logic             cur_buf_o,
    output var logic [CNT_W-1:0] cur_count_o,
    output var logic [31:0]      ticks_o,
    output var logic [31:0]      ticks_dropped_o,    // a tick while one runs
    output var logic [31:0]      ticks_unseeded_o,   // a tick before any seed
    output var logic [31:0]      seeds_o,
    output var logic [31:0]      seeds_refused_o,    // count > CAPACITY or a misaligned base
    output var logic [31:0]      rd_bursts_o,
    output var logic [31:0]      wr_bursts_o,
    output var logic [31:0]      records_read_o,     // handed to PART.STATE
    output var logic [31:0]      records_written_o,  // taken from PART.STATE
    // ---- the bridge refusal (R54) ------------------------------------------
    output var logic [31:0]      bridge_errs_o,      // `err` seen at B_REQ
    output var logic [31:0]      ticks_faulted_o,    // ticks truncated by one
    output var logic [31:0]      records_discarded_o // taken and thrown away
);

  localparam int unsigned BURST_REC = 4;   // 64 B / 16 B
  localparam int unsigned RPW = $clog2(RD_D);
  localparam int unsigned WPW = $clog2(WR_D);
  localparam int unsigned ERW = $clog2(ERR_RETRY_N + 1);

  initial begin
    if (RD_D < 2 * BURST_REC || (RD_D & (RD_D - 1)) != 0)
      $fatal(1, "zhao_part_hps: RD_D=%0d must be a power of two of at least %0d", RD_D, 2 * BURST_REC);
    if (WR_D < 2 * BURST_REC || (WR_D & (WR_D - 1)) != 0)
      $fatal(1, "zhao_part_hps: WR_D=%0d must be a power of two of at least %0d", WR_D, 2 * BURST_REC);
    if (CNT_W < 3)
      $fatal(1, "zhao_part_hps: CNT_W=%0d is too narrow", CNT_W);
    if (ERR_RETRY_N < 1)
      $fatal(1, "zhao_part_hps: ERR_RETRY_N=%0d must be at least 1", ERR_RETRY_N);
  end

  // ---- the generation ----------------------------------------------------
  localparam logic [1:0] T_IDLE  = 2'd0;
  localparam logic [1:0] T_RUN   = 2'd1;
  localparam logic [1:0] T_FLUSH = 2'd2;
  localparam logic [1:0] T_FAULT = 2'd3;   // R54: the tick is being abandoned

  logic [1:0]       t_q;
  logic             seeded_q;
  logic             cur_q;
  logic [CNT_W-1:0] cnt_q;          // records in buffer `cur_q`
  logic [31:0]      base_q [2];

  logic [31:0]      rd_base_q, wr_base_q;
  logic [CNT_W-1:0] tick_cnt_q;     // this tick's read length
  logic [CNT_W-1:0] rd_req_q;       // records requested from DDR
  logic [CNT_W-1:0] rd_taken_q;     // records handed to PART.STATE
  logic [CNT_W-1:0] wr_cnt_q;       // records taken from PART.STATE
  logic [CNT_W-1:0] wr_iss_q;       // records sent to DDR

  // ---- staging ------------------------------------------------------------
  logic [127:0] rf_m [RD_D];
  logic [RPW:0] rf_wp_q, rf_rp_q;
  wire  [RPW:0] rf_occ_c = rf_wp_q - rf_rp_q;

  logic [127:0] wf_m [WR_D];
  logic [WPW:0] wf_wp_q, wf_rp_q;
  wire  [WPW:0] wf_occ_c = wf_wp_q - wf_rp_q;

  // ---- the one burst in flight -------------------------------------------
  localparam logic [1:0] B_IDLE = 2'd0;
  localparam logic [1:0] B_REQ  = 2'd1;   // request held until grant or err
  localparam logic [1:0] B_RD   = 2'd2;   // read beats landing
  localparam logic [1:0] B_WR   = 2'd3;   // write beats leaving

  logic [1:0]  b_q;
  logic        b_write_q;
  logic [2:0]  b_nrec_q;                  // 1..4 records
  logic [31:0] b_addr_q;
  logic [2:0]  b_beat_q;                  // beat index within the burst, 0..7
  logic [63:0] rd_lo_q;                   // the low half of a landing record

  // ---- the refusal (R54) --------------------------------------------------
  logic [ERW-1:0] err_run_q;              // consecutive refusals of THIS burst
  logic           fault_done_q;           // PART.STATE has ended its tick

  // ---- the seed ------------------------------------------------------------
  wire seed_aligned_c = (cfg_base0_i[5:0] == 6'd0) && (cfg_base1_i[5:0] == 6'd0);
  wire seed_ok_c      = seed_aligned_c && (seed_count_i <= CNT_W'(CAPACITY));
  assign seed_ready_o = (t_q == T_IDLE);
  wire seed_fire_c    = seed_valid_i && seed_ready_o;

  // ---- the tick ------------------------------------------------------------
  wire tick_go_c  = tick_i && (t_q == T_IDLE) && seeded_q && !seed_fire_c;
  assign ps_tick_start_o = tick_go_c;
  assign ps_rd_empty_o   = (cnt_q == '0);

  // ---- PART.STATE's two streams -------------------------------------------
  // A FAULTED tick offers no more records: the stream is ended by
  // `ps_tick_abort_o` instead, and anything still staged belongs to a
  // generation that will not be completed.
  assign rd_valid_o  = ((t_q == T_RUN) || (t_q == T_FLUSH)) && (rf_occ_c != '0);
  assign rd_record_o = rf_m[rf_rp_q[RPW-1:0]];
  assign rd_last_o   = (rd_taken_q + CNT_W'(1) == tick_cnt_q);
  wire   rd_fire_c   = rd_valid_o && rd_ready_i;

  // IN A FAULT THE WRITE SIDE STAYS OPEN AND THROWS THE RECORDS AWAY. Refusing
  // them would back PART.STATE up against a consumer that is never coming
  // back, which is the hang the fault exists to avoid.
  assign wr_ready_o  = ((t_q == T_RUN) && (wf_occ_c != (WPW+1)'(WR_D))) ||
                       (t_q == T_FAULT);
  wire   wr_fire_c   = wr_valid_i && wr_ready_o;
  wire   wr_take_c   = wr_fire_c && (t_q == T_RUN);
  wire   wr_drop_c   = wr_fire_c && (t_q == T_FAULT);

  assign ps_tick_abort_o = (t_q == T_FAULT);

  // ---- what to issue next --------------------------------------------------
  // WRITES FIRST once a whole burst is staged (or the tick is flushing its
  // tail): a full write FIFO is backpressure on PART.STATE, whose survivor
  // pass is what consumes the reads, so starving the writes would stall the
  // reads' consumer. Reads fill whatever room the read FIFO has for a burst.
  logic [CNT_W-1:0] rd_left_c;
  assign rd_left_c = tick_cnt_q - rd_req_q;

  logic [2:0] rd_n_c, wr_n_c;
  always_comb begin
    rd_n_c = (rd_left_c >= CNT_W'(BURST_REC)) ? 3'(BURST_REC) : rd_left_c[2:0];
    wr_n_c = (wf_occ_c >= (WPW+1)'(BURST_REC)) ? 3'(BURST_REC) : wf_occ_c[2:0];
  end

  wire want_wr_c = (t_q != T_FAULT) &&
                   ((wf_occ_c >= (WPW+1)'(BURST_REC)) ||
                    ((t_q == T_FLUSH) && (wf_occ_c != '0)));
  wire want_rd_c = (t_q != T_IDLE) && (t_q != T_FAULT) && (rd_left_c != '0) &&
                   ((RPW+1)'(RD_D) - rf_occ_c >= (RPW+1)'(rd_n_c));

  // ---- the bridge request --------------------------------------------------
  always_comb begin
    hps_req_o        = '0;
    hps_req_o.valid  = (b_q == B_REQ);
    hps_req_o.write  = b_write_q;
    hps_req_o.client = CLIENT;
    hps_req_o.addr   = b_addr_q;
    hps_req_o.len    = {b_nrec_q, 4'd0};       // 16 B per record, 16..64
  end

  // Write beats: low half of the head record, then its high half.
  wire [127:0] wf_head_c = wf_m[wf_rp_q[WPW-1:0]];
  // OFFERED ONLY WHILE THE BRIDGE WILL TAKE IT. `zhao_hps_bridge` raises its
  // grant at ACCEPTANCE and consumes beats only once the HPS has issued the
  // burst; a beat offered in between is not consumed and is counted against
  // the client in `wr_early_beats`. So validity follows the level rather than
  // the grant, and a beat advances on the same condition that makes it valid.
  assign hps_wr_valid_o = (b_q == B_WR) && hps_wr_ready_i;
  assign hps_wr_data_o  = b_beat_q[0] ? wf_head_c[127:64] : wf_head_c[63:0];
  assign hps_wr_last_o  = (b_q == B_WR) && ({1'b0, b_beat_q} == {b_nrec_q, 1'b0} - 4'd1);
  wire   wbeat_fire_c   = (b_q == B_WR) && hps_wr_ready_i;

  wire   rbeat_c        = (b_q == B_RD) && hps_rsp_i.beat_valid;

  assign busy_o      = (t_q != T_IDLE);
  assign cur_buf_o   = cur_q;
  assign cur_count_o = cnt_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      t_q        <= T_IDLE;
      seeded_q   <= 1'b0;
      cur_q      <= 1'b0;
      cnt_q      <= '0;
      base_q[0]  <= 32'd0;
      base_q[1]  <= 32'd0;
      rd_base_q  <= 32'd0;
      wr_base_q  <= 32'd0;
      tick_cnt_q <= '0;
      rd_req_q   <= '0;
      rd_taken_q <= '0;
      wr_cnt_q   <= '0;
      wr_iss_q   <= '0;
      rf_wp_q    <= '0;
      rf_rp_q    <= '0;
      wf_wp_q    <= '0;
      wf_rp_q    <= '0;
      b_q        <= B_IDLE;
      b_write_q  <= 1'b0;
      b_nrec_q   <= 3'd0;
      b_addr_q   <= 32'd0;
      b_beat_q   <= 3'd0;
      rd_lo_q    <= 64'd0;
      err_run_q    <= '0;
      fault_done_q <= 1'b0;
      bridge_errs_o       <= 32'd0;
      ticks_faulted_o     <= 32'd0;
      records_discarded_o <= 32'd0;
      ticks_o           <= 32'd0;
      ticks_dropped_o   <= 32'd0;
      ticks_unseeded_o  <= 32'd0;
      seeds_o           <= 32'd0;
      seeds_refused_o   <= 32'd0;
      rd_bursts_o       <= 32'd0;
      wr_bursts_o       <= 32'd0;
      records_read_o    <= 32'd0;
      records_written_o <= 32'd0;
    end else begin
      // ---- seed ------------------------------------------------------------
      if (seed_fire_c) begin
        if (seed_ok_c) begin
          seeded_q  <= 1'b1;
          cur_q     <= seed_buf_i;
          cnt_q     <= seed_count_i;
          base_q[0] <= cfg_base0_i;
          base_q[1] <= cfg_base1_i;
          seeds_o   <= seeds_o + 32'd1;
        end else begin
          seeds_refused_o <= seeds_refused_o + 32'd1;
        end
      end

      // ---- tick arrival ----------------------------------------------------
      if (tick_i) begin
        if (tick_go_c) begin
          t_q        <= T_RUN;
          rd_base_q  <= base_q[cur_q];
          wr_base_q  <= base_q[~cur_q];
          tick_cnt_q <= cnt_q;
          rd_req_q   <= '0;
          rd_taken_q <= '0;
          wr_cnt_q   <= '0;
          wr_iss_q   <= '0;
          err_run_q    <= '0;
          fault_done_q <= 1'b0;
        end else if (!seeded_q || seed_fire_c) begin
          ticks_unseeded_o <= ticks_unseeded_o + 32'd1;
        end else begin
          ticks_dropped_o <= ticks_dropped_o + 32'd1;
        end
      end

      // ---- PART.STATE's streams ---------------------------------------------
      if (rd_fire_c) begin
        rf_rp_q        <= rf_rp_q + (RPW+1)'(1);
        rd_taken_q     <= rd_taken_q + CNT_W'(1);
        records_read_o <= records_read_o + 32'd1;
      end
      if (wr_take_c) begin
        wf_m[wf_wp_q[WPW-1:0]] <= wr_record_i;
        wf_wp_q           <= wf_wp_q + (WPW+1)'(1);
        wr_cnt_q          <= wr_cnt_q + CNT_W'(1);
        records_written_o <= records_written_o + 32'd1;
      end
      // R54: taken so PART.STATE can finish, and thrown away. NOT counted as
      // written -- `records_written_o` is what went into a generation.
      if (wr_drop_c) records_discarded_o <= records_discarded_o + 32'd1;

      // ---- the tick's end -------------------------------------------------
      // PART.STATE raises `tick_done` only after its last write was accepted
      // HERE, so everything it will write is staged; what remains is the tail.
      if ((t_q == T_RUN) && ps_tick_done_i) t_q <= T_FLUSH;
      if ((t_q == T_FLUSH) && (wf_occ_c == '0) && (b_q == B_IDLE)) begin
        t_q     <= T_IDLE;
        cur_q   <= ~cur_q;
        cnt_q   <= wr_cnt_q;
        ticks_o <= ticks_o + 32'd1;
      end

      // R54: the faulted tick ends when PART.STATE has finished (its read
      // stream was ended by `ps_tick_abort_o`) and nothing is in flight. The
      // new generation is EXACTLY what reached DDR.
      if (t_q == T_FAULT) begin
        if (ps_tick_done_i) fault_done_q <= 1'b1;
        if ((fault_done_q || ps_tick_done_i) && (b_q == B_IDLE)) begin
          t_q             <= T_IDLE;
          cur_q           <= ~cur_q;
          cnt_q           <= wr_iss_q;
          ticks_faulted_o <= ticks_faulted_o + 32'd1;
        end
      end

      // ---- the burst engine -----------------------------------------------
      case (b_q)
        B_IDLE: begin
          b_beat_q <= 3'd0;
          if (want_wr_c) begin
            b_q       <= B_REQ;
            b_write_q <= 1'b1;
            b_nrec_q  <= wr_n_c;
            b_addr_q  <= wr_base_q + 32'({wr_iss_q, 4'd0});
          end else if (want_rd_c) begin
            b_q       <= B_REQ;
            b_write_q <= 1'b0;
            b_nrec_q  <= rd_n_c;
            b_addr_q  <= rd_base_q + 32'({rd_req_q, 4'd0});
          end
        end

        // HELD UNTIL GRANTED OR REFUSED (R54). The bridge refuses a malformed
        // burst or one that collides with a busy port (`zhao_hps_bridge.sv`
        // 105-147) with `err` and NO grant. This block is believed to be
        // incapable of the first -- bases are refused at the seed unless
        // 64-byte aligned and every burst starts on a four-record boundary --
        // and `zhao_hps_arbiter_n` pulses the bridge only from A_IDLE, so the
        // second should not reach it either. That belief used to BE the
        // handling, and it is now only the reason `bridge_errs_o` is expected
        // to read zero in the console. The handling is below, because the
        // arbiter re-serves a held request: a refusal this state could not see
        // was an unbounded spin.
        B_REQ: begin
          if (hps_rsp_i.err) begin
            bridge_errs_o <= bridge_errs_o + 32'd1;
            b_q           <= B_IDLE;   // take the request DOWN, then re-offer
            if (ERW'(err_run_q + ERW'(1)) >= ERW'(ERR_RETRY_N)) begin
              // Permanent by definition: this burst has been refused
              // ERR_RETRY_N times running. Abandon the tick.
              t_q                 <= T_FAULT;
              fault_done_q        <= (t_q == T_FLUSH) || ps_tick_done_i;
              rf_rp_q             <= rf_wp_q;   // staged reads are moot
              wf_rp_q             <= wf_wp_q;   // staged writes never go out
              records_discarded_o <= records_discarded_o + 32'(wf_occ_c);
              rd_req_q            <= tick_cnt_q;
              err_run_q           <= '0;
            end else begin
              err_run_q <= ERW'(err_run_q + ERW'(1));
            end
          end else if (hps_grant_i) begin
            b_q       <= b_write_q ? B_WR : B_RD;
            err_run_q <= '0;
            if (b_write_q) wr_bursts_o <= wr_bursts_o + 32'd1;
            else           rd_bursts_o <= rd_bursts_o + 32'd1;
          end
        end

        // The bridge answers `err` only at the REQUEST (a malformed or
        // colliding burst, `zhao_hps_bridge.sv` 142-146) -- never once a burst
        // is granted -- so a granted read simply runs to its last beat. Every
        // burst this block asks for starts on a 64-byte boundary: bases are
        // refused unless aligned, and a burst starts at a record index that is
        // a multiple of four (a short burst is only ever a generation's tail).
        B_RD: begin
          if (rbeat_c) begin
            if (!b_beat_q[0]) begin
              rd_lo_q <= hps_rsp_i.data;
            end else begin
              rf_m[rf_wp_q[RPW-1:0]] <= {hps_rsp_i.data, rd_lo_q};
              rf_wp_q  <= rf_wp_q + (RPW+1)'(1);
              rd_req_q <= rd_req_q + CNT_W'(1);
            end
            b_beat_q <= b_beat_q + 3'd1;
            if (hps_rsp_i.last) b_q <= B_IDLE;
          end
        end

        B_WR: begin
          if (wbeat_fire_c) begin
            b_beat_q <= b_beat_q + 3'd1;
            if (b_beat_q[0]) begin
              wf_rp_q  <= wf_rp_q + (WPW+1)'(1);
              wr_iss_q <= wr_iss_q + CNT_W'(1);
            end
            if (hps_wr_last_o) b_q <= B_IDLE;
          end
        end

        default: b_q <= B_IDLE;
      endcase
    end
  end

endmodule : zhao_part_hps

`default_nettype wire
