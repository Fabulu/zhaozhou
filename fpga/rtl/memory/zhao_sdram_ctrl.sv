// zhao_sdram_ctrl.sv — synthesizable SDR SDRAM controller core (plan W2.5,
// D2). Law: spec/memory_rules.md §1, contract design/contracts/MEM.SDRAM.md.
//
// MEM.SDRAM stays SPECIFIED (blocked_on: hardware): this core is verified
// against the conservative sim profile ONLY (zhao_sdram_params_pkg); board
// truth arrives via ZH-004 (the params-package include seam). Evidence is
// banked in the ledger maturity_log; ZH-004 obligations: memory_rules §1.
//
// ===========================================================================
// FROZEN LAW TABLE (mirrored cycle-for-cycle by zref::SdramController and the
// behavioural model sim/models/zhao_sdram_model.sv).
//   * A command is a REGISTERED output presented during a cycle and sampled
//     by the DRAM at that cycle's ending edge. rsp.grant is REGISTERED: it
//     reads high during cycle G, the first command cycle of the burst that
//     was accepted at the edge ending cycle G-1.
//   * Timing params count the gap between presented commands (tRCD=3 means
//     READ_cycle >= ACT_cycle + 3).
//   * Open page (rows stay open; a row conflict pays PRE + tRP):
//       hit      : READ/WRITE command at G
//       miss     : ACT at G, READ/WRITE at G+3
//       conflict : PRE(bank) at G, ACT at G+3, READ/WRITE at G+6
//   * READ (CAS=3): DRAM drives DQ during [R+3, R+10] (8 beats); the ctrl
//     samples `words` of them (rdata_valid high during [R+4, R+3+words])
//     but WAITS the full 8-beat bus burst (bus shaping). The next grant can
//     read high at R+12 (credit return at the last sampled beat).
//   * WRITE (write latency 1): the ctrl drives DQ during [R+1, R+8] (8
//     beats; beats >= `words` are DQM-masked, so partial bursts write
//     nothing beyond the requested words). The next grant can read high at
//     R+10.
//   * Grant-to-grant spans (words=8): read 12/15/18 (hit/miss/conflict),
//     write 10/13/16. MAX_BURST_SPAN = 18.
//   * REFRESH: PRECHARGE-ALL at P, AUTO_REFRESH at P+3, bus released after
//     12 occupied cycles; the next grant reads high at P+13. A refresh
//     closes every row => the first burst after it is always a miss.
//   * refresh_cnt counts cycles since the last AUTO_REFRESH (presented).
//       cnt >= 780 : refresh at the next boundary if no request is offered
//       cnt >= 820 : refresh preempts requests, unless hold_refresh (the
//                    arbiter's aging override is firing — liveness first)
//       cnt >= 840 : refresh preempts everything (one override deferral
//                    bound; formal mem_sdram_refresh_bound asserts the
//                    register never exceeds this)
// ===========================================================================
//
// Port notes:
//   * req.len carries WORDS (1..8) at this edge (the arbiter splits client
//     byte requests into <=8-word bursts); 0 encodes 8.
//   * rsp.credits reads nonzero the cycle after a burst's last DRAM beat,
//     returning the retired word count (credit-based edge law, D3).
//   * wr_beat pulses the cycle BEFORE each word is needed on phy_dq_o:
//     beat k is requested during R+k and presented during R+k+1.
//
// Conservative SystemVerilog subset only (charter §2). Lint: clean under
// `verilator --lint-only -Wall` (lint_mem_sdram_ctrl CTest).

module zhao_sdram_ctrl
  import zhao_pkg::*;
  import zhao_sdram_params_pkg::*;
(
  input  logic        clk,
  input  logic        rst_n,

  // credit client port (driven by MEM.VRAM.ARBITER)
  input  zhao_arb_req_t req,
  output zhao_arb_rsp_t rsp,

  // the arbiter's aging-override boundary: urgent (not hard) refresh defers
  input  logic        hold_refresh,

  // write data path
  input  logic [15:0] wdata,
  output logic        wr_beat,
  // read data path
  output logic [15:0] rdata,
  output logic        rdata_valid,

  // SDR PHY pins (behavioural model in sim; real pins in hardware)
  output logic        phy_cs_n,
  output logic        phy_ras_n,
  output logic        phy_cas_n,
  output logic        phy_we_n,
  output logic [12:0] phy_a,
  output logic [1:0]  phy_ba,
  output logic [15:0] phy_dq_o,
  output logic        phy_dq_oe,
  output logic [1:0]  phy_dqm,
  input  logic [15:0] phy_dq_i,

  // status / counters (catalog: sdram_refresh_stalls, sdram_bank_conflicts)
  output logic        init_done,
  output logic [31:0] refresh_stalls,
  output logic [31:0] bank_conflicts,
  output logic        refresh_pulse   // reads high while AUTO_REFRESH is presented
);

  // ---------------------------------------------------------------- states --
  typedef enum logic [3:0] {
    S_INIT_PRE  = 4'd0,   // PRECHARGE-ALL
    S_INIT_RP   = 4'd1,   // wait tRP
    S_INIT_REF1 = 4'd2,   // AUTO_REFRESH #1
    S_INIT_RC1  = 4'd3,   // wait tRC
    S_INIT_REF2 = 4'd4,   // AUTO_REFRESH #2
    S_INIT_RC2  = 4'd5,   // wait tRC
    S_INIT_MRS  = 4'd6,   // MODE REGISTER SET (CAS=3, BL=8, sequential)
    S_READY     = 4'd7,   // idle: an offered request is accepted at this edge
    S_PRE       = 4'd8,   // PRECHARGE(bank) — row conflict path
    S_ACT       = 4'd9,   // ACTIVATE
    S_TRCD      = 4'd10,  // wait tRCD (2 cycles; R/W at ACT+3)
    S_RW        = 4'd11,  // READ / WRITE command cycle R
    S_CAS       = 4'd12,  // wait CAS-1 after READ
    S_RDATA     = 4'd13,  // sample read beats [R+3, R+2+words]
    S_WDATA     = 4'd14,  // drive 8 write beats [R+1, R+8] (DQM-masked tail)
    S_REF_PRE   = 4'd15   // refresh: PRE-ALL, REF at P+3, release at P+12
  } state_e;

  state_e    state;
  logic [3:0] t;                  // intra-sequence counter
  logic [3:0] beat;               // beat index within the data phase
  logic       grant_q;            // registered grant pulse (cycle G)

  logic        cur_write;
  logic [1:0]  cur_bank;
  logic [12:0] cur_row;
  logic [10:0] cur_col;
  logic [3:0]  cur_words;         // 1..8

  logic [12:0] open_row  [0:3];
  logic [3:0]  open_valid;

  logic [31:0] refresh_cnt;
  logic        ref_pending, ref_urgent, ref_hard;
  logic        ref_under_load;  // this refresh preempted client traffic
  logic        prev_busy_q;     // last cycle served a burst (traffic in flight)

  // ------------------------------------------------------------- geometry ---
  logic [25:0] waddr;
  logic [1:0]  req_bank;
  logic [12:0] req_row;
  logic [10:0] req_col;
  logic        req_hit, req_conflict;

  assign waddr        = req.addr[26:1];
  assign req_bank     = waddr[25:24];
  assign req_row      = waddr[23:11];
  assign req_col      = waddr[10:0];
  assign req_hit      = open_valid[req_bank] && (open_row[req_bank] == req_row);
  assign req_conflict = open_valid[req_bank] && (open_row[req_bank] != req_row);

  localparam logic [31:0] CNT_PENDING = 32'(REFRESH_INTERVAL);                             // 780
  localparam logic [31:0] CNT_URGENT  = 32'(REFRESH_INTERVAL + REFRESH_URGENT);            // 820
  localparam logic [31:0] CNT_HARD    = 32'(REFRESH_INTERVAL + REFRESH_URGENT + MAX_BURST_SPAN + 1); // 840
  assign ref_pending = (refresh_cnt >= CNT_PENDING) && !ref_hard;
  assign ref_urgent  = (refresh_cnt >= CNT_URGENT)  && !ref_hard;
  assign ref_hard    = (refresh_cnt >= CNT_HARD);

  // refresh takes this boundary? idle refresh / urgent preempt / hard preempt
  logic refresh_now;
  assign refresh_now = (state == S_READY)
    && (ref_hard
        || (ref_urgent && !hold_refresh)
        || (ref_pending && !req.valid));

  // grant: the request is accepted at the edge ending a READY cycle
  logic accept;
  assign accept = (state == S_READY) && req.valid && !refresh_now;
  assign rsp.grant = grant_q;

  // ------------------------------------------------------- command decode ---
  // SDR truth table (CS active low): ACT 0,1,1 | READ 1,0,1 | WRITE 1,0,0
  // PRE 0,1,0 | REF 0,0,1 | MRS 0,0,0  (ras,cas,we)
  logic cmd_act, cmd_read, cmd_write, cmd_pre, cmd_ref, cmd_mrs, pre_all;

  always_comb begin
    cmd_act   = 1'b0;
    cmd_read  = 1'b0;
    cmd_write = 1'b0;
    cmd_pre   = 1'b0;
    cmd_ref   = 1'b0;
    cmd_mrs   = 1'b0;
    pre_all   = 1'b0;
    unique case (state)
      S_INIT_PRE:  begin cmd_pre = 1'b1; pre_all = 1'b1; end
      S_INIT_REF1,
      S_INIT_REF2: cmd_ref = (t == 4'd0);
      S_INIT_MRS:  cmd_mrs = 1'b1;
      // PRECHARGE decodes ONLY on the first wait cycle (re-presenting it
      // would restart the DRAM's tRP window every cycle)
      S_PRE:       cmd_pre = (t == 4'd0);
      S_ACT:       cmd_act = 1'b1;
      S_RW:        if (cur_write) cmd_write = 1'b1; else cmd_read = 1'b1;
      S_REF_PRE: begin
        if (t == T_RP[3:0])        cmd_ref = 1'b1;
        else if (t == 4'd0) begin  cmd_pre = 1'b1; pre_all = 1'b1; end
      end
      default: ;
    endcase
  end

  assign phy_cs_n  = 1'b0;
  assign phy_ras_n = !(cmd_act || cmd_pre || cmd_ref || cmd_mrs);
  assign phy_cas_n = !(cmd_read || cmd_write || cmd_ref || cmd_mrs);
  assign phy_we_n  = !(cmd_write || cmd_pre || cmd_mrs);
  assign phy_ba    = cur_bank;
  assign phy_a     = cmd_pre  ? {2'b00, pre_all, 10'b0}
                    : cmd_act ? {cur_row}
                    : cmd_mrs  ? 13'b0000_00_011_0_011   // A[2:0]=BL8, A3=seq, A[6:4]=CAS3
                              : {2'b00, cur_col};        // READ/WRITE column, A10=0
  assign phy_dq_oe = (state == S_WDATA);

  // write beat requests: beat k during R+k (presented R+k+1); the SDR burst
  // is always 8 beats, tail beats >= `words` are DQM-masked
  assign wr_beat = cur_write
    && ((state == S_RW)                                     // word 0 (presented R+1)
        || (state == S_WDATA && (beat + 4'd1 < cur_words))); // word beat+1
  assign phy_dqm = (state == S_WDATA && (beat >= cur_words)) ? 2'b11 : 2'b00;

  // retirement: last DRAM beat of the burst
  logic retire;
  assign retire = (state == S_RDATA && (beat == cur_words - 4'd1))
               || (state == S_WDATA && (beat == 4'd7));
  // NOTE: a read BURST occupies the DQ bus for the full SDR burst length
  // (8 beats) even when the client consumes fewer words — S_RDATA below
  // waits for beat 7 (bus shaping; a following WRITE would otherwise
  // collide with the tail beats, which the behavioural model checks).

  logic       credits_valid;
  logic [3:0] credits_cnt;

  // -------------------------------------------------------------- seq core --
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state          <= S_INIT_PRE;
      t              <= 4'd0;
      beat           <= 4'd0;
      grant_q        <= 1'b0;
      cur_write      <= 1'b0;
      cur_bank       <= 2'd0;
      cur_row        <= 13'd0;
      cur_col        <= 11'd0;
      cur_words      <= 4'd0;
      open_valid     <= 4'b0;
      open_row[0]    <= 13'd0;
      open_row[1]    <= 13'd0;
      open_row[2]    <= 13'd0;
      open_row[3]    <= 13'd0;
      refresh_cnt    <= 32'd0;
      refresh_stalls <= 32'd0;
      ref_under_load <= 1'b0;
      prev_busy_q    <= 1'b0;
      bank_conflicts <= 32'd0;
      init_done      <= 1'b0;
      rdata_valid    <= 1'b0;
      rdata          <= 16'd0;
      phy_dq_o       <= 16'd0;
      credits_valid  <= 1'b0;
      credits_cnt    <= 4'd0;
    end else begin
      // defaults: one-cycle pulses
      grant_q       <= 1'b0;
      rdata_valid   <= 1'b0;
      credits_valid <= 1'b0;

      // refresh accounting (cleared while REF is presented, post-init only)
      if (state == S_INIT_MRS)            refresh_cnt <= 32'd0;
      else if (init_done && cmd_ref)      refresh_cnt <= 32'd0;
      else if (init_done)                 refresh_cnt <= refresh_cnt + 32'd1;

      // refresh steals: a refresh that preempted client traffic counts its
      // full REFRESH_OVERHEAD (the offer may retract for a cycle mid-
      // sequence, so counting req.valid per cycle would undercount)
      // "under load" = a client offer is present OR the previous cycle served
      // a burst (the arbiter's offer retracts for one cycle after a grant,
      // so req.valid alone would miss refreshes at back-to-back boundaries)
      prev_busy_q <= init_done && (state != S_READY);
      if ((state == S_READY) && refresh_now && (req.valid || prev_busy_q))
        ref_under_load <= 1'b1;
      else if (state == S_READY) ref_under_load <= 1'b0;
      // (the sequence occupies exactly REFRESH_OVERHEAD = tRP + tRC = 12
      //  cycles: one increment per cycle == 12 per preempted refresh)
      if (state == S_REF_PRE && ref_under_load)
        refresh_stalls <= refresh_stalls + 32'd1;

      // write data latching (beat k requested during R+k)
      if (wr_beat) phy_dq_o <= wdata;

      // read beat sampling ([R+3, R+2+words]); rdata_valid during R+4..R+3+words
      if (state == S_RDATA && (beat < cur_words)) begin
        rdata       <= phy_dq_i;
        rdata_valid <= 1'b1;
      end

      // credit return the cycle after the last beat
      if (retire) begin
        credits_valid <= 1'b1;
        credits_cnt   <= cur_words;
      end

      case (state)
        S_INIT_PRE: begin t <= 4'd0; state <= S_INIT_RP; end
        // (t MUST reset on these transitions: the bottom-of-block increment
        //  deliberately skips the transition cycle, so without this the first
        //  AUTO_REFRESH would enter with t=2, run short, and never decode)
        S_INIT_RP: begin
          if (t == T_RP[3:0] - 4'd1) begin t <= 4'd0; state <= S_INIT_REF1; end
          else state <= S_INIT_RP;
        end
        S_INIT_REF1: state <= (t == T_RC[3:0] - 4'd1) ? S_INIT_RC1 : S_INIT_REF1;
        S_INIT_RC1: begin t <= 4'd0; state <= S_INIT_REF2; end
        S_INIT_REF2: state <= (t == T_RC[3:0] - 4'd1) ? S_INIT_RC2 : S_INIT_REF2;
        S_INIT_RC2: begin t <= 4'd0; state <= S_INIT_MRS; end
        S_INIT_MRS: begin t <= 4'd0; state <= S_READY; init_done <= 1'b1; end
        S_READY: begin
          t    <= 4'd0;
          beat <= 4'd0;
          if (refresh_now) begin
            state <= S_REF_PRE;
          end else if (accept) begin
            grant_q   <= 1'b1;
            cur_write <= req.write;
            cur_bank  <= req_bank;
            cur_row   <= req_row;
            cur_col   <= req_col;
            cur_words <= (req.len[3:0] == 4'd0) ? 4'd8 : req.len[3:0];
            if (req_conflict) begin
              bank_conflicts       <= bank_conflicts + 32'd1;
              open_valid[req_bank] <= 1'b0;
              state                <= S_PRE;
            end else if (!req_hit) begin
              state <= S_ACT;
            end else begin
              state <= S_RW;
            end
          end
        end
        S_PRE: state <= (t == T_RP[3:0] - 4'd1) ? S_ACT : S_PRE;  // PRE at G, ACT at G+3
        S_ACT: begin
          open_valid[cur_bank] <= 1'b1;
          open_row[cur_bank]   <= cur_row;
          t    <= 4'd0;
          beat <= 4'd0;
          state <= S_TRCD;                                       // ACT at G
        end
        S_TRCD: state <= (t == T_RCD[3:0] - 4'd2) ? S_RW : S_TRCD; // R/W at ACT+3
        S_RW: begin
          t    <= 4'd0;
          beat <= 4'd0;
          state <= cur_write ? S_WDATA : S_CAS;                  // R/W at R
        end
        S_CAS: state <= (t == CAS_LATENCY[3:0] - 4'd2) ? S_RDATA : S_CAS; // data at R+3
        S_RDATA: begin
          if (beat == 4'd7) begin
            t    <= 4'd0;
            beat <= 4'd0;
            state <= S_READY;            // READY during R+11 (full bus burst)
          end
        end
        S_WDATA: begin
          if (beat == 4'd7) begin
            t    <= 4'd0;
            beat <= 4'd0;
            state <= S_READY;            // READY during R+9
          end
        end
        S_REF_PRE: begin
          if (t == T_RC[3:0] + T_RP[3:0] - 4'd1) begin  // REF at P+3, READY at P+12
            t         <= 4'd0;
            open_valid <= 4'b0;          // PRECHARGE-ALL closed every row
            state     <= S_READY;
          end
        end
        default: state <= S_READY;
      endcase

      // counters t / beat advance where the case arm did not assign them
      if (state == S_INIT_RP  && !(t == T_RP[3:0] - 4'd1))  t <= t + 4'd1;
      if (state == S_INIT_REF1 && !(t == T_RC[3:0] - 4'd1)) t <= t + 4'd1;
      if (state == S_INIT_REF2 && !(t == T_RC[3:0] - 4'd1)) t <= t + 4'd1;
      if (state == S_PRE       && !(t == T_RP[3:0] - 4'd1)) t <= t + 4'd1;
      if (state == S_TRCD      && !(t == T_RCD[3:0] - 4'd2)) t <= t + 4'd1;
      if (state == S_CAS       && !(t == CAS_LATENCY[3:0] - 4'd2)) t <= t + 4'd1;
      if (state == S_REF_PRE   && !(t == T_RC[3:0] + T_RP[3:0] - 4'd1)) t <= t + 4'd1;
      if ((state == S_RDATA && !(beat == 4'd7))
          || (state == S_WDATA && !(beat == 4'd7)))
        beat <= beat + 4'd1;
    end
  end

  assign rsp.credits   = credits_valid ? {4'b0, credits_cnt} : 8'd0;
  assign refresh_pulse = cmd_ref && init_done;

  // req.client carries no policy at this edge (the arbiter owns ids); the
  // port type is frozen (zhao_pkg) — consume the field to keep lint honest.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [2:0] unused_req_client;
  assign unused_req_client = req.client;
  /* verilator lint_on UNUSEDSIGNAL */

  // req.client carries no policy at this edge (the arbiter owns ids); keep
  // the port type frozen (zhao_pkg) and silence the field-unused lint here.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [ZHAO_VRAM_ADDR_BITS-1:0] unused_addr_msb;
  assign unused_addr_msb = req.addr;
  /* verilator lint_on UNUSEDSIGNAL */

endmodule
