// zhao_mem_share2.sv -- N logical requesters (two, historically), ONE permitted MEM.GUARD client.
//
// Law: reports/COMBINE-ASSETFETCH-RECOVERY-20260906.txt 12
//      spec/memory_rules.md 5d / 5f (a client identity is a PRIVILEGE, not a slot)
//
// ===========================================================================
// WHY THIS FILE EXISTS: THE SAME SHAPE CAME UP TWICE
// ===========================================================================
// This block IS `zhao_geom_mem_adapter`'s body, lifted out with the client
// identity made a parameter. Nothing about the arbitration, the ownership
// record or the three acceptance boundaries is geometry-specific; the only two
// geometry-specific lines in the whole file were
//
//     m_req_o.client = ZHAO_CLIENT_ENGINE1;
//     m_req_o.write  = 1'b0;
//
// and both are now `CLIENT_ID` and `FORCE_READ`. `zhao_geom_mem_adapter` is a
// WRAPPER over this module with those two at their old values, so its ports,
// its defaults and its directed test are unchanged.
//
// THE SECOND USER IS TERRAIN, and it is the reason this extraction happened
// rather than a tidy-up looking for a customer. Core entry I26 records that the
// shell exposes exactly ONE guard socket and it is named for GEOM, so the
// terrain compose path's second reader -- TERRAIN.HDRREAD, reading the 64-byte
// patch header -- had nowhere to go beside TERRAIN.PAGESTREAM's lattice reads.
// Writing a second copy of this FSM for it would have been ~200 lines of
// identical arbitration with its own bugs, its own counters and its own drift.
//
// ===========================================================================
// A SPARE SLOT IS NOT A SPARE PRIVILEGE (the original finding, unchanged)
// ===========================================================================
// D22 tread 10 put GEOM.ASSETFETCH on real memory and found GEOM.MESHFETCH
// could not follow it, for a reason that is not about wiring:
//
//   `zhao_vram_arbiter` builds the controller's client tag by CASTING THE SLOT
//   INDEX -- `ctrl_req.client = zhao_client_e'(offer_client)` -- so slot 3 IS
//   ENGINE1 and slot 4 IS DEBUG, positionally. And `zhao_mem_guard` grants each
//   window to ONE named client; everything else falls to
//   `default: pass_ok = 1'b0` and owns nothing.
//
// So a second requester in a subsystem has no client identity that both the
// guard admits and the arbiter carries. The answer is to **share the one
// permitted client**, upstream of the guard, with explicit logical ownership.
// Not a second controller, not a crossbar, and emphatically not a requester
// parked on a spare slot index to be relabelled one level down and refused.
//
//   requester A --+
//                 +--> THIS BLOCK --> CLIENT_ID's guard arm --> its arbiter slot
//   requester B --+
//
// ===========================================================================
// THE THREE ACCEPTANCE BOUNDARIES ARE DIFFERENT EVENTS (12.2)
// ===========================================================================
// Adapter acceptance, guard acceptance/verdict, and arbiter/controller grant
// and data completion are three separate things, and collapsing any two of them
// is how this subsystem has already been wrong twice:
//
//   * A GUARD OK IS NOT RETURNED DATA. The guard answers `ready` as a LEVEL and
//     pulses `ok` the cycle AFTER the accept, so A_VERD is preserved here
//     rather than testing `ready && ok` -- the mistake that made both fetchers
//     read every passing request as a denial.
//     ENFORCED-BY: tools/rtl/check_guard_verdict.py
//   * AN ARBITER GRANT IS NOT A COMPLETED LINE. The arbiter splits a 64-byte
//     line into four 8-word physical bursts and may interleave scanout between
//     them. The logical owner is NOT released on the first credit return.
//
// ===========================================================================
// TWO BURST SCALES, NOT ONE (12.3)
// ===========================================================================
// GEOM.ASSETFETCH asks for 64 bytes -> eight packed 64-bit words.
// GEOM.MESHFETCH  asks for 32 bytes -> four packed 64-bit words.
// TERRAIN's two readers both ask for 64.
//
// The brief is explicit that MESHFETCH's checked 32-byte footprint must NOT be
// normalised up to an unchecked 64-byte read to make this block simpler, and
// that any beat/last generator assuming eight words has to be generalised and
// TESTED at four and eight rather than bypassed. So the expected packed-word
// count is derived from the accepted request's own length and stored, and the
// `last` this block emits upstream is that count -- not a constant.
//
// ===========================================================================
// ONE LOGICAL REQUEST IN FLIGHT, ON PURPOSE (11.3, 12.4)
// ===========================================================================
// "Start with ONE logical memory request in flight. A queue of future requests
//  or a second reserved bank is not multiple outstanding response state. Do not
//  add more outstanding requests until measurement justifies the extra
//  routing/credit complexity."
//
// That single-request rule is what makes the return-routing proof trivial: a
// returning word belongs to the recorded owner because there is exactly one,
// and no later queued request can capture an earlier request's return. The
// counters below exist so the decision to relax it is made against a number.
//
// RELAXED, BY PARAMETER AND ON A NUMBER, 2026-09-19 (owner ruling R38). The
// post lease's ENGINE0 share measured its three requesters queued behind every
// 64-byte read's return (the console smoke's `post census`), and R38 asked for
// a measured second outstanding read. `MAX_RD` reads may now be in flight; the
// routing proof generalises rather than weakens, because the guard, the arbiter
// and the controller are all strictly in order: the returning word belongs to
// the OLDEST read in flight, and a FIFO of recorded owners is that proof with a
// depth. MAX_RD = 1 is the old law cycle for cycle, and every site but the post
// lease keeps it. ENFORCED-BY: tests/memory/mem_share_n_directed.cpp (the old
// law, unchanged) and tests/compositor/post_lease_directed.cpp (MAX_RD > 1).
//
// Conservative SystemVerilog subset only (charter 2).
// ===========================================================================
// N REQUESTERS -- the third ENGINE1 reader (MATERIAL.RESOLVE's record fetch)
// ===========================================================================
// `zhao_console_core`'s material seam 2: the record fetch wants a THIRD ENGINE1
// requester and the geometry adapter had two. The answer is the same one the
// top of this file gives for the second: share the ONE permitted client, never
// park a reader on a spare slot. So the machine above now lives ONCE, in
// `zhao_mem_share_n #(N)`, and `zhao_mem_share2` is its N=2 instance with the
// historical port names -- every existing site, source list and directed test
// is unchanged and now exercises the N core.
//
// EVERY GUARANTEE IS KEPT, and each one is stated for N:
//   * ONE LOGICAL REQUEST IN FLIGHT, owner recorded before issue -- one index
//     register instead of one bit;
//   * THE GUARD'S TWO-CYCLE LAW upstream -- ready a level for the picked
//     requester only, the verdict a pulse to the recorded owner only;
//   * `last` FROM THE ACCEPTED REQUEST'S OWN LENGTH -- four or eight words;
//   * TRUSTED FIXED IDENTITY and FORCE_READ, exactly as before;
//   * NO STARVATION: round-robin at logical-request boundaries. Priority
//     rotates to the index AFTER the last owner, so a requester that is always
//     ready waits at most N-1 other requests. At N=2 this is precisely the old
//     `pick_b = (a && b) ? ~last_b : b` -- the rotation starting after the last
//     owner, reduced to two -- which is why the old test is the N=2 proof.
// `tests/memory/mem_share_n_directed.cpp` proves the N=3 bound.
//
// It shares this file with its N=2 wrapper so that every source list, and
// `tools/rtl/check_guard_verdict.py`'s path-listed audit, keep naming one file.
`default_nettype none

/* verilator lint_off DECLFILENAME */
module zhao_mem_share_n
  import zhao_pkg::*;
#(
    parameter int unsigned N = 3,
    // THE ONE PERMITTED CLIENT, as an INT (see zhao_mem_share2's note: Quartus
    // 17.0 and the arbiter's own `zhao_client_e'(<3 bits>)` idiom).
    parameter int unsigned CLIENT_ID = 3,          // ZHAO_CLIENT_ENGINE1
    parameter bit FORCE_READ = 1'b1,
    // READS IN FLIGHT (owner ruling R38, 2026-09-19). 1 is the original
    // one-logical-request law and every existing site keeps it; the post lease
    // raises it on the evidence of the console smoke's post census.
    parameter int unsigned MAX_RD = 1
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- the logical requesters --------------------------------------------
    input  var zhao_guard_req_t [N-1:0] req_i,
    output var zhao_guard_rsp_t [N-1:0] rsp_o,
    output var logic            [N-1:0] beat_valid_o,
    output var logic            [63:0]  beat_data_o,   // one bus; valid routes it
    output var logic            [N-1:0] beat_last_o,

    // ---- the one permitted client, downstream to MEM.GUARD ----------------
    output var zhao_guard_req_t m_req_o,
    input  var zhao_guard_rsp_t m_rsp_i,
    input  var logic            m_beat_valid_i,
    input  var logic [63:0]     m_beat_data_i,
    input  var logic            m_beat_last_i,

    // ---- evidence ---------------------------------------------------------
    output var logic [N-1:0][31:0] jobs_o,        // logical requests served, per requester
    output var logic [31:0]     denied_o,          // guard violations, any requester
    // Arbitration cycles in which MORE THAN ONE requester asked, so at least
    // one waited. At N=2 this is the old count exactly.
    output var logic [31:0]     contention_o,
    output var logic [31:0]     err_short_o,
    output var logic [31:0]     err_long_o,
    output var logic [31:0]     err_unowned_o
);

  localparam int unsigned IW = (N > 1) ? $clog2(N) : 1;
  localparam int unsigned FW = (MAX_RD > 1) ? $clog2(MAX_RD) : 1;

  initial begin
    if (CLIENT_ID > 7) begin
      $fatal(1, "zhao_mem_share_n: CLIENT_ID %0d is not a zhao_client_e", CLIENT_ID);
    end
    if (N < 2) begin
      $fatal(1, "zhao_mem_share_n: N must be at least 2 (got %0d)", N);
    end
    if ((MAX_RD < 1) || ((MAX_RD & (MAX_RD - 1)) != 0)) begin
      $fatal(1, "zhao_mem_share_n: MAX_RD must be a power of two >= 1 (got %0d)", MAX_RD);
    end
  end

  localparam logic [2:0] CLIENT_BITS = 3'(CLIENT_ID);

  // ------------------------------------------------------------------ FSM --
  // The REQUEST side. A_FILL and A_DRAIN are gone from it (2026-09-19, owner
  // ruling R38): the return side is its own machine below, so that with
  // MAX_RD > 1 the request side can go on to the next request while a read's
  // beats are still coming back. At MAX_RD = 1 the request side waits for the
  // return side exactly where A_FILL/A_DRAIN used to hold it, so every existing
  // site is cycle-for-cycle unchanged (their directed tests are the proof).
  typedef enum logic [1:0] {
    A_IDLE  = 2'd0,  // arbitrate among the requesters allowed to go
    A_REQ   = 2'd1,  // offering the selected request to the guard
    A_VERD  = 2'd2   // the guard's verdict, one cycle after it accepted
  } astate_e;

  astate_e st_q;

  // THE LOGICAL OWNER of the request being offered, recorded BEFORE issue.
  logic [IW-1:0] own_q;
  zhao_guard_req_t sel_q;

  // ROUND-ROBIN AT LOGICAL-REQUEST BOUNDARIES (12.5): the search starts at the
  // index AFTER the last owner and wraps. `last_q` resets to 0, so a first tie
  // goes to requester 1 -- exactly share2's reset `last_b_q = 0`, under which a
  // first A/B tie goes to B.
  logic [IW-1:0] last_q;

  // ---------------------------------------------------- the return side --
  // THE READS IN FLIGHT, IN ORDER. MEM.GUARD, the arbiter and the controller
  // are all strictly in order, so a read's beats come back in the order the
  // guard passed the reads, and a FIFO of {owner, words} pushed at each passed
  // read IS the routing table: a returning word belongs to the head. With
  // MAX_RD = 1 the FIFO has one entry and this is the old single-owner proof
  // verbatim -- "a returning word belongs to the recorded owner because there
  // is exactly one".
  logic [IW-1:0] fl_own [0:MAX_RD-1];
  logic [3:0]    fl_exp [0:MAX_RD-1];
  logic [FW-1:0] fl_wp_q, fl_rp_q;
  logic [FW:0]   fl_n_q;
  logic [3:0]    recv_q;
  logic          drain_q;      // discarding an overlong return through LAST

  // May another READ be issued? Only with room in the table, and never while
  // an overlong return is being discarded (whose tail no entry owns).
  logic rd_room_c;
  assign rd_room_c = (fl_n_q < (FW+1)'(MAX_RD)) && !drain_q;
  // May a WRITE be issued? A write returns no beats, so with MAX_RD > 1 it may
  // pass while reads are in flight -- that is most of what R38 buys: the post
  // lease's two writers no longer wait out every read's return. At MAX_RD = 1
  // it waits exactly as it always did.
  logic wr_room_c;
  assign wr_room_c = (MAX_RD > 1) ? !drain_q : rd_room_c;

  logic [N-1:0] wants;
  logic         any_c;
  logic         many_c;
  logic [IW-1:0] pick_c;
  always_comb begin
    int idx;
    int n_asking;
    for (int i = 0; i < N; i++)
      wants[i] = req_i[i].valid
              && (((!FORCE_READ) && req_i[i].write) ? wr_room_c : rd_room_c);
    any_c = 1'b0;
    pick_c = '0;
    n_asking = 0;
    // Counted over the requesters ALLOWED to go this cycle: at MAX_RD = 1 that is
    // exactly the old A_IDLE-only count (nobody is allowed while a read fills).
    for (int i = 0; i < N; i++) if (wants[i]) n_asking = n_asking + 1;
    many_c = (n_asking > 1);
    for (int k = 1; k <= N; k++) begin
      idx = int'(last_q) + k;
      if (idx >= int'(N)) idx = idx - int'(N);
      if (!any_c && wants[idx]) begin
        any_c = 1'b1;
        pick_c = IW'(idx);
      end
    end
  end

  function automatic logic [3:0] words_of(input logic [6:0] len_bytes);
    words_of = 4'(len_bytes >> 3);
  endfunction

  // ---------------------------------------------------------- upstream rsp --
  // The guard's two-cycle law, per requester: ready is a level, and only for
  // the picked requester, so a losing requester is HELD rather than dropped;
  // the verdict is a pulse, and only for the recorded owner.
  logic rsp_ok_q, rsp_viol_q;

  always_comb begin
    for (int i = 0; i < N; i++) begin
      rsp_o[i] = '0;
      if (st_q == A_IDLE) rsp_o[i].ready = wants[i] && (pick_c == IW'(i));
      if (own_q == IW'(i)) begin
        rsp_o[i].ok        = rsp_ok_q;
        rsp_o[i].violation = rsp_viol_q;
      end
    end
  end

  // ------------------------------------------------------- downstream req --
  always_comb begin
    m_req_o        = sel_q;
    m_req_o.valid  = (st_q == A_REQ);
    m_req_o.client = zhao_client_e'(CLIENT_BITS);
    // ENFORCED-BY: tests/geometry/geom_mem_adapter_directed.cpp:read_only_substitution
    m_req_o.write  = FORCE_READ ? 1'b0 : sel_q.write;
  end

  // ------------------------------------------------------- beat returning --
  // Routed by the HEAD of the in-flight table, never by whoever is currently
  // asking.
  logic          beat_ok_c, last_c;
  logic [IW-1:0] head_own_c;
  logic [3:0]    head_exp_c;
  assign head_own_c = fl_own[fl_rp_q];
  assign head_exp_c = fl_exp[fl_rp_q];
  assign beat_ok_c  = m_beat_valid_i && (fl_n_q != '0) && !drain_q;
  assign last_c     = beat_ok_c && (recv_q + 4'd1 == head_exp_c);
  assign beat_data_o = m_beat_data_i;
  always_comb begin
    for (int i = 0; i < N; i++) begin
      beat_valid_o[i] = beat_ok_c && (head_own_c == IW'(i));
      beat_last_o[i]  = last_c    && (head_own_c == IW'(i));
    end
  end

  // ------------------------------------------------------------- seq core --
  logic push_c, pop_c;
  assign push_c = (st_q == A_VERD) && m_rsp_i.ok && !(!FORCE_READ && sel_q.write);
  assign pop_c  = beat_ok_c && ((recv_q + 4'd1 == head_exp_c) || m_beat_last_i);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_q         <= A_IDLE;
      own_q        <= '0;
      last_q       <= '0;
      sel_q        <= '0;
      rsp_ok_q     <= 1'b0;
      rsp_viol_q   <= 1'b0;
      fl_wp_q      <= '0;
      fl_rp_q      <= '0;
      fl_n_q       <= '0;
      recv_q       <= 4'd0;
      drain_q      <= 1'b0;
      for (int k = 0; k < int'(MAX_RD); k++) begin
        fl_own[k] <= '0;
        fl_exp[k] <= 4'd0;
      end
      jobs_o       <= '0;
      denied_o     <= 32'd0;
      contention_o <= 32'd0;
      err_short_o  <= 32'd0;
      err_long_o   <= 32'd0;
      err_unowned_o <= 32'd0;
    end else begin
      rsp_ok_q   <= 1'b0;      // one-cycle verdict pulses
      rsp_viol_q <= 1'b0;

      // ---- the return side ---------------------------------------------
      if (m_beat_valid_i) begin
        if (drain_q) begin
          if (m_beat_last_i) drain_q <= 1'b0;
        end else if (fl_n_q == '0) begin
          err_unowned_o <= err_unowned_o + 32'd1;
        end else if (recv_q + 4'd1 == head_exp_c) begin
          recv_q <= 4'd0;
          if (!m_beat_last_i) begin
            err_long_o <= err_long_o + 32'd1;
            drain_q    <= 1'b1;
          end
        end else if (m_beat_last_i) begin
          err_short_o <= err_short_o + 32'd1;
          recv_q      <= 4'd0;
        end else begin
          recv_q <= recv_q + 4'd1;
        end
      end
      if (push_c) begin
        fl_own[fl_wp_q] <= own_q;
        fl_exp[fl_wp_q] <= words_of(sel_q.len);
        fl_wp_q         <= (MAX_RD > 1) ? (fl_wp_q + FW'(1)) : '0;
      end
      if (pop_c) fl_rp_q <= (MAX_RD > 1) ? (fl_rp_q + FW'(1)) : '0;
      fl_n_q <= fl_n_q + (push_c ? (FW+1)'(1) : '0) - (pop_c ? (FW+1)'(1) : '0);

      // ---- the request side ---------------------------------------------
      unique case (st_q)
        A_IDLE: begin
          // MORE THAN ONE ASKED AND AT LEAST ONE WAITED.
          if (many_c) contention_o <= contention_o + 32'd1;

          if (any_c) begin
            own_q    <= pick_c;
            last_q   <= pick_c;
            sel_q    <= req_i[pick_c];
            st_q     <= A_REQ;
          end
        end

        A_REQ: begin
          // The guard's ready is a LEVEL; its verdict is the next cycle.
          if (m_rsp_i.ready) st_q <= A_VERD;
        end

        A_VERD: begin
          if (m_rsp_i.ok) begin
            rsp_ok_q <= 1'b1;
            // A passed READ enters the in-flight table (push_c above) and the
            // request side is free at once; whether it may issue again is
            // `rd_room_c` / `wr_room_c`'s business. A WRITE RETURNS NO BEATS
            // (2026-09-19, the ENGINE0 share of POST.COMPOSITE's lease): its
            // data travels on the requester's own write channel, never through
            // here. The ordering of the write DATA across requesters is the
            // integrator's (see `zhao_post_lease`): this block only promises
            // the guard sees one request at a time.
            st_q     <= A_IDLE;
            jobs_o[own_q] <= jobs_o[own_q] + 32'd1;
          end else if (m_rsp_i.violation) begin
            rsp_viol_q <= 1'b1;
            denied_o   <= denied_o + 32'd1;
            st_q       <= A_IDLE;
          end
          // Neither yet: WAIT. Reading silence as an answer is the mistake
          // A_VERD exists to end.
        end
        default: st_q <= A_IDLE;
      endcase
    end
  end

endmodule
/* verilator lint_on DECLFILENAME */

// The two-requester share every existing site instantiates: the N core at N=2,
// with the historical port names. Nothing here decides anything.
module zhao_mem_share2
  import zhao_pkg::*;
#(
    parameter int unsigned CLIENT_ID = 3,          // ZHAO_CLIENT_ENGINE1
    parameter bit FORCE_READ = 1'b1
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- requester A -------------------------------------------------------
    input  var zhao_guard_req_t a_req_i,
    output var zhao_guard_rsp_t a_rsp_o,
    output var logic            a_beat_valid_o,
    output var logic [63:0]     a_beat_data_o,
    output var logic            a_beat_last_o,

    // ---- requester B -------------------------------------------------------
    input  var zhao_guard_req_t b_req_i,
    output var zhao_guard_rsp_t b_rsp_o,
    output var logic            b_beat_valid_o,
    output var logic [63:0]     b_beat_data_o,
    output var logic            b_beat_last_o,

    // ---- the one permitted client, downstream to MEM.GUARD ----------------
    output var zhao_guard_req_t m_req_o,
    input  var zhao_guard_rsp_t m_rsp_i,
    input  var logic            m_beat_valid_i,
    input  var logic [63:0]     m_beat_data_i,
    input  var logic            m_beat_last_i,

    // ---- evidence ---------------------------------------------------------
    output var logic [31:0]     jobs_a_o,
    output var logic [31:0]     jobs_b_o,
    output var logic [31:0]     denied_o,
    output var logic [31:0]     contention_o,
    output var logic [31:0]     err_short_o,
    output var logic [31:0]     err_long_o,
    output var logic [31:0]     err_unowned_o
);

  zhao_guard_req_t [1:0] req;
  zhao_guard_rsp_t [1:0] rsp;
  logic [1:0]       bv, bl;
  logic [63:0]      bd;
  logic [1:0][31:0] jobs;

  assign req[0] = a_req_i;
  assign req[1] = b_req_i;
  assign a_rsp_o = rsp[0];
  assign b_rsp_o = rsp[1];
  assign a_beat_valid_o = bv[0];
  assign b_beat_valid_o = bv[1];
  assign a_beat_last_o  = bl[0];
  assign b_beat_last_o  = bl[1];
  assign a_beat_data_o  = bd;
  assign b_beat_data_o  = bd;
  assign jobs_a_o = jobs[0];
  assign jobs_b_o = jobs[1];

  zhao_mem_share_n #(
    .N         (2),
    .CLIENT_ID (CLIENT_ID),
    .FORCE_READ(FORCE_READ)
  ) u_core (
    .clk           (clk),
    .rst_n         (rst_n),
    .req_i         (req),
    .rsp_o         (rsp),
    .beat_valid_o  (bv),
    .beat_data_o   (bd),
    .beat_last_o   (bl),
    .m_req_o       (m_req_o),
    .m_rsp_i       (m_rsp_i),
    .m_beat_valid_i(m_beat_valid_i),
    .m_beat_data_i (m_beat_data_i),
    .m_beat_last_i (m_beat_last_i),
    .jobs_o        (jobs),
    .denied_o      (denied_o),
    .contention_o  (contention_o),
    .err_short_o   (err_short_o),
    .err_long_o    (err_long_o),
    .err_unowned_o (err_unowned_o)
  );

endmodule

`default_nettype wire
