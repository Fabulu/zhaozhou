// zhao_cmd_scheduler.sv — CMD.SCHEDULER, the 3-slot frame ownership FSM
// (plan W2.6, decision D8).
//
// Law (in citation order):
//   ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md 7.4 — the slot FSM
//       FREE -> ARM_WRITING -> READY -> FPGA_RUNNING -> DONE -> FREE:
//       forward-only, one owner per slot, the charter state names.
//   design/contracts/CMD.SCHEDULER.md — the block contract.
//   spec/memory_rules.md 4.1 — FRAME_RING descriptor law: the HPS owns
//       FREE->ARM_WRITING->READY (observed here through the hps_state word
//       view); the FPGA posts DONE (word 3) at FPGA_RUNNING->DONE and
//       returns DONE->FREE (word 0) at the next frame_tick. The word is NOT
//       rewritten at claim: the charter forbids the producer touching a
//       sealed (READY) slot, so READY-word + FPGA ownership is the claimed
//       state; DONE is the HPS-visible "fence posted" marker.
//   spec/counters.md (D9) — frame_cycles / deadline_faults / commands are
//       owned HERE as local registers; the broadcast frame_tick latches the
//       u64 shadows; no global event bus.
//   spec/video_rules.md 1 (D1/D6) — the deadline default is the ACTIVE
//       mode's frame period (zhao_pkg ZHAO_TIMING); BeginFrame
//       .deadline_cycles (nonzero) overrides; the mode register latches
//       only at frame_tick (effective next frame, never mid-frame).
//
// Fence law: EXACTLY ONE completion fence pulse per FPGA_RUNNING -> DONE
// transition — success (frame_complete at the frame boundary), CMD.DMA
// verdict fault, deadline miss, or display-window miss. A late seal never
// reaches FPGA_RUNNING, so a repeated (late) frame NEVER fences. Proven by
// tests/formal/cmd_scheduler_slot_fsm.sby (BMC, read_slang).
//
// Status space: 0..14 = zhao_abi_error (ABI law); 15/16 are module-local
// extensions shared with zref_cmd2.hpp (15 epoch mismatch, 16 deadline
// miss) — they never appear on the wire.
//
// Claim window: at most ONE claim per inter-tick interval (and at most one
// slot in FPGA_RUNNING — Phase 2 runs one frame pipeline), so a deadline
// fault cannot chain-claim inside the same frame. The Verilator harness
// drives the HPS word view (harness-as-HPS, plan D10).
//
// The evaluation order of the always_ff below mirrors zref::CmdScheduler
// step() statement-for-statement — never reorder one side alone (the
// cmd_random differential compares every observable every cycle).
//
// Conservative SystemVerilog subset only (charter 2).
// Lint: clean under `verilator_bin --lint-only -Wall` (lint_cmd_scheduler).

module zhao_cmd_scheduler
#(
  // FRAME_RING base in HPS DDR (memory_rules.md 4.1; the 4-KiB descriptor
  // table precedes the 3 x 1-MiB slot bodies)
  parameter logic [31:0] RING_BASE = 32'h0000_0000
) (
  input  logic clk,
  input  logic rst_n,

  // ---- harness-as-HPS ring word view (per slot; D10) -----------------------
  // word law memory_rules.md 4.1: 0=FREE 1=ARM_WRITING 2=READY 3=DONE
  input  logic [1:0]  hps_state_i [0:2],
  input  logic [31:0] hps_byte_len_i [0:2],  // descriptor byte_len (sealed len)

  // ---- FPGA-owned state-word writes back into the ring ---------------------
  output logic       ring_wr_valid_o,
  output logic [1:0] ring_wr_slot_o,
  output logic [1:0] ring_wr_state_o,  // 3 = DONE, 0 = FREE
  input  logic       ring_wr_ready_i,

  // ---- packet fetch request to CMD.DMA (on claim) --------------------------
  output logic        fetch_req_valid_o,
  input  logic        fetch_req_ready_i,
  output logic [1:0]  fetch_slot_o,
  output logic [31:0] fetch_addr_o,     // slot body base (HPS byte address)
  output logic [31:0] fetch_byte_len_o,
  output logic [31:0] fetch_epoch_o,    // the CURRENT epoch at claim

  // ---- CMD.DMA verdict for the claimed slot --------------------------------
  input  logic       dma_done_i,
  input  logic [1:0] dma_slot_i,
  input  logic [7:0] dma_status_i,      // 0 = OK (abi error / local 15/16)

  // ---- decoded record stream (post-verification, Phase-2 opcodes) ---------
  // rec_w0_i[31:24] unused: the Phase-2 records pack sub-32-bit fields into
  // w0's low bytes (video_mode u8s, pad/en/strength u8s); the high byte is
  // declared pad by the ABI and consumed (zero-checked) upstream in DMA.
  /* verilator lint_off UNUSEDSIGNAL */
  input  logic        rec_valid_i,
  /* verilator lint_on UNUSEDSIGNAL */
  output logic        rec_ready_o,
  input  logic [15:0] rec_opcode_i,
  /* verilator lint_off UNUSEDSIGNAL */
  input  logic [31:0] rec_w0_i,
  /* verilator lint_on UNUSEDSIGNAL */
  input  logic [31:0] rec_w1_i,
  input  logic [31:0] rec_w2_i,
  input  logic [31:0] rec_w3_i,

  // ---- frame boundary from VIDEO.FRAMECTL ----------------------------------
  // frame_tick_i.frame_id not consumed here (the deadline/repeat machinery
  // keys on the pulse + repeated bits; frame_id lives in the counter set)
  /* verilator lint_off UNUSEDSIGNAL */
  input  zhao_pkg::zhao_frame_tick_t frame_tick_i,
  input  logic       frame_complete_i,      // displayed frame came from slot
  input  logic [1:0] frame_complete_slot_i,

  // ---- engine dispatch: Phase-2 sinks (plan D8) ----------------------------
  // blit DMA request (consumed by zhao_cmd_dma's blit port; the payload CRC
  // is verified there BEFORE any byte commits to VRAM)
  output logic        dpy_blit_valid_o,
  input  logic        dpy_blit_ready_i,
  output logic [7:0]  dpy_blit_dst_slot_o,
  output logic [7:0]  dpy_blit_mode_o,    // ABI video_mode byte (validated)
  output logic [31:0] dpy_blit_src_o,
  output logic [31:0] dpy_blit_len_o,
  output logic [31:0] dpy_blit_crc_o,
  // rumble passthrough (INPUT.RUMBLE latches one update per frame at its
  // own frame_tick; last record of the frame wins here)
  output logic       dpy_rumble_valid_o,
  output logic [7:0] dpy_rumble_pad_o,
  output logic [7:0] dpy_rumble_en_o,
  output logic [7:0] dpy_rumble_str_o,
  // counter-snapshot trigger: the D9 read window opens at the frame boundary
  output logic       dpy_snap_req_o,

  // ---- mode register to VIDEO (latched at frame_tick, D6) ------------------
  output zhao_pkg::zhao_mode_e mode_o,

  // ---- D9 counters: owned here, shadows latched at frame_tick --------------
  output zhao_pkg::zhao_counter_snap_t snap_cycles_o,  // id 0 frame_cycles
  output zhao_pkg::zhao_counter_snap_t snap_faults_o,  // id 1 deadline_faults
  output zhao_pkg::zhao_counter_snap_t snap_cmds_o,    // id 2 commands

  // ---- the completion fence: one pulse per FPGA_RUNNING -> DONE ------------
  output logic       fence_valid_o,
  output logic [1:0] fence_slot_o,
  output logic       fence_ok_o,       // 1 = frame completed cleanly
  output logic [7:0] fence_status_o,

  // per-slot FSM state (differential key; 5-state charter encoding)
  output logic [2:0] slot_state_o [0:2]
);

  // charter 7.4 state encoding (one-hot-free 3-bit)
  localparam logic [2:0] S_FREE = 3'd0;
  localparam logic [2:0] S_ARM  = 3'd1;
  localparam logic [2:0] S_RDY  = 3'd2;
  localparam logic [2:0] S_RUN  = 3'd3;
  localparam logic [2:0] S_DONE = 3'd4;

  localparam logic [7:0] STATUS_DEADLINE = 8'd16;  // module-local (zref mirror)

  localparam logic [31:0] DESC_TABLE_BYTES = 32'd4096;

  // ------------------------------------------------------------ state -----
  // Declared inits mirror the async-reset state (formal start point; the
  // reset branch remains the authority) — same convention as W2.3.
  /* verilator lint_off PROCASSINIT */
  logic [2:0]  state     [0:2] = '{default: S_FREE};
  logic [31:0] dead_cnt  [0:2] = '{default: 32'd0};
  logic [31:0] dead_lim  [0:2] = '{default: 32'd0};
  logic        wr_pend_v [0:2] = '{default: 1'b0};
  logic [1:0]  wr_pend_x [0:2] = '{default: 2'd0};
  logic        claimed_window = 1'b0;

  logic        fetch_v = 1'b0;
  logic [1:0]  fetch_slot = 2'd0;
  logic [31:0] fetch_addr = 32'd0;
  logic [31:0] fetch_len = 32'd0;
  logic [31:0] fetch_ep = 32'd0;

  logic        blit_v = 1'b0;
  logic [7:0]  blit_dst = 8'd0;
  logic [7:0]  blit_mode = 8'd0;
  logic [31:0] blit_src = 32'd0;
  logic [31:0] blit_len = 32'd0;
  logic [31:0] blit_crc = 32'd0;

  logic        rumble_v = 1'b0;
  logic [7:0]  rumble_pad = 8'd0;
  logic [7:0]  rumble_en = 8'd0;
  logic [7:0]  rumble_str = 8'd0;

  logic [7:0]  mode_act = 8'd0;   // ZHAO_MODE_Z60
  logic [7:0]  mode_pend = 8'd0;
  logic [31:0] epoch_r = 32'd0;

  logic        fence_v = 1'b0;
  logic        fence_ok_r = 1'b0;
  logic [1:0]  fence_slot_r = 2'd0;
  logic [7:0]  fence_status_r = 8'd0;

  logic        snap_v = 1'b0;
  logic [63:0] sh_cycles = 64'd0;
  logic [63:0] sh_faults = 64'd0;
  logic [63:0] sh_cmds = 64'd0;
  logic [63:0] live_cycles = 64'd0;
  logic [63:0] live_faults = 64'd0;
  logic [63:0] live_cmds = 64'd0;
  /* verilator lint_on PROCASSINIT */

  // ------------------------------------------------- pre-edge helpers ------
  // the active mode's frame period (spec/video_rules.md 1): the deadline
  // default. mode_act is 0..2 by construction: the SetPresentationContract
  // latch below REFUSES out-of-range bytes (they are NOT validated upstream
  // in Phase 2 — CMD.DMA's structural walk skips enum-range check 7).
  function automatic logic [31:0] frame_period(input logic [7:0] m);
    frame_period = zhao_pkg::ZHAO_TIMING[int'(m)].frame_gpu_cycles;
  endfunction

  logic any_run_pre;
  logic [1:0] run_slot_pre;
  always_comb begin
    any_run_pre = 1'b0;
    run_slot_pre = 2'd0;
    for (int s = 0; s < 3; s++) begin
      if (state[s] == S_RUN) begin
        any_run_pre = 1'b1;
        run_slot_pre = 2'(s);
      end
    end
  end

  // claim evaluation: lowest READY slot whose word is still READY, when no
  // frame is running, the claim window is open and no fetch is pending
  logic claim_en;
  logic [1:0] claim_slot;
  always_comb begin
    claim_en = !any_run_pre && !claimed_window && !fetch_v;
    claim_slot = 2'd0;
    for (int s = 2; s >= 0; s--) begin  // ascending priority: last match wins
      if (state[s] == S_RDY && hps_state_i[s] == 2'd2) claim_slot = 2'(s);
    end
    if (!claim_en) claim_slot = 2'd0;
    claim_en = claim_en && (state[claim_slot] == S_RDY) && (hps_state_i[claim_slot] == 2'd2);
  end

  // record acceptance is combinational on the CURRENT blit occupancy
  assign rec_ready_o = !(blit_v && !dpy_blit_ready_i);

  // ------------------------------------------------------ sequential ------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      for (int s = 0; s < 3; s++) begin
        state[s]     <= S_FREE;
        dead_cnt[s]  <= 32'd0;
        dead_lim[s]  <= 32'd0;
        wr_pend_v[s] <= 1'b0;
        wr_pend_x[s] <= 2'd0;
      end
      claimed_window <= 1'b0;
      fetch_v <= 1'b0;  fetch_slot <= 2'd0;
      fetch_addr <= 32'd0; fetch_len <= 32'd0; fetch_ep <= 32'd0;
      blit_v <= 1'b0; blit_dst <= 8'd0; blit_mode <= 8'd0;
      blit_src <= 32'd0; blit_len <= 32'd0; blit_crc <= 32'd0;
      rumble_v <= 1'b0; rumble_pad <= 8'd0; rumble_en <= 8'd0; rumble_str <= 8'd0;
      mode_act <= 8'd0;
      mode_pend <= 8'd0;
      epoch_r <= 32'd0;
      fence_v <= 1'b0; fence_ok_r <= 1'b0;
      fence_slot_r <= 2'd0; fence_status_r <= 8'd0;
      snap_v <= 1'b0;
      sh_cycles <= 64'd0; sh_faults <= 64'd0; sh_cmds <= 64'd0;
      live_cycles <= 64'd0; live_faults <= 64'd0; live_cmds <= 64'd0;
    end else begin
      // ---- handshake drains (first; may be re-set below) -------------------
      if (fetch_v && fetch_req_ready_i) fetch_v <= 1'b0;
      if (blit_v && dpy_blit_ready_i)   blit_v  <= 1'b0;
      // ring-write drain: ONLY the slot presented this cycle (lowest OLD
      // pending — the combinational port below shows it pre-edge) and only
      // when the ring accepts. Pendings set by the transitions below first
      // appear NEXT cycle (held-until-accepted request semantics).
      begin
        int unsigned d;
        d = 3;
        for (int s = 0; s < 3; s++) begin
          if (wr_pend_v[s] && (d == 3)) d = unsigned'(s);
        end
        if ((d != 3) && ring_wr_ready_i) wr_pend_v[d] <= 1'b0;
      end

      fence_v <= 1'b0;
      snap_v  <= 1'b0;

      // ---- per-slot transitions (forward-only charter cycle) ---------------
      for (int s = 0; s < 3; s++) begin
        if (state[s] == S_FREE) begin
          if (hps_state_i[s] == 2'd1) state[s] <= S_ARM;
        end else if (state[s] == S_ARM) begin
          if (hps_state_i[s] == 2'd2) state[s] <= S_RDY;
        end else if (state[s] == S_RDY) begin
          if (claim_en && (2'(s) == claim_slot)) begin
            state[s]    <= S_RUN;
            dead_cnt[s] <= 32'd0;
            dead_lim[s] <= frame_period(mode_act);  // D8 deadline default
          end
        end else if (state[s] == S_RUN) begin
          if (dma_done_i && (dma_slot_i == 2'(s)) && (dma_status_i != 8'd0)) begin
            state[s] <= S_DONE;  // verdict fault: safe error, slot released
            wr_pend_v[s] <= 1'b1; wr_pend_x[s] <= 2'd3;
            fence_v <= 1'b1; fence_ok_r <= 1'b0;
            fence_slot_r <= 2'(s); fence_status_r <= dma_status_i;
          end else if (dead_cnt[s] >= dead_lim[s]) begin
            state[s] <= S_DONE;  // deadline miss: repeat path, no success
            wr_pend_v[s] <= 1'b1; wr_pend_x[s] <= 2'd3;
            fence_v <= 1'b1; fence_ok_r <= 1'b0;
            fence_slot_r <= 2'(s); fence_status_r <= STATUS_DEADLINE;
          end else if (frame_tick_i.pulse && frame_complete_i
                      && (frame_complete_slot_i == 2'(s))) begin
            state[s] <= S_DONE;  // the ONE success fence
            wr_pend_v[s] <= 1'b1; wr_pend_x[s] <= 2'd3;
            fence_v <= 1'b1; fence_ok_r <= 1'b1;
            fence_slot_r <= 2'(s); fence_status_r <= 8'd0;
          end else if (frame_tick_i.pulse) begin
            // boundary reached without this frame completing: display-window
            // miss — fail-safe termination (the video side repeats)
            state[s] <= S_DONE;
            wr_pend_v[s] <= 1'b1; wr_pend_x[s] <= 2'd3;
            fence_v <= 1'b1; fence_ok_r <= 1'b0;
            fence_slot_r <= 2'(s); fence_status_r <= STATUS_DEADLINE;
          end else begin
            dead_cnt[s] <= dead_cnt[s] + 32'd1;
          end
        end else if (state[s] == S_DONE) begin
          if (frame_tick_i.pulse) begin
            state[s] <= S_FREE;  // DONE -> FREE release (FPGA-owned)
            wr_pend_v[s] <= 1'b1; wr_pend_x[s] <= 2'd0;
          end
        end
      end

      // ---- claim side effects: the fetch request to CMD.DMA ----------------
      if (claim_en) begin
        claimed_window <= 1'b1;
        fetch_v    <= 1'b1;
        fetch_slot <= claim_slot;
        fetch_addr <= RING_BASE + DESC_TABLE_BYTES
                      + (32'(claim_slot) * zhao_abi_pkg::FRAME_SLOT_BYTES);
        fetch_len  <= hps_byte_len_i[claim_slot];
        fetch_ep   <= epoch_r;
      end

      // ---- frame_tick globals ----------------------------------------------
      if (frame_tick_i.pulse) begin
        live_cycles <= live_cycles + 64'd1;
        if (frame_tick_i.repeated) live_faults <= live_faults + 64'd1;
        mode_act        <= mode_pend;   // D6: effective next frame
        claimed_window  <= 1'b0;
        rumble_v        <= 1'b0;        // INPUT.RUMBLE consumed the update
        sh_cycles <= live_cycles + 64'd1;  // shadow = value AT the tick
        sh_faults <= frame_tick_i.repeated ? (live_faults + 64'd1) : live_faults;
        sh_cmds   <= live_cmds;  // records below land in the NEXT shadow
        snap_v    <= 1'b1;
      end

      // ---- record dispatch (after tick handling: a record on the tick
      //      cycle wins the dispatch registers) -------------------------------
      if (rec_valid_i && rec_ready_o) begin
        live_cmds <= live_cmds + 64'd1;
        if (rec_opcode_i == zhao_abi_pkg::ZHAO_OP_BEGIN_FRAME) begin
          if ((rec_w3_i != 32'd0) && any_run_pre) begin
            dead_lim[run_slot_pre] <= rec_w3_i;  // nonzero override only
          end
          epoch_r <= rec_w1_i;
        end else if (rec_opcode_i == zhao_abi_pkg::ZHAO_OP_SET_PRESENTATION_CONTRACT) begin
          // video_mode declares members 0-2 ONLY. Rejecting an out-of-range
          // byte is decoder law (capture_format.md 3.2 step 7, BAD_VALUE —
          // wave 3); the Phase-2 structural walk in CMD.DMA deliberately
          // does not perform step 7, so an unlawful byte CAN arrive here.
          // The latch refuses it: adopting it would index ZHAO_TIMING out
          // of bounds in frame_period() and collapse through the 2-bit
          // mode_o conversion. Found by cmd_random_soak (100k frames).
          if (rec_w0_i[7:0] <= 8'd2) mode_pend <= rec_w0_i[7:0];
        end else if (rec_opcode_i == zhao_abi_pkg::ZHAO_OP_DEBUG_FRAME_BLIT) begin
          blit_v   <= 1'b1;
          blit_dst <= rec_w0_i[7:0];
          blit_mode <= rec_w0_i[15:8];
          blit_src <= rec_w1_i;
          blit_len <= rec_w2_i;
          blit_crc <= rec_w3_i;
        end else if (rec_opcode_i == zhao_abi_pkg::ZHAO_OP_DEBUG_RUMBLE) begin
          rumble_v   <= 1'b1;
          rumble_pad <= rec_w0_i[7:0];
          rumble_en  <= rec_w0_i[15:8];
          rumble_str <= rec_w0_i[23:16];
        end
        // all other opcodes: counted, no dispatch (Phase-2 no-op sinks)
      end
    end
  end

  // ------------------------------------------------------- ring write ------
  always_comb begin
    ring_wr_valid_o = 1'b0;
    ring_wr_slot_o  = 2'd0;
    ring_wr_state_o = 2'd0;
    for (int s = 2; s >= 0; s--) begin
      if (wr_pend_v[s]) begin
        ring_wr_valid_o = 1'b1;
        ring_wr_slot_o  = 2'(s);
        ring_wr_state_o = wr_pend_x[s];
      end
    end
  end

  // ------------------------------------------------------- outputs ---------
  assign fetch_req_valid_o = fetch_v;
  assign fetch_slot_o      = fetch_slot;
  assign fetch_addr_o      = fetch_addr;
  assign fetch_byte_len_o  = fetch_len;
  assign fetch_epoch_o     = fetch_ep;

  assign dpy_blit_valid_o    = blit_v;
  assign dpy_blit_dst_slot_o = blit_dst;
  assign dpy_blit_mode_o     = blit_mode;
  assign dpy_blit_src_o      = blit_src;
  assign dpy_blit_len_o      = blit_len;
  assign dpy_blit_crc_o      = blit_crc;

  assign dpy_rumble_valid_o = rumble_v;
  assign dpy_rumble_pad_o   = rumble_pad;
  assign dpy_rumble_en_o    = rumble_en;
  assign dpy_rumble_str_o   = rumble_str;

  assign dpy_snap_req_o = snap_v;
  assign mode_o         = zhao_pkg::zhao_mode_from_abi(mode_act);

  assign fence_valid_o  = fence_v;
  assign fence_slot_o   = fence_slot_r;
  assign fence_ok_o     = fence_ok_r;
  assign fence_status_o = fence_status_r;

  always_comb begin
    for (int s = 0; s < 3; s++) slot_state_o[s] = state[s];
  end

  // D9 snapshots: one-cycle valid pulse at the tick, catalog-index ids
  // (spec/counters.md 5), u64 saturate law owned by the live registers
  // (they saturate never wrap — same adder-guard style as W2.3 when needed;
  // u64 wrap needs 2^64 events, unreachable in Phase 2 lifetimes).
  always_comb begin
    snap_cycles_o.valid      = snap_v;
    snap_cycles_o.counter_id = zhao_pkg::ZHAO_CNT_FRAME_CYCLES;
    snap_cycles_o.value      = sh_cycles;
    snap_faults_o.valid      = snap_v;
    snap_faults_o.counter_id = zhao_pkg::ZHAO_CNT_DEADLINE_FAULTS;
    snap_faults_o.value      = sh_faults;
    snap_cmds_o.valid        = snap_v;
    snap_cmds_o.counter_id   = zhao_pkg::ZHAO_CNT_COMMANDS;
    snap_cmds_o.value        = sh_cmds;
  end

  // ------------------------------------------------------- formal ---------
  // cmd_scheduler_slot_fsm (plan 4 / charter 7.4 + 20.4):
  //   (a) every slot is in exactly one of the five charter states
  //   (b) no two owners: at most one slot in FPGA_RUNNING
  //   (c) DONE precedes FREE: a slot enters FREE only from DONE
  //   (d) reset-idle: after reset release every slot is FREE
  //   (e) fence-exactly-once: every fence pulse coincides with exactly one
  //       slot's FPGA_RUNNING -> DONE transition (1:1, both directions)
`ifdef FORMAL
  logic f_past_valid = 1'b0;
  always_ff @(posedge clk) f_past_valid <= 1'b1;

  // reset released once and never re-asserted (async reset excluded)
  always_ff @(posedge clk) begin
    if (f_past_valid && $past(rst_n)) assume(rst_n);
  end

  // (d) reset-idle
  always_ff @(posedge clk) begin
    if (f_past_valid && !$past(rst_n)) begin
      for (int s = 0; s < 3; s++) assert(state[s] == S_FREE);
    end
  end

  always_ff @(posedge clk) begin
    if (f_past_valid && rst_n && $past(rst_n)) begin
      // (a) state legality
      for (int s = 0; s < 3; s++) assert(state[s] <= S_DONE);
      // (b) at most one FPGA_RUNNING slot (one frame pipeline)
      assert((state[0] == S_RUN) + (state[1] == S_RUN) + (state[2] == S_RUN) <= 1);
      // (c) a slot only enters FREE from DONE (forward-only cycle)
      for (int s = 0; s < 3; s++) begin
        if ((state[s] == S_FREE) && ($past(state[s]) != S_FREE)) begin
          assert($past(state[s]) == S_DONE);
        end
      end
      // (e) fence 1:1 with FPGA_RUNNING -> DONE. NB: every index here is a
      // loop constant — $past(state[dynamic_idx]) would sample the INDEX at
      // the past step as well (SV $past evaluates the whole expression
      // there), which is NOT the property; static indices only.
      begin
        logic [1:0] run2done_n;
        run2done_n = 2'd0;
        for (int s = 0; s < 3; s++) begin
          if (($past(state[s]) == S_RUN) && (state[s] == S_DONE)) begin
            run2done_n = run2done_n + 2'd1;
            assert(fence_valid_o);
            assert(fence_slot_o == 2'(s));
          end
        end
        assert(fence_valid_o == (run2done_n == 2'd1));
        if (fence_valid_o) begin
          assert(state[fence_slot_o] == S_DONE);
        end
      end
    end
  end

  // ---- non-vacuity covers (V16: covers must prove the antecedents) --------
  // (c) is guarded by a FREE-entering transition and (e) by a RUN -> DONE
  // transition; a model where no slot ever completes a frame satisfies both
  // vacuously (the MEM.GUARD failure shape). These witness the full charter
  // cycle FREE -> ARM -> READY -> RUN -> DONE -> FREE, a fence of each
  // polarity, and the deadline path (reachable at depth 40 only through a
  // small BeginFrame.deadline_cycles override — the mode-period default is
  // >= 217,984 cycles).
  always_ff @(posedge clk) begin
    if (f_past_valid && rst_n && $past(rst_n)) begin
      c_run:        cover (state[0] == S_RUN);
      c_fence_ok:   cover (fence_valid_o && fence_ok_o);
      c_fence_bad:  cover (fence_valid_o && !fence_ok_o);
      c_done2free:  cover ((state[0] == S_FREE) && ($past(state[0]) == S_DONE));
      c_deadline:   cover (fence_valid_o
                           && (fence_status_o == STATUS_DEADLINE));
    end
  end
`endif

endmodule : zhao_cmd_scheduler
