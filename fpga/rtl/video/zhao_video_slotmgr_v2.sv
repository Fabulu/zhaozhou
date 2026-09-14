// zhao_video_slotmgr_v2.sv -- Packet-G writer-aware framebuffer lease owner.
//
// One gpu-domain FSM arbitrates renderer/blitter lease requests, derives the
// immutable slot window internally, and emits READY only on an accepted clean
// publication. VIDEO.FRAMECTL's swap returns the complete generation-bearing
// tuple; stale or fault releases never create display work.
//
// AUTHORITY: reports/SHELL-TEXTURE-V3-COMPOSITION-ARCHITECTURE-20260913.md
//            section 12.9 and Packet G.
`default_nettype none

`ifndef ZHAO_SLOT_V2_TERM_MATCH
`define ZHAO_SLOT_V2_TERM_MATCH(writer, slot, generation, lease_writer, lease_slot, lease_generation) \
    (((writer) == (lease_writer)) && ((slot) == (lease_slot)) && \
     ((generation) == (lease_generation)))
`endif
`ifndef ZHAO_SLOT_V2_SWAP_MATCH
`define ZHAO_SLOT_V2_SWAP_MATCH(writer, slot, generation, mode, base, span, stored_writer, stored_generation, stored_mode, stored_base, stored_span) \
    (((writer) == (stored_writer)) && ((generation) == (stored_generation)) && \
     ((mode) == (stored_mode)) && ((base) == (stored_base)) && \
     ((span) == (stored_span)))
`endif
`ifndef ZHAO_SLOT_V2_READY_FROM_CLEAN
`define ZHAO_SLOT_V2_READY_FROM_CLEAN(clean) (clean)
`endif
`ifndef ZHAO_SLOT_V2_READY_WRITER
`define ZHAO_SLOT_V2_READY_WRITER(captured) (captured)
`endif
`ifndef ZHAO_SLOT_V2_DERIVED_BASE
`define ZHAO_SLOT_V2_DERIVED_BASE(slot) ((slot) ? ZHAO_FB_SLOT1_BASE : ZHAO_FB_SLOT0_BASE)
`endif

module zhao_video_slotmgr_v2
  import zhao_pkg::*;
(
    input  logic        clk,
    input  logic        rst_n,

    // Two held request sources. Exactly one READY is asserted on an accepted
    // arbitration edge; simultaneous traffic alternates by accepted history.
    input  logic        render_req_valid_i,
    output logic        render_req_ready_o,
    input  logic        render_req_slot_i,
    input  logic [1:0]  render_req_mode_i,
    input  logic        blit_req_valid_i,
    output logic        blit_req_ready_o,
    input  logic        blit_req_slot_i,
    input  logic [1:0]  blit_req_mode_i,

    // One held response tagged by writer. A granted response enters WRITING only
    // when accepted, so the recipient and the live lease see the same tuple.
    output logic        rsp_valid_o,
    input  logic        rsp_ready_i,
    output logic        rsp_writer_o,       // 0 blitter, 1 renderer
    output logic        rsp_granted_o,
    output logic        rsp_slot_o,
    output logic [15:0] rsp_generation_o,
    output logic [1:0]  rsp_mode_o,
    output logic [31:0] rsp_base_o,
    output logic [31:0] rsp_span_o,

    // Sole live lease authority for both framebuffer guards.
    output logic        lease_valid_o,
    output logic        lease_writer_o,
    output logic        lease_slot_o,
    output logic [15:0] lease_generation_o,
    output logic [1:0]  lease_mode_o,
    output logic [31:0] lease_base_o,
    output logic [31:0] lease_span_o,
    output logic        lease_fault_o,

    // A matching in-flight fault is sticky in the live record. A same-edge fault
    // wins over an otherwise-clean publication.
    input  logic        fault_valid_i,
    output logic        fault_ready_o,
    input  logic        fault_writer_i,
    input  logic        fault_slot_i,
    input  logic [15:0] fault_generation_i,

    // One terminal stream. publish=1 requests READY; publish=0 is cancel/release.
    // term_fault or a previously latched lease fault always releases to FREE.
    input  logic        term_valid_i,
    output logic        term_ready_o,
    input  logic        term_writer_i,
    input  logic        term_slot_i,
    input  logic [15:0] term_generation_i,
    input  logic        term_publish_i,
    input  logic        term_fault_i,

    // Accepted clean publication, held until the READY CDC accepts it.
    output logic        ready_valid_o,
    input  logic        ready_ready_i,
    output logic        ready_writer_o,
    output logic        ready_slot_o,
    output logic [15:0] ready_generation_o,
    output logic [1:0]  ready_mode_o,
    output logic [31:0] ready_base_o,
    output logic [31:0] ready_span_o,

    // Complete tuple echoed from video after its accepted swap.
    input  logic        swap_valid_i,
    output logic        swap_ready_o,
    input  logic        swap_writer_i,
    input  logic        swap_slot_i,
    input  logic [15:0] swap_generation_i,
    input  logic [1:0]  swap_mode_i,
    input  logic [31:0] swap_base_i,
    input  logic [31:0] swap_span_i,

    output logic        displayed_valid_o,
    output logic        displayed_writer_o,
    output logic        displayed_slot_o,
    output logic [15:0] displayed_generation_o,
    output logic [1:0]  displayed_mode_o,
    output logic [31:0] displayed_base_o,
    output logic [31:0] displayed_span_o,
    output logic [1:0]  slot_state_o [0:1],

    // Modulo-32-bit lifecycle evidence.
    output logic [31:0] requests_accepted_o,
    output logic [31:0] responses_accepted_o,
    output logic [31:0] leases_granted_o,
    output logic [31:0] leases_refused_o,
    output logic [31:0] faults_latched_o,
    output logic [31:0] publications_o,
    output logic [31:0] releases_o,
    output logic [31:0] ready_events_o,
    output logic [31:0] swaps_o,
    output logic [31:0] stale_events_o,
    output logic [31:0] contentions_o
);

  localparam logic [1:0] S_FREE      = 2'd0;
  localparam logic [1:0] S_WRITING   = 2'd1;
  localparam logic [1:0] S_READY     = 2'd2;
  localparam logic [1:0] S_DISPLAYED = 2'd3;
  localparam logic       WRITER_BLIT = 1'b0;

  initial begin : p_packet_g_selector_contract
`ifdef ZHAO_SLOT_V2_MUTANT_COLLISION
    $fatal(1, "ZHAO_VIDEO_SLOTMGR_V2_MUTANT_SELECTOR_COLLISION");
`endif
  end
`ifdef ZHAO_SLOT_V2_MUTANT_COLLISION
  ZHAO_VIDEO_SLOTMGR_V2_MUTANT_SELECTOR_COLLISION__DEFINE_EXACTLY_ONE
      u_packet_g_selector_collision();
`endif

  logic [1:0] state_q [0:1];
  logic [15:0] generation_q [0:1];
  logic writer_q [0:1];
  logic [1:0] mode_q [0:1];
  logic [31:0] base_q [0:1];
  logic [31:0] span_q [0:1];

  logic lease_valid_q, lease_writer_q, lease_slot_q, lease_fault_q;
  logic [15:0] lease_generation_q;
  logic [1:0] lease_mode_q;
  logic [31:0] lease_base_q, lease_span_q;

  logic displayed_valid_q, displayed_slot_q;
  logic rr_last_render_q;

  logic rsp_valid_q, rsp_writer_q, rsp_granted_q, rsp_slot_q;
  logic [15:0] rsp_generation_q;
  logic [1:0] rsp_mode_q;
  logic [31:0] rsp_base_q, rsp_span_q;

  logic ready_valid_q, ready_writer_q, ready_slot_q;
  logic [15:0] ready_generation_q;
  logic [1:0] ready_mode_q;
  logic [31:0] ready_base_q, ready_span_q;

  function automatic logic mode_legal(input logic [1:0] mode);
    mode_legal = mode != 2'd3;
  endfunction

  function automatic logic [31:0] mode_span(input logic [1:0] mode);
    unique case (mode)
      2'd0: mode_span = ZHAO_CANVAS_BYTES_Z60;
      2'd1: mode_span = ZHAO_CANVAS_BYTES_STORM;
      2'd2: mode_span = ZHAO_CANVAS_BYTES_DUO;
      default: mode_span = 32'd0;
    endcase
  endfunction

  assign slot_state_o[0] = state_q[0];
  assign slot_state_o[1] = state_q[1];

  assign rsp_valid_o = rsp_valid_q;
  assign rsp_writer_o = rsp_writer_q;
  assign rsp_granted_o = rsp_granted_q;
  assign rsp_slot_o = rsp_slot_q;
  assign rsp_generation_o = rsp_generation_q;
  assign rsp_mode_o = rsp_mode_q;
  assign rsp_base_o = rsp_base_q;
  assign rsp_span_o = rsp_span_q;

  assign lease_valid_o = lease_valid_q;
  assign lease_writer_o = lease_writer_q;
  assign lease_slot_o = lease_slot_q;
  assign lease_generation_o = lease_generation_q;
  assign lease_mode_o = lease_mode_q;
  assign lease_base_o = lease_base_q;
  assign lease_span_o = lease_span_q;
  assign lease_fault_o = lease_fault_q;

  assign ready_valid_o = ready_valid_q || term_clean_publish_c;
  assign ready_writer_o = ready_valid_q ? ready_writer_q
      : `ZHAO_SLOT_V2_READY_WRITER(lease_writer_q);
  assign ready_slot_o = ready_valid_q ? ready_slot_q : lease_slot_q;
  assign ready_generation_o = ready_valid_q ? ready_generation_q
                                             : lease_generation_q;
  assign ready_mode_o = ready_valid_q ? ready_mode_q : lease_mode_q;
  assign ready_base_o = ready_valid_q ? ready_base_q : lease_base_q;
  assign ready_span_o = ready_valid_q ? ready_span_q : lease_span_q;

  assign displayed_valid_o = displayed_valid_q;
  assign displayed_slot_o = displayed_slot_q;
  assign displayed_writer_o = writer_q[displayed_slot_q];
  assign displayed_generation_o = generation_q[displayed_slot_q];
  assign displayed_mode_o = mode_q[displayed_slot_q];
  assign displayed_base_o = base_q[displayed_slot_q];
  assign displayed_span_o = span_q[displayed_slot_q];

  logic request_pick_render_c, request_accept_c, request_granted_c;
  logic request_slot_c;
  logic [1:0] request_mode_c;
  logic [31:0] request_base_c, request_span_c;
  logic rsp_pop_c, rsp_room_c, request_room_c;

  assign rsp_pop_c = rsp_valid_q && rsp_ready_i;
  assign rsp_room_c = !rsp_valid_q || rsp_ready_i;
  // A grant accepted on this edge creates the sole live lease. Do not reload the
  // response slot from pre-edge lease state and accidentally promise a second.
  assign request_room_c = rsp_room_c && !(rsp_pop_c && rsp_granted_q);

  always_comb begin
    request_pick_render_c = render_req_valid_i;
    if (render_req_valid_i && blit_req_valid_i)
      request_pick_render_c = !rr_last_render_q;
    else if (!render_req_valid_i)
      request_pick_render_c = 1'b0;

    request_accept_c = rst_n && request_room_c &&
        (render_req_valid_i || blit_req_valid_i);
    render_req_ready_o = request_accept_c && request_pick_render_c;
    blit_req_ready_o = request_accept_c && !request_pick_render_c;
    request_slot_c = request_pick_render_c ? render_req_slot_i : blit_req_slot_i;
    request_mode_c = request_pick_render_c ? render_req_mode_i : blit_req_mode_i;
    request_base_c = `ZHAO_SLOT_V2_DERIVED_BASE(request_slot_c);
    request_span_c = mode_span(request_mode_c);
    request_granted_c = !lease_valid_q && (state_q[request_slot_c] == S_FREE)
                      && mode_legal(request_mode_c);
  end

  logic fault_fire_c, fault_match_c;
  assign fault_ready_o = rst_n;
  assign fault_fire_c = fault_valid_i && fault_ready_o;
  assign fault_match_c = fault_fire_c && lease_valid_q &&
      `ZHAO_SLOT_V2_TERM_MATCH(fault_writer_i, fault_slot_i, fault_generation_i,
                               lease_writer_q, lease_slot_q, lease_generation_q);

  logic term_match_c, term_effective_fault_c, term_clean_publish_c;
  logic ready_room_c, term_fire_c;
  assign ready_room_c = !ready_valid_q || ready_ready_i;
  assign term_match_c = term_valid_i && lease_valid_q &&
      `ZHAO_SLOT_V2_TERM_MATCH(term_writer_i, term_slot_i, term_generation_i,
                               lease_writer_q, lease_slot_q, lease_generation_q);
  assign term_effective_fault_c = term_fault_i || lease_fault_q || fault_match_c;
  assign term_clean_publish_c = term_match_c && term_publish_i &&
      !term_effective_fault_c;
  assign term_ready_o = rst_n && (!term_clean_publish_c || ready_room_c);
  assign term_fire_c = term_valid_i && term_ready_o;

  logic swap_fire_c, swap_match_c;
  assign swap_ready_o = rst_n;
  assign swap_fire_c = swap_valid_i && swap_ready_o;
  assign swap_match_c = swap_fire_c && (state_q[swap_slot_i] == S_READY) &&
      `ZHAO_SLOT_V2_SWAP_MATCH(
          swap_writer_i, swap_slot_i, swap_generation_i, swap_mode_i,
          swap_base_i, swap_span_i, writer_q[swap_slot_i],
          generation_q[swap_slot_i], mode_q[swap_slot_i], base_q[swap_slot_i],
          span_q[swap_slot_i]);

  logic [1:0] stale_increment_c;
  always_comb begin
    stale_increment_c = 2'd0;
    if (fault_fire_c && !fault_match_c)
      stale_increment_c = stale_increment_c + 2'd1;
    if (term_fire_c && !term_match_c)
      stale_increment_c = stale_increment_c + 2'd1;
    if (swap_fire_c && !swap_match_c)
      stale_increment_c = stale_increment_c + 2'd1;
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state_q[0] <= S_FREE;
      state_q[1] <= S_FREE;
      generation_q[0] <= 16'd0;
      generation_q[1] <= 16'd0;
      writer_q[0] <= WRITER_BLIT;
      writer_q[1] <= WRITER_BLIT;
      mode_q[0] <= 2'd0;
      mode_q[1] <= 2'd0;
      base_q[0] <= ZHAO_FB_SLOT0_BASE;
      base_q[1] <= ZHAO_FB_SLOT1_BASE;
      span_q[0] <= 32'd0;
      span_q[1] <= 32'd0;
      lease_valid_q <= 1'b0;
      lease_writer_q <= WRITER_BLIT;
      lease_slot_q <= 1'b0;
      lease_generation_q <= 16'd0;
      lease_mode_q <= 2'd0;
      lease_base_q <= ZHAO_FB_SLOT0_BASE;
      lease_span_q <= 32'd0;
      lease_fault_q <= 1'b0;
      displayed_valid_q <= 1'b0;
      displayed_slot_q <= 1'b0;
      rr_last_render_q <= 1'b0;
      rsp_valid_q <= 1'b0;
      rsp_writer_q <= WRITER_BLIT;
      rsp_granted_q <= 1'b0;
      rsp_slot_q <= 1'b0;
      rsp_generation_q <= 16'd0;
      rsp_mode_q <= 2'd0;
      rsp_base_q <= ZHAO_FB_SLOT0_BASE;
      rsp_span_q <= 32'd0;
      ready_valid_q <= 1'b0;
      ready_writer_q <= WRITER_BLIT;
      ready_slot_q <= 1'b0;
      ready_generation_q <= 16'd0;
      ready_mode_q <= 2'd0;
      ready_base_q <= ZHAO_FB_SLOT0_BASE;
      ready_span_q <= 32'd0;
      requests_accepted_o <= 32'd0;
      responses_accepted_o <= 32'd0;
      leases_granted_o <= 32'd0;
      leases_refused_o <= 32'd0;
      faults_latched_o <= 32'd0;
      publications_o <= 32'd0;
      releases_o <= 32'd0;
      ready_events_o <= 32'd0;
      swaps_o <= 32'd0;
      stale_events_o <= 32'd0;
      contentions_o <= 32'd0;
    end else begin
      if (rsp_room_c) begin
        rsp_valid_q <= request_accept_c;
        if (request_accept_c) begin
          rsp_writer_q <= request_pick_render_c;
          rsp_granted_q <= request_granted_c;
          rsp_slot_q <= request_slot_c;
          rsp_generation_q <= generation_q[request_slot_c] + 16'd1;
          rsp_mode_q <= request_mode_c;
          rsp_base_q <= request_base_c;
          rsp_span_q <= request_span_c;
        end
      end
      if (request_accept_c) begin
        // Only an accepted contention changes contention priority. A held loser
        // may later be accepted alone (and refused while the winning lease is
        // live); that solo disposition must not erase who won the contention.
        if (render_req_valid_i && blit_req_valid_i)
          rr_last_render_q <= request_pick_render_c;
        requests_accepted_o <= requests_accepted_o + 32'd1;
        if (render_req_valid_i && blit_req_valid_i)
          contentions_o <= contentions_o + 32'd1;
      end

      if (rsp_pop_c) begin
        responses_accepted_o <= responses_accepted_o + 32'd1;
        if (rsp_granted_q) begin
          state_q[rsp_slot_q] <= S_WRITING;
          generation_q[rsp_slot_q] <= rsp_generation_q;
          writer_q[rsp_slot_q] <= rsp_writer_q;
          mode_q[rsp_slot_q] <= rsp_mode_q;
          base_q[rsp_slot_q] <= rsp_base_q;
          span_q[rsp_slot_q] <= rsp_span_q;
          lease_valid_q <= 1'b1;
          lease_writer_q <= rsp_writer_q;
          lease_slot_q <= rsp_slot_q;
          lease_generation_q <= rsp_generation_q;
          lease_mode_q <= rsp_mode_q;
          lease_base_q <= rsp_base_q;
          lease_span_q <= rsp_span_q;
          lease_fault_q <= 1'b0;
          leases_granted_o <= leases_granted_o + 32'd1;
        end else begin
          leases_refused_o <= leases_refused_o + 32'd1;
        end
      end

      if (fault_match_c) begin
        lease_fault_q <= 1'b1;
        faults_latched_o <= faults_latched_o + 32'd1;
      end

      if (ready_room_c) begin
        ready_valid_q <= 1'b0;
        if (term_fire_c &&
            `ZHAO_SLOT_V2_READY_FROM_CLEAN(term_clean_publish_c) &&
            (ready_valid_q || !ready_ready_i)) begin
          // With an empty hold and a ready CDC, the combinational READY output
          // is consumed on this same terminal edge. Otherwise retain the new
          // tuple here (including pop-and-reload of an older held tuple).
          ready_valid_q <= 1'b1;
          ready_writer_q <= `ZHAO_SLOT_V2_READY_WRITER(lease_writer_q);
          ready_slot_q <= lease_slot_q;
          ready_generation_q <= lease_generation_q;
          ready_mode_q <= lease_mode_q;
          ready_base_q <= lease_base_q;
          ready_span_q <= lease_span_q;
        end
      end

      if (term_fire_c && term_match_c) begin
        lease_valid_q <= 1'b0;
        lease_fault_q <= 1'b0;
        if (`ZHAO_SLOT_V2_READY_FROM_CLEAN(term_clean_publish_c)) begin
          state_q[lease_slot_q] <= S_READY;
          publications_o <= publications_o + 32'd1;
          ready_events_o <= ready_events_o + 32'd1;
        end else begin
          state_q[lease_slot_q] <= S_FREE;
          releases_o <= releases_o + 32'd1;
        end
      end

      if (swap_match_c) begin
        if (displayed_valid_q && (displayed_slot_q != swap_slot_i))
          state_q[displayed_slot_q] <= S_FREE;
        state_q[swap_slot_i] <= S_DISPLAYED;
        displayed_valid_q <= 1'b1;
        displayed_slot_q <= swap_slot_i;
        swaps_o <= swaps_o + 32'd1;
      end

      if (stale_increment_c != 2'd0)
        stale_events_o <= stale_events_o + 32'(stale_increment_c);
    end
  end

`ifndef SYNTHESIS
`ifndef QUARTUS_SYNTHESIS
  logic sim_rsp_stalled_q, sim_ready_stalled_q;
  logic [84:0] sim_rsp_tuple_q;
  logic [83:0] sim_ready_tuple_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      sim_rsp_stalled_q <= 1'b0;
      sim_ready_stalled_q <= 1'b0;
      sim_rsp_tuple_q <= '0;
      sim_ready_tuple_q <= '0;
    end else begin
      if (sim_rsp_stalled_q) begin
        assert (rsp_valid_q &&
                ({rsp_writer_q, rsp_granted_q, rsp_slot_q,
                  rsp_generation_q, rsp_mode_q, rsp_base_q, rsp_span_q} ==
                 sim_rsp_tuple_q))
          else $error("slotmgr_v2: held response changed");
      end
      if (sim_ready_stalled_q) begin
        assert (ready_valid_q &&
                ({ready_writer_q, ready_slot_q, ready_generation_q,
                  ready_mode_q, ready_base_q, ready_span_q} ==
                 sim_ready_tuple_q))
          else $error("slotmgr_v2: held READY tuple changed");
      end
      sim_rsp_stalled_q <= rsp_valid_q && !rsp_ready_i;
      sim_ready_stalled_q <= ready_valid_q && !ready_ready_i;
      if (rsp_valid_q && !rsp_ready_i)
        sim_rsp_tuple_q <= {rsp_writer_q, rsp_granted_q, rsp_slot_q,
                            rsp_generation_q, rsp_mode_q, rsp_base_q,
                            rsp_span_q};
      if (ready_valid_q && !ready_ready_i)
        sim_ready_tuple_q <= {ready_writer_q, ready_slot_q,
                              ready_generation_q, ready_mode_q, ready_base_q,
                              ready_span_q};
      assert (!(lease_valid_q && (state_q[lease_slot_q] != S_WRITING)))
        else $error("slotmgr_v2: live lease does not name WRITING state");
      assert (!((state_q[0] == S_DISPLAYED) &&
                (state_q[1] == S_DISPLAYED)))
        else $error("slotmgr_v2: two displayed slots");
    end
  end
`endif
`endif

endmodule : zhao_video_slotmgr_v2

`undef ZHAO_SLOT_V2_TERM_MATCH
`undef ZHAO_SLOT_V2_SWAP_MATCH
`undef ZHAO_SLOT_V2_READY_FROM_CLEAN
`undef ZHAO_SLOT_V2_READY_WRITER
`undef ZHAO_SLOT_V2_DERIVED_BASE
`ifdef ZHAO_SLOT_V2_MUTANT_COLLISION
  `undef ZHAO_SLOT_V2_MUTANT_COLLISION
`endif
`default_nettype wire
