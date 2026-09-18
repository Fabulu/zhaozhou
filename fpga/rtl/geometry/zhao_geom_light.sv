// zhao_geom_light.sv — GEOM.LIGHT: the vertex-RGB COMPOSITION (ZH-085).
//
// *** THIS BLOCK CONTAINS NO LIGHT-TERM ARITHMETIC. IT INSTANTIATES ONE. ***
//
// TERRAIN.SHADE.md A7 and GEOM.LIGHT.md's ownership ruling say the same
// thing from both sides: `dot(n,L)/|n|` — ndot, nmag2, isqrt, the ONE
// div_rhu_s128 rounding, the nmag2==0 arm — lives EXACTLY ONCE in RTL, in
// `zhao_terrain_shade.sv`, whose n_x/y/z_i ports are a WORLD NORMAL and
// which has never contained a face-normal stage. GEOM.LIGHT.md:118 records
// that writing `normal -> ndot -> isqrt -> divide` here "would be a second
// implementation of the ratified arithmetic — the exact failure this
// contract was written to prevent, and the one that shipped in September's
// terrain shade header". So this file holds a single `u_shade` instance and
// a sequencer around it, and the only arithmetic it owns is the COLOUR
// FOLD: per-light clamp, gain, emission, ambient/spill, one saturate.
//
// If you are about to add a square root, a divide or a dot product to this
// file, stop: the answer is another turn of `u_shade`.
//
// ENFORCED-BY: tests/geometry/geom_light_directed.cpp:main
//
// ---------------------------------------------------------------------------
// THE RULED COMPOSITION (GEOM.LIGHT.md, owner ruling D-1 2026-09-03)
// ---------------------------------------------------------------------------
//     for each independent light i:
//         raw_i = shade_from_world_normal_unclamped(n, L_i)   <-- u_shade
//         ndl_i = clamp01(raw_i + normal_detail_i)            // THIS light's
//         rgb  += colour_i   * ndl_i                          // multiplicative
//         rgb  += emission_i * ndl_i                          // ADDITIVE, provisional
//     rgb += ambient + spill
//     final = saturate(rgb)                                   // ONCE, at the end
//
// Two placements in there are not negotiable and both are implemented as
// written, not as convenient:
//
//   * THE CLAMP IS ONCE PER LIGHT, NOT ONCE PER VERTEX. An earlier draft of
//     the contract said "sum the additive terms before the clamp"; that is
//     true only WITHIN one light. A second sun's negative dot must become
//     zero and must not SUBTRACT illumination the first sun contributed.
//     `ndl_clamp_lo_o` counts exactly that event, so the ruling is a
//     measured fact rather than a comment.
//   * THE SATURATE IS ONCE, AT THE END. Saturating per source clips each
//     contribution separately and CHANGES THE COLOUR OF AN OVERLAP — two
//     lights meeting produce a hue neither implies. The accumulator is
//     therefore proved wide enough never to wrap (see ACCW below) so that
//     "no intermediate saturation" is structural, not hopeful.
//
// The emission term is PROVISIONAL (owner: "It's fucking beautiful we must
// have it", and on the budget "Our budget is fucked"). Its cut seam is
// designed in: `emission_i = 0` is a bit-exact no-op — with every emission
// word zero this block's output is identical to the multiplicative-only
// path, and reverting means deleting words 7..9 of the descriptor and three
// MAC steps. Nothing else depends on it.
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK REFUSES, each refusal pointing at its real owner
// ---------------------------------------------------------------------------
//   * NO NORMAL PRODUCTION. Three producers feed it and that is the point of
//     the split: zhao_geom_skin_norm (creatures), zhao_terrain_normals
//     (terrain), a rigid transform (static meshes).
//   * NO FOG. GEOM.LIGHT.md says "this block is where fog is computed", and
//     that sentence has been OVERTAKEN: `zhao_geom_fogfactor.sv` exists, is
//     in design/prod_manifest.yml, has its own fit target and its own
//     directed test. Computing a second factor here would be the projector's
//     two-cores mistake with fog's name on it. D-5's transport rule still
//     binds this block, and it is obeyed by omission: the RGB emitted here
//     is UNFOGGED and the factor travels beside it.
//   * NO PER-FRAGMENT WORK, NO SHADOWING, NO TOP-K SELECTION. Selection is
//     the HPS's (D-6); this block is handed the bounded set.
//
// ---------------------------------------------------------------------------
// THE THROUGHPUT POINT, STATED BECAUSE THE CONTRACT DELIBERATELY DOES NOT
// ---------------------------------------------------------------------------
// GEOM.LIGHT.md's Latency section says `fixed`, "once the throughput point
// is chosen. NOT CHOSEN HERE", and the ledger's target_throughput is "see
// contract". So this is the first choice made, and it is made at the
// engine's own rate, which is the CHEAPEST point and not the fastest:
//
//     per light  = LOAD(11) + OFFER(>=1) + u_shade(~147) + MAC(6) ~= 165 clk
//     per vertex = nlights * 165 + FOLD(1) + OUT(>=1)
//                = ~661 clocks at the ruled four local lights.
//
// AGAINST the frame budget that is SLOW, and the number is written here so
// nobody inherits the gap silently: 1,666,666 clocks/frame against 120,000
// skinned vertices/frame is ~13.9 clocks per vertex, and the skinner itself
// runs one weighted vertex per 12 clocks. This block at 661 is ~47x short of
// creature rate. It is exactly at terrain rate (2,000 triangles/frame needs
// 833 clocks each).
//
// THAT IS DELIBERATE AND IT IS THE CONTRACT'S OWN ORDERING. "The accumulator
// does not need ten permanent light engines merely because ten light terms
// are legal — sequenced accumulation at the skinner's own rate is the
// default and parallel engines are a MEASURED ESCALATION, not a starting
// point", on a design at 171% of its DSP budget and ~15x over ALM. The
// escalation levers, in the order they should be spent, are:
//
//   1. a faster `u_shade` throughput point (its own contract says the unused
//      parallelism lives THERE: II=147 was priced for 2,000 triangles, not
//      120,000 vertices);
//   2. N engine instances behind this same sequencer (one localparam and a
//      generate, no law change);
//   3. only then anything structural here.
//
// Spending (2) before (1) buys N copies of a circuit deliberately built
// slow. The subsystem fit gate named in SHADE-LIGHT-CONSOLIDATION §6 is
// where the real ALM/DSP price of this shell gets measured; nothing here
// claims an area number.
//
// ---------------------------------------------------------------------------
// WHERE THE DESCRIPTORS LIVE, AND WHY IT IS NOT FLIP-FLOPS
// ---------------------------------------------------------------------------
// Eight lights x ten 32-bit words is 2,560 bits. Held in registers that is
// ~2,560 flops on the axis that is already the binding constraint; held in
// `ldesc_q` with a registered read and no reset it is one small memory.
// ALMs are the scarce resource here and memory is the slack, so the bank is
// a memory. The sequencer can afford the read latency trivially: it spends
// eleven clocks fetching a descriptor inside a 165-clock light.
//
// ---------------------------------------------------------------------------
// THE CREATURE LANE IS NOT WIRED, ON PURPOSE
// ---------------------------------------------------------------------------
// `reports/SHADE-LIGHT-CONSOLIDATION-20260909.md` §5.4 records an OPEN
// contract question: GEOM.LIGHT.md carries TWO ratified arithmetics — this
// render core (int32 un-normalised normal, signed unclamped, div_rhu_s128)
// and the creature pair skin_world_normal / lambert_from_world_normal (int64
// lanes, PRECOMPUTED magnitude, positive-only floor divide, clamp at 65536).
// They are not bit-identical laws, and zhao_geom_skin_norm emits a
// {direction:s64, magnitude:u64} pair that `u_shade` does not accept.
//
// So this block's input port is the ENGINE's own entry format: an
// un-normalised s32 Q16.16 world normal, from which the engine derives its
// own magnitude. That serves terrain and the rigid path exactly. Wiring
// SKIN.NORM's pair into it would either throw the magnitude away (a second
// isqrt, the rounding the SKIN.NORM contract exists to avoid) or silently
// pick one of the two laws — which is the same two-laws-one-quantity
// decision that caused this whole consolidation. It is an owner/contract
// call, not an implementation detail, and it is left undone and written
// down rather than guessed.
//
// ---------------------------------------------------------------------------
// FORMATS
// ---------------------------------------------------------------------------
//   n_x/y/z_i    s32 Q16.16, UN-NORMALISED world normal (u_shade's own port
//                format; the law is Q16.16, never s1.15 — assuming s1.15 is
//                a factor of two in the result wearing the clothes of a
//                tuning problem).
//   light dir    s32 Q16.16 (the renderer's key light is 26758/53521/26758).
//   detail       s32 Q16.16, THIS light's normal detail, added before clamp.
//   gains        u[GAINW] Q16.16: 0x10000 == 1.0. GAINW=20 gives [0, 16.0),
//                so a gain above unity is legal and an emission can exceed
//                full scale, which is the whole point of the additive term.
//   ndl          u17 Q0.16 in [0, 0x10000].
//   rgb_*_o      u17 Q16.16 in [0, 0x10000], UNFOGGED.
//
// Rounding: round-half-up, ONE rounding per product (spec/qformats.md §3).
// A `>>` floors and disagrees on every value with a fractional half, and
// the gains are unsigned so this is `(p + 32768) >> 16`. THE COLOUR FOLD
// HAS NO RATIFIED ORACLE YET (GEOM.LIGHT.md "Scalar reference function":
// the vertex-RGB half is ruled in prose and unwritten in C++ — the V6
// blocker for REFERENCE_COMPLETE). So §3 is the authority for this choice
// and the choice is stated here so the future oracle matches the silicon
// rather than the other way round.
//
// Conservative SystemVerilog subset (charter §2): elaboration guards inside
// `initial begin` and explicit generate keywords, both Quartus 17 form law.
// A clean lint is NOT synthesizability — this block has not been through
// quartus_map, and `--lint-only` does not run the initial blocks either.
`default_nettype none

module zhao_geom_light #(
    // Descriptor slots. 4 guaranteed for a near receiver, 6 for heroes, 8 is
    // the descriptor maximum (owner ruling D-6). A slot costs memory, not
    // logic, so this is the cheap knob.
    parameter int unsigned LIGHTS_MAX = 8,
    // Gain word width. Q16.16 unsigned: 20 bits is [0, 16.0). THIS IS THE
    // MULTIPLIER WIDTH and therefore the DSP lever — one GAINW x 17 product,
    // sequenced, is the entire multiplicative cost of the block.
    parameter int unsigned GAINW = 20,
    // Accumulator width. Proved below rather than chosen: the fold must not
    // saturate before the end, or two lights meeting change hue.
    parameter int unsigned ACCW = 32,
    parameter int unsigned SRCW = 16,
    parameter int unsigned CNTW = 32,
    // A vertex with no direction: 0 = the law's reading (shade 0, so it
    // receives AMBIENT ONLY — "a dark face, never a bright one",
    // TERRAIN.SHADE.md); 1 = GEOM.SKIN.NORM.md's reading ("light it black").
    // The two sibling contracts disagree and the owner keeps the knob.
    parameter bit DEGEN_BLACK = 1'b0
) (
    input var logic clk,
    input var logic rst_n,

    // ---- light / environment descriptor bank ---------------------------------
    // cfg_addr_i = {light_index[3:0], word[3:0]}; index 4'hF is the
    // environment. A write outside the map is REFUSED and counted, never
    // silently aliased onto a real light.
    //   word  0..2  light direction x, y, z   s32 Q16.16
    //   word  3     normal detail             s32 Q16.16
    //   word  4..6  colour gain    r, g, b    u[GAINW] Q16.16
    //   word  7..9  emission       r, g, b    u[GAINW] Q16.16   (provisional)
    // environment (index 4'hF):
    //   word  0..2  ambient        r, g, b    u[GAINW] Q16.16
    //   word  3..5  spill          r, g, b    u[GAINW] Q16.16
    input var logic        cfg_we_i,
    input var logic [7:0]  cfg_addr_i,
    input var logic [31:0] cfg_data_i,

    // ---- one vertex ----------------------------------------------------------
    input  var logic               v_valid_i,
    output var logic               v_ready_o,
    input  var logic signed [31:0] n_x_i,
    input  var logic signed [31:0] n_y_i,
    input  var logic signed [31:0] n_z_i,
    // The producer's own degeneracy verdict. It rides into u_shade's
    // degenerate_i so the engine's seam check keeps working; a producer that
    // has no such flag must NOT tie this low and then read the resulting
    // seam_mismatch_o as a defect (TERRAIN.SHADE.md A7's closing note).
    input  var logic               n_degenerate_i,
    input  var logic [3:0]         nlights_i,
    input  var logic [SRCW-1:0]    src_id_i,

    // ---- the lit vertex, UNFOGGED -------------------------------------------
    output var logic            r_valid_o,
    input  var logic            r_ready_i,
    output var logic [16:0]     rgb_r_o,
    output var logic [16:0]     rgb_g_o,
    output var logic [16:0]     rgb_b_o,
    output var logic            degenerate_vtx_o,
    output var logic [SRCW-1:0] src_id_o,

    // ---- evidence ------------------------------------------------------------
    output var logic [CNTW-1:0] vertices_lit_o,      // ledger: lit_vertices
    output var logic [CNTW-1:0] degenerate_o,        // ledger: degenerate_normals
    // Engine turns. A test that checks WHAT came out cannot see HOW MANY
    // TIMES the machine did it, and this block's whole economy is that the
    // count is exactly sum(nlights) and never more.
    output var logic [CNTW-1:0] light_terms_o,
    output var logic [CNTW-1:0] ndl_clamp_lo_o,      // D-1: a light clamped to 0
    output var logic [CNTW-1:0] ndl_clamp_hi_o,      // detail pushed past 1.0
    output var logic [CNTW-1:0] rgb_sat_o,           // the ONE final saturate
    output var logic [CNTW-1:0] cfg_refused_o,       // write outside the map
    output var logic [CNTW-1:0] nlights_clamped_o,   // asked for more than exist
    // Forwarded from u_shade, not recomputed: the producer flag disagreed
    // with the law's own nmag2==0 walk. Its two operands arrive by
    // INDEPENDENT paths, which is what lets it fire at all.
    output var logic [CNTW-1:0] seam_mismatch_o,
    // The engine's OWN counters, forwarded rather than duplicated. These are
    // the far side of an independent-path check and that is why they are
    // ports: `light_terms_o` counts offers ACCEPTED by this sequencer and
    // `engine_shaded_o` counts walks COMPLETED inside the engine. One
    // register enable does not drive both, so a sequencer that reissued a
    // light, or an engine that dropped one, separates them. Two counters
    // clocked by the same enable could not see either fault.
    output var logic [CNTW-1:0] engine_shaded_o,
    output var logic [CNTW-1:0] engine_degen_o,
    output var logic [CNTW-1:0] engine_base_sat_o,

    output var logic table_ready_o,
    output var logic idle_o
);

  // ---- named shapes --------------------------------------------------------
  localparam int unsigned LIDXW     = (LIGHTS_MAX <= 1) ? 1 : $clog2(LIGHTS_MAX);
  localparam int unsigned DAW       = LIDXW + 4;          // {index, word}
  localparam int unsigned DEPTH     = 1 << DAW;
  localparam int unsigned LWORDS    = 10;                 // words per light
  localparam int unsigned EWORDS    = 6;                  // ambient rgb + spill rgb
  localparam int unsigned MAC_STEPS = 6;                  // 3 colour + 3 emission
  localparam int unsigned NDLW      = 17;                 // [0, 0x10000]
  localparam logic [NDLW-1:0] NDL_ONE = 17'h1_0000;
  localparam logic [3:0] ENV_IDX = 4'hF;

  // ---- elaboration guards --------------------------------------------------
  // Quartus 17 rejects a bare module-scope `if`; these MUST be inside
  // `initial begin`. And `--lint-only` does not run initial blocks, so a
  // clean lint says nothing whatever about these — the simulation run is
  // what executes them.
  initial begin
    if (LIGHTS_MAX < 1 || LIGHTS_MAX > 8)
      $fatal(1, "zhao_geom_light: LIGHTS_MAX outside 1..8 (D-6 caps the descriptor at 8)");
    if (GAINW < 17 || GAINW > 32)
      $fatal(1, "zhao_geom_light: GAINW outside 17..32; below 17 a unit gain is unrepresentable");
    // THE NO-INTERMEDIATE-SATURATION PROOF, as an elaboration check rather
    // than a comment. Every term is < 2^GAINW (gain < 2^GAINW, ndl <= 2^16,
    // rescale by 16), and a channel sums 2*LIGHTS_MAX of them plus ambient
    // plus spill. If ACCW cannot hold that, the fold wraps and two lights
    // meeting produce a DARKER vertex than one — the exact failure the
    // saturate-once ruling forbids.
    if (ACCW < GAINW + $clog2(2 * LIGHTS_MAX + 2) + 1)
      $fatal(1, "zhao_geom_light: ACCW too narrow for LIGHTS_MAX/GAINW; the fold would wrap");
    if (CNTW < 8) $fatal(1, "zhao_geom_light: CNTW below 8 makes the counters decorative");
    if (LWORDS > 16) $fatal(1, "zhao_geom_light: descriptor does not fit the 4-bit word field");
  end

  // ---- state ---------------------------------------------------------------
  typedef enum logic [2:0] {
    ST_IDLE  = 3'd0,
    ST_LOAD  = 3'd1,  // fetch this light's 10 descriptor words
    ST_OFFER = 3'd2,  // hand n and L to the shared engine
    ST_WAIT  = 3'd3,  // the engine's ~147 clocks
    ST_MAC   = 3'd4,  // 6 sequenced products into the accumulator
    ST_FOLD  = 3'd5,  // + ambient + spill, ONE saturate
    ST_OUT   = 3'd6
  } state_e;

  state_e st_q;

  // ---- descriptor bank -----------------------------------------------------
  // No reset on the array and a registered read: the two things that let it
  // infer a memory instead of DEPTH*32 flops.
  logic [31:0] ldesc_q [0:DEPTH-1];
  logic [31:0] rd_q;
  logic [DAW-1:0] rd_addr_c;

  logic [3:0] cfg_idx_c, cfg_word_c;
  logic       cfg_light_ok_c, cfg_env_ok_c, cfg_ok_c;
  logic [DAW-1:0] cfg_wa_c;

  always_comb begin
    cfg_idx_c      = cfg_addr_i[7:4];
    cfg_word_c     = cfg_addr_i[3:0];
    cfg_light_ok_c = (cfg_idx_c < 4'(LIGHTS_MAX)) && (cfg_word_c < 4'(LWORDS));
    cfg_env_ok_c   = (cfg_idx_c == ENV_IDX) && (cfg_word_c < 4'(EWORDS));
    cfg_ok_c       = cfg_light_ok_c || cfg_env_ok_c;
    cfg_wa_c       = {cfg_idx_c[LIDXW-1:0], cfg_word_c};
  end

  // The environment is six words and is read on the fold cycle for all three
  // channels at once, so it stays in registers; putting it in the bank would
  // cost six more sequencer cycles to save 6*GAINW bits.
  logic [GAINW-1:0] amb_q [0:2];
  logic [GAINW-1:0] spl_q [0:2];

  // ---- captured vertex -----------------------------------------------------
  logic signed [31:0] nx_q, ny_q, nz_q;
  logic               ndegen_q;
  logic [3:0]         nlights_q;
  logic [SRCW-1:0]    src_q;
  // FOUR bits, not LIDXW. `nlights_q` reaches LIGHTS_MAX == 8 while a light
  // INDEX only reaches 7, so the loop test `li+1 < nlights` has to be done
  // one bit wider than the index or it wraps at exactly the full descriptor
  // set — the one configuration the block is sized for.
  logic [3:0]         li_q;
  logic               degen_law_q;   // this vertex has no direction

  // ---- descriptor fetch ----------------------------------------------------
  logic [3:0] ld_q;        // 0..LWORDS, the word being ISSUED
  logic [3:0] ld_word_q;   // the word whose data is arriving THIS cycle
  logic       ld_v_q;

  logic signed [31:0] lx_q, ly_q, lz_q, det_q;
  logic [GAINW-1:0]   col_q [0:2];
  logic [GAINW-1:0]   emi_q [0:2];

  // ---- the SHARED ENGINE, one instance ------------------------------------
  logic               eng_valid_c;
  logic               eng_ready_w;
  logic               eng_base_valid_w;
  logic               eng_base_ready_c;
  logic signed [31:0] eng_base_w;
  logic               eng_degen_w;
  logic [15:0]        eng_src_w;
  logic [CNTW-1:0]    eng_tri_w, eng_degcnt_w, eng_sat_w, eng_mismatch_w;
  logic               eng_table_w, eng_idle_w;

  assign eng_valid_c      = (st_q == ST_OFFER);
  assign eng_base_ready_c = (st_q == ST_WAIT);

  zhao_terrain_shade #(
      .CNTW(CNTW)
  ) u_shade (
      .clk  (clk),
      .rst_n(rst_n),

      .tri_valid_i (eng_valid_c),
      .tri_ready_o (eng_ready_w),
      .n_x_i       (nx_q),
      .n_y_i       (ny_q),
      .n_z_i       (nz_q),
      .degenerate_i(ndegen_q),
      .sun_x_i     (lx_q),
      .sun_y_i     (ly_q),
      .sun_z_i     (lz_q),
      .src_id_i    (16'(src_q)),

      .base_valid_o(eng_base_valid_w),
      .base_ready_i(eng_base_ready_c),
      .base_o      (eng_base_w),
      .degenerate_o(eng_degen_w),
      .src_id_o    (eng_src_w),

      .triangles_shaded_o(eng_tri_w),
      .degenerate_count_o(eng_degcnt_w),
      .base_sat_o        (eng_sat_w),
      .degen_mismatch_o  (eng_mismatch_w),
      .table_ready_o     (eng_table_w),
      .idle_o            (eng_idle_w)
  );

  assign seam_mismatch_o   = eng_mismatch_w;
  assign engine_shaded_o   = eng_tri_w;
  assign engine_degen_o    = eng_degcnt_w;
  assign engine_base_sat_o = eng_sat_w;
  assign table_ready_o     = eng_table_w;

  // ---- clamp01(raw + detail), ONCE PER LIGHT -------------------------------
  // s33 so a rail-to-rail sum cannot wrap before the clamp sees it: a wrap
  // here would look exactly like a legitimate small ndl, i.e. a face that
  // quietly stops catching light.
  logic signed [32:0]  ndl_sum_c;
  logic [NDLW-1:0]     ndl_c;
  logic                ndl_lo_c, ndl_hi_c;
  logic                eng_degen_now_c;

  always_comb begin
    eng_degen_now_c = eng_degen_w;
    ndl_sum_c       = 33'(eng_base_w) + 33'(det_q);
    ndl_lo_c        = !eng_degen_now_c && (ndl_sum_c < 33'sd0);
    ndl_hi_c        = !eng_degen_now_c && (ndl_sum_c > 33'sh1_0000);
    if (eng_degen_now_c)              ndl_c = '0;  // no direction, no light term
    else if (ndl_sum_c <= 33'sd0)     ndl_c = '0;
    else if (ndl_sum_c >= 33'sh1_0000) ndl_c = NDL_ONE;
    else                              ndl_c = ndl_sum_c[NDLW-1:0];
  end

  logic [NDLW-1:0] ndl_q;

  // ---- the ONE multiplier, sequenced --------------------------------------
  // GAINW x 17, six turns per light. K legal lights does not mean K light
  // engines, and three channels times two terms does not mean six
  // multipliers; this is the sentence that separates affordable from a
  // shader core, and it is spent here.
  logic [2:0]              mi_q;
  logic [1:0]              mac_ch_c;
  logic [GAINW-1:0]        mac_gain_c;
  logic [GAINW+NDLW-1:0]   mac_prod_c;
  logic [GAINW:0]          mac_term_c;

  always_comb begin
    mac_ch_c   = (mi_q < 3'd3) ? 2'(mi_q) : 2'(mi_q - 3'd3);
    mac_gain_c = (mi_q < 3'd3) ? col_q[mac_ch_c] : emi_q[mac_ch_c];
    mac_prod_c = mac_gain_c * {{(GAINW-1){1'b0}}, ndl_q};
    // ONE rounding per product, round-half-up (spec/qformats.md §3).
    mac_term_c = (GAINW+1)'((mac_prod_c + (GAINW+NDLW)'(32768)) >> 16);
  end

  logic [ACCW-1:0] acc_q [0:2];

  // ---- the fold: + ambient + spill, ONE saturate ---------------------------
  logic [ACCW-1:0] fold_c [0:2];
  logic            fold_sat_c;

  always_comb begin
    fold_sat_c = 1'b0;
    for (int unsigned c = 0; c < 3; c++) begin
      fold_c[c] = acc_q[c] + ACCW'(amb_q[c]) + ACCW'(spl_q[c]);
      if (fold_c[c] > ACCW'(NDL_ONE)) fold_sat_c = 1'b1;
    end
  end

  function automatic logic [NDLW-1:0] sat01(input logic [ACCW-1:0] v);
    sat01 = (v > ACCW'(NDL_ONE)) ? NDL_ONE : v[NDLW-1:0];
  endfunction

  // ---- descriptor read address --------------------------------------------
  assign rd_addr_c = {li_q[LIDXW-1:0], ld_q};

  // ---- handshakes ----------------------------------------------------------
  // The engine's quarter-square table fills for 512 cycles after reset and
  // gates its own ready; this block must not offer a vertex it cannot start.
  assign v_ready_o = (st_q == ST_IDLE) && eng_table_w;
  assign r_valid_o = (st_q == ST_OUT);
  // Idle means the SHELL and the engine it holds are both quiet; a shell
  // that reports idle while its engine is mid-walk would let a caller
  // conclude the pipeline had drained when a result is still coming.
  assign idle_o    = (st_q == ST_IDLE) && eng_idle_w;

  logic [3:0] nlights_eff_c;
  assign nlights_eff_c = (nlights_i > 4'(LIGHTS_MAX)) ? 4'(LIGHTS_MAX) : nlights_i;

  // ---- the descriptor memory: write port + registered read -----------------
  always_ff @(posedge clk) begin
    if (cfg_we_i && cfg_light_ok_c) ldesc_q[cfg_wa_c] <= cfg_data_i;
    rd_q <= ldesc_q[rd_addr_c];
  end

  // ---- sequencer -----------------------------------------------------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_q      <= ST_IDLE;
      nx_q      <= '0;
      ny_q      <= '0;
      nz_q      <= '0;
      ndegen_q  <= 1'b0;
      nlights_q <= '0;
      src_q     <= '0;
      li_q      <= '0;
      degen_law_q <= 1'b0;
      ld_q      <= '0;
      ld_word_q <= '0;
      ld_v_q    <= 1'b0;
      lx_q      <= '0;
      ly_q      <= '0;
      lz_q      <= '0;
      det_q     <= '0;
      ndl_q     <= '0;
      mi_q      <= '0;
      col_q[0] <= '0; col_q[1] <= '0; col_q[2] <= '0;
      emi_q[0] <= '0; emi_q[1] <= '0; emi_q[2] <= '0;
      amb_q[0] <= '0; amb_q[1] <= '0; amb_q[2] <= '0;
      spl_q[0] <= '0; spl_q[1] <= '0; spl_q[2] <= '0;
      acc_q[0] <= '0; acc_q[1] <= '0; acc_q[2] <= '0;
      rgb_r_o   <= '0;
      rgb_g_o   <= '0;
      rgb_b_o   <= '0;
      degenerate_vtx_o <= 1'b0;
      src_id_o  <= '0;
      vertices_lit_o    <= '0;
      degenerate_o      <= '0;
      light_terms_o     <= '0;
      ndl_clamp_lo_o    <= '0;
      ndl_clamp_hi_o    <= '0;
      rgb_sat_o         <= '0;
      cfg_refused_o     <= '0;
      nlights_clamped_o <= '0;
    end else begin
      ld_v_q <= 1'b0;

      // ---- environment writes, and the refusal count --------------------
      if (cfg_we_i) begin
        if (cfg_env_ok_c) begin
          if (cfg_word_c < 4'd3) amb_q[cfg_word_c[1:0]]           <= cfg_data_i[GAINW-1:0];
          else                   spl_q[2'(cfg_word_c - 4'd3)]     <= cfg_data_i[GAINW-1:0];
        end
        // A write outside the map is REFUSED — never folded onto light 0,
        // which is what a masked index would silently do.
        if (!cfg_ok_c) cfg_refused_o <= cfg_refused_o + CNTW'(1);
      end

      case (st_q)
        // ---- accept -------------------------------------------------------
        ST_IDLE: begin
          if (v_valid_i && eng_table_w) begin
            nx_q      <= n_x_i;
            ny_q      <= n_y_i;
            nz_q      <= n_z_i;
            ndegen_q  <= n_degenerate_i;
            nlights_q <= nlights_eff_c;
            src_q     <= src_id_i;
            li_q      <= '0;
            ld_q      <= '0;
            mi_q      <= '0;
            acc_q[0]  <= '0;
            acc_q[1]  <= '0;
            acc_q[2]  <= '0;
            // With no light evaluated the LAW is never asked, so the
            // producer's flag is the only verdict available. With one or
            // more, the engine's own nmag2==0 walk overrides it below.
            degen_law_q <= n_degenerate_i;
            if (nlights_i > 4'(LIGHTS_MAX)) nlights_clamped_o <= nlights_clamped_o + CNTW'(1);
            st_q <= (nlights_eff_c == 4'd0) ? ST_FOLD : ST_LOAD;
          end
        end

        // ---- fetch this light's descriptor --------------------------------
        // The address and the capture selector are the SAME counter one
        // cycle apart, so the word that arrives and the register it lands in
        // cannot separate under any stall — this state does not stall.
        ST_LOAD: begin
          if (ld_q < 4'(LWORDS)) begin
            ld_v_q    <= 1'b1;
            ld_word_q <= ld_q;
            ld_q      <= ld_q + 4'd1;
          end else begin
            st_q <= ST_OFFER;
          end
        end

        // ---- hand the packet to the shared engine -------------------------
        ST_OFFER: begin
          if (eng_ready_w) begin
            st_q          <= ST_WAIT;
            light_terms_o <= light_terms_o + CNTW'(1);
          end
        end

        // ---- the engine's fixed walk --------------------------------------
        ST_WAIT: begin
          if (eng_base_valid_w) begin
            ndl_q       <= ndl_c;
            degen_law_q <= eng_degen_w;
            if (ndl_lo_c) ndl_clamp_lo_o <= ndl_clamp_lo_o + CNTW'(1);
            if (ndl_hi_c) ndl_clamp_hi_o <= ndl_clamp_hi_o + CNTW'(1);
            mi_q <= '0;
            st_q <= ST_MAC;
          end
        end

        // ---- six sequenced products into the accumulator ------------------
        ST_MAC: begin
          acc_q[mac_ch_c] <= acc_q[mac_ch_c] + ACCW'(mac_term_c);
          if (mi_q == 3'(MAC_STEPS - 1)) begin
            mi_q <= '0;
            if ((li_q + 4'd1) < nlights_q) begin
              li_q <= li_q + 4'd1;
              ld_q <= '0;
              st_q <= ST_LOAD;
            end else begin
              st_q <= ST_FOLD;
            end
          end else begin
            mi_q <= mi_q + 3'd1;
          end
        end

        // ---- + ambient + spill, ONE saturate ------------------------------
        ST_FOLD: begin
          if (DEGEN_BLACK && degen_law_q) begin
            rgb_r_o <= '0;
            rgb_g_o <= '0;
            rgb_b_o <= '0;
          end else begin
            rgb_r_o <= sat01(fold_c[0]);
            rgb_g_o <= sat01(fold_c[1]);
            rgb_b_o <= sat01(fold_c[2]);
            if (fold_sat_c) rgb_sat_o <= rgb_sat_o + CNTW'(1);
          end
          degenerate_vtx_o <= degen_law_q;
          src_id_o         <= src_q;
          vertices_lit_o   <= vertices_lit_o + CNTW'(1);
          if (degen_law_q) degenerate_o <= degenerate_o + CNTW'(1);
          st_q <= ST_OUT;
        end

        // ---- hold until the consumer takes it -----------------------------
        ST_OUT: begin
          if (r_ready_i) st_q <= ST_IDLE;
        end

        default: st_q <= ST_IDLE;
      endcase

      // ---- descriptor word landing --------------------------------------
      if (ld_v_q) begin
        case (ld_word_q)
          4'd0:    lx_q     <= $signed(rd_q);
          4'd1:    ly_q     <= $signed(rd_q);
          4'd2:    lz_q     <= $signed(rd_q);
          4'd3:    det_q    <= $signed(rd_q);
          4'd4:    col_q[0] <= rd_q[GAINW-1:0];
          4'd5:    col_q[1] <= rd_q[GAINW-1:0];
          4'd6:    col_q[2] <= rd_q[GAINW-1:0];
          4'd7:    emi_q[0] <= rd_q[GAINW-1:0];
          4'd8:    emi_q[1] <= rd_q[GAINW-1:0];
          default: emi_q[2] <= rd_q[GAINW-1:0];
        endcase
      end
    end
  end

`ifndef SYNTHESIS
  logic armed_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) armed_q <= 1'b0;
    else        armed_q <= 1'b1;
  end
  always_ff @(posedge clk) begin
    if (armed_q) begin
      // The emitted colour NEVER leaves [0, 1.0]. A channel above full scale
      // downstream is a colour that brightens where it should clip.
      if (r_valid_o) begin
        a_rgb_r_in_range : assert (rgb_r_o <= NDL_ONE);
        a_rgb_g_in_range : assert (rgb_g_o <= NDL_ONE);
        a_rgb_b_in_range : assert (rgb_b_o <= NDL_ONE);
      end
      // The packet the engine hands back is the one this block offered. The
      // engine holds one packet at a time, so a mismatch means the sequencer
      // released a result that was not its own.
      if (st_q == ST_WAIT && eng_base_valid_w)
        a_engine_packet_is_ours : assert (eng_src_w == 16'(src_q));
      // ndl is the clamp's own range, per light, before any accumulation.
      if (st_q == ST_MAC) a_ndl_in_range : assert (ndl_q <= NDL_ONE);
    end
  end
`endif

endmodule : zhao_geom_light

`default_nettype wire
