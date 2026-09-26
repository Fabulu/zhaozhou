// zhao_terrain_spdesc.sv -- THE SUBPATCH DESCRIPTOR ASSEMBLER.  TERRAIN.LOD's
// `sp_*` input, assembled from the three places its sixteen fields actually
// live.  Entry I21's remainder, named by gz/jobissue on 2026-09-21 as the one
// block between a built TERRAIN group and a composed one.
//
// ===========================================================================
// WHAT IT IS FOR
// ===========================================================================
// `zhao_terrain_lod` takes sixteen PER-SUBPATCH descriptors and emits sixteen
// level decisions.  Each descriptor carries a world CENTRE (`sp_cx/cy/cz`),
// three stored coarse DEVIATIONS (`sp_dev1/2/3`), the previous frame's HISTORY
// (`sp_prev_level`, `sp_prev_morph`, `sp_hold`) and the patch's `src_id`.
//
// No single block in this tree holds those.  They live in three:
//
//   * the DEVIATIONS and the HISTORY are `zhao_terrain_devstore`'s, keyed by
//     PAGE SLOT, written at page load by `zhao_terrain_lodfeed` under ruling
//     R24;
//   * the CENTRE's x and z are TERRAIN.PLACE's placed column/row stream, which
//     `zhao_terrain_compcache_front` holds per SERVE PARITY and answers on
//     `lat_wx_o`/`lat_wz_o`;
//   * the CENTRE's y is `zhao_terrain_devstore`'s `r_cy_o`, carried across from
//     the page-load lattice because that lattice is gone by serve time.
//
// This block is the join.  It is a separate file and not wires in a composer
// for the reason `zhao_terrain_lodfeed` gives about itself: the door queue
// below is STATE, and the slot/src_id pairing is a DECISION.
//
// ===========================================================================
// THE THREE THINGS ENTRY I21 SAID WERE MISSING, AND WHAT EACH TURNED OUT TO BE
// ===========================================================================
// gz/jobissue measured three absences in its own tree and refused to adapt
// around any of them.  All three are answered here, and two are SMALLER than
// they were stated -- which is the direction ruling R237 says to check hardest,
// so each one names the evidence rather than the conclusion.
//
//   1. "`sp_cx_i`/`sp_cz_i` HAVE NO PRODUCER."  They have one, and
//      `zhao_terrain_lodfeed.sv`'s own `w_cy_o` port comment already named it:
//      "x and z come free from TERRAIN.PLACE's placed column/row stream at
//      serve time".  That stream is written into `zhao_terrain_compcache_front`
//      on `pos_we_i`/`pos_axis_i`/`pos_idx_i`/`pos_val_i` and served back on
//      `lat_wx_o`/`lat_wz_o`.  So this block READS THE NAMED SOURCE.  It does
//      NOT recompute the placement from an origin and a pitch, which is the
//      available shortcut and is a second implementation of TERRAIN.PLACE's
//      law -- the way two blocks come to disagree about where a patch is.
//
//      IT READS THE SERVE PORT AND NOT THE WRITE STREAM, deliberately.
//      Observing `pos_we_i` would need this block to hold its own copy of the
//      33 + 33 positions AND its own fill/serve PARITY, and the parity is a
//      decision `zhao_terrain_compcache_front` has already made
//      (`wx_m[(serve_par_q ? LAT_W : 0) + rd_vi_c]`).  Reading through the
//      serve port gets that swap for free and cannot disagree with it.
//
//   2. "`zhao_terrain_devstore` HAS NO `src_id` COLUMN."  True, and it does not
//      need one.  The store is READ WITH A SLOT THIS BLOCK ALREADY HOLDS, and
//      the `src_id` that goes out on `sp_src_id_o` is the one popped from the
//      door beside that slot.  Adding a column to the store would carry the id
//      the long way round -- in at page load, out at serve -- to arrive at a
//      block that was handed it at the door two hundred clocks earlier.
//      NOTHING IS NARROWED BY THIS: `zhao_terrain_lodfeed`'s `w_src_id_o` keeps
//      its existing reader (MEASURE.HISTOGRAM's event ingress, ruling R70) and
//      is not asked to acquire a second one.
//
//   3. "THE STORE IS KEYED BY PAGE SLOT AND THE CACHE SERVES BY SRC_ID, WITH NO
//      MAP."  This is the real one, and it is the reason this block exists.
//      The answer is NOT a map.  A map from `src_id` to slot would have to hold
//      one entry per resident page -- 1,024 of them, compared associatively
//      against a 16-bit id -- because a page stays resident across frames and
//      is re-composed every frame without being re-loaded.  That is a CAM, and
//      a 1,024 x 16 b CAM to recover a number that was in hand a moment ago is
//      the wrong shape.
//
//      THE SLOT IS KNOWN AT THE COMPOSE DOOR.  `zhao_terrain_pagestream` emits
//      `v_slot_o` and `v_src_id_o` ON THE SAME VERTEX BEAT, and the compose
//      fill starts on one of those beats.  So the pair is captured at the door,
//      queued, and popped for the patch the cache actually serves -- WHICH IS
//      EXACTLY `zhao_terrain_jobissue`'s draw-context queue, one block old and
//      proved by its own directed test.  Two blocks arming off one event with
//      one law between them is better than two laws.
//
//      AND THE PAIRING IS CHECKED RATHER THAN TRUSTED.  `door_src_mismatch_o`
//      differences the popped `src_id` against `serve_src_id_i`.  CLAUDE.md's
//      metadata-swap chapter is the reason that sentence is not enough on its
//      own, so: THE TWO OPERANDS ARE NOT CLOCKED BY ONE ENABLE.  The popped id
//      is written by `dq_push_c` (the compose door's acceptance) and read by
//      `dq_pop_c`; `serve_src_id_i` is combinational off the cache's own serve
//      parity, which moves on the SWAP.  A door that pushed the wrong patch, a
//      swap that did not happen and a swap that happened one patch early each
//      move exactly one of the two.  The directed test fires it on purpose
//      before its silence is quoted anywhere (ruling R95).
//
// ===========================================================================
// IT NEVER DELAYS THE LATTICE PORT'S EXISTING CLIENT
// ===========================================================================
// `zhao_terrain_compcache_front`'s `lat_req_i` HAS NO READY.  A request is made
// and the datum is there the cycle after; there is no back channel, so a
// pass-through that held a request up would not delay it, it would DESTROY it.
//
// `zhao_terrain_heighttap` already solved this and its solution is copied here
// rather than re-derived: TESS first, this block's read on the cycles TESS
// leaves.  `c_lat_req_o` is `o_lat_req_i` whenever the upstream client is
// asking, and this block's own request only on a cycle when it is not.  The
// response needs no multiplexing at all -- if the upstream asked, the next
// cycle's datum is the upstream's; if this block injected, it is this block's;
// and the two cases are exclusive by construction.
//
// THE COST OF LOSING THE RACE IS A DURATION AND NOT A FAULT, which is
// `zhao_terrain_jobissue`'s lesson from this same run: a counter that moves in
// normal operation cannot be read as a fault.  `lat_wait_clocks_o` climbs while
// this block wants the bus and the client has it.  In the composed console it
// should read very low -- TERRAIN.TESS has no job for a patch until
// TERRAIN.LOD has decided it, and TERRAIN.LOD cannot decide until this block
// has fed it -- but "should" is the word that makes it an instrument rather
// than an assumption, and the number is exported so the assumption is
// falsifiable in a console bench instead of argued here.
//
// ===========================================================================
// THE WALK
// ===========================================================================
// Sixteen subpatches, in `zhao_terrain_devstore`'s own read order, which is
// `zhao_terrain_loddev`'s emit order, which is `sp = {oz/8, ox/8}` -- the
// encoding is `zhao_terrain_loddev.sv`'s `dev_sp_o` comment and its `ox_c`/
// `oz_c`, and `zhao_terrain_lod` re-derives the same two fields from its own
// emit index (`out_ox_o <= {1'b0, e_i, 3'b000}`).  This block does not invent
// an order; it forwards the store's.
//
// The centre vertex of subpatch `sp` is (ox + 4, oz + 4), which is the vertex
// `zhao_terrain_lodfeed` sampled `w_cy_o` at.  ONE SOURCE OF TRUTH FOR THE
// CENTRE: if that offset ever moves, it moves in `CENTRE_OFF` below and in
// lodfeed together, and the elaboration guard checks it stays on the lattice.
//
// Per subpatch: take the store's record, request the centre's position on the
// first free lattice cycle, capture it the cycle after, emit.  Three to four
// clocks each, ~56 per patch against a frame's 1.67 M -- so the walk is priced
// and is not worth pipelining.  `assemble_clocks_o` prints the real number
// rather than this file pinning it (`zhao_terrain_lodfeed`'s rule: a pinned
// clock count goes stale silently).
//
// THE SURFACE READ IS 0 AND IT IS NOT A CHOICE.  `lat_wx_o`/`lat_wz_o` are the
// placed COLUMN and ROW positions; they are indexed by `vi`/`vj` alone and the
// compose cache does not select them on `lat_surface_i` at all.  Surface 0 is
// passed because the port needs a value, not because the top was preferred --
// and `zhao_terrain_lodfeed` buffers only surface 0 besides (its R59 section),
// so there is no second reading for this block to pick between.
//
// HEIGHT16 -> FX16 IS `raw << 8`, EXACT (spec/qformats.md 9; the same sentence
// is in `zhao_terrain_lodfeed.sv` and `zhao_terrain_patch.sv`).  `sp_cy_o` is
// therefore the store's `r_cy_i` sign-extended to 32 and shifted, with no
// rounding and no saturation, because 16 bits shifted by 8 cannot leave 32.
//
// Conservative SystemVerilog subset (charter 2); no package dependencies.
// Quartus 17.0: elaboration checks live inside `initial begin ... end` and
// there is no module-scope `if` and no implicit generate (CLAUDE.md, R212).
`default_nettype none

module zhao_terrain_spdesc #(
    // The residency's slot width, `zhao_terrain_devstore`'s `SLOTW`.
    parameter int unsigned SLOTW = 10,
    // `zhao_terrain_devstore`'s deviation and morph widths.  Named rather than
    // literal so a width that moves in the store cannot silently truncate here.
    parameter int unsigned DEVW  = 24,
    parameter int unsigned MORPHW = 17,
    // The compose cache's lattice, so this block cannot disagree with it about
    // where a row ends.  Elaboration-checked below.
    parameter int unsigned LAT_W = 33,
    parameter int unsigned LAT_H = 33,
    // TERRAIN.LOD's fill law: sixteen subpatches per patch.  Named so the count
    // this block walks cannot drift from the count that block expects.
    parameter int unsigned SUBPATCHES = 16,
    // The subpatch edge in cells, and the centre offset within it.  A KNOB
    // rather than a derivation: `zhao_terrain_loddev` places subpatch `sp` at
    // (sp[1:0] * SUB_EDGE, sp[3:2] * SUB_EDGE) and `zhao_terrain_lodfeed`
    // samples the centre height at + CENTRE_OFF.  Both are here so a reader
    // changing one is looking at the other.
    parameter int unsigned SUB_EDGE   = 8,
    parameter int unsigned CENTRE_OFF = 4,
    // The door queue.  The compose engine holds at most two patches (one
    // filling, one served) and the door may be one patch ahead of the walk, so
    // 4 is two clear of the composition that uses it -- the same size and the
    // same reasoning as `zhao_terrain_jobissue`'s `CTXD`, whose queue is fed by
    // the same door pulse.  It is a KNOB, not a derivation.
    parameter int unsigned DOORD = 4
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- the PAGE IDENTITY, captured at the compose fill door ---------------
    // {slot, src_id} taken from `zhao_terrain_pagestream`'s vertex beat on the
    // cycle `zhao_terrain_compcache_front` ACCEPTS a fill.  The acceptance and
    // not the offer: a door that was refused never becomes a served patch, and
    // pushing the offer would leave an entry nothing pops.  See fact 3 above
    // for why this is the identity and not a map.
    input  var logic             door_valid_i,
    output var logic             door_ready_o,
    input  var logic [SLOTW-1:0] door_slot_i,
    input  var logic [15:0]      door_src_id_i,
    // THE PAGE'S DUAL FLAG, taken on the same beat as the slot and the id.
    // Added 2026-09-21 (packet TERRACOMP) when the composition found it had no
    // other honest source. `zhao_terrain_lod` takes `dual_i` as a LEVEL for
    // the patch it is deciding, and its contract says the governor targets --
    // `dual_i` among them -- "must be held stable across a patch job". The
    // composer's available net is TERRAIN.PAGESTREAM's live `v_flags_o`, which
    // is the FILLING page's: with one patch filling and another served those
    // are DIFFERENT PAGES, so wiring it live would have decided the served
    // patch with the next patch's underside flag. The queue below already
    // carries the {slot, src_id} pair captured at fill and popped at serve --
    // this is the same fact about the same page, on the same beat, and it
    // costs one bit per entry.
    input  var logic             door_dual_i,

    // ---- TERRAIN.COMPCACHE's serve side -------------------------------------
    input  var logic        serve_valid_i,     // LEVEL: a patch is served
    input  var logic [15:0] serve_src_id_i,

    // ---- TERRAIN.COMPCACHE's lattice port, PASSED THROUGH -------------------
    // Upstream is whoever held this port before -- in the console,
    // `zhao_terrain_tess` by way of `zhao_terrain_heighttap`.  It is never
    // delayed; see the header.
    input  var logic               o_lat_req_i,
    input  var logic        [ 5:0] o_lat_vi_i,
    input  var logic        [ 5:0] o_lat_vj_i,
    input  var logic               o_lat_surface_i,
    // The velocity word, carried through on the SAME borrowed read.
    // NEW 2026-09-26 (TERRVEL). This block is a pass-through on the lattice
    // RESPONSE -- o_lat_h_o is a bare assign of c_lat_h_i -- so the velocity
    // rides the identical combinational path and cannot skew against the
    // height it describes. Routing it around this block instead, direct from
    // the compose cache to the heighttap, would have been two fewer edits and
    // would have put the two halves of one answer on two different paths.
    output var logic signed [15:0] o_lat_vel_o,
    output var logic               o_lat_vel_present_o,
    output var logic signed [31:0] o_lat_h_o,
    output var logic signed [31:0] o_lat_wx_o,
    output var logic signed [31:0] o_lat_wz_o,

    output var logic               c_lat_req_o,
    output var logic        [ 5:0] c_lat_vi_o,
    output var logic        [ 5:0] c_lat_vj_o,
    output var logic               c_lat_surface_o,
    input  var logic signed [15:0] c_lat_vel_i,
    input  var logic               c_lat_vel_present_i,
    input  var logic signed [31:0] c_lat_h_i,
    input  var logic signed [31:0] c_lat_wx_i,
    input  var logic signed [31:0] c_lat_wz_i,

    // ---- TERRAIN.DEVSTORE's read port ---------------------------------------
    output var logic              r_start_o,
    output var logic [SLOTW-1:0]  r_slot_o,
    input  var logic              r_ready_i,      // the store's `r_ready_o`
    input  var logic              r_valid_i,
    output var logic              r_ready_o,      // this block's take
    input  var logic [3:0]        r_sp_i,
    input  var logic [DEVW-1:0]   r_dev1_i,
    input  var logic [DEVW-1:0]   r_dev2_i,
    input  var logic [DEVW-1:0]   r_dev3_i,
    input  var logic signed [15:0] r_cy_i,
    input  var logic [1:0]        r_prev_level_i,
    input  var logic [MORPHW-1:0] r_prev_morph_i,
    input  var logic [7:0]        r_hold_i,
    input  var logic              r_fresh_i,

    // ---- TERRAIN.LOD's `sp_*` patch_state port ------------------------------
    output var logic               sp_valid_o,
    input  var logic               sp_ready_i,
    output var logic signed [31:0] sp_cx_o,
    output var logic signed [31:0] sp_cy_o,
    output var logic signed [31:0] sp_cz_o,
    output var logic [DEVW-1:0]    sp_dev1_o,
    output var logic [DEVW-1:0]    sp_dev2_o,
    output var logic [DEVW-1:0]    sp_dev3_o,
    output var logic [1:0]         sp_prev_level_o,
    output var logic [MORPHW-1:0]  sp_prev_morph_o,
    output var logic [7:0]         sp_hold_o,
    output var logic [15:0]        sp_src_id_o,
    // THE PATCH'S DUAL FLAG, as a LEVEL rather than a descriptor field: it is a
    // property of the PAGE, identical for all sixteen subpatches, and
    // `zhao_terrain_lod` takes it as a module input. Stable for the whole burst
    // by construction -- it is loaded with `act_slot_q` and `act_src_q` when a
    // patch is adopted and not touched again until the next one.
    output var logic               patch_dual_o,

    // ---- evidence -----------------------------------------------------------
    output var logic [31:0] patches_assembled_o,
    output var logic [31:0] descriptors_emitted_o,
    output var logic [31:0] door_refused_o,
    output var logic [31:0] serve_no_door_o,
    output var logic [31:0] door_src_mismatch_o,
    output var logic [31:0] sp_order_bad_o,
    output var logic [31:0] patches_unfresh_o,
    output var logic [31:0] store_wait_clocks_o,
    output var logic [31:0] lat_wait_clocks_o,
    output var logic [31:0] assemble_clocks_o,
    output var logic        busy_o
);

  // ---------------------------------------------------------------------------
  // ELABORATION GUARDS.  `initial begin ... end` and not a module-scope `if`:
  // Quartus 17.0 rejects the latter outright (CLAUDE.md), and `--lint-only`
  // does not run this -- which is exactly why the SHAPE is also checked by
  // `tools/quartus/check_quartus17_syntax.py` and the VALUES by the directed
  // test's parameter cases.
  // ---------------------------------------------------------------------------
  // synthesis translate_off
  initial begin
    if (SUBPATCHES != 16)
      $fatal(1, "zhao_terrain_spdesc: `sp` is a 4-bit index; SUBPATCHES must be 16, got %0d", SUBPATCHES);
    // The centre this block asks for must be ON the lattice the cache holds.
    // 3 * 8 + 4 = 28 < 33.  A SUB_EDGE or CENTRE_OFF that walks off the edge
    // would read `lat_oob_o` instead of a position, and the cache would answer
    // with POISON that looks exactly like a placement.
    if ((3 * SUB_EDGE + CENTRE_OFF) >= LAT_W)
      $fatal(1, "zhao_terrain_spdesc: subpatch centre column %0d is off a %0d-wide lattice",
             3 * SUB_EDGE + CENTRE_OFF, LAT_W);
    if ((3 * SUB_EDGE + CENTRE_OFF) >= LAT_H)
      $fatal(1, "zhao_terrain_spdesc: subpatch centre row %0d is off a %0d-deep lattice",
             3 * SUB_EDGE + CENTRE_OFF, LAT_H);
    if (DOORD < 2)
      $fatal(1, "zhao_terrain_spdesc: DOORD must cover the compose cache's two buffers, got %0d", DOORD);
  end
  // synthesis translate_on

  localparam int unsigned IDXW = (DOORD <= 2) ? 1 : $clog2(DOORD);
  localparam int unsigned CNTW = $clog2(DOORD + 1);

  // Sized once, here.  A part-select on an `int unsigned` parameter
  // (`SUB_EDGE[5:0]`) is the obvious spelling and Quartus 17.0 is not reliable
  // about it; a sized localparam is the form this tree already uses.
  localparam logic [5:0] SUB_EDGE_6   = 6'(SUB_EDGE);
  localparam logic [5:0] CENTRE_OFF_6 = 6'(CENTRE_OFF);

  // The subpatch index this walk expects next.  A function rather than an
  // expression inline so the order law is written ONCE and the checker below
  // cannot drift from it.  DECLARED BEFORE ITS USE: Quartus 17.0 does not take
  // a forward reference to a function inside a module, however cleanly the
  // lint tool accepts one (R212 -- lint-clean is not synthesizable).
  //
  // AND A COMMENT MAY NOT BEGIN WITH THAT TOOL'S NAME.  The line above used to,
  // and `--lint-only` rejected the file with BADVLTPRAGMA: a `//` comment whose
  // FIRST WORD is the tool name is parsed as a pragma, not as prose.  Recorded
  // because the error names an "unknown verilator comment" and points at a
  // sentence about Quartus, which reads like a corrupt file rather than a
  // reserved word.
  // The argument is FOUR bits and the caller narrows `act_count_q` to pass it.
  // A five-bit argument would carry a bit this function must not read: the
  // count reaches 16 exactly once, on the cycle the walk finishes, and that
  // cycle leaves for StIdle rather than asking for a seventeenth subpatch.
  function automatic logic [3:0] rec_sp_next(input logic [3:0] n);
    rec_sp_next = n;
  endfunction

  // ---------------------------------------------------------------------------
  // THE DOOR QUEUE
  // ---------------------------------------------------------------------------
  logic [SLOTW-1:0] dq_slot [0:DOORD-1];
  logic [15:0]      dq_src  [0:DOORD-1];
  logic             dq_dual [0:DOORD-1];
  logic [IDXW-1:0]  dq_wr_q, dq_rd_q;
  logic [CNTW-1:0]  dq_cnt_q;

  wire dq_empty_c = (dq_cnt_q == CNTW'(0));
  wire dq_full_c  = (dq_cnt_q == CNTW'(DOORD));

  assign door_ready_o = !dq_full_c;
  wire dq_push_c = door_valid_i && door_ready_o;

  // ---------------------------------------------------------------------------
  // THE WALK
  // ---------------------------------------------------------------------------
  typedef enum logic [2:0] {
    StIdle,   // waiting for a served patch with an identity at the door
    StStart,  // presenting `r_start_o` to the store
    StRec,    // waiting for the store's next record
    StLReq,   // waiting for a free lattice cycle
    StLRsp,   // the datum is on the port THIS cycle
    StEmit    // offering the descriptor to TERRAIN.LOD
  } state_e;

  state_e st_q;

  logic [SLOTW-1:0] act_slot_q;
  logic             act_dual_q;
  logic [15:0]      act_src_q;
  logic [4:0]       act_count_q;   // 0..16, so five bits
  logic             serve_seen_q;

  // The record in hand, latched out of the store so the store's read pointer
  // may advance while this block is waiting for the lattice bus.
  logic [3:0]        rec_sp_q;
  logic [DEVW-1:0]   rec_dev1_q, rec_dev2_q, rec_dev3_q;
  logic signed [15:0] rec_cy_q;
  logic [1:0]        rec_level_q;
  logic [MORPHW-1:0] rec_morph_q;
  logic [7:0]        rec_hold_q;
  logic signed [31:0] rec_wx_q, rec_wz_q;

  // The centre vertex of the record in hand.  `zhao_terrain_loddev`'s encoding,
  // forwarded rather than reinvented: ox = sp[1:0] * SUB_EDGE, oz = sp[3:2].
  wire [5:0] centre_vi_c = 6'({4'b0, rec_sp_q[1:0]} * SUB_EDGE_6 + CENTRE_OFF_6);
  wire [5:0] centre_vj_c = 6'({4'b0, rec_sp_q[3:2]} * SUB_EDGE_6 + CENTRE_OFF_6);

  // THE ARM.  One per rising `serve_valid_i`, by `zhao_terrain_jobissue`'s law
  // -- `serve_seen_q` clears when the level drops, and the level drops between
  // patches because the retirement pulse is what lowers it.  Copied and not
  // re-derived: two blocks arming off one event must not have two laws.
  wire dq_pop_c = (st_q == StIdle) && serve_valid_i && !serve_seen_q && !dq_empty_c;

  // THE LATTICE BUS.  Upstream always wins; this block injects on the cycles
  // upstream leaves.  See the header for why there is no ready to wait on.
  wire lat_free_c = !o_lat_req_i;
  wire lat_fire_c = (st_q == StLReq) && lat_free_c;

  assign c_lat_req_o     = o_lat_req_i || lat_fire_c;
  assign c_lat_vi_o      = o_lat_req_i ? o_lat_vi_i : centre_vi_c;
  assign c_lat_vj_o      = o_lat_req_i ? o_lat_vj_i : centre_vj_c;
  assign c_lat_surface_o = o_lat_req_i ? o_lat_surface_i : 1'b0;

  // The response is not multiplexed: the cycle after an upstream request the
  // datum belongs to upstream, and this block did not inject on that cycle.
  assign o_lat_vel_o         = c_lat_vel_i;
  assign o_lat_vel_present_o = c_lat_vel_present_i;
  assign o_lat_h_o  = c_lat_h_i;
  assign o_lat_wx_o = c_lat_wx_i;
  assign o_lat_wz_o = c_lat_wz_i;

  assign r_start_o = (st_q == StStart);
  assign r_slot_o  = act_slot_q;
  assign patch_dual_o = act_dual_q;
  assign r_ready_o = (st_q == StRec);

  assign sp_valid_o      = (st_q == StEmit);
  assign sp_cx_o         = rec_wx_q;
  assign sp_cz_o         = rec_wz_q;
  // height16 -> fx16 is `raw << 8`, exact.  Sign-extend to 32 FIRST, then
  // shift, so a negative height does not lose its sign in a 16-bit shift.
  assign sp_cy_o         = {{8{rec_cy_q[15]}}, rec_cy_q, 8'b0};
  assign sp_dev1_o       = rec_dev1_q;
  assign sp_dev2_o       = rec_dev2_q;
  assign sp_dev3_o       = rec_dev3_q;
  assign sp_prev_level_o = rec_level_q;
  assign sp_prev_morph_o = rec_morph_q;
  assign sp_hold_o       = rec_hold_q;
  assign sp_src_id_o     = act_src_q;

  assign busy_o = (st_q != StIdle) || !dq_empty_c;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      dq_wr_q  <= '0;
      dq_rd_q  <= '0;
      dq_cnt_q <= '0;

      st_q         <= StIdle;
      act_slot_q   <= '0;
      // A page with no underside is the legacy single-surface page, which is
      // what an unwritten flag should mean: reset to 0 presents no underside
      // rather than inventing one.
      act_dual_q   <= 1'b0;
      act_src_q    <= '0;
      act_count_q  <= '0;
      serve_seen_q <= 1'b0;

      rec_sp_q    <= '0;
      rec_dev1_q  <= '0;
      rec_dev2_q  <= '0;
      rec_dev3_q  <= '0;
      rec_cy_q    <= '0;
      rec_level_q <= '0;
      rec_morph_q <= '0;
      rec_hold_q  <= '0;
      rec_wx_q    <= '0;
      rec_wz_q    <= '0;

      patches_assembled_o   <= '0;
      descriptors_emitted_o <= '0;
      door_refused_o        <= '0;
      serve_no_door_o       <= '0;
      door_src_mismatch_o   <= '0;
      sp_order_bad_o        <= '0;
      patches_unfresh_o     <= '0;
      store_wait_clocks_o   <= '0;
      lat_wait_clocks_o     <= '0;
      assemble_clocks_o     <= '0;
    end else begin
      // ---- the door queue, accounted in ONE place -------------------------
      // The count moves here and nowhere else.  `zhao_terrain_jobissue`'s
      // header records what an earlier draft of the same queue cost when the
      // count was updated inside four separate case arms: a push landing in
      // the same cycle as a pop on an empty queue dropped the increment.
      if (dq_push_c) begin
        dq_slot[dq_wr_q] <= door_slot_i;
        dq_src[dq_wr_q]  <= door_src_id_i;
        dq_dual[dq_wr_q] <= door_dual_i;
        dq_wr_q          <= (dq_wr_q == IDXW'(DOORD - 1)) ? '0 : dq_wr_q + IDXW'(1);
      end
      if (dq_pop_c) begin
        dq_rd_q <= (dq_rd_q == IDXW'(DOORD - 1)) ? '0 : dq_rd_q + IDXW'(1);
      end
      if (dq_push_c && !dq_pop_c)      dq_cnt_q <= dq_cnt_q + CNTW'(1);
      else if (dq_pop_c && !dq_push_c) dq_cnt_q <= dq_cnt_q - CNTW'(1);

      if (door_valid_i && !door_ready_o && (door_refused_o != 32'hFFFF_FFFF)) begin
        door_refused_o <= door_refused_o + 32'd1;
      end

      // ---- the serve level's edge ----------------------------------------
      if (!serve_valid_i) serve_seen_q <= 1'b0;

      // ---- the two DURATION instruments ----------------------------------
      // Neither is a fault counter and neither may be read as one, which is
      // `zhao_terrain_jobissue`'s correction from this same run applied before
      // the mistake rather than after it.  `store_wait_clocks_o` climbs while
      // this block is armed and the store has not produced the next record;
      // `lat_wait_clocks_o` climbs while it wants the lattice bus and the
      // upstream client has it.  BOTH ARE ZERO-OR-MORE IN A HEALTHY CONSOLE.
      // The pair to read is either climbing while `patches_assembled_o` is
      // FLAT -- a starve -- and that reading needs both numbers, which is why
      // the flat one is exported beside them.
      if (((st_q == StRec) || (st_q == StStart)) &&
          (store_wait_clocks_o != 32'hFFFF_FFFF)) begin
        store_wait_clocks_o <= store_wait_clocks_o + 32'd1;
      end
      if ((st_q == StLReq) && !lat_free_c && (lat_wait_clocks_o != 32'hFFFF_FFFF)) begin
        lat_wait_clocks_o <= lat_wait_clocks_o + 32'd1;
      end
      if ((st_q != StIdle) && (assemble_clocks_o != 32'hFFFF_FFFF)) begin
        assemble_clocks_o <= assemble_clocks_o + 32'd1;
      end

      case (st_q)
        StIdle: begin
          if (serve_valid_i && !serve_seen_q) begin
            serve_seen_q <= 1'b1;
            if (dq_empty_c) begin
              // A patch was served that this block has no identity for.  It
              // cannot be assembled: there is no slot to read the store with,
              // and inventing one would file another page's deviations under
              // this patch.  Counted and skipped.
              if (serve_no_door_o != 32'hFFFF_FFFF) serve_no_door_o <= serve_no_door_o + 32'd1;
            end else begin
              act_slot_q  <= dq_slot[dq_rd_q];
              act_src_q   <= dq_src[dq_rd_q];
              act_dual_q  <= dq_dual[dq_rd_q];
              act_count_q <= '0;
              // THE PAIRING, CHECKED.  See fact 3 in the header for why these
              // two operands are not corrupted in lockstep.
              if ((dq_src[dq_rd_q] != serve_src_id_i) &&
                  (door_src_mismatch_o != 32'hFFFF_FFFF)) begin
                door_src_mismatch_o <= door_src_mismatch_o + 32'd1;
              end
              st_q <= StStart;
            end
          end
        end

        StStart: begin
          if (r_ready_i) st_q <= StRec;
        end

        StRec: begin
          if (r_valid_i) begin
            // `r_fresh_i` IS SAMPLED HERE AND NOT AT THE START, and the one
            // cycle matters.  `zhao_terrain_devstore` loads `r_fresh_q` from
            // `slot_valid_q[r_slot_i]` INSIDE its `R_IDLE: if (r_start_i)`
            // arm, so on the cycle this block presents the start the port
            // still carries the PREVIOUS patch's answer.  Reading it there
            // would attribute page A's freshness to page B -- the join fault
            // CLAUDE.md's metadata-swap chapter is about, one port along.
            // Sampled on the FIRST record, when the store is in R_STREAM and
            // the flag describes the slot being streamed.
            //
            // Counted PER PATCH.  The store's own `read_unwritten_o` counts
            // per READ; "one page arrived late" and "sixteen reads missed" are
            // different sentences and the budget wants the first one.
            if ((act_count_q == 5'd0) && !r_fresh_i &&
                (patches_unfresh_o != 32'hFFFF_FFFF)) begin
              patches_unfresh_o <= patches_unfresh_o + 32'd1;
            end
            // THE STORE'S ORDER IS FORWARDED, NOT ASSUMED.  The record's own
            // `sp` must be the one this walk is at; a store that re-ordered or
            // skipped would otherwise pair subpatch 3's deviations with
            // subpatch 4's centre and nothing downstream could see it.
            if ((r_sp_i != rec_sp_next(act_count_q[3:0])) &&
                (sp_order_bad_o != 32'hFFFF_FFFF)) begin
              sp_order_bad_o <= sp_order_bad_o + 32'd1;
            end
            rec_sp_q    <= r_sp_i;
            rec_dev1_q  <= r_dev1_i;
            rec_dev2_q  <= r_dev2_i;
            rec_dev3_q  <= r_dev3_i;
            rec_cy_q    <= r_cy_i;
            rec_level_q <= r_prev_level_i;
            rec_morph_q <= r_prev_morph_i;
            rec_hold_q  <= r_hold_i;
            st_q        <= StLReq;
          end
        end

        StLReq: begin
          if (lat_free_c) st_q <= StLRsp;
        end

        StLRsp: begin
          // The datum for the request made last cycle is on the port NOW.
          rec_wx_q <= c_lat_wx_i;
          rec_wz_q <= c_lat_wz_i;
          st_q     <= StEmit;
        end

        StEmit: begin
          if (sp_ready_i) begin
            if (descriptors_emitted_o != 32'hFFFF_FFFF) begin
              descriptors_emitted_o <= descriptors_emitted_o + 32'd1;
            end
            if (act_count_q + 5'd1 == 5'(SUBPATCHES)) begin
              if (patches_assembled_o != 32'hFFFF_FFFF) begin
                patches_assembled_o <= patches_assembled_o + 32'd1;
              end
              st_q <= StIdle;
            end else begin
              act_count_q <= act_count_q + 5'd1;
              st_q        <= StRec;
            end
          end
        end

        default: st_q <= StIdle;
      endcase
    end
  end

endmodule

`default_nettype wire
