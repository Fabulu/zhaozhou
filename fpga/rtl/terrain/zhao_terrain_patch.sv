// zhao_terrain_patch.sv — TERRAIN.PATCH: the Mantle entry point (phase 6,
// ZH-033).
//
// Law, in citation order:
//   design/contracts/TERRAIN.PATCH.md — the block contract.
//   design/blocks.yml — `inputs: [dispatch, field_results, baked_scars]`,
//       `outputs: [patch_state, subpatch_requests]`, `latency: variable`,
//       "1 patch-layer update per clock", counter `terrain_samples_evaluated`,
//       and the note "Overflow policy is reject, never silently drop (§11.4)".
//   spec/terrain_rules.md §3.4 — the composition chain and its TWO clamps,
//       reproduced here verbatim.
//   spec/terrain_rules.md §9.1 — MAX_PATCH_FIELDS = 16, frozen 2026-08-16 with
//       the 8-wizard donor worst case (8 held Erupts + 8 Quakes) as the sizing
//       floor, and the exact overflow law: append in command order, reject the
//       tail, NEVER evict, count in `programs_rejected`, emit a trace event.
//   spec/qformats.md §2/§9 — height16 -> fx16 is an EXACT `raw << 8`;
//       §3 — `fx_add` saturates; one rounding per result (there is no rounding
//       at all in this block, which is itself worth stating).
//   charter §11.4 — bounded field evaluation; PRIORITY LIVES ABOVE THE SEAM.
//   reference/src/zrender/terrain.cpp `compose_lattice` — the ratified chain.
//   reference/include/zref/zref_terrain_patch.hpp — the oracle, a thin view
//       onto that chain, cross-checked against it over a real 33x33 patch.
//
// ---------------------------------------------------------------------------
// THE COMPOSITION, AND WHY IT IS SAFE TO TRANSPOSE THE LOOPS
// ---------------------------------------------------------------------------
// §3.4 is:
//     compose_top = max( fx(base) + fx(scar),  fx(bottom) )
//     live_top    = max( compose_top + SUM field height lanes (command order,
//                        fx_add chain),  fx(bottom) )
//
// `compose_lattice` walks apps OUTER and vertices INNER, mutating one lattice
// in place. This block walks vertices OUTER and lanes INNER, which needs no
// lattice-sized accumulator at all. The two agree BIT-FOR-BIT, and the reason
// is worth stating because `fx_add` saturates and is therefore order-dependent:
// the order that can change a result is the order of the adds AT ONE VERTEX,
// and both forms apply the lanes to a given vertex in command order. Nothing
// about the transpose changes a single add's operands.
//
// The cost of the transpose is an INTERFACE REQUIREMENT, recorded as a chosen
// law below: FIELD.SEQ.EARTH must deliver its height lanes vertex-major.
//
// ---------------------------------------------------------------------------
// WIDTHS, STATED RATHER THAN ASSUMED
// ---------------------------------------------------------------------------
// base/scar/bottom are height16 (signed 16, S 1.7.8 metres). The up-conversion
// is `<< 8` into fx16, so each is signed 24 — EXACT, no rounding, no possible
// saturation (qformats §9 says so, and 2^23 << the fx16 word).
//   fx(base) + fx(scar) therefore needs signed 25 and cannot saturate either;
// the saturating add is still written faithfully because the FIELD lanes are
// full fx16 words and those absolutely can saturate. Each add is done at 33
// bits and narrowed with the §3 saturate, one add at a time — never a wide
// accumulate then one narrow, which would silently disagree with the reference
// wherever a partial sum leaves the word.
//
// VERILOG SIGNEDNESS. Every comparison here is between two signed operands. A
// Verilog comparison goes unsigned if EITHER operand is; that trap cost a real
// bug in GEOM.BINNER and made 29 tiles vanish (design/contracts/GEOM.CLIP.md).
//
// ---------------------------------------------------------------------------
// LAWS CHOSEN, NOT FOUND (each also argued in the contract)
// ---------------------------------------------------------------------------
// 1. FIELD RESULTS ARRIVE VERTEX-MAJOR, one per ACCEPTED lane per vertex, in
//    list order. The reference composes lane-major; a lane-major hardware
//    intake would need the whole 33x33 fx16 accumulator resident (1,089 x 32 b
//    = 4.25 KiB, five M10Ks) because a lane's pass must be applied to every
//    vertex before the next lane's. Vertex-major needs O(1) state. REJECTED
//    ALTERNATIVE: the lane-major form with the resident accumulator — it is
//    what §4.2's composed-height CACHE looks like, and if a later increment
//    puts that cache inside this block the intake can be turned around without
//    changing the arithmetic. Recorded, not hidden: this is a requirement this
//    block imposes on FIELD.SEQ.EARTH, whose contract is still a stub.
// 2. THE FOOTPRINT TEST LIVES HERE, not upstream. Every field result offered
//    for a vertex is gated by its lane's CLOSED-interval footprint rectangle,
//    exactly as compose_lattice gates it. A lane whose footprint misses the
//    vertex is consumed and DISCARDED, not added as zero — identical in value
//    and identical in saturation records. REJECTED ALTERNATIVE: trusting the
//    upstream to send only covering lanes, which would make the per-vertex
//    result count data-dependent and would move a ratified law out of the
//    block that owns composition.
// 3. THE SUBPATCH DIRTY MASK is `live_top != fx(base)` — the ground actually
//    moved — with border vertices marking BOTH neighbours (they are physically
//    shared, the same closed-interval reasoning §9.1 uses for binning; a corner
//    vertex marks four). REJECTED ALTERNATIVE: marking from the field
//    footprint rectangles alone. That is cheaper and wrong: a crater's
//    bounding rectangle is "dirty" in its corners where the field evaluates to
//    exactly zero, so it marks subpatches whose ground did not move and
//    defeats terrain_rules §4.4's "dirty patches only" entirely.
// 4. `programs_rejected_o` AND THE TRACE EVENT SURVIVE `list_clear_i`. The
//    ledger's `counters:` line for this block names only
//    `terrain_samples_evaluated`, but §9.1 law 2 names `programs_rejected` and
//    a trace event explicitly, so both are exposed. The counter is a
//    frame-life diagnostic across every patch and only reset clears it; the
//    per-patch LIST is what `list_clear_i` empties. REJECTED ALTERNATIVE:
//    clearing the counter per patch, which would make the frame total
//    unobtainable without summing pulses.
//
// NOT IN THIS BLOCK, deliberately: no VRAM port, no page loader, no page CRC,
// no scar writing, no breach law (TERRAIN.BAKE owns layers B and D), no field
// evaluation (FIELD.SEQ.EARTH; terrain_rules §4.1 forbids a second evaluator),
// no velocity lattice (TERRAIN.VELOCITY, phase 7), and no `age`/`phase`/
// `start_tick` gating (dispatch-side, and visible in compose_lattice's loop).
//
// Conservative SystemVerilog subset only (charter §2).

module zhao_terrain_patch (
    input logic clk,
    input logic rst_n,

    // -----------------------------------------------------------------------
    // dispatch: the §9.1 bounded live-field intake
    // -----------------------------------------------------------------------
    // Empties the per-patch list and the subpatch dirty mask. Asserted once per
    // patch per frame, before the first record. Does NOT clear
    // programs_rejected_o (chosen law 4).
    input logic        list_clear_i,
    input logic [15:0] patch_id_i,  // rides the reject trace event

    input  logic               fld_add_valid_i,
    output logic               fld_add_ready_o,
    input  logic signed [31:0] fld_add_x0_i,    // footprint, fx16 raw, CLOSED
    input  logic signed [31:0] fld_add_z0_i,
    input  logic signed [31:0] fld_add_x1_i,
    input  logic signed [31:0] fld_add_z1_i,
    input  logic        [31:0] fld_add_hash_i,  // program hash, trace only
    input  logic        [15:0] fld_add_cmd_i,   // command index, trace only

    output logic       fld_add_accept_o,  // 1-cycle pulse
    output logic       fld_add_reject_o,  // 1-cycle pulse, with the trace event
    output logic [4:0] fields_active_o,   // 0..16

    output logic [15:0] trace_patch_id_o,
    output logic [31:0] trace_hash_o,
    output logic [15:0] trace_cmd_o,
    output logic [31:0] programs_rejected_o,

    // -----------------------------------------------------------------------
    // the compose lane: one lattice vertex in, one patch_state record out
    // -----------------------------------------------------------------------
    input  logic               vtx_valid_i,
    output logic               vtx_ready_o,
    input  logic signed [15:0] base_i,       // layer A, height16
    input  logic signed [15:0] scar_i,       // layer B, height16
    input  logic signed [15:0] bottom_i,     // layer C, height16
    input  logic               dual_i,       // 0 = legacy single-surface page
    input  logic signed [31:0] wx_i,         // placed world x, fx16 raw
    input  logic signed [31:0] wz_i,         // placed world z, fx16 raw
    input  logic        [ 5:0] vi_i,         // lattice column, 0..32
    input  logic        [ 5:0] vj_i,         // lattice row, 0..32
    input  logic        [15:0] src_id_i,

    // field_results: one height lane per ACCEPTED list entry per vertex, in
    // list order (chosen law 1).
    input  logic               fld_valid_i,
    output logic               fld_ready_o,
    input  logic signed [31:0] fld_height_i,

    // The §9.1 CLOSED-interval footprint answer for the lane currently being
    // offered, valid in the cycle `fld_valid_i && fld_ready_o`. This block owns
    // the §9.1 list (chosen law 2: the footprint test lives HERE), so it is the
    // one place the test is decided; TERRAIN.VELOCITY consumes the same
    // per-vertex lane stream for out-lane 1 and takes this answer rather than
    // holding a second 16-rectangle list and re-deciding a ratified law
    // (charter §29-6). Exported 2026-08-19 with TERRAIN.VELOCITY; purely
    // additive, no internal behaviour changed.
    output logic               fld_covers_o,

    // -----------------------------------------------------------------------
    // patch_state out
    // -----------------------------------------------------------------------
    output logic               st_valid_o,
    input  logic               st_ready_i,
    output logic signed [31:0] top_o,          // live_top, fx16
    output logic signed [31:0] bottom_o,       // fx(bottom), or live_top legacy
    output logic signed [31:0] compose_top_o,  // pre-field, post-clamp
    output logic               st_dirty_o,     // this vertex moved
    output logic        [15:0] st_src_id_o,

    // subpatch_requests: the 4x4 dirty mask, bit row*4 + col (charter §11.1)
    output logic [15:0] subpatch_dirty_o,

    output logic [31:0] terrain_samples_evaluated_o,
    output logic        idle_o
);

  // ---- MAX_PATCH_FIELDS, frozen (terrain_rules §9.1) -----------------------
  localparam int unsigned MaxFields = 16;

  // ---- functions -----------------------------------------------------------

  // §3 saturating fx16 add: one add at 33 bits, then narrow with saturation.
  function automatic logic signed [31:0] fx_add_sat(input logic signed [31:0] a,
                                                    input logic signed [31:0] b);
    logic signed [32:0] s;
    begin
      s = $signed({a[31], a}) + $signed({b[31], b});
      if (s > 33'sd2147483647) fx_add_sat = 32'sh7FFF_FFFF;
      else if (s < -33'sd2147483648) fx_add_sat = 32'sh8000_0000;
      else fx_add_sat = s[31:0];
    end
  endfunction

  // The 4x4 subpatch mask of one lattice vertex (chosen law 3). A vertex on a
  // subpatch border marks both neighbours; a corner vertex marks four.
  function automatic logic [15:0] sp_mask(input logic [5:0] vi, input logic [5:0] vj);
    logic [5:0] col_lo, col_hi, row_lo, row_hi;
    logic [15:0] m;
    begin
      col_lo = (vi == 6'd0) ? 6'd0 : ((vi - 6'd1) >> 3);
      col_hi = ((vi >> 3) > 6'd3) ? 6'd3 : (vi >> 3);
      row_lo = (vj == 6'd0) ? 6'd0 : ((vj - 6'd1) >> 3);
      row_hi = ((vj >> 3) > 6'd3) ? 6'd3 : (vj >> 3);
      m = 16'd0;
      for (int r = 0; r < 4; r++) begin
        for (int c = 0; c < 4; c++) begin
          if (r >= int'(row_lo) && r <= int'(row_hi) && c >= int'(col_lo) && c <= int'(col_hi))
            m[r*4+c] = 1'b1;
        end
      end
      sp_mask = m;
    end
  endfunction

  // -------------------------------------------------------------------------
  // the field list: 16 footprint rectangles. The program hash and command
  // index are NOT stored — they are only ever needed by a reject's trace
  // event, which is emitted in the same cycle the reject is decided.
  // -------------------------------------------------------------------------
  logic signed [31:0] fp_x0[MaxFields];
  logic signed [31:0] fp_z0[MaxFields];
  logic signed [31:0] fp_x1[MaxFields];
  logic signed [31:0] fp_z1[MaxFields];
  logic        [ 4:0] n_fields;

  // The intake never stalls: a record is accepted or rejected in the cycle it
  // is offered. §9.1's "the frame never stalls" is a property of this line.
  assign fld_add_ready_o = 1'b1;
  assign fields_active_o = n_fields;

  wire list_full = (n_fields >= 5'(MaxFields));

  // -------------------------------------------------------------------------
  // the compose lane
  // -------------------------------------------------------------------------
  logic               busy;  // collecting field results for the held vertex
  logic        [ 4:0] lane;  // how many results have been consumed
  logic signed [31:0] acc;  // the running fx_add chain
  logic signed [31:0] held_ctop;
  logic signed [31:0] held_bot;
  logic signed [31:0] held_base;
  logic               held_dual;
  logic signed [31:0] held_wx;
  logic signed [31:0] held_wz;
  logic        [15:0] held_src;
  logic        [15:0] held_mask;

  // stage R: the completed composition, which is also the output register.
  logic               r_valid;
  logic signed [31:0] r_top;
  logic signed [31:0] r_bot;
  logic signed [31:0] r_ctop;
  logic               r_dirty;
  logic        [15:0] r_src;

  wire out_free = !r_valid || st_ready_i;

  // The footprint test of the lane whose result is currently offered. This is
  // compose_lattice's test verbatim, on a CLOSED interval: a vertex exactly on
  // a footprint edge is INSIDE (terrain_rules §9.1's closed-interval rule).
  wire signed [31:0] cur_x0 = fp_x0[lane[3:0]];
  wire signed [31:0] cur_z0 = fp_z0[lane[3:0]];
  wire signed [31:0] cur_x1 = fp_x1[lane[3:0]];
  wire signed [31:0] cur_z1 = fp_z1[lane[3:0]];
  wire cur_covers = !((held_wx < cur_x0) || (held_wx > cur_x1) ||
                      (held_wz < cur_z0) || (held_wz > cur_z1));

  wire last_lane = (lane + 5'd1) == n_fields;

  // A vertex is taken only when the result register can receive its answer.
  //
  // INVARIANT (the reason `fld_ready_o` needs no stall term of its own, and it
  // is stated because the first version of this block carried an UNREACHABLE
  // one and the directed suite caught that the branch could never fire):
  // `out_free` at the accept cycle T means `!r_valid_T || st_ready_T`. In the
  // first case r_valid is already 0; in the second the retire fires, so
  // r_valid_{T+1} = r_valid_T && !st_ready_T = 0. Either way r_valid is 0 the
  // cycle after an accept, and nothing raises it again until this vertex's own
  // chain completes. The result register is therefore ALWAYS free when the last
  // lane lands, so the field stream never has to be stalled by the output.
  // `terrain_patch_directed` case 6(b) asserts the invariant directly rather
  // than trusting this paragraph.
  assign vtx_ready_o = !busy && out_free;
  assign fld_ready_o = busy;
  assign fld_covers_o = cur_covers;

  // ---- the vertex's own composition, before any field lane ----------------
  // height16 -> fx16 is the EXACT `raw << 8` (qformats §9): sign-extend the
  // 16-bit word to 24 bits of fx16 raw. No rounding exists here.
  wire signed [31:0] base_fx = {{8{base_i[15]}}, base_i, 8'b0};
  wire signed [31:0] scar_fx = {{8{scar_i[15]}}, scar_i, 8'b0};
  wire signed [31:0] bot_fx = {{8{bottom_i[15]}}, bottom_i, 8'b0};
  wire signed [31:0] sum_bs = fx_add_sat(base_fx, scar_fx);
  // clamp at the underside (§3.4 line 1); a legacy page has no underside
  wire signed [31:0] ctop_new = (dual_i && (sum_bs < bot_fx)) ? bot_fx : sum_bs;

  // With no live field the chain is empty, so §3.4 line 2's clamp acts on
  // compose_top itself — which line 1 already clamped, so it is a no-op. It is
  // written out rather than elided so the two lines stay visibly separate.
  wire signed [31:0] ctop_clamped = (dual_i && (ctop_new < bot_fx)) ? bot_fx : ctop_new;

  // ---- the field chain, one lane at a time --------------------------------
  // A lane whose footprint misses this vertex is CONSUMED and discarded,
  // exactly as compose_lattice `continue`s past it — identical in value and
  // identical in saturation records.
  wire signed [31:0] acc_next = cur_covers ? fx_add_sat(acc, fld_height_i) : acc;
  // live_top = max(compose_top + fields, bottom): the ONE clamp after the whole
  // command-order fx_add chain (§3.4 line 2). A transient wave can never punch
  // below the underside, so it can never fake a breach.
  wire signed [31:0] acc_fin = (held_dual && (acc_next < held_bot)) ? held_bot : acc_next;

  // Which list slot an offered record lands in. A clear and an offer in the
  // same cycle put the record in slot 0 of the fresh list (the frame-start
  // case), which is why this is not simply n_fields.
  wire [3:0] add_slot = list_clear_i ? 4'd0 : n_fields[3:0];

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      for (int k = 0; k < int'(MaxFields); k++) begin
        fp_x0[k] <= '0;
        fp_z0[k] <= '0;
        fp_x1[k] <= '0;
        fp_z1[k] <= '0;
      end
      n_fields <= '0;
      fld_add_accept_o <= 1'b0;
      fld_add_reject_o <= 1'b0;
      trace_patch_id_o <= '0;
      trace_hash_o <= '0;
      trace_cmd_o <= '0;
      programs_rejected_o <= '0;
      busy <= 1'b0;
      lane <= '0;
      acc <= '0;
      held_ctop <= '0;
      held_bot <= '0;
      held_base <= '0;
      held_dual <= 1'b0;
      held_wx <= '0;
      held_wz <= '0;
      held_src <= '0;
      held_mask <= '0;
      r_valid <= 1'b0;
      r_top <= '0;
      r_bot <= '0;
      r_ctop <= '0;
      r_dirty <= 1'b0;
      r_src <= '0;
      subpatch_dirty_o <= '0;
      terrain_samples_evaluated_o <= '0;
    end else begin
      fld_add_accept_o <= 1'b0;
      fld_add_reject_o <= 1'b0;

      // ---- the §9.1 intake ------------------------------------------------
      // list_clear_i empties the per-patch list and the dirty mask. It is
      // ordered BEFORE the offer below so a clear and an offer in the same
      // cycle admit the record into the fresh list — the frame-start case.
      if (list_clear_i) begin
        n_fields <= '0;
        subpatch_dirty_o <= '0;
      end

      if (fld_add_valid_i) begin
        // The FIRST MaxFields records in command order win, every run,
        // identically. Nothing already listed is ever evicted, and there is no
        // priority notion in this block (charter §11.4: priority is software's,
        // above the seam).
        if (list_full && !list_clear_i) begin
          fld_add_reject_o <= 1'b1;
          trace_patch_id_o <= patch_id_i;
          trace_hash_o <= fld_add_hash_i;
          trace_cmd_o <= fld_add_cmd_i;
          programs_rejected_o <= programs_rejected_o + 32'd1;
        end else begin
          fp_x0[add_slot] <= fld_add_x0_i;
          fp_z0[add_slot] <= fld_add_z0_i;
          fp_x1[add_slot] <= fld_add_x1_i;
          fp_z1[add_slot] <= fld_add_z1_i;
          n_fields <= list_clear_i ? 5'd1 : (n_fields + 5'd1);
          fld_add_accept_o <= 1'b1;
        end
      end

      // ---- publish / retire ------------------------------------------------
      if (r_valid && st_ready_i) r_valid <= 1'b0;

      // ---- the compose lane ------------------------------------------------
      if (!busy) begin
        if (vtx_valid_i && out_free) begin
          held_ctop <= ctop_new;
          held_bot  <= bot_fx;
          held_base <= base_fx;
          held_dual <= dual_i;
          held_wx   <= wx_i;
          held_wz   <= wz_i;
          held_src  <= src_id_i;
          held_mask <= sp_mask(vi_i, vj_i);
          if (n_fields == 5'd0) begin
            // No live field touches this patch: the vertex composes in one
            // cycle, which is the ledger's "1 patch-layer update per clock".
            r_valid <= 1'b1;
            r_top <= ctop_clamped;
            r_bot <= dual_i ? bot_fx : ctop_clamped;
            r_ctop <= ctop_new;
            r_dirty <= ctop_clamped != base_fx;
            r_src <= src_id_i;
            if (ctop_clamped != base_fx) subpatch_dirty_o <= subpatch_dirty_o | sp_mask(vi_i, vj_i);
            terrain_samples_evaluated_o <= terrain_samples_evaluated_o + 32'd1;
          end else begin
            busy <= 1'b1;
            lane <= '0;
            acc  <= ctop_new;
          end
        end
      end else if (fld_valid_i && fld_ready_o) begin
        if (last_lane) begin
          busy <= 1'b0;
          r_valid <= 1'b1;
          r_top <= acc_fin;
          r_bot <= held_dual ? held_bot : acc_fin;
          r_ctop <= held_ctop;
          r_dirty <= acc_fin != held_base;
          r_src <= held_src;
          if (acc_fin != held_base) subpatch_dirty_o <= subpatch_dirty_o | held_mask;
          terrain_samples_evaluated_o <= terrain_samples_evaluated_o + 32'd1;
        end else begin
          acc  <= acc_next;
          lane <= lane + 5'd1;
        end
      end
    end
  end

  assign st_valid_o    = r_valid;
  assign top_o         = r_top;
  assign bottom_o      = r_bot;
  assign compose_top_o = r_ctop;
  assign st_dirty_o    = r_dirty;
  assign st_src_id_o   = r_src;
  assign idle_o        = !busy && !r_valid;

endmodule : zhao_terrain_patch
