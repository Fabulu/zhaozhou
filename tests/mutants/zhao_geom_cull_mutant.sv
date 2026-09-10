// zhao_geom_cull_mutant.sv -- A DELIBERATELY BROKEN COPY. NOT SHIPPED.
//
// This exists to make the MUL_LANES sequencer's NEW machinery -- the
// issue/commit accumulator that assembles each plane's `dot + slack` from
// registered lane products -- a demonstrated instrument rather than an
// argument. The accumulator's characteristic fault is a PLANE-BOUNDARY error:
// every product is individually exact, every handshake completes, the walk is
// exactly the declared 21 ticks, ready_o and valid_o behave -- and every plane
// after the first silently carries its predecessor's partial sum into its own
// verdict. The spatial arm (MUL_LANES=4) could never exhibit this, because it
// never held state between planes, so no earlier run of the directed suite has
// ever shown the checker can see it.
//
// The one substantive change, in g_seq's commit-side finishing sum:
//
//     assign psum[0] = first_q ? kterm_q : acc;          // a fresh plane starts a fresh sum
//  -> assign psum[0] = first_q ? (acc + kterm_q) : acc;  // MUTANT: the old sum is never cleared
//
// INVERTED POLARITY: driven by tests/differential/geom_cull_mutant_control.cpp
// against the same zref::cull oracle the directed suite uses; the control
// PASSES when the differential comparison FAILS. It also asserts that the exact
// walk law still holds on the mutant -- the timing pin is blind to this fault,
// which is the reason the differential exists beside it. Evidence about the
// instrument, not about the design.
//
// The module is RENAMED so a source-list mistake can never elaborate it in
// place of the real one, and it lives under tests/ where
// check_forbidden_sources.py will not find it in a production closure.
//
// REGENERATE IT if zhao_geom_cull.sv changes shape: this is a copy, and a copy
// of an old version is a positive control for a block that no longer exists.

// zhao_geom_cull.sv — GEOM.MESHFETCH's conservative per-camera frustum
// rejection of an instance bounding sphere (phase 8, ZH-037).
//
// Law, in citation order:
//   docs/OWNER_DOCKET.md, "RULED 2026-08-22 — 'visibility sectors' is deleted"
//       — THE ruling. Conservative per-camera frustum rejection of an instance
//       bounding sphere before vertex decode; reject only when the sphere is
//       outside EVERY active camera; carry a two-bit per-camera visibility
//       result downstream.
//   reference/include/zref/zref_cull.hpp — the oracle this block is measured
//       against, function for function. `frustum_planes`, `normal_len_ceil`,
//       `sphere_outside_plane`, `cull_instance`.
//   reference/src/zrender/rast.cpp:43 `zref::render::project_vertex` — the clip
//       convention the planes are extracted FROM. Row-major, +Y NDC downward,
//       and `w > 0` as the ONLY depth condition, hence FIVE planes.
//   spec/qformats.md §7.2 `isqrt_u64` — the restoring digit recurrence the
//       length bound below runs, widened to a 66-bit argument.
//
// WHAT THIS BLOCK IS NOT. GEOM.MESHFETCH's purpose line gives it three jobs.
// The LOD ladder is `zhao_geom_lod`. The DESCRIPTOR FETCH is not here and
// cannot be: the meshlet schema is explicitly unfrozen (blocks.yml, "Meshlet
// limits are Phase-0 data (P2 risk 1) — schema fields stay unfrozen"), so a
// block that fetched descriptors would have to invent a layout. This one takes
// the bound as PORTS, which is the same boundary `zhao_geom_lod` draws — the
// caller owns the per-instance data — and is therefore not blocked on a
// decision that has not been made.
//
// ---------------------------------------------------------------------------
// THE FIVE PLANES ARE ROWS OF THE MATRIX THE RENDERER ALREADY PROJECTS WITH
// ---------------------------------------------------------------------------
// clip.x = row0·v, clip.y = row1·v, clip.w = row3·v, and the volume
// `project_vertex` draws is w > 0, -w <= x <= w, -w <= y <= w. Each condition
// rearranges into a half-space p·v >= 0:
//
//     0 left   = row3 + row0        3 top    = row3 - row1
//     1 right  = row3 - row0        4 near   = row3        (this IS w > 0)
//     2 bottom = row3 + row1
//
// ROW2 IS NEVER READ, and that is not an omission — it is "this machine has no
// z clip", stated in arithmetic. Adding a far plane or a near-z plane would
// reject geometry the renderer would have drawn.
//
// Because the planes are combinations of stored matrix ROWS, they are NOT
// stored. Four 33-bit adds behind a five-way mux reproduce any plane from the
// matrix that is already registered, which is 1,320 flops this block does not
// pay for. What IS stored per (view, plane) is the one quantity that cannot be
// re-derived cheaply: the ceiling of the normal's length.
//
// A PLANE COMPONENT DOES NOT FIT IN 32 BITS. It is the sum of two fx16 words,
// so its range is [-2^32, 2^32-2] and it is carried as signed 33. Truncating it
// back to 32 would wrap on any matrix with a large row — silently, and only for
// the geometry near that edge.
//
// ---------------------------------------------------------------------------
// THE ROUNDING IS A CEILING, AND THE HABIT IS A FLOOR
// ---------------------------------------------------------------------------
// A sphere (c, r) is wholly outside plane p iff dot(p, c) < -r * |(a,b,c)|, and
// |(a,b,c)| is irrational. The ratified primitive is an EXACT FLOOR square root
// — and floor is the wrong direction. A floor makes the right-hand side less
// negative, which makes rejection EASIER, which rejects spheres that are
// visible. That does not cost performance, it DELETES GEOMETRY, near the screen
// edges, where it reads as objects popping out of existence. A ceiling can only
// keep a sphere that could have been dropped: wasted decode work, no geometry.
//
// The ceiling costs nothing extra here. The restoring recurrence ends with
// `sq_num == n - res*res`, so "n is not a perfect square" is exactly
// "the remainder is non-zero" — no squaring the result back, no second compare.
//
// ---------------------------------------------------------------------------
// WHERE THE WORK LIVES — AND WHY THE SQUARE ROOT IS ON THE CONFIG PATH
// ---------------------------------------------------------------------------
// The five planes and their five length bounds are per CAMERA per FRAME. The
// sphere test is per INSTANCE, and there are potentially thousands. So the five
// square roots run when a matrix is WRITTEN, never when an instance is tested —
// the same move `zhao_geom_lod` made when it turned its divides into compares.
//
// ---------------------------------------------------------------------------
// HOW MANY MULTIPLIERS — MUL_LANES
// ---------------------------------------------------------------------------
// One plane test is FOUR products: a·cx, b·cy, c·cz and r·len. The d term is a
// shift. An instance is ten plane tests (five planes, two views), so an
// evaluation is FORTY products, and the question is how many of them happen in
// the same clock.
//
// The first version of this block (fitted: 1,102 ALMs / 15 DSPs at 2a711f0)
// answered "four": one plane per cycle, three 33x32 multipliers for the dot
// plus one 32x34 for the slack, ten cycles per instance — and a FIFTH
// multiplier, the 33x33 square of the extraction path, which runs only on a
// matrix write and sat idle for the rest of the frame. Every multiply function
// call in an RTL file is its own physical multiplier, which is how a block with
// a 10-cycle instance path came to hold five of them. Its own header named the
// lever and declined to pull it against a guessed instance rate.
//
// The rate is no longer guessed. GEOM.MESHFETCH.md sizes the demand at roughly
// 6,100 meshlet decisions per frame (the 256-creature content tier at ~24
// meshlets each). Against that, even one product per clock (II 42) is under
// 20% of the reserved 1,333,333-clock frame, so the multipliers are the scarce
// resource and the clocks are not. Hence:
//
// MUL_LANES = 2 (DEFAULT): TWO 33x33 signed multipliers, products registered.
//   Each plane's four products issue over two cycles (a·cx and b·cy, then
//   c·cz and r·len); a plane's sum commits one cycle behind the issue, so the
//   walk is 20 issue cycles plus one trailing commit: `valid_o` 21 clocks after
//   the accepting edge, initiation interval 22. The extraction's squares run on
//   lane 0 — extraction and evaluation are never in the same cycle (`state` is
//   one or the other), so the share costs one operand-mux arm and nothing else.
//   Predicted 2 x 3 = 6 DSP: the calibration measures a signed 33-bit product
//   at 3 DSP (tools/budget/calibration.json, calib_mul_s33), and
//   zhao_terrain_normals maps its ONE muxed, registered 33x33 lane at exactly
//   3. NOT YET FITTED in this block; the leaf fit in design/fit_targets.yml is
//   the gate.
// MUL_LANES = 1: ONE lane, four issue cycles per plane; `valid_o` 41 clocks
//   after accept, II 42. Predicted 3 DSP.
// MUL_LANES = 4: the original spatial arm, kept verbatim so the fitted 15-DSP
//   circuit can still be elaborated and compared: one plane per cycle, four
//   combinational products, its own extraction square. `valid_o` 10 clocks
//   after accept, II 11.
//
// ALL THREE ARMS ARE BIT-IDENTICAL. Each product is an exact integer whichever
// cycle it is formed in, and the test `dot < -slack` is evaluated as
// `dot + slack < 0` on a 68-bit accumulator, which is the same predicate on
// exact integers (nothing here can overflow 68 bits; see DOT_W). Sequencing
// moves cycles, never bits — the same argument zhao_geom_mat3x4_mul makes.
//
// NO LANE OPERAND IS WIDER THAN 33 BITS, on purpose. A shared lane is sized by
// its widest operand, and the slack product's length bound is 34 bits unsigned
// (LEN_W). The calibration has a measured point at 33 bits (3 DSP) and the
// next at 40 (4 DSP), and nothing in between — so instead of asking a 33x35
// lane to land on the cheap side of an unmeasured cliff, the product is split
// at bit 32:
//
//     r * len  =  r * len[31:0]  +  (r * len[LEN_W-1:32]) << 32
//
// The first term is a 33x33 lane product (the low word zero-extended, so it is
// non-negative), the second is at most a two-bit multiplier — a shift-and-add
// of r, no DSP — and it enters the accumulator with the d term. Exact, because
// it is the integer identity it looks like.
//
// The extraction is fully sequential because it is rare. In the lane arms its
// square is registered like every other product, so one plane costs 4 + 33 + 1
// = 38 cycles and a view 190 (the spatial arm: 3 + 33 + 1 = 37, 185).
//
// ---------------------------------------------------------------------------
// THE HANDSHAKE EXISTS TO MAKE STALE PLANES IMPOSSIBLE
// ---------------------------------------------------------------------------
// A cull evaluated against a length bound belonging to a previous matrix would
// delete geometry, silently. So a write to any matrix word marks that view
// DIRTY, `ready_o` is low while anything is dirty or being extracted, and a
// tick is only honoured when `ready_o` is high. The caller cannot forget to ask
// for an extraction, because there is nothing to ask.
//
// Conservative SystemVerilog subset only (charter §2); no package deps.

`default_nettype none

module zhao_geom_cull_mutant #(
    // 2 (default): two shared 33x33 lanes, products registered; 1: one lane;
    // 4: the original spatial arm (four combinational multipliers plus its own
    // extraction square). See "HOW MANY MULTIPLIERS" above.
    parameter int MUL_LANES = 2
) (
    input wire clk,
    input wire rst_n,

    // ---- configuration: two views, sixteen matrix words each ---------------
    // addr 0..15 : matrix row-major m[0..15], fx16, EXACTLY the words
    //              zhao_geom_project takes at the same addresses.
    // addr >= 16 : ignored. GEOM.PROJECT puts the VIEWPORT there and the cull
    //              deliberately does not take it: rejection happens in CLIP
    //              space, and the viewport only maps NDC to pixels afterwards.
    //              A sphere outside the clip volume is outside it whatever the
    //              viewport does, so reading those words would imply a
    //              dependency that does not exist.
    input wire        cfg_we_i,
    input wire        cfg_view_i,
    input wire [ 4:0] cfg_addr_i,
    input wire [31:0] cfg_data_i,

    // ---- one instance bound in ---------------------------------------------
    // Accepted only while ready_o is high.
    input wire        tick_i,
    // bit v: camera v is active this frame
    input wire [ 1:0] active_i,
    // the instance bounding sphere, fx16 world
    input wire signed [31:0] centre_x_i,
    input wire signed [31:0] centre_y_i,
    input wire signed [31:0] centre_z_i,
    input wire signed [31:0] radius_i,

    output logic       ready_o,

    // ---- the verdict --------------------------------------------------------
    output logic       valid_o,
    // bit v: camera v is active AND may see the sphere
    output logic [1:0] vis_o,
    // no active camera may see it — the only case the instance is dropped
    output logic       reject_o
);

  // Quartus 17 needs elaboration checks inside `initial begin ... end` (a bare
  // module-scope `if` is a synthesis syntax error there), and `--lint-only`
  // does not run this block — only elaboration does.
  initial begin
    if (MUL_LANES != 1 && MUL_LANES != 2 && MUL_LANES != 4) begin
      $fatal(1, "zhao_geom_cull: MUL_LANES must be 1, 2 or 4, got %0d", MUL_LANES);
    end
    // The lane arms' slack split (`hi_mul`) is written out for a 34-bit bound.
    if (LEN_W != 34) begin
      $fatal(1, "zhao_geom_cull: hi_mul is written for LEN_W == 34, got %0d", LEN_W);
    end
  end

  // ---------------------------------------------------------------------------
  // widths, stated rather than assumed
  // ---------------------------------------------------------------------------
  // PC_W: a plane component is row3[j] +/- rowk[j], two fx16 words, so its
  // range is [-2^32, 2^32-2]. Signed 33 holds exactly that and not one bit more.
  localparam int unsigned PC_W = 33;

  // SQ_W: the sum of squares is 3 * (2^32)^2 = 3*2^64 at worst, which needs 66
  // bits unsigned. Every register in the recurrence is this wide: the argument
  // (< 2^66), the running remainder (<= the argument), the trial bit (starts at
  // 4^32 = 2^64) and the running root, whose INTERMEDIATE value reaches 2^64 on
  // the first step even though the answer is under 2^34. `res + bit` is at most
  // 2^65, so the compare cannot wrap either.
  localparam int unsigned SQ_W = 66;

  // LEN_W: the answer. sqrt(3*2^64) = 2^32*sqrt(3) < 2^32.8, so 33 bits would
  // hold it and the ceiling's +1 cannot carry out of that; 34 is declared,
  // one bit of headroom over the argued bound. (An earlier header wrote
  // "2^33.8" here; the exponent is 32.79.) The lane arms split the slack
  // product at bit 32, so LEN_W - 32 bits of the bound go through a shift-and-
  // add rather than a multiplier — see the header.
  localparam int unsigned LEN_W = 34;

  // SQRT_STEPS: the trial bit walks 4^32, 4^31, ... 4^0 — 33 steps. Starting
  // ABOVE the argument's leading power of four is harmless: the compare fails
  // and the root shifts a zero, which is what the reference's `while (bit >
  // num) bit >>= 2` prologue does in software.
  localparam int unsigned SQRT_STEPS = 33;

  // DOT_W: three 33x32 products (each |.| <= 2^63) plus d << 16 (|.| <= 2^48),
  // so |dot| < 3*2^63 + 2^48 < 2^64.6. The lane arms also fold the slack
  // (|r * len| < 2^31 * 2^34 = 2^65) into the same accumulator, so the sum
  // stays under 2^66 in magnitude at every partial. 68 bits leaves headroom
  // over that for ANY matrix word, not merely a plausible one; the slack is on
  // ADDERS, never on a multiplier operand, which is where width turns into
  // DSPs.
  localparam int unsigned DOT_W = 68;

  // SLK_W: radius (< 2^31) times the length ceiling (< 2^34) is under 2^65.
  // 66 bits holds it signed. (Spatial arm only; the lane arms accumulate.)
  localparam int unsigned SLK_W = 66;

  // ---------------------------------------------------------------------------
  // pure functions — width management only, no law
  // ---------------------------------------------------------------------------
  function automatic logic signed [PC_W-1:0] ext33(input logic signed [31:0] v);
    ext33 = $signed({v[31], v});
  endfunction

  // A plane-dot product: 33 x 32. The operands are widened to the RESULT width
  // rather than left to the assignment context, which is how zhao_geom_project
  // spells `mul32` — and the reason is `zhao_geom_lod`, where 72-bit slack on a
  // 32-bit operand cost 28 DSPs against 18 for the honest width.
  function automatic logic signed [64:0] mul_pc(input logic signed [PC_W-1:0] a,
                                                input logic signed [31:0] b);
    mul_pc = $signed({{(65 - PC_W) {a[PC_W-1]}}, a}) * $signed({{(65 - 32) {b[31]}}, b});
  endfunction

  function automatic logic signed [DOT_W-1:0] ext_dot(input logic signed [64:0] v);
    ext_dot = $signed({{(DOT_W - 65) {v[64]}}, v});
  endfunction

  function automatic logic signed [DOT_W-1:0] ext_pc_d(input logic signed [PC_W-1:0] v);
    ext_pc_d = $signed({{(DOT_W - PC_W) {v[PC_W-1]}}, v});
  endfunction

  // radius x length ceiling. The length is UNSIGNED and zero-extended; the
  // radius is signed and sign-extended, so a (domain-violating) negative radius
  // produces a negative slack exactly as the reference's __int128 product does.
  function automatic logic signed [SLK_W-1:0] mul_slack(input logic signed [31:0] r,
                                                        input logic [LEN_W-1:0] len);
    mul_slack = $signed({{(SLK_W - 32) {r[31]}}, r}) * $signed({{(SLK_W - LEN_W) {1'b0}}, len});
  endfunction

  function automatic logic signed [DOT_W-1:0] ext_slack(input logic signed [SLK_W-1:0] v);
    ext_slack = $signed({{(DOT_W - SLK_W) {v[SLK_W-1]}}, v});
  endfunction

  // a 66-bit lane product, sign-extended to the accumulator
  function automatic logic signed [DOT_W-1:0] ext_p(input logic signed [2*PC_W-1:0] v);
    ext_p = $signed({{(DOT_W - 2 * PC_W) {v[2*PC_W-1]}}, v});
  endfunction

  // ---------------------------------------------------------------------------
  // configuration registers, and the dirty bit that makes staleness impossible
  // ---------------------------------------------------------------------------
  logic signed [31:0] mat[0:1][0:15];
  logic [LEN_W-1:0] len_ceil[0:1][0:4];
  logic [1:0] dirty;

  // ---------------------------------------------------------------------------
  // the sequencer
  // ---------------------------------------------------------------------------
  localparam logic [2:0] S_IDLE = 3'd0;
  localparam logic [2:0] S_SQ = 3'd1;
  localparam logic [2:0] S_SQRT = 3'd2;
  localparam logic [2:0] S_STORE = 3'd3;
  localparam logic [2:0] S_EVAL = 3'd4;

  logic [2:0] state;

  // extraction — the recurrence is the FSM's; the squares are the arm's
  logic prep_view;
  logic [2:0] prep_plane;
  logic [SQ_W-1:0] sq_num, sq_res, sq_bit;
  logic [5:0] sq_cnt;

  // evaluation — the latched instance is the FSM's; the walk is the arm's
  logic [1:0] ev_active;
  logic signed [31:0] ev_cx, ev_cy, ev_cz, ev_r;

  // ---------------------------------------------------------------------------
  // what a multiplier arm exports, whichever arm it is
  // ---------------------------------------------------------------------------
  // The FSM below is the same for every MUL_LANES. An arm owns its own cursors
  // and accumulators and tells the FSM exactly two things per phase:
  //   S_SQ   : `sq_done` — the three squares are summed, in `sumsq_full`;
  //   S_EVAL : `ev_done` — the last plane's verdict is in, `ev_vis`/`ev_reject`.
  // Each arm resets its own walk whenever `state` is not its phase, so entry
  // into a phase is always clean and the FSM never has to know how long the
  // arm takes. `ev_view`/`ev_plane` are the arm's ISSUE cursor and drive the
  // shared plane mux.
  logic ev_view;
  logic [2:0] ev_plane;
  logic sq_done;
  logic [SQ_W-1:0] sumsq_full;
  logic ev_done;
  logic [1:0] ev_vis;
  logic ev_reject;

  // ---------------------------------------------------------------------------
  // ONE plane derivation, shared by extraction and evaluation
  // ---------------------------------------------------------------------------
  // Extraction and evaluation can never run in the same cycle — `state` is one
  // or the other — so the five-way mux is built once and selected by whichever
  // phase is live. Two copies of this is two places for the plane table to be
  // wrong in only one of them.
  // ENFORCED-BY: fpga/rtl/geometry/zhao_geom_cull.sv:state
  logic sel_view;
  logic [2:0] sel_plane;
  assign sel_view  = (state == S_EVAL) ? ev_view : prep_view;
  assign sel_plane = (state == S_EVAL) ? ev_plane : prep_plane;

  // Which row joins row3, and with which sign. `near` takes row3 alone.
  logic       cmb_row;  // 0 -> row0 (x), 1 -> row1 (y)
  logic       cmb_add;  // 1 -> add, 0 -> subtract
  logic       cmb_none;  // 1 -> row3 by itself (the near plane)
  always_comb begin
    case (sel_plane)
      3'd0: begin  // left   = row3 + row0
        cmb_row  = 1'b0;
        cmb_add  = 1'b1;
        cmb_none = 1'b0;
      end
      3'd1: begin  // right  = row3 - row0
        cmb_row  = 1'b0;
        cmb_add  = 1'b0;
        cmb_none = 1'b0;
      end
      3'd2: begin  // bottom = row3 + row1
        cmb_row  = 1'b1;
        cmb_add  = 1'b1;
        cmb_none = 1'b0;
      end
      3'd3: begin  // top    = row3 - row1
        cmb_row  = 1'b1;
        cmb_add  = 1'b0;
        cmb_none = 1'b0;
      end
      default: begin  // near = row3   (w > 0)
        cmb_row  = 1'b0;
        cmb_add  = 1'b0;
        cmb_none = 1'b1;
      end
    endcase
  end

  logic signed [31:0] base0, base1, base2, base3;  // row3
  logic signed [31:0] othr0, othr1, othr2, othr3;  // row0 or row1
  always_comb begin
    base0 = mat[sel_view][12];
    base1 = mat[sel_view][13];
    base2 = mat[sel_view][14];
    base3 = mat[sel_view][15];
    if (cmb_row) begin
      othr0 = mat[sel_view][4];
      othr1 = mat[sel_view][5];
      othr2 = mat[sel_view][6];
      othr3 = mat[sel_view][7];
    end else begin
      othr0 = mat[sel_view][0];
      othr1 = mat[sel_view][1];
      othr2 = mat[sel_view][2];
      othr3 = mat[sel_view][3];
    end
  end

  logic signed [PC_W-1:0] pl_a, pl_b, pl_c, pl_d;
  always_comb begin
    if (cmb_none) begin
      pl_a = ext33(base0);
      pl_b = ext33(base1);
      pl_c = ext33(base2);
      pl_d = ext33(base3);
    end else if (cmb_add) begin
      pl_a = ext33(base0) + ext33(othr0);
      pl_b = ext33(base1) + ext33(othr1);
      pl_c = ext33(base2) + ext33(othr2);
      pl_d = ext33(base3) + ext33(othr3);
    end else begin
      pl_a = ext33(base0) - ext33(othr0);
      pl_b = ext33(base1) - ext33(othr1);
      pl_c = ext33(base2) - ext33(othr2);
      pl_d = ext33(base3) - ext33(othr3);
    end
  end

  // ---------------------------------------------------------------------------
  // the multiplier arms
  // ---------------------------------------------------------------------------
  generate
    if (MUL_LANES == 4) begin : g_spatial

      // ---- the original arrangement: one plane per cycle, four products ------
      // Kept verbatim (moved into this arm, not rewritten) so the fitted
      // 15-DSP circuit remains elaborable under -GMUL_LANES=4.

      // extraction: one square per cycle, then the recurrence
      logic [1:0] sq_j;
      logic [SQ_W-1:0] sumsq;

      logic signed [PC_W-1:0] sq_operand;
      always_comb begin
        case (sq_j)
          2'd0:    sq_operand = pl_a;
          2'd1:    sq_operand = pl_b;
          default: sq_operand = pl_c;
        endcase
      end

      // A square is non-negative, so the signed product's sign bit is always
      // clear and reading it as unsigned is exact rather than a
      // reinterpretation.
      logic signed [2*PC_W-1:0] sq_prod;
      assign sq_prod = $signed(sq_operand) * $signed(sq_operand);

      logic [SQ_W-1:0] sq_term;
      assign sq_term = SQ_W'($unsigned(sq_prod));

      logic [SQ_W-1:0] sumsq_next;
      assign sumsq_next = sumsq + sq_term;

      // The recurrence starts from the FULL sum, which only exists on the
      // third cycle — hence sumsq_next rather than the register.
      assign sq_done    = (state == S_SQ) && (sq_j == 2'd2);
      assign sumsq_full = sumsq_next;

      always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
          sq_j  <= 2'd0;
          sumsq <= '0;
        end else if (state == S_SQ) begin
          sumsq <= sumsq_next;
          sq_j  <= sq_j + 2'd1;
        end else begin
          sq_j  <= 2'd0;
          sumsq <= '0;
        end
      end

      // evaluation: one plane per cycle
      logic [1:0] ev_outside;

      logic signed [DOT_W-1:0] dot;
      assign dot = ext_dot(mul_pc(pl_a, ev_cx)) + ext_dot(mul_pc(pl_b, ev_cy)) +
          ext_dot(mul_pc(pl_c, ev_cz)) + (ext_pc_d(pl_d) <<< 16);

      logic signed [DOT_W-1:0] slack;
      assign slack = ext_slack(mul_slack(ev_r, len_ceil[ev_view][ev_plane]));

      // wholly outside this plane
      logic outside_here;
      assign outside_here = (dot < -slack);

      // vis[v] = active[v] AND not outside[v]. `outside_here` is the last
      // plane of the last view and has not reached ev_outside yet, so it is
      // folded in here.
      assign ev_done = (state == S_EVAL) && (ev_plane == 3'd4) && (ev_view == 1'b1);
      assign ev_vis = {
        ev_active[1] & ~(ev_outside[1] | outside_here), ev_active[0] & ~ev_outside[0]
      };
      assign ev_reject = ~(ev_vis[1] | ev_vis[0]);

      always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
          ev_view    <= 1'b0;
          ev_plane   <= 3'd0;
          ev_outside <= 2'b00;
        end else if (state == S_EVAL) begin
          if (outside_here) ev_outside[ev_view] <= 1'b1;
          if (ev_plane == 3'd4) begin
            ev_plane <= 3'd0;
            if (ev_view == 1'b0) ev_view <= 1'b1;
          end else begin
            ev_plane <= ev_plane + 3'd1;
          end
        end else begin
          ev_view    <= 1'b0;
          ev_plane   <= 3'd0;
          ev_outside <= 2'b00;
        end
      end

    end else begin : g_seq

      // ---- MUL_LANES shared 33x33 lanes, products registered ----------------
      //
      // The pattern is zhao_terrain_normals.sv:203 and zhao_geom_mat3x4_mul's
      // g_seq: an ISSUE side walks an operand mux and presents one pair per
      // lane per cycle; the product lands in a register (the DSP block's own
      // output register — Quartus uses it when the product is registered, so
      // this costs no fabric); a COMMIT side one cycle behind accumulates.
      // Registering the product cuts the path into (mux + multiply) and (add),
      // which is why terrain_normals did it, and this block has no fitted Fmax
      // to protect but every reason not to start with a mux-multiply-add-
      // compare chain in one clock.

      // Term t of a plane: 0 = a·cx, 1 = b·cy, 2 = c·cz, 3 = r·len_lo. Issue
      // step s puts terms s*MUL_LANES .. s*MUL_LANES+MUL_LANES-1 on lanes
      // 0 .. MUL_LANES-1, so STEPS cycles issue one plane.
      localparam int unsigned STEPS = 4 / MUL_LANES;

      // ---- issue cursor ------------------------------------------------------
      logic [1:0] step;      // 0 .. STEPS-1
      logic       iss_done;  // every plane of both views has been issued
      logic [1:0] sq_j;      // 0..2 issue a square; 3 = the squares are all issued

      // ---- the slack operand, split at bit 32 --------------------------------
      // r * len = r * len[31:0] + (r * len[LEN_W-1:32]) << 32. The low word is
      // zero-extended to 33 bits (so it is a non-negative signed operand) and
      // goes through a lane; the high LEN_W-32 bits multiply r by a shift-and-
      // add and join the constant term. See the header for why no lane operand
      // is allowed past 33 bits.
      logic [LEN_W-1:0] len_cur;
      assign len_cur = len_ceil[ev_view][ev_plane];

      logic signed [PC_W-1:0] len_lo33;
      assign len_lo33 = $signed({1'b0, len_cur[31:0]});

      logic signed [DOT_W-1:0] r_ext;
      assign r_ext = $signed({{(DOT_W - 32) {ev_r[31]}}, ev_r});

      // r * len[33:32]: the multiplier is two bits, so this is
      // (len[32] ? r : 0) + (len[33] ? 2r : 0), written out rather than looped
      // so the RTL inventory sees two constant adds and not a variable shift.
      // The bound argued at LEN_W never sets bit 33; the term is here so the
      // arithmetic is exact for every value the register CAN hold.
      logic signed [DOT_W-1:0] hi_mul;
      assign hi_mul = (len_cur[32] ? r_ext : 68'sd0) + (len_cur[33] ? (r_ext + r_ext) : 68'sd0);

      logic signed [DOT_W-1:0] hi_term;
      assign hi_term = hi_mul <<< 32;

      // The plane's constant term: d << 16, plus the slack's high part. Formed
      // on the plane's first issue cycle and registered, so the commit side
      // never reads the plane mux.
      logic signed [DOT_W-1:0] kterm;
      assign kterm = (ext_pc_d(pl_d) <<< 16) + hi_term;

      // ---- the extraction's square operand -----------------------------------
      logic signed [PC_W-1:0] sq_operand;
      always_comb begin
        case (sq_j)
          2'd0:    sq_operand = pl_a;
          2'd1:    sq_operand = pl_b;
          default: sq_operand = pl_c;
        endcase
      end

      // ---- the lanes ---------------------------------------------------------
      logic signed [PC_W-1:0]   m_a [0:MUL_LANES-1];
      logic signed [PC_W-1:0]   m_b [0:MUL_LANES-1];
      logic signed [2*PC_W-1:0] m_p [0:MUL_LANES-1];
      logic signed [2*PC_W-1:0] p_q [0:MUL_LANES-1];

      genvar l;
      for (l = 0; l < MUL_LANES; l = l + 1) begin : g_lane
        // which of the four terms this lane carries on the current step
        logic [1:0] term;
        assign term = 2'(step * MUL_LANES + l);

        always_comb begin
          if ((l == 0) && (state == S_SQ)) begin
            // lane 0 squares a plane component during extraction
            m_a[l] = sq_operand;
            m_b[l] = sq_operand;
          end else begin
            case (term)
              2'd0: begin
                m_a[l] = pl_a;
                m_b[l] = ext33(ev_cx);
              end
              2'd1: begin
                m_a[l] = pl_b;
                m_b[l] = ext33(ev_cy);
              end
              2'd2: begin
                m_a[l] = pl_c;
                m_b[l] = ext33(ev_cz);
              end
              default: begin
                m_a[l] = ext33(ev_r);
                m_b[l] = len_lo33;
              end
            endcase
          end
        end

        // THE nonconstant multiply of this lane. 33x33 -> 66, exact.
        assign m_p[l] = m_a[l] * m_b[l];

        // the DSP output register
        always_ff @(posedge clk or negedge rst_n) begin
          if (!rst_n) p_q[l] <= '0;
          else p_q[l] <= m_p[l];
        end
      end

      // ---- commit-side bookkeeping, one cycle behind the issue ---------------
      logic pv_q;            // p_q holds evaluation products
      logic first_q;         // ... of a plane's first step (start a fresh sum)
      logic last_q;          // ... of a plane's last step (the verdict is in)
      logic view_q;          // ... for this view
      logic last_plane_q;    // ... and it is plane 4
      logic signed [DOT_W-1:0] kterm_q;
      logic sqv_q;           // p_q[0] holds a square
      logic sq_last_q;       // ... the third one

      logic signed [DOT_W-1:0] acc;
      logic [1:0] ev_outside;
      logic [SQ_W-1:0] sumsq;

      // The finishing sum of the products that landed this cycle, on top of
      // either the plane's constant term (first step) or the running sum — a
      // prefix chain over the lanes, so the sum is one adder per lane and no
      // combinational read-modify-write.
      logic signed [DOT_W-1:0] psum [0:MUL_LANES];
      assign psum[0] = first_q ? (acc + kterm_q) : acc;  // MUTANT: the previous plane's sum is never cleared
      genvar c;
      for (c = 0; c < MUL_LANES; c = c + 1) begin : g_sum
        assign psum[c+1] = psum[c] + ext_p(p_q[c]);
      end
      logic signed [DOT_W-1:0] fin;
      assign fin = psum[MUL_LANES];

      // dot + slack < 0  <=>  dot < -slack, exactly, on integers that fit.
      logic outside_now;
      assign outside_now = fin[DOT_W-1];

      // the square's commit: the third square completes the sum THIS cycle
      assign sq_done    = sqv_q && sq_last_q;
      assign sumsq_full = sumsq + SQ_W'($unsigned(p_q[0]));

      // the verdict: the last product of the last plane of the last view. As in
      // the spatial arm, this plane's `outside` has not reached ev_outside yet
      // and is folded in here.
      assign ev_done = pv_q && last_q && last_plane_q && view_q;
      assign ev_vis = {
        ev_active[1] & ~(ev_outside[1] | outside_now), ev_active[0] & ~ev_outside[0]
      };
      assign ev_reject = ~(ev_vis[1] | ev_vis[0]);

      always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
          step         <= 2'd0;
          iss_done     <= 1'b0;
          ev_view      <= 1'b0;
          ev_plane     <= 3'd0;
          pv_q         <= 1'b0;
          first_q      <= 1'b0;
          last_q       <= 1'b0;
          view_q       <= 1'b0;
          last_plane_q <= 1'b0;
          kterm_q      <= '0;
          acc          <= '0;
          ev_outside   <= 2'b00;
          sq_j         <= 2'd0;
          sqv_q        <= 1'b0;
          sq_last_q    <= 1'b0;
          sumsq        <= '0;
        end else begin
          // ---- extraction issue: squares on lane 0, one per cycle -----------
          if (state == S_SQ) begin
            sqv_q     <= (sq_j != 2'd3);
            sq_last_q <= (sq_j == 2'd2);
            if (sq_j != 2'd3) sq_j <= sq_j + 2'd1;
          end else begin
            sqv_q     <= 1'b0;
            sq_last_q <= 1'b0;
            sq_j      <= 2'd0;
            sumsq     <= '0;
          end
          // ---- extraction commit --------------------------------------------
          if (sqv_q) sumsq <= sumsq_full;

          // ---- evaluation issue ---------------------------------------------
          if (state == S_EVAL) begin
            if (!iss_done) begin
              pv_q         <= 1'b1;
              first_q      <= (step == 2'd0);
              last_q       <= (step == 2'(STEPS - 1));
              view_q       <= ev_view;
              last_plane_q <= (ev_plane == 3'd4);
              if (step == 2'd0) kterm_q <= kterm;
              if (step == 2'(STEPS - 1)) begin
                step <= 2'd0;
                if (ev_plane == 3'd4) begin
                  ev_plane <= 3'd0;
                  if (ev_view == 1'b1) iss_done <= 1'b1;
                  else ev_view <= 1'b1;
                end else begin
                  ev_plane <= ev_plane + 3'd1;
                end
              end else begin
                step <= step + 2'd1;
              end
            end else begin
              pv_q <= 1'b0;
            end
          end else begin
            step       <= 2'd0;
            iss_done   <= 1'b0;
            ev_view    <= 1'b0;
            ev_plane   <= 3'd0;
            pv_q       <= 1'b0;
            ev_outside <= 2'b00;
          end
          // ---- evaluation commit --------------------------------------------
          if (pv_q) begin
            if (last_q) begin
              if (outside_now) ev_outside[view_q] <= 1'b1;
            end else begin
              acc <= fin;
            end
          end
        end
      end

    end
  endgenerate

  // ---------------------------------------------------------------------------
  // one restoring step of the recurrence (the FSM's, whichever arm)
  // ---------------------------------------------------------------------------
  logic [SQ_W-1:0] sq_trial;
  logic sq_ge;
  assign sq_trial = sq_res + sq_bit;
  assign sq_ge = (sq_num >= sq_trial);

  // ---------------------------------------------------------------------------
  // the dirty bit, in its own block, with SET WRITTEN AFTER CLEAR
  // ---------------------------------------------------------------------------
  // A matrix write and the start of that view's extraction can land in the same
  // cycle, and nonblocking assignments resolve LAST-WINS, so the order of these
  // two lines decides which one survives.
  //
  // I FIRST WROTE THAT THE CLEAR WINNING WOULD SWALLOW THE WRITE AND LEAVE THE
  // BLOCK CULLING AGAINST A STALE LENGTH BOUND. THE MUTATION SWEEP DISPROVED IT.
  // Swapping these two lines (sweep M29) survives every test, and it survives
  // because it cannot be wrong:
  //
  //   · the clear only fires when `prep_start` does, i.e. in a cycle where the
  //     state is S_IDLE and an extraction is about to begin;
  //   · a write in that cycle commits `mat` on the same edge;
  //   · the extraction it just started makes its FIRST read of `mat` in the
  //     NEXT cycle, through the combinational plane mux below — so the write
  //     whose dirty bit was discarded is already in the matrix being extracted;
  //   · every later write lands while the state is not S_IDLE, so `prep_start`
  //     is low, nothing is cleared, and the view is re-extracted afterwards.
  //
  // So the two orderings are equivalent TODAY, and the honest description of
  // this line is that it is defensive rather than load-bearing. It stays this
  // way round because the argument above rests entirely on the matrix being
  // read COMBINATIONALLY: latch `mat` into the extraction at `prep_start` —
  // which is the obvious move if that mux ever sits on the critical path — and
  // the clear-dominates form starts losing writes silently, in the direction
  // that deletes geometry. The cheap ordering is the one that survives that
  // change.
  logic cfg_mat_we;
  assign cfg_mat_we = cfg_we_i && (cfg_addr_i < 5'd16);

  logic prep_start;
  logic prep_start_view;
  assign prep_start = (state == S_IDLE) && (dirty != 2'b00);
  assign prep_start_view = ~dirty[0];  // view 0 first when both are dirty

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      dirty <= 2'b00;
    end else begin
      if (prep_start) dirty[prep_start_view] <= 1'b0;
      if (cfg_mat_we) dirty[cfg_view_i] <= 1'b1;
    end
  end

  // ---------------------------------------------------------------------------
  // ready: no dirty view, no work in flight, and no matrix write landing THIS
  // cycle. The last term is not belt-and-braces — a write and a tick in the
  // same cycle would evaluate the new matrix against the old length bounds,
  // which is precisely the stale-plane failure the dirty bit exists to prevent.
  // Writes to addr >= 16 are inert here (see the port comment) and do not gate.
  // ---------------------------------------------------------------------------
  assign ready_o = (state == S_IDLE) && (dirty == 2'b00) && !cfg_mat_we;

  integer ci;
  integer vi;
  integer pi;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      for (ci = 0; ci < 16; ci = ci + 1) begin
        mat[0][ci] <= '0;
        mat[1][ci] <= '0;
      end
      for (vi = 0; vi < 2; vi = vi + 1) begin
        for (pi = 0; pi < 5; pi = pi + 1) begin
          len_ceil[vi][pi] <= '0;
        end
      end
      state      <= S_IDLE;
      prep_view  <= 1'b0;
      prep_plane <= 3'd0;
      sq_num     <= '0;
      sq_res     <= '0;
      sq_bit     <= '0;
      sq_cnt     <= 6'd0;
      ev_active  <= 2'b00;
      ev_cx      <= '0;
      ev_cy      <= '0;
      ev_cz      <= '0;
      ev_r       <= '0;
      valid_o    <= 1'b0;
      vis_o      <= 2'b00;
      reject_o   <= 1'b0;
    end else begin
      valid_o <= 1'b0;

      // ---- configuration. The dirty bit itself lives in its own block above,
      // where set dominates clear. The viewport words GEOM.PROJECT accepts at
      // 16..17 are ignored here.
      if (cfg_mat_we) begin
        mat[cfg_view_i][cfg_addr_i[3:0]] <= $signed(cfg_data_i);
      end

      case (state)
        S_IDLE: begin
          // Extraction outranks evaluation, which is what makes a stale plane
          // unreachable rather than merely unlikely. The bit is cleared at the
          // START of a view's extraction, so a write landing DURING it sets the
          // bit again and the view is extracted a second time.
          if (prep_start) begin
            prep_view  <= prep_start_view;
            prep_plane <= 3'd0;
            state      <= S_SQ;
          end else if (tick_i && !cfg_mat_we) begin
            ev_cx     <= centre_x_i;
            ev_cy     <= centre_y_i;
            ev_cz     <= centre_z_i;
            ev_r      <= radius_i;
            ev_active <= active_i;
            state     <= S_EVAL;
          end
        end

        S_SQ: begin
          // The arm sums the three squares at its own pace and says when the
          // FULL sum exists — on that cycle only, hence sumsq_full rather than
          // a register.
          if (sq_done) begin
            sq_num <= sumsq_full;
            sq_res <= '0;
            sq_bit <= {1'b0, 1'b1, {64{1'b0}}};  // 4^32
            sq_cnt <= 6'd0;
            state  <= S_SQRT;
          end
        end

        S_SQRT: begin
          if (sq_ge) begin
            sq_num <= sq_num - sq_trial;
            sq_res <= (sq_res >> 1) + sq_bit;
          end else begin
            sq_res <= sq_res >> 1;
          end
          sq_bit <= sq_bit >> 2;
          sq_cnt <= sq_cnt + 6'd1;
          if (sq_cnt == 6'(SQRT_STEPS - 1)) state <= S_STORE;
        end

        S_STORE: begin
          // ceil = floor + (remainder != 0). The recurrence's own remainder is
          // n - res*res, so the perfect-square test is free.
          len_ceil[prep_view][prep_plane] <= sq_res[LEN_W-1:0] +
              {{(LEN_W - 1) {1'b0}}, (sq_num != '0)};
          if (prep_plane == 3'd4) begin
            state <= S_IDLE;
          end else begin
            prep_plane <= prep_plane + 3'd1;
            state      <= S_SQ;
          end
        end

        S_EVAL: begin
          // The arm walks the ten plane tests and says when the verdict is in.
          if (ev_done) begin
            vis_o    <= ev_vis;
            reject_o <= ev_reject;
            valid_o  <= 1'b1;
            state    <= S_IDLE;
          end
        end

        default: state <= S_IDLE;
      endcase
    end
  end

`ifdef FORMAL
  // THE DOMAIN IS ASSERTED, NOT ASSUMED.
  //
  // A negative radius is arithmetically well defined here and agrees with the
  // reference (both form a negative slack), but it is meaningless as a bound
  // and it makes rejection EASIER — the one direction that deletes geometry.
  // It fires rather than drifting.
  //
  // A configuration write while an evaluation is in flight would change the
  // matrix under the plane mux between one plane and the next, producing a
  // verdict from two different cameras. `ready_o` already refuses a tick in a
  // write cycle; this catches the caller that writes DURING the walk.
  // ENFORCED-BY: tests/differential/geom_cull_directed.cpp
  always_ff @(posedge clk) begin
    if (rst_n && tick_i && ready_o) begin
      a_domain_radius : assert (radius_i >= 32'sd0);
    end
    if (rst_n && (state == S_EVAL)) begin
      a_no_cfg_in_flight : assert (!cfg_mat_we);
    end
  end
`endif

endmodule

`default_nettype wire
