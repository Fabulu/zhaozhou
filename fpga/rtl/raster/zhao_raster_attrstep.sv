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
// The law does not move. RASTER.ATTRDIV stays the oracle:
//
//     q(N) = sign(N) * floor((2|N| + A) / (2A))
//
// which rounds half AWAY FROM ZERO. The ruling stated the target as
// floor((N + floor(A/2))/A), and the proof's first finding is that THOSE ARE
// DIFFERENT LAWS -- they disagree on every negative exact half, which would
// move every golden capture CRC. This block implements ours.
//
// ---------------------------------------------------------------------------
// THE RECURRENCE, AND WHY IT NEEDS TWO BRANCHES
// ---------------------------------------------------------------------------
// Write the shipped law as two floor divisions, one per sign of N:
//
//     N >= 0 :  q =  floor(M  / D),  M  =  2N + A,   M  steps by  +2*dNdx
//     N <  0 :  q = -floor(M' / D),  M' = -2N + A,   M' steps by  -2*dNdx
//
// with D = 2A. Within one sign region each is a plain floor division of an
// integer advancing by a constant, so the Euclidean recurrence
//
//     q += qd;  r += rd;  if (r >= D) { r -= D; ++q; }
//
// is exact. N is stepped as an ordinary integer add -- RASTER.INTERP already
// does exactly that -- so the sign is known for free and a crossing simply
// reseeds. A row is 16 pixels and N is linear, so A ROW CROSSES ZERO AT MOST
// ONCE.
//
// ---------------------------------------------------------------------------
// WHERE THE STEP DECOMPOSITION COMES FROM: ONE DIVIDE, NOT FOUR
// ---------------------------------------------------------------------------
// The recurrence needs (qd, rd) with 2*dNdx = qd*D + rd. Since D = 2A that is
// just the Euclidean division of dNdx by A, doubled:
//
//     dNdx = A*f + g   =>   qd = f,  rd = 2g
//
// and RASTER.ATTRDIV already computes exactly that pair, in disguise. It
// returns q_mag = floor((2|n|+A)/(2A)) and rem = (2|n|+A) mod 2A, from which
//
//     rem >= A :  f = q_mag,      g = (rem - A)/2
//     rem <  A :  f = q_mag - 1,  g = (rem + A)/2
//
// recovers the magnitude pair exactly, and the sign is applied by
//
//     n <  0, g > 0 :  floor = -f-1,  mod = A-g
//     n <  0, g = 0 :  floor = -f,    mod = 0
//
// Both identities are checked over 200,000 random cases in the directed test
// before any of this is trusted. So ONE divide yields the x step, and the
// negative branch's step is derived from it without a second:
//
//     dM = qd*D + rd   =>   -dM = (rd == 0) ? (-qd)*D : (-qd-1)*D + (D-rd)
//
// ---------------------------------------------------------------------------
// WHAT IT COSTS
// ---------------------------------------------------------------------------
// Per attribute per tile: one divide for the x step, one to seed each covered
// row, and at most one more per row at a sign crossing. Per pixel: one add,
// one compare, one conditional subtract. The proof measures the total at 0.099
// divides a pixel.
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
  logic               dv_valid, dv_ready, dv_rvalid, dv_rready, dv_ovf;
  /* verilator lint_on UNUSEDSIGNAL */
  logic signed [95:0] dv_num;
  logic signed [31:0] dv_q;
  logic [47:0]        dv_rem;
  zhao_raster_attrdiv #(.RADIX(4)) u_div (
      .clk          (clk),
      .rst_n        (rst_n),
      .v_valid_i    (dv_valid),
      .v_ready_o    (dv_ready),
      .num_i        (dv_num),
      .area_i       (job_area_r),
      .r_valid_o    (dv_rvalid),
      .r_ready_i    (dv_rready),
      .q_o          (dv_q),
      .q_overflow_o (dv_ovf),
      .rem_o        (dv_rem),
      /* verilator lint_off PINCONNECTEMPTY */
      .divides_o    (),
      .busy_clocks_o()
      /* verilator lint_on PINCONNECTEMPTY */
  );

  localparam logic [2:0] S_IDLE  = 3'd0;
  localparam logic [2:0] S_STEP  = 3'd1;  // divide dNdx by A, once per job
  localparam logic [2:0] S_ROW   = 3'd2;  // wait for a coverage row
  localparam logic [2:0] S_SEED  = 3'd3;  // divide to seed (q, r) for this pixel
  localparam logic [2:0] S_WALK  = 3'd4;

  logic [2:0]         st_r;
  logic signed [95:0] dndx_r, dndy_r, base_r, acc_n_r;
  logic [46:0]        job_area_r;
  logic [48:0]        d_r;          // D = 2A

  // the x step, both branches
  logic signed [95:0] qxp_r, qxn_r;   // quotient part of the step
  logic [48:0]        rxp_r, rxn_r;   // remainder part, 0 <= r < D

  // ---- the running pair -----------------------------------------------------
  // p_r is the MAGNITUDE-side quotient: floor(M/D) in the positive branch and
  // floor(M'/D) in the negative one. The emitted attribute is its negation when
  // the branch is negative.
  //
  // The first version tracked the SIGNED quotient and added the branch step to
  // it, which stepped the negative branch the wrong way -- the error grew by
  // exactly two steps a pixel and the directed test caught it on the second
  // pixel of the first case. q = -P means q steps by MINUS what P steps by, and
  // conflating the two is the whole bug.
  logic signed [95:0] p_r;
  logic [48:0]        r_r;
  logic               sign_r, seeded_r;

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

  // ---- recover the Euclidean pair from the divider's (q, rem) --------------
  // See WHERE THE STEP DECOMPOSITION COMES FROM. `dv_q` carries the sign; the
  // magnitude pair is recovered from |q| and the remainder, then the sign is
  // applied.
  logic signed [95:0] mag_q_c;
  logic [47:0]        f_g_c;      // g, the magnitude remainder
  logic signed [95:0] f_c;        // f, the magnitude quotient
  always_comb begin
    mag_q_c = (dv_q[31]) ? (-(96'(dv_q))) : 96'(dv_q);
    if (dv_rem >= 48'({1'b0, job_area_r})) begin
      f_c   = mag_q_c;
      f_g_c = (dv_rem - 48'({1'b0, job_area_r})) >> 1;
    end else begin
      f_c   = mag_q_c - 96'sd1;
      f_g_c = (dv_rem + 48'({1'b0, job_area_r})) >> 1;
    end
  end

  // ---- the x step in the POSITIVE branch, and its negation ----------------
  // Hoisted to module scope: SystemVerilog will not take a bit-select of a
  // parenthesised expression, and will not accept a declaration after a
  // statement inside an unnamed block. Both were syntax errors in the first
  // version of this file.
  logic signed [95:0] qp_c, qxn_c;
  logic [48:0]        rp_c, rxn_c;
  always_comb begin
    if (!dndx_r[95]) begin
      qp_c = f_c;
      rp_c = 49'({1'b0, f_g_c}) << 1;
    end else if (f_g_c != 48'd0) begin
      qp_c = -f_c - 96'sd1;
      rp_c = (49'({1'b0, job_area_r}) - 49'({1'b0, f_g_c})) << 1;
    end else begin
      qp_c = -f_c;
      rp_c = 49'd0;
    end
    // -dM = (rd == 0) ? (-qd)*D : (-qd-1)*D + (D - rd)
    if (rp_c == 49'd0) begin
      qxn_c = -qp_c;
      rxn_c = 49'd0;
    end else begin
      qxn_c = -qp_c - 96'sd1;
      rxn_c = d_r - rp_c;
    end
  end

  // ---- one pixel of the walk ----------------------------------------------
  // Only next_n_c's SIGN is read -- it exists to answer "does the next pixel
  // belong to the other branch", not to be an accumulator.
  /* verilator lint_off UNUSEDSIGNAL */
  logic signed [95:0] next_n_c;
  /* verilator lint_on UNUSEDSIGNAL */
  logic signed [95:0] nq_c;
  logic [48:0]        rsum_c, nr_c;
  always_comb begin
    next_n_c = acc_n_r + dndx_r;
    // Both branches ADD their own step to the magnitude quotient; what differs
    // is which decomposition is used, not the direction of the add.
    nq_c     = p_r + (sign_r ? qxn_r : qxp_r);
    rsum_c   = r_r + (sign_r ? rxn_r : rxp_r);
    if (rsum_c >= d_r) begin
      nr_c = rsum_c - d_r;
      nq_c = nq_c + 96'sd1;
    end else begin
      nr_c = rsum_c;
    end
  end

  assign job_ready_o = (st_r == S_IDLE);
  assign cov_ready_o = (st_r == S_ROW);

  assign q_valid_o = (st_r == S_WALK) && mask_r[col_r] && seeded_r;
  assign q_o       = err_r ? 32'sd0
                   : (sign_r ? 32'(-p_r[31:0]) : 32'(p_r[31:0]));
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
      d_r        <= 49'd0;
      qxp_r      <= 96'sd0;
      qxn_r      <= 96'sd0;
      rxp_r      <= 49'd0;
      rxn_r      <= 49'd0;
      p_r        <= 96'sd0;
      r_r        <= 49'd0;
      sign_r     <= 1'b0;
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
            d_r        <= {1'b0, job_area_i, 1'b0};  // D = 2A
            err_r      <= (job_area_i == 47'd0);
            st_r       <= (job_area_i == 47'd0) ? S_ROW : S_STEP;
          end
        end

        // ---- one divide per JOB: the x step, both branches -----------------
        S_STEP: begin
          if (dv_rvalid) begin
            divides_o <= divides_o + 32'd1;
            if (dv_ovf) begin
              err_r <= 1'b1;
            end else begin
              // qd = floor(dNdx/A), rd = 2*(dNdx mod A), and the negated branch
              qxp_r <= qp_c;
              rxp_r <= rp_c;
              qxn_r <= qxn_c;
              rxn_r <= rxn_c;
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
            if (dv_ovf) begin
              err_r <= 1'b1;
            end else begin
              // |dv_q| is exactly floor(M/D) for the branch acc_n_r is in.
              p_r    <= dv_q[31] ? (-(96'(dv_q))) : 96'(dv_q);
              r_r    <= 49'({1'b0, dv_rem});
              sign_r <= acc_n_r[95];
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
              col_r   <= col_r + 4'd1;
              acc_n_r <= acc_n_r + dndx_r;
              // Advance the pair in the CURRENT branch. A sign change is caught
              // below and forces a reseed rather than a wrong step.
              if (!err_r) begin
                p_r <= nq_c;
                r_r <= nr_c;
                // If the NEXT pixel's N has a different sign, the pair belongs
                // to the wrong branch and must be reseeded rather than stepped.
                if (next_n_c[95] != sign_r) begin
                  seeded_r <= 1'b0;
                  st_r     <= S_SEED;
                end
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
