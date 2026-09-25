// tb_terrain_composepub.sv -- A LIVE EARTH FIELD, ALL THE WAY TO A CONSUMER.
//
//   FIELD.SEQ.EARTH -> TERRAIN.PATCH -> TERRAIN.COMPCACHE -> TERRAIN.HEIGHTTAP
//   (zhao_field_earth_adapter)  (patch)   (compcache_front)   (the CONSUMER)
//
// FOUR REAL BLOCKS AND NO ADAPTER ANYWHERE IN THE CHAIN. Every seam below is
// wired PORT-FOR-PORT AS `zhao_console_core.sv` WIRES IT, so this is the
// composed seam under a microscope rather than a second arrangement of it.
//
// ---------------------------------------------------------------------------
// WHAT THIS CLOSES, AND IT IS THE PACKET'S WHOLE POINT
// ---------------------------------------------------------------------------
// `tests/terrain/tb_terrain_compose.sv` is the nearest existing bench and it
// says this about itself, in its own header:
//
//     "The field list is EMPTY. TERRAIN.PATCH's section 3.4 chain is
//      `live_top = max(compose_top + SUM field lanes, fx(bottom))`, and with no
//      ... same empty list. The field half needs FIELD.SEQ.EARTH, which is a
//      different [block]."
//
// and it carries that claim in its wiring: `.fld_add_valid_i(1'b0)`,
// `.fld_valid_i(1'b0)`, `.fld_height_i(32'sd0)` (`:410,443,445`).
//
// SO SECTION 3.4'S SUM HAS NEVER HAD A NON-EMPTY TERM IN ANY BENCH OF THE REAL
// CHAIN. The composed height that `zhao_console_core` computes every frame has
// never been shown, anywhere in this repository, to differ from the authored
// height because a field moved it -- and never been shown to REACH A CONSUMER
// in that state. Entry I34's note that section 3.4's sum "now has a non-empty
// sum" describes ARITHMETIC REACHING A VALUE. This bench is the first thing
// that watches that value arrive at something that reads it.
//
// The consumer is real and is the one the console ships:
// `zhao_terrain_heighttap` is TERRAIN.TAPSHARE's single service, and its two
// clients are PART.COLLIDE (`zhao_part_terrain_tap` -> `zhao_part_collide`)
// and FORGE.SHADOW (`zhao_forge_shadow.sv:131-137`). A height this block
// returns is a height a particle collides against and a shadow conforms to.
//
// ---------------------------------------------------------------------------
// WHY THE CONSUMER IS THE HEIGHTTAP AND NOT AN SDRAM REGION
// ---------------------------------------------------------------------------
// MEASURED 2026-09-25 (COMPOSEPUB), and it is the finding that shaped this
// bench. `spec/memory_rules.md` 5b ratifies `TERRAIN.COMPOSED_HEIGHT` at
// 0x0566_0000 and `TERRAIN.COMPOSED_VELOCITY` at 0x056F_0000. Across all 377
// SystemVerilog files of `fpga/rtl` those two names appear on TEN lines and
// every one of them is a COMMENT; `COMPOSED_HEIGHT_BASE`, `ZHAO_TERRAIN_COMPOSED*`
// and the literal `0x0566` return ZERO hits (control: `TERRAIN` returns 1,585
// in the same sweep of the same tree, so the search reached the files).
// `zhao_mem_guard` has exactly TWO bank-2 read arms -- `terrain_rd_ok` (the
// page pool) and `devstore_rd_ok` -- and no third is reachable.
//
// The composed lattice therefore does not travel through SDRAM today. It
// travels through `zhao_terrain_compcache_front`, which holds ONE patch in
// M10K and says why in its own header:
//
//     "The full 256-patch composed store is 256 x 2,178 B for heights plus as
//      much again for velocity = 8.92 Mbit = 161% of this device's entire
//      5.53 Mbit of M10K ... The SDRAM backing attaches later on the FILL side
//      without changing the serve ports."
//
// "Attaches later" is the unbuilt half, and a writer into it would today be
// read by nothing -- the directive's own "A DMA into unused memory is not a
// consumer". So this bench gates the path that IS live, and the packet's
// decision record (`spec/memory_rules.md` 5b, COMPOSEPUB) records why the
// SDRAM half is not opened ahead of its reader, on TERRAIN.DEVSTORE's
// precedent: "a window opened WITH its block, never ahead of it."
//
// ---------------------------------------------------------------------------
// WHAT IS HARNESS HERE, DECLARED RATHER THAN HIDDEN
// ---------------------------------------------------------------------------
// TWO boundaries are driven by the C++ driver, and both are boundaries of a
// SUBSYSTEM PROVEN ELSEWHERE rather than a hole in this chain:
//
//   THE FIELD ENGINE (`req_*` / `resp_*`). The adapter is client 3 of the one
//   shared field engine. The engine is FIELD.HOST's subsystem with its own
//   proof (`tests/field/field_host_v2_directed.cpp`), and the adapter's own
//   seam against it is `tests/field/field_earth_adapter_directed.cpp`. Driving
//   it here lets a case place an EXACT out-lane value and an EXACT presence
//   mask, which is what makes "absent optional output is no write, not a write
//   of zero" testable at all.
//
//   THE PAGE LATTICE (`base_i` / `scar_i` / `bottom_i` / `wx_i` / `wz_i`).
//   `tb_terrain_compose.sv` already gates TERRAIN.PAGESTREAM -> TERRAIN.PATCH
//   on real page bytes. Re-streaming a page here would re-gate that seam and
//   not this one; this bench's subject is what the FIELD does to the composed
//   height and whether the CONSUMER sees it, so the authored lattice is a
//   declared constant the driver chooses per case.
//
// Neither boundary is a live console path replaced by stimulus: in
// `zhao_console_core` the engine side goes to FIELD.HOST and the lattice side
// to TERRAIN.PAGESTREAM, and both of those seams are gated by their own benches.
//
// ENFORCED-BY: tests/terrain/composepub_acceptance.cpp:main
// ---------------------------------------------------------------------------
`default_nettype none

module tb_terrain_composepub #(
    parameter int unsigned LAT_W      = 33,
    parameter int unsigned LAT_H      = 33,
    parameter int unsigned MAX_FIELDS = 16,
    parameter int unsigned OBJW       = 3,
    parameter int unsigned SLOTW      = 3,
    parameter int unsigned IN_LANES   = 15,
    parameter int unsigned OUT_LANES  = 7
) (
    input var logic clk,
    input var logic rst_n,

    // ---- the frame clock the adapter ages programs against -----------------
    input var logic [31:0] tick_i,

    // ---- CMD.EXEC's uniform records, into the adapter ----------------------
    input  var logic         rec_valid_i,
    output var logic         rec_ready_o,
    input  var logic [ 31:0] rec_start_tick_i,
    input  var logic [ 31:0] rec_duration_i,
    input  var logic [255:0] rec_params_i,
    input  var logic         rec_last_i,

    // ---- TERRAIN.FIELDLIST's per-patch replay, into PATCH's section 9.1 list
    // `add_fire_i` on the adapter is this handshake, exactly as the core wires
    // it: the adapter WATCHES the list and never backpressures it.
    input  var logic               list_clear_i,
    input  var logic [15:0]        patch_id_i,
    input  var logic               patch_open_i,
    input  var logic               fld_add_valid_i,
    output var logic               fld_add_ready_o,
    input  var logic signed [31:0] fld_add_x0_i,
    input  var logic signed [31:0] fld_add_z0_i,
    input  var logic signed [31:0] fld_add_x1_i,
    input  var logic signed [31:0] fld_add_z1_i,
    input  var logic        [31:0] fld_add_hash_i,
    input  var logic        [15:0] fld_add_cmd_i,
    input  var logic [OBJW-1:0]    add_obj_i,
    input  var logic               add_resident_i,
    output var logic               fld_add_accept_o,
    output var logic               fld_add_reject_o,
    output var logic [4:0]         fields_active_o,

    // ---- the authored lattice, into PATCH's compose lane -------------------
    input  var logic               vtx_valid_i,
    output var logic               vtx_ready_o,
    input  var logic signed [15:0] base_i,
    input  var logic signed [15:0] scar_i,
    input  var logic signed [15:0] bottom_i,
    input  var logic               dual_i,
    input  var logic signed [31:0] wx_i,
    input  var logic signed [31:0] wz_i,
    input  var logic        [ 5:0] vi_i,
    input  var logic        [ 5:0] vj_i,
    input  var logic        [15:0] src_id_i,

    // ---- the one field engine (FIELD.HOST's subsystem; driver-modelled) ----
    output var logic                   req_valid_o,
    input  var logic                   req_ready_i,
    output var logic [SLOTW-1:0]       req_slot_o,
    output var logic                   req_noprog_o,
    output var logic [IN_LANES*32-1:0] req_in_o,
    input  var logic                   resp_valid_i,
    output var logic                   resp_ready_o,
    input  var logic [OUT_LANES*32-1:0] resp_out_i,
    input  var logic [OUT_LANES-1:0]   resp_present_i,
    input  var logic [7:0]             resp_status_i,

    // ---- COMPCACHE's fill control and placement plane ----------------------
    input  var logic               fill_start_i,
    output var logic               fill_accept_o,
    output var logic               fill_busy_o,
    output var logic               fill_done_o,
    input  var logic               pos_we_i,
    input  var logic               pos_axis_i,
    input  var logic        [ 5:0] pos_idx_i,
    input  var logic signed [31:0] pos_val_i,
    input  var logic               cs_we_i,
    input  var logic        [ 4:0] cs_w_ci_i,
    input  var logic        [ 4:0] cs_w_cj_i,
    input  var logic        [ 1:0] cs_w_substance_i,
    input  var logic               serve_release_i,
    output var logic               serve_valid_o,
    output var logic [15:0]        serve_src_id_o,

    // ---- TERRAIN.TESS's owner port on the tap's pass-through ---------------
    // Held quiet by default. Raised by the stall case, which is the ONLY way
    // `tap_stall_clocks_o` can move: TESS's request wins unconditionally.
    input  var logic        o_lat_req_i,
    input  var logic [ 5:0] o_lat_vi_i,
    input  var logic [ 5:0] o_lat_vj_i,
    input  var logic        o_lat_surface_i,
    output var logic signed [31:0] o_lat_h_o,

    // ---- THE CONSUMER'S REQUEST PORT ---------------------------------------
    input  var logic signed [7:0]  pitch_log2_i,
    input  var logic               req_tap_valid_i,
    output var logic               req_tap_ready_o,
    input  var logic signed [31:0] req_tap_x_i,
    input  var logic signed [31:0] req_tap_z_i,
    input  var logic               req_tap_surface_i,
    output var logic               rsp_tap_valid_o,
    output var logic signed [31:0] rsp_tap_height_o,
    output var logic               rsp_tap_no_ground_o,
    output var logic signed [31:0] rsp_tap_ny_o,

    // ---- what the acceptance reads -----------------------------------------
    // PATCH's two heights side by side. `compose_top_o` is pre-field and
    // post-clamp; `top_o` is section 3.4's live_top. Differencing THEM is how a
    // case proves a field contributed, and differencing the TAP against `top_o`
    // is how it proves the contribution survived to the consumer.
    output var logic               st_valid_o,
    // THE CACHE'S OWN ACCEPT. Exported because the fill port is NOT one record
    // per clock -- "the cache accepts on alternate clocks: one record is two
    // writes" (zhao_console_core.sv) -- so a driver that captured the stream on
    // `st_valid_o` alone would record every record TWICE and hand a lattice of
    // 2,177 entries to an oracle expecting 1,089. It did, on this bench's first
    // run, which is why this port exists.
    output var logic               st_ready_o,
    output var logic signed [31:0] st_top_o,
    output var logic signed [31:0] st_compose_top_o,
    output var logic               st_dirty_o,
    output var logic [15:0]        st_src_id_o,
    output var logic [15:0]        subpatch_dirty_o,

    output var logic               efa_ans_valid_o,
    output var logic               efa_ans_ready_o,
    output var logic signed [31:0] efa_height_o,
    output var logic signed [31:0] efa_velocity_o,
    output var logic [31:0]        efa_material_o,
    output var logic signed [31:0] efa_nav_cost_o,
    output var logic [3:0]         efa_present_o,
    output var logic               efa_ans_field_o,
    output var logic               fld_covers_o,

    // adapter censuses
    output var logic [31:0] efa_records_o,
    output var logic [31:0] efa_tail_rejected_o,
    output var logic [31:0] efa_runs_o,
    output var logic [31:0] efa_skipped_uncovered_o,
    output var logic [31:0] efa_not_begun_o,
    output var logic [31:0] efa_noprog_o,
    output var logic [31:0] efa_faults_o,
    output var logic [31:0] efa_short_record_o,
    output var logic [31:0] efa_lane_desync_o,
    output var logic [31:0] efa_stall_cycles_o,

    // compcache censuses
    output var logic [31:0] cc_fill_records_o,
    output var logic [31:0] cc_patches_filled_o,
    output var logic [31:0] cc_patches_served_o,
    output var logic [31:0] cc_fill_overrun_o,
    output var logic [31:0] cc_lat_oob_o,

    // THE CONSUMER'S censuses. Refusals that are CORRECT are counted apart
    // from FAULTS, which is the tap's own law and this bench's subject at
    // case 7: an off-patch tap is not a defect and must not read as one.
    output var logic [31:0] taps_answered_o,
    output var logic [31:0] taps_void_o,
    output var logic [31:0] taps_off_patch_o,
    output var logic [31:0] place_mismatch_o,
    output var logic [31:0] pitch_bad_o,
    output var logic [31:0] interp_overflow_o,
    output var logic [31:0] tap_stall_clocks_o,

    output var logic patch_idle_o,
    output var logic efa_idle_o
);

  // ==========================================================================
  // the three internal seams, named so the wiring below reads as the core's
  // ==========================================================================
  logic               efa_ans_valid, efa_ans_ready;
  logic signed [31:0] efa_height;

  logic               pt_st_valid, pt_st_ready;
  logic signed [31:0] pt_top, pt_bottom;

  logic               t_c_lat_req, t_c_lat_surface;
  logic        [ 5:0] t_c_lat_vi, t_c_lat_vj;
  logic signed [31:0] c_lat_h, c_lat_wx, c_lat_wz;

  logic       t_c_cs_req;
  logic [4:0] t_c_cs_ci, t_c_cs_cj;
  logic [1:0] c_cs_substance;

  // ==========================================================================
  // FIELD.SEQ.EARTH -- the adapter, parameterised EXACTLY as the core does it
  // (`zhao_console_core.sv:22846-22861`), including the host's 15/7 arity
  // rather than the earth record's own 12/4.
  // ==========================================================================
  zhao_field_earth_adapter #(
      .MAX_FIELDS(MAX_FIELDS),
      .OBJW      (OBJW),
      .SLOTW     (SLOTW),
      .IN_LANES  (IN_LANES),
      .OUT_LANES (OUT_LANES)
  ) u_efa (
      .clk  (clk),
      .rst_n(rst_n),
      .tick_i(tick_i),

      .rec_valid_i     (rec_valid_i),
      .rec_ready_o     (rec_ready_o),
      .rec_start_tick_i(rec_start_tick_i),
      .rec_duration_i  (rec_duration_i),
      .rec_params_i    (rec_params_i),
      .rec_last_i      (rec_last_i),

      // the list replay, WATCHED -- the handshake itself, as the core wires it
      .patch_open_i  (patch_open_i),
      .add_fire_i    (fld_add_valid_i && fld_add_ready_o),
      .add_obj_i     (add_obj_i),
      .add_resident_i(add_resident_i),

      // the consumer's own vertex accept and the two coordinates on it
      .vtx_fire_i(vtx_valid_i && vtx_ready_o),
      .vtx_wx_i  (wx_i),
      .vtx_wz_i  (wz_i),
      .lanes_i   (fields_active_o),

      // section 9.1 decided ONCE, by the block that owns the list
      .lane_covers_i(fld_covers_o),

      .req_valid_o  (req_valid_o),
      .req_ready_i  (req_ready_i),
      .req_slot_o   (req_slot_o),
      .req_noprog_o (req_noprog_o),
      .req_in_o     (req_in_o),
      .resp_valid_i (resp_valid_i),
      .resp_ready_o (resp_ready_o),
      .resp_out_i   (resp_out_i),
      .resp_present_i(resp_present_i),
      .resp_status_i(resp_status_i),

      .ans_valid_o(efa_ans_valid),
      .ans_ready_i(efa_ans_ready),
      .height_o   (efa_height),

      // The three lanes that DEAD-END in `zhao_console_core` today. They are
      // exported here rather than left open because this bench is where a
      // later packet's material/nav reducer gets its first observation, and
      // because case 4's "present zero" needs `efa_present_o` beside them.
      .velocity_o (efa_velocity_o),
      .material_o (efa_material_o),
      .nav_cost_o (efa_nav_cost_o),
      .ans_field_o(efa_ans_field_o),
      .ans_present_o(efa_present_o),

      .records_o          (efa_records_o),
      .tail_rejected_o    (efa_tail_rejected_o),
      .runs_o             (efa_runs_o),
      .skipped_uncovered_o(efa_skipped_uncovered_o),
      .not_begun_o        (efa_not_begun_o),
      .noprog_o           (efa_noprog_o),
      .faults_o           (efa_faults_o),
      .short_record_o     (efa_short_record_o),
      .lane_desync_o      (efa_lane_desync_o),
      .stall_cycles_o     (efa_stall_cycles_o),
      .idle_o             (efa_idle_o)
  );

  assign efa_ans_valid_o = efa_ans_valid;
  assign efa_ans_ready_o = efa_ans_ready;
  assign efa_height_o    = efa_height;

  // ==========================================================================
  // TERRAIN.PATCH -- section 3.4's composition, command-ordered and saturating
  // ==========================================================================
  zhao_terrain_patch u_pt (
      .clk  (clk),
      .rst_n(rst_n),

      .list_clear_i(list_clear_i),
      .patch_id_i  (patch_id_i),

      .fld_add_valid_i(fld_add_valid_i),
      .fld_add_ready_o(fld_add_ready_o),
      .fld_add_x0_i   (fld_add_x0_i),
      .fld_add_z0_i   (fld_add_z0_i),
      .fld_add_x1_i   (fld_add_x1_i),
      .fld_add_z1_i   (fld_add_z1_i),
      .fld_add_hash_i (fld_add_hash_i),
      .fld_add_cmd_i  (fld_add_cmd_i),

      .fld_add_accept_o(fld_add_accept_o),
      .fld_add_reject_o(fld_add_reject_o),
      .fields_active_o (fields_active_o),

      .trace_patch_id_o   (),
      .trace_hash_o       (),
      .trace_cmd_o        (),
      .programs_rejected_o(),

      .vtx_valid_i(vtx_valid_i),
      .vtx_ready_o(vtx_ready_o),
      .base_i     (base_i),
      .scar_i     (scar_i),
      .bottom_i   (bottom_i),
      .dual_i     (dual_i),
      .wx_i       (wx_i),
      .wz_i       (wz_i),
      .vi_i       (vi_i),
      .vj_i       (vj_i),
      .src_id_i   (src_id_i),

      // THE LANE THIS BENCH EXISTS FOR. `tb_terrain_compose.sv` ties these
      // three to 0/0/0; here they are the adapter's real answer.
      .fld_valid_i (efa_ans_valid),
      .fld_ready_o (efa_ans_ready),
      .fld_height_i(efa_height),
      .fld_covers_o(fld_covers_o),

      .st_valid_o     (pt_st_valid),
      .st_ready_i     (pt_st_ready),
      .top_o          (pt_top),
      .bottom_o       (pt_bottom),
      .compose_top_o  (st_compose_top_o),
      .st_dirty_o     (st_dirty_o),
      .st_src_id_o    (st_src_id_o),
      .subpatch_dirty_o(subpatch_dirty_o),

      .terrain_samples_evaluated_o(),
      .idle_o                     (patch_idle_o)
  );

  assign st_valid_o = pt_st_valid;
  assign st_ready_o = pt_st_ready;
  assign st_top_o   = pt_top;

  // ==========================================================================
  // TERRAIN.COMPCACHE -- the on-chip front. PATCH's `st_*` IS its fill port.
  // ==========================================================================
  zhao_terrain_compcache_front #(
      .LAT_W(LAT_W),
      .LAT_H(LAT_H)
  ) u_cc (
      .clk  (clk),
      .rst_n(rst_n),

      .fill_start_i (fill_start_i),
      .fill_accept_o(fill_accept_o),
      .fill_busy_o  (fill_busy_o),

      .st_valid_i (pt_st_valid),
      .st_ready_o (pt_st_ready),
      .st_top_i   (pt_top),
      .st_bottom_i(pt_bottom),
      .st_src_id_i(st_src_id_o),

      .pos_we_i  (pos_we_i),
      .pos_axis_i(pos_axis_i),
      .pos_idx_i (pos_idx_i),
      .pos_val_i (pos_val_i),

      .cs_we_i         (cs_we_i),
      .cs_w_ci_i       (cs_w_ci_i),
      .cs_w_cj_i       (cs_w_cj_i),
      .cs_w_substance_i(cs_w_substance_i),

      // the layer-E material plane is TEXMAT's seam, not this bench's subject
      .mat_we_i      (1'b0),
      .mat_w_ci_i    (5'd0),
      .mat_w_cj_i    (5'd0),
      .mat_w_a_i     (8'd0),
      .mat_w_b_i     (8'd0),
      .mat_w_weight_i(8'd0),

      .dual_i(dual_i),

      .fill_done_o(fill_done_o),

      .serve_release_i(serve_release_i),
      .serve_valid_o  (serve_valid_o),
      .serve_src_id_o (serve_src_id_o),

      // THE SERVE SIDE IS THE TAP'S CACHE SIDE, port for port
      .lat_req_i    (t_c_lat_req),
      .lat_vi_i     (t_c_lat_vi),
      .lat_vj_i     (t_c_lat_vj),
      .lat_surface_i(t_c_lat_surface),
      .lat_h_o      (c_lat_h),
      .lat_wx_o     (c_lat_wx),
      .lat_wz_o     (c_lat_wz),

      .cs_req_i     (t_c_cs_req),
      .cs_ci_i      (t_c_cs_ci),
      .cs_cj_i      (t_c_cs_cj),
      .cs_substance_o(c_cs_substance),

      .mat_req_i  (1'b0),
      .mat_ci_i   (5'd0),
      .mat_cj_i   (5'd0),
      .mat_a_o    (),
      .mat_b_o    (),
      .mat_weight_o(),
      .mat_valid_o(),

      .fill_records_o  (cc_fill_records_o),
      .patches_filled_o(cc_patches_filled_o),
      .patches_served_o(cc_patches_served_o),
      .fill_overrun_o  (cc_fill_overrun_o),
      .lat_oob_o       (cc_lat_oob_o),
      .cs_oob_o        (),
      .mat_oob_o       (),
      .mat_cells_o     ()
  );

  // ==========================================================================
  // TERRAIN.HEIGHTTAP -- THE CONSUMER. It sits IN FRONT of the compose cache's
  // lattice port as a pass-through and injects only on cycles TERRAIN.TESS did
  // not want, which is why `o_lat_*` is wired through rather than tied off.
  // ==========================================================================
  zhao_terrain_heighttap #(
      .LAT_W(LAT_W),
      .LAT_H(LAT_H)
  ) u_ht (
      .clk  (clk),
      .rst_n(rst_n),

      .pitch_log2_i(pitch_log2_i),

      .req_valid_i  (req_tap_valid_i),
      .req_ready_o  (req_tap_ready_o),
      .req_x_i      (req_tap_x_i),
      .req_z_i      (req_tap_z_i),
      .req_surface_i(req_tap_surface_i),

      .rsp_valid_o    (rsp_tap_valid_o),
      .rsp_height_o   (rsp_tap_height_o),
      .rsp_no_ground_o(rsp_tap_no_ground_o),
      .rsp_nx_o       (),
      .rsp_ny_o       (rsp_tap_ny_o),
      .rsp_nz_o       (),
      .rsp_h00_o      (),
      .rsp_h10_o      (),
      .rsp_h01_o      (),
      .rsp_h11_o      (),
      .rsp_wx00_o     (),
      .rsp_wz00_o     (),
      .rsp_sh_o       (),
      .rsp_na_x_o     (),
      .rsp_na_y_o     (),
      .rsp_na_z_o     (),
      .rsp_nb_x_o     (),
      .rsp_nb_y_o     (),
      .rsp_nb_z_o     (),

      // TERRAIN.TESS's side of the pass-through
      .o_lat_req_i    (o_lat_req_i),
      .o_lat_vi_i     (o_lat_vi_i),
      .o_lat_vj_i     (o_lat_vj_i),
      .o_lat_surface_i(o_lat_surface_i),
      .o_lat_h_o      (o_lat_h_o),
      .o_lat_wx_o     (),
      .o_lat_wz_o     (),
      .o_cs_req_i     (1'b0),
      .o_cs_ci_i      (5'd0),
      .o_cs_cj_i      (5'd0),
      .o_cs_substance_o(),

      // the compose cache's side
      .c_lat_req_o    (t_c_lat_req),
      .c_lat_vi_o     (t_c_lat_vi),
      .c_lat_vj_o     (t_c_lat_vj),
      .c_lat_surface_o(t_c_lat_surface),
      .c_lat_h_i      (c_lat_h),
      .c_lat_wx_i     (c_lat_wx),
      .c_lat_wz_i     (c_lat_wz),
      .c_cs_req_o     (t_c_cs_req),
      .c_cs_ci_o      (t_c_cs_ci),
      .c_cs_cj_o      (t_c_cs_cj),
      .c_cs_substance_i(c_cs_substance),

      .taps_answered_o   (taps_answered_o),
      .taps_void_o       (taps_void_o),
      .taps_off_patch_o  (taps_off_patch_o),
      .place_mismatch_o  (place_mismatch_o),
      .pitch_bad_o       (pitch_bad_o),
      .interp_overflow_o (interp_overflow_o),
      .tap_stall_clocks_o(tap_stall_clocks_o),
      .normal_sats_o     ()
  );

endmodule

`default_nettype wire
