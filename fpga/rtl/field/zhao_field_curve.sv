// zhao_field_curve.sv — the Field IR table ops: OP_CURVE, OP_DCURVE, OP_SPLINE.
//
// A submodule of the FIELD.SEQ.* family. Reference: the interpreter's
// `segment_search` (§3.15) and the three op cases that use it
// (reference/src/zfield/zfield_interpret.cpp).
//
// These are the first ops in the engine that READ A TABLE, so the block takes a
// table port rather than owning a ROM: the tables are per-program data carried
// in the `.zprog` image, not constants of the hardware. The port is a registered
// read (the M10K rule), which is why every fetch below costs two states.
//
// ---------------------------------------------------------------------------
// THE LAW
// ---------------------------------------------------------------------------
//     clamped = clamp_raw(a, x[0], x[n-1])
//     i       = 6-step compare/select search for the segment containing clamped
//
//     CURVE  : dst = fx_mad(fx_sub(clamped, x[i]), dy[i], y[i])
//     DCURVE : dst = dy[i]
//     SPLINE : t   = clamp(rescale((clamped - x[i]) * dy[i], 16), 0, 1)
//              C1  = p2 - p0
//              C2  = 2*p0 - 5*p1 + 4*p2 - p3
//              C3  = -p0 + 3*p1 - 3*p2 + p3          (each ONE saturate to s32)
//              u   = fx_mad(t, C3, C2); u = fx_mad(t, u, C1); v = fx_mul(t, u)
//              dst = fx_add(p1, rescale_s32(v, 1))
//     where p0..p3 are y at i-1, i, i+1, i+2 with the ENDS REPLICATED.
//
// Six things are load-bearing:
//
// 1. **THE SEARCH IS SIX STEPS, ALWAYS.** Not ceil(log2(n)) — six, for every
//    table, including a two-entry one. `lo` starts at 0 and each step k = 5..0
//    offers `mid = lo + (1<<k)`, taken only when `mid <= n-1` AND
//    `x[mid] <= clamped`. The decoder caps a table at 64 entries, which is
//    exactly what six steps reach; a search that re-derives its step count from
//    n is a different function the moment a table is not a power of two.
// 2. **THE SEARCH RUNS ON THE CLAMPED VALUE, NOT ON `a`.** Both bounds come
//    from the table's own ends. Searching the raw `a` agrees everywhere except
//    outside the table — which is the whole reason the clamp is there.
// 3. **`dy` MEANS TWO DIFFERENT THINGS.** For a curve table (kind 0) it is the
//    segment SLOPE and CURVE multiplies the offset by it. For a spline table
//    (kind 1) it is the RECIPROCAL of the uniform step and SPLINE multiplies by
//    it to get the segment parameter t. Same field, same width, different
//    meaning by kind — and nothing in the encoding distinguishes them, so a
//    block that treats one as the other returns plausible numbers forever.
// 4. **SPLINE's FINAL TERM IS `rescale_s32(v, 1)`, NOT `v << 16`.** The raw v is
//    halved — that is the one-half of Catmull-Rom — and halved with the
//    round-half-up rescale, not a shift. The `<< 16` form amplified the term by
//    2^16 and is a fixed defect (review C1, RUN-20260814-1912 wave-1); it is
//    named here so it does not come back.
// 5. **FOUR LEDGER LANES, IN ONE OP.** SPLINE alone records in `add` (the
//    fx_sub and the final fx_add), `mul` (the three Horner steps) and `rescale`
//    (the t product, the three coefficient saturates, and the halving). The
//    reference keeps them apart deliberately; a block that pools them can return
//    every number correctly and still misreport where the range was lost.
// 6. **THE ENDS ARE REPLICATED, NOT EXTRAPOLATED.** p0 at i == 0 is y[0] again,
//    and p3 past the end is y[n-1] again. The curve therefore flattens at the
//    ends rather than running off, and the replication is by INDEX before the
//    fetch, so no out-of-range entry is ever read.
//
// CURVE at the top segment is safe for the same reason clamping makes it so:
// `i` can be n-1, where `clamped - x[n-1]` is exactly zero and the slope term
// vanishes. No entry past the end is needed.
module zhao_field_curve (
    input logic clk,
    input logic rst_n,

    input  logic               v_valid_i,
    output logic               v_ready_o,
    input  logic        [ 1:0] mode_i,    // 0 = CURVE, 1 = DCURVE, 2 = SPLINE
    input  logic signed [31:0] a_i,
    input  logic        [ 6:0] tbl_n_i,   // entry count, 2..64 (decoder-enforced)

    // Table read port. `tbl_idx_o` is presented for a whole cycle and the three
    // lanes answer on the NEXT one — a registered read, per the M10K rules.
    output logic        [ 5:0] tbl_idx_o,
    input  logic signed [31:0] tbl_x_i,
    input  logic signed [31:0] tbl_y_i,
    input  logic signed [31:0] tbl_dy_i,

    output logic               r_valid_o,
    input  logic               r_ready_i,
    output logic signed [31:0] result_o,
    output logic        [ 5:0] seg_idx_o,      // the segment the search landed on
    output logic               sat_add_o,
    output logic               sat_mul_o,
    output logic               sat_rescale_o,

    // ---- the shared multiplier, `zhao_field_mul` ---------------------------
    // Four nonconstant products: the segment offset times the table's slope,
    // and three Horner steps. Under the DSP ruling of 2026-08-23 none of them
    // is this block's own silicon any more; each is an issue and a wait on the
    // engine's one lane, which costs two clocks apiece on an op that already
    // spends twelve in its binary search.
    //
    // The two CONSTANT products in `c2_wide` and `c3_wide` (times five and
    // times three) stay where they are. The rule is about NONCONSTANT
    // multipliers; a multiply by three is an add and a shift, and routing it
    // through a shared lane would cost four clocks to save nothing.
    output logic               mul_issue_o,
    output logic signed [32:0] mul_a_o,
    output logic signed [32:0] mul_b_o,
    // The lane is 66 bits wide because DOT3 needs three products summed. An op
    // that consumes ONE 32x32 product reads only the low 64 (or 32) of them,
    // which is a property of this op rather than a hole in the port.
    /* verilator lint_off UNUSEDSIGNAL */
    input  logic signed [65:0] mul_p_i,
    /* verilator lint_on UNUSEDSIGNAL */
    input  logic               mul_valid_i
);

  // M_CURVE (2'd0) is the fall-through case and so is never named in a
  // comparison; it is documented here rather than declared.
  localparam logic [1:0] M_DCURVE = 2'd1;

  localparam logic [4:0] S_IDLE = 5'd0;
  localparam logic [4:0] S_LO = 5'd1;
  localparam logic [4:0] S_LOD = 5'd2;
  localparam logic [4:0] S_HI = 5'd3;
  localparam logic [4:0] S_HID = 5'd4;
  localparam logic [4:0] S_SRCH = 5'd5;
  localparam logic [4:0] S_SRCHD = 5'd6;
  localparam logic [4:0] S_ENT = 5'd7;
  localparam logic [4:0] S_ENTD = 5'd8;
  localparam logic [4:0] S_ENTM = 5'd19;   // issue d_off * dy
  localparam logic [4:0] S_ENTW = 5'd20;
  localparam logic [4:0] S_P0 = 5'd9;
  localparam logic [4:0] S_P0D = 5'd10;
  localparam logic [4:0] S_P2 = 5'd11;
  localparam logic [4:0] S_P2D = 5'd12;
  localparam logic [4:0] S_P3 = 5'd13;
  localparam logic [4:0] S_P3D = 5'd14;
  localparam logic [4:0] S_H1 = 5'd15;
  localparam logic [4:0] S_H1W = 5'd21;
  localparam logic [4:0] S_H2 = 5'd16;
  localparam logic [4:0] S_H2W = 5'd22;
  localparam logic [4:0] S_H3 = 5'd17;
  localparam logic [4:0] S_H3W = 5'd23;
  localparam logic [4:0] S_OUT = 5'd18;

  logic [4:0] state;

  logic signed [31:0] h_a;
  logic        [ 1:0] h_mode;
  logic        [ 6:0] h_n;
  logic        [ 6:0] n_m1;
  assign n_m1 = h_n - 7'd1;

  logic signed [31:0] x_lo, clamped;
  logic        [ 6:0] lo;
  logic        [ 2:0] k;
  logic        [ 6:0] mid;
  assign mid = lo + (7'd1 << k);

  logic signed [31:0] p1, p0, p2;
  logic signed [31:0] tt, c1, c2, c3, u_reg;

  // The selected entry, HELD. The table read is registered and `tbl_idx_o` is a
  // function of the state, so by the time the shared lane has answered its
  // product the memory is no longer presenting entry `lo`. Everything the
  // finish needs is captured on the cycle the entry is on the port.
  logic signed [31:0] x_ent, y_ent, dy_ent;

  // ---- the primitives, each recording in its own lane ----------------------
  function automatic logic signed [31:0] add_sat(input logic signed [31:0] a,
                                                 input logic signed [31:0] b);
    logic signed [32:0] s;
    begin
      s = $signed({a[31], a}) + $signed({b[31], b});
      if (s > 33'sd2147483647) add_sat = 32'sh7FFF_FFFF;
      else if (s < -33'sd2147483648) add_sat = 32'sh8000_0000;
      else add_sat = s[31:0];
    end
  endfunction

  function automatic logic add_fired(input logic signed [31:0] a, input logic signed [31:0] b);
    logic signed [32:0] s;
    begin
      s = $signed({a[31], a}) + $signed({b[31], b});
      add_fired = (s > 33'sd2147483647) || (s < -33'sd2147483648);
    end
  endfunction

  function automatic logic signed [31:0] sub_sat(input logic signed [31:0] a,
                                                 input logic signed [31:0] b);
    logic signed [32:0] d;
    begin
      d = $signed({a[31], a}) - $signed({b[31], b});
      if (d > 33'sd2147483647) sub_sat = 32'sh7FFF_FFFF;
      else if (d < -33'sd2147483648) sub_sat = 32'sh8000_0000;
      else sub_sat = d[31:0];
    end
  endfunction

  function automatic logic sub_fired(input logic signed [31:0] a, input logic signed [31:0] b);
    logic signed [32:0] d;
    begin
      d = $signed({a[31], a}) - $signed({b[31], b});
      sub_fired = (d > 33'sd2147483647) || (d < -33'sd2147483648);
    end
  endfunction

  // round-half-up then saturate; the rounding add is carried one bit wider so a
  // value near the s64 rail cannot wrap before the shift.
  function automatic logic signed [31:0] resc(input logic signed [63:0] v, input int unsigned kk);
    logic signed [64:0] r;
    begin
      r = (kk == 0) ? 65'(v) : ((65'(v) + (65'sd1 <<< (kk - 1))) >>> kk);
      if (r > 65'sd2147483647) resc = 32'sh7FFF_FFFF;
      else if (r < -65'sd2147483648) resc = 32'sh8000_0000;
      else resc = r[31:0];
    end
  endfunction

  function automatic logic resc_fired(input logic signed [63:0] v, input int unsigned kk);
    logic signed [64:0] r;
    begin
      r = (kk == 0) ? 65'(v) : ((65'(v) + (65'sd1 <<< (kk - 1))) >>> kk);
      resc_fired = (r > 65'sd2147483647) || (r < -65'sd2147483648);
    end
  endfunction

  function automatic logic signed [31:0] sat40(input logic signed [39:0] v);
    begin
      if (v > 40'sd2147483647) sat40 = 32'sh7FFF_FFFF;
      else if (v < -40'sd2147483648) sat40 = 32'sh8000_0000;
      else sat40 = v[31:0];
    end
  endfunction

  function automatic logic sat40_fired(input logic signed [39:0] v);
    begin
      sat40_fired = (v > 40'sd2147483647) || (v < -40'sd2147483648);
    end
  endfunction

  function automatic logic signed [63:0] sx(input logic signed [31:0] v);
    sx = $signed({{32{v[31]}}, v});
  endfunction

  // ---- the table index, combinational per state ---------------------------
  // Law 6: the end replication happens HERE, on the index, so an out-of-range
  // entry is never fetched in the first place.
  always_comb begin
    case (state)
      S_LO: tbl_idx_o = 6'd0;
      S_HI: tbl_idx_o = n_m1[5:0];
      S_SRCH: tbl_idx_o = mid[5:0];
      S_ENT: tbl_idx_o = lo[5:0];
      S_P0: tbl_idx_o = (lo > 7'd0) ? (lo[5:0] - 6'd1) : 6'd0;
      S_P2: tbl_idx_o = ((lo + 7'd1) < h_n) ? (lo[5:0] + 6'd1) : n_m1[5:0];
      S_P3: tbl_idx_o = ((lo + 7'd2) < h_n) ? (lo[5:0] + 6'd2) : n_m1[5:0];
      default: tbl_idx_o = 6'd0;
    endcase
  end

  assign v_ready_o = (state == S_IDLE);
  assign seg_idx_o = lo[5:0];

  // ---- the SPLINE segment parameter, and CURVE's whole result -------------
  // All four wide terms now read the SAME wire -- the shared lane's product --
  // because only one of them is ever in flight. Which one is decided by the
  // state that issued it, three states earlier.
  logic signed [63:0] mul_p;
  assign mul_p = $signed(mul_p_i[63:0]);

  logic signed [63:0] d_prod, tt_c3, tt_u, tt_v, curve_p;
  logic signed [31:0] d_off, tt_raw;
  assign d_off   = sub_sat(clamped, x_ent);
  assign d_prod  = mul_p;
  assign tt_raw  = resc(d_prod, 16);
  assign curve_p = d_prod + (sx(y_ent) <<< 16);

  assign tt_c3   = mul_p + (sx(c2) <<< 16);
  assign tt_u    = mul_p + (sx(c1) <<< 16);
  assign tt_v    = mul_p;

  // The lane requests. Every operand here is s32 and SIGN-extends.
  logic signed [31:0] mul_a, mul_b;
  always_comb begin
    case (state)
      S_ENTM: begin
        mul_a = d_off;
        mul_b = dy_ent;
      end
      S_H1: begin
        mul_a = tt;
        mul_b = c3;
      end
      default: begin
        // S_H2 and S_H3 both walk `tt * u`, which is what makes this Horner.
        mul_a = tt;
        mul_b = u_reg;
      end
    endcase
  end

  assign mul_issue_o = (state == S_ENTM) || (state == S_H1) ||
                       (state == S_H2)   || (state == S_H3);
  assign mul_a_o = $signed({mul_a[31], mul_a});
  assign mul_b_o = $signed({mul_b[31], mul_b});

  logic signed [39:0] c1_wide, c2_wide, c3_wide;
  logic signed [39:0] w_p0, w_p1, w_p2, w_p3;
  assign w_p0    = $signed({{8{p0[31]}}, p0});
  assign w_p1    = $signed({{8{p1[31]}}, p1});
  assign w_p2    = $signed({{8{p2[31]}}, p2});
  assign w_p3    = $signed({{8{tbl_y_i[31]}}, tbl_y_i});
  assign c1_wide = w_p2 - w_p0;
  assign c2_wide = (w_p0 <<< 1) - (w_p1 * 40'sd5) + (w_p2 <<< 2) - w_p3;
  assign c3_wide = -w_p0 + (w_p1 * 40'sd3) - (w_p2 * 40'sd3) + w_p3;

  logic signed [31:0] half_v;
  assign half_v = resc(sx(u_reg), 1);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state <= S_IDLE;
      h_a <= '0;
      h_mode <= 2'd0;
      h_n <= 7'd2;
      x_lo <= '0;
      clamped <= '0;
      x_ent <= '0;
      y_ent <= '0;
      dy_ent <= '0;
      lo <= 7'd0;
      k <= 3'd5;
      p1 <= '0;
      p0 <= '0;
      p2 <= '0;
      tt <= '0;
      c1 <= '0;
      c2 <= '0;
      c3 <= '0;
      u_reg <= '0;
      r_valid_o <= 1'b0;
      result_o <= '0;
      sat_add_o <= 1'b0;
      sat_mul_o <= 1'b0;
      sat_rescale_o <= 1'b0;
    end else begin
      case (state)
        S_IDLE: begin
          if (v_valid_i) begin
            h_a <= a_i;
            h_mode <= mode_i;
            h_n <= tbl_n_i;
            lo <= 7'd0;
            k <= 3'd5;
            sat_add_o <= 1'b0;
            sat_mul_o <= 1'b0;
            sat_rescale_o <= 1'b0;
            state <= S_LO;
          end
        end

        S_LO: state <= S_LOD;
        S_LOD: begin
          x_lo  <= tbl_x_i;
          state <= S_HI;
        end

        S_HI: state <= S_HID;
        S_HID: begin
          // Law 2: clamp_raw, written in the reference's own order, so even a
          // malformed lo > hi table agrees.
          clamped <= (h_a < x_lo) ? x_lo : ((h_a > tbl_x_i) ? tbl_x_i : h_a);
          state   <= S_SRCH;
        end

        // Law 1: six steps, k = 5 down to 0, guarded by mid <= n-1.
        S_SRCH: state <= S_SRCHD;
        S_SRCHD: begin
          if ((mid <= n_m1) && (tbl_x_i <= clamped)) lo <= mid;
          if (k == 3'd0) state <= S_ENT;
          else begin
            k <= k - 3'd1;
            state <= S_SRCH;
          end
        end

        S_ENT: state <= S_ENTD;
        S_ENTD: begin
          p1     <= tbl_y_i;
          x_ent  <= tbl_x_i;
          y_ent  <= tbl_y_i;
          dy_ent <= tbl_dy_i;
          if (h_mode == M_DCURVE) begin
            // DCURVE reads the slope and is done: no product, no lane, no wait.
            result_o  <= tbl_dy_i;
            r_valid_o <= 1'b1;
            state     <= S_OUT;
          end else begin
            state <= S_ENTM;
          end
        end

        S_ENTM: state <= S_ENTW;
        S_ENTW: begin
          if (mul_valid_i) begin
            if (h_mode == 2'd0) begin
              result_o  <= resc(curve_p, 16);
              sat_add_o <= sub_fired(clamped, x_ent);
              sat_mul_o <= resc_fired(curve_p, 16);
              r_valid_o <= 1'b1;
              state     <= S_OUT;
            end else begin
              // Law 3: for a spline table dy is 1/step, so this product is the
              // segment parameter, not a value on the curve.
              sat_add_o     <= sub_fired(clamped, x_ent);
              sat_rescale_o <= resc_fired(d_prod, 16);
              tt <= (tt_raw < 32'sd0) ? 32'sd0 : ((tt_raw > 32'sd65536) ? 32'sd65536 : tt_raw);
              state <= S_P0;
            end
          end
        end

        S_P0: state <= S_P0D;
        S_P0D: begin
          p0    <= tbl_y_i;
          state <= S_P2;
        end

        S_P2: state <= S_P2D;
        S_P2D: begin
          p2    <= tbl_y_i;
          state <= S_P3;
        end

        S_P3: state <= S_P3D;
        S_P3D: begin
          c1 <= sat40(c1_wide);
          c2 <= sat40(c2_wide);
          c3 <= sat40(c3_wide);
          sat_rescale_o <= sat_rescale_o || sat40_fired(c1_wide) || sat40_fired(c2_wide) ||
              sat40_fired(c3_wide);
          state <= S_H1;
        end

        // Horner, one product per STEP, each an issue and a wait on the
        // shared lane. The three steps are a chain -- H2 multiplies what H1
        // produced -- so there is nothing here to overlap.
        S_H1: state <= S_H1W;
        S_H1W: begin
          if (mul_valid_i) begin
            u_reg     <= resc(tt_c3, 16);
            sat_mul_o <= sat_mul_o || resc_fired(tt_c3, 16);
            state     <= S_H2;
          end
        end

        S_H2: state <= S_H2W;
        S_H2W: begin
          if (mul_valid_i) begin
            u_reg     <= resc(tt_u, 16);
            sat_mul_o <= sat_mul_o || resc_fired(tt_u, 16);
            state     <= S_H3;
          end
        end

        S_H3: state <= S_H3W;
        S_H3W: begin
          if (mul_valid_i) begin
            u_reg     <= resc(tt_v, 16);
            sat_mul_o <= sat_mul_o || resc_fired(tt_v, 16);
            state     <= S_OUT;
          end
        end

        S_OUT: begin
          if (!r_valid_o) begin
            // Law 4: the RAW v halved with a rescale, then added to p1.
            result_o      <= add_sat(p1, half_v);
            sat_rescale_o <= sat_rescale_o || resc_fired(sx(u_reg), 1);
            sat_add_o     <= sat_add_o || add_fired(p1, half_v);
            r_valid_o     <= 1'b1;
          end else if (r_ready_i) begin
            r_valid_o <= 1'b0;
            state     <= S_IDLE;
          end
        end

        default: state <= S_IDLE;
      endcase
    end
  end

endmodule : zhao_field_curve
