// tb_field_progdir_diff.sv -- the retained oracle (zhao_field_progcache, o_*)
// beside the scanned candidate (zhao_field_progdir, n_*), same parameters,
// same clock and reset. Driven by tests/field/field_progdir_differential.cpp,
// which runs the SAME transaction script through both at each one's own pace
// and compares transaction by transaction. Wiring only.
//
// With -DPROGDIR_MUTANT the candidate side is the committed mutant
// (tests/mutants/zhao_field_progdir_scan_mutant.sv), for the inverted-
// polarity control tests/field/field_progdir_mutant_control.cpp.

`default_nettype none

module tb_field_progdir_diff #(
    parameter int ENTRIES = 16,
    parameter int LRUW    = 48
) (
    input wire clk,
    input wire rst_n,

    // ---- oracle -------------------------------------------------------------
    input  wire        o_lu_valid_i,
    output wire        o_lu_ready_o,
    input  wire [31:0] o_lu_hash_i,
    output wire        o_lu_resp_valid_o,
    input  wire        o_lu_resp_ready_i,
    output wire        o_lu_hit_o,
    output wire [$clog2(ENTRIES)-1:0] o_lu_slot_o,
    input  wire        o_cm_valid_i,
    output wire        o_cm_ready_o,
    input  wire [31:0] o_cm_hash_i,
    input  wire        o_cm_ok_i,
    output wire        o_cm_resp_valid_o,
    input  wire        o_cm_resp_ready_i,
    output wire        o_cm_inserted_o,
    output wire        o_cm_evicted_o,
    output wire [$clog2(ENTRIES)-1:0] o_cm_slot_o,
    output wire [31:0] o_hits_o,
    output wire [31:0] o_misses_o,
    output wire [31:0] o_programs_rejected_o,
    output wire [31:0] o_evictions_o,
    output wire [$clog2(ENTRIES):0] o_occupancy_o,

    // ---- candidate ----------------------------------------------------------
    input  wire        n_lu_valid_i,
    output wire        n_lu_ready_o,
    input  wire [31:0] n_lu_hash_i,
    output wire        n_lu_resp_valid_o,
    input  wire        n_lu_resp_ready_i,
    output wire        n_lu_hit_o,
    output wire [$clog2(ENTRIES)-1:0] n_lu_slot_o,
    input  wire        n_cm_valid_i,
    output wire        n_cm_ready_o,
    input  wire [31:0] n_cm_hash_i,
    input  wire        n_cm_ok_i,
    output wire        n_cm_resp_valid_o,
    input  wire        n_cm_resp_ready_i,
    output wire        n_cm_inserted_o,
    output wire        n_cm_evicted_o,
    output wire [$clog2(ENTRIES)-1:0] n_cm_slot_o,
    output wire [31:0] n_hits_o,
    output wire [31:0] n_misses_o,
    output wire [31:0] n_programs_rejected_o,
    output wire [31:0] n_evictions_o,
    output wire [$clog2(ENTRIES):0] n_occupancy_o
);

  zhao_field_progcache #(
      .ENTRIES(ENTRIES),
      .LRUW   (LRUW)
  ) u_old (
      .clk(clk), .rst_n(rst_n),
      .lu_valid_i(o_lu_valid_i), .lu_ready_o(o_lu_ready_o), .lu_hash_i(o_lu_hash_i),
      .lu_resp_valid_o(o_lu_resp_valid_o), .lu_resp_ready_i(o_lu_resp_ready_i),
      .lu_hit_o(o_lu_hit_o), .lu_slot_o(o_lu_slot_o),
      .cm_valid_i(o_cm_valid_i), .cm_ready_o(o_cm_ready_o), .cm_hash_i(o_cm_hash_i), .cm_ok_i(o_cm_ok_i),
      .cm_resp_valid_o(o_cm_resp_valid_o), .cm_resp_ready_i(o_cm_resp_ready_i),
      .cm_inserted_o(o_cm_inserted_o), .cm_evicted_o(o_cm_evicted_o), .cm_slot_o(o_cm_slot_o),
      .hits_o(o_hits_o), .misses_o(o_misses_o), .programs_rejected_o(o_programs_rejected_o),
      .evictions_o(o_evictions_o), .occupancy_o(o_occupancy_o)
  );

`ifdef PROGDIR_MUTANT
  zhao_field_progdir_mutant #(
`else
  zhao_field_progdir #(
`endif
      .ENTRIES(ENTRIES),
      .LRUW   (LRUW)
  ) u_new (
      .clk(clk), .rst_n(rst_n),
      .lu_valid_i(n_lu_valid_i), .lu_ready_o(n_lu_ready_o), .lu_hash_i(n_lu_hash_i),
      .lu_resp_valid_o(n_lu_resp_valid_o), .lu_resp_ready_i(n_lu_resp_ready_i),
      .lu_hit_o(n_lu_hit_o), .lu_slot_o(n_lu_slot_o),
      .cm_valid_i(n_cm_valid_i), .cm_ready_o(n_cm_ready_o), .cm_hash_i(n_cm_hash_i), .cm_ok_i(n_cm_ok_i),
      .cm_resp_valid_o(n_cm_resp_valid_o), .cm_resp_ready_i(n_cm_resp_ready_i),
      .cm_inserted_o(n_cm_inserted_o), .cm_evicted_o(n_cm_evicted_o), .cm_slot_o(n_cm_slot_o),
      .hits_o(n_hits_o), .misses_o(n_misses_o), .programs_rejected_o(n_programs_rejected_o),
      .evictions_o(n_evictions_o), .occupancy_o(n_occupancy_o)
  );

endmodule

`default_nettype wire
