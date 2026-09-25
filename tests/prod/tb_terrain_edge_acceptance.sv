// tb_terrain_edge_acceptance.sv -- THE ACCEPTANCE BENCH for entry I21's
// neighbour-edge level producer. It asks the only question that matters:
// DO THE TESSELLATION LEVELS ON A SHARED SEAM AGREE?
//
// ENFORCED-BY: tests/prod/terrain_edge_acceptance.cpp:main
//
// Law: reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt section 2 --
//      "A bank collision/missing neighbor cannot earn a crack-free claim
//       merely because both sides return the same sentinel: TEST THE RESULTING
//       TESSELLATION LEVELS. ... Include nondegenerate terrain fixtures, mixed
//       neighbouring LODs, both views, changing camera state, deformation,
//       missing pages, history and backpressure."
//      design/contracts/TERRAIN.EDGERECON.md, "WHAT P4 STILL OWES" item 8
//
// ===========================================================================
// WHAT IS PRODUCTION HERE AND WHAT IS A MODEL
// ===========================================================================
// SEVEN PRODUCTION MODULES ARE INSTANTIATED, not copied -- the whole decision
// path from the sealed list to the levels TERRAIN.TESS consumes:
//
//   zhao_terrain_prepwalk    the admitted-set PREPARE walker
//   zhao_terrain_prepshare   the two borrowed ports, arbitrated
//   zhao_terrain_lodshare    the time-share and the frame-scoped freeze
//   zhao_terrain_lod         THE LADDER ITSELF -- the block that decides
//   zhao_terrain_edgerecon   the bank and the symmetry law
//   zhao_terrain_edgequery   the EMIT-side driver
//   zhao_terrain_islandseal  the island pitch seal and admission check
//
// FOUR THINGS ARE MODELLED, and each is modelled because it is a SOURCE of
// bytes rather than a decider of levels. Every one has its own directed suite
// and none of them is on the path from a descriptor to a level:
//
//   the HPS bridge      -- a byte source. `zhao_hps_bridge` is proven
//                          separately; what this bench needs from it is the
//                          sealed list, and the list's CONTENT is the fixture.
//   TERRAIN.RESIDENCY   -- an (ix,iz) -> slot answer.
//                          `terrain_residency_v2_directed` owns it.
//   TERRAIN.DEVSTORE    -- sixteen deviation records per slot.
//                          `terrain_lodpath_directed` owns it.
//   TERRAIN.SPDESC      -- the EMIT descriptor producer, 1,035 checks in
//                          `terrain_spdesc_directed`. The driver presents its
//                          PORT CONTRACT, field for field.
//
// THE MODELS ARE DECLARED AS MODELS AND ARE NOT COPIES OF ANYTHING. None of
// them reproduces a law that lives in production RTL, so none of them can go
// stale in the way `tools/budget/mutant_copy_drift.py` watches for.
//
// ===========================================================================
// THE FIXTURE IS NONDEGENERATE AND THAT IS THE WHOLE POINT
// ===========================================================================
// The console smoke reaches terrain with `crc_fail=0, resident=3,
// replay_triangles=128` and ALL 128 TRIANGLES ARE DEGENERATE, so it cannot
// evidence a tessellation result. Measured this session, the cause is not the
// one the smoke's own comment gives: it says "a flat zero height field, the
// cross product is exactly zero", and a flat but PLACED lattice does not give
// a zero cross product -- `zhao_terrain_place` supplies distinct world x/z from
// the patch coordinate alone, with no page payload reaching placement at all.
// The real cause is that the smoke writes only the 64-byte header, so the
// compose cache never fills and `zhao_terrain_compcache_front` returns POISON
// (`32'sh5BADF00D`) on all three lanes; three corners at one point give
// nx = ny = nz = 0 for every triangle.
//
// This bench does not have that problem because it does not go through the
// compose cache at all: it drives the DECISION path, whose inputs are the
// deviations and the camera, and it makes them differ per patch on purpose.
//
// Conservative SystemVerilog subset only where it costs nothing; this is a
// bench and is never synthesised.
// THE BENCH'S OWN LINT WAIVERS, and they are the bench's and NOT the design's.
// Every production module above is linted -Wall clean on its own and in the
// console's closure; these two cover the DRIVER-FACING ports of this wrapper,
// which are deliberately wider than the models behind them read (a 32-bit
// word index into a 1,024-word arena, a 32-bit table index into 64 entries).
// Narrowing them would put the widths of the MODELS into the C++ driver's
// ABI, which is exactly the coupling a test wrapper should not have.
/* verilator lint_off UNUSEDSIGNAL */
/* verilator lint_off WIDTHEXPAND */
`default_nettype none

module tb_terrain_edge_acceptance
  import zhao_pkg::*;
#(
    parameter int unsigned SLOTW    = 10,
    parameter int unsigned GENW     = 8,
    parameter int unsigned DEVW     = 24,
    parameter int unsigned MORPHW   = 17,
    parameter int unsigned ARENA_W  = 1024,   // 64-bit words of HPS arena
    parameter int unsigned RES_N    = 64,     // residency model entries
    parameter int unsigned DEV_N    = 64      // devstore model slots
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- the HPS arena, written by the driver ------------------------------
    input  var logic        aw_valid_i,
    input  var logic [31:0] aw_word_i,
    input  var logic [63:0] aw_data_i,
    // Bridge backpressure: grants are withheld while this is high, which is
    // the directive's "backpressure" on the read side of PREPARE.
    input  var logic        hps_stall_i,

    // ---- the residency model, written by the driver ------------------------
    input  var logic               rw_valid_i,
    input  var logic [31:0]        rw_idx_i,
    input  var logic signed [15:0] rw_ix_i,
    input  var logic signed [15:0] rw_iz_i,
    input  var logic               rw_hit_i,     // 0 = a MISSING PAGE
    input  var logic [SLOTW-1:0]   rw_slot_i,
    input  var logic [GENW-1:0]    rw_gen_i,
    input  var logic [31:0]        rw_count_i,   // live entries

    // ---- the devstore model, written by the driver -------------------------
    input  var logic               dw_valid_i,
    input  var logic [SLOTW-1:0]   dw_slot_i,
    input  var logic [3:0]         dw_sp_i,
    input  var logic [DEVW-1:0]    dw_dev1_i,
    input  var logic [DEVW-1:0]    dw_dev2_i,
    input  var logic [DEVW-1:0]    dw_dev3_i,
    input  var logic signed [15:0] dw_cy_i,
    input  var logic [1:0]         dw_prev_level_i,
    input  var logic [MORPHW-1:0]  dw_prev_morph_i,
    input  var logic [7:0]         dw_hold_i,
    input  var logic               dw_fresh_i,
    // Devstore read latency, in clocks before the first record. Non-zero is the
    // directive's "backpressure" on the frame-critical read socket, and it is
    // what makes `terr_ps_*_blocked_clocks` and the gate move.
    input  var logic [7:0]         dev_latency_i,

    // ---- the frame's job ----------------------------------------------------
    input  var logic        cfg_valid_i,
    input  var logic [31:0] cfg_epoch_i,
    input  var logic [31:0] cfg_arena_base_i,
    input  var logic [31:0] cfg_arena_bytes_i,
    input  var logic        j_valid_i,
    output var logic        j_ready_o,
    input  var logic [31:0] j_epoch_i,
    input  var logic [31:0] j_list_off_i,
    input  var logic [31:0] j_list_bytes_i,
    input  var logic [31:0] j_list_crc_i,
    input  var logic [15:0] j_patch_count_i,

    // ---- the frame's frozen state (VIEW.EYE + MEASURE.GOVERNOR live nets) ---
    // Driven LIVE by the bench so that CHANGING CAMERA STATE can be injected
    // mid-pass and `freeze_drift_o` can be read.
    input  var logic        frame_i,
    input  var logic signed [31:0] gv_cam0_x_i,
    input  var logic signed [31:0] gv_cam0_y_i,
    input  var logic signed [31:0] gv_cam0_z_i,
    input  var logic        [15:0] gv_cam0_scale_i,
    input  var logic               gv_cam0_en_i,
    input  var logic signed [31:0] gv_cam1_x_i,
    input  var logic signed [31:0] gv_cam1_y_i,
    input  var logic signed [31:0] gv_cam1_z_i,
    input  var logic        [15:0] gv_cam1_scale_i,
    input  var logic               gv_cam1_en_i,
    input  var logic        [15:0] gv_hyst_i,
    input  var logic        [ 7:0] gv_min_hold_i,
    input  var logic        [16:0] gv_morph_step_i,
    input  var logic               dual_i,
    // THE DEFORMATION / RESIDENCY WITNESS. One pulse moves it, which is what
    // a bake or a residency change does in the console.
    input  var logic               witness_bump_i,

    // ---- the page header beat, for TERRAIN.ISLANDSEAL ----------------------
    input  var logic               hdr_valid_i,
    input  var logic signed [ 7:0] hdr_pitch_log2_i,
    input  var logic        [31:0] hdr_island_i,
    input  var logic signed [15:0] hdr_ix_i,
    input  var logic signed [15:0] hdr_iz_i,
    input  var logic signed [31:0] hdr_env_x0_i,
    input  var logic signed [31:0] hdr_env_z0_i,

    // ---- the EMIT side: TERRAIN.SPDESC's port contract ---------------------
    input  var logic               rec_valid_i,
    input  var logic signed [15:0] rec_ix_i,
    input  var logic signed [15:0] rec_iz_i,
    input  var logic        [15:0] rec_src_id_i,
    input  var logic               door_valid_i,
    input  var logic        [15:0] door_src_id_i,
    input  var logic               serve_valid_i,
    input  var logic        [15:0] serve_src_id_i,
    input  var logic               e_sp_valid_i,
    output var logic               e_sp_ready_o,
    input  var logic signed [31:0] e_sp_cx_i,
    input  var logic signed [31:0] e_sp_cy_i,
    input  var logic signed [31:0] e_sp_cz_i,
    input  var logic [DEVW-1:0]    e_sp_dev1_i,
    input  var logic [DEVW-1:0]    e_sp_dev2_i,
    input  var logic [DEVW-1:0]    e_sp_dev3_i,
    input  var logic [1:0]         e_sp_prev_level_i,
    input  var logic [MORPHW-1:0]  e_sp_prev_morph_i,
    input  var logic [7:0]         e_sp_hold_i,
    input  var logic [15:0]        e_sp_src_id_i,

    // The history writeback's sink, modelling TERRAIN.JOBISSUE -> DEVSTORE.
    input  var logic               h_valid_i,
    input  var logic [1:0]         h_level_i,
    input  var logic [MORPHW-1:0]  h_morph_i,
    input  var logic [7:0]         h_hold_i,
    output var logic               h_out_valid_o,
    output var logic [1:0]         h_out_level_o,

    // ---- THE ANSWER THE DIRECTIVE ASKS FOR ---------------------------------
    // The EMIT decision stream: the thirteen fields TERRAIN.JOBISSUE forwards
    // and TERRAIN.TESS tessellates from. `e_out_level_o` is the patch's own
    // level and `e_out_lvl_*_o` are the four neighbour levels, and
    // `max(neighbour, own)` -- computed in the driver exactly as
    // `zhao_terrain_tess` computes it -- is THE TESSELLATION LEVEL.
    output var logic        e_out_valid_o,
    input  var logic        e_out_ready_i,
    output var logic [5:0]  e_out_ox_o,
    output var logic [5:0]  e_out_oz_o,
    output var logic [1:0]  e_out_level_o,
    output var logic [1:0]  e_out_lvl_nz_o,
    output var logic [1:0]  e_out_lvl_pz_o,
    output var logic [1:0]  e_out_lvl_nx_o,
    output var logic [1:0]  e_out_lvl_px_o,
    output var logic        e_out_surface_o,
    output var logic [15:0] e_out_src_id_o,

    // The HELD answer to the ladder, so the driver can see the four words move.
    output var logic [7:0]  edge_nz_o,
    output var logic [7:0]  edge_pz_o,
    output var logic [7:0]  edge_nx_o,
    output var logic [7:0]  edge_px_o,

    // ---- state -------------------------------------------------------------
    output var logic [1:0]  phase_o,
    output var logic        prep_valid_o,
    output var logic        prep_busy_o,
    output var logic        restart_req_o,
    output var logic signed [7:0] island_pitch_o,
    output var logic        island_refuse_o,

    // ---- counters: EDGERECON -----------------------------------------------
    output var logic [31:0] er_records_filed_o,
    output var logic [31:0] er_lanes_filed_o,
    output var logic [31:0] er_collisions_o,
    output var logic [31:0] er_queries_o,
    output var logic [31:0] er_edges_real_o,
    output var logic [31:0] er_edges_fallback_o,
    output var logic [31:0] er_query_own_missing_o,
    output var logic [31:0] er_file_out_of_phase_o,
    output var logic [31:0] er_query_out_of_phase_o,

    // ---- counters: PREPWALK -------------------------------------------------
    output var logic [31:0] pw_walks_started_o,
    output var logic [31:0] pw_walks_completed_o,
    output var logic [31:0] pw_records_walked_o,
    output var logic [31:0] pw_patches_prepared_o,
    output var logic [31:0] pw_skipped_not_resident_o,
    output var logic [31:0] pw_descriptors_emitted_o,
    output var logic [31:0] pw_patches_unfresh_o,
    output var logic [31:0] pw_list_crc_mismatch_o,
    output var logic [31:0] pw_freeze_broken_o,
    output var logic [31:0] pw_pitch_illegal_o,
    output var logic [31:0] pw_jobs_refused_o,
    output var logic [31:0] pw_place_range_o,
    output var logic [31:0] pw_bridge_errs_o,
    output var logic [31:0] pw_sub_order_bad_o,
    output var logic [31:0] pw_store_wait_clocks_o,

    // ---- counters: LODSHARE -------------------------------------------------
    output var logic [31:0] ls_prep_descriptors_o,
    output var logic [31:0] ls_emit_descriptors_o,
    output var logic [31:0] ls_prep_decisions_o,
    output var logic [31:0] ls_emit_decisions_o,
    output var logic [31:0] ls_prep_underside_o,
    output var logic [31:0] ls_ident_mismatch_o,
    output var logic [31:0] ls_hist_leak_o,
    output var logic [31:0] ls_sel_midpatch_o,
    output var logic [31:0] ls_idq_overflow_o,
    output var logic [31:0] ls_idq_full_stalls_o,
    output var logic [31:0] ls_freeze_drift_o,
    output var logic [31:0] ls_freezes_o,

    // ---- counters: EDGEQUERY ------------------------------------------------
    output var logic [31:0] eq_patches_queued_o,
    output var logic [31:0] eq_door_refused_o,
    output var logic [31:0] eq_door_src_unknown_o,
    output var logic [31:0] eq_serve_no_door_o,
    output var logic [31:0] eq_serve_src_mismatch_o,
    output var logic [31:0] eq_queries_issued_o,
    output var logic [31:0] eq_queries_answered_o,
    output var logic [31:0] eq_edges_real_o,
    output var logic [31:0] eq_fallback_patches_o,
    output var logic [31:0] eq_query_abandoned_o,
    output var logic [31:0] eq_descriptor_unarmed_o,
    output var logic [31:0] eq_gate_wait_clocks_o,

    // ---- counters: PREPSHARE ------------------------------------------------
    output var logic [31:0] ps_a_dev_grants_o,
    output var logic [31:0] ps_b_dev_grants_o,
    output var logic [31:0] ps_a_dev_blocked_clocks_o,
    output var logic [31:0] ps_b_dev_blocked_clocks_o,
    output var logic [31:0] ps_a_lu_grants_o,
    output var logic [31:0] ps_b_lu_grants_o,
    output var logic [31:0] ps_lu_ans_unowned_o,
    output var logic [31:0] ps_dev_contended_o,

    // ---- counters: ISLANDSEAL -----------------------------------------------
    output var logic [31:0] isl_headers_checked_o,
    output var logic [31:0] isl_seals_o,
    output var logic [31:0] isl_reseals_o,
    output var logic [31:0] isl_pitch_illegal_o,
    output var logic [31:0] isl_pitch_mismatch_o,
    output var logic [31:0] isl_island_bounced_o,
    output var logic [31:0] isl_envelope_bad_o
);

  // =========================================================================
  // MODEL 1 -- THE HPS ARENA AND ITS BURST RESPONDER
  // =========================================================================
  // A byte source, not a law. It implements exactly what
  // `zhao_terrain_prepwalk` reads from `zhao_hps_bridge`: a grant, then
  // `len/8` beats with `last` on the final one. `hps_stall_i` withholds the
  // grant, which is the directive's backpressure on this socket.
  logic [63:0] arena [ARENA_W];
  always_ff @(posedge clk) if (aw_valid_i) arena[aw_word_i[$clog2(ARENA_W)-1:0]] <= aw_data_i;

  zhao_hps_burst_req_t pw_hps_req;
  zhao_hps_burst_rsp_t pw_hps_rsp;
  logic                pw_hps_grant;

  logic [31:0] hb_addr_q;
  logic [6:0]  hb_left_q;
  logic        hb_busy_q;

  always_comb begin
    pw_hps_grant       = pw_hps_req.valid && !hb_busy_q && !hps_stall_i;
    pw_hps_rsp         = '0;
    pw_hps_rsp.beat_valid = hb_busy_q;
    pw_hps_rsp.data    = arena[hb_addr_q[31:3] % ARENA_W];
    pw_hps_rsp.last    = hb_busy_q && (hb_left_q <= 7'd8);
    pw_hps_rsp.err     = 1'b0;
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      hb_addr_q <= '0;
      hb_left_q <= '0;
      hb_busy_q <= 1'b0;
    end else if (!hb_busy_q) begin
      if (pw_hps_grant) begin
        hb_addr_q <= pw_hps_req.addr;
        hb_left_q <= pw_hps_req.len;
        hb_busy_q <= 1'b1;
      end
    end else begin
      hb_addr_q <= hb_addr_q + 32'd8;
      if (hb_left_q <= 7'd8) hb_busy_q <= 1'b0;
      else                   hb_left_q <= hb_left_q - 7'd8;
    end
  end

  // =========================================================================
  // MODEL 2 -- TERRAIN.RESIDENCY's LOOKUP ANSWER
  // =========================================================================
  logic signed [15:0] res_ix [RES_N];
  logic signed [15:0] res_iz [RES_N];
  logic               res_hit[RES_N];
  logic [SLOTW-1:0]   res_slot[RES_N];
  logic [GENW-1:0]    res_gen [RES_N];
  always_ff @(posedge clk) if (rw_valid_i) begin
    res_ix  [rw_idx_i[$clog2(RES_N)-1:0]] <= rw_ix_i;
    res_iz  [rw_idx_i[$clog2(RES_N)-1:0]] <= rw_iz_i;
    res_hit [rw_idx_i[$clog2(RES_N)-1:0]] <= rw_hit_i;
    res_slot[rw_idx_i[$clog2(RES_N)-1:0]] <= rw_slot_i;
    res_gen [rw_idx_i[$clog2(RES_N)-1:0]] <= rw_gen_i;
  end

  logic               d_lu_valid, d_lu_ready;
  logic [31:0]        d_lu_epoch, d_lu_island;
  logic signed [15:0] d_lu_ix, d_lu_iz;
  logic               d_lu_ans_valid, d_lu_ans_hit;
  logic [SLOTW-1:0]   d_lu_ans_slot;
  logic [GENW-1:0]    d_lu_ans_gen;

  // TWO CYCLES, because the real directory answers from a pipeline and a
  // combinational model would hide every join fault a pipeline can produce.
  logic               lu_p1_v, lu_p2_v;
  logic signed [15:0] lu_p1_ix, lu_p1_iz;
  assign d_lu_ready = 1'b1;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      lu_p1_v <= 1'b0; lu_p2_v <= 1'b0;
      lu_p1_ix <= '0;  lu_p1_iz <= '0;
      d_lu_ans_valid <= 1'b0; d_lu_ans_hit <= 1'b0;
      d_lu_ans_slot <= '0;    d_lu_ans_gen <= '0;
    end else begin
      lu_p1_v  <= d_lu_valid && d_lu_ready;
      lu_p1_ix <= d_lu_ix;
      lu_p1_iz <= d_lu_iz;
      lu_p2_v  <= lu_p1_v;
      d_lu_ans_valid <= lu_p2_v;
      if (lu_p1_v) begin
        d_lu_ans_hit  <= 1'b0;
        d_lu_ans_slot <= '0;
        d_lu_ans_gen  <= '0;
        for (int k = 0; k < RES_N; k++) begin
          if ((32'(k) < rw_count_i) && res_hit[k] &&
              (res_ix[k] == lu_p1_ix) && (res_iz[k] == lu_p1_iz)) begin
            d_lu_ans_hit  <= 1'b1;
            d_lu_ans_slot <= res_slot[k];
            d_lu_ans_gen  <= res_gen[k];
          end
        end
      end
    end
  end

  // =========================================================================
  // MODEL 3 -- TERRAIN.DEVSTORE's READ PORT
  // =========================================================================
  logic [DEVW-1:0]   dv1[DEV_N][16], dv2[DEV_N][16], dv3[DEV_N][16];
  logic signed [15:0] dvcy[DEV_N][16];
  logic [1:0]        dvpl[DEV_N][16];
  logic [MORPHW-1:0] dvpm[DEV_N][16];
  logic [7:0]        dvho[DEV_N][16];
  logic              dvfr[DEV_N];
  always_ff @(posedge clk) if (dw_valid_i) begin
    dv1 [dw_slot_i[$clog2(DEV_N)-1:0]][dw_sp_i] <= dw_dev1_i;
    dv2 [dw_slot_i[$clog2(DEV_N)-1:0]][dw_sp_i] <= dw_dev2_i;
    dv3 [dw_slot_i[$clog2(DEV_N)-1:0]][dw_sp_i] <= dw_dev3_i;
    dvcy[dw_slot_i[$clog2(DEV_N)-1:0]][dw_sp_i] <= dw_cy_i;
    dvpl[dw_slot_i[$clog2(DEV_N)-1:0]][dw_sp_i] <= dw_prev_level_i;
    dvpm[dw_slot_i[$clog2(DEV_N)-1:0]][dw_sp_i] <= dw_prev_morph_i;
    dvho[dw_slot_i[$clog2(DEV_N)-1:0]][dw_sp_i] <= dw_hold_i;
    dvfr[dw_slot_i[$clog2(DEV_N)-1:0]]          <= dw_fresh_i;
  end

  logic             d_r_start, d_r_ready, d_r_valid, d_r_take;
  logic [SLOTW-1:0] d_r_slot;
  logic [3:0]       d_r_sp;

  typedef enum logic [1:0] { DS_IDLE, DS_WAIT, DS_STREAM } ds_e;
  ds_e              ds_q;
  logic [7:0]       ds_wait_q;
  logic [3:0]       ds_ptr_q;
  logic [SLOTW-1:0] ds_slot_q;

  assign d_r_ready = (ds_q == DS_IDLE);
  assign d_r_valid = (ds_q == DS_STREAM);
  assign d_r_sp    = ds_ptr_q;

  wire [$clog2(DEV_N)-1:0] ds_idx_c = ds_slot_q[$clog2(DEV_N)-1:0];

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      ds_q <= DS_IDLE; ds_wait_q <= '0; ds_ptr_q <= '0; ds_slot_q <= '0;
    end else begin
      case (ds_q)
        DS_IDLE: if (d_r_start) begin
          ds_slot_q <= d_r_slot;
          ds_ptr_q  <= 4'd0;
          ds_wait_q <= dev_latency_i;
          ds_q      <= (dev_latency_i == 8'd0) ? DS_STREAM : DS_WAIT;
        end
        DS_WAIT: if (ds_wait_q <= 8'd1) ds_q <= DS_STREAM;
                 else                   ds_wait_q <= ds_wait_q - 8'd1;
        default: if (d_r_valid && d_r_take) begin
          if (ds_ptr_q == 4'd15) ds_q <= DS_IDLE;
          else                   ds_ptr_q <= ds_ptr_q + 4'd1;
        end
      endcase
    end
  end

  // =========================================================================
  // THE PRODUCTION CHAIN
  // =========================================================================
  wire signed [7:0] isl_pitch;
  wire              isl_refuse;
  assign island_pitch_o  = isl_pitch;
  assign island_refuse_o = isl_refuse;

  zhao_terrain_islandseal #(.CW(32)) u_islandseal (
    .clk(clk), .rst_n(rst_n),
    .epoch_i(cfg_epoch_i),
    .hdr_valid_i(hdr_valid_i),
    .hdr_pitch_log2_i(hdr_pitch_log2_i),
    .hdr_island_i(hdr_island_i),
    .hdr_ix_i(hdr_ix_i),
    .hdr_iz_i(hdr_iz_i),
    .hdr_env_x0_i(hdr_env_x0_i),
    .hdr_env_z0_i(hdr_env_z0_i),
    .pitch_log2_o(isl_pitch),
    .island_o(),
    .sealed_o(),
    .refuse_o(isl_refuse),
    .headers_checked_o(isl_headers_checked_o),
    .seals_o(isl_seals_o),
    .reseals_o(isl_reseals_o),
    .pitch_illegal_o(isl_pitch_illegal_o),
    .pitch_mismatch_o(isl_pitch_mismatch_o),
    .island_bounced_o(isl_island_bounced_o),
    .envelope_bad_o(isl_envelope_bad_o)
  );

  // THE FREEZE WITNESS, exactly the console's: a counter moved by a residency
  // publication, a deformation mark, or an island-seal refusal.
  logic [31:0] tok_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n)                             tok_q <= 32'd0;
    else if (witness_bump_i || isl_refuse)  tok_q <= tok_q + 32'd1;
  end

  wire               pw_lu_valid, pw_lu_ready, pw_lu_ansv;
  wire [31:0]        pw_lu_epoch, pw_lu_island;
  wire signed [15:0] pw_lu_ix, pw_lu_iz;
  wire               pw_r_start, pw_r_ready, pw_r_valid, pw_r_take;
  wire [SLOTW-1:0]   pw_r_slot;
  wire               pw_sp_valid, pw_sp_ready;
  wire signed [31:0] pw_sp_cx, pw_sp_cy, pw_sp_cz;
  wire [DEVW-1:0]    pw_sp_dev1, pw_sp_dev2, pw_sp_dev3;
  wire [1:0]         pw_sp_prev_level;
  wire [MORPHW-1:0]  pw_sp_prev_morph;
  wire [7:0]         pw_sp_hold;
  wire [15:0]        pw_sp_src_id;
  wire signed [15:0] pw_sp_ix, pw_sp_iz;
  wire [GENW-1:0]    pw_sp_gen;
  wire               pw_prep_begin, pw_prep_done;
  wire [1:0]         er_phase;

  assign phase_o      = er_phase;
  assign prep_busy_o  = prep_sel_c;
  wire pw_busy;

  zhao_terrain_prepwalk #(
    .REC_BYTES(32), .BURST_BYTES(64), .MAX_PATCHES(1024),
    .SLOTW(SLOTW), .GENW(GENW), .DEVW(DEVW), .MORPHW(MORPHW),
    .SUBPATCHES(16), .SUB_EDGE(8), .CENTRE_OFF(4),
    .LAT_W(33), .LAT_H(33), .TOKW(32)
  ) u_prepwalk (
    .clk(clk), .rst_n(rst_n),
    .cfg_hps_client_i(ZHAO_CLIENT_TERRAIN_BUILD),
    .cfg_epoch_i(cfg_epoch_i),
    .cfg_arena_base_i(cfg_arena_base_i),
    .cfg_arena_bytes_i(cfg_arena_bytes_i),
    .j_valid_i(j_valid_i), .j_ready_o(j_ready_o),
    .j_epoch_i(j_epoch_i), .j_list_off_i(j_list_off_i),
    .j_list_bytes_i(j_list_bytes_i), .j_list_crc_i(j_list_crc_i),
    .j_patch_count_i(j_patch_count_i),
    .frz_pitch_log2_i(isl_pitch),
    .frz_tok_i(tok_q), .frz_tok_o(),
    .hps_req_o(pw_hps_req), .hps_req_grant_i(pw_hps_grant), .hps_rsp_i(pw_hps_rsp),
    .lu_valid_o(pw_lu_valid), .lu_ready_i(pw_lu_ready),
    .lu_epoch_o(pw_lu_epoch), .lu_island_o(pw_lu_island),
    .lu_ix_o(pw_lu_ix), .lu_iz_o(pw_lu_iz),
    .lu_ans_valid_i(pw_lu_ansv), .lu_ans_hit_i(d_lu_ans_hit),
    .lu_ans_slot_i(d_lu_ans_slot), .lu_ans_gen_i(d_lu_ans_gen),
    .r_start_o(pw_r_start), .r_slot_o(pw_r_slot),
    .r_ready_i(pw_r_ready), .r_valid_i(pw_r_valid), .r_ready_o(pw_r_take),
    .r_sp_i(d_r_sp),
    .r_dev1_i(dv1[ds_idx_c][d_r_sp]), .r_dev2_i(dv2[ds_idx_c][d_r_sp]),
    .r_dev3_i(dv3[ds_idx_c][d_r_sp]), .r_cy_i(dvcy[ds_idx_c][d_r_sp]),
    .r_prev_level_i(dvpl[ds_idx_c][d_r_sp]), .r_prev_morph_i(dvpm[ds_idx_c][d_r_sp]),
    .r_hold_i(dvho[ds_idx_c][d_r_sp]), .r_fresh_i(dvfr[ds_idx_c]),
    .sp_valid_o(pw_sp_valid), .sp_ready_i(pw_sp_ready),
    .sp_cx_o(pw_sp_cx), .sp_cy_o(pw_sp_cy), .sp_cz_o(pw_sp_cz),
    .sp_dev1_o(pw_sp_dev1), .sp_dev2_o(pw_sp_dev2), .sp_dev3_o(pw_sp_dev3),
    .sp_prev_level_o(pw_sp_prev_level), .sp_prev_morph_o(pw_sp_prev_morph),
    .sp_hold_o(pw_sp_hold), .sp_src_id_o(pw_sp_src_id),
    .sp_ix_o(pw_sp_ix), .sp_iz_o(pw_sp_iz), .sp_gen_o(pw_sp_gen), .sp_sub_o(),
    .prep_begin_o(pw_prep_begin), .prep_gate_i(er_phase == 2'd1),
    .prep_done_o(pw_prep_done),
    .prep_valid_o(prep_valid_o), .restart_req_o(restart_req_o),
    .busy_o(pw_busy), .idle_o(),
    .walks_started_o(pw_walks_started_o),
    .walks_completed_o(pw_walks_completed_o),
    .records_walked_o(pw_records_walked_o),
    .patches_prepared_o(pw_patches_prepared_o),
    .skipped_not_resident_o(pw_skipped_not_resident_o),
    .descriptors_emitted_o(pw_descriptors_emitted_o),
    .patches_unfresh_o(pw_patches_unfresh_o),
    .list_crc_mismatch_o(pw_list_crc_mismatch_o),
    .freeze_broken_o(pw_freeze_broken_o),
    .pitch_illegal_o(pw_pitch_illegal_o),
    .jobs_refused_o(pw_jobs_refused_o),
    .place_range_o(pw_place_range_o),
    .bridge_errs_o(pw_bridge_errs_o),
    .sub_order_bad_o(pw_sub_order_bad_o),
    .store_wait_clocks_o(pw_store_wait_clocks_o),
    .list_wait_clocks_o(), .list_bytes_read_o(), .list_refetch_bytes_o()
  );

  // THE SHARE. Requester A is the EMIT side -- driven here by a tiny model of
  // TERRAIN.SPDESC's read, so that CONTENTION is real and `dev_contended_o`
  // can move. B is the walker.
  logic             a_r_start_q;
  logic [SLOTW-1:0] a_r_slot_q;
  wire              a_r_ready, a_r_valid;
  // The EMIT model asks for a patch whenever a serve edge arrives and holds
  // until it is granted, which is `zhao_terrain_spdesc`'s own shape
  // (`r_start_o = (st_q == StStart)`, a pure state, never gated on ready).
  logic             serve_seen_q;
  logic [4:0]       a_beats_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      a_r_start_q <= 1'b0; a_r_slot_q <= '0; serve_seen_q <= 1'b0; a_beats_q <= '0;
    end else begin
      if (!serve_valid_i)                   serve_seen_q <= 1'b0;
      else if (serve_valid_i && !serve_seen_q) begin
        serve_seen_q <= 1'b1;
        a_r_start_q  <= 1'b1;
        a_r_slot_q   <= SLOTW'(serve_src_id_i);
      end
      if (a_r_start_q && a_r_ready) begin
        a_r_start_q <= 1'b0;
        a_beats_q   <= 5'd16;
      end
      if (a_r_valid && (a_beats_q != 5'd0)) a_beats_q <= a_beats_q - 5'd1;
    end
  end

  zhao_terrain_prepshare #(.SLOTW(SLOTW), .CW(32), .LAST_SP(15)) u_prepshare (
    .clk(clk), .rst_n(rst_n),
    .a_r_start_i(a_r_start_q), .a_r_slot_i(a_r_slot_q),
    .a_r_ready_o(a_r_ready), .a_r_valid_o(a_r_valid),
    .a_r_ready_i(a_beats_q != 5'd0),
    .b_r_start_i(pw_r_start), .b_r_slot_i(pw_r_slot),
    .b_r_ready_o(pw_r_ready), .b_r_valid_o(pw_r_valid), .b_r_ready_i(pw_r_take),
    .d_r_start_o(d_r_start), .d_r_slot_o(d_r_slot),
    .d_r_ready_i(d_r_ready), .d_r_valid_i(d_r_valid), .d_r_ready_o(d_r_take),
    .d_r_sp_i(d_r_sp),
    .a_lu_valid_i(1'b0), .a_lu_ready_o(),
    .a_lu_epoch_i(32'd0), .a_lu_island_i(32'd0),
    .a_lu_ix_i(16'sd0), .a_lu_iz_i(16'sd0), .a_lu_ans_valid_o(),
    .b_lu_valid_i(pw_lu_valid), .b_lu_ready_o(pw_lu_ready),
    .b_lu_epoch_i(pw_lu_epoch), .b_lu_island_i(pw_lu_island),
    .b_lu_ix_i(pw_lu_ix), .b_lu_iz_i(pw_lu_iz), .b_lu_ans_valid_o(pw_lu_ansv),
    .d_lu_valid_o(d_lu_valid), .d_lu_ready_i(d_lu_ready),
    .d_lu_epoch_o(d_lu_epoch), .d_lu_island_o(d_lu_island),
    .d_lu_ix_o(d_lu_ix), .d_lu_iz_o(d_lu_iz),
    .d_lu_ans_valid_i(d_lu_ans_valid),
    .a_dev_grants_o(ps_a_dev_grants_o), .b_dev_grants_o(ps_b_dev_grants_o),
    .a_dev_blocked_clocks_o(ps_a_dev_blocked_clocks_o),
    .b_dev_blocked_clocks_o(ps_b_dev_blocked_clocks_o),
    .a_lu_grants_o(ps_a_lu_grants_o), .b_lu_grants_o(ps_b_lu_grants_o),
    .a_lu_blocked_clocks_o(), .b_lu_blocked_clocks_o(),
    .lu_ans_unowned_o(ps_lu_ans_unowned_o),
    .dev_contended_o(ps_dev_contended_o),
    .busy_o()
  );

  // THE QUERY DRIVER, between the EMIT descriptor source and the time-share.
  wire               teq_sp_valid, teq_sp_ready;
  wire signed [31:0] teq_sp_cx, teq_sp_cy, teq_sp_cz;
  wire [DEVW-1:0]    teq_sp_dev1, teq_sp_dev2, teq_sp_dev3;
  wire [1:0]         teq_sp_prev_level;
  wire [MORPHW-1:0]  teq_sp_prev_morph;
  wire [7:0]         teq_sp_hold;
  wire [15:0]        teq_sp_src_id;
  wire               er_q_valid, er_q_ready, er_q_done;
  wire [15:0]        er_q_ix, er_q_iz;
  wire [7:0]         er_e_nz, er_e_pz, er_e_nx, er_e_px;
  wire [3:0]         er_e_real;

  zhao_terrain_edgequery #(
    .DEVW(DEVW), .MORPHW(MORPHW), .DOORD(4), .CW(32)
  ) u_edgequery (
    .clk(clk), .rst_n(rst_n),
    .rec_valid_i(rec_valid_i), .rec_ix_i(rec_ix_i), .rec_iz_i(rec_iz_i),
    .rec_src_id_i(rec_src_id_i),
    .door_valid_i(door_valid_i), .door_ready_o(), .door_src_id_i(door_src_id_i),
    .serve_valid_i(serve_valid_i), .serve_src_id_i(serve_src_id_i),
    .e_sp_valid_i(e_sp_valid_i), .e_sp_ready_o(e_sp_ready_o),
    .e_sp_cx_i(e_sp_cx_i), .e_sp_cy_i(e_sp_cy_i), .e_sp_cz_i(e_sp_cz_i),
    .e_sp_dev1_i(e_sp_dev1_i), .e_sp_dev2_i(e_sp_dev2_i), .e_sp_dev3_i(e_sp_dev3_i),
    .e_sp_prev_level_i(e_sp_prev_level_i), .e_sp_prev_morph_i(e_sp_prev_morph_i),
    .e_sp_hold_i(e_sp_hold_i), .e_sp_src_id_i(e_sp_src_id_i),
    .o_sp_valid_o(teq_sp_valid), .o_sp_ready_i(teq_sp_ready),
    .o_sp_cx_o(teq_sp_cx), .o_sp_cy_o(teq_sp_cy), .o_sp_cz_o(teq_sp_cz),
    .o_sp_dev1_o(teq_sp_dev1), .o_sp_dev2_o(teq_sp_dev2), .o_sp_dev3_o(teq_sp_dev3),
    .o_sp_prev_level_o(teq_sp_prev_level), .o_sp_prev_morph_o(teq_sp_prev_morph),
    .o_sp_hold_o(teq_sp_hold), .o_sp_src_id_o(teq_sp_src_id),
    .q_valid_o(er_q_valid), .q_ready_i(er_q_ready),
    .q_ix_o(er_q_ix), .q_iz_o(er_q_iz), .q_done_i(er_q_done),
    .q_edge_nz_i(er_e_nz), .q_edge_pz_i(er_e_pz),
    .q_edge_nx_i(er_e_nx), .q_edge_px_i(er_e_px),
    .q_edge_real_i(er_e_real),
    .bank_emit_i(er_phase == 2'd2), .prep_valid_i(prep_valid_o),
    .edge_nz_o(edge_nz_o), .edge_pz_o(edge_pz_o),
    .edge_nx_o(edge_nx_o), .edge_px_o(edge_px_o),
    .patches_queued_o(eq_patches_queued_o),
    .door_refused_o(eq_door_refused_o),
    .door_src_unknown_o(eq_door_src_unknown_o),
    .serve_no_door_o(eq_serve_no_door_o),
    .serve_src_mismatch_o(eq_serve_src_mismatch_o),
    .queries_issued_o(eq_queries_issued_o),
    .queries_answered_o(eq_queries_answered_o),
    .edges_real_o(eq_edges_real_o),
    .fallback_patches_o(eq_fallback_patches_o),
    .query_abandoned_o(eq_query_abandoned_o),
    .descriptor_unarmed_o(eq_descriptor_unarmed_o),
    .gate_wait_clocks_o(eq_gate_wait_clocks_o),
    .busy_o()
  );

  // THE TIME-SHARE and THE LADDER.
  wire signed [31:0] lod_cam0_x, lod_cam0_y, lod_cam0_z;
  wire signed [31:0] lod_cam1_x, lod_cam1_y, lod_cam1_z;
  wire [15:0]        lod_cam0_scale, lod_cam1_scale;
  wire               lod_cam0_en, lod_cam1_en;
  wire [15:0]        lod_hyst;
  wire [7:0]         lod_min_hold;
  wire [16:0]        lod_morph_step;
  wire               lod_sp_valid, lod_sp_ready;
  wire signed [31:0] lod_sp_cx, lod_sp_cy, lod_sp_cz;
  wire [DEVW-1:0]    lod_sp_dev1, lod_sp_dev2, lod_sp_dev3;
  wire [1:0]         lod_sp_prev_level;
  wire [MORPHW-1:0]  lod_sp_prev_morph;
  wire [7:0]         lod_sp_hold;
  wire [15:0]        lod_sp_src_id;
  wire               lod_out_valid, lod_out_ready;
  wire [5:0]         lod_out_ox, lod_out_oz;
  wire [1:0]         lod_out_level, lod_out_nz, lod_out_pz, lod_out_nx, lod_out_px;
  wire [16:0]        lod_out_morph;
  wire               lod_out_surface, lod_out_dual;
  wire [15:0]        lod_out_src_id;
  wire [7:0]         lod_out_hold;
  // THE DRAIN, the console's, verbatim: PREPARE owns the ladder until the last
  // descriptor it fed has been answered, and the bank freezes only then. See
  // `zhao_console_core.sv`'s note at `tprep_done_pend_q` for why `busy_o` and
  // `prep_done_o` are both one patch too early.
  wire               tls_idle;
  logic              prep_done_pend_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n)             prep_done_pend_q <= 1'b0;
    else if (pw_prep_done)  prep_done_pend_q <= 1'b1;
    else if (tls_idle)      prep_done_pend_q <= 1'b0;
  end
  wire prep_done_c = prep_done_pend_q && tls_idle;
  wire prep_sel_c  = pw_busy || !tls_idle;

  wire               f_valid, f_ready;
  wire [15:0]        f_ix, f_iz;
  wire [5:0]         f_ox, f_oz;
  wire [1:0]         f_level;
  wire               f_surface;

  zhao_terrain_lodshare #(
    .DEVW(DEVW), .MORPHW(MORPHW), .GENW(GENW), .IDQ_DEPTH(16)
  ) u_lodshare (
    .clk(clk), .rst_n(rst_n),
    .frame_i(frame_i), .prep_sel_i(prep_sel_c),
    .gv_cam0_x_i(gv_cam0_x_i), .gv_cam0_y_i(gv_cam0_y_i), .gv_cam0_z_i(gv_cam0_z_i),
    .gv_cam0_scale_i(gv_cam0_scale_i), .gv_cam0_en_i(gv_cam0_en_i),
    .gv_cam1_x_i(gv_cam1_x_i), .gv_cam1_y_i(gv_cam1_y_i), .gv_cam1_z_i(gv_cam1_z_i),
    .gv_cam1_scale_i(gv_cam1_scale_i), .gv_cam1_en_i(gv_cam1_en_i),
    .gv_hyst_i(gv_hyst_i), .gv_min_hold_i(gv_min_hold_i),
    .gv_morph_step_i(gv_morph_step_i),
    .lod_cam0_x_o(lod_cam0_x), .lod_cam0_y_o(lod_cam0_y), .lod_cam0_z_o(lod_cam0_z),
    .lod_cam0_scale_o(lod_cam0_scale), .lod_cam0_en_o(lod_cam0_en),
    .lod_cam1_x_o(lod_cam1_x), .lod_cam1_y_o(lod_cam1_y), .lod_cam1_z_o(lod_cam1_z),
    .lod_cam1_scale_o(lod_cam1_scale), .lod_cam1_en_o(lod_cam1_en),
    .lod_hyst_o(lod_hyst), .lod_min_hold_o(lod_min_hold),
    .lod_morph_step_o(lod_morph_step),
    .p_sp_valid_i(pw_sp_valid), .p_sp_ready_o(pw_sp_ready),
    .p_sp_cx_i(pw_sp_cx), .p_sp_cy_i(pw_sp_cy), .p_sp_cz_i(pw_sp_cz),
    .p_sp_dev1_i(pw_sp_dev1), .p_sp_dev2_i(pw_sp_dev2), .p_sp_dev3_i(pw_sp_dev3),
    .p_sp_prev_level_i(pw_sp_prev_level), .p_sp_prev_morph_i(pw_sp_prev_morph),
    .p_sp_hold_i(pw_sp_hold), .p_sp_src_id_i(pw_sp_src_id),
    .p_sp_ix_i(pw_sp_ix), .p_sp_iz_i(pw_sp_iz), .p_sp_gen_i(pw_sp_gen),
    .e_sp_valid_i(teq_sp_valid), .e_sp_ready_o(teq_sp_ready),
    .e_sp_cx_i(teq_sp_cx), .e_sp_cy_i(teq_sp_cy), .e_sp_cz_i(teq_sp_cz),
    .e_sp_dev1_i(teq_sp_dev1), .e_sp_dev2_i(teq_sp_dev2), .e_sp_dev3_i(teq_sp_dev3),
    .e_sp_prev_level_i(teq_sp_prev_level), .e_sp_prev_morph_i(teq_sp_prev_morph),
    .e_sp_hold_i(teq_sp_hold), .e_sp_src_id_i(teq_sp_src_id),
    .lod_sp_valid_o(lod_sp_valid), .lod_sp_ready_i(lod_sp_ready),
    .lod_sp_cx_o(lod_sp_cx), .lod_sp_cy_o(lod_sp_cy), .lod_sp_cz_o(lod_sp_cz),
    .lod_sp_dev1_o(lod_sp_dev1), .lod_sp_dev2_o(lod_sp_dev2), .lod_sp_dev3_o(lod_sp_dev3),
    .lod_sp_prev_level_o(lod_sp_prev_level), .lod_sp_prev_morph_o(lod_sp_prev_morph),
    .lod_sp_hold_o(lod_sp_hold), .lod_sp_src_id_o(lod_sp_src_id),
    .lod_out_valid_i(lod_out_valid), .lod_out_ready_o(lod_out_ready),
    .lod_out_ox_i(lod_out_ox), .lod_out_oz_i(lod_out_oz),
    .lod_out_level_i(lod_out_level),
    .lod_out_lvl_nz_i(lod_out_nz), .lod_out_lvl_pz_i(lod_out_pz),
    .lod_out_lvl_nx_i(lod_out_nx), .lod_out_lvl_px_i(lod_out_px),
    .lod_out_morph_i(lod_out_morph), .lod_out_surface_i(lod_out_surface),
    .lod_out_dual_i(lod_out_dual), .lod_out_src_id_i(lod_out_src_id),
    .lod_out_hold_i(lod_out_hold),
    .f_valid_o(f_valid), .f_ready_i(f_ready),
    .f_ix_o(f_ix), .f_iz_o(f_iz), .f_ox_o(f_ox), .f_oz_o(f_oz),
    .f_level_o(f_level), .f_surface_o(f_surface),
    .e_out_valid_o(e_out_valid_o), .e_out_ready_i(e_out_ready_i),
    .e_out_ox_o(e_out_ox_o), .e_out_oz_o(e_out_oz_o),
    .e_out_level_o(e_out_level_o),
    .e_out_lvl_nz_o(e_out_lvl_nz_o), .e_out_lvl_pz_o(e_out_lvl_pz_o),
    .e_out_lvl_nx_o(e_out_lvl_nx_o), .e_out_lvl_px_o(e_out_lvl_px_o),
    .e_out_morph_o(), .e_out_surface_o(e_out_surface_o),
    .e_out_dual_o(), .e_out_src_id_o(e_out_src_id_o), .e_out_hold_o(),
    .h_valid_i(h_valid_i), .h_ready_o(),
    .h_level_i(h_level_i), .h_morph_i(h_morph_i), .h_hold_i(h_hold_i),
    .h_valid_o(h_out_valid_o), .h_ready_i(1'b1),
    .h_level_o(h_out_level_o), .h_morph_o(), .h_hold_o(),
    .prep_descriptors_o(ls_prep_descriptors_o),
    .emit_descriptors_o(ls_emit_descriptors_o),
    .prep_decisions_o(ls_prep_decisions_o),
    .emit_decisions_o(ls_emit_decisions_o),
    .prep_underside_o(ls_prep_underside_o),
    .ident_mismatch_o(ls_ident_mismatch_o),
    .hist_leak_o(ls_hist_leak_o),
    .sel_midpatch_o(ls_sel_midpatch_o),
    .idq_overflow_o(ls_idq_overflow_o),
    .idq_full_stalls_o(ls_idq_full_stalls_o),
    .freeze_drift_o(ls_freeze_drift_o),
    .freezes_o(ls_freezes_o),
    .idle_o(tls_idle)
  );

  zhao_terrain_lod u_lod (
    .clk(clk), .rst_n(rst_n),
    .cam0_x_i(lod_cam0_x), .cam0_y_i(lod_cam0_y), .cam0_z_i(lod_cam0_z),
    .cam0_scale_i(lod_cam0_scale), .cam0_en_i(lod_cam0_en),
    .cam1_x_i(lod_cam1_x), .cam1_y_i(lod_cam1_y), .cam1_z_i(lod_cam1_z),
    .cam1_scale_i(lod_cam1_scale), .cam1_en_i(lod_cam1_en),
    .hyst_i(lod_hyst), .min_hold_i(lod_min_hold), .morph_step_i(lod_morph_step),
    .dual_i(dual_i),
    // THE PRODUCER, not a literal. This is the line entry I21 existed for.
    .edge_nz_i(edge_nz_o), .edge_pz_i(edge_pz_o),
    .edge_nx_i(edge_nx_o), .edge_px_i(edge_px_o),
    .sp_valid_i(lod_sp_valid), .sp_ready_o(lod_sp_ready),
    .sp_cx_i(lod_sp_cx), .sp_cy_i(lod_sp_cy), .sp_cz_i(lod_sp_cz),
    .sp_dev1_i(lod_sp_dev1), .sp_dev2_i(lod_sp_dev2), .sp_dev3_i(lod_sp_dev3),
    .sp_prev_level_i(lod_sp_prev_level), .sp_prev_morph_i(lod_sp_prev_morph),
    .sp_hold_i(lod_sp_hold), .sp_src_id_i(lod_sp_src_id),
    .out_valid_o(lod_out_valid), .out_ready_i(lod_out_ready),
    .out_ox_o(lod_out_ox), .out_oz_o(lod_out_oz),
    .out_level_o(lod_out_level),
    .out_lvl_nz_o(lod_out_nz), .out_lvl_pz_o(lod_out_pz),
    .out_lvl_nx_o(lod_out_nx), .out_lvl_px_o(lod_out_px),
    .out_morph_o(lod_out_morph), .out_surface_o(lod_out_surface),
    .out_dual_o(lod_out_dual), .out_src_id_o(lod_out_src_id),
    .out_hold_o(lod_out_hold),
    .lod_rep_count0_o(), .lod_rep_count1_o(),
    .lod_rep_count2_o(), .lod_rep_count3_o(),
    .terrain_triangles_emitted_o(), .idle_o()
  );

  zhao_terrain_edgerecon #(.IXW(4), .IZW(4)) u_edgerecon (
    .clk(clk), .rst_n(rst_n),
    .frame_begin_i(pw_prep_begin), .prepare_done_i(prep_done_c),
    .phase_o(er_phase), .frame_count_o(),
    .f_valid_i(f_valid), .f_ready_o(f_ready),
    .f_ix_i(f_ix), .f_iz_i(f_iz), .f_ox_i(f_ox), .f_oz_i(f_oz),
    .f_level_i(f_level), .f_surface_i(f_surface),
    .q_valid_i(er_q_valid), .q_ready_o(er_q_ready),
    .q_ix_i(er_q_ix), .q_iz_i(er_q_iz), .q_done_o(er_q_done),
    .edge_nz_o(er_e_nz), .edge_pz_o(er_e_pz),
    .edge_nx_o(er_e_nx), .edge_px_o(er_e_px), .edge_real_o(er_e_real),
    .records_filed_o(er_records_filed_o),
    .lanes_filed_o(er_lanes_filed_o),
    .collisions_o(er_collisions_o),
    .queries_o(er_queries_o),
    .edges_real_o(er_edges_real_o),
    .edges_fallback_o(er_edges_fallback_o),
    .query_own_missing_o(er_query_own_missing_o),
    .file_out_of_phase_o(er_file_out_of_phase_o),
    .query_out_of_phase_o(er_query_out_of_phase_o),
    .busy_o()
  );

  /* verilator lint_off UNUSEDSIGNAL */
  wire _unused_ok = &{1'b0, cfg_valid_i, d_lu_epoch, d_lu_island, 1'b0};
  /* verilator lint_on UNUSEDSIGNAL */

endmodule

/* verilator lint_on WIDTHEXPAND */
/* verilator lint_on UNUSEDSIGNAL */
`default_nettype wire
