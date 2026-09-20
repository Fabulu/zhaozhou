// zhao_view_projq88.sv -- the per-camera PROJECTION SCALE in Q12.8, derived,
// under owner rulings R73, R83 and R98.
//
// ---------------------------------------------------------------------------
// THE CONTAINER IS Q12.8 IN 20 BITS, NOT Q8.8 IN 16 (R83, landed under R98)
// ---------------------------------------------------------------------------
// THE FILE NAME STILL SAYS q88 AND THAT IS DELIBERATE: the module name is what
// `design/blocks.yml`, `design/prod_manifest.yml`, `design/fit_targets.yml`,
// `tests/CMakeLists.txt` and the receipts all key on, and renaming a block to
// record a width change trades one wrong number for five stale references.
// The FORMAT is stated here, on the port, and in `PROJW`.
//
// R83 (2026-09-20) amended R73:
//
//   > the derivation stands, the container does not. `proj = kx*vw/2` gives
//   > 443.41 at 60 deg / 512 px, and the port is Q8.8, which caps at 255.996
//   > -- so it saturates below 90 deg hfov single-view and below 53.13 deg on
//   > Duo, which is an ordinary game camera. A saturated `proj` pegs the LOD
//   > ladder at its FINEST rung: maximum cost, no visible symptom, and nothing
//   > downstream can tell. Take 20-bit Q12.8 (G1's `STEPS` 33 to 37), NOT a
//   > unit rescale -- rescaling would move a quantity
//   > `zref::creature::projected_bound_radius_q8` already defines, which is
//   > the one thing R73 was chosen to avoid.
//
// R98 (2026-09-20) added that this block was BUILT THE SAME DAY, in the
// container R83 rejects, because the ruling was filed rather than routed to the
// live lane -- and that the widening therefore has to land in TWO ports: this
// one and `zhao_measure_governor`'s `proj0_i`/`proj1_i`. Both moved together.
//
//     viewport   90 deg   75 deg   60 deg   50 deg   saturates below
//     256 (Duo)  128.00   166.81   221.70   274.50       ~7 deg  (Q12.8)
//     512 (one)  256.00   333.63   443.41   548.99       ~7 deg  (Q12.8)
//
// Every cell in that table used to be a clamp except the four on the Duo row's
// left; none of them is now. The ceiling is 4095.996 px per unit tangent.
//
// THE SATURATION COUNTER STAYS, and so does the case that fires it. A counter
// whose state has become unreachable-in-practice is not a counter to delete:
// `kx` is a matrix magnitude bound, not an angle, and nothing structurally
// stops a caller presenting one that still clamps. `view_projq88_directed`
// case 3 drives it deliberately.
//
// ---------------------------------------------------------------------------
// THE RULING, AND WHY IT IS A DERIVATION AND NOT A WIRE
// ---------------------------------------------------------------------------
// R73 (2026-09-20), answering geomlod's D-A "the camera PROJECTION SCALE has
// no producer anywhere, and it is the last non-engineering blocker on
// MEASURE.GOVERNOR":
//
//   > DERIVE it, do not add an ABI field: `proj_Q8.8 = rhu(kx_raw *
//   > viewport_w / 512)` is exactly the NDC-to-pixel factor
//   > `zref::creature::projected_bound_radius_q8` already uses, so it
//   > introduces NO NEW LAW and nothing has to be ratified. This is the
//   > opposite case to R63, which put the eye on the wire precisely because
//   > recovering it would have meant an unratified 4x4 inverse -- here the
//   > quantity is already defined by an existing function and the wire would
//   > be a second statement of it.
//
// THE DIMENSIONAL CHECK, done here rather than taken on trust, because a
// formula quoted in a ruling is still a claim:
//
//   `kx` is the row-0 magnitude bound of the view-projection matrix, fx16
//   (Q16.16), so `kx_raw = kx * 65536`. It carries WORLD units to NDC.
//   NDC spans [-1, +1] across `viewport_w` pixels, so NDC -> pixels is
//   `viewport_w / 2`. Therefore
//
//       proj (pixels per world unit)  =  kx * viewport_w / 2
//       proj_Q8.8 = proj * 256        =  (kx_raw / 65536) * vw / 2 * 256
//                                     =  kx_raw * vw / 512
//
//   which is R73's expression exactly, with the 512 accounted for term by
//   term. `zhao_geom_projradius.sv:33-35` states the same factor from the
//   other side: the creature law "is S12.8 of `kx * R * viewport_w / (2 * w)`,
//   i.e. the same ratio multiplied by `kx * viewport_w / 2`, which converts
//   NDC to PIXELS."
//
// THIS IS NOT A SECOND IMPLEMENTATION OF `zhao_geom_projradius`. That block
// computes `kx * R * vw / (2w)` -- a projected RADIUS, for one creature, with
// the bound radius and the perspective `w` inside it. This block computes the
// CAMERA FACTOR alone, with no R and no w, which is a different quantity that
// that block never emits on a port. Checked deliberately, because
// `uncashed_cheques.py` check 3 exists for precisely the case where two blocks
// restate one law.
//
// ---------------------------------------------------------------------------
// WHERE THE INPUTS COME FROM
// ---------------------------------------------------------------------------
// `zhao_view_projscale` (fpga/rtl/common/) already snoops the projector's
// configuration bus and holds, per view, the row-0 magnitude `kx` and the
// viewport width `vw`. It is the producer of both inputs and this block adds
// no second snoop -- one listener on that bus is enough, and a second would be
// a second opinion about which address carries what.
//
// That bus is REAL and it has a REAL producer: `zhao_console_core.sv:8173`
// merges `cmd_exec_cfg_*_w` onto it, and `zhao_cmd_exec` lowers `SetView
// 0x0010`'s `mat4fx view_projection` onto cfg addresses 0..15 out of a packet
// CMD.DECODER has ratified (the I14 entry's "CLOSED: THE CAMERA"). The
// VIEWPORT RECT at cfg address 17 is the half of I14 that is still open, and
// it is the half this block's `vw` depends on -- named here so the dependency
// is not rediscovered.
//
// ---------------------------------------------------------------------------
// WHY IT IS SEQUENTIAL, AND WHY THAT COSTS NOTHING
// ---------------------------------------------------------------------------
// A 32x12 multiply inferred twice is DSP this device does not have spare (112
// on the 5CSEBA6U23I7, and the budget is the binding constraint together with
// ALMs). The value is CONFIGURATION -- it changes only when a SetView lands --
// so a shift-add costs nothing that matters. One free-running pass alternates
// the two views, twelve steps each, so a view's output is refreshed roughly
// every 26 clocks against a frame of 1,666,666. Nothing downstream can tell
// the difference, and the design keeps its multipliers.
//
// THE OPERANDS ARE LATCHED AT THE START OF A PASS. Reading `kx_i` and `vw_i`
// live across twelve steps would multiply the first half of one camera by the
// second half of another the moment a SetView lands mid-pass -- the
// "two operands that move together" failure with the operands moving APART.
// `busy_o` says a pass is in flight.
//
// ONE ROUNDING. `rhu(n/512)` is `spec/qformats.md` section 3's
// `round_half_up(n/d) = floor((n + floor(d/2))/d)` with d = 512, i.e.
// `(n + 256) >> 9`, and it is the only rounding in this file.
//
// SATURATION IS POSSIBLE AND IS COUNTED. `kx_raw * vw` reaches 2^44, and the
// Q12.8 port is 20 bits, so a camera with an extreme row-0 magnitude clamps.
// Unlike the governor's threshold rescale -- which its own header proves
// cannot saturate and therefore deliberately has no counter -- this one can,
// so it has one, and the directed test fires it.
//
// Conservative SystemVerilog subset only (charter section 2). No function-call
// result is indexed anywhere in this file (Quartus 17.0 rejects `f(x)[7:0]`).
`default_nettype none

module zhao_view_projq88 #(
    // Bits of the viewport width the shift-add walks. 12 matches
    // `zhao_view_projscale`'s `vw*_o` width and the projector's
    // "addr 17 : { h [27:16], w [11:0] }" layout. NAMED AND EDITABLE
    // (CLAUDE.md rule 6) so the block moves by parameter if that field does.
    parameter int unsigned VWW = 12,
    // Width of the projection-scale port, fraction bits fixed at 8. 20 is
    // Q12.8 under owner ruling R83; 16 is the Q8.8 container R83 rejects and
    // is kept reachable ONLY so the saturation table above can be reproduced.
    // NAMED AND EDITABLE (CLAUDE.md rule 6): the owner moves the container by
    // changing this and `zhao_measure_governor`'s matching `PROJW`, which are
    // the TWO ports R98 says must move together.
    parameter int unsigned PROJW = 20
) (
    input var logic clk,
    input var logic rst_n,

    // ---- from zhao_view_projscale, per view --------------------------------
    input var logic [31:0]    kx0_i,
    input var logic [31:0]    kx1_i,
    input var logic [VWW-1:0] vw0_i,
    input var logic [VWW-1:0] vw1_i,

    // ---- the camera projection scale, Q12.8, for MEASURE.GOVERNOR ----------
    // HELD. Each output is the last completed pass for that view and stands
    // until the next one completes. Q12.8 in PROJW=20 bits (R83/R98); it was
    // Q8.8 in 16 until 2026-09-20 and saturated on ordinary cameras.
    output var logic [PROJW-1:0] proj0_o,
    output var logic [PROJW-1:0] proj1_o,
    output var logic             busy_o,

    // ---- evidence ----------------------------------------------------------
    output var logic [31:0] passes0_o,
    output var logic [31:0] passes1_o,
    output var logic [31:0] saturations_o
);

  localparam logic [31:0] CNT_MAX = 32'hFFFF_FFFF;

  // kx is 32 bits and the shift reaches VWW-1, so the accumulator needs
  // 32 + VWW bits to hold `kx * (2^VWW - 1)` exactly, plus one for the
  // rounding bias. No term of this sum is ever truncated.
  localparam int unsigned ACCW = 32 + VWW + 1;

  // Quartus 17.0 needs an elaboration check inside `initial begin ... end`; a
  // bare module-scope `if` is a syntax error there however clean the lint
  // (CLAUDE.md, "Verilator lint-clean is not Quartus-synthesizable"). And
  // `--lint-only` does not run this block, so a clean lint is NOT evidence
  // about it -- it is fired by parameter override in the directed test.
  initial begin
    if (VWW < 1 || VWW > 16)
      $fatal(1, "zhao_view_projq88: VWW must be 1..16; the accumulator is sized from it");
    // PROJW must leave the saturation test a bit to look at, and must not
    // exceed the accumulator it selects from.
    if (PROJW < 8 || PROJW >= ACCW)
      $fatal(1, "zhao_view_projq88: PROJW must be 8..ACCW-1");
  end

  logic [ACCW-1:0]      acc_r;
  logic [ACCW-1:0]      kxsh_r;  // kx, shifted LEFT one place per step
  logic [VWW-1:0]       vwsh_r;  // vw, shifted RIGHT one place per step
  logic                 view_r;
  logic [4:0]           step_r;

  // A pass is in flight whenever the machine is past its latch cycle.
  assign busy_o = (step_r != 5'd0);

  // The shift-add walks vw's bits through position 0 and kx up past them, so
  // there is NO VARIABLE PART-SELECT anywhere in this file. That is deliberate
  // twice over: Quartus 17.0 is unhappy with indexed function results, and an
  // index wider than the vector it selects from is an out-of-range read: the
  // linter warns about it and synthesis resolves it silently.

  // The completed product, rounded once and clamped once.
  logic [ACCW-1:0]  biased_c;
  logic [ACCW-1:0]  shifted_c;
  logic             sat_c;
  logic [PROJW-1:0] result_c;
  always_comb begin
    biased_c  = acc_r + ACCW'(256);
    shifted_c = biased_c >> 9;
    sat_c     = |shifted_c[ACCW-1:PROJW];
    result_c  = sat_c ? {PROJW{1'b1}} : shifted_c[PROJW-1:0];
  end

  always_ff @(posedge clk) begin
    if (!rst_n) begin
      acc_r    <= ACCW'(0);
      kxsh_r   <= ACCW'(0);
      vwsh_r   <= VWW'(0);
      view_r   <= 1'b0;
      step_r   <= 5'd0;
      proj0_o  <= PROJW'(0);
      proj1_o  <= PROJW'(0);
      passes0_o     <= 32'd0;
      passes1_o     <= 32'd0;
      saturations_o <= 32'd0;
    end else if (step_r == 5'd0) begin
      // Latch this pass's operands. Everything after this reads the latch.
      kxsh_r <= view_r ? ACCW'({32'd0, kx1_i}) : ACCW'({32'd0, kx0_i});
      vwsh_r <= view_r ? vw1_i : vw0_i;
      acc_r  <= ACCW'(0);
      step_r <= 5'd1;
    end else if (step_r <= 5'(VWW)) begin
      if (vwsh_r[0]) acc_r <= acc_r + kxsh_r;
      kxsh_r <= kxsh_r << 1;
      vwsh_r <= vwsh_r >> 1;
      step_r <= step_r + 5'd1;
    end else begin
      // Commit. `view_r` still names the view this pass was for.
      if (view_r) begin
        proj1_o <= result_c;
        if (passes1_o != CNT_MAX) passes1_o <= passes1_o + 32'd1;
      end else begin
        proj0_o <= result_c;
        if (passes0_o != CNT_MAX) passes0_o <= passes0_o + 32'd1;
      end
      if (sat_c && saturations_o != CNT_MAX) saturations_o <= saturations_o + 32'd1;
      view_r <= ~view_r;
      step_r <= 5'd0;
    end
  end

endmodule

`default_nettype wire
