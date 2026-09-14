// zhao_texture_early_desc_v2.sv -- Packet-B owner-keyed early descriptor.
//
// The logical ABI is 287 bits, low field first.  The physical image is not an
// inferred widening: it is exactly {33'b0, logical287}, stored in eight static
// 40-bit held-read banks.  This block owns no allocator or lifetime cursor; the
// caller supplies the slot and owner generation allocated by zhao_texture_v3own.
`default_nettype none

module zhao_texture_early_desc_v2 #(
    parameter int unsigned SLOTW = 6,
    parameter int unsigned GENW  = 8,
    // Test-only elaboration control used solely by the committed renamed wrapper.
    parameter bit PAD_FAULT_MUTANT = 1'b0
) (
    input  logic clk,
    input  logic rst_n,

    // Sticky frame state is independently clearable.  The counters below are
    // reset-only and therefore do not alias frame lifetime.
    input  logic frame_fault_clear_i,

    // One write on the exact owner-admission edge.
    input  logic                 wr_valid_i,
    input  logic [SLOTW-1:0]     wr_slot_i,
    input  logic [GENW-1:0]      wr_owner_generation_i,
    input  logic [zhao_render_texture_pkg::AUX_SURFACE_CTX_W-1:0]
                                      wr_aux_context_i,
    input  logic [7:0]           wr_lod_q4_4_i,
    input  logic [1:0]           wr_response_class_i,
    input  logic                 wr_aux_required_i,
    input  logic [1:0]           wr_sample_count_i,
    input  logic [1:0]           wr_palette_slot_i,
    input  logic [7:0]           wr_palette_generation_i,
    input  logic [7:0]           wr_mosaic_material_a_i,
    input  logic [7:0]           wr_mosaic_material_b_i,
    input  logic [7:0]           wr_mosaic_weight_i,
    input  logic [7:0]           wr_binding_selector_i,
    input  logic [7:0]           wr_active_page_generation_i,

    // One synchronous held read.  The offered owner is captured independently
    // from the stored generation and is returned unchanged with the verdict.
    input  logic                 rd_valid_i,
    output logic                 rd_ready_o,
    input  logic [SLOTW+GENW-1:0] rd_owner_i,

    output logic                 rd_result_valid_o,
    input  logic                 rd_result_ready_i,
    output logic [SLOTW+GENW-1:0] rd_owner_o,
    output logic [286:0]         rd_logical_o,
    output logic                 rd_owner_generation_ok_o,
    output logic                 rd_descriptor_pad_ok_o,
    output logic                 rd_descriptor_usable_o,

    // Required exact observation port plus independent generation/fault state.
    output logic [31:0]          desc_pad_fault_o,
    output logic [31:0]          rd_generation_mismatch_o,
    output logic [31:0]          writes_o,
    output logic [31:0]          reads_o,
    output logic                 frame_fault_o,
    output logic                 idle_o
);
  import zhao_render_texture_pkg::*;

  localparam int unsigned ROWS      = 1 << SLOTW;
  localparam int unsigned LOGICAL_W = 287;
  localparam int unsigned PHYSICAL_W= 320;
  localparam int unsigned SLICE_W   = 40;
  localparam int unsigned SLICES    = 8;
  localparam int unsigned PAD_W     = 33;

  // Frozen low-first logical layout.
  localparam int unsigned AUX_CONTEXT_LO            = 0;
  localparam int unsigned LOD_Q4_4_LO               = 224;
  localparam int unsigned RESPONSE_CLASS_LO         = 232;
  localparam int unsigned AUX_REQUIRED_LO           = 234;
  localparam int unsigned SAMPLE_COUNT_LO           = 235;
  localparam int unsigned PALETTE_SLOT_LO           = 237;
  localparam int unsigned PALETTE_GENERATION_LO     = 239;
  localparam int unsigned MOSAIC_MATERIAL_A_LO      = 247;
  localparam int unsigned MOSAIC_MATERIAL_B_LO      = 255;
  localparam int unsigned MOSAIC_WEIGHT_LO          = 263;
  localparam int unsigned BINDING_SELECTOR_LO       = 271;
  localparam int unsigned ACTIVE_PAGE_GENERATION_LO = 279;

  typedef struct packed {
    logic [7:0]                    active_page_generation; // [286:279]
    logic [7:0]                    binding_selector;       // [278:271]
    logic [7:0]                    mosaic_weight;          // [270:263]
    logic [7:0]                    mosaic_material_b;      // [262:255]
    logic [7:0]                    mosaic_material_a;      // [254:247]
    logic [7:0]                    palette_generation;     // [246:239]
    logic [1:0]                    palette_slot;           // [238:237]
    logic [1:0]                    sample_count;           // [236:235]
    logic                          aux_required;           // [234]
    logic [1:0]                    response_class;         // [233:232]
    logic [7:0]                    lod_q4_4;                // [231:224]
    zhao_aux_surface_ctx_v2_t      aux_context;             // [223:0]
  } descriptor_logical_t;

  descriptor_logical_t wr_logical_t;
  logic [LOGICAL_W-1:0]  wr_logical_c;
  logic [PHYSICAL_W-1:0] wr_physical_c;

  always_comb begin
    wr_logical_t.active_page_generation = wr_active_page_generation_i;
    wr_logical_t.binding_selector       = wr_binding_selector_i;
    wr_logical_t.mosaic_weight          = wr_mosaic_weight_i;
    wr_logical_t.mosaic_material_b      = wr_mosaic_material_b_i;
    wr_logical_t.mosaic_material_a      = wr_mosaic_material_a_i;
    wr_logical_t.palette_generation     = wr_palette_generation_i;
    wr_logical_t.palette_slot           = wr_palette_slot_i;
    wr_logical_t.sample_count           = wr_sample_count_i;
    wr_logical_t.aux_required           = wr_aux_required_i;
    wr_logical_t.response_class         = wr_response_class_i;
    wr_logical_t.lod_q4_4               = wr_lod_q4_4_i;
    wr_logical_t.aux_context            =
        zhao_aux_surface_ctx_v2_t'(wr_aux_context_i);
  end
  assign wr_logical_c  = wr_logical_t;
  // Packet-B physical law.  The pad is a literal constant, not RAM residue or
  // an implicit extension.
  assign wr_physical_c = PAD_FAULT_MUTANT
      ? {32'b0, 1'b1, wr_logical_c}
      : {33'b0, wr_logical_c};

  // One separate generation bank.  Payload RAM is deliberately not reset.
  logic [GENW-1:0] owner_generation_m [ROWS];

  // All eight held read slices land in this one physical response register.
  // Each generated memory is a flat 40-bit-by-ROWS bank; there is no unpacked
  // array of banks for Quartus to mux into flip-flops.
  logic [PHYSICAL_W-1:0] rd_physical_q;
  genvar slice_index;
  generate
    for (slice_index = 0; slice_index < SLICES; slice_index = slice_index + 1)
        begin : g_slice
      logic [SLICE_W-1:0] slice_m [ROWS];
      always_ff @(posedge clk) begin
        if (wr_valid_i)
          slice_m[wr_slot_i] <=
              wr_physical_c[slice_index*SLICE_W +: SLICE_W];
        if (rd_valid_i && rd_ready_o)
          rd_physical_q[slice_index*SLICE_W +: SLICE_W] <=
              slice_m[rd_owner_i[GENW +: SLOTW]];
      end
    end
  endgenerate

  logic                    rd_v_q;
  logic                    rd_checked_q;
  logic [SLOTW+GENW-1:0]   rd_owner_q;
  logic [GENW-1:0]         rd_stored_generation_q;

  wire rd_accept_c = rd_valid_i && rd_ready_o;
  wire rd_retire_c = rd_result_valid_o && rd_result_ready_i;
  wire rd_check_c  = rd_v_q && !rd_checked_q;
  wire pad_bad_c   = |rd_physical_q[PHYSICAL_W-1:LOGICAL_W];
  wire generation_bad_c =
      rd_stored_generation_q != rd_owner_q[GENW-1:0];

  assign rd_ready_o               = !rd_v_q || rd_result_ready_i;
  assign rd_result_valid_o        = rd_v_q;
  assign rd_owner_o               = rd_owner_q;
  assign rd_owner_generation_ok_o = !generation_bad_c;
  assign rd_descriptor_pad_ok_o   = !pad_bad_c;
  assign rd_descriptor_usable_o   = rd_v_q && !generation_bad_c && !pad_bad_c;
  // No bit from a corrupt/stale row escapes as usable work.
  assign rd_logical_o = rd_descriptor_usable_o
                      ? rd_physical_q[LOGICAL_W-1:0] : '0;
  assign idle_o = !rd_v_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      rd_v_q                    <= 1'b0;
      rd_checked_q              <= 1'b1;
      rd_owner_q                <= '0;
      rd_stored_generation_q    <= '0;
      desc_pad_fault_o          <= 32'd0;
      rd_generation_mismatch_o  <= 32'd0;
      writes_o                  <= 32'd0;
      reads_o                   <= 32'd0;
      frame_fault_o             <= 1'b0;
    end else begin
      if (frame_fault_clear_i)
        frame_fault_o <= 1'b0;

      if (wr_valid_i) begin
        owner_generation_m[wr_slot_i] <= wr_owner_generation_i;
        writes_o <= writes_o + 32'd1;
      end

      // A synchronous bank makes the newly captured pad visible after its read
      // edge.  rd_checked_q turns that accepted read into exactly one verdict;
      // a stalled response cannot count twice, and a one-per-clock stream still
      // checks the old response while replacing it with the next one.
      if (rd_check_c) begin
        rd_checked_q <= 1'b1;
        if (pad_bad_c) begin
          desc_pad_fault_o <= desc_pad_fault_o + 32'd1;
          frame_fault_o    <= 1'b1;
        end
        if (generation_bad_c) begin
          rd_generation_mismatch_o <= rd_generation_mismatch_o + 32'd1;
          frame_fault_o             <= 1'b1;
        end
      end

      if (rd_accept_c) begin
        rd_v_q                 <= 1'b1;
        rd_checked_q           <= 1'b0;
        rd_owner_q             <= rd_owner_i;
        rd_stored_generation_q <=
            owner_generation_m[rd_owner_i[GENW +: SLOTW]];
        reads_o                 <= reads_o + 32'd1;
      end else if (rd_retire_c) begin
        rd_v_q <= 1'b0;
      end
    end
  end

  // Quartus 17 requires an explicit initial block for elaboration checks.
  initial begin : p_layout_contract
    if (GENW != 8)
      $fatal(1, "early_desc_v2 requires the Packet-B 8-bit owner generation");
    if (($bits(descriptor_logical_t) != 287) ||
        (LOGICAL_W != 287) || (PHYSICAL_W != 320) ||
        (SLICES != 8) || (SLICE_W != 40) || (PAD_W != 33))
      $fatal(1, "early_desc_v2 layout is not 287 -> 320 -> 8*40");
    if ((AUX_CONTEXT_LO != 0) || (LOD_Q4_4_LO != 224) ||
        (RESPONSE_CLASS_LO != 232) || (AUX_REQUIRED_LO != 234) ||
        (SAMPLE_COUNT_LO != 235) || (PALETTE_SLOT_LO != 237) ||
        (PALETTE_GENERATION_LO != 239) ||
        (MOSAIC_MATERIAL_A_LO != 247) ||
        (MOSAIC_MATERIAL_B_LO != 255) ||
        (MOSAIC_WEIGHT_LO != 263) || (BINDING_SELECTOR_LO != 271) ||
        (ACTIVE_PAGE_GENERATION_LO != 279))
      $fatal(1, "early_desc_v2 field offsets changed");
  end

`ifndef SYNTHESIS
  logic armed_q;
  logic stalled_response_q;
  logic [SLOTW+GENW-1:0] stalled_owner_q;
  logic [PHYSICAL_W-1:0] stalled_physical_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      armed_q <= 1'b0;
      stalled_response_q <= 1'b0;
      stalled_owner_q <= '0;
      stalled_physical_q <= '0;
    end else begin
      armed_q <= 1'b1;
      if (rd_result_valid_o && !rd_result_ready_i) begin
        if (stalled_response_q) begin
          a_response_owner_hold: assert (rd_owner_o == stalled_owner_q);
          a_response_payload_hold: assert (rd_physical_q == stalled_physical_q);
        end
        stalled_response_q <= 1'b1;
        stalled_owner_q <= rd_owner_o;
        stalled_physical_q <= rd_physical_q;
      end else begin
        stalled_response_q <= 1'b0;
      end
    end
  end
  always_ff @(posedge clk) begin
    // The committed renamed wrapper needs the shipped counter to observe its
    // deliberate bad pad before this corroborating assertion can stop simulation.
    if (armed_q && !PAD_FAULT_MUTANT)
      a_pad_write_constant: assert (wr_physical_c[319:287] == 33'b0);
  end
`endif

endmodule : zhao_texture_early_desc_v2

`default_nettype wire
