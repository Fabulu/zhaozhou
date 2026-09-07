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
    // THE BODY'S PHYSICAL DEPTH. §5.3: "BDEPTH is a physical body parameter."
    parameter int unsigned DEPTH = 64,
    // THE LOGICAL CAPACITY, which is a different thing and now has its own
    // name. §5.3: "For the ready queues, CAPACITY = 64 means 64 logical owner
    // tickets INCLUDING all heads and pending reads. A body of 64 plus two
    // heads must not silently advertise 66 logical owner credits."
    parameter int unsigned CAPACITY = DEPTH,
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
    output var logic [PW:0]      occ_o,

    // §5.1: "The distinction between !out_valid and owned_empty is
    // fundamental. A queue can have no visible head while a synchronous read
    // is in flight." `valid_o` answers "is there something to consume NOW";
    // this answers "does this queue own anything at all", which is the only
    // question a drain may ask.
    output var logic             owned_empty_o
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

  // ==========================================================================
  // THE REGISTERED LOGICAL CREDIT (§5.3, the brief's THIRD instruction)
  // ==========================================================================
  // WHY THIS REPLACED A SUM. The four-way endpoint split of the owner fit puts
  // the design's worst path at -3.194 ns, and it is
  //
  //     zhao_texture_v3rq:u_rq_tmu|wp_q[0]  ->  adm_accept_o
  //
  // reaching it through this block: wp_q -> body_occ_c -> occ_o ->
  // rq_occ_c == 0 -> quiet_c -> adm_ready_o -> adm_accept_o. Ten paths end at
  // the admission outputs and they are the ten worst in the fit. The brief
  // named that shape by inspection before any of it was measured.
  //
  // §5.2's correctness patch added `ld_q` to the old sum, which was right and
  // which made that sum one term wider on exactly this path. §5.2 said so at
  // the time -- "do not call that the final timing architecture" -- and §5.3
  // is the answer: a count maintained from ACCEPTED TRANSFERS, so the
  // consumer sees a register instead of a subtraction and three additions.
  //
  //     push_taken = in_valid && in_ready;
  //     pop_taken  = out_valid && out_ready;
  //     L_next     = L + push_taken - pop_taken;
  //
  // NO STALENESS IS INTRODUCED, and that is worth stating because a registered
  // occupancy feeding a DRAIN is exactly where a one-cycle lag would be a
  // correctness bug rather than a timing win. `wp_q` and `rp_q` are registers
  // too, so the old `body_occ_c` at cycle N already reflected transfers
  // through N-1. This count has the same visibility, taken from the same
  // edges. It is not fresher and it is not staler.
  //
  // NO SAME-CYCLE FULL/POP BYPASS, per §5.3: "Do not add a same-cycle
  // full/pop bypass unless measured throughput requires it. That bypass
  // connects downstream acceptance back to the producer. Removing one rare
  // full-boundary bubble is not worth reconstructing the timing loop we are
  // trying to remove." `in_ready` is the conservative `!full_o`.
  logic [PW:0] lcnt_q;
  logic [PW:0] lcnt_next_c;

  wire push_taken_c = wr_en_i && !full_o;
  wire pop_taken_c  = pop_c;   // valid_o && pop_i, by construction above

  assign lcnt_next_c = lcnt_q + (PW+1)'(push_taken_c) - (PW+1)'(pop_taken_c);

  // Full and empty come from the LOGICAL count, so heads and the pending read
  // are inside the advertised capacity rather than beyond it.
  assign full_o       = (lcnt_q >= (PW+1)'(CAPACITY));
  assign owned_empty_o = (lcnt_q == '0);
  // OCCUPANCY IS NOW THE REGISTERED LOGICAL COUNT, and the history matters.
  //
  // It was `body_occ_c + h_v_q + s_v_q` -- one term short of its own contract,
  // because `rp_q` advances when a read is ISSUED and the entry then belongs
  // to neither the body nor a head for one cycle. The port's own comment
  // above names that trap exactly. §5.2's correctness patch added `ld_q`; the
  // directed test written for it (one push, no pop, occupancy never zero)
  // failed 1 of 10 checks on the unrepaired block.
  //
  // §5.3 then replaces the whole sum. `lcnt_q` counts accepted pushes minus
  // accepted pops, so every ticket is inside it wherever it physically sits --
  // body, pending read, or either head -- and no future stage can fall out of
  // the accounting the way `ld_q` did. The class of defect is removed, not
  // just its one instance.
  assign occ_o = lcnt_q;

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
      wp_q   <= '0;
      rp_q   <= '0;
      ld_q   <= 1'b0;
      h_v_q  <= 1'b0;
      s_v_q  <= 1'b0;
      lcnt_q <= '0;
    end else begin
      lcnt_q <= lcnt_next_c;
      if (push_taken_c) wp_q <= wp_q + (PW+1)'(1);
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
      // 5.5: "Do not use a saturating occupancy counter: saturation hides
      // over-allocation. A COUNTER CROSSING ITS LEGAL RANGE IS A DESIGN ERROR
      // THAT MUST FIRE A DETECTOR." lcnt_q is not saturating, and this is the
      // detector. It is a REDUNDANCY check -- full_o gates the push and pop_c
      // requires a valid head, so the count is structurally bounded -- which
      // means it fires only if that gating is later broken. That is exactly
      // what it is for.
      a_rq_lcnt_in_range : assert (lcnt_q <= (PW+1)'(CAPACITY));
    end
  end

`ifndef SYNTHESIS
  // ELABORATION-TIME: the queue must not be PARAMETERISED into 5.3's defect.
  //
  // 5.3 names it exactly: "CAPACITY = 64 means 64 logical owner tickets
  // INCLUDING all heads and pending reads. A body of 64 plus two heads must not
  // silently advertise 66 logical owner credits."
  //
  // Until now nothing stopped that. A directed fire test re-elaborated this
  // module with CAPACITY = 66 and the queue cheerfully accepted 66 tickets --
  // the RTL had no guard against being configured into the very defect the
  // section is about, and the only thing that caught it was a test that
  // happened to be looking. A wrong parameter should stop elaboration, not
  // wait for a bench.
  initial begin
    if (CAPACITY > DEPTH) begin
      $fatal(1, "v3rq 5.3: CAPACITY (%0d) exceeds the body DEPTH (%0d) -- the logical capacity counts heads and pending reads INSIDE it, so it can never exceed the body", CAPACITY, DEPTH);
    end
    if (CAPACITY == 0) begin
      $fatal(1, "v3rq: CAPACITY of zero advertises a queue that can never accept");
    end
  end
`endif

endmodule

`default_nettype wire
