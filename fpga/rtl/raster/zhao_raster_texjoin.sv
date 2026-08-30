// zhao_raster_texjoin.sv — TEXJOIN: a surviving fragment, its perspective
// coordinates, its texture samples, and the context that has to still be there
// when they come back.
//
// ENFORCED-BY: tests/raster/raster_texjoin_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHAT RULING 7 ASKED FOR
// ---------------------------------------------------------------------------
//     survivor + affine attributes
//       -> context FIFO
//       -> perspective U/V
//       -> primary TMU request
//       -> optional AUX request IN PARALLEL
//       -> ordered rejoin
//       -> existing RASTER.FRAGMENT
//
// The context record holds tile/pixel address, depth, fragment state, lit
// colour and alpha, material binding, LOD, source id and an INTERNAL SEQUENCE
// NUMBER. This block owns the FIFO, the sequence numbers, the concurrency and
// the rejoin; it owns none of the arithmetic, which is `zhao_raster_perspuv`'s,
// and none of the sampling, which is the TMU's.
//
// ---------------------------------------------------------------------------
// THE SEQUENCE NUMBER IS THE WHOLE POINT, AND IT IS NOT THE SOURCE ID
// ---------------------------------------------------------------------------
// Ruling 7 says it in one line: "Never use the external source id as
// transaction identity." The reason is not stylistic. `src_id` identifies the
// DRAW that produced a fragment, so every fragment of one triangle carries the
// same one -- and a triangle covers hundreds of pixels. A block that keyed its
// rejoin on `src_id` would match a returning sample against the wrong pixel of
// the same triangle, producing a picture that is smeared within each primitive
// and perfect at every edge, which is close to the hardest artefact to trace.
//
// So this block mints its own sequence, sends it in the TMU's tag field, keeps
// the external `src_id` inside the context FIFO where it belongs, and CHECKS
// the returning tag against the head of the FIFO. A mismatch raises
// `seq_error_o` rather than being consumed -- because the alternative is
// shading a pixel with another pixel's texel and never finding out.
//
// The sequence is wide enough that it cannot alias within the window: a tag can
// only be outstanding while its context sits in a DEPTH-entry FIFO, so any
// counter wider than DEPTH is unambiguous. It is carried at 16 bits because
// that is what the TMU's tag field is.
//
// ---------------------------------------------------------------------------
// PRIMARY AND AUX ARE CONCURRENT, AND THAT IS MEASURED
// ---------------------------------------------------------------------------
// Ruling 7: "Primary TMU and TEXTURE.AUX must run concurrently for terrain",
// because AUX alone has no reserve against the terrain estimate. Issuing them
// one after the other would be simpler and would silently halve the terrain
// path. So the two request ports are independent -- either may be accepted
// first, or in the same clock -- and the test measures the rate with AUX on and
// off, and fails if enabling AUX costs throughput.
//
// ---------------------------------------------------------------------------
// THE REJOIN NEEDS NO CAM, AND THAT IS A PROPERTY OF THE TMU
// ---------------------------------------------------------------------------
// `zhao_texture_tmu_pipe` retires strictly in acceptance order. Given that, the
// context FIFO's head IS the transaction the next response belongs to, and no
// content-addressable lookup is needed. That is a dependency on a neighbour's
// contract, so it is not assumed quietly: the returning tag is compared against
// the head every time, and the block reports rather than mismatching. If the
// TMU ever reorders, this says so on the first fragment instead of on the
// hundredth frame.
`default_nettype none

module zhao_raster_texjoin #(
    // Fragments in flight. This bounds how much TMU latency the block can hide
    // and nothing else; when it fills, `full_clocks_o` counts the refusals so
    // the depth is a measurement rather than a guess.
    parameter int unsigned DEPTH = 16,
    // The opaque per-fragment record: tile/pixel address, depth, fragment
    // state, lit colour and alpha, material binding, LOD and source id. This
    // block never interprets it, which is what keeps the fragment format a
    // decision for the blocks that own it.
    parameter int unsigned CTXW  = 64
) (
    input var logic clk,
    input var logic rst_n,

    // ---- a surviving fragment ------------------------------------------------
    input  var logic                  f_valid_i,
    output var logic                  f_ready_o,
    input  var logic signed [31:0]    f_uow_i,      // S 8.24
    input  var logic signed [31:0]    f_vow_i,      // S 8.24
    input  var logic        [23:0]    f_invw24_i,   // U 0.0.24
    input  var logic [CTXW-1:0]       f_ctx_i,
    input  var logic                  f_aux_i,      // also wants an AUX sample

    // ---- primary TMU ---------------------------------------------------------
    output var logic                  tmu_valid_o,
    input  var logic                  tmu_ready_i,
    output var logic signed [31:0]    tmu_u_o,      // S 15.16, req_u_i
    output var logic signed [31:0]    tmu_v_o,
    output var logic        [15:0]    tmu_seq_o,    // OUR sequence, in req_src_id_i
    input  var logic                  tmu_rvalid_i,
    output var logic                  tmu_rready_o,
    input  var logic        [23:0]    tmu_rgb_i,
    input  var logic        [ 7:0]    tmu_a_i,
    input  var logic        [15:0]    tmu_rseq_i,   // echoed back in smp_src_id_o

    // ---- AUX, concurrent -----------------------------------------------------
    output var logic                  aux_valid_o,
    input  var logic                  aux_ready_i,
    output var logic signed [31:0]    aux_u_o,
    output var logic signed [31:0]    aux_v_o,
    output var logic        [15:0]    aux_seq_o,
    input  var logic                  aux_rvalid_i,
    output var logic                  aux_rready_o,
    input  var logic        [23:0]    aux_rgb_i,
    input  var logic        [ 7:0]    aux_a_i,
    input  var logic        [15:0]    aux_rseq_i,

    // ---- the joined fragment, IN ORDER ---------------------------------------
    output var logic                  o_valid_o,
    input  var logic                  o_ready_i,
    output var logic [CTXW-1:0]       o_ctx_o,
    output var logic        [23:0]    o_rgb_o,
    output var logic        [ 7:0]    o_a_o,
    output var logic        [23:0]    o_aux_rgb_o,
    output var logic        [ 7:0]    o_aux_a_o,
    output var logic                  o_has_aux_o,
    output var logic                  o_uv_sat_o,   // PERSPUV railed on this one

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0] fragments_o,
    output var logic [31:0] full_clocks_o,
    output var logic [31:0] seq_errors_o,
    output var logic        seq_error_o,   // sticky: a response did not match
    // The divide's own counters, forwarded rather than swallowed. These are how
    // the block says whether the divide or the TMU is the thing refusing, which
    // is the only question worth asking of a pipeline that is running slow.
    output var logic [31:0] uv_fragments_o,
    output var logic [31:0] uv_sat_fragments_o,
    output var logic [31:0] uv_recips_o,
    output var logic [31:0] uv_busy_clocks_o,
    // A fragment reached here with a zero depth. Early-Z should never pass one,
    // so this counting above zero means something upstream is broken -- and the
    // coordinates for that fragment are meaningless, not merely inaccurate.
    output var logic [31:0] uv_depth_zero_o
);

  localparam int unsigned PW = (DEPTH <= 2) ? 1 : $clog2(DEPTH);

  // ---- the context FIFO ----------------------------------------------------
  logic [CTXW-1:0] ctx_q   [DEPTH];
  logic            aux_q   [DEPTH];
  logic [15:0]     seq_q   [DEPTH];
  logic            sat_q   [DEPTH];
  logic [PW:0]     wr_r, rd_r;     // one extra bit so full and empty differ

  logic fifo_empty_c, fifo_full_c;
  assign fifo_empty_c = (wr_r == rd_r);
  assign fifo_full_c  = (wr_r[PW-1:0] == rd_r[PW-1:0]) && (wr_r[PW] != rd_r[PW]);

  logic [PW-1:0] head_c;
  assign head_c = rd_r[PW-1:0];

  // ---- issue-side state, declared before the divide that reads it ---------
  logic [15:0]        seq_ctr_r;
  logic               req_live_r;    // a request is formed and not yet issued
  logic               pri_sent_r, aux_sent_r;
  logic signed [31:0] req_u_r, req_v_r;
  logic [15:0]        req_seq_r;
  logic               req_aux_r;
  // The fragment between accept and request. Only ONE can be there -- the
  // divide takes one at a time and `f_ready_o` waits for it -- so these are
  // registers rather than a lookup. A CAM over the FIFO would work too and
  // would be a search for something whose location is already known.
  logic               inflight_aux_r;
  logic [PW-1:0]      inflight_slot_r;

  // ---- the perspective divide ----------------------------------------------
  logic        pv_vvalid, pv_vready, pv_rvalid, pv_rready, pv_sat, pv_zero;
  logic signed [31:0] pv_u, pv_v;
  logic [15:0] pv_tag;
  zhao_raster_perspuv u_pv (
      .clk              (clk),
      .rst_n            (rst_n),
      .v_valid_i        (pv_vvalid),
      .v_ready_o        (pv_vready),
      .u_over_w_i       (f_uow_i),
      .v_over_w_i       (f_vow_i),
      .invw24_i         (f_invw24_i),
      .tag_i            (seq_ctr_r),
      .r_valid_o        (pv_rvalid),
      .r_ready_i        (pv_rready),
      .u_o              (pv_u),
      .v_o              (pv_v),
      .tag_o            (pv_tag),
      .sat_o            (pv_sat),
      .depth_zero_o     (pv_zero),
      .fragments_o      (uv_fragments_o),
      .sat_fragments_o  (uv_sat_fragments_o),
      .rcp_recips_o     (uv_recips_o),
      .rcp_busy_clocks_o(uv_busy_clocks_o)
  );

  // ---- issue side ----------------------------------------------------------
  // A fragment is accepted only when EVERYTHING it will need is available: a
  // FIFO slot to hold its context, and the divide to take its coordinates. The
  // alternative -- accept now, find out later -- means dropping a fragment
  // whose inputs are already gone.
  assign f_ready_o = !fifo_full_c && pv_vready && !req_live_r;
  assign pv_vvalid = f_valid_i && f_ready_o;
  assign pv_rready = !req_live_r;

  assign tmu_valid_o = req_live_r && !pri_sent_r;
  assign tmu_u_o     = req_u_r;
  assign tmu_v_o     = req_v_r;
  assign tmu_seq_o   = req_seq_r;

  assign aux_valid_o = req_live_r && req_aux_r && !aux_sent_r;
  assign aux_u_o     = req_u_r;
  assign aux_v_o     = req_v_r;
  assign aux_seq_o   = req_seq_r;

  // ---- response side -------------------------------------------------------
  logic        pri_have_r, aux_have_r;
  logic [23:0] pri_rgb_r, aux_rgb_r;
  logic [ 7:0] pri_a_r, aux_a_r;

  // A response is only consumed if it belongs to the head of the FIFO. The TMU
  // retires in order, so this is always true -- and checking it is how the
  // block finds out the day it is not.
  logic pri_match_c, aux_match_c;
  assign pri_match_c = !fifo_empty_c && (tmu_rseq_i == seq_q[head_c]);
  assign aux_match_c = !fifo_empty_c && (aux_rseq_i == seq_q[head_c]);

  assign tmu_rready_o = !pri_have_r;
  assign aux_rready_o = !aux_have_r;

  logic head_needs_aux_c, joined_c;
  assign head_needs_aux_c = !fifo_empty_c && aux_q[head_c];
  assign joined_c = !fifo_empty_c && pri_have_r && (!head_needs_aux_c || aux_have_r);

  assign o_valid_o   = joined_c;
  assign o_ctx_o     = fifo_empty_c ? '0 : ctx_q[head_c];
  assign o_rgb_o     = pri_rgb_r;
  assign o_a_o       = pri_a_r;
  assign o_aux_rgb_o = aux_rgb_r;
  assign o_aux_a_o   = aux_a_r;
  assign o_has_aux_o = head_needs_aux_c;
  assign o_uv_sat_o  = fifo_empty_c ? 1'b0 : sat_q[head_c];

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      wr_r          <= '0;
      rd_r          <= '0;
      seq_ctr_r     <= 16'd0;
      req_live_r    <= 1'b0;
      pri_sent_r    <= 1'b0;
      aux_sent_r    <= 1'b0;
      req_u_r       <= 32'sd0;
      req_v_r       <= 32'sd0;
      req_seq_r     <= 16'd0;
      req_aux_r       <= 1'b0;
      inflight_aux_r  <= 1'b0;
      inflight_slot_r <= '0;
      pri_have_r    <= 1'b0;
      aux_have_r    <= 1'b0;
      pri_rgb_r     <= 24'd0;
      aux_rgb_r     <= 24'd0;
      pri_a_r       <= 8'd0;
      aux_a_r       <= 8'd0;
      fragments_o   <= 32'd0;
      full_clocks_o <= 32'd0;
      seq_errors_o  <= 32'd0;
      seq_error_o   <= 1'b0;
      uv_depth_zero_o <= 32'd0;
      for (int unsigned i = 0; i < DEPTH; ++i) begin
        ctx_q[i] <= '0;
        aux_q[i] <= 1'b0;
        seq_q[i] <= 16'd0;
        sat_q[i] <= 1'b0;
      end
    end else begin
      // ---- accept: context in, coordinates to the divide -------------------
      if (f_valid_i && f_ready_o) begin
        ctx_q[wr_r[PW-1:0]] <= f_ctx_i;
        aux_q[wr_r[PW-1:0]] <= f_aux_i;
        seq_q[wr_r[PW-1:0]] <= seq_ctr_r;
        inflight_aux_r      <= f_aux_i;
        inflight_slot_r     <= wr_r[PW-1:0];
        wr_r                <= wr_r + 1'b1;
        seq_ctr_r           <= seq_ctr_r + 16'd1;
      end else if (f_valid_i && fifo_full_c) begin
        // The refusal is COUNTED, because a FIFO depth nobody can see the cost
        // of is a number chosen by taste.
        full_clocks_o <= full_clocks_o + 32'd1;
      end

      // ---- the divide answers: form the request ----------------------------
      if (pv_rvalid && pv_rready) begin
        if (pv_zero) uv_depth_zero_o <= uv_depth_zero_o + 32'd1;
        req_u_r    <= pv_u;
        req_v_r    <= pv_v;
        req_seq_r  <= pv_tag;
        req_live_r <= 1'b1;
        pri_sent_r <= 1'b0;
        aux_sent_r <= 1'b0;
        // The rail flag belongs to the FRAGMENT, not to the sample, so it is
        // stored beside its context, in the slot latched at accept.
        //
        // That slot is the right one because `f_ready_o` requires `!req_live_r`
        // AND the divide's own ready, so at most one fragment is ever between
        // accept and request -- there is nothing else `inflight_slot_r` could
        // be pointing at. The consequence of getting it wrong is a rail flag on
        // a neighbouring pixel, which the ordering and pairing checks over 120
        // fragments would surface.
        // ENFORCED-BY: tests/raster/raster_texjoin_directed.cpp:main
        sat_q[inflight_slot_r] <= pv_sat;
        req_aux_r              <= inflight_aux_r;
      end

      // ---- issue: the two ports are independent ----------------------------
      if (tmu_valid_o && tmu_ready_i) pri_sent_r <= 1'b1;
      if (aux_valid_o && aux_ready_i) aux_sent_r <= 1'b1;
      if (req_live_r &&
          (pri_sent_r || (tmu_valid_o && tmu_ready_i)) &&
          (!req_aux_r || aux_sent_r || (aux_valid_o && aux_ready_i))) begin
        req_live_r <= 1'b0;
      end

      // ---- responses -------------------------------------------------------
      if (tmu_rvalid_i && tmu_rready_o) begin
        if (pri_match_c) begin
          pri_rgb_r  <= tmu_rgb_i;
          pri_a_r    <= tmu_a_i;
          pri_have_r <= 1'b1;
        end else begin
          seq_error_o  <= 1'b1;
          seq_errors_o <= seq_errors_o + 32'd1;
        end
      end
      if (aux_rvalid_i && aux_rready_o) begin
        if (aux_match_c) begin
          aux_rgb_r  <= aux_rgb_i;
          aux_a_r    <= aux_a_i;
          aux_have_r <= 1'b1;
        end else begin
          seq_error_o  <= 1'b1;
          seq_errors_o <= seq_errors_o + 32'd1;
        end
      end

      // ---- retire ----------------------------------------------------------
      if (o_valid_o && o_ready_i) begin
        rd_r        <= rd_r + 1'b1;
        pri_have_r  <= 1'b0;
        aux_have_r  <= 1'b0;
        fragments_o <= fragments_o + 32'd1;
      end
    end
  end

endmodule : zhao_raster_texjoin

`default_nettype wire
