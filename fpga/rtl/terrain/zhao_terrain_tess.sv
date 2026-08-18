// zhao_terrain_tess.sv — TERRAIN.TESS: the crack-safe tessellator (phase 6,
// ZH-034).
//
// Law, in citation order:
//   design/contracts/TERRAIN.TESS.md — the block contract.
//   design/blocks.yml — `inputs: [patch_state, lod_target]`, `outputs:
//       [terrain_mesh]`, `latency: variable`, "1 emitted vertex per clock",
//       counter `terrain_triangles_emitted`, and the note "Crack-safety is a
//       formal candidate (stitch invariants)".
//   spec/terrain_rules.md §4.3 — the FIXED i00-i11 diagonal and the emit order,
//       which §4.3 itself pins to `draw_heightfield`: (i00,i11,i10) then
//       (i00,i01,i11). §3.2/§3.5 — void columns emit no surface and a breach
//       shows sky. §5 — the underside is the same diagonal with INVERTED
//       winding, and the crack law ("along any rim boundary the underside LOD
//       must equal the top LOD on that edge").
//   reference/src/zrender/terrain.cpp — "y-up winding: e1 x e2 = +Y for a flat
//       cell", the sign TERRAIN.NORMALS already depends on, and the underside's
//       "inverted relative to the top: normal points DOWN", which that file
//       labels "TERRAIN.TESS law" in as many words.
//   spec/qformats.md §3 (fx_mul is ONE rescale(.,16)), §4 (round-half-up).
//   charter §11.1 — 32x32 cells, sixteen 8x8-cell subpatches, crack-safe grid
//       resolutions, precomputed border stitch patterns, geomorph between
//       levels.
//   reference/include/zref/zref_terrain_tess.hpp — the oracle.
//
// ---------------------------------------------------------------------------
// THE WINDING, AND WHY IT IS NOT NEGOTIABLE
// ---------------------------------------------------------------------------
// The top pair is (i00, i11, i10) then (i00, i01, i11). For a flat cell the
// first edge pair crosses to +Y, so the flat-shade normal points UP and the
// island top lights. TERRAIN.NORMALS computes exactly that cross product and
// does NOT normalise or absolute it, so if this block flipped a winding the two
// blocks would disagree about which way the island faces and every lit patch
// would go dark. The underside is the same pair with b and c swapped: one mux,
// in one place, so the inversion cannot drift.
//
// ---------------------------------------------------------------------------
// LEVELS AND EDGE STRIDES
// ---------------------------------------------------------------------------
// A subpatch is 8x8 cells (charter §11.1). Its edge must land on lattice
// vertices, so a stride must DIVIDE 8; the divisors of 8 are exactly
// {1, 2, 4, 8}. The level set is therefore FORCED, not chosen — what is chosen
// is only the encoding, level 0..3 with stride = 1 << level, which also makes
// "a lod_target above the legal resolution set" UNREPRESENTABLE on the wire
// rather than something to clamp and count.
//
// edge_stride[side] = 1 << max(own_level, neighbour_level[side]).
//
// That is symmetric, so two neighbours independently compute the SAME stride
// for the edge they share, and the shared-edge vertex SETS are identical — the
// contract's crack invariant, by construction rather than by testing.
// ENFORCED-BY: tests/terrain/terrain_tess_directed.cpp:on_x_line
// (all 16 level pairs, checked on the vertex sets two independently-run
// subpatches actually emit onto their shared line — not on this paragraph).
// The rejected alternative was vertex snapping (the finer side keeps its extra
// boundary vertices and slides them onto the coarse segment): it leaves
// T-junctions, and a fixed-point rasterizer cracks at a T-junction by a pixel.
// That is the 2026-08-15 seam-crack defect class recorded in
// design/contracts/GEOM.CLIP.md, and paying for it again to save a state
// machine would be a bad trade.
//
// ---------------------------------------------------------------------------
// THE TWO PATHS
// ---------------------------------------------------------------------------
// UNSTITCHED (every edge stride equals the own stride): run-cells in z-then-x
// scan order, the §4.3 pair each. At level 0 this is byte-for-byte
// `draw_heightfield`'s cell emission, which is what makes "RTL == oracle" mean
// "RTL == the geometry the golden captures already pin".
//
// STITCHED (any neighbour coarser): an ANNULUS. The inner (n-2)x(n-2) block of
// run-cells is emitted plain; the ring one run-cell deep is triangulated by
// walking the OUTER boundary — whose vertices are only the per-side COARSE ones,
// the whole point — clockwise from the subpatch corner nearest the origin, and
// fanning each outer segment onto the stretch of the inner rectangle it
// projects to.
//
// The annulus was chosen because it has NO CORNER CASE. A subpatch corner is a
// legal vertex on both its sides' coarse sets (index 0 of each), so a fan
// anchored there never asks for a vertex the neighbour does not have. The
// obvious cheaper shape — a one-cell strip per side — fails exactly there: when
// two adjacent sides are both coarsened, the corner square's other two corners
// lie on the two boundary lines and NEITHER is in its side's coarse set. That
// is the "stitch case matrix squares" problem terrain_rules §3.1(d) names.
//
// n == 2 (level 2 beside a level-3 neighbour) degenerates cleanly: the inner
// rectangle collapses to the centre vertex and the ring becomes a fan from it.
// n == 1 (level 3) can never be stitched, because 8 is the coarsest stride.
//
// ---------------------------------------------------------------------------
// LAWS CHOSEN, NOT FOUND (each also argued in the contract and the oracle)
// ---------------------------------------------------------------------------
// 1. THE ANNULUS ITSELF and the level encoding, above.
// 2. VOID AT STRIDE > 1: a run-cell is emitted iff EVERY one of the stride x
//    stride patch cells it covers is SOLID. REJECTED ALTERNATIVE: emit if any
//    is solid, which roofs over a breach — and terrain_rules §3.5 says what you
//    see through a breach is sky. The cost is stated: at a coarse level one
//    breached cell erases up to 8x8 cells of surface, which is TERRAIN.LOD's
//    problem to avoid (§4.4 keeps moved ground fine anyway).
// 3. A COARSENED SUBPATCH CONTAINING A VOID CELL IS REJECTED, loudly, counted
//    in `subpatch_rejected_o` with `job_reject_o`. The ring's fans are not
//    aligned to run-cells, so honouring a void inside one would need a
//    conservative bounding-box cell scan per fan — up to 64 reads for a fan
//    that emits two triangles — to buy geometry no projected-error LOD selector
//    should ask for. REJECTED ALTERNATIVES: roofing over the hole (silent and
//    wrong), or the per-fan scan (expensive and still conservative). The
//    obligation this creates is stated rather than hidden: TERRAIN.LOD must not
//    coarsen a neighbour past a subpatch that carries void cells.
// 4. GEOMORPH APPLIES ONLY STRICTLY INSIDE THE SUBPATCH. A boundary vertex is
//    shared with a neighbour that has its own morph factor, and `lod_target`
//    carries one factor plus four neighbour LEVELS — it cannot express the
//    neighbour's factor at all. Leaving boundary vertices unmorphed makes
//    crack-safety hold unconditionally for every level pair and every factor
//    pair. The cost is stated: during a transition the interior moves while the
//    border does not, so the border reads as a shallow crease bounded by the
//    level's own height deviation. REJECTED ALTERNATIVE: morphing boundary
//    vertices, which is the textbook form and looks strictly better — and which
//    cannot be implemented correctly until `lod_target` gains a per-edge morph
//    factor. THAT IS A CONTRACT GAP, recorded as one, not an RTL decision.
// 5. The morph TARGET is DERIVED, not chosen: it is §4.3's own interpolation of
//    the next-coarser cell at the vertex, which collapses to
//    `ha + rescale(hb - ha, 1)` at u = v = 1/2. The derivation is written out in
//    the oracle and PROVED by `terrain_tess_directed`, which evaluates
//    `zref::terrain::column_query` on the coarse cell and requires agreement.
//
// NOT IN THIS BLOCK, deliberately: rim walls (FORGE.CLIFF), normals
// (TERRAIN.NORMALS), LOD decisions (TERRAIN.LOD decides, this block obeys), and
// per-vertex UV. The reference computes terrain UV in its DRAW loop from a
// patch-level `top_shift`, and the mirrored-repeat fold (terrain_rules §6.2) is
// the TMU sampler's, not the tessellator's; emitting UV here would fix an
// underside/wall UV law that FORGE.CLIFF owns.
//
// Conservative SystemVerilog subset only (charter §2).

module zhao_terrain_tess (
    input logic clk,
    input logic rst_n,

    // -----------------------------------------------------------------------
    // patch_state + lod_target: one subpatch of one surface
    // -----------------------------------------------------------------------
    input  logic        job_valid_i,
    output logic        job_ready_o,
    input  logic [ 5:0] job_ox_i,        // subpatch cell origin x, multiple of 8
    input  logic [ 5:0] job_oz_i,        // subpatch cell origin z, multiple of 8
    input  logic [ 1:0] job_level_i,     // own level; stride = 1 << level
    input  logic [ 1:0] job_lvl_nz_i,    // neighbour levels, terrain_rules §6.6
    input  logic [ 1:0] job_lvl_pz_i,    //   side order: -z, +z, -x, +x
    input  logic [ 1:0] job_lvl_nx_i,
    input  logic [ 1:0] job_lvl_px_i,
    input  logic [16:0] job_morph_i,     // geomorph factor, Q16; > 65536 clamps
    input  logic        job_surface_i,   // 0 = top, 1 = underside
    input  logic        job_dual_i,      // 0 = legacy single-surface page
    input  logic [15:0] job_src_id_i,

    // -----------------------------------------------------------------------
    // lattice read port: registered, data valid the cycle AFTER the request
    // -----------------------------------------------------------------------
    output logic               lat_req_o,
    output logic        [ 5:0] lat_vi_o,
    output logic        [ 5:0] lat_vj_o,
    output logic               lat_surface_o,  // which height plane to return
    input  logic signed [31:0] lat_h_i,        // the selected plane, fx16
    input  logic signed [31:0] lat_wx_i,       // placed world x of lat_vi_o
    input  logic signed [31:0] lat_wz_i,       // placed world z of lat_vj_o

    // -----------------------------------------------------------------------
    // cell-state read port: registered, one cycle, layer D substance bits
    // -----------------------------------------------------------------------
    output logic       cs_req_o,
    output logic [4:0] cs_ci_o,
    output logic [4:0] cs_cj_o,
    input  logic [1:0] cs_substance_i,  // 0 = SOLID (terrain_rules §3.3)

    // -----------------------------------------------------------------------
    // terrain_mesh out — exactly TERRAIN.NORMALS' input packet
    // -----------------------------------------------------------------------
    output logic               tri_valid_o,
    input  logic               tri_ready_i,
    output logic signed [31:0] ax_o,
    output logic signed [31:0] ay_o,
    output logic signed [31:0] az_o,
    output logic signed [31:0] bx_o,
    output logic signed [31:0] by_o,
    output logic signed [31:0] bz_o,
    output logic signed [31:0] cx_o,
    output logic signed [31:0] cy_o,
    output logic signed [31:0] cz_o,
    output logic               surface_o,
    output logic        [15:0] src_id_o,

    output logic [31:0] terrain_triangles_emitted_o,
    output logic [31:0] subpatch_rejected_o,
    output logic [31:0] lod_clamped_o,
    output logic        job_reject_o,  // 1-cycle pulse with subpatch_rejected_o
    output logic        idle_o
);

  localparam int unsigned SubCells = 8;  // charter §11.1

  localparam logic [1:0] StIdle = 2'd0;
  localparam logic [1:0] StScan = 2'd1;
  localparam logic [1:0] StTri = 2'd2;

  // ---- §3/§4 arithmetic ----------------------------------------------------

  function automatic logic signed [31:0] fx_add_sat(input logic signed [32:0] a,
                                                    input logic signed [32:0] b);
    logic signed [33:0] s;
    begin
      s = $signed({a[32], a}) + $signed({b[32], b});
      if (s > 34'sd2147483647) fx_add_sat = 32'sh7FFF_FFFF;
      else if (s < -34'sd2147483648) fx_add_sat = 32'sh8000_0000;
      else fx_add_sat = s[31:0];
    end
  endfunction

  // rescale(x, 1): round-half-up shift by one, then saturate to the fx16 word.
  function automatic logic signed [31:0] rescale1(input logic signed [33:0] x);
    logic signed [33:0] r;
    begin
      r = (x + 34'sd1) >>> 1;
      if (r > 34'sd2147483647) rescale1 = 32'sh7FFF_FFFF;
      else if (r < -34'sd2147483648) rescale1 = 32'sh8000_0000;
      else rescale1 = r[31:0];
    end
  endfunction

  // rescale(x, 16): the fx_mul narrow.
  function automatic logic signed [31:0] rescale16(input logic signed [51:0] x);
    logic signed [51:0] r;
    begin
      r = (x + 52'sd32768) >>> 16;
      if (r > 52'sd2147483647) rescale16 = 32'sh7FFF_FFFF;
      else if (r < -52'sd2147483648) rescale16 = 32'sh8000_0000;
      else rescale16 = r[31:0];
    end
  endfunction

  // ---- job state -----------------------------------------------------------
  logic [ 5:0] j_ox, j_oz;
  logic [ 1:0] j_level;
  logic [ 3:0] j_s;  // stride, 1/2/4/8
  logic [ 3:0] j_n;  // run-cells per side, 8/4/2/1
  logic [ 5:0] j_scmask;  // (2*stride) - 1, for the "coarse vertex?" test
  logic [ 1:0] j_lvl_ord[4];  // edge LEVELS in CW order: -x, +z, +x, -z
  logic [ 3:0] j_m_ord[4];  // segments per side, 8 >> lvl_ord
  logic        j_stitch;
  logic [16:0] j_morph;
  logic        j_surface;
  logic [15:0] j_src;
  logic [ 3:0] j_u;  // n - 2 (0 when n <= 2)
  logic [ 5:0] j_P;  // inner-ring perimeter, 4u (1 when u == 0)

  logic [ 1:0] st;
  logic [63:0] solid;  // the subpatch's 8x8 solidity, read once

  // ---- the 8x8 cell-state pre-scan ----------------------------------------
  // Read ONCE per job rather than per run-cell: 65 cycles, after which every
  // per-run-cell void test is combinational. `substance` 0 is SOLID (§3.3).
  logic [6:0] sc_idx;  // 0..64: 64 issues then one capture cycle
  logic       sc_pend;
  logic [5:0] sc_pend_idx;

  assign cs_req_o = (st == StScan) && (sc_idx < 7'd64);
  assign cs_ci_o  = 5'(j_ox + {2'b0, sc_idx[2:0]});
  assign cs_cj_o  = 5'(j_oz + {2'b0, sc_idx[5:3]});

  // ---- the emission enumerator --------------------------------------------
  localparam logic [1:0] EmPlain = 2'd0;  // unstitched run-cells
  localparam logic [1:0] EmInner = 2'd1;  // the annulus's inner block
  localparam logic [1:0] EmFan = 2'd2;  // the annulus's ring fans

  logic [1:0] emode;
  logic [3:0] ea, eb;  // run-cell indices
  logic       etri;  // 0/1 within a run-cell
  logic [1:0] eside;  // 0..3, CW order
  logic [3:0] eg;  // segment within the side
  logic [5:0] efan;  // 0 = the segment's own triangle, then the fan
  logic       done;  // the enumerator has run out

  // ---- the per-triangle vertex fetch --------------------------------------
  logic [1:0] f_slot;  // 0..2
  logic [1:0] f_kind;  // 0 = the vertex, 1 = coarse parent A, 2 = parent B

  logic pend_v;
  logic [1:0] pend_slot, pend_kind;
  logic pend_last;

  logic signed [31:0] vx[3], vz[3], vh[3], vy[3];
  logic signed [31:0] v_ha;  // parent A of the slot being fetched

  // ---- output register -----------------------------------------------------
  logic o_valid;
  logic signed [31:0] o_ax, o_ay, o_az, o_bx, o_by, o_bz, o_cx, o_cy, o_cz;
  logic [15:0] o_src;
  logic o_surf;

  wire out_busy = o_valid && !tri_ready_i;

  // =========================================================================
  // combinational geometry
  // =========================================================================

  // is every patch cell under run-cell (ea, eb) SOLID?
  logic cell_solid;
  always_comb begin
    cell_solid = 1'b1;
    for (int cj = 0; cj < int'(SubCells); cj++) begin
      for (int ci = 0; ci < int'(SubCells); ci++) begin
        if (ci >= int'(ea) * int'(j_s) && ci < int'(ea) * int'(j_s) + int'(j_s) &&
            cj >= int'(eb) * int'(j_s) && cj < int'(eb) * int'(j_s) + int'(j_s)) begin
          if (!solid[cj*8+ci]) cell_solid = 1'b0;
        end
      end
    end
  end

  // the inner rectangle
  wire [5:0] x_lo = j_ox + {2'b0, j_s};
  wire [5:0] x_hi = j_ox + 6'd8 - {2'b0, j_s};
  wire [5:0] z_lo = j_oz + {2'b0, j_s};
  wire [5:0] z_hi = j_oz + 6'd8 - {2'b0, j_s};

  // W[t]: the inner ring, clockwise from the corner nearest the origin.
  function automatic logic [11:0] inner_v(input logic [5:0] t);
    logic [5:0] vi, vj;
    logic [5:0] tt;
    begin
      if (j_u == 4'd0) begin
        vi = x_lo;
        vj = z_lo;
      end else begin
        tt = t;
        if (tt < {2'b0, j_u}) begin
          vi = x_lo;
          vj = z_lo + ((tt) << j_level);
        end else if (tt < {1'b0, j_u, 1'b0}) begin
          vi = x_lo + ((tt - {2'b0, j_u}) << j_level);
          vj = z_hi;
        end else if (tt < ({2'b0, j_u} + {1'b0, j_u, 1'b0})) begin
          vi = x_hi;
          vj = z_hi - ((tt - {1'b0, j_u, 1'b0}) << j_level);
        end else begin
          vi = x_hi - ((tt - {2'b0, j_u} - {1'b0, j_u, 1'b0}) << j_level);
          vj = z_lo;
        end
      end
      inner_v = {vj, vi};
    end
  endfunction

  // the outer ring vertex of side k (CW order) at segment index g
  function automatic logic [11:0] outer_v(input logic [1:0] k, input logic [3:0] g);
    logic [5:0] step, vi, vj;
    begin
      step = 6'(({2'b0, g}) << j_lvl_ord[k]);
      case (k)
        2'd0: begin  // -x, going +z
          vi = j_ox;
          vj = j_oz + step;
        end
        2'd1: begin  // +z, going +x
          vi = j_ox + step;
          vj = j_oz + 6'd8;
        end
        2'd2: begin  // +x, going -z
          vi = j_ox + 6'd8;
          vj = j_oz + 6'd8 - step;
        end
        default: begin  // -z, going -x
          vi = j_ox + 6'd8 - step;
          vj = j_oz;
        end
      endcase
      outer_v = {vj, vi};
    end
  endfunction

  // project an outer vertex onto the inner ring: the same position clamped into
  // the inner rectangle. Exact, because every stride divides 8.
  function automatic logic [5:0] proj_t(input logic [1:0] k, input logic [3:0] g);
    logic [5:0] a, c, t;
    begin
      if (j_u == 4'd0) proj_t = 6'd0;
      else begin
        a = 6'((({2'b0, g}) << j_lvl_ord[k]) >> j_level);
        if (a == 6'd0) c = 6'd0;
        else if ((a - 6'd1) > {2'b0, j_u}) c = {2'b0, j_u};
        else c = a - 6'd1;
        t = 6'({2'b0, k} * {2'b0, j_u}) + c;
        proj_t = (t >= j_P) ? (t - j_P) : t;
      end
    end
  endfunction

  // the segment following (eside, eg)
  wire       seg_wrap = (eg + 4'd1) >= j_m_ord[eside];
  wire [1:0] nside = seg_wrap ? (eside + 2'd1) : eside;
  wire [3:0] ng = seg_wrap ? 4'd0 : (eg + 4'd1);

  wire [11:0] fan_v0 = outer_v(eside, eg);
  wire [11:0] fan_v1 = outer_v(nside, ng);
  wire [ 5:0] fan_t0 = proj_t(eside, eg);
  wire [ 5:0] fan_t1 = proj_t(nside, ng);
  // steps = (t1 - t0) mod P. P <= 24, so 5 bits carry it.
  wire [ 5:0] fan_steps = (j_u == 4'd0)  ? 6'd0
                          : (fan_t1 >= fan_t0) ? (fan_t1 - fan_t0)
                                               : (fan_t1 + j_P - fan_t0);

  wire [5:0] fan_sum = fan_t0 + efan;
  wire [5:0] fan_ta = (fan_sum >= j_P) ? (fan_sum - j_P) : fan_sum;
  wire [5:0] fan_sub = fan_sum - 6'd1;
  wire [5:0] fan_tb = (fan_sub >= j_P) ? (fan_sub - j_P) : fan_sub;

  // the three lattice vertices of the current triangle, in TOP order
  logic [5:0] tv_i[3], tv_j[3];
  always_comb begin
    logic [5:0] i0, j0;
    logic [11:0] wa, wb, w1;
    // Defaults first: every branch below overwrites what it uses, and a
    // combinational block that leaves a signal unassigned on any path infers a
    // latch (Verilator -Wall says so, and it is right).
    for (int p = 0; p < 3; p++) begin
      tv_i[p] = 6'd0;
      tv_j[p] = 6'd0;
    end
    w1 = 12'd0;
    wa = 12'd0;
    wb = 12'd0;
    i0 = j_ox + 6'(({2'b0, ea}) << j_level);
    j0 = j_oz + 6'(({2'b0, eb}) << j_level);
    if (emode == EmFan) begin
      w1 = inner_v(fan_t1);
      wa = inner_v(fan_ta);
      wb = inner_v(fan_tb);
      tv_i[0] = fan_v0[5:0];
      tv_j[0] = fan_v0[11:6];
      if (efan == 6'd0) begin
        tv_i[1] = fan_v1[5:0];
        tv_j[1] = fan_v1[11:6];
        tv_i[2] = w1[5:0];
        tv_j[2] = w1[11:6];
      end else begin
        tv_i[1] = wa[5:0];
        tv_j[1] = wa[11:6];
        tv_i[2] = wb[5:0];
        tv_j[2] = wb[11:6];
      end
    end else begin
      // the §4.3 pair: (i00, i11, i10) then (i00, i01, i11)
      tv_i[0] = i0;
      tv_j[0] = j0;
      if (!etri) begin
        tv_i[1] = i0 + {2'b0, j_s};
        tv_j[1] = j0 + {2'b0, j_s};
        tv_i[2] = i0 + {2'b0, j_s};
        tv_j[2] = j0;
      end else begin
        tv_i[1] = i0;
        tv_j[1] = j0 + {2'b0, j_s};
        tv_i[2] = i0 + {2'b0, j_s};
        tv_j[2] = j0 + {2'b0, j_s};
      end
    end
  end

  // ---- the geomorph case of each slot -------------------------------------
  // 0 = none (the coarse level already carries this vertex), 1 = x-midpoint,
  // 2 = z-midpoint, 3 = the coarse cell's diagonal midpoint. Boundary vertices
  // NEVER morph (chosen law 4).
  function automatic logic [1:0] mcase_f(input logic [5:0] vi, input logic [5:0] vj);
    logic xc, zc;
    begin
      if (j_morph == 17'd0) mcase_f = 2'd0;
      else if (vi <= j_ox || vi >= (j_ox + 6'd8) || vj <= j_oz || vj >= (j_oz + 6'd8))
        mcase_f = 2'd0;
      else begin
        xc = (vi & j_scmask) == 6'd0;
        zc = (vj & j_scmask) == 6'd0;
        if (xc && zc) mcase_f = 2'd0;
        else if (zc) mcase_f = 2'd1;
        else if (xc) mcase_f = 2'd2;
        else mcase_f = 2'd3;
      end
    end
  endfunction

  logic [1:0] mc[3];
  always_comb begin
    for (int p = 0; p < 3; p++) mc[p] = mcase_f(tv_i[p], tv_j[p]);
  end

  // the address of the read being issued
  logic [5:0] rd_vi, rd_vj;
  always_comb begin
    logic [5:0] bi, bj;
    bi = tv_i[f_slot];
    bj = tv_j[f_slot];
    rd_vi = bi;
    rd_vj = bj;
    if (f_kind != 2'd0) begin
      // parent A on kind 1, parent B on kind 2
      case (mc[f_slot])
        2'd1: rd_vi = (f_kind == 2'd1) ? (bi - {2'b0, j_s}) : (bi + {2'b0, j_s});
        2'd2: rd_vj = (f_kind == 2'd1) ? (bj - {2'b0, j_s}) : (bj + {2'b0, j_s});
        2'd3: begin
          rd_vi = (f_kind == 2'd1) ? (bi - {2'b0, j_s}) : (bi + {2'b0, j_s});
          rd_vj = (f_kind == 2'd1) ? (bj - {2'b0, j_s}) : (bj + {2'b0, j_s});
        end
        default: ;
      endcase
    end
  end

  // is the read being issued the LAST of this triangle?
  wire iss_last = (f_slot == 2'd2) && ((f_kind == 2'd0 && mc[2] == 2'd0) || (f_kind == 2'd2));
  // Skipping a void run-cell costs one cycle and issues nothing.
  wire cell_skip = (emode != EmFan) && !cell_solid;
  // the run-cell index bounds of the current mode: the whole subpatch on the
  // unstitched path, the annulus's inner block on the stitched one
  wire [3:0] cell_lo = (emode == EmInner) ? 4'd1 : 4'd0;
  wire [3:0] cell_hi = (emode == EmInner) ? (j_n - 4'd2) : (j_n - 4'd1);
  wire want_issue = (st == StTri) && !done && !cell_skip;
  wire do_issue = want_issue && !(iss_last && out_busy);

  assign lat_req_o = do_issue;
  assign lat_vi_o = rd_vi;
  assign lat_vj_o = rd_vj;
  assign lat_surface_o = j_surface;

  // ---- the geomorph blend, evaluated on the parent-B capture --------------
  // hc is §4.3's interpolation of the coarse cell at this vertex, which at
  // u = v = 1/2 collapses to ha + rescale(hb - ha, 1) — ONE round-half-up, the
  // same single rounding column_query performs over its common denominator.
  wire signed [33:0] m_dab = {{2{lat_h_i[31]}}, lat_h_i} - {{2{v_ha[31]}}, v_ha};
  wire signed [31:0] m_half = rescale1(m_dab);
  wire signed [31:0] m_hc = fx_add_sat({v_ha[31], v_ha}, {m_half[31], m_half});
  // y = h + fx_mul(morph, hc - h): §4.3's shape, an exact add of a rounded
  // delta. morph = 0 gives h and morph = 65536 gives hc, both bit-exactly.
  wire signed [33:0] m_d = {{2{m_hc[31]}}, m_hc} - {{2{vh[pend_slot][31]}}, vh[pend_slot]};
  wire signed [51:0] m_prod = $signed({1'b0, j_morph}) * m_d;
  wire signed [31:0] m_step = rescale16(m_prod);
  wire signed [31:0] m_y = fx_add_sat({vh[pend_slot][31], vh[pend_slot]},
                                      {m_step[31], m_step});

  // the slot-2 values at the emit cycle: registers unless slot 2's last read is
  // landing right now (it always is — that is what `pend_last` means)
  wire signed [31:0] last_x = (pend_kind == 2'd0) ? lat_wx_i : vx[2];
  wire signed [31:0] last_z = (pend_kind == 2'd0) ? lat_wz_i : vz[2];
  wire signed [31:0] last_y = (pend_kind == 2'd0) ? lat_h_i : m_y;

  // =========================================================================
  // sequential
  // =========================================================================
  assign job_ready_o = (st == StIdle);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st <= StIdle;
      j_ox <= '0;
      j_oz <= '0;
      j_level <= '0;
      j_s <= 4'd1;
      j_n <= 4'd8;
      j_scmask <= 6'd1;
      j_stitch <= 1'b0;
      j_morph <= '0;
      j_surface <= 1'b0;
      j_src <= '0;
      j_u <= '0;
      j_P <= 6'd1;
      for (int k = 0; k < 4; k++) begin
        j_lvl_ord[k] <= '0;
        j_m_ord[k]   <= 4'd8;
      end
      solid <= '0;
      sc_idx <= '0;
      sc_pend <= 1'b0;
      sc_pend_idx <= '0;
      emode <= EmPlain;
      ea <= '0;
      eb <= '0;
      etri <= 1'b0;
      eside <= '0;
      eg <= '0;
      efan <= '0;
      done <= 1'b1;
      f_slot <= '0;
      f_kind <= '0;
      pend_v <= 1'b0;
      pend_slot <= '0;
      pend_kind <= '0;
      pend_last <= 1'b0;
      v_ha <= '0;
      for (int p = 0; p < 3; p++) begin
        vx[p] <= '0;
        vz[p] <= '0;
        vh[p] <= '0;
        vy[p] <= '0;
      end
      o_valid <= 1'b0;
      o_ax <= '0;
      o_ay <= '0;
      o_az <= '0;
      o_bx <= '0;
      o_by <= '0;
      o_bz <= '0;
      o_cx <= '0;
      o_cy <= '0;
      o_cz <= '0;
      o_src <= '0;
      o_surf <= 1'b0;
      terrain_triangles_emitted_o <= '0;
      subpatch_rejected_o <= '0;
      lod_clamped_o <= '0;
      job_reject_o <= 1'b0;
    end else begin
      job_reject_o <= 1'b0;
      if (o_valid && tri_ready_i) o_valid <= 1'b0;

      case (st)
        StIdle: begin
          if (job_valid_i) begin
            automatic logic [1:0] lv_nx, lv_pz, lv_px, lv_nz;
            automatic logic [3:0] s_new, n_new;
            automatic logic stitch_new;
            j_ox <= job_ox_i;
            j_oz <= job_oz_i;
            j_level <= job_level_i;
            s_new = 4'(4'd1 << job_level_i);
            n_new = 4'(4'd8 >> job_level_i);
            j_s <= s_new;
            j_n <= n_new;
            j_scmask <= 6'({2'b0, s_new} << 1) - 6'd1;
            j_u <= (n_new >= 4'd2) ? (n_new - 4'd2) : 4'd0;
            j_P <= (n_new >= 4'd3) ? 6'({2'b0, n_new - 4'd2} << 2) : 6'd1;
            // edge LEVELS = max(own, neighbour), in CW order: -x, +z, +x, -z
            lv_nx = (job_lvl_nx_i > job_level_i) ? job_lvl_nx_i : job_level_i;
            lv_pz = (job_lvl_pz_i > job_level_i) ? job_lvl_pz_i : job_level_i;
            lv_px = (job_lvl_px_i > job_level_i) ? job_lvl_px_i : job_level_i;
            lv_nz = (job_lvl_nz_i > job_level_i) ? job_lvl_nz_i : job_level_i;
            j_lvl_ord[0] <= lv_nx;
            j_lvl_ord[1] <= lv_pz;
            j_lvl_ord[2] <= lv_px;
            j_lvl_ord[3] <= lv_nz;
            j_m_ord[0] <= 4'(4'd8 >> lv_nx);
            j_m_ord[1] <= 4'(4'd8 >> lv_pz);
            j_m_ord[2] <= 4'(4'd8 >> lv_px);
            j_m_ord[3] <= 4'(4'd8 >> lv_nz);
            stitch_new = (lv_nx != job_level_i) || (lv_pz != job_level_i) ||
                (lv_px != job_level_i) || (lv_nz != job_level_i);
            j_stitch <= stitch_new;
            // The level encoding makes an illegal RESOLUTION unrepresentable;
            // the morph factor is the only lod_target lane that can be out of
            // range, and it is clamped and counted.
            if (job_morph_i > 17'd65536) begin
              j_morph <= 17'd65536;
              lod_clamped_o <= lod_clamped_o + 32'd1;
            end else begin
              j_morph <= job_morph_i;
            end
            j_surface <= job_surface_i;
            j_src <= job_src_id_i;

            // A legacy single-surface page has no underside at all.
            if (job_surface_i && !job_dual_i) begin
              done <= 1'b1;
              st   <= StIdle;
            end else if (job_dual_i) begin
              solid  <= '0;
              sc_idx <= '0;
              sc_pend <= 1'b0;
              st     <= StScan;
            end else begin
              // A legacy page has no cell-state plane, so every cell is SOLID
              // and the 8x8 pre-scan is skipped entirely. It can still be
              // STITCHED, though: LOD levels have nothing to do with whether a
              // page models an underside. Forcing the plain path here was a
              // real defect — the RTL emitted the full grid while the oracle
              // built the annulus — and it survived every directed case because
              // the directed lattices are all dual. The randomized lane B
              // found it, which is what a second lane is for.
              solid <= {64{1'b1}};
              emode <= !stitch_new ? EmPlain : ((n_new < 4'd3) ? EmFan : EmInner);
              ea <= (stitch_new && n_new >= 4'd3) ? 4'd1 : 4'd0;
              eb <= (stitch_new && n_new >= 4'd3) ? 4'd1 : 4'd0;
              etri <= 1'b0;
              eside <= '0;
              eg <= '0;
              efan <= '0;
              f_slot <= '0;
              f_kind <= '0;
              done   <= 1'b0;
              st     <= StTri;
            end
          end
        end

        // ---- the 8x8 pre-scan ------------------------------------------------
        StScan: begin
          sc_pend <= cs_req_o;
          sc_pend_idx <= sc_idx[5:0];
          if (cs_req_o) sc_idx <= sc_idx + 7'd1;
          if (sc_pend) solid[sc_pend_idx] <= (cs_substance_i == 2'd0);
          if (sc_idx == 7'd64 && !sc_pend) begin
            // A coarsened subpatch carrying a void cell is REJECTED, loudly.
            if (j_stitch && (solid != {64{1'b1}})) begin
              subpatch_rejected_o <= subpatch_rejected_o + 32'd1;
              job_reject_o <= 1'b1;
              st <= StIdle;
            end else begin
              emode <= j_stitch ? EmInner : EmPlain;
              ea <= j_stitch ? 4'd1 : 4'd0;
              eb <= j_stitch ? 4'd1 : 4'd0;
              etri <= 1'b0;
              eside <= '0;
              eg <= '0;
              efan <= '0;
              f_slot <= '0;
              f_kind <= '0;
              // n <= 3 leaves the annulus with no inner block at all
              if (j_stitch && j_n < 4'd3) emode <= EmFan;
              done <= 1'b0;
              st <= StTri;
            end
          end
        end

        // ---- fetch and emit ---------------------------------------------------
        // THE ENUMERATOR ADVANCES AT ISSUE, NOT AT CAPTURE. That is what keeps
        // the pipe at three cycles per triangle — one lattice read per clock,
        // which is the ledger's "1 emitted vertex per clock". Advancing at
        // capture would insert a bubble, because the next triangle's first
        // address would still be pointing at the finished triangle. The emit
        // path reads no enumerator state at all (only the captured vertex
        // registers and the read landing this cycle), so the two can be moved
        // apart safely.
        StTri: begin
          pend_v <= do_issue;
          if (do_issue) begin
            pend_slot <= f_slot;
            pend_kind <= f_kind;
            pend_last <= iss_last;
            if (f_kind == 2'd0 && mc[f_slot] != 2'd0) f_kind <= 2'd1;
            else if (f_kind == 2'd1) f_kind <= 2'd2;
            else begin
              f_kind <= 2'd0;
              f_slot <= f_slot + 2'd1;
            end
            if (iss_last) begin
              f_slot <= 2'd0;
              f_kind <= 2'd0;
            end
          end

          // capture the read issued last cycle
          if (pend_v) begin
            if (pend_kind == 2'd0) begin
              vx[pend_slot] <= lat_wx_i;
              vz[pend_slot] <= lat_wz_i;
              vh[pend_slot] <= lat_h_i;
              vy[pend_slot] <= lat_h_i;
            end else if (pend_kind == 2'd1) begin
              v_ha <= lat_h_i;
            end else begin
              vy[pend_slot] <= m_y;
            end

            if (pend_last) begin
              // The underside is the top's pair with b and c swapped — the ONE
              // place the inverted winding lives.
              o_valid <= 1'b1;
              o_src   <= j_src;
              o_surf  <= j_surface;
              o_ax    <= vx[0];
              o_ay    <= vy[0];
              o_az    <= vz[0];
              if (!j_surface) begin
                o_bx <= vx[1];
                o_by <= vy[1];
                o_bz <= vz[1];
                o_cx <= last_x;
                o_cy <= last_y;
                o_cz <= last_z;
              end else begin
                o_bx <= last_x;
                o_by <= last_y;
                o_bz <= last_z;
                o_cx <= vx[1];
                o_cy <= vy[1];
                o_cz <= vz[1];
              end
              terrain_triangles_emitted_o <= terrain_triangles_emitted_o + 32'd1;
            end
          end

          // ---- advance the enumerator ------------------------------------
          // A void run-cell is SKIPPED here, at one cycle per skipped cell and
          // no lattice read at all.
          if (cell_skip || (do_issue && iss_last)) begin
            if (!cell_skip && !etri && emode != EmFan) begin
              etri <= 1'b1;
            end else if (emode == EmFan) begin
              if (efan >= fan_steps) begin
                efan <= '0;
                if (seg_wrap) begin
                  eg <= '0;
                  if (eside == 2'd3) done <= 1'b1;
                  else eside <= eside + 2'd1;
                end else begin
                  eg <= eg + 4'd1;
                end
              end else begin
                efan <= efan + 6'd1;
              end
            end else begin
              // the next run-cell in z-then-x scan order
              etri <= 1'b0;
              if (ea >= cell_hi) begin
                ea <= cell_lo;
                if (eb >= cell_hi) begin
                  if (emode == EmInner) begin
                    emode <= EmFan;
                    eside <= '0;
                    eg <= '0;
                    efan <= '0;
                  end else begin
                    done <= 1'b1;
                  end
                end else begin
                  eb <= eb + 4'd1;
                end
              end else begin
                ea <= ea + 4'd1;
              end
            end
          end

          if (done && !pend_v && !o_valid) st <= StIdle;
        end

        default: st <= StIdle;
      endcase
    end
  end

  assign tri_valid_o = o_valid;
  assign ax_o = o_ax;
  assign ay_o = o_ay;
  assign az_o = o_az;
  assign bx_o = o_bx;
  assign by_o = o_by;
  assign bz_o = o_bz;
  assign cx_o = o_cx;
  assign cy_o = o_cy;
  assign cz_o = o_cz;
  assign surface_o = o_surf;
  assign src_id_o = o_src;
  assign idle_o = (st == StIdle) && !o_valid;

endmodule : zhao_terrain_tess
