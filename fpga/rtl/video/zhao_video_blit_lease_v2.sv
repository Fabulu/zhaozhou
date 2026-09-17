// zhao_video_blit_lease_v2.sv -- Packet-H blitter lease front end.
//
// The writer-0 half of the V2 lease protocol, and the piece Packet H was
// missing. `zhao_renderer_lease_v2` acquires writer 1's lease and
// `zhao_video_terminal_adapter_v2` already carries BOTH writers' terminal
// events back to the manager -- but nothing acquired the blitter's lease, so
// `zhao_debug_frameblit` had no way to be told which slot and generation it
// owns. This leaf is that path, and it is deliberately a sibling of the
// renderer lease rather than glue inside the shell: the two writers contend for
// one channel, and a protocol participant that lives as loose always_ff in a
// 2,000-line top is one nobody can test on its own.
//
// WHAT THIS PRESERVES FROM THE HISTORICAL SHELL. `zhao_shell_top.sv` carries a
// four-state sequencer (`lseq`) and a comment that is the whole reason it
// exists:
//
//     DEBUG.FRAMEBLIT latches `fb_lease_generation_i` on the SAME edge it
//     accepts a request. So the lease must already be granted when the request
//     arrives: asking for the lease and handing over the request on one edge
//     makes the blitter latch the generation from BEFORE the grant, and every
//     publication is then refused as stale by a slot manager that is working
//     perfectly.
//
// That law did not go away with the protocol. It is enforced here structurally:
// the lease record is captured into registers when the response lands, and
// `blit_req_valid_o` is raised in a LATER state, so the record is stable across
// the accepting edge and cannot be the pre-grant one. `ZHAO_BLIT_LEASE_ISSUE`
// is the seam that breaks it, because a law with no way to fire it is a
// comment.
//
// AND WHAT IT PRESERVES DELIBERATELY. A REFUSED lease still issues the request,
// with `fb_lease_valid_o` low. That is the historical behaviour and its reason
// is unchanged: a refused lease is not the shell's to judge, the blitter
// answers it with ST_NO_LEASE, and that is a status somebody can read. Dropping
// the request instead would make a refusal silent, and would change `nb_status`
// on a path the Packet-H gate requires to match the old shell.
`default_nettype none

`ifndef ZHAO_BLIT_LEASE_ADMISSION_OPEN
`define ZHAO_BLIT_LEASE_ADMISSION_OPEN(open) (open)
`endif
`ifndef ZHAO_BLIT_LEASE_RESPONSE_IS_BLIT
`define ZHAO_BLIT_LEASE_RESPONSE_IS_BLIT(writer) ((writer) == 1'b0)
`endif
`ifndef ZHAO_BLIT_LEASE_REQ_SLOT
`define ZHAO_BLIT_LEASE_REQ_SLOT(captured, current) (captured)
`endif
`ifndef ZHAO_BLIT_LEASE_REQ_MODE
`define ZHAO_BLIT_LEASE_REQ_MODE(captured, current) (captured)
`endif
`ifndef ZHAO_BLIT_LEASE_RECORD_SLOT
`define ZHAO_BLIT_LEASE_RECORD_SLOT(captured, live) (captured)
`endif
`ifndef ZHAO_BLIT_LEASE_RECORD_GENERATION
`define ZHAO_BLIT_LEASE_RECORD_GENERATION(captured, live) (captured)
`endif
`ifndef ZHAO_BLIT_LEASE_RECORD_VALID
`define ZHAO_BLIT_LEASE_RECORD_VALID(granted) (granted)
`endif
`ifndef ZHAO_BLIT_LEASE_ISSUE
`define ZHAO_BLIT_LEASE_ISSUE(in_issue, in_response) (in_issue)
`endif

module zhao_video_blit_lease_v2 (
    input  logic        clk,
    input  logic        rst_n,

    // Synchronized Packet-H reset-epoch barrier. As in the renderer lease, it
    // gates CREATION of new blit work and never retirement of work this leaf
    // already owns -- a blit in flight when the epoch closes still finishes,
    // still publishes or releases, and still returns its slot.
    input  logic        lease_open_i,

    // The shell's dispatch seam. One blit at a time, which is the historical
    // contract: `blit_req_ready` was `lseq == L_IDLE`.
    input  logic        dispatch_valid_i,
    output logic        dispatch_ready_o,
    input  logic        dispatch_slot_i,
    input  logic [1:0]  dispatch_mode_i,

    // Blitter request channel into the retained zhao_debug_frameblit.
    output logic        blit_req_valid_o,
    input  logic        blit_req_ready_i,
    input  logic        blit_done_i,

    // The lease record the blitter latches on its accepting edge. Held from
    // the response until the blit retires.
    output logic        fb_lease_valid_o,
    output logic        fb_lease_slot_o,
    output logic [15:0] fb_lease_generation_o,

    // Blitter request channel into zhao_video_slotmgr_v2.
    output logic        mgr_req_valid_o,
    input  logic        mgr_req_ready_i,
    output logic        mgr_req_slot_o,
    output logic [1:0]  mgr_req_mode_o,

    // Shared Packet-G response. This leaf raises ready only for writer 0, for
    // the same reason its sibling raises ready only for writer 1: the guard
    // belongs in the participant, not in whatever wires them together.
    input  logic        rsp_valid_i,
    output logic        rsp_ready_o,
    input  logic        rsp_writer_i,
    input  logic        rsp_granted_i,
    input  logic        rsp_slot_i,
    input  logic [15:0] rsp_generation_i,

    // Witnesses. `leases_refused_o` is the one that matters for reading a
    // stalled machine: a blitter that never shows a frame looks identical
    // whether it is being refused every time or never asking at all.
    output logic        idle_o,
    output logic [31:0] leases_acquired_o,
    output logic [31:0] leases_refused_o,
    output logic [31:0] blits_dispatched_o
);

  typedef enum logic [2:0] {
    ST_IDLE,
    ST_REQUEST,
    ST_RESPONSE,
    ST_ISSUE,
    ST_RUN
  } state_e;

  initial begin : p_packet_h_blit_selector_contract
`ifdef ZHAO_BLIT_LEASE_MUTANT_COLLISION
    $fatal(1, "ZHAO_VIDEO_BLIT_LEASE_V2_MUTANT_SELECTOR_COLLISION");
`endif
  end
`ifdef ZHAO_BLIT_LEASE_MUTANT_COLLISION
  ZHAO_VIDEO_BLIT_LEASE_V2_MUTANT_SELECTOR_COLLISION__DEFINE_EXACTLY_ONE
      u_packet_h_blit_selector_collision();
`endif

  state_e state_q;
  logic request_owned_q;
  logic request_slot_q;
  logic [1:0] request_mode_q;

  logic record_valid_q, record_slot_q;
  logic [15:0] record_generation_q;

  logic admission_open_c;
  logic dispatch_fire_c, mgr_req_fire_c, rsp_fire_c, blit_req_fire_c;

  assign admission_open_c = `ZHAO_BLIT_LEASE_ADMISSION_OPEN(lease_open_i);

  assign dispatch_ready_o = rst_n && admission_open_c && (state_q == ST_IDLE);
  assign dispatch_fire_c = dispatch_valid_i && dispatch_ready_o;

  // As in the renderer lease, the epoch level is not a mask after capture. The
  // ownership bit was only ever created through an open admission edge, and it
  // keeps this held request alive if the level later closes.
  assign mgr_req_valid_o = rst_n && (state_q == ST_REQUEST) &&
      (admission_open_c || request_owned_q);
  assign mgr_req_slot_o = `ZHAO_BLIT_LEASE_REQ_SLOT(
      request_slot_q, dispatch_slot_i);
  assign mgr_req_mode_o = `ZHAO_BLIT_LEASE_REQ_MODE(
      request_mode_q, dispatch_mode_i);
  assign mgr_req_fire_c = mgr_req_valid_o && mgr_req_ready_i;

  assign rsp_ready_o = rst_n && (state_q == ST_RESPONSE) &&
      `ZHAO_BLIT_LEASE_RESPONSE_IS_BLIT(rsp_writer_i);
  assign rsp_fire_c = rsp_valid_i && rsp_ready_o;

  // THE ONE-CYCLE LAW, STRUCTURALLY. ST_ISSUE is a state later than the
  // response, so the record below is already registered when the blitter's
  // accepting edge arrives.
  assign blit_req_valid_o = rst_n &&
      `ZHAO_BLIT_LEASE_ISSUE(state_q == ST_ISSUE, state_q == ST_RESPONSE);
  assign blit_req_fire_c = blit_req_valid_o && blit_req_ready_i;

  assign fb_lease_valid_o = record_valid_q &&
      ((state_q == ST_ISSUE) || (state_q == ST_RUN));
  assign fb_lease_slot_o = `ZHAO_BLIT_LEASE_RECORD_SLOT(
      record_slot_q, rsp_slot_i);
  assign fb_lease_generation_o = `ZHAO_BLIT_LEASE_RECORD_GENERATION(
      record_generation_q, rsp_generation_i);

  assign idle_o = (state_q == ST_IDLE);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state_q <= ST_IDLE;
      request_owned_q <= 1'b0;
      request_slot_q <= 1'b0;
      request_mode_q <= 2'd0;
      record_valid_q <= 1'b0;
      record_slot_q <= 1'b0;
      record_generation_q <= 16'd0;
      leases_acquired_o <= 32'd0;
      leases_refused_o <= 32'd0;
      blits_dispatched_o <= 32'd0;
    end else begin
      unique case (state_q)
        ST_IDLE: begin
          if (dispatch_fire_c) begin
            request_owned_q <= 1'b1;
            request_slot_q <= dispatch_slot_i;
            request_mode_q <= dispatch_mode_i;
            record_valid_q <= 1'b0;
            state_q <= ST_REQUEST;
          end
        end

        ST_REQUEST: begin
          if (mgr_req_fire_c) state_q <= ST_RESPONSE;
        end

        ST_RESPONSE: begin
          if (rsp_fire_c) begin
            // The complete manager answer is frozen here, refused or not. A
            // refusal records nothing to own but still advances, because the
            // request must reach the blitter either way.
            record_valid_q <= `ZHAO_BLIT_LEASE_RECORD_VALID(rsp_granted_i);
            record_slot_q <= rsp_slot_i;
            record_generation_q <= rsp_generation_i;
            if (rsp_granted_i) leases_acquired_o <= leases_acquired_o + 32'd1;
            else leases_refused_o <= leases_refused_o + 32'd1;
            state_q <= ST_ISSUE;
          end
        end

        ST_ISSUE: begin
          if (blit_req_fire_c) begin
            blits_dispatched_o <= blits_dispatched_o + 32'd1;
            state_q <= ST_RUN;
          end
        end

        default: begin
          if (blit_done_i) begin
            request_owned_q <= 1'b0;
            record_valid_q <= 1'b0;
            state_q <= ST_IDLE;
          end
        end
      endcase
    end
  end

`ifndef SYNTHESIS
  // No rst_n term: reset clears every state these read, and a synchronous read
  // of rst_n beside an asynchronous design is SYNCASYNCNET.
  always_ff @(posedge clk) begin
    // The writer-0 law, stated where it is enforced rather than where it is
    // wired. Its sibling carries the same assertion for writer 1.
    if ((state_q == ST_RESPONSE) && rsp_valid_i && rsp_writer_i)
      assert (!rsp_ready_o)
        else $error("blit_lease_v2: consumed the renderer's response");

    // The one-cycle law as a property rather than a comment: whenever the
    // blitter can accept, the record it is about to latch must already be
    // registered -- never a combinational view of the response pins.
    if (blit_req_valid_o && (state_q != ST_ISSUE))
      $error("blit_lease_v2: request offered outside ISSUE; the blitter would latch a pre-grant generation");
  end
`endif

endmodule : zhao_video_blit_lease_v2

`default_nettype wire
