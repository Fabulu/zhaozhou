// zhao_part_update.sv — PART.UPDATE: the particle arithmetic, and only it.
//
// Law, in citation order:
//   design/contracts/PART.UPDATE.md — the block contract, owner ZH-062, phase 10.
//   Owner ruling 2026-08-31 §2.2  — the CLOSED twelve-recipe vocabulary.
//   qformats amendment C2 / ruling R3 — the particle128 layout AND the scales.
//
// ENFORCED-BY: tests/particles/part_update_directed.cpp:main
//
// ---------------------------------------------------------------------------
// THE SCALE QUESTION THE CONTRACT SAYS BLOCKS THIS BLOCK IS ALREADY ANSWERED
// ---------------------------------------------------------------------------
// PART.UPDATE.md says the position and velocity scales "are Class C and the
// ruling did not make them", and that "the block cannot be built until it is
// answered". That sentence is older than its answer. Amendment C2 (QFMT_VERSION
// 3, ruling R3), transcribed in `reference/include/zref/zref_particle.hpp`,
// pins both:
//
//     pos  s18  S 9.8 m   relative to the population origin   (LSB = 1/256 m)
//     vel  s11  S 2.8 m/tick                                  (LSB = 1/256 m/tick)
//     age  u10  whole 60 Hz ticks
//
// The consequence is worth stating because it is what makes the integrator
// trivial and would look like a missing conversion to anyone who did not check:
// POSITION AND VELOCITY SHARE THEIR EIGHT FRACTIONAL BITS, so integrating one
// tick of velocity into position is `pos + vel` with NO shift at all. A scale
// factor here would be a bug, not a safety margin.
//
// ---------------------------------------------------------------------------
// THERE IS NO ORACLE FOR THIS BLOCK, AND THE TEST SAYS SO
// ---------------------------------------------------------------------------
// `design/blocks.yml` declares `reference_model: zref::ParticleUpdate`. That
// symbol does not exist anywhere in the tree — it is a phantom of the kind
// `reports/PHANTOM_REFERENCES.md` catalogues. So
// `tests/particles/part_update_directed.cpp` is a SELF-CONSISTENCY test against
// hand-computed numbers, not a differential one, and it says that in its own
// header. Writing a `zref::ParticleUpdate` here would be a second
// implementation by the same hand, which is not an oracle.
//
// What IS ratified and IS used: the particle128 codec. This block instantiates
// `zhao_part_record` rather than restating twelve bit offsets, so the layout
// exists in exactly one place and PART.UPDATE cannot drift from it.
//
// ---------------------------------------------------------------------------
// THE EIGHT-STEP ORDER IS THE LAW, NOT AN IMPLEMENTATION DETAIL
// ---------------------------------------------------------------------------
//     1. advance age, evaluate lifetime
//     2. read species constants and external Field/wind input
//     3. evaluate the selected force/motion recipe
//     4. apply drag
//     5. integrate velocity and position
//     6. resolve at most ONE collision response for the tick
//     7. evaluate size and colour curves
//     8. emit deterministic spawn/death events
//
// "Reordering any pair changes the trajectory." The pair that is cheapest to
// get wrong is 3 and 4: drag is applied to the OLD velocity and the recipe's
// acceleration is added AFTER it, so a strong recipe is NOT dragged on the tick
// that produces it. Swapping them changes every trajectory in the frame and
// nothing crashes. The directed test hand-computes both answers and requires
// the first.
//
// Step 5 integrates position with the NEW velocity (semi-implicit Euler). That
// is a real choice, it is stable where explicit Euler is not for the spring-
// shaped recipes, and it is asserted with hand-computed numbers so it cannot
// drift silently into the other one.
//
// STEP 6 IS NOT IN THIS BLOCK AT ALL. RULED 2026-09-19.
//
// This file used to carry `col_valid_i/col_vx_i/col_vy_i/col_vz_i` and a mux
// that swapped the integrated velocity for a resolved one, on the reading that
// PART.UPDATE "owns the slot in the order, not the physics in it". Owner ruling
// `reports/RULING-I4-COLLISION-SPAWN-20260919.md` §1 RETIRED those four ports.
//
// The reason is not tidiness, it is that they had NO LEGAL PRODUCER.
// PART.COLLIDE is strictly downstream of this block (its contract calls itself
// a leaf), so driving them closes a cycle; `zhao_console_core` therefore tied
// them to zero, which left `collisions_applied_o` and the collision spawn event
// structurally stuck at zero and SPAWN-ON-COLLISION dead in the console. And
// the seam could not have worked even if something had driven it: PART.COLLIDE
// already resolves the response END TO END -- `vout_c` is the responded
// velocity, `pout_c` the contact-point placement, `w_flg` the collided flag --
// and all three reach the particle through the record, so applying a velocity
// here would apply the response A SECOND TIME. A velocity-only step 6 also
// cannot satisfy PART.COLLIDE.md's "a contact must not leave the particle
// inside the surface", because it does not move the position.
//
// So the eight-step order is unchanged as LAW and step 6 is executed by
// PART.COLLIDE, immediately downstream, on the same particle in the same tick.
// `design/contracts/PART.UPDATE.md` carries the amendment banner.
//
// The visible consequences here, so nobody reads them as an omission:
//   * `out_events_o[2]`, the COLLISION event bit, leaves this block ZERO.
//     PART.COLLIDE fills it from its own `c_contact_o` and hands the vector to
//     PART.SPAWN. One author per bit; see `zhao_part_collide.sv`.
//   * `kPartCollidedThisTick` is written ZERO here for the same reason. It is
//     PART.COLLIDE's to write and it writes it every beat.
//   * `collisions_applied_o` is GONE from this block's port list. A counter for
//     a step this block no longer performs could only read zero forever, and a
//     counter that cannot move is the thing CLAUDE.md says not to ship. The
//     console's `part_collisions_applied_o` is now driven by PART.COLLIDE's
//     `collision_events_o`, which can and does move.
//
// ---------------------------------------------------------------------------
// THE RECIPE ARITHMETIC IS AUTHORED AND PROVISIONAL. THE IDS ARE NOT.
// ---------------------------------------------------------------------------
// The ruling froze the VOCABULARY — twelve operations, closed for v1, "a recipe
// id is never silently reinterpreted". It did not publish the formulae. So the
// ids below are law and the arithmetic beside them is this file's authored
// proposal, every constant of it a named parameter or a species-table field so
// the owner can move it without touching RTL structure.
//
//   id  name          step-3 acceleration (per axis, before Field and gravity)
//   --  ------------  ----------------------------------------------------------
//    0  integrate     0 — the integration itself is step 5
//    1  gravity       (0, k, 0). The SIGN IS THE CONTENT'S; there is no hidden
//                     negation here, because a hidden negation is how a species
//                     table ends up meaning the opposite of what it reads
//    2  linear drag   -vel * k >> S — a second, species-selected drag term on
//                     top of the always-applied `spc_drag_i`
//    3  attraction    -d * k >> S, a LINEAR spring toward the centre. Not
//                     inverse-square: that needs a divider, and a divider here
//                     is a variable-latency stage in a fixed-latency block
//    4  repulsion      d * k >> S — exactly (3) negated
//    5  orbit         (-dz, 0, dx) * k >> S — tangential in the XZ plane
//    6  vortex        orbit plus an inward half-radius pull, plus k>>2 of lift
//    7  wind          (p0, p1, p2) * k >> S — a constant directional push
//    8  shockwave      d * k >> SHOCK_SHIFT. The SAME operand as repulsion at a
//                     larger gain: "the recipe with the widest dynamic range,
//                     and the one most likely to saturate velocity"
//    9  spline flow   (p - vel) * k >> S — targets a VELOCITY, not a position,
//                     which is what makes a flow follow rather than orbit
//   10  colour curve  0 — selects the step-7 colour lookup
//   11  size curve    0 — selects the step-7 size lookup
//
// where d = pos - (spc_cx, spc_cy, spc_cz), k = spc_strength_i, S = RECIPE_SHIFT.
//
// `spc_grav_i` and `spc_drag_i` are the contract's "optional gravity/drag" and
// are applied to EVERY species whatever its recipe; a species that wants
// neither stores zero. That is what "ordinary integration plus optional
// gravity/drag plus ONE bounded primary motion recipe — not a program" means
// when it is wired.
//
// ONE MULTIPLIER PER AXIS, NOT ONE PER ARM. The recipe case selects the
// multiplier's OPERAND; it does not contain a multiply. That is deliberate and
// it is the combiner lesson: `unit_mul_logic(...)` inside every arm of a
// seven-arm case produced fourteen multipliers against a rule of two. Here the
// arithmetic is three recipe products and three drag products, six in total,
// whatever the vocabulary grows to.
//
// ---------------------------------------------------------------------------
// ROUNDING
// ---------------------------------------------------------------------------
// Round-half AWAY FROM ZERO, computed on the MAGNITUDE so the negative side is
// the mirror of the positive one. An arithmetic right shift is a floor, which
// rounds -0.5 to -1 and +0.5 to 0 — a bias that points particles one way and
// would show up as drift over a thousand ticks rather than as a failure.
//
// There are TWO rounded quantities on the velocity path, not one, and it is
// worth saying so rather than claiming the tree's "one rounding per emitted
// value" covers it: the dragged velocity (step 4) and the recipe acceleration
// (step 3). They are different values; each is rounded once; the sum of them is
// exact.
//
// ---------------------------------------------------------------------------
// OVERFLOW
// ---------------------------------------------------------------------------
// | unknown recipe id       | refuse the particle, count it. NEVER fall back to
// |                         | integrate — a silently different motion is worse
// |                         | than a missing particle
// | species out of range    | refuse; PART.STATE should have caught it, and
// |                         | catching it twice is cheap
// | velocity saturates s11  | CLAMP and count. Wrapping would reverse a
// |                         | particle's direction, which reads as a physics bug
// | age reaches 2^10        | the particle dies
//
// Position saturation is NOT in that table — the contract is silent on it. This
// block clamps and counts it anyway, on the contract's own stated reasoning:
// an s18 position that wrapped would teleport a particle from one edge of the
// population to the other, which is the same class of fault as a reversed
// velocity and is even harder to read on screen. `position_saturations_o` is
// separate from the velocity counter so the two questions stay separable.
//
// ---------------------------------------------------------------------------
// LATENCY, AND WHAT IS NOT MEASURED HERE
// ---------------------------------------------------------------------------
// FIXED at one clock, one particle per clock while the consumer is ready. There
// is no data-dependent branching: every recipe resolves through the same
// operand mux, the same two products and the same adder.
//
// That puts twelve recipes, a drag multiply and two saturations in one
// combinational cone, which is an Fmax question and Fmax questions need a fit.
// This block has NOT been through `quartus_map`, so it is not known to be
// synthesizable however clean its Verilator lint — those are two tools with two
// opinions and only one of them ships.
//
// Conservative SystemVerilog subset only (charter §2).
`default_nettype none

module zhao_part_update #(
    // The species table's size. 7 bits of `species` can address 128; a smaller
    // table makes the out-of-range refusal REACHABLE, which at 128 it is not.
    parameter int unsigned SPECIES_N    = 128,
    parameter int unsigned REC_W        = 128,
    // The recipe gain. Every recipe divides its product by 2**RECIPE_SHIFT, so
    // this is the one knob that scales the whole vocabulary together.
    parameter int unsigned RECIPE_SHIFT = 8,
    // Shockwave's own, deliberately smaller so it is the recipe that saturates.
    parameter int unsigned SHOCK_SHIFT  = 5,
    parameter int unsigned AGE_W        = 10
) (
    input var logic clk,
    input var logic rst_n,

    // ---- the particle, from PART.STATE's `prt_*` channel ---------------------
    input  wire                  in_valid_i,
    output wire                  in_ready_o,
    input  wire [REC_W-1:0]      in_record_i,

    // ---- the species descriptor ----------------------------------------------
    // Read as PORTS. The table is on chip and belongs to PART.STATE (this
    // block's ceiling is 0 M10K), so what crosses here is the descriptor for
    // `spc_index_o`, combinationally, in the SAME cycle as `in_record_i`.
    //
    // There is no capture register on this path and that is on purpose. A bank
    // that registers its read and a stage that holds its request move apart on
    // a stall and deliver one record's data with another's metadata. Here both
    // sides of every comparison are the same cycle's wires, so there is no
    // stall for them to come apart across.
    output wire [6:0]            spc_index_o,
    input  wire [3:0]            spc_recipe_i,     // the closed vocabulary's id
    input  wire [AGE_W-1:0]      spc_lifetime_i,   // 0 = unbounded; the field width still ends it
    input  wire [AGE_W-1:0]      spc_age_mark_i,   // 0 = no marker event
    input  wire [7:0]            spc_drag_i,       // U0.8, always applied
    input  wire signed [10:0]    spc_grav_i,       // always applied, on Y
    input  wire signed [10:0]    spc_strength_i,   // the recipe's k
    input  wire signed [17:0]    spc_cx_i,
    input  wire signed [17:0]    spc_cy_i,
    input  wire signed [17:0]    spc_cz_i,
    input  wire signed [10:0]    spc_p0_i,
    input  wire signed [10:0]    spc_p1_i,
    input  wire signed [10:0]    spc_p2_i,

    // ---- the bounded Field/FLOW acceleration sample ---------------------------
    // "Forces come from FIELD.SEQ.FLOW, never ad-hoc math." Bounded by its own
    // width: s11, the same units as velocity.
    input  wire                  fld_valid_i,
    input  wire signed [10:0]    fld_ax_i,
    input  wire signed [10:0]    fld_ay_i,
    input  wire signed [10:0]    fld_az_i,

    // ---- step 6 has NO PORTS HERE. Ruling I4 §1, 2026-09-19: `col_valid_i`,
    // ---- `col_vx_i`, `col_vy_i` and `col_vz_i` are RETIRED. PART.COLLIDE
    // ---- performs step 6 on this block's own output. See the header.

    // ---- step 7: curves are LOOKUPS, not evaluated polynomials ----------------
    // "A curve is authored content and a table keeps it exact." The bucket is
    // emitted combinationally from the ADVANCED age, so the table answers in
    // the same cycle, exactly as the species descriptor does.
    output wire [3:0]            crv_index_o,
    input  wire [5:0]            crv_size_i,
    input  wire [7:0]            crv_colour_i,

    // ---- the verdict, to PART.STATE's `vrd_*` channel -------------------------
    output wire                  out_valid_o,
    input  wire                  out_ready_i,
    output wire [REC_W-1:0]      out_record_o,
    output wire                  out_survive_o,
    output wire                  out_refused_o,

    // ---- the events, ACROSS PART.COLLIDE, to PART.SPAWN -----------------------
    // {death, collision, age marker, birth} — the ruling's four, in one beat
    // with the verdict so PART.SPAWN never has to join two streams.
    //
    // BIT 2 (collision) IS ZERO HERE, always. Ruling I4 §2 gives the collision
    // event to PART.COLLIDE, the only block that observes one; this vector
    // crosses it on the same register enable as the record and comes out with
    // bit 2 filled in. The bit stays in the vector rather than being deleted
    // because the FOUR EVENTS ARE FROZEN (owner ruling 2026-08-31 §2.4) and
    // "event order = bit order" is PART.SPAWN's determinism contract.
    output wire [3:0]            out_events_o,
    output wire                  out_colour_en_o,
    output wire [7:0]            out_colour_o,

    // ---- counters. A counter asserted zero and never seen to move is a claim;
    // ---- every one of these is fired by the directed test.
    output var logic [31:0]      particles_updated_o,
    output var logic [31:0]      particles_refused_o,
    output var logic [31:0]      particles_died_by_age_o,
    output var logic [31:0]      velocity_saturations_o,
    output var logic [31:0]      position_saturations_o,
    // `collisions_applied_o` was here and is RETIRED with the ports it counted
    // (ruling I4 §1). PART.COLLIDE's `collision_events_o` is the live one.

    // The contract asks for `recipe_histogram[12]`. Twelve 32-bit ports is 384
    // bits of boundary for a diagnostic, so it is a SELECT and a VALUE instead:
    // one mux, synthesizable, and the whole histogram is still readable.
    input  wire [3:0]            hist_sel_i,
    output wire [31:0]           hist_val_o
);

  // ---- the closed vocabulary, owner ruling 2026-08-31 §2.2 ------------------
  // Twelve operations. Ids 12..15 are NOT reserved for future use in the sense
  // that matters: they are UNKNOWN, and an unknown id refuses the particle.
  // R_INTEGRATE and F_COLLIDED are reported unused and they stay. The CLOSED
  // vocabulary and the ratified flag layout belong in the RTL that carries
  // them; `integrate` is selected by leaving the operand at zero and
  // `kPartCollidedThisTick` is written by position -- as a constant ZERO since
  // ruling I4 gave that bit to PART.COLLIDE -- so neither name appears in an
  // expression, which is a fact about how they are implemented, not a reason to
  // delete the law from the file.
  /* verilator lint_off UNUSEDPARAM */
  localparam logic [3:0] R_INTEGRATE = 4'd0;
  localparam logic [3:0] R_GRAVITY   = 4'd1;
  localparam logic [3:0] R_DRAG      = 4'd2;
  localparam logic [3:0] R_ATTRACT   = 4'd3;
  localparam logic [3:0] R_REPULSE   = 4'd4;
  localparam logic [3:0] R_ORBIT     = 4'd5;
  localparam logic [3:0] R_VORTEX    = 4'd6;
  localparam logic [3:0] R_WIND      = 4'd7;
  localparam logic [3:0] R_SHOCK     = 4'd8;
  localparam logic [3:0] R_SPLINE    = 4'd9;
  localparam logic [3:0] R_COLOUR    = 4'd10;
  localparam logic [3:0] R_SIZE      = 4'd11;
  localparam logic [3:0] R_LAST      = 4'd11;

  // Amendment C2's flag bits, by position rather than by mask, because this
  // block reads and writes individual ones.
  localparam int unsigned F_STUCK    = 0;   // kPartStuck
  localparam int unsigned F_COLLIDED = 1;   // kPartCollidedThisTick
  localparam int unsigned F_BORN     = 2;   // kPartBornThisTick
  localparam int unsigned F_RESERVED = 3;   // kPartFlagReserved: zero in, zero out
  /* verilator lint_on UNUSEDPARAM */

  // A comment is not a check.
  initial begin
    if (REC_W != 128)
      $fatal(1, "zhao_part_update: REC_W=%0d; the particle128 codec is 128 bits", REC_W);
    if (AGE_W != 10)
      $fatal(1, "zhao_part_update: AGE_W=%0d; amendment C2 fixes age at 10 bits", AGE_W);
    if (SPECIES_N < 1 || SPECIES_N > 128)
      $fatal(1, "zhao_part_update: SPECIES_N=%0d outside the 7-bit species field", SPECIES_N);
    if (RECIPE_SHIFT < 1 || RECIPE_SHIFT > 24)
      $fatal(1, "zhao_part_update: RECIPE_SHIFT=%0d; the rounding needs 1..24", RECIPE_SHIFT);
    if (SHOCK_SHIFT < 1 || SHOCK_SHIFT > 24)
      $fatal(1, "zhao_part_update: SHOCK_SHIFT=%0d; the rounding needs 1..24", SHOCK_SHIFT);
    if (F_RESERVED != 3)
      $fatal(1, "zhao_part_update: the flag layout moved");
  end

  // ---- the two arithmetic laws, stated once ---------------------------------
  // Round-half AWAY FROM ZERO. On the magnitude, so negatives mirror positives
  // instead of inheriting the arithmetic shift's floor.
  function automatic logic signed [31:0] rsh_round(input logic signed [31:0] v,
                                                   input integer sh);
    logic signed [31:0] mag;
    logic signed [31:0] q;
    begin
      mag       = v[31] ? -v : v;
      q         = (mag + (32'sd1 <<< (sh - 1))) >>> sh;
      rsh_round = v[31] ? -q : q;
    end
  endfunction

  // CLAMP, never wrap. 11'sh400 is the s11 floor (-1024); writing it as a
  // pattern rather than as `-11'sd1024` avoids a literal that does not fit its
  // own declared width.
  function automatic logic signed [10:0] sat11(input logic signed [31:0] v);
    begin
      if (v > 32'sd1023)       sat11 = 11'sd1023;
      else if (v < -32'sd1024) sat11 = 11'sh400;
      else                     sat11 = $signed(v[10:0]);
    end
  endfunction

  function automatic logic signed [17:0] sat18(input logic signed [31:0] v);
    begin
      if (v > 32'sd131071)       sat18 = 18'sd131071;
      else if (v < -32'sd131072) sat18 = 18'sh20000;
      else                       sat18 = $signed(v[17:0]);
    end
  endfunction

  // ---- sign extension, spelled out -----------------------------------------
  // SystemVerilog would widen these implicitly and the result would be right,
  // but "implicitly" is doing the load-bearing work in a sentence about signed
  // arithmetic and the reader cannot see the width the operator actually ran
  // at. `d = pos - centre` is the case that matters: at 18 bits it overflows
  // for a particle at one extreme and a centre at the other, at 19 it cannot,
  // and nothing in the source says which one happens unless it is written down.
  function automatic logic signed [18:0] sx19_18(input logic signed [17:0] v);
    begin sx19_18 = {v[17], v}; end
  endfunction
  function automatic logic signed [18:0] sx19_11(input logic signed [10:0] v);
    begin sx19_11 = {{8{v[10]}}, v}; end
  endfunction
  function automatic logic signed [31:0] sx32_11(input logic signed [10:0] v);
    begin sx32_11 = {{21{v[10]}}, v}; end
  endfunction
  function automatic logic signed [31:0] sx32_18(input logic signed [17:0] v);
    begin sx32_18 = {{14{v[17]}}, v}; end
  endfunction
  function automatic logic signed [31:0] sx32_30(input logic signed [29:0] v);
    begin sx32_30 = {{2{v[29]}}, v}; end
  endfunction

  // ---- the record, through the RATIFIED codec -------------------------------
  logic signed [17:0] pos_i_c [0:2];
  logic signed [10:0] vel_i_c [0:2];
  logic [AGE_W-1:0]   age_i_c;
  logic [6:0]         spc_i_c;
  logic [5:0]         siz_i_c;
  logic [5:0]         spn_i_c;
  logic [3:0]         flg_i_c;
  logic [7:0]         var_i_c;

  logic signed [17:0] pos_o_c [0:2];
  logic signed [10:0] vel_o_c [0:2];
  logic [AGE_W-1:0]   age_o_c;
  logic [5:0]         siz_o_c;
  logic [3:0]         flg_o_c;
  logic [127:0]       rec_packed_c;

  /* verilator lint_off PINCONNECTEMPTY */
  zhao_part_record u_codec (
      .rec_i       (in_record_i),
      .pos_x_o     (pos_i_c[0]),
      .pos_y_o     (pos_i_c[1]),
      .pos_z_o     (pos_i_c[2]),
      .vel_x_o     (vel_i_c[0]),
      .vel_y_o     (vel_i_c[1]),
      .vel_z_o     (vel_i_c[2]),
      .age_o       (age_i_c),
      .species_o   (spc_i_c),
      .size_o      (siz_i_c),
      .spin_o      (spn_i_c),
      .flags_o     (flg_i_c),
      .variation_o (var_i_c),

      .pos_x_i     (pos_o_c[0]),
      .pos_y_i     (pos_o_c[1]),
      .pos_z_i     (pos_o_c[2]),
      .vel_x_i     (vel_o_c[0]),
      .vel_y_i     (vel_o_c[1]),
      .vel_z_i     (vel_o_c[2]),
      .age_i       (age_o_c),
      .species_i   (spc_i_c),          // species never changes here
      .size_i      (siz_o_c),
      .spin_i      (spn_i_c),          // spin belongs to the renderer
      .flags_i     (flg_o_c),
      .variation_i (var_i_c),          // stateless: a running value would make
                                       // identity depend on iteration order
      .rec_o       (rec_packed_c),

      .base_radius_i (32'sd0),
      .radius_o      (),               // PART.LADDER's, not ours
      .angle16_o     ()
  );
  /* verilator lint_on PINCONNECTEMPTY */

  // The species index is offered unconditionally so the table is a pure read.
  assign spc_index_o = spc_i_c;

  // =========================================================================
  // STEP 1 — advance age, evaluate lifetime
  // =========================================================================
  wire                age_full_c = (age_i_c == {AGE_W{1'b1}});
  wire [AGE_W-1:0]    age_next_c = age_full_c ? age_i_c : (age_i_c + {{(AGE_W-1){1'b0}}, 1'b1});
  wire                life_out_c = (spc_lifetime_i != {AGE_W{1'b0}}) &&
                                   (age_next_c >= spc_lifetime_i);
  wire                died_c     = age_full_c || life_out_c;

  // ---- refusal, before any arithmetic --------------------------------------
  wire species_bad_c = (SPECIES_N < 128) && ({25'd0, spc_i_c} >= 32'(SPECIES_N));
  wire recipe_bad_c  = (spc_recipe_i > R_LAST);
  wire refuse_c      = species_bad_c || recipe_bad_c;

  // A refused or dead particle contributes identity through steps 3..7. That
  // is what "refused, and no motion applied" and "a dead particle emits no
  // further motion" mean when they are wired rather than described.
  wire motion_c = !refuse_c && !died_c;

  // =========================================================================
  // STEP 2 — species constants and the external Field/wind input
  // =========================================================================
  logic signed [17:0] ctr_c [0:2];
  logic signed [10:0] par_c [0:2];
  logic signed [10:0] fld_a_c [0:2];
  always_comb begin
    ctr_c[0]   = spc_cx_i;  ctr_c[1]   = spc_cy_i;  ctr_c[2]   = spc_cz_i;
    par_c[0]   = spc_p0_i;  par_c[1]   = spc_p1_i;  par_c[2]   = spc_p2_i;
    fld_a_c[0] = fld_ax_i;  fld_a_c[1] = fld_ay_i;  fld_a_c[2] = fld_az_i;
  end

  // d = pos - centre, in s19 so the difference of two s18 cannot overflow.
  logic signed [18:0] d_c [0:2];
  always_comb begin
    for (int i = 0; i < 3; i = i + 1) d_c[i] = sx19_18(pos_i_c[i]) - sx19_18(ctr_c[i]);
  end

  // =========================================================================
  // STEP 3 — the selected recipe.
  //
  // The case selects the multiplier's OPERAND. It contains no multiply, which
  // is what keeps the vocabulary at three products instead of one per arm.
  // =========================================================================
  logic signed [18:0] mul_a_c [0:2];
  always_comb begin
    for (int i = 0; i < 3; i = i + 1) mul_a_c[i] = 19'sd0;
    case (spc_recipe_i)
      R_DRAG:     for (int i = 0; i < 3; i = i + 1) mul_a_c[i] = -sx19_11(vel_i_c[i]);
      R_ATTRACT:  for (int i = 0; i < 3; i = i + 1) mul_a_c[i] = -d_c[i];
      R_REPULSE,
      R_SHOCK:    for (int i = 0; i < 3; i = i + 1) mul_a_c[i] =  d_c[i];
      R_WIND:     for (int i = 0; i < 3; i = i + 1) mul_a_c[i] =  sx19_11(par_c[i]);
      R_SPLINE:   for (int i = 0; i < 3; i = i + 1)
                    mul_a_c[i] = sx19_11(par_c[i]) - sx19_11(vel_i_c[i]);
      R_ORBIT: begin
        mul_a_c[0] = -d_c[2];
        mul_a_c[2] =  d_c[0];
      end
      R_VORTEX: begin
        mul_a_c[0] = -d_c[2] - (d_c[0] >>> 1);
        mul_a_c[2] =  d_c[0] - (d_c[2] >>> 1);
      end
      // R_INTEGRATE, R_GRAVITY, R_COLOUR, R_SIZE and every unknown id leave the
      // operand at zero. Gravity's acceleration is a constant and does not need
      // the multiplier; the curves act at step 7.
      default: begin end
    endcase
  end

  // THE THREE RECIPE PRODUCTS. s19 * s11 -> s30, sized exactly so the fitter
  // infers a 19x11 and not a 32x32.
  logic signed [29:0] mul_p_c [0:2];
  always_comb begin
    for (int i = 0; i < 3; i = i + 1) mul_p_c[i] = mul_a_c[i] * spc_strength_i;
  end

  logic signed [31:0] acc_r_c [0:2];
  always_comb begin
    for (int i = 0; i < 3; i = i + 1)
      acc_r_c[i] = (spc_recipe_i == R_SHOCK) ? rsh_round(sx32_30(mul_p_c[i]), SHOCK_SHIFT)
                                             : rsh_round(sx32_30(mul_p_c[i]), RECIPE_SHIFT);
    if (spc_recipe_i == R_GRAVITY) begin
      acc_r_c[0] = 32'sd0;
      acc_r_c[1] = sx32_11(spc_strength_i);   // the sign is the content's
      acc_r_c[2] = 32'sd0;
    end else if (spc_recipe_i == R_VORTEX) begin
      acc_r_c[1] = rsh_round(sx32_11(spc_strength_i), 2);  // lift
    end
  end

  // The always-applied species gravity and the Field sample join here, so a
  // Field acceleration of zero is BIT-IDENTICAL to no Field at all.
  logic signed [31:0] acc_t_c [0:2];
  always_comb begin
    for (int i = 0; i < 3; i = i + 1) begin
      acc_t_c[i] = 32'sd0;
      if (motion_c) begin
        acc_t_c[i] = acc_r_c[i];
        if (fld_valid_i) acc_t_c[i] = acc_t_c[i] + sx32_11(fld_a_c[i]);
        if (i == 1)      acc_t_c[i] = acc_t_c[i] + sx32_11(spc_grav_i);
      end
    end
  end

  // =========================================================================
  // STEP 4 — drag, on the OLD velocity.
  //
  // vel * (256 - drag) / 256. The product can be at most 1024*256 = 262144, so
  // the rounded quotient is always inside s11 and sat11 below is belt and
  // braces rather than a live clamp.
  // =========================================================================
  // TEN bits, not nine. `256` needs a sign bit above it: as `9'sd256` the
  // literal is 0b1_0000_0000, which in nine signed bits IS -256, and a drag
  // coefficient of zero then NEGATES every velocity in the frame. It read as a
  // plausible width -- the value's range is 1..256, which is 9 bits of
  // magnitude -- and it cost a full run of the directed suite to see.
  wire signed [9:0] drag_k_c = 10'sd256 - $signed({2'b00, spc_drag_i});
  logic signed [20:0] drag_p_c [0:2];
  logic signed [10:0] vel_d_c  [0:2];
  always_comb begin
    for (int i = 0; i < 3; i = i + 1) begin
      drag_p_c[i] = vel_i_c[i] * drag_k_c;
      vel_d_c[i]  = motion_c ? sat11(rsh_round({{11{drag_p_c[i][20]}}, drag_p_c[i]}, 8))
                             : vel_i_c[i];
    end
  end

  // =========================================================================
  // STEP 5 — integrate velocity, then position with the NEW velocity.
  //
  // pos and vel share their eight fractional bits (amendment C2), so this is
  // an add and nothing else. Both clamp; neither wraps.
  // =========================================================================
  logic signed [31:0] vel_s_c [0:2];
  logic signed [10:0] vel_n_c [0:2];
  logic               vsat_c  [0:2];
  logic signed [31:0] pos_s_c [0:2];
  logic signed [17:0] pos_n_c [0:2];
  logic               psat_c  [0:2];
  always_comb begin
    for (int i = 0; i < 3; i = i + 1) begin
      vel_s_c[i] = sx32_11(vel_d_c[i]) + acc_t_c[i];
      vel_n_c[i] = sat11(vel_s_c[i]);
      vsat_c[i]  = motion_c && (vel_s_c[i] != sx32_11(vel_n_c[i]));
      pos_s_c[i] = motion_c ? (sx32_18(pos_i_c[i]) + sx32_11(vel_n_c[i]))
                            : sx32_18(pos_i_c[i]);
      pos_n_c[i] = sat18(pos_s_c[i]);
      psat_c[i]  = motion_c && (pos_s_c[i] != sx32_18(pos_n_c[i]));
    end
  end

  // =========================================================================
  // STEP 6 — PART.COLLIDE'S, WHOLE. Ruling I4 §1, 2026-09-19.
  //
  // There is no mux and no port here any more. The velocity that leaves this
  // block is the integrated one; PART.COLLIDE, immediately downstream, resolves
  // at most one contact on it and writes the responded velocity, the contact
  // placement and the collided flag into the record it hands on. The order is
  // preserved in the pipeline rather than inside one module.
  // =========================================================================

  // =========================================================================
  // STEP 7 — the curves, as LOOKUPS.
  // =========================================================================
  assign crv_index_o = age_next_c[AGE_W-1 -: 4];
  wire colour_en_c = motion_c && (spc_recipe_i == R_COLOUR);
  wire [7:0] colour_c = colour_en_c ? crv_colour_i : 8'd0;

  always_comb begin
    siz_o_c = (motion_c && (spc_recipe_i == R_SIZE)) ? crv_size_i : siz_i_c;
    age_o_c = refuse_c ? age_i_c : age_next_c;
    // The born flag is CLEARED on the tick that reports it, so the birth event
    // fires exactly once however many ticks the particle lives. The reserved
    // bit is driven to zero, per "zero in, preserved zero". kPartCollidedThisTick
    // is ZERO here and PART.COLLIDE writes it every beat downstream -- one
    // author for the bit, which is what stops the two disagreeing.
    flg_o_c = refuse_c ? flg_i_c : {1'b0, 1'b0, 1'b0, flg_i_c[F_STUCK]};
    for (int i = 0; i < 3; i = i + 1) begin
      pos_o_c[i] = pos_n_c[i];
      vel_o_c[i] = vel_n_c[i];
    end
  end

  // =========================================================================
  // STEP 8 — the deterministic events, {death, collision, marker, birth}.
  // =========================================================================
  wire ev_birth_c = motion_c && flg_i_c[F_BORN];
  wire ev_mark_c  = motion_c && (spc_age_mark_i != {AGE_W{1'b0}}) &&
                    (age_next_c == spc_age_mark_i);
  wire ev_death_c = !refuse_c && died_c;
  // Bit 2 is the COLLISION event and it leaves here ZERO: this block cannot
  // observe a collision, which is the whole of ruling I4. PART.COLLIDE fills it.
  wire [3:0] events_c = {ev_death_c, 1'b0, ev_mark_c, ev_birth_c};

  // A refused particle leaves BIT-IDENTICAL, by a mux rather than by trusting
  // a codec round trip: "refused, and no motion applied" is exact or it is not
  // the rule.
  wire [127:0] rec_next_c = refuse_c ? in_record_i : rec_packed_c;
  wire         survive_c  = !refuse_c && !died_c;

  // =========================================================================
  // The one-deep output register. Fixed latency 1, one particle per clock
  // while the consumer is ready, and a stall is safe at any point because the
  // particle carries its own state.
  // =========================================================================
  logic         out_v_q;
  logic [127:0] out_rec_q;
  logic         out_surv_q;
  logic         out_ref_q;
  logic [3:0]   out_ev_q;
  logic         out_cen_q;
  logic [7:0]   out_col_q;

  assign in_ready_o      = !out_v_q || out_ready_i;
  assign out_valid_o     = out_v_q;
  assign out_record_o    = out_rec_q;
  assign out_survive_o   = out_surv_q;
  assign out_refused_o   = out_ref_q;
  assign out_events_o    = out_ev_q;
  assign out_colour_en_o = out_cen_q;
  assign out_colour_o    = out_col_q;

  wire accept_c = in_valid_i && in_ready_o;

  // 0..3 axes saturated this particle. Counted per AXIS, because "if this is
  // ever large the scale question was answered wrongly" is a question about
  // how much saturation there is, not how many particles touched a rail.
  wire [1:0] vsat_n_c = {1'b0, vsat_c[0]} + {1'b0, vsat_c[1]} + {1'b0, vsat_c[2]};
  wire [1:0] psat_n_c = {1'b0, psat_c[0]} + {1'b0, psat_c[1]} + {1'b0, psat_c[2]};

  // recipe_histogram. Sixteen bins for a twelve-entry vocabulary so a variable
  // index can never read off the end; 12..15 are unreachable because an
  // unknown id refuses before the bump.
  logic [31:0] hist_q [0:15];
  assign hist_val_o = hist_q[hist_sel_i];

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      // "Reset abandons the particle in flight; no state survives it, because
      // every value this block needs arrives with the particle or from the
      // species table."
      out_v_q    <= 1'b0;
      out_rec_q  <= 128'd0;
      out_surv_q <= 1'b0;
      out_ref_q  <= 1'b0;
      out_ev_q   <= 4'd0;
      out_cen_q  <= 1'b0;
      out_col_q  <= 8'd0;
      particles_updated_o     <= 32'd0;
      particles_refused_o     <= 32'd0;
      particles_died_by_age_o <= 32'd0;
      velocity_saturations_o  <= 32'd0;
      position_saturations_o  <= 32'd0;
      for (int b = 0; b < 16; b = b + 1) hist_q[b] <= 32'd0;
    end else begin
      if (out_v_q && out_ready_i) out_v_q <= 1'b0;

      if (accept_c) begin
        out_v_q    <= 1'b1;
        out_rec_q  <= rec_next_c;
        out_surv_q <= survive_c;
        out_ref_q  <= refuse_c;
        out_ev_q   <= events_c;
        out_cen_q  <= colour_en_c;
        out_col_q  <= colour_c;

        if (refuse_c) begin
          particles_refused_o <= particles_refused_o + 32'd1;
        end else begin
          particles_updated_o  <= particles_updated_o + 32'd1;
          hist_q[spc_recipe_i] <= hist_q[spc_recipe_i] + 32'd1;
          if (died_c)      particles_died_by_age_o <= particles_died_by_age_o + 32'd1;
          velocity_saturations_o <= velocity_saturations_o + {30'd0, vsat_n_c};
          position_saturations_o <= position_saturations_o + {30'd0, psat_n_c};
        end
      end
    end
  end

`ifdef ZHAO_ASSERT
  logic         armed_q;
  logic         held_q;
  logic [127:0] held_rec_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      armed_q    <= 1'b0;
      held_q     <= 1'b0;
      held_rec_q <= 128'd0;
    end else begin
      armed_q    <= 1'b1;
      held_q     <= out_v_q && !out_ready_i;
      held_rec_q <= out_rec_q;
    end
  end

  always_ff @(posedge clk) begin
    if (armed_q) begin
      // Handshake hygiene: a beat that was not taken is still there, unchanged.
      a_out_stable: assert (!held_q || (out_v_q && (out_rec_q == held_rec_q)));

      // A refusal is never a survivor. The two fields are written by the same
      // accept, so this catches a polarity inversion and not a timing skew --
      // which is what it is for.
      a_refused_never_survives: assert (!out_v_q || !out_ref_q || !out_surv_q);

      // A death event and a survival cannot both be true.
      a_death_excludes_survival: assert (!out_v_q || !out_ev_q[3] || !out_surv_q);

      // The birth flag is reported once and cleared. Bit 118 is
      // kPartBornThisTick at offset 116 + 2.
      a_born_cleared: assert (!out_v_q || out_ref_q || !out_rec_q[118]);

      // "zero in, preserved zero" -- bit 119 is kPartFlagReserved.
      a_reserved_zero: assert (!out_v_q || out_ref_q || !out_rec_q[119]);
    end
  end
`endif

endmodule : zhao_part_update

`default_nettype wire
