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
    input  logic        [15:0] st_src_id_i,

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

    input logic dual_i,  // 0 = legacy single-surface page: bottom == top

    // -----------------------------------------------------------------------
    // SWAP
    // -----------------------------------------------------------------------
    output logic fill_done_o,    // LAT_W*LAT_H records landed
    input  logic serve_release_i,  // TESS is finished with the served patch
    output logic serve_valid_o,  // a complete patch is available to serve

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

    input  logic       cs_req_i,
    input  logic [4:0] cs_ci_i,
    input  logic [4:0] cs_cj_i,
    output logic [1:0] cs_substance_o,

    // -----------------------------------------------------------------------
    // Counters
    // -----------------------------------------------------------------------
    output logic [31:0] fill_records_o,     // records taken into the CURRENT fill
    output logic [31:0] patches_filled_o,
    output logic [31:0] patches_served_o,
    output logic [31:0] fill_overrun_o,     // records past LAT_W*LAT_H: refused
    output logic [31:0] lat_oob_o,          // lattice requests outside the grid
    output logic [31:0] cs_oob_o            // cell requests outside the plane
);

  localparam int unsigned VERTS = LAT_W * LAT_H;           // 1,089
  localparam int unsigned CELLS = (LAT_W - 1) * (LAT_H - 1);  // 1,024
  localparam int unsigned VW    = $clog2(VERTS);
  localparam int unsigned CW    = $clog2(CELLS);

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

  wire st_take_c  = st_valid_i && st_ready_o;
  wire st_spill_c = st_valid_i && fill_active_q && at_capacity_c && !wphase_q;

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

  wire [LAW-1:0] rd_addr_c =
      LAW'( (serve_par_q  ? PAR_STRIDE : 0) +
            (lat_surface_i ? SURF_STRIDE : 0) +
            (lat_in_range_c ? vidx_c : 12'd0) );

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

  wire cs_w_in_range_c = (cs_w_ci_i < 5'(LAT_W - 1)) && (cs_w_cj_i < 5'(LAT_H - 1));
  wire [CW-1:0] cs_wr_addr_c =
      CW'( (fill_par_q ? CELLS : 0) +
           (cs_w_in_range_c ? (int'(cs_w_cj_i) * (LAT_W - 1) + int'(cs_w_ci_i)) : 0) );

  wire cs_rd_in_range_c = (cs_ci_i < 5'(LAT_W - 1)) && (cs_cj_i < 5'(LAT_H - 1));
  wire [CW-1:0] cs_rd_addr_c =
      CW'( (serve_par_q ? CELLS : 0) +
           (cs_rd_in_range_c ? (int'(cs_cj_i) * (LAT_W - 1) + int'(cs_ci_i)) : 0) );

  always_ff @(posedge clk) begin
    if (cs_we_i && cs_w_in_range_c) sub_m[cs_wr_addr_c] <= cs_w_substance_i;
    sub_rd_q <= sub_m[cs_rd_addr_c];
  end

  // Position planes: 33 words each, far too small for an M10K and correctly
  // left as MLAB/registers.
  wire [5:0] wx_wr_c = pos_idx_i;
  always_ff @(posedge clk) begin
    if (pos_we_i && !pos_axis_i && pos_idx_i < 6'(LAT_W))
      wx_m[(fill_par_q ? LAT_W : 0) + int'(wx_wr_c)] <= pos_val_i;
    if (pos_we_i && pos_axis_i && pos_idx_i < 6'(LAT_H))
      wz_m[(fill_par_q ? LAT_H : 0) + int'(wx_wr_c)] <= pos_val_i;
    wx_rd_q <= wx_m[(serve_par_q ? LAT_W : 0) + int'(lat_in_range_c ? lat_vi_i : 6'd0)];
    wz_rd_q <= wz_m[(serve_par_q ? LAT_H : 0) + int'(lat_in_range_c ? lat_vj_i : 6'd0)];
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

  logic req_ok_q, cs_req_ok_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      req_ok_q    <= 1'b0;
      cs_req_ok_q <= 1'b0;
    end else begin
      req_ok_q    <= lat_req_i && lat_in_range_c && serve_valid_q;
      cs_req_ok_q <= cs_req_i && cs_rd_in_range_c && serve_valid_q;
    end
  end

  assign lat_h_o  = req_ok_q ? lat_rd_q : POISON;
  assign lat_wx_o = req_ok_q ? wx_rd_q  : POISON;
  assign lat_wz_o = req_ok_q ? wz_rd_q  : POISON;
  // Substance has no spare encoding for poison in two bits, and inventing one
  // would change TESS's port. 3 is what the existing composed test already
  // drives when no request is pending; sec 3.3 gives 0 = SOLID, so 3 is not the
  // dangerous default. The COUNTER is the alarm here, not the value.
  assign cs_substance_o = cs_req_ok_q ? sub_rd_q : 2'd3;

  // ---- control ------------------------------------------------------------
  logic [31:0] patches_filled_q, patches_served_q, fill_overrun_q;
  logic [31:0] lat_oob_q, cs_oob_q;

  assign fill_records_o   = {{(32 - CURW){1'b0}}, wcur_q};
  assign patches_filled_o = patches_filled_q;
  assign patches_served_o = patches_served_q;
  assign fill_overrun_o   = fill_overrun_q;
  assign lat_oob_o        = lat_oob_q;
  assign cs_oob_o         = cs_oob_q;

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
      patches_filled_q <= '0;
      patches_served_q <= '0;
      fill_overrun_q   <= '0;
      lat_oob_q        <= '0;
      cs_oob_q         <= '0;
    end else begin
      // ---- fill ---------------------------------------------------------
      if (fill_go_c) begin
        fill_active_q <= 1'b1;
        wcur_q        <= '0;
        wphase_q      <= 1'b0;
        dual_q        <= dual_i;
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
      end else if (wphase_q) begin
        // Phase 1 is unconditional: the bottom write is already committed by
        // the acceptance, and making it wait on the producer would stall the
        // store on a producer that has nothing more to send.
        wphase_q <= 1'b0;
        wcur_q   <= wcur_q + 1'b1;
      end

      if (st_spill_c) fill_overrun_q <= fill_overrun_q + 1'b1;

      // Completing a fill hands the buffer over. It waits for the serve side
      // to be released, so a finished fill does not snatch the lattice out
      // from under a tessellator mid-patch.
      if (fill_active_q && at_capacity_c && (!serve_valid_q || serve_release_i)) begin
        fill_active_q    <= 1'b0;
        serve_par_q      <= fill_par_q;
        serve_valid_q    <= 1'b1;
        patches_filled_q <= patches_filled_q + 1'b1;
      end else if (serve_valid_q && serve_release_i) begin
        serve_valid_q <= 1'b0;
      end

      // COUNTED SEPARATELY, not in the else-arm above. A release landing on
      // the same clock as a handover takes the first branch, and folding the
      // count in there loses exactly the patch that was retired at the busiest
      // moment -- the steady state, where a fill finishes as a patch is
      // released, every patch. The counter would have under-reported precisely
      // when the pipeline was working.
      if (serve_valid_q && serve_release_i) patches_served_q <= patches_served_q + 1'b1;

      // ---- refusals -----------------------------------------------------
      if (lat_req_i && !lat_in_range_c) lat_oob_q <= lat_oob_q + 1'b1;
      if (cs_req_i && !cs_rd_in_range_c) cs_oob_q <= cs_oob_q + 1'b1;
    end
  end

`ifndef SYNTHESIS
  // THE PROPERTIES THIS BLOCK EXISTS TO KEEP.
  //
  // Immediate assertions in a clocked block, the idiom used by
  // zhao_geom_assetfetch.sv and zhao_texture_cache_pipe.sv, so they run under
  // Verilator in the differential rather than only under a formal frontend.
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
