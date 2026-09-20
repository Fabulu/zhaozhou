// zhao_terrain_pageio.sv -- TERRAIN.BAKE's page window: layer D in, layers B
// and D out, inside one resident TERRAIN.PAGE_POOL slot.
//
// Contract: design/contracts/TERRAIN.PAGEIO.md
// Ledger:   design/blocks.yml, id TERRAIN.PAGEIO
// Law:      spec/terrain_rules.md sec 2 (page layout), sec 3.3 (cell-state
//           byte and the no_bake corner shadow), sec 3.4 (breach law),
//           sec 7 (ownership), ruling T2 (the pool), ruling T4 (B and D are
//           never journalled).
// Offsets:  reference/include/zref/zref_terrain_page.hpp (kLayerBOff,
//           kLayerDOff) -- summed ONCE there, with static_asserts, because a
//           column summed by hand is a column that will one day be summed
//           differently.
//
// ---------------------------------------------------------------------------
// WHY IT EXISTS: FOUR ABSENT OWNERS THAT ARE ONE BLOCK
// ---------------------------------------------------------------------------
// `zhao_terrain_bake_v2` touches the page on FOUR ports and the machine served
// none of them. Re-verified on 2026-09-21 in this tree, not inherited:
//
//   * LAYER D HAS NO READER ANYWHERE. Layer D is at page offset 6,598. Every
//     `6598`/`6,598` under fpga/ is either the area figure "6,598 ALM" quoted
//     in three headers, or a COMMENT recording a previous instance of this
//     same search. `zhao_terrain_pagestream.sv` reads `'{A_OFF, B_OFF, C_OFF}`
//     -- three planes, and D is not one of them. `zhao_terrain_pageloader`
//     writes whole pages and reads none back. `zhao_terrain_writeback` touches
//     layer F only. So `vtx_nobake_i` and `cell_state_i` had no producer.
//   * LAYERS B AND D HAVE NO WRITER. `sc_*` (the scar) and `cs_*` (the cell
//     state) had no consumer at all. A bake whose scar cannot be stored has
//     not deformed anything.
//
// ONE BLOCK AND NOT TWO, because all four ports need the same three things:
// the residency SLOT, the GENERATION, and one guard socket on the page pool. A
// layer-D reader and a separate page writer would pay for the job port, the
// slot arithmetic and the verdict machinery twice, and would create a second
// place where a bake's identity can be got wrong. `zhao_terrain_bake_v2`
// processes ONE record at a time with strictly sequential phases, so one agent
// per bake is sufficient by construction.
//
// ---------------------------------------------------------------------------
// WHAT IT MUST NOT OWN (contract sec 3, and terrain_rules sec 7 permits exactly
// one owner of each law)
// ---------------------------------------------------------------------------
//   * It does NOT compose. `compose_top = max(fx(base)+fx(scar), fx(bottom))`
//     is TERRAIN.PATCH's, with `zref::terrain::compose_vertex` its oracle. The
//     same sentence is in `zhao_terrain_pagestream.sv`'s header.
//   * It does NOT evaluate the breach law. Section 3.4's four-corner equality
//     and the heal arm are TERRAIN.BAKE's.
//   * It does NOT clamp, rail or saturate. It moves bytes.
//   * It does NOT decide residency, publish, or verify the page CRC.
//
// The ONE law it owns is section 3.3's NO_BAKE CORNER SHADOW, because `nb_o` is
// a reduced bit and something must reduce it:
//
//     nb_o is the OR of cell_state[cj][ci] & kNoBakeBit over the up-to-four
//     cells ci in {vi-1, vi}, cj in {vj-1, vj} that lie inside [0,31]x[0,31].
//
// Its executed statement is `zref::terrain::nobake_corner_shadow`, lifted into
// the oracle on 2026-09-21 by this packet. It was previously an inline loop in
// `bake_dig()` AND a second inline loop in `tests/terrain/bake_dev.hpp` -- a
// TEST helper as the only standalone statement of a ratified law, which is
// exactly how a second implementation is born. Both now call the one function,
// and so does this file's `nbv_q` construction.
//
// THIS FILE BUILDS THE REDUCTION BY SCATTER, NOT BY GATHER, and they are the
// same law. The oracle GATHERS: for a vertex, look at its up-to-four corner
// cells. `nbv_q` is built by SCATTERING: for each protected cell, set the four
// vertices it corners. Every (vertex, cell) incidence is visited exactly once
// either way, so the OR is identical -- and the scatter is 1,024 single-cell
// steps with no multi-port read, while the gather would need four random reads
// of a 1,024-entry plane on every vertex. `tests/terrain/pageio_rtl_directed`
// asserts the two agree over a plane of pseudo-random no_bake bits, because
// "these are the same law" is an argument until something checks it.
//
// ---------------------------------------------------------------------------
// THE WRITE GRANULARITY IN THIS MACHINE IS A 64-BIT WORD, NOT A BYTE
// ---------------------------------------------------------------------------
// The contract's section 4 recommends byte enables: "Byte enables are
// preferable and the guard request type already carries `.be`". THE GUARD
// REFUSES A SPARSE `.be`, and this was measured rather than assumed:
//
//     zhao_mem_guard.sv:   be_ok    = (req.be == mask_of(req.len));
//                          shape_ok = len_ok && be_ok;
//
// with `mask_of` the FULL CONTIGUOUS mask over len bytes, and the guard's own
// header saying "partial-word masking is NOT in the Phase-2 arbiter".
// `zhao_vram_arbiter.sv` confirms it from the other side -- it converts len to
// WORDS (`words_of(client_req[k].len)`) and the SDRAM controller never sees a
// byte mask at all. A sparse `be` is a guard VIOLATION, not an optimisation.
//
// So the edge handling is a READ-MODIFY-WRITE, and every existing client of
// this guard (`zhao_terrain_pageloader`, `zhao_terrain_pagestream`,
// `zhao_terrain_writeback`) uses exactly one request shape: 64-byte aligned
// address, len 64, be all ones. This block uses that shape and nothing else.
//
// WHICH BYTES ARE FOREIGN, written out rather than assumed:
//
//   layer B = [2242, 4420).  Bursts 35..69 cover [2240, 4480).
//       [2240, 2242)  2 bytes  -- layer A's tail
//       [4420, 4480)  60 bytes -- layer C's head
//   layer D = [6598, 7622).  Bursts 103..119 cover [6592, 7680).
//       [6592, 6598)  6 bytes  -- layer C's tail
//       [7622, 7680)  58 bytes -- layer E's head
//
// Layer D's foreign bytes cost NOTHING: this block reads all seventeen of D's
// bursts to serve `cell_state_i`, so it already holds both edge bursts
// verbatim and writes them back unchanged. Layer B is never read for its own
// sake, so its TWO edge bursts (35 and 69) are read before the bake is served
// and merged in place -- two extra reads per bake, and the interior of the
// plane is overwritten by the bake in full.
//
// `pageio_rtl_directed` asserts that layers A, C and E are BYTE-IDENTICAL
// after a bake. That is the acceptance test for this whole section, and it is
// a silent corruption of layers nothing in the machine reads back yet, so
// nothing else would ever have caught it.
//
// ---------------------------------------------------------------------------
// ONE LAYER-D BUFFER, IN AND OUT (contract sec 5's "further halving")
// ---------------------------------------------------------------------------
// `dwrd_q` is BOTH the layer-D read buffer and the layer-D write buffer. The
// contract's argument is that the breach phase reads cell (ci,cj) and writes
// cell (ci,cj) in the same z-then-x order, strictly read-before-write, and that
// section 3.4's four-corner conjunction reads the `meets` bits from
// TERRAIN.BAKE's own private RAM rather than from a neighbouring cell's state
// -- so no cell is ever re-read after it is written.
//
// THAT IS AN ARGUMENT, AND `pageio_rtl_directed` TESTS IT rather than trusting
// it: the aliasing case is driven deliberately (a cs write to cell k followed
// by a cell read of k-1) and the read is asserted to return the ORIGINAL byte.
// An aliasing bug here changes only breach decisions, which is the exact fault
// class no counter can see.
//
// ---------------------------------------------------------------------------
// THE CELL READ CANNOT BE ALLOWED TO ANSWER A DIFFERENT ADDRESS
// ---------------------------------------------------------------------------
// This is CLAUDE.md's metadata-swap defect in its natural habitat, and it is
// designed out rather than counted. `cell_ci_i`/`cell_cj_i` are TERRAIN.BAKE's
// LIVE cursor, and they advance the moment bake accepts a cell -- while this
// block may still be holding `cell_valid_o` for the previous one. Delivering
// the held byte against the moved cursor would give bake cell A's state under
// cell B's address, with every handshake and every counter agreeing.
//
// So `cell_valid_o` is gated on `(cell_ci_i == cell_ci_q) && (cell_cj_i ==
// cell_cj_q)`, the address LATCHED WHEN THE READ WAS ISSUED. The two sides of
// that comparison are loaded by DIFFERENT enables -- the offered pair by bake,
// the latched pair by this block's read issue -- so the comparison is not the
// lockstep-blind kind. A cursor that moves under a pending read drops the
// stale answer, re-issues, and counts `cell_refetch_o`.
//
// Conservative SystemVerilog subset (charter sec 2). Quartus 17.0: every
// elaboration check is inside `initial begin ... end` and there are no
// implicit generates -- both forms lint clean under Verilator and are SYNTAX
// ERRORS in quartus_map.
`default_nettype none

module zhao_terrain_pageio
  import zhao_pkg::*;
#(
    // ---- THE PAGE LAYOUT (spec/terrain_rules.md sec 2) ---------------------
    // Named constants, not knobs: a different layout is a different format.
    // They are parameters only so a bench can shrink the transfer, and every
    // derived count below comes from them rather than from a second sum done
    // by hand. The values are `zref_terrain_page.hpp`'s.
    parameter int unsigned PAGE_BYTES  = 21376,
    parameter int unsigned B_OFF       = 2242,   // layer B, top scar delta
    parameter int unsigned B_BYTES     = 2178,   // 33x33 height16
    parameter int unsigned D_OFF       = 6598,   // layer D, cell state
    parameter int unsigned D_BYTES     = 1024,   // 32x32 u8
    parameter int unsigned BURST_BYTES = 64,

    // ---- TERRAIN.PAGE_POOL (ruling T2) -------------------------------------
    // PARAMETERS, not hard-coded constants, for TERRAIN.WRITEBACK's reason:
    // the pool can move to any unmapped range, and a block that hard-codes an
    // address the guard also hard-codes gives the owner one knob with two
    // halves that must move together and no gate that says so.
    parameter logic [ZHAO_VRAM_ADDR_BITS-1:0] REGION_BASE  = 27'h400_0000,
    parameter int unsigned                    REGION_SLOTS = 1024,

    // ONE BIT WIDER THAN THE POOL NEEDS, TERRAIN.PAGELOADER's reason: at
    // exactly $clog2(1024) = 10 bits a slot index CANNOT express 1024, so a
    // producer that computed a bad slot would arrive TRUNCATED -- slot 1024
    // presenting as slot 0 -- and this block would write a LIVE page's scar
    // under an evicted page's key. A refusal is not a clamp.
    parameter int unsigned SLOTW = $clog2(REGION_SLOTS) + 1,
    parameter int unsigned GENW  = 8,   // ruling T10: "generation u8 minimum"

    // ---- THE LATTICE (Island Patch v1, frozen) -----------------------------
    parameter int unsigned EDGE  = 33,  // lattice vertices per side
    parameter int unsigned CELLS = 32,  // cells per side = EDGE - 1

    // Section 3.3's cell-state byte: bit 2 is kNoBakeBit. A parameter so the
    // bit can be re-sited with the spec rather than hunted for in a datapath.
    parameter int unsigned NOBAKE_BIT = 2
) (
    input var logic clk,
    input var logic rst_n,

    // ---- configuration -----------------------------------------------------
    input var zhao_client_e cfg_vram_client_i,  // ZHAO_CLIENT_TERRAIN_BUILD
    input var logic [31:0]  cfg_epoch_i,        // the live resource_epoch

    // ---- job in: the patch this bake is against ----------------------------
    input  var logic             j_valid_i,
    output var logic             j_ready_o,
    input  var logic [SLOTW-1:0] j_slot_i,
    input  var logic [GENW-1:0]  j_gen_i,
    input  var logic [31:0]      j_epoch_i,
    input  var logic [31:0]      j_src_id_i,

    // ---- MEM.GUARD client: READ and WRITE on the page pool -----------------
    // One requester on the existing write-capable share (`u_build_share`,
    // `zhao_mem_share_wr`), which already carries both directions per
    // requester. No guard change, no new client id, no new region: MEM.GUARD
    // already admits ZHAO_CLIENT_TERRAIN_BUILD on this pool in BOTH directions
    // (`terrain_ok` requires req.write, `terrain_rd_ok` requires !req.write).
    output var zhao_guard_req_t guard_req_o,
    input  var zhao_guard_rsp_t guard_rsp_i,
    input  var logic            beat_valid_i,
    input  var logic [63:0]     beat_data_i,
    input  var logic            beat_last_i,
    output var logic [63:0]     guard_wdata_o,
    output var logic            guard_wvalid_o,
    input  var logic            guard_wready_i,
    output var logic            guard_wlast_o,

    // ---- the BAKE face: layer D read, DIG phase ----------------------------
    // `nb_vi_i`/`nb_vj_i` are TERRAIN.BAKE's `vtx_vi_o`/`vtx_vj_o`; `nb_o` is
    // its `vtx_nobake_i`. COMBINATIONAL, because the page server delivers
    // base/scar/bottom/nobake on ONE beat or the vertex is not ready, and this
    // block does not own that beat's valid in contract form (b).
    input  var logic [5:0] nb_vi_i,
    input  var logic [5:0] nb_vj_i,
    input  var logic       nb_req_i,
    output var logic       nb_o,

    // ---- the BAKE face: layer D read, BREACH phase -------------------------
    input  var logic [5:0] cell_ci_i,     // <- bake.cell_ci_o (LIVE cursor)
    input  var logic [5:0] cell_cj_i,     // <- bake.cell_cj_o
    output var logic       cell_valid_o,  // -> bake.cell_valid_i
    input  var logic       cell_ready_i,  // <- bake.cell_ready_o
    output var logic [7:0] cell_state_o,  // -> bake.cell_state_i (FULL BYTE)

    // ---- the BAKE face: layer B write --------------------------------------
    input  var logic               sc_valid_i,
    output var logic               sc_ready_o,
    input  var logic signed [15:0] sc_scar_i,
    input  var logic [5:0]         sc_vi_i,
    input  var logic [5:0]         sc_vj_i,

    // ---- the BAKE face: layer D write --------------------------------------
    // THE FULL BYTE, flags included. The seam the core names at entry I32 --
    // bake's cs_* onto `zhao_terrain_compcache_front` -- carries the SUBSTANCE
    // ONLY (`logic [1:0] sub_m`), so through it the upper six bits including
    // kNoBakeBit are DROPPED. Both consumers are wanted; they are different
    // consumers, and only this one discharges section 7.
    input  var logic       cs_valid_i,
    output var logic       cs_ready_o,
    input  var logic [7:0] cs_state_i,
    input  var logic [5:0] cs_ci_i,
    input  var logic [5:0] cs_cj_i,

    // ---- retirement --------------------------------------------------------
    input var logic dig_done_i,   // <- bake.dig_done_o
    input var logic bake_done_i,  // <- bake.bake_done_o

    // ---- THE DEFORMATION MARK (entry I27's first half) ---------------------
    // This block is the only thing that holds the slot and generation the
    // patch was served under AND learns from `bake_done_i` that the bake
    // finished, so it is the natural -- and on present evidence the only --
    // writer of the directory's `terr_dm_*`.
    output var logic             dm_valid_o,
    input  var logic             dm_ready_i,
    output var logic [SLOTW-1:0] dm_slot_o,
    output var logic [GENW-1:0]  dm_gen_o,
    output var logic [31:0]      dm_epoch_o,
    output var logic             dm_bd_o,    // layers B and D are dirty
    output var logic             dm_f_o,     // see OWNER DECISION 3 below
    output var logic             dm_mips_o,  // see OWNER DECISION 3 below

    // ---- completion --------------------------------------------------------
    output var logic             done_valid_o,
    input  var logic             done_ready_i,
    output var logic             done_ok_o,
    output var logic [3:0]       done_verdict_o,
    output var logic [SLOTW-1:0] done_slot_o,
    output var logic [31:0]      done_src_id_o,

    // ---- counters ----------------------------------------------------------
    output var logic [31:0] pages_read_o,
    output var logic [31:0] pages_written_o,
    output var logic [31:0] guard_denied_o,
    output var logic [31:0] bursts_read_o,
    output var logic [31:0] bursts_written_o,
    output var logic [31:0] stale_gen_o,
    output var logic [31:0] marks_emitted_o,
    output var logic [31:0] jobs_refused_o,
    // A cs write whose kNoBakeBit DISAGREES with the plane this block read.
    // TERRAIN.BAKE preserves bits 7:2 (`cs_state_o <= {cell_state_i[7:2],
    // sub_out}`), so under a legal bake this is UNREACHABLE and must read
    // zero. It is fired deliberately by stimulus in `pageio_rtl_directed`,
    // because a detector reading zero is a claim and it is the claim to check
    // hardest.
    output var logic [31:0] nobake_mutated_o,
    // A pending cell read whose answer was dropped because bake's cursor moved
    // underneath it. Zero in scan order; fired by stimulus.
    output var logic [31:0] cell_refetch_o,
    output var logic        idle_o
);

  // ==========================================================================
  // DERIVED LAYOUT -- every count from the parameters, never a second sum
  // ==========================================================================
  localparam int unsigned BEATS = BURST_BYTES / 8;  // 8

  localparam int unsigned VERTS = EDGE * EDGE;    // 1,089
  localparam int unsigned NCELL = CELLS * CELLS;  // 1,024

  // Layer B's aligned burst window.
  localparam int unsigned B_BURST_LO = B_OFF / BURST_BYTES;                   // 35
  localparam int unsigned B_BURST_HI = (B_OFF + B_BYTES - 1) / BURST_BYTES;   // 69
  localparam int unsigned B_NBURST   = B_BURST_HI - B_BURST_LO + 1;           // 35
  localparam int unsigned B_BASE     = B_BURST_LO * BURST_BYTES;              // 2,240
  localparam int unsigned B_LEAD     = B_OFF - B_BASE;                        // 2
  localparam int unsigned B_WORDS    = B_NBURST * BEATS;                      // 280

  // Layer D's aligned burst window.
  localparam int unsigned D_BURST_LO = D_OFF / BURST_BYTES;                   // 103
  localparam int unsigned D_BURST_HI = (D_OFF + D_BYTES - 1) / BURST_BYTES;   // 119
  localparam int unsigned D_NBURST   = D_BURST_HI - D_BURST_LO + 1;           // 17
  localparam int unsigned D_BASE     = D_BURST_LO * BURST_BYTES;              // 6,592
  localparam int unsigned D_LEAD     = D_OFF - D_BASE;                        // 6
  localparam int unsigned D_WORDS    = D_NBURST * BEATS;                      // 136

  localparam int unsigned BWA = $clog2(B_WORDS);
  localparam int unsigned DWA = $clog2(D_WORDS);
  localparam int unsigned VIX = $clog2(VERTS);
  localparam int unsigned CIX = $clog2(NCELL + 1);

  // ---- verdicts (pagestream's codes, extended) -----------------------------
  localparam logic [3:0] V_OK         = 4'd0;
  localparam logic [3:0] V_SLOT_OOR   = 4'd1;  // slot >= REGION_SLOTS
  localparam logic [3:0] V_EPOCH      = 4'd2;  // job epoch != cfg_epoch_i
  localparam logic [3:0] V_GUARD      = 4'd3;  // MEM.GUARD refused
  localparam logic [3:0] V_INCOMPLETE = 4'd4;  // a burst returned short
  localparam logic [3:0] V_SHORT_B    = 4'd5;  // bake did not fill layer B
  localparam logic [3:0] V_SHORT_D    = 4'd6;  // bake part-filled layer D

`ifndef SYNTHESIS
  // THE LAYOUT ARITHMETIC, ENFORCED AT ELABORATION. The contract says "Check
  // it at elaboration rather than trusting it", and CLAUDE.md records that
  // `--lint-only` does NOT run `initial` blocks -- so a clean lint says
  // nothing whatever about these. They are exercised by the directed test's
  // parameterised build, which ELABORATES.
  //
  // Quartus 17.0 requires this inside `initial begin ... end`: a bare
  // module-scope `if (...) $fatal(...)` lints clean under Verilator with zero
  // diagnostics and fails quartus_map with "syntax error near text: `if`".
  initial begin
    if (BURST_BYTES % 8 != 0)
      $fatal(1, "pageio: BURST_BYTES must be whole 64-bit beats (got %0d)", BURST_BYTES);
    if (B_BYTES != VERTS * 2)
      $fatal(1, "pageio: layer B must be %0d vertices x 2 bytes (got %0d)", VERTS, B_BYTES);
    if (D_BYTES != NCELL)
      $fatal(1, "pageio: layer D must be %0d cells x 1 byte (got %0d)", NCELL, D_BYTES);
    if (CELLS + 1 != EDGE)
      $fatal(1, "pageio: CELLS+1 must equal EDGE (%0d, %0d)", CELLS, EDGE);
    // The contract's section 4 arithmetic, restated as a check rather than a
    // belief: D spans 17 bursts covering [6592, 7680), B spans 35 covering
    // [2240, 4480).
    if (D_NBURST != 17 && D_OFF == 6598 && BURST_BYTES == 64)
      $fatal(1, "pageio: layer D must span 17 bursts (got %0d)", D_NBURST);
    if (B_NBURST != 35 && B_OFF == 2242 && BURST_BYTES == 64)
      $fatal(1, "pageio: layer B must span 35 bursts (got %0d)", B_NBURST);
    // A 16-bit sample must never straddle a 64-bit word: B_OFF even is the
    // whole argument, and it is `zhao_terrain_pagestream.sv`'s own.
    if ((B_OFF % 2) != 0)
      $fatal(1, "pageio: B_OFF is odd (%0d) -- a 16-bit scar would straddle a word", B_OFF);
    if ((B_OFF + B_BYTES) > D_OFF)
      $fatal(1, "pageio: layer B runs into layer D");
    if ((D_OFF + D_BYTES) > PAGE_BYTES)
      $fatal(1, "pageio: layer D runs past the page");
    if (NOBAKE_BIT > 7)
      $fatal(1, "pageio: NOBAKE_BIT must be inside the cell-state byte");
  end
`endif

  // ==========================================================================
  // slot * PAGE_BYTES with no multiplier
  // ==========================================================================
  // COPIED, NOT RE-DERIVED. The identical shift-and-add appears verbatim in
  // zhao_terrain_writeback.sv, zhao_terrain_pagestream.sv,
  // zhao_terrain_pageloader.sv and zhao_terrain_hdrread.sv. The contract's
  // sentence is the reason: "A fifth spelling of the same thing is a fifth
  // thing that can be subtly different." 21,376 = 2^14+2^12+2^9+2^8+2^7 --
  // five shifts, four adders, NO DSP.
  function automatic logic [31:0] slot_scaled(input logic [SLOTW-1:0] s);
    logic [31:0] acc;
    begin
      acc = 32'd0;
      for (int unsigned b = 0; b < 32; b++) begin
        if (((PAGE_BYTES >> b) & 32'd1) != 32'd0) begin
          acc = acc + ({{(32 - SLOTW) {1'b0}}, s} << b);
        end
      end
      slot_scaled = acc;
    end
  endfunction

  // vj * EDGE with no multiplier: 33 = 32 + 1, one shift and one adder.
  // `vj` is 6 bits at the port, so the widest legal product is 63*33 = 2,079
  // and the return is sized for it rather than for the 32 the lattice can
  // actually present: a cursor that offers an out-of-range row must produce a
  // BOUNDED wrong index that `nb_o`'s range test can refuse, never a wrapped
  // one that lands on a real vertex.
  function automatic logic [VIX:0] vrow_scaled(input logic [5:0] vj);
    vrow_scaled = (VIX + 1)'({vj, 5'b0}) + (VIX + 1)'(vj);
  endfunction

  // ==========================================================================
  // THE PAGE BUFFERS
  // ==========================================================================
  // Both are 64 bits wide because the GUARD BEAT is 64 bits and a read beat
  // cannot be stalled: the block must absorb one beat per cycle. Byte- and
  // halfword-granular access from the bake face is therefore a
  // read-modify-write of one word, which costs two cycles against
  // TERRAIN.BAKE's ~20-cycle-per-vertex spine and is free in practice.
  //
  //   bwrd_q  280 x 64 = 17,920 bits  ~2 M10K   layer B write window
  //   dwrd_q  136 x 64 =  8,704 bits  ~1 M10K   layer D read AND write window
  //
  // which is contract section 5's form (b) (~3 M10K), the recommended one,
  // WITH its "further halving" taken: dwrd_q is one buffer serving both
  // directions. Form (a) -- also buffering A/B/C so bake pulls from here --
  // would cost ~8 M10K and is rejected on ruling R59's own rule, at 306 of 553
  // M10K already used (ruling R87: M10K is no longer the free currency).
  logic [63:0] bwrd_q [B_WORDS];
  logic [63:0] dwrd_q [D_WORDS];
  logic [63:0] bwrd_rd_q, dwrd_rd_q;

  logic             bwrd_we, dwrd_we;
  logic [BWA-1:0]   bwrd_wa, bwrd_ra;
  logic [DWA-1:0]   dwrd_wa, dwrd_ra;
  logic [63:0]      bwrd_wd, dwrd_wd;

  always_ff @(posedge clk) begin
    if (bwrd_we) bwrd_q[bwrd_wa] <= bwrd_wd;
    bwrd_rd_q <= bwrd_q[bwrd_ra];
  end

  always_ff @(posedge clk) begin
    if (dwrd_we) dwrd_q[dwrd_wa] <= dwrd_wd;
    dwrd_rd_q <= dwrd_q[dwrd_ra];
  end

  // THE VERTEX-DOMAIN NO_BAKE SHADOW, IN FLOPS AND NOT IN A RAM.
  // `nb_o` must answer any (vi,vj) COMBINATIONALLY (see the port comment), and
  // a RAM cannot. 1,089 flops plus one variable bit-select is the price, and
  // it is declared here rather than buried: it is this block's one
  // flop-heavy structure and the first thing to revisit if the fit says the
  // block is too big. The cheaper alternative is a two-row register WINDOW
  // refilled on a vj change -- 66 flops instead of 1,089 -- but it is only
  // correct for a cursor that scans z-then-x, so it trades an area number for
  // a coupling to TERRAIN.BAKE's traversal order. Not taken.
  logic [VERTS-1:0] nbv_q;

  // ==========================================================================
  // THE JOB, LATCHED
  // ==========================================================================
  logic [SLOTW-1:0] job_slot_q;
  logic [GENW-1:0]  job_gen_q;
  logic [31:0]      job_epoch_q;
  logic [31:0]      page_base_q;

  // ==========================================================================
  // STATES
  // ==========================================================================
  localparam logic [4:0] S_IDLE     = 5'd0;
  localparam logic [4:0] S_DRD_REQ  = 5'd1;   // layer D read: request burst
  localparam logic [4:0] S_DRD_VERD = 5'd2;   //   ... verdict, one cycle later
  localparam logic [4:0] S_DRD_BEAT = 5'd3;   //   ... absorb 8 beats
  localparam logic [4:0] S_BED_REQ  = 5'd4;   // layer B EDGE read: 2 bursts
  localparam logic [4:0] S_BED_VERD = 5'd5;
  localparam logic [4:0] S_BED_BEAT = 5'd6;
  localparam logic [4:0] S_NBV      = 5'd7;   // scatter the no_bake shadow
  localparam logic [4:0] S_SERVE    = 5'd8;   // the bake face is live
  localparam logic [4:0] S_BWR_REQ  = 5'd9;   // layer B write: 35 bursts
  localparam logic [4:0] S_BWR_VERD = 5'd10;
  localparam logic [4:0] S_BWR_BEAT = 5'd11;
  localparam logic [4:0] S_DWR_REQ  = 5'd12;  // layer D write: 17 bursts
  localparam logic [4:0] S_DWR_VERD = 5'd13;
  localparam logic [4:0] S_DWR_BEAT = 5'd14;
  localparam logic [4:0] S_MARK     = 5'd15;  // publish terr_dm_*
  localparam logic [4:0] S_DONE     = 5'd16;
  logic [4:0] state_q;

  // ---- burst / beat cursors -------------------------------------------------
  logic [7:0] burst_q;   // 0..34 within the plane being moved
  logic [3:0] beat_q;    // 0..7 within the burst
  logic       bed_hi_q;  // which of layer B's two edge bursts is in flight

  // ---- the section 3.3 scatter cursor ---------------------------------------
  logic [CIX-1:0] nbv_k_q;   // 0..NCELL, the cell being scattered
  logic           nbv_go_q;  // the pipelined read is valid

  // ---- bake-face bookkeeping -------------------------------------------------
  logic [CIX-1:0] sc_seen_q;  // how many scar words the bake wrote
  logic [CIX-1:0] cs_seen_q;  // how many cell states the bake wrote
  // `dig_done_i` is TERRAIN.BAKE saying layer B is complete, and it is read
  // for two things. `dig_seen_q` is the phase boundary contract section 5's
  // single-layer-D-buffer argument rests on -- the breach phase must not
  // begin before the dig phase ends, or a cell could be re-read after it is
  // written. `dig_short_q` catches a SHORT LAYER B one whole phase earlier
  // than `bake_done_i` would, and names the right phase when it does.
  logic dig_seen_q;
  logic dig_short_q;
  // `bake_done_i` IS A ONE-CYCLE PULSE AND IT CAN LAND MID-WRITE. The last
  // cs write of a record is a two-cycle read-modify-write, and
  // `zhao_terrain_bake_v2` raises `bake_done_o` one cycle after its last
  // handshake -- so the pulse arrives while OP_CS_A/OP_CS_B are still in
  // flight. Acting on it there ABORTS that write, and the first build of this
  // file did exactly that: 1,023 of 1,024 cells landed, the page write
  // completed, every counter agreed, and ONE CELL kept its old substance. A
  // single wrong cell is one wrong breach decision, which is the fault class
  // nothing in the machine can see. So the pulse is LATCHED and acted on when
  // the buffer is quiet.
  logic bake_pend_q;

  // ==========================================================================
  // THE BAKE FACE: pending operations against the page buffers
  // ==========================================================================
  // One buffer operation at a time, priority cell-read > cs-write > sc-write,
  // because bake BLOCKS on a cell read and merely backpressures on a write.
  localparam logic [2:0] OP_NONE    = 3'd0;
  localparam logic [2:0] OP_CELL_A  = 3'd1;  // cell read: address issued
  localparam logic [2:0] OP_CELL_B  = 3'd2;  // cell read: data landing
  localparam logic [2:0] OP_CS_A    = 3'd3;  // cs write: read the word
  localparam logic [2:0] OP_CS_B    = 3'd4;  // cs write: merge and store
  localparam logic [2:0] OP_SC_A    = 3'd5;  // sc write: read the word
  localparam logic [2:0] OP_SC_B    = 3'd6;  // sc write: merge and store
  logic [2:0] op_q;

  logic [5:0]         cell_ci_q, cell_cj_q;   // LATCHED at read issue
  logic [7:0]         cell_byte_q;
  logic               cell_hold_q;            // an answer is held

  logic [5:0]         cs_ci_q, cs_cj_q;
  logic [7:0]         cs_byte_q;

  logic [5:0]         sc_vi_q, sc_vj_q;
  logic signed [15:0] sc_word_q;

  // ---- byte / halfword addressing -------------------------------------------
  // Cell (ci,cj) is page byte D_OFF + cj*CELLS + ci, i.e. buffer byte
  // D_LEAD + cj*32 + ci. CELLS = 32 so cj*CELLS is a shift.
  function automatic logic [12:0] cell_byte_ix(input logic [5:0] ci, input logic [5:0] cj);
    cell_byte_ix = 13'(D_LEAD) + {2'b0, cj, 5'b0} + {7'b0, ci};
  endfunction

  // Vertex k = vj*EDGE + vi is page byte B_OFF + 2k, i.e. buffer byte
  // B_LEAD + 2k. The sample is 16 bits at an EVEN buffer byte, so it lies
  // wholly inside one 64-bit word (lane 0, 2, 4 or 6, and 6+2 = 8 fits).
  function automatic logic [13:0] vtx_byte_ix(input logic [5:0] vi, input logic [5:0] vj);
    logic [VIX:0] k;
    begin
      k = vrow_scaled(vj) + (VIX + 1)'(vi);
      vtx_byte_ix = 14'(B_LEAD) + 14'({k, 1'b0});
    end
  endfunction

  // ==========================================================================
  // THE NO_BAKE SHADOW, COMBINATIONAL (the one law this block owns)
  // ==========================================================================
  logic [VIX:0] nb_ix_c;
  assign nb_ix_c = vrow_scaled(nb_vj_i) + {{(VIX - 5) {1'b0}}, nb_vi_i};
  assign nb_o = nb_req_i && (nb_ix_c < (VIX + 1)'(VERTS)) ? nbv_q[nb_ix_c[VIX-1:0]] : 1'b0;

  // ==========================================================================
  // GUARD REQUEST -- ONE SHAPE, 64-byte aligned, len 64, be all ones
  // ==========================================================================
  // `guard_req_o.valid` is gated on the STATE, so a job that failed its
  // pre-checks never reaches the guard at all and `guard_denied_o` stays a
  // measurement OF THE GUARD rather than of this block's own bookkeeping.
  // That is `zhao_terrain_pagestream.sv`'s rule and it is copied deliberately.
  logic        req_v_c, req_w_c;
  logic [31:0] req_off_c;

  always_comb begin
    req_v_c   = 1'b0;
    req_w_c   = 1'b0;
    req_off_c = 32'd0;
    case (state_q)
      S_DRD_REQ: begin
        req_v_c   = 1'b1;
        req_off_c = 32'(D_BASE) + ({24'b0, burst_q} << $clog2(BURST_BYTES));
      end
      S_BED_REQ: begin
        req_v_c   = 1'b1;
        req_off_c = 32'(B_BASE) +
                    (bed_hi_q ? (32'(B_NBURST - 1) << $clog2(BURST_BYTES)) : 32'd0);
      end
      S_BWR_REQ: begin
        req_v_c   = 1'b1;
        req_w_c   = 1'b1;
        req_off_c = 32'(B_BASE) + ({24'b0, burst_q} << $clog2(BURST_BYTES));
      end
      S_DWR_REQ: begin
        req_v_c   = 1'b1;
        req_w_c   = 1'b1;
        req_off_c = 32'(D_BASE) + ({24'b0, burst_q} << $clog2(BURST_BYTES));
      end
      default: ;
    endcase
  end

  always_comb begin
    guard_req_o        = '0;
    guard_req_o.valid  = req_v_c;
    guard_req_o.write  = req_w_c;
    guard_req_o.client = cfg_vram_client_i;
    guard_req_o.addr   = ZHAO_VRAM_ADDR_BITS'(page_base_q + req_off_c);
    guard_req_o.len    = 7'(BURST_BYTES);
    guard_req_o.be     = '1;
  end

  assign guard_wvalid_o = (state_q == S_BWR_BEAT) || (state_q == S_DWR_BEAT);
  assign guard_wdata_o  = (state_q == S_BWR_BEAT) ? bwrd_rd_q : dwrd_rd_q;
  assign guard_wlast_o  = (beat_q == 4'(BEATS - 1));

  // ==========================================================================
  // PRE-CHECKS -- a refusal is not a clamp
  // ==========================================================================
  logic pre_slot_bad_c, pre_epoch_bad_c;
  assign pre_slot_bad_c  = (32'({{(32 - SLOTW) {1'b0}}, j_slot_i}) >= 32'(REGION_SLOTS));
  assign pre_epoch_bad_c = (j_epoch_i != cfg_epoch_i);

  assign j_ready_o = (state_q == S_IDLE) && !done_valid_o && !dm_valid_o;
  assign idle_o    = (state_q == S_IDLE) && !done_valid_o && !dm_valid_o;

  // ==========================================================================
  // THE BAKE-FACE HANDSHAKES
  // ==========================================================================
  // A cell answer is delivered ONLY against the address it was read for. See
  // the header: the two sides of this comparison are loaded by different
  // enables, so it is not the lockstep-blind kind of check.
  logic cell_addr_match_c;
  assign cell_addr_match_c = (cell_ci_i == cell_ci_q) && (cell_cj_i == cell_cj_q);

  assign cell_valid_o = (state_q == S_SERVE) && cell_hold_q && cell_addr_match_c;
  assign cell_state_o = cell_byte_q;

  // A fresh cell read is wanted when nothing is held for the offered address.
  logic cell_want_c;
  assign cell_want_c = (state_q == S_SERVE) && !(cell_hold_q && cell_addr_match_c);

  // A READY THAT DOES NOT ACCEPT IS NOT A READY, and the first draft of this
  // file got it wrong in the way that is hardest to see. `sc_ready_o` was
  // `(state == S_SERVE) && (op_q == OP_NONE)` -- which is true in the very
  // cycle the arbiter below takes the CELL branch instead, so a producer that
  // read the level and advanced lost that beat SILENTLY. It cost exactly one
  // scar word per bake: 1,088 of 1,089, `sc_seen_q` one short, and the whole
  // page write correctly refused as V_SHORT_B. The verdict was right and the
  // cause was two hundred lines away.
  //
  // So each ready carries the arbiter's own priority. This is a ready that
  // depends on a valid, which can deadlock against a producer whose valid
  // depends on ready -- TERRAIN.BAKE's `sc_valid_o`/`cs_valid_o` are
  // REGISTERED (raised at StEmit and at the StCell accept), so there is no
  // loop, and that is a property of the consumer this block is built for
  // rather than a general licence.
  assign cs_ready_o = (state_q == S_SERVE) && (op_q == OP_NONE) && !cell_want_c;
  assign sc_ready_o = (state_q == S_SERVE) && (op_q == OP_NONE) && !cell_want_c
                      && !cs_valid_i;

  // ==========================================================================
  // BUFFER PORT ARBITRATION
  // ==========================================================================
  logic [12:0] cs_bix_c;
  logic [12:0] cell_bix_c;
  logic [13:0] sc_bix_c;
  assign cs_bix_c   = cell_byte_ix(cs_ci_q, cs_cj_q);
  assign cell_bix_c = cell_byte_ix(cell_ci_i, cell_cj_i);
  assign sc_bix_c   = vtx_byte_ix(sc_vi_q, sc_vj_q);

  logic [12:0] nbv_bix_c;
  assign nbv_bix_c = 13'(D_LEAD) + {{(13 - CIX) {1'b0}}, nbv_k_q};

  // The beat the write path prefetches: the next one, held at the last.
  logic [3:0] beat_next_c;
  assign beat_next_c = (guard_wready_i && (beat_q != 4'(BEATS - 1)))
                       ? (beat_q + 4'd1) : beat_q;

  always_comb begin
    bwrd_we = 1'b0;
    bwrd_wa = '0;
    bwrd_wd = bwrd_rd_q;
    bwrd_ra = '0;
    dwrd_we = 1'b0;
    dwrd_wa = '0;
    dwrd_wd = dwrd_rd_q;
    dwrd_ra = '0;

    case (state_q)
      // ---- filling from the guard: one beat per cycle, no stall ------------
      S_DRD_BEAT: begin
        dwrd_we = beat_valid_i;
        dwrd_wa = DWA'({4'b0, burst_q} * BEATS + {4'b0, beat_q});
        dwrd_wd = beat_data_i;
      end
      S_BED_BEAT: begin
        bwrd_we = beat_valid_i;
        bwrd_wa = bed_hi_q ? BWA'(32'((B_NBURST - 1) * BEATS) + 32'(beat_q))
                           : BWA'(32'(beat_q));
        bwrd_wd = beat_data_i;
      end

      // ---- the section 3.3 scatter reads layer D one cell at a time --------
      S_NBV: dwrd_ra = DWA'(nbv_bix_c[12:3]);

      // ---- serving the bake face -------------------------------------------
      S_SERVE: begin
        case (op_q)
          OP_CELL_A: dwrd_ra = DWA'(cell_bix_c[12:3]);
          OP_CS_A:   dwrd_ra = DWA'(cs_bix_c[12:3]);
          OP_CS_B: begin
            dwrd_we = 1'b1;
            dwrd_wa = DWA'(cs_bix_c[12:3]);
            dwrd_wd = dwrd_rd_q;
            dwrd_wd[{7'b0, cs_bix_c[2:0]} * 8 +: 8] = cs_byte_q;
          end
          OP_SC_A: bwrd_ra = BWA'(sc_bix_c[13:3]);
          OP_SC_B: begin
            bwrd_we = 1'b1;
            bwrd_wa = BWA'(sc_bix_c[13:3]);
            bwrd_wd = bwrd_rd_q;
            bwrd_wd[{7'b0, sc_bix_c[2:0]} * 8 +: 16] = sc_word_q;
          end
          default: begin
            // Prefetch the next buffer word the instant an operation is
            // chosen, so the two-cycle RMW costs two cycles and not three.
            if (cell_want_c) dwrd_ra = DWA'(cell_bix_c[12:3]);
          end
        endcase
      end

      // ---- draining to the guard: one word per beat ------------------------
      // THE NEXT BEAT IS PREFETCHED, AND THE ADVANCE IS CLAMPED AT THE LAST
      // ONE. Without the clamp, the accepted last beat of the last burst
      // addresses word `B_NBURST * BEATS` = 280, one past the end of a
      // 280-entry array. The value is never used -- the final beat is already
      // on the wire and the state leaves -- so it is benign in behaviour, and
      // it is still an out-of-range index in a structure meant to infer an
      // M10K. Quartus is free to treat that differently from Verilator, and
      // "benign today" is not a property anyone re-checks.
      S_BWR_VERD: bwrd_ra = BWA'({4'b0, burst_q} * BEATS);
      S_BWR_BEAT: bwrd_ra = BWA'({4'b0, burst_q} * BEATS + {4'b0, beat_next_c});
      S_DWR_VERD: dwrd_ra = DWA'({4'b0, burst_q} * BEATS);
      S_DWR_BEAT: dwrd_ra = DWA'({4'b0, burst_q} * BEATS + {4'b0, beat_next_c});
      default: ;
    endcase
  end

  // The byte the bake face offered, selected out of the word that landed in
  // the read register one cycle later.
  //
  // THE LANE IS LATCHED AT ISSUE, NOT RECOMPUTED AT EXTRACT. Recomputing it
  // from the held address is the same value by construction today and is a
  // second expression that can drift; latching the three bits that went out
  // with the read keeps ONE statement of "which lane did I ask for". It is
  // also what makes `cell_bix_c` and `nbv_bix_c` wholly used rather than
  // half-used, which is why there is no UNUSEDSIGNAL waiver in this file.
  logic [2:0] cell_lane_q;
  logic [2:0] nbv_lane_q;
  logic [7:0] cell_lane_c;
  logic [7:0] nbv_lane_c;
  assign cell_lane_c = dwrd_rd_q[{7'b0, cell_lane_q} * 8 +: 8];
  assign nbv_lane_c  = dwrd_rd_q[{7'b0, nbv_lane_q} * 8 +: 8];

  // The scatter pass is pipelined one step behind the buffer read, so the
  // byte landing now belongs to cell `nbv_k_q - 1`.
  //
  // The four vertices that cell corners are (ci,cj), (ci+1,cj), (ci,cj+1) and
  // (ci+1,cj+1). `nbv_base_c` is the first of the lower pair; the upper pair
  // sits EDGE further on. This is the SCATTER form of the gather in
  // `zref::terrain::nobake_corner_shadow` -- see the header.
  logic [CIX-1:0] nbv_kp_c;
  logic [5:0]     nbv_ci_c, nbv_cj_c;
  logic [VIX-1:0] nbv_base_c;
  assign nbv_kp_c   = nbv_k_q - CIX'(1);
  assign nbv_ci_c   = 6'(nbv_kp_c & CIX'(CELLS - 1));
  assign nbv_cj_c   = 6'(nbv_kp_c >> $clog2(CELLS));
  assign nbv_base_c = VIX'(vrow_scaled(nbv_cj_c) + (VIX + 1)'(nbv_ci_c));

  // ==========================================================================
  // THE SEQUENCER
  // ==========================================================================
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state_q     <= S_IDLE;
      burst_q     <= '0;
      beat_q      <= '0;
      bed_hi_q    <= 1'b0;
      nbv_k_q     <= '0;
      nbv_go_q    <= 1'b0;
      nbv_lane_q  <= '0;
      cell_lane_q <= '0;
      dig_seen_q  <= 1'b0;
      dig_short_q <= 1'b0;
      bake_pend_q <= 1'b0;
      nbv_q       <= '0;
      op_q        <= OP_NONE;
      cell_ci_q   <= 6'd63;   // an address the cursor cannot present, so the
      cell_cj_q   <= 6'd63;   // first cell of a bake always reads
      cell_byte_q <= '0;
      cell_hold_q <= 1'b0;
      cs_ci_q     <= '0;
      cs_cj_q     <= '0;
      cs_byte_q   <= '0;
      sc_vi_q     <= '0;
      sc_vj_q     <= '0;
      sc_word_q   <= '0;
      sc_seen_q   <= '0;
      cs_seen_q   <= '0;
      job_slot_q  <= '0;
      job_gen_q   <= '0;
      job_epoch_q <= '0;
      page_base_q <= '0;

      dm_valid_o     <= 1'b0;
      dm_slot_o      <= '0;
      dm_gen_o       <= '0;
      dm_epoch_o     <= '0;
      dm_bd_o        <= 1'b0;
      dm_f_o         <= 1'b0;
      dm_mips_o      <= 1'b0;
      done_valid_o   <= 1'b0;
      done_ok_o      <= 1'b0;
      done_verdict_o <= V_OK;
      done_slot_o    <= '0;
      done_src_id_o  <= '0;

      pages_read_o     <= '0;
      pages_written_o  <= '0;
      guard_denied_o   <= '0;
      bursts_read_o    <= '0;
      bursts_written_o <= '0;
      stale_gen_o      <= '0;
      marks_emitted_o  <= '0;
      jobs_refused_o   <= '0;
      nobake_mutated_o <= '0;
      cell_refetch_o   <= '0;
    end else begin
      if (done_valid_o && done_ready_i) done_valid_o <= 1'b0;
      if (dm_valid_o && dm_ready_i) dm_valid_o <= 1'b0;

      case (state_q)
        // ------------------------------------------------------------------
        S_IDLE: begin
          if (j_valid_i && j_ready_o) begin
            job_slot_q  <= j_slot_i;
            job_gen_q   <= j_gen_i;
            job_epoch_q <= j_epoch_i;
            page_base_q <= 32'(REGION_BASE) + slot_scaled(j_slot_i);
            done_slot_o   <= j_slot_i;
            done_src_id_o <= j_src_id_i;

            // A REFUSAL IS NOT A CLAMP, and it never reaches the guard, so
            // `guard_denied_o` keeps measuring the guard.
            if (pre_slot_bad_c) begin
              done_verdict_o <= V_SLOT_OOR;
              done_ok_o      <= 1'b0;
              jobs_refused_o <= jobs_refused_o + 32'd1;
              state_q        <= S_DONE;
            end else if (pre_epoch_bad_c) begin
              done_verdict_o <= V_EPOCH;
              done_ok_o      <= 1'b0;
              stale_gen_o    <= stale_gen_o + 32'd1;
              jobs_refused_o <= jobs_refused_o + 32'd1;
              state_q        <= S_DONE;
            end else begin
              burst_q     <= '0;
              beat_q      <= '0;
              sc_seen_q   <= '0;
              cs_seen_q   <= '0;
              dig_seen_q  <= 1'b0;
              dig_short_q <= 1'b0;
              bake_pend_q <= 1'b0;
              nbv_q       <= '0;
              cell_hold_q <= 1'b0;
              cell_ci_q   <= 6'd63;
              cell_cj_q   <= 6'd63;
              op_q        <= OP_NONE;
              state_q     <= S_DRD_REQ;
            end
          end
        end

        // ---- LAYER D READ: 17 bursts ---------------------------------------
        // THE GUARD ANSWERS IN TWO CYCLES AND THE TWO BITS ARE NEVER BOTH
        // HIGH. `zhao_mem_guard` drives `rsp.ready = !fwd_active` as a LEVEL
        // and pulses `rsp.ok` the cycle AFTER it accepts. Testing them in one
        // arm reads every pass as a denial, silently, with the denial counter
        // stuck at zero -- the defect found in BOTH geometry fetchers on
        // 2026-09-06. So *_REQ waits on `ready` and *_VERD, a separate state
        // one cycle later, reads `ok`/`violation`.
        S_DRD_REQ:  if (guard_rsp_i.ready) state_q <= S_DRD_VERD;
        S_DRD_VERD: begin
          if (guard_rsp_i.violation) begin
            guard_denied_o <= guard_denied_o + 32'd1;
            done_verdict_o <= V_GUARD;
            done_ok_o      <= 1'b0;
            state_q        <= S_DONE;
          end else if (guard_rsp_i.ok) begin
            beat_q  <= '0;
            state_q <= S_DRD_BEAT;
          end
        end
        S_DRD_BEAT: begin
          if (beat_valid_i) begin
            beat_q <= beat_q + 4'd1;
            if (beat_last_i) begin
              bursts_read_o <= bursts_read_o + 32'd1;
              if (beat_q != 4'(BEATS - 1)) begin
                done_verdict_o <= V_INCOMPLETE;
                done_ok_o      <= 1'b0;
                state_q        <= S_DONE;
              end else if (burst_q == 8'(D_NBURST - 1)) begin
                pages_read_o <= pages_read_o + 32'd1;
                burst_q      <= '0;
                bed_hi_q     <= 1'b0;
                state_q      <= S_BED_REQ;
              end else begin
                burst_q <= burst_q + 8'd1;
                state_q <= S_DRD_REQ;
              end
            end
          end
        end

        // ---- LAYER B EDGE READ: bursts 35 and 69 ---------------------------
        // The ONLY reason layer B is read at all. Everything between these two
        // bursts is overwritten by the bake in full; these two carry layer A's
        // last 2 bytes and layer C's first 60, and writing them from a
        // zero-filled buffer would be a silent corruption of layers nothing in
        // the machine reads back yet.
        S_BED_REQ:  if (guard_rsp_i.ready) state_q <= S_BED_VERD;
        S_BED_VERD: begin
          if (guard_rsp_i.violation) begin
            guard_denied_o <= guard_denied_o + 32'd1;
            done_verdict_o <= V_GUARD;
            done_ok_o      <= 1'b0;
            state_q        <= S_DONE;
          end else if (guard_rsp_i.ok) begin
            beat_q  <= '0;
            state_q <= S_BED_BEAT;
          end
        end
        S_BED_BEAT: begin
          if (beat_valid_i) begin
            beat_q <= beat_q + 4'd1;
            if (beat_last_i) begin
              bursts_read_o <= bursts_read_o + 32'd1;
              if (beat_q != 4'(BEATS - 1)) begin
                done_verdict_o <= V_INCOMPLETE;
                done_ok_o      <= 1'b0;
                state_q        <= S_DONE;
              end else if (bed_hi_q) begin
                nbv_k_q  <= '0;
                nbv_go_q <= 1'b0;
                state_q  <= S_NBV;
              end else begin
                bed_hi_q <= 1'b1;
                state_q  <= S_BED_REQ;
              end
            end
          end
        end

        // ---- SCATTER THE SECTION 3.3 SHADOW --------------------------------
        // One cell per cycle, pipelined behind the buffer's one-cycle read:
        // at step k the address for cell k is offered and cell k-1's byte is
        // consumed out of `dwrd_rd_q`.
        S_NBV: begin
          if (nbv_go_q && nbv_lane_c[NOBAKE_BIT]) begin
            nbv_q[nbv_base_c]                       <= 1'b1;
            nbv_q[nbv_base_c + VIX'(1)]             <= 1'b1;
            nbv_q[nbv_base_c + VIX'(EDGE)]          <= 1'b1;
            nbv_q[nbv_base_c + VIX'(EDGE) + VIX'(1)] <= 1'b1;
          end
          if (nbv_k_q == CIX'(NCELL)) begin
            nbv_go_q <= 1'b0;
            state_q  <= S_SERVE;
          end else begin
            nbv_k_q    <= nbv_k_q + CIX'(1);
            nbv_lane_q <= nbv_bix_c[2:0];  // the lane this cycle's read asked for
            nbv_go_q   <= 1'b1;
          end
        end

        // ---- SERVE THE BAKE FACE -------------------------------------------
        S_SERVE: begin
          case (op_q)
            OP_NONE: begin
              // Priority: the cell read first, because TERRAIN.BAKE BLOCKS on
              // it (StCell waits for `cell_valid_i`) while a write merely
              // backpressures.
              //
              // NOTHING NEW IS STARTED ONCE THE RECORD IS RETIRING. The
              // transition below forces `op_q` back to OP_NONE in the same
              // cycle, so an operation begun here would be dropped -- and a
              // DROPPED sc ACCEPT would also have incremented `sc_seen_q`,
              // which is how a lost write becomes an unnoticed one.
              if (bake_done_i || bake_pend_q) begin
                op_q <= OP_NONE;
              end else if (cell_want_c) begin
                cell_ci_q   <= cell_ci_i;
                cell_cj_q   <= cell_cj_i;
                cell_lane_q <= cell_bix_c[2:0];   // the lane that went out
                cell_hold_q <= 1'b0;
                op_q        <= OP_CELL_B;  // the address went out this cycle
              end else if (cs_valid_i && cs_ready_o) begin
                cs_ci_q   <= cs_ci_i;
                cs_cj_q   <= cs_cj_i;
                cs_byte_q <= cs_state_i;
                cs_seen_q <= cs_seen_q + CIX'(1);
                op_q      <= OP_CS_A;
              end else if (sc_valid_i && sc_ready_o) begin
                sc_vi_q   <= sc_vi_i;
                sc_vj_q   <= sc_vj_i;
                sc_word_q <= sc_scar_i;
                sc_seen_q <= sc_seen_q + CIX'(1);
                op_q      <= OP_SC_A;
              end
            end

            OP_CELL_B: begin
              cell_byte_q <= cell_lane_c;
              cell_hold_q <= 1'b1;
              op_q        <= OP_NONE;
            end

            // A cs write is a read-modify-write of one 64-bit word: the guard
            // has no byte enables (see the header), and neither does this
            // buffer, so the seven bytes we do not own must be carried across.
            OP_CS_A: op_q <= OP_CS_B;
            OP_CS_B: begin
              // THE ONE DETECTOR THAT CANNOT FIRE UNDER A LEGAL BAKE.
              // TERRAIN.BAKE preserves bits 7:2 (`cs_state_o <=
              // {cell_state_i[7:2], sub_out}`), so a cs write whose kNoBakeBit
              // disagrees with the plane this block read is an ILLEGAL input.
              // Counted, not clamped and not refused: the byte still lands,
              // because narrowing a write is how a silent divergence is made
              // permanent, and the counter is what makes it visible.
              if (cs_byte_q[NOBAKE_BIT] !=
                  dwrd_rd_q[{7'b0, cs_bix_c[2:0]} * 8 + NOBAKE_BIT])
                nobake_mutated_o <= nobake_mutated_o + 32'd1;
              op_q <= OP_NONE;
            end

            OP_SC_A: op_q <= OP_SC_B;
            OP_SC_B: op_q <= OP_NONE;

            default: op_q <= OP_NONE;
          endcase

          // TERRAIN.BAKE says the dig sweep is finished. The scar count is
          // read HERE rather than only at `bake_done_i`, so a short layer B is
          // attributed to the phase that produced it instead of to the record.
          if (dig_done_i) begin
            dig_seen_q <= 1'b1;
            if (sc_seen_q != CIX'(VERTS)) dig_short_q <= 1'b1;
          end

          // A held answer whose address moved underneath it is DROPPED, never
          // delivered. See the header.
          if (cell_hold_q && !cell_addr_match_c) begin
            cell_hold_q    <= 1'b0;
            cell_refetch_o <= cell_refetch_o + 32'd1;
          end
          if (cell_valid_o && cell_ready_i) cell_hold_q <= 1'b0;

          // A HELD ANSWER FOR A CELL THAT IS BEING WRITTEN IS STALE, and it
          // must not be delivered. `dwrd_q` is one buffer for both directions
          // (contract section 5's "further halving"), so a cs write changes
          // the byte a held answer was read from. TERRAIN.BAKE's own order
          // never re-reads a written cell, which is exactly why this could sit
          // here wrong for a long time: it is invisible under the one traversal
          // the machine uses today, and it is the block's promise to any
          // traversal. Not counted -- it is ordinary invalidation, not a
          // fault -- and `pageio_rtl_directed` drives the re-read to check the
          // NEW byte comes back.
          if (cs_valid_i && cs_ready_o && cell_hold_q
              && (cs_ci_i == cell_ci_q) && (cs_cj_i == cell_cj_q))
            cell_hold_q <= 1'b0;

          // ---- the bake retired --------------------------------------------
          if (bake_done_i) bake_pend_q <= 1'b1;
          if ((bake_done_i || bake_pend_q) && (op_q == OP_NONE)) begin
            cell_hold_q <= 1'b0;
            bake_pend_q <= 1'b0;
            op_q        <= OP_NONE;
            burst_q     <= '0;
            beat_q      <= '0;
            // A PARTIAL PLANE IS NEVER WRITTEN. The whole of layer B is
            // rewritten by a bake -- `zhao_terrain_bake_v2` raises
            // `sc_valid_o` at StEmit for EVERY vertex, with `sc_touched_o`
            // saying which were inside the stencil -- so anything short of
            // VERTS means the buffer's interior holds the PREVIOUS bake's
            // words, and writing it would corrupt the page under a clean
            // handshake. Refuse the write and say so.
            if (dig_short_q || (sc_seen_q != CIX'(VERTS))) begin
              done_verdict_o <= V_SHORT_B;
              done_ok_o      <= 1'b0;
              jobs_refused_o <= jobs_refused_o + 32'd1;
              state_q        <= S_DONE;
            end else if ((cs_seen_q != CIX'(NCELL)) && (cs_seen_q != CIX'(0))) begin
              // Layer D is all-or-nothing: a record with `cmd_cells_i` low
              // runs no breach phase and writes no cell, which is legal and
              // leaves layer D untouched. A PART-filled plane is not.
              done_verdict_o <= V_SHORT_D;
              done_ok_o      <= 1'b0;
              jobs_refused_o <= jobs_refused_o + 32'd1;
              state_q        <= S_DONE;
            end else begin
              state_q <= S_BWR_REQ;
            end
          end
        end

        // ---- LAYER B WRITE: 35 bursts --------------------------------------
        S_BWR_REQ:  if (guard_rsp_i.ready) state_q <= S_BWR_VERD;
        S_BWR_VERD: begin
          if (guard_rsp_i.violation) begin
            guard_denied_o <= guard_denied_o + 32'd1;
            done_verdict_o <= V_GUARD;
            done_ok_o      <= 1'b0;
            state_q        <= S_DONE;
          end else if (guard_rsp_i.ok) begin
            beat_q  <= '0;
            state_q <= S_BWR_BEAT;
          end
        end
        S_BWR_BEAT: begin
          if (guard_wready_i) begin
            if (beat_q == 4'(BEATS - 1)) begin
              bursts_written_o <= bursts_written_o + 32'd1;
              if (burst_q == 8'(B_NBURST - 1)) begin
                burst_q <= '0;
                // Layer D is skipped entirely when the bake wrote no cell.
                state_q <= (cs_seen_q == CIX'(0)) ? S_MARK : S_DWR_REQ;
              end else begin
                burst_q <= burst_q + 8'd1;
                state_q <= S_BWR_REQ;
              end
            end else begin
              beat_q <= beat_q + 4'd1;
            end
          end
        end

        // ---- LAYER D WRITE: 17 bursts --------------------------------------
        S_DWR_REQ:  if (guard_rsp_i.ready) state_q <= S_DWR_VERD;
        S_DWR_VERD: begin
          if (guard_rsp_i.violation) begin
            guard_denied_o <= guard_denied_o + 32'd1;
            done_verdict_o <= V_GUARD;
            done_ok_o      <= 1'b0;
            state_q        <= S_DONE;
          end else if (guard_rsp_i.ok) begin
            beat_q  <= '0;
            state_q <= S_DWR_BEAT;
          end
        end
        S_DWR_BEAT: begin
          if (guard_wready_i) begin
            if (beat_q == 4'(BEATS - 1)) begin
              bursts_written_o <= bursts_written_o + 32'd1;
              if (burst_q == 8'(D_NBURST - 1)) begin
                burst_q <= '0;
                state_q <= S_MARK;
              end else begin
                burst_q <= burst_q + 8'd1;
                state_q <= S_DWR_REQ;
              end
            end else begin
              beat_q <= beat_q + 4'd1;
            end
          end
        end

        // ---- THE DEFORMATION MARK ------------------------------------------
        S_MARK: begin
          if (!dm_valid_o) begin
            dm_valid_o <= 1'b1;
            dm_slot_o  <= job_slot_q;
            // OWNER DECISION 2 (contract sec 7): a bake does NOT bump the
            // generation. The slot still holds the same patch, and
            // `terr_dm_*`'s own per-layer dirty bits are the mechanism for
            // "this content moved". Bumping it would make every handle held
            // across a bake stale and start `terr_chk_stale_o` firing on live
            // handles. The alternative is one line here and is NOT taken
            // quietly: it is the contract's own recommendation and it is
            // recorded as unwritten law.
            dm_gen_o   <= job_gen_q;
            dm_epoch_o <= job_epoch_q;
            dm_bd_o    <= 1'b1;   // layers B and D moved; that is the whole job
            // OWNER DECISION 3 (contract sec 7) IS NOT TAKEN HERE. A bake
            // dirties B and D and does not touch the F sheet, so `dm_f_o` is
            // low and that much is settled. Whether it dirties the MIPS
            // depends on `zhao_terrain_pagestream.sv`'s STILL-OPEN ruling
            // about which surface MIPGEN sees: mips built from layer A alone
            // are untouched by a bake; mips built from `compose_top` are
            // invalidated by every bake. Those are the same question and
            // should be answered once, by the owner, for both blocks. Until
            // then this reports the CONSERVATIVE reading of the settled half
            // and the SAFE reading of the open half -- `dm_mips_o` high, so a
            // stale mip can never be shown -- and says so here rather than
            // burying the choice in a datapath.
            dm_f_o     <= 1'b0;
            dm_mips_o  <= 1'b1;
            marks_emitted_o <= marks_emitted_o + 32'd1;
            pages_written_o <= pages_written_o + 32'd1;
            done_verdict_o  <= V_OK;
            done_ok_o       <= 1'b1;
            state_q         <= S_DONE;
          end
        end

        // ---- COMPLETION -----------------------------------------------------
        // EVERY JOB PRODUCES EXACTLY ONE, because a job that produces no
        // completion strands whatever was waiting on it. TERRAIN.WRITEBACK's
        // rule, copied.
        S_DONE: begin
          if (!done_valid_o) begin
            done_valid_o <= 1'b1;
          end else if (done_ready_i) begin
            state_q <= S_IDLE;
          end
        end

        default: state_q <= S_IDLE;
      endcase
    end
  end

`ifndef SYNTHESIS
  // Simulation-only corroboration. These are INDEPENDENT of the synthesizable
  // counters above: the counters are what ships, these fire louder and sooner
  // while a bench is driving.
  //
  // The `armed_q` guard is `zhao_terrain_pagestream.sv`'s shape, copied. A
  // `disable iff (!rst_n)` reads `rst_n` synchronously while the sequencer
  // reads it asynchronously, which is SYNCASYNCNET -- a real warning about a
  // real thing, in a file where the async reset is deliberate.
  logic armed_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) armed_q <= 1'b0;
    else armed_q <= 1'b1;
  end

  always_ff @(posedge clk) begin
    if (armed_q) begin
      // THE GUARD ADMITS EXACTLY ONE REQUEST SHAPE -- see the header. A sparse
      // `be` or a short `len` is a VIOLATION, so an arrangement that drifted
      // into one would read as a denial and be chased as a permissions bug.
      a_no_sparse_be :
      assert (!guard_req_o.valid
              || ((guard_req_o.be == '1) && (guard_req_o.len == 7'(BURST_BYTES))))
      else $error("pageio: issued a request the guard's be_ok cannot admit");

      a_cell_answer_matches_address :
      assert (!(cell_valid_o && cell_ready_i)
              || ((cell_ci_i == cell_ci_q) && (cell_cj_i == cell_cj_q)))
      else $error("pageio: delivered a cell state against a different address");

      a_one_buffer_op :
      assert (!(bwrd_we && dwrd_we))
      else $error("pageio: two page buffers written in one cycle");

      // A cs write that arrives before TERRAIN.BAKE says the dig phase ended
      // means the two phases OVERLAPPED, which is the one condition under
      // which contract section 5's single-layer-D-buffer argument fails.
      a_breach_after_dig :
      assert (!(cs_valid_i && cs_ready_o) || dig_seen_q)
      else $error("pageio: a cell state was written before dig_done_o");
    end
  end
`endif

endmodule

`default_nettype wire
