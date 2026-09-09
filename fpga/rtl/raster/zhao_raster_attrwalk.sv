// zhao_raster_attrwalk.sv — RASTER.ATTRWALK: one attribute across a tile by
// ADDS ONLY. No divider inside, no sign branches, no reseeds — ever.
//
// ENFORCED-BY: tests/proofs/attrwalk_rtl_differential.cpp:main
//
// ---------------------------------------------------------------------------
// WHAT THIS REPLACES
// ---------------------------------------------------------------------------
// zhao_raster_attrstep walks the same tile but tracks the MAGNITUDE quotient
// per sign branch, so it must:
//   * carry two step decompositions (qxp/rxp AND qxn/rxn) and mux per pixel;
//   * reseed through its private divider at EVERY covered row (36 clocks at
//     radix 2), and AGAIN whenever the numerator crosses zero mid-row;
//   * step a 96-bit shadow accumulator purely to know the sign of the NEXT
//     pixel.
// All of that machinery exists to serve a representation that breaks at zero.
//
// This block uses the representation that does not break: for area A > 0,
//
//     N = q*A + r,   0 <= r < A,   q = floor(N/A)   (negative N included)
//
// The floor quotient is continuous across zero, so ONE step decomposition
//     dN = dq*A + dr,  0 <= dr < A
// advances every pixel with
//     q += dq;  r += dr;  if (r >= A) { r -= A; q += 1; }
// exactly, for any signs of N and dN. A row starts by COPYING the row-base
// pair — never by dividing. The divides all live upstream, in the seed stage,
// THREE per plane (base, x step, y step) instead of one per covered row.
//
// ---------------------------------------------------------------------------
// THE ROUNDING LAW, AND THE ONE PLACE THE REPO DISAGREES WITH ITSELF
// ---------------------------------------------------------------------------
// The emitted value applies the rounding at the OUTPUT, on the exact pair:
//
//     rounded = q + (2r > A) + (2r == A && tie_up)
//
// Two tie laws exist in this repository and they are NOT the same law:
//
//   * TIE_TOWARD_POSITIVE = 0 (default): ties away from zero — tie_up is
//     (q >= 0). This is RASTER.ATTRDIV's shipped law, the one every RTL
//     directed test enforces. Proven equivalent to the verilated attrdiv on
//     every driven case, ties of both signs included, by the enforcing test.
//   * TIE_TOWARD_POSITIVE = 1: ties toward +infinity — tie_up is constant 1.
//     This is what reference/src/zrender/rast.cpp:div_rhu_s128 ACTUALLY
//     computes (floor((n + floor(d/2))/d), unchanged since the file was
//     created) and what spec/qformats.md §1 says in words: "ties round toward
//     +infinity". attrdiv's header claims rast.cpp rounds away from zero; the
//     claim is false, and every document repeating it restated the oracle
//     instead of calling it. The two laws differ by exactly +1 on NEGATIVE
//     EXACT HALVES and nowhere else (measured, not argued — see the enforcing
//     test's zref sections).
//
// The parameter exists so the OWNER ratifies the tie, not this file. The
// default matches the RTL boundary that today's tests enforce. Whichever way
// the ruling lands, the walker is one constant away — the architecture does
// not care, and note that the zref law is the CHEAPER one (the tie term loses
// its sign input entirely).
//
// ---------------------------------------------------------------------------
// WIDTHS, RANGE, AND THE WRAP HOLE THIS BLOCK DOES NOT COPY
// ---------------------------------------------------------------------------
// The walk pair is carried at full plane width (QW = 96), so the walk itself
// cannot overflow: q*A + r == N is an invariant, bit for bit, covered pixel or
// not. Range is enforced at EMIT: a covered pixel whose rounded quotient does
// not fit 32 signed bits raises q_error_o for that pixel and counts in
// range_errs_o. attrdiv's own overflow check (`|q_r[32]` on a 32-bit signed
// output) misses magnitudes in [2^31, 2^32) and emits them SIGN-FLIPPED with
// no flag — measured 2026-09-09 by the enforcing test. The full-width check
// here is the repair for this path: the check reads every bit above 30 and
// demands they agree with the sign.
//
// Uncovered columns are walked (16 clocks a row, like RASTER.INTERP) but their
// values are neither emitted nor range-checked, so a tile corner far outside
// the triangle cannot poison a row that contains real coverage — unlike the
// attrstep seed, which divides at column 0 covered or not and errs the whole
// job on overflow.
//
// ---------------------------------------------------------------------------
// WHAT ARRIVES PRE-COMPUTED, AND WHO OWES IT
// ---------------------------------------------------------------------------
// The job carries three Euclidean pairs, all against the SAME area A:
//   (q0,  r0)  — the plane at THIS TILE's first pixel centre. The pixel-centre
//                law (N0 + dNdx*tx + dNdy*ty + dNdx/2 + dNdy/2, halves exact
//                because ATTRSETUP's gradients are multiples of 256) is the
//                seed stage's to apply, exactly where RASTER.INTERP applies
//                it. This block never sees a numerator.
//   (dqx, drx) — one pixel of x step.
//   (dqy, dry) — one ROW of y step.
// The seed stage owns the three divides — or none: pairs compose exactly
// (pair_add / pair_double), so a per-triangle anchor decomposition reaches
// every tile of that triangle by shift-adds alone. 2,000/2,000 random
// anchor→tile reconstructions agree with direct division in the enforcing
// test. The seed budget is the seed stage's contract, not this block's; this
// block's contribution is that NOTHING here ever asks the divider anything.
//
// ---------------------------------------------------------------------------
// COST, AGAINST THE BLOCK IT REPLACES
// ---------------------------------------------------------------------------
// Per pixel: one QW add (q), one RW+1 add + compare + conditional subtract
// (r), and the emit rounding. Removed relative to attrstep: the second
// (negative-branch) step decomposition and its muxes, the 96-bit shadow
// accumulator and its adder, the sign compare, the per-row 36-clock divider
// occupancy, and the private divider instance itself. Added: the y-step pair
// and the row-base pair (the row jump is <= 4 pair-add cycles against a
// 36-clock divide). A row of 16 costs 4 + 16 clocks against attrstep's
// 36 + 16 — and the walker never stalls on a divider that is busy seeding
// some other plane.
`default_nettype none

module zhao_raster_attrwalk #(
    // Quotient width: full plane width so the WALK is exact everywhere and
    // range is a per-pixel emit question, never a walk question.
    parameter int unsigned QW = 96,
    // Remainder/area width: the divisor (zref's `area`, GEOM's 2A) is 47 bits.
    parameter int unsigned RW = 47,
    // The tie law. 0 = ties away from zero (RASTER.ATTRDIV's shipped law,
    // today's RTL boundary). 1 = ties toward +infinity (rast.cpp's ACTUAL law,
    // spec qformats §1/§4). See the header: the owner ratifies this, and the
    // two differ ONLY on negative exact halves.
    parameter bit TIE_TOWARD_POSITIVE = 1'b0
) (
    input var logic clk,
    input var logic rst_n,

    // ---- one attribute plane for one tile, as three exact pairs -------------
    input  var logic                   job_valid_i,
    output var logic                   job_ready_o,
    input  var logic signed [QW-1:0]   job_q0_i,
    input  var logic        [RW-1:0]   job_r0_i,
    input  var logic signed [QW-1:0]   job_dqx_i,
    input  var logic        [RW-1:0]   job_drx_i,
    input  var logic signed [QW-1:0]   job_dqy_i,
    input  var logic        [RW-1:0]   job_dry_i,
    input  var logic        [RW-1:0]   job_area_i,   // A > 0; zero refuses the job

    // ---- coverage in: RASTER.EDGEWALK's rows, unchanged ---------------------
    input  var logic        cov_valid_i,
    output var logic        cov_ready_o,
    input  var logic [3:0]  cov_row_i,
    input  var logic [15:0] cov_mask_i,
    input  var logic        cov_last_i,

    // ---- the attribute, raster order ---------------------------------------
    output var logic               q_valid_o,
    input  var logic               q_ready_i,
    output var logic signed [31:0] q_o,
    output var logic        [3:0]  q_row_o,
    output var logic        [3:0]  q_col_o,
    output var logic               q_last_o,
    output var logic               q_error_o,  // zero area, or this pixel out of s32 range

    // ---- evidence -----------------------------------------------------------
    output var logic [31:0] pixels_o,
    output var logic [31:0] range_errs_o   // covered pixels emitted with q_error_o
);

  localparam logic [2:0] S_IDLE   = 3'd0;
  localparam logic [2:0] S_PREP   = 3'd1;  // 3 clocks: double the y pair to 2/4/8 rows
  localparam logic [2:0] S_ROW    = 3'd2;  // wait for a coverage row
  localparam logic [2:0] S_ROWADD = 3'd3;  // 4 clocks: base + row-weighted y pairs
  localparam logic [2:0] S_WALK   = 3'd4;

  logic [2:0] st_r;
  logic [1:0] prep_r;   // which doubling / which row-add stage
  logic [1:0] rowk_r;

  // the job, latched
  logic signed [QW-1:0] baseq_r;
  logic        [RW-1:0] baser_r;
  logic signed [QW-1:0] dqx_r;
  logic        [RW-1:0] drx_r;
  logic        [RW-1:0] area_r;

  // y-step pairs for 1, 2, 4, 8 rows (index k = 2^k rows)
  logic signed [QW-1:0] yq_r[4];
  logic        [RW-1:0] yr_r[4];

  // the walk pair, and the row it was accumulated for
  logic signed [QW-1:0] q_r;
  logic        [RW:0]   r_r;      // one spare bit for the pre-wrap sum

  logic [15:0] mask_r;
  logic [3:0]  row_r, col_r;
  logic        last_row_r, err_r;

  // ---- pair doubling: (q, r) for 2^k rows from 2^(k-1) ---------------------
  logic signed [QW-1:0] dblq_c;
  logic        [RW:0]   dblr_c;
  always_comb begin
    dblr_c = {yr_r[prep_r], 1'b0};                        // 2r
    dblq_c = yq_r[prep_r] <<< 1;                          // 2q
    if (dblr_c >= {1'b0, area_r}) begin
      dblr_c = dblr_c - {1'b0, area_r};
      dblq_c = dblq_c + {{(QW - 1) {1'b0}}, 1'b1};
    end
  end

  // ---- pair add: walk pair += y pair k (row accumulation) ------------------
  logic signed [QW-1:0] rowq_c;
  logic        [RW:0]   rowr_c;
  always_comb begin
    rowr_c = r_r + {1'b0, yr_r[rowk_r]};
    rowq_c = q_r + yq_r[rowk_r];
    if (rowr_c >= {1'b0, area_r}) begin
      rowr_c = rowr_c - {1'b0, area_r};
      rowq_c = rowq_c + {{(QW - 1) {1'b0}}, 1'b1};
    end
  end

  // ---- one pixel of the walk: the whole recurrence -------------------------
  logic signed [QW-1:0] nq_c;
  logic        [RW:0]   nr_c;
  always_comb begin
    nr_c = r_r + {1'b0, drx_r};
    nq_c = q_r + dqx_r;
    if (nr_c >= {1'b0, area_r}) begin
      nr_c = nr_c - {1'b0, area_r};
      nq_c = nq_c + {{(QW - 1) {1'b0}}, 1'b1};
    end
  end

  // ---- the emit-side rounding law ------------------------------------------
  // rounded = q + (2r > A) + (2r == A && tie_up); see the header for the two
  // tie laws and which one is whose.
  logic [RW:0]          two_r_c;
  logic                 gt_c, eq_c, tie_up_c, bump_c;
  logic signed [QW-1:0] rounded_c;
  logic                 fits_c;
  always_comb begin
    two_r_c   = {r_r[RW-1:0], 1'b0};
    gt_c      = (two_r_c > {1'b0, area_r});
    eq_c      = (two_r_c == {1'b0, area_r});
    tie_up_c  = TIE_TOWARD_POSITIVE ? 1'b1 : ~q_r[QW-1];
    bump_c    = gt_c || (eq_c && tie_up_c);
    rounded_c = q_r + {{(QW - 1) {1'b0}}, bump_c};
    // s32 range: every bit above 30 must equal the sign bit. This is the
    // full-width check attrdiv lacks (its [2^31, 2^32) wrap hole).
    fits_c    = (rounded_c[QW-1:31] == {(QW - 31) {rounded_c[QW-1]}});
  end

  assign job_ready_o = (st_r == S_IDLE);
  assign cov_ready_o = (st_r == S_ROW);

  assign q_valid_o = (st_r == S_WALK) && mask_r[col_r];
  assign q_o       = (err_r || !fits_c) ? 32'sd0 : rounded_c[31:0];
  assign q_row_o   = row_r;
  assign q_col_o   = col_r;
  assign q_error_o = err_r || !fits_c;

  logic no_more_cols_c;
  always_comb begin
    no_more_cols_c = 1'b1;
    for (int unsigned cc = 0; cc < 16; ++cc)
      if ((cc > {28'd0, col_r}) && mask_r[cc]) no_more_cols_c = 1'b0;
  end
  assign q_last_o = last_row_r && no_more_cols_c;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_r         <= S_IDLE;
      prep_r       <= 2'd0;
      rowk_r       <= 2'd0;
      baseq_r      <= '0;
      baser_r      <= '0;
      dqx_r        <= '0;
      drx_r        <= '0;
      area_r       <= '0;
      yq_r[0]      <= '0;
      yq_r[1]      <= '0;
      yq_r[2]      <= '0;
      yq_r[3]      <= '0;
      yr_r[0]      <= '0;
      yr_r[1]      <= '0;
      yr_r[2]      <= '0;
      yr_r[3]      <= '0;
      q_r          <= '0;
      r_r          <= '0;
      mask_r       <= 16'd0;
      row_r        <= 4'd0;
      col_r        <= 4'd0;
      last_row_r   <= 1'b0;
      err_r        <= 1'b0;
      pixels_o     <= 32'd0;
      range_errs_o <= 32'd0;
    end else begin
      case (st_r)
        S_IDLE: begin
          if (job_valid_i && job_ready_o) begin
            baseq_r <= job_q0_i;
            baser_r <= job_r0_i;
            dqx_r   <= job_dqx_i;
            drx_r   <= job_drx_i;
            yq_r[0] <= job_dqy_i;
            yr_r[0] <= job_dry_i;
            area_r  <= job_area_i;
            err_r   <= (job_area_i == '0);
            prep_r  <= 2'd0;
            st_r    <= (job_area_i == '0) ? S_ROW : S_PREP;
          end
        end

        // ---- three clocks, once per job: y pairs for 2, 4, 8 rows ----------
        S_PREP: begin
          yq_r[prep_r+2'd1] <= dblq_c;
          yr_r[prep_r+2'd1] <= dblr_c[RW-1:0];
          if (prep_r == 2'd2) st_r <= S_ROW;
          else prep_r <= prep_r + 2'd1;
        end

        S_ROW: begin
          if (cov_valid_i && cov_ready_o) begin
            // the ROW-BASE COPY: start from the job base, add the row's
            // weighted y pairs over the next four clocks. No divider.
            q_r        <= baseq_r;
            r_r        <= {1'b0, baser_r};
            mask_r     <= cov_mask_i;
            row_r      <= cov_row_i;
            col_r      <= 4'd0;
            last_row_r <= cov_last_i;
            rowk_r     <= 2'd0;
            st_r       <= err_r ? S_WALK : S_ROWADD;
          end
        end

        // ---- four clocks per row: base + row*dNdy, as exact pair adds ------
        S_ROWADD: begin
          if (row_r[rowk_r]) begin
            q_r <= rowq_c;
            r_r <= {1'b0, rowr_c[RW-1:0]};
          end
          if (rowk_r == 2'd3) st_r <= S_WALK;
          else rowk_r <= rowk_r + 2'd1;
        end

        S_WALK: begin
          if (!q_valid_o || q_ready_i) begin
            if (q_valid_o) begin
              pixels_o <= pixels_o + 32'd1;
              if (q_error_o) range_errs_o <= range_errs_o + 32'd1;
            end
            if (col_r == 4'd15) begin
              st_r <= last_row_r ? S_IDLE : S_ROW;
            end else begin
              col_r <= col_r + 4'd1;
              if (!err_r) begin
                q_r <= nq_c;
                r_r <= {1'b0, nr_c[RW-1:0]};
              end
            end
          end
        end

        default: st_r <= S_IDLE;
      endcase
    end
  end

  // The walk invariant, live in simulation: 0 <= r < A whenever a value can be
  // emitted. Verilator executes this; Quartus is kept out via translate_off
  // (which Verilator deliberately does NOT honour — CLAUDE.md 2026-09-09).
  // synthesis translate_off
  // The rst_n term is a deliberate SYNCHRONOUS read: in reset r_r and area_r
  // are both zero, where "r < A" is vacuously false and must not fire.
  /* verilator lint_off SYNCASYNCNET */
  always_ff @(posedge clk) begin
    if (rst_n && (st_r == S_WALK) && !err_r && (r_r >= {1'b0, area_r}))
      $fatal(1, "attrwalk: remainder escaped [0, A)");
  end
  /* verilator lint_on SYNCASYNCNET */
  // synthesis translate_on

endmodule : zhao_raster_attrwalk

`default_nettype wire
