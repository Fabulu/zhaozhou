// zhao_geom_warp.sv -- GEOM.WARP, THE WARP APPLICATION STAGE.
//
// AUTHORITY. Owner directive reports/Zhaozhou_GEOM_WARP_Architecture_2026-09-20.txt
// (owner commit 4c256137), decisions W01-W18, ratified 2026-09-20 in
// reports/OWNER-RATIFICATION-20260920-WARP.md. Contract: design/contracts/GEOM.WARP.md.
// Reference (the arithmetic authority for this file's behaviour):
// reference/include/zref/zref_geom_warp.hpp -- `zref::geom_warp::apply_outputs`.
//
// WHAT THIS BLOCK IS. The APPLICATION half of W13's three separate claims. It
// takes a post-skin world vertex and the SIX words a Warp program returned, and
// publishes ONE coherent vertex:
//
//     Pout = componentwise canonical saturating ADD(Pin, d)            [W03, 5.3]
//     Nout = the program's three normal words, range-reduced for LIGHT [W03, 5.4]
//
// WHAT THIS BLOCK IS NOT.
//   * NOT an executor. There is no opcode, no interpreter, no math bank, no
//     reciprocal, no sine, no noise and no normalizer here. W12: "One Field
//     fabric, one projector complex. Add a logical Warp CLIENT." The six words
//     arrive from `zhao_field_warp_adapter`, which is a client of the one shared
//     Field host. This block never addresses the Field host directly.
//   * NOT a normal-deformation law. W17: "The normal-deformation law is authored
//     by the Warp PROGRAM, not guessed by the shell." There is no Jacobian, no
//     finite differencing, no blend with the old normal and no amplitude
//     multiplier. Each is forbidden by W03 BY NAME.
//   * NOT a clamp. On a bound violation this block POISONS. Directive 5.6: "Do
//     not clamp the displacement to the bound and pretend the program computed
//     that clamp." The returned displacement is preserved on the poison port so
//     the evidence survives the refusal.
//
// THE SEAM THAT IS THIS BLOCK'S OWN, AND IS EASY TO MISS. `zhao_geom_skin_norm`
// exports its world normal on SIGNED 64-BIT wires (`n_x_o` et al are
// `logic signed [63:0]`) whose reduced components are expected to fit signed 32.
// Directive 5.4: "Capture the exact low word only AFTER checking that sign
// extension reconstructs the full input. An out-of-range input is a seam fault,
// not permission to truncate it." `narrow_ok` below is that check, and
// `C_NORMAL_WIDTH` is that fault. A silent truncation here is the FLATTERING
// failure: it manufactures a plausible small number from an impossible large
// one, and nothing downstream could ever tell.
//
// TWO SHIFTS, AND WHY THE OBVIOUS ONE-LINER IS WRONG. The range reduction is the
// SAME loop `zref::skin_world_normal` uses: arithmetic-shift all three
// components together until max(abs) < 2^30. The tempting combinational form is
//
//     shifts = (max_abs >= 2**31) ? 2 : (max_abs >= 2**30) ? 1 : 0;
//
// and it is WRONG. Take n = -(2^31 - 1). Its magnitude is 2^31-1, which is below
// 2^31, so that form says one shift. But an ARITHMETIC right shift rounds toward
// negative infinity: -(2^31-1) >> 1 == -2^30 exactly, whose magnitude is 2^30 --
// still AT the ceiling, so the real loop shifts again. This block therefore
// implements the loop LITERALLY as two conditional stages, each re-deriving
// max(abs) from the values it actually has. Two stages suffice and the bound is
// proved, not asserted: the worst case entering stage 1 is 2^31, leaving it at
// 2^30, and leaving stage 2 at 2^29 -- below the ceiling. `normal_shifts_o`
// reports what was used and the directed test compares it to the oracle's count
// on every vertex, so a third-stage requirement could not hide.
//
// STATUS IS NOT A BOOLEAN [directive 5.5]. Three families are kept apart and
// each has its own saturating counter, per contract section 9's explicit refusal
// of "one `warp_error` that loses every cause":
//   * the PROGRAM's numerics      -- saturation and rcp0 are DEFINED answers and
//                                    arrive folded into the adapter's status;
//                                    they are NOT transport failures and they do
//                                    NOT drop a vertex [5.5];
//   * the APPLICATION's own add   -- `app_saturations_o`, counted SEPARATELY
//                                    from the program's, which 5.3 requires in
//                                    so many words;
//   * a transport/admission fault -- POISONS, via `poison_valid_o` + a cause.
//
// W10, PUBLISH NO PARTIALLY WARPED MESHLET. A poisoned vertex publishes on
// NEITHER output port. It does not become an identity deformation and it does
// not become a zero. "An absent output must not look like a zero result."
//
// THE FORK [contract section 4]. Position and normal go to two consumers and are
// accepted EXACTLY ONCE EACH. This uses the contract's second sanctioned option
// -- independent consumed bits in the output hold -- rather than ANDing the two
// readys while leaving both valids asserted, which directive 11.6 records as
// having previously duplicated accepted work elsewhere in this console. Neither
// output's `valid` is a function of its own `ready`, which is the property
// contract section 4 requires any block spliced into the palette->skin fork to
// preserve.
//
// COMPOSITION STATUS AT THIS COMMIT. This block is NOT instantiated in
// `zhao_console_core.sv`. That is C1's act and it is blocked on two things this
// file cannot repair: the console composes the Field host at IN_LANES=13 against
// Warp's fifteen (R103 prerequisite P1) with CLIENTS=2 and both taken (P2), and
// there is no `DrawWarpedForm` command, so no draw can set `d_warp_en_i`.
// The Field port is NOT tied off anywhere:
// `tests/field/warp_field_chain_directed.cpp`, over
// `tests/field/tb_warp_field_chain.sv`, drives it from the REAL
// `zhao_field_warp_adapter` against the REAL `zhao_field_host_v2` and follows a
// value all the way through a real Warp program on the real v3 engine.
//
// Packet W1's findings are in its COMMIT MESSAGES on branch `gz/fieldw1`, not
// in a run-folder file: the harness refuses `.md` under `runs/`, and
// `gz/fieldh1` hit the same refusal at `89bb9bad`. This comment named a
// `FINDINGS-fieldw1.md` that does not exist, and a sibling test file under a
// name I had changed; both were caught by walking every path these files NAME
// and checking it resolves. A header that cites a file nobody can open is the
// same defect as a counter nobody reads.

`default_nettype none

module zhao_geom_warp #(
    // Source-id width, matching `zhao_geom_skin_norm`'s SRCW.
    parameter int unsigned SRCW = 16,
    // Resident program slot width, matching `zhao_field_warp_adapter`'s SLOTW.
    parameter int unsigned SLOTW = 3,
    parameter int unsigned CNTW = 32,
    // zfield::WARP. A named constant, not a literal at the comparison site --
    // CLAUDE.md rule 6: every value the owner may want to move is a knob.
    parameter logic [7:0] WARP_PROFILE_ID = 8'd1
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- the post-skin vertex ------------------------------------------------
    // Position from GEOM.SKIN (`o_x_o`, signed 32, world Q16.16).
    // Direction from GEOM.SKIN.NORM (`n_x_o`, signed 64 -- see the header note).
    input  var logic                v_valid_i,
    output var logic                v_ready_o,
    input  var logic signed [31:0]  v_px_i,
    input  var logic signed [31:0]  v_py_i,
    input  var logic signed [31:0]  v_pz_i,
    input  var logic signed [63:0]  v_nx_i,
    input  var logic signed [63:0]  v_ny_i,
    input  var logic signed [63:0]  v_nz_i,
    input  var logic                v_n_degenerate_i,
    // a0..a3, INLINE4 (constant for the draw) or STREAM4 (per vertex) [W06].
    // Which of the two it is, is the RESOURCE READER's business, not this
    // block's: both present as four words beside the vertex.
    input  var logic [127:0]        v_attr_i,
    input  var logic [SRCW-1:0]     v_src_id_i,

    // ---- the per-draw descriptor [W05: SNAPSHOT EVERY DRAW] ------------------
    // W05 forbids a mutable global `current_warp` register: program handle,
    // time, params, attribute mode and displacement bound belong to THAT draw.
    // These are held stable for the draw by the descriptor sidecar upstream;
    // this block reads them, latches them with the vertex, and owns none of them.
    input  var logic                d_warp_en_i,
    input  var logic [SLOTW-1:0]    d_slot_i,
    input  var logic                d_slot_valid_i,
    input  var logic [7:0]          d_profile_i,
    input  var logic [31:0]         d_time_i,
    input  var logic [127:0]        d_par_i,
    // Componentwise WORLD-SPACE displacement bound [W11]. NONNEGATIVE is a
    // validity condition of the COMMAND (6.3); a negative one is refused, never
    // absolute-valued into something usable.
    input  var logic signed [31:0]  d_bx_i,
    input  var logic signed [31:0]  d_by_i,
    input  var logic signed [31:0]  d_bz_i,

    // ---- the Field client port -> `zhao_field_warp_adapter` ------------------
    // This is a LOGICAL Warp client [W12]. The adapter owns the fifteen-lane
    // packing and the host handshake; this block owns the offer and the answer.
    output var logic                f_vtx_valid_o,
    output var logic signed [31:0]  f_px_o,
    output var logic signed [31:0]  f_py_o,
    output var logic signed [31:0]  f_pz_o,
    output var logic signed [31:0]  f_nx_o,
    output var logic signed [31:0]  f_ny_o,
    output var logic signed [31:0]  f_nz_o,
    output var logic [127:0]        f_attr_o,
    output var logic                f_vtx_take_o,
    output var logic [31:0]         f_time_o,
    output var logic [127:0]        f_par_o,
    output var logic [SLOTW-1:0]    f_slot_o,
    output var logic                f_slot_valid_o,
    output var logic [7:0]          f_profile_o,
    // `f_ans_valid_i` says an answer exists for the held vertex.
    // `f_warp_valid_i` says that answer carries a REAL Field result. The two are
    // separate wires on purpose [W10]: a failed evaluation must not be readable
    // as a successful zero displacement, and the DATA can never be used to tell
    // them apart because a legitimate identity program returns all zeroes.
    input  var logic                f_ans_valid_i,
    input  var logic                f_warp_valid_i,
    input  var logic signed [31:0]  f_dx_i,
    input  var logic signed [31:0]  f_dy_i,
    input  var logic signed [31:0]  f_dz_i,
    input  var logic signed [31:0]  f_onx_i,
    input  var logic signed [31:0]  f_ony_i,
    input  var logic signed [31:0]  f_onz_i,

    // ---- the published vertex, two consumers, exactly once each --------------
    output var logic                o_p_valid_o,
    input  var logic                o_p_ready_i,
    output var logic signed [31:0]  o_px_o,
    output var logic signed [31:0]  o_py_o,
    output var logic signed [31:0]  o_pz_o,
    output var logic [SRCW-1:0]     o_p_src_id_o,

    output var logic                o_n_valid_o,
    input  var logic                o_n_ready_i,
    output var logic signed [31:0]  o_nx_o,
    output var logic signed [31:0]  o_ny_o,
    output var logic signed [31:0]  o_nz_o,
    output var logic                o_n_degenerate_o,
    output var logic [SRCW-1:0]     o_n_src_id_o,

    // ---- poison [W10] --------------------------------------------------------
    // One cycle per refused vertex. The offending vertex index and the RETURNED
    // displacement are preserved: 5.6 diagnoses the violation from what the
    // program returned, so discarding it would discard the evidence.
    output var logic                poison_valid_o,
    output var logic [SRCW-1:0]     poison_src_id_o,
    output var logic [2:0]          poison_cause_o,
    output var logic signed [31:0]  poison_dx_o,
    output var logic signed [31:0]  poison_dy_o,
    output var logic signed [31:0]  poison_dz_o,

    // ---- evidence [contract section 10] --------------------------------------
    // `vertices_transformed_o` carries the frozen catalog id
    // `geom_warp_vertices_transformed` (design/counter_ids.lock) and directive
    // section 20 gives it an EXACT meaning: SUCCESSFUL ENABLED APPLICATIONS.
    // Not bypasses -- those have their own counter -- and not both camera
    // landings, because this block runs once across both views.
    output var logic [CNTW-1:0]     vertices_transformed_o,
    output var logic [CNTW-1:0]     bypassed_o,
    output var logic [CNTW-1:0]     app_saturations_o,
    output var logic [CNTW-1:0]     normal_reduced_o,
    output var logic [CNTW-1:0]     degenerate_o,
    output var logic [CNTW-1:0]     bound_violations_o,
    output var logic [CNTW-1:0]     negative_bounds_o,
    output var logic [CNTW-1:0]     normal_width_faults_o,
    output var logic [CNTW-1:0]     profile_mismatches_o,
    output var logic [CNTW-1:0]     field_faults_o,
    // Conservation [contract section 10]: for a completed, non-aborted trace
    //   p_accepts == n_accepts == published vertex outcomes
    //   accepted vertices == bypass outcomes + active-Warp outcomes
    // Two independent accept counters, not one shared one -- a single counter
    // could not see the fork accepting twice on one side, which is the exact
    // fault 11.6 records.
    output var logic [CNTW-1:0]     p_accepts_o,
    output var logic [CNTW-1:0]     n_accepts_o
);

  // ---------------------------------------------------------------- causes ----
  // Mirrors `zref::geom_warp::Refusal` so the RTL and the oracle name the same
  // things. kInputLaneCount and kOutputLaneCount have no RTL member: those are
  // SIGNATURE faults and `zhao_field_warp_adapter` refuses them at ELABORATION
  // (it $fatals on IN_LANES < 15), so they cannot reach a running machine here.
  localparam logic [2:0] C_NONE         = 3'd0;
  localparam logic [2:0] C_PROFILE      = 3'd1;
  localparam logic [2:0] C_NEG_BOUND    = 3'd2;
  localparam logic [2:0] C_BOUND        = 3'd3;
  localparam logic [2:0] C_NORMAL_WIDTH = 3'd4;
  localparam logic [2:0] C_FIELD_FAULT  = 3'd5;

  // The range-reduction ceiling. THE SAME 2^30 as `zref::skin_world_normal` and
  // `zref::geom_warp::kNormalReduceCeiling`. Named, not inlined, so the sites
  // cannot drift apart.
  localparam logic [32:0] NORMAL_CEIL = 33'sd1073741824;  // 1 << 30

  localparam logic [1:0] S_IDLE = 2'd0;
  localparam logic [1:0] S_REQ  = 2'd1;
  localparam logic [1:0] S_HOLD = 2'd2;

  logic [1:0] st_q;

  // ---- the latched vertex + its draw snapshot [W05] -------------------------
  logic signed [31:0] px_q, py_q, pz_q;
  logic signed [31:0] nx_q, ny_q, nz_q;   // narrowed input normal
  logic [127:0]       attr_q;
  logic [SRCW-1:0]    src_q;
  logic               ndegen_q;
  logic               warp_en_q;
  logic [SLOTW-1:0]   slot_q;
  logic               slot_valid_q;
  logic [7:0]         profile_q;
  logic [31:0]        time_q;
  logic [127:0]       par_q;
  logic signed [31:0] bx_q, by_q, bz_q;
  logic               narrow_bad_q;

  // ---- the output hold, with INDEPENDENT consumed bits -----------------------
  logic               hold_p_q, hold_n_q;
  logic signed [31:0] opx_q, opy_q, opz_q;
  logic signed [31:0] onx_q, ony_q, onz_q;
  logic               odegen_q;
  logic [SRCW-1:0]    osrc_q;

  logic [CNTW-1:0] c_xform_q, c_bypass_q, c_appsat_q, c_reduced_q, c_degen_q;
  logic [CNTW-1:0] c_bound_q, c_negb_q, c_nwidth_q, c_prof_q, c_fault_q;
  logic [CNTW-1:0] c_pacc_q, c_nacc_q;

  logic [2:0]         poison_cause_q;
  logic               poison_valid_q;
  logic [SRCW-1:0]    poison_src_q;
  logic signed [31:0] poison_dx_q, poison_dy_q, poison_dz_q;

  // ------------------------------------------------------- seam narrowing ----
  // Directive 5.4. Sign extension must reconstruct the full 64-bit input, or it
  // is a SEAM FAULT. This is `zref::geom_warp::narrow_s64_to_s32`, in hardware.
  // The comparison is written with an EXPLICIT sign extension rather than
  // leaning on the language's implicit widening. Verilator's WIDTHEXPAND caught
  // the implicit form, and it was right to: a 32-vs-64 comparison whose
  // extension you did not write is the exact shape that silently succeeds for
  // small positives and fails for everything else.
  function automatic logic narrow_ok(input logic signed [63:0] v);
    return ($signed({{32{v[31]}}, v[31:0]}) == v);
  endfunction

  logic narrow_bad_c;
  always_comb begin
    narrow_bad_c = !(narrow_ok(v_nx_i) && narrow_ok(v_ny_i) && narrow_ok(v_nz_i));
  end

  // ---------------------------------------------- the canonical saturating add
  // [W03, 5.3] "formed in at least 33 bits", then clamped to signed 32. This is
  // `zref::fx_add`'s law: the sum is computed WIDE and clamped, never wrapped.
  // `sat` is reported so the APPLICATION's saturation stays separable from the
  // PROGRAM's, which 5.3 requires by name.
  function automatic logic signed [32:0] sat_add33(input logic signed [31:0] a,
                                                   input logic signed [31:0] b);
    logic signed [32:0] w;
    logic signed [31:0] clamped;
    logic               sat;
    begin
      w = $signed({a[31], a}) + $signed({b[31], b});
      if (w > 33'sd2147483647) begin
        clamped = 32'sh7FFFFFFF;
        sat     = 1'b1;
      end else if (w < -33'sd2147483648) begin
        clamped = 32'sh80000000;
        sat     = 1'b1;
      end else begin
        clamped = w[31:0];
        sat     = 1'b0;
      end
      // {sat, value}: one return, so the caller cannot use the value while
      // forgetting the flag.
      return {sat, clamped};
    end
  endfunction

  // Widened magnitude. Safe at INT32_MIN, where -x would overflow 32 bits --
  // `zref::geom_warp::abs_widened`'s reason, in hardware.
  //
  // THE EXTENSION MUST BE A SIGN EXTENSION, AND THE FIRST VERSION OF THIS
  // FUNCTION GOT IT WRONG. It read `v[31] ? (33'(~{1'b0, v}) + 33'd1) : ...`,
  // which ZERO-extends to 33 bits and then negates. Zero-extending 0x80000000
  // produces the POSITIVE number 2^31, and negating that in 33 bits gives
  // 2^33 - 2^31, not 2^31. Every negative input came out enormous.
  //
  // It was caught by the differential against `zref::geom_warp::apply_outputs`
  // on the first run, with two symptoms and one cause: displacements were
  // spuriously reported as exceeding their bound (a huge |d| beats any bound),
  // and normals took one reduction shift too many (a huge max(abs) is still
  // above the ceiling after the real loop would have stopped). A hand-written
  // table of expected values would have had to contain a negative INT32_MIN
  // case to notice; the oracle noticed without being asked.
  function automatic logic [32:0] abs33(input logic signed [31:0] v);
    logic signed [32:0] w;
    begin
      w = $signed({v[31], v});
      return v[31] ? 33'(-w) : 33'(w);
    end
  endfunction

  // ----------------------------------------------------- the application law --
  // Everything below is a combinational restatement of
  // `zref::geom_warp::apply_outputs`, in that function's own order. The directed
  // test does NOT trust this comment: it pushes the hardware's six words through
  // the actual C++ function and compares, which is W13's separation applied to
  // correctness rather than to performance.
  logic signed [32:0] addx_c, addy_c, addz_c;
  logic               appsat_c;
  logic               negb_c, boundbad_c;
  logic               degen_c;
  logic signed [31:0] rx_c, ry_c, rz_c;
  logic [1:0]         shifts_c;
  logic               adm_refuse_c;

  // stage 0 -> stage 1 -> stage 2 of the reduction loop, written literally.
  logic signed [31:0] s0x, s0y, s0z, s1x, s1y, s1z, s2x, s2y, s2z;
  logic [32:0]        m0_c, m1_c;
  logic               sh1_c, sh2_c;

  always_comb begin
    // --- 6.3: the bound must be nonnegative -- a COMMAND validity condition ---
    //
    // THE SNAPSHOT, NOT THE PORT [W05]. Both bound tests read `b*_q`, which was
    // latched WITH THIS VERTEX, and never the live `d_b*_i`. W05 forbids a
    // mutable global `current_warp`: "displacement bound belong to THAT draw".
    // The first version of this block compared against `d_b*_i` and Verilator's
    // UNUSEDSIGNAL on `bx_q` is what exposed it -- a descriptor advancing while
    // a vertex was in flight would have been checked against the NEXT draw's
    // bound, and the result would have been a plausible number every time. It is
    // the same live-pin defect class as the doorbell's `cfg_plan_base_i` sampled
    // at drain. A latched operand and a live one are not interchangeable even
    // when they agree in every test you happen to have written.
    negb_c = (bx_q[31] | by_q[31] | bz_q[31]);

    // --- 5.6: the bound applies to the RETURNED displacement -----------------
    // Widened absolute values, so INT32_MIN is DIAGNOSED rather than wrapped.
    boundbad_c = (abs33(f_dx_i) > abs33(bx_q))
              || (abs33(f_dy_i) > abs33(by_q))
              || (abs33(f_dz_i) > abs33(bz_q));

    // --- 5.3: position -------------------------------------------------------
    addx_c   = sat_add33(px_q, f_dx_i);
    addy_c   = sat_add33(py_q, f_dy_i);
    addz_c   = sat_add33(pz_q, f_dz_i);
    appsat_c = addx_c[32] | addy_c[32] | addz_c[32];

    // --- 5.4: the REPLACEMENT normal -----------------------------------------
    // Degeneracy is tested on the words the PROGRAM RETURNED, BEFORE reduction:
    // reduction can only shrink them, so a vector that reduces to zero was
    // already degenerate, and testing after would blame the wrong stage.
    degen_c = (f_onx_i == 32'sd0) && (f_ony_i == 32'sd0) && (f_onz_i == 32'sd0);

    s0x = f_onx_i;  s0y = f_ony_i;  s0z = f_onz_i;

    // stage 1
    m0_c  = abs33(s0x);
    if (abs33(s0y) > m0_c) m0_c = abs33(s0y);
    if (abs33(s0z) > m0_c) m0_c = abs33(s0z);
    sh1_c = (m0_c >= NORMAL_CEIL);
    s1x   = sh1_c ? (s0x >>> 1) : s0x;
    s1y   = sh1_c ? (s0y >>> 1) : s0y;
    s1z   = sh1_c ? (s0z >>> 1) : s0z;

    // stage 2 -- re-derived from the values stage 1 actually produced, which is
    // the whole point of the header's -(2^31 - 1) note.
    m1_c  = abs33(s1x);
    if (abs33(s1y) > m1_c) m1_c = abs33(s1y);
    if (abs33(s1z) > m1_c) m1_c = abs33(s1z);
    sh2_c = sh1_c && (m1_c >= NORMAL_CEIL);
    s2x   = sh2_c ? (s1x >>> 1) : s1x;
    s2y   = sh2_c ? (s1y >>> 1) : s1y;
    s2z   = sh2_c ? (s1z >>> 1) : s1z;

    shifts_c = {1'b0, sh1_c} + {1'b0, sh2_c};
    rx_c = degen_c ? 32'sd0 : s2x;
    ry_c = degen_c ? 32'sd0 : s2y;
    rz_c = degen_c ? 32'sd0 : s2z;

    // --- ADMISSION: refuse BEFORE offering, not after answering --------------
    // Contract section 9 puts `DRAW_INVALID` (bad signature, negative bound) in
    // the "refuse BEFORE emitting" row, and the seam fault is worse still: the
    // normal we would submit is not the one GEOM.SKIN.NORM computed, so no
    // evaluation of it could mean anything.
    //
    // This gate exists because the directed test caught its absence. The FSM
    // already resolved all three correctly -- it just did so at the CLOCK EDGE,
    // while `f_vtx_valid_o` was combinationally high for the cycle in between.
    // One cycle of a valid offer is a real offer: the adapter latches on
    // `req_ready`, and an invalid draw would have entered the shared Field host
    // and consumed a slot before being thrown away. "It refuses correctly" and
    // "it never asks" are different claims and only the second is what W09 and
    // section 9 are about.
    adm_refuse_c = narrow_bad_q || (profile_q != WARP_PROFILE_ID) || negb_c;
  end

  // --------------------------------------------------------------- handshake --
  // `v_ready_o` is a function of state only, never of any downstream ready.
  // Contract section 4 requires a block spliced into the palette->skin fork to
  // keep that property, or the fork's no-deadlock argument stops being true.
  assign v_ready_o = (st_q == S_IDLE);

  assign f_vtx_valid_o  = (st_q == S_REQ) && warp_en_q && !adm_refuse_c;
  assign f_px_o         = px_q;
  assign f_py_o         = py_q;
  assign f_pz_o         = pz_q;
  assign f_nx_o         = nx_q;
  assign f_ny_o         = ny_q;
  assign f_nz_o         = nz_q;
  assign f_attr_o       = attr_q;
  assign f_time_o       = time_q;
  assign f_par_o        = par_q;
  assign f_slot_o       = slot_q;
  assign f_slot_valid_o = slot_valid_q;
  assign f_profile_o    = profile_q;
  // The adapter retires its held offer on `vtx_take_i`. We take exactly when we
  // have consumed the answer -- one take per offer, never a level.
  assign f_vtx_take_o   = (st_q == S_REQ) && warp_en_q && f_ans_valid_i;

  assign o_p_valid_o      = hold_p_q;
  assign o_px_o           = opx_q;
  assign o_py_o           = opy_q;
  assign o_pz_o           = opz_q;
  assign o_p_src_id_o     = osrc_q;
  assign o_n_valid_o      = hold_n_q;
  assign o_nx_o           = onx_q;
  assign o_ny_o           = ony_q;
  assign o_nz_o           = onz_q;
  assign o_n_degenerate_o = odegen_q;
  assign o_n_src_id_o     = osrc_q;

  assign poison_valid_o  = poison_valid_q;
  assign poison_src_id_o = poison_src_q;
  assign poison_cause_o  = poison_cause_q;
  assign poison_dx_o     = poison_dx_q;
  assign poison_dy_o     = poison_dy_q;
  assign poison_dz_o     = poison_dz_q;

  assign vertices_transformed_o = c_xform_q;
  assign bypassed_o             = c_bypass_q;
  assign app_saturations_o      = c_appsat_q;
  assign normal_reduced_o       = c_reduced_q;
  assign degenerate_o           = c_degen_q;
  assign bound_violations_o     = c_bound_q;
  assign negative_bounds_o      = c_negb_q;
  assign normal_width_faults_o  = c_nwidth_q;
  assign profile_mismatches_o   = c_prof_q;
  assign field_faults_o         = c_fault_q;
  assign p_accepts_o            = c_pacc_q;
  assign n_accepts_o            = c_nacc_q;

  // ------------------------------------------------------------------ poison --
  // A local task would hide the counter, so the refusal is written out at each
  // site instead: every poison sets the SAME four registers and exactly one
  // cause counter, and a reader can see that without following a call.
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_q           <= S_IDLE;
      hold_p_q       <= 1'b0;
      hold_n_q       <= 1'b0;
      poison_valid_q <= 1'b0;
      poison_cause_q <= C_NONE;
      poison_src_q   <= '0;
      poison_dx_q    <= '0;
      poison_dy_q    <= '0;
      poison_dz_q    <= '0;
      px_q <= '0; py_q <= '0; pz_q <= '0;
      nx_q <= '0; ny_q <= '0; nz_q <= '0;
      attr_q <= '0; src_q <= '0; ndegen_q <= 1'b0;
      warp_en_q <= 1'b0; slot_q <= '0; slot_valid_q <= 1'b0; profile_q <= '0;
      time_q <= '0; par_q <= '0;
      bx_q <= '0; by_q <= '0; bz_q <= '0;
      narrow_bad_q <= 1'b0;
      opx_q <= '0; opy_q <= '0; opz_q <= '0;
      onx_q <= '0; ony_q <= '0; onz_q <= '0;
      odegen_q <= 1'b0; osrc_q <= '0;
      c_xform_q <= '0; c_bypass_q <= '0; c_appsat_q <= '0; c_reduced_q <= '0;
      c_degen_q <= '0; c_bound_q <= '0; c_negb_q <= '0; c_nwidth_q <= '0;
      c_prof_q <= '0; c_fault_q <= '0; c_pacc_q <= '0; c_nacc_q <= '0;
    end else begin
      poison_valid_q <= 1'b0;  // one cycle per refusal

      // ---- the fork retires independently -----------------------------------
      // Each side clears its OWN hold on its OWN ready. Neither valid is a
      // function of either ready, so one consumer stalling cannot re-present the
      // other's payload, and neither can be accepted twice.
      if (hold_p_q && o_p_ready_i) begin
        hold_p_q <= 1'b0;
        c_pacc_q <= c_pacc_q + 1'b1;
      end
      if (hold_n_q && o_n_ready_i) begin
        hold_n_q <= 1'b0;
        c_nacc_q <= c_nacc_q + 1'b1;
      end

      unique case (st_q)
        // ------------------------------------------------------------- IDLE --
        S_IDLE: begin
          if (v_valid_i && !hold_p_q && !hold_n_q) begin
            // The seam check happens on INTAKE, on the 64-bit wires, before
            // anything narrows them. Checking later would mean checking a value
            // that had already been truncated.
            narrow_bad_q <= narrow_bad_c;
            px_q <= v_px_i; py_q <= v_py_i; pz_q <= v_pz_i;
            nx_q <= v_nx_i[31:0]; ny_q <= v_ny_i[31:0]; nz_q <= v_nz_i[31:0];
            attr_q       <= v_attr_i;
            src_q        <= v_src_id_i;
            ndegen_q     <= v_n_degenerate_i;
            warp_en_q    <= d_warp_en_i;
            slot_q       <= d_slot_i;
            slot_valid_q <= d_slot_valid_i;
            profile_q    <= d_profile_i;
            time_q       <= d_time_i;
            par_q        <= d_par_i;
            bx_q <= d_bx_i; by_q <= d_by_i; bz_q <= d_bz_i;
            st_q <= S_REQ;
          end
        end

        // -------------------------------------------------------------- REQ --
        S_REQ: begin
          if (narrow_bad_q) begin
            // The seam fault outranks everything: the normal we would submit is
            // not the normal GEOM.SKIN.NORM computed, so no evaluation of it
            // could mean anything.
            poison_valid_q <= 1'b1;
            poison_cause_q <= C_NORMAL_WIDTH;
            poison_src_q   <= src_q;
            poison_dx_q    <= '0; poison_dy_q <= '0; poison_dz_q <= '0;
            c_nwidth_q     <= c_nwidth_q + 1'b1;
            st_q           <= S_IDLE;
          end else if (!warp_en_q) begin
            // ---- W09: THE ORDINARY PATH ---------------------------------
            // DrawForm disables Warp and performs ZERO Warp lookups. Nothing
            // is offered to the adapter (`f_vtx_valid_o` is gated on
            // `warp_en_q`), no normalization is introduced, and the vertex is
            // published exactly as it arrived. The upstream normal is already
            // range-reduced by GEOM.SKIN.NORM, so reducing again here would be
            // the "extra normalization on the disabled path" contract section 6
            // forbids -- and would make an identity Warp differ from a bypass.
            opx_q <= px_q; opy_q <= py_q; opz_q <= pz_q;
            onx_q <= nx_q; ony_q <= ny_q; onz_q <= nz_q;
            odegen_q   <= ndegen_q;
            osrc_q     <= src_q;
            hold_p_q   <= 1'b1;
            hold_n_q   <= 1'b1;
            c_bypass_q <= c_bypass_q + 1'b1;
            st_q       <= S_HOLD;
          end else if (profile_q != WARP_PROFILE_ID) begin
            // A profile check BEFORE numeric execution [W27/FH27 in spirit,
            // directive 6.3]. An Earth or Flow program is not a Warp program and
            // its six words would be six different quantities.
            poison_valid_q <= 1'b1;
            poison_cause_q <= C_PROFILE;
            poison_src_q   <= src_q;
            poison_dx_q    <= '0; poison_dy_q <= '0; poison_dz_q <= '0;
            c_prof_q       <= c_prof_q + 1'b1;
            st_q           <= S_IDLE;
          end else if (negb_c) begin
            // Checked against the LIVE descriptor bound, which is stable for the
            // draw. A negative bound is refused before any Field work is done.
            poison_valid_q <= 1'b1;
            poison_cause_q <= C_NEG_BOUND;
            poison_src_q   <= src_q;
            poison_dx_q    <= '0; poison_dy_q <= '0; poison_dz_q <= '0;
            c_negb_q       <= c_negb_q + 1'b1;
            st_q           <= S_IDLE;
          end else if (f_ans_valid_i) begin
            if (!f_warp_valid_i) begin
              // An answer that is not a result. W10: this must NOT become an
              // identity deformation and must NOT become a zero.
              poison_valid_q <= 1'b1;
              poison_cause_q <= C_FIELD_FAULT;
              poison_src_q   <= src_q;
              poison_dx_q    <= '0; poison_dy_q <= '0; poison_dz_q <= '0;
              c_fault_q      <= c_fault_q + 1'b1;
              st_q           <= S_IDLE;
            end else if (boundbad_c) begin
              // 5.6 / W11. POISON, do not clamp. The returned displacement is
              // preserved on the poison port precisely because the diagnosis is
              // made from it.
              poison_valid_q <= 1'b1;
              poison_cause_q <= C_BOUND;
              poison_src_q   <= src_q;
              poison_dx_q    <= f_dx_i;
              poison_dy_q    <= f_dy_i;
              poison_dz_q    <= f_dz_i;
              c_bound_q      <= c_bound_q + 1'b1;
              st_q           <= S_IDLE;
            end else begin
              opx_q <= addx_c[31:0];
              opy_q <= addy_c[31:0];
              opz_q <= addz_c[31:0];
              onx_q <= rx_c; ony_q <= ry_c; onz_q <= rz_c;
              odegen_q <= degen_c;
              osrc_q   <= src_q;
              hold_p_q <= 1'b1;
              hold_n_q <= 1'b1;
              if (appsat_c)       c_appsat_q  <= c_appsat_q + 1'b1;
              if (shifts_c != 2'd0) c_reduced_q <= c_reduced_q + 1'b1;
              if (degen_c)        c_degen_q   <= c_degen_q + 1'b1;
              // The ONE counter directive section 20 pins to an exact meaning.
              c_xform_q <= c_xform_q + 1'b1;
              st_q      <= S_HOLD;
            end
          end
        end

        // ------------------------------------------------------------- HOLD --
        S_HOLD: begin
          // Both consumers must have taken their half before the next vertex is
          // admitted. This is where "exactly once each" is enforced.
          if ((!hold_p_q || o_p_ready_i) && (!hold_n_q || o_n_ready_i)) begin
            st_q <= S_IDLE;
          end
        end

        default: st_q <= S_IDLE;
      endcase
    end
  end

  // A `shifts_q` register stood here, with a comment saying it was "exposed so
  // the directed test can compare it against the oracle's `normal_shifts`".
  // IT WAS NOT A PORT. It was a register nothing read, kept alive only by a
  // lint waiver, under a comment asserting an observation path that did not
  // exist -- the same shape as a status port whose services all terminate in
  // `*_unused`, just smaller and written by me.
  //
  // It is deleted rather than promoted to a port, because the property it
  // claimed to watch IS already watched, two ways that do not depend on it:
  // `geom_warp_rtl_directed` FT096 asserts DIRECTLY that every published normal
  // satisfies max(abs) < 2^30, and the differential compares the reduced value
  // itself against `zref::geom_warp::apply_outputs` on every vertex. A wrong
  // shift count cannot pass either. The shift COUNT is corroboration; the
  // VALUE is the thing, and the value is checked.
  //
  // `normal_reduced_o` remains and counts vertices that needed any shift at
  // all, which is a real port with a real reader.

  // ---- elaboration checks ----------------------------------------------------
  // Quartus 17.0 needs these INSIDE `initial begin ... end`; a bare module-scope
  // `if` is a syntax error there while Verilator lints it clean. CLAUDE.md
  // records both forms. And note `--lint-only` does NOT run `initial` blocks, so
  // a clean lint says nothing whatever about these firing.
  initial begin
    if (SRCW < 1)  $fatal(1, "zhao_geom_warp: SRCW must be at least 1");
    if (SLOTW < 1) $fatal(1, "zhao_geom_warp: SLOTW must be at least 1");
    if (CNTW < 8)  $fatal(1, "zhao_geom_warp: CNTW=%0d is too narrow to be evidence", CNTW);
  end

endmodule

`default_nettype wire
