// zhao_video_ready_bridge_v2.sv -- Packet-H video READY/echo and blank bridge.
//
// One complete Packet-G tuple is held between the READY CDC and FRAMECTL.  A
// FRAMECTL swap atomically moves that exact tuple into a one-entry reverse hold;
// the hold is not released until the reverse CDC accepts it.  Slot readiness is
// therefore an exact one-hot advertisement of the held tuple, and is suppressed
// whenever the reverse hold cannot accept the resulting echo.
//
// Either reset forms an asynchronous pair reset.  Pair reset discards both
// tuple holds and forces the registered pixel stream to black.  Reset assertion
// is asynchronous, but each clock domain leaves reset through its own three-flop
// synchronous release chain.  The video-domain blank acknowledgement is raised
// only after registered RGB is observably black.  The GPU-domain lease gate opens
// once per reset epoch only after both CDC barriers and a three-flop-synchronized
// blank acknowledgement; it remains open after the first safe frame unblanks.
//
// Unblanking is a two-proof join for one exact swap tuple.  The bridge remembers
// reverse-CDC acceptance and VIDEO.SCANOUT acknowledgement independently, in
// either order, and removes blank only after both facts belong to that tuple.
//
// The packed tuple is:
//   {writer1, slot1, generation16, mode2, base32, span32}
`default_nettype none

`ifndef ZHAO_VIDEO_BRIDGE_ECHO_VALID
`define ZHAO_VIDEO_BRIDGE_ECHO_VALID(held, ready) (held)
`endif
`ifndef ZHAO_VIDEO_BRIDGE_ECHO_TUPLE
`define ZHAO_VIDEO_BRIDGE_ECHO_TUPLE(held, offered) (held)
`endif
`ifndef ZHAO_VIDEO_BRIDGE_SLOT_ONEHOT
`define ZHAO_VIDEO_BRIDGE_SLOT_ONEHOT(slot) ((slot) ? 2'b10 : 2'b01)
`endif
`ifndef ZHAO_VIDEO_BRIDGE_BLANK_ACK
`define ZHAO_VIDEO_BRIDGE_BLANK_ACK(registered_ack, command) (registered_ack)
`endif
`ifndef ZHAO_VIDEO_BRIDGE_LEASE_GATE
`define ZHAO_VIDEO_BRIDGE_LEASE_GATE(gpu_barrier, vid_barrier, blank_ack) \
    ((gpu_barrier) && (vid_barrier) && (blank_ack))
`endif
`ifndef ZHAO_VIDEO_BRIDGE_RESET_VALUE
`define ZHAO_VIDEO_BRIDGE_RESET_VALUE(current) '0
`endif
`ifndef ZHAO_VIDEO_BRIDGE_UNBLANK_JOIN
`define ZHAO_VIDEO_BRIDGE_UNBLANK_JOIN(echo_seen, scanout_seen) \
    ((echo_seen) && (scanout_seen))
`endif

module zhao_video_ready_bridge_v2
  import zhao_pkg::*;
(
    input  logic        gpu_clk,
    input  logic        gpu_rst_n,
    input  logic        vid_clk,
    input  logic        vid_rst_n,

    // Barrier levels are produced by zhao_fb_ready_cdc_v2 in their named
    // domains.  vid_barrier_done_i and blank_ack_o are synchronized here before
    // they participate in the GPU-domain lease decision.
    input  logic        gpu_barrier_done_i,
    input  logic        vid_barrier_done_i,

    // Video-domain blank command and proof.  A command starts a blank epoch but
    // does not revoke tuple ownership; only pair reset may discard bridge work.
    input  logic        blank_cmd_i,
    output logic        blank_ack_o,
    output logic        lease_open_o,

    // READY output of the forward CDC (video domain).
    input  logic        cdc_ready_valid_i,
    output logic        cdc_ready_ready_o,
    input  logic [83:0] cdc_ready_tuple_i,

    // Swap input of the reverse CDC (video domain).
    output logic        cdc_swap_valid_o,
    input  logic        cdc_swap_ready_i,
    output logic [83:0] cdc_swap_tuple_o,

    // VIDEO.FRAMECTL decision and VIDEO.SCANOUT completion (video domain).
    output logic [1:0]  frame_slot_ready_o,
    input  logic        frame_swap_valid_i,
    input  logic        frame_swap_slot_i,
    input  logic        scanout_ack_i,

    // Registered final pixel seam.  During blank only RGB is replaced; raster
    // validity, coordinates, sync, and blanking travel through the same register
    // so colour can never become phase-shifted from its timing metadata.
    input  zhao_px_stream_t scanout_px_i,
    output zhao_px_stream_t output_px_o,

    // Integration observations; these are trace witnesses, not authorities.
    // These two aliases intentionally expose nets also used as local async reset.
    /* verilator lint_off SYNCASYNCNET */
    output logic        gpu_reset_released_o,
    output logic        vid_reset_released_o,
    /* verilator lint_on SYNCASYNCNET */
    output logic        pending_o,
    output logic [83:0] pending_tuple_o,
    output logic        echo_hold_o,
    output logic        blank_active_o,
    output logic        scanout_wait_o,
    output logic        unblank_candidate_o,
    output logic [83:0] unblank_tuple_o,
    output logic        unblank_echo_seen_o,
    output logic        unblank_scanout_seen_o
);

  localparam logic [15:0] BLACK_RGB565 = 16'h0000;
  wire pair_rst_n = gpu_rst_n && vid_rst_n;

  initial begin : p_packet_h_bridge_selector_contract
`ifdef ZHAO_VIDEO_BRIDGE_MUTANT_COLLISION
    $fatal(1, "ZHAO_VIDEO_READY_BRIDGE_V2_MUTANT_SELECTOR_COLLISION");
`endif
  end
`ifdef ZHAO_VIDEO_BRIDGE_MUTANT_COLLISION
  ZHAO_VIDEO_READY_BRIDGE_V2_MUTANT_SELECTOR_COLLISION__DEFINE_EXACTLY_ONE
      u_packet_h_bridge_selector_collision();
`endif

  // Assert both domains immediately when either reset asserts.  Each domain
  // releases only after three edges of its own clock, exactly like the paired
  // Packet-G CDC.  No data/state block is clocked directly by pair_rst_n release.
  /* verilator lint_off SYNCASYNCNET */
  (* ASYNC_REG = "TRUE" *) logic [2:0] gpu_reset_release_q;
  (* ASYNC_REG = "TRUE" *) logic [2:0] vid_reset_release_q;
  wire gpu_local_rst_n = gpu_reset_release_q[2];
  wire vid_local_rst_n = vid_reset_release_q[2];

  always_ff @(posedge gpu_clk or negedge pair_rst_n) begin
    if (!pair_rst_n)
      gpu_reset_release_q <= 3'b000;
    else
      gpu_reset_release_q <= {gpu_reset_release_q[1:0], 1'b1};
  end

  always_ff @(posedge vid_clk or negedge pair_rst_n) begin
    if (!pair_rst_n)
      vid_reset_release_q <= 3'b000;
    else
      vid_reset_release_q <= {vid_reset_release_q[1:0], 1'b1};
  end

  assign gpu_reset_released_o = gpu_local_rst_n;
  assign vid_reset_released_o = vid_local_rst_n;

  logic        pending_q;
  logic [83:0] pending_tuple_q;
  logic        echo_valid_q;
  logic [83:0] echo_tuple_q;
  logic        blank_active_q;
  logic        blank_ack_q;
  logic        scanout_wait_q;
  logic [83:0] scanout_tuple_q;
  logic        unblank_candidate_q;
  logic [83:0] unblank_tuple_q;
  logic        unblank_echo_seen_q;
  logic        unblank_scanout_seen_q;

  wire echo_visible_c =
      `ZHAO_VIDEO_BRIDGE_ECHO_VALID(echo_valid_q, cdc_swap_ready_i);
  wire echo_pop_c = echo_visible_c && cdc_swap_ready_i;
  wire echo_room_c = !echo_valid_q || echo_pop_c;
  wire pending_slot_c = pending_tuple_q[82];
  wire frame_swap_take_c = vid_local_rst_n && vid_barrier_done_i &&
      !blank_cmd_i && frame_swap_valid_i && pending_q &&
      !scanout_wait_q && echo_room_c &&
      (frame_swap_slot_i == pending_slot_c);

  wire unblank_echo_event_c = unblank_candidate_q && echo_pop_c &&
      (cdc_swap_tuple_o == unblank_tuple_q);
  wire unblank_scanout_event_c = unblank_candidate_q &&
      scanout_wait_q && scanout_ack_i &&
      (scanout_tuple_q == unblank_tuple_q);
  wire unblank_complete_c = unblank_candidate_q &&
      `ZHAO_VIDEO_BRIDGE_UNBLANK_JOIN(
          unblank_echo_seen_q || unblank_echo_event_c,
          unblank_scanout_seen_q || unblank_scanout_event_c);

  assign cdc_ready_ready_o = vid_local_rst_n && vid_barrier_done_i &&
      !blank_cmd_i && !pending_q;
  assign cdc_swap_valid_o = vid_local_rst_n && echo_visible_c;
  assign cdc_swap_tuple_o = `ZHAO_VIDEO_BRIDGE_ECHO_TUPLE(
      echo_tuple_q, cdc_ready_tuple_i);

  always_comb begin
    frame_slot_ready_o = 2'b00;
    if (vid_local_rst_n && vid_barrier_done_i && !blank_cmd_i && pending_q &&
        !scanout_wait_q && echo_room_c)
      frame_slot_ready_o = `ZHAO_VIDEO_BRIDGE_SLOT_ONEHOT(pending_slot_c);
  end

  assign blank_ack_o = vid_local_rst_n &&
      `ZHAO_VIDEO_BRIDGE_BLANK_ACK(blank_ack_q, blank_cmd_i);
  assign pending_o = pending_q;
  assign pending_tuple_o = pending_tuple_q;
  assign echo_hold_o = echo_valid_q;
  assign blank_active_o = blank_active_q;
  assign scanout_wait_o = scanout_wait_q;
  assign unblank_candidate_o = unblank_candidate_q;
  assign unblank_tuple_o = unblank_tuple_q;
  assign unblank_echo_seen_o = unblank_echo_seen_q;
  assign unblank_scanout_seen_o = unblank_scanout_seen_q;

  always_ff @(posedge vid_clk or negedge vid_local_rst_n) begin
    if (!vid_local_rst_n) begin
      pending_q <= `ZHAO_VIDEO_BRIDGE_RESET_VALUE(pending_q);
      pending_tuple_q <= `ZHAO_VIDEO_BRIDGE_RESET_VALUE(pending_tuple_q);
      echo_valid_q <= `ZHAO_VIDEO_BRIDGE_RESET_VALUE(echo_valid_q);
      echo_tuple_q <= `ZHAO_VIDEO_BRIDGE_RESET_VALUE(echo_tuple_q);
      blank_active_q <= 1'b1;
      blank_ack_q <= 1'b0;
      scanout_wait_q <= 1'b0;
      scanout_tuple_q <= '0;
      unblank_candidate_q <= 1'b0;
      unblank_tuple_q <= '0;
      unblank_echo_seen_q <= 1'b0;
      unblank_scanout_seen_q <= 1'b0;
      output_px_o <= '0;
    end else begin
      // The proof intentionally observes the PREVIOUSLY registered pixel.  It
      // can therefore never lead the register that actually forced RGB black.
      blank_ack_q <= blank_active_q &&
          (output_px_o.rgb565 == BLACK_RGB565);

      output_px_o <= scanout_px_i;
      if (blank_active_q || blank_cmd_i)
        output_px_o.rgb565 <= BLACK_RGB565;

      // Blanking is not a protocol reset.  Owned tuples still retire normally;
      // only either external reset may discard pending or reverse-held work.
      if (echo_pop_c)
        echo_valid_q <= 1'b0;

      if (cdc_ready_valid_i && cdc_ready_ready_o) begin
        pending_q <= 1'b1;
        pending_tuple_q <= cdc_ready_tuple_i;
      end

      if (scanout_wait_q && scanout_ack_i)
        scanout_wait_q <= 1'b0;

      if (blank_cmd_i) begin
        // A newly commanded blank requires a later swap; acknowledgements for a
        // swap begun before the command cannot unblank the new epoch.
        blank_active_q <= 1'b1;
        unblank_candidate_q <= 1'b0;
        unblank_tuple_q <= '0;
        unblank_echo_seen_q <= 1'b0;
        unblank_scanout_seen_q <= 1'b0;
      end else begin
        if (unblank_echo_event_c)
          unblank_echo_seen_q <= 1'b1;
        if (unblank_scanout_event_c)
          unblank_scanout_seen_q <= 1'b1;

        if (unblank_complete_c) begin
          blank_active_q <= 1'b0;
          unblank_candidate_q <= 1'b0;
          unblank_echo_seen_q <= 1'b0;
          unblank_scanout_seen_q <= 1'b0;
        end

        // This assignment follows the pop assignment deliberately: a reverse
        // acceptance and a new FRAMECTL swap may replace the echo without a
        // bubble, while the exact old tuple is what the CDC accepted that edge.
        if (frame_swap_take_c) begin
          pending_q <= 1'b0;
          echo_valid_q <= 1'b1;
          echo_tuple_q <= pending_tuple_q;
          scanout_wait_q <= 1'b1;
          scanout_tuple_q <= pending_tuple_q;

          if (blank_active_q && !unblank_candidate_q) begin
            unblank_candidate_q <= 1'b1;
            unblank_tuple_q <= pending_tuple_q;
            unblank_echo_seen_q <= 1'b0;
            unblank_scanout_seen_q <= 1'b0;
          end
        end
      end
    end
  end

  (* ASYNC_REG = "TRUE" *) logic [2:0] vid_barrier_gpu_q;
  (* ASYNC_REG = "TRUE" *) logic [2:0] blank_ack_gpu_q;

  always_ff @(posedge gpu_clk or negedge gpu_local_rst_n) begin
    if (!gpu_local_rst_n) begin
      vid_barrier_gpu_q <= 3'b000;
      blank_ack_gpu_q <= 3'b000;
      lease_open_o <= 1'b0;
    end else begin
      vid_barrier_gpu_q <= {vid_barrier_gpu_q[1:0], vid_barrier_done_i};
      blank_ack_gpu_q <= {blank_ack_gpu_q[1:0], blank_ack_o};

      if (!gpu_barrier_done_i || !vid_barrier_gpu_q[2])
        lease_open_o <= 1'b0;
      else if (`ZHAO_VIDEO_BRIDGE_LEASE_GATE(
                   gpu_barrier_done_i, vid_barrier_gpu_q[2],
                   blank_ack_gpu_q[2]))
        lease_open_o <= 1'b1;
    end
  end
  /* verilator lint_on SYNCASYNCNET */

`ifndef SYNTHESIS
`ifndef QUARTUS_SYNTHESIS
  always_ff @(posedge vid_clk) begin
    if (vid_local_rst_n) begin
      assert ($onehot0(frame_slot_ready_o))
        else $error("video_ready_bridge_v2: FRAMECTL readiness is not one-hot");
      if (blank_ack_o)
        assert (output_px_o.rgb565 == BLACK_RGB565)
          else $error("video_ready_bridge_v2: blank ack led registered black");
      if (echo_valid_q && !cdc_swap_ready_i) begin
        assert (cdc_swap_valid_o && (cdc_swap_tuple_o == echo_tuple_q))
          else $error("video_ready_bridge_v2: held reverse tuple changed/dropped");
      end
      if ($past(vid_local_rst_n && pending_q && !frame_swap_take_c)) begin
        assert (pending_q && $stable(pending_tuple_q))
          else $error("video_ready_bridge_v2: pending READY tuple changed/dropped");
      end
      if (unblank_echo_seen_q)
        assert (unblank_candidate_q)
          else $error("video_ready_bridge_v2: orphan unblank echo fact");
      if (unblank_scanout_seen_q)
        assert (unblank_candidate_q)
          else $error("video_ready_bridge_v2: orphan unblank scanout fact");
    end
  end
`endif
`endif

endmodule : zhao_video_ready_bridge_v2

`undef ZHAO_VIDEO_BRIDGE_ECHO_VALID
`undef ZHAO_VIDEO_BRIDGE_ECHO_TUPLE
`undef ZHAO_VIDEO_BRIDGE_SLOT_ONEHOT
`undef ZHAO_VIDEO_BRIDGE_BLANK_ACK
`undef ZHAO_VIDEO_BRIDGE_LEASE_GATE
`undef ZHAO_VIDEO_BRIDGE_RESET_VALUE
`undef ZHAO_VIDEO_BRIDGE_UNBLANK_JOIN
`ifdef ZHAO_VIDEO_BRIDGE_MUTANT_COLLISION
  `undef ZHAO_VIDEO_BRIDGE_MUTANT_COLLISION
`endif
`default_nettype wire
