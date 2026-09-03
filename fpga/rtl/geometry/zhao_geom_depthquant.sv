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
// ENFORCED-BY: tests/geometry/geom_depthquant_directed.cpp:main
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

  // ---- the generated profile table -----------------------------------------
  // Values mirrored from reference/include/zref/generated/zref_depth.hpp. They
  // are checked against it by the directed test rather than trusted: a table
  // and its user drifting is the QFMT_VERSION failure in a different costume,
  // and that one actually happened this morning.
  localparam logic [39:0] WMIN [3] = '{40'd65536,      40'd32768,     40'd16384};
  localparam logic [39:0] WMAX [3] = '{40'd1073741824, 40'd536870912, 40'd134217728};
  // SCALE is a power of two for these three profiles ONLY, so it is carried as
  // its log2 and the multiply becomes a shift. A fourth profile whose SCALE is
  // not a power of two would need a real multiplier, and that is a deliberate
  // limitation rather than an assumption: 2^40, 2^39, 2^38.
  localparam int unsigned SCALE_LOG2 [3] = '{40, 39, 38};

  localparam logic [23:0] DEPTH_MAX = 24'hFFFFFF;

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

  // ---- the clamp, which is part of the law ---------------------------------
  logic [39:0] w_clamped_c;
  logic        near_c, far_c;
  always_comb begin
    near_c = (v_w_i < WMIN[v_profile_i]);
    far_c  = (v_w_i > WMAX[v_profile_i]);
    if (near_c)      w_clamped_c = WMIN[v_profile_i];
    else if (far_c)  w_clamped_c = WMAX[v_profile_i];
    else             w_clamped_c = v_w_i;
  end

  // ---- the normalisation shift: smallest s with (W >> s) < 2^24 ------------
  // s is set by the HIGHEST set bit, so the loop must ASCEND: the last
  // assignment wins in an unrolled priority chain, and a descending loop picks
  // the LOWEST set bit above 23 instead. That is not a subtle difference -- for
  // a w with a run of high bits it under-shifts by the width of the run, and
  // the directed test caught it as depth exactly 2^5 too large at wmax.
  logic [5:0] shift_c;
  always_comb begin
    shift_c = 6'd0;
    for (int i = 24; i <= 39; i++) begin
      if (w_clamped_c[i]) shift_c = 6'(i - 23);
    end
  end

  assign rcp_d_o = 24'(w_q >> s_q);

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
  logic signed [8:0] sh_c;
  logic [23:0]       q_c;
  logic              sat_c, sh_bad_c;
  always_comb begin
    automatic logic [63:0] wide;
    automatic logic [63:0] half;
    automatic logic [63:0] res;
    sh_c     = 9'(48) + 9'(s_q) - 9'(rcp_k_i) - 9'(SCALE_LOG2[prof_q]);
    sh_bad_c = (sh_c < 9'sd0) || (sh_c > 9'sd40);
    q_c      = '0;
    sat_c    = 1'b0;
    wide     = 64'(rcp_r_i);
    half     = (sh_c == 9'sd0) ? 64'd0 : (64'd1 << (6'(sh_c) - 6'd1));
    res      = (wide + half) >> 6'(sh_c);
    if (!sh_bad_c) begin
      if (res > 64'(DEPTH_MAX)) begin
        q_c   = DEPTH_MAX;
        sat_c = 1'b1;
      end else begin
        q_c = 24'(res);
      end
    end
  end

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

`default_nettype wire
