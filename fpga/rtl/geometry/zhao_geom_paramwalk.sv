// zhao_geom_paramwalk.sv -- GEOM.PARAMBUF's RECORD READER and chunk walker.
//
// ---------------------------------------------------------------------------
// WHAT THIS IS
// ---------------------------------------------------------------------------
// `zhao_geom_paramarena` writes a frame's geometry into local SDRAM. THIS
// BLOCK READS IT BACK. It fetches the frame directory from the shared scratch,
// walks a tile's chunk chain out of the published view, fetches each triangle
// descriptor the chain names, and emits the decoded fields.
//
// The decode is NOT reimplemented here. `zhao_geom_parambuf` is instantiated
// inside this block and is the only thing that turns bytes into fields, owns
// the two legality rules and owns the staleness gate. That is the whole reason
// the record layer exists as its own block, and a walker that re-derived
// `fits_s21` or the chunk's illegality rule would be the second implementation
// this repository's own law names as the failure to avoid.
//
// ---------------------------------------------------------------------------
// WHY THE ROUND TRIP IS THE POINT
// ---------------------------------------------------------------------------
// The owner's completion ruling of 2026-09-22, item 4: "Do not pack fields
// into a byte vector merely to unpack them again and count that as
// external-memory integration."
//
// So the evidence this block exists to produce is not that a decoder decodes.
// It is `dir_mismatch_o`. The frame directory reaches this block TWICE by two
// independent paths: once as `pub_*_i`, combinationally from the arena's own
// registers, and once as 64 bytes that went out through the guard, the
// arbiter, the controller and the SDRAM model and came back. If those two
// disagree, the bytes did not survive the round trip, and NOTHING ELSE IN THE
// SYSTEM WOULD SAY SO -- a walk over corrupt records still produces triangles.
//
// AND THE TWO SIDES ARE NOT LOADED BY ONE ENABLE, which is the check CLAUDE.md
// requires of any detector: `pub_*_i` is driven by the arena's publish
// registers, loaded at publication; the memory copy is loaded beat by beat by
// this block's read FSM. A fault in the write path, the read path, the demux
// or the addressing moves one and not the other.
//
// ---------------------------------------------------------------------------
// REQUEST IDENTITY TRAVELS WITH THE WALK
// ---------------------------------------------------------------------------
// Item 4 again: "Carry request identity WITH the request; do not validate a
// queued request against a later global view selector."
//
// A walk latches `w_view_q`, `w_gen_q` and the three region bases AT WALK
// START, and every address it issues and every staleness test it makes is
// against THOSE. `ck_frame_gen_i` on the decoder is fed from `w_gen_q`, not
// from the live `pub_gen_i`. If the published frame changed mid-walk, a live
// comparison would silently start accepting the NEW frame's chunks as fresh
// while following the OLD frame's pointers -- response A's data with B's
// metadata, exactly.
//
// `gen_race_o` is the detector for that, and its two operands move on
// different enables (`w_gen_q` at walk start, `pub_gen_i` at publication). It
// is UNREACHABLE with legal stimulus, because `busy_o` feeds the arena's
// `reader_busy_i` and blocks the seal that would flip the frame -- so it is a
// counter asserted zero that owes a committed mutant rather than an argument.
// `tests/mutants/zhao_geom_paramarena_drain_mutant.sv` removes that drain and
// is what fires it.
//
// ---------------------------------------------------------------------------
// WHAT A WALK REFUSES, AND WHY EACH REFUSAL IS COUNTED SEPARATELY
// ---------------------------------------------------------------------------
//   * a STALE chunk -- its generation is not the one this walk started under.
//     Refused and counted; never followed. R7: "a chunk from last frame reads
//     as a valid chunk in every other respect."
//   * an ILLEGAL chunk -- count above capacity, or a `next` outside the arena.
//   * a chain longer than `MAX_WALK` chunks. A corrupt `next` that points
//     BACKWARDS makes a legal-looking cycle that every per-chunk test passes,
//     so the only thing that stops it is a bound on the walk. `walk_cut_o`
//     counts it, and the bound is a parameter rather than a constant because
//     the chain length is a property of the scene, not of this block.
//   * a SHORT BURST -- a read that ended before its last beat. The bytes are
//     incomplete and decoding them would produce a plausible record.
//   * a STRAY BEAT -- data offered while no read is in flight. The ENGINE1
//     share BROADCASTS beat data and demuxes only `beat_valid`, so a wrong
//     demux delivers ANOTHER CLIENT'S BYTES with every other signal correct.
//     Beat COUNT is the only thing that sees it; an address round trip cannot,
//     because it compares two of this block's own registers and knows nothing
//     about whose bytes arrived.
//
// Every one of these ends the walk rather than skipping the record. A walk that
// skipped would emit a tile's triangles minus an arbitrary subset, which is
// R7's forbidden "frame with an arbitrary missing tail" one level down.
//
// ---------------------------------------------------------------------------
// THE GUARD ANSWERS IN TWO CYCLES
// ---------------------------------------------------------------------------
// `rsp.ready` is a LEVEL and `rsp.ok` is pulsed the cycle AFTER the accept.
// Testing them in one arm reads every pass as a denial with the denial counter
// stuck at zero -- the defect found in both geometry fetchers on 2026-09-06.
// R_REQ waits on `ready`; R_VERD reads `ok`/`violation` one cycle later.
// ---------------------------------------------------------------------------
`default_nettype none

module zhao_geom_paramwalk
  import zhao_pkg::*;
#(
    parameter logic [31:0] SCRATCH_BASE = ZHAO_PARAMBUF_SCRATCH_BASE,
    // THE SDRAM'S BURST-ALIGNMENT QUANTUM, IN BYTES -- the same knob, with the
    // same default and the same reason, as `zhao_geom_paramarena`'s. The full
    // argument lives in that block's header under "THE BURST THAT WRAPS";
    // in one sentence, a JEDEC BL8 sequential burst wraps inside its aligned
    // eight-column block, a column here is one 16-bit word, and the arbiter
    // aligns only to the 2048-word row. This block READS, and a read that
    // wraps returns the right bytes in the wrong order -- which decodes to a
    // plausible record, not to an error.
    parameter int unsigned BURST_ALIGN_B = 16,
    // The longest chain a walk will follow before cutting it. A `next` that
    // points backwards is a legal-looking cycle, and nothing inside a chunk
    // can tell you so.
    parameter int unsigned MAX_WALK  = 4096,
    parameter int unsigned CHUNK_IDS = 14,
    // The arena's chunk count, in chunks. `zhao_geom_parambuf` refuses a
    // `next_chunk` at or above it.
    parameter int unsigned ARENA_CHUNKS = 16384
) (
    input  var logic clk,
    input  var logic rst_n,

    input  var zhao_client_e cfg_vram_client_i,

    // ---- the published frame, straight from the arena's registers -----------
    input  var logic        pub_valid_i,
    input  var logic [15:0] pub_gen_i,
    input  var logic [26:0] pub_vert_base_i,
    input  var logic [26:0] pub_tri_base_i,
    input  var logic [26:0] pub_chunk_base_i,
    input  var logic [17:0] pub_verts_i,
    input  var logic [17:0] pub_tris_i,
    input  var logic [17:0] pub_chunks_i,

    // ---- the scratch, whose owner is the arena ------------------------------
    output var logic        scr_req_o,
    input  var logic        scr_grant_i,

    // ---- a walk -------------------------------------------------------------
    input  var logic        walk_valid_i,
    output var logic        walk_ready_o,
    // The tile's first chunk index. 32 bits at the port because a chunk
    // handle is u32 in R7's layout; only the low 23 are ever addressable,
    // because ARENA_CHUNKS bounds the index far below that and a 4 MiB view
    // cannot hold more at a 64-byte stride.
    /* verilator lint_off UNUSEDSIGNAL */
    input  var logic [31:0] walk_head_i,
    /* verilator lint_on UNUSEDSIGNAL */
    output var logic        walk_done_o,       // pulse: the chain ended
    output var logic        walk_failed_o,     // ... and it ended badly

    // ---- the decoded triangles this block produces --------------------------
    output var logic        t_valid_o,
    input  var logic        t_ready_i,
    output var logic [15:0] t_v0_o,
    output var logic [15:0] t_v1_o,
    output var logic [15:0] t_v2_o,
    output var logic [15:0] t_material_o,
    output var logic [31:0] t_raster_o,
    output var logic [31:0] t_source_o,
    output var logic        t_illegal_o,       // a vertex id past the seal

    // ---- the ENGINE1 read socket --------------------------------------------
    output var zhao_guard_req_t guard_req_o,
    input  var zhao_guard_rsp_t guard_rsp_i,
    input  var logic            beat_valid_i,
    input  var logic [63:0]     beat_data_i,
    input  var logic            beat_last_i,

    // ---- evidence -----------------------------------------------------------
    output var logic [31:0] dirs_read_o,
    output var logic [31:0] dir_mismatch_o,
    output var logic [31:0] chunks_walked_o,
    output var logic [31:0] chunks_stale_o,
    output var logic [31:0] chunks_illegal_o,
    output var logic [31:0] tris_emitted_o,
    output var logic [31:0] tris_illegal_o,
    output var logic [31:0] walk_cut_o,
    output var logic [31:0] guard_denied_o,
    output var logic [31:0] short_burst_o,
    output var logic [31:0] stray_beat_o,
    output var logic [31:0] gen_race_o,
    // THE BURST-ALIGNMENT TRIPWIRE. Unlike the producer's, THIS ONE IS
    // REACHABLE WITH LEGAL STIMULUS and therefore owes no mutant: the walker
    // does not compute its bases, it is TOLD them on `pub_*_base_i`, so a
    // directed case that publishes a misaligned base fires it. That is the
    // useful direction anyway -- this counter's job is to catch a PRODUCER
    // whose layout drifted, not this block's own arithmetic.
    output var logic [31:0] burst_unaligned_o,
    output var logic [15:0] walk_depth_max_o,
    output var logic        busy_o
);

  localparam int unsigned CK_B  = ZHAO_PARAMBUF_CK_BYTES;  // w=64 bytes
  localparam int unsigned TD_B  = ZHAO_PARAMBUF_TD_BYTES;  // w=16 bytes
  localparam int unsigned CKW   = CK_B * 8;                // w=512 bits
  // The low bits an aligned address must have clear.
  localparam int unsigned ALIGN_LSB = $clog2(BURST_ALIGN_B);  // w=4 bits
  localparam int unsigned CK_BEATS = CK_B / 8;             // w=8 beats
  localparam int unsigned TD_BEATS = TD_B / 8;             // w=2 beats

  // synthesis translate_off
  initial begin
    if (CHUNK_IDS != 14)
      $fatal(1, "zhao_geom_paramwalk: R7's chunk holds fourteen ids");
    if (MAX_WALK < 1)
      $fatal(1, "zhao_geom_paramwalk: a walk bound of zero follows nothing");
  end
  // synthesis translate_on

  // ------------------------------------------------------------- the walk --
  typedef enum logic [3:0] {
    W_IDLE,
    W_SCR,        // ask the arena for the scratch
    W_DIR_REQ, W_DIR_VERD, W_DIR_BEAT, W_DIR_CHECK,
    W_CK_REQ,  W_CK_VERD,  W_CK_BEAT,  W_CK_CHECK,
    W_TD_REQ,  W_TD_VERD,  W_TD_BEAT,  W_TD_EMIT,
    W_END
  } wstate_e;
  wstate_e wstate_q;

  // LATCHED AT WALK START. Every address and every staleness test is against
  // these and never against the live pub_* inputs.
  logic [15:0] w_gen_q;
  logic [26:0] w_tri_base_q, w_chunk_base_q;
  // THE SEALED VERTEX COUNT, AND THE FIRST DRAFT LATCHED THE WRONG ONE.
  // `zhao_geom_parambuf`'s `td_sealed_vertices_i` is the frame's VERTEX count
  // -- it is what `v0/v1/v2 >= sealed` is tested against -- and this block fed
  // it the TRIANGLE count. Verilator's UNUSEDSIGNAL on the top two bits is
  // what surfaced it, which is worth recording: the defect was a plausible
  // name collision between two 18-bit counts from the same producer, it would
  // have refused or admitted triangles by an unrelated number, and no test
  // that seals equal counts could ever have told the two apart.
  //
  // SIXTEEN BITS, AND THE NARROWING IS DECLARED. The decoder's port is u16
  // because a `vertex_id` is u16, so a seal of 65,536 -- R7's own preferred
  // tier -- is not expressible in it. `zhao_geom_paramarena`'s elaboration
  // guard caps MAX_VERTS at 65,535 so the case cannot arise; the saturation
  // here is the belt to that guard's braces and runs in the REFUSING
  // direction, because admitting an id the seal does not cover is how one bad
  // descriptor reads somebody else's vertex.
  // 18 BITS, ARENAID 2026-09-25. See `zhao_geom_parambuf`'s
  // `td_sealed_vertices_i` for the directive-section-4 reason. It used to be
  // u16 and to SATURATE at 0xFFFF, which turned a full 65,536-vertex frame
  // into a seal of 65,535 and made the last vertex illegal -- the "silently
  // sacrificing a vertex" the directive names, arriving through a saturation
  // rather than through a truncation.
  logic [17:0] w_verts_q;
  logic [22:0] w_chunk_q;          // the chunk index being read; ARENA_CHUNKS
                                   // bounds it far below 2^23
  logic [15:0] w_depth_q;
  logic        w_failed_q;

  // TWO buffers, not one, and the reason is a defect the first draft of this
  // block had. `r_buf_q` must hold the CHUNK for as long as its fourteen ids
  // are being fetched; with one shared buffer the first descriptor overwrote
  // it, so the chain could only be continued by RE-READING the chunk -- which
  // re-enters the chunk state, re-counts it and re-tests the depth bound, i.e.
  // an unbounded loop that every per-chunk check passes. 128 extra flops buy
  // the correctness; a second 512-bit copy of the chunk would have bought the
  // same thing for four times the price.
  logic [CKW-1:0] r_buf_q;
  logic [127:0]   td_buf_q;
  // The chain, LATCHED at the chunk's decode. `ck_follow_c` and `ck_next_c`
  // are combinational over `r_buf_q` AND over the decoder's `ck_valid_i`,
  // which is high only in W_CK_CHECK -- so the verdict has to be HELD, not
  // re-read later from a decoder that is no longer being told the bytes are
  // valid. A block that read them later would get the decoder's idle output
  // and follow a pointer of zero.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [31:0]    ck_next_q;     // u32 in the layout; low 23 addressable
  /* verilator lint_on UNUSEDSIGNAL */
  logic           ck_follow_q;
  logic [3:0]     r_beat_q;
  logic [3:0]     r_beats_q;
  logic [26:0]    m_addr_q;        // LATCHED AT OP START, the only source of
                                   // guard_req_o.addr
  logic [6:0]     m_len_q;

  // within a chunk: which of its ids we are fetching
  logic [3:0]  id_idx_q;
  logic [15:0] id_count_q;

  // ------------------------------------------------------------- decoding --
  // The one decoder. Fed from the burst buffer for chunks, and from the low
  // 128 bits of the same buffer for descriptors.
  logic        dec_ck_valid_c, dec_td_valid_c;
  logic [31:0] ck_next_c;
  logic [15:0] ck_count_c;
  logic        ck_stale_c, ck_illegal_c, ck_follow_c;
  logic [15:0] td_v0_c, td_v1_c, td_v2_c, td_material_c;
  logic [31:0] td_raster_c, td_source_c;
  logic        td_illegal_c;

  assign dec_ck_valid_c = (wstate_q == W_CK_CHECK);
  assign dec_td_valid_c = (wstate_q == W_TD_EMIT);

  /* verilator lint_off PINCONNECTEMPTY */
  zhao_geom_parambuf #(
      .CHUNK_IDS    (CHUNK_IDS),
      .ARENA_CHUNKS (ARENA_CHUNKS)
  ) u_rec (
      .clk, .rst_n,
      // The vertex arm is not driven from here. GEOM.PARAMBUF's projected
      // vertices are consumed by the raster front end, not by the chunk walk,
      // and wiring a walk-side vertex decode would be a second consumer of the
      // same record with no reader -- an uncashed cheque authored on purpose.
      .pv_valid_i (1'b0),
      .pv_bytes_i ('0),
      .pv_x_o (), .pv_y_o (), .pv_invw_o (), .pv_status_o (),
      .pv_uow_o (), .pv_vow_o (), .pv_rgba_o (), .pv_illegal_o (),

      .td_valid_i (dec_td_valid_c),
      .td_bytes_i (td_buf_q),
      // THE SEALED COUNT IS THE ONE THIS WALK STARTED UNDER. A live
      // `pub_tris_i` here would let a descriptor from the old frame be
      // validated against the new frame's seal.
      .td_sealed_vertices_i (w_verts_q),
      .td_v0_o (td_v0_c), .td_v1_o (td_v1_c), .td_v2_o (td_v2_c),
      .td_material_o (td_material_c), .td_raster_o (td_raster_c),
      .td_source_o (td_source_c), .td_illegal_o (td_illegal_c),

      .ck_valid_i     (dec_ck_valid_c),
      .ck_bytes_i     (r_buf_q),
      .ck_frame_gen_i (w_gen_q),
      // `ck_gen_o` is not taken: the decoder's `ck_stale_o` is the verdict
      // this block acts on, and a second copy of the generation here would be
      // a value with no reader that the next person has to work out is dead.
      .ck_next_o (ck_next_c), .ck_count_o (ck_count_c), .ck_gen_o (),
      .ck_stale_o (ck_stale_c), .ck_illegal_o (ck_illegal_c),
      .ck_follow_o (ck_follow_c),

      // The decoder's own counters are ITS evidence about records; this
      // block's are evidence about the WALK. Both exist, deliberately: one
      // says a record was malformed, the other says a chain was abandoned.
      .pv_illegal_count_o (), .td_illegal_count_o (),
      .ck_stale_count_o (), .ck_illegal_count_o ()
  );
  /* verilator lint_on PINCONNECTEMPTY */

  // ---------------------------------------------- the directory, as read ---
  // The same field layout the arena writes. Read back out of the burst buffer
  // so the comparison below is between BYTES THAT TRAVELLED and registers that
  // did not.
  wire [31:0] dir_vert_base_c  = r_buf_q[  0 +: 32];
  wire [31:0] dir_tri_base_c   = r_buf_q[ 32 +: 32];
  wire [31:0] dir_chunk_base_c = r_buf_q[ 64 +: 32];
  wire [15:0] dir_gen_c        = r_buf_q[ 96 +: 16];
  wire        dir_valid_c      = r_buf_q[112];
  wire [17:0] dir_verts_c      = r_buf_q[128 +: 18];
  wire [17:0] dir_tris_c       = r_buf_q[160 +: 18];
  wire [17:0] dir_chunks_c     = r_buf_q[192 +: 18];

  // THE ROUND-TRIP CHECK. Eight fields, and every one of them is compared:
  // a check that compared only the generation would pass while every base was
  // wrong, and the bases are what a bad address or a wrong demux corrupts.
  wire dir_agrees_c =
         dir_valid_c
      && (dir_gen_c        == pub_gen_i)
      && (dir_vert_base_c  == {5'b0, pub_vert_base_i})
      && (dir_tri_base_c   == {5'b0, pub_tri_base_i})
      && (dir_chunk_base_c == {5'b0, pub_chunk_base_i})
      && (dir_verts_c      == pub_verts_i)
      && (dir_tris_c       == pub_tris_i)
      && (dir_chunks_c     == pub_chunks_i);

  // ------------------------------------------------------------- the port --
  always_comb begin
    guard_req_o        = '0;
    guard_req_o.valid  = (wstate_q == W_DIR_REQ) || (wstate_q == W_CK_REQ)
                      || (wstate_q == W_TD_REQ);
    guard_req_o.write  = 1'b0;            // this block never writes
    guard_req_o.client = cfg_vram_client_i;
    guard_req_o.addr   = m_addr_q;
    guard_req_o.len    = m_len_q;
    // The guard requires `be == mask_of(len)` EXACTLY. A 16-byte read with an
    // all-ones mask is refused on SHAPE, and that refusal reads like a
    // permissions problem.
    guard_req_o.be     = 64'(({64{1'b1}} >> (64 - m_len_q)));
  end

  assign scr_req_o    = (wstate_q == W_SCR) || (wstate_q == W_DIR_REQ)
                     || (wstate_q == W_DIR_VERD) || (wstate_q == W_DIR_BEAT)
                     || (wstate_q == W_DIR_CHECK);
  assign walk_ready_o = (wstate_q == W_IDLE) && pub_valid_i;
  assign busy_o       = (wstate_q != W_IDLE);

  assign t_valid_o    = (wstate_q == W_TD_EMIT);
  assign t_v0_o       = td_v0_c;
  assign t_v1_o       = td_v1_c;
  assign t_v2_o       = td_v2_c;
  assign t_material_o = td_material_c;
  assign t_raster_o   = td_raster_c;
  assign t_source_o   = td_source_c;
  assign t_illegal_o  = td_illegal_c;

  // The ids this chunk names, out of the burst buffer. The chunk's fourteen
  // u32s start at byte 8, so id `k` is at bit 64 + 32k. `id_c` is the one the
  // walk is ON and `nxt_id_c` the one it moves to -- two wires rather than one
  // indexed by a value that changes in the same cycle it is used.
  // Only the low 23 bits are taken. A triangle id above 2^23 cannot address a
  // descriptor inside a 4 MiB view at a 16-byte stride, and `td_illegal_o`
  // refuses any id past the seal regardless -- so the high bits are dead by
  // construction rather than dropped by accident.
  wire [22:0] id_c     = r_buf_q[64 + 32*{28'd0, id_idx_q} +: 23];
  wire [22:0] nxt_id_c = r_buf_q[64 + 32*({28'd0, id_idx_q} + 32'd1) +: 23];

  // --------------------------------------------------------------- core ----
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      wstate_q       <= W_IDLE;
      w_gen_q        <= 16'd0;
      w_tri_base_q   <= 27'd0;
      w_chunk_base_q <= 27'd0;
      w_verts_q      <= 18'd0;
      w_chunk_q      <= 23'd0;
      w_depth_q      <= 16'd0;
      w_failed_q     <= 1'b0;
      r_buf_q        <= '0;
      td_buf_q       <= '0;
      ck_next_q      <= 32'd0;
      ck_follow_q    <= 1'b0;
      r_beat_q       <= 4'd0;
      r_beats_q      <= 4'd0;
      m_addr_q       <= 27'd0;
      m_len_q        <= 7'd0;
      id_idx_q       <= 4'd0;
      id_count_q     <= 16'd0;
      walk_done_o    <= 1'b0;
      walk_failed_o  <= 1'b0;
      dirs_read_o      <= '0;
      dir_mismatch_o   <= '0;
      chunks_walked_o  <= '0;
      chunks_stale_o   <= '0;
      chunks_illegal_o <= '0;
      tris_emitted_o   <= '0;
      tris_illegal_o   <= '0;
      walk_cut_o       <= '0;
      guard_denied_o   <= '0;
      short_burst_o    <= '0;
      stray_beat_o     <= '0;
      gen_race_o       <= '0;
      burst_unaligned_o <= '0;
      walk_depth_max_o <= 16'd0;
    end else begin
      walk_done_o   <= 1'b0;
      walk_failed_o <= 1'b0;

      // ---- a beat with no read in flight -----------------------------------
      // The share BROADCASTS beat data and demuxes only `beat_valid`, so this
      // is the check that sees a wrong demux. An address round trip cannot: it
      // compares two of this block's own registers and knows nothing about
      // whose bytes arrived.
      if (beat_valid_i && (wstate_q != W_DIR_BEAT) && (wstate_q != W_CK_BEAT)
          && (wstate_q != W_TD_BEAT))
        stray_beat_o <= stray_beat_o + 32'd1;

      // ---- the frame moved under a live walk --------------------------------
      // UNREACHABLE while `busy_o` blocks the arena's seal, which is why it is
      // a counter asserted zero with a committed mutant behind it rather than
      // an argument. Its two operands load on DIFFERENT enables: w_gen_q at
      // walk start, pub_gen_i at publication.
      if ((wstate_q != W_IDLE) && pub_valid_i && (pub_gen_i != w_gen_q))
        gen_race_o <= gen_race_o + 32'd1;

      // ---- the burst-alignment tripwire -------------------------------------
      // An INVARIANT over one register, not a difference between two, so the
      // lockstep-blindness question does not arise: there is one operand and a
      // constant, and nothing for a corruption to move WITH. It covers all
      // three request states, including the directory read, whose address is
      // SCRATCH_BASE rather than a computed offset.
      if (guard_req_o.valid && (m_addr_q[ALIGN_LSB-1:0] != '0))
        burst_unaligned_o <= burst_unaligned_o + 32'd1;

      case (wstate_q)
        // ---- start -----------------------------------------------------
        W_IDLE: if (walk_valid_i && pub_valid_i) begin
          // IDENTITY LATCHED HERE, all of it, in one enable.
          w_gen_q        <= pub_gen_i;
          w_tri_base_q   <= pub_tri_base_i;
          w_chunk_base_q <= pub_chunk_base_i;
          // No saturation: the port is as wide as the published count, so
          // the seal the decoder is tested against is the seal the arena
          // actually published, for every value the arena can publish.
          w_verts_q      <= pub_verts_i;
          w_chunk_q      <= walk_head_i[22:0];
          w_depth_q      <= 16'd0;
          w_failed_q     <= 1'b0;
          wstate_q       <= W_SCR;
        end

        // ---- the directory ----------------------------------------------
        W_SCR: if (scr_grant_i) begin
          m_addr_q  <= 27'(SCRATCH_BASE);
          m_len_q   <= 7'(CK_B);
          r_beats_q <= 4'(CK_BEATS);
          r_beat_q  <= 4'd0;
          wstate_q  <= W_DIR_REQ;
        end

        // begin/end rather than a bare statement, and the reason is the
        // GATE: tools/rtl/check_guard_verdict.py walks the body of the
        // `if (ready)` arm looking for a `.ok` tested inside it, and on the
        // bare-statement form with the next state arm on the IMMEDIATELY
        // following line it walks past the statement and finds W_DIR_VERD's
        // `.ok`. The verdict IS in its own state and always was -- this is
        // the gate's shape, not a repair -- so the file adopts the shape the
        // gate's own _GOOD self-test uses. Recorded rather than silently
        // reformatted, so the next person does not read this as a fix.
        W_DIR_REQ: if (guard_rsp_i.ready) begin
          wstate_q <= W_DIR_VERD;
        end

        W_DIR_VERD: begin
          if (guard_rsp_i.violation) begin
            guard_denied_o <= guard_denied_o + 32'd1;
            w_failed_q <= 1'b1;
            wstate_q   <= W_END;
          end else if (guard_rsp_i.ok) begin
            r_beat_q <= 4'd0;
            wstate_q <= W_DIR_BEAT;
          end
        end
        W_DIR_BEAT: if (beat_valid_i) begin
          // Little-endian word order: beat 0 ends in bits [63:0] after the
          // last shift, so byte 0 of the burst is bit 0 of the buffer.
          r_buf_q  <= {beat_data_i, r_buf_q[CKW-1:64]};
          r_beat_q <= r_beat_q + 4'd1;
          if (beat_last_i) begin
            if (r_beat_q != (r_beats_q - 4'd1)) begin
              short_burst_o <= short_burst_o + 32'd1;
              w_failed_q    <= 1'b1;
              wstate_q      <= W_END;
            end else begin
              wstate_q <= W_DIR_CHECK;
            end
          end
        end
        W_DIR_CHECK: begin
          dirs_read_o <= dirs_read_o + 32'd1;
          if (!dir_agrees_c) begin
            // THE ROUND TRIP FAILED. Nothing else in the system would say so:
            // a walk over corrupt records still produces triangles.
            dir_mismatch_o <= dir_mismatch_o + 32'd1;
            w_failed_q     <= 1'b1;
            wstate_q       <= W_END;
          end else begin
            m_addr_q  <= 27'(w_chunk_base_q + 27'(w_chunk_q * 23'(CK_B)));
            m_len_q   <= 7'(CK_B);
            r_beats_q <= 4'(CK_BEATS);
            r_beat_q  <= 4'd0;
            wstate_q  <= W_CK_REQ;
          end
        end

        // ---- a chunk -----------------------------------------------------
        // begin/end rather than a bare statement, and the reason is the
        // GATE: tools/rtl/check_guard_verdict.py walks the body of the
        // `if (ready)` arm looking for a `.ok` tested inside it, and on the
        // bare-statement form with the next state arm on the IMMEDIATELY
        // following line it walks past the statement and finds W_CK_VERD's
        // `.ok`. The verdict IS in its own state and always was -- this is
        // the gate's shape, not a repair -- so the file adopts the shape the
        // gate's own _GOOD self-test uses. Recorded rather than silently
        // reformatted, so the next person does not read this as a fix.
        W_CK_REQ: if (guard_rsp_i.ready) begin
          wstate_q <= W_CK_VERD;
        end

        W_CK_VERD: begin
          if (guard_rsp_i.violation) begin
            guard_denied_o <= guard_denied_o + 32'd1;
            w_failed_q <= 1'b1;
            wstate_q   <= W_END;
          end else if (guard_rsp_i.ok) begin
            r_beat_q <= 4'd0;
            wstate_q <= W_CK_BEAT;
          end
        end
        W_CK_BEAT: if (beat_valid_i) begin
          r_buf_q  <= {beat_data_i, r_buf_q[CKW-1:64]};
          r_beat_q <= r_beat_q + 4'd1;
          if (beat_last_i) begin
            if (r_beat_q != (r_beats_q - 4'd1)) begin
              short_burst_o <= short_burst_o + 32'd1;
              w_failed_q    <= 1'b1;
              wstate_q      <= W_END;
            end else begin
              wstate_q <= W_CK_CHECK;
            end
          end
        end
        W_CK_CHECK: begin
          // The decoder's verdict, taken whole. None of these three rules is
          // restated here.
          if (ck_stale_c) begin
            chunks_stale_o <= chunks_stale_o + 32'd1;
            w_failed_q     <= 1'b1;
            wstate_q       <= W_END;
          end else if (ck_illegal_c) begin
            chunks_illegal_o <= chunks_illegal_o + 32'd1;
            w_failed_q       <= 1'b1;
            wstate_q         <= W_END;
          end else if (w_depth_q >= 16'(MAX_WALK)) begin
            // A `next` that points backwards is a legal-looking cycle that
            // every per-chunk test passes. Only the bound stops it.
            walk_cut_o <= walk_cut_o + 32'd1;
            w_failed_q <= 1'b1;
            wstate_q   <= W_END;
          end else begin
            chunks_walked_o <= chunks_walked_o + 32'd1;
            ck_next_q       <= ck_next_c;
            ck_follow_q     <= ck_follow_c;
            w_depth_q       <= w_depth_q + 16'd1;
            if (w_depth_q + 16'd1 > walk_depth_max_o)
              walk_depth_max_o <= w_depth_q + 16'd1;
            id_count_q <= ck_count_c;
            id_idx_q   <= 4'd0;
            if (ck_count_c == 16'd0) begin
              // An empty chunk is legal and simply names nothing.
              if (ck_follow_c) begin
                w_chunk_q <= ck_next_c[22:0];
                m_addr_q  <= 27'(w_chunk_base_q + 27'(ck_next_c[22:0] * 23'(CK_B)));
                m_len_q   <= 7'(CK_B);
                r_beats_q <= 4'(CK_BEATS);
                r_beat_q  <= 4'd0;
                wstate_q  <= W_CK_REQ;
              end else begin
                wstate_q <= W_END;
              end
            end else begin
              m_addr_q  <= 27'(w_tri_base_q + 27'(id_c * 23'(TD_B)));
              m_len_q   <= 7'(TD_B);
              r_beats_q <= 4'(TD_BEATS);
              r_beat_q  <= 4'd0;
              wstate_q  <= W_TD_REQ;
            end
          end
        end

        // ---- a triangle descriptor ---------------------------------------
        // begin/end rather than a bare statement, and the reason is the
        // GATE: tools/rtl/check_guard_verdict.py walks the body of the
        // `if (ready)` arm looking for a `.ok` tested inside it, and on the
        // bare-statement form with the next state arm on the IMMEDIATELY
        // following line it walks past the statement and finds W_TD_VERD's
        // `.ok`. The verdict IS in its own state and always was -- this is
        // the gate's shape, not a repair -- so the file adopts the shape the
        // gate's own _GOOD self-test uses. Recorded rather than silently
        // reformatted, so the next person does not read this as a fix.
        W_TD_REQ: if (guard_rsp_i.ready) begin
          wstate_q <= W_TD_VERD;
        end

        W_TD_VERD: begin
          if (guard_rsp_i.violation) begin
            guard_denied_o <= guard_denied_o + 32'd1;
            w_failed_q <= 1'b1;
            wstate_q   <= W_END;
          end else if (guard_rsp_i.ok) begin
            r_beat_q <= 4'd0;
            wstate_q <= W_TD_BEAT;
          end
        end
        // THE CHUNK IS NOT TOUCHED HERE. The descriptor shifts into its own
        // 128-bit buffer, so `r_buf_q` still holds the chunk whose ids are
        // being walked and the chain continues without re-reading it.
        W_TD_BEAT: if (beat_valid_i) begin
          td_buf_q <= {beat_data_i, td_buf_q[127:64]};
          r_beat_q <= r_beat_q + 4'd1;
          if (beat_last_i) begin
            if (r_beat_q != (r_beats_q - 4'd1)) begin
              short_burst_o <= short_burst_o + 32'd1;
              w_failed_q    <= 1'b1;
              wstate_q      <= W_END;
            end else begin
              wstate_q <= W_TD_EMIT;
            end
          end
        end
        W_TD_EMIT: if (t_ready_i) begin
          tris_emitted_o <= tris_emitted_o + 32'd1;
          // A malformed descriptor is EMITTED AND FLAGGED, not dropped. The
          // consumer is told which triangle is wrong; dropping it silently
          // would leave a hole nothing downstream could see. R7's rule is that
          // it is "rejected and counted", and `t_illegal_o` is the rejection
          // travelling with the record it is about.
          if (td_illegal_c) tris_illegal_o <= tris_illegal_o + 32'd1;
          if ({12'd0, id_idx_q} + 16'd1 < id_count_q) begin
            // MORE IDS IN THIS CHUNK. `r_buf_q` still holds it, so the next id
            // comes straight out of the buffer with no re-read.
            id_idx_q  <= id_idx_q + 4'd1;
            m_addr_q  <= 27'(w_tri_base_q + 27'(nxt_id_c * 23'(TD_B)));
            m_len_q   <= 7'(TD_B);
            r_beats_q <= 4'(TD_BEATS);
            r_beat_q  <= 4'd0;
            wstate_q  <= W_TD_REQ;
          end else if (ck_follow_q) begin
            // THE CHAIN CONTINUES, from the verdict LATCHED at this chunk's
            // decode. `ck_follow_o` is the decoder's single "the pointer may
            // be taken" signal and it is false for a stale or malformed chunk,
            // so following it is the only place `next_chunk` is ever used.
            w_chunk_q <= ck_next_q[22:0];
            m_addr_q  <= 27'(w_chunk_base_q + 27'(ck_next_q[22:0] * 23'(CK_B)));
            m_len_q   <= 7'(CK_B);
            r_beats_q <= 4'(CK_BEATS);
            r_beat_q  <= 4'd0;
            wstate_q  <= W_CK_REQ;
          end else begin
            wstate_q <= W_END;
          end
        end

        W_END: begin
          walk_done_o   <= 1'b1;
          walk_failed_o <= w_failed_q;
          wstate_q      <= W_IDLE;
        end

        default: wstate_q <= W_IDLE;
      endcase
    end
  end

endmodule : zhao_geom_paramwalk

`default_nettype wire
