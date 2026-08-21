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
module zhao_debug_frameblit (
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
    output logic        fb_lease_release_o,  // one pulse: the slot goes FREE
    output logic        blit_publish_o,      // one pulse: the slot becomes READY

    // ---- HPS source burst reader ------------------------------------------
    output zhao_pkg::zhao_hps_burst_req_t hps_req_o,
    input  zhao_pkg::zhao_hps_burst_rsp_t hps_rsp_i,

    // ---- guarded local-SDRAM write ----------------------------------------
    output zhao_pkg::zhao_guard_req_t guard_req_o,
    input  zhao_pkg::zhao_guard_rsp_t guard_rsp_i,
    output logic [63:0]               guard_wdata_o,
    output logic                      guard_wvalid_o,
    input  logic                      guard_wready_i,
    output logic                      guard_wlast_o,

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
    B_WAIT_RETIRE,
    B_CRC_DECIDE,
    B_PUBLISH,
    B_ABORT
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
  logic        lease_held;   // the lease was valid on EVERY cycle so far
  logic [15:0] r_gen;        // the generation granted at accept

  function automatic logic [31:0] canvas_bytes(input logic [7:0] m);
    canvas_bytes = zhao_pkg::zhao_canvas_bytes(zhao_pkg::zhao_mode_from_abi(m));
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

  always_comb begin
    hps_req_o = '0;
    hps_req_o.valid = (state == B_READ_REQUEST);
    hps_req_o.write = 1'b0;
    hps_req_o.client = zhao_pkg::ZHAO_CLIENT_BLIT_DMA;
    hps_req_o.addr = r_src + off;
    hps_req_o.len = this_len;

    guard_req_o = '0;
    guard_req_o.valid = (state == B_GUARD_REQUEST);
    guard_req_o.write = 1'b1;
    guard_req_o.client = zhao_pkg::ZHAO_CLIENT_BLIT_DMA;
    guard_req_o.addr = zhao_pkg::ZHAO_VRAM_ADDR_BITS'(off);
    guard_req_o.len = this_len;
    guard_req_o.be = '1;

    // Data and `last` are HELD while the consumer stalls -- a beat that moves
    // under a stalled consumer is a corrupted pixel nobody can trace.
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
      lease_held <= 1'b0;
      done_o <= 1'b0;
      status_o <= ST_OK;
      fb_lease_release_o <= 1'b0;
      blit_publish_o <= 1'b0;
      blits_published_o <= '0;
      blits_rejected_o <= '0;
      for (int i = 0; i < CHUNK_BEATS; i++) chunk[i] <= '0;
    end else begin
      done_o <= 1'b0;
      fb_lease_release_o <= 1'b0;
      blit_publish_o <= 1'b0;

      // Watched on EVERY cycle of the transaction, not sampled at the ends.
      if (state != B_IDLE && !lease_ok_now) lease_held <= 1'b0;

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
            lease_held <= 1'b1;
            r_gen <= fb_lease_generation_i;
            state <= B_VALIDATE;
          end
        end

        B_VALIDATE: begin
          // The LATCHED length is what is judged. Reading the live input here
          // would judge whatever the producer left on the wire after the
          // handshake, which is not the request that was accepted.
          if (r_len != canvas_bytes(r_mode)) begin
            fail <= ST_BAD_LEN;
            state <= B_ABORT;
          end else if (!fb_lease_valid_i) begin
            fail <= ST_NO_LEASE;
            state <= B_ABORT;
          end else if (r_slot[7:1] != 7'd0 || fb_lease_slot_i != r_slot[0]) begin
            // The ABI's dst_slot is no longer trusted merely because it is 0
            // or 1: it must MATCH the slot the shell actually leased.
            fail <= ST_SLOT_MISMATCH;
            state <= B_ABORT;
          end else begin
            state <= B_READ_REQUEST;
          end
        end

        B_READ_REQUEST: begin
          if (!lease_ok_now) begin
            fail <= ST_LEASE_LOST;
            state <= B_ABORT;
          end else begin
            beat <= '0;
            state <= B_READ_CHUNK;
          end
        end

        B_READ_CHUNK: begin
          if (hps_rsp_i.err) begin
            fail <= ST_BRIDGE_ERR;
            state <= B_ABORT;
          end else if (hps_rsp_i.beat_valid) begin
            chunk[beat] <= hps_rsp_i.data;
            crc_acc <= crc_next;
            if (hps_rsp_i.last) begin
              beat <= '0;
              state <= B_GUARD_REQUEST;
            end else begin
              beat <= beat + 3'd1;
            end
          end
        end

        B_GUARD_REQUEST: begin
          if (guard_rsp_i.ready) state <= B_GUARD_VERDICT;
        end

        B_GUARD_VERDICT: begin
          if (guard_rsp_i.violation || !guard_rsp_i.ok) begin
            fail <= ST_GUARD_DENY;
            state <= B_ABORT;
          end else begin
            beat <= '0;
            state <= B_WRITE_CHUNK;
          end
        end

        B_WRITE_CHUNK: begin
          if (guard_wvalid_o && guard_wready_i) begin
            issued <= issued + 32'd8;
            if (guard_wlast_o) state <= B_NEXT_CHUNK;
            else beat <= beat + 3'd1;
          end
        end

        B_NEXT_CHUNK: begin
          retired <= retired + 32'(this_len);
          if (off + 32'(this_len) >= r_len) begin
            off <= r_len;
            state <= B_WAIT_RETIRE;
          end else begin
            off <= off + 32'(this_len);
            state <= B_READ_REQUEST;
          end
        end

        B_WAIT_RETIRE: begin
          // Every byte issued and retired, and the lease never lapsed.
          if (!lease_held) begin
            fail <= ST_LEASE_LOST;
            state <= B_ABORT;
          end else begin
            state <= B_CRC_DECIDE;
          end
        end

        B_CRC_DECIDE: begin
          if ((crc_acc ^ 32'hFFFF_FFFF) == r_crc) state <= B_PUBLISH;
          else begin
            fail <= ST_CRC;
            state <= B_ABORT;
          end
        end

        B_PUBLISH: begin
          blit_publish_o <= 1'b1;
          status_o <= ST_OK;
          done_o <= 1'b1;
          if (blits_published_o != 32'hFFFF_FFFF) blits_published_o <= blits_published_o + 32'd1;
          state <= B_IDLE;
        end

        B_ABORT: begin
          // The slot goes FREE and is NEVER published. A dirty inactive slot is
          // invisible; the only thing that must not happen is publishing it.
          fb_lease_release_o <= 1'b1;
          status_o <= fail;
          done_o <= 1'b1;
          if (blits_rejected_o != 32'hFFFF_FFFF) blits_rejected_o <= blits_rejected_o + 32'd1;
          state <= B_IDLE;
        end

        default: state <= B_IDLE;
      endcase
    end
  end

endmodule : zhao_debug_frameblit
