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
    parameter int unsigned LODW  = 4
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
    output var logic [1:0]             tmu_gen_o,

    input  var logic                   tmu_rvalid_i,
    output var logic                   tmu_rready_o,
    input  var logic [23:0]            tmu_rgb_i,
    input  var logic [7:0]             tmu_a_i,
    input  var logic [$clog2(DEPTH)-1:0] tmu_rslot_i,
    input  var logic [1:0]             tmu_rsidx_i,
    input  var logic [1:0]             tmu_rgen_i,

    // ---- AUX, issued from the fragment context and NOT from perspective ------
    // v1's test modelled AUX as a second sampler taking perspective-correct U/V.
    // The brief says the production AUX consumes world position and an
    // envelope, so it must issue as soon as the fragment is accepted.
    output var logic                   aux_valid_o,
    input  var logic                   aux_ready_i,
    output var logic [CTXW-1:0]        aux_ctx_o,
    output var logic [$clog2(DEPTH)-1:0] aux_slot_o,
    output var logic [1:0]             aux_gen_o,
    input  var logic                   aux_rvalid_i,
    output var logic                   aux_rready_o,
    input  var logic [23:0]            aux_rgb_i,
    input  var logic [7:0]             aux_a_i,
    input  var logic [$clog2(DEPTH)-1:0] aux_rslot_i,
    input  var logic [1:0]             aux_rgen_i,

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
  logic [1:0]      gen_q  [DEPTH];
  logic [2:0]      req_q  [DEPTH];   // sample_required_mask
  logic [2:0]      arr_q  [DEPTH];   // sample_arrived_mask
  logic [2:0]      iss_q  [DEPTH];   // already handed to the TMU
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
  logic            auxiss_q [DEPTH];
  logic [23:0]     auxrgb_q [DEPTH];
  logic [7:0]      auxa_q   [DEPTH];
  logic            sat_q  [DEPTH];

  logic [SW-1:0]   head_q, tail_q;          // allocation order == retire order
  logic [SW:0]     free_cnt_q;

  // READY FROM LOCAL STORAGE ONLY. Never from tmu_ready_i — that is the whole
  // point of tokenising, and v1's coupling of the two is what serialises it.
  assign f_ready_o = (free_cnt_q != '0);

  // --------------------------------------------------- sample issue pick ----
  // One request at a time. Walk entries from the retire head so the oldest
  // fragment's samples go first, which keeps retirement from starving.
  logic            pick_v;
  logic [SW-1:0]   pick_slot;
  logic [1:0]      pick_sidx;
  always_comb begin
    pick_v    = 1'b0;
    pick_slot = head_q;
    pick_sidx = 2'd0;
    for (int k = 0; k < DEPTH; k++) begin
      automatic logic [SW-1:0] s = SW'((int'(head_q) + k) % DEPTH);
      if (!pick_v && val_q[s]) begin
        for (int j = 0; j < 3; j++) begin
          if (!pick_v && req_q[s][j] && !iss_q[s][j]) begin
            pick_v    = 1'b1;
            pick_slot = s;
            pick_sidx = 2'(j);
          end
        end
      end
    end
  end

  assign tmu_valid_o   = pick_v;
  assign tmu_u_o       = su_q[pick_slot][pick_sidx];
  assign tmu_v_o       = sv_q[pick_slot][pick_sidx];
  assign tmu_binding_o = sbind_q[pick_slot][pick_sidx];
  assign tmu_lod_o     = slod_q[pick_slot][pick_sidx];
  assign tmu_slot_o    = pick_slot;
  assign tmu_sidx_o    = pick_sidx;
  assign tmu_gen_o     = gen_q[pick_slot];
  assign tmu_rready_o  = 1'b1;   // returns write storage; never backpressured

  // ------------------------------------------------------------ AUX issue ---
  logic          apick_v;
  logic [SW-1:0] apick_slot;
  always_comb begin
    apick_v    = 1'b0;
    apick_slot = head_q;
    for (int k = 0; k < DEPTH; k++) begin
      automatic logic [SW-1:0] s = SW'((int'(head_q) + k) % DEPTH);
      if (!apick_v && val_q[s] && auxreq_q[s] && !auxiss_q[s]) begin
        apick_v    = 1'b1;
        apick_slot = s;
      end
    end
  end
  assign aux_valid_o = apick_v;
  assign aux_ctx_o   = ctx_q[apick_slot];
  assign aux_slot_o  = apick_slot;
  assign aux_gen_o   = gen_q[apick_slot];
  assign aux_rready_o = 1'b1;

  // --------------------------------------------------------- retirement -----
  // Allocation order, NOT completion order. A fragment is done when every
  // required primary sample has arrived and AUX has if it was asked for.
  logic head_done;
  assign head_done = val_q[head_q]
                  && (arr_q[head_q] == req_q[head_q])
                  && (auxarr_q[head_q] || !auxreq_q[head_q]);
  assign o_valid_o = head_done;

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

  // High whenever the retiring fragment asked for a recipe whose arithmetic is
  // not yet frozen. A gate that ignores this is testing a placeholder.
  assign combiner_unfrozen_o = head_done && (recipe_q[head_q] != RECIPE_PASSTHRU);

  assign o_rgb_o     = comb_rgb;
  assign o_a_o       = comb_a;
  assign o_aux_rgb_o = auxrgb_q[head_q];
  assign o_aux_a_o   = auxa_q[head_q];
  assign o_has_aux_o = auxarr_q[head_q];
  assign o_ctx_o     = ctx_q[head_q];
  assign o_uv_sat_o  = sat_q[head_q];

  // ------------------------------------------------------------ sequential --
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      head_q      <= '0;
      tail_q      <= '0;
      free_cnt_q  <= (SW + 1)'(DEPTH);
      fragments_o <= '0;
      samples_o   <= '0;
      full_clocks_o <= '0;
      id_errors_o <= '0;
      id_error_o  <= 1'b0;
      for (int i = 0; i < DEPTH; i++) begin
        val_q[i]    <= 1'b0;
        gen_q[i]    <= 2'd0;
        req_q[i]    <= 3'd0;
        arr_q[i]    <= 3'd0;
        iss_q[i]    <= 3'd0;
        auxreq_q[i] <= 1'b0;
        auxarr_q[i] <= 1'b0;
        auxiss_q[i] <= 1'b0;
      end
    end else begin
      if (f_valid_i && !f_ready_o) full_clocks_o <= full_clocks_o + 32'd1;

      // ---- allocate -------------------------------------------------------
      if (f_valid_i && f_ready_o) begin
        val_q[tail_q]    <= 1'b1;
        gen_q[tail_q]    <= gen_q[tail_q] + 2'd1;   // {slot, generation}
        req_q[tail_q]    <= (3'b111 >> (3 - f_sample_count_i)) & 3'b111;
        arr_q[tail_q]    <= 3'd0;
        iss_q[tail_q]    <= 3'd0;
        recipe_q[tail_q] <= f_recipe_i;
        ctx_q[tail_q]    <= f_ctx_i;
        sat_q[tail_q]    <= f_uv_sat_i;
        auxreq_q[tail_q] <= f_aux_i;
        auxarr_q[tail_q] <= 1'b0;
        auxiss_q[tail_q] <= 1'b0;
        for (int j = 0; j < 3; j++) begin
          su_q[tail_q][j]    <= f_u_i[j];
          sv_q[tail_q][j]    <= f_v_i[j];
          sbind_q[tail_q][j] <= f_binding_i[j];
          slod_q[tail_q][j]  <= f_lod_i[j];
        end
        tail_q     <= (tail_q == SW'(DEPTH - 1)) ? '0 : tail_q + SW'(1);
        free_cnt_q <= free_cnt_q - 1'b1;
        fragments_o <= fragments_o + 32'd1;
      end

      // ---- issue ----------------------------------------------------------
      if (pick_v && tmu_ready_i) begin
        iss_q[pick_slot][pick_sidx] <= 1'b1;
        samples_o <= samples_o + 32'd1;
      end
      if (apick_v && aux_ready_i) auxiss_q[apick_slot] <= 1'b1;

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
      if (head_done && o_ready_i) begin
        val_q[head_q] <= 1'b0;
        head_q     <= (head_q == SW'(DEPTH - 1)) ? '0 : head_q + SW'(1);
        free_cnt_q <= free_cnt_q + 1'b1;
      end
    end
  end

endmodule : zhao_raster_texjoin_v2

`default_nettype wire
