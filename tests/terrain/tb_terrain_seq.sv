// tb_terrain_seq.sv - TERRAIN.SEQ's ports flattened for the C++ side.
//
// THE RESIDENCY IS DRIVEN FROM C++, NOT MODELLED HERE, and that is the whole
// shape of this differential. TERRAIN.ISLAND's bench models its store because
// the thing under test there is the outcome MAPPING; the thing under test here
// is the SEQUENCE, so the directory's answers are an INPUT to both the RTL and
// the oracle rather than something either of them derives.
//
// A bench that modelled a set-associative directory would have to agree with
// `zhao_terrain_residency_v2` about victim choice, generation bumps and pin
// accounting before it could say anything about sequencing at all, and its
// first disagreement would be reported as a sequencing defect. Feeding the
// identical answer stream to `zref::terrain::seq::Sequencer` and to the RTL
// removes that failure class entirely: whatever the directory says, the two
// must do the same thing with it.
//
// COMPOSE_SLOTS IS 16 HERE AND 256 IN THE BLOCK'S OWN DEFAULT. T6's fault is
// "more than 256 required dynamic patches in one frame", and reaching it
// honestly at 256 costs 257 records on every frame that wants to see it. The
// allocator's law is "slot n goes to the n-th record of THIS frame needing
// composition, and the (n+1)-th faults", which is the same law at 16 and is
// reachable in a short frame. The 256 shape is elaborated by the lint gate,
// which runs the module on its own defaults.
module tb_terrain_seq (
    input  logic clk,
    input  logic rst_n,

    input  logic        fr_start,
    input  logic [31:0] fr_epoch,
    input  logic [15:0] fr_patch_count,
    input  logic [31:0] fr_sequence,
    output logic        fr_busy,
    output logic        fr_done,

    input  logic [15:0] cfg_load_budget,

    input  logic        rec_valid,
    output logic        rec_ready,
    input  logic [31:0] rec_island,
    input  logic [15:0] rec_ix,
    input  logic [15:0] rec_iz,
    input  logic [63:0] rec_hps_addr,
    input  logic [31:0] rec_crc,
    input  logic [15:0] rec_flags,
    input  logic [7:0]  rec_view_mask,
    input  logic [7:0]  rec_priority,
    input  logic [31:0] rec_src_id,

    // ---- the residency lookup, answered by the C++ side -------------------
    output logic        lu_valid,
    output logic [31:0] lu_epoch,
    output logic [31:0] lu_island,
    output logic [15:0] lu_ix,
    output logic [15:0] lu_iz,
    input  logic        lu_ans_valid,
    input  logic        lu_ans_hit,
    input  logic [9:0]  lu_ans_slot,
    input  logic [7:0]  lu_ans_gen,

    // ---- the residency claim ----------------------------------------------
    output logic        cl_valid,
    input  logic        cl_ready,
    output logic [31:0] cl_epoch,
    output logic [31:0] cl_island,
    output logic [15:0] cl_ix,
    output logic [15:0] cl_iz,
    output logic [31:0] cl_expect_crc,
    output logic [15:0] cl_seq,
    input  logic        cl_ans_valid,
    input  logic        cl_ans_same,
    input  logic        cl_ans_refused,
    input  logic [9:0]  cl_ans_slot,
    input  logic [7:0]  cl_ans_gen,
    input  logic        cl_ans_ev_dirty,
    input  logic [31:0] cl_ans_ev_island,
    input  logic [15:0] cl_ans_ev_ix,
    input  logic [15:0] cl_ans_ev_iz,
    input  logic [7:0]  cl_ans_ev_gen,

    output logic        pin_valid,
    input  logic        pin_ready,
    output logic [15:0] pin_slot,
    output logic [7:0]  pin_gen,
    output logic [31:0] pin_epoch,

    output logic        wb_valid,
    input  logic        wb_ready,
    output logic [15:0] wb_slot,
    output logic [7:0]  wb_gen,
    output logic [31:0] wb_epoch,
    output logic [31:0] wb_island,
    output logic [15:0] wb_ix,
    output logic [15:0] wb_iz,
    output logic [31:0] wb_src_id,

    output logic        ld_valid,
    input  logic        ld_ready,
    output logic [15:0] ld_slot,
    output logic [7:0]  ld_gen,
    output logic [31:0] ld_epoch,
    output logic [31:0] ld_island,
    output logic [15:0] ld_ix,
    output logic [15:0] ld_iz,
    output logic [63:0] ld_hps_addr,
    output logic [31:0] ld_expect_crc,
    output logic [31:0] ld_src_id,

    output logic        is_valid,
    input  logic        is_ready,
    output logic [15:0] is_slot,
    output logic [7:0]  is_gen,
    output logic [31:0] is_epoch,
    output logic [31:0] is_island,
    output logic [15:0] is_ix,
    output logic [15:0] is_iz,
    output logic        is_cslot_valid,
    output logic [7:0]  is_cslot,
    output logic [15:0] is_flags,
    output logic [7:0]  is_view_mask,
    output logic [7:0]  is_priority,
    output logic [31:0] is_src_id,

    output logic        frame_fault,
    output logic [31:0] fault_src_id,
    output logic [31:0] fault_island,
    output logic [15:0] fault_ix,
    output logic [15:0] fault_iz,

    output logic        err_stray_ans,

    output logic [31:0] c_records_consumed,
    output logic [31:0] c_patches_issued,
    output logic [31:0] c_prefetch_resident,
    output logic [31:0] c_skipped_not_resident,
    output logic [31:0] c_claims_issued,
    output logic [31:0] c_claims_refused,
    output logic [31:0] c_claims_same,
    output logic [31:0] c_loads_issued,
    output logic [31:0] c_loads_deferred,
    output logic [31:0] c_writebacks_issued,
    output logic [31:0] c_compose_slots_used,
    output logic [31:0] c_pins_issued,
    output logic [31:0] c_drained,
    output logic [31:0] c_frame_faults
);

  localparam int unsigned CS    = 16;  // COMPOSE_SLOTS under test
  localparam int unsigned SLOTW = 10;
  localparam int unsigned GENW  = 8;

  logic [SLOTW-1:0] d_pin_slot, d_wb_slot, d_ld_slot, d_is_slot;
  logic [3:0]       d_is_cslot;

  assign pin_slot = {{(16-SLOTW){1'b0}}, d_pin_slot};
  assign wb_slot  = {{(16-SLOTW){1'b0}}, d_wb_slot};
  assign ld_slot  = {{(16-SLOTW){1'b0}}, d_ld_slot};
  assign is_slot  = {{(16-SLOTW){1'b0}}, d_is_slot};
  assign is_cslot = {4'd0, d_is_cslot};

  zhao_terrain_seq #(
      .COMPOSE_SLOTS(CS),
      .SLOTW(SLOTW),
      .GENW(GENW),
      .SEQW(16)
  ) dut (
      .clk  (clk),
      .rst_n(rst_n),

      .fr_start_i      (fr_start),
      .fr_epoch_i      (fr_epoch),
      .fr_patch_count_i(fr_patch_count),
      .fr_sequence_i   (fr_sequence),
      .fr_busy_o       (fr_busy),
      .fr_done_o       (fr_done),

      .cfg_load_budget_i(cfg_load_budget),

      .rec_valid_i    (rec_valid),
      .rec_ready_o    (rec_ready),
      .rec_island_i   (rec_island),
      .rec_ix_i       (signed'(rec_ix)),
      .rec_iz_i       (signed'(rec_iz)),
      .rec_hps_addr_i (rec_hps_addr),
      .rec_crc_i      (rec_crc),
      .rec_flags_i    (rec_flags),
      .rec_view_mask_i(rec_view_mask),
      .rec_priority_i (rec_priority),
      .rec_src_id_i   (rec_src_id),

      .lu_valid_o    (lu_valid),
      .lu_epoch_o    (lu_epoch),
      .lu_island_o   (lu_island),
      .lu_ix_o       (lu_ix),
      .lu_iz_o       (lu_iz),
      .lu_ans_valid_i(lu_ans_valid),
      .lu_ans_hit_i  (lu_ans_hit),
      .lu_ans_slot_i (lu_ans_slot),
      .lu_ans_gen_i  (lu_ans_gen),

      .cl_valid_o        (cl_valid),
      .cl_ready_i        (cl_ready),
      .cl_epoch_o        (cl_epoch),
      .cl_island_o       (cl_island),
      .cl_ix_o           (cl_ix),
      .cl_iz_o           (cl_iz),
      .cl_expect_crc_o   (cl_expect_crc),
      .cl_seq_o          (cl_seq),
      .cl_ans_valid_i    (cl_ans_valid),
      .cl_ans_same_i     (cl_ans_same),
      .cl_ans_refused_i  (cl_ans_refused),
      .cl_ans_slot_i     (cl_ans_slot),
      .cl_ans_gen_i      (cl_ans_gen),
      .cl_ans_ev_dirty_i (cl_ans_ev_dirty),
      .cl_ans_ev_island_i(cl_ans_ev_island),
      .cl_ans_ev_ix_i    (signed'(cl_ans_ev_ix)),
      .cl_ans_ev_iz_i    (signed'(cl_ans_ev_iz)),
      .cl_ans_ev_gen_i   (cl_ans_ev_gen),

      .pin_valid_o(pin_valid),
      .pin_ready_i(pin_ready),
      .pin_slot_o (d_pin_slot),
      .pin_gen_o  (pin_gen),
      .pin_epoch_o(pin_epoch),

      .wb_valid_o (wb_valid),
      .wb_ready_i (wb_ready),
      .wb_slot_o  (d_wb_slot),
      .wb_gen_o   (wb_gen),
      .wb_epoch_o (wb_epoch),
      .wb_island_o(wb_island),
      .wb_ix_o    (wb_ix),
      .wb_iz_o    (wb_iz),
      .wb_src_id_o(wb_src_id),

      .ld_valid_o     (ld_valid),
      .ld_ready_i     (ld_ready),
      .ld_slot_o      (d_ld_slot),
      .ld_gen_o       (ld_gen),
      .ld_epoch_o     (ld_epoch),
      .ld_island_o    (ld_island),
      .ld_ix_o        (ld_ix),
      .ld_iz_o        (ld_iz),
      .ld_hps_addr_o  (ld_hps_addr),
      .ld_expect_crc_o(ld_expect_crc),
      .ld_src_id_o    (ld_src_id),

      .is_valid_o      (is_valid),
      .is_ready_i      (is_ready),
      .is_slot_o       (d_is_slot),
      .is_gen_o        (is_gen),
      .is_epoch_o      (is_epoch),
      .is_island_o     (is_island),
      .is_ix_o         (is_ix),
      .is_iz_o         (is_iz),
      .is_cslot_valid_o(is_cslot_valid),
      .is_cslot_o      (d_is_cslot),
      .is_flags_o      (is_flags),
      .is_view_mask_o  (is_view_mask),
      .is_priority_o   (is_priority),
      .is_src_id_o     (is_src_id),

      .frame_fault_o  (frame_fault),
      .fault_src_id_o (fault_src_id),
      .fault_island_o (fault_island),
      .fault_ix_o     (fault_ix),
      .fault_iz_o     (fault_iz),
      .err_stray_ans_o(err_stray_ans),

      .records_consumed_o    (c_records_consumed),
      .patches_issued_o      (c_patches_issued),
      .prefetch_resident_o   (c_prefetch_resident),
      .skipped_not_resident_o(c_skipped_not_resident),
      .claims_issued_o       (c_claims_issued),
      .claims_refused_o      (c_claims_refused),
      .claims_same_o         (c_claims_same),
      .loads_issued_o        (c_loads_issued),
      .loads_deferred_o      (c_loads_deferred),
      .writebacks_issued_o   (c_writebacks_issued),
      .compose_slots_used_o  (c_compose_slots_used),
      .pins_issued_o         (c_pins_issued),
      .drained_o             (c_drained),
      .frame_faults_o        (c_frame_faults)
  );

endmodule
