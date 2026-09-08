`default_nettype none
// zhao_raster_perspuv_pairpipe.sv — the paired PERSPUV candidate, §8.2.
//
// ---------------------------------------------------------------------------
// WHAT THIS DELETES, AND WHAT LICENSES DELETING IT
// ---------------------------------------------------------------------------
// `zhao_raster_perspuv_svc` carries TWO of everything on the control side: two
// work queues, two write pointers, two read pointers, two emptiness tests, two
// token selects. It then reassembles the axes at the far end through a
// sixteen-entry table with a per-axis `e_have` join and two result tables
// `e_q_u`/`e_q_v`.
//
// None of that duplication does anything. `reports/PERSPUV-AXIS-LOCKSTEP-PROOF-
// 20260908.md` proves by induction that the two schedulers are bit-identical for
// all time -- there are exactly four assignments to `wq_wp`/`wq_rp`, the push
// guard has no `ax` dependence, and the pop guard is equal across axes whenever
// the invariant holds -- and `tests/raster/perspuv_lockstep_directed.cpp`
// asserts it every cycle against the elaborated RTL, with depth-zero fragments
// in the mix and a live-probe control so its zeros mean something.
//
// So: ONE transaction in, TWO arithmetic lanes, ONE paired record out.
//
// What the proof does NOT license is merging the lanes. The two axes compute
// different numerators against a shared mantissa and saturate independently, so
// there are still two multipliers, two rounding adds, two shifts and two
// saturation tests. Serializing them would halve the throughput to buy nothing.
//
// The join disappears because there is nothing left to join: the pair travels
// as ONE item through one pipeline, so `e_have`, `e_q_u`, `e_q_v` and the
// sixteen-entry operand tables have no reason to exist.
//
// ---------------------------------------------------------------------------
// CREDITS ARE RESERVED AT ACCEPTANCE AND RELEASED AT EXTERNAL ACCEPTANCE
// ---------------------------------------------------------------------------
// Not at pipeline writeback. Releasing at writeback is the cache's documented
// lost-response bug: the producer is told there is room while the item is still
// occupying the terminal storage, and a long output stall then overruns it.
//
// `owned_q` counts everything accepted and not yet emitted -- items in the five
// pipeline stages, items in the terminal FIFO, and the item in the output
// register. `CAP` is the ceiling, and the FIFO is sized so that everything owned
// has somewhere to land: at most CAP-1 can be waiting behind the output stage,
// so FIFO depth CAP-1 can never overflow. That is a capacity ARGUMENT, and
// `perspuv_pairpipe_directed` measures the burst ceiling rather than trusting it.
//
// ---------------------------------------------------------------------------
// THE ARITHMETIC IS COPIED, NOT REDERIVED
// ---------------------------------------------------------------------------
// Every expression below is `zhao_raster_perspuv_svc.sv`'s, verbatim, with the
// per-axis array index replaced by an explicit lane. Same 64-bit widths, same
// round-half-up constant, same variable arithmetic shift, same saturation
// bounds and the same wrap behaviour of `sh = 6'd32 - k`.
//
// Copying is deliberate. §8.6 forbids narrowing the rescale to 56 bits, and the
// reason is a directed counterexample at k = 32, where `sh - 1` wraps to 63 and
// a narrower intermediate turns a zero product into a negative saturated result.
// Retyping the arithmetic "more cleanly" is how that comes back.
module zhao_raster_perspuv_pairpipe #(
    parameter int unsigned NTOK = 16,
    parameter int unsigned TAGW = 16
) (
    input var logic clk,
    input var logic rst_n,

    // ---- one PAIRED transaction in ------------------------------------------
    input  var logic               v_valid_i,
    output var logic               v_ready_o,
    input  var logic signed [31:0] u_over_w_i,   // S 8.24
    input  var logic signed [31:0] v_over_w_i,   // S 8.24
    input  var logic        [23:0] r_mant_i,     // reciprocal mantissa
    input  var logic        [ 5:0] r_k_i,        // reciprocal exponent
    input  var logic               depth_zero_i, // invw24 == 0, a caller bug
    input  var logic [TAGW-1:0]    tag_i,

    // ---- one PAIRED result out ----------------------------------------------
    output var logic               r_valid_o,
    input  var logic               r_ready_i,
    output var logic signed [31:0] u_o,          // S 15.16
    output var logic signed [31:0] v_o,
    output var logic [TAGW-1:0]    tag_o,
    output var logic               sat_o,
    output var logic               depth_zero_o,

    // ---- instruments ---------------------------------------------------------
    output var logic [31:0]        fragments_o,
    output var logic [31:0]        products_o,      // multiplier launches
    output var logic [31:0]        zero_products_o, // depth-zero, no product
    output var logic [4:0]         occupancy_o
);

  // Everything owned: pipeline + FIFO + the held output register.
  localparam int unsigned CAP   = NTOK + 1;   // 17
  localparam int unsigned FIFOD = NTOK;       // 16, a power of two
  localparam int unsigned FW    = $clog2(FIFOD);

  // ------------------------------------------------------------- credits ----
  logic [4:0] owned_q;
  wire accept_c = v_valid_i && (owned_q < 5'(CAP));
  assign v_ready_o = (owned_q < 5'(CAP));
  assign occupancy_o = owned_q;

  wire out_fire_c = r_valid_o && r_ready_i;

  // ---------------------------------------------------------------- P0 ------
  // Operands only. No multiplier input is an array read, which is the whole
  // content of the QUARTUS_GOTCHAS 14 repair the service already carries.
  logic               p0_v_q;
  logic signed [31:0] p0_nu_q, p0_nv_q;
  logic [23:0]        p0_mant_q;
  logic [5:0]         p0_k_q;
  logic [TAGW-1:0]    p0_tag_q;
  logic               p0_dz_q;

  // ---------------------------------------------------------------- P1 ------
  logic               p1_v_q;
  logic signed [63:0] p1_pu_q, p1_pv_q;
  logic [5:0]         p1_k_q;
  logic [TAGW-1:0]    p1_tag_q;
  logic               p1_dz_q;

  // ---------------------------------------------------------------- P2 ------
  logic               p2_v_q;
  logic signed [63:0] p2_su_q, p2_sv_q;
  logic [5:0]         p2_sh_q;
  logic [TAGW-1:0]    p2_tag_q;
  logic               p2_dz_q;

  // ---------------------------------------------------------------- P3 ------
  logic               p3_v_q;
  logic signed [63:0] p3_ru_q, p3_rv_q;
  logic [TAGW-1:0]    p3_tag_q;
  logic               p3_dz_q;

  // ---- combinational pieces, each between two registers --------------------
  // Identical to the service's `sh_c`/`sum_c`/`resc_c`/`sat_c`/`q_c`, unrolled
  // per lane instead of indexed by `ax`.
  wire [5:0]         sh_c  = 6'd32 - p1_k_q;
  wire signed [63:0] rnd_c = $signed(64'd1 <<< (sh_c - 6'd1));
  wire signed [63:0] sum_u_c = $signed(p1_pu_q) + rnd_c;
  wire signed [63:0] sum_v_c = $signed(p1_pv_q) + rnd_c;

  wire signed [63:0] resc_u_c = $signed(p2_su_q) >>> p2_sh_q;
  wire signed [63:0] resc_v_c = $signed(p2_sv_q) >>> p2_sh_q;

  wire sat_u_c = (p3_ru_q > 64'sh0000_0000_7FFF_FFFF)
              || (p3_ru_q < -64'sh0000_0000_8000_0000);
  wire sat_v_c = (p3_rv_q > 64'sh0000_0000_7FFF_FFFF)
              || (p3_rv_q < -64'sh0000_0000_8000_0000);

  wire signed [31:0] q_u_c = sat_u_c ? (p3_ru_q[63] ? 32'sh8000_0000
                                                    : 32'sh7FFF_FFFF)
                                     : p3_ru_q[31:0];
  wire signed [31:0] q_v_c = sat_v_c ? (p3_rv_q[63] ? 32'sh8000_0000
                                                    : 32'sh7FFF_FFFF)
                                     : p3_rv_q[31:0];

  // ---- the terminal FIFO ---------------------------------------------------
  // One paired record per entry. Nothing here is per-axis, which is the point.
  //
  // A DEPTH-ZERO fragment travels the SAME ordered path with its flag carried,
  // rather than taking a bypass. A bypass would let a zero overtake a nonzero
  // and silently reorder the interface, and interface order IS the contract --
  // v3own restores fragment order downstream, so there is no reorder buffer to
  // rescue it here.
  //
  // Its U/V are emitted as deterministic ZERO. The service leaves them as
  // whatever the previous user of that token wrote, because a depth-zero
  // fragment sets `e_have` at allocation and never writes `e_q_u`/`e_q_v` --
  // stale by contract rather than by accident, since the caller is told to read
  // `depth_zero_o` first. Deterministic zero is a strict improvement and it is
  // the one intentional behavioural difference from the service; the
  // differential excludes U/V on depth-zero results and says so.
  logic signed [31:0] fifo_u_q [FIFOD];
  logic signed [31:0] fifo_v_q [FIFOD];
  logic [TAGW-1:0]    fifo_tag_q [FIFOD];
  logic               fifo_sat_q [FIFOD];
  logic               fifo_dz_q [FIFOD];
  logic [FW:0]        fifo_wp_q, fifo_rp_q;

  wire fifo_empty_c = (fifo_wp_q == fifo_rp_q);

  // ---- the held output register -------------------------------------------
  // It holds the COMPLETE packet while stalled. D0's counterexample is the
  // checklist here: a stage that holds some fields and lets others track the
  // next item produces a record whose parts never belonged together, and every
  // accepted/emitted counter still balances.
  logic               o_v_q;
  logic signed [31:0] o_u_q, o_v_val_q;
  logic [TAGW-1:0]    o_tag_q;
  logic               o_sat_q, o_dz_q;

  wire o_room_c = !o_v_q || r_ready_i;
  wire fifo_pop_c = o_room_c && !fifo_empty_c;

  assign r_valid_o    = o_v_q;
  assign u_o          = o_u_q;
  assign v_o          = o_v_val_q;
  assign tag_o        = o_tag_q;
  assign sat_o        = o_sat_q;
  assign depth_zero_o = o_dz_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      p0_v_q          <= 1'b0;
      p1_v_q          <= 1'b0;
      p2_v_q          <= 1'b0;
      p3_v_q          <= 1'b0;
      o_v_q           <= 1'b0;
      fifo_wp_q       <= '0;
      fifo_rp_q       <= '0;
      owned_q         <= 5'd0;
      fragments_o     <= 32'd0;
      products_o      <= 32'd0;
      zero_products_o <= 32'd0;
    end else begin
      // ---- credits: reserved on acceptance, released on EXTERNAL emit ------
      // Both can happen on the same clock, so the update is ONE expression
      // rather than two branches. Two branches is the lost-update fault the
      // service's own launch comment describes.
      owned_q <= owned_q + (accept_c ? 5'd1 : 5'd0) - (out_fire_c ? 5'd1 : 5'd0);

      // ---- P0: accept -------------------------------------------------------
      p0_v_q <= accept_c;
      if (accept_c) begin
        p0_nu_q   <= u_over_w_i;
        p0_nv_q   <= v_over_w_i;
        p0_mant_q <= r_mant_i;
        p0_k_q    <= r_k_i;
        p0_tag_q  <= tag_i;
        p0_dz_q   <= depth_zero_i;
        fragments_o <= fragments_o + 32'd1;
        if (depth_zero_i) zero_products_o <= zero_products_o + 32'd1;
        else              products_o      <= products_o + 32'd2;  // two lanes
      end

      // ---- P1: TWO products, one clock -------------------------------------
      p1_v_q <= p0_v_q;
      if (p0_v_q) begin
        p1_pu_q  <= 64'(p0_nu_q) * $signed({40'd0, p0_mant_q});
        p1_pv_q  <= 64'(p0_nv_q) * $signed({40'd0, p0_mant_q});
        p1_k_q   <= p0_k_q;
        p1_tag_q <= p0_tag_q;
        p1_dz_q  <= p0_dz_q;
      end

      // ---- P2: + the rounding constant --------------------------------------
      p2_v_q <= p1_v_q;
      if (p1_v_q) begin
        p2_su_q  <= sum_u_c;
        p2_sv_q  <= sum_v_c;
        p2_sh_q  <= sh_c;
        p2_tag_q <= p1_tag_q;
        p2_dz_q  <= p1_dz_q;
      end

      // ---- P3: the variable shift -------------------------------------------
      p3_v_q <= p2_v_q;
      if (p2_v_q) begin
        p3_ru_q  <= resc_u_c;
        p3_rv_q  <= resc_v_c;
        p3_tag_q <= p2_tag_q;
        p3_dz_q  <= p2_dz_q;
      end

      // ---- P4: saturate, pair, push -----------------------------------------
      // ONE record. `sat` is the OR of the two lanes, written once -- the
      // service writes its single `e_sat` bit from an OR for exactly this
      // reason, and two branches there would silently drop a saturation flag.
      if (p3_v_q) begin
        fifo_u_q  [fifo_wp_q[FW-1:0]] <= p3_dz_q ? 32'sd0 : q_u_c;
        fifo_v_q  [fifo_wp_q[FW-1:0]] <= p3_dz_q ? 32'sd0 : q_v_c;
        fifo_tag_q[fifo_wp_q[FW-1:0]] <= p3_tag_q;
        fifo_sat_q[fifo_wp_q[FW-1:0]] <= !p3_dz_q && (sat_u_c || sat_v_c);
        fifo_dz_q [fifo_wp_q[FW-1:0]] <= p3_dz_q;
        fifo_wp_q <= fifo_wp_q + (FW+1)'(1);
      end

      // ---- output stage -----------------------------------------------------
      if (o_room_c) begin
        o_v_q <= !fifo_empty_c;
        if (!fifo_empty_c) begin
          o_u_q     <= fifo_u_q  [fifo_rp_q[FW-1:0]];
          o_v_val_q <= fifo_v_q  [fifo_rp_q[FW-1:0]];
          o_tag_q   <= fifo_tag_q[fifo_rp_q[FW-1:0]];
          o_sat_q   <= fifo_sat_q[fifo_rp_q[FW-1:0]];
          o_dz_q    <= fifo_dz_q [fifo_rp_q[FW-1:0]];
        end
      end
      if (fifo_pop_c) fifo_rp_q <= fifo_rp_q + (FW+1)'(1);
    end
  end

endmodule : zhao_raster_perspuv_pairpipe

`default_nettype wire
