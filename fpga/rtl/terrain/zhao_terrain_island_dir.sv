// zhao_terrain_island_dir.sv - the 8 km island's sparse patch directory.
//
// Law: reference/include/zref/zref_island.hpp  (zref::island::Directory::find)
//      spec/terrain_rules.md 1.4 (the island's ground area)
//      design/contracts/TERRAIN.RESIDENCY.md (the set-associative store below)
//
// ===========================================================================
// WHY THIS BLOCK IS SMALL, AND WHY THAT IS THE POINT
// ===========================================================================
// An 8 km island at the canonical 2.0 m pitch is 125 x 125 = 15,625 patches on
// a 64 m patch grid, against a 1,024-page residency. So the grid is SPARSE by
// two orders of magnitude, and the reference model's directory is sparse for
// exactly that reason -- it is a std::map, not an array.
//
// The obvious hardware reading is "build a 15,625-entry table". That is the
// wrong answer and the arithmetic says so immediately: a dense handle store
// would be 16,384 x 32 bits = 524,288 bits = FIFTY-TWO M10Ks, for a structure
// whose occupancy peaks around 800. Fifty-two M10Ks against a 553-block device,
// to hold mostly nothing.
//
// The sparse store already exists. `zhao_terrain_residency_v2` is a 256-set,
// 4-way directory over the canonical key
// `{resource_epoch, island_id, patch_ix, patch_iz}` and is UNIT_VERIFIED. What
// it does NOT do is give its own miss a meaning.
//
// That is this block: the extent and pitch gates, and the mapping from the
// residency's hit/miss to the island's four outcomes. It composes the existing
// store rather than duplicating it, and its whole cost is a comparator tree and
// four counters.
//
// ===========================================================================
// A MISS IS NOT A FAULT (zref_island.hpp, and it is the reason this exists)
// ===========================================================================
//   "NOT A MISS. Sky is the ordinary answer for most of an island's grid, and
//    reporting it as a failure would make the normal case look like an error
//    and hide the ones that are."
//
// 793 of 15,625 patches are ground -- 5.1%. If the residency's miss were
// reported as a fault, 94.9% of every query would raise one, the counter would
// be meaningless, and a genuine eviction fault would be invisible inside it.
// So OPEN SKY is a first-class answer with its own counter, and the outcomes
// are ordered so that the cheapest disqualifying test runs first.
//
// ===========================================================================
// THE OUTCOME ORDER IS THE REFERENCE'S, NOT A CONVENIENCE
// ===========================================================================
//   pitch illegal    -> BAD_PITCH      (the descriptor is malformed)
//   outside extent   -> OUT_OF_EXTENT  (the island does not span it)
//   residency miss   -> OPEN_SKY       (it spans it and there is no ground)
//   residency hit    -> RESIDENT       (+ the page handle)
//
// `find()` tests them in exactly that sequence, and the order is load-bearing:
// a coordinate outside the extent of an island with an illegal pitch must
// report BAD_PITCH, because the descriptor cannot be trusted to say what the
// extent even is.
//
// Conservative SystemVerilog subset only (charter section 2).
module zhao_terrain_island_dir
  import zhao_pkg::*;
(
    input  var logic clk,
    input  var logic rst_n,

    // ---- the island descriptor, frame-scoped ------------------------------
    // Held for the whole frame by the caller. These are NOT per-query inputs
    // and must not be read as if they were: the extent belongs to the island,
    // not to the patch being asked about.
    input  var logic [15:0]      desc_extent_ix_i,
    input  var logic [15:0]      desc_extent_iz_i,
    input  var logic signed [7:0] desc_pitch_log2_i,

    // ---- query in ---------------------------------------------------------
    input  var logic             q_valid_i,
    output var logic             q_ready_o,
    input  var logic signed [31:0] q_ix_i,
    input  var logic signed [31:0] q_iz_i,
    input  var logic [7:0]       q_tag_i,       // returned with the answer

    // ---- the residency store, consulted only when the gates pass ----------
    output var logic             res_valid_o,
    input  var logic             res_ready_i,
    output var logic [15:0]      res_ix_o,
    output var logic [15:0]      res_iz_o,
    input  var logic             res_ans_valid_i,
    input  var logic             res_ans_hit_i,
    input  var logic [31:0]      res_ans_handle_i,

    // ---- answer out -------------------------------------------------------
    output var logic             a_valid_o,
    input  var logic             a_ready_i,
    output var logic [1:0]       a_outcome_o,   // see the localparams below
    output var logic [31:0]      a_handle_o,
    output var logic [7:0]       a_tag_o,

    // ---- evidence: one counter per outcome, mirroring zref's Ledger -------
    output var logic [31:0]      cnt_resident_o,
    output var logic [31:0]      cnt_open_sky_o,
    output var logic [31:0]      cnt_out_of_extent_o,
    output var logic [31:0]      cnt_bad_pitch_o
);

  // zref::island::Outcome, same encoding order.
  localparam logic [1:0] OUT_RESIDENT      = 2'd0;
  localparam logic [1:0] OUT_OPEN_SKY      = 2'd1;
  localparam logic [1:0] OUT_OUT_OF_EXTENT = 2'd2;
  localparam logic [1:0] OUT_BAD_PITCH     = 2'd3;

  // LEGAL PITCHES ARE -1, 0, +1 AND +2, and nothing else. The canonical 2.0 m
  // is +1, giving a 64 m patch on the 32x32-cell lattice. An out-of-range value
  // is not clamped to the nearest legal one: a descriptor that names a pitch
  // the machine does not have is malformed, and silently rounding it would put
  // an island at a scale nobody asked for.
  function automatic logic pitch_legal(input logic signed [7:0] p);
    pitch_legal = (p >= -8'sd1) && (p <= 8'sd2);
  endfunction

  // ---- the gates, combinational on the CAPTURED query --------------------
  logic              busy_q;      // a residency lookup is outstanding
  logic [7:0]        tag_q;
  logic signed [31:0] ix_q, iz_q;

  wire pitch_bad_c = !pitch_legal(desc_pitch_log2_i);

  // IN EXTENT is the reference's test verbatim, including the signed lower
  // bound. A negative coordinate is outside the island, not a large unsigned
  // one -- dropping the sign here would wrap it into the middle of the grid.
  wire in_extent_c = (ix_q >= 32'sd0) && (iz_q >= 32'sd0) &&
                     (ix_q < $signed({16'd0, desc_extent_ix_i})) &&
                     (iz_q < $signed({16'd0, desc_extent_iz_i}));

  // ---- answer register ----------------------------------------------------
  logic        ans_full_q;
  logic [1:0]  ans_outcome_q;
  logic [31:0] ans_handle_q;
  logic [7:0]  ans_tag_q;

  assign a_valid_o   = ans_full_q;
  assign a_outcome_o = ans_outcome_q;
  assign a_handle_o  = ans_handle_q;
  assign a_tag_o     = ans_tag_q;

  wire ans_pop_c  = ans_full_q && a_ready_i;
  wire ans_free_c = !ans_full_q || ans_pop_c;

  // Accept a query only when the answer slot can take its result and nothing is
  // outstanding. ONE query in flight: the residency answers in order and the
  // proof that an answer belongs to its query is then trivial. A deeper
  // pipeline is a measurement away, not a guess away.
  assign q_ready_o = ans_free_c && !busy_q && !res_valid_o;

  // The residency is consulted ONLY for a query that passed both gates -- an
  // out-of-extent coordinate must never reach a store keyed on a truncated
  // 16-bit index, where it would alias onto a real patch.
  logic res_pending_q;
  assign res_valid_o = res_pending_q;
  assign res_ix_o    = ix_q[15:0];
  assign res_iz_o    = iz_q[15:0];

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      busy_q        <= 1'b0;
      res_pending_q <= 1'b0;
      tag_q         <= 8'd0;
      ix_q          <= 32'sd0;
      iz_q          <= 32'sd0;
      ans_full_q    <= 1'b0;
      ans_outcome_q <= OUT_OPEN_SKY;
      ans_handle_q  <= 32'd0;
      ans_tag_q     <= 8'd0;
      cnt_resident_o      <= 32'd0;
      cnt_open_sky_o      <= 32'd0;
      cnt_out_of_extent_o <= 32'd0;
      cnt_bad_pitch_o     <= 32'd0;
    end else begin
      if (ans_pop_c) ans_full_q <= 1'b0;

      // ---- accept -------------------------------------------------------
      if (q_valid_i && q_ready_o) begin
        ix_q  <= q_ix_i;
        iz_q  <= q_iz_i;
        tag_q <= q_tag_i;
        busy_q <= 1'b1;
      end

      // ---- decide, one cycle later, on the CAPTURED coordinate -----------
      // The gates read ix_q/iz_q and not the live pins. The descriptor is
      // frame-scoped so reading it live is legitimate; the coordinate is not.
      if (busy_q && !res_pending_q) begin
        busy_q <= 1'b0;
        if (pitch_bad_c) begin
          ans_outcome_q <= OUT_BAD_PITCH;
          ans_handle_q  <= 32'd0;
          ans_tag_q     <= tag_q;
          ans_full_q    <= 1'b1;
          cnt_bad_pitch_o <= cnt_bad_pitch_o + 32'd1;
        end else if (!in_extent_c) begin
          ans_outcome_q <= OUT_OUT_OF_EXTENT;
          ans_handle_q  <= 32'd0;
          ans_tag_q     <= tag_q;
          ans_full_q    <= 1'b1;
          cnt_out_of_extent_o <= cnt_out_of_extent_o + 32'd1;
        end else begin
          // Both gates passed: ask the store.
          res_pending_q <= 1'b1;
        end
      end

      // ---- the store's answer, and the mapping that is the whole point ----
      if (res_pending_q && res_ready_i) begin
        res_pending_q <= 1'b0;
      end
      if (res_ans_valid_i) begin
        ans_tag_q  <= tag_q;
        ans_full_q <= 1'b1;
        if (res_ans_hit_i) begin
          ans_outcome_q <= OUT_RESIDENT;
          ans_handle_q  <= res_ans_handle_i;
          cnt_resident_o <= cnt_resident_o + 32'd1;
        end else begin
          // A MISS IS SKY. This single `else` is why the block exists: the
          // store cannot know that most of its grid is legitimately absent,
          // and a fault counter that fires on 94.9% of queries measures
          // nothing.
          ans_outcome_q <= OUT_OPEN_SKY;
          ans_handle_q  <= 32'd0;
          cnt_open_sky_o <= cnt_open_sky_o + 32'd1;
        end
      end
    end
  end

endmodule
