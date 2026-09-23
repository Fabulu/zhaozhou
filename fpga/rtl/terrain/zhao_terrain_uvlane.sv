// zhao_terrain_uvlane.sv -- TERRAIN.UV: the terrain texture-coordinate law in
// hardware, and the FIRST LINK of the textured-terrain chain.
//
// Law, in citation order:
//   spec/terrain_rules.md 1.3 -- cell pitch is per-island, restricted to
//       {0.5, 1.0, 2.0, 4.0} m, encoded `pitch_log2 in {-1, 0, +1, +2}`.
//   spec/terrain_rules.md 6.2 (FROZEN 2026-08-16, capture-exact) -- "u in
//       Q16.16 TILE units, one tile period per cell on tops, per STRATA_M on
//       walls/underside". The mirrored-repeat FOLD that consumes these
//       coordinates is the TMU sampler's law and is NOT in this block
//       (`zhao_texture_tmu.sv`, `zref::terrain::mirror_texel`).
//   spec/terrain_rules.md 6.6 -- the underside samples planar world UV
//       (wx/STRATA_M, wz/STRATA_M), STRATA_M default 8 m.
//   reference/src/zrender/terrain.cpp:581-612 -- THE law, as executed:
//
//       int top_shift = 0;
//       ... if (pr == 1) top_shift = sh - 16;   // pitch = 2^k m -> u = wx >> k
//       if (top_shift < 0) top_shift = 0;       // <-- THE CLAMP
//       u_top[k] = wx[i] >> top_shift;  v_top[k] = wz[j] >> top_shift;
//       u_und[k] = wx[i] >> 3;          v_und[k] = wz[j] >> 3;
//
// ===========================================================================
// THE CLAMP IS NOT DECORATION, AND IT IS THE ONE LINE A SUMMARY LOSES
// ===========================================================================
// `pitch_log2 = -1` (0.5 m) is a LEGAL pitch by spec 1.3 and is accepted by
// `zhao_terrain_place` (`SH_HALF = 15`). The oracle clamps its shift to ZERO
// there rather than shifting left. An RTL `u = wx >>> pitch_log2` with a
// signed shift amount would diverge from the oracle on exactly one of the four
// legal pitches, quietly, on the pitch nobody tests first. `pitch_clamped_o`
// counts every fill where the clamp changed the answer, so the case is
// OBSERVABLE rather than merely handled.
//
// ===========================================================================
// WHY THIS IS A STORE BESIDE THE ARENA, AND NOT A WIDER PACKET
// ===========================================================================
// This is `zhao_terrain_lightlane`'s shape, deliberately, because it is the
// same problem: a PER-VERTEX quantity that the projected arena does not carry.
// That block's header records the three candidates and the ruling --
//
//   * widen the arena: a change to `zhao_vertex_arena`, whose SymbiYosys proof
//     and six refusals are the reason terrain reuses it;
//   * recompute at replay: a second implementation of the lattice law;
//   * a store beside the arena, keyed by the same {arena, index}, written on
//     the same beat.
//
// -- and R11/R21 ruled the third. This block is that ruling applied to the
// quantity terrain_rules 6.2 needs. NOTHING JOINS TWO STREAMS: the key is the
// producer's own, by construction, so no stall can pair vertex A's position
// with vertex B's coordinates. That is CLAUDE.md's "a record's metadata must
// travel WITH the record", and it is why the alternative -- widening
// `zhao_project_core`'s payload, ~32 flops per bit through the divider's
// shift-register family (that file's own line 904) and charged to the GEOMETRY
// client too, since the core is SHARED -- was not taken.
//
// ===========================================================================
// THE COORDINATES ARE COMPUTED AT FILL, NOT AT READ, AND THAT IS CORRECTNESS
// ===========================================================================
// Storing {u, v} costs 64 bits per row against 64 for {wx, wz}, so the choice
// is not an area argument. It is that `pitch_log2` is FRAME-SCOPED state: a
// vertex's world position was PLACED at the pitch live when it was placed
// (`zhao_terrain_place`: wx(i) = (patch_ix*32 + i) <<< (16 + pitch_log2)), so
// the shift that maps it to tile units is that same pitch. Shifting at READ
// time with a then-current pitch would re-map an old vertex with a new
// island's pitch on any frame where the two differ -- a wrong number with no
// counter able to see it. Computed at fill, the pitch and the vertex are the
// same beat's facts.
//
// The SURFACE bit is taken on that same beat for the same reason. It is a
// per-JOB quantity (`zhao_terrain_tess.job_surface_i`, 0 = top, 1 = underside)
// and `zhao_terrain_group_seq` passes its vertex stream through combinationally
// while holding that job's surface, so the bit presented with a vertex is the
// bit of the job that produced it. Reading a surface wire at REPLAY instead
// would pair a vertex with whatever job happened to be open then.
//
// ===========================================================================
// WHAT THIS BLOCK DOES NOT DO, DELIBERATELY
// ===========================================================================
//   * NO WALLS. terrain_rules 6.6 gives rim walls a different law entirely --
//     "Wall U accumulates rim length in lattice scan order ... V = (top - y)/
//     STRATA_M per vertex" -- which is NOT a function of world x/z and is
//     FORGE.CLIFF's (`zhao_forge_cliff.sv` states its emission stage is not
//     written). A summary of this law that says "underside and walls are the
//     same coordinates >> 3" is WRONG about walls; only the underside is
//     planar world UV. This block serves the two surfaces the tessellator
//     actually emits, which are top and underside, and refuses to guess the
//     third.
//   * NO FOLD. The mirrored repeat is the TMU's (terrain_rules 6.2) and is
//     already implemented there. Folding here would be a second implementation
//     of a frozen law.
//   * NO PERSPECTIVE DIVIDE. GEOM.CLIP's attribute slots 1 and 2 are u/w and
//     v/w; this block emits u and v. The multiply by invw belongs with the
//     `invw24` producer (a `zhao_geom_depthquant_stream` client and a
//     `pack_attr` analogue), which is the NEXT link and is not built here.
//   * NO MOSAIC PICK. Layer E's {mat_a, mat_b, weight} rides the projector
//     unselected and the pick is per TEXEL, which is TEXTURE.MOSAIC's.
//
// Conservative SystemVerilog subset only (charter 2); no package deps.
`default_nettype none

module zhao_terrain_uvlane #(
    parameter int unsigned ARENAS  = 4,
    parameter int unsigned DEPTH   = 81,
    parameter int unsigned GEN_W   = 8,
    parameter int unsigned SRCW    = 16,
    parameter int unsigned ARENA_W = $clog2(ARENAS) + 1,
    parameter int unsigned INDEX_W = $clog2(DEPTH) + 1
) (
    input var logic clk,
    input var logic rst_n,

    // ---- the fill beat: client B's ACCEPTED vertex ---------------------------
    // The same handshake that writes the projector's arena, so this store
    // cannot hold a vertex the arena does not.
    input var logic                 fill_valid_i,
    input var logic                 fill_ready_i,
    input var logic [ARENA_W-1:0]   fill_arena_i,
    input var logic [INDEX_W-1:0]   fill_index_i,
    input var logic signed [31:0]   fill_vx_i,      // fx16 world x (Q16.16 m)
    input var logic signed [31:0]   fill_vz_i,      // fx16 world z (Q16.16 m)
    input var logic                 fill_surface_i, // 0 = top, 1 = underside

    // The island's cell pitch, spec 1.3, HELD by the composer on TERRAIN.
    // HDRREAD's valid handshake -- never the header wire, which reads
    // HDR_PITCH_REFUSE (127) on nearly every cycle.
    input var logic signed [ 7:0]   pitch_log2_i,

    // ---- the arena's lifetime ------------------------------------------------
    input var logic                 open_i,
    /* verilator lint_off UNUSEDSIGNAL */
    // ARENA_W is clog2(ARENAS)+1 -- the shell's spare bit rides along and is
    // not an address here. Same waiver, same reason, as zhao_terrain_lightlane.
    input var logic [ARENA_W-1:0]   open_arena_i,
    /* verilator lint_on UNUSEDSIGNAL */
    input var logic [GEN_W-1:0]     open_gen_i,

    // ---- the reference stream ------------------------------------------------
    input  var logic                ref_valid_i,
    output var logic                ref_ready_o,
    input  var logic [ARENA_W-1:0]  ref_arena_i,
    input  var logic [GEN_W-1:0]    ref_gen_i,
    input  var logic [INDEX_W-1:0]  ref_ia_i,
    input  var logic [INDEX_W-1:0]  ref_ib_i,
    input  var logic [INDEX_W-1:0]  ref_ic_i,
    input  var logic [SRCW-1:0]     ref_src_id_i,

    // ---- the coordinates, per corner, Q16.16 TILE units ----------------------
    output var logic                uv_valid_o,
    input  var logic                uv_ready_i,
    output var logic signed [31:0]  uv_au_o,
    output var logic signed [31:0]  uv_av_o,
    output var logic signed [31:0]  uv_bu_o,
    output var logic signed [31:0]  uv_bv_o,
    output var logic signed [31:0]  uv_cu_o,
    output var logic signed [31:0]  uv_cv_o,
    output var logic [SRCW-1:0]     uv_src_id_o,

    // ---- census --------------------------------------------------------------
    output var logic [31:0] refs_taken_o,
    output var logic [31:0] uvs_emitted_o,
    output var logic [31:0] stale_reads_o,    // a corner from an older generation
    output var logic [31:0] pitch_clamped_o,  // the oracle's `top_shift < 0` clamp bit
    output var logic [31:0] pitch_illegal_o,  // a pitch spec 1.3 cannot carry
    output var logic        idle_o
);

  localparam int unsigned ROWS = ARENAS * DEPTH;
  localparam int unsigned RW   = (ROWS > 1) ? $clog2(ROWS) : 1;

  // terrain_rules 6.6: STRATA_M default 8 m, so the underside's planar world UV
  // is a shift of three. A NAMED constant, not a literal in an expression --
  // CLAUDE.md's rule that every shape value stays an editable knob.
  localparam logic [4:0] UNDER_SHIFT = 5'd3;

  // spec 1.3's four legal pitches.
  localparam logic signed [7:0] PITCH_MIN = -8'sd1;
  localparam logic signed [7:0] PITCH_MAX =  8'sd2;

  // Quartus 17 requires an elaboration check inside `initial begin`, and
  // `--lint-only` does not run initial blocks -- a clean lint says nothing
  // whatever about this guard.
  initial begin
    if (ARENAS == 0 || DEPTH == 0) begin
      $fatal(1, "zhao_terrain_uvlane: ARENAS (%0d) and DEPTH (%0d) must be non-zero",
             ARENAS, DEPTH);
    end
  end

  // ==========================================================================
  // THE LAW, evaluated on the fill beat
  // ==========================================================================
  // reference/src/zrender/terrain.cpp:589-591. The clamp is the `if
  // (top_shift < 0) top_shift = 0` line; `pitch_log2 = -1` is the only legal
  // pitch that reaches it.
  wire pitch_legal_c   = (pitch_log2_i >= PITCH_MIN) && (pitch_log2_i <= PITCH_MAX);
  wire pitch_negative_c = (pitch_log2_i < 8'sd0);

  logic [4:0] top_shift_c;
  always_comb begin
    if (pitch_negative_c) top_shift_c = 5'd0;              // THE CLAMP
    else                  top_shift_c = 5'(pitch_log2_i[4:0]);
  end

  wire [4:0] shift_c = fill_surface_i ? UNDER_SHIFT : top_shift_c;

  // Arithmetic shifts: a world coordinate is signed and the oracle's operands
  // are `int32_t`, so a logical shift would diverge west and north of the
  // island datum.
  wire signed [31:0] fill_u_c = fill_vx_i >>> shift_c;
  wire signed [31:0] fill_v_c = fill_vz_i >>> shift_c;

  // ==========================================================================
  // THE STORE
  // ==========================================================================
  // {generation, v, u}: one row per {arena, index}, written on the fill beat
  // and read three times per reference.
  localparam int unsigned ROW_W = 64 + GEN_W;
  logic [ROW_W-1:0] rd_q;
  logic [RW-1:0]    rd_addr_c;
  logic [ROW_W-1:0] mem [0:ROWS-1];

  logic [GEN_W-1:0] gen_q [0:ARENAS-1];
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      for (int i = 0; i < int'(ARENAS); i++) gen_q[i] <= '0;
    end else if (open_i) begin
      // The arena index's top bit is the shell's spare (ARENA_W is clog2+1);
      // the store is indexed by the arenas that exist.
      gen_q[open_arena_i[$clog2(ARENAS)-1:0]] <= open_gen_i;
    end
  end

  function automatic logic [RW-1:0] row_of(input logic [ARENA_W-1:0] a,
                                           input logic [INDEX_W-1:0] i);
    row_of = RW'((32'(a) * 32'(DEPTH)) + 32'(i));
  endfunction

  wire fill_fire_c = fill_valid_i && fill_ready_i;

  always_ff @(posedge clk) begin
    if (fill_fire_c) begin
      mem[row_of(fill_arena_i, fill_index_i)] <=
          {gen_q[fill_arena_i[$clog2(ARENAS)-1:0]], fill_v_c, fill_u_c};
    end
    rd_q <= mem[rd_addr_c];
  end

  // ==========================================================================
  // THE REFERENCE WALK -- zhao_terrain_lightlane's, corner for corner
  // ==========================================================================
  localparam logic [2:0] S_IDLE  = 3'd0;
  localparam logic [2:0] S_RD_B  = 3'd1;   // A's row is being read; ask for B
  localparam logic [2:0] S_CAP_B = 3'd3;
  localparam logic [2:0] S_CAP_C = 3'd4;
  localparam logic [2:0] S_OFFER = 3'd5;

  logic [2:0]          st_q;
  logic [ARENA_W-1:0]  r_arena_q;
  logic [GEN_W-1:0]    r_gen_q;
  logic [INDEX_W-1:0]  r_ib_q, r_ic_q;
  logic [SRCW-1:0]     r_src_q;
  logic signed [31:0]  au_q, av_q, bu_q, bv_q, cu_q, cv_q;
  logic                stale_q;

  always_comb begin
    rd_addr_c = '0;
    unique case (st_q)
      S_IDLE:  rd_addr_c = row_of(ref_arena_i, ref_ia_i);
      S_RD_B:  rd_addr_c = row_of(r_arena_q, r_ib_q);
      S_CAP_B: rd_addr_c = row_of(r_arena_q, r_ic_q);
      default: rd_addr_c = row_of(r_arena_q, r_ic_q);
    endcase
  end

  assign ref_ready_o = (st_q == S_IDLE);
  assign uv_valid_o  = (st_q == S_OFFER) && !stale_q;
  assign idle_o      = (st_q == S_IDLE);

  assign uv_au_o     = au_q;
  assign uv_av_o     = av_q;
  assign uv_bu_o     = bu_q;
  assign uv_bv_o     = bv_q;
  assign uv_cu_o     = cu_q;
  assign uv_cv_o     = cv_q;
  assign uv_src_id_o = r_src_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_q      <= S_IDLE;
      r_arena_q <= '0;
      r_gen_q   <= '0;
      r_ib_q    <= '0;
      r_ic_q    <= '0;
      r_src_q   <= '0;
      au_q      <= '0; av_q <= '0;
      bu_q      <= '0; bv_q <= '0;
      cu_q      <= '0; cv_q <= '0;
      stale_q   <= 1'b0;
      refs_taken_o    <= 32'd0;
      uvs_emitted_o   <= 32'd0;
      stale_reads_o   <= 32'd0;
      pitch_clamped_o <= 32'd0;
      pitch_illegal_o <= 32'd0;
    end else begin
      // The two pitch observations are made on the FILL beat, because that is
      // the beat whose answer the pitch changed. A top-surface fill is the
      // only one the pitch reaches: the underside's shift is STRATA_M's and is
      // pitch-independent by terrain_rules 6.6.
      if (fill_fire_c && !fill_surface_i) begin
        if (pitch_negative_c && (pitch_clamped_o != 32'hFFFF_FFFF))
          pitch_clamped_o <= pitch_clamped_o + 32'd1;
        if (!pitch_legal_c && (pitch_illegal_o != 32'hFFFF_FFFF))
          pitch_illegal_o <= pitch_illegal_o + 32'd1;
      end

      unique case (st_q)
        S_IDLE: begin
          if (ref_valid_i) begin
            r_arena_q <= ref_arena_i;
            r_gen_q   <= ref_gen_i;
            r_ib_q    <= ref_ib_i;
            r_ic_q    <= ref_ic_i;
            r_src_q   <= ref_src_id_i;
            stale_q   <= 1'b0;
            if (refs_taken_o != 32'hFFFF_FFFF) refs_taken_o <= refs_taken_o + 32'd1;
            st_q      <= S_RD_B;
          end
        end

        // A's row lands this clock (the read was issued in S_IDLE).
        S_RD_B: begin
          au_q <= signed'(rd_q[31:0]);
          av_q <= signed'(rd_q[63:32]);
          if (rd_q[64 +: GEN_W] != r_gen_q) stale_q <= 1'b1;
          st_q <= S_CAP_B;
        end

        S_CAP_B: begin
          bu_q <= signed'(rd_q[31:0]);
          bv_q <= signed'(rd_q[63:32]);
          if (rd_q[64 +: GEN_W] != r_gen_q) stale_q <= 1'b1;
          st_q <= S_CAP_C;
        end

        S_CAP_C: begin
          cu_q <= signed'(rd_q[31:0]);
          cv_q <= signed'(rd_q[63:32]);
          if (rd_q[64 +: GEN_W] != r_gen_q) stale_q <= 1'b1;
          st_q <= S_OFFER;
        end

        S_OFFER: begin
          // A stale corner is refused here exactly as the arena refuses it
          // downstream: nothing is offered, and the refusal is counted.
          if (stale_q) begin
            if (stale_reads_o != 32'hFFFF_FFFF) stale_reads_o <= stale_reads_o + 32'd1;
            st_q <= S_IDLE;
          end else if (uv_ready_i) begin
            if (uvs_emitted_o != 32'hFFFF_FFFF) uvs_emitted_o <= uvs_emitted_o + 32'd1;
            st_q <= S_IDLE;
          end
        end

        default: st_q <= S_IDLE;
      endcase
    end
  end

endmodule

`default_nettype wire
