// tb_geom_mem_adapter.sv - guard request/response structs flattened for the C++
// side, exactly as tb_assetfetch.sv does it. The struct fields are driven and
// observed individually so the test can assert on the CLIENT the adapter
// substitutes, which is one of the properties under test.
//
// NOTE the wrapping: a comment line whose first word is "verilator" is read as
// a PRAGMA, so this header cannot start a line with the tool's name. The first
// draft did, and the error names a pragma nobody wrote.
module tb_geom_mem_adapter
  import zhao_pkg::*;
(
    input  logic        clk,
    input  logic        rst_n,

    // ---- requester A (descriptors, 32 bytes) ------------------------------
    input  logic        a_valid,
    input  logic [26:0] a_addr,
    input  logic [6:0]  a_len,
    input  logic [2:0]  a_client,
    input  logic        a_write,
    output logic        a_ready,
    output logic        a_ok,
    output logic        a_violation,
    output logic        a_beat_valid,
    output logic [63:0] a_beat_data,
    output logic        a_beat_last,

    // ---- requester B (payload lines, 64 bytes) ----------------------------
    input  logic        b_valid,
    input  logic [26:0] b_addr,
    input  logic [6:0]  b_len,
    input  logic [2:0]  b_client,
    input  logic        b_write,
    output logic        b_ready,
    output logic        b_ok,
    output logic        b_violation,
    output logic        b_beat_valid,
    output logic [63:0] b_beat_data,
    output logic        b_beat_last,

    // ---- the single downstream client -------------------------------------
    output logic        m_valid,
    output logic [26:0] m_addr,
    output logic [6:0]  m_len,
    output logic [2:0]  m_client,
    output logic        m_write,
    input  logic        m_ready,
    input  logic        m_ok,
    input  logic        m_violation,
    input  logic        m_beat_valid,
    input  logic [63:0] m_beat_data,
    input  logic        m_beat_last,

    // ---- evidence ---------------------------------------------------------
    output logic [31:0] jobs_a,
    output logic [31:0] jobs_b,
    output logic [31:0] denied,
    output logic [31:0] contention,
    output logic [31:0] err_short,
    output logic [31:0] err_long,
    output logic [31:0] err_unowned
);

  zhao_guard_req_t areq, breq, mreq;
  zhao_guard_rsp_t arsp, brsp, mrsp;

  always_comb begin
    areq        = '0;
    areq.valid  = a_valid;
    areq.write  = a_write;   // a requester CAN ask to write; see the DUT
    areq.client = zhao_client_e'(a_client);
    areq.addr   = a_addr;
    areq.len    = a_len;
    areq.be     = {64{1'b1}};

    breq        = '0;
    breq.valid  = b_valid;
    breq.write  = b_write;   // ...and the adapter must still issue a READ
    breq.client = zhao_client_e'(b_client);
    breq.addr   = b_addr;
    breq.len    = b_len;
    breq.be     = {64{1'b1}};

    mrsp           = '0;
    mrsp.ready     = m_ready;
    mrsp.ok        = m_ok;
    mrsp.violation = m_violation;
  end

  assign a_ready     = arsp.ready;
  assign a_ok        = arsp.ok;
  assign a_violation = arsp.violation;
  assign b_ready     = brsp.ready;
  assign b_ok        = brsp.ok;
  assign b_violation = brsp.violation;

  assign m_valid  = mreq.valid;
  assign m_addr   = mreq.addr;
  assign m_len    = mreq.len;
  assign m_client = mreq.client;
  assign m_write  = mreq.write;

  /* verilator lint_off PINCONNECTEMPTY */
  zhao_geom_mem_adapter u_dut (
      .clk(clk), .rst_n(rst_n),
      .a_req_i(areq), .a_rsp_o(arsp),
      .a_beat_valid_o(a_beat_valid), .a_beat_data_o(a_beat_data),
      .a_beat_last_o(a_beat_last),
      .b_req_i(breq), .b_rsp_o(brsp),
      .b_beat_valid_o(b_beat_valid), .b_beat_data_o(b_beat_data),
      .b_beat_last_o(b_beat_last),
      // Requester C (MATERIAL.RESOLVE, 2026-09-19) held IDLE: this bench is the
      // evidence that A and B behave exactly as before the widening.
      .c_req_i('0), .c_rsp_o(), .c_beat_valid_o(), .c_beat_data_o(),
      .c_beat_last_o(),
      // Requester D (GEOM.DRAWJOB, 2026-09-20, R29) held IDLE for the same
      // reason: this bench is the evidence that A and B did not move.
      .d_req_i('0), .d_rsp_o(), .d_beat_valid_o(), .d_beat_data_o(),
      .d_beat_last_o(),
      // Requester E (PART.TABLE's species-page loader, 2026-09-19, R42) held
      // IDLE for the same reason again: this bench is the evidence that A and
      // B behave exactly as they did before each widening.
      .e_req_i('0), .e_rsp_o(), .e_beat_valid_o(), .e_beat_data_o(),
      .e_beat_last_o(),
      // Requester F, FORGE.PRIM's program-page bank, added 2026-09-21
      // (FORGECOMP). HELD IDLE here for the same reason C, D and E are: this
      // bench's properties are A's and B's ownership ones, and the share's
      // N-1 round-robin bound at SIX is proved by `mem_share_n`'s own directed
      // test rather than re-derived here. The port must still be NAMED -- an
      // unconnected input is a PINMISSING the next configure discovers, which
      // is how this bench found the widening in the first place.
      .f_req_i('0), .f_rsp_o(), .f_beat_valid_o(), .f_beat_data_o(),
      .f_beat_last_o(),
      // Requester G, GEOM.POSE's kind-8/kind-9 page reader, added 2026-09-22
      // (GEOMCLOSE). HELD IDLE for the reason C..F are, and NAMED for the
      // reason F is: an unconnected input is a PINMISSING the next configure
      // discovers. G has no `beat_last` port at all -- the reader counts its
      // own eight beats a line -- so there is nothing to leave open here.
      .g_req_i('0), .g_rsp_o(), .g_beat_valid_o(), .g_beat_data_o(),
      // Requester H, GEOM.LADDERBANK's kind-8 CREATURE_FORM page reader, added
      // 2026-09-23 (SHADOWRIDE). HELD IDLE and NAMED for the reasons C..G are.
      // H has no `beat_last` port either, for G's reason.
      .h_req_i('0), .h_rsp_o(), .h_beat_valid_o(), .h_beat_data_o(),
      // Requester I, TEXTURE.PALETTELOAD's CLUT palette reader, added
      // 2026-09-26 (I13CLOSE). HELD IDLE and NAMED for the reasons C..H are.
      // I has no `beat_last` port either, for G's reason.
      .i_req_i('0), .i_rsp_o(), .i_beat_valid_o(), .i_beat_data_o(),
      .m_req_o(mreq), .m_rsp_i(mrsp),
      .m_beat_valid_i(m_beat_valid), .m_beat_data_i(m_beat_data),
      .m_beat_last_i(m_beat_last),
      .jobs_a_o(jobs_a), .jobs_b_o(jobs_b), .jobs_c_o(), .jobs_d_o(), .jobs_e_o(), .jobs_f_o(), .jobs_g_o(), .jobs_h_o(), .jobs_i_o(),
      .denied_o(denied),
      .contention_o(contention),
      .err_short_o(err_short), .err_long_o(err_long), .err_unowned_o(err_unowned)
  );
  /* verilator lint_on PINCONNECTEMPTY */

endmodule
