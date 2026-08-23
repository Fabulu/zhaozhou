// zhao_geom_skin.sv — GEOM.SKIN: rigid and two-weight skinning.
//
// Contract: design/contracts/GEOM.SKIN.md
// Reference: `zref::skin_vertex` (reference/include/zref/zref_creature.hpp,
// implemented in reference/src/zcreature/creature_core.cpp). That function is
// what every creature the reference renderer has ever drawn was skinned with,
// so "RTL matches the oracle" means "the hardware moves vertices exactly where
// the shipped pictures put them".
//
// ---------------------------------------------------------------------------
// THE LAW, and the one part of it that is easy to get wrong
// ---------------------------------------------------------------------------
//
// Rigid (`b1 == b0` or `w0 == 64`), one bone:
//
//     o = rescale( A.m[r]*x + A.m[r+1]*y + A.m[r+2]*z + (A.m[r+3] << 16), 16 )
//
// Two-weight, otherwise, with `w1 = 64 - w0`:
//
//     pa = A.m[r]*x + A.m[r+1]*y + A.m[r+2]*z + (A.m[r+3] << 16)
//     pb = B.m[r]*x + B.m[r+1]*y + B.m[r+2]*z + (B.m[r+3] << 16)
//     o  = rescale( w0*pa + w1*pb, 22 )
//
// **SINGLE ROUNDING IS THE LAW** (qformats §3, A3b). `pa` and `pb` are NEVER
// rounded before the blend: the whole expression is exact and rounded ONCE, by
// 22 — sixteen fraction bits from the matrix product plus six from the 1/64
// weight quanta. Rounding the two skins separately and then blending would be
// a double rounding and would disagree with the reference by an LSB on a large
// share of vertices, which is exactly the sort of difference that shows up as
// a shimmering silhouette rather than as an obvious break.
//
// The rigid path is not an optimisation of the blend path; it rescales by 16,
// not 22, because there is no weight scale in it. Treating rigid as
// `w0 = 64, w1 = 0` through the 22 path is arithmetically identical and is
// what the reference's own branch avoids, so this block branches too and the
// directed test pins both against the same oracle.
//
// ---------------------------------------------------------------------------
// WHY THIS BLOCK IS SEQUENCED — the rate, derived, not assumed
// ---------------------------------------------------------------------------
// The first implementation issued all EIGHTEEN 32x32 products in one clock and
// measured **72 DSP blocks on a 112-DSP device** — 64% of the chip for one
// stage (`reports/synthesis/zhao_block_fit.json`, commit 16df9ee). It was left
// that way deliberately: `reports/REMAINING_BLOCKERS.md` could not answer
// whether skinning's RATE justified the parallelism without a stated vertex
// budget, and guessed that it might.
//
// The budget is now stated: **~120,000 skinned vertex instances per 60 Hz
// frame**, the point at which the Measure degrades. So:
//
//     gpu_clk                  100 MHz  (10.000 ns, zhao_shell_fit.sdc)
//     clocks per frame         1,666,666
//     demand                   120,000 vertices
//     clocks available/vertex  13.88
//     products needed/vertex   18 (two-weight), 9 (rigid)
//     honest multiplier count  18 / 13.88 = 1.30
//
// **The one-clock form was over-provisioned by 13.9x.** Sizing the farm to the
// frame demand instead of to a placeholder throughput is the whole change.
//
// 72 = 18 x 4: a signed 32x32 in the old combinational cone cost four DSP
// blocks. The six weight multiplies contributed approximately NONE of it —
// `w0` and `w1` are seven bits and Quartus had already put them in logic. That
// matters, because the `(pb << 6) + w0*(pa - pb)` identity used below is real
// and exact but it buys **ALMs, not DSPs**. The DSP win is entirely in the
// 32x32 farm, and a report claiming otherwise would be claiming the wrong win.
//
// ---------------------------------------------------------------------------
// LANES BY TERM, NOT BY ROW — and why that is not the obvious choice
// ---------------------------------------------------------------------------
// The obvious sequencing is three ROW lanes, one per output row, each walking
// its six products. It was evaluated and rejected:
//
//   * a row lane must mux its coordinate over {x, y, z} AND its matrix element
//     over six values, every cycle;
//   * a row's six products serialise into six cycles, so the earliest any row
//     can finish is the LAST issue cycle, and the blend cannot overlap.
//
// Lanes by TERM invert both. Lane 0 always multiplies by `x`, lane 1 by `y`,
// lane 2 by `z` — **the coordinate mux disappears entirely**, it is wiring. A
// whole row-product is then issued in ONE cycle as a three-lane dot product,
// so `pa[r]` and `pb[r]` become final three cycles apart and the blend walk
// overlaps the tail of the issue walk instead of queueing behind it. That
// overlap is worth two cycles at MUL_LANES = 3 and is why one shared blend
// unit costs nothing against three parallel ones.
//
// ---------------------------------------------------------------------------
// THE RESOURCE FRONTIER IS THE DELIVERABLE, NOT ONE POINT
// ---------------------------------------------------------------------------
// `MUL_LANES` decomposes into TL term lanes x RL row-product lanes. Latency is
// accept to `o_valid_o`, which is also the issue interval, all-blend worst
// case, never-stalling consumer, 1,666,666 clocks per 60 Hz frame:
//
//   MUL_LANES | TL | RL | issue slots | latency blend/rigid | vertices/frame
//   ----------+----+----+-------------+---------------------+---------------
//       1     |  1 |  1 |   18 /  9   |      22 / 13        |  75,757   FAILS
//       3     |  3 |  1 |    6 /  3   |      10 /  7        | 166,666   1.39x
//       6     |  3 |  2 |    3 /  2   |       8 /  7        | 208,333   1.74x
//
// **MUL_LANES = 1 is on this list precisely because it FAILS the demand.** A
// frontier with no failing end does not show where the wall is. 3 is the
// intended setting; 6 exists to price the parallel end honestly.
//
// **MUL_LANES = 2 is illegal, and that is not an oversight.** Two divides
// neither the three terms nor the three rows, so a cycle would straddle two
// row-products and every accumulator would need a masked multi-source adder.
// Elaboration refuses it rather than quietly generating something slower and
// larger than 3.
//
// ---------------------------------------------------------------------------
// WIDTHS, PROVEN — because operand slack is not free (QUARTUS_GOTCHAS §5)
// ---------------------------------------------------------------------------
// §5 cost `zhao_geom_lod` ten DSP blocks: a 72-bit operand asks for a 72x72
// multiplier where 32x32 was the honest need. The first draft of this block
// carried 67- and 75-bit lanes on the same "slack is free" reasoning. Proven
// bounds, from the s32 input widths and nothing else:
//
//   |m*x|                    <= 2^62               (both operands -2^31)
//   |pa| = |3 products + (m3 << 16)|
//                            <= 3*2^62 + 2^47
//                             = 1.3835e19 < 2^64   -> signed 65 bits
//   |pa - pb|                <= 2.767e19  < 2^65   -> signed 66 bits
//   |w0*(pa - pb)|, w0 <= 63 <= 1.743e21  < 2^71   -> signed 72 bits
//   |(pb << 6) + w0*(pa-pb)| <= 2.629e21  < 2^72   -> signed 73 bits
//
// The round-half-up addend (2^21) cannot disturb the last of those. Two bits
// come off the accumulator and two off the blend lane. **These are adders, not
// multipliers, so the saving is ALMs** — but §5's rule is *prove the width,
// then synthesise*, and the unproven width is what §5 punished.
//
// The multiplier operands stay a full signed 32x32. The DSP audit's suggestion
// that a bone matrix's 3x3 is a bounded rotation is true of the CONTENT and
// false of the CONTRACT: the oracle accepts any s32, so narrowing the
// multiplier would be a behavioural change wearing an optimisation's clothes.
//
// ---------------------------------------------------------------------------
// THE WEIGHT IDENTITY, and the domain it is only true on
// ---------------------------------------------------------------------------
//     w0*pa + (64 - w0)*pb  ==  (pb << 6) + w0*(pa - pb)
//
// Exact over the legal domain, and legal here because `blend` is exact and
// unrounded and `rescale_sat` is applied ONCE to the finished sum — which is
// the single-rounding law above, not a relaxation of it.
//
// **It is FALSE outside the legal domain**, and that is why the guard below is
// not decoration. `w0 > 64` is out of contract; the old form's
// `w1 = 7'd64 - v_w0_i` wrapped to `192 - w0` there, and this form reads
// `64 - w0` as negative. Neither is right, because there is no right answer
// for an input the contract excludes.
//
// ENFORCED-BY: tests/geometry/geom_skin_directed.cpp — `require_legal_w0()`
// aborts the differential if any driver ever presents `w0 > 64`, so every
// comparison this project makes against the oracle is a statement about the
// legal domain and is known to be one. The upstream HARDWARE obligation
// belongs to GEOM.VDECODE, which has no RTL yet; that is recorded as an
// owner-docket item in design/contracts/GEOM.SKIN.md and is NOT papered over
// here with a comment asserting what nothing checks.
//
// `w0*(pa - pb)` is written as an explicit SHIFT-ADD, six terms in a
// three-level tree. QUARTUS_GOTCHAS §3: `(* multstyle = "logic" *)` is
// silently ignored by Quartus 17.0.2 — no warning, no error, only a DSP count
// that will not fall — so a narrow-operand multiply has to be written as what
// it is rather than annotated into submission.
module zhao_geom_skin #(
    // 1, 3 or 6. See the frontier table above. 3 is the intended setting.
    parameter int MUL_LANES = 3
) (
    input  logic clk,
    input  logic rst_n,

    // ---- vertex in, ready/valid -------------------------------------------
    input  logic               v_valid_i,
    output logic               v_ready_o,
    input  logic signed [31:0] v_x_i,
    input  logic signed [31:0] v_y_i,
    input  logic signed [31:0] v_z_i,
    input  logic        [ 6:0] v_w0_i,     // 1/64 quanta; 64 == rigid
    input  logic               v_rigid_i,  // b1 == b0, decided upstream
    input  logic        [15:0] v_src_id_i,

    // ---- the two bone matrices for this vertex, row-major fx16 ------------
    // Presented with the vertex and LATCHED on accept. The palette itself is
    // GEOM.POSE's to own; this block holds no cache and never addresses memory.
    // The latch is what a multi-cycle engine costs: a ready/valid producer is
    // free to change its data the cycle after `v_ready_o` goes high, and the
    // engine reads these for up to eighteen cycles after that.
    input  logic signed [31:0] a_m_i [12],
    input  logic signed [31:0] b_m_i [12],

    // ---- skinned vertex out, ready/valid ----------------------------------
    output logic               o_valid_o,
    input  logic               o_ready_i,
    output logic signed [31:0] o_x_o,
    output logic signed [31:0] o_y_o,
    output logic signed [31:0] o_z_o,
    output logic        [15:0] o_src_id_o,

    output logic [31:0] vertices_transformed_o
);

  // ---- the shape of the farm, derived from the one parameter --------------
  localparam int TL = (MUL_LANES >= 3) ? 3 : 1;  // term lanes:  x/y/z at once
  localparam int RL = MUL_LANES / TL;            // row-product lanes
  localparam int TSTEPS = 3 / TL;                // cycles to cover three terms
  localparam int NG_BLEND = (6 + RL - 1) / RL;   // row-product groups, blend
  localparam int NG_RIGID = (3 + RL - 1) / RL;   // row-product groups, rigid

  // Proven widths, from the header. Named so a later edit has to argue with
  // the proof rather than with a magic number.
  localparam int ACCW = 65;    // |pa| < 2^64
  localparam int DIFFW = 66;   // |pa - pb| < 2^65
  localparam int BLENDW = 73;  // |(pb << 6) + w0*(pa - pb)| < 2^72

  localparam logic signed [BLENDW-1:0] ZERO_B = '0;

  // An illegal lane count must stop elaboration, not degrade quietly. An
  // unresolved module reference inside a generate-if is the portable static
  // assertion: when the condition is false nothing is elaborated and no tool
  // looks for the module, and when it is true every front end this project
  // uses refuses to build. `$error` was not used because Quartus 17.0.2's
  // support for elaboration system tasks is not something to discover during
  // a twenty-minute fit.
  //
  // THE `generate` KEYWORDS ARE NOT OPTIONAL. Verilator, slang and the LRM all
  // accept a bare module-scope `if`; Quartus 17.0.2 does not, and reports it as
  // a SYNTAX error pointing at the `if`:
  //
  //   Error (10170): Verilog HDL syntax error at zhao_geom_skin.sv(223) near
  //                  text: "if";  expecting "endmodule"
  //
  // MEASURED 2026-08-23: the first fit of this block died in analysis and
  // synthesis at 44 s for exactly this, having linted clean at all three
  // MUL_LANES settings first. See QUARTUS_GOTCHAS §8.
  generate
    if (!(MUL_LANES == 1 || MUL_LANES == 3 || MUL_LANES == 6)) begin : g_illegal
      ZHAO_GEOM_SKIN_MUL_LANES_MUST_BE_1_3_OR_6 u_static_assert ();
    end
  endgenerate

  // ---- round-half-up shift then saturate, qformats §3/§4 ------------------
  // Round-half-up on a NEGATIVE value is the trap: adding the half and shifting
  // arithmetically gives round-half-up (toward +inf), which is what
  // rescale_s32 does. A shift alone would floor, and the two disagree at every
  // exact half.
  function automatic logic signed [31:0] rescale_sat(input logic signed [BLENDW-1:0] v,
                                                     input int unsigned sh);
    logic signed [BLENDW-1:0] r;
    begin
      r = (v + (73'sd1 <<< (sh - 1))) >>> sh;
      if (r > 73'sd2147483647) rescale_sat = 32'sh7FFF_FFFF;
      else if (r < -73'sd2147483648) rescale_sat = 32'sh8000_0000;
      else rescale_sat = r[31:0];
    end
  endfunction

  // ---- latched vertex and palette ----------------------------------------
  // m_q[0..11] is A, m_q[12..23] is B, so row-product `rp` in 0..5 reads its
  // three matrix elements at m_q[rp*4 + term] and its translation at
  // m_q[rp*4 + 3]. A's rows land at 0,4,8 and B's at 12,16,20, which is
  // exactly 4*rp for rp = 0..5 — one flat index, and no matrix select anywhere
  // in the operand path.
  logic signed [31:0] m_q [24];
  logic signed [31:0] vx_q, vy_q, vz_q;
  logic        [ 5:0] w0_q;    // six bits: the blend path is entered only for
                               // w0 <= 63, and 63 is six bits. ENFORCED-BY above.
  logic               rigid_q;
  logic        [15:0] src_q;

  // ---- the accumulators, one per row-product ------------------------------
  logic signed [ACCW-1:0] acc      [6];
  logic                   acc_done [6];

  // ---- issue walk ---------------------------------------------------------
  logic       busy;      // an engine pass is in flight
  logic       issuing;   // slots remain to be issued
  logic [2:0] rp_grp;    // row-product group, 0 .. n_groups-1
  logic [1:0] t_grp;     // term group, 0 .. TSTEPS-1
  logic [1:0] br;        // blend walk row, 0..2; 3 == finished

  logic [2:0] n_groups;
  logic [2:0] n_rp;
  assign n_groups = rigid_q ? 3'(NG_RIGID) : 3'(NG_BLEND);
  assign n_rp     = rigid_q ? 3'd3 : 3'd6;

  // ---- the multiplier farm — LOCAL to this block --------------------------
  // Shared WITHIN the subsystem only. A console-global multiplier farm was
  // explicitly rejected: smallest local farm per subsystem, sharing only what
  // is mutually exclusive inside it. Nothing outside GEOM.SKIN can reach these,
  // and nothing inside it can issue two products to one lane in one cycle.
  //
  // Input-registered and output-registered, so the DSP block's own pipeline
  // registers are the ones inferred rather than logic in front of and behind a
  // combinational array. Issue at cycle N, product readable at N+2.
  logic signed [31:0] mul_a [MUL_LANES];
  logic signed [31:0] mul_b [MUL_LANES];
  logic signed [31:0] a_q   [MUL_LANES];
  logic signed [31:0] b_q   [MUL_LANES];
  logic signed [63:0] p_q   [MUL_LANES];

  // Operand select. `mul_b` is pure wiring whenever TL == 3: lane 0 is always
  // x, lane 1 always y, lane 2 always z.
  always_comb begin
    for (int l = 0; l < MUL_LANES; l++) begin
      int rl, tl, rp, trm;
      logic [4:0] idx;  // 0..22, five bits exactly; see the saturate below
      rl  = l / TL;
      tl  = l % TL;
      rp  = int'(rp_grp) * RL + rl;
      // rp_grp never exceeds n_groups-1, so rp is always <= 5 and this saturate
      // is dead logic. It is here so the array index is provably in range for
      // every front end rather than provably in range only in this comment.
      if (rp > 5) rp = 5;
      trm = int'(t_grp) * TL + tl;
      idx = 5'(rp * 4 + trm);
      mul_a[l] = m_q[idx];
      mul_b[l] = (trm == 0) ? vx_q : ((trm == 1) ? vy_q : vz_q);
    end
  end

  // ---- destination pipeline, two deep to match the lane latency -----------
  // A product issued at cycle N is readable at N+2, so the row-product it
  // belongs to — and whether it is that row-product's LAST term — must travel
  // with it rather than be re-derived from counters that have moved on.
  logic [2:0] rp_issue   [RL];
  logic       dv_issue   [RL];
  logic       dlast_issue;
  logic [2:0] dst_d1     [RL], dst_d2   [RL];
  logic       dv_d1      [RL], dv_d2    [RL];
  logic       dlast_d1   [RL], dlast_d2 [RL];
  logic signed [ACCW-1:0] lane_sum [RL];

  always_comb begin
    dlast_issue = (t_grp == 2'(TSTEPS - 1));
    for (int r = 0; r < RL; r++) begin
      rp_issue[r] = 3'(int'(rp_grp) * RL + r);
      dv_issue[r] = issuing && (rp_issue[r] < n_rp);
      lane_sum[r] = '0;
      for (int t = 0; t < TL; t++) lane_sum[r] = lane_sum[r] + ACCW'(p_q[r*TL + t]);
    end
  end

  // ---- the blend, one shared unit walked across the three rows ------------
  // One unit, not three. The walk costs no extra cycles: `pa[r]` and `pb[r]`
  // become final one cycle apart in the natural issue order, which is exactly
  // the cadence a one-row-per-cycle walk consumes them at.
  logic [2:0] br_a, br_b;
  logic signed [ACCW-1:0]   pa_sel, pb_sel;
  logic signed [DIFFW-1:0]  pdiff;
  logic signed [BLENDW-1:0] pd_ext, wp0, wp1, wp2, wprod, blend_v;
  logic signed [31:0]       res_row;
  logic                     row_ready;

  // br == 3 means "finished"; clamping keeps `br_b` inside acc[6] in that
  // cycle, where nothing reads the result anyway.
  assign br_a = (br == 2'd3) ? 3'd0 : {1'b0, br};
  assign br_b = br_a + 3'd3;

  assign pa_sel = acc[br_a];
  assign pb_sel = acc[br_b];
  assign pdiff  = DIFFW'(pa_sel) - DIFFW'(pb_sel);
  assign pd_ext = BLENDW'(pdiff);

  // w0 * pdiff as a six-term shift-add in a three-level tree. Not a `*`: see
  // QUARTUS_GOTCHAS §3. The zero arms are SIGNED literals — an unsigned arm
  // would make the whole conditional expression unsigned and silently turn a
  // negative partial product into a huge positive one.
  always_comb begin
    wp0 = (w0_q[0] ? pd_ext : ZERO_B) + (w0_q[1] ? (pd_ext <<< 1) : ZERO_B);
    wp1 = (w0_q[2] ? (pd_ext <<< 2) : ZERO_B) + (w0_q[3] ? (pd_ext <<< 3) : ZERO_B);
    wp2 = (w0_q[4] ? (pd_ext <<< 4) : ZERO_B) + (w0_q[5] ? (pd_ext <<< 5) : ZERO_B);
    wprod = (wp0 + wp1) + wp2;
  end

  assign blend_v = (BLENDW'(pb_sel) <<< 6) + wprod;

  // The branch the reference takes, for the reason it takes it: the rigid path
  // carries no weight scale, so it rescales by 16 and never forms the blend.
  assign res_row = rigid_q ? rescale_sat(BLENDW'(pa_sel), 16) : rescale_sat(blend_v, 22);

  // A row may be blended as soon as ITS OWN operands are final — not when the
  // whole engine is finished. That is what lets the walk overlap the issue
  // tail instead of queueing behind it.
  assign row_ready = busy && (br != 2'd3) && acc_done[br_a] && (rigid_q || acc_done[br_b]);

  // ---- handshake ----------------------------------------------------------
  // A single-entry skid in front of a multi-cycle engine: a vertex is accepted
  // only when the engine is idle AND the output register is free or being
  // emptied this cycle. Both in the same cycle is a legal back-to-back beat.
  logic take;
  assign v_ready_o = !busy && (!o_valid_o || o_ready_i);
  assign take = v_valid_i && v_ready_o;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      o_valid_o <= 1'b0;
      o_x_o <= '0; o_y_o <= '0; o_z_o <= '0; o_src_id_o <= '0;
      vertices_transformed_o <= '0;
      busy <= 1'b0;
      issuing <= 1'b0;
      rp_grp <= '0;
      t_grp <= '0;
      br <= '0;
      vx_q <= '0; vy_q <= '0; vz_q <= '0;
      w0_q <= '0;
      rigid_q <= 1'b0;
      src_q <= '0;
      for (int i = 0; i < 24; i++) m_q[i] <= '0;
      for (int i = 0; i < 6; i++) begin
        acc[i] <= '0;
        acc_done[i] <= 1'b0;
      end
      for (int l = 0; l < MUL_LANES; l++) begin
        a_q[l] <= '0;
        b_q[l] <= '0;
        p_q[l] <= '0;
      end
      for (int r = 0; r < RL; r++) begin
        dst_d1[r] <= '0; dst_d2[r] <= '0;
        dv_d1[r] <= 1'b0; dv_d2[r] <= 1'b0;
        dlast_d1[r] <= 1'b0; dlast_d2[r] <= 1'b0;
      end
    end else begin
      if (o_valid_o && o_ready_i) o_valid_o <= 1'b0;

      // ---- the multiplier lanes, always clocked ---------------------------
      // The operand registers HOLD between issues, so the array is not
      // re-driven by whatever the idle mux happens to present.
      for (int l = 0; l < MUL_LANES; l++) begin
        if (issuing) begin
          a_q[l] <= mul_a[l];
          b_q[l] <= mul_b[l];
        end
        // Both operands are sized to the product width BEFORE the multiply, so
        // the 64-bit result is the mathematical one and not a 32-bit
        // self-determined product widened afterwards. The casts preserve
        // signedness, which is what makes this a signed 32x32.
        p_q[l] <= 64'(a_q[l]) * 64'(b_q[l]);
      end

      // ---- destination pipeline -------------------------------------------
      for (int r = 0; r < RL; r++) begin
        dst_d1[r]   <= rp_issue[r];
        dv_d1[r]    <= dv_issue[r];
        dlast_d1[r] <= dlast_issue;
        dst_d2[r]   <= dst_d1[r];
        dv_d2[r]    <= dv_d1[r];
        dlast_d2[r] <= dlast_d1[r];
      end

      // ---- accumulate the landed products ---------------------------------
      // Two row-lanes never target the same accumulator: rp = rp_grp*RL + rl
      // is injective in rl, so each acc has exactly one writer per cycle.
      for (int r = 0; r < RL; r++) begin
        if (dv_d2[r]) begin
          acc[dst_d2[r]] <= acc[dst_d2[r]] + lane_sum[r];
          if (dlast_d2[r]) acc_done[dst_d2[r]] <= 1'b1;
        end
      end

      // ---- advance the issue walk -----------------------------------------
      if (issuing) begin
        if (dlast_issue) begin
          t_grp <= '0;
          if (rp_grp == n_groups - 3'd1) issuing <= 1'b0;
          else rp_grp <= rp_grp + 3'd1;
        end else begin
          t_grp <= t_grp + 2'd1;
        end
      end

      // ---- the blend walk -------------------------------------------------
      if (row_ready) begin
        case (br)
          2'd0: o_x_o <= res_row;
          2'd1: o_y_o <= res_row;
          default: o_z_o <= res_row;
        endcase
        if (br == 2'd2) begin
          // Last row: the vertex is finished this cycle. `busy` drops here, so
          // the next vertex is accepted on the FOLLOWING cycle -- which is why
          // the latency table's number is also the issue interval.
          br <= 2'd3;
          busy <= 1'b0;
          o_src_id_o <= src_q;
          o_valid_o <= 1'b1;
        end else begin
          br <= br + 2'd1;
        end
      end

      // ---- accept ---------------------------------------------------------
      // `v_ready_o` requires !busy, and `busy` is still high in the completion
      // cycle, so an accept and a completion can never land on the same clock
      // and these assignments never race the ones above.
      if (take) begin
        for (int i = 0; i < 12; i++) begin
          m_q[i]      <= a_m_i[i];
          m_q[12 + i] <= b_m_i[i];
        end
        vx_q <= v_x_i;
        vy_q <= v_y_i;
        vz_q <= v_z_i;
        w0_q <= v_w0_i[5:0];
        // `w0 == 64` takes the rigid path even when b1 != b0. That is the
        // reference's own branch, not an optimisation of it.
        rigid_q <= v_rigid_i || (v_w0_i == 7'd64);
        src_q <= v_src_id_i;

        // The translation SEEDS each accumulator: pa = (m3 << 16) + three
        // products, so the shifted translation is the accumulator's initial
        // value and costs no adder of its own.
        for (int rp = 0; rp < 3; rp++) begin
          acc[rp]     <= ACCW'(a_m_i[rp*4 + 3]) <<< 16;
          acc[rp + 3] <= ACCW'(b_m_i[rp*4 + 3]) <<< 16;
        end
        for (int i = 0; i < 6; i++) acc_done[i] <= 1'b0;

        busy <= 1'b1;
        issuing <= 1'b1;
        rp_grp <= '0;
        t_grp <= '0;
        br <= '0;

        // `vertices_transformed` is the shared catalog counter GEOM.LOOM and
        // GEOM.WARP also carry, so this stage reports under the same name
        // rather than inventing a skinning-specific one. It counts vertices
        // ACCEPTED, not offered: a vertex held off by backpressure is not work
        // done.
        if (vertices_transformed_o != 32'hFFFF_FFFF)
          vertices_transformed_o <= vertices_transformed_o + 32'd1;
      end
    end
  end

endmodule : zhao_geom_skin
