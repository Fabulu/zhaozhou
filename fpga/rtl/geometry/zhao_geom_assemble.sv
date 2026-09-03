// zhao_geom_assemble.sv — the index walk that turns a meshlet into triangles.
//
// ---------------------------------------------------------------------------
// THE ARROW NOBODY OWNED
// ---------------------------------------------------------------------------
// `reports/BORING_3D_FUNDAMENTALS_AUDIT.md` R1, verified against the tree
// before this file was written:
//
//   * GEOM.MESHFETCH's descriptor carries `index_offset` and `triangle_count`;
//   * zhao_geom_vdecode.sv accepts NEITHER -- it is the vertex side only;
//   * zhao_geom_setup.sv expects a COMPLETE triangle;
//   * `tri_ax_i` was driven only from zhao_shell_top and geom_bin_pipe, i.e.
//     from a HARNESS;
//   * the ledger had ZERO blocks matching GEOM.(ASSEMBLE|INDEX|TRI).
//
//       MESHFETCH
//         |- vertex_offset --> VDECODE --> SKIN/PROJECT
//         '- index_offset  --> ??? -----> triangle A/B/C --> CLIP/SETUP
//
// This block is that arrow. The audit's instruction is explicit and is why it
// has its own file: "Do not let it emerge accidentally as miscellaneous logic
// inside GEOM.PARAMBUF."
//
// ---------------------------------------------------------------------------
// THE TWO RULES THAT CAN BE SILENTLY WRONG
// ---------------------------------------------------------------------------
// The arithmetic is one addition. What this block actually owns is:
//
//   1. WHICH VERTEX A LOCAL INDEX MEANS. `vertex_offset` is PER VIEW, so the
//      same local index resolves to a different projected vertex in view 0 and
//      view 1. A single walk emitting into both views gives view 1 the
//      vertices of view 0 -- a correct image in one eye and a subtly wrong one
//      in the other, which is the hardest class of bug to see.
//
//   2. WHICH LOCAL INDICES ARE LEGAL. An index >= vertex_count is REFUSED, not
//      clamped. A clamped index draws a triangle from a real vertex belonging
//      to a different part of the mesh -- a visible corruption nothing
//      downstream can detect.
//
// ENFORCED-BY: tests/geometry/geom_assemble_directed.cpp:main
`default_nettype none

module zhao_geom_assemble #(
    // Frozen by ruling, and they are limits rather than suggestions: a u8
    // local index cannot address past 255, and 126 triangles x 3 indices is
    // 378 bytes.
    parameter int unsigned MAX_VERTICES  = 64,
    parameter int unsigned MAX_TRIANGLES = 126,
    parameter int unsigned VIDW          = 16,
    parameter int unsigned SRCW          = 16
) (
    input var logic clk,
    input var logic rst_n,

    // ---- one meshlet in, from GEOM.MESHFETCH --------------------------------
    input  var logic              m_valid_i,
    output var logic              m_ready_o,
    input  var logic [VIDW-1:0]   m_vertex_offset_i,   // PER VIEW
    input  var logic [7:0]        m_vertex_count_i,
    input  var logic [7:0]        m_triangle_count_i,
    input  var logic [15:0]       m_material_id_i,
    input  var logic [31:0]       m_raster_state_i,
    input  var logic [SRCW-1:0]   m_src_id_i,

    // ---- the u8 local index stream -----------------------------------------
    // Three indices per triangle. The walk pulls them as it goes rather than
    // buffering the meshlet, because 378 bytes of buffer to save a stream port
    // is the wrong trade at this depth.
    output var logic              ix_req_o,
    output var logic [8:0]        ix_index_o,          // triplet number
    input  var logic              ix_valid_i,
    input  var logic [7:0]        ix_a_i,
    input  var logic [7:0]        ix_b_i,
    input  var logic [7:0]        ix_c_i,

    // ---- one TriangleDescriptor out ----------------------------------------
    // Exactly GEOM.PARAMBUF's layout, so nothing is invented here.
    output var logic              t_valid_o,
    input  var logic              t_ready_i,
    output var logic [VIDW-1:0]   t_v0_o,
    output var logic [VIDW-1:0]   t_v1_o,
    output var logic [VIDW-1:0]   t_v2_o,
    output var logic [15:0]       t_material_o,
    output var logic [31:0]       t_raster_o,
    output var logic [SRCW-1:0]   t_src_id_o,
    output var logic              t_last_o,

    // ---- evidence -----------------------------------------------------------
    output var logic [31:0]       meshlets_o,
    output var logic [31:0]       triangles_o,
    output var logic [31:0]       refused_limits_o,    // whole meshlet refused
    output var logic [31:0]       refused_index_o      // one triplet refused
);

  // ---- the meshlet's legality, decided BEFORE any triangle is emitted -------
  // A meshlet is refused WHOLE, never as a truncated prefix: a mesh missing
  // its tail looks like a modelling error rather than a fault.
  logic limits_bad_c;
  assign limits_bad_c = (m_vertex_count_i == 8'd0) ||
                        (m_vertex_count_i  > 8'(MAX_VERTICES)) ||
                        (m_triangle_count_i > 8'(MAX_TRIANGLES));

  typedef enum logic [1:0] { S_IDLE, S_FETCH, S_HOLD } state_e;
  state_e st_q;

  logic [VIDW-1:0] voff_q;
  logic [7:0]      vcount_q;
  logic [7:0]      tcount_q;
  logic [15:0]     mat_q;
  logic [31:0]     rast_q;
  logic [SRCW-1:0] src_q;
  logic [8:0]      tri_q;        // which triplet, 0..triangle_count-1

  assign m_ready_o  = (st_q == S_IDLE);
  assign ix_req_o   = (st_q == S_FETCH);
  assign ix_index_o = tri_q;
  assign t_valid_o  = (st_q == S_HOLD);

  // THE LAST TRIANGLE, computed from the count that was captured -- not from
  // the live input, which has moved on.
  assign t_last_o = (tri_q + 9'd1 == 9'(tcount_q));

  // ---- legality of the triplet in hand ------------------------------------
  logic trip_bad_c;
  assign trip_bad_c = (ix_a_i >= vcount_q) ||
                      (ix_b_i >= vcount_q) ||
                      (ix_c_i >= vcount_q);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_q             <= S_IDLE;
      tri_q            <= '0;
      meshlets_o       <= '0;
      triangles_o      <= '0;
      refused_limits_o <= '0;
      refused_index_o  <= '0;
      t_v0_o           <= '0;
      t_v1_o           <= '0;
      t_v2_o           <= '0;
      t_material_o     <= '0;
      t_raster_o       <= '0;
      t_src_id_o       <= '0;
    end else begin
      case (st_q)
        S_IDLE: begin
          if (m_valid_i) begin
            meshlets_o <= meshlets_o + 32'd1;
            if (limits_bad_c) begin
              refused_limits_o <= refused_limits_o + 32'd1;
            end else if (m_triangle_count_i == 8'd0) begin
              // A meshlet with no triangles is legal and emits nothing. It is
              // not a refusal and must not be counted as one.
            end else begin
              voff_q   <= m_vertex_offset_i;
              vcount_q <= m_vertex_count_i;
              tcount_q <= m_triangle_count_i;
              mat_q    <= m_material_id_i;
              rast_q   <= m_raster_state_i;
              src_q    <= m_src_id_i;
              tri_q    <= '0;
              st_q     <= S_FETCH;
            end
          end
        end

        S_FETCH: begin
          if (ix_valid_i) begin
            if (trip_bad_c) begin
              // Refused, and the walk CONTINUES. One bad triplet is one lost
              // triangle, not a lost mesh -- the meshlet's own limits were
              // already checked, so this is a corrupt index rather than a
              // corrupt descriptor.
              refused_index_o <= refused_index_o + 32'd1;
              if (tri_q + 9'd1 == 9'(tcount_q)) st_q <= S_IDLE;
              else tri_q <= tri_q + 9'd1;
            end else begin
              // THE ONE ARITHMETIC ACT: local -> global, per view.
              t_v0_o       <= voff_q + VIDW'(ix_a_i);
              t_v1_o       <= voff_q + VIDW'(ix_b_i);
              t_v2_o       <= voff_q + VIDW'(ix_c_i);
              t_material_o <= mat_q;
              t_raster_o   <= rast_q;
              t_src_id_o   <= src_q;
              st_q         <= S_HOLD;
            end
          end
        end

        S_HOLD: begin
          // THE WALK IS RESUMABLE. A stalled consumer holds this state and
          // `tri_q` does not move, so position in the index stream is never
          // lost -- the failure that would drop triangles from the MIDDLE of a
          // mesh and look like a modelling error.
          if (t_ready_i) begin
            triangles_o <= triangles_o + 32'd1;
            if (tri_q + 9'd1 == 9'(tcount_q)) begin
              st_q <= S_IDLE;
            end else begin
              tri_q <= tri_q + 9'd1;
              st_q  <= S_FETCH;
            end
          end
        end

        default: st_q <= S_IDLE;
      endcase
    end
  end

endmodule : zhao_geom_assemble

`default_nettype wire
