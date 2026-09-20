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
// THE THREE MODES (2026-09-10, reports/PROJECTION-ADOPTION-20260910.md §7)
// ---------------------------------------------------------------------------
// `job_mode_i` selects, PER JOB, what the enumerator's walk is expanded into:
//
//   ModeTri (0)  today's block, bit- and cycle-identical: world-coordinate
//                triangles on `tri_*`.
//   ModeVtx (1)  the 81 lattice vertices of the 9x9 window in index order
//                k = 0..80, vi = ox + k % 9, vj = oz + k / 9, each with the
//                geomorph applied, on `vtx_*` — `zref::terrain::detail::
//                vertex_at` in hardware, using THIS block's lattice read, its
//                parent reads and its blend (`m_y`). This is the dense fill the
//                projected-vertex arena (`zhao_terrain_wcache`, DENSE_SEAL at
//                DEPTH = 81) needs per job. No lattice read is skipped for a
//                void cell: a vertex exists whether or not the cells around it
//                are solid, and the arena wants all 81 in order.
//   ModeRef (2)  the SAME triangle walk as ModeTri — run-cells, inner block,
//                annulus fans, void skips, the underside's b/c swap — but each
//                triangle leaves as three window INDICES (ia, ib, ic) on
//                `ref_*`, one triangle per clock, with NO lattice read at all.
//                This is what `zhao_terrain_topo` did for level 0 unstitched
//                and could not do for anything else; the topology is
//                job-dependent and lives here, so the references come from
//                here. The b/c swap stays the ONE place the winding lives.
//
// A separate fill block reimplementing `mcase_f` + the parent reads + the
// blend would have been the second-copy pattern this repository forbids
// (CLAUDE.md, "read the SIBLING contract"); it was named before it was written
// and is not written. Mode 3 is not a mode: it is counted in `mode_invalid_o`
// and the job runs as ModeTri, never silently.
//
// HOW MODE 0 IS KEPT IDENTICAL. Every place the new modes touch the existing
// machine is an AND/mux term on a REGISTERED job bit (`j_vtx`, `j_ref`) in a
// CONTROL path — the enumerator advance, the issue gate, the output-register
// enable — or a job register whose mode-0 value equals what the wire held
// before (`j_vshift` = level, `j_plain_hi` = n - 1). The geomorph blend
// `m_dab -> m_half -> m_hc -> m_d -> m_prod -> m_step -> m_y` is UNTOUCHED:
// ModeVtx lands `m_y` into the vertex skid register through the very same
// `last_y` wire ModeTri lands it into `o_cy`, so the cone is neither
// lengthened nor shortened (reports/TERRAIN_31MHZ_REARCHITECTURE.txt §6.2 —
// it is still the live arithmetic cone, and registering it is that
// document's Step 3, not this change).
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
// 6. AN OFF-GRID VERTEX IN ModeVtx IS THE PLAIN LATTICE VERTEX. At level L
//    only the (8 >> L + 1)^2 vertices on the job's own stride grid can be a
//    corner of any triangle of the job (the identity probe: a stitched ring's
//    outer vertices are coarser, never finer). The other window vertices are
//    emitted only because DENSE_SEAL wants 81 in order, and they are emitted
//    with morph case 0 — one lattice read, height verbatim — and flagged
//    `vtx_stride_o = 0`. REJECTED ALTERNATIVE: `vertex_at` semantics for them
//    too (morph toward parents at vi ± s). That is well defined for every
//    ON-grid interior vertex, whose parents stay inside the window, and NOT
//    for an off-grid one at the patch edge: ox = 0, vi = 1, s = 2 asks for
//    parent -1, outside the lattice. A law that is undefined on legal input
//    is not a law. The flag lets a bitmap-mode consumer (VALID_MODE = 0) drop
//    the fillers and fill only the stride set — the §3 remedy — without a
//    primitive change.
// 7. A JOB REJECTED IN ONE MODE IS REJECTED IN EVERY MODE. The stitched+void
//    reject (law 3) runs its cell-state scan in ModeVtx too, so the sequencer
//    that presents a job twice (fill, then references) learns of the reject
//    at the FIRST presentation and skips the second; `subpatch_rejected_o`
//    counts presentations. An UNSTITCHED ModeVtx job skips the 65-cycle scan
//    entirely — nothing in that mode consumes solidity — which is the one
//    place a mode differs in timing from ModeTri on purpose.
//
// NOT IN THIS BLOCK, deliberately: rim walls (FORGE.CLIFF), normals
// (TERRAIN.NORMALS), LOD decisions (TERRAIN.LOD decides, this block obeys), and
// per-vertex UV. The reference computes terrain UV in its DRAW loop from a
// patch-level `top_shift`, and the mirrored-repeat fold (terrain_rules §6.2) is
// the TMU sampler's, not the tessellator's; emitting UV here would fix an
// underside/wall UV law that FORGE.CLIFF owns.
//
// Conservative SystemVerilog subset only (charter §2).

module zhao_terrain_tess #(
    // Width of a window index on `vtx_index_o` / `ref_i*_o`: 81 vertices need
    // 7 bits. A consumer that carries a refusal bit beside the index
    // (zhao_terrain_wcache, INDEX_W = 8) zero-extends; the guard below refuses
    // a width that cannot hold index 80.
    parameter int unsigned IDX_W = 7
) (
    input logic clk,
    input logic rst_n,

    // -----------------------------------------------------------------------
    // patch_state + lod_target: one subpatch of one surface
    // -----------------------------------------------------------------------
    input  logic        job_valid_i,
    output logic        job_ready_o,
    input  logic [ 1:0] job_mode_i,      // 0 = triangles, 1 = vertices, 2 = references
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

    // -----------------------------------------------------------------------
    // ModeVtx out — the 81 window vertices, index order, geomorph applied.
    // Placed x/z verbatim from the lattice, y = `vertex_at`'s height.
    // -----------------------------------------------------------------------
    output logic               vtx_valid_o,
    input  logic               vtx_ready_i,
    output logic signed [31:0] vtx_x_o,
    output logic signed [31:0] vtx_y_o,
    output logic signed [31:0] vtx_z_o,
    output logic [IDX_W-1:0]   vtx_index_o,    // (vj - oz) * 9 + (vi - ox)
    output logic               vtx_stride_o,   // 1 = on the job's own stride grid (law 6)
    output logic               vtx_surface_o,
    output logic        [15:0] vtx_src_id_o,

    // -----------------------------------------------------------------------
    // ModeRef out — one triangle per clock as three window indices, in the
    // emitted order and winding of ModeTri (b/c swapped on the underside).
    // -----------------------------------------------------------------------
    output logic               ref_valid_o,
    input  logic               ref_ready_i,
    output logic [IDX_W-1:0]   ref_ia_o,
    output logic [IDX_W-1:0]   ref_ib_o,
    output logic [IDX_W-1:0]   ref_ic_o,
    output logic               ref_surface_o,
    output logic        [15:0] ref_src_id_o,

    output logic [31:0] terrain_triangles_emitted_o,
    output logic [31:0] terrain_vertices_emitted_o,  // ModeVtx vertices landed (saturating)
    output logic [31:0] terrain_refs_emitted_o,      // ModeRef triples loaded (saturating)
    output logic [31:0] mode_invalid_o,              // job_mode_i == 3 presented (saturating)
    output logic [31:0] subpatch_rejected_o,
    output logic [31:0] lod_clamped_o,
    output logic        job_reject_o,  // 1-cycle pulse with subpatch_rejected_o
    output logic        idle_o
);

  // The ratified TERRAIN.PATCH arithmetic, imported in MODULE scope rather
  // than $unit scope -- an import::* outside a module raises IMPORTSTAR under
  // -Wall and would put these names in every file compiled beside this one.
  // Same placement, and for the same reason, as zhao_terrain_patch_acc.sv.
  import zhao_terrain_patch_law_pkg::*;

  localparam int unsigned SubCells = 8;  // charter §11.1
  localparam int unsigned Side = SubCells + 1;  // 9 lattice vertices per window side
  localparam int unsigned Depth = Side * Side;  // 81, the arena's identity space

  // Quartus 17 wants an elaboration check inside `initial begin ... end`
  // (CLAUDE.md build note). `--lint-only` does not run it; the modes test
  // fires it with -GIDX_W=6.
  initial begin
    if (IDX_W < $clog2(Depth))
      $fatal(1, "zhao_terrain_tess: IDX_W (%0d) cannot carry window index %0d", IDX_W, Depth - 1);
  end

  localparam logic [1:0] StIdle = 2'd0;
  localparam logic [1:0] StScan = 2'd1;
  localparam logic [1:0] StTri = 2'd2;

  localparam logic [1:0] ModeTri = 2'd0;
  localparam logic [1:0] ModeVtx = 2'd1;
  localparam logic [1:0] ModeRef = 2'd2;

  // ---- §3/§4 arithmetic ----------------------------------------------------
  //
  // `fx_add_sat` LIVED HERE and is now zhao_tp_fx_add_sat in
  // zhao_terrain_patch_law_pkg (packet TERRLAW, owner ruling R176). It is worth
  // recording WHY it was not obviously the same function, because the shape is
  // the one that hides a duplicate from a reader AND from
  // `tools/budget/duplicate_functions.py`'s eye even while the tool names it:
  //
  //   this copy took **33-bit** inputs and summed at 34, where the package
  //   takes 32 and sums at 33 -- so it LOOKED like a deliberately wider
  //   variant, the kind of thing R176 says must make the package GROW rather
  //   than be narrowed away.
  //
  // IT IS NOT WIDER. Both call sites passed `{x[31], x}` -- a 32-bit signed
  // value hand-extended to 33 -- and all four operands (`v_ha`, `m_half`,
  // `ln3_vh_q`, `ln3_step_q`) are declared `logic signed [31:0]`. The extra bit
  // was always a duplicated sign bit, so the 34-bit add and the package's
  // 33-bit add compare identically against the same two bounds. The calls below
  // therefore drop the manual extension and pass the 32-bit values straight in:
  // nothing is narrowed, because nothing was ever wider than 32 bits.
  //
  // EVIDENCE, not argument. The pre-factoring module was instantiated beside
  // this one on identical stimulus and every output compared on every clock,
  // for 500,000 random vectors -- 490,958 of them on a clock where one of the
  // 42 outputs MOVED -- with no disagreement anywhere. The same bench with
  // zhao_tp_fx_add_sat's non-saturating result flipped in bit 0 disagreed at
  // vector 22,546, so the null above is a null this instrument could have
  // broken. `terrain_tess_directed` (6,751 checks), `terrain_tess_modes_directed`
  // (33) and `terrain_pipe_rpp3_matw18_fit_top_directed` (15) are the durable
  // regression evidence and are run at this commit.

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
  // the mode, decoded once at accept
  logic        j_vtx;  // ModeVtx
  logic        j_ref;  // ModeRef
  // ModeVtx walks the window at stride 1 whatever the level; ModeTri/ModeRef
  // walk run-cells at the level's stride. In mode 0 these two registers hold
  // exactly the values the wires they replace used to compute (level, n - 1).
  logic [ 1:0] j_vshift;    // shift applied to (ea, eb): level, or 0 in ModeVtx
  logic [ 3:0] j_plain_hi;  // last (ea, eb) of the plain walk: n - 1, or 8 in ModeVtx

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
  // ModeVtx: the enumerator has ADVANCED by the time the vertex's last read
  // lands, so its identity rides the pend registers.
  logic [IDX_W-1:0] pend_idx;
  logic             pend_stride;

  logic signed [31:0] vx[3], vz[3], vh[3], vy[3];

  // ---- T10c: BANK THE BLENDED HEIGHT BY TRIANGLE PARITY ------------------
  //
  // `vy[]` had to be snapshotted early ONLY because the next triangle
  // overwrites it before this one emits. Two banks and a parity bit that
  // flips when a triangle's last read issues remove that reason: the next
  // triangle writes the other bank and this one's values survive to the emit.
  //
  // So the early capture goes away, and with it everything that can be stale
  // -- which is what defeated the stage-D forward, the two-level forward and
  // the holding-copy attempt in turn. Each tried to repair a value captured
  // before it existed. This does not capture it.
  //
  // Bank N and bank N+2 share storage. A triangle emits four cycles after its
  // last read issues and triangle N+2 cannot start writing until N+1's reads
  // have all issued, so the margin is wide -- and terrain_tess_directed is
  // what decides that, not this paragraph.
  logic signed [31:0] vy1[3];
  logic               tri_par;
  logic               pend_par;
  logic               lnd_par_q, ln2_par_q, ln3_par_q;

  function automatic logic signed [31:0] vy_bank(input logic b,
                                                 input logic [1:0] s);
    vy_bank = b ? vy1[s] : vy[s];
  endfunction
  logic signed [31:0] v_ha;  // parent A of the slot being fetched

  // ---- output register -----------------------------------------------------
  // ---- ModeTri output: a credit-gated TWO-deep queue ----------------------
  //
  // It was ONE register with an `out_busy` gate on the last read, and that was
  // correct while the emit happened on the response edge: the gate looked at
  // `o_valid` one cycle before the emit, and the previous triangle's emit was
  // at least three cycles earlier, so nothing could occupy the slot in between.
  //
  // T1b BROKE THAT, and the arithmetic is worth writing down because it is the
  // same law as the vertex credit. A triangle is at least three reads, so its
  // last read is issued three cycles after the previous one's; the emit is now
  // three cycles after the last read. Those two threes cancel: triangle k's
  // emit lands on exactly the cycle triangle k+1's last read is being issued.
  // `out_busy` at that moment still sees the PRE-emit `o_valid`, so it reads
  // free, the read is issued, and three cycles later k+1 overwrites a k that
  // the consumer never took. terrain_tess_directed measured it exactly:
  // "a stalled consumer loses no triangle and corrupts none: expected 0x80, got
  // 0x40" -- half the triangles, silently.
  //
  // The fix is the rule T1 established, applied to the other output: THE BUFFER
  // MUST BE DEEPER THAN WHAT IS IN FLIGHT. Last reads are >= 3 apart and the
  // pipe is 3 deep, so at most ONE triangle is ever in flight, and two slots
  // are enough. The credit is the same shape as the vertex one, and with the
  // consumer always ready the steady state is 1 + 1 - 1 + 1 = 2 <= 2, so the
  // rate is unchanged at one triangle per three clocks.
  // G8B T10: 3, not 2. The blend gained a fourth in-flight stage, so the
  // credit below carries a fourth term and would block an issue one cycle
  // sooner at the same depth -- which is exactly how T1 met a timing target
  // and lost the vertex RATE, caught only by terrain_tess_modes_directed.
  // The law: the buffer must be DEEPER than the number of items that can be
  // in flight, because a read already issued cannot be told to wait.
  localparam int TQ_DEPTH = 3;
  logic               tq_valid [TQ_DEPTH];
  logic signed [31:0] tq_ax [TQ_DEPTH], tq_ay [TQ_DEPTH], tq_az [TQ_DEPTH];
  logic signed [31:0] tq_bx [TQ_DEPTH], tq_by [TQ_DEPTH], tq_bz [TQ_DEPTH];
  logic signed [31:0] tq_cx [TQ_DEPTH], tq_cy [TQ_DEPTH], tq_cz [TQ_DEPTH];
  logic [15:0]        tq_src [TQ_DEPTH];
  logic               tq_surf [TQ_DEPTH];

  logic [1:0] tocc;
  always_comb begin
    tocc = 2'd0;
    for (int k = 0; k < TQ_DEPTH; k++) tocc = tocc + {1'b0, tq_valid[k]};
  end

  // ---- ModeVtx output: a credit-gated THREE-deep skid ---------------------
  // A vertex needs as few as ONE lattice read, so unlike a triangle (>= 3
  // reads) the next vertex's last read can be issued the very cycle the
  // previous one lands. With a single output register that is a lost vertex
  // under a stall, and gating on "the register is free NOW" halves the rate to
  // one vertex per two clocks. The credit rule is the arena shell's
  // (`zhao_terrain_wcache`): issue only when occupancy + landings - pop leaves
  // a slot for every vertex already committed to land.
  //
  // G8B T1 MADE THIS THREE DEEP, and the depth is not a comfort margin -- it is
  // the exact price of the extra pipeline stage. THE BUFFER MUST BE DEEPER THAN
  // THE NUMBER OF VERTICES IN FLIGHT, or the credit can never be granted in
  // steady state. Two slots were right while one vertex was in flight (stage A
  // alone); splitting the blend put TWO in flight (stages A and B), and a read
  // already issued CANNOT be told to wait -- its lattice response arrives on the
  // next edge whatever the consumer is doing. So the credit has to reserve a
  // slot for both of them, and with only two slots
  //
  //     cnt=1, land=1, land_a=1, pop=1  ->  1+1+1-1 = 2 > 1
  //
  // is the steady state with the consumer ALWAYS READY: never grantable, so a
  // bubble every other vertex. terrain_tess_modes_directed measured exactly
  // that -- 128 cycles for 81 unstitched vertices against a budget of 93.
  // The third slot makes the same steady state 1+1+1-1 = 2 <= 2, and the rate
  // is one vertex per clock again. Latency may grow; the initiation rate may
  // not, and this is the register cost of keeping that true.
  //
  // The queue's entries fill in order from index 0, so a valid entry at k
  // implies a valid entry at every index below it.
  // T1b makes it FOUR, and at four the hand-written land/pop/land-and-pop
  // branches stop being readable: three named slots already needed a nested
  // three-way shift in each of three arms. It is an ordered queue, so it is
  // written as one -- `vq_*[0]` is the head the consumer sees, entries fill
  // upwards, and a pop shifts every entry down by one. The ORDERING SEMANTICS
  // ARE UNCHANGED from the named version; only the spelling is.
  // G8B T10: 5, not 4, for the reason TQ_DEPTH gives above.
  localparam int VQ_DEPTH = 5;
  logic               vq_valid  [VQ_DEPTH];
  logic signed [31:0] vq_x      [VQ_DEPTH];
  logic signed [31:0] vq_y      [VQ_DEPTH];
  logic signed [31:0] vq_z      [VQ_DEPTH];
  logic [IDX_W-1:0]   vq_idx    [VQ_DEPTH];
  logic               vq_stride [VQ_DEPTH];

  // occupancy, 0..VQ_DEPTH. Entries are contiguous from 0, so this is also the
  // index the next landing takes (before any pop shift).
  logic [2:0] vocc;
  always_comb begin
    vocc = 3'd0;
    for (int k = 0; k < VQ_DEPTH; k++) vocc = vocc + {2'b0, vq_valid[k]};
  end

  // ---- ModeRef output: one registered triple ------------------------------
  logic r_valid;
  logic [IDX_W-1:0] r_ia, r_ib, r_ic;

  // =========================================================================
  // combinational geometry
  // =========================================================================

  // is every patch cell under run-cell (ea, eb) SOLID?
  //
  // ---------------------------------------------------------------------
  // WRITTEN AS A MASK BECAUSE THIS EXPRESSION IS THE TERRAIN LANE'S CLOCK
  // ---------------------------------------------------------------------
  // `zhao_pair_tess_normals` -- the registered characterisation wrapper --
  // measures 32.42 MHz against a 100 MHz product clock, and splitting its
  // 1,803 paths by endpoint puts the TESS -> TESS family at 40.11 MHz. The
  // census of the worst 200 of those paths names its sources and destinations:
  //
  //     sources   solid 49, eg 43, ea, pend_last 85
  //     dests     vh 69, subpatch_rejected_o 42, f_kind, pend_slot
  //
  // Those signals meet in exactly one place -- here -- and the destinations are
  // the issue path and its reject counter, because this one bit gates
  // `want_issue` through `cell_skip`.
  //
  // WHAT THE OLD FORM COST. Eight-by-eight iterations, each with FOUR
  // comparisons against `ea * j_s` and `eb * j_s`, reduced to a single bit:
  // 256 comparisons and 128 multiply sites. `j_s` is a 4-bit REGISTER holding
  // 1/2/4/8 (declared at the top of this file), NOT a constant, so those are
  // genuine runtime multiplies and not shifts by a literal. It is the shape
  // S15.5 warns about in another block -- "do not write six independent `*`
  // operators and assume they pack" -- at 128.
  //
  // THE WINDOW IS A RECTANGLE, so it factors. A cell is inside it exactly when
  // its column is in the column span AND its row is in the row span, so two
  // 8-bit span masks and their outer product replace the whole double loop:
  // TWO multiply sites and SIXTEEN comparisons.
  //
  // BIT-IDENTICAL, DELIBERATELY. `(solid & win) == win` is true exactly when
  // every masked bit of `solid` is set, which is what the old loop computed;
  // and an EMPTY window (`lo` past the edge, mask all zero) yields true in
  // both forms, which is the old loop's `cell_solid` never being cleared. No
  // latency changes, no state is added, and the run-cell walk is untouched --
  // so the existing suites must pass UNCHANGED, and that is the check.
  //
  // THE REGISTERED FORM IS NOW BELOW, and this paragraph used to defer it.
  // reports/TERRAIN-TESS-CLOCK-20260907.md established that `win_mask` can be
  // registered at no latency cost, because at cycle N-1 the advance decision
  // is already made and so `ea(N)`/`eb(N)` are known -- a second step with
  // five paired assignment sites and a real chance of a stale mask, which is
  // a WRONG SOLIDITY ANSWER and not a timing bug, so it was taken separately
  // after this one was measured. It was measured four times (@g8b, @g8b-t2,
  // @g8b-t12, @g8b-t1b) and @g8b-t3bc named this block the sole cap, so G8B
  // T3a cashed it: see `win_mask_q` and `a_win_mask_fresh` below. What stays
  // true here is the FACTORISATION -- the outer product is what made the
  // registered form one 64-bit vector instead of a 128-multiply loop.

  // The 8-bit span [idx*s, idx*s + s) within a row or a column.
  //
  // G8B T4: THE MULTIPLY IS GONE, AND WITH IT THE ADD AND HALF THE COMPARES.
  // @g8b-t3 measured this function as the whole of the block's remaining cap:
  // every one of the worst forty endpoints was j_plain_hi -> win_mask_q at
  // 11.524 ns, through cell_hi, the advance compare, and then THIS.
  //
  // The stride is 1/2/4/8 -- always 2**level -- so the span of index `idx` is
  // exactly the set of k whose high bits are idx:
  //
  //     k >= idx*2**L  &&  k < (idx+1)*2**L   <=>   (k >> L) == idx
  //
  // and because k is the LOOP CONSTANT, `k >> L` is a four-way select among
  // four constants rather than a shifter. What was a 4x4 runtime multiply, an
  // add and sixteen magnitude compares is eight equality compares against a
  // 4-bit register.
  //
  // BIT-IDENTICAL, INCLUDING THE EDGE. `idx` is four bits and `k >> L` is at
  // most 3 bits, so any idx >= 8 matches nothing -- which is the old form's
  // `lo = idx*sw >= 8`, the span falling off the edge, and ModeVtx's
  // j_plain_hi = 8 depends on it. The equality is over the full four bits for
  // exactly that reason; comparing three would make idx = 8 alias idx = 0.
  //
  // It takes the LEVEL, not the stride. `j_s` is the decoded 1/2/4/8 and the
  // level is what the shift wants; `j_level` already exists and already
  // carries it, so nothing new is stored.
  function automatic logic [7:0] span_mask(input logic [3:0] idx,
                                           input logic [1:0] lvl);
    logic [7:0] m;
    begin
      m = 8'd0;
      for (int k = 0; k < int'(SubCells); k++)
        m[k] = (4'(k >> lvl) == idx);
      span_mask = m;
    end
  endfunction

  // The whole window mask for a run-cell, as one function, so the registered
  // form below and the assertion that checks it cannot drift apart.
  function automatic logic [63:0] window_mask(input logic [3:0] a,
                                              input logic [3:0] b,
                                              input logic [1:0] lvl);
    logic [7:0] col, row;
    logic [63:0] m;
    begin
      col = span_mask(a, lvl);
      row = span_mask(b, lvl);
      for (int cj = 0; cj < int'(SubCells); cj++)
        for (int ci = 0; ci < int'(SubCells); ci++)
          m[cj*8+ci] = row[cj] & col[ci];
      window_mask = m;
    end
  endfunction

  // ---- G8B T3a: THE WINDOW MASK IS REGISTERED ------------------------------
  //
  // This is the step line 553 deferred and reports/TERRAIN-TESS-CLOCK-20260907
  // designed: *"the registered form ... can be registered at no latency cost,
  // because at cycle N-1 the advance decision is already made and so ea(N)/eb(N)
  // are known."* Its stated precondition was a before-measurement, and there are
  // now four: @g8b, @g8b-t2, @g8b-t12, @g8b-t1b.
  //
  // MEASURED at @g8b-t3bc: with both projector cones cut, the tessellator is the
  // SOLE constraint at -3.482 ns and `zhao_project_core` behind it would allow
  // 88 MHz. The path is the enumerator closing a loop through the geomorph DSP:
  //
  //   j_s -> span_mask (a multiply) -> win_mask (64-bit outer product)
  //       -> cell_solid (64-bit masked compare) -> cell_skip
  //       -> the enumerator advance -> that same DSP's clock enable
  //
  // Registering the mask moves the multiply, the outer product and the 64
  // comparator groups off the consumed path into a next-state cone that has a
  // whole cycle. The one-cycle void skip is preserved, which the block's own
  // header states as a goal, so the RATE does not move.
  //
  // THE HAZARD IS NOT TIMING, AND THE REPORT SAYS SO: `ea`/`eb` are assigned in
  // five places and every one needs the paired mask, or the mask goes stale --
  // "a WRONG SOLIDITY ANSWER, not a timing bug", which emits or drops triangles
  // silently. Two things guard it:
  //
  //   * each site computes its next `ea`/`eb` into locals ONCE and uses them for
  //     both the state and the mask, so a site cannot pair itself incorrectly;
  //   * `a_win_mask_fresh` below asserts every cycle that the registered mask
  //     equals the mask of the state actually in `ea`/`eb`/`j_s`. A missed site
  //     fires it immediately instead of quietly changing the geometry.
  logic [63:0] win_mask_q;

  wire cell_solid = ((solid & win_mask_q) == win_mask_q);

  // ---- G8B T5: THE RUN-CELL'S LATTICE BASE IS REGISTERED TOO -------------
  //
  // @g8b-t4 left the block capping the machine at -0.818 ns with every worst
  // endpoint leaving `j_vshift[0]`, and the cone behind it is
  //
  //   j_vshift -> (ea << j_vshift) + j_ox -> tv_i/tv_j -> mcase_f x3
  //            -> mc[f_slot] -> iss_last -> the enumerator advance
  //
  // which is T3a's shape exactly one level up: a lattice coordinate
  // recomputed combinationally from the enumerator state on every cycle, on
  // the path that decides whether the enumerator may advance. `mcase_f` is
  // the expensive middle -- four 6-bit magnitude compares and two masked-zero
  // tests, three instances -- and it cannot start until the shift and the add
  // in front of it finish.
  //
  // So the shift and the add move into the next-state cone, which T3a already
  // built the mechanism for: `ea_n_c`/`eb_n_c` are decided one cycle early, at
  // the same five paired sites, and these registers pair at every one of them.
  // Nothing new is decided early -- the same decision now feeds two registers
  // instead of one.
  //
  // SAME HAZARD, SAME TWO GUARDS. A missed pairing is a WRONG LATTICE
  // COORDINATE, which silently emits a triangle from the wrong place rather
  // than failing; `a_cell_base_fresh` below differences the registers against
  // the value the live state would give, every StTri cycle.
  //
  // UNUSED IN EmFan, exactly like win_mask_q: the fan branch overwrites
  // tv_i/tv_j from the ring walk and never reads these.
  function automatic logic [5:0] cell_base(input logic [5:0] origin,
                                           input logic [3:0] e,
                                           input logic [1:0] vsh);
    cell_base = origin + 6'(({2'b0, e}) << vsh);
  endfunction

  logic [5:0] i0_q, j0_q;

`ifndef SYNTHESIS
  // THE STALE-MASK DETECTOR. Without this the failure mode of a missed paired
  // assignment is a triangle that should not exist, or one that should and does
  // not -- found, if at all, by a golden capture a long way downstream.
  always_ff @(posedge clk) begin
    // No rst_n term, for the reason the vertex-queue assertion below gives:
    // reading it synchronously while the design takes it asynchronously is a
    // SYNCASYNCNET warning. `st` is cleared to StIdle by that same reset, so
    // the StTri term already covers the reset window.
    if ((st == StTri) && (win_mask_q !== window_mask(ea, eb, j_level)))
      $fatal(1, "zhao_terrain_tess: win_mask_q is stale -- ea=%0d eb=%0d lvl=%0d",
             ea, eb, j_level);
  end

  // T5's half. Separate from the mask's so a failure names WHICH register
  // drifted; they pair at the same five sites but they are not the same bug.
  always_ff @(posedge clk) begin
    if ((st == StTri) && (emode != EmFan) &&
        ((i0_q !== cell_base(j_ox, ea, j_vshift)) ||
         (j0_q !== cell_base(j_oz, eb, j_vshift))))
      $fatal(1, "zhao_terrain_tess: cell base is stale -- i0_q=%0d want %0d, j0_q=%0d want %0d",
             i0_q, cell_base(j_ox, ea, j_vshift),
             j0_q, cell_base(j_oz, eb, j_vshift));
  end
`endif

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
    // `j_vshift` IS `j_level` in ModeTri/ModeRef; ModeVtx walks at stride 1.
    // REGISTERED since T5 -- the shift and the add are in the next-state cone
    // now, and `a_cell_base_fresh` is what holds these equal to the live state.
    i0 = i0_q;
    j0 = j0_q;
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

  // ModeVtx, law 6: is the vertex at (ea, eb) on the job's own stride grid?
  // (ea & (s - 1)) == 0 — ox is a multiple of 8 and s divides 8, so the
  // relative test equals the absolute one. Only slot 0 is ever fetched in
  // ModeVtx, so only mc[0] carries the override; it is an AND on a registered
  // bit at the output of `mcase_f`, transparent in mode 0.
  wire [3:0] on_grid_mask = j_s - 4'd1;
  wire       on_grid_c = ((ea & on_grid_mask) == 4'd0) && ((eb & on_grid_mask) == 4'd0);

  logic [1:0] mc[3];
  always_comb begin
    for (int p = 0; p < 3; p++) mc[p] = mcase_f(tv_i[p], tv_j[p]);
    if (j_vtx && !on_grid_c) mc[0] = 2'd0;
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

  // is the read being issued the LAST of this triangle (ModeTri: slot 2) or of
  // this vertex (ModeVtx: slot 0)? `mc[f_slot]` is the same mux `rd_vi` already
  // selects through; at f_slot == 2 it is mc[2], so mode 0 is unchanged.
  wire [1:0] last_slot = j_vtx ? 2'd0 : 2'd2;
  wire iss_last = (f_slot == last_slot) &&
      ((f_kind == 2'd0 && mc[f_slot] == 2'd0) || (f_kind == 2'd2));
  // Skipping a void run-cell costs one cycle and issues nothing. ModeVtx never
  // skips: a vertex exists whether or not the cells around it are solid.
  wire cell_skip = (emode != EmFan) && !cell_solid && !j_vtx;
  // the run-cell index bounds of the current mode: the whole subpatch on the
  // unstitched path (`j_plain_hi` = n - 1, or 8 in ModeVtx), the annulus's
  // inner block on the stitched one
  wire [3:0] cell_lo = (emode == EmInner) ? 4'd1 : 4'd0;
  wire [3:0] cell_hi = (emode == EmInner) ? (j_n - 4'd2) : j_plain_hi;

  // ---- ModeVtx credit: may a vertex's LAST read be issued now? ------------
  // It lands next cycle; the skid must then have a slot for certain.
  //     room = (occupancy + landing_now - popping_now) <= 1
  // G8B T1: a vertex now lands from the BLEND stage, one cycle after its
  // response, so the credit has to count BOTH the landing happening now and the
  // one already committed to land next cycle. Counting only the first would
  // issue a last read whose slot is promised to a vertex still in stage A --
  // the queue-occupancy defect this credit exists to prevent, one stage deeper.
  // FOUR TERMS SINCE T10, one per stage that can still produce a landing.
  // Missing one is the T1 defect exactly: the credit under-counts, the queue
  // overruns, and the assertion below is what says so.
  wire vland = ln3_v_q && ln3_last_q && j_vtx;   // lands this cycle, from stage D
  wire vland_b = ln2_v_q && ln2_last_q && j_vtx; // committed to land next cycle
  wire vland_c = lnd_v_q && lnd_last_q && j_vtx; // committed to land in two
  wire vland_a = pend_v && pend_last && j_vtx;   // committed to land in three
  wire vpop = vq_valid[0] && vtx_ready_i;  // the consumer drains one this cycle
  wire [3:0] vnxt = {1'b0, vocc} + {3'b0, vland} + {3'b0, vland_b}
                  + {3'b0, vland_c} + {3'b0, vland_a} - {3'b0, vpop};
  // FOUR slots, THREE vertices in flight. The invariant is unchanged --
  //     occupancy_next + in_flight_next <= DEPTH
  // -- and so is its consequence: the buffer must be deeper than the number in
  // flight or the credit is never grantable with the consumer always ready.
  // T1b adds the third stage, so DEPTH goes to 4 and the bound to 3; the
  // steady state is 1 + 1 + 1 + 1 - 1 = 3 <= 3.
  wire vtx_room = (vnxt <= 4'(VQ_DEPTH - 1));
  // Where a landing goes: the occupancy after this cycle's pop shift. Computed
  // three bits wide because `vocc` counts to VQ_DEPTH, then narrowed -- the
  // credit is what makes the narrowing safe, so it is asserted rather than
  // assumed. A landing at index VQ_DEPTH would silently wrap to 0 and overwrite
  // the head, which is the queue-occupancy defect this whole credit exists to
  // prevent, and it would look like a corrupted vertex rather than an overflow.
  wire [2:0] vland_slot = vpop ? (vocc - 3'd1) : vocc;
  wire [2:0] vland_idx  = vland_slot[2:0];
`ifndef SYNTHESIS
  // No `rst_n` term: reading it here synchronously while the design's own
  // always_ff takes it asynchronously is a SYNCASYNCNET warning, and it is not
  // needed -- `vland` depends on `ln2_v_q`, which reset clears.
  always_ff @(posedge clk) begin
    if (vland && vland_slot > 3'(VQ_DEPTH - 1))
      $fatal(1, "zhao_terrain_tess: landing at slot %0d with depth %0d -- the vtx_room credit is wrong",
             vland_slot, VQ_DEPTH);
  end
`endif
  // ---- ModeTri credit: may a triangle's LAST read be issued now? ----------
  // Same shape as the vertex credit above, and for the same reason: the emit
  // is three cycles after the last read, so "is the output register free NOW"
  // is a statement about the wrong cycle.
  wire tland   = ln3_v_q && ln3_last_q && !j_vtx;   // emits this cycle, stage D
  wire tland_b = ln2_v_q && ln2_last_q && !j_vtx;   // committed to emit next
  wire tland_c = lnd_v_q && lnd_last_q && !j_vtx;   // committed to emit in two
  wire tland_a = pend_v && pend_last && !j_vtx;     // committed to emit in three
  wire tpop    = tq_valid[0] && tri_ready_i;
  wire [3:0] tnxt = {2'b0, tocc} + {3'b0, tland} + {3'b0, tland_b}
                  + {3'b0, tland_c} + {3'b0, tland_a} - {3'b0, tpop};
  wire tri_room = (tnxt <= 4'(TQ_DEPTH - 1));
  wire [1:0] tland_slot = tpop ? (tocc - 2'd1) : tocc;
  wire [1:0] tland_idx  = tland_slot[1:0];
`ifndef SYNTHESIS
  always_ff @(posedge clk) begin
    if (tland && tland_slot > 2'(TQ_DEPTH - 1))
      $fatal(1, "zhao_terrain_tess: triangle emit at slot %0d with depth %0d -- the tri_room credit is wrong",
             tland_slot, TQ_DEPTH);
  end
`endif
  // the gate on the last read: the triangle credit in ModeTri, the vertex
  // credit in ModeVtx (j_vtx is a register)
  wire last_blocked = j_vtx ? !vtx_room : !tri_room;

  wire want_issue = (st == StTri) && !done && !cell_skip && !j_ref;
  wire do_issue = want_issue && !(iss_last && last_blocked);

  // ---- ModeRef: load the triple register when it is free or draining ------
  wire ref_can_load = !r_valid || ref_ready_i;
  wire ref_emit = (st == StTri) && !done && !cell_skip && j_ref && ref_can_load;

  // ---- window indices: (vj - oz) * 9 + (vi - ox), a constant multiply -----
  function automatic logic [IDX_W-1:0] win_idx(input logic [5:0] vi, input logic [5:0] vj);
    logic [3:0] li, lj;
    begin
      li = 4'(vi - j_ox);
      lj = 4'(vj - j_oz);
      win_idx = IDX_W'({lj, 3'b0}) + IDX_W'(lj) + IDX_W'(li);
    end
  endfunction

  // ModeVtx: the vertex being fetched is slot 0 = (ea, eb) at stride 1
  wire [IDX_W-1:0] v_idx = IDX_W'({eb, 3'b0}) + IDX_W'(eb) + IDX_W'(ea);
  // ModeRef: the three corners of the current triangle, TOP order
  wire [IDX_W-1:0] t_idx0 = win_idx(tv_i[0], tv_j[0]);
  wire [IDX_W-1:0] t_idx1 = win_idx(tv_i[1], tv_j[1]);
  wire [IDX_W-1:0] t_idx2 = win_idx(tv_i[2], tv_j[2]);

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
  wire signed [31:0] m_hc = zhao_tp_fx_add_sat(v_ha, m_half);
  // y = h + fx_mul(morph, hc - h): §4.3's shape, an exact add of a rounded
  // delta. morph = 0 gives h and morph = 65536 gives hc, both bit-exactly.
  wire signed [33:0] m_d = {{2{m_hc[31]}}, m_hc} - {{2{vh[pend_slot][31]}}, vh[pend_slot]};

  // ---- G8B T1: THE BLEND IS SPLIT AND THE LANDING MOVES WITH IT -----------
  //
  // MEASURED. The first G8B fit's worst path was this chain, end to end:
  //
  //   lat_h_q -> u_tess|vs_y[0]        -12.758 ns, data 22.481, skew -0.097
  //
  // Logic depth, not placement: a 34-bit subtract, a rescale, a saturating
  // add, a second 34-bit subtract, a 17x34 signed multiply, a second rescale
  // and a second saturating add, all between the registered lattice response
  // and the vertex registers.
  //
  // The split is at `m_d`, which halves it: stage A keeps the two subtracts,
  // `rescale1` and the first `fx_add_sat`; stage B takes the multiply,
  // `rescale16` and the second `fx_add_sat`. Stage B is the longer half at
  // roughly 13 ns, which clears T1's acceptance of better than -4.388 ns --
  // the level `zhao_project_core` and the RAM paths now share after T2.
  //
  // WHY THE LANDING HAS TO MOVE TOO, rather than only the arithmetic. `m_y` is
  // consumed by the kind-2 capture, by the ModeVtx skid landing and by the
  // ModeTri triangle emit, and the last read of a job is exactly the one whose
  // blend those consumers need. So a stage in the blend is a stage in the
  // landing, and everything the landing reads has to travel with it -- reading
  // `vx[0]`/`vy[1]` a cycle later would read the NEXT job's captures, because
  // the enumerator advances at issue and the next job's first read can be
  // issued on the same edge this one lands.
  //
  // WHAT IS NOT MOVED, deliberately: `vh[]` and `v_ha` are still written on the
  // response edge, because stage A's own arithmetic reads them. Moving those
  // would make the blend read its own delayed inputs.
  //
  // `rescale1`, `rescale16` and `fx_add_sat` are untouched, so the arithmetic
  // is the same expression in the same order -- `terrain_pipe_differential`
  // stays bit-exact against `zhao_terrain_project`.
  logic                    lnd_v_q;
  logic [1:0]              lnd_kind_q, lnd_slot_q;
  logic                    lnd_last_q;
  logic [IDX_W-1:0]        lnd_idx_q;
  logic                    lnd_stride_q;
  logic signed [33:0]      lnd_md_q;
  logic signed [31:0]      lnd_vh_q;
  logic signed [31:0]      lnd_x_q, lnd_z_q, lnd_h_q;
  // The triangle's other two corners, snapshotted with the landing so the emit
  // cannot read a successor job's captures.
  logic signed [31:0]      lnd_ax_q, lnd_az_q;
  logic signed [31:0]      lnd_bx_q, lnd_bz_q;
  // THE JOB'S OWN PARAMETERS TRAVEL WITH ITS VERTEX, and they have to.
  // `j_morph`, `j_surface` and `j_src` are JOB registers: read them at stage B
  // and a vertex whose job has since been replaced is blended with the NEXT
  // job's morph factor and emitted under the next job's winding. That is not
  // hypothetical -- terrain_tess_directed sweeps every morph factor, and with
  // these read live it reported the same blended y for factors that must
  // differ, because every vertex was using its successor's morph.
  logic [16:0]             lnd_morph_q;
  logic                    lnd_surface_q;
  logic [15:0]             lnd_src_q;

  // ---- G8B T1b: THE MULTIPLY'S OUTPUT REGISTER, WHICH WAS AGAIN UNUSED -----
  //
  // MEASURED, `@g8b-t12`. T1's split took the chain from 22.481 ns of data
  // delay to 16.383 and the subsystem from 43.54 to 57.87 MHz -- real, and NOT
  // the 69.5 the campaign brief predicted, because the brief estimated this
  // remaining half at "roughly 13 ns" by reading the expression and counting
  // operators. It is 16.4, and the tessellator is STILL the worst block:
  //
  //   u_tess|lnd_morph_q[0] -> u_tess|vo_y[7]     -7.280 ns, data 16.383
  //
  // (`vs_y`/`vo_y` above are the names the skid carried at the time each fit
  // ran. T1b replaced the three named slots with the `vq_*` queue, so those
  // endpoints will not be found by grep in this file -- they are quoted as the
  // fit reported them, not as the design now spells them.)
  //
  // What is left is one 17x34 multiply, one rescale and one saturating add, and
  // THE DSP'S OUTPUT REGISTER IS UNUSED -- `m_prod` feeds `rescale16`
  // combinationally before anything is registered. That is the identical
  // finding T2 cashed in `zhao_project_core`, in the block next door, and it is
  // worth saying plainly: the same unused output register was sitting in two
  // blocks of one subsystem, and reading one did not make anyone look at the
  // other.
  //
  // So the product is registered. The split is roughly 10 / 6 rather than the
  // 6 / 16 T1 actually achieved, and it costs a THIRD in-flight stage.
  logic signed [51:0]      ln2_prod_q;
  logic                    ln2_v_q;
  logic [1:0]              ln2_kind_q, ln2_slot_q;
  logic                    ln2_last_q;
  logic [IDX_W-1:0]        ln2_idx_q;
  logic                    ln2_stride_q;
  logic signed [31:0]      ln2_vh_q;
  logic signed [31:0]      ln2_x_q, ln2_z_q, ln2_h_q;
  logic signed [31:0]      ln2_ax_q, ln2_az_q;
  logic signed [31:0]      ln2_bx_q, ln2_bz_q;
  logic                    ln2_surface_q;
  logic [15:0]             ln2_src_q;

  // stage B: the multiply, and nothing else.
  wire signed [51:0] m_prod = $signed({1'b0, lnd_morph_q}) * lnd_md_q;
  // stage C: the rescale and the saturating add, on the registered product.
  // `rescale16` and `fx_add_sat` are untouched and applied in the same order to
  // the same values, so the arithmetic is bit-identical and
  // terrain_pipe_differential stays exact against zhao_terrain_project.
  wire signed [31:0] m_step = rescale16(ln2_prod_q);
  // m_y moved to stage D as m_y_d (T10).

  // Stage A's view of the last slot: the x/z mux sits on the register inputs,
  // not in the blend cone.
  wire signed [31:0] last_x = (pend_kind == 2'd0) ? lat_wx_i : (j_vtx ? vx[0] : vx[2]);
  wire signed [31:0] last_z = (pend_kind == 2'd0) ? lat_wz_i : (j_vtx ? vz[0] : vz[2]);

  // Stage C's landed values -- what every consumer below now reads.
  // ---- G8B T10: stage D, and WHY IT HOLDS `step` RATHER THAN THE LANDING -
  //
  // @g8b-t8-pins left one failing family on physical pins:
  //
  //     ln2_prod_q[22] -> vq_y[1][30]      -0.368 ns
  //
  // which is `rescale16` (a 52-bit add, shift and saturate) followed by
  // `fx_add_sat` (a 34-bit add and saturate) -- two dependent carry chains in
  // one cycle. Seed 2 measured WORSE (94.42, TNS -10.4 against seed 1's
  // 96.45 / -1.603), so this is a structural gap and not placement luck.
  //
  // The obvious stage -- register the LANDING -- buys nothing: the path would
  // become ln2_prod_q -> rescale16 -> fx_add_sat -> ln3_y_q, the same logic
  // with a different endpoint. The stage has to fall BETWEEN the two adds. So
  // stage D holds `m_step`, the rescale's 32-bit result, and performs the
  // saturating add itself:
  //
  //     C: ln2_prod_q -> rescale16      -> ln3_step_q
  //     D: ln3_step_q -> fx_add_sat     -> vy[] / the queues
  //
  // It is also CHEAPER in flops than carrying the product would be: 32 bits
  // instead of 52.
  logic                    ln3_v_q;
  logic [1:0]              ln3_kind_q, ln3_slot_q;
  logic                    ln3_last_q;
  logic [IDX_W-1:0]        ln3_idx_q;
  logic                    ln3_stride_q;
  logic signed [31:0]      ln3_step_q;   // rescale16(prod), the C->D cut
  logic signed [31:0]      ln3_vh_q;
  logic signed [31:0]      ln3_x_q, ln3_z_q, ln3_h_q;
  logic signed [31:0]      ln3_ax_q, ln3_ay_q, ln3_az_q;
  logic signed [31:0]      ln3_bx_q, ln3_by_q, ln3_bz_q;
  logic                    ln3_surface_q;
  logic [15:0]             ln3_src_q;

  // The blend's final value, now computed at D from registered operands.
  wire signed [31:0] m_y_d = zhao_tp_fx_add_sat(ln3_vh_q, ln3_step_q);

  wire signed [31:0] land_x = ln3_x_q;
  wire signed [31:0] land_z = ln3_z_q;
  wire signed [31:0] land_y = (ln3_kind_q == 2'd0) ? ln3_h_q : m_y_d;

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
      j_vtx <= 1'b0;
      j_ref <= 1'b0;
      j_vshift <= '0;
      j_plain_hi <= 4'd7;
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
      win_mask_q <= window_mask(4'd0, 4'd0, 2'd0);
      i0_q <= 6'd0;
      j0_q <= 6'd0;
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
      pend_idx <= '0;
      pend_stride <= 1'b0;
      // G8B T1's blend stage.
      lnd_v_q <= 1'b0;
      lnd_kind_q <= '0;
      lnd_slot_q <= '0;
      lnd_last_q <= 1'b0;
      lnd_idx_q <= '0;
      lnd_stride_q <= 1'b0;
      lnd_md_q <= '0;
      lnd_vh_q <= '0;
      lnd_x_q <= '0;
      lnd_z_q <= '0;
      lnd_h_q <= '0;
      lnd_ax_q <= '0; lnd_az_q <= '0;
      lnd_bx_q <= '0; lnd_bz_q <= '0;
      lnd_morph_q <= '0; lnd_surface_q <= 1'b0; lnd_src_q <= '0;
      // G8B T1b's registered-product stage.
      ln2_v_q <= 1'b0;
      ln2_prod_q <= '0;
      ln2_kind_q <= '0;
      ln2_slot_q <= '0;
      ln2_last_q <= 1'b0;
      ln2_idx_q <= '0;
      ln2_stride_q <= 1'b0;
      ln2_vh_q <= '0;
      ln2_x_q <= '0;
      ln2_z_q <= '0;
      ln2_h_q <= '0;
      ln2_ax_q <= '0; ln2_az_q <= '0;
      ln2_bx_q <= '0; ln2_bz_q <= '0;
      ln2_surface_q <= 1'b0; ln2_src_q <= '0;
      ln3_v_q <= 1'b0;
      ln3_step_q <= '0;
      ln3_kind_q <= '0;
      ln3_slot_q <= '0;
      ln3_last_q <= 1'b0;
      ln3_idx_q <= '0;
      ln3_stride_q <= 1'b0;
      ln3_vh_q <= '0;
      ln3_x_q <= '0;
      ln3_z_q <= '0;
      ln3_h_q <= '0;
      ln3_ax_q <= '0; ln3_ay_q <= '0; ln3_az_q <= '0;
      ln3_bx_q <= '0; ln3_by_q <= '0; ln3_bz_q <= '0;
      ln3_surface_q <= 1'b0; ln3_src_q <= '0;
      tri_par <= 1'b0; pend_par <= 1'b0;
      lnd_par_q <= 1'b0; ln2_par_q <= 1'b0; ln3_par_q <= 1'b0;
      vy1[0] <= '0; vy1[1] <= '0; vy1[2] <= '0;
      v_ha <= '0;
      for (int k = 0; k < VQ_DEPTH; k++) begin
        vq_valid[k]  <= 1'b0;
        vq_x[k]      <= '0;
        vq_y[k]      <= '0;
        vq_z[k]      <= '0;
        vq_idx[k]    <= '0;
        vq_stride[k] <= 1'b0;
      end
      r_valid <= 1'b0;
      r_ia <= '0;
      r_ib <= '0;
      r_ic <= '0;
      terrain_vertices_emitted_o <= '0;
      terrain_refs_emitted_o <= '0;
      mode_invalid_o <= '0;
      for (int p = 0; p < 3; p++) begin
        vx[p] <= '0;
        vz[p] <= '0;
        vh[p] <= '0;
        vy[p] <= '0;
      end
      for (int k = 0; k < TQ_DEPTH; k++) begin
        tq_valid[k] <= 1'b0;
        tq_ax[k] <= '0; tq_ay[k] <= '0; tq_az[k] <= '0;
        tq_bx[k] <= '0; tq_by[k] <= '0; tq_bz[k] <= '0;
        tq_cx[k] <= '0; tq_cy[k] <= '0; tq_cz[k] <= '0;
        tq_src[k] <= '0;
        tq_surf[k] <= 1'b0;
      end
      terrain_triangles_emitted_o <= '0;
      subpatch_rejected_o <= '0;
      lod_clamped_o <= '0;
      job_reject_o <= 1'b0;
    end else begin
      job_reject_o <= 1'b0;

      // ---- ModeTri queue: pop shifts down, an emit appends ------------------
      // Same two-step shape as the vertex queue below: the emit's assignment
      // comes second, so it wins on the index they share and emit-with-pop
      // needs no arm of its own.
      if (tpop) begin
        for (int k = 0; k < TQ_DEPTH; k++) begin
          if (k + 1 < TQ_DEPTH) begin
            tq_valid[k] <= tq_valid[k+1];
            tq_ax[k] <= tq_ax[k+1]; tq_ay[k] <= tq_ay[k+1]; tq_az[k] <= tq_az[k+1];
            tq_bx[k] <= tq_bx[k+1]; tq_by[k] <= tq_by[k+1]; tq_bz[k] <= tq_bz[k+1];
            tq_cx[k] <= tq_cx[k+1]; tq_cy[k] <= tq_cy[k+1]; tq_cz[k] <= tq_cz[k+1];
            tq_src[k] <= tq_src[k+1];
            tq_surf[k] <= tq_surf[k+1];
          end else begin
            tq_valid[k] <= 1'b0;
          end
        end
      end

      // ---- ModeVtx queue: pop shifts down, a landing appends ----------------
      // Two independent steps rather than three interleaved arms. A pop shifts
      // every entry down one and clears the tail; a landing then writes the
      // slot the entry belongs in, which is the post-shift occupancy. The
      // landing's assignment comes second, so where both touch the same index
      // the landing wins -- that is the land-and-pop case, and it needs no arm
      // of its own.
      //
      // The credit (`vtx_room`) guarantees `vland_idx <= VQ_DEPTH-1`, so the
      // append is never an overwrite. Entries stay contiguous from index 0, so
      // `vocc` is both the occupancy and the next free index.
      if (vpop) begin
        for (int k = 0; k < VQ_DEPTH; k++) begin
          if (k + 1 < VQ_DEPTH) begin
            vq_valid[k]  <= vq_valid[k+1];
            vq_x[k]      <= vq_x[k+1];
            vq_y[k]      <= vq_y[k+1];
            vq_z[k]      <= vq_z[k+1];
            vq_idx[k]    <= vq_idx[k+1];
            vq_stride[k] <= vq_stride[k+1];
          end else begin
            vq_valid[k] <= 1'b0;
          end
        end
      end
      if (vland) begin
        vq_valid[vland_idx]  <= 1'b1;
        vq_x[vland_idx]      <= land_x;
        vq_y[vland_idx]      <= land_y;
        vq_z[vland_idx]      <= land_z;
        vq_idx[vland_idx]    <= ln3_idx_q;
        vq_stride[vland_idx] <= ln3_stride_q;
      end
      if (vland && terrain_vertices_emitted_o != 32'hFFFF_FFFF)
        terrain_vertices_emitted_o <= terrain_vertices_emitted_o + 32'd1;

      // ---- ModeRef triple register --------------------------------------------
      if (r_valid && ref_ready_i) r_valid <= 1'b0;
      if (ref_emit) begin
        // The underside is the top's pair with b and c swapped — the same ONE
        // mux as the world-coordinate path below, on indices.
        r_valid <= 1'b1;
        r_ia <= t_idx0;
        r_ib <= j_surface ? t_idx2 : t_idx1;
        r_ic <= j_surface ? t_idx1 : t_idx2;
        if (terrain_refs_emitted_o != 32'hFFFF_FFFF)
          terrain_refs_emitted_o <= terrain_refs_emitted_o + 32'd1;
      end

      case (st)
        StIdle: begin
          if (job_valid_i) begin
            automatic logic [1:0] lv_nx, lv_pz, lv_px, lv_nz;
            automatic logic [3:0] s_new, n_new;
            automatic logic stitch_new;
            automatic logic vtx_new, ref_new;
            automatic logic [3:0] e_start_c;  // first run-cell, 0 or 1
            // the mode: anything that is not one of the three is counted and
            // runs as ModeTri, never silently
            vtx_new = (job_mode_i == ModeVtx);
            ref_new = (job_mode_i == ModeRef);
            j_vtx <= vtx_new;
            j_ref <= ref_new;
            if (job_mode_i != ModeTri && !vtx_new && !ref_new && mode_invalid_o != 32'hFFFF_FFFF)
              mode_invalid_o <= mode_invalid_o + 32'd1;
            j_ox <= job_ox_i;
            j_oz <= job_oz_i;
            j_level <= job_level_i;
            s_new = 4'(4'd1 << job_level_i);
            n_new = 4'(4'd8 >> job_level_i);
            j_s <= s_new;
            j_n <= n_new;
            j_vshift <= vtx_new ? 2'd0 : job_level_i;
            j_plain_hi <= vtx_new ? 4'(Side - 1) : (n_new - 4'd1);
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
            end else if (job_dual_i && !(vtx_new && !stitch_new)) begin
              // The scan serves the void skips (ModeTri/ModeRef) and the
              // stitched+void reject (every mode, law 7). An UNSTITCHED
              // ModeVtx job consumes neither and skips it (law 7).
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
              // (Also the unstitched ModeVtx path on a dual page, see above;
              // ModeVtx always walks the plain 9x9 window at stride 1.)
              solid <= {64{1'b1}};
              emode <= (!stitch_new || vtx_new) ? EmPlain : ((n_new < 4'd3) ? EmFan : EmInner);
              // PAIRED, and it takes job_level_i rather than j_level: j_level is
              // assigned on this same edge, so the consumer at the next cycle sees
              // the level this job was offered with, which is the same value.
              // (T4 changed the third argument from the decoded stride to the
              // level; before it, this line read s_new for the identical reason.)
              // The report names this exact hazard.
              e_start_c = (stitch_new && n_new >= 4'd3 && !vtx_new) ? 4'd1 : 4'd0;
              ea <= e_start_c;
              eb <= e_start_c;
              win_mask_q <= window_mask(e_start_c, e_start_c, job_level_i);
              // Same rule as the mask above and it bites harder here: j_ox, j_oz
              // and j_vshift are ALL assigned on this same edge, so the pair must
              // be built from the offered ox/oz and the offered shift, never from
              // the registers, which still hold the previous job's subpatch.
              i0_q <= cell_base(job_ox_i, e_start_c, vtx_new ? 2'd0 : job_level_i);
              j0_q <= cell_base(job_oz_i, e_start_c, vtx_new ? 2'd0 : job_level_i);
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
              automatic logic [3:0] e_start_c;  // first run-cell, 0 or 1
              // ModeVtx reaches here only when stitched (for the reject above)
              // and still walks the plain window.
              emode <= (j_stitch && !j_vtx) ? EmInner : EmPlain;
              e_start_c = (j_stitch && !j_vtx) ? 4'd1 : 4'd0;
              ea <= e_start_c;
              eb <= e_start_c;
              win_mask_q <= window_mask(e_start_c, e_start_c, j_level);
              i0_q <= cell_base(j_ox, e_start_c, j_vshift);
              j0_q <= cell_base(j_oz, e_start_c, j_vshift);
              etri <= 1'b0;
              eside <= '0;
              eg <= '0;
              efan <= '0;
              f_slot <= '0;
              f_kind <= '0;
              // n <= 3 leaves the annulus with no inner block at all
              if (j_stitch && j_n < 4'd3 && !j_vtx) emode <= EmFan;
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
            // This read belongs to the CURRENT triangle, so it carries the
            // current bank; the flip on the same edge sends the next
            // triangle's reads to the other one.
            pend_par  <= tri_par;
            if (iss_last) tri_par <= ~tri_par;
            pend_idx <= v_idx;
            pend_stride <= on_grid_c;
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

          // ---- stage A: capture the read issued last cycle -----------------
          // vh/v_ha/vx/vz stay HERE, on the response edge, because stage A's
          // own blend arithmetic reads them. Only the blend's second half and
          // everything that consumes it moved to stage B below.
          if (pend_v) begin
            if (pend_kind == 2'd0) begin
              vx[pend_slot] <= lat_wx_i;
              vz[pend_slot] <= lat_wz_i;
              vh[pend_slot] <= lat_h_i;
              if (pend_par) vy1[pend_slot] <= lat_h_i;
              else          vy[pend_slot]  <= lat_h_i;
            end else if (pend_kind == 2'd1) begin
              v_ha <= lat_h_i;
            end

          end

          // ---- stage A -> stage B ------------------------------------------
          // The blend's first half, the landed x/z, and the two corners the
          // emit will need, all snapshotted together on the response edge.
          lnd_v_q <= pend_v;
          if (pend_v) begin
            lnd_kind_q   <= pend_kind;
            lnd_slot_q   <= pend_slot;
            lnd_last_q   <= pend_last;
            lnd_idx_q    <= pend_idx;
            lnd_stride_q <= pend_stride;
            lnd_md_q     <= m_d;
            lnd_vh_q     <= vh[pend_slot];
            lnd_x_q      <= last_x;
            lnd_z_q      <= last_z;
            lnd_h_q      <= lat_h_i;
            lnd_ax_q     <= vx[0];
            lnd_az_q     <= vz[0];
            lnd_bx_q     <= vx[1];
            lnd_bz_q     <= vz[1];
            // WRITE-FORWARD, and it is not optional. `vy[]` is the one corner
            // field the blend writes, and that write happens at the LANDING
            // stage -- the same edge this snapshot is taken on. A slot whose
            // blend is completing right now would be captured at its stale
            // pre-blend value, which is exactly what happens when the last slot
            // carries no morph and its only read lands one cycle after the
            // previous slot's blend. terrain_tess_directed caught it as a wrong
            // `b` corner with a correct `a` and `c`.
            //
            // T1b MOVED THE LANDING AGAIN, from stage B to stage C, so this
            // forward now keys on `ln2_*` and not `lnd_*`. Leaving it on stage B
            // would forward a value that is no longer being written there and
            // miss the one that is -- wrong in both directions at once.
            // FORWARD FROM STAGE D, not C: `vy[]` is written where the blend
            // COMPLETES, and since T10 that is D. Comparing against ln2_* here
            // would forward a value that is still one add away from existing.
            lnd_par_q    <= pend_par;
            lnd_morph_q  <= j_morph;
            lnd_surface_q <= j_surface;
            lnd_src_q    <= j_src;
          end

          // ---- stage B -> stage C ------------------------------------------
          // Only the PRODUCT is computed here; everything the landing will read
          // travels with it. `lnd_md_q` and `lnd_morph_q` do not: the multiply
          // consumes them.
          //
          // The same write-forward hazard applies one stage earlier: `vy[]` is
          // written at stage C on the edge this snapshot is taken, so a slot
          // whose blend completes now must be forwarded here too.
          ln2_v_q <= lnd_v_q;
          if (lnd_v_q) begin
            ln2_prod_q   <= m_prod;
            ln2_kind_q   <= lnd_kind_q;
            ln2_slot_q   <= lnd_slot_q;
            ln2_last_q   <= lnd_last_q;
            ln2_idx_q    <= lnd_idx_q;
            ln2_stride_q <= lnd_stride_q;
            ln2_vh_q     <= lnd_vh_q;
            ln2_x_q      <= lnd_x_q;
            ln2_z_q      <= lnd_z_q;
            ln2_h_q      <= lnd_h_q;
            ln2_ax_q     <= lnd_ax_q;
            ln2_az_q     <= lnd_az_q;
            ln2_bx_q     <= lnd_bx_q;
            ln2_bz_q     <= lnd_bz_q;
            ln2_par_q    <= lnd_par_q;
            ln2_surface_q <= lnd_surface_q;
            ln2_src_q    <= lnd_src_q;
          end

          // ---- stage C -> stage D ------------------------------------------
          // The rescale happens HERE, on the C->D register input; the saturating
          // add happens at D. That is the whole of T10: one carry chain per
          // cycle instead of two.
          //
          // The same write-forward applies a third time, for the same reason it
          // applies at A->B and B->C: `vy[]` is written at D on the edge this
          // snapshot is taken, so a slot whose blend completes now must be
          // forwarded here too. Three snapshots, one landing, three forwards.
          ln3_v_q <= ln2_v_q;
          if (ln2_v_q) begin
            ln3_step_q   <= m_step;
            ln3_kind_q   <= ln2_kind_q;
            ln3_slot_q   <= ln2_slot_q;
            ln3_last_q   <= ln2_last_q;
            ln3_idx_q    <= ln2_idx_q;
            ln3_stride_q <= ln2_stride_q;
            ln3_vh_q     <= ln2_vh_q;
            ln3_x_q      <= ln2_x_q;
            ln3_z_q      <= ln2_z_q;
            ln3_h_q      <= ln2_h_q;
            ln3_ax_q     <= ln2_ax_q;
            ln3_az_q     <= ln2_az_q;
            ln3_bx_q     <= ln2_bx_q;
            ln3_bz_q     <= ln2_bz_q;
            // Read this triangle's own bank. The one case the bank cannot
            // already hold is a blend landing on THIS edge, which is the only
            // forward left.
            ln3_ay_q     <= (ln3_v_q && ln3_kind_q == 2'd2 && ln3_slot_q == 2'd0)
                            ? m_y_d : vy_bank(ln2_par_q, 2'd0);
            ln3_by_q     <= (ln3_v_q && ln3_kind_q == 2'd2 && ln3_slot_q == 2'd1)
                            ? m_y_d : vy_bank(ln2_par_q, 2'd1);
            ln3_par_q    <= ln2_par_q;
            ln3_surface_q <= ln2_surface_q;
            ln3_src_q    <= ln2_src_q;
          end

          // ---- stage D: the blend completes, and everything that reads it --
          if (ln3_v_q) begin
            if (ln3_kind_q == 2'd2) begin
              if (ln3_par_q) vy1[ln3_slot_q] <= m_y_d;
              else           vy[ln3_slot_q]  <= m_y_d;
            end

            // ModeTri only: a ModeVtx landing goes to the queue above.
            if (ln3_last_q && !j_vtx) begin
              // The underside is the top's pair with b and c swapped — the ONE
              // place the inverted winding lives.
              tq_valid[tland_idx] <= 1'b1;
              tq_src[tland_idx]   <= ln3_src_q;
              tq_surf[tland_idx]  <= ln3_surface_q;
              tq_ax[tland_idx]    <= ln3_ax_q;
              tq_ay[tland_idx]    <= ln3_ay_q;
              tq_az[tland_idx]    <= ln3_az_q;
              if (!ln3_surface_q) begin
                tq_bx[tland_idx] <= ln3_bx_q;
                tq_by[tland_idx] <= ln3_by_q;
                tq_bz[tland_idx] <= ln3_bz_q;
                tq_cx[tland_idx] <= land_x;
                tq_cy[tland_idx] <= land_y;
                tq_cz[tland_idx] <= land_z;
              end else begin
                tq_bx[tland_idx] <= land_x;
                tq_by[tland_idx] <= land_y;
                tq_bz[tland_idx] <= land_z;
                tq_cx[tland_idx] <= ln3_bx_q;
                tq_cy[tland_idx] <= ln3_by_q;
                tq_cz[tland_idx] <= ln3_bz_q;
              end
              terrain_triangles_emitted_o <= terrain_triangles_emitted_o + 32'd1;
            end
          end

          // ---- advance the enumerator ------------------------------------
          // A void run-cell is SKIPPED here, at one cycle per skipped cell and
          // no lattice read at all.
          // ModeRef advances when a triple is loaded (no read to issue);
          // ModeVtx has no second triangle per cell.
          if (cell_skip || (do_issue && iss_last) || ref_emit) begin
            if (!cell_skip && !etri && emode != EmFan && !j_vtx) begin
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
              automatic logic [3:0] ea_n_c, eb_n_c;
              // the next run-cell in z-then-x scan order
              //
              // ONE computation, used for BOTH the state and the mask. The two
              // advance arms are where the report expects a paired assignment to
              // be missed, so they do not get two chances to disagree: ea_n/eb_n
              // are decided here and then written once each.
              etri <= 1'b0;
              ea_n_c = ea;
              eb_n_c = eb;
              if (ea >= cell_hi) begin
                ea_n_c = cell_lo;
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
                  eb_n_c = eb + 4'd1;
                end
              end else begin
                ea_n_c = ea + 4'd1;
              end
              ea <= ea_n_c;
              eb <= eb_n_c;
              win_mask_q <= window_mask(ea_n_c, eb_n_c, j_level);
              i0_q <= cell_base(j_ox, ea_n_c, j_vshift);
              j0_q <= cell_base(j_oz, eb_n_c, j_vshift);
            end
          end

          // `lnd_v_q` JOINS THIS, and it is not optional. The state machine
          // already refuses to leave StTri while a read is in flight; G8B T1's
          // blend split added a SECOND in-flight stage, and the stage-A capture
          // and stage-B landing both live inside this case. Leaving while the
          // blend still holds a vertex would strand its landing -- the triangle
          // is never emitted and the job never drains, which is exactly what
          // the dense sparse-fill fault control reported.
          // `ln3_v_q` JOINS THIS, and it is not optional -- a stage left out of
          // the drain makes the block declare a job finished while it still holds
          // a vertex, which is the same class of omission as leaving one out of
          // `busy_o` in zhao_project_core.
          if (done && !pend_v && !lnd_v_q && !ln2_v_q && !ln3_v_q &&
              (tocc == 2'd0) && (vocc == 3'd0) && !r_valid) st <= StIdle;
        end

        default: st <= StIdle;
      endcase
    end
  end

  assign tri_valid_o = tq_valid[0];

  assign vtx_valid_o = vq_valid[0];
  assign vtx_x_o = vq_x[0];
  assign vtx_y_o = vq_y[0];
  assign vtx_z_o = vq_z[0];
  assign vtx_index_o = vq_idx[0];
  assign vtx_stride_o = vq_stride[0];
  assign vtx_surface_o = j_surface;
  assign vtx_src_id_o = j_src;

  assign ref_valid_o = r_valid;
  assign ref_ia_o = r_ia;
  assign ref_ib_o = r_ib;
  assign ref_ic_o = r_ic;
  assign ref_surface_o = j_surface;
  assign ref_src_id_o = j_src;
  assign ax_o = tq_ax[0];
  assign ay_o = tq_ay[0];
  assign az_o = tq_az[0];
  assign bx_o = tq_bx[0];
  assign by_o = tq_by[0];
  assign bz_o = tq_bz[0];
  assign cx_o = tq_cx[0];
  assign cy_o = tq_cy[0];
  assign cz_o = tq_cz[0];
  assign surface_o = tq_surf[0];
  assign src_id_o = tq_src[0];
  // EVERY IN-FLIGHT STAGE JOINS THIS REDUCTION. A read that has been issued but
  // not yet landed is work in flight; T1 made that two stages and T1b three.
  // Reporting idle while any of them holds a vertex is the queue-occupancy
  // defect -- and it is not theoretical here: the dense sparse-fill fault
  // control drives a job that faults mid-flight, and with the blend stage
  // uncounted the drain never completed.
  assign idle_o = (st == StIdle) && (tocc == 2'd0) && (vocc == 3'd0) &&
                  !r_valid && !pend_v && !lnd_v_q && !ln2_v_q && !ln3_v_q;

endmodule : zhao_terrain_tess
