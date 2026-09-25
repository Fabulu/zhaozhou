// zhao_terrain_islandseal.sv -- THE ISLAND PITCH SEAL, and the admission-time
// check that makes the page header a CHECKED REDUNDANT VALUE rather than a
// second source of placement law.
//
// Law: reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt section 2
//      spec/terrain_rules.md 1.3 (the four legal pitches), 1.5 (the island
//        table: ONE pitch per island), 2.1 (+2 pitch_log2 "must match the
//        island table", +16 envelope "must equal origin + coords x 32 x pitch
//        exactly -- the packer asserts it; redundancy is a corruption check")
//      design/contracts/TERRAIN.EDGERECON.md, "WHAT P4 STILL OWES" items 2, 3
//
// ===========================================================================
// THE DECISION THIS BLOCK IS, WRITTEN WHERE IT GOVERNS
// ===========================================================================
// QUESTION. Owner directive section 2 makes "the frame-sealed island
// descriptor's pitch_log2 authoritative for EVERY page belonging to that
// island generation", with the page-header pitch demoted to "a checked
// redundant value". `zhao_terrain_prepwalk` therefore takes the island pitch
// as a frame input, `frz_pitch_log2_i`. **THERE IS NO LIVE PRODUCER OF AN
// ISLAND DESCRIPTOR IN THE CONSOLE.** Measured, not inherited:
//   * `zhao_terrain_island_dir` is NOT instantiated in `zhao_console_core.sv`
//     -- and it would not help if it were, because `desc_pitch_log2_i` is an
//     INPUT of that block (`zhao_terrain_island_dir.sv:70`). It CONSUMES a
//     descriptor; it produces no pitch. TERRAIN.ISLAND is additionally already
//     `superseded by a ruling` in the completion register.
//   * The only ratified carrier of an island table is `TerrainEpoch 0x0220`'s
//     `island_table_handle` -- and that command is `reserved` in
//     `spec/commands.zidl:616`, refused by the executor with
//     ZH_ABI_UNIMPLEMENTED_COMMAND. Its own comment says there is no
//     `island_table` handle class and that minting one is a generator change
//     plus a ruling.
//
// CHOSEN OPTION. Derive the authoritative value from what IS composed, and
// make the derivation the CHECK the directive asks for. This block SEALS the
// island's pitch from the first legal page header admitted under a resource
// epoch / island id, and thereafter CHECKS every page header against the seal:
// pitch, island identity, and the declared envelope origin recomputed through
// `zhao_terrain_place_law_pkg` -- the SAME functions `zhao_terrain_place`
// calls, so this is not a second implementation of the placement law. A header
// that disagrees is REFUSED (`refuse_o`) and COUNTED, per failing field.
//
// REASON, AND THE ALTERNATIVES REFUSED.
//   * Spec 1.5 gives an island exactly ONE pitch and spec 2.1 +2 says the
//     header's copy "must match the island table". The two are therefore the
//     same number by construction, and the only thing a real island descriptor
//     would add over the first header is an INDEPENDENT witness for the
//     comparison. The comparison is what was missing -- not a register.
//   * Minting a command: an ABI change, a generator change, an executor arm
//     and a ruling about generation semantics, for a number the tree already
//     has four independent copies of. The directive says prefer what is
//     composed.
//   * Instantiating `zhao_terrain_island_dir`: composes a CONSUMER with no
//     producer -- R75's close-one-gap-open-another, and the block is
//     superseded besides.
//   * Tying `frz_pitch_log2_i` to a constant: a tie-off, which is the one move
//     this packet's whole subject forbids.
//
// CONSTRAINTS / COST. ~90 flops (the seal, the epoch, the island, seven
// counters), a comparator tree, two shifts through the shared package. 0 DSP,
// 0 M10K, no memory port, no handshake on any existing block.
//
// CONSEQUENCES. No port changes anywhere: this block SNOOPS the same
// `thr_h_*` header beat `zhao_terrain_place` takes and the same beat
// `ptt_pitch_q` already snoops in the console. Existing behaviour is
// unchanged -- EMIT still places from the page header through TERRAIN.PLACE,
// exactly as before. What is new is that a DISAGREEMENT is now detected,
// counted, and propagated to the freeze witness, so PREPARE and EMIT can never
// place one patch two ways and combine the halves.
//
// ===========================================================================
// WHY THIS IS NOT `ptt_pitch_q`, WHICH IS TWENTY LINES AWAY IN THE COMPOSER
// ===========================================================================
// `zhao_console_core.sv` already holds a pitch off this same beat, for
// PART.TERRAIN_TAP. Its law is LAST LEGAL VALUE WINS, with no comparison and
// no refusal, and it answers a different question -- "what pitch was the
// lattice the cache is SERVING placed at?", which must keep tracking even
// across a refused header. This block's law is FIRST VALUE SEALS, EVERY LATER
// ONE IS CHECKED. Two different laws for two different questions; neither is
// a copy of the other, and the composer keeps both.
//
// ===========================================================================
// WHAT "REFUSE" MEANS HERE, AND WHY IT IS NOT A GATE ON THE COMPOSE SPINE
// ===========================================================================
// The directive: "Refuse and count a mismatch before publishing residency; do
// not silently rescale a page." The crack hazard is a PLACEMENT hazard -- one
// patch placed at the island pitch in PREPARE and at its own header's pitch in
// EMIT -- so what must be prevented is the two halves COMBINING, and that is
// what `refuse_o` prevents.
//
// It does it by feeding the console's FREEZE WITNESS (`frz_tok_i` on
// `zhao_terrain_prepwalk`) rather than by gating `thr_h_valid` into
// TERRAIN.PLACE. Gating the compose spine on a comparator would let a single
// corrupt header WEDGE a patch that the sequencer is waiting to complete --
// trading a counted fallback for a hang. Moving the witness instead makes
// `freeze_broken_o` fire, drops `prep_valid_o`, and every edge in the frame
// falls back to the console's existing conservative behaviour. That is the
// directive's own "bounded collision handling or a correctness-preserving
// fallback", and it is never silent: seven counters say which field
// disagreed.
//
// NOTHING IS RESCALED. This block computes no placement and drives no
// coordinate. It compares.
//
// Conservative SystemVerilog subset only (charter section 2). ONE `import`
// statement in the module header: Quartus 17.0 rejects a second one outright
// while `verilator --lint-only -Wall` accepts both forms with 0 diagnostics.
`default_nettype none

module zhao_terrain_islandseal
  import zhao_terrain_place_law_pkg::*;
#(
    // The census width. 32 so the counters read the same as every other
    // counter at this module's boundary; `spec/counters.md` section 4
    // saturation is implemented below rather than assumed.
    parameter int unsigned CW = 32
) (
    input  var logic clk,
    input  var logic rst_n,

    // The LIVE resource epoch. A new epoch is a new island generation, so the
    // seal is dropped and re-taken rather than reported as a mismatch.
    input  var logic [31:0] epoch_i,

    // ---- the page header beat -----------------------------------------------
    // These are `zhao_terrain_hdrread`'s outputs, the SAME five nets
    // `zhao_terrain_place` takes on its `hdr_*` port and on the same cycle:
    // PLACE's `hdr_ready_o` is constant 1 by contract, so a header beat is
    // exactly one cycle and both blocks see the identical record.
    input  var logic               hdr_valid_i,
    input  var logic signed [ 7:0] hdr_pitch_log2_i,  // spec 2.1 +2
    input  var logic        [31:0] hdr_island_i,      // spec 2.1 +4
    input  var logic signed [15:0] hdr_ix_i,          // spec 2.1 +8
    input  var logic signed [15:0] hdr_iz_i,          // spec 2.1 +10
    input  var logic signed [31:0] hdr_env_x0_i,      // spec 2.1 +16
    input  var logic signed [31:0] hdr_env_z0_i,

    // ---- the authoritative island descriptor --------------------------------
    // `pitch_log2_o` is what owner directive section 2 makes authoritative for
    // every page of this island generation. It is frame-scoped in the sense
    // that matters: it changes only when a new island generation seals, and a
    // change during a PREPARE walk moves the freeze witness below.
    output var logic signed [ 7:0] pitch_log2_o,
    output var logic        [31:0] island_o,
    output var logic               sealed_o,

    // ---- the refusal --------------------------------------------------------
    // PULSE, on the header beat, exactly when that header disagrees with the
    // seal in any field. The console folds it into the PREPARE freeze witness.
    output var logic refuse_o,

    // ---- counters -----------------------------------------------------------
    output var logic [CW-1:0] headers_checked_o,  // legal headers compared against a live seal
    output var logic [CW-1:0] seals_o,            // seals taken (including the first)
    output var logic [CW-1:0] reseals_o,          // a seal replaced: new epoch or new island
    output var logic [CW-1:0] pitch_illegal_o,    // outside spec 1.3's {-1,0,+1,+2}
    output var logic [CW-1:0] pitch_mismatch_o,   // legal, and not the island's
    output var logic [CW-1:0] island_bounced_o,   // the island id changed under one epoch
    output var logic [CW-1:0] envelope_bad_o      // +16 is not origin + coords x 32 x pitch
);

  // The seal.
  logic               sealed_q;
  logic signed [ 7:0] pitch_q;
  logic        [31:0] island_q;
  logic        [31:0] epoch_q;

  assign pitch_log2_o = pitch_q;
  assign island_o     = island_q;
  assign sealed_o     = sealed_q;

  // ---- the four verdicts, all combinational off the header beat -------------
  // THE ENVELOPE IS RECOMPUTED THROUGH THE SHARED PACKAGE, not re-derived.
  // `zhao_terrain_place.sv:282` computes `org_x_c` the same way from the same
  // functions and compares it against the same field; the difference -- and it
  // is the whole point of this block -- is that PLACE uses the HEADER's pitch
  // and this uses the SEALED one. When the two agree the two checks are the
  // same check; when they disagree, this is the one that notices.
  wire legal_c = pitch_legal(hdr_pitch_log2_i);

  // A new epoch, or a new island inside one epoch, RE-SEALS. It is not a
  // mismatch: directive section 2 scopes authority to "that island
  // generation", and a different generation is a different authority.
  wire fresh_c = !sealed_q || (epoch_i != epoch_q) || (hdr_island_i != island_q);

  wire signed [31:0] org_x_c = place32(units_of(hdr_ix_i, 6'd0), pitch_q);
  wire signed [31:0] org_z_c = place32(units_of(hdr_iz_i, 6'd0), pitch_q);
  wire env_ok_c  = (org_x_c == hdr_env_x0_i) && (org_z_c == hdr_env_z0_i);
  wire fits_c    = units_fits(units_of(hdr_ix_i, 6'd32), pitch_q) &&
                   units_fits(units_of(hdr_iz_i, 6'd32), pitch_q);

  wire pitch_bad_c = sealed_q && !fresh_c && legal_c && (hdr_pitch_log2_i != pitch_q);
  wire env_bad_c   = sealed_q && !fresh_c && legal_c && (!env_ok_c || !fits_c);

  // THE REFUSAL. An illegal pitch refuses whether or not a seal exists -- there
  // is nothing to compare it against and it is not a legal page.
  assign refuse_o = hdr_valid_i && (!legal_c || pitch_bad_c || env_bad_c);

  // Saturating, `spec/counters.md` section 4. Written out at every site
  // rather than behind a macro: `tools/design/check_counters.py` reads the
  // INCREMENT, and a counter hidden inside a `define is a counter that tool
  // cannot see -- the same reason entry I21 refuses a named constant for its
  // tie-offs.
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      sealed_q          <= 1'b0;
      pitch_q           <= 8'sd0;
      island_q          <= 32'd0;
      epoch_q           <= 32'd0;
      headers_checked_o <= '0;
      seals_o           <= '0;
      reseals_o         <= '0;
      pitch_illegal_o   <= '0;
      pitch_mismatch_o  <= '0;
      island_bounced_o  <= '0;
      envelope_bad_o    <= '0;
    end else if (hdr_valid_i) begin
      if (!legal_c) begin
        // Spec 1.3 admits exactly four pitches. `zhao_terrain_hdrread` already
        // substitutes HDR_PITCH_REFUSE (127) for a header it rejected, so this
        // counter also catches a refused header reaching the placement beat.
        if (pitch_illegal_o != {CW{1'b1}}) pitch_illegal_o <= pitch_illegal_o + CW'(1);
      end else if (fresh_c) begin
        // RE-SEAL. Note the two events are counted separately: `seals_o` moves
        // every time a seal is taken, `reseals_o` only when one was replaced,
        // so `seals_o - reseals_o` is "how many island generations this run
        // has seen from cold" and neither number has to be inferred.
        if (sealed_q) begin
          if (reseals_o != {CW{1'b1}}) reseals_o <= reseals_o + CW'(1);
          if (epoch_i == epoch_q) begin
            // A SECOND ISLAND INSIDE ONE RESOURCE EPOCH. Legal -- nothing
            // forbids a frame spanning two islands -- and worth a counter of
            // its own, because a frame that bounces between two islands
            // re-seals per page and every page after the first of each pair
            // is checked against the WRONG island's seal for one beat.
            if (island_bounced_o != {CW{1'b1}}) island_bounced_o <= island_bounced_o + CW'(1);
          end
        end
        if (seals_o != {CW{1'b1}}) seals_o <= seals_o + CW'(1);
        sealed_q <= 1'b1;
        pitch_q  <= hdr_pitch_log2_i;
        island_q <= hdr_island_i;
        epoch_q  <= epoch_i;
      end else begin
        if (headers_checked_o != {CW{1'b1}}) headers_checked_o <= headers_checked_o + CW'(1);
        if (pitch_bad_c && (pitch_mismatch_o != {CW{1'b1}}))
          pitch_mismatch_o <= pitch_mismatch_o + CW'(1);
        if (env_bad_c && (envelope_bad_o != {CW{1'b1}}))
          envelope_bad_o <= envelope_bad_o + CW'(1);
      end
    end
  end

  // Elaboration guard. `initial begin ... end` and NOT a module-scope `if`:
  // Quartus 17.0 rejects the latter with "syntax error near text: `if`;
  // expecting `endmodule`" (CLAUDE.md). And `--lint-only` does not run this,
  // so a clean lint says nothing about it.
  // synthesis translate_off
  initial begin
    if (CW < 8)
      $fatal(1, "zhao_terrain_islandseal: CW=%0d is too narrow to be a census", CW);
  end
  // synthesis translate_on

endmodule

`default_nettype wire
