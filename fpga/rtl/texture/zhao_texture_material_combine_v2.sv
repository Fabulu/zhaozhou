// zhao_texture_material_combine_v2.sv - paired-phase material combiner.
//
// Law: reports/COMBINE-ASSETFETCH-RECOVERY-20260906.txt sections 4, 5 and 6
//      reference/include/zref/zref_material.hpp  (the arithmetic, unchanged)
//
// ===========================================================================
// WHAT CHANGES, AND WHAT DOES NOT
// ===========================================================================
// The owner's recovery brief, section 0: "COMBINE needs a different EXECUTION
// ORGANIZATION, not different material math. Preserve all eight recipes.
// Replace record-by-job priority scans with two explicit product lanes, a fixed
// paired-phase schedule, small ready queues, synchronous operand storage and
// one whole-context writeback per phase."
//
// So every equation here is V1's and the oracle's, byte for byte. What is
// replaced is HOW work is chosen and where operands live.
//
// V1 scans records and per-record issued/done masks, its two lanes may select
// DIFFERENT contexts, and each job writes back its own channel. That costs a
// broad operand-selection network and a scattered writeback, and the composed
// island fit put its worst path inside this block --
// `u_combine|Mux136~0_OTERM7702` at -5.737 ns, worse than any path the
// pre-repair census found anywhere.
//
// ===========================================================================
// THE PAIRED SCHEDULE (section 4.2)
// ===========================================================================
// Every product-bearing phase uses BOTH lanes for ONE context, so there is one
// address, one phase tag and one whole-row write:
//
//   PASSTHRU / ADD_SAT / MASK / count==0 / refused
//       one ZERO-PRODUCT phase through the same pipeline
//   MODULATE / MODULATE2X / LERP
//       phase 0: R and G      phase 1: B and A
//   DETAIL_MASK
//       phase 0: U(s0.r,s1.r), U(s0.g,s1.g)
//       phase 1: U(s0.b,s1.b), U(s0.a,s2.a)
//   DETAIL_LIGHT
//       phase 0: R1=U(s0.r,s1.r), G1=U(s0.g,s1.g)
//       phase 1: B1=U(s0.b,s1.b), R2=U(R1,s2.r)
//       phase 2: G2=U(G1,s2.g),   B2=U(B1,s2.b)
//       alpha:   s0.a, no multiplication
//
// DETAIL_LIGHT's fixed three-phase order is the whole reason a general
// dependency scheduler is not needed: its only cross-layer dependency is
// handled by placing R2 in phase 1 beside B1, which is already computed.
//
// EVERY PRODUCT RECIPE HAS AN EVEN JOB COUNT, which is what makes pairing
// within one context exact rather than approximate.
//
// ===========================================================================
// ONE WRITER AND ONE READER PER STORE (section 5)
// ===========================================================================
//   payload   written by admission,        read by the selected phase
//   scratch   written by nonfinal writeback, read by the selected continuation
//   completion written by final writeback,  read by the output
//   tag       written by admission,        read by the output
//
// Payload and scratch are separate because admission and writeback can happen
// in the same cycle. Scratch and completion are separate because an output read
// can happen while another context reads continuation operands.
//
// SCRATCH IS NOT INITIALISED ON ADMISSION -- that would be a second writer.
// Phase 0 ignores old scratch and writes every field its continuation needs.
//
// ===========================================================================
// THE PIPELINE (section 6.1), EDGE BY EDGE
// ===========================================================================
//   Q  selected ticket (context + phase)
//   R  synchronous payload / scratch read
//   O  selected operands, registered
//   M  two raw products, registered
//   F  rounding, add/sub, doubling/saturation, registered
//   W  one whole scratch-or-completion write
//
// The prohibited one-cycle cones from section 6.1 are prohibited BY THIS
// STRUCTURE, not by a comment: queue selection cannot reach a multiply because
// R and O sit between them; the DETAIL_LIGHT first-layer product cannot reach
// the second because they are in different phases; and no RAM output feeds an
// asynchronous indexed table before arithmetic.
//
// Conservative SystemVerilog subset only (charter section 2).
module zhao_texture_material_combine_v2 #(
    // Bounded execution contexts. EIGHT initially, and the brief is explicit
    // that this is "a latency-hiding choice, not the global fragment capacity"
    // -- the island's owner credit bounds fragments; this bounds how many
    // material jobs are in flight inside this block.
    parameter int NCTX = 8,
    parameter int TAGW = 16
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- fragment in -------------------------------------------------------
    input  var logic        f_valid_i,
    output var logic        f_ready_o,
    input  var logic [1:0]  f_sample_count_i,
    input  var logic [2:0]  f_recipe_i,
    input  var logic [7:0]  f_weight_i,
    input  var logic [23:0] f_s0_rgb_i,
    input  var logic [7:0]  f_s0_a_i,
    input  var logic [23:0] f_s1_rgb_i,
    input  var logic [7:0]  f_s1_a_i,
    input  var logic [23:0] f_s2_rgb_i,
    input  var logic [7:0]  f_s2_a_i,
    input  var logic [23:0] f_base_rgb_i,
    input  var logic [7:0]  f_base_a_i,
    input  var logic [TAGW-1:0] f_tag_i,

    // ---- fragment out ------------------------------------------------------
    output var logic        o_valid_o,
    input  var logic        o_ready_i,
    output var logic [23:0] o_rgb_o,
    output var logic [7:0]  o_a_o,
    output var logic [TAGW-1:0] o_tag_o,
    output var logic        o_refused_o,

    // ---- counters ----------------------------------------------------------
    output var logic [31:0] refused_recipe_o,
    output var logic [31:0] refused_missing_o,
    output var logic [31:0] saturated_add_o,
    output var logic [31:0] saturated_mul2x_o,
    output var logic [31:0] jobs_by_recipe_o [8],
    // Phases issued. The paired schedule's whole claim is that a fragment costs
    // a FIXED, KNOWN number of phases; a counter is how that stops being a
    // claim. V1 issued every microjob about twice and every colour still
    // matched the oracle exactly, because recomputing a product is idempotent.
    output var logic [31:0] phases_issued_o
);

  localparam int CW = (NCTX <= 1) ? 1 : $clog2(NCTX);

  // ---- recipe encoding, from zref::material::Recipe -------------------------
  localparam logic [2:0] R_PASSTHRU = 3'd0;
  localparam logic [2:0] R_MODULATE = 3'd1;
  localparam logic [2:0] R_MOD2X    = 3'd2;
  localparam logic [2:0] R_LERP     = 3'd3;
  localparam logic [2:0] R_ADDSAT   = 3'd4;
  localparam logic [2:0] R_MASK     = 3'd5;
  localparam logic [2:0] R_DLIGHT   = 3'd6;
  localparam logic [2:0] R_DMASK    = 3'd7;

  // How many samples each recipe REQUIRES (zref::material::samples_required).
  function automatic logic [1:0] samples_required(input logic [2:0] r);
    case (r)
      R_PASSTHRU: samples_required = 2'd1;
      R_DLIGHT, R_DMASK: samples_required = 2'd3;
      default: samples_required = 2'd2;
    endcase
  endfunction

  // Phases a recipe costs. Section 4.2's schedule, as a function.
  function automatic logic [1:0] phases_of(input logic [2:0] r);
    case (r)
      R_MODULATE, R_MOD2X, R_LERP, R_DMASK: phases_of = 2'd2;
      R_DLIGHT: phases_of = 2'd3;
      default: phases_of = 2'd1;   // PASSTHRU, ADDSAT, MASK: one zero-product
    endcase
  endfunction

  // ---- the frozen arithmetic, restated (RTL cannot call the oracle) ---------
  // `((a*b) + 128) >> 8`, clamp 255. multstyle is NOT relied on here: D19v
  // measured V1 at 2 DSP against a header claiming zero, because the attribute
  // sat on a function-local automatic variable. The product sites are named
  // registers in the pipeline below so the mapping question is answerable from
  // the RAM/DSP summary rather than from an attribute nobody verified.
  /* verilator lint_off UNUSEDSIGNAL */
  function automatic logic [7:0] round8(input logic [16:0] p);
    // The low eight bits are the discarded fraction: `+128` above has already
    // done the rounding, so this is a shift, not a truncation.
    round8 = (p[16:8] > 9'd255) ? 8'd255 : p[15:8];
  endfunction
  /* verilator lint_on UNUSEDSIGNAL */

  // ---- the single write port of each store, driven from ONE place ---------
  // Combinational selects feeding the clock-only processes above, so the
  // arrays live in a reset-free block while the control that decides what to
  // write keeps its reset. That separation is the whole point of 9.5 B.
  // Driven from exactly one place each, so the memory processes below stay
  // clock-only with one write address and one read address.
  logic            adm_we;
  logic [CW-1:0]   adm_ctx;
  logic            scr_we;
  logic [CW-1:0]   scr_ctx;
  logic [32:0]     scr_row;
  logic            cmp_we;
  logic [CW-1:0]   cmp_ctx;
  logic [33:0]     cmp_row;

  // Forward declarations: the drivers below are written after the pipeline,
  // because they depend on its M-stage outputs.
  wire             admit_c_w;
  wire [CW-1:0]    admit_ctx_w;

  // ==========================================================================
  // Stores. One write source and one read source each (section 5).
  // ==========================================================================
  // payload: s0/s1/s2 RGBA + weight + recipe + count + refused
  localparam int PAYW = 96 + 8 + 3 + 2 + 1;
  logic [PAYW-1:0]  payload_m [NCTX];
  logic [TAGW-1:0]  tag_m     [NCTX];
  logic [32:0]      scratch_m [NCTX];   // {sat, A, B, G, R}
  logic [33:0]      comp_m    [NCTX];   // {refused, sat, A, B, G, R}

  // ==========================================================================
  // FIFOs. NEW and CONT are separate because BOTH may enqueue in one cycle --
  // an input acceptance and a returning nonfinal phase (section 4.3).
  // ==========================================================================
  logic [CW-1:0] newq_m [NCTX];
  logic [CW:0]   newq_wp, newq_rp;
  logic [CW-1:0] contq_ctx_m [NCTX];
  logic [1:0]    contq_ph_m  [NCTX];
  logic [CW:0]   contq_wp, contq_rp;
  logic [CW-1:0] doneq_m [NCTX];
  logic [CW:0]   doneq_wp, doneq_rp;
  logic [CW-1:0] freeq_m [NCTX];
  logic [CW:0]   freeq_wp, freeq_rp;

  wire newq_empty  = (newq_wp == newq_rp);
  wire contq_empty = (contq_wp == contq_rp);
  wire doneq_empty = (doneq_wp == doneq_rp);
  wire freeq_empty = (freeq_wp == freeq_rp);

  // ==========================================================================
  // Admission. Ready falls when no context or NEW capacity remains; after the
  // handshake the producer is free to change its pins and this core uses only
  // captured data (section 6.3).
  // ==========================================================================
  assign f_ready_o = !freeq_empty;
  wire admit_c = f_valid_i && f_ready_o;
  wire [CW-1:0] admit_ctx = freeq_m[freeq_rp[CW-1:0]];
  assign admit_c_w   = admit_c;
  assign admit_ctx_w = admit_ctx;

  // Validation precedence is preserved BEFORE canonicalisation (section 5):
  // an unknown recipe is refused first, then a missing sample, and only then
  // does count==0 copy BASE into s0.
  wire bad_recipe_c  = 1'b0;   // f_recipe_i is 3 bits; all eight encodings exist
  wire bad_sample_c  = (f_sample_count_i < samples_required(f_recipe_i)) &&
                       (f_sample_count_i != 2'd0);
  // count == 0 is NOT a missing sample: it is the untextured case, and BASE
  // becomes s0 under a bypass operation rather than carrying a permanent
  // base-colour field on every textured context.
  wire zero_count_c  = (f_sample_count_i == 2'd0);
  wire refused_c     = bad_recipe_c || bad_sample_c;

  wire [31:0] s0_in = zero_count_c ? {f_base_a_i, f_base_rgb_i} : {f_s0_a_i, f_s0_rgb_i};
  wire [31:0] s1_in = (f_sample_count_i > 2'd1) ? {f_s1_a_i, f_s1_rgb_i} : s0_in;
  wire [31:0] s2_in = (f_sample_count_i > 2'd2) ? {f_s2_a_i, f_s2_rgb_i} : s0_in;
  // A zero-count or refused fragment runs the one zero-product phase, which is
  // PASSTHRU's, so the same pipeline carries it.
  wire [2:0] recipe_in = (zero_count_c || refused_c) ? R_PASSTHRU : f_recipe_i;

  // ==========================================================================
  // Q: ticket selection. CONT has priority over NEW so a partially executed
  // fragment finishes rather than being overtaken -- contexts are the scarce
  // resource and a started one holds one.
  // ==========================================================================
  wire       q_valid_c = !contq_empty || !newq_empty;
  wire       q_from_cont = !contq_empty;
  wire [CW-1:0] q_ctx_c = q_from_cont ? contq_ctx_m[contq_rp[CW-1:0]]
                                      : newq_m[newq_rp[CW-1:0]];
  wire [1:0] q_ph_c = q_from_cont ? contq_ph_m[contq_rp[CW-1:0]] : 2'd0;

  // ---- pipeline registers ---------------------------------------------------
  // Q -> R (index) -> D (memory output) -> O -> M -> F/W.
  //
  // `w_v` IS GONE, and that is addendum 9.5 A. It registered validity one edge
  // later than the context, phase and result it gated, so the write enable
  // belonged to the PREVIOUS M-stage occupant while the data belonged to the
  // current one. Back-to-back phases and bubbles tell the two apart.
  //
  // Of the two repairs the addendum offers, this takes the first: F stays
  // combinational from the stable M registers, and the write plus its
  // continuation-or-completion enqueue all happen on `m_v` at that same edge.
  // Nothing is shifted on its own. The other repair -- a real W register for
  // every field -- is equally valid and costs a stage; this one costs none.
  logic          r_v, d_v, o_v, m_v;
  logic [CW-1:0] r_ctx, d_ctx, o_ctx, m_ctx;
  logic [1:0]    r_ph, d_ph, o_ph, m_ph;

  // O-stage operands and control
  logic [7:0]  o_a0, o_b0, o_a1, o_b1;      // lane operands
  logic [2:0]  o_recipe;
  logic        o_final;
  logic [32:0] o_scratch;
  logic [7:0]  o_keep_a;                    // alpha carried, not multiplied
  logic        o_lerp;                      // lane op is LERP
  logic [7:0]  o_lbase0, o_lbase1;          // LERP: the `a` operand held
  logic        o_lneg0, o_lneg1;            // LERP: sign of (b - a)
  logic        o_zero;                      // zero-product phase
  logic [31:0] o_direct;                    // the zero-product result
  // REFUSAL TRAVELS WITH THE JOB. It is decided at admission, from the payload
  // row, and read at the final writeback -- not recomputed at the output from
  // live input, which is the rule section 6.2 states as "no output or product
  // return consults live input recipe, weight or the current FIFO head".
  logic        o_refused;
  logic        o_addsat_sat;                // ADD_SAT clamped a channel

  // M-stage products
  logic [16:0] m_p0, m_p1;
  logic [2:0]  m_recipe;
  logic        m_final, m_lerp, m_zero;
  logic [7:0]  m_lbase0, m_lbase1, m_keep_a;
  logic        m_lneg0, m_lneg1;
  logic [32:0] m_scratch;
  logic [31:0] m_direct;
  logic        m_refused;
  logic        m_addsat_sat;

  // ==========================================================================
  // Pipeline
  // ==========================================================================
  // ---- CLOCK-ONLY MEMORY PROCESSES (addendum 9.5 B) ------------------------
  // These were asynchronous wires. A registered INDEX is not a registered
  // memory OUTPUT, and the addendum says so plainly: leaving the reads
  // combinational into the O selection, with the writes sitting inside the
  // large asynchronous-reset control process, "risks recreating the inference
  // problem being fixed".
  //
  // So one clock-only process per store, NO RESET -- an array in an
  // asynchronous-reset block is the shape that stops inferring, which the
  // island's own RAM Summary has now demonstrated twice. Control reset stays
  // in the control process, where it belongs.
  //
  // The read output arrives ONE CYCLE after its index, so `d_*` below carries
  // the ticket alongside it. That alignment stage is what the first draft
  // lacked, and its absence is exactly why validity and payload could belong
  // to different transactions.
  logic [PAYW-1:0] pay_rd;
  logic [32:0]     scr_rd;
  logic [33:0]     cmp_rd;
  logic [TAGW-1:0] tag_rd;

  always_ff @(posedge clk) begin
    if (adm_we) payload_m[adm_ctx] <= {refused_c, f_sample_count_i, recipe_in,
                                       f_weight_i, s2_in, s1_in, s0_in};
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

  wire [31:0] p_s0 = pay_rd[31:0];
  wire [31:0] p_s1 = pay_rd[63:32];
  wire [31:0] p_s2 = pay_rd[95:64];
  wire [7:0]  p_w  = pay_rd[103:96];
  wire [2:0]  p_rc = pay_rd[106:104];
  // The sample count is stored for provenance and is not read after
  // canonicalisation: admission has already resolved count==0 into BASE-as-s0
  // and a missing sample into a refusal, so nothing downstream may re-decide it.
  /* verilator lint_off UNUSEDSIGNAL */
  wire [1:0]  p_ct = pay_rd[108:107];
  /* verilator lint_on UNUSEDSIGNAL */
  wire        p_rf = pay_rd[109];


  // Channel pickers: 0=R 1=G 2=B 3=A
  function automatic logic [7:0] chan(input logic [31:0] s, input logic [1:0] c);
    case (c)
      2'd0: chan = s[7:0];
      2'd1: chan = s[15:8];
      2'd2: chan = s[23:16];
      default: chan = s[31:24];
    endcase
  endfunction

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      r_v <= 1'b0; d_v <= 1'b0; o_v <= 1'b0; m_v <= 1'b0;
      r_ctx <= '0; d_ctx <= '0; o_ctx <= '0; m_ctx <= '0;
      r_ph <= 2'd0; d_ph <= 2'd0; o_ph <= 2'd0; m_ph <= 2'd0;
      phases_issued_o <= 32'd0;
    end else begin
      // ---- Q -> R ----------------------------------------------------------
      r_v   <= q_valid_c;
      r_ctx <= q_ctx_c;
      r_ph  <= q_ph_c;
      if (q_valid_c) phases_issued_o <= phases_issued_o + 32'd1;

      // ---- R -> D : the memory output arrives, and the ticket meets it ------
      d_v   <= r_v;
      d_ctx <= r_ctx;
      d_ph  <= r_ph;

      // ---- D -> O : operands selected from the REGISTERED read --------------
      o_v      <= d_v;
      o_ctx    <= d_ctx;
      o_ph     <= d_ph;
      o_recipe <= p_rc;
      o_scratch<= scr_rd;
      o_final  <= (d_ph + 2'd1 == phases_of(p_rc));
      o_keep_a <= p_s0[31:24];
      o_zero   <= 1'b0;
      o_lerp   <= 1'b0;
      o_a0 <= 8'd0; o_b0 <= 8'd0; o_a1 <= 8'd0; o_b1 <= 8'd0;
      o_lbase0 <= 8'd0; o_lbase1 <= 8'd0;
      o_lneg0 <= 1'b0; o_lneg1 <= 1'b0;
      o_direct <= 32'd0;
      o_refused <= p_rf;
      o_addsat_sat <= 1'b0;

      case (p_rc)
        R_MODULATE, R_MOD2X: begin
          // phase 0: R,G   phase 1: B,A
          o_a0 <= chan(p_s0, d_ph == 2'd0 ? 2'd0 : 2'd2);
          o_b0 <= chan(p_s1, d_ph == 2'd0 ? 2'd0 : 2'd2);
          o_a1 <= chan(p_s0, d_ph == 2'd0 ? 2'd1 : 2'd3);
          o_b1 <= chan(p_s1, d_ph == 2'd0 ? 2'd1 : 2'd3);
        end
        R_LERP: begin
          o_lerp <= 1'b1;
          // The signed difference is prepared BEFORE the multiply (section
          // 6.2): magnitude to the lane, sign and base held for stage F.
          begin
            logic [7:0] a0v, b0v, a1v, b1v;
            a0v = chan(p_s0, d_ph == 2'd0 ? 2'd0 : 2'd2);
            b0v = chan(p_s1, d_ph == 2'd0 ? 2'd0 : 2'd2);
            a1v = chan(p_s0, d_ph == 2'd0 ? 2'd1 : 2'd3);
            b1v = chan(p_s1, d_ph == 2'd0 ? 2'd1 : 2'd3);
            o_lbase0 <= a0v;
            o_lbase1 <= a1v;
            o_lneg0  <= (b0v < a0v);
            o_lneg1  <= (b1v < a1v);
            o_a0 <= (b0v < a0v) ? (a0v - b0v) : (b0v - a0v);
            o_a1 <= (b1v < a1v) ? (a1v - b1v) : (b1v - a1v);
            o_b0 <= p_w;
            o_b1 <= p_w;
          end
        end
        R_DMASK: begin
          // phase 0: r,g   phase 1: b, and alpha from s0.a x s2.a
          o_a0 <= chan(p_s0, d_ph == 2'd0 ? 2'd0 : 2'd2);
          o_b0 <= chan(p_s1, d_ph == 2'd0 ? 2'd0 : 2'd2);
          o_a1 <= (d_ph == 2'd0) ? chan(p_s0, 2'd1) : chan(p_s0, 2'd3);
          o_b1 <= (d_ph == 2'd0) ? chan(p_s1, 2'd1) : chan(p_s2, 2'd3);
        end
        R_DLIGHT: begin
          // phase 0: R1,G1   phase 1: B1, R2=U(R1,s2.r)   phase 2: G2,B2
          case (d_ph)
            2'd0: begin
              o_a0 <= chan(p_s0, 2'd0); o_b0 <= chan(p_s1, 2'd0);
              o_a1 <= chan(p_s0, 2'd1); o_b1 <= chan(p_s1, 2'd1);
            end
            2'd1: begin
              o_a0 <= chan(p_s0, 2'd2); o_b0 <= chan(p_s1, 2'd2);
              o_a1 <= scr_rd[7:0];      o_b1 <= chan(p_s2, 2'd0);
            end
            default: begin
              o_a0 <= scr_rd[15:8];     o_b0 <= chan(p_s2, 2'd1);
              o_a1 <= scr_rd[23:16];    o_b1 <= chan(p_s2, 2'd2);
            end
          endcase
        end
        default: begin
          // PASSTHRU / ADD_SAT / MASK / refused: ONE zero-product phase.
          o_zero <= 1'b1;
          if (p_rf) o_direct <= 32'd0;
          else if (p_rc == R_MASK)
            o_direct <= (p_s1[31:24] != 8'd0) ? p_s0 : 32'd0;
          else if (p_rc == R_ADDSAT) begin
            o_addsat_sat <= ((9'(p_s0[31:24]) + 9'(p_s1[31:24])) > 9'd255) ||
                            ((9'(p_s0[23:16]) + 9'(p_s1[23:16])) > 9'd255) ||
                            ((9'(p_s0[15:8])  + 9'(p_s1[15:8]))  > 9'd255) ||
                            ((9'(p_s0[7:0])   + 9'(p_s1[7:0]))   > 9'd255);
            o_direct <= {
              ((9'(p_s0[31:24]) + 9'(p_s1[31:24])) > 9'd255) ? 8'd255
                : 8'(p_s0[31:24] + p_s1[31:24]),
              ((9'(p_s0[23:16]) + 9'(p_s1[23:16])) > 9'd255) ? 8'd255
                : 8'(p_s0[23:16] + p_s1[23:16]),
              ((9'(p_s0[15:8])  + 9'(p_s1[15:8]))  > 9'd255) ? 8'd255
                : 8'(p_s0[15:8]  + p_s1[15:8]),
              ((9'(p_s0[7:0])   + 9'(p_s1[7:0]))   > 9'd255) ? 8'd255
                : 8'(p_s0[7:0]   + p_s1[7:0])};
          end else o_direct <= p_s0;
        end
      endcase
      if (p_rf) o_zero <= 1'b1;

      // ---- O -> M : the two products ---------------------------------------
      m_v      <= o_v;
      m_ctx    <= o_ctx;
      m_ph     <= o_ph;
      m_recipe <= o_recipe;
      m_final  <= o_final;
      m_lerp   <= o_lerp;
      m_zero   <= o_zero;
      m_keep_a <= o_keep_a;
      m_scratch<= o_scratch;
      m_direct <= o_direct;
      m_refused <= o_refused;
      m_addsat_sat <= o_addsat_sat;
      m_lbase0 <= o_lbase0; m_lbase1 <= o_lbase1;
      m_lneg0  <= o_lneg0;  m_lneg1  <= o_lneg1;
      // THE TWO PRODUCT SITES. Exactly two `*` operators in the block, both
      // here, both registered -- which is what makes the DSP question
      // answerable rather than attribute-dependent.
      m_p0 <= 17'({8'd0, o_a0} * {8'd0, o_b0}) + 17'd128;
      m_p1 <= 17'({8'd0, o_a1} * {8'd0, o_b1}) + 17'd128;

      // ---- M -> W : rounding, doubling, saturation, sign ---------------------
    end
  end

  // F stage, combinational into the W write (the brief allows F and W to share
  // an edge; the prohibited cone is selection->multiply->accumulate, not
  // round->write).
  logic [7:0] f_r0, f_r1;
  logic       f_sat;

  always_comb begin
    logic [7:0] q0, q1;
    logic [9:0] d0, d1;
    logic signed [9:0] sr0, sr1;
    // EVERY LOCAL ASSIGNED ON EVERY PATH. Without these four, the LERP and
    // MODULATE2X locals are written only inside their own branches, and a
    // LATCH is inferred for each -- a combinational block that remembers,
    // which is never what this stage means.
    //
    // (A comment line whose first word is the linter's own name is parsed as
    // a PRAGMA. That is the second time today; the sentence above is wrapped
    // to keep the name off the start of a line.)
    d0 = 10'd0; d1 = 10'd0; sr0 = 10'sd0; sr1 = 10'sd0;
    f_sat = 1'b0;
    q0 = round8(m_p0);
    q1 = round8(m_p1);
    f_r0 = q0;
    f_r1 = q1;
    if (m_lerp) begin
      sr0 = m_lneg0 ? (10'(m_lbase0) - 10'(q0)) : (10'(m_lbase0) + 10'(q0));
      sr1 = m_lneg1 ? (10'(m_lbase1) - 10'(q1)) : (10'(m_lbase1) + 10'(q1));
      f_r0 = (sr0 < 0) ? 8'd0 : ((sr0 > 255) ? 8'd255 : 8'(sr0));
      f_r1 = (sr1 < 0) ? 8'd0 : ((sr1 > 255) ? 8'd255 : 8'(sr1));
    end else if (m_recipe == R_MOD2X) begin
      d0 = 10'(q0) * 10'd2;
      d1 = 10'(q1) * 10'd2;
      if (d0 > 10'd255) begin f_r0 = 8'd255; f_sat = 1'b1; end else f_r0 = 8'(d0);
      if (d1 > 10'd255) begin f_r1 = 8'd255; f_sat = 1'b1; end else f_r1 = 8'(d1);
    end
  end

  // ==========================================================================
  // W: ONE whole-row write, to scratch (nonfinal) or completion (final).
  // ==========================================================================
  logic [32:0] next_scratch;
  always_comb begin
    next_scratch = m_scratch;
    if (m_zero) begin
      next_scratch = {1'b0, m_direct};
    end else begin
      case (m_recipe)
        R_DLIGHT: begin
          case (m_ph)
            2'd0: begin next_scratch[7:0] = f_r0; next_scratch[15:8] = f_r1; end
            2'd1: begin next_scratch[23:16] = f_r0; next_scratch[7:0] = f_r1; end
            default: begin next_scratch[15:8] = f_r0; next_scratch[23:16] = f_r1; end
          endcase
          next_scratch[31:24] = m_keep_a;
        end
        R_DMASK: begin
          if (m_ph == 2'd0) begin
            next_scratch[7:0] = f_r0; next_scratch[15:8] = f_r1;
          end else begin
            next_scratch[23:16] = f_r0; next_scratch[31:24] = f_r1;
          end
        end
        default: begin
          if (m_ph == 2'd0) begin
            next_scratch[7:0] = f_r0; next_scratch[15:8] = f_r1;
          end else begin
            next_scratch[23:16] = f_r0; next_scratch[31:24] = f_r1;
          end
        end
      endcase
      // PHASE ZERO IGNORES OLD SCRATCH -- including its flags (addendum 9.5 D).
      //
      // Scratch is deliberately not cleared at admission, because that would be
      // a second writer. The rule was "phase 0 ignores old scratch", and the
      // first draft turned it into "phase 0 keeps its old flags": it seeded the
      // accumulator with `m_scratch[32]`, so a saturating MODULATE2X could leak
      // its flag into the next fragment to reuse the context. The fix belongs
      // in the combinational write DATA, not in another RAM writer.
      next_scratch[32] = (m_ph == 2'd0) ? f_sat : (m_scratch[32] | f_sat);
    end
  end

  // ---- memory write-port drivers -------------------------------------------
  // ADMISSION writes payload and tag; the FINAL writeback writes completion;
  // a NONFINAL writeback writes scratch. Exactly one source each, which is
  // what lets the processes above stay 1W/1R.
  always_comb begin
    adm_we  = admit_c_w;
    adm_ctx = admit_ctx_w;

    scr_we  = m_v && !m_final;
    scr_ctx = m_ctx;
    scr_row = next_scratch;

    cmp_we  = m_v && m_final;
    cmp_ctx = m_ctx;
    // 34 bits: {refused, sat, A, B, G, R}. The refusal comes from the JOB, not
    // from a saturation flag -- the first draft built it from `f_sat` and an
    // always-false term, so a refused fragment would have retired as a
    // successful black one.
    cmp_row = {m_refused, next_scratch};
  end

  // ==========================================================================
  // Output: a HELD register. A newly completed record must not replace a result
  // already offered while ready is low (section 1.3) -- V1's combinational
  // priority choice among completed records is exactly that fault.
  // ==========================================================================
  logic              out_full_q;
  // Bit 32 is the SATURATION flag, carried through completion storage and not
  // surfaced: saturation is reported by its counters, once per fragment, and a
  // per-fragment output flag would be a second, differently-shaped answer to
  // the same question.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [33:0]       out_val_q;
  /* verilator lint_on UNUSEDSIGNAL */
  logic [TAGW-1:0]   out_tag_q;

  assign o_valid_o   = out_full_q;
  assign o_rgb_o     = out_val_q[23:0];
  assign o_a_o       = out_val_q[31:24];
  assign o_refused_o = out_val_q[33];
  assign o_tag_o     = out_tag_q;

  // THE COMPLETION READ IS SYNCHRONOUS TOO, so the output is a two-step:
  // issue the read (popping DONE), then load the held register from the
  // REGISTERED completion and tag outputs on the next edge. One read in
  // flight at a time, and it is only issued when the held slot is free.
  logic          done_rd_q;
  logic [CW-1:0] out_ctx_q;      // whose result is being held

  wire out_pop_c   = out_full_q && o_ready_i;
  wire out_free_c  = !out_full_q || out_pop_c;
  wire out_issue_c = out_free_c && !doneq_empty && !done_rd_q;
  wire [CW-1:0] done_ctx_c = doneq_m[doneq_rp[CW-1:0]];

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      newq_wp <= '0; newq_rp <= '0;
      contq_wp <= '0; contq_rp <= '0;
      doneq_wp <= '0; doneq_rp <= '0;
      freeq_wp <= '0; freeq_rp <= '0;
      out_full_q <= 1'b0;
      out_val_q  <= '0;
      out_tag_q  <= '0;
      done_rd_q  <= 1'b0;
      out_ctx_q  <= '0;
      refused_recipe_o <= 32'd0;
      refused_missing_o <= 32'd0;
      saturated_add_o <= 32'd0;
      saturated_mul2x_o <= 32'd0;
      for (int i = 0; i < 8; i++) jobs_by_recipe_o[i] <= 32'd0;
      // FREE starts holding every context.
      for (int i = 0; i < NCTX; i++) freeq_m[i] <= CW'(i);
      freeq_wp <= ($bits(freeq_wp))'(NCTX);
    end else begin
      // ---- admission -------------------------------------------------------
      if (admit_c) begin
        // payload_m and tag_m are written by their own clock-only processes;
        // this block only moves the QUEUES and the counters.
        newq_m[newq_wp[CW-1:0]] <= admit_ctx;
        newq_wp <= newq_wp + 1'b1;
        freeq_rp <= freeq_rp + 1'b1;
        if (refused_c) refused_missing_o <= refused_missing_o + 32'd1;
      end

      // ---- Q pop -----------------------------------------------------------
      if (q_valid_c) begin
        if (q_from_cont) contq_rp <= contq_rp + 1'b1;
        else             newq_rp  <= newq_rp + 1'b1;
      end

      // ---- W: the one whole-row write --------------------------------------
      // THE PRODUCT-JOB COUNTER COUNTS PRODUCTS (addendum 9.5 C).
      //
      // It counted ADMITTED FRAGMENTS: one for a passthrough costing zero
      // products and one for a DETAIL_LIGHT costing six. A fragment histogram
      // carrying a product-job name, and the oracle's `product_jobs()` would
      // have had to be bent to match it -- which is the wrong direction of fix.
      //
      // Two lanes fire per product-bearing phase, so two are counted, at the
      // M stage where the multiply actually launches and with that phase's own
      // recipe. Bypass phases count ZERO. MODULATE = 2 phases x 2 = 4;
      // DETAIL_LIGHT = 3 x 2 = 6; DETAIL_MASK = 2 x 2 = 4 -- which is exactly
      // what zref::material::product_jobs() says, without changing the oracle.
      if (m_v && !m_zero) begin
        jobs_by_recipe_o[m_recipe] <= jobs_by_recipe_o[m_recipe] + 32'd2;
      end

      if (m_v) begin
        if (m_final) begin
          // comp_m is written by its own process; the enqueue happens on this
          // SAME edge as the write, which is the convention 9.5 A asks for.
          doneq_m[doneq_wp[CW-1:0]] <= m_ctx;
          doneq_wp <= doneq_wp + 1'b1;
          // ONCE PER FRAGMENT, not per channel -- the oracle's own rule.
          if (m_recipe == R_MOD2X && (m_scratch[32] | f_sat))
            saturated_mul2x_o <= saturated_mul2x_o + 32'd1;
          if (m_recipe == R_ADDSAT && m_addsat_sat)
            saturated_add_o <= saturated_add_o + 32'd1;
        end else begin
          contq_ctx_m[contq_wp[CW-1:0]] <= m_ctx;
          contq_ph_m[contq_wp[CW-1:0]]  <= m_ph + 2'd1;
          contq_wp <= contq_wp + 1'b1;
        end
      end

      // ---- output ----------------------------------------------------------
      // Step 1: issue the completion/tag read and pop DONE.
      if (out_issue_c) begin
        doneq_rp  <= doneq_rp + 1'b1;
        done_rd_q <= 1'b1;
        out_ctx_q <= done_ctx_c;
      end

      // Step 2: the registered read has answered; hold it.
      if (done_rd_q) begin
        out_val_q  <= cmp_rd;
        out_tag_q  <= tag_rd;
        out_full_q <= 1'b1;
        done_rd_q  <= 1'b0;
      end else if (out_pop_c) begin
        out_full_q <= 1'b0;
      end

      // THE CONTEXT IS RELEASED WHEN THE OUTPUT IS ACCEPTED, not when DONE is
      // popped. Section 5 says so in as many words -- "Popping a DONE index
      // does not release the context; release occurs when its held COMBINE
      // output is accepted" -- and the first draft released it at the read,
      // which would let a new fragment overwrite completion storage whose
      // result was still sitting unaccepted in the held register.
      if (out_pop_c) begin
        freeq_m[freeq_wp[CW-1:0]] <= out_ctx_q;
        freeq_wp <= freeq_wp + 1'b1;
      end
    end
  end

endmodule
