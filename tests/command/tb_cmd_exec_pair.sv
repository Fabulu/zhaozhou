// tb_cmd_exec_pair.sv -- CMD.DECODER and CMD.EXEC on ONE forked byte stream.
//
// This is a WRAPPER, not a copy: it instantiates the two production modules and
// adds nothing but the fork. `tools/budget/mutant_copy_drift.py` excludes
// wrappers for exactly this reason -- there is no body here to go stale.
//
// WHY THE PAIR AND NOT THE EXECUTOR ALONE. `zhao_cmd_exec`'s verdict inputs
// could be driven from `zref::cmd::validate` in C++, and the test would pass
// while proving nothing about the two things most likely to be wrong:
//
//   1. THE FORK. `pkt_ready_o` is the AND of both consumers' readies. A tap
//      instead of a fork lets one consumer advance past a byte the other never
//      saw, and every counter on both sides still balances -- the byte simply
//      is not in one of the two walks. Driving the executor alone cannot see
//      this, because there is no second walk to disagree with.
//   2. THE VERDICT INSTANT. `decode_done_o` pulses the cycle after the decoder
//      takes the packet's last byte, and the executor takes that same byte in
//      that same cycle because the fork makes them move together. A C++-driven
//      verdict would be whatever the test author chose, which is how a
//      one-cycle staging race gets asserted rather than found.
//
// The fork below is the same expression `zhao_console_core.sv` uses. If that
// one changes, this must change with it -- the ONE place this harness can
// drift from the composition it stands for.
module tb_cmd_exec_pair #(
    parameter int unsigned STAMP_Q = 8,
    // Deliberately SMALL by default so `draw_overflow_o` is reachable with
    // legal stimulus -- five DrawForms in one packet -- rather than being a
    // counter asserted zero with an argument attached. CLAUDE.md: "a detector
    // that has not been shown to FIRE has not been tested".
    parameter int unsigned DRAW_Q  = 4
) (
    input  logic clk,
    input  logic rst_n,

    // the sealed packet byte stream, as CMD.DMA presents it
    input  logic        pkt_valid_i,
    output logic        pkt_ready_o,
    input  logic [ 7:0] pkt_byte_i,
    input  logic [31:0] pkt_len_i,

    // SURFACE.STAMP's backpressure, so the drain can be stalled on purpose
    input  logic        stamp_ready_i,
    // The draw dispatch's backpressure, same purpose. In the composition this
    // is the console boundary (entry I41), so it is genuinely an outside
    // opinion and is driven here rather than tied high.
    input  logic        draw_ready_i,
    // The matrix bank's refusal. In the composer this is `!proj_cfg_we_i` --
    // the host cfg port wins the cycle and CMD.EXEC re-presents. Driven here so
    // the re-presentation is exercised rather than assumed.
    input  logic        proj_cfg_ready_i,

    // ---- CMD.DECODER's verdict, observable -------------------------------
    output logic        decode_done_o,
    output logic [ 7:0] decode_error_o,
    output logic [31:0] decode_commands_o,

    // ---- CMD.EXEC's writes into the console ------------------------------
    output logic        proj_cfg_we_o,
    output logic        proj_cfg_view_o,
    output logic [ 4:0] proj_cfg_addr_o,
    output logic [31:0] proj_cfg_data_o,

    output logic               stamp_valid_o,
    output logic        [31:0] stamp_patch_o,
    output logic        [ 7:0] stamp_operation_o,
    output logic        [ 7:0] stamp_tag_o,
    output logic        [15:0] stamp_strength_o,
    output logic signed [31:0] stamp_tx_o,
    output logic signed [31:0] stamp_ty_o,
    output logic signed [31:0] stamp_radius_o,
    output logic signed [31:0] stamp_ring_width_o,
    output logic        [15:0] stamp_src_id_o,

    output logic        draw_valid_o,
    output logic [31:0] draw_form_o,
    output logic [31:0] draw_material_set_o,
    output logic [31:0] draw_transform_o,
    output logic [ 7:0] draw_viewport_mask_o,
    output logic [ 7:0] draw_semantic_weight_o,
    output logic [15:0] draw_flags_o,
    output logic [15:0] draw_src_id_o,

    // ---- CMD.EXEC's evidence ---------------------------------------------
    output logic [31:0] packets_committed_o,
    output logic [31:0] packets_abandoned_o,
    output logic [31:0] views_written_o,
    output logic [31:0] stamps_issued_o,
    output logic [31:0] stamp_overflow_o,
    output logic [31:0] view_range_refused_o,
    output logic [31:0] stamp_src_truncated_o,
    output logic [31:0] draws_issued_o,
    output logic [31:0] draw_overflow_o,
    output logic [31:0] draw_src_truncated_o,
    output logic [31:0] unsupported_o
);

  logic dec_ready, exe_ready;

  // THE FORK. Both consumers must be able to take the byte, or nobody does.
  assign pkt_ready_o = dec_ready && exe_ready;

  // The decoder's record-header port is retired unconditionally here for the
  // reason section 7b of the core gives: retiring is not acting, and holding
  // `rec_ready` low would stall the shared stream and deadlock the executor
  // behind it.
  logic        rec_valid_unused;
  logic [15:0] rec_opcode_unused, rec_bytes_unused;
  logic [31:0] rec_src_unused, rec_index_unused, bytes_consumed_unused;

  zhao_cmd_decoder u_dec (
      .clk  (clk),
      .rst_n(rst_n),

      .pkt_valid_i(pkt_valid_i),
      .pkt_ready_o(dec_ready),
      .pkt_byte_i (pkt_byte_i),
      .pkt_len_i  (pkt_len_i),

      .rec_valid_o    (rec_valid_unused),
      .rec_ready_i    (1'b1),
      .rec_opcode_o   (rec_opcode_unused),
      .rec_bytes_o    (rec_bytes_unused),
      .rec_source_id_o(rec_src_unused),
      .rec_index_o    (rec_index_unused),

      .decode_done_o   (decode_done_o),
      .decode_error_o  (decode_error_o),
      .bytes_consumed_o(bytes_consumed_unused),
      .commands_o      (decode_commands_o)
  );

  zhao_cmd_exec #(
      .STAMP_Q(STAMP_Q),
      .DRAW_Q (DRAW_Q)
  ) u_exec (
      .clk  (clk),
      .rst_n(rst_n),

      .pkt_valid_i     (pkt_valid_i),
      .pkt_ready_o     (exe_ready),
      .pkt_fork_ready_i(pkt_ready_o),  // the AND, handed back
      .pkt_byte_i      (pkt_byte_i),
      .pkt_len_i       (pkt_len_i),

      .verdict_valid_i(decode_done_o),
      .verdict_error_i(decode_error_o),

      .proj_cfg_we_o   (proj_cfg_we_o),
      .proj_cfg_ready_i(proj_cfg_ready_i),
      .proj_cfg_view_o(proj_cfg_view_o),
      .proj_cfg_addr_o(proj_cfg_addr_o),
      .proj_cfg_data_o(proj_cfg_data_o),

      .stamp_valid_o     (stamp_valid_o),
      .stamp_ready_i     (stamp_ready_i),
      .stamp_patch_o     (stamp_patch_o),
      .stamp_operation_o (stamp_operation_o),
      .stamp_tag_o       (stamp_tag_o),
      .stamp_strength_o  (stamp_strength_o),
      .stamp_tx_o        (stamp_tx_o),
      .stamp_ty_o        (stamp_ty_o),
      .stamp_radius_o    (stamp_radius_o),
      .stamp_ring_width_o(stamp_ring_width_o),
      .stamp_src_id_o    (stamp_src_id_o),

      .draw_valid_o          (draw_valid_o),
      .draw_ready_i          (draw_ready_i),
      .draw_form_o           (draw_form_o),
      .draw_material_set_o   (draw_material_set_o),
      .draw_transform_o      (draw_transform_o),
      .draw_viewport_mask_o  (draw_viewport_mask_o),
      .draw_semantic_weight_o(draw_semantic_weight_o),
      .draw_flags_o          (draw_flags_o),
      .draw_src_id_o         (draw_src_id_o),

      .packets_committed_o  (packets_committed_o),
      .packets_abandoned_o  (packets_abandoned_o),
      .views_written_o      (views_written_o),
      .stamps_issued_o      (stamps_issued_o),
      .stamp_overflow_o     (stamp_overflow_o),
      .view_range_refused_o (view_range_refused_o),
      .stamp_src_truncated_o(stamp_src_truncated_o),
      .draws_issued_o       (draws_issued_o),
      .draw_overflow_o      (draw_overflow_o),
      .draw_src_truncated_o (draw_src_truncated_o),
      .unsupported_o        (unsupported_o)
  );

endmodule : tb_cmd_exec_pair
