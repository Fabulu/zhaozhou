// zhao_part_clipfeed.sv -- PART.EXPAND's fan, made into a GEOM.CLIPDOOR client.
//
// ENFORCED-BY: tests/particles/part_clipfeed_directed.cpp:main
// Contract: design/contracts/PART.CLIPFEED.md
//
// ---------------------------------------------------------------------------
// WHY THIS BLOCK EXISTS
// ---------------------------------------------------------------------------
// Owner ruling 1 of 2026-09-22 authorises "the complete particle-to-raster
// connection, including the canonical depth conversion and any carrier still
// required by the current producer", and says in terms that it "is not
// permission to close the task after changing only the material gate".
//
// `design/contracts/GEOM.CLIPDOOR.md` had already MEASURED what the particle
// arm still owed, and this block is exactly those two things and nothing else:
//
//   1. THE CANONICAL DEPTH. `zhao_part_expand.t_d_o` is Q16.16 1/w
//      (`zref::ScreenV::d`); GEOM.CLIP's attribute slot 0 is invw24. The two
//      are different quantisations of one ordering and they SHARE ONE DEPTH
//      BUFFER with the mesh path, so a Q16.16 value in slot 0 z-tests wrong
//      against every mesh triangle. Owner ruling D-4: "all downstream consumers
//      receive only the canonical invw24. No consumer performs its own profile
//      conversion."
//   2. THE ATTRIBUTE PACKET. Seven 32-bit slots per corner, which a fan of
//      three screen vertices, one shared depth and one flat colour does not
//      have -- until R197 made the u/w and v/w slots don't-care for a declared
//      untextured primitive.
//
// It is a SEPARATE BLOCK rather than an addition to `zhao_part_expand` because
// that block is composed, unit-verified and has a committed width/winding
// measurement against it; and because a depth conversion inside an expansion
// block is a second home for a law that has one.
//
// ---------------------------------------------------------------------------
// THE CONVERSION IS A SECOND INSTANCE OF ONE LAW, NOT A SECOND LAW
// ---------------------------------------------------------------------------
// `zhao_geom_depthquant_stream` + `zhao_raster_rcp24_v4`, wired exactly as
// `zhao_geom_vattr` and `zhao_forge_assemble` wire them. There is no arithmetic
// in this file at all: no shift, no product, no rounding, no saturation. The
// `zhao_field_isqrt` precedent governs -- a second INSTANCE of one law is not a
// second law; a second EXPRESSION of it would be, and there is none here.
//
// THE INPUT IS `w`, WHICH IS WHY `zhao_part_project` AND `zhao_part_expand`
// GREW A LANE. The converter's own header: "It consumes w and performs its OWN
// reciprocal." `w` existed on the particle path all along -- the shared
// projector returns it at `zhao_part_project.a_w_i` -- and was dropped at that
// block's ladder queue. GEOM.CLIPDOOR.md located that; this packet carried it.
//
// ---------------------------------------------------------------------------
// THE ANSWERS COME BACK OUT OF ORDER, AND PARTICLES MUST NOT
// ---------------------------------------------------------------------------
// The stream answers "in the reciprocal's COMPLETION order with the caller's
// tag", and the reciprocal is multi-context, so two particles issued in order
// can land in either order. Particles are DEPTH-TESTED AND NOT DEPTH-WRITTEN
// (`draw_population`'s "pass-7 law: test only, no write"), so two particles
// that overlap are resolved by DRAW ORDER and by nothing else. Emitting them in
// completion order would reorder a blend.
//
// So there is a RING, and the tag IS the ring slot:
//
//   * a particle is accepted only when it holds a ring slot AND the converter
//     takes its `w` on the same clock, so the answer's home exists before the
//     question is asked;
//   * the answer lands by tag into that slot and sets its `landed` bit;
//   * the HEAD of the ring is emitted, and only when its bit is set.
//
// That is an in-order queue in front of an out-of-order engine, and it is the
// only state in this file. It is NOT the "independently advancing metadata
// queue" owner ruling 1 forbids: the thing it holds and the thing it is keyed
// by are ONE RECORD at ONE INDEX, written by one enable, and the material half
// does not travel through it at all -- a particle's material mode is a constant
// of the producer, presented at the door beside the beat it belongs to.
//
// AND IT CANNOT DEADLOCK. Every accepted particle has issued its `w`; the
// converter always answers; the landing's ready is constant 1 because the
// landing is a write into a slot nothing else writes. So the head's bit is
// always eventually set and the ring always drains.
//
// ---------------------------------------------------------------------------
// THE NARROWING 22 -> 21 IS LOSSLESS, AND IT IS CHECKED ANYWAY
// ---------------------------------------------------------------------------
// `zhao_part_expand`'s vertices are 22 bits and GEOM.CLIPDOOR's are 21.
// `tests/particles/part_expand_directed.cpp` section 7 proves exhaustively that
// `max |vertex| = 524288 + 4080 = 528368 < 2^20`, so the 22nd bit is HEADROOM
// and no input honouring `to_screen_xy`'s clamp can set it.
//
// The check here is not a second opinion about that measurement. It is the
// measurement's PREMISE made enforceable: the proof is conditional on the
// clamp, the clamp lives three blocks upstream, and a truncation that wraps
// silently turns a particle at the screen edge into one at the opposite edge.
// A vertex that does not fit is REFUSED WHOLE and counted on
// `range_refused_o`. That counter is reachable with legal stimulus AT THIS
// BLOCK'S OWN PORTS -- the port is 22 bits wide and a bench drives it -- so it
// is the `t_ack_i` shape and owes no committed mutant.
//
// ---------------------------------------------------------------------------
// WHAT THE DOOR BEAT DECLARES, FIELD BY FIELD, WITH ITS AUTHORITY
// ---------------------------------------------------------------------------
//   untex = 1        A polygon particle has NO TEXTURE COORDINATES BY LAW.
//                    `sprites.cpp::draw_population`'s tris branch calls
//                    `raster_tri(surf, vpp, a, b, cc, p.r, p.g, p.b, tm)` --
//                    three positions, three colour bytes, no `TextureSpan`.
//                    R197 is what makes that legal to declare.
//
//   material_mode    NO_MATERIAL. The SIBLING of the bit above and a DIFFERENT
//     = NONE         statement, which owner ruling 1 insists on keeping apart:
//                    "no texture coordinates" says the PRIMITIVE cannot support
//                    a material that samples; "no material" says the PRODUCER
//                    intentionally uses its own defined rendering profile. The
//                    same reference line establishes both, and the window is
//                    forbidden to infer either from the other.
//
//   material_set     ZERO, and this is not a tie-off: it is the declaration
//   material_id      `zhao_material_window.mode_contra_c` REQUIRES. A producer
//                    that declared NO_MATERIAL while handing over an identity
//                    it expected resolved would be refused and counted there.
//                    These two are what make this beat internally consistent.
//
//   quality_tier     ZERO. The tier is a label echoed by MATERIAL.RESOLVE on a
//                    request this producer never makes. Deliberately NOT part
//                    of the window's contradiction test, so zero here is the
//                    absence of a label rather than a claim about one.
//
//   behind = 3'b000  A FACT ABOUT THE PRODUCER, not a tie-off.
//                    `zhao_part_expand`'s `assign emits = take && p_in_i` means
//                    a behind-the-eye particle is consumed and emits nothing,
//                    so no beat can ever reach here with a behind corner.
//
//   cull_mode        `PART_CULL_MODE`, default CULL_NONE. `draw_population` is
//                    double-sided (`const TriMode m;` default), and the fan's
//                    2A is negative for every size -- GEOM.CLIP NORMALISES
//                    winding and REJECTS zero area, which is precisely why
//                    R187 put the door at its input.
//
//   slots 3,4,5      THE PARTICLE'S OWN r/g/b, expanded EXACTLY. R234 D1's
//                    Gouraud planes are not branched on `tri_untex_i`, because
//                    "an untextured primitive is still lit". The consumer's law
//                    is `zhao_raster_tile_pipe_v2::lit_unit8(v) = v[15:8]`
//                    saturating, so `{16'd0, c, 8'd0}` is its exact LEFT
//                    INVERSE and round-trips every one of the 256 bytes. This
//                    is DERIVED from the consumer, not invented. Three equal
//                    corner values are what GEOM.ATTRPACK turns into a constant
//                    plane; no special case exists or is needed.
//
//   slot 6           `PART_ALPHA`, default opaque. `raster_tri` is called with
//                    no alpha, and the reference's particles are opaque; the
//                    value is an ART VALUE and therefore a named editable
//                    parameter (CLAUDE.md rule 6), not a derived quantity.
//
//   slots 1,2        ZERO, agreeing with `zhao_geom_attrpack`, which branches
//                    on `tri_untex_i` and substitutes the zero operand for
//                    both. Agreeing with the block that overwrites them is not
//                    the same as relying on it.
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK DOES NOT CARRY, NAMED SO NOBODY READS IT AS AN OVERSIGHT
// ---------------------------------------------------------------------------
// `zhao_part_expand.t_depth_test_o` (1) and `t_depth_write_o` (0) -- the pass-7
// law -- do NOT enter here, and the reason is not particle-specific. In this
// console the raster state word is `zhao_console_core.render_state_i`, a
// BOUNDARY INPUT sampled per tile job at `zhao_raster_tile_pipe.job_state_i`.
// There is no per-primitive route for it for ANY producer: GEOM.CLIP carries
// `cull_mode` and consumes it internally, and nothing downstream of GEOM.SETUP
// takes a depth mode from the triangle. Adding one is a field through
// GEOM.CLIP, GEOM.SETUP, GEOM.BINNER and the shell door -- a subsystem, and a
// change to blocks on the critical path of every triangle the console draws.
//
// So a port here would have nowhere to go, which is a tie-off wearing a port's
// clothes. The gap is DECLARED in `zhao_console_core`'s INCOMPLETE block
// instead, where the completion register can see it. PART.EXPAND's two
// boundary outputs are untouched and still leave the module.
//
// Conservative SystemVerilog subset only; Quartus 17.0 syntax rules apply
// (elaboration checks inside `initial begin ... end`, explicit generate, no
// inline `for (genvar ...)`, loop variables declared inside their block).
`default_nettype none

module zhao_part_clipfeed #(
    // GEOM.CLIP's ruling-5 attribute packet.
    parameter int unsigned ATTRS      = 7,
    parameter int unsigned IDW        = 16,
    // WHICH SLOT CARRIES WHICH PLANE. Named rather than literal for the same
    // reason `zhao_console_core` names them: a ratified layout is still a knob.
    parameter int unsigned SLOT_INVW  = 0,
    parameter int unsigned SLOT_UOW   = 1,
    parameter int unsigned SLOT_VOW   = 2,
    parameter int unsigned SLOT_R     = 3,
    parameter int unsigned SLOT_G     = 4,
    parameter int unsigned SLOT_B     = 5,
    parameter int unsigned SLOT_ALPHA = 6,
    // THE RING. It is the in-order queue in front of the out-of-order
    // converter, and it must be at least as deep as the converter's context
    // count or IT becomes the rate limit rather than the reciprocal.
    parameter int unsigned SLOTS      = 16,
    parameter int unsigned DQ_SLOTS   = 16,
    parameter int unsigned RCP_NCTX   = 8,
    // AUTHORED, not derived. See the header: alpha is an art value, and the
    // cull mode is `draw_population`'s double-sided default stated as a knob.
    parameter int signed   PART_ALPHA     = 32'sd65536,  // 1.0, opaque
    parameter logic [1:0]  PART_CULL_MODE = 2'd0         // CULL_NONE
) (
    input var logic clk,
    input var logic rst_n,

    // ---- PART.EXPAND's fan --------------------------------------------------
    input  var logic               p_valid_i,
    output var logic               p_ready_o,
    input  var logic signed [21:0] p_ax_i,
    input  var logic signed [21:0] p_ay_i,
    input  var logic signed [21:0] p_bx_i,
    input  var logic signed [21:0] p_by_i,
    input  var logic signed [21:0] p_cx_i,
    input  var logic signed [21:0] p_cy_i,
    input  var logic        [30:0] p_w_i,
    input  var logic        [ 1:0] p_profile_i,
    input  var logic        [ 7:0] p_r_i,
    input  var logic        [ 7:0] p_g_i,
    input  var logic        [ 7:0] p_b_i,
    input  var logic        [IDW-1:0] p_src_id_i,

    // ---- one GEOM.CLIPDOOR client -------------------------------------------
    output var logic                 o_valid_o,
    input  var logic                 o_ready_i,
    output var logic signed [20:0]   o_ax_o,
    output var logic signed [20:0]   o_ay_o,
    output var logic signed [20:0]   o_bx_o,
    output var logic signed [20:0]   o_by_o,
    output var logic signed [20:0]   o_cx_o,
    output var logic signed [20:0]   o_cy_o,
    output var logic [2:0]           o_behind_o,
    output var logic [IDW-1:0]       o_src_id_o,
    output var logic                 o_untex_o,
    output var logic [1:0]           o_cull_mode_o,
    output var logic [ATTRS*32-1:0]  o_attr_a_o,
    output var logic [ATTRS*32-1:0]  o_attr_b_o,
    output var logic [ATTRS*32-1:0]  o_attr_c_o,
    output var logic [31:0]          o_material_set_o,
    output var logic [15:0]          o_material_id_o,
    output var logic [1:0]           o_material_mode_o,
    output var logic [7:0]           o_quality_tier_o,

    // ---- evidence -----------------------------------------------------------
    // THE CENSUS PAIR, and it DISCRIMINATES (ruling R95): a ring that stopped
    // draining shows `particles_o` climbing while `triangles_o` stands still,
    // and a converter that never answered shows the same -- while a block that
    // silently dropped a fan would show them diverging by exactly the drops.
    // Neither reads as healthy.
    output var logic [31:0]          particles_o,
    output var logic [31:0]          triangles_o,
    // A FAULT: a corner outside the +-2^20 the producer's clamp law permits.
    // The fan is refused WHOLE -- there is no half a triangle.
    output var logic [31:0]          range_refused_o,
    // NOT A FAULT: clocks in which a particle was offered and the ring had no
    // room. This is the block's own rate, and it is the number to read before
    // anyone changes SLOTS.
    output var logic [31:0]          stall_full_o,
    // THE CONVERTER'S OWN, forwarded rather than re-derived. `dq_stray_o` has a
    // fired positive control in `geom_depthquant_stream_directed` case 2; a
    // second counter here watching the same event would be a second number to
    // reconcile.
    output var logic [31:0]          dq_refused_o,
    output var logic [31:0]          dq_stray_o
);

  // synthesis translate_off
  initial begin
    if (ATTRS < 7)
      $fatal(1, "zhao_part_clipfeed: GEOM.CLIP's ruling-5 packet is SEVEN slots; ATTRS=%0d", ATTRS);
    if ((SLOT_INVW >= ATTRS) || (SLOT_UOW >= ATTRS) || (SLOT_VOW >= ATTRS)
        || (SLOT_R >= ATTRS) || (SLOT_G >= ATTRS) || (SLOT_B >= ATTRS)
        || (SLOT_ALPHA >= ATTRS))
      $fatal(1, "zhao_part_clipfeed: an attribute slot index is outside ATTRS");
    if (SLOTS < 2)
      $fatal(1, "zhao_part_clipfeed: SLOTS must be at least 2");
    if (SLOTS < DQ_SLOTS)
      $fatal(1, "zhao_part_clipfeed: SLOTS (%0d) below DQ_SLOTS (%0d) makes the ring the rate limit",
             SLOTS, DQ_SLOTS);
    if ((SLOTS & (SLOTS - 1)) != 0)
      $fatal(1, "zhao_part_clipfeed: SLOTS (%0d) must be a power of two -- the pointers wrap on it", SLOTS);
  end
  // synthesis translate_on

  localparam int unsigned SW  = $clog2(SLOTS);
  localparam int unsigned PW  = SW + 1;
  localparam int unsigned TRIW = 6*21 + 24 + IDW;   // the fan, its colour, its id

  // A POLYGON PARTICLE HAS NO MATERIAL, BY LAW RATHER THAN BY CHOICE, so this
  // is a localparam and not a parameter: `draw_population`'s tris branch passes
  // no `TextureSpan`, and a knob here would be a knob for disagreeing with the
  // reference. It must equal `zhao_material_window`'s `MATMODE_NONE_C`, which
  // the directed test checks rather than this comment.
  localparam logic [1:0] PART_MATERIAL_MODE_C = 2'd1;

  // ==========================================================================
  // THE RING
  // ==========================================================================
  logic [TRIW-1:0]  tri_q   [SLOTS];
  logic [23:0]      inv_q   [SLOTS];
  logic [SLOTS-1:0] land_q;

  logic [PW-1:0] wp_q, rp_q;
  wire  [PW-1:0] occ_c   = wp_q - rp_q;
  wire           full_c  = (occ_c == PW'(SLOTS));
  wire           empty_c = (wp_q == rp_q);

  // ==========================================================================
  // THE RANGE PREMISE, CHECKED
  // ==========================================================================
  // A 22-bit signed value fits in 21 bits exactly when its top two bits agree.
  wire range_bad_c = (p_ax_i[21] != p_ax_i[20]) || (p_ay_i[21] != p_ay_i[20]) ||
                     (p_bx_i[21] != p_bx_i[20]) || (p_by_i[21] != p_by_i[20]) ||
                     (p_cx_i[21] != p_cx_i[20]) || (p_cy_i[21] != p_cy_i[20]);

  // ==========================================================================
  // THE CONVERTER, AND THE ACCEPT
  // ==========================================================================
  wire         dq_v_ready;
  wire         dq_d_valid;
  wire [23:0]  dq_invw;
  /* verilator lint_off UNUSEDSIGNAL */
  // TAGW is 16 because that is the converter's parameter; the slot this block
  // puts in is SW bits and the rest are zeros it sent itself. The converter
  // ECHOES the tag verbatim, so the high bits are known-zero by construction
  // rather than merely unread -- a counter watching them would be a detector
  // wired to a quantity that cannot move.
  wire [15:0]  dq_tag;
  /* verilator lint_on UNUSEDSIGNAL */

  wire rcp_v_valid, rcp_v_ready, rcp_r_valid, rcp_r_ready;
  wire [23:0] rcp_d, rcp_r;
  wire [ 5:0] rcp_k;
  wire [ 7:0] rcp_v_tok, rcp_r_tok;

  /* verilator lint_off UNUSEDSIGNAL */
  // The converter's and the reciprocal's own census, and the law's normal
  // clamps. Left unread where `zhao_geom_vattr` already publishes the same
  // quantity for the same law: a second copy of a counter is a second number to
  // reconcile, and the two instances answer different questions about the same
  // arithmetic rather than about the same traffic.
  wire [31:0] dq_vertices, dq_near, dq_far, dq_sat;
  wire        dq_idle;
  wire        rcp_d_zero, rcp_qerr, rcp_idle;
  wire [31:0] rcp_accepted, rcp_completed, rcp_mul_jobs, rcp_zero_jobs,
              rcp_phase_jobs, rcp_negcorr_jobs;
  wire [ 5:0] rcp_occupancy;
  /* verilator lint_on UNUSEDSIGNAL */

  // THE READY READS NO VALID OF ITS OWN. `range_bad_c` is a function of offered
  // DATA, `full_c` of registered pointers, and `dq_v_ready` of the converter's
  // own occupancy -- none of the three reads `p_valid_i`. So the handshake
  // cannot lock and `zhao_part_expand`'s registered valid never sees its own
  // ready.
  wire issue_c = !full_c && dq_v_ready;
  assign p_ready_o = range_bad_c || issue_c;

  wire take_c   = p_valid_i && p_ready_o;
  wire accept_c = take_c && !range_bad_c;   // holds a slot and issues a `w`
  wire refuse_c = take_c &&  range_bad_c;   // consumed, counted, no slot

  zhao_geom_depthquant_stream #(
      .TAGW (16),
      .NSLOT(DQ_SLOTS)
  ) u_dq (
      .clk           (clk),
      .rst_n         (rst_n),
      // The offer is the accept itself: a particle takes a ring slot and issues
      // its `w` on ONE clock, so the answer's home exists before the question
      // is asked and no second queue is needed between them.
      .v_valid_i     (p_valid_i && !range_bad_c && !full_c),
      .v_ready_o     (dq_v_ready),
      // 40 bits in, 31 bits of guarded `w`. WIDENED, not converted: the value
      // is placed in the low bits and the rest are zero, which is what
      // GEOM.VATTR and FORGE.ASSEMBLE do with the same quantity.
      .v_w_i         ({9'd0, p_w_i}),
      .v_profile_i   (p_profile_i),
      // THE TAG IS THE RING SLOT. The converter echoes it verbatim, so the high
      // bits are known-zero BY CONSTRUCTION rather than merely unread.
      .v_tag_i       ({{(16-SW){1'b0}}, wp_q[SW-1:0]}),
      .d_valid_o     (dq_d_valid),
      // ALWAYS READY, and not a shortcut: the landing is a write into `inv_q`
      // at the tag's address, which no other writer contends for and which
      // cannot stall. A ready that could fall would need a second queue for a
      // value that already has a home.
      .d_ready_i     (1'b1),
      .d_invw24_o    (dq_invw),
      .d_tag_o       (dq_tag),
      .rcp_valid_o   (rcp_v_valid),
      .rcp_ready_i   (rcp_v_ready),
      .rcp_d_o       (rcp_d),
      .rcp_tok_o     (rcp_v_tok),
      .rcp_rvalid_i  (rcp_r_valid),
      .rcp_rready_o  (rcp_r_ready),
      .rcp_r_i       (rcp_r),
      .rcp_k_i       (rcp_k),
      .rcp_tok_i     (rcp_r_tok),
      .vertices_o    (dq_vertices),
      .clamped_near_o(dq_near),
      .clamped_far_o (dq_far),
      .saturated_o   (dq_sat),
      .refused_o     (dq_refused_o),
      .tok_stray_o   (dq_stray_o),
      .idle_o        (dq_idle)
  );

  zhao_raster_rcp24_v4 #(
      .NCTX(RCP_NCTX),
      .TOKW(8)
  ) u_rcp (
      .clk           (clk),
      .rst_n         (rst_n),
      .v_valid_i     (rcp_v_valid),
      .v_ready_o     (rcp_v_ready),
      .d_i           (rcp_d),
      .v_tok_i       (rcp_v_tok),
      .r_valid_o     (rcp_r_valid),
      .r_ready_i     (rcp_r_ready),
      .r_o           (rcp_r),
      .k_o           (rcp_k),
      .d_zero_o      (rcp_d_zero),
      .r_tok_o       (rcp_r_tok),
      .accepted_o    (rcp_accepted),
      .completed_o   (rcp_completed),
      .mul_jobs_o    (rcp_mul_jobs),
      .zero_jobs_o   (rcp_zero_jobs),
      .phase_jobs_o  (rcp_phase_jobs),
      .negcorr_jobs_o(rcp_negcorr_jobs),
      .occupancy_o   (rcp_occupancy),
      .qerr_o        (rcp_qerr),
      .idle_o        (rcp_idle)
  );

  wire [SW-1:0] land_slot_c = dq_tag[SW-1:0];

  // ==========================================================================
  // THE HEAD, AND THE BEAT IT BECOMES
  // ==========================================================================
  wire [SW-1:0]    head_c   = rp_q[SW-1:0];
  wire [TRIW-1:0]  head_tri_c = tri_q[head_c];
  wire [23:0]      head_inv_c = inv_q[head_c];

  // THE ONE PLACE THE PACKED RECORD'S OFFSETS APPEAR BESIDE THE PACK ORDER --
  // written from the top down in the same order as the write below, which is
  // the discipline `zhao_part_project` uses for its own two records.
  //   [TRIW-1 -: 21] ax  ... six corners ... [39:16] rgb  [IDW-1:0] src_id
  wire signed [20:0] hd_ax_c = $signed(head_tri_c[TRIW-1   -: 21]);
  wire signed [20:0] hd_ay_c = $signed(head_tri_c[TRIW-22  -: 21]);
  wire signed [20:0] hd_bx_c = $signed(head_tri_c[TRIW-43  -: 21]);
  wire signed [20:0] hd_by_c = $signed(head_tri_c[TRIW-64  -: 21]);
  wire signed [20:0] hd_cx_c = $signed(head_tri_c[TRIW-85  -: 21]);
  wire signed [20:0] hd_cy_c = $signed(head_tri_c[TRIW-106 -: 21]);
  wire        [ 7:0] hd_r_c  = head_tri_c[IDW+23 -: 8];
  wire        [ 7:0] hd_g_c  = head_tri_c[IDW+15 -: 8];
  wire        [ 7:0] hd_b_c  = head_tri_c[IDW+7  -: 8];
  wire [IDW-1:0]     hd_id_c = head_tri_c[IDW-1:0];

  // THE EXACT LEFT INVERSE of `zhao_raster_tile_pipe_v2::lit_unit8`, which is
  // `v[15:8]` saturating: `lit_unit8({16'd0, c, 8'd0}) == c` for all 256 bytes.
  // Derived from the CONSUMER's own law, not chosen.
  function automatic logic [31:0] lit_of_byte(input logic [7:0] c);
    begin
      lit_of_byte = {16'd0, c, 8'd0};
    end
  endfunction

  function automatic logic [ATTRS*32-1:0] pack_attr(input logic [23:0] invw,
                                                    input logic [ 7:0] r,
                                                    input logic [ 7:0] g,
                                                    input logic [ 7:0] b);
    logic [ATTRS*32-1:0] w;
    begin
      w = '0;
      w[32*SLOT_INVW  +: 32] = {8'd0, invw};
      w[32*SLOT_UOW   +: 32] = 32'd0;   // R197: replaced by the zero operand
      w[32*SLOT_VOW   +: 32] = 32'd0;   // downstream; agreed with, not relied on
      w[32*SLOT_R     +: 32] = lit_of_byte(r);
      w[32*SLOT_G     +: 32] = lit_of_byte(g);
      w[32*SLOT_B     +: 32] = lit_of_byte(b);
      w[32*SLOT_ALPHA +: 32] = 32'(PART_ALPHA);
      pack_attr = w;
    end
  endfunction

  // THE HEAD IS OFFERED ONLY WHEN ITS DEPTH HAS LANDED. `o_valid_o` therefore
  // reads registered state alone and never `o_ready_i`.
  assign o_valid_o = !empty_c && land_q[head_c];

  assign o_ax_o = hd_ax_c;
  assign o_ay_o = hd_ay_c;
  assign o_bx_o = hd_bx_c;
  assign o_by_o = hd_by_c;
  assign o_cx_o = hd_cx_c;
  assign o_cy_o = hd_cy_c;
  // A FACT ABOUT THE PRODUCER: `zhao_part_expand` consumes a behind-the-eye
  // particle and emits nothing, so no beat here can carry a behind corner.
  assign o_behind_o    = 3'b000;
  assign o_src_id_o    = hd_id_c;
  assign o_untex_o     = 1'b1;
  assign o_cull_mode_o = PART_CULL_MODE;
  // ALL THREE CORNERS SHARE ONE DEPTH AND ONE COLOUR. That is what a polygon
  // particle is: `zhao_part_expand.t_d_o`'s comment says "all three vertices
  // share it", and GEOM.ATTRPACK turns three equal slot values into a constant
  // plane with no special case.
  assign o_attr_a_o = pack_attr(head_inv_c, hd_r_c, hd_g_c, hd_b_c);
  assign o_attr_b_o = pack_attr(head_inv_c, hd_r_c, hd_g_c, hd_b_c);
  assign o_attr_c_o = pack_attr(head_inv_c, hd_r_c, hd_g_c, hd_b_c);

  assign o_material_set_o  = 32'd0;
  assign o_material_id_o   = 16'd0;
  assign o_material_mode_o = PART_MATERIAL_MODE_C;
  assign o_quality_tier_o  = 8'd0;

  wire emit_c = o_valid_o && o_ready_i;

  // ==========================================================================
  // STATE
  // ==========================================================================
  wire [TRIW-1:0] tri_wr_c = {p_ax_i[20:0], p_ay_i[20:0], p_bx_i[20:0],
                              p_by_i[20:0], p_cx_i[20:0], p_cy_i[20:0],
                              p_r_i, p_g_i, p_b_i, p_src_id_i};

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      wp_q            <= '0;
      rp_q            <= '0;
      land_q          <= '0;
      particles_o     <= 32'd0;
      triangles_o     <= 32'd0;
      range_refused_o <= 32'd0;
      stall_full_o    <= 32'd0;
      // `tri_q` and `inv_q` are deliberately NOT reset. Every slot is written
      // before it is read -- a beat is emitted only after its landing set the
      // bit, and the bit IS reset -- so resetting the arrays would spend flops
      // to initialise values nothing can observe.
    end else begin
      if (accept_c) begin
        tri_q[wp_q[SW-1:0]] <= tri_wr_c;
        // CLEARED AT ALLOCATION, SET AT LANDING. The two writers cannot collide
        // on one slot: a slot is emitted only after it has landed, so by the
        // time `wp_q` returns to it, its landing is in the past.
        land_q[wp_q[SW-1:0]] <= 1'b0;
        wp_q <= wp_q + PW'(1);
        if (particles_o != 32'hffff_ffff) particles_o <= particles_o + 32'd1;
      end

      if (dq_d_valid) begin
        inv_q[land_slot_c]  <= dq_invw;
        land_q[land_slot_c] <= 1'b1;
      end

      if (emit_c) begin
        rp_q <= rp_q + PW'(1);
        if (triangles_o != 32'hffff_ffff) triangles_o <= triangles_o + 32'd1;
      end

      if (refuse_c && (range_refused_o != 32'hffff_ffff))
        range_refused_o <= range_refused_o + 32'd1;

      // Offered, in range, and the ring had no room. NOT a fault: it is this
      // block's own rate against its producer's.
      if (p_valid_i && !range_bad_c && full_c && (stall_full_o != 32'hffff_ffff))
        stall_full_o <= stall_full_o + 32'd1;
    end
  end

  // An allocation and a landing CAN name the same slot only if the converter
  // answered a question that was never asked. The converter's own `tok_stray_o`
  // is the instrument for that class and it has a fired positive control; this
  // assertion is the same statement said where a bench sees it at once.
  // `synthesis translate_off` keeps it out of the fabric and does NOT keep it
  // out of Verilator, which is what is wanted.
  // synthesis translate_off
  /* verilator lint_off SYNCASYNCNET */
  always_ff @(posedge clk) begin
    if (rst_n) begin
      a_clipfeed_slot_clash : assert (!(accept_c && dq_d_valid &&
                                        (land_slot_c == wp_q[SW-1:0])))
        else $fatal(1, "zhao_part_clipfeed: a landing named the slot being allocated");
    end
  end
  /* verilator lint_on SYNCASYNCNET */
  // synthesis translate_on

endmodule

`default_nettype wire
