// zhao_field_warp_adapter.sv - THE W PROFILE'S STREAM ADAPTER: one post-skin
// world vertex in, one displacement triple and one replacement normal out,
// through the ONE field engine.
//
// GEOM.WARP prerequisite P2, and the third of the three profile adapters that
// FH17 requires to speak ONE result contract.
//
// Contract: design/contracts/FIELD.SEQ.WARP.md (a CONFIGURATION of
//           FIELD.SEQ.CORE, not a second engine) and
//           design/contracts/GEOM.WARP.md for the consumer's side.
// Consumer: `zhao_geom_warp.sv` -- WHICH DOES NOT EXIST YET, and that is the
//           point: this file is its prerequisite, not its replacement.
//
// ---------------------------------------------------------------------------
// WHAT THIS FILE IS, AND THE THREE THINGS IT IS NOT
// ---------------------------------------------------------------------------
// `design/contracts/FIELD.SEQ.CORE.md` 36-45 permits exactly this file and
// names this profile in the permission:
//
//   > **Profile adapters are PERMITTED and are the v3 front end.** A profile is
//   > a program set plus a STREAM ADAPTER -- a small generator that produces the
//   > varying input lanes directly (Earth: lattice x,z from patch origin +
//   > pitch; **Warp: vertex position/normal**; ...) and consumes the outputs
//   > directly from snooped export registers. Adapters generate and consume
//   > streams; they NEVER re-implement an op, and there are not five engines.
//
// `design/contracts/GEOM.WARP.md` 341-345 draws the same boundary from the
// other side: *"A Warp STREAM ADAPTER is permitted; a private Warp instruction
// set or a duplicate interpreter is not."*
//
// So, explicitly, this file:
//
//   * IS NOT `zhao_geom_warp.sv`. It does not perform W03's
//     `Pout = saturating_add(Pin, d)`. `GEOM.WARP.md` 133-136 puts that
//     addition in the consumer, formed in >= 33 bits through `zref::fx_add`,
//     with application saturation reported SEPARATELY from Field-program
//     saturation. This adapter hands over `d` and `n_out` and takes no view on
//     what is done with them. Doing the add here would be a second
//     implementation of a ratified arithmetic in the block next door -- the
//     failure `uncashed_cheques.py` check 3 exists to catch.
//   * IS NOT a normal normaliser. `GEOM.WARP.md` 141-148 and decision W17:
//     *"The normal-deformation law is authored by the Warp PROGRAM, not
//     guessed by the shell."* The input normal is the existing range-reduced,
//     NON-UNIT world direction and it is passed through unchanged; the output
//     normal is the program's REPLACEMENT, passed on unchanged. No extra
//     normalization is introduced on any path, which is precisely what makes
//     W03's identity case exact (see IDENTITY below).
//   * IS NOT a private engine, math bank or loader. Decision W12: *"One Field
//     fabric, one projector complex. Add a logical Warp CLIENT."* Decision
//     W16: *"Do not bury a second Field loader inside GEOM.WARP to avoid doing
//     this."* This is a client of `zhao_field_host`, nothing more.
//
// ===========================================================================
// FH17 -- THE ONE RESULT CONTRACT
// ===========================================================================
// The same two sentences the stamp and flow adapters carry, because FH17's
// whole content is that all three say the same thing:
//
//   PHYSICAL CLIENT PORT -- `IN_LANES` / `OUT_LANES`, the width of the SHARED
//     bus, which is the maximum over every composed profile and never this
//     profile's own arity. The console composes 13/7 today, which is why this
//     adapter CANNOT YET BE COMPOSED: Warp needs fifteen. That is not a defect
//     in this file, it is R103's prerequisite P1, and
//     `zhao_console_core.sv:15911-15913` predicted exactly this handover --
//     *"the day it does, this pair moves to 15/7 and the adapters' elaboration
//     guards are what will say so."* The guard below is that guard. It refuses
//     at elaboration rather than dropping p3 silently.
//
//   SEMANTIC PROFILE ARITY -- the W profile is 15 in / 6 out, from
//     `spec/form/field-ir.md` 7.1 line 523:
//       | warp | 1 | px,py,pz:fx, nx,ny,nz:fx, a0..a3:fx, time:u32, p0..p3:fx (15)
//                  | dx,dy,dz:fx, nx',ny',nz':fx (6) |
//     and independently in code at `zref_geom_warp.hpp:76,79`
//     (`kInLanes = 15`, `kOutLanes = 6`), in the generated schema at
//     `zhao_field_host_image_pkg.sv` (`ZFH_PROFILE_WARP_INPUTS = 15`,
//     `ZFH_PROFILE_WARP_OUTPUTS = 6`), and in `zfield_host_image.hpp`
//     (`{"warp", 1, 15, 6}`).
//
// **WHY FIFTEEN IS WRITTEN IN THREE PLACES AND CHECKED HERE.** The count was
// "(14)" in the spec until 2026-09-20 and the fifteen fields were always
// right. `field-ir.md` 536-545 records what the wrong parenthetical cost: it
// had already propagated into `zhao_console_core.sv`, and *"building to that
// sentence drops p3 -- and a dropped lane does not read as absent. It reads as
// whatever the host's register clear left in R14, which is the flattering
// direction: a plausible number from a lane nobody supplied."* So the arity is
// an elaboration REFUSAL here, not a comment.
//
// **AND THE INDEX SPACE OF `resp_out_i` IS THE CANONICAL OUTPUT ORDINAL, NOT
// THE PHYSICAL CAPTURE WINDOW.** Ordinal 0 is dx, 1 is dy, 2 is dz, 3 is nx',
// 4 is ny', 5 is nz' -- whatever physical register the program wrote. The
// window-to-ordinal compaction is the HOST's act through `OUTPUT_MAP` (owner
// directive 7.1/7.3). `zref_geom_warp.hpp:299-300` states the same law on the
// reference side: *"Their PHYSICAL register locations come from the decoded
// program's output map; they are NOT assumed to be contiguous, and this
// function never asks."* R111 measured that the shipped programs never are
// contiguous, so an adapter reading a window position would be reading
// whichever register happened to sit at that offset.
//
// ===========================================================================
// W10 AND THE ABSENT-VERSUS-ZERO DISTINCTION, WHICH IS THE WHOLE POINT
// ===========================================================================
// Decision W10: *"Publish no partially warped meshlet ... AN ABSENT OUTPUT
// MUST NOT LOOK LIKE A ZERO RESULT."* `GEOM.WARP.md` 202-203 repeats it.
//
// This is not a slogan here, it is two separate wires, and they are separate
// because a Warp IDENTITY is a legal, common, fully-successful result whose
// data is all zeros:
//
//   `ans_valid_o`  -- an answer for the HELD vertex is available.
//   `warp_valid_o` -- that answer carries a real Field result.
//
// Decision W09 and `GEOM.WARP.md` 149-151: *"An identity Warp (d = 0,
// n_out = n_in) over a lawful upstream normal performs NO additional shift and
// lighting is exact."* So `d = (0,0,0)` with `warp_valid_o` HIGH is a correct
// identity deformation, and `d = (0,0,0)` with `warp_valid_o` LOW is "no
// program answered". **Those two states are indistinguishable in the data and
// are distinguished only by the validity wire.** A consumer that inspected the
// displacement to decide whether a Warp happened would silently treat every
// refused run as an identity -- which is the flattering direction, because an
// identity looks exactly like a mesh that is fine.
//
// The flow adapter draws this line with the same two wires and the same words
// (`ans_valid_o` / `fld_valid_o`), and entry I34 draws it for the terrain
// height lane. One law, three consumers. That is FH17.
//
// Each reason for `warp_valid_o` being low is COUNTED SEPARATELY --
// `bypassed_o`, `noprog_o`, `sig_refused_o`, `faults_o` -- because a merged
// "no result" total cannot tell an unarmed console from a wrong-profile
// program from a broken one.
//
// ===========================================================================
// FH27 -- THE SIGNATURE IS CHECKED ABOVE NUMERIC EXECUTION
// ===========================================================================
// FH27: *"A Field program's generic decoder validation is necessary but not
// sufficient to bind it as a particular engine profile. Validate counts, lane
// types, map indices, immutable input set and the CHOSEN ADAPTER SIGNATURE as
// well."*
//
// `zref::geom_warp::check_signature` does this in the reference
// (`zref_geom_warp.hpp:280-285`, comparing `in_lanes.size() != kInLanes` and
// `out_lanes.size() != kOutLanes` BEFORE reading any lane) and nothing did it
// in RTL. `prog_profile_i` is the resident program's profile id from the
// program directory, and a slot holding anything other than WARP is REFUSED
// BEFORE a request is issued, with `sig_refused_o` counting it.
//
// This is cheap and it is the difference between "the wind program was loaded
// into the warp slot" producing a refusal and producing six plausible numbers
// from a thirteen-lane record read as fifteen.
//
// ---------------------------------------------------------------------------
// THE COST, SAID OUT LOUD
// ---------------------------------------------------------------------------
// `zhao_field_host`'s front holds ONE point in flight, so one vertex per field
// run is tens of clocks against `GEOM.WARP.md`'s target of one warped vertex
// per clock. `stall_cycles_o` is that gap, MEASURED rather than argued: it
// counts cycles in which a vertex was offered and this block had no answer
// yet. Decision W13 is explicit that three separate performance claims exist
// and none proves the other two, so this counter is exported rather than kept
// private. **THROUGHPUT IS UNMEASURED AND THIS FILE DOES NOT CLAIM IT.**
//
// PHYSICAL FIT PENDING: no Quartus run has measured this block.
//
// Conservative SystemVerilog subset (Quartus 17.0): elaboration checks live
// inside `initial begin ... end` because Quartus 17.0 rejects a module-scope
// `if`, and there is no `generate` here at all.
//
// ENFORCED-BY: tests/field/field_warp_adapter_directed.cpp:main
`default_nettype none

module zhao_field_warp_adapter #(
    parameter int unsigned SLOTW = 3,

    // ---- THE PHYSICAL CLIENT PORT (FH17), not the W profile's 15/6 ---------
    // The SHARED bus width. Warp is the widest INPUT profile, so composing
    // this adapter is what forces the console's pair from 13/7 to 15/7 --
    // R103's prerequisite P1. The guard below refuses anything narrower.
    parameter int unsigned IN_LANES  = 15,
    parameter int unsigned OUT_LANES = 7,

    // ---- the profile id this adapter will accept (FH27) --------------------
    // `spec/form/field-ir.md` 7.1: earth 0, warp 1, flow 2, formation 3,
    // stamp 4. Named rather than literal so the check reads as a check.
    parameter logic [7:0] WARP_PROFILE_ID = 8'd1
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- the vertex GEOM.SKIN is offering, held for the whole offer --------
    input  var logic          vtx_valid_i,
    // Post-skin WORLD position, fx Q16.16 (decision W02). Canonical lanes 0..2.
    input  var logic signed [31:0] px_i,
    input  var logic signed [31:0] py_i,
    input  var logic signed [31:0] pz_i,
    // The existing range-reduced, NON-UNIT world direction from
    // GEOM.SKIN.NORM (decision W02). Canonical lanes 3..5. PASSED THROUGH: no
    // normalisation is introduced here, which is what keeps W03's identity
    // exact.
    input  var logic signed [31:0] nx_i,
    input  var logic signed [31:0] ny_i,
    input  var logic signed [31:0] nz_i,
    // INLINE4 or STREAM4 attributes, fx. Canonical lanes 6..9.
    input  var logic [127:0]  attr_i,
    // The cycle the answer is CONSUMED. The answer is retired by this pulse
    // and not by `vtx_valid_i` falling: the producer may offer the next vertex
    // in the same cycle the last one retires, and a state machine that waited
    // for a gap would hold vertex A's displacement across vertex B's offer.
    // That is the metadata-swap shape exactly, and it is why `vtx_changed_o`
    // below exists as well.
    input  var logic          vtx_take_i,

    // ---- the association descriptor, captured with the request -------------
    // `time` is canonical lane 10 and is u32 TICK DATA.
    // `zref_geom_warp.hpp:206-207` is explicit: *"time is u32 tick DATA. It is
    // bit-cast, not converted."* It is THIS DrawWarpedForm's tick, never a
    // clock read, so it arrives as a latched value and not as a counter.
    input  var logic [31:0]   time_i,
    // THIS DrawWarpedForm's p0..p3, canonical lanes 11..14. The same shape and
    // the same reason as the flow adapter's `par_i`: they are the PROGRAM's
    // parameters, and field-ir.md 7.1 puts them in the input record rather
    // than in the uniform bank.
    input  var logic [127:0]  par_i,

    // ---- which resident program is the deformation -------------------------
    input  var logic [SLOTW-1:0] slot_i,
    input  var logic             slot_valid_i,
    // FH27: the resident program's profile id, from the program directory.
    // A slot holding a non-Warp program is refused before a request is issued.
    input  var logic [7:0]       prog_profile_i,

    // ---- the field engine client -------------------------------------------
    output var logic                     req_valid_o,
    input  var logic                     req_ready_i,
    output var logic [SLOTW-1:0]         req_slot_o,
    output var logic                     req_noprog_o,
    output var logic [IN_LANES*32-1:0]   req_in_o,
    input  var logic                     resp_valid_i,
    output var logic                     resp_ready_o,
    // CANONICAL OUTPUT ORDINALS 0..5, not physical window positions. The
    // shared bus carries OUT_LANES because flow declares seven; ordinal 6 is
    // not a field of the W record and reading it would be this block inventing
    // an output the profile does not declare.
    /* verilator lint_off UNUSEDSIGNAL */
    input  var logic [OUT_LANES*32-1:0]  resp_out_i,
    /* verilator lint_on UNUSEDSIGNAL */
    input  var logic [7:0]               resp_status_i,

    // ---- the answer, to zhao_geom_warp's field port ------------------------
    // `ans_valid_o` is the JOIN: the composer ANDs it into the consumer's own
    // valid, so the displacement and the vertex it belongs to are the same
    // cycle's wires.
    output var logic                 ans_valid_o,
    // W10: SEPARATE from the data. Low means "no Field result", never
    // "zero displacement" -- an identity Warp is d = 0 with this HIGH.
    output var logic                 warp_valid_o,
    output var logic signed [31:0]   dx_o,
    output var logic signed [31:0]   dy_o,
    output var logic signed [31:0]   dz_o,
    output var logic signed [31:0]   nx_o,
    output var logic signed [31:0]   ny_o,
    output var logic signed [31:0]   nz_o,

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0] vertices_o,      // answers carrying a real field
    output var logic [31:0] identities_o,    // ... of which d == 0 exactly
    output var logic [31:0] bypassed_o,      // no program armed: answered, no run
    output var logic [31:0] noprog_o,        // the engine had no program in the slot
    output var logic [31:0] sig_refused_o,   // FH27: the slot is not a Warp program
    output var logic [31:0] faults_o,        // the run ended on an alarm
    output var logic [31:0] stall_cycles_o,  // a vertex offered with no answer yet
    // THE IDENTITY GUARD, in the metadata-swap sense rather than the Warp
    // sense. It fires when the vertex being offered while `ans_valid_o` is
    // high is not the vertex the answer was computed from. Its two operands
    // are clocked by DIFFERENT things -- `held_key` is captured once at
    // request time, the live key is combinational from the input pins -- which
    // is the property CLAUDE.md's metadata-swap chapter says to check first:
    // a detector whose two sides move together cannot fire.
    output var logic [31:0] vtx_changed_o
);

  // The statuses `zhao_field_host` adds above the ratified field-ir set.
  // 0xF1 (no result), 0xF2 (alarm) and 0xF3 (partial outputs, owner ruling
  // R101) are counted together on `faults_o`: all three mean the run produced
  // nothing a vertex may be deformed by.
  localparam logic [7:0] StNoProgram = 8'hF0;

  // ---- the W profile's own arity (field-ir.md 7.1 line 523) ---------------
  localparam int unsigned W_CANONICAL_INPUTS  = 15;
  localparam int unsigned W_CANONICAL_OUTPUTS = 6;

  initial begin
    // FH17/W01: fifteen, not fourteen. Refused rather than truncated, because
    // a dropped p3 reads as a plausible number from a cleared register.
    if (IN_LANES < W_CANONICAL_INPUTS) begin
      $fatal(1, "zhao_field_warp_adapter: IN_LANES=%0d cannot carry the W profile's %0d canonical inputs (field-ir.md 7.1 line 523, decision W01). The console's shared pair must move to 15 before this adapter can be composed -- that is GEOM.WARP prerequisite P1, not a defect here.", IN_LANES, W_CANONICAL_INPUTS);
    end
    if (OUT_LANES < W_CANONICAL_OUTPUTS) begin
      $fatal(1, "zhao_field_warp_adapter: OUT_LANES=%0d cannot carry the W profile's %0d canonical outputs (dx,dy,dz,nx',ny',nz')", OUT_LANES, W_CANONICAL_OUTPUTS);
    end
    if (SLOTW < 1) $fatal(1, "zhao_field_warp_adapter: SLOTW must be at least 1");
  end

  // ==========================================================================
  // THE CANONICAL INPUT RECORD, IN DECLARATION ORDER
  // ==========================================================================
  // `zref_geom_warp.hpp:118-122` is the binding, and `pack_record` at :195-213
  // is its executable form. This is the RTL statement of the same table --
  // lanes, not struct order: :113-116 warns that the order is SEMANTIC and
  // that `pack_record` is "the only thing permitted to turn one into the
  // other".
  //
  //   lane 0..2   px,py,pz   post-skin world position, fx
  //   lane 3..5   nx,ny,nz   reduced direction, fx, NOT unit
  //   lane 6..9   a0..a3     attributes, fx
  //   lane 10     time       u32 tick, BIT-CAST not converted
  //   lane 11..14 p0..p3     this DrawWarpedForm's parameters, fx
  always_comb begin
    req_in_o = '0;
    req_in_o[( 0*32) +: 32] = px_i;
    req_in_o[( 1*32) +: 32] = py_i;
    req_in_o[( 2*32) +: 32] = pz_i;
    req_in_o[( 3*32) +: 32] = nx_i;
    req_in_o[( 4*32) +: 32] = ny_i;
    req_in_o[( 5*32) +: 32] = nz_i;
    req_in_o[( 6*32) +: 32] = attr_i[( 0*32) +: 32];   // a0
    req_in_o[( 7*32) +: 32] = attr_i[( 1*32) +: 32];   // a1
    req_in_o[( 8*32) +: 32] = attr_i[( 2*32) +: 32];   // a2
    req_in_o[( 9*32) +: 32] = attr_i[( 3*32) +: 32];   // a3
    req_in_o[(10*32) +: 32] = time_i;                  // bit-cast, not converted
    req_in_o[(11*32) +: 32] = par_i[( 0*32) +: 32];    // p0
    req_in_o[(12*32) +: 32] = par_i[( 1*32) +: 32];    // p1
    req_in_o[(13*32) +: 32] = par_i[( 2*32) +: 32];    // p2
    req_in_o[(14*32) +: 32] = par_i[( 3*32) +: 32];    // p3
  end

  assign req_slot_o   = slot_i;
  assign req_noprog_o = !slot_valid_i;

  // FH27: the signature check, ABOVE numeric execution. A slot that holds a
  // program of another profile never becomes a request.
  wire signature_ok_c = slot_valid_i && (prog_profile_i == WARP_PROFILE_ID);

  // ==========================================================================
  // THE RUN
  // ==========================================================================
  typedef enum logic [1:0] { W_IDLE, W_REQ, W_WAIT, W_ANS } state_e;
  state_e state;

  logic               ans_field;
  logic signed [31:0] ans_d [0:2];
  logic signed [31:0] ans_n [0:2];
  // The vertex this answer belongs to, captured once when the run is started
  // (or when the bypass answer is formed). Never re-loaded from the live
  // wires. The whole 15-lane record is 480 bits, so the guard holds a folded
  // KEY of it rather than the record: a 32-bit XOR-fold is enough to catch a
  // swapped vertex and costs 32 flops instead of 480.
  logic [31:0]        held_key;

  wire [31:0] live_key_c = px_i ^ py_i ^ pz_i ^ nx_i ^ ny_i ^ nz_i
                         ^ attr_i[(0*32) +: 32] ^ attr_i[(1*32) +: 32]
                         ^ attr_i[(2*32) +: 32] ^ attr_i[(3*32) +: 32]
                         ^ time_i
                         ^ par_i[(0*32) +: 32] ^ par_i[(1*32) +: 32]
                         ^ par_i[(2*32) +: 32] ^ par_i[(3*32) +: 32];

  assign req_valid_o  = (state == W_REQ);
  assign resp_ready_o = (state == W_WAIT);
  assign ans_valid_o  = (state == W_ANS);
  assign warp_valid_o = ans_field;
  assign dx_o = ans_d[0];
  assign dy_o = ans_d[1];
  assign dz_o = ans_d[2];
  assign nx_o = ans_n[0];
  assign ny_o = ans_n[1];
  assign nz_o = ans_n[2];

  // Canonical output ordinals 0..5. NOT window positions.
  wire signed [31:0] out_dx_c = resp_out_i[(0*32) +: 32];
  wire signed [31:0] out_dy_c = resp_out_i[(1*32) +: 32];
  wire signed [31:0] out_dz_c = resp_out_i[(2*32) +: 32];
  wire signed [31:0] out_nx_c = resp_out_i[(3*32) +: 32];
  wire signed [31:0] out_ny_c = resp_out_i[(4*32) +: 32];
  wire signed [31:0] out_nz_c = resp_out_i[(5*32) +: 32];

  // W09's identity, observed rather than special-cased. It is NOT a different
  // path: an identity run goes through the engine exactly like any other and
  // is counted here only so the evidence can say how many there were.
  wire identity_c = (out_dx_c == 32'sd0) && (out_dy_c == 32'sd0) && (out_dz_c == 32'sd0);

  // `zhao_field_host` answers 8'h00 for a run that reached OP_END with a
  // result. Anything else -- 0xF0/0xF1/0xF2/0xF3 or the fabric's own ratified
  // statuses -- is equally not a deformation. So OK is the exact zero, not
  // "not one of the four I remembered".
  wire status_ok_c = (resp_status_i == 8'h00);

  integer k;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state          <= W_IDLE;
      ans_field      <= 1'b0;
      held_key       <= 32'd0;
      vtx_changed_o  <= 32'd0;
      for (k = 0; k < 3; k = k + 1) begin
        ans_d[k] <= 32'sd0;
        ans_n[k] <= 32'sd0;
      end
      vertices_o     <= 32'd0;
      identities_o   <= 32'd0;
      bypassed_o     <= 32'd0;
      noprog_o       <= 32'd0;
      sig_refused_o  <= 32'd0;
      faults_o       <= 32'd0;
      stall_cycles_o <= 32'd0;
    end else begin
      // A vertex offered with no answer yet. The cost of a scalar front,
      // measured rather than argued (decision W13).
      if (vtx_valid_i && (state != W_ANS)) begin
        if (stall_cycles_o != 32'hFFFF_FFFF) stall_cycles_o <= stall_cycles_o + 32'd1;
      end

      // The metadata-swap guard. Live wires against a value captured at
      // request time; nothing loads both.
      if ((state == W_ANS) && vtx_valid_i && (live_key_c != held_key)) begin
        if (vtx_changed_o != 32'hFFFF_FFFF) vtx_changed_o <= vtx_changed_o + 32'd1;
      end

      case (state)
        W_IDLE: begin
          if (vtx_valid_i) begin
            held_key <= live_key_c;
            if (!slot_valid_i) begin
              // No deformation is armed. ANSWERED IMMEDIATELY, with the result
              // ABSENT rather than zero (W10), and counted apart from both a
              // refused signature and a refused run.
              ans_field <= 1'b0;
              for (k = 0; k < 3; k = k + 1) begin
                ans_d[k] <= 32'sd0;
                ans_n[k] <= 32'sd0;
              end
              if (bypassed_o != 32'hFFFF_FFFF) bypassed_o <= bypassed_o + 32'd1;
              state <= W_ANS;
            end else if (!signature_ok_c) begin
              // FH27. The slot holds a program of another profile: refused
              // ABOVE numeric execution, so no request is ever issued and the
              // engine never reads a 13-lane record as fifteen.
              ans_field <= 1'b0;
              for (k = 0; k < 3; k = k + 1) begin
                ans_d[k] <= 32'sd0;
                ans_n[k] <= 32'sd0;
              end
              if (sig_refused_o != 32'hFFFF_FFFF) sig_refused_o <= sig_refused_o + 32'd1;
              state <= W_ANS;
            end else begin
              state <= W_REQ;
            end
          end
        end

        W_REQ: begin
          if (req_ready_i) state <= W_WAIT;
        end

        W_WAIT: begin
          if (resp_valid_i) begin
            if (status_ok_c) begin
              // A REAL result, including the identity case. W03: the
              // displacement and the replacement normal are handed over
              // exactly as the program produced them -- no add, no gain, no
              // normalisation, no blend with the old normal.
              ans_field <= 1'b1;
              ans_d[0] <= out_dx_c;
              ans_d[1] <= out_dy_c;
              ans_d[2] <= out_dz_c;
              ans_n[0] <= out_nx_c;
              ans_n[1] <= out_ny_c;
              ans_n[2] <= out_nz_c;
              if (vertices_o != 32'hFFFF_FFFF) vertices_o <= vertices_o + 32'd1;
              if (identity_c && (identities_o != 32'hFFFF_FFFF)) begin
                identities_o <= identities_o + 32'd1;
              end
            end else begin
              ans_field <= 1'b0;
              for (k = 0; k < 3; k = k + 1) begin
                ans_d[k] <= 32'sd0;
                ans_n[k] <= 32'sd0;
              end
              if (resp_status_i == StNoProgram) begin
                if (noprog_o != 32'hFFFF_FFFF) noprog_o <= noprog_o + 32'd1;
              end else begin
                if (faults_o != 32'hFFFF_FFFF) faults_o <= faults_o + 32'd1;
              end
            end
            state <= W_ANS;
          end
        end

        W_ANS: begin
          // The answer is retired by the vertex's own ACCEPT, not by the offer
          // going away; if the offer simply vanishes without an accept (a
          // reset of the stream), the answer goes with it rather than being
          // carried to a stranger.
          if (vtx_take_i || !vtx_valid_i) state <= W_IDLE;
        end

        default: state <= W_IDLE;
      endcase
    end
  end

endmodule
