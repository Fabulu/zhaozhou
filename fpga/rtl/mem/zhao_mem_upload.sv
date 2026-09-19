// zhao_mem_upload.sv -- MEM.UPLOAD: the HPS->VRAM resource upload engine.
//
// WHY THIS BLOCK EXISTS. design/contracts/MEM.UPLOAD.md opens with the finding
// that made it necessary: "there is no path for a byte to get from where a
// resource lives to where the renderer can read it." CMD.DMA fetches sealed
// frame packets and since 2026-08-21 performs "no blit engine and no VRAM writes
// at all"; SW.STREAM is HPS-side and never crosses the bridge; DEBUG.FRAMEBLIT
// is the only block that reads HPS and writes SDRAM, and it is the right SHAPE
// and the wrong BLOCK -- a debug instrument, canvas-length locked, holding the
// framebuffer lease.
//
// It is deliberately GENERAL. Three consumers needed the identical service --
// creature clip banks, 21,376-byte terrain pages, and Sunder -- and all three
// were competing for ENGINE client 6. One engine with a tagged request resolves
// that contention instead of ratifying it.
//
// ---------------------------------------------------------------------------
// THE ORACLE IS THE LAW, INCLUDING ITS ORDER
// ---------------------------------------------------------------------------
// `reference/include/zref/zref_mem_upload.hpp` was written BEFORE this RTL, and
// `upload_verdict()` there is transcribed below gate for gate. Two things in it
// are easy to get wrong and are therefore stated here as well:
//
//   * THE ORDER IS PART OF THE LAW. "A malformed request is reported as
//     malformed even when it is ALSO stale, so a producer bug is never hidden
//     behind an epoch that happened to close." The epoch test is therefore LAST,
//     not first, even though it is the cheapest.
//
//   * A REFUSAL IS NOT A CLAMP. Every outcome is a verdict, never a corrected
//     value. "A clamped address writes real bytes into a real slot belonging to
//     something else, and nothing downstream can tell."
//
// CONTAINMENT IS CHECKED IN 33 BITS, not 32, and that is the oracle's reasoning
// carried across: `vram_addr + length` in 32 bits can WRAP, and a wrapped sum
// compares as a small number -- so a request running off the end of the arena
// would read as comfortably inside it. The oracle uses uint64; the widths here
// are one bit wider than the operands, which is the same guarantee.
//
// ---------------------------------------------------------------------------
// WHY THE SOURCE ADDRESS IS 64 BITS ON A 32-BIT BRIDGE
// ---------------------------------------------------------------------------
// `zhao_hps_burst_req_t.addr` is 32 bits. A descriptor may legally carry a wider
// HPS address, and the oracle's `kUploadSourceUnreachable` exists precisely so
// that such a descriptor is REFUSED rather than silently narrowed into a
// legal-looking 32-bit one. The upper half is tested BEFORE anything else looks
// at the address, exactly as the oracle does.
//
// ---------------------------------------------------------------------------
// ATOMICITY: THE DESTINATION IS A FRESH SLOT, AND THAT IS NOT THIS BLOCK'S CHOICE
// ---------------------------------------------------------------------------
// The contract's first law was CORRECTED on 2026-09-03 and the correction is the
// whole design: a generation counter alone does NOT make an in-place upload
// atomic, because new bytes written over the old slot while it still advertises
// the old generation let a consumer read a MIXTURE. "The generation does not
// protect the underlying memory."
//
// So `req_dst_slot_i` is a FRESH, unpinned, unpublished slot handed over by the
// allocator, and PUBLICATION IS A MAPPING UPDATE rather than a byte copy. This
// block never writes to a published slot, so a failed upload leaves the old
// mapping and old bytes untouched by construction rather than by care.
//
// What that costs the rest of the system is recorded in the contract and is not
// optional: "the arena needs at least one free slot beyond the working set."
// This block does NOT allocate -- residency policy is SW.STREAM's and residency
// state is TERRAIN.RESIDENCY's -- so it cannot enforce that, and a caller that
// hands it a slot somebody is reading will corrupt that reader. The allocator
// owns it; this comment exists so nobody discovers the coupling from a bug.
//
// A half-uploaded clip page is bytes that LOOK like animation: the decoder would
// read them, produce a pose, and draw a creature bent into a shape no artist
// authored, with nothing anywhere reporting an error.
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK DELIBERATELY DOES NOT DO
// ---------------------------------------------------------------------------
//   * NO CACHE INVALIDATION. Contract 3 is emphatic that publication must make
//     stale cache lines unable to match, and owner ruling D-3 chose the
//     mechanism: GENERATION-TAGGED CACHES, "cache tag = physical line tag +
//     residency generation". That is a change to each CACHE, not work for the
//     uploader -- and the ruling says why, in as many words: "correctness may
//     not depend on an uploader remembering an ad hoc invalidate list." So this
//     block publishes the generation and the caches are required to include it
//     in their identity. `publish_generation_o` is what they key on.
//   * NO COMPRESSION OR TRANSFORM. "Bytes arrive as they were staged."
//   * NO ALLOCATION AND NO EVICTION.
//   * NO FRAMEBUFFER WRITES.
//
// ---------------------------------------------------------------------------
// THE PUBLICATION IS A DIRECTORY ROW (spec/memory_rules.md 5f.1, 2026-09-19)
// ---------------------------------------------------------------------------
// Until this commit the publication was `{slot, generation, tag}`, and
// `zhao_material_resolve.sv`'s header said of it, correctly, "a slot and a
// generation, carrying NEITHER a base address, NOR an extent, NOR a resource
// kind. A directory cannot be built from it."
//
// Two thirds of that was never a missing VALUE -- it was a dropped one. This
// block already TAKES `req_vram_addr_i` and `req_len_i` and bounds-checks both
// against `cfg_region_*` in `in_guard_c` before a byte moves, and the resource
// kind already travelled as `publish_tag_o`. So base and extent are now two
// output ports carrying quantities this block had already validated.
//
// What was genuinely absent is the KEY, and owner ruling 5f.1 supplies it: **a
// published slot is named by the handle index of the resource it holds.** That
// makes `{index:24}` the directory key and `{slot, base, extent, kind}` its
// row, so `req_index_i` now rides the request and `publish_index_o` names the
// row. Nothing else here carries a 24-bit resource name and nothing could
// derive one -- see `req_index_i`'s own comment.
//
// THIS BLOCK STILL OWNS NO LAYOUT. It publishes where the bytes went; it does
// not say how the region is carved up, which 5f leaves open and 5f.1 does not
// close.
//
// ---------------------------------------------------------------------------
// ONE BURST IN FLIGHT, ON PURPOSE
// ---------------------------------------------------------------------------
// This is a BACKGROUND client (contract: "No blocking of the render path... a
// resource that is not resident in time is a deadline fault, never a stall"), so
// latency hiding here buys nothing the render path can spend, while a second
// outstanding burst would need a reorder buffer and a second CRC context. The
// serial shape is a considered choice against a measured need, not an oversight;
// if a deadline ever proves it insufficient, the fix is depth here and the
// evidence should be a missed deadline rather than an intuition.

`default_nettype none

module zhao_mem_upload
  import zhao_pkg::*;
#(
    // 64 bytes because that is the shape the console already moves HPS bytes in
    // (DEBUG.FRAMEBLIT). One transfer granularity, not two.
    // zref::mem::kUploadBurstBytes.
    parameter int unsigned BURST_BYTES = 64,
    parameter int unsigned CENSUS_W    = 16
) (
    input  var logic clk,
    input  var logic rst_n,

    // -----------------------------------------------------------------------
    // the tagged upload request
    // -----------------------------------------------------------------------
    input  var logic        req_valid_i,
    output var logic        req_ready_o,
    input  var logic [ 7:0] req_tag_i,        // which consumer asked; echoed back
    // THE RESOURCE'S OWN NAME -- `spec/memory_rules.md` 5f.1, ruled 2026-09-19:
    // "a published slot is named by the handle index of the resource it holds".
    // This is the 24-bit index half of the game's `handle32 {index:24,
    // generation:8}`, and it is a SEPARATE port because nothing else this block
    // holds is that name: `req_tag_i` is 8 bits of "which consumer asked" and
    // `req_dst_slot_i` is 8 bits of arena slot. A publication without it names a
    // directory row nobody can look up, which is precisely why MATERIAL.RESOLVE
    // sat built and uncomposed for sixteen days after its cartridge blocker died.
    //
    // It is CARRIED, never consulted. This block has no opinion about resource
    // naming and forms no address from it -- inventing one here would be the
    // layout ruling 5f explicitly still leaves open.
    input  var logic [23:0] req_index_i,
    input  var logic [63:0] req_hps_addr_i,   // may exceed 32 bits: then REFUSED
    input  var logic [31:0] req_vram_addr_i,
    input  var logic [31:0] req_len_i,
    input  var logic [15:0] req_epoch_i,
    input  var logic [ 7:0] req_dst_slot_i,   // FRESH, unpinned, unpublished
    input  var logic [15:0] req_new_gen_i,
    input  var logic [31:0] req_crc_i,        // expected CRC-32C over the payload

    // -----------------------------------------------------------------------
    // configuration: the destination region MEM.GUARD declares, and the HPS
    // staging arena the ACTIVE EPOCH registered.
    //
    // The arena is not decoration. The oracle gained `kUploadSourceOutsideArena`
    // on 2026-09-03 because the first version checked the DESTINATION and not
    // the source -- "a capability hole rather than a missing bounds check. The
    // bridge can see a broad HPS range; a descriptor may read only from the
    // staging arena registered for the active epoch."
    // -----------------------------------------------------------------------
    input  var logic [31:0] cfg_region_base_i,
    input  var logic [31:0] cfg_region_bytes_i,
    input  var logic [63:0] cfg_arena_base_i,
    input  var logic [31:0] cfg_arena_bytes_i,
    input  var logic [15:0] cfg_epoch_i,

    // -----------------------------------------------------------------------
    // HPS read side
    // -----------------------------------------------------------------------
    output var zhao_hps_burst_req_t hps_req_o,
    input  var logic                hps_req_grant_i,
    input  var zhao_hps_burst_rsp_t hps_rsp_i,

    // -----------------------------------------------------------------------
    // local-SDRAM write side, through MEM.GUARD
    // -----------------------------------------------------------------------
    output var zhao_guard_req_t guard_req_o,
    input  var zhao_guard_rsp_t guard_rsp_i,
    output var logic [63:0]     guard_wdata_o,
    output var logic            guard_wvalid_o,
    input  var logic            guard_wready_i,
    output var logic            guard_wlast_o,
    input  var logic [ 7:0]     retire_words_i,

    // -----------------------------------------------------------------------
    // publication -- a MAPPING update, one pulse, only after every write retired
    // and the CRC matched
    // -----------------------------------------------------------------------
    // THE PUBLICATION IS A DIRECTORY ROW, `spec/memory_rules.md` 5f.1:
    //
    //     key   {index:24}
    //     row   {slot:8, base:32, extent:32, kind:8}
    //
    // `publish_index_o` is the key; `publish_tag_o` IS the kind (`spec/
    // cartridge.md` 4a's .zpak kinds, owner ruling D-2); base and extent are
    // the destination range this block ALREADY bounds-checked against
    // `cfg_region_*` in `in_guard_c` above, so a consumer cannot be handed a
    // range MEM.GUARD would refuse. That check is why these two are output
    // ports and not a new obligation: the values were validated and then
    // dropped, which is a strictly smaller repair than it looked like.
    //
    // THEY ARE THE REQUEST'S, NOT THE WALKER'S. `dst_q` advances by
    // BURST_BYTES on every burst, so it holds the LAST burst's address by the
    // time publication fires; `base_q`/`extent_q` are captured whole on accept.
    // Publishing `dst_q` would name a row 64 bytes short of the resource with
    // nothing anywhere able to see it -- the wrong-surface fault in miniature.
    output var logic        publish_valid_o,
    output var logic [ 7:0] publish_slot_o,
    output var logic [15:0] publish_generation_o,
    output var logic [ 7:0] publish_tag_o,
    output var logic [23:0] publish_index_o,
    output var logic [31:0] publish_base_o,
    output var logic [31:0] publish_extent_o,

    // -----------------------------------------------------------------------
    // verdict and census
    // -----------------------------------------------------------------------
    output var logic        done_o,    // one pulse per retired request
    output var logic [ 7:0] status_o,  // zref::mem::UploadVerdict
    output var logic [CENSUS_W-1:0] uploads_published_o,
    // One SATURATING counter per refusal reason, INDEXED BY THE ORACLE'S ENUM
    // VALUE so the two cannot drift: bit slice 16*k is reason k. Index 0
    // (kUploadOk) is never incremented and is held at zero deliberately -- the
    // published count is its own counter, and a slot that means "not a refusal"
    // keeps the indexing honest rather than off by one.
    output var logic [8*CENSUS_W-1:0] refused_o
);

  // zref::mem::UploadVerdict, transcribed. Same values, same names.
  localparam logic [7:0] V_OK                   = 8'd0;
  localparam logic [7:0] V_UNALIGNED            = 8'd1;
  localparam logic [7:0] V_ZERO_LENGTH          = 8'd2;
  localparam logic [7:0] V_OUTSIDE_GUARD        = 8'd3;
  localparam logic [7:0] V_EPOCH_STALE          = 8'd4;
  localparam logic [7:0] V_CRC_FAIL             = 8'd5;
  localparam logic [7:0] V_SOURCE_OUTSIDE_ARENA = 8'd6;
  localparam logic [7:0] V_SOURCE_UNREACHABLE   = 8'd7;

  localparam int unsigned BEAT_BYTES = 8;                      // 64-bit bus
  localparam int unsigned BEATS_PER_BURST = BURST_BYTES / BEAT_BYTES;

  initial begin
    if (BURST_BYTES != 64) begin
      $fatal(1, "zhao_mem_upload: zref::mem::kUploadBurstBytes is 64");
    end
    if (CENSUS_W < 1) begin
      $fatal(1, "zhao_mem_upload: CENSUS_W must be positive");
    end
  end

  // ==========================================================================
  // THE VERDICT -- zref::mem::upload_verdict, gate for gate and IN ITS ORDER
  // ==========================================================================
  wire aligned_c =
         (req_hps_addr_i[5:0]  == 6'd0) &&
         (req_vram_addr_i[5:0] == 6'd0) &&
         (req_len_i[5:0]       == 6'd0);

  // 33-bit sums: a 32-bit sum can wrap and then compare as comfortably inside.
  wire [32:0] dst_lo_c = {1'b0, req_vram_addr_i};
  wire [32:0] dst_hi_c = dst_lo_c + {1'b0, req_len_i};
  wire [32:0] reg_lo_c = {1'b0, cfg_region_base_i};
  wire [32:0] reg_hi_c = reg_lo_c + {1'b0, cfg_region_bytes_i};
  wire in_guard_c = (dst_lo_c >= reg_lo_c) && (dst_hi_c <= reg_hi_c);

  wire [64:0] src_lo_c = {1'b0, req_hps_addr_i};
  wire [64:0] src_hi_c = src_lo_c + {33'b0, req_len_i};
  wire [64:0] arn_lo_c = {1'b0, cfg_arena_base_i};
  wire [64:0] arn_hi_c = arn_lo_c + {33'b0, cfg_arena_bytes_i};
  wire in_arena_c = (src_lo_c >= arn_lo_c) && (src_hi_c <= arn_hi_c);

  logic [7:0] verdict_c;
  always_comb begin
    if (req_len_i == 32'd0)                verdict_c = V_ZERO_LENGTH;
    else if (!aligned_c)                   verdict_c = V_UNALIGNED;
    else if (req_hps_addr_i[63:32] != '0)  verdict_c = V_SOURCE_UNREACHABLE;
    else if (!in_arena_c)                  verdict_c = V_SOURCE_OUTSIDE_ARENA;
    else if (!in_guard_c)                  verdict_c = V_OUTSIDE_GUARD;
    else if (req_epoch_i != cfg_epoch_i)   verdict_c = V_EPOCH_STALE;
    else                                   verdict_c = V_OK;
  end

  // ==========================================================================
  // STATE
  // ==========================================================================
  localparam logic [2:0] S_IDLE    = 3'd0;
  localparam logic [2:0] S_ISSUE   = 3'd1;  // offer the HPS burst
  localparam logic [2:0] S_MOVE    = 3'd2;  // rsp beats -> guard writes
  localparam logic [2:0] S_NEXT    = 3'd3;  // advance or finish
  localparam logic [2:0] S_RETIRE  = 3'd4;  // every VRAM write must land
  localparam logic [2:0] S_VERIFY  = 3'd5;  // CRC, then publish or discard
  localparam logic [2:0] S_REPORT  = 3'd6;  // one done pulse

  logic [2:0]  st_q;
  logic [31:0] src_q;        // current burst's HPS address
  logic [31:0] dst_q;        // current burst's VRAM address
  logic [31:0] bursts_left_q;
  logic [31:0] crc_q;
  logic [ 7:0] tag_q, slot_q;
  logic [23:0] index_q;      // 5f.1's directory key, carried from the request
  logic [31:0] base_q;       // the request's vram address, before dst_q walks
  logic [31:0] extent_q;     // the request's length, in bytes
  logic [15:0] gen_q;
  logic [31:0] exp_crc_q;
  logic [ 7:0] status_q;
  logic [$clog2(BEATS_PER_BURST+1)-1:0] beat_q;
  logic [15:0] outstanding_q;   // writes offered but not yet retired
  logic        hps_err_q;
  // MEM.GUARD denied a write. `zhao_guard_rsp_t.violation` means "request
  // denied (dropped; NOTHING was written)", so ignoring it would leave a HOLE in
  // the copy and then publish it -- bytes that look like a resource, which is
  // the exact failure this contract exists to prevent. The first draft of this
  // block did ignore it, and Verilator's UNUSEDSIGNAL on `guard_rsp_i` is what
  // said so; a port carried and never read is a check nobody is performing.
  logic        guard_denied_q;
  // The bridge's own end-of-burst against our beat count. These must agree, and
  // a disagreement means the bridge and this block disagree about how many bytes
  // moved -- which would silently shorten or lengthen the copy.
  logic        beat_mismatch_q;

  // ---- HPS request ---------------------------------------------------------
  always_comb begin
    hps_req_o        = '0;
    hps_req_o.valid  = (st_q == S_ISSUE);
    hps_req_o.write  = 1'b0;
    hps_req_o.client = ZHAO_CLIENT_TERRAIN_BUILD;  // ruling T3: background class
    hps_req_o.addr   = src_q;
    hps_req_o.len    = 7'(BURST_BYTES);
  end

  // ---- guard write ---------------------------------------------------------
  // One guard request per burst, offered with the burst's first beat; the data
  // beats follow on the write channel. Byte enables are all ones because a
  // refusal above has already guaranteed the length is a whole number of bursts
  // -- there is no partial tail to mask, by construction rather than by luck.
  always_comb begin
    guard_req_o        = '0;
    guard_req_o.valid  = (st_q == S_MOVE) && (beat_q == '0) && hps_rsp_i.beat_valid;
    guard_req_o.write  = 1'b1;
    guard_req_o.client = ZHAO_CLIENT_TERRAIN_BUILD;
    guard_req_o.addr   = dst_q[ZHAO_VRAM_ADDR_BITS-1:0];
    guard_req_o.len    = 7'(BURST_BYTES);
    guard_req_o.be     = {64{1'b1}};
  end

  assign guard_wdata_o  = hps_rsp_i.data;
  assign guard_wvalid_o = (st_q == S_MOVE) && hps_rsp_i.beat_valid;
  assign guard_wlast_o  = guard_wvalid_o && (beat_q == ($clog2(BEATS_PER_BURST+1))'(BEATS_PER_BURST - 1));

  assign req_ready_o = (st_q == S_IDLE);

  assign done_o    = (st_q == S_REPORT);
  assign status_o  = status_q;

  assign publish_valid_o      = (st_q == S_REPORT) && (status_q == V_OK);
  assign publish_slot_o       = slot_q;
  assign publish_generation_o = gen_q;
  assign publish_tag_o        = tag_q;
  assign publish_index_o      = index_q;
  assign publish_base_o       = base_q;
  assign publish_extent_o     = extent_q;

  // ==========================================================================
  // OUTSTANDING WRITES -- ONE ASSIGNMENT, AND THAT IS THE WHOLE POINT
  // ==========================================================================
  // This counter was written as two separate non-blocking assignments in the
  // same `always_ff`: a retire subtraction near the top and an increment inside
  // the S_MOVE arm. In SystemVerilog the LAST assignment wins, so on every cycle
  // that wrote a beat AND retired one, the retirement was silently discarded.
  // The counter then only ever grew, S_RETIRE waited on it forever, and no
  // upload ever published.
  //
  // It cost nothing to find only because the bench drives real retirement; a
  // bench that retired everything at the end would have passed. Two writers of
  // one register is the bug, so there is now exactly one, and the increment and
  // the decrement meet as ARITHMETIC where both are visible at once.
  wire took_beat_c = (st_q == S_MOVE) && guard_wvalid_o && guard_wready_i
                  && !guard_rsp_i.violation;
  wire [16:0] out_plus_c   = {1'b0, outstanding_q} + (took_beat_c ? 17'd1 : 17'd0);
  wire [16:0] retire_c     = {9'd0, retire_words_i};
  wire [16:0] out_wide_c   = (out_plus_c > retire_c) ? (out_plus_c - retire_c) : 17'd0;
  // SATURATE rather than truncate. Bit 16 can only be set by more outstanding
  // writes than the counter can hold, and wrapping it to a small number would
  // make S_RETIRE pass IMMEDIATELY -- publishing a mapping to bytes still in
  // flight, which is the one thing the retire wait exists to prevent. Holding
  // at maximum instead stalls forever, which is a visible hang rather than a
  // silent corruption, and that is the correct direction to fail in.
  wire [15:0] out_next_c   = out_wide_c[16] ? 16'hFFFF : out_wide_c[15:0];

  // ---- CRC over the bytes actually written --------------------------------
  wire [31:0] crc_next_w;
  zhao_crc32c_fold u_crc (
      .c_i (crc_q),
      .d_i (hps_rsp_i.data),
      .n_i (4'd8),
      .c_o (crc_next_w)
  );

  // ==========================================================================
  // SEQUENTIAL
  // ==========================================================================
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_q          <= S_IDLE;
      src_q         <= '0;
      dst_q         <= '0;
      bursts_left_q <= '0;
      crc_q         <= 32'hFFFF_FFFF;
      tag_q         <= '0;
      slot_q        <= '0;
      index_q       <= '0;
      base_q        <= '0;
      extent_q      <= '0;
      gen_q         <= '0;
      exp_crc_q     <= '0;
      status_q      <= V_OK;
      beat_q        <= '0;
      outstanding_q <= '0;
      hps_err_q     <= 1'b0;
      guard_denied_q <= 1'b0;
      beat_mismatch_q <= 1'b0;
      uploads_published_o <= '0;
      refused_o     <= '0;
    end else begin
      // ONE writer for outstanding_q; see the comment beside out_next_c.
      outstanding_q <= out_next_c;

      case (st_q)
        S_IDLE: begin
          if (req_valid_i) begin
            tag_q     <= req_tag_i;
            slot_q    <= req_dst_slot_i;
            gen_q     <= req_new_gen_i;
            // ONE ENABLE FOR THE WHOLE ROW, and here that is the property we
            // want rather than the one CLAUDE.md warns about. The warning is
            // about a CHECKER whose two operands move together; this is a
            // RECORD, and {index, slot, base, extent, kind, generation} are six
            // fields of one publication that must never describe two different
            // requests. Captured on the accept edge, all six, unconditionally --
            // a refusal simply never reaches `publish_valid_o`.
            index_q   <= req_index_i;
            base_q    <= req_vram_addr_i;
            extent_q  <= req_len_i;
            exp_crc_q <= req_crc_i;
            status_q  <= verdict_c;
            hps_err_q       <= 1'b0;
            guard_denied_q  <= 1'b0;
            beat_mismatch_q <= 1'b0;
            if (verdict_c == V_OK) begin
              src_q         <= req_hps_addr_i[31:0];
              dst_q         <= req_vram_addr_i;
              // exact, not rounded: an unaligned length was refused above
              // rather than padded, because padding writes bytes the producer
              // never staged. zref::mem::upload_bursts.
              bursts_left_q <= req_len_i >> 6;
              crc_q         <= 32'hFFFF_FFFF;
              beat_q        <= '0;
              st_q          <= S_ISSUE;
            end else begin
              // A REFUSAL IS NOT A CLAMP: nothing is issued, nothing is nudged.
              if (!(&refused_o[16*int'(verdict_c) +: CENSUS_W])) begin
                refused_o[CENSUS_W*int'(verdict_c) +: CENSUS_W]
                    <= refused_o[CENSUS_W*int'(verdict_c) +: CENSUS_W] + 1'b1;
              end
              st_q <= S_REPORT;
            end
          end
        end

        S_ISSUE: begin
          if (hps_req_grant_i) begin
            beat_q <= '0;
            st_q   <= S_MOVE;
          end
        end

        S_MOVE: begin
          if (hps_rsp_i.err) begin
            // The bridge issued nothing. Treat it as a CRC-class failure: the
            // copy is incomplete, so the slot is discarded unpublished. It is
            // NOT reported as OK and it is NOT retried here -- a retry policy
            // belongs to the requester, which knows whether the resource is
            // still wanted.
            hps_err_q <= 1'b1;
            st_q      <= S_RETIRE;
          end else if (guard_rsp_i.violation) begin
            // DENIED by the region check, and `zhao_guard_rsp_t` says plainly
            // that NOTHING was written. The copy would have a hole, so the slot
            // is discarded unpublished.
            //
            // Reported with the GUARD's verdict rather than as a CRC failure:
            // the CRC would also have failed, but saying "bad checksum" about a
            // rejected address sends the next reader to the wrong question.
            guard_denied_q <= 1'b1;
            st_q           <= S_RETIRE;
          end else if (hps_rsp_i.beat_valid) begin
            // THE THREE RESPONSE BITS MEAN THREE DIFFERENT THINGS and collapsing
            // any two of them is a bug. `violation` is a refusal and aborts,
            // above. `ready && ok` is positive acceptance of the burst's
            // request. NEITHER is the guard being BUSY, and that is a HOLD, not
            // a failure -- an earlier draft of this block treated "not ready" as
            // a denial, which would have discarded a perfectly good upload
            // whenever the arbiter was serving someone else.
            //
            // Acceptance is only required on the beat that carries the request
            // (beat 0); afterwards the burst is already accepted and the data
            // channel's own `guard_wready_i` paces it.
            if (guard_wready_i &&
                ((beat_q != '0) || (guard_rsp_i.ready && guard_rsp_i.ok))) begin
              crc_q <= crc_next_w;
              // The bridge's own `last` and our beat counter must agree. They
              // are two independent statements about how many bytes moved, and
              // a copy is exactly as correct as that agreement.
              if (hps_rsp_i.last != (beat_q == ($clog2(BEATS_PER_BURST+1))'(BEATS_PER_BURST - 1))) begin
                beat_mismatch_q <= 1'b1;
              end
              if (hps_rsp_i.last) begin
                beat_q <= '0;
                st_q   <= S_NEXT;
              end else begin
                beat_q <= beat_q + 1'b1;
              end
            end
            // guard not ready: hold. The HPS side must hold the beat too; this
            // block never drops a beat to keep moving.
          end
        end

        S_NEXT: begin
          if (bursts_left_q == 32'd1) begin
            st_q <= S_RETIRE;
          end else begin
            bursts_left_q <= bursts_left_q - 32'd1;
            src_q         <= src_q + 32'(BURST_BYTES);
            dst_q         <= dst_q + 32'(BURST_BYTES);
            st_q          <= S_ISSUE;
          end
        end

        S_RETIRE: begin
          // "wait for every local-SDRAM write to RETIRE" -- contract law 1.
          // Publishing before this point would advertise a mapping to bytes
          // that are still in flight.
          if (outstanding_q == '0) st_q <= S_VERIFY;
        end

        S_VERIFY: begin
          if (guard_denied_q) begin
            // Reported with the GUARD's verdict rather than as a CRC failure.
            // The CRC would also have failed, but saying "bad checksum" about a
            // rejected address sends the next reader to the wrong question.
            status_q <= V_OUTSIDE_GUARD;
            if (!(&refused_o[CENSUS_W*int'(V_OUTSIDE_GUARD) +: CENSUS_W])) begin
              refused_o[CENSUS_W*int'(V_OUTSIDE_GUARD) +: CENSUS_W]
                  <= refused_o[CENSUS_W*int'(V_OUTSIDE_GUARD) +: CENSUS_W] + 1'b1;
            end
          end else if (hps_err_q || beat_mismatch_q || (~crc_q != exp_crc_q)) begin
            status_q <= V_CRC_FAIL;
            if (!(&refused_o[CENSUS_W*int'(V_CRC_FAIL) +: CENSUS_W])) begin
              refused_o[CENSUS_W*int'(V_CRC_FAIL) +: CENSUS_W]
                  <= refused_o[CENSUS_W*int'(V_CRC_FAIL) +: CENSUS_W] + 1'b1;
            end
          end else begin
            status_q <= V_OK;
            if (!(&uploads_published_o)) begin
              uploads_published_o <= uploads_published_o + 1'b1;
            end
          end
          st_q <= S_REPORT;
        end

        default: begin  // S_REPORT, one cycle
          st_q <= S_IDLE;
        end
      endcase
    end
  end

endmodule

`default_nettype wire
