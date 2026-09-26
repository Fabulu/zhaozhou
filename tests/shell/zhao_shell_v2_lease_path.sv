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

module zhao_shell_v2_lease_path
  import zhao_pkg::*, zhao_fb_tuple_pkg::*;
(
    input  logic        clk,
    input  logic        rst_n,

    // The Packet-H reset-epoch barrier, normally from the CDC bridge.
    // ---- THE VIDEO DOMAIN AND THE BARRIER -------------------------------
    //
    // `lease_open` is NO LONGER A TEST INPUT. It comes from
    // `zhao_video_ready_bridge_v2`, which opens the gate once per reset epoch
    // only after both CDC barriers and a synchronized blank acknowledgement.
    // Holding it high from the test asserted the barrier clause instead of
    // exercising it.
    input  logic        vid_clk,
    input  logic        vid_rst_n,
    // THE BARRIERS ARE NOT TEST INPUTS EITHER. `zhao_fb_ready_cdc_v2`
    // produces `gpu_barrier_done_o` and `vid_barrier_done_o` from its own
    // reset-release chains, and the bridge consumes them. The whole
    // reset-epoch barrier is therefore self-driven and the test only
    // releases the two resets.
    input  logic        blank_cmd_i,
    input  logic        scanout_ack_i,

    // The FRAMECTL side of the echo: video says "I swapped to this slot" and
    // the bridge turns the tuple it holds into the reverse echo.
    input  logic        frame_swap_valid_i,
    input  logic        frame_swap_slot_i,

    output logic        lease_open_o,
    output logic        blank_ack_o,
    output logic        blank_active_o,
    output logic        bridge_pending_o,
    output logic [1:0]  frame_slot_ready_o,
    output logic        gpu_reset_released_o,
    output logic        vid_reset_released_o,
    output logic        gpu_barrier_done_o,
    output logic        vid_barrier_done_o,
    output logic        cdc_gpu_protocol_fault_o,
    output logic        cdc_vid_protocol_fault_o,
    output logic [31:0] ready_enqueued_o,
    output logic [31:0] ready_dequeued_o,
    output logic [31:0] swap_enqueued_o,
    output logic [31:0] swap_dequeued_o,

    // The renderer's frame request.
    input  logic        frame_req_valid_i,
    output logic        frame_req_ready_o,
    input  logic [1:0]  frame_req_mode_i,

    // V3's recoverable frame-clear handshake, normally to the island.
    // THE CLEAR HANDSHAKE IS NO LONGER DRIVEN BY THE TEST. Its ready now
    // comes from `zhao_geom_bin_pipe_v2`, which accepts a clear only once
    // the binner and tile path are quiet -- `binner_initialized_o &&
    // !frame_inflight_q && !frame_begin_i`. That gating IS the Packet-H
    // gate's old-work-drain ordering clause, and with a test driving ready
    // high it was being asserted rather than exercised.
    output logic        frame_fault_clear_valid_o,

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
    output logic        ready_writer_o,
    output logic        ready_slot_o,
    output logic [15:0] ready_generation_o,
    output logic [1:0]  ready_mode_o,
    output logic [31:0] ready_base_o,
    output logic [31:0] ready_span_o,

    // THE SWAP ECHO IS NO LONGER SIX TEST INPUTS. It arrives as the bridge's
    // 84-bit reverse tuple and is unpacked through zhao_fb_tuple_pkg -- which
    // is the entire reason that package exists, and the path on which a
    // reversed layout shows a plausible WRONG slot while every count
    // balances.
    output logic        swap_ready_o,

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
    output logic        fault_ready_o,

    // ---- THE BLIT SIDE, and why it is a REAL LEAF now ------------------
    //
    // `rsp_*` is ONE channel shared by both writers, and each lease raises
    // ready only for its own tag. All of that is invisible while only the
    // renderer requests -- which is the state the first version of this
    // harness tested. The shared channel is the single most likely thing to
    // deadlock in the real shell: if nobody accepts a writer-0 response, the
    // manager holds it and the RENDERER stops too, and the symptom is a
    // stalled renderer with nothing wrong in the renderer.
    //
    // The second version drove the manager's blit requester by hand. This one
    // instantiates `zhao_video_blit_lease_v2`, so both contenders are the
    // leaves the shell will actually contain and the channel is exercised by
    // the real protocol on both sides rather than by a stub that agrees with
    // it by construction.
    //
    // `zhao_debug_frameblit` is deliberately NOT here. It is a 38 KB retained
    // V1 block that moves bytes through guards and an arbiter; composing it
    // would test the blitter and the protocol at once, and the protocol is
    // the question. Its two seams are presented at this boundary instead: the
    // request handshake it would complete, and the lease record it latches.
    input  logic        blit_dispatch_valid_i,
    output logic        blit_dispatch_ready_o,
    input  logic        blit_dispatch_slot_i,
    input  logic [1:0]  blit_dispatch_mode_i,

    // The retained blitter's seams, stubbed at the boundary. `fb_req_*` is
    // the leaf's request to the BLITTER; `blit_req_ready_o` above is the
    // MANAGER's ready on the lease request channel. Two different channels,
    // deliberately not two similar names.
    input  logic        fb_req_ready_i,
    input  logic        blit_done_i,
    output logic        fb_req_valid_o,
    output logic        fb_lease_valid_o,
    output logic        fb_lease_slot_o,
    output logic [15:0] fb_lease_generation_o,

    // ---- THE V3 BIN PIPE -----------------------------------------------
    //
    // Composed for ONE reason: to own the frame-clear handshake. Everything
    // else about it is held quiescent and said so at the instantiation --
    // this harness is about the protocol, not about rasterisation, and a
    // triangle stream would test the binner and the ordering at once.
    // ---- THE V3 PROGRAMMING CHANNEL --------------------------------------
    //
    // Twenty inputs, exposed at the boundary rather than decoded from the
    // command stream. That was decided in
    // reports/PACKET-H-DRIVER-CONTRACT-20260917.md section 3.1: the
    // historical shell's entire configuration surface is already top-level
    // and harness-driven, a decoder is a second design with its own ABI and
    // tests that the Packet-H gate does not ask for, and a decoder inserted
    // later sits BEHIND these same ports and changes nothing the V2 blocks
    // see.
    //
    // NOTHING IN THE TREE DRIVES THESE FOR REAL. Every other instantiation
    // is a harness: zhao_prod_top feeds them from generated stimulus slices
    // and the V3 fit top fabricates a sequence so the fitter has something
    // to measure. This is the first composition that runs a legal
    // programming sequence into the block that will carry it.
    input  logic        cfg_valid_i,
    output logic        cfg_ready_o,
    input  logic [1:0]  cfg_op_i,
    input  logic [7:0]  cfg_page_generation_i,
    input  logic [7:0]  cfg_selector_i,
    input  logic [74:0] cfg_row_i,
    input  logic [31:0] cfg_crc32_i,
    output logic        cfg_rsp_valid_o,
    input  logic        cfg_rsp_ready_i,
    output logic [1:0]  cfg_rsp_op_o,
    output logic [3:0]  cfg_rsp_status_o,
    output logic [7:0]  cfg_rsp_page_generation_o,
    output logic [7:0]  active_page_generation_o,

    input  logic        pal_load_valid_i,
    output logic        pal_load_ready_o,
    input  logic [1:0]  pal_load_op_i,
    input  logic [1:0]  pal_load_slot_i,
    input  logic [7:0]  pal_load_gen_i,
    input  logic [7:0]  pal_load_idx_i,
    input  logic [15:0] pal_load_rgb565_i,
    input  logic        pal_load_crc_ok_i,

    // ---- THE LAST TWELVE -------------------------------------------------
    //
    // Packet-D attribute carriage, the Packet-E ENGINE1 share, the V3 clear
    // payload and Surface Sheet backpressure. Exposed rather than tied,
    // because the gate clause is "every new port is CONNECTED" and a literal
    // in a port map is indistinguishable from a decision nobody made.
    //
    // Driving them meaningfully needs triangle traffic, which this harness
    // deliberately does not generate -- Packet D has its own directed test
    // for that and running one here would test the binner and the protocol
    // at once. What this closes is the WIRING, and `packet_h_tieoff_audit.py`
    // is what stops the difference between "wired" and "tied" being a matter
    // of opinion.
    input  logic [46:0]  tri_area2_i,
    input  logic [239:0] tri_invw_plane_i,
    input  logic [239:0] tri_u_over_w_plane_i,
    input  logic [239:0] tri_v_over_w_plane_i,
    input  logic [239:0] tri_r_plane_i,
    input  logic [239:0] tri_g_plane_i,
    input  logic [239:0] tri_b_plane_i,
    input  logic [297:0] tri_flat_request_i,
    input  logic [47:0]  tri_continuation_tail_i,
    input  logic [31:0]  tri_fragment_state_i,

    input  logic         fill_req_ready_i,
    output logic         fill_req_valid_o,
    output logic [31:0]  fill_req_addr_o,
    input  logic         fill_data_valid_i,
    input  logic [15:0]  fill_data_i,
    input  logic         fill_refused_i,

    input  logic [63:0]  frame_clear_word_i,
    input  logic         sheet_req_ready_i,
    output logic         sheet_req_valid_o,
    output logic [1:0]   sheet_req_op_o,
    output logic [31:0]  sheet_req_handle_o,
    output logic [11:0]  sheet_req_texel_o,
    output logic [15:0]  sheet_req_src_id_o,

    input  logic        bin_frame_end_i,
    input  logic [5:0]  bin_grid_w_i,
    input  logic [5:0]  bin_grid_h_i,
    output logic        bin_quiet_o,
    output logic        bin_initialized_o,
    output logic        bin_drain_busy_o,
    output logic        bin_drain_done_o,
    output logic        bin_frame_fault_o,
    output logic        bin_lifetime_fault_o,
    output logic        bin_frame_begin_o,
    output logic        clear_valid_o,
    output logic        clear_ready_o,
    // COUNTED IN RTL, not sampled by the test. The gate says each newly
    // accepted renderer lease produces EXACTLY ONE clear handshake before the
    // frame is admitted, and "exactly one" is not a thing a driver that ticks
    // in bursts can honestly claim -- it sees the cycles it happens to look
    // at. This repository has a chapter on a machine doing the work twice
    // while producing identical output; a counter is what saw it.
    output logic [31:0] clear_handshakes_o,
    output logic [31:0] frames_admitted_o,

    // ---- STRUCTURAL FAULT ENTRY ----------------------------------------
    //
    // The gate requires five distinct faults -- RCP qerr, expander wq
    // overflow, consumed UV mismatch, invalid authoritative owner-mask
    // identity, metajoin sidx3 -- to bypass normal quiet/clear, RELEASE the
    // lease and produce no READY and no publication.
    //
    // NONE OF THEM IS REACHABLE FROM A QUIESCENT BIN PIPE, and a clause that
    // cannot be reached is not evidence about the clause. So the shell's
    // real aggregation is wired -- the bin pipe's structural outputs do
    // drive the manager's fault port -- and an INJECTION port is OR'd beside
    // it so the manager's response can be exercised now. The injection is
    // the test's; the aggregation is the shell's, and it is the part that
    // has to be right when real traffic arrives.
    input  logic        fault_inject_valid_i,
    input  logic        fault_inject_writer_i,
    input  logic        fault_inject_slot_i,
    input  logic [15:0] fault_inject_generation_i,
    output logic        bin_raster_abort_o,
  output logic        bin_attr_abort_o,
    output logic        bin_sequence_mismatch_o,
    output logic        bin_sequence_abort_o,
    output logic        shell_fault_valid_o,
    output logic [31:0] fault_pulses_o,

    output logic        blit_idle_o,
    output logic [31:0] blit_leases_acquired_o,
    output logic [31:0] blit_leases_refused_o,
    output logic [31:0] blits_dispatched_o,
    output logic        rsp_writer_o        // the live tag, so a test can see it
);

  // ---- lease <-> manager ---------------------------------------------------
  logic        render_req_valid, render_req_ready, render_req_slot;
  logic [1:0]  render_req_mode;

  logic        rsp_valid, rsp_ready, rsp_writer, rsp_granted, rsp_slot;
  logic        lease_rsp_ready;

  // ---- THE VIDEO BRIDGE AND THE 84-BIT TUPLE -----------------------------
  logic        lease_open_w;
  logic [83:0] cdc_ready_tuple_w;      // gpu side, packed here
  // A declared quiet pixel stream rather than a cast: the type is a struct
  // in zhao_pkg and Verilator will not cast an expression to it.
  zhao_px_stream_t px_quiet_w;
  assign px_quiet_w = '0;
  logic        cdc_ready_ready_w;      // gpu side, from the CDC

  // THE FORWARD AND REVERSE FIFOS SIT BETWEEN THE MANAGER AND THE BRIDGE,
  // and getting that wrong is what this composition caught. The bridge's
  // `cdc_ready_*` port is the VIDEO-domain OUTPUT of the forward FIFO -- its
  // own comment says so -- not a GPU-domain source. Wiring the manager
  // straight to it sampled a GPU level with a video flop across an
  // unsynchronised boundary. The symptom was `ready_events_o` reading 1
  // while the bridge's `pending_q` stayed 0: a CDC violation presenting as
  // "the screen never updates".
  logic        vid_ready_valid_w, vid_ready_ready_w;
  logic [83:0] vid_ready_tuple_w;
  logic        vid_swap_valid_w, vid_swap_ready_w;
  logic [83:0] vid_swap_tuple_w;
  logic        gpu_swap_valid_w;
  logic [83:0] gpu_swap_tuple_w;

  // THE PACK SIDE. The manager presents READY as six fields; the CDC beneath
  // the bridge carries 84 opaque bits. This is one of exactly two places the
  // layout is applied, and the other is the unpack at the manager's swap
  // port a few lines below -- both through the package, so they cannot
  // disagree.
  assign cdc_ready_tuple_w = zhao_fb_tuple_pack(
      ready_writer_o, ready_slot_o, ready_generation_o, ready_mode_o,
      ready_base_o, ready_span_o);
  assign lease_open_o = lease_open_w;
  logic        lease_clear_valid, lease_clear_ready;

  // The admitted frame becomes the bin pipe's frame_begin PULSE. This is the
  // shell's law in miniature: the renderer's work is withheld until the
  // lease has been granted AND the clear accepted, so `frame_begin_i` can
  // only ever be the fire of an admitted frame.
  logic        bin_frame_begin_w;
  assign bin_frame_begin_w = frame_valid_o && frame_ready_i;
  assign bin_frame_begin_o = bin_frame_begin_w;
  assign clear_valid_o     = lease_clear_valid;
  assign clear_ready_o     = lease_clear_ready;

  // THE SHELL'S FAULT ATTRIBUTION, and it is a decision rather than a wire.
  // A structural fault is reported by the bin pipe with no identity of its
  // own -- it is a property of the machine, not of a frame. The lease it
  // belongs to is therefore whichever one is LIVE, and the manager needs
  // that identity to match its lease record before it will latch anything.
  // Getting this wrong is silent: an unmatched fault is simply ignored, the
  // publication proceeds, and the frame that was ruined is shown.
  logic        bin_fault_w;
  logic        mgr_fault_valid, mgr_fault_writer, mgr_fault_slot;
  logic [15:0] mgr_fault_generation;

  // THE SIX TERMS, AND THEY MUST BE THE SHELL'S SIX. This OR is a copy of
  // `zhao_shell_top_v2.sv`'s `v2_fault_level_c`, and a copy drifts -- which is
  // the one failure mode this repository has written the most words about. If
  // the two lists ever disagree, the harness is measuring a machine that does
  // not ship, and it will go on passing while it does so.
  // `packet_h_fault_or_parity` compares them and is the reason that cannot
  // happen quietly.
  //
  // `attr_abort` and `cdc_gpu_protocol_fault` joined both lists together,
  // found by the tie-off audit once it stopped skipping empty connections.
  assign bin_fault_w = bin_frame_fault_o || bin_lifetime_fault_o ||
                       bin_raster_abort_o || bin_attr_abort_o ||
                       bin_sequence_mismatch_o || cdc_gpu_protocol_fault_o;

  // AND IT MUST BE A PULSE. `zhao_video_slotmgr_v2` increments
  // `faults_latched_o` on EVERY cycle a matching fault is asserted -- there
  // is no "already faulted" guard on the counter, and `fault_ready_o` is
  // simply `rst_n`, so the port is a level input with no handshake. The bin
  // pipe's structural outputs are LEVELS that stay high until reset. Wiring
  // one straight through turns a single structural fault into a fault count
  // of however many cycles the machine sat in it.
  //
  // That is not a defect in the manager; it is an unstated obligation on
  // whoever drives the port, and this is the first thing that ever drove it.
  // Measured before it was fixed: a six-cycle injection read SIX faults.
  logic fault_level_q;
  logic fault_level_c;
  assign fault_level_c = fault_inject_valid_i || bin_fault_w;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) fault_level_q <= 1'b0;
    else fault_level_q <= fault_level_c;
  end

  assign mgr_fault_valid = fault_level_c && !fault_level_q;
  assign mgr_fault_writer = fault_inject_valid_i ? fault_inject_writer_i
                                                 : lease_writer_o;
  assign mgr_fault_slot = fault_inject_valid_i ? fault_inject_slot_i
                                               : lease_slot_o;
  assign mgr_fault_generation = fault_inject_valid_i ? fault_inject_generation_i
                                                     : lease_generation_o;

  // Exposed so the test can assert the pulse is one cycle wide rather than
  // inferring it from a count that happens to read 1.
  logic [31:0] fault_pulses_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) fault_pulses_q <= 32'd0;
    else if (mgr_fault_valid) fault_pulses_q <= fault_pulses_q + 32'd1;
  end
  assign fault_pulses_o = fault_pulses_q;
  assign shell_fault_valid_o = mgr_fault_valid;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      clear_handshakes_o <= 32'd0;
      frames_admitted_o  <= 32'd0;
    end else begin
      if (lease_clear_valid && lease_clear_ready)
        clear_handshakes_o <= clear_handshakes_o + 32'd1;
      if (bin_frame_begin_w) frames_admitted_o <= frames_admitted_o + 32'd1;
    end
  end
  assign frame_fault_clear_valid_o = lease_clear_valid;
  logic        blit_rsp_ready;
  logic        blit_mgr_req_valid, blit_mgr_req_slot;
  logic [1:0]  blit_mgr_req_mode;

  // THE SHARED RESPONSE, DEMULTIPLEXED BY TAG. The manager sees one
  // `rsp_ready` and each leaf accepts only its own tag.
  //
  // WITH BOTH LEAVES REAL, THIS MUX AND A PLAIN OR ARE EQUIVALENT, and saying
  // so is the point. Each leaf already qualifies its own ready by the writer
  // tag -- `zhao_renderer_lease_v2` at line 209, `zhao_video_blit_lease_v2` at
  // its own `rsp_ready_o` -- and each asserts the invariant internally, so
  // neither can be high for the other's response and an OR could not
  // mis-retire anything. An earlier version of this file claimed the mux was
  // what prevented that; miswiring it as an OR fired at the claim and nothing
  // noticed, because the protection was always one level down.
  //
  // It stays a mux anyway: it costs nothing, it states the intent at the
  // junction where a reader looks for it, and it is the one form that stays
  // correct if a participant ever arrives without its own guard.
  assign rsp_ready    = rsp_writer ? lease_rsp_ready : blit_rsp_ready;
  assign rsp_writer_o = rsp_writer;
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



  // THE BIN PIPE, quiescent except for the clear handshake. Every one of its
// 165 ports is named: an input nobody decided about is a tie-off that looks
// deliberate, and an omitted port map entry and an intentional one are
// indistinguishable afterwards. Quiescent means ZERO for data and ONE for a
// downstream ready -- a ready held low is a stall, and a stall here would
// look exactly like the ordering law under test.
//
// 165 AND NOT 168. The last three ports sit behind
// `ifdef ZHAO_PACKET_D_TEST_HOOKS, so a default build does not have them and
// naming them here is a PINNOTFOUND elaboration error. That error is how the
// guard was found -- after a port-counting script had already "corrected"
// 165 to 168 and asserted 168 in its own self-check.
//
// The unread outputs are written `.name()` rather than omitted, and
// PINCONNECTEMPTY is waived for exactly that span. The warning is right to
// notice them; what it pushes towards -- leaving them out -- makes a
// deliberate decision and an oversight look identical.
  /* verilator lint_off PINCONNECTEMPTY */
  zhao_geom_bin_pipe_v2 u_bin (
      .ser_req_i                    (1'b0),  // TIE: no triangle traffic here, so there is no reference list to serialise; entry I54's pass is exercised by geom_chunkser_composed
      .ser_ready_i                  (1'b0),  // TIE: as above
      .ser_busy_o                   (),
      .ser_done_o                   (),
      .ser_valid_o                  (),
      .ser_tri_id_o                 (),
      .ser_tile_o                   (),
      .ser_first_o                  (),
      .ser_last_o                   (),
      .clk                          (clk),
      .rst_n                        (rst_n),
      .frame_begin_i                (bin_frame_begin_w),
      .frame_end_i                  (bin_frame_end_i),
      .grid_w_i                     (bin_grid_w_i),
      .grid_h_i                     (bin_grid_h_i),
      .frame_clear_word_i           (frame_clear_word_i),
      .tri_valid_i                  (1'b0),  // TIE: no triangle traffic here; Packet D owns the binner, and driving one would test the binner and the protocol at once
      .tri_ready_o                  (),
      .tri_kx0_i                    (23'd0),  // TIE: no triangle traffic here; Packet D owns the binner, and driving one would test the binner and the protocol at once
      .tri_ky0_i                    (23'd0),  // TIE: no triangle traffic here; Packet D owns the binner, and driving one would test the binner and the protocol at once
      .tri_kc0_i                    (48'd0),  // TIE: no triangle traffic here; Packet D owns the binner, and driving one would test the binner and the protocol at once
      .tri_kx1_i                    (23'd0),  // TIE: no triangle traffic here; Packet D owns the binner, and driving one would test the binner and the protocol at once
      .tri_ky1_i                    (23'd0),  // TIE: no triangle traffic here; Packet D owns the binner, and driving one would test the binner and the protocol at once
      .tri_kc1_i                    (48'd0),  // TIE: no triangle traffic here; Packet D owns the binner, and driving one would test the binner and the protocol at once
      .tri_kx2_i                    (23'd0),  // TIE: no triangle traffic here; Packet D owns the binner, and driving one would test the binner and the protocol at once
      .tri_ky2_i                    (23'd0),  // TIE: no triangle traffic here; Packet D owns the binner, and driving one would test the binner and the protocol at once
      .tri_kc2_i                    (48'd0),  // TIE: no triangle traffic here; Packet D owns the binner, and driving one would test the binner and the protocol at once
      .tri_tl_i                     (3'd0),  // TIE: no triangle traffic here; Packet D owns the binner, and driving one would test the binner and the protocol at once
      .tri_ax_i                     (21'd0),  // TIE: no triangle traffic here; Packet D owns the binner, and driving one would test the binner and the protocol at once
      .tri_ay_i                     (21'd0),  // TIE: no triangle traffic here; Packet D owns the binner, and driving one would test the binner and the protocol at once
      .tri_bx_i                     (21'd0),  // TIE: no triangle traffic here; Packet D owns the binner, and driving one would test the binner and the protocol at once
      .tri_by_i                     (21'd0),  // TIE: no triangle traffic here; Packet D owns the binner, and driving one would test the binner and the protocol at once
      .tri_cx_i                     (21'd0),  // TIE: no triangle traffic here; Packet D owns the binner, and driving one would test the binner and the protocol at once
      .tri_cy_i                     (21'd0),  // TIE: no triangle traffic here; Packet D owns the binner, and driving one would test the binner and the protocol at once
      .tri_min_x_i                  (12'd0),  // TIE: no triangle traffic here; Packet D owns the binner, and driving one would test the binner and the protocol at once
      .tri_max_x_i                  (12'd0),  // TIE: no triangle traffic here; Packet D owns the binner, and driving one would test the binner and the protocol at once
      .tri_min_y_i                  (12'd0),  // TIE: no triangle traffic here; Packet D owns the binner, and driving one would test the binner and the protocol at once
      .tri_max_y_i                  (12'd0),  // TIE: no triangle traffic here; Packet D owns the binner, and driving one would test the binner and the protocol at once
      .tri_src_id_i                 (16'd0),  // TIE: no triangle traffic here; Packet D owns the binner, and driving one would test the binner and the protocol at once
      .tri_area2_i                  (tri_area2_i),
      .tri_invw_plane_i             (tri_invw_plane_i),
      .tri_u_over_w_plane_i         (tri_u_over_w_plane_i),
      .tri_v_over_w_plane_i         (tri_v_over_w_plane_i),
      .tri_r_plane_i                (tri_r_plane_i),
      .tri_g_plane_i                (tri_g_plane_i),
      .tri_b_plane_i                (tri_b_plane_i),
      .tri_flat_request_i           (tri_flat_request_i),
      .tri_continuation_tail_i      (tri_continuation_tail_i),
      .tri_fragment_state_i         (tri_fragment_state_i),
      .tok_req_o                    (),
      .tok_grant_i                  (1'b1),  // TIE: MEASURE.TOKENS does not gate this path yet -- the V1 shell says the same at its own u_render_bin
      .frame_fault_clear_valid_i    (lease_clear_valid),
      .frame_fault_clear_ready_o    (lease_clear_ready),
      .frame_fault_o                (bin_frame_fault_o),
      .lifetime_structural_fault_o  (bin_lifetime_fault_o),
      .cfg_valid_i                  (cfg_valid_i),
      .cfg_ready_o                  (cfg_ready_o),
      .cfg_op_i                     (cfg_op_i),
      .cfg_page_generation_i        (cfg_page_generation_i),
      .cfg_selector_i               (cfg_selector_i),
      .cfg_row_i                    (cfg_row_i),
      .cfg_crc32_i                  (cfg_crc32_i),
      .cfg_rsp_valid_o              (cfg_rsp_valid_o),
      .cfg_rsp_ready_i              (cfg_rsp_ready_i),
      .cfg_rsp_op_o                 (cfg_rsp_op_o),
      .cfg_rsp_status_o             (cfg_rsp_status_o),
      .cfg_rsp_page_generation_o    (cfg_rsp_page_generation_o),
      .active_page_generation_o     (active_page_generation_o),
      .fill_req_valid_o             (fill_req_valid_o),
      .fill_req_ready_i             (fill_req_ready_i),
      .fill_req_addr_o              (fill_req_addr_o),
      .fill_data_valid_i            (fill_data_valid_i),
      .fill_data_i                  (fill_data_i),
      .fill_refused_i               (fill_refused_i),
      .pal_load_valid_i             (pal_load_valid_i),
      .pal_load_ready_o             (pal_load_ready_o),
      .pal_load_op_i                (pal_load_op_i),
      .pal_load_slot_i              (pal_load_slot_i),
      .pal_load_gen_i               (pal_load_gen_i),
      .pal_load_idx_i               (pal_load_idx_i),
      .pal_load_rgb565_i            (pal_load_rgb565_i),
      .pal_load_crc_ok_i            (pal_load_crc_ok_i),
      .sheet_req_valid_o            (sheet_req_valid_o),
      .sheet_req_ready_i            (sheet_req_ready_i),
      .sheet_req_op_o               (sheet_req_op_o),
      .sheet_req_handle_o           (sheet_req_handle_o),
      .sheet_req_texel_o            (sheet_req_texel_o),
      .sheet_req_src_id_o           (sheet_req_src_id_o),
      .pg_valid_i                   (1'b0),  // TIE: page-generation channel unexercised; its own clause, not this one
      .pg_ready_o                   (),
      .pg_op_i                      (2'd0),  // TIE: page-generation channel unexercised; its own clause, not this one
      .pg_status_i                  (2'd0),  // TIE: page-generation channel unexercised; its own clause, not this one
      .pg_tag_i                     (8'd0),  // TIE: page-generation channel unexercised; its own clause, not this one
      .pg_strength_i                (8'd0),  // TIE: page-generation channel unexercised; its own clause, not this one
      .pg_src_id_i                  (16'd0),  // TIE: page-generation channel unexercised; its own clause, not this one
      .fb_valid_o                   (),
      .fb_ready_i                   (1'b1),  // TIE: no framebuffer here; writes are always accepted
      .fb_rgb565_o                  (),
      .fb_tag_o                     (),
      .fb_addr_o                    (),
      .fb_x_o                       (),
      .fb_y_o                       (),
      .fb_last_o                    (),
      .fb_src_id_o                  (),
      .tile_crc_o                   (),
      .tile_crc_index_o             (),
      .tile_done_o                  (),
      .tile_cov_count_o             (),
      .tile_degenerate_o            (),
      .drain_busy_o                 (bin_drain_busy_o),
      .drain_done_o                 (bin_drain_done_o),
      .binner_initialized_o         (bin_initialized_o),
      .binner_tile_references_o     (),
      .binner_max_tile_list_depth_o (),
      .binner_triangles_culled_o    (),
      // TIE: the shell routes this to its inherited `render_overflow_o` top
      // port, which V1 had and this harness does not -- the harness composes
      // the LEASE path, and arena overflow is the binner's own clause.
      .binner_overflow_o            (),
      .binner_arena_full_o          (),
      .binner_arena_used_o          (),
      .jobs_taken_o                 (),
      .job_stall_clocks_o           (),
      .quiet_o                      (bin_quiet_o),
      .raster_abort_o               (bin_raster_abort_o),
      .local_attribute_abort_o      (bin_attr_abort_o),
      // TIE: the pulse form of the fault COUNT beside it, which is telemetry.
      // The two ABORTS are the structural faults and both are wired.
      .local_fault_pulse_o          (),
      .local_fault_count_o          (),
      .coordinate_fault_count_o     (),
      .range_fault_count_o          (),
      .aux_profile_fault_count_o    (),
      .candidate_cancel_count_o     (),
      .local_drop_count_o           (),
      .raster_jobs_started_o        (),
      .raster_jobs_sunk_o           (),
      .sequence_abort_o             (bin_sequence_abort_o),
      .sequence_mismatch_o          (bin_sequence_mismatch_o),
      .sequence_drop_count_o        (),
      .admission_sequence_o         (),
      .expected_sequence_o          (),
      .returned_sequence_o          (),
      .packet_c_cand_fire_o         (),
      .packet_c_fragment_fire_o     (),
      .packet_c_drop_fire_o         (),
      .tilestore_references_o       (),
      .resolved_tiles_o             (),
      .early_z_rejects_o            (),
      .early_z_covered_o            (),
      .fragment_covered_o           (),
      .blended_fragments_o          (),
      .texture_fragments_o          (),
      .texture_cache_hits_o         (),
      .texture_cache_misses_o       (),
      .texture_palette_lookups_o    (),
      .texture_plan_accepted_o      (),
      .texture_dispatch_accepted_o  (),
      .texture_combine_refused_o    (),
      // Entry I49, 2026-09-20: TMU samples PUBLISHED into a fragment.
      .texture_samples_o            (),
      // TIE: the SHELL routes this to its `render_fragment_error_o` top-level
      // port, which this harness does not have -- it composes the lease path,
      // not the shell's full output surface. It is not in either fault OR on
      // both sides, so the harness still models the attribution faithfully.
      .fragment_error_o             (),
      .coverage_hold_valid_o        (),
      .coverage_delivered_mask_o    (),
      .start_delivered_mask_o       (),
      .attribute_idle_o             (),
      .earlyz_hold_valid_o          (),
      .skid_level_o                 (),
      .stage_candidate_valid_o      (),
      .stage_candidate_data_o       (),
      .stage_fragment_valid_o       (),
      .stage_fragment_addr_o        (),
      .stage_fragment_depth_o       (),
      .stage_fragment_state_o       (),
      .stage_fragment_src_id_o      (),
      .stage_fragment_texel_rgb_o   (),
      .stage_fragment_texel_a_o     (),
      .stage_fragment_texel_idx_o   (),
      .stage_fragment_status_o      (),
      .texture_quiet_o              (),
      .fragment_idle_o              (),
      .front_bank_o                 (),
      .bin_mask_o                   (),
      .z_floor_o                    ()
  );
  /* verilator lint_on PINCONNECTEMPTY */

// THE VIDEO BRIDGE. Two clocks, and it owns the reset-epoch barrier: the
// lease gate opens once per epoch and only after both CDC barriers and a
// synchronized blank acknowledgement. Nothing else in this harness is
// allowed to open it.
// THE TWO ASYNCHRONOUS FIFOS. Depth four each, gray-coded, with their own
// reset-release chains -- which is where `gpu_barrier_done_o` and
// `vid_barrier_done_o` come from, so the reset-epoch barrier is self-driven
// and nothing external declares it complete.
/* verilator lint_off PINCONNECTEMPTY */
zhao_fb_ready_cdc_v2 u_cdc (
    .gpu_clk               (clk),
    .gpu_rst_n             (rst_n),
    .vid_clk               (vid_clk),
    .vid_rst_n             (vid_rst_n),
    .gpu_ready_valid_i     (ready_valid_o),
    .gpu_ready_ready_o     (cdc_ready_ready_w),
    .gpu_ready_tuple_i     (cdc_ready_tuple_w),
    .vid_ready_valid_o     (vid_ready_valid_w),
    .vid_ready_ready_i     (vid_ready_ready_w),
    .vid_ready_tuple_o     (vid_ready_tuple_w),
    .vid_swap_valid_i      (vid_swap_valid_w),
    .vid_swap_ready_o      (vid_swap_ready_w),
    .vid_swap_tuple_i      (vid_swap_tuple_w),
    .gpu_swap_valid_o      (gpu_swap_valid_w),
    .gpu_swap_ready_i      (swap_ready_o),
    .gpu_swap_tuple_o      (gpu_swap_tuple_w),
    .gpu_barrier_done_o    (gpu_barrier_done_o),
    .vid_barrier_done_o    (vid_barrier_done_o),
    .gpu_protocol_fault_o  (cdc_gpu_protocol_fault_o),
    .vid_protocol_fault_o  (cdc_vid_protocol_fault_o),
    .ready_enqueued_o      (ready_enqueued_o),
    .ready_dequeued_o      (ready_dequeued_o),
    .swap_enqueued_o       (swap_enqueued_o),
    .swap_dequeued_o       (swap_dequeued_o),
    .ready_memory_level_o  (),
    .swap_memory_level_o   (),
    .gpu_idle_o            (),
    .vid_idle_o            ()
);
/* verilator lint_on PINCONNECTEMPTY */

/* verilator lint_off PINCONNECTEMPTY */
zhao_video_ready_bridge_v2 u_bridge (
    .gpu_clk               (clk),
    .gpu_rst_n             (rst_n),
    .vid_clk               (vid_clk),
    .vid_rst_n             (vid_rst_n),
    .gpu_barrier_done_i    (gpu_barrier_done_o),
    .vid_barrier_done_i    (vid_barrier_done_o),
    .blank_cmd_i           (blank_cmd_i),
    .blank_ack_o           (blank_ack_o),
    .lease_open_o          (lease_open_w),
    .cdc_ready_valid_i     (vid_ready_valid_w),
    .cdc_ready_ready_o     (vid_ready_ready_w),
    .cdc_ready_tuple_i     (vid_ready_tuple_w),
    .cdc_swap_valid_o      (vid_swap_valid_w),
    .cdc_swap_ready_i      (vid_swap_ready_w),
    .cdc_swap_tuple_o      (vid_swap_tuple_w),
    .frame_slot_ready_o    (frame_slot_ready_o),
    .frame_swap_valid_i    (frame_swap_valid_i),
    .frame_swap_slot_i     (frame_swap_slot_i),
    .scanout_ack_i         (scanout_ack_i),
    // The pixel stream passes THROUGH the bridge: it registers colour and
    // timing metadata together so colour can never become phase-shifted from
    // its sync. This harness does not look at pixels, so the input is quiet
    // and the output is deliberately unread -- named rather than omitted.
    .scanout_px_i          (px_quiet_w),
    .output_px_o           (),
    .gpu_reset_released_o  (gpu_reset_released_o),
    .vid_reset_released_o  (vid_reset_released_o),
    .pending_o             (bridge_pending_o),
    .pending_tuple_o       (),
    .echo_hold_o           (),
    .blank_active_o        (blank_active_o),
    .scanout_wait_o        (),
    .unblank_candidate_o   (),
    .unblank_tuple_o       (),
    .unblank_echo_seen_o   (),
    .unblank_scanout_seen_o()
);
/* verilator lint_on PINCONNECTEMPTY */

// The writer-0 half of the protocol. Its ports face two ways: the manager
// channel it contends on, and the V1 `fb_lease_*` record the retained
// blitter latches on the edge it accepts a request.
zhao_video_blit_lease_v2 u_blit_lease (
    .clk                   (clk),
    .rst_n                 (rst_n),
    .lease_open_i          (lease_open_w),
    .dispatch_valid_i      (blit_dispatch_valid_i),
    .dispatch_ready_o      (blit_dispatch_ready_o),
    .dispatch_slot_i       (blit_dispatch_slot_i),
    .dispatch_mode_i       (blit_dispatch_mode_i),
    .blit_req_valid_o      (fb_req_valid_o),
    .blit_req_ready_i      (fb_req_ready_i),
    .blit_done_i           (blit_done_i),
    .fb_lease_valid_o      (fb_lease_valid_o),
    .fb_lease_slot_o       (fb_lease_slot_o),
    .fb_lease_generation_o (fb_lease_generation_o),
    .mgr_req_valid_o       (blit_mgr_req_valid),
    .mgr_req_ready_i       (blit_req_ready_o),
    .mgr_req_slot_o        (blit_mgr_req_slot),
    .mgr_req_mode_o        (blit_mgr_req_mode),
    .rsp_valid_i           (rsp_valid),
    .rsp_ready_o           (blit_rsp_ready),
    .rsp_writer_i          (rsp_writer),
    .rsp_granted_i         (rsp_granted),
    .rsp_slot_i            (rsp_slot),
    .rsp_generation_i      (rsp_generation),
    .idle_o                (blit_idle_o),
    .leases_acquired_o     (blit_leases_acquired_o),
    .leases_refused_o      (blit_leases_refused_o),
    .blits_dispatched_o    (blits_dispatched_o)
);

zhao_renderer_lease_v2 u_lease (
      .clk                      (clk),
      .rst_n                    (rst_n),
      .lease_open_i             (lease_open_w),
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
      .rsp_ready_o              (lease_rsp_ready),
      .rsp_writer_i             (rsp_writer),
      .rsp_granted_i            (rsp_granted),
      .rsp_slot_i               (rsp_slot),
      .rsp_generation_i         (rsp_generation),
      .rsp_mode_i               (rsp_mode),
      .rsp_base_i               (rsp_base),
      .rsp_span_i               (rsp_span),
      .frame_fault_clear_valid_o(lease_clear_valid),
      .frame_fault_clear_ready_i(lease_clear_ready),
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
      .blit_publish_valid_i     (1'b0),  // TIE: zhao_debug_frameblit is not composed -- 38 KB of retained V1 that moves bytes through guards; its lease side IS composed
      .blit_publish_slot_i      (1'b0),  // TIE: zhao_debug_frameblit is not composed -- 38 KB of retained V1 that moves bytes through guards; its lease side IS composed
      .blit_publish_generation_i(16'd0),  // TIE: zhao_debug_frameblit is not composed -- 38 KB of retained V1 that moves bytes through guards; its lease side IS composed
      .blit_release_valid_i     (1'b0),  // TIE: zhao_debug_frameblit is not composed -- 38 KB of retained V1 that moves bytes through guards; its lease side IS composed
      .blit_release_slot_i      (1'b0),  // TIE: zhao_debug_frameblit is not composed -- 38 KB of retained V1 that moves bytes through guards; its lease side IS composed
      .blit_release_generation_i(16'd0),  // TIE: zhao_debug_frameblit is not composed -- 38 KB of retained V1 that moves bytes through guards; its lease side IS composed
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
      .blit_req_valid_i      (blit_mgr_req_valid),
      .blit_req_ready_o      (blit_req_ready_o),
      .blit_req_slot_i       (blit_mgr_req_slot),
      .blit_req_mode_i       (blit_mgr_req_mode),
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
      .fault_valid_i         (mgr_fault_valid),
      .fault_ready_o         (fault_ready_o),
      .fault_writer_i        (mgr_fault_writer),
      .fault_slot_i          (mgr_fault_slot),
      .fault_generation_i    (mgr_fault_generation),
      .term_valid_i          (mterm_valid),
      .term_ready_o          (mterm_ready),
      .term_writer_i         (mterm_writer),
      .term_slot_i           (mterm_slot),
      .term_generation_i     (mterm_generation),
      .term_publish_i        (mterm_publish),
      .term_fault_i          (mterm_fault),
      .ready_valid_o         (ready_valid_o),
      .ready_ready_i         (cdc_ready_ready_w),
      .ready_writer_o        (ready_writer_o),
      .ready_slot_o          (ready_slot_o),
      .ready_generation_o    (ready_generation_o),
      .ready_mode_o          (ready_mode_o),
      .ready_base_o          (ready_base_o),
      .ready_span_o          (ready_span_o),
      .swap_valid_i          (gpu_swap_valid_w),
      .swap_ready_o          (swap_ready_o),
      .swap_writer_i         (zhao_fb_tuple_writer(gpu_swap_tuple_w)),
      .swap_slot_i           (zhao_fb_tuple_slot(gpu_swap_tuple_w)),
      .swap_generation_i     (zhao_fb_tuple_generation(gpu_swap_tuple_w)),
      .swap_mode_i           (zhao_fb_tuple_mode(gpu_swap_tuple_w)),
      .swap_base_i           (zhao_fb_tuple_base(gpu_swap_tuple_w)),
      .swap_span_i           (zhao_fb_tuple_span(gpu_swap_tuple_w)),
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
    // NO LONGER 'every response is writer 1' -- that held only while the blit
    // side was tied off, and it would now fire on correct behaviour. What
    // survives is that the lease must never accept a response that is not its
    // own.
    //
    // READ THIS AS A COMPOSITION-LEVEL RESTATEMENT OF A LEAF GUARD, NOT AS
    // EVIDENCE ABOUT THIS WIRING. `zhao_renderer_lease_v2.sv:361` already
    // asserts the same thing inside the leaf, and this copy was fired at by
    // miswiring `rsp_ready` as a plain OR: it stayed silent, because the leaf
    // makes the state unreachable from here. It can therefore only report a
    // regression in the lease, which is a real job and a smaller one than the
    // wording above suggests on its own.
    //
    // The control that IS evidence about the composition is in the driver:
    // stall the blit leaf's retirement during contention and the counts stall at
    // two requests and one response -- the renderer stopping behind a
    // writer-0 response nobody took.
    //
    // No rst_n term -- reset clears rsp_valid, and reading rst_n synchronously
    // beside an asynchronous design is SYNCASYNCNET.
    if (rsp_valid && lease_rsp_ready && (rsp_writer !== 1'b1))
      $fatal(1, "shell_v2_lease_path: the renderer's lease accepted a writer-%0d response", rsp_writer);
  end
`endif

endmodule
