// zhao_terrain_matjoin.sv -- TERRAIN.MATJOIN. Earth out-lane 2 composed onto
// the authored layer-E material, per cell, in list order.
//
// THE ASYMMETRY THIS BLOCK EXISTS TO REMOVE. In the composed console today the
// two halves of a terrain cell arrive by different routes:
//
//   heights  : TERRAIN.PAGESTREAM -> TERRAIN.PATCH (composed with the field
//              lane) -> TERRAIN.COMPCACHE
//   material : TERRAIN.PAGESTREAM ----------------------------> TERRAIN.COMPCACHE
//
// Height goes THROUGH the composer. Material goes AROUND it. That is the whole
// of entry I34's open material channel: `zhao_terrain_patch` takes
// `fld_height_i` and nothing else, and `zhao_terrain_compcache_front`'s layer-E
// write face has exactly one driver -- the page stream, i.e. authored layer E.
// A field material result had no port to arrive on, so `efa_material` and
// `efa_present` sat in `zhao_console_core` as declared wires that nothing read.
//
// This block is the missing hop, and it is deliberately the SAME SHAPE as the
// one TERRVEL built for out-lane 1: a named block with a contract and a test,
// riding the per-vertex lane stream, taking TERRAIN.PATCH's own section 9.1
// answer rather than re-deciding it. `zhao_terrain_veljoin` is the sibling to
// read beside this one.
//
// ---------------------------------------------------------------------------
// WHY THIS IS NOT A SECOND WRITER INTO THE PAGE, and why that distinction is
// the one that decides where the arbitration lives
// ---------------------------------------------------------------------------
// `zref_fieldir.hpp`'s sinks header is explicit: "ALL THREE ARE LIVE
// COMPOSITION, NEVER PERSISTENT MUTATION ... a field evaluated every frame must
// never rewrite a VRAM page every frame, which is the failure this separation
// exists to prevent."
//
// So the arbitration between authored layer E and a field result does NOT
// belong at the page store. It belongs exactly where height's already does: at
// the COMPOSE point, once per frame, leaving the authored value untouched.
// TERRAIN.COMPCACHE is the per-frame compose cache -- it is already where
// TERRAIN.PATCH deposits field-composed HEIGHTS -- so composing material into
// the same plane is symmetric with height rather than a mutation of anything.
//
// The authored triple is never modified. When no field covers a cell, or the
// field writes no material, this block emits the authored bytes BIT FOR BIT and
// the console is unchanged. That default is the reason composing it is safe.
//
// ---------------------------------------------------------------------------
// THE LAW, AND IT IS RATIFIED RATHER THAN CHOSEN HERE
// ---------------------------------------------------------------------------
// `zref::fieldir::compose_material`: start from authored layer E, and THE LAST
// ENABLED WRITER WINS, IN ACCEPTED COMMAND ORDER. The header beside it gives
// the reason the order is the priority: "hardware inventing an implicit
// material hierarchy would be a game rule smuggled into silicon, and software
// can already express any precedence it wants by choosing the order it
// submits."
//
// TERRAIN.PATCH's chosen law 1 is that the field lane carries "one height lane
// per ACCEPTED list entry per vertex, IN LIST ORDER". Out-lane 2 rides the same
// word. So "last enabled writer wins" is implemented here by simply letting
// each enabled lane word overwrite the held override -- the list order IS the
// command order, and the last one standing is the answer. No priority encoder,
// no hierarchy, no tie to break.
//
// A lane word is ENABLED when all three of these hold, and the three are
// independent:
//   * `f_covers_i`  -- TERRAIN.PATCH's section 9.1 footprint answer. Decided
//                      ONCE, by the block that owns the list, never re-decided
//                      here.
//   * `f_present_i` -- the Earth record's ordinal-2 presence bit. The ratified
//                      rule is that AN ABSENT OUTPUT IS NOT A WRITE OF ZERO, so
//                      a field that moves the ground and declares no material
//                      leaves the authored material alone.
//   * a v1 TAG      -- see below.
//
// ---------------------------------------------------------------------------
// THE TAG CHECK IS A REFUSAL, AND IT IS THE ONLY REASON THE PRESENCE RULE CAN
// BE HONOURED AT ALL
// ---------------------------------------------------------------------------
// `spec/qformats.md` 14 and `zhao_material_token_pkg` carry the encoding. The
// part that matters here: because `terrain_rules` 6.2 gives weight 0 and 255
// meanings, EVERY 24-bit pattern is a legal material state. A decoder without a
// tag therefore cannot tell a real result from a lane that was never written --
// and `32'd0`, which is what the adapter parks on an absent lane, would decode
// as the perfectly legal triple {0,0,0} and RENDER.
//
// So a token whose tag is not `ZMT_TAG_V1` is REFUSED: the authored triple is
// kept, unchanged, and `token_refused_o` counts it. Nothing is substituted.
// That counter is a real detector and not a blind one -- it differences the
// incoming word against a CONSTANT, not against a second register loaded by the
// same enable, which is the shape `CLAUDE.md` records as structurally unable to
// fire.
//
// ---------------------------------------------------------------------------
// THE SEQUENCING THIS BLOCK RESTS ON, STATED SO IT CAN BE CHECKED
// ---------------------------------------------------------------------------
// `zhao_terrain_patch` holds ONE vertex at a time: `vtx_ready_o = !busy &&
// out_free` and `fld_ready_o = busy`. So for a vertex V the order is
//
//   accept(V)  ->  lane beats for V  ->  state publish for V  ->  accept(V+1)
//
// and the two faces are mutually exclusive by construction: on the accept cycle
// `busy` is still low, so no lane word can be offered.
//
// This block therefore LATCHES the authored triple and the cell address at
// accept, accumulates the field override across V's lane beats, and EMITS the
// composed write when the patch publishes V's state record. Emitting at the
// state publish rather than at the next accept is deliberate: it pins the
// material write to the SAME record as the height it belongs to, so the write
// can never be outstanding when `fill_done_o` -- which counts height records --
// declares the parity full.
//
// THE ASSUMPTION IS GUARDED RATHER THAN TRUSTED. If a second accept ever
// arrives while a cell is still held, `held_overrun_o` counts it AND the held
// cell is emitted immediately so nothing is silently dropped. That counter
// differences two INDEPENDENTLY DRIVEN signals -- the page stream's beat and
// the patch's publish -- so it is not blind to the timing fault it exists for.
// It is unreachable with legal stimulus in the composed console, which is
// exactly why it is driven deliberately at this block's own ports in
// `tests/terrain/terrain_matjoin_directed.cpp` and seen to fire there.
//
// 65 OF THE 1,089 VERTICES OWN NO CELL. The lattice is 33x33 vertices over a
// 32x32 cell plane, so the last column and the last row write nothing -- the
// authored path already does exactly this (`mat_we_i` is the page stream's
// `have_cell` beat). Their lane words are counted by `lane_no_cell_o`, which is
// a CENSUS AND NOT A FAULT: it is expected to read non-zero on every patch that
// has a field, and it is named so that nobody reads it as an error later.

`default_nettype none

module zhao_terrain_matjoin #(
    parameter int unsigned CENSUS_W = 32
) (
    input var logic clk,
    input var logic rst_n,

    // ---- the authored layer-E write face, as TERRAIN.PAGESTREAM presents it,
    // ---- on the same beat TERRAIN.PATCH accepts the vertex.
    input var logic       a_we_i,
    input var logic [4:0] a_ci_i,
    input var logic [4:0] a_cj_i,
    input var logic [7:0] a_mat_a_i,
    input var logic [7:0] a_mat_b_i,
    input var logic [7:0] a_weight_i,

    // ---- the Earth answer lane, forked from the stream TERRAIN.PATCH consumes.
    // `f_fire_i` is that lane's accepted beat; the word and the coverage answer
    // are the adapter's and the patch's own, unbuffered.
    input var logic        f_fire_i,
    input var logic        f_covers_i,
    input var logic        f_present_i,
    input var logic [31:0] f_material_i,

    // ---- TERRAIN.PATCH's state publish for the held vertex: the emit beat.
    input var logic st_fire_i,

    // ---- the composed write face, port-for-port TERRAIN.COMPCACHE's layer E.
    output var logic       o_we_o,
    output var logic [4:0] o_ci_o,
    output var logic [4:0] o_cj_o,
    output var logic [7:0] o_mat_a_o,
    output var logic [7:0] o_mat_b_o,
    output var logic [7:0] o_weight_o,

    // ---- evidence
    output var logic [CENSUS_W-1:0] cells_written_o,
    output var logic [CENSUS_W-1:0] field_composed_o,
    output var logic [CENSUS_W-1:0] token_refused_o,
    output var logic [CENSUS_W-1:0] lane_no_cell_o,
    output var logic [CENSUS_W-1:0] held_overrun_o,
    output var logic                idle_o
);

  import zhao_material_token_pkg::*;

  // ---- the held cell -------------------------------------------------------
  logic       held_q;
  logic [4:0] held_ci_q, held_cj_q;
  logic [7:0] auth_a_q, auth_b_q, auth_w_q;

  // The field override. `ov_q` is "a v1 token was accepted for this cell"; the
  // bytes are the LAST such token, which is the ratified law.
  logic       ov_q;
  logic [7:0] ov_a_q, ov_b_q, ov_w_q;

  // ---- the lane word's verdict, this cycle --------------------------------
  // Deliberately three separate terms rather than one collapsed predicate: each
  // is a different law and a reader has to be able to see which one refused.
  wire lane_enabled_c = f_fire_i && f_covers_i && f_present_i;
  wire lane_tag_ok_c  = zmt_tag_ok(f_material_i);
  wire lane_takes_c   = lane_enabled_c && lane_tag_ok_c;
  wire lane_refuse_c  = lane_enabled_c && !lane_tag_ok_c;

  // ---- the emitted triple --------------------------------------------------
  // The override if a token was taken for this cell, otherwise the authored
  // bytes UNCHANGED. There is no third case and no default value.
  // A word arriving in the SAME cycle as the emit is still the last enabled
  // writer and must win. Folding it in combinationally makes the answer
  // independent of whether `f_fire_i` and `st_fire_i` can coincide -- which is
  // a fact about ANOTHER block's handshake, and therefore exactly the kind of
  // thing this file must not quietly depend on.
  wire       take_now_c = lane_takes_c && held_q;
  wire       ov_eff_c   = ov_q || take_now_c;
  wire [7:0] out_a_c = take_now_c ? zmt_mat_a(f_material_i) : (ov_q ? ov_a_q : auth_a_q);
  wire [7:0] out_b_c = take_now_c ? zmt_mat_b(f_material_i) : (ov_q ? ov_b_q : auth_b_q);
  wire [7:0] out_w_c = take_now_c ? zmt_weight(f_material_i) : (ov_q ? ov_w_q : auth_w_q);

  // An accept arriving while a cell is still held is the sequencing fault the
  // header names. It must not lose the held cell, so it emits too.
  wire overrun_c = a_we_i && held_q;
  wire emit_c    = held_q && (st_fire_i || overrun_c);

  assign o_we_o    = emit_c;
  assign o_ci_o    = held_ci_q;
  assign o_cj_o    = held_cj_q;
  assign o_mat_a_o = out_a_c;
  assign o_mat_b_o = out_b_c;
  assign o_weight_o = out_w_c;

  assign idle_o = !held_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      held_q    <= 1'b0;
      held_ci_q <= 5'd0;
      held_cj_q <= 5'd0;
      auth_a_q  <= 8'd0;
      auth_b_q  <= 8'd0;
      auth_w_q  <= 8'd0;
      ov_q      <= 1'b0;
      ov_a_q    <= 8'd0;
      ov_b_q    <= 8'd0;
      ov_w_q    <= 8'd0;

      cells_written_o  <= '0;
      field_composed_o <= '0;
      token_refused_o  <= '0;
      lane_no_cell_o   <= '0;
      held_overrun_o   <= '0;
    end else begin
      // ---- evidence, counted where the event is, saturating at all ones ----
      if (emit_c && (cells_written_o != {CENSUS_W{1'b1}}))
        cells_written_o <= cells_written_o + 1'b1;
      if (emit_c && ov_eff_c && (field_composed_o != {CENSUS_W{1'b1}}))
        field_composed_o <= field_composed_o + 1'b1;
      if (lane_refuse_c && (token_refused_o != {CENSUS_W{1'b1}}))
        token_refused_o <= token_refused_o + 1'b1;
      if (f_fire_i && !held_q && (lane_no_cell_o != {CENSUS_W{1'b1}}))
        lane_no_cell_o <= lane_no_cell_o + 1'b1;
      if (overrun_c && (held_overrun_o != {CENSUS_W{1'b1}}))
        held_overrun_o <= held_overrun_o + 1'b1;

      // ---- the field override accumulates across THIS cell's lane words ----
      // Last enabled writer wins: a later word simply overwrites an earlier
      // one. A refused token changes nothing, which is what "keep the authored
      // state" means at this level.
      if (lane_takes_c && held_q) begin
        ov_q   <= 1'b1;
        ov_a_q <= zmt_mat_a(f_material_i);
        ov_b_q <= zmt_mat_b(f_material_i);
        ov_w_q <= zmt_weight(f_material_i);
      end

      // ---- the cell boundary ----------------------------------------------
      // The accept latch is written LAST so that it wins over the state-publish
      // clear in the same cycle; the two are mutually exclusive in the composed
      // console and this makes the overrun path lossless rather than racy.
      if (st_fire_i && held_q && !a_we_i) begin
        held_q <= 1'b0;
        ov_q   <= 1'b0;
      end

      if (a_we_i) begin
        held_q    <= 1'b1;
        held_ci_q <= a_ci_i;
        held_cj_q <= a_cj_i;
        auth_a_q  <= a_mat_a_i;
        auth_b_q  <= a_mat_b_i;
        auth_w_q  <= a_weight_i;
        ov_q      <= 1'b0;
        ov_a_q    <= 8'd0;
        ov_b_q    <= 8'd0;
        ov_w_q    <= 8'd0;
      end
    end
  end

endmodule

`default_nettype wire
