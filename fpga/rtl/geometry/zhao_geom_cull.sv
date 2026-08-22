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
// The extraction is fully sequential (3 squaring cycles + 33 recurrence steps +
// 1 store, times five planes = 185 cycles per view) because it is rare. The
// per-instance path is one plane per cycle, ten cycles per instance, and pays
// for THREE 33x32 multipliers plus one 32x34. The alternative — all five planes
// of both views at once — is thirty multipliers for a block whose consumers do
// not exist yet.
//
// THE NAMED NEXT LEVER, recorded so the shape is not mistaken for a floor: the
// three products of a plane dot could themselves be sequenced through one
// multiplier, taking the instance path from 10 cycles and 4 multipliers to 30
// cycles and 2. Whether that trade is right depends on the instance rate
// GEOM.MESHFETCH must sustain, which is not known — restructuring against a
// guess and then measuring is the wrong order. NOTHING HERE HAS BEEN FITTED;
// unlike `zhao_geom_lod` there is no Quartus number for this block yet, so
// every width below is argued from its range and not from a measurement.
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

module zhao_geom_cull (
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

  // LEN_W: the answer. sqrt(3*2^64) = 2^32*sqrt(3) < 2^33.8, so 34 bits, and
  // the ceiling adds at most one which cannot carry out of that.
  localparam int unsigned LEN_W = 34;

  // SQRT_STEPS: the trial bit walks 4^32, 4^31, ... 4^0 — 33 steps. Starting
  // ABOVE the argument's leading power of four is harmless: the compare fails
  // and the root shifts a zero, which is what the reference's `while (bit >
  // num) bit >>= 2` prologue does in software.
  localparam int unsigned SQRT_STEPS = 33;

  // DOT_W: three 33x32 products (each |.| <= 2^63) plus d << 16 (|.| <= 2^48),
  // so |dot| < 3*2^63 + 2^48 < 2^64.6. 68 bits leaves headroom over that for
  // ANY matrix word, not merely a plausible one; the slack is on ADDERS, never
  // on a multiplier operand, which is where width turns into DSPs.
  localparam int unsigned DOT_W = 68;

  // SLK_W: radius (< 2^31) times the length ceiling (< 2^34) is under 2^65.
  // 66 bits holds it signed.
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

  // extraction
  logic prep_view;
  logic [2:0] prep_plane;
  logic [1:0] sq_j;
  logic [SQ_W-1:0] sumsq;
  logic [SQ_W-1:0] sq_num, sq_res, sq_bit;
  logic [5:0] sq_cnt;

  // evaluation
  logic ev_view;
  logic [2:0] ev_plane;
  logic [1:0] ev_active;
  logic [1:0] ev_outside;
  logic signed [31:0] ev_cx, ev_cy, ev_cz, ev_r;

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
  // extraction: one square per cycle, then the recurrence
  // ---------------------------------------------------------------------------
  logic signed [PC_W-1:0] sq_operand;
  always_comb begin
    case (sq_j)
      2'd0:    sq_operand = pl_a;
      2'd1:    sq_operand = pl_b;
      default: sq_operand = pl_c;
    endcase
  end

  // A square is non-negative, so the signed product's sign bit is always clear
  // and reading it as unsigned is exact rather than a reinterpretation.
  logic signed [2*PC_W-1:0] sq_prod;
  assign sq_prod = $signed(sq_operand) * $signed(sq_operand);

  logic [SQ_W-1:0] sq_term;
  assign sq_term = SQ_W'($unsigned(sq_prod));

  logic [SQ_W-1:0] sumsq_next;
  assign sumsq_next = sumsq + sq_term;

  // one restoring step
  logic [SQ_W-1:0] sq_trial;
  logic sq_ge;
  assign sq_trial = sq_res + sq_bit;
  assign sq_ge = (sq_num >= sq_trial);

  // ---------------------------------------------------------------------------
  // evaluation: one plane per cycle
  // ---------------------------------------------------------------------------
  logic signed [DOT_W-1:0] dot;
  assign dot = ext_dot(mul_pc(pl_a, ev_cx)) + ext_dot(mul_pc(pl_b, ev_cy)) +
      ext_dot(mul_pc(pl_c, ev_cz)) + (ext_pc_d(pl_d) <<< 16);

  logic signed [DOT_W-1:0] slack;
  assign slack = ext_slack(mul_slack(ev_r, len_ceil[ev_view][ev_plane]));

  // wholly outside this plane
  logic outside_here;
  assign outside_here = (dot < -slack);

  // ---------------------------------------------------------------------------
  // the dirty bit, in its OWN block, because set must dominate clear
  // ---------------------------------------------------------------------------
  // A write and the start of that view's extraction can land in the same cycle.
  // If the clear won, the write would be swallowed and the block would go on
  // culling against a length bound belonging to the previous matrix — the exact
  // silent geometry-deleting failure this bit exists to prevent. Nonblocking
  // assignments resolve LAST-WINS, so the set is written second and the losing
  // case is unreachable rather than unlikely.
  // ENFORCED-BY: fpga/rtl/geometry/zhao_geom_cull.sv:dirty
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
      sq_j       <= 2'd0;
      sumsq      <= '0;
      sq_num     <= '0;
      sq_res     <= '0;
      sq_bit     <= '0;
      sq_cnt     <= 6'd0;
      ev_view    <= 1'b0;
      ev_plane   <= 3'd0;
      ev_active  <= 2'b00;
      ev_outside <= 2'b00;
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
            sq_j       <= 2'd0;
            sumsq      <= '0;
            state      <= S_SQ;
          end else if (tick_i && !cfg_mat_we) begin
            ev_cx      <= centre_x_i;
            ev_cy      <= centre_y_i;
            ev_cz      <= centre_z_i;
            ev_r       <= radius_i;
            ev_active  <= active_i;
            ev_view    <= 1'b0;
            ev_plane   <= 3'd0;
            ev_outside <= 2'b00;
            state      <= S_EVAL;
          end
        end

        S_SQ: begin
          sumsq <= sumsq_next;
          if (sq_j == 2'd2) begin
            // The recurrence starts from the FULL sum, which only exists on
            // this cycle — hence sumsq_next rather than the register.
            sq_num <= sumsq_next;
            sq_res <= '0;
            sq_bit <= {1'b0, 1'b1, {64{1'b0}}};  // 4^32
            sq_cnt <= 6'd0;
            state  <= S_SQRT;
          end else begin
            sq_j <= sq_j + 2'd1;
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
            sq_j       <= 2'd0;
            sumsq      <= '0;
            state      <= S_SQ;
          end
        end

        S_EVAL: begin
          if (outside_here) ev_outside[ev_view] <= 1'b1;
          if (ev_plane == 3'd4) begin
            ev_plane <= 3'd0;
            if (ev_view == 1'b1) begin
              // vis[v] = active[v] AND not outside[v]. `outside_here` is the
              // last plane of the last view and has not reached ev_outside yet,
              // so it is folded in here.
              vis_o <= {
                ev_active[1] & ~(ev_outside[1] | outside_here), ev_active[0] & ~ev_outside[0]
              };
              reject_o <= ~((ev_active[1] & ~(ev_outside[1] | outside_here)) |
                            (ev_active[0] & ~ev_outside[0]));
              valid_o <= 1'b1;
              state   <= S_IDLE;
            end else begin
              ev_view <= 1'b1;
            end
          end else begin
            ev_plane <= ev_plane + 3'd1;
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
  // write cycle; this catches the caller that writes DURING the ten.
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
