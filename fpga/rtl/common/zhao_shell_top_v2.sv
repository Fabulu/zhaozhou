// zhao_shell_top_v2.sv -- the Packet-H sibling shell.
//
// NOT A RENAME OF `zhao_shell_top.sv`, and not generated from it either. This
// file was SEEDED from it once, at its protected SHA-256
// 00fdd238...5450783, so that the eighteen instances Packet H does not touch
// were carried across byte-for-byte rather than retyped -- a swapped port name
// in `u_scanout` is exactly the kind of damage a careful hand does. From that
// point it is an ordinary hand-maintained file.
//
// IT IS DELIBERATELY NOT REGENERATED. A generator keyed to the V1 shell would
// be the stale-generated-file trap wearing its best clothes: the V2 shell
// diverges from the V1 BY DESIGN, so anything that rebuilds it from the V1 is
// wrong the moment this packet does its actual work. What keeps the untouched
// regions honest instead is a differential test, which is a check rather than
// a rewrite.
//
// WHAT CHANGES, per reports/PACKET-H-DRIVER-CONTRACT-20260917.md:
//
//   * `zhao_geom_bin_pipe`  -> `zhao_geom_bin_pipe_v2`   (63 -> 166 ports)
//   * `zhao_video_slotmgr`  -> `zhao_video_slotmgr_v2`   (22 -> 74 ports)
//   * the `lseq` sequencer  -> `zhao_video_blit_lease_v2`
//   * added: `zhao_renderer_lease_v2`, `zhao_video_terminal_adapter_v2`,
//     `zhao_video_ready_bridge_v2`
//
// and 59 inputs across the two swapped blocks need a driver. Twelve come from
// the organs and are proven by `tests/shell/zhao_shell_v2_lease_path.sv`; the
// rest are the contract's sections 3.1 to 3.6.
//
// THE OLD SHELL REMAINS BYTE-IDENTICAL AND REMAINS SELECTED. This sibling is
// registered `excluded:not-yet-adopted`: a candidate being measured, not a
// replacement.
//
// ===========================================================================
// Below this line, the seeded V1 text and its original header.
// ===========================================================================
// zhao_shell_top.sv — the Phase-2 CONSOLE SHELL (plan W2.7): the assembled
// Zhaozhou machine as ONE running composition — CMD front end (scheduler +
// DMA), MEM (guards + arbiter + SDRAM controller), VIDEO (mode + scanout +
// scaler + framectl), INPUT (snapshot + rumble), AUDIO (FIFO) and DEBUG
// (counters + displayed-stream CRC) — wired together with the cross-domain
// and cross-block glue this file owns. `Zhaozhou.sv` stays the framework
// glue stub; this is the Verilator integration top (the tb wrapper
// tests/shell/tb_zhao_shell.sv adds the behavioural SDRAM model).
//
// Law (in citation order):
//   spec/video_rules.md   — raster/mode-latch/swap/repeat/displayed-CRC law
//   spec/memory_rules.md  — guard region law, arbiter D3, bridge bursts,
//                           FRAME_RING/pixel-arena (harness = HPS, D10)
//   spec/counters.md      — §3 snapshot timing law (providers present their
//                           latched shadows ONE cycle after the tick), §5
//                           Phase-2 owner table (which block feeds which id)
//   spec/input_rules.md   — PadFrame latch law, rumble frame gating
//   spec/audio_rules.md   — FIFO D4 law
//
// ---------------------------------------------------------------------------
// GLUE THIS FILE OWNS (each a real seam the block wave never composed):
//
//  1. SDRAM WRITE-DATA QUEUE — the guard/arbiter request path carries no
//     data lane; CMD.DMA streams ceil(len/8) x 64-bit beats per accepted
//     write request (guard_wvalid_o, the corrected W2.7 seam) and this file
//     converts them to the controller's 16-bit wr_beat pace. Blit DMA is
//     the ONLY Phase-2 writer, so the queue is strictly ordered; sticky
//     tripwires (shell_err_wfifo_o) catch over/underflow instead of
//     trusting the occupancy argument.
//  2. SDRAM READ-BEAT PACKER — rdata 16-bit words -> 64-bit beats for the
//     scanout fetch (4 words/beat, little-endian ascending: beat byte i =
//     VRAM byte addr+i, the same mapping the write queue uses, so canvas
//     bytes round-trip exactly). Scanout is the only Phase-2 reader;
//     shell_err_route_o trips if any other client's burst appears.
//  3. RECORD FRAMER — the DMA's verified packet byte stream -> the
//     scheduler's {opcode, w0..w3} record port (w0..w3 = record bytes
//     [16,32), the payload dwords; ZhCmdHeader is 16 bytes). A small
//     record QUEUE decouples presentation from consumption: the scheduler
//     backpressures records while a blit dispatch is pending, and the blit
//     can only be accepted after the SAME packet's stream fully drains —
//     without the queue that is a composition DEADLOCK (found composing
//     W2.6's verified halves; neither block is wrong in isolation). A
//     packet may carry at most FRAMER_Q-1 records after its DebugFrameBlit;
//     shell_err_framer_o trips (sticky) if a packet violates that instead
//     of wedging silently.
//  4. SLOT-READY PENDING REGISTERS (vid domain) — DebugFrameBlit completion
//     (gpu) -> FRAMECTL's slot_ready level; cleared exactly when FRAMECTL
//     issues swap_req for that slot, which closes the re-commit race after
//     the vswap decision (set wins over a simultaneous clear — that pairing
//     means a NEW completion raced the swap of the SAME slot, impossible in
//     the alternating-slot cadence and harmless if it ever happens: the
//     slot re-displays its own fresher content).
//  5. FRAME-COMPLETION CORRELATOR — the scheduler's frame_complete needs
//     the RING slot whose packet produced the displayed frame; this file
//     correlates {the RUN slot, its blit's dst FB slot, gpu_complete_slot,
//     !repeated} at the gpu tick.
//  6. MODE CDC (gpu -> vid) — the scheduler's mode register (changes only
//     at the tick) crossed with a 2FF + 2-cycle stability filter before a
//     mode_we pulse; the filter removes the multi-bit skew hazard
//     (STORM->DUO flips two bits) entirely.
//  7. DISPLAYED-FRAME CRC HANDOFF (vid -> gpu, ONCE PER FRAME) — DEBUG.CRC
//     runs in vid_clk and consumes the post-scaler pixel stream natively
//     (one RGB565 pixel per vid cycle = two bytes, low byte first, §3 LE
//     law). expect_bytes is zhao_displayed_bytes(mode) — NOT
//     zhao_canvas_bytes: for Duo those differ (245,760 displayed vs 196,608
//     stored) and the canvas value here would be the documented silent-Duo
//     bug. Only the FINALISED 32-bit CRC crosses to gpu, on a toggle with
//     the value held stable beside it. Until 2026-08-22 this was a per-pixel
//     re-timing that RELIED ON THE FROZEN SIM PHASE (vid_clk = gpu_clk/2,
//     coincident posedges — plan R1) and produced the two `vid_clk ->
//     gpu_clk` hold violations measured under HIGH PERFORMANCE effort; the
//     owner ruled it be moved rather than re-timed (docs/OWNER_DOCKET.md).
//  8. COUNTER PROVIDER ADAPTERS — spec/counters.md §5 owner table: the
//     scheduler's 3 channels (ids 0/1/2) + AUDIO.FIFO (31) are native
//     zhao_counter_snap_t providers; vram_bytes (28, summed over clients),
//     hps_ddr_bytes (29, summed), scanout_starvation_cycles (30, vid-
//     domain value quiescent through vblank — tripwire shell_err_cdc_o),
//     input_sequence_gaps (35) and rumble_frames_dropped (36) are adapted
//     here with the §3 timing law (valid pulses ONE cycle after the tick).
//     CMD.DMA's snap channels are deliberately NOT wired: its ids 1/2
//     duplicate the scheduler's (§5 names the scheduler/decoder as owner)
//     and id 29's owner is MEM.HPS.BRIDGE.
//  9. RUMBLE EDGE CONVERTER — the scheduler's dispatch register is a LEVEL
//     held until the tick; INPUT.RUMBLE counts every command pulse as a
//     replace ("dropped" law), so the level is converted to one pulse per
//     new dispatch (rise or payload change).
// 10. BLIT PACER — even after the FB-slot bank split removed the row
//     thrash, free write interleave leaves the serial scanout fetch ~2%
//     short of Duo line rate (accumulating into a 2-of-4-line limp); the
//     blit client paces itself into scanout's quiet windows instead.
//
// Conservative SystemVerilog subset (charter §2); lint-clean -Wall
// (lint_shell_top CTest). The behavioural SDRAM model is NOT here — it is
// testbench-only (D2) and lives in the tb wrapper.

module zhao_shell_top_v2
  import zhao_pkg::*, zhao_abi_pkg::*, zhao_fb_tuple_pkg::*;
#(
  parameter int unsigned FRAMER_Q = 8,    // record-queue depth (glue 3)
  parameter int unsigned WFIFO_W  = 64,   // write-data queue, 16-bit words
  // ---- THE RENDER BINNER'S REFERENCE CAPACITY (GIANTREFS, ruling R7) -----
  // R7 guarantees a giant of 32,768 TILE REFERENCES that is "never silently
  // truncated", and the owner's standing directive explicitly withholds any
  // authority to shrink it. Until 2026-09-26 this instantiation overrode
  // exactly ONE parameter of `zhao_geom_bin_pipe_v2`, so the composed binner
  // ran the leaf's DEFAULTS -- CHUNKS=256 x CHUNK_REFS=4 = 1,024 references
  // per frame -- and `zhao_geom_arena` is a bump allocator handed back WHOLE
  // at `frame_begin_i`, so nothing recycles inside a frame.
  //
  // REFPUSH measured what the ruled workloads need, at its own commit, with
  // `tools/render/count_bin_load.cpp` against the shipped `zref::Binner` --
  // the same binning law this block implements:
  //     giant near camera, 126 tris   25,704 refs   25.1x the old arena
  //     256 creatures, no LOD         30,609 refs   29.9x
  //     creature army, 200 x 96       23,912 refs   23.4x
  //     one terrain patch 32x32        4,080 refs    4.0x
  //     sky backdrop, 2 triangles        396 refs    0.4x
  // So a frame holding a near-camera giant lost ~96% of its tile references
  // to the binner's wall. The guarantee was not "not yet built"; it was
  // BREACHED, in the shipped composition, every frame.
  //
  // 8,192 x 4 = 32,768 is R7's number EXACTLY, chosen for that reason and not
  // sized to a workload we happen to test. It is a knob: raising it costs only
  // ref_ram, next_ram and two pointer fields of tile_ram, and the giant is
  // TRIANGLE-CHEAP and REFERENCE-EXPENSIVE (126 triangles, 25,704 references),
  // so TRI_CAP -- which carries the 1,160-bit-per-triangle Packet-D metadata
  // bank -- does not have to move with it. Scaling TRI_CAP to answer the
  // giant's question is the confident impossibility REFPUSH caught itself
  // making; the ARMY needs that and this device cannot pay for it.
  parameter int unsigned RENDER_CHUNKS     = 8192,
  parameter int unsigned RENDER_CHUNK_W    = 13,   // $clog2(RENDER_CHUNKS)
  parameter int unsigned RENDER_CHUNK_REFS = 4,
  // THE TERRAIN.BUILD SOCKET's HPS clients (owner ruling R4). Each one is a
  // client of the shell's ONE `zhao_hps_arbiter_n`, at indices 2.. -- BELOW
  // CMD.DMA (0) and DEBUG.FRAMEBLIT (1), which is `spec/memory_rules.md` 5d's
  // "best-effort / background" class said in the arbiter's own vocabulary.
  // A composer that needs a second background reader raises this; it does not
  // build a second socket.
  parameter int unsigned BUILD_HPS_N = 1,
  parameter int unsigned BUILD_WQ_W  = 64, // slot-6 write-data queue, 16-bit words
  // Slot 3's write-data queue, in 16-bit words. GEOM.PARAMBUF's largest
  // record is a 64-byte chunk = 32 words, so 64 words holds two in flight
  // while the third is being offered. Sized like BUILD_WQ_W and for the same
  // reason: the room gate below is EXACT, so a queue too small does not
  // corrupt anything, it throttles -- which is a throughput question and not
  // a correctness one.
  parameter int unsigned GEOM_WQ_W   = 64
) (
  // ---- clocks + reset (harness-driven, frozen ratios: vid = gpu/2,
  // ---- audio = gpu/4, fixed phase — plan R1) -----------------------------
  input  logic gpu_clk,
  input  logic vid_clk,
  input  logic audio_clk,
  input  logic rst_n,

  // ---- PACKET-H: THE V3 PROGRAMMING CHANNEL -----------------------------
  // Twenty inputs. The historical shell has a command scheduler and an HPS
  // bridge and NO V3 programming channel at all, so these are not a rename
  // of anything -- they are the binding/palette/page path arriving at the
  // shell boundary for the first time. A command-stream decoder would be a
  // second design with its own ABI and tests that this packet's gate does
  // not ask for, and one inserted later sits BEHIND these ports and changes
  // nothing the V2 blocks see.
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

  // ---- PACKET-H: attribute carriage, ENGINE1 share, clear, sheet --------
  input  logic [46:0]  tri_area2_i,
  input  logic [239:0] tri_invw_plane_i,
  input  logic [239:0] tri_u_over_w_plane_i,
  input  logic [239:0] tri_v_over_w_plane_i,
  // THE GOURAUD PLANES (owner decision R234 D1, 2026-09-21). GEOM.ATTRPACK's
  // lanes 3..5. They pass straight through to `zhao_geom_bin_pipe_v2`, which
  // concatenates them above v/w into the now 1,877-bit Packet-D metadata.
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
  // ---- SURFACE.SHEET's RESPONSE, into TEXTURE.AUX.V2 (TERRAINAUX 2026-09-25)
  // These six were TIED TO ZERO in this file -- "TIE: page-generation
  // residency is its own clause and its own packet; no producer exists in this
  // shell yet" -- while the REQUEST half left the shell, the core AND the
  // board as a dangling top-level output group, and `zhao_surface_sheet`, the
  // store that answers it, sat composed inside `zhao_console_core` the whole
  // time. The producer did exist; it was one module up and one arbiter short.
  //
  // The tie was not harmless bookkeeping: with `pg_valid_i` low forever, any
  // fragment whose material declared AUX would hold its credit in
  // `zhao_texture_aux_pipe_v2` and never retire -- which is exactly why the
  // core's flat request pinned `aux_required` to zero and
  // `zhao_geom_binner_v2` computed `meta_aux_bad_c` to drop such a job. Three
  // separate refusals, all of them downstream of this tie.
  input  logic         pg_valid_i,
  output logic         pg_ready_o,
  input  logic [1:0]   pg_op_i,
  input  logic [1:0]   pg_status_i,
  input  logic [7:0]   pg_tag_i,
  input  logic [7:0]   pg_strength_i,
  input  logic [15:0]  pg_src_id_i,

  // ---- PACKET-H: the video-domain barrier and echo ----------------------
  // `lease_open` is produced by zhao_video_ready_bridge_v2 and the two
  // `barrier_done` levels by zhao_fb_ready_cdc_v2, so the reset-epoch
  // barrier is self-driven and nothing outside declares it complete.
  input  logic        blank_cmd_i,
  input  logic        scanout_ack_i,
  input  logic        frame_swap_valid_i,
  input  logic        frame_swap_slot_i,
  output logic        blank_ack_o,
  output logic        blank_active_o,
  output logic        lease_open_o,
  output logic [1:0]  frame_slot_ready_o,

  // ---- PACKET-H: lifecycle evidence -------------------------------------
  // The gate asks for exact owner and hierarchy census; these are how a
  // reader gets it without a waveform.
  output logic [31:0] v2_requests_accepted_o,
  output logic [31:0] v2_responses_accepted_o,
  output logic [31:0] v2_leases_granted_o,
  output logic [31:0] v2_leases_refused_o,
  output logic [31:0] v2_faults_latched_o,
  output logic [31:0] v2_publications_o,
  output logic [31:0] v2_releases_o,
  output logic [31:0] v2_ready_events_o,
  output logic [31:0] v2_swaps_o,
  output logic [31:0] v2_contentions_o,
  output logic [31:0] v2_clear_handshakes_o,
  output logic [31:0] v2_frames_admitted_o,
  output logic [31:0] v2_blit_leases_acquired_o,
  output logic [31:0] v2_blit_leases_refused_o,

  // ---- FRAME_RING view (harness = HPS, D10; memory_rules.md 4.1) ---------
  input  logic [1:0]  hps_state_i [0:2],
  input  logic [31:0] hps_byte_len_i [0:2],
  output logic        ring_wr_valid_o,
  output logic [1:0]  ring_wr_slot_o,
  output logic [1:0]  ring_wr_state_o,
  input  logic        ring_wr_ready_i,

  // ---- HPS bridge, harness side (memory_rules.md 3) ----------------------
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

  // ---- raw decoded pad state (input_rules.md 1/4) ------------------------
  input  logic [3:0]  pad_present_i,
  input  logic [31:0] pad_buttons_i [0:3],
  input  logic [15:0] pad_lx_i [0:3],
  input  logic [15:0] pad_ly_i [0:3],
  input  logic [15:0] pad_rx_i [0:3],
  input  logic [15:0] pad_ry_i [0:3],

  // ---- audio: ring-read client seam (pairs in) + PCM out -----------------
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

  // ---- displayed pixel stream (vid domain, post-scaler) ------------------
  output logic        px_valid_o,
  output logic [15:0] px_rgb_o,
  output logic [9:0]  px_x_o,
  output logic [7:0]  px_y_o,
  output logic        px_hsync_o,
  output logic        px_vsync_o,
  output logic        px_hblank_o,
  output logic        px_vblank_o,
  output logic        scaler_violation_o,

  // ---- DEBUG.CRC (gpu domain): the displayed-stream CRC ------------------
  output logic [31:0] crc_frame_o,
  output logic        crc_valid_o,
  output logic [31:0] crc_bytes_o,
  output logic        crc_size_err_o,

  // ---- frame boundary observability --------------------------------------
  output logic        gpu_tick_o,
  output logic [31:0] gpu_tick_frame_id_o,
  output logic        gpu_tick_repeated_o,
  output logic [0:0]  gpu_complete_slot_o,
  output logic [63:0] deadline_faults_o,     // FRAMECTL (vid)
  output logic [63:0] frame_cycles_o,        // FRAMECTL (vid)

  // ---- CMD observability --------------------------------------------------
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

  // ---- INPUT observability ------------------------------------------------
  output logic [639:0] pad_frame_flat_o,
  output logic [15:0]  pad_sequence_o [0:3],
  output logic [63:0]  input_gaps_o,
  output logic [7:0]   rumble_duty_o [0:3],
  output logic [3:0]   rumble_active_o,
  output logic [3:0]   rumble_pwm_o,
  output logic [63:0]  rumble_drops_o,

  // ---- DEBUG.COUNTERS read window ----------------------------------------
  input  logic        cnt_snap_ready_i,
  output logic        cnt_snap_valid_o,
  output logic [15:0] cnt_snap_id_o,
  output logic [63:0] cnt_snap_value_o,
  output logic        cnt_window_open_o,
  output logic        cnt_cat_violation_o,

  // ---- MEM observability + shell integrity tripwires ---------------------
  output logic [31:0] guard_violations_o,    // both guards, summed
  output logic [63:0] starvation_o,
  output logic        init_done_o,
  output logic [31:0] refresh_stalls_o,
  output logic [31:0] bank_conflicts_o,
  output logic [31:0] scanout_preempted_o,
  output logic [31:0] hps_err_count_o,
  output logic        shell_err_wfifo_o,     // write queue over/underflow
  output logic        shell_err_route_o,     // burst from an impossible client
  output logic        shell_err_cdc_o,       // starvation sample moved at tick
  output logic        shell_err_framer_o,    // record queue overflow (glue 3)

  // ---- RENDER: the geometry front door ----------------------------------
  // The console's first render path. GEOM.BINNER -> RASTER.TILE_PIPE ->
  // RASTER.FBWRITE -> MEM.GUARD -> the arbiter ENGINE0 port, which zhao_pkg has
  // always called a "reserved guaranteed slot" and which was tied to zero until
  // now.
  //
  // The triangle port sits at the SHELL edge because CMD.SCHEDULER does not
  // feed it yet. That is provisional and says so: when the command front end
  // grows a draw path these become internal and nothing else here changes.
  //
  // What it draws is FLAT-shaded. zhao_raster_tile_pipe carries one colour,
  // alpha, depth and texel across a triangle because interpolating them is
  // GEOM.SETUP work and GEOM.SETUP has no attribute input yet. This is the
  // path, not the picture.
  input  logic        render_frame_begin_i,
  input  logic        render_frame_end_i,
  input  logic [5:0]  render_grid_w_i,
  input  logic [5:0]  render_grid_h_i,

  input  logic               render_tri_valid_i,
  output logic               render_tri_ready_o,
  // ---- D22 TREAD 10: the geometry memory clients -----------------------------
  // The last thing the bench still PLAYED was memory itself. Every earlier
  // tread took something the bench supplied and gave it to a composed block;
  // GEOM.MESHFETCH and GEOM.ASSETFETCH still had their guard grants answered
  // and their beats fabricated by hand, so the whole staircase rested on a
  // memory that granted immediately and answered in one cycle.
  //
  // These two ports put those fetchers behind the REAL MEM.GUARD and
  // VRAM.ARBITER that this shell already instantiates, on the arbiter's two
  // previously unused client slots. The bench keeps the fetchers -- relocating
  // them into production is a separate concern and is recorded as such -- but
  // it stops inventing the answers.
  //
  // Contention is the point. Everything measured in treads 6 through 9 assumed
  // a memory that never says no, and `prefetch_stall_o` was connected before
  // this tread precisely so its uncontended reading (27) exists to compare
  // against.
  input  var zhao_guard_req_t geom_guard_req_i,
  output var zhao_guard_rsp_t geom_guard_rsp_o,
  // ...and the beats coming back. Until this tread the shell had ONE reader,
  // so read data was wired straight to the scanout packer. Now it has two, and
  // which one a returning word belongs to is a fact that has to be tracked
  // rather than assumed.
  output var logic            geom_beat_valid_o,
  output var logic [63:0]     geom_beat_data_o,
  output var logic            geom_beat_last_o,
  // THE WRITE CHANNEL, opened by the owner's completion ruling of 2026-09-22,
  // item 4. Slot 3 has been READ-ONLY since D22 tread 10 and the comment below
  // it still says the geometry fetchers "READ the Phase-3 asset pool and never
  // write" -- which remains true OF THE ASSET POOL. It is no longer true of
  // the client: GEOM.PARAMBUF's arena producer writes the view the lease
  // names, and RENDER.ASSET_POOL stays read-only to ENGINE1 because
  // `render_asset_ok` still requires `!req.write`, not because nothing here
  // can write.
  //
  // Shaped exactly like slot 6's, because slot 6 already solved this problem:
  // the controller pops a word per `wr_beat` from the moment of grant, so the
  // arbiter must not accept a write whose words are not all in the queue and
  // not already owed to an accepted request. That gate is EXACT, not a race.
  input  var logic [63:0]     geom_wdata_i,
  input  var logic            geom_wvalid_i,
  output var logic            geom_wready_o,
  input  var logic            geom_wlast_i,
  // The VRAM arbiter's credit stream for slot 3, in 16-bit words. The ONLY
  // thing that means "the write landed" -- which is what item 4's "data must
  // not be published before its writes retire" is measured against upstream.
  output var logic [ 7:0]     geom_retire_words_o,
  // GEOM.PARAMBUF's frame lease (item 4). Driven by the arena producer, which
  // latches the view at frame seal and refuses to flip it while anything is in
  // flight. These are PASSED THROUGH to the guard and not interpreted here:
  // the shell is not where the lifetime argument lives.
  input  var logic            geom_pb_lease_i,
  input  var logic            geom_pb_wr_view_i,
  input  var logic            geom_pb_scratch_i,

  // ---- THE TERRAIN.BUILD SOCKET (VRAM slot 6 + HPS clients 2..) ------------
  // Added 2026-09-19 (cmdmem packet) so that MEM.UPLOAD and the terrain
  // paging spine reach the REAL MEM.HPS.BRIDGE and MEM.GUARD instead of each
  // leaving the core as a boundary (core entries I26, I20). ONE socket, and it
  // is shaped for every TERRAIN.BUILD client rather than for one of them:
  //
  //   * a MEM.GUARD client on VRAM slot 6 with BOTH directions -- the write
  //     channel (MEM.UPLOAD, TERRAIN.PAGELOADER, TERRAIN.WRITEBACK) and the
  //     read-beat return (TERRAIN.PAGESTREAM, TERRAIN.HDRREAD). The guard is
  //     the one `zhao_mem_guard` law, so TERRAIN.PAGE_POOL is the window,
  //     plus R32's write-only published-resource region (build_res_*).
  //   * BUILD_HPS_N clients of the shell's HPS arbiter, reading and (since
  //     2026-09-19) writing.
  //
  // Upstream sharing of the one guard port is the composer's, through
  // `zhao_mem_share_n` for readers, exactly as `u_terrain_rdshare` already does.
  //
  // HPS WRITES ARE OFFERED SINCE 2026-09-19 (terrain3, entry I26), and the
  // sentence that stood here explains exactly how. It read: "`zhao_hps_arbiter_n`
  // has no `b_wr_ready_i`, so the bridge's write READY cannot reach a writer ...
  // The first HPS writer must add that ready to the arbiter first." The first
  // HPS writers already existed -- TERRAIN.WRITEBACK's journal and PART.STATE's
  // generation store, both behind the core's terrain arbiter -- and neither
  // needs the ready routed THROUGH an arbiter: the bridge's `wr_ready` is a
  // LEVEL that is high only while the one granted write burst is streaming, so
  // only that burst's owner can see it high while it has data to give. Both
  // writers already gate on exactly that level (`terr_hps_wr_ready_i`). So the
  // socket carries each client's write beats into the arbiter's existing
  // `wr_*_i` and hands the bridge's level back out as `build_hps_wr_ready_o`.
  input  var zhao_guard_req_t build_guard_req_i,
  output var zhao_guard_rsp_t build_guard_rsp_o,
  input  var logic [63:0]     build_wdata_i,
  input  var logic            build_wvalid_i,
  output var logic            build_wready_o,
  input  var logic            build_wlast_i,
  // The VRAM arbiter's credit stream for slot 6, in 16-bit words. The ONLY
  // thing that means "the write landed".
  output var logic [ 7:0]     build_retire_words_o,
  output var logic            build_beat_valid_o,
  output var logic [63:0]     build_beat_data_o,
  output var logic            build_beat_last_o,
  input  var zhao_hps_burst_req_t [BUILD_HPS_N-1:0] build_hps_req_i,
  output var logic                [BUILD_HPS_N-1:0] build_hps_grant_o,
  output var zhao_hps_burst_rsp_t [BUILD_HPS_N-1:0] build_hps_rsp_o,
  // Rule 7's starvation instrument for each socket client: cycles it wanted
  // the bridge and did not have it. A background client is ALLOWED to starve
  // (5d); it is not allowed to starve invisibly.
  output var logic [BUILD_HPS_N-1:0][31:0] build_hps_wait_o,
  // Each socket client's HPS WRITE beats, and the bridge's write-acceptance
  // LEVEL (see the note above: only the granted burst's owner streams).
  input  var logic [BUILD_HPS_N-1:0]       build_hps_wr_valid_i,
  input  var logic [BUILD_HPS_N-1:0][63:0] build_hps_wr_data_i,
  input  var logic [BUILD_HPS_N-1:0]       build_hps_wr_last_i,
  output var logic                         build_hps_wr_ready_o,
  // Owner ruling R32 (provisional, 2026-09-19): the ONE region of
  // RENDER.ASSET_POOL this socket's guard client may WRITE -- the resource
  // region MEM.UPLOAD publishes into the residency directory. The guard admits
  // it only while the whole region lies inside the pool, and only for writes;
  // anything outside it in the pool stays refused. Zero/invalid = no region.
  input  var logic            build_res_valid_i,
  input  var logic [31:0]     build_res_base_i,
  input  var logic [31:0]     build_res_span_i,
  input  logic signed [22:0] render_kx0_i, render_ky0_i,
  input  logic signed [47:0] render_kc0_i,
  input  logic signed [22:0] render_kx1_i, render_ky1_i,
  input  logic signed [47:0] render_kc1_i,
  input  logic signed [22:0] render_kx2_i, render_ky2_i,
  input  logic signed [47:0] render_kc2_i,
  input  logic        [ 2:0] render_tl_i,
  input  logic signed [20:0] render_ax_i, render_ay_i,
  input  logic signed [20:0] render_bx_i, render_by_i,
  input  logic signed [20:0] render_cx_i, render_cy_i,
  input  logic signed [11:0] render_min_x_i, render_max_x_i,
  input  logic signed [11:0] render_min_y_i, render_max_y_i,
  input  logic        [15:0] render_src_id_i,

  input  logic [63:0] render_fill_word_i,
  input  logic [63:0] render_clear_word_i,
  input  logic [31:0] render_state_i,
  input  logic [ 7:0] render_src_a_i,
  input  logic [23:0] render_texel_rgb_i,
  input  logic [ 7:0] render_texel_a_i,
  input  logic [ 7:0] render_texel_idx_i,

  input  logic [26:0] render_fb_base_i,
  input  logic [15:0] render_fb_stride_i,

  // WHO HOLDS THE FRAMEBUFFER-WRITE LEASE THIS FRAME.
  // 0 = DEBUG.FRAMEBLIT, 1 = RASTER.FBWRITE.
  //
  // ONE SIGNAL, BOTH GUARDS. The first version of this wiring hardwired
  // `fb_writer` to 0 inside the blit guard and 1 inside the render guard, so
  // each compared the client against its OWN constant and BOTH writers passed
  // at once -- which is precisely the corruption the lease exists to prevent,
  // reintroduced by the wiring of the block that prevents it. The owner is one
  // value, and both guards are told the same one.
  //
  // Provisional at the shell edge: VIDEO.SLOTMGR already owns one lease at a
  // time with a generation, and this becomes that lease's owner field once
  // CMD.SCHEDULER selects the writer. Until then it is an input so a bench can
  // exercise either writer, and it defaults to the blit at the caller.
  input  logic        fb_writer_i,

  output logic        render_drain_done_o,
  output logic        render_busy_o,
  output logic [31:0] render_pixels_o,
  output logic [31:0] render_bursts_o,
  output logic        render_stream_error_o,
  // The frame transaction. `render_drained_o` is the ONLY signal a frame
  // controller may publish a slot on: it means every word handed to the guard
  // has been RETIRED by the arbiter. `render_busy_o` falls when the last beat
  // is merely accepted, several stages earlier.
  output logic        render_drained_o,
  output logic        render_fatal_o,
  output logic [31:0] render_issued_words_o,
  output logic [31:0] render_retired_words_o,
  output logic        render_overflow_o,
  output logic        render_fragment_error_o,
  // ---- TEXTURE EVIDENCE, promoted 2026-09-20 (entry I49, texmat2) ---------
  // Seven of these were left DANGLING at this instantiation and the eighth was
  // sunk inside the tile pipe, so the composed console had no way whatever to
  // ask whether the texture island had sampled anything. The samples counter
  // is the one that answers it: TMU samples PUBLISHED into a fragment. The
  // other seven say WHY a zero is a zero -- no fragment reached the island at
  // all, versus fragments that reached it and were refused.
  output logic [31:0] render_texture_fragments_o,
  output logic [31:0] render_texture_cache_hits_o,
  output logic [31:0] render_texture_cache_misses_o,
  output logic [31:0] render_texture_palette_lookups_o,
  output logic [31:0] render_texture_plan_accepted_o,
  output logic [31:0] render_texture_dispatch_accepted_o,
  output logic [31:0] render_texture_combine_refused_o,
  output logic [31:0] render_texture_samples_o,

  // ---- POST.GATHER's RESOLVED-FRAGMENT TAP (core entry I17, ruling R195) ---
  // Entry I17 refused POST.GATHER three times, and its own last correction
  // named what was actually in the way on the input side:
  //
  //   "`zhao_shell_top_v2.sv` line 1181 reads
  //      .fb_tag_o(rp_fb_tag_unused), .fb_addr_o(rp_fb_addr_unused),
  //    while fb_valid_o, fb_rgb565_o, fb_x_o, fb_y_o and fb_last_o all leave
  //    on the lines around it. So the tag is DISCARDED INSIDE THE SHELL, not
  //    absent from the machine."
  //
  // TRUE OF THE TILE PIPE, AND NOT TRUE OF THIS EDGE, which is worth saying
  // plainly because the entry's "the repair is ONE 8-BIT PORT" was costed off
  // it. The five neighbours leave `zhao_raster_tile_pipe`; they land on the
  // shell-internal wires `rpx_*` and go to RASTER.FBWRITE. NOTHING resolved
  // leaves this module today. The repair is this SEVEN-PORT GROUP, not one
  // port, and it is still small.
  //
  // WHY A TAP AND NOT A STREAM. POST.GATHER must never backpressure
  // RASTER.RESOLVE (R5) -- it is a side channel, and a side channel with a
  // `ready` is a throughput term. So there is no handshake here: `gth_valid_o`
  // is the ACCEPTED beat (`rpx_valid && rpx_ready`), asserted for exactly one
  // clock per fragment, and the consumer keeps up by construction.
  //
  // `gth_x_o`/`gth_y_o` are SURFACE coordinates and `gth_addr_o` is the same
  // beat's position INSIDE its 16x16 tile, so the tile origin is a four-bit
  // subtract at the far end. That is why no tile-start handshake crosses this
  // edge: a pulse is a second thing that can be one cycle out, and the
  // subtraction cannot be.
  output logic        gth_valid_o,       // an ACCEPTED resolved fragment
  output logic [15:0] gth_rgb565_o,      // the fragment's OWN colour (R195)
  output logic [7:0]  gth_tag_o,         // (channel << 6) | strength, NEVER dithered
  output logic [7:0]  gth_addr_o,        // {row[3:0], col[3:0]} within the tile
  output logic signed [11:0] gth_x_o,    // SURFACE pixel x of this beat
  output logic signed [11:0] gth_y_o,    // SURFACE pixel y of this beat
  output logic        gth_last_o,        // the 256th pixel of this tile

  // ---- POST.COMPOSITE's FRAMEBUFFER LEASE (core entries I15/I16, 2026-09-19)
  // POST.COMPOSITE.md: "an exclusive framebuffer read/write lease after resolve
  // and before publication". The lease is held HERE, by `zhao_post_lease`,
  // because this is where the render lease, RASTER.FBWRITE and the ENGINE0
  // guard live; the compositor itself stays in the core. Across this edge go
  // the compositor's addressless source stream (the back buffer read back in
  // raster order), its composited output (written back through the SAME
  // RASTER.FBWRITE, in place) and its POST.ECHO tap. `render_drained_o` now
  // means the RENDER FRAME -- raster AND post -- has retired.
  input  logic [8:0]  post_frame_w_i,       // the VIEW's size, from the video mode
  input  logic [7:0]  post_frame_h_i,
  input  logic        post_duo_i,           // two views per frame
  // R35/R36: CMD.EXEC's committed look. POST.ECHO captures only when ARMED,
  // and a pass may not start while the look or the grading table is being
  // written (CMD.EXEC's EX_POST).
  input  logic        post_echo_arm_i,
  input  logic        post_look_hold_i,
  output logic        post_pass_start_o,    // POST.COMPOSITE frame_start
  output logic        post_view_o,          // POST.COMPOSITE view_sel
  output logic        post_src_valid_o,
  input  logic        post_src_ready_i,
  output logic [15:0] post_src_rgb_o,
  input  logic        post_out_valid_i,
  output logic        post_out_ready_o,
  input  logic [15:0] post_out_rgb_i,
  input  logic [8:0]  post_out_x_i,
  input  logic [7:0]  post_out_y_i,
  input  logic        post_out_last_i,
  input  logic        post_echo_valid_i,
  input  logic [15:0] post_echo_rgb_i,
  output logic        post_busy_o,
  output logic [31:0] post_passes_o,
  output logic [31:0] post_frames_o,
  output logic        post_fault_o,
  output logic [31:0] post_src_reads_o,
  output logic [31:0] post_src_pixels_o,
  output logic [31:0] post_retire_unowned_o,
  output logic [31:0] post_share_contention_o,
  output logic [31:0] echo_passes_complete_o,
  output logic [31:0] echo_passes_torn_o,
  output logic [31:0] echo_pixels_written_o,
  output logic [31:0] echo_pixels_dropped_o,
  output logic        echo_fault_o,

  // ---- SDR PHY pins (behavioural model in the tb wrapper; D2) ------------
  output logic        phy_cs_n_o,
  output logic        phy_ras_n_o,
  output logic        phy_cas_n_o,
  output logic        phy_we_n_o,
  output logic [12:0] phy_a_o,
  output logic [1:0]  phy_ba_o,
  output logic [15:0] phy_dq_o,
  output logic        phy_dq_oe_o,
  output logic [1:0]  phy_dqm_o,
  input  logic [15:0] phy_dq_i,

  // --------------------------------------------------------------------------
  // CMD.DMA's PACKET STREAM, RE-EXPORTED.  Added 2026-09-19.
  // --------------------------------------------------------------------------
  // These were body wires (`pkt_valid`/`pkt_byte`/`pkt_len`/`pkt_ready`), and
  // being body wires is the whole reason CMD.DECODER could not be composed:
  // the stream it wants is REAL and is already flowing -- CMD.DMA emits it and
  // the smoke bench's played HPS bridge puts genuine packet bytes on it -- but
  // it had no way out of this module, so five completion-register entries
  // (I7, I14, I30, I33 and the FORGE.PRIM jobs) all recorded the same absent
  // command path.  The path was not absent.  It was enclosed.
  //
  // `cmd_pkt_ready_i` IS AN INPUT AND NOT AN ASSUMPTION.  The obvious cheaper
  // shape is to export only the three outputs and let the second consumer snoop
  // the stream, arguing that it never backpressures.  That argument would be
  // load-bearing and unproven, and when it failed it would fail SILENTLY, by
  // dropping command bytes -- the console's command front end is the wrong
  // place to find out.  So the external consumer gets a real veto and the
  // acceptance below is the AND of both consumers.
  //
  // Every pre-existing instantiator ties it high, which preserves this module's
  // behaviour exactly; see the paired-diff bench, which is the check that says
  // so rather than my saying so.
  output logic        cmd_pkt_valid_o,
  output logic [ 7:0] cmd_pkt_byte_o,
  output logic [31:0] cmd_pkt_len_o,
  input  logic        cmd_pkt_ready_i
  // ---- THE BINNER'S SERIALISE PASS LEFT THIS BOUNDARY (ARENACOMPOSE) ------
  // `render_ser_req_i` and seven `render_ser_*` outputs carried a SECOND read
  // walk over `zhao_geom_binner_v2`'s tile lists out to `zhao_geom_chunkser`,
  // which filled `zhao_geom_paramarena`'s chunk region. Console entry I55:
  // that made the on-chip arena the thing that FILLED the external one, so the
  // two were in SERIES and the SDRAM path could never become the sole
  // producer by subtraction. `zhao_geom_arenabin` fills the arena from the
  // post-clip stream in `zhao_console_core` instead. Nothing asks for the
  // pass, so the pass and these ports are REMOVED rather than tied off --
  // a live path left with no consumer is dangling ports, not a capability.
);

  // ==========================================================================
  // VIDEO: mode + scanout + scaler + framectl (the zhao_video_tb wiring,
  // now against the real memory chain)
  // ==========================================================================
  logic        hb_wr_ready;
  logic [31:0] hb_wr_early;
  logic [15:0] vx, vy;
  logic        vhsync, vvsync, vhblank, vvblank;
  logic        frame_start, frame_end, vswap_dec;
  zhao_mode_e  vmode, vmode_next;

  logic        mode_we;
  logic [1:0]  mode_in;

  zhao_px_stream_t px_ser, px_out;

  logic        swap_req;
  logic [0:0]  swap_slot;
  logic        swap_ack;

  logic        frame_tick_vid, frame_repeated_vid;
  logic [31:0] frame_id_vid, deadline_margin_vid;
  zhao_frame_tick_t gpu_tick;
  logic [0:0]  gpu_complete_slot;

  logic [1:0]  slot_ready_pending;   // glue 4 (vid domain)

  zhao_guard_req_t scan_guard_req;
  zhao_guard_rsp_t scan_guard_rsp;
  logic        scan_beat_valid;
  logic [63:0] scan_beat_data;

  zhao_video_mode u_mode (
    .vid_clk     (vid_clk),
    .rst_n       (rst_n),
    .mode_we     (mode_we),
    .mode_in     (mode_in),
    .x           (vx),
    .y           (vy),
    .hsync       (vhsync),
    .vsync       (vvsync),
    .hblank      (vhblank),
    .vblank      (vvblank),
    .frame_start (frame_start),
    .frame_end   (frame_end),
    .vswap_dec   (vswap_dec),
    .mode_out    (vmode),
    .mode_next   (vmode_next)
  );

  zhao_video_scanout u_scanout (
    .gpu_clk     (gpu_clk),
    .vid_clk     (vid_clk),
    .rst_n       (rst_n),
    .guard_req   (scan_guard_req),
    .guard_rsp   (scan_guard_rsp),
    .beat_valid  (scan_beat_valid),
    .beat_data   (scan_beat_data),
    .beat_last   (1'b0),             // conformance-only pin (fetch header)  // TIE: inherited verbatim from zhao_shell_top.sv; a V1 decision this packet carries rather than makes
    .x           (vx),
    .y           (vy),
    .hsync       (vhsync),
    .vsync       (vvsync),
    .hblank      (vhblank),
    .vblank      (vvblank),
    .frame_start (frame_start),
    .vswap_dec   (vswap_dec),
    .mode        (vmode),
    .mode_next   (vmode_next),
    .swap_req    (swap_req),
    .swap_slot   (swap_slot),
    .swap_ack    (swap_ack),
    .px          (px_ser),
    .starvation_cycles (starvation_o)
  );

  zhao_video_scaler u_scaler (
    .vid_clk     (vid_clk),
    .rst_n       (rst_n),
    .in          (px_ser),
    .out         (px_out),
    .out_ready   (1'b1),  // TIE: inherited verbatim from zhao_shell_top.sv; a V1 decision this packet carries rather than makes
    .never_active(scaler_violation_o)
  );

  zhao_video_framectl u_framectl (
    .vid_clk        (vid_clk),
    .rst_n          (rst_n),
    .x              (vx),
    .y              (vy),
    .vblank         (vvblank),
    .vswap_dec      (vswap_dec),
    .frame_start    (frame_start),
    .mode           (vmode),
    .slot_ready     (slot_ready_pending),
    .deadline_cycles(32'd0),          // mode-period default (D8)  // TIE: inherited verbatim from zhao_shell_top.sv; a V1 decision this packet carries rather than makes
    .swap_req       (swap_req),
    .swap_slot      (swap_slot),
    .swap_ack       (swap_ack),
    .frame_repeated (frame_repeated_vid),
    .frame_tick     (frame_tick_vid),
    .frame_id       (frame_id_vid),
    .frame_cycles   (frame_cycles_o),
    .deadline_faults(deadline_faults_o),
    .deadline_margin(deadline_margin_vid),
    .gpu_clk        (gpu_clk),
    .gpu_tick       (gpu_tick),
    .gpu_complete_slot (gpu_complete_slot)
  );

  assign px_valid_o  = px_out.valid;
  assign px_rgb_o    = px_out.rgb565;
  assign px_x_o      = px_out.x;
  assign px_y_o      = px_out.y;
  assign px_hsync_o  = px_out.hsync;
  assign px_vsync_o  = px_out.vsync;
  assign px_hblank_o = px_out.hblank;
  assign px_vblank_o = px_out.vblank;

  assign gpu_tick_o          = gpu_tick.pulse;
  assign gpu_tick_frame_id_o = gpu_tick.frame_id;
  assign gpu_tick_repeated_o = gpu_tick.repeated;
  assign gpu_complete_slot_o = gpu_complete_slot;

  // ==========================================================================
  // CMD: scheduler + DMA
  // ==========================================================================
  logic        fetch_req_valid, fetch_req_ready;
  logic [1:0]  fetch_slot;
  logic [31:0] fetch_addr, fetch_byte_len, fetch_epoch;

  logic        dma_done;
  logic [1:0]  dma_slot;
  logic [7:0]  dma_status;
  logic [31:0] dma_bytes_consumed, dma_cmds_consumed;

  zhao_hps_burst_req_t dma_hps_req;
  zhao_hps_burst_rsp_t dma_hps_rsp;

  // The HPS bridge now has an ARBITER in front of it. Client 1 is tied off in
  // this step: the point of inserting it before DEBUG.FRAMEBLIT exists is to
  // prove it does not disturb the path that already works, so that when the
  // blitter arrives a regression has only one possible cause.
  //
  // It also does real work already, because CMD.DMA and the bridge do not agree
  // on how long a request stays up: CMD.DMA PULSES `hps_req_v` for one cycle,
  // and the arbiter captures it on the edge it chooses an owner rather than
  // re-reading the port a cycle later.
  zhao_hps_burst_req_t arb_hps_req;
  zhao_hps_burst_rsp_t arb_hps_rsp;
  logic                arb_bridge_grant;
  logic                arb_wr_valid, arb_wr_last;
  logic [63:0]         arb_wr_data;
  logic                dma_hps_grant;
  logic                blit_hps_grant;
  zhao_hps_burst_req_t blit_hps_req;
  zhao_hps_burst_rsp_t blit_hps_rsp;
  logic [31:0]         hps_arb_c0_bursts, hps_arb_c1_bursts, hps_arb_c1_wait;

  logic        pkt_valid, pkt_ready;
  logic [7:0]  pkt_byte;
  logic [31:0] pkt_len;

  logic        dpy_blit_valid, blit_req_ready;
  logic [7:0]  dpy_blit_dst, dpy_blit_mode;
  logic [31:0] dpy_blit_src, dpy_blit_len, dpy_blit_crc;
  logic        blit_done;
  logic [7:0]  blit_status;

  logic        dpy_rumble_valid;
  logic [7:0]  dpy_rumble_pad, dpy_rumble_en, dpy_rumble_str;
  logic        dpy_snap_req;

  zhao_mode_e  sched_mode;

  zhao_guard_req_t blit_guard_req;
  zhao_guard_rsp_t blit_guard_rsp;
  logic [63:0] blit_wdata;
  logic        blit_wvalid;
  logic        blit_wready;   // the write queue can take a whole beat
  logic        blit_wlast;

  // ---- the blit path, writer-aware ---------------------------------------
  // DEBUG.FRAMEBLIT is retained; VIDEO.SLOTMGR is not. The V1
  // lease/publish/release nets -- `slot_lease_req/slot/grant/refused`,
  // `slot_ready_gpu`, `slot_state_gpu`, `slot_leases_granted`, and the
  // `swap_gpu_*` toggle pair -- are DELETED rather than sunk. They name an
  // interface this shell no longer speaks, and keeping them wired to an XOR
  // would preserve a shape instead of a signal.
  logic        fb_lease_valid, fb_lease_slot;
  logic [15:0] fb_lease_gen;
  logic        blit_pub_v, blit_pub_slot;
  logic [15:0] blit_pub_gen;
  logic        blit_rel_v, blit_rel_slot;
  logic [15:0] blit_rel_gen;
  logic        slot_displayed_v, slot_displayed_s;
  logic [31:0] slot_stale_events;

  // The request as the shell latched it, held for the whole transaction.
  logic [ 7:0] r_blit_dst, r_blit_mode;
  logic [31:0] r_blit_src, r_blit_len, r_blit_crc;

  // THE PAYLOAD LATCH, WHICH THE DELETED `lseq` FSM ALSO PERFORMED. That
  // sequencer did two jobs: it ordered the lease against the request, and it
  // captured the dispatch payload. `zhao_video_blit_lease_v2` replaces the
  // first and has no business with the second -- it is a lease, not a
  // blitter -- so the capture stays here, on the same accepting edge the
  // leaf uses.
  //
  // Removing this by accident is quiet: `zhao_debug_frameblit` would read
  // whatever `r_blit_*` held from the previous transaction, and a blit with
  // the right lease and the wrong source address writes plausible garbage.
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      r_blit_dst  <= 8'd0;
      r_blit_mode <= 8'd0;
      r_blit_src  <= 32'd0;
      r_blit_len  <= 32'd0;
      r_blit_crc  <= 32'd0;
    end else if (dpy_blit_valid && blit_req_ready) begin
      r_blit_dst  <= dpy_blit_dst;
      r_blit_mode <= dpy_blit_mode;
      r_blit_src  <= dpy_blit_src;
      r_blit_len  <= dpy_blit_len;
      r_blit_crc  <= dpy_blit_crc;
    end
  end
  logic        nb_req_valid;
  logic        nb_req_ready;
  logic        nb_done;
  logic [ 7:0] nb_status;
  logic [31:0] blits_published, blits_rejected;

  zhao_counter_snap_t sched_snap_cycles, sched_snap_faults, sched_snap_cmds;
  zhao_counter_snap_t dma_snap_cmds, dma_snap_bytes, dma_snap_drops;

  // record framer -> scheduler (glue 3)
  logic        rec_valid, rec_ready;
  logic [15:0] rec_opcode;
  logic [31:0] rec_w0, rec_w1, rec_w2, rec_w3;

  // frame completion correlator (glue 5)
  logic        frame_complete;
  logic [1:0]  frame_complete_slot;

  zhao_cmd_scheduler u_sched (
    .clk                  (gpu_clk),
    .rst_n                (rst_n),
    .hps_state_i          (hps_state_i),
    .hps_byte_len_i       (hps_byte_len_i),
    .ring_wr_valid_o      (ring_wr_valid_o),
    .ring_wr_slot_o       (ring_wr_slot_o),
    .ring_wr_state_o      (ring_wr_state_o),
    .ring_wr_ready_i      (ring_wr_ready_i),
    .fetch_req_valid_o    (fetch_req_valid),
    .fetch_req_ready_i    (fetch_req_ready),
    .fetch_slot_o         (fetch_slot),
    .fetch_addr_o         (fetch_addr),
    .fetch_byte_len_o     (fetch_byte_len),
    .fetch_epoch_o        (fetch_epoch),
    .dma_done_i           (dma_done),
    .dma_slot_i           (dma_slot),
    .dma_status_i         (dma_status),
    .rec_valid_i          (rec_valid),
    .rec_ready_o          (rec_ready),
    .rec_opcode_i         (rec_opcode),
    .rec_w0_i             (rec_w0),
    .rec_w1_i             (rec_w1),
    .rec_w2_i             (rec_w2),
    .rec_w3_i             (rec_w3),
    .frame_tick_i         (gpu_tick),
    .frame_complete_i     (frame_complete),
    .frame_complete_slot_i(frame_complete_slot),
    .dpy_blit_valid_o     (dpy_blit_valid),
    .dpy_blit_ready_i     (blit_req_ready),
    .dpy_blit_dst_slot_o  (dpy_blit_dst),
    .dpy_blit_mode_o      (dpy_blit_mode),
    .dpy_blit_src_o       (dpy_blit_src),
    .dpy_blit_len_o       (dpy_blit_len),
    .dpy_blit_crc_o       (dpy_blit_crc),
    .dpy_rumble_valid_o   (dpy_rumble_valid),
    .dpy_rumble_pad_o     (dpy_rumble_pad),
    .dpy_rumble_en_o      (dpy_rumble_en),
    .dpy_rumble_str_o     (dpy_rumble_str),
    .dpy_snap_req_o       (dpy_snap_req),
    .mode_o               (sched_mode),
    .snap_cycles_o        (sched_snap_cycles),
    .snap_faults_o        (sched_snap_faults),
    .snap_cmds_o          (sched_snap_cmds),
    .fence_valid_o        (fence_valid_o),
    .fence_slot_o         (fence_slot_o),
    .fence_ok_o           (fence_ok_o),
    .fence_status_o       (fence_status_o),
    .slot_state_o         (slot_state_o)
  );

  zhao_cmd_dma u_dma (
    .clk                 (gpu_clk),
    .rst_n               (rst_n),
    .fetch_req_valid_i   (fetch_req_valid),
    .fetch_req_ready_o   (fetch_req_ready),
    .fetch_slot_i        (fetch_slot),
    .fetch_addr_i        (fetch_addr),
    .fetch_byte_len_i    (fetch_byte_len),
    .fetch_epoch_i       (fetch_epoch),
    .dma_done_o          (dma_done),
    .dma_slot_o          (dma_slot),
    .dma_status_o        (dma_status),
    .dma_bytes_consumed_o(dma_bytes_consumed),
    .dma_cmds_consumed_o (dma_cmds_consumed),
    .hps_req_o           (dma_hps_req),
    .hps_rsp_i           (dma_hps_rsp),
    .pkt_valid_o         (pkt_valid),
    .pkt_ready_i         (pkt_ready),
    .pkt_byte_o          (pkt_byte),
    .pkt_len_o           (pkt_len),
    // CMD.DMA has no blitter and no MEM.GUARD client any more: step 6
    // deleted the machinery, and with it the 1.97 Mbit whole-canvas staging
    // buffer that was the entire point of the redesign. The blit dispatch is
    // DEBUG.FRAMEBLIT's, and it stages 64 bytes.
    .frame_tick_i        (gpu_tick),
    .snap_cmds_o         (dma_snap_cmds),
    .snap_bytes_o        (dma_snap_bytes),
    .snap_drops_o        (dma_snap_drops)
  );

  assign mode_act_o    = sched_mode;
  assign dma_done_o    = dma_done;
  assign dma_status_o  = dma_status;
  assign blit_done_o   = blit_done;
  assign blit_status_o = blit_status;

  // DMA snap channels intentionally unconsumed (header note, glue 8):
  // spec/counters.md 5 names the scheduler (ids 1/2) and MEM.HPS.BRIDGE
  // (id 29) as the Phase-2 owners of the ids the DMA also latches.
  /* verilator lint_off UNUSEDSIGNAL */
  logic unused_dma;
  assign unused_dma = ^dma_snap_cmds ^ ^dma_snap_bytes ^ ^dma_snap_drops
                    ^ ^dma_bytes_consumed ^ ^dma_cmds_consumed ^ ^dma_slot
                    ^ dpy_snap_req ^ ^frame_id_vid ^ ^deadline_margin_vid
                    ^ frame_repeated_vid ^ frame_tick_vid ^ frame_end
                    ^ starve_busy;
  /* verilator lint_on UNUSEDSIGNAL */

  // ==========================================================================
  // MEM: two guard instances (client field selects the region law), the
  // arbiter, the SDRAM controller, and the HPS bridge
  // ==========================================================================
  // SEVEN arbiter ports since the terrain amendment (ruling T3): index IS the
  // client id, so 5 is the unspent reservation and 6 is TERRAIN.BUILD. Neither
  // is driven here -- TERRAIN.PAGELOADER is not in the shell yet -- and both
  // are tied off below beside slot 4.
  zhao_arb_req_t [6:0] client_req;
  zhao_arb_rsp_t [6:0] client_rsp;
  zhao_arb_req_t       ctrl_req;
  zhao_arb_rsp_t       ctrl_rsp;
  logic                hold_refresh;

  // guard (scanout, read-only both slots)
  zhao_arb_req_t scan_arb_req, blit_arb_req;
  logic       scan_gv, blit_gv;
  logic [31:0] scan_gv_cnt, blit_gv_cnt;
  zhao_guard_req_t scan_gv_req, blit_gv_req;   // trace-only (harness lane)
  logic        ctrl_refresh_pulse;
  // `bridge_req_grant` is gone: the bridge's accept now lands on the arbiter,
  // not here. The lint sink below absorbs the arbiter's client-side grants
  // instead -- DEBUG.FRAMEBLIT will consume its own once it is wired.
  logic        bridge_req_grant;
  assign bridge_req_grant = dma_hps_grant;

  zhao_mem_guard u_guard_scan (
    .clk        (gpu_clk),
    .rst_n      (rst_n),
    .req        (scan_guard_req),
    .rsp        (scan_guard_rsp),
    .map_valid  (1'b0),  // TIE: inherited verbatim from zhao_shell_top.sv; a V1 decision this packet carries rather than makes
    .blit_slot  (1'b0),  // TIE: inherited verbatim from zhao_shell_top.sv; a V1 decision this packet carries rather than makes
    .blit_span  (32'd0),  // TIE: inherited verbatim from zhao_shell_top.sv; a V1 decision this packet carries rather than makes
    // Scanout reads and never writes, so the lease owner cannot reach its
    // verdict. Tied to the blit writer rather than left dangling.
    .fb_writer  (1'b0),  // TIE: inherited verbatim from zhao_shell_top.sv; a V1 decision this packet carries rather than makes
    .res_valid  (1'b0),   // TIE: this client is not MEM.UPLOAD; the R32 resource-write arm names TERRAIN_BUILD alone
    .res_base   (32'd0),  // TIE: this client is not MEM.UPLOAD; the R32 resource-write arm names TERRAIN_BUILD alone
    .res_span   (32'd0),  // TIE: this client is not MEM.UPLOAD; the R32 resource-write arm names TERRAIN_BUILD alone
    // TIE: this guard's client is never ENGINE1, so item 4's PARAMBUF window is
    // shut here in both directions
    .pb_lease_valid   (1'b0),
    .pb_wr_view       (1'b0),
    .pb_scratch_valid (1'b0),
    .arb_req    (scan_arb_req),
    .arb_rsp    (client_rsp[0]),
    .guard_violation     (scan_gv),
    .guard_violations    (scan_gv_cnt),
    .guard_violation_req (scan_gv_req)
  );

  // guard (blit, write-only into the granted window — glue 4 grants it)
  logic        map_valid_q;   // driven from the lease, see GLUE 4 below
  logic [0:0]  map_slot_q;
  logic [31:0] map_span_q;

  zhao_mem_guard u_guard_blit (
    .clk        (gpu_clk),
    .rst_n      (rst_n),
    .req        (blit_guard_req),
    .rsp        (blit_guard_rsp),
    .map_valid  (map_valid_q),
    .blit_slot  (map_slot_q),
    .blit_span  (map_span_q),
    // The lease owner, shared with the render guard below. This guard passes
    // only when the lease names the blit.
    .fb_writer  (fb_writer_i),
    .res_valid  (1'b0),   // TIE: this client is not MEM.UPLOAD; the R32 resource-write arm names TERRAIN_BUILD alone
    .res_base   (32'd0),  // TIE: this client is not MEM.UPLOAD; the R32 resource-write arm names TERRAIN_BUILD alone
    .res_span   (32'd0),  // TIE: this client is not MEM.UPLOAD; the R32 resource-write arm names TERRAIN_BUILD alone
    // TIE: this guard's client is never ENGINE1, so item 4's PARAMBUF window is
    // shut here in both directions
    .pb_lease_valid   (1'b0),
    .pb_wr_view       (1'b0),
    .pb_scratch_valid (1'b0),
    .arb_req    (blit_arb_req),
    .arb_rsp    (client_rsp[1]),
    .guard_violation     (blit_gv),
    .guard_violations    (blit_gv_cnt),
    .guard_violation_req (blit_gv_req)
  );

  // All THREE guards now: scanout, the blit, and the render engine. A render
  // write outside the leased slot is a violation like any other and must not be
  // invisible in the shell's own counter.
  // The geometry guard's violations are ADDED, not left out. A guard whose
  // refusals are not totalled is a guard nobody reads.
  assign guard_violations_o =
      scan_gv_cnt + blit_gv_cnt + render_gv_cnt + geom_gv_cnt + build_gv_cnt;

  // ---- GLUE 10: the blit pacer -------------------------------------------
  // Even with the FB-slot bank split (which removed the single-bank row
  // thrash — zhao_pkg ZHAO_FB_SLOT1_BASE note), free interleaving of blit
  // writes with the SERIAL scanout fetch leaves the fetch ~2% short of Duo
  // line rate: the deficit accumulates until the ping-pong limps (2 of
  // every 4 lines starved, measured). Starved lines re-emit held pixels,
  // which makes the displayed stream un-composable by zref — so the blit
  // client PACES itself (client pacing is lawful; the arbiter's D3 policy
  // is untouched): its arbiter request is offered only once scanout has
  // been quiet for BLIT_PACE_QUIET cycles, batching writes into the
  // line-fetch tail gaps, the Duo border lines and vblank. Measured:
  // starvation exactly zero at every cadence.
  localparam int unsigned BLIT_PACE_QUIET = 8;
  logic [3:0] scan_quiet_cnt;
  logic [7:0] scan_beats_pending;   // beats owed to the scanout fetch
  logic       scan_active;
  assign scan_active = scan_guard_req.valid || (scan_beats_pending != 8'd0);

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      scan_quiet_cnt     <= 4'd0;
      scan_beats_pending <= 8'd0;
    end else begin
      // one 64-B scanout request = 8 beats owed; each packed beat repays 1
      if (scan_guard_req.valid && scan_guard_rsp.ready
          && !scan_guard_rsp.violation) begin
        scan_beats_pending <= scan_beats_pending + 8'd8
                              - (scan_beat_valid ? 8'd1 : 8'd0);
      end else if (scan_beat_valid && (scan_beats_pending != 8'd0)) begin
        scan_beats_pending <= scan_beats_pending - 8'd1;
      end
      if (scan_active) scan_quiet_cnt <= 4'd0;
      else if (scan_quiet_cnt != 4'hF) scan_quiet_cnt <= scan_quiet_cnt + 4'd1;
    end
  end

  logic blit_gate_open;
  assign blit_gate_open = (scan_quiet_cnt >= 4'(BLIT_PACE_QUIET));

  always_comb begin
    client_req[1]       = blit_arb_req;
    client_req[1].valid = blit_arb_req.valid && blit_gate_open;
  end

  assign client_req[0] = scan_arb_req;
  // ---- RENDER: the path, its guard, and the arbiter port ------------------
  // Wired exactly as DEBUG.FRAMEBLIT is: producer -> zhao_mem_guard ->
  // client_req[n]. The guard fb_writer is 1 here and the blit guard is 0, so a
  // frame whose lease names one refuses the other -- one writer per frame,
  // enforced by the block whose contract is that nothing escapes the window.
  zhao_guard_req_t render_guard_req;
  zhao_guard_rsp_t render_guard_rsp;
  logic [63:0]     render_wdata;
  logic            render_wvalid, render_wready, render_wlast;
  zhao_arb_req_t   render_arb_req;
  logic            render_gv;
  logic [31:0]     render_gv_cnt;
  zhao_guard_req_t render_gv_req;

  logic [15:0] rp_fb_src_unused;
  // `rp_fb_tag_unused` and `rp_fb_addr_unused` are GONE. They were entry
  // I17's named obstacle and they are now `rpx_tag`/`rpx_addr`, leaving on
  // the POST.GATHER tap. A name ending in `_unused` that starts being used is
  // worse than either state, so it is renamed in the same edit.
  logic [15:0] rp_crc_idx_unused;
  logic [31:0] rp_crc_unused, rp_ez_unused;
  logic [31:0] rp_jobs_unused, rp_jobstall_unused, rp_stall_unused;
  logic [ 8:0] rp_cov_unused;
  logic        rp_done_unused, rp_degen_unused, rp_busy_unused;
  logic        rp_tok_unused;

  // ---- GEOM.BINNER's INSTRUMENTS, which used to end here (GIANTREFS) ------
  // `rp_refs_unused`, `rp_depth_unused`, `rp_culled_unused` and
  // `rp_arenafull_unused` are GONE, and `binner_arena_used_o`'s empty
  // connection with them. Five of the binner's six instruments were discarded
  // at this instantiation and the sixth, `binner_overflow_o`, left as
  // `render_overflow_o` and was read by nobody -- so the composed console
  // could not say that a frame had lost geometry to the binner's wall, nor by
  // how much. The counters are proven to fire at the LEAF
  // (`geom_binner_v2_cntw_wrap.cpp`, `geom_binner_directed.cpp:437`), which is
  // evidence about a bench, not about this machine.
  //
  // They now have THREE readers, none of them a pass-through:
  //   * `tile_references` and `max_tile_list_depth` are published into
  //     DEBUG.COUNTERS under GEOM.BINNER's OWN catalog ids -- 18 and 19,
  //     declared for this block in `design/blocks.yml` and owned by nothing
  //     else -- and leave the console on `cnt_snap_id_o`/`cnt_snap_value_o`.
  //   * the WALL is a term of this shell's fault aggregation, which is the
  //     directive's own rule: "Overflow remains a whole-frame fault."
  //   * `arena_full`, `arena_used` and `triangles_culled` drive the wall
  //     observer below, whose verdict is what makes the fault term mean
  //     CAPACITY rather than any other cull.
  logic [31:0] v2_bin_refs_w;
  logic [15:0] v2_bin_depth_w;
  logic [31:0] v2_bin_culled_w;
  logic        v2_bin_arena_full_w;
  logic [RENDER_CHUNK_W:0] v2_bin_arena_used_w;
  logic        v2_bin_overflow_w;

  // `render_overflow_o` is still this shell's top port and still carries the
  // wall, unchanged for every existing reader. What changed is that the wall
  // now ALSO has a reader INSIDE the console -- the fault aggregation below --
  // so the port is no longer the only thing standing between the event and
  // nobody at all.
  assign render_overflow_o = v2_bin_overflow_w;

  // ---- THREE INSTRUMENTS THAT STILL HAVE NO CONSOLE READER, AND THE
  //      MEASUREMENT THAT SAYS WHY, so the next packet inherits a number
  //      rather than a shrug (GIANTREFS, 2026-09-26).
  //
  // `binner_triangles_culled_o`, `binner_arena_full_o` and
  // `binner_arena_used_o` are NAMED here rather than discarded, but nothing
  // consumes them yet, and that is a measured refusal, not an oversight.
  //
  // DEBUG.COUNTERS is the console's only counter consumer and its read window
  // is a DENSE bank indexed by catalog id: `u_counters` below is instantiated
  // at CATALOG_IDS = 40, while `design/blocks.yml`'s `counter_catalog` holds
  // 351 entries. A provider whose id is at or above CATALOG_IDS does not
  // merely go unpublished -- `zhao_debug_counters` raises `cat_violation_o` on
  // it, with no fallback. So the only ids this shell can publish are 0..39.
  //
  // GEOM.BINNER's own catalog counters are `tile_references` (18),
  // `max_tile_list_depth` (19) and `geom_binner_triangles_culled` (241). The
  // first two are inside the window and ARE published below, at the ids the
  // catalog gives this block and no other. The third is outside it, and
  // `arena_used`/`arena_full` have no catalog name at all, so they would have
  // to be appended at 351 and beyond.
  //
  // WIDENING THE WINDOW WAS MEASURED, NOT ASSUMED. `-MapOnly` on
  // `zhao_debug_counters`, `-Device 5CSEBA6U23I7`, rows
  // `zhao_debug_counters@giantrefs-cat40` and
  // `zhao_debug_counters@giantrefs-cat353` in
  // `reports/synthesis/zhao_block_fit.json`, both `rtlCleanAtHead: true`:
  //     CATALOG_IDS =  40  ->   2,579 registers,  0 block memory bits
  //     CATALOG_IDS = 353  ->  22,611 registers,  0 block memory bits
  // **+20,032 registers, and ZERO of it in memory.** The bank accepts up to
  // PROV_N scattered writes per cycle at variable addresses, so it cannot
  // infer RAM, and the 0 memory bits at BOTH widths is that argued fact
  // measured instead. On a 41,910-ALM device already near 97% ALM, ~20k extra
  // registers is about a quarter of the part's whole register capacity, spent
  // on telemetry. Refused -- and refused WITHOUT shrinking anything: the
  // instruments stay connected and named, and the real fix is a SPARSE read
  // window in DEBUG.COUNTERS (stream the providers own ids instead of sweeping
  // a dense catalog), which would cost LESS than today at PROV_N=11 and is a
  // change to that block's contract in `spec/counters.md`.
  /* verilator lint_off UNUSEDSIGNAL */
  logic        v2_bin_unread_full;
  logic [31:0] v2_bin_unread_culled;
  logic [RENDER_CHUNK_W:0] v2_bin_unread_used;
  assign v2_bin_unread_full   = v2_bin_arena_full_w;
  assign v2_bin_unread_culled = v2_bin_culled_w;
  assign v2_bin_unread_used   = v2_bin_arena_used_w;
  /* verilator lint_on UNUSEDSIGNAL */

  logic               rpx_valid, rpx_ready, rpx_last;
  logic        [15:0] rpx_rgb565;
  logic        [ 7:0] rpx_tag, rpx_addr;
  logic signed [11:0] rpx_x, rpx_y;

  // ---- the POST.GATHER tap (entry I17, ruling R195) -----------------------
  // THE ACCEPTED BEAT, not the raw valid, and this is the whole subtlety of
  // the tap. POST.GATHER has no `ready` by law (R5), so a beat held across a
  // stall would be accumulated once per stalled clock and one bright fragment
  // would light its cell as though it were many. `rpx_ready` already carries
  // the post-phase interlock (`!post_phase_w && fbw_px_ready`), so gating on
  // it also keeps the gather silent while the compositor owns the lease --
  // which is the property `zhao_post_gather_store`'s single plane rests on
  // and which that block's `rdw_collide_o` measures rather than assumes.
  assign gth_valid_o  = rpx_valid && rpx_ready;
  assign gth_rgb565_o = rpx_rgb565;
  assign gth_tag_o    = rpx_tag;
  assign gth_addr_o   = rpx_addr;
  assign gth_x_o      = rpx_x;
  assign gth_y_o      = rpx_y;
  assign gth_last_o   = rpx_last;

  // ---- THE V2 BIN PIPE --------------------------------------------------
  //
  // 63 ports -> 165, and the ones that matter here are the frame-clear
  // handshake and the V3 programming channel. The clear is why this swap and
  // the slot-manager swap below CANNOT LAND SEPARATELY: zhao_renderer_lease_v2
  // holds `frame_fault_clear_valid_o` until the handshake completes, and the
  // V1 bin pipe has no clear port at all -- so a shell with the new lease and
  // the old pipe never leaves ST_CLEAR and never admits a frame. That is a
  // deadlock, not a tie-off, and it would present as "the sibling renders
  // nothing" with every block innocent.
  //
  // `frame_begin_i` is the ADMITTED FRAME, not the external request. That is
  // the packet's law in one wire: the renderer's work is withheld until the
  // lease is granted and the clear accepted.
  /* verilator lint_off PINCONNECTEMPTY */
  zhao_geom_bin_pipe_v2 #(
    .CHUNKS(RENDER_CHUNKS),
    .CHUNK_W(RENDER_CHUNK_W),
    .CHUNK_REFS(RENDER_CHUNK_REFS)
  ) u_render_bin (
    .clk(gpu_clk), .rst_n(rst_n),
    .frame_begin_i(v2_frame_admit_w), .frame_end_i(render_frame_end_i),
    .grid_w_i(render_grid_w_i), .grid_h_i(render_grid_h_i),
    .frame_clear_word_i(frame_clear_word_i),
    .tri_valid_i(render_tri_valid_i), .tri_ready_o(render_tri_ready_o),
    .tri_kx0_i(render_kx0_i), .tri_ky0_i(render_ky0_i), .tri_kc0_i(render_kc0_i),
    .tri_kx1_i(render_kx1_i), .tri_ky1_i(render_ky1_i), .tri_kc1_i(render_kc1_i),
    .tri_kx2_i(render_kx2_i), .tri_ky2_i(render_ky2_i), .tri_kc2_i(render_kc2_i),
    .tri_tl_i(render_tl_i),
    .tri_ax_i(render_ax_i), .tri_ay_i(render_ay_i),
    .tri_bx_i(render_bx_i), .tri_by_i(render_by_i),
    .tri_cx_i(render_cx_i), .tri_cy_i(render_cy_i),
    .tri_min_x_i(render_min_x_i), .tri_max_x_i(render_max_x_i),
    .tri_min_y_i(render_min_y_i), .tri_max_y_i(render_max_y_i),
    .tri_src_id_i(render_src_id_i),
    .tri_area2_i(tri_area2_i),
    .tri_invw_plane_i(tri_invw_plane_i),
    .tri_u_over_w_plane_i(tri_u_over_w_plane_i),
    .tri_v_over_w_plane_i(tri_v_over_w_plane_i),
    .tri_r_plane_i(tri_r_plane_i),
    .tri_g_plane_i(tri_g_plane_i),
    .tri_b_plane_i(tri_b_plane_i),
    .tri_flat_request_i(tri_flat_request_i),
    .tri_continuation_tail_i(tri_continuation_tail_i),
    .tri_fragment_state_i(tri_fragment_state_i),
    // TIE: MEASURE.TOKENS does not gate the shell render path yet -- the V1
    // shell says the same at its own u_render_bin; when the governor is wired
    // this becomes its grant.
    .tok_req_o(rp_tok_unused), .tok_grant_i(1'b1),
    .frame_fault_clear_valid_i(v2_clear_valid_w),
    .frame_fault_clear_ready_o(v2_clear_ready_w),
    .frame_fault_o(v2_bin_frame_fault_w),
    .lifetime_structural_fault_o(v2_bin_lifetime_fault_w),
    .cfg_valid_i(cfg_valid_i), .cfg_ready_o(cfg_ready_o),
    .cfg_op_i(cfg_op_i), .cfg_page_generation_i(cfg_page_generation_i),
    .cfg_selector_i(cfg_selector_i), .cfg_row_i(cfg_row_i),
    .cfg_crc32_i(cfg_crc32_i),
    .cfg_rsp_valid_o(cfg_rsp_valid_o), .cfg_rsp_ready_i(cfg_rsp_ready_i),
    .cfg_rsp_op_o(cfg_rsp_op_o), .cfg_rsp_status_o(cfg_rsp_status_o),
    .cfg_rsp_page_generation_o(cfg_rsp_page_generation_o),
    .active_page_generation_o(active_page_generation_o),
    .fill_req_valid_o(fill_req_valid_o), .fill_req_ready_i(fill_req_ready_i),
    .fill_req_addr_o(fill_req_addr_o),
    .fill_data_valid_i(fill_data_valid_i), .fill_data_i(fill_data_i),
    .fill_refused_i(fill_refused_i),
    .pal_load_valid_i(pal_load_valid_i), .pal_load_ready_o(pal_load_ready_o),
    .pal_load_op_i(pal_load_op_i), .pal_load_slot_i(pal_load_slot_i),
    .pal_load_gen_i(pal_load_gen_i), .pal_load_idx_i(pal_load_idx_i),
    .pal_load_rgb565_i(pal_load_rgb565_i),
    .pal_load_crc_ok_i(pal_load_crc_ok_i),
    .sheet_req_valid_o(sheet_req_valid_o), .sheet_req_ready_i(sheet_req_ready_i),
    .sheet_req_op_o(sheet_req_op_o), .sheet_req_handle_o(sheet_req_handle_o),
    .sheet_req_texel_o(sheet_req_texel_o),
    .sheet_req_src_id_o(sheet_req_src_id_o),
    // REAL since 2026-09-25 (TERRAINAUX): SURFACE.SHEET's response, straight
    // through from this shell's own port. `zhao_console_core` closes the loop
    // onto `u_surface_sheet` through `u_surface_sheetshare`'s CLIENT C. Nothing
    // is renamed, buffered or reinterpreted here -- the arbitration is in a
    // file with a contract and a test, which is what entry I32 asked for.
    .pg_valid_i(pg_valid_i), .pg_ready_o(pg_ready_o),
    .pg_op_i(pg_op_i), .pg_status_i(pg_status_i),
    .pg_tag_i(pg_tag_i), .pg_strength_i(pg_strength_i), .pg_src_id_i(pg_src_id_i),
    .fb_valid_o(rpx_valid), .fb_ready_i(rpx_ready),
    .fb_rgb565_o(rpx_rgb565),
    // REAL: the effect tag and the in-tile address now LEAVE this shell, on
    // the POST.GATHER tap above. Entry I17's named obstacle, closed.
    .fb_tag_o(rpx_tag), .fb_addr_o(rpx_addr),
    .fb_x_o(rpx_x), .fb_y_o(rpx_y), .fb_last_o(rpx_last),
    .fb_src_id_o(rp_fb_src_unused),
    .tile_crc_o(rp_crc_unused), .tile_crc_index_o(rp_crc_idx_unused),
    .tile_done_o(rp_done_unused), .tile_cov_count_o(rp_cov_unused),
    .tile_degenerate_o(rp_degen_unused),
    .drain_busy_o(rp_busy_unused), .drain_done_o(render_drain_done_o),
    .binner_initialized_o(v2_bin_initialized_w),
    .binner_tile_references_o(v2_bin_refs_w),
    .binner_max_tile_list_depth_o(v2_bin_depth_w),
    .binner_triangles_culled_o(v2_bin_culled_w),
    .binner_overflow_o(v2_bin_overflow_w),
    .binner_arena_full_o(v2_bin_arena_full_w),
    .binner_arena_used_o(v2_bin_arena_used_w),
    .jobs_taken_o(rp_jobs_unused),
    .job_stall_clocks_o(rp_jobstall_unused),
    .quiet_o(v2_bin_quiet_w),
    .raster_abort_o(v2_bin_raster_abort_w),
    .local_attribute_abort_o(v2_bin_attr_abort_w),
    // TIE: the pulse form of `local_fault_count_o`, which is telemetry. The
    // ABORTS beside it are the structural faults and they are wired; a fault
    // pulse that coincides with an abort adds no information the OR does not
    // already carry, and one that does not is a counter the shell has no
    // reader for. Packet K's telemetry pass owns it.
    .local_fault_pulse_o(),
    .local_fault_count_o(), .coordinate_fault_count_o(),
    .range_fault_count_o(), .aux_profile_fault_count_o(),
    .candidate_cancel_count_o(), .local_drop_count_o(),
    .raster_jobs_started_o(), .raster_jobs_sunk_o(),
    .sequence_abort_o(v2_bin_sequence_abort_w),
    .sequence_mismatch_o(v2_bin_sequence_mismatch_w),
    .sequence_drop_count_o(), .admission_sequence_o(),
    .expected_sequence_o(), .returned_sequence_o(),
    .packet_c_cand_fire_o(), .packet_c_fragment_fire_o(),
    .packet_c_drop_fire_o(), .tilestore_references_o(),
    .resolved_tiles_o(), .early_z_rejects_o(rp_ez_unused),
    .early_z_covered_o(), .fragment_covered_o(), .blended_fragments_o(),
    .texture_fragments_o(render_texture_fragments_o),
    .texture_cache_hits_o(render_texture_cache_hits_o),
    .texture_cache_misses_o(render_texture_cache_misses_o),
    .texture_palette_lookups_o(render_texture_palette_lookups_o),
    .texture_plan_accepted_o(render_texture_plan_accepted_o),
    .texture_dispatch_accepted_o(render_texture_dispatch_accepted_o),
    .texture_combine_refused_o(render_texture_combine_refused_o),
    .texture_samples_o(render_texture_samples_o),
    .fragment_error_o(render_fragment_error_o),
    .coverage_hold_valid_o(), .coverage_delivered_mask_o(),
    .start_delivered_mask_o(), .attribute_idle_o(),
    .earlyz_hold_valid_o(), .skid_level_o(),
    .stage_candidate_valid_o(), .stage_candidate_data_o(),
    .stage_fragment_valid_o(), .stage_fragment_addr_o(),
    .stage_fragment_depth_o(), .stage_fragment_state_o(),
    .stage_fragment_src_id_o(), .stage_fragment_texel_rgb_o(),
    .stage_fragment_texel_a_o(), .stage_fragment_texel_idx_o(),
    .stage_fragment_status_o(),
    .texture_quiet_o(), .fragment_idle_o(), .front_bank_o(),
    .bin_mask_o(), .z_floor_o()
  );
  /* verilator lint_on PINCONNECTEMPTY */

  // ---- RASTER.FBWRITE: ONE ENGINE, TWO PHASES (2026-09-19) -----------------
  // The raster's pixels, then POST.COMPOSITE's write-back of the same frame.
  // The two never overlap: post starts only once the raster is quiet, nothing
  // is on its pixel port and every raster word has retired, and the raster
  // cannot restart until a new frame is admitted -- which ends the post phase.
  // So the port is switched, not arbitrated, and the second writer costs a
  // 45-bit mux instead of a second FBWRITE (~300 ALM saved).
  logic               post_phase_w;
  logic               ppx_valid, ppx_ready, ppx_last;
  logic        [15:0] ppx_rgb565;
  logic signed [11:0] ppx_x, ppx_y;
  logic               fbw_px_valid, fbw_px_ready, fbw_px_last;
  logic        [15:0] fbw_px_rgb565;
  logic signed [11:0] fbw_px_x, fbw_px_y;
  assign fbw_px_valid  = post_phase_w ? ppx_valid  : rpx_valid;
  assign fbw_px_rgb565 = post_phase_w ? ppx_rgb565 : rpx_rgb565;
  assign fbw_px_x      = post_phase_w ? ppx_x      : rpx_x;
  assign fbw_px_y      = post_phase_w ? ppx_y      : rpx_y;
  assign fbw_px_last   = post_phase_w ? ppx_last   : rpx_last;
  assign rpx_ready     = !post_phase_w && fbw_px_ready;
  assign ppx_ready     =  post_phase_w && fbw_px_ready;

  // FBWRITE is now requester 0 of ENGINE0's share inside the post lease; its
  // guard port, write channel and retirement credits go THROUGH the lease.
  zhao_guard_req_t fbw_guard_req;
  zhao_guard_rsp_t fbw_guard_rsp;
  logic [63:0]     fbw_guard_wdata;
  logic            fbw_guard_wvalid, fbw_guard_wready, fbw_guard_wlast;
  logic [ 7:0]     fbw_retire_words;
  logic [31:0]     fbw_pixels_w, fbw_bursts_w;
  logic            fbw_drained_w;

  zhao_raster_fbwrite u_render_fbw (
    .clk(gpu_clk), .rst_n(rst_n),
    .fb_base_i(render_fb_base_i), .fb_stride_i(render_fb_stride_i),
    .px_valid_i(fbw_px_valid), .px_ready_o(fbw_px_ready),
    .px_rgb565_i(fbw_px_rgb565), .px_x_i(fbw_px_x), .px_y_i(fbw_px_y),
    .px_last_i(fbw_px_last),
    .frame_end_i(render_frame_end_i),
    // Retirement is the arbiter credit stream for the ENGINE0 client, exactly
    // as DEBUG.FRAMEBLIT takes client_rsp[1].credits -- now ATTRIBUTED by the
    // post lease, because three requesters share ENGINE0 and each must see
    // only the words its own requests retired.
    .retire_words_i(fbw_retire_words),
    .guard_req_o(fbw_guard_req), .guard_rsp_i(fbw_guard_rsp),
    .guard_wdata_o(fbw_guard_wdata), .guard_wvalid_o(fbw_guard_wvalid),
    .guard_wready_i(fbw_guard_wready), .guard_wlast_o(fbw_guard_wlast),
    .pixels_written_o(fbw_pixels_w),
    .bursts_issued_o(fbw_bursts_w),
    .stall_clocks_o(rp_stall_unused),
    .stream_error_o(render_stream_error_o),
    .issued_words_o(render_issued_words_o),
    .retired_words_o(render_retired_words_o),
    .drained_o(fbw_drained_w),
    .fatal_error_o(render_fatal_o),
    .busy_o(render_busy_o)
  );

  // `render_pixels_o` / `render_bursts_o` are the RASTER's, and stay so: the
  // write-back of the same frame is counted by POST.COMPOSITE's own
  // `output_writes_o`. FBWRITE's window counters run on through the post phase,
  // so the raster's totals are frozen at the phase change.
  logic [31:0] raster_pixels_q, raster_bursts_q;
  logic        post_phase_q;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      raster_pixels_q <= 32'd0;
      raster_bursts_q <= 32'd0;
      post_phase_q    <= 1'b0;
    end else begin
      post_phase_q <= post_phase_w;
      if (post_phase_w && !post_phase_q) begin
        raster_pixels_q <= fbw_pixels_w;
        raster_bursts_q <= fbw_bursts_w;
      end
    end
  end
  assign render_pixels_o = post_phase_w ? raster_pixels_q : fbw_pixels_w;
  assign render_bursts_o = post_phase_w ? raster_bursts_q : fbw_bursts_w;

  // THE FRAME TRANSACTION NOW INCLUDES POST. A frame controller publishes on
  // `render_drained_o`; the frame is not finished until the compositor has
  // written it back and every word of that has retired too.
  assign render_drained_o = fbw_drained_w && !post_busy_o;

  // ---- POST.COMPOSITE's lease ------------------------------------------------
  logic        e0_beat_valid, e0_beat_last;
  // Reads in flight on ENGINE0 (owner ruling R38): the lease's share and the
  // read-beat `last` queue below are sized by this ONE number.
  localparam int unsigned POST_E0_MAX_RD = 2;
  zhao_post_lease #(.XW(9), .YW(8), .MAX_RD(POST_E0_MAX_RD)) u_post_lease (
    .clk(gpu_clk), .rst_n(rst_n),
    // The renderer's live lease: the one the render guard's window comes from.
    .lease_live_i  (rmap_valid_q),
    .frame_admit_i (v2_frame_admit_w),
    .frame_end_i   (render_frame_end_i),
    .raster_quiet_i(v2_bin_quiet_w),
    .raster_px_i   (rpx_valid),
    .fbw_drained_i (fbw_drained_w),
    .fb_base_i     (render_fb_base_i),
    .fb_stride_i   (render_fb_stride_i),
    .frame_w_i     (post_frame_w_i),
    .frame_h_i     (post_frame_h_i),
    .duo_i         (post_duo_i),
    .echo_arm_i    (post_echo_arm_i),
    .look_hold_i   (post_look_hold_i),
    .pass_start_o  (post_pass_start_o),
    .view_o        (post_view_o),
    .src_valid_o   (post_src_valid_o),
    .src_ready_i   (post_src_ready_i),
    .src_rgb_o     (post_src_rgb_o),
    .out_valid_i   (post_out_valid_i),
    .out_ready_o   (post_out_ready_o),
    .out_rgb_i     (post_out_rgb_i),
    .out_x_i       (post_out_x_i),
    .out_y_i       (post_out_y_i),
    .out_last_i    (post_out_last_i),
    .echo_valid_i  (post_echo_valid_i),
    .echo_rgb_i    (post_echo_rgb_i),
    .phase_post_o  (post_phase_w),
    .fbw_px_valid_o(ppx_valid),
    .fbw_px_ready_i(ppx_ready),
    .fbw_px_rgb_o  (ppx_rgb565),
    .fbw_px_x_o    (ppx_x),
    .fbw_px_y_o    (ppx_y),
    .fbw_px_last_o (ppx_last),
    .fbw_req_i     (fbw_guard_req),
    .fbw_rsp_o     (fbw_guard_rsp),
    .fbw_wdata_i   (fbw_guard_wdata),
    .fbw_wvalid_i  (fbw_guard_wvalid),
    .fbw_wready_o  (fbw_guard_wready),
    .fbw_wlast_i   (fbw_guard_wlast),
    .fbw_retire_o  (fbw_retire_words),
    .e0_req_o      (render_guard_req),
    .e0_rsp_i      (render_guard_rsp),
    .e0_wdata_o    (render_wdata),
    .e0_wvalid_o   (render_wvalid),
    .e0_wready_i   (render_wready),
    .e0_wlast_o    (render_wlast),
    .e0_beat_valid_i(e0_beat_valid),
    .e0_beat_data_i(packed_data),
    .e0_beat_last_i(e0_beat_last),
    .e0_credits_i  (client_rsp[2].credits),
    .busy_o        (post_busy_o),
    .passes_o      (post_passes_o),
    .frames_o      (post_frames_o),
    .fault_o       (post_fault_o),
    .src_reads_o   (post_src_reads_o),
    .src_pixels_o  (post_src_pixels_o),
    .retire_unowned_o(post_retire_unowned_o),
    .share_contention_o(post_share_contention_o),
    .echo_passes_complete_o(echo_passes_complete_o),
    .echo_passes_torn_o(echo_passes_torn_o),
    .echo_pixels_written_o(echo_pixels_written_o),
    .echo_pixels_dropped_o(echo_pixels_dropped_o),
    .echo_fault_o  (echo_fault_o)
  );

  // THE RENDER GUARD'S WINDOW IS THE RENDERER'S LEASE, AND IT USED TO BE THE
  // BLITTER'S.
  //
  // "Wired exactly as DEBUG.FRAMEBLIT is" was true of the request path and was
  // also true of the WINDOW, which is where it stopped being right:
  // `map_valid_q`/`map_slot_q`/`map_span_q` are `fb_lease_valid`,
  // `fb_lease_slot` and `r_blit_len` -- the BLIT lease and the BLIT packet's
  // byte count. While the RENDERER holds the lease the blit lease is not live,
  // so `map_valid` is low, and `zhao_mem_guard`'s own law is "map_valid=0 (no
  // grant this frame) => deny-all". Every render burst was refused.
  //
  // It presented as a render path that produces nothing, with the counter that
  // would have explained it -- `render_gv_cnt` -- never read by anything.
  // Measured on the console smoke bench before this window existed: the bin
  // pipe resolved 6 tiles and handed 1,536 fragments to `zhao_raster_fbwrite`,
  // which latched `fatal_error_o` on the first burst and wrote zero.
  //
  // The window comes from VIDEO.SLOTMGR's LIVE LEASE rather than from the
  // renderer lease's `frame_*` identity, because the manager's record is the
  // authority the contract names -- "the accepted response, live lease, READY
  // event, stored slot record, displayed record and swap echo all carry the
  // same immutable {writer,slot,generation,mode,base,span}" -- and because it
  // is a LEVEL that lasts exactly as long as the permission does, where
  // `frame_valid_o` is one cycle at admission.
  //
  // `lease_span_o` is the manager's mode-derived canvas size, which is exactly
  // the `blit_span = canvas_bytes(mode)` the guard's header asks for; the guard
  // clamps it to the slot span regardless, so a wrong mode cannot open an
  // escape here.
  //
  // THE BLIT GUARD IS DELIBERATELY LEFT ALONE. Its window is `r_blit_len`, the
  // length of the packet in flight, which is NARROWER than the canvas. Moving
  // it to `lease_span_o` for symmetry would WIDEN a permission, and this packet
  // has no reason to do that.
  logic        rmap_valid_q;
  logic [0:0]  rmap_slot_q;
  logic [31:0] rmap_span_q;

  zhao_mem_guard u_guard_render (
    .clk        (gpu_clk),
    .rst_n      (rst_n),
    .req        (render_guard_req),
    .rsp        (render_guard_rsp),
    .map_valid  (rmap_valid_q),
    .blit_slot  (rmap_slot_q),
    .blit_span  (rmap_span_q),
    // The SAME lease owner the blit guard sees. This guard passes only when the
    // lease names the render engine, so exactly one of the two writers can ever
    // pass in a frame -- the lease ruling in hardware rather than in a comment.
    .fb_writer  (fb_writer_i),
    .res_valid  (1'b0),   // TIE: this client is not MEM.UPLOAD; the R32 resource-write arm names TERRAIN_BUILD alone
    .res_base   (32'd0),  // TIE: this client is not MEM.UPLOAD; the R32 resource-write arm names TERRAIN_BUILD alone
    .res_span   (32'd0),  // TIE: this client is not MEM.UPLOAD; the R32 resource-write arm names TERRAIN_BUILD alone
    // TIE: this guard's client is never ENGINE1, so item 4's PARAMBUF window is
    // shut here in both directions
    .pb_lease_valid   (1'b0),
    .pb_wr_view       (1'b0),
    .pb_scratch_valid (1'b0),
    .arb_req    (render_arb_req),
    .arb_rsp    (client_rsp[2]),
    .guard_violation     (render_gv),
    .guard_violations    (render_gv_cnt),
    .guard_violation_req (render_gv_req)
  );

  // ENGINE0's WRITES WAIT FOR THEIR DATA (2026-09-19). The render engine used
  // to reach the guard directly and push its row the cycle after the verdict,
  // so the data was always queued before the controller could want it -- a
  // latency race that happened to be won. With ENGINE0 shared (the post lease)
  // the verdict reaches a writer later, and the race is no longer won by
  // construction. Slot 6 already solved this exactly: the arbiter sees a write
  // only when all its words are in the queue and not yet owed to an accepted
  // request. The same gate, on the framebuffer queue, for the same reason.
  // ENFORCED-BY: tests/prod/tb_zhao_console_core_smoke.sv (shell_err_wfifo_o
  //              stays 0 through the raster AND the post write-back)
  logic [$clog2(WFIFO_W):0] wf_owed;
  logic                     wf_room_for_req;
  assign wf_room_for_req =
      ({1'b0, wf_occ} - {1'b0, wf_owed})
        >= ($bits(wf_occ)+1)'(build_words_of(render_arb_req.len));
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) wf_owed <= '0;
    else wf_owed <= wf_owed
         + ((client_rsp[2].grant && render_arb_req.write)
              ? ($bits(wf_owed))'(build_words_of(render_arb_req.len)) : '0)
         // A THIRD OWNER HAS TO BE EXCLUDED HERE TOO. `!wr_sel_build` alone
         // would count slot 3's beats as framebuffer pops and walk this
         // counter down under a write it has nothing to do with -- which
         // shows up as the framebuffer gate opening early, not as a geometry
         // fault. The two faults are in different blocks and only one of them
         // is where the change was made.
         - ((wr_beat_ctrl && !wr_sel_build && !wr_sel_geom && (wf_owed != '0))
              ? ($bits(wf_owed))'(1) : '0);
  end
  always_comb begin
    client_req[2]       = render_arb_req;
    client_req[2].valid = render_arb_req.valid
                          && (!render_arb_req.write || wf_room_for_req);
  end
  // D22 TREAD 10. Slot 3 was tied to '0 and is now the geometry fetch path to
  // real memory, through the real guard -- the same block the scanout and blit
  // clients use, not a second copy of its rules.
  //
  // ONE geometry client, not two, and the reason is not a simplification.
  // `zhao_vram_arbiter` builds the controller's client tag by CASTING THE SLOT
  // INDEX: `ctrl_req.client = zhao_client_e'(offer_client)`. So slot 3 IS
  // ENGINE1 and slot 4 IS DEBUG -- positional, not configurable. And
  // `zhao_mem_guard` grants the asset-pool window to ENGINE1 alone; DEBUG
  // falls to `default: pass_ok = 1'b0` and "still owns nothing".
  //
  // A second geometry fetcher therefore has NO LEGAL MEMORY IDENTITY today. It
  // is not that wiring it is hard: there is no client enum it could present
  // that both the guard admits and the arbiter would carry. GEOM.MESHFETCH and
  // GEOM.ASSETFETCH sharing ENGINE1 through one guard, or the memory law
  // allocating a second geometry client, is a decision for the memory rules --
  // recorded here rather than fudged by putting a fetcher on a slot that will
  // be relabelled DEBUG one level down and refused.
  zhao_arb_req_t geom_arb_req;
  logic          geom_gv;
  logic [31:0]   geom_gv_cnt;
  zhao_guard_req_t geom_gv_req;

  zhao_mem_guard u_guard_geom (
    .clk        (gpu_clk),
    .rst_n      (rst_n),
    .req        (geom_guard_req_i),
    .rsp        (geom_guard_rsp_o),
    .map_valid  (1'b0),  // TIE: inherited verbatim from zhao_shell_top.sv; a V1 decision this packet carries rather than makes
    .blit_slot  (1'b0),  // TIE: inherited verbatim from zhao_shell_top.sv; a V1 decision this packet carries rather than makes
    .blit_span  (32'd0),  // TIE: inherited verbatim from zhao_shell_top.sv; a V1 decision this packet carries rather than makes
    // The geometry fetchers READ the Phase-3 asset pool and never write, so
    // the framebuffer lease owner cannot reach its verdict -- same reasoning
    // as the scanout guard above.
    .fb_writer  (1'b0),  // TIE: inherited verbatim from zhao_shell_top.sv; a V1 decision this packet carries rather than makes
    .res_valid  (1'b0),   // TIE: this client is not MEM.UPLOAD; the R32 resource-write arm names TERRAIN_BUILD alone
    .res_base   (32'd0),  // TIE: this client is not MEM.UPLOAD; the R32 resource-write arm names TERRAIN_BUILD alone
    .res_span   (32'd0),  // TIE: this client is not MEM.UPLOAD; the R32 resource-write arm names TERRAIN_BUILD alone
    // ITEM 4's lease, straight from GEOM.PARAMBUF's arena producer. NOT tied:
    // this is the one guard instance in the console whose client is ENGINE1,
    // so it is the one place the window can be open at all.
    .pb_lease_valid   (geom_pb_lease_i),
    .pb_wr_view       (geom_pb_wr_view_i),
    .pb_scratch_valid (geom_pb_scratch_i),
    .arb_req    (geom_arb_req),
    .arb_rsp    (client_rsp[3]),
    .guard_violation     (geom_gv),
    .guard_violations    (geom_gv_cnt),
    .guard_violation_req (geom_gv_req)
  );

  // SLOT 3'S WRITE GATE (item 4), slot 6's discipline exactly. `gq_free` is
  // the queue's words NOT YET OWED to a request the arbiter has already
  // accepted, so a write is offered only when every one of its words is
  // already here. The controller raises `wr_beat` in the grant cycle itself on
  // a row hit, so a queue that was merely "going to be filled" hands the SDRAM
  // garbage.
  logic [$clog2(GEOM_WQ_W):0] gq_occ;
  logic [$clog2(GEOM_WQ_W):0] gq_owed;
  logic                       gq_room_for_req;
  assign gq_room_for_req =
      ({1'b0, gq_occ} - {1'b0, gq_owed}) >= ($bits(gq_occ)+1)'(build_words_of(geom_arb_req.len));
  always_comb begin
    client_req[3]       = geom_arb_req;
    client_req[3].valid = geom_arb_req.valid
                       && (!geom_arb_req.write || gq_room_for_req);
  end
  assign geom_retire_words_o = client_rsp[3].credits;
  // Slot 4 stays tied off: it is DEBUG by position, and DEBUG owns nothing.
  assign client_req[4] = '0;
  // Slot 5 is the client id ruling T3 reserves and forbids spending; the
  // arbiter refuses it at the port, and it is tied off here as well so the
  // refusal is never even exercised from this shell.
  assign client_req[5] = '0;
  // Slot 6 is TERRAIN.BUILD, and it is now a SOCKET rather than a reservation
  // (2026-09-19, cmdmem packet). Its guard is the one `zhao_mem_guard`, whose
  // TERRAIN_BUILD arm admits TERRAIN.PAGE_POOL in both directions and, since
  // owner ruling R32, WRITES inside the one published-resource region
  // (build_res_*) when that region lies wholly inside RENDER.ASSET_POOL -- so no
  // client of this socket can reach a framebuffer, or any other part of the
  // asset pool, whatever it asks for (mem_guard_no_escape, re-proved).
  //
  // THE WRITE GATE. Slot 6 has its OWN write-data queue (`bq`, below), and the
  // arbiter must not accept a write request whose words are not all in it:
  // the controller pops a word per `wr_beat` from the moment of grant, and an
  // empty queue would hand it garbage. Every other writer here pushes data
  // with its request and relies on the arbiter's latency; this one does not
  // rely on timing. `bq_free` is the queue's words NOT YET OWED to a request
  // the arbiter already accepted, so the gate is exact, not a race.
  zhao_arb_req_t   build_arb_req;
  logic            build_gv;
  logic [31:0]     build_gv_cnt;
  zhao_guard_req_t build_gv_req;

  zhao_mem_guard u_guard_build (
    .clk        (gpu_clk),
    .rst_n      (rst_n),
    .req        (build_guard_req_i),
    .rsp        (build_guard_rsp_o),
    // The TERRAIN.PAGE_POOL window has CONSTANT bounds and consults no map
    // (`zhao_mem_guard.sv`, "CONSTANT BOUNDS"), so the framebuffer-lease
    // inputs have nothing to say about it -- the geometry guard's reasoning.
    .map_valid  (1'b0),   // TIE: TERRAIN.PAGE_POOL has constant bounds; no framebuffer map applies to slot 6
    .blit_slot  (1'b0),   // TIE: TERRAIN.PAGE_POOL has constant bounds; no framebuffer map applies to slot 6
    .blit_span  (32'd0),  // TIE: TERRAIN.PAGE_POOL has constant bounds; no framebuffer map applies to slot 6
    .fb_writer  (1'b0),   // TIE: TERRAIN_BUILD holds no framebuffer lease; the guard's lease arms never name it
    .res_valid  (build_res_valid_i),
    .res_base   (build_res_base_i),
    .res_span   (build_res_span_i),
    // TIE: this guard's client is never ENGINE1, so item 4's PARAMBUF window is
    // shut here in both directions
    .pb_lease_valid   (1'b0),
    .pb_wr_view       (1'b0),
    .pb_scratch_valid (1'b0),
    .arb_req    (build_arb_req),
    .arb_rsp    (client_rsp[6]),
    .guard_violation     (build_gv),
    .guard_violations    (build_gv_cnt),
    .guard_violation_req (build_gv_req)
  );

  // words a request of `len` bytes occupies: the arbiter's own rounding
  function automatic logic [6:0] build_words_of(input logic [6:0] len_b);
    build_words_of = 7'((len_b + 7'd1) >> 1);
  endfunction

  logic [$clog2(BUILD_WQ_W):0] bq_occ;
  logic [$clog2(BUILD_WQ_W):0] bq_owed;   // words promised to accepted requests
  logic                        bq_room_for_req;
  assign bq_room_for_req =
      ({1'b0, bq_occ} - {1'b0, bq_owed}) >= ($bits(bq_occ)+1)'(build_words_of(build_arb_req.len));

  always_comb begin
    client_req[6]       = build_arb_req;
    client_req[6].valid = build_arb_req.valid
                       && (!build_arb_req.write || bq_room_for_req);
  end


  logic [6:0][31:0] vram_bytes, vram_bytes_shadow;

  zhao_vram_arbiter u_arb (
    .clk               (gpu_clk),
    .rst_n             (rst_n),
    .client_req        (client_req),
    .client_rsp        (client_rsp),
    .ctrl_req          (ctrl_req),
    .hold_refresh      (hold_refresh),
    .ctrl_rsp          (ctrl_rsp),
    .frame_tick        (gpu_tick.pulse),
    .vram_bytes        (vram_bytes),
    .vram_bytes_shadow (vram_bytes_shadow),
    .scanout_preempted (scanout_preempted_o)
  );

  // write-data queue (glue 1): dma beats (4 words each) -> wr_beat pops
  logic [15:0] wfifo [0:WFIFO_W-1];
  logic [$clog2(WFIFO_W):0] wf_wp, wf_rp;
  logic wf_err;
  logic wr_beat_ctrl;
  logic [15:0] wdata_ctrl;
  logic [$clog2(WFIFO_W):0] wf_occ;
  assign wf_occ = wf_wp - wf_rp;

  // ---- TWO WRITE QUEUES, and the burst says which one it pops ---------------
  // The framebuffer queue above holds ONE writer per frame because the lease
  // guarantees it. Slot 6 is not under that lease -- a TERRAIN.BUILD write can
  // be in flight during a render frame -- so sharing the queue would interleave
  // two writers' words and the controller would write one client's bytes at
  // the other's address. So slot 6 has its own queue, and every `wr_beat` pops
  // the queue of the client whose WRITE burst the controller granted.
  //
  // THE OWNER IS KNOWN AT GRANT, and word 0 can be needed IN THE GRANT CYCLE:
  // `zhao_sdram_ctrl` raises `wr_beat` in S_RW, which is cycle G itself on a
  // row hit. The arbiter's offer is still valid during G (it retires at the
  // END of G), so during G the owner is read straight off `ctrl_req`, and it
  // is registered for the burst's remaining beats.
  logic wr_owner_build_r;
  logic wr_sel_build;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) wr_owner_build_r <= 1'b0;
    else if (ctrl_rsp.grant && ctrl_req.write)
      wr_owner_build_r <= (ctrl_req.client == ZHAO_CLIENT_TERRAIN_BUILD);
  end
  assign wr_sel_build = (ctrl_rsp.grant && ctrl_req.write)
                        ? (ctrl_req.client == ZHAO_CLIENT_TERRAIN_BUILD)
                        : wr_owner_build_r;

  // SLOT 3'S WRITE-DATA QUEUE. A third owner on one controller word, and the
  // owner is read the same way slot 6's is: straight off `ctrl_req` during the
  // grant cycle (where `wr_beat` can already be high on a row hit) and from a
  // register for the burst's remaining beats.
  logic wr_owner_geom_r;
  logic wr_sel_geom;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) wr_owner_geom_r <= 1'b0;
    else if (ctrl_rsp.grant && ctrl_req.write)
      wr_owner_geom_r <= (ctrl_req.client == ZHAO_CLIENT_ENGINE1);
  end
  assign wr_sel_geom = (ctrl_rsp.grant && ctrl_req.write)
                       ? (ctrl_req.client == ZHAO_CLIENT_ENGINE1)
                       : wr_owner_geom_r;

  logic [15:0] gq [0:GEOM_WQ_W-1];
  logic [$clog2(GEOM_WQ_W):0] gq_wp, gq_rp;
  logic gq_err;
  assign gq_occ        = gq_wp - gq_rp;
  assign geom_wready_o = (gq_occ <= ($bits(gq_occ))'(GEOM_WQ_W - 4));

  wire gq_pop     = wr_beat_ctrl && wr_sel_geom;
  wire gq_promise = client_rsp[3].grant && geom_arb_req.write;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      gq_wp <= '0; gq_rp <= '0; gq_owed <= '0; gq_err <= 1'b0;
    end else begin
      if (geom_wvalid_i && geom_wready_o) begin
        for (int j = 0; j < 4; j++)
          gq[($clog2(GEOM_WQ_W))'(gq_wp + ($bits(gq_wp))'(j))] <= geom_wdata_i[16*j +: 16];
        gq_wp <= gq_wp + ($bits(gq_wp))'(4);
      end
      if (gq_pop) begin
        // A pop from an empty queue is a garbage word written to VRAM. The
        // write gate makes it unreachable; this is the tripwire that says so.
        if (gq_occ == '0) gq_err <= 1'b1;
        else gq_rp <= gq_rp + ($bits(gq_rp))'(1);
      end
      gq_owed <= gq_owed
               + (gq_promise ? ($bits(gq_owed))'(build_words_of(geom_arb_req.len)) : '0)
               - (gq_pop && (gq_owed != '0) ? ($bits(gq_owed))'(1) : '0);
    end
  end

  logic [15:0] bq [0:BUILD_WQ_W-1];
  logic [$clog2(BUILD_WQ_W):0] bq_wp, bq_rp;
  logic bq_err;
  assign bq_occ         = bq_wp - bq_rp;
  assign build_wready_o = (bq_occ <= ($bits(bq_occ))'(BUILD_WQ_W - 4));

  // THREE OWNERS, AND THE SELECTORS ARE MUTUALLY EXCLUSIVE BY CONSTRUCTION,
  // not by priority: each is `ctrl_req.client == <one enum value>` over the
  // same grant, and a request carries one client. The `if/else` order is
  // therefore readability and not arbitration -- which is worth saying,
  // because a priority mux that could pick the wrong source would need two
  // ACTIVE sources, and the arbiter grants one client at a time.
  assign wdata_ctrl = wr_sel_build ? bq[bq_rp[$clog2(BUILD_WQ_W)-1:0]]
                    : wr_sel_geom  ? gq[gq_rp[$clog2(GEOM_WQ_W)-1:0]]
                                   : wfifo[wf_rp[$clog2(WFIFO_W)-1:0]];

  // the words the arbiter has been promised: + a request's words when it is
  // ACCEPTED (registered `client_rsp[6].grant`, the cycle the guard still
  // presents that very request), - one per word popped. ONE assignment, so a
  // grant and a pop in the same cycle are both counted.
  wire bq_pop = wr_beat_ctrl && wr_sel_build;
  wire bq_promise = client_rsp[6].grant && build_arb_req.write;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      bq_wp <= '0; bq_rp <= '0; bq_owed <= '0; bq_err <= 1'b0;
    end else begin
      if (build_wvalid_i && build_wready_o) begin
        for (int j = 0; j < 4; j++)
          bq[($clog2(BUILD_WQ_W))'(bq_wp + ($bits(bq_wp))'(j))] <= build_wdata_i[16*j +: 16];
        bq_wp <= bq_wp + ($bits(bq_wp))'(4);
      end
      if (bq_pop) begin
        // A pop from an empty queue is a garbage word written to VRAM. The
        // write gate makes it unreachable; this is the tripwire that says so.
        if (bq_occ == '0) bq_err <= 1'b1;
        else bq_rp <= bq_rp + ($bits(bq_rp))'(1);
      end
      bq_owed <= bq_owed
               + (bq_promise ? ($bits(bq_owed))'(build_words_of(build_arb_req.len)) : '0)
               - (bq_pop && (bq_owed != '0) ? ($bits(bq_owed))'(1) : '0);
    end
  end

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      wf_wp <= '0;
      wf_rp <= '0;
      wf_err <= 1'b0;
    end else begin
      // The queue now ANSWERS. It used to accept every beat and set a sticky
      // error afterwards if it had not really had room -- which is a report
      // that pixels were lost, not a mechanism for not losing them. One 64-bit
      // beat becomes four 16-bit words, so it is ready exactly when four fit.
      // `wf_err` stays as a tripwire; the overflow branch is now structurally
      // unreachable from the write side.
      // ONE WRITE QUEUE, ONE WRITER PER FRAME. DEBUG.FRAMEBLIT and
      // RASTER.FBWRITE both produce 64-bit write beats, and MEM.GUARD has
      // already made it impossible for both to pass in the same frame -- the
      // lease names one of them and refuses the other. So the queue takes
      // whichever is presenting, with no arbitration to get wrong: a mux here
      // that could pick the wrong source would need two ACTIVE sources, and the
      // guard guarantees there is at most one.
      //
      // ENFORCED-BY: tests/memory/mem_guard_directed.cpp (owner mismatch)
      if (fbw_wvalid && fbw_wready) begin
        if (wf_occ > ($bits(wf_occ))'(WFIFO_W - 4)) begin
          wf_err <= 1'b1;                      // overflow: beats dropped
        end else begin
          for (int j = 0; j < 4; j++) begin
            // power-of-two depth: the pointer's low bits ARE the index
            wfifo[($clog2(WFIFO_W))'(wf_wp + ($bits(wf_wp))'(j))]
              <= fbw_wdata[16*j +: 16];
          end
          wf_wp <= wf_wp + ($bits(wf_wp))'(4);
        end
      end
      if (wr_beat_ctrl && !wr_sel_build && !wr_sel_geom) begin
        if (wf_occ == '0) wf_err <= 1'b1;      // underflow: garbage word
        else wf_rp <= wf_rp + ($bits(wf_rp))'(1);
      end
    end
  end
  // The selected framebuffer writer, and the readiness both are told.
  logic [63:0] fbw_wdata;
  logic        fbw_wvalid, fbw_wready;
  assign fbw_wvalid = blit_wvalid || render_wvalid;
  assign fbw_wdata  = render_wvalid ? render_wdata : blit_wdata;
  assign fbw_wready = (wf_occ <= ($bits(wf_occ))'(WFIFO_W - 4));
  assign blit_wready   = fbw_wready;
  assign render_wready = fbw_wready;
  // BOTH write queues' tripwires: a word written from an empty queue is a
  // garbage word in VRAM whichever queue it came from.
  // THREE QUEUES, ONE TRIPWIRE. `gq_err` joins it rather than getting its own
  // port: the smoke asserts this stays 0, and a new queue whose underflow was
  // invisible to that assertion would be a queue nothing watches.
  assign shell_err_wfifo_o = wf_err || bq_err || gq_err;

  // read-beat packer (glue 2): 4 rdata words -> one 64-bit beat
  //
  // D22 TREAD 10 added a SECOND destination. The packer itself is unchanged --
  // one burst is in flight at a time (`zhao_sdram_ctrl` is strictly in-order,
  // one burst deep) and a burst is at most 8 words, exactly two whole groups of
  // four, so a group never straddles two bursts and never two owners. What is
  // new is that the packed beat is now ROUTED.
  //
  // The owner is captured at GRANT, not derived from the returning word: the
  // word carries no tag, and inferring the owner from the address would be
  // reconstructing at the far end something that was known for free at the
  // near one.
  logic [15:0] ctrl_rdata;
  logic        ctrl_rdata_valid;
  logic [47:0] pack_lo;
  logic [1:0]  pack_cnt;

  logic rd_owner_geom_r;
  // THE THIRD READ OWNER, slot 6 (the TERRAIN.BUILD socket). Captured at the
  // same grant edge by the same rule; a returning word carries no tag.
  logic rd_owner_build_r;
  // THE FOURTH READ OWNER, ENGINE0 (2026-09-19): POST.COMPOSITE's source
  // read-back, through the post lease. Same capture rule, same reason.
  logic rd_owner_e0_r;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      rd_owner_geom_r  <= 1'b0;
      rd_owner_build_r <= 1'b0;
      rd_owner_e0_r    <= 1'b0;
    end else if (ctrl_rsp.grant && !ctrl_req.write) begin
      rd_owner_geom_r  <= (ctrl_req.client == ZHAO_CLIENT_ENGINE1);
      rd_owner_build_r <= (ctrl_req.client == ZHAO_CLIENT_TERRAIN_BUILD);
      rd_owner_e0_r    <= (ctrl_req.client == ZHAO_CLIENT_ENGINE0);
    end
  end

  logic        packed_valid;
  logic [63:0] packed_data;

  // Beat position WITHIN the guard request, so `last` marks the end of the
  // 64-byte line the fetcher asked for rather than the end of an arbiter
  // burst. The fetcher counts eight beats per line; the arbiter splits that
  // line into four 8-word bursts, and it must not see four `last` pulses.
  // AND THE COUNT COMES FROM THE REQUEST, NOT FROM A CONSTANT.
  //
  // This counted a fixed eight, because the only geometry client at tread 10
  // was GEOM.ASSETFETCH and its lines are 64 bytes. The owner's recovery brief
  // section 12.3 is explicit that there are TWO burst scales -- ASSETFETCH's
  // 64-byte line is eight packed words, MESHFETCH's 32-byte descriptor is four
  // -- and that "any existing geometry beat/last generator assuming eight words
  // must be generalized and tested for four/eight, not bypassed by fetching a
  // neighboring descriptor".
  //
  // A constant eight against a 32-byte request does not merely mis-mark the
  // end: the counter never reaches eight, so `last` never fires and the count
  // carries into the NEXT request, putting a `last` in the middle of a line
  // that has nothing to do with it.
  //
  // `len` is bytes and a packed word is eight of them, so the expected count is
  // `len >> 3`, captured at the same handshake that clears the counter -- the
  // request is not still on the pins when the beats come back.
  logic [3:0] geom_expect_r;
  logic [3:0] geom_beat_cnt_r;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) geom_beat_cnt_r <= 4'd0;
    // The reset is the guard HANDSHAKE, not `ready && ok`. Those two never
    // coincide: `rsp.ready` is the LEVEL `!fwd_active`, and `rsp.ok` pulses
    // one cycle AFTER the accept, by which time the request is forwarded and
    // ready has already dropped. A condition that cannot be true is a reset
    // that never happens.
    else if (geom_guard_req_i.valid && geom_guard_rsp_o.ready) geom_beat_cnt_r <= 4'd0;
    else if (geom_beat_valid_o) geom_beat_cnt_r <= geom_beat_cnt_r + 4'd1;
  end

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) geom_expect_r <= 4'd8;
    else if (geom_guard_req_i.valid && geom_guard_rsp_o.ready)
      geom_expect_r <= 4'(geom_guard_req_i.len >> 3);
  end

  assign geom_beat_valid_o = packed_valid && rd_owner_geom_r;
  assign geom_beat_data_o  = packed_data;
  assign geom_beat_last_o  = geom_beat_valid_o &&
                             (geom_beat_cnt_r + 4'd1 == geom_expect_r);

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      pack_lo      <= '0;
      pack_cnt     <= 2'd0;
      packed_valid <= 1'b0;
      packed_data  <= '0;
    end else begin
      packed_valid <= 1'b0;
      if (ctrl_rdata_valid) begin
        if (pack_cnt == 2'd3) begin
          packed_data  <= {ctrl_rdata, pack_lo};
          packed_valid <= 1'b1;
          pack_cnt     <= 2'd0;
        end else begin
          pack_lo[16*pack_cnt +: 16] <= ctrl_rdata;
          pack_cnt                   <= pack_cnt + 2'd1;
        end
      end
    end
  end

  assign scan_beat_valid = packed_valid && !rd_owner_geom_r && !rd_owner_build_r
                           && !rd_owner_e0_r;

  // ---- ENGINE0's read beats (the post source), last from ITS request ------
  // The geometry path's lesson, applied a third time: last marks the end of
  // the guard request, counted from the accepted request's own len.
  //
  // MORE THAN ONE READ IN FLIGHT (owner ruling R38, 2026-09-19). This counter
  // was reset at every read's guard ACCEPT, which is right only while one read
  // is outstanding: with the post lease's second read, read 2's accept landed
  // in the middle of read 1's return, `last` fired at the wrong beat, the share
  // discarded the tail as an overlong return and the post pass stopped dead
  // after 48 pixels (measured on the console smoke). A counter that resets on
  // the NEXT request is a frozen copy of "there is only ever one".
  //
  // So the expectations queue, in the order the guard PASSED the reads -- the
  // order their beats return in, since guard, arbiter and controller are all
  // strictly in order. An entry is pushed at the read's VERDICT, not its accept:
  // a refused read returns nothing and must owe nothing. The length is captured
  // at the accept (the request is off the pins by the verdict) and the queue is
  // as deep as the lease's MAX_RD, which is what bounds it: the share holds at
  // most that many passed reads unreturned.
  localparam int unsigned E0_RDQ = POST_E0_MAX_RD;
  localparam int unsigned E0_RQW = (E0_RDQ > 1) ? $clog2(E0_RDQ) : 1;
  logic [3:0]        e0_exp_q [0:E0_RDQ-1];
  logic [E0_RQW-1:0] e0_ewp_q, e0_erp_q;
  logic [3:0]        e0_acc_len_q;      // the accepted read's beats, until its verdict
  logic              e0_acc_rd_q;       // ... and whether the accepted request IS a read
  logic [3:0]        e0_expect_r;
  logic [3:0]        e0_beat_cnt_r;
  assign e0_expect_r = e0_exp_q[e0_erp_q];
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      e0_beat_cnt_r <= 4'd0;
      e0_ewp_q      <= '0;
      e0_erp_q      <= '0;
      e0_acc_len_q  <= 4'd8;
      e0_acc_rd_q   <= 1'b0;
      for (int k = 0; k < int'(E0_RDQ); k++) e0_exp_q[k] <= 4'd8;
    end else begin
      if (render_guard_req.valid && render_guard_rsp.ready) begin
        e0_acc_len_q <= 4'(render_guard_req.len >> 3);
        e0_acc_rd_q  <= !render_guard_req.write;
      end
      if (render_guard_rsp.ok && e0_acc_rd_q) begin
        e0_exp_q[e0_ewp_q] <= e0_acc_len_q;
        e0_ewp_q           <= (E0_RDQ > 1) ? (e0_ewp_q + E0_RQW'(1)) : '0;
      end
      if (e0_beat_valid) begin
        if (e0_beat_last) begin
          e0_beat_cnt_r <= 4'd0;
          e0_erp_q      <= (E0_RDQ > 1) ? (e0_erp_q + E0_RQW'(1)) : '0;
        end else begin
          e0_beat_cnt_r <= e0_beat_cnt_r + 4'd1;
        end
      end
    end
  end
  assign e0_beat_valid = packed_valid && rd_owner_e0_r;
  assign e0_beat_last  = e0_beat_valid && (e0_beat_cnt_r + 4'd1 == e0_expect_r);
  assign scan_beat_data  = packed_data;

  // ---- slot 6's read beats, with `last` counted from ITS request -----------
  // The geometry path's lesson, not re-learned: `last` marks the end of the
  // guard request, counted from that request's own `len`, never a constant.
  logic [3:0] build_expect_r;
  logic [3:0] build_beat_cnt_r;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      build_beat_cnt_r <= 4'd0;
      build_expect_r   <= 4'd8;
    end else if (build_guard_req_i.valid && build_guard_rsp_o.ready) begin
      build_beat_cnt_r <= 4'd0;
      build_expect_r   <= 4'(build_guard_req_i.len >> 3);
    end else if (build_beat_valid_o) begin
      build_beat_cnt_r <= build_beat_cnt_r + 4'd1;
    end
  end
  assign build_beat_valid_o = packed_valid && rd_owner_build_r;
  assign build_beat_data_o  = packed_data;
  assign build_beat_last_o  = build_beat_valid_o &&
                              (build_beat_cnt_r + 4'd1 == build_expect_r);
  assign build_retire_words_o = client_rsp[6].credits;

  // burst-owner tracking (integrity tripwire, glue 2): reads must be
  // scanout's, writes must be THE CURRENT FRAMEBUFFER-WRITE LEASE HOLDER'S --
  // anything else is a routing bug.
  //
  // THIS CHECK USED TO SAY `!= ZHAO_CLIENT_BLIT_DMA` FLAT, and that was a real
  // fault, not a strictness. `zhao_mem_guard` was taught the lease -- it admits
  // BLIT_DMA when `fb_writer == 0` and ENGINE0 when `fb_writer == 1` -- but the
  // tripwire below it was never updated. So every legal RASTER.FBWRITE burst
  // passed the guard, reached the controller, and then latched
  // `shell_err_route_o` on the way out. A renderer frame could not run without
  // raising the shell's own corruption alarm.
  //
  // The tripwire must consult the SAME lease the guard does. One signal, and
  // now three places agree on it (blit guard, render guard, this check) --
  // which is the rule `fb_writer_i`'s comment above already states.
  logic [2:0] expected_writer;
  assign expected_writer = fb_writer_i ? ZHAO_CLIENT_ENGINE0 : ZHAO_CLIENT_BLIT_DMA;

  logic route_err;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      route_err <= 1'b0;
    end else if (ctrl_rsp.grant) begin
      // TERRAIN_BUILD is the second legal writer (the slot-6 socket, 2026-09-19)
      // and is not under the framebuffer lease: MEM.GUARD confines it to
      // TERRAIN.PAGE_POOL, which is disjoint from both FB slots. The tripwire
      // learns it in the same edit the guard's client arm was connected --
      // the mistake this block's own comment records twice was learning one
      // and not the other.
      //
      // AND A THIRD TIME, 2026-09-22 (owner completion ruling ITEM 4).
      // ENGINE1 is now a legal WRITER -- GEOM.PARAMBUF's arena producer --
      // and this arm did not know it, so EVERY arena write raised the shell's
      // own corruption alarm. The paragraph above records the same mistake
      // twice and I made it a third time in the same file, which is worth
      // leaving on the record: the comment is not the mechanism. What finally
      // caught it was the console smoke asserting the tripwire stays 0, one
      // form and one assertion, after lint, the formal proof, four mutants and
      // a 316-check acceptance bench had all passed.
      //
      // Widened DELIBERATELY and no further, on the TERRAIN_BUILD precedent:
      // ENGINE1 is not under the framebuffer lease, and MEM.GUARD confines its
      // writes to the view the PARAMBUF lease NAMES -- disjoint from both FB
      // slots, from TERRAIN's regions, from POST.ECHO and from
      // RENDER.ASSET_POOL, which stays read-only to it. So this arm admits an
      // identity the guard has already bounded; it does not open a region.
      if (ctrl_req.write  && (ctrl_req.client != expected_writer)
                          && (ctrl_req.client != ZHAO_CLIENT_TERRAIN_BUILD)
                          && (ctrl_req.client != ZHAO_CLIENT_ENGINE1))
        route_err <= 1'b1;
      // TREAD 10: reads may now be SCANOUT'S OR ENGINE1'S. This is the same
      // mistake the write side already made once and is documented above --
      // the guard was taught a new legal client and the tripwire below it was
      // not, so every legal burst raised the shell's own corruption alarm.
      // Widened DELIBERATELY and no further: ENGINE1 is the one identity
      // `zhao_mem_guard` grants the asset-pool window to.
      // 2026-09-19: ENGINE0 READS are legal while the lease names the render
      // engine (POST.COMPOSITE's read/write lease, the guard's b_read_ok).
      // Learned in the SAME edit as the guard arm, for the reason this block's
      // own comments give twice.
      if (!ctrl_req.write && (ctrl_req.client != ZHAO_CLIENT_SCANOUT)
                          && (ctrl_req.client != ZHAO_CLIENT_ENGINE1)
                          && (ctrl_req.client != ZHAO_CLIENT_TERRAIN_BUILD)
                          && !((ctrl_req.client == ZHAO_CLIENT_ENGINE0) && fb_writer_i))
        route_err <= 1'b1;
    end
  end
  assign shell_err_route_o = route_err;

  zhao_sdram_ctrl u_ctrl (
    .clk           (gpu_clk),
    .rst_n         (rst_n),
    .req           (ctrl_req),
    .rsp           (ctrl_rsp),
    .hold_refresh  (hold_refresh),
    .wdata         (wdata_ctrl),
    .wr_beat       (wr_beat_ctrl),
    .rdata         (ctrl_rdata),
    .rdata_valid   (ctrl_rdata_valid),
    .phy_cs_n      (phy_cs_n_o),
    .phy_ras_n     (phy_ras_n_o),
    .phy_cas_n     (phy_cas_n_o),
    .phy_we_n      (phy_we_n_o),
    .phy_a         (phy_a_o),
    .phy_ba        (phy_ba_o),
    .phy_dq_o      (phy_dq_o),
    .phy_dq_oe     (phy_dq_oe_o),
    .phy_dqm       (phy_dqm_o),
    .phy_dq_i      (phy_dq_i),
    .init_done     (init_done_o),
    .refresh_stalls(refresh_stalls_o),
    .bank_conflicts(bank_conflicts_o),
    .refresh_pulse (ctrl_refresh_pulse)
  );

  // HPS bridge: the DMA's burst port through the verified bridge core
  // Seven: see zhao_hps_bridge's port comment. Five silently discarded
  // client 6's bytes, so the accounting read zero for the two terrain
  // movers that use it.
  logic [6:0][31:0] hps_bytes, hps_bytes_shadow;

  // ==========================================================================
  // THE BLIT PATH: DEBUG.FRAMEBLIT + VIDEO.SLOTMGR
  // ==========================================================================
  // What this replaces: CMD.DMA's in-house blitter, which carried a whole-canvas
  // staging buffer because the old rule said nothing may be written to VRAM
  // before the checksum passed -- and the `ready_tog` glue below, which stood in
  // for slot ownership with a bare toggle and no notion of which slot was on
  // screen.
  //
  // ---- THE ONE-CYCLE LAW THIS SEAM LIVES OR DIES BY ------------------------
  // DEBUG.FRAMEBLIT latches `fb_lease_generation_i` on the SAME edge it accepts
  // a request. So the lease must already be granted when the request arrives:
  // asking for the lease and handing over the request on one edge makes the
  // blitter latch the generation from BEFORE the grant, and every publication
  // is then refused as stale by a slot manager that is working perfectly.
  //
  // Hence the small sequencer below. It costs two cycles per blit and it is the
  // difference between a machine that works and one that quietly never shows a
  // frame.
  // =========================================================================
  // THE WRITER-AWARE LEASE SEAM
  // =========================================================================
  //
  // What this replaces: the four-state `lseq` sequencer and
  // `zhao_video_slotmgr`. The sequencer existed for one reason, and the reason
  // did not go away with the protocol --
  //
  //     DEBUG.FRAMEBLIT latches `fb_lease_generation_i` on the SAME edge it
  //     accepts a request. So the lease must already be granted when the
  //     request arrives [...] and every publication is then refused as stale
  //     by a slot manager that is working perfectly.
  //
  // `zhao_video_blit_lease_v2` carries that law structurally: the record is
  // frozen at the response and the request is offered a state LATER, so it
  // cannot be the pre-grant one. It also has the control the old comment never
  // had -- ISSUE_EARLY, which breaks exactly this and is DETECTED.
  //
  // The whole seam is driven end to end by
  // tests/shell/zhao_shell_v2_lease_path.sv: one legal frame, both writers
  // contending on the shared response, the clear ordering measured against the
  // bin pipe's own drain, a structural fault suppressing publication and
  // releasing the lease, and the swap echo through both CDC FIFOs.

  // ---- renderer lease <-> manager -----------------------------------------
  logic        v2_render_req_valid, v2_render_req_ready, v2_render_req_slot;
  logic [1:0]  v2_render_req_mode;
  logic        v2_lease_rsp_ready, v2_blit_rsp_ready;
  logic        v2_clear_valid_w, v2_clear_ready_w;
  logic        v2_frame_valid, v2_frame_admit_w;

  // ---- blit lease <-> manager ---------------------------------------------
  logic        v2_blit_mgr_req_valid, v2_blit_mgr_req_ready, v2_blit_mgr_req_slot;
  logic [1:0]  v2_blit_mgr_req_mode;

  // ---- the shared response -------------------------------------------------
  logic        v2_rsp_valid, v2_rsp_ready, v2_rsp_writer, v2_rsp_granted;
  logic        v2_rsp_slot;
  logic [15:0] v2_rsp_generation;
  logic [1:0]  v2_rsp_mode;
  logic [31:0] v2_rsp_base, v2_rsp_span;

  // ---- terminal join -> manager ---------------------------------------------
  logic        v2_term_valid, v2_term_ready, v2_term_writer, v2_term_slot;
  logic [15:0] v2_term_generation;
  logic        v2_term_publish, v2_term_fault;

  // ---- manager live lease and READY -----------------------------------------
  logic        v2_lease_valid, v2_lease_writer, v2_lease_slot, v2_lease_fault;
  logic [15:0] v2_lease_generation;
  logic [1:0]  v2_lease_mode;
  logic [31:0] v2_lease_base, v2_lease_span;
  logic        v2_ready_valid, v2_ready_writer, v2_ready_slot;
  logic [15:0] v2_ready_generation;
  logic [1:0]  v2_ready_mode;
  logic [31:0] v2_ready_base, v2_ready_span;
  logic        v2_swap_ready;
  logic [1:0]  v2_slot_state [0:1];

  // ---- bin pipe observability used by the seam ------------------------------
  logic v2_bin_frame_fault_w, v2_bin_lifetime_fault_w, v2_bin_quiet_w;
  logic v2_bin_initialized_w, v2_bin_raster_abort_w;
  logic v2_bin_sequence_abort_w, v2_bin_sequence_mismatch_w;
  logic v2_bin_attr_abort_w, v2_cdc_gpu_fault_w;

  // ---- the CDC and the bridge ------------------------------------------------
  logic        v2_lease_open_w;
  // A declared quiet pixel stream rather than a cast: the type is a struct
  // in zhao_pkg and Verilator will not cast an expression to it.
  zhao_px_stream_t v2_px_quiet_w;
  assign v2_px_quiet_w = '0;
  logic [83:0] v2_ready_tuple_w, v2_vid_ready_tuple_w, v2_vid_swap_tuple_w;
  logic [83:0] v2_gpu_swap_tuple_w;
  logic        v2_cdc_ready_ready_w, v2_vid_ready_valid_w, v2_vid_ready_ready_w;
  logic        v2_vid_swap_valid_w, v2_vid_swap_ready_w, v2_gpu_swap_valid_w;
  logic        v2_gpu_barrier_done_w, v2_vid_barrier_done_w;

  // THE ADMITTED FRAME IS A PULSE. `frame_valid_o` is a level held until the
  // consumer takes it; the bin pipe wants one `frame_begin_i` edge, and the
  // fire of the admitted frame is exactly that edge.
  assign v2_frame_admit_w = v2_frame_valid && 1'b1;

  // THE SHARED RESPONSE, DEMULTIPLEXED BY TAG. Each leaf already qualifies its
  // own ready by the writer tag and asserts the invariant internally, so a
  // plain OR would also be correct today. It stays a mux because it states the
  // intent at the junction a reader looks at, and because it is the form that
  // survives a future participant arriving without its own guard.
  assign v2_rsp_ready = v2_rsp_writer ? v2_lease_rsp_ready : v2_blit_rsp_ready;

  // THE 84-BIT TUPLE, applied in exactly two places and through one package.
  assign v2_ready_tuple_w = zhao_fb_tuple_pack(
      v2_ready_writer, v2_ready_slot, v2_ready_generation, v2_ready_mode,
      v2_ready_base, v2_ready_span);

  // THE SHELL'S FAULT ATTRIBUTION, and it is a decision rather than a wire. A
  // structural fault has no identity of its own -- it is a property of the
  // machine, not of a frame -- so it is attributed to whichever lease is LIVE,
  // which is the identity the manager matches against before latching.
  //
  // AND IT MUST BE A PULSE. `faults_latched_o` increments on EVERY cycle a
  // matching fault is asserted; there is no "already faulted" guard and
  // `fault_ready_o` is simply `rst_n`. The bin pipe's structural outputs are
  // LEVELS that stay high until reset, so wiring one straight through turns a
  // single fault into a count of however many cycles the machine sat in it.
  // Measured before it was fixed: a six-cycle assertion read SIX faults.
  logic v2_fault_level_c, v2_fault_level_q, v2_fault_valid_c;
  // THE FIVE TERMS, and two of them arrived by audit rather than by design.
  // `zhao_geom_bin_pipe_v2` brought structural fault outputs the V1 binner did
  // not have, and the first composition consumed four of them and left the
  // rest as empty port connections -- which the tie-off audit could not see,
  // because it skipped empty connections as "an unread output, named on
  // purpose" and because these two sit on a line that packs several
  // connections together. It reported 0 silent while two faults went in the
  // bin.
  //
  //   frame_fault          the frame-level structural fault
  //   lifetime_fault       the lease lifetime fault
  //   raster_abort         the tile pipe gave up on a raster job
  //   attribute_abort      the tile pipe gave up on attributes -- the same
  //                        class as raster_abort, from the same instance, and
  //                        dropped only because nobody looked
  //   sequence_mismatch    admission sequence did not match the return
  //   cdc_gpu_fault        the READY CDC saw a stalled producer mutate its
  //                        tuple or drop valid -- a stability violation on the
  //                        84-bit tuple, in THIS clock domain
  //
  // All six are sticky levels, which is why the edge detector below is load
  // bearing rather than decorative.
  //
  //   binner_overflow    THE BINNER'S WALL, added 2026-09-26 (GIANTREFS), and
  //                      it is the SEVENTH term. It is not a new policy: the
  //                      owner's standing directive rules that "Overflow
  //                      remains a whole-frame fault with drain, source
  //                      attribution and repeat of the prior complete frame."
  //                      It latches (LAWS CHOSEN D in the binner) when the
  //                      chunk arena or the triangle store is exhausted and the
  //                      frame's remaining geometry is walled off -- which is
  //                      exactly a frame that did not draw what it was asked to
  //                      draw. It left this shell as `render_overflow_o` and
  //                      was read by NOBODY, so the one event R7's giant
  //                      guarantee is about was invisible to the console.
  //                      It is a sticky LEVEL like the other six, which is why
  //                      the edge detector below covers it without change.
  assign v2_fault_level_c = v2_bin_frame_fault_w || v2_bin_lifetime_fault_w ||
                            v2_bin_raster_abort_w || v2_bin_attr_abort_w ||
                            v2_bin_sequence_mismatch_w || v2_cdc_gpu_fault_w ||
                            v2_bin_overflow_w;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) v2_fault_level_q <= 1'b0;
    else v2_fault_level_q <= v2_fault_level_c;
  end
  assign v2_fault_valid_c = v2_fault_level_c && !v2_fault_level_q;

  // ---- lifecycle evidence, counted where it happens -------------------------
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      v2_clear_handshakes_o <= 32'd0;
      v2_frames_admitted_o <= 32'd0;
    end else begin
      if (v2_clear_valid_w && v2_clear_ready_w)
        v2_clear_handshakes_o <= v2_clear_handshakes_o + 32'd1;
      if (v2_frame_admit_w) v2_frames_admitted_o <= v2_frames_admitted_o + 32'd1;
    end
  end

  /* verilator lint_off PINCONNECTEMPTY */
  zhao_renderer_lease_v2 u_render_lease (
    .clk(gpu_clk), .rst_n(rst_n),
    .lease_open_i(v2_lease_open_w),
    // The renderer asks for a frame the same way it always has: the harness
    // raises `render_frame_begin_i`. What changed is that the request now has
    // to be GRANTED before any work starts.
    .frame_req_valid_i(render_frame_begin_i), .frame_req_ready_o(),
    // TIE: mode 0 is the 384x240 canvas, and this shell has no mode SOURCE
    // for the render path -- the historical one does not carry the concept
    // at all. When VIDEO.MODE's selection reaches here this becomes it;
    // until then a fixed LEGAL mode is a decision, and mode 3 would not be:
    // the lease refuses it, which is how an illegal request is stopped from
    // becoming an infinite retry.
    .frame_req_mode_i(2'd0),
    .lease_valid_i(v2_lease_valid), .slot_state_i(v2_slot_state),
    .render_req_valid_o(v2_render_req_valid),
    .render_req_ready_i(v2_render_req_ready),
    .render_req_slot_o(v2_render_req_slot),
    .render_req_mode_o(v2_render_req_mode),
    .rsp_valid_i(v2_rsp_valid), .rsp_ready_o(v2_lease_rsp_ready),
    .rsp_writer_i(v2_rsp_writer), .rsp_granted_i(v2_rsp_granted),
    .rsp_slot_i(v2_rsp_slot), .rsp_generation_i(v2_rsp_generation),
    .rsp_mode_i(v2_rsp_mode), .rsp_base_i(v2_rsp_base),
    .rsp_span_i(v2_rsp_span),
    .frame_fault_clear_valid_o(v2_clear_valid_w),
    .frame_fault_clear_ready_i(v2_clear_ready_w),
    .frame_valid_o(v2_frame_valid),
    // TIE: the admitted frame is always accepted, because its consumer is a
    // PULSE. `frame_begin_i` on the bin pipe is one edge, not a stream, so
    // there is nothing here for a ready to throttle -- and holding it low
    // would stall the lease in ST_FRAME forever rather than apply
    // backpressure to anything.
    .frame_ready_i(1'b1),
    .frame_writer_o(), .frame_slot_o(), .frame_generation_o(),
    .frame_mode_o(), .frame_base_o(), .frame_span_o(),
    .frame_width_o(), .frame_height_o(), .frame_stride_o(),
    .frame_view1_y_o(), .frame_view1_offset_o()
  );

  zhao_video_blit_lease_v2 u_blit_lease (
    .clk(gpu_clk), .rst_n(rst_n),
    .lease_open_i(v2_lease_open_w),
    .dispatch_valid_i(dpy_blit_valid), .dispatch_ready_o(blit_req_ready),
    // TIE: the blitter has exactly one dispatch mode. The renderer's lease
    // carries a mode because the V3 island selects among several; the blit
    // path does not, and mode 0 is the only value its terminal accepts. A
    // second blit mode would be a protocol change, not a new literal here.
    .dispatch_slot_i(dpy_blit_dst[0]), .dispatch_mode_i(2'd0),
    .blit_req_valid_o(nb_req_valid), .blit_req_ready_i(nb_req_ready),
    .blit_done_i(nb_done),
    .fb_lease_valid_o(fb_lease_valid), .fb_lease_slot_o(fb_lease_slot),
    .fb_lease_generation_o(fb_lease_gen),
    .mgr_req_valid_o(v2_blit_mgr_req_valid),
    .mgr_req_ready_i(v2_blit_mgr_req_ready),
    .mgr_req_slot_o(v2_blit_mgr_req_slot),
    .mgr_req_mode_o(v2_blit_mgr_req_mode),
    .rsp_valid_i(v2_rsp_valid), .rsp_ready_o(v2_blit_rsp_ready),
    .rsp_writer_i(v2_rsp_writer), .rsp_granted_i(v2_rsp_granted),
    .rsp_slot_i(v2_rsp_slot), .rsp_generation_i(v2_rsp_generation),
    .idle_o(),
    .leases_acquired_o(v2_blit_leases_acquired_o),
    .leases_refused_o(v2_blit_leases_refused_o),
    .blits_dispatched_o()
  );

  zhao_video_terminal_adapter_v2 u_term_adapter (
    .clk(gpu_clk), .rst_n(rst_n),
    // The retained blitter still emits one-cycle publish/release pulses; the
    // renderer owns a held stream. This joins them into the manager's one
    // terminal channel, release-class outranking clean publication.
    .blit_publish_valid_i(blit_pub_v), .blit_publish_slot_i(blit_pub_slot),
    .blit_publish_generation_i(blit_pub_gen),
    .blit_release_valid_i(blit_rel_v), .blit_release_slot_i(blit_rel_slot),
    .blit_release_generation_i(blit_rel_gen),
    .blit_refused_o(), .blit_events_refused_o(),
    // TIE: the renderer terminal stream has no producer in this shell yet --
    // the V3 island's return path is Packet J's composition, not this one.
    .renderer_term_valid_i(1'b0), .renderer_term_ready_o(),
    // TIE: same clause as above -- no renderer terminal producer until J.
    .renderer_term_slot_i(1'b0), .renderer_term_generation_i(16'd0),
    // TIE: same clause as above -- no renderer terminal producer until J.
    .renderer_term_publish_i(1'b0), .renderer_term_fault_i(1'b0),
    .term_valid_o(v2_term_valid), .term_ready_i(v2_term_ready),
    .term_writer_o(v2_term_writer), .term_slot_o(v2_term_slot),
    .term_generation_o(v2_term_generation),
    .term_publish_o(v2_term_publish), .term_fault_o(v2_term_fault),
    .occupied_o(), .idle_o(), .source_events_captured_o(),
    .manager_terms_accepted_o()
  );

  zhao_video_slotmgr_v2 u_slotmgr (
    .clk(gpu_clk), .rst_n(rst_n),
    .render_req_valid_i(v2_render_req_valid),
    .render_req_ready_o(v2_render_req_ready),
    .render_req_slot_i(v2_render_req_slot),
    .render_req_mode_i(v2_render_req_mode),
    .blit_req_valid_i(v2_blit_mgr_req_valid),
    .blit_req_ready_o(v2_blit_mgr_req_ready),
    .blit_req_slot_i(v2_blit_mgr_req_slot),
    .blit_req_mode_i(v2_blit_mgr_req_mode),
    .rsp_valid_o(v2_rsp_valid), .rsp_ready_i(v2_rsp_ready),
    .rsp_writer_o(v2_rsp_writer), .rsp_granted_o(v2_rsp_granted),
    .rsp_slot_o(v2_rsp_slot), .rsp_generation_o(v2_rsp_generation),
    .rsp_mode_o(v2_rsp_mode), .rsp_base_o(v2_rsp_base),
    .rsp_span_o(v2_rsp_span),
    .lease_valid_o(v2_lease_valid), .lease_writer_o(v2_lease_writer),
    .lease_slot_o(v2_lease_slot), .lease_generation_o(v2_lease_generation),
    .lease_mode_o(v2_lease_mode), .lease_base_o(v2_lease_base),
    .lease_span_o(v2_lease_span), .lease_fault_o(v2_lease_fault),
    // TIE: `fault_ready_o` is `rst_n` and nothing else -- the manager never
    // refuses a fault. Reading it would invite a reader to gate the fault on
    // it, which is how a structural fault gets dropped while the shell is in
    // reset and the fault is exactly what reset is about to erase. The edge
    // detector above is what makes one fault count once.
    .fault_valid_i(v2_fault_valid_c), .fault_ready_o(),
    .fault_writer_i(v2_lease_writer), .fault_slot_i(v2_lease_slot),
    .fault_generation_i(v2_lease_generation),
    .term_valid_i(v2_term_valid), .term_ready_o(v2_term_ready),
    .term_writer_i(v2_term_writer), .term_slot_i(v2_term_slot),
    .term_generation_i(v2_term_generation),
    .term_publish_i(v2_term_publish), .term_fault_i(v2_term_fault),
    .ready_valid_o(v2_ready_valid), .ready_ready_i(v2_cdc_ready_ready_w),
    .ready_writer_o(v2_ready_writer), .ready_slot_o(v2_ready_slot),
    .ready_generation_o(v2_ready_generation), .ready_mode_o(v2_ready_mode),
    .ready_base_o(v2_ready_base), .ready_span_o(v2_ready_span),
    .swap_valid_i(v2_gpu_swap_valid_w), .swap_ready_o(v2_swap_ready),
    .swap_writer_i(zhao_fb_tuple_writer(v2_gpu_swap_tuple_w)),
    .swap_slot_i(zhao_fb_tuple_slot(v2_gpu_swap_tuple_w)),
    .swap_generation_i(zhao_fb_tuple_generation(v2_gpu_swap_tuple_w)),
    .swap_mode_i(zhao_fb_tuple_mode(v2_gpu_swap_tuple_w)),
    .swap_base_i(zhao_fb_tuple_base(v2_gpu_swap_tuple_w)),
    .swap_span_i(zhao_fb_tuple_span(v2_gpu_swap_tuple_w)),
    .displayed_valid_o(slot_displayed_v), .displayed_writer_o(),
    .displayed_slot_o(slot_displayed_s), .displayed_generation_o(),
    .displayed_mode_o(), .displayed_base_o(), .displayed_span_o(),
    .slot_state_o(v2_slot_state),
    .requests_accepted_o(v2_requests_accepted_o),
    .responses_accepted_o(v2_responses_accepted_o),
    .leases_granted_o(v2_leases_granted_o),
    .leases_refused_o(v2_leases_refused_o),
    .faults_latched_o(v2_faults_latched_o),
    .publications_o(v2_publications_o),
    .releases_o(v2_releases_o),
    .ready_events_o(v2_ready_events_o),
    .swaps_o(v2_swaps_o),
    .stale_events_o(slot_stale_events),
    .contentions_o(v2_contentions_o)
  );

  zhao_fb_ready_cdc_v2 u_fb_cdc (
    .gpu_clk(gpu_clk), .gpu_rst_n(rst_n),
    .vid_clk(vid_clk), .vid_rst_n(rst_n),
    .gpu_ready_valid_i(v2_ready_valid),
    .gpu_ready_ready_o(v2_cdc_ready_ready_w),
    .gpu_ready_tuple_i(v2_ready_tuple_w),
    .vid_ready_valid_o(v2_vid_ready_valid_w),
    .vid_ready_ready_i(v2_vid_ready_ready_w),
    .vid_ready_tuple_o(v2_vid_ready_tuple_w),
    .vid_swap_valid_i(v2_vid_swap_valid_w),
    .vid_swap_ready_o(v2_vid_swap_ready_w),
    .vid_swap_tuple_i(v2_vid_swap_tuple_w),
    .gpu_swap_valid_o(v2_gpu_swap_valid_w),
    .gpu_swap_ready_i(v2_swap_ready),
    .gpu_swap_tuple_o(v2_gpu_swap_tuple_w),
    .gpu_barrier_done_o(v2_gpu_barrier_done_w),
    .vid_barrier_done_o(v2_vid_barrier_done_w),
    .gpu_protocol_fault_o(v2_cdc_gpu_fault_w),
    // TIE: the video-domain twin, and it CANNOT join the fault OR the way its
    // GPU-domain sibling just did. `v2_fault_level_c` is a gpu_clk level; this
    // is a vid_clk one, and OR-ing it in would be the same CDC violation this
    // packet already made once -- the manager's GPU-domain `ready_valid_o`
    // wired to the bridge's video-domain input, which is why
    // `zhao_fb_ready_cdc_v2` exists at all. It needs a synchroniser of its
    // own, and that is owed work, not a decision to discard it.
    .vid_protocol_fault_o(),
    .ready_enqueued_o(), .ready_dequeued_o(),
    .swap_enqueued_o(), .swap_dequeued_o(),
    .ready_memory_level_o(), .swap_memory_level_o(),
    .gpu_idle_o(), .vid_idle_o()
  );

  zhao_video_ready_bridge_v2 u_ready_bridge (
    .gpu_clk(gpu_clk), .gpu_rst_n(rst_n),
    .vid_clk(vid_clk), .vid_rst_n(rst_n),
    .gpu_barrier_done_i(v2_gpu_barrier_done_w),
    .vid_barrier_done_i(v2_vid_barrier_done_w),
    .blank_cmd_i(blank_cmd_i), .blank_ack_o(blank_ack_o),
    .lease_open_o(v2_lease_open_w),
    .cdc_ready_valid_i(v2_vid_ready_valid_w),
    .cdc_ready_ready_o(v2_vid_ready_ready_w),
    .cdc_ready_tuple_i(v2_vid_ready_tuple_w),
    .cdc_swap_valid_o(v2_vid_swap_valid_w),
    .cdc_swap_ready_i(v2_vid_swap_ready_w),
    .cdc_swap_tuple_o(v2_vid_swap_tuple_w),
    .frame_slot_ready_o(frame_slot_ready_o),
    .frame_swap_valid_i(frame_swap_valid_i),
    .frame_swap_slot_i(frame_swap_slot_i),
    .scanout_ack_i(scanout_ack_i),
    // TIE: the pixel stream passes through the bridge so colour and timing
    // metadata share one register; this shell does not route pixels through it
    // yet -- VIDEO.SCANOUT composition is Packet J.
    .scanout_px_i(v2_px_quiet_w), .output_px_o(),
    .gpu_reset_released_o(), .vid_reset_released_o(),
    .pending_o(), .pending_tuple_o(), .echo_hold_o(),
    .blank_active_o(blank_active_o), .scanout_wait_o(),
    .unblank_candidate_o(), .unblank_tuple_o(),
    .unblank_echo_seen_o(), .unblank_scanout_seen_o()
  );
  /* verilator lint_on PINCONNECTEMPTY */

  assign lease_open_o = v2_lease_open_w;

  zhao_debug_frameblit u_frameblit (
    .clk                  (gpu_clk),
    .rst_n                (rst_n),
    .req_valid_i          (nb_req_valid),
    .req_ready_o          (nb_req_ready),
    .req_dst_slot_i       (r_blit_dst),
    .req_mode_i           (r_blit_mode),
    .req_src_i            (r_blit_src),
    .req_len_i            (r_blit_len),
    .req_crc_i            (r_blit_crc),
    .fb_lease_valid_i     (fb_lease_valid),
    .fb_lease_slot_i      (fb_lease_slot),
    .fb_lease_generation_i(fb_lease_gen),
    .release_valid_o      (blit_rel_v),
    .release_slot_o       (blit_rel_slot),
    .release_generation_o (blit_rel_gen),
    .publish_valid_o      (blit_pub_v),
    .publish_slot_o       (blit_pub_slot),
    .publish_generation_o (blit_pub_gen),
    .hps_req_o            (blit_hps_req),
    .hps_req_grant_i      (blit_hps_grant),
    .hps_rsp_i            (blit_hps_rsp),
    .guard_req_o          (blit_guard_req),
    .guard_rsp_i          (blit_guard_rsp),
    .guard_wdata_o        (blit_wdata),
    .guard_wvalid_o       (blit_wvalid),
    .guard_wready_i       (blit_wready),
    .guard_wlast_o        (blit_wlast),
    // Retirement is the VRAM arbiter's credit stream for the blit client. This
    // is the ONLY thing that means "the write landed"; the block's own issue
    // count says nothing about it.
    .retire_words_i       (client_rsp[1].credits),
    .done_o               (nb_done),
    .status_o             (nb_status),
    .blits_published_o    (blits_published),
    .blits_rejected_o     (blits_rejected)
  );

  assign blit_done   = nb_done;
  assign blit_status = nb_status;

  // THE N-CLIENT ARBITER (owner ruling R4), not the two-port wrapper, since the
  // TERRAIN.BUILD socket landed (2026-09-19, cmdmem packet). Index order IS
  // the priority law (`zhao_hps_arbiter_n` rule 7): CMD.DMA 0 and
  // DEBUG.FRAMEBLIT 1 keep exactly the ranks they had, and every socket client
  // sits BELOW them, as `spec/memory_rules.md` 5d's background class requires.
  // Clients 0 and 1 still see the same machine -- the wrapper this replaces is
  // this core at N=2.
  localparam int unsigned HPS_N = 2 + BUILD_HPS_N;
  zhao_hps_burst_req_t [HPS_N-1:0]       hn_req;
  logic                [HPS_N-1:0]       hn_grant;
  zhao_hps_burst_rsp_t [HPS_N-1:0]       hn_rsp;
  logic                [HPS_N-1:0][31:0] hn_bursts;
  logic                [HPS_N-1:1][31:0] hn_wait;
  /* verilator lint_off UNUSEDSIGNAL */
  logic                [31:0]            hn_pend_dropped;       // R55, see below
  logic                [HPS_N-1:0]       hn_pend_dropped_mask;
  /* verilator lint_on UNUSEDSIGNAL */

  always_comb begin
    hn_req[0] = dma_hps_req;
    hn_req[1] = blit_hps_req;
    for (int i = 0; i < int'(BUILD_HPS_N); i++) hn_req[2 + i] = build_hps_req_i[i];
  end
  assign dma_hps_grant  = hn_grant[0];
  assign blit_hps_grant = hn_grant[1];
  assign dma_hps_rsp    = hn_rsp[0];
  assign blit_hps_rsp   = hn_rsp[1];
  assign hps_arb_c0_bursts = hn_bursts[0];
  assign hps_arb_c1_bursts = hn_bursts[1];
  assign hps_arb_c1_wait   = hn_wait[1];
  always_comb begin
    for (int i = 0; i < int'(BUILD_HPS_N); i++) begin
      build_hps_grant_o[i] = hn_grant[2 + i];
      build_hps_rsp_o[i]   = hn_rsp[2 + i];
      build_hps_wait_o[i]  = hn_wait[2 + i];
    end
  end

  zhao_hps_arbiter_n #(
    .N (HPS_N)
  ) u_hps_arb (
    .clk        (gpu_clk),
    .rst_n      (rst_n),
    .req_i      (hn_req),
    .req_grant_o(hn_grant),
    // CMD.DMA (0) and DEBUG.FRAMEBLIT (1) never write HPS DDR, so their write
    // lanes stay zero, as in V1. The socket clients' lanes are their own (see
    // the socket's port comment for why no ready needs routing through here).
    .wr_valid_i ({build_hps_wr_valid_i, 2'b00}),
    .wr_data_i  ({build_hps_wr_data_i, 128'd0}),
    .wr_last_i  ({build_hps_wr_last_i, 2'b00}),
    .rsp_o      (hn_rsp),
    .b_req_o       (arb_hps_req),
    .b_req_grant_i (arb_bridge_grant),
    .b_wr_valid_o  (arb_wr_valid),
    .b_wr_data_o   (arb_wr_data),
    .b_wr_last_o   (arb_wr_last),
    .b_rsp_i       (arb_hps_rsp),
    .bursts_o      (hn_bursts),
    .wait_cycles_o (hn_wait),
    // Rule 6c / R55. Not a port of this shell: adding one here would put an
    // output on V2 that V1 does not have, which the paired differential then
    // has to declare sibling-only -- for a counter the socket clients cannot
    // move (every one of them is a holder; see the arbiter's rule 6c). The
    // reading that matters is `zhao_console_core`'s, on the four-client
    // instance whose clients are the real ones.
    .pend_dropped_o     (hn_pend_dropped),
    .pend_dropped_mask_o(hn_pend_dropped_mask)
  );

  zhao_hps_bridge u_bridge (
    .clk           (gpu_clk),
    .rst_n         (rst_n),
    .req           (arb_hps_req),
    .req_grant     (arb_bridge_grant),
    .wr_valid      (arb_wr_valid),
    .wr_data       (arb_wr_data),
    .wr_last       (arb_wr_last),
    .rsp           (arb_hps_rsp),
    .hps_req_valid (hps_req_valid_o),
    .hps_req_write (hps_req_write_o),
    .hps_req_addr  (hps_req_addr_o),
    .hps_req_len   (hps_req_len_o),
    .hps_req_grant (hps_req_grant_i),
    .hps_wr_valid  (hps_wr_valid_o),
    .hps_wr_data   (hps_wr_data_o),
    .hps_wr_last   (hps_wr_last_o),
    .hps_rd_valid  (hps_rd_valid_i),
    .hps_rd_data   (hps_rd_data_i),
    .hps_rd_last   (hps_rd_last_i),
    .frame_tick    (gpu_tick.pulse),
    .hps_bytes     (hps_bytes),
    .hps_bytes_shadow (hps_bytes_shadow),
    .hps_err_count (hps_err_count_o),
    .wr_ready (hb_wr_ready), .wr_early_beats (hb_wr_early)
  );
  assign build_hps_wr_ready_o = hb_wr_ready;

  /* verilator lint_off UNUSEDSIGNAL */
  logic unused_mem;
  assign unused_mem = ^vram_bytes ^ ^hps_bytes ^ scan_gv ^ blit_gv
                    ^ ^scan_gv_req ^ ^blit_gv_req ^ ctrl_refresh_pulse
                    ^ bridge_req_grant ^ blit_hps_grant ^ ^blit_hps_rsp
                    ^ ^hps_arb_c0_bursts ^ ^hps_arb_c1_bursts ^ ^hps_arb_c1_wait
                    ^ ^client_rsp[2] ^ ^client_rsp[3]
                    ^ ^client_rsp[4] ^ ^client_rsp[5]
                    ^ ^{client_rsp[0].credits}
                    ^ client_rsp[0].grant ^ client_rsp[1].grant
                    // `guard_wlast_o` marks the last beat of a guard request.
                    // The write queue does not need it: it enqueues four words
                    // per beat and the controller pops them on its own schedule,
                    // so nothing here has to know which beat was final.
                    ^ blit_wlast
                    // The render guard's trace-only outputs, sunk exactly
                    // as the scanout and blit guards' are. `render_wlast`
                    // is the burst's last beat, which the write queue does
                    // not need because the arbiter counts words.
                    ^ render_wlast ^ render_gv ^ ^render_gv_req
                    // The GEOMETRY guard's trace-only outputs, sunk the same
                    // way. They were MISSING: `lint_shell_top` passed on
                    // 2026-09-05 15:29 (build/Testing/Temporary logs) and was
                    // red on the two signals below when this terrain pass
                    // arrived, so `u_guard_geom` was composed in without its
                    // pulse and its latched request joining the sink that every
                    // other guard's do. `geom_gv_cnt` was already consumed by
                    // the violation total above; only these two were adrift.
                    // The slot manager's observability: exposed for tracing and
                    // for the counters, consumed by neither yet.
                    // THE V1 SYNTHETIC-FRAGMENT INPUTS, WHICH THE V2 BIN
                    // PIPE NO LONGER HAS. `zhao_geom_bin_pipe` took
                    // `job_fill_word_i`, `job_clear_word_i`, `job_state_i`,
                    // `job_src_a_i` and the three `job_texel_*` -- a
                    // synthetic fragment handed in from outside. The V2
                    // pipe gets that material through the Packet-D
                    // attribute path instead, so these seven shell ports
                    // have no consumer here.
                    //
                    // They are KEPT rather than deleted, because the
                    // Packet-H gate asks for an old/new differential on
                    // unaffected domains under paired traffic, and a
                    // sibling whose port list has drifted cannot be driven
                    // by the same harness. Sunk exactly as the guards'
                    // trace-only outputs are.
                    ^ ^render_fill_word_i ^ ^render_clear_word_i
                    ^ ^render_state_i ^ ^render_src_a_i
                    ^ ^render_texel_rgb_i ^ ^render_texel_a_i
                    ^ ^render_texel_idx_i
                    ^ ^v2_bin_quiet_w ^ ^v2_bin_initialized_w
                    ^ ^v2_bin_sequence_abort_w ^ ^v2_lease_fault
                    ^ ^v2_lease_mode ^ ^v2_lease_base ^ ^v2_lease_span
                    ^ slot_displayed_v ^ slot_displayed_s
                    ^ ^v2_slot_state[0] ^ ^v2_slot_state[1]
                    ^ ^slot_stale_events
                    ^ ^blits_published ^ ^blits_rejected
                    ^ geom_gv ^ ^geom_gv_req
                    // The slot-6 socket's guard trace outputs, sunk as every other
                    // guard's are; its violation COUNT is in the total above.
                    // uild_wlast_i is sunk for the write queue's reason: the
                    // arbiter counts words, so no beat has to be marked final.
                    ^ build_gv ^ ^build_gv_req ^ build_wlast_i
                    // Burst counts for the socket's HPS clients. Their WAIT counts
                    // leave the shell as uild_hps_wait_o; a count of bursts
                    // served is the composer's to read off its own client.
                    ^ ^hn_bursts[HPS_N-1:2]
                    // THE HPS BRIDGE'S WRITE READY, AND WHY IT IS UNUSED --
                    // which is a different statement from "it is spare".
                    //
                    // `98d7030e` added it because "the HPS bridge's write
                    // channel had no READY, so a beat offered a cycle early
                    // vanished". The bridge now raises it, the shell wires it
                    // out, AND NOTHING CONNECTS IT BACK TO THE PRODUCER. That
                    // is harmless today only because the producer cannot
                    // produce: `zhao_hps_arbiter`'s two client write ports are
                    // both tied off here -- `.c0_wr_valid_i(1'b0)` and
                    // `.c1_wr_valid_i(1'b0)` -- so `arb_wr_valid` is
                    // permanently low and no beat can be lost.
                    //
                    // THE TRAP IS FOR WHOEVER CONNECTS THE FIRST CLIENT.
                    // `zhao_hps_arbiter` has no `b_wr_ready_i` at all:
                    // `b_wr_valid_o` is a pure output (arbiter lines 116-118,
                    // driven combinationally at 147/162). So the bridge's
                    // READY cannot be honoured by wiring alone -- it needs a
                    // port on the arbiter and a stall in its write mux, and
                    // until that exists 98d7030e's repair is present in the
                    // bridge and INERT at the shell. A fix that reaches the
                    // signal but not the producer is a fix that has not
                    // landed, which is the same shape as an ignore rule that
                    // hides waste instead of removing it.
                    //
                    // SUPERSEDED 2026-09-19 (terrain3, entry I26). The ready now
                    // LEAVES the shell as `build_hps_wr_ready_o`, and the stall
                    // the paragraph above asks the arbiter for is supplied by the
                    // WRITERS instead: the bridge's level is high only while the
                    // ONE granted write burst streams, so its owner is the only
                    // client that can see it high with data to give, and both
                    // socket writers (TERRAIN.WRITEBACK's journal, PART.STATE's
                    // generation store) offer a beat only while it is high. The
                    // arbiter's pass-through write mux therefore never carries a
                    // beat the bridge refuses. `hb_wr_early` is the check on that
                    // claim -- beats offered while the level was low, zero for a
                    // compliant writer -- and it is still sunk, not exported: the
                    // one open end of this repair, named so it is not lost.
                    ^ ^hb_wr_early;
  /* verilator lint_on UNUSEDSIGNAL */

  // ==========================================================================
  // GLUE 4 + 5: blit tracking, slot-ready pendings, completion correlator
  // ==========================================================================
  // THE GUARD WINDOW IS THE LEASE. It used to be opened at dispatch and closed
  // at done, which meant the guard's idea of who owned a slot and the machine's
  // idea of it were two separate pieces of bookkeeping that merely happened to
  // agree. Now there is one: while a lease is live the guard admits writes to
  // that slot, and when it ends the window shuts with it.
  assign map_valid_q = fb_lease_valid;
  assign map_slot_q  = fb_lease_slot;
  assign map_span_q  = r_blit_len;

  // AND THE SAME LAW FOR THE OTHER WRITER. `lease_writer_o` is 1 for the
  // renderer, so this window exists only while the renderer's lease is live and
  // shuts with it -- the same sentence as the three lines above, read off the
  // slot manager's live record instead of the blitter's dispatch. See
  // `u_guard_render` for why it is the manager's record and not the renderer
  // lease's `frame_*` identity.
  assign rmap_valid_q = v2_lease_valid && v2_lease_writer;
  assign rmap_slot_q  = v2_lease_slot;
  assign rmap_span_q  = v2_lease_span;

  // Per-FB-slot completion toggles, now driven by a PUBLICATION the slot
  // manager accepted rather than by "the blitter said status zero". A blit that
  // finished and a blit whose slot is still ours are different facts, and only
  // the second one may put a frame on screen.
  logic [1:0] ready_tog;

  // Which FB slot the in-flight packet's blit targets. This is NOT the lease --
  // the lease ends when the blit publishes, while the completion correlator
  // fires at the frame tick, which is later. It is latched at dispatch exactly
  // as it always was, because the correlator's meaning has not changed.
  logic [0:0] run_blit_dst;

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      ready_tog    <= 2'b00;
      run_blit_dst <= 1'b0;
    end else begin
      if (dpy_blit_valid && blit_req_ready) run_blit_dst <= dpy_blit_dst[0];
      if (blit_pub_v) ready_tog[blit_pub_slot] <= ~ready_tog[blit_pub_slot];
    end
  end

  // THE VIDEO-DOMAIN HALF OF THE OLD SWAP TOGGLE IS GONE TOO. It existed only
  // to feed the GPU-side synchroniser deleted above; with the full tuple
  // crossing through zhao_fb_ready_cdc_v2, the video domain reports its swap
  // to the bridge as `frame_swap_valid_i` / `frame_swap_slot_i` and the bridge
  // returns the tuple it is holding. Two flops that encoded one bit of a
  // six-field record are not worth keeping for the shape.

  // THE V1 SWAP ECHO IS GONE, and what replaced it is the reason this whole
  // packet exists. A bare toggle plus a slot bit, three flops of
  // synchroniser, and an edge detector: it carried VALID and SLOT across the
  // boundary and nothing else -- no generation, no writer, no base or span.
  // The manager could not tell a swap of the frame it published from a swap
  // of the one before it.
  //
  // `zhao_fb_ready_cdc_v2` carries the complete 84-bit tuple both ways, so
  // the echo the manager matches against is the record it issued. The old
  // chain has no consumer and is deleted rather than left driving nothing.

  // vid domain: pending registers (2FF per toggle, set on edge, cleared
  // when FRAMECTL consumes the slot at the swap decision; set wins)
  logic [1:0] rt_s1, rt_s2, rt_s3;
  always_ff @(posedge vid_clk or negedge rst_n) begin
    if (!rst_n) begin
      rt_s1 <= 2'b00;
      rt_s2 <= 2'b00;
      rt_s3 <= 2'b00;
      slot_ready_pending <= 2'b00;
    end else begin
      rt_s1 <= ready_tog;
      rt_s2 <= rt_s1;
      rt_s3 <= rt_s2;
      for (int d = 0; d < 2; d++) begin
        if (rt_s2[d] != rt_s3[d]) begin
          slot_ready_pending[d] <= 1'b1;   // set wins (header note)
        end else if (swap_req && (swap_slot == 1'(d))) begin
          slot_ready_pending[d] <= 1'b0;
        end
      end
    end
  end

  // completion correlator (gpu): a fresh (non-repeat) tick displaying the
  // RUN slot's blit target completes that ring slot
  logic       any_run;
  logic [1:0] run_slot;
  always_comb begin
    any_run  = 1'b0;
    run_slot = 2'd0;
    for (int s = 0; s < 3; s++) begin
      if (slot_state_o[s] == 3'd3) begin   // S_RUN (charter encoding)
        any_run  = 1'b1;
        run_slot = 2'(s);
      end
    end
  end
  assign frame_complete = gpu_tick.pulse && !gpu_tick.repeated && any_run
                        && (gpu_complete_slot == run_blit_dst);
  assign frame_complete_slot = run_slot;

  // ==========================================================================
  // GLUE 6: mode CDC (gpu -> vid) with a 2-cycle stability filter
  // ==========================================================================
  logic [1:0] md_s1, md_s2, md_s3, md_cur;
  always_ff @(posedge vid_clk or negedge rst_n) begin
    if (!rst_n) begin
      md_s1 <= 2'd0;
      md_s2 <= 2'd0;
      md_s3 <= 2'd0;
      md_cur <= 2'd0;
      mode_we <= 1'b0;
      mode_in <= 2'd0;
    end else begin
      md_s1 <= 2'(sched_mode);
      md_s2 <= md_s1;
      md_s3 <= md_s2;
      mode_we <= 1'b0;
      if ((md_s2 == md_s3) && (md_s2 != md_cur)) begin
        md_cur  <= md_s2;
        mode_in <= md_s2;
        mode_we <= 1'b1;
      end
    end
  end

  // ==========================================================================
  // GLUE 7: the displayed CRC, now ENTIRELY INSIDE vid_clk (2026-08-22)
  // ==========================================================================
  // WHAT USED TO BE HERE, and why it had to go. The CRC ran on gpu_clk, so
  // this file sampled the vid-domain pixel register (px_out.valid, .rgb565,
  // .x, .y) from GPU logic on a vid phase toggle and unpacked it into two
  // gpu-cycle bytes. Per-pixel state crossed the clock boundary on every
  // active pixel, and it was correct only because the SIMULATION freezes
  // vid_clk = gpu_clk/2 with coincident posedges (plan R1). TimeQuest
  // measured what that costs in silicon: the two hold violations reported on
  // `vid_clk -> gpu_clk` under the HIGH PERFORMANCE fitter effort were both
  // on this seam, and a hold violation is not a speed problem — no clock is
  // slow enough to fix data that arrives too early.
  //
  // Fabian's ruling (docs/OWNER_DOCKET.md, "RULED 2026-08-22 — BALANCED stays
  // authoritative"): move the displayed CRC into vid_clk rather than crossing
  // per-pixel state. That also settled a standing contradiction —
  // design/contracts/DEBUG.CRC.md had always said "`vid_clk` domain for the
  // displayed-stream lane", design/blocks.yml said `clock_domain: gpu`, and
  // the code had followed the ledger. The contract was right.
  //
  // What crosses now: nothing per-pixel. `zhao_debug_crc` sits in vid_clk and
  // consumes the serializer's stream natively — one pixel per vid clock, two
  // bytes folded in one XOR tree. Only the FINALISED result crosses, once per
  // displayed frame, as a toggle with its value held stable beside it for the
  // whole frame that follows: hundreds of thousands of clocks of settling
  // against a three-flop synchronizer, instead of sixteen data bits re-timed
  // every pixel.

  // ---- vid side: framing markers, from vid-domain signals only ------------
  logic [15:0] px_active_w;
  assign px_active_w = ZHAO_TIMING[vmode].h_active;

  logic px_is_sof, px_is_eof;
  assign px_is_sof = px_out.valid && (px_out.x == 10'd0) && (px_out.y == 8'd0);
  assign px_is_eof = px_out.valid && (px_out.x == 10'(px_active_w - 16'd1))
                     && (px_out.y == 8'd239);

  // expect_bytes is zhao_displayed_bytes(mode) — NOT zhao_canvas_bytes: for
  // Duo those differ (245,760 displayed vs 196,608 stored) and the canvas
  // value here would be the documented silent-Duo bug.
  logic [31:0] crc_expect_bytes;
  assign crc_expect_bytes = zhao_displayed_bytes(vmode);

  logic [31:0] crc_frame_vid, crc_bytes_vid;
  logic        crc_valid_vid, crc_err_vid;

  zhao_debug_crc u_crc (
    .clk               (vid_clk),
    .rst_n             (rst_n),
    .in_valid_i        (px_out.valid),
    .in_px_i           (px_out.rgb565),
    .in_sof_i          (px_is_sof),
    .in_eof_i          (px_is_eof),
    .expect_bytes_i    (crc_expect_bytes),
    .frame_crc_o       (crc_frame_vid),
    .frame_crc_valid_o (crc_valid_vid),
    .bytes_captured_o  (crc_bytes_vid),
    .size_err_evt_o    (crc_err_vid)
  );

  // ---- the only crossing left on this lane: vid -> gpu, once per frame ----
  // A publish and a size-error each toggle their own bit; the reported values
  // register beside them and then hold. Publish and size-error are mutually
  // exclusive in the block (a frame either finalizes or is flagged), but they
  // are carried separately so a burst of raster violations cannot be mistaken
  // for a published frame.
  logic        crc_pub_tog_vid, crc_err_tog_vid;
  logic [31:0] crc_frame_hold, crc_bytes_hold;
  always_ff @(posedge vid_clk or negedge rst_n) begin
    if (!rst_n) begin
      crc_pub_tog_vid <= 1'b0;
      crc_err_tog_vid <= 1'b0;
      crc_frame_hold  <= 32'd0;
      crc_bytes_hold  <= 32'd0;
    end else begin
      if (crc_valid_vid) begin
        crc_pub_tog_vid <= ~crc_pub_tog_vid;
        crc_frame_hold  <= crc_frame_vid;
        crc_bytes_hold  <= crc_bytes_vid;
      end
      if (crc_err_vid) begin
        crc_err_tog_vid <= ~crc_err_tog_vid;
        crc_bytes_hold  <= crc_bytes_vid;
      end
    end
  end

  // gpu side: 3FF per toggle, edge-detected into a ONE-gpu-cycle pulse. The
  // held value is sampled at the same edge as the pulse it belongs to, so the
  // port pair {crc_valid_o, crc_frame_o} stays aligned for a consumer that
  // reads both after a gpu edge (which is what the harness does).
  logic cp_g1, cp_g2, cp_g3, ce_g1, ce_g2, ce_g3;
  logic crc_pub_edge, crc_err_edge;
  assign crc_pub_edge = (cp_g2 != cp_g3);
  assign crc_err_edge = (ce_g2 != ce_g3);

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      cp_g1 <= 1'b0; cp_g2 <= 1'b0; cp_g3 <= 1'b0;
      ce_g1 <= 1'b0; ce_g2 <= 1'b0; ce_g3 <= 1'b0;
      crc_frame_o    <= 32'd0;
      crc_bytes_o    <= 32'd0;
      crc_valid_o    <= 1'b0;
      crc_size_err_o <= 1'b0;
    end else begin
      cp_g1 <= crc_pub_tog_vid;
      cp_g2 <= cp_g1;
      cp_g3 <= cp_g2;
      ce_g1 <= crc_err_tog_vid;
      ce_g2 <= ce_g1;
      ce_g3 <= ce_g2;
      crc_valid_o    <= crc_pub_edge;
      crc_size_err_o <= crc_err_edge;
      if (crc_pub_edge) crc_frame_o <= crc_frame_hold;
      if (crc_pub_edge || crc_err_edge) crc_bytes_o <= crc_bytes_hold;
    end
  end

  // ==========================================================================
  // GLUE 3: record framer (packet byte stream -> scheduler record port)
  // ==========================================================================
  localparam int unsigned FQW = $clog2(FRAMER_Q);

  logic [31:0] f_pos;      // byte index within the packet
  logic [15:0] f_op;       // current record: opcode
  logic [15:0] f_len;      // current record: record_bytes
  logic [15:0] f_rpos;     // byte index within the current record
  logic [127:0] f_w;       // payload dwords w0..w3 (record bytes [16,32))

  logic [143:0] recq [0:FRAMER_Q-1];   // {op, w3, w2, w1, w0}
  logic [FQW:0] rq_wp, rq_rp;
  logic [FQW:0] rq_occ;
  assign rq_occ = rq_wp - rq_rp;

  logic rq_full, framer_err;
  assign rq_full = (rq_occ >= (FQW+1)'(FRAMER_Q));

  // the byte on the wires completes a record exactly when its position is
  // the record's last byte (f_len valid from byte 4 on; records are >=16 B)
  logic in_rec_region, rec_completes_now;
  // `pkt_len - 32'd4` USED TO BE COMPUTED HERE, in series with the comparison
  // that gates the record write. MEASURED on the composed shell fit of
  // 2026-08-24: the `f_pos -> recq[*]` family is the largest group of failing
  // setup endpoints on gpu_clk (~-0.765 ns at ~10.6 ns of data delay), and this
  // is a 32-bit subtract standing directly in front of a 32-bit compare whose
  // result is the write enable of a 144-bit array.
  //
  // EXACT, not approximately equal. `f_pos` resets to 0 whenever
  // `f_pos + 1 >= pkt_len`, so `f_pos < pkt_len` always holds, and
  // `in_rec_region` additionally requires `f_pos >= 36`. A packet therefore
  // cannot reach the region until 36 accepted bytes after its own first byte,
  // and a copy of `pkt_len - 4` that lags by ONE cycle has been correct for 35
  // cycles by the time anything consults it. The lag is only observable across
  // a packet boundary, which is exactly where the region test is false anyway.
  //
  // Underflow semantics are preserved deliberately: the registered expression
  // is the same expression, so a `pkt_len < 4` wraps identically to before.
  //
  // ENFORCED-BY: tests/shell/shell_golden.cpp:main
  logic [31:0] pkt_len_m4_q;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) pkt_len_m4_q <= 32'd0;
    else        pkt_len_m4_q <= pkt_len - 32'd4;
  end

  assign in_rec_region = (f_pos >= 32'd36) && (f_pos < pkt_len_m4_q);
  assign rec_completes_now = in_rec_region && (f_rpos >= 16'd4)
                           && (f_rpos + 16'd1 == f_len);

  // stall the LAST byte of a record while the queue is full (glue 3 law), AND
  // honour the re-exported stream's second consumer.  A byte is accepted only
  // when BOTH this inline framer and whatever sits on `cmd_pkt_*` can take it,
  // which is what makes the export a fork rather than a tap: a tap would let
  // this framer advance past a byte the other consumer never saw.
  assign pkt_ready = !(rec_completes_now && rq_full) && cmd_pkt_ready_i;

  assign cmd_pkt_valid_o = pkt_valid;
  assign cmd_pkt_byte_o  = pkt_byte;
  assign cmd_pkt_len_o   = pkt_len;

  // the record's payload dwords INCLUDING the byte on the wires this cycle
  logic [127:0] w_final;
  always_comb begin
    w_final = f_w;
    if ((f_rpos >= 16'd16) && (f_rpos < 16'd32))
      w_final[8*(f_rpos - 16'd16) +: 8] = pkt_byte;
  end

  assign rec_valid  = (rq_occ != '0);
  assign rec_opcode = recq[rq_rp[FQW-1:0]][143:128];
  assign rec_w0     = recq[rq_rp[FQW-1:0]][31:0];
  assign rec_w1     = recq[rq_rp[FQW-1:0]][63:32];
  assign rec_w2     = recq[rq_rp[FQW-1:0]][95:64];
  assign rec_w3     = recq[rq_rp[FQW-1:0]][127:96];

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      f_pos  <= 32'd0;
      f_op   <= 16'd0;
      f_len  <= 16'd0;
      f_rpos <= 16'd0;
      f_w    <= 128'd0;
      rq_wp  <= '0;
      rq_rp  <= '0;
      framer_err <= 1'b0;
    end else begin
      // pop (presentation side)
      if (rec_valid && rec_ready) rq_rp <= rq_rp + (FQW+1)'(1);

      // deadlock tripwire: the stream is stalled on a full queue while the
      // head cannot drain because the scheduler is backpressuring — a
      // packet shape the composition cannot execute (header, glue 3)
      if (pkt_valid && !pkt_ready && rec_valid && !rec_ready)
        framer_err <= 1'b1;

      // consume (byte side)
      if (pkt_valid && pkt_ready) begin
        if (in_rec_region) begin
          if (f_rpos == 16'd0) begin
            f_w        <= 128'd0;
            f_op[7:0]  <= pkt_byte;
          end else if (f_rpos == 16'd1) begin
            f_op[15:8] <= pkt_byte;
          end else if (f_rpos == 16'd2) begin
            f_len[7:0] <= pkt_byte;
          end else if (f_rpos == 16'd3) begin
            f_len[15:8] <= pkt_byte;
          end else if ((f_rpos >= 16'd16) && (f_rpos < 16'd32)) begin
            f_w[8*(f_rpos - 16'd16) +: 8] <= pkt_byte;
          end
          if (rec_completes_now) begin
            // push {op, w} — w_final patches THIS byte in when the final
            // byte itself lands inside [16,32) (exactly the 32-B records);
            // a 16-B record's w stays the zero-fill (NOP has no payload)
            recq[rq_wp[FQW-1:0]] <= {f_op, w_final};
            rq_wp  <= rq_wp + (FQW+1)'(1);
            f_rpos <= 16'd0;
          end else begin
            f_rpos <= f_rpos + 16'd1;
          end
        end
        // position bookkeeping (header/tail bytes just count)
        if (f_pos + 32'd1 >= pkt_len) begin
          f_pos  <= 32'd0;
          f_rpos <= 16'd0;
        end else begin
          f_pos <= f_pos + 32'd1;
        end
      end
    end
  end
  assign shell_err_framer_o = framer_err;

  // ==========================================================================
  // INPUT: snapshot + rumble (gpu domain, tick-latched)
  // ==========================================================================
  zhao_pad_frame_t pad_frame [0:3];
  logic [31:0] pad_frame_id;
  logic        input_gap_evt;

  zhao_input_snapshot u_snapshot (
    .clk         (gpu_clk),
    .rst_n       (rst_n),
    .pad_present (pad_present_i),
    .pad_buttons (pad_buttons_i),
    .pad_lx      (pad_lx_i),
    .pad_ly      (pad_ly_i),
    .pad_rx      (pad_rx_i),
    .pad_ry      (pad_ry_i),
    .frame_tick  (gpu_tick),
    .pad_frame   (pad_frame),
    .pad_frame_flat (pad_frame_flat_o),
    .pad_frame_id   (pad_frame_id),
    .pad_sequence   (pad_sequence_o),
    .input_sequence_gaps   (input_gaps_o),
    .input_sequence_gap_evt(input_gap_evt)
  );

  // rumble edge converter (glue 9): one pulse per NEW dispatch
  logic        rum_prev_v;
  logic [23:0] rum_prev_pl;
  logic        rum_pulse;
  assign rum_pulse = dpy_rumble_valid
                   && (!rum_prev_v
                       || (rum_prev_pl != {dpy_rumble_pad, dpy_rumble_en,
                                           dpy_rumble_str}));
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      rum_prev_v  <= 1'b0;
      rum_prev_pl <= 24'd0;
    end else begin
      rum_prev_v  <= dpy_rumble_valid;
      if (dpy_rumble_valid)
        rum_prev_pl <= {dpy_rumble_pad, dpy_rumble_en, dpy_rumble_str};
    end
  end

  zhao_input_rumble u_rumble (
    .clk              (gpu_clk),
    .rst_n            (rst_n),
    .rumble_cmd_valid (rum_pulse),
    .rumble_pad_index (dpy_rumble_pad),
    .rumble_enable    (dpy_rumble_en),
    .rumble_strength  (dpy_rumble_str),
    .frame_tick       (gpu_tick),
    .rumble_duty      (rumble_duty_o),
    .rumble_active    (rumble_active_o),
    .rumble_pwm       (rumble_pwm_o),
    .rumble_frames_dropped (rumble_drops_o)
  );

  /* verilator lint_off UNUSEDSIGNAL */
  logic unused_input;
  assign unused_input = input_gap_evt ^ ^pad_frame_id
                      ^ ^{pad_frame[0], pad_frame[1], pad_frame[2], pad_frame[3]};
  /* verilator lint_on UNUSEDSIGNAL */

  // ==========================================================================
  // AUDIO: the D4 FIFO (write side fed by the harness ring-read seam)
  // ==========================================================================
  zhao_counter_snap_t fifo_snap;

  zhao_audio_fifo u_fifo (
    .clk_gpu          (gpu_clk),
    .rst_gpu_n        (rst_n),
    .wr_valid_i       (aud_wr_valid_i),
    .wr_l_i           (aud_wr_l_i),
    .wr_r_i           (aud_wr_r_i),
    .wr_ready_o       (aud_wr_ready_o),
    .refill_req_o     (aud_refill_req_o),
    .occupancy_o      (aud_occupancy_o),
    .frame_tick_i     (gpu_tick.pulse),
    .cnt_snap_o       (fifo_snap),
    .clk_audio        (audio_clk),
    .rst_audio_n      (rst_n),
    .pcm_valid_o      (pcm_valid_o),
    .pcm_l_o          (pcm_l_o),
    .pcm_r_o          (pcm_r_o),
    .underrun_status_o(underrun_status_o),
    .audio_underruns_o(audio_underruns_o)
  );

  // ==========================================================================
  // GLUE 8: counter provider adapters + DEBUG.COUNTERS
  // ==========================================================================
  logic tick_d1;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) tick_d1 <= 1'b0;
    else        tick_d1 <= gpu_tick.pulse;
  end

  // ---- the starvation crossing, as a SNAPSHOT MAILBOX ---------------------
  // This used to sample the whole 64-bit vid_clk counter straight into gpu_clk
  // and check AFTERWARDS whether it had moved:
  //
  //     starve_samp <= starvation_o;
  //     if (tick_d1 && (starvation_o != starve_samp)) cdc_err <= 1'b1;
  //
  // The premise -- the counter only advances during active lines, the tick
  // lands in vblank -- was reasonable. The STRUCTURE was not: a value
  // comparison can notice that a bus moved, it cannot make a metastable sample
  // safe, and it says nothing about the 62 bits that did not move.
  //
  // MEASURED, and this is why it changed. Across FOUR composed fits that
  // touched nothing in this path, the crossing's hold slack read
  //
  //     -0.952 FAIL   +0.254 pass   +0.259 pass   -0.728 FAIL
  //
  // 1.2 ns of swing on placement alone, which made the shell's timing verdict
  // NONDETERMINISTIC -- worse than permanently red, because a verdict that
  // flips at random cannot be trusted in either direction and a real
  // regression arriving on a lucky fit is indistinguishable from luck. It had
  // already hidden one: the true worst synchronous path (-0.875 ns) sat behind
  // this crossing's -1.991.
  //
  // Published once per VID-DOMAIN frame, which is the rate the consumer wants:
  // `prov[6]` samples at `tick_d1`, once per frame.
  //
  // ENFORCED-BY: tests/formal/cdc_snapshot.sby
  logic [63:0] starve_snap;
  logic        starve_snap_valid;
  logic        starve_ovf;
  // Named rather than left empty: this lane forbids empty-by-name pin
  // connections, and the signal is genuinely observable -- it says the video
  // side is holding a snapshot the GPU side has not taken yet.
  logic        starve_busy;

  zhao_cdc_snapshot #(.W(64)) u_starve_mbx (
      .src_clk      (vid_clk),
      .src_rst_n    (rst_n),
      .src_publish_i(frame_tick_vid),
      .d_i          (starvation_o),
      .src_busy_o   (starve_busy),
      .ovf_o        (starve_ovf),
      .dst_clk      (gpu_clk),
      .dst_rst_n    (rst_n),
      .q_o          (starve_snap),
      .q_valid_o    (starve_snap_valid)
  );

  // The error bit now means something a person can act on: the GPU side failed
  // to collect a snapshot before the video side had another to publish. The
  // old bit meant "the counter moved while I happened to be looking", which is
  // a statement about luck.
  assign shell_err_cdc_o = starve_ovf;

  // summed byte counters (u64; each shadow saturates at u32 individually)
  logic [63:0] vram_total, hps_total;
  always_comb begin
    vram_total = 64'd0;
    hps_total  = 64'd0;
    // BOTH BOUNDS ARE SEVEN NOW, and the note that used to sit here was right
    // about the danger and wrong about the date. It said the HPS arbiter "still
    // has five", which was true until TERRAIN.PAGELOADER and TERRAIN.WRITEBACK
    // began moving bytes as ZHAO_CLIENT_TERRAIN_BUILD = 6 over the HPS bridge.
    // `hps_bytes[6]` was then an out-of-range write, which SystemVerilog
    // DISCARDS SILENTLY -- so the counter did not merely stop at five, it read
    // zero for the two blocks that use it, while the transfers themselves
    // worked perfectly.
    //
    // Exactly the failure the old note named -- "a total that reads LOW" -- and
    // it arrived by the bound being correct when written and never revisited
    // when the client set grew.
    for (int k = 0; k < 7; k++) vram_total = vram_total + {32'd0, vram_bytes_shadow[k]};
    for (int k = 0; k < 7; k++) hps_total  = hps_total  + {32'd0, hps_bytes_shadow[k]};
  end

  // ---- GEOM.BINNER's SHADOWS (GIANTREFS, 2026-09-26) ---------------------
  // `spec/counters.md` 3.2's protocol: every provider latches its shadow
  // REGISTERS at the frame_tick edge and presents them with a one-cycle valid
  // on the cycle AFTER, which `tick_d1` is. The binner's counters are
  // free-running saturating totals on `gpu_clk` -- the same domain as
  // `u_counters` -- so the crossing is a latch, not a synchroniser, and the
  // shadow is what makes the value STABLE all frame as the contract requires.
  //
  // These are the first two of the six binner instruments this shell used to
  // discard. They are published at the ids `design/blocks.yml` gives
  // GEOM.BINNER and gives no other block, so owner ruling R19 -- "no counter id
  // is shared across emitters" -- holds by construction rather than by care.
  logic [31:0] bin_refs_shadow_q;
  logic [15:0] bin_depth_shadow_q;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      bin_refs_shadow_q  <= 32'd0;
      bin_depth_shadow_q <= 16'd0;
    end else if (gpu_tick.pulse) begin
      bin_refs_shadow_q  <= v2_bin_refs_w;
      bin_depth_shadow_q <= v2_bin_depth_w;
    end
  end

  zhao_counter_snap_t prov [0:10];
  always_comb begin
    prov[0] = sched_snap_cycles;                                  // id 0
    prov[1] = sched_snap_faults;                                  // id 1
    prov[2] = sched_snap_cmds;                                    // id 2
    prov[3] = fifo_snap;                                          // id 31
    prov[4] = '{valid: tick_d1, counter_id: ZHAO_CNT_VRAM_BYTES,
                value: vram_total};                               // id 28
    prov[5] = '{valid: tick_d1, counter_id: ZHAO_CNT_HPS_BYTES,
                value: hps_total};                                // id 29
    // READS THE SNAPSHOT, NOT THE CROSSING. This line used to take
    // `starvation_o` -- a vid_clk value -- directly on a gpu_clk tick, so the
    // counter itself sampled across the domain boundary and the tripwire
    // above only watched it happen. `starve_snap_valid` gates the first
    // frame, before any snapshot has been collected, to zero rather than to
    // an undefined mailbox.
    prov[6] = '{valid: tick_d1, counter_id: ZHAO_CNT_SCANOUT_STARVE,
                value: starve_snap_valid ? starve_snap : 64'd0};  // id 30
    prov[7] = '{valid: tick_d1, counter_id: ZHAO_CNT_INPUT_SEQ_GAPS,
                value: input_gaps_o};                             // id 35
    prov[8] = '{valid: tick_d1, counter_id: ZHAO_CNT_RUMBLE_DROPPED,
                value: rumble_drops_o};                           // id 36
    // GEOM.BINNER, at the two catalog ids it owns inside the 40-id window.
    // `tile_references` is the number R7's giant guarantee is ABOUT: 32,768 of
    // them reserved, and until this shell raised RENDER_CHUNKS the composed
    // binner could hold 1,024. A console that cannot report the figure cannot
    // report the breach either, which is how the guarantee came to be violated
    // in the shipped composition without anything going red.
    prov[9]  = '{valid: tick_d1, counter_id: ZHAO_CNT_TILE_REFERENCES,
                 value: {32'd0, bin_refs_shadow_q}};              // id 18
    prov[10] = '{valid: tick_d1, counter_id: ZHAO_CNT_MAX_TILE_LIST_DEPTH,
                 value: {48'd0, bin_depth_shadow_q}};             // id 19
  end

  zhao_counter_snap_t cnt_snap;

  zhao_debug_counters #(
    .PROV_N      (11),
    .CATALOG_IDS (40)
  ) u_counters (
    .clk             (gpu_clk),
    .rst_n           (rst_n),
    .frame_tick_i    (gpu_tick),
    .prov_i          (prov),
    .snap_valid_o    (cnt_snap_valid_o),
    .snap_ready_i    (cnt_snap_ready_i),
    .snap_o          (cnt_snap),
    .window_open_o   (cnt_window_open_o),
    .cat_violation_o (cnt_cat_violation_o)
  );

  assign cnt_snap_id_o    = cnt_snap.counter_id;
  assign cnt_snap_value_o = cnt_snap.value;

  /* verilator lint_off UNUSEDSIGNAL */
  logic unused_cnt;
  assign unused_cnt = cnt_snap.valid;   // window-open is the level law
  /* verilator lint_on UNUSEDSIGNAL */

endmodule : zhao_shell_top_v2
