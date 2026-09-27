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
    parameter int unsigned ARENA_CHUNKS = 16384,
    // THE VERTEX ALLOCATION STRIDE, IN BYTES -- `zhao_geom_paramarena`'s
    // `PV_STRIDE_B`, which is how far one vertex slot is from the next and is
    // NOT the record size. R7 keeps the two concepts separate and the owner
    // directive section 4 repeats the separation by name; the arena allocates
    // `PV_STRIDE_B` per vertex whether or not it writes all of it.
    //
    // IT IS A PARAMETER RATHER THAN THE PACKAGE CONSTANT, and the reason is the
    // hazard the composer already names for `ARENA_CHUNKS`: two independent
    // defaults that happen to agree are two things that can drift apart with
    // nothing to say which one moved. The composer passes ONE localparam to
    // this block and to the arena, so a stride change reaches both or neither.
    parameter int unsigned PV_STRIDE_B = 32
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
    output var logic        t_illegal_o,       // a MALFORMED descriptor
    // ---- SCHEMA v2's FIVE, AND THEY ARE WHY THE RECORD GREW ----------------
    // `zhao_geom_setup` consumes `tri_area2_i` and the four bounds of the
    // section 8 SCISSORED scan box. Before TriangleDescriptor v2 the record
    // carried none of them, so this walk could offer a triangle no back end
    // could accept -- which is what "the planes are recomputable" hid for six
    // packets. They are READ BACK, never recomputed here: `kc2` is DEFINED from
    // `area2` in GEOM.SETUP, so the barycentric identity recovers nothing and a
    // walk-side derivation would be correct for any garbage value whatever.
    output var logic signed [47:0] t_area2_o,
    output var logic signed [11:0] t_min_x_o,
    output var logic signed [11:0] t_max_x_o,
    output var logic signed [11:0] t_min_y_o,
    output var logic signed [11:0] t_max_y_o,
    // ---- THE TILE LIST'S BRACKETS (SWAPCLOSE, 2026-09-27) -----------------
    // `zhao_raster_tile_pipe_v2` clears a tile's coverage on `job_first_i` and
    // RESOLVES the tile on `job_last_i` -- `rs_state_q <= last_q ? RS_SWAP :
    // RS_IDLE`. So these two are not decoration: `last` is what writes the tile
    // out, and a walk that never asserts it draws nothing.
    //
    // THEY ARE PUBLISHED FROM HERE BECAUSE ONLY HERE KNOWS. `t_last_o` is the
    // `else` arm of W_TD_EMIT's own branch below -- no more ids in this chunk
    // AND the chain does not continue -- which is the same decision that sends
    // the FSM to W_END. A consumer trying to derive it would have to see
    // whether another triangle is coming, which means letting this block
    // advance, which destroys the triangle it is still holding. That is a skid
    // buffer and a deadlock; this is a wire off a decision already made.
    output var logic        t_first_o,
    output var logic        t_last_o,
    // ---- SCHEMA v3's MATERIAL STATE (METASIDE, 2026-09-27) ---------------
    // The 115 live bits of the 378 that `zhao_geom_bin_pipe_v2` takes as
    // `tri_flat_request_i`, `tri_continuation_tail_i` and
    // `tri_fragment_state_i`. `zhao_geom_setup` mentions all three ZERO times
    // and `zhao_geom_attrpack` once, disclaiming one -- so no back end fed
    // from this walk could ever have produced them, however faithfully it was
    // built. Opaque here; `zhao_console_core` owns the field layout because
    // that is where the LIVE path composes it, and one layout with two
    // expressions is how two copies come to disagree.
    output var logic [127:0] t_matstate_o,
    // ---- THE KEY, WHICH IS THE ADDRESS THIS RECORD WAS FETCHED AT --------
    // I54's arena triangle index rides `tri_continuation_tail_i[41:24]`. On
    // the live path it comes from `u_geom_tidq`; on this path it is the id the
    // chunk named and this walk addressed the descriptor with
    // (`w_tri_base_q + id * TD_B`). It is emitted rather than stored INSIDE
    // the record because a record does not carry its own address -- that is
    // SCHEMA v2's own reasoning for keeping `untex` out, one fact one place.
    output var logic [17:0] t_arena_id_o,
    // AND THE HANDLE IS NOT TRUNCATED SILENTLY. A chunk id is 23 bits wide in
    // the serialised record and the tail's field is 18, so the top five are
    // structurally zero for every chunk this console writes -- and "the ids
    // are small in practice" is precisely the kind of premise directive
    // section 4 refuses ("do not ... truncate a full handle"). This counts the
    // records where it is not true instead of asserting that it always is.
    // Fireable with legal stimulus: offer a chunk holding an id >= 2^18.
    output var logic [31:0] tri_id_wide_o,

    // ---- THE THREE PROJECTED VERTICES THE DESCRIPTOR NAMES ------------------
    // THIS IS THE ARM THE BLOCK DELIBERATELY DID NOT DRIVE, and the comment
    // that declined it was right at the time. It said a walk-side vertex decode
    // would be "a second consumer of the same record with no reader -- an
    // uncashed cheque authored on purpose", and while the record stored colour
    // through `unit8_of_fx16` that was doubly true: the bytes could not have
    // rebuilt R234 D1's three Gouraud planes even with a reader.
    //
    // SCHEMA v2 CHANGED THE RECORD, NOT THE ARGUMENT'S FORM. The six plane
    // inputs are now stored at their full 32-bit slot widths, so the planes are
    // reconstructible, and `reports/DECISION-20260927-I55-SWAP-ARCHITECTURE.md`
    // names the reader: the time-multiplexed `u_geom_setup` / `u_geom_attrpack`
    // pair, fed from here during the raster drain window.
    //
    // WHAT IS EMITTED IS THE DECODED FIELD, NOT A PACKED ATTRIBUTE PACKET. The
    // ruling-5 slot order lives in the composer, where `GEOM_ATTR_SLOT_*` are
    // already declared and already spent on the live path; packing them a
    // second time here would be a second expression of that layout, and the two
    // copies would be free to disagree about which slot carries green.
    //
    // x AND y ARE RETURNED AT THEIR STORED WIDTH. `zhao_geom_parambuf` widens
    // them to 32 bits sign-extended from s21 as a decoding convenience; s21 is
    // what the record holds and what `zhao_geom_setup` takes, so narrowing back
    // to 21 here is exact and is not a domain claim.
    output var logic signed [20:0] t_a_x_o,
    output var logic signed [20:0] t_a_y_o,
    output var logic        [23:0] t_a_invw_o,
    output var logic signed [31:0] t_a_uow_o,
    output var logic signed [31:0] t_a_vow_o,
    output var logic signed [31:0] t_a_r_o,
    output var logic signed [31:0] t_a_g_o,
    output var logic signed [31:0] t_a_b_o,
    output var logic signed [31:0] t_a_alpha_o,
    output var logic signed [20:0] t_b_x_o,
    output var logic signed [20:0] t_b_y_o,
    output var logic        [23:0] t_b_invw_o,
    output var logic signed [31:0] t_b_uow_o,
    output var logic signed [31:0] t_b_vow_o,
    output var logic signed [31:0] t_b_r_o,
    output var logic signed [31:0] t_b_g_o,
    output var logic signed [31:0] t_b_b_o,
    output var logic signed [31:0] t_b_alpha_o,
    output var logic signed [20:0] t_c_x_o,
    output var logic signed [20:0] t_c_y_o,
    output var logic        [23:0] t_c_invw_o,
    output var logic signed [31:0] t_c_uow_o,
    output var logic signed [31:0] t_c_vow_o,
    output var logic signed [31:0] t_c_r_o,
    output var logic signed [31:0] t_c_g_o,
    output var logic signed [31:0] t_c_b_o,
    output var logic signed [31:0] t_c_alpha_o,
    // THE UNTEXTURED DECLARATION, AND IT IS NOT A NEW FIELD. Ruling R197's bit
    // is already in the record: `zhao_geom_vertid.sv:513` publishes
    // `pv_status_o = {4'd0, shareable_q, untex_q, dom_q}`, so it is `status[2]`
    // of every vertex the primitive published. It is taken from vertex A, and
    // `t_pv_split_o` below counts the case where the three disagree -- which
    // cannot happen while one primitive publishes all three, and is therefore a
    // counter that owes a demonstration rather than an assumption.
    output var logic        t_untex_o,

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
    // ---- the vertex arm's own evidence --------------------------------------
    // Three per emitted triangle. `verts_read_o` counts RECORDS FETCHED, so the
    // ratio `verts_read_o == 3 * tris_emitted_o` is an invariant a reader can
    // check: a fetch arm that silently skipped a vertex and reused the previous
    // one would keep every handshake healthy and break only this.
    output var logic [31:0] verts_read_o,
    // A ProjectedVertex whose STATUS BYTE is malformed. `zhao_geom_parambuf`'s
    // `pv_illegal_o` has had no consumer in this design since v2 moved the s21
    // refusal to the encoder; this arm is its first reachable reader, and the
    // fault it now watches -- reserved bits nonzero on the way back out of
    // SDRAM -- is one only the round trip can produce.
    output var logic [31:0] verts_illegal_o,
    // THE THREE VERTICES OF ONE TRIANGLE DISAGREEING ABOUT `untex`. Unreachable
    // while a single primitive publishes all three, which is why it is declared
    // here rather than left as a silent pick of vertex A's bit: the alternative
    // is a block that quietly prefers one vertex and never says that the others
    // were consulted.
    output var logic [31:0] t_pv_split_o,
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
  localparam int unsigned TD_B  = ZHAO_PARAMBUF_TD_BYTES;  // w=48 bytes
  localparam int unsigned CKW   = CK_B * 8;                // w=512 bits
  // The low bits an aligned address must have clear.
  localparam int unsigned ALIGN_LSB = $clog2(BURST_ALIGN_B);  // w=4 bits
  localparam int unsigned CK_BEATS = CK_B / 8;             // w=8 beats
  localparam int unsigned TD_BEATS = TD_B / 8;             // w=4 beats
  localparam int unsigned PV_B  = ZHAO_PARAMBUF_PV_BYTES;  // w=32 bytes
  localparam int unsigned PVW   = PV_B * 8;                // w=256 bits
  localparam int unsigned PV_BEATS = PV_B / 8;             // w=4 beats

  // synthesis translate_off
  initial begin
    if (CHUNK_IDS != 14)
      $fatal(1, "zhao_geom_paramwalk: R7's chunk holds fourteen ids");
    if (MAX_WALK < 1)
      $fatal(1, "zhao_geom_paramwalk: a walk bound of zero follows nothing");
    // THE STRIDE MUST COVER THE RECORD. A stride narrower than the record makes
    // vertex k+1 overlap vertex k, so a read of k returns bytes from both and
    // decodes to a plausible vertex. The arena refuses the same breach from the
    // writing side; this is the reading side of one law.
    if (PV_STRIDE_B < PV_B)
      $fatal(1, "zhao_geom_paramwalk: PV_STRIDE_B is narrower than the record");
    // A RECORD THAT IS NOT A WHOLE NUMBER OF BEATS IS SHORT-READ WITHOUT A
    // DIAGNOSTIC. This is the guard `PROJECTEDVERTEX-V2` added on the writing
    // side after finding `m_beats_q <= 4'(PV_B / 8)` truncating in silence; the
    // reader divides by the same eight and owes the same guard.
    if ((PV_B % 8) != 0)
      $fatal(1, "zhao_geom_paramwalk: PV_B is not a whole number of 8-byte beats");
    // The burst buffer is sized from PVW, so a record wider than the descriptor
    // buffer it shares a shift path with would silently drop its top beats.
    if (PV_BEATS > 15)
      $fatal(1, "zhao_geom_paramwalk: PV_BEATS does not fit the 4-bit beat counter");
  end
  // synthesis translate_on

  // ------------------------------------------------------------- the walk --
  // FIVE BITS AND NOT FOUR. The vertex arm takes the state count from fifteen
  // to nineteen; at `logic [3:0]` the four new names would have wrapped onto
  // W_IDLE..W_DIR_VERD and the walk would have re-entered the directory read
  // from the middle of a triangle, which every per-record check downstream
  // would have passed.
  typedef enum logic [4:0] {
    W_IDLE,
    W_SCR,        // ask the arena for the scratch
    W_DIR_REQ, W_DIR_VERD, W_DIR_BEAT, W_DIR_CHECK,
    W_CK_REQ,  W_CK_VERD,  W_CK_BEAT,  W_CK_CHECK,
    W_TD_REQ,  W_TD_VERD,  W_TD_BEAT,
    // ---- the vertex arm, between the descriptor and the emit ---------------
    // The descriptor NAMES three vertices; until they are fetched the triangle
    // is three ids and cannot feed a back end. These four states sit between
    // W_TD_BEAT and W_TD_EMIT so that a triangle is offered to the consumer
    // COMPLETE or not at all -- an emit that ran before its vertices arrived
    // would hand over the PREVIOUS triangle's corners with this triangle's ids,
    // which is this repository's own metadata-swap defect in a new place.
    W_PV_REQ,  W_PV_VERD,  W_PV_BEAT,  W_PV_DEC,
    W_TD_EMIT,
    W_END
  } wstate_e;
  wstate_e wstate_q;

  // LATCHED AT WALK START. Every address and every staleness test is against
  // these and never against the live pub_* inputs.
  logic [15:0] w_gen_q;
  logic [26:0] w_tri_base_q, w_chunk_base_q;
  // THE ID THAT ADDRESSED THE DESCRIPTOR IN HAND. Loaded by the SAME two
  // assignments that load `m_addr_q` from it, so the id and the bytes it
  // fetched cannot separate.
  logic [22:0] w_tri_id_q;
  // THE VERTEX REGION'S BASE. `pub_vert_base_i` has been a port of this block
  // since it was written and was used at exactly ONE site -- the directory
  // round-trip compare -- because nothing here addressed a vertex. It is
  // latched at walk start now, under the same enable as the other two bases.
  logic [26:0] w_vert_base_q;
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
  // WIDTH-DERIVED SINCE SCHEMA v2, AND IT WAS A LITERAL. At TD_B = 32 this
  // register holds 256 bits. Its declaration and the beat shift below were
  // BOTH written `[127:0]`/`[127:64]` while `TD_BEATS` was already derived from
  // `TD_B`, so the schema change had to find two typed numbers in a block whose
  // third copy of the same fact was already automatic.
  //
  // AND THE FAILURE MODE IS LOUD, WHICH IS WORTH RECORDING BECAUSE I FIRST
  // WROTE THE OPPOSITE. Leaving either number at 128 while the other moves is
  // a width MISMATCH, and it was fire tested rather than argued: restoring the
  // literal `td_buf_q[127:64]` under a 256-bit `td_buf_q` fails the build with
  //
  //   %Warning-WIDTHEXPAND: zhao_geom_paramwalk.sv:1017: Operator ASSIGNDLY
  //   expects 256 bits on the Assign RHS, but Assign RHS's REPLICATE generates
  //   128 bits.  %Error: Exiting due to 1 warning(s)
  //
  // -- so this is a cheap, immediate error and NOT a silent truncation. My
  // first version of this comment claimed the descriptor would have decoded out
  // of the wrong bytes "with every instrument in the block still balanced",
  // which is wrong in the ALARMING direction: it would send the next reader
  // hunting a quiet fault the toolchain refuses in seconds. The state that
  // WOULD be silent is both numbers agreeing at the wrong width, and a derived
  // width is what makes that unreachable.
  //
  // WHAT THE FIRE TEST ALSO SETTLED, because the run that followed it lied:
  // the broken build returned BUILD_RC=1 and the test binary then ran STALE and
  // printed the previous run's "0/620 checks failed" -- the documented
  // stale-binary trap, caught only by reading the build's exit code rather than
  // the pipeline's.
  logic [TD_B*8-1:0] td_buf_q;
  // A THIRD BUFFER, for the same reason there is a second. `r_buf_q` must hold
  // the chunk while its ids are walked and `td_buf_q` must hold the descriptor
  // while its three vertices are fetched -- the descriptor's ids are re-read on
  // every one of the three requests, so a shared buffer would destroy them on
  // the first vertex and the second and third would be fetched from whatever
  // the vertex bytes happened to decode as.
  logic [PVW-1:0] pv_buf_q;
  // Which of the descriptor's three vertices is in flight. Two bits, values
  // 0..2; the value 3 is never reached and the FSM does not depend on it.
  logic [1:0]     pv_idx_q;
  // THE THREE DECODED VERTICES, LATCHED. Arrays at MODULE scope, which is the
  // form this tree has measured as inferring correctly -- `zhao_geom_arenabin`
  // cost 146,414 registers against 1,010 by declaring storage inside a
  // `generate for`, and entry I55 carries the three map rows. These are 3-entry
  // arrays of a few hundred bits and are flip-flops either way; the form is
  // chosen so the rule is not eroded by an exception nobody re-measures.
  logic signed [20:0] v_x_q     [3];
  logic signed [20:0] v_y_q     [3];
  logic        [23:0] v_invw_q  [3];
  logic        [7:0]  v_status_q[3];
  logic signed [31:0] v_uow_q   [3];
  logic signed [31:0] v_vow_q   [3];
  logic signed [31:0] v_r_q     [3];
  logic signed [31:0] v_g_q     [3];
  logic signed [31:0] v_b_q     [3];
  logic signed [31:0] v_alpha_q [3];
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
  // Set by the first ACCEPTED emit of a walk and cleared when a walk starts, so
  // `t_first_o` is true for exactly one reference per walk. Cleared at the
  // start rather than at W_END, because a walk that fails part way through must
  // still begin the next one with a clean bracket.
  logic        emitted_any_q;

  // ------------------------------------------------------------- decoding --
  // The one decoder. Fed from the burst buffer for chunks, and from the low
  // 128 bits of the same buffer for descriptors.
  logic        dec_ck_valid_c, dec_td_valid_c, dec_pv_valid_c;
  // THE TOP ELEVEN BITS OF x AND y ARE DELIBERATELY NOT READ, and the waiver
  // says so rather than widening the port to make a warning go away.
  // `zhao_geom_parambuf` hands these back 32 bits wide, SIGN-EXTENDED from the
  // s21 the record actually stores; `zhao_geom_setup` takes s21. So `[20:0]` is
  // the stored value returned to its stored width -- exact, and not a domain
  // claim. Bits [31:21] are the sign extension the decoder just added and carry
  // no information this block did not already have.
  /* verilator lint_off UNUSEDSIGNAL */
  logic signed [31:0] pv_x_c, pv_y_c;
  /* verilator lint_on UNUSEDSIGNAL */
  logic signed [31:0] pv_uow_c, pv_vow_c;
  logic signed [31:0] pv_r_c, pv_g_c, pv_b_c, pv_alpha_c;
  logic        [23:0] pv_invw_c;
  logic        [7:0]  pv_status_c;
  logic               pv_illegal_c;
  logic        [15:0] pv_id_c;
  logic        [26:0] pv_addr_c;
  logic [31:0] ck_next_c;
  logic [15:0] ck_count_c;
  logic        ck_stale_c, ck_illegal_c, ck_follow_c;
  logic [15:0] td_v0_c, td_v1_c, td_v2_c, td_material_c;
  logic [31:0] td_raster_c, td_source_c;
  logic        td_illegal_c;
  logic signed [47:0] td_area2_c;
  logic signed [11:0] td_min_x_c, td_max_x_c, td_min_y_c, td_max_y_c;
  logic [127:0] td_matstate_c;
  // ---- THE PORT WIDTH IS A LITERAL AND THE LAW IS THIS GUARD (METASIDE) ---
  // `tools/quartus/gen_prod_top.py` resolves a packed port width by evaluating
  // the expression against MODULE parameters only; a PACKAGE constant is not
  // in that table, so `[ZHAO_TD_MATSTATE_W-1:0]` made it SKIP this block -- and
  // a skipped block is silently absent from the generated production top,
  // which is a regression no gate spells out. The tell is the instance COUNT
  // moving: 89 -> 86, in a line nobody reads. So the port carries a literal and
  // the package stays authoritative through the check below, which fails
  // ELABORATION rather than letting the two drift.
  //
  // CLAUDE.md: `--lint-only` does NOT run `initial` blocks, so a clean lint is
  // not evidence about this guard; elaboration in a Verilator test binary and
  // quartus_map are. And Quartus 17.0 rejects a bare module-scope `if`, which
  // is why it is inside `initial begin`.
  // synthesis translate_off
  initial begin : p_matstate_width
    if (ZHAO_TD_MATSTATE_W != 128)
      $fatal(1, "zhao_geom_paramwalk: ZHAO_TD_MATSTATE_W moved; the literal port width did not");
  end
  // synthesis translate_on


  assign dec_ck_valid_c = (wstate_q == W_CK_CHECK);
  // THE DESCRIPTOR'S DECODE IS HELD ACROSS THE VERTEX FETCH. `td_valid_i` used
  // to be asserted only in W_TD_EMIT, which was correct while the emit followed
  // the descriptor's last beat directly. The vertex arm now sits between them,
  // and the descriptor's ids are read out of `td_buf_q` to ADDRESS each vertex
  // -- so the decode has to be live in the fetch states too, or the walk would
  // address vertex slots from a decoder that is being told its bytes are not
  // valid and answers with its idle output, i.e. slot zero three times.
  assign dec_td_valid_c = (wstate_q == W_TD_EMIT) || (wstate_q == W_PV_REQ)
                       || (wstate_q == W_PV_VERD) || (wstate_q == W_PV_BEAT)
                       || (wstate_q == W_PV_DEC);
  assign dec_pv_valid_c = (wstate_q == W_PV_DEC);

  /* verilator lint_off PINCONNECTEMPTY */
  zhao_geom_parambuf #(
      .CHUNK_IDS    (CHUNK_IDS),
      .ARENA_CHUNKS (ARENA_CHUNKS)
  ) u_rec (
      .clk, .rst_n,
      // THE VERTEX ARM IS DRIVEN. The tie-off that stood here said a walk-side
      // vertex decode would be "a second consumer of the same record with no
      // reader -- an uncashed cheque authored on purpose", and it was right
      // twice over: there was no reader, and under schema v1 the bytes could
      // not have fed one, because colour was stored through `unit8_of_fx16`
      // and three of Packet-D's six planes were unrecoverable from it.
      //
      // Schema v2 stores all six plane inputs at full width, and
      // `reports/DECISION-20260927-I55-SWAP-ARCHITECTURE.md` names the reader.
      // The cheque is cashed here rather than re-authored.
      .pv_valid_i (dec_pv_valid_c),
      .pv_bytes_i (pv_buf_q),
      .pv_x_o (pv_x_c), .pv_y_o (pv_y_c),
      .pv_invw_o (pv_invw_c), .pv_status_o (pv_status_c),
      .pv_uow_o (pv_uow_c), .pv_vow_o (pv_vow_c),
      .pv_r_o (pv_r_c), .pv_g_o (pv_g_c), .pv_b_o (pv_b_c),
      .pv_alpha_o (pv_alpha_c),
      // R7's 8-bit view is DERIVED by the decoder and is not taken here. This
      // arm exists to feed the six full-precision planes; re-deriving the byte
      // form beside them would put two representations of one colour on one
      // interface, and a consumer would be free to pick the lossy one.
      .pv_rgba_o (),
      .pv_illegal_o (pv_illegal_c),

      .td_valid_i (dec_td_valid_c),
      .td_bytes_i (td_buf_q),
      .td_matstate_o (td_matstate_c),
      // THE SEALED COUNT IS THE ONE THIS WALK STARTED UNDER. A live
      // `pub_tris_i` here would let a descriptor from the old frame be
      // validated against the new frame's seal.
      .td_sealed_vertices_i (w_verts_q),
      .td_v0_o (td_v0_c), .td_v1_o (td_v1_c), .td_v2_o (td_v2_c),
      .td_material_o (td_material_c), .td_raster_o (td_raster_c),
      .td_source_o (td_source_c),
      // SCHEMA v2's five, straight out of the same decoder instance that
      // reads the ids -- so there is exactly one place in the design that
      // knows where `area2` sits in the record.
      .td_area2_o (td_area2_c), .td_min_x_o (td_min_x_c),
      .td_max_x_o (td_max_x_c), .td_min_y_o (td_min_y_c),
      .td_max_y_o (td_max_y_c),
      .td_illegal_o (td_illegal_c),

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

  // THE VERTEX SLOT ADDRESS, AND IT IS COMBINATIONAL RATHER THAN LATCHED.
  //
  // The other three reads latch `m_addr_q` in the state that decides them. A
  // vertex cannot: `td_buf_q` is still being shifted when W_TD_BEAT picks the
  // next state, so the descriptor's ids are not readable there, and `pv_idx_q`
  // then advances between the three requests. A latch would have to be loaded
  // from two places with the second easy to miss -- and the failure that
  // produces is fetching v0 three times, which decodes cleanly, yields a
  // degenerate triangle, and moves no counter in this block.
  //
  // `w_vert_base_q` is the base this walk STARTED under, never the live
  // `pub_vert_base_i`: the same law the tri and chunk bases already follow, and
  // for the same reason -- a frame that changed mid-walk would otherwise read
  // the new frame's vertices through the old frame's descriptor.
  //
  // The id comes from the DECODER's outputs, not from a second slice of
  // `td_buf_q`. `zhao_geom_parambuf` owns the descriptor's layout, and a walker
  // that re-sliced it would be the second implementation this block's header
  // exists to refuse.
  always_comb begin
    unique case (pv_idx_q)
      2'd0:    pv_id_c = td_v0_c;
      2'd1:    pv_id_c = td_v1_c;
      default: pv_id_c = td_v2_c;
    endcase
  end
  wire [26:0] pv_index_c = 27'(pv_id_c);
  assign pv_addr_c = 27'(w_vert_base_q + 27'(pv_index_c * 27'(PV_STRIDE_B)));

  // ------------------------------------------------------------- the port --
  always_comb begin
    guard_req_o        = '0;
    guard_req_o.valid  = (wstate_q == W_DIR_REQ) || (wstate_q == W_CK_REQ)
                      || (wstate_q == W_TD_REQ) || (wstate_q == W_PV_REQ);
    guard_req_o.write  = 1'b0;            // this block never writes
    guard_req_o.client = cfg_vram_client_i;
    // The vertex arm's address is the only one this block does not latch; see
    // the paragraph above. Every other state drives `m_addr_q` unchanged.
    guard_req_o.addr   = (wstate_q == W_PV_REQ) ? pv_addr_c : m_addr_q;
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
  // Combinational off the decoder, exactly like the six fields above, and
  // held for the whole of W_TD_EMIT because `td_buf_q` is not touched again
  // until the next descriptor request.
  assign t_area2_o    = td_area2_c;
  assign t_min_x_o    = td_min_x_c;
  assign t_max_x_o    = td_max_x_c;
  assign t_min_y_o    = td_min_y_c;
  assign t_max_y_o    = td_max_y_c;
  assign t_matstate_o = td_matstate_c;
  // The id the CURRENT descriptor was fetched with, latched beside the
  // address that used it -- the same enable, which is the whole of why it
  // cannot be one behind. `u_geom_tidq` was one behind and mis-attributed
  // 74 of 75 triangles with every range guard passing; that defect is a
  // separate register loaded by a separate event, and this is not one.
  assign t_arena_id_o = w_tri_id_q[17:0];

  // THE BRACKETS. `t_last_c` is written as the NEGATION OF THE TWO CONTINUE
  // CONDITIONS rather than as a third copy of the end condition, so it cannot
  // drift away from the branch it describes: W_TD_EMIT continues if there are
  // more ids in this chunk, or if the chain may be followed, and otherwise goes
  // to W_END. If either of those tests is ever changed, this line is wrong in
  // the same edit and the directed test says so -- which is the point of
  // writing it this way round.
  wire t_more_ids_c  = (({12'd0, id_idx_q} + 16'd1) < id_count_q);
  assign t_last_o    = (wstate_q == W_TD_EMIT) && !t_more_ids_c && !ck_follow_q;
  assign t_first_o   = (wstate_q == W_TD_EMIT) && !emitted_any_q;

  // The three fetched vertices, in the descriptor's own order. Index 0 is v0,
  // which `zhao_geom_setup` and `zhao_geom_attrpack` both call corner A.
  assign t_a_x_o     = v_x_q[0];
  assign t_a_y_o     = v_y_q[0];
  assign t_a_invw_o  = v_invw_q[0];
  assign t_a_uow_o   = v_uow_q[0];
  assign t_a_vow_o   = v_vow_q[0];
  assign t_a_r_o     = v_r_q[0];
  assign t_a_g_o     = v_g_q[0];
  assign t_a_b_o     = v_b_q[0];
  assign t_a_alpha_o = v_alpha_q[0];
  assign t_b_x_o     = v_x_q[1];
  assign t_b_y_o     = v_y_q[1];
  assign t_b_invw_o  = v_invw_q[1];
  assign t_b_uow_o   = v_uow_q[1];
  assign t_b_vow_o   = v_vow_q[1];
  assign t_b_r_o     = v_r_q[1];
  assign t_b_g_o     = v_g_q[1];
  assign t_b_b_o     = v_b_q[1];
  assign t_b_alpha_o = v_alpha_q[1];
  assign t_c_x_o     = v_x_q[2];
  assign t_c_y_o     = v_y_q[2];
  assign t_c_invw_o  = v_invw_q[2];
  assign t_c_uow_o   = v_uow_q[2];
  assign t_c_vow_o   = v_vow_q[2];
  assign t_c_r_o     = v_r_q[2];
  assign t_c_g_o     = v_g_q[2];
  assign t_c_b_o     = v_b_q[2];
  assign t_c_alpha_o = v_alpha_q[2];
  // Ruling R197's bit, out of the status byte vertex A was published with.
  assign t_untex_o   = v_status_q[0][2];



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
  // AND THE CHUNK'S FIRST ID IS ITS OWN WIRE, for exactly the reason the two
  // above are -- the paragraph over them was right about the hazard and missed
  // one of its two sites.
  //
  // REPAIR, 2026-09-26 (CHUNKSER). The chunk-decode arm issues the first
  // descriptor read of a new chunk on the SAME CLOCK it assigns
  // `id_idx_q <= 0`. Non-blocking, so `id_c` there is indexed by the PREVIOUS
  // chunk's final index, and the first descriptor of every chunk after the
  // very first was fetched from the wrong id.
  //
  // IT WAS UNREACHABLE UNTIL ENTRY I54 EXISTED. Nothing in this tree had ever
  // walked a chain of chunks holding DIFFERENT ids -- the chunk intake was
  // tied to zero, and the hand-driven cases only ever walked one chunk from a
  // fresh reset, where `id_idx_q` is 0 and the bug is invisible. MEASURED the
  // first time a real producer filled the arena: a tile whose references are
  // 4..22 walked back as `4 5 ... 16 17 [17] 19 20 21 22` -- an id that is in
  // range, decodes cleanly and names the wrong triangle, which is precisely
  // the fault console entry I54 is written against, arriving from the READER
  // instead of the writer. Every counter in the arena, the walker and the
  // parambuf read zero throughout.
  // ENFORCED-BY: tests/geometry/geom_chunkser_directed.cpp
  wire [22:0] id0_c    = r_buf_q[64 +: 23];

  // --------------------------------------------------------------- core ----
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      wstate_q       <= W_IDLE;
      w_gen_q        <= 16'd0;
      w_tri_base_q   <= 27'd0;
      w_tri_id_q     <= 23'd0;
      tri_id_wide_o  <= 32'd0;
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
      w_vert_base_q  <= 27'd0;
      pv_buf_q       <= '0;
      pv_idx_q       <= 2'd0;
      for (int unsigned k = 0; k < 3; k++) begin
        v_x_q[k]      <= 21'sd0;
        v_y_q[k]      <= 21'sd0;
        v_invw_q[k]   <= 24'd0;
        v_status_q[k] <= 8'd0;
        v_uow_q[k]    <= 32'sd0;
        v_vow_q[k]    <= 32'sd0;
        v_r_q[k]      <= 32'sd0;
        v_g_q[k]      <= 32'sd0;
        v_b_q[k]      <= 32'sd0;
        v_alpha_q[k]  <= 32'sd0;
      end
      verts_read_o    <= '0;
      verts_illegal_o <= '0;
      t_pv_split_o    <= '0;
      id_idx_q       <= 4'd0;
      id_count_q     <= 16'd0;
      emitted_any_q  <= 1'b0;
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
          && (wstate_q != W_TD_BEAT) && (wstate_q != W_PV_BEAT))
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
      // WIDENED TO WATCH THE PORT RATHER THAN THE REGISTER, MUXBUILD
      // 2026-09-27. It used to test `m_addr_q`, which was the only source of
      // `guard_req_o.addr` when it was written. The vertex arm adds a second
      // source, and a tripwire still reading the register would have gone
      // silent on exactly the requests this packet added -- a detector that
      // keeps its name, keeps reading zero, and no longer watches the thing it
      // is quoted about. It remains an INVARIANT over one value against a
      // constant, so the lockstep-blindness question does not arise.
      if (guard_req_o.valid && (guard_req_o.addr[ALIGN_LSB-1:0] != '0))
        burst_unaligned_o <= burst_unaligned_o + 32'd1;

      case (wstate_q)
        // ---- start -----------------------------------------------------
        W_IDLE: if (walk_valid_i && pub_valid_i) begin
          // IDENTITY LATCHED HERE, all of it, in one enable.
          w_gen_q        <= pub_gen_i;
          w_tri_base_q   <= pub_tri_base_i;
          w_chunk_base_q <= pub_chunk_base_i;
          w_vert_base_q  <= pub_vert_base_i;
          // No saturation: the port is as wide as the published count, so
          // the seal the decoder is tested against is the seal the arena
          // actually published, for every value the arena can publish.
          w_verts_q      <= pub_verts_i;
          w_chunk_q      <= walk_head_i[22:0];
          w_depth_q      <= 16'd0;
          w_failed_q     <= 1'b0;
          emitted_any_q  <= 1'b0;
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
              // `id0_c`, NOT `id_c`: `id_idx_q <= 0` is assigned above on this
              // same clock, so `id_c` here would name the PREVIOUS chunk's
              // last index. See the wire's own comment.
              m_addr_q  <= 27'(w_tri_base_q + 27'(id0_c * 23'(TD_B)));
              w_tri_id_q <= id0_c;
              if (id0_c[22:18] != 5'd0) tri_id_wide_o <= tri_id_wide_o + 32'd1;
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
          td_buf_q <= {beat_data_i, td_buf_q[TD_B*8-1:64]};
          r_beat_q <= r_beat_q + 4'd1;
          if (beat_last_i) begin
            if (r_beat_q != (r_beats_q - 4'd1)) begin
              short_burst_o <= short_burst_o + 32'd1;
              w_failed_q    <= 1'b1;
              wstate_q      <= W_END;
            end else begin
              // THE DESCRIPTOR IS DECODED BUT THE TRIANGLE IS NOT COMPLETE.
              // Three vertex reads sit between here and the emit.
              //
              // A MALFORMED DESCRIPTOR IS NOT FOLLOWED. `td_illegal_c` means an
              // id is past the frame's sealed vertex count, so the slot it
              // names is outside what this frame wrote -- reading it would put
              // a guard request on an address the arena never published and
              // decode whatever last occupied it. The triangle is still EMITTED
              // AND FLAGGED, which is R7's "rejected and counted" and is what
              // the arm below already did; what is skipped is the fetch, and
              // the vertex registers keep the previous triangle's values rather
              // than being filled with a convenient zero. `t_illegal_o` travels
              // with the record to say so.
              if (td_illegal_c) begin
                wstate_q <= W_TD_EMIT;
              end else begin
                // Length and beat count are the same for all three vertices and
                // are loaded once here; only the ADDRESS varies with the index,
                // and that one is combinational.
                pv_idx_q  <= 2'd0;
                m_len_q   <= 7'(PV_B);
                r_beats_q <= 4'(PV_BEATS);
                r_beat_q  <= 4'd0;
                wstate_q  <= W_PV_REQ;
              end
            end
          end
        end
        // ---- one ProjectedVertex -----------------------------------------
        // Three passes of these four states per descriptor. The address is
        // computed HERE rather than latched at W_TD_BEAT, because `pv_idx_q`
        // advances in W_PV_DEC and an address latched once would fetch vertex
        // v0 three times -- which decodes cleanly, produces a degenerate
        // triangle, and would be visible in no counter in this block.
        W_PV_REQ: if (guard_rsp_i.ready) begin
          wstate_q <= W_PV_VERD;
        end

        W_PV_VERD: begin
          if (guard_rsp_i.violation) begin
            guard_denied_o <= guard_denied_o + 32'd1;
            w_failed_q <= 1'b1;
            wstate_q   <= W_END;
          end else if (guard_rsp_i.ok) begin
            r_beat_q <= 4'd0;
            wstate_q <= W_PV_BEAT;
          end
        end

        // NEITHER THE CHUNK NOR THE DESCRIPTOR IS TOUCHED HERE. The vertex
        // shifts into its own buffer, so `r_buf_q` still holds the chunk whose
        // ids are being walked and `td_buf_q` still holds the descriptor whose
        // ids address these three vertices.
        W_PV_BEAT: if (beat_valid_i) begin
          pv_buf_q <= {beat_data_i, pv_buf_q[PVW-1:64]};
          r_beat_q <= r_beat_q + 4'd1;
          if (beat_last_i) begin
            if (r_beat_q != (r_beats_q - 4'd1)) begin
              short_burst_o <= short_burst_o + 32'd1;
              w_failed_q    <= 1'b1;
              wstate_q      <= W_END;
            end else begin
              wstate_q <= W_PV_DEC;
            end
          end
        end

        W_PV_DEC: begin
          verts_read_o <= verts_read_o + 32'd1;
          // A MALFORMED STATUS BYTE IS COUNTED AND THE WALK CONTINUES. Unlike a
          // stale chunk, this is not a reason to abandon the chain: the record
          // is the one the descriptor named and its geometry fields are still
          // the bytes that were written. The triangle carries `t_illegal_o` for
          // the identity fault; this counter is the RECORD fault, and the two
          // are different questions about the same triangle.
          if (pv_illegal_c) verts_illegal_o <= verts_illegal_o + 32'd1;
          v_x_q[pv_idx_q]      <= pv_x_c[20:0];
          v_y_q[pv_idx_q]      <= pv_y_c[20:0];
          v_invw_q[pv_idx_q]   <= pv_invw_c;
          v_status_q[pv_idx_q] <= pv_status_c;
          v_uow_q[pv_idx_q]    <= pv_uow_c;
          v_vow_q[pv_idx_q]    <= pv_vow_c;
          v_r_q[pv_idx_q]      <= pv_r_c;
          v_g_q[pv_idx_q]      <= pv_g_c;
          v_b_q[pv_idx_q]      <= pv_b_c;
          v_alpha_q[pv_idx_q]  <= pv_alpha_c;
          // THE UNTEXTURED BIT MUST AGREE ACROSS THE THREE. One primitive
          // publishes all three vertices, so it does; the counter exists
          // because "so it does" is an argument and this is a measurement.
          // Compared against vertex A's byte, which is already latched when
          // index 1 and 2 arrive.
          if ((pv_idx_q != 2'd0) && (pv_status_c[2] != v_status_q[0][2]))
            t_pv_split_o <= t_pv_split_o + 32'd1;
          if (pv_idx_q == 2'd2) begin
            wstate_q <= W_TD_EMIT;
          end else begin
            pv_idx_q <= pv_idx_q + 2'd1;
            wstate_q <= W_PV_REQ;
          end
        end

        W_TD_EMIT: if (t_ready_i) begin
          tris_emitted_o <= tris_emitted_o + 32'd1;
          emitted_any_q  <= 1'b1;
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
            w_tri_id_q <= nxt_id_c;
            if (nxt_id_c[22:18] != 5'd0) tri_id_wide_o <= tri_id_wide_o + 32'd1;
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
