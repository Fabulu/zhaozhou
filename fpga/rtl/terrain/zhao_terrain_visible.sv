// zhao_terrain_visible.sv - which patches the camera can see.
//
// Law: reference/include/zref/zref_island.hpp  (zref::island::visible_set)
//      design/contracts/TERRAIN.VISIBLE.md
//      design/contracts/TERRAIN.ISLAND.md     (the block this one composes)
//
// ===========================================================================
// THE HOLE THIS FILLS, IN THE OWNER'S OWN WORDS
// ===========================================================================
// reports/Missingterrain, on why the shipped world is "a little spot":
//
//     "Nothing currently does: camera moved -> inspect island directory ->
//      determine visible patch coordinates -> ... -> issue all visible patches
//      to the terrain engine."
//
// TERRAIN.ISLAND answers ONE question about ONE patch. What was missing is the
// thing that decides WHICH questions to ask. That is this block, and it is
// deliberately nothing more: a coordinate generator and a filter.
//
// It does NOT re-implement the extent test, the pitch test or the sky mapping.
// Those are TERRAIN.ISLAND's, it instantiates TERRAIN.ISLAND, and it believes
// whatever comes back. A second copy of the gates here would be a second thing
// to keep in step with `Directory::find`, and the first divergence would be
// invisible: a window that quietly omitted a patch draws slightly less ground
// on a grid where drawing nothing is the correct answer 94.9% of the time.
//
// ===========================================================================
// THE WINDOW IS SQUARE, AND THAT IS A DECISION
// ===========================================================================
// `zref::island::View` is a centre and a RADIUS IN PATCHES, and the window it
// describes is the square [cx-r, cx+r] x [cz-r, cz+r] -- not the disc.
//
// The visible set is a conservative SUPERSET of what is drawn. A residency
// policy exact about the frustum evicts patches the moment the camera turns and
// re-fetches them on the next turn, and page traffic is the expensive thing on
// this machine, not comparator area. Hysteresis belongs in the VIEW, not in the
// draw path. Rounding the corners off would drop ~21% of the queries and buy a
// thrash. This is not an optimisation left on the table; it is the shape the
// reference chose and the RTL matches it.
//
// ===========================================================================
// SKY IS THE COMMON CASE, SO REJECTION IS WHAT THIS BLOCK COSTS
// ===========================================================================
// An 8 km island at the canonical 2.0 m pitch is 125 x 125 = 15,625 patches, of
// which 793 are ground. A window of radius R asks (2R+1)^2 questions and
// answers a small fraction of them with a patch. So the throughput number that
// matters is the cost per REJECTED patch, not per emitted one, and this block
// is built to keep TERRAIN.ISLAND saturated rather than to hurry the hits:
//
//   * queries are ISSUED from one cursor and RETIRED against a second cursor
//     that walks the identical order, so the next query is presented on the
//     same cycle the directory frees its input -- no request/response
//     round-trip bubble is added on top of the directory's own;
//   * an OPEN SKY answer never touches the emit register, so a fully sky window
//     runs at exactly the directory's acceptance rate;
//   * emission backpressure stalls the retire side, which stalls the directory,
//     which stalls issue. A downstream that cannot keep up costs throughput and
//     never costs correctness.
//
// Measured on the modelled store in tests/terrain/tb_island_visible.sv, with
// the directory's own three-cycle in-extent round trip: see
// design/contracts/TERRAIN.VISIBLE.md for the figure this bench reports.
//
// ===========================================================================
// TWO CURSORS, NOT A FIFO
// ===========================================================================
// Something has to remember which coordinate each answer belongs to. A queue of
// in-flight coordinates would work and would cost a depth argument nobody can
// discharge without knowing the directory's pipeline. Instead the retire side
// keeps its OWN copy of the scan counters, advanced by one on each answer.
// Because TERRAIN.ISLAND answers strictly in order, the retire cursor is by
// construction the coordinate of the answer arriving -- no storage per
// in-flight query, and no depth to get wrong when the directory is pipelined
// deeper later.
//
// The claim is CHECKED rather than assumed: every query carries a rolling tag
// and `err_tag_o` latches if an answer ever arrives carrying a tag the retire
// cursor did not expect.
// ENFORCED-BY: tests/terrain/visible_rtl_directed.cpp A silently misaligned cursor would attach the right
// handle to the wrong coordinate, which is a wrong patch drawn in the right
// place -- and nothing downstream could tell.
//
// Conservative SystemVerilog subset only (charter section 2).
module zhao_terrain_visible
  import zhao_pkg::*;
(
    input  var logic clk,
    input  var logic rst_n,

    // ---- the island descriptor, frame-scoped ------------------------------
    // Passed straight through to TERRAIN.ISLAND. This block never reads them.
    input  var logic [15:0]       desc_extent_ix_i,
    input  var logic [15:0]       desc_extent_iz_i,
    input  var logic signed [7:0] desc_pitch_log2_i,

    // ---- the view in ------------------------------------------------------
    // One view in flight. Accepting a second while a window is still scanning
    // would interleave two patch streams on one output with no way to tell
    // them apart; the caller waits for `v_done_o`.
    input  var logic               v_valid_i,
    output var logic               v_ready_o,
    input  var logic signed [31:0] v_centre_ix_i,
    input  var logic signed [31:0] v_centre_iz_i,
    input  var logic [7:0]         v_radius_i,   // patches; window is 2r+1 a side

    // ---- the residency store, forwarded to the composed directory ---------
    output var logic        res_valid_o,
    input  var logic        res_ready_i,
    output var logic [15:0] res_ix_o,
    output var logic [15:0] res_iz_o,
    input  var logic        res_ans_valid_i,
    input  var logic        res_ans_hit_i,
    input  var logic [31:0] res_ans_handle_i,

    // ---- the visible patch stream out -------------------------------------
    output var logic               p_valid_o,
    input  var logic               p_ready_i,
    output var logic signed [31:0] p_ix_o,
    output var logic signed [31:0] p_iz_o,
    output var logic [31:0]        p_handle_o,

    // The window is finished AND drained. There is no `p_last_o`, and its
    // absence is deliberate: an emitted patch cannot know it is the last one
    // until an unknown number of sky cells after it have been rejected, so a
    // last-flag would need lookahead across the tail of the window. A separate
    // completion pulse costs one wire and no lookahead.
    output var logic v_done_o,
    output var logic v_busy_o,

    // ---- evidence ---------------------------------------------------------
    // `examined` is counted at ISSUE and the other four at ANSWER, so
    // `examined == emitted + sky + out_of_extent + bad_pitch` is a real
    // invariant across an idle boundary rather than one counter restated four
    // ways. A query that was asked and never answered breaks it; a test that
    // only compared outcomes could not see that at all.
    output var logic [31:0] cnt_examined_o,
    output var logic [31:0] cnt_emitted_o,
    output var logic [31:0] cnt_sky_o,
    output var logic [31:0] cnt_out_of_extent_o,
    output var logic [31:0] cnt_bad_pitch_o,

    // The composed directory's OWN counters, forwarded for observability.
    // They are its ledger, not this block's: the directory may legitimately be
    // shared with other clients later, at which point its totals stop equalling
    // this block's and the difference is the other client's traffic. Today it
    // has one client and the test asserts they agree exactly, which is how a
    // dropped or invented query would show.
    output var logic [31:0] isl_cnt_resident_o,
    output var logic [31:0] isl_cnt_open_sky_o,
    output var logic [31:0] isl_cnt_out_of_extent_o,
    output var logic [31:0] isl_cnt_bad_pitch_o,

    // Sticky: an answer arrived whose tag the retire cursor did not expect.
    output var logic err_tag_o
);

  // zref::island::Outcome, same encoding as TERRAIN.ISLAND's localparams.
  localparam logic [1:0] OUT_RESIDENT      = 2'd0;
  localparam logic [1:0] OUT_OPEN_SKY      = 2'd1;
  localparam logic [1:0] OUT_OUT_OF_EXTENT = 2'd2;
  localparam logic [1:0] OUT_BAD_PITCH     = 2'd3;

  // ---- the captured view -------------------------------------------------
  // INGRESS CAPTURE. The window bounds are formed from the view pins at the
  // one acceptance event and never read live afterwards: the scan runs for
  // thousands of cycles and the caller is entitled to change its mind about
  // where the camera is the instant the handshake completes.
  logic               active_q;
  // iz0 is not kept: the row restart only ever needs ix0.
  logic signed [31:0] ix0_q, ix1_q, iz1_q;

  wire signed [31:0] rad_c = $signed({24'd0, v_radius_i});

  // ---- the two cursors ---------------------------------------------------
  logic signed [31:0] iss_ix_q, iss_iz_q;
  logic signed [31:0] ret_ix_q, ret_iz_q;
  logic               iss_done_q, ret_done_q;
  logic [7:0]         iss_tag_q, ret_tag_q;

  // ---- the emit register -------------------------------------------------
  logic               p_full_q;
  logic signed [31:0] p_ix_q, p_iz_q;
  logic [31:0]        p_handle_q;

  assign p_valid_o  = p_full_q;
  assign p_ix_o     = p_ix_q;
  assign p_iz_o     = p_iz_q;
  assign p_handle_o = p_handle_q;

  wire p_pop_c  = p_full_q && p_ready_i;
  wire p_free_c = !p_full_q || p_pop_c;

  // ---- the composed directory --------------------------------------------
  logic        dq_valid, dq_ready;
  logic [7:0]  dq_tag;
  logic        da_valid, da_ready;
  logic [1:0]  da_outcome;
  logic [31:0] da_handle;
  logic [7:0]  da_tag;

  // Present a query whenever a window is open and the issue cursor has not run
  // off the end. The coordinate is the cursor's, which is registered state --
  // there is no path from the view pins to the directory's query port.
  assign dq_valid = active_q && !iss_done_q;
  assign dq_tag   = iss_tag_q;

  // An answer may be taken only when the emit slot could hold it. Refusing it
  // otherwise is what pushes backpressure all the way to the issue side; the
  // alternative -- taking the answer and dropping it -- is a patch that
  // silently never gets drawn.
  assign da_ready = p_free_c;

  wire iss_fire_c = dq_valid && dq_ready;
  wire ret_fire_c = da_valid && da_ready;

  wire iss_row_end_c = (iss_ix_q == ix1_q);
  wire iss_last_c    = iss_row_end_c && (iss_iz_q == iz1_q);
  wire ret_row_end_c = (ret_ix_q == ix1_q);
  wire ret_last_c    = ret_row_end_c && (ret_iz_q == iz1_q);

  // A view is accepted only between windows.
  assign v_ready_o = !active_q;
  assign v_busy_o  = active_q;

  // The window is finished when the last query has been issued, its answer
  // retired, and the emit register drained -- so `v_done_o` cannot arrive while
  // a patch is still sitting on the output waiting for a slow consumer.
  wire window_end_c = active_q && iss_done_q && ret_done_q && !p_full_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      active_q   <= 1'b0;
      ix0_q      <= 32'sd0;
      ix1_q      <= 32'sd0;
      iz1_q      <= 32'sd0;
      iss_ix_q   <= 32'sd0;
      iss_iz_q   <= 32'sd0;
      ret_ix_q   <= 32'sd0;
      ret_iz_q   <= 32'sd0;
      iss_done_q <= 1'b0;
      ret_done_q <= 1'b0;
      iss_tag_q  <= 8'd0;
      ret_tag_q  <= 8'd0;
      p_full_q   <= 1'b0;
      p_ix_q     <= 32'sd0;
      p_iz_q     <= 32'sd0;
      p_handle_q <= 32'd0;
      v_done_o   <= 1'b0;
      err_tag_o  <= 1'b0;
      cnt_examined_o      <= 32'd0;
      cnt_emitted_o       <= 32'd0;
      cnt_sky_o           <= 32'd0;
      cnt_out_of_extent_o <= 32'd0;
      cnt_bad_pitch_o     <= 32'd0;
    end else begin
      v_done_o <= 1'b0;
      if (p_pop_c) p_full_q <= 1'b0;

      // ---- accept a view: the one ingress capture event ------------------
      if (v_valid_i && v_ready_o) begin
        ix0_q      <= v_centre_ix_i - rad_c;
        ix1_q      <= v_centre_ix_i + rad_c;
        iz1_q      <= v_centre_iz_i + rad_c;
        iss_ix_q   <= v_centre_ix_i - rad_c;
        iss_iz_q   <= v_centre_iz_i - rad_c;
        ret_ix_q   <= v_centre_ix_i - rad_c;
        ret_iz_q   <= v_centre_iz_i - rad_c;
        iss_done_q <= 1'b0;
        ret_done_q <= 1'b0;
        iss_tag_q  <= 8'd0;
        ret_tag_q  <= 8'd0;
        active_q   <= 1'b1;
      end

      // ---- issue: row-major, iz outer, ix inner --------------------------
      // The order is the reference's, and it is contractual rather than
      // incidental: `zref::island::visible_set` emits row-major and the
      // differential asserts the ORDER of the emitted list, not just its
      // membership.
      if (iss_fire_c) begin
        cnt_examined_o <= cnt_examined_o + 32'd1;
        iss_tag_q      <= iss_tag_q + 8'd1;
        if (iss_last_c) begin
          iss_done_q <= 1'b1;
        end else if (iss_row_end_c) begin
          iss_ix_q <= ix0_q;
          iss_iz_q <= iss_iz_q + 32'sd1;
        end else begin
          iss_ix_q <= iss_ix_q + 32'sd1;
        end
      end

      // ---- retire: the same walk, one answer at a time -------------------
      if (ret_fire_c) begin
        ret_tag_q <= ret_tag_q + 8'd1;
        if (da_tag != ret_tag_q) err_tag_o <= 1'b1;

        case (da_outcome)
          OUT_RESIDENT: begin
            // The ONLY place the emit register is written, and it takes the
            // coordinate from the retire cursor rather than from the issue
            // cursor -- which by then has already moved on.
            p_full_q      <= 1'b1;
            p_ix_q        <= ret_ix_q;
            p_iz_q        <= ret_iz_q;
            p_handle_q    <= da_handle;
            cnt_emitted_o <= cnt_emitted_o + 32'd1;
          end
          OUT_OPEN_SKY:      cnt_sky_o           <= cnt_sky_o + 32'd1;
          OUT_OUT_OF_EXTENT: cnt_out_of_extent_o <= cnt_out_of_extent_o + 32'd1;
          default:           cnt_bad_pitch_o     <= cnt_bad_pitch_o + 32'd1;
        endcase

        if (ret_last_c) begin
          ret_done_q <= 1'b1;
        end else if (ret_row_end_c) begin
          ret_ix_q <= ix0_q;
          ret_iz_q <= ret_iz_q + 32'sd1;
        end else begin
          ret_ix_q <= ret_ix_q + 32'sd1;
        end
      end

      // ---- close the window ----------------------------------------------
      // Evaluated after the emit write above, so a window whose LAST patch is
      // resident does not close on the cycle that patch is registered.
      if (window_end_c && !(ret_fire_c && da_outcome == OUT_RESIDENT)) begin
        active_q <= 1'b0;
        v_done_o <= 1'b1;
      end
    end
  end

  zhao_terrain_island_dir u_dir (
      .clk(clk),
      .rst_n(rst_n),
      .desc_extent_ix_i(desc_extent_ix_i),
      .desc_extent_iz_i(desc_extent_iz_i),
      .desc_pitch_log2_i(desc_pitch_log2_i),
      .q_valid_i(dq_valid),
      .q_ready_o(dq_ready),
      .q_ix_i(iss_ix_q),
      .q_iz_i(iss_iz_q),
      .q_tag_i(dq_tag),
      .res_valid_o(res_valid_o),
      .res_ready_i(res_ready_i),
      .res_ix_o(res_ix_o),
      .res_iz_o(res_iz_o),
      .res_ans_valid_i(res_ans_valid_i),
      .res_ans_hit_i(res_ans_hit_i),
      .res_ans_handle_i(res_ans_handle_i),
      .a_valid_o(da_valid),
      .a_ready_i(da_ready),
      .a_outcome_o(da_outcome),
      .a_handle_o(da_handle),
      .a_tag_o(da_tag),
      .cnt_resident_o(isl_cnt_resident_o),
      .cnt_open_sky_o(isl_cnt_open_sky_o),
      .cnt_out_of_extent_o(isl_cnt_out_of_extent_o),
      .cnt_bad_pitch_o(isl_cnt_bad_pitch_o)
  );

endmodule
