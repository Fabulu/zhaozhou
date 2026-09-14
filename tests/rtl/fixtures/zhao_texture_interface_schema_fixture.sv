/* verilator lint_off ASCRANGE */
module zhao_texture_interface_schema_fixture #(
    parameter int unsigned W = 96
) (
    input wire logic clk_i,
    input var logic [W-1:0] wide_i,
    input var logic signed [0:7] ascending_signed_i,
    input var logic [zhao_render_texture_pkg::AUX_SURFACE_CTX_W-1:0]
        package_wide_i,
    output var logic [31:0] counters_o [2:1][5:7]
);

    always_comb begin
        counters_o[2][5] = {32{clk_i}};
        counters_o[2][6] = wide_i[31:0];
        counters_o[2][7] = wide_i[63:32];
        counters_o[1][5] = wide_i[95:64];
        counters_o[1][6] = {24'd0, ascending_signed_i};
        counters_o[1][7] = package_wide_i[31:0];
    end

endmodule
/* verilator lint_on ASCRANGE */
