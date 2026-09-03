// zhao_geom_project.sv — GEOM.PROJECT: dual-view per-vertex projection.
//
// Contract: design/contracts/GEOM.PROJECT.md
// Reference: `zref::render::project_vertex`
//   (declared reference/src/zrender/internal.hpp, implemented
//    reference/src/zrender/rast.cpp:43).
//
// The ledger declared this block's reference model as `zref::GeomProject`,
// which does not exist — one of the twenty-five phantoms in
// reports/PHANTOM_REFERENCES.md. `project_vertex` is the real law, it is what
// the software raster projects every vertex with, and TERRAIN.PROJECT is
// already verified against it.
//
// ---------------------------------------------------------------------------
// THE DUPLICATION IS GONE. THIS BLOCK IS NOW A THIN SHELL.
// ---------------------------------------------------------------------------
// This file used to contain a complete copy of `project_vertex` — the same two
// localparams, the same eight helper functions, the same configuration
// register file, the same three row sums, the same divider setup, the same
// 31-stage restoring recurrence and the same viewport `fx_mad` that
// `zhao_terrain_project` contained. Its own header called that
// "A COST, NOT A FEATURE" and the contract recorded extracting a shared core
// as the follow-up.
//
// That core is now `fpga/rtl/common/zhao_project_core.sv`, and this block is
// the vertex-level interface around it: a ready/valid handshake, the accepted-
// vertex counter, and nothing else. TERRAIN.PROJECT is the same core wrapped
// in triangle framing.
//
// **The merge was made on a differential, not on a resemblance.**
// RUN-20260824-0522 drove BOTH pre-merge blocks and the shipped oracle from one
// stimulus stream and compared every projected vertex three ways — 12,300 of
// them across two views, asymmetric viewports, the exact near-plane boundary
// `clip.w == 0`, both guard-band rails, rotating consumer stalls and
// reconfiguration without reset — with **zero mismatches**, and against ten
// positive controls that the harness had to catch and did.
//
// ---------------------------------------------------------------------------
// WHAT THIS SHELL OWNS, AND WHY THE LATENCY DID NOT MOVE
// ---------------------------------------------------------------------------
// The contract says **fixed 36** clocks from accept to result, and it still is.
// The core's own output register IS this block's output register — the seam
// was placed there precisely so that this block adds no stage. See the core's
// header: TERRAIN needs the core to end one stage before ITS output, GEOM needs
// it to end exactly AT its output, and those two requirements pick the boundary
// between them.
//
// This block owns:
//   * `advance` — the rigid-pipeline enable, derived from THIS block's
//     back-pressure boundary, which is the core's output register. It is passed
//     to the core as `en_i`; the core does not derive its own, because
//     TERRAIN's boundary is one stage further on and a core that decided for
//     itself would have silently changed TERRAIN's handshake.
//   * `vertices_transformed_o` — vertices ACCEPTED, not offered: a vertex held
//     off by backpressure has not been transformed.
//   * the `src_id` rider, which travels as the core's opaque payload.
//
// The core owns the law. The four consequences that used to be listed here —
// v.w is the constant 1.0, clip.z is never read, a behind-the-eye vertex
// carries ZERO and is not dropped, and the guard band is a CLAMP and not a
// clip — are stated once, in the core, rather than twice.
//   ENFORCED-BY: tests/geometry/geom_project_directed.cpp:main
//
// Conservative SystemVerilog subset only (charter §2); no package deps.
module zhao_geom_project (
    input logic clk,
    input logic rst_n,

    // ---- configuration: two views, sixteen matrix words + a viewport each ---
    // addr 0..15  : matrix row-major m[0..15] (row 2, words 8..11, inert)
    // addr 16     : { y0[27:16], x0[11:0] }
    // addr 17     : { h [27:16], w [11:0] }
    input logic        cfg_we_i,
    input logic        cfg_view_i,
    input logic [ 4:0] cfg_addr_i,
    input logic [31:0] cfg_data_i,

    // ---- vertices in -------------------------------------------------------
    input  logic               v_valid_i,
    output logic               v_ready_o,
    input  logic signed [31:0] vx_i,
    input  logic signed [31:0] vy_i,
    input  logic signed [31:0] vz_i,
    input  logic               view_i,
    input  logic        [15:0] src_id_i,

    // ---- view vertices out — GEOM.CLIP's per-vertex packet ------------------
    output logic               out_valid_o,
    input  logic               out_ready_i,
    output logic signed [20:0] out_x_o,      // S 12.8 canvas x, clamped ±2048 px
    output logic signed [20:0] out_y_o,      // S 12.8 canvas y, clamped ±2048 px
    output logic signed [31:0] out_d_o,      // Q16.16 1/w
    // clip.w itself, fx16 raw. GEOM.DEPTHQUANT consumes THIS and not out_d_o:
    // the ratified depth law performs its own rcp_u24 on w, and the quotient
    // has already lost the precision reconstruction would need. Zero when
    // out_behind_o.
    output logic        [30:0] out_w_o,
    output logic               out_behind_o, // clip.w <= 0: the vertex is zero
    output logic        [15:0] out_src_id_o,

    output logic [31:0] vertices_transformed_o
);

  // ---------------------------------------------------------------------------
  // the rigid-pipeline advance
  // ---------------------------------------------------------------------------
  // One enable for every stage. A stalled consumer freezes the whole chain; no
  // stage advances alone, so nothing is dropped and nothing overtakes. The
  // core's output register is this block's output register, so the condition
  // reads that register directly.
  logic advance;
  assign advance   = !out_valid_o || out_ready_i;
  assign v_ready_o = advance;

  logic accept;
  assign accept = v_valid_i && v_ready_o;

  // The core exposes two signals this block's packet has no field for. They are
  // named and left unread rather than connected by an empty port, so the reason
  // is in the source instead of in a silence.
  /* verilator lint_off UNUSEDSIGNAL */
  logic core_view;  // GEOM.CLIP's per-vertex packet carries no view tag.
  logic core_busy;  // GEOM.PROJECT's contract has no idle port; TERRAIN's does.
  /* verilator lint_on UNUSEDSIGNAL */

  zhao_project_core #(
      .PAYLOAD_W(16)
  ) u_core (
      .clk  (clk),
      .rst_n(rst_n),

      .cfg_we_i  (cfg_we_i),
      .cfg_view_i(cfg_view_i),
      .cfg_addr_i(cfg_addr_i),
      .cfg_data_i(cfg_data_i),

      .en_i(advance),

      .in_valid_i(accept),
      .vx_i      (vx_i),
      .vy_i      (vy_i),
      .vz_i      (vz_i),
      .view_i    (view_i),
      .payload_i (src_id_i),

      .out_valid_o  (out_valid_o),
      .out_x_o      (out_x_o),
      .out_y_o      (out_y_o),
      .out_d_o      (out_d_o),
      .out_w_o      (out_w_o),
      .out_behind_o (out_behind_o),
      .out_view_o   (core_view),
      .out_payload_o(out_src_id_o),

      .busy_o(core_busy)
  );

  // ---------------------------------------------------------------------------
  // the counter
  // ---------------------------------------------------------------------------
  // Counts vertices ACCEPTED, not offered: a vertex held off by backpressure has
  // not been transformed. `accept` already implies `advance` (v_ready_o IS
  // advance), and the guard is kept anyway so the counter cannot tick on a
  // frozen cycle if that ever stops being true.
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      vertices_transformed_o <= '0;
    end else if (advance) begin
      if (accept && vertices_transformed_o != 32'hFFFF_FFFF) begin
        vertices_transformed_o <= vertices_transformed_o + 32'd1;
      end
    end
  end

endmodule : zhao_geom_project
