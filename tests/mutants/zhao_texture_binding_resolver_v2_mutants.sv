// zhao_texture_binding_resolver_v2_mutants.sv
//
// DELIBERATELY BROKEN, RENAMED, TEST-ONLY wrappers. NOT SHIPPED.
// Each named top selects exactly one Packet-B failure while retaining the real
// resolver's loader, CRC scan, elastic read/disposition, and counters.
//
// Independent controls:
//   late_page_generation       job generation is replaced by current active page
//   owner_quiet_activation     activation uses owner quiet, not structural quiet
//   active_bank_write          a staging WRITE leaks into active lookup output
//   stale_invalid_crc          invalid selectors contribute stale payload to CRC
//   witness_class_route        sample-0 witness becomes routing authority
//   same_cycle_refusal         terminal refusal appears on the issue cycle
//   refusal_without_issue      local terminal remains but issue pulse is removed
`default_nettype none
/* verilator lint_off DECLFILENAME */

module zhao_texture_binding_resolver_v2_mutant_wrap #(
    parameter int MUTATION = 0,
    parameter int unsigned MAXLOG2 = 11,
    parameter int unsigned SLOTW = 6,
    parameter int unsigned GENW = 8
) (
    input logic clk, input logic rst_n, input logic frame_fault_clear_i,
    input logic cfg_valid_i, output logic cfg_ready_o,
    input logic [1:0] cfg_op_i,
    input logic [7:0] cfg_page_generation_i,
    input logic [7:0] cfg_selector_i,
    input logic [74:0] cfg_row_i,
    input logic [31:0] cfg_crc32_i,
    output logic cfg_rsp_valid_o, input logic cfg_rsp_ready_i,
    output logic [1:0] cfg_rsp_op_o,
    output logic [3:0] cfg_rsp_status_o,
    output logic [7:0] cfg_rsp_page_generation_o,
    input logic data_quiet_i,
    input logic owner_quiet_i,
    output logic admission_enable_o,
    output logic [7:0] active_page_generation_o,
    output logic cfg_loader_idle_o,
    output logic binding_crc_busy_o,
    output logic binding_seal_pending_o,
    input logic req_valid_i, output logic req_ready_o,
    input logic [SLOTW+2+GENW-1:0] req_sample_handle_i,
    input logic [7:0] req_page_generation_i,
    input logic req_selector_overflow_i,
    input logic req_force_refuse_i,
    input logic [7:0] req_binding_selector_i,
    input logic [7:0] req_mosaic_tile_i,
    input logic signed [31:0] req_u_i,
    input logic signed [31:0] req_v_i,
    input logic [7:0] req_lod_q4_4_i,
    input logic [1:0] req_sample0_class_witness_i,
    input logic [1:0] req_sample0_palette_slot_witness_i,
    input logic [7:0] req_sample0_palette_generation_witness_i,
    output logic iss_tmu_valid_o,
    output logic [SLOTW+2+GENW-1:0] iss_tmu_handle_o,
    output logic plan_valid_o, input logic plan_ready_i,
    output logic [2+SLOTW+2+GENW-1:0] plan_route_token_o,
    output logic [31:0] plan_base_o,
    output logic [31:0] plan_mode_o,
    output logic [1:0] plan_palette_slot_o,
    output logic [7:0] plan_palette_generation_o,
    output logic signed [31:0] plan_u_o,
    output logic signed [31:0] plan_v_o,
    output logic [7:0] plan_lod_q4_4_o,
    output logic refuse_valid_o, input logic refuse_ready_i,
    output logic [SLOTW+2+GENW-1:0] refuse_sample_handle_o,
    output logic [zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0] refuse_result_o,
    output logic [31:0] sample_jobs_accepted_o,
    output logic [31:0] planner_jobs_accepted_o,
    output logic [31:0] local_refused_o,
    output logic [31:0] selector_overflow_count_o,
    output logic [31:0] page_generation_mismatch_o,
    output logic [31:0] invalid_row_o,
    output logic [31:0] witness_mismatch_o,
    output logic [31:0] forced_refused_o,
    output logic [31:0] tileset_samples_o,
    output logic [31:0] cfg_errors_o,
    output logic binding_fault_o,
    output logic data_idle_o
);
  localparam int M_LATE_PAGE       = 1;
  localparam int M_OWNER_QUIET     = 2;
  localparam int M_ACTIVE_WRITE    = 3;
  localparam int M_STALE_CRC       = 4;
  localparam int M_WITNESS_CLASS   = 5;
  localparam int M_SAME_CYCLE      = 6;
  localparam int M_WITHOUT_ISSUE   = 7;

  function automatic logic [31:0] crc32_byte(
      input logic [31:0] crc_in, input logic [7:0] data);
    logic [31:0] crc;
    integer bit_index;
    begin
      crc = crc_in;
      for (bit_index = 0; bit_index < 8; bit_index = bit_index + 1)
        crc = (crc[0] ^ data[bit_index])
            ? ((crc >> 1) ^ 32'hEDB88320) : (crc >> 1);
      crc32_byte = crc;
    end
  endfunction

  // Shadow only for the stale-invalid CRC mutation. Payload is reset here because
  // a positive control needs deterministic stale bytes; production payload is
  // intentionally unreset and masks it out.
  logic [74:0] shadow_page0_q [0:255];
  logic [74:0] shadow_page1_q [0:255];
  logic [255:0] shadow_valid0_q, shadow_valid1_q;
  logic shadow_active_bank_q, shadow_staging_bank_q;
  logic [7:0] shadow_staging_generation_q;
  integer reset_index;
  wire cfg_accept_c = cfg_valid_i && cfg_ready_o;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      shadow_valid0_q <= '0;
      shadow_valid1_q <= '0;
      shadow_active_bank_q <= 1'b0;
      shadow_staging_bank_q <= 1'b1;
      shadow_staging_generation_q <= 8'd0;
      for (reset_index = 0; reset_index < 256; reset_index = reset_index + 1) begin
        shadow_page0_q[reset_index] <= '0;
        shadow_page1_q[reset_index] <= '0;
      end
    end else begin
      if (cfg_accept_c && (cfg_op_i == 2'd0)) begin
        shadow_staging_bank_q <= ~shadow_active_bank_q;
        shadow_staging_generation_q <= cfg_page_generation_i;
        if (~shadow_active_bank_q) shadow_valid1_q <= '0;
        else                       shadow_valid0_q <= '0;
      end
      if (cfg_accept_c && (cfg_op_i == 2'd1)) begin
        if (shadow_staging_bank_q) begin
          shadow_page1_q[cfg_selector_i] <= cfg_row_i;
          shadow_valid1_q[cfg_selector_i] <= 1'b1;
        end else begin
          shadow_page0_q[cfg_selector_i] <= cfg_row_i;
          shadow_valid0_q[cfg_selector_i] <= 1'b1;
        end
      end
      if (cfg_accept_c && (cfg_op_i == 2'd3)) begin
        if (shadow_staging_bank_q) shadow_valid1_q <= '0;
        else                       shadow_valid0_q <= '0;
      end
      if (cfg_rsp_valid_o && cfg_rsp_ready_i && (cfg_rsp_op_o == 2'd2)) begin
        if (cfg_rsp_status_o == 4'd0)
          shadow_active_bank_q <= shadow_staging_bank_q;
        else if (cfg_rsp_status_o == 4'd5) begin
          if (shadow_staging_bank_q) shadow_valid1_q <= '0;
          else                       shadow_valid0_q <= '0;
        end
      end
    end
  end

  logic [31:0] shadow_canonical_crc_c, shadow_stale_crc_c;
  logic [31:0] canonical_work_c, stale_work_c;
  logic [79:0] canonical_row_c, stale_row_c;
  integer selector_index;
  // `byte_index` moved INSIDE the block, 2026-09-21. It was declared here at
  // module scope and assigned by a `for` nested inside an `if`, which is the
  // form Quartus 17.0 refuses outright -- the variable is left unassigned on
  // the else path, so it holds its value and the whole `always_comb` stops
  // inferring purely combinational logic. That shape killed the first full
  // console fit in `zhao_host_regwin.sv`.
  //
  // This file is NOT in any fit closure, so it was never going to fail a map.
  // Fixed anyway because the change is semantically null and a mutant that
  // cannot be synthesized is a control with a second, unrelated reason to
  // fail. THE MUTATIONS ARE UNTOUCHED: this moves a loop variable, nothing
  // else, and no driver polarity or compared value changes.
  always_comb begin : p_shadow_crc
    integer byte_index;
    canonical_work_c = crc32_byte(32'hFFFF_FFFF,
                                  shadow_staging_generation_q);
    stale_work_c = canonical_work_c;
    canonical_row_c = '0;
    stale_row_c = '0;
    for (selector_index = 0; selector_index < 256;
         selector_index = selector_index + 1) begin
      if (shadow_staging_bank_q) begin
        canonical_row_c = shadow_valid1_q[selector_index]
            ? {5'b0, shadow_page1_q[selector_index]} : 80'd0;
        stale_row_c = {5'b0, shadow_page1_q[selector_index]};
      end else begin
        canonical_row_c = shadow_valid0_q[selector_index]
            ? {5'b0, shadow_page0_q[selector_index]} : 80'd0;
        stale_row_c = {5'b0, shadow_page0_q[selector_index]};
      end
      for (byte_index = 0; byte_index < 10; byte_index = byte_index + 1) begin
        canonical_work_c = crc32_byte(
            canonical_work_c, canonical_row_c[byte_index*8 +: 8]);
        stale_work_c = crc32_byte(
            stale_work_c, stale_row_c[byte_index*8 +: 8]);
      end
    end
    shadow_canonical_crc_c = canonical_work_c ^ 32'hFFFF_FFFF;
    shadow_stale_crc_c = stale_work_c ^ 32'hFFFF_FFFF;
  end

  logic [74:0] latest_write_row_q;
  logic latest_write_seen_q;
  logic [1:0] captured_witness_class_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      latest_write_seen_q <= 1'b0;
      latest_write_row_q <= '0;
      captured_witness_class_q <= 2'd0;
    end else begin
      if (cfg_accept_c && (cfg_op_i == 2'd1)) begin
        latest_write_seen_q <= 1'b1;
        latest_write_row_q <= cfg_row_i;
      end
      if (req_valid_i && req_ready_o)
        captured_witness_class_q <= req_sample0_class_witness_i;
    end
  end

  logic [7:0] inner_req_page_generation_c;
  logic inner_data_quiet_c;
  logic [31:0] inner_cfg_crc_c;
  always_comb begin
    inner_req_page_generation_c = (MUTATION == M_LATE_PAGE)
        ? active_page_generation_o : req_page_generation_i;
    inner_data_quiet_c = (MUTATION == M_OWNER_QUIET)
        ? owner_quiet_i : data_quiet_i;
    inner_cfg_crc_c = cfg_crc32_i;
    if ((MUTATION == M_STALE_CRC) && (cfg_op_i == 2'd2))
      // Real computes C. Comparing it with E^C^S is equivalent to mutant S==E.
      inner_cfg_crc_c = cfg_crc32_i ^ shadow_canonical_crc_c ^
                        shadow_stale_crc_c;
  end

  logic real_issue_valid_w;
  logic [SLOTW+2+GENW-1:0] real_issue_handle_w;
  logic real_plan_valid_w;
  logic [2+SLOTW+2+GENW-1:0] real_plan_route_w;
  logic [31:0] real_plan_base_w, real_plan_mode_w;
  logic [1:0] real_plan_palette_slot_w;
  logic [7:0] real_plan_palette_generation_w;
  logic signed [31:0] real_plan_u_w, real_plan_v_w;
  logic [7:0] real_plan_lod_w;
  logic real_refuse_valid_w;
  logic [SLOTW+2+GENW-1:0] real_refuse_handle_w;
  logic [47:0] real_refuse_result_w;

  zhao_texture_binding_resolver_v2 #(
      .MAXLOG2(MAXLOG2), .SLOTW(SLOTW), .GENW(GENW)
  ) u_real (
      .clk(clk), .rst_n(rst_n),
      .frame_fault_clear_i(frame_fault_clear_i),
      .cfg_valid_i(cfg_valid_i), .cfg_ready_o(cfg_ready_o),
      .cfg_op_i(cfg_op_i),
      .cfg_page_generation_i(cfg_page_generation_i),
      .cfg_selector_i(cfg_selector_i), .cfg_row_i(cfg_row_i),
      .cfg_crc32_i(inner_cfg_crc_c),
      .cfg_rsp_valid_o(cfg_rsp_valid_o),
      .cfg_rsp_ready_i(cfg_rsp_ready_i), .cfg_rsp_op_o(cfg_rsp_op_o),
      .cfg_rsp_status_o(cfg_rsp_status_o),
      .cfg_rsp_page_generation_o(cfg_rsp_page_generation_o),
      .data_quiet_i(inner_data_quiet_c),
      .admission_enable_o(admission_enable_o),
      .active_page_generation_o(active_page_generation_o),
      .cfg_loader_idle_o(cfg_loader_idle_o),
      .binding_crc_busy_o(binding_crc_busy_o),
      .binding_seal_pending_o(binding_seal_pending_o),
      .req_valid_i(req_valid_i), .req_ready_o(req_ready_o),
      .req_sample_handle_i(req_sample_handle_i),
      .req_page_generation_i(inner_req_page_generation_c),
      .req_selector_overflow_i(req_selector_overflow_i),
      .req_force_refuse_i(req_force_refuse_i),
      .req_binding_selector_i(req_binding_selector_i),
      .req_mosaic_tile_i(req_mosaic_tile_i),
      .req_u_i(req_u_i), .req_v_i(req_v_i),
      .req_lod_q4_4_i(req_lod_q4_4_i),
      .req_sample0_class_witness_i(req_sample0_class_witness_i),
      .req_sample0_palette_slot_witness_i(
          req_sample0_palette_slot_witness_i),
      .req_sample0_palette_generation_witness_i(
          req_sample0_palette_generation_witness_i),
      .iss_tmu_valid_o(real_issue_valid_w),
      .iss_tmu_handle_o(real_issue_handle_w),
      .plan_valid_o(real_plan_valid_w), .plan_ready_i(plan_ready_i),
      .plan_route_token_o(real_plan_route_w),
      .plan_base_o(real_plan_base_w), .plan_mode_o(real_plan_mode_w),
      .plan_palette_slot_o(real_plan_palette_slot_w),
      .plan_palette_generation_o(real_plan_palette_generation_w),
      .plan_u_o(real_plan_u_w), .plan_v_o(real_plan_v_w),
      .plan_lod_q4_4_o(real_plan_lod_w),
      .refuse_valid_o(real_refuse_valid_w),
      .refuse_ready_i(refuse_ready_i),
      .refuse_sample_handle_o(real_refuse_handle_w),
      .refuse_result_o(real_refuse_result_w),
      .sample_jobs_accepted_o(sample_jobs_accepted_o),
      .planner_jobs_accepted_o(planner_jobs_accepted_o),
      .local_refused_o(local_refused_o),
      .selector_overflow_count_o(selector_overflow_count_o),
      .page_generation_mismatch_o(page_generation_mismatch_o),
      .invalid_row_o(invalid_row_o),
      .witness_mismatch_o(witness_mismatch_o),
      .forced_refused_o(forced_refused_o),
      .tileset_samples_o(tileset_samples_o),
      .cfg_errors_o(cfg_errors_o), .binding_fault_o(binding_fault_o),
      .data_idle_o(data_idle_o));

  wire immediate_refusal_c = (MUTATION == M_SAME_CYCLE) &&
                             req_valid_i && req_ready_o;
  assign iss_tmu_valid_o = (MUTATION == M_WITHOUT_ISSUE)
                         ? 1'b0 : real_issue_valid_w;
  assign iss_tmu_handle_o = real_issue_handle_w;

  assign plan_valid_o = real_plan_valid_w;
  assign plan_route_token_o = (MUTATION == M_WITNESS_CLASS)
      ? {captured_witness_class_q, real_plan_route_w[15:0]}
      : real_plan_route_w;
  assign plan_base_o = ((MUTATION == M_ACTIVE_WRITE) && latest_write_seen_q)
      ? latest_write_row_q[31:0] : real_plan_base_w;
  assign plan_mode_o = ((MUTATION == M_ACTIVE_WRITE) && latest_write_seen_q)
      ? latest_write_row_q[63:32] : real_plan_mode_w;
  assign plan_palette_slot_o =
      ((MUTATION == M_ACTIVE_WRITE) && latest_write_seen_q)
      ? latest_write_row_q[65:64] : real_plan_palette_slot_w;
  assign plan_palette_generation_o =
      ((MUTATION == M_ACTIVE_WRITE) && latest_write_seen_q)
      ? latest_write_row_q[73:66] : real_plan_palette_generation_w;
  assign plan_u_o = real_plan_u_w;
  assign plan_v_o = real_plan_v_w;
  assign plan_lod_q4_4_o = real_plan_lod_w;

  assign refuse_valid_o = immediate_refusal_c || real_refuse_valid_w;
  assign refuse_sample_handle_o = immediate_refusal_c
      ? req_sample_handle_i : real_refuse_handle_w;
  assign refuse_result_o = immediate_refusal_c
      ? 48'h01_00_FF_FF00FF : real_refuse_result_w;

endmodule : zhao_texture_binding_resolver_v2_mutant_wrap

`define ZHAO_BIND_V2_MUTANT(NAME, ID)                                            \
module NAME #(parameter int unsigned MAXLOG2=11, SLOTW=6, GENW=8) (              \
    input logic clk, input logic rst_n, input logic frame_fault_clear_i,         \
    input logic cfg_valid_i, output logic cfg_ready_o,                           \
    input logic [1:0] cfg_op_i, input logic [7:0] cfg_page_generation_i,        \
    input logic [7:0] cfg_selector_i, input logic [74:0] cfg_row_i,              \
    input logic [31:0] cfg_crc32_i, output logic cfg_rsp_valid_o,                \
    input logic cfg_rsp_ready_i, output logic [1:0] cfg_rsp_op_o,                \
    output logic [3:0] cfg_rsp_status_o,                                         \
    output logic [7:0] cfg_rsp_page_generation_o,                               \
    input logic data_quiet_i, input logic owner_quiet_i,                        \
    output logic admission_enable_o,                                             \
    output logic [7:0] active_page_generation_o,                                \
    output logic cfg_loader_idle_o, output logic binding_crc_busy_o,             \
    output logic binding_seal_pending_o,                                         \
    input logic req_valid_i, output logic req_ready_o,                           \
    input logic [SLOTW+2+GENW-1:0] req_sample_handle_i,                         \
    input logic [7:0] req_page_generation_i,                                    \
    input logic req_selector_overflow_i, input logic req_force_refuse_i,         \
    input logic [7:0] req_binding_selector_i,                                    \
    input logic [7:0] req_mosaic_tile_i,                                         \
    input logic signed [31:0] req_u_i, input logic signed [31:0] req_v_i,       \
    input logic [7:0] req_lod_q4_4_i,                                            \
    input logic [1:0] req_sample0_class_witness_i,                              \
    input logic [1:0] req_sample0_palette_slot_witness_i,                       \
    input logic [7:0] req_sample0_palette_generation_witness_i,                 \
    output logic iss_tmu_valid_o,                                                \
    output logic [SLOTW+2+GENW-1:0] iss_tmu_handle_o,                           \
    output logic plan_valid_o, input logic plan_ready_i,                         \
    output logic [2+SLOTW+2+GENW-1:0] plan_route_token_o,                       \
    output logic [31:0] plan_base_o, output logic [31:0] plan_mode_o,           \
    output logic [1:0] plan_palette_slot_o,                                      \
    output logic [7:0] plan_palette_generation_o,                               \
    output logic signed [31:0] plan_u_o, output logic signed [31:0] plan_v_o,   \
    output logic [7:0] plan_lod_q4_4_o,                                         \
    output logic refuse_valid_o, input logic refuse_ready_i,                     \
    output logic [SLOTW+2+GENW-1:0] refuse_sample_handle_o,                     \
    output logic [zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0] refuse_result_o,\
    output logic [31:0] sample_jobs_accepted_o,                                 \
    output logic [31:0] planner_jobs_accepted_o,                                \
    output logic [31:0] local_refused_o,                                        \
    output logic [31:0] selector_overflow_count_o,                              \
    output logic [31:0] page_generation_mismatch_o,                            \
    output logic [31:0] invalid_row_o, output logic [31:0] witness_mismatch_o,  \
    output logic [31:0] forced_refused_o, output logic [31:0] cfg_errors_o,     \
    output logic [31:0] tileset_samples_o,                                       \
    output logic binding_fault_o, output logic data_idle_o);                     \
  zhao_texture_binding_resolver_v2_mutant_wrap #(                                \
      .MUTATION(ID), .MAXLOG2(MAXLOG2), .SLOTW(SLOTW), .GENW(GENW))             \
      u_mutant (.*);                                                             \
endmodule : NAME

`ZHAO_BIND_V2_MUTANT(zhao_texture_binding_resolver_v2_late_page_generation_mutant, 1)
`ZHAO_BIND_V2_MUTANT(zhao_texture_binding_resolver_v2_early_activation_while_held_mutant, 2)
`ZHAO_BIND_V2_MUTANT(zhao_texture_binding_resolver_v2_active_bank_write_mutant, 3)
`ZHAO_BIND_V2_MUTANT(zhao_texture_binding_resolver_v2_stale_invalid_crc_mutant, 4)
`ZHAO_BIND_V2_MUTANT(zhao_texture_binding_resolver_v2_witness_class_route_mutant, 5)
`ZHAO_BIND_V2_MUTANT(zhao_texture_binding_resolver_v2_same_cycle_refusal_mutant, 6)
`ZHAO_BIND_V2_MUTANT(zhao_texture_binding_resolver_v2_refusal_without_issue_mutant, 7)

`undef ZHAO_BIND_V2_MUTANT
/* verilator lint_on DECLFILENAME */
`default_nettype wire
