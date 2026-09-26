// tb_zhao_twod_chain.sv -- CMD.DECODER, CMD.EXEC, TWOD.CMD, TWOD.ASSET,
// TWOD.PLANE, TWOD.BAND, TWOD.SPRITE, TWOD.SAMPLER and POST.COMPOSITE, on one
// forked byte stream, with ONE HPS bridge port and ONE composited pixel stream.
//
// ===========================================================================
// WHY THIS HARNESS EXISTS AT ALL
// ===========================================================================
// The owner's completion ruling of 2026-09-22, item 3, names its acceptance
// test and then says what would NOT satisfy it:
//
//   "A command-fed two-player HUD, text/glyph sprites, overlapping ordered
//    sprites, legal planes, a disabled/empty next frame, malformed descriptors
//    and whole-sprite overflow. EXERCISE ACTUAL ASSET READS AND FINAL
//    COMPOSITED PIXELS WITHOUT DESCRIPTOR OR TEXEL INJECTION AT A DOWNSTREAM
//    BOUNDARY."
//
// and, in the completion-report section:
//
//   "An otherwise green smoke whose upstream fixture never reaches the new path
//    does not prove the path."
//
// So every descriptor in `twod_cmd_chain_directed.cpp` enters as BYTES of a
// command packet and every texel enters as BYTES of an HPS arena. The only
// things this file's C++ driver touches are the packet stream, the bridge and
// the composited output.
//
// ===========================================================================
// IT IS A WRAPPER, NOT A COPY
// ===========================================================================
// Every block below is the production module. `tools/budget/
// mutant_copy_drift.py` excludes wrappers for exactly this reason -- there is
// no body here to go stale. What this file adds is the FORK, the flattened
// bridge port and the constant look values, and nothing else.
//
// THE FORK IS `zhao_console_core.sv`'s OWN EXPRESSION. `pkt_ready_o` is the AND
// of both consumers' readies. A tap instead of a fork lets one consumer advance
// past a byte the other never saw, and every counter on both sides still
// balances -- `tb_cmd_exec_pair.sv` says this at length and it is the same
// hazard here. If the composer's expression changes, this must change with it.
//
// ===========================================================================
// THE BRIDGE PORT IS FLATTENED ON PURPOSE
// ===========================================================================
// `zhao_hps_burst_req_t` and `_rsp_t` are packed structs, and a C++ driver
// writing a 67-bit packed struct through `VlWide` is a bit-position argument
// that nobody can check by reading. Flattening them here makes the bridge model
// ordinary signals, and the flattening is four `assign`s that a reviewer can
// verify against `zhao_pkg.sv` in one look.
//
// ===========================================================================
// THE LOOK IS CONSTANT AND THAT IS DELIBERATE
// ===========================================================================
// `grade_valid_i = 0`, `bloom_gain_i = 0`, `flash_amt_i = 0`, and both gather
// planes report absent. So a composited pixel is the world colour, the
// atmosphere over it, and the HUD replacing both -- which is exactly the three
// things this packet built a producer for, with nothing else able to change a
// bit of it. A bench whose grading table moved could not say which stage a
// wrong colour came from.

`default_nettype none

module tb_zhao_twod_chain #(
    // SMALL ON PURPOSE. The console runs 384 x 240; this runs 64 x 32, which is
    // 2,048 pixels per frame instead of 92,160 and makes a ten-frame case a
    // second rather than a minute. What must NOT shrink with it is the band's
    // B and L -- the bucket's drain rate is LINE_W pixels per scanned LINE, so
    // narrowing the line already narrows the drain, and narrowing the store as
    // well would change R235's admission arithmetic into something this bench's
    // numbers could not be read against.
    parameter int unsigned LINE_W     = 64,
    // SIXTEEN ROWS, WHICH IS EXACTLY THE BAND STORE'S OWN HEIGHT (L = 16), and
    // that is the one parameter choice here with a consequence worth stating.
    // A frame no taller than the store fits in its four slots, so the filler
    // completes the WHOLE frame in the gap between the tick and the pass -- the
    // gap the console gives it for free while render and resolve run.
    //
    // WHAT THAT PUTS OUT OF THIS FILE'S REACH is the mid-sweep refill, where
    // `zhao_twod_sampler`'s single sample pipe has to serve the plane walk and
    // the sprite at the same time. At one composited pixel per clock the walk
    // takes every cycle and the HUD starves; the console's pass spends about
    // NINE clocks per output pixel (`zhao_twod_band`'s own header), so it has
    // eight spare. Driving this bench at one pixel per clock and also demanding
    // a mid-sweep refill would measure a console nobody has. Recorded in
    // FINDINGS rather than hidden behind a passing test.
    parameter int unsigned MAX_H      = 16,
    parameter int unsigned MAX_DESC   = 16,
    parameter int unsigned PAGE_WORDS = 1024,
    parameter int unsigned PAL_SLOTS  = 4,
    parameter int unsigned BIND_SLOTS = 8,
    parameter int unsigned PAGE_SLOTS = 8
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- the sealed packet byte stream, as CMD.DMA presents it -------------
    // CMD.DECODER IS ONE-SHOT: its S_DONE holds the verdict until reset, and
    // `zhao_console_core.sv` drives it from the console's own `rst_n`. A bench
    // that wants a SECOND packet -- which any multi-frame case needs, because
    // descriptors are re-sent per frame -- must therefore re-arm it, and doing
    // that with the chain's `rst_n` would also empty the page store and undo
    // the asset load. So the decoder's reset is its own port here.
    //
    // IT IS A BENCH FACILITY AND NOT A DESIGN CHANGE. Nothing in `fpga/rtl`
    // gained a port; the composer still ties the decoder to `rst_n`, which is
    // a console-level limitation this packet found and did not create.
    input  var logic        dec_rst_n,
    input  var logic        pkt_valid_i,
    output var logic        pkt_ready_o,
    input  var logic [ 7:0] pkt_byte_i,
    input  var logic [31:0] pkt_len_i,
    output var logic        decode_done_o,
    output var logic [ 7:0] decode_error_o,
    output var logic [31:0] decode_commands_o,

    // ---- the HPS bridge, flattened ----------------------------------------
    output var logic        hps_req_valid_o,
    output var logic [31:0] hps_req_addr_o,
    output var logic [ 6:0] hps_req_len_o,
    input  var logic        hps_grant_i,
    input  var logic        hps_beat_valid_i,
    input  var logic [63:0] hps_data_i,
    input  var logic        hps_last_i,
    input  var logic        hps_err_i,

    // ---- the frame ---------------------------------------------------------
    input  var logic                        frame_start_i,
    input  var logic [$clog2(LINE_W+1)-1:0] frame_w_i,
    input  var logic [$clog2(MAX_H +1)-1:0] frame_h_i,
    input  var logic [$clog2(MAX_H +1)-1:0] view_split_i,
    input  var logic                        view_sel_i,
    input  var logic [15:0]                 cfg_epoch_i,

    // ---- the resolved world, straight in ----------------------------------
    input  var logic        s_valid_i,
    output var logic        s_ready_o,
    input  var logic [15:0] s_rgb_i,

    // ---- the composited pixels, straight out -------------------------------
    output var logic                        o_valid_o,
    input  var logic                        o_ready_i,
    output var logic [15:0]                 o_rgb_o,
    output var logic [$clog2(LINE_W+1)-1:0] o_x_o,
    output var logic [$clog2(MAX_H +1)-1:0] o_y_o,
    output var logic                        o_last_o,

    // ---- the evidence the driver reads -------------------------------------
    output var logic [31:0] tc_planes_staged_o,
    output var logic [31:0] tc_sprites_staged_o,
    output var logic [31:0] tc_plane_refused_o,
    output var logic [31:0] tc_sprite_refused_o,
    output var logic [31:0] tc_list_overflow_o,
    output var logic [31:0] tc_packets_committed_o,
    output var logic [31:0] tc_packets_abandoned_o,
    output var logic [31:0] tc_frames_sealed_o,
    output var logic [31:0] tc_planes_published_o,
    output var logic [31:0] tc_sprites_published_o,
    output var logic [31:0] tc_slots_auto_disabled_o,
    output var logic [31:0] tc_bind_conflict_o,
    output var logic [31:0] tc_seal_overrun_o,

    output var logic [31:0] ta_loads_started_o,
    output var logic [31:0] ta_loads_done_o,
    output var logic [31:0] ta_words_written_o,
    output var logic [31:0] ta_slot_refused_o,
    output var logic [31:0] ta_len_refused_o,
    output var logic [31:0] ta_addr_refused_o,
    output var logic [31:0] ta_epoch_refused_o,
    output var logic [31:0] ta_crc_fails_o,
    output var logic [31:0] ta_regions_zeroed_o,
    output var logic [31:0] ta_bridge_errs_o,
    output var logic [31:0] ta_loads_during_pass_o,
    output var logic [31:0] ta_bursts_o,

    output var logic [31:0] ex_twod_planes_staged_o,
    output var logic [31:0] ex_twod_sprites_staged_o,
    output var logic [31:0] ex_twod_dropped_o,
    output var logic [31:0] ex_twod_loads_issued_o,
    output var logic [31:0] ex_uploads_issued_o,
    output var logic [31:0] ex_unsupported_o,

    output var logic [31:0] band_descriptors_o,
    output var logic [31:0] band_desc_overflow_o,
    output var logic [31:0] band_sprites_admitted_o,
    output var logic [31:0] band_sprites_refused_budget_o,
    output var logic [31:0] band_pixels_written_o,
    output var logic [31:0] band_underrun_o,
    output var logic [31:0] band_desc_mid_sweep_o,
    output var logic [31:0] band_order_inversion_o,
    output var logic [31:0] band_bands_o,
    // DIAGNOSTIC TAPS, and they are hierarchical READS of the band's own
    // state -- no production port, no tie-off, nothing the design can see.
    // They exist because a stalled filler looks identical from the outside to
    // a filler that has nothing to do.
    output var logic [7:0]  dbg_fill_band_o,
    output var logic [7:0]  dbg_rd_band_o,
    output var logic [7:0]  dbg_scan_st_o,
    output var logic [7:0]  dbg_outstanding_o,
    output var logic        tc_publishing_o,

    output var logic [31:0] plane_pixels_o,
    output var logic [31:0] plane_refused_role_o,
    output var logic [31:0] plane_refused_blend_o,
    output var logic [31:0] plane_disabled_o,

    output var logic [31:0] sprite_refused_o,
    output var logic [31:0] sprite_skipped_view_o,
    output var logic [31:0] smp_bind_missing_o,
    output var logic [31:0] smp_fmt_refused_o,
    output var logic [31:0] smp_page_oob_o,
    output var logic [31:0] smp_pal_refused_o,
    output var logic [31:0] smp_clut8_samples_o,
    output var logic [31:0] smp_rgb565_samples_o,
    output var logic [31:0] post_passes_completed_o
);

  import zhao_pkg::*;

  // ==========================================================================
  // THE FORK -- `zhao_console_core.sv`'s own expression
  // ==========================================================================
  logic dec_ready, exe_ready;
  assign pkt_ready_o = dec_ready && exe_ready;

  /* verilator lint_off UNUSEDSIGNAL */
  logic        rec_valid_w;
  logic [15:0] rec_opcode_w, rec_bytes_w;
  logic [31:0] rec_src_w, rec_index_w, bytes_consumed_w;
  /* verilator lint_on UNUSEDSIGNAL */

  zhao_cmd_decoder u_dec (
      .clk  (clk),
      .rst_n(rst_n && dec_rst_n),
      .pkt_valid_i(pkt_valid_i),
      .pkt_ready_o(dec_ready),
      .pkt_byte_i (pkt_byte_i),
      .pkt_len_i  (pkt_len_i),
      .rec_valid_o    (rec_valid_w),
      .rec_ready_i    (1'b1),
      .rec_opcode_o   (rec_opcode_w),
      .rec_bytes_o    (rec_bytes_w),
      .rec_source_id_o(rec_src_w),
      .rec_index_o    (rec_index_w),
      .decode_done_o   (decode_done_o),
      .decode_error_o  (decode_error_o),
      .bytes_consumed_o(bytes_consumed_w),
      .commands_o      (decode_commands_o)
  );

  // ==========================================================================
  // CMD.EXEC -- every port, by name, through `.*`
  // ==========================================================================
  // The locals below are `zhao_cmd_exec`'s port list minus the six this module
  // drives itself. Declaring them and binding with `.*` is what makes this
  // harness UNABLE TO DRIFT: a new port on that block appears here as a new
  // net, and a missing one is an elaboration error rather than a silent
  // `PINMISSING` reading zero -- which is the exact failure WARPCOMP found in
  // the composer (`tfld_ready_i` unconnected, so staged records could never
  // leave, and the counter that would have said so was unconnected too).
  logic        pkt_fork_ready_i;
  assign pkt_fork_ready_i = pkt_ready_o;
  logic verdict_valid_i;
  logic [7:0] verdict_error_i;
  assign verdict_valid_i = decode_done_o;
  assign verdict_error_i = decode_error_o;

`include "tb_zhao_twod_chain_exec_nets.svh"

  // The consumers CMD.EXEC has in the console and this bench does not. Every
  // one is a READY, and every one is high: this harness is about the TWOD path,
  // and a stalled stamp or draw port would backpressure the packet walk and
  // make a TWOD result depend on a block that is not here.
  assign proj_cfg_ready_i = 1'b1;
  assign stamp_ready_i    = 1'b1;
  assign draw_ready_i     = 1'b1;
  assign upl_ready_i      = 1'b1;
  // TERRAINMAT, 2026-09-26: `zhao_cmd_exec` gained two SetEnvironment
  // outputs for terrain's material identity. This bench binds it with
  // `.*`, which needs a declared net per port -- an implicit one is NOT
  // created for a `.*` connection, which is how this file failed to
  // elaborate the moment the ports landed. Declared and unread here: this
  // bench is about the 2-D compositor chain and has no terrain arm.
  logic [31:0] env_terr_mat_set_o;
  logic [15:0] env_terr_mat_id_o;
  assign env_ready_i      = 1'b1;
  assign pop_ready_i      = 1'b1;
  assign tfld_ready_i     = 1'b1;
  assign forge_ready_i    = 1'b1;
  assign post_idle_i      = 1'b1;

  assign ex_twod_planes_staged_o  = twod_planes_staged_o;
  assign ex_twod_sprites_staged_o = twod_sprites_staged_o;
  assign ex_twod_dropped_o        = twod_dropped_o;
  assign ex_twod_loads_issued_o   = twod_loads_issued_o;
  assign ex_uploads_issued_o      = uploads_issued_o;
  assign ex_unsupported_o         = unsupported_o;

  // `.pkt_ready_o` is named explicitly because the wrapper's port of that name
  // is the FORK's AND, not this block's own ready. Everything else binds by
  // `.*` against the declaration list included above.
  zhao_cmd_exec u_exec (.pkt_ready_o (exe_ready), .*);

  // ==========================================================================
  // TWOD.CMD -- the sealed frame list
  // ==========================================================================
  logic                tc_d_valid, tc_d_slot, tc_d_enable, tc_d_format;
  logic                tc_d_wrap_u, tc_d_wrap_v;
  logic [1:0]          tc_d_role, tc_d_blend, tc_d_vm;
  logic [7:0]          tc_d_opacity, tc_d_pal;
  logic [15:0]         tc_d_width, tc_d_height;
  logic signed [31:0]  tc_d_a, tc_d_b, tc_d_c, tc_d_d, tc_d_u0, tc_d_v0;
  logic                twod_pd_ready;

  logic                tc_s_valid;
  logic signed [15:0]  tc_s_x, tc_s_y;
  logic [15:0]         tc_s_w, tc_s_h, tc_s_tint, tc_s_src;
  logic signed [31:0]  tc_s_u, tc_s_v, tc_s_a00, tc_s_a01, tc_s_a10, tc_s_a11;
  logic [2:0]          tc_s_fmt;
  logic [7:0]          tc_s_pal, tc_s_ord;
  logic [1:0]          tc_s_blend, tc_s_vm;
  logic                twod_sd_ready;

  logic                          tc_bind_we;
  logic [$clog2(BIND_SLOTS)-1:0] tc_bind_sel;
  logic [$clog2(PAGE_WORDS)-1:0] tc_bind_base;
  logic [3:0]                    tc_bind_lstride, tc_bind_lheight;
  logic                          tc_atm_slot;
  logic signed [31:0]            tc_line_scroll;
  logic                          twod_seal;
  logic                          twod_list_busy;
  assign tc_publishing_o = twod_list_busy;
  assign dbg_fill_band_o   = 8'(u_twod_band.fill_band_q);
  assign dbg_rd_band_o     = 8'(u_twod_band.rd_band_q);
  assign dbg_scan_st_o     = 8'(u_twod_band.st_q);
  assign dbg_outstanding_o = 8'(u_twod_band.outstanding_q);

  zhao_twod_cmd #(
    .MAX_DESC   (MAX_DESC),
    .UVW        (32),
    .PAGE_WORDS (PAGE_WORDS),
    .BIND_SLOTS (BIND_SLOTS)
  ) u_twod_cmd (
    .clk (clk), .rst_n (rst_n),
    .pl_valid_i       (tpl_valid_o),
    .pl_ready_o       (tpl_ready_i),
    .pl_slot_i        (tpl_slot_o),
    .pl_role_i        (tpl_role_o),
    .pl_blend_i       (tpl_blend_o),
    .pl_opacity_i     (tpl_opacity_o),
    .pl_format_i      (tpl_format_o),
    .pl_wrap_i        (tpl_wrap_o),
    .pl_view_mask_i   (tpl_view_mask_o),
    .pl_palette_i     (tpl_palette_o),
    .pl_width_i       (tpl_width_o),
    .pl_height_i      (tpl_height_o),
    .pl_flags_i       (tpl_flags_o),
    .pl_base_i        (tpl_base_o),
    .pl_lstride_i     (tpl_lstride_o),
    .pl_lheight_i     (tpl_lheight_o),
    .pl_a_i           (tpl_a_o),
    .pl_b_i           (tpl_b_o),
    .pl_c_i           (tpl_c_o),
    .pl_d_i           (tpl_d_o),
    .pl_u0_i          (tpl_u0_o),
    .pl_v0_i          (tpl_v0_o),
    .pl_line_scroll_i (tpl_line_scroll_o),

    .sp_valid_i     (tsp_valid_o),
    .sp_ready_o     (tsp_ready_i),
    .sp_x_i         (tsp_x_o),
    .sp_y_i         (tsp_y_o),
    .sp_w_i         (tsp_w_o),
    .sp_h_i         (tsp_h_o),
    .sp_base_i      (tsp_base_o),
    .sp_lstride_i   (tsp_lstride_o),
    .sp_lheight_i   (tsp_lheight_o),
    .sp_format_i    (tsp_format_o),
    .sp_palette_i   (tsp_palette_o),
    .sp_blend_i     (tsp_blend_o),
    .sp_view_mask_i (tsp_view_mask_o),
    .sp_tint_i      (tsp_tint_o),
    .sp_order_i     (tsp_order_o),
    .sp_flags_i     (tsp_flags_o),
    .sp_src_id_i    (tsp_src_id_o),
    .sp_u_i         (tsp_u_o),
    .sp_v_i         (tsp_v_o),
    .sp_a00_i       (tsp_a00_o),
    .sp_a01_i       (tsp_a01_o),
    .sp_a10_i       (tsp_a10_o),
    .sp_a11_i       (tsp_a11_o),

    .pkt_commit_i  (twod_pkt_commit_o),
    .pkt_abandon_i (twod_pkt_abandon_o),
    .seal_i        (twod_seal),

    .d_valid_o (tc_d_valid), .d_ready_i (twod_pd_ready),
    .d_slot_o (tc_d_slot), .d_enable_o (tc_d_enable),
    .d_role_o (tc_d_role), .d_blend_o (tc_d_blend),
    .d_opacity_o (tc_d_opacity), .d_format_o (tc_d_format),
    .d_width_o (tc_d_width), .d_height_o (tc_d_height),
    .d_wrap_u_o (tc_d_wrap_u), .d_wrap_v_o (tc_d_wrap_v),
    .d_a_o (tc_d_a), .d_b_o (tc_d_b), .d_c_o (tc_d_c), .d_d_o (tc_d_d),
    .d_u0_o (tc_d_u0), .d_v0_o (tc_d_v0),
    .d_view_mask_o (tc_d_vm), .d_palette_o (tc_d_pal),

    .s_valid_o (tc_s_valid), .s_ready_i (twod_sd_ready),
    .s_x_o (tc_s_x), .s_y_o (tc_s_y), .s_w_o (tc_s_w), .s_h_o (tc_s_h),
    .s_u_o (tc_s_u), .s_v_o (tc_s_v),
    .s_a00_o (tc_s_a00), .s_a01_o (tc_s_a01),
    .s_a10_o (tc_s_a10), .s_a11_o (tc_s_a11),
    .s_format_o (tc_s_fmt), .s_palette_o (tc_s_pal),
    .s_tint_o (tc_s_tint), .s_blend_o (tc_s_blend),
    .s_view_mask_o (tc_s_vm), .s_order_o (tc_s_ord), .s_src_id_o (tc_s_src),

    .ld_bind_we_o      (tc_bind_we),
    .ld_bind_sel_o     (tc_bind_sel),
    .ld_bind_base_o    (tc_bind_base),
    .ld_bind_lstride_o (tc_bind_lstride),
    .ld_bind_lheight_o (tc_bind_lheight),
    .publishing_o  (twod_list_busy),
    .atm_slot_o    (tc_atm_slot),
    .line_scroll_o (tc_line_scroll),

    .planes_staged_o       (tc_planes_staged_o),
    .sprites_staged_o      (tc_sprites_staged_o),
    .plane_refused_o       (tc_plane_refused_o),
    .sprite_refused_o      (tc_sprite_refused_o),
    .list_overflow_o       (tc_list_overflow_o),
    .packets_committed_o   (tc_packets_committed_o),
    .packets_abandoned_o   (tc_packets_abandoned_o),
    .frames_sealed_o       (tc_frames_sealed_o),
    .planes_published_o    (tc_planes_published_o),
    .sprites_published_o   (tc_sprites_published_o),
    .slots_auto_disabled_o (tc_slots_auto_disabled_o),
    .bind_conflict_o       (tc_bind_conflict_o),
    .seal_overrun_o        (tc_seal_overrun_o)
  );

  // ==========================================================================
  // TWOD.ASSET -- the texels, from the arena, over the flattened bridge
  // ==========================================================================
  zhao_hps_burst_req_t ta_req;
  zhao_hps_burst_rsp_t ta_rsp;
  assign hps_req_valid_o = ta_req.valid;
  assign hps_req_addr_o  = ta_req.addr;
  assign hps_req_len_o   = ta_req.len;
  assign ta_rsp.beat_valid = hps_beat_valid_i;
  assign ta_rsp.data       = hps_data_i;
  assign ta_rsp.last       = hps_last_i;
  assign ta_rsp.err        = hps_err_i;
  /* verilator lint_off UNUSEDSIGNAL */
  wire _unused_req_write  = ta_req.write;
  wire [2:0] _unused_req_client = ta_req.client;
  /* verilator lint_on UNUSEDSIGNAL */

  logic                            ta_page_we, ta_pal_we;
  logic [$clog2(PAGE_WORDS)-1:0]   ta_page_addr;
  logic [$clog2(PAL_SLOTS*256)-1:0] ta_pal_addr;
  logic [15:0]                     ta_page_data, ta_pal_data;
  logic                            hud_req_v;

  zhao_twod_asset #(
    .PAGE_WORDS (PAGE_WORDS),
    .PAL_SLOTS  (PAL_SLOTS),
    .PAGE_SLOTS (PAGE_SLOTS)
  ) u_twod_asset (
    .clk (clk), .rst_n (rst_n),
    .cfg_epoch_i (cfg_epoch_i),
    .j_valid_i    (tld_valid_o),
    .j_ready_o    (tld_ready_i),
    .j_index_i    (tld_index_o),
    .j_hps_addr_i (tld_hps_addr_o),
    .j_len_i      (tld_len_o),
    .j_crc_i      (tld_crc_o),
    .j_epoch_i    (tld_epoch_o),
    .j_dst_slot_i (tld_dst_slot_o),
    .hps_req_o   (ta_req),
    .hps_grant_i (hps_grant_i),
    .hps_rsp_i   (ta_rsp),
    .ld_page_we_o   (ta_page_we),
    .ld_page_addr_o (ta_page_addr),
    .ld_page_data_o (ta_page_data),
    .ld_pal_we_o    (ta_pal_we),
    .ld_pal_addr_o  (ta_pal_addr),
    .ld_pal_data_o  (ta_pal_data),
    .pass_active_i (hud_req_v),
    .loads_started_o     (ta_loads_started_o),
    .loads_done_o        (ta_loads_done_o),
    .words_written_o     (ta_words_written_o),
    .slot_refused_o      (ta_slot_refused_o),
    .len_refused_o       (ta_len_refused_o),
    .addr_refused_o      (ta_addr_refused_o),
    .epoch_refused_o     (ta_epoch_refused_o),
    .crc_fails_o         (ta_crc_fails_o),
    .regions_zeroed_o    (ta_regions_zeroed_o),
    .bridge_errs_o       (ta_bridge_errs_o),
    .loads_during_pass_o (ta_loads_during_pass_o),
    .bursts_o            (ta_bursts_o)
  );

  // ==========================================================================
  // THE TWOD CHAIN -- exactly the composer's wiring
  // ==========================================================================
  logic                pw_valid, pw_ready, pw_slot;
  logic [15:0]         pw_x, pw_y;
  logic signed [31:0]  pw_scroll;
  logic                pl_valid, pl_ready, pl_fmt;
  logic [15:0]         pl_u, pl_v;
  logic [7:0]          pl_pal, pl_opacity;
  logic [1:0]          pl_blend, pl_role;

  zhao_twod_plane #(.CW (32)) u_twod_plane (
    .clk (clk), .rst_n (rst_n),
    .d_valid_i (tc_d_valid), .d_ready_o (twod_pd_ready),
    .d_slot_i (tc_d_slot), .d_enable_i (tc_d_enable),
    .d_role_i (tc_d_role), .d_blend_i (tc_d_blend),
    .d_opacity_i (tc_d_opacity), .d_format_i (tc_d_format),
    .d_width_i (tc_d_width), .d_height_i (tc_d_height),
    .d_wrap_u_i (tc_d_wrap_u), .d_wrap_v_i (tc_d_wrap_v),
    .d_a_i (tc_d_a), .d_b_i (tc_d_b), .d_c_i (tc_d_c), .d_d_i (tc_d_d),
    .d_u0_i (tc_d_u0), .d_v0_i (tc_d_v0),
    .d_view_mask_i (tc_d_vm), .d_palette_i (tc_d_pal),
    .p_valid_i (pw_valid), .p_ready_o (pw_ready), .p_slot_i (pw_slot),
    .p_x_i (pw_x), .p_y_i (pw_y), .p_line_scroll_i (pw_scroll),
    .view_sel_i (view_sel_i ? 2'b10 : 2'b01),
    .s_valid_o (pl_valid), .s_ready_i (pl_ready),
    .s_texel_u_o (pl_u), .s_texel_v_o (pl_v),
    .s_format_o (pl_fmt), .s_palette_o (pl_pal),
    .s_blend_o (pl_blend), .s_opacity_o (pl_opacity), .s_role_o (pl_role),
    .pixels_o (plane_pixels_o),
    .refused_role_o (plane_refused_role_o),
    .refused_blend_o (plane_refused_blend_o),
    .skipped_view_o (),
    .wrap_fail_o (),
    .disabled_o (plane_disabled_o)
  );

  logic                bd_valid, bd_ready;
  logic signed [15:0]  bd_x, bd_y;
  logic [15:0]         bd_w, bd_h, bd_tint, bd_srcid;
  logic signed [31:0]  bd_u, bd_v, bd_a00, bd_a01, bd_a10, bd_a11;
  logic [2:0]          bd_fmt;
  logic [7:0]          bd_pal, bd_ord;
  logic [1:0]          bd_blend, bd_vm, bd_view_sel;

  logic                sc_valid, sc_ready, sc_last;
  logic [15:0]         sc_rgb, sc_tint, sc_srcid;
  logic signed [15:0]  sc_x, sc_y;
  logic [1:0]          sc_blend;
  logic [7:0]          sc_order;

  logic [$clog2(LINE_W+1)-1:0] hud_req_x;
  logic [$clog2(MAX_H +1)-1:0] hud_req_y;
  logic                        hud_valid;
  logic [15:0]                 hud_rgb;

  zhao_twod_band #(
    .LINE_W (LINE_W), .MAX_H (MAX_H), .B (4), .L (16),
    .MAX_DESC (MAX_DESC), .UVW (32)
  ) u_twod_band (
    .clk (clk), .rst_n (rst_n),
    .frame_start_i (frame_start_i),
    .frame_w_i (frame_w_i), .frame_h_i (frame_h_i),
    .view_split_i (view_split_i),
    .list_busy_i (twod_list_busy),
    .d_valid_i (tc_s_valid), .d_ready_o (twod_sd_ready),
    .d_x_i (tc_s_x), .d_y_i (tc_s_y), .d_w_i (tc_s_w), .d_h_i (tc_s_h),
    .d_u_i (tc_s_u), .d_v_i (tc_s_v),
    .d_a00_i (tc_s_a00), .d_a01_i (tc_s_a01),
    .d_a10_i (tc_s_a10), .d_a11_i (tc_s_a11),
    .d_format_i (tc_s_fmt), .d_palette_i (tc_s_pal),
    .d_tint_i (tc_s_tint), .d_blend_i (tc_s_blend),
    .d_view_mask_i (tc_s_vm), .d_order_i (tc_s_ord), .d_src_id_i (tc_s_src),
    .e_valid_o (bd_valid), .e_ready_i (bd_ready),
    .e_x_o (bd_x), .e_y_o (bd_y), .e_w_o (bd_w), .e_h_o (bd_h),
    .e_u_o (bd_u), .e_v_o (bd_v),
    .e_a00_o (bd_a00), .e_a01_o (bd_a01), .e_a10_o (bd_a10), .e_a11_o (bd_a11),
    .e_format_o (bd_fmt), .e_palette_o (bd_pal),
    .e_tint_o (bd_tint), .e_blend_o (bd_blend),
    .e_view_mask_o (bd_vm), .e_order_o (bd_ord), .e_src_id_o (bd_srcid),
    .e_view_sel_o (bd_view_sel),
    .c_valid_i (sc_valid), .c_ready_o (sc_ready), .c_rgb_i (sc_rgb),
    .c_x_i (sc_x), .c_y_i (sc_y), .c_tint_i (sc_tint), .c_blend_i (sc_blend),
    .c_order_i (sc_order), .c_src_id_i (sc_srcid), .c_last_i (sc_last),
    .rd_req_v_i (hud_req_v), .rd_x_i (hud_req_x), .rd_y_i (hud_req_y),
    .rd_valid_o (hud_valid), .rd_rgb_o (hud_rgb),
    .descriptors_o (band_descriptors_o),
    .desc_overflow_o (band_desc_overflow_o),
    .sprites_admitted_o (band_sprites_admitted_o),
    .sprites_refused_budget_o (band_sprites_refused_budget_o),
    .slices_emitted_o (),
    .pixels_written_o (band_pixels_written_o),
    .pixels_clipped_o (),
    .write_oob_o (),
    .band_underrun_o (band_underrun_o),
    .scan_addr_mismatch_o (),
    .tint_dropped_o (),
    .blend_dropped_o (),
    .order_inversion_o (band_order_inversion_o),
    .bands_o (band_bands_o),
    .list_restart_o (twod_seal),
    .desc_mid_sweep_o (band_desc_mid_sweep_o)
  );

  logic                sp_valid, sp_ready, sp_last;
  logic signed [15:0]  sp_x, sp_y;
  logic signed [31:0]  sp_u, sp_v;
  logic [2:0]          sp_fmt;
  logic [7:0]          sp_pal, sp_order;
  logic [15:0]         sp_tint, sp_srcid;
  logic [1:0]          sp_blend;

  zhao_twod_sprite #(.UVW (32)) u_twod_sprite (
    .clk (clk), .rst_n (rst_n),
    .d_valid_i (bd_valid), .d_ready_o (bd_ready),
    .d_x_i (bd_x), .d_y_i (bd_y), .d_w_i (bd_w), .d_h_i (bd_h),
    .d_u_i (bd_u), .d_v_i (bd_v),
    .d_a00_i (bd_a00), .d_a01_i (bd_a01), .d_a10_i (bd_a10), .d_a11_i (bd_a11),
    .d_format_i (bd_fmt), .d_palette_i (bd_pal),
    .d_tint_i (bd_tint), .d_blend_i (bd_blend),
    .d_view_mask_i (bd_vm), .d_order_i (bd_ord), .d_src_id_i (bd_srcid),
    .view_sel_i (bd_view_sel),
    .s_valid_o (sp_valid), .s_ready_i (sp_ready),
    .s_x_o (sp_x), .s_y_o (sp_y), .s_u_o (sp_u), .s_v_o (sp_v),
    .s_format_o (sp_fmt), .s_palette_o (sp_pal),
    .s_tint_o (sp_tint), .s_blend_o (sp_blend),
    .s_order_o (sp_order), .s_src_id_o (sp_srcid), .s_last_o (sp_last),
    .descriptors_o (),
    .skipped_view_o (sprite_skipped_view_o),
    .refused_o (sprite_refused_o),
    .pixels_o ()
  );

  logic                        atm_req_v, atm_en, atm_valid, atm_add;
  logic [$clog2(LINE_W+1)-1:0] atm_req_x;
  logic [$clog2(MAX_H +1)-1:0] atm_req_y;
  logic [15:0]                 atm_rgb;
  logic [7:0]                  atm_opacity;

  zhao_twod_sampler #(
    .LINE_W (LINE_W), .MAX_H (MAX_H),
    .PAGE_WORDS (PAGE_WORDS), .PAL_SLOTS (PAL_SLOTS),
    .BIND_SLOTS (BIND_SLOTS), .ATM_LINES (4)
  ) u_twod_sampler (
    .clk (clk), .rst_n (rst_n),
    .frame_start_i (frame_start_i),
    .frame_w_i (frame_w_i), .frame_h_i (frame_h_i),
    .ld_page_we_i (ta_page_we),
    .ld_page_addr_i (ta_page_addr),
    .ld_page_data_i (ta_page_data),
    .ld_pal_we_i (ta_pal_we),
    .ld_pal_addr_i (ta_pal_addr),
    .ld_pal_data_i (ta_pal_data),
    .ld_bind_we_i (tc_bind_we),
    .ld_bind_sel_i (tc_bind_sel),
    .ld_bind_base_i (tc_bind_base),
    .ld_bind_lstride_i (tc_bind_lstride),
    .ld_bind_lheight_i (tc_bind_lheight),
    .atm_slot_i (tc_atm_slot),
    .line_scroll_i (tc_line_scroll),
    .pw_valid_o (pw_valid), .pw_ready_i (pw_ready), .pw_slot_o (pw_slot),
    .pw_x_o (pw_x), .pw_y_o (pw_y), .pw_line_scroll_o (pw_scroll),
    .pl_valid_i (pl_valid), .pl_ready_o (pl_ready),
    .pl_texel_u_i (pl_u), .pl_texel_v_i (pl_v),
    .pl_format_i (pl_fmt), .pl_palette_i (pl_pal),
    .pl_blend_i (pl_blend), .pl_opacity_i (pl_opacity), .pl_role_i (pl_role),
    .sp_valid_i (sp_valid), .sp_ready_o (sp_ready),
    .sp_x_i (sp_x), .sp_y_i (sp_y), .sp_u_i (sp_u), .sp_v_i (sp_v),
    .sp_format_i (sp_fmt), .sp_palette_i (sp_pal),
    .sp_tint_i (sp_tint), .sp_blend_i (sp_blend),
    .sp_order_i (sp_order), .sp_src_id_i (sp_srcid), .sp_last_i (sp_last),
    .sc_valid_o (sc_valid), .sc_ready_i (sc_ready), .sc_rgb_o (sc_rgb),
    .sc_x_o (sc_x), .sc_y_o (sc_y), .sc_tint_o (sc_tint),
    .sc_blend_o (sc_blend), .sc_order_o (sc_order),
    .sc_src_id_o (sc_srcid), .sc_last_o (sc_last),
    .atm_req_v_i (atm_req_v), .atm_req_x_i (atm_req_x), .atm_req_y_i (atm_req_y),
    .atm_en_o (atm_en), .atm_valid_o (atm_valid), .atm_rgb_o (atm_rgb),
    .atm_opacity_o (atm_opacity), .atm_add_o (atm_add),
    .samples_o (), .plane_samples_o (), .sprite_samples_o (),
    .clut8_samples_o (smp_clut8_samples_o),
    .rgb565_samples_o (smp_rgb565_samples_o),
    .texel_wrapped_o (),
    .page_oob_o (smp_page_oob_o),
    .bind_missing_o (smp_bind_missing_o),
    .fmt_refused_o (smp_fmt_refused_o),
    .pal_refused_o (smp_pal_refused_o),
    .skipped_fill_o (), .atm_underrun_o (),
    .walk_stalls_o (), .sprite_stalls_o (),
    .tint_unapplied_o (), .pair_lost_o ()
  );

  // ==========================================================================
  // POST.COMPOSITE -- the final pixels
  // ==========================================================================
  zhao_post_composite #(
    .LINE_W (LINE_W), .MAX_H (MAX_H)
  ) u_post (
    .clk (clk), .rst_n (rst_n),
    .frame_start_i (frame_start_i),
    .frame_w_i (frame_w_i), .frame_h_i (frame_h_i),
    .view_sel_i (view_sel_i),
    .s_valid_i (s_valid_i), .s_ready_o (s_ready_o), .s_rgb_i (s_rgb_i),
    // Both gather planes report ABSENT, so distortion, glow and ink cannot
    // move a bit of the output. See the header: a composited pixel here is the
    // world, the atmosphere over it, and the HUD replacing both.
    .gd_req_v_o (), .gd_view_o (), .gd_cx_o (), .gd_cy_o (),
    .gd_present_i (1'b0), .gd_dx_i (8'sd0), .gd_dy_i (8'sd0),
    .gg_req_v_o (), .gg_view_o (), .gg_cx_o (), .gg_cy_o (),
    .gg_present_i (1'b0), .gg_glow_i (16'd0), .gg_ink_i (1'b0),
    .atm_req_v_o (atm_req_v), .atm_req_x_o (atm_req_x), .atm_req_y_o (atm_req_y),
    .atm_en_i (atm_en), .atm_valid_i (atm_valid), .atm_rgb_i (atm_rgb),
    .atm_opacity_i (atm_opacity), .atm_add_i (atm_add),
    .bloom_gain_i (8'd0), .grade_valid_i (1'b0),
    .pv_we_i (1'b0), .pv_sel_i (2'd0), .pv_addr_i (6'd0), .pv_data_i (72'd0),
    .bias_r_i (9'sd0), .bias_g_i (9'sd0), .bias_b_i (9'sd0),
    .flash_rgb_i (16'd0), .flash_amt_i (8'd0), .ink_rgb_i (16'd0),
    .hud_req_v_o (hud_req_v), .hud_req_x_o (hud_req_x), .hud_req_y_o (hud_req_y),
    .hud_valid_i (hud_valid), .hud_rgb_i (hud_rgb),
    .o_valid_o (o_valid_o), .o_ready_i (o_ready_i), .o_rgb_o (o_rgb_o),
    .o_x_o (o_x_o), .o_y_o (o_y_o), .o_last_o (o_last_o),
    .echo_valid_o (), .echo_rgb_o (),
    .displacement_edge_clamps_o (), .bloom_cells_contributing_o (),
    .passes_completed_o (post_passes_completed_o),
    .grading_table_missing_o (), .plane_missing_o (),
    .line_fill_writes_o (), .output_writes_o (),
    .plane_reads_o (), .ring_hazard_o ()
  );

endmodule : tb_zhao_twod_chain

`default_nettype wire
