// tb_mem_upload.sv -- flat-port harness around MEM.UPLOAD.
//
// WHY A WRAPPER RATHER THAN DECODING THE STRUCTS IN C++. `zhao_hps_burst_req_t`,
// `zhao_guard_req_t` and their responses are PACKED STRUCTS, and Verilator
// presents anything over 64 bits as a word array. Reaching into those from the
// bench means hand-computing each field's bit offset -- and those offsets move
// the moment somebody adds a field to `zhao_pkg`, silently, with the bench still
// compiling and now testing the wrong bits. Flattening here means the compiler
// checks the field names instead, which is the whole difference between a
// harness that rots and one that fails loudly.
//
// This wrapper adds NO behaviour. Every port is a direct alias; there is no
// state, no arbitration and no stimulus in it, so nothing it does can make the
// DUT look better than it is.

`default_nettype none

module tb_mem_upload
  import zhao_pkg::*;
#(
    parameter int unsigned STAMP_UNUSED = 0   // no knobs; present for symmetry
) (
    input  var logic clk,
    input  var logic rst_n,

    input  var logic        req_valid_i,
    output var logic        req_ready_o,
    input  var logic [ 7:0] req_tag_i,
    input  var logic [23:0] req_index_i,
    input  var logic [63:0] req_hps_addr_i,
    input  var logic [31:0] req_vram_addr_i,
    input  var logic [31:0] req_len_i,
    input  var logic [15:0] req_epoch_i,
    input  var logic [ 7:0] req_dst_slot_i,
    input  var logic [15:0] req_new_gen_i,
    input  var logic [31:0] req_crc_i,

    input  var logic [31:0] cfg_region_base_i,
    input  var logic [31:0] cfg_region_bytes_i,
    input  var logic [63:0] cfg_arena_base_i,
    input  var logic [31:0] cfg_arena_bytes_i,
    input  var logic [15:0] cfg_epoch_i,

    // HPS read, flattened
    output var logic        hps_req_valid_o,
    output var logic [31:0] hps_req_addr_o,
    output var logic [ 6:0] hps_req_len_o,
    input  var logic        hps_req_grant_i,
    input  var logic        hps_beat_valid_i,
    input  var logic [63:0] hps_data_i,
    input  var logic        hps_last_i,
    input  var logic        hps_err_i,

    // guard write, flattened
    output var logic        guard_req_valid_o,
    output var logic [26:0] guard_req_addr_o,
    input  var logic        guard_ready_i,
    input  var logic        guard_ok_i,
    input  var logic        guard_violation_i,
    output var logic [63:0] guard_wdata_o,
    output var logic        guard_wvalid_o,
    input  var logic        guard_wready_i,
    output var logic        guard_wlast_o,
    input  var logic [ 7:0] retire_words_i,

    output var logic        publish_valid_o,
    output var logic [ 7:0] publish_slot_o,
    output var logic [15:0] publish_generation_o,
    output var logic [ 7:0] publish_tag_o,
    output var logic [23:0] publish_index_o,
    output var logic [31:0] publish_base_o,
    output var logic [31:0] publish_extent_o,

    output var logic        done_o,
    output var logic [ 7:0] status_o,
    output var logic [15:0] uploads_published_o,
    // Broken out one by one so the bench names a REASON rather than a bit
    // offset. The index is the oracle's enum value.
    output var logic [15:0] refused_unaligned_o,
    output var logic [15:0] refused_zero_length_o,
    output var logic [15:0] refused_outside_guard_o,
    output var logic [15:0] refused_epoch_stale_o,
    output var logic [15:0] refused_crc_fail_o,
    output var logic [15:0] refused_src_arena_o,
    output var logic [15:0] refused_src_unreachable_o
);

  zhao_hps_burst_req_t hps_req_w;
  zhao_hps_burst_rsp_t hps_rsp_w;
  zhao_guard_req_t     guard_req_w;
  zhao_guard_rsp_t     guard_rsp_w;
  logic [8*16-1:0]     refused_w;

  assign hps_req_valid_o = hps_req_w.valid;
  assign hps_req_addr_o  = hps_req_w.addr;
  assign hps_req_len_o   = hps_req_w.len;

  always_comb begin
    hps_rsp_w            = '0;
    hps_rsp_w.beat_valid = hps_beat_valid_i;
    hps_rsp_w.data       = hps_data_i;
    hps_rsp_w.last       = hps_last_i;
    hps_rsp_w.err        = hps_err_i;
  end

  assign guard_req_valid_o = guard_req_w.valid;
  assign guard_req_addr_o  = guard_req_w.addr;

  always_comb begin
    guard_rsp_w           = '0;
    guard_rsp_w.ready     = guard_ready_i;
    guard_rsp_w.ok        = guard_ok_i;
    guard_rsp_w.violation = guard_violation_i;
  end

  assign refused_unaligned_o       = refused_w[16*1 +: 16];
  assign refused_zero_length_o     = refused_w[16*2 +: 16];
  assign refused_outside_guard_o   = refused_w[16*3 +: 16];
  assign refused_epoch_stale_o     = refused_w[16*4 +: 16];
  assign refused_crc_fail_o        = refused_w[16*5 +: 16];
  assign refused_src_arena_o       = refused_w[16*6 +: 16];
  assign refused_src_unreachable_o = refused_w[16*7 +: 16];

  zhao_mem_upload u_dut (
      .clk   (clk),
      .rst_n (rst_n),

      .req_valid_i     (req_valid_i),
      .req_ready_o     (req_ready_o),
      .req_tag_i       (req_tag_i),
      .req_index_i     (req_index_i),
      .req_hps_addr_i  (req_hps_addr_i),
      .req_vram_addr_i (req_vram_addr_i),
      .req_len_i       (req_len_i),
      .req_epoch_i     (req_epoch_i),
      .req_dst_slot_i  (req_dst_slot_i),
      .req_new_gen_i   (req_new_gen_i),
      .req_crc_i       (req_crc_i),

      .cfg_region_base_i  (cfg_region_base_i),
      .cfg_region_bytes_i (cfg_region_bytes_i),
      .cfg_arena_base_i   (cfg_arena_base_i),
      .cfg_arena_bytes_i  (cfg_arena_bytes_i),
      .cfg_epoch_i        (cfg_epoch_i),

      .hps_req_o       (hps_req_w),
      .hps_req_grant_i (hps_req_grant_i),
      .hps_rsp_i       (hps_rsp_w),

      .guard_req_o    (guard_req_w),
      .guard_rsp_i    (guard_rsp_w),
      .guard_wdata_o  (guard_wdata_o),
      .guard_wvalid_o (guard_wvalid_o),
      .guard_wready_i (guard_wready_i),
      .guard_wlast_o  (guard_wlast_o),
      .retire_words_i (retire_words_i),

      .publish_valid_o      (publish_valid_o),
      .publish_slot_o       (publish_slot_o),
      .publish_generation_o (publish_generation_o),
      .publish_tag_o        (publish_tag_o),
      .publish_index_o      (publish_index_o),
      .publish_base_o       (publish_base_o),
      .publish_extent_o     (publish_extent_o),

      .done_o              (done_o),
      .status_o            (status_o),
      .uploads_published_o (uploads_published_o),
      .refused_o           (refused_w)
  );

endmodule

`default_nettype wire
