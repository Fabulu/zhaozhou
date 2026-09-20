// tb_terrain_lodhist.sv -- ENTRY I18's VALUE TRAVERSE, owner ruling R70.
//
//   TERRAIN.MIPFEED's fine stream -> zhao_terrain_lodfeed (buffer + the R8 walk,
//   which instantiates zhao_terrain_loddev) -> the 24 -> 32 widening ->
//   zhao_measure_histogram -> a host read of the frozen bank's bins.
//
// WHY THIS BENCH EXISTS AND `terrain_lodpath` DOES NOT COVER IT. That one ends
// at `zhao_terrain_devstore` and proves the deviation is CORRECT. This one ends
// at the organ the console actually composes and proves the deviation ARRIVES:
// they are different claims and the second is the one entry I18 was about.
//
// AND THE HARDER REASON, which is the whole point of the file. Ruling R70's
// third requirement is "CHECK FIRST THAT THE SMOKE'S STIMULUS DRIVES
// TERRAIN.MIPFEED'S FINE STREAM AT ALL ... A traverse quoted against a stream
// that never moves is the gap re-opened under a green gate." It was checked, by
// printing the lane, and the answer is NO: every page the console smoke plays
// fails its CRC, so TERRAIN.MIPREQ issues no job and `samples_sent` is 0. The
// console bench therefore CANNOT be the evidence for this chain, however green
// it is. This bench is, because here the lattice moves.
//
// IT IS A WRAPPER, NOT A SECOND IMPLEMENTATION. Every wire below is a
// port-to-port connection and the widening is COPIED FROM `zhao_console_core.sv`
// character for character -- if the two ever differ, this bench is measuring an
// arrangement the console does not have, which is the stale-mutant failure in
// new clothes. The parameters are the console's.
`default_nettype none

module tb_terrain_lodhist #(
    parameter int unsigned SLOTW = 4,          // 16 slots is plenty for a bench
    parameter bit DEV_INCLUDE_BOUNDARY = 1'b1, // owner ruling R22
    // The console's histogram geometry. Restated rather than defaulted, so a
    // parameter change in the core breaks this bench instead of rescaling it.
    parameter int unsigned EW       = 32,
    parameter int unsigned SUB_BITS = 1,
    parameter int unsigned LANES    = 4,
    parameter int unsigned CW       = 24,
    parameter int unsigned BINW     = $clog2((EW - SUB_BITS + 1) << SUB_BITS)
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- the mip pass, played by the bench --------------------------------
    input  var logic               f_start_i,
    input  var logic [SLOTW-1:0]   f_slot_i,
    input  var logic [15:0]        f_src_id_i,
    input  var logic               f_valid_i,
    input  var logic signed [15:0] f_h_i,

    // ---- the histogram's host side, driven by the bench --------------------
    // These names are the block's own, so `tests/measure/histogram_dev.hpp`'s
    // `read_bin`, `read_all` and `snapshot` helpers work on this top unchanged.
    input  var logic            snapshot_i,
    input  var logic            rd_valid_i,
    input  var logic [BINW-1:0] rd_bin_i,
    output var logic            rd_ready_o,
    output var logic            rd_data_valid_o,
    output var logic [CW-1:0]   rd_count_o,

    // ---- the interval ------------------------------------------------------
    output var logic          snap_valid_o,
    output var logic [CW-1:0] snap_total_o,
    output var logic [15:0]   snap_src_id_o,
    output var logic [CW-1:0] snap_index_o,

    // ---- the histogram's evidence -----------------------------------------
    output var logic [CW-1:0] events_o,
    output var logic [CW-1:0] updates_o,
    output var logic [CW-1:0] stall_cycles_o,
    output var logic [CW-1:0] bin_sat_o,
    output var logic [CW-1:0] fwd_hits_o,
    output var logic [CW-1:0] host_conflict_o,
    output var logic [CW-1:0] snapshots_o,
    output var logic [CW-1:0] frozen_write_o,

    // ---- the producer's evidence ------------------------------------------
    output var logic [31:0] lattices_seen_o,
    output var logic [31:0] lattices_walked_o,
    output var logic [31:0] lattices_dropped_o,
    output var logic [31:0] surface1_samples_o,
    output var logic [31:0] stray_samples_o,
    output var logic [31:0] walk_clocks_o,
    output var logic [31:0] dev_records_o,
    output var logic [31:0] dev_clipped_o,
    output var logic        feed_busy_o,

    // ---- the seam itself, exported so a check can read the WIRE and not
    // ---- only the counters at either end of it ----------------------------
    output var logic        w_valid_o,
    output var logic        w_ready_o,
    output var logic [23:0] w_dev1_o,
    output var logic [23:0] w_dev2_o,
    output var logic [23:0] w_dev3_o,
    output var logic [15:0] w_src_id_o
);

  /* verilator lint_off UNUSEDSIGNAL */
  // The devstore's half of lodfeed's write port. The console does not compose
  // the store either (ruling R59 prices it at ~77 M10K and its only reader,
  // zhao_terrain_lod, is core entry I21), so this bench reproduces the
  // console's arrangement by leaving them unread HERE TOO. Waived at the
  // declaration rather than left as empty pins, because an empty pin in a
  // bench that claims to mirror the core is exactly where a difference hides.
  wire [SLOTW-1:0]    w_slot_w;
  wire [3:0]          w_sp_w;
  wire signed [15:0]  w_cy_w;
  wire                inv_valid_w;
  wire [SLOTW-1:0]    inv_slot_w;
  wire [31:0]         dev_vertices_w, dev_lattice_reads_w;
  /* verilator lint_on UNUSEDSIGNAL */

  zhao_terrain_lodfeed #(
    .SLOTW               (SLOTW),
    .EDGE                (33),
    .DEV_INCLUDE_BOUNDARY(DEV_INCLUDE_BOUNDARY)
  ) u_feed (
    .clk  (clk),
    .rst_n(rst_n),

    .f_start_i (f_start_i),
    .f_slot_i  (f_slot_i),
    .f_src_id_i(f_src_id_i),
    .f_valid_i (f_valid_i),
    .f_h_i     (f_h_i),

    .w_valid_o (w_valid_o),
    .w_ready_i (w_ready_o),
    .w_slot_o  (w_slot_w),
    .w_sp_o    (w_sp_w),
    .w_dev1_o  (w_dev1_o),
    .w_dev2_o  (w_dev2_o),
    .w_dev3_o  (w_dev3_o),
    .w_cy_o    (w_cy_w),
    .w_src_id_o(w_src_id_o),

    .inv_valid_o(inv_valid_w),
    .inv_slot_o (inv_slot_w),

    .lattices_seen_o    (lattices_seen_o),
    .lattices_walked_o  (lattices_walked_o),
    .lattices_dropped_o (lattices_dropped_o),
    .surface1_samples_o (surface1_samples_o),
    .stray_samples_o    (stray_samples_o),
    .walk_clocks_o      (walk_clocks_o),
    .dev_records_o      (dev_records_o),
    .dev_clipped_o      (dev_clipped_o),
    .dev_vertices_o     (dev_vertices_w),
    .dev_lattice_reads_o(dev_lattice_reads_w),
    .busy_o             (feed_busy_o)
  );

  // THE WIDENING, COPIED FROM `zhao_console_core.sv`. Lanes 0..2 carry
  // dev1/dev2/dev3 zero-extended 24 -> 32; lane 3's `lane_valid` is low, so it
  // contributes no event rather than a zero one. `spec/measure_rules.md`
  // section 3 is why zero-extension moves no value into a different bucket.
  wire [LANES*EW-1:0] ev_err_c =
      { {EW{1'b0}},                            // lane 3: not valid
        {(EW-24){1'b0}}, w_dev3_o,             // lane 2
        {(EW-24){1'b0}}, w_dev2_o,             // lane 1
        {(EW-24){1'b0}}, w_dev1_o };           // lane 0

  // synthesis translate_off
  initial begin
    if (LANES != 4)
      $fatal(1, "tb_terrain_lodhist: the R70 event mapping assumes LANES=4, got %0d", LANES);
    if (EW < 24)
      $fatal(1, "tb_terrain_lodhist: EW=%0d truncates a 24-bit LOD deviation", EW);
  end
  // synthesis translate_on

  zhao_measure_histogram #(
    .EW       (EW),
    .SUB_BITS (SUB_BITS),
    .LANES    (LANES),
    .CW       (CW)
  ) u_hist (
    .clk   (clk),
    .rst_n (rst_n),

    .ev_valid_i     (w_valid_o),
    .ev_lane_valid_i(4'b0111),
    .ev_err_i       (ev_err_c),
    .ev_src_id_i    (w_src_id_o),
    .ev_ready_o     (w_ready_o),

    .snapshot_i(snapshot_i),

    .rd_valid_i     (rd_valid_i),
    .rd_bin_i       (rd_bin_i),
    .rd_ready_o     (rd_ready_o),
    .rd_data_valid_o(rd_data_valid_o),
    .rd_count_o     (rd_count_o),

    .snap_valid_o (snap_valid_o),
    .snap_total_o (snap_total_o),
    .snap_src_id_o(snap_src_id_o),
    .snap_index_o (snap_index_o),

    .events_o       (events_o),
    .updates_o      (updates_o),
    .stall_cycles_o (stall_cycles_o),
    .bin_sat_o      (bin_sat_o),
    .fwd_hits_o     (fwd_hits_o),
    .host_conflict_o(host_conflict_o),
    .snapshots_o    (snapshots_o),
    .frozen_write_o (frozen_write_o)
  );

endmodule

`default_nettype wire
