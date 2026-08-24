// zhao_pair_tmu_cache.sv — CHARACTERIZATION WRAPPER, not a console block.
//
// The audit's second named pair: "fit representative PAIRS, because the seam
// becomes internal: TMU+CACHE, FRAGMENT+TILESTORE, SETUP+BINNER, TESS+NORMALS".
// TESS+NORMALS was measured first and answered the question the audit asked --
// 1,045 virtual pins collapsed to 67, so 94% of the boundary a leaf fit measures
// is fictional, and the pair's real Fmax was 31.1 MHz against a 100 MHz target.
//
// This is the same instrument aimed at the texture path, which the docket has
// wanted since the TMU's 199.72 MHz turned out to be 36.92 once the SDC
// constrained I/O at all.
//
// SAME RULES AS THE FIRST PAIR:
//   * registered stimulus in, registered hash sink out, so nothing the pair
//     produces reaches a pin and no lane can be optimised away for being
//     unobserved;
//   * the memory behind the cache is a REGISTERED responder, not a wire. A
//     combinational fill would delete a real cycle boundary and flatter the
//     result;
//   * limits stated rather than implied -- see below.
//
// WHAT IT DOES NOT MODEL, stated so the number is not over-read:
//   * real texture content or locality. The fill responder returns a function
//     of the address, so hit/miss behaviour is driven by the stimulus pattern
//     and NOT by any real scene. This measures the pair's TIMING, not its hit
//     rate;
//   * memory latency distribution -- the responder answers at a fixed delay;
//   * `smp_ready_i` is tied high: "how fast can the pair run when nothing
//     stalls it", which is the question a clock target asks.
//
// NOT INSTANTIATED BY THE CONSOLE: absent from the shell QSF and ZHAO_SHELL_RTL,
// so it cannot affect the shell fit or the source-list parity gate.

module zhao_pair_tmu_cache (
    input  logic        clk,
    input  logic        rst_n,
    input  logic        stim_valid_i,
    input  logic [31:0] stim_i,
    output logic [31:0] hash_o
);

  // ---- registered stimulus ------------------------------------------------
  logic        req_valid_q;
  logic [31:0] stim_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      req_valid_q <= 1'b0;
      stim_q      <= 32'd0;
    end else begin
      req_valid_q <= stim_valid_i;
      stim_q      <= stim_i;
    end
  end

  // ---- the seam being measured --------------------------------------------
  logic         req_ready;
  logic         cac_valid, cac_ready;
  logic [  3:0] cac_en;
  logic [127:0] cac_addr;
  logic [ 15:0] cac_src_id;
  logic         cac_rvalid, cac_rready;
  logic [ 63:0] cac_data;

  logic        smp_valid;
  logic [23:0] smp_rgb;
  logic [ 7:0] smp_a, smp_idx;
  logic [15:0] smp_src_id;
  logic        mode_error, tmu_idle;
  logic [31:0] texture_samples;

  zhao_texture_tmu u_tmu (
      .clk(clk), .rst_n(rst_n),
      .req_valid_i(req_valid_q), .req_ready_o(req_ready),
      .req_u_i(stim_q), .req_v_i({stim_q[15:0], stim_q[31:16]}),
      .req_base_i(32'h0001_0000), .req_pal_base_i(32'h0002_0000),
      .req_mode_i({28'd0, stim_q[3:0]}),
      .req_lod_i(stim_q[23:16]), .req_src_id_i(stim_q[31:16]),
      .cac_valid_o(cac_valid), .cac_ready_i(cac_ready),
      .cac_en_o(cac_en), .cac_addr_o(cac_addr), .cac_src_id_o(cac_src_id),
      .cac_valid_i(cac_rvalid), .cac_ready_o(cac_rready), .cac_data_i(cac_data),
      .smp_valid_o(smp_valid), .smp_ready_i(1'b1),
      .smp_rgb_o(smp_rgb), .smp_a_o(smp_a), .smp_idx_o(smp_idx),
      .smp_src_id_o(smp_src_id),
      .mode_error_o(mode_error), .idle_o(tmu_idle),
      .texture_samples_o(texture_samples)
  );

  logic        fill_valid, fill_ready;
  logic [31:0] fill_addr;
  logic [15:0] fill_src_id;
  logic        fill_data_valid;
  logic [15:0] fill_data;
  logic [15:0] cac_rsrc;   // named, not empty: this lane forbids empty-by-name
  logic        cache_idle;
  logic [31:0] cache_hits, cache_misses;

  zhao_texture_cache u_cache (
      .clk(clk), .rst_n(rst_n),
      .acc_valid_i(cac_valid), .acc_ready_o(cac_ready),
      .acc_en_i(cac_en), .acc_addr_i(cac_addr), .acc_src_id_i(cac_src_id),
      .smp_valid_o(cac_rvalid), .smp_ready_i(cac_rready),
      .smp_data_o(cac_data), .smp_src_id_o(cac_rsrc),
      .fill_valid_o(fill_valid), .fill_ready_i(fill_ready),
      .fill_addr_o(fill_addr), .fill_src_id_o(fill_src_id),
      .fill_data_valid_i(fill_data_valid), .fill_data_i(fill_data),
      .inv_valid_i(1'b0), .inv_all_i(1'b0), .inv_addr_i(32'd0),
      .idle_o(cache_idle),
      .cache_hits_o(cache_hits), .cache_misses_o(cache_misses)
  );

  // ---- the memory behind the cache, as a REGISTERED responder -------------
  // Two registered stages, so a fill costs real cycles. A combinational answer
  // would delete a boundary the silicon has.
  logic        fill_v1, fill_v2;
  logic [15:0] fill_d1, fill_d2;
  assign fill_ready = 1'b1;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      fill_v1 <= 1'b0; fill_v2 <= 1'b0;
      fill_d1 <= 16'd0; fill_d2 <= 16'd0;
    end else begin
      fill_v1 <= fill_valid;
      fill_d1 <= fill_addr[15:0] ^ fill_addr[31:16] ^ fill_src_id;
      fill_v2 <= fill_v1;
      fill_d2 <= fill_d1;
    end
  end
  assign fill_data_valid = fill_v2;
  assign fill_data       = fill_d2;

  // ---- registered hash sink -----------------------------------------------
  logic [31:0] hash_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) hash_q <= 32'd0;
    else if (smp_valid)
      hash_q <= (hash_q ^ {8'd0, smp_rgb}) + (hash_q << 5)
              + {16'd0, smp_a, smp_idx} + {16'd0, smp_src_id};
    else
      hash_q <= hash_q
              ^ {31'd0, req_ready}
              ^ {31'd0, mode_error}
              ^ {31'd0, tmu_idle}
              ^ {31'd0, cache_idle}
              ^ {16'd0, cac_rsrc}
              ^ texture_samples ^ cache_hits ^ cache_misses;
  end
  assign hash_o = hash_q;

endmodule
