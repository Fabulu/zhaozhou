// zhao_geom_tidq.sv -- THE TRIANGLE-IDENTITY QUEUE between GEOM.VERTID and the
// shell door. Console entry I54.
//
// ---------------------------------------------------------------------------
// WHY THIS EXISTS AT ALL
// ---------------------------------------------------------------------------
// `zhao_geom_paramarena` gives every TriangleDescriptor its arena index at the
// clock it is allocated, and `zhao_geom_vertid` re-exports it on `tri_id_o`.
// The chunk serialiser needs that index to reach `zhao_geom_binner_v2`'s
// triangle store, because a chunk of BINNER SLOTS would decode cleanly into
// the wrong triangles with every range guard passing -- the failure entry I54
// exists to prevent.
//
// THE TWO BEATS ARE NOT THE SAME BEAT, and that is the whole job here.
// `zhao_console_core` forks GEOM.CLIP's output into GEOM.SETUP, GEOM.ATTRPACK
// and GEOM.VERTID under one shared ready, so all three take the SAME triangle
// on the SAME clock or none of them do. But `tri_id_valid_o` fires later, on
// VERTID's own S_TD step, after it has resolved three corners; and the shell
// door fires later again, three stages down GEOM.SETUP. The id and the
// triangle it belongs to therefore arrive at different times and must be
// rejoined in order.
//
// GEOM.SETUP MAKES THAT REJOIN SOUND, and this is the measured fact the design
// rests on: it is a strict three-stage shift register with ONE enable, no cull,
// no reject and no drop -- its own header says "no clipping, no winding
// decision and no zero-area reject", its valid chain is three unconditional
// assignments, and it has no reject counter because there is nothing to count.
// One triangle in is exactly one packet out, in order. So a FIFO is the right
// instrument and a re-association key is not needed.
//
// ---------------------------------------------------------------------------
// WHAT IS *NOT* ASSUMED, BECAUSE THIS IS WHERE THE LOCKSTEP TRAP LIVES
// ---------------------------------------------------------------------------
// The tempting version pushes on `tri_id_valid_o`. IT IS WRONG, and wrong
// silently. `zhao_geom_vertid` drives
//
//     assign tri_id_valid_o = td_valid_o && td_ready_i && td_accept_i;
//
// so a descriptor the ARENA REFUSED -- no frame sealed, or the frame already
// faulted -- retires without an id. The triangle still went to GEOM.SETUP. A
// queue pushed on acceptance would then be one entry short for the rest of the
// frame and would hand every later triangle its PREDECESSOR'S index: ids that
// are in range, decode cleanly, and are wrong. That is the entry's own failure
// wearing a different hat, and no counter in the arena or the parambuf looks at
// it.
//
// So this queue is pushed on the TD RETIRE beat -- `td_valid_o && td_ready_i`,
// accepted or not -- and carries the ACCEPTANCE AS A BIT. An entry whose bit is
// clear names no triangle, and says so.
//
// AN UNKNOWN ID IS EMITTED AS ALL-ONES, NEVER AS ZERO. Zero is a perfectly
// legal TriangleDescriptor index: it passes `td_illegal_o`, it decodes, and it
// silently aims a tile list at whatever triangle happens to be first. All-ones
// is above any sealed `tris` this console can express, so the parambuf's range
// guard REFUSES it and counts it. When this block does not know the answer it
// makes the downstream guard fire instead of making it pass -- which is the one
// direction a broken instrument is allowed to lie in.
//
// ---------------------------------------------------------------------------
// THE TWO COUNTERS, AND WHAT CLOCKS THEIR OPERANDS
// ---------------------------------------------------------------------------
// `underflow_o` counts a door beat that found the queue EMPTY -- the id stream
// has fallen behind the triangle stream, which is the misalignment above
// arriving. `overflow_o` counts a push into a full queue, the same fault in the
// other direction.
//
// Neither compares two things loaded by one enable. The push side is clocked by
// VERTID's TD retire and the pop side by the shell door's handshake; they are
// different events in different blocks, which is exactly what makes the
// occupancy able to be wrong and therefore able to be seen. A checker whose two
// operands moved together could not see a skew at all -- CLAUDE.md's standing
// law, and the reason this paragraph names the two clocks.
//
// Both counters ARE REACHABLE WITH LEGAL STIMULUS -- hold `pop_i` low and push,
// or pop with nothing pushed -- so neither owes a committed mutant.
//
// Conservative SystemVerilog subset (charter §2); elaboration guards inside
// `initial begin ... end` for Quartus 17.
`default_nettype none

module zhao_geom_tidq #(
    // The arena's TriangleDescriptor index width. `td_id_o` is u18.
    parameter int unsigned ID_W = 18,
    // DEPTH covers the skew between VERTID's S_TD step and the shell door,
    // which is GEOM.SETUP's three stages plus the door's own join. Eight is
    // four times the structural minimum and is a KNOB, not a derived constant:
    // the skew is a property of two blocks' pipelining and the owner keeps the
    // slack. `overflow_o` says when it was not enough rather than a comment
    // saying it always is.
    parameter int unsigned DEPTH = 8,
    parameter int unsigned AW    = $clog2(DEPTH)
) (
    input  var logic clk,
    input  var logic rst_n,

    // A frame boundary. `zhao_geom_vertid` ABORTS a triangle in flight when the
    // arena seals (`vid_seal_abort_o` counts it), so an id can be owed and
    // never delivered across that edge. Clearing here is exact: the queue holds
    // only the frame being built, and an entry that outlived its frame would
    // name an index in a different frame's arena.
    input  var logic flush_i,

    // ---- push: GEOM.VERTID's TD step RETIRED (accepted or not) -------------
    input  var logic            push_i,
    input  var logic            id_ok_i,
    input  var logic [ID_W-1:0] id_i,

    // ---- pop: the shell door took the triangle -----------------------------
    input  var logic            pop_i,
    output var logic [ID_W-1:0] id_o,

    // ---- evidence -----------------------------------------------------------
    output var logic [31:0] underflow_o,
    output var logic [31:0] overflow_o,
    output var logic [31:0] unnamed_o,      // popped an entry that names nothing
    output var logic [AW:0]  level_o
);

  // The id that means "this block does not know". See the header: it is
  // all-ones so the parambuf's range guard REFUSES it, rather than zero, which
  // it would accept.
  localparam logic [ID_W-1:0] ID_POISON = {ID_W{1'b1}};

  initial begin
    if (DEPTH < 2)
      $fatal(1, "zhao_geom_tidq: DEPTH must be at least two");
    if ((1 << AW) != DEPTH)
      $fatal(1, "zhao_geom_tidq: DEPTH must be a power of two");
    if (ID_W < 2)
      $fatal(1, "zhao_geom_tidq: ID_W must be at least two");
  end

  logic [ID_W:0] q [0:DEPTH-1];   // {ok, id}
  logic [AW-1:0] rd_q, wr_q;
  logic [AW:0]   lvl_q;

  wire full_c  = (lvl_q == (AW+1)'(DEPTH));
  wire empty_c = (lvl_q == {(AW+1){1'b0}});

  wire        head_ok_c = q[rd_q][ID_W];
  wire [ID_W-1:0] head_id_c = q[rd_q][ID_W-1:0];

  // Combinational, so the id is on the port on the very beat the door takes the
  // triangle -- the same clock the binner samples its metadata.
  assign id_o    = (empty_c || !head_ok_c) ? ID_POISON : head_id_c;
  assign level_o = lvl_q;

  wire do_push_c = push_i && !full_c;
  wire do_pop_c  = pop_i  && !empty_c;

  integer k;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      rd_q  <= {AW{1'b0}};
      wr_q  <= {AW{1'b0}};
      lvl_q <= {(AW+1){1'b0}};
      underflow_o <= 32'd0;
      overflow_o  <= 32'd0;
      unnamed_o   <= 32'd0;
      for (k = 0; k < DEPTH; k = k + 1) q[k] <= {(ID_W+1){1'b0}};
    end else if (flush_i) begin
      rd_q  <= {AW{1'b0}};
      wr_q  <= {AW{1'b0}};
      lvl_q <= {(AW+1){1'b0}};
    end else begin
      if (do_push_c) begin
        q[wr_q] <= {id_ok_i, id_i};
        wr_q    <= wr_q + {{(AW-1){1'b0}}, 1'b1};
      end
      if (do_pop_c) begin
        rd_q <= rd_q + {{(AW-1){1'b0}}, 1'b1};
      end

      if (do_push_c && !do_pop_c) lvl_q <= lvl_q + {{AW{1'b0}}, 1'b1};
      else if (do_pop_c && !do_push_c) lvl_q <= lvl_q - {{AW{1'b0}}, 1'b1};

      if (push_i && full_c)  overflow_o  <= overflow_o + 32'd1;
      if (pop_i  && empty_c) underflow_o <= underflow_o + 32'd1;
      if (do_pop_c && !head_ok_c) unnamed_o <= unnamed_o + 32'd1;
    end
  end

endmodule

`default_nettype wire
