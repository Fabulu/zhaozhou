// zhao_terrain_place.sv -- TERRAIN.PLACE: the owner of lattice PLACEMENT.
//
// WHY THIS FILE EXISTS, and it is a composition story rather than an algorithm
// one. Entry I27 in `fpga/rtl/prod/zhao_console_core.sv` records the terrain
// compose engine as blocked, and is precise about the reason:
//
//     "THE BLOCKER IS PLACEMENT, and it is a missing owner rather than missing
//      wiring. TERRAIN.PATCH needs `wx_i`/`wz_i`, the PLACED world x and z of
//      the lattice vertex, and TERRAIN.COMPCACHE needs the same 33 column x's
//      and 33 row z's through its `pos_*` write port. TERRAIN.PAGESTREAM emits
//      the heights and the lattice indices and NOT the placement; nothing else
//      in the tree emits it either. Deriving it here from the index, the patch
//      coordinate and a pitch is arithmetic invented in the composer, which
//      this file does not do."
//
// That refusal was right, and the fix it implies is this file: the arithmetic
// is not invented, it is RATIFIED, and what it lacked was a named owner with a
// contract and a test rather than a few lines inlined into a composer. The
// seams themselves already meet -- I27 says so -- so this block is the whole
// remaining distance between TERRAIN.PATCH, TERRAIN.COMPCACHE and TERRAIN.TESS.
//
// ---------------------------------------------------------------------------
// THE LAW, AND WHERE IT IS FROZEN
// ---------------------------------------------------------------------------
// spec/terrain_rules.md 1.3 freezes cell pitch to {0.5, 1.0, 2.0, 4.0} m,
// encoded `pitch_log2 in {-1, 0, +1, +2}`, and says why in as many words:
//
//     "Powers of two make world->cell lookup a shift and keep every lattice
//      x/z exactly representable in fx16 -- no division, no rounding, anywhere
//      in the addressing path."
//
// 1.3 also names 2.0 m the canonical battlefield pitch, and 2.1 fixes the patch
// at 32x32 cells on a 33x33 vertex lattice. The placement of lattice column `i`
// of patch `patch_ix` is therefore
//
//     wx(i) = (patch_ix * 32 + i) * pitch
//
// and with pitch = 2^pitch_log2 in metres and fx16 carrying 16 fractional bits
// (spec/qformats.md: fx16 is s32, S 1.15.16), that is EXACTLY
//
//     wx(i) = (patch_ix * 32 + i) <<< (16 + pitch_log2)
//
// with no multiplier and no rounding anywhere. The `* 32` folds into the same
// shift, which is why this block costs shifts and an adder and nothing else.
// Rows are the identical law in z with `patch_iz` and `j`.
//
// ---------------------------------------------------------------------------
// THE ENVELOPE IS AN INPUT AND IT IS CHECKED. THAT IS THE SPEC'S IDEA, NOT MINE
// ---------------------------------------------------------------------------
// The patch header (spec/terrain_rules.md 2.1, +16) carries
//
//     rectfx envelope  x0,z0,x1,z1 (fx16 world, island-datum-relative; must
//                      equal origin + coords x 32 x pitch exactly -- the
//                      packer asserts it; redundancy is a corruption check)
//
// So the header states the answer this block computes, and the spec says
// plainly what the duplication is FOR. Two wrong ways to use that:
//
//   * trust the envelope and skip the arithmetic -- then a corrupt header
//     silently places a whole patch somewhere else, and nothing notices;
//   * compute and ignore the envelope -- then the corruption check the format
//     was designed around is simply not performed by anybody.
//
// This block does both and compares them. `place_env_mismatch_o` is that
// comparison, and it is a REAL detector rather than a hopeful zero: its two
// operands come from genuinely independent sources -- one from the page header
// on the memory path, one from this block's own shifter -- so it is not the
// shape CLAUDE.md warns about, where a single register enable moves both sides
// of a comparison together and the checker is structurally blind. It is fired
// deliberately in `tests/terrain/terrain_place_directed.cpp`.
//
// ---------------------------------------------------------------------------
// WHY A 4-WAY SELECT AND NOT A BARREL SHIFTER
// ---------------------------------------------------------------------------
// `pitch_log2` is an input, so `x <<< (16 + pitch_log2)` reads as a variable
// shift and infers a barrel shifter. It has FOUR legal values. A 4-way mux over
// four constant shifts is the same function for every legal input, costs a mux
// instead of a shifter network, and makes the illegal cases visible rather than
// quietly producing an address. ALMs are the binding constraint on this device
// (the console measured 47,582 against a 41,910 budget), so spending a barrel
// shifter to express four constants would be a real cost for no function.
//
// An out-of-range `pitch_log2` does NOT silently pick a default: it refuses the
// patch and moves `place_pitch_bad_o`. A default here would place terrain at the
// wrong scale, which is the kind of wrong that looks like a content bug forever.
//
// ---------------------------------------------------------------------------
// RANGE. fx16 IS s32 AND THE WORLD IS NOT INFINITE
// ---------------------------------------------------------------------------
// `patch_ix` is i16 and the shift reaches 18, so the product needs up to 40
// bits while fx16 holds 32. That is a real overflow, not a theoretical one: a
// patch coordinate near the i16 extreme at 4 m pitch leaves the representable
// world. It is DETECTED and the patch is refused (`place_range_o`), because a
// wrapped placement puts a patch on the opposite side of the world and every
// downstream block would treat that as legitimate geometry. Saturating would be
// worse than refusing -- it would pile patches on the world edge and look like
// terrain.

`default_nettype none

module zhao_terrain_place
  // THE PLACEMENT ARITHMETIC MOVED OUT, 2026-09-25 (EDGEPREP), AND NOTHING
  // ABOUT IT CHANGED. `place32`, `units_fits`, `units_of` and the four pitch
  // shifts now live in `zhao_terrain_place_law_pkg` so that
  // `zhao_terrain_prepwalk`'s PREPARE pass and this block's EMIT-side fill
  // call ONE definition. The alternative -- a second copy in the walker -- is
  // the "second implementation of the ratified arithmetic" that
  // TERRAIN.EDGERECON.md refuses by name, and a copy is a thing that goes
  // stale in the flattering direction (CLAUDE.md, "a committed mutant is a
  // COPY").  No port changed and no behaviour changed: this block is still the
  // only PROVIDER of a placed patch, and still owns header acceptance, the
  // envelope check, the pitch refusal, the census and the 66-write fill.
  import zhao_terrain_place_law_pkg::*;
#(
    // 33x33 vertex lattice, spec/terrain_rules.md 2.1 / charter 11.1.
    parameter int unsigned LAT_W = 33,
    parameter int unsigned LAT_H = 33,

    // Census widths. Both SATURATE; a wrapping census reads zero after 2**W
    // events, which is the flattering direction.
    parameter int unsigned CENSUS_W = 16
) (
    input  var logic clk,
    input  var logic rst_n,

    // -----------------------------------------------------------------------
    // the patch header, from TERRAIN.PAGESTREAM's page header fields
    // -----------------------------------------------------------------------
    input  var logic               hdr_valid_i,
    output var logic               hdr_ready_o,
    input  var logic signed [ 7:0] hdr_pitch_log2_i,   // -1..+2, spec 2.1 +2
    input  var logic signed [15:0] hdr_patch_ix_i,     // spec 2.1 +8
    input  var logic signed [15:0] hdr_patch_iz_i,     // spec 2.1 +10
    // The header's own redundant statement of the same placement, fx16.
    // Checked, never trusted; see the header block above.
    input  var logic signed [31:0] hdr_env_x0_i,       // spec 2.1 +16
    input  var logic signed [31:0] hdr_env_z0_i,
    input  var logic        [15:0] hdr_src_id_i,

    // -----------------------------------------------------------------------
    // COMPCACHE's position fill: 33 column x's then 33 row z's.
    // Port-for-port with zhao_terrain_compcache_front's `pos_*` input.
    // -----------------------------------------------------------------------
    output var logic               pos_we_o,
    output var logic               pos_axis_o,   // 0 = wx by column, 1 = wz by row
    output var logic        [ 5:0] pos_idx_o,
    output var logic signed [31:0] pos_val_o,
    output var logic               pos_done_o,   // one-cycle pulse: 66 writes issued

    // -----------------------------------------------------------------------
    // TERRAIN.PATCH's per-vertex placement.
    //
    // COMBINATIONAL BY DESIGN and that is the cheap choice, not a shortcut.
    // The law is a shift of the index, so answering `(vi, vj)` needs no storage
    // whatever -- a registered version would need a 33-entry x plus 33-entry z
    // table and a read port, to hold values that cost an adder to recompute.
    // It rides alongside TERRAIN.PATCH's own `vtx_valid_i`/`vi_i`/`vj_i` and
    // introduces no handshake of its own, so it cannot stall that lane.
    // -----------------------------------------------------------------------
    input  var logic        [ 5:0] vtx_vi_i,
    input  var logic        [ 5:0] vtx_vj_i,
    output var logic signed [31:0] vtx_wx_o,
    output var logic signed [31:0] vtx_wz_o,
    output var logic               vtx_placed_o, // low = this patch was refused

    // -----------------------------------------------------------------------
    // verdict and census (all SATURATING)
    // -----------------------------------------------------------------------
    output var logic                    place_valid_o,       // a patch is placed
    output var logic [CENSUS_W-1:0]     place_env_mismatch_o,
    output var logic [CENSUS_W-1:0]     place_pitch_bad_o,
    output var logic [CENSUS_W-1:0]     place_range_o,
    output var logic [CENSUS_W-1:0]     place_patches_o,
    // The placed patch's source id, so a downstream trace can name the page.
    output var logic [15:0]             place_src_id_o
);

  // ==========================================================================
  // ELABORATION GUARDS.
  // Quartus 17.0 rejects a bare module-scope `if`; it must sit inside an
  // `initial begin ... end` (CLAUDE.md, the two SystemVerilog forms that lint
  // clean and do not synthesise).
  // ==========================================================================
  initial begin
    if (LAT_W != 33 || LAT_H != 33) begin
      $fatal(1, "zhao_terrain_place: spec/terrain_rules.md 2.1 fixes the lattice at 33x33");
    end
    if (CENSUS_W < 1) begin
      $fatal(1, "zhao_terrain_place: CENSUS_W must be positive");
    end
  end

  // ==========================================================================
  // THE FOUR LEGAL PITCHES -- now in `zhao_terrain_place_law_pkg`.
  // ==========================================================================
  // SH_HALF/SH_ONE/SH_TWO/SH_FOUR and UNITS_W come in through the import on
  // the module line. They are not redeclared here, because two declarations of
  // one constant is how the two callers come to disagree.

  // Latched header.
  logic signed [ 7:0] pitch_q;
  logic signed [15:0] ix_q, iz_q;
  logic        [15:0] src_q;
  logic               placed_q;     // a patch is currently placed and legal

  wire signed [7:0] pitch_c = hdr_valid_i ? hdr_pitch_log2_i : pitch_q;
  // `pitch_legal` is the package's, so the ISLAND-DESCRIPTOR pitch that owner
  // directive section 2 makes authoritative is tested by the identical
  // predicate a PAGE-HEADER pitch is tested by here.
  wire pitch_ok_c = pitch_legal(pitch_c);

  // ==========================================================================
  // THE PLACEMENT SHIFTER -- now in `zhao_terrain_place_law_pkg`.
  // ==========================================================================
  // `place32`, `units_fits` and `units_of` were defined here and are now
  // imported. THE ARITHMETIC IS UNCHANGED, character for character; only its
  // home moved, so that `zhao_terrain_prepwalk` calls the SAME function
  // rather than a copy of it. TERRAIN.EDGERECON.md's requirement is that
  // PREPARE and EMIT be "bit-identical BY CONSTRUCTION rather than by two
  // implementations agreeing"; one definition is what makes that structural.
  //
  // `units` is (patch_coord * 32 + index) -- the lattice vertex counted in
  // CELLS from the island datum -- and UNITS_W is 22 for the reason the
  // package records. Neither is redeclared here.

  // ==========================================================================
  // HEADER ACCEPTANCE, AND THE TWO REFUSALS
  // ==========================================================================
  // The envelope check uses index 0 of each axis: the patch ORIGIN is exactly
  // what spec 2.1 says the envelope's x0/z0 carry.
  // The FAR corner is the widest value this patch will ever place, so testing
  // it is what makes the whole patch safe rather than just its origin.
  wire range_ok_c =
        units_fits(units_of(hdr_patch_ix_i, 6'd0),            pitch_c)
     && units_fits(units_of(hdr_patch_iz_i, 6'd0),            pitch_c)
     && units_fits(units_of(hdr_patch_ix_i, 6'(LAT_W - 1)),   pitch_c)
     && units_fits(units_of(hdr_patch_iz_i, 6'(LAT_H - 1)),   pitch_c);

  wire signed [31:0] org_x_c = place32(units_of(hdr_patch_ix_i, 6'd0), pitch_c);
  wire signed [31:0] org_z_c = place32(units_of(hdr_patch_iz_i, 6'd0), pitch_c);

  wire env_ok_c = (org_x_c == hdr_env_x0_i) && (org_z_c == hdr_env_z0_i);

  // A header is ACCEPTED (retired from the producer) whatever the verdict --
  // refusing to retire it would stall the page path behind a page that will
  // never become valid. It is PLACED only when all three tests pass.
  assign hdr_ready_o = 1'b1;

  wire accept_c = hdr_valid_i && pitch_ok_c && range_ok_c && env_ok_c;

  // ==========================================================================
  // THE COMPCACHE FILL SEQUENCER
  // ==========================================================================
  // 33 column x's on axis 0, then 33 row z's on axis 1, in index order. The
  // order is DECLARED and deterministic for the same reason FORGE.PRIM's
  // emission order is: two orderings make the same picture and different
  // capture CRCs.
  localparam logic [1:0] S_IDLE = 2'd0;
  localparam logic [1:0] S_X    = 2'd1;
  localparam logic [1:0] S_Z    = 2'd2;
  localparam logic [1:0] S_DONE = 2'd3;

  logic [1:0] st_q;
  logic [5:0] fill_idx_q;

  wire signed [31:0] fill_x_c = place32(units_of(ix_q, fill_idx_q), pitch_q);
  wire signed [31:0] fill_z_c = place32(units_of(iz_q, fill_idx_q), pitch_q);

  assign pos_we_o   = (st_q == S_X) || (st_q == S_Z);
  assign pos_axis_o = (st_q == S_Z);
  assign pos_idx_o  = fill_idx_q;
  assign pos_val_o  = (st_q == S_Z) ? fill_z_c : fill_x_c;
  assign pos_done_o = (st_q == S_DONE);

  // ==========================================================================
  // TERRAIN.PATCH's per-vertex answer
  // ==========================================================================
  assign vtx_wx_o     = place32(units_of(ix_q, vtx_vi_i), pitch_q);
  assign vtx_wz_o     = place32(units_of(iz_q, vtx_vj_i), pitch_q);
  assign vtx_placed_o = placed_q;
  assign place_valid_o  = placed_q;
  assign place_src_id_o = src_q;

  // ==========================================================================
  // SEQUENTIAL
  // ==========================================================================
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      pitch_q    <= 8'sd0;
      ix_q       <= 16'sd0;
      iz_q       <= 16'sd0;
      src_q      <= 16'd0;
      placed_q   <= 1'b0;
      st_q       <= S_IDLE;
      fill_idx_q <= 6'd0;

      place_env_mismatch_o <= '0;
      place_pitch_bad_o    <= '0;
      place_range_o        <= '0;
      place_patches_o      <= '0;
    end else begin
      // ---- census, saturating ---------------------------------------------
      if (hdr_valid_i && !pitch_ok_c && !(&place_pitch_bad_o)) begin
        place_pitch_bad_o <= place_pitch_bad_o + 1'b1;
      end
      // Range and envelope are only meaningful once the pitch is legal: with an
      // illegal pitch the shifter returns zero and both tests would report a
      // second opinion about the same one fault.
      if (hdr_valid_i && pitch_ok_c && !range_ok_c && !(&place_range_o)) begin
        place_range_o <= place_range_o + 1'b1;
      end
      if (hdr_valid_i && pitch_ok_c && range_ok_c && !env_ok_c
          && !(&place_env_mismatch_o)) begin
        place_env_mismatch_o <= place_env_mismatch_o + 1'b1;
      end
      if (accept_c && !(&place_patches_o)) begin
        place_patches_o <= place_patches_o + 1'b1;
      end

      // ---- header latch ----------------------------------------------------
      if (hdr_valid_i) begin
        if (accept_c) begin
          pitch_q  <= hdr_pitch_log2_i;
          ix_q     <= hdr_patch_ix_i;
          iz_q     <= hdr_patch_iz_i;
          src_q    <= hdr_src_id_i;
          placed_q <= 1'b1;
        end else begin
          // A REFUSED patch UNPLACES the block. Leaving the previous patch's
          // placement standing would place this page's vertices at the last
          // page's coordinates, which is the worst of the three outcomes:
          // plausible geometry in the wrong place.
          placed_q <= 1'b0;
        end
      end

      // ---- the fill sequencer ---------------------------------------------
      case (st_q)
        S_IDLE: begin
          if (accept_c) begin
            st_q       <= S_X;
            fill_idx_q <= 6'd0;
          end
        end
        S_X: begin
          if (fill_idx_q == 6'(LAT_W - 1)) begin
            st_q       <= S_Z;
            fill_idx_q <= 6'd0;
          end else begin
            fill_idx_q <= fill_idx_q + 6'd1;
          end
        end
        S_Z: begin
          if (fill_idx_q == 6'(LAT_H - 1)) begin
            st_q       <= S_DONE;
            fill_idx_q <= 6'd0;
          end else begin
            fill_idx_q <= fill_idx_q + 6'd1;
          end
        end
        default: begin   // S_DONE, one cycle
          st_q <= S_IDLE;
        end
      endcase
    end
  end

`ifdef ZHAO_ASSERT
  // The law, asserted directly: canonical 2.0 m pitch places column i of patch
  // 0 at exactly i * 2.0 m. Independent of the shifter's implementation.
  a_canonical_pitch: assert property (@(posedge clk) disable iff (!rst_n)
      (placed_q && (pitch_q == 8'sd1) && (ix_q == 16'sd0))
      |-> (vtx_wx_o == (32'sd131072 * 32'(vtx_vi_i))))
    else $error("zhao_terrain_place: 2.0 m pitch placement is not i * 2.0 m");
`endif

endmodule

`default_nettype wire
