module counter #(parameter int W = 8)(input logic clk, input logic rst_n, output logic [W-1:0] count);
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) count <= 0;
    else count <= count + 1;
  end
endmodule
