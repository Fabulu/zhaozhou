// zhao_geom_replay.sv -- GEOM.REPLAY: the geometry replay customer. One meshlet's
// sealed arena groups and its TriangleDescriptors in; screen triangles with their
// corner depth and vertex attributes out, one per triangle per visible view.
//
//     GEOM.GROUP_SEQ --- grp_* (1 or 2 sealed handles) ---\
//     GEOM.ASSEMBLE  --- t_* (v0,v1,v2) + m_done -----------> GEOM.REPLAY --> GEOM.CLIP
//                                                         |      |   ^
//     GEOM.PROJ_LANE <-- look_* (3 per triangle per view) -'      |   |
//     vertex-attribute store <-- the SAME look_* nets ------------'   |
//     GEOM.DEPTHQUANT x3 + one rcp24 (inside) --------------------------'
//
// ---------------------------------------------------------------------------
// WHY THIS FILE EXISTS
// ---------------------------------------------------------------------------
// zhao_console_core entry I11 wrote this block's specification down before it
// existed: "take a sealed handle here and a TriangleDescriptor {v0,v1,v2} from
// GEOM.ASSEMBLE; issue THREE lookups on GEOM.PROJ_LANE; slice each 106-bit
// reply -- {x[20:0], y[20:0], d[31:0], w[30:0], behind} -- into the corner and
// its behind bit; present {ax,ay,bx,by,cx,cy,behind[2:0]} to GEOM.CLIP; pulse
// `rel_valid_o` back when the group is done with." And it said why the composer
// could not do it: "THE THREE-LOOKUP SEQUENCING IS NOT field routing: it is a
// state machine with a reply join." This is that state machine.
//
// It is `design/blocks.yml` GEOM.WCACHE's "project once, replay twice" made
// real on the geometry side: the terrain half of the same idea lives inside
// `zhao_terrain_wcache` (its ref_* -> out_* port), and this is its geometry
// twin, built beside the arena instead of inside it because the geometry arena
// is a sibling of the shared projector (`zhao_geom_proj_lane`'s header).
//
// ---------------------------------------------------------------------------
// ONE WALK, BOTH VIEWS -- AND WHY ASSEMBLE'S VERTEX OFFSET IS ZERO
// ---------------------------------------------------------------------------
// GEOM.GROUP_SEQ opens ONE arena per visible view per meshlet and fills vertex
// i of the meshlet at arena index i (its `vi_q`, from 0). So a meshlet-local
// index IS the arena index in every view, and what distinguishes the views is
// the ARENA HANDLE, not an offset. GEOM.ASSEMBLE's per-view `vertex_offset` is
// therefore zero by construction in this architecture -- its "one walk per
// visible view" guard is satisfied by replaying ONE walk into each view's own
// arena here, which is the property its header asks for (view 1 never reads
// view 0's vertices, because it never names view 0's arena).
//
// ---------------------------------------------------------------------------
// THE RELEASE IS PROVEN, NOT GUESSED (zhao_console_core entry I38)
// ---------------------------------------------------------------------------
// GEOM.ASSETFETCH holds one meshlet until "whoever knows that BOTH readers have
// finished" says so. This block knows, structurally:
//   * the VERTEX reader is done: every handle arrives only after GEOM.GROUP_SEQ
//     sealed, which it does only after `count` vertices LANDED, which needs all
//     `count` records to have left GEOM.ASSETFETCH's vertex stream;
//   * the INDEX reader is done: GEOM.ASSEMBLE's `m_done_i` pulses on every way
//     its walk ends, after its last triangle was accepted HERE.
// `af_release_o` fires only when both hold and every triangle has been emitted
// and every arena released. A meshlet GEOM.GROUP_SEQ would refuse (no vertices,
// or no visible view) expects NO handles, so it cannot wedge here.
//
// ---------------------------------------------------------------------------
// DEPTH: GEOM.DEPTHQUANT, THREE LANES, ONE RECIPROCAL
// ---------------------------------------------------------------------------
// Slot 0 of the ruling-5 packet is invw24, which only GEOM.DEPTHQUANT may make
// (owner ruling D-4: "no consumer performs its own profile conversion"). It is
// strictly one vertex at a time, so the three corners get three lanes; the
// lanes share ONE `zhao_raster_rcp24_v4` (the latest reciprocal, a second
// INSTANCE of the one law -- the `zhao_field_isqrt` precedent), steered by the
// token that service already carries. The profile is the one each vertex was
// PROJECTED under: a per-arena table written from the projector's own landings
// (the arena, not the view, because a view's profile is SetView state and may
// change between frames while a group from the previous frame is still here).
// Two landings in one arena with different profiles are counted on
// `profile_mixed_o` -- independent operands, two different landings.
//
// ---------------------------------------------------------------------------
// THE VERTEX-ATTRIBUTE STORE (owner ruling R11, provisional)
// ---------------------------------------------------------------------------
// Slots 1..6 (u_over_w, v_over_w, r, g, b, alpha) are per-vertex, per-view, and
// live in a store keyed EXACTLY like the arena. It listens to this block's
// `look_*` nets -- the same three lookups -- and must answer with the arena's
// timing: `att_rep_valid_i` in the same cycle as `rep_valid_i`. A disagreement
// is counted on `att_skew_o`, whose operands come from two different memories.
// The store's WRITER is not built (R11 names it); the store is this block's
// peer, not its part.
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK REFUSES
// ---------------------------------------------------------------------------
// A triangle with a REFUSED corner (bad arena, stale generation, unsealed, out
// of range) or a MISSED one (slot never written) is DROPPED and counted: it
// cannot be drawn, and drawing it from a stale or unwritten vertex would put
// somebody else's geometry on screen. A vertex id wider than the arena index is
// forced to an index the arena REFUSES rather than truncated onto a real slot.
//
// Throughput is stated, not claimed: see the directed test's measured clocks
// per triangle. Conservative SystemVerilog subset only (charter section 2).
`default_nettype none

module zhao_geom_replay #(
    parameter int unsigned ARENA_W   = 3,
    parameter int unsigned GEN_W     = 8,
    parameter int unsigned INDEX_W   = 12,
    parameter int unsigned VIDW      = 16,
    parameter int unsigned SRCW      = 16,
    parameter int unsigned PAYLOAD_W = 106,
    // Attribute store words per vertex (u_over_w, v_over_w, r, g, b, alpha).
    parameter int unsigned ATTRW     = 6 * 32,
    // Contexts in the private reciprocal. Three lanes, one request each.
    parameter int unsigned RCP_NCTX  = 4
) (
    input  wire clk,
    input  wire rst_n,

    // ---- the meshlet token, from the dispatcher fork -------------------------
    input  wire                    mt_valid_i,
    output wire                    mt_ready_o,
    input  wire [1:0]              mt_view_mask_i,
    input  wire [7:0]              mt_vertex_count_i,

    // ---- sealed group handles, from GEOM.GROUP_SEQ ----------------------------
    input  wire                    grp_valid_i,
    output wire                    grp_ready_o,
    input  wire [ARENA_W-1:0]      grp_arena_i,
    input  wire [GEN_W-1:0]        grp_gen_i,
    input  wire                    grp_view_i,
    output wire                    rel_valid_o,
    output wire [ARENA_W-1:0]      rel_arena_o,

    // ---- arena lifetime and landings, for the per-arena depth profile -------
    input  wire                    op_valid_i,     // GEOM.GROUP_SEQ open_o
    input  wire [ARENA_W-1:0]      op_arena_i,
    input  wire                    fl_valid_i,     // a geometry landing
    input  wire [ARENA_W-1:0]      fl_arena_i,
    input  wire [1:0]              fl_profile_i,

    // ---- TriangleDescriptors, from GEOM.ASSEMBLE -----------------------------
    input  wire                    t_valid_i,
    output wire                    t_ready_o,
    input  wire [VIDW-1:0]         t_v0_i,
    input  wire [VIDW-1:0]         t_v1_i,
    input  wire [VIDW-1:0]         t_v2_i,
    input  wire [15:0]             t_material_i,
    input  wire [31:0]             t_raster_i,
    input  wire [SRCW-1:0]         t_src_id_i,
    input  wire                    m_done_i,       // the walk ended

    // ---- lookups: the arena, and the attribute store on the SAME nets -------
    output wire                    look_valid_o,
    input  wire                    look_ready_i,
    output wire [ARENA_W-1:0]      look_arena_o,
    output wire [GEN_W-1:0]        look_gen_o,
    output wire [INDEX_W-1:0]      look_index_o,
    input  wire                    rep_valid_i,
    input  wire                    rep_hit_i,
    input  wire                    rep_refuse_i,
    input  wire [PAYLOAD_W-1:0]    rep_payload_i,
    input  wire                    att_rep_valid_i,
    input  wire [ATTRW-1:0]        att_rep_data_i,

    // ---- GEOM.ASSETFETCH's buffer release (entry I38) ------------------------
    output wire                    af_release_o,

    // ---- the replayed triangle -----------------------------------------------
    output wire                    o_valid_o,
    input  wire                    o_ready_i,
    output wire signed [20:0]      o_ax_o,
    output wire signed [20:0]      o_ay_o,
    output wire signed [20:0]      o_bx_o,
    output wire signed [20:0]      o_by_o,
    output wire signed [20:0]      o_cx_o,
    output wire signed [20:0]      o_cy_o,
    output wire [2:0]              o_behind_o,     // bit 0 = A, 1 = B, 2 = C
    output wire [23:0]             o_invw_a_o,
    output wire [23:0]             o_invw_b_o,
    output wire [23:0]             o_invw_c_o,
    output wire [ATTRW-1:0]        o_attr_a_o,
    output wire [ATTRW-1:0]        o_attr_b_o,
    output wire [ATTRW-1:0]        o_attr_c_o,
    output wire                    o_view_o,
    output wire [SRCW-1:0]         o_src_id_o,
    output wire [15:0]             o_material_o,
    output wire [31:0]             o_raster_o,

    // ---- evidence -------------------------------------------------------------
    output logic [31:0]            meshlets_o,       // meshlets released
    output logic [31:0]            groups_o,         // handles taken
    output logic [31:0]            triangles_in_o,   // descriptors taken
    output logic [31:0]            triangles_out_o,  // triangles emitted (all views)
    output logic [31:0]            refused_o,        // dropped: a corner refused
    output logic [31:0]            missed_o,         // dropped: a corner missed
    output logic [31:0]            att_skew_o,       // store and arena disagreed on timing
    output logic [31:0]            profile_mixed_o,  // one arena, two profiles
    output logic [31:0]            view_bad_o,       // a handle for a view not in the mask
    output logic [31:0]            dq_refused_o      // DEPTHQUANT refusals, all lanes
);

  localparam int unsigned NA = 1 << ARENA_W;

  initial begin
    if (PAYLOAD_W != 106)
      $fatal(1, "zhao_geom_replay: PAYLOAD_W is %0d; the arena packs {behind,w,d,y,x} = 106", PAYLOAD_W);
    if (VIDW < INDEX_W)
      $fatal(1, "zhao_geom_replay: VIDW (%0d) narrower than INDEX_W (%0d)", VIDW, INDEX_W);
  end

  // ---- state ---------------------------------------------------------------
  localparam logic [2:0] S_IDLE  = 3'd0;
  localparam logic [2:0] S_HAND  = 3'd1;
  localparam logic [2:0] S_TRI   = 3'd2;
  localparam logic [2:0] S_LOOK  = 3'd3;
  localparam logic [2:0] S_DEPTH = 3'd4;
  localparam logic [2:0] S_EMIT  = 3'd5;
  localparam logic [2:0] S_REL   = 3'd6;

  logic [2:0] st_q;

  logic [1:0]         need_q;          // handles this meshlet expects, 0..2
  logic [1:0]         nh_q;            // handles taken
  logic [1:0]         mask_q;
  logic               done_seen_q;     // GEOM.ASSEMBLE's walk has ended
  logic [ARENA_W-1:0] sl_arena_q [2];
  logic [GEN_W-1:0]   sl_gen_q   [2];
  logic               sl_view_q  [2];
  logic               vs_q;            // the slot being replayed
  logic               rk_rel_q;        // the slot being released

  // the triangle in hand
  logic [VIDW-1:0]    tv_q [3];
  logic [15:0]        tmat_q;
  logic [31:0]        trast_q;
  logic [SRCW-1:0]    tsrc_q;

  // the lookups
  logic [1:0]         ik_q;            // lookups issued, 0..3
  logic [1:0]         rk_q;            // replies taken, 0..3
  logic               any_ref_q, any_miss_q;
  logic signed [20:0] cx_q [3];
  logic signed [20:0] cy_q [3];
  logic [30:0]        cw_q [3];
  logic [2:0]         cb_q;
  logic [ATTRW-1:0]   ca_q [3];

  // depth
  logic [2:0]         dq_started_q, dq_got_q;
  logic [23:0]        invw_q [3];

  // per-arena projection profile
  logic [1:0]         prof_q [NA];
  logic [NA-1:0]      prof_seen_q;

  // ---- handshakes ----------------------------------------------------------
  assign mt_ready_o  = (st_q == S_IDLE);
  assign grp_ready_o = (st_q == S_HAND);
  assign t_ready_o   = (st_q == S_TRI);
  assign o_valid_o   = (st_q == S_EMIT) && !any_ref_q && !any_miss_q;

  wire [1:0] mt_need_c = ((mt_vertex_count_i == 8'd0) || (mt_view_mask_i == 2'b00))
                         ? 2'd0
                         : (2'(mt_view_mask_i[0]) + 2'(mt_view_mask_i[1]));

  // ---- lookups -------------------------------------------------------------
  // A vertex id wider than the arena index is FORCED to the all-ones index,
  // which is >= DEPTH for every geometry arena (DEPTH 1089 < 2^12), so the arena
  // REFUSES it. Truncating would alias it onto a real slot.
  logic [INDEX_W-1:0] look_ix_c;
  logic [VIDW-1:0]    look_v_c;
  always_comb begin
    look_v_c  = tv_q[ik_q];
    look_ix_c = look_v_c[INDEX_W-1:0];
    if (VIDW > INDEX_W) begin
      if ((look_v_c >> INDEX_W) != '0) look_ix_c = '1;
    end
  end

  assign look_valid_o = (st_q == S_LOOK) && (ik_q != 2'd3);
  assign look_arena_o = sl_arena_q[vs_q];
  assign look_gen_o   = sl_gen_q[vs_q];
  assign look_index_o = look_ix_c;

  // ---- release -------------------------------------------------------------
  assign rel_valid_o  = (st_q == S_REL) && (need_q != 2'd0);
  assign rel_arena_o  = sl_arena_q[rk_rel_q];
  // The buffer goes in the SAME cycle as the last arena (or at once for a
  // meshlet that held none).
  assign af_release_o = (st_q == S_REL) &&
                        ((need_q == 2'd0) || (rk_rel_q == (need_q == 2'd2)));

  // ---- the replayed triangle -----------------------------------------------
  assign o_ax_o       = cx_q[0];
  assign o_ay_o       = cy_q[0];
  assign o_bx_o       = cx_q[1];
  assign o_by_o       = cy_q[1];
  assign o_cx_o       = cx_q[2];
  assign o_cy_o       = cy_q[2];
  assign o_behind_o   = cb_q;
  assign o_invw_a_o   = invw_q[0];
  assign o_invw_b_o   = invw_q[1];
  assign o_invw_c_o   = invw_q[2];
  assign o_attr_a_o   = ca_q[0];
  assign o_attr_b_o   = ca_q[1];
  assign o_attr_c_o   = ca_q[2];
  assign o_view_o     = sl_view_q[vs_q];
  assign o_src_id_o   = tsrc_q;
  assign o_material_o = tmat_q;
  assign o_raster_o   = trast_q;

  // ---- DEPTHQUANT x3 on one reciprocal --------------------------------------
  wire [1:0] cur_prof_c = prof_q[sl_arena_q[vs_q]];

  logic        dq_v_valid [3];
  logic        dq_v_ready [3];
  logic        dq_d_valid [3];
  logic        dq_d_ready [3];
  logic [23:0] dq_invw    [3];
  logic        dq_rcp_valid [3];
  logic        dq_rcp_ready [3];
  logic [23:0] dq_rcp_d     [3];
  logic        dq_rcp_rvalid [3];
  logic        dq_rcp_rready [3];
  logic [31:0] dq_refused_c  [3];
  // SATURATION IS NOT SUMMED OUT, deliberately: for the three shipped profiles
  // it is unreachable -- the generator solves each SCALE so the near pin lands
  // EXACTLY on 0xFFFFFF (GEOM.DEPTHQUANT's own header) -- so a port for it
  // could never be seen to fire, and a counter nobody can fire is not evidence.
  // The refusal IS summed: a profile outside the three is refused, and the
  // directed test fires it from `fl_profile_i`.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [31:0] dq_sat_c      [3];
  /* verilator lint_on UNUSEDSIGNAL */
  logic [23:0] lane_r_q      [3];
  logic [5:0]  lane_k_q      [3];

  // Unread per-lane outputs, named rather than left as empty connections: the
  // behind bit and source id come back unchanged from what this block sent,
  // and the near/far clamps are the LAW's normal behaviour, not faults.
  /* verilator lint_off UNUSEDSIGNAL */
  logic        dq_d_behind [3];
  logic [SRCW-1:0] dq_d_src [3];
  logic [31:0] dq_vertices_c [3];
  logic [31:0] dq_near_c     [3];
  logic [31:0] dq_far_c      [3];
  /* verilator lint_on UNUSEDSIGNAL */

  // the shared reciprocal
  logic        rcp_v_valid, rcp_v_ready, rcp_r_valid, rcp_r_ready;
  logic [23:0] rcp_d, rcp_r;
  logic [5:0]  rcp_k;
  logic [7:0]  rcp_v_tok, rcp_r_tok;
  /* verilator lint_off UNUSEDSIGNAL */
  logic        rcp_d_zero, rcp_qerr, rcp_idle;
  logic [31:0] rcp_accepted, rcp_completed, rcp_mul_jobs, rcp_zero_jobs,
               rcp_phase_jobs, rcp_negcorr_jobs;
  logic [5:0]  rcp_occupancy;
  /* verilator lint_on UNUSEDSIGNAL */

  genvar L;
  generate
    for (L = 0; L < 3; L = L + 1) begin : g_dq
      assign dq_v_valid[L] = (st_q == S_DEPTH) && !dq_started_q[L];
      assign dq_d_ready[L] = (st_q == S_DEPTH) && !dq_got_q[L];

      zhao_geom_depthquant #(
          .SRCW(SRCW)
      ) u_dq (
          .clk         (clk),
          .rst_n       (rst_n),
          .v_valid_i   (dq_v_valid[L]),
          .v_ready_o   (dq_v_ready[L]),
          .v_w_i       ({9'd0, cw_q[L]}),
          .v_behind_i  (cb_q[L]),
          .v_profile_i (cur_prof_c),
          .v_src_id_i  (tsrc_q),
          .d_valid_o   (dq_d_valid[L]),
          .d_ready_i   (dq_d_ready[L]),
          .d_invw24_o  (dq_invw[L]),
          .d_behind_o  (dq_d_behind[L]),
          .d_src_id_o  (dq_d_src[L]),
          .rcp_valid_o (dq_rcp_valid[L]),
          .rcp_ready_i (dq_rcp_ready[L]),
          .rcp_d_o     (dq_rcp_d[L]),
          .rcp_rvalid_i(dq_rcp_rvalid[L]),
          .rcp_rready_o(dq_rcp_rready[L]),
          .rcp_r_i     (lane_r_q[L]),
          .rcp_k_i     (lane_k_q[L]),
          .vertices_o     (dq_vertices_c[L]),
          .clamped_near_o (dq_near_c[L]),
          .clamped_far_o  (dq_far_c[L]),
          .saturated_o    (dq_sat_c[L]),
          .refused_o      (dq_refused_c[L])
      );

      // Results come back in COMPLETION order, steered by the token.
      assign dq_rcp_rvalid[L] = rcp_r_valid && (rcp_r_tok == 8'(L));

      // LATCHED AT THIS LANE'S OWN HANDSHAKE. GEOM.DEPTHQUANT reads the reply
      // in S_COMB, the clock AFTER it accepted it -- correct against a service
      // that holds its answer, wrong against a SHARED one: by then the bus may
      // carry another lane's result. Each lane is shown the value it took.
      always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
          lane_r_q[L] <= '0;
          lane_k_q[L] <= '0;
        end else if (dq_rcp_rvalid[L] && dq_rcp_rready[L]) begin
          lane_r_q[L] <= rcp_r;
          lane_k_q[L] <= rcp_k;
        end
      end
    end
  endgenerate

  // Request side: fixed priority, lane 0 first. Each lane asks at most once per
  // triangle, so priority cannot starve a lane of this triangle's work.
  always_comb begin
    rcp_v_valid     = 1'b0;
    rcp_d           = '0;
    rcp_v_tok       = '0;
    dq_rcp_ready[0] = 1'b0;
    dq_rcp_ready[1] = 1'b0;
    dq_rcp_ready[2] = 1'b0;
    if (dq_rcp_valid[0]) begin
      rcp_v_valid = 1'b1; rcp_d = dq_rcp_d[0]; rcp_v_tok = 8'd0;
      dq_rcp_ready[0] = rcp_v_ready;
    end else if (dq_rcp_valid[1]) begin
      rcp_v_valid = 1'b1; rcp_d = dq_rcp_d[1]; rcp_v_tok = 8'd1;
      dq_rcp_ready[1] = rcp_v_ready;
    end else if (dq_rcp_valid[2]) begin
      rcp_v_valid = 1'b1; rcp_d = dq_rcp_d[2]; rcp_v_tok = 8'd2;
      dq_rcp_ready[2] = rcp_v_ready;
    end
  end

  always_comb begin
    rcp_r_ready = 1'b0;
    if (rcp_r_tok == 8'd0) rcp_r_ready = dq_rcp_rready[0];
    if (rcp_r_tok == 8'd1) rcp_r_ready = dq_rcp_rready[1];
    if (rcp_r_tok == 8'd2) rcp_r_ready = dq_rcp_rready[2];
  end

  zhao_raster_rcp24_v4 #(
      .NCTX(RCP_NCTX),
      .TOKW(8)
  ) u_rcp (
      .clk        (clk),
      .rst_n      (rst_n),
      .v_valid_i  (rcp_v_valid),
      .v_ready_o  (rcp_v_ready),
      .d_i        (rcp_d),
      .v_tok_i    (rcp_v_tok),
      .r_valid_o  (rcp_r_valid),
      .r_ready_i  (rcp_r_ready),
      .r_o        (rcp_r),
      .k_o        (rcp_k),
      .d_zero_o   (rcp_d_zero),
      .r_tok_o    (rcp_r_tok),
      .accepted_o (rcp_accepted),
      .completed_o(rcp_completed),
      .mul_jobs_o (rcp_mul_jobs),
      .zero_jobs_o(rcp_zero_jobs),
      .phase_jobs_o(rcp_phase_jobs),
      .negcorr_jobs_o(rcp_negcorr_jobs),
      .occupancy_o(rcp_occupancy),
      .qerr_o     (rcp_qerr),
      .idle_o     (rcp_idle)
  );

  // The lanes' fault counters, summed: one number per fault kind.
  always_comb begin
    dq_refused_o   = dq_refused_c[0] + dq_refused_c[1] + dq_refused_c[2];
  end

  // ---- the machine ----------------------------------------------------------
  // The arena's `d` field (the projector's Q16.16 1/w, bits 73:42) is NOT read:
  // depth is invw24, which only GEOM.DEPTHQUANT makes, from `w` (owner ruling
  // D-4). Reading `d` as a depth would be the second profile conversion that
  // ruling forbids.
  /* verilator lint_off UNUSEDSIGNAL */
  wire [PAYLOAD_W-1:0] rp = rep_payload_i;
  /* verilator lint_on UNUSEDSIGNAL */
  wire rep_bad_ref_c  = rep_valid_i && rep_refuse_i;
  wire rep_bad_miss_c = rep_valid_i && !rep_refuse_i && !rep_hit_i;

  integer ai;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_q            <= S_IDLE;
      need_q          <= '0;
      nh_q            <= '0;
      mask_q          <= '0;
      done_seen_q     <= 1'b0;
      sl_arena_q[0]   <= '0;
      sl_arena_q[1]   <= '0;
      sl_gen_q[0]     <= '0;
      sl_gen_q[1]     <= '0;
      sl_view_q[0]    <= 1'b0;
      sl_view_q[1]    <= 1'b0;
      vs_q            <= 1'b0;
      rk_rel_q        <= 1'b0;
      tv_q[0]         <= '0;
      tv_q[1]         <= '0;
      tv_q[2]         <= '0;
      tmat_q          <= '0;
      trast_q         <= '0;
      tsrc_q          <= '0;
      ik_q            <= '0;
      rk_q            <= '0;
      any_ref_q       <= 1'b0;
      any_miss_q      <= 1'b0;
      cb_q            <= '0;
      dq_started_q    <= '0;
      dq_got_q        <= '0;
      prof_seen_q     <= '0;
      for (ai = 0; ai < 3; ai = ai + 1) begin
        cx_q[ai]   <= '0;
        cy_q[ai]   <= '0;
        cw_q[ai]   <= '0;
        ca_q[ai]   <= '0;
        invw_q[ai] <= '0;
      end
      for (ai = 0; ai < NA; ai = ai + 1) prof_q[ai] <= '0;
      meshlets_o      <= '0;
      groups_o        <= '0;
      triangles_in_o  <= '0;
      triangles_out_o <= '0;
      refused_o       <= '0;
      missed_o        <= '0;
      att_skew_o      <= '0;
      profile_mixed_o <= '0;
      view_bad_o      <= '0;
    end else begin
      // --- the per-arena profile, from the projector's own landings ---------
      // An OPEN clears the arena's entry; its first landing sets it; a later
      // landing that disagrees is counted. Open wins on a same-cycle collision
      // because GEOM.GROUP_SEQ never lands into an arena it is opening.
      if (fl_valid_i) begin
        if (!prof_seen_q[fl_arena_i]) begin
          prof_q[fl_arena_i]      <= fl_profile_i;
          prof_seen_q[fl_arena_i] <= 1'b1;
        end else if ((prof_q[fl_arena_i] != fl_profile_i) &&
                     (profile_mixed_o != 32'hFFFF_FFFF)) begin
          profile_mixed_o <= profile_mixed_o + 32'd1;
        end
      end
      if (op_valid_i) prof_seen_q[op_arena_i] <= 1'b0;

      // --- the attribute store must answer with the arena's timing ---------
      if ((att_rep_valid_i != rep_valid_i) && (att_skew_o != 32'hFFFF_FFFF))
        att_skew_o <= att_skew_o + 32'd1;

      // --- the walk's end may arrive in any non-idle state ------------------
      if ((st_q != S_IDLE) && m_done_i) done_seen_q <= 1'b1;

      case (st_q)
        S_IDLE: begin
          if (mt_valid_i) begin
            need_q      <= mt_need_c;
            mask_q      <= mt_view_mask_i;
            nh_q        <= '0;
            done_seen_q <= 1'b0;
            rk_rel_q    <= 1'b0;
            st_q        <= (mt_need_c == 2'd0) ? S_TRI : S_HAND;
          end
        end

        S_HAND: begin
          if (grp_valid_i) begin
            sl_arena_q[nh_q[0]] <= grp_arena_i;
            sl_gen_q[nh_q[0]]   <= grp_gen_i;
            sl_view_q[nh_q[0]]  <= grp_view_i;
            if (groups_o != 32'hFFFF_FFFF) groups_o <= groups_o + 32'd1;
            if (!mask_q[grp_view_i] && (view_bad_o != 32'hFFFF_FFFF))
              view_bad_o <= view_bad_o + 32'd1;
            nh_q <= nh_q + 2'd1;
            if ((nh_q + 2'd1) == need_q) st_q <= S_TRI;
          end
        end

        S_TRI: begin
          if (t_valid_i) begin
            tv_q[0] <= t_v0_i;
            tv_q[1] <= t_v1_i;
            tv_q[2] <= t_v2_i;
            tmat_q  <= t_material_i;
            trast_q <= t_raster_i;
            tsrc_q  <= t_src_id_i;
            if (triangles_in_o != 32'hFFFF_FFFF) triangles_in_o <= triangles_in_o + 32'd1;
            vs_q       <= 1'b0;
            ik_q       <= '0;
            rk_q       <= '0;
            any_ref_q  <= 1'b0;
            any_miss_q <= 1'b0;
            // A meshlet that holds no arena cannot draw: its triangles are
            // taken and dropped as refused, so the walk still drains.
            if (need_q == 2'd0) begin
              if (refused_o != 32'hFFFF_FFFF) refused_o <= refused_o + 32'd1;
            end else begin
              st_q <= S_LOOK;
            end
          end else if (done_seen_q || m_done_i) begin
            rk_rel_q <= 1'b0;
            st_q     <= S_REL;
          end
        end

        S_LOOK: begin
          if (look_valid_o && look_ready_i) ik_q <= ik_q + 2'd1;
          if (rep_valid_i) begin
            cx_q[rk_q] <= $signed(rp[20:0]);
            cy_q[rk_q] <= $signed(rp[41:21]);
            cw_q[rk_q] <= rp[104:74];
            cb_q[rk_q] <= rp[105];
            ca_q[rk_q] <= att_rep_data_i;
            if (rep_bad_ref_c)  any_ref_q  <= 1'b1;
            if (rep_bad_miss_c) any_miss_q <= 1'b1;
            rk_q <= rk_q + 2'd1;
            if (rk_q == 2'd2) begin
              // The third reply. Judge the triangle on all three.
              if (any_ref_q || rep_bad_ref_c) begin
                if (refused_o != 32'hFFFF_FFFF) refused_o <= refused_o + 32'd1;
                st_q <= S_EMIT;       // resolved below as a drop
              end else if (any_miss_q || rep_bad_miss_c) begin
                if (missed_o != 32'hFFFF_FFFF) missed_o <= missed_o + 32'd1;
                st_q <= S_EMIT;
              end else begin
                dq_started_q <= '0;
                dq_got_q     <= '0;
                st_q         <= S_DEPTH;
              end
            end
          end
        end

        S_DEPTH: begin
          for (ai = 0; ai < 3; ai = ai + 1) begin
            if (dq_v_valid[ai] && dq_v_ready[ai]) dq_started_q[ai] <= 1'b1;
            if (dq_d_valid[ai] && dq_d_ready[ai]) begin
              dq_got_q[ai] <= 1'b1;
              invw_q[ai]   <= dq_invw[ai];
            end
          end
          if ((dq_got_q | ({dq_d_valid[2], dq_d_valid[1], dq_d_valid[0]} &
                           {dq_d_ready[2], dq_d_ready[1], dq_d_ready[0]})) == 3'b111)
            st_q <= S_EMIT;
        end

        S_EMIT: begin
          // A dropped triangle (a refused or missed corner) passes through here
          // for ONE clock with its flag set and is never offered: `o_valid_o`
          // is gated below by the flags. A drawable one waits for `o_ready_i`.
          if (any_ref_q || any_miss_q || o_ready_i) begin
            if (!(any_ref_q || any_miss_q) && (triangles_out_o != 32'hFFFF_FFFF))
              triangles_out_o <= triangles_out_o + 32'd1;
            ik_q       <= '0;
            rk_q       <= '0;
            any_ref_q  <= 1'b0;
            any_miss_q <= 1'b0;
            if ((need_q == 2'd2) && (vs_q == 1'b0)) begin
              vs_q <= 1'b1;
              st_q <= S_LOOK;
            end else begin
              st_q <= S_TRI;
            end
          end
        end

        S_REL: begin
          if (af_release_o) begin
            if (meshlets_o != 32'hFFFF_FFFF) meshlets_o <= meshlets_o + 32'd1;
            done_seen_q <= 1'b0;
            st_q        <= S_IDLE;
          end else begin
            rk_rel_q <= 1'b1;
          end
        end

        default: st_q <= S_IDLE;
      endcase
    end
  end

endmodule : zhao_geom_replay

`default_nettype wire
