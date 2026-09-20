// zhao_terrain_bake_v2.sv — TERRAIN.BAKE, rearchitected to spend memory and
// a SINGLE multiplier instead of seven private ones. (2026-09-09 terrain
// rescue, per reports/terrain-recon/ARCHITECT-BRIEF-terrain-rearchitecture.md
// target 1.)
//
// PORT-COMPATIBLE with zhao_terrain_bake: same ports, same laws, same
// counters, same handshake contracts. Latency grows (see THE PRICE below);
// nothing else may move, and tests/terrain/terrain_bake_v2_directed.cpp holds
// it to the same zref oracle the v1 suite uses, boundary for boundary.
//
// Law, in citation order (identical to v1 — the disc law did not move; the
// per-vertex layer-F mode added 2026-09-20 is additive and cites §9.3):
//   design/contracts/TERRAIN.BAKE.md — the block contract.
//   spec/terrain_rules.md §9.3 — stamp-to-bake: the ART depth table and the
//       nearest-texel resample, implemented by `zhao_terrain_stampdepth`.
//   spec/terrain_rules.md §2 §3.3 §3.4 §7 §9/§9.2 — layers, corner shadow,
//       breach law, ownership, cadence budget.
//   spec/qformats.md §2/§3/§4/§9 — height16/fx16, rescale, round-half-up.
//   reference/src/zterrain/terrain_core.cpp `bake_dig`, `apply_breach_law`,
//       `zref::terrain::lattice_lerp` — THE EXECUTED LAW.
//
// ---------------------------------------------------------------------------
// WHAT STOPS THIS BLOCK BEING COMPOSED — 2026-09-20 (terrain7)
// ---------------------------------------------------------------------------
// Kept here rather than only in `zhao_console_core.sv`'s entry I32, because
// this is where the next person composing it will look, and three of the four
// items below were found by re-checking a blocker that had stopped being true.
//
// 1. THE A/B/C READ SIDE IS SOLVED. THE LAYER-D READ SIDE IS NOT, AND
//    `u_terrain_pagestream` already emits `v_base_o`/`v_scar_o`/`v_bottom_o`/
//    `v_vi_o`/`v_vj_o` — this block's `vtx_base_i`/`vtx_scar_i`/`vtx_bottom_i`
//    and its two index outputs PORT FOR PORT, same widths, same signedness,
//    same vi=column/vj=row convention. `zhao_terrain_psmux` is the composed
//    two-client share of that exact stream, so a third client is a widening,
//    not a new arbiter. What is left is a CURSOR-MATCH ADAPTER (the streamer
//    pushes, this block pulls) and `vtx_nobake_i`, which has no producer.
//
// 1b. **LAYER D HAS NO READER ANYWHERE IN THE MACHINE**, and this block needs
//    it on TWO ports, one per phase. Found 2026-09-20 (terrain8) by re-checking
//    item 1 rather than inheriting it; the correction runs in the direction of
//    MORE missing work, which is why it had not been made.
//      * `vtx_nobake_i` -- the sec 3.3 corner shadow, a reduction over up to
//        four layer-D cells. The entry above already names it.
//      * `cell_state_i` (with `cell_ci_o`/`cell_cj_o`) -- the BREACH phase's
//        32x32 layer-D read-modify-write. NOT previously recorded anywhere.
//    `zref_terrain_page.hpp:317` puts layer D at page offset 6,598. SEARCHED
//    every .sv/.v/.txt/.qsf/.yml under fpga/ for 6598, `D_OFF` and kLayerDOff:
//    ZERO hits that are a page offset (the matches are `d_off`/`rd_off` in
//    FIELD and CMD.DMA, and the area figure "6,598 ALM"). `pagestream.sv:251`
//    reads exactly '{A_OFF, B_OFF, C_OFF}; `pageloader` writes whole pages and
//    reads none back; `writeback` is layer F only. This is the same shape as
//    the layer-E absence at offset 7,622: TWO of the eight page layers have no
//    reader, and both are on this block's critical path.
//
// 1c. AND THE SEAM `zhao_console_core.sv:2546-2551` NAMES IS NOT A PAGE WRITE.
//    Mapping `cs_event_o`/`cs_sub_o`/`cs_ci_o`/`cs_cj_o` onto
//    `zhao_terrain_compcache_front`'s `cs_we_i`/`cs_w_substance_i`/`cs_w_ci_i`/
//    `cs_w_cj_i` is real and useful and it is an ON-CHIP MIRROR: that block has
//    no VRAM port, and it stores `logic [1:0] sub_m` -- the SUBSTANCE field
//    only. This block preserves sec 3.3's flag bits deliberately
//    (`cs_state_o <= {cell_state_i[7:2], sub_out}`) and through that seam all
//    six are DROPPED, while `cs_state_o`, the byte that carries them, has no
//    consumer at all. Both consumers are wanted; they are different consumers,
//    and only a page write discharges terrain_rules sec 7.
//
// 1d. THE FOUR UNSERVED PORTS ARE ONE BLOCK, NOT FOUR OWNERS.
//    `design/contracts/TERRAIN.PAGEIO.md` (written 2026-09-20, NOT BUILT)
//    specifies it: every one of them needs the same residency slot, the same
//    generation and the same guard socket on the page pool, and this block
//    bakes ONE record at a time with strictly sequential phases, so one agent
//    per bake serves all four by construction. It also makes that agent the
//    only honest writer of entry I27's `terr_dm_*`, because it is the only
//    thing that holds the slot and generation the patch was served under.
//
// 2. THE WRITE SIDE IS THE REAL HOLE AND NOBODY HAD WRITTEN IT DOWN.
//    **`sc_*`, the layer-B scar writeback, HAS NO CONSUMER ANYWHERE.**
//    `zhao_terrain_compcache_front` accepts cell-state writes and composed
//    heights and never a scar; `zhao_terrain_pagestream` is read-only;
//    `zhao_terrain_writeback` writes the F SHEET, not layer B. Nothing in the
//    closure writes a page's height layers at all. A bake whose scar cannot be
//    stored has not deformed anything — it has computed a deformation and
//    dropped it. The same absent block is why entry I27's deformation mark has
//    no writer and why I28's writeback has never seen a beat.
//
// 3. THIS BLOCK DOES NOT DRIVE THE DEFORMATION MARK AND CANNOT.
//    Entry I27 said "its writer is TERRAIN.BAKE". That is an INTENTION in the
//    ledger, not a port match: there is NO slot, NO generation, NO epoch and
//    NO per-layer dirty bit anywhere on this module. `cmd_patch_id_i` and
//    `cmd_src_id_i` are not residency handles. Whoever drives `terr_dm_*` must
//    hold the slot and generation the patch was served under and pair them
//    with `bake_done_o` — a third block, not a wire.
//
// 4. THE PER-VERTEX DEPTH MODE IS RULED AND IS NOW **BUILT**. This item used
//    to read "RULED AND NOT BUILDABLE YET", blocked on an ART JUDGEMENT.
//    **OWNER RULING R194, 2026-09-20, by looking: *"Shipped is fine. Slightly
//    different but not off."*** The nearest-texel rim is ACCEPTED, the terrain
//    page format is FROZEN AT 64x64 and `sheet_texel_for_vertex` stands as
//    written — it does NOT become the identity. The reader's address generator
//    was exactly the contested thing, and it is decided.
//
//    So Option A is implemented, and it is the ONLY item in this list that has
//    moved: this block KEEPS its parametric disc (`cmd_radius_i`,
//    `cmd_depth_from_i`/`_to_i`, oracle `zref::terrain::bake_dig`, 267 directed
//    checks, bit for bit unchanged) and has GAINED the second, per-vertex depth
//    mode on `cmd_depth_sheet_i`, fed from layer F through
//    `zhao_terrain_stampdepth` — spec/terrain_rules.md §9.3's two laws in RTL
//    for the first time, oracle `zref::terrain::stamp_depth_at_vertex`,
//    differenced exhaustively by `tests/terrain/terrain_stampdepth_directed.cpp`
//    and driven through a whole bake by
//    `tests/terrain/terrain_bake_v2_sheet_directed.cpp`. Do not re-open the
//    choice; do not re-open the format.
//
//    WHAT IT DOES NOT CLOSE, said plainly so the next reader does not have to
//    re-derive it: the mode needs somebody to SERVE `sheet_strength_i`, and
//    `zhao_surface_sheet`'s request port is annotated "SURFACE.STAMP is the
//    only requester". That share is a SCHEDULER — one small block with a
//    contract, the shape `zhao_terrain_psmux` and `zhao_mem_share_n` already
//    have twice. It is a smaller obstacle than items 1b, 2 and 3 above, and it
//    is not the reason this block is uncomposed.
//
// AND ONE THING THAT USED TO BE HERE IS **STRUCK, BOTH HALVES**, 2026-09-20
// (seamdig). It said `zhao_prod_top.sv` instantiates v1 and
// `fpga/quartus/prod_fit_sources.txt` carries the v1 file. Re-measured rather
// than inherited: `zhao_prod_top.sv` instantiates **`zhao_terrain_bake_v2`**
// (`u59_i`), adopted by owner ruling R86, and there is no
// `prod_fit_sources.txt` — the file is `prod_fit_sources.ORPHANED.txt`, whose
// own first line reads "ORPHANED 2026-09-09. NOTHING READS THIS FILE. Do not
// edit it and do not draw conclusions from it." The real closure is
// `design/fit_targets.yml` under `- top: zhao_prod_top`, which names v2. A
// warning that quotes a file nobody reads is worse than no warning: it sends
// the next packet to repair something that is not broken, using evidence that
// describes nothing.
//
// ---------------------------------------------------------------------------
// WHAT MOVED, AND WHY IT IS LEGAL
// ---------------------------------------------------------------------------
// V1 holds SEVEN multiplier sites (dx*dx, dz*dz, radius^2, lat_lerp x2 call
// sites, and bake_delta's two 32x18 products) and ZERO memory bits, with the
// 1,089-bit `meets` plane in flip-flops. The device has 41,910 ALM (139%
// committed), 112 DSP (171% committed) and ~400 FREE M10K. This block is the
// densest arithmetic-with-no-RAM block in terrain, and its own FSM is strictly
// sequential — one vertex at a time through a 17-cycle divide — so the seven
// multipliers are busy in DIFFERENT STATES and can be one physical multiplier
// with operand muxes, exactly the `mseq` pattern zhao_terrain_normals.sv:203
// and zhao_terrain_lod.sv:273 already ship.
//
// M1. ONE 34x34 SIGNED MULTIPLIER, `mul_p <= mul_a * mul_b`, product
//     registered, operands muxed by state. Every v1 product fits inside it:
//       radius^2      signed 32 x 32   (StRad)
//       span   * num  signed 33 x  7   (StVzM per row, StVxM per vertex)
//       dz     * dz   signed 33 x 33   (StDzM per row)
//       dx     * dx   signed 33 x 33   (StDxM per vertex)
//       depth  * s    signed 32 x 18   (StPfM/StPtM per covered vertex)
//     The product register has an ENABLE (`mul_go`): states that do not issue
//     a product hold the last one, which is what lets StVtx read dx^2 out of
//     `mul_p` while the handshake waits, and StEmit read p_to out of it after
//     the divide.
//
// M2. THE `meets` PLANE MOVES TO A RAM — and this REVISITS v1's chosen B2
//     rather than ignoring it. B2's rejected alternative was taking the meets
//     bits FROM THE CALLER, which moves the §3.4 breach equality out of the
//     only block terrain_rules §7 permits to own it. Moving the STORAGE into
//     a RAM inside this same block moves no law anywhere: the equality
//     (`composed18 <= bottom18`) is still computed here, per vertex, and the
//     stored bit is still this block's private bridge between its two phases.
//     B2's argument was about OWNERSHIP; this change is about SUBSTRATE.
//
//     The access shapes both fit one simple dual-port RAM:
//       DIG writes one full ROW per 33 vertices — bits accumulate in a 33-bit
//         row register (`wrow`) and the completed word is written once, at the
//         row's last vertex.
//       BREACH reads rows cj and cj+1 — a two-row REGISTER WINDOW
//         (`mrow_lo`/`mrow_hi`), with row cj+2 prefetched during the current
//         row's 32-cell scan (a scan is >= 32 cycles; the prefetch needs 1).
//     34 words x 33 bits = 1,122 bits — one M10K at any legal aspect
//     (256x40 holds it 7x over). The RAM is never reset: a breach phase can
//     only run after a full 33x33 dig sweep has written every row of the
//     current record, so pre-sweep contents are UNREACHABLE, not hazardous
//     (the zhao_proj_arena3 dense-fill argument; a reset loop would also
//     break M10K inference, QUARTUS_GOTCHAS 10).
//
// THE PRICE, MEASURED NOT GUESSED (terrain_bake_v2_directed.cpp measures both
// blocks' cycles on the same records):
//     covered vertex   v1: 19 clocks   v2: 24   (3 geometry muls + 2 delta muls)
//     uncovered vertex v1:  2 clocks   v2:  5
//     per row          v2 adds 4 (vz/dz^2 recompute), per record 1 (radius^2)
// The block is `backpressure: ready_valid, latency: variable`
// (design/blocks.yml), and §9.2's cadence law is built around bakes that DO
// NOT complete in their frame — the budget counts ACCEPTANCES and the
// deferral law carries remainders forward — so a slower sweep changes no
// contract. The frame arithmetic lives in
// reports/TERRAIN-REARCHITECTURE-20260909.md §bake.
//
// EVERYTHING ELSE IS V1, VERBATIM: the two phases, chosen B1/B3/B4/B5, the
// radius ruling, the divider, the clamp, the rails, the breach law, the
// counters, the handshake free-slot argument. Where v1's comment said why, the
// why is unchanged and not repeated here — read v1 alongside this file.
//
// Conservative SystemVerilog subset only (charter §2).

module zhao_terrain_bake_v2 (
    input logic clk,
    input logic rst_n,

    // -----------------------------------------------------------------------
    // the §9.2 frame window
    // -----------------------------------------------------------------------
    input  logic       frame_start_i,       // 1 cycle: open a new bake window
    output logic       budget_full_o,       // BAKE_PATCH_BUDGET reached
    output logic [7:0] bakes_this_frame_o,

    // -----------------------------------------------------------------------
    // stamp_results: ONE patch-bake record
    // -----------------------------------------------------------------------
    input  logic               cmd_valid_i,
    output logic               cmd_ready_o,
    input  logic        [15:0] cmd_patch_id_i,
    input  logic signed [31:0] cmd_cx_i,          // stencil centre, fx16 raw
    input  logic signed [31:0] cmd_cz_i,
    input  logic signed [31:0] cmd_radius_i,      // fx16 raw; <= 0 writes nothing
    input  logic signed [31:0] cmd_depth_from_i,  // fx16 raw, absolute depth
    input  logic signed [31:0] cmd_depth_to_i,
    input  logic signed [31:0] cmd_env_x0_i,      // the patch envelope, fx16 raw
    input  logic signed [31:0] cmd_env_z0_i,
    input  logic signed [31:0] cmd_env_x1_i,
    input  logic signed [31:0] cmd_env_z1_i,
    input  logic               cmd_dual_i,        // layer C present
    input  logic               cmd_cells_i,       // layer D present
    input  logic        [15:0] cmd_src_id_i,
    // WHICH LAW DIGS THIS RECORD (owner decision on I32: OPTION A).
    //   0 -- the PARAMETRIC DISC. cmd_cx/cz/radius/depth_from/depth_to with the
    //        radial falloff, oracle `zref::terrain::bake_dig`. Unchanged, bit
    //        for bit; a record that leaves this low behaves exactly as every
    //        record did before 2026-09-20.
    //   1 -- PER-VERTEX FROM LAYER F. The depth at each lattice vertex is read
    //        from the surface sheet through `sheet_texel_o`/`sheet_strength_i`
    //        and section 9.3's two laws, oracle
    //        `zref::terrain::stamp_depth_at_vertex`. cmd_cx/cz/radius/depth_*
    //        are NOT read on this path.
    input  logic               cmd_depth_sheet_i,
    output logic        [15:0] trace_patch_id_o,  // the record under bake

    // -----------------------------------------------------------------------
    // DIG phase: layer B read-modify-write, 33x33 vertices, z-then-x
    // -----------------------------------------------------------------------
    output logic        [ 5:0] vtx_vi_o,
    output logic        [ 5:0] vtx_vj_o,
    input  logic               vtx_valid_i,
    output logic               vtx_ready_o,
    input  logic signed [15:0] vtx_base_i,    // layer A
    input  logic signed [15:0] vtx_scar_i,    // layer B in
    input  logic signed [15:0] vtx_bottom_i,  // layer C
    input  logic               vtx_nobake_i,  // §3.3 corner shadow

    // -----------------------------------------------------------------------
    // THE LAYER-F READ, for `cmd_depth_sheet_i` records only (OWNER RULING R194)
    // -----------------------------------------------------------------------
    // `sheet_texel_o` is combinational on the DIG cursor and is therefore valid
    // whenever `vtx_vi_o`/`vtx_vj_o` are — it is the SAME beat, addressed by
    // §9.3(b)'s nearest-texel law, in `zhao_surface_sheet`'s own `req_texel_i`
    // encoding (j*64 + i, scan order). `sheet_strength_i` is sampled with
    // `vtx_valid_i`, exactly like base/scar/bottom/nobake: the page server
    // delivers all five together or the vertex is not ready.
    //
    // It is presented on EVERY record, disc or sheet, because gating a
    // combinational address behind a mode bit buys nothing and costs a reader
    // the ability to check the address against the cursor at a glance. Nothing
    // reads the strength on a disc record.
    //
    // WHO SERVES IT is NOT this block and is not settled: `zhao_surface_sheet`
    // is composed in `zhao_console_core` and its request port is annotated
    // "SURFACE.STAMP is the only requester". Whatever shares that port between
    // the stamp and this reader is a SCHEDULER — one small block with a
    // contract, of the shape `zhao_terrain_psmux` and `zhao_mem_share_n`
    // already have twice — and a composer may not write an arbiter inline.
    // Entry I32 in `zhao_console_core.sv` carries this.
    output logic        [11:0] sheet_texel_o,
    input  logic        [ 7:0] sheet_strength_i,

    output logic               sc_valid_o,
    input  logic               sc_ready_i,
    output logic signed [15:0] sc_scar_o,     // layer B out
    output logic        [ 5:0] sc_vi_o,
    output logic        [ 5:0] sc_vj_o,
    output logic               sc_touched_o,  // inside the stencil
    output logic               sc_meets_o,    // base + scar <= bottom (§3.4)
    output logic               sc_clamped_o,  // the no_bake clamp fired
    output logic        [15:0] sc_src_id_o,

    // -----------------------------------------------------------------------
    // BREACH phase: layer D read-modify-write, 32x32 cells, z-then-x
    // -----------------------------------------------------------------------
    output logic [5:0] cell_ci_o,
    output logic [5:0] cell_cj_o,
    input  logic       cell_valid_i,
    output logic       cell_ready_o,
    input  logic [7:0] cell_state_i,

    output logic       cs_valid_o,
    input  logic       cs_ready_i,
    output logic [7:0] cs_state_o,
    output logic [5:0] cs_ci_o,
    output logic [5:0] cs_cj_o,
    output logic       cs_event_o,  // a §3.4 transition: the trace event
    output logic [1:0] cs_sub_o,    // the new substance, valid with cs_event_o
    output logic [15:0] cs_src_id_o,

    // -----------------------------------------------------------------------
    // status, counters
    // -----------------------------------------------------------------------
    output logic        dig_done_o,       // 1-cycle pulse: layer B complete
    output logic        bake_done_o,      // 1-cycle pulse: the record retired
    output logic        breach_active_o,  // the block wants cells, not vertices
    output logic [31:0] surface_texels_touched_o,
    output logic [31:0] breach_events_o,
    output logic [31:0] scar_saturations_o,
    output logic [31:0] nobake_clamps_o,
    // OWNER RULING 2026-08-24: MAX_BAKE_RADIUS = 512 m. Rejected, never
    // clamped, counted (v1's comment says why; the ruling did not move).
    output logic [31:0] bake_radius_rejects_o,
    // How many vertices the PER-VERTEX LAYER-F mode has actually dug — a
    // vertex whose sheet strength is non-zero, on a `cmd_depth_sheet_i` record.
    // It is the counter that separates "the sheet mode ran" from "the sheet
    // mode was wired and read zeros", which are the two readings a silent
    // machine allows and a counter reading zero cannot distinguish (CLAUDE.md:
    // a detector reading zero is a claim, and it is the claim to check
    // hardest). Fired by stimulus in `terrain_bake_v2_sheet_directed`.
    output logic [31:0] sheet_vertices_dug_o,
    output logic        idle_o
);

  // ---- frozen constants (terrain_rules §2, §9.2 — law, not preference) ----
  localparam int unsigned Lat = 33;  // Island Patch v1 lattice
  localparam logic [5:0] LatMax = 6'd32;  // last lattice index
  localparam logic [5:0] CellMax = 6'd31;  // last cell index
  localparam logic [7:0] BakePatchBudget = 8'd64;  // terrain_rules §9.2, frozen

  // ---- named, editable shape parameters (CLAUDE.md art law rule 6) --------
  // The shared multiplier's width: must hold signed 33x33 (dx*dx). 34 covers
  // every site; widening it is legal, narrowing it below 34 breaks dx^2.
  localparam int unsigned MulW = 34;
  localparam int unsigned MulPW = 2 * MulW;  // 68
  // The meets RAM: Lat rows plus one PAD row so the breach prefetch address
  // (cj + 2, clamped) always has a legal word to read. The pad row is never
  // written and its read value is never consumed.
  localparam int unsigned MeetsDepth = Lat + 1;  // 34
  localparam logic [5:0] MeetsPadRow = 6'd33;

  // ---- cell state byte (terrain_rules §3.3) -------------------------------
  localparam logic [1:0] SubSolid = 2'd0;
  localparam logic [1:0] SubVoidAuthored = 2'd1;
  localparam logic [1:0] SubVoidBreached = 2'd2;

  // ---- states --------------------------------------------------------------
  // The sequencer: per record StRad once; per row StVzM/StVzC/StDzM/StDzC;
  // per vertex StVxM/StVxC/StDxM then the v1 spine StVtx/StDiv/StEmit with
  // StPfM/StPtM between divide and emit on the covered path; per breach
  // entry StBrA/StBrB/StBrC to fill the row window, then StCell.
  localparam logic [4:0] StIdle = 5'd0;
  localparam logic [4:0] StRad  = 5'd1;   // mul: radius^2
  localparam logic [4:0] StVzM  = 5'd2;   // mul: span_z * vj  (also lands r2)
  localparam logic [4:0] StVzC  = 5'd3;   // vz_q <= lerp finish
  localparam logic [4:0] StDzM  = 5'd4;   // mul: dz * dz
  localparam logic [4:0] StDzC  = 5'd5;   // dz2_q <= product
  localparam logic [4:0] StVxM  = 5'd6;   // mul: span_x * vi
  localparam logic [4:0] StVxC  = 5'd7;   // vx_q <= lerp finish
  localparam logic [4:0] StDxM  = 5'd8;   // mul: dx * dx
  localparam logic [4:0] StVtx  = 5'd9;   // waiting for a vertex (covers comb)
  localparam logic [4:0] StDiv  = 5'd10;  // the 17-step stencil divide
  localparam logic [4:0] StPfM  = 5'd11;  // mul: depth_from * s
  localparam logic [4:0] StPtM  = 5'd12;  // mul: depth_to * s; pf_q lands
  localparam logic [4:0] StEmit = 5'd13;  // publish the scar word
  localparam logic [4:0] StBrA  = 5'd14;  // meets row window: address row 0
  localparam logic [4:0] StBrB  = 5'd15;  // mrow_lo <= row 0; address row 1
  localparam logic [4:0] StBrC  = 5'd16;  // mrow_hi <= row 1; prefetch row 2
  localparam logic [4:0] StCell = 5'd17;  // waiting for a cell
  localparam logic [4:0] StR2C  = 5'd18;  // c_r2 <= radius^2 (ONCE per record --
                                          // StVzM recurs per row and must never
                                          // touch r2: the first build captured
                                          // r2 there and clobbered it with the
                                          // previous row's last product)
  logic [4:0] state;

  // -------------------------------------------------------------------------
  // the held record
  // -------------------------------------------------------------------------
  logic signed [31:0] c_cx, c_cz, c_from, c_to;
  logic signed [31:0] c_x0, c_z0, c_x1, c_z1;
  logic signed [31:0] c_rad;  // held so radius^2 can be sequenced (StRad)
  logic               c_dual, c_cells;
  logic               c_sheet;  // this record digs from layer F, not from the disc
  logic        [15:0] c_src;
  logic        [62:0] c_r2;  // radius^2, unsigned; 0 when radius <= 0 (B5)
  logic signed [32:0] c_spx, c_spz;  // envelope spans, captured at StRad

  // 512.0 m in fx16 raw (owner ruling 2026-08-24; v1's derivation applies).
  localparam logic signed [31:0] MAX_BAKE_RADIUS_RAW = 32'sh0200_0000;
  wire radius_illegal = (cmd_radius_i > MAX_BAKE_RADIUS_RAW);

  logic [5:0] vi, vj;  // lattice indices, 0..32
  logic [5:0] ci, cj;  // cell indices, 0..31

  // ---- the vertex under evaluation ----------------------------------------
  logic signed [15:0] h_base, h_scar, h_bottom;
  logic               h_nobake;
  logic        [ 7:0] h_strength;  // layer F at this vertex (sheet mode only)
  logic               v_covered;  // d2 < r2, or (strength != 0) in sheet mode

  // ---- the stencil divider (v1, verbatim) ---------------------------------
  logic [79:0] div_rem;
  logic [79:0] div_dsh;
  logic [16:0] div_quo;
  logic [ 4:0] div_cnt;

  // -------------------------------------------------------------------------
  // THE ONE MULTIPLIER (M1) — the factory every former site now queues for
  // -------------------------------------------------------------------------
  logic signed [MulW-1:0] mul_a, mul_b;
  // The top two product bits are pure sign for every site (the widest real
  // product, dx*dx, is 66 bits) — kept so the register IS the full multiplier
  // output and no site needs a width argument at the landing pad.
  /* verilator lint_off UNUSEDSIGNAL */
  logic signed [MulPW-1:0] mul_p;
  /* verilator lint_on UNUSEDSIGNAL */
  logic mul_go;  // product-register enable: only mul states overwrite mul_p

  always_comb begin
    // Operand mux, one arm per issuing state. Defaults keep the mux total
    // rather than latch-shaped; `mul_go` is what gates the landing.
    mul_a  = '0;
    mul_b  = '0;
    mul_go = 1'b0;
    unique case (state)
      StRad: begin
        mul_a  = {{2{c_rad[31]}}, c_rad};
        mul_b  = {{2{c_rad[31]}}, c_rad};
        mul_go = 1'b1;
      end
      StVzM: begin
        mul_a  = {c_spz[32], c_spz};
        mul_b  = {28'b0, vj};  // 0..32, positive
        mul_go = 1'b1;
      end
      StDzM: begin
        mul_a  = {dz_c[32], dz_c};
        mul_b  = {dz_c[32], dz_c};
        mul_go = 1'b1;
      end
      StVxM: begin
        mul_a  = {c_spx[32], c_spx};
        mul_b  = {28'b0, vi};
        mul_go = 1'b1;
      end
      StDxM: begin
        mul_a  = {dx_c[32], dx_c};
        mul_b  = {dx_c[32], dx_c};
        mul_go = 1'b1;
      end
      StPfM: begin
        mul_a  = {{2{c_from[31]}}, c_from};
        mul_b  = {17'b0, div_quo};  // s, Q16, 0..65536: positive
        mul_go = 1'b1;
      end
      StPtM: begin
        mul_a  = {{2{c_to[31]}}, c_to};
        mul_b  = {17'b0, div_quo};
        mul_go = 1'b1;
      end
      default: ;
    endcase
  end

  // -------------------------------------------------------------------------
  // lattice_lerp, SPLIT ACROSS THE PRODUCT REGISTER — the arithmetic is v1's
  // function body operator for operator; only the multiply moved into `mul_p`.
  // prod = span * num fits signed 40 (33b span x 7b num), so `mul_p[39:0]` IS
  // v1's `prod` — the wider product's upper bits are pure sign.
  // -------------------------------------------------------------------------
  function automatic logic signed [31:0] lerp_finish(input logic signed [31:0] a,
                                                     input logic signed [39:0] prod);
    logic signed [39:0] nadj;
    logic signed [39:0] quot;
    /* verilator lint_off UNUSEDSIGNAL */
    logic signed [39:0] sum;
    /* verilator lint_on UNUSEDSIGNAL */
    begin
      nadj = prod + 40'sd16;
      quot = nadj >>> 5;
      if (nadj[39] && (nadj[4:0] != 5'd0)) quot = quot + 40'sd1;  // toward zero
      sum = {{8{a[31]}}, a} + quot;
      lerp_finish = sum[31:0];  // static_cast<int32_t>: wraps, as the ref does
    end
  endfunction

  // the placed lattice point, registered per row (vz_q) and per vertex (vx_q)
  logic signed [31:0] vz_q, vx_q;
  logic signed [65:0] dz2_q;  // dz*dz, held for the whole row

  // re-domained differences (v1's dx/dz, now fed from the registered points)
  logic signed [32:0] dx_c, dz_c;
  assign dx_c = $signed({vx_q[31], vx_q}) - $signed({c_cx[31], c_cx});
  assign dz_c = $signed({vz_q[31], vz_q}) - $signed({c_cz[31], c_cz});

  // -------------------------------------------------------------------------
  // the radial test (v1, with dx^2 read from the product register)
  // -------------------------------------------------------------------------
  logic [66:0] d2;
  assign d2 = {1'b0, mul_p[65:0]} + {1'b0, dz2_q};  // mul_p holds dx^2 in StVtx

  logic [79:0] d2_ext, r2_ext;
  assign d2_ext = {13'b0, d2};
  assign r2_ext = {17'b0, c_r2};

  logic covers;
  assign covers = d2_ext < r2_ext;

  logic [79:0] sn_init;
  assign sn_init = ((r2_ext - d2_ext) << 16) + (r2_ext >> 1);

  logic [79:0] dsh_init;
  assign dsh_init = r2_ext << 16;

  // -------------------------------------------------------------------------
  // the incremental delta — bake_delta's post-multiply arithmetic, operator
  // for operator (zhao_terrain_bake_delta.sv is the annotated original; its
  // two 32x18 products are the ONLY thing that moved, into StPfM/StPtM).
  // `pf_q` holds p_from; `mul_p[49:0]` holds p_to during StEmit.
  // -------------------------------------------------------------------------
  logic signed [49:0] pf_q;

  function automatic logic signed [31:0] delta_g(input logic signed [49:0] p);
    // rescale(.,16) with the fx16 saturate, then rescale(.,8); returns g
    // sign-extended to the fx16 word so the subtraction below is exact
    // (|g| <= 2^23, bake_delta's own bound). The discarded low bits of `q`
    // and `t` ARE the two roundings — the shifts are the operators.
    /* verilator lint_off UNUSEDSIGNAL */
    logic signed [49:0] q;
    logic signed [32:0] t;
    /* verilator lint_on UNUSEDSIGNAL */
    logic signed [33:0] a_raw;
    logic signed [31:0] a;
    begin
      q = p + 50'sd32768;
      a_raw = q[49:16];
      a = (a_raw > 34'sd2147483647)  ? 32'sh7FFF_FFFF :
          (a_raw < -34'sd2147483648) ? 32'sh8000_0000 : a_raw[31:0];
      t = $signed({a[31], a}) + 33'sd128;
      delta_g = {{7{t[32]}}, t[32:8]};
    end
  endfunction

  function automatic logic delta_g_sat(input logic signed [49:0] p);
    /* verilator lint_off UNUSEDSIGNAL */
    logic signed [49:0] q;
    /* verilator lint_on UNUSEDSIGNAL */
    logic signed [33:0] a_raw;
    begin
      q = p + 50'sd32768;
      a_raw = q[49:16];
      delta_g_sat = (a_raw > 34'sd2147483647) || (a_raw < -34'sd2147483648);
    end
  endfunction

  logic signed [31:0] delta16;
  logic               delta_sat;
  logic signed [31:0] g_from_c, g_to_c;
  assign g_from_c = delta_g(pf_q);
  assign g_to_c   = delta_g(mul_p[49:0]);
  // An uncovered vertex has stencil 0: both products are 0, both g are 0 and
  // the delta is 0 with no saturation — proved by substitution in
  // bake_delta's own arithmetic ((0+32768)>>>16 = 0; (0+128)>>>8 = 0). The
  // covered path never skips StPfM/StPtM, so the mux below is exactly v1's
  // `stencil = v_covered ? div_quo : 0` one stage later.
  // -------------------------------------------------------------------------
  // THE SECOND DEPTH LAW — per-vertex, out of layer F (OWNER RULING R194)
  // -------------------------------------------------------------------------
  // `zhao_terrain_stampdepth` is spec/terrain_rules.md §9.3's two laws: the
  // nearest-texel address generator §9.3(b) and the sixteen-entry ART TABLE
  // §9.3(a). Its oracle is `zref::terrain::stamp_depth_at_vertex` and
  // `tests/terrain/terrain_stampdepth_directed.cpp` differences it exhaustively
  // — every one of the 33x33 vertices and all 256 strengths.
  //
  // ONE INSTANCE SERVES BOTH HALVES, and that is a property of the block rather
  // than a coincidence: its `texel_o` depends on `vi_i`/`vj_i` ALONE and its
  // depth outputs depend on `strength_i` ALONE — two independent combinational
  // paths through one file. So the address comes out against the LIVE DIG
  // cursor (valid with `vtx_vi_o`/`vtx_vj_o`, which is what the page server
  // needs) while the depth comes out against the REGISTERED strength captured
  // at StVtx (which is what StEmit needs, one state later). The independence is
  // ASSERTED, not assumed: `terrain_stampdepth_directed` case 1 sweeps the
  // cursor with the strength held and case 2 sweeps the strength with the
  // cursor held, and `terrain_bake_v2_sheet_directed` checks the address
  // against the cursor on every beat of a whole bake.
  logic        [11:0] sd_texel;
  logic signed [31:0] sd_depth_h16;
  /* verilator lint_off UNUSEDSIGNAL */
  // `depth_fx16_o` is the law's own intermediate and is brought out of the
  // reader for its directed test, not for this block: bake's scar arithmetic
  // is height16 and consuming the fx16 word here would mean rounding it twice.
  logic signed [31:0] sd_depth_fx16;
  logic               sd_covered;
  /* verilator lint_on UNUSEDSIGNAL */

  zhao_terrain_stampdepth u_stampdepth (
      .vi_i        (vi),
      .vj_i        (vj),
      .texel_o     (sd_texel),
      .strength_i  (h_strength),
      .depth_fx16_o(sd_depth_fx16),
      .depth_h16_o (sd_depth_h16),
      .covered_o   (sd_covered)
  );

  assign sheet_texel_o = sd_texel;

  // THE MUX IS THE WHOLE OF OPTION A. An uncovered vertex contributes zero on
  // BOTH paths, so the three-way select collapses to "which law computed the
  // delta", and the disc arm is v1's expression untouched — a record that
  // leaves `cmd_depth_sheet_i` low is bit-identical to every record this block
  // has ever baked. That is what makes the mode ADDITIVE and independently
  // testable, and it is why `terrain_bake_v2_directed`'s 267 checks still hold
  // the disc law with nothing changed.
  assign delta16 = !v_covered ? 32'sd0 :
                   c_sheet    ? sd_depth_h16 : (g_from_c - g_to_c);

  // The sheet path CANNOT saturate here and the disc path can. `delta_g`'s
  // fx16 saturate exists because `depth * stencil` is a 50-bit product; the
  // sheet's depth is a 32-bit table entry whose height16 form is 25 bits
  // sign-extended into 32, which is exact. A runaway table entry is caught by
  // `rail_hi`/`rail_lo` below, once, where `scar_saturations_o` counts it —
  // NOT counted twice as a delta saturation it never had.
  assign delta_sat = (v_covered && !c_sheet)
                   ? (delta_g_sat(pf_q) || delta_g_sat(mul_p[49:0])) : 1'b0;

  // -------------------------------------------------------------------------
  // scar = scar + delta, the no_bake clamp, the height16 rails (v1, verbatim)
  // -------------------------------------------------------------------------
  logic signed [33:0] scar_sum;
  assign scar_sum = {{18{h_scar[15]}}, h_scar} + {{2{delta16[31]}}, delta16};

  logic signed [33:0] min_scar;
  assign min_scar = {{18{h_bottom[15]}}, h_bottom} + 34'sd1 - {{18{h_base[15]}}, h_base};

  logic guard_on;
  assign guard_on = c_dual && c_cells && h_nobake;

  logic clamp_fires;
  assign clamp_fires = v_covered && guard_on && (scar_sum < min_scar);

  logic signed [33:0] scar_guarded;
  assign scar_guarded = clamp_fires ? min_scar : scar_sum;

  logic rail_hi, rail_lo;
  assign rail_hi = v_covered && (scar_guarded > 34'sd32767);
  assign rail_lo = v_covered && (scar_guarded < -34'sd32768);

  logic signed [15:0] scar_new;
  assign scar_new = !v_covered ? h_scar :
                    rail_hi    ? 16'sh7FFF :
                    rail_lo    ? 16'sh8000 : scar_guarded[15:0];

  // the §3.4 equality — STILL COMPUTED HERE (see M2 in the header); signedness
  // rails exactly as v1 (the GEOM.BINNER unsigned-compare trap).
  logic signed [17:0] composed18, bottom18;
  assign composed18 = $signed({{2{h_base[15]}}, h_base}) + $signed({{2{scar_new[15]}}, scar_new});
  assign bottom18   = $signed({{2{h_bottom[15]}}, h_bottom});

  logic meets_new;
  assign meets_new = composed18 <= bottom18;

  // -------------------------------------------------------------------------
  // THE MEETS PLANE AS A RAM (M2): write side
  // -------------------------------------------------------------------------
  logic [32:0] meets_ram[MeetsDepth];
  logic [31:0] wrow;  // the row under accumulation (bits 0..31; bit 32 joins at write)
  logic [32:0] mq;    // registered RAM read data
  logic [5:0]  m_raddr;

  // The completed row word: every bit vi < 32 was written into `wrow` at its
  // own emit; the last bit rides the write itself.
  logic [32:0] m_wdata;
  assign m_wdata = {meets_new, wrow};

  logic m_we;
  assign m_we = (state == StEmit) && (vi == LatMax);

  always_comb begin
    // Breach read/prefetch address. During the cell scan the prefetch wants
    // row cj+2; clamped to the pad row when past the last real row (the
    // prefetched value is then never consumed — see MeetsPadRow).
    unique case (state)
      StBrA:   m_raddr = 6'd0;
      StBrB:   m_raddr = 6'd1;
      StBrC:   m_raddr = 6'd2;
      StCell:  m_raddr = (cj <= 6'd30) ? (cj + 6'd2) : MeetsPadRow;
      default: m_raddr = 6'd0;
    endcase
  end

  // The RAM proper: synchronous read, one write port, no reset (header M2
  // says why unreset contents are unreachable).
  always_ff @(posedge clk) begin
    mq <= meets_ram[m_raddr];
    if (m_we) meets_ram[vj] <= m_wdata;
  end

  // ---- the breach row window ----------------------------------------------
  logic [32:0] mrow_lo, mrow_hi;

  logic all4;
  assign all4 = mrow_lo[ci] && mrow_lo[ci+6'd1] && mrow_hi[ci] && mrow_hi[ci+6'd1];

  logic [1:0] sub_in;
  assign sub_in = cell_state_i[1:0];

  logic cell_nobake;
  assign cell_nobake = cell_state_i[2];

  logic breach_fires, heal_fires;
  assign breach_fires = (sub_in == SubSolid) && all4 && !cell_nobake;
  assign heal_fires = (sub_in == SubVoidBreached) && !all4;

  logic [1:0] sub_out;
  assign sub_out = breach_fires ? SubVoidBreached : heal_fires ? SubSolid : sub_in;

  logic cell_event;
  assign cell_event = (sub_in != SubVoidAuthored) && (breach_fires || heal_fires);

  // -------------------------------------------------------------------------
  // handshakes (v1's free-slot argument holds unchanged: `sc_free` at the
  // accept cycle means the register is free at every later cycle of this
  // vertex, because nothing re-raises sc_valid_o until its own StEmit)
  // -------------------------------------------------------------------------
  logic sc_free, cs_free;
  assign sc_free = !sc_valid_o || sc_ready_i;
  assign cs_free = !cs_valid_o || cs_ready_i;

  assign vtx_ready_o = (state == StVtx) && sc_free;
  assign cell_ready_o = (state == StCell) && cs_free;
  assign cmd_ready_o = (state == StIdle) && !budget_full_o;

  assign vtx_vi_o = vi;
  assign vtx_vj_o = vj;
  assign cell_ci_o = ci;
  assign cell_cj_o = cj;

  assign breach_active_o = (state == StCell);
  assign idle_o = (state == StIdle) && !sc_valid_o && !cs_valid_o;

  logic [7:0] bakes_this_frame;
  assign bakes_this_frame_o = bakes_this_frame;
  assign budget_full_o = bakes_this_frame >= BakePatchBudget;

  wire last_vtx = (vi == LatMax) && (vj == LatMax);
  wire last_cell = (ci == CellMax) && (cj == CellMax);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state <= StIdle;
      c_cx <= '0;
      c_cz <= '0;
      c_from <= '0;
      c_to <= '0;
      c_x0 <= '0;
      c_z0 <= '0;
      c_x1 <= '0;
      c_z1 <= '0;
      c_rad <= '0;
      c_dual <= 1'b0;
      c_cells <= 1'b0;
      c_sheet <= 1'b0;
      c_src <= '0;
      c_r2 <= '0;
      c_spx <= '0;
      c_spz <= '0;
      vi <= '0;
      vj <= '0;
      ci <= '0;
      cj <= '0;
      h_base <= '0;
      h_scar <= '0;
      h_bottom <= '0;
      h_nobake <= 1'b0;
      h_strength <= '0;
      v_covered <= 1'b0;
      div_rem <= '0;
      div_dsh <= '0;
      div_quo <= '0;
      div_cnt <= '0;
      mul_p <= '0;
      vz_q <= '0;
      vx_q <= '0;
      dz2_q <= '0;
      pf_q <= '0;
      wrow <= '0;
      mrow_lo <= '0;
      mrow_hi <= '0;
      sc_valid_o <= 1'b0;
      sc_scar_o <= '0;
      sc_vi_o <= '0;
      sc_vj_o <= '0;
      sc_touched_o <= 1'b0;
      sc_meets_o <= 1'b0;
      sc_clamped_o <= 1'b0;
      sc_src_id_o <= '0;
      cs_valid_o <= 1'b0;
      cs_state_o <= '0;
      cs_ci_o <= '0;
      cs_cj_o <= '0;
      cs_event_o <= 1'b0;
      cs_sub_o <= '0;
      cs_src_id_o <= '0;
      trace_patch_id_o <= '0;
      dig_done_o <= 1'b0;
      bake_done_o <= 1'b0;
      bakes_this_frame <= '0;
      surface_texels_touched_o <= '0;
      breach_events_o <= '0;
      scar_saturations_o <= '0;
      nobake_clamps_o <= '0;
      bake_radius_rejects_o <= '0;
      sheet_vertices_dug_o <= '0;
    end else begin
      dig_done_o  <= 1'b0;
      bake_done_o <= 1'b0;

      if (frame_start_i) bakes_this_frame <= '0;

      if (sc_valid_o && sc_ready_i) sc_valid_o <= 1'b0;
      if (cs_valid_o && cs_ready_i) cs_valid_o <= 1'b0;

      // the shared product register — every site's landing pad
      if (mul_go) mul_p <= mul_a * mul_b;

      case (state)
        StIdle: begin
          if (cmd_valid_i && cmd_ready_o && radius_illegal) begin
            bake_radius_rejects_o <= bake_radius_rejects_o + 32'd1;
          end else if (cmd_valid_i && cmd_ready_o) begin
            c_cx <= cmd_cx_i;
            c_cz <= cmd_cz_i;
            c_from <= cmd_depth_from_i;
            c_to <= cmd_depth_to_i;
            c_x0 <= cmd_env_x0_i;
            c_z0 <= cmd_env_z0_i;
            c_x1 <= cmd_env_x1_i;
            c_z1 <= cmd_env_z1_i;
            c_rad <= cmd_radius_i;
            c_dual <= cmd_dual_i;
            c_cells <= cmd_cells_i;
            c_sheet <= cmd_depth_sheet_i;
            c_src <= cmd_src_id_i;
            trace_patch_id_o <= cmd_patch_id_i;
            vi <= '0;
            vj <= '0;
            ci <= '0;
            cj <= '0;
            bakes_this_frame <= frame_start_i ? 8'd1 : (bakes_this_frame + 8'd1);
            state <= StRad;
          end
        end

        StRad: begin
          // mul: radius^2 lands at this edge. The spans are captured here so
          // every later lerp reads a register, not a subtraction.
          c_spx <= $signed({c_x1[31], c_x1}) - $signed({c_x0[31], c_x0});
          c_spz <= $signed({c_z1[31], c_z1}) - $signed({c_z0[31], c_z0});
          state <= StR2C;
        end

        StR2C: begin
          // radius^2 is read out ONCE per record (B5: r2 = 0 for radius <= 0
          // makes `covers` false everywhere)
          c_r2  <= (c_rad > 0) ? mul_p[62:0] : 63'd0;
          state <= StVzM;
        end

        StVzM: begin
          // mul: span_z * vj lands at this edge
          state <= StVzC;
        end

        StVzC: begin
          vz_q  <= lerp_finish(c_z0, mul_p[39:0]);
          state <= StDzM;
        end

        StDzM: begin
          // mul: dz * dz lands at this edge
          state <= StDzC;
        end

        StDzC: begin
          dz2_q <= mul_p[65:0];
          state <= StVxM;
        end

        StVxM: begin
          // mul: span_x * vi lands at this edge
          state <= StVxC;
        end

        StVxC: begin
          vx_q  <= lerp_finish(c_x0, mul_p[39:0]);
          state <= StDxM;
        end

        StDxM: begin
          // mul: dx * dx lands at this edge
          state <= StVtx;
        end

        StVtx: begin
          if (vtx_valid_i && vtx_ready_o) begin
            h_base <= vtx_base_i;
            h_scar <= vtx_scar_i;
            h_bottom <= vtx_bottom_i;
            h_nobake <= vtx_nobake_i;
            // Layer F rides the SAME beat as layers A/B/C: the page server
            // delivers all five or the vertex is not ready. Captured on every
            // record; read only on a sheet one.
            h_strength <= sheet_strength_i;
            // Coverage, by whichever law digs this record. The sheet arm is
            // `zhao_terrain_stampdepth`'s `covered_o` law applied to this
            // beat's own strength — the reader instance sees the REGISTERED
            // copy one state later and gives the same answer;
            // `terrain_bake_v2_sheet_directed` asserts the two agree rather
            // than leaving it as an argument.
            v_covered <= c_sheet ? (sheet_strength_i != 8'd0) : covers;
            // THE SHEET PATH SKIPS THE STENCIL ENTIRELY. There is no divide and
            // there are no depth products on it: the depth is a table lookup,
            // already in height16, valid combinationally off `h_strength` at
            // the next edge. So it goes straight to StEmit and a sheet vertex
            // costs 20 fewer states than a covered disc vertex. The disc arm
            // below is untouched.
            if (c_sheet) begin
              div_quo <= '0;
              state   <= StEmit;
            end else if (covers) begin
              div_rem <= sn_init;
              div_dsh <= dsh_init;
              div_quo <= '0;
              div_cnt <= 5'd16;
              state   <= StDiv;
            end else begin
              div_quo <= '0;
              state   <= StEmit;
            end
          end
        end

        StDiv: begin
          if (div_rem >= div_dsh) begin
            div_rem <= div_rem - div_dsh;
            div_quo <= {div_quo[15:0], 1'b1};
          end else begin
            div_quo <= {div_quo[15:0], 1'b0};
          end
          div_dsh <= div_dsh >> 1;
          if (div_cnt == 5'd0) state <= StPfM;
          else div_cnt <= div_cnt - 5'd1;
        end

        StPfM: begin
          // mul: depth_from * s lands at this edge
          state <= StPtM;
        end

        StPtM: begin
          // mul: depth_to * s lands at this edge; p_from is read out first
          pf_q  <= mul_p[49:0];
          state <= StEmit;
        end

        StEmit: begin
          sc_valid_o <= 1'b1;
          sc_scar_o <= scar_new;
          sc_vi_o <= vi;
          sc_vj_o <= vj;
          sc_touched_o <= v_covered;
          sc_meets_o <= meets_new;
          sc_clamped_o <= clamp_fires;
          sc_src_id_o <= c_src;
          // bit 32 rides the RAM write itself (m_wdata) at this same edge
          if (vi != LatMax) wrow[vi[4:0]] <= meets_new;
          if (v_covered) surface_texels_touched_o <= surface_texels_touched_o + 32'd1;
          // THE SHEET MODE'S OWN COUNTER, and it counts a DIFFERENT thing from
          // the one above: `surface_texels_touched_o` counts every covered
          // vertex on either law, so it cannot separate "the layer-F mode ran"
          // from "a disc record covered the same vertices". This one moves only
          // on a `cmd_depth_sheet_i` record and only where the sheet actually
          // carried strength — which is precisely the question a wired-up but
          // dead reader would answer wrongly with every other counter balanced.
          if (c_sheet && v_covered) sheet_vertices_dug_o <= sheet_vertices_dug_o + 32'd1;
          if (clamp_fires) nobake_clamps_o <= nobake_clamps_o + 32'd1;
          if (rail_hi || rail_lo || (v_covered && delta_sat))
            scar_saturations_o <= scar_saturations_o + 32'd1;
          if (last_vtx) begin
            dig_done_o <= 1'b1;
            if (c_dual && c_cells) begin
              state <= StBrA;
            end else begin
              bake_done_o <= 1'b1;
              state <= StIdle;
            end
          end else begin
            if (vi == LatMax) begin
              vi <= '0;
              vj <= vj + 6'd1;
              state <= StVzM;  // new row: vz and dz^2 recompute
            end else begin
              vi <= vi + 6'd1;
              state <= StVxM;
            end
          end
        end

        StBrA: begin
          // m_raddr = 0; row 0 lands in mq at this edge
          state <= StBrB;
        end

        StBrB: begin
          mrow_lo <= mq;  // row 0
          // m_raddr = 1; row 1 lands in mq at this edge
          state <= StBrC;
        end

        StBrC: begin
          mrow_hi <= mq;  // row 1
          // m_raddr = 2; the prefetch lands in mq at this edge
          state <= StCell;
        end

        StCell: begin
          if (cell_valid_i && cell_ready_o) begin
            cs_valid_o <= 1'b1;
            cs_state_o <= {cell_state_i[7:2], sub_out};
            cs_ci_o <= ci;
            cs_cj_o <= cj;
            cs_event_o <= cell_event;
            cs_sub_o <= sub_out;
            cs_src_id_o <= c_src;
            if (cell_event) breach_events_o <= breach_events_o + 32'd1;
            if (last_cell) begin
              bake_done_o <= 1'b1;
              state <= StIdle;
            end else begin
              if (ci == CellMax) begin
                ci <= '0;
                cj <= cj + 6'd1;
                // the row window slides: cj+1's row was prefetched into mq
                // during this row's >= 32-cycle scan (1 cycle was enough)
                mrow_lo <= mrow_hi;
                mrow_hi <= mq;
              end else begin
                ci <= ci + 6'd1;
              end
            end
          end
        end

        default: state <= StIdle;
      endcase
    end
  end

endmodule : zhao_terrain_bake_v2
