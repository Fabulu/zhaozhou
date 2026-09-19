// zhao_part_project.sv -- PART.PROJECT: the particle's seat at the SHARED
// projector, plus the world-radius-to-screen-size conversion PART.EXPAND's own
// header refused to invent.
//
// ===========================================================================
// THIS BLOCK CONTAINS NO PROJECTION ARITHMETIC, AND THAT IS THE ENTIRE POINT
// ===========================================================================
// `zhao_console_core`'s header entry I24 said "NOTHING IN THIS CORE PROJECTS A
// PARTICLE" and refused PART.EXPAND, PART.SOFT and PART.LADDER on that ground.
// The obvious repair is a particle projector. It would be the WRONG repair:
// `zhao_project_core` is 6,199 ALM and 33 DSP, `zhao_project_service`'s header
// exists because this machine already shipped TWO copies of it and that was
// the largest single DSP item on the device. A third copy for particles would
// undo that campaign to draw a sprite.
//
// WHAT WAS SEARCHED BEFORE WRITING ANY RTL (the refusal was not trusted):
//   * `fpga/rtl/geometry/` -- 35 files. `zhao_geom_project` is a THIN SHELL; it
//     holds no arithmetic and instantiates `fpga/rtl/common/zhao_project_core
//     .sv`. `zhao_geom_proj_lane` is the client-A result path.
//   * `fpga/rtl/common/` -- `zhao_project_core` is the law, `zhao_project_
//     service` is ONE core behind a two-client round-robin arbiter, and
//     `zhao_proj_subsystem` composes that service with the terrain arena shell.
//   * `fpga/rtl/prod/zhao_console_core.sv` -- ONE `u_proj_subsystem`, client A
//     driven by GEOM.GROUP_SEQ, client B by TERRAIN.GROUP_SEQ. Both live.
//   * `fpga/rtl/particles/zhao_part_state.sv` -- it "carries the bits without
//     interpreting them"; the records are particle128 WORLD records and the
//     position scale is an unruled Class-C decision. See POS_SHIFT below.
//
// So the projector exists, it is shared, and its two ports are taken. I24
// concluded from that "a particle client is a third port and that is an owner
// ruling". THAT CONCLUSION IS TOO STRONG, and this block is why.
//
// A third port on `zhao_project_service` would change a verified block's
// starvation law. A third CLIENT does not have to be a third PORT. This block
// sits IN FRONT OF client A and time-multiplexes it: geometry passes straight
// through, particles are interleaved round-robin on the cycles geometry does
// not want, and the results are demultiplexed on the way back. Nothing inside
// `zhao_project_service`, `zhao_project_core` or `zhao_proj_subsystem` changes
// by one character. The arbitration law is the SAME law the service already
// uses for A against B -- round-robin with a toggling priority, flipped only on
// a contended grant -- restated here one level up rather than reinvented.
//
// THE DEMUX IS BY RIDER, NOT BY A SHADOW FIFO, for the reason the service's own
// header gives: "the rider carried the owner all the way through the pipeline,
// so routing needs no shadow FIFO and cannot drift from the data it
// describes." The top bit of the client-A rider is the owner tag.
//
// **THAT BIT IS PROVABLY FREE, and it is checked rather than assumed.**
// `zhao_geom_proj_lane` packs `{{(PAYLOAD_A_W-ARENA_W-INDEX_W){1'b0}},
// rider_arena_i, rider_index_i}` -- the padding is at the TOP. In the console
// core ARENA_W=3, INDEX_W=12, PAYLOAD_A_W=16, so bit 15 is a hard zero. The
// elaboration guard below requires the slot index to fit under the tag, and
// `geom_tag_collision_o` COUNTS a geometry rider that arrives with the tag bit
// set. It is a live detector with a positive control in the directed test, not
// a comment -- the day somebody widens the geometry rider to 16 useful bits,
// the number moves instead of the particles silently becoming vertices.
//
// THROUGHPUT, because a shared resource's cost is a rate and not an opinion.
// `zhao_project_service`'s header computes the composed demand with the vertex
// arena in place as 398,784 of 1,666,666 clocks (23.9%). Particles add, at the
// required tier of 32,768 and two views, 65,536 projections. At the default
// SLOTS=8 the round trip through the core's 36-clock latency caps this block at
// 8/36 = 0.22 particles per clock, so 65,536 costs about 295,000 clocks -- 17.7%
// -- and the composed total is about 41.6%. At SLOTS=40 the same work costs
// 65,536 clocks (3.9%). SLOTS is therefore a real frontier knob and is small by
// default because its storage is registers; see the note at its declaration.
//
// ===========================================================================
// THE SIZE CONVERSION -- A RATIFIED LAW, CALLED. NOT A DECISION TAKEN HERE.
// ===========================================================================
// `zhao_part_expand.sv` opens with a SUPERSEDED ASSUMPTION banner: amendment C2
// / ruling R3 made `size` a U 2.4 WORLD-SCALE multiplier on a species base
// radius, so `p_size_i` needs a SCREEN size, and it says "turning a world-space
// radius into a screen-space half-side is a projection... It needs the same
// treatment GEOM.PROJECT's attribute carry got: a decision, then an
// implementation."
//
// **THAT DECISION HAD ALREADY BEEN TAKEN, and this block calls it rather than
// making a second one.** `reference/include/zref/zref_particle.hpp` carries a
// correction dated 2026-09-06 whose whole point is that the next reader was
// being steered away from an answer that already existed:
//
//   > THE PROJECTION IS NOT THE MISSING PIECE. Turning a world radius into a
//   > screen half-extent is already implemented, already tested, and already
//   > has its trap written down -- `zref::render::draw_form_marker`.
//
// Its world-space branch, and the trap beside it, verbatim:
//
//   > world-space size: perspective divide at projection scale 1. The depth
//   > lane c.s.d IS 1/w (Q16.16), so the divide is already done -- the screen
//   > half-extent is size * (1/w), a MULTIPLY. Dividing by c.s.d computes
//   > size * w instead, which makes markers GROW with distance; only ortho
//   > matrices (w == 1) hide the inversion.
//
//       w_fx     = fx_mul(size_fx16, c.s.d)     // ONE rescale(.,16), saturating
//       half_sub = rescale_s32(w_fx, 8)         // S 12.8 subpixels
//       half_sub = |half_sub|
//
// That is what this block implements, bit for bit, with `size_fx16` being
// `zhao_part_record`'s `radius_o` (base_radius * size6 / 16, ONE round-half-up,
// amendment C2's own law). `tests/render/render_directed.cpp` exercises the
// branch with `flags = 0`, so it is a live law and not a dormant one.
//
// **PROJECTION SCALE IS 1, AND THAT IS THE RATIFIED CHOICE, NOT AN OMISSION.**
// An earlier draft of this block carried a `cfg_px_scale_i` knob -- pixels per
// world unit -- which would have been a second opinion about a number the
// reference does not have. The scale lives in `base_radius_fx16`, and the
// reference is explicit that THAT is the missing piece and that it is the
// owner's: "a radius per species is a content decision, and guessing one would
// be the actual instance of the failure the warning describes." So
// `cfg_base_radius_i` is the one owner value here, and PART.TABLE has no slice
// for it yet.
//
// **1/w IS NOT RECOMPUTED.** The core already divided; `a_d_i` is its Q16.16
// quotient and this block multiplies by it. There is no second reciprocal --
// which is also the trap above, stated as hardware: a divider here would be
// both expensive and wrong.
//
// WHAT THIS MAKES SAFE. With a projected SCREEN size on its input,
// `zhao_part_expand`'s `size << 4` is CORRECT rather than superseded -- its
// banner's complaint was never the shift, it was that the value arriving was a
// world multiplier. U 0.4.4 screen pixels shifted left four IS S 12.8
// subpixels, and `side_sub = 2 * half_sub` is what this block hands it. So the
// block is adopted UNCHANGED; not one character of it is edited to make it fit,
// which is what its banner asks for.
//
// THE TWO PORTS ONTO THE SAME NUMBER:
//   q_size_o    U 0.4.4 screen px  = |half_sub| >> 3   (EXPAND, SOFT)
//   q_size16_o  U 8.8  screen px   = |half_sub| << 1   (LADDER's own units)
// Both are the SAME `half_sub`, re-expressed; they cannot disagree.
//
// `size_saturations_o` counts every clamp -- the reference's own two saturating
// rescales plus the two narrowing clamps onto those ports. A particle large
// enough to saturate is a particle filling the screen, and the endpoints
// scissor it; the counter exists so that "the sprites stopped growing" is a
// number rather than a mystery.
//
// ===========================================================================
// THE LADDER LOOP
// ===========================================================================
// I24's other refusal of PART.LADDER stands and is NOT repaired here: its
// `p_prev_rung_i`/`p_hold_i` are per-(particle, camera) state its own contract
// keeps OFF chip. What this block fixes is the PAIRING. The hold state rides
// the slot store with its particle and comes back out beside the rung that
// consumed it, so the composer never has to join two streams by hand. Where the
// state comes FROM is still the absent DDR owner, and the console core says so.
//
// The ladder is not instantiated here -- it is a separate block with its own
// contract and tests. It is driven through a PORT PAIR (`lad_*` out, `rng_*`
// back) so that this block owns the packet that is waiting for a verdict. A
// composer holding that packet in its own register would be exactly the hidden
// adapter this tree forbids.
//
// ===========================================================================
// COUNTERS AND WHAT CAN FIRE THEM (CLAUDE.md: a detector reading zero is a claim)
// ===========================================================================
//   particles_projected_o  every particle result that lands.        stimulus
//   particles_behind_o     clip.w <= 0 on a particle.               stimulus
//   geom_grants_o          geometry accepted at the shared port.    stimulus
//   part_grants_o          a particle accepted there.               stimulus
//   contended_o            both asked on a cycle the port took one. stimulus
//   size_saturations_o     any of the three clamps above.           stimulus
//   slot_pressure_o        cycles a particle was offered and all    stimulus
//                          slots were busy (raise offers, or run
//                          the bench at SLOTS = 2).
//   geom_tag_collision_o   a geometry rider arrived with the owner  stimulus
//                          tag set. Driveable directly: the bench
//                          owns `g_payload_i`.
//   ladder_unexpected_o    the ladder answered with nothing         stimulus
//                          outstanding. Driveable directly: the
//                          bench owns `rng_valid_i`.
// Every one of the nine is reachable with legal stimulus through a port, so
// none of them needs a committed mutant. The overflow states they would
// otherwise watch -- a slot allocated with none free, a ladder queue pushed
// while full -- are UNREACHABLE BY CONSTRUCTION rather than uncounted: both
// allocations are gated on their own emptiness (`pv_valid_c` on `!slot_full_c`,
// `lad_valid_o` on `!lq_full_c`), so the correct behaviour asserted by the
// bench is that the producer STALLS, which is a positive property and not a
// counter reading zero.
//
// Conservative SystemVerilog subset only (charter 2). Quartus 17.0: no implicit
// generate, no module-scope elaboration `if`, guards inside `initial`.
`default_nettype none

module zhao_part_project #(
    parameter int unsigned REC_W = 128,      // particle128, amendment C2

    // The shared projector's client-A rider. The TOP bit is this block's owner
    // tag and the low SLOT_W bits are the slot index; everything between is
    // untouched padding.
    parameter int unsigned PAY_W = 16,

    // Particles in flight at the projector. A FRONTIER KNOB, not a law: the
    // core's latency is 36 clocks, so SLOTS below that throttles the particle
    // stream in exchange for storage. The slot store is registers (async read
    // at three pointers), so 8 costs roughly a few hundred ALM where 40 would
    // cost thousands -- and ALMs are this machine's binding constraint. Raise
    // it after the composed fit if the particle rate needs it; the only thing
    // that changes is the rate.
    parameter int unsigned SLOTS = 8,
    parameter int unsigned SLOT_W = $clog2(SLOTS),

    // The ladder-loop queue. PART.LADDER is a one-deep skid, so 2 is already
    // slack; it is a parameter so a deeper ladder does not silently become a
    // pairing bug.
    parameter int unsigned LAD_D = 2,
    parameter int unsigned LAD_W = $clog2(LAD_D),

    // particle128 position is 18 bits and its SCALE IS UNRULED -- PART.STATE's
    // contract calls it Class-C and that block "carries the bits without
    // interpreting them". The projector wants fx16 world coordinates. This
    // shift is the conversion and it is a named knob for exactly that reason:
    // 8 reads the record as fx8 world units. The day the scale is ruled, this
    // is the one line that moves.
    parameter int unsigned POS_SHIFT = 8
) (
    input var logic clk,
    input var logic rst_n,

    // ---- owner configuration ------------------------------------------------
    // The species base radius, fx16 world units, at projection scale 1. The
    // reference names THIS as the one missing piece and as the owner's --
    // PART.TABLE has four slices and none of them is a radius, so it is a
    // console-level value today. There is deliberately no pixel-scale knob
    // beside it; see the header.
    input var logic signed [31:0] cfg_base_radius_i,
    // Which view this pass projects. Selection is PER CAMERA (ladder ruling
    // 2026-08-31 2.5), so a two-view frame is two passes.
    input var logic cfg_view_i,

    // ---- particles in: particle128 WORLD records ----------------------------
    input  var logic             p_valid_i,
    output var logic             p_ready_o,
    input  var logic [REC_W-1:0] p_record_i,
    // The attributes the endpoints need and the record does not carry. They
    // ride the slot store with their particle, so they cannot separate from it.
    input  var logic      [15:0] p_trail_i,      // U 8.8 px, the ladder's streak test
    input  var logic             p_narrow_i,     // species: reads as a line
    input  var logic             p_protected_i,  // species: never culled
    input  var logic      [ 2:0] p_gov_floor_i,  // governor: coarsest allowed rung
    input  var logic      [ 2:0] p_prev_rung_i,  // per-(particle,camera) hold state
    input  var logic      [ 3:0] p_hold_i,
    input  var logic             p_first_i,
    input  var logic      [ 7:0] p_r_i,
    input  var logic      [ 7:0] p_g_i,
    input  var logic      [ 7:0] p_b_i,
    input  var logic      [15:0] p_src_id_i,

    // ---- the shared projector, client A: GEOMETRY IN (passes through) -------
    input  var logic               g_valid_i,
    output var logic               g_ready_o,
    input  var logic signed [31:0] g_vx_i,
    input  var logic signed [31:0] g_vy_i,
    input  var logic signed [31:0] g_vz_i,
    input  var logic               g_view_i,
    input  var logic [PAY_W-1:0]   g_payload_i,

    // ---- the shared projector, client A: THE MULTIPLEXED REQUEST ------------
    output var logic               a_valid_o,
    input  var logic               a_ready_i,
    output var logic signed [31:0] a_vx_o,
    output var logic signed [31:0] a_vy_o,
    output var logic signed [31:0] a_vz_o,
    output var logic               a_view_o,
    output var logic [PAY_W-1:0]   a_payload_o,

    // ---- the shared projector, client A: THE RESULT (no backpressure) -------
    // The core cannot be stalled on its way out, which is why the slot store
    // and the output queue are the same structure: at most SLOTS projections
    // are ever outstanding, so a result always has somewhere to land.
    input  var logic               a_valid_i,
    input  var logic signed [20:0] a_x_i,
    input  var logic signed [20:0] a_y_i,
    input  var logic signed [31:0] a_d_i,
    input  var logic        [30:0] a_w_i,
    input  var logic               a_behind_i,
    input  var logic [PAY_W-1:0]   a_payload_i,

    // ---- geometry's results, demultiplexed back out -------------------------
    output var logic               h_valid_o,
    output var logic signed [20:0] h_x_o,
    output var logic signed [20:0] h_y_o,
    output var logic signed [31:0] h_d_o,
    output var logic        [30:0] h_w_o,
    output var logic               h_behind_o,
    output var logic [PAY_W-1:0]   h_payload_o,

    // ---- the ladder loop: out to PART.LADDER ... ----------------------------
    output var logic        lad_valid_o,
    input  var logic        lad_ready_i,
    output var logic [15:0] lad_size_o,       // projected size, U 8.8 px
    output var logic [15:0] lad_trail_o,
    output var logic        lad_narrow_o,
    output var logic        lad_protected_o,
    output var logic [ 2:0] lad_gov_floor_o,
    output var logic [ 2:0] lad_prev_rung_o,
    output var logic [ 3:0] lad_hold_o,
    output var logic        lad_first_o,

    // ---- ... and its verdict back -------------------------------------------
    input  var logic        rng_valid_i,
    output var logic        rng_ready_o,
    input  var logic [ 2:0] rng_rung_i,
    input  var logic [ 3:0] rng_hold_i,
    input  var logic        rng_changed_i,

    // ---- the projected particle out -----------------------------------------
    output var logic               q_valid_o,
    input  var logic               q_ready_i,
    output var logic               q_in_o,       // 1 = in front of the eye
    output var logic signed [20:0] q_x_o,        // S 12.8 canvas
    output var logic signed [20:0] q_y_o,
    output var logic signed [31:0] q_d_o,        // Q16.16 1/w
    output var logic        [ 7:0] q_size_o,     // U 0.4.4 SCREEN px -- EXPAND/SOFT
    output var logic        [15:0] q_size16_o,   // U 8.8 SCREEN px -- the same value
    output var logic        [ 7:0] q_r_o,
    output var logic        [ 7:0] q_g_o,
    output var logic        [ 7:0] q_b_o,
    output var logic        [15:0] q_src_id_o,
    output var logic        [ 2:0] q_rung_o,     // PART.LADDER's verdict
    output var logic        [ 3:0] q_hold_new_o, // ... and the hold state to store
    output var logic               q_changed_o,

    // ---- observation ---------------------------------------------------------
    output var logic [31:0] particles_projected_o,
    output var logic [31:0] particles_behind_o,
    output var logic [31:0] geom_grants_o,
    output var logic [31:0] part_grants_o,
    output var logic [31:0] contended_o,
    output var logic [31:0] size_saturations_o,
    output var logic [31:0] slot_pressure_o,
    output var logic [31:0] geom_tag_collision_o,
    output var logic [31:0] ladder_unexpected_o
);

  // ---- elaboration guards, inside `initial` (Quartus 17.0) ------------------
  initial begin
    if (REC_W != 128)
      $fatal(1, "zhao_part_project: REC_W is %0d; particle128 (amendment C2) is 128", REC_W);
    if (SLOT_W + 1 > PAY_W)
      $fatal(1, "zhao_part_project: the rider is %0d bits but a slot index (%0d) plus the owner tag does not fit",
             PAY_W, SLOT_W);
    if (SLOTS < 2)
      $fatal(1, "zhao_part_project: SLOTS is %0d; one in-flight particle stalls the shared port for 36 clocks",
             SLOTS);
    // The ring pointers wrap by truncation, which is only the same thing as
    // wrapping modulo SLOTS when SLOTS is a power of two.
    if ((SLOTS & (SLOTS - 1)) != 0)
      $fatal(1, "zhao_part_project: SLOTS is %0d; the slot ring needs a power of two", SLOTS);
    if (LAD_D < 1)
      $fatal(1, "zhao_part_project: LAD_D is %0d", LAD_D);
    if ((LAD_D & (LAD_D - 1)) != 0)
      $fatal(1, "zhao_part_project: LAD_D is %0d; the ladder queue needs a power of two", LAD_D);
  end

  localparam int unsigned TAG_BIT = PAY_W - 1;
  localparam int unsigned PTR_W   = SLOT_W + 1;
  localparam int unsigned LQP_W   = LAD_W + 1;

  // ==========================================================================
  // INGRESS: decode the record ONCE, through the committed codec.
  //
  // `zhao_part_record` is instantiated rather than re-sliced. The console core's
  // PART.TABLE note says why in as many words: a second decode of a frozen
  // layout is a second thing that can disagree with the layout. The radius law
  // -- one round-half-up on the whole product -- comes with it for free, and it
  // is the law amendment C2 ruled.
  // ==========================================================================
  logic signed [17:0] rec_px, rec_py, rec_pz;
  logic signed [31:0] rec_radius;

  /* verilator lint_off UNUSEDSIGNAL */
  logic signed [10:0] rec_vx, rec_vy, rec_vz;
  logic        [ 9:0] rec_age;
  logic        [ 6:0] rec_species;
  logic        [ 5:0] rec_size, rec_spin;
  logic        [ 3:0] rec_flags;
  logic        [ 7:0] rec_variation;
  logic        [15:0] rec_angle16;
  logic [REC_W-1:0]   rec_repack;
  /* verilator lint_on UNUSEDSIGNAL */

  zhao_part_record u_rec (
      .rec_i      (p_record_i),
      .pos_x_o    (rec_px),
      .pos_y_o    (rec_py),
      .pos_z_o    (rec_pz),
      .vel_x_o    (rec_vx),
      .vel_y_o    (rec_vy),
      .vel_z_o    (rec_vz),
      .age_o      (rec_age),
      .species_o  (rec_species),
      .size_o     (rec_size),
      .spin_o     (rec_spin),
      .flags_o    (rec_flags),
      .variation_o(rec_variation),
      // The pack half is fed from the unpack half. It is not used; the codec is
      // one module with both directions and this is the cheapest way to leave
      // the pack side legally driven. Synthesis removes it -- nothing reads
      // `rec_repack`.
      .pos_x_i    (rec_px),
      .pos_y_i    (rec_py),
      .pos_z_i    (rec_pz),
      .vel_x_i    (rec_vx),
      .vel_y_i    (rec_vy),
      .vel_z_i    (rec_vz),
      .age_i      (rec_age),
      .species_i  (rec_species),
      .size_i     (rec_size),
      .spin_i     (rec_spin),
      .flags_i    (rec_flags),
      .variation_i(rec_variation),
      .rec_o      (rec_repack),
      .base_radius_i(cfg_base_radius_i),
      .radius_o   (rec_radius),
      .angle16_o  (rec_angle16)
  );

  // The world position, at the projector's fx16 scale. S18 shifted left 8 is
  // 26 bits, so nothing wraps in 32.
  logic signed [31:0] p_wx_c, p_wy_c, p_wz_c;
  always_comb begin
    p_wx_c = 32'($signed(rec_px)) <<< POS_SHIFT;
    p_wy_c = 32'($signed(rec_py)) <<< POS_SHIFT;
    p_wz_c = 32'($signed(rec_pz)) <<< POS_SHIFT;
  end

  // The world radius rides the slot store WHOLE, fx16, exactly as
  // `zhao_part_record` produced it. It is deliberately NOT narrowed at ingress:
  // the ratified law is `fx_mul` on the full fx16 word, and a narrowing here
  // would be the "narrower accumulator" zref_fixp.hpp permits only "with a
  // PROVED domain bound and a differential against this exact-wide reference".
  // There is no such differential, so there is no narrowing.

  // ==========================================================================
  // THE SLOT STORE.
  //
  // Three pointers over one ring: `wp_q` allocates at ingress, `dp_q` advances
  // when a result lands, `rp_q` when the packet is drained into the ladder. A
  // slot is therefore held from the moment the vertex is offered to the moment
  // its particle has left, which is exactly what bounds the outstanding work to
  // SLOTS and is what makes a result always have somewhere to land.
  // ==========================================================================
  localparam int unsigned ATTR_W = 16 + 1 + 1 + 3 + 3 + 4 + 1 + 24 + 16;  // 69
  localparam int unsigned PROJ_W = 1 + 21 + 21 + 32 + 8 + 16;             // 99

  logic signed [31:0] m_rad [SLOTS];  // written at wp, read at dp
  logic [ATTR_W-1:0] m_attr [SLOTS];  // written at wp, read at rp
  logic [PROJ_W-1:0] m_proj [SLOTS];  // written at dp, read at rp

  logic [PTR_W-1:0] wp_q, dp_q, rp_q;

  logic [PTR_W-1:0] occ_c;
  logic             slot_full_c;
  assign occ_c       = wp_q - rp_q;
  assign slot_full_c = (occ_c == PTR_W'(SLOTS));

  // ==========================================================================
  // ARBITRATION ONTO CLIENT A.
  //
  // The same law `zhao_project_service` uses for A against B, one level up:
  // round-robin with a toggling priority, flipped only on a CONTENDED accept so
  // that a lone client cannot hand its turn to an idle one.
  //
  // `a_valid_o` reads only the two VALIDs -- never `a_ready_i` -- because the
  // service's `a_ready_o` is a function of its `a_valid_i`. A valid that read
  // that ready would close a combinational loop through the arbiter.
  // ==========================================================================
  logic pv_valid_c, pv_take_c;
  assign pv_valid_c = p_valid_i && !slot_full_c;

  logic both_c, sel_p_c;
  logic prefer_p_q;
  assign both_c  = g_valid_i && pv_valid_c;
  assign sel_p_c = (both_c && prefer_p_q) || (!g_valid_i && pv_valid_c);

  assign a_valid_o   = g_valid_i || pv_valid_c;
  assign a_vx_o      = sel_p_c ? p_wx_c : g_vx_i;
  assign a_vy_o      = sel_p_c ? p_wy_c : g_vy_i;
  assign a_vz_o      = sel_p_c ? p_wz_c : g_vz_i;
  assign a_view_o    = sel_p_c ? cfg_view_i : g_view_i;

  // The particle rider: the owner tag, then zero padding, then the slot. The
  // geometry rider passes through with its top bit FORCED low -- the guard and
  // the lane's own zero padding say it is already low, and `geom_tag_collision_o`
  // is what says so at run time instead of a comment saying it.
  logic [PAY_W-1:0] ride_p_c, ride_g_c;
  always_comb begin
    ride_p_c            = '0;
    ride_p_c[TAG_BIT]   = 1'b1;
    ride_p_c[SLOT_W-1:0]= wp_q[SLOT_W-1:0];
    ride_g_c            = g_payload_i;
    ride_g_c[TAG_BIT]   = 1'b0;
  end
  assign a_payload_o = sel_p_c ? ride_p_c : ride_g_c;

  assign g_ready_o = a_ready_i && !sel_p_c;
  assign pv_take_c = a_ready_i && sel_p_c && pv_valid_c;
  assign p_ready_o = a_ready_i && sel_p_c && !slot_full_c;

  logic g_take_c;
  assign g_take_c = g_valid_i && g_ready_o;

  // ==========================================================================
  // THE RESULT SIDE.
  // ==========================================================================
  logic res_is_part_c;
  assign res_is_part_c = a_valid_i && a_payload_i[TAG_BIT];

  assign h_valid_o   = a_valid_i && !a_payload_i[TAG_BIT];
  assign h_x_o       = a_x_i;
  assign h_y_o       = a_y_i;
  assign h_d_o       = a_d_i;
  assign h_w_o       = a_w_i;
  assign h_behind_o  = a_behind_i;
  assign h_payload_o = a_payload_i;

  // ---- THE RATIFIED HALF-EXTENT ------------------------------------------
  //     w_fx     = fx_mul(radius_fx16, d)  = sat32( (p + 2^15) >>> 16 )
  //     half_sub = rescale_s32(w_fx, 8)    = sat32( (w + 2^7)  >>> 8  )
  //     half_sub = |half_sub|
  // One signed 32x32 product, two round-half-up shifts, two saturations, no
  // divider. The saturations are the reference's own and they are COUNTED
  // rather than assumed unreachable -- `rescale_s32` bumps a SatLedger at both
  // sites, and `size_saturations_o` is this block's ledger.
  logic signed [31:0] rad_rd_c;
  assign rad_rd_c = m_rad[dp_q[SLOT_W-1:0]];

  logic signed [63:0] prod_c, rnd16_c;
  logic signed [31:0] wfx_c;
  logic signed [23:0] half_c;
  logic        [23:0] abs_c;
  logic               sat16_c, sat8_c, s16_sat_c, s8_sat_c;
  logic        [15:0] size16_c;
  logic        [ 7:0] size8_c;

  always_comb begin
    prod_c  = $signed(rad_rd_c) * $signed(a_d_i);
    rnd16_c = (prod_c + 64'sd32768) >>> 16;
    if (rnd16_c > 64'sd2147483647) begin
      wfx_c   = 32'sh7FFF_FFFF;
      sat16_c = 1'b1;
    end else if (rnd16_c < -64'sd2147483648) begin
      wfx_c   = 32'sh8000_0000;
      sat16_c = 1'b1;
    end else begin
      wfx_c   = rnd16_c[31:0];
      sat16_c = 1'b0;
    end

    // A 32-bit signed value rounded down eight bits cannot leave the int32
    // range, so the reference's second saturation has no reachable clamp here
    // and the flag is a constant 0. It is NAMED rather than dropped so that the
    // two rescale sites in the law and the two here stay in correspondence.
    half_c = 24'((wfx_c + 32'sd128) >>> 8);
    sat8_c = 1'b0;
    abs_c  = half_c[23] ? (24'd0 - 24'(half_c)) : 24'(half_c);

    // U 8.8 screen pixels: the DIAMETER, which is twice the half-extent.
    if (abs_c > 24'd32767) begin
      size16_c  = 16'hFFFF;
      s16_sat_c = 1'b1;
    end else begin
      size16_c  = {abs_c[14:0], 1'b0};
      s16_sat_c = 1'b0;
    end

    // U 0.4.4 screen pixels: the same diameter in sixteenths of a pixel, which
    // is `side_sub >> 4` and therefore `|half_sub| >> 3`.
    if ((abs_c >> 3) > 24'd255) begin
      size8_c  = 8'hFF;
      s8_sat_c = 1'b1;
    end else begin
      size8_c  = abs_c[10:3];
      s8_sat_c = 1'b0;
    end
  end

  logic [PROJ_W-1:0] proj_wr_c;
  assign proj_wr_c = {!a_behind_i, a_x_i, a_y_i, a_d_i, size8_c, size16_c};

  // ==========================================================================
  // THE DRAIN AND THE LADDER LOOP.
  // ==========================================================================
  logic landed_c;
  assign landed_c = (dp_q != rp_q);

  logic [ATTR_W-1:0] attr_rd_c;
  logic [PROJ_W-1:0] proj_rd_c;
  assign attr_rd_c = m_attr[rp_q[SLOT_W-1:0]];
  assign proj_rd_c = m_proj[rp_q[SLOT_W-1:0]];

  // attr layout, most significant field first (the pack order is the only place
  // these offsets appear twice, exactly as zhao_part_record does it).
  //   [68:53] trail  [52] narrow  [51] protected  [50:48] gov_floor
  //   [47:45] prev_rung  [44:41] hold  [40] first  [39:16] rgb  [15:0] src_id
  logic [15:0] at_trail_c;
  logic        at_narrow_c, at_prot_c, at_first_c;
  logic [ 2:0] at_gov_c, at_prev_c;
  logic [ 3:0] at_hold_c;
  logic [23:0] at_rgb_c;
  logic [15:0] at_src_c;
  always_comb begin
    at_trail_c  = attr_rd_c[68:53];
    at_narrow_c = attr_rd_c[52];
    at_prot_c   = attr_rd_c[51];
    at_gov_c    = attr_rd_c[50:48];
    at_prev_c   = attr_rd_c[47:45];
    at_hold_c   = attr_rd_c[44:41];
    at_first_c  = attr_rd_c[40];
    at_rgb_c    = attr_rd_c[39:16];
    at_src_c    = attr_rd_c[15:0];
  end

  // The ladder queue: the packet waiting for its verdict.
  localparam int unsigned LQ_W = PROJ_W + 24 + 16;
  logic [LQ_W-1:0]  lq_m [LAD_D];
  logic [LQP_W-1:0] lq_wp_q, lq_rp_q;
  logic             lq_full_c, lq_empty_c;
  assign lq_full_c  = ((lq_wp_q - lq_rp_q) == LQP_W'(LAD_D));
  assign lq_empty_c = (lq_wp_q == lq_rp_q);

  assign lad_valid_o     = landed_c && !lq_full_c;
  assign lad_size_o      = proj_rd_c[15:0];
  assign lad_trail_o     = at_trail_c;
  assign lad_narrow_o    = at_narrow_c;
  assign lad_protected_o = at_prot_c;
  assign lad_gov_floor_o = at_gov_c;
  assign lad_prev_rung_o = at_prev_c;
  assign lad_hold_o      = at_hold_c;
  assign lad_first_o     = at_first_c;

  logic lad_take_c;
  assign lad_take_c = lad_valid_o && lad_ready_i;

  // The verdict comes back with the packet the queue has been holding. One
  // accept in, one verdict out, in order: the queue is what makes that a
  // structural pairing instead of an assumption about the ladder's depth.
  logic [LQ_W-1:0] lq_rd_c;
  assign lq_rd_c = lq_m[lq_rp_q[LAD_W-1:0]];

  assign q_valid_o    = rng_valid_i && !lq_empty_c;
  assign rng_ready_o  = q_ready_i && !lq_empty_c;
  assign q_in_o       = lq_rd_c[LQ_W-1];
  assign q_x_o        = $signed(lq_rd_c[LQ_W-2 -: 21]);
  assign q_y_o        = $signed(lq_rd_c[LQ_W-23 -: 21]);
  assign q_d_o        = $signed(lq_rd_c[LQ_W-44 -: 32]);
  assign q_size_o     = lq_rd_c[LQ_W-76 -: 8];
  assign q_size16_o   = lq_rd_c[LQ_W-84 -: 16];
  assign q_r_o        = lq_rd_c[39:32];
  assign q_g_o        = lq_rd_c[31:24];
  assign q_b_o        = lq_rd_c[23:16];
  assign q_src_id_o   = lq_rd_c[15:0];
  assign q_rung_o     = rng_rung_i;
  assign q_hold_new_o = rng_hold_i;
  assign q_changed_o  = rng_changed_i;

  logic q_take_c;
  assign q_take_c = q_valid_o && q_ready_i;

  // ==========================================================================
  // STATE
  // ==========================================================================
  logic [ATTR_W-1:0] attr_wr_c;
  assign attr_wr_c = {p_trail_i, p_narrow_i, p_protected_i, p_gov_floor_i,
                      p_prev_rung_i, p_hold_i, p_first_i,
                      p_r_i, p_g_i, p_b_i, p_src_id_i};

  // The two clamp sites -- the ingress radius and the result product -- can fire
  // on the SAME clock, on two different particles. They are summed into ONE
  // increment rather than written from two branches of the same always_ff,
  // where the second assignment would silently discard the first and the
  // counter would read low. Low is the flattering direction for a saturation
  // count, which is the direction this tree's broken-instrument law warns about.
  logic sat_bump_c;
  assign sat_bump_c = res_is_part_c && (sat16_c || sat8_c || s16_sat_c || s8_sat_c);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      wp_q                  <= '0;
      dp_q                  <= '0;
      rp_q                  <= '0;
      lq_wp_q               <= '0;
      lq_rp_q               <= '0;
      prefer_p_q            <= 1'b0;
      particles_projected_o <= '0;
      particles_behind_o    <= '0;
      geom_grants_o         <= '0;
      part_grants_o         <= '0;
      contended_o           <= '0;
      size_saturations_o    <= '0;
      slot_pressure_o       <= '0;
      geom_tag_collision_o  <= '0;
      ladder_unexpected_o   <= '0;
    end else begin
      // ---- the shared port ------------------------------------------------
      if (both_c && a_ready_i) begin
        prefer_p_q  <= ~prefer_p_q;
        contended_o <= contended_o + 32'd1;
      end
      if (g_take_c) begin
        geom_grants_o <= geom_grants_o + 32'd1;
        if (g_payload_i[TAG_BIT]) geom_tag_collision_o <= geom_tag_collision_o + 32'd1;
      end
      if (pv_take_c) begin
        part_grants_o            <= part_grants_o + 32'd1;
        m_rad [wp_q[SLOT_W-1:0]] <= rec_radius;
        m_attr[wp_q[SLOT_W-1:0]] <= attr_wr_c;
        wp_q                     <= wp_q + 1'b1;
      end
      if (sat_bump_c) size_saturations_o <= size_saturations_o + 32'd1;
      // Offered and refused for want of a slot. This is the backpressure the
      // bench asserts on: the producer HOLDS, nothing is dropped.
      if (p_valid_i && slot_full_c) slot_pressure_o <= slot_pressure_o + 32'd1;

      // ---- a result lands -------------------------------------------------
      if (res_is_part_c) begin
        m_proj[dp_q[SLOT_W-1:0]] <= proj_wr_c;
        dp_q                     <= dp_q + 1'b1;
        particles_projected_o    <= particles_projected_o + 32'd1;
        if (a_behind_i) particles_behind_o <= particles_behind_o + 32'd1;
      end

      // ---- the ladder loop ------------------------------------------------
      if (lad_take_c) begin
        lq_m[lq_wp_q[LAD_W-1:0]] <= {proj_rd_c, at_rgb_c, at_src_c};
        lq_wp_q                  <= lq_wp_q + 1'b1;
        rp_q                     <= rp_q + 1'b1;
      end
      if (q_take_c) lq_rp_q <= lq_rp_q + 1'b1;

      // A verdict with nothing outstanding. The ladder cannot produce one; the
      // port can, and the bench drives it. Counted rather than ignored because
      // an unpaired verdict would otherwise be applied to the NEXT particle.
      if (rng_valid_i && lq_empty_c) ladder_unexpected_o <= ladder_unexpected_o + 32'd1;
    end
  end

endmodule : zhao_part_project

`default_nettype wire
