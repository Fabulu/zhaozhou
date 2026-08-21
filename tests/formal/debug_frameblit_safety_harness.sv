// debug_frameblit_safety_harness.sv — formal harness for
// tests/formal/debug_frameblit_safety.sby. Testbench component, NEVER
// synthesis or the Verilator ctests.
//
// The harness instantiates the REAL zhao_debug_frameblit with a SHRUNK canvas
// (FORMAL_CANVAS_BYTES = 64, one chunk) so a whole transaction — accept,
// validate, read, guard, write, retire, publish — fits inside the BMC depth.
//
// Why that parameter is load-bearing rather than a convenience: the smallest
// lawful canvas is 184,320 bytes, which is 2,880 chunks and over 46,000 cycles.
// A bounded model cannot reach `publish_valid_o` at any tractable depth, so
// every publish property would hold VACUOUSLY — satisfied by never raising its
// own antecedent. That is precisely the shape MEM.GUARD's formal lane failed
// in, and it is why the cover task is not optional here.
//
// EVERY DUT INPUT IS A FREE FORMAL INPUT. In particular the lease, the bridge
// responses, the bridge grant, the guard verdict and the retirement credits are
// ARBITRARY — which is the adversarial model the properties need. Can ANY
// combination of a flickering lease, a hostile guard, an erratic bridge and
// credits arriving out of nowhere push this block into publishing a slot it
// does not own, or releasing one whose writes are still in flight?
module zhao_debug_frameblit_safety_harness (
    input logic clk,
    input logic rst_n,

    input logic        req_valid_i,
    input logic [ 7:0] req_dst_slot_i,
    input logic [ 7:0] req_mode_i,
    input logic [31:0] req_src_i,
    input logic [31:0] req_len_i,
    input logic [31:0] req_crc_i,

    input logic        fb_lease_valid_i,
    input logic        fb_lease_slot_i,
    input logic [15:0] fb_lease_generation_i,

    input logic                          hps_req_grant_i,
    input zhao_pkg::zhao_hps_burst_rsp_t hps_rsp_i,

    input zhao_pkg::zhao_guard_rsp_t guard_rsp_i,
    input logic                      guard_wready_i,

    input logic [7:0] retire_words_i
);

  logic        req_ready_o;
  logic        release_valid_o;
  logic        release_slot_o;
  logic [15:0] release_generation_o;
  logic        publish_valid_o;
  logic        publish_slot_o;
  logic [15:0] publish_generation_o;

  zhao_pkg::zhao_hps_burst_req_t hps_req_o;
  zhao_pkg::zhao_guard_req_t     guard_req_o;
  logic [63:0]                   guard_wdata_o;
  logic                          guard_wvalid_o;
  logic                          guard_wlast_o;

  logic        done_o;
  logic [ 7:0] status_o;
  logic [31:0] blits_published_o;
  logic [31:0] blits_rejected_o;

  zhao_debug_frameblit #(
      .FORMAL_CANVAS_BYTES(64)
  ) u_blit (
      .clk  (clk),
      .rst_n(rst_n),

      .req_valid_i   (req_valid_i),
      .req_ready_o   (req_ready_o),
      .req_dst_slot_i(req_dst_slot_i),
      .req_mode_i    (req_mode_i),
      .req_src_i     (req_src_i),
      .req_len_i     (req_len_i),
      .req_crc_i     (req_crc_i),

      .fb_lease_valid_i     (fb_lease_valid_i),
      .fb_lease_slot_i      (fb_lease_slot_i),
      .fb_lease_generation_i(fb_lease_generation_i),
      .release_valid_o      (release_valid_o),
      .release_slot_o       (release_slot_o),
      .release_generation_o (release_generation_o),
      .publish_valid_o      (publish_valid_o),
      .publish_slot_o       (publish_slot_o),
      .publish_generation_o (publish_generation_o),

      .hps_req_o      (hps_req_o),
      .hps_req_grant_i(hps_req_grant_i),
      .hps_rsp_i      (hps_rsp_i),

      .guard_req_o   (guard_req_o),
      .guard_rsp_i   (guard_rsp_i),
      .guard_wdata_o (guard_wdata_o),
      .guard_wvalid_o(guard_wvalid_o),
      .guard_wready_i(guard_wready_i),
      .guard_wlast_o (guard_wlast_o),

      .retire_words_i(retire_words_i),

      .done_o           (done_o),
      .status_o         (status_o),
      .blits_published_o(blits_published_o),
      .blits_rejected_o (blits_rejected_o)
  );

endmodule : zhao_debug_frameblit_safety_harness
