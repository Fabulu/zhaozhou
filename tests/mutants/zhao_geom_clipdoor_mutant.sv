// zhao_geom_clipdoor_mutant.sv -- THE POSITIVE CONTROL FOR
// `err_hold_broken_o`, which no legal stimulus can fire.
//
// ---------------------------------------------------------------------------
// WHY THIS FILE EXISTS
// ---------------------------------------------------------------------------
// `err_hold_broken_o` counts the grant moving while a beat is offered and
// unaccepted. With the hold law correct that state is UNREACHABLE: `take_new_c`
// requires `!owner_offering_c` and `o_valid_o` IS `owner_offering_c`, so the
// two conditions are mutually exclusive by construction. No input at this
// block's own ports can make it move, and `CLAUDE.md` is explicit that a
// counter asserted zero and never seen to move is a CLAIM rather than a
// measurement -- "a guard you cannot reach with legal stimulus needs a
// COMMITTED MUTANT".
//
// It is a COPY, not a wrapper, because the fault is inside a combinational
// block and no parameter reaches it. `tools/budget/mutant_copy_drift.py`
// therefore gates it: REGENERATE THIS FILE if `zhao_geom_clipdoor.sv` changes
// shape. A copy of an old version is a positive control for a block that no
// longer exists, and it goes stale in the FLATTERING direction -- it keeps
// passing.
//
// ---------------------------------------------------------------------------
// THE ONE SUBSTANTIVE CHANGE
// ---------------------------------------------------------------------------
//     take_new_c = (!held_q || !owner_offering_c) && cand_any_c;
//  -> take_new_c = cand_any_c;
//
// WHAT THE FAULT WOULD COST IN THE CONSOLE, which is the reason the counter is
// worth having: the door's output feeds `zhao_material_window`'s material input
// and `zhao_geom_clip`'s triangle input on the SAME beat. A grant that moves
// under a stalled handshake puts one client's material beside the other
// client's triangle -- the metadata-swap shape, with a particle shaded by a
// mesh's texture and every accepted/emitted counter in the console balancing
// perfectly.
//
// THE DRIVER IS tests/geometry/geom_clipdoor_mutant_control.cpp AND ITS
// POLARITY IS INVERTED: it passes when `err_hold_broken_o` is NON-ZERO. It is
// evidence about the instrument, not about the design.
//
// Renamed so no production source list can elaborate it by mistake.
//
// ---------------------------------------------------------------------------
// RE-VERIFIED AGAINST CURRENT PRODUCTION 2026-09-23 (packet gz/trimerge)
// ---------------------------------------------------------------------------
// `mutant_copy_drift.py` flagged this copy because TRIMERGE committed a comment
// correction to `zhao_geom_clipdoor.sv`'s header, which made production newer
// than this file. The tool's signal is PROVENANCE, and it was right to fire:
// commit order really had inverted. The copy was then diffed against current
// production body-to-body, and it is FAITHFUL -- the only differences are this
// file's own replaced header, the one substantive line below, the deliberately
// disabled simulation assertion (with its reason beside it) and the
// UNUSEDSIGNAL pragma around the now-unread `held_q`. Nothing of production's
// shape was missing.
//
// This note is the refresh. It is recorded rather than done silently because a
// copy whose provenance is corrected without anyone LOOKING at the body is the
// stale-copy failure with a newer timestamp on it -- which is worse than the
// stale copy, since the timestamp then argues against checking.
`default_nettype none

module zhao_geom_clipdoor_mutant #(
    // How many producers share the door.  Two today (GEOM.REPLAY's mesh
    // triangles through the material window, and PART.EXPAND's polygon
    // particles); FORGE.PRIM and FORGE.SHADOW Route A are the named third and
    // fourth, which is why this is a parameter and not a pair of hard arms.
    parameter int unsigned NCLIENT = 2,
    // `zhao_geom_clip`'s ruling-5 attribute packet, flattened.  The door never
    // reads inside it.
    parameter int unsigned ATTRS   = 7,
    parameter int unsigned IDW     = 16,
    // ARENAID 2026-09-25: carried forward from production so this copy still
    // elaborates against the shared driver. NOT the mutation.
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
    input  var logic [NCLIENT*VKEYW-1:0]        c_key_a_i,
    input  var logic [NCLIENT*VKEYW-1:0]        c_key_b_i,
    input  var logic [NCLIENT*VKEYW-1:0]        c_key_c_i,
    input  var logic [NCLIENT*RIDERW-1:0]       c_rider_i,
    // the material half -- what `zhao_material_window`'s input takes
    input  var logic [NCLIENT*32-1:0]           c_material_set_i,
    input  var logic [NCLIENT*16-1:0]           c_material_id_i,
    input  var logic [NCLIENT*2-1:0]            c_material_mode_i,
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
      $fatal(1, "zhao_geom_clipdoor_mutant: NCLIENT must be at least 1");
    if (ATTRS < 1)
      $fatal(1, "zhao_geom_clipdoor_mutant: ATTRS must be at least 1");
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
  // MUTANT CONSEQUENCE, not a second mutation: production reads `held_q`
  // in the hold law that the one substantive line above deletes, so here it
  // is written and never read. It is kept rather than removed so this file
  // stays a one-line diff against production for `mutant_copy_drift.py`.
  /* verilator lint_off UNUSEDSIGNAL */
  logic               held_q;
  /* verilator lint_on UNUSEDSIGNAL */

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
    // MUTANT, THE ONE SUBSTANTIVE LINE. Production reads
    //     take_new_c = (!held_q || !owner_offering_c) && cand_any_c;
    // The `!owner_offering_c` term IS the hold law: without it the grant
    // re-arbitrates every clock, so a beat offered into a stalled sink is
    // replaced by the other client's while the handshake is still open.
    take_new_c = cand_any_c;
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
  // THE ASSERTION IS DISABLED IN THE MUTANT ONLY, and the reason is beside it:
  // it is the SAME statement the counter below makes, and it $fatal's on the
  // first mutated clock -- before the synthesizable counter can be read. The
  // assertion firing is independent corroboration; the counter is the thing
  // that ships, so the counter is what this control measures (CLAUDE.md,
  // "when a mutant trips a SIMULATION assertion before the synthesizable
  // counter can be read").
  //
  //  always_ff @(posedge clk) begin
  //    if (rst_n) begin
  //      a_clipdoor_hold : assert (!(o_valid_o && !o_ready_i && (grant_c != grant_q)))
  //        else $fatal(1, "... the grant moved while a beat was offered and unaccepted");
  //    end
  //  end
  // synthesis translate_on

endmodule

`default_nettype wire
