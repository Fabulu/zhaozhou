// zhao_hps_arbiter.sv — MEM.HPS.ARBITER: two clients, one HPS bridge port.
//
// Contract: design/contracts/MEM.HPS.ARBITER.md
// Design:   reports/DEBUG.FRAMEBLIT_Integration_Corrections.md §7 (Step 3)
//
// ---------------------------------------------------------------------------
// WHY THIS EXISTS
// ---------------------------------------------------------------------------
// Splitting the blit out of CMD.DMA gave the machine two things that want the
// HPS bridge:
//
//     CMD.DMA          \
//                       -> this -> zhao_hps_bridge
//     DEBUG.FRAMEBLIT  /
//
// `zhao_hps_bridge` has ONE client port and ONE burst in flight. Wiring two
// requesters to it without an arbiter does not merely risk a collision — the
// bridge treats a request arriving while it is busy as a PROTOCOL VIOLATION and
// answers it with `err`. The loser of an unarbitrated race does not wait; it is
// told its transfer failed.
//
// ---------------------------------------------------------------------------
// THE SIX RULES
// ---------------------------------------------------------------------------
// 1. **ONE OWNER AT A TIME, AND THE OWNER IS LATCHED.** Ownership is decided
//    once, when the bridge is idle, and does not change until the burst ends.
//    Re-deciding while a burst is in flight is how a response beat reaches the
//    wrong client.
//
// 2. **A RESPONSE BEAT GOES TO THE OWNER AND NOWHERE ELSE.** The non-owner's
//    response port is held quiet for the whole transaction. This is the rule
//    the review names, and it is the one that corrupts data rather than merely
//    stalling it: a beat delivered to the wrong client is written into the
//    wrong buffer with no error anywhere.
//
// 3. **THE BRIDGE REQUEST IS A ONE-CYCLE PULSE, AND THAT IS NOT NEGOTIABLE.**
//    This is the rule that is easy to get exactly backwards, and I did.
//    `zhao_hps_bridge` accepts combinationally and sets `busy` on the same
//    edge, then raises `req_grant` the NEXT cycle. So a request still asserted
//    on the cycle the grant arrives is a request arriving WHILE BUSY -- which
//    the bridge treats as a protocol violation and answers with `err`.
//
//    "Hold the request stable until grant" is therefore the correct rule for
//    the CLIENT-facing side and precisely the wrong one for the bridge-facing
//    side. CMD.DMA already knew this: its `hps_req_v` is set in one state and
//    cleared by a default assignment the next cycle, a one-cycle pulse.
//
//    Converting between the two is the arbiter's job, and arguably its most
//    useful one: clients get a proper ready/valid handshake they can hold, and
//    the bridge gets the single pulse it requires, issued only when the arbiter
//    KNOWS the bridge is idle -- which no individual client can know.
//
// 4. **AN `err` WITHOUT A GRANT STILL ENDS THE TRANSACTION.** A malformed burst
//    is rejected by the bridge with `err | last` and NO grant and NO busy — so
//    an arbiter that waits only for `req_grant` waits forever, holding the port
//    against a client that will never be served. This is not a corner case; it
//    is what happens to every malformed request.
//
// 5. **A WRITE BURST ENDS ON `wr_last`, NOT ON A RESPONSE.** The bridge sends
//    NO response beats for a write: it forwards the data and clears `busy` when
//    `wr_last` goes through. An arbiter that waits for `rsp.last` therefore
//    holds the port forever on the first write it ever arbitrates.
//
//    Nothing in Phase 2 writes to HPS DDR, which is exactly why this was worth
//    getting right rather than leaving to be discovered later: an untested path
//    with a latent hang in it is worse than an absent one, because it looks
//    finished. The mutation sweep is what surfaced it -- a mutation that routed
//    write data from the wrong client SURVIVED, because no test issued a write
//    at all.
//
// 6. **CLIENT 0 HAS STRICT PRIORITY, AND THE WAITING IS COUNTED.** Command
//    packet acquisition outranks a debug blit, which is not game-facing and may
//    wait. Strict priority means client 1 can be starved by a client 0 that
//    never stops asking, so `c1_wait_cycles_o` exists to make that visible
//    rather than leaving it to be discovered as "the debug blit sometimes never
//    happens".
//
// Client 0 is CMD.DMA; client 1 is DEBUG.FRAMEBLIT. The mapping is the shell's
// to make, and it is encoded and tested here rather than assumed: even if the
// scheduler's ordering makes the two mutually exclusive today, that is a
// property of the scheduler and not of this wire.
module zhao_hps_arbiter (
    input logic clk,
    input logic rst_n,

    // ---- client 0 (high priority: CMD.DMA) ---------------------------------
    input  zhao_pkg::zhao_hps_burst_req_t c0_req_i,
    output logic                          c0_req_grant_o,
    input  logic                          c0_wr_valid_i,
    input  logic [63:0]                   c0_wr_data_i,
    input  logic                          c0_wr_last_i,
    output zhao_pkg::zhao_hps_burst_rsp_t c0_rsp_o,

    // ---- client 1 (low priority: DEBUG.FRAMEBLIT) --------------------------
    input  zhao_pkg::zhao_hps_burst_req_t c1_req_i,
    output logic                          c1_req_grant_o,
    input  logic                          c1_wr_valid_i,
    input  logic [63:0]                   c1_wr_data_i,
    input  logic                          c1_wr_last_i,
    output zhao_pkg::zhao_hps_burst_rsp_t c1_rsp_o,

    // ---- the one bridge port ------------------------------------------------
    output zhao_pkg::zhao_hps_burst_req_t b_req_o,
    input  logic                          b_req_grant_i,
    output logic                          b_wr_valid_o,
    output logic [63:0]                   b_wr_data_o,
    output logic                          b_wr_last_o,
    input  zhao_pkg::zhao_hps_burst_rsp_t b_rsp_i,

    // ---- counters -----------------------------------------------------------
    output logic [31:0] c0_bursts_o,
    output logic [31:0] c1_bursts_o,
    output logic [31:0] c1_wait_cycles_o   // rule 5: starvation must be visible
);

  localparam logic [1:0] A_IDLE = 2'd0;
  localparam logic [1:0] A_PULSE = 2'd1;  // the request is on the wire, ONE cycle
  localparam logic [1:0] A_WAIT = 2'd2;   // awaiting the grant (or an err)
  localparam logic [1:0] A_ACTIVE = 2'd3; // the burst is the owner's

  logic [1:0] state;
  logic       owner;        // 0 or 1; meaningful outside A_IDLE
  logic       owner_write;  // rule 5: a write burst ends differently
  logic       pick;      // rule 5: strict priority
  assign pick = c0_req_i.valid ? 1'b0 : 1'b1;

  // Rule 1: the request presented to the bridge is the OWNER's once one is
  // chosen -- never a fresh re-decision, which is how a grant lands on one
  // client while the beats belong to another.
  always_comb begin
    b_req_o = '0;
    b_wr_valid_o = 1'b0;
    b_wr_data_o = 64'd0;
    b_wr_last_o = 1'b0;

    case (state)
      // Rule 3: the request appears for EXACTLY ONE CYCLE, from the latched
      // owner, and only in this state. Driving it in A_IDLE as well -- which is
      // the obvious way to save a cycle -- presents it twice: the bridge
      // accepts on the first and counts the second as a violation.
      A_PULSE: begin
        b_req_o = owner ? c1_req_i : c0_req_i;
        b_req_o.valid = 1'b1;
      end
      A_ACTIVE: begin
        // Write beats belong to the owner alone.
        b_wr_valid_o = owner ? c1_wr_valid_i : c0_wr_valid_i;
        b_wr_data_o  = owner ? c1_wr_data_i : c0_wr_data_i;
        b_wr_last_o  = owner ? c1_wr_last_i : c0_wr_last_i;
      end
      default: begin
        b_req_o = '0;
      end
    endcase
  end

  // Rule 2: a response beat reaches the OWNER and nobody else. In A_IDLE it
  // reaches nobody, because there is nothing in flight to answer.
  logic route_c0, route_c1;
  assign route_c0 = (state != A_IDLE) && (owner == 1'b0);
  assign route_c1 = (state != A_IDLE) && (owner == 1'b1);

  always_comb begin
    c0_rsp_o = '0;
    c1_rsp_o = '0;
    if (route_c0) c0_rsp_o = b_rsp_i;
    if (route_c1) c1_rsp_o = b_rsp_i;
  end

  // When the burst is over.
  //
  //   * a READ ends on the last response beat;
  //   * a WRITE ends when the last write beat goes through, because the bridge
  //     answers a write with NOTHING (rule 5);
  //   * either ends on `err` (rule 4), which can arrive before any grant.
  logic burst_done;
  assign burst_done = (b_rsp_i.beat_valid && b_rsp_i.last) || b_rsp_i.err ||
                      (owner_write && b_wr_valid_o && b_wr_last_o);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state <= A_IDLE;
      owner <= 1'b0;
      owner_write <= 1'b0;
      c0_req_grant_o <= 1'b0;
      c1_req_grant_o <= 1'b0;
      c0_bursts_o <= 32'd0;
      c1_bursts_o <= 32'd0;
      c1_wait_cycles_o <= 32'd0;
    end else begin
      c0_req_grant_o <= 1'b0;
      c1_req_grant_o <= 1'b0;

      // Rule 5: client 1 waiting while it wants the bridge and does not have
      // it. Strict priority is the policy; this is what makes its cost legible.
      if (c1_req_i.valid && !((state != A_IDLE) && (owner == 1'b1))) begin
        if (c1_wait_cycles_o != 32'hFFFF_FFFF) c1_wait_cycles_o <= c1_wait_cycles_o + 32'd1;
      end

      case (state)
        A_IDLE: begin
          if (c0_req_i.valid || c1_req_i.valid) begin
            owner <= pick;
            owner_write <= pick ? c1_req_i.write : c0_req_i.write;
            state <= A_PULSE;
          end
        end

        A_PULSE: begin
          // The request was on the wire for this cycle and no longer will be.
          state <= A_WAIT;
        end

        A_WAIT: begin
          // Rule 4 first: a malformed burst is answered with err and NO grant
          // and NO busy. An arbiter that waits only for the grant holds the
          // port forever -- and the symptom is not this transfer failing, it is
          // the NEXT one never happening.
          if (b_rsp_i.err) begin
            state <= A_IDLE;
          end else if (b_req_grant_i) begin
            if (owner) begin
              c1_req_grant_o <= 1'b1;
              if (c1_bursts_o != 32'hFFFF_FFFF) c1_bursts_o <= c1_bursts_o + 32'd1;
            end else begin
              c0_req_grant_o <= 1'b1;
              if (c0_bursts_o != 32'hFFFF_FFFF) c0_bursts_o <= c0_bursts_o + 32'd1;
            end
            state <= A_ACTIVE;
          end
        end

        A_ACTIVE: begin
          if (burst_done) state <= A_IDLE;
        end

        default: state <= A_IDLE;
      endcase
    end
  end

endmodule : zhao_hps_arbiter
