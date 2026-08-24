// zhao_pair_tess_normals.sv — CHARACTERIZATION WRAPPER, not a console block.
//
// ---------------------------------------------------------------------------
// WHY THIS EXISTS
// ---------------------------------------------------------------------------
// The 2026-08-23 budget audit asked for two things this repository never built:
//
//   "registered characterisation wrappers -- registered stimulus -> DUT ->
//    registered hash sink. Raw leaf blocks with hundreds of virtual pins are
//    poor physical models."
//   "fit representative PAIRS, because the seam becomes internal: TMU+CACHE,
//    FRAGMENT+TILESTORE, SETUP+BINNER, TESS+NORMALS, ..."
//
// Every renderer number in the census is a LEAF fit. `zhao_terrain_tess` alone
// presents its nine 32-bit vertex outputs as virtual pins, so the fitter is
// told they are free and the seam that actually matters -- TESS handing a
// triangle to NORMALS -- is never placed at all. The composed shell has been
// measured four times; the RENDERER has never been built as one machine.
//
// This is the smallest honest step toward that: one real seam, internal.
//
// NOT INSTANTIATED BY THE CONSOLE. It is absent from the shell QSF and from
// ZHAO_SHELL_RTL, so it cannot affect the shell fit or the source-list parity
// gate. Synthesis reports only.
//
// ---------------------------------------------------------------------------
// WHAT IT MODELS, AND WHAT IT DOES NOT
// ---------------------------------------------------------------------------
// The lattice port is driven from a REGISTERED memory read rather than from
// combinational logic, because that is what the real lattice is: a memory with
// a registered output. Driving it from a function of the request would flatter
// the result by removing a real cycle boundary.
//
// The stimulus is registered and the results are folded into a registered hash,
// so no DUT output reaches a pin directly. What this measures is the pair's
// internal timing, not a testbench's.
//
// It does NOT model the lattice's true depth, the real height distribution, or
// backpressure from the consumer of normals. `nrm_ready_i` is tied high: this
// asks "how fast can the pair run when nothing stalls it", which is the
// question a clock target is about.
// ---------------------------------------------------------------------------

module zhao_pair_tess_normals (
    input  logic        clk,
    input  logic        rst_n,
    input  logic        stim_valid_i,
    input  logic [31:0] stim_i,
    output logic [31:0] hash_o
);

  // ---- registered stimulus ------------------------------------------------
  logic        job_valid_q;
  logic [31:0] stim_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      job_valid_q <= 1'b0;
      stim_q      <= 32'd0;
    end else begin
      job_valid_q <= stim_valid_i;
      stim_q      <= stim_i;
    end
  end

  logic job_ready;

  // ---- the lattice, as a registered-read memory ---------------------------
  // 64 entries is not the real lattice size; it is enough to make the read a
  // memory read rather than a wire, which is the property being modelled.
  logic signed [31:0] lat_mem [0:63];
  logic signed [31:0] lat_h_q, lat_wx_q, lat_wz_q;
  logic        [ 1:0] cs_sub_q;

  logic               lat_req;
  logic        [ 5:0] lat_vi, lat_vj;
  logic               lat_surface;
  logic               cs_req;
  logic        [ 4:0] cs_ci, cs_cj;

  always_ff @(posedge clk) begin
    lat_h_q  <= lat_mem[lat_vi];
    lat_wx_q <= {26'd0, lat_vi} <<< 16;
    lat_wz_q <= {26'd0, lat_vj} <<< 16;
    cs_sub_q <= 2'd0;  // SOLID, terrain_rules 3.3
    if (stim_valid_i) lat_mem[stim_i[5:0]] <= $signed(stim_i);
  end

  // ---- DUT 1: tessellator -------------------------------------------------
  logic               tri_valid, tri_ready;
  logic signed [31:0] ax, ay, az, bx, by, bz, cx, cy, cz;
  logic               surface_w;
  logic        [15:0] src_id_w;
  logic [31:0] tess_emitted, tess_rejected, tess_clamped;
  logic        tess_reject, tess_idle;

  zhao_terrain_tess u_tess (
      .clk(clk), .rst_n(rst_n),
      .job_valid_i(job_valid_q), .job_ready_o(job_ready),
      .job_ox_i(stim_q[5:0] & 6'h38), .job_oz_i(stim_q[11:6] & 6'h38),
      .job_level_i(stim_q[13:12]), .job_lvl_nz_i(stim_q[15:14]),
      .job_lvl_pz_i(stim_q[17:16]), .job_lvl_nx_i(stim_q[19:18]),
      .job_lvl_px_i(stim_q[21:20]),
      .job_morph_i({1'b0, stim_q[31:16]}),
      .job_surface_i(stim_q[22]), .job_dual_i(stim_q[23]),
      .job_src_id_i(stim_q[31:16]),
      .lat_req_o(lat_req), .lat_vi_o(lat_vi), .lat_vj_o(lat_vj),
      .lat_surface_o(lat_surface),
      .lat_h_i(lat_h_q), .lat_wx_i(lat_wx_q), .lat_wz_i(lat_wz_q),
      .cs_req_o(cs_req), .cs_ci_o(cs_ci), .cs_cj_o(cs_cj),
      .cs_substance_i(cs_sub_q),
      .tri_valid_o(tri_valid), .tri_ready_i(tri_ready),
      .ax_o(ax), .ay_o(ay), .az_o(az),
      .bx_o(bx), .by_o(by), .bz_o(bz),
      .cx_o(cx), .cy_o(cy), .cz_o(cz),
      .surface_o(surface_w), .src_id_o(src_id_w),
      .terrain_triangles_emitted_o(tess_emitted),
      .subpatch_rejected_o(tess_rejected),
      .lod_clamped_o(tess_clamped),
      .job_reject_o(tess_reject), .idle_o(tess_idle)
  );

  // ---- DUT 2: normals. THIS is the seam being measured. -------------------
  logic               nrm_valid;
  logic signed [31:0] nx, ny, nz;
  logic               degenerate;
  logic        [15:0] nrm_src_id;
  logic [31:0] nrm_evaluated;
  logic        nrm_idle;

  zhao_terrain_normals u_normals (
      .clk(clk), .rst_n(rst_n),
      .tri_valid_i(tri_valid), .tri_ready_o(tri_ready),
      .ax_i(ax), .ay_i(ay), .az_i(az),
      .bx_i(bx), .by_i(by), .bz_i(bz),
      .cx_i(cx), .cy_i(cy), .cz_i(cz),
      .src_id_i(src_id_w),
      .nrm_valid_o(nrm_valid), .nrm_ready_i(1'b1),
      .nx_o(nx), .ny_o(ny), .nz_o(nz),
      .degenerate_o(degenerate), .src_id_o(nrm_src_id),
      .terrain_samples_evaluated_o(nrm_evaluated), .idle_o(nrm_idle)
  );

  // ---- registered hash sink ----------------------------------------------
  // Everything the pair produces folds into one register, so no result reaches
  // a pin and the fitter cannot optimise a lane away for being unobserved.
  logic [31:0] hash_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) hash_q <= 32'd0;
    else if (nrm_valid)
      hash_q <= (hash_q ^ nx) + (hash_q << 5) + nz
              + {15'd0, degenerate, nrm_src_id}
              + ny;
    else
      hash_q <= hash_q
              ^ {31'd0, job_ready}
              ^ {31'd0, tess_idle}
              ^ {31'd0, nrm_idle}
              ^ {31'd0, tess_reject}
              ^ {31'd0, lat_req}
              ^ {31'd0, cs_req}
              ^ {27'd0, cs_ci}
              ^ {27'd0, cs_cj}
              ^ {31'd0, lat_surface}
              ^ {31'd0, surface_w}
              ^ tess_emitted ^ tess_rejected ^ tess_clamped ^ nrm_evaluated;
  end
  assign hash_o = hash_q;

endmodule
