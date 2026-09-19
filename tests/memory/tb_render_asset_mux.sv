// tb_render_asset_mux.sv -- flattened Packet-E3 mux + real MEM.GUARD harness.
//
// The exact mux request feeds the exact zhao_mem_guard. The only test seam is an
// explicit OR-injection on the returning verdict bits, used to fire otherwise
// unreachable mux-side both/unsolicited/duplicate-verdict detectors. Ordinary
// traffic leaves both injection inputs low. Arbiter grant is independently
// controllable so guard acceptance, verdict, forwarding, and raw return remain
// four observable events.
`default_nettype none
module tb_render_asset_mux
  import zhao_pkg::*;
(
    input  logic        clk,
    input  logic        rst_n,
    input  logic        frame_fault_clear,

    input  logic        geom_valid,
    input  logic        geom_write,
    input  logic [2:0]  geom_client,
    input  logic [26:0] geom_addr,
    input  logic [6:0]  geom_len,
    input  logic [63:0] geom_be,
    output logic        geom_ready,
    output logic        geom_ok,
    output logic        geom_violation,
    output logic        geom_beat_valid,
    output logic [63:0] geom_beat_data,
    output logic        geom_beat_last,

    input  logic        texture_valid,
    input  logic [31:0] texture_addr,
    output logic        texture_ready,
    output logic        texture_data_valid,
    output logic [15:0] texture_data,
    output logic        texture_refused,

    input  logic        raw_valid,
    input  logic [15:0] raw_data,
    input  logic        raw_last,

    input  logic        arb_grant,
    input  logic        inject_guard_ok,
    input  logic        inject_guard_violation,

    // Exact mux->guard request observability.
    output logic        guard_valid,
    output logic        guard_write,
    output logic [2:0]  guard_client,
    output logic [26:0] guard_addr,
    output logic [6:0]  guard_len,
    output logic [63:0] guard_be,

    // Real guard response and the response delivered to the mux (after the
    // explicit test-only verdict injection seam).
    output logic        real_guard_ready,
    output logic        real_guard_ok,
    output logic        real_guard_violation,
    output logic        mux_guard_ready,
    output logic        mux_guard_ok,
    output logic        mux_guard_violation,

    output logic        arb_valid,
    output logic        arb_write,
    output logic [2:0]  arb_client,
    output logic [26:0] arb_addr,
    output logic [6:0]  arb_len,
    output logic        guard_violation_pulse,
    output logic [31:0] guard_violations,
    output logic        guard_violation_req_valid,
    output logic [2:0]  guard_violation_client,
    output logic        guard_violation_write,
    output logic [26:0] guard_violation_addr,
    output logic [6:0]  guard_violation_len,
    output logic [63:0] guard_violation_be,

    output logic        quiet,
    output logic        protocol_fault,
    output logic        structural_fault,
    output logic [31:0] guard_accepted,
    output logic [31:0] guard_ok_count,
    output logic [31:0] guard_denied,
    output logic [31:0] geometry_accepted,
    output logic [31:0] geometry_refused,
    output logic [31:0] texture_accepted,
    output logic [31:0] texture_refused_count,
    output logic [31:0] contention,
    output logic [31:0] raw_halfwords,
    output logic [31:0] protocol_faults
);

  zhao_guard_req_t geom_req;
  zhao_guard_rsp_t geom_rsp;
  zhao_guard_req_t mux_guard_req;
  zhao_guard_rsp_t mux_guard_rsp;
  zhao_guard_rsp_t real_guard_rsp;
  zhao_arb_req_t arb_req;
  zhao_arb_rsp_t arb_rsp;
  zhao_guard_req_t violation_req;

  always_comb begin
    geom_req        = '0;
    geom_req.valid  = geom_valid;
    geom_req.write  = geom_write;
    geom_req.client = zhao_client_e'(geom_client);
    geom_req.addr   = geom_addr;
    geom_req.len    = geom_len;
    geom_req.be     = geom_be;

    mux_guard_rsp           = real_guard_rsp;
    mux_guard_rsp.ok        = real_guard_rsp.ok || inject_guard_ok;
    mux_guard_rsp.violation = real_guard_rsp.violation
                            || inject_guard_violation;

    arb_rsp         = '0;
    arb_rsp.grant   = arb_grant;
  end

  assign geom_ready       = geom_rsp.ready;
  assign geom_ok          = geom_rsp.ok;
  assign geom_violation   = geom_rsp.violation;

  assign guard_valid      = mux_guard_req.valid;
  assign guard_write      = mux_guard_req.write;
  assign guard_client     = mux_guard_req.client;
  assign guard_addr       = mux_guard_req.addr;
  assign guard_len        = mux_guard_req.len;
  assign guard_be         = mux_guard_req.be;

  assign real_guard_ready     = real_guard_rsp.ready;
  assign real_guard_ok        = real_guard_rsp.ok;
  assign real_guard_violation = real_guard_rsp.violation;
  assign mux_guard_ready      = mux_guard_rsp.ready;
  assign mux_guard_ok         = mux_guard_rsp.ok;
  assign mux_guard_violation  = mux_guard_rsp.violation;

  assign arb_valid  = arb_req.valid;
  assign arb_write  = arb_req.write;
  assign arb_client = arb_req.client;
  assign arb_addr   = arb_req.addr;
  assign arb_len    = arb_req.len;

  assign guard_violation_req_valid = violation_req.valid;
  assign guard_violation_client = violation_req.client;
  assign guard_violation_write  = violation_req.write;
  assign guard_violation_addr   = violation_req.addr;
  assign guard_violation_len    = violation_req.len;
  assign guard_violation_be     = violation_req.be;

  zhao_render_asset_mux u_mux (
      .clk,
      .rst_n,
      .frame_fault_clear_i(frame_fault_clear),
      .geom_req_i(geom_req),
      .geom_rsp_o(geom_rsp),
      .geom_beat_valid_o(geom_beat_valid),
      .geom_beat_data_o(geom_beat_data),
      .geom_beat_last_o(geom_beat_last),
      .texture_fill_valid_i(texture_valid),
      .texture_fill_ready_o(texture_ready),
      .texture_fill_addr_i(texture_addr),
      .texture_fill_data_valid_o(texture_data_valid),
      .texture_fill_data_o(texture_data),
      .texture_fill_refused_o(texture_refused),
      .guard_req_o(mux_guard_req),
      .guard_rsp_i(mux_guard_rsp),
      .engine1_raw16_valid_i(raw_valid),
      .engine1_raw16_data_i(raw_data),
      .engine1_raw16_last_i(raw_last),
      .quiet_o(quiet),
      .protocol_fault_o(protocol_fault),
      .structural_fault_o(structural_fault),
      .guard_accepted_o(guard_accepted),
      .guard_ok_o(guard_ok_count),
      .guard_denied_o(guard_denied),
      .geometry_accepted_o(geometry_accepted),
      .geometry_refused_o(geometry_refused),
      .texture_accepted_o(texture_accepted),
      .texture_refused_o(texture_refused_count),
      .contention_o(contention),
      .raw_halfwords_o(raw_halfwords),
      .protocol_faults_o(protocol_faults)
  );

  zhao_mem_guard u_guard (
      .clk,
      .rst_n,
      .req(mux_guard_req),
      .rsp(real_guard_rsp),
      .map_valid(1'b0),
      .blit_slot(1'b0),
      .blit_span(32'd0),
      .fb_writer(1'b0),
      .res_valid  (1'b0),   // TIE: this client is not MEM.UPLOAD; the R32 resource-write arm names TERRAIN_BUILD alone
      .res_base   (32'd0),  // TIE: this client is not MEM.UPLOAD; the R32 resource-write arm names TERRAIN_BUILD alone
      .res_span   (32'd0),  // TIE: this client is not MEM.UPLOAD; the R32 resource-write arm names TERRAIN_BUILD alone
      .arb_req,
      .arb_rsp,
      .guard_violation(guard_violation_pulse),
      .guard_violations,
      .guard_violation_req(violation_req)
  );

endmodule
`default_nettype wire
