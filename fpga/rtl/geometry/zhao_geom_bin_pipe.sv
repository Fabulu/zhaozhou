// zhao_geom_bin_pipe.sv — the phase-5 "real tile lists drive the real
// rasterizer" composition: GEOM.BINNER → zhao_raster_tile_pipe, which is
// itself RASTER.EDGEWALK → RASTER.EARLYZ → RASTER.FRAGMENT →
// RASTER.TILESTORE → RASTER.RESOLVE.
//
// ---------------------------------------------------------------------------
// THIS BLOCK IS NOT IN design/blocks.yml, AND THAT IS DELIBERATE
// ---------------------------------------------------------------------------
// The ledger's GEOMETRY group has no entry for "the composition", exactly as
// its RASTER group has none for `zhao_raster_tile_pipe.sv`, and registering a
// block is a validator-gated ledger edit (charter §4) — not this increment's
// call. So the rationale lives here rather than in a contract no ledger row
// points at. This file is a COMPOSITION ONLY: every law lives in the blocks it
// instantiates and in their contracts, and it adds exactly TWO things of its
// own, both named below.
//
// What it buys, and why it was built: until now every rasterizer test in this
// tree has been handed ONE triangle and ONE tile by a C++ driver. This file
// closes the loop — a triangle enters GEOM.BINNER as a setup packet, the chunk
// arena decides which tiles it lands in, and the DRAINED TILE LIST is what
// drives the edge walker. A binning bug that a differential lane could only
// see as a wrong tile index now shows up as a missing tile in a rendered
// picture, which is the thing that actually matters.
//
// ---------------------------------------------------------------------------
// THE TWO LAWS THIS FILE OWNS
// ---------------------------------------------------------------------------
// 1. THE JOB SPLICE. GEOM.BINNER's drain port is RASTER.EDGEWALK's job port
//    field for field — six 21-bit vertices, two signed 12-bit tile
//    coordinates and a source id — and `zhao_raster_tile_pipe`'s job port is
//    that same port plus a per-job RECIPE (the clear/fill words, the fragment
//    state, the vertex alpha and the texel). Nothing in the binner knows about
//    a recipe and nothing should: GEOM.SETUP does not carry material state and
//    RASTER.FRAGMENT takes it flat. So the recipe fields are module inputs,
//    held constant across the frame by the caller, and the splice is a pure
//    ready/valid pass-through with no buffering: `job_valid` and `job_ready`
//    are wired straight across.
//
// 2. THE TILE INDEX. `zhao_raster_tile_pipe` wants a 16-bit `job_tile_index_i`
//    for the .zcap TILE_CRC record. The binner drains in row-major grid order,
//    so the index is `{ty, tx}` in TILES — the drain port carries the tile's
//    top-left PIXEL (RASTER.EDGEWALK's unit), so it is shifted down by 4 here
//    — the tile's own grid position,
//    which is stable, unique per tile and readable in a trace. It is NOT a
//    running counter: two frames that bin the same scene then produce the same
//    indices, which is what a capture diff needs.
//
// ---------------------------------------------------------------------------
// THE RESTRICTION THIS COMPOSITION HAS, STATED RATHER THAN HIDDEN
// ---------------------------------------------------------------------------
// `zhao_raster_tile_pipe` is ONE CLEAR + ONE TRIANGLE + ONE RESOLVE per job —
// its own header says so ("no multi-triangle accumulation into one tile"). A
// tile that appears TWICE in the drain therefore gets cleared twice and
// resolved twice, and the second resolve overwrites the first rather than
// compositing onto it. That is a real limitation of the composition, not of
// GEOM.BINNER: the binner's whole point is that a tile CAN hold many
// triangles, and the tile list it drains is correct either way.
//
// Fixing it would mean giving `zhao_raster_tile_pipe` a "continue this tile"
// job flavour — a change to an existing, finished block's interface, which
// this increment does not make. So the composition is exercised on scenes
// where every tile receives at most one triangle (one triangle across many
// tiles is the natural such scene, and it is the interesting one), and the
// restriction is recorded here, in design/contracts/GEOM.BINNER.md, and in the
// increment report.
//
// Simulation only: this file is not in fpga/files.qip, has no Quartus fit, no
// capture, and has never run on hardware. Simulated is not synthesized and
// neither is on-hardware.
//
// Conservative SystemVerilog subset only (charter §2); depends on
// zhao_geom_binner (and through it zhao_geom_arena, zhao_raster_fill) and
// zhao_raster_tile_pipe (and through it the whole RASTER chain).
// Lint: clean under `verilator_bin --lint-only -Wall` (lint_geom_bin_pipe).

module zhao_geom_bin_pipe (
  input  logic clk,
  input  logic rst_n,

  // ---- GEOM.BINNER's frame boundaries and setup-triangle port ------------
  input  logic               frame_begin_i,
  input  logic               frame_end_i,
  input  logic        [5:0]  grid_w_i,
  input  logic        [5:0]  grid_h_i,

  input  logic               tri_valid_i,
  output logic               tri_ready_o,
  input  logic signed [22:0] tri_kx0_i,
  input  logic signed [22:0] tri_ky0_i,
  input  logic signed [47:0] tri_kc0_i,
  input  logic signed [22:0] tri_kx1_i,
  input  logic signed [22:0] tri_ky1_i,
  input  logic signed [47:0] tri_kc1_i,
  input  logic signed [22:0] tri_kx2_i,
  input  logic signed [22:0] tri_ky2_i,
  input  logic signed [47:0] tri_kc2_i,
  input  logic        [2:0]  tri_tl_i,
  input  logic signed [20:0] tri_ax_i,
  input  logic signed [20:0] tri_ay_i,
  input  logic signed [20:0] tri_bx_i,
  input  logic signed [20:0] tri_by_i,
  input  logic signed [20:0] tri_cx_i,
  input  logic signed [20:0] tri_cy_i,
  input  logic signed [11:0] tri_min_x_i,
  input  logic signed [11:0] tri_max_x_i,
  input  logic signed [11:0] tri_min_y_i,
  input  logic signed [11:0] tri_max_y_i,
  input  logic        [15:0] tri_src_id_i,
  output logic               tok_req_o,
  input  logic               tok_grant_i,

  // ---- the flat recipe, held constant across the frame (law 1) -----------
  input  logic        [63:0] job_fill_word_i,
  input  logic        [63:0] job_clear_word_i,
  input  logic        [31:0] job_state_i,
  input  logic        [7:0]  job_src_a_i,
  input  logic        [23:0] job_texel_rgb_i,
  input  logic        [7:0]  job_texel_a_i,
  input  logic        [7:0]  job_texel_idx_i,

  // ---- the rasterizer's framebuffer stream -------------------------------
  output logic               fb_valid_o,
  input  logic               fb_ready_i,
  output logic        [15:0] fb_rgb565_o,
  output logic        [7:0]  fb_tag_o,
  output logic        [7:0]  fb_addr_o,
  output logic signed [11:0] fb_x_o,
  output logic signed [11:0] fb_y_o,
  output logic               fb_last_o,
  output logic        [15:0] fb_src_id_o,

  // ---- per-tile completion ----------------------------------------------
  output logic        [31:0] tile_crc_o,
  output logic        [15:0] tile_crc_index_o,
  output logic               tile_done_o,
  output logic        [8:0]  tile_cov_count_o,
  output logic               tile_degenerate_o,

  // ---- binner status ------------------------------------------------------
  output logic               drain_busy_o,
  output logic               drain_done_o,
  output logic        [31:0] tile_references_o,
  output logic        [15:0] max_tile_list_depth_o,
  output logic        [31:0] triangles_culled_o,
  output logic               overflow_o,
  output logic               arena_full_o,

  // ---- SEAM observability, added 2026-08-30 ------------------------------
  // Counters, not behaviour: nothing below drives a datapath. They exist
  // because the interesting property of this composition is not whether it is
  // CORRECT -- both halves were green before it was written -- but how much of
  // the frame the binner spends waiting for the rasterizer, which is a number
  // neither block can report about itself. `job_stall_clocks_o` counts clocks
  // where the binner is OFFERING a job the pipe will not take.
  //
  // ENFORCED-BY: tests/render/render_pipe_directed.cpp:main
  output logic        [31:0] jobs_taken_o,
  output logic        [31:0] job_stall_clocks_o,
  output logic        [31:0] early_z_rejects_o,
  output logic               fragment_error_o
);

  // ------------------------------------------------------- the job splice --
  logic               job_valid, job_ready;
  logic signed [20:0] job_ax, job_ay, job_bx, job_by, job_cx, job_cy;
  logic signed [11:0] job_tile_x, job_tile_y;
  logic        [15:0] job_src_id;
  // The tile-list position, straight through. This composition adds nothing to
  // it -- the binner knows where a reference sits and the tile pipe acts on it.
  logic               job_first, job_last;

  // (arena_full_o is a port now; arena_used_o stays unused here.)
  logic        [8:0]  arena_used_unused;

  zhao_geom_binner u_binner (
    .clk                  (clk),
    .rst_n                (rst_n),
    .frame_begin_i        (frame_begin_i),
    .frame_end_i          (frame_end_i),
    .grid_w_i             (grid_w_i),
    .grid_h_i             (grid_h_i),
    .tri_valid_i          (tri_valid_i),
    .tri_ready_o          (tri_ready_o),
    .tri_kx0_i            (tri_kx0_i),
    .tri_ky0_i            (tri_ky0_i),
    .tri_kc0_i            (tri_kc0_i),
    .tri_kx1_i            (tri_kx1_i),
    .tri_ky1_i            (tri_ky1_i),
    .tri_kc1_i            (tri_kc1_i),
    .tri_kx2_i            (tri_kx2_i),
    .tri_ky2_i            (tri_ky2_i),
    .tri_kc2_i            (tri_kc2_i),
    .tri_tl_i             (tri_tl_i),
    .tri_ax_i             (tri_ax_i),
    .tri_ay_i             (tri_ay_i),
    .tri_bx_i             (tri_bx_i),
    .tri_by_i             (tri_by_i),
    .tri_cx_i             (tri_cx_i),
    .tri_cy_i             (tri_cy_i),
    .tri_min_x_i          (tri_min_x_i),
    .tri_max_x_i          (tri_max_x_i),
    .tri_min_y_i          (tri_min_y_i),
    .tri_max_y_i          (tri_max_y_i),
    .tri_src_id_i         (tri_src_id_i),
    .tok_req_o            (tok_req_o),
    .tok_grant_i          (tok_grant_i),
    .job_valid_o          (job_valid),
    .job_ready_i          (job_ready),
    .job_ax_o             (job_ax),
    .job_ay_o             (job_ay),
    .job_bx_o             (job_bx),
    .job_by_o             (job_by),
    .job_cx_o             (job_cx),
    .job_cy_o             (job_cy),
    .job_first_o          (job_first),
    .job_last_o           (job_last),
    .job_tile_x_o         (job_tile_x),
    .job_tile_y_o         (job_tile_y),
    .job_src_id_o         (job_src_id),
    .drain_busy_o         (drain_busy_o),
    .drain_done_o         (drain_done_o),
    .tile_references_o    (tile_references_o),
    .max_tile_list_depth_o(max_tile_list_depth_o),
    .triangles_culled_o   (triangles_culled_o),
    .overflow_o           (overflow_o),
    .arena_full_o         (arena_full_o),
    .arena_used_o         (arena_used_unused)
  );

  // LAW 2 — the tile index is the tile's own grid position, not a counter.
  logic [15:0] tile_index;
  assign tile_index = {4'd0, job_tile_y[9:4], job_tile_x[9:4]};

  logic [31:0] tp_tile_refs_unused, tp_resolved_unused;
  logic [31:0] tp_ez_cov_unused, tp_fr_cov_unused, tp_blended_unused;
  logic [7:0]  tp_bin_mask_unused;
  logic [23:0] tp_zfloor_unused;
  logic        tp_front_unused;

  zhao_raster_tile_pipe u_pipe (
    .clk                (clk),
    .rst_n              (rst_n),
    .job_valid_i        (job_valid),
    .job_ready_o        (job_ready),
    .job_ax_i           (job_ax),
    .job_ay_i           (job_ay),
    .job_bx_i           (job_bx),
    .job_by_i           (job_by),
    .job_cx_i           (job_cx),
    .job_cy_i           (job_cy),
    .job_first_i        (job_first),
    .job_last_i         (job_last),
    .job_tile_x_i       (job_tile_x),
    .job_tile_y_i       (job_tile_y),
    .job_fill_word_i    (job_fill_word_i),
    .job_clear_word_i   (job_clear_word_i),
    .job_tile_index_i   (tile_index),
    .job_src_id_i       (job_src_id),
    .job_state_i        (job_state_i),
    .job_src_a_i        (job_src_a_i),
    .job_texel_rgb_i    (job_texel_rgb_i),
    .job_texel_a_i      (job_texel_a_i),
    .job_texel_idx_i    (job_texel_idx_i),
    .fb_valid_o         (fb_valid_o),
    .fb_ready_i         (fb_ready_i),
    .fb_rgb565_o        (fb_rgb565_o),
    .fb_tag_o           (fb_tag_o),
    .fb_addr_o          (fb_addr_o),
    .fb_x_o             (fb_x_o),
    .fb_y_o             (fb_y_o),
    .fb_last_o          (fb_last_o),
    .fb_src_id_o        (fb_src_id_o),
    .tile_crc_o         (tile_crc_o),
    .tile_crc_index_o   (tile_crc_index_o),
    .tile_done_o        (tile_done_o),
    .tile_cov_count_o   (tile_cov_count_o),
    .tile_degenerate_o  (tile_degenerate_o),
    .front_bank_o       (tp_front_unused),
    .tile_references_o  (tp_tile_refs_unused),
    .resolved_tiles_o   (tp_resolved_unused),
    .early_z_rejects_o  (early_z_rejects_o),
    .ez_covered_o       (tp_ez_cov_unused),
    .fr_covered_o       (tp_fr_cov_unused),
    .blended_fragments_o(tp_blended_unused),
    .bin_mask_o         (tp_bin_mask_unused),
    .z_floor_o          (tp_zfloor_unused),
    .fragment_error_o   (fragment_error_o)
  );

  // The seam, counted. Free-running from reset, so a measured window is the
  // DIFFERENCE of two reads -- the Field engine shipped a 123%-occupancy
  // report by dividing a from-reset counter by a windowed clock count, and
  // this file is not repeating it.
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      jobs_taken_o       <= 32'd0;
      job_stall_clocks_o <= 32'd0;
    end else begin
      if (job_valid && job_ready) jobs_taken_o <= jobs_taken_o + 32'd1;
      if (job_valid && !job_ready) job_stall_clocks_o <= job_stall_clocks_o + 32'd1;
    end
  end

endmodule : zhao_geom_bin_pipe
