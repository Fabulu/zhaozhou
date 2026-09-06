// FIXTURE -- NEGATIVE CONTROL. Not built, not fitted, never instantiated.
//
// The shape section 6.3 and Appendix B.6 describe: one writer, one reader,
// a clock-only payload process, and the array read landing directly in a flop
// with nothing in between. check_v3_banks must report this CLEAN. If it ever
// reports a finding here, the gate has become a noise generator.
module fx_good_bank (
    input  var logic        clk,
    input  var logic        rst_n,
    input  var logic        wr_en,
    input  var logic [5:0]  wr_addr,
    input  var logic [39:0] wr_data,
    input  var logic        rd_launch,
    input  var logic [5:0]  rd_addr,
    output var logic        rd_valid,
    output var logic [39:0] rd_data
);
  // V3-BANK: SAMPLE_RESULT_0
  (* ramstyle = "M10K" *) logic [39:0] sample_result_0_m [0:63];

  logic [39:0] ram_q;
  logic        read_v;

  // Payload memory and its output register: posedge clk only.
  always_ff @(posedge clk) begin
    if (wr_en) sample_result_0_m[wr_addr] <= wr_data;
    ram_q <= sample_result_0_m[rd_addr];
  end

  always_ff @(posedge clk) begin
    if (read_v) rd_data <= ram_q;
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      read_v   <= 1'b0;
      rd_valid <= 1'b0;
    end else begin
      read_v   <= rd_launch;
      rd_valid <= read_v;
    end
  end
endmodule
