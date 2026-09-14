// zhao_texture_early_desc_v2_pad_mutant.sv -- DELIBERATELY BROKEN, TEST ONLY.
//
// Renamed wrapper around the exact selected implementation. The sole mutation is
// PAD_FAULT_MUTANT=1, which raises physical pad bit 287. The production block's
// own desc_pad_fault_o detector/counter remains the only detector being tested;
// no copied queue, RAM, verdict, counter, or fault logic exists here.
`default_nettype none

module zhao_texture_early_desc_v2_pad_mutant #(
    parameter int unsigned SLOTW = 6,
    parameter int unsigned GENW  = 8
) (
    input  logic clk,
    input  logic rst_n,
    input  logic frame_fault_clear_i,
    input  logic wr_valid_i,
    input  logic [SLOTW-1:0] wr_slot_i,
    input  logic [GENW-1:0] wr_owner_generation_i,
    input  logic [zhao_render_texture_pkg::AUX_SURFACE_CTX_W-1:0]
                                      wr_aux_context_i,
    input  logic [7:0] wr_lod_q4_4_i,
    input  logic [1:0] wr_response_class_i,
    input  logic wr_aux_required_i,
    input  logic [1:0] wr_sample_count_i,
    input  logic [1:0] wr_palette_slot_i,
    input  logic [7:0] wr_palette_generation_i,
    input  logic [7:0] wr_mosaic_material_a_i,
    input  logic [7:0] wr_mosaic_material_b_i,
    input  logic [7:0] wr_mosaic_weight_i,
    input  logic [7:0] wr_binding_selector_i,
    input  logic [7:0] wr_active_page_generation_i,
    input  logic rd_valid_i,
    output logic rd_ready_o,
    input  logic [SLOTW+GENW-1:0] rd_owner_i,
    output logic rd_result_valid_o,
    input  logic rd_result_ready_i,
    output logic [SLOTW+GENW-1:0] rd_owner_o,
    output logic [286:0] rd_logical_o,
    output logic rd_owner_generation_ok_o,
    output logic rd_descriptor_pad_ok_o,
    output logic rd_descriptor_usable_o,
    output logic [31:0] desc_pad_fault_o,
    output logic [31:0] rd_generation_mismatch_o,
    output logic [31:0] writes_o,
    output logic [31:0] reads_o,
    output logic frame_fault_o,
    output logic idle_o
);
  zhao_texture_early_desc_v2 #(
      .SLOTW(SLOTW), .GENW(GENW), .PAD_FAULT_MUTANT(1'b1)
  ) u_mutant (
      .clk(clk), .rst_n(rst_n),
      .frame_fault_clear_i(frame_fault_clear_i),
      .wr_valid_i(wr_valid_i), .wr_slot_i(wr_slot_i),
      .wr_owner_generation_i(wr_owner_generation_i),
      .wr_aux_context_i(wr_aux_context_i), .wr_lod_q4_4_i(wr_lod_q4_4_i),
      .wr_response_class_i(wr_response_class_i),
      .wr_aux_required_i(wr_aux_required_i),
      .wr_sample_count_i(wr_sample_count_i),
      .wr_palette_slot_i(wr_palette_slot_i),
      .wr_palette_generation_i(wr_palette_generation_i),
      .wr_mosaic_material_a_i(wr_mosaic_material_a_i),
      .wr_mosaic_material_b_i(wr_mosaic_material_b_i),
      .wr_mosaic_weight_i(wr_mosaic_weight_i),
      .wr_binding_selector_i(wr_binding_selector_i),
      .wr_active_page_generation_i(wr_active_page_generation_i),
      .rd_valid_i(rd_valid_i), .rd_ready_o(rd_ready_o),
      .rd_owner_i(rd_owner_i), .rd_result_valid_o(rd_result_valid_o),
      .rd_result_ready_i(rd_result_ready_i), .rd_owner_o(rd_owner_o),
      .rd_logical_o(rd_logical_o),
      .rd_owner_generation_ok_o(rd_owner_generation_ok_o),
      .rd_descriptor_pad_ok_o(rd_descriptor_pad_ok_o),
      .rd_descriptor_usable_o(rd_descriptor_usable_o),
      .desc_pad_fault_o(desc_pad_fault_o),
      .rd_generation_mismatch_o(rd_generation_mismatch_o),
      .writes_o(writes_o), .reads_o(reads_o),
      .frame_fault_o(frame_fault_o), .idle_o(idle_o));
endmodule : zhao_texture_early_desc_v2_pad_mutant

`default_nettype wire
