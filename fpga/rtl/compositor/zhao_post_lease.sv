// zhao_post_lease.sv -- POST.COMPOSITE's framebuffer read/write LEASE: the
// sequencing, the memory identity and the byte ledger that put the compositor
// between the raster and publication.
//
// ENFORCED-BY: tests/prod/tb_zhao_console_core_smoke.sv (the composed pass:
//              framebuffer and capture compared word for word, every counter
//              and tripwire read), with its three parts proven standalone by
//              tests/compositor/post_fbread_directed.cpp, post_echo_directed.cpp
//              and tests/memory/mem_share_n_directed.cpp
//
// ---------------------------------------------------------------------------
// WHAT THE CONTRACT SAYS, AND WHAT THIS FILE DOES ABOUT EACH CLAUSE
// ---------------------------------------------------------------------------
// POST.COMPOSITE.md: "POST.COMPOSITE owns an exclusive framebuffer read/write
// lease after resolve and before publication. HUD follows post and never forces
// another read." And zhao_post_composite's own header: "NEVER POST-PROCESS THE
// CURRENTLY SCANNED-OUT FRONT BUFFER ... The lease ... is the integrator's to
// hold", and "COMPLETION USES THE OWNED TERMINAL PROTOCOL".
//
//   * AFTER RESOLVE. The pass arms on the render frame's `frame_end` and starts
//     only when the bin pipe is QUIET (every tile resolved), no raster pixel is
//     pending, and RASTER.FBWRITE is DRAINED -- every word the raster issued has
//     RETIRED, so the read-back cannot overtake a write still in flight.
//   * EXCLUSIVE, AND NOT THE FRONT BUFFER. The pass arms only while the RENDER
//     lease is live (`lease_live_i`: the slot manager's live lease naming the
//     renderer). That lease is on the BACK slot by construction -- the manager
//     never grants the displayed one -- and it is exclusive by construction.
//     The guard enforces it a second time: ENGINE0's read arm is inside the
//     leased window and lease-gated (tests/formal/mem_guard_no_escape.sby).
//   * BEFORE PUBLICATION. `busy_o` is high from arming to the last retired
//     write of the last view; the shell folds it into `render_drained_o`, which
//     is the only signal a frame controller may publish on.
//
// ---------------------------------------------------------------------------
// THE MEMORY IDENTITY -- ENGINE0, SHARED, NOT A NEW CLIENT
// ---------------------------------------------------------------------------
// Core entry I15 said the real blocker was "a memory identity for a third
// framebuffer agent": the arbiter's client id IS its slot index, T3 forbids
// spending client 5, and post both reads and writes. The resolution is the
// one `zhao_mem_share_n` exists for -- "a client identity is a PRIVILEGE, not a
// slot" -- applied to ENGINE0: the render engine's lease, the render engine's
// identity, three logical requesters behind one share:
//
//   0  RASTER.FBWRITE  (the raster's writes, then post's write-back: ONE engine,
//                       the shell switches its pixel port at the phase change)
//   1  zhao_post_fbread (the back buffer, read in raster order)
//   2  zhao_post_echo   (POST.ECHO's capture writes)
//
// TWO things the share does not do, and this file does:
//
//   * WRITE-DATA ORDER. A write's data travels on its requester's own channel
//     into the shell's one write queue, which the controller pops in grant
//     order. Two writers pushing at once would interleave words. So a write is
//     offered to the share only while no other write's data is outstanding
//     (`wpend`), and the data channel is muxed to the one outstanding writer.
//     The mask is COMBINATIONAL on the verdict pulse as well as registered, so
//     the cycle a write passes is already closed to the other writer.
//   * RETIREMENT ATTRIBUTION. The arbiter returns credits per CLIENT, and
//     RASTER.FBWRITE's `drained_o` balances issued words against retired words.
//     Three requesters on one client would each see the others' credits. The
//     controller is strictly in order, so a FIFO of {requester, words} pushed at
//     each passed verdict and drained by the credit stream attributes every
//     retired word to the request that issued it. `retire_unowned_o` is the
//     tripwire: a credit with no owner (structurally unreachable).
//
// ---------------------------------------------------------------------------
// DUO -- two passes, one read
// ---------------------------------------------------------------------------
// spec/video_rules.md 1: Duo's stored surface is ONE logical 256 x 384 image at
// 512 bytes/row, view 1 at logical row 192. So the reader walks ONE tall pass
// (views * h rows from the slot base, no view offset, no multiplier), and the
// compositor and the echo run one pass per view: the compositor stops taking
// source pixels at its view's last pixel, so the reader's next pixel simply
// waits for the next pass. Post's write-back labels view 1's rows `y + h`.
//
// ---------------------------------------------------------------------------
// THE MEMORY BUDGET -- owner ruling R38, measured 2026-09-19 (post pass 2)
// ---------------------------------------------------------------------------
// R38: "Add a measured second outstanding read (and more if measurement says
// so) and PROVE post fits inside the frame alongside raster and replay, in
// clocks, with the margin stated." The instrument is the console smoke's
// `post census` (tests/prod/tb_zhao_console_core_smoke.sv), Z60 384 x 240,
// echo armed, every number below read off it:
//
//   MAX_RD  lease busy  sdram busy     e0 bursts rd/wr  other   conflicts
//     1      729,594   632,628 (86%)   11,520/23,360   17,635   14,319
//     2      697,494   622,683 (89%)   11,520/23,360   16,810   14,307
//     4      698,215   623,373 (89%)   11,520/23,360   16,840   14,367
//
// So the second read is worth 4.4% and a fourth is worth nothing: the SDRAM
// controller is already 86% busy with MAX_RD = 1. POST IS MEMORY-BOUND, not
// latency-bound -- 51,690 bursts at 12.05 clocks each is the pass. More reads
// in flight cannot help past 2 because `zhao_vram_arbiter` gives each client a
// 32-word credit pool: a 64-byte read owns all of it until it retires.
// MAX_RD = 2 ships (the share's in-flight table and the shell's `last` queue
// cost ~30 ALM between them).
//
// THE FRAME (1,666,666 gpu clocks, Z60 at the 100 MHz placeholder):
//   post, echo ARMED, measured          697,494   41.8%
//   left for the render phase           969,172   58.2%
// The measurement is PESSIMISTIC in one known direction: the smoke's scanout
// runs about 3.5x the real Z60 rate (16,810 "other" bursts in 697k clocks,
// against 11,520 per 1,666,666 in a real frame), and every scanout burst in the
// window is time post waited. Post's OWN demand is 34,880 bursts x 12.05 =
// 420,300 SDRAM clocks; with real-rate scanout interleaved at the measured 89%
// utilisation that is about 521,000, 31% of the frame. An UNARMED echo (R35)
// removes 11,680 of the write bursts.
//
// WHAT THIS PROVES AND WHAT IT DOES NOT. Post fits: it leaves at least 969,172
// clocks of every frame to the render phase, measured. It does NOT prove the
// render phase fits in them, and that is not post's to prove: GEOM.REPLAY
// measures 56 clocks per view-triangle (reports/R3-CLIENT-A-SCHEDULE-PROOF-
// 20260919.md), ~4.5M clocks at the guaranteed tier. R31's rate packet must
// therefore bring raster + replay under 969,172 clocks (armed) -- NOT under the
// whole 1,666,666 frame -- because post runs AFTER the raster on the same slot.
// The three levers left on the post side are named in FINDINGS-post2: echo
// armed only when used (R35, built with SetPost), a bank-interleaved address
// map (14,307 conflicts x ~6 clocks = ~86k), and a third framebuffer slot so
// post N overlaps raster N+1 (an architectural decision, not taken here).
//
// COST (estimated, UNMEASURED): the share (~90 ALM, +~20 for MAX_RD = 2), the
// sequencer and the retire FIFO (~80 ALM with RQ = 8), plus the reader (now a
// 16-beat queue, unchanged) and the echo (their own headers).
`default_nettype none

module zhao_post_lease
  import zhao_pkg::*;
#(
    parameter int unsigned XW = 9,
    parameter int unsigned YW = 8,
    // READS IN FLIGHT on ENGINE0 (owner ruling R38): the share's MAX_RD, and the
    // reader's queue is sized to hold that many 64-byte reads (8 beats each).
    // Chosen on the console smoke's post census -- see THE MEMORY BUDGET below.
    parameter int unsigned MAX_RD = 2
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- the render frame this pass belongs to ------------------------------
    input  var logic          lease_live_i,     // the render lease is live
    input  var logic          frame_admit_i,    // a render frame was admitted
    input  var logic          frame_end_i,      // the raster's frame_end pulse
    input  var logic          raster_quiet_i,   // bin pipe quiet (all tiles resolved)
    input  var logic          raster_px_i,      // a raster pixel is on offer
    input  var logic          fbw_drained_i,    // RASTER.FBWRITE: every word retired
    input  var logic [ZHAO_VRAM_ADDR_BITS-1:0] fb_base_i,
    input  var logic [15:0]   fb_stride_i,
    input  var logic [XW-1:0] frame_w_i,        // the VIEW's size, from the mode
    input  var logic [YW-1:0] frame_h_i,
    input  var logic          duo_i,            // two views per frame

    // ---- to / from POST.COMPOSITE --------------------------------------------
    output var logic          pass_start_o,     // the compositor's frame_start
    output var logic          view_o,           // the compositor's view_sel
    output var logic          src_valid_o,
    input  var logic          src_ready_i,
    output var logic [15:0]   src_rgb_o,
    input  var logic          out_valid_i,
    output var logic          out_ready_o,
    input  var logic [15:0]   out_rgb_i,
    input  var logic [XW-1:0] out_x_i,
    input  var logic [YW-1:0] out_y_i,
    input  var logic          out_last_i,
    input  var logic          echo_valid_i,     // POST.COMPOSITE's tap
    input  var logic [15:0]   echo_rgb_i,

    // ---- the shared RASTER.FBWRITE's pixel port during the post phase -------
    output var logic               phase_post_o,
    output var logic               fbw_px_valid_o,
    input  var logic               fbw_px_ready_i,
    output var logic [15:0]        fbw_px_rgb_o,
    output var logic signed [11:0] fbw_px_x_o,
    output var logic signed [11:0] fbw_px_y_o,
    output var logic               fbw_px_last_o,

    // ---- requester 0: RASTER.FBWRITE's guard master and write channel -------
    input  var zhao_guard_req_t fbw_req_i,
    output var zhao_guard_rsp_t fbw_rsp_o,
    input  var logic [63:0]     fbw_wdata_i,
    input  var logic            fbw_wvalid_i,
    output var logic            fbw_wready_o,
    input  var logic            fbw_wlast_i,
    output var logic [7:0]      fbw_retire_o,

    // ---- ENGINE0 towards MEM.GUARD, the write queue and the credit stream ---
    output var zhao_guard_req_t e0_req_o,
    input  var zhao_guard_rsp_t e0_rsp_i,
    output var logic [63:0]     e0_wdata_o,
    output var logic            e0_wvalid_o,
    input  var logic            e0_wready_i,
    output var logic            e0_wlast_o,
    input  var logic            e0_beat_valid_i,  // ENGINE0 read beats, routed
    input  var logic [63:0]     e0_beat_data_i,
    input  var logic            e0_beat_last_i,
    input  var logic [7:0]      e0_credits_i,     // client_rsp[ENGINE0].credits

    // ---- evidence -------------------------------------------------------------
    output var logic          busy_o,            // armed or running: do not publish
    output var logic [31:0]   passes_o,          // compositor passes completed
    output var logic [31:0]   frames_o,          // frames fully post-processed
    output var logic          fault_o,           // sticky: a refused source read
    output var logic [31:0]   src_reads_o,
    output var logic [31:0]   src_pixels_o,
    output var logic [31:0]   retire_unowned_o,  // tripwire, must read 0
    output var logic [31:0]   share_contention_o,
    output var logic [31:0]   echo_passes_complete_o,
    output var logic [31:0]   echo_passes_torn_o,
    output var logic [31:0]   echo_pixels_written_o,
    output var logic [31:0]   echo_pixels_dropped_o,
    output var logic          echo_fault_o
);

  localparam int unsigned RYW = YW + 1;          // the reader's tall Duo pass

  // ==========================================================================
  // THE SEQUENCER
  // ==========================================================================
  typedef enum logic [2:0] {
    S_IDLE  = 3'd0,   // no render frame to post-process
    S_ARMED = 3'd1,   // frame_end seen; waiting for the raster to drain
    S_PASS  = 3'd2,   // a compositor pass is running
    S_SETTLE= 3'd3,   // the pass's last pixel is out; wait for retirement
    S_DONE  = 3'd4    // every view post-processed; hold until the next frame
  } sstate_e;

  sstate_e       st_q;
  logic          view_q;
  logic          start_c;
  logic          last_out_seen_q;
  logic          admit_pend_q;     // an admit arrived mid-pass; taken at S_DONE
  logic          admit_early_q;    // sticky until the next pass: see frame_admit_i

  // The view the NEXT pass opens: the first pass of a frame is view 0, the
  // second (Duo only) is view 1.
  logic          next_view_c;
  assign next_view_c = (st_q == S_SETTLE);

  // The share's view of each requester, declared here because the sequencer,
  // the write-order mask and the retire ledger all read it.
  zhao_guard_req_t [2:0] sh_req;
  zhao_guard_rsp_t [2:0] sh_rsp;
  logic [2:0][6:0]       sh_req_len_q;     // length when the share took it

  // The raster is finished with the slot: every tile resolved, nothing on the
  // pixel port, every raster word retired.
  logic raster_done_c;
  assign raster_done_c = raster_quiet_i && !raster_px_i && fbw_drained_i;

  logic echo_busy, rd_busy, rd_done_unused, rd_fault;

  assign start_c = ((st_q == S_ARMED) && raster_done_c && lease_live_i && !frame_admit_i)
                || ((st_q == S_SETTLE) && fbw_drained_i && !echo_busy
                    && duo_i && !view_q);

  assign pass_start_o = start_c;
  // THE VIEW A PASS OPENS IS KNOWN ON ITS START CYCLE, not one cycle later.
  // `view_q` is loaded AT the start edge (S_ARMED -> 0, S_SETTLE -> 1), so on
  // the start cycle itself it still names the PREVIOUS pass: Duo's second pass
  // opened reading view 0, and the first pass after a Duo frame opened reading
  // view 1. The echo already took `start_c ? next_view_c : view_q`; this is the
  // same expression, so the compositor and the echo can never disagree about
  // which view a pass is. (Found by review, Q004 F1; nothing downstream
  // LATCHES the view at frame_start today -- POST.COMPOSITE drives gd/gg_view
  // combinationally and its front pointer is idle on the start cycle -- so the
  // stale cycle was harmless in the composition and wrong as a contract.)
  // ENFORCED-BY: tests/compositor/post_lease_directed.cpp (case 1: the view is
  // sampled ON the pass_start_o cycle, Duo and the frame after it)
  assign view_o       = start_c ? next_view_c : view_q;
  assign phase_post_o = (st_q == S_PASS) || (st_q == S_SETTLE) || (st_q == S_DONE);
  assign busy_o       = (st_q == S_ARMED) || (st_q == S_PASS) || (st_q == S_SETTLE);

  // ==========================================================================
  // THE SOURCE -- one tall read for every view of the frame
  // ==========================================================================
  zhao_guard_req_t rd_req;
  zhao_guard_rsp_t rd_rsp;
  logic            rd_beat_v;
  logic [63:0]     rd_beat_d;
  logic [RYW-1:0]  rd_h_c;
  assign rd_h_c = duo_i ? (RYW'(frame_h_i) << 1) : RYW'(frame_h_i);

  logic [31:0] rd_overflow_unused;
  zhao_post_fbread #(.XW(XW), .YW(RYW), .FIFO_BEATS((MAX_RD < 2) ? 16 : 8 * MAX_RD)) u_source (
    .clk(clk), .rst_n(rst_n),
    // ONE read per FRAME: only the first view's start opens it.
    .start_i (start_c && (st_q == S_ARMED)),
    .origin_i(fb_base_i), .stride_i(fb_stride_i),
    .w_i(frame_w_i), .h_i(rd_h_c),
    .guard_req_o(rd_req), .guard_rsp_i(rd_rsp),
    .beat_valid_i(rd_beat_v), .beat_data_i(rd_beat_d),
    .px_valid_o(src_valid_o), .px_ready_i(src_ready_i), .px_rgb_o(src_rgb_o),
    .busy_o(rd_busy), .done_o(rd_done_unused), .fault_o(rd_fault),
    .reads_o(src_reads_o), .pixels_o(src_pixels_o), .overflow_o(rd_overflow_unused)
  );
  // A refused source read, or a frame admitted under a live post phase.
  assign fault_o = rd_fault || admit_early_q;

  // ==========================================================================
  // THE WRITE-BACK -- the compositor's output onto RASTER.FBWRITE's port
  // ==========================================================================
  // Canvas row = view-local row, plus h for view 1 (the stored Duo surface).
  logic [RYW-1:0] out_row_c;
  assign out_row_c = view_q ? (RYW'(out_y_i) + RYW'(frame_h_i)) : RYW'(out_y_i);

  assign fbw_px_valid_o = out_valid_i && (st_q == S_PASS);
  assign out_ready_o    = fbw_px_ready_i && (st_q == S_PASS);
  assign fbw_px_rgb_o   = out_rgb_i;
  assign fbw_px_x_o     = 12'($signed({1'b0, out_x_i}));
  assign fbw_px_y_o     = 12'($signed({1'b0, out_row_c}));
  assign fbw_px_last_o  = out_last_i;

  logic out_fire_c;
  assign out_fire_c = out_valid_i && out_ready_o;

  // ==========================================================================
  // POST.ECHO -- on the compositor's tap, qualified by the output handshake
  // ==========================================================================
  zhao_guard_req_t ec_req;
  zhao_guard_rsp_t ec_rsp;
  logic [63:0]     ec_wdata;
  logic            ec_wvalid, ec_wready, ec_wlast;
  logic [7:0]      ec_retire;
  logic            ec_complete_unused;

  zhao_post_echo #(.XW(XW), .YW(YW), .SKID(256)) u_echo (
    .clk(clk), .rst_n(rst_n),
    .pass_start_i(start_c), .view_i(view_o),
    .w_i(frame_w_i), .h_i(frame_h_i),
    // The raw tap repeats a stalled beat; the ACCEPTED beat happens once.
    .tap_valid_i(echo_valid_i && out_fire_c), .tap_rgb_i(echo_rgb_i),
    .tap_x_i(out_x_i), .tap_y_i(out_y_i),
    .guard_req_o(ec_req), .guard_rsp_i(ec_rsp),
    .guard_wdata_o(ec_wdata), .guard_wvalid_o(ec_wvalid),
    .guard_wready_i(ec_wready), .guard_wlast_o(ec_wlast),
    .retire_words_i(ec_retire),
    .busy_o(echo_busy), .pass_complete_o(ec_complete_unused),
    .passes_complete_o(echo_passes_complete_o), .passes_torn_o(echo_passes_torn_o),
    .pixels_written_o(echo_pixels_written_o), .pixels_dropped_o(echo_pixels_dropped_o),
    .fault_o(echo_fault_o)
  );


  // ==========================================================================
  // ENGINE0 -- three logical requesters, one client (zhao_mem_share_n)
  // ==========================================================================
  // WRITE-DATA ORDER: from the moment the share TAKES a write until that
  // write's last data beat (or its refusal), the other writer's request is
  // withheld. Registered at the take, which is early enough: the share takes
  // one requester per idle cycle and is busy until the verdict, so no second
  // write can be taken before this mask is up -- and registered, so it forms
  // no combinational path back through the share's ready.
  logic            wpend_q;
  logic            wown_q;                 // 0 = FBWRITE, 1 = ECHO
  logic            wpend_c;
  assign wpend_c = wpend_q;

  // THE RETIRE LEDGER'S ROOM (see RETIREMENT ATTRIBUTION below). The share
  // holds at most ONE taken-but-unverdicted request, so withholding every
  // requester while the ledger has fewer than two free entries means the
  // verdict that request earns always has an entry to land in.
  logic rq_room_c;

  always_comb begin
    sh_req[0] = fbw_req_i;
    sh_req[1] = rd_req;
    sh_req[2] = ec_req;
    if (wpend_c) begin
      sh_req[0].valid = 1'b0;
      sh_req[2].valid = 1'b0;
    end
    if (!rq_room_c) begin
      sh_req[0].valid = 1'b0;
      sh_req[1].valid = 1'b0;
      sh_req[2].valid = 1'b0;
    end
  end

  logic [2:0]       sh_bv, sh_bl_unused;
  logic [63:0]      sh_bd;
  logic [2:0][31:0] sh_jobs_unused;
  logic [31:0]      sh_denied_unused, sh_short_unused, sh_long_unused, sh_unowned_unused;

  zhao_mem_share_n #(.N(3), .CLIENT_ID(2), .FORCE_READ(1'b0), .MAX_RD(MAX_RD)) u_engine0_share (
    .clk(clk), .rst_n(rst_n),
    .req_i(sh_req), .rsp_o(sh_rsp),
    .beat_valid_o(sh_bv), .beat_data_o(sh_bd), .beat_last_o(sh_bl_unused),
    .m_req_o(e0_req_o), .m_rsp_i(e0_rsp_i),
    .m_beat_valid_i(e0_beat_valid_i), .m_beat_data_i(e0_beat_data_i),
    .m_beat_last_i(e0_beat_last_i),
    .jobs_o(sh_jobs_unused), .denied_o(sh_denied_unused),
    .contention_o(share_contention_o),
    .err_short_o(sh_short_unused), .err_long_o(sh_long_unused),
    .err_unowned_o(sh_unowned_unused)
  );

  // The share is the guard as each requester sees it. A requester whose
  // request is masked sees no ready, which is simply the guard being busy.
  assign fbw_rsp_o = sh_rsp[0];
  assign rd_rsp    = sh_rsp[1];
  assign ec_rsp    = sh_rsp[2];
  assign rd_beat_v = sh_bv[1];
  assign rd_beat_d = sh_bd;

  // The one outstanding writer owns the data channel.
  assign e0_wvalid_o  = wown_q ? ec_wvalid : fbw_wvalid_i;
  assign e0_wdata_o   = wown_q ? ec_wdata  : fbw_wdata_i;
  assign e0_wlast_o   = wown_q ? ec_wlast  : fbw_wlast_i;
  assign fbw_wready_o = !wown_q && e0_wready_i;
  assign ec_wready    =  wown_q && e0_wready_i;

  // ==========================================================================
  // RETIREMENT ATTRIBUTION -- in order, as the controller retires
  // ==========================================================================
  // {requester, words} pushed at each passed verdict; the credit stream drains
  // the head.
  //
  // IT HAD NO FULL GUARD (found by review, Q004 F4). Depth 4 was argued from
  // today's arbiter -- "a request in the guard, one at the arbiter and one
  // retiring, with one to spare" -- and nothing enforced the argument: a fifth
  // passed verdict before the first credit wrapped `rq_wp_q` over a live entry
  // and misattributed every word after it, which surfaces as a FBWRITE that
  // never drains or an echo reported torn. The argument is also exactly the one
  // R38 retires: every extra outstanding read is one more entry.
  //
  // So the ledger now GUARDS itself (`rq_room_c` masks the share, above) and is
  // deep enough that the guard never binds at the arbiter's own depth: 8
  // entries of 8 bits is 64 flops (~20 ALM). The ledger no longer depends on
  // how deep the memory system downstream happens to be.
  // ENFORCED-BY: tests/compositor/post_lease_directed.cpp (case 3: credits
  // held back for hundreds of clocks while all three requesters run)
  localparam int unsigned RQ  = 8;
  localparam int unsigned RQW = $clog2(RQ);
  logic [1:0]     rq_own [0:RQ-1];
  logic [5:0]     rq_left[0:RQ-1];
  logic [RQW-1:0] rq_wp_q, rq_rp_q;
  logic [RQW:0]   rq_n_q;

  assign rq_room_c = (rq_n_q <= (RQW+1)'(RQ - 2));

  logic       push_c;
  logic [1:0] push_own_c;
  logic [5:0] push_words_c;
  always_comb begin
    push_c       = 1'b0;
    push_own_c   = 2'd0;
    push_words_c = 6'd0;
    for (int k = 0; k < 3; k++) begin
      if (sh_rsp[k].ok) begin
        push_c       = 1'b1;
        push_own_c   = 2'(k);
        push_words_c = 6'(sh_req_len_q[k] >> 1);   // bytes -> 16-bit words
      end
    end
  end

  // The length each requester last offered, captured when the share took it
  // (its ready), because the requester drops the request before the verdict.
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) sh_req_len_q <= '0;
    else begin
      for (int k = 0; k < 3; k++)
        if (sh_req[k].valid && sh_rsp[k].ready) sh_req_len_q[k] <= sh_req[k].len;
    end
  end

  logic       credit_c;
  logic [1:0] head_own_c;
  assign credit_c   = (e0_credits_i != 8'd0);
  assign head_own_c = rq_own[rq_rp_q];

  assign fbw_retire_o = (credit_c && (rq_n_q != '0) && (head_own_c == 2'd0)) ? e0_credits_i : 8'd0;
  assign ec_retire    = (credit_c && (rq_n_q != '0) && (head_own_c == 2'd2)) ? e0_credits_i : 8'd0;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      rq_wp_q <= '0; rq_rp_q <= '0; rq_n_q <= '0;
      retire_unowned_o <= '0;
      for (int i = 0; i < RQ; i++) begin rq_own[i] <= '0; rq_left[i] <= '0; end
    end else begin
      automatic logic pop = 1'b0;
      if (credit_c) begin
        if (rq_n_q == '0) begin
          retire_unowned_o <= retire_unowned_o + 32'd1;
        end else if (6'(e0_credits_i) >= rq_left[rq_rp_q]) begin
          pop = 1'b1;
          rq_rp_q <= rq_rp_q + RQW'(1);
          // A burst's credits belong to ONE request. More than the head is
          // owed means a word was attributed to the wrong requester.
          if (6'(e0_credits_i) != rq_left[rq_rp_q])
            retire_unowned_o <= retire_unowned_o + 32'd1;
        end else begin
          rq_left[rq_rp_q] <= rq_left[rq_rp_q] - 6'(e0_credits_i);
        end
      end
      if (push_c) begin
        rq_own[rq_wp_q]  <= push_own_c;
        rq_left[rq_wp_q] <= push_words_c;
        rq_wp_q          <= rq_wp_q + RQW'(1);
      end
      rq_n_q <= rq_n_q + (push_c ? (RQW+1)'(1) : '0) - (pop ? (RQW+1)'(1) : '0);
    end
  end

  // ==========================================================================
  // THE ONE SEQUENTIAL BLOCK FOR THE PASS
  // ==========================================================================
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_q            <= S_IDLE;
      view_q          <= 1'b0;
      last_out_seen_q <= 1'b0;
      admit_pend_q    <= 1'b0;
      admit_early_q   <= 1'b0;
      wpend_q         <= 1'b0;
      wown_q          <= 1'b0;
      passes_o        <= '0;
      frames_o        <= '0;
    end else begin
      // ---- write-data ownership --------------------------------------------
      if (sh_req[0].valid && sh_rsp[0].ready && sh_req[0].write) begin
        wpend_q <= 1'b1; wown_q <= 1'b0;
      end
      if (sh_req[2].valid && sh_rsp[2].ready) begin
        wpend_q <= 1'b1; wown_q <= 1'b1;
      end
      // Released by the owner's last data beat, or by its REFUSAL: a refused
      // write sends no data, and holding the mask would lock out the other
      // writer for good.
      if ((e0_wvalid_o && e0_wready_i && e0_wlast_o)
          || (wown_q ? sh_rsp[2].violation : sh_rsp[0].violation))
        wpend_q <= 1'b0;

      // ---- the pass ---------------------------------------------------------
      if (out_fire_c && out_last_i) last_out_seen_q <= 1'b1;

      case (st_q)
        S_IDLE: begin
          if (frame_end_i && lease_live_i) st_q <= S_ARMED;
        end
        S_ARMED: begin
          if (start_c) begin
            st_q            <= S_PASS;
            view_q          <= 1'b0;
            last_out_seen_q <= 1'b0;
          end
        end
        S_PASS: begin
          if (last_out_seen_q || (out_fire_c && out_last_i)) st_q <= S_SETTLE;
        end
        S_SETTLE: begin
          if (start_c) begin                       // Duo: view 1
            st_q            <= S_PASS;
            view_q          <= 1'b1;
            last_out_seen_q <= 1'b0;
            passes_o        <= passes_o + 32'd1;
          end else if (fbw_drained_i && !echo_busy) begin
            st_q     <= S_DONE;
            passes_o <= passes_o + 32'd1;
            frames_o <= frames_o + 32'd1;
          end
        end
        S_DONE: ;
        default: st_q <= S_IDLE;
      endcase

      // A newly admitted render frame ends the previous post phase: the shell
      // gives RASTER.FBWRITE back to the raster from here.
      //
      // ONLY A FINISHED PHASE IS ENDED BY IT (found by review, Q004 F2). This
      // used to force S_IDLE unconditionally, so an admit during S_PASS or
      // S_SETTLE would hand FBWRITE's port back to the raster with post
      // pixels still in it, and leave the reader and the echo running with
      // nobody sequencing them.
      //
      // IT CANNOT HAPPEN IN THIS SHELL, and the chain is short enough to state:
      //   1. an admit is `zhao_renderer_lease_v2`'s ST_FRAME, reached only from
      //      ST_IDLE/ST_RETRY with `!lease_valid_i` -- no live manager lease;
      //   2. post arms only with `lease_live_i` (the manager's lease, writer =
      //      renderer), and stays busy until its last word retires;
      //   3. `zhao_video_slotmgr_v2` drops a live lease ONLY on a matching
      //      TERMINAL; faults latch and never release it;
      //   4. the renderer's terminal is tied off in `zhao_shell_top_v2`, and the
      //      blit terminal cannot match a renderer lease (writer differs).
      // So between arming and `busy_o` falling there is no admit. When the
      // renderer terminal gets its producer, it must be issued on
      // `render_drained_o` -- which is already `fbw_drained && !post_busy_o`.
      //
      // Because (4) is a tie-off rather than a law, the lease DEFINES what an
      // early admit does instead of relying on it:
      //   * in S_ARMED nothing has been read or written, so the arming is
      //     simply abandoned (no pass, no frame counted);
      //   * in S_PASS / S_SETTLE the pass is FINISHED, not torn: the admit is
      //     held and taken at S_DONE, so FBWRITE's port changes hands only
      //     between whole writers and ENGINE0's three requesters are never
      //     orphaned. The new frame's raster waits on FBWRITE's ready meanwhile.
      // Either way `fault_o` latches (`admit_early_q`) until the next pass
      // starts, so the frame cannot be mistaken for a clean one.
      // ENFORCED-BY: tests/compositor/post_lease_directed.cpp (cases 4 and 5)
      if (frame_admit_i) begin
        if ((st_q == S_PASS) || (st_q == S_SETTLE)) begin
          admit_pend_q  <= 1'b1;
          admit_early_q <= 1'b1;
        end else begin
          st_q <= S_IDLE;
          if (st_q == S_ARMED) admit_early_q <= 1'b1;
        end
      end
      if (admit_pend_q && (st_q == S_DONE)) begin
        st_q         <= S_IDLE;
        admit_pend_q <= 1'b0;
      end
      if (start_c && (st_q == S_ARMED)) admit_early_q <= 1'b0;
    end
  end


  logic unused_c;
  assign unused_c = ^{rd_busy, rd_done_unused, rd_overflow_unused, ec_complete_unused,
                      sh_bv[0], sh_bv[2], sh_bl_unused, sh_jobs_unused, sh_denied_unused,
                      sh_short_unused, sh_long_unused, sh_unowned_unused};

endmodule : zhao_post_lease

`default_nettype wire
