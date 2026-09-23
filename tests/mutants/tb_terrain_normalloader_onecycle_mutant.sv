// tb_terrain_normalloader_onecycle_mutant.sv -- the struct-port flattening
// wrapper for the DELIBERATELY BROKEN copy. Identical to
// tests/terrain/tb_terrain_normalloader.sv except for the two names, so
// the mutant is driven by exactly the bench the real block is.
`default_nettype none

module tb_terrain_normalloader_onecycle_mutant
  import zhao_pkg::*;
#(
    parameter logic [7:0]  PAGE_KIND    = 8'd16,
    parameter int unsigned LAYOUT_WORDS = 5461
) (
    input  var logic clk,
    input  var logic rst_n,

    input  var logic        pub_valid_i,
    input  var logic [ 7:0] pub_tag_i,
    input  var logic [31:0] pub_base_i,
    input  var logic [31:0] pub_extent_i,

    // the asset window, field by field
    output var logic        req_valid_o,
    output var logic        req_write_o,
    output var logic [26:0] req_addr_o,
    output var logic [ 6:0] req_len_o,
    output var logic [63:0] req_be_o,
    output var logic [ 2:0] req_client_o,
    input  var logic        rsp_ready_i,
    input  var logic        rsp_ok_i,
    input  var logic        rsp_violation_i,
    input  var logic        beat_valid_i,
    input  var logic [63:0] beat_data_i,

    // TERRAIN.NORMALMAP's tile upload port
    output var logic        tw_we_o,
    output var logic [12:0] tw_addr_o,
    output var logic [15:0] tw_data_o,

    output var logic [31:0] pages_o,
    output var logic [31:0] words_o,
    output var logic [31:0] pages_dropped_o,
    output var logic [31:0] bad_magic_o,
    output var logic [31:0] oversize_o,
    output var logic [31:0] truncated_o,
    output var logic [31:0] denied_o,
    output var logic        busy_o
);

  zhao_guard_req_t req_c;
  zhao_guard_rsp_t rsp_c;

  assign req_valid_o  = req_c.valid;
  assign req_write_o  = req_c.write;
  assign req_addr_o   = req_c.addr;
  assign req_len_o    = req_c.len;
  assign req_be_o     = req_c.be;
  assign req_client_o = 3'(req_c.client);

  always_comb begin
    rsp_c           = '0;
    rsp_c.ready     = rsp_ready_i;
    rsp_c.ok        = rsp_ok_i;
    rsp_c.violation = rsp_violation_i;
  end

  zhao_terrain_normalloader_onecycle_mutant #(
      .PAGE_KIND   (PAGE_KIND),
      .LAYOUT_WORDS(LAYOUT_WORDS)
  ) u_dut (
      .clk  (clk),
      .rst_n(rst_n),

      .pub_valid_i (pub_valid_i),
      .pub_tag_i   (pub_tag_i),
      .pub_base_i  (pub_base_i),
      .pub_extent_i(pub_extent_i),

      .g_req_o       (req_c),
      .g_rsp_i       (rsp_c),
      .g_beat_valid_i(beat_valid_i),
      .g_beat_data_i (beat_data_i),

      .tw_we_o  (tw_we_o),
      .tw_addr_o(tw_addr_o),
      .tw_data_o(tw_data_o),

      .pages_o        (pages_o),
      .words_o        (words_o),
      .pages_dropped_o(pages_dropped_o),
      .bad_magic_o    (bad_magic_o),
      .oversize_o     (oversize_o),
      .truncated_o    (truncated_o),
      .denied_o       (denied_o),
      .busy_o         (busy_o)
  );

endmodule : tb_terrain_normalloader_onecycle_mutant

`default_nettype wire
