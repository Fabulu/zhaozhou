// zhao_project_core_mutant.sv -- A DELIBERATELY BROKEN COPY. NOT SHIPPED.
//
// This exists to make the MATW refusal law's two instruments -- the
// `mat_refused_o` counter and the MATW=18-vs-32 differential -- DEMONSTRATED
// rather than argued, by showing what the alternative looks like. The
// alternative is the naive narrowing: store the low MATW bits of whatever
// arrives and never say so. Its characteristic fault is not a wrong product
// but a PLAUSIBLE one: +2.0 (0x0002_0000) has the same low 18 bits as -2.0,
// so a `cot(fov/2)` one LSB over the ruled cap projects a mirrored, in-range
// picture with every handshake completing, every other counter balancing, and
// the fits-counter -- the instrument that ships -- reading zero because the
// check it reports on is gone.
//
// The one substantive change, in g_fits_chk:
//
//     assign cfg_fits = (cfg_data_i[31:MATW-1] == {(33 - MATW) {cfg_data_i[31]}});
//  -> assign cfg_fits = 1'b1;
//
// so no write is ever refused. On LEGAL content (every product word inside
// +-1.99998) this mutant is INDISTINGUISHABLE from the real core -- which is
// the weak-vector half of the demonstration: a differential that only ever
// drove in-range matrices would wave the broken guard through.
//
// INVERTED POLARITY: driven by tests/geometry/proj_matw_mutant_control.cpp
// against the REAL core at MATW=32; the control PASSES when (a) the streams
// agree on legal content, (b) a write of +2.0 leaves this mutant's counter at
// ZERO, and (c) the differential FAILS on the same content afterwards.
// Evidence about the instruments, not about the design.
//
// The module is RENAMED so a source-list mistake can never elaborate it in
// place of the real one, and it lives under tests/ where
// check_forbidden_sources.py will not find it in a production closure.
//
// REGENERATE IT if zhao_project_core.sv changes shape: this is a copy, and a
// copy of an old version is a positive control for a block that no longer
// exists. Generator: the one-line diff above applied to the shipped file.

// zhao_project_core.sv — the projection law, once.
//
// Contract: design/contracts/GEOM.PROJECT.md (Notes, "Follow-up"), and
//           design/contracts/TERRAIN.PROJECT.md.
// Reference: `zref::render::project_vertex`
//   (declared reference/src/zrender/internal.hpp, implemented
//    reference/src/zrender/rast.cpp:43).
//
// ---------------------------------------------------------------------------
// WHY THIS FILE EXISTS
// ---------------------------------------------------------------------------
// `zhao_geom_project` and `zhao_terrain_project` each contained a complete
// implementation of `project_vertex`. Not a similar one — the SAME one: the
// two localparams, all eight helper functions, the configuration register file
// and its decode, the three row sums, the near-plane verdict, the divider
// setup, the 31-stage restoring recurrence, the quotient assembly and the
// viewport `fx_mad` were byte-identical between the two files modulo
// whitespace. `zhao_geom_project`'s header said so and called it "A COST, NOT
// A FEATURE".
//
// It was measured before it was merged, and by two independent instruments:
//
//   * `docs/OWNER_DOCKET.md` 2026-08-24 — 11 nonconstant multiplies, widest
//     operand 32 bits, **33 mapped DSPs each**, identical arithmetic
//     signatures.
//   * RUN-20260824-0522's `pair_equivalence` differential — both blocks and
//     the shipped oracle driven from one stimulus stream, **12,300 projected
//     vertices compared three ways with zero mismatches**, across two views,
//     asymmetric viewports, the near-plane boundary, both guard-band rails,
//     rotating consumer stalls, and reconfiguration without reset.
//
// The census said the two had the same SHAPE. The differential is what said
// they had the same BEHAVIOUR, and only the second is a licence to merge.
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK IS, AND EXACTLY WHERE ITS BOUNDARY FALLS
// ---------------------------------------------------------------------------
// One vertex in, one projected vertex out, 36 clocks later, fully pipelined at
// one vertex per clock. The boundary is chosen so that BOTH callers keep the
// latency their contracts already state, to the cycle:
//
//   * `zhao_geom_project` — contract latency **fixed 36**. This core's output
//     register IS that block's output register. The caller adds no stage.
//   * `zhao_terrain_project` — contract latency **fixed 38**. This core's
//     output register is that block's `s6`, the caller adds a vertex
//     sequencer in front (1) and a triangle reassembly register behind (1).
//
// That is why the core ends at the `to_screen_xy` register and not one stage
// earlier or later. Ending earlier would have forced GEOM to add a stage;
// ending later would have forced TERRAIN to lose one. **The seam is placed by
// the two latency numbers, not by taste.**
//
// The pipeline is RIGID and its enable is the CALLER'S, taken as `en_i`: every
// stage advances together or none does, so a stalled consumer freezes the
// whole chain and nothing is dropped or reordered. This core does not derive
// its own stall condition, because the two callers back-pressure from
// different places — GEOM from this core's own output register, TERRAIN from
// the triangle register one stage further on. Deriving it here would have
// silently changed TERRAIN's handshake.
//
// `payload_i`/`payload_o` is OPAQUE. It rides the pipeline in lockstep with
// its vertex and this block never interprets it. GEOM sends a source id;
// TERRAIN sends a corner index, a source id and the layer-E Mosaic triple.
// `view_i` is NOT payload — it selects the matrix at stage 1 and the viewport
// at stage 6, so it is a first-class signal and is exposed again on the output
// because TERRAIN's packet carries it.
//
// ---------------------------------------------------------------------------
// THE LAW, step by step, each one cited
// ---------------------------------------------------------------------------
//   clip   = mat4_vec4(vp, {x, y, z, 1.0})   qformats §2 — EXACT s128 row sum,
//                                            then ONE rescale(.,16) per row
//   if (clip.w <= 0) -> behind the eye, and the vertex carries ZERO
//   ndc    = fx_div_exact(clip, clip.w)      §3 — one rounding, exact division
//   screen = fx_mad(ndc, half_extent, c)     §3 — one rounding
//   px     = to_screen_xy(screen)            §8 — rescale(.,8) then CLAMP ±2048
//   depth  = fx_div_exact(1.0, clip.w)       Q16.16 1/w (D7)
//
// Four consequences worth stating because each is a place an implementation
// drifts:
//
// 1. `v.w` IS THE CONSTANT 1.0, so matrix column 3 is a shift, not a multiply.
// 2. `clip.z` IS NEVER READ — the depth lane is 1/w, not z, and `ProjOut` has
//    no z field. Row 2 of the matrix is never computed. Nine multipliers, not
//    sixteen. The row-2 words remain writable so the register map stays a plain
//    sixteen-word block, and they are inert by construction.
//    ENFORCED-BY: tests/geometry/geom_project_directed.cpp:main
// 3. A BEHIND-THE-EYE VERTEX CARRIES ZERO AND IS NOT DROPPED. `project_vertex`
//    returns a default-constructed `ProjOut` whose screen vertex is {0,0,0}.
//    This core emits those zeros and raises `out_behind_o`. Dropping is
//    GEOM.CLIP's verdict; duplicating it here would make two counters disagree
//    about one primitive. `w == 0` exactly is the boundary and belongs on the
//    REJECT side — `<=`, not `<`.
// 4. THE GUARD BAND IS A CLAMP, NOT A CLIP. §8's `to_screen_xy` rescales to
//    S 12.8 and clamps to ±2048 px. GEOM.CLIP's header assumes every arriving
//    vertex is already inside that band; this core is what makes that true.
//
// ---------------------------------------------------------------------------
// THE DIVIDER
// ---------------------------------------------------------------------------
// `fx_div_exact` is an EXACT round-half-up division, not a reciprocal multiply.
// No reciprocal reproduces it bit-for-bit, so a real divider is required.
//
// The three quotients share the divisor `clip.w`, and `clip.w > 0` on every
// path that reaches the divider — the near plane already rejected the rest — so
// all three lanes are unsigned with a divisor that is never zero and never
// negative.
//
// The quotient needs 31 bits, not 48. With N = |a| << 16 (48 bits) and
// D = clip.w (31 bits, >= 1), `q >= 2^31` is exactly `h[47:31] >= D`, a 31-bit
// compare taken BEFORE any division; when it fires the answer is a rail and no
// division happens. When it does not fire, the same inequality is the invariant
// a restoring recurrence needs to START at bit 30 with remainder h[47:31]. One
// compare, then 31 restoring steps — never 48.
//
// ---------------------------------------------------------------------------
// TWO THINGS IN THE DIVIDER THAT NO TEST CAN EVER CATCH, AND WHY THEY STAY
// ---------------------------------------------------------------------------
// RUN-20260824-0522's mutation sweep left four survivors. Two were test gaps
// and were closed with tests. The other two are PROVABLE EQUIVALENTS — not
// "argued", not "we could not find a case": no stimulus can distinguish them,
// and here is why. Both are kept anyway, and the reason is the same in each
// case: they are what make the correctness ARGUMENT above sound, rather than
// making the block correct by coincidence.
//
// 1. `pre_sat` — the pre-division saturation compare — CHANGES NO OUTPUT.
//
//    When it fires, rem_0 = h[47:31] >= D. In the recurrence, if rem_k >= D
//    then t = 2*rem_k + b >= 2D > D, so the subtract fires and
//    rem_{k+1} = t - D >= D. By induction every one of the 31 steps subtracts
//    and emits a quotient bit of ONE, so `work` ends as 0x7FFF_FFFF and the
//    remainder ends nonzero. Therefore:
//      * positive lane: q_mag = 0x7FFF_FFFF, result INT32_MAX — the rail.
//      * negative lane: the `remainder != 0` carry makes q_mag 0x8000_0000, and
//        ~0x8000_0000 + 1 is 0x8000_0000 — INT32_MIN, also the rail.
//    **The overflowing recurrence saturates by itself, on both signs.** The
//    compare is kept because it is what makes the "rem < D at every step"
//    invariant TRUE, which is the premise the paragraph above reasons from. A
//    design that got the right answer only through this overflow coincidence
//    would be correct by accident, and the next person to touch the recurrence
//    would have no invariant to rely on.
//
// 2. FORCING `pre_d` TO 1 ON THE BEHIND-THE-EYE PATH CHANGES NO OUTPUT either,
//    and this file already said so ("a behind-the-eye vertex never uses its
//    quotients"). It is now confirmed rather than asserted: `out_x_o`,
//    `out_y_o` and `out_d_o` are all forced to zero when `s5_behind`, and
//    `pre_d`, `pre_d2` and `pre_sat` reach nothing else, so the whole divider's
//    output is discarded on that path. A divisor of 0 is also well defined in
//    this formulation — `t >= 0` is always true, so each step subtracts zero —
//    so there is no X to propagate and no simulation-versus-synthesis
//    divergence to hide behind. Kept for the same reason: the invariant is
//    stated as holding on EVERY cycle, not merely on the cycles that matter.
//
// ---------------------------------------------------------------------------
// COST, AND THE THREE LEVERS -- two are parameters of this file, one waits
// ---------------------------------------------------------------------------
// Eleven nonconstant multiplies: nine MATW x 32 row products (three rows of
// three; column 3 is a shift because v.w is 1.0) and two 32x27 `fx_mad`
// products. `tools/budget/calibration.json` (this tool, this device) prices a
// signed product at 1 DSP with BOTH operands 8..27 bits and **3** with either
// operand at 28..33 -- and on the asymmetric rows that decide this file:
// **32x18 = 2**, 32x19 = 4 (worse than 32x32), 32x20 .. 32x32 = 3. At the
// default MATW=32 that is 11 x 3 = **33 DSPs**, and the map agrees exactly.
//
// ROWS_PER_PASS (2026-09-09) is dsp.md's lever 1, now a PARAMETER of this
// file rather than a deliberate omission. At 3 (the default, and what every
// existing caller gets) nothing changes: eleven sites, 33 DSP, one vertex per
// clock. At 1 the three matrix rows share three multiplier sites sequenced
// over three cycles: five sites, 5 x 3 = **15 DSP** by the same calibration
// line, one vertex per THREE clocks, latency +3. dsp.md's "~33 to ~12"
// predates the calibration cliff and forgot the two viewport products; the
// honest number at this setting is 15. The sequencer's operand routing is
// bought with REGISTERS, not with muxes in the multiplier cone -- see the
// g_rows_seq header below and reports/PROJECT-CORE-ROW-MULTIPLEX-20260909.md.
//
// MATW (2026-09-10) is the operand-width lever, and it is a parameter of this
// file because of OWNER RULING R1 -- Fabian, 2026-09-09: "minimum fov you
// propose is ok" (reports/OWNER-RULINGS-20260909-2300.md;
// reports/OWNER-QUESTION-FOV-FLOOR-20260909.md is the question answered). A
// vertical field-of-view floor a shade above 53.13 degrees is ACCEPTED, which
// caps the view-projection matrix's perspective coefficients at +-2.0 and
// lets the nine PRODUCT words be carried as signed 18-bit Q16.16
// (representable +-1.99998; -2.0 exactly fits, +2.0 exactly does not).
// `m11 = cot(fov/2)` is the binding coefficient and aspect only ever REDUCES
// `m00`, so one floor covers all three video modes. **VERTICES ARE UNTOUCHED**
// -- full-width s32, +-32,768 world units; this was never a world-size
// question. And the bound assumes what a view-projection matrix normally is:
// a RIGID view (rotation + translation, no scale, no shear) times a
// SYMMETRIC frustum. A world-unit scale or an off-axis frustum folded into
// the matrix moves coefficients independently of the field of view, and the
// refusal law below is what makes that a counted fault rather than a picture.
//
//   * Legal values: 32 (default -- nothing ships differently until the
//     projection-subsystem fit gate passes) and 18. Elaboration $fatal on
//     anything else, DELIBERATELY: 19..27 are measured to buy nothing, and 19
//     is measured to cost MORE than 32. "One bit of headroom" is the trap.
//   * At MATW=32 the block is bit- and cycle-identical to the pre-parameter
//     file: every function below degenerates to the shape it had, ROW_W is
//     the same 68, the fits-check is the constant 1. Proven by the existing
//     suites passing unchanged and by proj_rowmux_directed still showing
//     RPP=1 byte-identical to RPP=3.
//   * At MATW=18 the nine product words enter the multiplier at 18 bits, the
//     stored registers' bits above 17 have no reader and are removed by
//     synthesis (9 x 2 x 14 = 252 flops; STRUCTURAL until the map gate reads
//     the register count), and ROW_W shrinks to 54.
//
// THE REFUSAL LAW at MATW < 32. A configuration write to one of the nine
// product words -- columns 0..2 of rows 0, 1 and 3 -- whose value does not
// fit a signed MATW-bit word is REFUSED: the register KEEPS its previous
// value and `mat_refused_o` counts the write. Never a clamp: a clamped
// `cot(fov/2)` projects a plausible, wrong picture with nothing to say so,
// which is exactly the silent drift this repository forbids ("deterministic
// refusal, counted, never a silent clamp"). Refusal leaves the previous
// matrix in force -- a picture that is stale rather than wrong -- and a
// counter that names the fault. The translation column (words 3, 7, 15) never
// enters a multiplier and keeps the full 32 bits at every MATW; the row-2
// words (8..11) are inert and stay writable, unchecked, so the register map
// remains a plain sixteen-word block. At MATW=32 every word fits and the
// counter is structurally zero; it is SEEN TO FIRE at MATW=18 by
// tests/geometry/proj_matw_directed.cpp, and the committed mutant
// tests/mutants/zhao_project_core_mutant.sv shows what its absence looks like
// (a silent wrap of +2.0 to -2.0 that only the differential can see).
//
// THE DSP LATTICE, re-derived 2026-09-10 from the calibration rows rather
// than inherited. The three levers act on the same sites and COMPOSE
// MULTIPLICATIVELY; nothing here adds.
//
//     two instances, RPP=3, MATW=32    2 x (9x3 + 2x3) = 66   measured (twice)
//     shared service, RPP=3, MATW=32         9x3 + 2x3 = 33   measured
//     shared,         RPP=1, MATW=32         3x3 + 2x3 = 15   structural
//     shared,         RPP=1, MATW=18         3x2 + 2x3 = 12   structural
//
//   So MATW=18 is worth -18 taken ALONE on two spatial instances (66 -> 48),
//   -9 on the shared spatial service (33 -> 24), and **-3** after
//   ROWS_PER_PASS=1 (15 -> 12). The rulings docket carries "-10 marginal,
//   66 -> 33 -> 15 -> 5"; 15 -> 5 would need every one of the five sites to
//   fall from 3 DSP to 1, which the calibration grants only when BOTH operands
//   are <= 27 bits -- i.e. narrowing the VERTICES, which R1 does not do. The
//   two `fx_mad` sites (ndc x viewport) never touch the matrix and stay at 3
//   each at any MATW. The one fit gate that turns the two structural rows
//   into measurements is named in reports/PROJECT-CORE-MATW18-20260910.md.
//
// The original 2026-08-24 text, kept because its reasoning is still the right
// shape and only its premise moved: "At <= 27 bits the same eleven products
// cost 11. What that needs is a PROOF that 27 bits covers a world coordinate,
// which is a question about map size and the fixed-point format and belongs
// to the owner, not to this file." Two things have moved since: 27 bits on
// ONE side buys nothing (32x27 = 3, measured), and 18 bits on the MATRIX side
// is not a world-size question at all. The proof it asked for is still owed at
// the other two places it named -- the vertex operand and the post-division
// `ndc` operand of `fx_mad` -- and neither is touched here.
//
// The projected-vertex cache is not here either. TERRAIN.PROJECT projects
// triangle corners, so a 33x33 patch performs 6,144 projections for 1,089
// unique lattice vertices. `zhao_terrain_project.sv`'s own header records this
// and names `GEOM.WCACHE` as the owner. It is a separate block, not a
// parameter of this one.
//
// Conservative SystemVerilog subset only (charter §2); no package deps.

module zhao_project_core_mutant #(
    // Opaque per-vertex rider, carried in lockstep and never interpreted.
    parameter int unsigned PAYLOAD_W = 16,
    // Matrix rows computed per cycle. 3 = all rows spatially (nine mul32
    // sites, one vertex per clock — the historical shape, and cycle-identical
    // to the pre-parameter block). 1 = one row per cycle through three shared
    // sites (one vertex per three clocks, latency +3, `in_ready_o` gates
    // acceptance). Legal values are 3 and 1 only; elaboration $fatal otherwise.
    parameter int unsigned ROWS_PER_PASS = 3,
    // Width of the nine matrix PRODUCT words as they enter the multiplier.
    // 32 = the historical full-width operand (default; nothing ships
    // differently). 18 = owner ruling R1's +-2.0 coefficient cap, signed
    // Q16.16 -- the only other width the calibration says pays (32x18 = 2 DSP
    // against 3; 32x19 = 4, WORSE than 32). Legal values are 32 and 18 only;
    // elaboration $fatal otherwise. See the MATW header section.
    parameter int unsigned MATW = 32
) (
    input logic clk,
    input logic rst_n,

    // ---- configuration: two views, sixteen matrix words + a viewport each ---
    // addr 0..15  : matrix row-major m[0..15] (row 2, words 8..11, inert)
    // addr 16     : { y0[27:16], x0[11:0] }
    // addr 17     : { h [27:16], w [11:0] }
    input logic        cfg_we_i,
    input logic        cfg_view_i,
    input logic [ 4:0] cfg_addr_i,
    input logic [31:0] cfg_data_i,

    // ---- the rigid-pipeline enable, owned by the caller ---------------------
    // Every stage advances together or none does. The caller derives this from
    // wherever ITS back-pressure boundary is; see the header.
    input logic en_i,

    // ---- one vertex in, sampled on `en_i` -----------------------------------
    // A vertex is ACCEPTED on a cycle where `en_i && in_valid_i && in_ready_o`.
    // At ROWS_PER_PASS=3, `in_ready_o` is the constant 1 and this is exactly
    // the historical contract (every caller predating the port may ignore it).
    // At ROWS_PER_PASS=1 it is high only in the row sequencer's capture slots
    // — once per three `en_i`-cycles under saturation — and a caller that
    // ignores it loses vertices.
    output logic                    in_ready_o,
    input logic                     in_valid_i,
    input logic signed [31:0]       vx_i,
    input logic signed [31:0]       vy_i,
    input logic signed [31:0]       vz_i,
    input logic                     view_i,
    input logic [PAYLOAD_W-1:0]     payload_i,

    // ---- one projected vertex out, 36 clocks later --------------------------
    output logic                    out_valid_o,
    output logic signed [20:0]      out_x_o,      // S 12.8 canvas x, ±2048 px
    output logic signed [20:0]      out_y_o,      // S 12.8 canvas y, ±2048 px
    output logic signed [31:0]      out_d_o,      // Q16.16 1/w
    // clip.w itself, fx16 raw, aligned with out_d_o. GEOM.DEPTHQUANT needs w
    // and NOT the quotient: the ratified depth law performs its own rcp_u24 on
    // w, and 1/w has already lost the precision that reconstruction would need.
    // It is nearly free here -- the divider carries the divisor through every
    // step because each step subtracts it -- so this is one register at s5 and
    // one at the output, not a four-stage carry.
    //
    // Zero when out_behind_o, matching out_d_o's convention. Behind the eye the
    // internal divisor is forced to 1 rather than the real w, so a consumer
    // that ignored out_behind_o would read a plausible near value; zeroing here
    // makes that misuse produce the far floor instead of the near plane.
    output logic        [30:0]       out_w_o,
    output logic                    out_behind_o, // clip.w <= 0: vertex is zero
    output logic                    out_view_o,
    output logic [PAYLOAD_W-1:0]    out_payload_o,

    // Any vertex anywhere in the pipe, output register included. A caller with
    // its own stages ANDs its own emptiness with this.
    output logic                    busy_o,

    // Configuration writes REFUSED because the value did not fit a signed
    // MATW-bit product word (the refusal law in the header). Saturating.
    // Structurally zero at MATW=32; seen to fire at MATW=18.
    output logic [31:0]             mat_refused_o
);

  // ---------------------------------------------------------------------------
  // widths, stated rather than assumed
  // ---------------------------------------------------------------------------
  // MUL_W: one MATW x s32 product, exact. 64 at MATW=32, 50 at MATW=18.
  localparam int unsigned MUL_W = MATW + 32;
  // ROW_W: a row sum is three MATW x s32 products (each |.| <= 2^(MATW+30))
  // plus m[i][3] << 16 (|.| <= 2^47), so |sum| < 3*2^(MATW+30) + 2^47 <
  // 2^(MATW+32) for every MATW >= 16. MATW + 36 leaves at least three bits
  // over that and cannot wrap for ANY input word, not merely legal ones. At
  // MATW=32 this is the historical 68; at 18 it is 54.
  localparam int unsigned ROW_W = MATW + 36;
  // The rescale(.,16) constants at ROW_W, typed once so the function below
  // is width-generic without a size cast (Quartus 17 is conservative there).
  localparam logic signed [ROW_W-1:0] ROW_HALF = 32768;
  localparam logic signed [ROW_W-1:0] ROW_MAX  = 2147483647;
  localparam logic signed [ROW_W-1:0] ROW_MIN  = -ROW_MAX - 1;
  // MAD_W: |ndc| <= 2^31 and hw = w*2^15 < 2^27, so the product is < 2^58 and
  // the addend (c << 32) is < 2^45. 64 bits clears both.
  localparam int unsigned MAD_W = 64;
  // DIV_STEPS: 31 quotient bits, once the saturation compare has ruled out
  // q >= 2^31.
  localparam int unsigned DIV_STEPS = 31;

  // ---------------------------------------------------------------------------
  // the pure functions, all of them views onto zref_fixp.hpp
  // ---------------------------------------------------------------------------
  // The row product: a MATW-bit matrix word times a full s32 coordinate,
  // exact in MUL_W bits. At MATW=32 this is the historical `mul32`, character
  // for character in its result. Both operands are sign-extended to the
  // product width, the same shape the calibration measured.
  function automatic logic signed [MUL_W-1:0] mulm(input logic signed [MATW-1:0] a,
                                                  input logic signed [31:0]     b);
    mulm = $signed({{32{a[MATW-1]}}, a}) * $signed({{MATW{b[31]}}, b});
  endfunction

  function automatic logic signed [ROW_W-1:0] extp(input logic signed [MUL_W-1:0] v);
    extp = $signed({{(ROW_W - MUL_W) {v[MUL_W-1]}}, v});
  endfunction

  // The product word as the multiplier sees it. At MATW=32 the whole word; at
  // 18 the low 18 bits -- which, because the refusal law admits only values
  // that fit, IS the whole word's value. The register bits above have no
  // reader and are removed by synthesis: the storage narrows with the operand.
  // At MATW < 32 the upper bits of `v` are unread BY DESIGN -- that is the
  // whole point of the function -- so the lint warning is silenced here and
  // nowhere else.
  /* verilator lint_off UNUSEDSIGNAL */
  function automatic logic signed [MATW-1:0] mw(input logic signed [31:0] v);
    mw = v[MATW-1:0];
  endfunction
  /* verilator lint_on UNUSEDSIGNAL */

  function automatic logic signed [ROW_W-1:0] ext32r(input logic signed [31:0] v);
    ext32r = $signed({{(ROW_W - 32) {v[31]}}, v});
  endfunction

  function automatic logic signed [MAD_W-1:0] ext32m(input logic signed [31:0] v);
    ext32m = $signed({{(MAD_W - 32) {v[31]}}, v});
  endfunction

  // rescale(x, 16): round-half-up shift then saturating narrow to the fx16 word
  // (§4). The shift is arithmetic, so it floors — which is what makes
  // (x + 2^15) >>> 16 round half UP rather than toward zero.
  function automatic logic signed [31:0] rescale16_row(input logic signed [ROW_W-1:0] x);
    logic signed [ROW_W-1:0] r;
    begin
      r = (x + ROW_HALF) >>> 16;
      if (r > ROW_MAX) rescale16_row = 32'sh7FFF_FFFF;
      else if (r < ROW_MIN) rescale16_row = 32'sh8000_0000;
      else rescale16_row = r[31:0];
    end
  endfunction

  function automatic logic signed [31:0] rescale16_mad(input logic signed [MAD_W-1:0] x);
    logic signed [MAD_W-1:0] r;
    begin
      r = (x + 64'sd32768) >>> 16;
      if (r > 64'sd2147483647) rescale16_mad = 32'sh7FFF_FFFF;
      else if (r < -64'sd2147483648) rescale16_mad = 32'sh8000_0000;
      else rescale16_mad = r[31:0];
    end
  endfunction

  // §8 to_screen_xy: rescale(.,8) — |x| <= 2^31, so the shift lands inside 24
  // bits and the fx16 saturating narrow cannot fire — then CLAMP to the guard
  // band. The clamp is the law; it is not a clip and it is not optional.
  function automatic logic signed [20:0] to_screen_xy(input logic signed [31:0] x);
    logic signed [40:0] r;
    begin
      r = ($signed({{9{x[31]}}, x}) + 41'sd128) >>> 8;
      if (r > 41'sd524288) to_screen_xy = 21'sd524288;
      else if (r < -41'sd524288) to_screen_xy = -21'sd524288;
      else to_screen_xy = r[20:0];
    end
  endfunction

  // |v| as an unsigned 32-bit word. INT32_MIN maps to 0x8000_0000 = 2^31, which
  // is exactly right unsigned — the one place ~v + 1 is not a bug.
  function automatic logic [31:0] mag32(input logic signed [31:0] v);
    mag32 = v[31] ? (~$unsigned(v) + 32'd1) : $unsigned(v);
  endfunction

  // ---------------------------------------------------------------------------
  // configuration registers
  // ---------------------------------------------------------------------------
  // The sixteen matrix words are stored at the full register-map width. The
  // nine PRODUCT words are read through `mw()` at MATW bits (see there); the
  // translation column and the inert row 2 are read at 32 or not at all.
  logic signed [31:0] mat  [0:1][0:15];
  logic        [11:0] vp_x0[0:1];
  logic        [11:0] vp_y0[0:1];
  logic        [11:0] vp_w [0:1];
  logic        [11:0] vp_h [0:1];

  // ---- the refusal law (header, "THE REFUSAL LAW") ------------------------
  // A product word is any matrix address in columns 0..2 of rows 0, 1 or 3.
  // Column 3 (words 3, 7, 11, 15) is the shifted translation column; row 2
  // (words 8..11) is inert. Neither reaches a multiplier and neither is
  // checked.
  logic cfg_is_prod;
  logic cfg_fits;
  assign cfg_is_prod = (cfg_addr_i < 5'd16) && (cfg_addr_i[1:0] != 2'd3) &&
                       (cfg_addr_i[3:2] != 2'd2);
  generate
    if (MATW == 32) begin : g_fits_all
      // Every 32-bit word fits a 32-bit word. The counter below is
      // structurally zero at this setting, and says so in the header.
      assign cfg_fits = 1'b1;
    end else begin : g_fits_chk
      // Fits signed MATW iff bits [31:MATW-1] are all the sign bit. This is
      // the exact test, not a magnitude compare: -2^(MATW-1) fits, +2^(MATW-1)
      // does not, and both land on the boundary this reads.
      // MUTANT: the fits-check is REMOVED (the one substantive change). The
      // real line is
      //   assign cfg_fits = (cfg_data_i[31:MATW-1] == {(33 - MATW) {cfg_data_i[31]}});
      // With it gone, every write is accepted, the counter can never move,
      // and the multiplier reads the low MATW bits of whatever arrived:
      // +2.0 (0x0002_0000) becomes -2.0 in silence.
      assign cfg_fits = 1'b1;
    end
  endgenerate

  // Refused writes, counted. Not gated by `en_i`: configuration writes are
  // not, either. Saturating, like every other counter in this subsystem.
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      mat_refused_o <= '0;
    end else if (cfg_we_i && cfg_is_prod && !cfg_fits &&
                 mat_refused_o != 32'hFFFF_FFFF) begin
      mat_refused_o <= mat_refused_o + 32'd1;
    end
  end

  integer ci;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      for (ci = 0; ci < 16; ci = ci + 1) begin
        mat[0][ci] <= '0;
        mat[1][ci] <= '0;
      end
      vp_x0[0] <= '0; vp_x0[1] <= '0;
      vp_y0[0] <= '0; vp_y0[1] <= '0;
      vp_w[0]  <= '0; vp_w[1]  <= '0;
      vp_h[0]  <= '0; vp_h[1]  <= '0;
    end else if (cfg_we_i) begin
      if (cfg_addr_i < 5'd16) begin
        // A refused product word KEEPS its previous value -- the register is
        // simply not written. Every other matrix word is written as before.
        if (!cfg_is_prod || cfg_fits) begin
          mat[cfg_view_i][cfg_addr_i[3:0]] <= $signed(cfg_data_i);
        end
      end else if (cfg_addr_i == 5'd16) begin
        vp_x0[cfg_view_i] <= cfg_data_i[11:0];
        vp_y0[cfg_view_i] <= cfg_data_i[27:16];
      end else if (cfg_addr_i == 5'd17) begin
        vp_w[cfg_view_i] <= cfg_data_i[11:0];
        vp_h[cfg_view_i] <= cfg_data_i[27:16];
      end
    end
  end

  // ---------------------------------------------------------------------------
  // stage 1 — §2 mat4_vec4: three EXACT row sums, spatial or sequenced
  // ---------------------------------------------------------------------------
  // ROWS_PER_PASS picks the shape; the ARITHMETIC is one expression either
  // way, and the two generate branches below are asserted byte-identical by
  // tests/geometry/proj_rowmux_directed.cpp across every stall pattern,
  // including a configuration write landing mid-sequence.
  initial begin
    if (ROWS_PER_PASS != 3 && ROWS_PER_PASS != 1)
      $fatal(1, "zhao_project_core_mutant: ROWS_PER_PASS (%0d) must be 3 or 1",
             ROWS_PER_PASS);
    // 19..27 buy nothing and 19 costs MORE than 32 (calibration.json,
    // asymmetric rows). Only the two measured-to-pay widths are legal.
    if (MATW != 32 && MATW != 18)
      $fatal(1, "zhao_project_core_mutant: MATW (%0d) must be 32 or 18", MATW);
  end

  logic                        s1_valid;
  logic signed [ROW_W-1:0]     s1_rx, s1_ry, s1_rw;
  logic                        s1_view;
  logic        [PAYLOAD_W-1:0] s1_pay;
  // A vertex captured by the row sequencer but not yet launched into s1.
  // Constant 0 at ROWS_PER_PASS=3, where no such holding state exists. Joins
  // busy_o's reduction: a block that reports idle while its sequencer holds a
  // vertex is the queue-occupancy defect busy_o's own comment warns about.
  logic                        seq_holds;

  generate
    if (ROWS_PER_PASS == 3) begin : g_rows_spatial
      // ------------------------------------------------------------------------
      // nine products, three row sums, one cycle — the historical stage 1,
      // verbatim. `in_ready_o` is constant so a caller predating the port
      // sees exactly the old behaviour, cycle for cycle.
      // ------------------------------------------------------------------------
      logic signed [ROW_W-1:0] row_x, row_y, row_cw;
      always_comb begin
        row_x = extp(mulm(mw(mat[view_i][0]), vx_i)) + extp(mulm(mw(mat[view_i][1]), vy_i)) +
            extp(mulm(mw(mat[view_i][2]), vz_i)) + (ext32r(mat[view_i][3]) <<< 16);
        row_y = extp(mulm(mw(mat[view_i][4]), vx_i)) + extp(mulm(mw(mat[view_i][5]), vy_i)) +
            extp(mulm(mw(mat[view_i][6]), vz_i)) + (ext32r(mat[view_i][7]) <<< 16);
        row_cw = extp(mulm(mw(mat[view_i][12]), vx_i)) + extp(mulm(mw(mat[view_i][13]), vy_i)) +
            extp(mulm(mw(mat[view_i][14]), vz_i)) + (ext32r(mat[view_i][15]) <<< 16);
      end

      assign in_ready_o = 1'b1;
      assign seq_holds  = 1'b0;

      always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
          s1_valid <= 1'b0; s1_rx <= '0; s1_ry <= '0; s1_rw <= '0;
          s1_view <= 1'b0; s1_pay <= '0;
        end else if (en_i) begin
          s1_valid <= in_valid_i;
          s1_rx    <= row_x;
          s1_ry    <= row_y;
          s1_rw    <= row_cw;
          s1_view  <= view_i;
          s1_pay   <= payload_i;
        end
      end
    end else begin : g_rows_seq
      // ------------------------------------------------------------------------
      // three products, one row sum per cycle — dsp.md lever 1
      // ------------------------------------------------------------------------
      // THE MULTIPLIER CONE MUST NOT GROW. This block already misses the
      // product clock on exactly this cone (73.62 MHz after the stage-5b cut;
      // reports/PROJECT-CORE-CLOCK-20260907.md names `mat -> view mux -> Mult0
      // -> row adder -> s1` as the standing worst path), so the sequencer may
      // not put a phase mux in front of the DSPs — that is the census's
      // "control depth" objection, and this structure is its answer.
      //
      // Operand routing is bought with REGISTERS instead: on the accept edge
      // the three rows' matrix words are CAPTURED into a shifting hold bank
      // (the 2:1 view mux happens once, on the capture path, a mat-register to
      // hold-register hop with no arithmetic behind it), and each cycle the
      // bank shifts the next row into the multiplier position. The multiplier
      // cone is `ha -> mul32 -> row adder -> s1` — one view-mux SHORTER than
      // the spatial branch's, at the price of +3 cycles of latency and an
      // initiation interval of 3, which is what the composed frame budget has
      // headroom for (23.9% at II=1; see the row-multiplex report).
      //
      // Capture-at-accept also means a configuration write landing between a
      // vertex's row cycles cannot tear its transform: at either parameter
      // setting a vertex reads its matrix exactly once, on its accept edge.
      // The FSM's W cycle both launches the finished vertex into s1 and may
      // capture the next one, so the initiation interval is 3, not 4.
      localparam logic [1:0] SeqIdle = 2'd0;  // empty; may capture
      localparam logic [1:0] SeqMx   = 2'd1;  // multiplier on row X
      localparam logic [1:0] SeqMy   = 2'd2;  // multiplier on row Y
      localparam logic [1:0] SeqMw   = 2'd3;  // row W; launch; may capture

      logic        [1:0]           st;
      logic signed [31:0]          ha[0:3];  // the row under the multiplier NOW
      logic signed [31:0]          hb[0:3];  // the next row
      logic signed [31:0]          hc[0:3];  // the row after
      logic signed [31:0]          hvx, hvy, hvz;
      logic                        hview;
      logic        [PAYLOAD_W-1:0] hpay;

      // Identical expression shape to the spatial row_x — same functions, same
      // term order, same widths — with `mat[view_i][.]` / `v*_i` renamed to
      // the held copies. That identity is what makes the byte-identity claim
      // an argument as well as a measurement.
      logic signed [ROW_W-1:0] row_seq;
      always_comb begin
        row_seq = extp(mulm(mw(ha[0]), hvx)) + extp(mulm(mw(ha[1]), hvy)) +
            extp(mulm(mw(ha[2]), hvz)) + (ext32r(ha[3]) <<< 16);
      end

      assign in_ready_o = (st == SeqIdle) || (st == SeqMw);
      assign seq_holds  = (st != SeqIdle);

      integer hi;
      always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
          st <= SeqIdle;
          for (hi = 0; hi < 4; hi = hi + 1) begin
            ha[hi] <= '0;
            hb[hi] <= '0;
            hc[hi] <= '0;
          end
          hvx <= '0; hvy <= '0; hvz <= '0;
          hview <= 1'b0; hpay <= '0;
          s1_valid <= 1'b0; s1_rx <= '0; s1_ry <= '0; s1_rw <= '0;
          s1_view <= 1'b0; s1_pay <= '0;
        end else if (en_i) begin
          case (st)
            SeqIdle: begin
              s1_valid <= 1'b0;
              if (in_valid_i) begin
                for (hi = 0; hi < 4; hi = hi + 1) begin
                  ha[hi] <= mat[view_i][hi];
                  hb[hi] <= mat[view_i][4 + hi];
                  hc[hi] <= mat[view_i][12 + hi];
                end
                hvx <= vx_i; hvy <= vy_i; hvz <= vz_i;
                hview <= view_i; hpay <= payload_i;
                st <= SeqMx;
              end
            end
            SeqMx: begin
              s1_valid <= 1'b0;
              s1_rx    <= row_seq;
              for (hi = 0; hi < 4; hi = hi + 1) begin
                ha[hi] <= hb[hi];
                hb[hi] <= hc[hi];
              end
              st <= SeqMy;
            end
            SeqMy: begin
              s1_valid <= 1'b0;
              s1_ry    <= row_seq;
              for (hi = 0; hi < 4; hi = hi + 1) ha[hi] <= hb[hi];
              st <= SeqMw;
            end
            default: begin  // SeqMw: launch, and capture in the same cycle
              s1_rw    <= row_seq;  // reads the OLD ha — nonblocking, read-old
              s1_view  <= hview;
              s1_pay   <= hpay;
              s1_valid <= 1'b1;
              if (in_valid_i) begin
                for (hi = 0; hi < 4; hi = hi + 1) begin
                  ha[hi] <= mat[view_i][hi];
                  hb[hi] <= mat[view_i][4 + hi];
                  hc[hi] <= mat[view_i][12 + hi];
                end
                hvx <= vx_i; hvy <= vy_i; hvz <= vz_i;
                hview <= view_i; hpay <= payload_i;
                st <= SeqMx;
              end else begin
                st <= SeqIdle;
              end
            end
          endcase
        end
      end
    end
  endgenerate

  // ---------------------------------------------------------------------------
  // stage 2 — §2's ONE rescale per row, and the near-plane verdict
  // ---------------------------------------------------------------------------
  logic                        s2_valid;
  logic signed [31:0]          s2_cx, s2_cy, s2_cw;
  logic                        s2_view;
  logic        [PAYLOAD_W-1:0] s2_pay;

  // ---------------------------------------------------------------------------
  // stage 3 — the divider setup
  // ---------------------------------------------------------------------------
  logic [47:0] pre_n [0:2];
  logic [47:0] pre_h [0:2];
  logic [30:0] pre_d;
  logic [29:0] pre_d2;
  logic [ 2:0] pre_neg;
  logic [ 2:0] pre_sat;
  logic        pre_behind;
  integer      li;

  always_comb begin
    pre_behind = (s2_cw <= 32'sd0);
    // A behind-the-eye vertex never uses its quotients, but the divisor must
    // still be legal: forcing 1 keeps the recurrence's rem < D invariant true on
    // every cycle instead of only on the cycles that matter.
    pre_d  = pre_behind ? 31'd1 : s2_cw[30:0];
    pre_d2 = pre_d[30:1];

    pre_neg[0] = !pre_behind && s2_cx[31];
    pre_neg[1] = !pre_behind && s2_cy[31];
    pre_neg[2] = 1'b0;  // the 1/w lane's numerator is the constant +1.0

    pre_n[0] = {mag32(s2_cx), 16'b0};
    pre_n[1] = {mag32(s2_cy), 16'b0};
    pre_n[2] = 48'h0001_0000_0000;  // (1 << 16) << 16

    for (li = 0; li < 3; li = li + 1) begin
      if (pre_neg[li]) begin
        pre_h[li] = (pre_n[li] >= {18'b0, pre_d2}) ? (pre_n[li] - {18'b0, pre_d2}) : 48'd0;
      end else begin
        pre_h[li] = pre_n[li] + {18'b0, pre_d2};
      end
      pre_sat[li] = ({14'b0, pre_h[li][47:31]} >= pre_d);
    end
  end

  logic                        s3_valid;
  logic        [30:0]          s3_d;
  logic        [62:0]          s3_dv[0:2];  // {rem[31:0], work[30:0]}
  logic        [ 2:0]          s3_neg;
  logic        [ 2:0]          s3_sat;
  logic                        s3_behind;
  logic                        s3_view;
  logic        [PAYLOAD_W-1:0] s3_pay;

  // ---------------------------------------------------------------------------
  // stages 4 .. 4+DIV_STEPS-1 — the restoring recurrence
  // ---------------------------------------------------------------------------
  // dv = {rem[31:0], work[30:0]}. Each step shifts the pair left by one, so the
  // top bit of `work` joins the remainder and a quotient bit takes its place at
  // the bottom. rem < D <= 2^31-1 holds at every step, so dv[62] is always 0
  // before a shift and nothing is lost off the top.
  logic                        dstep_valid [0:DIV_STEPS];
  logic        [30:0]          dstep_d     [0:DIV_STEPS];
  logic        [62:0]          dstep_dv    [0:DIV_STEPS][0:2];
  logic        [ 2:0]          dstep_neg   [0:DIV_STEPS];
  logic        [ 2:0]          dstep_sat   [0:DIV_STEPS];
  logic                        dstep_behind[0:DIV_STEPS];
  logic                        dstep_view  [0:DIV_STEPS];
  logic        [PAYLOAD_W-1:0] dstep_pay   [0:DIV_STEPS];

  assign dstep_valid[0]  = s3_valid;
  assign dstep_d[0]      = s3_d;
  assign dstep_dv[0][0]  = s3_dv[0];
  assign dstep_dv[0][1]  = s3_dv[1];
  assign dstep_dv[0][2]  = s3_dv[2];
  assign dstep_neg[0]    = s3_neg;
  assign dstep_sat[0]    = s3_sat;
  assign dstep_behind[0] = s3_behind;
  assign dstep_view[0]   = s3_view;
  assign dstep_pay[0]    = s3_pay;

  genvar gs, gl;
  generate
    for (gs = 0; gs < DIV_STEPS; gs = gs + 1) begin : g_div_stage
      logic                    r_valid;
      logic [30:0]             r_d;
      logic [ 2:0]             r_neg;
      logic [ 2:0]             r_sat;
      logic                    r_behind;
      logic                    r_view;
      logic [PAYLOAD_W-1:0]    r_pay;

      for (gl = 0; gl < 3; gl = gl + 1) begin : g_div_lane
        logic [31:0] t;
        logic [62:0] nxt;
        logic [62:0] r_dv;
        always_comb begin
          t   = {dstep_dv[gs][gl][61:31], dstep_dv[gs][gl][30]};
          nxt = (t >= {1'b0, dstep_d[gs]}) ?
              {t - {1'b0, dstep_d[gs]}, dstep_dv[gs][gl][29:0], 1'b1} :
              {t, dstep_dv[gs][gl][29:0], 1'b0};
        end
        always_ff @(posedge clk or negedge rst_n) begin
          if (!rst_n) r_dv <= '0;
          else if (en_i) r_dv <= nxt;
        end
        assign dstep_dv[gs+1][gl] = r_dv;
      end

      always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
          r_valid  <= 1'b0;
          r_d      <= '0;
          r_neg    <= '0;
          r_sat    <= '0;
          r_behind <= 1'b0;
          r_view   <= 1'b0;
          r_pay    <= '0;
        end else if (en_i) begin
          r_valid  <= dstep_valid[gs];
          r_d      <= dstep_d[gs];
          r_neg    <= dstep_neg[gs];
          r_sat    <= dstep_sat[gs];
          r_behind <= dstep_behind[gs];
          r_view   <= dstep_view[gs];
          r_pay    <= dstep_pay[gs];
        end
      end

      assign dstep_valid[gs+1]  = r_valid;
      assign dstep_d[gs+1]      = r_d;
      assign dstep_neg[gs+1]    = r_neg;
      assign dstep_sat[gs+1]    = r_sat;
      assign dstep_behind[gs+1] = r_behind;
      assign dstep_view[gs+1]   = r_view;
      assign dstep_pay[gs+1]    = r_pay;
    end
  endgenerate

  // ---------------------------------------------------------------------------
  // stage 5 — the quotients, signed and saturated exactly as fx_div_exact does
  // ---------------------------------------------------------------------------
  // A negative result whose magnitude is exactly 2^31 is INT32_MIN and is NOT a
  // saturation; a saturating negative is also INT32_MIN. Both land on the same
  // word, which is why the rail test can be taken before the division without
  // losing the exact case.
  logic signed [31:0] q_res[0:2];
  logic        [31:0] q_mag[0:2];
  integer             qi;
  always_comb begin
    for (qi = 0; qi < 3; qi = qi + 1) begin
      q_mag[qi] = {1'b0, dstep_dv[DIV_STEPS][qi][30:0]} +
          ((dstep_neg[DIV_STEPS][qi] && (dstep_dv[DIV_STEPS][qi][62:31] != 32'd0)) ? 32'd1 :
                                                                                     32'd0);
      if (dstep_sat[DIV_STEPS][qi]) begin
        q_res[qi] = dstep_neg[DIV_STEPS][qi] ? 32'sh8000_0000 : 32'sh7FFF_FFFF;
      end else begin
        q_res[qi] = dstep_neg[DIV_STEPS][qi] ? $signed(~q_mag[qi] + 32'd1) : $signed(q_mag[qi]);
      end
    end
  end

  logic                        s5_valid;
  logic signed [31:0]          s5_ndc_x, s5_ndc_y, s5_invw;
  logic        [30:0]          s5_w;
  logic                        s5_behind;
  logic                        s5_view;
  logic        [PAYLOAD_W-1:0] s5_pay;

  // ---------------------------------------------------------------------------
  // stage 6 — §3 fx_mad into canvas fx16, then §8 to_screen_xy
  // ---------------------------------------------------------------------------
  //   hw = w * 2^15 = (w/2) << 16      hh = h * 2^15
  //   cx = (x0 + w/2) << 16            cy = (y0 + h/2) << 16
  //   screen = rescale(ndc*hw + (cx << 16), 16)      §3, ONE rounding
  //   px     = clamp(rescale(screen, 8), ±2048*256)  §8
  // `cx << 16` inside fx_mad's own `(c << 16)` makes the whole addend
  // (x0 + w/2) << 32, which is why the shift below is 32 and not 16.
  // ==========================================================================
  // STAGE 5b -- THE PRODUCT IS REGISTERED, AND THE MEASUREMENT CHOSE THE CUT
  // ==========================================================================
  // `zhao_project_core` is instantiated by BOTH zhao_geom_project and
  // zhao_terrain_project and measured 61.09 MHz, worst path core-to-core with
  // no boundary to blame -- 39% short, on two lanes at once.
  //
  // `tools/quartus/path_anatomy.py` walked that path hop by hop:
  //
  //     0.000  uTco   s5_ndc_x[18]
  //     3.762  CELL   u_core|Mult9~430|resulta[16]   <- the DSP, COMBINATIONAL
  //     ...           the multiply's own carry chain
  //     0.355  CELL   u_core|Mult9~816|sumout        ---- 6.611 ns cumulative
  //     0.837  CELL   u_core|Add114~101|cout         a full carry chain
  //     0.912  CELL   u_core|Add118~21|sumout        a second adder
  //     0.544  CELL   u_core|scr_fx_x[7]~0|combout   saturation
  //     0.804  IC     u_core|Add123~33|datac         a third adder
  //                                                  ---- 15.906 ns total
  //
  // Multiply, then add, then add, then saturate, then add, in ONE cycle, with
  // the DSP's own output register unused -- `resulta` reached as a cell delay
  // worth 3.762 ns, a quarter of the path. A tree-wide sweep found the same
  // shape in TERRAIN.TESS's geomorph blend, so this is one mechanical fix and
  // not two bespoke ones.
  //
  // THE SPLIT WAS ARITHMETIC BEFORE IT WAS CODE:
  //     s5_ndc -> registered product                6.611 ns   ~151 MHz
  //     product -> Add114 -> Add118 -> sat -> Add123  9.295 ns   ~107 MHz
  // Both halves inside 10 ns, so ONE cut, not two. Falsifiable: if the refit
  // leaves the second half over 10 ns the split was in the wrong place and the
  // remaining chain needs its own cut between Add118 and the saturation.
  //
  // LATENCY, NOT INITIATION INTERVAL. This is a pipeline register, so a vertex
  // still enters every cycle and only the drain grows by one.
  // `terrain_project_directed` bounds the batch at `3*N + 64` cycles and
  // measures 422 against 448, so the one extra cycle is inside the slack it
  // already had -- checked before the edit, not after.
  //
  // `cx13`/`cy13` ride along registered rather than being recomputed at stage
  // 6, because `vp_*[s5_view]` is indexed by the STAGE 5 view and reading it a
  // cycle later would silently take the next vertex's viewport. That is the
  // kind of aliasing a pipeline cut introduces when the carried state is not
  // carried with it.
  logic [12:0] cx13, cy13;
  logic signed [MAD_W-1:0] prod_x_c, prod_y_c;
  always_comb begin
    cx13 = {1'b0, vp_x0[s5_view]} + {2'b0, vp_w[s5_view][11:1]};
    cy13 = {1'b0, vp_y0[s5_view]} + {2'b0, vp_h[s5_view][11:1]};
    prod_x_c = ext32m(s5_ndc_x) * $signed({{(MAD_W - 27) {1'b0}}, vp_w[s5_view], 15'b0});
    prod_y_c = ext32m(s5_ndc_y) * $signed({{(MAD_W - 27) {1'b0}}, vp_h[s5_view], 15'b0});
  end

  logic                        s6_valid, s6_behind, s6_view;
  logic signed [MAD_W-1:0]     s6_prod_x, s6_prod_y;
  logic        [12:0]          s6_cx13, s6_cy13;
  logic signed [31:0]          s6_invw;
  logic        [30:0]          s6_w;
  logic        [PAYLOAD_W-1:0] s6_pay;

  logic signed [MAD_W-1:0] mad_x, mad_y;
  logic signed [31:0] scr_fx_x, scr_fx_y;
  always_comb begin
    mad_x = s6_prod_x + ($signed({{(MAD_W - 13) {1'b0}}, s6_cx13}) <<< 32);
    mad_y = s6_prod_y + ($signed({{(MAD_W - 13) {1'b0}}, s6_cy13}) <<< 32);
    scr_fx_x = rescale16_mad(mad_x);
    scr_fx_y = rescale16_mad(mad_y);
  end

  // ---------------------------------------------------------------------------
  // the pipeline registers
  // ---------------------------------------------------------------------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      s2_valid <= 1'b0; s2_cx <= '0; s2_cy <= '0; s2_cw <= '0; s2_view <= 1'b0; s2_pay <= '0;
      s3_valid <= 1'b0; s3_d <= '0; s3_dv[0] <= '0; s3_dv[1] <= '0; s3_dv[2] <= '0;
      s3_neg <= '0; s3_sat <= '0; s3_behind <= 1'b0; s3_view <= 1'b0; s3_pay <= '0;
      s5_valid <= 1'b0; s5_ndc_x <= '0; s5_ndc_y <= '0; s5_invw <= '0; s5_w <= '0;
      s5_behind <= 1'b0;
      s5_view <= 1'b0; s5_pay <= '0;
      s6_valid <= 1'b0; s6_behind <= 1'b0; s6_view <= 1'b0;
      s6_prod_x <= '0; s6_prod_y <= '0;
      s6_cx13 <= '0; s6_cy13 <= '0;
      s6_invw <= '0; s6_w <= '0; s6_pay <= '0;
      out_valid_o <= 1'b0;
      out_x_o <= '0; out_y_o <= '0; out_d_o <= '0; out_behind_o <= 1'b0;
      out_view_o <= 1'b0; out_payload_o <= '0;
    end else if (en_i) begin
      // stage 1 lives in the ROWS_PER_PASS generate above.

      // stage 2 — the one rescale per row
      s2_valid <= s1_valid;
      s2_cx <= rescale16_row(s1_rx);
      s2_cy <= rescale16_row(s1_ry);
      s2_cw <= rescale16_row(s1_rw);
      s2_view <= s1_view;
      s2_pay <= s1_pay;

      // stage 3 — divider setup
      s3_valid <= s2_valid;
      s3_d <= pre_d;
      // rem = h[47:31] (17 bits, zero-extended into the 32-bit remainder
      // field), work = h[30:0]. The saturation compare above has already ruled
      // out rem >= D, which is exactly the invariant the recurrence needs.
      s3_dv[0] <= {15'b0, pre_h[0][47:31], pre_h[0][30:0]};
      s3_dv[1] <= {15'b0, pre_h[1][47:31], pre_h[1][30:0]};
      s3_dv[2] <= {15'b0, pre_h[2][47:31], pre_h[2][30:0]};
      s3_neg <= pre_neg;
      s3_sat <= pre_sat;
      s3_behind <= pre_behind;
      s3_view <= s2_view;
      s3_pay <= s2_pay;

      // stage 5 — quotients
      s5_valid <= dstep_valid[DIV_STEPS];
      s5_ndc_x <= q_res[0];
      s5_ndc_y <= q_res[1];
      s5_invw <= q_res[2];
      // The divisor at the END of the chain, so it is aligned with the
      // quotients rather than with the vertex that entered five stages ago.
      s5_w <= dstep_d[DIV_STEPS];
      s5_behind <= dstep_behind[DIV_STEPS];
      s5_view <= dstep_view[DIV_STEPS];
      s5_pay <= dstep_pay[DIV_STEPS];

      // stage 5b — the multiply, and everything that must arrive with it.
      s6_valid  <= s5_valid;
      s6_prod_x <= prod_x_c;
      s6_prod_y <= prod_y_c;
      s6_cx13   <= cx13;
      s6_cy13   <= cy13;
      s6_behind <= s5_behind;
      s6_view   <= s5_view;
      s6_invw   <= s5_invw;
      s6_w      <= s5_w;
      s6_pay    <= s5_pay;

      // stage 6 / output — the viewport add, the rescale and the behind-the-eye
      // zeros. `project_vertex` returns a default ProjOut on the near-plane
      // branch and never writes ScreenV at all, so the vertex carries {0,0,0}.
      out_valid_o <= s6_valid;
      out_x_o <= s6_behind ? 21'sd0 : to_screen_xy(scr_fx_x);
      out_y_o <= s6_behind ? 21'sd0 : to_screen_xy(scr_fx_y);
      out_d_o <= s6_behind ? 32'sd0 : s6_invw;
      out_w_o <= s6_behind ? 31'd0 : s6_w;
      out_behind_o <= s6_behind;
      out_view_o <= s6_view;
      out_payload_o <= s6_pay;
    end
  end

  // Any vertex anywhere, output register included. `dstep_valid[0]` is
  // `s3_valid` by assignment, so the loop covers stage 3 as well.
  integer bi;
  always_comb begin
    // s6_valid JOINS THIS LIST. `busy_o` claims "any vertex anywhere, output
    // register included"; a new pipeline stage that is not in the reduction
    // makes the block report idle while it still holds a vertex, which is the
    // same class of defect as a queue occupancy that omits its pending read.
    // seq_holds covers a vertex the ROWS_PER_PASS=1 sequencer has captured
    // but not yet launched into s1 (constant 0 at ROWS_PER_PASS=3).
    busy_o = seq_holds || s1_valid || s2_valid || s5_valid || s6_valid || out_valid_o;
    for (bi = 0; bi <= DIV_STEPS; bi = bi + 1) busy_o = busy_o || dstep_valid[bi];
  end

endmodule : zhao_project_core_mutant
