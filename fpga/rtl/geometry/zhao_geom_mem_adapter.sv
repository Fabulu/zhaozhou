// zhao_geom_mem_adapter.sv — two logical geometry requesters, one ENGINE1 client.
//
// Law: reports/COMBINE-ASSETFETCH-RECOVERY-20260906.txt §12
//      spec/memory_rules.md §5f (the asset pool window, ENGINE1, read-only)
//
// ===========================================================================
// WHY THIS BLOCK EXISTS: A SPARE SLOT IS NOT A SPARE PRIVILEGE
// ===========================================================================
// D22 tread 10 put GEOM.ASSETFETCH on real memory and found that GEOM.MESHFETCH
// could not follow it, for a reason that is not about wiring:
//
//   `zhao_vram_arbiter` builds the controller's client tag by CASTING THE SLOT
//   INDEX -- `ctrl_req.client = zhao_client_e'(offer_client)` -- so slot 3 IS
//   ENGINE1 and slot 4 IS DEBUG, positionally. And `zhao_mem_guard` grants the
//   asset-pool window to ENGINE1 alone; DEBUG falls to `default: pass_ok = 1'b0`
//   and owns nothing.
//
// So the second geometry fetcher had no client identity that both the guard
// admits and the arbiter carries. The owner's recovery brief §12.1 names the
// answer and it is this block: **share the one permitted client**, upstream of
// the guard, with explicit logical ownership. Not a second controller, not a
// crossbar, and emphatically not a fetcher parked on slot 4 to be relabelled
// DEBUG one level down and refused.
//
//   MESHFETCH descriptors --+
//                           +--> THIS BLOCK --> ENGINE1 guard --> arbiter slot 3
//   ASSETFETCH payloads ----+
//
// ===========================================================================
// THE THREE ACCEPTANCE BOUNDARIES ARE DIFFERENT EVENTS (§12.2)
// ===========================================================================
// Adapter acceptance, guard acceptance/verdict, and arbiter/controller grant
// and data completion are three separate things, and collapsing any two of them
// is how this subsystem has already been wrong twice:
//
//   * A GUARD OK IS NOT RETURNED DATA. The guard answers `ready` as a LEVEL and
//     pulses `ok` the cycle AFTER the accept, so S_VERD is preserved here
//     rather than testing `ready && ok` -- the mistake that made both fetchers
//     read every passing request as a denial.
//   * AN ARBITER GRANT IS NOT A COMPLETED LINE. The arbiter splits a 64-byte
//     line into four 8-word physical bursts and may interleave scanout between
//     them. The logical owner is NOT released on the first credit return.
//
// ===========================================================================
// TWO BURST SCALES, NOT ONE (§12.3)
// ===========================================================================
// ASSETFETCH asks for 64 bytes -> eight packed 64-bit words.
// MESHFETCH  asks for 32 bytes -> four packed 64-bit words.
//
// The brief is explicit that MESHFETCH's checked 32-byte footprint must NOT be
// normalised up to an unchecked 64-byte read to make this block simpler, and
// that any beat/last generator assuming eight words has to be generalised and
// TESTED at four and eight rather than bypassed. So the expected packed-word
// count is derived from the accepted request's own length and stored, and the
// `last` this block emits upstream is that count -- not a constant.
//
// ===========================================================================
// ONE LOGICAL REQUEST IN FLIGHT, ON PURPOSE (§11.3, §12.4)
// ===========================================================================
// "Start with ONE logical geometry memory request in flight. A queue of future
//  requests or a second reserved bank is not multiple outstanding response
//  state. Do not add more outstanding requests until measurement justifies the
//  extra routing/credit complexity."
//
// That single-request rule is what makes the return-routing proof trivial: a
// returning word belongs to the recorded owner because there is exactly one,
// and no later queued request can capture an earlier request's return. The
// counters below exist so the decision to relax it is made against a number.
//
// Conservative SystemVerilog subset only (charter §2).
module zhao_geom_mem_adapter
  import zhao_pkg::*;
(
    input  var logic clk,
    input  var logic rst_n,

    // ---- requester A: GEOM.MESHFETCH, descriptors (32 bytes) ---------------
    input  var zhao_guard_req_t a_req_i,
    output var zhao_guard_rsp_t a_rsp_o,
    output var logic            a_beat_valid_o,
    output var logic [63:0]     a_beat_data_o,
    output var logic            a_beat_last_o,

    // ---- requester B: GEOM.ASSETFETCH, payload lines (64 bytes) ------------
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
    output var logic [31:0]     jobs_a_o,          // logical requests served, A
    output var logic [31:0]     jobs_b_o,          // ...and B
    output var logic [31:0]     denied_o,          // guard violations, either
    // A logical request that WAITED because the other requester held the
    // adapter. This is the number that says whether sharing one client costs
    // anything, and it is the number the decision to widen to two outstanding
    // requests has to be made against.
    output var logic [31:0]     contention_o,
    // Return-count faults, distinct because their causes differ: SHORT is the
    // line ending before its expected packed-word count, LONG is a word
    // arriving after it, UNOWNED is a word with no logical request recorded.
    output var logic [31:0]     err_short_o,
    output var logic [31:0]     err_long_o,
    output var logic [31:0]     err_unowned_o
);

  // ------------------------------------------------------------------ FSM --
  typedef enum logic [2:0] {
    A_IDLE  = 3'd0,  // nothing in flight; arbitrate
    A_REQ   = 3'd1,  // offering the selected request to the guard
    A_VERD  = 3'd2,  // the guard's verdict, one cycle after it accepted
    A_FILL  = 3'd3,  // the logical request's useful words are returning
    A_DRAIN = 3'd4   // discard an overlong return through physical LAST
  } astate_e;

  astate_e st_q;

  // THE LOGICAL OWNER, recorded BEFORE issue (§12.4). One request in flight, so
  // this is one register and not a table -- but it is a NAMED record rather
  // than an implicit "whoever asked last", because that distinction is the
  // whole point of the block.
  logic       own_b_q;          // 0 = requester A, 1 = requester B
  logic [3:0] expect_q;         // expected packed 64-bit words: 4 or 8
  logic [3:0] recv_q;           // received so far
  zhao_guard_req_t sel_q;       // the captured request, held stable while
                                // the guard is blocked (§12.2)

  // ROUND-ROBIN AT LOGICAL-REQUEST BOUNDARIES (§12.5). `last_b_q` remembers who
  // went last, so a requester that is always ready cannot starve the other.
  // Deliberately NOT a descriptor preference: the brief permits a bounded one
  // to maintain lookahead but forbids it starving payload filling, and a plain
  // alternation cannot starve anything. A preference can be added later against
  // `contention_o` rather than ahead of it.
  logic last_b_q;

  wire a_wants = a_req_i.valid;
  wire b_wants = b_req_i.valid;
  // Alternate when both ask; otherwise take whoever asked.
  wire pick_b_c = (a_wants && b_wants) ? ~last_b_q : b_wants;

  // EXPECTED WORDS FROM THE REQUEST'S OWN LENGTH, not from who asked. A 64-byte
  // line is eight packed words and a 32-byte descriptor is four; `len` is in
  // BYTES and a packed word is eight of them.
  function automatic logic [3:0] words_of(input logic [6:0] len_bytes);
    words_of = 4'(len_bytes >> 3);
  endfunction

  // ---------------------------------------------------------- upstream rsp --
  // Each requester sees a guard-shaped response, and it must observe the SAME
  // two-cycle law the real guard has: ready is a level, the verdict is a pulse
  // the next cycle. Presenting a one-cycle `ready && ok` here would hand both
  // fetchers back the protocol they were just repaired away from.
  logic rsp_ok_q, rsp_viol_q;

  always_comb begin
    a_rsp_o = '0;
    b_rsp_o = '0;
    // Ready only when the adapter is free AND this requester is the one the
    // arbitration picked, so a losing requester is held rather than dropped.
    if (st_q == A_IDLE) begin
      a_rsp_o.ready = a_wants && !pick_b_c;
      b_rsp_o.ready = b_wants &&  pick_b_c;
    end
    if (own_b_q) begin
      b_rsp_o.ok        = rsp_ok_q;
      b_rsp_o.violation = rsp_viol_q;
    end else begin
      a_rsp_o.ok        = rsp_ok_q;
      a_rsp_o.violation = rsp_viol_q;
    end
  end

  // ------------------------------------------------------- downstream req --
  // THE TRUSTED FIXED IDENTITY (§11.2). The client field is forced to ENGINE1
  // here rather than forwarded from the requester: it is the one identity the
  // guard grants the asset pool to, and a leaf test's generic client input must
  // never be able to reach the production guard through this path.
  always_comb begin
    m_req_o        = sel_q;
    m_req_o.valid  = (st_q == A_REQ);
    m_req_o.client = ZHAO_CLIENT_ENGINE1;
    m_req_o.write  = 1'b0;   // the asset window is READ-ONLY by construction
  end

  // ------------------------------------------------------- beat returning --
  // Routed by the RECORDED owner, never by whoever is currently asking. A
  // scanout burst between two asset bursts does not touch this record, and a
  // queued request cannot capture it, because there is only one.
  wire beat_ok_c = (st_q == A_FILL) && m_beat_valid_i;

  assign a_beat_valid_o = beat_ok_c && !own_b_q;
  assign b_beat_valid_o = beat_ok_c &&  own_b_q;
  assign a_beat_data_o  = m_beat_data_i;
  assign b_beat_data_o  = m_beat_data_i;

  // `last` IS THE EXPECTED COUNT, not the downstream flag. The shell's geometry
  // return generator counts a fixed eight; a 32-byte descriptor must see its
  // last on word four. Generalised here so both lengths work from one
  // downstream generator, which is what §12.3 requires.
  wire last_c = beat_ok_c && (recv_q + 4'd1 == expect_q);
  assign a_beat_last_o = last_c && !own_b_q;
  assign b_beat_last_o = last_c &&  own_b_q;

  // ------------------------------------------------------------- seq core --
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_q         <= A_IDLE;
      own_b_q      <= 1'b0;
      last_b_q     <= 1'b0;
      expect_q     <= 4'd0;
      recv_q       <= 4'd0;
      sel_q        <= '0;
      rsp_ok_q     <= 1'b0;
      rsp_viol_q   <= 1'b0;
      jobs_a_o     <= 32'd0;
      jobs_b_o     <= 32'd0;
      denied_o     <= 32'd0;
      contention_o <= 32'd0;
      err_short_o  <= 32'd0;
      err_long_o   <= 32'd0;
      err_unowned_o <= 32'd0;
    end else begin
      rsp_ok_q   <= 1'b0;      // one-cycle verdict pulses
      rsp_viol_q <= 1'b0;

      // A word arriving with no logical owner. An overlong return in A_DRAIN
      // still belongs to the recorded request even though its useful count is
      // complete, so it is discarded there rather than mislabelled UNOWNED.
      if (m_beat_valid_i && (st_q != A_FILL) && (st_q != A_DRAIN)) begin
        err_unowned_o <= err_unowned_o + 32'd1;
      end

      unique case (st_q)
        A_IDLE: begin
          // BOTH ASKED AND ONE WAITED. Counted at the moment the loser is held,
          // which is the only cycle the fact exists.
          if (a_wants && b_wants) contention_o <= contention_o + 32'd1;

          if (a_wants || b_wants) begin
            own_b_q  <= pick_b_c;
            last_b_q <= pick_b_c;
            sel_q    <= pick_b_c ? b_req_i : a_req_i;
            expect_q <= words_of(pick_b_c ? b_req_i.len : a_req_i.len);
            recv_q   <= 4'd0;
            st_q     <= A_REQ;
          end
        end

        A_REQ: begin
          // The guard's ready is a LEVEL; its verdict is the next cycle. The
          // selected request stays stable meanwhile (§12.2) because `sel_q` is
          // a register, not a mux of the live requester ports.
          if (m_rsp_i.ready) st_q <= A_VERD;
        end

        A_VERD: begin
          if (m_rsp_i.ok) begin
            rsp_ok_q <= 1'b1;
            st_q     <= A_FILL;
            if (own_b_q) jobs_b_o <= jobs_b_o + 32'd1;
            else         jobs_a_o <= jobs_a_o + 32'd1;
          end else if (m_rsp_i.violation) begin
            rsp_viol_q <= 1'b1;
            denied_o   <= denied_o + 32'd1;
            st_q       <= A_IDLE;
          end
          // Neither yet: WAIT. Reading silence as an answer is the mistake
          // S_VERD exists to end, in this block as in the two fetchers.
        end

        A_FILL: begin
          if (m_beat_valid_i) begin
            recv_q <= recv_q + 4'd1;
            if (recv_q + 4'd1 == expect_q) begin
              // The requester's useful record is complete. Release the owner
              // only if the physical return agrees. Otherwise retain ownership
              // in A_DRAIN through physical LAST so surplus words cannot be
              // mistaken for an idle gap or captured by a later request.
              if (m_beat_last_i) begin
                st_q <= A_IDLE;
              end else begin
                err_long_o <= err_long_o + 32'd1;
                st_q       <= A_DRAIN;
              end
            end else if (m_beat_last_i) begin
              // Downstream said last before the expected count: the line ended
              // early and the rest of the record would be stale RAM.
              err_short_o <= err_short_o + 32'd1;
              st_q        <= A_IDLE;
            end
          end
        end

        A_DRAIN: begin
          // Surplus words belong to the overlong physical response and are not
          // exposed to either requester. Keeping the adapter occupied until its
          // LAST preserves the one-request-in-flight ownership law.
          if (m_beat_valid_i && m_beat_last_i) st_q <= A_IDLE;
        end
        default: st_q <= A_IDLE;
      endcase
    end
  end

endmodule
