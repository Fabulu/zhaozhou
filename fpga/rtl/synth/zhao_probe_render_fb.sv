// zhao_probe_render_fb.sv — the whole render path, triangle in, VRAM writes out.
//
// ENFORCED-BY: tests/render/render_fb_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHAT THIS CLOSES
// ---------------------------------------------------------------------------
// `zhao_geom_bin_pipe` is GEOM.BINNER driving `zhao_raster_tile_pipe`, and it
// is green: a setup triangle goes in, tile lists are built, coverage is walked,
// fragments are shaded and resolved, and a stream of RGB565 pixels comes out
// with their surface coordinates. That stream has never had a consumer.
//
// `zhao_raster_fbwrite` is the consumer, and this file is the two of them
// joined. With it, a triangle described in screen coordinates becomes BYTES AT
// ADDRESSES for the first time in this project — which is the step between "the
// rasterizer is verified" and "the console draws".
//
// It is a COMPOSITION and owns no laws. The binning is the binner's, the
// coverage and resolve are the tile pipe's, the burst shape is FBWRITE's, and
// the region check is MEM.GUARD's, which sits outside this probe on purpose:
// the guard is already integrated in `zhao_shell_top` and re-instantiating it
// here would be testing a second copy rather than the shipping one.
//
// ---------------------------------------------------------------------------
// THE SEAM, AND WHY IT IS ONLY A WIRE
// ---------------------------------------------------------------------------
// RASTER.RESOLVE emits `fb_rgb565_o` with `fb_x_o`/`fb_y_o` — the pixel's
// SURFACE coordinate, tile origin already added — and `fb_last_o` on the tile's
// last beat. RASTER.FBWRITE takes exactly those four fields plus ready/valid.
// There is no width adaptation, no reordering and no buffering between them,
// which is the whole reason FBWRITE was shaped to consume a raster-ordered
// stream rather than a tile.
//
// The backpressure runs the other way and matters more: FBWRITE holds
// `px_ready_o` low while it is issuing a burst, that stalls RESOLVE, which
// stalls the tile store, the fragment path and finally the binner's drain. So a
// slow memory reaches all the way back to geometry through this one wire, and
// the composed test can make memory slow and watch it happen.
`default_nettype none

module zhao_probe_render_fb
  import zhao_pkg::*;
(
    input var logic clk,
    input var logic rst_n,

    // ---- frame and the triangle port, straight through to the binner ------
    input var logic        frame_begin_i,
    input var logic        frame_end_i,
    input var logic [5:0]  grid_w_i,
    input var logic [5:0]  grid_h_i,

    input  var logic               tri_valid_i,
    output var logic               tri_ready_o,
    input  var logic signed [22:0] tri_kx0_i, tri_ky0_i,
    input  var logic signed [47:0] tri_kc0_i,
    input  var logic signed [22:0] tri_kx1_i, tri_ky1_i,
    input  var logic signed [47:0] tri_kc1_i,
    input  var logic signed [22:0] tri_kx2_i, tri_ky2_i,
    input  var logic signed [47:0] tri_kc2_i,
    input  var logic        [ 2:0] tri_tl_i,
    input  var logic signed [20:0] tri_ax_i, tri_ay_i, tri_bx_i, tri_by_i,
    input  var logic signed [20:0] tri_cx_i, tri_cy_i,
    input  var logic signed [11:0] tri_min_x_i, tri_max_x_i,
    input  var logic signed [11:0] tri_min_y_i, tri_max_y_i,
    input  var logic        [15:0] tri_src_id_i,
    output var logic               tok_req_o,
    input  var logic               tok_grant_i,

    // ---- the flat recipe, held across the frame ---------------------------
    input var logic [63:0] job_fill_word_i,
    input var logic [63:0] job_clear_word_i,
    input var logic [31:0] job_state_i,
    input var logic [ 7:0] job_src_a_i,
    input var logic [23:0] job_texel_rgb_i,
    input var logic [ 7:0] job_texel_a_i,
    input var logic [ 7:0] job_texel_idx_i,

    // ---- where the picture goes -------------------------------------------
    input var logic [ZHAO_VRAM_ADDR_BITS-1:0] fb_base_i,
    input var logic [15:0]                    fb_stride_i,

    // ---- MEM.GUARD, modelled by the harness -------------------------------
    output var zhao_guard_req_t guard_req_o,
    input  var zhao_guard_rsp_t guard_rsp_i,
    output var logic [63:0]     guard_wdata_o,
    output var logic            guard_wvalid_o,
    input  var logic            guard_wready_i,
    output var logic            guard_wlast_o,
    // The arbiter's credit stream, modelled by the harness.
    input  var logic [7:0]      retire_words_i,

    // ---- evidence ----------------------------------------------------------
    output var logic        drain_busy_o,
    output var logic        drain_done_o,
    output var logic        overflow_o,
    output var logic        fragment_error_o,
    output var logic        fb_stream_error_o,
    output var logic        fb_busy_o,
    output var logic        fb_drained_o,
    output var logic        fb_fatal_o,
    output var logic [31:0] fb_issued_words_o,
    output var logic [31:0] fb_retired_words_o,
    output var logic [31:0] pixels_written_o,
    output var logic [31:0] bursts_issued_o,
    output var logic [31:0] fb_stall_clocks_o,
    output var logic [31:0] jobs_taken_o,
    output var logic [31:0] job_stall_clocks_o,
    output var logic [31:0] tile_references_o,
    output var logic [31:0] triangles_culled_o,

    // The seam itself, counted. `pixels_written_o` is FBWRITE's view and says
    // nothing about whether RESOLVE ever offered a pixel; these two separate
    // "the producer never spoke" from "the consumer never listened".
    output var logic [31:0] px_offered_o,
    output var logic [31:0] px_taken_o
);

  // ---- the seam ------------------------------------------------------------
  logic               px_valid, px_ready, px_last;
  logic        [15:0] px_rgb565;
  logic signed [11:0] px_x, px_y;

  logic [15:0] fb_src_id_unused;
  logic [ 7:0] fb_tag_unused, fb_addr_unused;
  logic [31:0] tile_crc_unused, ez_rej_unused, arena_used_unused;
  logic [15:0] tile_crc_index_unused, max_depth_unused;
  logic [ 8:0] tile_cov_unused;
  logic        tile_done_unused, tile_degen_unused, arena_full_unused;

  zhao_geom_bin_pipe u_bin (
      .clk(clk), .rst_n(rst_n),
      .frame_begin_i(frame_begin_i), .frame_end_i(frame_end_i),
      .grid_w_i(grid_w_i), .grid_h_i(grid_h_i),
      .tri_valid_i(tri_valid_i), .tri_ready_o(tri_ready_o),
      .tri_kx0_i(tri_kx0_i), .tri_ky0_i(tri_ky0_i), .tri_kc0_i(tri_kc0_i),
      .tri_kx1_i(tri_kx1_i), .tri_ky1_i(tri_ky1_i), .tri_kc1_i(tri_kc1_i),
      .tri_kx2_i(tri_kx2_i), .tri_ky2_i(tri_ky2_i), .tri_kc2_i(tri_kc2_i),
      .tri_tl_i(tri_tl_i),
      .tri_ax_i(tri_ax_i), .tri_ay_i(tri_ay_i),
      .tri_bx_i(tri_bx_i), .tri_by_i(tri_by_i),
      .tri_cx_i(tri_cx_i), .tri_cy_i(tri_cy_i),
      .tri_min_x_i(tri_min_x_i), .tri_max_x_i(tri_max_x_i),
      .tri_min_y_i(tri_min_y_i), .tri_max_y_i(tri_max_y_i),
      .tri_src_id_i(tri_src_id_i),
      .tok_req_o(tok_req_o), .tok_grant_i(tok_grant_i),
      .job_fill_word_i(job_fill_word_i), .job_clear_word_i(job_clear_word_i),
      .job_state_i(job_state_i), .job_src_a_i(job_src_a_i),
      .job_texel_rgb_i(job_texel_rgb_i), .job_texel_a_i(job_texel_a_i),
      .job_texel_idx_i(job_texel_idx_i),
      // THE SEAM. `px_ready` is FBWRITE's own ready, so a slow memory stalls
      // the resolve, the tile store, the fragment path and the binner's drain
      // through this one wire.
      .fb_valid_o(px_valid), .fb_ready_i(px_ready),
      .fb_rgb565_o(px_rgb565),
      .fb_tag_o(fb_tag_unused), .fb_addr_o(fb_addr_unused),
      .fb_x_o(px_x), .fb_y_o(px_y), .fb_last_o(px_last),
      .fb_src_id_o(fb_src_id_unused),
      .tile_crc_o(tile_crc_unused), .tile_crc_index_o(tile_crc_index_unused),
      .tile_done_o(tile_done_unused), .tile_cov_count_o(tile_cov_unused),
      .tile_degenerate_o(tile_degen_unused),
      .drain_busy_o(drain_busy_o), .drain_done_o(drain_done_o),
      .tile_references_o(tile_references_o),
      .max_tile_list_depth_o(max_depth_unused),
      .triangles_culled_o(triangles_culled_o),
      .overflow_o(overflow_o),
      .arena_full_o(arena_full_unused),
      .jobs_taken_o(jobs_taken_o),
      .job_stall_clocks_o(job_stall_clocks_o),
      .early_z_rejects_o(ez_rej_unused),
      .fragment_error_o(fragment_error_o)
  );

  zhao_raster_fbwrite u_fbw (
      .clk(clk), .rst_n(rst_n),
      .fb_base_i(fb_base_i), .fb_stride_i(fb_stride_i),
      .px_valid_i(px_valid), .px_ready_o(px_ready),
      .px_rgb565_i(px_rgb565), .px_x_i(px_x), .px_y_i(px_y), .px_last_i(px_last),
      .frame_end_i(frame_end_i),
      .retire_words_i(retire_words_i),
      .guard_req_o(guard_req_o), .guard_rsp_i(guard_rsp_i),
      .guard_wdata_o(guard_wdata_o), .guard_wvalid_o(guard_wvalid_o),
      .guard_wready_i(guard_wready_i), .guard_wlast_o(guard_wlast_o),
      .pixels_written_o(pixels_written_o),
      .bursts_issued_o(bursts_issued_o),
      .stall_clocks_o(fb_stall_clocks_o),
      .stream_error_o(fb_stream_error_o),
      .issued_words_o(fb_issued_words_o),
      .retired_words_o(fb_retired_words_o),
      .drained_o(fb_drained_o),
      .fatal_error_o(fb_fatal_o),
      .busy_o(fb_busy_o)
  );

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      px_offered_o <= 32'd0;
      px_taken_o   <= 32'd0;
    end else begin
      if (px_valid)             px_offered_o <= px_offered_o + 32'd1;
      if (px_valid && px_ready) px_taken_o   <= px_taken_o + 32'd1;
    end
  end

  logic unused_ok;
  assign unused_ok = |{fb_src_id_unused, fb_tag_unused, fb_addr_unused,
                       tile_crc_unused, tile_crc_index_unused, tile_cov_unused,
                       tile_done_unused, tile_degen_unused, arena_full_unused,
                       max_depth_unused, ez_rej_unused, arena_used_unused, 1'b0};

endmodule : zhao_probe_render_fb

`default_nettype wire
