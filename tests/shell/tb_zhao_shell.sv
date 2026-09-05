// tb_zhao_shell.sv — TESTBENCH WRAPPER (W2.7): zhao_shell_top (the whole
// Phase-2 console) + the behavioural SDRAM model (sim/models/, testbench-
// only, D2). Pure wiring: every shell port passes straight through; the
// model hangs off the PHY pins and adds its peek/error surface.
//
// TESTBENCH COMPONENT — excluded from synthesis and from every lint target
// (the model is non-synthesizable by design).

// GEOM.MESHFETCH (D22 step 6) is the first block this bench composes whose
// ports are PACKAGE TYPEDEFS -- zhao_guard_req_t and zhao_guard_rsp_t. Every
// earlier step used plain vectors, so the bench had never needed an import,
// and the message for the lack of one is "Can't find typedef/interface" --
// which reads like a missing FILE rather than a missing IMPORT.
//
// (The first draft of this note began the line with the tool's name, which is
// parsed as a lint pragma and fails the build with "Unknown verilator
// comment". A comment that starts with that word is a directive, not prose.)
import zhao_pkg::*;

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
  // ---- D22 STEP 4: GEOM.PROJECT ---------------------------------------------
  // The staircase so far has moved the boundary back one block at a time:
  //   1  edge coefficients -> GEOM.SETUP
  //   2  invw24 depth      -> GEOM.DEPTHQUANT
  //   3  2A and scan box   -> GEOM.CLIP
  //   4  SCREEN VERTICES   -> GEOM.PROJECT
  //
  // Step 4 is the one that ties the front end together, because PROJECT feeds
  // BOTH downstream blocks. Its own header says so about the second:
  //
  //   > clip.w itself, fx16 raw. GEOM.DEPTHQUANT consumes THIS and not
  //   > out_d_o: the ratified depth law performs its own rcp_u24 on w, and the
  //   > quotient has already lost the precision reconstruction would need.
  //
  // So in project mode the bench stops supplying screen vertices AND stops
  // supplying `w`: PROJECT produces the screen x/y CLIP consumes, the behind
  // flags CLIP tests, and the w DEPTHQUANT quantises.
  //
  // PROJECT IS PER-VERTEX and CLIP is per-triangle, so the collector below
  // pushes A, B, C through in turn and latches the three results. That
  // sequencer is bench scaffolding, not console logic -- in the machine the
  // vertex stream arrives from GEOM.ASSEMBLE and the collector is that block's
  // job.
  // ---- D22 STEP 5: GEOM.ASSEMBLE --------------------------------------------
  // Steps 1-4 moved the boundary back through the per-triangle maths. Step 5
  // moves it past a different kind of line: the triangle's VERTEX SELECTION.
  //
  // Until now the bench has said "here are three vertices". GEOM.ASSEMBLE
  // instead takes a MESHLET -- a vertex offset, a vertex count, a triangle
  // count, a material and a raster state -- pulls the u8 local index stream
  // three at a time, and emits one TriangleDescriptor per triangle carrying
  // VERTEX IDS. So in assemble mode the bench supplies a meshlet and an index
  // stream, and the hardware decides which three vertices form the triangle.
  //
  // The bench still holds the vertex TABLE and looks up `t_v0_o` .. `t_v2_o`
  // in it. That table is GEOM.VDECODE's job in the machine (32 bytes per
  // vertex, naturally aligned, per its contract) and is deliberately NOT
  // pretended here -- step 5 moves the selection, not the decode.
  //
  // Defaults to 0, bit-identical to before.
  // ---- D22 STEP 6: GEOM.MESHFETCH -------------------------------------------
  // The last tread. Step 5 had the bench hand ASSEMBLE a meshlet -- a vertex
  // count, a triangle count, a material. Step 6 stops supplying it: MESHFETCH
  // reads a 64-byte meshlet DESCRIPTOR out of memory, validates it, culls it,
  // and emits the meshlet ASSEMBLE consumes.
  //
  // WHAT THE BENCH PLAYS, SAID PLAINLY. MESHFETCH is the only zhao_guard_req_t
  // client in the geometry subsystem, so this bench plays THREE interfaces on
  // its behalf: the memory guard (grant), the beat stream (the descriptor
  // bytes) and the cull service (a visibility verdict). That is a lot of
  // played surface, and it means this step proves the DESCRIPTOR PATH inside
  // the composed shell -- not the asset fetcher, and not culling.
  //
  // The real thing needs one asset fetcher over GEOM.ASSET_POOL serving three
  // consumers (descriptors, the u8 index stream, vertex records), which is the
  // memory path docket D22 identified as the single blocker and
  // `spec/memory_rules.md` §5f ruled the region for. Playing it here is
  // scaffolding with a name, not that fetcher.
  input  logic               meshfetch_mode_i,
  input  logic [63:0]        mf_desc_i [8],     // the 64-byte descriptor
  input  logic               mf_crc_ok_i,
  output logic               dbg_mf_valid_o,
  output logic [7:0]         dbg_mf_vcount_o,
  output logic [7:0]         dbg_mf_tcount_o,
  output logic [15:0]        dbg_mf_material_o,
  output logic [1:0]         dbg_mf_vis_o,
  // Why a meshlet did or did not come out. MESHFETCH refuses for seven
  // distinct reasons and counts them apart on purpose -- its contract says
  // three different failures with three different causes, and one counter for
  // all of them would name none. Exposing them means a silent non-emission is
  // LOCATED rather than merely observed.
  output logic               dbg_mf_greq_o,
  output logic               dbg_mf_granted_o,
  output logic [3:0]         dbg_mf_beat_o,
  output logic               dbg_mf_cull_tick_o,
  output logic [31:0]        dbg_mf_fetched_o,
  output logic [31:0]        dbg_mf_denied_o,
  output logic [31:0]        dbg_mf_refused0_o,
  input  logic               assemble_mode_i,
  input  logic [7:0]         asm_vertex_count_i,
  input  logic [7:0]         asm_triangle_count_i,
  // The index stream the walk pulls. Three u8 local indices per triplet; the
  // bench answers combinationally from a flat vector so the responder cannot
  // be the thing under test.
  input  logic [8*3*4-1:0]   asm_index_stream_i,   // up to 4 triplets
  output logic               dbg_asm_valid_o,
  output logic [15:0]        dbg_asm_v0_o,
  output logic [15:0]        dbg_asm_v1_o,
  output logic [15:0]        dbg_asm_v2_o,
  output logic [31:0]        dbg_asm_triangles_o,
  // The vertex TABLE, bench-held: four vertices' worth of clip-space
  // coordinates, selected by the ID GEOM.ASSEMBLE emits.
  input  logic signed [31:0] asm_vtx_x_i [4],
  input  logic signed [31:0] asm_vtx_y_i [4],
  input  logic signed [31:0] asm_vtx_z_i [4],
  // ---- TREAD 7: GEOM.VDECODE ------------------------------------------------
  // The bench stops supplying DECODED coordinates and supplies the 32-byte
  // RECORDS they came from. `zhao_geom_vdecode` turns bytes into the vertex,
  // and the table above stops being an input to the drawing path.
  //
  // WHAT THIS TREAD DELIBERATELY DOES NOT PROVE, stated here so the step is not
  // read as more than it is:
  //
  //   * THE TRANSFORM. A record holds a MODEL-space position; the table it
  //     replaces held CLIP-space. There is no transform block in this shell, so
  //     the test authors records whose positions ARE the clip-space values and
  //     the transform is IDENTITY BY CONSTRUCTION. That is the same device step
  //     6 used when it answered the cull with a constant VISIBLE: hold the
  //     neighbouring stage at a known value so the tread under test is the only
  //     thing that can fail.
  //   * THE BATCH ENGINE. zhao_geom_vdecode's own header says it is the record
  //     leaf and not GEOM.VDECODE's batch engine -- vertex_count, addressing
  //     across burst boundaries, and the all-or-nothing batch rule are not
  //     here. The ledger entry stays SPECIFIED; this tread must not be read as
  //     advancing it.
  //   * ANY FORMAT BUT 0. Formats 1 and 2 are bake-off gated.
  input  logic               vdecode_mode_i,
  input  logic [255:0]       vd_rec_i [4],
  // ---- TREAD 8: GEOM.ASSETFETCH ---------------------------------------------
  // The bench stops SYNTHESISING vertex records and supplies raw POOL BYTES.
  // GEOM.ASSETFETCH reads the meshlet's footprint out of them as aligned
  // 64-byte lines and streams each 32-byte record to GEOM.VDECODE, so the
  // record port stops being a bench input and becomes an internal seam.
  //
  // Its footprint comes from GEOM.MESHFETCH's own descriptor outputs
  // (`r_vertex_offset_o`, `r_vertex_count_o`), so this tread also closes the
  // MESHFETCH -> ASSETFETCH -> VDECODE path rather than only replacing one
  // producer.
  //
  // WHAT IT DOES NOT MOVE: the INDEX stream. GEOM.ASSEMBLE is still handed
  // `asm_index_stream_i` by the bench even though ASSETFETCH can serve it,
  // because a tread moves ONE thing and this one moves the vertex records.
  // The bench also still plays the guard and the beats -- the boundary moves
  // out by one block, not to zero.
  input  logic               assetfetch_mode_i,
  // ---- TREAD 9: the u8 INDEX STREAM through GEOM.ASSETFETCH ----------------
  // Tread 8 routed the vertex records through the fetcher and left the index
  // stream the bench's. The fetcher was ALREADY READING the index line -- of
  // its 24 beats, 8 were the index run -- and simply throwing the result away
  // because `ix_req_i` was tied off. This connects it.
  //
  // ASSEMBLE's index port has NO READY, by that block's own deliberate choice
  // ("378 bytes of buffer to save a stream port is the wrong trade at this
  // depth"). That is exactly why ASSETFETCH buffers the whole footprint rather
  // than caching it: a cache behind a port that cannot stall is not an
  // optimisation, it is a protocol violation waiting for a miss.
  input  logic               indexfetch_mode_i,
  input  logic [63:0]        af_pool_i [32],
  output logic [31:0]        dbg_af_meshlets_o,
  output logic [31:0]        dbg_af_beats_o,
  output logic [31:0]        dbg_af_denied_o,
  output logic [31:0]        dbg_af_refused_o,
  // EVERY CYCLE A CONSUMER WAITED ON A BUFFER STILL FILLING. The block's own
  // header names this as the counter that will say whether double buffering is
  // worth its ~2.4 KB -- it is single-buffered on purpose, and this number is
  // the evidence that decides. It was tied off when ASSETFETCH was first
  // composed, which is the "block whose evidence ports are dangling" pattern
  // this bench criticises elsewhere in its own comments.
  output logic [31:0]        dbg_af_stall_o,
  output logic               dbg_vd_have_o,
  output logic [31:0]        dbg_vd_vertices_o,
  output logic [31:0]        dbg_vd_format_bad_o,
  output logic               dbg_vd_refused_o,
  input  logic               project_mode_i,
  input  logic               proj_cfg_we_i,
  input  logic               proj_cfg_view_i,
  input  logic [ 4:0]        proj_cfg_addr_i,
  input  logic [31:0]        proj_cfg_data_i,
  input  logic signed [31:0] proj_ax_i, proj_ay_i, proj_az_i,
  input  logic signed [31:0] proj_bx_i, proj_by_i, proj_bz_i,
  input  logic signed [31:0] proj_cx_i, proj_cy_i, proj_cz_i,
  output logic               dbg_proj_ready_o,
  output logic signed [20:0] dbg_proj_ax_o,
  output logic signed [20:0] dbg_proj_ay_o,
  output logic [30:0]        dbg_proj_w_o,
  output logic [2:0]         dbg_proj_behind_o,
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
      .v_valid_i((render_tri_valid_i & depth_mode_i) |
                 (project_mode_i & pj_tri_ready)),
      .v_ready_o(dq_v_ready),
      // PROJECT's w, not the bench's, once PROJECT is in the path. Its
      // header is explicit that DEPTHQUANT must take clip.w and not the
      // reciprocal: the quotient has already lost the precision the depth
      // law needs.
      .v_w_i(project_mode_i ? {9'd0, pj_w_r[0]} : depth_w_i),
      .v_behind_i(project_mode_i ? pj_behind_r[0] : 1'b0),
      .v_profile_i(depth_profile_i),
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

  // ---- D22 step 6: GEOM.MESHFETCH, and the three interfaces the bench plays --
  logic              mf_j_ready, mf_r_valid;
  zhao_guard_req_t   mf_guard_req;
  zhao_guard_rsp_t   mf_guard_rsp;
  logic              mf_beat_valid, mf_beat_last;
  logic [63:0]       mf_beat_data;
  logic              mf_cull_tick, mf_cull_ready, mf_cull_valid, mf_cull_reject;
  logic [1:0]        mf_cull_active, mf_cull_vis;
  logic [31:0]       mf_r_voff, mf_r_ioff;
  logic [7:0]        mf_r_vcount, mf_r_tcount, mf_r_flags;
  logic [15:0]       mf_r_material, mf_r_instance;
  logic [1:0]        mf_r_vis;
  /* verilator lint_off UNUSEDSIGNAL */
  logic signed [31:0] mf_cull_cx, mf_cull_cy, mf_cull_cz, mf_cull_radius;
  logic [31:0] mf_considered, mf_culled;
  /* verilator lint_on UNUSEDSIGNAL */
  logic [31:0] mf_fetched, mf_denied;
  logic [31:0] mf_refused [7];

  logic signed [31:0] mf_xform [12];
  always_comb begin
    for (int k = 0; k < 12; k++) mf_xform[k] = 32'sd0;
    mf_xform[0] = 32'sd65536;   // identity 3x4, fx16
    mf_xform[5] = 32'sd65536;
    mf_xform[10] = 32'sd65536;
  end

  // THE PLAYED GUARD. Grant on the cycle the request is valid.
  //
  // The beats may only start AFTER the grant. Feeding them from cycle zero is
  // the mistake the unit bench already paid for, and its comment is worth
  // repeating because the symptom is so misleading: every bound read (0,0,0)
  // r=0, "which looks like broken arithmetic and is actually a testbench that
  // answered out of order".
  logic       mf_granted_r, mf_sent_r;
  logic [3:0] mf_beat_r;

  always_comb begin
    mf_guard_rsp = '0;
    if (mf_guard_req.valid && !mf_granted_r) begin
      mf_guard_rsp.ready = 1'b1;
      mf_guard_rsp.ok    = 1'b1;
    end
  end

  assign mf_beat_valid = mf_granted_r && (mf_beat_r < 4'd8);
  assign mf_beat_last  = mf_granted_r && (mf_beat_r == 4'd7);
  assign mf_beat_data  = mf_desc_i[mf_beat_r[2:0]];

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      mf_granted_r <= 1'b0;
      mf_beat_r    <= 4'd0;
      mf_sent_r    <= 1'b0;
    end else if (!render_tri_valid_i) begin
      mf_granted_r <= 1'b0;
      mf_beat_r    <= 4'd0;
      mf_sent_r    <= 1'b0;
    end else begin
      if (render_tri_valid_i && meshfetch_mode_i && mf_j_ready && !mf_sent_r)
        mf_sent_r <= 1'b1;
      if (mf_guard_req.valid && !mf_granted_r) mf_granted_r <= 1'b1;
      if (mf_beat_valid && mf_beat_r < 4'd8)   mf_beat_r <= mf_beat_r + 4'd1;
    end
  end

  // THE PLAYED CULL. Always ready, answers VISIBLE. Culling has its own unit
  // evidence; answering "reject" here would make step 6 a test of the cull
  // path with the descriptor path silently unexercised.
  // THE VERDICT COMES AFTER THE TICK, NOT WITH IT.
  //
  // `cull_tick_o` is asserted in S_CULL and the block then moves to S_WAIT to
  // await `cull_valid_i`. Driving valid FROM the tick makes it high only while
  // the block is asking and low by the time it is listening -- so it parks in
  // S_WAIT forever. And it does so during `render_offer`, before any sampling
  // window opens, which is why the trace read "cull ticks 0" while the
  // descriptor had been fetched and not refused: the tick had already come and
  // gone.
  //
  // The verdict is therefore LATCHED once the tick is seen and held.
  logic mf_cull_seen_r;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n)                     mf_cull_seen_r <= 1'b0;
    else if (!render_tri_valid_i)   mf_cull_seen_r <= 1'b0;
    else if (mf_cull_tick)          mf_cull_seen_r <= 1'b1;
  end

  assign mf_cull_ready  = 1'b1;
  assign mf_cull_valid  = mf_cull_seen_r;
  assign mf_cull_vis    = 2'b01;
  assign mf_cull_reject = 1'b0;

  zhao_geom_meshfetch u_meshfetch (
      .clk(gpu_clk), .rst_n(rst_n),
      .j_valid_i(render_tri_valid_i & meshfetch_mode_i & ~mf_sent_r),
      .j_ready_o(mf_j_ready),
      .j_instance_id_i(16'h1234), .j_desc_addr_i(27'h40),
      .j_format_i(8'd1), .j_generation_i(16'd1), .j_active_mask_i(2'b01),
      .j_xform_i(mf_xform), .j_client_i(zhao_client_e'(0)),
      .guard_req_o(mf_guard_req), .guard_rsp_i(mf_guard_rsp),
      .beat_valid_i(mf_beat_valid), .beat_data_i(mf_beat_data),
      .beat_last_i(mf_beat_last), .crc_ok_i(mf_crc_ok_i),
      .cull_tick_o(mf_cull_tick), .cull_active_o(mf_cull_active),
      .cull_cx_o(mf_cull_cx), .cull_cy_o(mf_cull_cy), .cull_cz_o(mf_cull_cz),
      .cull_radius_o(mf_cull_radius),
      .cull_ready_i(mf_cull_ready), .cull_valid_i(mf_cull_valid),
      .cull_vis_i(mf_cull_vis), .cull_reject_i(mf_cull_reject),
      .r_valid_o(mf_r_valid), .r_ready_i(1'b1),
      .r_instance_id_o(mf_r_instance), .r_visible_mask_o(mf_r_vis),
      .r_vertex_offset_o(mf_r_voff), .r_index_offset_o(mf_r_ioff),
      .r_vertex_count_o(mf_r_vcount), .r_triangle_count_o(mf_r_tcount),
      .r_material_id_o(mf_r_material), .r_flags_o(mf_r_flags),
      // The evidence ports. Connected rather than left dangling: a block whose
      // counters are unconnected still elaborates, and the missing-pin warning
      // is the only thing that says the trace nobody is reading was never
      // wired.
      .meshlets_considered_o(mf_considered), .culled_all_cameras_o(mf_culled),
      .descriptors_fetched_o(mf_fetched), .guard_denied_o(mf_denied),
      .refused_o(mf_refused));

  // The meshlet is LATCHED, for the same reason the descriptor was in step 5:
  // ASSEMBLE walks it over many cycles and it must hold still.
  logic [7:0]  mf_vc_r, mf_tc_r;
  logic [15:0] mf_mat_r;
  logic [1:0]  mf_vis_r;
  logic        mf_have_r;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      mf_vc_r <= 8'd0; mf_tc_r <= 8'd0; mf_mat_r <= 16'd0; mf_vis_r <= 2'd0;
      mf_have_r <= 1'b0;
    end else if (!render_tri_valid_i) begin
      mf_have_r <= 1'b0;
    end else if (mf_r_valid && !mf_have_r) begin
      mf_vc_r   <= mf_r_vcount;
      mf_tc_r   <= mf_r_tcount;
      mf_mat_r  <= mf_r_material;
      mf_vis_r  <= mf_r_vis;
      mf_have_r <= 1'b1;
    end
  end

  assign dbg_mf_valid_o    = mf_have_r;
  assign dbg_mf_vcount_o   = mf_vc_r;
  assign dbg_mf_tcount_o   = mf_tc_r;
  assign dbg_mf_material_o = mf_mat_r;
  assign dbg_mf_vis_o      = mf_vis_r;
  assign dbg_mf_greq_o     = mf_guard_req.valid;
  assign dbg_mf_granted_o  = mf_granted_r;
  assign dbg_mf_beat_o     = mf_beat_r;
  assign dbg_mf_cull_tick_o= mf_cull_seen_r;
  assign dbg_mf_fetched_o  = mf_fetched;
  assign dbg_mf_denied_o   = mf_denied;
  assign dbg_mf_refused0_o = mf_refused[0] | mf_refused[1] | mf_refused[2] |
                             mf_refused[3] | mf_refused[4] | mf_refused[5] |
                             mf_refused[6];

  // ---- D22 step 5: GEOM.ASSEMBLE --------------------------------------------
  logic        asm_m_ready, asm_ix_req, asm_t_valid, asm_t_last;
  logic [8:0]  asm_ix_index;
  logic [15:0] asm_v0, asm_v1, asm_v2;
  /* verilator lint_off UNUSEDSIGNAL */
  logic [15:0] asm_material, asm_src;
  logic [31:0] asm_raster, asm_meshlets, asm_ref_lim, asm_ref_idx;
  /* verilator lint_on UNUSEDSIGNAL */

  // The index responder. ASSEMBLE asks for triplet `asm_ix_index` and this
  // answers from the flat stream the bench supplied -- combinationally, and
  // always valid, so a missing answer can never be mistaken for a walk that
  // stopped. A responder with its own back-pressure would be a second thing
  // under test.
  logic [7:0] asm_ix_a, asm_ix_b, asm_ix_c;
  logic       asm_ix_valid;
  always_comb begin
    if (indexfetch_mode_i) begin
      // TREAD 9: the triplet comes out of the fetched footprint.
      asm_ix_a     = af_ix_a;
      asm_ix_b     = af_ix_b;
      asm_ix_c     = af_ix_c;
      asm_ix_valid = af_ix_valid;
    end else begin
      asm_ix_a = 8'd0; asm_ix_b = 8'd0; asm_ix_c = 8'd0;
      asm_ix_valid = 1'b1;
      for (int unsigned k = 0; k < 4; k++)
        if (asm_ix_index == 9'(k)) begin
          asm_ix_a = asm_index_stream_i[k*24 +: 8];
          asm_ix_b = asm_index_stream_i[k*24 + 8 +: 8];
          asm_ix_c = asm_index_stream_i[k*24 + 16 +: 8];
        end
    end
  end

  zhao_geom_assemble #(
      .MAX_VERTICES(64), .MAX_TRIANGLES(126), .VIDW(16), .SRCW(16)
  ) u_assemble (
      .clk(gpu_clk), .rst_n(rst_n),
      // ONE MESHLET PER OFFER. `render_tri_valid_i` is a level that the bench
      // holds for the whole offer window, so driving m_valid_i from it
      // directly re-submits the same meshlet every cycle it is accepted --
      // measured as `triangles = 15` for a one-triangle meshlet. The counter
      // caught it; the framebuffer could not, because every re-run produced
      // the identical triangle.
      // In meshfetch mode the meshlet is MESHFETCH's, and ASSEMBLE may not be
      // offered it until the descriptor has actually been read and validated.
      .m_valid_i(render_tri_valid_i & assemble_mode_i & ~asm_sent_r
                 & (~meshfetch_mode_i | mf_have_r)),
      .m_ready_o(asm_m_ready),
      .m_vertex_offset_i(16'd0),
      .m_vertex_count_i(meshfetch_mode_i ? mf_vc_r : asm_vertex_count_i),
      .m_triangle_count_i(meshfetch_mode_i ? mf_tc_r : asm_triangle_count_i),
      .m_material_id_i(meshfetch_mode_i ? mf_mat_r : 16'd1),
      .m_raster_state_i(32'd0),
      .m_src_id_i(render_src_id_i),
      .ix_req_o(asm_ix_req), .ix_index_o(asm_ix_index),
      // TREAD 9: valid follows the SOURCE. The bench responder is always
      // valid when asked, so this was tied to the request; the fetcher has a
      // real valid and tying it high would turn a missed answer into a
      // silently wrong triplet.
      .ix_valid_i(indexfetch_mode_i ? asm_ix_valid : asm_ix_req),
      .ix_a_i(asm_ix_a), .ix_b_i(asm_ix_b),
      .ix_c_i(asm_ix_c),
      .t_valid_o(asm_t_valid), .t_ready_i(1'b1),
      .t_v0_o(asm_v0), .t_v1_o(asm_v1), .t_v2_o(asm_v2),
      .t_material_o(asm_material), .t_raster_o(asm_raster),
      .t_src_id_o(asm_src), .t_last_o(asm_t_last),
      .meshlets_o(asm_meshlets), .triangles_o(dbg_asm_triangles_o),
      .refused_limits_o(asm_ref_lim), .refused_index_o(asm_ref_idx));

  // The descriptor is LATCHED. ASSEMBLE emits one triangle per handshake and
  // the vertex IDs must hold still while PROJECT walks the three of them --
  // a combinational path here would change the triangle underneath the
  // collector, which is the class of fault that looks like a projection bug.
  logic [15:0] asm_v_r [3];
  logic        asm_have_r;
  logic        asm_sent_r;   // the meshlet has been handed over once
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      asm_v_r[0] <= 16'd0; asm_v_r[1] <= 16'd0; asm_v_r[2] <= 16'd0;
      asm_have_r <= 1'b0;
      asm_sent_r <= 1'b0;
    end else if (!render_tri_valid_i) begin
      asm_have_r <= 1'b0;
      asm_sent_r <= 1'b0;
    end else begin
      // THE ONE-SHOT MUST MIRROR THE ACTUAL HANDSHAKE, not a weaker condition.
      //
      // This read `render_tri_valid_i && assemble_mode_i && asm_m_ready`, which
      // omits the meshfetch gate that `m_valid_i` carries. So in step-6 mode it
      // fired while ASSEMBLE was merely READY and the meshlet did not exist
      // yet -- then latched `asm_sent_r`, which gates `m_valid_i` off forever.
      // ASSEMBLE never received anything, and the trace read
      // `asm 0 | proj 0 | clip 0 | setup 0` with the descriptor correctly
      // fetched, validated and culled one block upstream.
      //
      // A one-shot whose set condition is broader than the event it is
      // recording will always fire early. Same expression, both places.
      if (render_tri_valid_i && assemble_mode_i && asm_m_ready && !asm_sent_r
          && (~meshfetch_mode_i | mf_have_r))
        asm_sent_r <= 1'b1;
      if (asm_t_valid && !asm_have_r) begin
        asm_v_r[0] <= asm_v0;
        asm_v_r[1] <= asm_v1;
        asm_v_r[2] <= asm_v2;
        asm_have_r <= 1'b1;
      end
    end
  end

  assign dbg_asm_valid_o = asm_have_r;
  assign dbg_asm_v0_o    = asm_v_r[0];
  assign dbg_asm_v1_o    = asm_v_r[1];
  assign dbg_asm_v2_o    = asm_v_r[2];

  // ---- D22 step 4: GEOM.PROJECT and its three-vertex collector --------------
  logic               pj_v_valid, pj_v_ready, pj_out_valid;
  logic signed [31:0] pj_vx, pj_vy, pj_vz;
  logic signed [20:0] pj_out_x, pj_out_y;
  logic [30:0]        pj_out_w;
  logic               pj_out_behind;
  /* verilator lint_off UNUSEDSIGNAL */
  logic signed [31:0] pj_out_d;
  logic [15:0]        pj_out_src;
  logic [31:0]        pj_transformed;
  /* verilator lint_on UNUSEDSIGNAL */

  // The collector. THREE vertices in, one triangle out.
  //
  // `pj_idx_r` is which vertex is being pushed; `pj_got_r` counts how many
  // have come back. The triangle is offered to CLIP only when all three have
  // landed -- offering it early would hand CLIP one new vertex and two stale
  // ones, which is a wrong triangle that still draws.
  logic [1:0]         pj_idx_r, pj_got_r;
  logic signed [20:0] pj_x_r [3];
  logic signed [20:0] pj_y_r [3];
  logic [30:0]        pj_w_r [3];
  logic [2:0]         pj_behind_r;
  logic               pj_tri_ready;

  // ==========================================================================
  // TREAD 8: GEOM.ASSETFETCH over a bench-played pool
  // ==========================================================================
  // THE PLAYED POOL. GEOM.ASSETFETCH requests ONE 64-BYTE LINE AT A TIME --
  // S_REQ, then eight beats ended by `beat_last_i`, then the next line, and a
  // phase switch from the index run to the vertex run in between. So the player
  // grants PER REQUEST and serves exactly eight beats from the line the guard
  // request names; a single grant streaming the whole pool would feed the
  // index phase everything and the vertex phase would never start.
  //
  // The address is pool-relative here because the bench IS the pool: line
  // `addr >> 6`, beat `addr[5:3] + n`.
  zhao_guard_req_t af_guard_req;
  zhao_guard_rsp_t af_guard_rsp;
  logic            af_serving_r;
  logic [2:0]      af_beat_r;
  logic [4:0]      af_line_r;
  logic            af_m_ready, af_s_valid;
  logic            af_v_valid, af_v_ready;
  logic [255:0]    af_v_bytes;
  logic            af_ix_valid;
  logic [7:0]      af_ix_a, af_ix_b, af_ix_c;
  logic            af_sent_r;

  logic af_beat_valid, af_beat_last;
  logic [63:0] af_beat_data;
  assign af_beat_valid = af_serving_r;
  assign af_beat_last  = af_serving_r && (af_beat_r == 3'd7);
  assign af_beat_data  = af_pool_i[{af_line_r[1:0], af_beat_r}];

  always_comb begin
    af_guard_rsp = '0;
    if (af_guard_req.valid && !af_serving_r) begin
      af_guard_rsp.ready = 1'b1;
      af_guard_rsp.ok    = 1'b1;
    end
  end

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      af_serving_r <= 1'b0;
      af_beat_r    <= 3'd0;
      af_line_r    <= 5'd0;
      af_sent_r    <= 1'b0;
    end else if (!render_tri_valid_i) begin
      af_serving_r <= 1'b0;
      af_beat_r    <= 3'd0;
      af_line_r    <= 5'd0;
      af_sent_r    <= 1'b0;
    end else begin
      if (assetfetch_mode_i && af_m_ready && !af_sent_r) af_sent_r <= 1'b1;
      if (af_guard_req.valid && !af_serving_r) begin
        af_serving_r <= 1'b1;
        af_beat_r    <= 3'd0;
        af_line_r    <= af_guard_req.addr[10:6];
      end else if (af_serving_r) begin
        if (af_beat_r == 3'd7) af_serving_r <= 1'b0;
        af_beat_r <= af_beat_r + 3'd1;
      end
    end
  end

  zhao_geom_assetfetch #(.SRCW(16)) u_assetfetch (
      .clk(gpu_clk), .rst_n(rst_n),
      .m_valid_i(render_tri_valid_i & assetfetch_mode_i & ~af_sent_r
                 & (~meshfetch_mode_i | mf_have_r)),
      .m_ready_o(af_m_ready),
      // The footprint is MESHFETCH's own answer when it is in the path, and the
      // bench's fixed offsets otherwise. Pool-relative bytes.
      .m_vertex_offset_i(32'd0),
      .m_index_offset_i(32'd128),
      .m_vertex_count_i(meshfetch_mode_i ? mf_vc_r : 8'd4),
      .m_triangle_count_i(meshfetch_mode_i ? mf_tc_r : 8'd1),
      .m_src_id_i(16'd0), .m_client_i(zhao_client_e'(0)),
      .guard_req_o(af_guard_req), .guard_rsp_i(af_guard_rsp),
      .beat_valid_i(af_beat_valid), .beat_data_i(af_beat_data),
      .beat_last_i(af_beat_last),
      .s_valid_o(af_s_valid), .s_ready_i(1'b1),
      .s_vertex_count_o(), .s_triangle_count_o(), .s_src_id_o(),
      .release_i(1'b0),
      // TREAD 9: the index port is wired. `ix_req_i` is gated on the mode so
      // tread 8's behaviour is unchanged when it is off.
      .ix_req_i(indexfetch_mode_i & asm_ix_req), .ix_index_i(asm_ix_index),
      .ix_valid_o(af_ix_valid),
      .ix_a_o(af_ix_a), .ix_b_o(af_ix_b), .ix_c_o(af_ix_c),
      .v_valid_o(af_v_valid), .v_ready_i(af_v_ready),
      .v_bytes_o(af_v_bytes), .v_src_id_o(),
      .meshlets_fetched_o(dbg_af_meshlets_o), .beats_read_o(dbg_af_beats_o),
      .guard_denied_o(dbg_af_denied_o),
      .refused_footprint_o(dbg_af_refused_o),
      .prefetch_stall_o(dbg_af_stall_o));

  // ==========================================================================
  // TREAD 7: the four records, decoded into the table PROJECT reads
  // ==========================================================================
  // One record at a time through the leaf decoder, latched by arrival order.
  // `vd_have_r` gates the drawing path so no vertex is pushed from a
  // half-filled table -- the failure that would otherwise look like a
  // projection bug.
  logic [2:0]         vd_wp_r;      // records pushed, 0..4
  logic [2:0]         vd_rp_r;      // records decoded, 0..4
  logic               vd_have_r;
  logic signed [31:0] vd_x_r [4];
  logic signed [31:0] vd_y_r [4];
  logic signed [31:0] vd_z_r [4];

  logic               vd_v_valid, vd_v_ready, vd_d_valid;
  logic signed [31:0] vd_d_x, vd_d_y, vd_d_z;
  logic               vd_d_refused;

  // THE RECORD SEAM. In tread-8 mode the records arrive from GEOM.ASSETFETCH
  // and the bench's `vd_rec_i` is not read at all; otherwise the bench feeds
  // them directly, which is tread 7.
  assign vd_v_valid = assetfetch_mode_i
                    ? af_v_valid
                    : (vdecode_mode_i && render_tri_valid_i && (vd_wp_r < 3'd4));
  assign af_v_ready = assetfetch_mode_i && vd_v_ready;
  // OBSERVATION IS NOT THE GATE. The gate must clear when the triangle goes
  // away, or the next one would draw from a stale table -- but a test reading
  // it after the frame drains then sees 0 and calls a working decode a failure,
  // which is exactly what happened on this tread's first run. So the reported
  // flag is STICKY: did this table ever fill, cleared only by reset.
  logic vd_have_seen_r;
  assign dbg_vd_have_o = vd_have_seen_r;

  zhao_geom_vdecode #(.SRCW(16)) u_vdecode (
      .clk(gpu_clk), .rst_n(rst_n),
      .v_valid_i(vd_v_valid), .v_ready_o(vd_v_ready),
      .v_bytes_i(assetfetch_mode_i ? af_v_bytes : vd_rec_i[vd_wp_r[1:0]]),
      .v_format_i(3'd0), .v_src_id_i({13'd0, vd_wp_r}),
      .d_valid_o(vd_d_valid), .d_ready_i(1'b1),
      .d_x_o(vd_d_x), .d_y_o(vd_d_y), .d_z_o(vd_d_z),
      .d_nx_o(), .d_ny_o(), .d_nz_o(),
      .d_w0_o(), .d_rigid_o(), .d_u_o(), .d_v_o(),
      .d_bone0_o(), .d_bone1_o(), .d_src_id_o(),
      .d_refused_o(vd_d_refused),
      .d_reserved_nz_o(), .d_w0_illegal_o(), .d_format_bad_o(),
      .vertices_o(dbg_vd_vertices_o), .reserved_nz_o(), .w0_illegal_o(),
      .format_bad_o(dbg_vd_format_bad_o));

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      vd_wp_r <= 3'd0;
      vd_rp_r <= 3'd0;
      vd_have_r <= 1'b0;
      vd_have_seen_r <= 1'b0;
      dbg_vd_refused_o <= 1'b0;
    end else if (!render_tri_valid_i) begin
      vd_wp_r <= 3'd0;
      vd_rp_r <= 3'd0;
      vd_have_r <= 1'b0;
    end else begin
      if (vd_v_valid && vd_v_ready) vd_wp_r <= vd_wp_r + 3'd1;
      if (vd_d_valid) begin
        vd_x_r[vd_rp_r[1:0]] <= vd_d_x;
        vd_y_r[vd_rp_r[1:0]] <= vd_d_y;
        vd_z_r[vd_rp_r[1:0]] <= vd_d_z;
        vd_rp_r <= vd_rp_r + 3'd1;
        if (vd_rp_r == 3'd3) begin
          vd_have_r      <= 1'b1;
          vd_have_seen_r <= 1'b1;
        end
        if (vd_d_refused) dbg_vd_refused_o <= 1'b1;
      end
    end
  end

  // In ASSEMBLE mode the vertex pushed is the one GEOM.ASSEMBLE named, looked
  // up in the bench's table -- or, in VDECODE mode, in the table this shell
  // decoded for itself. Otherwise it is the bench's own A/B/C.
  logic [1:0] pj_sel;
  always_comb begin
    pj_sel = pj_idx_r;
    if (assemble_mode_i) begin
      // The ID is a table index here; a real machine would range-check it
      // against the meshlet's vertex_count, which is GEOM.VDECODE's business.
      case (pj_idx_r)
        2'd0:    pj_sel = asm_v_r[0][1:0];
        2'd1:    pj_sel = asm_v_r[1][1:0];
        default: pj_sel = asm_v_r[2][1:0];
      endcase
      pj_vx = vdecode_mode_i ? vd_x_r[pj_sel] : asm_vtx_x_i[pj_sel];
      pj_vy = vdecode_mode_i ? vd_y_r[pj_sel] : asm_vtx_y_i[pj_sel];
      pj_vz = vdecode_mode_i ? vd_z_r[pj_sel] : asm_vtx_z_i[pj_sel];
    end else begin
      case (pj_idx_r)
        2'd0:    begin pj_vx = proj_ax_i; pj_vy = proj_ay_i; pj_vz = proj_az_i; end
        2'd1:    begin pj_vx = proj_bx_i; pj_vy = proj_by_i; pj_vz = proj_bz_i; end
        default: begin pj_vx = proj_cx_i; pj_vy = proj_cy_i; pj_vz = proj_cz_i; end
      endcase
    end
  end

  // Push while the bench is offering a triangle and fewer than three vertices
  // have been sent.
  assign pj_v_valid = render_tri_valid_i & project_mode_i & (pj_idx_r < 2'd3)
                    & (~assemble_mode_i | asm_have_r)
                    & (~vdecode_mode_i | vd_have_r);

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      pj_idx_r    <= 2'd0;
      pj_got_r    <= 2'd0;
      pj_behind_r <= 3'd0;
      for (int k = 0; k < 3; k++) begin
        pj_x_r[k] <= 21'sd0;
        pj_y_r[k] <= 21'sd0;
        pj_w_r[k] <= 31'd0;
      end
    end else begin
      if (!render_tri_valid_i) begin
        pj_idx_r <= 2'd0;
        pj_got_r <= 2'd0;
      end else begin
        if (pj_v_valid && pj_v_ready && pj_idx_r < 2'd3)
          pj_idx_r <= pj_idx_r + 2'd1;
        if (pj_out_valid && pj_got_r < 2'd3) begin
          pj_x_r[pj_got_r]      <= pj_out_x;
          pj_y_r[pj_got_r]      <= pj_out_y;
          pj_w_r[pj_got_r]      <= pj_out_w;
          pj_behind_r[pj_got_r] <= pj_out_behind;
          pj_got_r              <= pj_got_r + 2'd1;
        end
      end
    end
  end

  assign pj_tri_ready       = (pj_got_r == 2'd3);
  assign dbg_proj_ready_o   = pj_tri_ready;
  assign dbg_proj_ax_o      = pj_x_r[0];
  assign dbg_proj_ay_o      = pj_y_r[0];
  assign dbg_proj_w_o       = pj_w_r[0];
  assign dbg_proj_behind_o  = pj_behind_r;

  zhao_geom_project u_project (
      .clk(gpu_clk), .rst_n(rst_n),
      .cfg_we_i(proj_cfg_we_i), .cfg_view_i(proj_cfg_view_i),
      .cfg_addr_i(proj_cfg_addr_i), .cfg_data_i(proj_cfg_data_i),
      .v_valid_i(pj_v_valid), .v_ready_o(pj_v_ready),
      .vx_i(pj_vx), .vy_i(pj_vy), .vz_i(pj_vz),
      .view_i(1'b0), .src_id_i(render_src_id_i),
      .out_valid_o(pj_out_valid), .out_ready_i(1'b1),
      .out_x_o(pj_out_x), .out_y_o(pj_out_y), .out_d_o(pj_out_d),
      .out_w_o(pj_out_w), .out_behind_o(pj_out_behind),
      .out_src_id_o(pj_out_src),
      .vertices_transformed_o(pj_transformed));

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
      // In project mode the triangle is offered only once all three vertices
      // have come back from PROJECT.
      .tri_valid_i(project_mode_i ? pj_tri_ready
                                  : (render_tri_valid_i & clip_mode_i)),
      .tri_ready_o(cl_tri_ready),
      .tri_ax_i(project_mode_i ? pj_x_r[0] : render_ax_i),
      .tri_ay_i(project_mode_i ? pj_y_r[0] : render_ay_i),
      .tri_bx_i(project_mode_i ? pj_x_r[1] : render_bx_i),
      .tri_by_i(project_mode_i ? pj_y_r[1] : render_by_i),
      .tri_cx_i(project_mode_i ? pj_x_r[2] : render_cx_i),
      .tri_cy_i(project_mode_i ? pj_y_r[2] : render_cy_i),
      // No vertex is behind: the bench supplies screen-space vertices that
      // GEOM.PROJECT would already have accepted. A w <= 0 verdict is step 4's
      // concern, and asserting it here would exercise a rejection path this
      // step is not moving.
      // The behind flags are PROJECT's verdict in project mode. Tying them to
      // zero there would hide exactly the case CLIP exists to reject.
      .tri_behind_i(project_mode_i ? pj_behind_r : 3'b000),
      .tri_src_id_i(render_src_id_i),
      .tri_attr_a_i('0), .tri_attr_b_i('0), .tri_attr_c_i('0),
      // The scissor is the render grid in whole pixels; sixteen pixels per
      // tile is the shell's own tile size, so this is the same rectangle the
      // bench derives its scan box from -- which is why the two agree.
      .vp_x0_i(12'd0), .vp_y0_i(12'd0),
      // grid tiles x 16 pixels, sized to the port rather than assembled
      // from a guessed concatenation -- the first version added up to 14
      // bits for a 12-bit pin.
      .vp_w_i(12'(render_grid_w_i) << 4),
      .vp_h_i(12'(render_grid_h_i) << 4),
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
  wire               c_valid = (clip_mode_i || project_mode_i)
                             ? cl_out_valid
                             : (render_tri_valid_i & setup_mode_i);
  wire signed [20:0] c_ax    = (clip_mode_i || project_mode_i) ? cl_ax : render_ax_i;
  wire signed [20:0] c_ay    = (clip_mode_i || project_mode_i) ? cl_ay : render_ay_i;
  wire signed [20:0] c_bx    = (clip_mode_i || project_mode_i) ? cl_bx : render_bx_i;
  wire signed [20:0] c_by    = (clip_mode_i || project_mode_i) ? cl_by : render_by_i;
  wire signed [20:0] c_cx    = (clip_mode_i || project_mode_i) ? cl_cx : render_cx_i;
  wire signed [20:0] c_cy    = (clip_mode_i || project_mode_i) ? cl_cy : render_cy_i;
  wire signed [47:0] c_area2 = (clip_mode_i || project_mode_i) ? cl_area2 : setup_area2_i;
  wire signed [11:0] c_min_x = (clip_mode_i || project_mode_i) ? cl_min_x : render_min_x_i;
  wire signed [11:0] c_max_x = (clip_mode_i || project_mode_i) ? cl_max_x : render_max_x_i;
  wire signed [11:0] c_min_y = (clip_mode_i || project_mode_i) ? cl_min_y : render_min_y_i;
  wire signed [11:0] c_max_y = (clip_mode_i || project_mode_i) ? cl_max_y : render_max_y_i;

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
  assign render_tri_ready_o = project_mode_i ? (pj_tri_ready & cl_tri_ready)
                            : clip_mode_i    ? cl_tri_ready
                            : setup_mode_i   ? su_tri_ready
                                             : shell_tri_ready;
  assign dbg_su_tri_ready_o    = su_tri_ready;
  assign dbg_su_out_valid_o    = su_out_valid;
  assign dbg_shell_tri_ready_o = shell_tri_ready;

  wire               m_tri_valid = (setup_mode_i || clip_mode_i || project_mode_i)
                                 ? su_out_valid : render_tri_valid_i;
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
