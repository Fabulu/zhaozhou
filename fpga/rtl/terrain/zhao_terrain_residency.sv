// zhao_terrain_residency.sv — the island page directory: which patches are
// resident, which slot holds each, and what must be written back before a slot
// is reused.
//
// FIRST BLOCK OF THE WORLD LAYER. Nothing instantiates it yet.
//
// ---------------------------------------------------------------------------
// WHY THE 8 KM WORLD IS NOT THERE YET
// ---------------------------------------------------------------------------
// `reports/Missingterrain` puts it exactly:
//
//   > The terrain engine's organs exist. The circulatory system that feeds them
//   > an island does not.
//
// The patch arithmetic is built and heavily tested -- live field composition,
// scar baking, projected-error LOD, crack-safe tessellation, undersides,
// normals, projection, velocity. What is missing is everything that decides
// WHICH patches those organs should be processing:
//
//   > A deterministic 1,024-slot residency manager.
//   > Page generations, CRCs and stale-handle protection.
//   > Dirty-page writeback for permanent scars and breaches.
//
// This is that directory. It answers three questions and nothing else:
// is this patch resident, which slot holds it, and what has to be saved before
// that slot is taken away.
//
// It deliberately does NOT fetch, does not touch VRAM and does not know what a
// page contains. Those belong to the loader and to MEM.GUARD; a directory that
// also moved bytes would be two blocks welded together and impossible to test
// as either.
//
// ---------------------------------------------------------------------------
// WHY DIRECT-MAPPED, AND WHY THAT IS NOT A COMPROMISE
// ---------------------------------------------------------------------------
// The report asks for a DETERMINISTIC manager, and determinism is not a nice
// property here -- the console's whole verification story is deterministic
// replay. An LRU or pseudo-random victim makes residency depend on history,
// and two runs of the same capture can then evict different pages, stream
// different bytes and diverge in timing while both being "correct".
//
// Direct-mapping on the patch's own low coordinate bits makes the victim a
// pure function of the incoming key. Same camera path, same evictions, every
// time. It also matches how the world is actually walked: a camera moves
// through neighbouring patches, and neighbours differ in their low bits, so
// the common case spreads across slots rather than colliding.
//
// The cost is honest and stated: two patches 32 apart in X and 32 apart in Y
// collide permanently. At 64 m a patch, that is a 2,048 m period -- so a
// single island smaller than 2 km never self-collides, and two islands that do
// collide thrash. `collisions_o` counts exactly that, so the ugly case is
// visible in a capture instead of being discovered as a frame-rate mystery.
//
// ---------------------------------------------------------------------------
// STALE HANDLES
// ---------------------------------------------------------------------------
// A patch job can be in flight when its slot is reused -- the whole point of
// prefetching is that residency changes while work is outstanding. A handle is
// therefore {slot, generation}, and the generation advances on every claim.
// `check_*` validates a handle against the directory: a job holding a stale
// handle is told so and must be abandoned, never silently pointed at whatever
// now occupies the slot.
//
// This is the same mechanism the resident palette uses, for the same reason,
// and the same reason a valid bit will not do: clearing valid makes in-flight
// work miss (slow but correct), leaving it set makes in-flight work read
// ANOTHER PATCH'S GROUND (fast and catastrophically wrong -- terrain from one
// island appearing inside another).
// ---------------------------------------------------------------------------
`default_nettype none

module zhao_terrain_residency #(
    // 1,024 resident patches, from the report's memory budget: 4.19 km2 of
    // registered ground at 2 m pitch, 16.8 km2 at 4 m.
    parameter int unsigned SLOTS = 1024,
    // Patch coordinates. fx16 world is about +/-32 km and a patch is 64 m, so
    // +/-512 patches needs 10 bits and a sign. 12 gives headroom without
    // pretending to a range the coordinate system does not have.
    parameter int unsigned PCW   = 12,
    parameter int unsigned GENW  = 4
) (
    input var logic clk,
    input var logic rst_n,

    // ---- lookup: is this patch resident? ------------------------------------
    input  var logic                     lu_valid_i,
    input  var logic signed [PCW-1:0]    lu_px_i,
    input  var logic signed [PCW-1:0]    lu_py_i,
    output var logic                     lu_valid_o,
    output var logic                     lu_hit_o,
    output var logic [$clog2(SLOTS)-1:0] lu_slot_o,
    output var logic [GENW-1:0]          lu_gen_o,

    // ---- claim: take a slot for this patch ----------------------------------
    // Returns the victim so the caller can write it back BEFORE the loader
    // overwrites it. A claim whose victim is dirty is the only path by which a
    // permanent scar can be lost, so the eviction is reported, not implied.
    input  var logic                     cl_valid_i,
    input  var logic signed [PCW-1:0]    cl_px_i,
    input  var logic signed [PCW-1:0]    cl_py_i,
    output var logic                     cl_valid_o,
    output var logic [$clog2(SLOTS)-1:0] cl_slot_o,
    output var logic [GENW-1:0]          cl_gen_o,
    output var logic                     cl_evicted_o,       // a page was displaced
    output var logic                     cl_evicted_dirty_o, // ...and it had scars
    output var logic signed [PCW-1:0]    cl_evicted_px_o,
    output var logic signed [PCW-1:0]    cl_evicted_py_o,

    // ---- the loader says a page is complete ---------------------------------
    input  var logic                     fin_valid_i,
    input  var logic [$clog2(SLOTS)-1:0] fin_slot_i,
    input  var logic [GENW-1:0]          fin_gen_i,

    // ---- deformation marks a page dirty -------------------------------------
    input  var logic                     dirty_valid_i,
    input  var logic [$clog2(SLOTS)-1:0] dirty_slot_i,
    input  var logic [GENW-1:0]          dirty_gen_i,

    // ---- handle check: is this in-flight job still looking at its own page? --
    input  var logic                     chk_valid_i,
    input  var logic [$clog2(SLOTS)-1:0] chk_slot_i,
    input  var logic [GENW-1:0]          chk_gen_i,
    output var logic                     chk_valid_o,
    output var logic                     chk_stale_o,

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0]              hits_o,
    output var logic [31:0]              misses_o,
    output var logic [31:0]              evictions_o,
    output var logic [31:0]              dirty_evictions_o,  // scars needing writeback
    output var logic [31:0]              collisions_o,       // displaced a LIVE page
    output var logic [31:0]              stale_handles_o,
    output var logic [15:0]              resident_o
);

  localparam int SW = $clog2(SLOTS);
  // 1024 slots = 32 x 32 of the patch's own low bits.
  localparam int HALF = SW / 2;

  // ---- the directory --------------------------------------------------------
  logic                  res_r   [SLOTS];  // a page is present
  logic                  load_r  [SLOTS];  // ...and the loader has finished it
  logic                  dirty_r [SLOTS];  // ...and it carries scars
  logic signed [PCW-1:0] px_r    [SLOTS];
  logic signed [PCW-1:0] py_r    [SLOTS];
  logic [GENW-1:0]       gen_r   [SLOTS];

  // Direct map on the low coordinate bits. A pure function of the key, which
  // is what makes eviction deterministic.
  // The HIGH bits are deliberately unused: that is what "direct-mapped" means
  // here, and they are not lost -- px_r/py_r store the FULL key, so a hit
  // requires the whole coordinate to match. Dropping the high bits from the
  // INDEX is the mapping; dropping them from the TAG would alias two islands
  // onto one another silently.
  /* verilator lint_off UNUSEDSIGNAL */
  function automatic logic [SW-1:0] slot_of(input logic signed [PCW-1:0] px,
                                            input logic signed [PCW-1:0] py);
    slot_of = {py[HALF-1:0], px[HALF-1:0]};
  endfunction
  /* verilator lint_on UNUSEDSIGNAL */

  logic [SW-1:0] lu_slot_c, cl_slot_c;
  assign lu_slot_c = slot_of(lu_px_i, lu_py_i);
  assign cl_slot_c = slot_of(cl_px_i, cl_py_i);

  // A hit needs the page present, LOADED, and the key to match. Present but
  // not loaded is a page still streaming: reporting it resident would hand a
  // patch job a half-written lattice.
  logic lu_hit_c;
  assign lu_hit_c = res_r[lu_slot_c] && load_r[lu_slot_c]
                 && (px_r[lu_slot_c] == lu_px_i) && (py_r[lu_slot_c] == lu_py_i);

  // A claim on the page already in the slot is a RE-claim, not an eviction.
  logic cl_same_c;
  assign cl_same_c = res_r[cl_slot_c]
                  && (px_r[cl_slot_c] == cl_px_i) && (py_r[cl_slot_c] == cl_py_i);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      lu_valid_o        <= 1'b0;
      cl_valid_o        <= 1'b0;
      chk_valid_o       <= 1'b0;
      hits_o            <= 32'd0;
      misses_o          <= 32'd0;
      evictions_o       <= 32'd0;
      dirty_evictions_o <= 32'd0;
      collisions_o      <= 32'd0;
      stale_handles_o   <= 32'd0;
      resident_o        <= 16'd0;
      for (int i = 0; i < SLOTS; i++) begin
        res_r[i]   <= 1'b0;
        load_r[i]  <= 1'b0;
        dirty_r[i] <= 1'b0;
        gen_r[i]   <= '0;
      end
    end else begin
      // ---- lookup ---------------------------------------------------------
      lu_valid_o <= lu_valid_i;
      if (lu_valid_i) begin
        lu_hit_o  <= lu_hit_c;
        lu_slot_o <= lu_slot_c;
        lu_gen_o  <= gen_r[lu_slot_c];
        if (lu_hit_c) hits_o   <= hits_o + 32'd1;
        else          misses_o <= misses_o + 32'd1;
      end

      // ---- claim ----------------------------------------------------------
      cl_valid_o <= cl_valid_i;
      if (cl_valid_i) begin
        cl_slot_o          <= cl_slot_c;
        cl_evicted_px_o    <= px_r[cl_slot_c];
        cl_evicted_py_o    <= py_r[cl_slot_c];
        cl_evicted_o       <= res_r[cl_slot_c] && !cl_same_c;
        cl_evicted_dirty_o <= res_r[cl_slot_c] && !cl_same_c && dirty_r[cl_slot_c];

        if (cl_same_c) begin
          // Re-claiming the page already here. The generation does NOT advance:
          // handles held by jobs already working on this patch stay valid, which
          // is the entire point of distinguishing this case.
          cl_gen_o <= gen_r[cl_slot_c];
        end else begin
          gen_r[cl_slot_c]  <= gen_r[cl_slot_c] + 1'b1;
          cl_gen_o          <= gen_r[cl_slot_c] + 1'b1;
          px_r[cl_slot_c]   <= cl_px_i;
          py_r[cl_slot_c]   <= cl_py_i;
          // Present but NOT loaded: the loader has not filled it yet, so a
          // lookup must not call it resident.
          load_r[cl_slot_c] <= 1'b0;
          dirty_r[cl_slot_c] <= 1'b0;

          if (!res_r[cl_slot_c]) begin
            res_r[cl_slot_c] <= 1'b1;
            resident_o <= resident_o + 16'd1;
          end else begin
            // A LIVE page was displaced. This is the direct-mapped collision
            // the header warns about, counted so thrash is visible in a
            // capture rather than discovered as a frame-rate mystery.
            evictions_o  <= evictions_o + 32'd1;
            collisions_o <= collisions_o + 32'd1;
            if (dirty_r[cl_slot_c]) dirty_evictions_o <= dirty_evictions_o + 32'd1;
          end
        end
      end

      // ---- the loader finished a page ------------------------------------
      // Guarded by generation: a load that completes AFTER its slot was
      // re-claimed must not mark the new occupant loaded.
      if (fin_valid_i && res_r[fin_slot_i] && gen_r[fin_slot_i] == fin_gen_i)
        load_r[fin_slot_i] <= 1'b1;

      // ---- deformation ----------------------------------------------------
      if (dirty_valid_i && res_r[dirty_slot_i] && gen_r[dirty_slot_i] == dirty_gen_i)
        dirty_r[dirty_slot_i] <= 1'b1;

      // ---- handle check ---------------------------------------------------
      chk_valid_o <= chk_valid_i;
      if (chk_valid_i) begin
        automatic logic stale = !res_r[chk_slot_i] || (gen_r[chk_slot_i] != chk_gen_i);
        chk_stale_o <= stale;
        if (stale) stale_handles_o <= stale_handles_o + 32'd1;
      end
    end
  end

endmodule : zhao_terrain_residency

`default_nettype wire
