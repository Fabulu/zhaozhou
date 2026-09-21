// zhao_geom_warpbook.sv -- THE PER-DRAW WARP DESCRIPTOR BOOK.
//
// Owner directive reports/Zhaozhou_GEOM_WARP_Architecture_2026-09-20.txt,
// ratified in reports/OWNER-RATIFICATION-20260920-WARP.md.
//
//   7.1 -- "the common draw item holds `warp_enabled` plus a compact
//   > descriptor cookie, NOT an 80-byte Warp record copied through every
//   > geometry stage."
//
//   W05 -- SNAPSHOT EVERY DRAW. "a setting that could retroactively change
//   > older draws" is forbidden: the program handle, the tick, the parameters
//   > and the displacement bound belong to THAT draw.
//
// ---------------------------------------------------------------------------
// WHAT THIS IS FOR, IN ONE PARAGRAPH
// ---------------------------------------------------------------------------
// `zhao_cmd_exec` decodes DrawWarpedForm 0x0304 into a 456-bit per-draw
// snapshot. `zhao_geom_warp` sits POST-SKIN and needs that snapshot in phase
// with that draw's vertices. The geometry front is pipelined, so draw N's tail
// vertices overlap draw N+1's head; a single held register at the skin stage
// would hand vertex B's position to descriptor A, which is CLAUDE.md's
// metadata-swap defect exactly, and every accepted/emitted counter would
// balance while it happened.
//
// 7.1 refuses the other obvious answer -- widening every geometry stage by 456
// bits -- by name. So the snapshot stays HERE, in a small book, and what rides
// the vertex is a SIX-BIT COOKIE.
//
// ---------------------------------------------------------------------------
// THE COOKIE, AND WHY IT CARRIES A STAMP RATHER THAN JUST AN INDEX
// ---------------------------------------------------------------------------
//   cookie = { en, stamp[STAMPW-1:0] }          CKW = 1 + STAMPW
//   entry  = stamp[IDXW-1:0]
//
// An index alone would be an unqualified pointer into a ring. With ENTRIES
// draws' worth of storage, the (ENTRIES+1)-th warped draw overwrites the first
// -- and a vertex of the first draw arriving after that would read the new
// descriptor and DEFORM AGAINST THE WRONG PROGRAM, silently, with a plausible
// shape. The stamp makes that state NAMEABLE: the book keeps the stamp it
// stored, the vertex carries the stamp it was issued, and a read whose stamps
// disagree is refused and counted rather than answered.
//
// THE TWO SIDES OF THAT COMPARISON ARE CLOCKED BY DIFFERENT THINGS, which is
// the property CLAUDE.md's lockstep-blindness law asks for and the one a
// detector usually fails. `stamp_q[e]` is written by the WRITE port, on the
// draw handshake, inside this module. `r_cookie_i[STAMPW-1:0]` arrives on the
// VERTEX, having travelled the whole geometry front in the per-vertex
// registers beside `src_id`. No register enable drives both; an overwrite
// moves one and cannot move the other.
//
// STAMPW IS WIDER THAN IDXW ON PURPOSE. At STAMPW = 5 and ENTRIES = 4, the
// stamp repeats every 32 warped draws while the entry repeats every 4, so the
// detector sees an overwrite at any distance from 4 to 31 draws. Making them
// equal would make the stamp a restatement of the index and the check
// vacuous -- a comparison that cannot disagree, which is worse than no
// comparison because it reads as one.
//
// ---------------------------------------------------------------------------
// WHAT IT DOES NOT DO
// ---------------------------------------------------------------------------
// It does not free entries. There is no retire signal in this console saying
// "the last vertex of draw N has passed the skinner", and inventing one from a
// vertex count would be a second implementation of the meshlet walk that
// `zhao_geom_assetfetch` owns -- and would be WRONG besides, because that
// block's `release_i` may truncate a meshlet's vertex service by design. A ring
// with a stamp needs no free list: the storage is reused on a schedule and the
// reuse is DETECTED rather than prevented.
//
// It does not resolve `warp_program` to a resident slot. That is decision W07's
// "one shared program-binding authority", it lives in `fpga/rtl/field/`, and
// GEOM.WARP prerequisite P8 records that its middle link -- canonical program
// to resident slot -- has no storage anywhere in this tree. The composer takes
// the slot from the console boundary, exactly as it does for the FLOW and the
// STAMP profiles.
//
// Conservative SystemVerilog subset (Quartus 17.0): no module-scope `if`, no
// implicit generate, elaboration guards inside `initial begin ... end`.
//
// ENFORCED-BY: tests/geometry/geom_warpbook_directed.cpp:main
`default_nettype none

module zhao_geom_warpbook #(
    // Descriptors held at once. FOUR is the draw depth `zhao_cmd_exec`'s own
    // sidecar uses (DRAW_Q), so the book is not the narrower of the two.
    parameter int unsigned ENTRIES = 4,
    parameter int unsigned IDXW    = 2,   // $clog2(ENTRIES) at ENTRIES = 4
    parameter int unsigned STAMPW  = 5,
    parameter int unsigned CKW     = 6,   // 1 + STAMPW
    parameter int unsigned CNTW    = 32
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- WRITE: one per ACCEPTED draw ---------------------------------------
    // `w_fire_i` is the draw handshake itself (`valid && ready`), so the book
    // and the job leave the same cycle with the same identity and there is no
    // second handshake for them to skew across.
    input  var logic                w_fire_i,
    input  var logic                w_en_i,        // this draw asked for a Warp
    input  var logic [31:0]         w_time_i,
    input  var logic [127:0]        w_par_i,
    input  var logic [127:0]        w_attr_i,
    input  var logic [7:0]          w_attr_mode_i,
    input  var logic signed [31:0]  w_bx_i,
    input  var logic signed [31:0]  w_by_i,
    input  var logic signed [31:0]  w_bz_i,
    // THE COOKIE FOR THE DRAW BEING ACCEPTED, combinational in the accept
    // cycle. It has to be: `zhao_geom_drawjob` latches the whole draw on that
    // edge, and a cookie arriving one clock later would belong to the draw
    // after it.
    output var logic [CKW-1:0]      w_cookie_o,

    // ---- READ: one per vertex, combinational --------------------------------
    input  var logic [CKW-1:0]      r_cookie_i,
    // THE CONSUMER'S ACCEPT. The read itself is combinational and a cookie
    // SITS on the port for as long as the vertex it belongs to is being
    // offered, so counting on the wire would count stall cycles. This pulse is
    // what makes a read a read, and it is the vertex handshake -- not a second
    // one this block invents, which would be a handshake that could skew
    // against the thing it counts.
    input  var logic                r_fire_i,
    // HIGH means "this vertex's draw asked for a Warp AND its descriptor is
    // still the one it was issued". It is the ONLY thing that should reach
    // `zhao_geom_warp.d_warp_en_i`: a stale read must fall back to W09's
    // bypass, never to another draw's deformation.
    output var logic                r_en_o,
    output var logic [31:0]         r_time_o,
    output var logic [127:0]        r_par_o,
    output var logic [127:0]        r_attr_o,
    output var logic [7:0]          r_attr_mode_o,
    output var logic signed [31:0]  r_bx_o,
    output var logic signed [31:0]  r_by_o,
    output var logic signed [31:0]  r_bz_o,

    // ---- evidence -----------------------------------------------------------
    output var logic [CNTW-1:0]     allocated_o,   // warped draws given a cookie
    output var logic [CNTW-1:0]     hits_o,        // vertices answered
    output var logic [CNTW-1:0]     stale_o        // the overwrite detector
);

  // --------------------------------------------------------------------------
  // ELABORATION GUARDS. Inside `initial begin ... end` -- Quartus 17.0 rejects
  // a bare module-scope `if`, and `--lint-only` does not run these at all, so
  // a clean lint says nothing whatever about them (CLAUDE.md, R212).
  // --------------------------------------------------------------------------
  initial begin
    if (CKW != (1 + STAMPW))
      $fatal(1, "zhao_geom_warpbook: CKW is {en, stamp} -- 1 + STAMPW");
    if (STAMPW <= IDXW)
      // STAMPW <= IDXW makes the stamp a restatement of the entry index, so
      // the staleness comparison could never disagree -- a checker that
      // cannot fire, which is worse than no checker because it reads as one.
      $fatal(1, "zhao_geom_warpbook: STAMPW must EXCEED IDXW");
    if ((32'd1 << IDXW) != ENTRIES)
      $fatal(1, "zhao_geom_warpbook: IDXW must be exactly $clog2(ENTRIES)");
  end

  // --------------------------------------------------------------------------
  // THE STORE
  // --------------------------------------------------------------------------
  logic [STAMPW-1:0] alloc_q;          // the next stamp to issue
  logic              vld_q   [ENTRIES];
  logic [STAMPW-1:0] stamp_q [ENTRIES];
  logic [31:0]       time_q  [ENTRIES];
  logic [127:0]      par_q   [ENTRIES];
  logic [127:0]      attr_q  [ENTRIES];
  logic [7:0]        amode_q [ENTRIES];
  logic [31:0]       bx_q    [ENTRIES];
  logic [31:0]       by_q    [ENTRIES];
  logic [31:0]       bz_q    [ENTRIES];

  wire [IDXW-1:0] w_idx_c = alloc_q[IDXW-1:0];

  // The cookie is the stamp ABOUT to be issued, qualified by the draw's own
  // enable. A draw that names no program gets `en` low and no entry: W09
  // requires that path to perform zero Warp lookups, and consuming a stamp for
  // it would shorten the detector's reach for no gain.
  assign w_cookie_o = {w_en_i, alloc_q};

  // --------------------------------------------------------------------------
  // THE READ. Combinational, by cookie.
  // --------------------------------------------------------------------------
  wire [IDXW-1:0]   r_idx_c   = r_cookie_i[IDXW-1:0];
  wire [STAMPW-1:0] r_stamp_c = r_cookie_i[STAMPW-1:0];
  wire              r_ask_c   = r_cookie_i[CKW-1];
  wire              r_hit_c   = r_ask_c && vld_q[r_idx_c] &&
                                (stamp_q[r_idx_c] == r_stamp_c);

  assign r_en_o        = r_hit_c;
  assign r_time_o      = time_q[r_idx_c];
  assign r_par_o       = par_q[r_idx_c];
  assign r_attr_o      = attr_q[r_idx_c];
  assign r_attr_mode_o = amode_q[r_idx_c];
  assign r_bx_o        = $signed(bx_q[r_idx_c]);
  assign r_by_o        = $signed(by_q[r_idx_c]);
  assign r_bz_o        = $signed(bz_q[r_idx_c]);

  // --------------------------------------------------------------------------
  // THE COUNTERS, and one note on what `hits_o` is NOT.
  // --------------------------------------------------------------------------
  // `hits_o` counts READ CYCLES in which a cookie resolved, not vertices: the
  // read port is combinational and the consumer samples it on its own accept.
  // It is here to give `stale_o` a denominator, and it is the composer's
  // `warp_desc_*` pair that the console exports. Counting it as "vertices
  // warped" would be a second, disagreeing definition of a number
  // `zhao_geom_warp.vertices_transformed_o` already owns.
  //
  // `stale_o` IS REACHABLE WITH LEGAL STIMULUS and needs no mutant: allocate
  // ENTRIES+1 warped draws, then present the first draw's cookie. That is the
  // exact overwrite the ring makes possible, it is ordinary input, and
  // geom_warpbook_directed.cpp does it -- and does the negative half too, so
  // the counter is discriminated in both directions rather than merely seen
  // to move.
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      alloc_q     <= '0;
      allocated_o <= '0;
      hits_o      <= '0;
      stale_o     <= '0;
      for (int unsigned k = 0; k < ENTRIES; k++) begin
        vld_q[k]   <= 1'b0;
        stamp_q[k] <= '0;
        time_q[k]  <= 32'd0;
        par_q[k]   <= 128'd0;
        attr_q[k]  <= 128'd0;
        amode_q[k] <= 8'd0;
        bx_q[k]    <= 32'd0;
        by_q[k]    <= 32'd0;
        bz_q[k]    <= 32'd0;
      end
    end else begin
      // ---- the write -------------------------------------------------------
      if (w_fire_i && w_en_i) begin
        vld_q[w_idx_c]   <= 1'b1;
        stamp_q[w_idx_c] <= alloc_q;
        time_q[w_idx_c]  <= w_time_i;
        par_q[w_idx_c]   <= w_par_i;
        attr_q[w_idx_c]  <= w_attr_i;
        amode_q[w_idx_c] <= w_attr_mode_i;
        bx_q[w_idx_c]    <= w_bx_i;
        by_q[w_idx_c]    <= w_by_i;
        bz_q[w_idx_c]    <= w_bz_i;
        alloc_q          <= alloc_q + 1'b1;
        if (allocated_o != {CNTW{1'b1}}) allocated_o <= allocated_o + 1'b1;
      end

      // ---- the counters, ON THE CONSUMER'S ACCEPT --------------------------
      // A cookie asking for a Warp resolves or it does not; the two arms are
      // exclusive and exhaustive, so `hits_o + stale_o` is exactly the number
      // of accepted vertices whose draw named a program. A cookie with `en`
      // low is an ordinary draw and is counted by neither -- W09's bypass has
      // its own counter one block along and a second one here would be a
      // second definition of it.
      if (r_fire_i && r_ask_c && r_hit_c && (hits_o != {CNTW{1'b1}}))
        hits_o <= hits_o + 1'b1;
      if (r_fire_i && r_ask_c && !r_hit_c && (stale_o != {CNTW{1'b1}}))
        stale_o <= stale_o + 1'b1;
    end
  end

endmodule : zhao_geom_warpbook

`default_nettype wire
