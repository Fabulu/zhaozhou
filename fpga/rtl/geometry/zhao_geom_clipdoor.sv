// zhao_geom_clipdoor.sv -- N PRODUCERS ON ONE `zhao_geom_clip` INPUT.
//
// ENFORCED-BY: tests/geometry/geom_clipdoor_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHY THIS BLOCK EXISTS, AND WHY IT IS AT *THIS* DOOR
// ---------------------------------------------------------------------------
// Owner ruling R187 named the place: *"the honest door is at GEOM.CLIP's
// INPUT, not GEOM.SETUP's -- it yields winding normalisation, 2A, bbox,
// zero-area reject and three-tine lockstep for free."*  `zhao_console_core`'s
// own R197 block says the same thing from the other side: *"it is also the
// door R187 names for every non-mesh producer ... so the arbiter that
// eventually admits particles and shadow hulls will present its `untex` bit to
// THIS gate, not to a second copy of it downstream."*
//
// This is that arbiter.  It is deliberately NOT a change to `zhao_geom_clip`:
// that block is composed, verified and on the critical path of every triangle
// the console draws, and `CLAUDE.md` records that "a port on a leaf costs its
// WHOLE instantiation chain plus every bench".  A door in front costs
// GEOM.CLIP nothing and no existing bench anything.
//
// ---------------------------------------------------------------------------
// THE DOOR CARRIES *TWO* HALVES OF ONE BEAT, AND THAT IS THE WHOLE POINT
// ---------------------------------------------------------------------------
// In the composed console the stream into GEOM.CLIP is not one wire bundle.
// It is:
//
//     producer --> zhao_material_window (the MATERIAL half) --> R197's
//                  untextured gate --> zhao_geom_clip (the TRIANGLE half)
//
// and the window is a PURE COMBINATIONAL GATE on that path -- its own body is
// `assign t_valid_o = t_valid_i && pass_c;` and
// `assign t_ready_o = refuse_c || (t_ready_i && pass_c);`.
// (CORRECTED 2026-09-23, gz/trimerge: this header quoted the ready line as
// `t_ready_i && pass_c`, which is one revision behind the block it quotes --
// the `refuse_c ||` term is what CONSUMES a refused primitive instead of
// stalling it.  The argument below is unaffected, because both forms are
// unbuffered and neither reorders; but a header that quotes a line production
// no longer has is the citation rot this repository keeps paying for, and it
// sits in the block this door's own entry cites as authority.)
// It buffers nothing and reorders nothing.  So the beat that is
// offered at the window's material input is the SAME beat whose triangle is
// offered at GEOM.CLIP's data input, on the same clock, and one arbiter can
// own both halves without a tag and without a shadow FIFO.
//
// That is why this block emits `o_material_*` beside `o_ax_o..o_untex_o` and
// why the composer must drive BOTH from it.  Splitting the two across two
// arbiters would be the metadata-swap shape `CLAUDE.md` has a chapter about --
// "response A's data and B's metadata" wearing a material record.
//
// ---------------------------------------------------------------------------
// THE ARBITRATION LAW: RUN-LENGTH FAIR, NEVER BEAT FAIR
// ---------------------------------------------------------------------------
// This is the one law here that is not obvious, it is forced by a block
// downstream, and getting it wrong costs throughput rather than correctness --
// which is the direction that survives every result-checking test.
//
// `zhao_material_window` publishes ONE material for the whole span between
// GEOM.CLIP's input and the shell's triangle door, and **it drains that span
// before it changes what it publishes**.  Its header states the cost in terms:
// *"THE COST IS A DRAIN PER MATERIAL CHANGE"*, counted on
// `drain_stall_cycles_o`, and *"a repeat of the SAME {set, id} ... costs
// nothing at all: no drain, no request, no stall."*
//
// The span is GEOM.CLIP's three stages plus GEOM.SETUP plus GEOM.ATTRPACK, and
// the attrpack fork holds the pair to roughly one triangle every fourteen
// clocks.  So a beat-fair round robin between two producers of DIFFERENT
// materials would issue a full drain *and a resolve* between every pair of
// triangles: the pipeline would run at the drain rate, not the triangle rate,
// with every counter in the console reading healthy.
//
// So the grant is held while its owner keeps offering:
//
//   * the grant CHANGES only on a clock where the current owner is NOT
//     offering (`!c_valid_i[grant]`) and somebody else is;
//   * it never changes while a beat is offered and unaccepted, so the bundle
//     at GEOM.CLIP's input cannot move underneath a stalled handshake --
//     `err_hold_broken_o` is the guard that says so at run time;
//   * and the search for the next owner is round-robin from the one after the
//     current grant, so an owner that never goes idle cannot be starved by an
//     owner that never stops.
//
// `switches_o` counts the changes and `granted_o` counts the beats per client.
// The pair DISCRIMINATES (ruling R95): a door that lost its hold would show
// `switches_o` climbing with `granted_o`, and a door that never switched would
// show `switches_o` pinned at 1 while a client's `granted_o` stayed at zero.
// Neither reads as healthy.
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK DOES NOT DO, STATED SO NOBODY LOOKS FOR IT
// ---------------------------------------------------------------------------
//   * It does not interpret, convert or invent any field.  Every output is one
//     client's own input, selected.  There is no arithmetic in this file.
//   * It does not hold the R197 untextured gate.  That gate compares a
//     declaration against the window's PUBLISHED material and therefore lives
//     downstream of the window, in the composer, where it already is.  This
//     block carries `untex` through so that gate sees the right one.
//   * It does not buffer.  A door with a skid would put a triangle between the
//     material half and the triangle half, and those two halves would then be
//     one beat apart -- exactly the fault the section above refuses.
//
// Conservative SystemVerilog subset only; Quartus 17.0 syntax rules apply
// (elaboration checks inside `initial begin ... end`, explicit generate, no
// inline `for (genvar ...)`, loop variables declared inside their block).
`default_nettype none

module zhao_geom_clipdoor #(
    // How many producers share the door.  Two today (GEOM.REPLAY's mesh
    // triangles through the material window, and PART.EXPAND's polygon
    // particles); FORGE.PRIM and FORGE.SHADOW Route A are the named third and
    // fourth, which is why this is a parameter and not a pair of hard arms.
    parameter int unsigned NCLIENT = 2,
    // `zhao_geom_clip`'s ruling-5 attribute packet, flattened.  The door never
    // reads inside it.
    parameter int unsigned ATTRS   = 7,
    parameter int unsigned IDW     = 16,
    // ---- THE PER-CORNER IDENTITY AND THE PER-PRIMITIVE RIDER (ARENAID) ----
    // `zhao_geom_vertid` publishes GEOM.PARAMBUF's projected vertices from
    // GEOM.CLIP's OUTPUT, so the identity of each corner and the primitive's
    // material/raster/domain rider have to reach that output -- and they have
    // to arrive on the beat that WON, not on a live wire selected later.
    //
    // That is the whole reason they are muxed HERE rather than in the
    // composer. This door's own header states the law for the material half:
    // "a door that arbitrated only the triangle would let a resolve answer for
    // the material of a beat that did not win". An identity muxed outside the
    // door on `o_owner_o` would be the same shape with an index in it, and it
    // would be a SECOND selection network that has to agree with this one.
    //
    // The door never reads inside either field, exactly as it never reads
    // inside the attribute packet.
    parameter int unsigned VKEYW   = 24,
    parameter int unsigned RIDERW  = 50
) (
    input var logic clk,
    input var logic rst_n,

    // ---- the producers ------------------------------------------------------
    // Flattened per client, least significant slice is client 0.  A flattened
    // port is not a style choice: Quartus 17.0 will not take an unpacked array
    // port, and `CLAUDE.md` records what an inline `for (genvar ...)` does to
    // it.
    input  var logic [NCLIENT-1:0]              c_valid_i,
    output var logic [NCLIENT-1:0]              c_ready_o,
    // the triangle half
    input  var logic [NCLIENT*21-1:0]           c_ax_i,
    input  var logic [NCLIENT*21-1:0]           c_ay_i,
    input  var logic [NCLIENT*21-1:0]           c_bx_i,
    input  var logic [NCLIENT*21-1:0]           c_by_i,
    input  var logic [NCLIENT*21-1:0]           c_cx_i,
    input  var logic [NCLIENT*21-1:0]           c_cy_i,
    input  var logic [NCLIENT*3-1:0]            c_behind_i,
    input  var logic [NCLIENT*IDW-1:0]          c_src_id_i,
    input  var logic [NCLIENT-1:0]              c_untex_i,
    input  var logic [NCLIENT*2-1:0]            c_cull_mode_i,
    input  var logic [NCLIENT*ATTRS*32-1:0]     c_attr_a_i,
    input  var logic [NCLIENT*ATTRS*32-1:0]     c_attr_b_i,
    input  var logic [NCLIENT*ATTRS*32-1:0]     c_attr_c_i,
    // the identity half -- per corner, opaque, never interpreted here
    input  var logic [NCLIENT*VKEYW-1:0]        c_key_a_i,
    input  var logic [NCLIENT*VKEYW-1:0]        c_key_b_i,
    input  var logic [NCLIENT*VKEYW-1:0]        c_key_c_i,
    // the per-primitive rider -- opaque, granted with its triangle
    input  var logic [NCLIENT*RIDERW-1:0]       c_rider_i,
    // the material half -- what `zhao_material_window`'s input takes
    input  var logic [NCLIENT*32-1:0]           c_material_set_i,
    input  var logic [NCLIENT*16-1:0]           c_material_id_i,
    // THE PRODUCER'S MATERIAL-MODE DECLARATION (owner ruling 1, 2026-09-22).
    // It belongs to the MATERIAL HALF, so it is selected by the same grant on
    // the same beat as the pair it qualifies -- which is the whole reason this
    // door owns both halves. A mode arbitrated separately from its `{set, id}`
    // would be the metadata-swap shape with a third field in it.
    input  var logic [NCLIENT*2-1:0]            c_material_mode_i,
    // ---- THE PER-PRIMITIVE RASTER DECLARATION, 2026-09-23 (SHADOWRIDE) ----
    // R89's FLAT per-primitive alpha (unit8) and the raster state word
    // (`zhao_raster_fragment.sv:213-236`'s layout), granted on the SAME BEAT as
    // the triangle and its material -- which is the whole reason this door
    // exists rather than a second arbiter downstream: "a door that arbitrated
    // only the triangle would let a resolve answer for the material of a beat
    // that did not win", and the same sentence is true of a primitive's alpha.
    //
    // The pair travels TOGETHER and not separately: the state's `[4:3]` BLEND
    // field decides whether the alpha is read at all, because
    // `zhao_raster_blend_fin`'s BL_REPLACE arm throws the alpha product away.
    // Splitting them across two paths would let a primitive arrive transparent
    // with the previous one's opacity.
    input  var logic [NCLIENT*8-1:0]            c_vertex_alpha_i,
    input  var logic [NCLIENT*32-1:0]           c_frag_state_i,
    input  var logic [NCLIENT*8-1:0]            c_quality_tier_i,

    // ---- the door -----------------------------------------------------------
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
    output var logic [VKEYW-1:0]     o_key_a_o,
    output var logic [VKEYW-1:0]     o_key_b_o,
    output var logic [VKEYW-1:0]     o_key_c_o,
    output var logic [RIDERW-1:0]    o_rider_o,
    output var logic [31:0]          o_material_set_o,
    output var logic [15:0]          o_material_id_o,
    output var logic [1:0]           o_material_mode_o,
    output var logic [7:0]           o_vertex_alpha_o,
    output var logic [31:0]          o_frag_state_o,
    output var logic [7:0]           o_quality_tier_o,
    // Which client this beat belongs to, so a composer or a bench can say so
    // out loud rather than inferring it.  It is valid only while `o_valid_o`.
    output var logic [NCLIENT-1:0]   o_owner_o,

    // ---- evidence -----------------------------------------------------------
    // Beats taken, per client, flattened 32 bits each.  Saturating
    // (spec/counters.md 4): a counter that wraps reads LOW, and low is the
    // flattering direction for "how much work did the other producer get".
    output var logic [NCLIENT*32-1:0] granted_o,
    // Grant changes.  One at the first grant out of reset, then one per switch.
    output var logic [31:0]           switches_o,
    // Clocks in which somebody offered and nothing was granted, i.e. the door
    // itself was the stall.  Distinct from a sink stall, which is counted by
    // GEOM.CLIP.
    output var logic [31:0]           idle_offered_o,

    // ---- THE STRUCTURAL GUARD ----------------------------------------------
    // The grant moved while a beat was offered and unaccepted.  That is the one
    // fault that would put client A's material under client B's triangle, and
    // it is UNREACHABLE while the hold law below is correct -- so no legal
    // stimulus at these ports can move it, and it owes a COMMITTED MUTANT
    // rather than an argument (`tests/mutants/zhao_geom_clipdoor_mutant.sv`).
    output var logic [31:0]           err_hold_broken_o
);

  // Two clients is the composed case; one is legal and degenerate (the door
  // becomes a wire) and is used by the directed test's first section.  Zero is
  // not, and neither is an ATTRS the packet cannot hold.
  // synthesis translate_off
  initial begin
    if (NCLIENT < 1)
      $fatal(1, "zhao_geom_clipdoor: NCLIENT must be at least 1");
    if (ATTRS < 1)
      $fatal(1, "zhao_geom_clipdoor: ATTRS must be at least 1");
  end
  // synthesis translate_on

  localparam int unsigned AW = ATTRS * 32;

  // ==========================================================================
  // THE GRANT
  // ==========================================================================
  // `grant_q` is a one-hot register.  `held_q` says a grant exists; out of
  // reset there is none, so the first offering client takes it and that first
  // acquisition is counted as a switch (there is no material published before
  // it either, so the window treats it the same way).
  logic [NCLIENT-1:0] grant_q;
  logic               held_q;

  // The offered beat of the CURRENT owner, and whether the owner is offering.
  logic owner_offering_c;
  always_comb begin
    integer i;
    owner_offering_c = 1'b0;
    for (i = 0; i < NCLIENT; i = i + 1) begin
      if (grant_q[i] && c_valid_i[i]) owner_offering_c = 1'b1;
    end
  end

  // Round-robin search for the next owner, starting one PAST the current
  // grant.  Written as a priority walk over a rotated index so that it is a
  // plain combinational function with no state of its own -- a second pointer
  // would be a second opinion about whose turn it is.
  logic [NCLIENT-1:0] cand_c;
  logic               cand_any_c;
  // The rotation is written as TWO ascending passes -- [base, NCLIENT) then
  // [0, base) -- rather than one pass over `(base + k) % NCLIENT`.  That is not
  // style: a modulo needs an index variable, an `integer` index variable trips
  // UNUSEDSIGNAL on its top 31 bits, and waiving a warning to keep a modulo is
  // a worse trade than writing the two passes the modulo was standing in for.
  always_comb begin
    integer  k;
    integer  base;
    cand_c     = '0;
    cand_any_c = 1'b0;
    base       = 0;
    for (k = 0; k < NCLIENT; k = k + 1) begin
      if (grant_q[k]) base = k + 1;
    end
    for (k = 0; k < NCLIENT; k = k + 1) begin
      if (!cand_any_c && (k >= base) && c_valid_i[k]) begin
        cand_any_c = 1'b1;
        cand_c     = '0;
        cand_c[k]  = 1'b1;
      end
    end
    for (k = 0; k < NCLIENT; k = k + 1) begin
      if (!cand_any_c && (k < base) && c_valid_i[k]) begin
        cand_any_c = 1'b1;
        cand_c     = '0;
        cand_c[k]  = 1'b1;
      end
    end
  end

  // THE HOLD LAW.  The grant may move only when the current owner is not
  // offering.  `o_valid_o` is exactly `owner_offering_c`, so this is the same
  // sentence as "the grant never moves while a beat is offered".
  logic              take_new_c;
  logic [NCLIENT-1:0] grant_c;
  always_comb begin
    take_new_c = (!held_q || !owner_offering_c) && cand_any_c;
    grant_c    = take_new_c ? cand_c : grant_q;
  end

  // ==========================================================================
  // THE SELECTED BEAT
  // ==========================================================================
  // Field placement only.  Every output below is one client's own input.
  always_comb begin
    integer i;
    o_ax_o           = '0;
    o_ay_o           = '0;
    o_bx_o           = '0;
    o_by_o           = '0;
    o_cx_o           = '0;
    o_cy_o           = '0;
    o_behind_o       = '0;
    o_src_id_o       = '0;
    o_untex_o        = 1'b0;
    o_cull_mode_o    = '0;
    o_attr_a_o       = '0;
    o_attr_b_o       = '0;
    o_attr_c_o       = '0;
    o_key_a_o        = '0;
    o_key_b_o        = '0;
    o_key_c_o        = '0;
    o_rider_o        = '0;
    o_material_set_o = '0;
    o_material_id_o  = '0;
    o_material_mode_o = '0;
    o_vertex_alpha_o = '0;
    o_frag_state_o   = '0;
    o_quality_tier_o = '0;
    for (i = 0; i < NCLIENT; i = i + 1) begin
      if (grant_q[i]) begin
        o_ax_o           = $signed(c_ax_i[i*21 +: 21]);
        o_ay_o           = $signed(c_ay_i[i*21 +: 21]);
        o_bx_o           = $signed(c_bx_i[i*21 +: 21]);
        o_by_o           = $signed(c_by_i[i*21 +: 21]);
        o_cx_o           = $signed(c_cx_i[i*21 +: 21]);
        o_cy_o           = $signed(c_cy_i[i*21 +: 21]);
        o_behind_o       = c_behind_i[i*3 +: 3];
        o_src_id_o       = c_src_id_i[i*IDW +: IDW];
        o_untex_o        = c_untex_i[i];
        o_cull_mode_o    = c_cull_mode_i[i*2 +: 2];
        o_attr_a_o       = c_attr_a_i[i*AW +: AW];
        o_attr_b_o       = c_attr_b_i[i*AW +: AW];
        o_attr_c_o       = c_attr_c_i[i*AW +: AW];
        o_key_a_o        = c_key_a_i[i*VKEYW +: VKEYW];
        o_key_b_o        = c_key_b_i[i*VKEYW +: VKEYW];
        o_key_c_o        = c_key_c_i[i*VKEYW +: VKEYW];
        o_rider_o        = c_rider_i[i*RIDERW +: RIDERW];
        o_material_set_o = c_material_set_i[i*32 +: 32];
        o_material_id_o  = c_material_id_i[i*16 +: 16];
        o_material_mode_o = c_material_mode_i[i*2 +: 2];
        o_vertex_alpha_o = c_vertex_alpha_i[i*8 +: 8];
        o_frag_state_o   = c_frag_state_i[i*32 +: 32];
        o_quality_tier_o = c_quality_tier_i[i*8 +: 8];
      end
    end
  end

  assign o_owner_o = grant_q;
  assign o_valid_o = owner_offering_c;

  // THE READY FAN-OUT, and the property that makes it legal: no client's ready
  // is a function of that client's own valid.  `grant_q` is a REGISTER and
  // `o_ready_i` is the sink's, so `c_ready_o[i] = grant_q[i] && o_ready_i` has
  // no combinational path from `c_valid_i` at all.  `zhao_geom_clip`'s
  // `tri_ready_o` is likewise `pipe_en`, a function of its own stage 3 and the
  // sink -- so the composed path closes no loop.
  always_comb begin
    integer i;
    for (i = 0; i < NCLIENT; i = i + 1) begin
      c_ready_o[i] = grant_q[i] && o_ready_i;
    end
  end

  wire beat_take_c = o_valid_o && o_ready_i;
  wire anyone_c    = |c_valid_i;

  // ==========================================================================
  // STATE AND EVIDENCE
  // ==========================================================================
  generate
    genvar g;
    for (g = 0; g < NCLIENT; g = g + 1) begin : g_granted
      always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
          granted_o[g*32 +: 32] <= 32'd0;
        end else if (beat_take_c && grant_q[g] &&
                     (granted_o[g*32 +: 32] != 32'hffff_ffff)) begin
          granted_o[g*32 +: 32] <= granted_o[g*32 +: 32] + 32'd1;
        end
      end
    end
  endgenerate

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      grant_q           <= '0;
      held_q            <= 1'b0;
      switches_o        <= 32'd0;
      idle_offered_o    <= 32'd0;
      err_hold_broken_o <= 32'd0;
    end else begin
      grant_q <= grant_c;
      if (take_new_c) held_q <= 1'b1;

      if (take_new_c && (grant_c != grant_q) &&
          (switches_o != 32'hffff_ffff))
        switches_o <= switches_o + 32'd1;

      // Somebody offered, and this door granted nobody.  With the law above
      // that can only happen on the clock a grant is being acquired, so it is
      // the door's own acquisition latency and nothing else.
      if (anyone_c && !o_valid_o && (idle_offered_o != 32'hffff_ffff))
        idle_offered_o <= idle_offered_o + 32'd1;

      // THE GUARD.  A beat was offered and not taken, and the grant moved
      // anyway.  `take_new_c` requires `!owner_offering_c`, and `o_valid_o` IS
      // `owner_offering_c`, so this is unreachable while the hold law holds.
      if (o_valid_o && !o_ready_i && (grant_c != grant_q) &&
          (err_hold_broken_o != 32'hffff_ffff))
        err_hold_broken_o <= err_hold_broken_o + 32'd1;
    end
  end

  // The same statement as an assertion, so a bench sees it immediately rather
  // than reading a counter at the end.  `synthesis translate_off` keeps it out
  // of the fabric and does NOT keep it out of Verilator, which is what is
  // wanted (CLAUDE.md, proven by planting a syntax error inside one).
  // synthesis translate_off
  // The rst_n term is a deliberate SYNCHRONOUS read: in reset `grant_q` and
  // `grant_c` are both zero and `o_valid_o` is low, where the assertion is
  // vacuously true and must not be evaluated against a half-reset state.
  /* verilator lint_off SYNCASYNCNET */
  always_ff @(posedge clk) begin
    if (rst_n) begin
      a_clipdoor_hold : assert (!(o_valid_o && !o_ready_i && (grant_c != grant_q)))
        else $fatal(1, "zhao_geom_clipdoor: the grant moved while a beat was offered and unaccepted");
    end
  end
  /* verilator lint_on SYNCASYNCNET */
  // synthesis translate_on

endmodule

`default_nettype wire
