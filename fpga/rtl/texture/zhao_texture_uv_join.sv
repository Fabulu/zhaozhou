// zhao_texture_uv_join.sv — the post-PERSPUV join, Decrufter §5.3.
//
// ---------------------------------------------------------------------------
// WHAT IT IS FOR
// ---------------------------------------------------------------------------
// §5.3, verbatim: "Add one bounded, stall-safe join between PERSPUV and the
// expander. It captures {owner identity, computed U/V, saturation/zero flags as
// required, synchronous early descriptor}. Advance all parts together. Accept
// from PERSPUV only when the join has a reserved destination for its descriptor
// result. Hold the complete bundle while the expander stalls."
//
// And the sentence that is the whole point:
//
//   "Feed the expander and Mosaic with this descriptor's values, not live
//    ingress values and not another stage's current owner."
//
// Today the expander is fed a mixture: some fields travel with the fragment,
// some are read off whatever stage is currently holding them. That is the same
// class of defect as D0 -- a record assembled from parts that were never
// guaranteed to belong to each other -- and it is invisible to any counter,
// because the counts balance however the fields are paired.
//
// ---------------------------------------------------------------------------
// WHY THE ALIGNMENT IS STRUCTURAL AND NOT A TIMING ARGUMENT
// ---------------------------------------------------------------------------
// The descriptor bank's read has one cycle of latency and its output register is
// gated on an actual read (the D0 hold law). This join drives that read with
// EXACTLY the same enable that registers the PERSPUV fields:
//
//     wire accept_c = p_valid_i && room_c;
//     assign d_rd_valid_o = accept_c;     // launches the descriptor read
//     if (accept_c) begin ... end         // registers u, v, tag, flags
//
// So the descriptor output and the registered PERSPUV fields advance on the same
// clocks and cannot be from different transactions. That is the repair that
// fixed D0 used as a construction rule rather than as a patch: the record is
// atomic because one enable moves all of it, not because the latencies happen to
// line up.
//
// `room_c` is the reserved destination §5.3 asks for. Nothing is accepted from
// PERSPUV unless the output stage is empty or is being drained this cycle, so
// there is always somewhere for the descriptor answer to land.
//
// ---------------------------------------------------------------------------
// THE FORK, AND WHY MOSAIC'S READY IS NOT IGNORED
// ---------------------------------------------------------------------------
// §5.3: "Any required Mosaic acceptance must be included in the fork's
// handshake; a future functional Mosaic branch cannot drop requests merely
// because its ready was ignored in the characterization composition."
//
// A tied-off `m_ready_i` in today's composition would make an ignored ready look
// correct forever and then drop requests the day Mosaic becomes functional. So
// both branches are tracked with sticky accepted-bits and the record retires
// only when both have taken it. With `m_ready_i` tied high this costs nothing
// and behaves exactly like the expander-only fork.
module zhao_texture_uv_join #(
    parameter int unsigned TAGW  = 14,   // v3own handle {slot[5:0], gen[7:0]}
    parameter int unsigned SLOTW = 6,
    parameter int unsigned GENW  = 8,
    parameter int unsigned CTXW  = 64,
    // A depth-zero fragment is a caller bug that PERSPUV flags rather than
    // computes, so its U/V are meaningless. Whether that should also force the
    // sample count to zero is a POLICY, and policy belongs in a named knob
    // rather than in whatever the code happened to do. Default 0 preserves
    // today's behaviour: the descriptor's own count is used and the flag is
    // reported, so the island decides.
    parameter bit DZ_FORCES_ZERO_SAMPLES = 1'b0
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- from PERSPUV -------------------------------------------------------
    input  var logic               p_valid_i,
    output var logic               p_ready_o,
    input  var logic signed [31:0] p_u_i,
    input  var logic signed [31:0] p_v_i,
    input  var logic [TAGW-1:0]    p_tag_i,
    input  var logic               p_sat_i,
    input  var logic               p_dz_i,

    // ---- to the early descriptor bank --------------------------------------
    output var logic               d_rd_valid_o,
    output var logic [SLOTW-1:0]   d_rd_slot_o,
    output var logic [GENW-1:0]    d_rd_owner_gen_o,

    // ---- from the early descriptor bank, one cycle later --------------------
    input  var logic [CTXW-1:0]    d_aux_context_i,
    input  var logic [7:0]         d_lod_i,
    input  var logic [1:0]         d_raw_class_i,
    input  var logic               d_needs_aux_i,
    input  var logic [1:0]         d_sample_count_i,
    input  var logic [7:0]         d_binding_sel_i,
    input  var logic [7:0]         d_mosaic_mat_a_i,
    input  var logic [7:0]         d_mosaic_mat_b_i,
    input  var logic [7:0]         d_mosaic_weight_i,
    input  var logic [GENW-1:0]    d_owner_gen_i,
    // PALETTE IDENTITY, §6's forward carriage. The bank already stores and
    // outputs this pair; before this it stopped here, and the metajoin's write
    // side went on reading `palslot_m`/`palgen_m` by owner slot instead -- a
    // sidecar lookup by an identity that may already have been recycled, which
    // is the same class of hazard D0 was. Carried WITH the request, it cannot
    // be stale by construction.
    input  var logic [1:0]         d_palette_slot_i,
    input  var logic [GENW-1:0]    d_palette_gen_i,

    // ---- to the fragment expander ------------------------------------------
    output var logic               f_valid_o,
    input  var logic               f_ready_i,
    output var logic [13:0]        f_owner_o,
    output var logic signed [31:0] f_u_o,
    output var logic signed [31:0] f_v_o,
    output var logic [7:0]         f_binding_o,
    output var logic [7:0]         f_lod_o,
    output var logic [1:0]         f_count_o,
    output var logic               f_aux_o,
    output var logic [1:0]         f_class_o,
    output var logic [CTXW-1:0]    f_ctx_o,
    // §5.3 has the join capture "saturation/zero flags as required". They are
    // captured AND exposed: a flag held in a register that nothing can read is
    // precisely the dead field the brief says to eliminate rather than carry,
    // and the choice of whether to act on them belongs to the island.
    output var logic               f_sat_o,
    output var logic               f_depth_zero_o,
    output var logic [1:0]         f_pal_slot_o,
    output var logic [GENW-1:0]    f_pal_gen_o,

    // ---- to Mosaic ----------------------------------------------------------
    output var logic               m_valid_o,
    input  var logic               m_ready_i,
    output var logic [7:0]         m_mat_a_o,
    output var logic [7:0]         m_mat_b_o,
    output var logic [7:0]         m_weight_o,

    // ---- instruments --------------------------------------------------------
    output var logic [31:0]        joined_o,
    output var logic [31:0]        saturated_o,
    output var logic [31:0]        depth_zero_o,
    output var logic [31:0]        gen_mismatch_o
);

  // ---- the output record ---------------------------------------------------
  logic                r_v_q;
  logic signed [31:0]  r_u_q, r_v_val_q;
  logic [TAGW-1:0]     r_tag_q;
  logic                r_sat_q, r_dz_q;
  logic                r_f_done_q, r_m_done_q;

  // A record retires when BOTH branches have taken it. `*_done_q` are sticky so
  // a branch that accepted on an earlier cycle is not offered the same record
  // again -- which is the "counters see what pictures cannot" failure: a
  // re-offered request produces identical output and doubles the work.
  wire f_fire_c   = f_valid_o && f_ready_i;
  wire m_fire_c   = m_valid_o && m_ready_i;
  wire f_taken_c  = r_f_done_q || f_fire_c;
  wire m_taken_c  = r_m_done_q || m_fire_c;
  wire retire_c   = r_v_q && f_taken_c && m_taken_c;

  // THE RESERVED DESTINATION. Accept from PERSPUV only when the output stage is
  // empty, or is retiring this very cycle -- so the descriptor answer arriving
  // next cycle always has somewhere to land.
  wire room_c   = !r_v_q || retire_c;
  wire accept_c = p_valid_i && room_c;

  assign p_ready_o = room_c;

  // ONE ENABLE MOVES EVERYTHING. The bank's read register is gated on
  // `d_rd_valid_o`, so its output advances on exactly the clocks these
  // registers do.
  assign d_rd_valid_o     = accept_c;
  assign d_rd_slot_o      = p_tag_i[TAGW-1 -: SLOTW];
  assign d_rd_owner_gen_o = p_tag_i[GENW-1:0];

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      r_v_q          <= 1'b0;
      r_f_done_q     <= 1'b0;
      r_m_done_q     <= 1'b0;
      joined_o       <= 32'd0;
      saturated_o    <= 32'd0;
      depth_zero_o   <= 32'd0;
      gen_mismatch_o <= 32'd0;
    end else begin
      if (accept_c) begin
        r_v_q      <= 1'b1;
        r_u_q      <= p_u_i;
        r_v_val_q  <= p_v_i;
        r_tag_q    <= p_tag_i;
        r_sat_q    <= p_sat_i;
        r_dz_q     <= p_dz_i;
        r_f_done_q <= 1'b0;
        r_m_done_q <= 1'b0;
        joined_o   <= joined_o + 32'd1;
        if (p_sat_i) saturated_o  <= saturated_o + 32'd1;
        if (p_dz_i)  depth_zero_o <= depth_zero_o + 32'd1;
      end else if (retire_c) begin
        r_v_q      <= 1'b0;
        r_f_done_q <= 1'b0;
        r_m_done_q <= 1'b0;
      end else begin
        // Partially taken: remember which branch is satisfied.
        if (f_fire_c) r_f_done_q <= 1'b1;
        if (m_fire_c) r_m_done_q <= 1'b1;
      end

      // The identity carried by the token must be the identity the bank has for
      // that slot. A mismatch means the owner was recycled while this
      // transaction was in flight -- §5.5's window, reported and not enforced,
      // because the brief refuses a lease on suspicion: "prove the window or
      // reproduce it".
      //
      // Counted ONCE PER RECORD, at retirement. The obvious `if (r_v_q && ...)`
      // is a per-CYCLE test, so a single mismatched record stalled for nine
      // cycles reports nine events -- an instrument whose reading depends on
      // downstream backpressure rather than on what happened. That is the
      // counters-see-what-pictures-cannot law pointed at a counter.
      if (retire_c && (d_owner_gen_i != r_tag_q[GENW-1:0]))
        gen_mismatch_o <= gen_mismatch_o + 32'd1;
    end
  end

  // ---- outputs: every field from the CAPTURED descriptor -------------------
  // `f_binding_o`, `f_lod_o`, `f_count_o`, `f_aux_o`, `f_class_o` and `f_ctx_o`
  // all come from the bank's held output, which belongs to `r_tag_q` by
  // construction. None of them is read from a live ingress port or from another
  // stage's current owner, which is §5.3's requirement and the thing that made
  // the old arrangement unauditable.
  assign f_valid_o   = r_v_q && !r_f_done_q;
  assign f_owner_o   = r_tag_q[13:0];
  assign f_u_o       = r_u_q;
  assign f_v_o       = r_v_val_q;
  assign f_binding_o = d_binding_sel_i;
  assign f_lod_o     = d_lod_i;
  assign f_aux_o     = d_needs_aux_i;
  assign f_ctx_o     = d_aux_context_i;

  // Raw class is sanitized ONLY from the captured descriptor -- §5.3 again. All
  // four encodings are legal today, so this is a pass-through; the point is the
  // SOURCE, and stating it here means a future sanitization rule has one place
  // to live instead of being applied to whichever copy the author had at hand.
  assign f_class_o   = d_raw_class_i;

  assign f_sat_o        = r_sat_q;
  assign f_depth_zero_o = r_dz_q;

  // From the bank's HELD output, exactly like `f_lod_o` and for the same
  // reason: that output is the captured record for `r_tag_q`, held by the D0
  // gate, so this pair belongs to this fragment and not to whichever owner the
  // sidecar table happens to hold now.
  assign f_pal_slot_o   = d_palette_slot_i;
  assign f_pal_gen_o    = d_palette_gen_i;

  // The depth-zero policy, behind its knob rather than baked in.
  assign f_count_o   = (DZ_FORCES_ZERO_SAMPLES && r_dz_q) ? 2'd0 : d_sample_count_i;

  assign m_valid_o   = r_v_q && !r_m_done_q;
  assign m_mat_a_o   = d_mosaic_mat_a_i;
  assign m_mat_b_o   = d_mosaic_mat_b_i;
  assign m_weight_o  = d_mosaic_weight_i;

endmodule
