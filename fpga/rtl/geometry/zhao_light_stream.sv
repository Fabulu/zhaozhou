// zhao_light_stream.sv -- GEOM.LIGHT as a STREAMED service: one light term
// every two clocks, one exact magnitude per normal, enough descriptor and
// colour bandwidth to keep both fed.
//
// THIS REPLACES THE OWNER, IT DOES NOT ADD A SECOND LAW
// ---------------------------------------------------------------------------
// `zhao_geom_light.sv` is the scalar arrangement: one `zhao_terrain_shade`
// turn per light term, MEASURED at II = 167.0 clocks, which is 48.1x over the
// frame for the ruled 120,000-vertex / 480,000-term stress profile. Its own
// header says so and names the levers. This file spends them.
//
// The law is UNCHANGED. What changed is the interface it is reached through,
// and the two adapters that reach it:
//
//   raw      = sat_s32( floor( (dot(n,L) + floor(|n|/2)) / |n| ) )    <- shared
//   render   : ndl = clamp01(raw + detail_of_this_light)
//   creature : ndl = clamp01(raw)
//
// Those two ARE the same quotient. `reports/LIGHTING-BLOCKER-RETIRED-20260918.md`
// works it through; the short form is that `lambert_from_world_normal` reads
// `lam = (dot + mag/2)/mag` with an early `dot <= 0 -> 0`, and for dot <= 0
// the renderer's raw quotient is already <= 0 and clamps to the same zero. The
// long-standing "the creature law is floor-only" description was STALE; the
// C++ has said round-half-up all along. So these are adapters, not engines.
//
// THE TERRAIN DETAIL MUST NOT BE CLAMPED EARLY. `clamp01(raw + detail)` and
// `clamp01(raw) + detail` are different functions -- raw = -1, detail = 2
// gives 1 and 2. The creature profile's early clamp is NOT pushed onto the
// render path, and the bench pins that exact pair as a directed case.
//
// ---------------------------------------------------------------------------
// THE SCHEDULE, WHICH IS THE WHOLE DESIGN
// ---------------------------------------------------------------------------
// Two clocks per light term. Everything below is sized from that.
//
//   clock  lane 0 (s32 x s32)   lane 1 (s32 x s32)       descriptor port
//    2t    nx*lx                ny*ly                    one 128-bit read
//    2t+1  nz*lz                a normal square, or idle one 128-bit read
//
// * PRODUCTS. Three dot products per term across two lanes over two clocks
//   leaves exactly ONE lane-1 slot per term. A four-light vertex therefore
//   offers four spare slots and needs three squares -- the emergency report's
//   eight-clock calendar, with its one spare. At ONE light per vertex there is
//   one spare slot against three squares needed and PREPARATION becomes the
//   bottleneck. That is a real property of this arrangement, it is counted on
//   `normal_queue_wait_o`, and it is not hidden behind an average.
//
// * ROOTS. One exact floor root per NORMAL, never per term, at II8. For the
//   ruled fixture that is 120,000 roots inside a 960,000-clock term schedule:
//   100.0% occupancy, with no margin whatsoever. An elastic queue sits between
//   the square accumulator and the root precisely because a fixed phase offset
//   between two things both running at one per eight clocks would otherwise
//   halve the root's throughput permanently.
//
// * DESCRIPTORS. The scalar block reads TEN 32-bit words serially per light.
//   Even an ideal ten-clock walk is 4,800,000 clocks over 480,000 terms, so a
//   fast divider behind it would simply wait. Here the record is PREPARED and
//   split into two 128-bit halves:
//       half A: light direction s32 x3 + detail s32     (128 bits)
//       half B: six GAINW coefficients + flags          (128 bits)
//   held as four 32-bit banks read together, depth 32 (two generations x
//   eight lights x two halves). Half A is fetched AHEAD of issue through a
//   short queue; half B is fetched LATE, by the light-set handle carried with
//   the result, so 120 coefficient bits never travel through the divider.
//   Exactly one 128-bit read per clock on average at II2 -- one port.
//
//   The prefetch is a QUEUE and not a single register on purpose: with one
//   register the read latency lands on the same clock as the consume and
//   every other term takes a bubble, which turns II2 into II3 silently.
//
//   GAINW is pinned at 20 by an elaboration guard rather than truncated. Six
//   fields at 24 bits is 144 and the record does not exist; refusing to
//   elaborate is the honest form of "a nondefault wider GAINW requires a
//   newly priced record and port schedule, not truncation".
//
// * COLOUR. Six separately rounded products per term cannot share one
//   multiplier at II2 -- 2,880,000 product issues misses the raw frame on its
//   own. THREE narrow lanes: RGB gain on one clock, RGB emission on the next.
//
//   AND THE TWO PRODUCTS ARE NOT FUSED. `rhu(c*ndl) + rhu(e*ndl)` is not
//   `rhu((c+e)*ndl)`: with c = e = 1 and ndl = 32768 the first is 2 and the
//   second is 1. The existing per-product round-half-up is kept and the bench
//   pins that exact pair.
//
// * EPOCHS. A light set is published atomically: writes land in the SHADOW
//   generation, `cfg_commit_i` publishes it. The old set stays PINNED while
//   any term still references it -- a write or a commit that would disturb a
//   generation with live terms is REFUSED and counted. "Latest descriptor" is
//   not a valid identity for an in-flight term, and a refusal enforces that
//   rather than asserting it.
//
// ---------------------------------------------------------------------------
// COUNTING, AND THE ONE PLACE MAGNITUDE REUSE COULD CORRUPT IT
// ---------------------------------------------------------------------------
// Computing |n| once per normal instead of once per term divides the ROOT work
// by K. It must not divide the EVIDENCE by K. So the per-term facts stay
// per-term: `logical_raw_saturations_o` and `degenerate_terms_o` count light
// terms, exactly as the scalar engine's did, while `normal_prepared_o`,
// `roots_issued_o` and `supplied_mags_o` are the new per-normal counters that
// make the saving visible. A second 64-step divider synthesized to reproduce
// diagnostic data would be the cure being worse than the disease.
//
// `tag_mismatch_o` is the identity check with genuinely independent operands:
// the tag travels through the divider's sixteen folded stages, the side entry
// travels through a FIFO beside it. One register enable does not drive both,
// which is the only reason the comparison can ever fire.
//
// ---------------------------------------------------------------------------
// WHAT THIS ARRANGEMENT DOES NOT PROMISE
// ---------------------------------------------------------------------------
// * EIGHT LIGHTS ON EVERY VERTEX IS A DIFFERENT WORKLOAD AND IT IS OVER
//   BUDGET. 120,000 x 8 = 960,000 terms = 1,920,000 issue clocks, which fails
//   even the unreserved 1,666,666-clock frame. Eight descriptors remain
//   supported; the bench runs that fixture and asserts it is OVER, because
//   "supports eight lights" quietly standing in for "meets rate at eight
//   lights" is how an admitted workload gets relabelled instead of solved.
// * NO POINT-LIGHT DIRECTION, ATTENUATION, TOP-K SELECTION OR SHADOWING. This
//   block is handed directions, exactly as the scalar one was. A direction
//   that varies per vertex must be produced and transported at rate by
//   somebody, and caching a constant directional descriptor does not make a
//   point light constant. The dot core accepts either form unchanged.
// * NO FIT. Area, Fmax, DSP mapping and RAM inference are unmeasured. The
//   multiplier count is stated exactly -- FIVE operators, two s32 x s32 and
//   three GAINW x 17 -- because that is countable from this source. How
//   Quartus maps them is not, and this header does not guess.
//
// Quartus 17 form law: elaboration guards inside `initial begin ... end`, no
// implicit generates. Form is not proof -- a clean `verilator --lint-only`
// settles one tool's opinion about syntax, `--lint-only` does not even run the
// `initial` blocks, and this block has not been through `quartus_map`.
`default_nettype none

module zhao_light_stream #(
    parameter int unsigned LIGHTS_MAX = 8,
    parameter int unsigned GAINW      = 20,
    parameter int unsigned ACCW       = 32,
    parameter int unsigned SRCW       = 16,
    parameter int unsigned CNTW       = 32,
    // Prepared-normal arena depth. The emergency report: "Start with 16
    // bounded prepared-normal/context slots and measure whether fewer
    // suffice." It must cover the root pipeline (33 clocks of latency plus
    // four jobs in flight) AND the divider pipeline (33 clocks) at eight
    // clocks per four-light vertex -- about nine live vertices at the ruled
    // fixture. Sixteen is a measured-adequate bound, not a derived minimum.
    parameter int unsigned NSLOTS     = 16,
    // 0 = the law's reading (no direction -> shade 0, so AMBIENT ONLY);
    // 1 = GEOM.SKIN.NORM.md's reading ("light it black"). The two sibling
    // contracts disagree and the owner keeps the knob.
    parameter bit DEGEN_BLACK = 1'b0,
    // ---- TWO CONTROLS THAT EXIST SO THAT TWO GUARDS CAN BE FIRED ----------
    // Both guards below are UNREACHABLE while this block is correct, so no
    // legal stimulus can move their counters and "it can fire" would stay an
    // argument forever. CLAUDE.md's rule is a COMMITTED MUTANT rather than a
    // temporary edit -- and a mutant that is a WRAPPER cannot go stale the way
    // a copied body does. These two parameters are what the wrappers in
    // 	ests/mutants/zhao_light_stream_guard_mutants.sv turn.
    //
    // THE ONLY SAFE VALUES ARE THE DEFAULTS. Production must not override them.
    //
    // RQ_ROOM_MARGIN: how much room the root-request ring must show at the
    // moment a square GROUP STARTS. It must be 3 and not 1, because the
    // admission test runs three or more clocks before this group's push and
    // the PREVIOUS group's push may still be in flight. Setting it to 1 is the
    // real defect this block shipped with once: entry N+SQQD overwrote entry N,
    // four normals were sealed with another normal's magnitude, and the machine
    // wedged with a full arena. Every emitted colour was plausible.
    // SQ_QUEUE_DEPTH: the root-request ring. Eight, not four: the original
    // four-deep ring is what made the admission slip reachable in the first
    // place, and the positive control needs BOTH the shallow ring and the
    // thin margin to reproduce the shipped defect exactly.
    parameter int unsigned SQ_QUEUE_DEPTH = 8,
    parameter int unsigned RQ_ROOM_MARGIN = 3,
    // SIDE_SKEW: offsets the side-channel read against the divider's own tag.
    // Nonzero injects exactly the join skew 	ag_mismatch_o exists to catch.
    parameter int unsigned SIDE_SKEW = 0
) (
    input var logic clk,
    input var logic rst_n,

    // ---- prepared descriptor bank -------------------------------------------
    // cfg_addr_i = {light[3:0], half[0], word[2:0]}; light 4'hF is the
    // environment (words 0..5, half must be 0). Writes land in the SHADOW
    // generation; `cfg_commit_i` publishes it.
    //
    // half A word 0..3 : lx, ly, lz, detail                 (s32 Q16.16 each)
    // half B word 0..3 : the 128-bit coefficient record, little-endian
    //                    {8'flags, eb, eg, er, cb, cg, cr} at GAINW = 20
    // environment      : words 0..2 ambient rgb, 3..5 spill rgb (u[GAINW])
    input  var logic        cfg_we_i,
    input  var logic        cfg_commit_i,
    input  var logic [7:0]  cfg_addr_i,
    input  var logic [31:0] cfg_data_i,
    output var logic        cfg_gen_o,

    // ---- prepared normal in (both adapters land here) ------------------------
    // `n_mag_valid_i` HIGH is the CREATURE path: the magnitude arrived with the
    // direction, no root job is issued, and no detail term is admitted. LOW is
    // the renderer/rigid path: the sum of squares and the floor root are
    // computed here, ONCE.
    input  var logic               v_valid_i,
    output var logic               v_ready_o,
    input  var logic signed [31:0] n_x_i,
    input  var logic signed [31:0] n_y_i,
    input  var logic signed [31:0] n_z_i,
    input  var logic               n_mag_valid_i,
    input  var logic [31:0]        n_mag_i,
    input  var logic               n_degenerate_i,
    input  var logic               n_profile_i,   // 0 = render, 1 = creature
    input  var logic [3:0]         n_lights_i,
    input  var logic [SRCW-1:0]    n_src_id_i,

    // ---- the lit vertex, UNFOGGED -------------------------------------------
    output var logic            r_valid_o,
    input  var logic            r_ready_i,
    output var logic [16:0]     rgb_r_o,
    output var logic [16:0]     rgb_g_o,
    output var logic [16:0]     rgb_b_o,
    output var logic            degenerate_vtx_o,
    output var logic [SRCW-1:0] src_id_o,

    // ---- evidence ------------------------------------------------------------
    output var logic [CNTW-1:0] normal_inputs_o,
    output var logic [CNTW-1:0] normal_prepared_o,
    output var logic [CNTW-1:0] roots_issued_o,
    output var logic [CNTW-1:0] roots_retired_o,
    output var logic [CNTW-1:0] supplied_mags_o,
    output var logic [CNTW-1:0] terms_accepted_o,
    output var logic [CNTW-1:0] terms_retired_o,
    output var logic [CNTW-1:0] terms_null_o,
    output var logic [CNTW-1:0] normal_queue_wait_o,
    output var logic [CNTW-1:0] descriptor_wait_o,
    output var logic [CNTW-1:0] dot_product_slots_o,
    output var logic [CNTW-1:0] square_product_slots_o,
    output var logic [CNTW-1:0] unused_product_slots_o,
    output var logic [CNTW-1:0] divider_backpressure_o,
    output var logic [CNTW-1:0] colour_backpressure_o,
    output var logic [CNTW-1:0] output_backpressure_o,
    output var logic [CNTW-1:0] epoch_refusals_o,
    output var logic [CNTW-1:0] logical_raw_saturations_o,
    output var logic [CNTW-1:0] degenerate_terms_o,
    output var logic [CNTW-1:0] vertices_lit_o,
    output var logic [CNTW-1:0] degenerate_o,
    output var logic [CNTW-1:0] ndl_clamp_lo_o,
    output var logic [CNTW-1:0] ndl_clamp_hi_o,
    output var logic [CNTW-1:0] rgb_sat_o,
    output var logic [CNTW-1:0] cfg_refused_o,
    output var logic [CNTW-1:0] nlights_clamped_o,
    output var logic [CNTW-1:0] seam_mismatch_o,
    output var logic [CNTW-1:0] tag_mismatch_o,
    output var logic [CNTW-1:0] root_queue_overflow_o,

    output var logic idle_o
);

  // ---- named shapes ---------------------------------------------------------
  localparam int unsigned SLOTW  = (NSLOTS <= 2) ? 1 : $clog2(NSLOTS);
  localparam int unsigned DDEPTH = 32;
  localparam int unsigned NDLW   = 17;
  localparam logic [NDLW-1:0] NDL_ONE = 17'h1_0000;
  localparam int unsigned TAGW   = 16;
  localparam int unsigned DIVQD  = 8;
  localparam int unsigned SQQD   = SQ_QUEUE_DEPTH;
  localparam int unsigned RQW    = (SQQD <= 2) ? 1 : $clog2(SQQD);
  localparam int unsigned SIDED  = 64;
  localparam int unsigned OUTQD  = 8;
  localparam int unsigned PFAD   = 4;
  localparam logic [3:0] ENV_IDX = 4'hF;

  // The tag layout lives in ONE place, here, and both the packer and every
  // unpacker use these names. A tag whose fields are sliced by hand in three
  // places is a rename away from a silent remap of light onto slot.
  localparam int unsigned TG_LI   = SLOTW;        // 3 bits
  localparam int unsigned TG_NULL = SLOTW + 3;
  localparam int unsigned TG_LAST = SLOTW + 4;
  localparam int unsigned TG_GEN  = SLOTW + 5;

  // ---- elaboration guards ---------------------------------------------------
  initial begin
    if (LIGHTS_MAX < 1 || LIGHTS_MAX > 8)
      $fatal(1, "zhao_light_stream: LIGHTS_MAX outside 1..8 (D-6 caps the descriptor at 8)");
    if (GAINW != 20)
      $fatal(1, "zhao_light_stream: GAINW != 20 needs a repriced two-half record, not a truncation");
    if (ACCW < GAINW + $clog2(2 * LIGHTS_MAX + 2) + 1)
      $fatal(1, "zhao_light_stream: ACCW too narrow for LIGHTS_MAX/GAINW; the fold would wrap");
    if (SQ_QUEUE_DEPTH < 4 || (SQ_QUEUE_DEPTH & (SQ_QUEUE_DEPTH - 1)) != 0)
      $fatal(1, "zhao_light_stream: SQ_QUEUE_DEPTH must be a power of two and at least 4");
    if (NSLOTS < 8 || (NSLOTS & (NSLOTS - 1)) != 0)
      $fatal(1, "zhao_light_stream: NSLOTS must be a power of two and at least 8");
    if (TG_GEN >= TAGW)
      $fatal(1, "zhao_light_stream: the identity does not fit TAGW; widen the tag, do not overlap fields");
    if (CNTW < 8) $fatal(1, "zhao_light_stream: CNTW below 8 makes the counters decorative");
  end

  // ==========================================================================
  // DESCRIPTOR BANK -- four 32-bit banks read together, depth 32
  // ==========================================================================
  logic [127:0]    desc_q [0:DDEPTH-1];
  logic [127:0]    desc_rd_q;
  logic [4:0]      desc_rd_addr_c;
  logic            active_gen_q;
  logic            shadow_gen_c;
  logic [CNTW-1:0] gen_inflight_q [0:1];

  assign shadow_gen_c = !active_gen_q;
  assign cfg_gen_o    = active_gen_q;

  logic [3:0] cfg_light_c;
  logic       cfg_half_c;
  logic [2:0] cfg_word_c;
  logic       cfg_is_env_c, cfg_map_ok_c, cfg_pin_ok_c, cfg_ok_c;
  logic [4:0] cfg_wa_c;

  always_comb begin
    cfg_light_c  = cfg_addr_i[7:4];
    cfg_half_c   = cfg_addr_i[3];
    cfg_word_c   = cfg_addr_i[2:0];
    cfg_is_env_c = (cfg_light_c == ENV_IDX);
    cfg_map_ok_c = cfg_is_env_c ? (!cfg_half_c && (cfg_word_c < 3'd6))
                                : ((cfg_light_c < 4'(LIGHTS_MAX)) && (cfg_word_c < 3'd4));
    cfg_pin_ok_c = (gen_inflight_q[shadow_gen_c] == '0);
    cfg_ok_c     = cfg_map_ok_c && (cfg_is_env_c || cfg_pin_ok_c);
    cfg_wa_c     = {shadow_gen_c, cfg_half_c, cfg_light_c[2:0]};
  end

  logic [GAINW-1:0] amb_q [0:2];
  logic [GAINW-1:0] spl_q [0:2];

  // ==========================================================================
  // THE PREPARED-NORMAL ARENA
  // ==========================================================================
  logic signed [31:0] a_nx_q [0:NSLOTS-1];
  logic signed [31:0] a_ny_q [0:NSLOTS-1];
  logic signed [31:0] a_nz_q [0:NSLOTS-1];
  logic        [31:0] a_mag_q [0:NSLOTS-1];
  logic               a_sealed_q [0:NSLOTS-1];
  logic               a_pdegen_q [0:NSLOTS-1];
  logic               a_profile_q [0:NSLOTS-1];
  logic               a_null_q [0:NSLOTS-1];
  logic [3:0]         a_nl_q [0:NSLOTS-1];
  logic [SRCW-1:0]    a_src_q [0:NSLOTS-1];

  logic [SLOTW:0]   alloc_q, prep_q, iss_q, ret_q, pf_slot_q;
  logic [SLOTW-1:0] alloc_i_c, prep_i_c, iss_i_c, pf_i_c;
  assign alloc_i_c = alloc_q[SLOTW-1:0];
  assign prep_i_c  = prep_q[SLOTW-1:0];
  assign iss_i_c   = iss_q[SLOTW-1:0];
  assign pf_i_c    = pf_slot_q[SLOTW-1:0];

  logic arena_full_c;
  assign arena_full_c = ((alloc_q - ret_q) >= (SLOTW+1)'(NSLOTS));
  assign v_ready_o = !arena_full_c;

  logic [3:0] nl_eff_c, nl_store_c;
  logic       nl_null_c;
  always_comb begin
    nl_eff_c   = (n_lights_i > 4'(LIGHTS_MAX)) ? 4'(LIGHTS_MAX) : n_lights_i;
    nl_null_c  = (nl_eff_c == 4'd0);
    // A vertex asking for no lights still owes the consumer an ambient+spill
    // packet IN ORDER. It gets ONE null term, whose response is forced to
    // zero, so ordering and slot lifetime need no second path. It costs two
    // clocks and is counted separately on `terms_null_o` so it can never be
    // mistaken for light work.
    nl_store_c = nl_null_c ? 4'd1 : nl_eff_c;
  end

  // ==========================================================================
  // TWO WIDE PRODUCT LANES -- multiplier operators 1 and 2
  // ==========================================================================
  logic signed [31:0] l0a_c, l0b_c, l1a_c, l1b_c;
  logic signed [63:0] l0p_q, l1p_q;

  // ==========================================================================
  // TERM ISSUE
  // ==========================================================================
  logic       sl_q;
  logic [2:0] iss_li_q;
  logic [2:0] pf_li_q;

  // half-A prefetch queue
  logic [127:0] pfa_rec_q [0:PFAD-1];
  logic [2:0]   pfa_li_q  [0:PFAD-1];
  logic [2:0]   pfa_wr_q, pfa_rd_q;
  logic         pfa_empty_c, pfa_room_c;
  logic         desc_a_pend_q;
  assign pfa_empty_c = (pfa_wr_q == pfa_rd_q);

  logic [127:0]       lA_head_c;
  logic signed [31:0] cur_lx_c, cur_ly_c, cur_lz_c, cur_det_c;
  assign lA_head_c = pfa_rec_q[pfa_rd_q[1:0]];
  always_comb begin
    cur_lx_c  = $signed(lA_head_c[31:0]);
    cur_ly_c  = $signed(lA_head_c[63:32]);
    cur_lz_c  = $signed(lA_head_c[95:64]);
    cur_det_c = $signed(lA_head_c[127:96]);
  end

  // ==========================================================================
  // SQUARE / ROOT PREPARATION
  // ==========================================================================
  logic           sq_active_q;
  logic [1:0]     sq_axis_q;
  logic [SLOTW:0] sq_slot_q;
  logic [63:0]    sq_acc_q;
  logic           sq_land_v_q;
  logic [1:0]     sq_land_axis_q;
  logic [SLOTW:0] sq_land_slot_q;

  logic [63:0]    rq_rad_q  [0:SQQD-1];
  logic [SLOTW:0] rq_slot_q [0:SQQD-1];
  logic [3:0]     rq_wr_q, rq_rd_q;
  logic           rq_room_c, rq_over_c, rq_empty_c;
  // The admission test is taken when a square GROUP STARTS, three or more
  // clocks before that group's radicand is pushed -- and the PREVIOUS group's
  // push may still be in flight at that moment. So the true occupancy at push
  // time can be two above the counted one, and a gate written as "not full
  // right now" lets the ring wrap and a request overwrite an unread entry.
  // That is exactly what happened: with a four-deep ring, entry N+4 replaced
  // entry N, four normals were sealed with ANOTHER normal's magnitude, their
  // own slots never sealed, and the machine wedged with a full arena and
  // nothing issuable. Nothing in the packet values could have shown it --
  // the magnitudes were all plausible.
  assign rq_room_c  = ((rq_wr_q - rq_rd_q) <= 4'(SQQD - RQ_ROOM_MARGIN));
  assign rq_over_c  = ((rq_wr_q - rq_rd_q) >= 4'(SQQD));
  assign rq_empty_c = (rq_wr_q == rq_rd_q);

  logic            rt_valid_c, rt_ready_w, rt_rvalid_w;
  logic [31:0]     rt_root_w;
  logic [SLOTW-1:0] rt_tago_w;
  assign rt_valid_c = !rq_empty_c;

  zhao_light_isqrt64_ii8 #(.TAGW(SLOTW)) u_root (
      .clk       (clk),
      .rst_n     (rst_n),
      .v_valid_i (rt_valid_c),
      .v_ready_o (rt_ready_w),
      .radicand_i(rq_rad_q[rq_rd_q[RQW-1:0]]),
      .tag_i     (rq_slot_q[rq_rd_q[RQW-1:0]][SLOTW-1:0]),
      .r_valid_o (rt_rvalid_w),
      .r_ready_i (1'b1),
      .root_o    (rt_root_w),
      .tag_o     (rt_tago_w)
  );

  // ==========================================================================
  // DIVIDER REQUEST QUEUE AND THE SHARED QUOTIENT
  // ==========================================================================
  logic [63:0]     dq_num_q [0:DIVQD-1];
  logic [31:0]     dq_den_q [0:DIVQD-1];
  logic            dq_neg_q [0:DIVQD-1];
  logic [TAGW-1:0] dq_tag_q [0:DIVQD-1];
  logic [3:0]      dq_wr_q, dq_rd_q;
  logic            dq_room_c, dq_empty_c;
  assign dq_room_c  = ((dq_wr_q - dq_rd_q) <= 4'(DIVQD - 3));
  assign dq_empty_c = (dq_wr_q == dq_rd_q);

  logic               dv_ready_w, dv_rvalid_w, dv_sat_w, dv_degen_w;
  logic signed [31:0] dv_res_w;
  logic [TAGW-1:0]    dv_tago_w;
  logic               dv_rready_c;

  zhao_light_div32_ii2 #(.TAGW(TAGW)) u_div (
      .clk         (clk),
      .rst_n       (rst_n),
      .v_valid_i   (!dq_empty_c),
      .v_ready_o   (dv_ready_w),
      .num_i       (dq_num_q[dq_rd_q[2:0]]),
      .den_i       (dq_den_q[dq_rd_q[2:0]]),
      .neg_i       (dq_neg_q[dq_rd_q[2:0]]),
      .tag_i       (dq_tag_q[dq_rd_q[2:0]]),
      .r_valid_o   (dv_rvalid_w),
      .r_ready_i   (dv_rready_c),
      .result_o    (dv_res_w),
      .saturated_o (dv_sat_w),
      .degenerate_o(dv_degen_w),
      .tag_o       (dv_tago_w)
  );

  // ---- the dot pipeline's own identity carrier ----------------------------
  logic            s0_q, s1_q;
  logic [TAGW-1:0] ta_tag_q, tb_tag_q;
  logic signed [31:0] ta_det_q, tb_det_q;
  logic [31:0]     ta_mag_q, tb_mag_q;
  logic signed [65:0] dot_part_q;

  logic signed [65:0] dot_full_c, h_c;
  logic [63:0]        habs_c;
  always_comb begin
    dot_full_c = dot_part_q + 66'($signed(l0p_q));
    h_c        = dot_full_c + 66'($signed({2'b0, tb_mag_q[31:1]}));
    habs_c     = h_c[65] ? (~h_c[63:0] + 64'd1) : h_c[63:0];
  end

  // ---- the side channel: detail plus an INDEPENDENT copy of the identity --
  logic signed [31:0] sd_det_q [0:SIDED-1];
  logic [TAGW-1:0]    sd_tag_q [0:SIDED-1];
  logic [6:0]         sd_wr_q, sd_rd_q;

  // ==========================================================================
  // COLOUR / RETIRE
  // ==========================================================================
  logic            cl_ph_q;
  logic            cap_v_q;
  logic [NDLW-1:0] cap_ndl_q;
  logic [TAGW-1:0] cap_tag_q;
  logic            cur_v_q;
  logic [NDLW-1:0] cur_ndl_q;
  logic [TAGW-1:0] cur_tag_q;
  logic [119:0]    lB_cur_q;

  // three narrow lanes -- multiplier operators 3, 4 and 5
  logic [GAINW-1:0]      c0a_c, c1a_c, c2a_c;
  logic [GAINW+NDLW-1:0] c0p_q, c1p_q, c2p_q;
  logic                  cp_v_q, cp_emit_q, cp_last_q, cp_gen_q;
  logic [SLOTW-1:0]      cp_slot_q;

  logic [ACCW-1:0] acc_q [0:2];

  logic [16:0]     oq_r_q [0:OUTQD-1];
  logic [16:0]     oq_g_q [0:OUTQD-1];
  logic [16:0]     oq_b_q [0:OUTQD-1];
  logic            oq_d_q [0:OUTQD-1];
  logic [SRCW-1:0] oq_s_q [0:OUTQD-1];
  logic [3:0]      oq_wr_q, oq_rd_q;
  logic            oq_full_c, oq_empty_c;
  assign oq_full_c  = ((oq_wr_q - oq_rd_q) >= 4'(OUTQD));
  assign oq_empty_c = (oq_wr_q == oq_rd_q);

  assign r_valid_o        = !oq_empty_c;
  assign rgb_r_o          = oq_r_q[oq_rd_q[2:0]];
  assign rgb_g_o          = oq_g_q[oq_rd_q[2:0]];
  assign rgb_b_o          = oq_b_q[oq_rd_q[2:0]];
  assign degenerate_vtx_o = oq_d_q[oq_rd_q[2:0]];
  assign src_id_o         = oq_s_q[oq_rd_q[2:0]];

  // The fold would drop a vertex if the output queue were full, so instead the
  // WHOLE colour engine freezes on that clock and retries. Backpressure then
  // propagates outward through the divider and stalls term issue, which is the
  // only arrangement in which a stalled consumer cannot change an answer.
  logic col_stall_c;
  assign col_stall_c = cp_v_q && cp_emit_q && cp_last_q && oq_full_c;

  // ==========================================================================
  // ISSUE SCHEDULING
  // ==========================================================================
  logic iss_slot_ok_c, iss_desc_ok_c, iss_div_ok_c, iss_go_c, iss_last_c;
  always_comb begin
    iss_slot_ok_c = (iss_q != alloc_q) && a_sealed_q[iss_i_c];
    iss_desc_ok_c = !pfa_empty_c && (pfa_li_q[pfa_rd_q[1:0]] == iss_li_q);
    iss_div_ok_c  = dq_room_c;
    iss_go_c      = (sl_q == 1'b0) && iss_slot_ok_c && iss_desc_ok_c && iss_div_ok_c;
    iss_last_c    = (({1'b0, iss_li_q} + 4'd1) >= a_nl_q[iss_i_c]);
  end

  // ---- descriptor port arbitration ----------------------------------------
  logic pfA_want_c, rdB_want_c, pf_has_c;
  always_comb begin
    pf_has_c   = (pf_slot_q != alloc_q) && a_sealed_q[pf_i_c];
    pfa_room_c = ((pfa_wr_q - pfa_rd_q) + {2'b0, desc_a_pend_q}) < 3'(PFAD);
    pfA_want_c = pf_has_c && pfa_room_c && !desc_a_pend_q;
    rdB_want_c = (cl_ph_q == 1'b0) && dv_rvalid_w && !cap_v_q && !col_stall_c;
  end

  logic desc_kind_b_c;
  always_comb begin
    desc_kind_b_c  = rdB_want_c;
    desc_rd_addr_c = rdB_want_c
        ? {dv_tago_w[TG_GEN], 1'b1, dv_tago_w[TG_LI +: 3]}
        : {active_gen_q, 1'b0, pf_li_q};
  end
  assign dv_rready_c = rdB_want_c;

  logic       desc_kind_b_q;
  logic [2:0] desc_a_li_q;

  // ---- ndl: the two profiles, one quotient --------------------------------
  logic signed [32:0] ndl_sum_c;
  logic [NDLW-1:0]    ndl_c;
  logic               ndl_lo_c, ndl_hi_c, ndl_degen_c, ndl_null_c;
  logic signed [31:0] side_det_c;
  logic               side_profile_c;
  logic [5:0] sd_rd_sk_c;
  assign sd_rd_sk_c     = 6'(sd_rd_q + 7'(SIDE_SKEW));
  assign side_det_c     = sd_det_q[sd_rd_sk_c];
  assign side_profile_c = a_profile_q[dv_tago_w[SLOTW-1:0]];

  always_comb begin
    ndl_degen_c = dv_degen_w;
    ndl_null_c  = dv_tago_w[TG_NULL];
    // The CREATURE profile admits no detail term. A creature normal-detail
    // extension needs its own explicit profile law and is not invented here.
    ndl_sum_c = side_profile_c ? 33'(dv_res_w) : (33'(dv_res_w) + 33'(side_det_c));
    ndl_lo_c  = !ndl_degen_c && !ndl_null_c && (ndl_sum_c < 33'sd0);
    ndl_hi_c  = !ndl_degen_c && !ndl_null_c && (ndl_sum_c > 33'sh1_0000);
    if (ndl_degen_c || ndl_null_c)     ndl_c = '0;
    else if (ndl_sum_c <= 33'sd0)      ndl_c = '0;
    else if (ndl_sum_c >= 33'sh1_0000) ndl_c = NDL_ONE;
    else                               ndl_c = ndl_sum_c[NDLW-1:0];
  end

  always_comb begin
    if (cl_ph_q == 1'b0) begin
      c0a_c = lB_cur_q[0   +: GAINW];
      c1a_c = lB_cur_q[20  +: GAINW];
      c2a_c = lB_cur_q[40  +: GAINW];
    end else begin
      c0a_c = lB_cur_q[60  +: GAINW];
      c1a_c = lB_cur_q[80  +: GAINW];
      c2a_c = lB_cur_q[100 +: GAINW];
    end
  end

  function automatic logic [GAINW:0] rhu16(input logic [GAINW+NDLW-1:0] p);
    rhu16 = (GAINW+1)'((p + (GAINW+NDLW)'(32768)) >> 16);
  endfunction

  logic [ACCW-1:0] fold_c [0:2];
  logic            fold_sat_c;
  always_comb begin
    fold_sat_c = 1'b0;
    fold_c[0]  = acc_q[0] + ACCW'(rhu16(c0p_q)) + ACCW'(amb_q[0]) + ACCW'(spl_q[0]);
    fold_c[1]  = acc_q[1] + ACCW'(rhu16(c1p_q)) + ACCW'(amb_q[1]) + ACCW'(spl_q[1]);
    fold_c[2]  = acc_q[2] + ACCW'(rhu16(c2p_q)) + ACCW'(amb_q[2]) + ACCW'(spl_q[2]);
    for (int unsigned c = 0; c < 3; c++)
      if (fold_c[c] > ACCW'(NDL_ONE)) fold_sat_c = 1'b1;
  end

  function automatic logic [NDLW-1:0] sat01(input logic [ACCW-1:0] v);
    sat01 = (v > ACCW'(NDL_ONE)) ? NDL_ONE : v[NDLW-1:0];
  endfunction

  assign idle_o = (alloc_q == ret_q) && dq_empty_c && rq_empty_c && !cur_v_q && !cap_v_q &&
                  oq_empty_c && !dv_rvalid_w && !cp_v_q && !s0_q && !s1_q;

  // ---- lane operand selection ---------------------------------------------
  // Lane 1 does a normal SQUARE on every clock it is not doing a dot product.
  //
  // That "every clock" is load-bearing and was not free. Restricting squares to
  // the second half of a term slot -- which is where the eight-clock calendar
  // puts them -- deadlocks the machine on the very first vertex: a term cannot
  // issue until its slot is SEALED, a slot is not sealed until its root
  // returns, the root needs the squares, and the squares were waiting for a
  // term issue that could never happen. The calendar describes STEADY STATE;
  // the arbitration has to also describe the cold start and every stall.
  logic l0en_c, l1dot_c, l1en_c, sq_go_c;
  logic [1:0] dot_used_c, sq_used_c;
  always_comb begin
    l0a_c = '0; l0b_c = '0; l1a_c = '0; l1b_c = '0;
    l0en_c = 1'b0; l1dot_c = 1'b0;
    if (sl_q == 1'b0) begin
      if (iss_go_c) begin
        l0a_c = a_nx_q[iss_i_c]; l0b_c = cur_lx_c; l0en_c = 1'b1;
        l1a_c = a_ny_q[iss_i_c]; l1b_c = cur_ly_c; l1dot_c = 1'b1;
      end
    end else begin
      l0a_c = a_nz_q[iss_i_c]; l0b_c = cur_lz_c; l0en_c = 1'b1;
    end
    sq_go_c = sq_active_q && !l1dot_c;
    if (sq_go_c) begin
      l1a_c = (sq_axis_q == 2'd0) ? a_nx_q[sq_slot_q[SLOTW-1:0]] :
              (sq_axis_q == 2'd1) ? a_ny_q[sq_slot_q[SLOTW-1:0]] :
                                    a_nz_q[sq_slot_q[SLOTW-1:0]];
      l1b_c = l1a_c;
    end
    l1en_c = l1dot_c || sq_go_c;
    dot_used_c = 2'({1'b0, l0en_c} + {1'b0, l1dot_c});
    sq_used_c  = 2'({1'b0, sq_go_c});
  end

  // ---- preparation pointer: one writer, three mutually exclusive causes ----
  logic prep_pending_c, prep_start_c, prep_skip_c, prep_done_c;
  always_comb begin
    prep_pending_c = !sq_active_q && (prep_q != alloc_q) && rq_room_c;
    // A supplied-magnitude normal is already sealed and owes no root, so the
    // pointer steps over it. Advancing the pointer in the ALLOCATOR instead --
    // which is what this did first -- is only correct while nothing is queued
    // ahead of it, and steps past render normals that still owe a root.
    prep_skip_c    = prep_pending_c && a_sealed_q[prep_i_c];
    prep_start_c   = prep_pending_c && !a_sealed_q[prep_i_c];
    prep_done_c    = sq_go_c && (sq_axis_q == 2'd2);
  end

  // ---- live references per generation: ONE writer per index ---------------
  // This is the epoch pin's whole basis, and it had the classic shape of a
  // counter that drifts: an increment and a decrement written as two separate
  // non-blocking assignments to the same array. When both fired on the same
  // clock for the same generation the increment was silently dropped, the
  // count walked negative, and every subsequent `cfg_commit_i` was refused --
  // so the descriptor set stopped being published and every light term after
  // that point used a STALE set while nothing reported an error.
  logic term_issue_c, term_retire_c, iss_gen_c;
  always_comb begin
    term_issue_c  = (sl_q == 1'b1);
    iss_gen_c     = ta_tag_q[TG_GEN];
    term_retire_c = !col_stall_c && cp_v_q && cp_emit_q;
  end

  logic [TAGW-1:0] iss_tag_c;
  assign iss_tag_c = TAGW'({active_gen_q, iss_last_c, a_null_q[iss_i_c], iss_li_q, iss_i_c});

  // ==========================================================================
  // THE DESCRIPTOR MEMORY: one write port, one registered read port
  // ==========================================================================
  always_ff @(posedge clk) begin
    if (cfg_we_i && cfg_ok_c && !cfg_is_env_c)
      desc_q[cfg_wa_c][7'({cfg_word_c, 5'd0}) +: 32] <= cfg_data_i;
    desc_rd_q <= desc_q[desc_rd_addr_c];
  end

  // ==========================================================================
  // THE SEQUENCER
  // ==========================================================================
  integer i;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      active_gen_q <= 1'b0;
      alloc_q <= '0; prep_q <= '0; iss_q <= '0; ret_q <= '0; pf_slot_q <= '0;
      sl_q <= 1'b0; iss_li_q <= '0; pf_li_q <= '0;
      pfa_wr_q <= '0; pfa_rd_q <= '0; desc_a_pend_q <= 1'b0;
      l0p_q <= '0; l1p_q <= '0;
      sq_active_q <= 1'b0; sq_axis_q <= '0; sq_slot_q <= '0; sq_acc_q <= '0;
      sq_land_v_q <= 1'b0; sq_land_axis_q <= '0; sq_land_slot_q <= '0;
      rq_wr_q <= '0; rq_rd_q <= '0;
      dq_wr_q <= '0; dq_rd_q <= '0;
      sd_wr_q <= '0; sd_rd_q <= '0;
      s0_q <= 1'b0; s1_q <= 1'b0;
      ta_tag_q <= '0; tb_tag_q <= '0; ta_det_q <= '0; tb_det_q <= '0;
      ta_mag_q <= '0; tb_mag_q <= '0; dot_part_q <= '0;
      cl_ph_q <= 1'b0; cap_v_q <= 1'b0; cur_v_q <= 1'b0;
      cap_ndl_q <= '0; cap_tag_q <= '0; cur_ndl_q <= '0; cur_tag_q <= '0;
      lB_cur_q <= '0;
      c0p_q <= '0; c1p_q <= '0; c2p_q <= '0;
      cp_v_q <= 1'b0; cp_emit_q <= 1'b0; cp_last_q <= 1'b0; cp_gen_q <= 1'b0; cp_slot_q <= '0;
      oq_wr_q <= '0; oq_rd_q <= '0;
      desc_kind_b_q <= 1'b0; desc_a_li_q <= '0;
      acc_q[0] <= '0; acc_q[1] <= '0; acc_q[2] <= '0;
      amb_q[0] <= '0; amb_q[1] <= '0; amb_q[2] <= '0;
      spl_q[0] <= '0; spl_q[1] <= '0; spl_q[2] <= '0;
      gen_inflight_q[0] <= '0; gen_inflight_q[1] <= '0;
      normal_inputs_o <= '0; normal_prepared_o <= '0; roots_issued_o <= '0;
      roots_retired_o <= '0; supplied_mags_o <= '0; terms_accepted_o <= '0;
      terms_retired_o <= '0; terms_null_o <= '0; normal_queue_wait_o <= '0;
      descriptor_wait_o <= '0; dot_product_slots_o <= '0; square_product_slots_o <= '0;
      unused_product_slots_o <= '0; divider_backpressure_o <= '0;
      colour_backpressure_o <= '0; output_backpressure_o <= '0;
      epoch_refusals_o <= '0; logical_raw_saturations_o <= '0; degenerate_terms_o <= '0;
      vertices_lit_o <= '0; degenerate_o <= '0; ndl_clamp_lo_o <= '0;
      ndl_clamp_hi_o <= '0; rgb_sat_o <= '0; cfg_refused_o <= '0;
      nlights_clamped_o <= '0; seam_mismatch_o <= '0; tag_mismatch_o <= '0;
      root_queue_overflow_o <= '0;
      for (i = 0; i < NSLOTS; i = i + 1) a_sealed_q[i] <= 1'b0;
    end else begin
      // ---------------------------------------------------------------- cfg --
      if (cfg_we_i) begin
        if (cfg_ok_c && cfg_is_env_c) begin
          if (cfg_word_c < 3'd3) amb_q[cfg_word_c[1:0]]       <= cfg_data_i[GAINW-1:0];
          else                   spl_q[2'(cfg_word_c - 3'd3)] <= cfg_data_i[GAINW-1:0];
        end
        if (!cfg_map_ok_c)                       cfg_refused_o    <= cfg_refused_o + CNTW'(1);
        else if (!cfg_is_env_c && !cfg_pin_ok_c) epoch_refusals_o <= epoch_refusals_o + CNTW'(1);
      end
      if (cfg_commit_i) begin
        if (gen_inflight_q[shadow_gen_c] == '0) active_gen_q <= shadow_gen_c;
        else epoch_refusals_o <= epoch_refusals_o + CNTW'(1);
      end

      // ------------------------------------------------------- normal input --
      if (v_valid_i && v_ready_o) begin
        a_nx_q[alloc_i_c]      <= n_x_i;
        a_ny_q[alloc_i_c]      <= n_y_i;
        a_nz_q[alloc_i_c]      <= n_z_i;
        a_pdegen_q[alloc_i_c]  <= n_degenerate_i;
        a_profile_q[alloc_i_c] <= n_profile_i;
        a_nl_q[alloc_i_c]      <= nl_store_c;
        a_null_q[alloc_i_c]    <= nl_null_c;
        a_src_q[alloc_i_c]     <= n_src_id_i;
        a_mag_q[alloc_i_c]     <= n_mag_valid_i ? n_mag_i : 32'd0;
        a_sealed_q[alloc_i_c]  <= n_mag_valid_i;
        alloc_q                <= alloc_q + (SLOTW+1)'(1);
        normal_inputs_o        <= normal_inputs_o + CNTW'(1);
        if (n_mag_valid_i) supplied_mags_o <= supplied_mags_o + CNTW'(1);
        if (n_lights_i > 4'(LIGHTS_MAX)) nlights_clamped_o <= nlights_clamped_o + CNTW'(1);
      end

      // ------------------------------------------------ square preparation ---
      if (prep_start_c) begin
        sq_active_q <= 1'b1;
        sq_slot_q   <= prep_q;
        sq_axis_q   <= 2'd0;
        sq_acc_q    <= 64'd0;
      end
      // prep_q has EXACTLY ONE writer. It used to have two -- one here and one
      // in the allocator for supplied-magnitude normals -- and two
      // non-blocking writes to one register in the same clock silently drop
      // one of them, which walks the pointer off the vertices that still owe
      // a root. The skip and the completion are mutually exclusive by
      // construction (sq_active_q is false in one and true in the other).
      if (prep_skip_c || prep_done_c) prep_q <= prep_q + (SLOTW+1)'(1);

      // ---- live references per generation, one assignment per index -------
      gen_inflight_q[0] <= gen_inflight_q[0]
                           + ((term_issue_c  && (iss_gen_c == 1'b0)) ? CNTW'(1) : CNTW'(0))
                           - ((term_retire_c && (cp_gen_q  == 1'b0)) ? CNTW'(1) : CNTW'(0));
      gen_inflight_q[1] <= gen_inflight_q[1]
                           + ((term_issue_c  && (iss_gen_c == 1'b1)) ? CNTW'(1) : CNTW'(0))
                           - ((term_retire_c && (cp_gen_q  == 1'b1)) ? CNTW'(1) : CNTW'(0));

      // ------------------------------------------------------- term issue ----
      s0_q        <= 1'b0;
      s1_q        <= 1'b0;
      sq_land_v_q <= 1'b0;
      // Product-slot accounting is done ONCE, from the lane enables, so the
      // conservation law (dot + square + unused == 2 * clocks) is a property
      // of the same signals that drive the multipliers rather than of a
      // parallel tally that could drift away from them.
      dot_product_slots_o <= dot_product_slots_o + CNTW'(dot_used_c);
      square_product_slots_o <= square_product_slots_o + CNTW'(sq_used_c);
      unused_product_slots_o <= unused_product_slots_o + CNTW'(2) -
                                CNTW'(dot_used_c) - CNTW'(sq_used_c);

      if (sq_go_c) begin
        sq_land_v_q    <= 1'b1;
        sq_land_axis_q <= sq_axis_q;
        sq_land_slot_q <= sq_slot_q;
        if (sq_axis_q == 2'd2) sq_active_q <= 1'b0;
        else                   sq_axis_q   <= sq_axis_q + 2'd1;
      end

      if (sl_q == 1'b0) begin
        if (iss_go_c) begin
          s0_q     <= 1'b1;
          ta_tag_q <= iss_tag_c;
          ta_det_q <= cur_det_c;
          ta_mag_q <= a_mag_q[iss_i_c];
          sl_q     <= 1'b1;
        end else begin
          if (!iss_slot_ok_c)      normal_queue_wait_o    <= normal_queue_wait_o + CNTW'(1);
          else if (!iss_desc_ok_c) descriptor_wait_o      <= descriptor_wait_o + CNTW'(1);
          else                     divider_backpressure_o <= divider_backpressure_o + CNTW'(1);
        end
      end else begin
        s1_q <= 1'b1;
        terms_accepted_o <= terms_accepted_o + CNTW'(1);
        if (a_null_q[iss_i_c]) terms_null_o <= terms_null_o + CNTW'(1);
        pfa_rd_q <= pfa_rd_q + 3'd1;
        if (iss_last_c) begin
          iss_li_q <= '0;
          iss_q    <= iss_q + (SLOTW+1)'(1);
        end else begin
          iss_li_q <= iss_li_q + 3'd1;
        end
        sl_q <= 1'b0;
      end

      // -------------------------------------------- descriptor read landing --
      desc_kind_b_q <= desc_kind_b_c;
      desc_a_pend_q <= !desc_kind_b_c && pfA_want_c;
      desc_a_li_q   <= pf_li_q;
      if (!desc_kind_b_c && pfA_want_c) begin
        if (({1'b0, pf_li_q} + 4'd1) >= a_nl_q[pf_i_c]) begin
          pf_li_q   <= '0;
          pf_slot_q <= pf_slot_q + (SLOTW+1)'(1);
        end else begin
          pf_li_q <= pf_li_q + 3'd1;
        end
      end
      if (desc_a_pend_q) begin
        pfa_rec_q[pfa_wr_q[1:0]] <= desc_rd_q;
        pfa_li_q[pfa_wr_q[1:0]]  <= desc_a_li_q;
        pfa_wr_q                 <= pfa_wr_q + 3'd1;
      end

      // ----------------------------------------------------- lane products ---
      if (l0en_c) l0p_q <= l0a_c * l0b_c;
      if (l1en_c) l1p_q <= l1a_c * l1b_c;

      // ------------------------------------------------ dot / square landing -
      if (s0_q) begin
        dot_part_q <= 66'($signed(l0p_q)) + 66'($signed(l1p_q));
        tb_tag_q   <= ta_tag_q;
        tb_det_q   <= ta_det_q;
        tb_mag_q   <= ta_mag_q;
      end
      if (s1_q) begin
        dq_num_q[dq_wr_q[2:0]] <= habs_c;
        dq_den_q[dq_wr_q[2:0]] <= tb_mag_q;
        dq_neg_q[dq_wr_q[2:0]] <= h_c[65];
        dq_tag_q[dq_wr_q[2:0]] <= tb_tag_q;
        dq_wr_q                <= dq_wr_q + 4'd1;
        sd_det_q[sd_wr_q[5:0]] <= tb_det_q;
        sd_tag_q[sd_wr_q[5:0]] <= tb_tag_q;
        sd_wr_q                <= sd_wr_q + 7'd1;
      end
      if (sq_land_v_q) begin
        if (sq_land_axis_q == 2'd2) begin
          rq_rad_q[rq_wr_q[RQW-1:0]]  <= sq_acc_q + l1p_q;
          rq_slot_q[rq_wr_q[RQW-1:0]] <= sq_land_slot_q;
          rq_wr_q                 <= rq_wr_q + 4'd1;
          // UNREACHABLE while the admission test above is correct, which is
          // precisely why it owes a committed mutant rather than a comment:
          // tests/mutants/zhao_light_stream_rqfull_mutant.sv restores the
          // "not full right now" gate and this counter FIRES.
          if (rq_over_c) root_queue_overflow_o <= root_queue_overflow_o + CNTW'(1);
          sq_acc_q                <= 64'd0;
        end else begin
          sq_acc_q <= sq_acc_q + l1p_q;
        end
      end

      // ------------------------------------------------------- root traffic --
      if (rt_valid_c && rt_ready_w) begin
        rq_rd_q        <= rq_rd_q + 4'd1;
        roots_issued_o <= roots_issued_o + CNTW'(1);
      end
      if (rt_rvalid_w) begin
        a_mag_q[rt_tago_w]    <= rt_root_w;
        a_sealed_q[rt_tago_w] <= 1'b1;
        roots_retired_o   <= roots_retired_o + CNTW'(1);
        normal_prepared_o <= normal_prepared_o + CNTW'(1);
      end

      // ---------------------------------------------------- divider accept ---
      if (!dq_empty_c && dv_ready_w) dq_rd_q <= dq_rd_q + 4'd1;

      // A finished quotient offered on a GAIN clock that the colour engine
      // does not take. It lives OUTSIDE the stall guard on purpose: written
      // inside it, the only way to reach it was `cap_v_q` still set at a gain
      // clock, and that state cannot occur -- the promote at the intervening
      // emission clock always clears it. It was a counter that could never
      // fire, asserted zero, reading like evidence. The sweep in
      // `light_stream_directed` is what found it, by naming the counters that
      // never moved instead of only checking the ones that did.
      if (dv_rvalid_w && !dv_rready_c && (cl_ph_q == 1'b0))
        colour_backpressure_o <= colour_backpressure_o + CNTW'(1);

      // ------------------------------------------------- colour / retire -----
      if (!col_stall_c) begin
        cl_ph_q <= !cl_ph_q;

        if (rdB_want_c) begin
          cap_v_q   <= 1'b1;
          cap_ndl_q <= ndl_c;
          cap_tag_q <= dv_tago_w;
          sd_rd_q   <= sd_rd_q + 7'd1;
          if (sd_tag_q[sd_rd_sk_c] != dv_tago_w)
            tag_mismatch_o <= tag_mismatch_o + CNTW'(1);
          if (ndl_lo_c) ndl_clamp_lo_o <= ndl_clamp_lo_o + CNTW'(1);
          if (ndl_hi_c) ndl_clamp_hi_o <= ndl_clamp_hi_o + CNTW'(1);
          // Per LIGHT TERM, not per prepared normal: magnitude reuse divides
          // the work by K and must not divide the evidence by K.
          if (dv_sat_w && !dv_tago_w[TG_NULL])
            logical_raw_saturations_o <= logical_raw_saturations_o + CNTW'(1);
          if (dv_degen_w && !dv_tago_w[TG_NULL])
            degenerate_terms_o <= degenerate_terms_o + CNTW'(1);
        end

        cp_v_q <= 1'b0;
        if (cur_v_q) begin
          c0p_q     <= c0a_c * {{(GAINW-1){1'b0}}, cur_ndl_q};
          c1p_q     <= c1a_c * {{(GAINW-1){1'b0}}, cur_ndl_q};
          c2p_q     <= c2a_c * {{(GAINW-1){1'b0}}, cur_ndl_q};
          cp_v_q    <= 1'b1;
          cp_emit_q <= cl_ph_q;
          cp_last_q <= cur_tag_q[TG_LAST];
          cp_gen_q  <= cur_tag_q[TG_GEN];
          cp_slot_q <= cur_tag_q[SLOTW-1:0];
          if (cl_ph_q == 1'b1) cur_v_q <= 1'b0;
        end

        // The captured term is promoted on the EMISSION clock so that the very
        // next clock is a gain clock with `cur` already loaded. Half B landed
        // one clock after its read was issued, which is this clock.
        if (cl_ph_q == 1'b1 && desc_kind_b_q && cap_v_q) begin
          cur_v_q   <= 1'b1;
          cur_ndl_q <= cap_ndl_q;
          cur_tag_q <= cap_tag_q;
          lB_cur_q  <= desc_rd_q[119:0];
          cap_v_q   <= 1'b0;
        end

        // ---------------------------------------- colour product landing ----
        if (cp_v_q) begin
          if (!cp_emit_q) begin
            acc_q[0] <= acc_q[0] + ACCW'(rhu16(c0p_q));
            acc_q[1] <= acc_q[1] + ACCW'(rhu16(c1p_q));
            acc_q[2] <= acc_q[2] + ACCW'(rhu16(c2p_q));
          end else begin
            terms_retired_o <= terms_retired_o + CNTW'(1);
            if (!cp_last_q) begin
              acc_q[0] <= acc_q[0] + ACCW'(rhu16(c0p_q));
              acc_q[1] <= acc_q[1] + ACCW'(rhu16(c1p_q));
              acc_q[2] <= acc_q[2] + ACCW'(rhu16(c2p_q));
            end else begin
              // ONE saturate, at the end, after ambient and spill.
              if (DEGEN_BLACK && (a_mag_q[cp_slot_q] == 32'd0)) begin
                oq_r_q[oq_wr_q[2:0]] <= '0;
                oq_g_q[oq_wr_q[2:0]] <= '0;
                oq_b_q[oq_wr_q[2:0]] <= '0;
              end else begin
                oq_r_q[oq_wr_q[2:0]] <= sat01(fold_c[0]);
                oq_g_q[oq_wr_q[2:0]] <= sat01(fold_c[1]);
                oq_b_q[oq_wr_q[2:0]] <= sat01(fold_c[2]);
                if (fold_sat_c) rgb_sat_o <= rgb_sat_o + CNTW'(1);
              end
              oq_d_q[oq_wr_q[2:0]] <= (a_mag_q[cp_slot_q] == 32'd0);
              oq_s_q[oq_wr_q[2:0]] <= a_src_q[cp_slot_q];
              oq_wr_q              <= oq_wr_q + 4'd1;
              vertices_lit_o       <= vertices_lit_o + CNTW'(1);
              if (a_mag_q[cp_slot_q] == 32'd0) degenerate_o <= degenerate_o + CNTW'(1);
              if (a_pdegen_q[cp_slot_q] != (a_mag_q[cp_slot_q] == 32'd0))
                seam_mismatch_o <= seam_mismatch_o + CNTW'(1);
              acc_q[0] <= '0;
              acc_q[1] <= '0;
              acc_q[2] <= '0;
              ret_q    <= ret_q + (SLOTW+1)'(1);
            end
          end
        end
      end else begin
        output_backpressure_o <= output_backpressure_o + CNTW'(1);
      end

      // ------------------------------------------------------ output pop -----
      if (r_valid_o && r_ready_i) oq_rd_q <= oq_rd_q + 4'd1;
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
      if (r_valid_o) begin
        a_rgb_r_in_range : assert (rgb_r_o <= NDL_ONE);
        a_rgb_g_in_range : assert (rgb_g_o <= NDL_ONE);
        a_rgb_b_in_range : assert (rgb_b_o <= NDL_ONE);
      end
      // The biased numerator fits 64 bits: |dot| <= 3 * 2^62 for s32 lanes and
      // floor(mag/2) < 2^31, so |h| < 2^64. If that ever stops holding, the
      // divider is being handed a number it cannot represent and the answer
      // would be quietly wrong rather than loudly refused.
      if (s1_q) a_numerator_fits_64 : assert (h_c[65] == h_c[64]);
      // A term may never be issued against an unsealed arena slot.
      if (s0_q) a_issued_slot_sealed : assert (a_sealed_q[ta_tag_q[SLOTW-1:0]]);
    end
  end
`endif

endmodule : zhao_light_stream

`default_nettype wire
