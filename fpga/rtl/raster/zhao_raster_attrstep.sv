// zhao_raster_attrstep.sv — RASTER.ATTRSTEP: one attribute across a tile by
// ADDS, with the divide moved off the per-pixel path.
//
// ENFORCED-BY: tests/raster/raster_attrstep_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHAT THIS REPLACES, AND WHAT IT MUST NOT CHANGE
// ---------------------------------------------------------------------------
// Owner ruling 2026-08-31 #2: do not freeze a large per-pixel divider farm;
// first prove and prototype exact quotient/remainder stepping, and keep the
// divider as oracle and fallback.
//
// `tests/proofs/attribute_step_equivalence.cpp` is that proof: 640,000
// pixel-attributes, zero mismatches, 0.099 divides per pixel against 1.000 --
// AND EVERY RENDERED BIT UNCHANGED. This block is the RTL of that proof.
//
// ---------------------------------------------------------------------------
// THE LAW MOVED ON 2026-09-20, AND THE MACHINERY GOT SMALLER (R100 / R104)
// ---------------------------------------------------------------------------
// This block seeded from `zhao_raster_attrdiv` (v1) until today, whose law is
//
//     q(N) = sign(N) * floor((2|N| + A) / (2A))        round half AWAY FROM ZERO
//
// The ratified law is round-half-UP -- `spec/qformats.md:53` and `:147`,
// `rast.cpp:31-41`'s `div_rhu_s128`, all three agreeing:
//
//     q(N) = floor((N + floor(A/2)) / A)               ties toward +INFINITY
//
// R100 measured the difference over 640,000 sampled pairs: v2 matches the
// reference EXACTLY in every sweep, v1 disagrees on 100% of negative exact
// halves with an even divisor, a rate of about 1/(4d) -- over 10% on a
// one-subpixel triangle. ADOPTING V2 IS A BUG FIX, and R86 makes composing the
// superseded module fatal besides.
//
// ---------------------------------------------------------------------------
// ONE BRANCH, NOT TWO -- AND THAT IS THE REAL CONTENT OF THIS CHANGE
// ---------------------------------------------------------------------------
// The old law is SYMMETRIC ABOUT ZERO, so its quotient is a magnitude and the
// representation BREAKS AT ZERO. Everything below existed to serve that break:
// two step decompositions (qxp/rxp and qxn/rxn) muxed per pixel, a `sign_r`
// bit, a 96-bit shadow accumulator stepped purely to learn the NEXT pixel's
// sign, and a RESEED THROUGH THE DIVIDER whenever a row crossed zero.
//
// The ratified law is a plain floor division, and floor is CONTINUOUS ACROSS
// ZERO. Write M = N + floor(A/2); M advances by dNdx exactly, because the bias
// is a constant. Then for any signs whatever,
//
//     q = floor(M / A),   r = M - q*A,   0 <= r < A
//     dNdx = dq*A + dr,   0 <= dr < A
//     per pixel:  q += dq;  r += dr;  if (r >= A) { r -= A; ++q; }
//
// is exact everywhere, with no case at zero. So this change DELETES the second
// decomposition, the sign bit, the shadow accumulator's per-pixel add and the
// mid-row reseed. None of that is function being removed: the function is
// "the correct attribute at every covered pixel", and it is preserved and
// measured across zero -- `raster_attrdiv_v2_rem_directed` walks the pair
// through zero one unit at a time over six sign crossings with zero breaks,
// and section 2 of this block's own directed test drives planes that cross.
// What is removed is machinery that only ever compensated for the old law.
//
// This is also, deliberately, the representation `zhao_raster_attrwalk` uses.
// That block is the divider-free successor to this one; it cannot replace it
// today because NOTHING IN THE TREE BUILDS THE THREE EUCLIDEAN PAIRS ITS JOB
// PORT REQUIRES -- there is no seed stage, verified by search, so adopting it
// here would have converted a working self-contained block into a port with no
// producer. This block keeps its own divider and therefore keeps working.
//
// ---------------------------------------------------------------------------
// WHERE THE STEP DECOMPOSITION COMES FROM: STILL ONE DIVIDE
// ---------------------------------------------------------------------------
// The recurrence needs (dq, dr) with dNdx = dq*A + dr, which is the plain
// Euclidean division of dNdx by A. v2 does not publish that directly, because
// it ALWAYS adds the rounding bias h = floor(A/2). Undoing the bias is one
// compare and one add: from dNdx + h = q'*A + r',
//
//     r' >= h :  dq = q',      dr = r' - h
//     r' <  h :  dq = q' - 1,  dr = r' + A - h
//
// which replaces the old f/g recovery line for line -- the same shape, against
// the correct divisor. It is proved against the REAL divider rather than a
// restatement of it, over 3,026 decompositions with zero wrong, in
// `tests/raster/raster_attrdiv_v2_rem_directed.cpp` section 5.
//
// The negative branch's step used to be derived from the positive one by
// `-dM = (rd == 0) ? (-qd)*D : (-qd-1)*D + (D-rd)`. There is no negative
// branch now, so that identity and its two registers are gone.
//
// ---------------------------------------------------------------------------
// SATURATION IS REFUSED AT A SEED, AND THAT IS A DECISION
// ---------------------------------------------------------------------------
// v1 published `q_overflow_o` and a zero quotient. v2 publishes `q_error_o`
// (zero area, quotient meaningless) and `q_saturated_o` (quotient clamped to
// INT32_MAX/MIN -- a VALID answer, and the one the reference gives).
//
// This block refuses on BOTH, so its observable behaviour on the overflow path
// is exactly v1's. The reason is not caution, it is arithmetic: a saturated
// quotient is clamped, so `q*A + r == M` no longer holds and the pair is not a
// legal recurrence seed. Stepping a clamped pair would produce sixteen
// confidently wrong pixels instead of sixteen flagged ones. Emitting the
// saturated VALUE per pixel would be strictly better than refusing, and
// `zhao_raster_attrwalk` already does it by carrying the pair at full width
// and range-checking at emit -- but that needs v2 to publish the UNSATURATED
// quotient, which it does not, so it is named here and not faked.
//
// NOT FIXED HERE, AND NOT MADE WORSE: `q_o` is `p_r[31:0]` with no per-pixel
// range check, so a quotient that grows past 2^31 MID-ROW is emitted
// sign-flipped and unflagged. That is a pre-existing defect of this block
// (attrwalk's header records it at :66-70) and the v2 swap neither causes nor
// repairs it -- v1 truncated identically. The repair is attrwalk's full-width
// pair plus an emit-time check; doing it here would be a second implementation
// of that.
//
// ---------------------------------------------------------------------------
// WHAT IT COSTS
// ---------------------------------------------------------------------------
// Per attribute per tile: one divide for the x step and one to seed each
// covered row. THE SIGN-CROSSING RESEED IS GONE -- that was v1's law needing a
// new branch, and the ratified law has no branches, so a row costs exactly one
// divide however many times it crosses zero. Measured by the directed test:
// 1,536 pixel-attributes with 69 SIGN CHANGES inside the walk, zero reseeds,
// every pixel matching the reference; and a 256-pixel tile at 17 divides,
// 0.066 a pixel against 1.000 through the bare divider, 15.1x.
//
// Per pixel: one add, one compare, one conditional subtract -- and no branch
// mux and no 96-bit shadow accumulator, both of which existed only to answer
// "which side of zero is the next pixel on".
//
// THE DIVIDES THEMSELVES GOT LONGER, and that is the other half of the trade.
// v2 walks 98 quotient positions where v1 walked 33, so a seed is 52 clocks at
// radix 4 rather than 20. Fewer divides, each about 2.6x longer: on this tile
// 17 x 52 = 884 clocks against v1's 17 x 20 = 340 plus its crossing reseeds.
// The rounding is the reason for the swap and it is not negotiable (R100), but
// the occupancy is a real cost and recovering it -- teaching v2 v1's window
// trick, which touches no rounding -- is a named follow-on, not an assumption.
//
// This block is a PROTOTYPE in the ruling's sense: it exists so the recurrence
// can be composed against the divider path and compared for resource and Fmax
// before UNITS is frozen. The divider path stays.
`default_nettype none

module zhao_raster_attrstep (
    input var logic clk,
    input var logic rst_n,

    // ---- one attribute plane for one tile ------------------------------------
    input  var logic               job_valid_i,
    output var logic               job_ready_o,
    input  var logic signed [95:0] job_n0_i,     // GEOM.ATTRSETUP's plane,
    input  var logic signed [71:0] job_dndx_i,   // anchored at the origin
    input  var logic signed [71:0] job_dndy_i,
    input  var logic        [46:0] job_area_i,   // 2A, > 0 after winding
    input  var logic signed [11:0] job_tile_x_i, // the tile's top-left PIXEL
    input  var logic signed [11:0] job_tile_y_i,

    // ---- coverage in: RASTER.EDGEWALK's rows, unchanged ----------------------
    input  var logic        cov_valid_i,
    output var logic        cov_ready_o,
    input  var logic [3:0]  cov_row_i,
    input  var logic [15:0] cov_mask_i,
    input  var logic        cov_last_i,

    // ---- the attribute, raster order ----------------------------------------
    output var logic               q_valid_o,
    input  var logic               q_ready_i,
    output var logic signed [31:0] q_o,
    output var logic        [3:0]  q_row_o,
    output var logic        [3:0]  q_col_o,
    output var logic               q_last_o,
    output var logic               q_error_o,   // a divide refused; q_o is not usable

    // ---- evidence: the whole point of the block ------------------------------
    output var logic [31:0] pixels_o,
    output var logic [31:0] divides_o    // seeds + reseeds + the one step divide
);

  // ---- the seeding divider -------------------------------------------------
  // Radix 4: this is off the per-pixel path now, but a seed still stalls a row.
  // `dv_ready` is not consulted: S_STEP and S_SEED are only entered with the
  // divider idle, and the FSM leaves on dv_rvalid, so a second request cannot
  // be offered while one is in flight. Sunk rather than wired into a condition
  // that would make `valid` depend on `ready`.
  /* verilator lint_off UNUSEDSIGNAL */
  logic               dv_valid, dv_ready, dv_rvalid, dv_rready;
  logic               dv_sat, dv_err;
  logic [31:0]        dv_remerr;
  /* verilator lint_on UNUSEDSIGNAL */
  logic signed [95:0] dv_num;
  logic signed [31:0] dv_q;
  logic [46:0]        dv_rem;
  // V2 AS OF 2026-09-20 (R100/R104). `rem_o` is 47 bits and is the TRUE FLOOR
  // remainder of M = num + floor(A/2) by A -- not v1's remainder, which was mod
  // 2A on a doubled dividend. `rem_range_err_o` is the unit's own width guard;
  // it is sunk here because this block reads the PAIR, and a pair that failed
  // the width check would already have broken the invariant the seed relies on.
  // Its positive control is the committed mutant beside the divider.
  zhao_raster_attrdiv_v2 #(.RADIX(4)) u_div (
      .clk             (clk),
      .rst_n           (rst_n),
      .v_valid_i       (dv_valid),
      .v_ready_o       (dv_ready),
      .num_i           (dv_num),
      .area_i          (job_area_r),
      .r_valid_o       (dv_rvalid),
      .r_ready_i       (dv_rready),
      .q_o             (dv_q),
      .q_saturated_o   (dv_sat),
      .q_error_o       (dv_err),
      .rem_o           (dv_rem),
      .rem_range_err_o (dv_remerr),
      /* verilator lint_off PINCONNECTEMPTY */
      .divides_o     (),
      .saturations_o (),
      .errors_o      (),
      .busy_clocks_o ()
      /* verilator lint_on PINCONNECTEMPTY */
  );

  localparam logic [2:0] S_IDLE  = 3'd0;
  localparam logic [2:0] S_STEP  = 3'd1;  // divide dNdx by A, once per job
  localparam logic [2:0] S_ROW   = 3'd2;  // wait for a coverage row
  localparam logic [2:0] S_SEED  = 3'd3;  // divide to seed (q, r) for this pixel
  localparam logic [2:0] S_WALK  = 3'd4;

  logic [2:0]         st_r;
  logic signed [95:0] dndx_r, dndy_r, base_r, acc_n_r;
  logic [46:0]        job_area_r;   // A, the divisor. v1 carried D = 2A here.

  // ---- the x step: ONE decomposition, because floor does not break at zero --
  // dNdx = dqx*A + drx, 0 <= drx < A. v1 needed a second pair (qxn/rxn) for the
  // negative branch and a mux per pixel; the ratified law has no branches, so
  // there is nothing to mux and nothing to negate.
  logic signed [95:0] qxp_r;     // dqx
  logic [46:0]        rxp_r;     // drx, 0 <= drx < A

  // ---- the running pair -----------------------------------------------------
  // p_r is now the SIGNED quotient floor(M/A) directly -- not a magnitude. That
  // is the whole simplification: v1's pair was a magnitude because its law was
  // symmetric about zero, which forced a sign bit, a shadow accumulator and a
  // reseed at every crossing.
  //
  // (The historical hazard that comment recorded is gone with the branch: v1's
  // first version added the branch step to a SIGNED quotient, which stepped the
  // negative branch the wrong way. There is one branch now and one step, so the
  // two quantities cannot be conflated.)
  logic signed [95:0] p_r;
  logic [47:0]        r_r;       // 0 <= r < A, one spare bit for the pre-wrap sum
  logic               seeded_r;

  logic [15:0] mask_r;
  logic [3:0]  row_r, col_r;
  logic        last_row_r, err_r;

  // ---- the tile's first pixel centre, exactly as RASTER.INTERP places it ----
  logic signed [95:0] dndx_c, dndy_c, base_c;
  always_comb begin
    dndx_c = 96'(job_dndx_i);
    dndy_c = 96'(job_dndy_i);
    base_c = 96'(job_n0_i) + dndx_c * 96'(job_tile_x_i) + dndy_c * 96'(job_tile_y_i) +
             (dndx_c >>> 1) + (dndy_c >>> 1);
  end

  // ---- row base: base + row*dNdy, four conditional adds --------------------
  logic signed [95:0] rowoff_c, rowbase_c;
  always_comb begin
    rowoff_c = 96'sd0;
    if (cov_row_i[0]) rowoff_c = rowoff_c + dndy_r;
    if (cov_row_i[1]) rowoff_c = rowoff_c + (dndy_r <<< 1);
    if (cov_row_i[2]) rowoff_c = rowoff_c + (dndy_r <<< 2);
    if (cov_row_i[3]) rowoff_c = rowoff_c + (dndy_r <<< 3);
    rowbase_c = base_r + rowoff_c;
  end

  // ---- undo the divider's rounding bias to get the PLAIN Euclidean pair ----
  // See WHERE THE STEP DECOMPOSITION COMES FROM. v2 returns the pair of
  // M = dNdx + h against A, where h = floor(A/2); this block needs the pair of
  // dNdx itself. One compare and one add, replacing v1's f/g recovery line for
  // line -- same shape, correct divisor, and no sign fixup because there is no
  // magnitude to re-sign.
  //
  // Proved against the real divider, not a restatement, over 3,026
  // decompositions with zero wrong: raster_attrdiv_v2_rem_directed section 5.
  logic [46:0]        bias_c;     // h = floor(A/2)
  logic signed [95:0] qp_c;       // dqx
  logic [46:0]        rp_c;       // drx
  always_comb begin
    bias_c = {1'b0, job_area_r[46:1]};
    if (dv_rem >= bias_c) begin
      qp_c = 96'(dv_q);
      rp_c = dv_rem - bias_c;
    end else begin
      qp_c = 96'(dv_q) - 96'sd1;
      rp_c = (dv_rem + job_area_r) - bias_c;
    end
  end

  // ---- one pixel of the walk ----------------------------------------------
  // ONE add, ONE compare, ONE conditional subtract, with no branch mux and no
  // shadow accumulator. v1 stepped a 96-bit `next_n_c` here purely to learn the
  // NEXT pixel's sign so it could decide whether to reseed; the floor pair is
  // continuous across zero, so nothing needs to know the sign and the adder is
  // gone with the question.
  logic signed [95:0] nq_c;
  logic [47:0]        rsum_c, nr_c;
  always_comb begin
    nq_c   = p_r + qxp_r;
    rsum_c = r_r + 48'({1'b0, rxp_r});
    if (rsum_c >= 48'({1'b0, job_area_r})) begin
      nr_c = rsum_c - 48'({1'b0, job_area_r});
      nq_c = nq_c + 96'sd1;
    end else begin
      nr_c = rsum_c;
    end
  end

  assign job_ready_o = (st_r == S_IDLE);
  assign cov_ready_o = (st_r == S_ROW);

  assign q_valid_o = (st_r == S_WALK) && mask_r[col_r] && seeded_r;
  // p_r IS the signed quotient now, so there is no sign to re-apply. v1 carried
  // a magnitude and negated it here.
  assign q_o       = err_r ? 32'sd0 : 32'(p_r[31:0]);
  assign q_row_o   = row_r;
  assign q_col_o   = col_r;
  assign q_error_o = err_r;

  logic no_more_cols_c;
  always_comb begin
    no_more_cols_c = 1'b1;
    for (int unsigned c = 0; c < 16; ++c)
      if ((c > {28'd0, col_r}) && mask_r[c]) no_more_cols_c = 1'b0;
  end
  assign q_last_o = last_row_r && no_more_cols_c;

  // ---- the divider's request, by state -------------------------------------
  always_comb begin
    dv_valid = 1'b0;
    dv_num   = 96'sd0;
    if (st_r == S_STEP) begin
      dv_valid = 1'b1;
      dv_num   = dndx_r;      // decompose one pixel of x step
    end else if (st_r == S_SEED) begin
      dv_valid = 1'b1;
      dv_num   = acc_n_r;     // seed (q, r) at this pixel's N
    end
  end
  assign dv_rready = (st_r == S_STEP) || (st_r == S_SEED);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_r       <= S_IDLE;
      dndx_r     <= 96'sd0;
      dndy_r     <= 96'sd0;
      base_r     <= 96'sd0;
      acc_n_r    <= 96'sd0;
      job_area_r <= 47'd0;
      qxp_r      <= 96'sd0;
      rxp_r      <= 47'd0;
      p_r        <= 96'sd0;
      r_r        <= 48'd0;
      seeded_r   <= 1'b0;
      mask_r     <= 16'd0;
      row_r      <= 4'd0;
      col_r      <= 4'd0;
      last_row_r <= 1'b0;
      err_r      <= 1'b0;
      pixels_o   <= 32'd0;
      divides_o  <= 32'd0;
    end else begin
      case (st_r)
        S_IDLE: begin
          if (job_valid_i && job_ready_o) begin
            dndx_r     <= dndx_c;
            dndy_r     <= dndy_c;
            base_r     <= base_c;
            job_area_r <= job_area_i;
            err_r      <= (job_area_i == 47'd0);
            st_r       <= (job_area_i == 47'd0) ? S_ROW : S_STEP;
          end
        end

        // ---- one divide per JOB: the x step, both branches -----------------
        S_STEP: begin
          if (dv_rvalid) begin
            divides_o <= divides_o + 32'd1;
            // REFUSE ON BOTH. `dv_err` is v1's refusal; `dv_sat` is new and is
            // NOT a refusal at the divider -- but a clamped quotient breaks
            // q*A + r == M, so it is not a legal step decomposition. See
            // SATURATION IS REFUSED AT A SEED in the header.
            if (dv_sat || dv_err) begin
              err_r <= 1'b1;
            end else begin
              // dqx = floor(dNdx/A), drx = dNdx mod A. One pair; v1 stored two.
              qxp_r <= qp_c;
              rxp_r <= rp_c;
            end
            st_r <= S_ROW;
          end
        end

        S_ROW: begin
          if (cov_valid_i && cov_ready_o) begin
            acc_n_r    <= rowbase_c;
            mask_r     <= cov_mask_i;
            row_r      <= cov_row_i;
            col_r      <= 4'd0;
            last_row_r <= cov_last_i;
            // A refused job (zero area, or a step that overflowed) still walks
            // the row and emits FLAGGED pixels. The first version left
            // `seeded_r` low on that path, so `q_valid_o` never rose and the
            // job hung -- the directed test timed out on it, which is how a
            // refusal is supposed to behave only if it also terminates.
            seeded_r   <= err_r;
            st_r       <= err_r ? S_WALK : S_SEED;
          end
        end

        // ---- seed or reseed (q, r) at the current pixel --------------------
        S_SEED: begin
          if (dv_rvalid) begin
            divides_o <= divides_o + 32'd1;
            if (dv_sat || dv_err) begin
              err_r <= 1'b1;
            end else begin
              // DIRECT. dv_q IS floor(M/A) -- the attribute itself, signed --
              // and dv_rem IS its floor remainder. v1 had to take a magnitude,
              // re-sign it, and latch which branch it belonged to.
              p_r <= 96'(dv_q);
              r_r <= 48'({1'b0, dv_rem});
            end
            seeded_r <= 1'b1;
            st_r     <= S_WALK;
          end
        end

        S_WALK: begin
          if (!q_valid_o || q_ready_i) begin
            if (q_valid_o) pixels_o <= pixels_o + 32'd1;
            if (col_r == 4'd15) begin
              st_r <= last_row_r ? S_IDLE : S_ROW;
            end else begin
              col_r <= col_r + 4'd1;
              // Advance the pair. NO BRANCH, NO CROSSING TEST, NO RESEED: the
              // floor pair is exact for every sign of M and dNdx, so a row is
              // now exactly ONE divide instead of one plus a reseed per
              // crossing. v1 also stepped a 96-bit acc_n_r here to learn the
              // next pixel's sign; nothing asks that question any more, so the
              // adder is gone rather than left driving nothing.
              if (!err_r) begin
                p_r <= nq_c;
                r_r <= nr_c;
              end
            end
          end
        end

        default: st_r <= S_IDLE;
      endcase
    end
  end

endmodule : zhao_raster_attrstep

`default_nettype wire
