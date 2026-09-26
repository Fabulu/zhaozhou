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
//     NORMAL as well as a height. Until 2026-09-19 this block emitted none,
//     because the normal's law was contradicted in writing (terrain_rules 4.4
//     said finite differences, TERRAIN.NORMALS.md:194 said face normals and
//     left the question open). OWNER RULING R1 settled it -- "the collision
//     normal is normalize3_approx(face_normal(...)) of the triangle 4.3
//     already picks for the height" -- and this block now emits exactly that
//     (THE NORMAL, below), plus the whole CELL it came from, which
//     PART.TERRAIN_TAP caches so that PART.COLLIDE's particle path is never
//     stalled on this multi-cycle read.
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

module zhao_terrain_heighttap #(
    // The compose cache's lattice, so this block cannot disagree with it about
    // where a row ends. Elaboration-checked below.
    parameter int unsigned LAT_W    = 33,
    parameter int unsigned LAT_H    = 33,
    parameter int unsigned CENSUS_W = 32
) (
    input var logic clk,
    input var logic rst_n,

    // ---- the island's cell pitch, spec/terrain_rules.md 1.3 ------------------
    // The pitch TERRAIN.PLACE placed the served lattice at: the value on
    // `zhao_terrain_place.hdr_pitch_log2_i`, taken on the SAME handshake PLACE
    // takes it on, and HELD. One source of truth used twice; a second opinion
    // about the pitch is how the inverse and the forward map come apart.
    // CORRECTED 2026-09-19: this used to say "the SAME net", and in the console
    // that net is TERRAIN.HDRREAD's `h_pitch_log2_o`, which reads 127 (REFUSE)
    // on every cycle a header is not being presented -- wiring it here literally
    // lints clean and makes nearly every tap a pitch fault. `zhao_console_core`
    // item 14 holds it.
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

    // ---- THE NORMAL, owner ruling R1 (2026-09-19) ----------------------------
    // `normalize3_approx(face_normal(t))` of the triangle t this answer's height
    // came from, fx16 (1.0 = 65536), +y up. Valid with `rsp_valid_o` when
    // `rsp_no_ground_o` is low; zero otherwise. See THE NORMAL in the header.
    output var logic signed [31:0] rsp_nx_o,
    output var logic signed [31:0] rsp_ny_o,
    output var logic signed [31:0] rsp_nz_o,

    // ---- THE CELL the answer came from --------------------------------------
    // Everything a client needs to evaluate OTHER points of the same cell
    // without asking again: the four corner heights of the requested surface,
    // the placed x/z of corner 00, the shift that is the cell width, and BOTH
    // triangles' normals (A = (i00,i11,i10), B = (i00,i01,i11), spec 4.3).
    // PART.TERRAIN_TAP caches it, because PART.COLLIDE's contract forbids
    // stalling the particle path on a terrain read and this service is a
    // multi-cycle one. Valid with the answer when it has ground; these are
    // read straight off the registers the answer was computed from, so they
    // cannot disagree with it.
    // terrain_rules 4.3's ratified return tuple is
    // {class, top, bottom, velocity, matA, matB, weight, sheet} and the
    // velocity member had no implementation anywhere -- not here, and not in
    // zref::terrain::ColumnResult either. These four corners finish it.
    //
    // WHY THE CORNERS AND NOT AN INTERPOLATED ANSWER. This block already
    // publishes THE CELL and lets the client run 4.3 on it: that is exactly
    // what zhao_part_terrain_tap does with rsp_h00_o..rsp_h11_o, through its
    // own single multiplier. Interpolating velocity HERE would be a second
    // implementation of 4.3 in the same datapath, and would buy multipliers
    // on a device already over on DSP to answer a question the client is
    // already set up to answer. The triangle pick is then the SAME pick --
    // the client has rsp_sh_o, rsp_wx00_o and rsp_wz00_o and decides
    // un >= vn once -- so height and velocity structurally cannot choose
    // different triangles, which they could if this block picked one and the
    // client picked another.
    //
    // The word is height16 (s16), 4.2's frozen storage format, not fx16.
    output var logic signed [15:0] rsp_v00_o,
    output var logic signed [15:0] rsp_v10_o,
    output var logic signed [15:0] rsp_v01_o,
    output var logic signed [15:0] rsp_v11_o,
    // ALL FOUR corners came from a completely written velocity plane.
    // Presence travels with the result (owner directive section 1): absent
    // means NOT MEASURED, and a zero word means MEASURED AS STILL. Those are
    // different statements and a consumer needs both.
    output var logic               rsp_vel_present_o,
    output var logic signed [31:0] rsp_h00_o,
    output var logic signed [31:0] rsp_h10_o,
    output var logic signed [31:0] rsp_h01_o,
    output var logic signed [31:0] rsp_h11_o,
    output var logic signed [31:0] rsp_wx00_o,
    output var logic signed [31:0] rsp_wz00_o,
    output var logic        [ 4:0] rsp_sh_o,
    output var logic signed [31:0] rsp_na_x_o,
    output var logic signed [31:0] rsp_na_y_o,
    output var logic signed [31:0] rsp_na_z_o,
    output var logic signed [31:0] rsp_nb_x_o,
    output var logic signed [31:0] rsp_nb_y_o,
    output var logic signed [31:0] rsp_nb_z_o,

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
    // The VELOCITY word at the SAME vertex, on the SAME borrowed read.
    // NEW 2026-09-26 (TERRVEL). It costs this block no extra cycle and no
    // arithmetic: the compose cache answers it beside the height, and this
    // block does with it exactly what it does with the height corners --
    // hands the CELL to the client and lets the client interpolate.
    input  var logic signed [15:0] c_lat_vel_i,
    input  var logic               c_lat_vel_present_i,
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
    output var logic [CENSUS_W-1:0] tap_stall_clocks_o, // cycles the owner took
    // face_normal's rescale into Q16.16 saturated on an answered tap. The
    // reference saturates identically (`rescale_s32`), so the answer still
    // equals it; this counts that the lattice was steep enough to need it.
    output var logic [CENSUS_W-1:0] normal_sats_o
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
  logic signed [15:0] v00_q, v10_q, v01_q, v11_q;
  logic               vpres_q;
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
  localparam logic [3:0] S_VERD = 4'd10;  // the verdict is final; ground -> normals
  localparam logic [3:0] S_NRM  = 4'd11;  // offer both face normals to the normaliser
  localparam logic [3:0] S_NRMW = 4'd12;  // wait for normalize3_approx

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
    quot_c  = (acc_q + round_c) >>> rq_sh_q;
    sum_c   = 64'(h00_q) + quot_c;
    ovf_c   = (sum_c > S32_MAX) || (sum_c < S32_MIN);
  end

  // ---- THE NORMAL (owner ruling R1) -------------------------------------------
  // `zref::terrain::collision_normal`: face_normal of the picked triangle with
  // its vertices at the placed lattice points (wx, h, wz) in spec 4.3's emit
  // order, then normalize3_approx. On a cell whose placement has passed the
  // `ud == vd == D` check above, the cross product COLLAPSES EXACTLY -- the
  // x and z edge components are 0 or D -- to
  //
  //     A = (i00, i11, i10):  n = D * ( -(h10-h00),  D,  -(h11-h10) )
  //     B = (i00, i01, i11):  n = D * ( -(h11-h01),  D,  -(h01-h00) )
  //
  // (write e1 = b-a, e2 = c-a and expand face_normal's three lines; every term
  // with a zero x or z edge component vanishes). The dh's are the SAME four
  // differences the height interpolation uses, and D = 1 <<< SH, so each lane
  // is a SHIFT, not a product: no multiplier is spent on the cross product.
  // face_normal then rescales Q32.32 -> Q16.16 by `rescale_s32(., 16)` --
  // round half up, SATURATING into s32 -- and that is reproduced exactly,
  // saturation included, because the reference's answer is what is owed.
  //
  // The y lane is D*D rescaled, 2^(2*SH-16), never zero, so a heightfield face
  // is never degenerate and `normalize3_approx`'s zero-vector rule cannot be
  // reached from here.
  function automatic logic signed [31:0] resc16(input logic signed [63:0] v);
    logic signed [63:0] r;
    begin
      r = (v + 64'sd32768) >>> 16;
      if (r > 64'sh0000_0000_7FFF_FFFF)       resc16 = 32'sh7FFF_FFFF;
      else if (r < -64'sh0000_0000_8000_0000) resc16 = 32'sh8000_0000;
      else                                     resc16 = r[31:0];
    end
  endfunction

  function automatic logic resc16_sat(input logic signed [63:0] v);
    logic signed [63:0] r;
    begin
      r = (v + 64'sd32768) >>> 16;
      resc16_sat = (r > 64'sh0000_0000_7FFF_FFFF) || (r < -64'sh0000_0000_8000_0000);
    end
  endfunction

  logic signed [63:0] fa_x_c, fa_z_c, fb_x_c, fb_z_c, f_y_c;
  logic signed [31:0] na_x_c, na_y_c, na_z_c, nb_x_c, nb_y_c, nb_z_c;
  logic               nsat_c;
  always_comb begin
    fa_x_c = -((64'(h10_q) - 64'(h00_q)) <<< rq_sh_q);
    fa_z_c = -((64'(h11_q) - 64'(h10_q)) <<< rq_sh_q);
    fb_x_c = -((64'(h11_q) - 64'(h01_q)) <<< rq_sh_q);
    fb_z_c = -((64'(h01_q) - 64'(h00_q)) <<< rq_sh_q);
    f_y_c  = 64'sd1 <<< (6'(rq_sh_q) + 6'(rq_sh_q));
    na_x_c = resc16(fa_x_c);
    na_y_c = resc16(f_y_c);
    na_z_c = resc16(fa_z_c);
    nb_x_c = resc16(fb_x_c);
    nb_y_c = na_y_c;
    nb_z_c = resc16(fb_z_c);
    nsat_c = resc16_sat(fa_x_c) || resc16_sat(fa_z_c) ||
             resc16_sat(fb_x_c) || resc16_sat(fb_z_c);
  end

  // ---- normalize3_approx: THE ONE IMPLEMENTATION, not a second --------------
  // `zhao_field_v3_normalize` is the tree's implementation of
  // `zref::normalize3_approx` (the latest version; v1 `zhao_field_normalize` is
  // frozen and the console inventory's G3 would refuse it). It is four points
  // wide; this block asks for two -- triangle A on lane 0, triangle B on lane 1
  // -- and lanes 2 and 3 carry the zero vector, which that block answers with
  // zeros WITHOUT touching its isqrt (its law 2), so they cost two clocks.
  //
  // It expects an ENGINE'S four-wide multiplier bank. This block is not an
  // engine and does not buy four 33x33 multipliers for a service that answers
  // one tap at a time: the bank below is ONE multiplier walked over the four
  // lanes, which is legal because the normaliser waits on `mul_valid_i`
  // rather than assuming a latency. Ten bank issues x four lanes is forty
  // clocks per answer; the two isqrt walks dominate either way.
  logic               nrm_v_ready, nrm_r_valid;
  logic signed [31:0] nrm_o0 [4], nrm_o1 [4], nrm_o2 [4];
  // Lanes 2 and 3 are the zero vector by construction (above), so their outputs
  // are zero and unread. `sat_rescale_o` cannot fire for a unit vector: every
  // output is v*r >>> (31+e) with |v| <= len, i.e. at most 1.0 plus the law's
  // 2 LSB. `rcp0_o` is for NORMALIZE2's zero vector and NORMALIZE3 never sets
  // it. `tag_o` echoes a constant. All named, none consumed.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [3:0]         nrm_sat, nrm_rcp0;
  logic [7:0]         nrm_tag;
  /* verilator lint_on UNUSEDSIGNAL */

  logic               bk_issue, bk_ready, bk_valid;
  logic signed [32:0] bk_a [4], bk_b [4];
  logic signed [65:0] bk_p [4];

  zhao_field_v3_normalize u_normalize (
      .clk(clk), .rst_n(rst_n),
      .v_valid_i(st_q == S_NRM), .v_ready_o(nrm_v_ready), .is_n3_i(1'b1),
      .a0_0_i(na_x_c), .a0_1_i(nb_x_c), .a0_2_i(32'sd0), .a0_3_i(32'sd0),
      .a1_0_i(na_y_c), .a1_1_i(nb_y_c), .a1_2_i(32'sd0), .a1_3_i(32'sd0),
      .a2_0_i(na_z_c), .a2_1_i(nb_z_c), .a2_2_i(32'sd0), .a2_3_i(32'sd0),
      .tag_i(8'd0),
      .r_valid_o(nrm_r_valid), .r_ready_i(st_q == S_NRMW),
      .o0_0_o(nrm_o0[0]), .o0_1_o(nrm_o0[1]), .o0_2_o(nrm_o0[2]), .o0_3_o(nrm_o0[3]),
      .o1_0_o(nrm_o1[0]), .o1_1_o(nrm_o1[1]), .o1_2_o(nrm_o1[2]), .o1_3_o(nrm_o1[3]),
      .o2_0_o(nrm_o2[0]), .o2_1_o(nrm_o2[1]), .o2_2_o(nrm_o2[2]), .o2_3_o(nrm_o2[3]),
      .sat_rescale_o(nrm_sat), .rcp0_o(nrm_rcp0), .tag_o(nrm_tag),
      .mul_issue_o(bk_issue), .mul_ready_i(bk_ready),
      .mul_a_0_o(bk_a[0]), .mul_a_1_o(bk_a[1]), .mul_a_2_o(bk_a[2]), .mul_a_3_o(bk_a[3]),
      .mul_b_0_o(bk_b[0]), .mul_b_1_o(bk_b[1]), .mul_b_2_o(bk_b[2]), .mul_b_3_o(bk_b[3]),
      .mul_valid_i(bk_valid),
      .mul_p_0_i(bk_p[0]), .mul_p_1_i(bk_p[1]), .mul_p_2_i(bk_p[2]), .mul_p_3_i(bk_p[3])
  );

  // The one-multiplier bank. The operands are LATCHED at issue, because the
  // normaliser's operand mux moves on as soon as it leaves the issuing state.
  logic               wk_busy;
  logic [1:0]         wk_lane;
  logic signed [32:0] wk_a [4], wk_b [4];
  logic signed [65:0] wk_prod_c;
  assign bk_ready  = !wk_busy && !bk_valid;
  assign wk_prod_c = wk_a[wk_lane] * wk_b[wk_lane];

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      wk_busy  <= 1'b0;
      wk_lane  <= 2'd0;
      bk_valid <= 1'b0;
      for (int l = 0; l < 4; l++) begin
        wk_a[l] <= '0;
        wk_b[l] <= '0;
        bk_p[l] <= '0;
      end
    end else begin
      bk_valid <= 1'b0;
      if (bk_issue && bk_ready) begin
        for (int l = 0; l < 4; l++) begin
          wk_a[l] <= bk_a[l];
          wk_b[l] <= bk_b[l];
        end
        wk_lane <= 2'd0;
        wk_busy <= 1'b1;
      end else if (wk_busy) begin
        bk_p[wk_lane] <= wk_prod_c;
        wk_lane       <= wk_lane + 2'd1;
        if (wk_lane == 2'd3) begin
          wk_busy  <= 1'b0;
          bk_valid <= 1'b1;
        end
      end
    end
  end

  // The normaliser's answer, held for the response.
  logic signed [31:0] rna_x_q, rna_y_q, rna_z_q, rnb_x_q, rnb_y_q, rnb_z_q;
  logic               rtri_a_q;   // the pick, frozen with the normals

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
      v00_q              <= '0;
      v10_q              <= '0;
      v01_q              <= '0;
      v11_q              <= '0;
      vpres_q            <= 1'b0;
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
      normal_sats_o      <= '0;
      rsp_nx_o           <= '0;
      rsp_ny_o           <= '0;
      rsp_nz_o           <= '0;
      rna_x_q            <= '0;
      rna_y_q            <= '0;
      rna_z_q            <= '0;
      rnb_x_q            <= '0;
      rnb_y_q            <= '0;
      rnb_z_q            <= '0;
      rtri_a_q           <= 1'b1;
    end else begin
      rsp_valid_o <= 1'b0;
      lat_in_q    <= lat_go_c ? st_q : S_IDLE;

      // A cycle this block wanted and the owner took. Counted on both ports,
      // because either one can be the thing that limits the service.
      if ((lat_want_c && o_lat_req_i) || (cs_want_c && o_cs_req_i))
        tap_stall_clocks_o <= tap_stall_clocks_o + 1;

      // Capture whichever borrowed read is landing now.
      case (lat_in_q)
        // The velocity corner rides the SAME capture as its height corner, so
        // the two cannot come from different vertices. vpres_q is SET by the
        // first corner and ANDed by the other three: a cell is present only if
        // every corner of it was, which is what the client's interpolation
        // actually requires.
        S_C00: begin h00_q <= c_lat_h_i; wx00_q <= c_lat_wx_i; wz00_q <= c_lat_wz_i;
                     v00_q <= c_lat_vel_i; vpres_q <= c_lat_vel_present_i; end
        S_C10: begin h10_q <= c_lat_h_i; wx10_q <= c_lat_wx_i;
                     v10_q <= c_lat_vel_i; vpres_q <= vpres_q && c_lat_vel_present_i; end
        S_C01: begin h01_q <= c_lat_h_i; wz01_q <= c_lat_wz_i;
                     v01_q <= c_lat_vel_i; vpres_q <= vpres_q && c_lat_vel_present_i; end
        S_C11: begin h11_q <= c_lat_h_i;
                     v11_q <= c_lat_vel_i; vpres_q <= vpres_q && c_lat_vel_present_i; end
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
          st_q  <= S_VERD;
        end

        // The accumulator is final from here. Only an answer WITH ground has
        // a triangle, so only that one pays for the normal; every refusal goes
        // straight out with the latency it always had. The verdict below is
        // the same combinational set S_OUT reads, over registers nothing
        // writes between here and there, so the two cannot disagree.
        S_VERD: begin
          rtri_a_q <= tri_a_c;
          if (rq_pitch_ok_q && !place_bad_c && !off_patch_c && !void_c && !ovf_c)
            st_q <= S_NRM;
          else
            st_q <= S_OUT;
        end

        S_NRM: if (nrm_v_ready) st_q <= S_NRMW;

        S_NRMW: if (nrm_r_valid) begin
          rna_x_q <= nrm_o0[0];
          rna_y_q <= nrm_o1[0];
          rna_z_q <= nrm_o2[0];
          rnb_x_q <= nrm_o0[1];
          rnb_y_q <= nrm_o1[1];
          rnb_z_q <= nrm_o2[1];
          st_q    <= S_OUT;
        end

        S_OUT: begin
          rsp_valid_o <= 1'b1;
          st_q        <= S_IDLE;
          rsp_nx_o    <= '0;
          rsp_ny_o    <= '0;
          rsp_nz_o    <= '0;
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
            rsp_nx_o        <= rtri_a_q ? rna_x_q : rnb_x_q;
            rsp_ny_o        <= rtri_a_q ? rna_y_q : rnb_y_q;
            rsp_nz_o        <= rtri_a_q ? rna_z_q : rnb_z_q;
            if (nsat_c) normal_sats_o <= normal_sats_o + 1;
          end
        end

        default: st_q <= S_IDLE;
      endcase
    end
  end

  // ---- the cell, read straight off the registers the answer came from ------
  // Continuous rather than re-registered: every one of these is written only
  // in the corner states or at request capture, both of which are strictly
  // AFTER the cycle `rsp_valid_o` is high (capture happens at the END of the
  // IDLE cycle that cycle is), so on that cycle they are the answer's cell.
  assign rsp_v00_o         = v00_q;
  assign rsp_v10_o         = v10_q;
  assign rsp_v01_o         = v01_q;
  assign rsp_v11_o         = v11_q;
  assign rsp_vel_present_o = vpres_q;
  assign rsp_h00_o  = h00_q;
  assign rsp_h10_o  = h10_q;
  assign rsp_h01_o  = h01_q;
  assign rsp_h11_o  = h11_q;
  assign rsp_wx00_o = wx00_q;
  assign rsp_wz00_o = wz00_q;
  assign rsp_sh_o   = rq_sh_q;
  assign rsp_na_x_o = rna_x_q;
  assign rsp_na_y_o = rna_y_q;
  assign rsp_na_z_o = rna_z_q;
  assign rsp_nb_x_o = rnb_x_q;
  assign rsp_nb_y_o = rnb_y_q;
  assign rsp_nb_z_o = rnb_z_q;

endmodule : zhao_terrain_heighttap

`default_nettype wire
