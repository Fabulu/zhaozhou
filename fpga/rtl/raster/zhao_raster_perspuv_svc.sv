// zhao_raster_perspuv_svc.sv — the perspective multiply lane, one product a clock.
//
// BESIDE `zhao_raster_perspuv.sv`, which stays the golden implementation.
// Nothing instantiates this yet.
//
// ---------------------------------------------------------------------------
// WHY
// ---------------------------------------------------------------------------
// REARCHITECTUREADVICE.md: "zhao_raster_perspuv then walks U and V through one
// shared multiplier while refusing another fragment." Two dependent products
// per fragment, and the block will not take another until both are done.
//
// The brief's replacement:
//
//   U0  enqueue U product / U1  multiply / U2  rescale, saturate, store
//   V0  enqueue V product / V1  multiply / V2  rescale, saturate, store
//   > One product launches per cycle. Two are needed per fragment, so this lane
//   > can support one completed perspective pair every two cycles -- well above
//   > the reciprocal engine's one-per-four rate.
//   > A direct-index table stores U and V by token and emits the perspective
//   > result once both have arrived.
//
// That last sentence is the design. U and V are INDEPENDENT given the
// reciprocal, so they never needed to be serial with each other -- only with
// the reciprocal that feeds them.
//
// ---------------------------------------------------------------------------
// SCOPE, STATED SO IT IS NOT MISTAKEN FOR MORE
// ---------------------------------------------------------------------------
// This is the MULTIPLY LANE only. It consumes a reciprocal result (mantissa and
// exponent) rather than instantiating a divider, where `zhao_raster_perspuv`
// contains its own `zhao_raster_rcp24`. Wiring this to `zhao_raster_rcp24_svc`
// is a separate integration step and a separate fit.
//
// ---------------------------------------------------------------------------
// THE ARITHMETIC IS TRANSCRIBED, NOT REDERIVED
// ---------------------------------------------------------------------------
// From `zhao_raster_perspuv.sv`, character for character:
//
//     prod = 64'(num) * 64'({40'd0, mant})
//     sh   = 32 - k
//     resc = (prod + (1 <<< (sh-1))) >>> sh          arithmetic, round-half-up
//     sat  = resc > 2^31-1 || resc < -2^31
//     q    = sat ? (resc<0 ? -2^31 : 2^31-1) : resc[31:0]
//
// The variable shift is the expensive part and it is kept variable. Replacing
// it with a fixed shift plus a correction would be a different function, and
// the gate that would catch that is an exponent boundary nobody thinks to
// write.
// ---------------------------------------------------------------------------
`default_nettype none

module zhao_raster_perspuv_svc #(
    parameter int unsigned NTOK = 16,
    parameter int unsigned TAGW = 16
) (
    input var logic clk,
    input var logic rst_n,

    // ---- a fragment whose reciprocal has already returned --------------------
    input  var logic               v_valid_i,
    output var logic               v_ready_o,
    input  var logic signed [31:0] u_over_w_i,   // S 8.24
    input  var logic signed [31:0] v_over_w_i,   // S 8.24
    input  var logic        [23:0] r_mant_i,     // reciprocal mantissa
    input  var logic        [ 5:0] r_k_i,        // reciprocal exponent
    input  var logic               depth_zero_i, // invw24 == 0, a caller bug
    input  var logic        [TAGW-1:0] tag_i,

    // ---- the perspective pair, in completion order --------------------------
    output var logic               r_valid_o,
    input  var logic               r_ready_i,
    output var logic signed [31:0] u_o,          // S 15.16
    output var logic signed [31:0] v_o,
    output var logic        [TAGW-1:0] tag_o,
    output var logic               sat_o,
    output var logic               depth_zero_o,

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0]        fragments_o,
    output var logic [31:0]        products_o,   // multiplier launches
    output var logic [3:0]         occupancy_o
);

  localparam int TW = $clog2(NTOK);

  // ------------------------------------------------------------- entries ----
  logic                  e_val  [NTOK];
  logic [1:0]            e_need [NTOK];  // bit0 = U outstanding, bit1 = V
  logic [1:0]            e_have [NTOK];
  logic signed [31:0]    e_num  [NTOK][2];
  logic [23:0]           e_mant [NTOK];
  logic [5:0]            e_k    [NTOK];
  logic signed [31:0]    e_q    [NTOK][2];
  logic                  e_sat  [NTOK];
  logic                  e_dz   [NTOK];
  logic [TAGW-1:0]       e_tag  [NTOK];

  logic [TW-1:0] head_q, tail_q;
  logic [TW:0]   free_cnt_q;

  // Ready from LOCAL storage, never from the consumer. Same property the
  // reciprocal service and TEXJOIN v2 rest on.
  assign v_ready_o = (free_cnt_q != '0);

  // -------------------------------------------------- product selection -----
  // One launch per clock, walking from the retire head so the oldest fragment
  // finishes first and the table does not fill with half-done pairs.
  logic          pick_v;
  logic [TW-1:0] pick_i;
  logic          pick_axis;   // 0 = U, 1 = V
  always_comb begin
    pick_v    = 1'b0;
    pick_i    = head_q;
    pick_axis = 1'b0;
    for (int n = 0; n < NTOK; n++) begin
      automatic logic [TW-1:0] s = TW'((int'(head_q) + n) % NTOK);
      if (!pick_v && e_val[s]) begin
        if (e_need[s][0]) begin
          pick_v = 1'b1; pick_i = s; pick_axis = 1'b0;
        end else if (e_need[s][1]) begin
          pick_v = 1'b1; pick_i = s; pick_axis = 1'b1;
        end
      end
    end
  end

  // ---- P1: the registered product ------------------------------------------
  logic               p1_v_q;
  logic [TW-1:0]      p1_i_q;
  logic               p1_ax_q;
  logic signed [63:0] p1_prod_q;
  logic [5:0]         p1_k_q;

  // ---- P2: rescale and saturate, exactly the original's expression ---------
  logic [5:0]         sh_c;
  logic signed [63:0] resc_c;
  logic               sat_c;
  logic signed [31:0] q_c;
  always_comb begin
    sh_c   = 6'd32 - p1_k_q;
    resc_c = ($signed(p1_prod_q) + $signed(64'd1 <<< (sh_c - 6'd1))) >>> sh_c;
    sat_c  = (resc_c > 64'sh0000_0000_7FFF_FFFF) || (resc_c < -64'sh0000_0000_8000_0000);
    q_c    = sat_c ? (resc_c[63] ? 32'sh8000_0000 : 32'sh7FFF_FFFF) : resc_c[31:0];
  end

  // ---------------------------------------------------------- retirement ----
  // Allocation order. A pair is done when BOTH axes have landed.
  logic head_done;
  assign head_done = e_val[head_q] && (e_have[head_q] == 2'b11);

  assign r_valid_o    = head_done;
  assign u_o          = e_q[head_q][0];
  assign v_o          = e_q[head_q][1];
  assign tag_o        = e_tag[head_q];
  assign sat_o        = e_sat[head_q];
  assign depth_zero_o = e_dz[head_q];

  always_comb begin
    occupancy_o = 4'd0;
    for (int i = 0; i < NTOK; i++) occupancy_o = occupancy_o + 4'(e_val[i]);
  end

  // ------------------------------------------------------------ sequential --
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      head_q      <= '0;
      tail_q      <= '0;
      free_cnt_q  <= (TW + 1)'(NTOK);
      p1_v_q      <= 1'b0;
      fragments_o <= 32'd0;
      products_o  <= 32'd0;
      for (int i = 0; i < NTOK; i++) begin
        e_val[i]  <= 1'b0;
        e_need[i] <= 2'b00;
        e_have[i] <= 2'b00;
      end
    end else begin
      // ---- the occupancy counter moves ONCE ------------------------------
      // Accept and retire can happen in the SAME clock, and two non-blocking
      // assignments to free_cnt_q in one always_ff do not add up -- the last
      // one wins and the other is silently lost. That produced a count which
      // grew without bound, v_ready_o stuck high, and tail_q wrapping over
      // live entries: the lane answered 193 of 335 fragments and then hung.
      //
      // Computing the NET delta once is the fix. The first draft had the two
      // increments in their respective branches, which reads correctly and is
      // wrong, and only a test that stalled the output found it -- with
      // r_ready_i always high the two events never coincide.
      begin
        automatic logic acc = v_valid_i && v_ready_o;
        automatic logic ret = head_done && r_ready_i;
        if (acc && !ret)      free_cnt_q <= free_cnt_q - 1'b1;
        else if (!acc && ret) free_cnt_q <= free_cnt_q + 1'b1;
      end

      // ---- accept ---------------------------------------------------------
      if (v_valid_i && v_ready_o) begin
        e_val[tail_q]     <= 1'b1;
        // A depth-zero fragment is a caller bug the original flags rather than
        // computes. It still occupies a slot and still retires, so the caller
        // sees one answer per request -- it simply produces no product.
        e_need[tail_q]    <= depth_zero_i ? 2'b00 : 2'b11;
        e_have[tail_q]    <= depth_zero_i ? 2'b11 : 2'b00;
        e_num[tail_q][0]  <= u_over_w_i;
        e_num[tail_q][1]  <= v_over_w_i;
        e_mant[tail_q]    <= r_mant_i;
        e_k[tail_q]       <= r_k_i;
        e_q[tail_q][0]    <= 32'sd0;
        e_q[tail_q][1]    <= 32'sd0;
        e_sat[tail_q]     <= 1'b0;
        e_dz[tail_q]      <= depth_zero_i;
        e_tag[tail_q]     <= tag_i;
        tail_q            <= (tail_q == TW'(NTOK - 1)) ? '0 : tail_q + TW'(1);
        fragments_o       <= fragments_o + 32'd1;
      end

      // ---- P0 launch ------------------------------------------------------
      p1_v_q <= pick_v;
      if (pick_v) begin
        p1_i_q    <= pick_i;
        p1_ax_q   <= pick_axis;
        p1_k_q    <= e_k[pick_i];
        p1_prod_q <= 64'(e_num[pick_i][pick_axis]) * $signed({40'd0, e_mant[pick_i]});
        e_need[pick_i][pick_axis] <= 1'b0;
        products_o <= products_o + 32'd1;
      end

      // ---- P2 writeback ---------------------------------------------------
      if (p1_v_q) begin
        e_q[p1_i_q][p1_ax_q]   <= q_c;
        e_have[p1_i_q][p1_ax_q] <= 1'b1;
        if (sat_c) e_sat[p1_i_q] <= 1'b1;
      end

      // ---- retire ---------------------------------------------------------
      if (head_done && r_ready_i) begin
        e_val[head_q] <= 1'b0;
        head_q     <= (head_q == TW'(NTOK - 1)) ? '0 : head_q + TW'(1);
      end
    end
  end

endmodule : zhao_raster_perspuv_svc

`default_nettype wire
