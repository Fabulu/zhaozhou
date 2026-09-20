// tb_geom_loomfeed_chain.sv -- the carrier and the REAL loom behind it.
//
// The DUT is `zhao_geom_loomfeed` driving `zhao_geom_loom`, not the carrier
// alone, and that is deliberate. The carrier's two hardest laws are both
// statements about the LOOM'S BEHAVIOUR:
//
//   * law 4 -- "record the refusal and play on" -- is only correct because the
//     loom's S_DRAIN swallows beats until the stream's own `last`. A mocked
//     loom would have been written to match whatever the carrier does, which
//     is exactly how the first draft of law 4 came to be backwards.
//   * law 6 -- the flush pass -- depends on S_RUN and S_DRAIN both being
//     released by a `last` and by nothing else.
//
// So the loom is the real one. What IS played here is the HPS-DDR bridge, so a
// case can hand over a stream, a malformed header or a refusal on cue; and the
// palette side, which in the console never stalls.
//
// MAX_NODES = 16 on BOTH, so a stream is a handful of bursts. IDXW stays 10:
// the record's index fields are ten bits and `zhao_geom_loomfeed`'s elaboration
// guard refuses anything else.
`default_nettype none

module tb_geom_loomfeed_chain #(
    parameter int unsigned MAX_NODES = 16,
    parameter int unsigned POSTS     = 2,
    parameter int unsigned RETQ      = 4,
    parameter int unsigned ERR_RETRY_N = 4
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- the doorbell, flattened for the driver ----------------------------
    input  var logic [31:0] cfg_plan_base_i,
    input  var logic        post_valid_i,
    output var logic        post_ready_o,
    input  var logic [31:0] post_base_i,
    input  var logic [31:0] post_ticket_i,

    output var logic        ret_valid_o,
    input  var logic        ret_ready_i,
    output var logic [31:0] ret_ticket_o,
    output var logic        ret_ok_o,
    output var logic        ret_refused_o,
    output var logic [2:0]  ret_reason_o,
    output var logic [2:0]  ret_loom_reason_o,
    output var logic [15:0] ret_nodes_o,
    output var logic [31:0] ret_plan_o,

    // ---- the HPS-DDR bridge, PLAYED by the driver --------------------------
    output var logic        req_valid_o,
    output var logic [31:0] req_addr_o,
    output var logic [6:0]  req_len_o,
    input  var logic        grant_i,
    input  var logic        err_i,
    input  var logic        beat_valid_i,
    input  var logic [63:0] beat_data_i,
    input  var logic        beat_last_i,

    // ---- the carrier's evidence -------------------------------------------
    output var logic [31:0] posts_o,
    output var logic [31:0] streams_o,
    output var logic [31:0] nodes_o,
    output var logic [31:0] bursts_o,
    output var logic [31:0] posts_refused_align_o,
    output var logic [31:0] headers_refused_o,
    output var logic [31:0] streams_refused_o,
    output var logic [31:0] streams_faulted_o,
    output var logic [31:0] streams_replayed_o,
    output var logic [31:0] post_stalls_o,
    output var logic [31:0] bridge_errs_o,
    output var logic [31:0] feed_wait_cycles_o,
    output var logic [31:0] ret_overflow_o,

    // ---- the LOOM's own outputs, so a test can see a transform arrive -----
    output var logic               lm_out_valid_o,
    input  var logic               lm_out_ready_i,
    output var logic [9:0]         lm_out_node_index_o,
    output var logic signed [31:0] lm_out_m0_o,   // element 0 of the 3x4
    output var logic signed [31:0] lm_out_m3_o,   // the x translation
    output var logic [15:0]        lm_out_src_id_o,
    output var logic               lm_out_last_o,
    output var logic [31:0]        lm_nodes_transformed_o,
    output var logic [31:0]        lm_streams_composed_o,
    output var logic [31:0]        lm_refused_framing_o,
    output var logic [31:0]        lm_refused_sorted_o
);

  zhao_pkg::zhao_hps_burst_req_t fd_req;
  zhao_pkg::zhao_hps_burst_rsp_t fd_rsp;

  assign req_valid_o = fd_req.valid;
  assign req_addr_o  = fd_req.addr;
  assign req_len_o   = fd_req.len;

  always_comb begin
    fd_rsp            = '0;
    fd_rsp.beat_valid = beat_valid_i;
    fd_rsp.data       = beat_data_i;
    fd_rsp.last       = beat_last_i;
    fd_rsp.err        = err_i;
  end

  // ---- the carrier -> loom stream -----------------------------------------
  logic               f_valid, f_ready;
  logic [9:0]         f_node, f_parent;
  logic [3:0]         f_kind;
  logic signed [31:0] f_param [12];
  logic [15:0]        f_angle, f_src;
  logic [1:0]         f_axis;
  logic               f_bodypatch, f_first, f_last;
  logic signed [31:0] f_cam [9];

  logic       lm_ref_valid;
  logic [2:0] lm_ref_reason;
  /* verilator lint_off UNUSEDSIGNAL */
  logic [9:0]  lm_ref_node;
  logic [15:0] lm_ref_src;
  logic [15:0] lm_nodes_max, lm_depth_max;
  logic [31:0] lm_kind_hist [10];
  logic [31:0] lm_stall_cycles;
  /* verilator lint_on UNUSEDSIGNAL */
  logic [31:0] lm_refused [6];

  logic signed [31:0] lm_m [12];
  assign lm_out_m0_o = lm_m[0];
  assign lm_out_m3_o = lm_m[3];

  assign lm_refused_sorted_o  = lm_refused[0];
  assign lm_refused_framing_o = lm_refused[5];

  zhao_geom_loomfeed #(
    .MAX_NODES  (MAX_NODES),
    .IDXW       (10),
    .POSTS      (POSTS),
    .RETQ       (RETQ),
    .ERR_RETRY_N(ERR_RETRY_N)
  ) u_feed (
    .clk  (clk),
    .rst_n(rst_n),

    .cfg_plan_base_i(cfg_plan_base_i),
    .post_valid_i   (post_valid_i),
    .post_ready_o   (post_ready_o),
    .post_base_i    (post_base_i),
    .post_ticket_i  (post_ticket_i),

    .ret_valid_o      (ret_valid_o),
    .ret_ready_i      (ret_ready_i),
    .ret_ticket_o     (ret_ticket_o),
    .ret_ok_o         (ret_ok_o),
    .ret_refused_o    (ret_refused_o),
    .ret_reason_o     (ret_reason_o),
    .ret_loom_reason_o(ret_loom_reason_o),
    .ret_nodes_o      (ret_nodes_o),
    .ret_plan_o       (ret_plan_o),

    .hps_req_o  (fd_req),
    .hps_grant_i(grant_i),
    .hps_rsp_i  (fd_rsp),

    .lm_valid_o       (f_valid),
    .lm_ready_i       (f_ready),
    .lm_node_index_o  (f_node),
    .lm_parent_index_o(f_parent),
    .lm_kind_o        (f_kind),
    .lm_param_o       (f_param),
    .lm_angle_o       (f_angle),
    .lm_axis_o        (f_axis),
    .lm_bodypatch_o   (f_bodypatch),
    .lm_src_id_o      (f_src),
    .lm_first_o       (f_first),
    .lm_last_o        (f_last),
    .lm_cam_basis_o   (f_cam),

    .lm_refuse_valid_i (lm_ref_valid),
    .lm_refuse_reason_i(lm_ref_reason),

    .posts_o              (posts_o),
    .streams_o            (streams_o),
    .nodes_o              (nodes_o),
    .bursts_o             (bursts_o),
    .posts_refused_align_o(posts_refused_align_o),
    .headers_refused_o    (headers_refused_o),
    .streams_refused_o    (streams_refused_o),
    .streams_faulted_o    (streams_faulted_o),
    .streams_replayed_o   (streams_replayed_o),
    .post_stalls_o        (post_stalls_o),
    .bridge_errs_o        (bridge_errs_o),
    .feed_wait_cycles_o   (feed_wait_cycles_o),
    .ret_overflow_o       (ret_overflow_o)
  );

  zhao_geom_loom #(
    .MAX_NODES(MAX_NODES),
    .IDXW     (10),
    .MUL_LANES(1)
  ) u_loom (
    .clk  (clk),
    .rst_n(rst_n),

    .in_valid_i       (f_valid),
    .in_ready_o       (f_ready),
    .in_node_index_i  (f_node),
    .in_parent_index_i(f_parent),
    .in_kind_i        (f_kind),
    .in_param_i       (f_param),
    .in_angle_i       (f_angle),
    .in_axis_i        (f_axis),
    .in_bodypatch_i   (f_bodypatch),
    .in_src_id_i      (f_src),
    .in_first_i       (f_first),
    .in_last_i        (f_last),
    .cam_basis_i      (f_cam),

    .out_valid_o     (lm_out_valid_o),
    .out_ready_i     (lm_out_ready_i),
    .out_node_index_o(lm_out_node_index_o),
    .out_m_o         (lm_m),
    .out_src_id_o    (lm_out_src_id_o),
    .out_last_o      (lm_out_last_o),

    .refuse_valid_o     (lm_ref_valid),
    .refuse_reason_o    (lm_ref_reason),
    .refuse_node_index_o(lm_ref_node),
    .refuse_src_id_o    (lm_ref_src),

    .nodes_transformed_o   (lm_nodes_transformed_o),
    .streams_composed_o    (lm_streams_composed_o),
    .streams_refused_o     (lm_refused),
    .nodes_per_stream_max_o(lm_nodes_max),
    .chain_depth_max_o     (lm_depth_max),
    .node_kind_hist_o      (lm_kind_hist),
    .consumer_stall_cycles_o(lm_stall_cycles)
  );

endmodule

`default_nettype wire
