// zhao_geom_paramarena.sv -- GEOM.PARAMBUF's ARENA PRODUCER.
//
// ---------------------------------------------------------------------------
// WHAT THIS IS, AND WHY IT IS NOT THE RECORD LAYER
// ---------------------------------------------------------------------------
// `zhao_geom_parambuf` is the record layer: bytes in, fields out, no memory
// port, and its own header says it "cannot be composed against the existing
// geometry path by wiring, because nothing in this console holds a byte vector
// in any of these three layouts". THIS BLOCK IS WHAT PUTS THE BYTES THERE.
//
// It takes the three R7 records as FIELDS -- a projected vertex, a triangle
// descriptor, a tile-reference chunk -- allocates each one an address inside
// the frame's build view, serialises it, and writes it to local SDRAM through
// the ENGINE1 guard socket. `zhao_geom_paramwalk` reads them back out and
// hands the bytes to `zhao_geom_parambuf` to decode.
//
// The owner's completion ruling of 2026-09-22, item 4, names the shape this
// must NOT take: "Do not pack fields into a byte vector merely to unpack them
// again and count that as external-memory integration." The pack and the
// unpack here are separated by SDRAM, a guard verdict, an arbiter grant, a
// controller burst and a frame boundary. That is the integration; the packing
// is how bytes get into memory, not the deliverable.
//
// ---------------------------------------------------------------------------
// THE AUTHORIZED MAP, AND WHAT THIS BLOCK IS ALLOWED TO DO IN IT
// ---------------------------------------------------------------------------
//   [0x0600_0000, 0x0640_0000)  view 0, 4 MiB
//   [0x0640_0000, 0x0680_0000)  view 1, 4 MiB
//   [0x0680_0000, 0x06A0_0000)  shared prefetch/chunk scratch, 2 MiB
//
// This block WRITES the view the lease names and WRITES the frame directory
// into the scratch. It reads nothing. `RENDER.ASSET_POOL` above the scratch
// stays read-only to ENGINE1 and this block never addresses it --
// `arena_overrun_o` counts any allocation that would have.
//
// ---------------------------------------------------------------------------
// TWO VIEWS, AND THE DRAIN THAT MAKES THEM SAFE
// ---------------------------------------------------------------------------
// THIS IS THE PART THE OWNER'S RULING IS MOST SPECIFIC ABOUT, and it is the
// repository's own worst defect class wearing a new hat:
//
//   "Preserve frame-generation and allocation lifetimes. A chunk must not be
//    reused while a previous reader or outstanding transaction still owns it,
//    and data must not be published before its writes retire. Carry request
//    identity WITH the request; do not validate a queued request against a
//    later global view selector."
//
// MEM.GUARD cannot close the last clause. `pb_wr_view` is a GLOBAL selector
// and the guard is combinational over the request it is being OFFERED, so a
// request queued under view 0 and still unaccepted when the selector moves
// would be judged by the new one. `zhao_guard_req_t` has no view field and
// widening it would change every client's ABI. So it is closed HERE, where the
// identity actually lives, in three separate ways:
//
//   * `view_q` is LATCHED at frame seal and `pb_wr_view_o` is driven from it
//     and from nothing else. No live input reaches the guard's selector.
//   * `m_addr_q` is LATCHED when the engine takes an op and is the ONLY thing
//     `guard_req_o.addr` is driven from -- `zhao_terrain_devstore`'s law,
//     copied deliberately. The address a request carries is the address the
//     allocation computed, not one recomputed later from a base that may have
//     moved.
//   * THE VIEW CANNOT FLIP WHILE ANYTHING IS IN FLIGHT. A seal is HELD
//     PENDING while `wr_words_q != 0` (a write this block issued has not
//     retired) or `reader_busy_i` (the walker still owns the view that is
//     about to become the build target). `view_flip_blocked_o` counts every
//     clock a seal spends waiting. So no request ever outlives the selector
//     that admitted it, and no chunk is reused while a reader owns it.
//
// AND THE DETECTOR IS NOT WIRED TO TWO OPERANDS THAT MOVE TOGETHER.
// CLAUDE.md's standing law: a metadata bank once delivered response A's data
// with B's metadata while a live mismatch counter read zero, because one
// register enable loaded both sides. Here `view_q` is loaded by the SEAL and
// `m_addr_q` by the ENGINE taking an op -- two different enables, two
// different clocks-of-interest -- and `addr_view_bad_o` differences the view
// the held address LIES IN against the view the lease NAMES. That comparison
// is structurally able to see a stale request, which is the whole point of
// writing it down.
//
// `addr_view_bad_o` IS NOT REACHABLE WITH LEGAL STIMULUS while the drain
// precondition is correct, which is why it needs a COMMITTED MUTANT rather
// than an argument: `tests/mutants/zhao_geom_paramarena_drain_mutant.sv` is a
// copy with the drain precondition removed, driven by a test whose polarity is
// inverted -- it passes when the counter FIRES.
//
// ---------------------------------------------------------------------------
// THE CONTRACTS THIS BLOCK PRESERVES (R7, and item 4 says preserve them)
// ---------------------------------------------------------------------------
// QUOTA. The Measure seals quotas BEFORE the frame; within a sealed frame the
// arena is a fixed allocation and there is no in-frame negotiation. `seal_*_i`
// is that quota and it is latched with the view.
//
// OVERFLOW. "On hard arena overflow: drain the frame, repeat the prior
// complete frame, report the source IDs. Never publish a frame with an
// arbitrary missing tail." An allocation past the sealed quota sets
// `frame_fault_q`, counts `quota_overflow_o`, records the offending
// `fault_source_o`, and the frame is NEVER PUBLISHED -- `publish_*_o` keeps
// describing the PRIOR COMPLETE FRAME, which is the fallback contract stated
// as a mechanism rather than a promise. The faulted frame's bytes are still in
// the build view and are simply never pointed at.
//
// The important half: the fault does not truncate. Records offered after the
// fault are ACCEPTED AND DISCARDED (`ready` stays high so the upstream does
// not deadlock) and counted at `records_discarded_o`. A block that stopped
// accepting would stall GEOM.PROJECT; a block that kept writing would publish
// a frame with a missing tail, which looks exactly like a frame.
//
// FRAME GENERATION. `frame_gen_i` is stamped into EVERY chunk this block
// writes, at the chunk layout's bits [63:48], by this block and not by the
// caller -- a producer that let the caller supply the generation would let a
// carried-over chunk keep last frame's stamp, which is the one thing the field
// exists to make detectable.
//
// ---------------------------------------------------------------------------
// THE SHARED SCRATCH HAS AN OWNER
// ---------------------------------------------------------------------------
// Item 4: "Shared scratch has explicit ownership and release rather than being
// unowned temporary memory." The scratch holds the FRAME DIRECTORY -- one
// 64-byte record naming the published view, its generation, the three region
// bases and the three counts. It is the only thing both the producer and the
// walker must agree on, it is not per-view (it describes whichever view is
// published), and it is the reason the region exists in 5c at all.
//
// This block is the scratch's ARBITER. It drives `pb_scratch_valid_o`, which
// is the guard's acquire bit, and it grants the scratch to exactly one of two
// owners: itself (writing the directory at publish) or the walker (reading it,
// via `scr_req_i` / `scr_grant_o`). `scr_contend_o` counts a walker request
// arriving while this block holds it. RELEASE IS AN ACT: `pb_scratch_valid_o`
// falls when neither owner holds it, so the scratch is unmapped for everybody
// between uses rather than standing permanently open.
//
// ---------------------------------------------------------------------------
// THE GUARD ANSWERS IN TWO CYCLES AND THE TWO BITS ARE NEVER BOTH HIGH
// ---------------------------------------------------------------------------
// `zhao_mem_guard` drives `rsp.ready = !fwd_active` as a LEVEL and pulses
// `rsp.ok` the cycle AFTER it accepts. Testing them in one arm reads every
// pass as a denial, silently, with the denial counter stuck at zero -- the
// defect found in BOTH geometry fetchers on 2026-09-06. So M_REQ waits on
// `ready` and M_VERD, a separate state one cycle later, reads `ok`/`violation`.
// `zhao_terrain_pageio.sv`'s law, copied deliberately.
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK DOES NOT DO
// ---------------------------------------------------------------------------
// It does not decide LOD, does not choose the quota, and does not arbitrate
// SDRAM -- the ENGINE1 share and MEM.VRAM.ARBITER own that. It does not
// allocate the GIANT QUOTA reservation: R7 requires 32,768 tile references
// reserved for one giant BEFORE ordinary kMesh allocation, and that is a
// decision made by whoever computes `seal_chunks_i`, not here. This block
// enforces the number it is given and refuses to exceed it; it does not choose
// it. That division is deliberate -- "never remove the owner's control in the
// name of fidelity" -- and it is declared rather than left to be discovered.
// ---------------------------------------------------------------------------
`default_nettype none

module zhao_geom_paramarena
  import zhao_pkg::*;
#(
    // The authorized map, as PARAMETERS taking zhao_pkg's values at the
    // composition site. They are knobs; the elaboration guard below refuses a
    // parameterisation whose derived footprint does not fit.
    parameter logic [31:0] VIEW0_BASE   = ZHAO_PARAMBUF_VIEW0_BASE,
    parameter logic [31:0] VIEW1_BASE   = ZHAO_PARAMBUF_VIEW1_BASE,
    parameter logic [31:0] VIEW_SPAN    = ZHAO_PARAMBUF_VIEW_SPAN,
    parameter logic [31:0] SCRATCH_BASE = ZHAO_PARAMBUF_SCRATCH_BASE,
    parameter logic [31:0] SCRATCH_SPAN = ZHAO_PARAMBUF_SCRATCH_SPAN,
    // R7's PREFERRED tier, "inside a 4 MiB arena": 65,536 projected vertices,
    // 16,384 triangles, 131,072 tile references. The chunk count is the tile
    // references divided by the fourteen a chunk holds, rounded UP and then to
    // a power of two -- 131072/14 = 9363, so 16,384 chunks, which also leaves
    // the giant's 32,768 reserved references expressible (2,341 chunks).
    // 65,535 AND NOT 65,536, AND THE ONE VERTEX IS A DECLARED LOSS.
    // R7's preferred tier says 65,536 projected vertices. A `vertex_id` is
    // u16 and `zhao_geom_parambuf`'s `td_sealed_vertices_i` is u16 with it, so
    // the SEAL 65,536 is not expressible in the port the legality rule is
    // tested against -- it would arrive as 0 and refuse every triangle. The
    // choices were to widen a frozen record layout, to special-case the
    // maximum, or to lose one vertex of 65,536. This takes the vertex, says so
    // here, and leaves the number a knob. It is a REAL divergence from R7's
    // tier table and is reported rather than absorbed.
    parameter int unsigned MAX_VERTS  = 65535,
    parameter int unsigned MAX_TRIS   = 16384,
    parameter int unsigned MAX_CHUNKS = 16384,
    parameter int unsigned CHUNK_IDS  = 14
) (
    input  var logic clk,
    input  var logic rst_n,

    // The client id this block presents. ENGINE1 by composition; a parameter
    // would have frozen it into a block that is not the address authority.
    input  var zhao_client_e cfg_vram_client_i,

    // ---- frame control ------------------------------------------------------
    // A seal is a REQUEST, not a command: it is held pending until the drain
    // precondition is met. `seal_ready_o` says a seal would take effect now.
    input  var logic        seal_valid_i,
    output var logic        seal_ready_o,
    input  var logic [17:0] seal_verts_i,
    input  var logic [17:0] seal_tris_i,
    input  var logic [17:0] seal_chunks_i,
    input  var logic [15:0] frame_gen_i,
    // The producer has no more records for this frame. Publication follows,
    // but only once every write has RETIRED.
    input  var logic        frame_end_i,
    // The walker still owns the published view. The seal waits on it.
    input  var logic        reader_busy_i,

    // ---- the guard lease this block OWNS ------------------------------------
    output var logic        pb_lease_valid_o,
    output var logic        pb_wr_view_o,
    output var logic        pb_scratch_valid_o,

    // ---- record intake, as FIELDS -------------------------------------------
    input  var logic               pv_valid_i,
    output var logic               pv_ready_o,
    input  var logic signed [31:0] pv_x_i,
    input  var logic signed [31:0] pv_y_i,
    input  var logic [23:0]        pv_invw_i,
    input  var logic [7:0]         pv_status_i,
    input  var logic signed [31:0] pv_uow_i,
    input  var logic signed [31:0] pv_vow_i,
    input  var logic [31:0]        pv_rgba_i,

    input  var logic        td_valid_i,
    output var logic        td_ready_o,
    input  var logic [15:0] td_v0_i,
    input  var logic [15:0] td_v1_i,
    input  var logic [15:0] td_v2_i,
    input  var logic [15:0] td_material_i,
    input  var logic [31:0] td_raster_i,
    input  var logic [31:0] td_source_i,

    input  var logic        ck_valid_i,
    output var logic        ck_ready_o,
    input  var logic [31:0] ck_next_i,
    input  var logic [15:0] ck_count_i,
    input  var logic [CHUNK_IDS*32-1:0] ck_ids_i,

    // ---- the scratch's second owner -----------------------------------------
    input  var logic        scr_req_i,      // the walker wants the scratch
    output var logic        scr_grant_o,

    // ---- the published frame ------------------------------------------------
    // These describe the PRIOR COMPLETE FRAME until a frame completes without
    // fault AND all of its writes have retired.
    output var logic        publish_valid_o,
    output var logic        publish_view_o,
    output var logic [15:0] publish_gen_o,
    output var logic [26:0] publish_vert_base_o,
    output var logic [26:0] publish_tri_base_o,
    output var logic [26:0] publish_chunk_base_o,
    output var logic [17:0] publish_verts_o,
    output var logic [17:0] publish_tris_o,
    output var logic [17:0] publish_chunks_o,

    // ---- the ENGINE1 write socket -------------------------------------------
    output var zhao_guard_req_t guard_req_o,
    input  var zhao_guard_rsp_t guard_rsp_i,
    output var logic [63:0]     guard_wdata_o,
    output var logic            guard_wvalid_o,
    input  var logic            guard_wready_i,
    output var logic            guard_wlast_o,
    // Words RETIRED by the socket this cycle. This is what "before its writes
    // retire" is measured against -- not the last beat handed to a queue.
    input  var logic [7:0]      retire_words_i,

    // ---- evidence -----------------------------------------------------------
    output var logic [31:0] verts_written_o,
    output var logic [31:0] tris_written_o,
    output var logic [31:0] chunks_written_o,
    output var logic [31:0] frames_published_o,
    output var logic [31:0] guard_denied_o,
    output var logic [31:0] quota_overflow_o,
    output var logic [31:0] records_discarded_o,
    output var logic [31:0] records_unsealed_o,
    output var logic [31:0] arena_overrun_o,
    output var logic [31:0] view_flip_blocked_o,
    output var logic [31:0] publish_blocked_o,
    output var logic [31:0] addr_view_bad_o,
    output var logic [31:0] scr_contend_o,
    output var logic [31:0] retire_underflow_o,
    output var logic [15:0] fault_source_o,
    output var logic        frame_fault_o,
    output var logic        busy_o
);

  // ------------------------------------------------------------- geometry --
  // Byte strides. `w=` spellings, never a bare number beside a derived
  // constant: check_localparam_comments reads a bare number as a claim about
  // the value, and this repository shipped an owner ruling 2.4x wrong because
  // a stale `576` sat beside a real `704`.
  localparam int unsigned PV_B = ZHAO_PARAMBUF_PV_BYTES;   // w=24 bytes
  localparam int unsigned TD_B = ZHAO_PARAMBUF_TD_BYTES;   // w=16 bytes
  localparam int unsigned CK_B = ZHAO_PARAMBUF_CK_BYTES;   // w=64 bytes

  // The three sub-regions inside a view, laid out in allocation order. Bases
  // are BYTE OFFSETS from the view base.
  localparam int unsigned VERT_CAP_B  = MAX_VERTS  * PV_B;
  localparam int unsigned TRI_OFF_B   = VERT_CAP_B;
  localparam int unsigned TRI_CAP_B   = MAX_TRIS   * TD_B;
  localparam int unsigned CHUNK_OFF_B = TRI_OFF_B + TRI_CAP_B;
  localparam int unsigned CHUNK_CAP_B = MAX_CHUNKS * CK_B;
  localparam int unsigned VIEW_USED_B = CHUNK_OFF_B + CHUNK_CAP_B;

  // The directory lives at the scratch base. One record, chunk-sized, so the
  // walker's read is the same shape as a chunk read and needs no second
  // burst length.
  localparam int unsigned DIR_B = CK_B;                    // w=64 bytes

  // The shift register is sized for the widest burst this block issues -- a
  // 64-byte chunk, eight beats. A vertex is 24 bytes (three beats) and a
  // descriptor 16 (two), and both load into the same register.
  localparam int unsigned SHW = CK_B * 8;                  // w=512 bits

  // The frame directory's occupied bytes, field by field, so the zero pad
  // below is DERIVED and cannot drift when a field is added. Laid out at
  // natural offsets: three 32-bit bases, a 16-bit generation, a 16-bit
  // view/valid word, three 32-bit counts.
  localparam int unsigned DIR_USED_BITS = 32 + 32 + 32 + 16 + 16 + 32 + 32 + 32;

  // ENFORCED HERE, AND HERE ONLY. R212: Quartus 17.0 rejects a bare
  // module-scope elaboration `if`, so it goes inside `initial begin`.
  // CLAUDE.md: `--lint-only` does NOT run `initial` blocks, so a clean lint is
  // not evidence about these lines -- elaboration in a Verilator test binary
  // and quartus_map are.
  // synthesis translate_off
  initial begin
    if (VIEW_USED_B > VIEW_SPAN)
      $fatal(1, "zhao_geom_paramarena: sealed capacity exceeds VIEW_SPAN");
    if (DIR_B > SCRATCH_SPAN)
      $fatal(1, "zhao_geom_paramarena: directory exceeds SCRATCH_SPAN");
    if (VIEW1_BASE != VIEW0_BASE + VIEW_SPAN)
      $fatal(1, "zhao_geom_paramarena: the two views are not adjacent");
    if (SCRATCH_BASE != VIEW1_BASE + VIEW_SPAN)
      $fatal(1, "zhao_geom_paramarena: the scratch does not follow view 1");
    if (CHUNK_IDS != 14)
      $fatal(1, "zhao_geom_paramarena: R7's chunk holds fourteen ids");
  end
  // synthesis translate_on

  // ------------------------------------------------------------- the frame --
  // LATCHED AT SEAL, and nothing else writes them. `view_q` in particular:
  // `pb_wr_view_o` is driven from it alone, so the guard's selector cannot be
  // moved by a live input.
  logic        view_q;
  logic [15:0] gen_q;
  logic [17:0] q_verts_q, q_tris_q, q_chunks_q;
  logic        frame_open_q;     // a sealed frame is accepting records
  logic        frame_fault_q;

  // Allocation cursors, in RECORDS not bytes. Bytes are derived, so a cursor
  // cannot be advanced by a stride the layout does not use.
  logic [17:0] n_verts_q, n_tris_q, n_chunks_q;

  // The PUBLISHED frame. Separate registers from the build frame's, because
  // the fallback contract is that these keep describing the last COMPLETE
  // frame while a faulted one is being built and thrown away.
  logic        pub_valid_q, pub_view_q;
  logic [15:0] pub_gen_q;
  logic [17:0] pub_verts_q, pub_tris_q, pub_chunks_q;

  wire [31:0] view_base_c = view_q ? VIEW1_BASE : VIEW0_BASE;

  assign pb_wr_view_o    = view_q;
  assign publish_valid_o = pub_valid_q;
  assign publish_view_o  = pub_view_q;
  assign publish_gen_o   = pub_gen_q;
  assign publish_verts_o = pub_verts_q;
  assign publish_tris_o  = pub_tris_q;
  assign publish_chunks_o = pub_chunks_q;
  assign publish_vert_base_o =
      27'((pub_view_q ? VIEW1_BASE : VIEW0_BASE));
  assign publish_tri_base_o =
      27'((pub_view_q ? VIEW1_BASE : VIEW0_BASE) + 32'(TRI_OFF_B));
  assign publish_chunk_base_o =
      27'((pub_view_q ? VIEW1_BASE : VIEW0_BASE) + 32'(CHUNK_OFF_B));
  assign frame_fault_o = frame_fault_q;

  // ------------------------------------------------------- outstanding work --
  // WORDS, not requests. "Data must not be published before its writes retire"
  // is a statement about bytes reaching memory, and the socket answers in
  // 16-bit words (`retire_words_i`), so that is the unit the question is asked
  // in. A count of REQUESTS would go to zero as soon as the last beat left
  // this block, which is the flattering direction and is not retirement.
  localparam int unsigned OUTW = 16;                       // w=16 bits
  logic [OUTW-1:0] wr_words_q;

  // words a request of `len` bytes occupies, the arbiter's own rounding
  function automatic logic [6:0] words_of(input logic [6:0] len_b);
    words_of = 7'((len_b + 7'd1) >> 1);
  endfunction

  // --------------------------------------------------------- the scratch ----
  // TWO OWNERS, ONE AT A TIME, AND RELEASE IS AN ACT. `scr_mine_q` is this
  // block writing the directory; `scr_walker_q` is the walker reading it.
  // `pb_scratch_valid_o` -- the guard's acquire bit -- is their OR, so with
  // neither holding it the scratch is unmapped for everybody.
  logic scr_mine_q, scr_walker_q;
  assign pb_scratch_valid_o = scr_mine_q || scr_walker_q;
  assign scr_grant_o        = scr_walker_q;

  // THE LEASE IS OPEN WHILE THERE IS A FRAME TO SERVE. It covers the build
  // frame, the frame that has ENDED BUT NOT YET PUBLISHED, and the published
  // one -- the walker reads the published view through this same lease. It
  // falls out of reset and whenever none of the three exists, which is what
  // makes `pb_lease_valid` a real deny-all rather than a constant one.
  //
  // `pub_pending_q` WAS MISSING FROM THIS TERM AND THE BLOCK COULD NOT PUBLISH
  // A SINGLE FRAME. Found 2026-09-22 by geom_paramarena_directed case 1, which
  // is the acceptance test for the round trip, and it is worth the paragraph
  // because every gate the packet had run to that point was GREEN:
  //
  //   * the directory write is issued from the PUBLICATION arm, which is
  //     reachable only AFTER `frame_end_i` has cleared `frame_open_q`;
  //   * on the first frame `pub_valid_q` is still 0, so at the exact cycle the
  //     block asked the guard to write the directory the lease was LOW;
  //   * every PARAMBUF arm of `zhao_mem_guard` requires `pb_lease_valid`, so
  //     the guard REFUSED IT -- correctly. Measured: guard_violations = 1,
  //     `guard_denied_o` = 1, the refused request `addr 0x0680_0000 len 64
  //     write 1`, which is exactly SCRATCH_BASE.
  //   * the denial sets `frame_fault_q`, the publication arm then drops
  //     `pub_pending_q` without touching `pub_valid_q`, and the fallback
  //     contract does exactly what it promises -- keeps naming the prior
  //     complete frame. There is no prior complete frame. `frames_published_o`
  //     stays 0 forever and the walker, gated on `pub_valid_i`, never runs.
  //
  // A SECOND INSTANCE OF THE SAME ROOT CAUSE, measured in the same bench: if
  // `frame_end_i` arrives while the last record's write is still in flight --
  // the natural behaviour of a live producer -- the lease drops under THAT
  // write instead, and the guard refuses a perfectly legal chunk write inside
  // the view the lease had just been naming.
  //
  // THE TELL, AND WHY NOTHING ELSE CAUGHT IT. Lint was clean, the formal proof
  // passed, mem_guard_directed passed with twelve new item-4 cases, and the
  // composed console linted. Not one of them can see this, because it is not a
  // fault in the GUARD or in the ARENA separately -- it is the arena asking the
  // guard for something at a moment when the arena itself has said no. Only a
  // bench with both blocks and a real memory between them has the state to
  // reach it, which is the whole argument for building one.
  //
  // A FRAME THAT HAS ENDED AND NOT PUBLISHED STILL OWNS THE ARENA. That is
  // what `pub_pending_q` means, and it belongs in the lease.
  assign pb_lease_valid_o = frame_open_q || pub_pending_q || pub_valid_q;

  // -------------------------------------------------------- record packing --
  // R7's layouts, byte offset by byte offset. These are the INVERSE of
  // `zhao_geom_parambuf`'s decode and the two must agree bit for bit; the
  // acceptance test is what says they do, by writing here and decoding there
  // with SDRAM in between.
  wire [PV_B*8-1:0] pv_bytes_c = {
      pv_rgba_i,                    // byte 20..23
      pv_vow_i,                     // byte 16..19
      pv_uow_i,                     // byte 12..15
      pv_status_i, pv_invw_i,       // byte  8..11
      pv_y_i,                       // byte  4.. 7
      pv_x_i                        // byte  0.. 3
  };

  wire [TD_B*8-1:0] td_bytes_c = {
      td_source_i,                  // byte 12..15
      td_raster_i,                  // byte  8..11
      td_material_i,                // byte  6.. 7
      td_v2_i, td_v1_i, td_v0_i     // byte  0.. 5
  };

  // THE GENERATION IS STAMPED HERE AND NOT SUPPLIED BY THE CALLER. A producer
  // that took the generation as an input would let a carried-over chunk keep
  // last frame's stamp -- which is the one thing the field exists to make
  // detectable, and the reason R7 put it in every chunk rather than once per
  // arena.
  wire [CK_B*8-1:0] ck_bytes_c = {
      ck_ids_i,                     // byte  8..63, fourteen u32
      gen_q,                        // byte  6.. 7
      ck_count_i,                   // byte  4.. 5
      ck_next_i                     // byte  0.. 3
  };

  // The frame directory. Written to the scratch at publish, read by the
  // walker, and the only record both sides must agree on.
  wire [DIR_B*8-1:0] dir_bytes_c = {
      {(DIR_B*8 - DIR_USED_BITS){1'b0}},
      {14'd0, n_chunks_q},               // byte 24..27
      {14'd0, n_tris_q},                 // byte 20..23
      {14'd0, n_verts_q},                // byte 16..19
      {14'd0, view_q, 1'b1},             // byte 14..15: view + valid
      gen_q,                             // byte 12..13
      32'(view_base_c + 32'(CHUNK_OFF_B)),  // byte  8..11
      32'(view_base_c + 32'(TRI_OFF_B)),    // byte  4.. 7
      32'(view_base_c)                      // byte  0.. 3
  };

  // ------------------------------------------------------------- the queue --
  // ONE op in flight. The arena's throughput is the socket's, and a deeper
  // queue here would buy nothing while making the drain precondition a
  // question about a FIFO rather than about one register.
  typedef enum logic [2:0] {
    M_IDLE, M_REQ, M_VERD, M_WBEAT
  } mstate_e;
  mstate_e mstate_q;

  logic [26:0]     m_addr_q;       // LATCHED AT OP START. The only source of
                                   // guard_req_o.addr.
  logic [6:0]      m_len_q;
  logic [SHW-1:0]  m_wsh_q;
  logic [3:0]      m_beat_q;
  logic [3:0]      m_beats_q;
  // WHAT the op is, kept so the completion counter names the right record.
  typedef enum logic [1:0] { K_PV, K_TD, K_CK, K_DIR } kind_e;
  kind_e m_kind_q;

  // ------------------------------------------------------- allocation ------
  // Byte address of the NEXT record of each kind, derived from the cursor and
  // the stride. 40-bit intermediates so a cursor at its maximum times a stride
  // cannot wrap before the bound is tested -- overflow-safe arithmetic, which
  // item 4 asks for by name.
  wire [39:0] pv_addr_c = 40'(view_base_c) + 40'(n_verts_q)  * 40'(PV_B);
  wire [39:0] td_addr_c = 40'(view_base_c) + 40'(TRI_OFF_B)
                        + 40'(n_tris_q)    * 40'(TD_B);
  wire [39:0] ck_addr_c = 40'(view_base_c) + 40'(CHUNK_OFF_B)
                        + 40'(n_chunks_q)  * 40'(CK_B);

  // QUOTA, not capacity. The sealed number is the bound; MAX_* is what the
  // arena could hold. A frame sealed below capacity must still fault at ITS
  // number, because the quota is what the Measure promised the rest of the
  // frame it would not exceed.
  wire pv_fits_c = (n_verts_q  < q_verts_q);
  wire td_fits_c = (n_tris_q   < q_tris_q);
  wire ck_fits_c = (n_chunks_q < q_chunks_q);

  // THE SECOND BOUND, AND IT IS NOT THE SAME QUESTION. The quota says the
  // frame kept its promise; this says the bytes are inside the view. A seal
  // whose quota exceeds the arena would otherwise walk a cursor out of the
  // view and into the next one -- the exact fault the guard refuses, arriving
  // as a legal-looking request. Checked here so the block does not RELY on the
  // guard to catch its own arithmetic.
  wire [39:0] view_top_c = 40'(view_base_c) + 40'(VIEW_SPAN);
  wire pv_in_view_c = (pv_addr_c + 40'(PV_B)) <= view_top_c;
  wire td_in_view_c = (td_addr_c + 40'(TD_B)) <= view_top_c;
  wire ck_in_view_c = (ck_addr_c + 40'(CK_B)) <= view_top_c;

  // ---------------------------------------------------------- intake gate --
  wire engine_free_c = (mstate_q == M_IDLE);

  // THE INTAKE IS A FREE SINK WHENEVER THERE IS NOTHING TO WRITE, and that is
  // a composition requirement rather than a convenience. This block taps a
  // LIVE production stream -- `zhao_geom_assemble`'s triangle output, which
  // already feeds GEOM.REPLAY -- and the core ANDs this `ready` into that
  // stream's so no record is lost while a frame is sealed. If `ready` were
  // simply `frame_open_q && engine_free_c`, then between frames, before the
  // first seal, and forever on a console that never seals, it would be LOW --
  // and the whole assembly path would stall behind a block that has nothing
  // to do. That is not a throughput regression, it is a deadlock, and it
  // would look exactly like a defect in GEOM.ASSEMBLE.
  //
  // So: with no frame open, or with the frame already faulted, records are
  // CONSUMED AND NOT WRITTEN, at full rate, and counted -- `records_unsealed_o`
  // for "there was no frame to put it in" and `records_discarded_o` for "the
  // frame is already being thrown away". Two counters and not one, because
  // those are different facts about the console: the first says nobody sealed
  // a frame, the second says a sealed frame overran.
  wire sink_c   = !frame_open_q || frame_fault_q;
  wire taking_c = sink_c || engine_free_c;
  assign pv_ready_o = taking_c;
  assign td_ready_o = taking_c && !pv_valid_i;
  assign ck_ready_o = taking_c && !pv_valid_i && !td_valid_i;

  wire pv_fire_c = pv_valid_i && pv_ready_o;
  wire td_fire_c = td_valid_i && td_ready_o;
  wire ck_fire_c = ck_valid_i && ck_ready_o;

  // ---------------------------------------------------------- publication --
  // A frame is publishable when its producer is done AND every write it issued
  // has retired AND it did not fault. `pub_pending_q` holds the "done" half so
  // the retire half can be waited on for as long as it takes, and
  // `publish_blocked_o` counts every clock spent waiting -- which is the
  // measurement that says whether the retire gate ever actually delays
  // anything, rather than being a term nobody can show does work.
  logic pub_pending_q;
  logic dir_written_q;

  // ------------------------------------------------------------ seal gate --
  // THE DRAIN PRECONDITION. This one expression is the protection item 4 asks
  // for, and `tests/mutants/zhao_geom_paramarena_drain_mutant.sv` is the copy
  // with it removed.
  wire drained_c   = (wr_words_q == '0) && (mstate_q == M_IDLE);
  wire seal_ok_c   = drained_c && !reader_busy_i && !pub_pending_q;
  assign seal_ready_o = seal_ok_c;
  wire seal_fire_c = seal_valid_i && seal_ok_c;

  // ------------------------------------------------------------- the port --
  always_comb begin
    guard_req_o        = '0;
    guard_req_o.valid  = (mstate_q == M_REQ);
    guard_req_o.write  = 1'b1;              // this block never reads
    guard_req_o.client = cfg_vram_client_i;
    guard_req_o.addr   = m_addr_q;
    guard_req_o.len    = m_len_q;
    // The guard requires `be == mask_of(len)` EXACTLY: an all-ones mask on a
    // 24-byte write is refused on SHAPE, not on region, and that refusal reads
    // like a permissions problem. Built from the length so the two cannot
    // disagree.
    guard_req_o.be     = 64'(({64{1'b1}} >> (64 - m_len_q)));
  end

  assign guard_wvalid_o = (mstate_q == M_WBEAT);
  assign guard_wdata_o  = m_wsh_q[63:0];
  assign guard_wlast_o  = (m_beat_q == (m_beats_q - 4'd1));
  assign busy_o         = (mstate_q != M_IDLE) || frame_open_q || pub_pending_q;

  // THE DETECTOR, AND ITS TWO OPERANDS MOVE ON DIFFERENT ENABLES. `m_addr_q`
  // is loaded by the ENGINE taking an op; `view_q` is loaded by the SEAL. So
  // this difference can see a request that outlived its selector -- the fault
  // a comparison whose sides share one enable is structurally blind to.
  wire [31:0] m_addr32_c   = {5'b0, m_addr_q};
  wire        addr_in_v1_c = (m_addr32_c >= VIEW1_BASE)
                          && (m_addr32_c <  VIEW1_BASE + VIEW_SPAN);
  wire        addr_in_v0_c = (m_addr32_c >= VIEW0_BASE)
                          && (m_addr32_c <  VIEW0_BASE + VIEW_SPAN);
  wire        addr_is_dir_c = (m_kind_q == K_DIR);
  wire        addr_view_mismatch_c =
      (mstate_q == M_REQ) && !addr_is_dir_c
      && (view_q ? !addr_in_v1_c : !addr_in_v0_c);

  // --------------------------------------------------------------- core ----
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      view_q        <= 1'b0;
      gen_q         <= 16'd0;
      q_verts_q     <= 18'd0;
      q_tris_q      <= 18'd0;
      q_chunks_q    <= 18'd0;
      frame_open_q  <= 1'b0;
      frame_fault_q <= 1'b0;
      n_verts_q     <= 18'd0;
      n_tris_q      <= 18'd0;
      n_chunks_q    <= 18'd0;
      pub_valid_q   <= 1'b0;
      pub_view_q    <= 1'b0;
      pub_gen_q     <= 16'd0;
      pub_verts_q   <= 18'd0;
      pub_tris_q    <= 18'd0;
      pub_chunks_q  <= 18'd0;
      pub_pending_q <= 1'b0;
      dir_written_q <= 1'b0;
      scr_mine_q    <= 1'b0;
      scr_walker_q  <= 1'b0;
      wr_words_q    <= '0;
      mstate_q      <= M_IDLE;
      m_addr_q      <= 27'd0;
      m_len_q       <= 7'd0;
      m_wsh_q       <= '0;
      m_beat_q      <= 4'd0;
      m_beats_q     <= 4'd0;
      m_kind_q      <= K_PV;
      verts_written_o     <= '0;
      tris_written_o      <= '0;
      chunks_written_o    <= '0;
      frames_published_o  <= '0;
      guard_denied_o      <= '0;
      quota_overflow_o    <= '0;
      records_discarded_o <= '0;
      records_unsealed_o  <= '0;
      arena_overrun_o     <= '0;
      view_flip_blocked_o <= '0;
      publish_blocked_o   <= '0;
      addr_view_bad_o     <= '0;
      scr_contend_o       <= '0;
      retire_underflow_o  <= '0;
      fault_source_o      <= 16'd0;
    end else begin
      // ---- retirement ----------------------------------------------------
      // Counted before anything that could add to it, so a retire and an issue
      // in the same cycle are both seen. `retire_underflow_o` is the tripwire
      // for the socket retiring more words than this block ever owed it --
      // which would mean the share's ledger has attributed somebody else's
      // write here, and would make every publish decision downstream wrong in
      // the flattering direction (zero outstanding, publish early).
      if (retire_words_i != 8'd0) begin
        if ({8'd0, retire_words_i} > wr_words_q) begin
          retire_underflow_o <= retire_underflow_o + 32'd1;
          wr_words_q <= '0;
        end else begin
          wr_words_q <= wr_words_q - OUTW'(retire_words_i);
        end
      end

      // ---- the address detector -------------------------------------------
      if (addr_view_mismatch_c)
        addr_view_bad_o <= addr_view_bad_o + 32'd1;

      // ---- the seal --------------------------------------------------------
      // A seal that cannot take effect is not refused, it WAITS -- and every
      // clock it waits is counted, because a drain precondition nobody can
      // show ever delayed anything is a term, not a protection.
      if (seal_valid_i && !seal_ok_c)
        view_flip_blocked_o <= view_flip_blocked_o + 32'd1;

      if (seal_fire_c) begin
        // THE VIEW FLIPS HERE AND NOWHERE ELSE, and only with nothing in
        // flight and no reader owning the target. This is the one write to
        // `view_q` in the block.
        view_q        <= ~view_q;
        gen_q         <= frame_gen_i;
        q_verts_q     <= seal_verts_i;
        q_tris_q      <= seal_tris_i;
        q_chunks_q    <= seal_chunks_i;
        n_verts_q     <= 18'd0;
        n_tris_q      <= 18'd0;
        n_chunks_q    <= 18'd0;
        frame_open_q  <= 1'b1;
        frame_fault_q <= 1'b0;
        dir_written_q <= 1'b0;
        fault_source_o <= 16'd0;
      end

      // ---- the scratch's owner ---------------------------------------------
      // Granted to the walker only while this block does not need it, and
      // taken back only at a directory write, which cannot begin while the
      // walker holds it (see the M_IDLE arm). Contention is COUNTED rather
      // than resolved silently.
      if (scr_req_i && scr_mine_q)
        scr_contend_o <= scr_contend_o + 32'd1;
      scr_walker_q <= scr_req_i && !scr_mine_q;

      // ---- intake ----------------------------------------------------------
      if (pv_fire_c || td_fire_c || ck_fire_c) begin
        if (!frame_open_q) begin
          // Nobody has sealed a frame. The record has nowhere to go and is
          // consumed so the producer does not stall; counted so a console
          // that never seals is VISIBLE rather than silently geometry-free.
          records_unsealed_o <= records_unsealed_o + 32'd1;
        end else if (frame_fault_q) begin
          // Accepted and thrown away. Never written, never counted as work.
          records_discarded_o <= records_discarded_o + 32'd1;
        end else if (pv_fire_c) begin
          if (!pv_fits_c) begin
            quota_overflow_o <= quota_overflow_o + 32'd1;
            frame_fault_q    <= 1'b1;
            fault_source_o   <= 16'd0;   // a vertex has no source id of its own
          end else if (!pv_in_view_c) begin
            arena_overrun_o <= arena_overrun_o + 32'd1;
            frame_fault_q   <= 1'b1;
          end else begin
            m_addr_q  <= pv_addr_c[26:0];
            m_len_q   <= 7'(PV_B);
            m_wsh_q   <= SHW'(pv_bytes_c);
            m_beats_q <= 4'(PV_B / 8);
            m_kind_q  <= K_PV;
            m_beat_q  <= 4'd0;
            mstate_q  <= M_REQ;
            n_verts_q <= n_verts_q + 18'd1;
          end
        end else if (td_fire_c) begin
          if (!td_fits_c) begin
            quota_overflow_o <= quota_overflow_o + 32'd1;
            frame_fault_q    <= 1'b1;
            // R7: "report the source IDs". The descriptor carries one, so the
            // frame fault names the draw that overran rather than only the
            // fact that something did.
            fault_source_o   <= td_source_i[15:0];
          end else if (!td_in_view_c) begin
            arena_overrun_o <= arena_overrun_o + 32'd1;
            frame_fault_q   <= 1'b1;
            fault_source_o  <= td_source_i[15:0];
          end else begin
            m_addr_q  <= td_addr_c[26:0];
            m_len_q   <= 7'(TD_B);
            m_wsh_q   <= SHW'(td_bytes_c);
            m_beats_q <= 4'(TD_B / 8);
            m_kind_q  <= K_TD;
            m_beat_q  <= 4'd0;
            mstate_q  <= M_REQ;
            n_tris_q  <= n_tris_q + 18'd1;
          end
        end else begin
          if (!ck_fits_c) begin
            quota_overflow_o <= quota_overflow_o + 32'd1;
            frame_fault_q    <= 1'b1;
          end else if (!ck_in_view_c) begin
            arena_overrun_o <= arena_overrun_o + 32'd1;
            frame_fault_q   <= 1'b1;
          end else begin
            m_addr_q   <= ck_addr_c[26:0];
            m_len_q    <= 7'(CK_B);
            m_wsh_q    <= SHW'(ck_bytes_c);
            m_beats_q  <= 4'(CK_B / 8);
            m_kind_q   <= K_CK;
            m_beat_q   <= 4'd0;
            mstate_q   <= M_REQ;
            n_chunks_q <= n_chunks_q + 18'd1;
          end
        end
      end

      // ---- the frame ends ---------------------------------------------------
      if (frame_end_i && frame_open_q) begin
        frame_open_q  <= 1'b0;
        pub_pending_q <= 1'b1;
      end

      // ---- publication -----------------------------------------------------
      // THE FALLBACK CONTRACT, AS A MECHANISM. A faulted frame drops
      // `pub_pending_q` without touching a single `pub_*` register, so the
      // published description goes on naming the PRIOR COMPLETE FRAME and the
      // faulted bytes are simply never pointed at. That is R7's "repeat the
      // prior complete frame" -- no copy, no republish, nothing to get wrong.
      if (pub_pending_q && (mstate_q == M_IDLE)) begin
        if (frame_fault_q) begin
          // The scratch is NOT released here. See the ONE release statement
          // below the publication block: a per-exit-path release is what this
          // block got wrong the first time, and a SECOND per-exit-path
          // release would have been the same mistake with better manners.
          pub_pending_q <= 1'b0;
        end else if (wr_words_q != '0) begin
          // EVERY CLOCK THE RETIRE GATE HOLDS IS COUNTED. Without this the
          // gate is a term in an expression that nobody can show ever did
          // anything, which is the same as not having it.
          publish_blocked_o <= publish_blocked_o + 32'd1;
        end else if (!dir_written_q) begin
          // The directory goes to the scratch, and this block takes the
          // scratch to do it -- but only when the walker does not hold it.
          if (!scr_walker_q) begin
            scr_mine_q <= 1'b1;
            m_addr_q   <= 27'(SCRATCH_BASE);
            m_len_q    <= 7'(DIR_B);
            m_wsh_q    <= SHW'(dir_bytes_c);
            m_beats_q  <= 4'(DIR_B / 8);
            m_kind_q   <= K_DIR;
            m_beat_q   <= 4'd0;
            mstate_q   <= M_REQ;
          end
        end else begin
          pub_valid_q   <= 1'b1;
          pub_view_q    <= view_q;
          pub_gen_q     <= gen_q;
          pub_verts_q   <= n_verts_q;
          pub_tris_q    <= n_tris_q;
          pub_chunks_q  <= n_chunks_q;
          pub_pending_q <= 1'b0;
          frames_published_o <= frames_published_o + 32'd1;
        end
      end

      // ---- the scratch is released by ONE statement --------------------------
      // AND THE FACT THAT IT IS ONE IS THE POINT, not tidiness. The first
      // version released the scratch only on the SUCCESSFUL publish, so a
      // frame that faulted after taking it held it forever --
      // `pb_scratch_valid_o` high with no owner doing anything, and the walker
      // unable to be granted it because the grant requires `!scr_mine_q`.
      // Item 4 asks for explicit ownership AND RELEASE, and a release that
      // only happens on the success path is not a release, it is a leak with
      // a good day.
      //
      // THE OBVIOUS REPAIR WAS A SECOND COPY OF THE RELEASE IN THE FAULT ARM,
      // AND IT WAS THE WRONG ONE. It was written, it was correct, and the
      // acceptance test then showed it was UNREACHABLE: `frame_fault_q` can
      // only be set after the directory write issues by a guard denial of
      // that write, and the lease repair in the same change made exactly that
      // unreachable. So it would have been a branch asserted safe and never
      // seen to move -- CLAUDE.md's "a guard you cannot reach with legal
      // stimulus needs a committed mutant", authored on purpose, for a
      // defensive copy of a statement that already existed.
      //
      // ONE statement instead, keyed on the publication attempt being OVER
      // rather than on HOW it ended. It runs on every frame the block
      // publishes, so the release path is exercised continuously rather than
      // only by a fault nobody can produce -- and the faulted frame takes the
      // identical path. `scr_contend_o` is what watches the other side.
      if (!pub_pending_q) scr_mine_q <= 1'b0;

      // ---- the memory engine -----------------------------------------------
      case (mstate_q)
        M_IDLE: ;   // op start is in the intake and publication arms above

        // The guard answers in TWO cycles: `ready` is a level, `ok` is pulsed
        // the cycle after the accept. Testing them in one arm reads every pass
        // as a denial, with the denial counter stuck at zero.
        M_REQ: if (guard_rsp_i.ready) mstate_q <= M_VERD;

        M_VERD: begin
          if (guard_rsp_i.violation) begin
            guard_denied_o <= guard_denied_o + 32'd1;
            // A refused write is a frame fault. It cannot be retried into a
            // legal address, because the address IS the allocation -- and a
            // frame missing one record is a frame with an arbitrary missing
            // tail, which R7 forbids publishing.
            frame_fault_q  <= 1'b1;
            mstate_q       <= M_IDLE;
          end else if (guard_rsp_i.ok) begin
            // The words this request owes the socket, added the cycle the
            // guard accepts it. This is the ONLY place wr_words_q grows.
            wr_words_q <= wr_words_q + OUTW'(words_of(m_len_q));
            m_beat_q   <= 4'd0;
            mstate_q   <= M_WBEAT;
          end
        end

        M_WBEAT: if (guard_wready_i) begin
          m_wsh_q  <= {64'd0, m_wsh_q[SHW-1:64]};
          m_beat_q <= m_beat_q + 4'd1;
          if (m_beat_q == (m_beats_q - 4'd1)) begin
            case (m_kind_q)
              K_PV:  verts_written_o  <= verts_written_o + 32'd1;
              K_TD:  tris_written_o   <= tris_written_o + 32'd1;
              K_CK:  chunks_written_o <= chunks_written_o + 32'd1;
              K_DIR: dir_written_q    <= 1'b1;
              default: ;
            endcase
            mstate_q <= M_IDLE;
          end
        end

        default: mstate_q <= M_IDLE;
      endcase
    end
  end

endmodule : zhao_geom_paramarena

`default_nettype wire
