// zhao_terrain_compcache_front.sv -- TERRAIN.COMPCACHE's on-chip patch front.
//
// THE MISSING MIDDLE. TERRAIN.PATCH composes one lattice vertex per clock and
// pushes `patch_state` records; TERRAIN.TESS reads a lattice through REGISTERED
// ports at one datum per clock. Nothing joined them, so the organ chain
// PATCH -> LOD -> TESS has never run end to end
// (design/contracts/TERRAIN.PATCH.md "Integration capture cases: None yet",
// design/contracts/TERRAIN.TESS.md "Not yet composed"). This block is that
// join: it catches the compose stream into a lattice and serves the
// tessellator from it.
//
// It is the ON-CHIP FRONT of a cache whose bulk cannot be on-chip. The full
// 256-patch composed store is 256 x 2,178 B for heights plus as much again for
// velocity = 8.92 Mbit = 161% of this device's entire 5.53 Mbit of M10K
// (reports/TERRAIN_WORLD_LAYER_ARCHITECTURE.md sec 2.5). The bulk therefore
// lives in the terrain hot-cache pool in SDRAM, and SDRAM cannot answer a
// registered port at one datum per clock. So exactly one patch is staged on
// chip, double-buffered so patch N+1 fills while TESS eats patch N. The SDRAM
// backing attaches later on the FILL side without changing the serve ports.
//
// ---------------------------------------------------------------------------
// WHY THIS STORES 33 + 33 WORLD POSITIONS AND NOT 1,089 PAIRS
// ---------------------------------------------------------------------------
// The obvious reading of TESS's port -- it asks for (vi, vj) and wants back
// h, wx, wz -- is that the front holds a world position per VERTEX. That would
// be 1,089 x 64 b per parity, about 14 M10K on top of the heights, doubling
// the block.
//
// It is separable, and this is not an assumption. zref::terrain::ComposedLattice
// declares `wx` per lattice COLUMN and `wz` per lattice ROW
// (reference/include/zref/zref_terrain.hpp) and states the requirement
// outright: "The lattice must be axis-aligned monotone (identity/axis
// placement -- island-datum space, the space sec 4.3 is written in)". The
// existing composed test reads exactly that way -- `lat_.wx[lat_vi_]`,
// `lat_.wz[lat_vj_]` (tests/terrain/terrain_lod_tess.cpp) -- so the RTL storing
// a pair per vertex would be storing 1,089 copies of 66 numbers.
//
// A ROTATED sheet would break this, and rotated terrain sheets are on the
// owner's feature list. It would not break silently: the placement space is
// axis-aligned BY THE REFERENCE'S OWN STATED PRECONDITION, so a rotated sheet
// is a change to sec 4.3's space that the reference must make first. When it
// does, the fix here is a 2x2 basis and four multiplies at one datum per clock
// -- affordable -- not a redesign. Recorded so the next reader knows this is a
// LAW being followed rather than a shortcut being taken.
//
// ---------------------------------------------------------------------------
// PATCH_STATE CARRIES NO VERTEX INDEX. THE ORDER IS THE INDEX.
// ---------------------------------------------------------------------------
// TERRAIN.PATCH's output port is {top, bottom, compose_top, dirty, src_id} --
// there is no (vi, vj) on it. The record's identity is its POSITION in the
// stream, matched against the order the vertices were submitted. That is sound
// because the compose lane is a single in-order lane, "1 cycle per vertex with
// no live field, and 1 + n cycles with n accepted field lanes"
// (design/contracts/TERRAIN.PATCH.md "Latency"), which cannot reorder.
//
// Sound is not the same as checked, so it is checked. The write cursor counts,
// `a_fill_no_overrun` refuses a 1,090th record rather than wrapping onto vertex
// zero, and `fill_records_o` is exported so a consumer can assert the count
// instead of trusting this paragraph. The differential additionally feeds the
// vertex index through `src_id` and requires it to come back matching, which
// pins the positional contract to a value rather than to a count.
//
// ---------------------------------------------------------------------------
// WHAT IT DOES NOT DO
// ---------------------------------------------------------------------------
// It does not compose (TERRAIN.PATCH's law), does not tessellate, does not
// allocate the 256 SDRAM slots (that allocator is frame-scoped and lives with
// the sequencer), and does not store the per-vertex dirty bit -- PATCH already
// reduces dirt to `subpatch_dirty_o`, the 4x4 mask that is what anything
// downstream actually consumes, so storing 1,089 loose bits here would be a
// second copy of an answer that already exists.

module zhao_terrain_compcache_front #(
    // 33 x 33 vertices over 32 x 32 cells. Parameters rather than literals so
    // a test can shrink the lattice; the production value is the only one the
    // budget was costed against.
    parameter int unsigned LAT_W = 33,
    parameter int unsigned LAT_H = 33
) (
    input logic clk,
    input logic rst_n,

    // -----------------------------------------------------------------------
    // FILL: patch_state in, port-for-port from zhao_terrain_patch
    // -----------------------------------------------------------------------
    // Pulse once before the first record of a patch. Resets the write cursor
    // and takes the fill buffer. Refused while no buffer is free, which is the
    // backpressure that keeps a fill from landing on the patch TESS is reading.
    input  logic fill_start_i,
    output logic fill_accept_o,  // 1-cycle pulse: the start was taken
    output logic fill_busy_o,

    input  logic               st_valid_i,
    output logic               st_ready_o,
    input  logic signed [31:0] st_top_i,     // live_top, fx16 raw
    input  logic signed [31:0] st_bottom_i,  // bottom surface, fx16 raw
    // Port-for-port with zhao_terrain_patch's st_src_id_o, and CARRIED rather
    // than discarded. It is not stored PER VERTEX -- that would be another
    // 2 x 1,089 x 16 b ~ 4 M10K for a field the tessellator never reads, the
    // same argument that keeps the per-vertex dirty bit out (see below). What
    // is stored is ONE id per BUFFER, handed to the serve side by the swap and
    // read back on serve_src_id_o.
    //
    // WHAT THE READBACK IS FOR. A consumer does not ask this block for a
    // patch. It asks the sequencer, the sequencer arranges a fill, and the
    // consumer then reads whatever parity the swap has left on the serve port.
    // Every other signal it could check is self-consistent under the failure
    // that actually matters: a swap that did not happen, or happened one patch
    // early, still answers every request with a real composed height from a
    // real patch, at a plausible world position, with every counter agreeing.
    // serve_src_id_o is the only thing on the serve side that names WHICH
    // patch, so a consumer can compare it against the id it asked for BEFORE
    // it tessellates a couple of thousand triangles of the wrong terrain.
    //
    // WHY THE LAST RECORD'S ID AND NOT THE FIRST. A buffer is handed over on
    // the clock its cursor reaches LAT_W*LAT_H, which is immediately after its
    // last record, so the last id is written by the same acceptance that
    // COMPLETED the fill. It therefore witnesses a finished fill; a
    // first-record capture would name a patch whose fill was abandoned halfway
    // with exactly the same confidence. It also composes with fill_records_o:
    // a producer that carries the vertex index in the low bits of src_id makes
    // serve_src_id_o a second, independent, SERVE-SIDE witness of the
    // order-is-the-index law, which has to agree with the fill-side one.
    //
    // WHY IT IS DOUBLE-BUFFERED, AND WHY A PLAIN `last_src_id_o` WOULD BE
    // WORSE THAN NOTHING. One flop holding "the src_id of the last record this
    // block accepted" tracks the buffer being FILLED. While TESS reads patch N
    // and patch N+1 is landing -- the entire reason this block is
    // double-buffered -- that flop names N+1. A consumer checking the identity
    // of what it is reading would be handed the identity of what it is NOT
    // reading, and would reject the correct patch or accept the wrong one at
    // precisely the moment the pipeline is doing its job. The id follows the
    // parity, like every other byte in here.
    input logic [15:0] st_src_id_i,

    // The 33 column x's and 33 row z's. Written before or during the record
    // stream; they are the placement, not the composition, so they do not
    // arrive on patch_state.
    input logic               pos_we_i,
    input logic               pos_axis_i,  // 0 = wx by column, 1 = wz by row
    input logic        [ 5:0] pos_idx_i,
    input logic signed [31:0] pos_val_i,

    // Layer D, the (LAT_W-1) x (LAT_H-1) cell-state plane. Two bits used;
    // 0 = SOLID per terrain_rules sec 3.3.
    input logic       cs_we_i,
    input logic [4:0] cs_w_ci_i,
    input logic [4:0] cs_w_cj_i,
    input logic [1:0] cs_w_substance_i,

    // -----------------------------------------------------------------------
    // Layer E, the (LAT_W-1) x (LAT_H-1) BASE MATERIAL plane -- {matA, matB,
    // weight} per cell, terrain_rules sec 2 and sec 6.2. Owner ruling R13.
    // -----------------------------------------------------------------------
    // IT LIVES HERE AND NOT IN A BLOCK OF ITS OWN, and the reason is the
    // arming law rather than the storage. A material plane needs to be
    // double-buffered by exactly the same parity as the heights and the
    // substance, released by exactly the same pulse, and armed on exactly the
    // same cycle -- and `zhao_terrain_spdesc`'s header already records this
    // tree's rule that two blocks arming off one event must not have two laws.
    // A separate plane would have had to COPY `fill_par_q`, `serve_par_q` and
    // `serve_valid_q`'s handover branch, and a copy of an arming law diverges
    // in the direction nobody looks: the material of patch N under the
    // heights of patch N+1, which RENDERS.
    //
    // The write face is `cs_we_i`'s, verbatim: fire and forget, no ready, the
    // producer owning the handshake. See the note at the cell write below for
    // why that is correct rather than a shortcut.
    input logic       mat_we_i,
    input logic [4:0] mat_w_ci_i,
    input logic [4:0] mat_w_cj_i,
    input logic [7:0] mat_w_a_i,
    input logic [7:0] mat_w_b_i,
    input logic [7:0] mat_w_weight_i,

    // -----------------------------------------------------------------------
    // The VELOCITY plane -- terrain_rules 4.2's height16 lattice, 2 B/vertex.
    // NEW 2026-09-26 (TERRVEL).
    // -----------------------------------------------------------------------
    // IT LIVES HERE FOR THE REASON 4.1 LAW 2 GIVES, VERBATIM: "Every consumer
    // -- tessellation/render, sim height query, particle collision, velocity,
    // normals, nav -- reads the SAME composed lattice." This block IS that
    // lattice in fabric. A velocity plane of its own would have had to copy
    // `fill_par_q`, `serve_par_q` and `serve_valid_q`'s handover branch, which
    // is the same argument the layer-E comment above makes, and a copy of an
    // arming law diverges.
    //
    // THE HEADER'S OWN 161% SENTENCE IS ABOUT A DIFFERENT THING AND MUST NOT
    // BE READ AS A REFUSAL OF THIS. "256 x 2,178 B for heights plus as much
    // again for velocity = 8.92 Mbit = 161%" prices ALL 256 PATCHES RESIDENT
    // AT ONCE, which is the SDRAM region's shape, not this block's. This block
    // holds TWO patches -- one filling, one serving. The velocity plane is
    // therefore 2 x 1,089 x 16 b = 34,848 bit, about 4 M10K, against a device
    // with 553. The owner directive authorises exactly this trade ("synchronous
    // banks ... exact narrower representations ... are authorized") and the
    // ALM/M10K note in CLAUDE.md names memory as the slack side.
    //
    // THE WRITE FACE IS `cs_we_i`'S AND `mat_we_i`'S, VERBATIM: fire and
    // forget, no ready, the producer owning the handshake. TERRAIN.VELOCITY
    // emits one word per vertex and can always be accepted, because this is one
    // RAM write -- which is also what lets TERRAIN.VELJOIN's ready-join never
    // backpressure the height lane once a sweep is running.
    //
    // THE WORD IS height16 (s16), NOT fx16. That is terrain_rules 4.2's frozen
    // storage format and `design/ops.yml` FIELD.OUT.VELOCITY's ratified
    // bake-back, and TERRAIN.VELOCITY has already done the single rounding
    // qformats 3 permits. Widening it here to 32 bits would store eight bits of
    // nothing per vertex and invite a second rounding downstream.
    input logic               vel_we_i,
    input logic        [ 5:0] vel_w_vi_i,
    input logic        [ 5:0] vel_w_vj_i,
    input logic signed [15:0] vel_w_val_i,
    // TERRAIN.VELOCITY's `patch_done_o`: the 1,089th word of THIS sweep landed.
    // PRESENCE TRAVELS WITH THE RESULT (owner directive 1) -- a plane that was
    // not completely written is served as ABSENT rather than as whatever the
    // previous patch left, so a consumer is never handed a stale velocity
    // beside a fresh height with every counter agreeing.
    input logic               vel_done_i,

    input logic dual_i,  // 0 = legacy single-surface page: bottom == top

    // -----------------------------------------------------------------------
    // SWAP
    // -----------------------------------------------------------------------
    output logic fill_done_o,  // LAT_W*LAT_H records landed
    // TESS is finished with the served patch. A PULSE, not a level: one patch
    // is retired per RISING EDGE. See the decision at `serve_release_c` -- a
    // consumer with two patches to retire lowers the line between them, which
    // it has to do anyway because it cannot have consumed the second one in
    // zero cycles.
    input  logic        serve_release_i,
    output logic        serve_valid_o,    // a complete patch is available to serve
    // The source id of the patch on the serve port, or SRC_NONE when there is
    // none. Combinational off serve_valid_o and the parity, so it is stable
    // for as long as the patch is: a consumer reads it once at the top of the
    // patch, not per request. See st_src_id_i for what it is for.
    output logic [15:0] serve_src_id_o,

    // -----------------------------------------------------------------------
    // SERVE: registered lattice port, port-for-port into zhao_terrain_tess
    // -----------------------------------------------------------------------
    // The datum is present the cycle AFTER the request
    // (design/contracts/TERRAIN.TESS.md).
    input  logic               lat_req_i,
    input  logic        [ 5:0] lat_vi_i,
    input  logic        [ 5:0] lat_vj_i,
    input  logic               lat_surface_i,  // 0 = top, 1 = bottom
    output logic signed [31:0] lat_h_o,
    output logic signed [31:0] lat_wx_o,
    output logic signed [31:0] lat_wz_o,
    // The velocity word at the SAME vertex, on the SAME request. It is not a
    // second serve port: a consumer asking for a vertex is asking terrain_rules
    // 4.3's `column_query`, whose ratified return tuple is
    // `{class, top, bottom, velocity, matA, matB, weight, sheet}` -- velocity
    // is a FIELD of that answer, not a separate query, and giving it its own
    // request would let the two disagree about which vertex they describe.
    // `lat_surface_i` does not apply: 4.2's velocity lattice is per-vertex with
    // no top/underside distinction, because a RATE has no surface (the same
    // reason TERRAIN.VELOCITY declines 3.4's underside clamp).
    output logic signed [15:0] lat_vel_o,
    output logic               lat_vel_present_o,

    input  logic       cs_req_i,
    input  logic [4:0] cs_ci_i,
    input  logic [4:0] cs_cj_i,
    output logic [1:0] cs_substance_o,

    // R13's layer-E query, port for port into `zhao_terrain_tess`'s `mat_*`.
    // `mat_valid_o` is this port's `cs_substance_o == 2'd3`: a material triple
    // has no spare encoding (0,0,0 is a legal cell -- tile 0 everywhere), so
    // the "I did not answer this" signal has to be a bit of its own rather
    // than a poison value the consumer has to recognise.
    input  logic       mat_req_i,
    input  logic [4:0] mat_ci_i,
    input  logic [4:0] mat_cj_i,
    output logic [7:0] mat_a_o,
    output logic [7:0] mat_b_o,
    output logic [7:0] mat_weight_o,
    output logic       mat_valid_o,

    // -----------------------------------------------------------------------
    // Counters
    // -----------------------------------------------------------------------
    output logic [31:0] fill_records_o,     // records taken into the CURRENT fill
    output logic [31:0] patches_filled_o,
    output logic [31:0] patches_served_o,
    output logic [31:0] fill_overrun_o,     // records past LAT_W*LAT_H: refused
    output logic [31:0] lat_oob_o,          // lattice requests outside the grid
    output logic [31:0] cs_oob_o,           // cell requests outside the plane
    output logic [31:0] mat_oob_o,          // layer-E requests outside the plane
    output logic [31:0] mat_cells_o,        // layer-E cells taken into a fill
    output logic [31:0] vel_words_o,        // velocity words taken into a fill
    output logic [31:0] vel_oob_o,          // velocity writes outside the grid
    // A velocity word arriving with NO fill buffer held. It is dropped, because
    // the only parity it could land in is the one a consumer is reading. A
    // non-zero value means TERRAIN.VELOCITY's sweep outlived its patch's fill,
    // which is a real finding about the walk and not noise.
    output logic [31:0] vel_orphan_o,
    // TERRAIN.VELOCITY claimed a sweep complete on a clock that was NOT the one
    // this block's 1,089th word landed on. The two operands are clocked by
    // different things on purpose -- see `vel_last_word_c`.
    output logic [31:0] vel_done_mismatch_o
);

  localparam int unsigned VERTS = LAT_W * LAT_H;           // 1,089
  localparam int unsigned CELLS = (LAT_W - 1) * (LAT_H - 1);  // 1,024
  localparam int unsigned VW    = $clog2(VERTS);
  // THE FULL ARRAY, NOT ONE PARITY. sub_m is 2*CELLS deep because it holds both
  // buffers, so its address needs $clog2(2*CELLS) = 11 bits. The first draft
  // used $clog2(CELLS) = 10, and `(fill_par_q ? CELLS : 0) + cell` truncated
  // 1024 to zero: BOTH PARITIES ALIASED ONTO THE SAME 1,024 CELLS. The cell
  // plane was single-buffered while the heights were double-buffered, so a fill
  // of patch N+1 rewrote the substance under a tessellator still reading patch
  // N -- real cell states, from the wrong patch, which renders. The lattice
  // side had it right (LAW = $clog2(LAT_N), the WHOLE array); this line took
  // the size of one half. Verilator says it outright: "Bit extraction of
  // array[2047:0] requires 11 bit index, not 10 bits".
  localparam int unsigned CW    = $clog2(2 * CELLS);

  // One array, both parities, both surfaces. ONE write address and ONE read
  // address across the whole thing, which is what a simple-dual-port M10K
  // needs -- splitting this into four arrays would give the fitter four
  // narrow memories instead of one deep one and would not change the bit
  // count. 4 x 1,089 x 32 b = 139,392 bit ~ 14 M10K.
  localparam int unsigned SURF_STRIDE = VERTS;
  localparam int unsigned PAR_STRIDE  = 2 * VERTS;
  localparam int unsigned LAT_N       = 2 * PAR_STRIDE;
  localparam int unsigned LAW         = $clog2(LAT_N);

  logic signed [31:0] lat_m [LAT_N];
  logic        [ 1:0] sub_m [2*CELLS];
  // Layer E, both parities, ONE array and ONE word: {matA, matB, weight}
  // always travel together and are never read apart, so three arrays would
  // give the fitter three narrow memories instead of one and would not change
  // the bit count. 2 x 1,024 x 24 b = 49,152 bit ~ 5 M10K. That is ALM traded
  // for M10K, which is the direction this device has slack in.
  //
  // THE ADDRESS IS `CW` WIDE, WHICH IS THE WHOLE ARRAY AND NOT ONE HALF. That
  // is not a style choice: the paragraph at `CW` records that taking
  // $clog2(CELLS) here truncated 1,024 to zero and aliased both parities onto
  // the same 1,024 cells, so the substance plane was single-buffered while the
  // heights were double-buffered. A material plane that aliased the same way
  // would put patch N+1's tile ids under a tessellator still reading patch N,
  // and unlike a wrong height a wrong tile id does not move anything -- it
  // just renders the wrong ground, which no geometric check would catch.
  logic        [23:0] mat_m [2*CELLS];

  // The velocity plane, both parities. ONE word per VERTEX and no surface
  // dimension -- 2 x 1,089 x 16 b = 34,848 bit, about 4 M10K. Addressed with
  // the lattice's own `vidx = vj * LAT_W + vi`, the same index the height
  // plane uses, so a velocity word and a height word at one vertex cannot
  // land at two different places.
  logic signed [15:0] vel_m [2*VERTS];

  // Per-buffer completion, handed over by the swap exactly as `src_m` is.
  // NOT a single flag: the filling buffer's completion and the serving
  // buffer's completion are different facts about different patches, and one
  // flag would let a fill in progress mark the patch being SERVED as present.
  logic vel_full_m[2];
  logic signed [31:0] wx_m  [2*LAT_W];
  logic signed [31:0] wz_m  [2*LAT_H];

  // ---- buffer ownership ---------------------------------------------------
  // fill_par is the buffer being written; serve_par is the one being read.
  // They are never equal while both are active, which is the entire reason
  // this is double-buffered.
  logic fill_par_q, serve_par_q;
  logic fill_active_q;   // a fill is in progress
  logic serve_valid_q;   // a complete patch is available
  logic dual_q;

  // The per-buffer source id: two flops, not a memory. Written on every
  // acceptance into the FILL parity, so each buffer ends holding the id of the
  // last record that landed in it, and read out of the SERVE parity.
  logic [15:0] src_m [2];

  // Poison for "no patch is being served". The low half of the 0x5BADF00D the
  // lattice port already poisons with, so a consumer that trips this sees a
  // value from a family it has recognised since before this block existed. A
  // real src_id could legally be any 16-bit number, so this is a TELL and not
  // a proof -- serve_valid_o is the proof.
  localparam logic [15:0] SRC_NONE = 16'hF00D;

  assign serve_src_id_o = serve_valid_q ? src_m[serve_par_q] : SRC_NONE;

  localparam int unsigned CURW = VW + 1;
  logic [CURW-1:0] wcur_q;  // write cursor, one bit wider than VERTS needs so
                            // "at capacity" is representable rather than a wrap

  assign fill_busy_o   = fill_active_q;
  assign serve_valid_o = serve_valid_q;

  // A fill may start when no fill is running and the buffer it would take is
  // not the one being served. With two buffers that is simply "not serving, or
  // serving the other one" -- and since a new fill always takes ~serve_par
  // when a patch is being served, the only blocking case is a fill already in
  // progress.
  wire fill_can_start_c = !fill_active_q;
  wire fill_go_c        = fill_start_i && fill_can_start_c;
  assign fill_accept_o  = fill_go_c;

  wire at_capacity_c = (wcur_q == CURW'(VERTS));

  // ONE RECORD IS TWO WRITE CLOCKS, AND READY MUST BE LOW ON THE SECOND.
  // A record carries both surfaces of one vertex, and the store has a single
  // write port, so top and bottom go in on consecutive clocks. The first draft
  // held st_ready_o up for both -- which under ready/valid means the PRODUCER
  // ADVANCES, so the next record's top would have been written into this
  // vertex's bottom plane. Every height would have been a real composed height,
  // every count would have matched, and the underside would have been the
  // neighbouring vertex's top surface: a lattice that is wrong by one vertex
  // is a terrain that renders. Hence: accept only on phase 0, and write the
  // bottom on phase 1 from the value CAPTURED at acceptance.
  assign st_ready_o = fill_active_q && !at_capacity_c && !wphase_q;

  wire st_take_c = st_valid_i && st_ready_o;

  // ---- fill_overrun_o COUNTS REFUSED RECORDS, NOT REFUSED CYCLES ----------
  // DECISION, 2026-09-06. Under ready/valid a producer holds its payload
  // stable and valid high until the record is taken. At capacity nothing is
  // ever taken, so ONE refused record sits on the port for as long as the
  // producer cares to wait, and counting the condition per clock reports the
  // producer's PATIENCE rather than the overrun: the same 1,090th record reads
  // as 1 or as 400 depending only on how long the consumer took to release the
  // patch that is blocking the swap. That number cannot be used for the thing
  // the port is named for. One count per distinct OFFER instead; an offer ends
  // when the producer withdraws valid, or when the record is finally taken.
  //
  // AND NOT A RISING EDGE OF st_valid_i, which is the obvious cheap version
  // and is wrong in the direction that hides things. A producer streaming back
  // to back never lowers valid, so its first REFUSED record follows an
  // ACCEPTED one with no edge anywhere in between, and an edge detector would
  // miss the very first overrun of a full-rate fill -- an instrument reading
  // low exactly where it matters. The flag clears on a take as well as on a
  // withdrawal, which covers both shapes of producer.
  //
  // AND THE FLAG IS READ BEFORE IT IS WRITTEN, ON PURPOSE, IN THAT ORDER.
  // The first draft put the flag's update at the TOP of the sequential block
  // and read it further down through a `wire st_spill_c = st_spill_raw_c &&
  // !spill_counted_q`. Under SystemVerilog's own semantics that is fine -- a
  // nonblocking assignment does not land until the NBA region, so the read
  // gets the pre-edge value -- but it is a read-after-write inside one block
  // through a continuous assignment, and a simulator is free to inline the
  // wire into its single consumer. This one does: adding a SECOND always_ff
  // that merely $display'd `spill_counted_q` changed the count, on identical
  // stimulus, because materialising the signal changed how it was scheduled.
  // A property that depends on whether anybody is looking at it is not a
  // property. So there is no wire, the test is written out at its one use
  // site, and the update sits immediately AFTER that use -- which reads the
  // same under blocking and nonblocking rules, and needs no scheduler to be
  // charitable.
  logic spill_counted_q;
  wire  st_spill_raw_c = st_valid_i && fill_active_q && at_capacity_c && !wphase_q;

  // ---- serve_release_i IS AN EVENT, NOT A LEVEL ---------------------------
  // DECISION, 2026-09-06, and it closes a real control defect rather than only
  // a counting one. The port means "TESS is finished with the served patch",
  // which happens once per patch. Read as a level it means something else, and
  // the difference is not academic: with a fill already complete and waiting
  // -- the steady state -- a release held high for two clocks hands the new
  // patch over on the first clock and RELEASES IT ON THE SECOND. A whole patch
  // retired without one vertex being read, serve_valid_o dropping under a
  // tessellator that had just been told a patch was there, and
  // patches_served_o counting it as if it had been consumed. A counter that
  // counts cycles while claiming to count events is a broken instrument; a
  // control path that does the same throws away terrain.
  //
  // So: one release per RISING EDGE. The cost is that a consumer retiring two
  // patches must lower the line between them, which it has to do anyway --
  // it cannot have consumed the second patch in zero cycles -- and a consumer
  // that ties the port high now serves each patch instead of discarding it,
  // which is the failure the old reading turned into silence.
  logic serve_release_q;
  wire  serve_release_c = serve_release_i && !serve_release_q;

  assign fill_done_o = fill_active_q && at_capacity_c;

  // ---- write address ------------------------------------------------------
  // The cursor IS the vertex index, z-then-x, matching ComposedLattice's
  // `top` ordering. Both surfaces of a vertex are written from one record, so
  // two writes per record would be needed -- instead the bottom plane is
  // written on the same clock at its own address, which is why the surface
  // stride is a separate array region rather than a wider word: a 64-bit word
  // would halve the addressable depth and force TESS's single-surface read to
  // fetch both.
  //
  // Two writes per record and one read per request is NOT simple-dual-port.
  // Resolved by alternating: a record occupies TWO fill clocks, top then
  // bottom. st_ready_o already gates on that through `wphase_q`.
  logic wphase_q;  // 0 = write top, 1 = write bottom

  logic signed [31:0] bot_q;  // the bottom surface, captured at acceptance

  wire lat_we_c = st_take_c || wphase_q;

  wire [LAW-1:0] wr_addr_c =
      LAW'( (fill_par_q ? PAR_STRIDE : 0) +
            (wphase_q   ? SURF_STRIDE : 0) +
            wcur_q );

  // On phase 1 the bottom comes from the capture register, never from the
  // port -- the port is showing the NEXT record by then.
  wire signed [31:0] wr_data_c = wphase_q ? bot_q : st_top_i;

  // ---- read address -------------------------------------------------------
  // vj * LAT_W is a shift-add for LAT_W = 33: (vj << 5) + vj. Written as a
  // multiply and left to the synthesiser, which does exactly that for a
  // constant; a hand-rolled shift-add here would be a hand-rolled bug for a
  // production value it already handles.
  wire [11:0] vidx_c = 12'(lat_vj_i) * 12'(LAT_W) + 12'(lat_vi_i);

  wire lat_in_range_c = (lat_vi_i < 6'(LAT_W)) && (lat_vj_i < 6'(LAT_H));

  // The out-of-range fold to vertex 0 is its own wire rather than a ternary
  // inside the address sum: in the sum it sits in 32-bit context and the
  // 12-bit arms get expanded (WIDTHEXPAND). The answer is poisoned by
  // `req_ok_q` regardless of what this address reads.
  wire [11:0] rd_vidx_c = lat_in_range_c ? vidx_c : 12'd0;
  wire [ 5:0] rd_vi_c   = lat_in_range_c ? lat_vi_i : 6'd0;
  wire [ 5:0] rd_vj_c   = lat_in_range_c ? lat_vj_i : 6'd0;

  wire [LAW-1:0] rd_addr_c =
      LAW'( (serve_par_q  ? PAR_STRIDE : 0) +
            (lat_surface_i ? SURF_STRIDE : 0) +
            int'(rd_vidx_c) );

  // ---- the memories -------------------------------------------------------
  // Clock only, no reset, no logic between the array read and the register it
  // lands in -- QUARTUS_GOTCHAS sec 14: anything combinational there blocks the
  // M10K output-register absorption and turns 14 M10K into flip-flops.
  logic signed [31:0] lat_rd_q;
  logic        [ 1:0] sub_rd_q;
  logic signed [31:0] wx_rd_q, wz_rd_q;

  always_ff @(posedge clk) begin
    if (lat_we_c) lat_m[wr_addr_c] <= wr_data_c;
    lat_rd_q <= lat_m[rd_addr_c];
  end

  // COMPARE AT A WIDTH THAT CAN HOLD THE BOUND. `5'(LAT_W - 1)` is `5'(32)`,
  // which truncates to ZERO, so the first draft's test was `cs_w_ci_i < 0` --
  // false for every input. Every cell write was dropped, every cell read
  // counted as out of range and returned substance 3, and the entire layer-D
  // plane was dead while every height was perfect. Verilator names it exactly:
  // "Comparison is constant due to unsigned arithmetic". Zero-extend the 5-bit
  // port and state the bound in 6 bits: at the production 32 x 32 plane every
  // value a 5-bit port can carry IS in range, which is the right answer, and a
  // shrunken lattice (the parameters exist for that) still rejects.
  wire cs_w_in_range_c = ({1'b0, cs_w_ci_i} < 6'(LAT_W - 1)) &&
                         ({1'b0, cs_w_cj_i} < 6'(LAT_H - 1));
  wire [CW-1:0] cs_wr_addr_c =
      CW'( (fill_par_q ? CELLS : 0) +
           (cs_w_in_range_c ? (int'(cs_w_cj_i) * (LAT_W - 1) + int'(cs_w_ci_i)) : 0) );

  wire cs_rd_in_range_c = ({1'b0, cs_ci_i} < 6'(LAT_W - 1)) &&
                          ({1'b0, cs_cj_i} < 6'(LAT_H - 1));
  wire [CW-1:0] cs_rd_addr_c =
      CW'( (serve_par_q ? CELLS : 0) +
           (cs_rd_in_range_c ? (int'(cs_cj_i) * (LAT_W - 1) + int'(cs_ci_i)) : 0) );

  always_ff @(posedge clk) begin
    if (cs_we_i && cs_w_in_range_c) sub_m[cs_wr_addr_c] <= cs_w_substance_i;
    sub_rd_q <= sub_m[cs_rd_addr_c];
  end

  // ---- layer E: the same two addresses, the same two parities -------------
  // Written out rather than shared with the block above because the two planes
  // have DIFFERENT PRODUCERS on different walks -- substance arrives from
  // TERRAIN.BAKE's cell stream, material from TERRAIN.PAGESTREAM's vertex
  // beat -- so a single write enable driving both is the lockstep this file's
  // own `cs_wr_addr_c` paragraph warns about, one level up.
  wire mat_w_in_range_c = ({1'b0, mat_w_ci_i} < 6'(LAT_W - 1)) &&
                          ({1'b0, mat_w_cj_i} < 6'(LAT_H - 1));
  wire [CW-1:0] mat_wr_addr_c =
      CW'( (fill_par_q ? CELLS : 0) +
           (mat_w_in_range_c ? (int'(mat_w_cj_i) * (LAT_W - 1) + int'(mat_w_ci_i)) : 0) );

  wire mat_rd_in_range_c = ({1'b0, mat_ci_i} < 6'(LAT_W - 1)) &&
                           ({1'b0, mat_cj_i} < 6'(LAT_H - 1));
  wire [CW-1:0] mat_rd_addr_c =
      CW'( (serve_par_q ? CELLS : 0) +
           (mat_rd_in_range_c ? (int'(mat_cj_i) * (LAT_W - 1) + int'(mat_ci_i)) : 0) );

  logic [23:0] mat_rd_q;
  always_ff @(posedge clk) begin
    if (mat_we_i && mat_w_in_range_c)
      mat_m[mat_wr_addr_c] <= {mat_w_a_i, mat_w_b_i, mat_w_weight_i};
    mat_rd_q <= mat_m[mat_rd_addr_c];
  end

  // ---- the velocity plane: the same two parities, the lattice's own index --
  // The range test zero-extends the 6-bit port and states the bound in 7 bits,
  // for the reason the layer-D comment below spells out at length: `6'(LAT_W)`
  // is `6'(33)`, which fits, but the next lattice size up would not, and a
  // comparison that is constant due to unsigned arithmetic fails SILENTLY and
  // in the flattering direction -- every write dropped, every read poisoned,
  // and every height still perfect.
  wire vel_w_in_range_c = ({1'b0, vel_w_vi_i} < 7'(LAT_W)) &&
                          ({1'b0, vel_w_vj_i} < 7'(LAT_H));

  // A word with no fill buffer held has nowhere legal to go: the only parity
  // that exists for it is the one being SERVED. Dropped and counted (J3's
  // hazard, from the other side).
  wire vel_we_ok_c = vel_we_i && vel_w_in_range_c && fill_active_q;

  wire [11:0] vel_w_vidx_c = 12'(vel_w_vj_i) * 12'(LAT_W) + 12'(vel_w_vi_i);
  wire [11:0] vel_wr_vidx_c = vel_w_in_range_c ? vel_w_vidx_c : 12'd0;

  wire [VW:0] vel_wr_addr_c =
      (VW + 1)'( (fill_par_q ? VERTS : 0) + int'(vel_wr_vidx_c) );

  // The read reuses the HEIGHT request's folded index, so the two planes are
  // structurally incapable of answering about different vertices.
  wire [VW:0] vel_rd_addr_c =
      (VW + 1)'( (serve_par_q ? VERTS : 0) + int'(rd_vidx_c) );

  logic signed [15:0] vel_rd_q;
  always_ff @(posedge clk) begin
    if (vel_we_ok_c) vel_m[vel_wr_addr_c] <= vel_w_val_i;
    vel_rd_q <= vel_m[vel_rd_addr_c];
  end

  // Position planes: 33 words each, far too small for an M10K and correctly
  // left as MLAB/registers.
  wire [5:0] wx_wr_c = pos_idx_i;
  always_ff @(posedge clk) begin
    if (pos_we_i && !pos_axis_i && pos_idx_i < 6'(LAT_W))
      wx_m[(fill_par_q ? LAT_W : 0) + int'(wx_wr_c)] <= pos_val_i;
    if (pos_we_i && pos_axis_i && pos_idx_i < 6'(LAT_H))
      wz_m[(fill_par_q ? LAT_H : 0) + int'(wx_wr_c)] <= pos_val_i;
    wx_rd_q <= wx_m[(serve_par_q ? LAT_W : 0) + int'(rd_vi_c)];
    wz_rd_q <= wz_m[(serve_par_q ? LAT_H : 0) + int'(rd_vj_c)];
  end

  // ---- the registered answer ---------------------------------------------
  // POISON, NOT ZERO, for a request outside the grid or with no request
  // pending. Zero is a legal height and a legal world coordinate, so returning
  // zero would let a consumer that reads without asking, or asks off the edge,
  // produce a plausible flat triangle. 0x5BADF00D is the value the existing
  // composed test already uses for exactly this
  // (tests/terrain/terrain_lod_tess.cpp), so a consumer that trips this sees a
  // value it has been able to recognise since before this block existed.
  localparam logic signed [31:0] POISON = 32'sh5BADF00D;

  logic req_ok_q, cs_req_ok_q, mat_req_ok_q, vel_ok_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      req_ok_q     <= 1'b0;
      cs_req_ok_q  <= 1'b0;
      mat_req_ok_q <= 1'b0;
      vel_ok_q     <= 1'b0;
    end else begin
      req_ok_q     <= lat_req_i && lat_in_range_c && serve_valid_q;
      // A SEPARATE FLOP AGAIN, and here it carries an EXTRA term the height's
      // does not: the served buffer's velocity plane must have been completely
      // written. Sharing `req_ok_q` would have made presence a property of the
      // REQUEST when it is a property of the BUFFER, and the two are exactly
      // what disagree on the patch where a sweep was aborted -- the one case
      // the flag exists for. Read from `serve_par_q`, which is the parity this
      // cycle's read address used.
      vel_ok_q     <= lat_req_i && lat_in_range_c && serve_valid_q
                      && vel_full_m[serve_par_q];
      cs_req_ok_q  <= cs_req_i && cs_rd_in_range_c && serve_valid_q;
      // A SEPARATE FLOP, not a share of `cs_req_ok_q`. The two queries come
      // from different ports of the same consumer on different cycles -- TESS
      // scans substance once per JOB and reads material once per TRIANGLE --
      // so one accept flop serving both would make `mat_valid_o` a statement
      // about whether a CELL-STATE request was in range. That is the
      // two-operands-one-enable fault exactly: the flag would read true on
      // every cycle the other port was busy and the material plane would be
      // trusted for cells nobody asked it about.
      mat_req_ok_q <= mat_req_i && mat_rd_in_range_c && serve_valid_q;
    end
  end

  assign lat_h_o  = req_ok_q ? lat_rd_q : POISON;
  assign lat_wx_o = req_ok_q ? wx_rd_q  : POISON;
  assign lat_wz_o = req_ok_q ? wz_rd_q  : POISON;

  // ZERO, NOT POISON, and the difference is deliberate. A height has no legal
  // zero -- terrain at exactly 0 is meaningful, so an unanswered height must
  // be a value nothing could mistake for one. A VELOCITY's zero is the law V2
  // answer for ground no field touches, so zero is the correct reading of
  // "absent" for a consumer that ignores `lat_vel_present_o`, and poison would
  // be a huge fake speed. The present bit is still the thing to read: absent
  // means NOT MEASURED, and zero means MEASURED AS STILL.
  assign lat_vel_o         = vel_ok_q ? vel_rd_q : 16'sd0;
  assign lat_vel_present_o = vel_ok_q;
  // Substance has no spare encoding for poison in two bits, and inventing one
  // would change TESS's port. 3 is what the existing composed test already
  // drives when no request is pending; sec 3.3 gives 0 = SOLID, so 3 is not the
  // dangerous default. The COUNTER is the alarm here, not the value.
  assign cs_substance_o = cs_req_ok_q ? sub_rd_q : 2'd3;

  // A MATERIAL TRIPLE HAS NO SPARE ENCODING. {0, 0, 0} is a perfectly legal
  // cell -- terrain_rules sec 6.2 makes weight 0 "matB everywhere", so it means
  // tile 0 -- and every one of the 2^24 words is reachable from a legal page.
  // So the not-answered signal is a BIT and not a value, and the consumer is
  // the one that decides what to emit when it is low. TESS declares {0,0,0}
  // and counts; nothing has to recognise a poison pattern.
  assign mat_a_o      = mat_req_ok_q ? mat_rd_q[23:16] : 8'd0;
  assign mat_b_o      = mat_req_ok_q ? mat_rd_q[15: 8] : 8'd0;
  assign mat_weight_o = mat_req_ok_q ? mat_rd_q[ 7: 0] : 8'd0;
  assign mat_valid_o  = mat_req_ok_q;

  // ---- control ------------------------------------------------------------
  logic [31:0] patches_filled_q, patches_served_q, fill_overrun_q;
  logic [31:0] lat_oob_q, cs_oob_q;
  logic [31:0] mat_oob_q, mat_cells_q;
  logic [31:0] vel_words_q, vel_oob_q, vel_orphan_q, vel_done_mm_q;

  // THE COUNT IS THE AUTHORITY ON COMPLETENESS, NOT THE PULSE. The plane is
  // marked present when the 1,089th word of THIS fill lands, which is a fact
  // this block observed. `vel_done_i` -- TERRAIN.VELOCITY's own `patch_done_o`
  // -- is then CROSS-CHECKED against it rather than believed, because the two
  // are clocked by different things: the count by this block's write enable,
  // the pulse by TERRAIN.VELOCITY's internal `last_vtx`. A detector whose two
  // operands share an enable cannot fire, and this repository has already
  // shipped one that did not.
  wire vel_last_word_c = vel_we_ok_c && (vel_words_q == 32'(VERTS - 1));

  assign fill_records_o   = {{(32 - CURW){1'b0}}, wcur_q};
  assign patches_filled_o = patches_filled_q;
  assign patches_served_o = patches_served_q;
  assign fill_overrun_o   = fill_overrun_q;
  assign lat_oob_o        = lat_oob_q;
  assign cs_oob_o         = cs_oob_q;
  assign mat_oob_o        = mat_oob_q;
  // `mat_cells_o` is NOT a tautology of the write enable and is the one number
  // that says the layer-E fill is COMPLETE rather than merely happening: a
  // patch owes exactly (LAT_W-1)*(LAT_H-1) = 1,024 cells, and a producer whose
  // walk skips the last row -- the shape a 33-vertex walk reused for 32 cells
  // fails in -- lands 992 and every other counter in this block still balances.
  assign mat_cells_o      = mat_cells_q;
  assign vel_words_o      = vel_words_q;
  assign vel_oob_o        = vel_oob_q;
  assign vel_orphan_o     = vel_orphan_q;
  assign vel_done_mismatch_o = vel_done_mm_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      fill_par_q       <= 1'b0;
      serve_par_q      <= 1'b1;
      fill_active_q    <= 1'b0;
      serve_valid_q    <= 1'b0;
      wcur_q           <= '0;
      wphase_q         <= 1'b0;
      bot_q            <= '0;
      dual_q           <= 1'b0;
      src_m[0]         <= SRC_NONE;
      src_m[1]         <= SRC_NONE;
      spill_counted_q  <= 1'b0;
      serve_release_q  <= 1'b0;
      patches_filled_q <= '0;
      patches_served_q <= '0;
      fill_overrun_q   <= '0;
      lat_oob_q        <= '0;
      cs_oob_q         <= '0;
      mat_oob_q        <= '0;
      mat_cells_q      <= '0;
      vel_words_q      <= '0;
      vel_oob_q        <= '0;
      vel_orphan_q     <= '0;
      vel_done_mm_q    <= '0;
      vel_full_m[0]    <= 1'b0;
      vel_full_m[1]    <= 1'b0;
    end else begin
      serve_release_q <= serve_release_i;

      // ---- the velocity plane ---------------------------------------------
      // These sit ABOVE the fill branch, so `fill_go_c`'s `vel_words_q <= '0`
      // overrides this increment on a clock where both fire. That is the
      // correct precedence and it is stated rather than left to the reader:
      // a fill start is the patch's FIRST vertex and TERRAIN.VELOCITY has not
      // been started yet, so no word can legitimately arrive on that clock --
      // and if one ever did it would belong to the sweep being abandoned, not
      // to the one beginning.
      if (vel_we_ok_c) vel_words_q <= vel_words_q + 1'b1;
      if (vel_we_i && !vel_w_in_range_c) vel_oob_q <= vel_oob_q + 1'b1;
      // A word with no buffer held. Dropped by `vel_we_ok_c` and counted here:
      // the only parity available to it is the one being served, so writing it
      // would corrupt a patch a consumer is reading.
      if (vel_we_i && vel_w_in_range_c && !fill_active_q)
        vel_orphan_q <= vel_orphan_q + 1'b1;
      // The 1,089th word of this fill makes the plane PRESENT.
      if (vel_last_word_c) vel_full_m[fill_par_q] <= 1'b1;
      // The cross-check, not a belief: TERRAIN.VELOCITY says 'sweep
      // complete' and this block says 'the 1,089th word just landed'. They
      // must be the same clock. Nothing loads both operands.
      if (vel_done_i && fill_active_q && !vel_last_word_c)
        vel_done_mm_q <= vel_done_mm_q + 1'b1;

      // ---- fill ---------------------------------------------------------
      if (fill_go_c) begin
        fill_active_q <= 1'b1;
        wcur_q        <= '0;
        wphase_q      <= 1'b0;
        dual_q        <= dual_i;
        // The incoming buffer's velocity plane is ABSENT until this fill's own
        // 1,089 words land. Cleared on the buffer being TAKEN, not on the swap,
        // so a fill that is abandoned part-way can never hand over a plane that
        // is half this patch and half the last one.
        vel_words_q       <= '0;
        vel_full_m[~serve_par_q] <= 1'b0;
        // Take the buffer that is NOT being served. When nothing is served
        // yet this is still well defined because serve_par_q resets to the
        // opposite of fill_par_q.
        fill_par_q    <= ~serve_par_q;
      end

      if (st_take_c) begin
        // A legacy single-surface page has no modelled underside, so its
        // bottom IS its top (terrain_rules sec 3.1 option (a), the degenerate
        // case ComposedLattice calls dual == false). Resolved HERE, at
        // capture, so the serve side never has to know which kind of page it
        // is holding.
        bot_q    <= dual_q ? st_bottom_i : st_top_i;
        wphase_q <= 1'b1;
        // The buffer's identity, overwritten by every record so the buffer
        // ends holding the id of the record that COMPLETED it. Into the FILL
        // parity -- writing one flop for "the last id this block saw" would
        // name the patch being written while a consumer reads the other one.
        src_m[fill_par_q] <= st_src_id_i;
      end else if (wphase_q) begin
        // Phase 1 is unconditional: the bottom write is already committed by
        // the acceptance, and making it wait on the producer would stall the
        // store on a producer that has nothing more to send.
        wphase_q <= 1'b0;
        wcur_q   <= wcur_q + 1'b1;
      end

      // ONE COUNT PER DISTINCT OFFER. Read the flag...
      if (st_spill_raw_c && !spill_counted_q) fill_overrun_q <= fill_overrun_q + 1'b1;
      // ...then update it, on the next lines and never on earlier ones: armed
      // by the first refused clock of an offer, disarmed when the producer
      // withdraws the offer or the block finally takes the record.
      if (!st_valid_i || st_take_c) spill_counted_q <= 1'b0;
      else if (st_spill_raw_c) spill_counted_q <= 1'b1;

      // Completing a fill hands the buffer over. It waits for the serve side
      // to be released, so a finished fill does not snatch the lattice out
      // from under a tessellator mid-patch.
      if (fill_active_q && at_capacity_c && (!serve_valid_q || serve_release_c)) begin
        fill_active_q    <= 1'b0;
        serve_par_q      <= fill_par_q;
        serve_valid_q    <= 1'b1;
        patches_filled_q <= patches_filled_q + 1'b1;
        // AND MOVE THE FILL POINTER OFF THE BUFFER WE JUST HANDED OVER.
        // fill_par_q used to stay put until the next fill_start_i, so between
        // a handover and the next start it EQUALLED serve_par_q -- and the two
        // write ports that are not gated on fill_active_q, the placement planes
        // (pos_we_i) and the cell plane (cs_we_i), therefore wrote straight
        // into the patch the tessellator was reading. The port comments invite
        // exactly that: they say the placement is "written BEFORE or during the
        // record stream". The record path was never exposed (st_ready_o gates
        // on fill_active_q); these two were. One assignment closes it, and the
        // next fill_go_c writes the same value, so nothing else changes.
        fill_par_q       <= ~fill_par_q;
      end else if (serve_valid_q && serve_release_c) begin
        serve_valid_q <= 1'b0;
      end

      // COUNTED SEPARATELY, not in the else-arm above. A release landing on
      // the same clock as a handover takes the first branch, and folding the
      // count in there loses exactly the patch that was retired at the busiest
      // moment -- the steady state, where a fill finishes as a patch is
      // released, every patch. The counter would have under-reported precisely
      // when the pipeline was working.
      if (serve_valid_q && serve_release_c) patches_served_q <= patches_served_q + 1'b1;

      // ---- refusals -----------------------------------------------------
      if (lat_req_i && !lat_in_range_c) lat_oob_q <= lat_oob_q + 1'b1;
      if (cs_req_i && !cs_rd_in_range_c) cs_oob_q <= cs_oob_q + 1'b1;
      if (mat_req_i && !mat_rd_in_range_c) mat_oob_q <= mat_oob_q + 1'b1;
      // THE LAYER-E CELL COUNT IS PER FILL, AND THE START CYCLE COUNTS.
      //
      // The clear and the increment are in one always_ff, so they must be ONE
      // decision rather than two assignments racing to be last. Writing them
      // as two -- a clear in the `fill_go_c` arm above and a guarded increment
      // here -- is what the first draft did, and it is WRONG IN THE CONSOLE
      // AND NOWHERE ELSE, which is the kind that ships:
      //
      //   `zhao_console_core`'s `tcc_fill_start` is
      //       tps_v_valid && tps_v_ready && tps_v_first && tpc_placed
      //   -- the FIRST accepted vertex beat, which is vertex (0,0), which IS
      //   a cell origin. So the start and the first cell land on the SAME
      //   CLOCK on every page.
      //
      // With a `!fill_go_c` guard cell 0 is never counted and this reads 1,023
      // forever -- a counter permanently announcing the exact fault it was
      // built to detect, and the one value that makes an operator distrust a
      // plane that is fine. With no guard at all the increment wins and the
      // clear is lost, so the count runs on from the previous patch.
      //
      // One decision, both cases named:
      if (fill_go_c)
        mat_cells_q <= (mat_we_i && mat_w_in_range_c) ? 32'd1 : 32'd0;
      else if (mat_we_i && mat_w_in_range_c && (mat_cells_q != 32'hFFFF_FFFF))
        mat_cells_q <= mat_cells_q + 32'd1;
    end
  end

`ifndef SYNTHESIS
  // THE PROPERTIES THIS BLOCK EXISTS TO KEEP.
  //
  // Immediate assertions in a clocked block, the idiom used by
  // zhao_geom_assetfetch.sv and zhao_texture_cache_pipe.sv, so they run in the
  // differential under a simulator rather than only under a formal frontend.
  //
  // A COMMENT LINE MUST NOT BEGIN WITH THE SIMULATOR'S NAME. The previous
  // wording broke this line across the comment so that one line started with
  // that word, and the lexer reads a comment beginning with it as a PRAGMA:
  // "%Error-BADVLTPRAGMA: Unknown verilator comment". The file did not lex at
  // all -- which is the evidence that it had never been through the tool.
  // `rst_n` is deliberately not read synchronously here (SYNCASYNCNET); a plain
  // armed flag is what the assertions actually want.
  //
  // ENFORCED-BY: tests/terrain/compcache_front_rtl_directed.cpp
  logic armed_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) armed_q <= 1'b0;
    else armed_q <= 1'b1;
  end

  always_ff @(posedge clk) begin
    if (armed_q) begin
      // The whole point of double buffering.
      a_buffers_differ :
      assert (!(fill_active_q && serve_valid_q) || (fill_par_q != serve_par_q))
      else $error("compcache_front: fill and serve on the same buffer %0d", fill_par_q);

      // The cursor is the vertex index; past the end it must have stopped, not
      // wrapped.
      a_fill_no_overrun :
      assert (wcur_q <= CURW'(VERTS))
      else $error("compcache_front: write cursor %0d past %0d vertices", wcur_q, VERTS);

      // A record must never be taken with no fill running -- that would write
      // the served buffer.
      a_no_orphan_record :
      assert (!(st_valid_i && st_ready_o && !fill_active_q))
      else $error("compcache_front: patch_state record taken with no fill open");
    end
  end
`endif

endmodule
