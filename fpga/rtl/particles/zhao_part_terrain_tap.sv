// zhao_part_terrain_tap.sv -- PART.COLLIDE's terrain sample, from the live
// composed terrain. Closes console entry I6 under owner ruling R1 (2026-09-19).
//
// ENFORCED-BY: tests/particles/part_terrain_tap_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHAT IT IS FOR
// ---------------------------------------------------------------------------
// `zhao_part_collide` takes a terrain sample -- {t_valid, t_height (S 9.8 m),
// t_nx/ny/nz (S1.NRM_Q unit normal)} -- "sampled WITH the particle beat" and
// declares its producer. Until this file nothing produced it: the heights
// existed (TERRAIN.COMPCACHE, read through TERRAIN.HEIGHTTAP) and the normal's
// law was contradicted in writing. Owner ruling R1 settled the law -- the
// collision normal is `normalize3_approx(face_normal(t))` of the triangle
// terrain_rules 4.3 picks for the height -- and `zhao_terrain_heighttap` now
// answers with it. This block turns that SERVICE into a SAMPLE.
//
// ---------------------------------------------------------------------------
// WHY A CACHE, AND NOT "ASK THE TAP FOR EVERY PARTICLE"
// ---------------------------------------------------------------------------
// PART.COLLIDE's contract, failure table: "terrain sample unavailable | treat
// as no contact for this tick, count it. DO NOT STALL THE PARTICLE PATH ON A
// TERRAIN READ." The tap is a multi-cycle, single-in-flight service that only
// gets the compose cache's read port on cycles TERRAIN.TESS does not want --
// so waiting for it per particle is exactly the stall the contract forbids,
// and an unbounded one.
//
// So the particle NEVER waits on a read. Terrain arrives at CELL granularity:
// one tap returns the whole cell it answered from (four corner heights, corner
// 00's placed x/z, the shift that is the cell width, and BOTH triangles'
// normals), which this block keeps in CELLS entries. A particle over a cached
// cell is evaluated here, exactly; a particle over an uncached cell goes on
// with `t_valid_o` LOW -- the contract's "unavailable, no contact, counted" --
// and the cell is fetched in the background for the particles that follow.
// Debris clusters, which is why a handful of cells is enough; CELLS is a knob.
//
// ---------------------------------------------------------------------------
// THE ARITHMETIC IS THE RATIFIED LAW, AND IT IS CHECKED AGAINST THE REFERENCE
// ---------------------------------------------------------------------------
// Height: spec/terrain_rules.md 4.3 on the cached cell, with the two exact
// collapses `zhao_terrain_heighttap` licenses and documents (ud == vd == D, a
// power of two, so the pick is `un >= vn` and the one round-half-up division
// is a shift). The tap verified `ud == vd == D` when it answered the fill; the
// containment test `0 <= un, vn < D` is re-done HERE for every particle,
// because the cache key is a shift of the particle's position and nothing
// else has checked that the cached placement agrees with it (`cell_mismatch_o`,
// fired by a lattice placed off the D grid).
//
// Normal: the tap's `normalize3_approx` output for the picked triangle, fx16,
// delivered in PART.COLLIDE's S1.NRM_Q by ONE round-half-up rescale
// (qformats 4), which is the format conversion R1's spec amendment names.
// A unit vector cannot saturate S1.10, so there is no counter for it.
//
// ENFORCED-BY: tests/particles/part_terrain_tap_directed.cpp -- every sample is
// differenced against `zref::terrain::column_query` and
// `zref::terrain::collision_normal` themselves, not a transcription.
//
// ---------------------------------------------------------------------------
// THE FRAME. The particle is not in world space, and the terrain is.
// ---------------------------------------------------------------------------
// spec/qformats.md 10: a particle's position is S 9.8 m "RELATIVE TO THE
// POPULATION ORIGIN", and "World position = origin + local"; the origin is
// fx16 on a 1/256-m grid. PART.COLLIDE compares its LOCAL `py` against
// `t_height`, so the height it is handed must be in the same local frame:
//
//     world x/z  = origin + (local <<< 8)                 exact
//     t_height   = sat_s18( rescale(h_world - origin_y, 8) )   ONE rounding
//
// The origin is a POPULATION DESCRIPTOR value (qformats 10 "Population
// descriptor") and arrives on `origin_*_i`. It has no producer inside the
// console today, the same way PART.COLLIDE's plane does not (entry I7).
//
// ---------------------------------------------------------------------------
// COHERENCE: THE CACHE MAY NEVER OUTLIVE THE LATTICE IT COPIED
// ---------------------------------------------------------------------------
// "The LIVE deformed terrain, crater and all." A cached cell is a copy of the
// compose cache's SERVED lattice, and is only true while that is. So
// `inval_i` -- driven by the composer from every event that can change what
// the compose cache serves -- empties the cache in one clock, and a fill in
// flight across an invalidation is DISCARDED rather than landed
// (`fills_discarded_o`): it describes a lattice that is no longer there.
// A pitch change is caught the same way from the other side: an answer whose
// shift is not the one its request was keyed with is discarded too.
//
// ---------------------------------------------------------------------------
// THE PRICE: SIX CLOCKS PER PARTICLE, NOT ONE, AND WHY THAT IS DECLARED
// ---------------------------------------------------------------------------
// PART.COLLIDE's ledger row says one test per clock. This block does not meet
// that, in the same way `zhao_geom_loom` states it does not: accept, look up,
// two multiplies through ONE multiplier, finish, present. The device is over
// its DSP ceiling and a second multiplier here buys throughput nothing needs
// yet: 32,768 particles x 6 clocks is 196,608 clocks, 12% of a 100 MHz 60 Hz
// frame, and the cost is FIXED -- it does not depend on terrain reads, so it is
// not the stall the contract forbids. Pipelining it is a contained change and
// the directed test measures the number, so it cannot drift silently.
//
// NOT IN THIS BLOCK: no second implementation of normalize3_approx (the tap's);
// no read port on the compose cache (the tap's); no invented height for an
// uncovered or void column -- `t_valid_o` goes low and the census says why.
//
// Conservative SystemVerilog subset only (charter 2).
`default_nettype none

module zhao_part_terrain_tap #(
    parameter int unsigned REC_W    = 128,
    // PART.COLLIDE's formats, passed through so the two cannot disagree.
    parameter int unsigned POS_W    = 18,   // S 9.8 m (qformats 10, frozen)
    parameter int unsigned NRM_W    = 12,
    // The particle record's velocity width (zhao_part_record, bits 54..86).
    parameter int unsigned VEL_W    = 11,
    parameter int unsigned NRM_Q    = 10,
    // OWNER KNOB: how many terrain cells are held. Debris clusters; four
    // cells covers a crater's worth of it. Round-robin replacement.
    parameter int unsigned CELLS    = 4,
    // A SIDEBAND THAT TRAVELS WITH THE PARTICLE. The console joins facts about
    // a particle (PART.UPDATE's survive verdict, its ordinal) to PART.COLLIDE's
    // beat by loading them on the collider's own take. Put a stage in front of
    // the collider and that join would load them one particle EARLY -- the
    // metadata-swap CLAUDE.md records -- so they ride through here instead, in
    // the same register as the record, loaded by the same enable.
    parameter int unsigned SIDE_W   = 1,
    parameter int unsigned CENSUS_W = 32
) (
    input var logic clk,
    input var logic rst_n,

    // ---- the population's origin, fx16, frame-scoped (qformats 10) ------------
    input var logic signed [31:0] origin_x_i,
    input var logic signed [31:0] origin_y_i,
    input var logic signed [31:0] origin_z_i,

    // ---- the island's cell pitch: THE SAME NET as the tap's and TERRAIN.PLACE's
    input var logic signed [7:0]  pitch_log2_i,

    // ---- the served lattice changed: forget every cached cell -----------------
    input var logic               inval_i,

    // ---- the particle, from PART.UPDATE ----------------------------------------
    input  var logic               p_valid_i,
    output var logic               p_ready_o,
    input  var logic [REC_W-1:0]   p_record_i,
    input  var logic [3:0]         p_events_i,
    input  var logic [SIDE_W-1:0]  p_side_i,

    // ---- the particle and its sample, to PART.COLLIDE ---------------------------
    output var logic               q_valid_o,
    input  var logic               q_ready_i,
    output var logic [REC_W-1:0]   q_record_o,
    output var logic [3:0]         q_events_o,
    output var logic [SIDE_W-1:0]  q_side_o,
    output var logic               t_valid_o,
    output var logic signed [POS_W-1:0] t_height_o,
    output var logic signed [NRM_W-1:0] t_nx_o,
    output var logic signed [NRM_W-1:0] t_ny_o,
    output var logic signed [NRM_W-1:0] t_nz_o,
    // The GROUND's vertical rate at the sample, interpolated by the SAME 4.3
    // triangle pick as the height beside it -- one `un >= vn` decision, so the
    // two cannot disagree about which triangle they came from.
    output var logic signed [VEL_W-1:0] t_vy_o,
    output var logic                    t_vy_valid_o,

    // ---- TERRAIN.HEIGHTTAP, as a client --------------------------------------
    output var logic               tap_req_valid_o,
    input  var logic               tap_req_ready_i,
    output var logic signed [31:0] tap_req_x_o,
    output var logic signed [31:0] tap_req_z_o,
    output var logic               tap_req_surface_o,
    input  var logic               tap_rsp_valid_i,
    input  var logic               tap_rsp_no_ground_i,
    // terrain_rules 4.3's VELOCITY member, as a cell, from TERRAIN.HEIGHTTAP.
    // NEW 2026-09-26 (TERRVEL).
    //
    // THE UNITS NEED NO CONVERSION AND THAT IS A MEASURED FACT, NOT A CHOSEN
    // ONE. terrain_rules 4.2 stores velocity as height16; height16 is fx16
    // rescaled by 8, i.e. Q8.8 metres, so 1 LSB = 1/256 m. zhao_part_collide's
    // ratified format block (amendment C2 / ruling R3) reads
    //   pos  s18  S 9.8 m      -> 1 LSB = 1/256 m
    //   vel  s11  S 2.8 m/tick -> 1 LSB = 1/256 m
    // and the field tick and the particle tick are ONE clock in this console
    // (zhao_console_core wires the same gpu_tick_frame_id_o to both). So the
    // terrain rate and the particle velocity are the SAME UNIT and the only
    // thing this block does is a saturating narrow s16 -> s11, counted.
    // Had a scale factor been needed it would have been an INVENTED constant,
    // because no spec in this tree states the velocity lane's time base.
    input  var logic signed [15:0] tap_rsp_v00_i,
    input  var logic signed [15:0] tap_rsp_v10_i,
    input  var logic signed [15:0] tap_rsp_v01_i,
    input  var logic signed [15:0] tap_rsp_v11_i,
    input  var logic               tap_rsp_vel_present_i,
    input  var logic signed [31:0] tap_rsp_h00_i,
    input  var logic signed [31:0] tap_rsp_h10_i,
    input  var logic signed [31:0] tap_rsp_h01_i,
    input  var logic signed [31:0] tap_rsp_h11_i,
    input  var logic signed [31:0] tap_rsp_wx00_i,
    input  var logic signed [31:0] tap_rsp_wz00_i,
    input  var logic        [ 4:0] tap_rsp_sh_i,
    input  var logic signed [31:0] tap_rsp_na_x_i,
    input  var logic signed [31:0] tap_rsp_na_y_i,
    input  var logic signed [31:0] tap_rsp_na_z_i,
    input  var logic signed [31:0] tap_rsp_nb_x_i,
    input  var logic signed [31:0] tap_rsp_nb_y_i,
    input  var logic signed [31:0] tap_rsp_nb_z_i,

    // ---- evidence ---------------------------------------------------------------
    // Every particle lands in exactly ONE of the first five, so
    //   particles = ground + no_ground + missed + mismatch + out_of_range
    // is a real invariant; a particle lost or double-counted breaks it.
    output var logic [CENSUS_W-1:0] particles_o,
    output var logic [CENSUS_W-1:0] samples_ground_o,     // t_valid high
    output var logic [CENSUS_W-1:0] samples_no_ground_o,  // correct: void / off the served patch
    output var logic [CENSUS_W-1:0] samples_missed_o,     // correct: cell not cached yet
    output var logic [CENSUS_W-1:0] cell_mismatch_o,      // FAULT: cached placement off the D grid
    output var logic [CENSUS_W-1:0] out_of_range_o,       // origin + local outside fx16
    output var logic [CENSUS_W-1:0] height_sats_o,        // terrain outside the population cube
    output var logic [CENSUS_W-1:0] fills_issued_o,
    output var logic [CENSUS_W-1:0] fills_landed_o,
    output var logic [CENSUS_W-1:0] fills_discarded_o,    // an invalidation overtook it
    output var logic [CENSUS_W-1:0] invalidations_o,
    // Samples whose interpolated ground rate was NON-ZERO. This is the number
    // that moves when a TerrainField moves, measured at the CONSUMER's edge
    // rather than at the producer's port -- which is the whole point of the
    // chain. It is not a tautology of t_vy_valid_o: a present velocity plane
    // over still ground reports valid and zero, every tick.
    output var logic [CENSUS_W-1:0] samples_moving_o,
    output var logic [CENSUS_W-1:0] vel_sats_o
);

  initial begin
    if (CELLS < 1 || CELLS > 16)
      $fatal(1, "zhao_part_terrain_tap: CELLS must be 1..16");
    if (NRM_Q > 15 || NRM_W < NRM_Q + 2)
      $fatal(1, "zhao_part_terrain_tap: NRM_W must hold a signed unit at Q NRM_Q");
    if (POS_W != 18)
      $fatal(1, "zhao_part_terrain_tap: POS_W is frozen at 18 (qformats 10)");
    if (CENSUS_W < 8)
      $fatal(1, "zhao_part_terrain_tap: CENSUS_W too narrow to be evidence");
  end

  localparam int unsigned IW = (CELLS <= 1) ? 1 : $clog2(CELLS);
  localparam int unsigned NSH = 16 - NRM_Q;   // fx16 -> S1.NRM_Q

  // ---- the pitch, exactly as the tap derives it ------------------------------
  logic       pitch_ok_c;
  logic [4:0] sh_c;
  always_comb begin
    pitch_ok_c = (pitch_log2_i >= -8'sd1) && (pitch_log2_i <= 8'sd2);
    sh_c       = pitch_ok_c ? 5'(16 + int'(pitch_log2_i)) : 5'd16;
  end

  // ---- the record codec: the ONE decoder of particle128 -----------------------
  logic [REC_W-1:0]   rec_q;
  logic [3:0]         ev_q;
  logic [SIDE_W-1:0]  side_q;
  logic signed [17:0] u_px, u_py, u_pz;
  /* verilator lint_off UNUSEDSIGNAL */
  // Only the position is this block's business; the record rides to the
  // collider untouched and is decoded again there, by the same codec.
  logic signed [10:0] u_vx, u_vy, u_vz;
  logic [9:0]         u_age;
  logic [6:0]         u_spc;
  logic [5:0]         u_siz, u_spn;
  logic [3:0]         u_flg;
  logic [7:0]         u_var;
  logic [127:0]       u_repack;
  logic signed [31:0] u_radius;
  logic [15:0]        u_angle;
  /* verilator lint_on UNUSEDSIGNAL */

  zhao_part_record u_codec (
      .rec_i(rec_q[127:0]),
      .pos_x_o(u_px), .pos_y_o(u_py), .pos_z_o(u_pz),
      .vel_x_o(u_vx), .vel_y_o(u_vy), .vel_z_o(u_vz),
      .age_o(u_age), .species_o(u_spc), .size_o(u_siz), .spin_o(u_spn),
      .flags_o(u_flg), .variation_o(u_var),
      .pos_x_i(u_px), .pos_y_i(u_py), .pos_z_i(u_pz),
      .vel_x_i(u_vx), .vel_y_i(u_vy), .vel_z_i(u_vz),
      .age_i(u_age), .species_i(u_spc), .size_i(u_siz), .spin_i(u_spn),
      .flags_i(u_flg), .variation_i(u_var),
      .rec_o(u_repack),
      .base_radius_i(32'sd0), .radius_o(u_radius), .angle16_o(u_angle)
  );

  // ---- world position and cell key --------------------------------------------
  logic signed [33:0] wx_c, wz_c;
  logic               in_range_c;
  logic signed [31:0] kx_c, kz_c;
  always_comb begin
    wx_c       = 34'(origin_x_i) + (34'(u_px) <<< 8);
    wz_c       = 34'(origin_z_i) + (34'(u_pz) <<< 8);
    in_range_c = (wx_c >= -34'sd2147483648) && (wx_c <= 34'sd2147483647) &&
                 (wz_c >= -34'sd2147483648) && (wz_c <= 34'sd2147483647);
    kx_c       = 32'(wx_c >>> sh_c);
    kz_c       = 32'(wz_c >>> sh_c);
  end

  // ---- the cache --------------------------------------------------------------
  logic               ce_valid  [CELLS];
  logic               ce_ground [CELLS];
  logic [4:0]         ce_sh     [CELLS];
  logic signed [31:0] ce_kx [CELLS], ce_kz [CELLS];
  logic signed [31:0] ce_h00 [CELLS], ce_h10 [CELLS], ce_h01 [CELLS], ce_h11 [CELLS];
  logic signed [31:0] ce_wx0 [CELLS], ce_wz0 [CELLS];
  logic signed [NRM_W-1:0] ce_nax [CELLS], ce_nay [CELLS], ce_naz [CELLS];
  logic signed [NRM_W-1:0] ce_nbx [CELLS], ce_nby [CELLS], ce_nbz [CELLS];
  // The velocity corners ride the SAME cache entry as the heights they belong
  // to, so a cell can never hold this patch's heights beside the last patch's
  // rates. `ce_vpres` is the compose cache's per-buffer presence, carried.
  logic signed [15:0] ce_v00 [CELLS], ce_v10 [CELLS], ce_v01 [CELLS], ce_v11 [CELLS];
  logic               ce_vpres [CELLS];
  logic [IW-1:0]      rr_q;     // round-robin victim

  logic          hit_c;
  logic [IW-1:0] hit_idx_c;
  always_comb begin
    hit_c     = 1'b0;
    hit_idx_c = '0;
    for (int e = 0; e < CELLS; e++) begin
      if (!hit_c && ce_valid[e] && (ce_sh[e] == sh_c) &&
          (ce_kx[e] == kx_c) && (ce_kz[e] == kz_c)) begin
        hit_c     = 1'b1;
        hit_idx_c = IW'(e);
      end
    end
  end

  // ---- the particle FSM -------------------------------------------------------
  localparam logic [2:0] S_IDLE = 3'd0;
  localparam logic [2:0] S_LOOK = 3'd1;
  localparam logic [2:0] S_EV0  = 3'd2;
  localparam logic [2:0] S_EV1  = 3'd3;
  localparam logic [2:0] S_EV2  = 3'd4;
  localparam logic [2:0] S_OUT  = 3'd5;
  // Two more MAD cycles for the velocity, through THE SAME multiplier. Six
  // clocks per particle becomes eight. A second multiplier would have been
  // one clock cheaper and is refused: this device is over on DSP, and the
  // block's header already argues that trade for the height.
  localparam logic [2:0] S_EV3  = 3'd6;
  localparam logic [2:0] S_EV4  = 3'd7;

  logic [2:0] st_q;

  // What the lookup found, frozen for the evaluation.
  localparam logic [2:0] K_GROUND = 3'd0;
  localparam logic [2:0] K_NOGND  = 3'd1;
  localparam logic [2:0] K_MISS   = 3'd2;
  localparam logic [2:0] K_RANGE  = 3'd3;
  logic [2:0]         kind_q;
  logic signed [32:0] un_q, vn_q;
  logic [4:0]         sh_q;
  logic signed [31:0] h00_q, h10_q, h01_q, h11_q;
  logic signed [NRM_W-1:0] nax_q, nay_q, naz_q, nbx_q, nby_q, nbz_q;
  logic signed [31:0] oy_q;
  logic signed [63:0] acc_q;
  logic signed [63:0] vacc_q;
  logic signed [15:0] v00_q, v10_q, v01_q, v11_q;
  logic               vpres_q;

  // ---- the evaluation: spec 4.3 on the cached cell ----------------------------
  logic               tri_a_c, contained_c;
  logic signed [32:0] d_c;
  logic signed [33:0] dh_a_c, dh_b_c;
  logic signed [33:0] dv_a_c, dv_b_c;
  logic signed [33:0] mul_a_c;
  logic signed [19:0] mul_b_c;
  logic signed [63:0] mul_p_c;
  always_comb begin
    d_c         = 33'sd1 <<< sh_q;
    contained_c = (un_q >= 33'sd0) && (un_q < d_c) && (vn_q >= 33'sd0) && (vn_q < d_c);
    tri_a_c     = (un_q >= vn_q);   // ud == vd == D: licensed by the tap's check
    dh_a_c      = tri_a_c ? (34'(h10_q) - 34'(h00_q)) : (34'(h11_q) - 34'(h01_q));
    dh_b_c      = tri_a_c ? (34'(h11_q) - 34'(h10_q)) : (34'(h01_q) - 34'(h00_q));
    // The velocity deltas take the SAME triangle as the height deltas above.
    // One `tri_a_c`, so 4.3 is decided once for both members of the answer.
    dv_a_c      = tri_a_c ? (34'(v10_q) - 34'(v00_q)) : (34'(v11_q) - 34'(v01_q));
    dv_b_c      = tri_a_c ? (34'(v11_q) - 34'(v10_q)) : (34'(v01_q) - 34'(v00_q));
    case (st_q)
      S_EV0:   begin mul_a_c = dh_a_c; mul_b_c = 20'(un_q[19:0]); end
      S_EV1:   begin mul_a_c = dh_b_c; mul_b_c = 20'(vn_q[19:0]); end
      S_EV3:   begin mul_a_c = dv_a_c; mul_b_c = 20'(un_q[19:0]); end
      default: begin mul_a_c = dv_b_c; mul_b_c = 20'(vn_q[19:0]); end
    endcase
    mul_p_c     = mul_a_c * mul_b_c;
  end

  // The one rounding of the height, then the frame change and ITS one rounding.
  // `h_c` cannot leave s32 -- the interpolation is a convex combination of the
  // three corners once `contained_c` holds; the tap's header has the algebra
  // and its committed mutant is the positive control for that guard.
  localparam logic signed [63:0] P_MAX = 64'sd131071;    // 2^17 - 1
  localparam logic signed [63:0] P_MIN = -64'sd131072;
  logic signed [63:0] h_c, hl_c, hq_c;
  logic               hsat_c;
  // The velocity needs NO frame change: it is a RATE, so the population
  // origin does not enter it, and no second rounding is taken.
  localparam logic signed [63:0] V_MAX = 64'sd1023;    // 2^(VEL_W-1) - 1
  localparam logic signed [63:0] V_MIN = -64'sd1024;
  logic signed [63:0] v_c;
  logic               vsat_c;
  always_comb begin
    h_c    = 64'(h00_q) + ((acc_q + (64'sd1 <<< (sh_q - 5'd1))) >>> sh_q);
    hl_c   = h_c - 64'(oy_q);
    hq_c   = (hl_c + 64'sd128) >>> 8;
    hsat_c = (hq_c > P_MAX) || (hq_c < P_MIN);
    v_c    = 64'(v00_q) + ((vacc_q + (64'sd1 <<< (sh_q - 5'd1))) >>> sh_q);
    vsat_c = (v_c > V_MAX) || (v_c < V_MIN);
  end

  // ---- the fill engine ----------------------------------------------------------
  localparam logic [1:0] F_IDLE = 2'd0;
  localparam logic [1:0] F_REQ  = 2'd1;
  localparam logic [1:0] F_WAIT = 2'd2;
  logic [1:0]         fst_q;
  logic signed [31:0] fx_q, fz_q, fkx_q, fkz_q;
  logic [4:0]         fsh_q;
  logic               fstale_q;

  assign tap_req_valid_o   = (fst_q == F_REQ);
  assign tap_req_x_o       = fx_q;
  assign tap_req_z_o       = fz_q;
  assign tap_req_surface_o = 1'b0;   // PART.COLLIDE tests the TOP surface only

  // fx16 -> S1.NRM_Q, round half up: ONE rounding (qformats 4).
  //
  // THE CLAMP CANNOT ENGAGE on a normal the tap produced, and it is here
  // anyway because -Wall asked the right question. normalize3_approx's output
  // is at most 1.0 plus its law's 2 LSB (qformats 7.4), i.e. |r| <= 2^NRM_Q,
  // which S1.NRM_Q in NRM_W >= NRM_Q + 2 bits holds with room. A plain
  // truncation would have been correct for every input the tap can give and
  // silently wrong for any other; a clamp costs a comparator and makes the
  // failure mode "a unit normal" rather than "a sign flip". No counter: a
  // counter no legal input can move is decoration, and the bound is the
  // normaliser's own, differentially tested law.
  localparam logic signed [32:0] NRM_MAX = 33'sd1 <<< (NRM_W - 1);
  function automatic logic signed [NRM_W-1:0] to_nrm(input logic signed [31:0] n);
    logic signed [32:0] r;
    begin
      r = (33'(n) + (33'sd1 <<< (NSH - 1))) >>> NSH;
      if (r > NRM_MAX - 33'sd1)  to_nrm = NRM_W'(NRM_MAX - 33'sd1);
      else if (r < -NRM_MAX)     to_nrm = NRM_W'(-NRM_MAX);
      else                       to_nrm = NRM_W'(r);
    end
  endfunction

  // A miss the fill engine can take THIS cycle.
  logic want_fill_c;
  assign want_fill_c = (st_q == S_LOOK) && in_range_c && !hit_c && (fst_q == F_IDLE) &&
                       !inval_i;

  assign p_ready_o = (st_q == S_IDLE);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_q    <= S_IDLE;
      rec_q   <= '0;
      ev_q    <= '0;
      side_q  <= '0;
      kind_q  <= K_MISS;
      un_q    <= '0;
      vn_q    <= '0;
      sh_q    <= 5'd16;
      h00_q   <= '0; h10_q <= '0; h01_q <= '0; h11_q <= '0;
      nax_q   <= '0; nay_q <= '0; naz_q <= '0;
      nbx_q   <= '0; nby_q <= '0; nbz_q <= '0;
      oy_q    <= '0;
      acc_q   <= '0;
      q_valid_o  <= 1'b0;
      q_record_o <= '0;
      q_events_o <= '0;
      q_side_o   <= '0;
      t_valid_o  <= 1'b0;
      t_height_o <= '0;
      t_nx_o     <= '0;
      t_ny_o     <= '0;
      t_nz_o     <= '0;
      fst_q    <= F_IDLE;
      fx_q     <= '0;
      fz_q     <= '0;
      fkx_q    <= '0;
      fkz_q    <= '0;
      fsh_q    <= 5'd16;
      fstale_q <= 1'b0;
      rr_q     <= '0;
      for (int e = 0; e < CELLS; e++) begin
        ce_valid[e]  <= 1'b0;
        ce_ground[e] <= 1'b0;
        ce_sh[e]     <= 5'd16;
        ce_kx[e]     <= '0;
        ce_kz[e]     <= '0;
        ce_h00[e]    <= '0;
        ce_h10[e]    <= '0;
        ce_h01[e]    <= '0;
        ce_h11[e]    <= '0;
        ce_wx0[e]    <= '0;
        ce_wz0[e]    <= '0;
        ce_nax[e]    <= '0;
        ce_nay[e]    <= '0;
        ce_naz[e]    <= '0;
        ce_nbx[e]    <= '0;
        ce_nby[e]    <= '0;
        ce_nbz[e]    <= '0;
        ce_v00[e]    <= '0;
        ce_v10[e]    <= '0;
        ce_v01[e]    <= '0;
        ce_v11[e]    <= '0;
        ce_vpres[e]  <= 1'b0;
      end
      particles_o         <= '0;
      samples_ground_o    <= '0;
      samples_no_ground_o <= '0;
      samples_missed_o    <= '0;
      cell_mismatch_o     <= '0;
      out_of_range_o      <= '0;
      height_sats_o       <= '0;
      fills_issued_o      <= '0;
      fills_landed_o      <= '0;
      fills_discarded_o   <= '0;
      invalidations_o     <= '0;
      samples_moving_o    <= '0;
      vel_sats_o          <= '0;
      t_vy_o              <= '0;
      t_vy_valid_o        <= 1'b0;
      vacc_q              <= '0;
      v00_q               <= '0;
      v10_q               <= '0;
      v01_q               <= '0;
      v11_q               <= '0;
      vpres_q             <= 1'b0;
    end else begin
      // ---------------- the particle ----------------
      case (st_q)
        S_IDLE: begin
          if (p_valid_i) begin
            rec_q <= p_record_i;
            ev_q  <= p_events_i;
            side_q <= p_side_i;
            st_q  <= S_LOOK;
          end
        end

        // The lookup, over the record just captured. An invalidation arriving
        // on this very cycle wins: the cache is emptied below and this
        // particle is a miss, never a hit on a cell that is going away.
        S_LOOK: begin
          sh_q <= sh_c;
          oy_q <= origin_y_i;
          un_q <= 33'(wx_c) - 33'(ce_wx0[hit_idx_c]);
          vn_q <= 33'(wz_c) - 33'(ce_wz0[hit_idx_c]);
          h00_q <= ce_h00[hit_idx_c];
          h10_q <= ce_h10[hit_idx_c];
          h01_q <= ce_h01[hit_idx_c];
          h11_q <= ce_h11[hit_idx_c];
          nax_q <= ce_nax[hit_idx_c];
          nay_q <= ce_nay[hit_idx_c];
          naz_q <= ce_naz[hit_idx_c];
          nbx_q <= ce_nbx[hit_idx_c];
          nby_q <= ce_nby[hit_idx_c];
          nbz_q <= ce_nbz[hit_idx_c];
          v00_q <= ce_v00[hit_idx_c];
          v10_q <= ce_v10[hit_idx_c];
          v01_q <= ce_v01[hit_idx_c];
          v11_q <= ce_v11[hit_idx_c];
          vpres_q <= ce_vpres[hit_idx_c];
          if (!in_range_c)                            kind_q <= K_RANGE;
          else if (!hit_c || inval_i)                 kind_q <= K_MISS;
          else if (!ce_ground[hit_idx_c])             kind_q <= K_NOGND;
          else                                        kind_q <= K_GROUND;
          st_q <= S_EV0;
        end

        S_EV0: begin
          acc_q <= mul_p_c;
          st_q  <= S_EV1;
        end

        S_EV1: begin
          acc_q <= acc_q + mul_p_c;
          // THE VELOCITY MADS ARE ONLY SPENT WHEN THERE IS A VELOCITY TO
          // INTERPOLATE, and that is not tidiness -- it is the block's
          // declared throughput. This lane is SIX CLOCKS PER PARTICLE
          // (header, and `part_terrain_tap_directed` asserts the number), and
          // running the two extra MADs unconditionally made it EIGHT: a 33%
          // cut to particle collision sampling, paid on every frame, to
          // interpolate a plane that is absent in every one of them today.
          // Skipping them when `vpres_q` is low keeps the no-field console
          // byte-identical in TIMING as well as in value, and charges the two
          // clocks only to the frames that use the feature.
          //
          // `vacc_q` is not cleared on the skip and does not need to be: `v_c`
          // is read only under `vpres_q`, which is the same bit that chose
          // this branch.
          st_q  <= vpres_q ? S_EV3 : S_EV2;
        end

        S_EV3: begin
          vacc_q <= mul_p_c;
          st_q   <= S_EV4;
        end

        S_EV4: begin
          vacc_q <= vacc_q + mul_p_c;
          st_q   <= S_EV2;
        end

        // Finish and present. Exactly one census bucket per particle.
        S_EV2: begin
          q_valid_o   <= 1'b1;
          q_record_o  <= rec_q;
          q_events_o  <= ev_q;
          q_side_o    <= side_q;
          t_valid_o   <= 1'b0;
          t_height_o  <= '0;
          t_nx_o      <= '0;
          t_ny_o      <= '0;
          t_nz_o      <= '0;
          t_vy_o       <= '0;
          t_vy_valid_o <= 1'b0;
          particles_o <= particles_o + 1;
          case (kind_q)
            K_RANGE: out_of_range_o      <= out_of_range_o + 1;
            K_MISS:  samples_missed_o    <= samples_missed_o + 1;
            K_NOGND: samples_no_ground_o <= samples_no_ground_o + 1;
            default: begin
              if (!contained_c) begin
                cell_mismatch_o <= cell_mismatch_o + 1;
              end else begin
                samples_ground_o <= samples_ground_o + 1;
                t_valid_o  <= 1'b1;
                if (hq_c > P_MAX)      t_height_o <= POS_W'(P_MAX);
                else if (hq_c < P_MIN) t_height_o <= POS_W'(P_MIN);
                else                   t_height_o <= POS_W'(hq_c);
                if (hsat_c) height_sats_o <= height_sats_o + 1;
                t_nx_o <= tri_a_c ? nax_q : nbx_q;
                t_ny_o <= tri_a_c ? nay_q : nby_q;
                t_nz_o <= tri_a_c ? naz_q : nbz_q;
                // PRESENCE TRAVELS WITH THE RESULT: a cell whose velocity
                // plane was not completely written answers NOT MEASURED, and
                // a written zero answers MEASURED AS STILL. The consumer is
                // handed both statements, never one standing for the other.
                if (vpres_q) begin
                  t_vy_valid_o <= 1'b1;
                  if (v_c > V_MAX)      t_vy_o <= VEL_W'(V_MAX);
                  else if (v_c < V_MIN) t_vy_o <= VEL_W'(V_MIN);
                  else                  t_vy_o <= VEL_W'(v_c);
                  if (vsat_c) vel_sats_o <= vel_sats_o + 1;
                  if (v_c != 64'sd0) samples_moving_o <= samples_moving_o + 1;
                end
              end
            end
          endcase
          st_q <= S_OUT;
        end

        S_OUT: begin
          if (q_ready_i) begin
            q_valid_o <= 1'b0;
            st_q      <= S_IDLE;
          end
        end

        default: st_q <= S_IDLE;
      endcase

      // ---------------- the fill ----------------
      case (fst_q)
        F_IDLE: begin
          if (want_fill_c) begin
            fx_q     <= 32'(wx_c);
            fz_q     <= 32'(wz_c);
            fkx_q    <= kx_c;
            fkz_q    <= kz_c;
            fsh_q    <= sh_c;
            fstale_q <= 1'b0;
            fst_q    <= F_REQ;
            fills_issued_o <= fills_issued_o + 1;
          end
        end
        F_REQ:  if (tap_req_ready_i) fst_q <= F_WAIT;
        F_WAIT: begin
          if (tap_rsp_valid_i) begin
            fst_q <= F_IDLE;
            if (fstale_q || inval_i || (tap_rsp_sh_i != fsh_q)) begin
              fills_discarded_o <= fills_discarded_o + 1;
            end else begin
              fills_landed_o      <= fills_landed_o + 1;
              ce_valid[rr_q]  <= 1'b1;
              ce_ground[rr_q] <= !tap_rsp_no_ground_i;
              ce_sh[rr_q]     <= fsh_q;
              ce_kx[rr_q]     <= fkx_q;
              ce_kz[rr_q]     <= fkz_q;
              ce_h00[rr_q]    <= tap_rsp_h00_i;
              ce_h10[rr_q]    <= tap_rsp_h10_i;
              ce_h01[rr_q]    <= tap_rsp_h01_i;
              ce_h11[rr_q]    <= tap_rsp_h11_i;
              ce_wx0[rr_q]    <= tap_rsp_wx00_i;
              ce_wz0[rr_q]    <= tap_rsp_wz00_i;
              ce_nax[rr_q]    <= to_nrm(tap_rsp_na_x_i);
              ce_nay[rr_q]    <= to_nrm(tap_rsp_na_y_i);
              ce_naz[rr_q]    <= to_nrm(tap_rsp_na_z_i);
              ce_nbx[rr_q]    <= to_nrm(tap_rsp_nb_x_i);
              ce_nby[rr_q]    <= to_nrm(tap_rsp_nb_y_i);
              ce_nbz[rr_q]    <= to_nrm(tap_rsp_nb_z_i);
              // The velocity corners land in the SAME entry, on the SAME
              // response, so a cell cannot hold one patch's heights beside
              // another's rates.
              ce_v00[rr_q]    <= tap_rsp_v00_i;
              ce_v10[rr_q]    <= tap_rsp_v10_i;
              ce_v01[rr_q]    <= tap_rsp_v01_i;
              ce_v11[rr_q]    <= tap_rsp_v11_i;
              ce_vpres[rr_q]  <= tap_rsp_vel_present_i;
              rr_q <= (rr_q == IW'(CELLS - 1)) ? '0 : rr_q + 1'b1;
            end
          end
        end
        default: fst_q <= F_IDLE;
      endcase

      // ---------------- coherence: last, so it wins ----------------
      if (inval_i) begin
        invalidations_o <= invalidations_o + 1;
        for (int e = 0; e < CELLS; e++) ce_valid[e] <= 1'b0;
        if (fst_q != F_IDLE) fstale_q <= 1'b1;
      end
    end
  end

endmodule : zhao_part_terrain_tap

`default_nettype wire
