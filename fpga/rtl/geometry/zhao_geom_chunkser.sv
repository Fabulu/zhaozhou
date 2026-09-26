// zhao_geom_chunkser.sv -- GEOM.PARAMBUF's TILE-REFERENCE-CHUNK SERIALISER.
//
// Console entry I54. This block is the translation between two objects that
// are NOT the same object, and the entry is right about that:
//
//   `zhao_geom_binner_v2`'s arena is `ref_ram [0:(CHUNKS*CHUNK_REFS)-1]` with
//   CHUNKS = 256, CHUNK_REFS = 4 and a ref of TRI_W = SEVEN BITS, chained by
//   `next_ram [0:CHUNKS-1]`. R7's chunk is 64 bytes: a u32 `next`, a u16
//   count, a u16 generation and FOURTEEN u32 ids.
//
// Four 7-bit refs is a different object from fourteen 32-bit ids. This block
// re-aggregates one into the other, fourteen at a time, in the binner's own
// per-tile reference order, and hands each chunk to `zhao_geom_paramarena`.
//
// ---------------------------------------------------------------------------
// WHAT IT DOES *NOT* DO, AND THE BRIEF THAT SAID IT DID
// ---------------------------------------------------------------------------
// IT DOES NOT STAMP THE GENERATION. The packet brief and entry I54 both say
// the serialiser "stamps the generation"; MEASURED AGAINST THE ARENA, that is
// false and it is false in the dangerous direction, because a producer that
// stamped it would be reintroducing the exact defect the field exists to
// catch. `zhao_geom_paramarena.sv` puts `gen_q` into chunk bytes 6..7 itself
// and says why in as many words:
//
//   "THE GENERATION IS STAMPED HERE AND NOT SUPPLIED BY THE CALLER. A producer
//    that took the generation as an input would let a carried-over chunk keep
//    last frame's stamp -- which is the one thing the field exists to make
//    detectable."
//
// So this block has no generation port and must not grow one.
//
// ---------------------------------------------------------------------------
// WHY IT READS A SECOND PASS AND NOT THE LIVE DRAIN
// ---------------------------------------------------------------------------
// The obvious composition is to watch `zhao_geom_binner_v2`'s job drain go by.
// IT IS UNSOUND, AND THE ARITHMETIC IS WHY.
//
// The drain costs two cycles per tile of the grid plus about four per emitted
// job. A grid of 576 tiles each holding exactly ONE reference therefore drains
// in ~3,456 cycles while requiring 576 chunks -- 36,864 bytes, 4,608 64-bit
// beats through the guard, which cannot be issued in fewer than 4,608 cycles
// even with the socket entirely to ourselves. The producer is structurally
// slower than the stream it would be watching, so NO FIFO depth makes a
// passive tap correct: it makes the overflow rarer and keeps it silent.
//
// The two ways out are (a) backpressure the drain, which puts this block in
// the live raster path and changes RASTER's throughput -- the precise change
// entry I55 declines to make, and not this packet's to make either -- and
// (b) read the lists AGAIN, after the raster drain has finished with them.
//
// (b) is what this is. The binner's `tile_ram`, `ref_ram` and `next_ram` are
// written only at frame_begin (clear) and at push, so THE LISTS ARE STILL
// INTACT when the drain ends: a second walk is a pure read. `ser_req_o` asks
// for that walk, and this block can stall it as hard as it likes, because
// nothing downstream of it is drawing a picture.
//
// THE COST IS DECLARED: one extra whole-grid scan per frame, 720 cycles of a
// 251,520-cycle frame (0.3%), entirely outside the raster path, plus whatever
// the arena's socket takes. The raster drain's timing is UNCHANGED -- this
// block is not a term in `job_ready_i`.
//
// ---------------------------------------------------------------------------
// THE CHAIN, AND WHY `next` IS NOT A SECOND COUNTER
// ---------------------------------------------------------------------------
// R7's `next_chunk` is a chunk INDEX (`zhao_geom_parambuf` refuses one at or
// above ARENA_CHUNKS, with all-ones as the "no next chunk" sentinel), and the
// arena allocates chunk indices sequentially from its own cursor. A chunk's
// `next` therefore has to name a chunk that has NOT BEEN ALLOCATED YET.
//
// The tempting way to get it is a counter in here kept in step with the
// arena's `n_chunks_q`. That is exactly CLAUDE.md's "detector wired to two
// operands that move together", and `zhao_geom_paramarena`'s own header
// refuses the same shape for the vertex index: the two copies diverge on the
// first discarded record and nothing notices, because nothing looks at the
// producer's copy.
//
// So this block keeps NO copy. `ck_alloc_id_i` is the arena's OWN live cursor
// -- the index the next accepted chunk will receive -- and:
//
//   * a chunk that is the last of its tile is emitted with `next` = the
//     all-ones sentinel;
//   * any other chunk is emitted with `next` = `ck_alloc_id_i + 1`, which is
//     the index the arena will hand the chunk this block offers NEXT, because
//     this block is the arena's only chunk producer and offers a tile's chunks
//     in order.
//
// THAT LAST CLAUSE IS AN ASSUMPTION, SO IT IS MEASURED RATHER THAN ASSERTED.
// `expect_q` is loaded with `ck_alloc_id_i + 1` on an ACCEPT and compared
// against `ck_alloc_id_i` on the NEXT ACCEPT. The two sides are one acceptance
// apart -- a held value against a live one -- so the comparison is
// structurally able to see the fault it is about: a second producer slipping a
// chunk in between, or an allocator that stops being sequential. If the
// detector were clocked from one enable it could not see either, which is the
// whole reason this paragraph exists.
//
// `chain_break_o` IS REACHABLE WITH LEGAL STIMULUS and therefore owes no
// mutant: `ck_alloc_id_i` is an INPUT, so a bench that skips an index fires
// it. `tests/geometry/geom_chunkser_directed.cpp` does exactly that.
//
// ---------------------------------------------------------------------------
// THE FAILURE THIS BLOCK EXISTS TO PREVENT, QUOTED FROM THE ENTRY
// ---------------------------------------------------------------------------
//   "a binner slot is 0..127, every one of those is far below any plausible
//    sealed `tris`, so `zhao_geom_parambuf`'s `ck_illegal_o` and `td_illegal_o`
//    would both PASS and the walk would return WRONG DESCRIPTORS THAT DECODE
//    CLEANLY. Every counter would balance."
//
// The consequence for this file is specific: **a binner slot must never leave
// this block.** What arrives on `ser_tri_id_i` is the ARENA's TriangleDescriptor
// index, carried with the triangle from `zhao_geom_vertid.tri_id_o` through
// GEOM.SETUP (which is 1:1 and order-preserving and culls nothing) and stored
// in the binner's metadata bank under the binner's own triangle identity. It is
// not re-derived here, not looked up here, and not counted here -- because
// every one of those is a second copy of an identity, and a second copy is the
// thing that decodes cleanly and is wrong.
//
// Both range guards being blind to that fault is also why the acceptance test
// for this block does not read them. It reads the arena's DESCRIPTOR BYTES for
// each id a chunk carries and compares the vertex triple against the triangle
// the tile actually references.
//
// Conservative SystemVerilog subset (charter §2). Elaboration guards live
// inside `initial begin ... end` because Quartus 17 rejects the bare
// module-scope form, and every generate is explicit -- both forms passed
// `verilator --lint-only` with zero diagnostics while failing `quartus_map`.
`default_nettype none

module zhao_geom_chunkser #(
    // R7's chunk holds fourteen ids. Not a knob in the layout sense -- the
    // record is frozen -- but named here because the arena takes it as a
    // parameter too and the two must agree.
    parameter int unsigned CHUNK_IDS = 14,
    // The binner's tile grid. TILES is what the clear phase walks.
    parameter int unsigned TILES  = 576,
    parameter int unsigned TIDX_W = 10,
    // The arena's TriangleDescriptor index width: `td_id_o` is u18.
    parameter int unsigned ID_W = 18,
    // The arena's chunk index width: `n_chunks_q` is u18.
    parameter int unsigned CHIDX_W = 18
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- the binner's SERIALISE PASS ---------------------------------------
    // `ser_req_o` is a level: it is raised when a pass is wanted and dropped
    // when the binner reports the pass done. The binner starts the pass only
    // when its own raster drain has finished, so this block never competes
    // with the picture.
    output var logic               ser_req_o,
    input  var logic               ser_done_i,      // one-cycle pulse
    input  var logic               ser_valid_i,
    output var logic               ser_ready_o,
    input  var logic [ID_W-1:0]    ser_tri_id_i,    // the ARENA's index
    input  var logic [TIDX_W-1:0]  ser_tile_i,
    input  var logic               ser_first_i,     // first ref of its tile
    input  var logic               ser_last_i,      // last ref of its tile

    // ---- frame edges --------------------------------------------------------
    // `frame_start_i` clears the head table. It is the arena's `seal_fire_o`
    // at composition: the clock the arena's cursors actually move, not the
    // frame edge that ASKED for a seal, because a seal is held pending while
    // the drain completes and the two can be many cycles apart.
    input  var logic               frame_start_i,
    // Geometry has no more triangles this frame. The pass is requested once
    // the binner's raster drain is done.
    input  var logic               geom_done_i,

    // ---- the arena's chunk intake ------------------------------------------
    output var logic                     ck_valid_o,
    input  var logic                     ck_ready_i,
    output var logic [31:0]              ck_next_o,
    output var logic [15:0]              ck_count_o,
    output var logic [CHUNK_IDS*32-1:0]  ck_ids_o,
    // The arena's OWN cursor: the index the next accepted chunk receives.
    input  var logic [CHIDX_W-1:0]       ck_alloc_id_i,

    // ---- the per-tile head, which is what a walk starts FROM ---------------
    // Registered read: present a tile index, read the chunk index one clock
    // later. `head_chunk_o` is the all-ones sentinel for a tile with no
    // references, so a reader that ignores `head_valid_o` still cannot start a
    // walk over nothing.
    input  var logic [TIDX_W-1:0]  head_tile_i,
    output var logic [31:0]        head_chunk_o,
    output var logic               head_valid_o,

    // ---- this block is finished and the arena may end its frame ------------
    // ONE-CYCLE PULSE, raised when the pass has ended AND the last chunk has
    // been accepted. The composer must hold the arena's `frame_end_i` until
    // this fires: the arena publishes as soon as its frame ends and its writes
    // retire, so a frame ended before the chunks are in is a published frame
    // whose tile lists are empty -- correct-looking and wrong.
    output var logic               ser_frame_done_o,

    // ---- evidence ----------------------------------------------------------
    output var logic [31:0] chunks_emitted_o,
    output var logic [31:0] refs_serialised_o,
    output var logic [31:0] tiles_with_refs_o,
    output var logic [31:0] chain_break_o,
    output var logic [31:0] head_clash_o,      // a tile opened twice in a pass
    output var logic [31:0] pass_truncated_o,  // the pass ended mid-chunk
    output var logic        busy_o
);

  // The "no next chunk" sentinel. `zhao_geom_parambuf` names the same constant
  // and refuses to follow it; it is all-ones because that is not an address.
  localparam logic [31:0] CK_NULL = 32'hFFFF_FFFF;

  localparam int unsigned SLOT_W = $clog2(CHUNK_IDS + 1);

  initial begin
    if (CHUNK_IDS == 0)
      $fatal(1, "zhao_geom_chunkser: CHUNK_IDS must be positive");
    if (ID_W > 32)
      $fatal(1, "zhao_geom_chunkser: ID_W must fit a u32 chunk id field");
    if (CHIDX_W > 31)
      $fatal(1, "zhao_geom_chunkser: CHIDX_W must leave the sentinel expressible");
    if (TILES > (1 << TIDX_W))
      $fatal(1, "zhao_geom_chunkser: TIDX_W cannot address TILES");
  end

  // ------------------------------------------------------------- states ----
  localparam logic [2:0] C_CLEAR = 3'd0;   // wipe the head table
  localparam logic [2:0] C_IDLE  = 3'd1;
  localparam logic [2:0] C_PASS  = 3'd2;   // taking references
  localparam logic [2:0] C_EMIT  = 3'd3;   // offering a chunk to the arena
  localparam logic [2:0] C_END   = 3'd4;   // the pass ended; drain the tail

  logic [2:0] st_q;

  // --------------------------------------------------------- the staging ---
  // One chunk under construction. Fourteen ids and a count; the ids are
  // zero-extended arena indices, and the record's `count` is what bounds them,
  // so the unused tail is zero rather than stale.
  logic [31:0]       stage_q [0:CHUNK_IDS-1];
  logic [SLOT_W-1:0] fill_q;
  logic [TIDX_W-1:0] tile_q;
  logic              tile_open_q;   // this tile has already placed a head
  logic              last_q;        // the chunk being emitted ends its tile

  // ------------------------------------------------------ the head table ---
  logic [CHIDX_W-1:0] head_ram [0:TILES-1];
  logic               head_v_ram [0:TILES-1];
  logic [TIDX_W-1:0]  clear_q;
  logic               head_we;
  logic [TIDX_W-1:0]  head_wa;
  logic [CHIDX_W-1:0] head_wd;
  logic               head_wv;

  // ------------------------------------------------------------ the chain --
  logic [CHIDX_W-1:0] expect_q;
  logic               expect_v_q;

  logic               done_q;
  // `ser_done_i` is a ONE-CYCLE PULSE and can land while a chunk is being
  // offered to the arena, which may take arbitrarily long behind the guard.
  // Sampling it only in C_PASS would drop it and hang the frame, so it is
  // latched the moment it arrives and consumed once the chunk lands.
  logic               pass_end_q;

  // --------------------------------------------------------------- output --
  // The ids vector, LSB-first: id 0 occupies bytes 8..11 of the record, which
  // is the low end of `ck_ids_i` at the arena.
  genvar gi;
  generate
    for (gi = 0; gi < CHUNK_IDS; gi = gi + 1) begin : g_ids
      assign ck_ids_o[gi*32 +: 32] = stage_q[gi];
    end
  endgenerate

  assign ck_valid_o = (st_q == C_EMIT);
  assign ck_count_o = {{(16-SLOT_W){1'b0}}, fill_q};
  // A chunk that ends its tile terminates the chain; any other names the index
  // the arena will give the chunk offered next. See the header.
  assign ck_next_o  = last_q ? CK_NULL
                             : {{(32-CHIDX_W){1'b0}}, ck_alloc_id_i} + 32'd1;

  assign ser_req_o   = (st_q == C_PASS) || (st_q == C_EMIT) || (st_q == C_END);
  // A reference is taken only while a chunk is not being offered: the staging
  // register is the resource, and one chunk is in flight at a time.
  assign ser_ready_o = (st_q == C_PASS);
  assign busy_o      = (st_q != C_IDLE);
  assign ser_frame_done_o = done_q;

  wire ck_fire_c  = ck_valid_o && ck_ready_i;
  wire ref_fire_c = ser_valid_i && ser_ready_o;
  // The staging register is full, or this reference ends its tile: either way
  // the chunk goes now. `ser_last_i` is known ON the beat, so a fourteenth
  // reference that is also its tile's last is emitted terminated rather than
  // waiting for a reference that will never come.
  wire stage_full_c = (fill_q == SLOT_W'(CHUNK_IDS - 1));
  wire emit_now_c   = ref_fire_c && (stage_full_c || ser_last_i);

  // --------------------------------------------------------- head table ----
  always_comb begin
    head_we = (st_q == C_CLEAR);
    head_wa = clear_q;
    head_wd = {CHIDX_W{1'b0}};
    head_wv = 1'b0;
    // A tile's head is the index of its FIRST accepted chunk, taken from the
    // arena's own cursor on the acceptance clock.
    if (ck_fire_c && !tile_open_q) begin
      head_we = 1'b1;
      head_wa = tile_q;
      head_wd = ck_alloc_id_i;
      head_wv = 1'b1;
    end
  end

  always_ff @(posedge clk) begin
    if (head_we) begin
      head_ram[head_wa]   <= head_wd;
      head_v_ram[head_wa] <= head_wv;
    end
  end

  logic [CHIDX_W-1:0] head_rd_q;
  logic               head_rv_q;
  always_ff @(posedge clk) begin
    head_rd_q <= head_ram[head_tile_i];
    head_rv_q <= head_v_ram[head_tile_i];
  end
  assign head_valid_o = head_rv_q;
  assign head_chunk_o = head_rv_q ? {{(32-CHIDX_W){1'b0}}, head_rd_q} : CK_NULL;

  // -------------------------------------------------------- the sequencer --
  integer k;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_q        <= C_CLEAR;
      clear_q     <= {TIDX_W{1'b0}};
      fill_q      <= {SLOT_W{1'b0}};
      tile_q      <= {TIDX_W{1'b0}};
      tile_open_q <= 1'b0;
      last_q      <= 1'b0;
      expect_q    <= {CHIDX_W{1'b0}};
      expect_v_q  <= 1'b0;
      done_q      <= 1'b0;
      pass_end_q  <= 1'b0;
      for (k = 0; k < CHUNK_IDS; k = k + 1) stage_q[k] <= 32'd0;
      chunks_emitted_o  <= 32'd0;
      refs_serialised_o <= 32'd0;
      tiles_with_refs_o <= 32'd0;
      chain_break_o     <= 32'd0;
      head_clash_o      <= 32'd0;
      pass_truncated_o  <= 32'd0;
    end else begin
      done_q <= 1'b0;

      // A new frame wipes the head table. Clearing is exact rather than
      // stamped: a generation tag narrow enough to be cheap aliases after its
      // own period, and a tile that has not been written for exactly that many
      // frames would then read as this frame's -- a stale head that decodes
      // cleanly, which is the class of fault this whole block is written
      // against.
      if (frame_start_i) begin
        st_q        <= C_CLEAR;
        clear_q     <= {TIDX_W{1'b0}};
        fill_q      <= {SLOT_W{1'b0}};
        tile_open_q <= 1'b0;
        expect_v_q  <= 1'b0;
        pass_end_q  <= 1'b0;
      end else begin
        case (st_q)
          C_CLEAR: begin
            if (clear_q == TIDX_W'(TILES - 1)) begin
              st_q <= C_IDLE;
            end else begin
              clear_q <= clear_q + TIDX_W'(1);
            end
          end

          C_IDLE: begin
            if (geom_done_i) st_q <= C_PASS;
          end

          C_PASS: begin
            if (ref_fire_c) begin
              stage_q[fill_q] <= {{(32-ID_W){1'b0}}, ser_tri_id_i};
              refs_serialised_o <= refs_serialised_o + 32'd1;

              // `ser_first_i` opens a tile. `tile_open_q` is cleared here and
              // set when the tile's first chunk is ACCEPTED, so a tile whose
              // first chunk has not landed yet still places its head.
              if (ser_first_i) begin
                tile_q            <= ser_tile_i;
                tile_open_q       <= 1'b0;
                tiles_with_refs_o <= tiles_with_refs_o + 32'd1;
                // Two opens for one tile inside a pass would mean the walk
                // visited it twice and the second head would overwrite the
                // first, silently losing the tile's earlier references.
                if (fill_q != {SLOT_W{1'b0}}) head_clash_o <= head_clash_o + 32'd1;
              end

              if (emit_now_c) begin
                fill_q <= fill_q + SLOT_W'(1);
                last_q <= ser_last_i;
                st_q   <= C_EMIT;
              end else begin
                fill_q <= fill_q + SLOT_W'(1);
              end
            end else if (ser_done_i) begin
              st_q <= C_END;
            end
          end

          C_EMIT: begin
            if (ck_fire_c) begin
              chunks_emitted_o <= chunks_emitted_o + 32'd1;

              // THE CHAIN CHECK. `expect_q` was loaded on the PREVIOUS
              // acceptance and `ck_alloc_id_i` is live on THIS one, so the two
              // sides of this comparison are one acceptance apart and it can
              // see an allocator that stopped being sequential.
              if (expect_v_q && (ck_alloc_id_i != expect_q))
                chain_break_o <= chain_break_o + 32'd1;
              expect_q   <= ck_alloc_id_i + CHIDX_W'(1);
              expect_v_q <= 1'b1;

              tile_open_q <= 1'b1;
              fill_q      <= {SLOT_W{1'b0}};
              for (k = 0; k < CHUNK_IDS; k = k + 1) stage_q[k] <= 32'd0;

              // The pass may have ended while this chunk was being offered.
              if (ser_done_i || pass_end_q) begin
                st_q       <= C_END;
                pass_end_q <= 1'b0;
              end else begin
                st_q <= C_PASS;
              end
            end else if (ser_done_i) begin
              pass_end_q <= 1'b1;
            end
          end

          C_END: begin
            // Nothing may be left staged: every tile's last reference carries
            // `ser_last_i`, which forces an emit. A non-zero fill here means
            // the binner ended its pass mid-tile.
            if (fill_q != {SLOT_W{1'b0}}) begin
              pass_truncated_o <= pass_truncated_o + 32'd1;
              last_q     <= 1'b1;
              // Re-arm the end latch, or the flush's acceptance would send the
              // sequencer back to C_PASS and wait for a pass that has ended.
              pass_end_q <= 1'b1;
              st_q       <= C_EMIT;
            end else begin
              done_q <= 1'b1;
              st_q   <= C_IDLE;
            end
          end

          default: st_q <= C_IDLE;
        endcase
      end
    end
  end

endmodule

`default_nettype wire
