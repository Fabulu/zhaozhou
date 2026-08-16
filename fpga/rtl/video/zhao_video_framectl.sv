// zhao_video_framectl.sv — VIDEO.FRAMECTL, the frame boundary owner
// (plan W2.2; law: spec/video_rules.md §4-§5, §7; spec/counters.md §3).
//
// At every swap/repeat DECISION instant (vswap_dec = raster (x==0, y==244),
// the first cycle of vertical sync — spec/video_rules.md §4) FRAMECTL:
//   * swaps to the committed READY slot (swap_req{slot} to SCANOUT, executed
//     in this vblank), or — no committed slot — repeats the previous frame;
//   * emits the machine-wide `frame_tick` (one vid-cycle pulse) carrying
//     {frame_id, repeated} — INPUT.SNAPSHOT latches pads on it, counters
//     latch shadows, CMD.SCHEDULER closes the slot FSM (spec/counters.md §3);
//   * emits the exactly-one completion fence {slot, repeated} per displayed
//     frame (formal property video_framectl_one_fence);
//   * counts deadline_faults (once per missed frame, never per line) and
//     frame_cycles (one per tick).
//
// Commit law: a slot counts for the frame being closed iff its READY rose
// (or was already high, uncommitted) while the deadline window was still
// open (deadline_left != 0). READY is level-sampled in the vid domain every
// cycle, so "READY before the deadline" is exact to the vid cycle. A READY
// that first lands with the window closed is LATE -> repeat, deadline_fault.
// Lowest slot index wins when both are READY.
//
// Deadline window: `deadline_cycles` is in GPU cycles (BeginFrame ABI
// units); the monitor decrements it by ZHAO_VID_CYCLES_PER_GPU per vid
// cycle (the vid-domain mirror of the gpu deadline, exact while the frozen
// 2:1 ratio holds — plan D1). deadline_cycles == 0 selects the DEFAULT =
// the latched mode's frame period (spec/video_rules.md §2). The window is
// loaded at frame_start and closes at vswap_dec (the frame's own vblank).
//
// The gpu-domain broadcast crosses vid->gpu as a toggle + 2-flop
// synchronizer (spec/video_rules.md §7, plan R1): the crossing is
// deterministic because the harness tick scheduler fixes the domain phase.
//
// Conservative SystemVerilog subset only (charter §2).
// Lint: clean under `verilator_bin --lint-only -Wall` (CTest
// lint_zhao_video_framectl).

module zhao_video_framectl
  import zhao_pkg::*;
(
  input  logic        vid_clk,
  input  logic        rst_n,

  // raster position + boundary pulses from VIDEO.MODE (vid domain)
  input  logic [15:0] x,
  input  logic [15:0] y,
  input  logic        vblank,
  input  logic        vswap_dec,     // decision instant (x==0, y==244)
  input  logic        frame_start,   // (x==0, y==0): reload the deadline
  input  zhao_mode_e  mode,          // latched mode (default deadline table)

  // slot status from CMD.SCHEDULER (vid-synchronized at the shell seam)
  input  logic [1:0]  slot_ready,    // level; consumed at the decision
  input  logic [31:0] deadline_cycles, // 0 => mode frame period (gpu cycles)

  // SCANOUT handshake (vid domain, vblank only)
  output logic        swap_req,      // 1-cycle command at vswap_dec
  output logic        swap_slot,
  input  logic        swap_ack,      // must arrive within the same vblank
  output logic        frame_repeated,// 1-cycle pulse with the tick

  // vid-domain tick + completion fence (frame boundary broadcast)
  output logic        frame_tick,
  output logic [31:0] frame_id,
  output logic [63:0] frame_cycles,   // ZHAO_CNT_FRAME_CYCLES shadow source
  output logic [63:0] deadline_faults,// ZHAO_CNT_DEADLINE_FAULTS shadow source
  output logic [31:0] deadline_margin,// trace: deadline_left at the decision

  // gpu-domain broadcast: toggle + 2FF (spec/video_rules.md §7)
  input  logic        gpu_clk,
  output zhao_frame_tick_t gpu_tick,  // {pulse, frame_id, repeated} in gpu
  output logic [0:0]  gpu_complete_slot
);

  // ------------------------------------------------------------ registers --
  logic        committed_v;           // a READY slot was signed off in-window
  logic        committed_slot;
  logic [31:0] deadline_left;         // gpu-cycle units, vid-decremented x2
  logic [31:0] frame_id_q;
  logic [63:0] frame_cycles_q;
  logic [63:0] deadline_faults_q;
  logic        tick_tog;              // vid -> gpu toggle
  logic [0:0]  cur_slot;              // the displayed slot FRAMECTL last chose
  logic        cur_repeated;

  logic [31:0] deadline_load;
  assign deadline_load = (deadline_cycles != 32'd0)
                       ? deadline_cycles
                       : ZHAO_TIMING[mode].frame_gpu_cycles;

  // commit condition: an uncommitted READY slot while the window is open
  logic has_ready;
  logic [0:0] ready_pick;
  assign has_ready  = slot_ready[0] || slot_ready[1];
  assign ready_pick = slot_ready[0] ? 1'b0 : 1'b1;   // lowest index wins

  always_ff @(posedge vid_clk or negedge rst_n) begin
    if (!rst_n) begin
      committed_v     <= 1'b0;
      committed_slot  <= 1'b0;
      deadline_left   <= 32'd0;
      frame_id_q      <= 32'd0;
      frame_cycles_q  <= 64'd0;
      deadline_faults_q <= 64'd0;
      tick_tog        <= 1'b0;
      cur_slot        <= 1'b0;
      cur_repeated    <= 1'b0;
      frame_repeated  <= 1'b0;
      frame_tick      <= 1'b0;
      frame_id        <= 32'd0;
      deadline_margin <= 32'd0;
    end else begin
      // default: single-cycle pulses
      frame_repeated <= 1'b0;
      frame_tick     <= 1'b0;

      if (frame_start) begin
        // reload the deadline window for the frame now starting (gpu units)
        deadline_left <= deadline_load;
      end else if (deadline_left >= 32'(ZHAO_VID_CYCLES_PER_GPU)) begin
        deadline_left <= deadline_left - 32'(ZHAO_VID_CYCLES_PER_GPU);
      end else begin
        deadline_left <= 32'd0;    // closed; stays closed until frame_start
      end

      if (!committed_v && has_ready && (deadline_left != 32'd0)) begin
        committed_v    <= 1'b1;
        committed_slot <= ready_pick;
      end

      if (vswap_dec) begin
        // ---- the swap/repeat decision (fail-safe direction = repeat) ----
        if (committed_v) begin
          cur_slot     <= committed_slot;
          cur_repeated <= 1'b0;
        end else begin
          cur_repeated <= 1'b1;      // repeat: previous complete frame again
          deadline_faults_q <= (deadline_faults_q == 64'hFFFF_FFFF_FFFF_FFFF)
                             ? 64'hFFFF_FFFF_FFFF_FFFF   // saturate, never wrap
                             : deadline_faults_q + 64'd1; // (spec/counters.md §4)
        end
        committed_v   <= 1'b0;
        frame_id_q    <= frame_id_q + 32'd1;
        frame_cycles_q<= (frame_cycles_q == 64'hFFFF_FFFF_FFFF_FFFF)
                       ? frame_cycles_q : frame_cycles_q + 64'd1;
        tick_tog      <= ~tick_tog;
        deadline_margin <= deadline_left;
        frame_tick    <= 1'b1;
        // the repeated flag of the frame being closed is decided by this
        // edge's committed_v sample (fail-safe direction = repeat)
        frame_repeated<= ~committed_v;
        frame_id      <= frame_id_q + 32'd1;
      end
    end
  end

  // note: swap_ack is accepted any cycle it is high within vblank (SCANOUT
  // acks one cycle after swap_req by construction); FRAMECTL does not wait
  // for it — if SCANOUT cannot ack, the fail-safe is the next-frame repeat.
  // ENFORCED-BY: fpga/rtl/video/zhao_video_scanout.sv:swap_ack
  // The raw raster inputs (x, y, vblank) are carried for the contract
  // surface (VIDEO.FRAMECTL "Input and output packet layouts"); the decision
  // itself decodes the pre-pulsed vswap_dec/frame_start from VIDEO.MODE.
  /* verilator lint_off UNUSEDSIGNAL */
  logic unused_raster;
  assign unused_raster = swap_ack ^ vblank ^ x[15] ^ y[15] ^ x[14] ^ y[14]
                       ^ x[13] ^ y[13] ^ x[12] ^ y[12] ^ x[11] ^ y[11]
                       ^ x[10] ^ y[10] ^ x[9]  ^ y[9]  ^ x[8]  ^ y[8]
                       ^ x[7]  ^ y[7]  ^ x[6]  ^ y[6]  ^ x[5]  ^ y[5]
                       ^ x[4]  ^ y[4]  ^ x[3]  ^ y[3]  ^ x[2]  ^ y[2]
                       ^ x[1]  ^ y[1]  ^ x[0]  ^ y[0];
  /* verilator lint_on UNUSEDSIGNAL */

  assign frame_cycles    = frame_cycles_q;
  assign deadline_faults = deadline_faults_q;

  // ------------------------------------------------ vid -> gpu crossing ----
  // toggle + 2-flop synchronizer + edge detect (spec/video_rules.md §7).
  // Data (frame id / slot / repeated) is a vid register that only changes
  // at the toggle edge and is captured on the gpu side when the pulse is
  // seen — stable-by-construction, no gray coding needed beyond the toggle.
  // ENFORCED-BY: tests/formal/video_framectl_one_fence.sby:a_cdc_data_stable_unless_toggle
  logic       tog_s1, tog_s2, tog_s2q;
  logic [0:0] slot_s1, slot_s2;

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      tog_s1  <= 1'b0;
      tog_s2  <= 1'b0;
      tog_s2q <= 1'b0;
      slot_s1 <= 1'b0;
      slot_s2 <= 1'b0;
    end else begin
      tog_s1  <= tick_tog;
      tog_s2  <= tog_s1;
      tog_s2q <= tog_s2;
      slot_s1 <= cur_slot;
      slot_s2 <= slot_s1;
    end
  end

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      gpu_tick       <= '0;
      gpu_complete_slot <= 1'b0;
    end else begin
      gpu_tick.pulse    <= (tog_s2 != tog_s2q);
      gpu_tick.frame_id <= (tog_s2 != tog_s2q) ? (frame_id_q) : gpu_tick.frame_id;
      gpu_tick.repeated <= (tog_s2 != tog_s2q) ? (cur_repeated) : gpu_tick.repeated;
      gpu_complete_slot <= (tog_s2 != tog_s2q) ? slot_s2 : gpu_complete_slot;
    end
  end

  // ------------------------------------------------- swap command (comb) ---
  // The swap command is issued COMBINATIONALLY during the vswap_dec cycle
  // (contract FRAMECTL: "decision combinational in the vblank window") so
  // SCANOUT registers the new display slot at the dec edge itself and the
  // 2FF-crossed copy is settled in the gpu domain BEFORE the (one-cycle
  // slower) dec_sync re-arm pulse reaches the fetch side.
  assign swap_req  = vswap_dec && committed_v;
  assign swap_slot = committed_slot;

endmodule : zhao_video_framectl
