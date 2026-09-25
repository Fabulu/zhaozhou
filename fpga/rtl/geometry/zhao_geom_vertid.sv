// zhao_geom_vertid.sv -- GEOM.VERTID: the console's ONE geometry identity
// space, and GEOM.PARAMBUF's projected-vertex producer.
//
// Law (in citation order):
//   reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt section 4 -- the decision
//       this block implements, quoted where each part of it is implemented.
//   design/contracts/GEOM.VERTID.md -- this block's contract.
//   design/contracts/GEOM.PARAMBUF.md -- the ProjectedVertex and
//       TriangleDescriptor layouts (R7), which this block fills and does not
//       change.
//   reference/include/zref/zref_fixp.hpp -- `unit8_from_fx16`, which owns the
//       lit-colour quantisation this block performs. NOT invented here.
//
// ENFORCED-BY: tests/geometry/geom_vertid_directed.cpp:main
//
// ---------------------------------------------------------------------------
// THE PROBLEM, STATED THE WAY THE CONSOLE ACTUALLY HAS IT
// ---------------------------------------------------------------------------
// `zhao_console_core`'s entry I53 recorded the projected-vertex intake as TIED
// and gave the reason: "GEOM.PROJECT's output reaches this file as
// `proj_out_*`, which is a TRIANGLE -- three screen positions, three depths,
// one source id -- and the arena's intake is a VERTEX. Turning one into the
// other is not plumbing: it needs a vertex-identity scheme, because a
// TriangleDescriptor names vertices by u16 INDEX and two triangles that share
// an edge must name the SAME index, or the arena stores every vertex two or
// three times and the 65,535-vertex seal buys a third of what R7's tier
// promises."
//
// That is this block. It is the only thing in the geometry path that assigns a
// vertex id, and the ids it assigns are the arena's own allocation indices --
// see THE ID IS NOT MINE below, which is the single most important sentence in
// this file.
//
// ---------------------------------------------------------------------------
// THE IDENTITY, COMPONENT BY COMPONENT -- AND WHERE EACH COMPONENT COMES FROM
// ---------------------------------------------------------------------------
// The directive lists what a vertex identity must include: "the source
// geometry/object generation and draw instance, view, relevant pose/warp
// state, source vertex identity and any attribute discontinuity. Equal
// positions alone do not prove identity."
//
// This console already HAS that identity and has had it since GEOM.WCACHE was
// built. It is the key GEOM.REPLAY presents on every corner lookup:
//
//     { arena, generation, index }
//
//   * `arena` is a GEOM.GROUP_SEQ arena handle, opened per MESHLET per VISIBLE
//     VIEW. So it carries the VIEW and the DRAW INSTANCE: two instances of the
//     same mesh are two dispatches, two opens, two handles.
//   * `generation` is `zhao_vertex_arena`'s per-arena generation, bumped at
//     every open. It is what separates one use of arena slot 2 from the next.
//     Together with `arena` it names exactly one (meshlet instance, view), so
//     it carries the OBJECT GENERATION and the POSE/WARP STATE: a re-posed
//     instance is a different dispatch and therefore a different handle.
//   * `index` is the vertex's ordinal among the batch's DECODED vertices --
//     `zhao_geom_vattr`'s header proves it is exactly that -- so it is the
//     SOURCE VERTEX IDENTITY. An ATTRIBUTE DISCONTINUITY (a UV seam, a hard
//     normal) is already a distinct source vertex in a mesh record and
//     therefore a distinct index. The console never merges two source vertices
//     that differ in an attribute, because it never merges anything by value.
//
// POSITIONS ARE NEVER COMPARED. This block reads `tri_a*_i` and the attribute
// packets to PUBLISH them; it never tests them for equality and never hashes
// them. The directive's "equal positions alone do not prove identity" is
// satisfied structurally: there is no position comparator in this file.
//
// AND NEITHER A HASH NOR A CRC IS USED. The map below is a DIRECT-MAPPED EXACT
// TABLE. The row address is the whole of the {arena, index} part of the key --
// not a digest of it -- and the remaining part of the key is STORED in the row
// and compared for exact equality. Two different identities cannot share a row
// and be called equal, because the only way to read a row as a hit is for the
// stored discriminator to be bit-for-bit the one being offered. There is no
// collision domain to argue about; there is no digest anywhere in this block.
//
// ---------------------------------------------------------------------------
// THE ID IS NOT MINE. IT IS THE ARENA'S, AND IT ARRIVES WITH ITS ACCEPTANCE.
// ---------------------------------------------------------------------------
// The obvious implementation keeps a dense counter here and hands the arena a
// vertex it is confident will land at that index. That is CLAUDE.md's
// "detector wired to two operands that move together" in its productive form:
// two counters, two enables, and a divergence on the first record the arena
// discards -- with `verts_written_o` and this block's count BOTH still
// balancing, because neither looks at the other.
//
// So `zhao_geom_paramarena` gained `pv_accept_o` / `pv_id_o`, driven from the
// same expressions its allocation arm tests, and THIS block takes the id from
// there. There is exactly one vertex counter in the console and it is the
// allocator's. If the arena discards a record -- no sealed frame, a faulted
// frame, a quota overrun -- `pv_accept_i` is LOW, no id is recorded, and
// `vid_sunk_o` counts it. Nothing here can drift, because nothing here counts.
//
// ---------------------------------------------------------------------------
// THE MAP: WHAT IT HOLDS, HOW LONG, AND THE EVICTION PROOF
// ---------------------------------------------------------------------------
// One row per {arena, index}, ARENAS * VSLOTS of them. Each row holds
//
//     { epoch, id16 }      and a VALID bit in a flop bitmap beside it
//
// LIFETIME: exactly one frame. The valid bitmap is cleared on `frame_seal_i`,
// which is the arena's OWN `seal_fire_o` -- the clock the allocator resets its
// cursor -- and not the frame edge that ASKED for a seal. A seal here is a
// request that may be held pending for the whole drain, so clearing on the
// request would leave this block describing frame N+1 while the allocator is
// still filling frame N: a stale id naming a record index that now belongs to
// somebody else. One event, one enable, both sides.
//
// EPOCH, AND WHY NOT THE RAW GENERATION. `generation` is 8 bits, so an arena
// slot reused 256 times in a frame presents the same generation twice, and a
// row left over from the earlier use would read as a HIT and hand out the
// earlier vertex's id. Storing the generation is therefore not exact over a
// frame, however exact the comparison is. Instead this block keeps a per-arena
// FRAME-LOCAL EPOCH: when the generation offered for an arena differs from the
// last one seen for that arena, the epoch advances, and the epoch is what the
// row stores. The epoch does not wrap within a frame -- see the bound below --
// so two different uses of a slot can never collide.
//
// THE BOUND: every epoch advance is followed by a corner that must MISS (the
// epoch is new, so no row can carry it) and is therefore published. Publishes
// are bounded by the arena's sealed vertex quota, at most 65,536. EPOCH_W is
// 18, so the epoch tops out at 262,143, four times the reachable maximum. A
// wrap counter is DECLINED for that reason and the reason is arithmetic rather
// than confidence.
//
// THE EVICTION PROOF the directive asks for -- "prove that eviction/reuse
// cannot change a still-referenced identity":
//
//   1. A row is overwritten only by a corner with the SAME {arena, index} and
//      a DIFFERENT epoch, i.e. a different identity. Same identity, same row,
//      same epoch: a hit, never a write.
//   2. A different epoch on arena `a` means a different generation on arena
//      `a`, which means GEOM.GROUP_SEQ re-opened that arena. It cannot do that
//      until GEOM.REPLAY released the handle (`rel_valid_o`), and GEOM.REPLAY
//      does not release a handle until it has emitted every triangle that
//      references it. GEOM.CLIP preserves order and creates no vertices (see
//      below), so at this block's input every triangle of the earlier use
//      precedes every triangle of the later one.
//   3. Therefore a still-referenced identity is never the one evicted. And if
//      premise 2 were ever broken by a future producer, the failure mode is a
//      MISS -- a second, duplicate record with its own id -- and never a wrong
//      hit, because a hit requires the exact stored epoch. The safe direction
//      is the structural one, not a lucky one.
//
// ---------------------------------------------------------------------------
// WHY POST-CLIP, AND THE PREMISE THAT DIED TO PUT IT THERE
// ---------------------------------------------------------------------------
// The directive: the arena is "authoritative for the FINAL geometry actually
// referenced by the binner and raster consumer, including clipping-derived
// vertices", and "Carry clipping lineage explicitly. New intersections are
// identified from their canonical source edge/lineage and clipping operation".
//
// THIS CONSOLE HAS NO CLIPPING-DERIVED VERTICES, and `zhao_geom_clip`'s own
// header says so in terms: "THE NEAR PLANE IS A WHOLE-PRIMITIVE REJECTION, NOT
// A CLIP ... GEOM.CLIP never produces more than one triangle for one triangle
// in." It drops primitives and it normalises winding; it never interpolates a
// new vertex, and the block has no divider and no vertex queue to do it with.
// The clipping-lineage clause therefore describes a machine this console is
// not. There is no lineage field in the key because there is nothing for it to
// name -- and that is a MEASUREMENT of the tree, recorded here so the next
// person does not add a field for a case that cannot arise. If a
// Sutherland-Hodgman clipper is ever built, the lineage becomes a fourth key
// component and this is the file it is added to.
//
// What GEOM.CLIP DOES do is DROP, so publishing post-clip publishes only
// vertices the binner will actually reference. That is the directive's "FINAL
// geometry", and it is why this block sits on `zhao_geom_clip`'s output rather
// than on GEOM.REPLAY's. It also means the winding flip has already happened:
// when GEOM.CLIP swaps B and C it swaps their identities with their attributes
// (the same swap, in the same place, for the same reason), so the descriptor
// this block emits names the corners in the order the raster will walk them.
//
// ---------------------------------------------------------------------------
// PRODUCERS THAT HAVE NO IDENTITY TO SHARE
// ---------------------------------------------------------------------------
// `zhao_geom_clipdoor` arbitrates three producers into GEOM.CLIP: the mesh arm
// (GEOM.REPLAY), the forge arm and the particle arm. Only the mesh arm has an
// arena key; a forge primitive and a polygon particle are built corner by
// corner and no two of them are the same vertex by any definition this console
// holds.
//
// So `tri_domain_i` names the producer and `SHARED_DOMAINS` is the knob that
// says which domains carry a dedupable identity. A corner from an unshared
// domain is published as its OWN vertex, every time, and counted at
// `vid_unshared_o`. It is NOT looked up and found missing -- it is declared
// unshareable, because "we failed to find it" and "it cannot be found" are
// different facts and only one of them is true here.
//
// ---------------------------------------------------------------------------
// THE STATUS BYTE, WHICH HAD NO DEFINITION ANYWHERE
// ---------------------------------------------------------------------------
// R7's ProjectedVertex carries "invw24 + status byte, one word" and nothing in
// this repository ever said what the status byte holds: `zhao_geom_parambuf`
// decodes `pv_status_o` out of the bytes and no producer ever wrote one. The
// directive gives this packet the authority to amend record schemas, and this
// is the amendment -- written here, in the contract, and in the reference:
//
//     [1:0]  domain           0 MESH, 1 FORGE, 2 PARTICLE, 3 reserved
//     [2]    untextured       the primitive that first published this vertex
//                             declared no texture coordinates (R197), so
//                             u_over_w and v_over_w are DON'T-CARE and a
//                             consumer must not read them
//     [3]    shared_capable   published under a dedupable identity. 0 means
//                             this record is one corner's own vertex: either
//                             an unshared domain, or a mesh corner whose key
//                             was outside the map (see `vid_index_oob_o`)
//     [7:4]  reserved, written 0. Nonzero is a malformed record.
//
// It is FIELDS AND NOT A CONVENIENT ZERO. The one thing the byte deliberately
// does NOT carry is the near-plane `behind` bit: post-clip it is zero for every
// accepted triangle by construction, and a field that can never be set is a
// lie that reads like evidence.
//
// AN UNTEXTURED-CONFLICT COUNTER IS DECLINED, AT DESIGN TIME. Two primitives
// sharing a vertex could in principle disagree about the untextured
// declaration. They cannot here: the declaration is per PRODUCER ARM
// (`GEOM_REPLAY_UNTEX_DECL` for the only arm with shareable identities), and
// identities never span arms because the domain is part of the publication
// decision. A counter whose two operands are the same constant is the thing
// this repository has been caught building four times; it is not built again.
//
// ---------------------------------------------------------------------------
// THE COLOUR QUANTISATION IS CITED, NOT CHOSEN
// ---------------------------------------------------------------------------
// The lit channels arrive as fx16 (1.0 = 0x1_0000) in 32-bit slots and R7's
// record holds `rgba8 u32`. `zref::unit8_from_fx16` is the published law for
// that conversion -- negative to 0, above 0xFFFF to 255, otherwise
// (v + 128) >> 8 with a 255 rail -- including the Review C2 clamp that stops a
// ~1.0 weight wrapping to 0. This block implements exactly that function and
// the directed test differences every channel against it.
//
// THE BYTE ORDER IS DECLARED: rgba8 = { a, b, g, r } with r in the LOW byte,
// LSB-first like every other packing in this subsystem.
`default_nettype none

module zhao_geom_vertid #(
    // GEOM.GROUP_SEQ's arena geometry, as the console holds it.
    parameter int unsigned ARENAS   = 4,
    parameter int unsigned ARENA_W  = 3,
    parameter int unsigned GEN_W    = 8,
    parameter int unsigned INDEX_W  = 12,
    // Rows per arena in the identity map. GEOM.ASSETFETCH's MAX_VERTICES is
    // the most vertices one batch can hold and `zhao_geom_vattr` sizes its own
    // store by the same number, so the map covers exactly the keys that can
    // exist. A key outside it is published unshared and counted, never
    // truncated into somebody else's row.
    parameter int unsigned VSLOTS   = 64,
    // See THE BOUND in the header. 18 bits against a reachable maximum of
    // 65,536 advances.
    parameter int unsigned EPOCH_W  = 18,
    // GEOM.CLIP's ruling-5 attribute packet: slot 0 invw24, then u_over_w,
    // v_over_w, r, g, b, alpha.
    parameter int unsigned ATTRS    = 7,
    // WHICH DOMAINS CARRY A DEDUPABLE IDENTITY, as a bitmask over
    // `tri_domain_i`. Bit 0 is the mesh arm. A knob and not a hard-coded
    // comparison, because the day the forge arm grows a vertex store this is
    // the one line that changes.
    parameter logic [3:0]  SHARED_DOMAINS = 4'b0001,
    // The widest id a u16 `vertex_id` can name, as a COUNT. Records past it
    // cannot be named by a descriptor and are counted rather than truncated.
    parameter int unsigned ID_LIMIT = 65536
) (
    input  var logic clk,
    input  var logic rst_n,

    // THE ARENA'S OWN SEAL, not the frame edge that asked for it. See the
    // header's LIFETIME paragraph.
    input  var logic frame_seal_i,

    // ---- the post-clip triangle --------------------------------------------
    input  var logic                    tri_valid_i,
    output var logic                    tri_ready_o,
    input  var logic [1:0]              tri_domain_i,
    input  var logic [ARENA_W+GEN_W+INDEX_W-1:0] tri_key_a_i,
    input  var logic [ARENA_W+GEN_W+INDEX_W-1:0] tri_key_b_i,
    input  var logic [ARENA_W+GEN_W+INDEX_W-1:0] tri_key_c_i,
    input  var logic signed [20:0]      tri_ax_i,
    input  var logic signed [20:0]      tri_ay_i,
    input  var logic signed [20:0]      tri_bx_i,
    input  var logic signed [20:0]      tri_by_i,
    input  var logic signed [20:0]      tri_cx_i,
    input  var logic signed [20:0]      tri_cy_i,
    input  var logic [ATTRS*32-1:0]     tri_attr_a_i,
    input  var logic [ATTRS*32-1:0]     tri_attr_b_i,
    input  var logic [ATTRS*32-1:0]     tri_attr_c_i,
    input  var logic                    tri_untex_i,
    input  var logic [15:0]             tri_material_i,
    input  var logic [31:0]             tri_raster_i,
    input  var logic [15:0]             tri_src_id_i,

    // ---- GEOM.PARAMARENA's ProjectedVertex intake --------------------------
    output var logic                    pv_valid_o,
    input  var logic                    pv_ready_i,
    output var logic signed [31:0]      pv_x_o,
    output var logic signed [31:0]      pv_y_o,
    output var logic [23:0]             pv_invw_o,
    output var logic [7:0]              pv_status_o,
    output var logic signed [31:0]      pv_uow_o,
    output var logic signed [31:0]      pv_vow_o,
    output var logic [31:0]             pv_rgba_o,
    // The allocator's verdict and the index it gave. See THE ID IS NOT MINE.
    input  var logic                    pv_accept_i,
    input  var logic [17:0]             pv_id_i,

    // ---- GEOM.PARAMARENA's TriangleDescriptor intake -----------------------
    output var logic                    td_valid_o,
    input  var logic                    td_ready_i,
    output var logic [15:0]             td_v0_o,
    output var logic [15:0]             td_v1_o,
    output var logic [15:0]             td_v2_o,
    output var logic [15:0]             td_material_o,
    output var logic [31:0]             td_raster_o,
    output var logic [31:0]             td_source_o,
    // The triangle's own arena index, for I54's chunk serialisation. Valid
    // with `td_accept_i` and meaningless otherwise, exactly like `pv_id_i`.
    input  var logic                    td_accept_i,
    input  var logic [17:0]             td_id_i,
    output var logic                    tri_id_valid_o,
    output var logic [17:0]             tri_id_o,

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0]             vid_tris_o,
    output var logic [31:0]             vid_refs_o,
    output var logic [31:0]             vid_published_o,
    // THE NUMBER THIS BLOCK EXISTS FOR: a corner answered from the map, i.e. a
    // vertex published once and referenced again.
    output var logic [31:0]             vid_reused_o,
    output var logic [31:0]             vid_unshared_o,
    output var logic [31:0]             vid_opens_o,
    output var logic [31:0]             vid_sunk_o,
    output var logic [31:0]             vid_index_oob_o,
    output var logic [31:0]             vid_key_split_o,
    output var logic [31:0]             vid_id_unnameable_o,
    output var logic [31:0]             vid_seal_abort_o,
    output var logic [31:0]             vid_stall_o,
    output var logic                    busy_o
);

  localparam int unsigned KEYW = ARENA_W + GEN_W + INDEX_W;
  localparam int unsigned ROWS = ARENAS * VSLOTS;
  localparam int unsigned ROWA = $clog2(ROWS);
  localparam int unsigned SLOTA = $clog2(VSLOTS);
  localparam int unsigned ARNA = $clog2(ARENAS);
  localparam int unsigned MAPW = EPOCH_W + 16;

  // Quartus 17.0 will not take a bare module-scope elaboration check; it needs
  // `initial begin ... end` (CLAUDE.md, 2026-09-08). And `--lint-only` does not
  // run initial blocks, so a clean lint says nothing about these.
  // synthesis translate_off
  initial begin
    if (ARENAS != (1 << ARNA))
      $fatal(1, "zhao_geom_vertid: ARENAS must be a power of two");
    if (VSLOTS != (1 << SLOTA))
      $fatal(1, "zhao_geom_vertid: VSLOTS must be a power of two");
    if (ARENA_W < ARNA + 1)
      $fatal(1, "zhao_geom_vertid: ARENA_W must be one bit wider than the arena address, so an out-of-range arena can be PRESENTED and refused");
    if (INDEX_W < SLOTA)
      $fatal(1, "zhao_geom_vertid: INDEX_W cannot address VSLOTS");
    if (ATTRS < 7)
      $fatal(1, "zhao_geom_vertid: the ruling-5 attribute packet is seven slots");
    if (EPOCH_W < 17)
      $fatal(1, "zhao_geom_vertid: EPOCH_W must exceed the 65,536 reachable advances");
    if (ID_LIMIT > 65536)
      $fatal(1, "zhao_geom_vertid: a u16 vertex_id names at most 65,536 records");
  end
  // synthesis translate_on

  // ------------------------------------------------------------ key fields --
  // Each selector reads ITS OWN field and leaves the rest -- which is what a
  // field selector is. Waived by name rather than by a file-wide lint-off.
  /* verilator lint_off UNUSEDSIGNAL */
  function automatic logic [ARENA_W-1:0] key_arena(input logic [KEYW-1:0] k);
    key_arena = k[KEYW-1 -: ARENA_W];
  endfunction
  function automatic logic [GEN_W-1:0] key_gen(input logic [KEYW-1:0] k);
    key_gen = k[INDEX_W +: GEN_W];
  endfunction
  function automatic logic [INDEX_W-1:0] key_index(input logic [KEYW-1:0] k);
    key_index = k[0 +: INDEX_W];
  endfunction
  /* verilator lint_on UNUSEDSIGNAL */

  // ------------------------------------------- zref::unit8_from_fx16 in RTL --
  // The published law, not a local rounding choice. `v` is the 32-bit slot;
  // the lit channels occupy its low 17 bits and alpha its low 17 too, so the
  // sign test below is the slot's own and never a reinterpretation.
  function automatic logic [7:0] unit8_of_fx16(input logic [31:0] v);
    logic [31:0] q;
    begin
      if (v[31]) begin
        unit8_of_fx16 = 8'd0;
      end else if (v[30:16] != 15'd0) begin
        unit8_of_fx16 = 8'd255;
      end else begin
        q = (v + 32'd128) >> 8;
        unit8_of_fx16 = (q > 32'd255) ? 8'd255 : q[7:0];
      end
    end
  endfunction

  // -------------------------------------------------------- the held record --
  typedef enum logic [2:0] { S_IDLE, S_LOOK, S_DEC, S_PUB, S_TD } st_e;
  st_e st_q;

  logic [1:0]          dom_q;
  logic                untex_q;
  logic [15:0]         mat_q;
  logic [31:0]         rast_q;
  logic [15:0]         src_q;
  logic [KEYW-1:0]     key_q  [0:2];
  logic signed [20:0]  cx_q   [0:2];
  logic signed [20:0]  cy_q   [0:2];
  logic [ATTRS*32-1:0] att_q  [0:2];
  logic [15:0]         id_q   [0:2];
  logic [1:0]          k_q;              // which corner is in hand
  logic                poison_q;         // a corner was consumed and not allocated

  // -------------------------------------------------------------- the map ----
  // A DIRECT-MAPPED EXACT TABLE. The row address is the whole {arena, index}
  // part of the key -- no digest -- and the epoch stored in the row is
  // compared bit for bit. There is no collision domain.
  //
  // RAM TEMPLATE: single write port, single synchronous read port, and the
  // array is never reset. The VALID bits live in flops beside it precisely so
  // the array does not need a clear -- `zhao_vertex_arena`'s header records
  // what a clear over an array does to inference.
  logic [MAPW-1:0] map_ram [0:ROWS-1];
  logic [MAPW-1:0] map_rd_q;
  logic [ROWS-1:0] map_valid_q;
  logic            map_hitv_q;

  // Per-arena frame-local epoch state.
  logic [EPOCH_W-1:0] epoch_q   [0:ARENAS-1];
  logic [GEN_W-1:0]   lastgen_q [0:ARENAS-1];
  logic [ARENAS-1:0]  seen_q;

  // The epoch the corner in hand was looked up under, registered beside the
  // row it was used to address. The two travel together on ONE enable, which
  // is the whole point: a comparison whose two sides load on different enables
  // is blind to exactly the fault a stall produces.
  logic [EPOCH_W-1:0] eff_epoch_q;
  logic               shareable_q;
  logic [ROWA-1:0]    row_q;

  // --------------------------------------------------- the corner in flight --
  wire [KEYW-1:0]      cur_key_c   = key_q[k_q];
  wire [ARENA_W-1:0]   cur_arena_c = key_arena(cur_key_c);
  wire [GEN_W-1:0]     cur_gen_c   = key_gen(cur_key_c);
  wire [INDEX_W-1:0]   cur_index_c = key_index(cur_key_c);

  wire arena_ok_c = (32'(cur_arena_c) < ARENAS);
  wire index_ok_c = (32'(cur_index_c) < VSLOTS);
  wire key_ok_c   = arena_ok_c && index_ok_c;
  wire dom_shared_c = SHARED_DOMAINS[dom_q];
  wire shareable_c  = dom_shared_c && key_ok_c;

  wire [ARNA-1:0]  cur_an_c  = cur_arena_c[ARNA-1:0];
  wire [SLOTA-1:0] cur_sl_c  = cur_index_c[SLOTA-1:0];
  wire [ROWA-1:0]  cur_row_c = {cur_an_c, cur_sl_c};

  wire open_c = shareable_c && (!seen_q[cur_an_c] || (lastgen_q[cur_an_c] != cur_gen_c));
  wire [EPOCH_W-1:0] eff_epoch_c = open_c ? (epoch_q[cur_an_c] + {{(EPOCH_W-1){1'b0}}, 1'b1})
                                          : epoch_q[cur_an_c];

  wire hit_c = shareable_q && map_hitv_q && (map_rd_q[16 +: EPOCH_W] == eff_epoch_q);

  // ------------------------------------------------- the published record ----
  // Slot 0's top eight bits are the console's own zero pad -- the packet is
  // built as `{store_word, 8'd0, invw24}` -- so this block reads 24 of them
  // and the pad is unused ON PURPOSE. Waived by name.
  /* verilator lint_off UNUSEDSIGNAL */
  wire [ATTRS*32-1:0] a_c = att_q[k_q];
  /* verilator lint_on UNUSEDSIGNAL */
  wire [23:0] invw_c = a_c[0*32 +: 24];
  wire [31:0] uow_c  = a_c[1*32 +: 32];
  wire [31:0] vow_c  = a_c[2*32 +: 32];
  wire [31:0] r_c    = a_c[3*32 +: 32];
  wire [31:0] g_c    = a_c[4*32 +: 32];
  wire [31:0] b_c    = a_c[5*32 +: 32];
  wire [31:0] al_c   = a_c[6*32 +: 32];

  assign pv_valid_o  = (st_q == S_PUB) && !frame_seal_i;
  assign pv_x_o      = 32'(cx_q[k_q]);
  assign pv_y_o      = 32'(cy_q[k_q]);
  assign pv_invw_o   = invw_c;
  assign pv_status_o = {4'd0, shareable_q, untex_q, dom_q};
  assign pv_uow_o    = uow_c;
  assign pv_vow_o    = vow_c;
  assign pv_rgba_o   = {unit8_of_fx16(al_c), unit8_of_fx16(b_c),
                        unit8_of_fx16(g_c),  unit8_of_fx16(r_c)};

  assign td_valid_o    = (st_q == S_TD) && !frame_seal_i;
  assign td_v0_o       = id_q[0];
  assign td_v1_o       = id_q[1];
  assign td_v2_o       = id_q[2];
  assign td_material_o = mat_q;
  assign td_raster_o   = rast_q;
  assign td_source_o   = {16'd0, src_q};

  assign tri_id_valid_o = td_valid_o && td_ready_i && td_accept_i;
  assign tri_id_o       = td_id_i;

  // `tri_ready_o` is a STATE decode and the seal, never a function of
  // `tri_valid_i`, so the console's fork at GEOM.CLIP's output cannot close a
  // combinational loop through this block.
  assign tri_ready_o = (st_q == S_IDLE) && !frame_seal_i;
  assign busy_o      = (st_q != S_IDLE);

  wire pv_fire_c = pv_valid_o && pv_ready_i;
  wire td_fire_c = td_valid_o && td_ready_i;

  // An id the arena allocated that a u16 `vertex_id` cannot name. Unreachable
  // while the seal is within ID_LIMIT, and reachable at THIS BLOCK'S PORTS by
  // a bench that offers one -- so it is stimulus-fireable and owes no mutant.
  wire id_unnameable_c = pv_fire_c && pv_accept_i && (32'(pv_id_i) >= ID_LIMIT);

  // ---- the identity map's one write port and one read port -----------------
  wire             map_we_c    = pv_fire_c && pv_accept_i && shareable_q && !id_unnameable_c;
  wire [ROWA-1:0]  map_wa_c    = row_q;
  wire [MAPW-1:0]  map_wd_c    = {eff_epoch_q, pv_id_i[15:0]};
  wire [ROWA-1:0]  map_ra_c    = cur_row_c;

  always_ff @(posedge clk) begin
    if (map_we_c) map_ram[map_wa_c] <= map_wd_c;
    map_rd_q <= map_ram[map_ra_c];
  end

  // --------------------------------------------------------------- engine ----
  integer i;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_q        <= S_IDLE;
      k_q         <= 2'd0;
      poison_q    <= 1'b0;
      dom_q       <= 2'd0;
      untex_q     <= 1'b0;
      mat_q       <= 16'd0;
      rast_q      <= 32'd0;
      src_q       <= 16'd0;
      map_valid_q <= '0;
      map_hitv_q  <= 1'b0;
      eff_epoch_q <= '0;
      shareable_q <= 1'b0;
      row_q       <= '0;
      seen_q      <= '0;
      for (i = 0; i < ARENAS; i = i + 1) begin
        epoch_q[i]   <= '0;
        lastgen_q[i] <= '0;
      end
      for (i = 0; i < 3; i = i + 1) begin
        key_q[i] <= '0;
        cx_q[i]  <= 21'sd0;
        cy_q[i]  <= 21'sd0;
        att_q[i] <= '0;
        id_q[i]  <= 16'd0;
      end
      vid_tris_o          <= 32'd0;
      vid_refs_o          <= 32'd0;
      vid_published_o     <= 32'd0;
      vid_reused_o        <= 32'd0;
      vid_unshared_o      <= 32'd0;
      vid_opens_o         <= 32'd0;
      vid_sunk_o          <= 32'd0;
      vid_index_oob_o     <= 32'd0;
      vid_key_split_o     <= 32'd0;
      vid_id_unnameable_o <= 32'd0;
      vid_seal_abort_o    <= 32'd0;
      vid_stall_o         <= 32'd0;
    end else begin
      // spec/counters.md 4: saturate, never wrap.
      if (tri_valid_i && !tri_ready_o && (vid_stall_o != 32'hffff_ffff))
        vid_stall_o <= vid_stall_o + 32'd1;

      if (frame_seal_i) begin
        // THE FRAME BOUNDARY, ON THE ALLOCATOR'S OWN EDGE. Everything derived
        // from the previous frame's allocation goes at once: no row survives,
        // no epoch survives, no generation memory survives.
        map_valid_q <= '0;
        seen_q      <= '0;
        for (i = 0; i < ARENAS; i = i + 1) begin
          epoch_q[i]   <= '0;
          lastgen_q[i] <= '0;
        end
        // A triangle straddling the boundary is DROPPED, not carried: its
        // corners were identified in the old frame and the ids it holds name
        // records the allocator has just stopped owning. `pv_valid_o` and
        // `td_valid_o` are suppressed on this clock too, so the arena's intake
        // arm and its seal arm cannot both fire on the same edge.
        if (st_q != S_IDLE) begin
          if (vid_seal_abort_o != 32'hffff_ffff)
            vid_seal_abort_o <= vid_seal_abort_o + 32'd1;
        end
        st_q     <= S_IDLE;
        k_q      <= 2'd0;
        poison_q <= 1'b0;
      end else begin
        case (st_q)
          S_IDLE: begin
            if (tri_valid_i) begin
              dom_q    <= tri_domain_i;
              untex_q  <= tri_untex_i;
              mat_q    <= tri_material_i;
              rast_q   <= tri_raster_i;
              src_q    <= tri_src_id_i;
              key_q[0] <= tri_key_a_i;
              key_q[1] <= tri_key_b_i;
              key_q[2] <= tri_key_c_i;
              cx_q[0]  <= tri_ax_i;  cy_q[0] <= tri_ay_i;
              cx_q[1]  <= tri_bx_i;  cy_q[1] <= tri_by_i;
              cx_q[2]  <= tri_cx_i;  cy_q[2] <= tri_cy_i;
              att_q[0] <= tri_attr_a_i;
              att_q[1] <= tri_attr_b_i;
              att_q[2] <= tri_attr_c_i;
              id_q[0]  <= 16'd0; id_q[1] <= 16'd0; id_q[2] <= 16'd0;
              k_q      <= 2'd0;
              poison_q <= 1'b0;
              st_q     <= S_LOOK;
              if (vid_tris_o != 32'hffff_ffff) vid_tris_o <= vid_tris_o + 32'd1;
              // THE ASSUMPTION THIS BLOCK MAKES ABOUT ITS PRODUCER, MEASURED
              // RATHER THAN ASSERTED. All three corners of a mesh triangle come
              // from one meshlet in one view, so they share {arena, gen}.
              // GEOM.REPLAY drives all three lookups from one register pair, so
              // it holds by construction -- and this counts the clock it stops
              // holding instead of the block quietly coping.
              if (SHARED_DOMAINS[tri_domain_i] &&
                  ((tri_key_a_i[KEYW-1 -: (ARENA_W+GEN_W)] !=
                    tri_key_b_i[KEYW-1 -: (ARENA_W+GEN_W)]) ||
                   (tri_key_a_i[KEYW-1 -: (ARENA_W+GEN_W)] !=
                    tri_key_c_i[KEYW-1 -: (ARENA_W+GEN_W)])) &&
                  (vid_key_split_o != 32'hffff_ffff))
                vid_key_split_o <= vid_key_split_o + 32'd1;
            end
          end

          S_LOOK: begin
            // The epoch advance and the row read happen on ONE edge, and the
            // epoch used is registered beside the row address it was used
            // with. S_DEC then compares two quantities captured together.
            shareable_q <= shareable_c;
            row_q       <= cur_row_c;
            eff_epoch_q <= eff_epoch_c;
            map_hitv_q  <= map_valid_q[cur_row_c];
            if (open_c) begin
              epoch_q[cur_an_c]   <= eff_epoch_c;
              lastgen_q[cur_an_c] <= cur_gen_c;
              seen_q[cur_an_c]    <= 1'b1;
              if (vid_opens_o != 32'hffff_ffff) vid_opens_o <= vid_opens_o + 32'd1;
            end
            if (vid_refs_o != 32'hffff_ffff) vid_refs_o <= vid_refs_o + 32'd1;
            if (!dom_shared_c) begin
              if (vid_unshared_o != 32'hffff_ffff) vid_unshared_o <= vid_unshared_o + 32'd1;
            end else if (!key_ok_c) begin
              // A MESH corner whose key cannot be held. Published as its own
              // vertex and counted -- never folded into row zero, which is what
              // a truncated index would do.
              if (vid_index_oob_o != 32'hffff_ffff) vid_index_oob_o <= vid_index_oob_o + 32'd1;
              if (vid_unshared_o != 32'hffff_ffff) vid_unshared_o <= vid_unshared_o + 32'd1;
            end
            st_q <= S_DEC;
          end

          S_DEC: begin
            if (hit_c) begin
              id_q[k_q] <= map_rd_q[15:0];
              if (vid_reused_o != 32'hffff_ffff) vid_reused_o <= vid_reused_o + 32'd1;
              if (k_q == 2'd2) begin
                st_q <= S_TD;
              end else begin
                k_q  <= k_q + 2'd1;
                st_q <= S_LOOK;
              end
            end else begin
              st_q <= S_PUB;
            end
          end

          S_PUB: begin
            if (pv_fire_c) begin
              if (pv_accept_i && !id_unnameable_c) begin
                id_q[k_q] <= pv_id_i[15:0];
                if (vid_published_o != 32'hffff_ffff)
                  vid_published_o <= vid_published_o + 32'd1;
              end else begin
                // Consumed and not allocated. The arena is sinking this frame's
                // records -- no seal, or a fault -- so the descriptor that
                // names this corner is being sunk with it. The id is left at
                // its defined 0 and the fact is COUNTED, so a console whose
                // arena is silently empty is visible rather than plausible.
                id_q[k_q] <= 16'd0;
                poison_q  <= 1'b1;
                if (vid_sunk_o != 32'hffff_ffff) vid_sunk_o <= vid_sunk_o + 32'd1;
                if (id_unnameable_c && (vid_id_unnameable_o != 32'hffff_ffff))
                  vid_id_unnameable_o <= vid_id_unnameable_o + 32'd1;
              end
              if (map_we_c) map_valid_q[row_q] <= 1'b1;
              if (k_q == 2'd2) begin
                st_q <= S_TD;
              end else begin
                k_q  <= k_q + 2'd1;
                st_q <= S_LOOK;
              end
            end
          end

          S_TD: begin
            if (td_fire_c) begin
              st_q     <= S_IDLE;
              k_q      <= 2'd0;
              poison_q <= 1'b0;
            end
          end

          default: st_q <= S_IDLE;
        endcase
      end
    end
  end

  // `poison_q` is held so a future consumer of this block can be told which
  // descriptors named a sunk corner. It is read by nothing today; the count is
  // `vid_sunk_o` and that is the number a reader wants.
  /* verilator lint_off UNUSEDSIGNAL */
  wire _unused_poison = poison_q;
  /* verilator lint_on UNUSEDSIGNAL */

endmodule

`default_nettype wire
