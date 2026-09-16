// zhao_texture_early_desc_v2_slotswap_mutant.sv -- DELIBERATELY BROKEN.
//
// Positive control for the held owner/data join. The response owner changes only
// on rd_valid&&rd_ready, but every offered rd_valid overwrites the eight payload
// slices and stored generation. Offering B while held A is backpressured creates
// exactly {owner A, descriptor B}; the ordinary scoreboard must fire.
`default_nettype none

module zhao_texture_early_desc_v2_slotswap_mutant #(
    parameter int unsigned SLOTW = 6,
    parameter int unsigned GENW  = 8
) (
    input logic clk, input logic rst_n, input logic frame_fault_clear_i,
    input logic wr_valid_i,
    input logic [SLOTW-1:0] wr_slot_i,
    input logic [GENW-1:0] wr_owner_generation_i,
    input logic [zhao_render_texture_pkg::AUX_SURFACE_CTX_W-1:0] wr_aux_context_i,
    input logic [7:0] wr_lod_q4_4_i,
    input logic [1:0] wr_response_class_i,
    input logic wr_aux_required_i,
    input logic [1:0] wr_sample_count_i,
    input logic [1:0] wr_palette_slot_i,
    input logic [7:0] wr_palette_generation_i,
    input logic [7:0] wr_mosaic_material_a_i,
    input logic [7:0] wr_mosaic_material_b_i,
    input logic [7:0] wr_mosaic_weight_i,
    input logic [7:0] wr_binding_selector_i,
    input logic [7:0] wr_active_page_generation_i,
    input logic rd_valid_i,
    output logic rd_ready_o,
    input logic [SLOTW+GENW-1:0] rd_owner_i,
    output logic rd_result_valid_o,
    input logic rd_result_ready_i,
    output logic [SLOTW+GENW-1:0] rd_owner_o,
    output logic [286:0] rd_logical_o,
    // TIMING4 E1: same unmasked view the production leaf now exposes.
    output logic [286:0] rd_logical_raw_o,
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
  import zhao_render_texture_pkg::*;
  localparam int unsigned ROWS = 1 << SLOTW;

  logic [319:0] wr_physical_c;
  assign wr_physical_c = {
      33'b0, wr_active_page_generation_i, wr_binding_selector_i,
      wr_mosaic_weight_i, wr_mosaic_material_b_i,
      wr_mosaic_material_a_i, wr_palette_generation_i,
      wr_palette_slot_i, wr_sample_count_i, wr_aux_required_i,
      wr_response_class_i, wr_lod_q4_4_i, wr_aux_context_i};

  logic [GENW-1:0] owner_generation_m [ROWS];
  logic [319:0] rd_physical_q;
  genvar k;
  generate
    for (k = 0; k < 8; k = k + 1) begin : g_slice
      logic [39:0] slice_m [ROWS];
      always_ff @(posedge clk) begin
        if (wr_valid_i)
          slice_m[wr_slot_i] <= wr_physical_c[k*40 +: 40];
        // MUTATION: production gates this with rd_ready_o as well.
        if (rd_valid_i)
          rd_physical_q[k*40 +: 40] <=
              slice_m[rd_owner_i[GENW +: SLOTW]];
      end
    end
  endgenerate

  logic rd_v_q, rd_checked_q;
  logic [SLOTW+GENW-1:0] rd_owner_q;
  logic [GENW-1:0] rd_stored_generation_q;
  wire rd_accept_c = rd_valid_i && rd_ready_o;
  wire pad_bad_c = |rd_physical_q[319:287];
  wire generation_bad_c =
      rd_stored_generation_q != rd_owner_q[GENW-1:0];

  assign rd_ready_o = !rd_v_q || rd_result_ready_i;
  assign rd_result_valid_o = rd_v_q;
  assign rd_owner_o = rd_owner_q;
  assign rd_owner_generation_ok_o = !generation_bad_c;
  assign rd_descriptor_pad_ok_o = !pad_bad_c;
  assign rd_descriptor_usable_o = rd_v_q && !pad_bad_c && !generation_bad_c;
  assign rd_logical_o = rd_descriptor_usable_o ? rd_physical_q[286:0] : '0;
  assign rd_logical_raw_o = rd_physical_q[286:0];
  assign idle_o = !rd_v_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      rd_v_q <= 1'b0;
      rd_checked_q <= 1'b1;
      rd_owner_q <= '0;
      rd_stored_generation_q <= '0;
      desc_pad_fault_o <= 32'd0;
      rd_generation_mismatch_o <= 32'd0;
      writes_o <= 32'd0;
      reads_o <= 32'd0;
      frame_fault_o <= 1'b0;
    end else begin
      if (frame_fault_clear_i) frame_fault_o <= 1'b0;
      if (wr_valid_i) begin
        owner_generation_m[wr_slot_i] <= wr_owner_generation_i;
        writes_o <= writes_o + 32'd1;
      end
      if (rd_v_q && !rd_checked_q) begin
        rd_checked_q <= 1'b1;
        if (pad_bad_c) begin
          desc_pad_fault_o <= desc_pad_fault_o + 32'd1;
          frame_fault_o <= 1'b1;
        end
        if (generation_bad_c) begin
          rd_generation_mismatch_o <= rd_generation_mismatch_o + 32'd1;
          frame_fault_o <= 1'b1;
        end
      end
      // MUTATION: data generation follows any offered address; owner does not.
      if (rd_valid_i)
        rd_stored_generation_q <=
            owner_generation_m[rd_owner_i[GENW +: SLOTW]];
      if (rd_accept_c) begin
        rd_v_q <= 1'b1;
        rd_checked_q <= 1'b0;
        rd_owner_q <= rd_owner_i;
        reads_o <= reads_o + 32'd1;
      end else if (rd_v_q && rd_result_ready_i) begin
        rd_v_q <= 1'b0;
      end
    end
  end
endmodule : zhao_texture_early_desc_v2_slotswap_mutant

`default_nettype wire
