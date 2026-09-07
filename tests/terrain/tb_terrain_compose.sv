// tb_terrain_compose.sv -- a page, all the way to triangles.
//
//   TERRAIN.PAGESTREAM -> TERRAIN.PATCH -> TERRAIN.COMPCACHE -> TERRAIN.TESS
//
// Four real blocks and NO ADAPTER ANYWHERE IN THE CHAIN. Each seam is
// port-for-port: PATCH's compose lane takes the three planes the streamer emits
// on one beat; COMPCACHE's fill port is PATCH's `st_*` output; TESS's `lat_*`
// and `cs_*` ports are COMPCACHE's serve side, one-cycle read latency included.
// That the four fit together with nothing between them is the claim this bench
// exists to make, and it is a claim four green differentials cannot make.
//
// ---------------------------------------------------------------------------
// WHAT THIS CLOSES
// ---------------------------------------------------------------------------
// `tests/terrain/tb_terrain_world.sv`'s header has said since it was written:
//
//     NOT composed, and the reason is a MISSING BLOCK rather than a choice:
//     TERRAIN.SEQ's `is_*` patch issue cannot reach TERRAIN.PATCH, because
//     nothing in the tree turns a resident page slot into TERRAIN.PATCH's
//     `vtx_*` lattice intake ... the chain PATCH -> COMPCACHE -> LOD -> TESS
//     -> NORMALS is reachable only from a harness on BOTH ends, which is
//     decoration rather than composition.
//
// The missing block landed on 2026-09-07. `zhao_terrain_pagestream` reads
// layers A, B and C out of a pool slot and emits exactly the three planes
// TERRAIN.PATCH's compose lane takes, one vertex at a time, on the same beat.
// So the two go together with no glue at all on the height path, and this bench
// is the first place §3.4's composition runs on REAL PAGE BYTES rather than on
// a lattice a harness invented.
//
// ---------------------------------------------------------------------------
// WHAT IS STILL HARNESS, AND WHY IT IS NOT PRETENDING OTHERWISE
// ---------------------------------------------------------------------------
// TWO of TERRAIN.PATCH's inputs do not come from the page and are driven here:
//
//   `wx_i` / `wz_i`  the vertex's PLACED world position. It comes from the
//                    island directory and the patch envelope -- not from
//                    layers A, B or C -- so the streamer has no business
//                    producing it and does not. The bench places the lattice on
//                    a declared grid and the test knows the same law.
//
//   `dual_i`         NO LONGER HARNESS, as of 2026-09-07. It is a flag on T5's
//                    patch record (`kFlagDual`), and nothing in the tree routed
//                    it to the compose lane -- this bench tied it to a knob and
//                    the gap was reported as a finding. TERRAIN.PAGESTREAM now
//                    carries the record's whole 16-bit `flags` the same way it
//                    carries slot, generation, epoch and source id: as IDENTITY
//                    that rides the stream, whole and uninterpreted. The bench
//                    sets the flag in the JOB and the routing is what is under
//                    test.
//
// ---------------------------------------------------------------------------
// AND THE THIRD BLOCK: TERRAIN.COMPCACHE
// ---------------------------------------------------------------------------
// `zhao_terrain_compcache_front`'s fill port is port-for-port TERRAIN.PATCH's
// `st_*` output, so it goes on the end with no glue either. That makes this
// bench PAGESTREAM -> PATCH -> COMPCACHE: a page's bytes, composed by §3.4, and
// held in the double-buffered lattice the tessellator reads.
//
// The cache is where the claim gets its second half. PATCH's output stream can
// be checked as it goes past; the cache can only be checked by READING IT BACK
// through the serve port afterwards, which is a different question -- did the
// right value land at the right (vi, vj) -- and it is the one a wrong write
// cursor, a wrong buffer parity or a wrong row stride would fail while every
// counter agreed.
//
// The placement goes in through `pos_we_i` rather than on the record stream,
// because it is the ISLAND DIRECTORY's, not the composition's, and this block's
// own contract makes that split. The bench writes the same 33 column x's and 33
// row z's it tells TERRAIN.PATCH about, and the readback checks the cache
// returns them -- so a bench that wrote one law and expected another would fail
// rather than agree with itself.

// The field list is EMPTY. TERRAIN.PATCH's §3.4 chain is
// `live_top = max(compose_top + SUM field lanes, fx(bottom))`, and with no
// accepted programs there are no lanes -- so this bench measures the
// `compose_top` half exactly, against `zref::terrain::compose_vertex` with the
// same empty list. The field half needs FIELD.SEQ.EARTH, which is a different
// lane.
`default_nettype none

module tb_terrain_compose
  import zhao_pkg::*;
(
    input var logic clk,
    input var logic rst_n,

    // ---- the page-pool image ----------------------------------------------
    input  var logic        mw_en,
    input  var logic [13:0] mw_addr,
    input  var logic [63:0] mw_data,

    input var logic [31:0] cfg_vram_window_base_i,
    input var logic [7:0]  cfg_grant_hold_i,
    input var logic [7:0]  cfg_rd_latency_i,
    input var logic [7:0]  cfg_rd_gap_i,

    // ---- DUT configuration -------------------------------------------------
    input var logic [2:0]  cfg_vram_client_i,
    input var logic [31:0] cfg_epoch_i,
    // THE PATCH RECORD'S FLAGS, driven as a JOB FIELD rather than as a pin on
    // TERRAIN.PATCH. That is the point of the phase that uses it: the bench
    // sets `kFlagDual` in the record and the ROUTING is what is under test --
    // streamer captures it at job start, carries it on every vertex, and the
    // compose lane reads bit 3. A bench that tied `dual_i` directly would test
    // TERRAIN.PATCH's clamp and nothing about whether the flag ever arrives.
    input var logic [15:0] j_flags,

    // The lattice's placement, which is the island directory's business and not
    // the streamer's. `wx = x0 + vi * step`, `wz = z0 + vj * step`, all fx16 --
    // vi is the COLUMN and vj the ROW, this tree's convention throughout.
    input var logic signed [31:0] cfg_x0_i,
    input var logic signed [31:0] cfg_z0_i,
    input var logic signed [31:0] cfg_step_i,

    // ---- the job -----------------------------------------------------------
    // TERRAIN.PATCH's subpatch dirty mask ACCUMULATES until this clears it, and
    // its contract says so: "Empties the per-patch list and the subpatch dirty
    // mask. Asserted once per patch per frame, BEFORE the first record." It is
    // driven from the C++ side rather than off `v_first_o`, because "before the
    // first record" is a cycle the streamer has no way to name.
    input  var logic        pt_list_clear,

    input  var logic        j_valid,
    output var logic        j_ready,
    input  var logic [10:0] j_slot,
    input  var logic [ 7:0] j_gen,
    input  var logic [31:0] j_epoch,
    input  var logic [31:0] j_src_id,

    // ---- the composed vertex out -------------------------------------------
    output var logic        st_valid,
    input  var logic        st_ready,
    output var logic [31:0] st_top,
    output var logic [31:0] st_bottom,
    output var logic [31:0] st_compose_top,
    output var logic        st_dirty,
    output var logic [15:0] st_src_id,
    output var logic [15:0] subpatch_dirty,

    // what the STREAMER said about the same vertex, for the comparison
    output var logic [15:0] v_base,
    output var logic [15:0] v_scar,
    output var logic [15:0] v_bottom,
    output var logic [ 5:0] v_vi,
    output var logic [ 5:0] v_vj,
    output var logic        v_last,

    // ---- TERRAIN.COMPCACHE ---------------------------------------------------
    // The placement, written by the bench because it is the island directory's.
    input  var logic               pos_we,
    input  var logic               pos_axis,      // 0 = wx by column, 1 = wz by row
    input  var logic        [ 5:0] pos_idx,
    input  var logic signed [31:0] pos_val,

    output var logic        cc_fill_accept,
    output var logic        cc_fill_busy,
    output var logic        cc_fill_done,
    output var logic [31:0] cc_fill_records,
    output var logic [31:0] cc_patches_filled,
    output var logic [31:0] cc_fill_overrun,
    output var logic [31:0] cc_lat_oob,

    input  var logic        cc_serve_release,
    output var logic        cc_serve_valid,
    output var logic [15:0] cc_serve_src_id,
    output var logic [31:0] cc_patches_served,

    // THE LATTICE PORT IS SHARED between the C++ readback and TERRAIN.TESS.
    // `cfg_tess_i` decides which drives it -- the readback of phase E has to be
    // able to ask the cache directly, and TESS has to have it uncontested while
    // it walks a subpatch. A bench that let both drive would produce answers
    // neither had asked for.
    input  var logic        cfg_tess_i,
    input  var logic        cc_lat_req,
    input  var logic [ 5:0] cc_lat_vi,
    input  var logic [ 5:0] cc_lat_vj,
    input  var logic        cc_lat_surface,   // 0 = top, 1 = bottom
    output var logic [31:0] cc_lat_h,
    output var logic [31:0] cc_lat_wx,
    output var logic [31:0] cc_lat_wz,

    // ---- TERRAIN.TESS --------------------------------------------------------
    // The job is driven from the bench because it is TERRAIN.LOD's decision,
    // and LOD is a separate block with its own reference. What is composed here
    // is the SEAM: TESS reading a cache that a page filled.
    input  var logic        ts_job_valid,
    output var logic        ts_job_ready,
    input  var logic [ 5:0] ts_job_ox,
    input  var logic [ 5:0] ts_job_oz,
    input  var logic [ 1:0] ts_job_level,
    input  var logic [ 1:0] ts_job_lvl_nz,
    input  var logic [ 1:0] ts_job_lvl_pz,
    input  var logic [ 1:0] ts_job_lvl_nx,
    input  var logic [ 1:0] ts_job_lvl_px,
    input  var logic [16:0] ts_job_morph,
    input  var logic        ts_job_surface,
    input  var logic [15:0] ts_job_src_id,

    output var logic        ts_tri_valid,
    input  var logic        ts_tri_ready,
    output var logic [31:0] ts_ax, ts_ay, ts_az,
    output var logic [31:0] ts_bx, ts_by, ts_bz,
    output var logic [31:0] ts_cx, ts_cy, ts_cz,
    output var logic        ts_surface,
    output var logic [15:0] ts_src_id,

    // ---- completion and counters -------------------------------------------
    output var logic        ps_done_valid,
    input  var logic        ps_done_ready,
    output var logic        ps_done_ok,
    output var logic [ 3:0] ps_done_verdict,
    output var logic [31:0] ps_lattices,
    output var logic [31:0] ps_vertices,
    output var logic [31:0] pt_samples,
    output var logic [ 4:0] pt_fields_active,
    output var logic        pt_idle
);

  localparam int unsigned SLOTW = 11;
  localparam int unsigned GENW  = 8;
  localparam int unsigned IMG_WORDS = 4 * 2672;
  localparam int unsigned VW = $clog2(IMG_WORDS);

  logic [63:0] vram_mem [IMG_WORDS];
  always_ff @(posedge clk) if (mw_en) vram_mem[mw_addr] <= mw_data;

  // ------------------------------------------------ TERRAIN.PAGESTREAM -----
  // The played engine reads `valid` and `addr`; client, length and byte mask
  // are the guard's business and this models the fabric.
  /* verilator lint_off UNUSEDSIGNAL */
  zhao_guard_req_t psg_req;
  /* verilator lint_on UNUSEDSIGNAL */
  zhao_guard_rsp_t psg_rsp;
  logic            psg_beat_valid, psg_beat_last;
  logic [63:0]     psg_beat_data;

  logic               ps_v_valid, ps_v_ready;
  logic signed [15:0] ps_base, ps_scar, ps_bottom;
  logic [5:0]         ps_vi, ps_vj;
  logic               ps_last;
  // The streamer's identity passthrough and its other counters are checked by
  // its OWN differential; this bench is about the height path, and reading them
  // here would be a second place they can disagree.
  /* verilator lint_off UNUSEDSIGNAL */
  logic               ps_first;
  logic [SLOTW-1:0]   ps_v_slot, ps_d_slot;
  logic [GENW-1:0]    ps_v_gen, ps_d_gen;
  logic [31:0]        ps_v_epoch, ps_d_epoch, ps_d_src;
  logic [31:0]        ps_refused, ps_bursts, ps_denied, ps_incomplete;
  logic               ps_idle;
  // TERRAIN.PATCH's `src_id_i` is SIXTEEN bits and the streamer carries
  // thirty-two -- the low half is passed and the high half is not, which is a
  // real narrowing on the seam and is named here rather than left to a
  // truncation nobody wrote down.
  logic [31:0]        ps_v_src;
  /* verilator lint_on UNUSEDSIGNAL */
  logic [15:0]        ps_v_flags;

  // kFlagDual is bit 3 of T5's record flags -- `zref::swstream::kFlagDual`,
  // `1u << 3`. Named here rather than written as a bare index, because a bare
  // 3 in a wiring diagram is a constant nobody can grep for.
  localparam int unsigned FLAG_DUAL_BIT = 3;

  assign v_base   = ps_base;
  assign v_scar   = ps_scar;
  assign v_bottom = ps_bottom;
  assign v_vi     = ps_vi;
  assign v_vj     = ps_vj;
  assign v_last   = ps_last;

  zhao_terrain_pagestream #(
      .REGION_BASE (27'h400_0000),
      .REGION_SLOTS(1024),
      .SLOTW       (SLOTW),
      .GENW        (GENW)
  ) u_ps (
      .clk  (clk),
      .rst_n(rst_n),

      .cfg_vram_client_i(zhao_client_e'(cfg_vram_client_i)),
      .cfg_epoch_i      (cfg_epoch_i),

      .j_valid_i (j_valid),
      .j_ready_o (j_ready),
      .j_slot_i  (j_slot),
      .j_gen_i   (j_gen),
      .j_epoch_i (j_epoch),
      .j_src_id_i(j_src_id),
      .j_flags_i (j_flags),

      .guard_req_o (psg_req),
      .guard_rsp_i (psg_rsp),
      .beat_valid_i(psg_beat_valid),
      .beat_data_i (psg_beat_data),
      .beat_last_i (psg_beat_last),

      .v_valid_o (ps_v_valid),
      .v_ready_i (ps_v_ready),
      .v_base_o  (ps_base),
      .v_scar_o  (ps_scar),
      .v_bottom_o(ps_bottom),
      .v_vi_o    (ps_vi),
      .v_vj_o    (ps_vj),
      .v_first_o (ps_first),
      .v_last_o  (ps_last),
      .v_slot_o  (ps_v_slot),
      .v_gen_o   (ps_v_gen),
      .v_epoch_o (ps_v_epoch),
      .v_src_id_o(ps_v_src),
      .v_flags_o (ps_v_flags),

      .done_valid_o  (ps_done_valid),
      .done_ready_i  (ps_done_ready),
      .done_slot_o   (ps_d_slot),
      .done_gen_o    (ps_d_gen),
      .done_epoch_o  (ps_d_epoch),
      .done_ok_o     (ps_done_ok),
      .done_verdict_o(ps_done_verdict),
      .done_src_id_o (ps_d_src),

      .lattices_streamed_o(ps_lattices),
      .lattices_refused_o (ps_refused),
      .vertices_streamed_o(ps_vertices),
      .bursts_read_o      (ps_bursts),
      .guard_denied_o     (ps_denied),
      .incomplete_o       (ps_incomplete),
      .idle_o             (ps_idle)
  );

  // ------------------------------------------------- TERRAIN.PATCH ---------
  // NO GLUE ON THE HEIGHT PATH. base/scar/bottom/vi/vj go straight across --
  // which is the whole reason the streamer emits three planes on one beat
  // instead of one plane per pass.
  // wx FOLLOWS vi AND wz FOLLOWS vj, which is what TERRAIN.COMPCACHE writes
  // into hardware: `wx_m[rd_vi_c]`, `wz_m[rd_vj_c]`. It was the other way round
  // here for one run and the readback said so -- wx came back tracking the row.
  logic signed [31:0] wx_c, wz_c;
  assign wx_c = cfg_x0_i + (cfg_step_i * signed'({26'd0, ps_vi}));
  assign wz_c = cfg_z0_i + (cfg_step_i * signed'({26'd0, ps_vj}));

  /* verilator lint_off UNUSEDSIGNAL */
  logic        pt_fld_add_ready, pt_fld_accept, pt_fld_reject, pt_fld_ready;
  logic        pt_fld_covers;
  logic [15:0] pt_trace_id, pt_trace_cmd;
  logic [31:0] pt_trace_hash, pt_rejected;
  /* verilator lint_on UNUSEDSIGNAL */

  logic signed [31:0] pt_top, pt_bottom, pt_compose_top;
  assign st_top         = pt_top;
  assign st_bottom      = pt_bottom;
  assign st_compose_top = pt_compose_top;

  zhao_terrain_patch u_pt (
      .clk  (clk),
      .rst_n(rst_n),

      .list_clear_i(pt_list_clear),
      .patch_id_i  (16'd0),

      .fld_add_valid_i(1'b0),
      .fld_add_ready_o(pt_fld_add_ready),
      .fld_add_x0_i   (32'sd0),
      .fld_add_z0_i   (32'sd0),
      .fld_add_x1_i   (32'sd0),
      .fld_add_z1_i   (32'sd0),
      .fld_add_hash_i (32'd0),
      .fld_add_cmd_i  (16'd0),

      .fld_add_accept_o(pt_fld_accept),
      .fld_add_reject_o(pt_fld_reject),
      .fields_active_o (pt_fields_active),

      .trace_patch_id_o   (pt_trace_id),
      .trace_hash_o       (pt_trace_hash),
      .trace_cmd_o        (pt_trace_cmd),
      .programs_rejected_o(pt_rejected),

      .vtx_valid_i(ps_v_valid),
      .vtx_ready_o(ps_v_ready),
      .base_i     (ps_base),
      .scar_i     (ps_scar),
      .bottom_i   (ps_bottom),
      .dual_i     (ps_v_flags[FLAG_DUAL_BIT]),
      .wx_i       (wx_c),
      .wz_i       (wz_c),
      .vi_i       (ps_vi),
      .vj_i       (ps_vj),
      .src_id_i   (ps_v_src[15:0]),

      // THE FIELD LANE, TIED OFF. With no accepted programs there is no lane,
      // and §3.4's `live_top` collapses to `compose_top` -- which is exactly
      // the half this bench can measure against a page.
      .fld_valid_i (1'b0),
      .fld_ready_o (pt_fld_ready),
      .fld_height_i(32'sd0),

      .fld_covers_o(pt_fld_covers),

      .st_valid_o     (st_valid),
      .st_ready_i     (st_ready),
      .top_o          (pt_top),
      .bottom_o       (pt_bottom),
      .compose_top_o  (pt_compose_top),
      .st_dirty_o     (st_dirty),
      .st_src_id_o    (st_src_id),
      .subpatch_dirty_o(subpatch_dirty),

      .terrain_samples_evaluated_o(pt_samples),
      .idle_o                     (pt_idle)
  );

  // ------------------------------------------------ TERRAIN.COMPCACHE ------
  // PORT-FOR-PORT off TERRAIN.PATCH's output, which is what its own contract
  // says it is. The only thing this bench decides is WHEN the fill starts, and
  // that is one pulse before the first record.
  //
  // `fill_start` is driven from the streamer's `v_first_o` -- the vertex that
  // IS the first, rather than a cycle the bench counted -- so a lattice that
  // began somewhere else would start the fill somewhere else too, instead of
  // quietly filling from the middle.
  logic cc_fill_start;
  assign cc_fill_start = ps_v_valid && ps_v_ready && ps_first;

  /* verilator lint_off UNUSEDSIGNAL */
  logic [1:0]  cc_cs_substance;
  logic [31:0] cc_cs_oob;
  /* verilator lint_on UNUSEDSIGNAL */

  logic signed [31:0] cc_lat_h_s, cc_lat_wx_s, cc_lat_wz_s;
  assign cc_lat_h  = cc_lat_h_s;
  assign cc_lat_wx = cc_lat_wx_s;
  assign cc_lat_wz = cc_lat_wz_s;

  zhao_terrain_compcache_front #(
      .LAT_W(33),
      .LAT_H(33)
  ) u_cc (
      .clk  (clk),
      .rst_n(rst_n),

      .fill_start_i (cc_fill_start),
      .fill_accept_o(cc_fill_accept),
      .fill_busy_o  (cc_fill_busy),

      // THE CACHE'S READY IS NOT WIRED BACK INTO TERRAIN.PATCH, and that is
      // deliberate: `st_ready` stays the C++ side's, so a phase can stall the
      // composed stream independently of whether the cache would have. What is
      // checked instead is `fill_overrun_o` and `fill_records_o` -- if the
      // cache ever refused a record this arrangement offered, the counts say
      // so rather than the record vanishing.
      .st_valid_i (st_valid && st_ready),
      .st_ready_o (cc_st_ready),
      .st_top_i   (pt_top),
      .st_bottom_i(pt_bottom),
      .st_src_id_i(st_src_id),

      .pos_we_i  (pos_we),
      .pos_axis_i(pos_axis),
      .pos_idx_i (pos_idx),
      .pos_val_i (pos_val),

      .cs_we_i         (1'b0),
      .cs_w_ci_i       (5'd0),
      .cs_w_cj_i       (5'd0),
      .cs_w_substance_i(2'd0),

      .dual_i(ps_v_flags[FLAG_DUAL_BIT]),

      .fill_done_o(cc_fill_done),

      .serve_release_i(cc_serve_release),
      .serve_valid_o  (cc_serve_valid),
      .serve_src_id_o (cc_serve_src_id),

      .lat_req_i    (cfg_tess_i ? ts_lat_req        : cc_lat_req),
      .lat_vi_i     (cfg_tess_i ? ts_lat_vi         : cc_lat_vi),
      .lat_vj_i     (cfg_tess_i ? ts_lat_vj         : cc_lat_vj),
      .lat_surface_i(cfg_tess_i ? ts_lat_surface_w  : cc_lat_surface),
      .lat_h_o      (cc_lat_h_s),
      .lat_wx_o     (cc_lat_wx_s),
      .lat_wz_o     (cc_lat_wz_s),

      .cs_req_i      (cfg_tess_i && ts_cs_req),
      .cs_ci_i       (ts_cs_ci),
      .cs_cj_i       (ts_cs_cj),
      .cs_substance_o(cc_cs_substance),

      .fill_records_o  (cc_fill_records),
      .patches_filled_o(cc_patches_filled),
      .patches_served_o(cc_patches_served),
      .fill_overrun_o  (cc_fill_overrun),
      .lat_oob_o       (cc_lat_oob),
      .cs_oob_o        (cc_cs_oob)
  );

  /* verilator lint_off UNUSEDSIGNAL */
  logic cc_st_ready;
  /* verilator lint_on UNUSEDSIGNAL */

  // ------------------------------------------------- TERRAIN.TESS ----------
  // Its `lat_*` port is port-for-port TERRAIN.COMPCACHE's serve side and its
  // `cs_*` port is that block's cell-state read, so both go on with no glue.
  // The one-cycle read latency is the cache's own contract and TESS was built
  // to it -- this bench does not adapt anything, which is the claim.
  logic       ts_lat_req, ts_lat_surface_w;
  logic [5:0] ts_lat_vi, ts_lat_vj;
  logic       ts_cs_req;
  logic [4:0] ts_cs_ci, ts_cs_cj;

  logic signed [31:0] ts_ax_s, ts_ay_s, ts_az_s;
  logic signed [31:0] ts_bx_s, ts_by_s, ts_bz_s;
  logic signed [31:0] ts_cx_s, ts_cy_s, ts_cz_s;
  assign ts_ax = ts_ax_s; assign ts_ay = ts_ay_s; assign ts_az = ts_az_s;
  assign ts_bx = ts_bx_s; assign ts_by = ts_by_s; assign ts_bz = ts_bz_s;
  assign ts_cx = ts_cx_s; assign ts_cy = ts_cy_s; assign ts_cz = ts_cz_s;

  /* verilator lint_off UNUSEDSIGNAL */
  logic [31:0] ts_tris, ts_rejected, ts_clamped;
  logic        ts_idle, ts_job_reject;
  /* verilator lint_on UNUSEDSIGNAL */

  zhao_terrain_tess u_ts (
      .clk  (clk),
      .rst_n(rst_n),

      .job_valid_i  (ts_job_valid),
      .job_ready_o  (ts_job_ready),
      .job_ox_i     (ts_job_ox),
      .job_oz_i     (ts_job_oz),
      .job_level_i  (ts_job_level),
      .job_lvl_nz_i (ts_job_lvl_nz),
      .job_lvl_pz_i (ts_job_lvl_pz),
      .job_lvl_nx_i (ts_job_lvl_nx),
      .job_lvl_px_i (ts_job_lvl_px),
      .job_morph_i  (ts_job_morph),
      .job_surface_i(ts_job_surface),
      .job_dual_i   (ps_v_flags[FLAG_DUAL_BIT]),
      .job_src_id_i (ts_job_src_id),

      .lat_req_o    (ts_lat_req),
      .lat_vi_o     (ts_lat_vi),
      .lat_vj_o     (ts_lat_vj),
      .lat_surface_o(ts_lat_surface_w),
      .lat_h_i      (cc_lat_h_s),
      .lat_wx_i     (cc_lat_wx_s),
      .lat_wz_i     (cc_lat_wz_s),

      .cs_req_o      (ts_cs_req),
      .cs_ci_o       (ts_cs_ci),
      .cs_cj_o       (ts_cs_cj),
      .cs_substance_i(cc_cs_substance),

      .tri_valid_o(ts_tri_valid),
      .tri_ready_i(ts_tri_ready),
      .ax_o(ts_ax_s), .ay_o(ts_ay_s), .az_o(ts_az_s),
      .bx_o(ts_bx_s), .by_o(ts_by_s), .bz_o(ts_bz_s),
      .cx_o(ts_cx_s), .cy_o(ts_cy_s), .cz_o(ts_cz_s),
      .surface_o(ts_surface),
      .src_id_o (ts_src_id),

      .terrain_triangles_emitted_o(ts_tris),
      .subpatch_rejected_o        (ts_rejected),
      .lod_clamped_o              (ts_clamped),
      .job_reject_o               (ts_job_reject),
      .idle_o                     (ts_idle)
  );

  // ------------------------------------------- the played read engine ------
  logic          rg_fwd;
  logic [7:0]    rg_hold;
  logic          rg_ok_q, rg_viol_q;
  logic          rg_busy;
  logic [7:0]    rg_wait;
  logic [2:0]    rg_beat;
  logic [VW-1:0] rg_word, rg_idx;

  assign psg_rsp.ready     = !rg_fwd;
  assign psg_rsp.ok        = rg_ok_q;
  assign psg_rsp.violation = rg_viol_q;
  assign rg_idx = rg_word + {{(VW-3){1'b0}}, rg_beat};

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      rg_fwd    <= 1'b0;
      rg_hold   <= 8'd0;
      rg_ok_q   <= 1'b0;
      rg_viol_q <= 1'b0;
      rg_busy   <= 1'b0;
      rg_wait   <= 8'd0;
      rg_beat   <= 3'd0;
      rg_word   <= '0;
      psg_beat_valid <= 1'b0;
      psg_beat_data  <= 64'd0;
      psg_beat_last  <= 1'b0;
    end else begin
      rg_ok_q        <= 1'b0;
      rg_viol_q      <= 1'b0;
      psg_beat_valid <= 1'b0;
      psg_beat_last  <= 1'b0;

      if (rg_fwd) begin
        if (rg_hold != 8'd0) rg_hold <= rg_hold - 8'd1;
        else                 rg_fwd  <= 1'b0;
      end else if (psg_req.valid) begin
        rg_ok_q <= 1'b1;
        rg_fwd  <= 1'b1;
        rg_hold <= cfg_grant_hold_i;
        rg_busy <= 1'b1;
        rg_wait <= cfg_rd_latency_i;
        rg_beat <= 3'd0;
        rg_word <= VW'(({5'd0, psg_req.addr} - cfg_vram_window_base_i) >> 3);
      end

      if (rg_busy) begin
        if (rg_wait != 8'd0) begin
          rg_wait <= rg_wait - 8'd1;
        end else begin
          psg_beat_valid <= 1'b1;
          psg_beat_data  <= vram_mem[rg_idx];
          psg_beat_last  <= (rg_beat == 3'd7);
          rg_wait        <= cfg_rd_gap_i;
          if (rg_beat == 3'd7) rg_busy <= 1'b0;
          else                 rg_beat <= rg_beat + 3'd1;
        end
      end
    end
  end

endmodule
