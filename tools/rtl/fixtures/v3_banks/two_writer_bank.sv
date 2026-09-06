// FIXTURE -- DELIBERATELY WRONG. Not built, not fitted, never instantiated.
//
// AUX_RESULT with a TMU write port and an AUX write port. Section 6 declares
// exactly one writer ("AUX commit"), and section 26.1 says the completion-bank
// experiment must not introduce new multiwrite payload state.
// Expect V3-MULTIWRITE.
module fx_two_writer_bank (
    input  var logic        clk,
    input  var logic        tmu_en,
    input  var logic [5:0]  tmu_addr,
    input  var logic [39:0] tmu_data,
    input  var logic        aux_en,
    input  var logic [5:0]  aux_addr,
    input  var logic [39:0] aux_data,
    input  var logic [5:0]  rd_addr,
    output var logic [39:0] rd_data
);
  // V3-BANK: AUX_RESULT
  (* ramstyle = "M10K" *) logic [39:0] aux_result_m [0:63];

  always_ff @(posedge clk) begin
    if (tmu_en) aux_result_m[tmu_addr] <= tmu_data;
    if (aux_en) aux_result_m[aux_addr] <= aux_data;
    rd_data <= aux_result_m[rd_addr];
  end
endmodule
