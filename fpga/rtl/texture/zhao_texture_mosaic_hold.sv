// zhao_texture_mosaic_hold.sv -- TEXTURE.MOSAIC's answer, held per owner until
// the sample that asked for it is ready to leave.
//
// ENFORCED-BY: tests/texture/texture_mosaic_hold_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHY THIS EXISTS
// ---------------------------------------------------------------------------
// `zhao_texture_mosaic_v2` computes the frozen `terrain_rules` 6.2 tile pick for
// every fragment the V3 island admits, and until 2026-09-26 NOTHING READ THE
// ANSWER: `mosaic_tile_w`, `mosaic_tx_w` and `mosaic_ty_w` occurred exactly
// twice each in `zhao_texture_island_v3_top.sv` -- a declaration and a port
// connection. The reader is `zhao_texture_binding_resolver_v2`'s TILESET row,
// which turns the tile into the byte displacement `zref::Tileset` gives it. This
// block is the join between the two.
//
// ---------------------------------------------------------------------------
// THE JOIN IS THE WHOLE PROBLEM, AND IT IS THE ONE CLAUDE.md HAS A CHAPTER ON
// ---------------------------------------------------------------------------
// The expander offers a fragment's SAMPLE job and its MOSAIC job on the same
// beat, on two independent handshakes. The mosaic answers TWO cycles later. So
// the sample is always offered BEFORE its own pick exists, and an ungated read
// of a per-owner table hands the resolver THE PREVIOUS OCCUPANT OF THE SLOT --
// exactly the metadata-swap defect CLAUDE.md records, where every accepted and
// emitted counter balances because no counter looks at the field that moved.
//
// So the table is SEALED BY THE OWNER'S GENERATION and the sample is HELD:
// `pick_ready_o` is false until the generation stored for this owner's slot is
// this owner's own. The two sides of that comparison are loaded by DIFFERENT
// enables in different blocks -- the stored generation by the mosaic's response,
// the offered one by the expander's handle -- which is the property the
// two-operands-that-move-together law asks for.
//
// IT CANNOT DEADLOCK, and the argument is structural rather than empirical. The
// expander's `mosaic_valid_o` does not depend on its `sample_ready_i`; the mosaic
// accepts whenever it can advance; and the island drives the mosaic's
// `pick_ready_i` with a constant one. So the pick lands whether or not the
// sample moves, and holding the sample cannot stop the thing it waits for.
//
// WHAT IT DOES NOT DO: `pick_tx_o`/`pick_ty_o` are deliberately NOT carried.
// They are the mirrored texel pair, which the TMU recomputes from u/v under the
// tileset row's own mirror wrap at log2w = log2h = 6 -- the same
// `zref::terrain::mirror_texel`, by the same law. Carrying them would be a
// second implementation of ratified arithmetic sitting beside the one that
// ships.
//
// Conservative SystemVerilog subset only; Quartus 17.0 syntax rules apply.
`default_nettype none

module zhao_texture_mosaic_hold #(
    parameter int unsigned SLOTW = 6,
    parameter int unsigned GENW  = 8
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- TEXTURE.MOSAIC's answer -------------------------------------------
    // `pick_src_id_i` is the owner the island gave the mosaic on the request,
    // {slot, generation}, returned unchanged beside the pick.
    input  var logic                   pick_valid_i,
    input  var logic [SLOTW+GENW-1:0]  pick_src_id_i,
    input  var logic [7:0]             pick_tile_i,

    // ---- the sample job asking for it --------------------------------------
    // The expander's handle is {slot, sample_index, generation}: the slot is its
    // top SLOTW bits and the generation its low GENW, which is how
    // `zhao_texture_frag_expand_v2` builds it.
    // The SAMPLE INDEX bits ([SLOTW+GENW +: 2]) are deliberately unread: the
    // mosaic answers ONCE PER FRAGMENT, and every sample of that fragment reads
    // the same pick. Taking the whole handle rather than {slot, generation}
    // separately means the caller cannot split it differently from the expander
    // that built it; the waiver is narrow and this sentence is why it is here.
    /* verilator lint_off UNUSEDSIGNAL */
    input  var logic [SLOTW+2+GENW-1:0] smp_handle_i,
    /* verilator lint_on UNUSEDSIGNAL */
    output var logic                    pick_ready_o,
    output var logic [7:0]              pick_tile_o,

    // ---- evidence ----------------------------------------------------------
    // CENSUS: picks stored. A pick that overwrites a slot whose previous pick
    // was never consumed is NOT a fault -- a zero-sample fragment produces
    // exactly that -- so this counts arrivals and nothing here counts a loss.
    output var logic [31:0]             picks_held_o,
    // A FAULT, AND THE ONE THIS BLOCK EXISTS TO MAKE IMPOSSIBLE: a sample whose
    // slot holds a pick from a DIFFERENT generation. It is an observation, not a
    // gate -- `pick_ready_o` already refuses to let such a sample out -- so it
    // measures how often the hold is doing work rather than how often it fails.
    output var logic [31:0]             stale_slot_holds_o
);

  localparam int unsigned SLOTS = 1 << SLOTW;
  localparam logic [31:0] CNT_MAX = 32'hFFFF_FFFF;

  logic [7:0]      tile_m [0:SLOTS-1];
  logic [GENW-1:0] generation_m [0:SLOTS-1];
  logic            present_q [0:SLOTS-1];

  wire [SLOTW-1:0] pick_slot_c = pick_src_id_i[SLOTW+GENW-1 -: SLOTW];
  wire [SLOTW-1:0] smp_slot_c  = smp_handle_i[SLOTW+2+GENW-1 -: SLOTW];
  wire [GENW-1:0]  smp_gen_c   = smp_handle_i[GENW-1:0];

  assign pick_ready_o = present_q[smp_slot_c] &&
                        (generation_m[smp_slot_c] == smp_gen_c);
  assign pick_tile_o  = tile_m[smp_slot_c];

  // The stale observation is deliberately NOT `!pick_ready_o`: a slot that has
  // never held a pick at all is the reset state and says nothing about a swap.
  // This fires only when a pick IS present and belongs to somebody else, which
  // is the fault shape, and it needs the hold to be exercised to reach it.
  wire stale_slot_c = present_q[smp_slot_c] &&
                      (generation_m[smp_slot_c] != smp_gen_c);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      for (int unsigned i = 0; i < SLOTS; i++)
        present_q[i] <= 1'b0;
      picks_held_o       <= 32'd0;
      stale_slot_holds_o <= 32'd0;
    end else begin
      if (pick_valid_i) begin
        present_q[pick_slot_c] <= 1'b1;
        if (picks_held_o != CNT_MAX)
          picks_held_o <= picks_held_o + 32'd1;
      end
      if (stale_slot_c && (stale_slot_holds_o != CNT_MAX))
        stale_slot_holds_o <= stale_slot_holds_o + 32'd1;
    end
  end

  // Payload is unreset: the presence bit and the generation seal are what make
  // a row readable, exactly as `zhao_texture_early_desc_v2` and the island's
  // own per-owner tables do it.
  always_ff @(posedge clk) begin
    if (pick_valid_i) begin
      tile_m[pick_slot_c]       <= pick_tile_i;
      generation_m[pick_slot_c] <= pick_src_id_i[GENW-1:0];
    end
  end

  initial begin : p_widths
    if ((SLOTW < 1) || (GENW < 1))
      $fatal(1, "zhao_texture_mosaic_hold: SLOTW and GENW must be positive");
  end

endmodule : zhao_texture_mosaic_hold

`default_nettype wire
