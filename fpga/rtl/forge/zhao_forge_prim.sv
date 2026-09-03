// zhao_forge_prim.sv — six primitive families, one bounded topology generator.
//
// ---------------------------------------------------------------------------
// THE FAMILIES ARE CLOSED (owner ruling 2026-08-31 §6.5, restated by R11)
// ---------------------------------------------------------------------------
//     ribbon          radial fan / ring       tube
//     radial shell    billboard sheet         terrain cliff / skirt
//
// The ruling's interesting half is what it DELETED, by recognising four things
// as uses of something else rather than as generators:
//
//     shard burst   -> a PARTICLE POPULATION. PART.*, not geometry. Building it
//                      here would put a second particle system in the geometry
//                      path.
//     chain         -> a tube or ribbon, or repeated meshlet instances
//     spline wall   -> a ribbon or tube use
//     low cone      -> a radial fan or shell use
//
// So there are six `family` encodings and there is no seventh. A job naming one
// is REFUSED -- the deleted four are not reserved for later, they are things
// this block must not grow back.
//
// ---------------------------------------------------------------------------
// THE ORDER IS THE CONTRACT
// ---------------------------------------------------------------------------
//   > A vertex stream to GEOM.SETUP, in a declared deterministic order --
//   > because two orderings of the same primitive produce the same picture but
//   > different capture CRCs, and the capture is the contract.
//
// That sentence is why this block emits indices in one fixed walk and why the
// test checks the ORDER and not merely the set. A generator that emitted the
// same triangles in a different sequence would look identical on screen and
// break every capture in the repository.
//
// ---------------------------------------------------------------------------
// TOPOLOGY ONLY
// ---------------------------------------------------------------------------
// This block decides WHICH vertices form which triangles. It does not place
// them: positions come from `params` through the evaluator, and a topology that
// depended on a position would stop being bounded.
//
// The per-family segment and side counts below are an INTERPRETATION where the
// contract names a family and not a mesh. They are pinned by the one number the
// contract does give -- "worst case per primitive: 64 x 8 = 512 quads = 1,024
// triangles" -- which is `segments * sides` quads, so tube and shell are the
// two-dimensional families and the rest collapse one axis.
// ---------------------------------------------------------------------------
`default_nettype none

module zhao_forge_prim #(
    parameter int unsigned MAX_SEGMENTS = 64,
    parameter int unsigned MAX_SIDES    = 8
) (
    input var logic clk,
    input var logic rst_n,

    // ---- job -----------------------------------------------------------------
    input  var logic         j_valid_i,
    output var logic         j_ready_o,
    input  var logic [2:0]   j_family_i,
    input  var logic [6:0]   j_segments_i,     // 1..MAX_SEGMENTS
    input  var logic [3:0]   j_sides_i,        // 1..MAX_SIDES
    input  var logic [15:0]  j_material_i,
    input  var logic [1:0]   j_view_mask_i,
    input  var logic [15:0]  j_src_id_i,
    input  var logic [1:0]   view_sel_i,

    // ---- the index stream ----------------------------------------------------
    output var logic         t_valid_o,
    input  var logic         t_ready_i,
    output var logic [15:0]  t_i0_o,
    output var logic [15:0]  t_i1_o,
    output var logic [15:0]  t_i2_o,
    output var logic [15:0]  t_material_o,
    output var logic [15:0]  t_src_id_o,
    output var logic         t_last_o,

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0]  jobs_o,
    output var logic [31:0]  triangles_o,
    output var logic [31:0]  refused_family_o,   // a deleted or unknown family
    output var logic [31:0]  refused_limit_o,    // segments or sides out of range
    output var logic [31:0]  skipped_view_o
);

  // The six, and only the six.
  localparam logic [2:0] FAM_RIBBON    = 3'd0;
  localparam logic [2:0] FAM_FAN       = 3'd1;   // radial fan / ring
  localparam logic [2:0] FAM_TUBE      = 3'd2;
  localparam logic [2:0] FAM_SHELL     = 3'd3;   // radial shell
  localparam logic [2:0] FAM_BILLBOARD = 3'd4;
  localparam logic [2:0] FAM_CLIFF     = 3'd5;   // terrain cliff / skirt

  // ---- effective grid ------------------------------------------------------
  // Every family is a (segments x sides) grid of quads; the one-dimensional
  // ones pin an axis to 1 rather than being a separate walk. That is what makes
  // this ONE generator instead of six, and it is why the worst case is the
  // product the contract states.
  logic [6:0] eff_seg_c;
  logic [3:0] eff_side_c;
  always_comb begin
    unique case (j_family_i)
      FAM_RIBBON:    begin eff_seg_c = j_segments_i; eff_side_c = 4'd1; end
      FAM_CLIFF:     begin eff_seg_c = j_segments_i; eff_side_c = 4'd1; end
      FAM_FAN:       begin eff_seg_c = 7'd1;         eff_side_c = j_sides_i; end
      FAM_TUBE:      begin eff_seg_c = j_segments_i; eff_side_c = j_sides_i; end
      FAM_SHELL:     begin eff_seg_c = j_segments_i; eff_side_c = j_sides_i; end
      FAM_BILLBOARD: begin eff_seg_c = 7'd1;         eff_side_c = 4'd1; end
      default:       begin eff_seg_c = 7'd0;         eff_side_c = 4'd0; end
    endcase
  end

  logic family_bad_c, limit_bad_c, for_view_c;
  assign family_bad_c = (j_family_i > FAM_CLIFF);
  assign limit_bad_c  = (j_segments_i == 7'd0) || (j_sides_i == 4'd0) ||
                        (j_segments_i > 7'(MAX_SEGMENTS)) ||
                        (j_sides_i > 4'(MAX_SIDES));
  assign for_view_c   = (j_view_mask_i & view_sel_i) != 2'd0;

  // ---- the walk ------------------------------------------------------------
  logic        busy_q;
  logic [6:0]  seg_q;
  logic [3:0]  side_q;
  logic        tri_q;          // 0 = first triangle of the quad, 1 = second
  logic [6:0]  nseg_q;
  logic [3:0]  nside_q;
  logic [3:0]  ring_q;         // vertices per ring = sides (+1 when open)
  logic [15:0] mat_q, src_q;

  assign j_ready_o = !busy_q;

  // A ring of `sides` vertices closes on itself for tube, shell and fan; a
  // ribbon, cliff or billboard is OPEN and needs one more vertex. Closing a
  // ribbon would weld its two edges together.
  logic closed_c;
  assign closed_c = (j_family_i == FAM_TUBE) || (j_family_i == FAM_SHELL) ||
                    (j_family_i == FAM_FAN);

  // index of vertex (s, k) in ring-major order
  function automatic logic [15:0] vidx(input logic [6:0] s, input logic [4:0] k,
                                       input logic [3:0] ring);
    vidx = 16'(s) * 16'(ring) + 16'(k);
  endfunction

  logic [4:0] k0_c, k1_c;
  assign k0_c = {1'b0, side_q};
  // The next vertex around the ring wraps for a closed family and does not for
  // an open one -- which is the whole difference between a tube and a ribbon.
  assign k1_c = (side_q + 4'd1 == nside_q && ring_q == nside_q)
              ? 5'd0 : {1'b0, side_q} + 5'd1;

  logic last_c;
  assign last_c = busy_q && tri_q &&
                  (side_q + 4'd1 == nside_q) && (seg_q + 7'd1 == nseg_q);

  assign t_valid_o    = busy_q;
  assign t_material_o = mat_q;
  assign t_src_id_o   = src_q;
  assign t_last_o     = last_c;

  // Two triangles per quad, in a FIXED winding. The order is the contract.
  always_comb begin
    if (!tri_q) begin
      t_i0_o = vidx(seg_q,        k0_c, ring_q);
      t_i1_o = vidx(seg_q,        k1_c, ring_q);
      t_i2_o = vidx(seg_q + 7'd1, k0_c, ring_q);
    end else begin
      t_i0_o = vidx(seg_q,        k1_c, ring_q);
      t_i1_o = vidx(seg_q + 7'd1, k1_c, ring_q);
      t_i2_o = vidx(seg_q + 7'd1, k0_c, ring_q);
    end
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      busy_q           <= 1'b0;
      jobs_o           <= '0;
      triangles_o      <= '0;
      refused_family_o <= '0;
      refused_limit_o  <= '0;
      skipped_view_o   <= '0;
    end else begin
      if (j_valid_i && j_ready_o) begin
        jobs_o <= jobs_o + 32'd1;
        if (family_bad_c) begin
          // The four deleted families are not reserved for later. A job naming
          // one is a caller that has not read the ruling.
          refused_family_o <= refused_family_o + 32'd1;
        end else if (limit_bad_c) begin
          refused_limit_o <= refused_limit_o + 32'd1;
        end else if (!for_view_c) begin
          skipped_view_o <= skipped_view_o + 32'd1;
        end else begin
          busy_q  <= 1'b1;
          seg_q   <= 7'd0;
          side_q  <= 4'd0;
          tri_q   <= 1'b0;
          nseg_q  <= eff_seg_c;
          nside_q <= eff_side_c;
          ring_q  <= closed_c ? eff_side_c : (eff_side_c + 4'd1);
          mat_q   <= j_material_i;
          src_q   <= j_src_id_i;
        end
      end

      if (busy_q && t_ready_i) begin
        triangles_o <= triangles_o + 32'd1;
        if (!tri_q) begin
          tri_q <= 1'b1;
        end else begin
          tri_q <= 1'b0;
          if (side_q + 4'd1 != nside_q) begin
            side_q <= side_q + 4'd1;
          end else begin
            side_q <= 4'd0;
            if (seg_q + 7'd1 != nseg_q) seg_q <= seg_q + 7'd1;
            else busy_q <= 1'b0;
          end
        end
      end
    end
  end

endmodule : zhao_forge_prim

`default_nettype wire
