// tb_mem_share_wr.sv -- zhao_mem_share_wr at N=3, ports flattened per requester
// so tests/memory/mem_share_wr_directed.cpp names fields instead of indexing
// packed struct bits. RQ is a parameter so the ledger-full path can be reached
// at a small depth. Testbench only.
module tb_mem_share_wr
  import zhao_pkg::*;
#(
    parameter int unsigned RQ = 4
) (
    input  logic clk,
    input  logic rst_n,

    input  logic [2:0]        r_valid,
    input  logic [2:0]        r_write,
    input  logic [2:0][26:0]  r_addr,
    input  logic [2:0][6:0]   r_len,
    output logic [2:0]        r_ready,
    output logic [2:0]        r_ok,
    output logic [2:0]        r_violation,
    output logic [2:0]        r_beat_valid,
    output logic [2:0]        r_beat_last,
    output logic [63:0]       r_beat_data,
    input  logic [2:0][63:0]  r_wdata,
    input  logic [2:0]        r_wvalid,
    input  logic [2:0]        r_wlast,
    output logic [2:0]        r_wready,
    output logic [2:0][7:0]   r_retire,

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
    output logic [63:0] m_wdata,
    output logic        m_wvalid,
    output logic        m_wlast,
    input  logic        m_wready,
    input  logic [7:0]  m_credits,

    output logic [2:0][31:0] jobs,
    output logic [31:0] denied,
    output logic [31:0] contention,
    output logic [31:0] err_short,
    output logic [31:0] err_long,
    output logic [31:0] err_unowned,
    output logic [31:0] retire_unowned,
    output logic [31:0] wbeat_unowned,
    output logic [31:0] ledger_full
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
      req[i].client = ZHAO_CLIENT_SCANOUT;     // untrusted: the share must replace it
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

  zhao_mem_share_wr #(.N(3), .CLIENT_ID(6), .RQ(RQ)) u_dut (
    .clk             (clk),
    .rst_n           (rst_n),
    .req_i           (req),
    .rsp_o           (rsp),
    .beat_valid_o    (r_beat_valid),
    .beat_data_o     (r_beat_data),
    .beat_last_o     (r_beat_last),
    .wdata_i         (r_wdata),
    .wvalid_i        (r_wvalid),
    .wlast_i         (r_wlast),
    .wready_o        (r_wready),
    .retire_o        (r_retire),
    .m_req_o         (m_req),
    .m_rsp_i         (m_rsp),
    .m_beat_valid_i  (m_beat_valid),
    .m_beat_data_i   (m_beat_data),
    .m_beat_last_i   (m_beat_last),
    .m_wdata_o       (m_wdata),
    .m_wvalid_o      (m_wvalid),
    .m_wlast_o       (m_wlast),
    .m_wready_i      (m_wready),
    .m_credits_i     (m_credits),
    .jobs_o          (jobs),
    .denied_o        (denied),
    .contention_o    (contention),
    .err_short_o     (err_short),
    .err_long_o      (err_long),
    .err_unowned_o   (err_unowned),
    .retire_unowned_o(retire_unowned),
    .wbeat_unowned_o (wbeat_unowned),
    .ledger_full_o   (ledger_full)
  );

endmodule
