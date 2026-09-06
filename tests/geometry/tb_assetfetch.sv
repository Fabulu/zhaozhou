// tb_assetfetch.sv — TESTBENCH WRAPPER: zhao_geom_assetfetch with its
// zhao_guard_req_t / zhao_guard_rsp_t ports flattened for the Verilator C++
// harness. Testbench tree — never linted.
//
// The wrapper exists so the harness never has to guess how Verilator packs a
// struct port. Guessing works until somebody adds a field, and then it works
// wrongly and silently, which is worse than not working.
module tb_assetfetch
  import zhao_pkg::*;
(
    input logic clk,
    input logic rst_n,

    // ---- meshlet in ---------------------------------------------------------
    input  logic        m_valid,
    output logic        m_ready,
    input  logic [31:0] m_vertex_offset,
    input  logic [31:0] m_index_offset,
    input  logic [7:0]  m_vertex_count,
    input  logic [7:0]  m_triangle_count,
    input  logic [15:0] m_src_id,
    input  logic [2:0]  m_client,

    // ---- the guard port, flat ----------------------------------------------
    output logic        g_valid,
    output logic        g_write,
    output logic [2:0]  g_client,
    output logic [26:0] g_addr,
    output logic [6:0]  g_len,
    output logic [63:0] g_be,
    input  logic        g_ready,
    input  logic        g_ok,
    input  logic        g_violation,

    input  logic        beat_valid,
    input  logic [63:0] beat_data,
    input  logic        beat_last,

    // ---- servable handshake -------------------------------------------------
    output logic        s_valid,
    input  logic        s_ready,
    output logic [7:0]  s_vertex_count,
    output logic [7:0]  s_triangle_count,
    output logic [15:0] s_src_id,
    input  logic        release_pulse,

    // ---- index service ------------------------------------------------------
    input  logic        ix_req,
    input  logic [8:0]  ix_index,
    output logic        ix_valid,
    output logic [7:0]  ix_a,
    output logic [7:0]  ix_b,
    output logic [7:0]  ix_c,

    // ---- vertex stream ------------------------------------------------------
    output logic         v_valid,
    input  logic         v_ready,
    output logic [255:0] v_bytes,
    output logic [15:0]  v_src_id,

    // ---- counters -----------------------------------------------------------
    output logic [31:0] meshlets_fetched,
    output logic [31:0] beats_read,
    output logic [31:0] guard_denied,
    output logic [31:0] refused_footprint,
    output logic [31:0] prefetch_stall,
    // Beat-protocol faults (owner brief 11.4). Surfaced so the leaf test can
    // drive a short and a long line and see the block REFUSE them, rather than
    // trusting beat_last and building a record out of stale RAM.
    output logic [31:0] err_beat_truncated,
    output logic [31:0] err_beat_overrun,
    output logic [31:0] err_beat_unowned
);

  zhao_guard_req_t req;
  zhao_guard_rsp_t rsp;

  assign g_valid  = req.valid;
  assign g_write  = req.write;
  assign g_client = req.client;
  assign g_addr   = req.addr;
  assign g_len    = req.len;
  assign g_be     = req.be;

  assign rsp.ready     = g_ready;
  assign rsp.ok        = g_ok;
  assign rsp.violation = g_violation;

  zhao_geom_assetfetch u_dut (
      .clk   (clk),
      .rst_n (rst_n),

      .m_valid_i          (m_valid),
      .m_ready_o          (m_ready),
      .m_vertex_offset_i  (m_vertex_offset),
      .m_index_offset_i   (m_index_offset),
      .m_vertex_count_i   (m_vertex_count),
      .m_triangle_count_i (m_triangle_count),
      .m_src_id_i         (m_src_id),
      .m_client_i         (zhao_client_e'(m_client)),

      .guard_req_o  (req),
      .guard_rsp_i  (rsp),
      .beat_valid_i (beat_valid),
      .beat_data_i  (beat_data),
      .beat_last_i  (beat_last),

      .s_valid_o          (s_valid),
      .s_ready_i          (s_ready),
      .s_vertex_count_o   (s_vertex_count),
      .s_triangle_count_o (s_triangle_count),
      .s_src_id_o         (s_src_id),
      .release_i          (release_pulse),

      .ix_req_i   (ix_req),
      .ix_index_i (ix_index),
      .ix_valid_o (ix_valid),
      .ix_a_o     (ix_a),
      .ix_b_o     (ix_b),
      .ix_c_o     (ix_c),

      .v_valid_o  (v_valid),
      .v_ready_i  (v_ready),
      .v_bytes_o  (v_bytes),
      .v_src_id_o (v_src_id),

      .meshlets_fetched_o  (meshlets_fetched),
      .beats_read_o        (beats_read),
      .guard_denied_o      (guard_denied),
      .refused_footprint_o (refused_footprint),
      .prefetch_stall_o    (prefetch_stall),
      .err_beat_truncated_o(err_beat_truncated),
      .err_beat_overrun_o  (err_beat_overrun),
      .err_beat_unowned_o  (err_beat_unowned)
  );

endmodule
