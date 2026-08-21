// zhao_debug_frameblit.sv — DEBUG.FRAMEBLIT: the debug frame blit, split out
// of CMD.DMA.
//
// Contract: design/contracts/DEBUG.FRAMEBLIT.md
// Design:   reports/CMD.DMA_Redesign_Proposal.md, Part 2
// Reference: `zref::debug::FrameBlit` (reference/include/zref/zref_frameblit.hpp)
//
// ---------------------------------------------------------------------------
// WHY THIS BLOCK EXISTS AT ALL
// ---------------------------------------------------------------------------
// `DebugFrameBlit` is a debug-umbrella command and is never game-facing, yet as
// shipped it lived inside CMD.DMA and carried a WHOLE-CANVAS staging buffer —
// 245,760 bytes — because the old contract said no VRAM write may occur before
// the source CRC passes. That one buffer is what pushed the command front end,
// and therefore the whole shell, out of a fittable size. A debug-only transport
// path must not be able to do that.
//
// ---------------------------------------------------------------------------
// THE AMENDED LAW, and it is an amendment rather than a relaxation
// ---------------------------------------------------------------------------
// OLD: no byte is written to VRAM before CRC verification.
// NEW: **no framebuffer slot becomes visible or READY before every byte has
//      been written, all writes have retired, and the CRC matches.**
//
// Raw writes into an inactive, uncommitted slot are not visible to anyone: the
// shell only toggles slot readiness when `blit_status == 0`, and FRAMECTL only
// swaps to a committed READY slot. So the externally meaningful commit point was
// never the first write — it is the moment the slot becomes READY. The
// framebuffer slot itself is the transaction buffer, which is what double
// buffering is for.
//
// The staging buffer therefore collapses from 1,966,080 bits to 512:
//
//     logic [63:0] chunk [0:7];
//
// ---------------------------------------------------------------------------
// WHY NOT THE TWO-PASS DDR ALTERNATIVE
// ---------------------------------------------------------------------------
// Reading the source twice — once to verify, once to commit — preserves "zero
// guard writes on reject" and is UNSOUND here, because the pixel arena is a raw
// HPS address with no descriptor or ownership state. The HPS can mutate it
// between the passes: pass 1 verifies bytes A, pass 2 commits bytes B, and the
// CRC that was checked describes data that is no longer there.
//
// Single-pass has no such hole. If the HPS changes the source mid-read, the
// stream fails its expected CRC and the dirty inactive slot is simply never
// published.
//
// ---------------------------------------------------------------------------
// THE LEASE, which is the part that makes speculative writes safe
// ---------------------------------------------------------------------------
// The old guard checked only that writes fell inside the command-granted slot.
// It did not know whether that slot was on screen. Writing speculatively into a
// slot somebody is looking at would be visible corruption.
//
// So the slot arrives as a LEASE from the shell/frame-control seam, and
// `dst_slot` from the frozen ABI must MATCH the granted lease — it is no longer
// trusted merely because it is 0 or 1. A slot is leasable only when it is not
// displayed, not already READY or committed for the next swap, and not already
// being written.
//
// **The lease must hold for the WHOLE transaction.** If it drops mid-blit the
// transaction aborts without publishing: a lease that was valid at the start and
// not at the end proves nothing about where those bytes went.
//
// **AND THE GENERATION IS PART OF THAT CHECK.** Watching only `valid` and `slot`
// has an ABA hole: a lease that drops and is re-granted for the SAME slot
// mid-transaction looks identical to one that never lapsed, and the bytes
// already written belong to somebody else's lease. The generation is latched at
// accept and compared every cycle, so a re-grant is a lease loss.
//
// ---------------------------------------------------------------------------
// BACKPRESSURE IS A REAL HANDSHAKE NOW
// ---------------------------------------------------------------------------
// The old DMA emitted `guard_wvalid_o` with no `guard_wready_i` and caught
// overflow afterwards with a sticky error. Here the write data has a proper
// handshake, and while `wvalid && !wready` the data and the `last` marker are
// HELD stable — a beat that changes under a stalled consumer is a corrupted
// pixel nobody can trace.
//
// ---------------------------------------------------------------------------
// WHAT IS NOT BUILT HERE
// ---------------------------------------------------------------------------
// The slot manager's FREE -> WRITING -> READY -> DISPLAYED -> FREE state machine
// lives at the shell seam, not in this block. This block consumes a lease and
// reports `publish` or `release`; it does not decide which slot is displayed.
// ---------------------------------------------------------------------------
// WHAT THE INTEGRATION REVIEW CHANGED (reports/DEBUG.FRAMEBLIT_Integration_Corrections.md)
// ---------------------------------------------------------------------------
// The first version of this block was unit-verified against a FAKE guard, a
// FAKE bridge and a self-declared notion of retirement, and every one of those
// three fakes was more agreeable than the real thing. Six corrections, each a
// place the block would have been wrong the moment it met real hardware:
//
// 1. **THE GUARD ADDRESS IS ABSOLUTE, NOT SLOT-RELATIVE.** `zhao_mem_guard`
//    checks `addr >= blit_base`, where `blit_base` is ZHAO_FB_SLOT1_BASE
//    (0x0200_0000) for slot 1. This block used to emit the byte offset. Slot 0
//    worked only because its base happens to be zero; EVERY slot-1 request
//    would have been denied. The test could not see it because its fake guard
//    returned OK without looking at the address.
//
// 2. **"RETIRED" NOW MEANS RETIRED.** The counter used to advance when a chunk
//    was handed downstream, which is not the same as the SDRAM controller
//    having completed the writes. A slot could be published while its data sat
//    in the write FIFO, the arbiter, or the controller's pending burst. The
//    real signal already existed — the VRAM arbiter returns per-client credits
//    as bursts retire — so retirement now comes in from outside on
//    `retire_words_i`, in 16-bit words, and is accumulated in EVERY state
//    because credits come back while the next chunk is being read.
//
// 3. **PUBLISH AND RELEASE CARRY IDENTITY.** A bare pulse does not say which
//    slot or which generation it refers to, which is exactly what an ABA
//    sequence exploits. Both events now carry slot and generation so the slot
//    manager can refuse a stale one.
//
// 4. **A FAILURE DRAINS BEFORE IT RELEASES.** Releasing a slot while writes are
//    still in flight lets the next owner start writing and then be overwritten
//    by the dead transaction's bytes. Failure now stops, drains to
//    `retired == issued`, and only then releases.
//
// 5. **PRE-ACQUISITION FAILURES RELEASE NOTHING.** A bad length, a missing
//    lease and a slot mismatch never acquired ownership, so releasing on them
//    would free somebody else's lease. `owns_lease` is set only after all three
//    validations pass, and release is gated on it. An error completion is not
//    an ownership transition.
//
// 6. **THE LEASE IS CHECKED AT THE PUBLICATION EDGE ITSELF.** Checking it in an
//    earlier state and publishing in a later one leaves a window; the final
//    check and the publish pulse now happen in the same state on the same edge.
//    And losing the lease immediately stops every outward side effect rather
//    than being noticed at the end.
//
// One more, on the bridge: the block used to assert an HPS request for exactly
// one state and move on, assuming the bridge was idle and accepted it. It now
// waits for `hps_req_grant_i`, and holds the request stable until then.
//
module zhao_debug_frameblit
`ifdef FORMAL
#(
  // FORMAL-ONLY (structurally absent outside `ifdef FORMAL, so synthesis and
  // the Verilator lane always run the real law): a nonzero value overrides
  // `canvas_bytes(mode)` so a WHOLE transaction fits a tractable BMC depth.
  //
  // Without it every publish property below would be vacuous. The smallest
  // lawful canvas is 184,320 bytes -- 2,880 chunks, over 46,000 cycles -- so a
  // bounded model can never reach `publish_valid_o` and would "prove" the
  // publish properties by never raising their antecedent. That is exactly the
  // failure shape MEM.GUARD hit, and the covers below exist to catch it.
  parameter int unsigned FORMAL_CANVAS_BYTES = 0
)
`endif
(
    input logic clk,
    input logic rst_n,

    // ---- blit request, from CMD.SCHEDULER's dispatch -----------------------
    input  logic        req_valid_i,
    output logic        req_ready_o,
    input  logic [ 7:0] req_dst_slot_i,  // frozen ABI field; must match the lease
    input  logic [ 7:0] req_mode_i,      // ABI video_mode byte
    input  logic [31:0] req_src_i,       // HPS source address
    input  logic [31:0] req_len_i,       // must equal canvas_bytes(mode)
    input  logic [31:0] req_crc_i,       // expected CRC-32C over the payload

    // ---- the framebuffer-slot lease ---------------------------------------
    input  logic        fb_lease_valid_i,
    input  logic        fb_lease_slot_i,
    input  logic [15:0] fb_lease_generation_i,

    // Terminal events, each carrying the identity of the lease it belongs to.
    // A bare pulse cannot say WHICH slot and WHICH generation it refers to, and
    // after an ABA re-grant that is precisely the question. The slot manager
    // accepts one only when the slot is WRITING and the generation matches.
    output logic        release_valid_o,
    output logic        release_slot_o,
    output logic [15:0] release_generation_o,
    output logic        publish_valid_o,
    output logic        publish_slot_o,
    output logic [15:0] publish_generation_o,

    // ---- HPS source burst reader ------------------------------------------
    output zhao_pkg::zhao_hps_burst_req_t hps_req_o,
    input  logic                          hps_req_grant_i,  // the bridge accepted
    input  zhao_pkg::zhao_hps_burst_rsp_t hps_rsp_i,

    // ---- guarded local-SDRAM write ----------------------------------------
    output zhao_pkg::zhao_guard_req_t guard_req_o,
    input  zhao_pkg::zhao_guard_rsp_t guard_rsp_i,
    output logic [63:0]               guard_wdata_o,
    output logic                      guard_wvalid_o,
    input  logic                      guard_wready_i,
    output logic                      guard_wlast_o,

    // ---- retirement, from the VRAM arbiter's BLIT client credit port -------
    // 16-bit SDRAM words, one credit per retired word. This is the ONLY source
    // of truth for "the write actually landed"; the block's own issue counter
    // says nothing about it.
    input  logic [7:0] retire_words_i,

    // ---- completion --------------------------------------------------------
    output logic       done_o,        // one pulse per request
    output logic [7:0] status_o,      // 0 = published; else the reason
    output logic [31:0] blits_published_o,
    output logic [31:0] blits_rejected_o
);

  // Status codes. 0 is the only success, and every failure is distinguishable
  // because "the blit did not appear" is otherwise unactionable.
  localparam logic [7:0] ST_OK          = 8'd0;
  localparam logic [7:0] ST_BAD_LEN     = 8'd1;   // len != canvas_bytes(mode)
  localparam logic [7:0] ST_NO_LEASE    = 8'd2;   // no lease at start
  localparam logic [7:0] ST_SLOT_MISMATCH = 8'd3; // dst_slot != leased slot
  localparam logic [7:0] ST_LEASE_LOST  = 8'd4;   // lease dropped mid-transaction
  localparam logic [7:0] ST_BRIDGE_ERR  = 8'd5;   // HPS burst error
  localparam logic [7:0] ST_GUARD_DENY  = 8'd6;   // the guard refused a write
  localparam logic [7:0] ST_CRC         = 8'd7;   // every byte written, CRC wrong

  localparam int CHUNK_BEATS = 8;   // 64 bytes
  localparam int CHUNK_BYTES = 64;

  typedef enum logic [3:0] {
    B_IDLE,
    B_VALIDATE,
    B_READ_REQUEST,
    B_READ_CHUNK,
    B_GUARD_REQUEST,
    B_GUARD_VERDICT,
    B_WRITE_CHUNK,
    B_NEXT_CHUNK,
    B_DECIDE,        // retirement + live lease + CRC + publish, ONE edge
    B_ABORT_STOP,    // stop issuing; drain any burst already in flight
    B_ABORT_DRAIN,   // wait for retired == issued before touching the lease
    B_RELEASE        // release the exact generation, iff ownership was acquired
  } state_e;

  state_e state;

  // The ONE buffer. 512 bits, against the 1,966,080 the old design carried.
  logic [63:0] chunk [0:CHUNK_BEATS-1];
  logic [ 2:0] beat;

  logic [ 7:0] r_slot, r_mode;
  logic [31:0] r_src, r_len, r_crc;
  logic [31:0] off;          // byte offset into the canvas
  logic [31:0] issued, retired;
  logic [31:0] crc_acc;
  logic [ 7:0] fail;
  logic [15:0] r_gen;        // the generation granted at accept

  // Ownership, and the two flags that make failure safe.
  //
  // `owns_lease` is set ONLY after length, lease validity, slot and generation
  // all check out. Release is gated on it, because a bad length or a missing
  // lease never acquired anything and releasing on them would free a lease
  // belonging to somebody else. An error completion is not an ownership
  // transition.
  //
  // `abort_pending` latches the first thing that went wrong. Once it is set no
  // new HPS request, guard request or write beat may be started -- the reason
  // the lease exists is that writes stop when it does, not that somebody
  // notices at the end.
  logic        owns_lease;
  logic        abort_pending;
  logic        hps_inflight;  // a granted burst whose `last` has not arrived

  function automatic logic [31:0] canvas_bytes(input logic [7:0] m);
`ifdef FORMAL
    canvas_bytes = (FORMAL_CANVAS_BYTES != 0)
                 ? 32'(FORMAL_CANVAS_BYTES)
                 : zhao_pkg::zhao_canvas_bytes(zhao_pkg::zhao_mode_from_abi(m));
`else
    canvas_bytes = zhao_pkg::zhao_canvas_bytes(zhao_pkg::zhao_mode_from_abi(m));
`endif
  endfunction

  // CRC-32C (Castagnoli), the same polynomial the ABI's zhao_crc32c uses,
  // fed a byte at a time in stream order. The reflected form: the message bit
  // enters at the LSB end.
  function automatic logic [31:0] crc32c_byte(input logic [31:0] c, input logic [7:0] d);
    logic [31:0] x;
    int k;
    begin
      x = c ^ {24'd0, d};
      for (k = 0; k < 8; k++) begin
        x = x[0] ? ((x >> 1) ^ 32'h82F6_3B78) : (x >> 1);
      end
      crc32c_byte = x;
    end
  endfunction

  logic [31:0] crc_next;
  always_comb begin
    crc_next = crc_acc;
    for (int k = 0; k < 8; k++) begin
      crc_next = crc32c_byte(crc_next, hps_rsp_i.data[k*8 +: 8]);
    end
  end

  logic [31:0] remaining;
  assign remaining = r_len - off;

  logic [6:0] this_len;
  assign this_len = (remaining >= 32'(CHUNK_BYTES)) ? 7'(CHUNK_BYTES) : 7'(remaining[6:0]);

  assign req_ready_o = (state == B_IDLE);

  // The lease must hold for the WHOLE transaction: a lease valid at the start
  // and gone at the end proves nothing about where the bytes went.
  logic lease_ok_now;
  assign lease_ok_now = fb_lease_valid_i && (fb_lease_slot_i == r_slot[0]) &&
                        (r_slot[7:1] == 7'd0) && (fb_lease_generation_i == r_gen);

  // Correction 1: the guard wants an ABSOLUTE VRAM address. `zhao_mem_guard`
  // passes a blit write only when `addr >= blit_base`, and blit_base is
  // ZHAO_FB_SLOT1_BASE for slot 1. Emitting the bare offset works for slot 0
  // solely because that base is zero. The base comes from the LATCHED and
  // validated lease target, never from a live request input.
  logic [31:0] fb_base;
  assign fb_base = r_slot[0] ? zhao_pkg::ZHAO_FB_SLOT1_BASE
                             : zhao_pkg::ZHAO_FB_SLOT0_BASE;

  // Correction 2: retirement arrives from outside, in 16-bit words. One word is
  // two bytes. It is accumulated in EVERY state below, not only while waiting,
  // because credits come back while the next chunk is still being read.
  logic [31:0] retire_bytes;
  assign retire_bytes = {23'd0, retire_words_i, 1'b0};

  // How many bytes this write beat actually carries. Every lawful canvas is a
  // multiple of 64 so this is always 8, but a tail beat must not be counted as
  // a full one or `issued` would never meet `retired`.
  logic [31:0] beat_left;
  logic [ 3:0] beat_bytes;
  assign beat_left  = 32'(this_len) - ({29'd0, beat} * 32'd8);
  assign beat_bytes = (beat_left >= 32'd8) ? 4'd8 : 4'(beat_left[3:0]);

  always_comb begin
    // The request is asserted for as long as B_READ_REQUEST lasts, which is
    // until the bridge grants it. Holding a request stable until grant is the
    // bridge's protocol; the block used to assert it for exactly one state and
    // advance regardless, which is only correct if the bridge happens to be
    // idle. That assumption stops holding the moment CMD.DMA shares the port.
    hps_req_o = '0;
    hps_req_o.valid = (state == B_READ_REQUEST);
    hps_req_o.write = 1'b0;
    hps_req_o.client = zhao_pkg::ZHAO_CLIENT_BLIT_DMA;
    hps_req_o.addr = r_src + off;
    hps_req_o.len = this_len;

    // The review's rule 6 in its direct form: a side-effecting output checks the
    // LIVE lease, rather than relying on a state having been entered while a
    // latched flag was clear. The state-level check below is still there and is
    // still the thing that stops the transaction; this gate is what guarantees
    // no guard request is even ASSERTED on a cycle the lease is not ours.
    //
    // It does mean a request can be withdrawn before the guard accepts it. That
    // is the intended trade: the guard drops an unaccepted request harmlessly,
    // whereas a write into a slot we no longer own is unrecoverable.
    guard_req_o = '0;
    guard_req_o.valid = (state == B_GUARD_REQUEST) && !abort_pending && lease_ok_now;
    guard_req_o.write = 1'b1;
    guard_req_o.client = zhao_pkg::ZHAO_CLIENT_BLIT_DMA;
    // Correction 1: absolute, not slot-relative.
    guard_req_o.addr = zhao_pkg::ZHAO_VRAM_ADDR_BITS'(fb_base + off);
    guard_req_o.len = this_len;
    guard_req_o.be = '1;

    // Data and `last` are HELD while the consumer stalls -- a beat that moves
    // under a stalled consumer is a corrupted pixel nobody can trace. Note that
    // `guard_wvalid_o` is a function of the STATE alone: an abort leaves
    // B_WRITE_CHUNK only on a beat boundary, so valid never drops mid-beat.
    guard_wvalid_o = (state == B_WRITE_CHUNK);
    guard_wdata_o = chunk[beat];
    guard_wlast_o = (state == B_WRITE_CHUNK) &&
                    (({29'd0, beat} + 32'd1) * 32'd8 >= 32'(this_len));
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state <= B_IDLE;
      beat <= '0;
      r_slot <= '0; r_mode <= '0; r_src <= '0; r_len <= '0; r_crc <= '0;
      r_gen <= '0;
      off <= '0; issued <= '0; retired <= '0;
      crc_acc <= 32'hFFFF_FFFF;
      fail <= ST_OK;
      owns_lease <= 1'b0;
      abort_pending <= 1'b0;
      hps_inflight <= 1'b0;
      done_o <= 1'b0;
      status_o <= ST_OK;
      release_valid_o <= 1'b0;
      release_slot_o <= 1'b0;
      release_generation_o <= '0;
      publish_valid_o <= 1'b0;
      publish_slot_o <= 1'b0;
      publish_generation_o <= '0;
      blits_published_o <= '0;
      blits_rejected_o <= '0;
      for (int i = 0; i < CHUNK_BEATS; i++) chunk[i] <= '0;
    end else begin
      done_o <= 1'b0;
      release_valid_o <= 1'b0;
      publish_valid_o <= 1'b0;

      // ---- retirement, accumulated in EVERY state ------------------------
      // Credits come back while the next chunk is being read, so a counter that
      // only advanced in the waiting state would miss most of them and then
      // wait forever for bytes that already landed.
      if (state != B_IDLE && retire_words_i != 8'd0) begin
        retired <= retired + retire_bytes;
      end

      // ---- the lease, watched on EVERY cycle we own it -------------------
      // Only once ownership is acquired: before that a false `lease_ok_now` is
      // the NORMAL state of affairs (there may be no lease at all, which is its
      // own status code) and must not be mistaken for losing one.
      if (owns_lease && !lease_ok_now && !abort_pending) begin
        abort_pending <= 1'b1;
        if (fail == ST_OK) fail <= ST_LEASE_LOST;
      end

      unique case (state)
        B_IDLE: begin
          if (req_valid_i && req_ready_o) begin
            r_slot <= req_dst_slot_i;
            r_mode <= req_mode_i;
            r_src <= req_src_i;
            r_len <= req_len_i;
            r_crc <= req_crc_i;
            off <= '0;
            issued <= '0;
            retired <= '0;
            crc_acc <= 32'hFFFF_FFFF;
            fail <= ST_OK;
            owns_lease <= 1'b0;
            abort_pending <= 1'b0;
            hps_inflight <= 1'b0;
            r_gen <= fb_lease_generation_i;
            state <= B_VALIDATE;
          end
        end

        B_VALIDATE: begin
          // The LATCHED length is what is judged. Reading the live input here
          // would judge whatever the producer left on the wire after the
          // handshake, which is not the request that was accepted.
          //
          // NOTHING here acquires ownership until all the checks pass, and each
          // failure goes to the release path with `owns_lease` still clear --
          // so it completes with a status and releases nothing.
          if (r_len != canvas_bytes(r_mode)) begin
            fail <= ST_BAD_LEN;
            abort_pending <= 1'b1;
            state <= B_ABORT_STOP;
          end else if (!fb_lease_valid_i) begin
            fail <= ST_NO_LEASE;
            abort_pending <= 1'b1;
            state <= B_ABORT_STOP;
          end else if (r_slot[7:1] != 7'd0 || fb_lease_slot_i != r_slot[0]) begin
            // The ABI's dst_slot is no longer trusted merely because it is 0
            // or 1: it must MATCH the slot the shell actually leased.
            fail <= ST_SLOT_MISMATCH;
            abort_pending <= 1'b1;
            state <= B_ABORT_STOP;
          end else if (fb_lease_generation_i != r_gen) begin
            // The generation moved between accept and validation: what we
            // latched is already somebody else's lease.
            fail <= ST_LEASE_LOST;
            abort_pending <= 1'b1;
            state <= B_ABORT_STOP;
          end else begin
            owns_lease <= 1'b1;
            state <= B_READ_REQUEST;
          end
        end

        B_READ_REQUEST: begin
          // Hold the request until the bridge grants it. If the lease has
          // already gone we do NOT withdraw a request that is on the wire --
          // the bridge protocol wants it stable -- we take the grant and drain
          // the burst in B_ABORT_STOP without writing any of it anywhere.
          if (hps_req_grant_i) begin
            hps_inflight <= 1'b1;
            beat <= '0;
            state <= abort_pending ? B_ABORT_STOP : B_READ_CHUNK;
          end
        end

        B_READ_CHUNK: begin
          if (hps_rsp_i.err) begin
            hps_inflight <= 1'b0;
            if (fail == ST_OK) fail <= ST_BRIDGE_ERR;
            abort_pending <= 1'b1;
            state <= B_ABORT_STOP;
          end else if (hps_rsp_i.beat_valid) begin
            if (abort_pending) begin
              // Drain. Never stored, never folded into the CRC.
              if (hps_rsp_i.last) begin
                hps_inflight <= 1'b0;
                state <= B_ABORT_STOP;
              end
            end else begin
              chunk[beat] <= hps_rsp_i.data;
              crc_acc <= crc_next;
              if (hps_rsp_i.last) begin
                hps_inflight <= 1'b0;
                beat <= '0;
                state <= B_GUARD_REQUEST;
              end else begin
                beat <= beat + 3'd1;
              end
            end
          end
        end

        B_GUARD_REQUEST: begin
          // A guard request is a side effect: once the lease is gone, none.
          if (abort_pending) state <= B_ABORT_STOP;
          else if (guard_rsp_i.ready) state <= B_GUARD_VERDICT;
        end

        B_GUARD_VERDICT: begin
          if (guard_rsp_i.violation || !guard_rsp_i.ok) begin
            if (fail == ST_OK) fail <= ST_GUARD_DENY;
            abort_pending <= 1'b1;
            state <= B_ABORT_STOP;
          end else if (abort_pending) begin
            state <= B_ABORT_STOP;
          end else begin
            beat <= '0;
            state <= B_WRITE_CHUNK;
          end
        end

        B_WRITE_CHUNK: begin
          if (guard_wvalid_o && guard_wready_i) begin
            issued <= issued + {28'd0, beat_bytes};
            // An abort leaves only on a beat boundary, so `wvalid` never drops
            // with a beat half-offered.
            if (abort_pending) state <= B_ABORT_STOP;
            else if (guard_wlast_o) state <= B_NEXT_CHUNK;
            else beat <= beat + 3'd1;
          end
        end

        B_NEXT_CHUNK: begin
          // `retired` is NOT touched here. It used to be, and that is exactly
          // what made the old atomicity claim false: handing a chunk downstream
          // is not the SDRAM controller completing the write.
          if (abort_pending) begin
            state <= B_ABORT_STOP;
          end else if (off + 32'(this_len) >= r_len) begin
            off <= r_len;
            state <= B_DECIDE;
          end else begin
            off <= off + 32'(this_len);
            state <= B_READ_REQUEST;
          end
        end

        B_DECIDE: begin
          // Correction 6: the final check and the publish pulse are the SAME
          // edge. A separate publish state leaves a window in which the lease
          // can lapse after being checked and before the pulse goes out.
          if (abort_pending || !lease_ok_now) begin
            if (fail == ST_OK) fail <= ST_LEASE_LOST;
            abort_pending <= 1'b1;
            state <= B_ABORT_STOP;
          end else if (issued != r_len || retired != r_len) begin
            // Every byte issued AND retired. There is no timeout: a blit that
            // never retires is a broken machine, and publishing anyway would
            // hide it behind a picture that looks fine.
            state <= B_DECIDE;
          end else if ((crc_acc ^ 32'hFFFF_FFFF) != r_crc) begin
            fail <= ST_CRC;
            abort_pending <= 1'b1;
            state <= B_ABORT_STOP;
          end else begin
            publish_valid_o <= 1'b1;
            publish_slot_o <= r_slot[0];
            publish_generation_o <= r_gen;
            status_o <= ST_OK;
            done_o <= 1'b1;
            owns_lease <= 1'b0;
            if (blits_published_o != 32'hFFFF_FFFF) begin
              blits_published_o <= blits_published_o + 32'd1;
            end
            state <= B_IDLE;
          end
        end

        B_ABORT_STOP: begin
          // Everything outward has already stopped by construction: no state
          // from here on asserts an HPS request, a guard request or a write
          // beat. What remains is a burst that was already granted.
          // ENFORCED-BY: tests/debug/debug_frameblit_directed.cpp:side_effect_after_lease_loss
          abort_pending <= 1'b1;
          if (hps_inflight) begin
            if (hps_rsp_i.err) hps_inflight <= 1'b0;
            else if (hps_rsp_i.beat_valid && hps_rsp_i.last) hps_inflight <= 1'b0;
          end else begin
            state <= B_ABORT_DRAIN;
          end
        end

        B_ABORT_DRAIN: begin
          // Correction 4: a slot released while writes are still in flight can
          // be leased to somebody else and then overwritten by this dead
          // transaction's bytes. Nothing touches the lease until every write
          // this transaction got accepted has actually retired.
          //
          // A failure before any write was issued passes straight through --
          // both counters are zero.
          if (retired >= issued) state <= B_RELEASE;
        end

        B_RELEASE: begin
          // Correction 5: release only what we actually own. A bad length, a
          // missing lease or a slot mismatch never acquired ownership, and
          // releasing on them would free a lease belonging to somebody else.
          if (owns_lease) begin
            release_valid_o <= 1'b1;
            release_slot_o <= r_slot[0];
            release_generation_o <= r_gen;
          end
          owns_lease <= 1'b0;
          status_o <= fail;
          done_o <= 1'b1;
          if (blits_rejected_o != 32'hFFFF_FFFF) blits_rejected_o <= blits_rejected_o + 32'd1;
          state <= B_IDLE;
        end

        default: state <= B_IDLE;
      endcase
    end
  end

`ifdef FORMAL
  // ---------------------------------------------------------------------------
  // THE SAFETY PROPERTIES (reports/DEBUG.FRAMEBLIT_Integration_Corrections.md 13)
  // ---------------------------------------------------------------------------
  // Every one is a safety property over a small state machine, so all are within
  // reach of a bounded proof. The three that matter are the first three: a
  // publication implies a live matching lease, every byte issued AND retired,
  // and a CRC that passed.
  //
  // The covers at the bottom are not decoration. Each assertion here is an
  // IMPLICATION, and a model that cannot raise the antecedent satisfies it while
  // proving nothing. `c_publish` is reachable ONLY because the harness sets
  // FORMAL_CANVAS_BYTES.
  logic f_past_valid = 1'b0;
  always_ff @(posedge clk) f_past_valid <= 1'b1;

  // The machine starts from a REAL reset. Without this the model begins with
  // every register unconstrained -- including `state` and `fail` -- and the
  // first counterexample is a fabricated state that publishes out of nowhere,
  // which says nothing about the design. Once released, reset stays released.
  always_ff @(posedge clk) begin
    if (!f_past_valid) assume (!rst_n);
    if (f_past_valid && $past(rst_n)) assume (rst_n);
  end

  // Reset leaves nothing half-done and, above all, publishes nothing.
  always_ff @(posedge clk) begin
    if (f_past_valid && !$past(rst_n)) begin
      a_rst_no_publish: assert (!publish_valid_o);
      a_rst_no_release: assert (!release_valid_o);
      a_rst_no_guard:   assert (!guard_req_o.valid);
      a_rst_no_hps:     assert (!hps_req_o.valid);
      a_rst_no_wdata:   assert (!guard_wvalid_o);
    end
  end

  always_ff @(posedge clk) begin
    if (f_past_valid && rst_n) begin
      // ---- publication ----------------------------------------------------
      if (publish_valid_o) begin
        a_pub_lease:   assert ($past(lease_ok_now));
        a_pub_slot:    assert (publish_slot_o == r_slot[0]);
        a_pub_gen:     assert (publish_generation_o == r_gen);
        a_pub_issued:  assert ($past(issued) == $past(r_len));
        a_pub_retired: assert ($past(retired) == $past(r_len));
        a_pub_crc:     assert (($past(crc_acc) ^ 32'hFFFF_FFFF) == $past(r_crc));
        a_pub_nofail:  assert ($past(fail) == ST_OK);
        a_pub_noabort: assert (!$past(abort_pending));
      end

      // ---- release ---------------------------------------------------------
      if (release_valid_o) begin
        // It released only what it had actually acquired, and only after every
        // accepted write had retired.
        a_rel_owned:   assert ($past(owns_lease));
        a_rel_drained: assert ($past(retired) >= $past(issued));
        a_rel_slot:    assert (release_slot_o == r_slot[0]);
        a_rel_gen:     assert (release_generation_o == r_gen);
      end

      // The two terminal events are exclusive: one transaction, one outcome.
      a_excl: assert (!(publish_valid_o && release_valid_o));

      // ---- the guard request ----------------------------------------------
      if (guard_req_o.valid) begin
        a_gr_lease: assert (lease_ok_now);
        a_gr_abort: assert (!abort_pending);
        // Absolute, and inside the leased slot's window.
        a_gr_lo: assert (32'(guard_req_o.addr) >= fb_base);
        a_gr_hi: assert ((32'(guard_req_o.addr) + 32'(guard_req_o.len)) <= (fb_base + r_len));
      end

      // ---- write data is held while the consumer stalls --------------------
      if ($past(guard_wvalid_o) && !$past(guard_wready_i) && guard_wvalid_o) begin
        a_w_data: assert (guard_wdata_o == $past(guard_wdata_o));
        a_w_last: assert (guard_wlast_o == $past(guard_wlast_o));
      end

      // ---- lease loss stops NEW side effects -------------------------------
      // Not "stops side effects instantly": a request already on the wire is
      // held until the bridge takes it, and a write beat already offered is
      // allowed to finish, because withdrawing either mid-handshake is its own
      // corruption. What must never happen is ENTERING those states afresh.
      if ($past(abort_pending) && (state == B_READ_REQUEST)) begin
        a_no_new_hps: assert ($past(state) == B_READ_REQUEST);
      end
      if ($past(abort_pending) && (state == B_WRITE_CHUNK)) begin
        a_no_new_wbeat: assert ($past(state) == B_WRITE_CHUNK);
      end
      if (abort_pending) begin
        a_abort_no_publish: assert (!publish_valid_o);
      end

      // ---- V19 scope guard -------------------------------------------------
      // This proof covers a SINGLE-CHUNK transaction. The multi-chunk loop
      // (B_NEXT_CHUNK -> B_READ_REQUEST with `off` advancing, and retirement
      // credits arriving for chunk N while chunk N+1 is being read) is NOT in
      // the cone at FORMAL_CANVAS_BYTES = 64.
      //
      // Raising the canvas to cover it makes this assertion FIRE, which is the
      // point: the depth above was chosen for one chunk, and a wider canvas
      // needs a re-justified depth rather than a quietly larger number.
      if (owns_lease) begin
        a_scope_single_chunk: assert (r_len <= 32'(CHUNK_BYTES));
      end
    end
  end

  // ---- non-vacuity covers (V16: the covers must prove the antecedents) -----
  always_ff @(posedge clk) begin
    if (f_past_valid && rst_n) begin
      c_publish:  cover (publish_valid_o);
      c_release:  cover (release_valid_o);
      c_guard:    cover (guard_req_o.valid);
      c_wbeat:    cover (guard_wvalid_o);
      c_wstall:   cover (guard_wvalid_o && !guard_wready_i);
      c_abort:    cover (abort_pending);
      c_crc_fail: cover (release_valid_o && (fail == ST_CRC));
      c_drain:    cover (state == B_ABORT_DRAIN);
    end
  end
`endif

endmodule : zhao_debug_frameblit
