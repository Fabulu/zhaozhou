// zhao_geom_replay.sv -- GEOM.REPLAY: the geometry replay customer. One meshlet's
// sealed arena groups and its TriangleDescriptors in; screen triangles with their
// corner depth and vertex attributes out, one per triangle per visible view.
//
//     GEOM.GROUP_SEQ --- grp_* (1 or 2 sealed handles) ---\
//     GEOM.ASSEMBLE  --- t_* (v0,v1,v2) + m_done -----------> GEOM.REPLAY --> GEOM.CLIP
//                                                         |      |   ^
//     GEOM.PROJ_LANE <-- look_* (3 per triangle per view) -'      |
//     GEOM.VATTR     <-- the SAME look_* nets -------------------'
//                        (invw24 + slots 1..6, on the arena's clock)
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
// DEPTH IS READ, NOT COMPUTED HERE (owner ruling R31, 2026-09-19)
// ---------------------------------------------------------------------------
// Slot 0 of the ruling-5 packet is invw24, which only GEOM.DEPTHQUANT may make
// (owner ruling D-4). This block used to run three DEPTHQUANT lanes per
// TRIANGLE CORNER on a private reciprocal: MEASURED 56 clocks per
// view-triangle, 2.7x the frame at the 120,000-vertex tier, because a vertex
// shared by six corners paid for its depth six times and every triangle waited
// out a ~40-clock reciprocal round trip. invw24 is a property of the landed
// VERTEX in its view, so GEOM.VATTR now computes it once per landing (on
// `zhao_geom_depthquant_stream`, the same law) and returns it here on
// `att_invw_i` with the rest of the vertex's attributes. The per-arena profile
// table and `profile_mixed_o` moved with it.
//
// ---------------------------------------------------------------------------
// THE VERTEX-ATTRIBUTE STORE (owner ruling R11; entry I46, CLOSED)
// ---------------------------------------------------------------------------
// invw24 and slots 1..6 (u_over_w, v_over_w, r, g, b, alpha) are per-vertex,
// per-view, and live in GEOM.VATTR, keyed EXACTLY like the arena. It listens to
// this block's `look_*` nets -- the same three lookups -- and must answer with
// the arena's timing: `att_rep_valid_i` in the same cycle as `rep_valid_i`. A
// disagreement is counted on `att_skew_o`, whose operands come from two
// different memories. The store is this block's peer, not its part.
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK REFUSES
// ---------------------------------------------------------------------------
// A triangle with a REFUSED corner (bad arena, stale generation, unsealed, out
// of range) or a MISSED one (slot never written) is DROPPED and counted: it
// cannot be drawn, and drawing it from a stale or unwritten vertex would put
// somebody else's geometry on screen. A vertex id wider than the arena index is
// forced to an index the arena REFUSES rather than truncated onto a real slot.
// A meshlet whose GROUP_SEQ handle arrives POISONED (a record GEOM.VDECODE
// refused, owner ruling R31) has every triangle taken and dropped WITHOUT a
// lookup -- its later indices name the wrong vertices, so a lookup would HIT
// -- counted on `poisoned_o`, and its arenas are released as usual.
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
    parameter int unsigned ATTRW     = 6 * 32
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
    // GEOM.GROUP_SEQ's verdict that the batch lost a record upstream (a
    // GEOM.VDECODE refusal, owner ruling R31): the meshlet's triangles are
    // taken and DROPPED, and its arenas are still released.
    input  wire                    grp_poison_i,
    output wire                    rel_valid_o,
    output wire [ARENA_W-1:0]      rel_arena_o,

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
    input  wire [23:0]             att_invw_i,     // GEOM.VATTR's invw24 (R31)
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
    output logic [31:0]            view_bad_o,       // a handle for a view not in the mask
    output logic [31:0]            poisoned_o        // dropped: the batch lost a record (R31)
);

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
  localparam logic [2:0] S_EMIT  = 3'd5;
  localparam logic [2:0] S_REL   = 3'd6;

  logic [2:0] st_q;

  logic [1:0]         need_q;          // handles this meshlet expects, 0..2
  logic [1:0]         nh_q;            // handles taken
  logic [1:0]         mask_q;
  logic               done_seen_q;     // GEOM.ASSEMBLE's walk has ended
  logic               pois_q;          // a handle of this meshlet was poisoned
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
  logic [2:0]         cb_q;
  logic [ATTRW-1:0]   ca_q [3];
  logic [23:0]        invw_q [3];

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

  // ---- the machine ----------------------------------------------------------
  // The arena's `d` field (the projector's Q16.16 1/w, bits 73:42) and `w`
  // (bits 104:74) are NOT read: depth is invw24, which only GEOM.DEPTHQUANT
  // makes, from `w` (owner ruling D-4) -- once per landed vertex, in GEOM.VATTR,
  // which answers it on `att_invw_i`. Reading `d` as a depth would be the second
  // profile conversion that ruling forbids.
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
      pois_q          <= 1'b0;
      poisoned_o      <= '0;
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
      for (ai = 0; ai < 3; ai = ai + 1) begin
        cx_q[ai]   <= '0;
        cy_q[ai]   <= '0;
        ca_q[ai]   <= '0;
        invw_q[ai] <= '0;
      end
      meshlets_o      <= '0;
      groups_o        <= '0;
      triangles_in_o  <= '0;
      triangles_out_o <= '0;
      refused_o       <= '0;
      missed_o        <= '0;
      att_skew_o      <= '0;
      view_bad_o      <= '0;
    end else begin

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
            pois_q      <= 1'b0;
            rk_rel_q    <= 1'b0;
            st_q        <= (mt_need_c == 2'd0) ? S_TRI : S_HAND;
          end
        end

        S_HAND: begin
          if (grp_valid_i) begin
            sl_arena_q[nh_q[0]] <= grp_arena_i;
            sl_gen_q[nh_q[0]]   <= grp_gen_i;
            sl_view_q[nh_q[0]]  <= grp_view_i;
            // One poisoned handle poisons the MESHLET: both views were filled
            // from the same record stream, so the same hole is in both.
            if (grp_poison_i) pois_q <= 1'b1;
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
            end else if (pois_q) begin
              // R31: the batch lost a record, so every arena index after the
              // hole names the wrong vertex. The descriptor is taken so the
              // walk drains, and dropped WITHOUT a lookup -- a lookup would
              // HIT, with a plausible corner from the wrong vertex.
              if (poisoned_o != 32'hFFFF_FFFF) poisoned_o <= poisoned_o + 32'd1;
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
            cb_q[rk_q] <= rp[105];
            ca_q[rk_q]   <= att_rep_data_i;
            invw_q[rk_q] <= att_invw_i;
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
                // All three corners, their depth and their attributes are in
                // hand on this clock: nothing left to wait for (R31).
                st_q <= S_EMIT;
              end
            end
          end
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
