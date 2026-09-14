// tb_uv_join_v2_pair.sv -- production Packet-B UV join beside both committed
// generation-carriage mutants.
//
// All three instances receive the same independently handshaken descriptor and
// UV streams and share output backpressure.  Their ready/valid control remains
// identical; only the two mutant views of joined365[350:343] may differ.
`default_nettype none

module tb_uv_join_v2_pair (
    input var logic clk,
    input var logic rst_n,

    input  var logic         desc_valid_i,
    output var logic         desc_ready_o,
    input  var logic [13:0]  desc_owner_i,
    input  var logic [286:0] desc_logical_i,

    input  var logic               uv_valid_i,
    output var logic               uv_ready_o,
    input  var logic [13:0]        uv_owner_i,
    input  var logic signed [31:0] uv_u_i,
    input  var logic signed [31:0] uv_v_i,

    input var logic out_ready_i,
    input var logic [7:0] active_page_generation_i,

    // Independent controls for the cadence-only no-reload mutant.  It reuses
    // the descriptor/UV payload pins above but never shares their valid/ready
    // handshakes with the production triplet.
    input  var logic reload_desc_valid_i,
    output var logic reload_desc_ready_o,
    input  var logic reload_uv_valid_i,
    output var logic reload_uv_ready_o,
    input  var logic reload_out_ready_i,

    output var logic         good_valid_o,
    output var logic [364:0] good_data_o,
    output var logic [31:0]  good_mismatch_o,
    output var logic         good_lifetime_fault_o,
    output var logic         good_idle_o,

    output var logic         generation_mutant_valid_o,
    output var logic [364:0] generation_mutant_data_o,
    output var logic [31:0]  generation_mutant_mismatch_o,
    output var logic         generation_mutant_lifetime_fault_o,
    output var logic         generation_mutant_idle_o,

    output var logic         late_mutant_valid_o,
    output var logic [364:0] late_mutant_data_o,
    output var logic [31:0]  late_mutant_mismatch_o,
    output var logic         late_mutant_lifetime_fault_o,
    output var logic         late_mutant_idle_o,

    output var logic         reload_mutant_valid_o,
    output var logic [364:0] reload_mutant_data_o,
    output var logic [31:0]  reload_mutant_mismatch_o,
    output var logic         reload_mutant_lifetime_fault_o,
    output var logic         reload_mutant_idle_o
);

  wire [300:0] desc_data_w = {desc_owner_i, desc_logical_i};
  wire [77:0]  uv_data_w   = {uv_owner_i, uv_u_i, uv_v_i};

  logic generation_desc_ready_w, generation_uv_ready_w;
  logic late_desc_ready_w, late_uv_ready_w;

  zhao_texture_uv_join_v2 u_good (
      .clk                     (clk),
      .rst_n                   (rst_n),
      .desc_valid_i            (desc_valid_i),
      .desc_ready_o            (desc_ready_o),
      .desc_data_i             (desc_data_w),
      .uv_valid_i              (uv_valid_i),
      .uv_ready_o              (uv_ready_o),
      .uv_data_i               (uv_data_w),
      .out_valid_o             (good_valid_o),
      .out_ready_i             (out_ready_i),
      .out_data_o              (good_data_o),
      .uvjoin_owner_mismatch_o (good_mismatch_o),
      .lifetime_fault_o           (good_lifetime_fault_o),
      .idle_o                  (good_idle_o)
  );

  zhao_texture_uv_join_v2_generation_swap_mutant u_generation_mutant (
      .clk                     (clk),
      .rst_n                   (rst_n),
      .desc_valid_i            (desc_valid_i),
      .desc_ready_o            (generation_desc_ready_w),
      .desc_data_i             (desc_data_w),
      .uv_valid_i              (uv_valid_i),
      .uv_ready_o              (generation_uv_ready_w),
      .uv_data_i               (uv_data_w),
      .out_valid_o             (generation_mutant_valid_o),
      .out_ready_i             (out_ready_i),
      .out_data_o              (generation_mutant_data_o),
      .active_page_generation_i(active_page_generation_i),
      .uvjoin_owner_mismatch_o (generation_mutant_mismatch_o),
      .lifetime_fault_o           (generation_mutant_lifetime_fault_o),
      .idle_o                  (generation_mutant_idle_o)
  );

  zhao_texture_uv_join_v2_late_overwrite_mutant u_late_mutant (
      .clk                     (clk),
      .rst_n                   (rst_n),
      .desc_valid_i            (desc_valid_i),
      .desc_ready_o            (late_desc_ready_w),
      .desc_data_i             (desc_data_w),
      .uv_valid_i              (uv_valid_i),
      .uv_ready_o              (late_uv_ready_w),
      .uv_data_i               (uv_data_w),
      .out_valid_o             (late_mutant_valid_o),
      .out_ready_i             (out_ready_i),
      .out_data_o              (late_mutant_data_o),
      .active_page_generation_i(active_page_generation_i),
      .uvjoin_owner_mismatch_o (late_mutant_mismatch_o),
      .lifetime_fault_o           (late_mutant_lifetime_fault_o),
      .idle_o                  (late_mutant_idle_o)
  );

  zhao_texture_uv_join_v2_no_same_edge_reload_mutant u_reload_mutant (
      .clk                     (clk),
      .rst_n                   (rst_n),
      .desc_valid_i            (reload_desc_valid_i),
      .desc_ready_o            (reload_desc_ready_o),
      .desc_data_i             (desc_data_w),
      .uv_valid_i              (reload_uv_valid_i),
      .uv_ready_o              (reload_uv_ready_o),
      .uv_data_i               (uv_data_w),
      .out_valid_o             (reload_mutant_valid_o),
      .out_ready_i             (reload_out_ready_i),
      .out_data_o              (reload_mutant_data_o),
      .uvjoin_owner_mismatch_o (reload_mutant_mismatch_o),
      .lifetime_fault_o           (reload_mutant_lifetime_fault_o),
      .idle_o                  (reload_mutant_idle_o)
  );

`ifndef SYNTHESIS
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      // No testbench state; use the DUT's asynchronous reset style so reset is
      // not ambiguously sampled as both synchronous and asynchronous.
    end else begin
      a_desc_ready_lockstep : assert ((desc_ready_o == generation_desc_ready_w) &&
                                      (desc_ready_o == late_desc_ready_w));
      a_uv_ready_lockstep : assert ((uv_ready_o == generation_uv_ready_w) &&
                                    (uv_ready_o == late_uv_ready_w));
      a_valid_lockstep : assert ((good_valid_o == generation_mutant_valid_o) &&
                                 (good_valid_o == late_mutant_valid_o));
    end
  end
`endif

endmodule : tb_uv_join_v2_pair

`default_nettype wire
