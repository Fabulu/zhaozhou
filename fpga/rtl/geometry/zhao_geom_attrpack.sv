// zhao_geom_attrpack.sv -- GEOM.ATTRPACK: the six-plane front end that was
// missing in front of GEOM.ATTRSETUP.
//
// It shipped as THREE planes and grew to SIX on 2026-09-21 under owner
// decision R234 D1 (`(owner, explicit)`), which reconnected the lit per-vertex
// colour. See the `tri_attr_a_i` waiver below for the whole chain and the
// measured price.
//
// ENFORCED-BY: tests/geometry/geom_attrpack_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHY THIS FILE EXISTS, AND WHY IT IS NOT A SECOND IMPLEMENTATION
// ---------------------------------------------------------------------------
// `zhao_console_core.sv` entry I20 says it exactly: the Packet-D attribute
// carriage (`tri_invw_plane_i`, `tri_u_over_w_plane_i`, `tri_v_over_w_plane_i`)
// had NO producer anywhere in `fpga/rtl`, and every `[239:0]` in the tree was an
// input. The arithmetic was never missing -- `zhao_geom_attrsetup` is real,
// directed-tested and emits `n0_o`/`dndx_o`/`dndy_o`, 96+72+72 = exactly the 240
// bits of ONE plane. Its own header says
//
//     "ONE ATTRIBUTE PER REQUEST. A textured Gouraud triangle needs seven
//      planes and asks seven times."
//
// What did not exist is the block that ASKS six times and packs the answers.
// This is that block, and it contains no plane arithmetic of its own. Every
// coefficient that leaves here was computed by the one `zhao_geom_attrsetup`
// instance below. Writing the cross products again here would be the exact
// failure CLAUDE.md names -- the projector's two cores, `TERRAIN.SHADE` against
// `GEOM.LIGHT` -- and it is refused.
//
// ---------------------------------------------------------------------------
// ONE CORE, SIX LANES: WHY TIME-MULTIPLEXED AND NOT SIX INSTANCES
// ---------------------------------------------------------------------------
// The obvious shape is one `zhao_geom_attrsetup` instance per plane, running in
// parallel. It is refused on the budget: ALMs bind at 47,582 against 41,910,
// and `zhao_geom_attrsetup` is multiplier-heavy -- three 46x32 products for
// `n0` and six 22x32 products for the two gradients, per instance. Six copies
// sextuple all of it to buy throughput nothing asks for.
//
// The rate this needs is per TRIANGLE, not per pixel. One core, six passes,
// costs THIRTEEN gpu clocks per triangle (two per lane plus the accept),
// against the owner-ruled 120,000 vertices/frame -- about 40,000 triangles at
// 60 Hz, or roughly 41 clocks of budget each at 100 MHz. Thirteen fits inside
// forty-one with room, so the parallel version would be buying a margin that is
// already there with silicon that is not.
//
// THIS IS WHY THE GOURAUD LANES ARE CHEAP AT THIS END AND EXPENSIVE AT THE
// OTHER. Adding three planes here adds NO arithmetic whatsoever -- the operand
// mux grows from 3:1 to 6:1 and the schedule grows by six clocks. The ~1,420
// ALM and +24 DSP the decision cost are all in `zhao_raster_tile_pipe_v2`,
// which must interpolate per PIXEL and therefore cannot time-multiplex.
//
// The multiplexing is in the OPERAND MUX, not in the arithmetic: `as_va_c` and
// its two siblings select which of the eighteen latched attribute words the
// shared core sees. That is three 32-bit 6:1 muxes against five whole extra
// copies of a 96-bit multiply-add tree.
//
// ---------------------------------------------------------------------------
// THE ATTRIBUTE SLOTS, AND WHY THEY ARE PARAMETERS
// ---------------------------------------------------------------------------
// `zhao_geom_clip`'s ruling-5 vertex packet is seven 32-bit fields, flattened
// little-end-first: slot k occupies bits [32*k+31 : 32*k]. The order is
//
//     0 invw24   1 u_over_w   2 v_over_w   3 r   4 g   5 b   6 alpha
//
// and `tests/geometry/geom_clip_attrswap_directed.cpp` states the same three
// words at its own line 41. Packet-D wants the first SIX. They are PARAMETERS
// and not literals because CLAUDE.md's rule 6 is explicit -- every shape value
// belongs in a named, editable constant, and "the layout is ratified so it is
// not a knob" is how a wrong number becomes an unadjustable wrong number. A
// caller that reorders the packet re-points these and nothing else moves.
//
// ---------------------------------------------------------------------------
// THE PLANE WORD
// ---------------------------------------------------------------------------
// `{n0[95:0], dndx[71:0], dndy[71:0]}`, dndy in the low bits. That is read off
// `zhao_raster_tile_pipe_v2`'s own unpack (lines 271-283: plane 0 is dndy at
// meta[508:437], dndx at [580:509], n0 at [676:581]; lane k is that triple
// offset by 240*k) and it is the same word
// `tools/quartus/gen_shell_fit_top.py` builds for the fit. This block is now
// the PRODUCER of that word; the fit generator stays what it always was, which
// is stimulus.
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK DOES NOT DO
// ---------------------------------------------------------------------------
// It does not divide -- the quotient is per pixel and belongs to
// RASTER.ATTRGRAD. It does not build the FLAT material request
// (`zhao_texture_v3_request_v2_t[297:0]`): that is draw state -- palette slot,
// base binding, material recipe, the aux surface context -- and it has no
// producer in this tree either. Inventing one here would be the tie-off
// dressed as wiring that I20 refuses. It does not interpret the attributes; it
// selects six of seven and hands them to the core one at a time. In particular
// it does NOT convert the Gouraud channels out of their Q0.16 light scale --
// that conversion is per fragment and lives at `vertex_rgb`'s field build in
// `zhao_raster_tile_pipe_v2`, where RASTER.TOON will one day sit in front of
// it.
//
// Conservative SystemVerilog subset only; Quartus 17.0 syntax rules apply
// (elaboration checks inside `initial begin`, explicit generate).
`default_nettype none

module zhao_geom_attrpack #(
    // The ruling-5 attribute packet width, in 32-bit fields.
    parameter int unsigned ATTRS = 7,
    // Which slot carries which Packet-D plane. See the header.
    parameter int unsigned SLOT_INVW     = 0,
    parameter int unsigned SLOT_U_OVER_W = 1,
    parameter int unsigned SLOT_V_OVER_W = 2,
    // THE GOURAUD SLOTS (owner decision R234 D1, 2026-09-21). The same three
    // words GEOM.VATTR wrote as `c_rgb_q` and GEOM.CLIP winding-flipped with
    // the corners. They are named parameters for exactly the reason the other
    // three are -- a caller that reorders the ruling-5 packet re-points these
    // and nothing else moves.
    parameter int unsigned SLOT_R        = 3,
    parameter int unsigned SLOT_G        = 4,
    parameter int unsigned SLOT_B        = 5,
    // The identity carried beside the triangle so a downstream join can prove
    // the planes and the edge coefficients describe the SAME triangle.
    parameter int unsigned IDW = 16
) (
    input  logic clk,
    input  logic rst_n,

    // ---- one clipped, winding-normalised triangle -------------------------
    // The same S 12.8 screen vertices GEOM.SETUP is handed, from the same
    // GEOM.CLIP packet. B and C have already been swapped if the winding
    // needed it, and the attributes below swapped with them -- that swap is
    // the whole reason the attributes travel through GEOM.CLIP at all.
    input  logic                tri_valid_i,
    output logic                tri_ready_o,
    input  logic signed  [20:0] tri_ax_i,
    input  logic signed  [20:0] tri_ay_i,
    input  logic signed  [20:0] tri_bx_i,
    input  logic signed  [20:0] tri_by_i,
    input  logic signed  [20:0] tri_cx_i,
    input  logic signed  [20:0] tri_cy_i,
    // ONE OF THE SEVEN SLOTS IS DELIBERATELY UNREAD, and the waiver says which
    // rather than silencing the warning. Packet-D's carriage is now SIX planes
    // -- invw24, u/w, v/w and the three Gouraud channels -- so the only slot
    // this block does not ask for is `alpha` (slot 6), whose value is governed
    // by ruling R48's named `ALPHA_C` constant and whose PER-PRIMITIVE producer
    // is `tri_continuation_tail_i`'s `vertex_alpha` (ruling R89). Interpolated
    // per-vertex alpha remains a real, uncommissioned feature and R48 remains
    // its named seam; it is NOT a fourth Gouraud lane and R89 says so.
    //
    // SLOTS 3, 4 AND 5 ARRIVE HERE FULL, AND AS OF R234 D1 THEY LEAVE AGAIN.
    // The chain is composed and live, every hop verified by hand in this tree:
    //
    //   zhao_light_stream  (GEOM.LIGHT, owner ruling R2)
    //     -> geom_light_{valid,r,g,b}_o, 17 bits per channel, Q0.16 with
    //        0x1_0000 == 1.0 (`NDL_ONE`, asserted in range at that block)
    //     -> zhao_geom_vattr .lit_*_i, latched into the colour store
    //        (`c_rgb_q`, counted by `colours_written_o`)
    //     -> rep_data_o[95:64] r, [127:96] g, [159:128] b
    //     -> GEOM.REPLAY's attribute store -> rp_st_{a,b,c}
    //     -> rp_attr_* = {rp_st_*, 8'd0, rp_invw_*}      == packet slots 3,4,5
    //     -> zhao_geom_clip, winding-flipped WITH the corners
    //     -> geom_clip_attr_{a,b,c}_o -> tri_attr_{a,b,c}_i, immediately below
    //     -> lanes 3,4,5 of the shared attrsetup core
    //     -> out_{r,g,b}_plane_o -> the shell -> `zhao_geom_bin_pipe_v2`'s
    //        1,877-bit metadata -> `zhao_raster_tile_pipe_v2` lanes 3..5
    //     -> `continuation_w.post_earlyz.vertex_rgb`
    //     -> `zhao_raster_fragment.frag_vert_rgb_i`.
    //
    // WHAT CHANGED AND WHY IT WAS NOT DONE SOONER. Until 2026-09-21 this block
    // packed THREE planes and the lit colour stopped at this port: severed
    // DELIVERY, not dead computation, which is why removing the lighting was
    // refused rather than reclaimed. The owner was shown rendered boards and
    // chose to pay for the lanes (R234 D1, `(owner, explicit)`): the free
    // stand-in is a provoking-vertex pick that crawls a herringbone sawtooth
    // across the body, and 100.00% of 3,168,243 drawn triangles take the
    // Gouraud path, so the stand-in differs on nearly every triangle of every
    // frame.
    //
    // R89 DOES NOT TRANSFER. It refused a fourth attrpack plane for a value
    // that "does not vary across the primitive" -- true of shadow alpha, false
    // of lit vertex colour, because varying across the primitive is what
    // Gouraud IS. R230 records that distinction explicitly.
    //
    // WHAT IT COST, measured rather than guessed. From section 17 of
    // `reports/synthesis/blockpaths/zhao_raster_texture_v3_fit_top@g8a.fit.rpt`
    // (truth device 5CSEBA6U23I7, `rtlCleanAtHead: true`), one raster attribute
    // lane is 488.1 / 463.6 / 467.8 ALM and **8 DSP** each -- of which
    // `zhao_raster_attrdiv_v2` alone is ~295 ALM, because the plane is
    // unnormalised and EVERY lane divides by 2A. Three more lanes is therefore
    // **~1,420 ALM and +24 DSP**. (Do NOT quote the 3-DSP figure: that is the
    // `ATTR_DSP3 = 1` variant, which is G8A characterization only. The composed
    // console runs `ATTR_DSP3 = 0`.) It also widens `METAW` from 1157 to 1877,
    // taking `zhao_geom_binner_v2`'s metadata bank from 29 forty-bit slices to
    // 47 -- M10K AND per-triangle bank time. THE GEOMETRY SIDE IS THE CHEAP
    // END: the shared attrsetup core is time-multiplexed, so six lanes add no
    // arithmetic here at all, only +2 clocks per lane against ~41 of budget.
    // Full working in
    // `runs/CLAUDE-RUNS/RUN-20260919-1656-gaps-to-zero/FINDINGS-attrlane.md`.
    //
    // `ATTRS` STAYS 7. Entry I13's refusal to narrow `GEOM_CLIP_ATTRS` to 3 is
    // load-bearing and is honoured here; it is now additionally true that six
    // of the seven slots have a reader.
    /* verilator lint_off UNUSEDSIGNAL */
    input  logic [ATTRS*32-1:0] tri_attr_a_i,
    input  logic [ATTRS*32-1:0] tri_attr_b_i,
    input  logic [ATTRS*32-1:0] tri_attr_c_i,
    /* verilator lint_on UNUSEDSIGNAL */
    input  logic    [IDW-1:0]   tri_src_id_i,
    // THE UNTEXTURED DECLARATION (owner ruling R197). THIS BLOCK IS THE ONLY
    // READER OF SLOTS U_OVER_W AND V_OVER_W IN THE TREE, so this is where the
    // law's "consumers BRANCH on the flag; when set, nothing reads the slot"
    // is discharged. With the bit set the two slots are NOT LATCHED: lanes 1
    // and 2 are fed the ZERO operand instead, so whatever the producer left in
    // the slots never enters the plane arithmetic, and the packed u/w and v/w
    // planes are the null plane {n0 = 0, dndx = 0, dndy = 0} -- the plane of
    // the constant 0, which the raster's gradient dividers cannot refuse
    // (0 / 2A never saturates or errors), so a declared-absent attribute can
    // never raise the tile pipe's frame-terminating range fault the way
    // don't-care content honestly could. The lane SCHEDULE is unchanged: the
    // core still runs three lanes, so `planes_o == 3 * triangles_o` stays the
    // invariant the composer asserts. (Four clocks per untextured triangle
    // could be reclaimed by skipping the two lanes; not taken, because it
    // would change that ratio and buy throughput nothing asks for.)
    //
    // The bit is a DECLARATION and the zero is its CONSEQUENCE here, not the
    // other way round: no consumer may infer "untextured" from a zero plane.
    // The door at GEOM.CLIP's input has already refused any declared-untextured
    // primitive whose material takes a sample, so downstream of this block
    // `untex` implies `sample_count == 0` and the null plane is read by nothing
    // that samples. `geom_attrpack_directed.cpp` case 5 pins both halves:
    // the same corners with the bit set yield the null u/w and v/w planes and
    // an UNCHANGED invw24 plane.
    input  logic                tri_untex_i,

    // ---- the six Packet-D planes ------------------------------------------
    output logic                out_valid_o,
    input  logic                out_ready_i,
    output logic       [239:0]  out_invw_plane_o,
    output logic       [239:0]  out_u_over_w_plane_o,
    output logic       [239:0]  out_v_over_w_plane_o,
    // THE GOURAUD PLANES (R234 D1). Same word shape, same core, same clock
    // budget; they exist because the lit colour must vary ACROSS the primitive
    // and a per-triangle constant cannot. They are NOT converted to unit8 here
    // -- the plane is evaluated per pixel downstream and the conversion belongs
    // with the pixel.
    output logic       [239:0]  out_r_plane_o,
    output logic       [239:0]  out_g_plane_o,
    output logic       [239:0]  out_b_plane_o,
    output logic    [IDW-1:0]   out_src_id_o,

    // ---- observability -----------------------------------------------------
    // Both fire on every legal triangle, and their RATIO is the invariant:
    // `planes_o` must be exactly `LANES` times `triangles_o` once the block is
    // idle -- SIX since R234 D1, three before it. A lane that silently stopped
    // asking would break the ratio while every handshake still looked healthy,
    // and `tests/prod/tb_zhao_console_core_smoke.sv` checks that exact multiple
    // on the composed console.
    output logic        [31:0]  triangles_o,
    output logic        [31:0]  planes_o
);

  localparam logic [1:0] S_IDLE = 2'd0;
  localparam logic [1:0] S_RUN  = 2'd1;
  localparam logic [1:0] S_HOLD = 2'd2;

  // THE LANE COUNT IS A NAMED CONSTANT, not a literal sprinkled through the
  // body. It went 3 -> 6 on 2026-09-21 and every array, loop bound, mux and
  // terminal comparison below reads it, so the next change is one line.
  localparam int unsigned LANES  = 6;
  localparam int unsigned LANE_W = 3;   // wide enough to hold LANES-1
  localparam int unsigned SLOTW  = 8;   // slot index field, see SLOT_OF

  // Lane k reads packet slot `SLOT_OF[SLOTW*k +: SLOTW]`. It is a PACKED
  // vector, not an unpacked array, so the elaboration check below can walk it
  // with an ordinary part-select -- an unpacked localparam array is exactly the
  // kind of construct that lints clean and then costs a 33-second Quartus 17.0
  // failure, and a packed one cannot.
  localparam logic [LANES*SLOTW-1:0] SLOT_OF = {
      SLOTW'(SLOT_B), SLOTW'(SLOT_G), SLOTW'(SLOT_R),
      SLOTW'(SLOT_V_OVER_W), SLOTW'(SLOT_U_OVER_W), SLOTW'(SLOT_INVW)};

  // Quartus 17.0 rejects a bare module-scope `if`; the check lives in an
  // `initial begin` (QUARTUS_GOTCHAS). And `--lint-only` does not RUN initial
  // blocks, so a clean lint says nothing about this -- it is fired by
  // parameterising the block wrongly in the directed suite.
  // synthesis translate_off
  initial begin : p_attrpack_slots
    integer a;
    integer b;
    if (ATTRS > (1 << SLOTW))
      $fatal(1, "zhao_geom_attrpack: ATTRS outgrew SLOT_OF's index field");
    for (a = 0; a < LANES; a = a + 1) begin
      if (32'(SLOT_OF[SLOTW*a +: SLOTW]) >= ATTRS)
        $fatal(1, "zhao_geom_attrpack: an attribute slot is outside the packet");
      for (b = 0; b < LANES; b = b + 1)
        if ((a != b) &&
            (SLOT_OF[SLOTW*a +: SLOTW] == SLOT_OF[SLOTW*b +: SLOTW]))
          $fatal(1, "zhao_geom_attrpack: two planes were pointed at one slot");
    end
  end
  // synthesis translate_on

  logic [1:0] state_q;
  logic [LANE_W-1:0] lane_q;
  logic       lane_sent_q;

  // The latched triangle. Eighteen attribute words and six coordinates, not the
  // whole 672-bit packet: only six of seven slots leave this block, and holding
  // the seventh would be 96 flops carrying nothing.
  logic signed [20:0] ax_q, ay_q, bx_q, by_q, cx_q, cy_q;
  logic signed [31:0] va_q [0:LANES-1];
  logic signed [31:0] vb_q [0:LANES-1];
  logic signed [31:0] vc_q [0:LANES-1];
  logic    [IDW-1:0]  src_id_q;

  logic signed [95:0] n0_q   [0:LANES-1];
  logic signed [71:0] dndx_q [0:LANES-1];
  logic signed [71:0] dndy_q [0:LANES-1];

  // ---- the shared core ------------------------------------------------------
  logic               as_v_valid_c, as_v_ready_w;
  logic               as_r_valid_w, as_r_ready_c;
  logic signed [31:0] as_va_c, as_vb_c, as_vc_c;
  logic signed [95:0] as_n0_w;
  logic signed [71:0] as_dndx_w, as_dndy_w;

  // The 6:1 operand mux. This IS the whole cost of the three Gouraud lanes on
  // the geometry side: three more inputs on three muxes, and no arithmetic.
  always_comb begin : p_attrpack_operand_mux
    as_va_c = va_q[lane_q];
    as_vb_c = vb_q[lane_q];
    as_vc_c = vc_q[lane_q];
  end

  assign as_v_valid_c = (state_q == S_RUN) && !lane_sent_q;
  assign as_r_ready_c = (state_q == S_RUN) && lane_sent_q;

  zhao_geom_attrsetup u_attrsetup (
      .clk      (clk),
      .rst_n    (rst_n),
      .v_valid_i(as_v_valid_c),
      .v_ready_o(as_v_ready_w),
      .ax_i     (ax_q),
      .ay_i     (ay_q),
      .bx_i     (bx_q),
      .by_i     (by_q),
      .cx_i     (cx_q),
      .cy_i     (cy_q),
      .va_i     (as_va_c),
      .vb_i     (as_vb_c),
      .vc_i     (as_vc_c),
      .r_valid_o(as_r_valid_w),
      .r_ready_i(as_r_ready_c),
      .n0_o     (as_n0_w),
      .dndx_o   (as_dndx_w),
      .dndy_o   (as_dndy_w)
  );

  // `tri_ready_o` is a function of registers only, never of `out_ready_i`.
  // The core is forked off GEOM.CLIP's single output, and a ready that watched
  // the far side of the fork would close the loop through GEOM.SETUP.
  assign tri_ready_o = (state_q == S_IDLE);
  assign out_valid_o = (state_q == S_HOLD);

  assign out_invw_plane_o     = {n0_q[0], dndx_q[0], dndy_q[0]};
  assign out_u_over_w_plane_o = {n0_q[1], dndx_q[1], dndy_q[1]};
  assign out_v_over_w_plane_o = {n0_q[2], dndx_q[2], dndy_q[2]};
  assign out_r_plane_o        = {n0_q[3], dndx_q[3], dndy_q[3]};
  assign out_g_plane_o        = {n0_q[4], dndx_q[4], dndy_q[4]};
  assign out_b_plane_o        = {n0_q[5], dndx_q[5], dndy_q[5]};
  assign out_src_id_o         = src_id_q;

  logic accept_w, ask_w, reply_w;
  assign accept_w = tri_valid_i && tri_ready_o;
  assign ask_w    = as_v_valid_c && as_v_ready_w;
  assign reply_w  = as_r_valid_w && as_r_ready_c;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state_q     <= S_IDLE;
      lane_q      <= '0;
      lane_sent_q <= 1'b0;
      ax_q        <= '0;
      ay_q        <= '0;
      bx_q        <= '0;
      by_q        <= '0;
      cx_q        <= '0;
      cy_q        <= '0;
      src_id_q    <= '0;
      triangles_o <= 32'd0;
      planes_o    <= 32'd0;
      for (int k = 0; k < LANES; k++) begin
        va_q[k]   <= 32'sd0;
        vb_q[k]   <= 32'sd0;
        vc_q[k]   <= 32'sd0;
        n0_q[k]   <= 96'sd0;
        dndx_q[k] <= 72'sd0;
        dndy_q[k] <= 72'sd0;
      end
    end else begin
      if (accept_w) begin
        ax_q     <= tri_ax_i;
        ay_q     <= tri_ay_i;
        bx_q     <= tri_bx_i;
        by_q     <= tri_by_i;
        cx_q     <= tri_cx_i;
        cy_q     <= tri_cy_i;
        src_id_q <= tri_src_id_i;

        va_q[0]  <= $signed(tri_attr_a_i[32*SLOT_INVW     +: 32]);
        vb_q[0]  <= $signed(tri_attr_b_i[32*SLOT_INVW     +: 32]);
        vc_q[0]  <= $signed(tri_attr_c_i[32*SLOT_INVW     +: 32]);
        // R197's branch: a declared-untextured primitive's u/w and v/w slots
        // are never read. See the port's comment.
        if (tri_untex_i) begin
          va_q[1] <= 32'sd0;
          vb_q[1] <= 32'sd0;
          vc_q[1] <= 32'sd0;
          va_q[2] <= 32'sd0;
          vb_q[2] <= 32'sd0;
          vc_q[2] <= 32'sd0;
        end else begin
          va_q[1] <= $signed(tri_attr_a_i[32*SLOT_U_OVER_W +: 32]);
          vb_q[1] <= $signed(tri_attr_b_i[32*SLOT_U_OVER_W +: 32]);
          vc_q[1] <= $signed(tri_attr_c_i[32*SLOT_U_OVER_W +: 32]);
          va_q[2] <= $signed(tri_attr_a_i[32*SLOT_V_OVER_W +: 32]);
          vb_q[2] <= $signed(tri_attr_b_i[32*SLOT_V_OVER_W +: 32]);
          vc_q[2] <= $signed(tri_attr_c_i[32*SLOT_V_OVER_W +: 32]);
        end

        // THE GOURAUD LANES ARE NOT BRANCHED ON `tri_untex_i`, DELIBERATELY.
        // R197's declaration is about TEXTURE COORDINATES: an untextured
        // primitive's u/w and v/w slots hold whatever the producer left there,
        // so they are replaced by the zero operand. Lit colour is orthogonal --
        // an untextured primitive is still lit, and in the reference oracle it
        // is the UNTEXTURED case that carries pre-lit colour on the Gouraud
        // lanes (`reference/src/zrender/internal.hpp`, the `ScreenV::cr`
        // comment). Zeroing them here would make every untextured surface
        // black, which is removing function, not declaring absence.
        va_q[3] <= $signed(tri_attr_a_i[32*SLOT_R +: 32]);
        vb_q[3] <= $signed(tri_attr_b_i[32*SLOT_R +: 32]);
        vc_q[3] <= $signed(tri_attr_c_i[32*SLOT_R +: 32]);
        va_q[4] <= $signed(tri_attr_a_i[32*SLOT_G +: 32]);
        vb_q[4] <= $signed(tri_attr_b_i[32*SLOT_G +: 32]);
        vc_q[4] <= $signed(tri_attr_c_i[32*SLOT_G +: 32]);
        va_q[5] <= $signed(tri_attr_a_i[32*SLOT_B +: 32]);
        vb_q[5] <= $signed(tri_attr_b_i[32*SLOT_B +: 32]);
        vc_q[5] <= $signed(tri_attr_c_i[32*SLOT_B +: 32]);

        lane_q      <= '0;
        lane_sent_q <= 1'b0;
        state_q     <= S_RUN;
        if (triangles_o != 32'hffff_ffff) triangles_o <= triangles_o + 32'd1;
      end

      if (ask_w) lane_sent_q <= 1'b1;

      if (reply_w) begin
        n0_q[lane_q]   <= as_n0_w;
        dndx_q[lane_q] <= as_dndx_w;
        dndy_q[lane_q] <= as_dndy_w;
        lane_sent_q    <= 1'b0;
        if (planes_o != 32'hffff_ffff) planes_o <= planes_o + 32'd1;
        if (lane_q == LANE_W'(LANES - 1)) begin
          state_q <= S_HOLD;
          lane_q  <= '0;
        end else begin
          lane_q <= lane_q + LANE_W'(1);
        end
      end

      if ((state_q == S_HOLD) && out_ready_i) state_q <= S_IDLE;
    end
  end

endmodule : zhao_geom_attrpack

`default_nettype wire
