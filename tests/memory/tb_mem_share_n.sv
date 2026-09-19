// tb_mem_share_n.sv -- zhao_mem_share_n at N=3, ports flattened per requester
// so tests/memory/mem_share_n_directed.cpp names fields instead of indexing
// packed struct bits it could be silently wrong about. Testbench only.
module tb_mem_share_n
  import zhao_pkg::*;
(
    input  logic clk,
    input  logic rst_n,

    input  logic [2:0]        r_valid,
    input  logic [2:0]        r_write,
    input  logic [2:0][2:0]   r_client,
    input  logic [2:0][26:0]  r_addr,
    input  logic [2:0][6:0]   r_len,
    output logic [2:0]        r_ready,
    output logic [2:0]        r_ok,
    output logic [2:0]        r_violation,
    output logic [2:0]        r_beat_valid,
    output logic [2:0]        r_beat_last,
    output logic [63:0]       r_beat_data,

    output logic        m_valid,
    output logic        m_write,
    output logic [2:0]  m_client,
    output logic [26:0] m_addr,
    output logic [6:0]  m_len,
    input  logic        m_ready,
    input  logic        m_ok,
    input  logic        m_violation,
    input  logic        m_beat_valid,
    input  logic [63:0] m_beat_data,
    input  logic        m_beat_last,

    output logic [2:0][31:0] jobs,
    output logic [31:0] denied,
    output logic [31:0] contention,
    output logic [31:0] err_short,
    output logic [31:0] err_long,
    output logic [31:0] err_unowned
);

  zhao_guard_req_t [2:0] req;
  zhao_guard_rsp_t [2:0] rsp;
  zhao_guard_req_t m_req;
  zhao_guard_rsp_t m_rsp;

  always_comb begin
    for (int i = 0; i < 3; i++) begin
      req[i]        = '0;
      req[i].valid  = r_valid[i];
      req[i].write  = r_write[i];
      req[i].client = zhao_client_e'(r_client[i]);
      req[i].addr   = r_addr[i];
      req[i].len    = r_len[i];
      req[i].be     = '1;
      r_ready[i]     = rsp[i].ready;
      r_ok[i]        = rsp[i].ok;
      r_violation[i] = rsp[i].violation;
    end
    m_rsp           = '0;
    m_rsp.ready     = m_ready;
    m_rsp.ok        = m_ok;
    m_rsp.violation = m_violation;
  end

  assign m_valid  = m_req.valid;
  assign m_write  = m_req.write;
  assign m_client = 3'(m_req.client);
  assign m_addr   = m_req.addr;
  assign m_len    = m_req.len;

  zhao_mem_share_n #(.N(3), .CLIENT_ID(3), .FORCE_READ(1'b1)) u_dut (
    .clk           (clk),
    .rst_n         (rst_n),
    .req_i         (req),
    .rsp_o         (rsp),
    .beat_valid_o  (r_beat_valid),
    .beat_data_o   (r_beat_data),
    .beat_last_o   (r_beat_last),
    .m_req_o       (m_req),
    .m_rsp_i       (m_rsp),
    .m_beat_valid_i(m_beat_valid),
    .m_beat_data_i (m_beat_data),
    .m_beat_last_i (m_beat_last),
    .jobs_o        (jobs),
    .denied_o      (denied),
    .contention_o  (contention),
    .err_short_o   (err_short),
    .err_long_o    (err_long),
    .err_unowned_o (err_unowned)
  );

endmodule
