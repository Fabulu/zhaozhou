// zhao_geom_attrpack.sv -- GEOM.ATTRPACK: the three-plane front end that was
// missing in front of GEOM.ATTRSETUP.
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
// What did not exist is the block that ASKS three times and packs the answers.
// This is that block, and it contains no plane arithmetic of its own. Every
// coefficient that leaves here was computed by the one `zhao_geom_attrsetup`
// instance below. Writing the cross products again here would be the exact
// failure CLAUDE.md names -- the projector's two cores, `TERRAIN.SHADE` against
// `GEOM.LIGHT` -- and it is refused.
//
// ---------------------------------------------------------------------------
// ONE CORE, THREE LANES: WHY TIME-MULTIPLEXED AND NOT THREE INSTANCES
// ---------------------------------------------------------------------------
// The obvious shape is three `zhao_geom_attrsetup` instances, one per plane,
// running in parallel. It is refused on the budget: ALMs bind at 47,582 against
// 41,910, and `zhao_geom_attrsetup` is multiplier-heavy -- three 46x32 products
// for `n0` and six 22x32 products for the two gradients, per instance. Three
// copies triple all of it to buy throughput nothing asks for.
//
// The rate this needs is per TRIANGLE, not per pixel. One core, three passes,
// costs SEVEN gpu clocks per triangle (two per lane plus the accept), against
// the owner-ruled 120,000 vertices/frame -- about 40,000 triangles at 60 Hz, or
// roughly 41 clocks of budget each at 100 MHz. Seven fits inside forty-one with
// room, so the parallel version would be buying a margin that is already there
// with silicon that is not.
//
// The multiplexing is in the OPERAND MUX, not in the arithmetic: `as_va_c` and
// its two siblings select which of the nine latched attribute words the shared
// core sees. That is three 32-bit 3:1 muxes against two whole extra copies of a
// 96-bit multiply-add tree.
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
// words at its own line 41. Packet-D wants the first three. They are PARAMETERS
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
// meta[508:437], dndx at [580:509], n0 at [676:581]) and it is the same word
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
// selects three of seven and hands them to the core one at a time.
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
    // FOUR OF THE SEVEN SLOTS ARE DELIBERATELY UNREAD, and the waiver says
    // which rather than silencing the warning. Packet-D's carriage is three
    // planes; the ruling-5 packet also carries lit r/g/b and alpha, whose
    // planes belong to the Gouraud lanes this console does not have composed.
    // GEOM.ATTRSETUP's own header says a textured Gouraud triangle asks SEVEN
    // times -- this block asks the three that `zhao_raster_tile_pipe_v2`
    // unpacks, and reading the other four would mean packing planes with no
    // port to carry them.
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

    // ---- the three Packet-D planes ----------------------------------------
    output logic                out_valid_o,
    input  logic                out_ready_i,
    output logic       [239:0]  out_invw_plane_o,
    output logic       [239:0]  out_u_over_w_plane_o,
    output logic       [239:0]  out_v_over_w_plane_o,
    output logic    [IDW-1:0]   out_src_id_o,

    // ---- observability -----------------------------------------------------
    // Both fire on every legal triangle, and their RATIO is the invariant:
    // `planes_o` must be exactly three times `triangles_o` once the block is
    // idle. A lane that silently stopped asking would break the ratio while
    // every handshake still looked healthy.
    output logic        [31:0]  triangles_o,
    output logic        [31:0]  planes_o
);

  // Quartus 17.0 rejects a bare module-scope `if`; the check lives in an
  // `initial begin` (QUARTUS_GOTCHAS). And `--lint-only` does not RUN initial
  // blocks, so a clean lint says nothing about this -- it is fired by
  // parameterising the block wrongly in the directed suite.
  // synthesis translate_off
  initial begin
    if ((SLOT_INVW >= ATTRS) || (SLOT_U_OVER_W >= ATTRS) ||
        (SLOT_V_OVER_W >= ATTRS))
      $fatal(1, "zhao_geom_attrpack: an attribute slot is outside the packet");
    if ((SLOT_INVW == SLOT_U_OVER_W) || (SLOT_INVW == SLOT_V_OVER_W) ||
        (SLOT_U_OVER_W == SLOT_V_OVER_W))
      $fatal(1, "zhao_geom_attrpack: two planes were pointed at one slot");
  end
  // synthesis translate_on

  localparam logic [1:0] S_IDLE = 2'd0;
  localparam logic [1:0] S_RUN  = 2'd1;
  localparam logic [1:0] S_HOLD = 2'd2;

  logic [1:0] state_q;
  logic [1:0] lane_q;
  logic       lane_sent_q;

  // The latched triangle. Nine attribute words and six coordinates, not the
  // whole 672-bit packet: only three of seven slots leave this block, and
  // holding the other four would be 384 flops carrying nothing.
  logic signed [20:0] ax_q, ay_q, bx_q, by_q, cx_q, cy_q;
  logic signed [31:0] va_q [0:2];
  logic signed [31:0] vb_q [0:2];
  logic signed [31:0] vc_q [0:2];
  logic    [IDW-1:0]  src_id_q;

  logic signed [95:0] n0_q   [0:2];
  logic signed [71:0] dndx_q [0:2];
  logic signed [71:0] dndy_q [0:2];

  // ---- the shared core ------------------------------------------------------
  logic               as_v_valid_c, as_v_ready_w;
  logic               as_r_valid_w, as_r_ready_c;
  logic signed [31:0] as_va_c, as_vb_c, as_vc_c;
  logic signed [95:0] as_n0_w;
  logic signed [71:0] as_dndx_w, as_dndy_w;

  always_comb begin
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
  assign out_src_id_o         = src_id_q;

  logic accept_w, ask_w, reply_w;
  assign accept_w = tri_valid_i && tri_ready_o;
  assign ask_w    = as_v_valid_c && as_v_ready_w;
  assign reply_w  = as_r_valid_w && as_r_ready_c;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state_q     <= S_IDLE;
      lane_q      <= 2'd0;
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
      for (int k = 0; k < 3; k++) begin
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

        lane_q      <= 2'd0;
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
        if (lane_q == 2'd2) begin
          state_q <= S_HOLD;
          lane_q  <= 2'd0;
        end else begin
          lane_q <= lane_q + 2'd1;
        end
      end

      if ((state_q == S_HOLD) && out_ready_i) state_q <= S_IDLE;
    end
  end

endmodule : zhao_geom_attrpack

`default_nettype wire
