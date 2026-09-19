// zhao_terrain_heighttap_mutant.sv -- A DELIBERATELY BROKEN COPY. NOT SHIPPED.
//
// WHAT IT PROVES. `interp_overflow_o` in zhao_terrain_heighttap.sv watches for
// an interpolated height that does not fit its s32 port. That state is
// UNREACHABLE while the block is correct: with the containment checks in place
// the interpolation is a CONVEX COMBINATION of three corner heights and cannot
// leave the interval they span (the algebra is written out in the production
// header). No legal stimulus moves the counter -- a first version of the
// directed test tried, with corners at both s32 rails, and could not. So "it
// can fire" would otherwise stay an argument forever, and the next person would
// inherit the same argument and no evidence.
//
// THE ONE SUBSTANTIVE CHANGE, in the rounding stage:
//
//     quot_c = (acc_q + round_c) >>> rq_sh_q;   // divide by the cell width D
//  -> quot_c = (acc_q + round_c);               // MUTANT: no divide
//
// That shift IS the division by the common denominator. Without it the
// expression is h00 + D*(interp - h00) -- an EXTRAPOLATION by a factor of the
// cell width -- so the three coefficients no longer sum to one, the combination
// stops being convex, and an ordinary few-metre relief lattice drives the result
// past the s32 rail by four orders of magnitude. It is the precondition the
// convexity proof rests on, broken in one line, which is why it is the right
// control: it demonstrates that the proof and the guard are describing the same
// quantity.
//
// INVERTED POLARITY: driven by
// tests/terrain/terrain_heighttap_mutant_control.cpp, which PASSES when
// `interp_overflow_o` FIRES and when `taps_answered_o` therefore does NOT
// advance. It is evidence about the INSTRUMENT, not about the design.
//
// The module is RENAMED so a source-list mistake can never elaborate it in
// place of the real one, and it lives under tests/ where
// check_forbidden_sources.py will not find it in a production closure.
//
// REGENERATE IT if zhao_terrain_heighttap.sv changes shape: this is a copy, and
// a copy of an old version is a positive control for a block that no longer
// exists. tools/budget/mutant_copy_drift.py watches for exactly that.
// ---------------------------------------------------------------------------
// zhao_terrain_heighttap.sv -- TERRAIN.HEIGHTTAP: world (x,z) -> {height, no_ground}.
//
// THE INVERSE MAP. Until this file existed the tree could only travel the
// placement in one direction. `fpga/rtl/prod/zhao_console_core.sv` states the
// absence in as many words:
//
//   "Across all of `fpga/rtl` -- every one of the 23 subdirectories, `synth/`
//    and every `probe`-named file included -- ZERO output ports match a height
//    keyed by a world coordinate, and only THREE modules emit world x/z at all:
//    `zhao_terrain_place` (`vtx_wx_o`/`vtx_wz_o`), `zhao_terrain_compcache_front`
//    (`lat_wx_o`/`lat_wz_o`) and this block itself. Both of the first two are the
//    FORWARD map, lattice index -> world; NOTHING IN THE TREE PERFORMS THE
//    INVERSE. That is the missing half of a tap service, and it is a block rather
//    than a wrapper."
//
// It is a block rather than a wrapper, and this is the block. TWO composed-
// adjacent consumers were already waiting for it, which is the argument the core
// makes for building it rather than refusing again:
//
//   * FORGE.SHADOW `tap_req_valid_o`/`tap_x_o`/`tap_z_o` -> `tap_rsp_valid_i`/
//     `tap_height_i`/`tap_no_ground_i` -- `zhao_forge_shadow.sv:131-137`. This
//     block's service port is that port MIRRORED, signal for signal.
//   * PART.COLLIDE's terrain sample, console entry I6. That one needs a SURFACE
//     NORMAL as well as a height and this block emits no normal, so I6 does not
//     close here. Said plainly rather than nearly-claimed: wiring height alone
//     and inventing a normal is the hidden-adapter failure I6 already names.
//
// ---------------------------------------------------------------------------
// THE ARITHMETIC IS TRANSCRIBED FROM THE RATIFIED LAW, NOT REDERIVED
// ---------------------------------------------------------------------------
// `spec/terrain_rules.md` 4.3 is normative pseudocode and the reference symbol
// is `zref::terrain::column_query` (`reference/src/zterrain/terrain_core.cpp:47`).
// Every quantity below carries the reference's own name:
//
//     un = wx - lat_wx[ci]      ud = lat_wx[ci+1] - lat_wx[ci]
//     vn = wz - lat_wz[cj]      vd = lat_wz[cj+1] - lat_wz[cj]
//     tri_a = un*vd >= vn*ud                       // ties to A (4.3)
//     A: num = (h10-h00)*un*vd + (h11-h10)*vn*ud
//     B: num = (h11-h01)*un*vd + (h01-h00)*vn*ud
//     den = ud*vd
//     h   = h00 + div_rhu(num, den)                // ONE rounding, qformats 4
//
// TWO ALGEBRAIC COLLAPSES ARE TAKEN AND BOTH ARE EXACT, not approximations, and
// both are LICENSED BY A CHECK RATHER THAN ASSUMED (see the next section):
//
//   1. With ud == vd == D, `un*vd >= vn*ud` is `un >= vn`. Two 64-bit products
//      removed from the triangle pick with no change of result.
//   2. With ud == vd == D, num = D*(dh1*un + dh2*vn) and den = D*D, so
//      div_rhu(num, den) = div_rhu(dh1*un + dh2*vn, D). Scaling numerator and
//      denominator of floor((2n+d)/(2d)) by the same positive D leaves it
//      unchanged: floor((2*D*acc + D*D)/(2*D*D)) == floor((2*acc + D)/(2*D)).
//      And D is a power of two by `spec/terrain_rules.md` 1.3, so that whole
//      division is `(acc + (1 <<< (SH-1))) >>> SH` -- an arithmetic shift, which
//      floors, which is what div_rhu does. NO DIVIDER ANYWHERE.
//
// 1.3 froze the pitch set for exactly this reason and says so: "Powers of two
// make world->cell lookup a shift and keep every lattice x/z exactly
// representable in fx16 -- no division, no rounding, anywhere in the addressing
// path." The collapses are that ruling being spent, not a shortcut around it.
//
// ---------------------------------------------------------------------------
// THE OFF-PATCH TEST IS THE ONE THING THAT COULD HAVE BEEN FAKED, SO IT IS NOT
// ---------------------------------------------------------------------------
// `zhao_terrain_compcache_front` stages EXACTLY ONE patch. A tap for an
// arbitrary world (x,z) usually lands on a patch that is not the served one, and
// answering with the served patch's height would be a confident wrong number of
// precisely the kind CLAUDE.md is about -- every handshake legal, every counter
// balancing, the height belonging to somewhere else entirely.
//
// The obvious fix is to take the served patch index as a port and compare. THIS
// BLOCK DOES NOT DO THAT, because it would be a detector wired to two operands
// that move together: the composer would drive the served index from the same
// sequencer nets that placed the lattice, so the comparison could only ever
// catch a wiring slip and never a staleness.
//
// Instead the containment test IS the reference's own `un`, read back through
// the RAM:
//
//     un = req_x - lat_wx(vi, vj)          -- my shift-derived index on one side,
//     answer off-patch unless 0 <= un < ud    TERRAIN.PLACE's stored placement,
//                                             through an M10K, on the other.
//
// If the served patch is a different patch, `lat_wx` is that patch's column x,
// so un differs by at least 32*D and the test fails by a wide margin. The two
// sides are clocked by different things -- one is this cycle's combinational
// shift of a request, the other is a memory written frames ago by
// `zhao_terrain_place` through `pos_we_i` -- so the comparison is structurally
// capable of catching staleness, wrong patch, wrong parity and wrong pitch. It
// also costs no port: the block never needs to be told which patch is served.
//
// ---------------------------------------------------------------------------
// HOW IT GETS ITS FOUR CORNERS WITHOUT DELAYING TERRAIN.TESS BY ONE CLOCK
// ---------------------------------------------------------------------------
// The compose cache's lattice port has NO HANDSHAKE. It accepts a request every
// cycle and answers the cycle after, and TERRAIN.TESS -- its owner -- cannot be
// stalled. So this block does not arbitrate for it; it sits IN FRONT of it as a
// pass-through and takes only the cycles the owner did not want:
//
//     o_lat_* (TERRAIN.TESS) --> [ this block ] --> c_lat_* (the compose cache)
//                                     ^ injects on cycles where o_lat_req_i == 0
//
// The owner's request wins unconditionally and is never gated, delayed or
// reordered. `c_lat_h_i` is passed STRAIGHT BACK to `o_lat_h_o` with no
// registers and no mux, and that is correct rather than lucky: the owner only
// reads the cycle after a cycle in which it requested, and on any such cycle
// this block did not inject. The one cost the owner does pay is a 2:1 mux in its
// ADDRESS path, and it is named here rather than left for a timing report to
// find.
//
// `tap_stall_clocks_o` is what that costs THIS block, and it is the number that
// decides whether the arrangement is good enough. A service that cannot report
// its own refusals cannot be sized.
//
// ---------------------------------------------------------------------------
// FOUR CORNERS, ONE CELL-STATE READ, ONE MULTIPLIER
// ---------------------------------------------------------------------------
// Six borrowed cycles per tap, then two multiply cycles. The two products go
// through ONE multiplier for the reason `zhao_geom_lod.sv` records after its own
// 18-DSP first draft: the block already has cycles it is not using, so
// parallelism here is bought by nothing. There is exactly one `*` on the
// datapath below.
//
// ---------------------------------------------------------------------------
// REFUSALS THAT ARE CORRECT ARE COUNTED APART FROM FAULTS
// ---------------------------------------------------------------------------
// Following FORGE.SHADOW's own header, which is following CLAUDE.md: counting
// them together would make a healthy scene look broken and a broken one look
// busy. `taps_void_o` and `taps_off_patch_o` are CORRECT -- a creature over a
// chasm, or over a patch that is not staged this cycle, has no ground and must
// say so. `place_mismatch_o` and `pitch_bad_o` are FAULTS: the lattice does not
// hold the placement the pitch says it should, or the island's pitch is outside
// the frozen set. All four drive `no_ground` high, because the safe answer to a
// question this block cannot answer is "no ground", and none of them is allowed
// to hide inside another's total.
//
// ENFORCED-BY: tests/terrain/terrain_heighttap_directed.cpp:main
// ---------------------------------------------------------------------------
`default_nettype none

module zhao_terrain_heighttap_mutant #(
    // The compose cache's lattice, so this block cannot disagree with it about
    // where a row ends. Elaboration-checked below.
    parameter int unsigned LAT_W    = 33,
    parameter int unsigned LAT_H    = 33,
    parameter int unsigned CENSUS_W = 32
) (
    input var logic clk,
    input var logic rst_n,

    // ---- the island's cell pitch, spec/terrain_rules.md 1.3 ------------------
    // The SAME net that drives `zhao_terrain_place.hdr_pitch_log2_i`. One source
    // of truth used twice; a second opinion about the pitch is how the inverse
    // and the forward map come apart.
    input var logic signed [7:0] pitch_log2_i,

    // ---- the service, mirroring zhao_forge_shadow.sv:131-137 -----------------
    input  var logic               req_valid_i,
    output var logic               req_ready_o,
    input  var logic signed [31:0] req_x_i,        // world x, fx16
    input  var logic signed [31:0] req_z_i,        // world z, fx16
    // 0 = the top surface (the ground a shadow lands on), 1 = the bottom.
    // A port rather than a constant because PART.COLLIDE's keel test wants the
    // other one, and a service that can only answer one of two questions makes
    // its second caller build a second service.
    input  var logic               req_surface_i,

    // The response is a PUSH with no ready, because the client that defines this
    // protocol takes it that way (`tap_rsp_valid_i` has no back channel).
    output var logic               rsp_valid_o,
    output var logic signed [31:0] rsp_height_o,
    output var logic               rsp_no_ground_o,

    // ---- the OWNER's read ports, in (TERRAIN.TESS) --------------------------
    input  var logic               o_lat_req_i,
    input  var logic        [ 5:0] o_lat_vi_i,
    input  var logic        [ 5:0] o_lat_vj_i,
    input  var logic               o_lat_surface_i,
    output var logic signed [31:0] o_lat_h_o,
    output var logic signed [31:0] o_lat_wx_o,
    output var logic signed [31:0] o_lat_wz_o,
    input  var logic               o_cs_req_i,
    input  var logic        [ 4:0] o_cs_ci_i,
    input  var logic        [ 4:0] o_cs_cj_i,
    output var logic        [ 1:0] o_cs_substance_o,

    // ---- the compose cache, out ---------------------------------------------
    output var logic               c_lat_req_o,
    output var logic        [ 5:0] c_lat_vi_o,
    output var logic        [ 5:0] c_lat_vj_o,
    output var logic               c_lat_surface_o,
    input  var logic signed [31:0] c_lat_h_i,
    input  var logic signed [31:0] c_lat_wx_i,
    input  var logic signed [31:0] c_lat_wz_i,
    output var logic               c_cs_req_o,
    output var logic        [ 4:0] c_cs_ci_o,
    output var logic        [ 4:0] c_cs_cj_o,
    input  var logic        [ 1:0] c_cs_substance_i,

    // ---- evidence ------------------------------------------------------------
    output var logic [CENSUS_W-1:0] taps_answered_o,
    output var logic [CENSUS_W-1:0] taps_void_o,        // correct: cell is not solid
    output var logic [CENSUS_W-1:0] taps_off_patch_o,   // correct: not the staged patch
    output var logic [CENSUS_W-1:0] place_mismatch_o,   // FAULT: lattice vs pitch
    output var logic [CENSUS_W-1:0] pitch_bad_o,        // FAULT: pitch outside 1.3
    output var logic [CENSUS_W-1:0] interp_overflow_o,  // FAULT: answer exceeds s32
    output var logic [CENSUS_W-1:0] tap_stall_clocks_o  // cycles the owner took
);

  // The compose cache's no-answer value, `zhao_terrain_compcache_front.sv:452`.
  // Named here rather than compared as a literal because it is that block's
  // constant and this block is only borrowing it.
  localparam logic signed [31:0] POISON = 32'sh5BADF00D;
  // Its cell-state no-answer code. Two bits have no spare encoding, so 3 is the
  // one that is not SOLID rather than a poison proper -- same file, line 472.
  localparam logic [1:0] SUB_SOLID    = 2'd0;
  localparam logic [1:0] SUB_NO_ANSWER = 2'd3;

  // The lattice is 33 vertices over 32 cells, so a cell index is 5 bits and its
  // far corner is always in range. That is the property the address arithmetic
  // below leans on, and it is elaboration-checked rather than commented.
  // Quartus 17.0 needs an elaboration check inside `initial begin ... end`
  // (CLAUDE.md); a bare module-scope `if` is a syntax error there, and the
  // linter here accepts that form silently, so a clean lint proves nothing
  // about it.
  initial begin
    if (LAT_W != 33 || LAT_H != 33)
      $fatal(1, "zhao_terrain_heighttap: LAT_W/LAT_H must be 33; the 5-bit cell index and the compose cache both assume it");
    if (CENSUS_W < 8)
      $fatal(1, "zhao_terrain_heighttap: CENSUS_W too narrow to be evidence");
  end

  // ---- the pitch, and the shift it names ------------------------------------
  // wx(i) = (patch_ix * 32 + i) <<< (16 + pitch_log2), zhao_terrain_place.sv:219-233.
  // So the inverse is >>> the same amount, arithmetic, which floors -- and floor
  // is the law: zref_island_scene.hpp:119-125's comment names the trap, "a camera
  // at x = -1 m is in patch -1, and C's `/` would put it in patch 0".
  logic        pitch_ok_c;
  logic [ 4:0] sh_c;
  always_comb begin
    pitch_ok_c = (pitch_log2_i >= -8'sd1) && (pitch_log2_i <= 8'sd2);
    sh_c       = pitch_ok_c ? 5'(16 + int'(pitch_log2_i)) : 5'd16;
  end

  // ---- request capture -------------------------------------------------------
  logic signed [31:0] rq_x_q, rq_z_q;
  logic               rq_surf_q;
  logic [ 4:0]        rq_sh_q;
  logic               rq_pitch_ok_q;
  logic [ 5:0]        ci_q, cj_q;          // cell corner, 0..31

  // The shift-derived cell index. `>>>` on a signed value floors, which is what
  // `locate()` does in the reference for a monotone axis.
  //
  // THE DISCARD IS THE DESIGN, so it is written as an explicit cast rather than
  // left as a slice a linter has to notice. The bits above 4 are the PATCH
  // index, and this block deliberately does not use them: it never learns which
  // patch is staged, and the containment test in the header -- `0 <= un < ud`
  // against TERRAIN.PLACE's stored placement -- is what recovers that fact from
  // a source this block does not control. Keeping the patch index here and
  // comparing it against a served index would be the detector-with-two-operands-
  // that-move-together failure CLAUDE.md records.
  logic [4:0] ci_c, cj_c;
  always_comb begin
    ci_c = 5'(req_x_i >>> sh_c);
    cj_c = 5'(req_z_i >>> sh_c);
  end

  // ---- the four corners, the cell state, and the answer ----------------------
  logic signed [31:0] h00_q, h10_q, h01_q, h11_q;
  logic signed [31:0] wx00_q, wx10_q, wz00_q, wz01_q;
  logic [ 1:0]        sub_q;

  localparam logic [3:0] S_IDLE = 4'd0;
  localparam logic [3:0] S_C00  = 4'd1;   // request corner (ci,   cj  )
  localparam logic [3:0] S_C10  = 4'd2;   // request corner (ci+1, cj  ), capture 00
  localparam logic [3:0] S_C01  = 4'd3;   // request corner (ci,   cj+1), capture 10
  localparam logic [3:0] S_C11  = 4'd4;   // request corner (ci+1, cj+1), capture 01
  localparam logic [3:0] S_SUB  = 4'd5;   // request cell state,          capture 11
  localparam logic [3:0] S_SUBW = 4'd6;   // capture cell state
  localparam logic [3:0] S_MUL0 = 4'd7;
  localparam logic [3:0] S_MUL1 = 4'd8;
  localparam logic [3:0] S_OUT  = 4'd9;

  logic [3:0] st_q;
  assign req_ready_o = (st_q == S_IDLE);

  // A lattice state wants a borrowed cycle; the cell-state read wants one on the
  // sibling port, which TERRAIN.TESS also owns.
  logic lat_want_c, cs_want_c;
  always_comb begin
    lat_want_c = (st_q == S_C00) || (st_q == S_C10) ||
                 (st_q == S_C01) || (st_q == S_C11);
    cs_want_c  = (st_q == S_SUB);
  end

  // The injection. The owner is never gated: `c_*_req_o` is its request OR ours,
  // and the address mux selects ours only when it did not ask.
  logic lat_go_c, cs_go_c;
  assign lat_go_c = lat_want_c && !o_lat_req_i;
  assign cs_go_c  = cs_want_c  && !o_cs_req_i;

  logic [5:0] my_vi_c, my_vj_c;
  always_comb begin
    my_vi_c = ci_q;
    my_vj_c = cj_q;
    if (st_q == S_C10 || st_q == S_C11) my_vi_c = ci_q + 6'd1;
    if (st_q == S_C01 || st_q == S_C11) my_vj_c = cj_q + 6'd1;
  end

  assign c_lat_req_o     = o_lat_req_i || lat_go_c;
  assign c_lat_vi_o      = o_lat_req_i ? o_lat_vi_i      : my_vi_c;
  assign c_lat_vj_o      = o_lat_req_i ? o_lat_vj_i      : my_vj_c;
  assign c_lat_surface_o = o_lat_req_i ? o_lat_surface_i : rq_surf_q;
  assign c_cs_req_o      = o_cs_req_i || cs_go_c;
  assign c_cs_ci_o       = o_cs_req_i ? o_cs_ci_i : ci_q[4:0];
  assign c_cs_cj_o       = o_cs_req_i ? o_cs_cj_i : cj_q[4:0];

  // Straight through. See the header: on any cycle the owner reads, the owner
  // won the cycle before, so the cache's output is its own datum.
  assign o_lat_h_o        = c_lat_h_i;
  assign o_lat_wx_o       = c_lat_wx_i;
  assign o_lat_wz_o       = c_lat_wz_i;
  assign o_cs_substance_o = c_cs_substance_i;

  // Which of my reads is landing this cycle -- one deep, because the port is one
  // deep. `lat_in_q` is set by the cycle that injected and read by the next.
  logic [3:0] lat_in_q;

  // ---- ONE multiplier --------------------------------------------------------
  // Operands are selected by the state; there is exactly one `*` below.
  logic signed [33:0] dh_a_c, dh_b_c;
  logic signed [33:0] mul_a_c;
  logic signed [19:0] mul_b_c;
  logic signed [63:0] mul_p_c;
  logic signed [63:0] acc_q;
  logic               tri_a_c;
  logic signed [32:0] un_c, vn_c, ud_c, vd_c;

  always_comb begin
    un_c = 33'(rq_x_q) - 33'(wx00_q);
    ud_c = 33'(wx10_q) - 33'(wx00_q);
    vn_c = 33'(rq_z_q) - 33'(wz00_q);
    vd_c = 33'(wz01_q) - 33'(wz00_q);
    // Licensed by the ud == vd == D check below: the cross-multiplied compare
    // `un*vd >= vn*ud` with equal positive denominators is this.
    tri_a_c = (un_c >= vn_c);
    // spec 4.3: A takes (h10-h00, h11-h10); B takes (h11-h01, h01-h00).
    dh_a_c  = tri_a_c ? (34'(h10_q) - 34'(h00_q)) : (34'(h11_q) - 34'(h01_q));
    dh_b_c  = tri_a_c ? (34'(h11_q) - 34'(h10_q)) : (34'(h01_q) - 34'(h00_q));
    mul_a_c = (st_q == S_MUL0) ? dh_a_c : dh_b_c;
    mul_b_c = (st_q == S_MUL0) ? 20'(un_c[19:0]) : 20'(vn_c[19:0]);
    mul_p_c = mul_a_c * mul_b_c;
  end

  // ---- the verdict -----------------------------------------------------------
  // Everything that can refuse, evaluated once, on the cycle the corners are in.
  logic signed [32:0] d_expect_c;       // the expected cell width, D = 1 <<< SH
  logic poison_c, place_bad_c, off_patch_c, void_c;
  always_comb begin
    d_expect_c  = 33'sd1 <<< rq_sh_q;
    poison_c    = (wx00_q == POISON) || (wx10_q == POISON) ||
                  (wz00_q == POISON) || (wz01_q == POISON);
    place_bad_c = !poison_c && ((ud_c != d_expect_c) || (vd_c != d_expect_c));
    off_patch_c = poison_c ||
                  (!place_bad_c && ((un_c < 33'sd0) || (un_c >= ud_c) ||
                                    (vn_c < 33'sd0) || (vn_c >= vd_c)));
    void_c      = (sub_q != SUB_SOLID);
  end

  // ---- the one rounding, and the one place the answer can still not fit ------
  // `acc_q` is dh*un + dh*vn with dh up to s33 and un/vn up to 2^18, so the
  // shifted quotient needs 35 bits and `h00 + quot` needs 36. Truncating that to
  // the 32-bit port would be silent, and a silently wrapped height reads as a
  // creature standing on a cliff that is not there.
  //
  // THIS GUARD EXISTS BECAUSE THE LINTER FOUND IT. `quot_c[63:32]` came back as
  // UNUSEDSIGNAL on the first lint of this file -- the handover's note that
  // "`verilate()` does not pass `-Wall`, so lint explicitly; UNUSEDSIGNAL found
  // three real correctness holes today" earned its place again here. The
  // physical argument (heights are metres, differences are small) is a WORKLOAD
  // argument about a STRUCTURAL hazard, which is the reasoning this repository
  // refuses, so the guard is structural and `interp_overflow_o` counts it.
  //
  // AND THEN THE TEST SAID IT COULD NOT FIRE, WHICH IS THE MORE USEFUL RESULT.
  // A first version of the directed suite tried to reach it with corner heights
  // at both s32 rails and could not, so the algebra was done rather than the
  // stimulus tuned:
  //
  //     tri A, with 0 <= v <= u < 1:
  //       h = h00 + (h10-h00)*u + (h11-h10)*v
  //         = h00*(1-u) + h10*(u-v) + h11*v
  //     tri B, with 0 <= u < v < 1:
  //       h = h00*(1-v) + h01*(v-u) + h11*u
  //
  // In both arms the three coefficients are NON-NEGATIVE and SUM TO ONE, so the
  // result is a CONVEX COMBINATION of three corner heights and cannot leave the
  // interval they span. The preconditions are exactly the ones already checked
  // above -- `0 <= un < ud`, `0 <= vn < vd`, `ud == vd == D > 0` -- so this is a
  // structural invariant and not an observation about content. The round-half-up
  // cannot break it either: quot <= floor(exact + 1/2) <= ceil(exact), and the
  // bound it is compared against is an integer.
  //
  // SO THE GUARD IS UNREACHABLE WITH LEGAL STIMULUS, and this repository's rule
  // for that is a COMMITTED MUTANT rather than an argument that never ends:
  // `tests/mutants/zhao_terrain_heighttap_mutant.sv` widens the containment test
  // by one line so the combination stops being convex, and its driver PASSES
  // WHEN THIS COUNTER FIRES. The guard stays because it is one comparator
  // standing between a subtle width and a silently wrapped height, and because
  // the convexity proof above is only as good as the four checks it rests on.
  localparam logic signed [63:0] S32_MAX =  64'sh0000_0000_7FFF_FFFF;
  localparam logic signed [63:0] S32_MIN = -64'sh0000_0000_8000_0000;
  logic signed [63:0] round_c;
  logic signed [63:0] quot_c;
  logic signed [63:0] sum_c;
  logic               ovf_c;
  always_comb begin
    round_c = 64'sd1 <<< (rq_sh_q - 5'd1);
    quot_c  = (acc_q + round_c);  // MUTANT: the divide by D is gone
    sum_c   = 64'(h00_q) + quot_c;
    ovf_c   = (sum_c > S32_MAX) || (sum_c < S32_MIN);
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_q               <= S_IDLE;
      rq_x_q             <= '0;
      rq_z_q             <= '0;
      rq_surf_q          <= 1'b0;
      rq_sh_q            <= 5'd16;
      rq_pitch_ok_q      <= 1'b0;
      ci_q               <= '0;
      cj_q               <= '0;
      h00_q              <= '0;
      h10_q              <= '0;
      h01_q              <= '0;
      h11_q              <= '0;
      wx00_q             <= '0;
      wx10_q             <= '0;
      wz00_q             <= '0;
      wz01_q             <= '0;
      sub_q              <= SUB_NO_ANSWER;
      lat_in_q           <= S_IDLE;
      acc_q              <= '0;
      rsp_valid_o        <= 1'b0;
      rsp_height_o       <= '0;
      rsp_no_ground_o    <= 1'b0;
      taps_answered_o    <= '0;
      taps_void_o        <= '0;
      taps_off_patch_o   <= '0;
      place_mismatch_o   <= '0;
      pitch_bad_o        <= '0;
      interp_overflow_o  <= '0;
      tap_stall_clocks_o <= '0;
    end else begin
      rsp_valid_o <= 1'b0;
      lat_in_q    <= lat_go_c ? st_q : S_IDLE;

      // A cycle this block wanted and the owner took. Counted on both ports,
      // because either one can be the thing that limits the service.
      if ((lat_want_c && o_lat_req_i) || (cs_want_c && o_cs_req_i))
        tap_stall_clocks_o <= tap_stall_clocks_o + 1;

      // Capture whichever borrowed read is landing now.
      case (lat_in_q)
        S_C00: begin h00_q <= c_lat_h_i; wx00_q <= c_lat_wx_i; wz00_q <= c_lat_wz_i; end
        S_C10: begin h10_q <= c_lat_h_i; wx10_q <= c_lat_wx_i; end
        S_C01: begin h01_q <= c_lat_h_i; wz01_q <= c_lat_wz_i; end
        S_C11: begin h11_q <= c_lat_h_i; end
        default: ;
      endcase

      case (st_q)
        S_IDLE: begin
          if (req_valid_i) begin
            rq_x_q        <= req_x_i;
            rq_z_q        <= req_z_i;
            rq_surf_q     <= req_surface_i;
            rq_sh_q       <= sh_c;
            rq_pitch_ok_q <= pitch_ok_c;
            ci_q          <= 6'(ci_c);
            cj_q          <= 6'(cj_c);
            st_q          <= S_C00;
          end
        end

        // Each corner state holds until it gets a cycle the owner did not take.
        S_C00: if (lat_go_c) st_q <= S_C10;
        S_C10: if (lat_go_c) st_q <= S_C01;
        S_C01: if (lat_go_c) st_q <= S_C11;
        S_C11: if (lat_go_c) st_q <= S_SUB;
        S_SUB: if (cs_go_c)  st_q <= S_SUBW;

        S_SUBW: begin
          sub_q <= c_cs_substance_i;
          acc_q <= '0;
          st_q  <= S_MUL0;
        end

        S_MUL0: begin
          acc_q <= mul_p_c;
          st_q  <= S_MUL1;
        end

        S_MUL1: begin
          acc_q <= acc_q + mul_p_c;
          st_q  <= S_OUT;
        end

        S_OUT: begin
          rsp_valid_o <= 1'b1;
          st_q        <= S_IDLE;
          if (!rq_pitch_ok_q) begin
            rsp_height_o    <= '0;
            rsp_no_ground_o <= 1'b1;
            pitch_bad_o     <= pitch_bad_o + 1;
          end else if (place_bad_c) begin
            rsp_height_o     <= '0;
            rsp_no_ground_o  <= 1'b1;
            place_mismatch_o <= place_mismatch_o + 1;
          end else if (off_patch_c) begin
            rsp_height_o     <= '0;
            rsp_no_ground_o  <= 1'b1;
            taps_off_patch_o <= taps_off_patch_o + 1;
          end else if (void_c) begin
            rsp_height_o    <= '0;
            rsp_no_ground_o <= 1'b1;
            taps_void_o     <= taps_void_o + 1;
          end else if (ovf_c) begin
            rsp_height_o      <= '0;
            rsp_no_ground_o   <= 1'b1;
            interp_overflow_o <= interp_overflow_o + 1;
          end else begin
            rsp_height_o    <= 32'(sum_c);
            rsp_no_ground_o <= 1'b0;
            taps_answered_o <= taps_answered_o + 1;
          end
        end

        default: st_q <= S_IDLE;
      endcase
    end
  end

endmodule : zhao_terrain_heighttap_mutant

`default_nettype wire
