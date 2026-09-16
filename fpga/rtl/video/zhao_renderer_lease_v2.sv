// zhao_renderer_lease_v2.sv -- Packet-H renderer lease/clear front end.
//
// Captures one legal canvas request, chooses the lowest FREE framebuffer slot,
// and holds the Packet-G renderer request until the manager accepts it. Only a
// renderer-tagged response can advance this leaf. A grant freezes the manager's
// complete identity, then one held V3 frame-fault clear must handshake before
// the corresponding frame can be admitted. Refusals retain the pending frame
// but cannot retry until the reset-epoch lease gate is open, the live lease is
// gone, and a FREE slot is observed. Closing the epoch gate never drops work
// already owned by this leaf; it gates new admission only.
//
// The caller supplies mode only. Base/span come exclusively from the manager;
// width/height/stride and the Duo view-1 boundary come exclusively from the
// accepted mode.
`default_nettype none

`ifndef ZHAO_RENDERER_LEASE_ADMISSION_OPEN
`define ZHAO_RENDERER_LEASE_ADMISSION_OPEN(open) (open)
`endif
`ifndef ZHAO_RENDERER_LEASE_PICK_SLOT
`define ZHAO_RENDERER_LEASE_PICK_SLOT(slot0_free, slot1_free) \
    ((slot0_free) ? 1'b0 : 1'b1)
`endif
`ifndef ZHAO_RENDERER_LEASE_REQ_SLOT
`define ZHAO_RENDERER_LEASE_REQ_SLOT(captured, current) (captured)
`endif
`ifndef ZHAO_RENDERER_LEASE_REQ_MODE
`define ZHAO_RENDERER_LEASE_REQ_MODE(captured, current) (captured)
`endif
`ifndef ZHAO_RENDERER_LEASE_RESPONSE_IS_RENDERER
`define ZHAO_RENDERER_LEASE_RESPONSE_IS_RENDERER(writer) ((writer) == 1'b1)
`endif
`ifndef ZHAO_RENDERER_LEASE_CLEAR_REQUIRED
`define ZHAO_RENDERER_LEASE_CLEAR_REQUIRED 1'b1
`endif
`ifndef ZHAO_RENDERER_LEASE_DUO_STRIDE
`define ZHAO_RENDERER_LEASE_DUO_STRIDE 16'd512
`endif
`ifndef ZHAO_RENDERER_LEASE_DUO_VIEW1_OFFSET
`define ZHAO_RENDERER_LEASE_DUO_VIEW1_OFFSET 32'h0001_8000
`endif
`ifndef ZHAO_RENDERER_LEASE_FRAME_WRITER
`define ZHAO_RENDERER_LEASE_FRAME_WRITER(captured, live) (captured)
`endif
`ifndef ZHAO_RENDERER_LEASE_FRAME_SLOT
`define ZHAO_RENDERER_LEASE_FRAME_SLOT(captured, live) (captured)
`endif
`ifndef ZHAO_RENDERER_LEASE_FRAME_GENERATION
`define ZHAO_RENDERER_LEASE_FRAME_GENERATION(captured, live) (captured)
`endif
`ifndef ZHAO_RENDERER_LEASE_FRAME_MODE
`define ZHAO_RENDERER_LEASE_FRAME_MODE(captured, live) (captured)
`endif
`ifndef ZHAO_RENDERER_LEASE_FRAME_BASE
`define ZHAO_RENDERER_LEASE_FRAME_BASE(captured, live) (captured)
`endif
`ifndef ZHAO_RENDERER_LEASE_FRAME_SPAN
`define ZHAO_RENDERER_LEASE_FRAME_SPAN(captured, live) (captured)
`endif

module zhao_renderer_lease_v2
  import zhao_pkg::*;
(
    input  logic        clk,
    input  logic        rst_n,
    // Synchronized Packet-H reset-epoch barrier. It gates creation of new
    // renderer work, never retirement of work this leaf already owns.
    input  logic        lease_open_i,

    // One held upstream frame request. Mode 3 is not a canvas and is never
    // accepted, so an illegal request cannot become an infinite retry loop.
    input  logic        frame_req_valid_i,
    output logic        frame_req_ready_o,
    input  logic [1:0]  frame_req_mode_i,

    // Current Packet-G ownership observations. state encoding is the manager's
    // FREE=0, WRITING=1, READY=2, DISPLAYED=3 contract.
    input  logic        lease_valid_i,
    input  logic [1:0]  slot_state_i [0:1],

    // Renderer request channel into zhao_video_slotmgr_v2.
    output logic        render_req_valid_o,
    input  logic        render_req_ready_i,
    output logic        render_req_slot_o,
    output logic [1:0]  render_req_mode_o,

    // Shared Packet-G response. This leaf raises ready only for writer 1.
    input  logic        rsp_valid_i,
    output logic        rsp_ready_o,
    input  logic        rsp_writer_i,
    input  logic        rsp_granted_i,
    input  logic        rsp_slot_i,
    input  logic [15:0] rsp_generation_i,
    input  logic [1:0]  rsp_mode_i,
    input  logic [31:0] rsp_base_i,
    input  logic [31:0] rsp_span_i,

    // Sole Packet-H driver of the V3 recoverable frame-clear handshake.
    output logic        frame_fault_clear_valid_o,
    input  logic        frame_fault_clear_ready_i,

    // Exactly one held admitted frame follows the accepted clear. The identity
    // is the complete immutable manager response; geometry is mode-derived.
    output logic        frame_valid_o,
    input  logic        frame_ready_i,
    output logic        frame_writer_o,
    output logic        frame_slot_o,
    output logic [15:0] frame_generation_o,
    output logic [1:0]  frame_mode_o,
    output logic [31:0] frame_base_o,
    output logic [31:0] frame_span_o,
    output logic [15:0] frame_width_o,
    output logic [15:0] frame_height_o,
    output logic [15:0] frame_stride_o,
    output logic [15:0] frame_view1_y_o,
    output logic [31:0] frame_view1_offset_o
);

  localparam logic [1:0] S_FREE = 2'd0;

  typedef enum logic [2:0] {
    ST_IDLE,
    ST_REQUEST,
    ST_RESPONSE,
    ST_RETRY,
    ST_CLEAR,
    ST_FRAME
  } state_e;

  initial begin : p_packet_h_selector_contract
`ifdef ZHAO_RENDERER_LEASE_MUTANT_COLLISION
    $fatal(1, "ZHAO_RENDERER_LEASE_V2_MUTANT_SELECTOR_COLLISION");
`endif
  end
`ifdef ZHAO_RENDERER_LEASE_MUTANT_COLLISION
  ZHAO_RENDERER_LEASE_V2_MUTANT_SELECTOR_COLLISION__DEFINE_EXACTLY_ONE
      u_packet_h_selector_collision();
`endif

  state_e state_q;
  logic request_owned_q;
  logic request_slot_q;
  logic [1:0] request_mode_q;

  logic identity_writer_q, identity_slot_q;
  logic [15:0] identity_generation_q;
  logic [1:0] identity_mode_q;
  logic [31:0] identity_base_q, identity_span_q;

  logic slot0_free_c, slot1_free_c, any_free_c, current_pick_c;
  logic admission_open_c;
  logic frame_req_fire_c, render_req_fire_c, rsp_fire_c;
  logic clear_fire_c, frame_fire_c;

  function automatic logic mode_legal(input logic [1:0] mode);
    mode_legal = mode != 2'd3;
  endfunction

  function automatic logic [15:0] mode_width(input logic [1:0] mode);
    unique case (mode)
      2'd0: mode_width = 16'd384;
      2'd1: mode_width = 16'd320;
      2'd2: mode_width = 16'd256;
      default: mode_width = 16'd0;
    endcase
  endfunction

  function automatic logic [15:0] mode_height(input logic [1:0] mode);
    unique case (mode)
      2'd0, 2'd1: mode_height = 16'd240;
      2'd2:       mode_height = 16'd384;
      default:    mode_height = 16'd0;
    endcase
  endfunction

  function automatic logic [15:0] mode_stride(input logic [1:0] mode);
    unique case (mode)
      2'd0: mode_stride = 16'd768;
      2'd1: mode_stride = 16'd640;
      2'd2: mode_stride = `ZHAO_RENDERER_LEASE_DUO_STRIDE;
      default: mode_stride = 16'd0;
    endcase
  endfunction

  assign slot0_free_c = slot_state_i[0] == S_FREE;
  assign slot1_free_c = slot_state_i[1] == S_FREE;
  assign any_free_c = slot0_free_c || slot1_free_c;
  assign current_pick_c = `ZHAO_RENDERER_LEASE_PICK_SLOT(
      slot0_free_c, slot1_free_c);
  assign admission_open_c =
      `ZHAO_RENDERER_LEASE_ADMISSION_OPEN(lease_open_i);

  assign frame_req_ready_o = rst_n && admission_open_c &&
      (state_q == ST_IDLE) && !lease_valid_i && any_free_c &&
      mode_legal(frame_req_mode_i);
  assign frame_req_fire_c = frame_req_valid_i && frame_req_ready_o;

  // lease_open_i is deliberately not a level-mask after capture. The ownership
  // bit was created only through an open admission edge; it keeps this held
  // request alive if the epoch level later closes.
  assign render_req_valid_o = rst_n && (state_q == ST_REQUEST) &&
      (admission_open_c || request_owned_q);
  assign render_req_slot_o = `ZHAO_RENDERER_LEASE_REQ_SLOT(
      request_slot_q, current_pick_c);
  assign render_req_mode_o = `ZHAO_RENDERER_LEASE_REQ_MODE(
      request_mode_q, frame_req_mode_i);
  assign render_req_fire_c = render_req_valid_o && render_req_ready_i;

  assign rsp_ready_o = rst_n && (state_q == ST_RESPONSE) &&
      `ZHAO_RENDERER_LEASE_RESPONSE_IS_RENDERER(rsp_writer_i);
  assign rsp_fire_c = rsp_valid_i && rsp_ready_o;

  assign frame_fault_clear_valid_o = rst_n && (state_q == ST_CLEAR);
  assign clear_fire_c = frame_fault_clear_valid_o &&
      frame_fault_clear_ready_i;

  assign frame_valid_o = rst_n && (state_q == ST_FRAME);
  assign frame_fire_c = frame_valid_o && frame_ready_i;

  assign frame_writer_o = `ZHAO_RENDERER_LEASE_FRAME_WRITER(
      identity_writer_q, rsp_writer_i);
  assign frame_slot_o = `ZHAO_RENDERER_LEASE_FRAME_SLOT(
      identity_slot_q, rsp_slot_i);
  assign frame_generation_o = `ZHAO_RENDERER_LEASE_FRAME_GENERATION(
      identity_generation_q, rsp_generation_i);
  assign frame_mode_o = `ZHAO_RENDERER_LEASE_FRAME_MODE(
      identity_mode_q, rsp_mode_i);
  assign frame_base_o = `ZHAO_RENDERER_LEASE_FRAME_BASE(
      identity_base_q, rsp_base_i);
  assign frame_span_o = `ZHAO_RENDERER_LEASE_FRAME_SPAN(
      identity_span_q, rsp_span_i);

  // Geometry intentionally keys from the captured manager mode, not from the
  // upstream request pins or the later shared response bus.
  assign frame_width_o = mode_width(identity_mode_q);
  assign frame_height_o = mode_height(identity_mode_q);
  assign frame_stride_o = mode_stride(identity_mode_q);
  assign frame_view1_y_o = (identity_mode_q == 2'd2) ? 16'd192 : 16'd0;
  assign frame_view1_offset_o = (identity_mode_q == 2'd2)
      ? `ZHAO_RENDERER_LEASE_DUO_VIEW1_OFFSET : 32'd0;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state_q <= ST_IDLE;
      request_owned_q <= 1'b0;
      request_slot_q <= 1'b0;
      request_mode_q <= 2'd0;
      identity_writer_q <= 1'b1;
      identity_slot_q <= 1'b0;
      identity_generation_q <= 16'd0;
      identity_mode_q <= 2'd0;
      identity_base_q <= ZHAO_FB_SLOT0_BASE;
      identity_span_q <= 32'd0;
    end else begin
      unique case (state_q)
        ST_IDLE: begin
          if (frame_req_fire_c) begin
            request_owned_q <= 1'b1;
            request_slot_q <= current_pick_c;
            request_mode_q <= frame_req_mode_i;
            state_q <= ST_REQUEST;
          end
        end

        ST_REQUEST: begin
          if (render_req_fire_c) begin
            request_owned_q <= 1'b0;
            state_q <= ST_RESPONSE;
          end
        end

        ST_RESPONSE: begin
          if (rsp_fire_c) begin
            if (rsp_granted_i) begin
              identity_writer_q <= rsp_writer_i;
              identity_slot_q <= rsp_slot_i;
              identity_generation_q <= rsp_generation_i;
              identity_mode_q <= rsp_mode_i;
              identity_base_q <= rsp_base_i;
              identity_span_q <= rsp_span_i;
              if (`ZHAO_RENDERER_LEASE_CLEAR_REQUIRED)
                state_q <= ST_CLEAR;
              else
                state_q <= ST_FRAME;
            end else begin
              // Retain request_mode_q as the pending frame. A fresh slot and
              // manager request are created only after the epoch gate reopens,
              // the manager's live lease clears, and a FREE slot is visible.
              state_q <= ST_RETRY;
            end
          end
        end

        ST_RETRY: begin
          if (admission_open_c && !lease_valid_i && any_free_c) begin
            request_owned_q <= 1'b1;
            request_slot_q <= current_pick_c;
            state_q <= ST_REQUEST;
          end
        end

        ST_CLEAR: begin
          if (clear_fire_c)
            state_q <= ST_FRAME;
        end

        ST_FRAME: begin
          if (frame_fire_c)
            state_q <= ST_IDLE;
        end

        default: begin
          request_owned_q <= 1'b0;
          state_q <= ST_IDLE;
        end
      endcase
    end
  end

`ifndef SYNTHESIS
`ifndef QUARTUS_SYNTHESIS
  logic sim_req_stalled_q, sim_clear_stalled_q, sim_frame_stalled_q;
  logic [2:0] sim_req_tuple_q;
  logic [83:0] sim_frame_identity_q;
  logic [95:0] sim_frame_geometry_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      sim_req_stalled_q <= 1'b0;
      sim_clear_stalled_q <= 1'b0;
      sim_frame_stalled_q <= 1'b0;
      sim_req_tuple_q <= '0;
      sim_frame_identity_q <= '0;
      sim_frame_geometry_q <= '0;
    end else begin
      if (sim_req_stalled_q) begin
        assert (render_req_valid_o &&
                ({render_req_slot_o, render_req_mode_o} == sim_req_tuple_q))
          else $error("renderer_lease_v2: held manager request changed");
      end
      if (sim_clear_stalled_q)
        assert (frame_fault_clear_valid_o)
          else $error("renderer_lease_v2: held V3 clear disappeared");
      if (sim_frame_stalled_q) begin
        assert (frame_valid_o &&
                ({frame_writer_o, frame_slot_o, frame_generation_o,
                  frame_mode_o, frame_base_o, frame_span_o} ==
                 sim_frame_identity_q) &&
                ({frame_width_o, frame_height_o, frame_stride_o,
                  frame_view1_y_o, frame_view1_offset_o} ==
                 sim_frame_geometry_q))
          else $error("renderer_lease_v2: held frame changed");
      end
      assert (!(frame_fault_clear_valid_o && frame_valid_o))
        else $error("renderer_lease_v2: clear and frame overlap");
      assert (!request_owned_q || (state_q == ST_REQUEST))
        else $error("renderer_lease_v2: request ownership escaped request state");
      if (!admission_open_c && !request_owned_q)
        assert (!frame_req_ready_o && !render_req_valid_o)
          else $error("renderer_lease_v2: pre-barrier admission exposed");
      if ((state_q == ST_RESPONSE) && rsp_valid_i && !rsp_writer_i)
        assert (!rsp_ready_o)
          else $error("renderer_lease_v2: consumed non-renderer response");

      sim_req_stalled_q <= render_req_valid_o && !render_req_ready_i;
      sim_clear_stalled_q <= frame_fault_clear_valid_o &&
                            !frame_fault_clear_ready_i;
      sim_frame_stalled_q <= frame_valid_o && !frame_ready_i;
      if (render_req_valid_o && !render_req_ready_i)
        sim_req_tuple_q <= {render_req_slot_o, render_req_mode_o};
      if (frame_valid_o && !frame_ready_i) begin
        sim_frame_identity_q <= {frame_writer_o, frame_slot_o,
                                 frame_generation_o, frame_mode_o,
                                 frame_base_o, frame_span_o};
        sim_frame_geometry_q <= {frame_width_o, frame_height_o,
                                 frame_stride_o, frame_view1_y_o,
                                 frame_view1_offset_o};
      end
    end
  end
`endif
`endif

endmodule : zhao_renderer_lease_v2

`undef ZHAO_RENDERER_LEASE_ADMISSION_OPEN
`undef ZHAO_RENDERER_LEASE_PICK_SLOT
`undef ZHAO_RENDERER_LEASE_REQ_SLOT
`undef ZHAO_RENDERER_LEASE_REQ_MODE
`undef ZHAO_RENDERER_LEASE_RESPONSE_IS_RENDERER
`undef ZHAO_RENDERER_LEASE_CLEAR_REQUIRED
`undef ZHAO_RENDERER_LEASE_DUO_STRIDE
`undef ZHAO_RENDERER_LEASE_DUO_VIEW1_OFFSET
`undef ZHAO_RENDERER_LEASE_FRAME_WRITER
`undef ZHAO_RENDERER_LEASE_FRAME_SLOT
`undef ZHAO_RENDERER_LEASE_FRAME_GENERATION
`undef ZHAO_RENDERER_LEASE_FRAME_MODE
`undef ZHAO_RENDERER_LEASE_FRAME_BASE
`undef ZHAO_RENDERER_LEASE_FRAME_SPAN
`ifdef ZHAO_RENDERER_LEASE_MUTANT_COLLISION
  `undef ZHAO_RENDERER_LEASE_MUTANT_COLLISION
`endif
`default_nettype wire
