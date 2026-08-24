// zhao_pair_setup_binner.sv — CHARACTERIZATION WRAPPER, not a console block.
//
// The audit's fourth named pair, and the one with the cleanest seam in the
// renderer: zhao_geom_setup's `out_*` map ONE TO ONE onto zhao_geom_binner's
// `tri_*`, with no width or protocol adaptation at all.
//
// It also re-tests, in a composed context, the binner DSP change of 2026-08-24
// (multiply the 23-bit edge slope at 23 bits rather than out of its 36-bit
// accumulator register: 12 -> 6 DSPs, mapped). A leaf map proved the saving; a
// pair fit says whether it survives placement next to its producer.
//
// Three pairs measured so far, and they do NOT agree:
//
//     TESS+NORMALS         31.10 MHz
//     TMU+CACHE            37.25 MHz
//     FRAGMENT+TILESTORE   55.52 MHz
//
// A 1.8x spread, which killed the "uniformly slow renderer" reading I had drawn
// from the first two. FRAGMENT+TILESTORE is the TIGHTEST loop of the three -- a
// read-modify-write across its seam -- and it is the FASTEST, so these blocks
// are limited by their own logic depth rather than by the seams between them.
//
// SETUP is the interesting fourth case because it is almost pure arithmetic:
// three edge functions, each a pair of 21-bit differences and a 48-bit constant
// term. If logic depth is what limits this renderer, SETUP should be slow. If
// it is fast, the depth hypothesis needs narrowing to the blocks with real
// state machines.
//
// SAME RULES: registered stimulus in, registered hash sink out, and EVERY
// decoded field driven from the stimulus -- the discarded TMU wrapper tied
// three request fields to constants and measured a block the tool had deleted.
//
// NOT MODELLED: real triangle distributions, so tile-list depth and overflow
// behaviour are stimulus-driven. This measures TIMING.
//
// NOT INSTANTIATED BY THE CONSOLE: absent from the shell QSF and ZHAO_SHELL_RTL.

module zhao_pair_setup_binner (
    input  logic         clk,
    input  logic         rst_n,
    input  logic         stim_valid_i,
    input  logic [127:0] stim_i,
    output logic [31:0]  hash_o
);

  logic         tri_valid_q;
  logic [127:0] stim_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      tri_valid_q <= 1'b0;
      stim_q      <= 128'd0;
    end else begin
      tri_valid_q <= stim_valid_i;
      stim_q      <= stim_i;
    end
  end

  // ---- SETUP --------------------------------------------------------------
  logic               s_tri_ready, s_out_valid;
  logic signed [22:0] s_kx0, s_ky0, s_kx1, s_ky1, s_kx2, s_ky2;
  logic signed [47:0] s_kc0, s_kc1, s_kc2, s_area2;
  logic        [ 2:0] s_tl;
  logic signed [20:0] s_ax, s_ay, s_bx, s_by, s_cx, s_cy;
  logic signed [11:0] s_minx, s_maxx, s_miny, s_maxy;
  logic        [15:0] s_src;
  logic        [31:0] s_submitted;
  logic               b_tri_ready;

  zhao_geom_setup u_setup (
      .clk(clk), .rst_n(rst_n),
      .tri_valid_i(tri_valid_q), .tri_ready_o(s_tri_ready),
      .tri_ax_i(stim_q[20:0]),   .tri_ay_i(stim_q[41:21]),
      .tri_bx_i(stim_q[62:42]),  .tri_by_i(stim_q[83:63]),
      .tri_cx_i(stim_q[104:84]), .tri_cy_i(stim_q[125:105]),
      .tri_area2_i($signed({stim_q[127:96], stim_q[47:32]})),
      .tri_min_x_i(stim_q[11:0]),   .tri_max_x_i(stim_q[23:12]),
      .tri_min_y_i(stim_q[35:24]),  .tri_max_y_i(stim_q[47:36]),
      .tri_src_id_i(stim_q[63:48]),
      .out_valid_o(s_out_valid), .out_ready_i(b_tri_ready),
      .out_kx0_o(s_kx0), .out_ky0_o(s_ky0), .out_kc0_o(s_kc0),
      .out_kx1_o(s_kx1), .out_ky1_o(s_ky1), .out_kc1_o(s_kc1),
      .out_kx2_o(s_kx2), .out_ky2_o(s_ky2), .out_kc2_o(s_kc2),
      .out_tl_o(s_tl), .out_area2_o(s_area2),
      .out_ax_o(s_ax), .out_ay_o(s_ay), .out_bx_o(s_bx),
      .out_by_o(s_by), .out_cx_o(s_cx), .out_cy_o(s_cy),
      .out_min_x_o(s_minx), .out_max_x_o(s_maxx),
      .out_min_y_o(s_miny), .out_max_y_o(s_maxy),
      .out_src_id_o(s_src),
      .triangles_submitted_o(s_submitted)
  );

  // ---- BINNER -------------------------------------------------------------
  logic               tok_req, job_valid;
  logic signed [20:0] j_ax, j_ay, j_bx, j_by, j_cx, j_cy;
  logic signed [11:0] j_tx, j_ty;
  logic        [15:0] j_src;
  logic               drain_busy, drain_done, overflow, arena_full;
  logic        [31:0] tile_refs, tris_culled;
  logic        [15:0] max_depth;
  logic        [ 8:0] arena_used;   // CHUNK_W+1, from the binner

  zhao_geom_binner u_binner (
      .clk(clk), .rst_n(rst_n),
      .frame_begin_i(stim_q[0] & ~stim_valid_i),
      .frame_end_i(stim_q[1] & ~stim_valid_i),
      .grid_w_i(stim_q[7:2]), .grid_h_i(stim_q[13:8]),
      .tri_valid_i(s_out_valid), .tri_ready_o(b_tri_ready),
      .tri_kx0_i(s_kx0), .tri_ky0_i(s_ky0), .tri_kc0_i(s_kc0),
      .tri_kx1_i(s_kx1), .tri_ky1_i(s_ky1), .tri_kc1_i(s_kc1),
      .tri_kx2_i(s_kx2), .tri_ky2_i(s_ky2), .tri_kc2_i(s_kc2),
      .tri_tl_i(s_tl),
      .tri_ax_i(s_ax), .tri_ay_i(s_ay), .tri_bx_i(s_bx),
      .tri_by_i(s_by), .tri_cx_i(s_cx), .tri_cy_i(s_cy),
      .tri_min_x_i(s_minx), .tri_max_x_i(s_maxx),
      .tri_min_y_i(s_miny), .tri_max_y_i(s_maxy),
      .tri_src_id_i(s_src),
      .tok_req_o(tok_req), .tok_grant_i(1'b1),
      .job_valid_o(job_valid), .job_ready_i(1'b1),
      .job_ax_o(j_ax), .job_ay_o(j_ay), .job_bx_o(j_bx),
      .job_by_o(j_by), .job_cx_o(j_cx), .job_cy_o(j_cy),
      .job_tile_x_o(j_tx), .job_tile_y_o(j_ty), .job_src_id_o(j_src),
      .drain_busy_o(drain_busy), .drain_done_o(drain_done),
      .tile_references_o(tile_refs), .max_tile_list_depth_o(max_depth),
      .triangles_culled_o(tris_culled),
      .overflow_o(overflow), .arena_full_o(arena_full),
      .arena_used_o(arena_used)
  );

  // ---- registered hash sink ----------------------------------------------
  logic [31:0] hash_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) hash_q <= 32'd0;
    else if (job_valid)
      hash_q <= (hash_q ^ {11'd0, j_ax}) + (hash_q << 5)
              + {11'd0, j_ay} + {11'd0, j_bx} + {11'd0, j_by}
              + {11'd0, j_cx} + {11'd0, j_cy}
              + {20'd0, j_tx} + {20'd0, j_ty} + {16'd0, j_src};
    else
      hash_q <= hash_q
              ^ {31'd0, s_tri_ready} ^ {31'd0, tok_req}
              ^ {31'd0, drain_busy}  ^ {31'd0, drain_done}
              ^ {31'd0, overflow}    ^ {31'd0, arena_full}
              ^ {23'd0, arena_used}  ^ {16'd0, max_depth}
              ^ s_submitted ^ tile_refs ^ tris_culled
              ^ s_area2[31:0] ^ {16'd0, s_area2[47:32]};
  end
  assign hash_o = hash_q;

endmodule
