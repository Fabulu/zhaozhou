// zhao_terrain_sheetseam.sv -- THE SHEET SEAM: SURFACE.SHEET's layer F, read
// at the rate TERRAIN.BAKE's per-vertex dig needs it, with owner ruling R221's
// ST_MISS law.
//
// ===========================================================================
// WHAT WAS MISSING, AND WHO MEASURED IT
// ===========================================================================
// Owner ruling R194 gave `zhao_terrain_bake_v2` a second depth law: the
// per-vertex mode on `cmd_depth_sheet_i`, whose depth comes out of layer F
// through `zhao_terrain_stampdepth`.  The block presents `sheet_texel_o` and
// samples `sheet_strength_i`, and its own header says who serves them:
//
//     "WHO SERVES IT is NOT this block and is not settled."
//
// The PAGEIO packet then measured the seam against the ACTUAL port and found
// that "arbiter" understates it by three items (recorded as decision 5 in
// `design/contracts/TERRAIN.PAGEIO.md`).  `zhao_surface_sheet`'s `req_*` is a
// CONTROL-AND-READ port -- `OP_ACQUIRE` / `OP_READ` / `OP_RELEASE`, a 32-bit
// handle, a separate `pg_*` response stream carrying `ST_HIT` / `ST_ALLOCATED`
// / `ST_OVERFLOW` / `ST_MISS` -- while bake wants a COMBINATIONAL lookup on
// the same beat as layers A/B/C.  Four things were missing:
//
//   1. a handle and its lifetime;
//   2. a latency adapter;
//   3. the arbiter proper;
//   4. a law for ST_MISS -- "and this one is not an engineering question".
//
// Item 4 is OWNER RULING R221: **fall back to the parametric disc, and COUNT
// the fallback.**  Item 3 is `zhao_surface_sheetshare`, a file of its own
// because a mux written inline in a composer is an arbiter nobody can point
// at.  Items 1 and 2 are this block, and both of them came out SMALLER than
// the measurement that commissioned them.  Each is argued below with the
// number that decided it, because "we priced both" is only worth something if
// the prices are written down.
//
// ===========================================================================
// ITEM 1 -- THE HANDLE AND ITS LIFETIME: THERE ISN'T ONE, AND THAT IS FORCED
// ===========================================================================
// The commissioning note reads "A bake that acquires and never releases leaks
// one of `Slots` -- which defaults to 2."  True, and the way not to leak a
// slot is NOT to take one.  **This block never issues `OP_ACQUIRE` and never
// issues `OP_RELEASE`.  It issues `OP_READ` and nothing else.**
//
// That is not thrift, it is the only reading that survives R221.  Look at what
// `OP_ACQUIRE` does to a handle that is not resident
// (`zhao_surface_sheet.sv`, `do_acquire_new`):
//
//     dir_live[req_free_slot] <= 1'b1;  clr_active <= 1'b1;  ...
//     pg_status_q <= StAllocated;       // after a 4,096-cycle clear sweep
//
// It ALLOCATES A BLANK SHEET and answers `ST_ALLOCATED`.  Every texel of that
// sheet reads zero, every vertex is therefore uncovered, and the record digs
// nothing -- which is R221's explicitly REFUSED option, *"dig zero: a visible
// no-op -- they acted, the ground did not move"*, wearing a residency costume
// so that no status code says a miss happened.  It would also steal one of
// `Slots = 2` from SURFACE.STAMP, which is the only block terrain_rules 7
// allows to write layer F, to hand the bake an empty page.
//
// `OP_READ` is the only opcode that reports residency WITHOUT changing it:
// `do_read_hit` requires `req_hit`, and the default `pg_status_q <= StMiss`
// catches every other case with `dir_live` untouched.  So the lifetime
// question has an answer and the answer is that this block has no lifetime:
// SURFACE.STAMP owns acquisition, and the bake reader is a pure observer of
// residency.  It cannot leak a slot because it never holds one.
//
// The handle itself is an INPUT (`job_handle_i`) and is not derived from
// `cmd_patch_id_i`.  `zhao_surface_sheet`'s choice C4 says why: "The handle is
// the identity the ABI carries (`commands.zidl` SurfaceStamp
// `handle32[patch] patch`); using anything else re-derives identity that was
// already stated."  Constructing a 32-bit handle from a 16-bit patch id here
// would be a second identity law, invented in the one place nobody would look
// for one.
//
// ===========================================================================
// ITEM 2 -- THE LATENCY ADAPTER: BOTH OPTIONS PRICED, AND THE PRICE MOVED
// ===========================================================================
// The commissioning note prices it as "1,089 round trips per record, *or* an
// 8,192-byte second copy of layer F".  The second figure is an OVERSTATEMENT
// BY 7.5x and the correction is what decides the choice, so it is measured
// here rather than inherited (R165: a blocker is a claim about a moment).
//
// A. THE PREFETCH IS 1,089 BYTES, NOT 8,192.  Two independent halvings, both
//    read off the RTL:
//
//    * **Bake never reads the tag.**  `zhao_terrain_bake_v2` has
//      `sheet_strength_i` and no tag port at all; grep the file for `sheet_`
//      and the ports are `sheet_texel_o`, `sheet_strength_i` and
//      `sheet_vertices_dug_o`.  Layer F is {tag u8, strength u8}, so half of
//      the 8,192 bytes is a plane this consumer cannot see.  4,096 left.
//
//    * **Only 1,089 of the 4,096 texels are ADDRESSABLE.**  Section 9.3(b)'s
//      law, as `zhao_terrain_stampdepth` states it, is
//          ti = (vi >= 32) ? 63 : 2*vi        tj likewise
//      so the 33x33 lattice samples a decimated grid: ti takes 33 distinct
//      values (0, 2, ... 62, 63) and tj the same.  33 x 33 = 1,089 texels.
//      The other 3,007 can never be asked for.
//
//    1,089 bytes = 8,712 bits, which is ONE M10K of the 553 on the device
//    (306 in use at R87).  The feared figure was 65,536 bits, about seven.
//    This is not "a second copy of layer F": it is layer F RESAMPLED ONTO THE
//    LATTICE, which is a different and much smaller object.
//
// B. THE PREFETCH COSTS ~1,089 CYCLES, ONCE PER RECORD, OFF THE DIG'S PATH.
//    `zhao_surface_sheet` accepts a request whenever its single response slot
//    is free (`req_ready_o = !clr_active && !pend_valid && pg_slot_free`, and
//    `pg_slot_free = !pg_valid_q || pg_ready_i`), so with the response drained
//    every cycle the port sustains ONE READ PER CLOCK with a one-cycle memory
//    latency.  The fill is therefore ~1,090 cycles plus whatever the
//    round-robin share gives away to SURFACE.STAMP -- about 13 cycles at the
//    ledger's "one texel per ~83 clocks".
//
// C. THE PER-VERTEX ROUND TRIP IS NOT 1,089 CYCLES, IT IS AN INTRUSION.  Three
//    things kill it, and only the first is about speed:
//
//    * It is ~2 cycles per vertex IN SERIES with the dig, ~2,178 added cycles,
//      against ~1,090 off to the side.  Twice the time, in the worse place.
//    * `sheet_strength_i` is sampled with `vtx_valid_i`, which THIS BLOCK DOES
//      NOT OWN -- `zhao_terrain_pagestream` drives layers A/B/C and
//      `zhao_terrain_pageio` drives `nb_o`.  A per-vertex adapter would have
//      to gate a valid owned by two other blocks, so the blast radius is a
//      port change on the terrain page spine rather than one new file.
//    * **The sheet's latency is not bounded.**  `req_ready_o` is LOW for the
//      whole 4,096-cycle clear sweep of an allocating ACQUIRE.  In the
//      per-vertex arrangement a stamp's ACQUIRE lands the dig in a 4,096-cycle
//      stall in the middle of a lattice, holding the A/B/C page beat.  In the
//      prefetch arrangement the fill phase absorbs it and the dig never sees
//      it.
//
// **TAKEN: THE PREFETCH.**  One M10K, ~1,090 cycles per sheet record, no port
// change anywhere outside this block, and no coupling to bake's traversal
// order.  A disc record prefetches NOTHING and pays nothing.
//
// ===========================================================================
// AND THE ANSWER IS STILL A VALID, NOT AN ASSUMPTION ABOUT BAKE'S TIMING
// ===========================================================================
// A 1,089-byte store that must answer combinationally is 8,712 FLOPS, which is
// eight times `zhao_terrain_pageio`'s shadow plane and not affordable.  A
// synchronous M10K read answers one cycle late.  Reading bake's state machine
// shows the address is stable for three cycles before `vtx_ready_o` rises
// (`StEmit` advances the cursor, then `StVxM`, `StVxC`, `StDxM`, then
// `StVtx`), so a registered read would always be in time --
//
// -- and that is a COUPLING TO A TRAVERSAL, which is exactly the trade
// `zhao_terrain_pageio` wrote down and refused for its own shadow plane ("it
// trades an area number for a coupling to TERRAIN.BAKE's traversal order").
// So this block does not take it either.  It publishes `str_valid_o`, which
// the composer ANDs into bake's `vtx_valid_i`, and bake's own port comment
// already specifies exactly that contract:
//
//     "the page server delivers all five together or the vertex is not ready."
//
// The result costs nothing in the real machine and stays correct if bake's
// timing ever moves.  `dig_stall_cycles_o` MEASURES the difference rather than
// asserting it: it counts cycles in which bake was ready for a vertex and this
// block could not answer.  It reads ZERO on the real traversal -- which is a
// claim, and the claim to check hardest, so `sheetseam_rtl_directed` FIRES it
// by holding `dig_ready_i` high while jumping the cursor.
//
// THE HELD ANSWER IS KEYED ON THE ADDRESS IT WAS READ AT.  `rd_texel_q` is
// loaded by THIS block's read issue; `sheet_texel_i` is driven by BAKE's
// cursor.  Two different enables, so the comparison can see a timing fault and
// not only a value fault -- CLAUDE.md's "a detector wired to two operands that
// move together cannot fire", applied before the fact rather than after it.
// The same shape guards the prefetch: `pf_handle_q` is latched by this block
// at fill start and differenced against the handle the producer is OFFERING,
// so a record that changes underneath a running prefetch is caught and
// re-fetched (`refetches_o`) instead of digging one patch's crater with
// another patch's sheet.
//
// ===========================================================================
// ITEM 4 -- OWNER RULING R221, THE ST_MISS LAW
// ===========================================================================
// **"RULED: fall back to the parametric disc, and COUNT the fallback."**
//
// The fallback is not a third crater shape and this block does not compute
// one.  It is ONE BIT: `bk_depth_sheet_o` goes LOW, and
// `zhao_terrain_bake_v2` runs the law it has always run.  R221's own reasoning
// is why that is the whole implementation:
//
//     "The disc is not an invention -- it is the RATIFIED v1 LAW.  SEAMDIG
//      measured the sheet mode to be additive: `terrain_bake_v2_directed`
//      passes 267/267 unchanged with the sheet arm present.  So the fallback
//      reaches behaviour already ratified, tested and shipped."
//
// A record that falls back is therefore bit-identical to the same record with
// `cmd_depth_sheet_i` low, and that equivalence is not an argument here: it is
// 267 checks that already exist.
//
// WHAT COUNTS AS A MISS, and it is wider than `ST_MISS` on purpose.  ANY
// response that is not `ST_HIT` fails the prefetch:
//
//   ST_MISS       the handle is not resident.  The ruled case.
//   ST_ALLOCATED  unreachable from here (we never ACQUIRE) and, if it ever
//                 arrived, it would mean a blank sheet -- the refused "dig
//                 zero".  Treated as a miss so it can never be served.
//   ST_OVERFLOW   likewise unreachable from here; likewise not a sheet.
//
// A miss ANYWHERE in the 1,089 reads fails the WHOLE record, not the vertex.
// Mixing laws inside one crater would invent a fourth shape -- half sheet,
// half disc, with a seam nobody authored -- and R221 forbids inventing a third.
// It also means a sheet RELEASEd mid-prefetch (the only way residency changes
// under us) lands on the ratified law rather than on a half-read page, and
// `miss_texels_o` records how far the fill got.
//
// `fallbacks_o` IS R221's mandated counter and its positive control is
// STIMULUS, not a mutant: a miss is legally reachable at this block's own port
// by offering a handle nobody acquired, which is ordinary input.  That is the
// same distinction `zhao_terrain_pageio` drew between its five stimulus-fired
// counters and `wq_overflow_o`, which needs `tests/mutants/` because no legal
// input can reach it.
//
// ===========================================================================
// WHAT THIS BLOCK IS NOT
// ===========================================================================
// No writes: it never touches `wr_*`, because terrain_rules 7 gives layer F
// one writer and it is SURFACE.STAMP.  No arbitration: that is
// `zhao_surface_sheetshare`, and this block drives a plain client port.  No
// record storage: the producer holds the other twelve `cmd_*` fields under the
// ready/valid contract while this block refuses, so nothing is copied and
// nothing can go stale.  No second spelling of section 9.3(b): the fill
// address comes from a `zhao_terrain_stampdepth` instance, the same module
// bake reads the law from, because "a fifth spelling of the same thing is a
// fifth thing that can be subtly different".
//
// Conservative SystemVerilog subset only (charter S2); no package deps.

module zhao_terrain_sheetseam #(
    // The lattice.  Island Patch v1 is 33x33 vertices (terrain_rules 2).
    parameter int unsigned Lat = 33,
    // Layer F is 64x64 texels, FROZEN by owner ruling R194.
    parameter int unsigned SheetEdge = 64
) (
    input var logic clk,
    input var logic rst_n,

    // -----------------------------------------------------------------------
    // THE JOB FACE -- the bake record's sheet identity, WATCHED not copied
    // -----------------------------------------------------------------------
    // `job_valid_i` is the producer's `cmd_valid`; `job_ready_o` is what the
    // producer sees as `cmd_ready`.  This block holds `job_ready_o` LOW while
    // it prefetches, so the producer keeps every field stable under the
    // ready/valid contract and nothing here has to store the record.
    input  var logic        job_valid_i,
    output var logic        job_ready_o,
    input  var logic [31:0] job_handle_i,       // handle32, the ABI's identity
    input  var logic        job_want_sheet_i,   // the record asked for layer F
    input  var logic [15:0] job_src_id_i,

    // -----------------------------------------------------------------------
    // TO TERRAIN.BAKE's cmd PORT -- one field decided, the rest pass by
    // -----------------------------------------------------------------------
    output var logic        bk_valid_o,         // -> zhao_terrain_bake_v2.cmd_valid_i
    input  var logic        bk_ready_i,         // <- zhao_terrain_bake_v2.cmd_ready_o
    output var logic        bk_depth_sheet_o,   // -> cmd_depth_sheet_i (R221: LOW on a miss)
    output var logic        bk_fallback_o,      // valid with bk_valid_o: this record fell back

    // -----------------------------------------------------------------------
    // THE LAYER-F READ FACE -- to TERRAIN.BAKE's dig
    // -----------------------------------------------------------------------
    input  var logic [11:0] sheet_texel_i,      // <- zhao_terrain_bake_v2.sheet_texel_o
    output var logic [ 7:0] sheet_strength_o,   // -> zhao_terrain_bake_v2.sheet_strength_i
    // AND this into bake's `vtx_valid_i`.  HIGH throughout a record that is
    // not on the sheet law, so a disc record is never slowed by this block.
    output var logic        str_valid_o,
    input  var logic        dig_ready_i,        // <- zhao_terrain_bake_v2.vtx_ready_o
    input  var logic        bake_done_i,        // <- zhao_terrain_bake_v2.bake_done_o

    // -----------------------------------------------------------------------
    // THE SHEET CLIENT PORT -- into zhao_surface_sheetshare's client B
    // -----------------------------------------------------------------------
    output var logic        req_valid_o,
    input  var logic        req_ready_i,
    output var logic [ 1:0] req_op_o,
    output var logic [31:0] req_handle_o,
    output var logic [11:0] req_texel_o,
    output var logic [15:0] req_src_id_o,

    input  var logic        pg_valid_i,
    output var logic        pg_ready_o,
    input  var logic [ 1:0] pg_status_i,
    input  var logic [ 7:0] pg_strength_i,

    // -----------------------------------------------------------------------
    // evidence (spec/counters.md S4: saturate, never wrap)
    // -----------------------------------------------------------------------
    output var logic [31:0] jobs_o,              // records admitted to bake
    output var logic [31:0] sheet_served_o,      // ... on the layer-F law
    output var logic [31:0] fallbacks_o,         // ... R221: asked for the sheet, got the disc
    output var logic [31:0] miss_texels_o,       // non-ST_HIT responses seen
    output var logic [31:0] prefetch_beats_o,    // texels actually read in
    output var logic [31:0] refetches_o,         // the offered job moved under a fill
    output var logic [31:0] dig_stall_cycles_o,  // bake was ready and we were not
    output var logic [31:0] bad_texels_o,        // an address no lattice vertex can produce
    output var logic [31:0] stray_done_o,        // bake_done with no record in flight
    output var logic        idle_o
);

  // ---- frozen shape --------------------------------------------------------
  localparam int unsigned Texels = Lat * Lat;  // 1,089 -- see ITEM 2 above
  localparam int unsigned IdxW = 11;  // holds 0..1089 inclusive
  localparam logic [5:0] LatMax = 6'd32;  // Lat - 1
  localparam logic [5:0] TexelMax = 6'd63;  // SheetEdge - 1

  // ---- SURFACE.SHEET's opcodes and statuses, mirrored ----------------------
  // Mirrored rather than imported: `zhao_surface_sheet.sv` declares them as
  // localparams in its own body and there is no package.  `zhao_texture_aux.sv`
  // mirrors the identical set the identical way, so this is the tree's
  // established form, not a new one.  The values are checked against the
  // store's own by `tests/terrain/sheetseam_rtl_directed.cpp`, which drives
  // the REAL `zhao_surface_sheet`.
  localparam logic [1:0] OpRead = 2'd1;
  localparam logic [1:0] StHit = 2'd0;

  // ---- states --------------------------------------------------------------
  localparam logic [1:0] S_WATCH = 2'd0;  // no record in flight
  localparam logic [1:0] S_FILL = 2'd1;  // prefetching for pf_handle_q
  localparam logic [1:0] S_READY = 2'd2;  // the verdict is available
  localparam logic [1:0] S_BUSY = 2'd3;  // bake holds the record

  logic [1:0] state_q;

  // ---- the prefetch --------------------------------------------------------
  logic [31:0] pf_handle_q;  // the handle the fill is FOR
  logic [15:0] pf_src_id_q;
  logic pf_missed_q;  // a non-ST_HIT response was seen
  logic [IdxW-1:0] i_idx_q;  // issue cursor,  0..Texels
  logic [IdxW-1:0] w_idx_q;  // write cursor,  0..Texels
  logic [1:0] out_q;  // requests issued, responses not yet back

  // ---- the lattice-resampled layer F, 1,089 x 8 ----------------------------
  // Deliberately NOT reset and read in the SAME always_ff as its write, which
  // is `zhao_surface_sheet`'s own measured M10K template
  // (`calib_ram_8192x8_shared_re`): a reset loop over the array and a split
  // process are two of QUARTUS_GOTCHAS 10's three killers of storage
  // inference, and there are no byte enables here to be the third.
  logic [7:0] str_q[Texels];
  logic [7:0] rd_data_q;
  logic [11:0] rd_texel_q;
  logic rd_valid_q;

  // ---- the record in flight ------------------------------------------------
  logic serve_q;  // bake is digging on the layer-F law
  logic busy_sheet_q;  // ... and therefore owes us a bake_done

  // =========================================================================
  // THE FILL ADDRESS -- section 9.3(b), from the module that states it
  // =========================================================================
  // `w_idx_q` and `i_idx_q` walk the lattice in the same z-then-x order bake
  // does, and the texel for each is `zhao_terrain_stampdepth`'s `texel_o`.
  // Writing `ti = 2*vi` here instead would be a second spelling of a ratified
  // law inside the one block whose job is to feed that law's consumer.
  logic [5:0] f_vi_q, f_vj_q;  // the ISSUE cursor as a lattice vertex
  logic [11:0] fill_texel_c;
  logic signed [31:0] sd_unused_fx16;
  logic signed [31:0] sd_unused_h16;
  logic sd_unused_cov;

  zhao_terrain_stampdepth #(
      .SheetEdge(SheetEdge),
      .Lat(Lat)
  ) u_fill_addr (
      .vi_i(f_vi_q),
      .vj_i(f_vj_q),
      .texel_o(fill_texel_c),
      // The depth half of the law is not used HERE -- bake owns it, through
      // its own instance, from the registered strength.  This instance exists
      // for the ADDRESS only.  The three outputs below are deliberately dead:
      // the arithmetic behind them is a pure function of `strength_i`, which
      // is tied off, so synthesis prunes the whole art table and this costs
      // the two multiplexers the address law is made of.
      .strength_i(8'd0),
      .depth_fx16_o(sd_unused_fx16),
      .depth_h16_o(sd_unused_h16),
      .covered_o(sd_unused_cov)
  );

  // The lint tool reports the three above as unused, which is TRUE and
  // INTENDED.  Naming them in a concatenation nobody reads would hide the
  // fact; this states it where a reader looking for "why is that connected to
  // nothing" will find it.  (The comment does not begin with the tool's name
  // on purpose: a comment whose first word is that name is parsed as a pragma
  // and rejected -- BADVLTPRAGMA, which is how this line first failed.)
  /* verilator lint_off UNUSEDSIGNAL */
  wire _unused_sd = &{1'b0, sd_unused_fx16, sd_unused_h16, sd_unused_cov};
  /* verilator lint_on UNUSEDSIGNAL */

  // =========================================================================
  // THE READ ADDRESS -- the inverse of the same law
  // =========================================================================
  // ti is 2*vi for vi in 0..31 and 63 for vi = 32, so the inverse is exact:
  // vi = (ti == 63) ? 32 : ti >> 1.  An ODD ti below 63 is an address no
  // lattice vertex can produce; it aliases down to its floor and is COUNTED
  // rather than stalled, because a silent stall is worse evidence than a
  // counted anomaly (`zhao_terrain_psmux`'s rule about stray beats).
  wire [5:0] off_ti = sheet_texel_i[5:0];
  wire [5:0] off_tj = sheet_texel_i[11:6];
  wire [5:0] off_vi = (off_ti == TexelMax) ? LatMax : {1'b0, off_ti[5:1]};
  wire [5:0] off_vj = (off_tj == TexelMax) ? LatMax : {1'b0, off_tj[5:1]};
  wire off_legal = (off_ti == TexelMax || off_ti[0] == 1'b0) &&
                   (off_tj == TexelMax || off_tj[0] == 1'b0);

  // idx = vj*Lat + vi.  Lat is 33 = 32 + 1, so this is a shift and two adds
  // and NO multiplier -- the same reason `zhao_terrain_stampdepth` writes its
  // interpolation as shift-adds.
  wire [IdxW-1:0] rd_idx_c = {off_vj, 5'b0} + {5'b0, off_vj} + {5'b0, off_vi};

  // THE HELD ANSWER, keyed on the address it was read at.  `rd_texel_q` is
  // loaded by this block's read issue; `sheet_texel_i` comes from bake's
  // cursor.  Different enables -- see the header.
  wire rd_held_c = rd_valid_q && (rd_texel_q == sheet_texel_i);
  wire rd_issue_c = serve_q && !rd_held_c;

  assign sheet_strength_o = serve_q ? rd_data_q : 8'd0;
  // HIGH when this block is not serving, so a disc record -- or a record that
  // fell back under R221 -- runs at full speed with this AND in its valid.
  assign str_valid_o = serve_q ? rd_held_c : 1'b1;

  // =========================================================================
  // THE CLIENT PORT
  // =========================================================================
  // OP_READ and nothing else.  See ITEM 1 in the header: an ACQUIRE would
  // allocate a blank sheet and serve R221's refused "dig zero".
  assign req_valid_o = (state_q == S_FILL) && !pf_missed_q && (i_idx_q != IdxW'(Texels));
  assign req_op_o = OpRead;
  assign req_handle_o = pf_handle_q;
  assign req_texel_o = fill_texel_c;
  assign req_src_id_o = pf_src_id_q;
  // Always take the response.  The fill is the only thing that can produce
  // one, and leaving it unconsumed would wedge the shared store for the stamp.
  assign pg_ready_o = 1'b1;

  wire req_fire_c = req_valid_o && req_ready_i;
  wire pg_fire_c = pg_valid_i && pg_ready_o;

  // The fill's completion test, written on NEXT values so the last response
  // and the transition happen in one cycle instead of costing an idle beat.
  // `out_next == 0` is the whole of "nothing is in flight"; a request that
  // fired this cycle makes it 1 and defers the verdict, which is why the
  // abort path needs no separate drain state.
  wire [IdxW-1:0] w_next_c = pg_fire_c ? (w_idx_q + IdxW'(1)) : w_idx_q;
  wire [1:0] out_next_c = out_q + (req_fire_c ? 2'd1 : 2'd0) - (pg_fire_c ? 2'd1 : 2'd0);
  wire miss_next_c = pf_missed_q || (pg_fire_c && (pg_status_i != StHit));
  wire fill_done_c = (out_next_c == 2'd0) && (miss_next_c || (w_next_c == IdxW'(Texels)));

  // =========================================================================
  // THE JOB FACE AND THE VERDICT
  // =========================================================================
  // The offered job is matched against the fill that was performed FOR it.
  // If the producer changes the handle or the mode under a completed
  // prefetch, this goes false and the fill restarts -- the record-swap defect
  // designed out rather than counted after the fact.
  wire job_match_c = (state_q == S_READY) && job_want_sheet_i && (job_handle_i == pf_handle_q);
  // A record that does not want layer F needs nothing from this block and is
  // passed through in the cycle it is offered.
  wire pass_now_c = (state_q == S_WATCH) && !job_want_sheet_i;

  wire admit_c = pass_now_c || job_match_c;

  assign bk_valid_o = job_valid_i && admit_c;
  assign job_ready_o = admit_c && bk_ready_i;
  // R221: the sheet law survives only if every one of the 1,089 reads hit.
  assign bk_depth_sheet_o = job_match_c && !pf_missed_q;
  assign bk_fallback_o = job_match_c && pf_missed_q;

  wire accept_c = bk_valid_o && bk_ready_i;

  assign idle_o = (state_q == S_WATCH) && (out_q == 2'd0) && !busy_sheet_q;

  function automatic logic [31:0] sat_inc(input logic [31:0] a);
    sat_inc = (a == 32'hFFFF_FFFF) ? a : a + 32'd1;
  endfunction

  // =========================================================================
  // THE STORE -- one process, synchronous read, no reset, no byte enables
  // =========================================================================
  always_ff @(posedge clk) begin
    if (pg_fire_c && (pg_status_i == StHit) && (state_q == S_FILL))
      str_q[w_idx_q] <= pg_strength_i;
    if (rd_issue_c) rd_data_q <= str_q[rd_idx_c];
  end

  // =========================================================================
  // CONTROL
  // =========================================================================
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state_q <= S_WATCH;
      pf_handle_q <= 32'd0;
      pf_src_id_q <= 16'd0;
      pf_missed_q <= 1'b0;
      i_idx_q <= '0;
      w_idx_q <= '0;
      out_q <= 2'd0;
      f_vi_q <= 6'd0;
      f_vj_q <= 6'd0;
      rd_texel_q <= 12'd0;
      rd_valid_q <= 1'b0;
      serve_q <= 1'b0;
      busy_sheet_q <= 1'b0;
      jobs_o <= 32'd0;
      sheet_served_o <= 32'd0;
      fallbacks_o <= 32'd0;
      miss_texels_o <= 32'd0;
      prefetch_beats_o <= 32'd0;
      refetches_o <= 32'd0;
      dig_stall_cycles_o <= 32'd0;
      bad_texels_o <= 32'd0;
      stray_done_o <= 32'd0;
    end else begin
      // ---- the held read answer -----------------------------------------
      if (rd_issue_c) begin
        rd_texel_q <= sheet_texel_i;
        rd_valid_q <= 1'b1;
      end

      // ---- bake was ready and we were not --------------------------------
      // A CYCLE count, and the name says so.  Its two operands are bake's
      // `dig_ready_i` and our own held-address compare, which are driven by
      // different machines.  It reads ZERO on the real traversal; the
      // directed test fires it deliberately before that zero is quoted.
      if (serve_q && dig_ready_i && !rd_held_c) dig_stall_cycles_o <= sat_inc(dig_stall_cycles_o);
      // Counted on the ISSUE, not on the level: the offered texel is a level
      // held for many cycles, so counting cycles would read orders of
      // magnitude high -- the defect `zhao_console_core.sv` records at
      // `terr_pl_slot_overflow_o`. One illegal address, one count.
      if (rd_issue_c && !off_legal) bad_texels_o <= sat_inc(bad_texels_o);

      // ---- the request / response bookkeeping ----------------------------
      // `out_q` is 0 or 1 while `zhao_surface_sheetshare` is correct: the
      // share holds one transaction and the store holds one response.  It is
      // two bits and carries an assertion rather than a counter, because a
      // counter that no legal input can move is a counter that owes a
      // committed mutant, and the honest instrument for a structural
      // invariant is an assertion that fires in simulation.
      if (req_fire_c && !pg_fire_c) out_q <= out_q + 2'd1;
      else if (pg_fire_c && !req_fire_c) out_q <= out_q - 2'd1;

      case (state_q)
        // -----------------------------------------------------------------
        S_WATCH: begin
          serve_q <= 1'b0;
          if (job_valid_i && job_want_sheet_i) begin
            pf_handle_q <= job_handle_i;
            pf_src_id_q <= job_src_id_i;
            pf_missed_q <= 1'b0;
            i_idx_q <= '0;
            w_idx_q <= '0;
            f_vi_q <= 6'd0;
            f_vj_q <= 6'd0;
            state_q <= S_FILL;
          end else if (accept_c) begin
            // A disc record: admitted in this cycle, nothing prefetched.
            jobs_o <= sat_inc(jobs_o);
            busy_sheet_q <= 1'b1;
            rd_valid_q <= 1'b0;
            state_q <= S_BUSY;
          end
        end

        // -----------------------------------------------------------------
        S_FILL: begin
          // THE RECORD MOVED UNDER THE FILL.  The producer may only change
          // the offered record by dropping `valid`, but if it does, the bytes
          // being read belong to a patch nobody is baking.  Abandon and count
          // rather than serve one patch's crater from another's sheet.
          if (!job_valid_i || !job_want_sheet_i || (job_handle_i != pf_handle_q)) begin
            refetches_o <= sat_inc(refetches_o);
            state_q <= S_WATCH;
          end else begin
            if (req_fire_c) begin
              i_idx_q <= i_idx_q + IdxW'(1);
              if (f_vi_q == LatMax) begin
                f_vi_q <= 6'd0;
                f_vj_q <= f_vj_q + 6'd1;
              end else begin
                f_vi_q <= f_vi_q + 6'd1;
              end
            end
            if (pg_fire_c) begin
              w_idx_q <= w_idx_q + IdxW'(1);
              if (pg_status_i == StHit) begin
                prefetch_beats_o <= sat_inc(prefetch_beats_o);
              end else begin
                // R221: ANY non-hit fails the WHOLE record.  Mixing laws
                // inside one crater would invent a fourth shape.
                pf_missed_q   <= 1'b1;
                miss_texels_o <= sat_inc(miss_texels_o);
              end
            end
            // Done when nothing is outstanding and either the lattice is
            // complete or a miss has stopped the issue.
            if (fill_done_c) state_q <= S_READY;
          end
        end

        // -----------------------------------------------------------------
        S_READY: begin
          if (accept_c) begin
            jobs_o <= sat_inc(jobs_o);
            busy_sheet_q <= 1'b1;
            rd_valid_q <= 1'b0;
            if (pf_missed_q) begin
              // ***** OWNER RULING R221, IN ONE PLACE *****
              // The record goes to bake with `cmd_depth_sheet_i` LOW, which
              // is the ratified parametric disc, and the fallback is counted
              // because "an uncounted fallback is a console quietly serving
              // the wrong crater shape with no way to know how often".
              fallbacks_o <= sat_inc(fallbacks_o);
              serve_q <= 1'b0;
            end else begin
              sheet_served_o <= sat_inc(sheet_served_o);
              serve_q <= 1'b1;
            end
            state_q <= S_BUSY;
          end else if (!job_valid_i || !job_want_sheet_i || (job_handle_i != pf_handle_q)) begin
            // The verdict was for a record nobody is offering any more.
            refetches_o <= sat_inc(refetches_o);
            state_q <= S_WATCH;
          end
        end

        // -----------------------------------------------------------------
        S_BUSY: begin
          if (bake_done_i) begin
            busy_sheet_q <= 1'b0;
            serve_q <= 1'b0;
            rd_valid_q <= 1'b0;
            pf_missed_q <= 1'b0;
            state_q <= S_WATCH;
          end
        end

        default: state_q <= S_WATCH;
      endcase

      // A completion with no record in flight is a real fault and this is the
      // one place it can be seen.  Counted outside the case so the S_BUSY arm
      // cannot mask it.
      if (bake_done_i && !busy_sheet_q) stray_done_o <= sat_inc(stray_done_o);
    end
  end

  // ==========================================================================
  // ELABORATION GUARDS
  // ==========================================================================
  // Inside `initial begin ... end`: a bare module-scope `if` is lint-clean
  // under Verilator with ZERO diagnostics and a SYNTAX ERROR in quartus_map
  // (CLAUDE.md, proven twice on 2026-09-08).  And `--lint-only` does not run
  // `initial` blocks, so a clean lint says nothing about these -- the directed
  // test's parameterised build is what elaborates them.
  initial begin
    if (SheetEdge != 64)
      $fatal(
          1,
          "zhao_terrain_sheetseam: SheetEdge must be 64 -- the terrain page format is FROZEN by owner ruling R194"
      );
    if (Lat != 33)
      $fatal(
          1, "zhao_terrain_sheetseam: Lat must be 33 -- the Island Patch v1 lattice (terrain_rules 2)"
      );
    if (SheetEdge != 2 * (Lat - 1))
      $fatal(
          1,
          "zhao_terrain_sheetseam: SheetEdge must be 2*(Lat-1) -- the read-address inverse of 9.3(b) is only exact on that ratio"
      );
    if (Texels != 1089)
      $fatal(1, "zhao_terrain_sheetseam: the prefetch store must be Lat*Lat = 1089 entries");
  end

`ifndef SYNTHESIS
  // The structural invariant ITEM 2 rests on: the share holds one transaction
  // and the store one response, so this block can never have two reads in
  // flight.  An ASSERTION and not a counter -- a counter no legal input can
  // move owes a committed mutant, and this is a statement about the
  // COMPOSITION rather than about this design's own reachable states.
  //
  // The `armed_q` guard is `zhao_terrain_pageio.sv`'s shape, copied rather
  // than re-derived, and its header says why: reading `rst_n` synchronously
  // here while the sequencer reads it asynchronously is SYNCASYNCNET -- a
  // real warning about a real thing, in a file where the async reset is
  // deliberate. This block hit that warning on its first lint.
  logic armed_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) armed_q <= 1'b0;
    else armed_q <= 1'b1;
  end

  always_ff @(posedge clk) begin
    if (armed_q) begin
      a_one_read_in_flight :
      assert (out_q <= 2'd1)
      else $error("sheetseam: more than one sheet read in flight (out_q=%0d)", out_q);

      a_responses_follow_requests :
      assert (!(state_q == S_FILL) || (w_idx_q <= i_idx_q))
      else $error("sheetseam: more responses than requests");
    end
  end
`endif

endmodule : zhao_terrain_sheetseam
