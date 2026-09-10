// zhao_geom_mat3x4_mul_mutant.sv -- A DELIBERATELY BROKEN COPY. NOT SHIPPED.
//
// This exists to make the R4 single-lane walk's NEW machinery -- the
// issue/commit accumulator that assembles each element from three registered
// products -- a demonstrated instrument rather than an argument. The
// accumulator's characteristic fault is an element BOUNDARY error: every
// product is individually correct, every handshake completes, the counter
// advances exactly once per matrix, the walk is exactly the declared 37
// cycles -- and every element after the first silently carries its
// predecessors' sum. Exactly the class of corruption the pre-R4 per-element
// arm could never exhibit, because it never held state between elements.
//
// The one substantive change, in g_seq's commit arm:
//
//     if (pn_q == 2'd0) begin
//       acc <= 67'(m_p_q);          // a fresh element starts a fresh sum
//  ->   acc <= acc + 67'(m_p_q);    // MUTANT: the sum is never cleared
//
// INVERTED POLARITY: driven by tests/geometry/geom_mat3x4_mul_mutant_control.cpp
// against the same zref oracle the directed suite uses; the control PASSES
// when the differential comparison FAILS from element 1 onward. Evidence
// about the instrument, not about the design.
//
// The module is RENAMED so a source-list mistake can never elaborate it in
// place of the real one, and it lives under tests/ where
// check_forbidden_sources.py will not find it in a production closure.
//
// REGENERATE IT if zhao_geom_mat3x4_mul.sv changes shape: this is a copy, and
// a copy of an old version is a positive control for a block that no longer
// exists.

// zhao_geom_mat3x4_mul.sv — 3x4 affine matrix product, sequenced.
//
// A submodule of GEOM.POSE (design/contracts/GEOM.POSE.md), not a ledger block
// of its own. It is the step the per-bone decode chain applies twice:
//
//     A_b = (b == 0) ? LR : A_parent * LR      <-- THIS BLOCK
//     S_b = A_b * inv_rest[b]                  <-- and again here
//
// Reference: `zref::creature::mat3x4_mul`
// (reference/src/zcreature/creature_core.cpp:74).
//
// ---------------------------------------------------------------------------
// WHY THIS ONE IS SEQUENTIAL, AND HOW SEQUENTIAL — MUL_LANES
// ---------------------------------------------------------------------------
// The obvious implementation is combinational, and it would be thirty-six 32x32
// products — twice GEOM.SKIN's eighteen, on a device with 112 DSPs that the
// project already over-subscribes. The first cut of this block therefore
// computed ONE OUTPUT ELEMENT PER CYCLE: three 32x32 products, twelve cycles
// per matrix — 9 DSP by the measured calibration (a 32x32 product is 3 DSP;
// tools/budget/calibration.json, and the 28..33-bit cliff note at
// zhao_project_core.sv's cost section).
//
// OWNER RULING R4 (2026-09-09, reports/OWNER-RULINGS-20260909-2300.md §R4):
// "relax the 1 bone/clock rule. It must have been arbitrary." That ruling
// removes the only reason to hold three products in flight, so:
//
// MUL_LANES = 1 (DEFAULT): ONE 32x32 multiplier walked across all thirty-six
//   products by an operand-mux sequencer (the zhao_terrain_normals.sv:203
//   pattern), product registered, accumulated one product per cycle.
//   3 DSP. Latency: valid rises 37 cycles after accept (36 issue cycles, one
//   trailing commit).
// MUL_LANES = 3: the previous arrangement — one element per cycle, three
//   products at once. 9 DSP. Latency: valid rises 12 cycles after accept.
//
// The two arms are BIT-IDENTICAL: the three products of an element are exact
// s64 integers whichever cycle each is computed in, their s67 sum is the same
// sum whether formed in one cycle or accumulated across three, and the single
// rescale is shared. Sequencing moves cycles, never bits.
//
// The operand mux is a 12:1 of 32-bit words per side. zhao_project_core's
// ROWS_PER_PASS chose a shifting hold bank over exactly this shape to keep the
// multiplier's input cone shallow; if the pose-decode leaf fit shows this cone
// gating Fmax, that is the recorded fallback — a structural change, not a
// law change.
//
// Twelve — now thirty-seven — cycles per multiply is affordable precisely
// BECAUSE of what this block feeds. `spec/creature_rules.md` §2.2 rejected
// baking every pose at load (x6 memory); the decode is a cache MISS cost, not
// a per-frame-per-instance cost, shared by every instance of that creature
// type on that frame.
//
// ---------------------------------------------------------------------------
// THE LAW
// ---------------------------------------------------------------------------
// For output row i, column j:
//
//     j < 3:  out[i][j] = rescale(a[i][0]*b[0][j] + a[i][1]*b[1][j]
//                                 + a[i][2]*b[2][j], 16)
//     j == 3: out[i][3] = rescale(a[i][0]*b[0][3] + a[i][1]*b[1][3]
//                                 + a[i][2]*b[2][3] + (a[i][3] << 16), 16)
//
// One rescale per element, round-half-up then saturate (qformats §3/§4). The
// two cases are the SAME three products against a different column of `b`, plus
// a translation term that is zero for j < 3 — so one datapath serves both and
// there is no second multiplier set for the translation column.
//
// WIDTHS: a product is s64; three of them is s66; plus the s48 translation is
// s67. Round-half-up cannot overflow that. `>>> 16` leaves s51, which the
// saturating narrow takes to s32. The MUL_LANES=1 accumulator holds at most
// two products (s65) before the third joins combinationally at the rescale,
// so s67 covers every partial as well as the total.
//
// Unlike the quaternion block, saturation here CAN fire: `a` and `b` are
// arbitrary fx16 affines, and a large translation chained through a large
// rotation genuinely exceeds s32. The reference saturates too (rescale_s32),
// so the two agree — but this is a real rail, not a formality, and the directed
// test drives it.
module zhao_geom_mat3x4_mul_mutant #(
    // 1 = one shared multiplier lane (default, ruling R4); 3 = one element
    // per cycle, the previous arrangement.
    parameter int MUL_LANES = 1
) (
    input  logic clk,
    input  logic rst_n,

    // ---- operands in, ready/valid -----------------------------------------
    input  logic               in_valid_i,
    output logic               in_ready_o,
    input  logic signed [31:0] a_m_i [12],
    input  logic signed [31:0] b_m_i [12],
    input  logic        [ 7:0] in_tag_i,

    // ---- product out, ready/valid -----------------------------------------
    output logic               out_valid_o,
    input  logic               out_ready_i,
    output logic signed [31:0] out_m_o [12],
    output logic        [ 7:0] out_tag_o,

    output logic [31:0] products_done_o
);

  // Quartus 17 needs elaboration checks inside `initial begin ... end` (a
  // bare module-scope `if` is a synthesis syntax error there), and
  // `--lint-only` does not run this block — only elaboration does.
  initial begin
    if (MUL_LANES != 1 && MUL_LANES != 3) begin
      $fatal(1, "zhao_geom_mat3x4_mul_mutant: MUL_LANES must be 1 or 3, got %0d", MUL_LANES);
    end
  end

  // ---- round-half-up then saturate, qformats §3/§4 ------------------------
  function automatic logic signed [31:0] rescale_sat16(input logic signed [66:0] v);
    logic signed [66:0] r;
    begin
      r = (v + (67'sd1 <<< 15)) >>> 16;
      if (r > 67'sd2147483647) rescale_sat16 = 32'sh7FFF_FFFF;
      else if (r < -67'sd2147483648) rescale_sat16 = 32'sh8000_0000;
      else rescale_sat16 = r[31:0];
    end
  endfunction

  generate
    if (MUL_LANES == 3) begin : g_elem

      // ---- held operands and the element cursor ---------------------------
      logic signed [31:0] a_q [12];
      logic signed [31:0] b_q [12];
      logic        [ 7:0] tag_q;
      logic        [ 3:0] idx;      // 0..11, the element being produced
      logic               busy;

      // Row and column of the element under the cursor. `idx` walks row-major,
      // the same order the reference's nested loops produce, so a partial
      // result is always a prefix of the reference's own.
      logic [1:0] row, col;
      assign row = idx[3:2];
      assign col = idx[1:0];

      // ---- the three products, plus the translation term ------------------
      // The translation term is zero unless this is the fourth column; that is
      // the whole difference between the two cases in the law above.
      logic signed [66:0] acc;
      always_comb begin
        acc = 67'(a_q[{row, 2'd0}]) * 67'(b_q[{2'd0, col}])
            + 67'(a_q[{row, 2'd1}]) * 67'(b_q[{2'd1, col}])
            + 67'(a_q[{row, 2'd2}]) * 67'(b_q[{2'd2, col}]);
        if (col == 2'd3) acc = acc + (67'(a_q[{row, 2'd3}]) <<< 16);
      end

      logic accept;
      assign in_ready_o = !busy && (!out_valid_o || out_ready_i);
      assign accept = in_valid_i && in_ready_o;

      always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
          busy <= 1'b0;
          idx <= '0;
          tag_q <= '0;
          out_valid_o <= 1'b0;
          out_tag_o <= '0;
          products_done_o <= '0;
          for (int i = 0; i < 12; i++) begin
            a_q[i] <= '0;
            b_q[i] <= '0;
            out_m_o[i] <= '0;
          end
        end else begin
          if (out_valid_o && out_ready_i) out_valid_o <= 1'b0;

          if (accept) begin
            for (int i = 0; i < 12; i++) begin
              a_q[i] <= a_m_i[i];
              b_q[i] <= b_m_i[i];
            end
            tag_q <= in_tag_i;
            idx <= '0;
            busy <= 1'b1;
            out_valid_o <= 1'b0;
          end else if (busy) begin
            out_m_o[idx] <= rescale_sat16(acc);
            if (idx == 4'd11) begin
              busy <= 1'b0;
              out_valid_o <= 1'b1;
              out_tag_o <= tag_q;
              // Counts COMPLETED products, not accepted operands: a multiply
              // still walking its twelve elements is not a product this block
              // has delivered.
              if (products_done_o != 32'hFFFF_FFFF) products_done_o <= products_done_o + 32'd1;
            end else begin
              idx <= idx + 4'd1;
            end
          end
        end
      end

    end else begin : g_seq

      // ---- one lane: issue one product per cycle, commit one behind --------
      logic signed [31:0] a_q [12];
      logic signed [31:0] b_q [12];
      logic        [ 7:0] tag_q;
      logic               busy;

      // Issue cursor: element `en` (0..11, row-major — the reference's own
      // order), product step `pn` (0..2) within the element.
      logic [3:0] en;
      logic [1:0] pn;
      logic       issuing;

      // The pipeline register between issue and commit: the product computed
      // last cycle, and which (element, step) it belongs to. Registering the
      // product keeps the multiplier out of the accumulator's timing path,
      // exactly as zhao_terrain_normals does.
      logic signed [63:0] m_p_q;
      logic               pv_q;
      logic        [3:0]  en_q;
      logic        [1:0]  pn_q;

      logic signed [66:0] acc;

      logic [1:0] row, col;
      assign row = en[3:2];
      assign col = en[1:0];

      // THE ONE nonconstant multiply in this arm. 32x32 -> s64, exact.
      wire signed [31:0] m_a = a_q[{row, pn}];
      wire signed [31:0] m_b = b_q[{pn, col}];
      wire signed [63:0] m_p = 64'(m_a) * 64'(m_b);

      // Commit-side coordinates and the finishing sum: the accumulator holds
      // the first two products, the third joins here, and the translation
      // term lands only on column 3 — the same law text as the other arm.
      logic [1:0] row_q, col_q;
      assign row_q = en_q[3:2];
      assign col_q = en_q[1:0];
      logic signed [66:0] fin;
      always_comb begin
        fin = acc + 67'(m_p_q);
        if (col_q == 2'd3) fin = fin + (67'(a_q[{row_q, 2'd3}]) <<< 16);
      end

      logic accept;
      assign in_ready_o = !busy && (!out_valid_o || out_ready_i);
      assign accept = in_valid_i && in_ready_o;

      always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
          busy <= 1'b0;
          issuing <= 1'b0;
          en <= '0;
          pn <= '0;
          m_p_q <= '0;
          pv_q <= 1'b0;
          en_q <= '0;
          pn_q <= '0;
          acc <= '0;
          tag_q <= '0;
          out_valid_o <= 1'b0;
          out_tag_o <= '0;
          products_done_o <= '0;
          for (int i = 0; i < 12; i++) begin
            a_q[i] <= '0;
            b_q[i] <= '0;
            out_m_o[i] <= '0;
          end
        end else begin
          if (out_valid_o && out_ready_i) out_valid_o <= 1'b0;

          if (accept) begin
            for (int i = 0; i < 12; i++) begin
              a_q[i] <= a_m_i[i];
              b_q[i] <= b_m_i[i];
            end
            tag_q <= in_tag_i;
            en <= '0;
            pn <= '0;
            issuing <= 1'b1;
            pv_q <= 1'b0;
            busy <= 1'b1;
            out_valid_o <= 1'b0;
          end else if (busy) begin
            // ---- issue: one product per cycle -----------------------------
            if (issuing) begin
              m_p_q <= m_p;
              pv_q <= 1'b1;
              en_q <= en;
              pn_q <= pn;
              if (pn == 2'd2) begin
                pn <= 2'd0;
                if (en == 4'd11) issuing <= 1'b0;
                else en <= en + 4'd1;
              end else begin
                pn <= pn + 2'd1;
              end
            end else begin
              pv_q <= 1'b0;
            end

            // ---- commit: accumulate, or finish an element -----------------
            if (pv_q) begin
              if (pn_q == 2'd0) begin
                // MUTANT: the one substantive change -- the accumulator is
                // never cleared at an element boundary.
                acc <= acc + 67'(m_p_q);
              end else if (pn_q == 2'd1) begin
                acc <= acc + 67'(m_p_q);
              end else begin
                out_m_o[en_q] <= rescale_sat16(fin);
                if (en_q == 4'd11) begin
                  busy <= 1'b0;
                  out_valid_o <= 1'b1;
                  out_tag_o <= tag_q;
                  // Counts COMPLETED products, not accepted operands — the
                  // same law as the other arm, at the same edge.
                  if (products_done_o != 32'hFFFF_FFFF) products_done_o <= products_done_o + 32'd1;
                end
              end
            end
          end
        end
      end

    end
  endgenerate

endmodule : zhao_geom_mat3x4_mul_mutant
