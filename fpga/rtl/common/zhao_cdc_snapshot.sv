// zhao_cdc_snapshot.sv — one-entry asynchronous snapshot mailbox.
//
// ---------------------------------------------------------------------------
// WHY THIS EXISTS
// ---------------------------------------------------------------------------
// A wide value produced in one clock domain, read once per frame in another.
// The shell had been doing this by sampling the whole 64-bit bus directly:
//
//     starve_samp <= starvation_o;                                  // gpu_clk
//     if (tick_d1 && (starvation_o != starve_samp)) cdc_err <= 1'b1;
//
// with a runtime tripwire on the premise that the source is quiescent in the
// sample window. The premise is reasonable. The structure is not: a value
// COMPARISON can detect that a bus moved, it cannot make a metastable sample
// safe, and it says nothing about the 62 bits that did not move.
//
// MEASURED, and this is why it changed. Across four composed fits that touched
// NOTHING in this path, the crossing's hold slack read:
//
//     -0.952 FAIL   +0.254 pass   +0.259 pass   -0.728 FAIL
//
// 1.2 ns of swing on placement alone, which made the whole shell's timing
// verdict NONDETERMINISTIC -- worse than permanently red, because a flag that
// flips at random cannot be trusted in either direction and a real regression
// arriving on a lucky fit is indistinguishable from luck.
//
// ---------------------------------------------------------------------------
// THE PROTOCOL: bundled data behind a toggle handshake
// ---------------------------------------------------------------------------
//   1. the source latches `d_i` into `hold` and TOGGLES `req` -- but only when
//      `req == ack`, i.e. the previous snapshot was collected;
//   2. `req` crosses on a THREE-flop synchronizer. Only this one bit crosses
//      asynchronously; it is the only signal that can go metastable;
//   3. the destination sees the toggle change and captures `hold`, which by
//      then has been STABLE for at least two destination clocks plus however
//      long the source waited -- vastly longer than any routing skew;
//   4. the destination toggles `ack`, which crosses back on its own three-flop
//      synchronizer, releasing the source to publish again.
//
// So the wide bus is never sampled while it can be changing. That is the whole
// difference: the old design sampled a live counter and checked afterwards
// whether it had moved; this one only ever samples a value the source has
// promised not to touch.
//
// `ovf_o` is a STICKY error: the source wanted to publish while the previous
// snapshot was still uncollected. That is a real, meaningful condition -- the
// destination is not draining -- unlike "the counter moved while I looked at
// it", which was a statement about luck.
//
// ---------------------------------------------------------------------------
// WHAT THIS DOES NOT DO
// ---------------------------------------------------------------------------
// It does not make the crossing free of constraint work: `req_meta`/`ack_meta`
// and the `hold` bus still need named timing exceptions, and `hold` still wants
// a max-skew or net-delay bound. The RTL makes those exceptions LAWFUL rather
// than a way of hiding a path -- which is the distinction between constraining
// a synchronizer and false-pathing a real one.
// ---------------------------------------------------------------------------

module zhao_cdc_snapshot #(
    parameter int unsigned W = 64
) (
    input  logic         src_clk,
    input  logic         src_rst_n,
    input  logic         src_publish_i,   // 1-cycle request to publish d_i
    input  logic [W-1:0] d_i,
    output logic         src_busy_o,      // previous snapshot not yet collected
    output logic         ovf_o,           // sticky: publish while busy

    input  logic         dst_clk,
    input  logic         dst_rst_n,
    output logic [W-1:0] q_o,             // last collected snapshot
    output logic         q_valid_o        // at least one snapshot collected
);

  // Declared ahead of both domains: `read_slang` (the formal frontend)
  // enforces declaration-before-use where Verilator does not, and this pair of
  // toggles is by nature referenced from BOTH sides. Ordering only.
  logic req_tog, ack_tog;

  // ---- source domain ------------------------------------------------------
  logic [W-1:0] hold;
  logic         ack_meta, ack_sync, ack_q;

  // Three flops, not two. The first is the metastability catcher and its
  // output is NEVER used for logic -- only the second and third are read.
  always_ff @(posedge src_clk or negedge src_rst_n) begin
    if (!src_rst_n) begin
      ack_meta <= 1'b0;
      ack_sync <= 1'b0;
      ack_q    <= 1'b0;
    end else begin
      ack_meta <= ack_tog;
      ack_sync <= ack_meta;
      ack_q    <= ack_sync;
    end
  end

  assign src_busy_o = (req_tog != ack_q);

  always_ff @(posedge src_clk or negedge src_rst_n) begin
    if (!src_rst_n) begin
      hold    <= '0;
      req_tog <= 1'b0;
      ovf_o   <= 1'b0;
    end else if (src_publish_i) begin
      if (!src_busy_o) begin
        // The value is captured BEFORE the toggle flips, so the destination
        // cannot observe a toggle whose data has not landed.
        hold    <= d_i;
        req_tog <= ~req_tog;
      end else begin
        ovf_o <= 1'b1;   // sticky, deliberately: one lost snapshot is a fault
      end
    end
  end

  // ---- destination domain -------------------------------------------------
  logic req_meta, req_sync, req_q;

  always_ff @(posedge dst_clk or negedge dst_rst_n) begin
    if (!dst_rst_n) begin
      req_meta <= 1'b0;
      req_sync <= 1'b0;
      req_q    <= 1'b0;
    end else begin
      req_meta <= req_tog;
      req_sync <= req_meta;
      req_q    <= req_sync;
    end
  end

  wire req_edge = (req_sync != req_q);

  always_ff @(posedge dst_clk or negedge dst_rst_n) begin
    if (!dst_rst_n) begin
      q_o       <= '0;
      q_valid_o <= 1'b0;
      ack_tog   <= 1'b0;
    end else if (req_edge) begin
      // `hold` is stable here because the source may not touch it while
      // req != ack, which is asserted rather than asserted-about:
      // ENFORCED-BY: fpga/rtl/common/zhao_cdc_snapshot.sv:a_hold_stable_while_busy
      q_o       <= hold;
      q_valid_o <= 1'b1;
      ack_tog   <= req_sync;  // acknowledge exactly the request consumed
    end
  end


`ifdef FORMAL
  // -------------------------------------------------------------------------
  // THE PROTOCOL PROPERTIES
  // -------------------------------------------------------------------------
  // Written as IMMEDIATE assertions inside clocked blocks, matching
  // zhao_debug_frameblit. Concurrent SVA (`assert property`, `disable iff`)
  // is rejected by read_slang, the formal frontend -- "encountered unsupported
  // SVA feature" on every one of them.
  //
  // WHAT THIS PROVES, and what it does not, stated first because a CDC proof
  // that overstates itself is worse than no proof:
  //
  //   PROVEN: the handshake cannot lose, duplicate or fabricate a snapshot,
  //   and the source cannot disturb the held value while the destination may
  //   still be sampling it. That is precisely what a value-comparison tripwire
  //   could never establish, and it is why this module replaces one.
  //
  //   NOT PROVEN -- METASTABILITY. No formal tool establishes it. That is the
  //   three-flop synchronizer's job and a settling-time argument, not a
  //   logical one.
  //
  //   NOT PROVEN -- DIFFERING CLOCK RATES. Both domains are driven from one
  //   clock here, so the synchronizers degrade to plain delays. The rate case,
  //   especially a destination slower than the publish rate (which is what
  //   `ovf_o` exists for), needs a simulation test at randomised relative
  //   phase. A REAL GAP, recorded as one rather than implied away.
  logic f_past_valid;
  initial f_past_valid = 1'b0;
  always_ff @(posedge src_clk) f_past_valid <= 1'b1;

  always_ff @(posedge src_clk) begin
    if (f_past_valid && src_rst_n && $past(src_rst_n)) begin
      // 1. THE SOURCE MAY NOT TOUCH `hold` WHILE A SNAPSHOT IS UNCOLLECTED.
      //    The whole safety argument: the destination samples `hold`
      //    asynchronously, so it must be stable from toggle until ack.
      if ($past(src_busy_o) && src_busy_o)
        a_hold_stable_while_busy: assert (hold == $past(hold));

      // 2. A PUBLISH WHILE BUSY MUST NOT OVERWRITE. It raises the fault and
      //    leaves the request toggle alone -- never a silent drop.
      if ($past(src_publish_i) && $past(src_busy_o)) begin
        a_no_overwrite_flag: assert (ovf_o);
        a_no_overwrite_tog:  assert (req_tog == $past(req_tog));
        a_no_overwrite_hold: assert (hold == $past(hold));
      end

      // 3. THE FAULT IS STICKY. A lost snapshot is not a transient.
      if ($past(ovf_o)) a_ovf_sticky: assert (ovf_o);
    end
  end

  always_ff @(posedge dst_clk) begin
    if (f_past_valid && dst_rst_n && $past(dst_rst_n)) begin
      // 4. THE DESTINATION NEVER FABRICATES A SNAPSHOT: the collected value
      //    moves only on a request edge, so nothing appears that the source
      //    did not publish.
      if (!$past(req_edge)) begin
        a_no_fabrication: assert (q_o == $past(q_o));
        a_valid_steady:   assert (q_valid_o == $past(q_valid_o));
      end

      // 5. THE ACK NAMES THE REQUEST ACTUALLY CONSUMED, which is what lets the
      //    source distinguish "my value was taken" from "time passed".
      if ($past(req_edge)) begin
        a_ack_matches: assert (ack_tog == $past(req_sync));
        a_captured:    assert (q_o == $past(hold));
        a_valid_set:   assert (q_valid_o);
      end

      // 6. VALIDITY NEVER RETRACTS.
      if ($past(q_valid_o)) a_valid_sticky: assert (q_valid_o);
    end
  end

  // COVERS. Every assertion above is guarded, and a machine that never
  // publishes satisfies all of them while proving nothing -- the vacuity shape
  // MEM.GUARD once shipped and DEBUG.FRAMEBLIT nearly did. These demand the
  // machine actually RUNS.
  always_ff @(posedge src_clk) begin
    if (src_rst_n) begin
      c_publishes: cover (src_publish_i && !src_busy_o);
      c_busy:      cover (src_busy_o);
      c_overflow:  cover (ovf_o);
    end
  end
  always_ff @(posedge dst_clk) begin
    if (dst_rst_n) begin
      c_collected: cover (q_valid_o);
      c_edge:      cover (req_edge);
      c_nonzero:   cover (q_valid_o && q_o != '0);
    end
  end
`endif

endmodule
