// zhao_part_collide.sv — PART.COLLIDE: one collision response per particle
// per tick, against the LIVE deformed terrain and one plane.
//
// Law, in citation order:
//   design/contracts/PART.COLLIDE.md — the block contract, owner ZH-063, phase 10.
//   Owner ruling 2026-08-31 §2.3 — the five-response enum, FROZEN.
//   Amendment C2 / ruling R3 — the particle128 v1 record, FROZEN. The codec is
//     `zhao_part_record` and this block INSTANTIATES it rather than restating
//     the bit offsets, because "the bit positions want to exist in exactly one
//     place" and that place is not here.
//
// ---------------------------------------------------------------------------
// THERE IS NO ORACLE FOR THIS BLOCK, AND THE LEDGER SAYS THERE IS
// ---------------------------------------------------------------------------
// `design/blocks.yml` declares `reference_model: zref::ParticleCollide`. That
// symbol does not exist anywhere in the tree and is entry 10 of
// `reports/PHANTOM_REFERENCES.md`. So this RTL is written from the contract's
// prose, and `tests/particles/part_collide_directed.cpp` is a SELF-CONSISTENCY
// check against that prose — not a differential test against a ratified law.
// Nothing here should be read as "checked against the oracle"; there is none.
//
// ---------------------------------------------------------------------------
// THE TABLE DECISION (standing owner direction: ALMs are the binding
// constraint; prefer a lookup over computation)
// ---------------------------------------------------------------------------
// WHAT IS A TABLE HERE, and it is the one that matters:
//
//   The five responses do NOT get five arithmetic arms. `rsp_rom()` is a ROM
//   indexed by the response enum that emits four control bits and two
//   coefficient SELECTORS, and there is exactly ONE velocity datapath:
//
//       v' = tan * v_tangential + nrm * v_normal
//          = tan * v + (nrm - tan) * (v.n) * n
//
//   with (tan, nrm) read from the table:
//
//       IGNORE  (-,      -     )   no response at all
//       DIE     (-,      -     )   removed; not moved
//       STICK   (0,      0     )   velocity zeroed
//       SLIDE   (friction,   0 )   inward normal component removed, friction on
//                                  the tangential part
//       BOUNCE  (damping, -restitution)  normal component reflected by
//                                  restitution, tangential damped
//
//   A five-arm case statement over three-axis fixed-point arithmetic is several
//   hundred ALMs of duplicated adders and muxes. A nine-bit ROM plus one
//   datapath is a handful. That is the lookup-for-computation trade the standing
//   direction asks for, applied where the ALMs actually are.
//
// WHAT IS DELIBERATELY *NOT* A TABLE, with the reason, because applying the
// direction mechanically here would be worse than not applying it:
//
//   1. THE SPECIES DESCRIPTOR IS NOT STORED HERE. The contract's memory
//      ownership section is "**None.** ... This block owns no memory and
//      performs no fetch". The species table belongs to PART.STATE. A
//      descriptor M10K here would be a second copy of somebody else's memory.
//      The descriptor arrives as ports.
//
//      Amended 2026-09-19: it also LEAVES as a port. `d_index_o` publishes the
//      species this block decoded, so the owner that serves `d_*_i` has an
//      address without anyone decoding the record a second time. That owner is
//      now `zhao_part_table` (PART.TABLE), not PART.STATE -- four contracts
//      handed the table to each other and none implemented it; see that file's
//      header. Nothing about "no memory here" changes.
//
//   2. THE MULTIPLIERS ARE NOT QUARTER-SQUARE TABLES. The obvious next move is
//      a*b = ((a+b)^2 - (a-b)^2)/4 with the squares in M10K. This block has ~10
//      products; at these widths each needs a ~2048x22b square ROM and two read
//      ports, so the swap costs roughly 100 M10Ks to remove ~10 DSPs. DSPs cost
//      approximately ZERO ALMs. It would spend a fifth of the device's memory
//      and buy nothing on the constraint that is 15x over. Written down so the
//      next reader does not have to re-derive it, and so "we did not table the
//      multipliers" reads as a decision rather than an omission.
//
// ---------------------------------------------------------------------------
// FORMATS — ratified, not chosen here
// ---------------------------------------------------------------------------
// From `reference/include/zref/zref_particle.hpp` (amendment C2 / ruling R3):
//
//     pos  s18  S 9.8 m   relative to the population origin   -> 1 LSB = 1/256 m
//     vel  s11  S 2.8 m/tick                                  -> 1 LSB = 1/256 m
//
// The two scales AGREE, and that is load-bearing twice over: a tick of velocity
// v displaces the position by exactly v LSBs, so (a) terrain height and position
// are directly comparable, and (b) the `already_inside_at_entry` test below can
// compare a penetration depth against a velocity with no scale factor at all.
//
// PART.UPDATE.md still says these scales are "Class C and the ruling did not
// make them". That text predates amendment C2 and is stale; the ratified header
// is the law followed here.
//
// Everything else is a NAMED, EDITABLE PARAMETER — normal format, coefficient
// format, clearance. None of it is generated from anything.
//
// ---------------------------------------------------------------------------
// THE ANTI-JITTER PROPERTY AND ITS HONEST BOUND
// ---------------------------------------------------------------------------
// "A contact must not leave the particle inside the surface ... the contact
// point is computed so that re-testing it in the same tick would not report a
// contact."
//
// The placement is p' = p + (EPS - d) * the axis the contact was MEASURED on,
// and that phrase is the whole of it. Measuring on one axis and pushing along
// another is what breaks the property; see the terrain paragraph in the body,
// where the "more correct" sloped form leaves the particle still inside.
//
//   * HEIGHTFIELD: measured vertically, pushed vertically. The particle lands
//     at h + CLEAR_EPS and the re-test is exact — no rounding at all.
//   * AXIS-ALIGNED PLANE: exact for the same reason.
//   * TILTED PLANE: measured along n, pushed along n, so the re-test is EPS
//     up to two bounded errors — per-axis integer rounding of the displacement
//     (at most 0.5*sum|n_i| <= 0.866 position LSBs) and |n|^2 = 1 +/- delta for
//     a normal quantised to Q1.NRM_Q (delta ~ sqrt(3)*2^-NRM_Q). With the
//     defaults the clearance survives while the entry penetration stays under
//     roughly CLEAR_EPS/delta ~ 1,100 position LSBs (~4.3 m). Deeper than that
//     and a tilted-plane placement can still be marginally inside — and
//     `already_inside_at_entry_o` is exactly the counter that says so. Stated
//     rather than assumed away.
//
// ---------------------------------------------------------------------------
// THIS BLOCK EMITS THE COLLISION EVENT. RULED 2026-09-19.
// ---------------------------------------------------------------------------
// `reports/RULING-I4-COLLISION-SPAWN-20260919.md`. Three ratified contracts had
// formed a closed contradiction: PART.SPAWN's four FROZEN events (owner ruling
// 2026-08-31 §2.4) include COLLISION and were said to arrive "from PART.UPDATE";
// PART.UPDATE's inputs contain no collision velocity; and this block is a leaf
// downstream of it with no return edge. PART.UPDATE therefore had to announce a
// collision it cannot observe, and in `zhao_console_core` the seam that was cut
// to escape that was tied to zero -- so SPAWN-ON-COLLISION, sparks on impact,
// did not work in this console and its counter read zero exactly as it would
// have if nothing had ever collided.
//
// The ruling gives the event to the block that observes the collision. THE
// EVENT'S CONTENT IS UNCHANGED; only its announced origin was wrong.
//
// Three things make this cheap rather than new machinery:
//
//   1. `p_events_i` -> `c_events_o` CARRIES PART.UPDATE'S OTHER THREE BITS
//      ACROSS, on the SAME register enable as the record. That is deliberate
//      and it is the point: a vector loaded by a second enable can separate
//      from the record it describes on a stall, and deliver one particle's
//      events with another particle's position. One enable, no join, nothing to
//      drift across.
//   2. BIT 2 IS OVERWRITTEN, not OR-ed. PART.UPDATE drives it zero, so the two
//      forms are identical today -- and overwriting says there is exactly ONE
//      author for the bit, which is what stops a future edit making two.
//   3. The bit is `respond_c`, THE SAME WIRE that writes `kPartCollidedThisTick`
//      into the record below. The announced event and the recorded flag are one
//      decision, so they cannot disagree.
//
// WHERE THE CHILD IS PLACED -- the one question no contract answered, decided by
// ruling §3 and REVERSIBLE BY ONE EDIT. `CHILD_AT_POST_CONTACT` selects what
// PART.SPAWN receives as the parent record:
//
//   1 (default)  `c_record_o` -- the POST-CONTACT particle, at `pout_c`. The
//                ruling's reasons: it is what this block already computes so no
//                second placement law enters the tree; pre-contact puts the
//                spark inside the surface the particle just hit, which is the
//                visible artefact the whole collision response exists to
//                remove; and `kPartStuck` already describes a particle at rest
//                ON the surface, so the rest of the record agrees with it.
//   0            the record exactly as PART.UPDATE handed it over, pre-contact.
//
// At the default this costs NOTHING -- `c_spawn_record_o` is `c_record_o`, the
// same wires. Only the reversal pays for a second register, which is the right
// way round. CLAUDE.md's art law applies to the choice itself: it is to be
// judged by looking at sparks in motion, not by reasoning about it further.
//
// ---------------------------------------------------------------------------
// SYNTHESIZABILITY
// ---------------------------------------------------------------------------
// This file has NOT been through `quartus_map`. Verilator lint-clean is one
// tool's opinion and is not evidence of synthesizability (CLAUDE.md). The
// Quartus 17.0 forms are respected on purpose: the elaboration guard is inside
// `initial begin ... end` and there is no implicit generate anywhere.
//
// The whole datapath is combinational into ONE output register, which is what
// makes it one beat per clock with a fixed one-cycle latency. Three chained
// multiplies in a single clock is an Fmax question and only a fit can answer
// it; if it misses, the split is between `vn` and `kv` and costs one cycle of
// latency, not any behaviour.
//
// Conservative SystemVerilog subset only (charter §2).
`default_nettype none

module zhao_part_collide #(
    parameter int unsigned REC_W     = 128,

    // FROZEN by amendment C2 / ruling R3. Parameters rather than literals so
    // the elaboration guard below has something to check, NOT so they can be
    // changed: `zhao_part_record` hardcodes both.
    parameter int unsigned POS_W     = 18,
    parameter int unsigned VEL_W     = 11,

    // Unit-normal component, signed Q1.NRM_Q. TERRAIN.PATCH's sample and the
    // plane both arrive in this format. Owner knob.
    parameter int unsigned NRM_W     = 12,
    parameter int unsigned NRM_Q     = 10,

    // Descriptor coefficients, signed "fx16" per the contract: Q1.FX_Q.
    // Q1.14 spans +/-2.0, which leaves room for a restitution above 1 (a
    // superball) rather than silently forbidding it. Owner knob.
    parameter int unsigned FX_W      = 16,
    parameter int unsigned FX_Q      = 14,

    // How far OUTSIDE the surface the contact point is placed, in position
    // LSBs (1 LSB = 1/256 m). 2 LSBs ~= 7.8 mm. This is the anti-jitter
    // margin; see the bound above. Owner knob.
    parameter int unsigned CLEAR_EPS = 2,

    // WHERE A COLLISION-SPAWNED CHILD IS PLACED. Owner ruling I4 §3,
    // 2026-09-19: POST-CONTACT, and reversible by changing this one bit.
    // See the header paragraph "WHERE THE CHILD IS PLACED".
    parameter bit CHILD_AT_POST_CONTACT = 1'b1
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- the updated particle, from PART.UPDATE ------------------------------
    input  var logic                    p_valid_i,
    output var logic                    p_ready_o,
    input  var logic [REC_W-1:0]        p_record_i,

    // PART.UPDATE's own three events, riding beside the record it describes.
    // {death, collision, age marker, birth}; bit 2 arrives ZERO and this block
    // fills it. Ruling I4 §2 -- see the header.
    //
    // BIT 2 IS DELIBERATELY NOT READ, and Verilator is right to notice. The
    // alternative spelling `p_events_i | {1'b0, respond_c, 2'b0}` would consume
    // it and lint silently -- and it would make the collision bit have TWO
    // authors the moment anyone upstream set it, which is the arrangement this
    // ruling exists to end. The waiver is narrow, it names the bit, and
    // `a_upstream_collision_bit_zero` below asserts in simulation that the bit
    // really does arrive zero, so the claim is checked rather than assumed.
    /* verilator lint_off UNUSEDSIGNAL */
    input  var logic [3:0]              p_events_i,
    /* verilator lint_on UNUSEDSIGNAL */

    // ---- the species descriptor, sampled with the particle -------------------
    // The response is SELECTED EXPLICITLY by the descriptor. Owner ruling
    // 2026-08-31 §2.3: "Hardware must not guess from speed, colour, particle
    // size or material" — so nothing below reads any of those.
    //
    // `d_index_o` SAYS WHICH SPECIES THE FOUR INPUTS BELOW MUST DESCRIBE.
    // Added 2026-09-19, composing PART.TABLE (core header I2/I3). Until then
    // this block took a descriptor and never said whose, so a table serving it
    // had no address — and the only alternative was for the COMPOSER to slice
    // `species` out of `p_record_i` itself. That is a second decode of the
    // frozen layout invented in `zhao_console_core.sv`, which that file's own
    // header exists to refuse; and it would be a second reader of a field
    // THIS block has already read. So the port is the honest fix: it publishes
    // the index the block is ALREADY using, off its own `zhao_part_record`
    // instance, and nothing new is computed anywhere.
    //
    // IT IS COMBINATIONAL OFF `p_record_i`, deliberately, and that is what
    // makes it safe rather than a metadata-swap hazard. The four `d_*_i`
    // inputs are read in the same instant as the record (the contract's
    // "sampled with the particle"); an index taken from THE SAME WIRE in THE
    // SAME instant cannot move apart from it, because there is no register
    // between them for a stall to open. A REGISTERED index would reintroduce
    // exactly the two-operands-that-move-together defect CLAUDE.md records.
    //
    // It costs no logic: `u_spc` already exists and already fans out to the
    // codec's re-pack input.
    // `wire`, matching `c_spawn_record_o` below: it is a continuous assignment
    // from an existing net, not state, and the wire form is the one this file
    // has already put through both tools for exactly that shape.
    output wire [6:0]                   d_index_o,
    input  var logic [2:0]              d_response_i,
    input  var logic signed [FX_W-1:0]  d_restitution_i,
    input  var logic signed [FX_W-1:0]  d_friction_i,
    input  var logic signed [FX_W-1:0]  d_damping_i,

    // ---- the LIVE deformed terrain, sampled from TERRAIN.PATCH ---------------
    // Sampled WITH the particle beat, so the surface tested is the one that
    // exists this tick, crater and all. `t_valid_i` low means the sample did
    // not arrive: no contact, counted, never a stall.
    input  var logic                    t_valid_i,
    input  var logic signed [POS_W-1:0] t_height_i,   // S 9.8 m, the +Y axis
    input  var logic signed [NRM_W-1:0] t_nx_i,
    input  var logic signed [NRM_W-1:0] t_ny_i,
    input  var logic signed [NRM_W-1:0] t_nz_i,

    // ---- the ground's own vertical rate at the sample ----------------------
    // NEW 2026-09-26 (TERRVEL). This is terrain_rules 4.4's velocity lane,
    // composed by TERRAIN.VELOCITY from the same Earth evaluation that moved
    // the height beside it, interpolated by 4.3 in zhao_part_terrain_tap with
    // the SAME triangle pick as t_height_i.
    //
    // THIS IS THIS BLOCK'S OWN CONTRACT BEING CARRIED OUT, NOT A NEW LAW.
    // design/contracts/PART.COLLIDE.md says, in as many words: "Moving terrain
    // bodies, when they arrive: body surface velocity enters the
    // relative-velocity calculation. The ruling names this explicitly so that
    // the oracle is written with a relative-velocity form now, rather than an
    // absolute-velocity form that would have to be rewritten later." The RTL
    // shipped the absolute form the paragraph warned against; this is the
    // repair, and terrain deforming under a live TerrainField IS the moving
    // surface it was waiting for.
    //
    // NOTHING CHANGES WITHOUT A LIVE FIELD, AND THAT IS STRUCTURAL RATHER THAN
    // HOPED FOR. TERRAIN.VELOCITY's law V2 makes the lattice word EXACTLY zero
    // at any vertex no field lane covers, so `t_vy_i` is 0 over still ground
    // and every expression below collapses to the one that was here before,
    // bit for bit. `t_vy_valid_i` low does the same. That is why no existing
    // particle test moves.
    //
    // UNITS: s11, 1 LSB = 1/256 m per tick -- the SAME scale as `vel` in the
    // FORMATS block above. The tap does the saturating narrow and counts it.
    input  var logic signed [VEL_W-1:0] t_vy_i,
    input  var logic                    t_vy_valid_i,

    // ---- the plane -----------------------------------------------------------
    // The contract says planes "arrive as parameters". They are PORTS here:
    // strictly more general, the same silicon, and it keeps the owner's control
    // over a per-frame value instead of freezing it at elaboration.
    //
    // ONE plane. The contract's v1 list says "simple planes", plural; each
    // additional plane is a full three-product dot and a comparator, and ALMs
    // are 15x over. A second plane is a `generate` over `plane_test` when the
    // owner asks for it, and is deliberately not carried unasked.
    input  var logic                    pl_en_i,
    input  var logic signed [NRM_W-1:0] pl_nx_i,
    input  var logic signed [NRM_W-1:0] pl_ny_i,
    input  var logic signed [NRM_W-1:0] pl_nz_i,
    input  var logic signed [31:0]      pl_c_i,       // Q NRM_Q; surface is n.p = c

    // ---- out -----------------------------------------------------------------
    output var logic                    c_valid_o,
    input  var logic                    c_ready_i,
    output var logic [REC_W-1:0]        c_record_o,
    output var logic                    c_alive_o,    // low: remove (DIE / refused)
    output var logic                    c_contact_o,
    output var logic [2:0]              c_response_o,
    output var logic                    c_refused_o,

    // ---- to PART.SPAWN --------------------------------------------------------
    // The four FROZEN events with bit 2 now filled in, and the parent record the
    // child is derived from. Both leave on the same beat as `c_record_o` and are
    // loaded by the same enable, so they cannot separate from it.
    output var logic [3:0]              c_events_o,
    output wire [REC_W-1:0]             c_spawn_record_o,

    // ---- counters ------------------------------------------------------------
    // The contract's list, one port each. Faults leave the module; a counter
    // that is asserted zero and never seen to move is not evidence.
    output var logic [31:0] contacts_ignore_o,
    output var logic [31:0] contacts_die_o,
    output var logic [31:0] contacts_stick_o,
    output var logic [31:0] contacts_slide_o,
    output var logic [31:0] contacts_bounce_o,
    output var logic [31:0] contacts_terrain_o,
    output var logic [31:0] contacts_plane_o,
    output var logic [31:0] already_inside_at_entry_o,
    output var logic [31:0] terrain_sample_unavailable_o,
    output var logic [31:0] response_refused_o,
    // The instrument for the EVENT this block now emits. It is wired to the
    // event gate, not to the response classification, and that is the whole
    // reason it exists beside the five `contacts_*` counters whose sum it
    // currently equals: if the two ever stop agreeing, the difference is the
    // bug, and a counter derived from the other five could never show it.
    // It is also what drives the console's `part_collisions_applied_o`, which
    // before ruling I4 was a counter that could not move.
    output var logic [31:0] collision_events_o,
    // Beyond the contract's list, deliberately: a response with a coefficient
    // above 1 can drive a component past its field, and a SILENT wrap would
    // reverse a particle's direction and read as a physics bug rather than a
    // numeric one — PART.UPDATE's contract says exactly that about velocity.
    // So this block clamps, and says when it clamped.
    output var logic [31:0] field_clamps_o,
    // Contacts resolved against ground that was actually MOVING. Not a
    // tautology of t_vy_valid_i: a present velocity plane over still ground is
    // valid and zero. This is the number that moves when a TerrainField moves,
    // measured at the last consumer in the chain.
    output var logic [31:0] contacts_moving_ground_o
);

  // The two frozen widths. `zhao_part_record` hardcodes 18 and 11, so a
  // parameter override here would silently truncate at the instance rather than
  // fail. Quartus 17.0 needs this inside `initial begin ... end`.
  initial begin
    if (POS_W != 18)
      $fatal(1, "zhao_part_collide: POS_W=%0d; ruling R3 freezes the position axis at 18 bits", POS_W);
    if (VEL_W != 11)
      $fatal(1, "zhao_part_collide: VEL_W=%0d; ruling R3 freezes the velocity axis at 11 bits", VEL_W);
    if (REC_W != 128)
      $fatal(1, "zhao_part_collide: REC_W=%0d; the particle128 record is 128 bits", REC_W);
    if (NRM_Q >= NRM_W)
      $fatal(1, "zhao_part_collide: NRM_Q=%0d needs a sign bit inside NRM_W=%0d", NRM_Q, NRM_W);
    if (FX_Q >= FX_W)
      $fatal(1, "zhao_part_collide: FX_Q=%0d needs a sign bit inside FX_W=%0d", FX_Q, FX_W);
    if (CLEAR_EPS == 0)
      $fatal(1, "zhao_part_collide: CLEAR_EPS=0 places the contact point ON the surface, which re-tests as a contact");
  end

  // ---- derived widths -------------------------------------------------------
  // Written from the parameters so a format knob moves them together.
  localparam int unsigned DIST_W = POS_W + NRM_W + 4;      // signed distance, Q NRM_Q
  localparam int unsigned VN_W   = VEL_W + NRM_W + 3;      // v.n,              Q NRM_Q
  localparam int unsigned COEF_W = FX_W + 1;               // a coefficient, negatable
  localparam int unsigned K_W    = FX_W + 2;               // nrm - tan
  localparam int unsigned KV_W   = K_W + VN_W;             // (nrm-tan) * v.n
  localparam int unsigned VSH    = FX_Q + 2*NRM_Q;         // velocity result binary point
  localparam int unsigned ACC_W  = KV_W + NRM_W + 2;       // velocity accumulator
  localparam int unsigned PUSH_W = DIST_W + 2;             // EPS - d
  localparam int unsigned PSH    = 2*NRM_Q;                // displacement binary point
  localparam int unsigned DISP_W = PUSH_W + NRM_W + 1;     // push * n

  // ---- the response enum, FROZEN by owner ruling 2026-08-31 §2.3 ------------
  localparam logic [2:0] R_IGNORE = 3'd0;
  localparam logic [2:0] R_DIE    = 3'd1;
  localparam logic [2:0] R_STICK  = 3'd2;
  localparam logic [2:0] R_SLIDE  = 3'd3;
  localparam logic [2:0] R_BOUNCE = 3'd4;
  // 5..7 are NOT responses. They are refused and counted, never defaulted to
  // IGNORE: "a particle that should have died and instead flew on is a visible
  // bug with no error."

  // ---- the ratified flag bits ----------------------------------------------
  // Names and values from zref_particle.hpp, not invented here.
  localparam logic [3:0] FLG_STUCK    = 4'b0001;  // kPartStuck
  localparam logic [3:0] FLG_COLLIDED = 4'b0010;  // kPartCollidedThisTick
  // kPartBornThisTick (0x4) belongs to PART.SPAWN and kPartFlagReserved (0x8)
  // is "zero in, preserved zero". Neither is touched below.

  // ---- THE RESPONSE TABLE ---------------------------------------------------
  // Nine bits, indexed by the response enum. This is the block's one lookup and
  // it is what lets the five responses share a single arithmetic datapath.
  //
  //   [8]   set kPartStuck
  //   [7]   known response (0 => refuse and count)
  //   [6]   remove the particle
  //   [5]   place it at the contact point
  //   [4]   write the response velocity
  //   [3:2] tangential coefficient source
  //   [1:0] normal     coefficient source
  localparam int unsigned RSP_W = 9;
  localparam logic [1:0] SRC_ZERO = 2'd0;
  localparam logic [1:0] SRC_FRIC = 2'd1;
  localparam logic [1:0] SRC_DAMP = 2'd2;
  localparam logic [1:0] SRC_NEGE = 2'd3;  // -restitution

  function automatic logic [RSP_W-1:0] rsp_rom(input logic [2:0] r);
    case (r)
      //                stuck known kill place setv  tangential normal
      R_IGNORE: rsp_rom = {1'b0, 1'b1, 1'b0, 1'b0, 1'b0, SRC_ZERO, SRC_ZERO};
      R_DIE:    rsp_rom = {1'b0, 1'b1, 1'b1, 1'b0, 1'b0, SRC_ZERO, SRC_ZERO};
      R_STICK:  rsp_rom = {1'b1, 1'b1, 1'b0, 1'b1, 1'b1, SRC_ZERO, SRC_ZERO};
      R_SLIDE:  rsp_rom = {1'b0, 1'b1, 1'b0, 1'b1, 1'b1, SRC_FRIC, SRC_ZERO};
      R_BOUNCE: rsp_rom = {1'b0, 1'b1, 1'b0, 1'b1, 1'b1, SRC_DAMP, SRC_NEGE};
      default:  rsp_rom = {1'b0, 1'b0, 1'b0, 1'b0, 1'b0, SRC_ZERO, SRC_ZERO};
    endcase
  endfunction

  // ---- the codec, instantiated once, both directions ------------------------
  logic signed [17:0] u_px, u_py, u_pz;
  logic signed [10:0] u_vx, u_vy, u_vz;
  logic [9:0]         u_age;
  logic [6:0]         u_spc;
  logic [5:0]         u_siz, u_spn;
  logic [3:0]         u_flg;
  logic [7:0]         u_var;

  logic signed [17:0] w_px, w_py, w_pz;
  logic signed [10:0] w_vx, w_vy, w_vz;
  logic [3:0]         w_flg;
  logic [REC_W-1:0]   rec_out_c;

  // The codec also derives radius and angle. This block needs neither; the
  // radius law belongs to whoever draws the particle. Tied off and declared
  // unused rather than quietly re-derived somewhere else.
  /* verilator lint_off UNUSEDSIGNAL */
  logic signed [31:0] unused_radius;
  logic [15:0]        unused_angle16;
  /* verilator lint_on UNUSEDSIGNAL */

  zhao_part_record u_codec (
      .rec_i         (p_record_i),
      .pos_x_o       (u_px),
      .pos_y_o       (u_py),
      .pos_z_o       (u_pz),
      .vel_x_o       (u_vx),
      .vel_y_o       (u_vy),
      .vel_z_o       (u_vz),
      .age_o         (u_age),
      .species_o     (u_spc),
      .size_o        (u_siz),
      .spin_o        (u_spn),
      .flags_o       (u_flg),
      .variation_o   (u_var),

      .pos_x_i       (w_px),
      .pos_y_i       (w_py),
      .pos_z_i       (w_pz),
      .vel_x_i       (w_vx),
      .vel_y_i       (w_vy),
      .vel_z_i       (w_vz),
      .age_i         (u_age),      // this block does not age a particle
      .species_i     (u_spc),
      .size_i        (u_siz),
      .spin_i        (u_spn),
      .flags_i       (w_flg),
      .variation_i   (u_var),
      .rec_o         (rec_out_c),

      .base_radius_i (32'sd0),
      .radius_o      (unused_radius),
      .angle16_o     (unused_angle16)
  );

  // The published descriptor address. One net, no arithmetic, no register: see
  // the note at the port declaration for why the absence of a register is the
  // property that makes it correct rather than a shortcut.
  assign d_index_o = u_spc;

  // ---- the two surface tests ------------------------------------------------
  // TERRAIN. The heightfield's up axis is +Y, and `t_height_i` is in the same
  // S 9.8 m units as the position axis, so the gap is a plain subtraction.
  //
  // THE MEASURE AND THE PLACEMENT MUST BE THE SAME GEOMETRY, and getting that
  // wrong is subtle enough to be worth the paragraph. The tempting "sloped"
  // form is the distance to the plane through the sampled point with the
  // sampled normal, (p - s).n = (py - h)*ny, pushed back out along n. It reads
  // as more correct and it BREAKS THE CONTRACT'S ANTI-JITTER PROPERTY: pushing
  // along n moves x and z, the heightfield sample under the particle moves with
  // them, and the re-test measures a different surface point. Worked by hand on
  // a 0.6/0.8 slope it leaves the particle 27 LSBs still inside — a contact
  // reported twice in one tick, which is exactly the jitter source the ruling's
  // "one response per tick" is meant to remove.
  //
  // A heightfield contact is therefore VERTICAL: the gap is (py - h) and the
  // particle is placed at h + CLEAR_EPS. The re-test is then exact with no
  // rounding at all. The sampled NORMAL is still the real one and still drives
  // the whole velocity response — the slope shapes how the particle leaves, not
  // where it is put.
  logic signed [POS_W:0]    t_dy_c;
  logic signed [DIST_W-1:0] t_dist_c;
  always_comb begin
    t_dy_c   = (POS_W+1)'(u_py) - (POS_W+1)'(t_height_i);
    t_dist_c = DIST_W'(t_dy_c) <<< NRM_Q;
  end

  // PLANE. Surface is n.p = c, so the signed distance is p.n - c.
  logic signed [DIST_W-1:0] pl_dist_c;
  always_comb begin
    pl_dist_c = DIST_W'(u_px) * DIST_W'(pl_nx_i)
              + DIST_W'(u_py) * DIST_W'(pl_ny_i)
              + DIST_W'(u_pz) * DIST_W'(pl_nz_i)
              - DIST_W'(pl_c_i);
  end

  // Which one is resolved. ONE response per particle per tick is the ruling's
  // bound, so a choice has to be made and it has to be deterministic: the
  // DEEPER penetration wins, and a tie goes to the terrain because the live
  // deformed surface is the authored one.
  logic t_hit_c, pl_hit_c, use_plane_c, hit_c;
  always_comb begin
    t_hit_c     = t_valid_i && (t_dist_c < 0);
    pl_hit_c    = pl_en_i   && (pl_dist_c < 0);
    use_plane_c = pl_hit_c && (!t_hit_c || (pl_dist_c < t_dist_c));
    hit_c       = t_hit_c || pl_hit_c;
  end

  // TWO normals, and they are different on purpose.
  //   `nrm_c`  — the SURFACE normal. Drives the whole velocity response.
  //   `push_c` — the axis the contact point is displaced along, which must be
  //              the axis the contact test measures: the plane's own normal,
  //              or straight up for a heightfield. See the paragraph above.
  logic signed [DIST_W-1:0] dist_c;
  logic signed [NRM_W-1:0]  nrm_c  [3];
  logic signed [NRM_W-1:0]  pshn_c [3];
  always_comb begin
    dist_c    = use_plane_c ? pl_dist_c : t_dist_c;
    nrm_c[0]  = use_plane_c ? pl_nx_i : t_nx_i;
    nrm_c[1]  = use_plane_c ? pl_ny_i : t_ny_i;
    nrm_c[2]  = use_plane_c ? pl_nz_i : t_nz_i;
    pshn_c[0] = use_plane_c ? pl_nx_i : NRM_W'(0);
    pshn_c[1] = use_plane_c ? pl_ny_i : NRM_W'(signed'(1 << NRM_Q));
    pshn_c[2] = use_plane_c ? pl_nz_i : NRM_W'(0);
  end

  // ---- v.n, and the inward clamp -------------------------------------------
  // Only an INWARD approach has a normal component to remove or reflect. A
  // particle that is inside the surface but already travelling outward gets its
  // placement and nothing else — reflecting an outward component would drive it
  // back in, which is the sign error the contract's cross-check exists to find.
  logic signed [VEL_W-1:0] vel_c [3];
  logic signed [POS_W-1:0] pos_c [3];
  logic signed [VN_W-1:0]  vn_c, vn_eff_c;

  // The surface's own velocity. Vertical only, because the terrain lattice
  // stores dH/dt at a vertex and nothing in this console gives the ground a
  // horizontal rate -- terrain_rules 4.2's lattice is one word per vertex.
  // ZERO FOR THE PLANE, and that is a statement about the plane rather than a
  // convenience: `pl_*` is a static authored surface with no velocity input,
  // so claiming the terrain's rate for it would attribute one surface's motion
  // to another. Zero for a sample with no ground and zero when the velocity
  // plane is absent.
  logic signed [VEL_W-1:0] gv_c [3];
  logic                    ground_moving_c;

  // One bit wider than the record's field: a particle at the negative rail
  // minus a ground at the positive rail does not fit VEL_W, and silently
  // wrapping it would invert the approach direction -- the exact sign error
  // this block's inward clamp exists to catch.
  logic signed [VEL_W:0]   vrel_c [3];

  always_comb begin
    vel_c[0] = u_vx; vel_c[1] = u_vy; vel_c[2] = u_vz;
    pos_c[0] = u_px; pos_c[1] = u_py; pos_c[2] = u_pz;

    gv_c[0] = VEL_W'(0);
    gv_c[1] = (!use_plane_c && t_valid_i && t_vy_valid_i) ? t_vy_i : VEL_W'(0);
    gv_c[2] = VEL_W'(0);
    ground_moving_c = (gv_c[1] != VEL_W'(0));

    for (int unsigned a = 0; a < 3; a++) begin
      vrel_c[a] = (VEL_W + 1)'(vel_c[a]) - (VEL_W + 1)'(gv_c[a]);
    end

    // v_rel . n, not v . n. A particle falling onto ground that is rising to
    // meet it approaches faster than its own speed; one resting on ground
    // that is falling away is not in contact at all.
    vn_c = VN_W'(vrel_c[0]) * VN_W'(nrm_c[0])
         + VN_W'(vrel_c[1]) * VN_W'(nrm_c[1])
         + VN_W'(vrel_c[2]) * VN_W'(nrm_c[2]);
    vn_eff_c = (vn_c < 0) ? vn_c : VN_W'(0);
  end

  // ---- the table read, and the one datapath it drives -----------------------
  logic [RSP_W-1:0] rsp_c;
  logic             r_stuck_c, r_known_c, r_kill_c, r_place_c, r_setv_c;
  logic [1:0]       r_tan_src_c, r_nrm_src_c;
  always_comb begin
    rsp_c       = rsp_rom(d_response_i);
    r_stuck_c   = rsp_c[8];
    r_known_c   = rsp_c[7];
    r_kill_c    = rsp_c[6];
    r_place_c   = rsp_c[5];
    r_setv_c    = rsp_c[4];
    r_tan_src_c = rsp_c[3:2];
    r_nrm_src_c = rsp_c[1:0];
  end

  logic signed [COEF_W-1:0] tan_c, nrm_k_c;
  always_comb begin
    case (r_tan_src_c)
      SRC_FRIC: tan_c = COEF_W'(d_friction_i);
      SRC_DAMP: tan_c = COEF_W'(d_damping_i);
      SRC_NEGE: tan_c = -(COEF_W'(d_restitution_i));
      default:  tan_c = COEF_W'(0);
    endcase
    case (r_nrm_src_c)
      SRC_FRIC: nrm_k_c = COEF_W'(d_friction_i);
      SRC_DAMP: nrm_k_c = COEF_W'(d_damping_i);
      SRC_NEGE: nrm_k_c = -(COEF_W'(d_restitution_i));
      default:  nrm_k_c = COEF_W'(0);
    endcase
  end

  // v' = tan*v + (nrm - tan)*(v.n)*n, in Q(FX_Q + 2*NRM_Q), ONE rounding per
  // emitted component, round-half AWAY FROM ZERO. The fold to a single `k`
  // saves three products against computing the tangential and normal parts
  // separately, and is the identity
  //     tan*(v - vn*n) + nrm*(vn*n) == tan*v + (nrm - tan)*vn*n.
  logic signed [K_W-1:0]   k_c;
  logic signed [KV_W-1:0]  kv_c;
  logic signed [ACC_W-1:0] vacc_c [3];
  logic signed [ACC_W-1:0] vrnd_c [3];

  localparam logic signed [ACC_W-1:0] V_HALF = ACC_W'(1) <<< (VSH - 1);
  localparam logic signed [DISP_W-1:0] D_HALF = DISP_W'(1) <<< (PSH - 1);

  always_comb begin
    k_c  = K_W'(nrm_k_c) - K_W'(tan_c);
    kv_c = KV_W'(k_c) * KV_W'(vn_eff_c);
    for (int unsigned a = 0; a < 3; a++) begin
      // The response is computed in the GROUND's frame -- tangential and
      // normal parts of the RELATIVE velocity -- and the ground's own motion
      // is added back at `vsel_c` below. Doing it in the world frame instead
      // would apply friction and restitution to the ground's speed as though
      // it were the particle's, so a spark resting on a rising wave would be
      // decelerated by friction against the surface carrying it.
      vacc_c[a] = (ACC_W'(tan_c) * ACC_W'(vrel_c[a]) <<< PSH)
                + ACC_W'(kv_c) * ACC_W'(nrm_c[a]);
      vrnd_c[a] = (vacc_c[a] >= 0) ? ((vacc_c[a] + V_HALF) >>> VSH)
                                   : -(((-vacc_c[a]) + V_HALF) >>> VSH);
    end
  end

  // The contact point: p' = p + (EPS - d) * push_axis. `d` is negative here, so
  // the push is outward and at least EPS.
  localparam logic signed [DIST_W-1:0] EPS_Q = DIST_W'(CLEAR_EPS) <<< NRM_Q;
  logic signed [PUSH_W-1:0]  push_c;
  logic signed [DISP_W-1:0]  disp_c [3];
  logic signed [DISP_W-1:0]  drnd_c [3];
  logic signed [DISP_W-1:0]  pacc_c [3];
  always_comb begin
    push_c = PUSH_W'(EPS_Q) - PUSH_W'(dist_c);
    for (int unsigned a = 0; a < 3; a++) begin
      disp_c[a] = DISP_W'(push_c) * DISP_W'(pshn_c[a]);
      drnd_c[a] = (disp_c[a] >= 0) ? ((disp_c[a] + D_HALF) >>> PSH)
                                   : -(((-disp_c[a]) + D_HALF) >>> PSH);
      pacc_c[a] = DISP_W'(pos_c[a]) + drnd_c[a];
    end
  end

  // ---- already inside at entry ----------------------------------------------
  // The discriminator is EXACT and needs no threshold knob, because the two
  // ratified scales agree: position is S 9.8 m and velocity is S 2.8 m/tick, so
  // one tick of velocity v displaces the position by exactly v LSBs. After a
  // placing response the particle sat at distance >= EPS; one tick of motion
  // moves it by `vp` along the measured axis. A legal arrival therefore has
  //     penetration <= inward_travel - EPS.
  // Anything deeper could not have got there this tick, which is precisely
  // "the case a previous tick's response should have prevented".
  //
  // `vp_c` is the velocity along the axis the contact was MEASURED on, which is
  // the surface normal for a plane and straight up for a heightfield. Comparing
  // a vertical penetration against a normal-direction velocity would measure
  // the slope instead of the regression.
  //
  // WHAT IT ALSO CATCHES, and this is not a defect but it IS a thing to know
  // before reading the number: on LIVE DEFORMED TERRAIN a surface that rises
  // INTO a particle produces a penetration the particle's own motion cannot
  // account for, and fires this counter. That is the same arithmetic reaching
  // the right conclusion about a different cause — nothing the particle did put
  // it there. `tests/particles/part_collide_directed.cpp` case 11 does exactly
  // that and the counter moves. So a non-zero reading means "a particle began a
  // tick inside a surface", which is what the counter is named after; it does
  // NOT by itself mean a previous response misplaced something. Separating the
  // two needs the terrain's own deformation record and is not this block's.
  logic signed [VN_W-1:0]   vp_c;
  logic signed [PUSH_W-1:0] pen_c, inward_c;
  logic                     inside_at_entry_c;
  always_comb begin
    vp_c              = use_plane_c ? vn_c : (VN_W'(vel_c[1]) <<< NRM_Q);
    pen_c             = -(PUSH_W'(dist_c));
    inward_c          = -(PUSH_W'(vp_c));
    inside_at_entry_c = hit_c && ((pen_c + PUSH_W'(EPS_Q)) > inward_c);
  end

  // ---- assemble the outgoing record -----------------------------------------
  logic respond_c;                 // a contact that a KNOWN response acts on
  logic clamp_c;
  assign respond_c = hit_c && r_known_c;

  logic signed [ACC_W-1:0]  vsel_c [3];
  logic signed [DISP_W-1:0] psel_c [3];
  logic signed [VEL_W-1:0]  vout_c [3];
  logic signed [POS_W-1:0]  pout_c [3];

  localparam logic signed [ACC_W-1:0]  V_MAX = ACC_W'((1 << (VEL_W-1)) - 1);
  localparam logic signed [ACC_W-1:0]  V_MIN = -(ACC_W'(1 << (VEL_W-1)));
  localparam logic signed [DISP_W-1:0] P_MAX = DISP_W'((1 << (POS_W-1)) - 1);
  localparam logic signed [DISP_W-1:0] P_MIN = -(DISP_W'(1 << (POS_W-1)));

  always_comb begin
    clamp_c = 1'b0;
    for (int unsigned a = 0; a < 3; a++) begin
      // Back into the world frame. The clamp below is the SAME one the
      // absolute form already had, so an overflow introduced by the addition
      // is caught and counted by `field_clamps_o` without a second guard.
      // With `gv_c` zero this is `vrnd_c[a]` exactly, which is what makes the
      // no-field case bit-identical to the previous behaviour.
      vsel_c[a] = (respond_c && r_setv_c)
                    ? (vrnd_c[a] + ACC_W'(gv_c[a]))
                    : ACC_W'(vel_c[a]);
      psel_c[a] = (respond_c && r_place_c) ? pacc_c[a] : DISP_W'(pos_c[a]);

      if (vsel_c[a] > V_MAX)      begin vout_c[a] = VEL_W'(V_MAX); clamp_c = 1'b1; end
      else if (vsel_c[a] < V_MIN) begin vout_c[a] = VEL_W'(V_MIN); clamp_c = 1'b1; end
      else                              vout_c[a] = VEL_W'(vsel_c[a]);

      if (psel_c[a] > P_MAX)      begin pout_c[a] = POS_W'(P_MAX); clamp_c = 1'b1; end
      else if (psel_c[a] < P_MIN) begin pout_c[a] = POS_W'(P_MIN); clamp_c = 1'b1; end
      else                              pout_c[a] = POS_W'(psel_c[a]);
    end
  end

  // A REFUSED particle is handed back BYTE-IDENTICAL and not alive. Passing it
  // through alive and unmoved would be exactly the IGNORE default the contract
  // forbids, wearing a different name.
  logic refuse_c;
  assign refuse_c = !r_known_c;

  always_comb begin
    if (refuse_c) begin
      w_px  = u_px;  w_py  = u_py;  w_pz  = u_pz;
      w_vx  = u_vx;  w_vy  = u_vy;  w_vz  = u_vz;
      w_flg = u_flg;
    end else begin
      w_px = pout_c[0]; w_py = pout_c[1]; w_pz = pout_c[2];
      w_vx = vout_c[0]; w_vy = vout_c[1]; w_vz = vout_c[2];
      // kPartCollidedThisTick means THIS tick, so it is written every beat and
      // not merely set — a flag nobody clears stops meaning "this tick" after
      // the first contact of a particle's life. kPartBornThisTick and
      // kPartFlagReserved pass through untouched.
      w_flg = (u_flg & ~FLG_COLLIDED)
            | (hit_c ? FLG_COLLIDED : 4'b0)
            | ((hit_c && r_stuck_c) ? FLG_STUCK : 4'b0);
    end
  end

  logic alive_c;
  assign alive_c = r_known_c && !(hit_c && r_kill_c);

  // ---- handshake: one beat in, one beat out, fixed latency ------------------
  assign p_ready_o = !c_valid_o || c_ready_i;
  logic take_c;
  assign take_c = p_valid_i && p_ready_o;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      // "Reset abandons the particle in flight ... so no state survives."
      c_valid_o    <= 1'b0;
      c_record_o   <= '0;
      c_alive_o    <= 1'b0;
      c_contact_o  <= 1'b0;
      c_response_o <= 3'd0;
      c_refused_o  <= 1'b0;
      c_events_o   <= 4'd0;
      contacts_ignore_o            <= 32'd0;
      contacts_die_o               <= 32'd0;
      contacts_stick_o             <= 32'd0;
      contacts_slide_o             <= 32'd0;
      contacts_bounce_o            <= 32'd0;
      contacts_terrain_o           <= 32'd0;
      contacts_plane_o             <= 32'd0;
      already_inside_at_entry_o    <= 32'd0;
      terrain_sample_unavailable_o <= 32'd0;
      response_refused_o           <= 32'd0;
      field_clamps_o               <= 32'd0;
      contacts_moving_ground_o     <= 32'd0;
      collision_events_o           <= 32'd0;
    end else begin
      if (c_valid_o && c_ready_i) c_valid_o <= 1'b0;

      if (take_c) begin
        c_valid_o    <= 1'b1;
        c_record_o   <= rec_out_c;
        c_alive_o    <= alive_c;
        c_contact_o  <= respond_c;
        c_response_o <= d_response_i;
        c_refused_o  <= refuse_c;

        // THE COLLISION EVENT, ruling I4 §2. Bits 0/1/3 are PART.UPDATE's and
        // ride across untouched; bit 2 is this block's `respond_c` -- the same
        // wire that writes kPartCollidedThisTick into `w_flg` above, so the
        // announced event and the recorded flag are one decision. Loaded by
        // `take_c`, the enable that loads the record, so the two cannot part.
        c_events_o   <= {p_events_i[3], respond_c, p_events_i[1], p_events_i[0]};

        if (respond_c) collision_events_o <= collision_events_o + 32'd1;

        // "terrain sample unavailable: treat as no contact for this tick, count
        // it. Do not stall the particle path on a terrain read."
        if (!t_valid_i)
          terrain_sample_unavailable_o <= terrain_sample_unavailable_o + 32'd1;

        if (refuse_c) begin
          response_refused_o <= response_refused_o + 32'd1;
        end else if (hit_c) begin
          case (d_response_i)
            R_IGNORE: contacts_ignore_o <= contacts_ignore_o + 32'd1;
            R_DIE:    contacts_die_o    <= contacts_die_o    + 32'd1;
            R_STICK:  contacts_stick_o  <= contacts_stick_o  + 32'd1;
            R_SLIDE:  contacts_slide_o  <= contacts_slide_o  + 32'd1;
            R_BOUNCE: contacts_bounce_o <= contacts_bounce_o + 32'd1;
            default:  ;  // unreachable: refuse_c covers 5..7
          endcase
          if (use_plane_c) contacts_plane_o   <= contacts_plane_o   + 32'd1;
          else             contacts_terrain_o <= contacts_terrain_o + 32'd1;

          // Beside the terrain count and not inside it: this says how many of
          // those contacts were against ground that was actually MOVING, which
          // is the number the whole TERRAIN.VELOCITY chain exists to make
          // non-zero. It sits here, at the accepted contact, so it counts
          // RESOLVED contacts rather than samples offered.
          if (ground_moving_c && !use_plane_c)
            contacts_moving_ground_o <= contacts_moving_ground_o + 32'd1;

          // Counted only for the responses that PLACE. IGNORE and DIE promise
          // nothing about where the particle ends up, so a deep entry under
          // either is not the regression this counter names.
          if (inside_at_entry_c && r_place_c)
            already_inside_at_entry_o <= already_inside_at_entry_o + 32'd1;

          if (clamp_c && (r_place_c || r_setv_c))
            field_clamps_o <= field_clamps_o + 32'd1;
        end
      end
    end
  end

  // ---- the parent record PART.SPAWN derives its child from ------------------
  // Owner ruling I4 §3, and the whole of the reversal is the parameter above.
  // Quartus 17.0 needs the explicit generate/endgenerate keywords; an implicit
  // generate is a syntax error there however clean the Verilator lint.
  generate
    if (CHILD_AT_POST_CONTACT) begin : g_child_post
      // THE RULED CHOICE, AND IT IS FREE: the same wires as `c_record_o`, which
      // already carries `pout_c`, `vout_c` and kPartCollidedThisTick.
      assign c_spawn_record_o = c_record_o;
    end else begin : g_child_pre
      // The reversal pays for its own register -- the record exactly as
      // PART.UPDATE handed it over -- loaded by `take_c`, the SAME enable as
      // `c_record_o`, so the two cannot come apart on a stall.
      logic [REC_W-1:0] pre_rec_q;
      always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n)      pre_rec_q <= '0;
        else if (take_c) pre_rec_q <= p_record_i;
      end
      assign c_spawn_record_o = pre_rec_q;
    end
  endgenerate

`ifdef ZHAO_ASSERT
  logic armed_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) armed_q <= 1'b0;
    else        armed_q <= 1'b1;
  end
  always_ff @(posedge clk) begin
    if (armed_q) begin
      // ONE RESPONSE PER PARTICLE PER TICK is structural: there is no loop and
      // no second test, so it is a fact about the datapath and not about a
      // comparison. What can be checked is that the two verdicts are exclusive
      // and that a refusal never also claims a contact.
      a_no_contact_when_refused: assert (!(c_valid_o && c_refused_o && c_contact_o));

      // A refused particle is never alive: refusing and then passing it on is
      // the IGNORE default the contract forbids.
      a_refused_not_alive: assert (!(c_valid_o && c_refused_o && c_alive_o));

      // Handshake hygiene: a raised valid holds until it is taken.
      if ($past(c_valid_o) && !$past(c_ready_i))
        a_valid_holds: assert (c_valid_o);

      // THE EVENT VECTOR RETIRES WITH THE RECORD. This is deliberately a check
      // about the ENABLE and not about the values: `c_events_o[2]` and the
      // record's kPartCollidedThisTick come from the same wire, so a checker
      // differencing them is blind by construction (CLAUDE.md, "a detector
      // wired to two operands that move together cannot fire"). What CAN go
      // wrong is somebody giving the event vector a second enable -- an
      // unconditional load -- at which point a stall delivers one particle's
      // record with the next particle's events. That fault moves `c_events_o`
      // while the beat is held, and this fires on it.
      if ($past(c_valid_o) && !$past(c_ready_i))
        a_events_hold: assert (c_events_o == $past(c_events_o));

      // ONE AUTHOR FOR THE COLLISION BIT. The port comment says bit 2 arrives
      // zero; this is what makes that a checked claim rather than a promise.
      // If PART.UPDATE ever starts authoring it, the overwrite above would
      // silently discard it and this fires instead.
      if (take_c)
        a_upstream_collision_bit_zero: assert (!p_events_i[2]);
    end
  end
`endif

endmodule : zhao_part_collide

`default_nettype wire
