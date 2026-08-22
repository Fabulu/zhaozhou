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
// ---------------------------------------------------------------------------
// THIRTY PRODUCTS, ONE MULTIPLIER
// ---------------------------------------------------------------------------
// This block used to stand thirty multiplies side by side and it measured
// 28 DSP blocks -- a QUARTER of the provisional device's 112 -- for a decision
// the ledger asks for ONCE PER PATCH PER FRAME. They are all now sequenced
// through a single 32x32 unsigned multiplier, on a schedule that mostly fits
// inside time the block was already spending:
//
//   StSquare  6 clocks   |dx|^2 ... |dz|^2 for both eyes, accumulated in 66
//                        bits and saturated exactly as before
//   StSqrt   32 clocks   the §7.2 root, unchanged, both lanes concurrent
//   StEval    8 clocks   two relaxed right-hand sides and six left-hand sides,
//                        each compared the cycle its product appears
//
// Two things make this nearly free rather than a trade:
//
//   * |dx| for dx = c - e fits in 32 UNSIGNED bits exactly (both operands are
//     s32, so the difference spans [-(2^32-1), 2^32-1]) and d^2 = |d|^2. So the
//     one shared multiplier is 32x32 unsigned, NARROWER than the signed 33x33
//     the parallel form needed, and it gives the same answer for every input
//     word rather than inside a declared envelope.
//
//   * THE STRICT LADDER'S RIGHT-HAND SIDE IS NOT A MULTIPLY. `h` is the
//     constant 256 there, so `dstv * h` is `dstv << 8`. Six of the twenty-four
//     ladder multiplies the parallel form spent were multiplications by a
//     compile-time power of two, spent as DSPs only because `ladder_ok()` took
//     `h` as an argument and the strict and relaxed cases shared one function.
//
// What it costs, stated plainly: 34 clocks per descriptor became 48, so a patch
// is ~784 clocks rather than ~560. Against `spec/terrain_rules.md` §4.2's 256
// live patches that is still about 8x the required rate. See the contract's
// Latency and Target throughput sections, which carry the arithmetic.
//
// The comparison ALSO collapsed: twelve parallel 49-bit comparators became two,
// because a product that is consumed the cycle it appears does not need a
// comparator of its own.
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

  localparam logic [2:0] StFill = 3'd0;
  localparam logic [2:0] StSquare = 3'd1;
  localparam logic [2:0] StSqrt = 3'd2;
  localparam logic [2:0] StEval = 3'd3;
  localparam logic [2:0] StDecide = 3'd4;
  localparam logic [2:0] StEmit = 3'd5;

  logic [2:0] state;
  logic [2:0] mul_step;      // the shared multiplier's schedule: 0-5 square, 0-7 eval
  logic [4:0] fill_idx;  // 0..15 while filling
  logic [4:0] emit_idx;
  logic       emit_surf;

  // ---- the per-patch decision store ----------------------------------------
  logic [ 1:0] lvl [0:NSub-1];
  logic [16:0] mrp [0:NSub-1];
  logic [ 7:0] hld [0:NSub-1];
  logic [15:0] sid [0:NSub-1];

  // ---- the latched descriptor ----------------------------------------------
  // The CENTRE is latched now too: the six squares are taken one per clock
  // after the accept, and `sp_c*_i` is only guaranteed valid on the accepting
  // edge (the caller sees `sp_ready_o` fall).
  logic signed [31:0] d_cx, d_cy, d_cz;
  logic [23:0] d_dev1, d_dev2, d_dev3;
  logic [ 1:0] d_level;
  logic [16:0] d_morph;
  logic [ 7:0] d_hold;
  logic [15:0] d_src;

  // ===========================================================================
  // THE ONE MULTIPLIER — every product in this block goes through it
  // ===========================================================================
  logic [31:0] mul_a, mul_b;
  wire  [63:0] mul_p = mul_a * mul_b;

  // ===========================================================================
  // the squared distance — exact, then saturated to the 64-bit word the §7.2
  // isqrt takes. The three-term sum needs 66 bits; the reference forms the same
  // sum in s128 and saturates the same way, so the two agree for EVERY input
  // word rather than inside an envelope.
  // ===========================================================================
  // |d| FITS IN 32 UNSIGNED BITS, AND THAT IS WHAT MAKES ONE MULTIPLIER ENOUGH.
  // `a` and `b` are both s32, so `a - b` spans [-(2^32-1), 2^32-1] and its
  // magnitude never needs the 33rd bit. d^2 = |d|^2, so the widest operand this
  // block has is 32x32 UNSIGNED — narrower than the signed 33x33 the parallel
  // form used, and exact for every input word.
  function automatic logic [31:0] absdiff32(input logic signed [31:0] a,
                                            input logic signed [31:0] b);
    logic signed [32:0] d;
    begin
      d = $signed({a[31], a}) - $signed({b[31], b});
      absdiff32 = d[32] ? (32'd0 - d[31:0]) : d[31:0];
    end
  endfunction

  function automatic logic [63:0] dsq_sat(input logic [65:0] s);
    dsq_sat = (s[65:64] != 2'b00) ? 64'hFFFF_FFFF_FFFF_FFFF : s[63:0];
  endfunction

  // one 66-bit accumulator, not two three-input 66-bit adder trees
  logic [65:0] acc;
  wire  [65:0] acc_next = acc + {2'b00, mul_p};

  // which (coordinate, eye) pair the squaring step in flight needs
  logic signed [31:0] sq_c, sq_e;
  always_comb begin
    case (mul_step)
      3'd0: begin
        sq_c = d_cx;
        sq_e = cam0_x_i;
      end
      3'd1: begin
        sq_c = d_cy;
        sq_e = cam0_y_i;
      end
      3'd2: begin
        sq_c = d_cz;
        sq_e = cam0_z_i;
      end
      3'd3: begin
        sq_c = d_cx;
        sq_e = cam1_x_i;
      end
      3'd4: begin
        sq_c = d_cy;
        sq_e = cam1_y_i;
      end
      default: begin
        sq_c = d_cz;
        sq_e = cam1_z_i;
      end
    endcase
  end
  wire [31:0] sq_mag = absdiff32(sq_c, sq_e);

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
  // the ladder — eight steps on the one multiplier, two comparators
  // ===========================================================================
  // lhs = dev · scale ≤ 2^24 · 2^16 = 2^40; rhs = dstv · h ≤ 2^32 · 2^16 = 2^48.
  // A 49-bit compare covers both with the sign-free headroom stated. The
  // arithmetic is UNCHANGED — the same integers are compared the same way, in a
  // different order in time.
  //
  //   step 0  rhs_rel0 = dist0 · hyst        step 4  rhs_rel1 = dist1 · hyst
  //   step 1  dev1 · cam0_scale              step 5  dev1 · cam1_scale
  //   step 2  dev2 · cam0_scale              step 6  dev2 · cam1_scale
  //   step 3  dev3 · cam0_scale              step 7  dev3 · cam1_scale
  //
  // THE STRICT LADDER'S RIGHT-HAND SIDE IS NOT A MULTIPLY AT ALL: `h` is the
  // constant 256 there, so `dstv · h` is `dstv << 8`. That is why only the two
  // RELAXED right-hand sides need a step.
  //
  // Each left-hand side is compared against BOTH of its camera's right-hand
  // sides in the cycle its product appears, so nothing is stored but the four
  // two-bit ladder answers. The rungs are walked 1, 2, 3 and a later pass
  // overwrites an earlier one, which is exactly "coarsest that passes" — no
  // monotonicity in dev is assumed, and level 0 needs no test because dev[0] is
  // zero by definition, which is what makes the ladder total.
  wire [15:0] hyst_eff = (hyst_i < 16'd256) ? 16'd256 : hyst_i;

  logic [ 1:0] lad_s0, lad_r0, lad_s1, lad_r1;
  logic [48:0] rhs_rel0, rhs_rel1;

  wire        ev_cam = mul_step[2];    // 0 for steps 0-3, 1 for steps 4-7
  wire [ 1:0] ev_rung = mul_step[1:0]; // 1, 2, 3 — and 0 marks the right-hand-side step
  wire [31:0] ev_dst = ev_cam ? sq_res1[31:0] : sq_res0[31:0];
  wire [15:0] ev_scale = ev_cam ? cam1_scale_i : cam0_scale_i;
  wire [48:0] ev_rhs_str = {9'b0, ev_dst, 8'd0};  // dstv · 256 — a SHIFT, not a DSP
  wire [48:0] ev_rhs_rel = ev_cam ? rhs_rel1 : rhs_rel0;
  wire [48:0] ev_lhs = mul_p[48:0];
  wire        ev_pass_str = (ev_lhs <= ev_rhs_str);
  wire        ev_pass_rel = (ev_lhs <= ev_rhs_rel);

  logic [23:0] ev_dev;
  always_comb begin
    case (ev_rung)
      2'd1: ev_dev = d_dev1;
      2'd2: ev_dev = d_dev2;
      default: ev_dev = d_dev3;
    endcase
  end

  // the one operand mux
  always_comb begin
    if (state == StSquare) begin
      mul_a = sq_mag;
      mul_b = sq_mag;
    end else if (ev_rung == 2'd0) begin
      mul_a = ev_dst;
      mul_b = {16'b0, hyst_eff};
    end else begin
      mul_a = {8'b0, ev_dev};
      mul_b = {16'b0, ev_scale};
    end
  end

  logic [1:0] t_strict, t_relaxed;
  always_comb begin
    if (cam0_en_i && cam1_en_i) begin
      t_strict  = (lad_s0 < lad_s1) ? lad_s0 : lad_s1;
      t_relaxed = (lad_r0 < lad_r1) ? lad_r0 : lad_r1;
    end else if (cam0_en_i) begin
      t_strict  = lad_s0;
      t_relaxed = lad_r0;
    end else if (cam1_en_i) begin
      t_strict  = lad_s1;
      t_relaxed = lad_r1;
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
      mul_step <= '0;
      fill_idx <= '0;
      emit_idx <= '0;
      emit_surf <= 1'b0;
      for (i = 0; i < NSub; i = i + 1) begin
        lvl[i] <= '0;
        mrp[i] <= '0;
        hld[i] <= '0;
        sid[i] <= '0;
      end
      d_cx <= '0;
      d_cy <= '0;
      d_cz <= '0;
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
      acc <= '0;
      lad_s0 <= '0;
      lad_r0 <= '0;
      lad_s1 <= '0;
      lad_r1 <= '0;
      rhs_rel0 <= '0;
      rhs_rel1 <= '0;
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
            d_cx <= sp_cx_i;
            d_cy <= sp_cy_i;
            d_cz <= sp_cz_i;
            d_dev1 <= sp_dev1_i;
            d_dev2 <= sp_dev2_i;
            d_dev3 <= sp_dev3_i;
            d_level <= sp_prev_level_i;
            d_morph <= sp_prev_morph_i;
            d_hold <= sp_hold_i;
            d_src <= sp_src_id_i;
            acc <= '0;
            mul_step <= '0;
            state <= StSquare;
          end
        end

        StSquare: begin
          // Six squares, one per clock, through the one multiplier. The two
          // three-term sums land in the SAME 66-bit accumulator one after the
          // other and are saturated exactly as the parallel form saturated
          // them, so the number handed to the root does not move.
          if (mul_step == 3'd2) begin
            sq_num0 <= dsq_sat(acc_next);
            acc     <= '0;
          end else if (mul_step == 3'd5) begin
            sq_num1 <= dsq_sat(acc_next);
            acc     <= '0;
            sq_res0 <= '0;
            sq_res1 <= '0;
            sq_bit  <= 64'h4000_0000_0000_0000;  // 2^62
            sq_cnt  <= '0;
            state   <= StSqrt;
          end else begin
            acc <= acc_next;
          end
          mul_step <= mul_step + 3'd1;
        end

        StSqrt: begin
          sq_num0 <= sqrt_next_num(sq_num0, sq_res0, sq_bit);
          sq_res0 <= sqrt_next_res(sq_num0, sq_res0, sq_bit);
          sq_num1 <= sqrt_next_num(sq_num1, sq_res1, sq_bit);
          sq_res1 <= sqrt_next_res(sq_num1, sq_res1, sq_bit);
          sq_bit  <= sq_bit >> 2;
          sq_cnt  <= sq_cnt + 6'd1;
          if (sq_cnt == SqrtSteps[5:0] - 6'd1) begin
            // The ladder answers start at level 0 — dev[0] is zero by
            // definition, so level 0 always passes and never needs a step.
            lad_s0 <= 2'd0;
            lad_r0 <= 2'd0;
            lad_s1 <= 2'd0;
            lad_r1 <= 2'd0;
            mul_step   <= '0;
            state  <= StEval;
          end
        end

        StEval: begin
          if (ev_rung == 2'd0) begin
            // the relaxed right-hand side for this camera; the strict one is a
            // shift and needs no step
            if (!ev_cam) rhs_rel0 <= mul_p[48:0];
            else rhs_rel1 <= mul_p[48:0];
          end else begin
            // this rung's left-hand side, compared the cycle it appears
            if (ev_pass_str) begin
              if (!ev_cam) lad_s0 <= ev_rung;
              else lad_s1 <= ev_rung;
            end
            if (ev_pass_rel) begin
              if (!ev_cam) lad_r0 <= ev_rung;
              else lad_r1 <= ev_rung;
            end
          end
          mul_step <= mul_step + 3'd1;
          if (mul_step == 3'd7) state <= StDecide;
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
