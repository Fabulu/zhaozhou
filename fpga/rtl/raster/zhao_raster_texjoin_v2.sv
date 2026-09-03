// zhao_raster_texjoin_v2.sv — the multi-sample fragment join.
//
// BESIDE `zhao_raster_texjoin.sv`, NOT REPLACING IT. REARCHITECTUREADVICE.md
// asks for v2 to land alongside the working block so the old one stays the
// golden implementation while this is proven. Nothing instantiates this yet.
//
// WHY IT EXISTS. v1 carries `required_mask = PRIMARY | AUX` — exactly one
// primary texture result per fragment. MATERIAL_ARCHITECTURE.md ratified
// something wider and is the senior document:
//
//     material_recipe
//     sample_count        0..3
//     binding[3]
//     uv_set[3]
//     lod[3]
//     optional blend weight
//
//     responses return {record_id, sample_index}
//
// reports/Addendum (owner, 2026-09-02) made fixing this necessary before the
// texture island is built, because sizing an II=1 sampler against the
// one-sample workload (541,640) would freeze the join around a target the
// material ruling already superseded — the three-sample profile is 1,094,600.
//
// WHAT THIS BLOCK IS NOT ALLOWED TO DECIDE. MATERIAL_ARCHITECTURE.md is
// explicit:
//
//   > Freeze the combiner's arithmetic and rounding only after checking the
//   > donor's actual combination law. Do not invent "Sacrifice-compatible"
//   > blending from the operator names alone.
//
// So the combiner here is DELIBERATELY UNFROZEN. `RECIPE_PASSTHRU` is exact and
// is the only recipe any test may currently rely on; every other opcode is
// wired to the same passthrough and flagged, so a caller that asks for one gets
// sample 0 rather than invented arithmetic. The join — allocation, issue,
// return routing, retirement — is the architecture work and is complete.
//
// ---------------------------------------------------------------------------
// THE THREE PROPERTIES THE BRIEF ASKS FOR, AND WHERE THEY LIVE
//
//   internal {slot, generation} identity     `gen_q`, checked on every return
//   returns write token-indexed entries      `rsp_slot` indexes directly
//   retirement order separate from completion `head_q` walks allocation order
//   ready depends on LOCAL STORAGE only      `f_ready_o` reads `free_cnt_q`
//
// The last one is the subtle one and it is why this can be II=1: accepting a
// fragment must never wait on the TMU's ready, only on whether a slot exists.
// ---------------------------------------------------------------------------
`default_nettype none

module zhao_raster_texjoin_v2 #(
    // 16 entries is v1's DEPTH and the brief's starting point ("Start with 16
    // entries"). Depth bounds outstanding fragments, not outstanding samples:
    // one fragment may have up to three in flight.
    parameter int unsigned DEPTH = 16,
    parameter int unsigned CTXW  = 64,
    parameter int unsigned BINDW = 8,
    parameter int unsigned LODW  = 4,
    // GENERATION WIDTH. Was 2. Ruled X5: a 2-bit generation wraps after four
    // reuses of a slot, so a return delayed longer than four reuses matches the
    // WRONG fragment and is silently accepted as correct -- worse than the
    // stale return it was there to catch. 8 bits is the ruled width; it costs
    // 6 flops a slot.
    parameter int unsigned GENW  = 8
) (
    input var logic clk,
    input var logic rst_n,

    // ---- accepted fragment (post Early-Z, post perspective) ------------------
    input  var logic                   f_valid_i,
    output var logic                   f_ready_o,
    input  var logic [1:0]             f_sample_count_i,          // 0..3
    input  var logic signed [31:0]     f_u_i        [3],          // S 15.16
    input  var logic signed [31:0]     f_v_i        [3],
    input  var logic [BINDW-1:0]       f_binding_i  [3],
    input  var logic [LODW-1:0]        f_lod_i      [3],
    input  var logic [2:0]             f_recipe_i,                // see RECIPE_*
    input  var logic [CTXW-1:0]        f_ctx_i,
    input  var logic                   f_aux_i,
    input  var logic                   f_uv_sat_i,

    // ---- to the single TMU: ONE sample request at a time, II=1 ---------------
    output var logic                   tmu_valid_o,
    input  var logic                   tmu_ready_i,
    output var logic signed [31:0]     tmu_u_o,
    output var logic signed [31:0]     tmu_v_o,
    output var logic [BINDW-1:0]       tmu_binding_o,
    output var logic [LODW-1:0]        tmu_lod_o,
    // {record_id, sample_index} — the identity the ruling names. Carried
    // through the TMU as its opaque src_id and echoed back untouched.
    output var logic [$clog2(DEPTH)-1:0] tmu_slot_o,
    output var logic [1:0]             tmu_sidx_o,
    output var logic [GENW-1:0]             tmu_gen_o,

    input  var logic                   tmu_rvalid_i,
    output var logic                   tmu_rready_o,
    input  var logic [23:0]            tmu_rgb_i,
    input  var logic [7:0]             tmu_a_i,
    input  var logic [$clog2(DEPTH)-1:0] tmu_rslot_i,
    input  var logic [1:0]             tmu_rsidx_i,
    input  var logic [GENW-1:0]             tmu_rgen_i,

    // ---- AUX, issued from the fragment context and NOT from perspective ------
    // v1's test modelled AUX as a second sampler taking perspective-correct U/V.
    // The brief says the production AUX consumes world position and an
    // envelope, so it must issue as soon as the fragment is accepted.
    output var logic                   aux_valid_o,
    input  var logic                   aux_ready_i,
    output var logic [CTXW-1:0]        aux_ctx_o,
    output var logic [$clog2(DEPTH)-1:0] aux_slot_o,
    output var logic [GENW-1:0]             aux_gen_o,
    input  var logic                   aux_rvalid_i,
    output var logic                   aux_rready_o,
    input  var logic [23:0]            aux_rgb_i,
    input  var logic [7:0]             aux_a_i,
    input  var logic [$clog2(DEPTH)-1:0] aux_rslot_i,
    input  var logic [GENW-1:0]             aux_rgen_i,

    // ---- the joined fragment, IN ALLOCATION ORDER ---------------------------
    output var logic                   o_valid_o,
    input  var logic                   o_ready_i,
    output var logic [CTXW-1:0]        o_ctx_o,
    output var logic [23:0]            o_rgb_o,
    output var logic [7:0]             o_a_o,
    output var logic [23:0]            o_aux_rgb_o,
    output var logic [7:0]             o_aux_a_o,
    output var logic                   o_has_aux_o,
    output var logic                   o_uv_sat_o,

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0]            fragments_o,
    output var logic [31:0]            samples_o,       // TMU requests issued
    output var logic [31:0]            full_clocks_o,   // no free slot
    output var logic [31:0]            id_errors_o,     // stale//bad return
    // The work queue cannot overflow BY CONSTRUCTION -- a fragment pushes at
    // most three entries and cannot be allocated without a free slot. Sticky,
    // and out loud, because "by construction" is a claim and not a proof.
    output var logic                   wq_overflow_o,
    output var logic                   id_error_o,
    // Sample 0 was returned for a recipe whose blend is not yet frozen.
    output var logic                   combiner_unfrozen_o
);

  localparam int SW = $clog2(DEPTH);

  // The combiner vocabulary MATERIAL_ARCHITECTURE.md names. Only PASSTHRU has
  // frozen arithmetic; the rest are placeholders until the donor law is read.
  localparam logic [2:0] RECIPE_PASSTHRU   = 3'd0;
  localparam logic [2:0] RECIPE_MODULATE   = 3'd1;
  localparam logic [2:0] RECIPE_MODULATE2X = 3'd2;
  localparam logic [2:0] RECIPE_LERP       = 3'd3;
  localparam logic [2:0] RECIPE_ADD_SAT    = 3'd4;
  localparam logic [2:0] RECIPE_MASK       = 3'd5;

  // ------------------------------------------------------------- entries ----
  logic            val_q  [DEPTH];
  logic [GENW-1:0] gen_q  [DEPTH];
  logic [2:0]      req_q  [DEPTH];   // sample_required_mask
  logic [2:0]      arr_q  [DEPTH];   // sample_arrived_mask
  logic [23:0]     srgb_q [DEPTH][3];
  logic [7:0]      sa_q   [DEPTH][3];
  logic [31:0]     su_q   [DEPTH][3];
  logic [31:0]     sv_q   [DEPTH][3];
  logic [BINDW-1:0] sbind_q [DEPTH][3];
  logic [LODW-1:0]  slod_q  [DEPTH][3];
  logic [2:0]      recipe_q [DEPTH];
  logic [CTXW-1:0] ctx_q  [DEPTH];
  logic            auxreq_q [DEPTH];
  logic            auxarr_q [DEPTH];
  logic [23:0]     auxrgb_q [DEPTH];
  logic [7:0]      auxa_q   [DEPTH];
  logic            sat_q  [DEPTH];

  logic [SW-1:0]   head_q, tail_q;          // allocation order == retire order
  logic [SW:0]     free_cnt_q;

  // Registered TMU request packet (X3.7).
  logic            tmu_valid_q;
  logic signed [31:0] tmu_u_q, tmu_v_q;
  logic [BINDW-1:0]   tmu_bind_q;
  logic [LODW-1:0]    tmu_lod_q;
  logic [SW-1:0]      tmu_slot_q;
  logic [1:0]         tmu_sidx_q;
  logic [GENW-1:0]    tmu_gen_q;

  // Registered AUX request packet.
  logic            aux_valid_q;
  logic [CTXW-1:0] aux_ctx_q;
  logic [SW-1:0]   aux_slot_q;
  logic [GENW-1:0] aux_gen_q;

  // Registered retirement packet (X3.7).
  logic            o_valid_q;
  logic [23:0]     o_rgb_q;
  logic [7:0]      o_a_q;
  logic [23:0]     o_auxrgb_q;
  logic [7:0]      o_auxa_q;
  logic            o_hasaux_q;
  logic [CTXW-1:0] o_ctx_q;
  logic            o_uvsat_q;
  logic            o_unfrozen_q;

  assign tmu_valid_o   = tmu_valid_q;
  assign tmu_u_o       = tmu_u_q;
  assign tmu_v_o       = tmu_v_q;
  assign tmu_binding_o = tmu_bind_q;
  assign tmu_lod_o     = tmu_lod_q;
  assign tmu_slot_o    = tmu_slot_q;
  assign tmu_sidx_o    = tmu_sidx_q;
  assign tmu_gen_o     = tmu_gen_q;

  assign aux_valid_o = aux_valid_q;
  assign aux_ctx_o   = aux_ctx_q;
  assign aux_slot_o  = aux_slot_q;
  assign aux_gen_o   = aux_gen_q;

  assign o_valid_o           = o_valid_q;
  assign o_rgb_o             = o_rgb_q;
  assign o_a_o               = o_a_q;
  assign o_aux_rgb_o         = o_auxrgb_q;
  assign o_aux_a_o           = o_auxa_q;
  assign o_has_aux_o         = o_hasaux_q;
  assign o_ctx_o             = o_ctx_q;
  assign o_uv_sat_o          = o_uvsat_q;
  assign combiner_unfrozen_o = o_unfrozen_q;

  // READY FROM LOCAL STORAGE ONLY. Never from tmu_ready_i — that is the whole
  // point of tokenising, and v1's coupling of the two is what serialises it.
  assign f_ready_o = (free_cnt_q != '0);

  // ============================================================= WORK FIFOS ==
  // MEASURED, 2026-09-03, and this replaced a scan rather than being chosen on
  // taste. This block fitted at 61.66 MHz and the setup report gave the whole
  // path, thirteen logic levels deep:
  //
  //   free_cnt_q[3] -> req_q[3][0]~6 -> req_q[15][0]_NEW66 -> Mux1~10
  //                 -> Mux8~1 -> always0~3 -> pick_v~1 -> always0~10
  //                 -> ... -> pick_slot~13_OTERM1173
  //
  //   13 logic levels, 15.679 ns of data delay, and 69% of it INTERCONNECT
  //   (10.829 ns routing against 4.850 ns of cells), with one 4.869 ns hop.
  //
  // The old code walked all sixteen entries from the retire head, and three
  // sample slots inside each, in one combinational priority chain -- and AUX
  // walked the same sixteen again. Forty-eight sequentially dependent
  // comparisons whose result then had to reach an output pin. Ruling X3 called
  // it "likely a timing wall at 120-125 MHz"; it is a wall at 61.66.
  //
  // A scan asks "which entry still needs work?" every clock. A FIFO is told
  // once, at allocation, and never asks again. The order is identical --
  // allocation order, oldest first -- because that is the order things are
  // pushed in; what disappears is the priority chain, not the policy.
  //
  // Capacity must hold DEPTH*3 = 48 entries and must be a POWER OF TWO.
  //
  // The second half is not decoration. The occupancy is `wq_wp - wq_rp` with
  // one spare pointer bit, and that subtraction is only a correct count if the
  // pointers wrap at a multiple of the capacity. 48 is not a power of two, so
  // 7-bit pointers wrapping at 128 would have produced a count that is right
  // almost always and wrong exactly at the wrap -- the kind of fault that
  // survives every short test. 64 costs sixteen unused entries of six bits.
  //
  // It cannot overflow BY CONSTRUCTION anyway: a fragment pushes at most three
  // entries and cannot be allocated unless a slot is free, so entries in
  // flight are bounded by the same DEPTH the slots are. That is asserted
  // rather than assumed -- see `wq_overflow_o`.
  localparam int unsigned WQN = 1 << $clog2(DEPTH * 3);
  localparam int unsigned WQW = $clog2(WQN);

  logic [SW+1:0]   wq       [WQN];   // {slot, sidx}
  logic [WQW:0]    wq_wp, wq_rp;     // one spare bit: wrap-safe count
  logic [WQW:0]    wq_cnt;
  assign wq_cnt = wq_wp - wq_rp;

  logic [SW-1:0]   aq       [DEPTH];
  logic [SW:0]     aq_wp, aq_rp;
  logic [SW:0]     aq_cnt;
  assign aq_cnt = aq_wp - aq_rp;

  // ---- the registered TMU request -----------------------------------------
  // Ruled defect X3.7 as well: the outputs used to be a combinational view of
  // table storage. Now they are a held packet, which is both a correctness
  // property (it cannot change under a stalled consumer) and the reason the
  // 4.869 ns routing hop is no longer inside a logic cone.
  logic [SW-1:0]   wq_head_slot;
  logic [1:0]      wq_head_sidx;
  assign wq_head_slot = wq[wq_rp[WQW-1:0]][SW+1:2];
  assign wq_head_sidx = wq[wq_rp[WQW-1:0]][1:0];

  logic            aq_head_valid;
  logic [SW-1:0]   aq_head_slot;
  assign aq_head_valid = (aq_cnt != '0);
  assign aq_head_slot  = aq[aq_rp[SW-1:0]];

  assign tmu_rready_o = 1'b1;   // returns write storage; never backpressured
  assign aux_rready_o = 1'b1;

  // --------------------------------------------------------- retirement -----
  // Allocation order, NOT completion order. A fragment is done when every
  // required primary sample has arrived and AUX has if it was asked for.
  logic head_done;
  assign head_done = val_q[head_q]
                  && (arr_q[head_q] == req_q[head_q])
                  && (auxarr_q[head_q] || !auxreq_q[head_q]);

  // `o_valid_o` is now a REGISTER (X3.7). `head_done` stays the internal
  // completion signal; the clock the head actually leaves is
  // `head_done && (!o_valid_q || o_ready_i)`, written where it is used.

  // ---- the combiner, DELIBERATELY UNFROZEN --------------------------------
  // Written as an explicit case over the ratified vocabulary rather than left
  // out, so the unfrozen state is visible AT THE POINT OF USE. Every arm
  // currently returns sample 0.
  //
  // That is EXACT for PASSTHRU and a STAND-IN for the rest.
  // MATERIAL_ARCHITECTURE.md:
  //
  //   > Freeze the combiner's arithmetic and rounding only after checking the
  //   > donor's actual combination law. Do not invent "Sacrifice-compatible"
  //   > blending from the operator names alone.
  //
  // MODULATE could plausibly be written this afternoon as (a*b + 127)/255 and
  // it would look right, pass a self-consistent test, and be unfalsifiable
  // until someone compared it to the donor. This project has shipped a
  // measured-looking wrong number before. So a caller asking for MODULATE gets
  // sample 0 and `combiner_unfrozen_o` high -- a visibly incomplete answer
  // rather than a plausible one.
  logic [23:0] comb_rgb;
  logic [7:0]  comb_a;
  always_comb begin
    unique case (recipe_q[head_q])
      RECIPE_PASSTHRU:   begin comb_rgb = srgb_q[head_q][0]; comb_a = sa_q[head_q][0]; end
      RECIPE_MODULATE:   begin comb_rgb = srgb_q[head_q][0]; comb_a = sa_q[head_q][0]; end
      RECIPE_MODULATE2X: begin comb_rgb = srgb_q[head_q][0]; comb_a = sa_q[head_q][0]; end
      RECIPE_LERP:       begin comb_rgb = srgb_q[head_q][0]; comb_a = sa_q[head_q][0]; end
      RECIPE_ADD_SAT:    begin comb_rgb = srgb_q[head_q][0]; comb_a = sa_q[head_q][0]; end
      RECIPE_MASK:       begin comb_rgb = srgb_q[head_q][0]; comb_a = sa_q[head_q][0]; end
      default:           begin comb_rgb = srgb_q[head_q][0]; comb_a = sa_q[head_q][0]; end
    endcase
  end

  // High whenever the fragment being retired asked for a recipe whose
  // arithmetic is not yet frozen. A gate that ignores this is testing a
  // placeholder. Registered with the packet it describes, so it cannot
  // describe a different fragment than the one on the output.

  // ------------------------------------------------------------ sequential --
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      head_q      <= '0;
      tail_q      <= '0;
      free_cnt_q  <= (SW + 1)'(DEPTH);
      wq_wp <= '0; wq_rp <= '0;
      aq_wp <= '0; aq_rp <= '0;
      tmu_valid_q <= 1'b0;
      aux_valid_q <= 1'b0;
      o_valid_q   <= 1'b0;
      o_unfrozen_q <= 1'b0;
      wq_overflow_o <= 1'b0;
      fragments_o <= '0;
      samples_o   <= '0;
      full_clocks_o <= '0;
      id_errors_o <= '0;
      id_error_o  <= 1'b0;
      for (int i = 0; i < DEPTH; i++) begin
        val_q[i]    <= 1'b0;
        gen_q[i]    <= '0;
        req_q[i]    <= 3'd0;
        arr_q[i]    <= 3'd0;
        auxreq_q[i] <= 1'b0;
        auxarr_q[i] <= 1'b0;
      end
    end else begin
      if (f_valid_i && !f_ready_o) full_clocks_o <= full_clocks_o + 32'd1;

      // ---- allocate -------------------------------------------------------
      if (f_valid_i && f_ready_o) begin
        val_q[tail_q]    <= 1'b1;
        gen_q[tail_q]    <= gen_q[tail_q] + GENW'(1);   // {slot, generation}
        req_q[tail_q]    <= (3'b111 >> (3 - f_sample_count_i)) & 3'b111;
        arr_q[tail_q]    <= 3'd0;
        recipe_q[tail_q] <= f_recipe_i;
        ctx_q[tail_q]    <= f_ctx_i;
        sat_q[tail_q]    <= f_uv_sat_i;
        auxreq_q[tail_q] <= f_aux_i;
        auxarr_q[tail_q] <= 1'b0;
        for (int j = 0; j < 3; j++) begin
          su_q[tail_q][j]    <= f_u_i[j];
          sv_q[tail_q][j]    <= f_v_i[j];
          sbind_q[tail_q][j] <= f_binding_i[j];
          slod_q[tail_q][j]  <= f_lod_i[j];
        end
        tail_q     <= (tail_q == SW'(DEPTH - 1)) ? '0 : tail_q + SW'(1);
        fragments_o <= fragments_o + 32'd1;

        // ---- push one work entry per REQUIRED sample --------------------
        // Up to three in one clock, at consecutive addresses. This is where
        // the old priority scan's work is done: once, by the thing that
        // already knows the answer.
        for (int unsigned j = 0; j < 3; j++)
          if (j < f_sample_count_i)
            wq[WQW'(wq_wp + (WQW+1)'(j))] <= {tail_q, 2'(j)};
        wq_wp <= wq_wp + (WQW+1)'(f_sample_count_i);
        if (f_aux_i) begin
          aq[aq_wp[SW-1:0]] <= tail_q;
          aq_wp <= aq_wp + (SW+1)'(1);
        end

        // A zero-sample fragment reads NO texel (R9: count 0 means
        // has_texture = 0). Its sample-0 storage would otherwise be whatever
        // the previous occupant left, and the combiner reads it -- ruled
        // defect X3.6. Written to a defined value instead of trusted.
        if (f_sample_count_i == 2'd0) begin
          srgb_q[tail_q][0] <= 24'd0;
          sa_q[tail_q][0]   <= 8'd0;
        end
      end

      // ---- issue: pop a work entry into the held request packet -----------
      // The FIFO IS the record of what has been handed out. `iss_q` and
      // `auxiss_q` existed only so the scan could skip what it had already
      // issued; with the scan gone they are state with no reader, and state
      // with no reader is where a stale bit hides. Removed rather than kept
      // "for evidence" -- the counters and the queue pointers are the
      // evidence.
      if (!tmu_valid_q || tmu_ready_i) begin
        if (wq_cnt != '0) begin
          tmu_valid_q <= 1'b1;
          tmu_u_q     <= su_q[wq_head_slot][wq_head_sidx];
          tmu_v_q     <= sv_q[wq_head_slot][wq_head_sidx];
          tmu_bind_q  <= sbind_q[wq_head_slot][wq_head_sidx];
          tmu_lod_q   <= slod_q[wq_head_slot][wq_head_sidx];
          tmu_slot_q  <= wq_head_slot;
          tmu_sidx_q  <= wq_head_sidx;
          tmu_gen_q   <= gen_q[wq_head_slot];
          wq_rp       <= wq_rp + (WQW+1)'(1);
          samples_o   <= samples_o + 32'd1;
        end else begin
          tmu_valid_q <= 1'b0;
        end
      end

      if (!aux_valid_q || aux_ready_i) begin
        if (aq_head_valid) begin
          aux_valid_q <= 1'b1;
          aux_ctx_q   <= ctx_q[aq_head_slot];
          aux_slot_q  <= aq_head_slot;
          aux_gen_q   <= gen_q[aq_head_slot];
          aq_rp       <= aq_rp + (SW+1)'(1);
        end else begin
          aux_valid_q <= 1'b0;
        end
      end

      // BY CONSTRUCTION the work queue cannot overflow: a fragment pushes at
      // most three entries and cannot be allocated without a free slot. Said
      // out loud, with a sticky flag, because "by construction" is a claim.
      if (wq_cnt > (WQW+1)'(WQN)) wq_overflow_o <= 1'b1;

      // ---- returns write TOKEN-INDEXED entries ----------------------------
      // The generation check is what makes a stale return harmless rather than
      // corrupting a reused slot. It is the reason identity is internal.
      if (tmu_rvalid_i) begin
        if (val_q[tmu_rslot_i] && gen_q[tmu_rslot_i] == tmu_rgen_i) begin
          srgb_q[tmu_rslot_i][tmu_rsidx_i] <= tmu_rgb_i;
          sa_q[tmu_rslot_i][tmu_rsidx_i]   <= tmu_a_i;
          arr_q[tmu_rslot_i][tmu_rsidx_i]  <= 1'b1;
        end else begin
          id_errors_o <= id_errors_o + 32'd1;
          id_error_o  <= 1'b1;
        end
      end
      if (aux_rvalid_i) begin
        if (val_q[aux_rslot_i] && gen_q[aux_rslot_i] == aux_rgen_i) begin
          auxrgb_q[aux_rslot_i] <= aux_rgb_i;
          auxa_q[aux_rslot_i]   <= aux_a_i;
          auxarr_q[aux_rslot_i] <= 1'b1;
        end else begin
          id_errors_o <= id_errors_o + 32'd1;
          id_error_o  <= 1'b1;
        end
      end

      // ---- retire ---------------------------------------------------------
      if (!o_valid_q || o_ready_i) begin
        if (head_done) begin
          o_valid_q    <= 1'b1;
          o_rgb_q      <= comb_rgb;
          o_a_q        <= comb_a;
          o_auxrgb_q   <= auxrgb_q[head_q];
          o_auxa_q     <= auxa_q[head_q];
          o_hasaux_q   <= auxarr_q[head_q];
          o_ctx_q      <= ctx_q[head_q];
          o_uvsat_q    <= sat_q[head_q];
          o_unfrozen_q <= (recipe_q[head_q] != RECIPE_PASSTHRU);
          val_q[head_q] <= 1'b0;
          head_q <= (head_q == SW'(DEPTH - 1)) ? '0 : head_q + SW'(1);
        end else begin
          o_valid_q <= 1'b0;
        end
      end

      // ---- free count: ONE assignment, both directions ---------------------
      // Ruled defect. The accept branch decremented and the retire branch
      // incremented with SEPARATE nonblocking assignments to the same variable
      // in the same always_ff. On a cycle that does both -- which is the steady
      // state of a full elastic queue, not an edge case -- only the LAST
      // assignment lands, so the count drifts up by one per such cycle and
      // f_ready_o eventually admits fragments into occupied slots.
      //
      // This is the SAME fault already found and fixed in
      // zhao_raster_perspuv_svc.sv, reproduced here in the next block written.
      // The pattern, not the instance, is the thing to remember: any counter
      // moved by two branches of one always_ff needs a single assignment fed by
      // both conditions.
      begin
        automatic logic acc = f_valid_i && f_ready_o;
        automatic logic ret = head_done && (!o_valid_q || o_ready_i);
        if (acc && !ret)      free_cnt_q <= free_cnt_q - 1'b1;
        else if (!acc && ret) free_cnt_q <= free_cnt_q + 1'b1;
      end
    end
  end

endmodule : zhao_raster_texjoin_v2

`default_nettype wire
