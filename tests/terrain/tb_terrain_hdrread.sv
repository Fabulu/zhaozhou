// tb_terrain_hdrread.sv -- flattening wrapper for TERRAIN.HDRREAD.
//
// It exists for one reason and it is the same reason `tb_geom_mem_adapter.sv`
// exists: `zhao_guard_req_t` and `zhao_guard_rsp_t` are packed structs, and a
// Verilated C++ driver would have to index them by bit position -- a layout
// the driver would then be silently wrong about the moment a field width
// changed in `zhao_pkg`. Flattening here means the field names are checked by
// the elaborator instead.
//
// NO BEHAVIOUR LIVES HERE. Every signal is a rename. The MEM.GUARD model and
// the beat generator are in the C++ driver, where they can be made to deny, to
// stall and to end a burst early.
`default_nettype none

module tb_terrain_hdrread
  import zhao_pkg::*;
(
    input  var logic        clk,
    input  var logic        rst_n,

    input  var logic [31:0] cfg_epoch,

    // ---- the compose door -------------------------------------------------
    input  var logic        j_valid,
    output var logic        j_ready,
    input  var logic [10:0] j_slot,
    input  var logic [ 7:0] j_gen,
    input  var logic [31:0] j_epoch,
    input  var logic [31:0] j_src_id,
    input  var logic [15:0] j_flags,
    input  var logic [ 7:0] j_view_mask,
    input  var logic [31:0] j_island,
    input  var logic [15:0] j_ix,
    input  var logic [15:0] j_iz,

    // ---- the guard client, flattened --------------------------------------
    output var logic        g_valid,
    output var logic        g_write,
    output var logic [ 2:0] g_client,
    output var logic [26:0] g_addr,
    output var logic [ 6:0] g_len,
    input  var logic        g_ready,
    input  var logic        g_ok,
    input  var logic        g_violation,
    input  var logic        beat_valid,
    input  var logic [63:0] beat_data,
    input  var logic        beat_last,

    // ---- the header record ------------------------------------------------
    output var logic        h_valid,
    input  var logic        h_ready,
    output var logic [ 7:0] h_pitch_log2,
    output var logic [15:0] h_patch_ix,
    output var logic [15:0] h_patch_iz,
    output var logic [31:0] h_env_x0,
    output var logic [31:0] h_env_z0,
    output var logic [15:0] h_src_id,
    output var logic        h_ok,
    output var logic [ 3:0] h_verdict,

    // ---- the forwarded job ------------------------------------------------
    output var logic        f_valid,
    input  var logic        f_ready,
    output var logic [10:0] f_slot,
    output var logic [ 7:0] f_gen,
    output var logic [31:0] f_epoch,
    output var logic [31:0] f_src_id,
    output var logic [15:0] f_flags,
    output var logic [ 7:0] f_view_mask,

    // ---- counters ---------------------------------------------------------
    output var logic [31:0] headers_read,
    output var logic [31:0] headers_refused,
    output var logic [31:0] guard_denied,
    output var logic [31:0] incomplete,
    output var logic [31:0] ident_fails,
    output var logic        idle
);

  // The BYTE ENABLES are driven by the DUT and read by nobody here: the C++
  // guard model serves whole 64-byte lines, which is the only shape the real
  // guard admits for this request. Waived at the declaration rather than
  // exported to a port the driver would ignore.
  /* verilator lint_off UNUSEDSIGNAL */
  zhao_guard_req_t greq;
  /* verilator lint_on UNUSEDSIGNAL */
  zhao_guard_rsp_t grsp;

  always_comb begin
    grsp           = '0;
    grsp.ready     = g_ready;
    grsp.ok        = g_ok;
    grsp.violation = g_violation;
  end

  assign g_valid  = greq.valid;
  assign g_write  = greq.write;
  assign g_client = greq.client;
  assign g_addr   = greq.addr;
  assign g_len    = greq.len;

  // THE DUT IS SELECTED BY A PLAIN `ifdef, and that is deliberate. CLAUDE.md
  // records that Verilator's -D cannot override a FUNCTION-LIKE `define and
  // says nothing when it fails to, so two combiner mutants once measured
  // unmutated production and passed. A plain `ifdef selecting between two
  // module names is a form -D does reach, and the negative control is that the
  // control driver FAILS when built without the define -- which is asserted in
  // tests/terrain/terrain_hdrread_mutant_control.cpp's own header.
`ifdef ZHAO_HDRREAD_LIVE_FORWARD_MUTANT
  zhao_terrain_hdrread_live_forward_mutant u_dut (
`else
  zhao_terrain_hdrread u_dut (
`endif
    .clk  (clk),
    .rst_n(rst_n),

    .cfg_vram_client_i(ZHAO_CLIENT_TERRAIN_BUILD),
    .cfg_epoch_i      (cfg_epoch),

    .j_valid_i (j_valid),
    .j_ready_o (j_ready),
    .j_slot_i  (j_slot),
    .j_gen_i   (j_gen),
    .j_epoch_i (j_epoch),
    .j_src_id_i(j_src_id),
    .j_flags_i (j_flags),
    .j_view_mask_i(j_view_mask),
    .j_island_i(j_island),
    .j_ix_i    (signed'(j_ix)),
    .j_iz_i    (signed'(j_iz)),

    .guard_req_o (greq),
    .guard_rsp_i (grsp),
    .beat_valid_i(beat_valid),
    .beat_data_i (beat_data),
    .beat_last_i (beat_last),

    .h_valid_o     (h_valid),
    .h_ready_i     (h_ready),
    .h_pitch_log2_o(h_pitch_log2),
    .h_patch_ix_o  (h_patch_ix),
    .h_patch_iz_o  (h_patch_iz),
    .h_env_x0_o    (h_env_x0),
    .h_env_z0_o    (h_env_z0),
    .h_src_id_o    (h_src_id),
    .h_ok_o        (h_ok),
    .h_verdict_o   (h_verdict),

    .f_valid_o (f_valid),
    .f_ready_i (f_ready),
    .f_slot_o  (f_slot),
    .f_gen_o   (f_gen),
    .f_epoch_o (f_epoch),
    .f_src_id_o(f_src_id),
    .f_flags_o (f_flags),
    .f_view_mask_o(f_view_mask),

    .headers_read_o   (headers_read),
    .headers_refused_o(headers_refused),
    .guard_denied_o   (guard_denied),
    .incomplete_o     (incomplete),
    .ident_fails_o    (ident_fails),
    .idle_o           (idle)
  );

endmodule

`default_nettype wire
