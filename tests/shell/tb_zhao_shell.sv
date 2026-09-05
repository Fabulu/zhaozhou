// tb_zhao_shell.sv — TESTBENCH WRAPPER (W2.7): zhao_shell_top (the whole
// Phase-2 console) + the behavioural SDRAM model (sim/models/, testbench-
// only, D2). Pure wiring: every shell port passes straight through; the
// model hangs off the PHY pins and adds its peek/error surface.
//
// TESTBENCH COMPONENT — excluded from synthesis and from every lint target
// (the model is non-synthesizable by design).

module tb_zhao_shell (
  // clocks + reset (harness-driven, fixed phase: vid=gpu/2, audio=gpu/4)
  input  logic gpu_clk,
  input  logic vid_clk,
  input  logic audio_clk,
  input  logic rst_n,

  // FRAME_RING view (harness = HPS)
  input  logic [1:0]  hps_state_i [0:2],
  input  logic [31:0] hps_byte_len_i [0:2],
  output logic        ring_wr_valid_o,
  output logic [1:0]  ring_wr_slot_o,
  output logic [1:0]  ring_wr_state_o,
  input  logic        ring_wr_ready_i,

  // HPS bridge harness side
  output logic        hps_req_valid_o,
  output logic        hps_req_write_o,
  output logic [31:0] hps_req_addr_o,
  output logic [6:0]  hps_req_len_o,
  input  logic        hps_req_grant_i,
  output logic        hps_wr_valid_o,
  output logic [63:0] hps_wr_data_o,
  output logic        hps_wr_last_o,
  input  logic        hps_rd_valid_i,
  input  logic [63:0] hps_rd_data_i,
  input  logic        hps_rd_last_i,

  // pads
  input  logic [3:0]  pad_present_i,
  input  logic [31:0] pad_buttons_i [0:3],
  input  logic [15:0] pad_lx_i [0:3],
  input  logic [15:0] pad_ly_i [0:3],
  input  logic [15:0] pad_rx_i [0:3],
  input  logic [15:0] pad_ry_i [0:3],

  // audio
  input  logic        aud_wr_valid_i,
  input  logic [15:0] aud_wr_l_i,
  input  logic [15:0] aud_wr_r_i,
  output logic        aud_wr_ready_o,
  output logic        aud_refill_req_o,
  output logic [11:0] aud_occupancy_o,
  output logic        pcm_valid_o,
  output logic [15:0] pcm_l_o,
  output logic [15:0] pcm_r_o,
  output logic        underrun_status_o,
  output logic [31:0] audio_underruns_o,

  // displayed pixel stream
  output logic        px_valid_o,
  output logic [15:0] px_rgb_o,
  output logic [9:0]  px_x_o,
  output logic [7:0]  px_y_o,
  output logic        px_hsync_o,
  output logic        px_vsync_o,
  output logic        px_hblank_o,
  output logic        px_vblank_o,
  output logic        scaler_violation_o,

  // DEBUG.CRC
  output logic [31:0] crc_frame_o,
  output logic        crc_valid_o,
  output logic [31:0] crc_bytes_o,
  output logic        crc_size_err_o,

  // frame boundary
  output logic        gpu_tick_o,
  output logic [31:0] gpu_tick_frame_id_o,
  output logic        gpu_tick_repeated_o,
  output logic [0:0]  gpu_complete_slot_o,
  output logic [63:0] deadline_faults_o,
  output logic [63:0] frame_cycles_o,

  // CMD observability
  output logic [2:0]  slot_state_o [0:2],
  output logic        fence_valid_o,
  output logic [1:0]  fence_slot_o,
  output logic        fence_ok_o,
  output logic [7:0]  fence_status_o,
  output logic [1:0]  mode_act_o,
  output logic        dma_done_o,
  output logic [7:0]  dma_status_o,
  output logic        blit_done_o,
  output logic [7:0]  blit_status_o,

  // INPUT observability
  output logic [639:0] pad_frame_flat_o,
  output logic [15:0]  pad_sequence_o [0:3],
  output logic [63:0]  input_gaps_o,
  output logic [7:0]   rumble_duty_o [0:3],
  output logic [3:0]   rumble_active_o,
  output logic [3:0]   rumble_pwm_o,
  output logic [63:0]  rumble_drops_o,

  // counters window
  input  logic        cnt_snap_ready_i,
  output logic        cnt_snap_valid_o,
  output logic [15:0] cnt_snap_id_o,
  output logic [63:0] cnt_snap_value_o,
  output logic        cnt_window_open_o,
  output logic        cnt_cat_violation_o,

  // MEM observability + shell tripwires
  output logic [31:0] guard_violations_o,
  output logic [63:0] starvation_o,
  output logic        init_done_o,
  output logic [31:0] refresh_stalls_o,
  output logic [31:0] bank_conflicts_o,
  output logic [31:0] scanout_preempted_o,
  output logic [31:0] hps_err_count_o,
  output logic        shell_err_wfifo_o,
  output logic        shell_err_route_o,
  output logic        shell_err_cdc_o,
  output logic        shell_err_framer_o,

  // WHO HOLDS THE FRAMEBUFFER-WRITE LEASE. Driveable, not tied.
  //
  // This was `1'b0` hardwired inside the instantiation below, and that is
  // exactly why the shell's route tripwire could reject every legal ENGINE0
  // burst without any test noticing: the bench could only ever be the blit.
  // A lease with one reachable value is not a lease.
  input  logic        fb_writer_i,

  // SDRAM model peek + errors
  input  logic        peek_en,
  input  logic [25:0] peek_waddr,
  output logic [15:0] peek_data,
  output logic        model_error,
  output logic [5:0]  model_err_kind,  // {mrs,protocol,refresh,trc,trp,trcd}

  // ---- THE RENDER PORT, EXPOSED 2026-09-04 (docket D19e) ------------------
  // Until now every one of these was tied to a constant here and the bench's
  // own header said it "does not draw". That made zhao_shell_top's composition
  // of geometry and raster the only part of the console that was elaborated,
  // fitted and timed but NEVER SIMULATED -- and it is the part D1 spent eleven
  // rounds closing timing on.
  //
  // They are exposed rather than driven from inside, so a bench that does not
  // draw still names every pin (the discipline the tie-offs were protecting)
  // and a bench that does draw gets them without a second wrapper.
  input  logic               render_frame_begin_i,
  input  logic               render_frame_end_i,
  input  logic [5:0]         render_grid_w_i,
  input  logic [5:0]         render_grid_h_i,
  // ---- D22 step 1: move the input boundary back to SETUP ------------------
  // The shell has always been fed PRECOMPUTED EDGE EQUATIONS. D22's staircase
  // starts by moving that boundary backwards one block, so the bench supplies
  // VERTICES and GEOM.SETUP computes the coefficients inside the composed
  // design. `setup_mode_i` selects which path drives the raster front end.
  //
  // It defaults to 0 -- the precomputed path, bit-identical to before -- so
  // every existing test and every golden is unaffected by this port existing.
  // The point of the mode is that the SAME triangle can be drawn both ways and
  // the two pictures compared, which is the only evidence that actually proves
  // the newly connected hardware draws.
  input  logic               setup_mode_i,
  // Observability for the D22 step-1 handshake. Without these, "the triangle
  // was not accepted" is a symptom with no visible cause.
  output logic               dbg_su_tri_ready_o,
  output logic               dbg_su_out_valid_o,
  output logic               dbg_shell_tri_ready_o,
  input  logic signed [47:0] setup_area2_i,

  // ---- D22 STEP 2: GEOM.DEPTHQUANT -----------------------------------------
  // Step 1 moved the boundary from precomputed edge coefficients to vertices.
  // Step 2 moves it again, one block further back, for DEPTH.
  //
  // The depth value has ALWAYS come in -- it is just not where a port-name
  // search finds it. `zhao_raster_tile_pipe.sv:446` reads
  //
  //     assign frag_depth = fill_r[31:8];
  //
  // so `render_fill_word_i` is not a colour but a flat-fragment record:
  // [63:40] vertex RGB, [39:32] effect tag, [31:8] the 24-bit invw24 depth,
  // [7:0] stencil reference. The bench has been hand-packing a PRECOMPUTED
  // invw24 into those bits.
  //
  // In depth mode the bench supplies `w` instead and GEOM.DEPTHQUANT computes
  // the canonical invw24 inside the composed design, driving those same bits.
  // Same evidence shape as step 1: draw the same triangle both ways, require
  // identical framebuffers.
  //
  // Defaults to 0 -- the precomputed path, bit-identical -- so every existing
  // test and golden is unaffected by this port existing.
  input  logic               depth_mode_i,
  input  logic [39:0]        depth_w_i,          // fx16 raw, S15.16
  input  logic [1:0]         depth_profile_i,    // per-vertex, not latched
  // ---- D22 STEP 3: GEOM.CLIP ------------------------------------------------
  // Step 1 replaced precomputed edge coefficients with vertices. Step 2
  // replaced the precomputed depth with w. Step 3 moves the boundary again:
  // the bench stops supplying setup_area2_i and the scan box (render_min_x_i
  // .. render_max_y_i), and GEOM.CLIP computes them -- together with the
  // WINDING NORMALISATION, which is the part that makes this more than
  // plumbing.
  //
  // CLIP may swap B and C to make 2A positive, and when it does it swaps their
  // ATTRIBUTES with them. Its own header says why that swap lives beside the
  // decision: "Swapping the positions and not the attributes produces a
  // triangle that is geometrically correct and shaded wrong, on exactly the
  // back-facing half of the scene -- so it survives any test whose triangles
  // are all wound one way."
  //
  // So the vertices SETUP sees in clip mode are CLIP's, not the bench's.
  // Defaults to 0, bit-identical to before.
  input  logic               clip_mode_i,
  output logic               dbg_clip_valid_o,
  output logic signed [47:0] dbg_clip_area2_o,
  output logic               dbg_clip_flip_o,
  output logic               dbg_dq_valid_o,
  output logic [23:0]        dbg_dq_invw24_o,
  input  logic               render_tri_valid_i,
  output logic               render_tri_ready_o,
  input  logic signed [22:0] render_kx0_i,
  input  logic signed [22:0] render_ky0_i,
  input  logic signed [47:0] render_kc0_i,
  input  logic signed [22:0] render_kx1_i,
  input  logic signed [22:0] render_ky1_i,
  input  logic signed [47:0] render_kc1_i,
  input  logic signed [22:0] render_kx2_i,
  input  logic signed [22:0] render_ky2_i,
  input  logic signed [47:0] render_kc2_i,
  input  logic [2:0]         render_tl_i,
  input  logic signed [20:0] render_ax_i,
  input  logic signed [20:0] render_ay_i,
  input  logic signed [20:0] render_bx_i,
  input  logic signed [20:0] render_by_i,
  input  logic signed [20:0] render_cx_i,
  input  logic signed [20:0] render_cy_i,
  input  logic signed [11:0] render_min_x_i,
  input  logic signed [11:0] render_max_x_i,
  input  logic signed [11:0] render_min_y_i,
  input  logic signed [11:0] render_max_y_i,
  input  logic [15:0]        render_src_id_i,

  // The JOB words that say what a covered fragment becomes. Tied to zero here
  // until 2026-09-04 alongside the triangle port, which made "the tile was
  // written" indistinguishable from "the tile was untouched" -- a fill of zero
  // into memory that is already zero writes nothing observable.
  input  logic [63:0]        render_fill_word_i,
  input  logic [63:0]        render_clear_word_i,
  input  logic [31:0]        render_state_i,
  input  logic [7:0]         render_src_a_i,
  input  logic [23:0]        render_texel_rgb_i,
  input  logic [7:0]         render_texel_a_i,
  input  logic [7:0]         render_texel_idx_i,

  // ---- render OBSERVABLES ------------------------------------------------
  // The shell has always emitted these and the wrapper has always discarded
  // them with `()`. Bringing them out is what turns "nothing landed in memory"
  // from a guess into a measurement: a triangle that produced zero pixels and
  // one that produced pixels nobody wrote are different faults with the same
  // symptom.
  output logic        render_busy_o,
  output logic [31:0] render_pixels_o,
  output logic [31:0] render_bursts_o,
  output logic [31:0] render_issued_words_o,
  output logic [31:0] render_retired_words_o,
  output logic        render_drained_o,
  output logic        render_fatal_o,
  output logic        render_stream_error_o,
  output logic        render_overflow_o,
  output logic        render_fragment_error_o,

  // ---- THE GUARD WINDOW, probed hierarchically ---------------------------
  // `fb_lease_valid` and `map_span_q` are internal to zhao_shell_top and there
  // is no reason to give production RTL a debug port for them. This is a
  // TESTBENCH -- never linted, never synthesised -- so a cross-module reference
  // here costs nothing in silicon and is the honest place for it.
  //
  // A drawing test needs them because docket D19f: RASTER.FBWRITE may only
  // write while a DISPLAY BLIT lease is live, into that blit's span. Without
  // seeing the window, a test can only offer a triangle and hope it lands
  // inside one.
  output logic        dbg_fb_lease_valid_o,
  output logic        dbg_fb_lease_slot_o,
  output logic [31:0] dbg_map_span_o,
  // The RENDER guard's latched violating request. `fatal` says a write was
  // refused; this says which one, which is the difference between "the path is
  // dead" and "the path aimed somewhere the window does not cover".
  output logic [26:0] dbg_render_gv_addr_o,
  output logic [6:0]  dbg_render_gv_len_o,
  output logic [2:0]  dbg_render_gv_client_o,
  output logic        dbg_render_gv_write_o,
  output logic [31:0] dbg_render_gv_cnt_o,
  // Where fbwrite actually ADDRESSES. D19h: the counter says 3,328 pixels and
  // memory shows 64 changed halfwords, and "the writes land somewhere the peek
  // does not cover" is one of the three unevidenced candidates. This settles it
  // by watching the request rather than inferring from the result.
  output logic        dbg_render_req_valid_o,
  output logic [26:0] dbg_render_req_addr_o,
  output logic [6:0]  dbg_render_req_len_o,
  output logic        dbg_render_req_write_o,

  // WHERE THE CANVAS IS. Tied to zero here until 2026-09-04, which is why the
  // first drawing test saw fbwrite's address never advance: with stride 0 every
  // row of every tile resolves to the same address, so 208 bursts hammered 128
  // bytes. Not a defect -- an unconfigured frame (docket D19h).
  input  logic [26:0] render_fb_base_i,
  input  logic [15:0] render_fb_stride_i
);

  logic        phy_cs_n, phy_ras_n, phy_cas_n, phy_we_n, phy_dq_oe;
  logic [12:0] phy_a;
  logic [1:0]  phy_ba, phy_dqm;
  logic [15:0] phy_dq_o, phy_dq_i;

  assign dbg_fb_lease_valid_o = u_shell.fb_lease_valid;
  assign dbg_fb_lease_slot_o  = u_shell.fb_lease_slot;
  assign dbg_map_span_o       = u_shell.map_span_q;
  assign dbg_render_gv_addr_o   = u_shell.render_gv_req.addr;
  assign dbg_render_gv_len_o    = u_shell.render_gv_req.len;
  assign dbg_render_gv_client_o = u_shell.render_gv_req.client;
  assign dbg_render_gv_write_o  = u_shell.render_gv_req.write;
  assign dbg_render_gv_cnt_o    = u_shell.render_gv_cnt;
  assign dbg_render_req_valid_o = u_shell.render_guard_req.valid;
  assign dbg_render_req_addr_o  = u_shell.render_guard_req.addr;
  assign dbg_render_req_len_o   = u_shell.render_guard_req.len;
  assign dbg_render_req_write_o = u_shell.render_guard_req.write;

  // ---- GEOM.SETUP, composed (D22 step 1) -----------------------------------
  logic               shell_tri_ready;   // declared before use: Verilator's
                                         // IMPLICIT warning is an error here
  logic               su_out_valid;
  logic               su_tri_ready;
  logic signed [22:0] su_kx0, su_ky0, su_kx1, su_ky1, su_kx2, su_ky2;
  logic signed [47:0] su_kc0, su_kc1, su_kc2;
  logic        [2:0]  su_tl;
  logic signed [20:0] su_ax, su_ay, su_bx, su_by, su_cx, su_cy;

  // ---- D22 step 2: DEPTHQUANT and the reciprocal it calls -------------------
  // DEPTHQUANT does not own a reciprocal; its header is explicit that "the
  // console already has ONE rcp_u24 law; a second ROM would be a second law",
  // so it calls zhao_raster_rcp24_svc. That block is the same one the composed
  // texture island uses, where it completed 64 reciprocals in the composed
  // test -- so this is a proven service, not a new dependency.
  logic        dq_v_ready, dq_d_valid;
  logic [23:0] dq_invw24;
  logic        dq_d_behind;
  logic [15:0] dq_d_src;
  logic        dq_rcp_valid, dq_rcp_rready;
  logic [23:0] dq_rcp_d;
  logic        dq_rcp_v_ready, dq_rcp_r_valid;
  logic [23:0] dq_rcp_r;
  logic [5:0]  dq_rcp_k;
  /* verilator lint_off UNUSEDSIGNAL */
  logic        dq_rcp_dzero;
  logic [7:0]  dq_rcp_tok;
  logic [3:0]  dq_rcp_occ;
  logic [31:0] dq_rcp_acc, dq_rcp_comp, dq_rcp_busy;
  logic [31:0] dq_vertices, dq_near, dq_far, dq_sat, dq_refused;
  /* verilator lint_on UNUSEDSIGNAL */

  zhao_geom_depthquant #(.SRCW(16)) u_depthquant (
      .clk(gpu_clk), .rst_n(rst_n),
      .v_valid_i(render_tri_valid_i & depth_mode_i), .v_ready_o(dq_v_ready),
      .v_w_i(depth_w_i), .v_behind_i(1'b0), .v_profile_i(depth_profile_i),
      .v_src_id_i(render_src_id_i),
      .d_valid_o(dq_d_valid), .d_ready_i(1'b1),
      .d_invw24_o(dq_invw24), .d_behind_o(dq_d_behind), .d_src_id_o(dq_d_src),
      .rcp_valid_o(dq_rcp_valid), .rcp_ready_i(dq_rcp_v_ready),
      .rcp_d_o(dq_rcp_d),
      .rcp_rvalid_i(dq_rcp_r_valid), .rcp_rready_o(dq_rcp_rready),
      .rcp_r_i(dq_rcp_r), .rcp_k_i(dq_rcp_k),
      .vertices_o(dq_vertices), .clamped_near_o(dq_near),
      .clamped_far_o(dq_far), .saturated_o(dq_sat), .refused_o(dq_refused));

  zhao_raster_rcp24_svc #(.NCTX(8), .TOKW(8)) u_dq_rcp (
      .clk(gpu_clk), .rst_n(rst_n),
      .v_valid_i(dq_rcp_valid), .v_ready_o(dq_rcp_v_ready),
      .d_i(dq_rcp_d), .v_tok_i(8'd0),
      .r_valid_o(dq_rcp_r_valid), .r_ready_i(dq_rcp_rready),
      .r_o(dq_rcp_r), .k_o(dq_rcp_k), .d_zero_o(dq_rcp_dzero),
      .r_tok_o(dq_rcp_tok),
      .accepted_o(dq_rcp_acc), .completed_o(dq_rcp_comp),
      .mul_busy_o(dq_rcp_busy), .occupancy_o(dq_rcp_occ));

  // The computed depth is LATCHED, because the fill word must hold still for
  // the whole triangle while DEPTHQUANT's answer arrives some cycles after the
  // vertex was offered. A combinational path here would change the depth
  // mid-triangle, which is the class of fault that looks like a rendering bug.
  logic [23:0] dq_invw24_r;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n)          dq_invw24_r <= 24'd0;
    else if (dq_d_valid) dq_invw24_r <= dq_invw24;
  end

  assign dbg_dq_valid_o   = dq_d_valid;
  assign dbg_dq_invw24_o  = dq_invw24_r;

  // THE BOUNDARY. In depth mode the fill word's depth field comes from
  // DEPTHQUANT; every other field is the bench's, untouched.
  wire [63:0] m_fill_word = depth_mode_i
                          ? {render_fill_word_i[63:32], dq_invw24_r,
                             render_fill_word_i[7:0]}
                          : render_fill_word_i;

  // ---- D22 step 3: GEOM.CLIP ------------------------------------------------
  localparam int unsigned CLIP_ATTRS = 7;
  logic               cl_tri_ready, cl_out_valid;
  logic signed [20:0] cl_ax, cl_ay, cl_bx, cl_by, cl_cx, cl_cy;
  logic signed [47:0] cl_area2;
  logic signed [11:0] cl_min_x, cl_max_x, cl_min_y, cl_max_y;
  /* verilator lint_off UNUSEDSIGNAL */
  logic [15:0] cl_src_id;
  logic [CLIP_ATTRS*32-1:0] cl_attr_a, cl_attr_b, cl_attr_c;
  logic        cl_ret_valid;
  logic [2:0]  cl_ret_verdict;
  logic [31:0] cl_sub, cl_clipped, cl_culled;
  /* verilator lint_on UNUSEDSIGNAL */

  zhao_geom_clip #(.ATTRS(CLIP_ATTRS)) u_clip (
      .clk(gpu_clk), .rst_n(rst_n),
      .tri_valid_i(render_tri_valid_i & clip_mode_i),
      .tri_ready_o(cl_tri_ready),
      .tri_ax_i(render_ax_i), .tri_ay_i(render_ay_i),
      .tri_bx_i(render_bx_i), .tri_by_i(render_by_i),
      .tri_cx_i(render_cx_i), .tri_cy_i(render_cy_i),
      // No vertex is behind: the bench supplies screen-space vertices that
      // GEOM.PROJECT would already have accepted. A w <= 0 verdict is step 4's
      // concern, and asserting it here would exercise a rejection path this
      // step is not moving.
      .tri_behind_i(3'b000),
      .tri_src_id_i(render_src_id_i),
      .tri_attr_a_i('0), .tri_attr_b_i('0), .tri_attr_c_i('0),
      // The scissor is the render grid in whole pixels; sixteen pixels per
      // tile is the shell's own tile size, so this is the same rectangle the
      // bench derives its scan box from -- which is why the two agree.
      .vp_x0_i(12'd0), .vp_y0_i(12'd0),
      .vp_w_i({4'd0, render_grid_w_i, 4'd0}),
      .vp_h_i({4'd0, render_grid_h_i, 4'd0}),
      .cull_mode_i(2'd0),               // NONE: culling is not this step
      .out_valid_o(cl_out_valid), .out_ready_i(su_tri_ready),
      .out_ax_o(cl_ax), .out_ay_o(cl_ay),
      .out_bx_o(cl_bx), .out_by_o(cl_by),
      .out_cx_o(cl_cx), .out_cy_o(cl_cy),
      .out_area2_o(cl_area2),
      .out_min_x_o(cl_min_x), .out_max_x_o(cl_max_x),
      .out_min_y_o(cl_min_y), .out_max_y_o(cl_max_y),
      .out_src_id_o(cl_src_id),
      .out_attr_a_o(cl_attr_a), .out_attr_b_o(cl_attr_b), .out_attr_c_o(cl_attr_c),
      .out_flip_o(dbg_clip_flip_o),
      .ret_valid_o(cl_ret_valid), .ret_verdict_o(cl_ret_verdict),
      .triangles_submitted_o(cl_sub), .triangles_clipped_o(cl_clipped),
      .triangles_culled_o(cl_culled));

  assign dbg_clip_valid_o = cl_out_valid;
  assign dbg_clip_area2_o = cl_area2;

  // THE STEP-3 BOUNDARY. In clip mode every geometric input SETUP sees comes
  // from CLIP -- vertices included, because the winding normalisation may have
  // swapped two of them.
  wire               c_valid = clip_mode_i ? cl_out_valid
                                           : (render_tri_valid_i & setup_mode_i);
  wire signed [20:0] c_ax    = clip_mode_i ? cl_ax    : render_ax_i;
  wire signed [20:0] c_ay    = clip_mode_i ? cl_ay    : render_ay_i;
  wire signed [20:0] c_bx    = clip_mode_i ? cl_bx    : render_bx_i;
  wire signed [20:0] c_by    = clip_mode_i ? cl_by    : render_by_i;
  wire signed [20:0] c_cx    = clip_mode_i ? cl_cx    : render_cx_i;
  wire signed [20:0] c_cy    = clip_mode_i ? cl_cy    : render_cy_i;
  wire signed [47:0] c_area2 = clip_mode_i ? cl_area2 : setup_area2_i;
  wire signed [11:0] c_min_x = clip_mode_i ? cl_min_x : render_min_x_i;
  wire signed [11:0] c_max_x = clip_mode_i ? cl_max_x : render_max_x_i;
  wire signed [11:0] c_min_y = clip_mode_i ? cl_min_y : render_min_y_i;
  wire signed [11:0] c_max_y = clip_mode_i ? cl_max_y : render_max_y_i;

  zhao_geom_setup u_setup (
      .clk(gpu_clk), .rst_n(rst_n),
      .tri_valid_i(c_valid),
      .tri_ready_o(su_tri_ready),
      .tri_ax_i(c_ax), .tri_ay_i(c_ay),
      .tri_bx_i(c_bx), .tri_by_i(c_by),
      .tri_cx_i(c_cx), .tri_cy_i(c_cy),
      .tri_area2_i(c_area2),
      .tri_min_x_i(c_min_x), .tri_max_x_i(c_max_x),
      .tri_min_y_i(c_min_y), .tri_max_y_i(c_max_y),
      .tri_src_id_i(render_src_id_i),
      .out_valid_o(su_out_valid),
      .out_ready_i(shell_tri_ready),
      .out_kx0_o(su_kx0), .out_ky0_o(su_ky0), .out_kc0_o(su_kc0),
      .out_kx1_o(su_kx1), .out_ky1_o(su_ky1), .out_kc1_o(su_kc1),
      .out_kx2_o(su_kx2), .out_ky2_o(su_ky2), .out_kc2_o(su_kc2),
      .out_tl_o(su_tl), .out_area2_o(),
      .out_ax_o(su_ax), .out_ay_o(su_ay),
      .out_bx_o(su_bx), .out_by_o(su_by),
      .out_cx_o(su_cx), .out_cy_o(su_cy),
      .out_min_x_o(), .out_max_x_o(), .out_min_y_o(), .out_max_y_o(),
      .out_src_id_o(), .triangles_submitted_o()
  );

  // The mux. In setup mode the shell is driven by SETUP's outputs and the
  // bench's ready comes from SETUP's input side; otherwise everything is
  // exactly as before.
  assign render_tri_ready_o = clip_mode_i  ? cl_tri_ready
                            : setup_mode_i ? su_tri_ready
                                           : shell_tri_ready;
  assign dbg_su_tri_ready_o    = su_tri_ready;
  assign dbg_su_out_valid_o    = su_out_valid;
  assign dbg_shell_tri_ready_o = shell_tri_ready;

  wire               m_tri_valid = (setup_mode_i || clip_mode_i) ? su_out_valid
                                                                : render_tri_valid_i;
  wire signed [22:0] m_kx0 = setup_mode_i ? su_kx0 : render_kx0_i;
  wire signed [22:0] m_ky0 = setup_mode_i ? su_ky0 : render_ky0_i;
  wire signed [47:0] m_kc0 = setup_mode_i ? su_kc0 : render_kc0_i;
  wire signed [22:0] m_kx1 = setup_mode_i ? su_kx1 : render_kx1_i;
  wire signed [22:0] m_ky1 = setup_mode_i ? su_ky1 : render_ky1_i;
  wire signed [47:0] m_kc1 = setup_mode_i ? su_kc1 : render_kc1_i;
  wire signed [22:0] m_kx2 = setup_mode_i ? su_kx2 : render_kx2_i;
  wire signed [22:0] m_ky2 = setup_mode_i ? su_ky2 : render_ky2_i;
  wire signed [47:0] m_kc2 = setup_mode_i ? su_kc2 : render_kc2_i;
  wire        [2:0]  m_tl  = setup_mode_i ? su_tl  : render_tl_i;
  wire signed [20:0] m_ax = setup_mode_i ? su_ax : render_ax_i;
  wire signed [20:0] m_ay = setup_mode_i ? su_ay : render_ay_i;
  wire signed [20:0] m_bx = setup_mode_i ? su_bx : render_bx_i;
  wire signed [20:0] m_by = setup_mode_i ? su_by : render_by_i;
  wire signed [20:0] m_cx = setup_mode_i ? su_cx : render_cx_i;
  wire signed [20:0] m_cy = setup_mode_i ? su_cy : render_cy_i;

  zhao_shell_top u_shell (
    .gpu_clk, .vid_clk, .audio_clk, .rst_n,
    .hps_state_i, .hps_byte_len_i,
    .ring_wr_valid_o, .ring_wr_slot_o, .ring_wr_state_o, .ring_wr_ready_i,
    .hps_req_valid_o, .hps_req_write_o, .hps_req_addr_o, .hps_req_len_o,
    .hps_req_grant_i,
    .hps_wr_valid_o, .hps_wr_data_o, .hps_wr_last_o,
    .hps_rd_valid_i, .hps_rd_data_i, .hps_rd_last_i,
    .pad_present_i, .pad_buttons_i, .pad_lx_i, .pad_ly_i, .pad_rx_i, .pad_ry_i,
    .aud_wr_valid_i, .aud_wr_l_i, .aud_wr_r_i,
    .aud_wr_ready_o, .aud_refill_req_o, .aud_occupancy_o,
    .pcm_valid_o, .pcm_l_o, .pcm_r_o, .underrun_status_o, .audio_underruns_o,
    .px_valid_o, .px_rgb_o, .px_x_o, .px_y_o,
    .px_hsync_o, .px_vsync_o, .px_hblank_o, .px_vblank_o, .scaler_violation_o,
    .crc_frame_o, .crc_valid_o, .crc_bytes_o, .crc_size_err_o,
    .gpu_tick_o, .gpu_tick_frame_id_o, .gpu_tick_repeated_o,
    .gpu_complete_slot_o,
    .deadline_faults_o, .frame_cycles_o,
    .slot_state_o, .fence_valid_o, .fence_slot_o, .fence_ok_o, .fence_status_o,
    .mode_act_o, .dma_done_o, .dma_status_o, .blit_done_o, .blit_status_o,
    .pad_frame_flat_o, .pad_sequence_o, .input_gaps_o,
    .rumble_duty_o, .rumble_active_o, .rumble_pwm_o, .rumble_drops_o,
    .cnt_snap_ready_i, .cnt_snap_valid_o, .cnt_snap_id_o, .cnt_snap_value_o,
    .cnt_window_open_o, .cnt_cat_violation_o,
    .guard_violations_o, .starvation_o,
    .init_done_o, .refresh_stalls_o, .bank_conflicts_o, .scanout_preempted_o,
    .hps_err_count_o,
    .shell_err_wfifo_o, .shell_err_route_o, .shell_err_cdc_o,
    .shell_err_framer_o,
    .phy_cs_n_o (phy_cs_n),
    .phy_ras_n_o(phy_ras_n),
    .phy_cas_n_o(phy_cas_n),
    .phy_we_n_o (phy_we_n),
    .phy_a_o    (phy_a),
    .phy_ba_o   (phy_ba),
    .phy_dq_o   (phy_dq_o),
    .phy_dq_oe_o(phy_dq_oe),
    .phy_dqm_o  (phy_dqm),
    // ---- RENDER, tied off ------------------------------------------------
    // The render port is now BROUGHT OUT (docket D19e) rather than tied to
    // zero here. A bench that does not draw holds these quiet from the C++
    // side, which keeps the original discipline -- every pin named, none left
    // dangling to inherit an X -- while making the shell's geometry/raster
    // composition simulable for the first time.
    .render_frame_begin_i(render_frame_begin_i), .render_frame_end_i(render_frame_end_i),
    .render_grid_w_i(render_grid_w_i), .render_grid_h_i(render_grid_h_i),
    .render_tri_valid_i(m_tri_valid), .render_tri_ready_o(shell_tri_ready),
    .render_kx0_i(m_kx0), .render_ky0_i(m_ky0), .render_kc0_i(m_kc0),
    .render_kx1_i(m_kx1), .render_ky1_i(m_ky1), .render_kc1_i(m_kc1),
    .render_kx2_i(m_kx2), .render_ky2_i(m_ky2), .render_kc2_i(m_kc2),
    .render_tl_i(m_tl),
    .render_ax_i(m_ax), .render_ay_i(m_ay),
    .render_bx_i(m_bx), .render_by_i(m_by),
    .render_cx_i(m_cx), .render_cy_i(m_cy),
    .render_min_x_i(render_min_x_i), .render_max_x_i(render_max_x_i),
    .render_min_y_i(render_min_y_i), .render_max_y_i(render_max_y_i),
    .render_src_id_i(render_src_id_i),
    .render_fill_word_i(m_fill_word), .render_clear_word_i(render_clear_word_i),
    .render_state_i(render_state_i), .render_src_a_i(render_src_a_i),
    .render_texel_rgb_i(render_texel_rgb_i), .render_texel_a_i(render_texel_a_i),
    .render_texel_idx_i(render_texel_idx_i),
    .render_fb_base_i(render_fb_base_i), .render_fb_stride_i(render_fb_stride_i),
    // Driven by the bench now -- see the port comment above.
    .fb_writer_i(fb_writer_i),
    .render_drain_done_o(), .render_busy_o(render_busy_o),
    .render_pixels_o(render_pixels_o), .render_bursts_o(render_bursts_o),
    .render_stream_error_o(render_stream_error_o), .render_overflow_o(render_overflow_o),
    .render_fragment_error_o(render_fragment_error_o),
    .render_drained_o(render_drained_o), .render_fatal_o(render_fatal_o),
    .render_issued_words_o(render_issued_words_o),
    .render_retired_words_o(render_retired_words_o),

    .phy_dq_i   (phy_dq_i)
  );

  zhao_sdram_model u_model (
    .clk (gpu_clk),
    .phy_cs_n, .phy_ras_n, .phy_cas_n, .phy_we_n,
    .phy_a, .phy_ba,
    .phy_dq_o, .phy_dq_oe, .phy_dqm, .phy_dq_i,
    .peek_en, .peek_waddr, .peek_data,
    .err_trcd (model_err_kind[0]), .err_trp (model_err_kind[1]),
    .err_trc (model_err_kind[2]),
    .err_refresh_interval (model_err_kind[3]), .err_protocol (model_err_kind[4]),
    .err_mrs (model_err_kind[5]),
    .model_error
  );

endmodule : tb_zhao_shell
