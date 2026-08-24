// zhao_pair_fragment_tilestore.sv — CHARACTERIZATION WRAPPER, not a console block.
//
// The audit's third named pair. Two are measured so far and they agree:
//
//     TESS+NORMALS   31.10 MHz
//     TMU+CACHE      37.25 MHz
//
// both about a third of the 100 MHz target, which suggests the renderer is
// systemically slow rather than holding one bad seam. This wrapper exists to
// test that hypothesis rather than to assume it -- two points are not a trend,
// and a third landing at, say, 80 MHz would kill the reading outright.
//
// FRAGMENT<->TILESTORE is the right third choice because it is the tightest
// loop in the renderer: the fragment pipe READS the tile, decides, and WRITES
// it back, so the seam carries a read-modify-write dependency rather than a
// one-way stream. If any pair is limited by its seam rather than by its own
// logic depth, it should be this one.
//
// SAME RULES, and one learned the hard way:
//   * registered stimulus in, registered hash sink out;
//   * EVERY decoded field driven from the stimulus. The first TMU+CACHE wrapper
//     tied three request fields to constants, the tool folded 85% of the TMU
//     away, and the fit measured a block that did not exist. A characterization
//     wrapper that over-constrains its stimulus measures what survives the
//     folding, not the design;
//   * the tilestore's own read port answers the fragment pipe directly -- this
//     seam needs no synthetic memory model, which is exactly why it is a good
//     pair to measure.
//
// NOT MODELLED: real coverage or depth distributions, so the hit/blend mix is
// driven by stimulus rather than by a scene. This measures TIMING.
//
// NOT INSTANTIATED BY THE CONSOLE: absent from the shell QSF and ZHAO_SHELL_RTL.

module zhao_pair_fragment_tilestore (
    input  logic        clk,
    input  logic        rst_n,
    input  logic        stim_valid_i,
    input  logic [95:0] stim_i,
    output logic [31:0] hash_o
);

  logic        frag_valid_q;
  logic [95:0] stim_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      frag_valid_q <= 1'b0;
      stim_q       <= 96'd0;
    end else begin
      frag_valid_q <= stim_valid_i;
      stim_q       <= stim_i;
    end
  end

  logic        frag_ready;
  logic        rd_valid, rd_ready, rd_rvalid;
  logic [ 7:0] rd_addr;
  logic [15:0] rd_src_id, rd_rsrc;
  logic [63:0] rd_data;
  logic        wr_valid, wr_ready;
  logic [ 7:0] wr_addr;
  logic [63:0] wr_data;
  logic        frag_error, frag_idle;
  logic [31:0] covered, blended;

  zhao_raster_fragment u_frag (
      .clk(clk), .rst_n(rst_n),
      .frag_valid_i(frag_valid_q), .frag_ready_o(frag_ready),
      .frag_addr_i(stim_q[7:0]),
      .frag_depth_i(stim_q[31:8]),
      .frag_state_i(stim_q[63:32]),
      .frag_src_id_i(stim_q[79:64]),
      .frag_vert_rgb_i(stim_q[23:0] ^ stim_q[87:64]),
      .frag_vert_a_i(stim_q[39:32]),
      .frag_tag_i(stim_q[47:40]),
      .frag_sten_ref_i(stim_q[55:48]),
      .frag_texel_rgb_i(stim_q[86:63]),
      .frag_texel_a_i(stim_q[71:64]),
      .frag_texel_idx_i(stim_q[95:88]),
      .rd_valid_o(rd_valid), .rd_ready_i(rd_ready),
      .rd_addr_o(rd_addr), .rd_src_id_o(rd_src_id),
      .rd_valid_i(rd_rvalid), .rd_data_i(rd_data),
      .wr_valid_o(wr_valid), .wr_ready_i(wr_ready),
      .wr_addr_o(wr_addr), .wr_data_o(wr_data),
      .fragment_error_o(frag_error), .idle_o(frag_idle),
      .covered_fragments_o(covered), .blended_fragments_o(blended)
  );

  logic        clear_ready, res_ready, res_rvalid, swap_ready, front_bank;
  logic [63:0] res_data;
  logic [31:0] tile_refs;

  zhao_raster_tilestore u_store (
      .clk(clk), .rst_n(rst_n),
      .clear_valid_i(stim_q[0] & ~stim_valid_i), .clear_ready_o(clear_ready),
      .clear_data_i(stim_q[63:0]),
      .wr_valid_i(wr_valid), .wr_ready_o(wr_ready),
      .wr_addr_i(wr_addr), .wr_data_i(wr_data),
      .rd_valid_i(rd_valid), .rd_ready_o(rd_ready),
      .rd_addr_i(rd_addr), .rd_src_id_i(rd_src_id),
      .rd_valid_o(rd_rvalid), .rd_data_o(rd_data), .rd_src_id_o(rd_rsrc),
      .res_valid_i(stim_q[1] & ~stim_valid_i), .res_ready_o(res_ready),
      .res_addr_i(stim_q[15:8]),
      .res_valid_o(res_rvalid), .res_data_o(res_data),
      .swap_valid_i(stim_q[2] & ~stim_valid_i), .swap_ready_o(swap_ready),
      .front_bank_o(front_bank), .tile_references_o(tile_refs)
  );

  // ---- registered hash sink -----------------------------------------------
  logic [31:0] hash_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) hash_q <= 32'd0;
    else if (res_rvalid)
      hash_q <= (hash_q ^ res_data[31:0]) + (hash_q << 5) + res_data[63:32];
    else
      hash_q <= hash_q
              ^ {31'd0, frag_ready}  ^ {31'd0, frag_error}
              ^ {31'd0, frag_idle}   ^ {31'd0, clear_ready}
              ^ {31'd0, res_ready}   ^ {31'd0, swap_ready}
              ^ {31'd0, front_bank}  ^ {16'd0, rd_rsrc}
              ^ covered ^ blended ^ tile_refs;
  end
  assign hash_o = hash_q;

endmodule
