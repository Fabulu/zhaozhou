`timescale 1ns/1ps
`default_nettype none

module tb_ssone_spec_tests;
  logic clk = 1'b0;
  logic rst_n = 1'b0;

  wire good_done;
  wire good_pass;
  wire [7:0] good_fail;
  wire [7:0] good_active;
  wire [31:0] good_completed;
  wire [31:0] good_signature;

  wire mutant_done;
  wire mutant_pass;
  wire [7:0] mutant_fail;
  wire [7:0] mutant_active;
  wire [31:0] mutant_completed;
  wire [31:0] mutant_signature;

  always #5 clk = ~clk;

  zhao_ssone_spec_tests u_good (
      .clk(clk),
      .rst_n(rst_n),
      .done_o(good_done),
      .pass_o(good_pass),
      .fail_code_o(good_fail),
      .active_test_o(good_active),
      .completed_o(good_completed),
      .signature_o(good_signature)
  );

  zhao_ssone_spec_tests #(.INJECT_FAILURE(1'b1)) u_mutant (
      .clk(clk),
      .rst_n(rst_n),
      .done_o(mutant_done),
      .pass_o(mutant_pass),
      .fail_code_o(mutant_fail),
      .active_test_o(mutant_active),
      .completed_o(mutant_completed),
      .signature_o(mutant_signature)
  );

  integer cycles;
  initial begin
    cycles = 0;
    repeat (3) @(posedge clk);
    rst_n <= 1'b1;

    while (!(good_done && mutant_done) && (cycles < 40)) begin
      @(posedge clk);
      cycles = cycles + 1;
    end

    if (!good_done || !mutant_done)
      $fatal(1, "spec tests timed out: good_active=%0d mutant_active=%0d",
             good_active, mutant_active);
    if (!good_pass || (good_fail != 8'd0) || (good_completed != 32'd16))
      $fatal(1, "shipping vectors failed: pass=%0d fail=%0d completed=%0d",
             good_pass, good_fail, good_completed);
    if (mutant_pass || (mutant_fail != 8'd1) || (mutant_completed != 32'd16))
      $fatal(1, "failure detector did not fire: pass=%0d fail=%0d completed=%0d",
             mutant_pass, mutant_fail, mutant_completed);
    if (good_signature != 32'hE5F1C57F)
      $fatal(1, "datapath signature changed: got=%08x expected=e5f1c57f", good_signature);
    if (good_signature != mutant_signature)
      $fatal(1, "diagnostic injection changed datapath signature: good=%08x mutant=%08x",
             good_signature, mutant_signature);

    $display("SSONE_SPEC_TESTS_PASS vectors=%0d mutant_fail=%0d signature=%08x",
             good_completed, mutant_fail, good_signature);
    $finish;
  end
endmodule

`default_nettype wire
