// zhao_shell_v2_lease_path.sv -- Packet H's lease path, composed and driven.
//
// NOT PRODUCTION RTL, and not the sibling shell. This is the harness
// reports/PACKET-H-CHANNEL-MAP-20260917.md recommends as the step BEFORE
// `zhao_shell_top_v2.sv`: compose the three blocks that carry a renderer frame
// from request to published-ready, and drive one legal transaction through
// them. A few hundred lines that fail loudly if a channel is wired wrong, so
// the 2,000-line shell composes a protocol already shown to work instead of
// debugging the protocol and the composition at once.
//
// WHAT IT COMPOSES, and what it deliberately leaves out:
//
//   zhao_video_slotmgr_v2        the manager -- slots, writer-tagged leases,
//                                one terminal channel, ready publication
//   zhao_renderer_lease_v2       the renderer's client -- one held request in,
//                                one held admitted frame out, and the sole
//                                driver of V3's frame-clear handshake
//   zhao_video_terminal_adapter_v2   funnels the renderer's terminal return
//                                (and the blit's publish/release) into the
//                                manager's single term_* channel
//
// `zhao_video_ready_bridge_v2` is NOT here: it is a two-clock CDC block, and
// including it would test the CDC and the protocol at once. The signals it
// would produce -- `lease_open`, and the swap echo -- are driven directly by
// the testbench instead, which is the same thing the shell will see. It gets
// its own harness.
//
// `zhao_engine1_raw_last_v2` is not here either, and that is not an omission:
// the channel map establishes it is not on this path at all.
//
// THE FIVE FACTS THIS HARNESS EXISTS TO CATCH, from the channel map:
//
//   1. `lease_open` flows from the CDC bridge INTO the lease, gating creation
//      of new work and never retirement. Driven here as an input.
//   2. `rsp_*` is SHARED and writer-tagged; the lease raises ready only for
//      writer 1. Only the renderer requests here, so every response is writer
//      1 -- and `a_rsp_writer` below asserts it, because a harness that cannot
//      see the tag would pass with the tag wired wrong.
//   3. The renderer never publishes directly; its terminal return goes through
//      the adapter.
//   4. Frame geometry is mode-derived inside the lease and must not be
//      recomputed. Exported so the testbench can check it moves with the mode.
//   5. engine1_raw_last is off this path.

module zhao_shell_v2_lease_path (
    input  logic        clk,
    input  logic        rst_n,

    // The Packet-H reset-epoch barrier, normally from the CDC bridge.
    input  logic        lease_open_i,

    // The renderer's frame request.
    input  logic        frame_req_valid_i,
    output logic        frame_req_ready_o,
    input  logic [1:0]  frame_req_mode_i,

    // V3's recoverable frame-clear handshake, normally to the island.
    output logic        frame_fault_clear_valid_o,
    input  logic        frame_fault_clear_ready_i,

    // The admitted frame.
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

    // The renderer's terminal return, into the adapter.
    input  logic        term_valid_i,
    output logic        term_ready_o,
    input  logic        term_slot_i,
    input  logic [15:0] term_generation_i,
    input  logic        term_publish_i,
    input  logic        term_fault_i,

    // The manager's ready publication, normally into the CDC bridge.
    output logic        ready_valid_o,
    input  logic        ready_ready_i,
    output logic        ready_writer_o,
    output logic        ready_slot_o,
    output logic [15:0] ready_generation_o,
    output logic [1:0]  ready_mode_o,
    output logic [31:0] ready_base_o,
    output logic [31:0] ready_span_o,

    // The swap echo, normally from the CDC bridge after video accepts.
    input  logic        swap_valid_i,
    output logic        swap_ready_o,
    input  logic        swap_writer_i,
    input  logic        swap_slot_i,
    input  logic [15:0] swap_generation_i,
    input  logic [1:0]  swap_mode_i,
    input  logic [31:0] swap_base_i,
    input  logic [31:0] swap_span_i,

    output logic        displayed_valid_o,
    output logic        displayed_slot_o,
    output logic [15:0] displayed_generation_o,

    // Lifecycle evidence, so the test asserts counts rather than appearances.
    output logic [31:0] requests_accepted_o,
    output logic [31:0] responses_accepted_o,
    output logic [31:0] leases_granted_o,
    output logic [31:0] publications_o,
    output logic [31:0] releases_o,
    output logic [31:0] ready_events_o,
    output logic [31:0] manager_terms_accepted_o,

    // The manager's live lease authority -- in the shell this drives both
    // framebuffer guards, so a test that cannot see it cannot check the
    // thing the lease exists to produce.
    output logic        lease_valid_o,
    output logic        lease_writer_o,
    output logic        lease_slot_o,
    output logic [15:0] lease_generation_o,
    output logic [1:0]  lease_mode_o,
    output logic [31:0] lease_base_o,
    output logic [31:0] lease_span_o,
    output logic        lease_fault_o,

    // Derived frame geometry, exported so the test can confirm fact 4 --
    // that it is produced HERE from the mode and is not recomputed upstream.
    output logic [15:0] frame_view1_y_o,
    output logic [31:0] frame_view1_offset_o,

    // The rest of the manager's and adapter's evidence.
    output logic [1:0]  slot_state0_o,
    output logic [1:0]  slot_state1_o,
    output logic        adapter_idle_o,
    output logic        adapter_occupied_o,
    output logic        blit_refused_o,
    output logic [31:0] blit_events_refused_o,
    output logic [31:0] source_events_captured_o,
    output logic [31:0] leases_refused_o,
    output logic [31:0] faults_latched_o,
    output logic [31:0] swaps_o,
    output logic [31:0] contentions_o,
    output logic [31:0] stale_events_o,
    output logic        displayed_writer_o,
    output logic [1:0]  displayed_mode_o,
    output logic [31:0] displayed_base_o,
    output logic [31:0] displayed_span_o,
    output logic        blit_req_ready_o,
    output logic        fault_ready_o
);

  // ---- lease <-> manager ---------------------------------------------------
  logic        render_req_valid, render_req_ready, render_req_slot;
  logic [1:0]  render_req_mode;

  logic        rsp_valid, rsp_ready, rsp_writer, rsp_granted, rsp_slot;
  logic [15:0] rsp_generation;
  logic [1:0]  rsp_mode;
  logic [31:0] rsp_base, rsp_span;

  logic [1:0]  slot_state [0:1];
  assign slot_state0_o = slot_state[0];
  assign slot_state1_o = slot_state[1];

  // ---- adapter -> manager --------------------------------------------------
  logic        mterm_valid, mterm_ready, mterm_writer, mterm_slot;
  logic [15:0] mterm_generation;
  logic        mterm_publish, mterm_fault;



  zhao_renderer_lease_v2 u_lease (
      .clk                      (clk),
      .rst_n                    (rst_n),
      .lease_open_i             (lease_open_i),
      .frame_req_valid_i        (frame_req_valid_i),
      .frame_req_ready_o        (frame_req_ready_o),
      .frame_req_mode_i         (frame_req_mode_i),
      .lease_valid_i            (lease_valid_o),
      .slot_state_i             (slot_state),
      .render_req_valid_o       (render_req_valid),
      .render_req_ready_i       (render_req_ready),
      .render_req_slot_o        (render_req_slot),
      .render_req_mode_o        (render_req_mode),
      .rsp_valid_i              (rsp_valid),
      .rsp_ready_o              (rsp_ready),
      .rsp_writer_i             (rsp_writer),
      .rsp_granted_i            (rsp_granted),
      .rsp_slot_i               (rsp_slot),
      .rsp_generation_i         (rsp_generation),
      .rsp_mode_i               (rsp_mode),
      .rsp_base_i               (rsp_base),
      .rsp_span_i               (rsp_span),
      .frame_fault_clear_valid_o(frame_fault_clear_valid_o),
      .frame_fault_clear_ready_i(frame_fault_clear_ready_i),
      .frame_valid_o            (frame_valid_o),
      .frame_ready_i            (frame_ready_i),
      .frame_writer_o           (frame_writer_o),
      .frame_slot_o             (frame_slot_o),
      .frame_generation_o       (frame_generation_o),
      .frame_mode_o             (frame_mode_o),
      .frame_base_o             (frame_base_o),
      .frame_span_o             (frame_span_o),
      .frame_width_o            (frame_width_o),
      .frame_height_o           (frame_height_o),
      .frame_stride_o           (frame_stride_o),
      .frame_view1_y_o          (frame_view1_y_o),
      .frame_view1_offset_o     (frame_view1_offset_o)
  );

  zhao_video_terminal_adapter_v2 u_adapter (
      .clk                      (clk),
      .rst_n                    (rst_n),
      // The blit side is idle in this harness; the channel map says the
      // renderer's return shares this adapter with it, and that sharing is
      // what the shell will exercise.
      .blit_publish_valid_i     (1'b0),
      .blit_publish_slot_i      (1'b0),
      .blit_publish_generation_i(16'd0),
      .blit_release_valid_i     (1'b0),
      .blit_release_slot_i      (1'b0),
      .blit_release_generation_i(16'd0),
      .blit_refused_o           (blit_refused_o),
      .blit_events_refused_o    (blit_events_refused_o),
      .renderer_term_valid_i    (term_valid_i),
      .renderer_term_ready_o    (term_ready_o),
      .renderer_term_slot_i     (term_slot_i),
      .renderer_term_generation_i(term_generation_i),
      .renderer_term_publish_i  (term_publish_i),
      .renderer_term_fault_i    (term_fault_i),
      .term_valid_o             (mterm_valid),
      .term_ready_i             (mterm_ready),
      .term_writer_o            (mterm_writer),
      .term_slot_o              (mterm_slot),
      .term_generation_o        (mterm_generation),
      .term_publish_o           (mterm_publish),
      .term_fault_o             (mterm_fault),
      .occupied_o               (adapter_occupied_o),
      .idle_o                   (adapter_idle_o),
      .source_events_captured_o (source_events_captured_o),
      .manager_terms_accepted_o (manager_terms_accepted_o)
  );

  zhao_video_slotmgr_v2 u_mgr (
      .clk                   (clk),
      .rst_n                 (rst_n),
      .render_req_valid_i    (render_req_valid),
      .render_req_ready_o    (render_req_ready),
      .render_req_slot_i     (render_req_slot),
      .render_req_mode_i     (render_req_mode),
      .blit_req_valid_i      (1'b0),
      .blit_req_ready_o      (blit_req_ready_o),
      .blit_req_slot_i       (1'b0),
      .blit_req_mode_i       (2'd0),
      .rsp_valid_o           (rsp_valid),
      .rsp_ready_i           (rsp_ready),
      .rsp_writer_o          (rsp_writer),
      .rsp_granted_o         (rsp_granted),
      .rsp_slot_o            (rsp_slot),
      .rsp_generation_o      (rsp_generation),
      .rsp_mode_o            (rsp_mode),
      .rsp_base_o            (rsp_base),
      .rsp_span_o            (rsp_span),
      .lease_valid_o         (lease_valid_o),
      .lease_writer_o        (lease_writer_o),
      .lease_slot_o          (lease_slot_o),
      .lease_generation_o    (lease_generation_o),
      .lease_mode_o          (lease_mode_o),
      .lease_base_o          (lease_base_o),
      .lease_span_o          (lease_span_o),
      .lease_fault_o         (lease_fault_o),
      .fault_valid_i         (1'b0),
      .fault_ready_o         (fault_ready_o),
      .fault_writer_i        (1'b0),
      .fault_slot_i          (1'b0),
      .fault_generation_i    (16'd0),
      .term_valid_i          (mterm_valid),
      .term_ready_o          (mterm_ready),
      .term_writer_i         (mterm_writer),
      .term_slot_i           (mterm_slot),
      .term_generation_i     (mterm_generation),
      .term_publish_i        (mterm_publish),
      .term_fault_i          (mterm_fault),
      .ready_valid_o         (ready_valid_o),
      .ready_ready_i         (ready_ready_i),
      .ready_writer_o        (ready_writer_o),
      .ready_slot_o          (ready_slot_o),
      .ready_generation_o    (ready_generation_o),
      .ready_mode_o          (ready_mode_o),
      .ready_base_o          (ready_base_o),
      .ready_span_o          (ready_span_o),
      .swap_valid_i          (swap_valid_i),
      .swap_ready_o          (swap_ready_o),
      .swap_writer_i         (swap_writer_i),
      .swap_slot_i           (swap_slot_i),
      .swap_generation_i     (swap_generation_i),
      .swap_mode_i           (swap_mode_i),
      .swap_base_i           (swap_base_i),
      .swap_span_i           (swap_span_i),
      .displayed_valid_o     (displayed_valid_o),
      .displayed_writer_o    (displayed_writer_o),
      .displayed_slot_o      (displayed_slot_o),
      .displayed_generation_o(displayed_generation_o),
      .displayed_mode_o      (displayed_mode_o),
      .displayed_base_o      (displayed_base_o),
      .displayed_span_o      (displayed_span_o),
      .slot_state_o          (slot_state),
      .requests_accepted_o   (requests_accepted_o),
      .responses_accepted_o  (responses_accepted_o),
      .leases_granted_o      (leases_granted_o),
      .leases_refused_o      (leases_refused_o),
      .faults_latched_o      (faults_latched_o),
      .publications_o        (publications_o),
      .releases_o            (releases_o),
      .ready_events_o        (ready_events_o),
      .swaps_o               (swaps_o),
      .contentions_o         (contentions_o),
      .stale_events_o        (stale_events_o)
  );

`ifndef SYNTHESIS
  // FACT 2 FROM THE CHANNEL MAP, asserted rather than assumed. Only the
  // renderer requests here, so every response the manager emits must be tagged
  // writer 1. If the tag were wired backwards the lease would simply never
  // raise `rsp_ready_o`, the transaction would stall, and the test would report
  // a timeout -- which reads as "the protocol is wrong" rather than "one wire
  // is". This says which.
  always_ff @(posedge clk) begin
    // No rst_n term -- reset clears rsp_valid, and reading rst_n
    // synchronously beside an asynchronous design is SYNCASYNCNET. Third time
    // today; it is a reflex worth having.
    if (rsp_valid && (rsp_writer !== 1'b1))
      $fatal(1, "shell_v2_lease_path: manager tagged a response writer %0d with only the renderer requesting", rsp_writer);
  end
`endif

endmodule
