// zhao_terrain_lod.sv — TERRAIN.LOD: the projected-error ladder, the stability
// band and the geomorph walk, for one patch's sixteen subpatches
// (phase 6, ZH-050).
//
// Law, in citation order:
//   design/contracts/TERRAIN.LOD.md — the block contract.
//   design/blocks.yml — `inputs: [patch_state, lod_targets]`, `outputs:
//       [lod_decisions]`, `upstream: [TERRAIN.PATCH, MEASURE.GOVERNOR]`,
//       `downstream: [TERRAIN.PROJECT]`, `backpressure: ready_valid`,
//       `latency: variable`, "1 decision per patch per frame", counters
//       `lod_representation_counts` and `terrain_triangles_emitted`, and the
//       note "Geomorph transitions keep cracks invisible (§11)".
//   ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md §11.5 — what a terrain projected
//       error COMBINES: "stored coarse-level height deviation; live
//       deformation curvature; camera distance; terrain velocity; semantic
//       importance near units and spell impacts; both camera requirements".
//       An ingredient list. NOT an arithmetic.
//   ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md §9 "Stability" — "Every LOD path
//       requires: hysteresis; minimum hold duration; parent/child geomorph
//       where possible". MANDATORY. The constants are explicitly provisional
//       (design/contracts/MEASURE.GOVERNOR.md).
//   spec/terrain_rules.md §2 — the derived coarse height mips ("17×17 + 9×9 …
//       for TERRAIN.LOD"), which is why a per-level height deviation is a
//       STORED quantity here and not an invention.
//   spec/qformats.md §7.2 — `isqrt_u32`/`isqrt_u64`, the ratified exact floor
//       square root, already used for a distance in terrain_rules §3.7.
//   design/contracts/TERRAIN.TESS.md — the `lod_target` packet this block
//       produces, field for field.
//
// ---------------------------------------------------------------------------
// THE HONEST POSITION: THIS BLOCK'S LAW IS CHOSEN, NOT FOUND
// ---------------------------------------------------------------------------
// There is no ratified terrain LOD arithmetic anywhere in this tree. §11.5
// lists ingredients, §9 lists required stability properties, MEASURE.GOVERNOR's
// contract is a stub. So every numbered item below is a DECISION, recorded as
// one, with what it rejected. `reference/include/zref/zref_terrain_lod.hpp`
// carries the same list and is the executable form of it.
//
// 1. THE LADDER IS `dev[L] · scale ≤ distance`, COARSEST WINS, AND IT IS EXACT.
//    In raw integers, with dev and distance fx16 and scale Q8.8:
//        dev[L] · scale  ≤  distance · h
//    `h` is 256 for the strict ladder and `hyst_i` for the relaxed one, so ONE
//    comparator serves both. There is NO ROUNDING in this test at all —
//    deliberately, because the flip point is the only thing a LOD law is really
//    made of, and a rounded intermediate flips at a different distance than the
//    inequality it claims to implement.
//    `dev[0]` is zero BY DEFINITION (level 0 is the full lattice), which is
//    what guarantees the ladder always terminates. The dev values are NOT
//    assumed monotonic in L, so the ladder asks "which is the coarsest level
//    that passes" rather than "which is the first that fails".
//    REJECTED: a squared-domain comparison that avoids the square root. It is
//    exact too, but it needs (dev·scale)² — a 40×40 multiply — where the
//    §7.2 isqrt needs 32 iterations of a 64-bit compare-subtract at a rate the
//    ledger sets at ONE DECISION PER PATCH PER FRAME. And "camera distance" is
//    §11.5's own word; a block whose contract can name its intermediate is
//    worth more than one that saved a multiplier.
//
// 2. DISTANCE IS EUCLIDEAN EYE-TO-SUBPATCH-CENTRE, VIA THE §7.2 ISQRT.
//    REJECTED: view-space depth (the perspective-correct projected error). It
//    is more accurate and it is ROTATION-DEPENDENT: a player turning on the
//    spot would re-tessellate the ground under their feet. Euclidean distance
//    is rotation-invariant, which is what makes the decision stable and what
//    lets ONE deformed-height cache serve both views (§11.5: "The deformed
//    height cache is produced once and projected into both views").
//
// 3. THE TWO CAMERAS COMBINE BY TAKING THE FINER DECISION. Charter §9's Duo
//    fairness rule is that "one player looking directly into a volcano cannot
//    make the other player's army disappear"; the same reasoning gives the
//    ground to whichever camera needs it finest. A disabled camera contributes
//    nothing, and with NO camera enabled nothing changes at all — stated,
//    rather than left to decay into "coarsest".
//
// 4. HYSTERESIS IS A BAND BETWEEN A STRICT AND A RELAXED LADDER.
//    `T_strict ≤ level ≤ T_relaxed` ⇒ hold. Below the band ⇒ coarsen to
//    `T_strict`; above it ⇒ refine to `T_relaxed`. The target is always the
//    NEAR EDGE of the band, never an overshoot, so a camera moving smoothly
//    walks the ladder one rung at a time.
//    REJECTED: a distance-based dead-band (remember the distance at which the
//    level last changed and require the camera to move by some margin). It
//    needs another 32 bits of per-subpatch history and it does not compose with
//    a governor that changes `scale` between frames.
//
// 5. THE HISTORY RIDES THE PACKET. `sp_prev_level_i`, `sp_prev_morph_i` and
//    `sp_hold_i` come in; the new level, morph and hold go out, for the caller
//    to store. REJECTED: an internal history RAM. 1,024 patches × 16 subpatches
//    × (2 + 17 + 8) bits is 6.6 KB of M10K that only this block can address,
//    and TERRAIN.PATCH already owns the per-patch state this belongs beside.
//    A block whose contract can say "Memory ownership: none" is worth more than
//    one that saved a packet field.
//
// 6. CURVATURE, VELOCITY AND SEMANTIC WEIGHT ARE NOT IN v1. §11.5 lists them.
//    None of them has a ratified magnitude, a Q format or a source block that
//    exists (TERRAIN.VELOCITY is phase 7). Folding an invented weight into the
//    ladder would ratify it by omission. They enter through `dev` — whoever
//    writes the mips may bias them — or as a later amendment. Recorded as
//    missing rather than faked.
//
// 7. THE UNDERSIDE TAKES THE TOP'S LEVEL. terrain_rules §5: "the underside LOD
//    may be coarser than top EXCEPT along rim boundaries", where it must be
//    EQUAL. Choosing "coarser" needs FORGE.CLIFF's rim edge set, which this
//    block does not have and which does not exist yet. Equal-everywhere
//    satisfies the crack constraint on every edge, at the cost of some
//    underside triangles. A later amendment can coarsen it once the rim set is
//    real.
//
// ---------------------------------------------------------------------------
// SHAPE
// ---------------------------------------------------------------------------
// One PATCH is one job: sixteen subpatch descriptors stream in, in a fixed
// order (x fastest: descriptor n is the subpatch at ox = (n & 3)·8,
// oz = (n >> 2)·8), and then sixteen — or thirty-two, on a dual page —
// `lod_target` packets stream out. **The output packet IS
// `zhao_terrain_tess`'s job port**, field for field, because a decision that
// cannot be handed to the tessellator is not a decision.
//
// The two phases exist for ONE reason: a subpatch's `lod_target` carries its
// four NEIGHBOURS' levels, and a streaming block cannot know them. Sixteen
// 2-bit levels is 32 flip-flops, which is the whole cost of knowing.
//
// Conservative SystemVerilog subset only (charter §2); no package deps.

module zhao_terrain_lod (
    input logic clk,
    input logic rst_n,

    // -----------------------------------------------------------------------
    // governor targets (MEASURE.GOVERNOR), held stable across a patch job
    // -----------------------------------------------------------------------
    input logic signed [31:0] cam0_x_i,
    input logic signed [31:0] cam0_y_i,
    input logic signed [31:0] cam0_z_i,
    input logic        [15:0] cam0_scale_i,  // Q8.8 error-per-distance budget
    input logic               cam0_en_i,
    input logic signed [31:0] cam1_x_i,
    input logic signed [31:0] cam1_y_i,
    input logic signed [31:0] cam1_z_i,
    input logic        [15:0] cam1_scale_i,
    input logic               cam1_en_i,
    input logic        [15:0] hyst_i,        // Q8.8, values below 256 read as 256
    input logic        [ 7:0] min_hold_i,    // frames a level must hold
    input logic        [16:0] morph_step_i,  // Q16 per frame; 0 = snap
    input logic               dual_i,        // the page models an underside

    // the adjacent patches' border levels, four 2-bit lanes each, indexed by
    // the subpatch coordinate ALONG that edge (terrain_rules §6.6 side order)
    input logic [7:0] edge_nz_i,
    input logic [7:0] edge_pz_i,
    input logic [7:0] edge_nx_i,
    input logic [7:0] edge_px_i,

    // -----------------------------------------------------------------------
    // patch_state: sixteen subpatch descriptors, in order
    // -----------------------------------------------------------------------
    input  logic               sp_valid_i,
    output logic               sp_ready_o,
    input  logic signed [31:0] sp_cx_i,        // subpatch centre, fx16 world
    input  logic signed [31:0] sp_cy_i,
    input  logic signed [31:0] sp_cz_i,
    input  logic        [23:0] sp_dev1_i,      // |fine − coarse| at level 1, fx16
    input  logic        [23:0] sp_dev2_i,
    input  logic        [23:0] sp_dev3_i,
    input  logic        [ 1:0] sp_prev_level_i,
    input  logic        [16:0] sp_prev_morph_i,
    input  logic        [ 7:0] sp_hold_i,
    input  logic        [15:0] sp_src_id_i,

    // -----------------------------------------------------------------------
    // lod_decisions out — EXACTLY zhao_terrain_tess's job port, plus the
    // history the caller writes back
    // -----------------------------------------------------------------------
    output logic        out_valid_o,
    input  logic        out_ready_i,
    output logic [ 5:0] out_ox_o,
    output logic [ 5:0] out_oz_o,
    output logic [ 1:0] out_level_o,
    output logic [ 1:0] out_lvl_nz_o,
    output logic [ 1:0] out_lvl_pz_o,
    output logic [ 1:0] out_lvl_nx_o,
    output logic [ 1:0] out_lvl_px_o,
    output logic [16:0] out_morph_o,
    output logic        out_surface_o,
    output logic        out_dual_o,
    output logic [15:0] out_src_id_o,
    output logic [ 7:0] out_hold_o,

    // -----------------------------------------------------------------------
    // counters (spec/counters.md §4: saturate, never wrap)
    // -----------------------------------------------------------------------
    output logic [31:0] lod_rep_count0_o,  // the four lanes of
    output logic [31:0] lod_rep_count1_o,  //   `lod_representation_counts`
    output logic [31:0] lod_rep_count2_o,
    output logic [31:0] lod_rep_count3_o,
    output logic [31:0] terrain_triangles_emitted_o,
    output logic        idle_o
);

  localparam int unsigned NSub = 16;
  localparam int unsigned SqrtSteps = 32;
  localparam logic [16:0] MorphOne = 17'd65536;

  localparam logic [1:0] StFill = 2'd0;
  localparam logic [1:0] StSqrt = 2'd1;
  localparam logic [1:0] StDecide = 2'd2;
  localparam logic [1:0] StEmit = 2'd3;

  logic [1:0] state;
  logic [4:0] fill_idx;  // 0..15 while filling
  logic [4:0] emit_idx;
  logic       emit_surf;

  // ---- the per-patch decision store ----------------------------------------
  logic [ 1:0] lvl [0:NSub-1];
  logic [16:0] mrp [0:NSub-1];
  logic [ 7:0] hld [0:NSub-1];
  logic [15:0] sid [0:NSub-1];

  // ---- the latched descriptor ----------------------------------------------
  logic [23:0] d_dev1, d_dev2, d_dev3;
  logic [ 1:0] d_level;
  logic [16:0] d_morph;
  logic [ 7:0] d_hold;
  logic [15:0] d_src;

  // ===========================================================================
  // the squared distance — exact, then saturated to the 64-bit word the §7.2
  // isqrt takes. |dx| ≤ 2^32 so a square is ≤ 2^64 and the three-term sum needs
  // 66 bits; the reference forms the same sum in s128 and saturates the same
  // way, so the two agree for EVERY input word rather than inside an envelope.
  // ===========================================================================
  function automatic logic signed [32:0] diff33(input logic signed [31:0] a,
                                                input logic signed [31:0] b);
    diff33 = $signed({a[31], a}) - $signed({b[31], b});
  endfunction

  function automatic logic [65:0] sq66(input logic signed [32:0] d);
    logic signed [65:0] w;
    begin
      w    = $signed({{33{d[32]}}, d}) * $signed({{33{d[32]}}, d});
      sq66 = w[65:0];
    end
  endfunction

  function automatic logic [63:0] dsq_sat(input logic [65:0] s);
    dsq_sat = (s[65:64] != 2'b00) ? 64'hFFFF_FFFF_FFFF_FFFF : s[63:0];
  endfunction

  logic [63:0] dsq0, dsq1;
  always_comb begin
    dsq0 = dsq_sat(sq66(diff33(sp_cx_i, cam0_x_i)) + sq66(diff33(sp_cy_i, cam0_y_i)) +
                   sq66(diff33(sp_cz_i, cam0_z_i)));
    dsq1 = dsq_sat(sq66(diff33(sp_cx_i, cam1_x_i)) + sq66(diff33(sp_cy_i, cam1_y_i)) +
                   sq66(diff33(sp_cz_i, cam1_z_i)));
  end

  // ===========================================================================
  // §7.2 isqrt, two lanes, thirty-two fixed steps
  // ===========================================================================
  // `zref::isqrt_u64`'s recurrence, with the leading `while (bit > num)`
  // normalisation dropped: from bit = 2^62 downward the loop body simply takes
  // its else branch until bit fits, which is the same answer in a fixed number
  // of cycles. `res` never exceeds 2^33 (it is 2·root·2^k at step k), so
  // `res + bit` fits inside 64 bits with room over.
  logic [63:0] sq_num0, sq_res0, sq_num1, sq_res1;
  logic [63:0] sq_bit;
  logic [ 5:0] sq_cnt;

  function automatic logic [63:0] sqrt_next_num(input logic [63:0] num, input logic [63:0] res,
                                                input logic [63:0] bitv);
    sqrt_next_num = (num >= (res + bitv)) ? (num - (res + bitv)) : num;
  endfunction

  function automatic logic [63:0] sqrt_next_res(input logic [63:0] num, input logic [63:0] res,
                                                input logic [63:0] bitv);
    sqrt_next_res = (num >= (res + bitv)) ? ((res >> 1) + bitv) : (res >> 1);
  endfunction

  // ===========================================================================
  // the ladder
  // ===========================================================================
  // lhs = dev · scale ≤ 2^24 · 2^16 = 2^40; rhs = dstv · h ≤ 2^32 · 2^16 = 2^48.
  // A 49-bit compare covers both with the sign-free headroom stated.
  function automatic logic ladder_ok(input logic [23:0] dev, input logic [15:0] scale,
                                     input logic [31:0] dstv, input logic [15:0] h);
    logic [48:0] lhs;
    logic [48:0] rhs;
    begin
      lhs       = {9'b0, dev} * {24'b0, scale};
      rhs       = {17'b0, dstv} * {33'b0, h};
      ladder_ok = (lhs <= rhs);
    end
  endfunction

  function automatic logic [1:0] ladder(input logic [23:0] dev1, input logic [23:0] dev2,
                                        input logic [23:0] dev3, input logic [15:0] scale,
                                        input logic [31:0] dstv, input logic [15:0] h);
    begin
      // dev[0] is zero by definition, so level 0 always passes and the ladder
      // always terminates. Coarsest wins; no monotonicity is assumed.
      ladder = 2'd0;
      if (ladder_ok(dev1, scale, dstv, h)) ladder = 2'd1;
      if (ladder_ok(dev2, scale, dstv, h)) ladder = 2'd2;
      if (ladder_ok(dev3, scale, dstv, h)) ladder = 2'd3;
    end
  endfunction

  wire [15:0] hyst_eff = (hyst_i < 16'd256) ? 16'd256 : hyst_i;

  logic [1:0] t_strict, t_relaxed;
  logic [1:0] s0, r0, s1, r1;
  always_comb begin
    s0 = ladder(d_dev1, d_dev2, d_dev3, cam0_scale_i, sq_res0[31:0], 16'd256);
    r0 = ladder(d_dev1, d_dev2, d_dev3, cam0_scale_i, sq_res0[31:0], hyst_eff);
    s1 = ladder(d_dev1, d_dev2, d_dev3, cam1_scale_i, sq_res1[31:0], 16'd256);
    r1 = ladder(d_dev1, d_dev2, d_dev3, cam1_scale_i, sq_res1[31:0], hyst_eff);

    if (cam0_en_i && cam1_en_i) begin
      t_strict  = (s0 < s1) ? s0 : s1;
      t_relaxed = (r0 < r1) ? r0 : r1;
    end else if (cam0_en_i) begin
      t_strict  = s0;
      t_relaxed = r0;
    end else if (cam1_en_i) begin
      t_strict  = s1;
      t_relaxed = r1;
    end else begin
      // No camera: nothing is visible, so nothing changes.
      t_strict  = d_level;
      t_relaxed = d_level;
    end
  end

  // ===========================================================================
  // the band, and the geomorph walk
  // ===========================================================================
  logic [ 1:0] want;
  logic [ 1:0] n_level;
  logic [16:0] n_morph;
  logic [ 7:0] n_hold;
  logic        n_changed;

  wire        hold_ok = (d_hold >= min_hold_i);
  wire [16:0] step = morph_step_i;
  wire [16:0] morph_in = (d_morph > MorphOne) ? MorphOne : d_morph;
  wire [17:0] morph_up = {1'b0, morph_in} + {1'b0, step};

  always_comb begin
    if (d_level < t_strict) want = t_strict;
    else if (d_level > t_relaxed) want = t_relaxed;
    else want = d_level;

    n_level   = d_level;
    n_morph   = morph_in;
    n_changed = 1'b0;

    if ((want > d_level) && hold_ok) begin
      // Coarsening: TERRAIN.TESS's morph blends the CURRENT level toward the
      // next coarser one, so the level is held and the factor walks to unity.
      if ((step == 17'd0) || (morph_up >= {1'b0, MorphOne})) begin
        n_level   = d_level + 2'd1;
        n_morph   = 17'd0;
        n_changed = 1'b1;
      end else begin
        n_morph = morph_up[16:0];
      end
    end else if ((want < d_level) && hold_ok) begin
      // Refining: the finer level is adopted AT ONCE with the factor at unity,
      // so the geometry on screen does not move at the moment of the swap, and
      // the factor then walks back down to zero.
      if (morph_in == 17'd0) begin
        n_level   = d_level - 2'd1;
        n_morph   = (step == 17'd0) ? 17'd0 : MorphOne;
        n_changed = 1'b1;
      end else begin
        n_morph = ((step == 17'd0) || (morph_in <= step)) ? 17'd0 : (morph_in - step);
      end
    end else begin
      n_morph = ((step == 17'd0) || (morph_in <= step)) ? 17'd0 : (morph_in - step);
    end

    if (n_changed) n_hold = 8'd0;
    else n_hold = (d_hold == 8'hFF) ? 8'hFF : (d_hold + 8'd1);
  end

  // triangles a subpatch emits at level L, ignoring void cells and stitching:
  // 2·(8 >> L)² = 128, 32, 8, 2. An UPPER BOUND, and named as one.
  function automatic logic [31:0] tris_of(input logic [1:0] l);
    case (l)
      2'd0: tris_of = 32'd128;
      2'd1: tris_of = 32'd32;
      2'd2: tris_of = 32'd8;
      default: tris_of = 32'd2;
    endcase
  endfunction

  // ===========================================================================
  // the neighbour lookup, on the emit side
  // ===========================================================================
  wire [1:0] e_i = emit_idx[1:0];
  wire [1:0] e_j = emit_idx[3:2];

  function automatic logic [1:0] edge_lane(input logic [7:0] e, input logic [1:0] k);
    edge_lane = e[{1'b0, k} * 2+:2];
  endfunction

  logic [1:0] nb_nz, nb_pz, nb_nx, nb_px;
  always_comb begin
    nb_nz = (e_j != 2'd0) ? lvl[{e_j - 2'd1, e_i}] : edge_lane(edge_nz_i, e_i);
    nb_pz = (e_j != 2'd3) ? lvl[{e_j + 2'd1, e_i}] : edge_lane(edge_pz_i, e_i);
    nb_nx = (e_i != 2'd0) ? lvl[{e_j, e_i - 2'd1}] : edge_lane(edge_nx_i, e_j);
    nb_px = (e_i != 2'd3) ? lvl[{e_j, e_i + 2'd1}] : edge_lane(edge_px_i, e_j);
  end

  // ===========================================================================
  // control
  // ===========================================================================
  assign sp_ready_o = (state == StFill);

  wire out_free = !out_valid_o || out_ready_i;
  wire emit_last = (emit_idx == 5'd15) && (!dual_i || emit_surf);

  integer i;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state <= StFill;
      fill_idx <= '0;
      emit_idx <= '0;
      emit_surf <= 1'b0;
      for (i = 0; i < NSub; i = i + 1) begin
        lvl[i] <= '0;
        mrp[i] <= '0;
        hld[i] <= '0;
        sid[i] <= '0;
      end
      d_dev1 <= '0;
      d_dev2 <= '0;
      d_dev3 <= '0;
      d_level <= '0;
      d_morph <= '0;
      d_hold <= '0;
      d_src <= '0;
      sq_num0 <= '0;
      sq_res0 <= '0;
      sq_num1 <= '0;
      sq_res1 <= '0;
      sq_bit <= '0;
      sq_cnt <= '0;
      out_valid_o <= 1'b0;
      out_ox_o <= '0;
      out_oz_o <= '0;
      out_level_o <= '0;
      out_lvl_nz_o <= '0;
      out_lvl_pz_o <= '0;
      out_lvl_nx_o <= '0;
      out_lvl_px_o <= '0;
      out_morph_o <= '0;
      out_surface_o <= 1'b0;
      out_dual_o <= 1'b0;
      out_src_id_o <= '0;
      out_hold_o <= '0;
      lod_rep_count0_o <= '0;
      lod_rep_count1_o <= '0;
      lod_rep_count2_o <= '0;
      lod_rep_count3_o <= '0;
      terrain_triangles_emitted_o <= '0;
    end else begin
      if (out_valid_o && out_ready_i) out_valid_o <= 1'b0;

      case (state)
        StFill: begin
          if (sp_valid_i) begin
            d_dev1 <= sp_dev1_i;
            d_dev2 <= sp_dev2_i;
            d_dev3 <= sp_dev3_i;
            d_level <= sp_prev_level_i;
            d_morph <= sp_prev_morph_i;
            d_hold <= sp_hold_i;
            d_src <= sp_src_id_i;
            sq_num0 <= dsq0;
            sq_num1 <= dsq1;
            sq_res0 <= '0;
            sq_res1 <= '0;
            sq_bit <= 64'h4000_0000_0000_0000;  // 2^62
            sq_cnt <= '0;
            state <= StSqrt;
          end
        end

        StSqrt: begin
          sq_num0 <= sqrt_next_num(sq_num0, sq_res0, sq_bit);
          sq_res0 <= sqrt_next_res(sq_num0, sq_res0, sq_bit);
          sq_num1 <= sqrt_next_num(sq_num1, sq_res1, sq_bit);
          sq_res1 <= sqrt_next_res(sq_num1, sq_res1, sq_bit);
          sq_bit  <= sq_bit >> 2;
          sq_cnt  <= sq_cnt + 6'd1;
          if (sq_cnt == SqrtSteps[5:0] - 6'd1) state <= StDecide;
        end

        StDecide: begin
          lvl[fill_idx[3:0]] <= n_level;
          mrp[fill_idx[3:0]] <= n_morph;
          hld[fill_idx[3:0]] <= n_hold;
          sid[fill_idx[3:0]] <= d_src;
          if (fill_idx == 5'd15) begin
            fill_idx  <= '0;
            emit_idx  <= '0;
            emit_surf <= 1'b0;
            state     <= StEmit;
          end else begin
            fill_idx <= fill_idx + 5'd1;
            state    <= StFill;
          end
        end

        default: begin  // StEmit
          if (out_free) begin
            out_valid_o   <= 1'b1;
            out_ox_o      <= {1'b0, e_i, 3'b000};
            out_oz_o      <= {1'b0, e_j, 3'b000};
            out_level_o   <= lvl[emit_idx[3:0]];
            out_lvl_nz_o  <= nb_nz;
            out_lvl_pz_o  <= nb_pz;
            out_lvl_nx_o  <= nb_nx;
            out_lvl_px_o  <= nb_px;
            out_morph_o   <= mrp[emit_idx[3:0]];
            out_surface_o <= emit_surf;
            out_dual_o    <= dual_i;
            out_src_id_o  <= sid[emit_idx[3:0]];
            out_hold_o    <= hld[emit_idx[3:0]];

            // The counters advance on the TOP decision only, so a dual page
            // does not double-count a subpatch's representation.
            if (!emit_surf) begin
              case (lvl[emit_idx[3:0]])
                2'd0: if (lod_rep_count0_o != 32'hFFFF_FFFF) lod_rep_count0_o <= lod_rep_count0_o + 32'd1;
                2'd1: if (lod_rep_count1_o != 32'hFFFF_FFFF) lod_rep_count1_o <= lod_rep_count1_o + 32'd1;
                2'd2: if (lod_rep_count2_o != 32'hFFFF_FFFF) lod_rep_count2_o <= lod_rep_count2_o + 32'd1;
                default: if (lod_rep_count3_o != 32'hFFFF_FFFF) lod_rep_count3_o <= lod_rep_count3_o + 32'd1;
              endcase
            end
            terrain_triangles_emitted_o <= terrain_triangles_emitted_o + tris_of(lvl[emit_idx[3:0]]);

            if (emit_last) begin
              emit_idx  <= '0;
              emit_surf <= 1'b0;
              state     <= StFill;
            end else if (dual_i && !emit_surf) begin
              emit_surf <= 1'b1;
            end else begin
              emit_idx  <= emit_idx + 5'd1;
              emit_surf <= 1'b0;
            end
          end
        end
      endcase
    end
  end

  assign idle_o = (state == StFill) && (fill_idx == 5'd0) && !out_valid_o;

endmodule : zhao_terrain_lod
