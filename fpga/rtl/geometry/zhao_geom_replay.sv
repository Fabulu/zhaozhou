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
// TWO MESHLETS IN FLIGHT -- OWNER RULING R57 (2026-09-20)
// ---------------------------------------------------------------------------
// R47 measured the composed meshlet loop at 305 clocks with ZERO overlap
// between a meshlet's VERTEX phase and its REPLAY phase, and R57 found the
// cause: four "accept only when idle" gates, of which this block held three by
// itself -- `mt_ready_o`, and, through the buffer release it owns, GEOM.
// ASSETFETCH's `m_ready_o` and GEOM.ASSEMBLE's `m_ready_o`. A meshlet's
// triangle descriptors were produced by ASSEMBLE in about three clocks each and
// consumed here at about fifteen, so ASSEMBLE spent the whole replay stalled on
// `t_ready_o`, its walk never ended, and the asset buffer could not be released
// until the LAST triangle had been drawn.
//
// The fix is a QUEUE and a second meshlet slot, both here:
//
//   * an INTAKE side takes the token, the handles and EVERY TriangleDescriptor
//     of a meshlet as fast as they are offered, pushing {v0,v1,v2} into a
//     `TRIQ_DEPTH`-entry queue (M10K-shaped: one word, one write port, one read
//     port). ASSEMBLE's walk therefore ends at ITS OWN rate, and `m_done_i`
//     arrives while the meshlet is still being drawn;
//   * `af_release_o` fires as soon as the two readers the entry-I38 proof names
//     are done -- which is now EARLY, not at the last emission (see below);
//   * an EMIT side walks the queue for the OLDER slot while the intake side
//     fills the newer one, so meshlet N+1's vertex phase runs concurrently with
//     meshlet N's replay.
//
// TWO SLOTS AND NOT MORE, because `GEOM_ARENAS` is 4 and a meshlet holds one
// arena per visible view: two meshlets in flight is exactly the arena budget.
// A third would stall in GEOM.GROUP_SEQ's allocator instead, which is the same
// wait one block further from where it can be understood.
//
// THE I38 PROOF IS UNCHANGED IN KIND AND STRONGER IN TIME. GEOM.ASSETFETCH
// holds one meshlet until "whoever knows that BOTH readers have finished" says
// so. This block still knows, structurally, and by exactly the same two facts:
//   * the VERTEX reader is done: every handle arrives only after GEOM.GROUP_SEQ
//     sealed, which it does only after `count` vertices LANDED, which needs all
//     `count` records to have left GEOM.ASSETFETCH's vertex stream;
//   * the INDEX reader is done: GEOM.ASSEMBLE's `m_done_i` pulses on every way
//     its walk ends, after its last triangle was accepted HERE -- and "accepted
//     here" now means "in the queue", which is still after ASSETFETCH's index
//     service answered it.
// What changed is only WHEN both hold: `closed && handles-complete` rather than
// `closed && every triangle drawn`. Emission reads the ARENA and the STORE, not
// the asset buffer, so nothing the release frees is still being read. A meshlet
// GEOM.GROUP_SEQ would refuse (no vertices, or no visible view) expects NO
// handles and is marked handles-complete at its token, exactly as before, so it
// cannot wedge here.
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
// A triangle offered while the queue is FULL is refused on `t_ready_o` and the
// refusal is counted on `triq_stall_o` -- backpressure, never a drop. The queue
// is sized at twice `MAX_TRIANGLES` so the two-slot pipeline cannot fill it with
// legal traffic; `triq_stall_o` is therefore the instrument that says the
// sizing assumption still holds, and it is fired by stimulus in
// tests/geometry/geom_replay_directed.cpp CASE M.
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
    // GEOM.ASSETFETCH's own limit: the most triangles one meshlet may carry.
    parameter int unsigned MAX_TRIANGLES = 126,
    // The descriptor queue. TWO meshlets are in flight, so it holds two full
    // meshlets and the pipeline never backpressures itself. Power of two.
    parameter int unsigned TRIQ_DEPTH    = 256
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
  // The draw's MATERIAL_SET handle32 -- the other half of MATERIAL.RESOLVE's
  // request (core entry I49). PER-MESHLET, like the material id and the
  // raster word beside it, and latched by the same enable, so the three
  // cannot separate.
  input  wire [31:0]             t_material_set_i,
  // The draw's SEMANTIC WEIGHT -- MATERIAL.RESOLVE's quality tier. Per-meshlet
  // like the three beside it.
  input  wire [ 7:0]             t_quality_tier_i,
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
    // ---- THE CORNER'S IDENTITY, ARENAID 2026-09-25 -------------------------
    // {arena, generation, index} for each corner of the emitted triangle --
    // the key this block already presents on `look_*_o`, now carried out with
    // the reply it produced. It is the console's vertex identity: the arena
    // handle names the (meshlet instance, view), the generation separates one
    // use of that handle from the next, and the index is the vertex's ordinal
    // among the batch's decoded vertices.
    //
    // CAPTURED AT ISSUE, NOT AT REPLY. `ik_q` is the corner being OFFERED and
    // `rk_q` the corner being ANSWERED, and they are not the same corner while
    // a lookup is in flight. Reading `look_*_o` at the reply would pair corner
    // rk's position with corner ik's identity -- "response A's data and B's
    // metadata", the defect this repository names in three separate headers.
    // So the key is written into `ck_q[ik_q]` by the same enable that advances
    // `ik_q`, and the output reads the slot, never the live nets.
    output wire [ARENA_W+GEN_W+INDEX_W-1:0] o_key_a_o,
    output wire [ARENA_W+GEN_W+INDEX_W-1:0] o_key_b_o,
    output wire [ARENA_W+GEN_W+INDEX_W-1:0] o_key_c_o,
    output wire                    o_view_o,
    output wire [SRCW-1:0]         o_src_id_o,
    output wire [15:0]             o_material_o,
    output wire [31:0]             o_raster_o,
  output wire [31:0]             o_material_set_o,
  output wire [ 7:0]             o_quality_tier_o,

    // ---- evidence -------------------------------------------------------------
    output logic [31:0]            meshlets_o,       // meshlets released
    output logic [31:0]            groups_o,         // handles taken
    output logic [31:0]            triangles_in_o,   // descriptors taken
    output logic [31:0]            triangles_out_o,  // triangles emitted (all views)
    output logic [31:0]            refused_o,        // dropped: a corner refused
    output logic [31:0]            missed_o,         // dropped: a corner missed
    output logic [31:0]            att_skew_o,       // store and arena disagreed on timing
    output logic [31:0]            view_bad_o,       // a handle for a view not in the mask
    output logic [31:0]            poisoned_o,       // dropped: the batch lost a record (R31)
    output logic [31:0]            triq_stall_o      // a descriptor refused: the queue was full
);

  initial begin
    if (PAYLOAD_W != 106)
      $fatal(1, "zhao_geom_replay: PAYLOAD_W is %0d; the arena packs {behind,w,d,y,x} = 106", PAYLOAD_W);
    if (VIDW < INDEX_W)
      $fatal(1, "zhao_geom_replay: VIDW (%0d) narrower than INDEX_W (%0d)", VIDW, INDEX_W);
    if ((TRIQ_DEPTH & (TRIQ_DEPTH - 1)) != 0)
      $fatal(1, "zhao_geom_replay: TRIQ_DEPTH (%0d) is not a power of two", TRIQ_DEPTH);
    if (TRIQ_DEPTH < 2 * MAX_TRIANGLES)
      $fatal(1, "zhao_geom_replay: TRIQ_DEPTH (%0d) holds fewer than the two meshlets in flight (2 x %0d)",
             TRIQ_DEPTH, MAX_TRIANGLES);
  end

  localparam int unsigned QAW  = $clog2(TRIQ_DEPTH);
  localparam int unsigned QW   = 3 * VIDW;
  localparam int unsigned OCCW = QAW + 1;

  // ---- the two meshlet slots ------------------------------------------------
  // Written by the intake side, read by the emit side; never the same slot at
  // the same time, because a slot is allocated at its token and freed only when
  // the emit side retires it.
  logic               busy_q    [2];   // token taken, not yet retired
  logic               hc_q      [2];   // every handle this meshlet expects is in
  logic               closed_q  [2];   // GEOM.ASSEMBLE's walk for it has ended
  logic               relsent_q [2];   // its asset buffer has been released
  logic [1:0]         need_q    [2];
  logic [1:0]         mask_q    [2];
  logic [1:0]         nh_q      [2];
  logic               pois_q    [2];
  logic [ARENA_W-1:0] sa_q      [2][2];
  logic [GEN_W-1:0]   sg_q      [2][2];
  logic               sv_q      [2][2];
  logic [15:0]        mat_q     [2];
  logic [31:0]        rast_q    [2];
  logic [31:0]        mset_q    [2];
  logic [ 7:0]        qtier_q   [2];
  logic [SRCW-1:0]    src_q     [2];
  logic [OCCW-1:0]    ntri_q    [2];   // descriptors pushed for this meshlet

  logic mr_q;                          // the slot the emit side is walking

  // THE FOUR PORT POINTERS, each the OLDEST slot that still owes that port its
  // work. They are decodes of `mr_q` and the slot flags, never counters of their
  // own: a second counter is a second opinion about which meshlet is which, and
  // that is the drift this file's own header warns about.
  //
  // Slot allocation is strictly in order, so `busy_q[~mr_q] && !busy_q[mr_q]`
  // is unreachable and no pointer has to consider it.
  wire tkp_c = busy_q[mr_q]                       ? ~mr_q : mr_q;
  wire hp_c  = (busy_q[mr_q] && !hc_q[mr_q])      ?  mr_q : ~mr_q;
  wire wp_c  = (busy_q[mr_q] && !closed_q[mr_q])  ?  mr_q : ~mr_q;
  wire rp_c  = (busy_q[mr_q] && !relsent_q[mr_q]) ?  mr_q : ~mr_q;

  // ---- the descriptor queue -------------------------------------------------
  // One word, one write port, one read port, synchronous read: the shape a
  // Cyclone V M10K infers. The emit side spends about fifteen clocks on a
  // triangle and the queue refills its head in two, so the read latency is
  // paid ONCE per run of triangles and never per triangle.
  logic [QW-1:0]   triq_q [TRIQ_DEPTH];
  logic [QAW-1:0]  wptr_q, rptr_q;
  logic [OCCW-1:0] qcnt_q;    // entries sitting in the RAM, not yet read
  logic [OCCW-1:0] occ_q;     // entries anywhere in the queue, head included
  logic            rdp_q;     // a read is in flight
  logic            head_v_q;
  logic [QW-1:0]   head_q;
  logic [QW-1:0]   ram_q;

  wire q_full_c = (occ_q == OCCW'(TRIQ_DEPTH));

  // ---- intake handshakes ----------------------------------------------------
  assign mt_ready_o  = !busy_q[tkp_c];
  assign grp_ready_o = busy_q[hp_c] && !hc_q[hp_c];
  assign t_ready_o   = busy_q[wp_c] && !closed_q[wp_c] && !q_full_c;

  wire tk_take_c  = mt_valid_i  && mt_ready_o;
  wire grp_take_c = grp_valid_i && grp_ready_o;
  wire t_take_c   = t_valid_i   && t_ready_o;

  wire [1:0] mt_need_c = ((mt_vertex_count_i == 8'd0) || (mt_view_mask_i == 2'b00))
                         ? 2'd0
                         : (2'(mt_view_mask_i[0]) + 2'(mt_view_mask_i[1]));

  // ---- the asset-buffer release (entry I38) ---------------------------------
  // A ONE-CYCLE pulse the cycle both readers are proven done, for the OLDEST
  // meshlet that has not had one. `relsent_q` is what makes it one pulse.
  assign af_release_o = busy_q[rp_c] && closed_q[rp_c] && hc_q[rp_c] && !relsent_q[rp_c];

  // ---- the emit machine -----------------------------------------------------
  localparam logic [1:0] E_IDLE = 2'd0;
  localparam logic [1:0] E_LOOK = 2'd1;
  localparam logic [1:0] E_EMIT = 2'd2;
  localparam logic [1:0] E_REL  = 2'd3;

  logic [1:0] est_q;

  logic [VIDW-1:0]    tv_q [3];
  logic [1:0]         ik_q;            // lookups issued, 0..3
  logic [1:0]         rk_q;            // replies taken, 0..3
  logic               vs_q;            // the slot being replayed (0 or 1)
  logic               rk_rel_q;        // the arena being released
  logic               any_ref_q, any_miss_q;
  logic signed [20:0] cx_q [3];
  logic signed [20:0] cy_q [3];
  logic [2:0]         cb_q;
  // The three corner identities, written at ISSUE (see o_key_a_o's comment).
  logic [ARENA_W+GEN_W+INDEX_W-1:0] ck_q [0:2];
  logic [ATTRW-1:0]   ca_q [3];
  logic [23:0]        invw_q [3];
  logic [OCCW-1:0]    tdone_q;         // descriptors taken off the queue for mr_q

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

  assign look_valid_o = (est_q == E_LOOK) && (ik_q != 2'd3);
  assign look_arena_o = sa_q[mr_q][vs_q];
  assign look_gen_o   = sg_q[mr_q][vs_q];
  assign look_index_o = look_ix_c;

  // ---- the arena release ----------------------------------------------------
  assign rel_valid_o  = (est_q == E_REL) && (need_q[mr_q] != 2'd0);
  assign rel_arena_o  = sa_q[mr_q][rk_rel_q];
  wire rel_done_c     = (need_q[mr_q] == 2'd0) ||
                        (rk_rel_q == (need_q[mr_q] == 2'd2));

  // ---- the replayed triangle -----------------------------------------------
  assign o_valid_o    = (est_q == E_EMIT) && !any_ref_q && !any_miss_q;
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
  assign o_key_a_o    = ck_q[0];
  assign o_key_b_o    = ck_q[1];
  assign o_key_c_o    = ck_q[2];
  assign o_view_o     = sv_q[mr_q][vs_q];
  assign o_src_id_o   = src_q[mr_q];
  assign o_material_o = mat_q[mr_q];
  assign o_raster_o   = rast_q[mr_q];
  assign o_material_set_o = mset_q[mr_q];
  assign o_quality_tier_o = qtier_q[mr_q];

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

  // The emit side takes a descriptor off the queue when it has one and the
  // meshlet's handles are all in (it needs the arena to look anything up).
  //
  // THE HEAD MAY ALREADY HOLD THE NEXT MESHLET'S FIRST DESCRIPTOR, because the
  // queue is one FIFO across both slots and the intake side runs ahead. So what
  // says a descriptor is THIS meshlet's is its ORDINAL, not the head's validity:
  // descriptor `tdone_q` belongs to `mr_q` exactly while `tdone_q` is below the
  // count pushed for it. Reading `!head_v_q` as "this meshlet is finished" would
  // replay the next meshlet's first triangle against this one's arenas -- a
  // plausible corner from the wrong mesh, which is the class of fault this
  // block's own refusals exist to prevent.
  wire emit_armed_c = busy_q[mr_q] && hc_q[mr_q];
  wire tri_left_c   = (tdone_q < ntri_q[mr_q]);
  wire pop_c        = (est_q == E_IDLE) && emit_armed_c && head_v_q && tri_left_c;
  wire finish_c     = (est_q == E_IDLE) && emit_armed_c && !tri_left_c &&
                      closed_q[mr_q];

  // Queue read issue: fill the head whenever it is (or is about to be) empty.
  wire rd_issue_c = (!head_v_q || pop_c) && !rdp_q && (qcnt_q != '0);

  integer ai;
  integer si;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      for (si = 0; si < 2; si = si + 1) begin
        busy_q[si]    <= 1'b0;
        hc_q[si]      <= 1'b0;
        closed_q[si]  <= 1'b0;
        relsent_q[si] <= 1'b0;
        need_q[si]    <= '0;
        mask_q[si]    <= '0;
        nh_q[si]      <= '0;
        pois_q[si]    <= 1'b0;
        sa_q[si][0]   <= '0;
        sa_q[si][1]   <= '0;
        sg_q[si][0]   <= '0;
        sg_q[si][1]   <= '0;
        sv_q[si][0]   <= 1'b0;
        sv_q[si][1]   <= 1'b0;
        mat_q[si]     <= '0;
        rast_q[si]    <= '0;
      mset_q[si]    <= '0;
      qtier_q[si]   <= '0;
        src_q[si]     <= '0;
        ntri_q[si]    <= '0;
      end
      mr_q            <= 1'b0;
      est_q           <= E_IDLE;
      wptr_q          <= '0;
      rptr_q          <= '0;
      qcnt_q          <= '0;
      occ_q           <= '0;
      rdp_q           <= 1'b0;
      head_v_q        <= 1'b0;
      head_q          <= '0;
      ram_q           <= '0;
      tv_q[0]         <= '0;
      tv_q[1]         <= '0;
      tv_q[2]         <= '0;
      ik_q            <= '0;
      rk_q            <= '0;
      vs_q            <= 1'b0;
      rk_rel_q        <= 1'b0;
      any_ref_q       <= 1'b0;
      any_miss_q      <= 1'b0;
      cb_q            <= '0;
      tdone_q         <= '0;
      for (ai = 0; ai < 3; ai = ai + 1) begin
        cx_q[ai]   <= '0;
        ck_q[ai]   <= '0;
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
      poisoned_o      <= '0;
      triq_stall_o    <= '0;
    end else begin

      // --- the attribute store must answer with the arena's timing ---------
      if ((att_rep_valid_i != rep_valid_i) && (att_skew_o != 32'hFFFF_FFFF))
        att_skew_o <= att_skew_o + 32'd1;

      // ====================== THE INTAKE SIDE ==============================
      // The token allocates a slot.
      if (tk_take_c) begin
        busy_q[tkp_c]    <= 1'b1;
        need_q[tkp_c]    <= mt_need_c;
        mask_q[tkp_c]    <= mt_view_mask_i;
        nh_q[tkp_c]      <= '0;
        // A meshlet that expects no handle is handles-complete at once, which
        // is what keeps a GROUP_SEQ refusal from wedging the pipeline.
        hc_q[tkp_c]      <= (mt_need_c == 2'd0);
        closed_q[tkp_c]  <= 1'b0;
        relsent_q[tkp_c] <= 1'b0;
        pois_q[tkp_c]    <= 1'b0;
        ntri_q[tkp_c]    <= '0;
      end

      // A sealed handle.
      if (grp_take_c) begin
        sa_q[hp_c][nh_q[hp_c][0]] <= grp_arena_i;
        sg_q[hp_c][nh_q[hp_c][0]] <= grp_gen_i;
        sv_q[hp_c][nh_q[hp_c][0]] <= grp_view_i;
        // One poisoned handle poisons the MESHLET: both views were filled from
        // the same record stream, so the same hole is in both.
        if (grp_poison_i) pois_q[hp_c] <= 1'b1;
        if (groups_o != 32'hFFFF_FFFF) groups_o <= groups_o + 32'd1;
        if (!mask_q[hp_c][grp_view_i] && (view_bad_o != 32'hFFFF_FFFF))
          view_bad_o <= view_bad_o + 32'd1;
        nh_q[hp_c] <= nh_q[hp_c] + 2'd1;
        if ((nh_q[hp_c] + 2'd1) == need_q[hp_c]) hc_q[hp_c] <= 1'b1;
      end

      // A TriangleDescriptor, straight into the queue.
      if (t_take_c) begin
        triq_q[wptr_q] <= {t_v2_i, t_v1_i, t_v0_i};
        wptr_q         <= wptr_q + QAW'(1);
        ntri_q[wp_c]   <= ntri_q[wp_c] + OCCW'(1);
        // Per-MESHLET fields, and GEOM.ASSEMBLE latches them for the whole walk,
        // so writing them on every descriptor writes the same value.
        mat_q[wp_c]    <= t_material_i;
        rast_q[wp_c]   <= t_raster_i;
      mset_q[wp_c]   <= t_material_set_i;
      qtier_q[wp_c]  <= t_quality_tier_i;
        src_q[wp_c]    <= t_src_id_i;
        if (triangles_in_o != 32'hFFFF_FFFF) triangles_in_o <= triangles_in_o + 32'd1;
      end else if (t_valid_i && busy_q[wp_c] && !closed_q[wp_c] && q_full_c) begin
        // Backpressure, counted. `t_ready_o` is low, so nothing is dropped --
        // this is the instrument that says TRIQ_DEPTH is still large enough.
        if (triq_stall_o != 32'hFFFF_FFFF) triq_stall_o <= triq_stall_o + 32'd1;
      end

      // The walk's end closes the slot for writing.
      if (m_done_i && busy_q[wp_c] && !closed_q[wp_c]) closed_q[wp_c] <= 1'b1;

      // The asset buffer goes back, once.
      if (af_release_o) relsent_q[rp_c] <= 1'b1;

      // ====================== THE QUEUE ====================================
      if (rd_issue_c) begin
        ram_q  <= triq_q[rptr_q];
        rptr_q <= rptr_q + QAW'(1);
        rdp_q  <= 1'b1;
      end else begin
        rdp_q  <= 1'b0;
      end

      if (rdp_q)      head_v_q <= 1'b1;
      else if (pop_c) head_v_q <= 1'b0;
      if (rdp_q)      head_q   <= ram_q;

      qcnt_q <= qcnt_q + (t_take_c ? OCCW'(1) : OCCW'(0)) - (rd_issue_c ? OCCW'(1) : OCCW'(0));
      occ_q  <= occ_q  + (t_take_c ? OCCW'(1) : OCCW'(0)) - (pop_c     ? OCCW'(1) : OCCW'(0));

      // ====================== THE EMIT SIDE ================================
      case (est_q)
        E_IDLE: begin
          if (pop_c) begin
            tv_q[0]    <= head_q[0        +: VIDW];
            tv_q[1]    <= head_q[VIDW     +: VIDW];
            tv_q[2]    <= head_q[2 * VIDW +: VIDW];
            tdone_q    <= tdone_q + OCCW'(1);
            vs_q       <= 1'b0;
            ik_q       <= '0;
            rk_q       <= '0;
            any_ref_q  <= 1'b0;
            any_miss_q <= 1'b0;
            // A meshlet that holds no arena cannot draw: its triangles are
            // taken and dropped as refused, so the walk still drains.
            if (need_q[mr_q] == 2'd0) begin
              if (refused_o != 32'hFFFF_FFFF) refused_o <= refused_o + 32'd1;
            end else if (pois_q[mr_q]) begin
              // R31: the batch lost a record, so every arena index after the
              // hole names the wrong vertex. The descriptor is taken so the
              // walk drains, and dropped WITHOUT a lookup -- a lookup would
              // HIT, with a plausible corner from the wrong vertex.
              if (poisoned_o != 32'hFFFF_FFFF) poisoned_o <= poisoned_o + 32'd1;
            end else begin
              est_q <= E_LOOK;
            end
          end else if (finish_c) begin
            rk_rel_q <= 1'b0;
            est_q    <= E_REL;
          end
        end

        E_LOOK: begin
          if (look_valid_o && look_ready_i) begin
            ik_q       <= ik_q + 2'd1;
            // ONE ENABLE, BOTH QUANTITIES: the key that is being asked and the
            // slot it will be answered into move on the same clock.
            ck_q[ik_q] <= {look_arena_o, look_gen_o, look_index_o};
          end
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
                est_q <= E_EMIT;       // resolved below as a drop
              end else if (any_miss_q || rep_bad_miss_c) begin
                if (missed_o != 32'hFFFF_FFFF) missed_o <= missed_o + 32'd1;
                est_q <= E_EMIT;
              end else begin
                // All three corners, their depth and their attributes are in
                // hand on this clock: nothing left to wait for (R31).
                est_q <= E_EMIT;
              end
            end
          end
        end

        E_EMIT: begin
          // A dropped triangle (a refused or missed corner) passes through here
          // for ONE clock with its flag set and is never offered: `o_valid_o`
          // is gated above by the flags. A drawable one waits for `o_ready_i`.
          if (any_ref_q || any_miss_q || o_ready_i) begin
            if (!(any_ref_q || any_miss_q) && (triangles_out_o != 32'hFFFF_FFFF))
              triangles_out_o <= triangles_out_o + 32'd1;
            ik_q       <= '0;
            rk_q       <= '0;
            any_ref_q  <= 1'b0;
            any_miss_q <= 1'b0;
            if ((need_q[mr_q] == 2'd2) && (vs_q == 1'b0)) begin
              vs_q  <= 1'b1;
              est_q <= E_LOOK;
            end else begin
              est_q <= E_IDLE;
            end
          end
        end

        E_REL: begin
          if (rel_done_c) begin
            if (meshlets_o != 32'hFFFF_FFFF) meshlets_o <= meshlets_o + 32'd1;
            busy_q[mr_q] <= 1'b0;
            tdone_q      <= '0;
            mr_q         <= ~mr_q;
            est_q        <= E_IDLE;
          end else begin
            rk_rel_q <= 1'b1;
          end
        end

        default: est_q <= E_IDLE;
      endcase
    end
  end

`ifndef SYNTHESIS
  // ENFORCED-BY: tests/geometry/geom_replay_directed.cpp
  // The reset is sensed ASYNCHRONOUSLY here, exactly as the datapath above
  // senses it: a checker that flopped the same net synchronously would make
  // `rst_n` both, which Verilator reports as SYNCASYNCNET and which is a real
  // recovery-timing hazard, not a lint nicety.
  logic chk_arm_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      chk_arm_q <= 1'b0;
    end else begin
      chk_arm_q <= 1'b1;
      // GEOM.ASSEMBLE is single-in-flight and takes its meshlet on the SAME
      // dispatcher-fork clock as this block takes the token, so every `m_done_i`
      // has an open slot waiting for it. If this fires, the fork has stopped
      // being an AND-fork and the queue is attributing triangles to the wrong
      // meshlet.
      a_done_has_slot: assert (!chk_arm_q || !m_done_i || (busy_q[wp_c] && !closed_q[wp_c]))
        else $error("zhao_geom_replay: m_done_i with no open meshlet slot");
      // Slots retire in order, so the newer one can never be busy alone.
      a_slots_in_order: assert (!(busy_q[~mr_q] && !busy_q[mr_q]))
        else $error("zhao_geom_replay: the newer meshlet slot outlived the older one");
    end
  end
`endif

endmodule : zhao_geom_replay

`default_nettype wire
