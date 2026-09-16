// zhao_video_terminal_adapter_v2.sv -- Packet-H terminal pulse/stream join.
//
// The retained blitter emits one-cycle publish/release pulses, while the renderer
// owns a conventional held ready/valid terminal stream.  This one-entry adapter
// resolves release/fault before clean publication, captures exactly one complete
// {writer,slot,generation,publish,fault} tuple, and holds it until the V2 slot
// manager accepts it.  A blitter pulse which cannot enter is never silent: its
// logical event count is reported by a pulse and saturating refusal counter.
`default_nettype none

`ifndef ZHAO_VIDEO_TERM_VALID
`define ZHAO_VIDEO_TERM_VALID(occupied, ready) (occupied)
`endif
`ifndef ZHAO_VIDEO_TERM_TUPLE
`define ZHAO_VIDEO_TERM_TUPLE(held, live) (held)
`endif
`ifndef ZHAO_VIDEO_TERM_WRITER
`define ZHAO_VIDEO_TERM_WRITER(writer) (writer)
`endif
`ifndef ZHAO_VIDEO_TERM_RELEASE_CHOICE
`define ZHAO_VIDEO_TERM_RELEASE_CHOICE(release_valid, publish_valid, same_key) \
    (release_valid)
`endif
`ifndef ZHAO_VIDEO_TERM_RENDER_READY
`define ZHAO_VIDEO_TERM_RENDER_READY(selected, room, render_valid) (selected)
`endif
`ifndef ZHAO_VIDEO_TERM_REFUSE_DELTA
`define ZHAO_VIDEO_TERM_REFUSE_DELTA(delta) (delta)
`endif
`ifndef ZHAO_VIDEO_TERM_ROOM
`define ZHAO_VIDEO_TERM_ROOM(occupied, pop) (!(occupied) || (pop))
`endif

module zhao_video_terminal_adapter_v2 (
    input  logic        clk,
    input  logic        rst_n,

    // Retained FRAMEBLIT one-cycle terminal pulses.  Publish and release retain
    // their own keys because malformed simultaneous pulses need not agree.
    input  logic        blit_publish_valid_i,
    input  logic        blit_publish_slot_i,
    input  logic [15:0] blit_publish_generation_i,
    input  logic        blit_release_valid_i,
    input  logic        blit_release_slot_i,
    input  logic [15:0] blit_release_generation_i,
    output logic        blit_refused_o,
    output logic [31:0] blit_events_refused_o,

    // Renderer terminal stream.  The source retains this tuple while READY is
    // low.  fault or publish=0 is release-class and outranks clean publication.
    input  logic        renderer_term_valid_i,
    output logic        renderer_term_ready_o,
    input  logic        renderer_term_slot_i,
    input  logic [15:0] renderer_term_generation_i,
    input  logic        renderer_term_publish_i,
    input  logic        renderer_term_fault_i,

    // Sole held terminal stream into zhao_video_slotmgr_v2.
    output logic        term_valid_o,
    input  logic        term_ready_i,
    output logic        term_writer_o,
    output logic        term_slot_o,
    output logic [15:0] term_generation_o,
    output logic        term_publish_o,
    output logic        term_fault_o,

    // Integration witnesses and conservation counters.
    output logic        occupied_o,
    output logic        idle_o,
    output logic [31:0] source_events_captured_o,
    output logic [31:0] manager_terms_accepted_o
);

  localparam int unsigned TUPLE_W = 20;

  initial begin : p_packet_h_terminal_selector_contract
`ifdef ZHAO_VIDEO_TERM_MUTANT_COLLISION
    $fatal(1, "ZHAO_VIDEO_TERMINAL_ADAPTER_V2_MUTANT_SELECTOR_COLLISION");
`endif
  end
`ifdef ZHAO_VIDEO_TERM_MUTANT_COLLISION
  ZHAO_VIDEO_TERMINAL_ADAPTER_V2_MUTANT_SELECTOR_COLLISION__DEFINE_EXACTLY_ONE
      u_packet_h_terminal_selector_collision();
`endif

  /* verilator lint_off UNUSEDSIGNAL */
  logic                 occupied_q;
  logic [TUPLE_W-1:0]   tuple_q;
  /* verilator lint_on UNUSEDSIGNAL */

  logic                 pop_c;
  logic                 room_c;
  logic                 blit_has_c;
  logic                 blit_same_key_c;
  logic                 blit_use_release_c;
  logic [1:0]           blit_event_count_c;
  logic                 renderer_release_c;
  logic                 select_blit_c;
  logic                 select_renderer_c;
  /* verilator lint_off UNUSEDSIGNAL */
  logic [1:0]           blit_refused_delta_raw_c;
  /* verilator lint_on UNUSEDSIGNAL */
  logic [1:0]           blit_refused_delta_c;
  logic                 push_c;
  logic [TUPLE_W-1:0]   push_tuple_c;
  /* verilator lint_off UNUSEDSIGNAL */
  logic [TUPLE_W-1:0]   live_tuple_c;
  /* verilator lint_on UNUSEDSIGNAL */
  logic [TUPLE_W-1:0]   visible_tuple_c;

  function automatic logic [31:0] sat_inc32(input logic [31:0] value);
    sat_inc32 = (&value) ? value : value + 32'd1;
  endfunction

  function automatic logic [31:0] sat_add_refused(
      input logic [31:0] value,
      input logic [1:0]  delta
  );
    logic [32:0] wide;
    begin
      wide = {1'b0, value} + {{31{1'b0}}, delta};
      sat_add_refused = wide[32] ? 32'hffff_ffff : wide[31:0];
    end
  endfunction

  assign blit_has_c = blit_publish_valid_i || blit_release_valid_i;
  assign blit_same_key_c = blit_publish_valid_i && blit_release_valid_i
      && (blit_publish_slot_i == blit_release_slot_i)
      && (blit_publish_generation_i == blit_release_generation_i);
  assign blit_use_release_c = `ZHAO_VIDEO_TERM_RELEASE_CHOICE(
      blit_release_valid_i, blit_publish_valid_i, blit_same_key_c);
  assign blit_event_count_c = !blit_has_c ? 2'd0
      : ((blit_publish_valid_i && blit_release_valid_i && !blit_same_key_c)
          ? 2'd2 : 2'd1);
  assign renderer_release_c = renderer_term_fault_i
      || !renderer_term_publish_i;

  assign term_valid_o = rst_n
      && `ZHAO_VIDEO_TERM_VALID(occupied_q, term_ready_i);
  assign pop_c = term_valid_o && term_ready_i;
  assign room_c = rst_n && `ZHAO_VIDEO_TERM_ROOM(occupied_q, pop_c);

  // Release-class events outrank clean publication.  Within one class the
  // unbackpressurable blitter wins; a losing renderer remains held by READY=0.
  always_comb begin
    select_blit_c = 1'b0;
    select_renderer_c = 1'b0;
    if (room_c) begin
      if (blit_has_c && blit_use_release_c) begin
        select_blit_c = 1'b1;
      end else if (renderer_term_valid_i && renderer_release_c) begin
        select_renderer_c = 1'b1;
      end else if (blit_has_c) begin
        select_blit_c = 1'b1;
      end else if (renderer_term_valid_i) begin
        select_renderer_c = 1'b1;
      end
    end
  end

  assign renderer_term_ready_o = rst_n
      && `ZHAO_VIDEO_TERM_RENDER_READY(
          select_renderer_c, room_c, renderer_term_valid_i);
  assign push_c = select_blit_c
      || (select_renderer_c && renderer_term_valid_i);

  always_comb begin
    push_tuple_c = '0;
    if (select_blit_c) begin
      if (blit_use_release_c) begin
        push_tuple_c = {1'b0, blit_release_slot_i,
                        blit_release_generation_i, 1'b0, 1'b0};
      end else begin
        push_tuple_c = {1'b0, blit_publish_slot_i,
                        blit_publish_generation_i, 1'b1, 1'b0};
      end
    end else if (select_renderer_c) begin
      push_tuple_c = {1'b1, renderer_term_slot_i,
                      renderer_term_generation_i,
                      renderer_term_publish_i && !renderer_term_fault_i,
                      renderer_term_fault_i};
    end
  end

  // A matching publish+release pair is one resolved release.  Different keys
  // are two events: the release is captured and the publication is refused.
  always_comb begin
    blit_refused_delta_raw_c = 2'd0;
    if (blit_has_c) begin
      if (!select_blit_c)
        blit_refused_delta_raw_c = blit_event_count_c;
      else if (blit_event_count_c == 2'd2)
        blit_refused_delta_raw_c = 2'd1;
    end
  end
  assign blit_refused_delta_c = `ZHAO_VIDEO_TERM_REFUSE_DELTA(
      blit_refused_delta_raw_c);
  assign blit_refused_o = rst_n && (blit_refused_delta_c != 2'd0);

  // The live bus is deliberately unrelated to the retained output.  The
  // CHANGE_HELD mutant substitutes it to prove the hold checker can fire.
  assign live_tuple_c = {1'b1, renderer_term_slot_i,
                         renderer_term_generation_i,
                         renderer_term_publish_i && !renderer_term_fault_i,
                         renderer_term_fault_i};
  assign visible_tuple_c = `ZHAO_VIDEO_TERM_TUPLE(tuple_q, live_tuple_c);
  assign term_writer_o = `ZHAO_VIDEO_TERM_WRITER(visible_tuple_c[19]);
  assign term_slot_o = visible_tuple_c[18];
  assign term_generation_o = visible_tuple_c[17:2];
  assign term_publish_o = visible_tuple_c[1];
  assign term_fault_o = visible_tuple_c[0];

  assign occupied_o = occupied_q;
  assign idle_o = !occupied_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      occupied_q <= 1'b0;
      tuple_q <= '0;
      blit_events_refused_o <= 32'd0;
      source_events_captured_o <= 32'd0;
      manager_terms_accepted_o <= 32'd0;
    end else begin
      if (blit_refused_delta_c != 2'd0)
        blit_events_refused_o <= sat_add_refused(
            blit_events_refused_o, blit_refused_delta_c);

      if (pop_c)
        manager_terms_accepted_o <= sat_inc32(manager_terms_accepted_o);
      if (push_c)
        source_events_captured_o <= sat_inc32(source_events_captured_o);

      unique case ({push_c, pop_c})
        2'b01: begin
          occupied_q <= 1'b0;
          tuple_q <= '0;
        end
        2'b10, 2'b11: begin
          occupied_q <= 1'b1;
          tuple_q <= push_tuple_c;
        end
        default: begin
          occupied_q <= occupied_q;
          tuple_q <= tuple_q;
        end
      endcase
    end
  end

`ifndef SYNTHESIS
`ifndef QUARTUS_SYNTHESIS
  logic sim_stalled_q;
  logic [TUPLE_W-1:0] sim_stalled_tuple_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      sim_stalled_q <= 1'b0;
      sim_stalled_tuple_q <= '0;
    end else begin
      if (sim_stalled_q) begin
        assert (term_valid_o
                && ({term_writer_o, term_slot_o, term_generation_o,
                     term_publish_o, term_fault_o} == sim_stalled_tuple_q))
          else $error("video_terminal_adapter_v2: held terminal changed/dropped");
      end
      assert (!(renderer_term_ready_o && !select_renderer_c))
        else $error("video_terminal_adapter_v2: renderer accepted without selection");
`ifndef ZHAO_VIDEO_TERM_MUTANT_DISABLE_RELEASE_ASSERT
      if (blit_same_key_c && select_blit_c)
        assert (!push_tuple_c[1])
          else $error("video_terminal_adapter_v2: release lost to publication");
`endif
      assert (manager_terms_accepted_o <= source_events_captured_o)
        else $error("video_terminal_adapter_v2: manager accepted uncaptured term");

      sim_stalled_q <= term_valid_o && !term_ready_i;
      if (term_valid_o && !term_ready_i)
        sim_stalled_tuple_q <= {term_writer_o, term_slot_o,
                                term_generation_o, term_publish_o, term_fault_o};
    end
  end
`endif
`endif

endmodule : zhao_video_terminal_adapter_v2

`undef ZHAO_VIDEO_TERM_VALID
`undef ZHAO_VIDEO_TERM_TUPLE
`undef ZHAO_VIDEO_TERM_WRITER
`undef ZHAO_VIDEO_TERM_RELEASE_CHOICE
`undef ZHAO_VIDEO_TERM_RENDER_READY
`undef ZHAO_VIDEO_TERM_REFUSE_DELTA
`undef ZHAO_VIDEO_TERM_ROOM
`ifdef ZHAO_VIDEO_TERM_MUTANT_DISABLE_RELEASE_ASSERT
  `undef ZHAO_VIDEO_TERM_MUTANT_DISABLE_RELEASE_ASSERT
`endif
`ifdef ZHAO_VIDEO_TERM_MUTANT_COLLISION
  `undef ZHAO_VIDEO_TERM_MUTANT_COLLISION
`endif
`default_nettype wire
