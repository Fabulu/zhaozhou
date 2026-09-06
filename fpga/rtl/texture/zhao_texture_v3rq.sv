// zhao_texture_v3rq.sv -- ONE producer-owned ready queue: M10K body, 2-entry
// show-ahead register head.
//
// reports/TEXTURE-ISLAND-V3-ARCHITECTURE-20260906.txt section 9.3:
//
//   > Use three producer-owned ready FIFOs: READY_TMU, READY_AUX,
//   > READY_INITIAL. Each stores a 14-bit owner handle and has one writer and
//   > one reader. Baseline depth is 64 for each. This deliberately spends
//   > three small M10Ks to avoid a multiwrite queue.
//
//   > Do not reduce every FIFO to 21 entries by dividing 64 by three.
//
// So the depth is 64 and there are three of them, and that is a decision the
// document makes on purpose rather than a sizing this file gets to optimise.
//
// ---------------------------------------------------------------------------
// WHY THE HEAD IS TWO REGISTERS AND NOT ZERO
// ---------------------------------------------------------------------------
// The body is a SYNCHRONOUS memory, so its head is not available
// combinationally and a plain `mem[rp]` read would be exactly the
// asynchronous-array pathology this architecture exists to remove. A
// show-ahead head costs 2 x WIDTH flops per queue and buys back the arbiter's
// ability to look at a head at all.
//
// Two, not one: with one, a launch cannot be issued on the same cycle its
// predecessor lands, and the queue tops out at one pop per TWO clocks. Two
// entries let a launch, a landing and a pop overlap, which is one pop per
// clock sustained. The reservation `reserved - pop < 2` is what makes the
// second entry always available for a launch already in flight -- section
// 19.4's "a registered ready without that extra capacity can lose exactly one
// packet at every stall", applied to this queue's own boundary.
//
// ---------------------------------------------------------------------------
// WRITE-ENABLE CONTRACT
// ---------------------------------------------------------------------------
// `wr_en_i` must be a register output. The caller sets ready_claimed on the
// eligibility edge and registers the queue write for the following edge --
// Appendix B.4: "A queue write delayed by a register does not delay the claim
// that prevents a second ticket." That is what keeps the ticket once-only
// while keeping combinational scoreboard logic out of a memory write enable.
// ---------------------------------------------------------------------------
`default_nettype none

module zhao_texture_v3rq #(
    parameter int unsigned WIDTH = 14,
    parameter int unsigned DEPTH = 64,
    parameter int unsigned PW    = $clog2(DEPTH)
) (
    input  var logic             clk,
    input  var logic             rst_n,

    // ---- producer (single writer) -------------------------------------------
    input  var logic             wr_en_i,
    input  var logic [WIDTH-1:0] wr_data_i,
    output var logic             full_o,

    // ---- consumer (single reader), show-ahead --------------------------------
    output var logic             valid_o,
    output var logic [WIDTH-1:0] data_o,
    input  var logic             pop_i,

    // ---- evidence ------------------------------------------------------------
    // TOTAL tickets held, body plus head registers. The drain/quiescence test
    // needs "this queue holds nothing", and a body-only occupancy answers a
    // different question while looking like the right one.
    output var logic [PW:0]      occ_o
);

  logic [PW:0] wp_q, rp_q;
  logic [PW:0] body_occ_c;
  assign body_occ_c = wp_q - rp_q;

  logic ld_c, ld_q;
  logic pop_c;
  logic [WIDTH-1:0] rd_data_c;

  logic             h_v_q, s_v_q;
  logic [WIDTH-1:0] h_d_q, s_d_q;

  assign valid_o = h_v_q;
  assign data_o  = h_d_q;
  assign pop_c   = pop_i && h_v_q;
  assign full_o  = (body_occ_c == (PW+1)'(DEPTH));
  assign occ_o   = body_occ_c + (PW+1)'(h_v_q) + (PW+1)'(s_v_q);

  // Reserved = held in the head registers + one possible in-flight read. The
  // pop that is happening on THIS edge frees an entry, so it is subtracted
  // before the comparison; without that term the queue settles at one pop per
  // two clocks and the throughput loss is invisible in a functional test.
  logic [2:0] reserved_c;
  assign reserved_c = 3'(h_v_q) + 3'(s_v_q) + 3'(ld_q);
  assign ld_c = (body_occ_c != '0) && ((reserved_c - 3'(pop_c)) < 3'd2);

  zhao_texture_v3bank #(.WIDTH(WIDTH), .DEPTH(DEPTH)) u_body (
      .clk      (clk),
      .wr_en_i  (wr_en_i),
      .wr_addr_i(wp_q[PW-1:0]),
      .wr_data_i(wr_data_i),
      .rd_addr_i(rp_q[PW-1:0]),
      .rd_data_o(rd_data_c)
  );

  // ---- head placement ------------------------------------------------------
  logic             n_h_v_c, n_s_v_c;
  logic [WIDTH-1:0] n_h_d_c, n_s_d_c;
  logic             ovf_c;
  always_comb begin
    n_h_v_c = h_v_q;  n_h_d_c = h_d_q;
    n_s_v_c = s_v_q;  n_s_d_c = s_d_q;
    if (pop_c) begin
      n_h_v_c = s_v_q;  n_h_d_c = s_d_q;
      n_s_v_c = 1'b0;
    end
    ovf_c = 1'b0;
    if (ld_q) begin
      if (!n_h_v_c) begin
        n_h_v_c = 1'b1;  n_h_d_c = rd_data_c;
      end else if (!n_s_v_c) begin
        n_s_v_c = 1'b1;  n_s_d_c = rd_data_c;
      end else begin
        ovf_c = 1'b1;
      end
    end
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      wp_q  <= '0;
      rp_q  <= '0;
      ld_q  <= 1'b0;
      h_v_q <= 1'b0;
      s_v_q <= 1'b0;
    end else begin
      if (wr_en_i) wp_q <= wp_q + (PW+1)'(1);
      if (ld_c)    rp_q <= rp_q + (PW+1)'(1);
      ld_q  <= ld_c;
      h_v_q <= n_h_v_c;
      s_v_q <= n_s_v_c;
    end
  end

  // Payload registers take no reset: their use is gated by h_v_q / s_v_q.
  always_ff @(posedge clk) begin
    h_d_q <= n_h_d_c;
    s_d_q <= n_s_d_c;
  end

  // ---- self-asserting guards ----------------------------------------------
  // The sizing claim above ("total tickets can never exceed the 64 live
  // owners") is enforced here rather than left as prose. Both of these are
  // unreachable in a legal workload and that is the point: if a future change
  // makes them reachable, simulation says so instead of the queue silently
  // wrapping.
  logic armed_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) armed_q <= 1'b0;
    else        armed_q <= 1'b1;
  end

  always_ff @(posedge clk) begin
    if (armed_q) begin
      a_rq_no_overflow      : assert (!ovf_c);
      a_rq_no_write_when_full : assert (!(wr_en_i && full_o));
    end
  end

endmodule

`default_nettype wire
