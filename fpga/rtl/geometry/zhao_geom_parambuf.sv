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
//   ProjectedVertex, 32 B -- SCHEMA v2, 2026-09-27 (packet PVSCHEMA)
//     screen_x s21, screen_y s21, invw24, status byte,
//     u_over_w s32, v_over_w s32,
//     gouraud_r s32, gouraud_g s32, gouraud_b s32, alpha s22
//
//     THE LAYOUT IS DECLARED ONCE, in `zhao_pkg` as ZHAO_PV_*_LO / ZHAO_PV_*_W,
//     and this decoder and `zhao_geom_paramarena`'s encoder both derive from
//     it. Until v2 they were two hand-maintained inverses whose only guarantee
//     was the encoder's own comment saying "the two must agree bit for bit".
//
//     WHY v2 EXISTS. v1 stored colour as `rgba8 u32` -- eight bits per channel
//     through `unit8_of_fx16`, railed at both ends -- while
//     `zhao_geom_attrpack` builds its six planes from the FULL 32-bit
//     attribute slots those channels arrive in. So the three Gouraud planes
//     owner ruling R234 D1 added were not reconstructible from the record by
//     any back end. That is entry I55's real blocker, it is a RECORD and not
//     an architecture, and v2 removes it by storing the three channels
//     EXACTLY. Full record: reports/DECISION-20260927-PROJECTEDVERTEX-V2.md.
//
//     IT COST NO ADDRESS SPACE: `PV_STRIDE_B` was already 32 while the record
//     was 24, so the eight bytes were allocated and skipped.
//
//     `pv_rgba_o` IS RETAINED AND IS NOW DERIVED HERE rather than stored. The
//     published conversion `zref::unit8_from_fx16` moved to `zhao_pkg` as
//     `zhao_unit8_of_fx16` so that this block and `zhao_geom_vertid` are two
//     INSTANCES of one law rather than two EXPRESSIONS of it -- the
//     distinction `zhao_forge_assemble.sv:62-66` states and CLAUDE.md's
//     sibling-contract rule exists to enforce.
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
// s21 STORED AND s21 LEGAL -- AND THE DETECTOR THAT WENT BLIND, DECLARED
// ---------------------------------------------------------------------------
// R7 said screen coordinates are STORED as s32 with a legal range of s21, and
// the rule that came with it is that a value outside s21 is a MALFORMED
// DESCRIPTOR, not a coordinate to be wrapped or clamped: clamping would place
// a triangle somewhere plausible, refusing says the producer is wrong.
//
// v2 STORES s21. That is not a truncation -- it is the domain the hardware
// ALREADY refuses outside of -- but it has a consequence that must be written
// down rather than left as a reassuring zero:
//
//   `pv_illegal_o`'s OLD s21 TERM CAN NEVER FIRE AGAIN. A field stored in
//   twenty-one bits decodes to a legal s21 by construction. CLAUDE.md: "a
//   detector reading zero is a claim, and it is the claim to check hardest."
//   A term that cannot reach its own fault is worse than absent, because it
//   looks like enforcement.
//
// SO THE REFUSAL MOVED TO THE ENCODER, which is the only place the 32-bit
// value still exists to be judged: `zhao_geom_paramarena.pv_narrow_o` counts a
// vertex whose x or y does not fit s21 and REFUSES the record. The illegal
// state is now unrepresentable in the arena, which is a strengthening.
//
// AND `pv_illegal_o` KEEPS ITS PORT AND ITS MEANING -- "this record is
// malformed" -- now watching the two things that CAN still be malformed on the
// way back, both of which are written laws that had NO DETECTOR AT ALL before
// v2 (design/contracts/GEOM.VERTID.md, "The ProjectedVertex status byte"):
//
//   * `status[7:4]` is "reserved, written 0. Nonzero is a malformed record."
//   * `status[1:0]` domain 3 is "reserved (illegal)".
//
// Both are reachable with legal stimulus and both are exercised by
// `geom_parambuf_directed`. This is strictly more enforcement than v1 had, not
// a port kept alive for appearances.
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

module zhao_geom_parambuf
  import zhao_pkg::*;
#(
    parameter int unsigned CHUNK_IDS = 14,
    // The arena, in chunk units. A `next_chunk` outside it is malformed.
    parameter int unsigned ARENA_CHUNKS = 65536
) (
    input var logic clk,
    input var logic rst_n,

    // ---- ProjectedVertex: SCHEMA v2, 32 bytes in, fields out ---------------
    // The width is the package's, not a literal, so the port cannot disagree
    // with the record the allocator strides by.
    input  var logic          pv_valid_i,
    input  var logic [ZHAO_PARAMBUF_PV_BYTES*8-1:0] pv_bytes_i,
    output var logic signed [31:0] pv_x_o,       // sign-extended from s21
    output var logic signed [31:0] pv_y_o,       // sign-extended from s21
    output var logic [23:0]   pv_invw_o,
    output var logic [7:0]    pv_status_o,
    output var logic signed [31:0] pv_uow_o,
    output var logic signed [31:0] pv_vow_o,
    // THE THREE GOURAUD CHANNELS AT FULL PRECISION -- the whole reason v2
    // exists. These are the 32-bit attribute slots `zhao_geom_attrpack` reads
    // to build R234 D1's three planes, stored EXACTLY, so a back end fed from
    // this arena produces planes that are bit-identical to the live path's
    // rather than merely close.
    output var logic signed [31:0] pv_r_o,
    output var logic signed [31:0] pv_g_o,
    output var logic signed [31:0] pv_b_o,
    // ALPHA IS s22 AND IT IS THE ONLY FIELD NARROWED BELOW ITS SLOT, because
    // it is the only one with no plane consumer -- `zhao_geom_attrpack` waives
    // slot 6 by name. Its full declared domain across all four attribute
    // producers is 0x1_0000, seventeen bits; s22 is that plus a sign bit plus
    // five bits of overbright headroom. See `zhao_pkg`.
    output var logic signed [31:0] pv_alpha_o,   // sign-extended from s22
    // R7's 8-bit view, DERIVED rather than stored. `zhao_unit8_of_fx16` is the
    // published law and lives in `zhao_pkg`; byte order is { a, b, g, r } with
    // r in the low byte, as GEOM.VERTID.md declares.
    output var logic [31:0]   pv_rgba_o,
    output var logic          pv_illegal_o,   // a MALFORMED status byte

    // ---- TriangleDescriptor: 16 bytes ---------------------------------------
    input  var logic          td_valid_i,
    input  var logic [ZHAO_PARAMBUF_TD_BYTES*8-1:0] td_bytes_i,
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
    // ---- SCHEMA v2's SECOND SIXTEEN BYTES (SWAPCLOSE, 2026-09-27) ---------
    // `zhao_geom_setup`'s `tri_area2_i` and the four bounds of the section 8
    // SCISSORED scan box, at that block's own widths. These are the fields
    // that make a back end fed from this arena possible at all: `kc2` is
    // DEFINED from `area2` in GEOM.SETUP, so the barycentric identity cannot
    // recover it and a circuit built on that identity would be correct for any
    // garbage value. `zhao_pkg`'s TriangleDescriptor table has the argument.
    output var logic signed [47:0] td_area2_o,
    output var logic signed [11:0] td_min_x_o,
    output var logic signed [11:0] td_max_x_o,
    output var logic signed [11:0] td_min_y_o,
    output var logic signed [11:0] td_max_y_o,
    // A MALFORMED DESCRIPTOR. Two terms since v2, and the port comment says
    // both because "a vertex id past the sealed count" alone would send the
    // next reader looking for an id when the reserve was what moved.
    output var logic          td_illegal_o,

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

  // ---- ProjectedVertex, SCHEMA v2 -----------------------------------------
  // EVERY OFFSET AND WIDTH BELOW IS `zhao_pkg`'s. Not one literal, because the
  // encoder in `zhao_geom_paramarena` reads the same constants and the pair
  // used to be two hand-maintained inverses.
  wire signed [ZHAO_PV_X_W-1:0] x_raw_c =
      $signed(pv_bytes_i[ZHAO_PV_X_LO +: ZHAO_PV_X_W]);
  wire signed [ZHAO_PV_Y_W-1:0] y_raw_c =
      $signed(pv_bytes_i[ZHAO_PV_Y_LO +: ZHAO_PV_Y_W]);
  wire signed [ZHAO_PV_A_W-1:0] a_raw_c =
      $signed(pv_bytes_i[ZHAO_PV_A_LO +: ZHAO_PV_A_W]);

  // Signed-to-wider-signed assignment sign-extends, which is what makes s21
  // storage lossless for every value the encoder is willing to accept.
  // The sized cast is what performs the sign extension -- for a SIGNED
  // operand `32'(v)` replicates the sign bit, where a bare assignment leaves
  // the tool to widen it and warn. Written out so the widening is visible.
  // (The word that cannot start this line is the linter's own name: a comment
  // whose first token after the slashes is that word is parsed as a pragma.)
  assign pv_x_o      = 32'(x_raw_c);
  assign pv_y_o      = 32'(y_raw_c);
  assign pv_alpha_o  = 32'(a_raw_c);

  assign pv_invw_o   = pv_bytes_i[ZHAO_PV_INVW_LO   +: ZHAO_PV_INVW_W];
  assign pv_status_o = pv_bytes_i[ZHAO_PV_STATUS_LO +: ZHAO_PV_STATUS_W];
  assign pv_uow_o    = $signed(pv_bytes_i[ZHAO_PV_UOW_LO +: ZHAO_PV_UOW_W]);
  assign pv_vow_o    = $signed(pv_bytes_i[ZHAO_PV_VOW_LO +: ZHAO_PV_VOW_W]);
  assign pv_r_o      = $signed(pv_bytes_i[ZHAO_PV_R_LO   +: ZHAO_PV_R_W]);
  assign pv_g_o      = $signed(pv_bytes_i[ZHAO_PV_G_LO   +: ZHAO_PV_G_W]);
  assign pv_b_o      = $signed(pv_bytes_i[ZHAO_PV_B_LO   +: ZHAO_PV_B_W]);

  // R7's rgba8, DERIVED from the stored channels by the published law rather
  // than stored lossily beside them. { a, b, g, r }, r in the low byte.
  assign pv_rgba_o   = {zhao_unit8_of_fx16(pv_alpha_o), zhao_unit8_of_fx16(pv_b_o),
                        zhao_unit8_of_fx16(pv_g_o),     zhao_unit8_of_fx16(pv_r_o)};

  // ---- what MALFORMED means in v2 -----------------------------------------
  // See the header. The s21 term is gone because it cannot fire against an
  // s21-stored field; these two CAN fire and had no detector before v2.
  // design/contracts/GEOM.VERTID.md, "The ProjectedVertex status byte":
  //   [7:4] "reserved, written 0. Nonzero is a malformed record."
  //   [1:0] domain 3 is "reserved (illegal)".
  wire status_reserved_bad_c =
      (pv_status_o[ZHAO_PV_STATUS_W-1:4] != 4'd0);
  wire status_domain_bad_c   = (pv_status_o[1:0] == 2'b11);

  assign pv_illegal_o = pv_valid_i && (status_reserved_bad_c || status_domain_bad_c);

  // ---- TriangleDescriptor -- SCHEMA v2 ------------------------------------
  // EVERY SLICE IS NAMED. The v1 form read `td_bytes_i[64 +: 32]` and friends
  // out of bare literals while the encoder packed a positional concatenation,
  // and the two agreed only because two people kept them agreeing -- the exact
  // "two hand-maintained inverses" the ProjectedVertex was rescued from one
  // packet earlier. `zhao_geom_paramarena` now asserts at elaboration that
  // this table FILLS the record, so a field added without moving
  // `ZHAO_TD_END_BIT` is a $fatal and not a silently short write.
  logic [15:0] v0_c, v1_c, v2_c;
  assign v0_c = td_bytes_i[ZHAO_TD_V0_LO +: ZHAO_TD_V0_W];
  assign v1_c = td_bytes_i[ZHAO_TD_V1_LO +: ZHAO_TD_V1_W];
  assign v2_c = td_bytes_i[ZHAO_TD_V2_LO +: ZHAO_TD_V2_W];

  assign td_v0_o       = v0_c;
  assign td_v1_o       = v1_c;
  assign td_v2_o       = v2_c;
  assign td_material_o = td_bytes_i[ZHAO_TD_MATERIAL_LO +: ZHAO_TD_MATERIAL_W];
  assign td_raster_o   = td_bytes_i[ZHAO_TD_RASTER_LO   +: ZHAO_TD_RASTER_W];
  assign td_source_o   = td_bytes_i[ZHAO_TD_SOURCE_LO   +: ZHAO_TD_SOURCE_W];

  // v2's five. Each is `$signed` at its STORED width and then carried at that
  // same width, because GEOM.SETUP's ports are s48 and s12 -- there is no
  // sign extension to get wrong, and no wider intermediate for a reader to
  // wonder about.
  assign td_area2_o = $signed(td_bytes_i[ZHAO_TD_AREA2_LO +: ZHAO_TD_AREA2_W]);
  assign td_min_x_o = $signed(td_bytes_i[ZHAO_TD_MINX_LO  +: ZHAO_TD_MINX_W]);
  assign td_max_x_o = $signed(td_bytes_i[ZHAO_TD_MAXX_LO  +: ZHAO_TD_MAXX_W]);
  assign td_min_y_o = $signed(td_bytes_i[ZHAO_TD_MINY_LO  +: ZHAO_TD_MINY_W]);
  assign td_max_y_o = $signed(td_bytes_i[ZHAO_TD_MAXY_LO  +: ZHAO_TD_MAXY_W]);

  // A vertex id past the frame's sealed vertex count indexes memory that
  // belongs to no vertex. Refused rather than clamped: clamping would draw a
  // triangle using somebody else's position.
  // The ids are u16 and the seal is u18, so the comparison is written at the
  // WIDER width explicitly. Left implicit it is still correct here, but the
  // next person reading a mixed-width compare has to prove that to themselves.
  wire td_id_bad_c = (18'(v0_c) >= td_sealed_vertices_i) ||
                     (18'(v1_c) >= td_sealed_vertices_i) ||
                     (18'(v2_c) >= td_sealed_vertices_i);

  // SCHEMA v2's RESERVED FIELD HAS A DETECTOR RATHER THAN A PROMISE.
  // `zhao_pkg` rules the top 32 bits "WRITTEN 0. Nonzero is a malformed
  // record", and the arena writes them from a `'0` initialisation, so on the
  // live path this term is structurally silent. It is here because a reserve
  // nobody checks is a reserve that gets quietly spent: the status byte's own
  // `[7:4]` went the whole of v1 with no detector at all, which is recorded in
  // `zhao_geom_parambuf`'s header as the thing v2 fixed. This one is
  // REACHABLE -- the descriptor makes a round trip through a writable SDRAM
  // model, so a bench can poke those four bytes between the write and the
  // walk, and `geom_paramarena_directed` does exactly that.
  wire td_rsvd_bad_c = (td_bytes_i[ZHAO_TD_RSVD_LO +: ZHAO_TD_RSVD_W] != 32'd0);

  assign td_illegal_o = td_valid_i && (td_id_bad_c || td_rsvd_bad_c);

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
