// tb_geom_loomfeed_mutant.sv -- the driver harness for the loomfeed positive
// control. A WRAPPER, not a copy: it instantiates
// `zhao_geom_loomfeed_mutant` and flattens the HPS-DDR bridge so the C++ can
// play it. Nothing here is mutated and nothing here is duplicated from
// production, so `tools/budget/mutant_copy_drift.py` correctly does not treat
// it as a copy.
//
// The LOOM is not present. This control is about the return queue's credit,
// which is upstream of every node beat -- the node port is simply always ready
// and never refuses, which is the condition under which the credit is the only
// thing bounding the queue. Putting a loom here would add a second reason for
// the queue to be short and weaken the control.
`default_nettype none

module tb_geom_loomfeed_mutant #(
    parameter int unsigned MAX_NODES = 16,
    parameter int unsigned POSTS     = 2,
    parameter int unsigned RETQ      = 4
) (
    input  var logic clk,
    input  var logic rst_n,

    input  var logic [31:0] cfg_plan_base_i,
    input  var logic        post_valid_i,
    output var logic        post_ready_o,
    input  var logic [31:0] post_base_i,
    input  var logic [31:0] post_ticket_i,

    output var logic        ret_valid_o,
    input  var logic        ret_ready_i,

    output var logic        req_valid_o,
    output var logic [31:0] req_addr_o,
    input  var logic        grant_i,
    input  var logic        err_i,
    input  var logic        beat_valid_i,
    input  var logic [63:0] beat_data_i,
    input  var logic        beat_last_i,

    output var logic [31:0] posts_o,
    output var logic [31:0] streams_o,
    output var logic [31:0] ret_overflow_o
);

  zhao_pkg::zhao_hps_burst_req_t fd_req;
  zhao_pkg::zhao_hps_burst_rsp_t fd_rsp;

  assign req_valid_o = fd_req.valid;
  assign req_addr_o  = fd_req.addr;

  always_comb begin
    fd_rsp            = '0;
    fd_rsp.beat_valid = beat_valid_i;
    fd_rsp.data       = beat_data_i;
    fd_rsp.last       = beat_last_i;
    fd_rsp.err        = err_i;
  end

  /* verilator lint_off UNUSEDSIGNAL */
  logic               f_valid;
  logic [9:0]         f_node, f_parent;
  logic [3:0]         f_kind;
  logic signed [31:0] f_param [12];
  logic [15:0]        f_angle, f_src;
  logic [1:0]         f_axis;
  logic               f_bodypatch, f_first, f_last;
  logic signed [31:0] f_cam [9];
  logic [31:0]        u_nodes, u_bursts, u_align, u_hdr, u_sref, u_sflt, u_srep;
  logic [31:0]        u_stalls, u_errs, u_wait;
  logic [31:0]        u_ret_ticket, u_ret_plan;
  logic [15:0]        u_ret_nodes;
  logic [2:0]         u_ret_reason, u_ret_lreason;
  logic               u_ret_ok, u_ret_refused;
  /* verilator lint_on UNUSEDSIGNAL */

  zhao_geom_loomfeed_mutant #(
    .MAX_NODES(MAX_NODES),
    .IDXW     (10),
    .POSTS    (POSTS),
    .RETQ     (RETQ)
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
    .ret_ticket_o     (u_ret_ticket),
    .ret_ok_o         (u_ret_ok),
    .ret_refused_o    (u_ret_refused),
    .ret_reason_o     (u_ret_reason),
    .ret_loom_reason_o(u_ret_lreason),
    .ret_nodes_o      (u_ret_nodes),
    .ret_plan_o       (u_ret_plan),

    .hps_req_o  (fd_req),
    .hps_grant_i(grant_i),
    .hps_rsp_i  (fd_rsp),

    .lm_valid_o       (f_valid),
    .lm_ready_i       (1'b1),
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

    .lm_refuse_valid_i (1'b0),
    .lm_refuse_reason_i(3'd0),

    .posts_o              (posts_o),
    .streams_o            (streams_o),
    .nodes_o              (u_nodes),
    .bursts_o             (u_bursts),
    .posts_refused_align_o(u_align),
    .headers_refused_o    (u_hdr),
    .streams_refused_o    (u_sref),
    .streams_faulted_o    (u_sflt),
    .streams_replayed_o   (u_srep),
    .post_stalls_o        (u_stalls),
    .bridge_errs_o        (u_errs),
    .feed_wait_cycles_o   (u_wait),
    .ret_overflow_o       (ret_overflow_o)
  );

endmodule

`default_nettype wire
