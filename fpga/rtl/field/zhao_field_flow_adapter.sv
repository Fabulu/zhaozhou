// zhao_field_flow_adapter.sv - THE F PROFILE'S STREAM ADAPTER: one particle
// record in, one bounded s11 acceleration triple out, through the ONE field
// engine.
//
// Entry I5 of `zhao_console_core.sv`, and owner ruling R40.
//
// ---------------------------------------------------------------------------
// WHAT WAS ACTUALLY MISSING, AND WHAT RULED IT
// ---------------------------------------------------------------------------
// I5 named it exactly: "nothing in this tree says which fields of the 128-bit
// particle128 record become which registers, nor which registers the three s11
// accelerations are read back from ... choosing it inside a composition packet
// would be inventing an ABI, and an invented lane map produces a perfectly
// plausible wind."
//
// It is no longer being chosen here. TWO ratified documents settle it between
// them and this file only wires them:
//
//   `spec/form/field-ir.md` 7.1 -- the flow profile's I/O records, verbatim:
//       in  (13): px,py,pz, vx,vy,vz:fx, age:u32, seed:u32, dt:fx, p0..p3:fx
//       out ( 7): px',py',pz', vx',vy',vz':fx, attr0:fx
//     Input lanes map to R0.. in the listed order; output lanes take their
//     values from the named registers at END. So the lane map is the spec's.
//
//   Owner ruling R40 -- the mapping onto PART.UPDATE's port:
//       acceleration = sat_s11((v' - v) >> 8), seed = the variation byte,
//       dt = 1 tick, and the host widened to 13 inputs / 7 outputs. The shift
//       and the saturation live in named, editable constants.
//
// R40 is marked PROVISIONAL and "particle MOTION is art, so the owner judges
// the result by eye". Every value it names is therefore a parameter of this
// module -- `ACC_SHIFT`, `ACC_ROUND`, `DT_FX`, `POS_SHIFT`, `VEL_SHIFT` -- so
// the owner's revision is a parameter edit at the composition and not a change
// to this file's structure. CLAUDE.md rule 6.
//
// ---------------------------------------------------------------------------
// THE UNIT LAW IS THE TREE'S, NOT A NEW ONE
// ---------------------------------------------------------------------------
// `zhao_part_terrain_tap.sv` carries the frozen conversion (qformats 10, its
// own elaboration guard refuses any other POS_W):
//
//     POS_W = 18, S 9.8 m      world x/z = origin + (local <<< 8)   exact
//
// So a particle position is 1/256 m and a lane is Q16.16: the widening is a
// left shift of 8 plus the population origin, and it is EXACT -- no rounding
// is introduced on the way in. Velocity is the same 1/256 scale, which is what
// makes R40's `>> 8` on the way out the exact inverse and not a fudge: the
// ruling and the frozen format agree, and the agreement is why `VEL_SHIFT` and
// `ACC_SHIFT` are both 8 and are separately named.
//
// ---------------------------------------------------------------------------
// THE JOIN IS THE ONE I5 ALREADY SOLVED, AND THIS BLOCK DOES NOT RE-DECIDE IT
// ---------------------------------------------------------------------------
// I5: "`zhao_part_update` takes this sample COMBINATIONALLY in the same cycle
// as `in_record_i` ... So a FLOW adapter needs NO new port on that block --
// this file gates `in_valid_i` and PART.STATE's `prt_ready_i` on 'the answer
// for THIS record is ready', and the record is held stable for the whole
// offer."
//
// That is what `ans_valid_o` is. It rises only while `rec_i` is the record the
// answer was computed from, and the composer ANDs it into PART.UPDATE's
// `in_valid_i`. Both sides of the comparison are then the same cycle's wires,
// which is the property `zhao_part_update`'s own header requires of this seam
// and the property CLAUDE.md's metadata-swap chapter says to check first.
//
// ---------------------------------------------------------------------------
// AN ABSENT FIELD IS NOT A ZERO FIELD, AND THE PORT ALREADY SAYS SO
// ---------------------------------------------------------------------------
// `fld_valid_o` LOW is the honest answer when no FLOW program is resident, when
// the engine refuses the run, or when the run raises an alarm. PART.UPDATE adds
// the sample only `if (fld_valid_i)`, so a low valid removes the term rather
// than adding a zero one -- the same distinction entry I34 draws for the
// terrain height lane, where a constant would be "a field program that raises
// or lowers every vertex by the same amount, and it would be invisible".
//
// Each of the three reasons is COUNTED SEPARATELY (`bypassed_o`, `noprog_o`,
// `faults_o`), because a merged "no sample" total cannot tell an unarmed
// console from a broken program.
//
// ---------------------------------------------------------------------------
// THE COST, SAID OUT LOUD
// ---------------------------------------------------------------------------
// `zhao_field_host`'s front holds ONE point in flight and its run is
// REGS + IN_LANES + the program's own length. So one particle per field run is
// tens of clocks against PART.UPDATE's one particle per clock. `stall_cycles_o`
// is that cost, measured rather than argued: it counts cycles in which a record
// was offered and this block had no answer yet. It is the number that decides
// whether the gathering front `zhao_field_host`'s header calls for has to be
// built before particles can run a field at population scale, and it is
// exported rather than kept private for exactly that reason.
//
// Conservative SystemVerilog subset (Quartus 17.0).
// ENFORCED-BY: tests/field/field_flow_adapter_directed.cpp:main
`default_nettype none

module zhao_field_flow_adapter #(
    parameter int unsigned SLOTW     = 3,
    // The shared field port's lane counts. The flow record is 13/7
    // (field-ir.md 7.1) and R40 widens the host to match.
    parameter int unsigned IN_LANES  = 13,
    parameter int unsigned OUT_LANES = 7,

    // ---- R40's named, editable constants ------------------------------------
    // The particle position's frozen scale (qformats 10, S 9.8 m): local <<< 8
    // is world fx16. EXACT, no rounding.
    parameter int unsigned POS_SHIFT = 8,
    // The particle velocity's scale, the same 1/256 per tick.
    parameter int unsigned VEL_SHIFT = 8,
    // R40's `>> 8`: fx16 velocity delta -> s11 acceleration.
    parameter int unsigned ACC_SHIFT = 8,
    // 0 = R40 as written (arithmetic shift, a floor). 1 = round half AWAY FROM
    // ZERO, which is `zhao_part_update`'s own rounding law and removes the
    // directional bias its header warns about ("a bias that points particles
    // one way and would show up as drift over a thousand ticks"). R40 wrote a
    // shift, so a shift is the default and the alternative is one parameter
    // away rather than one edit away.
    parameter bit          ACC_ROUND = 1'b0,
    // R40: "dt = 1 tick". Q16.16.
    parameter logic signed [31:0] DT_FX = 32'sh0001_0000
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- the record PART.STATE is offering, held for the whole offer --------
    input  var logic          rec_valid_i,
    input  var logic [127:0]  rec_i,
    // The cycle the record is CONSUMED (PART.UPDATE's `in_valid && in_ready`).
    // The answer is retired by this pulse and not by `rec_valid_i` falling:
    // PART.STATE may offer the next record in the same cycle the last one
    // retires, and a state machine that waited for a gap would then hold
    // record A's acceleration across record B's offer. That is the
    // metadata-swap shape exactly -- "response A's data with B's metadata" --
    // and it is the reason `rec_changed_o` below exists as well.
    input  var logic          rec_take_i,

    // ---- the population origin (R41's SetPopulation bank) -------------------
    // The same three values PART.COLLIDE and PART.TERRAIN_TAP read, so a
    // particle's world position is one law in this console and not two.
    input  var logic signed [31:0] origin_x_i,
    input  var logic signed [31:0] origin_y_i,
    input  var logic signed [31:0] origin_z_i,

    // ---- the FLOW program's four parameters (p0..p3 of the 7.1 record) ------
    // SW.STREAM's, travelling with the plan that named the program: they are
    // the PROGRAM's parameters, not the particle's, and field-ir.md 7.1 puts
    // them in the input record rather than in the uniform bank.
    input  var logic [127:0]  par_i,

    // ---- which resident program is the wind ---------------------------------
    // The same shape as `fld_stamp_slot_i` beside it and for the same reason:
    // no opcode carries it.
    input  var logic [SLOTW-1:0] slot_i,
    input  var logic             slot_valid_i,

    // ---- the field engine client ---------------------------------------------
    output var logic                     req_valid_o,
    input  var logic                     req_ready_i,
    output var logic [SLOTW-1:0]         req_slot_o,
    output var logic                     req_noprog_o,
    output var logic [IN_LANES*32-1:0]   req_in_o,
    input  var logic                     resp_valid_i,
    output var logic                     resp_ready_o,
    // THE LANES READ HERE ARE CANONICAL OUTPUT ORDINALS, NOT PHYSICAL WINDOW
    // POSITIONS -- FH17's one result contract, stated identically in
    // `zhao_field_stamp_adapter.sv` and `zhao_field_warp_adapter.sv`. Ordinal
    // 3/4/5 is vx'/vy'/vz' of the flow record whatever physical register the
    // program wrote; the window-to-ordinal compaction is the HOST's act
    // through OUTPUT_MAP (owner directive 7.1/7.3). R101's WINDOW mask and
    // FH05's REQUIRED mask are different named fields of different widths and
    // are never assigned to one another. R111 measured that the shipped
    // programs' output registers are never contiguous, so an adapter reading a
    // window POSITION here would be reading whichever register happened to sit
    // at that offset -- and the flow adapter reads lanes 3-5, which R111 names
    // as the exact case that would have fed three zero velocities, "a
    // plausible-looking maximum deceleration".
    //
    // THREE OF THE SEVEN OUTPUT LANES ARE READ, AND THAT IS R40 RATHER THAN AN
    // OVERSIGHT. The flow profile also answers px'/py'/pz' and attr0; R40 maps
    // the seam onto PART.UPDATE's ACCELERATION port, and PART.UPDATE integrates
    // position itself (its step 5) from the velocity it owns. Taking the
    // program's position would be a second integrator with a different rounding
    // law running beside the ratified one. `attr0` has no port on that block at
    // all. `zhao_field_stamp_adapter`'s "only 48 of 128 bits are the record" is
    // the same situation and the precedent for saying so here.
    /* verilator lint_off UNUSEDSIGNAL */
    input  var logic [OUT_LANES*32-1:0]  resp_out_i,
    /* verilator lint_on UNUSEDSIGNAL */
    input  var logic [7:0]               resp_status_i,

    // ---- the answer, to PART.UPDATE's `fld_*` port ---------------------------
    // `ans_valid_o` is the JOIN: the composer ANDs it into PART.UPDATE's
    // `in_valid_i`, so the acceleration and the record it belongs to are the
    // same cycle's wires.
    output var logic                 ans_valid_o,
    output var logic                 fld_valid_o,
    output var logic signed [10:0]   fld_ax_o,
    output var logic signed [10:0]   fld_ay_o,
    output var logic signed [10:0]   fld_az_o,

    // ---- evidence -------------------------------------------------------------
    output var logic [31:0] samples_o,       // answers carrying a real field
    output var logic [31:0] bypassed_o,      // no program armed: answered, no run
    output var logic [31:0] noprog_o,        // the engine had no program in the slot
    output var logic [31:0] faults_o,        // the run ended on an alarm
    output var logic [31:0] saturations_o,   // an axis clamped at s11
    output var logic [31:0] stall_cycles_o,  // a record offered with no answer yet
    // THE IDENTITY GUARD. It fires when the record being offered while
    // `ans_valid_o` is high is not the record the answer was computed from.
    // Its two operands are clocked by DIFFERENT things -- `held_rec` is
    // captured once at request time, `rec_i` is the live wire -- which is the
    // property CLAUDE.md's metadata-swap chapter says to check first: a
    // detector whose two sides move together cannot fire.
    output var logic [31:0] rec_changed_o
);

  // The two statuses `zhao_field_host` adds above the ratified field-ir set.
  // 0xF1 (no result) and 0xF2 (alarm) are counted together on `faults_o`: both
  // mean the run produced nothing a particle may be moved by, and neither is
  // separable from this side without a second status port.
  localparam logic [7:0] StNoProgram = 8'hF0;

  localparam logic signed [10:0] S11_MAX = 11'sh3FF;
  localparam logic signed [10:0] S11_MIN = -11'sh400;

  initial begin
    if (IN_LANES < 13) begin
      $fatal(1, "zhao_field_flow_adapter: IN_LANES=%0d; field-ir.md 7.1 flow is 13", IN_LANES);
    end
    if (OUT_LANES < 7) begin
      $fatal(1, "zhao_field_flow_adapter: OUT_LANES=%0d; field-ir.md 7.1 flow is 7", OUT_LANES);
    end
  end

  // ==========================================================================
  // THE RECORD, UNPACKED BY THE ONE CODEC
  // ==========================================================================
  // `zhao_part_record` is the single implementation of the particle128 layout.
  // Re-slicing `rec_i` here with literals would be a second opinion about a
  // frozen record, which is the duplication CLAUDE.md's sibling-contract
  // chapter is about.
  logic signed [17:0] u_px, u_py, u_pz;
  logic signed [10:0] u_vx, u_vy, u_vz;
  logic [9:0]         u_age;
  logic [7:0]         u_var;

  /* verilator lint_off PINCONNECTEMPTY */
  zhao_part_record u_codec (
      .rec_i      (rec_i),
      .pos_x_o    (u_px),
      .pos_y_o    (u_py),
      .pos_z_o    (u_pz),
      .vel_x_o    (u_vx),
      .vel_y_o    (u_vy),
      .vel_z_o    (u_vz),
      .age_o      (u_age),
      .species_o  (),
      .size_o     (),
      .spin_o     (),
      .flags_o    (),
      .variation_o(u_var),

      .pos_x_i    (18'sd0),
      .pos_y_i    (18'sd0),
      .pos_z_i    (18'sd0),
      .vel_x_i    (11'sd0),
      .vel_y_i    (11'sd0),
      .vel_z_i    (11'sd0),
      .age_i      (10'd0),
      .species_i  (7'd0),
      .size_i     (6'd0),
      .spin_i     (6'd0),
      .flags_i    (4'd0),
      .variation_i(8'd0),
      .rec_o      (),

      .base_radius_i(32'sd0),
      .radius_o     (),
      .angle16_o    ()
  );
  /* verilator lint_on PINCONNECTEMPTY */

  // ---- the E record, field-ir.md 7.1 flow, in declaration order ------------
  wire signed [31:0] in_px = origin_x_i + (32'(u_px) <<< POS_SHIFT);
  wire signed [31:0] in_py = origin_y_i + (32'(u_py) <<< POS_SHIFT);
  wire signed [31:0] in_pz = origin_z_i + (32'(u_pz) <<< POS_SHIFT);
  wire signed [31:0] in_vx = 32'(u_vx) <<< VEL_SHIFT;
  wire signed [31:0] in_vy = 32'(u_vy) <<< VEL_SHIFT;
  wire signed [31:0] in_vz = 32'(u_vz) <<< VEL_SHIFT;

  // THE CANDIDATE PAYLOAD, formed from the LIVE pins. It is what WOULD be sent
  // if a run were started this cycle; it is never what is sent.
  logic [IN_LANES*32-1:0] cap_in_c;
  always_comb begin
    cap_in_c = '0;
    cap_in_c[( 0*32) +: 32] = in_px;
    cap_in_c[( 1*32) +: 32] = in_py;
    cap_in_c[( 2*32) +: 32] = in_pz;
    cap_in_c[( 3*32) +: 32] = in_vx;
    cap_in_c[( 4*32) +: 32] = in_vy;
    cap_in_c[( 5*32) +: 32] = in_vz;
    cap_in_c[( 6*32) +: 32] = {22'd0, u_age};              // age:u32
    cap_in_c[( 7*32) +: 32] = {24'd0, u_var};              // seed:u32 -- R40
    cap_in_c[( 8*32) +: 32] = DT_FX;                       // dt:fx    -- R40
    cap_in_c[( 9*32) +: 32] = par_i[( 0*32) +: 32];        // p0
    cap_in_c[(10*32) +: 32] = par_i[( 1*32) +: 32];        // p1
    cap_in_c[(11*32) +: 32] = par_i[( 2*32) +: 32];        // p2
    cap_in_c[(12*32) +: 32] = par_i[( 3*32) +: 32];        // p3
  end

  // ==========================================================================
  // 15.1's CAPTURE: THE OFFER AND THE SUBTRAHEND COME FROM THE SAME LATCH
  // ==========================================================================
  // Owner directive 15.1, verbatim: "Latch parameters, origin, dt, frame and
  // the actual particle record at request capture. **Derive no later result
  // from unrelated live `rec_i` or `par_i` pins.**"
  //
  // THAT NAMED A DEFECT THAT WAS LIVE IN THIS FILE, and it is worth writing
  // down because every gate passed over it. Until 2026-09-20 `req_in_o` was
  // driven combinationally from the live pins and `a_c[]` -- R40's
  // `sat_s11((v' - v) >> 8)` -- took its SUBTRAHEND `in_vx/in_vy/in_vz` from
  // those same live pins at RESPONSE time, tens of clocks after the run
  // started. So if PART.STATE moved the offered record while a run was in
  // flight, the console computed
  //
  //     acceleration = (v' OF RECORD A)  -  (v OF RECORD B)
  //
  // a well-formed, plausible, entirely wrong wind. This is the metadata-swap
  // shape from CLAUDE.md exactly -- "response A's data with B's metadata" --
  // and the guard beside it could not prevent it: `rec_changed_o` is sampled
  // in F_ANS, one state AFTER the wrong difference has already been latched,
  // and a record that moved during F_WAIT and moved BACK by F_ANS never fired
  // it at all. A detector downstream of the corruption is not a guard.
  //
  // There is now ONE latch, taken once, at the cycle the run is decided:
  //
  //   * `req_in_o` is driven FROM IT, so an offer standing in F_REQ cannot
  //     have its operands moved underneath it while the engine has not yet
  //     accepted -- the hazard `zhao_field_stamp_adapter`'s S_DROP comment
  //     names from the other direction.
  //   * R40's subtrahend is read BACK OUT OF IT, from lanes 3/4/5, so the two
  //     sides of the subtraction are provably the same point by construction
  //     rather than by timing argument. That is the first question CLAUDE.md's
  //     metadata-swap chapter says to ask of any checker, answered structurally.
  //   * parameters, origin and dt ride in the same latch, because they are
  //     part of the payload and 15.1 names all of them.
  //
  // THE COST, SAID OUT LOUD: this is IN_LANES*32 = 416 flops that the live-pin
  // version did not spend, on a console already over its ALM budget. It buys
  // the property that a result cannot belong to two different particles.
  // PHYSICAL FIT PENDING.
  logic [IN_LANES*32-1:0] held_in;
  assign req_in_o = held_in;

  // R40's subtrahend, read back out of the latch the engine was given.
  wire signed [31:0] cap_vx = held_in[(3*32) +: 32];
  wire signed [31:0] cap_vy = held_in[(4*32) +: 32];
  wire signed [31:0] cap_vz = held_in[(5*32) +: 32];

  assign req_slot_o   = slot_i;
  assign req_noprog_o = !slot_valid_i;

  // ==========================================================================
  // THE RUN
  // ==========================================================================
  typedef enum logic [1:0] { F_IDLE, F_REQ, F_WAIT, F_ANS } state_e;
  state_e state;

  // The three answers, latched from the response and held for the whole offer.
  logic               ans_field;
  logic signed [10:0] ans_a [0:2];
  // The record this answer belongs to, captured once when the run is started
  // (or when the bypass answer is formed). Never re-loaded from the live wire.
  logic [127:0]       held_rec;

  assign req_valid_o  = (state == F_REQ);
  assign resp_ready_o = (state == F_WAIT);
  assign ans_valid_o  = (state == F_ANS);
  assign fld_valid_o  = ans_field;
  assign fld_ax_o     = ans_a[0];
  assign fld_ay_o     = ans_a[1];
  assign fld_az_o     = ans_a[2];

  // ---- R40's mapping, once, in a function ---------------------------------
  // (v' - v) >> ACC_SHIFT, saturated into s11. The delta is formed in 33 bits
  // so the subtraction of two full-range s32 lanes cannot wrap before the
  // shift -- a wrap here would reverse a particle's direction, which
  // `zhao_part_update`'s overflow table calls out as reading like a physics bug.
  function automatic logic signed [10:0] accel_of(input logic signed [31:0] vp,
                                                  input logic signed [31:0] v,
                                                  output logic             sat);
    logic signed [32:0] d;
    logic signed [32:0] q;
    logic signed [32:0] half;
    begin
      d = 33'(vp) - 33'(v);
      if (ACC_ROUND) begin
        half = 33'sd1 <<< (ACC_SHIFT - 1);
        q    = (d >= 0) ? ((d + half) >>> ACC_SHIFT) : -((-d + half) >>> ACC_SHIFT);
      end else begin
        q = d >>> ACC_SHIFT;
      end
      if (q > 33'(S11_MAX)) begin
        sat = 1'b1;
        accel_of = S11_MAX;
      end else if (q < 33'(S11_MIN)) begin
        sat = 1'b1;
        accel_of = S11_MIN;
      end else begin
        sat = 1'b0;
        accel_of = q[10:0];
      end
    end
  endfunction

  wire signed [31:0] out_vx = resp_out_i[(3*32) +: 32];
  wire signed [31:0] out_vy = resp_out_i[(4*32) +: 32];
  wire signed [31:0] out_vz = resp_out_i[(5*32) +: 32];

  logic signed [10:0] a_c [0:2];
  logic               s_c [0:2];
  // R40, unchanged: `sat_s11((v' - v) >> ACC_SHIFT)`. The MINUEND is the
  // engine's answer; the SUBTRAHEND is `cap_v*`, read out of the very latch
  // the engine was handed -- NOT the live pins. See the CAPTURE chapter above.
  //
  // AND THERE IS STILL NO SECOND INTEGRATOR. R40 maps this seam onto
  // PART.UPDATE's ACCELERATION port and PART.UPDATE integrates position itself
  // (its step 5) from the velocity it owns. Output lanes 0/1/2 (px'/py'/pz')
  // and lane 6 (attr0) are read by nothing here, deliberately: taking the
  // program's position would be a second integrator with a different rounding
  // law running beside the ratified one.
  always_comb begin
    a_c[0] = accel_of(out_vx, cap_vx, s_c[0]);
    a_c[1] = accel_of(out_vy, cap_vy, s_c[1]);
    a_c[2] = accel_of(out_vz, cap_vz, s_c[2]);
  end

  // `zhao_field_host` answers 8'h00 for a run that reached OP_END with a
  // result, and 0xF0/0xF1/0xF2 for no-program / no-result / alarm. Anything
  // else is the fabric's own ratified status and is equally not a field value.
  // So OK is the exact zero, not "not one of the three I remembered".
  wire status_ok = (resp_status_i == 8'h00);

  integer k;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state <= F_IDLE;
      ans_field <= 1'b0;
      held_rec <= 128'd0;
      held_in <= '0;
      rec_changed_o <= 32'd0;
      for (k = 0; k < 3; k = k + 1) ans_a[k] <= 11'sd0;
      samples_o <= 32'd0;
      bypassed_o <= 32'd0;
      noprog_o <= 32'd0;
      faults_o <= 32'd0;
      saturations_o <= 32'd0;
      stall_cycles_o <= 32'd0;
    end else begin
      // A record offered with no answer yet. The cost of a scalar front,
      // measured rather than argued.
      if (rec_valid_i && (state != F_ANS)) begin
        if (stall_cycles_o != 32'hFFFF_FFFF) stall_cycles_o <= stall_cycles_o + 32'd1;
      end

      // The identity guard. Live wire against a value captured at request
      // time; nothing loads both.
      if ((state == F_ANS) && rec_valid_i && (rec_i != held_rec)) begin
        if (rec_changed_o != 32'hFFFF_FFFF) rec_changed_o <= rec_changed_o + 32'd1;
      end

      case (state)
        F_IDLE: begin
          if (rec_valid_i) begin
            // 15.1's CAPTURE, and it is ONE act. `held_rec` is the identity
            // guard's operand; `held_in` is the payload the engine is given
            // and the source of R40's subtrahend. Both are loaded here, from
            // this cycle's pins, and neither is ever re-loaded from a live
            // wire while a run is in flight. The BYPASS answer is formed under
            // the same capture, so a record that is answered without a run is
            // held to the same identity rule as one that is not.
            held_rec <= rec_i;
            held_in  <= cap_in_c;
            if (!slot_valid_i) begin
              // No wind is armed. ANSWERED IMMEDIATELY, with the sample absent
              // rather than zero, and counted apart from a refused run.
              ans_field <= 1'b0;
              for (k = 0; k < 3; k = k + 1) ans_a[k] <= 11'sd0;
              if (bypassed_o != 32'hFFFF_FFFF) bypassed_o <= bypassed_o + 32'd1;
              state <= F_ANS;
            end else begin
              state <= F_REQ;
            end
          end
        end

        F_REQ: begin
          if (req_ready_i) state <= F_WAIT;
        end

        F_WAIT: begin
          if (resp_valid_i) begin
            if (status_ok) begin
              ans_field <= 1'b1;
              for (k = 0; k < 3; k = k + 1) ans_a[k] <= a_c[k];
              if (samples_o != 32'hFFFF_FFFF) samples_o <= samples_o + 32'd1;
              if ((s_c[0] || s_c[1] || s_c[2]) && (saturations_o != 32'hFFFF_FFFF)) begin
                saturations_o <= saturations_o + 32'd1;
              end
            end else begin
              ans_field <= 1'b0;
              for (k = 0; k < 3; k = k + 1) ans_a[k] <= 11'sd0;
              if (resp_status_i == StNoProgram) begin
                if (noprog_o != 32'hFFFF_FFFF) noprog_o <= noprog_o + 32'd1;
              end else begin
                if (faults_o != 32'hFFFF_FFFF) faults_o <= faults_o + 32'd1;
              end
            end
            state <= F_ANS;
          end
        end

        F_ANS: begin
          // The answer is retired by the record's own ACCEPT, not by the offer
          // going away. PART.UPDATE takes record and acceleration in the same
          // cycle, so the handshake that retires one retires the other; if the
          // offer simply vanishes without an accept (a reset of the stream),
          // the answer goes with it rather than being carried to a stranger.
          if (rec_take_i || !rec_valid_i) state <= F_IDLE;
        end

        default: state <= F_IDLE;
      endcase
    end
  end

endmodule
