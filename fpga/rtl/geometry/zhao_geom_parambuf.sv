// zhao_geom_parambuf.sv — the external geometry arena's record layer.
//
// ---------------------------------------------------------------------------
// SCOPE, FIRST
// ---------------------------------------------------------------------------
// GEOM.PARAMBUF is where a frame's geometry lives: projected vertices, compact
// triangle descriptors and tile-reference chunks, in LOCAL SDRAM, owned by
// ENGINE1 (ruling R7). This block is its RECORD LAYER -- the three layouts,
// their legality rules, and the chunk walk's staleness gate. It does not own
// SDRAM, does not arbitrate, and does not allocate the arena.
//
// The arena's capacity policy, the quota seal and the frame-fault path are the
// composed block's; what is here is what every one of them will encode and
// decode with, in one place.
//
// ---------------------------------------------------------------------------
// THE THREE RECORDS (R7)
// ---------------------------------------------------------------------------
//   ProjectedVertex, 24 B
//     screen_x s32 (LEGAL RANGE s21), screen_y s32 (legal s21),
//     invw24 + status byte, u_over_w s32, v_over_w s32, rgba8 u32
//
//   TriangleDescriptor, 16 B
//     vertex_id[3] u16, material_id u16, raster_state u32, source_id u32
//
//   Tile-reference chunk, 64 B
//     next_chunk u32, count u16, frame_generation u16, fourteen triangle IDs
//
// ---------------------------------------------------------------------------
// WHY frame_generation IS IN EVERY CHUNK
// ---------------------------------------------------------------------------
// A chunk from last frame reads as a perfectly valid chunk in every other
// respect: its count is sane, its next pointer is inside the arena, its
// triangle ids index real triangles. Nothing about its CONTENT says it is old.
// The generation is the only thing that does, which is why it is per chunk and
// not per arena -- an arena-level stamp cannot catch a chunk that was written
// this frame into a list that was not.
//
// ---------------------------------------------------------------------------
// s32 STORED, s21 LEGAL
// ---------------------------------------------------------------------------
// R7 says screen coordinates are stored as s32 with a legal range of s21. That
// is deliberate slack, and the rule that comes with it is that a value outside
// s21 is a MALFORMED DESCRIPTOR, not a coordinate to be wrapped or clamped.
// Clamping it would place a triangle somewhere plausible; refusing it says the
// producer is wrong.
// ---------------------------------------------------------------------------
// WHY THIS BLOCK IS NOT COMPOSED, MEASURED 2026-09-22 (packet GEOMCLOSE)
// ---------------------------------------------------------------------------
// THE RECORDED BLOCKER IS TRUE AND IT IS NOT THE FIRST OBSTACLE, and the
// correction runs in the UNFLATTERING direction, which is why it is written
// down rather than assumed. Packet WARPBUILD recorded: "its blocker is an
// arena WRITER; every geometry memory client is hard-coded read-only with a
// formal assertion." Every clause of that is accurate. What it understates is
// that a READ of this arena is refused too, so a read-only composition -- the
// obvious cheap first step, and the one a lane would reach for -- is refused
// by construction as well.
//
// MEASURED ON THE CONSTANTS, not read off a comment:
//
//   `spec/memory_rules.md` 5c RULES THE REGION. Bank 3 holds PARAMBUF view 0
//   at [0x0600_0000, 0x0640_0000), view 1 at [0x0640_0000, 0x0680_0000) and
//   the shared prefetch/chunk scratch at [0x0680_0000, 0x06A0_0000), all
//   owned by ENGINE1. So the memory map is not the gap; it is ruled and it is
//   in `zhao_pkg`'s own bank-3 note.
//
//   `zhao_mem_guard` HAS NO WINDOW FOR ANY OF IT. Its `unique case` gives
//   ENGINE1 exactly one arm --
//
//       ZHAO_CLIENT_ENGINE1: pass_ok = shape_ok && render_asset_ok;
//
//   -- and `render_asset_ok` is `!req.write && addr32 >= ZHAO_RENDER_ASSET_BASE
//   && end32 <= ZHAO_RENDER_ASSET_BASE + ZHAO_RENDER_ASSET_SPAN`, which is
//   0x06A0_0000 + 0x0160_0000 = [0x06A0_0000, 0x0800_0000). THE WHOLE PARAMBUF
//   REGION LIES STRICTLY BELOW THAT WINDOW. Not a direction bit short of
//   legal: outside the bounds, in both directions, for the only client that
//   owns it.
//
// SO THE FIRST THING THIS BLOCK NEEDS IS A MEM.GUARD WINDOW, WHICH IS A RULING
// ABOUT THE MEMORY LAW AND NOT A BUILD. The guard's own header calls the asset
// pool "A THIRD WINDOW ... stated plainly rather than folded in", and says
// what keeps the no-escape guarantee intact there: the window is READ-ONLY and
// its bounds are CONSTANTS. A PARAMBUF window is a write arm on a frame-scoped
// arena, which is the harder case, and it is covered by a committed formal
// proof -- `tests/formal/mem_guard_no_escape.sby`, with
// `tests/mutants/zhao_mem_guard_resbound_mutant.sv` already demonstrating that
// removing a containment term makes that proof FAIL.
//
// **THAT ASSERTION IS NOT TO BE WEAKENED TO MAKE A COMPOSITION FIT.** It is
// the statement that nothing escapes the map, it has a positive control, and
// an arena that needs it relaxed is an owner decision about what the console
// is allowed to overwrite. The shape of the decision, stated so it can be
// taken rather than rediscovered:
//
//   * a FIFTH ENGINE1-reachable window over 5c's ranges, or a single window
//     with two arms in the terrain pool's shape (`terrain_ok` requires
//     `req.write`, `terrain_rd_ok` requires `!req.write`, same two constant
//     bounds) -- the precedent is in the guard already;
//   * the two PARAMBUF VIEWS are disjoint "for the same reason the two FB
//     slots are" (5c), so whatever admits a write has to say WHICH view, and
//     that is frame-scoped state like `fb_writer`, not a constant;
//   * `mem_guard_no_escape.sby` extended to cover it, and a committed mutant
//     that makes the extended proof fail.
//
// ONLY THEN IS THE ARENA WRITER THE BLOCKER, and it is a subsystem rather than
// a nodule: a packer that serialises 24-byte ProjectedVertex and 16-byte
// TriangleDescriptor records, a 64-byte chunk allocator with the frame
// generation stamped per chunk, a write-capable ENGINE1 client
// (`zhao_mem_share_wr` already exists and is composed as `u_build_share` for
// TERRAIN_BUILD, so the share is not the missing part), and a chunk walker
// that reads them back. The console today runs this path ON CHIP through
// `zhao_geom_binner_v2`'s bounded chunk arena (CHUNKS = 256, CHUNK_REFS = 4),
// which is what this contract's "active chunk tails" permits and what R7's
// external arena is meant to stop being grown.
//
// WHAT THIS BLOCK IS, FOR THE AVOIDANCE OF THE OTHER MISREADING: pure
// combinational DECODE, bytes in and fields out, with the legality rules. It
// has no memory port and never will. It cannot be composed against the
// existing geometry path by wiring, because nothing in this console holds a
// byte vector in any of these three layouts -- `zhao_geom_assemble` already
// emits a triangle as FIELDS "exactly GEOM.PARAMBUF's layout", which is the
// INVERSE of this block, not a producer for it. Packing fields into bytes here
// so a decoder could unpack them again would be a composition that produces a
// picture and proves nothing.
//
// ---------------------------------------------------------------------------
`default_nettype none

module zhao_geom_parambuf #(
    parameter int unsigned CHUNK_IDS = 14,
    // The arena, in chunk units. A `next_chunk` outside it is malformed.
    parameter int unsigned ARENA_CHUNKS = 65536
) (
    input var logic clk,
    input var logic rst_n,

    // ---- ProjectedVertex: 24 bytes in, fields out ---------------------------
    input  var logic          pv_valid_i,
    input  var logic [191:0]  pv_bytes_i,
    output var logic signed [31:0] pv_x_o,
    output var logic signed [31:0] pv_y_o,
    output var logic [23:0]   pv_invw_o,
    output var logic [7:0]    pv_status_o,
    output var logic signed [31:0] pv_uow_o,
    output var logic signed [31:0] pv_vow_o,
    output var logic [31:0]   pv_rgba_o,
    output var logic          pv_illegal_o,   // outside s21

    // ---- TriangleDescriptor: 16 bytes ---------------------------------------
    input  var logic          td_valid_i,
    input  var logic [127:0]  td_bytes_i,
    // THE FRAME'S VERTEX COUNT -- 18 BITS AND NOT 16, ARENAID 2026-09-25.
    // Owner vacation directive section 4: "IDs 0..65,535 fit u16, but a count
    // of 65,536 requires a wider internal count/limit. Use at least 17 bits
    // for that count instead of silently sacrificing a vertex or representing
    // full capacity as zero."
    //
    // WHAT WAS SUPERSEDED, AND WHERE IT IS WRITTEN DOWN. This port was u16,
    // and because it was, `zhao_geom_paramarena`'s MAX_VERTS defaulted to
    // 65,535 with a header paragraph calling the lost vertex "a DECLARED
    // LOSS", and `zhao_geom_paramwalk` SATURATED the published count at
    // 0xFFFF. Three places, one cause. GEOM.PARAMBUF.md's section "One
    // declared divergence from the tier table above" recorded it as permanent;
    // it is not permanent, it was a port width. 18 rather than 17 because the
    // arena's own cursors, quotas and publication ports are already 18.
    input  var logic [17:0]   td_sealed_vertices_i,   // the frame's vertex count
    output var logic [15:0]   td_v0_o,
    output var logic [15:0]   td_v1_o,
    output var logic [15:0]   td_v2_o,
    output var logic [15:0]   td_material_o,
    output var logic [31:0]   td_raster_o,
    output var logic [31:0]   td_source_o,
    output var logic          td_illegal_o,   // a vertex id past the sealed count

    // ---- tile-reference chunk: 64 bytes -------------------------------------
    input  var logic          ck_valid_i,
    // The fourteen triangle ids are NOT decoded here. This block answers
    // whether the chunk may be walked at all; which ids it holds is the
    // walker's question, and decoding them here would put the walk's fanout
    // into the validity check's timing.
    /* verilator lint_off UNUSEDSIGNAL */
    input  var logic [511:0]  ck_bytes_i,
    /* verilator lint_on UNUSEDSIGNAL */
    input  var logic [15:0]   ck_frame_gen_i,         // the CURRENT frame
    output var logic [31:0]   ck_next_o,
    output var logic [15:0]   ck_count_o,
    output var logic [15:0]   ck_gen_o,
    output var logic          ck_stale_o,             // generation mismatch
    output var logic          ck_illegal_o,           // count or next out of range
    output var logic          ck_follow_o,            // safe to follow next_chunk

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0]   pv_illegal_count_o,
    output var logic [31:0]   td_illegal_count_o,
    output var logic [31:0]   ck_stale_count_o,
    output var logic [31:0]   ck_illegal_count_o
);

  // ---- ProjectedVertex ----------------------------------------------------
  logic signed [31:0] x_c, y_c;
  assign x_c = $signed(pv_bytes_i[  0 +: 32]);
  assign y_c = $signed(pv_bytes_i[ 32 +: 32]);

  assign pv_x_o      = x_c;
  assign pv_y_o      = y_c;
  assign pv_invw_o   = pv_bytes_i[ 64 +: 24];
  assign pv_status_o = pv_bytes_i[ 88 +:  8];
  assign pv_uow_o    = $signed(pv_bytes_i[ 96 +: 32]);
  assign pv_vow_o    = $signed(pv_bytes_i[128 +: 32]);
  assign pv_rgba_o   = pv_bytes_i[160 +: 32];

  // s21 legality: every bit above bit 20 must equal bit 20, which is the
  // definition of "this s32 fits in s21" and needs no comparison.
  /* verilator lint_off UNUSEDSIGNAL */
  function automatic logic fits_s21(input logic signed [31:0] v);
    // Only the sign-extension bits matter: the low twenty carry the value and
    // are legal whatever they hold.
    fits_s21 = (v[31:20] == 12'h000) || (v[31:20] == 12'hFFF);
  endfunction
  /* verilator lint_on UNUSEDSIGNAL */

  assign pv_illegal_o = pv_valid_i && (!fits_s21(x_c) || !fits_s21(y_c));

  // ---- TriangleDescriptor -------------------------------------------------
  logic [15:0] v0_c, v1_c, v2_c;
  assign v0_c = td_bytes_i[ 0 +: 16];
  assign v1_c = td_bytes_i[16 +: 16];
  assign v2_c = td_bytes_i[32 +: 16];

  assign td_v0_o       = v0_c;
  assign td_v1_o       = v1_c;
  assign td_v2_o       = v2_c;
  assign td_material_o = td_bytes_i[48 +: 16];
  assign td_raster_o   = td_bytes_i[64 +: 32];
  assign td_source_o   = td_bytes_i[96 +: 32];

  // A vertex id past the frame's sealed vertex count indexes memory that
  // belongs to no vertex. Refused rather than clamped: clamping would draw a
  // triangle using somebody else's position.
  // The ids are u16 and the seal is u18, so the comparison is written at the
  // WIDER width explicitly. Left implicit it is still correct here, but the
  // next person reading a mixed-width compare has to prove that to themselves.
  assign td_illegal_o = td_valid_i &&
                        ((18'(v0_c) >= td_sealed_vertices_i) ||
                         (18'(v1_c) >= td_sealed_vertices_i) ||
                         (18'(v2_c) >= td_sealed_vertices_i));

  // ---- tile-reference chunk -----------------------------------------------
  logic [31:0] next_c;
  logic [15:0] count_c, gen_c;
  assign next_c  = ck_bytes_i[ 0 +: 32];
  assign count_c = ck_bytes_i[32 +: 16];
  assign gen_c   = ck_bytes_i[48 +: 16];

  assign ck_next_o  = next_c;
  assign ck_count_o = count_c;
  assign ck_gen_o   = gen_c;

  // A chunk from a previous frame is valid in every other respect. The
  // generation is the ONLY thing that distinguishes it.
  assign ck_stale_o = ck_valid_i && (gen_c != ck_frame_gen_i);

  // `count` above the chunk's capacity, or a `next` outside the arena. The
  // sentinel for "no next chunk" is all-ones, which is not an address.
  localparam logic [31:0] CK_NULL = 32'hFFFF_FFFF;
  assign ck_illegal_o = ck_valid_i &&
                        ((count_c > 16'(CHUNK_IDS)) ||
                         ((next_c != CK_NULL) && (next_c >= 32'(ARENA_CHUNKS))));

  // Following a stale or malformed chunk is how one bad record becomes a walk
  // through arbitrary memory. `follow` is the single signal that says the
  // pointer may be taken, and it is false for both.
  assign ck_follow_o = ck_valid_i && !ck_stale_o && !ck_illegal_o &&
                       (next_c != CK_NULL);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      pv_illegal_count_o <= '0;
      td_illegal_count_o <= '0;
      ck_stale_count_o   <= '0;
      ck_illegal_count_o <= '0;
    end else begin
      if (pv_illegal_o) pv_illegal_count_o <= pv_illegal_count_o + 32'd1;
      if (td_illegal_o) td_illegal_count_o <= td_illegal_count_o + 32'd1;
      if (ck_stale_o)   ck_stale_count_o   <= ck_stale_count_o + 32'd1;
      if (ck_illegal_o) ck_illegal_count_o <= ck_illegal_count_o + 32'd1;
    end
  end

endmodule : zhao_geom_parambuf

`default_nettype wire
