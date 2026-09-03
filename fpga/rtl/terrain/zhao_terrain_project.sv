// zhao_terrain_project.sv — TERRAIN.PROJECT: `project_vertex` in hardware, and
// the door between the Mantle and the rasterizer (phase 6, ZH-051).
//
// Law, in citation order:
//   design/contracts/TERRAIN.PROJECT.md — the block contract.
//   design/blocks.yml — `inputs: [lod_decisions, terrain_normals]`, `outputs:
//       [terrain_primitives, mosaic_candidates]`, `downstream: [GEOM.CLIP,
//       TEXTURE.MOSAIC]`, `backpressure: ready_valid`, `latency: variable`,
//       "1 projected vertex per clock", counter `terrain_triangles_emitted`.
//   reference/src/zrender/rast.cpp — `project_vertex`. THE law.
//   reference/include/zref/zref_fixp.hpp — `mat4_vec4` (§2), `fx_div_exact`
//       (§3), `fx_mad` (§3), `to_screen_xy` (§8), `rescale_s32` (§4).
//   spec/qformats.md §2, §3, §4, §8.
//   reference/src/zrender/terrain.cpp — the terrain draw path: the grid is
//       projected once per view call, and a primitive whose corner vertices
//       include one behind the eye is dropped whole.
//
// ---------------------------------------------------------------------------
// THE PROJECTOR IS NO LONGER IN THIS FILE
// ---------------------------------------------------------------------------
// The ledger's note used to read "Kept separate from GEOM.PROJECT by architect
// ruling (1.D): merging later is a trivial edit." The merge has now happened,
// and it was not quite trivial — the seam had to be placed by the two blocks'
// stated latencies rather than by convenience.
//
// The law — the exact row sums, the near-plane verdict, the 31-step restoring
// divider, the viewport `fx_mad` and `to_screen_xy` — lives once, in
// `fpga/rtl/common/zhao_project_core.sv`. `zhao_geom_project` instantiates the
// same core with a narrower payload.
//
// **The merge was made on a differential, not on a resemblance.** The budget
// audit reported the two blocks had byte-identical arithmetic SIGNATURES — 11
// nonconstant multiplies, widest operand 32 bits, 33 mapped DSPs each — which
// is a statement about shape, not behaviour. RUN-20260824-0522 drove both
// pre-merge blocks and the shipped oracle from one stimulus stream and compared
// every projected vertex three ways, with zero mismatches, against ten positive
// controls the harness had to catch and did.
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK STILL OWNS — the triangle framing
// ---------------------------------------------------------------------------
//   * **Stage 0, the vertex sequencer.** One triangle in, three vertices out on
//     three consecutive clocks.
//   * **The reassembly register.** Vertices A and B are held; vertex C's bits
//     come straight off the core on the clock that assembles the triangle.
//   * **The riders**: the corner index, the source id and the layer-E Mosaic
//     triple, all carried through the core as its opaque payload, which this
//     block packs and unpacks and the core never interprets.
//   * **`idle_o`**, which ANDs this block's own emptiness with the core's.
//
// **The latency did not move: still 38, and still for the same reason.** 1
// sequencer + 36 core + 1 reassembly. The core ends at its `to_screen_xy`
// register, which is exactly where this block's `s6` used to be; that is why
// the seam is there and not one stage either side.
//
// ---------------------------------------------------------------------------
// LAWS CHOSEN (no spec states these; they are decisions, recorded as such)
// ---------------------------------------------------------------------------
// A. THIS BLOCK TAKES A TRIANGLE AND EMITS A TRIANGLE. `project_vertex` is a
//    per-VERTEX function and the ledger's rate line is per vertex, but the
//    only consumer that exists — GEOM.CLIP — takes triangles, and the only
//    producer that exists — TERRAIN.TESS, through TERRAIN.NORMALS — emits
//    triangles. So the input packet is TERRAIN.NORMALS' input packet, the
//    output packet is GEOM.CLIP's input packet, and the three vertices go
//    through ONE projector on three consecutive clocks: one projected vertex
//    per clock — the ledger's rate, met literally — and one triangle every
//    three clocks.
//
//    **THE COST, WHICH IS LARGE AND IS NOT THIS BLOCK'S TO FIX.** A shared
//    lattice vertex is projected once per triangle that uses it, up to six
//    times: a 33×33 patch is 1,089 unique vertices but 2,048 triangles, so
//    **6,144 projections**. The fix is a post-transform vertex cache and the
//    ledger's `GEOM.WCACHE` is exactly that. Deliberately NOT built into the
//    merge: it is a separate block, and folding it in here would have made one
//    change out of two.
// B. THE DUAL VIEW IS TWO REGISTER SETS AND A PACKET BIT, NOT TWO DATAPATHS.
//    Duo runs two 256×192 views that share the frame's clock budget rather
//    than each needing all of it. The register sets live in the core; this
//    block passes `view_i` through and receives it back on the packet.
// C. `mosaic_candidates` IS THREE FIELDS ON THE PRIMITIVE PACKET, NOT A SECOND
//    STREAM, AND THIS BLOCK SELECTS NOTHING. terrain_rules.md §6.2 gives the
//    Mosaic PICK to TEXTURE.MOSAIC, and the pick is per TEXEL, which is not a
//    quantity this block has. Layer E's {matA, matB, weight} triple is per CELL
//    and rides through unaltered.
// D. THE FACE NORMAL DOES NOT RIDE THROUGH. It is a per-triangle quantity, this
//    block does no shading, and `src_id` is the tree's existing re-association
//    mechanism. Recorded as a deliberate divergence from the ledger's input
//    list rather than hidden.
//
// The guard band is a CLAMP and it is enforced inside the core, at both rails
// on both axes.
//   ENFORCED-BY: tests/terrain/terrain_project_directed.cpp:main
//
// Conservative SystemVerilog subset only (charter §2); no package deps.

module zhao_terrain_project (
    input logic clk,
    input logic rst_n,

    // -----------------------------------------------------------------------
    // configuration: the view-projection matrix and the viewport, per view.
    // Written one word at a time (a register file, the way CMD.DECODER writes
    // one), so no kilobit-wide port appears on the boundary. The registers
    // themselves live in the shared core.
    //   cfg_addr_i 0..15 : matrix element, row-major, row*4 + col, fx16
    //   cfg_addr_i 16    : viewport origin { y0 = data[27:16], x0 = data[11:0] }
    //   cfg_addr_i 17    : viewport extent { h  = data[27:16], w  = data[11:0] }
    // -----------------------------------------------------------------------
    input logic        cfg_we_i,
    input logic        cfg_view_i,
    input logic [ 4:0] cfg_addr_i,
    input logic [31:0] cfg_data_i,

    // -----------------------------------------------------------------------
    // terrain_primitives in — EXACTLY TERRAIN.NORMALS' input packet, plus the
    // view select and the layer-E Mosaic candidates that ride with the cell.
    // -----------------------------------------------------------------------
    input  logic               tri_valid_i,
    output logic               tri_ready_o,
    input  logic signed [31:0] ax_i,
    input  logic signed [31:0] ay_i,
    input  logic signed [31:0] az_i,
    input  logic signed [31:0] bx_i,
    input  logic signed [31:0] by_i,
    input  logic signed [31:0] bz_i,
    input  logic signed [31:0] cx_i,
    input  logic signed [31:0] cy_i,
    input  logic signed [31:0] cz_i,
    input  logic        [15:0] src_id_i,
    input  logic               view_i,
    input  logic        [ 7:0] mat_a_i,
    input  logic        [ 7:0] mat_b_i,
    input  logic        [ 7:0] weight_i,

    // -----------------------------------------------------------------------
    // terrain_primitives out — EXACTLY zhao_geom_clip's input packet, plus the
    // per-vertex Q16.16 1/w depth, the view tag and the Mosaic candidates.
    // -----------------------------------------------------------------------
    output logic               out_valid_o,
    input  logic               out_ready_i,
    output logic signed [20:0] out_ax_o,
    output logic signed [20:0] out_ay_o,
    output logic signed [20:0] out_bx_o,
    output logic signed [20:0] out_by_o,
    output logic signed [20:0] out_cx_o,
    output logic signed [20:0] out_cy_o,
    output logic        [ 2:0] out_behind_o,  // bit 0 = A, 1 = B, 2 = C
    output logic        [15:0] out_src_id_o,
    output logic signed [31:0] out_ad_o,      // Q16.16 1/w, vertex A
    output logic signed [31:0] out_bd_o,
    output logic signed [31:0] out_cd_o,
    output logic               out_view_o,
    output logic        [ 7:0] out_mat_a_o,   // mosaic_candidates: layer E,
    output logic        [ 7:0] out_mat_b_o,   //   forwarded, never selected
    output logic        [ 7:0] out_weight_o,

    output logic [31:0] terrain_triangles_emitted_o,
    output logic        idle_o
);

  // The riders, carried through the core as one opaque word. The core never
  // interprets it; this block packs it at stage 0 and unpacks it at stage 6.
  //   [41:40] corner index k   [39:24] src_id
  //   [23:16] mat_a            [15:8]  mat_b        [7:0] weight
  localparam int unsigned PAY_W = 42;

  // ---------------------------------------------------------------------------
  // the rigid-pipeline advance
  // ---------------------------------------------------------------------------
  // This block's back-pressure boundary is the TRIANGLE register, one stage
  // downstream of the core's output register. That is why the core takes its
  // enable from here rather than deriving one: GEOM.PROJECT's boundary is the
  // core's own output, and a core that decided for itself would have had to
  // pick one of the two and silently change the other.
  logic out_valid_r;
  wire  advance = !out_valid_r || out_ready_i;

  // ---------------------------------------------------------------------------
  // stage 0 — the vertex sequencer: one triangle in, three vertices out
  // ---------------------------------------------------------------------------
  logic               job_valid;
  logic        [ 1:0] job_k;
  logic signed [31:0] job_x[0:2];
  logic signed [31:0] job_y[0:2];
  logic signed [31:0] job_z[0:2];
  logic        [15:0] job_src;
  logic               job_view;
  logic        [ 7:0] job_mat_a, job_mat_b, job_weight;

  wire job_last = job_valid && (job_k == 2'd2);
  wire job_free = !job_valid || job_last;
  assign tri_ready_o = advance && job_free;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      job_valid  <= 1'b0;
      job_k      <= 2'd0;
      job_x[0]   <= '0;
      job_x[1]   <= '0;
      job_x[2]   <= '0;
      job_y[0]   <= '0;
      job_y[1]   <= '0;
      job_y[2]   <= '0;
      job_z[0]   <= '0;
      job_z[1]   <= '0;
      job_z[2]   <= '0;
      job_src    <= '0;
      job_view   <= 1'b0;
      job_mat_a  <= '0;
      job_mat_b  <= '0;
      job_weight <= '0;
    end else if (advance) begin
      if (job_free && tri_valid_i) begin
        job_valid  <= 1'b1;
        job_k      <= 2'd0;
        job_x[0]   <= ax_i;
        job_y[0]   <= ay_i;
        job_z[0]   <= az_i;
        job_x[1]   <= bx_i;
        job_y[1]   <= by_i;
        job_z[1]   <= bz_i;
        job_x[2]   <= cx_i;
        job_y[2]   <= cy_i;
        job_z[2]   <= cz_i;
        job_src    <= src_id_i;
        job_view   <= view_i;
        job_mat_a  <= mat_a_i;
        job_mat_b  <= mat_b_i;
        job_weight <= weight_i;
      end else if (job_last) begin
        job_valid <= 1'b0;
      end else if (job_valid) begin
        job_k <= job_k + 2'd1;
      end
    end
  end

  logic signed [31:0] sel_x, sel_y, sel_z;
  always_comb begin
    sel_x = job_x[job_k];
    sel_y = job_y[job_k];
    sel_z = job_z[job_k];
  end

  // ---------------------------------------------------------------------------
  // stages 1 .. 36 — the shared projection core
  // ---------------------------------------------------------------------------
  logic             s6_valid;
  logic signed [20:0] s6_px, s6_py;
  logic signed [31:0] s6_invw;
  logic [30:0]          s6_clipw_unused;  // see the u_core comment
  logic             s6_behind;
  logic             s6_view;
  logic [PAY_W-1:0] s6_pay;
  logic             core_busy;

  zhao_project_core #(
      .PAYLOAD_W(PAY_W)
  ) u_core (
      .clk  (clk),
      .rst_n(rst_n),

      .cfg_we_i  (cfg_we_i),
      .cfg_view_i(cfg_view_i),
      .cfg_addr_i(cfg_addr_i),
      .cfg_data_i(cfg_data_i),

      .en_i(advance),

      .in_valid_i(job_valid),
      .vx_i      (sel_x),
      .vy_i      (sel_y),
      .vz_i      (sel_z),
      .view_i    (job_view),
      .payload_i ({job_k, job_src, job_mat_a, job_mat_b, job_weight}),

      .out_valid_o  (s6_valid),
      .out_x_o      (s6_px),
      .out_y_o      (s6_py),
      .out_d_o      (s6_invw),
      // clip.w, added to the core 2026-09-04 for GEOM.DEPTHQUANT. TERRAIN does
      // not carry it YET and the pin is connected explicitly rather than left
      // to default, so the seam is visible instead of silent.
      //
      // The gap is real and named: terrain writes s6_invw -- a Q16.16 quotient
      // -- into a depth field that the rest of the console reads as invw24, so
      // terrain depth is on a different scale from geometry depth until
      // DEPTHQUANT is in this path too. Carrying w here costs THREE registers,
      // not one, because this block accumulates a triangle's three corners. It
      // is deliberately not paid before there is a consumer.
      .out_w_o      (s6_clipw_unused),
      .out_behind_o (s6_behind),
      .out_view_o   (s6_view),
      .out_payload_o(s6_pay),

      .busy_o(core_busy)
  );

  wire [ 1:0] s6_k      = s6_pay[41:40];
  wire [15:0] s6_src    = s6_pay[39:24];
  wire [ 7:0] s6_mat_a  = s6_pay[23:16];
  wire [ 7:0] s6_mat_b  = s6_pay[15:8];
  wire [ 7:0] s6_weight = s6_pay[7:0];

  // ---------------------------------------------------------------------------
  // stage 7 — reassemble the triangle
  // ---------------------------------------------------------------------------
  logic signed [20:0] acc_x[0:2];
  logic signed [20:0] acc_y[0:2];
  logic signed [31:0] acc_d[0:2];
  // Only vertices A and B are held: vertex C's bits are still in flight at the
  // clock that assembles the triangle, so they come straight off the core.
  logic        [ 1:0] acc_behind;

  integer k4;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      for (k4 = 0; k4 < 3; k4 = k4 + 1) begin
        acc_x[k4] <= '0;
        acc_y[k4] <= '0;
        acc_d[k4] <= '0;
      end
      acc_behind <= '0;

      out_valid_r  <= 1'b0;
      out_ax_o     <= '0;
      out_ay_o     <= '0;
      out_bx_o     <= '0;
      out_by_o     <= '0;
      out_cx_o     <= '0;
      out_cy_o     <= '0;
      out_behind_o <= '0;
      out_src_id_o <= '0;
      out_ad_o     <= '0;
      out_bd_o     <= '0;
      out_cd_o     <= '0;
      out_view_o   <= 1'b0;
      out_mat_a_o  <= '0;
      out_mat_b_o  <= '0;
      out_weight_o <= '0;

      terrain_triangles_emitted_o <= '0;
    end else if (advance) begin
      if (out_valid_r && out_ready_i) out_valid_r <= 1'b0;
      if (s6_valid) begin
        acc_x[s6_k] <= s6_px;
        acc_y[s6_k] <= s6_py;
        acc_d[s6_k] <= s6_invw;
        if (s6_k != 2'd2) acc_behind[s6_k[0]] <= s6_behind;
        if (s6_k == 2'd2) begin
          out_valid_r  <= 1'b1;
          out_ax_o     <= acc_x[0];
          out_ay_o     <= acc_y[0];
          out_bx_o     <= acc_x[1];
          out_by_o     <= acc_y[1];
          out_cx_o     <= s6_px;
          out_cy_o     <= s6_py;
          out_ad_o     <= acc_d[0];
          out_bd_o     <= acc_d[1];
          out_cd_o     <= s6_invw;
          out_behind_o <= {s6_behind, acc_behind[1], acc_behind[0]};
          out_src_id_o <= s6_src;
          out_view_o   <= s6_view;
          out_mat_a_o  <= s6_mat_a;
          out_mat_b_o  <= s6_mat_b;
          out_weight_o <= s6_weight;

          terrain_triangles_emitted_o <= terrain_triangles_emitted_o + 32'd1;
        end
      end
    end
  end

  assign out_valid_o = out_valid_r;

  // idle: nothing latched at stage 0, nothing anywhere in the core, nothing
  // waiting in the triangle register.
  assign idle_o = !job_valid && !core_busy && !out_valid_r;

endmodule : zhao_terrain_project
