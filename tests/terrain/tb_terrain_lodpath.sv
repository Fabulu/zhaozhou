// tb_terrain_lodpath.sv -- the PAGE-LOAD DEVIATION PATH, end to end.
//
// TERRAIN.MIPFEED's fine stream -> zhao_terrain_lodfeed (buffer + the R8 walk)
// -> zhao_terrain_devstore (the R24/R59 per-page store) -> a read.
//
// It is a wrapper and not a second implementation of anything: every wire below
// is a port-to-port connection, and the two blocks are the production files.
// The point of testing them together is that the thing to prove is a VALUE
// TRAVERSING -- a height that entered as a page-load sample coming back out as
// the deviation zref computes -- and neither block alone can show that.
`default_nettype none

module tb_terrain_lodpath #(
    parameter int unsigned SLOTW = 4,          // 16 slots is plenty for a bench
    parameter bit DEV_INCLUDE_BOUNDARY = 1'b1  // owner ruling R22
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- the mip pass, played by the bench --------------------------------
    input  var logic               f_start_i,
    input  var logic [SLOTW-1:0]   f_slot_i,
    input  var logic [15:0]        f_src_id_i,
    input  var logic               f_valid_i,
    input  var logic signed [15:0] f_h_i,

    // ---- the store's read side, driven by the bench -----------------------
    input  var logic             r_start_i,
    input  var logic [SLOTW-1:0] r_slot_i,
    output var logic             r_ready_o,
    output var logic             r_valid_o,
    input  var logic             r_ready_i,
    output var logic [3:0]       r_sp_o,
    output var logic [23:0]      r_dev1_o,
    output var logic [23:0]      r_dev2_o,
    output var logic [23:0]      r_dev3_o,
    output var logic signed [15:0] r_cy_o,
    output var logic [1:0]       r_prev_level_o,
    output var logic [16:0]      r_prev_morph_o,
    output var logic [7:0]       r_hold_o,
    output var logic             r_fresh_o,

    // ---- the history writeback, driven by the bench -----------------------
    input  var logic        h_valid_i,
    output var logic        h_ready_o,
    input  var logic [1:0]  h_level_i,
    input  var logic [16:0] h_morph_i,
    input  var logic [7:0]  h_hold_i,

    // ---- evidence ---------------------------------------------------------
    output var logic [31:0] lattices_seen_o,
    output var logic [31:0] lattices_walked_o,
    output var logic [31:0] lattices_dropped_o,
    output var logic [31:0] surface1_samples_o,
    output var logic [31:0] stray_samples_o,
    output var logic [31:0] walk_clocks_o,
    output var logic [31:0] dev_records_o,
    output var logic [31:0] dev_clipped_o,
    output var logic [31:0] dev_vertices_o,
    output var logic [31:0] dev_lattice_reads_o,
    output var logic        feed_busy_o,

    // `zhao_terrain_lodfeed` gained `w_src_id_o` on 2026-09-20 (ruling R70).
    // The store keys on the SLOT and has no src_id input, so this wrapper
    // carries it out to the bench rather than leaving the pin empty: a new
    // port left unconnected here is the PINMISSING the packet protocol warns
    // about, and an empty pin says nothing about whether the value is right.
    output var logic [15:0] w_src_id_o,

    output var logic [31:0] records_written_o,
    output var logic [31:0] patches_committed_o,
    output var logic [31:0] patches_read_o,
    output var logic [31:0] read_unwritten_o,
    output var logic [31:0] hist_step_bad_o,
    output var logic [31:0] invalidations_o,
    output var logic        store_busy_o
);

  logic               w_valid, w_ready;
  logic [SLOTW-1:0]   w_slot;
  logic [3:0]         w_sp;
  logic [23:0]        w_dev1, w_dev2, w_dev3;
  logic signed [15:0] w_cy;
  /* verilator lint_off UNUSEDSIGNAL */
  logic               store_patch_done_w;   // the store's own commit pulse; the
                                            // bench reads patches_committed_o
  /* verilator lint_on UNUSEDSIGNAL */
  logic               inv_valid;
  logic [SLOTW-1:0]   inv_slot;

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

    .w_valid_o(w_valid),
    .w_ready_i(w_ready),
    .w_slot_o (w_slot),
    .w_sp_o   (w_sp),
    .w_dev1_o (w_dev1),
    .w_dev2_o (w_dev2),
    .w_dev3_o (w_dev3),
    .w_cy_o   (w_cy),
    .w_src_id_o(w_src_id_o),

    .inv_valid_o(inv_valid),
    .inv_slot_o (inv_slot),

    .lattices_seen_o    (lattices_seen_o),
    .lattices_walked_o  (lattices_walked_o),
    .lattices_dropped_o (lattices_dropped_o),
    .surface1_samples_o (surface1_samples_o),
    .stray_samples_o    (stray_samples_o),
    .walk_clocks_o      (walk_clocks_o),
    .dev_records_o      (dev_records_o),
    .dev_clipped_o      (dev_clipped_o),
    .dev_vertices_o     (dev_vertices_o),
    .dev_lattice_reads_o(dev_lattice_reads_o),
    .busy_o             (feed_busy_o)
  );

  zhao_terrain_devstore #(
    .SLOTS (1 << SLOTW),
    .SLOTW (SLOTW),
    .DEVW  (24),
    .MORPHW(17)
  ) u_store (
    .clk  (clk),
    .rst_n(rst_n),

    .w_valid_i     (w_valid),
    .w_ready_o     (w_ready),
    .w_slot_i      (w_slot),
    .w_sp_i        (w_sp),
    .w_dev1_i      (w_dev1),
    .w_dev2_i      (w_dev2),
    .w_dev3_i      (w_dev3),
    .w_cy_i        (w_cy),
    .w_patch_done_o(store_patch_done_w),

    .inv_valid_i(inv_valid),
    .inv_slot_i (inv_slot),

    .r_start_i     (r_start_i),
    .r_slot_i      (r_slot_i),
    .r_ready_o     (r_ready_o),
    .r_valid_o     (r_valid_o),
    .r_ready_i     (r_ready_i),
    .r_sp_o        (r_sp_o),
    .r_dev1_o      (r_dev1_o),
    .r_dev2_o      (r_dev2_o),
    .r_dev3_o      (r_dev3_o),
    .r_cy_o        (r_cy_o),
    .r_prev_level_o(r_prev_level_o),
    .r_prev_morph_o(r_prev_morph_o),
    .r_hold_o      (r_hold_o),
    .r_fresh_o     (r_fresh_o),

    .h_valid_i(h_valid_i),
    .h_ready_o(h_ready_o),
    .h_level_i(h_level_i),
    .h_morph_i(h_morph_i),
    .h_hold_i (h_hold_i),

    .records_written_o  (records_written_o),
    .patches_committed_o(patches_committed_o),
    .patches_read_o     (patches_read_o),
    .read_unwritten_o   (read_unwritten_o),
    .hist_step_bad_o    (hist_step_bad_o),
    .invalidations_o    (invalidations_o),
    .busy_o             (store_busy_o)
  );

endmodule

`default_nettype wire
