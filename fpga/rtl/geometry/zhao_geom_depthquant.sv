// zhao_geom_depthquant.sv — w to invw24, under the selected depth profile.
//
// ---------------------------------------------------------------------------
// WHY THIS BLOCK EXISTS
// ---------------------------------------------------------------------------
// `reports/BORING_3D_FUNDAMENTALS_AUDIT.md` R6, verified against the tree:
//
//   * zhao_geom_project.sv emits `out_d_o`, documented "Q16.16 1/w";
//   * TWELVE RTL files consume `invw24`;
//   * no `depth_profile` port exists anywhere in fpga/rtl, though the ABI
//     carries a two-bit profile in SetView.
//
// Twelve consumers use a value called `invw24` whose producer emits something
// else, and the conversion had no home. Owner ruling D-4: a separate named
// block, and "all downstream consumers receive only the canonical invw24. No
// consumer performs its own profile conversion."
//
// ---------------------------------------------------------------------------
// THE INPUT IS w, NOT 1/w
// ---------------------------------------------------------------------------
// The ratified law (spec/qformats.md 8, owner ruling 2026-08-31 #1) is
//
//     s      = smallest shift with (W >> s) < 2^24      -- W is w, fx16 raw
//     {r, k} = rcp_u24(W >> s)
//     d      = rescale(SCALE * r, 48 + s - k), round-half-up, sat 0xFFFFFF
//
// It consumes w and performs its OWN reciprocal. The first draft of the
// contract said this block rescaled the projector's reciprocal; reading
// `zref::depth_of_raw` corrected it. A block built to the draft would have
// been fetching the wrong quantity.
//
// `zhao_project_core` HAS w -- "the three quotients share the divisor clip.w"
// -- and does not expose it. Exposing it is a required change to that block,
// named in this block's contract.
//
// ---------------------------------------------------------------------------
// THREE THINGS THE ORACLE MAKES EXPLICIT AND THIS COPIES
// ---------------------------------------------------------------------------
//   * the clamp to [wmin, wmax] is part of the LAW, not a caller courtesy.
//     wmax is a depth CLAMP, not a far-clip plane, so a w beyond it is legal
//     geometry sharing the floor depth rather than being culled.
//   * the product SCALE * r reaches ~2^80, so the intermediate is 128 bits.
//     A 64-bit accumulator "would truncate silently and produce a plausible
//     wrong depth".
//   * an out-of-range shift returns 0 rather than a wrong number, because
//     "returning a wrong number quietly is worse than clamping loudly at the
//     floor".
//
// ---------------------------------------------------------------------------
// ONE LAW, TWO SCHEDULES (split 2026-09-19, owner ruling R31)
// ---------------------------------------------------------------------------
// The law's two halves are the two leaf modules at the end of this file:
//
//   zhao_geom_depthquant_pre   w, profile      -> clamped W, shift s, near/far
//   zhao_geom_depthquant_post  r, k, s, profile -> invw24, saturated, refused
//
// and every schedule that computes invw24 instantiates THOSE, so the
// arithmetic exists once. Two schedules do:
//
//   * `zhao_geom_depthquant` -- the original one-vertex-at-a-time FSM, ports
//     unchanged, still used by `tests/shell/tb_zhao_shell.sv`.
//   * `zhao_geom_depthquant_stream` -- a TAGGED stream for R31's rate work:
//     one vertex accepted per clock while a context is free, many in flight on
//     the multi-context `zhao_raster_rcp24_v4`, answers in the reciprocal's
//     COMPLETION order with the caller's tag. GEOM.REPLAY used to run three
//     FSM lanes per TRIANGLE CORNER (56 clocks per view-triangle, 2.7x the
//     frame); GEOM.VATTR runs this stream once per LANDED VERTEX instead.
//
// The split moved lines, it did not rewrite them: the clamp, the ascending
// shift loop, the combine and its near-pin comment are the same statements.
//
// ENFORCED-BY: tests/geometry/geom_depthquant_directed.cpp:main (the FSM, the
// law, the table), tests/geometry/geom_vattr_directed.cpp (the stream)
`default_nettype none

module zhao_geom_depthquant #(
    parameter int unsigned SRCW = 16
) (
    input var logic clk,
    input var logic rst_n,

    // ---- one projected vertex ----------------------------------------------
    input  var logic              v_valid_i,
    output var logic              v_ready_o,
    // w in fx16 raw (S15.16). Wide because wmax for WORLD_LONG is
    // 1,073,741,824 -- 16384 m in fx16 -- which needs 31 bits.
    input  var logic [39:0]       v_w_i,
    input  var logic              v_behind_i,
    // The profile travels PER VERTEX rather than as latched state: a capture
    // must replay without depending on command ordering, and two views may
    // legally differ.
    input  var logic [1:0]        v_profile_i,
    input  var logic [SRCW-1:0]   v_src_id_i,

    // ---- the canonical depth -----------------------------------------------
    output var logic              d_valid_o,
    input  var logic              d_ready_i,
    output var logic [23:0]       d_invw24_o,
    output var logic              d_behind_o,
    output var logic [SRCW-1:0]   d_src_id_o,

    // ---- the reciprocal service --------------------------------------------
    // The console already has ONE rcp_u24 law; a second ROM would be a second
    // law. This block calls the existing block rather than reimplementing it.
    output var logic              rcp_valid_o,
    input  var logic              rcp_ready_i,
    output var logic [23:0]       rcp_d_o,
    input  var logic              rcp_rvalid_i,
    output var logic              rcp_rready_o,
    input  var logic [23:0]       rcp_r_i,
    input  var logic [5:0]        rcp_k_i,

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0]       vertices_o,
    output var logic [31:0]       clamped_near_o,   // w below wmin
    output var logic [31:0]       clamped_far_o,    // w above wmax
    output var logic [31:0]       saturated_o,      // hit 0xFFFFFF
    output var logic [31:0]       refused_o         // malformed
);

  // S_RCP issues the request and waits for it to be ACCEPTED; S_WAIT waits for
  // the answer. Collapsing the two -- asserting valid and waiting for the
  // response without ever checking `ready` -- would deadlock against a busy
  // service, because the request might never have been taken.
  typedef enum logic [2:0] { S_IDLE, S_RCP, S_WAIT, S_COMB, S_HOLD } state_e;
  state_e st_q;

  logic [39:0]     w_q;
  logic [1:0]      prof_q;
  logic            behind_q;
  logic [SRCW-1:0] src_q;
  logic [5:0]      s_q;          // normalisation shift

  assign v_ready_o    = (st_q == S_IDLE);
  assign rcp_valid_o  = (st_q == S_RCP);
  assign rcp_rready_o = (st_q == S_WAIT);
  assign d_valid_o    = (st_q == S_HOLD);
  assign d_behind_o   = behind_q;
  assign d_src_id_o   = src_q;

  // ---- the law, once: the clamp and the normalisation shift ------------------
  logic [39:0] w_clamped_c;
  logic        near_c, far_c;
  logic [5:0]  shift_c;
  zhao_geom_depthquant_pre u_pre (
      .w_i       (v_w_i),
      .profile_i (v_profile_i),
      .w_clamped_o(w_clamped_c),
      .shift_o   (shift_c),
      .near_o    (near_c),
      .far_o     (far_c)
  );

  assign rcp_d_o = 24'(w_q >> s_q);

  // ---- the law, once: the combine --------------------------------------------
  // Read in S_COMB, the clock after the answer was accepted, against a service
  // that holds its answer (the contract this FSM was written to).
  logic [23:0] q_c;
  logic        sat_c, sh_bad_c;
  zhao_geom_depthquant_post u_post (
      .r_i       (rcp_r_i),
      .k_i       (rcp_k_i),
      .s_i       (s_q),
      .profile_i (prof_q),
      .q_o       (q_c),
      .sat_o     (sat_c),
      .bad_o     (sh_bad_c)
  );

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_q           <= S_IDLE;
      d_invw24_o     <= '0;
      vertices_o     <= '0;
      clamped_near_o <= '0;
      clamped_far_o  <= '0;
      saturated_o    <= '0;
      refused_o      <= '0;
    end else begin
      case (st_q)
        S_IDLE: begin
          if (v_valid_i) begin
            vertices_o <= vertices_o + 32'd1;
            prof_q     <= v_profile_i;
            behind_q   <= v_behind_i;
            src_q      <= v_src_id_i;
            if (near_c) clamped_near_o <= clamped_near_o + 32'd1;
            if (far_c)  clamped_far_o  <= clamped_far_o  + 32'd1;
            w_q  <= w_clamped_c;
            s_q  <= shift_c;
            st_q <= S_RCP;
          end
        end

        S_RCP: begin
          // The request is only issued once the service TAKES it.
          if (rcp_ready_i) st_q <= S_WAIT;
        end

        S_WAIT: begin
          if (rcp_rvalid_i) st_q <= S_COMB;
        end

        S_COMB: begin
          // An out-of-range shift returns 0 and is COUNTED, rather than a
          // plausible wrong depth. The three shipped profiles never produce
          // it; a fourth that did would be unusable.
          if (sh_bad_c) begin
            d_invw24_o <= 24'd0;
            refused_o  <= refused_o + 32'd1;
          end else begin
            d_invw24_o <= q_c;
            if (sat_c) saturated_o <= saturated_o + 32'd1;
          end
          st_q <= S_HOLD;
        end

        S_HOLD: begin
          if (d_ready_i) st_q <= S_IDLE;
        end

        default: st_q <= S_IDLE;
      endcase
    end
  end

endmodule : zhao_geom_depthquant

// The law's leaves and its second schedule live in THIS file on purpose -- one
// file, one law -- so the one-module-per-file naming rule is waived for them.
/* verilator lint_off DECLFILENAME */

// ===========================================================================
// zhao_geom_depthquant_stream -- the SAME law on a tagged stream.
//
// One vertex is accepted per clock while a context is free. Its clamp and
// shift are taken at acceptance (the `_pre` leaf), the reciprocal request
// leaves from a one-deep register with the context index as its token, and
// each answer is combined (the `_post` leaf) against the side record its token
// names. Answers leave in the reciprocal's COMPLETION order, carrying the
// caller's `v_tag_i` -- a caller that needs order must key by the tag, which is
// exactly what GEOM.VATTR does ({arena, index} is the tag).
//
// NSLOT bounds what is in flight and must cover the reciprocal's latency at
// its throughput: `zhao_raster_rcp24_v4` launches one job per clock on four
// phases through a ten-clock loop, so a nonzero reciprocal takes ~40 clocks
// and at most one completes per four -- ten or so in flight keeps it busy.
// Sixteen, matching the reciprocal's own NCTX, is the default for that reason
// and is a knob, not a derived minimum.
//
// A token naming a context that is not in flight is a FAULT (the reciprocal
// answering a job nobody asked for): it is counted on `tok_stray_o` and the
// answer is dropped rather than written against a stale side record. It is
// unreachable with a correct service, and fired by stimulus in
// tests/geometry/geom_vattr_directed.cpp, which drives this block's ports.
// ===========================================================================
module zhao_geom_depthquant_stream #(
    parameter int unsigned TAGW  = 16,
    parameter int unsigned NSLOT = 16
) (
    input var logic clk,
    input var logic rst_n,

    input  var logic              v_valid_i,
    output var logic              v_ready_o,
    input  var logic [39:0]       v_w_i,
    input  var logic [1:0]        v_profile_i,
    input  var logic [TAGW-1:0]   v_tag_i,

    output var logic              d_valid_o,
    input  var logic              d_ready_i,
    output var logic [23:0]       d_invw24_o,
    output var logic [TAGW-1:0]   d_tag_o,

    output var logic              rcp_valid_o,
    input  var logic              rcp_ready_i,
    output var logic [23:0]       rcp_d_o,
    output var logic [7:0]        rcp_tok_o,
    input  var logic              rcp_rvalid_i,
    output var logic              rcp_rready_o,
    input  var logic [23:0]       rcp_r_i,
    input  var logic [5:0]        rcp_k_i,
    input  var logic [7:0]        rcp_tok_i,

    output var logic [31:0]       vertices_o,
    output var logic [31:0]       clamped_near_o,
    output var logic [31:0]       clamped_far_o,
    output var logic [31:0]       saturated_o,
    output var logic [31:0]       refused_o,
    output var logic [31:0]       tok_stray_o,
    output var logic              idle_o
);

  localparam int unsigned SW = (NSLOT <= 2) ? 1 : $clog2(NSLOT);

  initial begin
    if (NSLOT < 2 || NSLOT > 256)
      $fatal(1, "zhao_geom_depthquant_stream: NSLOT (%0d) must be 2..256 -- the context index rides an 8-bit token",
             NSLOT);
  end

  // ---- the context bitmap, lowest free first --------------------------------
  logic [NSLOT-1:0] busy_q;
  logic             any_free_c;
  logic [SW-1:0]    free_c;
  integer fi;
  always_comb begin
    any_free_c = 1'b0;
    free_c     = '0;
    for (fi = NSLOT - 1; fi >= 0; fi = fi - 1) begin
      if (!busy_q[fi]) begin
        any_free_c = 1'b1;
        free_c     = SW'(fi);
      end
    end
  end

  // ---- the side records, one per context ------------------------------------
  logic [5:0]      sd_s_q    [NSLOT];
  logic [1:0]      sd_prof_q [NSLOT];
  logic [TAGW-1:0] sd_tag_q  [NSLOT];

  // ---- the law at acceptance ------------------------------------------------
  logic [39:0] w_clamped_c;
  logic        near_c, far_c;
  logic [5:0]  shift_c;
  zhao_geom_depthquant_pre u_pre (
      .w_i       (v_w_i),
      .profile_i (v_profile_i),
      .w_clamped_o(w_clamped_c),
      .shift_o   (shift_c),
      .near_o    (near_c),
      .far_o     (far_c)
  );

  // ---- the request register ---------------------------------------------------
  logic          a_v_q;
  logic [23:0]   a_d_q;
  logic [SW-1:0] a_slot_q;

  wire rcp_take_c = a_v_q && rcp_ready_i;
  assign v_ready_o   = any_free_c && (!a_v_q || rcp_ready_i);
  wire acc_c       = v_valid_i && v_ready_o;
  assign rcp_valid_o = a_v_q;
  assign rcp_d_o     = a_d_q;
  assign rcp_tok_o   = 8'(a_slot_q);

  // ---- the answer, combined against its own side record ---------------------
  wire [SW-1:0] r_slot_c = rcp_tok_i[SW-1:0];
  wire          r_known_c = (32'(rcp_tok_i) < NSLOT) && busy_q[r_slot_c];
  logic [23:0]  q_c;
  logic         sat_c, bad_c;
  zhao_geom_depthquant_post u_post (
      .r_i       (rcp_r_i),
      .k_i       (rcp_k_i),
      .s_i       (sd_s_q[r_slot_c]),
      .profile_i (sd_prof_q[r_slot_c]),
      .q_o       (q_c),
      .sat_o     (sat_c),
      .bad_o     (bad_c)
  );

  assign rcp_rready_o = !d_valid_o || d_ready_i;
  wire ans_c = rcp_rvalid_i && rcp_rready_o;

  assign idle_o = (busy_q == '0) && !a_v_q && !d_valid_o;

  integer si;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      busy_q         <= '0;
      a_v_q          <= 1'b0;
      a_d_q          <= '0;
      a_slot_q       <= '0;
      d_valid_o      <= 1'b0;
      d_invw24_o     <= '0;
      d_tag_o        <= '0;
      vertices_o     <= '0;
      clamped_near_o <= '0;
      clamped_far_o  <= '0;
      saturated_o    <= '0;
      refused_o      <= '0;
      tok_stray_o    <= '0;
      for (si = 0; si < NSLOT; si = si + 1) begin
        sd_s_q[si]    <= '0;
        sd_prof_q[si] <= '0;
        sd_tag_q[si]  <= '0;
      end
    end else begin
      // --- the output register ---------------------------------------------
      if (d_valid_o && d_ready_i) d_valid_o <= 1'b0;

      // --- an answer: combine, free its context -----------------------------
      if (ans_c) begin
        if (r_known_c) begin
          busy_q[r_slot_c] <= 1'b0;
          d_valid_o        <= 1'b1;
          d_tag_o          <= sd_tag_q[r_slot_c];
          // Out-of-range shift: 0 and COUNTED, exactly as the FSM does.
          if (bad_c) begin
            d_invw24_o <= 24'd0;
            refused_o  <= refused_o + 32'd1;
          end else begin
            d_invw24_o <= q_c;
            if (sat_c) saturated_o <= saturated_o + 32'd1;
          end
        end else if (tok_stray_o != 32'hFFFF_FFFF) begin
          tok_stray_o <= tok_stray_o + 32'd1;
        end
      end

      // --- the request register ---------------------------------------------
      if (rcp_take_c) a_v_q <= 1'b0;
      if (acc_c) begin
        // Set AFTER the answer's clear above: a context freed and reused on one
        // clock is impossible (acceptance picks a context that was free BEFORE
        // this edge), so the two never name the same index.
        busy_q[free_c]    <= 1'b1;
        sd_s_q[free_c]    <= shift_c;
        sd_prof_q[free_c] <= v_profile_i;
        sd_tag_q[free_c]  <= v_tag_i;
        a_v_q    <= 1'b1;
        a_d_q    <= 24'(w_clamped_c >> shift_c);
        a_slot_q <= free_c;
        vertices_o <= vertices_o + 32'd1;
        if (near_c) clamped_near_o <= clamped_near_o + 32'd1;
        if (far_c)  clamped_far_o  <= clamped_far_o  + 32'd1;
      end
    end
  end

endmodule : zhao_geom_depthquant_stream

// ===========================================================================
// THE LAW'S TWO HALVES. Combinational; nothing else in the tree computes them.
// ===========================================================================
module zhao_geom_depthquant_pre (
    input  var logic [39:0] w_i,
    input  var logic [1:0]  profile_i,
    output var logic [39:0] w_clamped_o,
    output var logic [5:0]  shift_o,
    output var logic        near_o,
    output var logic        far_o
);
  // ---- the generated profile table -----------------------------------------
  // Values mirrored from reference/include/zref/generated/zref_depth.hpp. They
  // are checked against it by the directed test rather than trusted: a table
  // and its user drifting is the QFMT_VERSION failure in a different costume,
  // and that one actually happened this morning.
  localparam logic [39:0] WMIN [3] = '{40'd65536,      40'd32768,     40'd16384};
  localparam logic [39:0] WMAX [3] = '{40'd1073741824, 40'd536870912, 40'd134217728};

  // THE RESERVED PROFILE (2'd3) indexes nothing: the tables have three rows.
  // It used to read one past the end of all three -- whatever the tool made of
  // that -- and was "refused" only when the result happened to land out of
  // range. It is now DEFINED: the pre-stage reads row 0 (a legal, discarded
  // value) and `zhao_geom_depthquant_post` refuses the vertex outright (depth 0,
  // counted). `zhao_project_core` refuses 2'd3 upstream too; this is the second
  // line, and it no longer depends on an out-of-bounds read.
  wire [1:0] prof_c = (profile_i == 2'd3) ? 2'd0 : profile_i;

  // ---- the clamp, which is part of the law ---------------------------------
  always_comb begin
    near_o = (w_i < WMIN[prof_c]);
    far_o  = (w_i > WMAX[prof_c]);
    if (near_o)      w_clamped_o = WMIN[prof_c];
    else if (far_o)  w_clamped_o = WMAX[prof_c];
    else             w_clamped_o = w_i;
  end

  // ---- the normalisation shift: smallest s with (W >> s) < 2^24 ------------
  // s is set by the HIGHEST set bit, so the loop must ASCEND: the last
  // assignment wins in an unrolled priority chain, and a descending loop picks
  // the LOWEST set bit above 23 instead. That is not a subtle difference -- for
  // a w with a run of high bits it under-shifts by the width of the run, and
  // the directed test caught it as depth exactly 2^5 too large at wmax.
  always_comb begin
    shift_o = 6'd0;
    for (int i = 24; i <= 39; i++) begin
      if (w_clamped_o[i]) shift_o = 6'(i - 23);
    end
  end
endmodule : zhao_geom_depthquant_pre

module zhao_geom_depthquant_post (
    input  var logic [23:0] r_i,
    input  var logic [5:0]  k_i,
    input  var logic [5:0]  s_i,
    input  var logic [1:0]  profile_i,
    output var logic [23:0] q_o,
    output var logic        sat_o,
    output var logic        bad_o
);
  // SCALE is a power of two for these three profiles ONLY, so it is carried as
  // its log2 and the multiply becomes a shift. A fourth profile whose SCALE is
  // not a power of two would need a real multiplier, and that is a deliberate
  // limitation rather than an assumption: 2^40, 2^39, 2^38.
  localparam int unsigned SCALE_LOG2 [3] = '{40, 39, 38};

  localparam logic [23:0] DEPTH_MAX = 24'hFFFFFF;

  // ---- the combine: (SCALE * r) >> (48 + s - k), round-half-up -------------
  // SCALE is 2^SCALE_LOG2, so SCALE * r is r << SCALE_LOG2 and the whole
  // expression collapses to a single shift of r. The 128-bit intermediate the
  // oracle needs is therefore never materialised -- but the SHIFT AMOUNT still
  // has to be computed in a width that cannot wrap.
  // A shift of ZERO is not an edge case -- it is the near pin. The generator
  // SOLVES each profile's SCALE so that at wmin the shift lands on exactly
  // zero and the depth IS the raw reciprocal, which is how 0xFFFFFF is hit
  // exactly rather than one short. Rejecting sh_c == 0 as out of range refused
  // every near-plane vertex and returned depth 0 -- the value that means "far"
  // -- for the closest geometry in the scene.
  // The reserved profile is refused outright -- see `zhao_geom_depthquant_pre`.
  wire [1:0] prof_c = (profile_i == 2'd3) ? 2'd0 : profile_i;

  logic signed [8:0] sh_c;
  always_comb begin
    automatic logic [63:0] wide;
    automatic logic [63:0] half;
    automatic logic [63:0] res;
    sh_c  = 9'(48) + 9'(s_i) - 9'(k_i) - 9'(SCALE_LOG2[prof_c]);
    bad_o = (profile_i == 2'd3) || (sh_c < 9'sd0) || (sh_c > 9'sd40);
    q_o   = '0;
    sat_o = 1'b0;
    wide  = 64'(r_i);
    half  = (sh_c == 9'sd0) ? 64'd0 : (64'd1 << (6'(sh_c) - 6'd1));
    res   = (wide + half) >> 6'(sh_c);
    if (!bad_o) begin
      if (res > 64'(DEPTH_MAX)) begin
        q_o   = DEPTH_MAX;
        sat_o = 1'b1;
      end else begin
        q_o = 24'(res);
      end
    end
  end
endmodule : zhao_geom_depthquant_post
/* verilator lint_on DECLFILENAME */

`default_nettype wire
