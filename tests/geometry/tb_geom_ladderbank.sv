// tb_geom_ladderbank.sv -- the REAL `zhao_geom_ladderbank` with its two
// packed-struct ports flattened, so the C++ driver names fields instead of bit
// ranges. Nothing here decides anything.
//
// The flattening is the whole file, and it exists for the reason the repo's
// other struct-port benches exist: a driver that pokes `g_req_o[97:71]` is a
// driver that will still compile after somebody inserts a field.
`default_nettype none

module tb_geom_ladderbank
  import zhao_pkg::*;
#(
    parameter int unsigned ROWS = 16,
    parameter logic [7:0]  PAGE_KIND = 8'd8
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

    input  var logic        q_valid_i,
    input  var logic [23:0] q_form_i,
    output var logic        a_valid_o,
    output var logic        a_hit_o,
    output var logic signed [31:0] a_bound_o,
    output var logic signed [31:0] a_micro_o,
    output var logic signed [31:0] a_splat_o,
    output var logic signed [31:0] a_glint_o,

    output var logic [31:0] pages_o,
    output var logic [31:0] records_o,
    output var logic [31:0] pages_dropped_o,
    output var logic [31:0] bad_magic_o,
    output var logic [31:0] truncated_o,
    output var logic [31:0] bad_record_o,
    output var logic [31:0] overflow_o,
    output var logic [31:0] denied_o,
    output var logic [31:0] lookup_miss_o,
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

  zhao_geom_ladderbank #(
      .ROWS     (ROWS),
      .PAGE_KIND(PAGE_KIND)
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

      .q_valid_i(q_valid_i),
      .q_form_i (q_form_i),
      .a_valid_o(a_valid_o),
      .a_hit_o  (a_hit_o),
      .a_bound_o(a_bound_o),
      .a_micro_o(a_micro_o),
      .a_splat_o(a_splat_o),
      .a_glint_o(a_glint_o),

      .pages_o        (pages_o),
      .records_o      (records_o),
      .pages_dropped_o(pages_dropped_o),
      .bad_magic_o    (bad_magic_o),
      .truncated_o    (truncated_o),
      .bad_record_o   (bad_record_o),
      .overflow_o     (overflow_o),
      .denied_o       (denied_o),
      .lookup_miss_o  (lookup_miss_o),
      .busy_o         (busy_o)
  );

endmodule : tb_geom_ladderbank

`default_nettype wire
