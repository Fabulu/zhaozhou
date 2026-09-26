// zhao_geom_arenabin.sv -- GEOM.ARENABIN: the INDEPENDENT chunk producer.
//
// Console entry I55, and owner vacation directive 2026-09-23 section 4:
//
//   "I55 requires the walker to feed the live raster path from SDRAM. A
//    parallel legacy on-chip frame arena that still supplies the actual pixels
//    is not closure."
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK IS FOR, IN ONE PARAGRAPH
// ---------------------------------------------------------------------------
// `zhao_geom_chunkser` fills the external arena's chunk region, and its ONLY
// input is `zhao_geom_binner_v2`'s serialise pass -- a second read walk over
// that block's `tile_ram` / `ref_ram` / `next_ram`. WALKSWAP measured the
// consequence and entry I55 records it: the on-chip arena is what FILLS the
// external one, the two are in SERIES, and the external path therefore cannot
// become the sole producer by subtraction. Its closing paragraph names the
// shape that would close it:
//
//   "a producer that fills the external arena WITHOUT the on-chip one -- tile
//    binning performed against arena triangle ids directly -- after which the
//    walk has an independent source."
//
// THIS IS THAT BLOCK. It bins the post-clip triangle stream itself, against
// the ARENA's TriangleDescriptor indices, and writes 64-byte chunk records
// straight into `zhao_geom_paramarena`. It reads no binner RAM, it holds no
// frame-sized reference array, and `zhao_geom_binner_v2` is not in its
// closure -- `tests/geometry/geom_arenabin.sources.txt` is the evidence, and
// `geom_arenabin_directed` runs the whole producer-to-walk round trip with no
// binner compiled in at all.
//
// ---------------------------------------------------------------------------
// WHERE THE TRANSPOSE LIVES, WHICH IS THE WHOLE ARCHITECTURE
// ---------------------------------------------------------------------------
// Binning is a TRANSPOSE. Triangles arrive in submission order; a tile list is
// wanted in tile-major order. Something has to buffer the difference and the
// only real choice is WHERE. Directive section 4 states the preference:
//
//   "The architect chooses a bounded implementation and may move backing state
//    to SDRAM rather than growing a frame-sized FPGA register file."
//
// `zhao_geom_binner_v2` holds the whole transpose on chip. At the shipped
// RENDER_CHUNKS = 8192 / CHUNK_REFS = 4 (GIANTREFS, R7's 32,768 references)
// that is
//
//     ref_ram   32768 x  7b = 229,376 bits
//     next_ram   8192 x 13b = 106,496 bits
//     tile_ram    576 x 27b =  15,552 bits      (CNT_W 16 + 2x CHUNK_W 13 = 42
//                                                at the shipped parameters:
//                                                576 x 42 = 24,192 bits)
//                             ------------------
//                             ~360,000 bits of on-chip transpose.
//
// THIS BLOCK HOLDS ONE PARTIAL CHUNK PER TILE AND A PER-TILE DIRECTORY:
//
//     stage_ram  STAGE_IDS x TILES x ID_W = 14 x 576 x 18 = 145,152 bits
//     head_ram   576 x CHIDX_W 18                         =  10,368 bits
//     tail_ram   576 x CHIDX_W 18                         =  10,368 bits
//     hv_ram     576 x 1                                  =     576 bits
//     fill_ram   576 x STG_W 4                            =   2,304 bits
//     nch_ram    576 x CHW 16                             =   9,216 bits
//                                                           -------------
//                                                             177,984 bits
//
// THAT TOTAL WAS 168,768 IN THE FIRST VERSION OF THIS HEADER AND IT WAS WRONG
// BY `nch_ram`, WHICH I ADDED AN HOUR LATER. Recorded rather than quietly
// corrected, because it went wrong in the direction this repo's own law says
// nobody audits: a block that looks CHEAPER than it is. The per-tile chunk
// counter exists so `max_tile_chunks_o` measures a tile's list depth instead
// of a frame total, and it is 9,216 bits that have to be paid for.
//
// -- and every FULL chunk leaves for SDRAM the moment its fourteenth id lands.
// The bound is per-TILE and constant, not per-frame: a giant that covers every
// tile costs exactly the same on-chip state as a single triangle. R7's "keep
// only the bounded directories/caches/FIFOs" is satisfied by construction
// rather than by a capacity argument.
//
// THE REFERENCE CAPACITY IS THEREFORE THE ARENA'S SEALED CHUNK QUOTA, not a
// local parameter. There is no `REF_CAP` here to overflow: a frame that needs
// more chunks than the seal allows is refused by `zhao_geom_paramarena` on its
// own quota, which faults the frame, attributes it and repeats the prior
// complete frame. One bound, in one place, enforced by the block that owns the
// memory. `chunk_refused_o` counts it here so the fault has a source on this
// side too.
//
// ---------------------------------------------------------------------------
// THE CHAIN, AND WHY THIS BLOCK CANNOT USE CHUNKSER'S TRICK
// ---------------------------------------------------------------------------
// `zhao_geom_chunkser.sv`'s header states its own assumption precisely:
//
//   "any other chunk is emitted with `next` = `ck_alloc_id_i + 1`, which is
//    the index the arena will hand the chunk this block offers NEXT, because
//    this block is the arena's only chunk producer AND OFFERS A TILE'S CHUNKS
//    IN ORDER."
//
// That last clause is true of a TILE-MAJOR producer and false of this one. A
// serialiser walks tile 0 to completion, then tile 1; its successor chunk is
// always the very next allocation. This block bins in SUBMISSION order, so
// between tile T's third chunk and its fourth there may be two hundred chunks
// belonging to other tiles. `ck_alloc_id_i + 1` would name one of THEM.
//
// Three ways out were considered and two were rejected with reasons:
//
//   (a) HOLD THE TILE'S CHUNKS ON CHIP UNTIL ITS SUCCESSOR IS KNOWN. That is
//       the frame-sized reference array again, wearing a different name, and
//       it is exactly the state the directive says to move to SDRAM.
//   (b) BUILD THE CHAIN BACKWARD -- each new chunk points at the tile's
//       previous one, head = newest. Correct as a data structure and WRONG as
//       a rendering order: `zhao_geom_binner_v2` keeps a TAIL pointer
//       precisely so a tile list is FIFO, because the painter's algorithm
//       requires frame-wide submission order within a tile (:52-62). A
//       backward chain delivers the tile's triangles newest-first and the
//       picture is wrong in a way no counter reads.
//   (c) PATCH THE PREDECESSOR. A chunk is written with `next` = the all-ones
//       sentinel, so it is a VALID TERMINAL CHUNK from the instant it lands.
//       When that tile's successor is allocated, its predecessor's first eight
//       bytes are rewritten with the real link. This is what this block does.
//
// THE PRICE OF (c) IS DECLARED AND IS SMALL: one extra 8-byte write per chunk
// that is not the last of its tile -- 8 bytes against the chunk's own 64, so
// at most +12.5% on the chunk region's write traffic and nothing at all on the
// vertex or descriptor regions. It is counted at `links_patched_o`.
//
// THE PATCH IS NOT A SECOND WRITER. It goes through `zhao_geom_paramarena`'s
// `lk_*` intake, is issued by that block's one write engine, from that block's
// one address register, retires through the same `wr_words_q` gate, and the
// generation in bytes 6..7 is re-stamped by the arena from `gen_q` exactly as
// the original write was -- the caller still cannot supply a generation, which
// is the law that file states in its own header and it is preserved here.
//
// A PATCHED CHUNK IS ALWAYS FULL. A tile's chunks are all `STAGE_IDS` long
// except the last, and the last is created only by the end-of-frame flush and
// therefore never acquires a successor. `lk_count_o` is nevertheless carried
// explicitly rather than assumed, because "the count is always fourteen" is a
// property of the flush order and not of the record.
//
// ---------------------------------------------------------------------------
// IDENTITY -- DIRECTIVE SECTION 4, AND THERE IS NOTHING TO EVICT
// ---------------------------------------------------------------------------
// The directive requires the mapping's LIFETIME stated and eviction proved
// unable to change a still-referenced identity. Both are trivial here, and
// they are trivial ON PURPOSE:
//
//   * WHAT IS STORED IS THE IDENTITY ITSELF. `tri_arena_id_i` is the arena's
//     own TriangleDescriptor index -- `zhao_geom_paramarena.td_id_o`, carried
//     through `zhao_geom_vertid.tri_id_o` and `zhao_geom_tidq`. It is copied
//     into `stage_ram` verbatim at its full ID_W width and copied out into the
//     chunk record verbatim. There is no hash, no CRC, no compression, no
//     local renumbering and NO SECOND NAMESPACE. A binner slot 0..127 cannot
//     reach this block because this block has no slots.
//   * THERE IS NO CACHE, SO THERE IS NO EVICTION. `stage_ram` is indexed by
//     TILE, not by identity; it is a per-tile accumulator, not an associative
//     store. Nothing is ever looked up, so nothing can miss, so nothing can be
//     evicted and reallocated under a live reference.
//   * THE LIFETIME IS ONE FRAME, bounded by two explicit edges. `frame_start_i`
//     (the arena's own `seal_fire_o`) clears `fill_ram` and `hv_ram`;
//     `geom_done_i` flushes every partial chunk and pulses `bin_frame_done_o`,
//     which is what ends the arena's frame. No entry survives a seal and no
//     chunk index is reused, because chunk indices come from the arena's
//     monotonic per-frame cursor and the arena refuses reuse while a prior
//     frame or reader owns the allocation.
//   * A TRIANGLE WITH NO ARENA IDENTITY IS NOT BINNED. `tri_id_ok_i` says the
//     arena ACCEPTED the descriptor. A triangle the arena refused has no index,
//     so a reference to it would name someone else's descriptor -- the exact
//     "wrong descriptors that decode cleanly" failure entry I54 names. It is
//     dropped and counted at `tris_unnamed_o` instead of being given a
//     convenient zero.
//
// ---------------------------------------------------------------------------
// THE COVERAGE LAW IS THE RASTERISER'S OWN, NOT A SECOND OPINION
// ---------------------------------------------------------------------------
// The tile trivial-reject below is `zhao_raster_fill` -- the SAME module
// `zhao_raster_edgewalk` instantiates and `tests/formal/raster_edgewalk_top_left
// .sby` proves -- fed with the same `E' = floor(E0/256)` decomposition,
// the same `rnz` constant bit, the same 15k max-corner offset and the same
// clamped `tile_of` as `zhao_geom_binner_v2`. That is deliberate: this block
// and the binner must reference the SAME set of (triangle, tile) pairs, or the
// external arena describes a different picture from the one the raster draws,
// and nothing downstream would say so.
//
// IT IS NOT ARGUED, IT IS MEASURED. `geom_arenabin_equiv` runs this block and
// `zhao_geom_binner_v2` on one triangle stream and compares the reference sets
// pair for pair. That test is the reason the arithmetic below is a copy rather
// than a shared submodule: the binner is a shared hot file in a live campaign,
// and a differential against the shipped block is stronger evidence than a
// refactor that makes both sides the same code and can therefore agree while
// both are wrong.
//
// ---------------------------------------------------------------------------
// THIS BLOCK IS IN THE GEOMETRY STREAM'S BACKPRESSURE PATH, AND THAT IS
// DECLARED RATHER THAN HIDDEN
// ---------------------------------------------------------------------------
// `zhao_geom_chunkser` is explicitly NOT in the raster path -- it reads the
// binner's lists after the drain, and can stall as hard as it likes. This
// block cannot: it bins the live stream, so while it is writing a chunk to
// SDRAM it holds `tri_ready_o` low and the geometry front end waits. That is
// the real cost of making the external arena the live producer and it is the
// number entry I55 asks to be reported, so it is INSTRUMENTED rather than
// smoothed over: `intake_stall_o` counts every clock this block offers no
// intake while a triangle is waiting. A chunk-output FIFO would hide that
// number behind a depth; it is not added here on purpose, because the first
// thing this entry needs is the measurement.
//
// Conservative SystemVerilog subset (charter section 2). Elaboration guards
// live inside `initial begin ... end` and every generate is explicit, because
// Quartus 17 rejects the bare module-scope forms that Verilator lints clean.
`default_nettype none

module zhao_geom_arenabin #(
    // The tile grid. Same shape and same defaults as `zhao_geom_binner_v2`,
    // because the two must reference the same tiles.
    parameter int unsigned GRID_W = 24,
    parameter int unsigned GRID_H = 24,
    parameter int unsigned TILES  = GRID_W * GRID_H,
    parameter int unsigned TIDX_W = 10,

    // R7's chunk holds fourteen ids. A knob, not a constant, because the
    // owner's control over every shipped value is a standing rule -- lowering
    // it trades on-chip staging for chunk count and SDRAM writes, and both
    // sides of that trade are counted below.
    parameter int unsigned STAGE_IDS = 14,

    // The ARENA's TriangleDescriptor index width (`td_id_o`) and chunk index
    // width (`ck_alloc_id_o`). Not local names for local things.
    parameter int unsigned ID_W    = 18,
    parameter int unsigned CHIDX_W = 18,

    // The edge accumulator width, `zhao_geom_binner_v2`'s ACC_W.
    parameter int unsigned ACC_W = 36
) (
    input  var logic clk,
    input  var logic rst_n,

    // ------------------------------------------------------- frame edges --
    // `frame_start_i` is the arena's `seal_fire_o`: the instant a frame's
    // quota and generation exist. `geom_done_i` is the console's real frame
    // end. Between them this block owns one frame's chunk region.
    input  var logic frame_start_i,
    input  var logic geom_done_i,

    // The live grid, same ports as the binner's.
    input  var logic [5:0] grid_w_i,
    input  var logic [5:0] grid_h_i,

    // ------------------------------------------------- the triangle intake --
    // The same post-clip geometry `zhao_geom_binner_v2` is fed, from
    // GEOM.SETUP, PLUS the arena identity from `zhao_geom_tidq`. No positions
    // are read back from SDRAM: the producer sees the triangle at the same
    // seam the binner does, which is what makes the two reference sets
    // comparable at all.
    input  var logic                     tri_valid_i,
    output var logic                     tri_ready_o,
    input  var logic signed [22:0]       tri_kx0_i,
    input  var logic signed [22:0]       tri_ky0_i,
    input  var logic signed [47:0]       tri_kc0_i,
    input  var logic signed [22:0]       tri_kx1_i,
    input  var logic signed [22:0]       tri_ky1_i,
    input  var logic signed [47:0]       tri_kc1_i,
    input  var logic signed [22:0]       tri_kx2_i,
    input  var logic signed [22:0]       tri_ky2_i,
    input  var logic signed [47:0]       tri_kc2_i,
    input  var logic [2:0]               tri_tl_i,
    input  var logic signed [11:0]       tri_min_x_i,
    input  var logic signed [11:0]       tri_max_x_i,
    input  var logic signed [11:0]       tri_min_y_i,
    input  var logic signed [11:0]       tri_max_y_i,
    // THE ARENA'S OWN DESCRIPTOR INDEX, and whether the arena took it.
    input  var logic [ID_W-1:0]          tri_arena_id_i,
    input  var logic                     tri_id_ok_i,

    // ------------------------------------------- the arena's chunk intake --
    output var logic                     ck_valid_o,
    input  var logic                     ck_ready_i,
    output var logic [31:0]              ck_next_o,
    output var logic [15:0]              ck_count_o,
    output var logic [STAGE_IDS*32-1:0]  ck_ids_o,
    input  var logic [CHIDX_W-1:0]       ck_alloc_id_i,
    input  var logic                     ck_accept_i,

    // -------------------------------------------- the arena's link intake --
    // The chain patch. See "THE CHAIN" above: this rewrites bytes 0..7 of an
    // ALREADY WRITTEN chunk so a terminal chunk becomes an interior one.
    output var logic                     lk_valid_o,
    input  var logic                     lk_ready_i,
    output var logic [CHIDX_W-1:0]       lk_index_o,
    output var logic [31:0]              lk_next_o,
    output var logic [15:0]              lk_count_o,

    // ------------------------------------------------------ the head table --
    // The walker's `walk_head_i` comes from here: tile -> first chunk index,
    // or the all-ones sentinel when the tile holds nothing.
    input  var logic [TIDX_W-1:0]        head_tile_i,
    output var logic [31:0]              head_chunk_o,
    output var logic                     head_valid_o,

    // ------------------------------------------------------------ the done --
    // ONE-CYCLE PULSE, after the last partial chunk has been offered. This is
    // what ends the arena's frame, exactly as `ser_frame_done_o` does today.
    output var logic                     bin_frame_done_o,

    // ----------------------------------------------------------- evidence --
    output var logic [31:0] tris_binned_o,
    output var logic [31:0] tris_unnamed_o,
    output var logic [31:0] refs_binned_o,
    output var logic [31:0] chunks_emitted_o,
    output var logic [31:0] links_patched_o,
    output var logic [31:0] tiles_with_refs_o,
    output var logic [31:0] chunk_refused_o,
    output var logic [31:0] intake_stall_o,
    output var logic [31:0] flush_cut_o,
    output var logic [15:0] max_tile_chunks_o,
    output var logic        overflow_o,
    output var logic        busy_o
);

  // ------------------------------------------------------------ localparams --
  localparam int unsigned STG_W   = $clog2(STAGE_IDS + 1);
  // The per-tile chunk-count instrument's width. Sixteen bits, and the compare
  // below saturates rather than wrapping.
  localparam int unsigned CHW     = 16;
  localparam logic signed [ACC_W-1:0] ACC_ZERO = {ACC_W{1'b0}};
  localparam logic [31:0] CK_NULL = 32'hFFFF_FFFF;

  // The all-ones id written into a chunk slot the producer did not fill.
  // DELIBERATELY an id `zhao_geom_parambuf` refuses rather than a legal zero,
  // which is the same choice `zhao_geom_chunkser` makes and for the same
  // reason: a convenient zero is a valid descriptor index.
  localparam logic [31:0] ID_NULL = 32'hFFFF_FFFF;

  // ------------------------------------------------- elaboration guards ----
  // Inside `initial begin ... end`: Quartus 17 rejects the bare module-scope
  // `if (...) $fatal(...)` that Verilator lints clean, and `--lint-only` does
  // not run `initial` blocks, so a clean lint says nothing about these.
  initial begin
    if (TILES != GRID_W * GRID_H)
      $fatal(1, "zhao_geom_arenabin: TILES must be GRID_W*GRID_H");
    if (TIDX_W < $clog2(TILES))
      $fatal(1, "zhao_geom_arenabin: TIDX_W too narrow for TILES");
    if (STAGE_IDS < 2)
      $fatal(1, "zhao_geom_arenabin: STAGE_IDS must be at least 2");
    if (STAGE_IDS > 14)
      $fatal(1, "zhao_geom_arenabin: STAGE_IDS above R7's fourteen-id chunk");
    if (ID_W > 32)
      $fatal(1, "zhao_geom_arenabin: ID_W wider than the chunk's u32 id slot");
    if (CHIDX_W > 32)
      $fatal(1, "zhao_geom_arenabin: CHIDX_W wider than the chunk's u32 next");
    if (ACC_W < 36)
      $fatal(1, "zhao_geom_arenabin: ACC_W below the binner's 36-bit domain");
    if (GRID_W > 63 || GRID_H > 63)
      $fatal(1, "zhao_geom_arenabin: grid beyond the 6-bit tile coordinate");
  end

  // ------------------------------------------------------------- the state --
  typedef enum logic [3:0] {
    A_CLEAR, A_IDLE, A_SETUP1, A_SETUP2, A_TILE, A_PUSH,
    A_EMITR, A_EMIT, A_LINK, A_DIR, A_FLUSH, A_FLUSHD, A_DONE
  } astate_e;
  astate_e st_q;

  logic frame_open_q;
  logic done_pend_q;
  logic wall_q;               // the frame is faulted; bin nothing more
  logic flush_q;              // the emit path is serving the end-of-frame sweep

  // ---------------------------------------------------------- the directory --
  // FOUR NARROW ARRAYS RATHER THAN ONE WIDE ONE. `head_ram` needs a second
  // read port for `head_tile_i` and the others do not, and splitting them
  // keeps that second port off 41 bits of state that nobody queries.
  logic [CHIDX_W-1:0] head_ram [0:TILES-1];
  logic [CHIDX_W-1:0] tail_ram [0:TILES-1];
  logic               hv_ram   [0:TILES-1];
  logic [STG_W-1:0]   fill_ram [0:TILES-1];

  // The PER-TILE CHUNK COUNT, so `max_tile_chunks_o` measures a tile's list
  // depth rather than a frame total. It is read and written on the SAME
  // directory accesses as head/tail/fill, so the high-water compare below
  // differences a value that came OUT of the array against that array's own
  // update -- not a parallel counter that moves with it.
  logic [CHW-1:0]     nch_ram  [0:TILES-1];

  logic [CHIDX_W-1:0] tail_rd_q;
  logic               hv_rd_q;
  logic [STG_W-1:0]   fill_rd_q;
  logic [CHW-1:0]     nch_rd_q;

  // ---- the staging banks -------------------------------------------------
  // ONE RAM PER CHUNK SLOT. Appending a reference is then a single narrow
  // write to ONE bank at the tile's address; one STAGE_IDS*ID_W-wide RAM would
  // need a read-modify-write of all 252 bits per reference. A_EMITR reads all
  // STAGE_IDS slots of a tile in ONE clock, which is why there are fourteen
  // one-read-port banks and not one array.
  //
  // THE STAGING DOES NOT REACH BLOCK MEMORY ON QUARTUS 17.0.2, AND THAT IS A
  // MEASUREMENT RATHER THAN A SUSPICION. ARENACOMPOSE, 2026-09-26, put this
  // block through `quartus_map` for the first time -- R212: BINARENA counted
  // these bits from the DECLARATIONS and was forbidden a fit. Three map_only
  // rows on the shipping part 5CSEBA6U23I7, all three IDENTICAL:
  //
  //   `@arenacompose`      as written                146,414 reg   33,408 bits
  //   `@ramstyle`          (* ramstyle = `MACRO *)   146,414 reg   33,408 bits
  //   `@ramstyle-literal`  (* ramstyle = "M10K" *)   146,414 reg   33,408 bits
  //
  // WHAT INFERRED: only the five MODULE-SCOPE directory arrays -- head_ram,
  // tail_ram, hv_ram, fill_ram, nch_ram, 33,408 bits between them, named in
  // the map report's own RAM Summary. The 14 x 576 x 18 = 145,152-bit STAGING
  // array did not infer at all, in any of the three, and went to flip-flops.
  // A 5CSEBA6U23I7 holds about 167,640 of those, so this ONE BLOCK asks for
  // roughly 87% of the device's registers.
  //
  // THE ATTRIBUTE IS THEREFORE NOT HERE. It was tried twice, it changed
  // nothing either time, and Quartus said nothing either time. An INERT
  // synthesis directive left in shipped RTL is worse than none: it reads as a
  // guarantee that the storage is in memory, and the next person to look at
  // this file would inherit the guarantee and not the measurement.
  //
  // THE DECLARATIONS ARE IDENTICAL IN STYLE to the five that DID infer; the
  // one difference is that these are declared INSIDE A GENERATE. The header
  // used to assert the opposite -- "declared inside an explicit generate
  // rather than as a 2-D unpacked array, which Quartus 17 does not reliably
  // infer as memory" -- and that is the sentence the fit refuted. Hoisting the
  // banks to module scope is the next experiment and it is NOT done here: it
  // is a change to this block's storage architecture, and ARENACOMPOSE's job
  // was to compose the block and measure it.
  //
  // FOURTEEN ONE-READ-PORT BANKS ARE ARCHITECTURALLY REQUIRED whatever the
  // storage: A_EMITR reads all STAGE_IDS slots of a tile in ONE clock. At
  // 576 x 18 each, M10K would cost 2 blocks per bank -- 28 of the device's 553
  // -- so the memory is affordable if it can be reached. THE LIMIT IS THE
  // INFERENCE, NOT THE CAPACITY.
  logic              stg_we;
  logic [STG_W-1:0]  stg_wsel;
  logic [TIDX_W-1:0] stg_wa;
  logic [ID_W-1:0]   stg_wd;
  logic              stg_re;
  logic [TIDX_W-1:0] stg_ra;

  // The head table's own read port.
  logic [CHIDX_W-1:0] hq_chunk_q;
  logic               hq_valid_q;
  assign head_chunk_o = hq_valid_q ? {{(32-CHIDX_W){1'b0}}, hq_chunk_q} : CK_NULL;
  assign head_valid_o = hq_valid_q;

  // --------------------------------------------------------- the triangle ---
  logic signed [ACC_W-1:0] kx_r [0:2];
  logic signed [ACC_W-1:0] ky_r [0:2];
  logic signed [47:0]      kc_r [0:2];
  logic [2:0]              tl_r;
  logic signed [ACC_W-1:0] ep_r  [0:2];
  logic signed [ACC_W-1:0] epr_r [0:2];
  logic signed [ACC_W-1:0] off_r [0:2];
  logic [2:0]              rnz_r;
  logic [5:0]              tx0_r, tx1_r, ty0_r, ty1_r, tx_r, ty_r;
  logic [TIDX_W-1:0]       row_base_r;
  logic [ID_W-1:0]         id_r;

  // the tile currently being serviced, and the chunk being emitted
  logic [TIDX_W-1:0]  tile_r;
  logic [STG_W-1:0]   cnt_r;
  logic [CHIDX_W-1:0] alloc_q;
  logic [TIDX_W-1:0]  sweep_r;      // A_CLEAR / A_FLUSH cursor

  // --------------------------------------------------------------- helpers --
  function automatic logic signed [ACC_W-1:0] ext23(input logic signed [22:0] v);
    ext23 = $signed({{(ACC_W-23){v[22]}}, v});
  endfunction

  // The slope times the tile offset, multiplied AT ITS REAL 23-BIT WIDTH.
  // Copied deliberately from `zhao_geom_binner_v2`, whose header records the
  // measurement: 23x11 is one DSP where 36x11 is three, and the value is
  // identical because the accumulator's high bits are sign extension by
  // construction.
  function automatic logic signed [ACC_W-1:0] k_mul_tile(
      input logic signed [22:0] k,
      input logic signed [10:0] t);
    logic signed [33:0] p;
    p = k * t;
    k_mul_tile = $signed({{(ACC_W-34){p[33]}}, p});
  endfunction

  // floor(p/16), clamped into [0, limit-1]. Loses tiles rather than aliasing
  // one tile's list onto another's, which is the binner's LAWS CHOSEN B/D.
  function automatic logic [5:0] tile_of(input logic signed [11:0] p,
                                         input logic        [5:0]  limit);
    logic signed [11:0] q;
    logic signed [11:0] hi;
    begin
      q  = p >>> 4;
      hi = $signed({6'd0, limit}) - 12'sd1;
      if (limit == 6'd0)   tile_of = 6'd0;
      else if (q[11])      tile_of = 6'd0;
      else if (q > hi)     tile_of = limit - 6'd1;
      else                 tile_of = q[5:0];
    end
  endfunction

  function automatic logic signed [ACC_W-1:0] mul15(input logic signed [ACC_W-1:0] k);
    mul15 = (k <<< 4) - k;
  endfunction

  function automatic logic signed [47:0] e0_base(input logic [1:0] e);
    logic signed [47:0] kxw;
    logic signed [47:0] kyw;
    begin
      kxw = $signed({{(48-ACC_W){kx_r[e][ACC_W-1]}}, kx_r[e]});
      kyw = $signed({{(48-ACC_W){ky_r[e][ACC_W-1]}}, ky_r[e]});
      e0_base = (kxw <<< 7) + (kyw <<< 7) + kc_r[e];
    end
  endfunction

  function automatic logic signed [ACC_W-1:0] ep_of(input logic signed [47:0] e0);
    ep_of = ACC_W'(e0 >>> 8);
  endfunction

  function automatic logic [TIDX_W-1:0] tidx(input logic [5:0] ty);
    tidx = TIDX_W'(ty) * TIDX_W'(GRID_W);
  endfunction

  // ------------------------------------------------------ the tile predicate --
  // `zhao_raster_fill` -- the shipped module, not a copy of its rule.
  logic signed [ACC_W-1:0] emax [0:2];
  logic [2:0] tile_accept;

  genvar ge;
  generate
    for (ge = 0; ge < 3; ge = ge + 1) begin : g_edge
      assign emax[ge] = ep_r[ge] + off_r[ge];
      zhao_raster_fill #(.W(ACC_W)) u_fill (
        .e_i     (emax[ge]),
        .rnz_i   (rnz_r[ge]),
        .tl_i    (tl_r[ge]),
        .accept_o(tile_accept[ge])
      );
    end
  endgenerate

  wire tile_keep_c = (tile_accept == 3'b111);
  wire [TIDX_W-1:0] tile_c = row_base_r + TIDX_W'(tx_r);

  // ------------------------------------------------------------ the ports ---
  // Each staging bank owns its own u32 slot of the chunk record. Slots past
  // the chunk's count carry ID_NULL -- an id `zhao_geom_parambuf` refuses --
  // rather than a legal-looking zero.
  genvar gs;
  generate
    for (gs = 0; gs < STAGE_IDS; gs = gs + 1) begin : g_stage
      logic [ID_W-1:0] bank [0:TILES-1];
      logic [ID_W-1:0] rd_q;
      always_ff @(posedge clk) begin
        if (stg_we && (stg_wsel == STG_W'(gs))) bank[stg_wa] <= stg_wd;
        if (stg_re) rd_q <= bank[stg_ra];
      end
      assign ck_ids_o[gs*32 +: 32] = (STG_W'(gs) < cnt_r)
                                   ? {{(32-ID_W){1'b0}}, rd_q}
                                   : ID_NULL;
    end
  endgenerate

  // The staging port drives. A_PUSH appends one id; A_EMITR reads the whole
  // row one cycle before A_EMIT offers it.
  assign stg_we   = (st_q == A_PUSH);
  assign stg_wsel = fill_rd_q;
  assign stg_wa   = tile_r;
  assign stg_wd   = id_r;
  assign stg_re   = (st_q == A_EMITR);
  assign stg_ra   = tile_r;

  assign ck_valid_o = (st_q == A_EMIT);
  assign ck_count_o = {{(16-STG_W){1'b0}}, cnt_r};
  // EVERY CHUNK IS BORN TERMINAL. The link arrives later, as a patch, if a
  // successor is ever allocated for this tile -- see "THE CHAIN" above.
  assign ck_next_o  = CK_NULL;

  assign lk_valid_o = (st_q == A_LINK);
  assign lk_index_o = tail_rd_q;
  assign lk_next_o  = {{(32-CHIDX_W){1'b0}}, alloc_q};
  assign lk_count_o = {{(16-STG_W){1'b0}}, STG_W'(STAGE_IDS)};

  assign busy_o = (st_q != A_IDLE) || frame_open_q || done_pend_q;

  // Intake is offered only from A_IDLE, with a frame open and no wall. A
  // triangle waiting while this block is elsewhere is the backpressure this
  // architecture pays for, and it is counted rather than hidden.
  // `!done_pend_q` MATTERS AND IS NOT DEFENSIVE. Without it a triangle can be
  // accepted in the very cycle A_IDLE hands over to A_FLUSH, and it is then
  // simply gone -- binned by nobody, referenced by nothing, and invisible,
  // because a reference that was never made cannot be missed by any counter.
  wire intake_open_c = (st_q == A_IDLE) && frame_open_q && !wall_q && !done_pend_q;
  // NOT A DEADLOCK WHEN THE FRAME IS SHUT. With no frame open, or with the
  // frame already walled, records are consumed at full rate and dropped, for
  // the reason `zhao_geom_paramarena`'s intake gate states at length: this
  // block sits on a live production stream and a permanently low `ready`
  // would stall the whole geometry path, which looks exactly like a defect
  // somewhere else.
  wire sink_c = (st_q == A_IDLE) && (!frame_open_q || wall_q);
  assign tri_ready_o = intake_open_c || sink_c;

  wire tri_fire_c = tri_valid_i && tri_ready_o;

  // ---------------------------------------------------------------- core ----
  integer k;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_q              <= A_IDLE;
      frame_open_q      <= 1'b0;
      done_pend_q       <= 1'b0;
      wall_q            <= 1'b0;
      flush_q           <= 1'b0;
      sweep_r           <= '0;
      tile_r            <= '0;
      cnt_r             <= '0;
      alloc_q           <= '0;
      id_r              <= '0;
      tx0_r             <= 6'd0;
      tx1_r             <= 6'd0;
      ty0_r             <= 6'd0;
      ty1_r             <= 6'd0;
      tx_r              <= 6'd0;
      ty_r              <= 6'd0;
      row_base_r        <= '0;
      tl_r              <= 3'd0;
      rnz_r             <= 3'd0;
      tail_rd_q         <= '0;
      hv_rd_q           <= 1'b0;
      fill_rd_q         <= '0;
      hq_chunk_q        <= '0;
      hq_valid_q        <= 1'b0;
      bin_frame_done_o  <= 1'b0;
      tris_binned_o     <= 32'd0;
      tris_unnamed_o    <= 32'd0;
      refs_binned_o     <= 32'd0;
      chunks_emitted_o  <= 32'd0;
      links_patched_o   <= 32'd0;
      tiles_with_refs_o <= 32'd0;
      chunk_refused_o   <= 32'd0;
      intake_stall_o    <= 32'd0;
      flush_cut_o       <= 32'd0;
      max_tile_chunks_o <= 16'd0;
      overflow_o        <= 1'b0;
      for (k = 0; k < 3; k = k + 1) begin
        kx_r[k]  <= '0;
        ky_r[k]  <= '0;
        kc_r[k]  <= '0;
        ep_r[k]  <= '0;
        epr_r[k] <= '0;
        off_r[k] <= '0;
      end
      nch_rd_q          <= '0;
    end else begin
      bin_frame_done_o <= 1'b0;

      // ---- the head table's read port -----------------------------------
      // Registered, one cycle, independent of the binning FSM. `hv_ram` is
      // the same array the FSM writes, so a head query DURING binning sees
      // the frame as it stands -- which is why the console must only walk a
      // PUBLISHED frame, and `zhao_geom_paramwalk` gates on `pub_valid_i`.
      hq_chunk_q <= head_ram[head_tile_i];
      hq_valid_q <= hv_ram[head_tile_i];

      // ---- the backpressure instrument ------------------------------------
      if (tri_valid_i && !tri_ready_o)
        intake_stall_o <= intake_stall_o + 32'd1;

      case (st_q)
        // -----------------------------------------------------------------
        // Only `fill_ram` and `hv_ram` need clearing: with `hv` low a tile's
        // head and tail describe nothing, and with `fill` zero its staging
        // holds nothing. Clearing the 145 Kbit staging array would be 576
        // cycles of writing values no reader can reach.
        A_CLEAR: begin
          fill_ram[sweep_r] <= '0;
          hv_ram[sweep_r]   <= 1'b0;
          nch_ram[sweep_r]  <= '0;
          if (sweep_r == TIDX_W'(TILES - 1)) begin
            sweep_r <= '0;
            st_q    <= A_IDLE;
          end else begin
            sweep_r <= sweep_r + TIDX_W'(1);
          end
        end

        // -----------------------------------------------------------------
        A_IDLE: begin
          if (done_pend_q && frame_open_q) begin
            // The frame has ended: flush every partial chunk, tile by tile.
            flush_q <= 1'b1;
            sweep_r <= '0;
            st_q    <= A_FLUSH;
          end else if (tri_fire_c) begin
            if (!frame_open_q || wall_q) begin
              // consumed and dropped -- see `sink_c`
            end else if (!tri_id_ok_i) begin
              // NO ARENA IDENTITY, NO REFERENCE. Counted, never binned.
              tris_unnamed_o <= tris_unnamed_o + 32'd1;
            end else begin
              tris_binned_o <= tris_binned_o + 32'd1;
              id_r    <= tri_arena_id_i;
              kx_r[0] <= ext23(tri_kx0_i);
              ky_r[0] <= ext23(tri_ky0_i);
              kx_r[1] <= ext23(tri_kx1_i);
              ky_r[1] <= ext23(tri_ky1_i);
              kx_r[2] <= ext23(tri_kx2_i);
              ky_r[2] <= ext23(tri_ky2_i);
              kc_r[0] <= tri_kc0_i;
              kc_r[1] <= tri_kc1_i;
              kc_r[2] <= tri_kc2_i;
              tl_r    <= tri_tl_i;
              tx0_r   <= tile_of(tri_min_x_i, grid_w_i);
              tx1_r   <= tile_of(tri_max_x_i, grid_w_i);
              ty0_r   <= tile_of(tri_min_y_i, grid_h_i);
              ty1_r   <= tile_of(tri_max_y_i, grid_h_i);
              st_q    <= A_SETUP1;
            end
          end
        end

        // -----------------------------------------------------------------
        A_SETUP1: begin
          for (k = 0; k < 3; k = k + 1) begin
            // Quartus 17 rejects indexing a function call's return value
            // directly; bind it to a temporary first. The binner's own
            // comment records that this was found by synthesis, not by
            // simulation.
            automatic logic signed [47:0] e0k = e0_base(2'(k));
            rnz_r[k] <= (e0k[7:0] != 8'd0);
            ep_r[k]  <= ep_of(e0k);
            off_r[k] <= ((kx_r[k] > ACC_ZERO) ? mul15(kx_r[k]) : ACC_ZERO) +
                        ((ky_r[k] > ACC_ZERO) ? mul15(ky_r[k]) : ACC_ZERO);
          end
          tx_r <= tx0_r;
          ty_r <= ty0_r;
          st_q <= A_SETUP2;
        end

        // -----------------------------------------------------------------
        A_SETUP2: begin
          for (k = 0; k < 3; k = k + 1) begin
            ep_r[k]  <= ep_r[k]
                      + k_mul_tile($signed(kx_r[k][22:0]), $signed({1'b0, tx0_r, 4'd0}))
                      + k_mul_tile($signed(ky_r[k][22:0]), $signed({1'b0, ty0_r, 4'd0}));
            epr_r[k] <= ep_r[k]
                      + k_mul_tile($signed(kx_r[k][22:0]), $signed({1'b0, tx0_r, 4'd0}))
                      + k_mul_tile($signed(ky_r[k][22:0]), $signed({1'b0, ty0_r, 4'd0}));
          end
          row_base_r <= tidx(ty0_r);
          st_q       <= A_TILE;
        end

        // -----------------------------------------------------------------
        // One cycle per candidate tile. The directory read is ISSUED here and
        // consumed in A_PUSH, which is also why a write in A_PUSH and a read
        // for the next tile can never be the same cycle: the FSM is in one
        // state at a time, so there is no read-during-write hazard to bypass.
        A_TILE: begin
          tile_r    <= tile_c;
          tail_rd_q <= tail_ram[tile_c];
          hv_rd_q   <= hv_ram[tile_c];
          fill_rd_q <= fill_ram[tile_c];
          nch_rd_q  <= nch_ram[tile_c];
          if (tile_keep_c) begin
            st_q <= A_PUSH;
          end else begin
            // advance the cursor
            if (tx_r != tx1_r) begin
              tx_r <= tx_r + 6'd1;
              for (k = 0; k < 3; k = k + 1) ep_r[k] <= ep_r[k] + (kx_r[k] <<< 4);
            end else if (ty_r != ty1_r) begin
              ty_r       <= ty_r + 6'd1;
              tx_r       <= tx0_r;
              row_base_r <= row_base_r + TIDX_W'(GRID_W);
              for (k = 0; k < 3; k = k + 1) begin
                epr_r[k] <= epr_r[k] + (ky_r[k] <<< 4);
                ep_r[k]  <= epr_r[k] + (ky_r[k] <<< 4);
              end
            end else begin
              st_q <= A_IDLE;
            end
          end
        end

        // -----------------------------------------------------------------
        A_PUSH: begin
          // The id itself is written by `stg_we`/`stg_wsel` above, into the
          // bank this tile's fill level names.
          refs_binned_o <= refs_binned_o + 32'd1;

          if ((fill_rd_q + STG_W'(1)) == STG_W'(STAGE_IDS)) begin
            // The fourteenth id: this chunk leaves for SDRAM.
            cnt_r   <= STG_W'(STAGE_IDS);
            flush_q <= 1'b0;
            st_q    <= A_EMITR;
          end else begin
            fill_ram[tile_r] <= fill_rd_q + STG_W'(1);
            // advance the cursor -- the same three arms as A_TILE
            if (tx_r != tx1_r) begin
              tx_r <= tx_r + 6'd1;
              for (k = 0; k < 3; k = k + 1) ep_r[k] <= ep_r[k] + (kx_r[k] <<< 4);
              st_q <= A_TILE;
            end else if (ty_r != ty1_r) begin
              ty_r       <= ty_r + 6'd1;
              tx_r       <= tx0_r;
              row_base_r <= row_base_r + TIDX_W'(GRID_W);
              for (k = 0; k < 3; k = k + 1) begin
                epr_r[k] <= epr_r[k] + (ky_r[k] <<< 4);
                ep_r[k]  <= epr_r[k] + (ky_r[k] <<< 4);
              end
              st_q <= A_TILE;
            end else begin
              st_q <= A_IDLE;
            end
          end
        end

        // -----------------------------------------------------------------
        // One cycle for the staging read. `fill_ram` goes to zero here, so a
        // later reference to this tile starts a fresh chunk.
        A_EMITR: begin
          fill_ram[tile_r] <= '0;
          st_q             <= A_EMIT;
        end

        // -----------------------------------------------------------------
        A_EMIT: begin
          if (ck_ready_i) begin
            if (!ck_accept_i) begin
              // The arena refused it: its quota or its view bound. The frame
              // is walled HERE as well, so this block stops producing chunks
              // that would be discarded anyway, and the fault has a source on
              // the producer side rather than only inside the arena.
              chunk_refused_o <= chunk_refused_o + 32'd1;
              wall_q          <= 1'b1;
              overflow_o      <= 1'b1;
              st_q            <= flush_q ? A_DONE : A_IDLE;
            end else begin
              chunks_emitted_o <= chunks_emitted_o + 32'd1;
              alloc_q          <= ck_alloc_id_i;
              st_q             <= hv_rd_q ? A_LINK : A_DIR;
            end
          end
        end

        // -----------------------------------------------------------------
        A_LINK: begin
          if (lk_ready_i) begin
            links_patched_o <= links_patched_o + 32'd1;
            st_q            <= A_DIR;
          end
        end

        // -----------------------------------------------------------------
        A_DIR: begin
          tail_ram[tile_r] <= alloc_q;
          hv_ram[tile_r]   <= 1'b1;
          if (!hv_rd_q) begin
            head_ram[tile_r]  <= alloc_q;
            tiles_with_refs_o <= tiles_with_refs_o + 32'd1;
          end
          // THE PER-TILE HIGH-WATER MARK. `nch_rd_q` came OUT of `nch_ram` on
          // the directory read for this tile; the compare is that held value
          // against the instrument, so the two sides are not loaded by one
          // enable and the instrument can see a tile the frame total cannot.
          if (nch_rd_q != {CHW{1'b1}}) nch_ram[tile_r] <= nch_rd_q + CHW'(1);
          if ((nch_rd_q + CHW'(1)) > max_tile_chunks_o)
            max_tile_chunks_o <= nch_rd_q + CHW'(1);
          if (flush_q) begin
            if (sweep_r == TIDX_W'(TILES - 1)) begin
              st_q <= A_DONE;
            end else begin
              sweep_r <= sweep_r + TIDX_W'(1);
              st_q    <= A_FLUSH;
            end
          end else begin
            // mid-frame emission: resume the tile cursor
            if (tx_r != tx1_r) begin
              tx_r <= tx_r + 6'd1;
              for (k = 0; k < 3; k = k + 1) ep_r[k] <= ep_r[k] + (kx_r[k] <<< 4);
              st_q <= A_TILE;
            end else if (ty_r != ty1_r) begin
              ty_r       <= ty_r + 6'd1;
              tx_r       <= tx0_r;
              row_base_r <= row_base_r + TIDX_W'(GRID_W);
              for (k = 0; k < 3; k = k + 1) begin
                epr_r[k] <= epr_r[k] + (ky_r[k] <<< 4);
                ep_r[k]  <= epr_r[k] + (ky_r[k] <<< 4);
              end
              st_q <= A_TILE;
            end else begin
              st_q <= A_IDLE;
            end
          end
        end

        // -----------------------------------------------------------------
        A_FLUSH: begin
          tile_r    <= sweep_r;
          tail_rd_q <= tail_ram[sweep_r];
          hv_rd_q   <= hv_ram[sweep_r];
          fill_rd_q <= fill_ram[sweep_r];
          nch_rd_q  <= nch_ram[sweep_r];
          st_q      <= A_FLUSHD;
        end

        // -----------------------------------------------------------------
        A_FLUSHD: begin
          if (wall_q) begin
            flush_cut_o <= flush_cut_o + 32'd1;
            st_q        <= A_DONE;
          end else if (fill_rd_q != '0) begin
            cnt_r <= fill_rd_q;
            st_q  <= A_EMITR;
          end else if (sweep_r == TIDX_W'(TILES - 1)) begin
            st_q <= A_DONE;
          end else begin
            sweep_r <= sweep_r + TIDX_W'(1);
            st_q    <= A_FLUSH;
          end
        end

        // -----------------------------------------------------------------
        A_DONE: begin
          bin_frame_done_o <= 1'b1;
          frame_open_q     <= 1'b0;
          done_pend_q      <= 1'b0;
          flush_q          <= 1'b0;
          st_q             <= A_IDLE;
        end

        default: st_q <= A_IDLE;
      endcase

      // ---- the frame edges, AFTER the case and not before it ---------------
      // ORDER IS LOAD-BEARING HERE. These are non-blocking assignments to the
      // same variables the case arms write, so the LAST one wins -- textual
      // order does not merge them. Written above the case, a `seal_fire` that
      // coincided with any state transition would be silently overwritten by
      // that transition and the frame would start in the wrong state with its
      // directory uncleared. The arena's own header records the identical
      // trap costing a lost retirement count, so it is placed rather than
      // assumed.
      if (frame_start_i) begin
        frame_open_q <= 1'b1;
        done_pend_q  <= 1'b0;
        wall_q       <= 1'b0;
        overflow_o   <= 1'b0;
        flush_q      <= 1'b0;
        sweep_r      <= '0;
        st_q         <= A_CLEAR;
      end else if (geom_done_i && frame_open_q) begin
        done_pend_q <= 1'b1;
      end
    end
  end

endmodule : zhao_geom_arenabin

`default_nettype wire
