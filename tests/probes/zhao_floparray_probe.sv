// =============================================================================
// FLOPARRAY probe -- WHICH of the two shapes keeps an array in flip-flops?
//
// WHY THIS FILE EXISTS. The ground-contact law: "a probe that does this was
// written once and thrown away, so its numbers are unreproducible -- commit the
// probe." PALRAM and ATTRSETUP both committed theirs; this is FLOPARRAY's.
//
// THE QUESTION. `zhao_forge_assemble` holds `pos_q`/`inv_q` (34,840 bits) and
// `zhao_geom_lodstate` holds `st_q` (9,216 bits) in flops. Both carry TWO
// shapes that `tools/quartus/check_ram_inference.py` already flags:
//
//   (1) an ASYNCHRONOUS RESET LOOP over the array, inside the `!rst_n` branch
//       of an `always_ff @(posedge clk or negedge rst_n)`;
//   (2) a COMBINATIONAL READ through a dynamic index -- a continuous assign
//       `wire [W-1:0] rd_c = arr[addr_q];`.
//
// PALRAM's lesson is why this file has four arms and not two: its cause was a
// CONJUNCTION of two properties, either one harmless alone, and its first
// single-cause story was refuted only by the controls it had already predicted.
// So each arm here changes EXACTLY ONE property away from the production shape,
// and the fourth changes both. If only the fourth infers, the cause is a
// conjunction. If one of the middle arms infers, that one property is the
// blocker on its own.
//
//   VARIANT 0  production verbatim ............. reset loop  + comb read
//   VARIANT 1  reset loop removed .............. no reset    + comb read
//   VARIANT 2  read registered ................. reset loop  + reg read
//   VARIANT 3  both ............................ no reset    + reg read
//
// READ VARIANT 0 FIRST, ALWAYS. It is the POSITIVE CONTROL: it must reproduce
// the production block's failure to infer. If arm 0 infers M10K, this probe is
// not reproducing production and NO other arm's row means anything.
//
// USAGE (PowerShell, tools/env/zhao-env.ps1 sourced, from the repo root):
//   .\tools\quartus\floparray_map_probe.ps1 -Arm 0
//   .\tools\quartus\floparray_map_probe.ps1 -Arm 0 -Module zhao_floparray_lod_probe
//
// Quartus echoes VARIANT into the map report's "Parameter Settings for User
// Entity Instance" table, so a kept .map.rpt states which arm produced it
// rather than relying on the row label having been typed correctly.
//
// QUARTUS 17.0 LEGALITY: explicit `generate`/`endgenerate` (an implicit
// generate is a syntax error there), no nested generate, and every elaboration
// check inside `initial begin` within a translate_off region. `--lint-only`
// does NOT run those, so a clean Verilator lint says nothing about them.
// =============================================================================
`default_nettype none

// This file holds TWO probe modules, one per target block, so neither can be
// named after the file. The tree's convention for that is a DECLFILENAME
// suppression block (`zhao_field_seq.sv:155`, `zhao_mem_share2.sv`).
/* verilator lint_off DECLFILENAME */

// -----------------------------------------------------------------------------
// ARM MODULE A -- the `zhao_forge_assemble` vertex store.
//
// Faithful to production in every respect that could matter to inference:
//   * TWO arrays, POSW=43 and 24 bits wide, MAX_VERTS=520 deep;
//   * each written at ONE dynamic address, WHOLE-ELEMENT (no part-select, so
//     PALRAM's rule-5 killer is absent here), from a NARROWED index -- so
//     PALRAM's over-wide-index half is absent too. This block's two candidate
//     blockers are the reset loop and the read style, and nothing else.
//   * the read address is registered ONE CYCLE AHEAD of the consumption, which
//     is what makes arm 2/3's "move the register from the address to the data"
//     latency-neutral rather than a pipeline stage. See the packet's findings,
//     where that claim is measured on the bench rather than argued.
// -----------------------------------------------------------------------------
// The address ports are 15 bits and are NARROWED to VW=10 at every use. That
// narrowing is production's own (`rs_slot_i[VW-1:0]`, `t_i0_i[VW-1:0]`) and it
// is deliberately preserved here, because an over-wide index is one half of
// PALRAM's conjunction and this probe's whole point is that that half is ABSENT
// in this block. The unused upper bits are therefore the faithful shape, not an
// oversight -- so the warning is suppressed rather than the ports resized.
/* verilator lint_off UNUSEDSIGNAL */
module zhao_floparray_pos_probe #(
    parameter int unsigned VARIANT   = 0,
    parameter int unsigned MAX_VERTS = 520
) (
    input  var logic        clk,
    input  var logic        rst_n,
    // the projector's un-refusable screen-position landing
    input  var logic        pos_we_i,
    input  var logic [14:0] pos_wa_i,
    input  var logic [42:0] pos_wd_i,
    // the canonical depth's landing, by TOKEN, on a different path
    input  var logic        inv_we_i,
    input  var logic [14:0] inv_wa_i,
    input  var logic [23:0] inv_wd_i,
    // the triangle walk's read
    input  var logic        rd_en_i,
    input  var logic [14:0] rd_a_i,
    input  var logic        cap_i,
    output var logic signed [20:0] ax_o,
    output var logic signed [20:0] ay_o,
    output var logic               behind_o,
    output var logic [23:0]        iw_o
);
  localparam int unsigned VW   = (MAX_VERTS <= 2) ? 1 : $clog2(MAX_VERTS);
  localparam int unsigned POSW = 1 + 21 + 21;

  // synthesis translate_off
  initial begin
    if (VARIANT > 3)
      $fatal(1, "zhao_floparray_pos_probe: VARIANT (%0d) must be 0..3", VARIANT);
  end
  // synthesis translate_on

  logic [POSW-1:0] pos_q [MAX_VERTS];
  logic [23:0]     inv_q [MAX_VERTS];

  // ---------------------------------------------------------------------------
  // THE WRITE PORTS. The write ADDRESSES, DATA and ENABLES are identical in
  // every arm so they are never the variable -- the discipline
  // `zhao_palram_probe.sv` states for its own valid bits. The ONLY difference
  // is whether the array carries production's asynchronous reset loop.
  // ---------------------------------------------------------------------------
  generate
    if ((VARIANT == 0) || (VARIANT == 2)) begin : g_wr_asyncreset
      integer k;
      always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
          // THE RESET LOOP UNDER TEST -- `zhao_forge_assemble.sv:710-713`.
          for (k = 0; k < MAX_VERTS; k = k + 1) begin
            pos_q[k] <= '0;
            inv_q[k] <= 24'd0;
          end
        end else begin
          if (pos_we_i) pos_q[pos_wa_i[VW-1:0]] <= pos_wd_i;
          if (inv_we_i) inv_q[inv_wa_i[VW-1:0]] <= inv_wd_i;
        end
      end
    end else begin : g_wr_noreset
      // NO RESET ON THE ARRAY, which is what `zhao_geom_drawjob.sv:402` says
      // lets it infer M10K. Readability after reset is then owed to an explicit
      // validity discipline, not to the array's contents -- in production that
      // is the A_DRAIN both-halves barrier plus `idx_bad_c`.
      always_ff @(posedge clk) begin
        if (pos_we_i) pos_q[pos_wa_i[VW-1:0]] <= pos_wd_i;
        if (inv_we_i) inv_q[inv_wa_i[VW-1:0]] <= inv_wd_i;
      end
    end
  endgenerate

  // ---------------------------------------------------------------------------
  // THE READ PORT -- the second property under test.
  // ---------------------------------------------------------------------------
  generate
    if ((VARIANT == 0) || (VARIANT == 1)) begin : g_rd_comb
      // PRODUCTION: the ADDRESS is registered and the array read is a
      // continuous assignment through it -- `zhao_forge_assemble.sv:579-580`.
      // This is check_ram_inference.py rule 2, and it forces a per-bit mux the
      // width of the array.
      logic [VW-1:0] rd_a_q;
      wire [POSW-1:0] pos_rd_c = pos_q[rd_a_q];
      wire [23:0]     inv_rd_c = inv_q[rd_a_q];
      always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
          rd_a_q   <= '0;
          ax_o     <= 21'sd0;
          ay_o     <= 21'sd0;
          behind_o <= 1'b0;
          iw_o     <= 24'd0;
        end else begin
          if (rd_en_i) rd_a_q <= rd_a_i[VW-1:0];
          if (cap_i) begin
            ax_o     <= $signed(pos_rd_c[20:0]);
            ay_o     <= $signed(pos_rd_c[41:21]);
            behind_o <= pos_rd_c[42];
            iw_o     <= inv_rd_c;
          end
        end
      end
    end else begin : g_rd_registered
      // THE CONVERSION: the register moves from the ADDRESS to the DATA.
      //
      // `rd_a_c` is the value the address register WOULD take this cycle, so
      // `arr[rd_a_c]` sampled now equals `arr[rd_a_q]` read combinationally
      // next cycle. The consumption therefore happens on the SAME clock as
      // before: latency-neutral, not a pipeline stage.
      //
      // The read-data registers carry NO RESET, deliberately: an asynchronous
      // clear on a RAM's output register is the other half of what destroys the
      // inference, which is why `zhao_geom_drawjob` keeps `pal_rd_q` unreset.
      logic [VW-1:0]   rd_a_q;
      logic [POSW-1:0] pos_rd_q;
      logic [23:0]     inv_rd_q;
      wire  [VW-1:0]   rd_a_c = rd_en_i ? rd_a_i[VW-1:0] : rd_a_q;

      always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) rd_a_q <= '0;
        else        rd_a_q <= rd_a_c;
      end

      always_ff @(posedge clk) begin
        pos_rd_q <= pos_q[rd_a_c];
        inv_rd_q <= inv_q[rd_a_c];
      end

      always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
          ax_o     <= 21'sd0;
          ay_o     <= 21'sd0;
          behind_o <= 1'b0;
          iw_o     <= 24'd0;
        end else if (cap_i) begin
          ax_o     <= $signed(pos_rd_q[20:0]);
          ay_o     <= $signed(pos_rd_q[41:21]);
          behind_o <= pos_rd_q[42];
          iw_o     <= inv_rd_q;
        end
      end
    end
  endgenerate

endmodule : zhao_floparray_pos_probe
/* verilator lint_on UNUSEDSIGNAL */


// -----------------------------------------------------------------------------
// ARM MODULE B -- the `zhao_geom_lodstate` slot store.
//
// THE SHAPE IS NOT THE SAME AS MODULE A, and that difference is the packet's
// second finding. `st_q[slot_c]` is read COMBINATIONALLY into `zhao_geom_lod`'s
// inputs and the evaluator's result is written back to `st_q[slot_c]` IN THE
// SAME CYCLE -- a read-modify-write at one address. Module A's address is
// registered a cycle ahead of its use, so moving the register costs nothing;
// module B's is not, so the registered read genuinely inserts a stage.
//
// The evaluator is modelled here by a small combinational function rather than
// instantiated: the probe's question is about the ARRAY, and pulling in
// `zhao_geom_lod` would add its own arithmetic to every arm equally while
// making the map cone larger and slower.
// -----------------------------------------------------------------------------
module zhao_floparray_lod_probe #(
    parameter int unsigned VARIANT = 0,
    parameter int unsigned SLOTS_C = 512
) (
    input  var logic        clk,
    input  var logic        rst_n,
    input  var logic        ev_i,
    input  var logic [ 8:0] slot_i,
    input  var logic [ 1:0] rung_i,
    input  var logic [15:0] hold_i,
    output var logic [ 1:0] rung_o,
    output var logic [15:0] hold_o
);
  localparam int unsigned STW    = 18;                 // {rung[1:0], hold[15:0]}
  localparam int unsigned SIDX_W = $clog2(SLOTS_C);

  // synthesis translate_off
  initial begin
    if (VARIANT > 3)
      $fatal(1, "zhao_floparray_lod_probe: VARIANT (%0d) must be 0..3", VARIANT);
  end
  // synthesis translate_on

  logic [STW-1:0] st_q [SLOTS_C];
  wire [SIDX_W-1:0] slot_c = slot_i[SIDX_W-1:0];

  // FLAT, ONE BRANCH PER ARM. A nested generate would express this more
  // compactly and Quartus 17 is unforgiving about generate forms, so each arm
  // gets its own branch and `st_q` has exactly ONE driver in every one of them.
  generate
    if (VARIANT == 0) begin : g_v0_production
      // Combinational read, evaluate, write back to the SAME address on the
      // same edge -- `zhao_geom_lodstate.sv:513-514` and `:673` -- with the
      // array's asynchronous reset loop from `:549`.
      wire [1:0]  rd_rung_c = st_q[slot_c][17:16];
      wire [15:0] rd_hold_c = st_q[slot_c][15:0];
      wire [1:0]  nx_rung_c = rd_rung_c ^ rung_i;
      wire [15:0] nx_hold_c = (rd_hold_c == 16'd0) ? hold_i : (rd_hold_c - 16'd1);
      integer i0;
      always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
          for (i0 = 0; i0 < SLOTS_C; i0 = i0 + 1) st_q[i0] <= 18'd0;
          rung_o <= 2'd0;
          hold_o <= 16'd0;
        end else begin
          if (ev_i) st_q[slot_c] <= {nx_rung_c, nx_hold_c};
          rung_o <= rd_rung_c;
          hold_o <= rd_hold_c;
        end
      end
    end else if (VARIANT == 1) begin : g_v1_noreset
      // Reset loop removed, read-modify-write kept.
      wire [1:0]  rd_rung_c = st_q[slot_c][17:16];
      wire [15:0] rd_hold_c = st_q[slot_c][15:0];
      wire [1:0]  nx_rung_c = rd_rung_c ^ rung_i;
      wire [15:0] nx_hold_c = (rd_hold_c == 16'd0) ? hold_i : (rd_hold_c - 16'd1);
      always_ff @(posedge clk) begin
        if (ev_i) st_q[slot_c] <= {nx_rung_c, nx_hold_c};
      end
      always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
          rung_o <= 2'd0;
          hold_o <= 16'd0;
        end else begin
          rung_o <= rd_rung_c;
          hold_o <= rd_hold_c;
        end
      end
    end else if (VARIANT == 2) begin : g_v2_regread_reset
      // Registered read, reset loop KEPT. Both live in the one clocked block
      // because `st_q` must have a single driver.
      logic [STW-1:0]    rd_q;
      logic              ev_q;
      logic [SIDX_W-1:0] slot_q;
      logic [1:0]        rung_d_q;
      logic [15:0]       hold_d_q;
      wire  [1:0]  rd_rung_c = rd_q[17:16];
      wire  [15:0] rd_hold_c = rd_q[15:0];
      wire  [1:0]  nx_rung_c = rd_rung_c ^ rung_d_q;
      wire  [15:0] nx_hold_c = (rd_hold_c == 16'd0) ? hold_d_q : (rd_hold_c - 16'd1);
      integer i2;
      always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
          for (i2 = 0; i2 < SLOTS_C; i2 = i2 + 1) st_q[i2] <= 18'd0;
          rd_q <= '0;
        end else begin
          rd_q <= st_q[slot_c];
          if (ev_q) st_q[slot_q] <= {nx_rung_c, nx_hold_c};
        end
      end
      always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
          ev_q <= 1'b0; slot_q <= '0; rung_d_q <= 2'd0; hold_d_q <= 16'd0;
          rung_o <= 2'd0; hold_o <= 16'd0;
        end else begin
          ev_q <= ev_i; slot_q <= slot_c; rung_d_q <= rung_i; hold_d_q <= hold_i;
          rung_o <= rd_rung_c; hold_o <= rd_hold_c;
        end
      end
    end else begin : g_v3_regread_noreset
      // Registered read AND no array reset -- the conversion, and it costs one
      // clock on this path because the evaluate must wait for the read.
      logic [STW-1:0]    rd_q;
      logic              ev_q;
      logic [SIDX_W-1:0] slot_q;
      logic [1:0]        rung_d_q;
      logic [15:0]       hold_d_q;
      wire  [1:0]  rd_rung_c = rd_q[17:16];
      wire  [15:0] rd_hold_c = rd_q[15:0];
      wire  [1:0]  nx_rung_c = rd_rung_c ^ rung_d_q;
      wire  [15:0] nx_hold_c = (rd_hold_c == 16'd0) ? hold_d_q : (rd_hold_c - 16'd1);
      always_ff @(posedge clk) begin
        rd_q <= st_q[slot_c];
        if (ev_q) st_q[slot_q] <= {nx_rung_c, nx_hold_c};
      end
      always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
          ev_q <= 1'b0; slot_q <= '0; rung_d_q <= 2'd0; hold_d_q <= 16'd0;
          rung_o <= 2'd0; hold_o <= 16'd0;
        end else begin
          ev_q <= ev_i; slot_q <= slot_c; rung_d_q <= rung_i; hold_d_q <= hold_i;
          rung_o <= rd_rung_c; hold_o <= rd_hold_c;
        end
      end
    end
  endgenerate

endmodule : zhao_floparray_lod_probe

/* verilator lint_on DECLFILENAME */
`default_nettype wire
