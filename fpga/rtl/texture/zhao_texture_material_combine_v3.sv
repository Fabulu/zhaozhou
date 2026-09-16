// zhao_texture_material_combine_v3.sv -- Packet-B R9 material combiner.
//
// Normative law:
//   reports/MATERIAL_ARCHITECTURE.md, owner ruling R9
//   reports/SHELL-TEXTURE-V3-COMPOSITION-ARCHITECTURE-20260913.md ss3.5
//
// This is a versioned successor rather than an edit of V2.  V2 remains the
// executable oracle for the old island, including its now-superseded material
// arithmetic and AUX-as-sample-2 convention.
//
// The execution organisation deliberately remains V2's paired context engine:
//
//   PASSTHRU / ADD_SAT / MASK / malformed / source error  one phase
//   MODULATE / MODULATE2X / LERP / DETAIL_MASK            two phases
//   DETAIL_LIGHT                                           three phases
//
// Two registered product sites serve one context in a phase.  R9 no longer
// multiplies alpha for recipes 1--4, so the second lane is intentionally idle
// in phase 1 of those three-product RGB recipes.  MASK uses one lane for its
// continuous alpha product.  The meaningful product-job cadence is therefore
// 0/3/3/3/0/1/6/4 for recipe IDs 0..7; phases remain 1/2/2/2/1/1/3/2.
//
// A TMU plane is exactly {status8, raw_index8, alpha8, RGB24}.  The AUX plane
// has a different type ({status8, tag8, strength8, 24'b0}); this block reads
// ONLY its status byte.  In particular AUX never occupies sample 2.
//
// READ_LATE=1 is the Packet-B island shape: admission captures only the owner
// slot and immutable 46-bit material row.  A phase requests the owner planes
// only when sample_count!=0 or AUX is required; count-zero/no-AUX reads none.
// READ_LATE=0 is retained solely
// as the leaf-differential seam: typed planes are copied at admission.  Both
// modes execute the same phase engine and arithmetic.
`default_nettype none

module zhao_texture_material_combine_v3 #(
    parameter int NCTX = 8,
    parameter int TAGW = 14,
    parameter int READ_LATE = 0,
    parameter int SLOTW = 6
) (
    input  var logic clk,
    input  var logic rst_n,

    // Immutable material job.  These fields are the 46-bit owner-keyed row:
    // {base_rgb24, base_a8, weight8, recipe3, sample_count2, aux_required1}.
    input  var logic        f_valid_i,
    output var logic        f_ready_o,
    input  var logic [1:0]  f_sample_count_i,
    input  var logic [2:0]  f_recipe_i,
    input  var logic [7:0]  f_weight_i,
    input  var logic        f_aux_required_i,
    input  var logic [23:0] f_base_rgb_i,
    input  var logic [7:0]  f_base_a_i,
    input  var logic [TAGW-1:0] f_tag_i,

    // READ_LATE=0: complete typed planes copied on fragment admission.
    /* verilator lint_off UNUSEDSIGNAL */
    input  var logic [zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0] f_s0_i,
    input  var logic [zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0] f_s1_i,
    input  var logic [zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0] f_s2_i,
    input  var logic [zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0] f_aux_i,

    // READ_LATE=1: owner slot captured on admission.  When source data is
    // required, all four synchronous plane outputs answer one edge after
    // src_rd_slot_o is presented; count-zero/no-AUX asserts no read request.
    input  var logic [SLOTW-1:0] f_slot_i,
    input  var logic [zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0] src_s0_i,
    input  var logic [zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0] src_s1_i,
    input  var logic [zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0] src_s2_i,
    input  var logic [zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0] src_aux_i,
    /* verilator lint_on UNUSEDSIGNAL */
    output var logic        src_rd_valid_o,
    output var logic [SLOTW-1:0] src_rd_slot_o,

    // Held typed result.  The split fields are aliases of o_result_o, not
    // separately registered authorities.
    output var logic        o_valid_o,
    input  var logic        o_ready_i,
    output var logic [zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0] o_result_o,
    output var logic [23:0] o_rgb_o,
    output var logic [7:0]  o_a_o,
    output var logic [7:0]  o_raw_index_o,
    output var logic [7:0]  o_status_o,
    output var logic [TAGW-1:0] o_tag_o,
    output var logic        o_refused_o,

    // Structural observation: true only when every context, phase/read pipe,
    // completion read, queue and held result owned by this instance is empty.
    output var logic        idle_o,

    // Instrumentation.  All counters wrap modulo 2^32.
    //   jobs_accepted_o   : f_valid_i && f_ready_o
    //   jobs_completed_o  : o_valid_o && o_ready_i
    //   phases_issued_o   : Q-stage physical phase launch
    //   phases_completed_o: accepted scratch/completion write, scr_we||cmp_we
    // At structural drain CJ==CD and PI==PC==the recipe phase demand.
    output var logic [31:0] refused_material_o,
    output var logic [31:0] saturated_add_o,
    output var logic [31:0] saturated_mul2x_o,
    output var logic [31:0] jobs_by_recipe_o [8],
    output var logic [31:0] jobs_recipe_0_o,
    output var logic [31:0] jobs_recipe_1_o,
    output var logic [31:0] jobs_recipe_2_o,
    output var logic [31:0] jobs_recipe_3_o,
    output var logic [31:0] jobs_recipe_4_o,
    output var logic [31:0] jobs_recipe_5_o,
    output var logic [31:0] jobs_recipe_6_o,
    output var logic [31:0] jobs_recipe_7_o,
    output var logic [31:0] jobs_accepted_o,
    output var logic [31:0] jobs_completed_o,
    output var logic [31:0] phases_issued_o,
    output var logic [31:0] phases_completed_o
);

  import zhao_render_texture_pkg::*;

  assign jobs_recipe_0_o = jobs_by_recipe_o[0];
  assign jobs_recipe_1_o = jobs_by_recipe_o[1];
  assign jobs_recipe_2_o = jobs_by_recipe_o[2];
  assign jobs_recipe_3_o = jobs_by_recipe_o[3];
  assign jobs_recipe_4_o = jobs_by_recipe_o[4];
  assign jobs_recipe_5_o = jobs_by_recipe_o[5];
  assign jobs_recipe_6_o = jobs_by_recipe_o[6];
  assign jobs_recipe_7_o = jobs_by_recipe_o[7];

  localparam int CW = (NCTX <= 1) ? 1 : $clog2(NCTX);

  localparam logic [2:0] R_PASSTHRU = 3'd0;
  localparam logic [2:0] R_MODULATE = 3'd1;
  localparam logic [2:0] R_MOD2X    = 3'd2;
  localparam logic [2:0] R_LERP     = 3'd3;
  localparam logic [2:0] R_ADDSAT   = 3'd4;
  localparam logic [2:0] R_MASK     = 3'd5;
  localparam logic [2:0] R_DLIGHT   = 3'd6;
  localparam logic [2:0] R_DMASK    = 3'd7;

  localparam logic [1:0] OP_IDLE = 2'd0;
  localparam logic [1:0] OP_UNIT = 2'd1;
  localparam logic [1:0] OP_MOD2 = 2'd2;
  localparam logic [1:0] OP_LERP = 2'd3;

  initial begin
    if (NCTX < 2 || ((NCTX & (NCTX - 1)) != 0))
      $fatal(1, "zhao_texture_material_combine_v3: NCTX must be a power of two >= 2");
    if (READ_LATE != 0 && READ_LATE != 1)
      $fatal(1, "zhao_texture_material_combine_v3: READ_LATE must be 0 or 1");
  end

  // R9 accepts exact counts, not merely "at least enough".  Count zero is
  // legal only for PASSTHRU.
  function automatic logic count_legal(
      input logic [2:0] recipe,
      input logic [1:0] count);
    case (recipe)
      R_PASSTHRU: count_legal = (count == 2'd0) || (count == 2'd1);
      R_DLIGHT,
      R_DMASK:    count_legal = (count == 2'd3);
      default:    count_legal = (count == 2'd2);
    endcase
  endfunction

  function automatic logic [1:0] phases_of(input logic [2:0] recipe);
    case (recipe)
      R_MODULATE,
      R_MOD2X,
      R_LERP,
      R_DMASK:  phases_of = 2'd2;
      R_DLIGHT: phases_of = 2'd3;
      default:  phases_of = 2'd1;
    endcase
  endfunction

  // Channel number is byte position in {alpha, RGB}: 0=B, 1=G, 2=R, 3=A.
  function automatic logic [7:0] chan32(
      input logic [31:0] rgba,
      input logic [1:0] channel);
    case (channel)
      2'd0: chan32 = rgba[7:0];
      2'd1: chan32 = rgba[15:8];
      2'd2: chan32 = rgba[23:16];
      default: chan32 = rgba[31:24];
    endcase
  endfunction

  function automatic logic [7:0] add_sat8(
      input logic [7:0] a,
      input logic [7:0] b);
    logic [8:0] sum;
    begin
      sum = {1'b0, a} + {1'b0, b};
      add_sat8 = sum[8] ? 8'hFF : sum[7:0];
    end
  endfunction

  // Finish one already-registered raw product.  Bit 8 reports MODULATE2X
  // saturation; no other primitive can saturate for legal byte endpoints.
  function automatic logic [8:0] finish_lane(
      input logic [15:0] product,
      input logic [1:0]  operation,
      input logic        negative,
      input logic [7:0]  lerp_base);
    // SHORT EXACT FINISH. Every arm below computes the SAME function as the
    // long form it replaces, for every one of the 65,536 raw products -- not
    // merely for products reachable from two legal byte operands. The long form
    // built a 17-bit biased sum, or an 18-bit signed negate followed by a
    // biased shift and an 11-bit clamped add, and put all of it in the M->F
    // cone. These are byte-wide carry chains instead.
    //
    // The algebra, with p = 256*high + low:
    //
    //   UNIT   (p + 128) >> 8            == high + (low >= 128)
    //   LERP+  a + floor((p + 128)/256)  == a + high + (low >= 128)
    //   LERP-  a + floor((-p + 128)/256) == a + ~high + (low <= 128)
    //          because floor((-p+128)/256) = -high - (low > 128)
    //
    // The two LERP thresholds are deliberately ASYMMETRIC -- >= 128 positive,
    // <= 128 negative -- and that asymmetry is exactly what preserves the old
    // ties-toward-positive-infinity behaviour on the negative half. Do not
    // "tidy" them into one comparison, and do not rewrite the negative half as
    // a - round(p/256): that disagrees at real ties.
    //
    // In the negative arm bit 8 of the 9-bit sum is the NO-BORROW witness of an
    // unsigned subtraction, so it selects the value rather than the clamp: set
    // means in range, clear means the true result was below zero.
    //
    // MOD2 keeps R9's one-rounding law. Saturation is a direct threshold on the
    // raw product: (p + 64) >> 7 > 255 exactly when p >= 32704. That constant is
    // not 32640 and not 32768. Bit 8 of the result remains the saturation
    // witness and is reported separately from the clamped byte.
    //
    // Checked exhaustively against the previous expressions by
    // tests/tools/test_material_finish_equivalence.py.
    logic [7:0] high;
    logic [7:0] low;
    logic       round_up;
    logic       no_borrow_carry;
    logic [8:0] lerp_sum;
    logic [9:0] mod2_sum;
    begin
      finish_lane = 9'd0;
      high = product[15:8];
      low  = product[7:0];
      round_up = low[7];                 // low >= 128
      no_borrow_carry = (low <= 8'd128); // NOT low < 128; the tie belongs here
      lerp_sum = 9'd0;
      mod2_sum = 10'd0;
      case (operation)
        OP_UNIT: begin
          finish_lane[7:0] = high + {7'd0, round_up};
        end
        OP_MOD2: begin
          if (product >= 16'd32704) begin
            finish_lane = {1'b1, 8'hFF};
          end else begin
            mod2_sum = {1'b0, product[15:7]} +
                       {9'd0, (product[6:0] >= 7'd64)};
            finish_lane[7:0] = mod2_sum[7:0];
          end
        end
        OP_LERP: begin
          if (negative) begin
            lerp_sum = {1'b0, lerp_base} + {1'b0, ~high} +
                       {8'd0, no_borrow_carry};
            finish_lane[7:0] = lerp_sum[8] ? lerp_sum[7:0] : 8'd0;
          end else begin
            lerp_sum = {1'b0, lerp_base} + {1'b0, high} + {8'd0, round_up};
            finish_lane[7:0] = lerp_sum[8] ? 8'hFF : lerp_sum[7:0];
          end
        end
        default: finish_lane = 9'd0;
      endcase
    end
  endfunction

  // One write source per store.
  logic          adm_we;
  logic [CW-1:0] adm_ctx;
  logic          scr_we;
  logic [CW-1:0] scr_ctx;
  logic [32:0]   scr_row;
  logic          cmp_we;
  logic [CW-1:0] cmp_ctx;
  logic [48:0]   cmp_row;

  // Copy mode row: material47 plus four typed planes = 239 bits.
  // Read-late row: {material_refused,count,recipe,weight,aux_required,base32}.
  localparam int PAYW = (READ_LATE != 0)
      ? 47
      : (47 + 4 * TEXTURE_RESULT_W);
  logic [PAYW-1:0] payload_m [NCTX];
  logic [TAGW-1:0] tag_m [NCTX];
  logic [32:0] scratch_m [NCTX];       // {mod2_sat, A, R, G, B}
  logic [48:0] comp_m [NCTX];          // {status8,index8,mod2_sat,A,RGB}

  // Separate NEW and CONT queues allow one admission and one continuation on
  // the same edge.  DONE and FREE retain V2's release-after-output law.
  logic [CW-1:0] newq_m [NCTX];
  logic [CW:0] newq_wp, newq_rp;
  logic [CW-1:0] contq_ctx_m [NCTX];
  logic [1:0] contq_ph_m [NCTX];
  logic [CW:0] contq_wp, contq_rp;
  logic [CW-1:0] doneq_m [NCTX];
  logic [CW:0] doneq_wp, doneq_rp;
  logic [CW-1:0] freeq_m [NCTX];
  logic [CW:0] freeq_wp, freeq_rp;

  wire newq_empty  = (newq_wp == newq_rp);
  wire contq_empty = (contq_wp == contq_rp);
  wire doneq_empty = (doneq_wp == doneq_rp);
  wire freeq_empty = (freeq_wp == freeq_rp);

  assign f_ready_o = !freeq_empty;
  wire admit_c = f_valid_i && f_ready_o;
  wire [CW-1:0] admit_ctx = freeq_m[freeq_rp[CW-1:0]];
  wire material_refused_c = !count_legal(f_recipe_i, f_sample_count_i);

  // CONT priority keeps a started context moving while NEW work fills latency.
  //
  // WB->Q CONTINUATION FORWARDING, AND WHY IT IS NOT A SHORTCUT.
  //
  // MEASURED on this RTL before the bypass existed: a lone multi-phase job
  // relaunched its next phase every EIGHT clocks, and 48 saturated DETAIL_LIGHT
  // jobs sustained only 0.878 phases per clock rather than the 1.000 that eight
  // contexts covering an eight-clock recurrence should give. The S and F
  // registers each added one clock to that loop, and a context is not free when
  // its last phase writes back -- it is free on its OUTPUT handshake -- so the
  // contexts ran out before the pipeline did.
  //
  // The fix is the narrow one: when the continuation queue is EMPTY, the phase
  // that is writing scratch on this very edge launches its own next phase
  // directly, instead of taking a lap through the queue. Priority is unchanged;
  // an already queued continuation still wins, and new work still comes last.
  //
  // Three properties make this safe rather than a same-edge hazard:
  //   * WB is a registered stage, so the bypass carries wb_ctx/wb_ph and never
  //     reaches backwards into unregistered finish arithmetic;
  //   * the scratch write happens on THIS edge and `scr_rd <= scratch_m[r_ctx]`
  //     reads on the FOLLOWING one, so the next phase observes the completed
  //     write without relying on same-edge old-data behaviour;
  //   * a bypassed event is never also enqueued. Doing both would run the phase
  //     twice; the committed double-issue mutant exists for exactly that.
  //
  // phases_issued stays tied to this q_valid_c launch and phases_completed to
  // the actual scratch/completion write, so one physical phase still books one
  // of each.
  // The two seams below are plain `ifdef` selectors on purpose. A function-like
  // `define cannot be overridden from a Verilator -D on the command line: the
  // override is silently ignored and the default is compiled, so a mutant built
  // that way measures unmutated production and "passes". That was observed here
  // before these were rewritten, and it is the broken-instrument failure in its
  // most flattering direction -- a control that cannot fire reports success.
  // The empty-queue guard is NOT behind a selector. Inverting it was tried as a
  // control and could not be made to fire: with CONT holding absolute priority
  // at Q, the continuation queue is virtually always empty at the moment a
  // non-final phase retires, so the guarded state is unreachable under every
  // workload this suite drives. A control that cannot fire would report success
  // forever, so none is shipped; proving this guard needs stimulus that first
  // backs CONT up deliberately, and that is recorded as owed rather than faked.
  localparam bit CONT_BYPASS_SUPPRESS_PUSH =
`ifdef ZHAO_MATV3_MUTANT_CONT_BYPASS_DOUBLE_ISSUE
      1'b0;  // WRONG: enqueues the very phase it also forwards.
`else
      1'b1;
`endif

  wire wb_cont_c = wb_v && !wb_final;
  wire cont_bypass_c = wb_cont_c && contq_empty;
  wire q_valid_c = !contq_empty || cont_bypass_c || !newq_empty;
  wire q_from_cont_c = !contq_empty;
  wire q_from_bypass_c = !q_from_cont_c && cont_bypass_c;
  wire [CW-1:0] q_ctx_c = q_from_cont_c
      ? contq_ctx_m[contq_rp[CW-1:0]]
      : (q_from_bypass_c ? wb_ctx : newq_m[newq_rp[CW-1:0]]);
  wire [1:0] q_ph_c = q_from_cont_c
      ? contq_ph_m[contq_rp[CW-1:0]]
      : (q_from_bypass_c ? (wb_ph + 2'd1) : 2'd0);

  logic [PAYW-1:0] pay_wr_c;
  logic [PAYW-1:0] pay_rd;
  logic [32:0] scr_rd;
  logic [48:0] cmp_rd;
  logic [TAGW-1:0] tag_rd;

  logic [CW-1:0] done_ctx_c;

  always_ff @(posedge clk) begin
    if (adm_we) payload_m[adm_ctx] <= pay_wr_c;
    pay_rd <= payload_m[r_ctx];
  end

  always_ff @(posedge clk) begin
    if (scr_we) scratch_m[scr_ctx] <= scr_row;
    scr_rd <= scratch_m[r_ctx];
  end

  always_ff @(posedge clk) begin
    if (adm_we) tag_m[adm_ctx] <= f_tag_i;
    tag_rd <= tag_m[done_ctx_c];
  end

  always_ff @(posedge clk) begin
    if (cmp_we) comp_m[cmp_ctx] <= cmp_row;
    cmp_rd <= comp_m[done_ctx_c];
  end

  // Q -> R -> D aligns the synchronous payload/scratch and optional owner-plane
  // reads.  S captures those complete source facts before recipe selection; O
  // selects operands; M contains exactly two product registers; F captures the
  // finished lanes; and WB captures the assembled whole row.  The two added
  // boundaries make Q-to-continuation exactly eight clocks at NCTX=8.
  logic r_v, d_v, s_v, o_v, m_v, f_v;
  logic [CW-1:0] r_ctx, d_ctx, s_ctx, o_ctx, m_ctx, f_ctx;
  logic [1:0] r_ph, d_ph, s_ph, o_ph, m_ph, f_ph;

  // Sample-1/2 raw indices and all AUX payload bits are present because these
  // are typed planes.  R9 deliberately consumes neither; only sample-0 index
  // and AUX status have meaning at this boundary.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0] p_s0, p_s1, p_s2, p_aux;
  logic [zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0] s_s0, s_s1, s_s2, s_aux;
  /* verilator lint_on UNUSEDSIGNAL */
  logic [31:0] p_base, s_base;
  logic p_aux_required, s_aux_required;
  logic [7:0] p_weight, s_weight;
  logic [2:0] p_recipe, s_recipe;
  logic [1:0] p_count, s_count;
  logic p_material_refused, s_material_refused;
  logic [32:0] s_scratch;

  generate
    if (READ_LATE == 0) begin : g_copy
      assign pay_wr_c = {
          material_refused_c,
          f_sample_count_i,
          f_recipe_i,
          f_weight_i,
          f_aux_required_i,
          f_base_a_i,
          f_base_rgb_i,
          f_aux_i,
          f_s2_i,
          f_s1_i,
          f_s0_i
      };

      assign p_s0 = pay_rd[zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0];
      assign p_s1 = pay_rd[95:48];
      assign p_s2 = pay_rd[143:96];
      assign p_aux = pay_rd[191:144];
      assign p_base = pay_rd[223:192];
      assign p_aux_required = pay_rd[224];
      assign p_weight = pay_rd[232:225];
      assign p_recipe = pay_rd[235:233];
      assign p_count = pay_rd[237:236];
      assign p_material_refused = pay_rd[238];

      assign src_rd_valid_o = 1'b0;
      assign src_rd_slot_o = '0;
    end else begin : g_readlate
      assign pay_wr_c = {
          material_refused_c,
          f_sample_count_i,
          f_recipe_i,
          f_weight_i,
          f_aux_required_i,
          f_base_a_i,
          f_base_rgb_i
      };

      assign p_s0 = src_s0_i;
      assign p_s1 = src_s1_i;
      assign p_s2 = src_s2_i;
      assign p_aux = src_aux_i;
      assign p_base = pay_rd[31:0];
      assign p_aux_required = pay_rd[32];
      assign p_weight = pay_rd[40:33];
      assign p_recipe = pay_rd[43:41];
      assign p_count = pay_rd[45:44];
      assign p_material_refused = pay_rd[46];

      logic [SLOTW-1:0] slot_m [NCTX];
      logic needs_source_m [NCTX];
      logic [SLOTW-1:0] r_slot;
      logic r_needs_source;
      always_ff @(posedge clk) begin
        if (adm_we) begin
          slot_m[adm_ctx] <= f_slot_i;
          needs_source_m[adm_ctx] <=
              (f_sample_count_i != 2'd0) || f_aux_required_i;
        end
        r_slot <= slot_m[q_ctx_c];
        r_needs_source <= needs_source_m[q_ctx_c];
      end
      assign src_rd_slot_o = r_slot;
      assign src_rd_valid_o = r_v && r_needs_source;
    end
  endgenerate

  // Canonical operands and exact required-source reduction consume only S-stage
  // registers.  Raw owner-bank outputs therefore terminate at S and cannot feed
  // an O-stage multiplier input in the same timing cone.
  logic [31:0] s_rgba0, s_rgba1, s_rgba2;
  logic [7:0] s_status;
  logic [7:0] s_raw_index;
  always_comb begin
    s_rgba0 = (s_count == 2'd0) ? s_base : s_s0[31:0];
    s_rgba1 = (s_count >= 2'd2) ? s_s1[31:0] : s_rgba0;
    s_rgba2 = (s_count == 2'd3) ? s_s2[31:0] : s_rgba0;

    s_status = 8'd0;
    case (s_count)
      2'd1: s_status = s_s0[TEXTURE_RESULT_STATUS_HI:TEXTURE_RESULT_STATUS_LO];
      2'd2: s_status = s_s0[TEXTURE_RESULT_STATUS_HI:TEXTURE_RESULT_STATUS_LO] | s_s1[TEXTURE_RESULT_STATUS_HI:TEXTURE_RESULT_STATUS_LO];
      2'd3: s_status = s_s0[TEXTURE_RESULT_STATUS_HI:TEXTURE_RESULT_STATUS_LO] | s_s1[TEXTURE_RESULT_STATUS_HI:TEXTURE_RESULT_STATUS_LO] | s_s2[TEXTURE_RESULT_STATUS_HI:TEXTURE_RESULT_STATUS_LO];
      default: s_status = 8'd0;
    endcase
    if (s_aux_required)
      s_status = s_status | s_aux[TEXTURE_RESULT_STATUS_HI:TEXTURE_RESULT_STATUS_LO];
    if (s_material_refused) s_status[0] = 1'b1;

    s_raw_index = (s_count == 2'd0) ? 8'd0 :
        s_s0[TEXTURE_RESULT_SAMPLE0_INDEX_HI:TEXTURE_RESULT_SAMPLE0_INDEX_LO];
  end

  // O-stage controls and operands.
  logic [7:0] o_a0, o_b0, o_a1, o_b1;
  logic [1:0] o_op0, o_op1;
  logic o_en0, o_en1;
  logic [7:0] o_lerp_base0, o_lerp_base1;
  logic o_lerp_neg0, o_lerp_neg1;
  logic [2:0] o_recipe;
  logic o_final, o_bypass;
  logic [32:0] o_scratch;
  logic [31:0] o_direct;
  logic [7:0] o_keep_a;
  logic [7:0] o_status;
  logic [7:0] o_index;
  logic o_addsat_sat;

  // M-stage registered products and carried controls.
  logic [15:0] m_p0, m_p1;
  logic [1:0] m_op0, m_op1;
  logic m_en0, m_en1;
  logic [7:0] m_lerp_base0, m_lerp_base1;
  logic m_lerp_neg0, m_lerp_neg1;
  logic [2:0] m_recipe;
  logic m_final, m_bypass;
  logic [32:0] m_scratch;
  logic [31:0] m_direct;
  logic [7:0] m_keep_a;
  logic [7:0] m_status;
  logic [7:0] m_index;
  logic m_addsat_sat;

  // F-stage registered finish results and controls.  The expensive signed
  // LERP/round/saturate functions end here; row assembly begins only from F.
  logic [8:0] f_lane0, f_lane1;
  logic f_en0, f_en1;
  logic [2:0] f_recipe;
  logic f_final, f_bypass;
  logic [32:0] f_scratch;
  logic [31:0] f_direct;
  logic [7:0] f_keep_a;
  logic [7:0] f_status;
  logic [7:0] f_index;
  logic f_addsat_sat;

  // WB is a narrow assembled-row boundary.  Completion/scratch RAM writes and
  // continuation ownership consume only this registered row.
  logic wb_v;
  logic [CW-1:0] wb_ctx;
  logic [1:0] wb_ph;
  logic wb_final;
  logic [48:0] wb_row;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      r_v <= 1'b0;
      d_v <= 1'b0;
      s_v <= 1'b0;
      o_v <= 1'b0;
      m_v <= 1'b0;
      f_v <= 1'b0;
      wb_v <= 1'b0;
      r_ctx <= '0;
      d_ctx <= '0;
      s_ctx <= '0;
      o_ctx <= '0;
      m_ctx <= '0;
      f_ctx <= '0;
      r_ph <= 2'd0;
      d_ph <= 2'd0;
      s_ph <= 2'd0;
      o_ph <= 2'd0;
      m_ph <= 2'd0;
      f_ph <= 2'd0;
      phases_issued_o <= 32'd0;
      phases_completed_o <= 32'd0;
    end else begin
      // Q is the physical phase-launch edge.  WB is the independently clocked
      // scratch/completion write edge; neither counter aliases the other.
      r_v <= q_valid_c;
      r_ctx <= q_ctx_c;
      r_ph <= q_ph_c;
      if (q_valid_c) phases_issued_o <= phases_issued_o + 32'd1;
      if (scr_we || cmp_we)
        phases_completed_o <= phases_completed_o + 32'd1;

      // R -> D, aligned with synchronous store outputs.
      d_v <= r_v;
      d_ctx <= r_ctx;
      d_ph <= r_ph;

      // D -> S captures the complete synchronous-read answer.  All recipe and
      // operand logic below consumes only these registers.
      s_v <= d_v;
      s_ctx <= d_ctx;
      s_ph <= d_ph;
      if (d_v) begin
        s_s0 <= p_s0;
        s_s1 <= p_s1;
        s_s2 <= p_s2;
        s_aux <= p_aux;
        s_base <= p_base;
        s_aux_required <= p_aux_required;
        s_weight <= p_weight;
        s_recipe <= p_recipe;
        s_count <= p_count;
        s_material_refused <= p_material_refused;
        s_scratch <= scr_rd;
      end

      // S -> O defaults.
      o_v <= s_v;
      o_ctx <= s_ctx;
      o_ph <= s_ph;
      o_recipe <= s_recipe;
      o_final <= s_material_refused || (|s_status) ||
                 (s_ph + 2'd1 == phases_of(s_recipe));
      o_bypass <= 1'b0;
      o_scratch <= s_scratch;
      o_direct <= 32'd0;
      o_keep_a <= s_rgba0[31:24];
      o_status <= s_status;
      o_index <= s_raw_index;
      o_addsat_sat <= 1'b0;
      o_a0 <= 8'd0;
      o_b0 <= 8'd0;
      o_a1 <= 8'd0;
      o_b1 <= 8'd0;
      o_op0 <= OP_IDLE;
      o_op1 <= OP_IDLE;
      o_en0 <= 1'b0;
      o_en1 <= 1'b0;
      o_lerp_base0 <= 8'd0;
      o_lerp_base1 <= 8'd0;
      o_lerp_neg0 <= 1'b0;
      o_lerp_neg1 <= 1'b0;

      // Any nonzero final status is a terminal loud error.  It still retires,
      // preserving index and tag, but never publishes a plausible partial mix.
      if (|s_status) begin
        o_bypass <= 1'b1;
        o_direct <= {8'hFF, 24'hFF00FF};
      end else begin
        case (s_recipe)
          R_PASSTHRU: begin
            o_bypass <= 1'b1;
            o_direct <= s_rgba0;
          end

          R_ADDSAT: begin
            o_bypass <= 1'b1;
            o_direct <= {
                s_rgba0[31:24],
                add_sat8(s_rgba0[23:16], s_rgba1[23:16]),
                add_sat8(s_rgba0[15:8], s_rgba1[15:8]),
                add_sat8(s_rgba0[7:0], s_rgba1[7:0])
            };
            o_addsat_sat <=
                ({1'b0, s_rgba0[7:0]} + {1'b0, s_rgba1[7:0]} > 9'd255) ||
                ({1'b0, s_rgba0[15:8]} + {1'b0, s_rgba1[15:8]} > 9'd255) ||
                ({1'b0, s_rgba0[23:16]} + {1'b0, s_rgba1[23:16]} > 9'd255);
          end

          R_MASK: begin
            // One product in one phase: RGB is exactly sample 0, while alpha
            // is continuous unit8 s0.a*s1.a.  It is not a binary gate.
            o_a0 <= s_rgba0[31:24];
            o_b0 <= s_rgba1[31:24];
            o_op0 <= OP_UNIT;
            o_en0 <= 1'b1;
            o_direct <= {8'd0, s_rgba0[23:0]};
          end

          R_MODULATE,
          R_MOD2X,
          R_LERP: begin
            logic [1:0] c0, c1;
            logic [7:0] av0, bv0, av1, bv1;
            c0 = (s_ph == 2'd0) ? 2'd0 : 2'd2;
            c1 = 2'd1;
            av0 = chan32(s_rgba0, c0);
            bv0 = chan32(s_rgba1, c0);
            av1 = chan32(s_rgba0, c1);
            bv1 = chan32(s_rgba1, c1);

            o_en0 <= 1'b1;
            o_en1 <= (s_ph == 2'd0);
            o_a0 <= (s_recipe == R_LERP && bv0 < av0) ? (av0 - bv0) :
                    ((s_recipe == R_LERP) ? (bv0 - av0) : av0);
            o_b0 <= (s_recipe == R_LERP) ? s_weight : bv0;
            o_a1 <= (s_recipe == R_LERP && bv1 < av1) ? (av1 - bv1) :
                    ((s_recipe == R_LERP) ? (bv1 - av1) : av1);
            o_b1 <= (s_recipe == R_LERP) ? s_weight : bv1;
            o_lerp_base0 <= av0;
            o_lerp_base1 <= av1;
            o_lerp_neg0 <= (bv0 < av0);
            o_lerp_neg1 <= (bv1 < av1);
            if (s_recipe == R_MOD2X) begin
              o_op0 <= OP_MOD2;
              o_op1 <= OP_MOD2;
            end else if (s_recipe == R_LERP) begin
              o_op0 <= OP_LERP;
              o_op1 <= OP_LERP;
            end else begin
              o_op0 <= OP_UNIT;
              o_op1 <= OP_UNIT;
            end
          end

          R_DMASK: begin
            // First layer is MODULATE2X RGB.  Phase 1 pairs its final RGB
            // channel with the independent unit-alpha mask from sample 2.
            o_en0 <= 1'b1;
            o_en1 <= 1'b1;
            o_op0 <= OP_MOD2;
            o_a0 <= chan32(s_rgba0, (s_ph == 2'd0) ? 2'd0 : 2'd2);
            o_b0 <= chan32(s_rgba1, (s_ph == 2'd0) ? 2'd0 : 2'd2);
            if (s_ph == 2'd0) begin
              o_op1 <= OP_MOD2;
              o_a1 <= chan32(s_rgba0, 2'd1);
              o_b1 <= chan32(s_rgba1, 2'd1);
            end else begin
              o_op1 <= OP_UNIT;
              o_a1 <= s_rgba0[31:24];
              o_b1 <= s_rgba2[31:24];
            end
          end

          R_DLIGHT: begin
            // MODULATE2X first layer, then unit-multiply by the true sample 2.
            // Phase 1 pairs first-layer R with second-layer B, exactly as V2's
            // dependency-aware schedule did; AUX is nowhere in this data path.
            case (s_ph)
              2'd0: begin
                o_en0 <= 1'b1;
                o_en1 <= 1'b1;
                o_op0 <= OP_MOD2;
                o_op1 <= OP_MOD2;
                o_a0 <= chan32(s_rgba0, 2'd0);
                o_b0 <= chan32(s_rgba1, 2'd0);
                o_a1 <= chan32(s_rgba0, 2'd1);
                o_b1 <= chan32(s_rgba1, 2'd1);
              end
              2'd1: begin
                o_en0 <= 1'b1;
                o_en1 <= 1'b1;
                o_op0 <= OP_MOD2;
                o_op1 <= OP_UNIT;
                o_a0 <= chan32(s_rgba0, 2'd2);
                o_b0 <= chan32(s_rgba1, 2'd2);
                o_a1 <= s_scratch[7:0];
                o_b1 <= chan32(s_rgba2, 2'd0);
              end
              default: begin
                o_en0 <= 1'b1;
                o_en1 <= 1'b1;
                o_op0 <= OP_UNIT;
                o_op1 <= OP_UNIT;
                o_a0 <= s_scratch[15:8];
                o_b0 <= chan32(s_rgba2, 2'd1);
                o_a1 <= s_scratch[23:16];
                o_b1 <= chan32(s_rgba2, 2'd2);
              end
            endcase
          end

          default: begin
            // All three-bit encodings are covered above.  This defensive path
            // remains loud if the encoding width changes without this table.
            o_bypass <= 1'b1;
            o_direct <= {8'hFF, 24'hFF00FF};
            o_status <= s_status | 8'h01;
            o_final <= 1'b1;
          end
        endcase
      end

      // O -> M.  These are the only two multiplication operators in the block.
      m_v <= o_v;
      m_ctx <= o_ctx;
      m_ph <= o_ph;
      m_recipe <= o_recipe;
      m_final <= o_final;
      m_bypass <= o_bypass;
      m_scratch <= o_scratch;
      m_direct <= o_direct;
      m_keep_a <= o_keep_a;
      m_status <= o_status;
      m_index <= o_index;
      m_addsat_sat <= o_addsat_sat;
      m_op0 <= o_op0;
      m_op1 <= o_op1;
      m_en0 <= o_en0;
      m_en1 <= o_en1;
      m_lerp_base0 <= o_lerp_base0;
      m_lerp_base1 <= o_lerp_base1;
      m_lerp_neg0 <= o_lerp_neg0;
      m_lerp_neg1 <= o_lerp_neg1;
      m_p0 <= o_a0 * o_b0;
      m_p1 <= o_a1 * o_b1;

      // M -> F.  Finish arithmetic terminates at these registers.
      f_v <= m_v;
      if (m_v) begin
        f_ctx <= m_ctx;
        f_ph <= m_ph;
        f_lane0 <= finish_lane(m_p0, m_op0, m_lerp_neg0, m_lerp_base0);
        f_lane1 <= finish_lane(m_p1, m_op1, m_lerp_neg1, m_lerp_base1);
        f_en0 <= m_en0;
        f_en1 <= m_en1;
        f_recipe <= m_recipe;
        f_final <= m_final;
        f_bypass <= m_bypass;
        f_scratch <= m_scratch;
        f_direct <= m_direct;
        f_keep_a <= m_keep_a;
        f_status <= m_status;
        f_index <= m_index;
        f_addsat_sat <= m_addsat_sat;
      end

      // F -> WB.  Only the registered finish results enter row assembly.
      wb_v <= f_v;
      if (f_v) begin
        wb_ctx <= f_ctx;
        wb_ph <= f_ph;
        wb_final <= f_final;
        wb_row <= {f_status, f_index, next_scratch};
      end
    end
  end

  logic [7:0] f_r0, f_r1;
  logic f_mod2_sat;
  always_comb begin
    f_r0 = f_lane0[7:0];
    f_r1 = f_lane1[7:0];
    f_mod2_sat = (f_en0 && f_lane0[8]) || (f_en1 && f_lane1[8]);
  end

  logic [32:0] next_scratch;
  always_comb begin
    next_scratch = f_scratch;
    if (f_bypass) begin
      next_scratch = {1'b0, f_direct};
    end else begin
      case (f_recipe)
        R_MASK: begin
          next_scratch = {1'b0, f_r0, f_direct[23:0]};
        end

        R_DLIGHT: begin
          case (f_ph)
            2'd0: begin
              next_scratch[7:0] = f_r0;
              next_scratch[15:8] = f_r1;
            end
            2'd1: begin
              next_scratch[23:16] = f_r0;
              next_scratch[7:0] = f_r1;
            end
            default: begin
              next_scratch[15:8] = f_r0;
              next_scratch[23:16] = f_r1;
            end
          endcase
          next_scratch[31:24] = f_keep_a;
          next_scratch[32] = (f_ph == 2'd0)
              ? f_mod2_sat
              : (f_scratch[32] | f_mod2_sat);
        end

        R_DMASK: begin
          if (f_ph == 2'd0) begin
            next_scratch[7:0] = f_r0;
            next_scratch[15:8] = f_r1;
            next_scratch[31:24] = f_keep_a;
          end else begin
            next_scratch[23:16] = f_r0;
            next_scratch[31:24] = f_r1;
          end
          next_scratch[32] = (f_ph == 2'd0)
              ? f_mod2_sat
              : (f_scratch[32] | f_mod2_sat);
        end

        default: begin
          if (f_ph == 2'd0) begin
            next_scratch[7:0] = f_r0;
            next_scratch[15:8] = f_r1;
          end else begin
            next_scratch[23:16] = f_r0;
          end
          next_scratch[31:24] = f_keep_a;
          next_scratch[32] = (f_ph == 2'd0)
              ? f_mod2_sat
              : (f_scratch[32] | f_mod2_sat);
        end
      endcase
    end
  end

  always_comb begin
    adm_we = admit_c;
    adm_ctx = admit_ctx;

    scr_we = wb_v && !wb_final;
    scr_ctx = wb_ctx;
    scr_row = wb_row[32:0];

    cmp_we = wb_v && wb_final;
    cmp_ctx = wb_ctx;
    cmp_row = wb_row;
  end

  logic out_full_q;
  // Bit 32 is the internal per-fragment MODULATE2X saturation witness.  It is
  // consumed by counters before completion is read and is not a result bit.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [48:0] out_val_q;
  logic [48:0] prefetch_val_q;
  /* verilator lint_on UNUSEDSIGNAL */
  logic [TAGW-1:0] out_tag_q, prefetch_tag_q;
  logic [CW-1:0] out_ctx_q, prefetch_ctx_q;
  logic prefetch_full_q;
  logic done_rd_q;
  logic [CW-1:0] done_rd_ctx_q;

  assign o_valid_o = out_full_q;
  assign o_rgb_o = out_val_q[23:0];
  assign o_a_o = out_val_q[31:24];
  assign o_raw_index_o = out_val_q[40:33];
  assign o_status_o = out_val_q[48:41];
  assign o_result_o = {o_status_o, o_raw_index_o, o_a_o, o_rgb_o};
  assign o_refused_o = o_status_o[0];
  assign o_tag_o = out_tag_q;

  // Completion reads are a one-cycle synchronous pipeline feeding two elastic
  // result slots: the public output and one prefetch register.  Counting the
  // read in flight as reserved occupancy guarantees an arbitrary future output
  // stall always has somewhere to land, while a ready consumer permits one
  // pop, one arriving response, and one new read issue on the same edge.
  wire out_pop_c = out_full_q && o_ready_i;
  wire out_free_c = !out_full_q || out_pop_c;
  wire prefetch_to_out_c = prefetch_full_q && out_free_c;
  wire arrival_to_out_c = done_rd_q && !prefetch_full_q && out_free_c;
  wire arrival_to_prefetch_c = done_rd_q && !arrival_to_out_c;
  wire [2:0] response_occupancy_c =
      {2'd0, out_full_q} + {2'd0, prefetch_full_q} + {2'd0, done_rd_q};
  wire [2:0] response_after_pop_c =
      response_occupancy_c - (out_pop_c ? 3'd1 : 3'd0);
  wire out_issue_c = !doneq_empty && (response_after_pop_c < 3'd2);
  assign done_ctx_c = doneq_m[doneq_rp[CW-1:0]];

  wire [CW:0] free_level_c = freeq_wp - freeq_rp;
  wire all_contexts_free_c =
      (free_level_c == ($bits(free_level_c))'(NCTX));

  assign idle_o = all_contexts_free_c
               && newq_empty
               && contq_empty
               && doneq_empty
               && !r_v
               && !d_v
               && !s_v
               && !o_v
               && !m_v
               && !f_v
               && !wb_v
               && !done_rd_q
               && !prefetch_full_q
               && !out_full_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      newq_wp <= '0;
      newq_rp <= '0;
      contq_wp <= '0;
      contq_rp <= '0;
      doneq_wp <= '0;
      doneq_rp <= '0;
      freeq_wp <= '0;
      freeq_rp <= '0;
      out_full_q <= 1'b0;
      out_val_q <= '0;
      out_tag_q <= '0;
      out_ctx_q <= '0;
      prefetch_full_q <= 1'b0;
      prefetch_val_q <= '0;
      prefetch_tag_q <= '0;
      prefetch_ctx_q <= '0;
      done_rd_q <= 1'b0;
      done_rd_ctx_q <= '0;
      refused_material_o <= 32'd0;
      saturated_add_o <= 32'd0;
      saturated_mul2x_o <= 32'd0;
      jobs_accepted_o <= 32'd0;
      jobs_completed_o <= 32'd0;
      for (int i = 0; i < 8; i++) jobs_by_recipe_o[i] <= 32'd0;
      for (int i = 0; i < NCTX; i++) freeq_m[i] <= CW'(i);
      freeq_wp <= ($bits(freeq_wp))'(NCTX);
    end else begin
      if (admit_c) begin
        newq_m[newq_wp[CW-1:0]] <= admit_ctx;
        newq_wp <= newq_wp + 1'b1;
        freeq_rp <= freeq_rp + 1'b1;
        jobs_accepted_o <= jobs_accepted_o + 32'd1;
        if (material_refused_c)
          refused_material_o <= refused_material_o + 32'd1;
      end

      // A forwarded continuation consumes neither queue: it never entered one.
      if (q_valid_c) begin
        if (q_from_cont_c)           contq_rp <= contq_rp + 1'b1;
        else if (!q_from_bypass_c)   newq_rp <= newq_rp + 1'b1;
      end

      // Count meaningful product jobs at the M result edge, not powered-but-idle
      // lane slots.  Saturation consumes the aligned registered F results.
      if (m_v) begin
        case ({m_en1, m_en0})
          2'b01,
          2'b10: jobs_by_recipe_o[m_recipe] <=
                     jobs_by_recipe_o[m_recipe] + 32'd1;
          2'b11: jobs_by_recipe_o[m_recipe] <=
                     jobs_by_recipe_o[m_recipe] + 32'd2;
          default: begin end
        endcase
      end

      if (f_v && f_final) begin
        if ((f_recipe == R_MOD2X || f_recipe == R_DLIGHT ||
             f_recipe == R_DMASK) &&
            (((f_ph == 2'd0) ? 1'b0 : f_scratch[32]) | f_mod2_sat))
          saturated_mul2x_o <= saturated_mul2x_o + 32'd1;
        if (f_recipe == R_ADDSAT && f_addsat_sat)
          saturated_add_o <= saturated_add_o + 32'd1;
      end

      // Continuation/done ownership advances only with the actual WB RAM write.
      if (wb_v) begin
        if (wb_final) begin
          doneq_m[doneq_wp[CW-1:0]] <= wb_ctx;
          doneq_wp <= doneq_wp + 1'b1;
        end else if (!(cont_bypass_c && CONT_BYPASS_SUPPRESS_PUSH)) begin
          // Enqueue ONLY the continuation that was not forwarded this edge.
          // Enqueueing a bypassed phase as well would launch it twice.
          contq_ctx_m[contq_wp[CW-1:0]] <= wb_ctx;
          contq_ph_m[contq_wp[CW-1:0]] <= wb_ph + 2'd1;
          contq_wp <= contq_wp + 1'b1;
        end
      end

      // One synchronous completion read may launch every clock while the two
      // elastic response slots have reserved capacity for its future arrival.
      done_rd_q <= out_issue_c;
      if (out_issue_c) begin
        doneq_rp <= doneq_rp + 1'b1;
        done_rd_ctx_q <= done_ctx_c;
      end

      // Existing prefetch data is older and therefore wins the public slot.
      // A simultaneous new read arrival refills prefetch on the same edge.
      if (prefetch_to_out_c) begin
        out_val_q <= prefetch_val_q;
        out_tag_q <= prefetch_tag_q;
        out_ctx_q <= prefetch_ctx_q;
        out_full_q <= 1'b1;
      end else if (arrival_to_out_c) begin
        out_val_q <= cmp_rd;
        out_tag_q <= tag_rd;
        out_ctx_q <= done_rd_ctx_q;
        out_full_q <= 1'b1;
      end else if (out_pop_c) begin
        out_full_q <= 1'b0;
      end

      if (arrival_to_prefetch_c) begin
        prefetch_val_q <= cmp_rd;
        prefetch_tag_q <= tag_rd;
        prefetch_ctx_q <= done_rd_ctx_q;
        prefetch_full_q <= 1'b1;
      end else if (prefetch_to_out_c) begin
        prefetch_full_q <= 1'b0;
      end

      // Completion is the held-output handshake, never merely presentation.
      // A stalled valid therefore cannot complete the same job twice.
      if (out_pop_c) begin
        jobs_completed_o <= jobs_completed_o + 32'd1;
        freeq_m[freeq_wp[CW-1:0]] <= out_ctx_q;
        freeq_wp <= freeq_wp + 1'b1;
      end
    end
  end

endmodule : zhao_texture_material_combine_v3

`default_nettype wire
